/*
 * mOS — instalador. Ver include/install.h para as regras. Trabalho incremental: install_poll() executa uma fatia
 * (preparar, particionar, formatar, copiar UM arquivo, verificar UM arquivo...) e volta, para a interface continuar
 * respondendo e mostrar o progresso.
 *
 * Ordem das escritas no destino (a ordem é o que torna uma queda de energia segura):
 *   1. (disco inteiro) GPT nova;  2. formatar FAT32;  3. estado "INSTALANDO";  4. cópia dos arquivos do manifesto
 *   (menos o carregador), cada um com o SHA-256 conferido na leitura;  5. preferências de idioma/teclado;
 *   6. sync + flush;  7. releitura de TODOS os arquivos copiados e conferência dos SHA-256;
 *   8. carregador como arquivo temporário (\EFI\BOOT\MOSNEW.TMP) + releitura conferida;  9. estado "PRIMEIRO_BOOT";
 *   10. sync + flush;  11. renomear o temporário para BOOTX64.EFI (o único passo que torna o destino inicializável);
 *   12. sync + flush e conferência final do carregador.
 */
#include "install.h"
#include "origin.h"
#include "sysstate.h"
#include "crypto.h"
#include "i18n.h"
#include "vfs.h"

#define MANIFEST   "/mOS/Sistema/Instalacao/manifesto.txt"
#define LOADER     "/EFI/BOOT/BOOTX64.EFI"
#define LOADER_TMP "/EFI/BOOT/MOSNEW.TMP"
#define STATE      "/mOS/Sistema/Biblioteca/estado-sistema"
#define PREFS      "/mOS/Sistema/Biblioteca/Preferencias/com.mos.sistema.cfg"
#define MIB        (1ull << 20)

/* ------------------------------------------------------------------ manifesto */
typedef struct { char *path; u64 size; u8 hash[32]; } mfile_t;
static mfile_t *mf;
static int nmf, loader_idx = -1;
static payload_info_t pinfo;
static bool pinfo_done;

static int hexv(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; }
static bool hex32(const char *h, u8 *out) {
    for (int i = 0; i < 32; i++) {
        int a = hexv(h[i * 2]), b = hexv(h[i * 2 + 1]);
        if (a < 0 || b < 0) return false;
        out[i] = (u8)(a << 4 | b);
    }
    return true;
}

/* Política de conteúdo do payload (espelha a do mkimg): o manifesto de uma mídia adulterada ou mal gerada que liste contas, logs,
 * preferências, pastas pessoais ou estado NUNCA é aceito, mesmo com a assinatura de integridade correta. */
bool install_path_allowed(const char *p) {
    static const char *const deny[] = {"/mOS/Users", "/mOS/Sistema/Biblioteca/Usuarios", "/mOS/Sistema/Biblioteca/Logs",
                                       "/mOS/Sistema/Biblioteca/Preferencias", "/mOS/Sistema/Instalacao", "/mOS/tmp", "/mOS/Volumes",
                                       "/mOS/Sistema/Biblioteca/estado-sistema"};
    usize n = strlen(p);
    if (n < 2 || n > 1000 || p[0] != '/') return false;     /* apps como o Chrome têm caminhos de 270+ caracteres */
    if (strstr(p, "..") || strchr(p, '\\') || strstr(p, "//") || p[n - 1] == '/') return false;
    if (strcmp(p, LOADER) && strncmp(p, "/mOS/", 5)) return false;
    for (u32 i = 0; i < ARRAY_LEN(deny); i++) {
        usize d = strlen(deny[i]);
        if (!strncmp(p, deny[i], d) && (p[d] == 0 || p[d] == '/')) return false;
    }
    const char *base = strrchr(p, '/') + 1;
    usize bl = strlen(base);
    if (bl > 6 && !strcmp(base + bl - 6, ".conta")) return false;
    if (bl > 4 && !strcmp(base + bl - 4, ".log")) return false;
    if (strstr(p, "/.Lixeira")) return false;
    return true;
}

static void perr(const char *m) { pinfo.ok = false; snprintf(pinfo.err, sizeof pinfo.err, "%s", m); kprintf("install: payload recusado: %s\n", m); }

