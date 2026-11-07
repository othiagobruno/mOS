/* mOS — registro de discos físicos (AHCI, NVMe, USB...). Os drivers registram cada disco como
 * blkdev_t (fs.h); a persistência (fs/persist.c) e a montagem em /Volumes usam esta lista. */
#pragma once
#include "fs.h"

void        storage_register(blkdev_t *dev, const char *name, const char *desc); /* name: "sata0", "nvme0n1"... */

/* ---- identidade física do disco (cadeia PCI + porta/namespace), comparada com o caminho de dispositivo UEFI
 * de onde o sistema foi carregado (fs/origin.c). Os drivers a preenchem logo depois de storage_register(). */
enum { PHYS_NONE, PHYS_SATA, PHYS_NVME, PHYS_USB };
typedef struct {
    u8  kind;
    u8  npci;  u8 pci[8][2];            /* cadeia da raiz até o controlador: {dispositivo, função} */
    u32 sata_port;                      /* PHYS_SATA: porta do HBA */
    u32 nsid;                           /* PHYS_NVME */
    u8  nusb;  u8 usb_port[8];          /* PHYS_USB: portas (numeração do kernel, a partir de 1) da raiz até o dispositivo */
} diskphys_t;
void        storage_set_phys(blkdev_t *dev, const diskphys_t *p);
const diskphys_t *storage_disk_phys(int i);
int         storage_disk_index(blkdev_t *root);                      /* índice de um disco físico registrado, ou -1 */
void        pci_chain(pci_dev_t *d, diskphys_t *p);                  /* dev/pci.c: preenche npci/pci[] */

/* ---- contenção de escrita (storage.c): NADA grava em disco físico sem uma faixa liberada de forma explícita.
 * Faixas são em bytes absolutos do disco físico. Os drivers consultam storage_write_ok() no ponto mais baixo
 * (comando de escrita), então nenhum caminho do kernel (VFS, logs, contas, montagens, instalador, selftests)
 * contorna a política. */
bool        storage_grant_write(blkdev_t *disk, u64 off, u64 len);   /* disk = disco físico registrado */
void        storage_revoke_write(blkdev_t *disk, u64 off, u64 len);  /* revoga só a faixa liberada exatamente assim */
void        storage_revoke_all(void);
bool        storage_write_ok(blkdev_t *root, u64 off, u64 len);      /* chamado pelos drivers */
u64         storage_denied_writes(void);                             /* tentativas recusadas desde o boot */
int         storage_grant_count(void);
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
typedef struct { u64 first, last; const u8 *type; const char *name; } gpt_new_t;   /* LBAs inclusivos */
extern const u8 GPT_TYPE_ESP[16], GPT_TYPE_DATA[16];
bool        gpt_create(blkdev_t *disk, const gpt_new_t *parts, int n);              /* tabela NOVA (apaga a anterior), 1..4 partições */
bool        gpt_add_partition(blkdev_t *disk, const gpt_new_t *p, int *number);     /* acrescenta numa GPT válida, sem tocar nas demais */
u32        crc32_calc(const void *data, usize n);

/* ---- volume do sistema (fs/persist.c) */
fatfs_t    *persist_volume(void);                                   /* NULL se rodando só em RAM */
int         persist_disk_index(void);                               /* índice em storage_disk(), ou -1 */

/* instalador: ver include/install.h */
