/* Dois processos no AF_UNIX: socketpair depois do fork, bind/listen/accept/connect,
 * send/recv, SCM_RIGHTS, LOCAL_PEERPID e um datagrama com o caminho de quem enviou.
 * O autoteste (userland/teste.c) dispara este programa. Sem o teste geral, proc_init
 * o sobe direto quando a imagem traz /Sistema/Programas/.autoteste. */
#include "mos.h"

static int fails;
#define CHECK(c, ...) do { if (c) print("  ok  " __VA_ARGS__); else { fails++; print("  FALHOU  " __VA_ARGS__); } print("\n"); } while (0)

typedef struct { unsigned char len, family; char path[104]; } sun_t;
typedef struct { void *base; unsigned long len; } iov_t;
typedef struct {
    void *name; unsigned namelen, pad;
    iov_t *iov; int iovlen, pad2;
    void *control; unsigned controllen; int flags;
} msg_t;
typedef struct { int level, name; unsigned len; void *val; } opt_t;

_Static_assert(sizeof(msg_t) == 48, "msghdr");
_Static_assert(sizeof(opt_t) == 24, "sockopt");

static int U(int op, u64 a, u64 b, u64 c) { return (int)sys4(SYS_UNIX, (u64)op, a, b, c); }

static void sun_set(sun_t *s, const char *p) {
    memset(s, 0, sizeof *s);
    usize n = strlen(p);
    s->family = 1;
    s->len = (unsigned char)(n + 2);
    memcpy(s->path, p, n);
}

static int peer_pid(int fd) {
    int pid = -1;
    opt_t o;
    o.level = 0;
    o.name = 0x002; /* LOCAL_PEERPID */
    o.len = 4;
    o.val = &pid;
    if (U(MOS_UNIX_GETSOCKOPT, (u64)fd, (u64)&o, 0) < 0) return -1;
    return pid;
}

static int msg_io(int fd, int recv, void *buf, unsigned n, void *ctl, unsigned *clen, void *name, unsigned *nlen) {
    iov_t iov;
    msg_t m;
    iov.base = buf;
    iov.len = n;
    memset(&m, 0, sizeof m);
    m.iov = &iov;
    m.iovlen = 1;
    m.control = ctl;
    m.controllen = clen ? *clen : 0;
    m.name = name;
    m.namelen = nlen ? *nlen : 0;
    int r = U(recv ? MOS_UNIX_RECVMSG : MOS_UNIX_SENDMSG, (u64)fd, (u64)&m, 0);
    if (clen) *clen = m.controllen;
    if (nlen) *nlen = m.namelen;
    return r;
}

static void pair_test(void) {
    int sv[2] = { -1, -1 };
    CHECK(U(MOS_UNIX_PAIR, 1, 0, (u64)sv) == 0 && sv[0] >= 0 && sv[1] >= 0, "socketpair devolve dois fds");
    int kid = fork();
    CHECK(kid >= 0, "fork do socketpair");
    if (kid == 0) {
        char b[8];
        i64 n = read(sv[1], b, sizeof b);
        if (n == 3 && b[0] == 'p' && b[1] == 'a' && b[2] == 'r') {
            write(sv[1], "ok", 2);
            exit(0);
        }
        exit(1);
    }
    if (kid > 0) {
        CHECK(write(sv[0], "par", 3) == 3, "pai escreve no socketpair");
        char b[8] = {0};
        i64 n = read(sv[0], b, sizeof b);
        CHECK(n == 2 && b[0] == 'o' && b[1] == 'k', "filho responde pelo socketpair depois do fork");
        CHECK(wait(kid) == 0, "filho do socketpair sai com 0");
    }
    if (sv[0] >= 0) close(sv[0]);
    if (sv[1] >= 0) close(sv[1]);
}

