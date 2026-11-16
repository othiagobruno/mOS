/* Teste de host do núcleo mnet (docs/rede-apps.md): HTTP/1.1, cookies, datas e HPACK.
 *   gcc -std=gnu11 -Wall -Wextra -DMNET_HOST -I userland/libsystem userland/libsystem/mnet_*.c \
 *       tools/mnet_host_test.c -lz -l:libbrotlidec.so.1 -o /tmp/mnet_host_test && /tmp/mnet_host_test
 */
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mnet.h"
#include "mnet_hpack_vec.h"
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FALHOU %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define CHECKS(a, b) do { const char *_a = (a), *_b = (b); checks++; \
    if (!_a || strcmp(_a, _b)) { fails++; printf("FALHOU %s:%d: \"%s\" != \"%s\"\n", __FILE__, __LINE__, _a ? _a : "(null)", _b); } } while (0)

/* ---- HTTP/1.1 */
typedef struct { mnet_buf_t body; int infos; } sink_t;
static int on_body(void *c, const uint8_t *d, size_t n) { return mnet_buf_put(&((sink_t *)c)->body, d, n); }
static void on_info(void *c, int st, const mnet_hdrs_t *h) { (void)h; if (st >= 100) ((sink_t *)c)->infos++; }

/* alimenta de `step` em `step` bytes; devolve o que sobrou depois do fim da mensagem */
static long feed_all(mnet_h1_t *p, const char *s, size_t step) {
    size_t n = strlen(s), i = 0;
    while (i < n) {
        size_t k = n - i < step ? n - i : step;
        long r = mnet_h1_feed(p, (const uint8_t *)s + i, k);
        if (r < 0) return -1;
        i += (size_t)r;
        if (p->state == MNET_H1_DONE) return (long)(n - i);
        if ((size_t)r < k) return -1;
    }
    return 0;
}
static void t_h1(void) {
    const char *msg =
        "HTTP/1.1 100 Continue\r\n\r\n"
        "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\nTransfer-Encoding: gzip, chunked\r\n"
        "X-Long: um\r\n  dois\r\n\r\n"
        "4;ext=1\r\nWiki\r\n5\r\npedia\r\nE\r\n in\r\n\r\nchunks.\r\n0\r\nX-Trailer: fim\r\n\r\n"
        "HTTP/1.1 204";                             /* começo da próxima resposta (pipeline) */
    for (size_t step = 1; step <= 7; step += 3) {
        sink_t s = {{0}, 0};
        mnet_h1_t p = {0};
        p.ctx = &s; p.on_body = on_body; p.on_info = on_info;
        mnet_h1_init(&p, 0);
        long left = feed_all(&p, msg, step);
        CHECK(left == 12);
        CHECK(p.status == 200 && p.chunked && p.keep_alive && s.infos == 1);
        CHECKS(p.reason, "OK");
        CHECKS(mnet_hdrs_get(&p.hdrs, "x-long"), "um dois");
        CHECKS(mnet_hdrs_get(&p.trailers, "X-TRAILER"), "fim");
        CHECKS((const char *)s.body.p, "Wikipedia in\r\n\r\nchunks.");
        mnet_h1_free(&p); mnet_buf_free(&s.body);
    }
    {   /* Content-Length, HTTP/1.0 sem keep-alive */
        sink_t s = {{0}, 0};
        mnet_h1_t p = {0}; p.ctx = &s; p.on_body = on_body;
        mnet_h1_init(&p, 0);
        CHECK(feed_all(&p, "HTTP/1.0 200 OK\r\nContent-Length: 5, 5\r\n\r\nolá!", 3) == 0);
        CHECK(p.state == MNET_H1_DONE && !p.keep_alive && p.length == 5);
        mnet_h1_free(&p); mnet_buf_free(&s.body);
    }
    {   /* fim por fechamento */
        sink_t s = {{0}, 0};
        mnet_h1_t p = {0}; p.ctx = &s; p.on_body = on_body;
        mnet_h1_init(&p, 0);
        feed_all(&p, "HTTP/1.1 200 OK\nConnection: close\n\nabc", 64);
        CHECK(p.state == MNET_H1_BODY_CLOSE && !p.keep_alive);
        CHECK(mnet_h1_eof(&p) == 0 && s.body.n == 3);
        mnet_h1_free(&p); mnet_buf_free(&s.body);
    }
    {   /* HEAD, 304, 101 */
        mnet_h1_t p = {0}; mnet_h1_init(&p, 1);
        CHECK(feed_all(&p, "HTTP/1.1 200 OK\r\nContent-Length: 1000\r\n\r\n", 5) == 0 && p.state == MNET_H1_DONE);
        mnet_h1_free(&p);
        mnet_h1_init(&p, 0);
        CHECK(feed_all(&p, "HTTP/1.1 304 Not Modified\r\nContent-Length: 10\r\n\r\n", 64) == 0 && p.state == MNET_H1_DONE);
        mnet_h1_free(&p);
        mnet_h1_init(&p, 0);
        CHECK(feed_all(&p, "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\n\r\n\x81\x02hi", 64) == 4 && p.upgrade);
        mnet_h1_free(&p);
    }
    {   /* erros */
        const char *bad[] = {
            "HTTP/1.1 200 OK\r\nContent-Length: 5, 6\r\n\r\n",
            "HTTP/1.1 200 OK\r\nX Bad: 1\r\n\r\n",
            "HTTP/1.1 200 OK\r\nBad : 1\r\n\r\n",
            "HTTP/2 200\r\n\r\n",
            "HTTP/1.1 200 OK\r\nTransfer-Encoding: chunked\r\n\r\nzz\r\n",
            "HTTP/1.1 200 OK\r\nX: a\rb\r\n\r\n",
        };
        for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
            mnet_h1_t p = {0}; mnet_h1_init(&p, 0);
            CHECK(feed_all(&p, bad[i], 64) < 0 && p.err);
            mnet_h1_free(&p);
        }
        mnet_h1_t p = {0}; mnet_h1_init(&p, 0);
        feed_all(&p, "HTTP/1.1 200 OK\r\nContent-Length: 10\r\n\r\nabc", 64);
        CHECK(mnet_h1_eof(&p) < 0);
        mnet_h1_free(&p);
    }
    {   /* pedido */
        mnet_hdrs_t h = {0};
        mnet_hdrs_add(&h, "Accept", 6, "*/*", 3);
        mnet_hdrs_add(&h, "X-Inj", 5, "a\r\nEvil: 1", 10);
        mnet_buf_t o = {0};
        mnet_h1_request(&o, "POST", "/api?q=1", "exemplo.com:8080", &h, 3);
        CHECKS((char *)o.p, "POST /api?q=1 HTTP/1.1\r\nHost: exemplo.com:8080\r\nAccept: */*\r\nX-Inj: a\r\nContent-Length: 3\r\n\r\n");
        o.n = 0;
        mnet_h1_request(&o, "GET", "/", "a", 0, 0);
        CHECKS((char *)o.p, "GET / HTTP/1.1\r\nHost: a\r\n\r\n");
        o.n = 0;
        mnet_h1_chunk(&o, "hello world, 26 bytes!!!!!", 26);
        mnet_h1_chunk(&o, 0, 0);
        CHECKS((char *)o.p, "1a\r\nhello world, 26 bytes!!!!!\r\n0\r\n\r\n");
        mnet_buf_free(&o); mnet_hdrs_free(&h);
    }
}

