/* mOS — shim de host para o motor web (kernel/web) e o gfx do kernel rodarem no Linux.
 *
 * Só para testes de host (tools/web_host.sh): liga kmalloc/kprintf/timer à libc, serve "http://teste/<arquivo>"
 * a partir de um diretório local (WEB_HOST_DIR) e dá às funções de janela/GPU/VFS respostas neutras.
 * Não entra na imagem do mOS. Compile SEM os headers do kernel (é código de host). */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <stdint.h>
#include <time.h>
#include <math.h>

typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;

void *kmalloc(size_t n) { return malloc(n ? n : 1); }
void *kcalloc(size_t n, size_t s) { return calloc(n ? n : 1, s ? s : 1); }
void *krealloc(void *p, size_t n) { return realloc(p, n ? n : 1); }
void  kfree(void *p) { free(p); }
void *kmalloc_pages(size_t n) { return calloc(1, n ? n : 1); }

int web_host_verbose;
void kprintf(const char *fmt, ...) {
    if (!web_host_verbose) return;
    va_list ap; va_start(ap, fmt); vfprintf(stderr, fmt, ap); va_end(ap);
}
void panic(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); fprintf(stderr, "PANIC: "); vfprintf(stderr, fmt, ap); va_end(ap);
    fputc('\n', stderr); abort();
}

static u64 t0;
u64 timer_ticks(void) {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
    u64 ms = (u64)ts.tv_sec * 1000 + ts.tv_nsec / 1000000;
    if (!t0) t0 = ms - 1;
    return ms - t0 + 100000;
}
u64 rdtsc(void) { struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts); return (u64)ts.tv_sec * 3000000000ull + ts.tv_nsec * 3; }
static u32 rs = 12345;
u32 rand32(void) { rs ^= rs << 13; rs ^= rs >> 17; rs ^= rs << 5; return rs; }
void srand32(u32 s) { rs = s ? s : 1; }

typedef struct { int year, month, day, hour, minute, second, weekday; } datetime_t;
void rtc_now(datetime_t *d) { d->year = 2026; d->month = 11; d->day = 15; d->hour = 12; d->minute = 0; d->second = 0; d->weekday = 0; }
void rtc_now_utc(datetime_t *d) { rtc_now(d); }

float fsqrt(float x) { return sqrtf(x); }
float fabsf_(float x) { return fabsf(x); }
float fsin(float x) { return sinf(x); }
float fcos(float x) { return cosf(x); }
float fexp(float x) { return expf(x); }
float fpow(float b, float e) { return powf(b, e); }
float ffloor(float x) { return floorf(x); }
float fceil(float x) { return ceilf(x); }

volatile u32 g_irqoff[64];
void *g_boot;
int par_cpus(void) { return 1; }
typedef void (*par_fn_t)(void *ctx, int begin, int end, int cpu);
void par_for(int n, int grain, par_fn_t fn, void *ctx) { (void)grain; if (n > 0) fn(ctx, 0, n, 0); }

/* janela, GPU, notificações: nada a fazer fora do desktop */
void desktop_displays_changed(void) {}
void gpu_displays_changed(void) {}
void wm_displays_changed(void) {}
void gpu_vsync_wait(void) {}
int  gpu_present(const u32 *px, int w, int h, int x, int y, int rw, int rh) { (void)px; (void)w; (void)h; (void)x; (void)y; (void)rw; (void)rh; return 0; }
typedef struct { int x, y, w, h; } rect_t;
void wm_damage(rect_t r) { (void)r; }
void notify(const char *t, const char *b) { (void)t; (void)b; }
int  net_configured(void) { return 1; }

/* VFS: o motor só consulta (file:, preferências); no host não há sistema de arquivos do mOS */
void *vfs_lookup(const char *p) { (void)p; return NULL; }
void *vfs_mkdir(const char *p) { (void)p; return NULL; }
void *vfs_write_file(const char *p, const void *d, size_t n) { (void)p; (void)d; (void)n; return NULL; }
int   vfs_ensure(void *n) { (void)n; return 0; }

/* fonte monoespaçada do kernel (Terminal): no host, células de 0,6 em */
typedef struct surface surface_t;
int  mono_cell_w(float size) { return (int)(size * 0.6f + 0.5f); }
void mono_draw(surface_t *s, float size, int x, int b, u32 c, const char *t, int m) { (void)s; (void)size; (void)x; (void)b; (void)c; (void)t; (void)m; }

/* a pintura chama isto para controles de formulário; o teste só precisa que exista */
void web_paint_control(void *u, void *d, void *c, rect_t r) { (void)u; (void)d; (void)c; (void)r; }

/* ---------- HTTP de mentira: http://teste/<nome> lê WEB_HOST_DIR/<nome> */
#define MOS_URL_MAX 8192
typedef struct http_req {
    char url[MOS_URL_MAX];
    int status;
    char content_type[96];
    char location[MOS_URL_MAX];
    u8 *body;
    u32 body_len;
    char error[128];
    int state;
    void *priv;
    u64 max_body;
    char *headers;
    int redirects;
} http_req_t;

static const char *ctype_for(const char *n) {
    const char *e = strrchr(n, '.');
    if (!e) return "application/octet-stream";
    if (!strcmp(e, ".html")) return "text/html";
    if (!strcmp(e, ".css")) return "text/css";
    if (!strcmp(e, ".js")) return "text/javascript";
    if (!strcmp(e, ".svg")) return "image/svg+xml";
    if (!strcmp(e, ".png")) return "image/png";
    if (!strcmp(e, ".webp")) return "image/webp";
    return "application/octet-stream";
}

