/*
 * mOS — atualização do sistema sem reinstalar. Regras e formato em include/update.h.
 *
 * Área de trabalho no volume do sistema (FAT, fora do VFS: persist.c não a carrega nem a grava):
 *   /mOS/Sistema/Atualizacao/estado            BAIXANDO | PRONTA | APLICANDO
 *   /mOS/Sistema/Atualizacao/novos/<i>         arquivo i do manifesto novo, já conferido
 *   /mOS/Sistema/Atualizacao/plano.txt         "S <i> <caminho>" (trocar), "D <caminho>" (apagar) e "FIM"
 *   /mOS/Sistema/Atualizacao/manifesto-novo.txt
 *   /mOS/Sistema/Atualizacao/instalado.txt     manifesto do que está instalado (gravado pelo instalador e por cada atualização)
 *   /mOS/Sistema/Atualizacao/ultima.txt        versão aplicada no último reinício
 *
 * Ordem ao aplicar: estado APLICANDO (flush) -> trocas na ordem do plano (o carregador EFI é a última) -> apagar os arquivos
 * que saíram do payload -> instalado.txt <- manifesto-novo.txt -> ultima.txt -> remover novos/, plano e estado. Cada passo pode
 * ser repetido: arquivo já trocado não tem mais cópia em novos/, fat_replace reconhece a troca pela metade.
 */
#include "update.h"
#include "install.h"
#include "sysstate.h"
#include "crypto.h"
#include "vfs.h"
#include "net.h"

#define UDIR    "/mOS/Sistema/Atualizacao"
#define UNEW    UDIR "/novos"
#define UPLAN   UDIR "/plano.txt"
#define UMAN    UDIR "/manifesto-novo.txt"
#define UINST   UDIR "/instalado.txt"
#define UST     UDIR "/estado"
#define ULAST   UDIR "/ultima.txt"
#define MANIFEST_P "/mOS/Sistema/Instalacao/manifesto.txt"   /* no pacote (e na mídia live) */
#define PUBKEY_P   "/mOS/Sistema/Biblioteca/chave-atualizacao.pub"
#define LOADER  "/EFI/BOOT/BOOTX64.EFI"
#define MIB     (1ull << 20)
#define SLICE_MS 25

/* ------------------------------------------------------------------ manifesto */
typedef struct { char *path; u64 size; u8 hash[32]; } ment_t;
typedef struct { ment_t *e; int n; u8 *raw; u64 rawlen; char version[80]; } mlist_t;

static void mlist_free(mlist_t *m) {
    for (int i = 0; m->e && i < m->n; i++) kfree(m->e[i].path);
    kfree(m->e); kfree(m->raw);
    memset(m, 0, sizeof *m);
}

static int hexv(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; }
static bool unhex(const char *h, u8 *out, int n) {
    for (int i = 0; i < n; i++) {
        int a = hexv(h[i * 2]), b = hexv(h[i * 2 + 1]);
        if (a < 0 || b < 0) return false;
        out[i] = (u8)(a << 4 | b);
    }
    return true;
}

/* Linha "\nPREFIXO" mais ao fim do texto (o próprio início não conta: o cabeçalho vem antes) */
static char *find_line(char *d, u64 len, const char *prefix) {
    usize pl = strlen(prefix);
    for (u64 i = len; i > 0; i--)
        if (d[i - 1] == '\n' && i + pl <= len && !strncmp(d + i, prefix, pl)) return d + i;
    return NULL;
}

