/*
 * mOS — ponte entre os volumes de imagens de disco (dfs: DMG/HFS+/APFS) e o VFS do sistema.
 *
 * Cada volume montado aparece em /Volumes/<nome> como vnodes comuns com ->ext apontando para
 * o dnode correspondente. Nada é copiado até ser usado: o VFS chama vfs_ensure() quando um
 * diretório é percorrido (cria os filhos) ou um arquivo é aberto (carrega o conteúdo).
 *
 * Memória: o conteúdo carregado entra numa fila LRU com orçamento fixo; ao estourar, os mais
 * antigos voltam a ser "não carregados" (data = NULL) e são relidos no próximo vfs_lookup.
 */
#include "vfs.h"
#include "fs.h"

#define MAX_FILE_LOAD (256ull << 20)
#define CACHE_BUDGET  (32ull << 20)   /* conteúdo de arquivos de volumes mantido em RAM */
#define KEEP_RECENT   4               /* nunca descarta os últimos carregados */
#define NAME_MAX_VFS  (sizeof(((vnode_t *)0)->name) - 1)

static dnode_t *lru_head, *lru_tail;  /* head = mais antigo */
static u64 lru_bytes;
static int lru_count;

static void lru_unlink(dnode_t *d) {
    if (!d->in_lru) return;
    if (d->lru_prev) d->lru_prev->lru_next = d->lru_next; else lru_head = d->lru_next;
    if (d->lru_next) d->lru_next->lru_prev = d->lru_prev; else lru_tail = d->lru_prev;
    d->lru_prev = d->lru_next = NULL;
    d->in_lru = false;
    lru_bytes -= d->size;
    lru_count--;
    d->vn = NULL;
}

static void lru_push(dnode_t *d, vnode_t *v) {
    lru_unlink(d);
    d->vn = v;
    d->in_lru = true;
    d->lru_prev = lru_tail;
    if (lru_tail) lru_tail->lru_next = d; else lru_head = d;
    lru_tail = d;
    lru_bytes += d->size;
    lru_count++;
}

/* devolve um arquivo ao estado "não carregado" se o conteúdo ainda for o que nós carregamos */
static void evict(dnode_t *d) {
    vnode_t *v = d->vn;
    lru_unlink(d);
    if (!v || v->ext != d) return;
    if (v->data && v->cap == d->size + 1 && v->size == d->size) {
        kfree(v->data);
        v->data = NULL;
        v->cap = 0;
    }
}

static void enforce_budget(void) {
    if (lru_bytes <= CACHE_BUDGET) return;
    while (lru_head && lru_bytes > CACHE_BUDGET && lru_count > KEEP_RECENT) evict(lru_head);
    dfs_trim(); /* sob pressão, solta também os caches internos dos drivers */
}

/* nomes do VFS têm tamanho fixo: corta sem quebrar um caractere UTF-8 */
static void fit_name(const char *src, char *dst) {
    usize n = 0, len = strlen(src);
    for (; n < len && n < NAME_MAX_VFS; n++) dst[n] = src[n] == '/' ? ':' : src[n];
    while (n && n < len && ((u8)src[n] & 0xC0) == 0x80) n--;
    dst[n] = 0;
}

static int mirroring;                 /* > 0: criando espelhos de um diretório (não é escrita do usuário) */

static void mirror_dir(vnode_t *dir, dnode_t *d) {
    mirroring++;
    char *base = kmalloc(1024), *path = kmalloc(1024 + NAME_MAX_VFS + 2);
    char name[NAME_MAX_VFS + 1];
    if (!base || !path) { kfree(base); kfree(path); mirroring--; return; }
    vfs_path(dir, base, 1024);
    for (dnode_t *c = dfs_first_child(d); c; c = c->next) {
        fit_name(c->name, name);
        if (!name[0]) continue;
        snprintf(path, 1024 + NAME_MAX_VFS + 2, "%s/%s", base, name);
        vnode_t *v = c->type == DN_DIR ? vfs_mkdir(path) : vfs_create(path);
        if (!v) continue;
        v->ext = c;
        v->mtime = 0;
        if (c->type != DN_DIR) { v->data = NULL; v->size = c->size; v->cap = 0; }
    }
    d->mirrored = true;
    kfree(base);
    kfree(path);
    mirroring--;
}

