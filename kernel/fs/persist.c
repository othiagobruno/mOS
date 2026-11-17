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
#include "update.h"

#define SYS_DIR       "/mOS"
#define STATE_FILE    "/Sistema/Biblioteca/estado-sistema"
#define SYNC_DELAY_MS 1500
#ifndef MAX_LOAD                       /* sobrescrevível em builds de teste (ex.: -DMAX_LOAD=...) */
#define MAX_LOAD      (512ull << 20)  /* maior arquivo que vai inteiro para a RAM (exec/dyld); o Codex CLI tem 277 MB */
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

static bool authorize(void);
#define META_FILE "/.vfsmeta"                 /* fs/persist.c: modo, dono, flags, datas e xattrs (fora do VFS) */
#define META_NEW  "/.vfsmeta.new"
static void meta_preload(void);
static void meta_apply(void);
#define UPDATE_DIR "/Sistema/Atualizacao"      /* área do atualizador (fs/update.c): fica fora do VFS */
static bool is_update_dir(const char *vp) {
    usize n = strlen(UPDATE_DIR);
    return !strncmp(vp, UPDATE_DIR, n) && (vp[n] == 0 || vp[n] == '/');
}

/* VFS sob demanda: a carga do boot só cria os vnodes (nome, tamanho e o 1º cluster no FAT); nenhum conteúdo vem para a
 * RAM. O conteúdo é lido quando alguém usa o arquivo: o vfs_lookup traz inteiro o que tem até VFS_PAGED_MIN (vfs.h); o
 * maior fica paginado e read/pread/mmap leem só o trecho pedido (persist_read_at, pelo page cache). Carregado e sem
 * mudança, o arquivo entra numa LRU com orçamento (SYS_CACHE_BUDGET): ao estourar, os mais antigos voltam a ficar só no
 * disco. Antes a imagem inteira (6,7 GB em /Aplicativos) ia para a RAM e, numa máquina de 2 GB, 4,7 GB ficavam de fora. */
#define APPS_DIR         "/Aplicativos"
#define SYS_CACHE_BUDGET (48ull << 20)
#define SYS_KEEP_RECENT  8
#define LOAD_RESERVE     (64ull << 20)   /* carregar um arquivo nunca deixa menos que isto livre */
/* Fora de /Aplicativos, o que tem até EAGER_MAX vem no boot e fica fixo (fora da LRU), como antes: os apps nativos do kernel
 * (Notas, Calendário, scripts, contas...) percorrem a árvore e leem c->data direto, sem vfs_lookup. Os apps de terceiros
 * (/Aplicativos) e os arquivos grandes do sistema ficam todos sob demanda. */
#define EAGER_MAX        (1ull << 20)

static vnode_t *blru_head, *blru_tail;  /* head = mais antigo */
static u64 blru_bytes;
static int blru_count;
static u64 sys_loads, sys_evicts;

static void blru_unlink(vnode_t *v) {
    if (!v->in_blru) return;
    if (v->blru_prev) v->blru_prev->blru_next = v->blru_next; else blru_head = v->blru_next;
    if (v->blru_next) v->blru_next->blru_prev = v->blru_prev; else blru_tail = v->blru_prev;
    v->blru_prev = v->blru_next = NULL;
    v->in_blru = false;
    blru_bytes -= v->blru_bytes;
    blru_count--;
}
static void blru_push(vnode_t *v) {
    blru_unlink(v);
    v->in_blru = true;
    v->blru_prev = blru_tail;
    if (blru_tail) blru_tail->blru_next = v; else blru_head = v;
    blru_tail = v;
    v->blru_bytes = v->cap;
    blru_bytes += v->cap;
    blru_count++;
}
/* devolve ao disco: só o que ainda é igual ao do disco e não está emprestado (páginas que o macho.c/lazy.c mapeiam) */
static bool evictable(vnode_t *v) {
    return v->dclus && v->data && !v->dirty && v->cap == v->size + 1 && !kmalloc_is_pages(v->data);
}
static void blru_enforce(void) {
    for (vnode_t *v = blru_head, *nx; v && blru_bytes > SYS_CACHE_BUDGET && blru_count > SYS_KEEP_RECENT; v = nx) {
        nx = v->blru_next;
        if (!evictable(v)) { blru_unlink(v); continue; }
        blru_unlink(v);
        kfree(v->data);
        v->data = NULL;
        v->cap = 0;
        sys_evicts++;
    }
}