/* Valida e interpreta um manifesto. pk != NULL: exige a assinatura Ed25519 dessa chave. Fica com "d" (kmalloc). */
static bool parse_manifest(u8 *d, u64 len, const u8 *pk, mlist_t *out, char *err, usize cap) {
    memset(out, 0, sizeof *out);
#define BAD(...) do { snprintf(err, cap, __VA_ARGS__); kfree(d); kfree(txt); mlist_free(out); return false; } while (0)
    char *txt = NULL;
    if (!d || len < 64 || len > (8u << 20)) BAD("manifesto ausente ou de tamanho inválido");
    txt = kmalloc(len + 1);
    if (!txt) BAD("sem memória para o manifesto");
    memcpy(txt, d, len);
    txt[len] = 0;
    char *sha = find_line(txt, len, "ASSINATURA-SHA256 ");
    u8 want[32], got[32];
    if (!sha || !unhex(sha + 18, want, 32)) BAD("manifesto sem linha ASSINATURA-SHA256");
    sha256(txt, (usize)(sha - txt), got);
    if (memcmp(want, got, 32)) BAD("manifesto corrompido (SHA-256 não confere)");
    if (pk) {
        char *ed = find_line(txt, len, "ASSINATURA-ED25519 ");
        u8 sig[64];
        if (!ed || ed < sha || !unhex(ed + 19, sig, 64)) BAD("pacote sem assinatura Ed25519");
        if (!ed25519_check(sig, txt, (usize)(ed - txt), pk)) BAD("assinatura Ed25519 inválida: o pacote não foi gerado com a chave deste sistema");
    }
    *sha = 0;
    if (strncmp(txt, "mOS-MANIFESTO-1 ", 16)) BAD("versão de manifesto desconhecida");
    /* cabeçalho: mOS-MANIFESTO-1 <n> <total> [versão] (a versão é opcional e o instalador a ignora) */
    char *nl = strchr(txt, '\n');
    if (!nl) BAD("manifesto sem entradas");
    *nl = 0;
    int cnt = atoi(txt + 16);
    char *tp = strchr(txt + 16, ' ');
    u64 want_total = tp ? (u64)strtol(tp + 1, NULL, 10) : 0;
    char *vp = tp ? strchr(tp + 1, ' ') : NULL;
    if (vp) snprintf(out->version, sizeof out->version, "%s", vp + 1);
    if (cnt < 1 || cnt > 200000) BAD("manifesto com contagem inválida");
    out->e = kcalloc((usize)cnt, sizeof *out->e);
    if (!out->e) BAD("sem memória para o manifesto");
    u64 total = 0;
    char *line = nl + 1;
    while (*line && out->n < cnt) {
        nl = strchr(line, '\n');
        if (nl) *nl = 0;
        ment_t *m = &out->e[out->n];
        if (strlen(line) < 68 || line[64] != ' ' || !unhex(line, m->hash, 32)) BAD("linha de manifesto inválida");
        char *sp = strchr(line + 65, ' ');
        if (!sp) BAD("linha de manifesto sem caminho");
        m->size = (u64)strtol(line + 65, NULL, 10);
        if (!install_path_allowed(sp + 1)) BAD("caminho recusado no manifesto: %s", sp + 1);
        if (out->n && strcmp(out->e[out->n - 1].path, sp + 1) >= 0) BAD("manifesto fora de ordem ou com duplicata");
        m->path = strdup(sp + 1);
        if (!m->path) BAD("sem memória");
        total += m->size;
        out->n++;
        if (!nl) break;
        line = nl + 1;
    }
    if (out->n != cnt || total != want_total) BAD("manifesto incompleto (contagem ou total)");
    kfree(txt);
    out->raw = d;
    out->rawlen = len;
    return true;
#undef BAD
}

static int mfind(const mlist_t *m, const char *path) {
    int lo = 0, hi = m->n - 1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2, c = strcmp(m->e[mid].path, path);
        if (!c) return mid;
        if (c < 0) lo = mid + 1; else hi = mid - 1;
    }
    return -1;
}

/* ------------------------------------------------------------------ estado */
static update_status_t st;
static mlist_t nm, om;                 /* manifesto novo (do pacote) e o instalado */
static u8 *chg;                        /* chg[i]: arquivo i do manifesto novo precisa ser baixado */
static char **dels;                    /* caminhos do manifesto instalado que saíram do payload */
static int ndels;
static int phase, cur;
static bool fetching;
static http_req_t *req;
static bool is_http;
static char base[256];
static u8 *buf;                        /* conteúdo em trabalho (baixado ou relido) */
static u64 buflen;
static sha256_ctx hc;
static u64 hpos;
static const u8 *hdata;
static u64 hlen;

enum { PH_NONE, PH_MAN, PH_CMP, PH_CMP_HASH, PH_DL, PH_DL_HASH, PH_DL_WRITE, PH_DL_RB, PH_DL_DONE };

const update_status_t *update_status(void) { return &st; }

static fatfs_t *sysfs(void) { return persist_volume(); }