/* ---- datas */
static void t_dates(void) {
    CHECK(mnet_http_date("Sun, 06 Nov 1994 08:49:37 GMT", 29) == 784111777);
    const char *rfc850 = "Sunday, 06-Nov-94 08:49:37 GMT", *asc = "Sun Nov  6 08:49:37 1994";
    CHECK(mnet_http_date(rfc850, strlen(rfc850)) == 784111777);
    CHECK(mnet_http_date(asc, strlen(asc)) == 784111777);
    const char *odd = "Wed, 09-Jun-2021 10:18:14 GMT";
    CHECK(mnet_http_date(odd, strlen(odd)) == 1623233894);
    CHECK(mnet_http_date("Feb 30 2020 10:00:00", 20) == -1);
    CHECK(mnet_http_date("bogus", 5) == -1);
    char b[30];
    mnet_fmt_date(784111777, b);
    CHECKS(b, "Sun, 06 Nov 1994 08:49:37 GMT");
    mnet_fmt_date(1767225600, b);   /* 2026-01-01 */
    CHECKS(b, "Thu, 01 Jan 2026 00:00:00 GMT");
}

/* ---- cookies */
static void t_cookies(void) {
    long long now = 1763200000;   /* nov. 2025 */
    mnet_jar_t j; mnet_jar_init(&j);
    CHECK(mnet_jar_set(&j, "SID=31d4d96e407aad42; Path=/; Secure; HttpOnly", "https", "www.example.com", "/", 1, now));
    CHECK(mnet_jar_set(&j, "lang=en-US; Path=/; Domain=example.com", "https", "www.example.com", "/x", 1, now));
    CHECK(!mnet_jar_set(&j, "bad=1; Domain=com", "https", "www.example.com", "/", 1, now));          /* sufixo público */
    CHECK(!mnet_jar_set(&j, "bad=1; Domain=co.uk", "https", "a.b.co.uk", "/", 1, now));
    CHECK(!mnet_jar_set(&j, "bad=1; Domain=github.io", "https", "x.github.io", "/", 1, now));       /* regra privada */
    CHECK(!mnet_jar_set(&j, "bad=1; Domain=other.com", "https", "www.example.com", "/", 1, now));
    CHECK(!mnet_jar_set(&j, "bad=1; Secure", "http", "www.example.com", "/", 1, now));
    CHECK(!mnet_jar_set(&j, "h=1; HttpOnly", "https", "www.example.com", "/", 0, now));
    CHECK(!mnet_jar_set(&j, "__Host-x=1; Secure; Path=/; Domain=example.com", "https", "www.example.com", "/", 1, now));
    CHECK(mnet_jar_set(&j, "__Host-x=1; Secure; Path=/", "https", "www.example.com", "/", 1, now));
    CHECK(!mnet_jar_set(&j, "ss=1; SameSite=None", "https", "www.example.com", "/", 1, now));
    CHECK(mnet_jar_set(&j, "deep=1", "https", "www.example.com", "/a/b/c.html", 1, now));                /* caminho /a/b */
    CHECK(mnet_public_suffix("com") && mnet_public_suffix("co.uk") && mnet_public_suffix("com.br") && !mnet_public_suffix("example.com"));
    CHECK(mnet_public_suffix("foo.ck") && !mnet_public_suffix("www.ck"));   /* *.ck e !www.ck */

    char *h = mnet_jar_header(&j, "https", "www.example.com", "/a/b/d", 1, now);
    CHECKS(h, "deep=1; SID=31d4d96e407aad42; lang=en-US; __Host-x=1");
    free(h);
    h = mnet_jar_header(&j, "http", "sub.example.com", "/", 1, now);
    CHECKS(h, "lang=en-US");
    free(h);
    h = mnet_jar_header(&j, "https", "www.example.com", "/", 0, now);     /* API não-HTTP não vê HttpOnly */
    CHECKS(h, "lang=en-US; __Host-x=1");
    free(h);
    CHECK(!mnet_jar_header(&j, "https", "example.org", "/", 1, now));

    /* Max-Age e Expires; remoção; persistência */
    CHECK(mnet_jar_set(&j, "p=1; Max-Age=3600; Expires=Wed, 09 Jun 2001 10:18:14 GMT", "https", "example.com", "/", 1, now));
    CHECK(mnet_jar_set(&j, "e=2; Expires=Wed, 09 Jun 2100 10:18:14 GMT", "https", "example.com", "/", 1, now));
    h = mnet_jar_header(&j, "https", "example.com", "/", 1, now + 3599);
    CHECK(h && strstr(h, "p=1") && strstr(h, "e=2"));
    free(h);
    h = mnet_jar_header(&j, "https", "example.com", "/", 1, now + 3601);
    CHECK(h && !strstr(h, "p=1") && strstr(h, "e=2"));
    free(h);
    CHECK(!mnet_jar_set(&j, "e=; Max-Age=0", "https", "example.com", "/", 1, now));
    h = mnet_jar_header(&j, "https", "example.com", "/", 1, now);
    CHECK(!h || !strstr(h, "e="));
    free(h);
    /* origem insegura não troca cookie Secure */
    CHECK(!mnet_jar_set(&j, "SID=evil", "http", "www.example.com", "/", 1, now));

    CHECK(mnet_jar_set(&j, "keep=v; Max-Age=86400; HttpOnly; Domain=example.com", "https", "example.com", "/", 1, now));
    mnet_buf_t o = {0};
    mnet_jar_save(&j, &o, 0);
    CHECK(strstr((char *)o.p, "#HttpOnly_.example.com\tTRUE\t/\tFALSE\t") != 0);
    mnet_jar_t k; mnet_jar_init(&k);
    CHECK(mnet_jar_load(&k, (char *)o.p, o.n, now) == 1);
    h = mnet_jar_header(&k, "https", "a.example.com", "/", 1, now);
    CHECKS(h, "keep=v");
    free(h);
    mnet_buf_free(&o);
    mnet_jar_clear(&k); mnet_jar_clear(&j);

    CHECK(mnet_domain_match("a.b.c", "b.c") && !mnet_domain_match("ab.c", "b.c") && !mnet_domain_match("1.2.3.4", "2.3.4"));
    CHECK(mnet_path_match("/a/b", "/a") && mnet_path_match("/a/", "/a/") && !mnet_path_match("/ab", "/a"));
}

