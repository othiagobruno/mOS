/*
 * mOS — teste dos sockets UDP (SOCK_DGRAM) dos apps: libSystem (sockets_udp.inc) + kernel (kernel/net/userudp.c).
 * Compila igual no macOS (só a API POSIX/BSD). Saída: "udptest: ok <nome>" / "udptest: FALHA <nome>" e "udptest: N/M ok".
 *   .\build.ps1 -OutDir build-udp
 *   .\tools\build-udptest.ps1 -OutDir build-udp
 *   .\run.ps1 -Headless -Image build-udp\mOS.img -Serial build-udp\serial.log      (procure "udptest:" no log)
 */
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/event.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

static int ok, total;
static void check(int c, const char *name, const char *detail) {
    total++;
    if (c) { ok++; fprintf(stderr, "udptest: ok %s\n", name); }
    else fprintf(stderr, "udptest: FALHA %s (%s; errno %d)\n", name, detail ? detail : "", errno);
}

static unsigned short port_of(int fd) {
    struct sockaddr_storage ss;
    socklen_t l = sizeof ss;
    if (getsockname(fd, (struct sockaddr *)&ss, &l) < 0) return 0;
    if (ss.ss_family == AF_INET) return ntohs(((struct sockaddr_in *)&ss)->sin_port);
    return ntohs(((struct sockaddr_in6 *)&ss)->sin6_port);
}

static int udp4_bound(const char *ip, unsigned short port) {
    int s = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in a = {0};
    a.sin_len = sizeof a;
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    inet_pton(AF_INET, ip, &a.sin_addr);
    if (s >= 0 && bind(s, (struct sockaddr *)&a, sizeof a) < 0) { close(s); return -1; }
    return s;
}