static void wipe_lists(void) {
    mlist_free(&nm); mlist_free(&om);
    kfree(chg); chg = NULL;
    for (int i = 0; i < ndels; i++) kfree(dels[i]);
    kfree(dels); dels = NULL; ndels = 0;
    kfree(buf); buf = NULL; buflen = 0;
    if (req) { http_free(req); req = NULL; }
    fetching = false;
}

static void fail(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(st.error, sizeof st.error, fmt, ap);
    va_end(ap);
    kprintf("update: ERRO: %s\n", st.error);
    st.state = UPD_ERROR;
    phase = PH_NONE;
    wipe_lists();
}

bool update_supported(char *why, usize cap) {
    const char *w = NULL;
    int s = sysstate();
    if (!sysfs()) w = "O sistema está rodando só da memória: não há disco do sistema para atualizar.";
    else if (s != SYS_INSTALLED && s != SYS_LIVE_PERSIST) w = "Só um mOS instalado (ou um pendrive com persistência) pode ser atualizado.";
    else if (!persist_writable()) w = "A gravação no disco do sistema não está liberada.";
    else if (!vfs_lookup("/Sistema/Biblioteca/chave-atualizacao.pub"))
        w = "Este sistema não tem chave de atualização (foi instalado antes do atualizador). Reinstale uma vez com uma imagem nova.";
    if (w && why) snprintf(why, cap, "%s", w);
    return w == NULL;
}

static bool load_pubkey(u8 pk[32]) {
    vnode_t *v = vfs_lookup("/Sistema/Biblioteca/chave-atualizacao.pub");
    return v && !v->dir && v->data && v->size >= 64 && unhex((const char *)v->data, pk, 32);
}

/* manifesto do que está instalado: instalado.txt; numa mídia live (persistente), o próprio manifesto do payload dela */
static u8 *read_installed(u64 *len) {
    if (!sysfs()) return NULL;
    u8 *d = fat_read_file(sysfs(), UINST, len);
    return d ? d : fat_read_file(sysfs(), MANIFEST_P, len);
}

const char *update_current_version(void) {
    static char v[80];
    static bool done;
    if (!done) {
        done = true;
        snprintf(v, sizeof v, "%s", MOS_VERSION);
        u64 n = 0;
        u8 *d = read_installed(&n);
        if (d && n > 16 && !strncmp((char *)d, "mOS-MANIFESTO-1 ", 16)) {
            u64 k = 0;
            while (k < n && d[k] != '\n') k++;
            if (k < n) {
                d[k] = 0;
                char *a = strchr((char *)d + 16, ' '), *b = a ? strchr(a + 1, ' ') : NULL;
                if (b && b[1]) snprintf(v, sizeof v, "%s", b + 1);
            }
        }
        kfree(d);
    }
    return v;
}

const char *update_last_applied(void) {
    static char v[80];
    static bool done;
    if (!done) {
        done = true;
        u64 n = 0;
        u8 *d = sysfs() ? fat_read_file(sysfs(), ULAST, &n) : NULL;
        if (d && n) {
            usize k = MIN((usize)n, sizeof v - 1);
            memcpy(v, d, k);
            v[k] = 0;
            char *nl = strchr(v, '\n');
            if (nl) *nl = 0;
        }
        kfree(d);
    }
    return v;
}

static char *read_small(const char *path) {
    u64 n = 0;
    u8 *d = sysfs() ? fat_read_file(sysfs(), path, &n) : NULL;
    if (!d) return NULL;
    char *s = kmalloc(n + 1);
    if (s) { memcpy(s, d, n); s[n] = 0; }
    kfree(d);
    return s;
}

bool update_ready(void) {
    if (st.state == UPD_READY) return true;
    char *s = read_small(UST);
    bool r = s && !strncmp(s, "PRONTA", 6);
    kfree(s);
    return r;
}

/* ------------------------------------------------------------------ origem (HTTP ou pasta do VFS) */
static void url_of(const char *path, char *out, usize cap) {
    usize n = (usize)snprintf(out, cap, "%s", base);
    static const char hx[] = "0123456789ABCDEF";
    for (const u8 *p = (const u8 *)path; *p && n + 4 < cap; p++) {
        u8 c = *p;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || strchr("/-._~", c)) out[n++] = (char)c;
        else { out[n++] = '%'; out[n++] = hx[c >> 4]; out[n++] = hx[c & 15]; }
    }
    out[n] = 0;
}