const payload_info_t *install_payload(void) {
    if (pinfo_done) return &pinfo;
    pinfo_done = true;
    memset(&pinfo, 0, sizeof pinfo);
    fatfs_t *src = persist_volume();
    if (!src || origin_get()->status != ORIGIN_TRUSTED) { perr("origem não confiável: sem payload"); return &pinfo; }
    u64 len = 0;
    u8 *d = fat_read_file(src, MANIFEST, &len);
    if (!d || len < 64 || len > (8u << 20)) { kfree(d); perr("manifesto do payload ausente ou inválido"); return &pinfo; }
    char *txt = (char *)d;
    /* última linha: ASSINATURA-SHA256 <hex> = SHA-256 de tudo que vem antes dela */
    u8 *sig = NULL;
    for (u64 i = len - 1; i > 0; i--) if (d[i - 1] == '\n' && !strncmp((char *)d + i, "ASSINATURA-SHA256 ", 18)) { sig = d + i; break; }
    u8 want[32], got[32];
    if (!sig || !hex32((char *)sig + 18, want)) { kfree(d); perr("manifesto sem assinatura de integridade"); return &pinfo; }
    sha256(d, (usize)(sig - d), got);
    if (memcmp(want, got, 32)) { kfree(d); perr("manifesto corrompido (SHA-256 não confere)"); return &pinfo; }
    *sig = 0;
    int n = 0;
    u64 total = 0;
    char *line = txt;
    if (strncmp(line, "mOS-MANIFESTO-1 ", 16)) { kfree(d); perr("versão de manifesto desconhecida"); return &pinfo; }
    int cnt = atoi(line + 16);
    char *tp = strchr(line + 16, ' ');
    u64 want_total = tp ? (u64)strtol(tp + 1, NULL, 10) : 0;
    if (cnt < 1 || cnt > 200000) { kfree(d); perr("manifesto com contagem inválida"); return &pinfo; }
    mf = kcalloc((usize)cnt, sizeof *mf);
    if (!mf) { kfree(d); perr("sem memória para o manifesto"); return &pinfo; }
    char *nl = strchr(line, '\n');
    line = nl ? nl + 1 : NULL;
    while (line && *line && n < cnt) {
        nl = strchr(line, '\n');
        if (nl) *nl = 0;
        /* "<hex64> <tamanho> <caminho>" */
        if (strlen(line) < 68 || line[64] != ' ' || !hex32(line, mf[n].hash)) { perr("linha de manifesto inválida"); goto bad; }
        char *sp = strchr(line + 65, ' ');
        if (!sp) { perr("linha de manifesto sem caminho"); goto bad; }
        mf[n].size = (u64)strtol(line + 65, NULL, 10);
        if (!install_path_allowed(sp + 1)) { perr("caminho inválido no manifesto"); goto bad; }
        if (n && strcmp(mf[n - 1].path, sp + 1) >= 0) { perr("manifesto fora de ordem ou com duplicata"); goto bad; }
        mf[n].path = strdup(sp + 1);
        if (!strcmp(mf[n].path, LOADER)) loader_idx = n;
        total += mf[n].size;
        n++;
        line = nl ? nl + 1 : NULL;
    }
    if (n != cnt || total != want_total || loader_idx < 0) { perr("manifesto incompleto (contagem, total ou carregador EFI)"); goto bad; }
    nmf = n;
    pinfo.ok = true;
    pinfo.files = n;
    pinfo.total_bytes = total;
    kfree(d);
    kprintf("install: payload %d arquivos, %llu KB (manifesto conferido)\n", n, total >> 10);
    return &pinfo;
bad:
    for (int i = 0; i < n; i++) kfree(mf[i].path);
    kfree(mf); mf = NULL; nmf = 0;
    kfree(d);
    return &pinfo;
}

/* ------------------------------------------------------------------ elegibilidade */
static u64 need_bytes(void) {
    u64 n = install_payload()->total_bytes;
    return MAX(64 * MIB, n + n / 4 + 32 * MIB);
}

int install_reason_str(int ir) {
    switch (ir) {
    case IR_NO_ORIGIN: return S_IR_NO_ORIGIN;
    case IR_NO_PAYLOAD: return S_IR_NO_PAYLOAD;
    case IR_IS_ORIGIN: return S_IR_IS_ORIGIN;
    case IR_RO: return S_IR_RO;
    case IR_LBS: return S_IR_LBS;
    case IR_TOO_SMALL: return S_IR_TOO_SMALL;
    case IR_TOO_BIG: return S_IR_TOO_BIG;
    case IR_BAD_TABLE: return S_IR_BAD_TABLE;
    case IR_NO_BACKUP: return S_IR_NO_BACKUP;
    case IR_NO_TABLE: return S_IR_NO_TABLE;
    case IR_NOT_ESP: return S_IR_NOT_ESP;
    case IR_NOT_DEDICATED: return S_IR_NOT_DEDICATED;
    case IR_BUSY: return S_IR_BUSY;
    }
    return S_IR_NO_ORIGIN;
}

