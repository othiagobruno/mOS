/*
 * mOS — sistema de arquivos virtual e compatibilidade com imagens de disco do macOS.
 *
 * Inspirado na pilha de armazenamento do ravynOS (Darwin/FreeBSD), mas reescrito do zero
 * para o Nucleo: tudo é somente leitura, preguiçoso (nada é descomprimido antes de ser lido)
 * e sem dependências externas.
 *
 *   .dmg (UDIF: raw, zero, ADC, zlib, bzip2, LZFSE, LZMA)  ->  disco virtual
 *   disco virtual (GPT, APM, MBR ou volume direto)   ->  partições
 *   partição HFS+/HFSX ou contêiner APFS             ->  /Volumes/<nome>
 *
 * Volumes FAT12/16/32 e HFS+/HFSX de discos graváveis (AHCI, NVMe, pendrive) também aceitam escrita
 * (dfs_ops_t.write_file & cia.; ver docs/volumes-graveis.md). Imagens .dmg e APFS continuam só leitura.
 */
#pragma once
#ifdef MOS_HOST_TEST
#include "host_shim.h"
#else
#include "kernel.h"
#endif

/* ---------------------------------------------------------------- dispositivos de bloco */
typedef struct blkdev {
    u64 size;                                              /* bytes */
    bool (*read)(struct blkdev *d, u64 off, void *buf, u64 len);
    bool (*write)(struct blkdev *d, u64 off, const void *buf, u64 len); /* NULL = somente leitura */
    bool (*flush)(struct blkdev *d);                                    /* opcional */
    void (*release)(struct blkdev *d);
    void *ctx;
    u64 base;                                              /* deslocamento (partições) */
    struct blkdev *parent;
    u32 lbs;                                               /* tamanho do bloco lógico do hardware (0 = 512) */
} blkdev_t;

/* Contenção de escrita (dev/storage.c): todo disco físico começa SEM permissão de escrita. Os drivers
 * (ahci/nvme/usb_msc) recusam qualquer comando de escrita fora das faixas liberadas por storage_grant_write(),
 * e blk_writable() só vale enquanto houver faixa liberada que cubra o dispositivo. */
#ifdef MOS_HOST_TEST
static inline bool blk_policy_allows(struct blkdev *d) { (void)d; return true; }
#else
bool blk_policy_allows(struct blkdev *d);
#endif

static inline bool blk_hw_writable(blkdev_t *d) { return d->write != NULL; }   /* o hardware aceita escrita (ignora a política) */
static inline bool blk_writable(blkdev_t *d) { return d->write != NULL && blk_policy_allows(d); }
/* Leitura/escrita passam pelo page cache unificado (fs/pcache.c). A escrita é writeback:
 * o dispositivo só muda em blk_flush, pcache_tick ou fsync/aio_fsync. */
bool blk_read(blkdev_t *d, u64 off, void *buf, u64 len);
bool blk_write(blkdev_t *d, u64 off, const void *buf, u64 len);
bool blk_flush(blkdev_t *d);
void pcache_tick(void);          /* flush periódico do writeback + um passo de I/O assíncrono */
void pcache_drop(blkdev_t *d);   /* solta páginas deste disco (blk_release) */

typedef struct pcache_stats {
    u64 hits, misses, dev_reads, dev_read_bytes, dev_writes, writebacks, ra_pages;
} pcache_stats_t;
void pcache_stats(pcache_stats_t *o);
void pcache_stats_reset(void);

/* I/O assíncrono de bloco: submeter e consultar não bloqueiam; aio_pump/pcache_tick completam. */
#define MOS_EINPROGRESS 36
typedef struct blk_aiocb {
    blkdev_t *dev;
    u64 off;
    void *buf;
    u64 len;
    bool write;
    int err;     /* MOS_EINPROGRESS enquanto pendente; 0 concluído */
    i64 ret;
    void *link;
} blk_aiocb_t;
int  blk_aio_submit(blk_aiocb_t *cb); /* 0 enfileirado; -1 recusado */
int  blk_aio_error(const blk_aiocb_t *cb);
i64  blk_aio_return(blk_aiocb_t *cb);
void aio_pump(void);