static bool fetch_start(const char *path, u64 expect) {
    kfree(buf); buf = NULL; buflen = 0;
    if (is_http) {
        char url[1400];
        url_of(path, url, sizeof url);
        req = http_get(url);
        if (!req) return false;
        req->max_body = expect ? expect + 1 : (16u << 20);
        return true;
    }
    return true;                       /* pasta local: lida no fetch_poll */
}

/* 1 = pronto (buf/buflen), 0 = em andamento, -1 = erro (st.error) */
static int fetch_poll(const char *path) {
    if (is_http) {
        int r = http_poll(req);
        if (!r) return 0;
        if (r < 0 || req->status != 200) {
            snprintf(st.error, sizeof st.error, "%s: %s", path, r < 0 ? req->error : "o servidor não tem o arquivo");
            if (r >= 0) snprintf(st.error, sizeof st.error, "%s: resposta HTTP %d", path, req->status);
            http_free(req); req = NULL;
            return -1;
        }
        buf = req->body; buflen = req->body_len;
        req->body = NULL;
        http_free(req); req = NULL;
        if (!buf) buf = kmalloc(1);
        return 1;
    }
    char vp[1300];
    snprintf(vp, sizeof vp, "%s%s", base, path);
    vnode_t *v = vfs_lookup(vp);
    if (v && v->ext) vfs_ensure(v);
    if (!v || v->dir) { snprintf(st.error, sizeof st.error, "%s: arquivo ausente na origem", vp); return -1; }
    buf = kmalloc(v->size + 1);
    if (!buf) { snprintf(st.error, sizeof st.error, "sem memória para %s (%llu bytes)", path, (u64)v->size); return -1; }
    if (v->size) memcpy(buf, v->data, v->size);
    buflen = v->size;
    return 1;
}

/* hash em fatias de tempo (arquivos de centenas de MB não congelam a interface) */
static void hash_begin(const u8 *d, u64 n) { sha256_init(&hc); hdata = d; hlen = n; hpos = 0; }
static bool hash_run(u8 out[32]) {
    u64 t0 = timer_ticks();
    while (hpos < hlen && timer_ticks() - t0 < SLICE_MS) {
        u64 k = MIN(4 * MIB, hlen - hpos);
        sha256_update(&hc, hdata + hpos, (usize)k);
        hpos += k;
    }
    if (hpos < hlen) return false;
    sha256_final(&hc, out);
    return true;
}

/* ------------------------------------------------------------------ verificar */
bool update_check(const char *source) {
    if (st.state == UPD_CHECKING || st.state == UPD_DOWNLOADING) return false;
    wipe_lists();
    memset(&st, 0, sizeof st);
    snprintf(st.cur_version, sizeof st.cur_version, "%s", update_current_version());
    char why[160];
    if (!update_supported(why, sizeof why)) { fail("%s", why); return false; }
    while (*source == ' ') source++;
    snprintf(st.source, sizeof st.source, "%s", source);
    snprintf(base, sizeof base, "%s", source);
    usize bl = strlen(base);
    while (bl > 1 && base[bl - 1] == '/') base[--bl] = 0;      /* "http://x/" e "/Volumes/MOS/" */
    is_http = !strncmp(base, "http://", 7) || !strncmp(base, "https://", 8);
    if (!is_http && base[0] != '/') { fail("Origem inválida: use http://servidor[:porta] ou uma pasta como /Volumes/MOS."); return false; }
    if (!fetch_start(MANIFEST_P, 0)) { fail("Não foi possível iniciar a conexão com %s.", base); return false; }
    st.state = UPD_CHECKING;
    snprintf(st.step, sizeof st.step, "Procurando atualizações em %s", base);
    phase = PH_MAN;
    kprintf("update: procurando em %s\n", base);
    return true;
}

