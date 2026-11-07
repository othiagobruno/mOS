/*
 * mOS — persistência do VFS no volume do sistema (FAT32 de boot, pasta \mOS\).
 *
 * No boot, se algum disco SATA tiver uma partição FAT32 com \mOS\, a árvore inteira é carregada
 * de lá (no lugar dos arquivos entregues pelo UEFI, que perdem acentos e param em 64 arquivos).
 * Depois disso, o VFS avisa cada mudança:
 *   - pastas criadas, apagadas e renomeadas/movidas vão para o disco na hora;
 *   - arquivos alterados ficam "sujos" e são gravados inteiros por persist_tick() ~1,5 s depois da
 *     última alteração (ou em persist_sync(), chamado antes de desligar/reiniciar).
 * /Volumes e /tmp nunca são gravados aqui (volumes graváveis de /Volumes: vfs_bridge.c). Caminho no disco = "/mOS" + caminho no VFS.
 */
#include "vfs.h"
#include "storage.h"

#define SYS_DIR       "/mOS"
#define SYNC_DELAY_MS 1500
#ifndef MAX_LOAD                       /* sobrescrevível em builds de teste (ex.: -DMAX_LOAD=...) */
#define MAX_LOAD      (512ull << 20)  /* o Codex CLI (Rust, um binário só) tem 277 MB; antes 256 MB */
#endif

static fatfs_t *sys;
static bool enabled;
static int quiet;            /* > 0: mudanças no VFS não são propagadas (carga inicial) */
static bool pending;
static u64 last_change;
static int nfiles, ndirs;

bool persist_available(void) { return sys != NULL; }
fatfs_t *persist_volume(void) { return sys; }
static int sys_disk = -1;
int persist_disk_index(void) { return sys_disk; }

/* ------------------------------------------------------------ localizar o volume */
/* Pode haver mais de um disco com \mOS\ (ex.: pendrive de instalação + disco interno). O volume
 * certo é aquele de onde o firmware carregou os arquivos: pontua cada candidato pelos arquivos
 * entregues pelo UEFI (g_boot->files) que existem nele com o mesmo tamanho. */
static fatfs_t *best;
static blkdev_t *best_part;
static int best_score = -1;
static const char *best_disk;
static u64 best_size;
static const char *cur_disk;
static int cur_idx, best_idx = -1;

static int boot_score(fatfs_t *fs) {
    int score = 0;
    char path[160];
    for (u32 i = 0; i < g_boot->file_count; i++) {
        boot_file_t *bf = &g_boot->files[i];
        snprintf(path, sizeof path, SYS_DIR "%s", bf->path);
        bool dir;
        u64 size;
        if (!fat_stat(fs, path, &dir, &size)) continue;
        if (bf->size == ~0ULL ? dir : (!dir && size == bf->size)) score++;
    }
    return score;
}

static bool try_part(blkdev_t *disk, u64 off, u64 size) {
    blkdev_t *p = blk_slice(disk, off, size);
    if (!p) return false;
    fatfs_t *fs = fat_mount(p);
    bool dir = false;
    if (fs && fat_stat(fs, SYS_DIR, &dir, NULL) && dir) {
        int score = boot_score(fs);
        kprintf("persist: candidato %s (\"%s\", %llu MB): %d de %u arquivos do boot conferem\n", cur_disk,
                fat_label(fs)[0] ? fat_label(fs) : "sem nome", size >> 20, score, g_boot->file_count);
        if (score > best_score) {
            if (best) { fat_unmount(best); blk_release(best_part); }
            best = fs; best_part = p; best_score = score; best_disk = cur_disk; best_size = size; best_idx = cur_idx;
            return true;
        }
    }
    fat_unmount(fs);
    blk_release(p);
    return false;
}

static bool scan_disk(blkdev_t *disk) {
    u8 *s = kmalloc(1024);
    if (!s || !blk_read(disk, 0, s, 1024)) { kfree(s); return false; }
    bool found = false;
    if (!memcmp(s + 512, "EFI PART", 8)) {
        u64 lba = le64(s + 512 + 72);
        u32 cnt = MIN(le32(s + 512 + 80), 128u), esz = le32(s + 512 + 84);
        u8 *e = esz >= 56 && esz <= 4096 ? kmalloc(esz) : NULL;
        for (u32 i = 0; e && i < cnt && !found; i++) {
            if (!blk_read(disk, lba * 512 + (u64)i * esz, e, esz)) break;
            static const u8 zero[16];
            if (!memcmp(e, zero, 16)) continue;
            u64 first = le64(e + 32), last = le64(e + 40);
            if (last >= first) found = try_part(disk, first * 512, (last - first + 1) * 512);
        }
        kfree(e);
    } else if (s[510] == 0x55 && s[511] == 0xAA) {
        for (int i = 0; i < 4 && !found; i++) {
            const u8 *e = s + 446 + i * 16;
            if (e[4] == 0x0B || e[4] == 0x0C || e[4] == 0xEF)
                found = try_part(disk, (u64)le32(e + 8) * 512, (u64)le32(e + 12) * 512);
        }
        if (!found) found = try_part(disk, 0, disk->size);   /* "superfloppy" sem partições */
    }
    kfree(s);
    return found;
}

