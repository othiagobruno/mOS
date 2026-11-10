/* Transporte AX: vazio até um dos agentes de IPC chamar cx_ax_transport_bind.
 * Não há socket, porta Mach nem serviço XPC criado aqui. */
#include "axtransport.h"

static const cx_ax_transport *g_transport;
static void (*g_on_bind)(void);

void cx_ax_transport_bind(const cx_ax_transport *t) {
    g_transport = t;
    if (g_on_bind) g_on_bind();
}

void cx_ax_transport_on_bind(void (*fn)(void)) {
    g_on_bind = fn;
    if (fn && g_transport && g_transport->ready && g_transport->ready()) fn();
}

int cx_ax_transport_ready(void) {
    return g_transport && g_transport->ready && g_transport->ready() && g_transport->call ? 1 : 0;
}

const char *cx_ax_transport_dependency(void) { return CX_AX_TRANSPORT_DEPENDENCY; }

const cx_ax_transport *cx_ax_transport_get(void) { return g_transport; }

int cx_ax_wire_status(int rc) {
    return rc == 0 ? 0 : -25204;
}