static void finish_compare(void) {
    /* o que saiu do payload: só dá para saber com o manifesto instalado */
    if (om.n) {
        dels = kcalloc((usize)om.n, sizeof *dels);
        for (int i = 0; dels && i < om.n; i++) {
            if (mfind(&nm, om.e[i].path) >= 0) continue;
            if (!strcmp(om.e[i].path, LOADER)) continue;
            bool dir;
            if (!fat_stat(sysfs(), om.e[i].path, &dir, NULL) || dir) continue;
            dels[ndels++] = strdup(om.e[i].path);
        }
    }
    st.files_removed = ndels;
    snprintf(st.new_version, sizeof st.new_version, "%s", nm.version[0] ? nm.version : "(sem número de versão)");
    if (!st.files_changed && !ndels) {
        st.state = UPD_UPTODATE;
        snprintf(st.step, sizeof st.step, "O mOS está atualizado");
        kprintf("update: nada a atualizar (%d arquivos conferidos)\n", nm.n);
        wipe_lists();
    } else {
        st.state = UPD_AVAILABLE;
        snprintf(st.step, sizeof st.step, "Atualização disponível");
        kprintf("update: %d arquivo(s) a baixar (%llu KB), %d a apagar%s\n", st.files_changed, st.bytes_changed >> 10, ndels,
                st.loader_changed ? ", inclui o kernel" : "");
    }
    phase = PH_NONE;
}

static void mark_changed(int i) {
    chg[i] = 1;
    st.files_changed++;
    st.bytes_changed += nm.e[i].size;
    if (!strcmp(nm.e[i].path, LOADER)) st.loader_changed = true;
}

/* uma entrada do manifesto novo: true = decidida agora; false = começou um hash em fatias (PH_CMP_HASH) */
static bool compare_one(int i) {
    ment_t *e = &nm.e[i];
    int o = om.n ? mfind(&om, e->path) : -1;
    bool same_as_installed = o >= 0 && om.e[o].size == e->size && !memcmp(om.e[o].hash, e->hash, 32);
    if (!strcmp(e->path, LOADER)) {
        u64 n = 0;
        bool dir;
        if (same_as_installed && fat_stat(sysfs(), LOADER, &dir, &n) && n == e->size) return true;
        kfree(buf);
        buf = fat_read_file(sysfs(), LOADER, &buflen);
        if (!buf || buflen != e->size) { mark_changed(i); return true; }
        hash_begin(buf, buflen);
        return false;
    }
    vnode_t *v = vfs_lookup(e->path + 4);                 /* "/mOS/x" no disco = "/x" no VFS */
    if (!v || v->dir || v->ext || v->size != e->size) { mark_changed(i); return true; }
    if (same_as_installed) return true;                   /* o manifesto instalado já diz que é este conteúdo */
    hash_begin(v->data ? v->data : (const u8 *)"", v->size);
    return false;
}

/* ------------------------------------------------------------------ baixar */
static bool put(const char *path, const void *d, u64 n) {
    char tmp[1100];
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p != '/') continue;
        *p = 0;
        bool dir;
        if (!(fat_stat(sysfs(), tmp, &dir, NULL) && dir) && !fat_mkdir(sysfs(), tmp)) return false;
        *p = '/';
    }
    return fat_write_file(sysfs(), path, d, n);
}

static void clear_workdir(fatfs_t *fs) {
    bool dir;
    if (fat_stat(fs, UNEW, &dir, NULL)) fat_remove(fs, UNEW);
    if (fat_stat(fs, UPLAN, &dir, NULL)) fat_remove(fs, UPLAN);
    if (fat_stat(fs, UMAN, &dir, NULL)) fat_remove(fs, UMAN);
    if (fat_stat(fs, UST, &dir, NULL)) fat_remove(fs, UST);
}

bool update_download(void) {
    if (st.state != UPD_AVAILABLE || !nm.n) return false;
    clear_workdir(sysfs());
    if (!put(UST, "BAIXANDO\n", 9) || !fat_mkdir(sysfs(), UNEW)) { fail("Não foi possível preparar a área de atualização no disco."); return false; }
    st.state = UPD_DOWNLOADING;
    st.files_done = 0;
    st.bytes_done = 0;
    st.percent = 0;
    cur = -1;
    fetching = false;
    phase = PH_DL;
    kprintf("update: baixando %d arquivo(s)\n", st.files_changed);
    return true;
}

void update_cancel(void) {
    bool busy = st.state == UPD_DOWNLOADING || st.state == UPD_READY;
    wipe_lists();
    if (busy && sysfs() && persist_writable()) { clear_workdir(sysfs()); fat_flush(sysfs()); }
    memset(&st, 0, sizeof st);
    phase = PH_NONE;
}