/* caminho POSIX (aiocb mínimo). O fd aponta para um blkdev ligado com aio_bind. */
struct aiocb {
    int aio_fildes;
    int aio_pad;
    i64 aio_offset;
    volatile void *aio_buf;
    u64 aio_nbytes;
    int aio_reqprio;
    int aio_lio_opcode; /* 0 = segundo o pedido (read/write); 1 = fsync de dados */
};
/* fila de aio sobre memória de arquivo (syscall SYS_AIO). O ponteiro do aiocb do usuário é a chave. */
int  aio_mem_submit(u64 key, int write, u8 *mem, u64 memlen, u64 off, u64 n, u8 *bounce, void *ubuf);
int  aio_mem_poll(u64 key, i64 *ret);                 /* MOS_EINPROGRESS, 0 ou errno */
void aio_mem_take(u64 key, u8 **bounce, u64 *n, void **ubuf, i64 *ret);
int  aio_mem_fsync(u64 key);
int  aio_bind(int fd, blkdev_t *dev);
int  aio_read(struct aiocb *cb);
int  aio_write(struct aiocb *cb);
int  aio_error(const struct aiocb *cb);
i64  aio_return(struct aiocb *cb);
int  aio_fsync(int op, struct aiocb *cb);
int  aio_suspend(const struct aiocb *const list[], int nent, const void *timeout);
int  aio_suspend_mem(const u64 *keys, int nent, const void *timeout); /* SYS_AIO: chaves do processo */
#ifdef MOS_HOST_TEST
extern u64 aio_host_ms;   /* relógio do aio_suspend no teste de host, em ms */
extern int aio_host_intr; /* 1 = a próxima espera devolve EINTR */
#endif
blkdev_t *blk_from_memory(const u8 *data, u64 size);
blkdev_t *blk_slice(blkdev_t *parent, u64 off, u64 size);
void      blk_release(blkdev_t *d);

/* ---------------------------------------------------------------- descompressores
 * Todos retornam o número de bytes escritos em dst, ou -1 em erro. */
i64 zlib_decompress(const u8 *src, u64 slen, u8 *dst, u64 dlen);   /* zlib ou deflate puro */
i64 bzip2_decompress(const u8 *src, u64 slen, u8 *dst, u64 dlen);
i64 lzfse_decompress(const u8 *src, u64 slen, u8 *dst, u64 dlen);  /* bvx2/bvx-/bvxn */
i64 lzvn_decompress(const u8 *src, u64 slen, u8 *dst, u64 dlen);
i64 adc_decompress(const u8 *src, u64 slen, u8 *dst, u64 dlen);
i64 xz_decompress(const u8 *src, u64 slen, u8 *dst, u64 dlen);     /* .xz com LZMA2 (chunks ULMO) */

/* decmpfs (compressão transparente de arquivos do HFS+ e APFS) */
typedef bool (*fork_reader_t)(void *ctx, u64 off, void *buf, u64 len);
u8 *decmpfs_decode(const u8 *xattr, u64 xlen, fork_reader_t rsrc, void *rctx, u64 rsrc_size, u64 *out_size);
bool decmpfs_header(const u8 *xattr, u64 xlen, u32 *type, u64 *size);

/* ---------------------------------------------------------------- DMG (UDIF) */
blkdev_t *dmg_open(const u8 *data, u64 size, char *err, usize errlen);
const char *dmg_format_name(blkdev_t *d);

/* ---------------------------------------------------------------- VFS */
enum { DN_FILE = 1, DN_DIR = 2, DN_LINK = 3 };