/* ------------------------------------------------------------ carga inicial */
typedef struct { char vpath[1024]; char fpath[1040]; int depth; } walk_t;

static void load_tree(const char *vpath, int depth);

static void load_entry(void *ctx, const char *name, bool is_dir, u64 size) {
    walk_t *w = ctx;
    char vp[1024], fp[1040];
    snprintf(vp, sizeof vp, "%s/%s", w->vpath, name);
    snprintf(fp, sizeof fp, SYS_DIR "%s", vp);
    if (is_dir) {
        vfs_mkdir(vp);
        ndirs++;
        load_tree(vp, w->depth + 1);
        return;
    }
    if (size > MAX_LOAD) { kprintf("persist: %s grande demais (%llu MB), ignorado\n", vp, size >> 20); return; }
    u64 len = 0;
    u8 *data = fat_read_file(sys, fp, &len);
    if (!data) { kprintf("persist: erro ao ler %s\n", fp); return; }
    vnode_t *v = vfs_create(vp);
    if (!v) { kfree(data); return; }
    if (v->cap) kfree(v->data);
    v->data = data;
    v->size = len;
    v->cap = len + 1;
    v->mtime = 0;
    v->dirty = false;
    nfiles++;
}

static void load_tree(const char *vpath, int depth) {
    if (depth > 16) return;
    walk_t *w = kmalloc(sizeof *w);
    if (!w) return;
    strncpy(w->vpath, vpath, sizeof w->vpath - 1);
    w->vpath[sizeof w->vpath - 1] = 0;
    snprintf(w->fpath, sizeof w->fpath, SYS_DIR "%s", vpath);
    w->depth = depth;
    fat_list(sys, w->fpath, load_entry, w);
    kfree(w);
}

/* chamado por vfs_init(): devolve true se carregou a árvore do disco */
bool persist_load(void) {
    for (int i = 0; i < storage_disk_count(); i++) { cur_disk = storage_disk_name(i); cur_idx = i; scan_disk(storage_disk(i)); }
    if (!best) return false;
    sys = best;
    sys_disk = best_idx;
    fat_set_system(sys);
    kprintf("persist: volume do sistema \"%s\" (FAT32, %llu MB) em %s\n",
            fat_label(sys)[0] ? fat_label(sys) : "sem nome", best_size >> 20, best_disk);
    quiet++;
    u64 t0 = timer_ticks();
    load_tree("", 0);
    quiet--;
    kprintf("persist: %d arquivos e %d pastas carregados do disco em %llu ms\n", nfiles, ndirs, timer_ticks() - t0);
    return true;
}

void persist_enable(void) { enabled = sys != NULL; }

/* ------------------------------------------------------------ ganchos do VFS */
static bool tracked_path(const char *p) {
    if (!strncmp(p, "/Volumes", 8) && (p[8] == 0 || p[8] == '/')) return false;
    if (!strncmp(p, "/tmp", 4) && (p[4] == 0 || p[4] == '/')) return false;
    return p[1] != 0;
}

static bool tracked(vnode_t *n, char *vpath, usize cap) {
    if (!enabled || quiet || !n) return false;
    for (vnode_t *p = n; p; p = p->parent) if (p->ext) return false;
    vfs_path(n, vpath, cap);
    return tracked_path(vpath);
}

static void fpath_of(const char *vpath, char *out, usize cap) { snprintf(out, cap, SYS_DIR "%s", vpath); }

/* cria no disco as pastas que faltam até o pai de fpath */
static bool mkdirs_parent(const char *fpath) {
    char tmp[1040];
    strncpy(tmp, fpath, sizeof tmp - 1);
    tmp[sizeof tmp - 1] = 0;
    char *slash = strrchr(tmp, '/');
    if (!slash || slash == tmp) return true;
    *slash = 0;
    bool dir;
    if (fat_stat(sys, tmp, &dir, NULL)) return dir;
    if (!mkdirs_parent(tmp)) return false;
    return fat_mkdir(sys, tmp);
}

