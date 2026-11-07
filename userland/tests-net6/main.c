/*
 * mOS — teste do IPv6 dos apps (kernel/net/ipv6.c, tcp.c, usersock.c; userland/libsystem/sockets_inet.inc).
 * Compila igual no macOS (clang main.c). No mOS: .\tools\build-nettest6.ps1 -OutDir build-net6 (abre no boot; saída na serial).
 *   1. laço local: servidor TCP em [::1]:0 (bind/listen/getsockname/accept), cliente, troca de dados, getpeername
 *   2. dual-stack: ouvinte em [::] aceita um cliente IPv4 (127.0.0.1) como ::ffff:127.0.0.1; com IPV6_V6ONLY recusa
 *   3. servidor IPv4 em 127.0.0.1 e connect para porta local fechada = ECONNREFUSED
 *   4. ping6 (SOCK_DGRAM + IPPROTO_ICMPV6) para ::1 e para o gateway da rede (padrão fec0::2, o do user-net do QEMU)
 *   5. getaddrinfo: AAAA de um nome (padrão example.com), "localhost" com AF_INET6, AF_UNSPEC com IPv4 primeiro
 *   6. TCP sobre IPv6 para fora: GET HTTP em [alvo]:porta (padrão [fec0::2]:8086, que o QEMU leva ao ::1 do host)
 * Argumentos: [nome-AAAA] [alvo-v6] [porta]. Imprime "net6test: N/N ok".
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#ifndef ICMP6_ECHO_REQUEST            /* <netinet/icmp6.h> não vem no SDK mínimo do zig */
#define ICMP6_ECHO_REQUEST 128
#define ICMP6_ECHO_REPLY 129
#endif
#include <arpa/inet.h>

static int ok, total;
static void check(int cond, const char *what) {
    total++;
    if (cond) ok++;
    printf("%s %s\n", cond ? "  ok  " : "  FALHA", what);
    fflush(stdout);
}

static int wait_fd(int fd, short ev, int ms) {
    struct pollfd p = {fd, ev, 0};
    return poll(&p, 1, ms) == 1 && (p.revents & (ev | POLLHUP | POLLERR));
}

/* servidor em addr (porta 0) e cliente na família cfam para caddr; troca "ping"/"pong". 1 = tudo certo */
static int roundtrip(int sfam, const char *saddr, int cfam, const char *caddr, int v6only, char *peer, size_t peerlen) {
    int ls = socket(sfam, SOCK_STREAM, 0);
    if (ls < 0) { printf("  socket: %s\n", strerror(errno)); return 0; }
    if (sfam == AF_INET6) setsockopt(ls, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof v6only);
    struct sockaddr_storage ss;
    memset(&ss, 0, sizeof ss);
    socklen_t sl;
    if (sfam == AF_INET6) {
        struct sockaddr_in6 *a = (struct sockaddr_in6 *)&ss;
        a->sin6_len = sizeof *a; a->sin6_family = AF_INET6;
        inet_pton(AF_INET6, saddr, &a->sin6_addr);
        sl = sizeof *a;
    } else {
        struct sockaddr_in *a = (struct sockaddr_in *)&ss;
        a->sin_len = sizeof *a; a->sin_family = AF_INET;
        inet_pton(AF_INET, saddr, &a->sin_addr);
        sl = sizeof *a;
    }
    if (bind(ls, (struct sockaddr *)&ss, sl) || listen(ls, 4)) { printf("  bind/listen: %s\n", strerror(errno)); close(ls); return 0; }
    struct sockaddr_storage got;
    socklen_t gl = sizeof got;
    if (getsockname(ls, (struct sockaddr *)&got, &gl)) { close(ls); return 0; }
    unsigned short port = ntohs(sfam == AF_INET6 ? ((struct sockaddr_in6 *)&got)->sin6_port : ((struct sockaddr_in *)&got)->sin_port);
    if (!port) { printf("  getsockname: porta 0\n"); close(ls); return 0; }

    int c = socket(cfam, SOCK_STREAM, 0);
    struct sockaddr_storage cs;
    memset(&cs, 0, sizeof cs);
    socklen_t cl;
    if (cfam == AF_INET6) {
        struct sockaddr_in6 *a = (struct sockaddr_in6 *)&cs;
        a->sin6_len = sizeof *a; a->sin6_family = AF_INET6; a->sin6_port = htons(port);
        inet_pton(AF_INET6, caddr, &a->sin6_addr);
        cl = sizeof *a;
    } else {
        struct sockaddr_in *a = (struct sockaddr_in *)&cs;
        a->sin_len = sizeof *a; a->sin_family = AF_INET; a->sin_port = htons(port);
        inet_pton(AF_INET, caddr, &a->sin_addr);
        cl = sizeof *a;
    }
    int r = connect(c, (struct sockaddr *)&cs, cl);
    if (r) {
        int e = errno;
        close(c); close(ls);
        errno = e;
        return -1;                                     /* quem chama decide se a recusa era esperada */
    }
    int ret = 0;
    if (!wait_fd(ls, POLLIN, 3000)) { printf("  accept: nada em 3 s\n"); goto out; }
    struct sockaddr_storage ps;
    socklen_t pl = sizeof ps;
    int a = accept(ls, (struct sockaddr *)&ps, &pl);
    if (a < 0) { printf("  accept: %s\n", strerror(errno)); goto out; }
    if (peer) {
        if (ps.ss_family == AF_INET6) inet_ntop(AF_INET6, &((struct sockaddr_in6 *)&ps)->sin6_addr, peer, (socklen_t)peerlen);
        else inet_ntop(AF_INET, &((struct sockaddr_in *)&ps)->sin_addr, peer, (socklen_t)peerlen);
    }
    char buf[16] = {0};
    if (write(c, "ping", 4) == 4 && wait_fd(a, POLLIN, 3000) && read(a, buf, sizeof buf) == 4 && !memcmp(buf, "ping", 4) &&
        write(a, "pong", 4) == 4 && wait_fd(c, POLLIN, 3000) && read(c, buf, sizeof buf) == 4 && !memcmp(buf, "pong", 4)) {
        close(a);                                      /* o cliente vê o fim do fluxo */
        ret = wait_fd(c, POLLIN, 3000) && read(c, buf, sizeof buf) == 0;
        if (!ret) printf("  sem EOF depois do close\n");
    } else {
        printf("  troca de dados falhou\n");
        close(a);
    }
out:
    close(c);
    close(ls);
    return ret;
}