bool vfs_ensure(vnode_t *n) {
    if (!n || !n->ext) return true;
    dnode_t *d = n->ext;
    if (n->dir) {
        if (!d->mirrored) mirror_dir(n, d);
        return true;
    }
    if (n->data) {
        if (d->in_lru && d->vn == n) lru_push(d, n); /* uso recente */
        return true;
    }
    u64 len = 0;
    u8 *b = d->size <= MAX_FILE_LOAD ? dfs_read_all(d, MAX_FILE_LOAD, &len) : NULL;
    if (!b) {
        kprintf("dfs: não foi possível ler %s\n", d->name);
        n->size = 0; /* nunca deixa size > 0 com data == NULL para os apps */
        return false;
    }
    u64 raw_len = len;                                  /* d->size fica com o tamanho no volume (releitura) */
    if (d->type == DN_LINK) {
        /* symlink do volume (APFS/HFS+): no formato dos links do mOS ("#mOS-link:<alvo>"), para o lstat do
         * libSystem ver S_IFLNK e o walk do VFS seguir. Antes virava arquivo comum com o alvo de conteúdo, e o
         * inventário do Homebrew (casks .dmg) entrava pelo atalho "Applications" do volume. */
        static const char magic[] = "#mOS-link:";
        u8 *l = kmalloc(len + sizeof magic);
        if (l) {
            memcpy(l, magic, sizeof magic - 1);
            memcpy(l + sizeof magic - 1, b, len);
            l[len + sizeof magic - 1] = 0;
            kfree(b);
            b = l;
            len += sizeof magic - 1;
        }
    }
    n->data = b;
    n->size = len;
    n->cap = len + 1;
    d->size = raw_len;
    lru_push(d, n);
    enforce_budget();
    return true;
}

/* ------------------------------------------------------------ ejetar */
bool vfs_is_volume(vnode_t *n) {
    if (!n || !n->ext) return false;
    dnode_t *d = n->ext;
    return d->mnt && d->mnt->root == d;
}

bool vfs_eject(vnode_t *vol) {
    if (!vfs_is_volume(vol)) return false;
    dmount_t *m = ((dnode_t *)vol->ext)->mnt;
    /* volume gravável: tudo que estiver pendente vai para o disco antes; se falhar, não ejeta */
    if (m->writable && !vol_flush(vol)) {
        kprintf("dfs: \"%s\" não foi ejetado: falha ao gravar o pendente\n", m->volname);
        return false;
    }
    /* tira da LRU tudo que é deste volume; os buffers são liberados pelo vfs_delete */
    for (dnode_t *d = lru_head, *nx; d; d = nx) {
        nx = d->lru_next;
        if (d->mnt == m) lru_unlink(d);
    }
    vol->ext = NULL; /* vira uma pasta comum só para poder ser removida */
    if (!vfs_delete(vol)) { vol->ext = m->root; return false; }
    dfs_unmount(m);
    return true;
}

/* ------------------------------------------------------------ montagem */
static bool is_image(const char *name);

