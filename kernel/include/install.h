/* mOS — instalador (fs/install.c). Copia o PAYLOAD LIMPO do volume de origem (somente os arquivos listados no manifesto
 * gerado pelo build, cada um conferido por SHA-256) para um destino escolhido pelo usuário:
 *   INST_DISK:    disco inteiro (APAGA o disco: GPT nova). Com mos_bytes = 0, uma ESP FAT32 ocupa o disco todo; com
 *                 mos_bytes > 0 o disco é PARTICIONADO: a ESP "mOS" tem esse tamanho e o restante vira uma partição de
 *                 dados FAT32 "DADOS" (montada em /Volumes).
 *   INST_PART:    uma partição JÁ EXISTENTE, dedicada ao mOS (tipo ESP, vazia ou só com um mOS anterior): só o interior dela é
 *                 formatado; a tabela de partições, as demais partições e a NVRAM do firmware NÃO são alterados.
 *   INST_NEWPART: cria uma partição NOVA (tipo ESP) num espaço livre de uma GPT válida, com mos_bytes (0 = o espaço todo).
 *                 Só a entrada nova é acrescentada à tabela (cabeçalhos primário e de backup reescritos); as demais
 *                 partições não são tocadas nem movidas.
 * zero_fill ("formatar antes"): grava zeros em TODO o destino (o disco inteiro, ou só a partição) antes de criar o sistema
 * de arquivos. Sem ela a formatação é rápida: os dados antigos deixam de aparecer, mas não são sobrescritos.
 * Regras: origem confiável obrigatória (fs/origin.c); NADA é gravado antes da confirmação digitada exata; o alvo é
 * revalidado imediatamente antes da primeira escrita; a escrita só é liberada para as faixas do alvo (storage_grant_write); o
 * carregador EFI é o ÚLTIMO arquivo a aparecer no destino (cópia temporária + renomeação), então uma queda de energia no
 * meio nunca deixa um destino que boote como se estivesse completo. Contas e senhas NÃO são copiadas: são criadas no
 * primeiro boot do sistema instalado. Alvo incompatível NUNCA vira "apagar o disco". */
#pragma once
#include "storage.h"
#include "partscan.h"

enum { INST_DISK, INST_PART, INST_NEWPART };
enum {
    IR_OK, IR_NO_ORIGIN, IR_NO_PAYLOAD, IR_IS_ORIGIN, IR_RO, IR_LBS, IR_TOO_SMALL, IR_TOO_BIG, IR_BAD_TABLE, IR_NO_BACKUP,
    IR_NO_TABLE, IR_NOT_ESP, IR_NOT_DEDICATED, IR_BUSY, IR_NOT_FREE
};
int  install_reason_str(int ir);                              /* id de texto (i18n.h) que explica IR_* */

typedef struct { u64 total_bytes; int files; bool ok; char err[96]; } payload_info_t;
bool install_path_allowed(const char *p);                     /* política de conteúdo limpo do payload (mesma do mkimg) */
const payload_info_t *install_payload(void);                  /* lê e valida o manifesto (uma vez) */
u64  install_min_bytes(void);                                 /* menor partição de sistema aceita (payload + folga) */
#define INSTALL_DATA_MIN (64ull << 20)                        /* partição "DADOS": menor que isto não é criada */

int  install_check_disk(int disk, const ptable_t *t);         /* IR_OK se o disco pode ser apagado e receber a instalação */
int  install_check_part(int disk, const ptable_t *t, int idx);/* IR_OK se a partição t->p[idx] pode receber a instalação */
int  install_check_gap(int disk, const ptable_t *t, const gap_t *g);   /* IR_OK se cabe uma partição nova neste espaço livre */

typedef struct {
    int  mode, disk;
    int  part_number;            /* INST_PART */
    u64  mos_bytes;              /* INST_DISK: 0 = disco inteiro, >0 = particionar; INST_NEWPART: 0 = o espaço livre todo */
    u64  gap_first, gap_last;    /* INST_NEWPART: o espaço livre escolhido (LBAs inclusivos) */
    bool zero_fill;              /* formatar antes (zeros em todo o destino) */
} install_plan_t;
void install_confirm_phrase(const install_plan_t *pl, char *out, usize cap);   /* texto exato a digitar */

enum { INSTALL_IDLE, INSTALL_RUNNING, INSTALL_DONE, INSTALL_ERROR };
typedef struct {
    int  state, percent;
    char step[96], error[160];
    u64  bytes_done, bytes_total;
    int  files_done, files_total;
    bool wrote;                  /* já houve escrita no alvo (se false, o erro não alterou nada) */
    int  mode, disk, part_number;
    char target[64];
} install_status_t;

/* typed: o que o usuário digitou; snap: resumo da tabela quando ele escolheu o alvo (revalidação). Devolve false SEM
 * escrever nada se a confirmação não bate, a origem não é confiável ou o alvo mudou/ficou incompatível. */
bool install_begin(const install_plan_t *pl, const u8 snap[32], const char *typed, const char *lang_pref, const char *kbd_pref);
void install_poll(void);                                      /* uma fatia de trabalho; chame a cada quadro */
const install_status_t *install_status(void);
