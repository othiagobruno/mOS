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
#include "origin.h"
#include "sysstate.h"
#include "crypto.h"

#define SYS_DIR       "/mOS"
#define STATE_FILE    "/Sistema/Biblioteca/estado-sistema"
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

/* ------------------------------------------------------------ localizar o volume
 * O volume do sistema é SOMENTE aquele que a identificação confiável da origem (fs/origin.c: caminho de dispositivo
 * UEFI + identidade física do disco + identidade da partição) aponta. Não há mais pontuação nem "o primeiro disco com
 * \mOS\": origem ambígua => nenhum volume => o sistema roda só na RAM, com os arquivos entregues pelo firmware. */
static int sys_state = SYS_EPHEMERAL;
static bool authorized;                 /* a escrita no volume do sistema foi liberada (política + enable) */

int sysstate(void) { return sys_state; }
const char *sysstate_name(void) {
    switch (sys_state) {
    case SYS_LIVE_PERSIST: return "LIVE_PERSISTENTE";
    case SYS_FIRSTBOOT: return "PRIMEIRO_BOOT";
    case SYS_INSTALLED: return "INSTALADO";
    case SYS_INCOMPLETE: return "INSTALANDO";
    }
    return "LIVE";
}
bool sysstate_guest_allowed(void) { return sys_state == SYS_EPHEMERAL; }
bool persist_writable(void) { return authorized; }

static int parse_state(const char *txt) {
    if (!strncmp(txt, "LIVE_PERSISTENTE", 16)) return SYS_LIVE_PERSIST;
    if (!strncmp(txt, "PRIMEIRO_BOOT", 13)) return SYS_FIRSTBOOT;
    if (!strncmp(txt, "INSTALADO", 9)) return SYS_INSTALLED;
    if (!strncmp(txt, "INSTALANDO", 10)) return SYS_INCOMPLETE;
    return SYS_EPHEMERAL;
}

static void read_state(void) {
    u64 n = 0;
    u8 *d = fat_read_file(sys, SYS_DIR STATE_FILE, &n);
    sys_state = SYS_EPHEMERAL;
    if (d && n) {
        char buf[32];
        usize k = MIN((usize)n, sizeof buf - 1);
        memcpy(buf, d, k);
        buf[k] = 0;
        sys_state = parse_state(buf);
    }
    kfree(d);
}

static bool open_origin_volume(void) {
    const origin_t *o = origin_get();
    if (o->status != ORIGIN_TRUSTED) return false;
    blkdev_t *disk = storage_disk(o->disk);
    blkdev_t *p = disk ? blk_slice(disk, o->part_off, o->part_size) : NULL;
    if (!p) return false;
    fatfs_t *fs = fat_mount(p);
    bool dir = false;
    if (!fs || !fat_stat(fs, SYS_DIR, &dir, NULL) || !dir) {
        kprintf("persist: a partição de origem não é FAT com \\mOS\\: sistema só na RAM\n");
        if (fs) fat_unmount(fs);
        blk_release(p);
        return false;
    }
    sys = fs;
    sys_disk = o->disk;
    return true;
}

/* ------------------------------------------------------------ carga inicial */
#define PERSIST_MAX_DEPTH 64
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
    /* node_modules aninhados passam de 16 níveis: o @opentelemetry do Cursor fica no 17 (/Aplicativos/Cursor.app/.../
     * platform/node/machine-id), e com 16 os arquivos dele não entravam no VFS (o Extension Host caía no require).
     * Pilha do boot: 1 MiB (efi_main.c), uns 3 KiB por nível. */
    if (depth > PERSIST_MAX_DEPTH) { kprintf("persist: %s fundo demais, ignorado\n", vpath); return; }
    walk_t *w = kmalloc(sizeof *w);
    if (!w) return;
    strncpy(w->vpath, vpath, sizeof w->vpath - 1);
    w->vpath[sizeof w->vpath - 1] = 0;
    snprintf(w->fpath, sizeof w->fpath, SYS_DIR "%s", vpath);
    w->depth = depth;
    fat_list(sys, w->fpath, load_entry, w);
    kfree(w);
}

/* chamado por vfs_init(): devolve true se carregou a árvore do disco (volume de origem confiável) */
bool persist_load(void) {
    if (!open_origin_volume()) return false;
    read_state();
    fat_set_system(sys);
    kprintf("persist: volume do sistema \"%s\" (FAT, %llu MB) em %s, estado %s\n",
            fat_label(sys)[0] ? fat_label(sys) : "sem nome", origin_get()->part_size >> 20, storage_disk_name(sys_disk), sysstate_name());
    quiet++;
    u64 t0 = timer_ticks();
    load_tree("", 0);
    quiet--;
    kprintf("persist: %d arquivos e %d pastas carregados do disco em %llu ms\n", nfiles, ndirs, timer_ticks() - t0);
    return true;
}

