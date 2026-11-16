/* Teste de host da libcurl do mOS (userland/libsystem/libcurl.c) sobre o mnet e o TLS do motor do mOS.
 *   gcc -std=gnu11 -O1 -g -Wall -DMNET_HOST -DMOS_HOST_TEST -I userland/libsystem -I kernel/include -I tools \
 *       userland/libsystem/mnet_*.c userland/libsystem/libcurl.c tools/libcurl_mnet_test.c kernel/net/crypto.c \
 *       kernel/net/tls.c kernel/net/pk.c kernel/net/x509.c -lz -l:libbrotlidec.so.1 -lpthread -o /tmp/libcurl_mnet_test
 *   /tmp/libcurl_mnet_test [rede]
 */
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>
#include "tls_shim.h"
#include "x509.h"
#include "mnet.h"

static int fails, checks;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FALHOU %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#include "mnet_host_io.inc"

static mnet_client_t g_cl;
static int g_cl_ready;
mnet_client_t *cx_mnet_client(void) { if (!g_cl_ready) { mnet_client_init(&g_cl, &io); g_cl_ready = 1; } return &g_cl; }

/* API da libcurl usada aqui (sem o curl.h do host) */
typedef void CURL;
struct curl_slist;
typedef long long curl_off_t;
struct curl_slist *curl_slist_append(struct curl_slist *, const char *);
void curl_slist_free_all(struct curl_slist *);
CURL *curl_easy_init(void); void curl_easy_cleanup(CURL *); int curl_easy_setopt(CURL *, int, ...); int curl_easy_perform(CURL *);
int curl_easy_getinfo(CURL *, int, ...); void curl_easy_reset(CURL *); char *curl_easy_escape(CURL *, const char *, int);
char *curl_easy_unescape(CURL *, const char *, int, int *); void curl_free(void *);
void *curl_multi_init(void); int curl_multi_add_handle(void *, CURL *); int curl_multi_perform(void *, int *);
int curl_multi_wait(void *, void *, unsigned, int, int *); void *curl_multi_info_read(void *, int *);
int curl_multi_remove_handle(void *, CURL *); int curl_multi_cleanup(void *);
void *curl_url(void); int curl_url_set(void *, int, const char *, unsigned); int curl_url_get(void *, int, char **, unsigned); void curl_url_cleanup(void *);
void *curl_mime_init(CURL *); void *curl_mime_addpart(void *); int curl_mime_name(void *, const char *); int curl_mime_data(void *, const char *, size_t);
int curl_mime_filename(void *, const char *); void curl_mime_free(void *);
enum { O_URL = 10002, O_WRITEFN = 20011, O_WRITEDATA = 10001, O_HEADERFN = 20079, O_HEADERDATA = 10029, O_POSTFIELDS = 10015,
       O_COPYPOSTFIELDS = 10165, O_FOLLOW = 52, O_MAXREDIRS = 68, O_ACCEPT_ENCODING = 10102, O_COOKIEFILE = 10031, O_COOKIEJAR = 10082,
       O_FAILONERROR = 45, O_USERPWD = 10005, O_HTTPAUTH = 107, O_TIMEOUT_MS = 155, O_UPLOAD = 46, O_READFN = 20012, O_READDATA = 10009,
       O_INFILESIZE = 14, O_CUSTOM = 10036, O_HTTPHEADER = 10023, O_NOBODY = 44, O_MIMEPOST = 10269, O_PRIVATE = 10103,
       O_ERRORBUFFER = 10010, O_XFERFN = 20219, O_NOPROGRESS = 43, O_USERAGENT = 10018 };
enum { I_RESPONSE_CODE = 0x200002, I_EFFECTIVE_URL = 0x100001, I_CONTENT_TYPE = 0x100012, I_SIZE_DL_T = 0x600008,
       I_REDIRECT_COUNT = 0x200014, I_REDIRECT_URL = 0x10001f, I_HTTP_VERSION = 0x20002e, I_PRIVATE = 0x100015 };