static bool write_plan(void) {
    usize cap = 64 + (usize)st.files_changed * 1040 + (usize)ndels * 1040, n = 0;
    char *p = kmalloc(cap);
    if (!p) return false;
    int loader = -1;
    for (int i = 0; i < nm.n; i++) {
        if (!chg[i]) continue;
        if (!strcmp(nm.e[i].path, LOADER)) { loader = i; continue; }
        n += (usize)snprintf(p + n, cap - n, "S %d %s\n", i, nm.e[i].path);
    }
    if (loader >= 0) n += (usize)snprintf(p + n, cap - n, "S %d %s\n", loader, LOADER);   /* o kernel por último */
    for (int i = 0; i < ndels; i++) n += (usize)snprintf(p + n, cap - n, "D %s\n", dels[i]);
    n += (usize)snprintf(p + n, cap - n, "FIM\n");
    bool ok = put(UPLAN, p, n);
    kfree(p);
    return ok;
}

void update_poll(void) {
    if (phase == PH_NONE) return;
    u8 h[32];
    char np[64];
    switch (phase) {
    case PH_MAN: {
        int r = fetch_poll(MANIFEST_P);
        if (!r) return;
        if (r < 0) { char e[192]; snprintf(e, sizeof e, "%s", st.error); fail("Pacote não encontrado: %s", e); return; }
        u8 pk[32];
        if (!load_pubkey(pk)) { fail("Chave de atualização deste sistema ilegível."); return; }
        char err[160];
        u8 *d = buf; buf = NULL;
        if (!parse_manifest(d, buflen, pk, &nm, err, sizeof err)) { fail("Pacote recusado: %s.", err); return; }
        if (mfind(&nm, LOADER) < 0 || mfind(&nm, PUBKEY_P) < 0) { fail("Pacote recusado: falta o carregador EFI ou a chave de atualização."); return; }
        u64 n = 0;
        u8 *od = read_installed(&n);
        if (od && !parse_manifest(od, n, NULL, &om, err, sizeof err)) kprintf("update: manifesto instalado ignorado (%s)\n", err);
        chg = kcalloc((usize)nm.n, 1);
        if (!chg) { fail("Sem memória."); return; }
        st.files_total = nm.n;
        kprintf("update: manifesto do pacote conferido (Ed25519): %d arquivos, versão %s; instalado: %d entradas\n", nm.n,
                nm.version[0] ? nm.version : "?", om.n);
        cur = 0;
        phase = PH_CMP;
        return;
    }
    case PH_CMP: {
        u64 t0 = timer_ticks();
        while (cur < nm.n && timer_ticks() - t0 < SLICE_MS) {
            if (!compare_one(cur)) { phase = PH_CMP_HASH; break; }
            cur++;
        }
        snprintf(st.step, sizeof st.step, "Comparando com o sistema instalado (%d de %d)", cur, nm.n);
        st.percent = nm.n ? cur * 100 / nm.n : 100;
        if (phase == PH_CMP && cur >= nm.n) finish_compare();
        return;
    }
    case PH_CMP_HASH:
        if (!hash_run(h)) return;
        if (memcmp(h, nm.e[cur].hash, 32)) mark_changed(cur);
        kfree(buf); buf = NULL;
        cur++;
        phase = PH_CMP;
        return;
    case PH_DL: {
        if (!fetching) {                                  /* próximo arquivo */
            do cur++; while (cur < nm.n && !chg[cur]);
            if (cur >= nm.n) { phase = PH_DL_DONE; return; }
            snprintf(st.step, sizeof st.step, "Baixando %s", nm.e[cur].path);
            if (!fetch_start(nm.e[cur].path, nm.e[cur].size)) { fail("Falha ao pedir %s.", nm.e[cur].path); return; }
            fetching = true;
        }
        int r = fetch_poll(nm.e[cur].path);
        if (!r) {
            if (req && st.bytes_changed) {
                float f = http_progress(req);
                st.percent = (int)((st.bytes_done + (u64)(f * (float)nm.e[cur].size)) * 100 / st.bytes_changed);
            }
            return;
        }
        fetching = false;
        if (r < 0) { char e[192]; snprintf(e, sizeof e, "%s", st.error); fail("Falha ao baixar: %s", e); return; }
        if (buflen != nm.e[cur].size) { fail("%s: tamanho diferente do manifesto (%llu, esperado %llu).", nm.e[cur].path, buflen, nm.e[cur].size); return; }
        hash_begin(buf, buflen);
        phase = PH_DL_HASH;
        return;
    }
    case PH_DL_HASH:
        if (!hash_run(h)) return;
        if (memcmp(h, nm.e[cur].hash, 32)) { fail("%s: conteúdo baixado não confere com o manifesto (SHA-256).", nm.e[cur].path); return; }
        phase = PH_DL_WRITE;
        return;
    case PH_DL_WRITE:
        snprintf(np, sizeof np, UNEW "/%d", cur);
        snprintf(st.step, sizeof st.step, "Gravando %s", nm.e[cur].path);
        if (!fat_write_file(sysfs(), np, buf, buflen)) { fail("Não foi possível gravar %s na área de atualização (disco cheio?).", nm.e[cur].path); return; }
        kfree(buf);
        buf = fat_read_file(sysfs(), np, &buflen);         /* releitura: confere o que foi para o disco */
        if (!buf || buflen != nm.e[cur].size) { fail("Releitura de %s falhou.", nm.e[cur].path); return; }
        hash_begin(buf, buflen);
        phase = PH_DL_RB;
        return;
    case PH_DL_RB:
        if (!hash_run(h)) return;
        if (memcmp(h, nm.e[cur].hash, 32)) { fail("%s: o que foi gravado difere do baixado (SHA-256).", nm.e[cur].path); return; }
        st.bytes_done += nm.e[cur].size;
        st.files_done++;
        st.percent = st.bytes_changed ? (int)(st.bytes_done * 100 / st.bytes_changed) : 100;
        kfree(buf); buf = NULL;
        phase = PH_DL;
        return;
    case PH_DL_DONE:
        snprintf(st.step, sizeof st.step, "Preparando a instalação");
        if (!write_plan() || !put(UMAN, nm.raw, nm.rawlen) || !fat_flush(sysfs())) { fail("Não foi possível gravar o plano de atualização."); return; }
        if (!put(UST, "PRONTA\n", 7) || !fat_flush(sysfs())) { fail("Não foi possível marcar a atualização como pronta."); return; }
        st.state = UPD_READY;
        st.percent = 100;
        snprintf(st.step, sizeof st.step, "Pronta: será instalada ao reiniciar");
        kprintf("update: PRONTA (%d arquivos, %llu KB conferidos; %d a apagar) — aplica ao reiniciar\n", st.files_done, st.bytes_done >> 10, ndels);
        wipe_lists();
        phase = PH_NONE;
        return;
    }
}

