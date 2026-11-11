/* Teste de host do AX entre processos (userland/libsystem/axremote.c + axunix.c reais, dois processos).
 *
 * O filho faz o papel do app: escuta no AF_UNIX (/tmp/mos-ax/<pid>) e responde com as páginas de ax_page_kids /
 * ax_page_str / ax_page_single, as mesmas que o axserver.c usa. O pai é o agente de computer use:
 *   1. o fio vinculado (um "xpc-mach" que devolve 61, como quando o outro lado não fez check-in Mach) falha e o
 *      pedido segue pela reserva AF_UNIX, em vez de virar -25204;
 *   2. AXChildren com 40 filhos chega inteiro (um ax_msg leva 12);
 *   3. AXValue com 500 bytes chega inteiro (um ax_msg leva 95);
 *   4. AXFocusedUIElement chega como um elemento só (loc = -1);
 *   5. servidor antigo (len 0) vale como página única, sem laço;
 *   6. duas threads pedindo ao mesmo tempo recebem cada uma a sua resposta (o g_wait do af-unix é um só);
 *   7. a janela sob um ponto diz o pid dono (AXUIElementCopyElementAtPosition do system-wide).
 *
 *   gcc -std=gnu11 -Wall -Wextra -pthread -Iuserland/libsystem userland/libsystem/axproto.c \
 *       userland/libsystem/axtransport.c userland/libsystem/axunix.c userland/libsystem/axremote.c \
 *       tools/axremote_host.c -o /tmp/axremote-host && /tmp/axremote-host
 */
#define _GNU_SOURCE
#include "axremote.h"
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

enum { KIDS = 40, FOCUS_ID = 4242 };
static char g_value[501];

static int ok, total;
static void check(int cond, const char *what) {
    total++;
    if (cond) ok++;
    printf("%s %s\n", cond ? "  ok  " : "  FALHA", what);
}

/* ---------------------------------------------------------------- o app (filho) */
static void serve(int from, const void *bytes, int n, void *ctx) {
    ax_msg in, out;
    unsigned char rep[640];
    const cx_ax_transport *t = ctx;
    int kinds[KIDS], ids[KIDS];
    if (ax_msg_decode(bytes, n, &in) != 0) return;
    memset(&out, 0, sizeof out);
    out.op = AX_OP_REPLY;
    out.pid = in.pid;
    out.kind = in.kind;
    out.id = in.id;
    if (!strcmp(in.name, "AXChildren")) {
        for (int i = 0; i < KIDS; i++) { kinds[i] = 3; ids[i] = 1000 + i; }
        ax_page_kids(&out, kinds, ids, KIDS, in.loc);
    } else if (!strcmp(in.name, "AXValue")) {
        ax_page_str(&out, g_value, in.loc);
    } else if (!strcmp(in.name, "AXFocusedUIElement")) {
        ax_page_single(&out, 3, FOCUS_ID);
    } else if (!strcmp(in.name, "AXVelho")) {        /* servidor de antes das páginas: 12 filhos, len 0 */
        out.pay = AX_PAY_KIDS;
        out.nkids = AX_KIDS_MAX;
        for (int i = 0; i < AX_KIDS_MAX; i++) { out.kid_kind[i] = 3; out.kid_id[i] = i; }
    } else if (!strcmp(in.name, "AXTitle")) {
        out.pay = AX_PAY_STR;
        snprintf(out.str, sizeof out.str, "titulo-%d", in.id);
    } else {
        out.status = -25205;
    }
    int rn = ax_msg_encode(&out, rep, (int)sizeof rep);
    if (rn > 0) t->send(from, rep, rn);
}

/* ---------------------------------------------------------------- o "xpc-mach" sem o outro lado */
static int dead_ready(void) { return 1; }
static int dead_calls;
static int dead_call(int pid, const void *req, int n, void *reply, int cap, int *out_n) {
    (void)pid; (void)req; (void)n; (void)reply; (void)cap;
    if (out_n) *out_n = 0;
    dead_calls++;
    return 61;                                     /* ninguém escutando em com.mos.xpc.peer.<pid> */
}
static int dead_send(int pid, const void *b, int n) { (void)pid; (void)b; (void)n; return 61; }
static int dead_listen(void (*fn)(int, const void *, int, void *), void *ctx) { (void)fn; (void)ctx; return 61; }
static const cx_ax_transport g_dead = {"xpc-mach (sem par)", dead_ready, dead_call, dead_send, dead_listen};

/* ---------------------------------------------------------------- janelas do compositor (frente -> fundo) */
static const ax_win_t g_wins[] = {
    {7, 0, 0, 1, 0, 0, 1440, 24},          /* barra de menus do sistema: pid 0, não é de app */
    {11, 900, 0, 0, 100, 100, 400, 300},   /* fora da tela */
    {12, 500, 0, 1, 100, 100, 400, 300},   /* frente */
    {13, 600, 0, 1, 50, 50, 800, 600},     /* atrás, maior */
};
static int win_at(int i, ax_win_t *o) {
    if (i < 0 || i >= (int)(sizeof g_wins / sizeof g_wins[0])) return 0;
    *o = g_wins[i];
    return 1;
}

