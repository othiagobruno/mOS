/* Dois processos de app e um de teste, no mesmo modelo de memória compartilhada.
 * Fundo fica atrás. Frente fica em foco. O pai observa.
 *   CGEventPostToPid é aplicado pelo processo do pid e não muda o foco.
 *   AXValueChanged desse processo chega no observador.
 *   A árvore (role, title, children) e AXStringForRange atravessam o ax_msg.
 * Um terceiro fork usa o vtable de axtransport.h num par de sockets: call no pai,
 * listen/send no filho. Isso não é o bootstrap Mach. cx_xpc_peer_call continua
 * PENDENTE neste host: o link de xpc.c não fecha sem o kernel.
 *
 *   gcc -std=c11 -Wall -Wextra -Iuserland/tests-appkit -Iuserland/libsystem \
 *       userland/libsystem/axproto.c userland/libsystem/axtransport.c \
 *       userland/tests-appkit/computeruse_core.c \
 *       userland/tests-appkit/computeruse_remote.c -o /tmp/computer-use-remote \
 *       && /tmp/computer-use-remote
 */
#define _GNU_SOURCE
#include "computeruse_core.h"
#include "axproto.h"
#include "axtransport.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/wait.h>

enum { PID_FUNDO = 41, PID_FRENTE = 42 };

static int ok, total, pending;
static cu_world *G;

static void check(int cond, const char *what) {
    total++;
    if (cond) ok++;
    printf("%s %s\n", cond ? "  ok  " : "  FALHA", what);
}

static int wait_post(int seq) {
    for (int i = 0; i < 2000; i++) {
        if (cu_post_seq(G) == seq + 2) return 1;
        usleep(1000);
    }
    return 0;
}

static int wait_ax(int seq, unsigned char *rep, int cap, int *rn) {
    for (int i = 0; i < 2000; i++) {
        if (cu_ax_seq(G) == seq + 2) {
            *rn = cu_ax_reply(G, rep, cap);
            return 1;
        }
        usleep(1000);
    }
    return 0;
}

static void serve(cu_world *w, const unsigned char *req, int n, unsigned char *rep, int *rn) {
    ax_msg in, out;
    cu_attr attr;
    memset(&out, 0, sizeof out);
    out.op = AX_OP_REPLY;
    if (ax_msg_decode(req, n, &in) != 0) {
        out.status = -25201;
        *rn = ax_msg_encode(&out, rep, 640);
        return;
    }
    out.pid = in.pid;
    out.kind = in.kind;
    out.id = in.id;
    if (in.op == AX_OP_COPY) {
        int st = cu_ax_copy(w, in.kind, in.id, in.name, &attr);
        out.status = st;
        if (!st && attr.type == CU_ATTR_STR) {
            out.pay = AX_PAY_STR;
            snprintf(out.str, sizeof out.str, "%s", attr.str);
        } else if (!st && attr.type == CU_ATTR_KIDS) {
            out.pay = AX_PAY_KIDS;
            out.nkids = attr.nkids;
            for (int i = 0; i < attr.nkids && i < AX_KIDS_MAX; i++) {
                out.kid_kind[i] = attr.kid_kind[i];
                out.kid_id[i] = attr.kid_id[i];
            }
        }
    } else if (in.op == AX_OP_PARAM && !strcmp(in.name, "AXStringForRange")) {
        int st = cu_ax_copy(w, in.kind, in.id, "AXValue", &attr);
        int loc = in.loc, len = in.len, nch;
        out.status = st ? st : 0;
        if (!st && attr.type == CU_ATTR_STR) {
            nch = (int)strlen(attr.str);
            if (loc < 0) loc = 0;
            if (loc > nch) loc = nch;
            if (len < 0) len = 0;
            if (loc + len > nch) len = nch - loc;
            if (len >= AX_STR_MAX) len = AX_STR_MAX - 1;
            out.pay = AX_PAY_STR;
            memcpy(out.str, attr.str + loc, (unsigned long)len);
            out.str[len] = 0;
        }
    } else out.status = -25201;
    *rn = ax_msg_encode(&out, rep, 640);
}

static void child_main(int pid) {
    while (!cu_load(G, CU_DONE)) {
        unsigned char req[640], rep[640];
        int n, rn = 0;
        cu_post_poll(G, pid);
        n = cu_ax_poll(G, pid, req, (int)sizeof req);
        if (n > 0) {
            serve(G, req, n, rep, &rn);
            cu_ax_finish(G, rep, rn);
        }
        usleep(1000);
    }
    _exit(0);
}