/* ---- HPACK */
static int unhex(const char *s, uint8_t *o) {
    int n = 0;
    for (; s[0] && s[1]; s += 2) { unsigned v; sscanf(s, "%2x", &v); o[n++] = (uint8_t)v; }
    return n;
}
static void t_hpack(void) {
    /* C.3/C.4: pedidos (tabela 4096); C.5/C.6: respostas com tabela de 256 */
    for (int sec = 3; sec <= 6; sec++) {
        mnet_hpack_t dec, enc;
        size_t tmax = sec >= 5 ? 256 : 4096;
        mnet_hpack_init(&dec, tmax);
        mnet_hpack_init(&enc, tmax);
        for (size_t v = 0; v < sizeof hvec / sizeof *hvec; v++) {
            if (hvec[v].sec != sec) continue;
            uint8_t raw[256];
            int n = unhex(hvec[v].hex, raw);
            mnet_hdrs_t h = {0};
            CHECK(mnet_hpack_decode(&dec, raw, (size_t)n, &h, 4096) == 0);
            CHECK(h.n == hvec[v].nh);
            for (int i = 0; i < h.n && i < hvec[v].nh; i++) {
                CHECKS(h.v[i].name, hvec[v].h[i][0]);
                CHECKS(h.v[i].value, hvec[v].h[i][1]);
            }
            /* o codificador produz os mesmos bytes da RFC (mesma estratégia: indexa todo campo novo) */
            mnet_buf_t o = {0};
            mnet_hpack_encode(&enc, &h, &o, sec == 4 || sec == 6);
            CHECK((int)o.n == n && !memcmp(o.p, raw, (size_t)n));
            if ((int)o.n != n || memcmp(o.p, raw, (size_t)n)) { printf("  C.%d.%d: %zu vs %d:", sec, hvec[v].n, o.n, n); for (size_t q = 0; q < o.n; q++) printf("%02x", o.p[q]); printf("\n"); }
            mnet_buf_free(&o);
            mnet_hdrs_free(&h);
        }
        CHECK(dec.size <= tmax);
        if (sec == 5 || sec == 6) CHECK(dec.size == 215 && dec.count == 3);   /* estado final da RFC */
        if (sec == 3 || sec == 4) CHECK(dec.size == 164 && dec.count == 3);
        mnet_hpack_free(&dec); mnet_hpack_free(&enc);
    }
    /* ida e volta com tabela trocando de tamanho e campo sensível */
    mnet_hpack_t e, d;
    mnet_hpack_init(&e, 4096); mnet_hpack_init(&d, 4096);
    for (int round = 0; round < 50; round++) {
        mnet_hdrs_t h = {0}, g = {0};
        char v[64];
        snprintf(v, sizeof v, "valor-%d-%s", round % 7, round % 3 ? "x" : "çãõ");
        mnet_hdrs_add(&h, ":method", 7, "GET", 3);
        mnet_hdrs_add(&h, ":path", 5, "/api/v1/itens", 13);
        mnet_hdrs_add(&h, "Authorization", 13, "Bearer abc", 10);
        mnet_hdrs_add(&h, "x-rodada", 8, v, strlen(v));
        if (round == 20) mnet_hpack_set_max(&e, 100);
        if (round == 21) { mnet_hpack_set_max(&e, 0); mnet_hpack_set_max(&e, 2048); }
        mnet_buf_t o = {0};
        CHECK(mnet_hpack_encode(&e, &h, &o, round & 1) == 0);
        CHECK(mnet_hpack_decode(&d, o.p, o.n, &g, 4096) == 0);
        CHECK(g.n == 4);
        for (int i = 0; i < g.n && i < 4; i++) CHECK(!strcmp(g.v[i].value, h.v[i].value) && mnet_ieq(g.v[i].name, h.v[i].name));
        CHECK(d.size == e.size && d.count == e.count);
        mnet_buf_free(&o); mnet_hdrs_free(&h); mnet_hdrs_free(&g);
    }
    mnet_hpack_free(&e); mnet_hpack_free(&d);
    /* erros: índice 0, índice fora da tabela, Huffman com EOS, atualização acima do limite, atualização no meio */
    const char *bad[] = {"80", "ff00", "7f00", "0081ff", "3fe21f", "823f00"};
    for (size_t i = 0; i < sizeof bad / sizeof *bad; i++) {
        uint8_t raw[16]; int n = unhex(bad[i], raw);
        mnet_hpack_t t; mnet_hpack_init(&t, 4096);
        mnet_hdrs_t h = {0};
        CHECK(mnet_hpack_decode(&t, raw, (size_t)n, &h, i == 4 ? 100 : 4096) < 0);
        mnet_hdrs_free(&h); mnet_hpack_free(&t);
    }
    /* Huffman: todos os bytes */
    uint8_t all[256];
    for (int i = 0; i < 256; i++) all[i] = (uint8_t)i;
    mnet_buf_t hb = {0}, back = {0};
    CHECK(mnet_huff_encode(all, 256, &hb) == 0 && hb.n == mnet_huff_len(all, 256));
    CHECK(mnet_huff_decode(hb.p, hb.n, &back) == 0 && back.n == 256 && !memcmp(back.p, all, 256));
    mnet_buf_free(&hb); mnet_buf_free(&back);
}

/* ---- URL (RFC 3986 §5.4) e Content-Encoding */
static void t_url(void) {
    const char *base = "http://a/b/c/d;p?q";
    const char *ex[][2] = {
        {"g:h", "g:h"}, {"g", "http://a/b/c/g"}, {"./g", "http://a/b/c/g"}, {"g/", "http://a/b/c/g/"},
        {"/g", "http://a/g"}, {"//g", "http://g"}, {"?y", "http://a/b/c/d;p?y"}, {"g?y", "http://a/b/c/g?y"},
        {"#s", "http://a/b/c/d;p?q#s"}, {"g#s", "http://a/b/c/g#s"}, {"g?y#s", "http://a/b/c/g?y#s"},
        {";x", "http://a/b/c/;x"}, {"g;x", "http://a/b/c/g;x"}, {"", "http://a/b/c/d;p?q"}, {".", "http://a/b/c/"},
        {"./", "http://a/b/c/"}, {"..", "http://a/b/"}, {"../", "http://a/b/"}, {"../g", "http://a/b/g"},
        {"../..", "http://a/"}, {"../../", "http://a/"}, {"../../g", "http://a/g"},
        {"../../../g", "http://a/g"}, {"../../../../g", "http://a/g"}, {"/./g", "http://a/g"}, {"/../g", "http://a/g"},
        {"g.", "http://a/b/c/g."}, {".g", "http://a/b/c/.g"}, {"g..", "http://a/b/c/g.."}, {"..g", "http://a/b/c/..g"},
        {"./../g", "http://a/b/g"}, {"./g/.", "http://a/b/c/g/"}, {"g/./h", "http://a/b/c/g/h"}, {"g/../h", "http://a/b/c/h"},
        {"g;x=1/./y", "http://a/b/c/g;x=1/y"}, {"g;x=1/../y", "http://a/b/c/y"},
    };
    for (size_t i = 0; i < sizeof ex / sizeof *ex; i++) {
        char *r = mnet_url_resolve(base, ex[i][0]);
        CHECKS(r, ex[i][1]);
        free(r);
    }
    mnet_url_t u;
    CHECK(mnet_url_parse("HTTPS://user:pw@Exemplo.COM.:8443/a b?x=1#frag", &u) == 0);
    CHECKS(u.scheme, "https"); CHECKS(u.host, "exemplo.com"); CHECK(u.port == 8443 && u.port_explicit);
    CHECKS(u.target, "/a%20b?x=1"); CHECKS(u.userinfo, "user:pw");
    char *au = mnet_url_authority(&u); CHECKS(au, "exemplo.com:8443"); free(au);
    mnet_url_free(&u);
    CHECK(mnet_url_parse("http://[::1]/", &u) == 0 && u.ipv6 && !strcmp(u.host, "::1") && u.port == 80);
    au = mnet_url_authority(&u); CHECKS(au, "[::1]"); free(au);
    mnet_url_free(&u);
    CHECK(mnet_url_parse("http://h?q", &u) == 0); CHECKS(u.target, "/?q"); mnet_url_free(&u);
    CHECK(mnet_url_parse("http://h:99999/", &u) < 0 && mnet_url_parse("nada", &u) < 0 && mnet_url_parse("http:///x", &u) < 0);
}
static int dec_sink(void *c, const uint8_t *d, size_t n) { return mnet_buf_put(c, d, n); }
static int run_dec(const char *ce, const char *file, mnet_buf_t *out, size_t step) {
    FILE *f = fopen(file, "rb");
    if (!f) return -2;
    uint8_t buf[1 << 16]; size_t n = fread(buf, 1, sizeof buf, f); fclose(f);
    mnet_dec_t *d = mnet_dec_new(ce, dec_sink, out);
    if (!d) return -3;
    int r = 0;
    for (size_t i = 0; i < n && !r; i += step) r = mnet_dec_feed(d, buf + i, n - i < step ? n - i : step);
    if (!r) r = mnet_dec_finish(d);
    mnet_dec_free(d);
    return r;
}
static void t_decode(void) {
    /* conteúdo de referência e versões comprimidas pelas ferramentas do host */
    FILE *f = fopen("/tmp/mnet_dec_src", "wb");
    for (int i = 0; i < 3000; i++) fprintf(f, "linha %d do corpo comprimido do mOS\n", i);
    fclose(f);
    int ok = system("python3 -I tools/mnet_dec_amostras.py") == 0;
    f = fopen("/tmp/mnet_dec_src", "rb");
    static uint8_t src[1 << 17]; size_t sn = fread(src, 1, sizeof src, f); fclose(f);
    const char *cases[][2] = {{"gzip", "/tmp/mnet_dec.gz"}, {"x-gzip", "/tmp/mnet_dec.gz"}, {"deflate", "/tmp/mnet_dec.zlib"},
                              {"deflate", "/tmp/mnet_dec.raw"}, {"br", "/tmp/mnet_dec.br"}};
    CHECK(ok);
    for (size_t i = 0; i < 5; i++) {
        for (size_t step = 1; step <= 5000; step *= 70) {
            mnet_buf_t o = {0};
            int r = run_dec(cases[i][0], cases[i][1], &o, step);
            if (r == -2 && i == 4) { printf("  (sem brotli no host: br não testado)\n"); break; }
            CHECK(r == 0 && o.n == sn && !memcmp(o.p, src, sn));
            mnet_buf_free(&o);
        }
    }
    mnet_buf_t o = {0};
    CHECK(run_dec("gzip", "/tmp/mnet_dec2.gz", &o, 999) == 0 && o.n == 2 * sn);   /* dois membros */
    mnet_buf_free(&o);
    CHECK(!mnet_dec_new("zstd", dec_sink, &o) && !mnet_dec_new("identity", dec_sink, &o));
    /* truncado */
    f = fopen("/tmp/mnet_dec.gz", "rb"); uint8_t g[1 << 16]; size_t gn = fread(g, 1, sizeof g, f); fclose(f);
    mnet_dec_t *d = mnet_dec_new("gzip", dec_sink, &o);
    CHECK(mnet_dec_feed(d, g, gn / 2) == 0 && mnet_dec_finish(d) < 0);
    mnet_dec_free(d); mnet_buf_free(&o);
    system("rm -f /tmp/mnet_dec_src /tmp/mnet_dec.gz /tmp/mnet_dec2.gz /tmp/mnet_dec.zlib /tmp/mnet_dec.raw /tmp/mnet_dec.br");
}