/* ping6: eco ICMPv6 para dst; 1 se veio resposta com o mesmo identificador */
static int ping6(const char *dst, int tries) {
    int s = socket(AF_INET6, SOCK_DGRAM, IPPROTO_ICMPV6);
    if (s < 0) { printf("  socket ICMPv6: %s\n", strerror(errno)); return 0; }
    struct sockaddr_in6 to;
    memset(&to, 0, sizeof to);
    to.sin6_len = sizeof to; to.sin6_family = AF_INET6;
    inet_pton(AF_INET6, dst, &to.sin6_addr);
    int got = 0;
    for (int i = 0; i < tries && !got; i++) {
        unsigned char m[64];
        memset(m, 0, sizeof m);
        m[0] = ICMP6_ECHO_REQUEST;
        m[4] = 0x6E; m[5] = 0x36;                      /* identificador */
        m[7] = (unsigned char)(i + 1);                 /* sequência */
        memcpy(m + 8, "mOS ping6", 9);
        if (sendto(s, m, sizeof m, 0, (struct sockaddr *)&to, sizeof to) != sizeof m) { printf("  sendto: %s\n", strerror(errno)); sleep(1); continue; }
        if (!wait_fd(s, POLLIN, 1500)) continue;
        unsigned char r[128];
        struct sockaddr_in6 from;
        socklen_t fl = sizeof from;
        long n = recvfrom(s, r, sizeof r, 0, (struct sockaddr *)&from, &fl);
        char fs[64] = "?";
        inet_ntop(AF_INET6, &from.sin6_addr, fs, sizeof fs);
        if (n >= 8 && r[0] == ICMP6_ECHO_REPLY && r[4] == 0x6E && r[5] == 0x36) { got = 1; printf("  resposta de %s, seq %u\n", fs, r[7]); }
    }
    close(s);
    return got;
}

