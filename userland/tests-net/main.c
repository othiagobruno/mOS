/*
 * mOS — teste da rede dos apps: getaddrinfo + sockets TCP (userland/libsystem/sockets_inet.inc, kernel/net/usersock.c).
 * Compila igual no macOS (clang main.c). No mOS: .\tools\build-nettest.ps1 -OutDir build-net (abre no boot; saída na serial).
 *   1. getaddrinfo("example.com", "80") e numérico ("10.0.2.2"), getnameinfo, inet_pton/ntop
 *   2. connect bloqueante + GET HTTP/1.0 + leitura até o fim (EOF)
 *   3. connect com O_NONBLOCK: EINPROGRESS, poll(POLLOUT) até conectar, SO_ERROR = 0, recv com EAGAIN, kqueue EVFILT_READ
 *   4. connect para uma porta fechada (127.0.0.1) = ECONNREFUSED
 * Imprime "nettest: N/N ok". Argumento opcional: o host a usar (padrão example.com).
 */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/event.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static int ok, total;
static void check(int cond, const char *what) {
    total++;
    if (cond) ok++;
    printf("%s %s\n", cond ? "  ok  " : "  FALHA", what);
    fflush(stdout);
}

static int read_all(int s, char *buf, int cap, int *first_line_end) {
    int n = 0;
    for (;;) {
        long k = read(s, buf + n, (size_t)(cap - 1 - n));
        if (k <= 0) break;
        n += (int)k;
        if (n >= cap - 1) {           /* buffer cheio: continua lendo e descartando, para chegar ao EOF */
            char junk[4096];
            while (read(s, junk, sizeof junk) > 0) ;
            break;
        }
    }
    buf[n] = 0;
    char *e = strstr(buf, "\r\n");
    *first_line_end = e ? (int)(e - buf) : 0;
    return n;
}

int main(int argc, char **argv) {
    const char *host = argc > 1 ? argv[1] : "example.com";
    char req[256];
    snprintf(req, sizeof req, "GET / HTTP/1.0\r\nHost: %s\r\nUser-Agent: mOS-nettest\r\nConnection: close\r\n\r\n", host);

    /* 1. resolução */
    struct addrinfo hints, *res = 0;
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int g = getaddrinfo(host, "80", &hints, &res);
    check(g == 0 && res && res->ai_family == AF_INET && res->ai_addrlen == sizeof(struct sockaddr_in), "getaddrinfo(host, \"80\")");
    if (g) { printf("  getaddrinfo: %s\n", gai_strerror(g)); printf("nettest: %d/%d ok\n", ok, total); return 1; }
    char ipstr[32] = "", port[8] = "";
    check(getnameinfo(res->ai_addr, res->ai_addrlen, ipstr, sizeof ipstr, port, sizeof port, NI_NUMERICHOST | NI_NUMERICSERV) == 0 &&
          !strcmp(port, "80"), "getnameinfo numérico");
    printf("  %s = %s\n", host, ipstr);
    struct addrinfo *num = 0;
    check(getaddrinfo("10.0.2.2", "https", &hints, &num) == 0 && ntohs(((struct sockaddr_in *)num->ai_addr)->sin_port) == 443,
          "getaddrinfo numérico + serviço \"https\"");
    freeaddrinfo(num);
    struct in_addr ia;
    char back[INET_ADDRSTRLEN];
    check(inet_pton(AF_INET, "192.168.10.200", &ia) == 1 && !strcmp(inet_ntop(AF_INET, &ia, back, sizeof back), "192.168.10.200"),
          "inet_pton/inet_ntop");

    /* 2. bloqueante */
    int s = socket(AF_INET, SOCK_STREAM, 0);
    check(s >= 0, "socket(AF_INET, SOCK_STREAM)");
    int c = connect(s, res->ai_addr, res->ai_addrlen);
    check(c == 0, "connect bloqueante");
    if (c) printf("  errno %d (%s)\n", errno, strerror(errno));
    struct sockaddr_in peer;
    socklen_t pl = sizeof peer;
    check(c == 0 && getpeername(s, (struct sockaddr *)&peer, &pl) == 0 && peer.sin_port == htons(80), "getpeername");
    long w = c == 0 ? write(s, req, strlen(req)) : -1;
    check(w == (long)strlen(req), "write da requisição");
    static char buf[256 * 1024];
    int fl = 0;
    int n = c == 0 ? read_all(s, buf, sizeof buf, &fl) : 0;
    check(n > 0 && !strncmp(buf, "HTTP/1.", 7), "resposta HTTP lida até o fim (EOF)");
    if (n > 0) printf("  %d bytes: %.*s\n", n, fl > 0 && fl < 80 ? fl : 40, buf);
    close(s);

    /* 3. não-bloqueante + poll + SO_ERROR + kqueue */
    s = socket(AF_INET, SOCK_STREAM, 0);
    fcntl(s, F_SETFL, fcntl(s, F_GETFL) | O_NONBLOCK);
    check((fcntl(s, F_GETFL) & O_NONBLOCK) != 0, "fcntl O_NONBLOCK");
    c = connect(s, res->ai_addr, res->ai_addrlen);
    check(c == -1 && errno == EINPROGRESS, "connect não-bloqueante = EINPROGRESS");
    struct pollfd pf = {s, POLLOUT, 0};
    int pr = poll(&pf, 1, 15000);
    int soerr = -1;
    socklen_t sl = sizeof soerr;
    getsockopt(s, SOL_SOCKET, SO_ERROR, &soerr, &sl);
    check(pr == 1 && (pf.revents & POLLOUT) && soerr == 0, "poll(POLLOUT) até conectar, SO_ERROR = 0");
    char tmp[64];
    long r = recv(s, tmp, sizeof tmp, 0);
    check(r == -1 && errno == EAGAIN, "recv sem dados = EAGAIN");
    send(s, req, strlen(req), 0);
    int kq = kqueue();
    struct kevent ev;
    EV_SET(&ev, s, EVFILT_READ, EV_ADD, 0, 0, 0);
    kevent(kq, &ev, 1, 0, 0, 0);
    struct timespec to = {15, 0};
    struct kevent got;
    int ke = kevent(kq, 0, 0, &got, 1, &to);
    check(ke == 1 && (int)got.ident == s && got.filter == EVFILT_READ && got.data > 0, "kqueue EVFILT_READ acorda com a resposta");
    close(kq);
    int total_nb = 0;
    for (int tries = 0; tries < 3000; tries++) {
        long k = recv(s, buf, sizeof buf, 0);
        if (k > 0) { total_nb += (int)k; continue; }
        if (k == 0) break;
        if (errno == EAGAIN) { struct pollfd p2 = {s, POLLIN, 0}; poll(&p2, 1, 100); continue; }
        break;
    }
    check(total_nb > 0, "resposta lida em modo não-bloqueante");
    close(s);

    /* 4. porta fechada */
    s = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in lo = {0};
    lo.sin_len = sizeof lo;
    lo.sin_family = AF_INET;
    lo.sin_port = htons(9);
    lo.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    check(connect(s, (struct sockaddr *)&lo, sizeof lo) == -1 && errno == ECONNREFUSED, "127.0.0.1:9 = ECONNREFUSED");
    close(s);

    freeaddrinfo(res);
    printf("nettest: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