/* ---- cache HTTP (RFC 9111) */
static void hset(mnet_hdrs_t *h, const char *n, const char *v) { mnet_hdrs_add(h, n, strlen(n), v, strlen(v)); }
static void t_cache(void) {
    mnet_hdrs_t rq = {0}, rs = {0};
    long long t0 = 784111777;                    /* Sun, 06 Nov 1994 08:49:37 GMT */
    hset(&rs, "Date", "Sun, 06 Nov 1994 08:49:37 GMT");
    hset(&rs, "Cache-Control", "public, max-age=600");
    hset(&rs, "ETag", "\"v1\"");
    CHECK(mnet_cache_storable("GET", 200, &rq, &rs) && !mnet_cache_storable("POST", 200, &rq, &rs));
    CHECK(mnet_cache_fresh_left(&rs, 200, t0, t0, t0 + 100) == 500);
    CHECK(mnet_cache_fresh_left(&rs, 200, t0, t0, t0 + 700) < 0);
    hset(&rs, "Age", "550");
    CHECK(mnet_cache_fresh_left(&rs, 200, t0, t0, t0 + 10) == 40);   /* Age conta */
    mnet_hdrs_free(&rs);
    hset(&rs, "Date", "Sun, 06 Nov 1994 08:49:37 GMT");
    hset(&rs, "Expires", "Sun, 06 Nov 1994 09:49:37 GMT");
    CHECK(mnet_cache_fresh_left(&rs, 200, t0, t0, t0) == 3600);
    mnet_hdrs_free(&rs);
    hset(&rs, "Date", "Sun, 06 Nov 1994 08:49:37 GMT");
    hset(&rs, "Expires", "0");
    CHECK(mnet_cache_fresh_left(&rs, 200, t0, t0, t0) <= 0);              /* Expires inválido = vencido */
    mnet_hdrs_free(&rs);
    hset(&rs, "Date", "Sun, 06 Nov 1994 08:49:37 GMT");
    hset(&rs, "Last-Modified", "Thu, 27 Oct 1994 08:49:37 GMT");          /* 10 dias antes: heurística = 1 dia */
    CHECK(mnet_cache_fresh_left(&rs, 200, t0, t0, t0) == 86400);
    CHECK(mnet_cache_fresh_left(&rs, 302, t0, t0, t0) <= 0);
    mnet_hdrs_free(&rs);
    hset(&rs, "Cache-Control", "no-store");
    CHECK(!mnet_cache_storable("GET", 200, &rq, &rs));
    mnet_hdrs_free(&rs);
    hset(&rs, "Cache-Control", "no-cache, max-age=600");
    CHECK(mnet_cache_storable("GET", 200, &rq, &rs) && mnet_cache_fresh_left(&rs, 200, t0, t0, t0) <= 0 && mnet_cache_must_revalidate(&rs));
    mnet_hdrs_free(&rs);
    hset(&rs, "Vary", "*");
    CHECK(!mnet_cache_storable("GET", 200, &rq, &rs));
    mnet_hdrs_free(&rs);
    hset(&rq, "Cache-Control", "max-age=0");
    CHECK(mnet_cache_req_revalidate(&rq));
    mnet_hdrs_free(&rq);
    hset(&rq, "Pragma", "no-cache");
    CHECK(mnet_cache_req_revalidate(&rq));
    mnet_hdrs_free(&rq);
    /* validadores, 304 e Vary */
    hset(&rs, "ETag", "\"v1\""); hset(&rs, "Last-Modified", "Thu, 27 Oct 1994 08:49:37 GMT");
    hset(&rs, "Content-Length", "10"); hset(&rs, "Cache-Control", "max-age=1"); hset(&rs, "Vary", "Accept-Encoding, Accept-Language");
    mnet_hdrs_t cond = {0};
    CHECK(mnet_cache_validators(&rs, &cond) == 2 && !strcmp(mnet_hdrs_get(&cond, "if-none-match"), "\"v1\""));
    mnet_hdrs_t r304 = {0};
    hset(&r304, "Cache-Control", "max-age=900"); hset(&r304, "Content-Length", "0"); hset(&r304, "ETag", "\"v2\"");
    mnet_cache_merge_304(&rs, &r304);
    CHECKS(mnet_hdrs_get(&rs, "cache-control"), "max-age=900");
    CHECKS(mnet_hdrs_get(&rs, "content-length"), "10");
    CHECKS(mnet_hdrs_get(&rs, "etag"), "\"v2\"");
    hset(&rq, "Accept-Encoding", "gzip"); hset(&rq, "Accept-Language", "pt-BR");
    char *k1 = mnet_cache_vary_key(&rs, &rq);
    CHECKS(k1, "accept-encoding=gzip\naccept-language=pt-BR\n");
    mnet_hdrs_set(&rq, "Accept-Language", "en");
    char *k2 = mnet_cache_vary_key(&rs, &rq);
    CHECK(strcmp(k1, k2) != 0);
    free(k1); free(k2);
    mnet_hdrs_free(&rq); mnet_hdrs_free(&rs); mnet_hdrs_free(&cond); mnet_hdrs_free(&r304);
}