int main(int argc, char **argv) {
    const char *name = argc > 1 ? argv[1] : "example.com";
    const char *target = argc > 2 ? argv[2] : "fec0::2";
    const char *tport = argc > 3 ? argv[3] : "8086";
    char peer[64] = "";

    printf("net6test: laço local\n");
    check(roundtrip(AF_INET6, "::1", AF_INET6, "::1", 0, peer, sizeof peer) == 1 && !strcmp(peer, "::1"), "TCP [::1] servidor + cliente, getsockname/accept/EOF");
    check(roundtrip(AF_INET6, "::", AF_INET, "127.0.0.1", 0, peer, sizeof peer) == 1 && !strcmp(peer, "::ffff:127.0.0.1"), "dual-stack: [::] aceita 127.0.0.1 como ::ffff:127.0.0.1");
    printf("  (par visto: %s)\n", peer);
    int r = roundtrip(AF_INET6, "::", AF_INET, "127.0.0.1", 1, 0, 0);
    check(r == -1 && errno == ECONNREFUSED, "IPV6_V6ONLY: cliente IPv4 recusado");
    check(roundtrip(AF_INET, "127.0.0.1", AF_INET, "127.0.0.1", 0, peer, sizeof peer) == 1 && !strcmp(peer, "127.0.0.1"), "TCP 127.0.0.1 servidor + cliente");
    {
        int c = socket(AF_INET6, SOCK_STREAM, 0);
        struct sockaddr_in6 a;
        memset(&a, 0, sizeof a);
        a.sin6_len = sizeof a; a.sin6_family = AF_INET6; a.sin6_port = htons(9); a.sin6_addr = in6addr_loopback;
        int k = connect(c, (struct sockaddr *)&a, sizeof a);
        check(k < 0 && errno == ECONNREFUSED, "connect [::1]:9 (sem escuta) = ECONNREFUSED");
        close(c);
    }
    check(ping6("::1", 2), "ping6 ::1");

    printf("net6test: rede (alvo %s)\n", target);
    /* o RA do roteador chega logo após o boot; dá até ~10 s */
    int pg = 0;
    for (int i = 0; i < 4 && !(pg = ping6(target, 2)); i++) sleep(1);
    check(pg, "ping6 para o gateway/alvo");

    struct addrinfo h, *res = 0;
    memset(&h, 0, sizeof h);
    h.ai_family = AF_INET6; h.ai_socktype = SOCK_STREAM;
    int g = getaddrinfo("localhost", "80", &h, &res);
    char s6[64] = "";
    if (!g && res) inet_ntop(AF_INET6, &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr, s6, sizeof s6);
    check(!g && res && res->ai_family == AF_INET6 && res->ai_addrlen == sizeof(struct sockaddr_in6) && !strcmp(s6, "::1"), "getaddrinfo(localhost, AF_INET6) = ::1");
    if (res) freeaddrinfo(res);
    res = 0;
    g = getaddrinfo(name, "443", &h, &res);
    if (!g && res) inet_ntop(AF_INET6, &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr, s6, sizeof s6);
    check(!g && res && res->ai_family == AF_INET6, "getaddrinfo(nome, AF_INET6) devolve AAAA");
    printf("  %s -> %s\n", name, g ? gai_strerror(g) : s6);
    if (res) freeaddrinfo(res);
    res = 0;
    h.ai_family = AF_UNSPEC;
    g = getaddrinfo(name, "443", &h, &res);
    int n4 = 0, n6 = 0;
    for (struct addrinfo *p = res; p; p = p->ai_next) { if (p->ai_family == AF_INET) n4++; else if (p->ai_family == AF_INET6) n6++; }
    check(!g && res && res->ai_family == AF_INET, "getaddrinfo(nome, AF_UNSPEC): IPv4 primeiro");
    printf("  AF_UNSPEC: %d A, %d AAAA\n", n4, n6);
    if (res) freeaddrinfo(res);

    /* TCP sobre IPv6 para fora */
    res = 0;
    h.ai_family = AF_INET6; h.ai_flags = AI_NUMERICHOST;
    g = getaddrinfo(target, tport, &h, &res);
    int tcpok = 0;
    if (!g && res) {
        int c = socket(AF_INET6, SOCK_STREAM, 0);
        if (connect(c, res->ai_addr, res->ai_addrlen) == 0) {
            char req[160], buf[512];
            snprintf(req, sizeof req, "GET / HTTP/1.0\r\nHost: [%s]\r\nUser-Agent: mOS-net6test\r\nConnection: close\r\n\r\n", target);
            write(c, req, strlen(req));
            long n = 0, k;
            while (n < (long)sizeof buf - 1 && wait_fd(c, POLLIN, 5000) && (k = read(c, buf + n, sizeof buf - 1 - n)) > 0) n += k;
            buf[n] = 0;
            char *e = strstr(buf, "\r\n");
            if (e) *e = 0;
            printf("  [%s]:%s -> %s\n", target, tport, n ? buf : "(nada)");
            tcpok = !strncmp(buf, "HTTP/", 5);
            struct sockaddr_in6 me;
            socklen_t ml = sizeof me;
            char ms[64] = "?";
            if (!getsockname(c, (struct sockaddr *)&me, &ml)) inet_ntop(AF_INET6, &me.sin6_addr, ms, sizeof ms);
            printf("  endereço local: %s\n", ms);
        } else printf("  connect: %s\n", strerror(errno));
        close(c);
    }
    if (res) freeaddrinfo(res);
    check(tcpok, "TCP sobre IPv6 (GET HTTP no alvo)");

    printf("net6test: %d/%d ok\n", ok, total);
    fflush(stdout);
    return ok == total ? 0 : 1;
}
