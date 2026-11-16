/* mOS — sistema de arquivos virtual em memória (populado a partir do pendrive no boot) */
#pragma once
#ifdef MOS_HOST_TEST
#include "vfs_host_kernel.h"   /* tools/vfsmeta_host: teste de host */
#else
#include "kernel.h"
#endif

/* atributo estendido (getxattr/setxattr): nome UTF-8 até 127 bytes, valor binário */
typedef struct vxattr {
    struct vxattr *next;
    u32 len;
    u8 *val;
    char name[128];
} vxattr_t;

typedef struct vnode {
    char name[256];
    bool dir;
    u8 *data;
    usize size, cap;
    u64 mtime;          /* ms desde o boot (ou 0 = arquivo do pendrive) */
    unsigned mode;      /* permissão; bit 0x10000 = foi gravada (open/chmod) */
    struct vnode *parent, *child, *next;
    void *ext;          /* nó de um sistema de arquivos externo (ex.: volume .dmg), carregado sob demanda */
    bool dirty;         /* alterado e ainda não gravado no disco do sistema (fs/persist.c) */
    /* metadados (kernel/lib/vfsmeta.c): valem para todo nó, persistidos em \mOS\.vfsmeta */
    bool meta;          /* tem metadados próprios (entra no .vfsmeta) */
    u64 ino;            /* número de inode, estável enquanto o nó existir (rename não muda) */
    u32 uid, gid;
    u32 flags;          /* st_flags do BSD: UF_* / SF_* */
    i64 atime_ns, mtime_ns, ctime_ns, btime_ns; /* ns desde 1970; 0 = desconhecido (usa a hora do boot) */
    vxattr_t *xattr;
    u64 xattr_bytes;
    bool xattr_loaded;  /* xattrs de um volume de /Volumes já lidos do driver (vfs_bridge.c) */
} vnode_t;

/* ---- metadados (kernel/lib/vfsmeta.c) */
#define VFS_UF_NODUMP     0x00000001
#define VFS_UF_IMMUTABLE  0x00000002
#define VFS_UF_APPEND     0x00000004
#define VFS_UF_OPAQUE     0x00000008
#define VFS_UF_COMPRESSED 0x00000020
#define VFS_UF_TRACKED    0x00000040
#define VFS_UF_DATAVAULT  0x00000080
#define VFS_UF_HIDDEN     0x00008000
#define VFS_UF_SETTABLE   0x0000ffff
#define VFS_SF_ARCHIVED   0x00010000
#define VFS_SF_IMMUTABLE  0x00020000
#define VFS_SF_APPEND     0x00040000
#define VFS_SF_RESTRICTED 0x00080000
#define VFS_SF_NOUNLINK   0x00100000
#define VFS_SF_SETTABLE   0xffff0000
#define VFS_DEFAULT_UID   501   /* o usuário do mOS (conta única do sistema; ver cred.c) */
#define VFS_DEFAULT_GID   20    /* staff */
#define VFS_XATTR_NAME_MAX  127
#define VFS_XATTR_VAL_MAX   (64u << 20)   /* um valor (o resource fork pode ser grande) */
#define VFS_XATTR_NODE_MAX  (128u << 20)  /* soma por nó */
#define VFS_XATTR_CREATE  0x0002          /* opções do setxattr do macOS */
#define VFS_XATTR_REPLACE 0x0004
enum { VT_ATIME = 1, VT_MTIME = 2, VT_CTIME = 4, VT_BTIME = 8, VT_MOD = VT_MTIME | VT_CTIME };
i64  vfs_now_ns(void);
void vfs_meta_init(vnode_t *n, vnode_t *parent);   /* nó novo: inode, dono e datas */
void vfs_touch(vnode_t *n, int which);             /* datas = agora (e mtime em ms do boot com VT_MTIME) */
void vfs_meta_changed(vnode_t *n);                 /* metadado mudou: ctime e gravação do .vfsmeta */
u64  vfs_ino_next(void);
void vfs_ino_reserve(u64 ino);                     /* inode lido do .vfsmeta: o contador passa dele */
i64  vfs_xattr_get(vnode_t *n, const char *name, void *buf, u64 cap, u64 pos); /* tamanho ou -errno */
int  vfs_xattr_set(vnode_t *n, const char *name, const void *val, u64 len, u64 pos, int opts);
i64  vfs_xattr_list(vnode_t *n, char *buf, u64 cap);  /* bytes ("a\0b\0") ou -errno */
int  vfs_xattr_remove(vnode_t *n, const char *name);
vxattr_t *vfs_xattr_find(vnode_t *n, const char *name);
void vfs_xattr_free(vnode_t *n);
void vfs_xattr_attach(vnode_t *n, const char *name, const void *val, u64 len);   /* carga do disco: sem evento nem write-through */
/* ganchos dos volumes (vfs_bridge.c; padrão = nada): ler os xattrs do driver e gravar através dele */
void vfs_xattr_load(vnode_t *n);
int  vol_xattr_write(vnode_t *n, const char *name, const void *val, u64 len, bool remove);   /* 0 ou -errno */
int  vol_meta_write(vnode_t *n, unsigned mask);    /* DA_MODE/OWNER/FLAGS/TIMES (fs.h) pelo driver; 0 ou -errno */
#define VFS_META_MODE  1
#define VFS_META_OWNER 2
#define VFS_META_FLAGS 4
#define VFS_META_TIMES 8
bool vfs_xattr_copy(vnode_t *dst, const vnode_t *src);
bool vfs_immutable(vnode_t *n);                    /* uchg/schg: não pode mudar conteúdo, nome nem metadados */
bool vfs_append_only(vnode_t *n);                  /* uappnd/sappnd: só acrescentar */
bool vfs_no_unlink(vnode_t *n);                    /* imutável, append-only ou sunlnk: não pode apagar/renomear */
i64  vfs_time_ns(const vnode_t *n, int which);