/* ---- HTTP/2: cliente e servidor do mnet ligados em memória */
typedef struct { mnet_h2_t *c; int is_srv; mnet_buf_t body; int status, closed, close_code, trailers, heads, pings; long long body_want; } peer_t;
static void h2_on_headers(void *ctx, mnet_h2_stream_t *st, const mnet_hdrs_t *h, int end) {
    peer_t *p = ctx;
    p->heads++;
    if (!p->is_srv) {
        const char *v = mnet_hdrs_get(h, ":status");
        if (!v) p->trailers++;
        else if (v[0] != '1') p->status = atoi(v);
        return;
    }
    /* servidor: responde ao GET com um corpo grande (passa das janelas) e trailers */
    (void)end;
    mnet_hdrs_t r = {0};
    mnet_hdrs_add(&r, ":status", 7, "103", 3);
    mnet_h2_respond(p->c, st, &r, 0);
    mnet_hdrs_free(&r);
    mnet_hdrs_add(&r, ":status", 7, "200", 3);
    char big[20000]; memset(big, 'v', sizeof big - 1); big[sizeof big - 1] = 0;
    mnet_hdrs_add(&r, "x-grande", 8, big, strlen(big));             /* força CONTINUATION */
    mnet_h2_respond(p->c, st, &r, 0);
    mnet_hdrs_free(&r);
    st->user = (void *)1;
}
static int h2_on_data(void *ctx, mnet_h2_stream_t *st, const uint8_t *d, size_t n, int end) {
    (void)st; (void)end;
    peer_t *p = ctx;
    return mnet_buf_put(&p->body, d, n);
}
static void h2_on_close(void *ctx, mnet_h2_stream_t *st, int code) { (void)st; peer_t *p = ctx; p->closed++; p->close_code = code; }
static void h2_on_ping(void *ctx, const uint8_t *o) { (void)o; ((peer_t *)ctx)->pings++; }
static int pump(mnet_h2_t *a, mnet_h2_t *b) {
    int moved = 0;
    if (a->out.n) { size_t n = a->out.n; uint8_t *cp = malloc(n); memcpy(cp, a->out.p, n); a->out.n = 0; mnet_h2_feed(b, cp, n); free(cp); moved = 1; }
    if (b->out.n) { size_t n = b->out.n; uint8_t *cp = malloc(n); memcpy(cp, b->out.p, n); b->out.n = 0; mnet_h2_feed(a, cp, n); free(cp); moved = 1; }
    return moved;
}
static void t_h2(void) {
    mnet_h2_t cl = {0}, sv = {0};
    peer_t pc = {&cl, 0, {0}, 0, 0, 0, 0, 0, 0, 0}, ps = {&sv, 1, {0}, 0, 0, 0, 0, 0, 0, 0};
    cl.ctx = &pc; cl.on_headers = h2_on_headers; cl.on_data = h2_on_data; cl.on_close = h2_on_close; cl.on_ping_ack = h2_on_ping;
    sv.ctx = &ps; sv.on_headers = h2_on_headers; sv.on_data = h2_on_data; sv.on_close = h2_on_close;
    CHECK(mnet_h2_init(&cl, 0) == 0 && mnet_h2_init(&sv, 1) == 0);
    mnet_hdrs_t h = {0};
    mnet_hdrs_add(&h, ":method", 7, "GET", 3); mnet_hdrs_add(&h, ":scheme", 7, "https", 5);
    mnet_hdrs_add(&h, ":authority", 10, "exemplo.com", 11); mnet_hdrs_add(&h, ":path", 5, "/", 1);
    mnet_h2_stream_t *st = mnet_h2_request(&cl, &h, 1);
    CHECK(st && st->id == 1);
    uint8_t ping[8] = "mOSping";
    mnet_h2_ping(&cl, ping);
    /* o servidor manda 9 MiB respeitando as janelas, depois trailers */
    size_t total = 9u << 20, sent = 0;
    uint8_t *blob = malloc(total);
    for (size_t i = 0; i < total; i++) blob[i] = (uint8_t)(i * 7);
    for (int round = 0; round < 100000; round++) {
        pump(&cl, &sv);
        mnet_h2_stream_t *ss = mnet_h2_find(&sv, 1);
        if (ss && ss->user && sent < total) {
            long k = mnet_h2_send_data(&sv, ss, blob + sent, total - sent, 0);
            if (k > 0) sent += (size_t)k;
            if (sent == total) {
                mnet_hdrs_t t = {0}; mnet_hdrs_add(&t, "x-soma", 6, "ok", 2);
                mnet_h2_respond(&sv, ss, &t, 1);
                mnet_hdrs_free(&t);
            }
        }
        if (!cl.out.n && !sv.out.n && pc.closed) break;
    }
    CHECK(sent == total && pc.body.n == total && !memcmp(pc.body.p, blob, total));
    CHECK(pc.status == 200 && pc.trailers == 1 && pc.heads == 3 && pc.closed == 1 && pc.close_code == 0);
    CHECK(pc.pings == 1 && !cl.dead && !sv.dead && cl.open_streams == 0 && sv.open_streams == 0);
    free(blob);
    /* RST e GOAWAY */
    st = mnet_h2_request(&cl, &h, 1);
    CHECK(st && st->id == 3);
    pump(&cl, &sv); pump(&cl, &sv);
    mnet_h2_reset(&sv, mnet_h2_find(&sv, 3), MNET_H2_CANCEL);
    while (pump(&cl, &sv)) {}
    CHECK(pc.closed == 2 && pc.close_code == MNET_H2_CANCEL);
    st = mnet_h2_request(&cl, &h, 1);
    mnet_h2_goaway(&sv, MNET_H2_NO_ERROR);
    /* o GOAWAY sai com last = 3: o fluxo 5 chega depois e é recusado do lado do cliente */
    pump(&cl, &sv);
    pump(&cl, &sv);
    CHECK(cl.goaway_recv && !mnet_h2_can_open(&cl) && pc.close_code == MNET_H2_REFUSED_STREAM);
    mnet_hdrs_free(&h);
    mnet_h2_free(&cl); mnet_h2_free(&sv);
    mnet_buf_free(&pc.body); mnet_buf_free(&ps.body);
    /* erros de conexão: primeiro quadro não é SETTINGS; PUSH_PROMISE com push desligado */
    mnet_h2_t x = {0};
    mnet_h2_init(&x, 0);
    const uint8_t ping0[] = {0, 0, 8, 6, 0, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8};
    CHECK(mnet_h2_feed(&x, ping0, sizeof ping0) < 0 && x.err_code == MNET_H2_PROTOCOL_ERROR);
    mnet_h2_free(&x);
    mnet_h2_init(&x, 0);
    const uint8_t set0[] = {0, 0, 0, 4, 0, 0, 0, 0, 0}, push[] = {0, 0, 4, 5, 4, 0, 0, 0, 1, 0, 0, 0, 2};
    CHECK(mnet_h2_feed(&x, set0, sizeof set0) == 9 && mnet_h2_feed(&x, push, sizeof push) < 0);
    mnet_h2_free(&x);
}

