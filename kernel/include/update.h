/* mOS — atualização do sistema instalado sem reinstalar (fs/update.c).
 *
 * Um pacote de atualização é o MESMO formato do payload de instalação: o manifesto /mOS/Sistema/Instalacao/manifesto.txt
 * (SHA-256 de cada arquivo) e os arquivos nos mesmos caminhos. Serve tanto um pendrive/imagem do mOS montado em /Volumes
 * quanto um servidor HTTP com esse layout (tools/servir-atualizacao.ps1 entrega o build\esp do host).
 *
 * Regras:
 *   - o manifesto precisa estar assinado (linha ASSINATURA-ED25519) pela chave pública que o sistema instalado já tem em
 *     /Sistema/Biblioteca/chave-atualizacao.pub; sem chave ou com assinatura inválida, nada é baixado;
 *   - só os arquivos que mudaram são baixados (comparação com o manifesto instalado e com o conteúdo atual), cada um
 *     conferido por SHA-256 na chegada e de novo depois de gravado na área de trabalho /mOS/Sistema/Atualizacao (fora do VFS);
 *   - contas, preferências, pastas pessoais e logs nunca são tocados (a política do payload os exclui);
 *   - o sistema em uso não muda até o reinício: ao desligar/reiniciar, cada arquivo é TROCADO atomicamente (fat_replace),
 *     o carregador EFI por último. Um plano gravado antes da 1ª troca torna a aplicação retomável: se a energia cair no meio, o
 *     próximo boot termina o serviço antes de carregar a árvore (e reinicia se o carregador acabou de ser trocado).
 */
#pragma once
#include "kernel.h"
#include "fs.h"

enum { UPD_IDLE, UPD_CHECKING, UPD_UPTODATE, UPD_AVAILABLE, UPD_DOWNLOADING, UPD_READY, UPD_ERROR };

typedef struct {
    int  state;
    char source[256];
    char step[160], error[192];
    char cur_version[80], new_version[80];
    int  files_total;            /* no manifesto novo */
    int  files_changed, files_removed, files_done;
    bool loader_changed;         /* o kernel (carregador EFI) muda */
    u64  bytes_changed, bytes_done;
    int  percent;
} update_status_t;

bool update_supported(char *why, usize cap);    /* sistema instalado (ou live persistente) com gravação liberada */
bool update_check(const char *source);          /* "http://host[:porta][/base]" ou uma pasta do VFS ("/Volumes/MOS") */
bool update_download(void);                     /* depois de UPD_AVAILABLE */
void update_cancel(void);                       /* interrompe e apaga o que já foi baixado */
void update_poll(void);                         /* uma fatia de trabalho; chamado no loop principal */
const update_status_t *update_status(void);
bool update_ready(void);                        /* pacote completo à espera do reinício */
const char *update_current_version(void);       /* versão do manifesto instalado (ou MOS_VERSION) */
const char *update_last_applied(void);          /* versão aplicada no último reinício ("" se nenhuma) */

void update_on_shutdown(void);                  /* desktop.c: aplica o pacote pronto antes de desligar/reiniciar */
bool update_boot_pending(fatfs_t *sys);         /* persist.c: há plano a aplicar/retomar? (só lê) */
bool update_boot_apply(fatfs_t *sys);           /* persist.c: aplica; true = o carregador foi trocado agora (reiniciar) */