void _start(const char *arg) {
    (void)arg;
    print("unixipc: AF_UNIX entre processos\n");
    pair_test();

    int a = U(MOS_UNIX_SOCKET, 1, 1, 0);
    int b = U(MOS_UNIX_SOCKET, 1, 1, 0);
    sun_t addr;
    sun_set(&addr, "/tmp/ux-srv");
    CHECK(a >= 0 && U(MOS_UNIX_BIND, (u64)a, (u64)&addr, sizeof addr) == 0, "bind /tmp/ux-srv");
    CHECK(b >= 0 && U(MOS_UNIX_BIND, (u64)b, (u64)&addr, sizeof addr) == -MOS_EADDRINUSE, "segundo bind: EADDRINUSE");
    if (b >= 0) close(b);
    CHECK(U(MOS_UNIX_LISTEN, (u64)a, 4, 0) == 0, "listen");

    CHECK(write_file("/tmp/ux-payload", "fdok", 4) == 0, "arquivo que vai passar por SCM_RIGHTS");
    int file = open("/tmp/ux-payload", MOS_O_RDONLY);
    CHECK(file >= 0, "abre o arquivo");

    int dg = U(MOS_UNIX_SOCKET, 1, 2, 0);
    sun_t daddr;
    sun_set(&daddr, "/tmp/ux-dp");
    CHECK(dg >= 0 && U(MOS_UNIX_BIND, (u64)dg, (u64)&daddr, sizeof daddr) == 0, "bind do datagrama /tmp/ux-dp");

    int syncp[2] = { -1, -1 };
    CHECK(pipe(syncp) == 0, "pipe de sincronia");
    int ppid = getpid();
    int kid = fork();
    CHECK(kid >= 0, "fork do servidor");
    if (kid == 0) {
        if (syncp[1] >= 0) close(syncp[1]);
        /* o filho herda o pipe de leitura em syncp[0]. child_main lia o fd 3, que só é verdade
         * se o pipe foi o primeiro fd livre. Usa o número real. */
        char go;
        if (read(syncp[0], &go, 1) != 1) exit(2);
        sun_t to;
        sun_set(&to, "/tmp/ux-srv");
        int c = U(MOS_UNIX_SOCKET, 1, 1, 0);
        if (c < 0 || U(MOS_UNIX_CONNECT, (u64)c, (u64)&to, sizeof to) < 0) exit(3);
        if (write(c, "ping!", 5) != 5) exit(4);
        char buf[8] = {0};
        if (read(c, buf, 4) != 4 || memcmp(buf, "pong", 4) != 0) exit(5);
        unsigned char ctl[32];
        unsigned clen = sizeof ctl;
        char one = 0;
        if (msg_io(c, 1, &one, 1, ctl, &clen, 0, 0) != 1 || one != 'R' || clen < 16) exit(6);
        int rfd = 0;
        memcpy(&rfd, ctl + 12, 4);
        char payload[4] = {0};
        if (read(rfd, payload, 4) != 4 || memcmp(payload, "fdok", 4) != 0) exit(7);
        if (peer_pid(c) != ppid) exit(8);
        if (write(c, "rights", 6) != 6) exit(9);
        int d = U(MOS_UNIX_SOCKET, 1, 2, 0);
        sun_t mine, dest;
        sun_set(&mine, "/tmp/ux-dc");
        sun_set(&dest, "/tmp/ux-dp");
        unsigned dlen = sizeof dest;
        if (d < 0 || U(MOS_UNIX_BIND, (u64)d, (u64)&mine, sizeof mine) < 0) exit(10);
        if (msg_io(d, 0, "dg", 2, 0, 0, &dest, &dlen) != 2) exit(11);
        char back[4] = {0};
        sun_t from;
        unsigned nl = sizeof from;
        if (msg_io(d, 1, back, 2, 0, 0, &from, &nl) != 2 || memcmp(back, "ok", 2) != 0) exit(12);
        if (U(MOS_UNIX_SHUTDOWN, (u64)c, 1, 0) < 0) exit(13);
        exit(0);
    }

    if (kid > 0) {
        close(syncp[0]);
        CHECK(write(syncp[1], "g", 1) == 1, "acorda o cliente");
        sun_t peer;
        unsigned plen = sizeof peer;
        int cfd = U(MOS_UNIX_ACCEPT, (u64)a, (u64)&peer, (u64)&plen);
        CHECK(cfd >= 0, "accept");
        char buf[8] = {0};
        CHECK(read(cfd, buf, 4) == 4 && memcmp(buf, "ping", 4) == 0, "recv ping");
        mos_stat_t st;
        memset(&st, 0, sizeof st);
        CHECK(fstat(cfd, &st) == 0 && st.is_dir == 6 && st.size >= 1, "fstat do socket: is_dir 6 e o resto do send");
        CHECK(read(cfd, buf, 1) == 1 && buf[0] == '!', "o recv não atravessa o send inteiro de uma vez só quando o buffer é menor");
        CHECK(write(cfd, "pong", 4) == 4, "send pong");
        CHECK(peer_pid(cfd) == kid, "LOCAL_PEERPID do accept é o filho");
        unsigned char cred[76];
        opt_t o;
        o.level = 0;
        o.name = 0x001;
        o.len = sizeof cred;
        o.val = cred;
        memset(cred, 0, sizeof cred);
        CHECK(U(MOS_UNIX_GETSOCKOPT, (u64)cfd, (u64)&o, 0) == 0, "LOCAL_PEERCRED");
        unsigned uid = 0;
        memcpy(&uid, cred + 4, 4);
        CHECK(uid == 501, "xucred uid 501");

        struct { unsigned len; int level, type, fd; } cmsg;
        cmsg.len = 16;
        cmsg.level = 0xffff;
        cmsg.type = 1;
        cmsg.fd = file;
        unsigned clen = 16;
        CHECK(msg_io(cfd, 0, "R", 1, &cmsg, &clen, 0, 0) == 1, "SCM_RIGHTS");
        CHECK(read(cfd, buf, 6) == 6 && memcmp(buf, "rights", 6) == 0, "filho leu o fd recebido");

        char dgbuf[4] = {0};
        sun_t from;
        unsigned nl = sizeof from;
        CHECK(msg_io(dg, 1, dgbuf, 2, 0, 0, &from, &nl) == 2 && memcmp(dgbuf, "dg", 2) == 0, "datagrama do outro processo");
        CHECK(peer_pid(dg) == kid, "LOCAL_PEERPID do datagrama é quem enviou");
        int path_ok = 0;
        for (int i = 0; from.path[i]; i++) if (from.path[i] == 'u' && from.path[i + 1] == 'x') path_ok = 1;
        CHECK(path_ok, "recvfrom devolve o caminho em que o remetente fez bind");
        sun_t back;
        sun_set(&back, "/tmp/ux-dc");
        unsigned bl = sizeof back;
        CHECK(msg_io(dg, 0, "ok", 2, 0, 0, &back, &bl) == 2, "resposta em datagrama");

        char z;
        CHECK(read(cfd, &z, 1) == 0, "shutdown de escrita do outro lado: read 0");
        int code = wait(kid);
        CHECK(code == 0, "cliente sai com 0 (código %d)", (i64)code);

        close(a);
        int late = U(MOS_UNIX_SOCKET, 1, 1, 0);
        CHECK(late >= 0 && U(MOS_UNIX_CONNECT, (u64)late, (u64)&addr, sizeof addr) == -MOS_ECONNREFUSED,
              "fechou sem unlink: connect ECONNREFUSED");
        CHECK(unlink("/tmp/ux-srv") == 0, "unlink do socket");
        CHECK(U(MOS_UNIX_CONNECT, (u64)late, (u64)&addr, sizeof addr) == -MOS_ENOENT,
              "depois do unlink: connect ENOENT");
        CHECK(U(MOS_UNIX_BIND, (u64)late, (u64)&addr, sizeof addr) == 0, "bind de novo no mesmo caminho");
        close(late);
        unlink("/tmp/ux-srv");
        unlink("/tmp/ux-dp");
        unlink("/tmp/ux-dc");
        unlink("/tmp/ux-payload");
    }

    print(fails ? "unixipc: %d FALHA(S)\n" : "unixipc: TODOS OS TESTES OK\n", (i64)fails);
    exit(fails);
}
