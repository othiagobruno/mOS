/* mOS — teste de host do cliente HTTP do kernel (kernel/net/http.c) com URLs longas.
 *
 * O OAuth do Google no Antigravity voltava com invalid_scope ("invalid=[http]"): o pedido tem ~650 bytes, o
 * http_req_t guardava 511, e o 302 do Google (continue= codificado duas vezes) passa de 1 KiB. O scope chegava
 * cortado no meio de "https%3A...". Aqui o http.c real roda contra um servidor falso (TCP e TLS de mentira):
 *   1. o pedido de OAuth sai inteiro na linha de requisição (scope com os cinco escopos);
 *   2. o 302 com Location de ~2 KiB é seguido sem corte, e r->location é a URL final inteira;
 *   3. Location relativo ("?..." e "../") resolve contra o caminho atual;
 *   4. URL acima de MOS_URL_MAX falha com "URL longa demais" e nenhuma conexão abre.
 *
 *   gcc -std=gnu11 -DMOS_HOST_TEST -Ikernel/include -Itools tools/http_url_longa_host.c kernel/net/http.c \
 *       -o /tmp/http-url-longa && /tmp/http-url-longa
 */
/* Sem headers da libc: o kernel.h declara as funções de texto com os tipos do kernel. A libc do host as fornece. */
#include "net.h"
int printf(const char *fmt, ...);
int vprintf(const char *fmt, va_list ap);
void *malloc(usize n);
void *calloc(usize a, usize b);
void *realloc(void *p, usize n);
void free(void *p);
#ifndef MOS_URL_MAX
#define MOS_URL_MAX 8192   /* para rodar o mesmo teste contra o http.c antigo */
#endif

/* ------------------------------------------------------------------ kernel falso */
void *kmalloc(usize n) { return malloc(n ? n : 1); }
void *kcalloc(usize a, usize b) { return calloc(a ? a : 1, b ? b : 1); }
void *krealloc(void *p, usize n) { return realloc(p, n ? n : 1); }
void kfree(void *p) { free(p); }
static int quiet;
void kprintf(const char *fmt, ...) {
    if (quiet) return;
    va_list ap;
    va_start(ap, fmt);
    printf("        [kprintf] ");
    vprintf(fmt, ap);
    va_end(ap);
}
static u64 now_ms;
u64 timer_ticks(void) { return now_ms; }
bool net_configured(void) { return true; }
int dns_resolve(const char *h, u32 *ip) { (void)h; *ip = 0x01020304; return 1; }

/* ------------------------------------------------------------------ servidor falso: uma resposta por conexão */
#define MAXC 8
static struct { char *sent; usize sent_n; const char *resp; usize off; } conn[MAXC];
static int nconn;
static const char *script[MAXC];

int tcp_connect(u32 ip, u16 port) {
    (void)ip; (void)port;
    if (nconn >= MAXC) return -1;
    conn[nconn].sent = calloc(1, 1);
    conn[nconn].resp = script[nconn] ? script[nconn] : "HTTP/1.1 500 X\r\nContent-Length: 0\r\n\r\n";
    return nconn++;
}
int tcp_state(int s) { (void)s; return TCP_OPEN; }
const char *tcp_error_reason(int s) { (void)s; return "?"; }
int tcp_send(int s, const void *d, int n) {
    conn[s].sent = realloc(conn[s].sent, conn[s].sent_n + (usize)n + 1);
    memcpy(conn[s].sent + conn[s].sent_n, d, (usize)n);
    conn[s].sent_n += (usize)n;
    conn[s].sent[conn[s].sent_n] = 0;
    return n;
}
int tcp_recv(int s, void *b, int max) {
    usize left = strlen(conn[s].resp) - conn[s].off;
    if (!left) return -1;
    usize k = left < (usize)max ? left : (usize)max;
    memcpy(b, conn[s].resp + conn[s].off, k);
    conn[s].off += k;
    return (int)k;
}
void tcp_close(int s) { (void)s; }
/* TLS: o mesmo fio, sem cifra */
typedef struct { int s; } fake_tls;
tls_conn_t *tls_new(int s, const char *host) { (void)host; fake_tls *t = malloc(sizeof *t); t->s = s; return (tls_conn_t *)t; }
int tls_handshake(tls_conn_t *c) { (void)c; return 1; }
int tls_send(tls_conn_t *c, const void *d, int n) { return tcp_send(((fake_tls *)c)->s, d, n); }
int tls_recv(tls_conn_t *c, void *b, int max) { return tcp_recv(((fake_tls *)c)->s, b, max); }
void tls_free(tls_conn_t *c) { free(c); }
const char *tls_error(tls_conn_t *c) { (void)c; return ""; }