/* monta uma imagem aberta pelo usuário (ex.: duplo clique num .dmg); retorna nº de volumes */
int vfs_mount_file(vnode_t *f) {
    if (!f || f->dir || !is_image(f->name)) return 0;
    char path[1024];
    vfs_path(f, path, sizeof(path));
    if (dfs_find_source(path)) return 0; /* já montada */
    dmount_t *before = dfs_mounts();
    int n;
    if (f->ext) {
        u64 len = 0;                                        /* .dmg dentro de outro volume */
        u8 *data = dfs_read_all(f->ext, 512ull << 20, &len);
        n = data ? dfs_mount_buffer(data, len, path) : 0;
    } else if (f->cap == 0 && f->data) {
        n = dfs_mount_memory(f->data, f->size, path);       /* memória do pendrive: fixa */
    } else {
        /* arquivo criado/alterado nesta sessão: pode ser realocado, então usa uma cópia */
        u8 *copy = f->size ? kmalloc(f->size) : NULL;
        if (!copy) return 0;
        memcpy(copy, f->data, f->size);
        n = dfs_mount_buffer(copy, f->size, path);
    }
    if (n) dfs_publish_new(before);
    return n;
}

static bool is_image(const char *name) {
    usize n = strlen(name);
    return n > 4 && (!strcasecmp(name + n - 4, ".dmg") || !strcasecmp(name + n - 4, ".img"));
}

/* publica em /Volumes do VFS os volumes montados depois de efore (ex.: dfs_mount_disk);
 * chame depois de vfs_init */
void dfs_publish_new(dmount_t *before) {
    char name[NAME_MAX_VFS + 1], vpath[NAME_MAX_VFS + 16];
    for (dmount_t *m = dfs_mounts(); m && m != before; m = m->next) {
        fit_name(m->root->name, name);
        snprintf(vpath, sizeof(vpath), "/Volumes/%s", name);
        vnode_t *v = vfs_mkdir(vpath);
        if (v) v->ext = m->root;
    }
}

static void scan(vnode_t *dir, int depth) {
    if (depth > 8) return;
    for (vnode_t *c = dir->child; c; c = c->next) {
        if (c->ext) continue; /* não entra nos volumes já montados */
        if (c->dir) { scan(c, depth + 1); continue; }
        if (!is_image(c->name) || !c->data || !c->size) continue;
        char path[1024];
        vfs_path(c, path, sizeof(path));
        dmount_t *before = dfs_mounts();
        if (dfs_mount_memory(c->data, c->size, path)) dfs_publish_new(before);
    }
}

void dfs_mount_all(void) {
    dfs_init();
    vfs_mkdir("/Volumes");
    scan(vfs_root(), 0);
    int n = 0;
    for (dmount_t *m = dfs_mounts(); m; m = m->next) n++;
    kprintf("dfs: %d volume(s) de imagens de disco em /Volumes\n", n);
}

/* ------------------------------------------------------------ escrita em volumes graváveis
 * Volumes FAT/HFS+ de discos graváveis (dmount->writable) aceitam mudanças pelo VFS. O VFS continua
 * sendo a cópia de trabalho em RAM; os ganchos de persistência (persist.c) chamam vol_* e daqui as
 * mudanças vão para o driver por caminho relativo ao volume:
 *   - pastas criadas, apagadas e renomeadas/movidas vão para o disco na hora;
 *   - arquivos alterados ficam "sujos" e são gravados inteiros ~1,5 s depois da última alteração,
 *     ao ejetar (vfs_eject) ou em persist_sync() (desligar/reiniciar).
 * Um arquivo alterado deixa de apontar para o dnode (ext = NULL): a versão em RAM é a verdadeira e a
 * LRU nunca a descarta. Mover para fora de um volume carrega tudo para a RAM antes ("materializa"). */
#define VOL_SYNC_DELAY_MS 1500

static bool vol_pending;
static u64 vol_last;

bool vfs_ext_writable(vnode_t *n) {
    dnode_t *d = n ? n->ext : NULL;
    return d && d->mnt && d->mnt->writable && d->mnt->ops->write_file;
}

static dmount_t *mount_of(vnode_t *vol) { return ((dnode_t *)vol->ext)->mnt; }

static vnode_t *volume_of(vnode_t *n) {
    for (vnode_t *p = n; p; p = p->parent) if (vfs_is_volume(p)) return p;
    return NULL;
}