/* ---- HTTP/2 do mnet como servidor h2c contra o curl do host (nghttp2): interoperabilidade real */
static void t_h2_curl(void) {
    if (system("curl -V 2>/dev/null | grep -q HTTP2") != 0) { printf("  (curl sem HTTP/2: interoperabilidade não testada)\n"); return; }
    int ls = socket(AF_INET, SOCK_STREAM, 0), one = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in a; memset(&a, 0, sizeof a);
    a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(0x7f000001); a.sin_port = 0;
    bind(ls, (struct sockaddr *)&a, sizeof a); listen(ls, 1);
    socklen_t al = sizeof a; getsockname(ls, (struct sockaddr *)&a, &al);
    char cmd[512];
    snprintf(cmd, sizeof cmd, "curl -s --max-time 20 --http2-prior-knowledge -H 'X-Teste: ç' -d 'corpo-do-pedido' "
             "-o /tmp/mnet_h2_curl.out -w '%%{http_version} %%{http_code}' http://127.0.0.1:%d/eco > /tmp/mnet_h2_curl.w",
             ntohs(a.sin_port));
    pid_t pid = fork();
    if (!pid) { execl("/bin/sh", "sh", "-c", cmd, (char *)0); _exit(127); }
    int fd = accept(ls, 0, 0);
    mnet_h2_t sv = {0};
    peer_t ps = {&sv, 1, {0}, 0, 0, 0, 0, 0, 0, 0};
    sv.ctx = &ps; sv.on_data = h2_on_data; sv.on_close = h2_on_close;
    mnet_h2_init(&sv, 1);
    uint8_t buf[65536];
    int answered = 0;
    for (int it = 0; it < 2000 && fd >= 0; it++) {
        if (sv.out.n) { (void)!write(fd, sv.out.p, sv.out.n); sv.out.n = 0; }
        long n = read(fd, buf, sizeof buf);
        if (n <= 0) break;
        if (mnet_h2_feed(&sv, buf, (size_t)n) < 0) { printf("  servidor h2: %s\n", sv.err); break; }
        mnet_h2_stream_t *st = mnet_h2_find(&sv, 1);
        if (st && (st->state & 2) && !answered) {                     /* pedido inteiro chegou */
            mnet_hdrs_t r = {0};
            mnet_hdrs_add(&r, ":status", 7, "201", 3);
            mnet_hdrs_add(&r, "content-type", 12, "text/plain", 10);
            mnet_h2_respond(&sv, st, &r, 0);
            mnet_hdrs_free(&r);
            mnet_buf_t b = {0};
            mnet_buf_puts(&b, "eco:");
            mnet_buf_put(&b, ps.body.p, ps.body.n);
            for (int i = 0; i < 4000; i++) mnet_buf_puts(&b, "0123456789abcdef");   /* 64 KB: passa da janela inicial */
            size_t off = 0;
            while (off < b.n) {
                long k = mnet_h2_send_data(&sv, mnet_h2_find(&sv, 1), b.p + off, b.n - off, 1);
                if (k < 0) break;
                off += (size_t)k;
                if (sv.out.n) { (void)!write(fd, sv.out.p, sv.out.n); sv.out.n = 0; }
                if (off < b.n) {
                    n = read(fd, buf, sizeof buf);
                    if (n <= 0 || mnet_h2_feed(&sv, buf, (size_t)n) < 0) break;
                }
            }
            mnet_buf_free(&b);
            answered = 1;
        }
        if (answered && sv.out.n) { (void)!write(fd, sv.out.p, sv.out.n); sv.out.n = 0; }
    }
    if (fd >= 0) close(fd);
    close(ls);
    int stc = 0; waitpid(pid, &stc, 0);
    char w[64] = {0};
    FILE *f = fopen("/tmp/mnet_h2_curl.w", "r");
    if (f) { if (!fgets(w, sizeof w, f)) w[0] = 0; fclose(f); }
    CHECKS(w, "2 201");
    f = fopen("/tmp/mnet_h2_curl.out", "rb");
    char body[80] = {0}; long len = 0;
    if (f) { len = (long)fread(body, 1, 19, f); fseek(f, 0, SEEK_END); len = ftell(f); fclose(f); }
    CHECK(!strncmp(body, "eco:corpo-do-pedido", 19) && len == 19 + 64000);
    unlink("/tmp/mnet_h2_curl.out"); unlink("/tmp/mnet_h2_curl.w");
    mnet_h2_free(&sv); mnet_buf_free(&ps.body);
}