int web_host_http_count;
http_req_t *http_get(const char *url) {
    http_req_t *r = calloc(1, sizeof *r);
    snprintf(r->url, sizeof r->url, "%s", url);
    snprintf(r->location, sizeof r->location, "%s", url);
    web_host_http_count++;
    const char *dir = getenv("WEB_HOST_DIR");
    const char *pre = "http://teste/";
    if (!dir || strncmp(url, pre, strlen(pre))) { r->state = -1; snprintf(r->error, sizeof r->error, "sem rede no host"); return r; }
    char path[4096];
    snprintf(path, sizeof path, "%s/%s", dir, url + strlen(pre));
    char *q = strpbrk(path + strlen(dir), "?#"); if (q) *q = 0;
    FILE *f = fopen(path, "rb");
    if (!f) { r->status = 404; r->state = 1; snprintf(r->content_type, sizeof r->content_type, "text/html"); return r; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    r->body = malloc(n + 1); r->body_len = (u32)fread(r->body, 1, n, f); r->body[r->body_len] = 0; fclose(f);
    r->status = 200; r->state = 1;
    snprintf(r->content_type, sizeof r->content_type, "%s", ctype_for(path));
    return r;
}
http_req_t *http_post(const char *url, const char *ct, const void *b, u32 n) { (void)ct; (void)b; (void)n; return http_get(url); }
/* http://teste/__eco devolve método, cabeçalhos e corpo recebidos; __status/NNN responde com esse código;
 * __redir vai para /__eco com 302. O resto vem do diretório de testes. */
http_req_t *http_request(const char *url, const char *method, const char *hdrs, const void *body, u32 len, int redirect) {
    const char *eco = "http://teste/__eco", *stp = "http://teste/__status/", *rd = "http://teste/__redir";
    if (!strcmp(url, "http://teste/__lento")) {
        /* nunca responde: para timeout e abort */
        http_req_t *r = calloc(1, sizeof *r);
        snprintf(r->url, sizeof r->url, "%s", url);
        return r;
    }
    if (!strncmp(url, rd, strlen(rd))) {
        if (redirect == 1) {
            http_req_t *r = calloc(1, sizeof *r);
            snprintf(r->url, sizeof r->url, "%s", url); snprintf(r->location, sizeof r->location, "%s", url);
            r->status = 302; r->state = 1; r->body = calloc(1, 1);
            r->headers = strdup("Location: /__eco\r\n");
            return r;
        }
        http_req_t *r = http_request(eco, method, hdrs, body, len, 0);
        r->redirects = 1;
        return r;
    }
    if (!strncmp(url, eco, strlen(eco))) {
        http_req_t *r = calloc(1, sizeof *r);
        snprintf(r->url, sizeof r->url, "%s", url); snprintf(r->location, sizeof r->location, "%s", url);
        size_t cap = 256 + (hdrs ? strlen(hdrs) : 0) + len;
        r->body = malloc(cap);
        int k = snprintf((char *)r->body, cap, "%s\n%s\n", method, hdrs ? hdrs : "");
        if (len) memcpy(r->body + k, body, len);
        r->body_len = (u32)(k + len);
        r->body[r->body_len] = 0;
        r->status = 200; r->state = 1;
        snprintf(r->content_type, sizeof r->content_type, "text/plain");
        r->headers = strdup("Content-Type: text/plain\r\nX-Teste: um\r\nSet-Cookie: a=1\r\nX-Teste: dois\r\n");
        return r;
    }
    if (!strncmp(url, stp, strlen(stp))) {
        http_req_t *r = calloc(1, sizeof *r);
        snprintf(r->url, sizeof r->url, "%s", url); snprintf(r->location, sizeof r->location, "%s", url);
        r->status = atoi(url + strlen(stp)); r->state = 1;
        r->body = (u8 *)strdup("corpo"); r->body_len = 5;
        r->headers = strdup("Content-Type: text/plain\r\n");
        return r;
    }
    (void)redirect;
    http_req_t *r = http_get(url);
    if (r->state == 1 && r->content_type[0]) { char b[160]; snprintf(b, sizeof b, "Content-Type: %s\r\n", r->content_type); r->headers = strdup(b); }
    return r;
}
int  http_poll(http_req_t *r) { return r->state; }
void http_free(http_req_t *r) { if (r) { free(r->body); free(r->headers); free(r); } }
float http_progress(http_req_t *r) { return r->state ? 1.0f : 0.0f; }

/* ---------- utilidades para os testes (o teste usa os headers do kernel e não vê a stdio) */
u8 *host_read_file(const char *path, size_t *n) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); long len = ftell(f); fseek(f, 0, SEEK_SET);
    u8 *b = malloc(len + 1); *n = fread(b, 1, len, f); b[*n] = 0; fclose(f);
    return b;
}
int host_printf(const char *fmt, ...) {
    va_list ap; va_start(ap, fmt); int r = vprintf(fmt, ap); va_end(ap); fflush(stdout); return r;
}
/* grava um surface BGRA pré-multiplicado como PPM (para olhar o resultado) */
int host_write_ppm(const char *path, const u32 *px, int w, int h, int stride) {
    FILE *f = fopen(path, "wb");
    if (!f) return -1;
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) { u32 c = px[y * stride + x]; u8 rgb[3] = { c >> 16, c >> 8, c }; fwrite(rgb, 1, 3, f); }
    fclose(f);
    return 0;
}

/* CSPRNG do kernel (kernel/crypto): no host, o getrandom do Linux */
#include <sys/random.h>
void crypto_random(void *buf, size_t len) {
    u8 *p = buf;
    while (len) { ssize_t r = getrandom(p, len, 0); if (r <= 0) abort(); p += r; len -= (size_t)r; }
}
void net_poll(void) {}
/* pausa real (temporizadores de JS que dependem do relógio) */
#include <unistd.h>
void host_sleep_ms(int ms) { struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L }; nanosleep(&ts, NULL); }
