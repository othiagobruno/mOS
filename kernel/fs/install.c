/*
 * mOS — instalador: copia o volume do sistema em execução (EFI\BOOT\BOOTX64.EFI + \mOS\) para outro
 * disco, que é reparticionado (GPT com uma partição EFI) e formatado em FAT32. O resultado boota
 * sozinho pelo UEFI e já tem a persistência funcionando.
 *
 * Trabalho incremental: install_poll() executa uma fatia (particionar, formatar, listar, ou copiar
 * um arquivo) e volta, para a interface continuar respondendo e mostrar o progresso.
 */
#include "vfs.h"
#include "storage.h"

enum { PH_PART, PH_LIST, PH_COPY, PH_FINISH };

typedef struct { char *path; bool dir; u64 size; } item_t;

static install_status_t st;
static int phase, target;
static blkdev_t *part;
static fatfs_t *dst;
static item_t *items;
static int nitems, capitems, next_item;

const install_status_t *install_status(void) { return &st; }

bool install_can_target(int disk, char *why, usize n) {
    blkdev_t *d = storage_disk(disk);
    const char *r = NULL;
    if (!d) r = "Disco inexistente.";
    else if (!persist_volume()) r = "O sistema não está rodando de um disco (somente RAM): não há o que copiar.";
    else if (disk == persist_disk_index()) r = "Este é o disco de onde o sistema está rodando.";
    else if (!blk_writable(d)) r = "O disco está protegido contra gravação.";
    else if (d->size < (64ull << 20)) r = "O disco é pequeno demais (mínimo de 64 MB).";
    if (r && why) snprintf(why, n, "%s", r);
    return r == NULL;
}

static void fail(const char *msg) {
    st.state = INSTALL_ERROR;
    snprintf(st.error, sizeof st.error, "%s", msg);
    kprintf("install: ERRO: %s\n", msg);
    if (dst) { fat_unmount(dst); dst = NULL; }
    if (part) { blk_release(part); part = NULL; }
}

static void free_items(void) {
    for (int i = 0; i < nitems; i++) kfree(items[i].path);
    kfree(items);
    items = NULL;
    nitems = capitems = next_item = 0;
}

bool install_start(int disk) {
    if (st.state == INSTALL_RUNNING) return false;
    char why[128];
    if (!install_can_target(disk, why, sizeof why)) {
        memset(&st, 0, sizeof st);
        st.state = INSTALL_ERROR;
        snprintf(st.error, sizeof st.error, "%s", why);
        return false;
    }
    free_items();
    memset(&st, 0, sizeof st);
    st.state = INSTALL_RUNNING;
    snprintf(st.step, sizeof st.step, "Preparando %s", storage_disk_desc(disk));
    target = disk;
    phase = PH_PART;
    kprintf("install: instalando em %s (%s)\n", storage_disk_name(disk), storage_disk_desc(disk));
    return true;
}

/* ---- listagem recursiva do volume de origem */
typedef struct { const char *base; int depth; } lctx_t;
static void list_dir(const char *path, int depth);

static void add_item(const char *path, bool dir, u64 size) {
    if (nitems == capitems) {
        int nc = capitems ? capitems * 2 : 64;
        item_t *ni = kmalloc(nc * sizeof *ni);
        if (!ni) return;
        if (items) memcpy(ni, items, nitems * sizeof *ni);
        kfree(items);
        items = ni;
        capitems = nc;
    }
    items[nitems].path = strdup(path);
    items[nitems].dir = dir;
    items[nitems].size = size;
    nitems++;
    if (!dir) st.bytes_total += size, st.files_total++;
}

static void list_cb(void *ctx, const char *name, bool dir, u64 size) {
    lctx_t *c = ctx;
    char p[1024];
    snprintf(p, sizeof p, "%s/%s", c->base, name);
    add_item(p, dir, size);
    if (dir) list_dir(p, c->depth + 1);
}

static void list_dir(const char *path, int depth) {
    if (depth > 24) return;
    lctx_t c = {path, depth};
    fat_list(persist_volume(), path[0] ? path : "/", list_cb, &c);
}