static int check_common(int disk) {
    const origin_t *o = origin_get();
    if (o->status != ORIGIN_TRUSTED) return IR_NO_ORIGIN;
    if (!install_payload()->ok) return IR_NO_PAYLOAD;
    blkdev_t *d = storage_disk(disk);
    if (!d) return IR_NO_ORIGIN;
    if (disk == o->disk) return IR_IS_ORIGIN;
    if (!blk_hw_writable(d)) return IR_RO;
    if (d->lbs && d->lbs != 512) return IR_LBS;
    return IR_OK;
}

int install_check_disk(int disk, const ptable_t *t) {
    (void)t;   /* disco inteiro: uma tabela ilegível não impede (o usuário escolheu apagar este disco), mas o tamanho vale */
    int r = check_common(disk);
    if (r) return r;
    blkdev_t *d = storage_disk(disk);
    if (d->size < need_bytes() + 2 * MIB) return IR_TOO_SMALL;
    if (d->size > (2048ull * 1024 * MIB) - 64 * MIB) return IR_TOO_BIG;      /* fat_format: até 2 TiB */
    return IR_OK;
}

typedef struct { bool ok; int n; } dctx_t;
static bool name_is(const char *a, const char *b) { return !strcasecmp(a, b); }
static void root_cb(void *ctx, const char *name, bool dir, u64 size) {
    dctx_t *c = ctx;
    c->n++;
    if (!(dir && (name_is(name, "EFI") || name_is(name, "mOS")))) c->ok = false;
}
static void efi_cb(void *ctx, const char *name, bool dir, u64 size) {
    dctx_t *c = ctx;
    c->n++;
    if (!(dir && name_is(name, "BOOT"))) c->ok = false;
}
static void boot_cb(void *ctx, const char *name, bool dir, u64 size) {
    dctx_t *c = ctx;
    c->n++;
    if (dir || !name_is(name, "BOOTX64.EFI")) c->ok = false;
}

/* "dedicada": vazia, ou um FAT que só contém \EFI\BOOT\BOOTX64.EFI e/ou \mOS (um mOS anterior). Qualquer outra coisa
 * (Microsoft, ubuntu, arquivos pessoais...) = partição compartilhada ou com dados: NÃO serve. */
static bool dedicated_fat(blkdev_t *slice) {
    fatfs_t *fs = fat_mount(slice);
    if (!fs) return false;
    dctx_t c = {true, 0};
    bool ok = fat_list(fs, "/", root_cb, &c) >= 0 && c.ok;
    if (ok) {
        bool dir;
        if (fat_stat(fs, "/EFI", &dir, NULL) && dir) { dctx_t e = {true, 0}; ok = fat_list(fs, "/EFI", efi_cb, &e) >= 0 && e.ok; }
        if (ok && fat_stat(fs, "/EFI/BOOT", &dir, NULL) && dir) { dctx_t b = {true, 0}; ok = fat_list(fs, "/EFI/BOOT", boot_cb, &b) >= 0 && b.ok; }
    }
    fat_unmount(fs);
    return ok;
}

int install_check_part(int disk, const ptable_t *t, int idx) {
    int r = check_common(disk);
    if (r) return r;
    if (t->kind == PT_BAD) return IR_BAD_TABLE;
    if (t->kind != PT_GPT && t->kind != PT_MBR) return IR_NO_TABLE;
    if (t->kind == PT_GPT && !t->backup_ok) return IR_NO_BACKUP;
    if (idx < 0 || idx >= t->n) return IR_NO_TABLE;
    const part_t *p = &t->p[idx];
    u64 size = (p->last - p->first + 1) * 512;
    if (!p->esp) return IR_NOT_ESP;
    if (size < need_bytes()) return IR_TOO_SMALL;
    if (size > (2048ull * 1024 * MIB) - 64 * MIB) return IR_TOO_BIG;
    if (p->fs != FSK_EMPTY) {
        if (p->fs != FSK_FAT) return IR_NOT_DEDICATED;
        blkdev_t *sl = blk_slice(storage_disk(disk), p->first * 512, size);
        bool ok = sl && dedicated_fat(sl);
        blk_release(sl);
        if (!ok) return IR_NOT_DEDICATED;
    }
    return IR_OK;
}