/* Liga a escrita no volume do sistema: libera à política de armazenamento SOMENTE a faixa da partição de origem. */
static bool authorize(void) {
    const origin_t *o = origin_get();
    if (!sys || o->status != ORIGIN_TRUSTED || authorized) return authorized;
    if (!storage_grant_write(storage_disk(o->disk), o->part_off, o->part_size)) return false;
    fat_enable_rw(sys);
    authorized = true;
    return true;
}

/* Chamado no fim do vfs_init. Padrão FECHADO: só liga a persistência quando o sistema está instalado (ou é um live
 * em que o usuário já consentiu antes) E a origem é confiável. Live comum, RAM e origem ambígua: nada é gravado,
 * nem logs, nem contas, nem metadados. */
void persist_enable(void) {
    enabled = false;
    if (!sys) return;
    if (sys_state == SYS_INSTALLED || sys_state == SYS_FIRSTBOOT || sys_state == SYS_LIVE_PERSIST) enabled = authorize();
    kprintf("persist: gravação no disco do sistema %s (estado %s)\n", enabled ? "LIGADA" : "DESLIGADA", sysstate_name());
}

/* Consentimento explícito do usuário (assistente) para o live gravar no próprio pendrive USB. */
bool persist_consent_live(void) {
    const origin_t *o = origin_get();
    if (!sys || o->status != ORIGIN_TRUSTED || o->bus != PHYS_USB || sys_state != SYS_EPHEMERAL) return false;
    if (!crypto_rng_hw()) { kprintf("persist: persistência recusada: sem gerador de números aleatórios de hardware (senhas não podem ser criadas com segurança)\n"); return false; }
    if (!authorize()) return false;
    enabled = true;
    sys_state = SYS_LIVE_PERSIST;
    vfs_write_file(STATE_FILE, "LIVE_PERSISTENTE\n", 17);
    persist_sync();
    kprintf("persist: live PERSISTENTE ativado por consentimento: gravando em %s\n", storage_disk_name(sys_disk));
    return true;
}

/* grava o estado do sistema (instalado, primeiro boot concluído...) no volume do sistema */
bool persist_set_state(int st) {
    if (!enabled) return false;
    sys_state = st;
    char line[32];
    int n = snprintf(line, sizeof line, "%s\n", sysstate_name());
    vfs_write_file(STATE_FILE, line, (usize)n);
    persist_sync();
    return true;
}

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
/* cria as pastas que faltam até a mãe de fpath. Iterativo: a versão recursiva gastava 1 KiB por pasta nova, e uma
 * árvore funda criada de uma vez (node_modules) estourava os 32 KiB de pilha das threads do kernel */
static bool mkdirs_parent(const char *fpath) {
    char tmp[1040];
    strncpy(tmp, fpath, sizeof tmp - 1);
    tmp[sizeof tmp - 1] = 0;
    char *slash = strrchr(tmp, '/');
    if (!slash || slash == tmp) return true;
    *slash = 0;
    bool dir;
    if (fat_stat(sys, tmp, &dir, NULL)) return dir;
    for (char *p = tmp + 1;; p++) {
        if (*p != '/' && *p) continue;
        char c = *p;
        *p = 0;
        if (fat_stat(sys, tmp, &dir, NULL)) { if (!dir) return false; }
        else if (!fat_mkdir(sys, tmp)) return false;
        *p = c;
        if (!c) break;
    }
    return true;
}

__attribute__((noinline)) void persist_mark(vnode_t *n) {   /* noinline: o add_tree recursivo não leva o vp[] junto */
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
/* um arquivo sujo vai ao disco; fora da recursão de propósito: os 2 KiB de caminhos ficariam em cada nível do
 * write_dirty, e ele roda em threads do kernel com 32 KiB de pilha (sched.c). Com 64 níveis de pastas (node_modules
 * do Codex e do Cursor) isso estourava a pilha e a VM reiniciava (falha tripla). */
static __attribute__((noinline)) int write_one(vnode_t *c) {
    char vp[1024], fp[1040];
    vfs_path(c, vp, sizeof vp);
    if (!tracked_path(vp)) return 0;
    fpath_of(vp, fp, sizeof fp);
    if (mkdirs_parent(fp) && fat_write_file(sys, fp, c->data, c->size)) return 1;
    kprintf("persist: falha ao gravar %s (%llu bytes)\n", fp, (u64)c->size);
    return 0;
}
static int write_dirty(vnode_t *n, int depth) {
    int k = 0;
    if (depth > PERSIST_MAX_DEPTH) return 0;
    for (vnode_t *c = n->child; c; c = c->next) {
        if (c->ext) continue;
        if (c->dir) { k += write_dirty(c, depth + 1); continue; }
        if (!c->dirty) continue;
        c->dirty = false;
        k += write_one(c);
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