/* ------------------------------------------------------------------ aplicar (desligar/reiniciar ou boot) */
static bool mkdirs_fs(fatfs_t *fs, const char *path) {
    char tmp[1100];
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p != '/') continue;
        *p = 0;
        bool dir;
        if (!(fat_stat(fs, tmp, &dir, NULL) && dir) && !fat_mkdir(fs, tmp)) return false;
        *p = '/';
    }
    return true;
}

static char *read_fs(fatfs_t *fs, const char *path) {
    u64 n = 0;
    u8 *d = fat_read_file(fs, path, &n);
    if (!d) return NULL;
    char *s = kmalloc(n + 1);
    if (s) { memcpy(s, d, n); s[n] = 0; }
    kfree(d);
    return s;
}

bool update_boot_pending(fatfs_t *fs) {
    char *s = fs ? read_fs(fs, UST) : NULL;
    bool r = s && (!strncmp(s, "PRONTA", 6) || !strncmp(s, "APLICANDO", 9));
    kfree(s);
    return r;
}

/* versão do cabeçalho do manifesto novo (para ultima.txt) */
static void man_version(fatfs_t *fs, char *out, usize cap) {
    out[0] = 0;
    char *m = read_fs(fs, UMAN);
    if (!m) return;
    char *nl = strchr(m, '\n');
    if (nl) *nl = 0;
    char *a = strchr(m + (strlen(m) > 16 ? 16 : 0), ' '), *b = a ? strchr(a + 1, ' ') : NULL;
    snprintf(out, cap, "%s", b && b[1] ? b + 1 : "nova");
    kfree(m);
}