void install_poll(void) {
    if (st.state != INSTALL_RUNNING) return;
    switch (phase) {
    case PH_PART: {
        blkdev_t *disk = storage_disk(target);
        u64 off = 0, size = 0;
        snprintf(st.step, sizeof st.step, "Particionando %s", storage_disk_name(target));
        if (!gpt_create_single_esp(disk, "mOS", &off, &size)) { fail("Não foi possível gravar a tabela de partições."); return; }
        part = blk_slice(disk, off, size);
        snprintf(st.step, sizeof st.step, "Formatando (FAT32)");
        if (!part || !fat_format(part, "MOS")) { fail("Não foi possível formatar o disco."); return; }
        dst = fat_mount(part);
        if (!dst) { fail("O volume recém-formatado não pôde ser montado."); return; }
        st.percent = 3;
        phase = PH_LIST;
        return;
    }
    case PH_LIST:
        snprintf(st.step, sizeof st.step, "Lendo os arquivos do sistema");
        persist_sync();                         /* o que ainda está só na RAM também vai */
        /* só o sistema: o carregador e \mOS\. Nada de arquivos do firmware (ex.: NvVars do OVMF, que
         * restauraria uma ordem de boot do disco antigo) nem de outros sistemas no mesmo ESP. */
        add_item("/EFI", true, 0);
        add_item("/EFI/BOOT", true, 0);
        add_item("/EFI/BOOT/BOOTX64.EFI", false, 0);
        { bool d; u64 sz = 0; fat_stat(persist_volume(), "/EFI/BOOT/BOOTX64.EFI", &d, &sz); items[nitems - 1].size = sz; st.bytes_total += sz; }
        add_item("/mOS", true, 0);
        list_dir("/mOS", 0);
        kprintf("install: %d arquivos e pastas, %llu KB\n", nitems, st.bytes_total >> 10);
        phase = PH_COPY;
        st.percent = 5;
        return;
    case PH_COPY: {
        if (next_item >= nitems) { phase = PH_FINISH; return; }
        item_t *it = &items[next_item++];
        snprintf(st.step, sizeof st.step, "Copiando %s", it->path);
        if (it->dir) {
            if (!fat_mkdir(dst, it->path)) { char m[160]; snprintf(m, sizeof m, "Falha ao criar %s.", it->path); fail(m); return; }
        } else {
            u64 len = 0;
            u8 *data = fat_read_file(persist_volume(), it->path, &len);
            bool ok = data && fat_write_file(dst, it->path, data, len);
            kfree(data);
            if (!ok) { char m[160]; snprintf(m, sizeof m, "Falha ao copiar %s (disco cheio?).", it->path); fail(m); return; }
            st.bytes_done += len;
            st.files_done++;
        }
        u64 tot = st.bytes_total ? st.bytes_total : 1;
        st.percent = 5 + (int)(st.bytes_done * 93 / tot);
        return;
    }
    case PH_FINISH: {
        snprintf(st.step, sizeof st.step, "Concluindo");
        bool dir;
        bool boots = fat_stat(dst, "/EFI/BOOT/BOOTX64.EFI", &dir, NULL) && !dir;
        fat_unmount(dst);
        dst = NULL;
        blk_release(part);
        part = NULL;
        free_items();
        if (!boots) { fail("O carregador EFI não foi copiado."); return; }
        st.percent = 100;
        st.state = INSTALL_DONE;
        snprintf(st.step, sizeof st.step, "Instalação concluída");
        kprintf("install: concluído (%d arquivos, %llu KB)\n", st.files_done, st.bytes_done >> 10);
        return;
    }
    }
}

/* Autoteste: um disco cujo setor 0 começa com "mOS-INSTALL-TEST" recebe a instalação no boot
 * (tools\qemu-test.ps1 -InstallTest). Discos comuns nunca são tocados. */
void install_selftest(void) {
    static const char magic[] = "mOS-INSTALL-TEST";
    u8 s[512];
    for (int i = 0; i < storage_disk_count(); i++) {
        blkdev_t *d = storage_disk(i);
        if (!blk_read(d, 0, s, sizeof s) || memcmp(s, magic, sizeof magic - 1)) continue;
        if (!install_start(i)) { kprintf("install: autoteste não pôde começar: %s\n", st.error); return; }
        u64 t0 = timer_ticks();
        while (st.state == INSTALL_RUNNING) install_poll();
        kprintf("install: autoteste %s em %llu ms (%d%%)\n", st.state == INSTALL_DONE ? "OK" : "FALHOU", timer_ticks() - t0, st.percent);
        return;
    }
}