/* caminho de n dentro do volume vol ("/" para a raiz) */
static bool rel_path(vnode_t *vol, vnode_t *n, char *out, usize cap) {
    char *tmp = kmalloc(cap), *t2 = kmalloc(cap);
    if (!tmp || !t2) { kfree(tmp); kfree(t2); return false; }
    tmp[0] = 0;
    vnode_t *p = n;
    for (; p && p != vol; p = p->parent) {
        snprintf(t2, cap, "/%s%s", p->name, tmp);
        strncpy(tmp, t2, cap - 1);
        tmp[cap - 1] = 0;
    }
    bool ok = p == vol && strlen(tmp) < cap - 1;
    snprintf(out, cap, "%s", tmp[0] ? tmp : "/");
    kfree(tmp);
    kfree(t2);
    return ok;
}

/* volume montado cujo caminho no VFS é prefixo de path; *rel = resto ("/a/b") */
static vnode_t *volume_by_path(const char *path, const char **rel) {
    if (strncmp(path, "/Volumes/", 9)) return NULL;
    vnode_t *vols = vfs_lookup("/Volumes");
    for (vnode_t *c = vols ? vols->child : NULL; c; c = c->next) {
        if (!vfs_is_volume(c)) continue;
        usize n = strlen(c->name);
        if (!strncmp(path + 9, c->name, n) && (path[9 + n] == '/' || !path[9 + n])) {
            *rel = path[9 + n] ? path + 9 + n : "/";
            return c;
        }
    }
    return NULL;
}

/* solta o dnode de um arquivo: o conteúdo atual fica só na RAM (carrega antes se preciso) */
static void detach_file(vnode_t *n) {
    dnode_t *d = n->ext;
    if (!d) return;
    if (!n->data && n->size) vfs_ensure(n);
    if (d->in_lru && d->vn == n) lru_unlink(d);
    n->ext = NULL;
}

/* carrega toda a subárvore para a RAM e solta os dnodes (antes de sair do volume) */
static void materialize(vnode_t *n, int depth) {
    if (depth > 64) return;
    if (n->dir) {
        if (n->ext) vfs_ensure(n);                   /* espelha os filhos ainda não listados */
        for (vnode_t *c = n->child; c; c = c->next) materialize(c, depth + 1);
        n->ext = NULL;
        return;
    }
    detach_file(n);
}

/* esquece as referências da LRU a uma subárvore que vai ser liberada */
static void forget(vnode_t *n, int depth) {
    if (depth > 64) return;
    dnode_t *d = n->ext;
    if (d && d->in_lru && d->vn == n) lru_unlink(d);
    for (vnode_t *c = n->child; c; c = c->next) forget(c, depth + 1);
}

void vol_mark(vnode_t *n) {
    if (mirroring || !n) return;
    vnode_t *vol = volume_of(n);
    if (!vol || vol == n) return;
    dmount_t *m = mount_of(vol);
    if (!m->writable) return;
    if (n->dir) {
        char p[1024];
        if (!n->ext && rel_path(vol, n, p, sizeof p) && !m->ops->mkdir(m, p))
            kprintf("vol: \"%s\": não foi possível criar a pasta %s\n", m->volname, p);
        return;
    }
    detach_file(n);
    n->dirty = true;
    vol_pending = true;
    vol_last = timer_ticks();
}

void vol_delete(vnode_t *n) {
    if (!n) return;
    vnode_t *vol = volume_of(n);
    forget(n, 0);
    if (!vol || vol == n) return;
    dmount_t *m = mount_of(vol);
    char p[1024];
    if (!m->writable || !rel_path(vol, n, p, sizeof p)) return;
    /* um arquivo novo ainda não gravado não existe no disco: falhar aqui é normal */
    if (!m->ops->remove(m, p) && !n->dirty) kprintf("vol: \"%s\": não foi possível apagar %s\n", m->volname, p);
}