struct dnode;
struct dmount;
typedef struct dfs_ops {
    const char *name;
    bool (*readdir)(struct dmount *m, struct dnode *dir);              /* cria os filhos */
    i64  (*read)(struct dmount *m, struct dnode *f, u64 off, void *buf, u64 len);
    void (*unmount)(struct dmount *m);                                 /* libera o estado do driver */
    void (*free_node)(struct dnode *n);                                /* libera o conteúdo de n->priv */
    void (*trim)(struct dmount *m);                                    /* solta caches (pressão de memória) */
    /* escrita (opcional: NULL = somente leitura; vale só com dmount->writable). Caminhos relativos à raiz
     * do volume ("/a/b.txt"), nomes UTF-8. O VFS do sistema (vfs_bridge.c) chama estas funções quando um
     * processo/app altera algo em /Volumes/<nome>. Cada chamada deixa o volume consistente no disco. */
    bool (*write_file)(struct dmount *m, const char *path, const void *data, u64 len); /* cria ou substitui */
    bool (*mkdir)(struct dmount *m, const char *path);                 /* o pai existe; true se já existir */
    bool (*remove)(struct dmount *m, const char *path);                /* arquivo ou pasta (recursivo) */
    bool (*rename)(struct dmount *m, const char *from, const char *to);/* mover/renomear no mesmo volume */
    bool (*sync)(struct dmount *m);                                    /* grava caches; true = tudo no disco */
    bool (*enable_write)(struct dmount *m);                            /* o usuário permitiu gravar neste volume (vfs_bridge.c: vol_enable_write); NULL = não suporta */
} dfs_ops_t;

/* uma imagem aberta: disco virtual + partições; vive enquanto houver volume montado dela */
typedef struct dimage {
    blkdev_t *disk;
    blkdev_t *parts[16];
    int nparts, mounts;
    u8 *owned;              /* bytes da imagem alocados por nós (imagem dentro de volume) */
} dimage_t;

typedef struct dmount {
    const dfs_ops_t *ops;
    blkdev_t *dev;
    struct dnode *root;
    char volname[768];      /* até 255 caracteres em UTF-8 */
    char source[256];       /* caminho do .dmg de origem */
    char format[32];        /* "UDZO", "ULFO", ... */
    void *priv;
    dimage_t *img;
    bool writable;          /* o driver aceita write_file/mkdir/remove/rename (disco gravável) */
    struct dmount *next;
} dmount_t;

typedef struct dnode {
    char *name;             /* UTF-8 (NFC para nomes latinos) */
    u8   type;
    bool listed;            /* filhos já carregados */
    bool hidden;
    bool mirrored;          /* filhos já copiados para o VFS do sistema (vfs_bridge.c) */
    u16  mode;
    u64  size;
    i64  mtime;             /* segundos Unix */
    u64  ino;               /* identificador interno do fs */
    const u8 *mem;          /* arquivos em RAM (vindos do boot) */
    dmount_t *mnt;           /* NULL = vnode em RAM */
    void *priv;             /* dados do driver (liberado com kfree) */
    struct dnode *parent, *child, *next;
    /* usados por vfs_bridge.c */
    void *vn;               /* vnode espelho com conteúdo carregado */
    struct dnode *lru_prev, *lru_next;
    bool in_lru;
} dnode_t;

void     dfs_init(void);
dnode_t *dfs_root(void);
dnode_t *dfs_lookup(const char *path);           /* "/Volumes/App/App.app/Contents" */
dnode_t *dfs_first_child(dnode_t *dir);          /* carrega o diretório se preciso */
i64      dfs_read(dnode_t *f, u64 off, void *buf, u64 len);
u8      *dfs_read_all(dnode_t *f, u64 max, u64 *out_len);   /* kfree no retorno */
void     dfs_path(dnode_t *n, char *buf, usize len);
dmount_t *dfs_mounts(void);
int      dfs_mount_memory(const u8 *data, u64 size, const char *path); /* nº de volumes */
int      dfs_mount_buffer(u8 *data, u64 size, const char *path);       /* assume a posse (kfree no fim) */
dmount_t *dfs_find_source(const char *path);     /* volume já montado a partir desta imagem */
void     dfs_publish_new(dmount_t *before);       /* vfs_bridge.c: mostra em /Volumes os montados após "before" */
int      dfs_mount_image(dnode_t *file);         /* nº de volumes montados */
int      dfs_mount_disk(blkdev_t *disk, const char *name); /* disco físico; a imagem passa a ser dona do blkdev se montar algo */

