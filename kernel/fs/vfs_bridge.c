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
#include "syscall.h"
#include "fs.h"
#include "storage.h"
#include "origin.h"

#define MAX_FILE_LOAD (256ull << 20)
#define CACHE_BUDGET  (32ull << 20)   /* conteúdo de arquivos de volumes mantido em RAM */
#define KEEP_RECENT   4               /* nunca descarta os últimos carregados */
#define NAME_MAX_VFS  (sizeof(((vnode_t *)0)->name) - 1)

static dmount_t *mount_of(vnode_t *vol) { return ((dnode_t *)vol->ext)->mnt; }

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

/* modo, dono, flags e datas do volume no vnode espelho. Dono 99 (_unknown, o dos volumes externos do macOS) aparece
 * como o usuário do mOS, como o macOS faz com a opção "ignorar permissões" */
static void meta_from_dnode(vnode_t *v, dnode_t *c) {
    if (c->mode) v->mode = (c->mode & 07777) | 0x10000u;
    if (c->has_owner) {
        v->uid = c->uid == 99 ? VFS_DEFAULT_UID : c->uid;
        v->gid = c->gid == 99 ? VFS_DEFAULT_GID : c->gid;
        v->flags = c->flags & ~(u32)0x20;                  /* UF_COMPRESSED é do driver */
    }
    if (c->mtime > 0) v->mtime_ns = c->mtime * 1000000000ll;
    if (c->btime > 0) v->btime_ns = c->btime * 1000000000ll;
    if (c->atime > 0) v->atime_ns = c->atime * 1000000000ll;
    if (c->ctime > 0) v->ctime_ns = c->ctime * 1000000000ll;
}

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
        meta_from_dnode(v, c);
        if (c->type != DN_DIR) { v->data = NULL; v->size = c->size; v->cap = 0; }
    }
    d->mirrored = true;
    kfree(base);
    kfree(path);
    mirroring--;
}

