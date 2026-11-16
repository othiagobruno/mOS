/*
 * TCC local do mOS: todos os serviços (tela, acessibilidade, câmera, microfone, contatos, agenda, fotos, pastas,
 * Apple Events "kTCCServiceAppleEvents+<alvo>", notificações...).
 * O arquivo é NSHomeDirectory()/Library/Application Support/com.mos.tcc/decisions.txt.
 * O diálogo é da Interface (kernel/ui/tcc_prompt.c): ela vê o "pending" no arquivo e grava a escolha.
 * cx_tcc_request_async é o pedido dos frameworks com bloco de conclusão (requestAccess…): com escolha já feita o
 * retorno é imediato; senão grava "pending" e acompanha o arquivo, numa fila global, até o usuário responder.
 */
#include "foundation.h"
#include "libc.h"

int fprintf(void *f, const char *fmt, ...);
char *fgets(char *s, int n, void *f);
int sscanf(const char *s, const char *fmt, ...);
int rename(const char *a, const char *b);

#include "tcc_core.inc"

id NSHomeDirectory(void);

static void tcc_file_path(char *out, size_t cap) {
    id h = NSHomeDirectory();
    const char *s = h ? cx_cstr(h) : 0;
    if (!s || !s[0] || !strcmp(s, "(null)")) s = "/Users/Visitante";
    snprintf(out, cap, "%s/Library/Application Support/com.mos.tcc/decisions.txt", s);
}

static const char *tcc_bundle(void) {
    static char buf[256];
    static int ok;
    if (ok) return buf;
    id b = MSG0(id, (id)cx_class_named("NSBundle"), "mainBundle");
    id bid = b ? MSG0(id, b, "bundleIdentifier") : 0;
    const char *s = bid && cx_is_str(bid) ? ((nsstr_t *)bid)->s : 0;
    if (!s || !s[0]) s = "com.mos.app";
    snprintf(buf, sizeof buf, "%s", s);
    ok = 1;
    return buf;
}

int cx_tcc_preflight(const char *service) {
    char path[512];
    tcc_file_path(path, sizeof path);
    return tcc_preflight(path, tcc_bundle(), service);
}

int cx_tcc_request(const char *service) {
    char path[512];
    tcc_file_path(path, sizeof path);
    return tcc_request(path, tcc_bundle(), service);
}

int cx_tcc_state(const char *service) {
    char path[512];
    tcc_file_path(path, sizeof path);
    return tcc_state_at(path, tcc_bundle(), service);
}

int cx_tcc_decide(const char *bundle, const char *service, int granted) {
    char path[512];
    tcc_file_path(path, sizeof path);
    const char *b = (bundle && bundle[0]) ? bundle : tcc_bundle();
    return tcc_decide(path, b, service, granted);
}

/*  -25211 API desligada (sem permissão). -25200 autorizado, mas a árvore AX é da Interface. */
int cx_ax_unavailable(void) {
    return cx_tcc_preflight("kTCCServiceAccessibility") ? -25200 : -25211;
}

/* ---- pedido assíncrono (o requestAccess… dos frameworks) */
typedef void (*cx_tcc_cb)(int granted, void *ctx);
typedef struct { char service[184]; cx_tcc_cb cb; void *ctx; int polls; } tcc_wait_t;
void *dispatch_get_global_queue(long id, unsigned long flags);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
void dispatch_after_f(unsigned long long when, void *q, void *ctx, void (*fn)(void *));
unsigned long long dispatch_time(unsigned long long when, long long delta);
void *calloc(size_t n, size_t m);
void free(void *p);
#define TCC_POLL_NS 200000000ll                           /* 0,2 s: o diálogo relê o arquivo 4 vezes por segundo */
static void tcc_wait_step(void *p) {
    tcc_wait_t *w = p;
    int st = cx_tcc_state(w->service);
    if (st == TCC_GRANTED || st == TCC_DENIED || st == TCC_MISSING) {
        /* MISSING: alguém apagou o pedido (tccutil reset) antes da resposta: conta como "não" */
        w->cb(st == TCC_GRANTED, w->ctx);
        free(w);
        return;
    }
    w->polls++;
    dispatch_after_f(dispatch_time(0, TCC_POLL_NS), dispatch_get_global_queue(0, 0), w, tcc_wait_step);
}
void cx_tcc_request_async(const char *service, cx_tcc_cb cb, void *ctx) {
    if (!cb) return;
    tcc_wait_t *w = calloc(1, sizeof *w);
    if (!w || !service || strlen(service) >= sizeof w->service) { free(w); cb(0, ctx); return; }
    snprintf(w->service, sizeof w->service, "%s", service);
    w->cb = cb;
    w->ctx = ctx;
    cx_tcc_request(service);                              /* sem registro: grava pending (o diálogo abre) */
    dispatch_async_f(dispatch_get_global_queue(0, 0), w, tcc_wait_step);
}