typedef struct { char *d; size_t n; pthread_t th; int bad_thread; } buf_t;
static size_t wr(char *p, size_t s, size_t n, void *u) {
    buf_t *b = u;
    if (b->th && !pthread_equal(b->th, pthread_self())) b->bad_thread = 1;
    b->d = realloc(b->d, b->n + s * n + 1);
    memcpy(b->d + b->n, p, s * n); b->n += s * n; b->d[b->n] = 0;
    return s * n;
}
static size_t rd(char *p, size_t s, size_t n, void *u) {
    const char **src = u;
    size_t l = strlen(*src), k = l < s * n ? l : s * n;
    memcpy(p, *src, k); *src += k;
    return k;
}
static int xfer_calls;
static int xfer(void *u, curl_off_t dt, curl_off_t dn, curl_off_t ut, curl_off_t un) { (void)u; (void)dt; (void)dn; (void)ut; (void)un; xfer_calls++; return 0; }
static void reset_buf(buf_t *b) { free(b->d); memset(b, 0, sizeof *b); }

static void local(int port) {
    char u[160];
#define URL(p) (snprintf(u, sizeof u, "http://127.0.0.1:%d%s", port, p), u)
    CURL *e = curl_easy_init();
    buf_t b = {0}, h = {0};
    curl_easy_setopt(e, O_URL, URL("/ola"));
    curl_easy_setopt(e, O_WRITEFN, wr); curl_easy_setopt(e, O_WRITEDATA, &b);
    curl_easy_setopt(e, O_HEADERFN, wr); curl_easy_setopt(e, O_HEADERDATA, &h);
    curl_easy_setopt(e, O_XFERFN, xfer); curl_easy_setopt(e, O_NOPROGRESS, 0L);
    CHECK(curl_easy_perform(e) == 0 && b.d && !strcmp(b.d, "ola mundo"));
    long code = 0; curl_easy_getinfo(e, I_RESPONSE_CODE, &code);
    curl_off_t dl = 0; curl_easy_getinfo(e, I_SIZE_DL_T, &dl);
    CHECK(code == 200 && dl == 9 && h.d && !strncmp(h.d, "HTTP/1.1 200 OK\r\n", 17) && strstr(h.d, "\r\n\r\n") && xfer_calls >= 1);
    reset_buf(&b); reset_buf(&h);
    /* POST e PUT por READFUNCTION */
    curl_easy_setopt(e, O_URL, URL("/eco")); curl_easy_setopt(e, O_COPYPOSTFIELDS, "a=1&b=2");
    CHECK(curl_easy_perform(e) == 0 && b.d && !strcmp(b.d, "eco:POST:a=1&b=2"));
    reset_buf(&b);
    curl_easy_reset(e);
    const char *src = "conteudo enviado";
    curl_easy_setopt(e, O_URL, URL("/eco")); curl_easy_setopt(e, O_WRITEFN, wr); curl_easy_setopt(e, O_WRITEDATA, &b);
    curl_easy_setopt(e, O_CUSTOM, "POST"); curl_easy_setopt(e, O_UPLOAD, 1L); curl_easy_setopt(e, O_READFN, rd); curl_easy_setopt(e, O_READDATA, &src);
    curl_easy_setopt(e, O_INFILESIZE, (long)strlen(src));
    CHECK(curl_easy_perform(e) == 0 && b.d && !strcmp(b.d, "eco:POST:conteudo enviado"));
    reset_buf(&b);
    curl_easy_reset(e);
    curl_easy_setopt(e, O_WRITEFN, wr); curl_easy_setopt(e, O_WRITEDATA, &b);
    /* redirects */
    curl_easy_setopt(e, O_URL, URL("/redir/3")); curl_easy_setopt(e, O_FOLLOW, 1L);
    long rc = 0; char *eff = 0;
    CHECK(curl_easy_perform(e) == 0 && b.d && !strcmp(b.d, "ola mundo"));
    curl_easy_getinfo(e, I_REDIRECT_COUNT, &rc); curl_easy_getinfo(e, I_EFFECTIVE_URL, &eff);
    CHECK(rc == 3 && eff && strstr(eff, "/ola"));
    reset_buf(&b);
    curl_easy_setopt(e, O_MAXREDIRS, 2L);
    CHECK(curl_easy_perform(e) == 47);
    reset_buf(&b);
    curl_easy_setopt(e, O_FOLLOW, 0L);
    char *ru = 0;
    CHECK(curl_easy_perform(e) == 0);
    curl_easy_getinfo(e, I_RESPONSE_CODE, &code); curl_easy_getinfo(e, I_REDIRECT_URL, &ru);
    CHECK(code == 302 && ru && strstr(ru, "/redir/2"));
    reset_buf(&b);
    /* gzip: sem ACCEPT_ENCODING não descomprime nem pede; com "" pede e descomprime */
    curl_easy_setopt(e, O_URL, URL("/gzip"));
    CHECK(curl_easy_perform(e) == 0 && b.n == 55000);
    reset_buf(&b);
    curl_easy_setopt(e, O_ACCEPT_ENCODING, "");
    CHECK(curl_easy_perform(e) == 0 && b.n == 55000 && !strncmp(b.d, "comprimido", 10));
    reset_buf(&b);
    struct curl_slist *hl = curl_slist_append(0, "Accept-Encoding: gzip");
    curl_easy_setopt(e, O_ACCEPT_ENCODING, (char *)0); curl_easy_setopt(e, O_HTTPHEADER, hl);
    CHECK(curl_easy_perform(e) == 0 && b.n < 55000 && (unsigned char)b.d[0] == 0x1f);   /* cru: o app pediu sozinho */
    reset_buf(&b);
    curl_easy_setopt(e, O_HTTPHEADER, (struct curl_slist *)0); curl_slist_free_all(hl);
    /* cookies: motor ligado e jar em arquivo */
    curl_easy_setopt(e, O_COOKIEFILE, ""); curl_easy_setopt(e, O_COOKIEJAR, "/tmp/libcurl_mnet_jar.txt");
    curl_easy_setopt(e, O_FOLLOW, 1L); curl_easy_setopt(e, O_MAXREDIRS, 5L);
    curl_easy_setopt(e, O_URL, URL("/cookie/set"));
    CHECK(curl_easy_perform(e) == 0 && b.d && strstr(b.d, "sessao=abc123") && strstr(b.d, "tema=escuro"));
    reset_buf(&b);
    /* erros HTTP, autenticação, tempo */
    curl_easy_setopt(e, O_URL, URL("/nada")); curl_easy_setopt(e, O_FAILONERROR, 1L);
    char ebuf[256] = "";
    curl_easy_setopt(e, O_ERRORBUFFER, ebuf);
    CHECK(curl_easy_perform(e) == 22 && strstr(ebuf, "404"));
    curl_easy_setopt(e, O_FAILONERROR, 0L);
    reset_buf(&b);
    curl_easy_setopt(e, O_URL, URL("/basic")); curl_easy_setopt(e, O_USERPWD, "Aladdin:open sesame");
    CHECK(curl_easy_perform(e) == 0 && b.d && !strcmp(b.d, "basic ok"));
    reset_buf(&b);
    curl_easy_setopt(e, O_URL, URL("/digest")); curl_easy_setopt(e, O_USERPWD, "maria:segredo"); curl_easy_setopt(e, O_HTTPAUTH, 2L);
    CHECK(curl_easy_perform(e) == 0 && b.d && !strcmp(b.d, "digest ok"));
    reset_buf(&b);
    curl_easy_setopt(e, O_HTTPAUTH, 1L); curl_easy_setopt(e, O_USERPWD, (char *)0);
    curl_easy_setopt(e, O_URL, URL("/lento")); curl_easy_setopt(e, O_TIMEOUT_MS, 700L);
    CHECK(curl_easy_perform(e) == 28);
    curl_easy_setopt(e, O_TIMEOUT_MS, 0L);
    reset_buf(&b);
    /* tipo, HEAD, MIME */
    char *ct = 0;
    curl_easy_setopt(e, O_URL, URL("/json"));
    CHECK(curl_easy_perform(e) == 0 && !curl_easy_getinfo(e, I_CONTENT_TYPE, &ct) && ct && !strcmp(ct, "application/json; charset=utf-8"));
    reset_buf(&b);
    curl_easy_setopt(e, O_NOBODY, 1L); curl_easy_setopt(e, O_URL, URL("/ola"));
    CHECK(curl_easy_perform(e) == 0 && b.n == 0);
    curl_easy_setopt(e, O_NOBODY, 0L);
    void *m = curl_mime_init(e), *pt = curl_mime_addpart(m);
    curl_mime_name(pt, "campo"); curl_mime_data(pt, "valor do campo", (size_t)-1);
    pt = curl_mime_addpart(m); curl_mime_name(pt, "arquivo"); curl_mime_filename(pt, "a.txt"); curl_mime_data(pt, "linha1\nlinha2", 13);
    curl_easy_setopt(e, O_URL, URL("/eco")); curl_easy_setopt(e, O_MIMEPOST, m);
    CHECK(curl_easy_perform(e) == 0 && b.d && strstr(b.d, "eco:POST:--") && strstr(b.d, "name=\"campo\"\r\n\r\nvalor do campo\r\n") &&
          strstr(b.d, "filename=\"a.txt\"\r\nContent-Type: application/octet-stream\r\n\r\nlinha1\nlinha2\r\n"));
    curl_mime_free(m);
    reset_buf(&b);
    curl_easy_cleanup(e);                                       /* grava o jar */
    FILE *jf = fopen("/tmp/libcurl_mnet_jar.txt", "r");
    char jt[1024] = {0}; if (jf) { if (!fread(jt, 1, sizeof jt - 1, jf)) jt[0] = 0; fclose(jf); }
    CHECK(strstr(jt, "#HttpOnly_127.0.0.1\tFALSE\t/\tFALSE\t0\tsessao\tabc123") && strstr(jt, "\ttema\tescuro"));
    unlink("/tmp/libcurl_mnet_jar.txt");
    /* multi: três ao mesmo tempo, callbacks no fio de quem chama */
    void *mu = curl_multi_init();
    CURL *es[3]; buf_t bs[3]; memset(bs, 0, sizeof bs);
    const char *paths[3] = {"/ola", "/chunked", "/gzip"};
    for (int i = 0; i < 3; i++) {
        es[i] = curl_easy_init();
        bs[i].th = pthread_self();
        char uu[160]; snprintf(uu, sizeof uu, "http://127.0.0.1:%d%s", port, paths[i]);
        curl_easy_setopt(es[i], O_URL, uu); curl_easy_setopt(es[i], O_WRITEFN, wr); curl_easy_setopt(es[i], O_WRITEDATA, &bs[i]);
        curl_easy_setopt(es[i], O_ACCEPT_ENCODING, ""); curl_easy_setopt(es[i], O_PRIVATE, (void *)(long)(i + 1));
        curl_multi_add_handle(mu, es[i]);
    }
    int running = 1, done = 0, okres = 0;
    for (int it = 0; it < 2000 && (running || done < 3); it++) {
        curl_multi_perform(mu, &running);
        int left;
        struct { int msg; void *easy; int result; } *msg;
        while ((msg = curl_multi_info_read(mu, &left))) { if (msg->msg == 1) { done++; if (msg->result == 0) okres++; } }
        if (running) curl_multi_wait(mu, 0, 0, 50, 0);
    }
    CHECK(done == 3 && okres == 3);
    CHECK(bs[0].d && !strcmp(bs[0].d, "ola mundo") && bs[1].d && strstr(bs[1].d, "pedaco 49;") && bs[2].n == 55000);
    CHECK(!bs[0].bad_thread && !bs[1].bad_thread && !bs[2].bad_thread);
    for (int i = 0; i < 3; i++) { char *pv = 0; curl_easy_getinfo(es[i], I_PRIVATE, &pv); CHECK((long)pv == i + 1); curl_multi_remove_handle(mu, es[i]); curl_easy_cleanup(es[i]); reset_buf(&bs[i]); }
    curl_multi_cleanup(mu);
}
static void url_api(void) {
    void *u = curl_url();
    CHECK(curl_url_set(u, 0, "https://user:pw@Exemplo.com:8443/a/b?x=1#f", 0) == 0);
    char *s = 0;
    CHECK(!curl_url_get(u, 5, &s, 0) && !strcmp(s, "exemplo.com")); free(s);
    CHECK(!curl_url_get(u, 6, &s, 0) && !strcmp(s, "8443")); free(s);
    CHECK(!curl_url_get(u, 2, &s, 0) && !strcmp(s, "user")); free(s);
    CHECK(!curl_url_get(u, 8, &s, 0) && !strcmp(s, "x=1")); free(s);
    CHECK(curl_url_set(u, 0, "../c?y=2", 0) == 0 && !curl_url_get(u, 0, &s, 0) && !strcmp(s, "https://user:pw@exemplo.com:8443/c?y=2")); free(s);
    CHECK(curl_url_set(u, 8, "z=3 4", 256 | 128) == 0 && !curl_url_get(u, 8, &s, 0) && !strcmp(s, "y=2&z=3%204")); free(s);
    CHECK(curl_url_set(u, 6, "99999", 0) == 4 && curl_url_get(u, 9, &s, 0) == 17);
    curl_url_cleanup(u); u = curl_url();               /* vazio: sem esquema não é relativa, é erro (ou o padrão com DEFAULT_SCHEME) */
    CHECK(curl_url_set(u, 0, "semesquema.com/x", 0) == 10 && curl_url_set(u, 0, "semesquema.com/x", 4) == 0);
    CHECK(!curl_url_get(u, 1, &s, 0) && !strcmp(s, "https")); free(s);
    curl_url_cleanup(u);
    char *e = curl_easy_escape(0, "a b/ç", 0);
    CHECK(e && !strcmp(e, "a%20b%2F%C3%A7"));
    int ol; char *d = curl_easy_unescape(0, e, 0, &ol);
    CHECK(d && !strcmp(d, "a b/ç") && ol == 6);
    curl_free(e); curl_free(d);
}
static void rede(void) {
    FILE *f = fopen("assets/certs/cacert.pem", "rb");
    if (!f) { fails++; return; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    char *pem = malloc((size_t)n + 1); n = (long)fread(pem, 1, (size_t)n, f); pem[n] = 0; fclose(f);
    x509_trust_load_pem(pem, (u32)n); free(pem);
    CURL *e = curl_easy_init();
    buf_t b = {0};
    curl_easy_setopt(e, O_URL, "https://example.com/"); curl_easy_setopt(e, O_WRITEFN, wr); curl_easy_setopt(e, O_WRITEDATA, &b);
    curl_easy_setopt(e, O_ACCEPT_ENCODING, "");
    int r = curl_easy_perform(e);
    long v = 0, code = 0; curl_easy_getinfo(e, I_HTTP_VERSION, &v); curl_easy_getinfo(e, I_RESPONSE_CODE, &code);
    printf("  https://example.com -> %d status %ld versão %ld corpo %zu\n", r, code, v, b.n);
    CHECK(r == 0 && code == 200 && v == 3 && b.n > 100);
    reset_buf(&b);
    curl_easy_setopt(e, O_URL, "https://expired.badssl.com/");
    CHECK(curl_easy_perform(e) == 35);
    curl_easy_cleanup(e);
}
int main(int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN);
    for (int i = 0; i < 64; i++) socks[i] = -1;
    int port = 19000 + (getpid() % 1000);
    char cmd[200]; snprintf(cmd, sizeof cmd, "exec python3 -I tools/mnet_http_servidor.py %d 2>/dev/null", port);
    FILE *srv = popen(cmd, "r");
    char line[32] = {0};
    CHECK(srv && fgets(line, sizeof line, srv) && !strncmp(line, "pronto", 6));
    int pid = atoi(line + 7);
    local(port);
    url_api();
    if (argc > 1 && !strcmp(argv[1], "rede")) rede();
    if (pid > 0) kill(pid, SIGTERM);
    pclose(srv);
    printf("libcurl_mnet: %d/%d ok\n", checks - fails, checks);
    return fails ? 1 : 0;
}