bool vfs_ensure(vnode_t *n) {
    if (n && !n->ext && !n->dir && n->dclus && !n->data && n->size) return persist_ensure(n);   /* sistema: do disco */
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
        kprintf("dfs: não foi possível ler %s inteiro (%llu bytes)\n", d->name, (u64)d->size);
        /* grande: continua paginado (vfs_read_at lê aos pedaços); pequeno: size 0, como antes, para quem só olha data */
        if (d->type != DN_LINK && d->size > VFS_PAGED_MIN) n->size = d->size;
        else n->size = 0;
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

/* o que o vfs_lookup faz: pasta espelhada; arquivo pequeno (e link) inteiro na RAM; grande fica paginado */
bool vfs_ensure_lookup(vnode_t *n) {
    if (!n || !n->ext) return true;
    dnode_t *d = n->ext;
    if (!n->dir && !n->data && d->type != DN_LINK && d->size > VFS_PAGED_MIN) {
        n->size = d->size;
        return true;
    }
    return vfs_ensure(n);
}

/* leitura de um trecho direto do volume (vnode paginado) */
i64 vfs_ext_read(vnode_t *n, u64 off, void *buf, u64 len) {
    dnode_t *d = n ? n->ext : NULL;
    if (!d || d->type == DN_DIR) return -1;
    if (off >= d->size) return 0;
    return dfs_read(d, off, buf, MIN(len, d->size - off));
}

/* ------------------------------------------------------------ ejetar */
bool vfs_is_volume(vnode_t *n) {
    if (!n || !n->ext) return false;
    dnode_t *d = n->ext;
    return d->mnt && d->mnt->root == d;
}

/* Consentimento de gravação POR VOLUME: negado por padrão (também no Live). Cada consentimento libera, na política de armazenamento,
 * SOMENTE a faixa da partição daquele volume; ejetar o volume revoga. Nunca libera um disco inteiro, o disco de origem do sistema, nem
 * nada quando a origem não é confiável. */
#define MAX_CONSENTS 8
static struct { dmount_t *m; blkdev_t *disk; u64 off, len; } consents[MAX_CONSENTS];

static void consent_revoke(dmount_t *m) {
    for (int i = 0; i < MAX_CONSENTS; i++)
        if (consents[i].m == m) { storage_revoke_write(consents[i].disk, consents[i].off, consents[i].len); consents[i].m = NULL; }
}

bool vol_enable_write(vnode_t *vol, char *err, usize cap) {
#define REFUSE(msg) do { if (err) snprintf(err, cap, "%s", msg); kprintf("vol: gravação NÃO liberada: %s\n", msg); return false; } while (0)
    if (!vol || !vfs_is_volume(vol)) REFUSE("não é um volume.");
    dmount_t *m = mount_of(vol);
    if (m->writable) return true;
    if (!m->ops->enable_write) REFUSE("este formato de volume só pode ser lido.");
    blkdev_t *root = m->dev;
    u64 base = 0;
    while (root->parent) { base += root->base; root = root->parent; }
    int di = storage_disk_index(root);
    if (di < 0) REFUSE("não é um volume de disco físico.");
    const origin_t *o = origin_get();
    if (o->status != ORIGIN_TRUSTED) REFUSE("a origem do sistema não foi identificada com segurança: nenhuma gravação é liberada.");
    if (di == o->disk) REFUSE("este disco contém o sistema em execução; use a persistência do assistente.");
    if (!blk_hw_writable(root)) REFUSE("o disco é protegido contra gravação.");
    int slot = -1;
    for (int i = 0; i < MAX_CONSENTS; i++) if (!consents[i].m) { slot = i; break; }
    if (slot < 0) REFUSE("limite de volumes liberados atingido; ejete algum.");
    if (!storage_grant_write(root, base, m->dev->size)) REFUSE("o armazenamento recusou liberar a escrita.");
    if (!m->ops->enable_write(m)) {
        storage_revoke_write(root, base, m->dev->size);
        REFUSE("o volume não pôde ser aberto para escrita (partição EFI, journal pendente ou formato não suportado).");
    }
    consents[slot].m = m; consents[slot].disk = root; consents[slot].off = base; consents[slot].len = m->dev->size;
    kprintf("vol: gravação liberada pelo usuário em \"%s\" (%s, faixa [%llu, +%llu))\n", m->volname, storage_disk_name(di), base, (u64)m->dev->size);
    return true;
#undef REFUSE
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
    char *vp = kmalloc(1024);
    if (vp) vfs_path(vol, vp, 1024);
    vfs_ev_mute++;                       /* os itens do volume não "somem": o volume sai (um evento Unmount) */
    bool del = vfs_delete(vol);
    vfs_ev_mute--;
    if (!del) { vol->ext = m->root; kfree(vp); return false; }
    if (vp) { vfs_ev_mount(vp, false); kfree(vp); }
    consent_revoke(m);
    dfs_unmount(m);
    return true;
}

/* ------------------------------------------------------------ montagem */
static bool is_image(const char *name);

/* monta uma imagem aberta pelo usuário (ex.: duplo clique num .dmg); retorna nº de volumes */
/* imagem montada para leitura e escrita: o dmg.c entrega o arquivo inteiro (CRCs já corrigidos) a cada flush, e
 * ele substitui o conteúdo do .dmg de origem, que segue o caminho normal de gravação (persistência do pendrive ou o
 * volume gravável onde ele está). arg = caminho do .dmg. */
static bool image_sink(void *arg, const u8 *img, u64 size) {
    vnode_t *f = vfs_lookup((const char *)arg);
    if (!f || f->dir) { kprintf("dfs: imagem %s sumiu; as mudanças do volume não foram gravadas\n", (const char *)arg); return false; }
    u8 *nd = kmalloc(size + 1);
    if (!nd) return false;
    memcpy(nd, img, size);
    nd[size] = 0;
    dnode_t *d = f->ext;                                /* conteúdo novo é só da RAM até o volume de origem gravar */
    if (d && d->in_lru && d->vn == f) lru_unlink(d);
    f->ext = NULL;
    if (f->cap) kfree(f->data);
    f->data = nd;
    f->size = size;
    f->cap = size + 1;
    vfs_touch(f, VT_MOD);
    persist_mark(f);
    return true;
}

int vfs_mount_file_ex(vnode_t *f, bool readonly) {
    if (!f || f->dir || !is_image(f->name)) return 0;
    char path[1024];
    vfs_path(f, path, sizeof(path));
    if (dfs_find_source(path)) return 0; /* já montada */
    dmount_t *before = dfs_mounts();
    /* gravável como no macOS (UDRW/crua), salvo -readonly ou .dmg num volume que só pode ser lido */
    bool rw = !readonly && (!f->ext || vfs_ext_writable(f));
    int n;
    if (f->ext) {
        u64 len = 0;                                        /* .dmg dentro de outro volume */
        u8 *data = dfs_read_all(f->ext, 512ull << 20, &len);
        n = !data ? 0 : rw ? dfs_mount_buffer_rw(data, len, path, image_sink, strdup(path)) : dfs_mount_buffer(data, len, path);
    } else if (f->cap == 0 && f->data && !rw) {
        n = dfs_mount_memory(f->data, f->size, path);       /* memória do pendrive: fixa */
    } else {
        /* arquivo criado/alterado nesta sessão (pode ser realocado) ou montado gravável: usa uma cópia */
        u8 *copy = f->size ? kmalloc(f->size) : NULL;
        if (!copy) return 0;
        memcpy(copy, f->data, f->size);
        n = rw ? dfs_mount_buffer_rw(copy, f->size, path, image_sink, strdup(path)) : dfs_mount_buffer(copy, f->size, path);
    }
    if (n) dfs_publish_new(before);
    return n;
}

int vfs_mount_file(vnode_t *f) { return vfs_mount_file_ex(f, false); }

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
        if (v) { v->ext = m->root; vfs_ev_mount(vpath, true); }
    }
}

