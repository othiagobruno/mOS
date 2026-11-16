/* Transporte de host do mnet para os testes (TCP do Linux + TLS do motor do mOS, kernel/net/tls.c). */
/* ---- o que o tls.c pede do kernel */
u64 timer_ticks(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (u64)ts.tv_sec * 1000ull + (u64)ts.tv_nsec / 1000000ull; }
void rtc_now(datetime_t *dt) {
    time_t t = time(NULL); struct tm tm; gmtime_r(&t, &tm);
    dt->year = tm.tm_year + 1900; dt->month = tm.tm_mon + 1; dt->day = tm.tm_mday;
    dt->hour = tm.tm_hour; dt->minute = tm.tm_min; dt->second = tm.tm_sec; dt->weekday = tm.tm_wday;
}
void rtc_now_utc(datetime_t *dt) { rtc_now(dt); }
static int socks[64];
int tcp_state(int s) { return s >= 0 && s < 64 && socks[s] >= 0 ? TCP_OPEN : TCP_ERROR; }
int tcp_send(int s, const void *d, int n) { int k = (int)send(socks[s], d, (size_t)n, MSG_NOSIGNAL); return k < 0 ? (errno == EAGAIN ? 0 : -1) : k; }
int tcp_recv(int s, void *b, int n) { int k = (int)recv(socks[s], b, (size_t)n, 0); if (!k) return -1; return k < 0 ? (errno == EAGAIN ? 0 : -1) : k; }

/* ---- transporte de host para o mnet */
typedef struct { int fd, slot; tls_conn_t *tls; } hc_t;
static pthread_mutex_t mu = PTHREAD_MUTEX_INITIALIZER;
static long long now_ms(void *e) { (void)e; return (long long)timer_ticks(); }
static long long unix_t(void *e) { (void)e; return (long long)time(NULL); }
static void hlock(void *e) { (void)e; pthread_mutex_lock(&mu); }
static void hunlock(void *e) { (void)e; pthread_mutex_unlock(&mu); }
static int wait_fd(int fd, int ev, int ms) { struct pollfd p = {fd, (short)ev, 0}; return poll(&p, 1, ms); }
static void *h_connect(void *e, const char *host, int port, int tmo, int *err) {
    (void)e;
    char ps[8]; snprintf(ps, sizeof ps, "%d", port);
    struct addrinfo hint = {0}, *ai = 0;
    hint.ai_socktype = SOCK_STREAM;
    if (getaddrinfo(host, ps, &hint, &ai) || !ai) { *err = MNET_E_NO_HOST; return 0; }
    int fd = -1;
    for (struct addrinfo *a = ai; a && fd < 0; a = a->ai_next) {
        fd = socket(a->ai_family, SOCK_STREAM, 0);
        if (fd < 0) continue;
        fcntl(fd, F_SETFL, O_NONBLOCK);
        int r = connect(fd, a->ai_addr, a->ai_addrlen);
        if (r && errno == EINPROGRESS && wait_fd(fd, POLLOUT, tmo) == 1) {
            int so = 0; socklen_t sl = sizeof so; getsockopt(fd, SOL_SOCKET, SO_ERROR, &so, &sl); r = so ? -1 : 0;
        }
        if (r) { close(fd); fd = -1; }
    }
    freeaddrinfo(ai);
    if (fd < 0) { *err = MNET_E_CONNECT; return 0; }
    int one = 1; setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
    hc_t *c = calloc(1, sizeof *c);
    c->fd = fd; c->slot = -1;
    return c;
}
static int h_starttls(void *e, void *cv, const char *sni, const uint8_t *alpn, size_t al, char out[33], int tmo, char *msg, size_t ml) {
    (void)e;
    hc_t *c = cv;
    pthread_mutex_lock(&mu);
    for (int i = 0; i < 64; i++) if (socks[i] < 0) { socks[i] = c->fd; c->slot = i; break; }
    pthread_mutex_unlock(&mu);
    if (c->slot < 0) return MNET_E_TLS;
    c->tls = tls_new(c->slot, sni);
    if (alpn && al) tls_set_alpn(c->tls, alpn, (u32)al);
    long long t0 = now_ms(0);
    int r;
    while ((r = tls_handshake(c->tls)) == 0) {
        if (now_ms(0) - t0 > tmo) break;
        wait_fd(c->fd, POLLIN, 20);
    }
    if (r != 1) { snprintf(msg, ml, "%s", r < 0 ? tls_error(c->tls) : "tempo esgotado no TLS"); return MNET_E_TLS; }
    snprintf(out, 33, "%s", tls_get_alpn(c->tls));
    return 0;
}
static long h_read(void *e, void *cv, void *b, size_t n, int tmo) {
    (void)e;
    hc_t *c = cv;
    if (n > 16384) n = 16384;
    for (int pass = 0; pass < 2; pass++) {
        long k;
        if (c->tls) { k = tls_recv(c->tls, b, (int)n); if (k < 0) return 0; if (k > 0) return k; }
        else { k = recv(c->fd, b, n, 0); if (k >= 0) return k; if (errno != EAGAIN) return -1; }
        if (pass == 0 && wait_fd(c->fd, POLLIN, tmo) <= 0) return -2;
    }
    return -2;
}
static long h_write(void *e, void *cv, const void *b, size_t n, int tmo) {
    (void)e;
    hc_t *c = cv;
    for (;;) {
        long k = c->tls ? tls_send(c->tls, b, (int)(n > 16384 ? 16384 : n)) : send(c->fd, b, n, MSG_NOSIGNAL);
        if (k > 0) return k;
        if (k < 0 && (c->tls || errno != EAGAIN)) return -1;
        if (wait_fd(c->fd, POLLOUT, tmo) <= 0) return -2;
    }
}
static void h_close(void *e, void *cv) {
    (void)e;
    hc_t *c = cv;
    if (c->tls) tls_free(c->tls);
    pthread_mutex_lock(&mu);
    if (c->slot >= 0) socks[c->slot] = -1;
    pthread_mutex_unlock(&mu);
    close(c->fd);
    free(c);
}
static const mnet_io_t io = {0, h_connect, h_starttls, h_read, h_write, h_close, now_ms, unix_t, hlock, hunlock};

