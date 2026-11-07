/* mOS — origem do sistema em execução: de qual disco/partição o firmware UEFI carregou o kernel e \mOS\.
 * A identificação NÃO é heurística: exige (1) o caminho de dispositivo UEFI casando com a identidade física do
 * disco registrado (cadeia PCI + porta SATA/namespace NVMe/porta USB) E (2) o nó de partição (número, LBA inicial,
 * tamanho e GUID/assinatura) casando com exatamente uma partição desse disco. Qualquer dúvida => AMBÍGUA, e então
 * nada grava em disco e a instalação fica bloqueada (o sistema roda só na RAM). */
#pragma once
#include "storage.h"

enum { ORIGIN_UNKNOWN, ORIGIN_TRUSTED, ORIGIN_AMBIGUOUS };

typedef struct {
    bool valid;                 /* caminho bem formado e com tipos suportados */
    bool unsupported;           /* há nós que não sabemos casar (ATAPI, SCSI, ...) */
    diskphys_t phys;            /* identidade física; usb_port já em numeração do kernel */
    bool have_part;
    u32  part_no;
    u64  part_start, part_size; /* em blocos do dispositivo (512 bytes) */
    u8   sig[16];
    u8   sig_type;              /* 0 nenhuma, 1 MBR, 2 GPT */
    char err[96];
} devpath_t;

typedef struct {
    int  status;                /* ORIGIN_* */
    int  disk;                  /* índice em storage_disk() (TRUSTED) */
    int  bus;                   /* PHYS_SATA/NVME/USB */
    int  part_number;
    u64  part_off, part_size;   /* em bytes, absolutos no disco */
    char why[160];              /* motivo, quando não é TRUSTED */
} origin_t;

/* o kernel de UEFI numera as portas USB a partir de 0; o do mOS a partir de 1 */
#define USB_PORT_BIAS 1

bool origin_parse_devpath(const u8 *dp, usize len, devpath_t *out);
bool origin_phys_match(const diskphys_t *disk, const diskphys_t *want);
void origin_resolve(void);                 /* uma vez, depois dos drivers de armazenamento (antes do vfs_init) */
const origin_t *origin_get(void);
