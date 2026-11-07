/* mOS — leitura e validação de tabelas de partição (GPT e MBR) e identificação do conteúdo de cada partição.
 * Só LEITURA: nada aqui grava em disco. Setor lógico de 512 bytes (os chamadores recusam discos 4Kn). */
#pragma once
#include "fs.h"

enum { PT_NONE, PT_GPT, PT_MBR, PT_BAD };
enum { FSK_UNKNOWN, FSK_EMPTY, FSK_FAT, FSK_NTFS, FSK_EXFAT, FSK_EXT, FSK_HFS, FSK_APFS, FSK_BITLOCKER, FSK_LUKS, FSK_REFS, FSK_SWAP };
#define PT_MAX_PARTS 128

typedef struct {
    int  number;                 /* 1..n, como nos nós HD() do caminho UEFI */
    u64  first, last;            /* LBAs inclusivos */
    u8   type_guid[16];          /* GPT */
    u8   uniq[16];               /* GPT: GUID único da partição */
    u8   mbr_type;               /* MBR */
    char name[40];
    int  fs;                     /* FSK_* */
    bool esp;                    /* GPT: tipo ESP; MBR: tipo 0xEF */
} part_t;

typedef struct {
    int    kind;                 /* PT_* */
    int    n;
    part_t p[PT_MAX_PARTS];
    u8     disk_guid[16];
    u32    mbr_sig;
    bool   backup_ok;            /* GPT: cabeçalho de backup válido, consistente e no fim do disco */
    bool   has_extended;         /* MBR com partição estendida (partições lógicas NÃO são listadas) */
    bool   whole_volume;         /* PT_NONE: o disco inteiro parece um volume (sem tabela) */
    int    whole_fs;
    char   err[112];             /* PT_BAD: o motivo */
    u8     snap[32];             /* SHA-256 das áreas de metadados (setor 0, cabeçalhos e entradas): revalidação */
    u64    total_sectors;
    u64    first_usable, last_usable;   /* GPT: limites usáveis do cabeçalho primário */
} ptable_t;

/* espaço livre (só GPT) entre as partições, alinhado a 1 MiB, com pelo menos min_sectors setores. Devolve quantos achou. */
typedef struct { u64 first, last; } gap_t;   /* LBAs inclusivos */
int         part_free_gaps(const ptable_t *t, gap_t *out, int max, u64 min_sectors);

/* false só em erro de leitura. Tabela inconsistente => true com kind == PT_BAD e err preenchido. */
bool        partscan(blkdev_t *disk, ptable_t *t);
int         part_sniff(blkdev_t *disk, u64 off, u64 size);      /* FSK_*: FSK_EMPTY = 1 MiB inicial todo zero */
const char *part_fs_name(int fsk);
const char *part_type_name(const part_t *p);
bool        guid_is_esp(const u8 *g);
void        guid_to_str(const u8 *g, char *out);                /* 36 caracteres + NUL, formato textual do GPT */
