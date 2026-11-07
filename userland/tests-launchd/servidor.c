/* mOS — teste dos serviços: servidor XPC iniciado sob demanda pelo launchd.
 * O mesmo binário serve como daemon (/Library/LaunchDaemons/com.mos.teste.xpc.plist, MachServices) e como XPC
 * service dentro do bundle do cliente (LaunchdTeste.app/Contents/XPCServices/com.mos.teste.bundle.xpc): nos dois casos
 * o launchd põe XPC_SERVICE_NAME e o xpc_main faz o check_in.
 * Mensagens: {op: "soma", a, b} -> {r: a+b, pid}; {op: "eco", s} -> {s}; {op: "sair"} -> termina (o launchd reinicia
 * na próxima mensagem). Ao subir, registra "com.mos.teste.ping" no notify e responde com um post de "...pong". */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include "svcteste.h"

static os_log_t lg;

static void peer_event(xpc_connection_t peer, xpc_object_t ev) {
    if (xpc_get_type(ev) != XPC_TYPE_DICTIONARY) return;          /* erro: o cliente foi embora */
    const char *op = xpc_dictionary_get_string(ev, "op");
    xpc_object_t reply = xpc_dictionary_create_reply(ev);
    if (op && !strcmp(op, "soma")) {
        int64_t a = xpc_dictionary_get_int64(ev, "a"), b = xpc_dictionary_get_int64(ev, "b");
        os_log(lg, "soma %lld + %lld pedida pelo pid %d", a, b, xpc_connection_get_pid(peer));
        if (reply) { xpc_dictionary_set_int64(reply, "r", a + b); xpc_dictionary_set_int64(reply, "pid", getpid()); }
    } else if (op && !strcmp(op, "eco")) {
        const char *s = xpc_dictionary_get_string(ev, "s");
        if (reply) xpc_dictionary_set_string(reply, "s", s ? s : "");
    } else if (op && !strcmp(op, "sair")) {
        os_log(lg, "servidor saindo a pedido (pid %d)", getpid());
        if (reply) { xpc_dictionary_set_bool(reply, "ok", true); xpc_connection_send_message(peer, reply); }
        usleep(200000);
        exit(0);
    }
    if (reply) xpc_connection_send_message(peer, reply);
}

static void new_peer(xpc_connection_t peer) {
    xpc_connection_set_event_handler(peer, ^(xpc_object_t ev) { peer_event(peer, ev); });
    xpc_connection_resume(peer);
}

int main(void) {
    const char *name = getenv("XPC_SERVICE_NAME");
    lg = os_log_create("com.mos.teste", "servidor");
    os_log(lg, "servidor XPC no ar: %{public}s (pid %d)", name ? name : "?", getpid());
    fprintf(stderr, "servidor: %s pid %d\n", name ? name : "?", getpid());
    int tok = 0;
    notify_register_dispatch("com.mos.teste.ping", &tok, dispatch_get_global_queue(0, 0), ^(int t) {
        (void)t;
        os_log(lg, "ping recebido pelo notify; respondendo pong");
        notify_post("com.mos.teste.pong");
    });
    xpc_main(new_peer);
    return 0;
}