/* ------------------------------------------------------------------ apoio */
static int ok, total;
static void check(int cond, const char *what) {
    total++;
    if (cond) ok++;
    printf("%s %s\n", cond ? "  ok  " : "  FALHA", what);
}
static void reset(void) {
    for (int i = 0; i < nconn; i++) free(conn[i].sent);
    memset(conn, 0, sizeof conn);
    memset(script, 0, sizeof script);
    nconn = 0;
}
static int run(http_req_t *r) {
    for (int i = 0; i < 1000; i++) {
        int st = http_poll(r);
        if (st) return st;
        now_ms += 1;
    }
    return 0;
}
/* caminho (com a consulta) da linha "GET <caminho> HTTP/1.1" da conexão i */
static char *req_path(int i) {
    static char out[16384];
    out[0] = 0;
    if (i >= nconn || !conn[i].sent) return out;
    const char *sp = strchr(conn[i].sent, ' ');
    const char *e = sp ? strstr(sp + 1, " HTTP/1.1\r\n") : NULL;
    if (sp && e && (usize)(e - sp - 1) < sizeof out) { memcpy(out, sp + 1, (usize)(e - sp - 1)); out[e - sp - 1] = 0; }
    return out;
}
/* valor bruto (ainda codificado) de um parâmetro da consulta */
static const char *param(const char *path, const char *name, char *out, usize cap) {
    usize nl = strlen(name);
    const char *q = strchr(path, '?');
    out[0] = 0;
    for (const char *p = q; p && *p; ) {
        p++;
        if (!strncmp(p, name, nl) && p[nl] == '=') {
            const char *v = p + nl + 1, *e = strchr(v, '&');
            usize n = e ? (usize)(e - v) : strlen(v);
            if (n >= cap) n = cap - 1;
            memcpy(out, v, n);
            out[n] = 0;
            return out;
        }
        p = strchr(p, '&');
    }
    return out;
}

/* Pedido no formato do Go (url.Values.Encode: chaves em ordem, espaço vira '+'), com os escopos que o
 * language_server do Antigravity 2.29.0 traz (strings do binário). client_id e state são de exemplo. */
#define SC "https%3A%2F%2Fwww.googleapis.com%2Fauth%2F"
static const char *SCOPE = SC "cloud-platform+" SC "userinfo.email+" SC "userinfo.profile+" SC "cclog+" SC "experimentsandconfigs";