/* marca uma subárvore inteira como nova no volume em que ela está agora */
static void mark_tree(vnode_t *n, int depth) {
    if (depth > 64) return;
    if (n->dir && n->ext) vfs_ensure(n);
    if (n->dir) n->ext = NULL;                       /* pasta recriada: não é mais o dnode antigo */
    vol_mark(n);
    if (n->dir) for (vnode_t *c = n->child; c; c = c->next) mark_tree(c, depth + 1);
}

void vol_move(const char *old_path, vnode_t *n) {
    if (!n || !old_path) return;
    const char *orel = NULL;
    vnode_t *ovol = volume_by_path(old_path, &orel), *nvol = volume_of(n);
    if (ovol == n) ovol = NULL;                      /* o próprio volume não se move (vfs.c recusa) */
    char np[1024];
    if (ovol && ovol == nvol) {                      /* dentro do mesmo volume */
        dmount_t *m = mount_of(nvol);
        if (!m->writable || !rel_path(nvol, n, np, sizeof np)) return;
        if (m->ops->rename(m, orel, np)) return;
        /* não estava no disco (arquivo novo pendente) ou o driver recusou: regrava em np */
        m->ops->remove(m, orel);
        materialize(n, 0);
        mark_tree(n, 0);
        return;
    }
    if (ovol) {                                      /* saiu de um volume: vira RAM e sai do disco */
        materialize(n, 0);
        dmount_t *m = mount_of(ovol);
        if (m->writable && !m->ops->remove(m, orel) && !n->dirty)
            kprintf("vol: \"%s\": não foi possível apagar %s\n", m->volname, orel);
    }
    if (nvol && nvol != n && mount_of(nvol)->writable) { materialize(n, 0); mark_tree(n, 0); }
}

/* grava os arquivos sujos de uma subárvore de vol; devolve false se algum falhou */
static bool flush_tree(vnode_t *vol, dmount_t *m, vnode_t *dir, int depth, int *count) {
    bool ok = true;
    if (depth > 64) return true;
    for (vnode_t *c = dir->child; c; c = c->next) {
        if (c->dir) { ok &= flush_tree(vol, m, c, depth + 1, count); continue; }
        if (!c->dirty || c->ext) continue;
        char p[1024];
        if (!rel_path(vol, c, p, sizeof p)) continue;
        if (m->ops->write_file(m, p, c->data ? c->data : (const u8 *)"", c->size)) { c->dirty = false; (*count)++; }
        else { kprintf("vol: \"%s\": falha ao gravar %s (%llu bytes)\n", m->volname, p, (u64)c->size); ok = false; }
    }
    return ok;
}

bool vol_flush(vnode_t *vol) {
    if (!vfs_is_volume(vol)) return false;
    dmount_t *m = mount_of(vol);
    if (!m->writable) return true;
    int k = 0;
    u64 t0 = timer_ticks();
    bool ok = flush_tree(vol, m, vol, 0, &k);
    ok &= !m->ops->sync || m->ops->sync(m);
    if (k) kprintf("vol: \"%s\": %d arquivo(s) gravado(s) em %llu ms\n", m->volname, k, timer_ticks() - t0);
    return ok;
}

bool vol_sync_all(void) {
    if (!vol_pending) return true;
    vol_pending = false;
    bool ok = true;
    vnode_t *vols = vfs_lookup("/Volumes");
    for (vnode_t *c = vols ? vols->child : NULL; c; c = c->next)
        if (vfs_is_volume(c) && mount_of(c)->writable) ok &= vol_flush(c);
    if (!ok) { vol_pending = true; vol_last = timer_ticks(); }   /* tenta de novo mais tarde */
    return ok;
}

void vol_tick(void) {
    if (vol_pending && timer_ticks() - vol_last >= VOL_SYNC_DELAY_MS) vol_sync_all();
}