static struct sockaddr_in loop4(unsigned short port) {
    struct sockaddr_in a = {0};
    a.sin_len = sizeof a;
    a.sin_family = AF_INET;
    a.sin_port = htons(port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    return a;
}

int main(int argc, char **argv) {
    char buf[4096];
    char dt[128];

    /* 1. laço local IPv4: bind efêmero, sendto, recvfrom com a origem */
    int a = udp4_bound("127.0.0.1", 0), b = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    unsigned short pa = port_of(a);
    check(a >= 0 && b >= 0 && pa >= 1024, "socket + bind efêmero", NULL);
    struct sockaddr_in to = loop4(pa);
    ssize_t n = sendto(b, "ola", 3, 0, (struct sockaddr *)&to, sizeof to);
    check(n == 3, "sendto 127.0.0.1", NULL);
    unsigned short pb = port_of(b);
    check(pb != 0, "bind implícito do sendto (getsockname)", NULL);
    struct sockaddr_in from;
    socklen_t fl = sizeof from;
    memset(buf, 0, sizeof buf);
    n = recvfrom(a, buf, sizeof buf, 0, (struct sockaddr *)&from, &fl);
    snprintf(dt, sizeof dt, "n=%zd '%s' porta %u/%u", n, buf, ntohs(from.sin_port), pb);
    check(n == 3 && !memcmp(buf, "ola", 3) && from.sin_family == AF_INET && ntohs(from.sin_port) == pb &&
          from.sin_addr.s_addr == htonl(INADDR_LOOPBACK), "recvfrom com origem", dt);

    /* 2. resposta para a origem e connect (send/recv sem endereço, getpeername) */
    n = sendto(a, "resposta", 8, 0, (struct sockaddr *)&from, fl);
    memset(buf, 0, sizeof buf);
    n = recv(b, buf, sizeof buf, 0);
    check(n == 8 && !memcmp(buf, "resposta", 8), "resposta de volta", NULL);
    check(connect(b, (struct sockaddr *)&to, sizeof to) == 0, "connect UDP", NULL);
    struct sockaddr_in peer;
    socklen_t pl = sizeof peer;
    check(getpeername(b, (struct sockaddr *)&peer, &pl) == 0 && ntohs(peer.sin_port) == pa, "getpeername", NULL);
    check(send(b, "conectado", 9, 0) == 9, "send conectado", NULL);
    memset(buf, 0, sizeof buf);
    check(recv(a, buf, sizeof buf, 0) == 9 && !memcmp(buf, "conectado", 9), "recv do conectado", NULL);
    check(sendto(b, "x", 1, 0, (struct sockaddr *)&to, sizeof to) < 0 && errno == EISCONN, "sendto num conectado: EISCONN", NULL);

    /* 3. conectado descarta outras origens */
    int c = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in tob = loop4(pb);
    sendto(c, "intruso", 7, 0, (struct sockaddr *)&tob, sizeof tob);
    sendto(a, "do par", 6, 0, (struct sockaddr *)&tob, sizeof tob);
    memset(buf, 0, sizeof buf);
    n = recv(b, buf, sizeof buf, 0);
    check(n == 6 && !memcmp(buf, "do par", 6), "conectado ignora outras origens", buf);
    close(c);

    /* 4. não-bloqueante: EAGAIN, poll, MSG_PEEK, truncamento e datagrama vazio */
    fcntl(a, F_SETFL, fcntl(a, F_GETFL) | O_NONBLOCK);
    n = recv(a, buf, sizeof buf, 0);
    check(n < 0 && errno == EAGAIN, "recv sem dados: EAGAIN", NULL);
    struct pollfd pf = {a, POLLIN, 0};
    check(poll(&pf, 1, 0) == 0, "poll sem dados", NULL);
    send(b, "espiar", 6, 0);
    pf.revents = 0;
    check(poll(&pf, 1, 1000) == 1 && (pf.revents & POLLIN), "poll acorda com datagrama", NULL);
    memset(buf, 0, sizeof buf);
    n = recv(a, buf, sizeof buf, MSG_PEEK);
    check(n == 6 && !memcmp(buf, "espiar", 6), "MSG_PEEK", NULL);
    memset(buf, 0, sizeof buf);
    n = recv(a, buf, 3, 0);
    check(n == 3 && !memcmp(buf, "esp", 3), "truncado no buffer pequeno", NULL);
    n = recv(a, buf, sizeof buf, 0);
    check(n < 0 && errno == EAGAIN, "o resto do truncado foi descartado", NULL);
    send(b, "grande demais", 13, 0);
    char small[4];
    struct iovec iov = {small, sizeof small};
    struct msghdr mh = {0};
    struct sockaddr_in mfrom;
    mh.msg_name = &mfrom;
    mh.msg_namelen = sizeof mfrom;
    mh.msg_iov = &iov;
    mh.msg_iovlen = 1;
    usleep(20000);
    n = recvmsg(a, &mh, 0);
    check(n == 4 && (mh.msg_flags & MSG_TRUNC) && ntohs(mfrom.sin_port) == pb, "recvmsg com MSG_TRUNC e msg_name", NULL);
    send(b, "", 0, 0);
    usleep(20000);
    socklen_t zl = sizeof from;
    n = recvfrom(a, buf, sizeof buf, 0, (struct sockaddr *)&from, &zl);
    check(n == 0 && ntohs(from.sin_port) == pb, "datagrama vazio", NULL);

    /* 5. kqueue EVFILT_READ */
    int kq = kqueue();
    struct kevent ev;
    EV_SET(&ev, a, EVFILT_READ, EV_ADD, 0, 0, NULL);
    kevent(kq, &ev, 1, NULL, 0, NULL);
    send(b, "kq", 2, 0);
    struct timespec ts = {1, 0};
    struct kevent out;
    int k = kevent(kq, NULL, 0, &out, 1, &ts);
    check(k == 1 && (int)out.ident == a && out.data == 2, "kqueue EVFILT_READ com tamanho", NULL);
    recv(a, buf, sizeof buf, 0);
    close(kq);

    /* 6. limites e erros */
    memset(buf, 'z', sizeof buf);
    n = send(b, buf, 2000, 0);
    check(n < 0 && errno == EMSGSIZE, "2000 bytes: EMSGSIZE", NULL);
    n = send(b, buf, 1472, 0);
    check(n == 1472, "1472 bytes passam", NULL);
    usleep(20000);
    n = recv(a, buf, sizeof buf, 0);
    check(n == 1472, "1472 bytes chegam inteiros", NULL);
    int dup = udp4_bound("127.0.0.1", pa);
    check(dup < 0 && errno == EADDRINUSE, "bind na mesma porta: EADDRINUSE", NULL);
    int d2 = socket(AF_INET, SOCK_DGRAM, 0);
    check(send(d2, "x", 1, 0) < 0 && errno == EDESTADDRREQ, "send sem destino: EDESTADDRREQ", NULL);
    struct timeval tv = {0, 200000};
    setsockopt(d2, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof tv);
    struct sockaddr_in any = {0};
    any.sin_len = sizeof any;
    any.sin_family = AF_INET;
    bind(d2, (struct sockaddr *)&any, sizeof any);
    n = recv(d2, buf, sizeof buf, 0);
    check(n < 0 && errno == EAGAIN, "SO_RCVTIMEO: EAGAIN no prazo", NULL);
    int type = 0;
    socklen_t tl = sizeof type;
    check(getsockopt(d2, SOL_SOCKET, SO_TYPE, &type, &tl) == 0 && type == SOCK_DGRAM, "SO_TYPE = SOCK_DGRAM", NULL);
    int on = 1;
    check(setsockopt(d2, SOL_SOCKET, SO_BROADCAST, &on, sizeof on) == 0, "SO_BROADCAST", NULL);
    close(d2);

    /* 7. IPv6 ::1 e dual-stack (socket AF_INET6 em :: recebe de IPv4 como ::ffff:127.0.0.1) */
    int s6 = socket(AF_INET6, SOCK_DGRAM, 0);
    struct sockaddr_in6 a6 = {0};
    a6.sin6_len = sizeof a6;
    a6.sin6_family = AF_INET6;
    check(s6 >= 0 && bind(s6, (struct sockaddr *)&a6, sizeof a6) == 0, "bind AF_INET6 em ::", NULL);
    unsigned short p6 = port_of(s6);
    int c6 = socket(AF_INET6, SOCK_DGRAM, 0);
    struct sockaddr_in6 t6 = {0};
    t6.sin6_len = sizeof t6;
    t6.sin6_family = AF_INET6;
    t6.sin6_port = htons(p6);
    t6.sin6_addr = in6addr_loopback;
    check(sendto(c6, "seis", 4, 0, (struct sockaddr *)&t6, sizeof t6) == 4, "sendto ::1", NULL);
    struct sockaddr_in6 f6;
    socklen_t f6l = sizeof f6;
    memset(buf, 0, sizeof buf);
    n = recvfrom(s6, buf, sizeof buf, 0, (struct sockaddr *)&f6, &f6l);
    check(n == 4 && f6.sin6_family == AF_INET6 && IN6_IS_ADDR_LOOPBACK(&f6.sin6_addr), "recvfrom de ::1", NULL);
    struct sockaddr_in t4 = loop4(p6);
    int c4 = socket(AF_INET, SOCK_DGRAM, 0);
    sendto(c4, "quatro", 6, 0, (struct sockaddr *)&t4, sizeof t4);
    f6l = sizeof f6;
    n = recvfrom(s6, buf, sizeof buf, 0, (struct sockaddr *)&f6, &f6l);
    check(n == 6 && IN6_IS_ADDR_V4MAPPED(&f6.sin6_addr), "dual-stack: IPv4 chega como ::ffff:127.0.0.1", NULL);
    on = 1;
    int v6o = socket(AF_INET6, SOCK_DGRAM, 0);
    setsockopt(v6o, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof on);
    struct sockaddr_in6 a6b = {0};
    a6b.sin6_len = sizeof a6b;
    a6b.sin6_family = AF_INET6;
    bind(v6o, (struct sockaddr *)&a6b, sizeof a6b);
    struct sockaddr_in t4b = loop4(port_of(v6o));
    sendto(c4, "nao", 3, 0, (struct sockaddr *)&t4b, sizeof t4b);
    fcntl(v6o, F_SETFL, O_NONBLOCK);
    usleep(20000);
    n = recv(v6o, buf, sizeof buf, 0);
    check(n < 0 && errno == EAGAIN, "IPV6_V6ONLY não recebe IPv4", NULL);
    close(s6); close(c6); close(c4); close(v6o);

    /* 8. rede de verdade: uma consulta DNS (registro A de example.com) ao servidor da rede */
    int dns = socket(AF_INET, SOCK_DGRAM, 0);
    struct sockaddr_in ds = {0};
    ds.sin_len = sizeof ds;
    ds.sin_family = AF_INET;
    ds.sin_port = htons(53);
    inet_pton(AF_INET, argc > 1 ? argv[1] : "10.0.2.3", &ds.sin_addr);
    unsigned char q[] = {0x12, 0x34, 1, 0, 0, 1, 0, 0, 0, 0, 0, 0, 7, 'e', 'x', 'a', 'm', 'p', 'l', 'e', 3, 'c', 'o', 'm', 0, 0, 1, 0, 1};
    struct timeval dtv = {5, 0};
    setsockopt(dns, SOL_SOCKET, SO_RCVTIMEO, &dtv, sizeof dtv);
    int got = 0;
    for (int tries = 0; tries < 3 && !got; tries++) {
        sendto(dns, q, sizeof q, 0, (struct sockaddr *)&ds, sizeof ds);
        unsigned char r[512];
        n = recv(dns, r, sizeof r, 0);
        if (n > 12 && r[0] == 0x12 && r[1] == 0x34 && (r[2] & 0x80) && (r[7] | r[6] << 8) > 0) got = 1;
    }
    check(got, "DNS por UDP na rede (resposta com registros)", NULL);
    close(dns);

    close(a);
    close(b);
    fprintf(stderr, "udptest: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
