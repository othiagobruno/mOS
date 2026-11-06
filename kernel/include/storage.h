/* mOS — registro de discos físicos (AHCI, NVMe, USB...). Os drivers registram cada disco como
 * blkdev_t (fs.h); a persistência (fs/persist.c) e a montagem em /Volumes usam esta lista. */
#pragma once
#include "fs.h"

void        storage_register(blkdev_t *dev, const char *name, const char *desc); /* name: "sata0", "nvme0n1"... */
int         storage_disk_count(void);
blkdev_t   *storage_disk(int i);
const char *storage_disk_name(int i);
const char *storage_disk_desc(int i);

void        nvme_init(void);   /* dev/nvme.c: registra cada namespace NVMe */
int         nvme_disk_count(void);

/* ---- formatação e partições (fs/fat.c, fs/gpt.c) */
bool        fat_format(blkdev_t *part, const char *label);          /* FAT32, 33 MB .. 2 TB */
int         fat_type(fatfs_t *fs);                                   /* 12, 16 ou 32 */
bool        gpt_create_single_esp(blkdev_t *disk, const char *name, u64 *part_off, u64 *part_size);
u32         crc32_calc(const void *data, usize n);

/* ---- volume do sistema (fs/persist.c) */
fatfs_t    *persist_volume(void);                                   /* NULL se rodando só em RAM */
int         persist_disk_index(void);                               /* índice em storage_disk(), ou -1 */

/* ---- instalador (fs/install.c): copia o sistema em execução para outro disco.
 * Uso pela interface: install_start(i) e depois install_poll() a cada quadro até state != INSTALL_RUNNING.
 * APAGA o disco de destino inteiro. */
enum { INSTALL_IDLE, INSTALL_RUNNING, INSTALL_DONE, INSTALL_ERROR };
typedef struct {
    int  state;
    int  percent;              /* 0..100 */
    char step[96];             /* "Particionando", "Copiando /mOS/...", ... */
    char error[128];
    u64  bytes_done, bytes_total;
    int  files_done, files_total;
} install_status_t;
bool        install_can_target(int disk, char *why, usize whylen);  /* false: é o disco do sistema, pequeno, somente leitura... */
bool        install_start(int disk);
void        install_poll(void);                                     /* faz uma fatia do trabalho (~1 arquivo) */
const install_status_t *install_status(void);