void install_confirm_phrase(int mode, int disk, int part_number, char *out, usize cap) {
    if (mode == INST_PART) snprintf(out, cap, "%sp%d", storage_disk_name(disk), part_number);
    else snprintf(out, cap, "%s", storage_disk_name(disk));
}

/* ------------------------------------------------------------------ execução */
enum { PH_PRECHECK, PH_PART, PH_FORMAT, PH_COPY, PH_PREFS, PH_SYNC, PH_VERIFY, PH_LOADER, PH_FINISH };

static install_status_t st;
static int phase, next_i;
static blkdev_t *tdisk, *part;
static u64 grant_off, grant_len, grant_want_len, copy_total, copy_done;   /* copy_*: só a fase de cópia (sem o carregador), para o percentual */
static u64 part_off_sel, part_size_sel;
static fatfs_t *dst;
static char pref_lang[8], pref_kbd[8];

const install_status_t *install_status(void) { return &st; }
static bool mode_is_disk(void) { return st.mode == INST_DISK; }

static void cleanup(void) {
    if (dst) { fat_unmount(dst); dst = NULL; }
    if (part) { blk_release(part); part = NULL; }
    if (tdisk && grant_len) storage_revoke_write(tdisk, grant_off, grant_len);
    grant_len = 0;
}

static void fail(const char *msg) {
    st.state = INSTALL_ERROR;
    snprintf(st.error, sizeof st.error, "%s", msg);
    kprintf("install: ERRO: %s (destino %s)\n", msg, st.wrote ? "MODIFICADO/incompleto" : "intacto");
    cleanup();
}

static void ferr(const char *what, const char *path) {
    char m[160];
    snprintf(m, sizeof m, "%s: %s", what, path);
    fail(m);
}

bool install_begin(int mode, int disk, int part_number, const u8 snap[32], const char *typed, const char *lang_pref, const char *kbd_pref) {
    if (st.state == INSTALL_RUNNING) return false;
    memset(&st, 0, sizeof st);
    st.mode = mode; st.disk = disk; st.part_number = part_number;
#define REFUSE(m) do { st.state = INSTALL_ERROR; snprintf(st.error, sizeof st.error, "%s", m); kprintf("install: recusado, nada foi gravado: %s\n", m); return false; } while (0)
    char expect[40];
    install_confirm_phrase(mode, disk, part_number, expect, sizeof expect);
    snprintf(st.target, sizeof st.target, "%s", expect);
    if (!typed || strcmp(typed, expect)) REFUSE("A confirmação digitada não confere.");
    blkdev_t *d = storage_disk(disk);
    if (!d) REFUSE("Disco inexistente.");
    ptable_t *t = kmalloc(sizeof *t);
    if (!t) REFUSE("Sem memória.");
    if (!partscan(d, t)) { kfree(t); REFUSE("Não foi possível ler a tabela de partições do destino."); }
    if (memcmp(t->snap, snap, 32)) { kfree(t); REFUSE("A tabela de partições do destino mudou desde a seleção. Nada foi gravado."); }
    int ir, idx = -1;
    if (mode == INST_DISK) ir = install_check_disk(disk, t);
    else {
        for (int i = 0; i < t->n; i++) if (t->p[i].number == part_number) idx = i;
        ir = idx < 0 ? IR_NO_TABLE : install_check_part(disk, t, idx);
        if (ir == IR_OK) { part_off_sel = t->p[idx].first * 512; part_size_sel = (t->p[idx].last - t->p[idx].first + 1) * 512; }
    }
    kfree(t);
    if (ir) REFUSE("O alvo não é (mais) compatível com a instalação. Nada foi gravado.");
    snprintf(pref_lang, sizeof pref_lang, "%s", lang_pref ? lang_pref : "pt");
    snprintf(pref_kbd, sizeof pref_kbd, "%s", kbd_pref ? kbd_pref : "abnt2");
    tdisk = d;
    grant_off = mode == INST_DISK ? 0 : part_off_sel;
    grant_len = 0;                                  /* a escrita só é liberada DEPOIS de conferir a origem inteira (PH_PRECHECK) */
    next_i = 0;
    phase = PH_PRECHECK;
    st.state = INSTALL_RUNNING;
    /* contadores do resumo: TODOS os arquivos do manifesto, inclusive o carregador EFI (conferível por fora contra o manifesto) */
    st.files_total = nmf;
    copy_total = copy_done = 0;
    for (int i = 0; i < nmf; i++) { st.bytes_total += mf[i].size; if (i != loader_idx) copy_total += mf[i].size; }
    grant_want_len = mode == INST_DISK ? d->size : part_size_sel;
    snprintf(st.step, sizeof st.step, "Preparando %s", expect);
    kprintf("install: INSTALANDO em %s (%s)\n", expect, mode == INST_DISK ? "disco inteiro" : "partição dedicada");
    return true;
#undef REFUSE
}

