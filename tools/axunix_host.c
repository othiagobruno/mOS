/* Dois processos no AF_UNIX de /tmp/mos-ax/<pid>: o filho escuta, o pai pede o role.
 * gcc -std=c11 -Wall -Wextra -Iuserland/libsystem \
 *   userland/libsystem/axproto.c userland/libsystem/axtransport.c userland/libsystem/axunix.c \
 *   tools/axunix_host.c -o /tmp/axunix-host && /tmp/axunix-host
 */
#define _GNU_SOURCE
#include "axproto.h"
#include "axtransport.h"
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>

static int got_role;

static void on_req(int from, const void *bytes, int n, void *ctx) {
    ax_msg in, out;
    unsigned char rep[640];
    int rn;
    const cx_ax_transport *t;
    (void)ctx;
    memset(&out, 0, sizeof out);
    if (ax_msg_decode(bytes, n, &in) != 0) return;
    out.op = AX_OP_REPLY;
    out.pid = in.pid;
    out.kind = in.kind;
    out.id = in.id;
    out.pay = AX_PAY_STR;
    out.status = 0;
    snprintf(out.str, sizeof out.str, "AXButton");
    rn = ax_msg_encode(&out, rep, (int)sizeof rep);
    t = cx_ax_transport_get();
    if (rn > 0 && t && t->send) t->send(from, rep, rn);
}

int main(void) {
    int pipefd[2];
    pid_t child;
    if (pipe(pipefd) != 0) return 1;
    child = fork();
    if (child < 0) return 1;
    if (child == 0) {
        const cx_ax_transport *t;
        close(pipefd[0]);
        cx_ax_unix_bind();
        t = cx_ax_transport_get();
        if (!t || t->listen(on_req, 0) != 0) _exit(2);
        if (write(pipefd[1], "1", 1) != 1) _exit(3);
        for (;;) pause();
    }
    close(pipefd[1]);
    {
        char go = 0;
        if (read(pipefd[0], &go, 1) != 1) return 4;
    }
    {
        const cx_ax_transport *t;
        ax_msg req;
        unsigned char raw[640], reply[640];
        int n, rn = 0, rc;
        ax_msg out;
        cx_ax_unix_bind();
        t = cx_ax_transport_get();
        memset(&req, 0, sizeof req);
        req.op = AX_OP_COPY;
        req.pid = (int)child;
        req.kind = 3;
        req.id = 7;
        snprintf(req.name, sizeof req.name, "AXRole");
        n = ax_msg_encode(&req, raw, (int)sizeof raw);
        rc = t->call((int)child, raw, n, reply, (int)sizeof reply, &rn);
        if (rc != 0) {
            fprintf(stderr, "call %d\n", rc);
            return 5;
        }
        if (ax_msg_decode(reply, rn, &out) != 0) return 6;
        if (out.op != AX_OP_REPLY || strcmp(out.str, "AXButton") != 0) return 7;
        got_role = 1;
    }
    if (!got_role) return 8;
    kill(child, 9);
    printf("axunix host ok\n");
    return 0;
}