void persist_mark(vnode_t *n) {
    char vp[1024];
    vol_mark(n);                         /* dentro de /Volumes/<gravável>: vai para aquele disco */
    if (!tracked(n, vp, sizeof vp)) return;
    if (n->dir) {
        char fp[1040];
        fpath_of(vp, fp, sizeof fp);
        if (!mkdirs_parent(fp) || !fat_mkdir(sys, fp)) kprintf("persist: não foi possível criar %s\n", fp);
        return;
    }
    n->dirty = true;
    pending = true;
    last_change = timer_ticks();
}

void persist_delete(vnode_t *n) {
    char vp[1024], fp[1040];
    vol_delete(n);
    if (!tracked(n, vp, sizeof vp)) return;
    fpath_of(vp, fp, sizeof fp);
    bool dir;
    if (fat_stat(sys, fp, &dir, NULL) && !fat_remove(sys, fp)) kprintf("persist: não foi possível apagar %s\n", fp);
}

/* marca uma subárvore inteira como nova no disco (ex.: movida de /tmp para a pasta pessoal) */
static void add_tree(vnode_t *n) {
    persist_mark(n);
    if (n->dir) for (vnode_t *c = n->child; c; c = c->next) add_tree(c);
}

void persist_move(const char *old_vpath, vnode_t *n) {
    vol_move(old_vpath, n);              /* antes: tira de n as referências ao volume de origem */
    if (!enabled || quiet) return;
    char vp[1024];
    bool was = tracked_path(old_vpath), now = tracked(n, vp, sizeof vp);
    if (!was && !now) return;
    char from[1040], to[1040];
    fpath_of(old_vpath, from, sizeof from);
    if (was && !now) {                                   /* saiu da área persistente */
        bool dir;
        if (fat_stat(sys, from, &dir, NULL)) fat_remove(sys, from);
        return;
    }
    fpath_of(vp, to, sizeof to);
    bool dir;
    if (was && fat_stat(sys, from, &dir, NULL)) {
        if (mkdirs_parent(to) && fat_rename(sys, from, to)) return;
        kprintf("persist: não foi possível mover %s -> %s, regravando\n", from, to);
        fat_remove(sys, from);
    }
    add_tree(n);
}

/* ------------------------------------------------------------ gravação */
static int write_dirty(vnode_t *n, int depth) {
    int k = 0;
    if (depth > 32) return 0;
    for (vnode_t *c = n->child; c; c = c->next) {
        if (c->ext) continue;
        if (c->dir) { k += write_dirty(c, depth + 1); continue; }
        if (!c->dirty) continue;
        c->dirty = false;
        char vp[1024], fp[1040];
        vfs_path(c, vp, sizeof vp);
        if (!tracked_path(vp)) continue;
        fpath_of(vp, fp, sizeof fp);
        if (mkdirs_parent(fp) && fat_write_file(sys, fp, c->data, c->size)) k++;
        else kprintf("persist: falha ao gravar %s (%llu bytes)\n", fp, (u64)c->size);
    }
    return k;
}

void persist_sync(void) {
    vol_sync_all();                      /* volumes graváveis de /Volumes (antes de desligar, etc.) */
    if (!enabled || !pending) return;
    pending = false;
    u64 t0 = timer_ticks();
    int k = write_dirty(vfs_root(), 0);
    fat_sync(sys);
    if (k) kprintf("persist: %d arquivo(s) gravado(s) em %llu ms\n", k, timer_ticks() - t0);
}

/* o log do kernel vai para o disco (para testes em hardware real sem porta serial):
 * /Sistema/Biblioteca/Logs/kernel.log, regravado a cada 5 s quando mudou */
static void save_klog(void) {
    extern char klog_buf[];
    extern usize klog_len;
    static usize saved_len = (usize)-1;
    static u64 last;
    if (!enabled || klog_len == saved_len || timer_ticks() - last < 5000) return;
    last = timer_ticks();
    saved_len = klog_len;
    vfs_mkdir("/Sistema/Biblioteca/Logs");
    vfs_write_file("/Sistema/Biblioteca/Logs/kernel.log", klog_buf, klog_len);
}

void persist_tick(void) {
    save_klog();
    vol_tick();
    if (pending && timer_ticks() - last_change >= SYNC_DELAY_MS) persist_sync();
}