/* aplica o plano; *loader = o carregador EFI foi trocado nesta passada. false = nada aplicado ou erro (o estado fica) */
static bool apply(fatfs_t *fs, bool *loader) {
    *loader = false;
    char *s = read_fs(fs, UST);
    bool ok = s && (!strncmp(s, "PRONTA", 6) || !strncmp(s, "APLICANDO", 9));
    bool resume = s && !strncmp(s, "APLICANDO", 9);
    kfree(s);
    if (!ok) return false;
    char *plan = read_fs(fs, UPLAN);
    usize pl = plan ? strlen(plan) : 0;
    if (!plan || pl < 4 || strcmp(plan + pl - 4, "FIM\n")) {
        kprintf("update: plano ausente ou incompleto: atualização descartada\n");
        kfree(plan);
        clear_workdir(fs);
        fat_flush(fs);
        return false;
    }
    if (!resume) {
        if (!fat_write_file(fs, UST, "APLICANDO\n", 10) || !fat_flush(fs)) { kfree(plan); kprintf("update: não foi possível marcar APLICANDO\n"); return false; }
    }
    kprintf("update: aplicando%s\n", resume ? " (retomando depois de uma interrupção)" : "");
    u64 t0 = timer_ticks();
    int done = 0, errs = 0, removed = 0;
    for (char *line = plan; *line;) {
        char *nl = strchr(line, '\n');
        if (nl) *nl = 0;
        if (line[0] == 'S' && line[1] == ' ') {
            int idx = atoi(line + 2);
            char *path = strchr(line + 2, ' ');
            char staged[64];
            snprintf(staged, sizeof staged, UNEW "/%d", idx);
            bool dir;
            if (path && fat_stat(fs, staged, &dir, NULL) && !dir) {
                path++;
                if (!mkdirs_fs(fs, path) || !fat_replace(fs, staged, path)) { errs++; kprintf("update: falha ao trocar %s\n", path); }
                else { done++; if (!strcmp(path, LOADER)) { *loader = true; fat_flush(fs); } }
            }
        } else if (line[0] == 'D' && line[1] == ' ') {
            bool dir;
            if (fat_stat(fs, line + 2, &dir, NULL) && !dir) { if (fat_remove(fs, line + 2)) removed++; else errs++; }
        }
        if (!nl) break;
        line = nl + 1;
    }
    kfree(plan);
    if (errs) {
        fat_flush(fs);
        kprintf("update: %d erro(s) ao aplicar: o estado APLICANDO fica para o próximo boot tentar de novo\n", errs);
        return false;
    }
    char ver[80];
    man_version(fs, ver, sizeof ver);
    bool dir;
    if (fat_stat(fs, UMAN, &dir, NULL)) {
        if (fat_stat(fs, MANIFEST_P, &dir, NULL)) {            /* mídia live: o instalador dela continua valendo */
            u64 n = 0;
            u8 *m = fat_read_file(fs, UMAN, &n);
            if (m) fat_write_file(fs, MANIFEST_P, m, n);
            kfree(m);
        }
        fat_replace(fs, UMAN, UINST);
    }
    char line[96];
    int n = snprintf(line, sizeof line, "%s\n", ver);
    if (fat_stat(fs, ULAST, &dir, NULL)) fat_remove(fs, ULAST);
    fat_write_file(fs, ULAST, line, (u64)n);
    clear_workdir(fs);
    fat_flush(fs);
    kprintf("update: APLICADA (versão %s): %d arquivo(s) trocado(s), %d apagado(s), %llu ms%s\n", ver, done, removed,
            timer_ticks() - t0, *loader ? "; kernel novo no próximo boot" : "");
    return true;
}

bool update_boot_apply(fatfs_t *fs) {
    bool loader = false;
    apply(fs, &loader);
    return loader;
}

void update_on_shutdown(void) {
    fatfs_t *fs = sysfs();
    if (!fs || !persist_writable() || !update_boot_pending(fs)) return;
    if (st.state == UPD_DOWNLOADING || st.state == UPD_CHECKING) return;   /* incompleto: nada a aplicar */
    bool loader;
    apply(fs, &loader);
}