static int g_pipe = -1;

static int io_full(int fd, void *buf, int n, int wr) {
    int off = 0;
    char *p = buf;
    while (off < n) {
        int k = wr ? (int)write(fd, p + off, (size_t)(n - off)) : (int)read(fd, p + off, (size_t)(n - off));
        if (k <= 0) return -1;
        off += k;
    }
    return 0;
}

static int pipe_ready(void) { return g_pipe >= 0; }

static int pipe_send(int pid, const void *bytes, int n) {
    int len = n;
    (void)pid;
    if (n < 0 || (n && !bytes)) return 22;
    if (io_full(g_pipe, &len, 4, 1) || (n && io_full(g_pipe, (void *)bytes, n, 1))) return 61;
    return 0;
}

static int pipe_call(int pid, const void *req, int n, void *reply, int cap, int *out_n) {
    int len = n, rn = 0;
    (void)pid;
    if (out_n) *out_n = 0;
    if (n < 0 || cap < 0 || (n && !req)) return 22;
    if (io_full(g_pipe, &len, 4, 1) || (n && io_full(g_pipe, (void *)req, n, 1))) return 61;
    if (io_full(g_pipe, &rn, 4, 0)) return 60;
    if (rn < 0 || rn > cap) return 22;
    if (rn && io_full(g_pipe, reply, rn, 0)) return 60;
    if (out_n) *out_n = rn;
    return 0;
}

static int pipe_listen(void (*fn)(int from_pid, const void *bytes, int n, void *ctx), void *ctx) {
    for (;;) {
        int n = 0;
        unsigned char buf[640];
        if (io_full(g_pipe, &n, 4, 0)) return 0;
        if (n <= 0 || n > (int)sizeof buf) return 22;
        if (io_full(g_pipe, buf, n, 0)) return 61;
        if (fn) fn(0, buf, n, ctx);
    }
}

static int ready_off(void) { return 0; }
static int call_61(int pid, const void *req, int n, void *reply, int cap, int *out_n) {
    (void)pid; (void)req; (void)n; (void)reply; (void)cap;
    if (out_n) *out_n = 0;
    return 61;
}

static void on_ax_pipe(int from, const void *bytes, int n, void *ctx) {
    unsigned char rep[640];
    int rn = 0;
    (void)from;
    (void)ctx;
    serve(G, bytes, n, rep, &rn);
    if (rn > 0) pipe_send(from, rep, rn);
}

static int vtable_copy(int pid, int kind, int id, const char *name, ax_msg *out) {
    const cx_ax_transport *t = cx_ax_transport_get();
    ax_msg req;
    unsigned char buf[640], rep[640];
    int n, rn = 0, rc;
    if (!t || !t->call || !out) return 0;
    memset(&req, 0, sizeof req);
    req.op = AX_OP_COPY;
    req.pid = pid;
    req.kind = kind;
    req.id = id;
    snprintf(req.name, sizeof req.name, "%s", name);
    n = ax_msg_encode(&req, buf, (int)sizeof buf);
    if (n < 0) return 0;
    rc = t->call(pid, buf, n, rep, (int)sizeof rep, &rn);
    if (cx_ax_wire_status(rc)) return 0;
    return ax_msg_decode(rep, rn, out) == 0;
}

static int ax_call(int pid, const ax_msg *req, ax_msg *out) {
    unsigned char buf[640], rep[640];
    int n = ax_msg_encode(req, buf, (int)sizeof buf);
    int seq, rn = 0;
    if (n < 0) return 0;
    seq = cu_ax_begin(G, pid, buf, n);
    if (seq < 0 || !wait_ax(seq, rep, (int)sizeof rep, &rn)) return 0;
    return ax_msg_decode(rep, rn, out) == 0;
}

