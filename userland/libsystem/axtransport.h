/* Transporte da árvore AX entre processos.
 *
 * O vtable é o de xpctransport.h (PR #49, bootstrap Mach, serviço com.mos.xpc.peer.<pid>).
 * cx_ax_bind_xpc grava ready/call/send/listen nessas funções. Este arquivo não abre
 * socket, porta Mach nem XPC. Se ready falha, ou se call devolve 60 (prazo) ou 61
 * (ninguém escutando), a leitura remota continua -25204. */
#pragma once

#define CX_AX_TRANSPORT_DEPENDENCY \
    "cx_xpc_peer_* do PR #49 (xpctransport.h, commit a6141d4, serviço com.mos.xpc.peer.<pid>, bootstrap Mach). " \
    "As assinaturas batem com este vtable e cx_ax_bind_xpc já liga xpc-mach. " \
    "Neste Linux o xpctest não chega a rodar: gcc recusa -fblocks; clang -fblocks compila userland/tests-xpc/main.c, " \
    "e o link de xpc.c para com referências indefinidas a bootstrap_check_in, bootstrap_look_up, bootstrap_port, " \
    "mach_msg, mach_port_allocate, objc_msgSend e _Block_copy. Não há libSystem.mos nem zig neste ambiente."

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
/* 0 segue. 60, 61 e qualquer outra falha do fio viram -25204. */
int cx_ax_wire_status(int rc);
void cx_ax_bind_xpc(void);
