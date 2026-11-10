/*
 * TCC local do mOS: gravação de tela, acessibilidade e notificação do usuário.
 * O arquivo é NSHomeDirectory()/Library/Application Support/com.mos.tcc/decisions.txt.
 * A Interface consulta cx_tcc_preflight antes de devolver pixels ou a árvore AX,
 * e responde o prompt com cx_tcc_decide. Não há diálogo desenhado aqui.
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