int main(void) {
    int fundo_win, frente_win, fundo_btn, frente_btn, fundo_field;
    int note0, seq, front_scroll, back_scroll;
    cu_info info[4];
    int ninfo, saw_fundo = 0, saw_frente = 0, apps = 0;
    pid_t a, b;
    ax_msg req, round, reply;
    unsigned char raw[640];
    int raw_n;

    G = cu_world_bytes(mmap(0, cu_world_bytes_size(), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0));
    if (G == (void *)-1) return 1;
    cu_init(G);
    cu_grant(G, "kTCCServiceScreenCapture");
    cu_grant(G, "kTCCServiceAccessibility");

    fundo_win = cu_open_window(G, PID_FUNDO, "Fundo", "Janela Fundo", 20, 20, 200, 120, 0xFF224466u);
    fundo_btn = cu_add_button(G, fundo_win, "Ok", 10, 10, 40, 20, 0xFF3388FFu);
    fundo_field = cu_add_field(G, fundo_win, 10, 40, 120, 24);
    frente_win = cu_open_window(G, PID_FRENTE, "Frente", "Janela Frente", 260, 20, 200, 120, 0xFF446622u);
    frente_btn = cu_add_button(G, frente_win, "Go", 10, 10, 40, 20, 0xFF88CC44u);
    cu_add_field(G, frente_win, 10, 40, 120, 24);
    cu_activate(G, PID_FRENTE);

    a = fork();
    if (a == 0) child_main(PID_FUNDO);
    b = fork();
    if (b == 0) child_main(PID_FRENTE);

    ninfo = cu_list(G, info, 4);
    check(ninfo == 2, "lista tem as duas janelas");
    for (int i = 0; i < ninfo; i++) {
        if (info[i].pid == PID_FUNDO && info[i].x == 20 && info[i].y == 20 && info[i].w == 200 && info[i].h == 120 && !strcmp(info[i].name, "Janela Fundo"))
            saw_fundo = 1;
        if (info[i].pid == PID_FRENTE && info[i].x == 260 && info[i].w == 200 && !strcmp(info[i].name, "Janela Frente"))
            saw_frente = 1;
    }
    check(saw_fundo && saw_frente, "cada janela tem pid, bounds e title");

    for (int i = 0; cu_running(G, i, 0, 0, 0); i++) apps++;
    check(apps == 2, "runningApplications lista os dois processos");
    check(cu_front_pid(G) == PID_FRENTE, "frontmostApplication é a Frente");

    cu_post_mouse(G, 0, 5, 300, 40);
    cu_post_scroll(G, 0, 3);
    check(cu_win_scroll(G, frente_win) == 3 && cu_win_scroll(G, fundo_win) == 0, "rolagem global cai na Frente");

    seq = cu_post_begin(G, PID_FUNDO, 22, 40, 40, 5);
    check(seq >= 0 && wait_post(seq), "o processo Fundo aplica o PostToPid");
    check(cu_win_scroll(G, fundo_win) == 5 && cu_win_scroll(G, frente_win) == 3, "a rolagem do pid foi para o Fundo");
    check(cu_front_pid(G) == PID_FRENTE, "PostToPid não mudou o foco");

    seq = cu_post_begin(G, PID_FUNDO, 1, 30, 30, 0);
    check(seq >= 0 && wait_post(seq), "o clique do pid chega no Fundo");
    check(cu_ctl_clicks(G, fundo_btn) == 1 && cu_ctl_clicks(G, frente_btn) == 0, "o botão da Frente não recebeu o clique");
    check(cu_front_pid(G) == PID_FRENTE, "o clique no app de fundo não roubou o foco");

    note0 = cu_note_seq(G);
    seq = cu_post_begin(G, PID_FUNDO, 10, 0, 0, 'A');
    check(seq >= 0 && wait_post(seq), "a tecla do pid chega no campo do Fundo");
    {
        char value[80], note[48];
        int np = 0, nk = 0, nid = 0;
        cu_read_value(G, fundo_field, value, 80);
        cu_note_read(G, &np, &nk, &nid, note, 48);
        check(!strcmp(value, "A"), "o campo de fundo ficou com A");
        check(cu_note_seq(G) == note0 + 1 && np == PID_FUNDO && nid == fundo_field && !strcmp(note, "AXValueChanged"),
              "AXObserver recebe AXValueChanged do outro processo");
    }

    memset(&req, 0, sizeof req);
    req.op = AX_OP_COPY;
    req.pid = PID_FUNDO;
    req.kind = CU_KIND_WIN;
    req.id = fundo_win;
    snprintf(req.name, sizeof req.name, "AXRole");
    check(ax_call(PID_FUNDO, &req, &reply) && reply.status == 0 && reply.pay == AX_PAY_STR && !strcmp(reply.str, "AXWindow"),
          "AX remoto devolve o role");
    snprintf(req.name, sizeof req.name, "AXTitle");
    check(ax_call(PID_FUNDO, &req, &reply) && !strcmp(reply.str, "Janela Fundo"), "AX remoto devolve o title");
    snprintf(req.name, sizeof req.name, "AXChildren");
    check(ax_call(PID_FUNDO, &req, &reply) && reply.pay == AX_PAY_KIDS && reply.nkids == 2, "AX remoto devolve os children");

    memset(&req, 0, sizeof req);
    req.op = AX_OP_PARAM;
    req.pid = PID_FUNDO;
    req.kind = CU_KIND_CTL;
    req.id = fundo_field;
    req.loc = 0;
    req.len = 1;
    snprintf(req.name, sizeof req.name, "AXStringForRange");
    check(ax_call(PID_FUNDO, &req, &reply) && reply.pay == AX_PAY_STR && !strcmp(reply.str, "A"),
          "AXStringForRange devolve o trecho do texto");

    memset(&round, 0, sizeof round);
    round.op = AX_OP_COPY;
    round.pid = PID_FUNDO;
    round.kind = CU_KIND_WIN;
    round.id = fundo_win;
    snprintf(round.name, sizeof round.name, "AXTitle");
    raw_n = ax_msg_encode(&round, raw, (int)sizeof raw);
    memset(&reply, 0, sizeof reply);
    check(raw_n > 16 && ax_msg_decode(raw, raw_n, &reply) == 0 && reply.id == fundo_win && !strcmp(reply.name, "AXTitle"),
          "o ax_msg sobrevive a um encode/decode");

    front_scroll = cu_win_scroll(G, frente_win);
    back_scroll = cu_win_scroll(G, fundo_win);
    check(front_scroll == 3 && back_scroll == 5 && cu_front_pid(G) == PID_FRENTE, "foco e filas continuam separados");

    check(cx_ax_wire_status(0) == 0 && cx_ax_wire_status(60) == -25204 && cx_ax_wire_status(61) == -25204,
          "60 e 61 do transporte viram -25204");
    {
        cx_ax_transport down = {"xpc-mach", ready_off, call_61, 0, 0};
        cx_ax_transport_bind(&down);
        check(!cx_ax_transport_ready(), "ready falho deixa o AX remoto fechado");
    }
    {
        int sv[2];
        pid_t c;
        ax_msg got;
        int opened = socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0;
        c = opened ? fork() : -1;
        if (c == 0) {
            g_pipe = sv[1];
            close(sv[0]);
            pipe_listen(on_ax_pipe, 0);
            _exit(0);
        }
        if (c > 0) {
            g_pipe = sv[0];
            close(sv[1]);
        }
        {
            cx_ax_transport up = {"xpc-mach", pipe_ready, pipe_call, pipe_send, pipe_listen};
            if (c > 0) cx_ax_transport_bind(&up);
        }
        check(c > 0 && cx_ax_transport_ready() && vtable_copy(PID_FUNDO, CU_KIND_WIN, fundo_win, "AXRole", &got) &&
                  got.status == 0 && got.pay == AX_PAY_STR && !strcmp(got.str, "AXWindow"),
              "fork + call/listen devolve o role");
        check(c > 0 && vtable_copy(PID_FUNDO, CU_KIND_WIN, fundo_win, "AXTitle", &got) && !strcmp(got.str, "Janela Fundo"),
              "fork + call/listen devolve o title");
        check(c > 0 && vtable_copy(PID_FUNDO, CU_KIND_WIN, fundo_win, "AXChildren", &got) && got.pay == AX_PAY_KIDS && got.nkids == 2,
              "fork + call/listen devolve os children");
        if (g_pipe >= 0) close(g_pipe);
        g_pipe = -1;
        if (c > 0) waitpid(c, 0, 0);
    }

    pending++;
    printf("  PENDENTE  cx_xpc_peer_call no Linux: %s\n", CX_AX_TRANSPORT_DEPENDENCY);

    cu_store(G, CU_DONE, 1);
    waitpid(a, 0, 0);
    waitpid(b, 0, 0);
    printf("computer-use remoto: %d/%d ok, %d pendente\n", ok, total, pending);
    return ok == total ? 0 : 1;
}