static void scan(vnode_t *dir, int depth) {
    if (depth > 8) return;
    for (vnode_t *c = dir->child; c; c = c->next) {
        if (c->ext) continue; /* não entra nos volumes já montados */
        if (c->dir) { scan(c, depth + 1); continue; }
        if (!is_image(c->name) || !c->size) continue;
        if (!c->data && c->dclus && !persist_ensure(c)) continue;   /* do volume do sistema: inteira, em páginas fixas */
        if (c->in_blru) persist_forget(c);                          /* montada direto do buffer: não pode ser devolvida */
        if (!c->data) continue;
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
/* mudança que foi direto ao disco: um sync logo depois (o HFS+ volta a marcar "desmontado corretamente") */
static void touch(void) { vol_pending = true; vol_last = timer_ticks(); }

bool vfs_ext_writable(vnode_t *n) {
    dnode_t *d = n ? n->ext : NULL;
    return d && d->mnt && d->mnt->writable && d->mnt->ops->write_file;
}


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
    if (!n->data && n->size && !vfs_ensure(n)) return;   /* grande demais para a RAM: continua ligado ao volume */
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
        touch();
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
    touch();
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
        if (m->ops->rename(m, orel, np)) { touch(); return; }
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
static bool write_node(dmount_t *m, const char *p, vnode_t *n);
static bool flush_tree(vnode_t *vol, dmount_t *m, vnode_t *dir, int depth, int *count) {
    bool ok = true;
    if (depth > 64) return true;
    for (vnode_t *c = dir->child; c; c = c->next) {
        if (c->dir) { ok &= flush_tree(vol, m, c, depth + 1, count); continue; }
        if (!c->dirty || c->ext) continue;
        char p[1024];
        if (!rel_path(vol, c, p, sizeof p)) continue;
        if (write_node(m, p, c)) { c->dirty = false; (*count)++; }
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

/* O autoteste de gravação que existia aqui (vol_selftest, disparado pela existência de um arquivo no VFS) foi REMOVIDO
 * do kernel: ele gravava em todo volume gravável de /Volumes. A verificação de gravação em volumes agora é feita
 * por tools/voltest.c e por imagens virtuais descartáveis, fora do kernel (docs/volumes-graveis.md). */

/* statfs de um volume de /Volumes (kernel/lib/vfsops.c) */
bool vfs_volume_info(vnode_t *vol, mos_statfs_t *o) {
    if (!vfs_is_volume(vol)) return false;
    dmount_t *m = mount_of(vol);
    const char *t = m->ops && m->ops->name ? m->ops->name : "";
    const char *ty = !strcmp(t, "HFS+") ? "hfs" : !strcmp(t, "APFS") ? "apfs" : !strcmp(t, "FAT") ? "msdos" : "hfs";
    strncpy(o->fstype, ty, sizeof o->fstype - 1);
    if (m->source[0]) strncpy(o->mntfrom, m->source, sizeof o->mntfrom - 1);
    else snprintf(o->mntfrom, sizeof o->mntfrom, "/dev/disk%d", 2);
    if (m->volname[0]) strncpy(o->volname, m->volname, sizeof o->volname - 1);
    o->total = m->dev ? m->dev->size : 0;
    o->avail = 0;                                   /* os drivers não contam o livre; só leitura mostra 0 */
    if (!m->writable) o->flags |= 1;
    return true;
}

/* ------------------------------------------------------------ atributos estendidos dos volumes
 * Lidos do driver no primeiro uso do vnode (vfsmeta.c chama vfs_xattr_load) e gravados por ele antes de a RAM mudar
 * (vol_xattr_write). Arquivo novo que ainda não foi para o disco é gravado antes do atributo. */
static dnode_t *dnode_of(vnode_t *vol, vnode_t *n) {
    if (n->ext) return n->ext;
    char rel[1024], full[1300];
    if (!rel_path(vol, n, rel, sizeof rel)) return NULL;
    dmount_t *m = mount_of(vol);
    snprintf(full, sizeof full, "/Volumes/%s%s", m->root->name, strcmp(rel, "/") ? rel : "");
    return dfs_lookup(full);
}
void vfs_xattr_load(vnode_t *n) {
    vnode_t *vol = volume_of(n);
    if (!vol || !vol->ext) return;
    dmount_t *m = mount_of(vol);
    if (!m->ops->listxattr || !m->ops->getxattr) return;
    dnode_t *d = dnode_of(vol, n);
    if (!d) return;
    i64 need = m->ops->listxattr(m, d, NULL, 0);
    if (need <= 0) return;
    char *names = kmalloc((usize)need);
    if (!names) return;
    i64 got = m->ops->listxattr(m, d, names, (u64)need);
    for (i64 o = 0; got > 0 && o < got; o += (i64)strlen(names + o) + 1) {
        i64 vl = m->ops->getxattr(m, d, names + o, NULL, 0);
        if (vl < 0 || vl > (64ll << 20)) continue;
        u8 *v = kmalloc(vl ? (usize)vl : 1);
        if (v && m->ops->getxattr(m, d, names + o, v, (u64)vl) == vl) vfs_xattr_attach(n, names + o, v, (u64)vl);
        kfree(v);
    }
    kfree(names);
}
int vol_xattr_write(vnode_t *n, const char *name, const void *val, u64 len, bool remove) {
    vnode_t *vol = volume_of(n);
    if (!vol || !vol->ext) return 0;                       /* fora de /Volumes: só na RAM (e no .vfsmeta) */
    dmount_t *m = mount_of(vol);
    if (!m->writable) return -MOS_EROFS;
    if (!m->ops->setxattr || !m->ops->removexattr) return -MOS_ENOTSUP;   /* FAT: sem atributos */
    char p[1024];
    if (!rel_path(vol, n, p, sizeof p)) return -MOS_EINVAL;
    if (!n->dir && n->dirty && !n->ext) {                  /* o arquivo ainda não chegou ao disco */
        if (!write_node(m, p, n)) return -MOS_EIO;
        n->dirty = false;
    }
    bool ok = remove ? m->ops->removexattr(m, p, name) : m->ops->setxattr(m, p, name, val, len);
    return ok ? 0 : -MOS_EIO;
}

/* grava um arquivo pendente: link do mOS ("#mOS-link:") vira link simbólico de verdade no volume */
static bool write_node(dmount_t *m, const char *p, vnode_t *n) {
    static const char magic[] = "#mOS-link:";
    if (n->data && n->size > sizeof magic - 1 && !memcmp(n->data, magic, sizeof magic - 1) && m->ops->symlink) {
        char t[1024];
        usize k = MIN(n->size - (sizeof magic - 1), sizeof t - 1);
        memcpy(t, n->data + sizeof magic - 1, k);
        t[k] = 0;
        return m->ops->symlink(m, p, t);
    }
    return m->ops->write_file(m, p, n->data ? n->data : (const u8 *)"", n->size);
}
/* chmod/chown/chflags/utimes num volume: grava os campos pedidos pelo driver */
int vol_meta_write(vnode_t *n, unsigned mask) {
    vnode_t *vol = volume_of(n);
    if (!vol || !vol->ext || vol == n) return 0;
    dmount_t *m = mount_of(vol);
    if (!m->writable) return -MOS_EROFS;
    if (!m->ops->setattr) return 0;                        /* FAT: modo e dono não existem no disco */
    char p[1024];
    if (!rel_path(vol, n, p, sizeof p)) return -MOS_EINVAL;
    if (!n->dir && n->dirty && !n->ext) {
        if (!write_node(m, p, n)) return -MOS_EIO;
        n->dirty = false;
    }
    dfs_attr_t a = {0};
    a.mask = mask;
    a.mode = (u16)(n->mode & 07777);
    a.uid = n->uid;
    a.gid = n->gid;
    a.flags = n->flags;
    a.btime = n->btime_ns / 1000000000ll;
    a.mtime = n->mtime_ns / 1000000000ll;
    a.atime = n->atime_ns / 1000000000ll;
    a.ctime = n->ctime_ns / 1000000000ll;
    return m->ops->setattr(m, p, &a) ? 0 : -MOS_EIO;
}
