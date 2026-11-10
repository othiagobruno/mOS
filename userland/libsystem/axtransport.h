/* Transporte da árvore AX entre processos.
 *
 * Este lado não abre socket, porta Mach nem XPC. Quem implementa call/send/listen é
 * SYS-XPC-MACH (bc-1a126ada, Mach/XPC) ou KER-UNIX-IPC (bc-4bbc4f8a, AF_UNIX).
 * Enquanto nenhum dos dois publicar a API, cx_ax_transport_ready() é 0 e a leitura
 * remota continua -25204. O servidor da árvore (axserver.c) já responde no processo
 * alvo assim que alguém chamar cx_ax_transport_bind. */
#pragma once

#define CX_AX_TRANSPORT_DEPENDENCY \
    "SYS-XPC-MACH (bc-1a126ada, Mach/XPC) e KER-UNIX-IPC (bc-4bbc4f8a, AF_UNIX). " \
    "git fetch origin e gh pr list em 2026-11-10: nenhum PR aberto e nenhuma branch com essa API. " \
    "Os agentes bc-1a126ada e bc-4bbc4f8a não estão acessíveis neste ambiente."

typedef struct cx_ax_transport {
    const char *name;
    int (*ready)(void);
    /* pedido e resposta. 0 = ok e *out_n preenchido. */
    int (*call)(int pid, const void *req, int n, void *reply, int cap, int *out_n);
    /* aviso sem espera (notificação). 0 = aceito. */
    int (*send)(int pid, const void *bytes, int n);
    int (*listen)(void (*fn)(int from_pid, const void *bytes, int n, void *ctx), void *ctx);
} cx_ax_transport;

void cx_ax_transport_bind(const cx_ax_transport *t);
void cx_ax_transport_on_bind(void (*fn)(void));
int cx_ax_transport_ready(void);
const char *cx_ax_transport_dependency(void);
const cx_ax_transport *cx_ax_transport_get(void);