static bool mkdirs(const char *path) {         /* cria as pastas até o pai de path */
    char tmp[1040];
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p != '/') continue;
        *p = 0;
        bool dir;
        if (!(fat_stat(dst, tmp, &dir, NULL) && dir) && !fat_mkdir(dst, tmp)) return false;
        *p = '/';
    }
    return true;
}

static bool put(const char *path, const void *data, u64 len) { return mkdirs(path) && fat_write_file(dst, path, data, len); }

static bool same_hash(const u8 *data, u64 len, const mfile_t *m) {
    u8 h[32];
    if (len != m->size) return false;
    sha256(data, (usize)len, h);
    return !memcmp(h, m->hash, 32);
}

static bool verify_dst(const char *dpath, const mfile_t *m) {
    u64 len = 0;
    u8 *d = fat_read_file(dst, dpath, &len);
    bool ok = d && same_hash(d, len, m);
    kfree(d);
    return ok;
}

void install_poll(void) {
    if (st.state != INSTALL_RUNNING) return;
    static int last_logged = -1;
    if (phase != last_logged) {                       /* uma linha por fase: serve de ponto de referência nos testes de queda de energia */
        static const char *names[] = {"PRECHECK", "PART", "FORMAT", "COPY", "PREFS", "SYNC", "VERIFY", "LOADER", "FINISH"};
        kprintf("install: fase %s\n", names[phase]);
        last_logged = phase;
    }
    switch (phase) {
    case PH_PRECHECK: {
        /* Nada foi gravado ainda. Confere TODA a origem contra o manifesto antes da primeira escrita: se um arquivo da mídia
         * estiver corrompido, a instalação recusa sem ter tocado no destino. */
        if (next_i >= nmf) {
            if (!storage_grant_write(tdisk, grant_off, grant_want_len)) { fail("O armazenamento recusou liberar a escrita. Nada foi gravado."); return; }
            grant_len = grant_want_len;
            next_i = 0;
            phase = mode_is_disk() ? PH_PART : PH_FORMAT;
            st.percent = 8;
            return;
        }
        mfile_t *m = &mf[next_i++];
        snprintf(st.step, sizeof st.step, "Conferindo a origem: %s", m->path);
        u64 len = 0;
        u8 *data = fat_read_file(persist_volume(), m->path, &len);
        bool ok = (data || !m->size) && same_hash(data ? data : (u8 *)"", len, m);
        kfree(data);
        if (!ok) { ferr("A origem está corrompida ou ilegível (SHA-256 não confere); nada foi gravado", m->path); return; }
        st.percent = 8 * next_i / (nmf ? nmf : 1);
        return;
    }
    case PH_PART: {
        u64 off = 0, size = 0;
        snprintf(st.step, sizeof st.step, "Criando a tabela de partições");
        st.wrote = true;
        if (!gpt_create_single_esp(tdisk, "mOS", &off, &size)) { fail("Não foi possível gravar a tabela de partições."); return; }
        part_off_sel = off; part_size_sel = size;
        st.percent = 10;
        phase = PH_FORMAT;
        return;
    }
    case PH_FORMAT:
        snprintf(st.step, sizeof st.step, "Formatando (FAT32)");
        st.wrote = true;
        part = blk_slice(tdisk, part_off_sel, part_size_sel);
        if (!part || !fat_format(part, "MOS")) { fail("Não foi possível formatar o destino."); return; }
        dst = fat_mount(part);
        if (!dst) { fail("O volume recém-formatado não pôde ser montado."); return; }
        if (!put(STATE, "INSTALANDO\n", 11)) { fail("Não foi possível gravar o estado de instalação."); return; }
        st.percent = 12;
        phase = PH_COPY;
        return;
    case PH_COPY: {
        while (next_i < nmf && next_i == loader_idx) next_i++;
        if (next_i >= nmf) { phase = PH_PREFS; return; }
        mfile_t *m = &mf[next_i++];
        snprintf(st.step, sizeof st.step, "Copiando %s", m->path);
        u64 len = 0;
        u8 *data = fat_read_file(persist_volume(), m->path, &len);
        if (!data && m->size) { ferr("Falha ao ler na origem", m->path); return; }
        if (!same_hash(data ? data : (u8 *)"", len, m)) { kfree(data); ferr("Arquivo da origem não confere com o manifesto (SHA-256)", m->path); return; }
        bool ok = put(m->path, data ? data : (u8 *)"", len);
        kfree(data);
        if (!ok) { ferr("Falha ao gravar (destino cheio?)", m->path); return; }
        st.bytes_done += len;
        st.files_done++;
        copy_done += len;
        st.percent = 12 + (int)(copy_done * 56 / (copy_total ? copy_total : 1));
        return;
    }
    case PH_PREFS: {
        snprintf(st.step, sizeof st.step, "Gravando as preferências de idioma e teclado");
        char b[64];
        int n = snprintf(b, sizeof b, "idioma=%s\nteclado=%s\n", pref_lang, pref_kbd);
        if (!put(PREFS, b, (u64)n)) { fail("Não foi possível gravar as preferências."); return; }
        st.percent = 70;
        phase = PH_SYNC;
        return;
    }
    case PH_SYNC:
        snprintf(st.step, sizeof st.step, "Gravando no disco");
        if (!fat_sync(dst) || !blk_flush(part)) { fail("Falha ao descarregar as gravações no disco."); return; }
        st.percent = 72;
        next_i = 0;
        phase = PH_VERIFY;
        return;
    case PH_VERIFY: {
        while (next_i < nmf && next_i == loader_idx) next_i++;
        if (next_i >= nmf) { phase = PH_LOADER; st.percent = 92; return; }
        mfile_t *m = &mf[next_i++];
        snprintf(st.step, sizeof st.step, "Verificando %s", m->path);
        if (!verify_dst(m->path, m)) { ferr("Verificação falhou (conteúdo gravado difere do manifesto)", m->path); return; }
        st.percent = 72 + 20 * next_i / (nmf ? nmf : 1);
        return;
    }
    case PH_LOADER: {
        snprintf(st.step, sizeof st.step, "Concluindo (carregador EFI)");
        mfile_t *m = &mf[loader_idx];
        u64 len = 0;
        u8 *data = fat_read_file(persist_volume(), LOADER, &len);
        if (!data || !same_hash(data, len, m)) { kfree(data); fail("O carregador EFI da origem não confere com o manifesto."); return; }
        bool ok = put(LOADER_TMP, data, len);
        kfree(data);
        if (!ok || !verify_dst(LOADER_TMP, m)) { fail("Não foi possível gravar e verificar o carregador EFI."); return; }
        if (!put(STATE, "PRIMEIRO_BOOT\n", 14)) { fail("Não foi possível gravar o estado final."); return; }
        if (!fat_sync(dst) || !blk_flush(part)) { fail("Falha ao descarregar as gravações no disco."); return; }
        /* o ÚNICO passo que torna o destino inicializável */
        if (!fat_rename(dst, LOADER_TMP, LOADER)) { fail("Não foi possível ativar o carregador EFI."); return; }
        if (!fat_sync(dst) || !blk_flush(part) || !verify_dst(LOADER, m)) { fail("Verificação final do carregador EFI falhou."); return; }
        st.bytes_done += mf[loader_idx].size;       /* o carregador entra na contagem só depois de ativado e conferido */
        st.files_done++;
        phase = PH_FINISH;
        st.percent = 99;
        return;
    }
    case PH_FINISH:
        cleanup();
        st.percent = 100;
        st.state = INSTALL_DONE;
        snprintf(st.step, sizeof st.step, "Instalação concluída");
        kprintf("install: concluído em %s (%d de %d arquivos do manifesto, incluindo o carregador EFI; %llu de %llu bytes; todos verificados por SHA-256)\n", st.target, st.files_done, st.files_total, st.bytes_done, st.bytes_total);
        return;
    }
}