int main(void) {
    char auth[2048], loc[4096], cont[3072], resp[8192], got[4096];
    snprintf(auth, sizeof auth,
             "https://accounts.google.com/o/oauth2/auth?access_type=offline"
             "&client_id=000000000000-exemploexemploexemploexemplo00.apps.googleusercontent.com"
             "&code_challenge=AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA&code_challenge_method=S256&prompt=consent"
             "&redirect_uri=http%%3A%%2F%%2Flocalhost%%3A51121%%2Foauth-callback&response_type=code"
             "&scope=%s&state=BBBBBBBBBBBBBBBBBBBBBBBBBBBBBBBB", SCOPE);
    printf("pedido de OAuth: %u bytes (o http_req_t antigo guardava 511)\n", (unsigned)strlen(auth));

    /* continue= é o próprio pedido codificado de novo (% vira %25), como o Google faz no 302 */
    usize o = 0;
    for (const char *p = auth; *p && o + 4 < sizeof cont; p++) {
        if (*p == '%') { memcpy(cont + o, "%25", 3); o += 3; }
        else if (*p == '&') { memcpy(cont + o, "%26", 3); o += 3; }
        else if (*p == '=') { memcpy(cont + o, "%3D", 3); o += 3; }
        else if (*p == '?') { memcpy(cont + o, "%3F", 3); o += 3; }
        else cont[o++] = *p;
    }
    cont[o] = 0;
    snprintf(loc, sizeof loc, "https://accounts.google.com/v3/signin/identifier?opparams=%%253Fscope%%253D%s&dsh=S-1&flowName=GeneralOAuthFlow&continue=%s", SCOPE, cont);
    printf("Location do 302: %u bytes\n", (unsigned)strlen(loc));
    snprintf(resp, sizeof resp, "HTTP/1.1 302 Found\r\nLocation: %s\r\nContent-Length: 0\r\n\r\n", loc);

    printf("1. pedido de OAuth e 302 do Google\n");
    reset();
    script[0] = resp;
    script[1] = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\nContent-Length: 5\r\n\r\nlogin";
    http_req_t *r = http_get(auth);
    int st = run(r);
    check(st == 1 && r->status == 200, "o pedido termina com 200 depois do 302");
    check(nconn == 2, "duas conexões: o pedido e o redirecionamento");
    const char *expect_path = auth + strlen("https://accounts.google.com");
    check(!strcmp(req_path(0), expect_path), "a linha de requisição leva o pedido inteiro");
    param(req_path(0), "scope", got, sizeof got);
    check(!strcmp(got, SCOPE), "o scope chega com os cinco escopos (nada de \"http\" cortado)");
    printf("        scope recebido termina em: ...%s\n", got[0] ? got + (strlen(got) > 40 ? strlen(got) - 40 : 0) : "(vazio)");
    check(!strcmp(req_path(1), loc + strlen("https://accounts.google.com")), "o 302 é seguido com o Location inteiro");
    check(!strcmp(r->location, loc), "r->location é a URL final inteira");
    http_free(r);

    printf("2. Location relativo\n");
    reset();
    script[0] = "HTTP/1.1 302 Found\r\nLocation: ?passo=2\r\nContent-Length: 0\r\n\r\n";
    script[1] = "HTTP/1.1 301 Moved\r\nLocation: ../b/c?d=1\r\nContent-Length: 0\r\n\r\n";
    script[2] = "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\nok";
    r = http_get("https://example.test/x/y/pagina?passo=1");
    st = run(r);
    check(st == 1, "três respostas seguidas");
    check(!strcmp(req_path(1), "/x/y/pagina?passo=2"), "\"?passo=2\" troca só a consulta");
    check(!strcmp(req_path(2), "/x/y/../b/c?d=1"), "\"../b/c\" resolve a partir do diretório atual");
    http_free(r);

    printf("3. acima do limite: falha, não corta\n");
    reset();
    usize big = MOS_URL_MAX + 100;
    char *huge = malloc(big + 1);
    memcpy(huge, "https://example.test/?q=", 24);
    memset(huge + 24, 'a', big - 24);
    huge[big] = 0;
    quiet = 1;
    r = http_get(huge);
    st = run(r);
    quiet = 0;
    check(st == -1 && strstr(r->error, "longa demais"), "URL maior que MOS_URL_MAX devolve \"URL longa demais\"");
    check(nconn == 0, "nenhuma conexão abre com a URL longa demais");
    http_free(r);
    /* Location longo demais no 302 também falha */
    reset();
    char *bigresp = malloc(big + 128);
    snprintf(bigresp, big + 128, "HTTP/1.1 302 Found\r\nLocation: %s\r\nContent-Length: 0\r\n\r\n", huge);
    script[0] = bigresp;
    quiet = 1;
    r = http_get("https://example.test/");
    st = run(r);
    quiet = 0;
    check(st == -1 && strstr(r->error, "longa demais") && nconn == 1, "302 com Location acima do limite falha sem seguir uma URL cortada");
    http_free(r);
    free(bigresp);
    free(huge);
    reset();

    printf("%d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