/* ---- permissões e ACL (kernel/lib/vfsperm.c). Direitos e ACEs com os valores do xnu (sys/kauth.h). */
#define VFS_XATTR_SECURITY "com.apple.system.Security"   /* kauth_filesec em big-endian; escondido do listxattr */
#define VFS_ACE_PERMIT 1
#define VFS_ACE_DENY   2
#define VFS_ACE_INHERITED         (1u << 4)
#define VFS_ACE_FILE_INHERIT      (1u << 5)
#define VFS_ACE_DIRECTORY_INHERIT (1u << 6)
#define VFS_ACE_LIMIT_INHERIT     (1u << 7)
#define VFS_ACE_ONLY_INHERIT      (1u << 8)
#define VFS_KR_READ_DATA       (1u << 1)    /* = LIST_DIRECTORY */
#define VFS_KR_WRITE_DATA      (1u << 2)    /* = ADD_FILE */
#define VFS_KR_EXECUTE         (1u << 3)    /* = SEARCH */
#define VFS_KR_DELETE          (1u << 4)
#define VFS_KR_APPEND_DATA     (1u << 5)    /* = ADD_SUBDIRECTORY */
#define VFS_KR_DELETE_CHILD    (1u << 6)
#define VFS_KR_READ_ATTRIBUTES (1u << 7)
#define VFS_KR_WRITE_ATTRIBUTES (1u << 8)
#define VFS_KR_READ_EXTATTR    (1u << 9)
#define VFS_KR_WRITE_EXTATTR   (1u << 10)
#define VFS_KR_READ_SECURITY   (1u << 11)
#define VFS_KR_WRITE_SECURITY  (1u << 12)
#define VFS_KR_TAKE_OWNERSHIP  (1u << 13)
int  vfs_authorize(vnode_t *v, u32 rights, int euid, int egid);   /* 0, -EACCES ou -EPERM */
int  vfs_access(vnode_t *v, int rwx, int euid, int egid);          /* rwx: 4 ler, 2 escrever, 1 executar/buscar */
int  vfs_may_delete(vnode_t *n, int euid, int egid);               /* unlink/rmdir/rename (sticky incluso) */
int  vfs_may_create(vnode_t *d, bool dir, int euid, int egid);
void vfs_acl_inherit(vnode_t *n, vnode_t *dir);                     /* nó novo: ACEs herdáveis da pasta */
bool vfs_filesec_valid(const u8 *d, u64 n);
bool vfs_in_group(u32 gid, int euid, int egid);
/* ---- operações do macOS (kernel/lib/vfsops.c); mos_statfs_t em syscall.h */
int  vfs_statfs(vnode_t *v, void *mos_statfs);
vnode_t *vfs_by_ino(u64 ino);
int  vfs_clone(vnode_t *src, const char *dst, u32 flags, int euid, int egid);
int  vfs_exchange(vnode_t *a, vnode_t *b);
int  vfs_swap(vnode_t *a, vnode_t *b);
void vfs_guid_user(u8 g[16], u32 uid);
void vfs_guid_group(u8 g[16], u32 gid);
int  vfs_guid_id(const u8 g[16], u32 *id);                          /* 1 usuário, 2 grupo, 0 outro */     /* a data, com a hora do boot quando desconhecida */
void persist_meta_mark(void);
/* ---- journal de eventos (kernel/lib/vfsevents.c) para FSEvents/EVFILT_VNODE; flags FSEV_* do syscall.h */
extern int vfs_ev_mute;                            /* > 0: mudanças não viram eventos (espelho de volume, ejetar) */
void vfs_ev(vnode_t *n, u32 flags);
void vfs_ev_rename(vnode_t *n, const char *old_path);
void vfs_ev_mount(const char *path, bool mounted);
void vfs_ev_set_base(u64 id);
u64  vfs_ev_next_id(void);
int  vfs_ev_read(u64 since, void *out_mos_fsevent, int max, u64 *latest);