/* ---------------------------------------------------------------- FAT32 (fs/fat.c), leitura e escrita
 * Caminhos absolutos com '/', nomes UTF-8, comparação sem diferenciar maiúsculas (como o FAT). */
typedef struct fatfs fatfs_t;
typedef void (*fat_list_cb)(void *ctx, const char *name, bool is_dir, u64 size);
fatfs_t *fat_mount(blkdev_t *part);              /* NULL se não for FAT32 */
void     fat_unmount(fatfs_t *fs);
const char *fat_label(fatfs_t *fs);
bool     fat_stat(fatfs_t *fs, const char *path, bool *is_dir, u64 *size);
int      fat_list(fatfs_t *fs, const char *path, fat_list_cb cb, void *ctx); /* nº de entradas ou -1 */
u8      *fat_read_file(fatfs_t *fs, const char *path, u64 *len);  /* kfree no retorno */
bool     fat_write_file(fatfs_t *fs, const char *path, const void *data, u64 len); /* cria ou substitui */
bool     fat_mkdir(fatfs_t *fs, const char *path);   /* o pai precisa existir; true se já existir */
bool     fat_remove(fatfs_t *fs, const char *path);  /* arquivo ou pasta (recursivo) */
bool     fat_rename(fatfs_t *fs, const char *from, const char *to);
bool     fat_sync(fatfs_t *fs);
bool     fat_replace(fatfs_t *fs, const char *from, const char *to); /* troca atômica de "to" por "from" (retomável); sem "to" = rename */
bool     fat_flush(fatfs_t *fs);                     /* fat_sync + flush do dispositivo */
void     fat_enable_rw(fatfs_t *fs);                 /* reavalia a política de escrita (depois de storage_grant_write) */
void     fat_set_system(fatfs_t *fs);                /* este volume não é exposto em /Volumes */
int      fat_probe(blkdev_t *dev, const char *source, const char *fmt);

/* ---------------------------------------------------------------- discos SATA (dev/ahci.c) */
blkdev_t *ahci_disk(int i);
void     dfs_unmount(dmount_t *m);                /* libera nós, driver e (se for o último) a imagem */
void     dfs_trim(void);                          /* solta caches de todos os volumes */
void     dfs_dump(dnode_t *n, int max_depth);    /* lista pela serial */

/* usados pelos drivers */
dnode_t *dfs_new_child(dnode_t *dir, const char *name, u8 type, u64 size);
void     utf16be_to_utf8(const u8 *src, u32 nchars, char *dst, usize cap);
void     utf8_compose(char *s);                  /* NFD -> NFC para acentos latinos */

/* detectores de sistemas de arquivos: retornam nº de volumes montados */
int hfsplus_probe(blkdev_t *dev, const char *source, const char *fmt);
int apfs_probe(blkdev_t *dev, const char *source, const char *fmt);
dmount_t *dfs_attach(const dfs_ops_t *ops, blkdev_t *dev, const char *volname, const char *source,
                    const char *fmt, void *priv, u64 root_ino);

/* leitura big/little-endian */
static inline u16 be16(const void *p) { const u8 *b = p; return (u16)(b[0] << 8 | b[1]); }
static inline u32 be32(const void *p) { const u8 *b = p; return (u32)b[0] << 24 | (u32)b[1] << 16 | (u32)b[2] << 8 | b[3]; }
static inline u64 be64(const void *p) { return (u64)be32(p) << 32 | be32((const u8 *)p + 4); }
static inline u16 le16(const void *p) { const u8 *b = p; return (u16)(b[0] | b[1] << 8); }
static inline u32 le32(const void *p) { const u8 *b = p; return (u32)b[0] | (u32)b[1] << 8 | (u32)b[2] << 16 | (u32)b[3] << 24; }
static inline u64 le64(const void *p) { return (u64)le32(p) | (u64)le32((const u8 *)p + 4) << 32; }
