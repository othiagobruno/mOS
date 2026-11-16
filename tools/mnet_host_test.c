/* Teste de host do núcleo mnet (docs/rede-apps.md): HTTP/1.1, cookies, datas e HPACK.
 *   gcc -std=c11 -Wall -Wextra -DMNET_HOST -I userland/libsystem userland/libsystem/mnet_*.c \
 *       tools/mnet_host_test.c -o /tmp/mnet_host_test && /tmp/mnet_host_test
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "mnet.h"
#include "mnet_hpack_vec.h"

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

int main(void) {
    t_h1();
    t_dates();
    t_cookies();
    t_hpack();
    printf("mnet: %d/%d ok\n", checks - fails, checks);
    return fails ? 1 : 0;
}