/* núcleo do SYS_FSMETA (syscall.h): rq e kbuf já no kernel; name = nome do xattr (ou NULL) */
i64  vfs_fsmeta(vnode_t *v, int op, const void *rq, const char *name, void *kbuf, int euid, int egid);
void vfs_statx(vnode_t *v, void *mos_statx);
/* .vfsmeta (fs/persist.c): serialização dos metadados dos nós com v->meta */
typedef bool (*vmeta_track_fn)(void *ctx, vnode_t *v, char *vpath, usize cap);  /* entra? (e o caminho) */
typedef vnode_t *(*vmeta_lookup_fn)(void *ctx, const char *vpath);
u8  *vmeta_build(vnode_t *root, vmeta_track_fn track, void *ctx, u64 *len);  /* kfree no retorno */
bool vmeta_check(const u8 *d, u64 n, u64 *next_ino);
int  vmeta_apply(const u8 *d, u64 n, vmeta_lookup_fn lookup, void *ctx);    /* nº de nós restaurados */
u32  vfs_crc32(const void *d, u64 n);                      /* fs/persist.c: o .vfsmeta precisa ser regravado */

/* persistência no disco do sistema (kernel/fs/persist.c; padrão = sem disco, só RAM) */
bool persist_load(void);                  /* carrega a árvore do disco; false = usar arquivos do boot */
void persist_enable(void);
void persist_mark(vnode_t *n);            /* pasta criada (grava já) ou arquivo alterado (grava depois) */
void persist_delete(vnode_t *n);          /* antes de remover n */
void persist_move(const char *old_path, vnode_t *n); /* depois de mover/renomear */
void persist_sync(void);                  /* grava tudo que estiver pendente */
void persist_capture_existing(void);      /* nós criados na RAM (mtime != 0 ou dirty) entram no próximo sync */
void persist_tick(void);                  /* chamado no loop principal */
bool persist_available(void);

/* ganchos para sistemas de arquivos externos (implementados em kernel/fs/vfs_bridge.c; padrão = no-op) */
bool vfs_ensure(vnode_t *n);   /* materializa filhos/conteúdo de um nó com ext */
void dfs_mount_all(void);      /* monta imagens .dmg/.img encontradas em /Volumes */
bool vfs_is_volume(vnode_t *n); /* n é a raiz de um volume montado em /Volumes */
bool vfs_eject(vnode_t *vol);   /* desmonta e libera o volume (n deixa de ser válido) */
int  vfs_mount_file(vnode_t *f); /* monta uma imagem .dmg/.img; devolve nº de volumes criados (0 = falha/já montada) */
bool vfs_is_read_only(vnode_t *n); /* dentro de um volume externo (somente leitura) */
bool vfs_ext_writable(vnode_t *n); /* n->ext é de um volume gravável (FAT/HFS+ em disco) */
bool vol_enable_write(vnode_t *vol, char *err, usize cap); /* consentimento do usuário para gravar NESTE volume (liberação só da faixa dele) */
/* escrita em volumes de /Volumes (vfs_bridge.c), chamados pelos ganchos de persistência */
void vol_mark(vnode_t *n);                  /* pasta criada (já) ou arquivo alterado (depois) */
void vol_delete(vnode_t *n);                /* antes de remover n */
void vol_move(const char *old_path, vnode_t *n); /* depois de mover/renomear */
bool vol_sync_all(void);                    /* grava o pendente de todos os volumes */
void vol_tick(void);                        /* gravação atrasada (~1,5 s depois da última mudança) */
bool vol_flush(vnode_t *vol);               /* grava o pendente de um volume (antes de ejetar) */

vnode_t *vfs_root(void);
vnode_t *vfs_lookup(const char *path);
vnode_t *vfs_lookup_dir(const char *path);   /* segue link também no último componente; NULL se não for pasta */
vnode_t *vfs_mkdir(const char *path);              /* cria toda a cadeia */
vnode_t *vfs_create(const char *path);             /* arquivo vazio (sobrescreve) */
vnode_t *vfs_write_file(const char *path, const void *data, usize n);
bool     vfs_copy_tree(const char *src, const char *dst); /* pasta ou arquivo; destino existente fica como está */
bool     vfs_append(vnode_t *f, const void *data, usize n);
bool     vfs_delete(vnode_t *n);
bool     vfs_move(vnode_t *n, vnode_t *newparent, const char *newname);
void     vfs_path(vnode_t *n, char *buf, usize cap);
int      vfs_count(vnode_t *dir);
vnode_t *vfs_child(vnode_t *dir, const char *name);
int      vfs_search(const char *q, const char **out, int max);
usize    vfs_total_bytes(void);
const char *vfs_kind(vnode_t *n);
const char *vfs_ext(const char *name);

#define HOME "/Users/Visitante"
#define TRASH HOME "/.Lixeira"

/* pasta pessoal da conta atual (account_home()) + sufixo; buffers rotativos, não guardar o ponteiro */
const char *home_sub(const char *sub);
#define CUR_HOME  home_sub("")
#define CUR_TRASH home_sub("/.Lixeira")