/* ---------------------------------------------------------------- duas threads ao mesmo tempo */
static int g_app;
static int g_thread_ok[2];
static void *asker(void *arg) {
    int me = (int)(long)arg, good = 0;
    for (int i = 0; i < 50; i++) {
        ax_msg q, r;
        memset(&q, 0, sizeof q);
        q.op = AX_OP_COPY;
        q.kind = 3;
        q.id = me * 1000 + i;
        snprintf(q.name, sizeof q.name, "AXTitle");
        char want[32];
        snprintf(want, sizeof want, "titulo-%d", q.id);
        if (cx_ax_call(g_app, &q, &r) == 0 && !strcmp(r.str, want)) good++;
    }
    g_thread_ok[me] = good;
    return 0;
}

int main(void) {
    int sync[2];
    for (int i = 0; i < 500; i++) g_value[i] = (char)('a' + i % 26);
    g_value[500] = 0;
    if (pipe(sync) != 0) return 1;
    pid_t app = fork();
    if (app == 0) {
        const cx_ax_transport *u = cx_ax_unix_transport();
        close(sync[0]);
        if (u->listen(serve, (void *)u) != 0) _exit(2);
        if (write(sync[1], "1", 1) != 1) _exit(3);
        for (;;) pause();
    }
    close(sync[1]);
    char c;
    if (read(sync[0], &c, 1) != 1) { printf("o app não subiu\n"); return 1; }
    g_app = (int)app;

    printf("1. reserva do transporte\n");
    cx_ax_transport_bind(&g_dead);
    ax_msg q, r;
    memset(&q, 0, sizeof q);
    q.op = AX_OP_COPY; q.kind = 3; q.id = 5;
    snprintf(q.name, sizeof q.name, "AXTitle");
    check(cx_ax_call(g_app, &q, &r) == -25204, "sem reserva, o fio morto dá -25204 (o defeito de antes)");
    cx_ax_transport_set_fallback(cx_ax_unix_transport());
    check(cx_ax_call(g_app, &q, &r) == 0 && !strcmp(r.str, "titulo-5"), "com a reserva AF_UNIX o pedido chega e volta");
    check(dead_calls >= 2, "o fio vinculado foi tentado primeiro");

    printf("2. lista maior que um ax_msg\n");
    int kinds[AX_PAGE_KIDS_MAX], ids[AX_PAGE_KIDS_MAX], n = 0, seq = 1;
    snprintf(q.name, sizeof q.name, "AXChildren");
    check(cx_ax_call_kids(g_app, &q, kinds, ids, AX_PAGE_KIDS_MAX, &n, 0) == 0 && n == KIDS, "AXChildren traz os 40 filhos");
    for (int i = 0; i < n; i++) if (ids[i] != 1000 + i || kinds[i] != 3) seq = 0;
    check(seq, "na ordem, sem repetir página");
    check(cx_ax_call_kids(g_app, &q, kinds, ids, 25, &n, 0) == 0 && n == 25, "o teto de quem chama é respeitado");

    printf("3. texto maior que um ax_msg\n");
    char buf[4096];
    snprintf(q.name, sizeof q.name, "AXValue");
    check(cx_ax_call_str(g_app, &q, buf, sizeof buf, 0) == 0 && strlen(buf) == 500 && !strcmp(buf, g_value), "AXValue de 500 bytes chega inteiro");
    check(cx_ax_call_str(g_app, &q, buf, 101, 0) == 0 && strlen(buf) == 100 && !strncmp(buf, g_value, 100), "buffer menor: corta no tamanho de quem chama, terminado em 0");

    printf("4. elemento único\n");
    ax_msg first;
    snprintf(q.name, sizeof q.name, "AXFocusedUIElement");
    check(cx_ax_call_kids(g_app, &q, kinds, ids, 8, &n, &first) == 0 && n == 1 && ids[0] == FOCUS_ID && first.loc == AX_SINGLE,
          "AXFocusedUIElement vem como um elemento (loc = -1)");

    printf("5. servidor sem páginas\n");
    snprintf(q.name, sizeof q.name, "AXVelho");
    check(cx_ax_call_kids(g_app, &q, kinds, ids, AX_PAGE_KIDS_MAX, &n, 0) == 0 && n == AX_KIDS_MAX, "len 0 vale como página única (12 filhos, sem laço)");
    snprintf(q.name, sizeof q.name, "AXNada");
    check(cx_ax_call_kids(g_app, &q, kinds, ids, 8, &n, 0) == -25205, "o status do servidor volta a quem chamou");

    printf("6. duas threads\n");
    pthread_t t0, t1;
    pthread_create(&t0, 0, asker, (void *)0L);
    pthread_create(&t1, 0, asker, (void *)1L);
    pthread_join(t0, 0);
    pthread_join(t1, 0);
    check(g_thread_ok[0] == 50 && g_thread_ok[1] == 50, "100 pedidos de duas threads, cada resposta para quem pediu");

    printf("7. dono da janela num ponto\n");
    int pid = 0;
    unsigned wid = 0;
    check(ax_window_owner_at(win_at, 200, 200, &pid, &wid) && pid == 500 && wid == 12, "o ponto na janela da frente é do pid 500");
    check(ax_window_owner_at(win_at, 60, 60, &pid, &wid) && pid == 600 && wid == 13, "fora da da frente, a de trás (pid 600)");
    check(!ax_window_owner_at(win_at, 5, 5, &pid, &wid), "barra do sistema (pid 0) e área vazia não têm dono");

    kill(app, SIGKILL);
    waitpid(app, 0, 0);
    printf("%d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