/* ---- WebSocket (RFC 6455, RFC 7692) */
typedef struct { mnet_buf_t got; int op, n_msgs, closed, code, pings, pongs; } wsr_t;
static int ws_msg(void *c, int op, const uint8_t *d, size_t n) { wsr_t *r = c; r->op = op; r->n_msgs++; r->got.n = 0; mnet_buf_put(&r->got, d, n); mnet_buf_putc(&r->got, 0); r->got.n--; return 0; }
static void ws_cls(void *c, int code, const char *why) { (void)why; wsr_t *r = c; r->closed++; r->code = code; }
static void ws_ping(void *c, const uint8_t *d, size_t n) { (void)d; (void)n; ((wsr_t *)c)->pings++; }
static void ws_pong(void *c, const uint8_t *d, size_t n) { (void)d; (void)n; ((wsr_t *)c)->pongs++; }
static void ws_rand(void *c, uint8_t *b, size_t n) { (void)c; for (size_t i = 0; i < n; i++) b[i] = (uint8_t)(rand() & 0xff); }
static void ws_setup(mnet_ws_t *w, wsr_t *r, int srv) {
    mnet_buf_free(&r->got);
    memset(w, 0, sizeof *w); memset(r, 0, sizeof *r);
    w->ctx = r; w->on_message = ws_msg; w->on_close = ws_cls; w->on_ping = ws_ping; w->on_pong = ws_pong; w->random = ws_rand;
    mnet_ws_init(w, srv);
}
static void ws_pump(mnet_ws_t *a, mnet_ws_t *b) {
    for (int k = 0; k < 4; k++) {
        if (a->out.n) { mnet_buf_t t = a->out; memset(&a->out, 0, sizeof a->out); mnet_ws_feed(b, t.p, t.n); mnet_buf_free(&t); }
        if (b->out.n) { mnet_buf_t t = b->out; memset(&b->out, 0, sizeof b->out); mnet_ws_feed(a, t.p, t.n); mnet_buf_free(&t); }
    }
}
static void t_ws(void) {
    uint8_t h[20]; char hx[41];
    mnet_sha1("abc", 3, h); for (int i = 0; i < 20; i++) sprintf(hx + 2 * i, "%02x", h[i]);
    CHECKS(hx, "a9993e364706816aba3e25717850c26c9cd0d89d");
    const char *m56 = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    mnet_sha1(m56, strlen(m56), h); for (int i = 0; i < 20; i++) sprintf(hx + 2 * i, "%02x", h[i]);
    CHECKS(hx, "84983e441c3bd26ebaae4aa1f95129e5e54670f1");
    char acc[29]; mnet_ws_accept("dGhlIHNhbXBsZSBub25jZQ==", acc);
    CHECKS(acc, "s3pPLMBiTxaQ9kYGzzhZRbK+xOo=");
    /* §5.7: "Hello" sem máscara (servidor -> cliente), com máscara (cliente -> servidor), fragmentado, ping */
    mnet_ws_t w; wsr_t r = {{0}, 0, 0, 0, 0, 0, 0};
    ws_setup(&w, &r, 0);
    const uint8_t f1[] = {0x81, 0x05, 'H', 'e', 'l', 'l', 'o'};
    CHECK(mnet_ws_feed(&w, f1, sizeof f1) == 7 && r.n_msgs == 1 && r.op == 1 && !strcmp((char *)r.got.p, "Hello"));
    const uint8_t f3[] = {0x01, 0x03, 'H', 'e', 'l', 0x89, 0x05, 'H', 'e', 'l', 'l', 'o', 0x80, 0x02, 'l', 'o'};
    for (size_t i = 0; i < sizeof f3; i++) mnet_ws_feed(&w, f3 + i, 1);          /* byte a byte, ping no meio */
    CHECK(r.n_msgs == 2 && !strcmp((char *)r.got.p, "Hello") && r.pings == 1);
    CHECK(w.out.n == 2 + 4 + 5 && w.out.p[0] == 0x8a && w.out.p[1] == (0x80 | 5));  /* pong mascarado */
    mnet_ws_free(&w);
    ws_setup(&w, &r, 1);
    const uint8_t f2[] = {0x81, 0x85, 0x37, 0xfa, 0x21, 0x3d, 0x7f, 0x9f, 0x4d, 0x51, 0x58};
    CHECK(mnet_ws_feed(&w, f2, sizeof f2) == 11 && !strcmp((char *)r.got.p, "Hello"));
    CHECK(mnet_ws_feed(&w, f1, sizeof f1) < 0 && w.close_code == 1002);              /* cliente sem máscara */
    mnet_ws_free(&w);
    /* RFC 7692 §7.2.3: "Hello" comprimido, e o segundo com contexto */
    ws_setup(&w, &r, 0); w.deflate = 1;
    const uint8_t c1[] = {0xc1, 0x07, 0xf2, 0x48, 0xcd, 0xc9, 0xc9, 0x07, 0x00}, c2[] = {0xc1, 0x05, 0xf2, 0x00, 0x11, 0x00, 0x00};
    CHECK(mnet_ws_feed(&w, c1, sizeof c1) == 9 && !strcmp((char *)r.got.p, "Hello"));
    CHECK(mnet_ws_feed(&w, c2, sizeof c2) == 7 && !strcmp((char *)r.got.p, "Hello") && r.n_msgs == 2);
    mnet_ws_free(&w);
    /* erros */
    const uint8_t bad_utf[] = {0x81, 0x02, 0xc3, 0x28}, bad_code[] = {0x88, 0x02, 0x03, 0xed}, rsv[] = {0xa1, 0x00},
                  big_ping[] = {0x89, 0x7e, 0x00, 0x7e}, cont0[] = {0x80, 0x00}, op3[] = {0x83, 0x00};
    const uint8_t *bads[] = {bad_utf, bad_code, rsv, big_ping, cont0, op3};
    size_t blen[] = {4, 4, 2, 4, 2, 2}; int codes[] = {1007, 1002, 1002, 1002, 1002, 1002};
    for (int i = 0; i < 6; i++) { ws_setup(&w, &r, 0); CHECK(mnet_ws_feed(&w, bads[i], blen[i]) < 0 && w.close_code == codes[i]); mnet_ws_free(&w); }
    /* handshake e ida e volta com deflate, mensagem de 1 MiB e close */
    mnet_ws_t cl, sv; wsr_t rc = {{0}, 0, 0, 0, 0, 0, 0}, rs = {{0}, 0, 0, 0, 0, 0, 0};
    ws_setup(&cl, &rc, 0); ws_setup(&sv, &rs, 1);
    uint8_t rnd[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16}; char key[25];
    mnet_ws_key(rnd, key);
    mnet_buf_t rq = {0};
    mnet_ws_request(&rq, "/chat", "exemplo.com", "https://exemplo.com", key, "v2.chat, v1.chat", 0, 1);
    /* o servidor lê o pedido como cabeçalhos */
    mnet_hdrs_t hh = {0};
    char *line = strstr((char *)rq.p, "\r\n") + 2;
    while (*line && strncmp(line, "\r\n", 2)) { char *e = strstr(line, "\r\n"), *col = strchr(line, ':'); mnet_hdrs_add(&hh, line, (size_t)(col - line), col + 2, (size_t)(e - col - 2)); line = e + 2; }
    const char *sup[] = {"v1.chat", 0};
    mnet_buf_t resp = {0}; char *sproto = 0, *cproto = 0;
    CHECK(mnet_ws_server_accept(&sv, &hh, sup, &resp, &sproto) == 0 && sproto && !strcmp(sproto, "v1.chat") && sv.deflate);
    mnet_h1_t p = {0}; mnet_h1_init(&p, 0);
    mnet_h1_feed(&p, resp.p, resp.n);
    const char *why = 0;
    CHECK(mnet_ws_client_accept(&cl, &p, key, "v2.chat, v1.chat", &cproto, &why) == 0 && cproto && !strcmp(cproto, "v1.chat") && cl.deflate);
    mnet_h1_free(&p); mnet_hdrs_free(&hh); mnet_buf_free(&rq); mnet_buf_free(&resp); free(sproto); free(cproto);
    size_t big = 1u << 20;
    char *blob = malloc(big + 1);
    for (size_t i = 0; i < big; i++) blob[i] = "abcdefgh ç"[i % 10] == (char)0xc3 ? 'x' : "abcdefgh x"[i % 10];
    blob[big] = 0;
    for (int k = 0; k < 3; k++) { mnet_ws_send(&cl, MNET_WS_TEXT, blob, big); ws_pump(&cl, &sv); }
    CHECK(rs.n_msgs == 3 && rs.got.n == big && !memcmp(rs.got.p, blob, big));
    mnet_ws_send(&sv, MNET_WS_BINARY, "\x00\x01\x02", 3); mnet_ws_ping(&sv, "p", 1); ws_pump(&cl, &sv);
    CHECK(rc.n_msgs == 1 && rc.op == 2 && rc.got.n == 3 && rc.pings == 1 && rs.pongs == 1);
    mnet_ws_close(&cl, 1000, "tchau"); ws_pump(&cl, &sv);
    CHECK(rs.closed == 1 && rs.code == 1000 && rc.closed == 1 && rc.code == 1000 && cl.close_recv && sv.close_sent);
    free(blob);
    mnet_ws_free(&cl); mnet_ws_free(&sv); mnet_buf_free(&rc.got); mnet_buf_free(&rs.got); mnet_buf_free(&r.got);
}