void persist_forget(vnode_t *n) { if (n) blru_unlink(n); }

i64 persist_read_at(vnode_t *n, u64 off, void *buf, u64 len) {
    if (!sys || !n || !n->dclus || n->dir) return -1;
    if (off >= n->size) return 0;
    len = MIN(len, (u64)n->size - off);
    return fat_read_chain(sys, n->dclus, off, buf, len) ? (i64)len : -1;
}

/* o arquivo inteiro na RAM. A partir de 256 KiB em páginas próprias, como o fat_read_file: o macho.c mapeia o __TEXT dos
 * frameworks e o lazy.c as páginas de mmap direto delas (essas não voltam para a LRU) */
static bool load_full(vnode_t *n, bool pages, bool lru) {
    if (n->data) return true;
    if (!sys || !n->dclus) return false;
    if (n->size > MAX_LOAD) { kprintf("persist: %s grande demais para ir inteiro à RAM (%llu MB); lido aos pedaços\n", n->name, (unsigned long long)(n->size >> 20)); return false; }
    if (pmm_free_pages() * 4096ull < n->size + LOAD_RESERVE) {
        blru_enforce();
        if (pmm_free_pages() * 4096ull < n->size + LOAD_RESERVE) {
            kprintf("persist: sem memória para carregar %s (%llu KiB)\n", n->name, (unsigned long long)(n->size >> 10));
            return false;
        }
    }
    u8 *b = pages && n->size >= (256u << 10) ? kmalloc_pages(n->size + 1) : kmalloc(n->size + 1);
    if (!b) return false;
    if (!fat_read_chain(sys, n->dclus, 0, b, n->size)) {
        kprintf("persist: erro ao ler %s do disco\n", n->name);
        kfree(b);
        return false;
    }
    b[n->size] = 0;
    n->data = b;
    n->cap = n->size + 1;
    sys_loads++;
    if (lru && !kmalloc_is_pages(b)) { blru_push(n); blru_enforce(); }
    return true;
}

/* quem pediu o arquivo inteiro (exec, dyld, apps do kernel): em páginas, para poder ser mapeado */
bool persist_ensure(vnode_t *n) {
    if (!n || n->dir) return false;
    if (n->data) { if (n->in_blru) blru_push(n); return true; }
    return load_full(n, true, true);
}

/* vfs_lookup: pequeno vem inteiro (e fica na LRU); grande fica paginado */
bool persist_ensure_lookup(vnode_t *n) {
    if (!n || n->dir || !n->dclus) return true;
    if (n->data) { if (n->in_blru) blru_push(n); return true; }
    if (n->size > VFS_PAGED_MIN) return true;
    return load_full(n, false, true);
}

/* o conteúdo deixa de ser o do disco (vai mudar ou o arquivo sai do disco): carrega antes e solta o cluster */
void persist_detach(vnode_t *n) {
    if (!n || n->dir || !n->dclus) return;
    if (!n->data && n->size && !load_full(n, true, false)) return;   /* sem memória: continua lendo do disco */
    blru_unlink(n);
    n->dclus = 0;
}

/* a subárvore inteira para a RAM (antes de o disco perdê-la); false se algum arquivo não coube */
static bool detach_tree(vnode_t *n, int depth) {
    if (depth > PERSIST_MAX_DEPTH) return false;
    if (!n->dir) { persist_detach(n); return !n->dclus; }
    bool ok = true;
    for (vnode_t *c = n->child; c; c = c->next) ok &= detach_tree(c, depth + 1);
    return ok;
}

static void load_entry(void *ctx, const char *name, bool is_dir, u64 size, u32 clus) {
    walk_t *w = ctx;
    char vp[1024];
    snprintf(vp, sizeof vp, "%s/%s", w->vpath, name);
    if (is_update_dir(vp)) return;
    if (!strcmp(vp, APPS_DIR)) return;                              /* persist_load carrega depois do resto */
    if (!strcmp(vp, META_FILE) || !strcmp(vp, META_NEW)) return;   /* metadados do VFS: não são arquivos do VFS */
    if (is_dir) {
        vfs_mkdir(vp);
        ndirs++;
        load_tree(vp, w->depth + 1);
        return;
    }
    vnode_t *v = vfs_create(vp);
    if (!v) return;
    if (v->cap) kfree(v->data);
    v->data = NULL;
    v->cap = 0;
    v->size = size;
    v->dclus = size ? clus : 0;
    if (!size) { v->data = kmalloc(1); if (v->data) { v->data[0] = 0; v->cap = 1; } }   /* vazio: buffer de 1 byte, como antes */
    else if (size <= EAGER_MAX && strncmp(vp, APPS_DIR "/", sizeof APPS_DIR) != 0) load_full(v, true, false);
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
    fat_list2(sys, w->fpath, load_entry, w);
    kfree(w);
}

/* chamado por vfs_init(): devolve true se carregou a árvore do disco (volume de origem confiável) */
bool persist_load(void) {
    if (!open_origin_volume()) return false;
    read_state();
    fat_set_system(sys);
    kprintf("persist: volume do sistema \"%s\" (FAT, %llu MB) em %s, estado %s\n",
            fat_label(sys)[0] ? fat_label(sys) : "sem nome", origin_get()->part_size >> 20, storage_disk_name(sys_disk), sysstate_name());
    /* atualização pronta (ou interrompida no meio): aplicar ANTES de carregar a árvore, para o VFS já nascer com a versão
     * nova. Se o carregador EFI foi trocado agora, o kernel em execução é o antigo: reinicia para entrar no novo. */
    if ((sys_state == SYS_INSTALLED || sys_state == SYS_LIVE_PERSIST) && update_boot_pending(sys) && authorize()) {
        if (update_boot_apply(sys)) {
            kprintf("persist: kernel atualizado: reiniciando\n");
            acpi_reboot();
        }
    }
    quiet++;
    u64 t0 = timer_ticks();
    meta_preload();                      /* antes da árvore: os inodes novos começam depois dos gravados */
    load_tree("", 0);
    bool apps_dir = false;
    if (fat_stat(sys, SYS_DIR APPS_DIR, &apps_dir, NULL) && apps_dir) {
        vfs_mkdir(APPS_DIR);
        ndirs++;
        load_tree(APPS_DIR, 1);
    }
    meta_apply();
    quiet--;
    kprintf("persist: %d arquivos e %d pastas do disco em %llu ms (conteúdo sob demanda)\n", nfiles, ndirs, timer_ticks() - t0);
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

/* Arquivos e pastas criados na RAM antes da gravação existir (mtime != 0, ou dirty). O que veio do
 * disco ou do payload entra com mtime 0 e não é reescrito. Não chame de persist_enable: isso gravaria
 * a árvore inteira do sistema por cima da imagem. */
void persist_capture_existing(void) {
    vnode_t **st;
    int cap = 64, n = 0;
    if (!enabled) return;
    st = kmalloc(sizeof(vnode_t *) * (usize)cap);
    if (!st) return;
    st[n++] = vfs_root();
    while (n) {
        vnode_t *v = st[--n];
        if (v != vfs_root() && (v->dir ? v->mtime != 0 : (v->mtime != 0 || v->dirty))) persist_mark(v);
        if (!v->dir) continue;
        for (vnode_t *c = v->child; c; c = c->next) {
            if (n >= cap) {
                int nc = cap * 2;
                vnode_t **ns = kmalloc(sizeof(vnode_t *) * (usize)nc);
                if (!ns) { kfree(st); return; }
                memcpy(ns, st, sizeof(vnode_t *) * (usize)n);
                kfree(st);
                st = ns;
                cap = nc;
            }
            st[n++] = c;
        }
    }
    kfree(st);
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
    persist_capture_existing();   /* arquivos criados na RAM antes do consentimento (preferências, apps) */
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
    if (is_update_dir(p)) return false;
    if (!strcmp(p, META_FILE) || !strcmp(p, META_NEW)) return false;
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
    if (n && !n->dir && n->dclus && !quiet) persist_detach(n);   /* mudou: o conteúdo passa a ser o da RAM */
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
        /* o conteúdo ainda no disco vem para a RAM antes; se não couber, o arquivo fica no disco (o vnode lê pelo cluster) */
        if (!detach_tree(n, 0)) { kprintf("persist: %s saiu da área gravada mas não coube na RAM: mantido no disco\n", from); return; }
        if (fat_stat(sys, from, &dir, NULL)) fat_remove(sys, from);
        return;
    }
    fpath_of(vp, to, sizeof to);
    bool dir;
    if (was && fat_stat(sys, from, &dir, NULL)) {
        if (mkdirs_parent(to) && fat_rename(sys, from, to)) return;
        kprintf("persist: não foi possível mover %s -> %s, regravando\n", from, to);
        if (!detach_tree(n, 0)) { kprintf("persist: sem memória para regravar %s; fica no lugar antigo\n", from); return; }
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
    if (!c->data && c->size) { kprintf("persist: %s sem conteúdo na RAM, não gravado\n", fp); return 0; }
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

static bool meta_pending;
static bool meta_save(void);
void persist_meta_mark(void) {
    if (!enabled || quiet) return;
    meta_pending = true;
    pending = true;
    last_change = timer_ticks();
}

void persist_sync(void) {
    vol_sync_all();                      /* volumes graváveis de /Volumes (antes de desligar, etc.) */
    if (!enabled || !pending) return;
    pending = false;
    u64 t0 = timer_ticks();
    int k = write_dirty(vfs_root(), 0);
    if (meta_pending) { meta_pending = false; if (!meta_save()) { meta_pending = true; pending = true; } }
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
    pcache_tick(); /* writeback do page cache e um passo de aio */
    update_poll();                       /* atualização em andamento (fs/update.c), mesmo com Ajustes fechado */
    save_klog();
    vol_tick();
    if (pending && timer_ticks() - last_change >= SYNC_DELAY_MS) persist_sync();
}

/* ------------------------------------------------------------ metadados (\mOS\.vfsmeta)
 * O FAT não guarda dono, modo, flags BSD, datas em ns nem xattrs. Tudo isso vai para um arquivo só, regravado por
 * inteiro no persist_sync quando algo muda (vfs_meta_changed/vfs_touch -> persist_meta_mark) e trocado de forma
 * atômica (fat_replace: uma queda deixa o antigo ou o novo). Só entram os nós com metadados próprios (v->meta:
 * criados ou mudados depois do boot); o resto continua com o padrão (501:20, 0644/0755, hora do boot).
 *
 *   "MOSVMETA" u32 versão(1) u32 registros u64 próximo-inode
 *   registro: u16 tam, caminho (sem o \mOS), u8 pasta, u32 modo, uid, gid, flags, u64 inode,
 *             i64 acesso, modificação, mudança, criação (ns), u16 nº de xattrs, { u8 tam, nome, u32 tam, valor }
 *   u32 CRC-32 de tudo antes dele
 */


static u8 *meta_raw;           /* lido no meta_preload, aplicado no meta_apply */
static u64 meta_rawlen;

static void meta_preload(void) {
    u64 n = 0;
    u8 *d = fat_read_file(sys, SYS_DIR META_FILE, &n);
    if (!d) d = fat_read_file(sys, SYS_DIR META_NEW, &n);        /* queda antes da troca: o novo já está inteiro */
    if (!d) return;
    u64 next = 0;
    if (!vmeta_check(d, n, &next)) { kprintf("persist: .vfsmeta corrompido (ignorado)\n"); kfree(d); return; }
    vfs_ino_reserve(next ? next - 1 : 0);
    meta_raw = d;
    meta_rawlen = n;
}

static vnode_t *meta_lookup(void *ctx, const char *path) { (void)ctx; return vfs_lookup(path); }

static void meta_apply(void) {
    if (!meta_raw) return;
    int n = vmeta_apply(meta_raw, meta_rawlen, meta_lookup, NULL);
    kprintf("persist: metadados de %d nó(s) restaurados\n", n);
    kfree(meta_raw);
    meta_raw = NULL;
}

static bool meta_track(void *ctx, vnode_t *v, char *vp, usize cap) {
    (void)ctx;
    for (vnode_t *p = v; p; p = p->parent) if (p->ext) return false;
    vfs_path(v, vp, cap);
    return tracked_path(vp);
}

static bool meta_save(void) {
    u64 len = 0;
    u8 *buf = vmeta_build(vfs_root(), meta_track, NULL, &len);
    if (!buf) { kprintf("persist: sem memória para o .vfsmeta\n"); return false; }
    bool ok = fat_write_file(sys, SYS_DIR META_NEW, buf, len) && fat_replace(sys, SYS_DIR META_NEW, SYS_DIR META_FILE);
    if (!ok) kprintf("persist: falha ao gravar o .vfsmeta (%llu bytes)\n", (unsigned long long)len);
    kfree(buf);
    return ok;
}

/* statfs/getattrlist do volume do sistema (kernel/lib/vfsops.c) */
bool persist_space(u64 *total, u64 *avail, char *label, usize cap) {
    if (!sys) return false;
    if (!fat_space(sys, total, avail)) return false;
    if (label && cap) { strncpy(label, fat_label(sys), cap - 1); label[cap - 1] = 0; }
    return true;
}