/* servidor WebSocket do mnet contra o cliente WebSocket do Node (undici) */
static void t_ws_node(void) {
    if (system("node -e 'process.exit(typeof WebSocket===\"function\"?0:1)' 2>/dev/null")) { printf("  (sem node com WebSocket: interoperabilidade não testada)\n"); return; }
    int ls = socket(AF_INET, SOCK_STREAM, 0), one = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    struct sockaddr_in a; memset(&a, 0, sizeof a);
    a.sin_family = AF_INET; a.sin_addr.s_addr = htonl(0x7f000001);
    bind(ls, (struct sockaddr *)&a, sizeof a); listen(ls, 1);
    socklen_t al = sizeof a; getsockname(ls, (struct sockaddr *)&a, &al);
    char js[900];
    snprintf(js, sizeof js,
             "const w=new WebSocket('ws://127.0.0.1:%d/eco',['v1.chat']);let n=0;"
             "w.onopen=()=>{w.send('olá mOS');w.send(new Uint8Array(100000).fill(7));};"
             "w.onmessage=async e=>{n++;const v=typeof e.data==='string'?e.data:(await e.data.arrayBuffer()).byteLength;"
             "console.log('msg',v);if(n==2)w.close(1000,'fim');};"
             "w.onclose=e=>{console.log('close',e.code,w.protocol,w.extensions);};w.onerror=e=>console.log('erro');",
             ntohs(a.sin_port));
    FILE *jf = fopen("/tmp/mnet_ws_node.js", "w"); fputs(js, jf); fclose(jf);
    pid_t pid = fork();
    if (!pid) { int fd = open("/tmp/mnet_ws_node.out", O_WRONLY | O_CREAT | O_TRUNC, 0644); dup2(fd, 1); execlp("timeout", "timeout", "20", "node", "/tmp/mnet_ws_node.js", (char *)0); _exit(127); }
    int fd = accept(ls, 0, 0);
    mnet_h1_t rq = {0};
    uint8_t buf[65536];
    /* lê o pedido como se fosse uma resposta HTTP: troca a primeira linha */
    mnet_buf_t head = {0};
    while (!strstr(head.p ? (char *)head.p : "", "\r\n\r\n")) { long k = read(fd, buf, sizeof buf); if (k <= 0) break; mnet_buf_put(&head, buf, (size_t)k); }
    mnet_hdrs_t hh = {0};
    char *line = strstr((char *)head.p, "\r\n") + 2;
    while (*line && strncmp(line, "\r\n", 2)) { char *e = strstr(line, "\r\n"), *col = strchr(line, ':'); if (col && col < e) mnet_hdrs_add(&hh, line, (size_t)(col - line), col + 2, (size_t)(e - col - 2)); line = e + 2; }
    (void)rq;
    mnet_ws_t sv; wsr_t rs = {{0}, 0, 0, 0, 0, 0, 0}; ws_setup(&sv, &rs, 1);
    const char *sup[] = {"v1.chat", 0};
    mnet_buf_t resp = {0}; char *proto = 0;
    CHECK(mnet_ws_server_accept(&sv, &hh, sup, &resp, &proto) == 0);
    int had_ext = mnet_hdrs_get(&hh, "sec-websocket-extensions") != 0;
    (void)!write(fd, resp.p, resp.n);
    int echoed = 0;
    for (int it = 0; it < 200 && !rs.closed; it++) {
        long k = read(fd, buf, sizeof buf);
        if (k <= 0) break;
        mnet_ws_feed(&sv, buf, (size_t)k);
        while (rs.n_msgs > echoed) { mnet_ws_send(&sv, rs.op, rs.got.p, rs.got.n); echoed++; }
        if (sv.out.n) { (void)!write(fd, sv.out.p, sv.out.n); sv.out.n = 0; }
    }
    if (sv.out.n) (void)!write(fd, sv.out.p, sv.out.n);
    close(fd); close(ls);
    int st; waitpid(pid, &st, 0);
    char out[512] = {0};
    FILE *f = fopen("/tmp/mnet_ws_node.out", "r"); size_t on = f ? fread(out, 1, sizeof out - 1, f) : 0; if (f) fclose(f); out[on] = 0;
    printf("  node: %s", out);
    CHECK(strstr(out, "msg olá mOS") && strstr(out, "msg 100000") && strstr(out, "close 1000 v1.chat"));
    CHECK(!had_ext || sv.deflate);
    if (had_ext) CHECK(strstr(out, "permessage-deflate") != 0);
    unlink("/tmp/mnet_ws_node.js"); unlink("/tmp/mnet_ws_node.out");
    mnet_ws_free(&sv); mnet_buf_free(&rs.got); mnet_buf_free(&resp); mnet_buf_free(&head); mnet_hdrs_free(&hh); free(proto);
}

/* ---- proxy */
static void t_proxy(void) {
    mnet_proxy_cfg_t c; memset(&c, 0, sizeof c);
    c.http_on = c.https_on = 1;
    strcpy(c.http_host, "px.local"); c.http_port = 3128;
    strcpy(c.https_host, "pxs.local"); c.https_port = 3129;
    mnet_proxy_add_exception(&c, "*.local"); mnet_proxy_add_exception(&c, "169.254/16");
    mnet_proxy_add_exception(&c, "intranet.exemplo.com"); mnet_proxy_add_exception(&c, "10.0.0.0/8"); mnet_proxy_add_exception(&c, "192.168.1");
    mnet_proxy_t o[8];
    int n = mnet_proxy_for_url(&c, "http://www.apple.com/", o, 8);
    CHECK(n == 2 && o[0].type == MNET_PX_HTTP && !strcmp(o[0].host, "px.local") && o[0].port == 3128 && o[1].type == MNET_PX_NONE);
    n = mnet_proxy_for_url(&c, "https://www.apple.com/", o, 8);
    CHECK(n == 2 && o[0].type == MNET_PX_HTTPS && o[0].port == 3129);
    const char *direct[] = {"http://printer.local/", "http://169.254.3.4/", "http://a.intranet.exemplo.com/", "http://10.9.8.7/",
                            "http://192.168.1.20/", "http://localhost:8080/", "http://127.0.0.1/", "http://[::1]/"};
    for (int i = 0; i < 8; i++) { n = mnet_proxy_for_url(&c, direct[i], o, 8); CHECK(n == 1 && o[0].type == MNET_PX_NONE); }
    n = mnet_proxy_for_url(&c, "http://192.168.2.1/", o, 8);
    CHECK(o[0].type == MNET_PX_HTTP);
    CHECK(!mnet_proxy_bypass(&c, "servidor"));
    c.exclude_simple = 1;
    CHECK(mnet_proxy_bypass(&c, "servidor"));
    c.socks_on = 1; strcpy(c.socks_host, "sk"); c.socks_port = 0;
    c.pac_on = 1; c.pac_url = mnet_strndup("http://wpad/wpad.dat", 20);
    n = mnet_proxy_for_url(&c, "http://exemplo.org/", o, 8);
    CHECK(n == 4 && o[0].type == MNET_PX_PAC_URL && o[1].type == MNET_PX_HTTP && o[2].type == MNET_PX_SOCKS && o[2].port == 1080);
    mnet_proxy_cfg_free(&c);
    n = mnet_proxy_parse_pac_result("PROXY p1:8080; SOCKS5 s1:1081;HTTPS h1:443 ; DIRECT", o, 8);
    CHECK(n == 4 && o[0].type == MNET_PX_HTTP && !strcmp(o[0].host, "p1") && o[0].port == 8080 && o[1].type == MNET_PX_SOCKS &&
          o[1].port == 1081 && o[2].type == MNET_PX_HTTPS && o[3].type == MNET_PX_NONE);
    n = mnet_proxy_parse_pac_result("PROXY [2001:db8::1]:3128", o, 8);
    CHECK(n == 1 && !strcmp(o[0].host, "2001:db8::1") && o[0].port == 3128);
    CHECK(mnet_proxy_parse_pac_result("", o, 8) == 1 && o[0].type == MNET_PX_NONE);
}

int main(void) {
    t_h1();
    t_dates();
    t_cookies();
    t_hpack();
    t_url();
    t_proxy();
    t_cache();
    t_decode();
    t_ws();
    t_ws_node();
    t_h2();
    t_h2_curl();
    printf("mnet: %d/%d ok\n", checks - fails, checks);
    return fails ? 1 : 0;
}
