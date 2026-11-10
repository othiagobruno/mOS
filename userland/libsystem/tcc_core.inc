/* Permissões locais de TCC (gravação de tela, acessibilidade, notificação).
 * Não é o TCC da Apple e não desenha o prompt: grava pending e espera tcc_decide.
 * Incluído por tcc.c e por tools/tcctest.c.
 *
 * Estado ausente = não determinado = preflight negado.
 * request em estado ausente grava pending e devolve 0.
 * request em denied não pede de novo.
 * request em granted devolve 1.
 */
#ifdef TCC_HOST
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
typedef FILE tcc_file;
#else
typedef FILE_ tcc_file;
#endif

enum { TCC_MISSING = 0, TCC_DENIED = 1, TCC_GRANTED = 2, TCC_PENDING = 3 };

typedef struct {
    char bundle[184];
    char service[184];
    int state;
} tcc_rec;

#define TCC_CAP 64

static int tcc_tok_ok(const char *s) {
    if (!s || !s[0] || strlen(s) > 180) return 0;
    if (strstr(s, "..")) return 0;
    for (const char *p = s; *p; p++) {
        char c = *p;
        int ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-' || c == '+';
        if (!ok) return 0;
    }
    return 1;
}

static int tcc_find(tcc_rec *r, int n, const char *b, const char *s) {
    for (int i = 0; i < n; i++) if (!strcmp(r[i].bundle, b) && !strcmp(r[i].service, s)) return i;
    return -1;
}

static int tcc_load(const char *path, tcc_rec *r, int cap) {
    if (!path) return 0;
    tcc_file *f = fopen(path, "rb");
    if (!f) return 0;
    char line[512];
    int n = 0;
    while (n < cap && fgets(line, sizeof line, f)) {
        if (line[0] == '#' || line[0] == '\n') continue;
        char b[184], s[184], st[32];
        if (sscanf(line, "%183s %183s %31s", b, s, st) != 3) continue;
        if (!tcc_tok_ok(b) || !tcc_tok_ok(s)) continue;
        int state = TCC_MISSING;
        if (!strcmp(st, "granted")) state = TCC_GRANTED;
        else if (!strcmp(st, "denied")) state = TCC_DENIED;
        else if (!strcmp(st, "pending")) state = TCC_PENDING;
        else continue;
        snprintf(r[n].bundle, sizeof r[n].bundle, "%s", b);
        snprintf(r[n].service, sizeof r[n].service, "%s", s);
        r[n].state = state;
        n++;
    }
    fclose(f);
    return n;
}

static void tcc_mkdir_parent(const char *path) {
    char b[1024];
    snprintf(b, sizeof b, "%s", path);
    char *slash = strrchr(b, '/');
    if (!slash || slash == b) return;
    *slash = 0;
    for (char *p = b + 1; *p; p++) if (*p == '/') { *p = 0; mkdir(b, 0755); *p = '/'; }
    mkdir(b, 0755);
}

static int tcc_save(const char *path, const tcc_rec *r, int n) {
    if (!path) return -1;
    tcc_mkdir_parent(path);
    char tmp[1100];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    tcc_file *f = fopen(tmp, "wb");
    if (!f) return -1;
    for (int i = 0; i < n; i++) {
        const char *st = r[i].state == TCC_GRANTED ? "granted" : r[i].state == TCC_DENIED ? "denied" : r[i].state == TCC_PENDING ? "pending" : 0;
        if (!st) continue;
        fprintf(f, "%s %s %s\n", r[i].bundle, r[i].service, st);
    }
    if (fclose(f) != 0) return -1;
    if (rename(tmp, path) != 0) { unlink(tmp); return -1; }
    return 0;
}

static int tcc_state_at(const char *path, const char *bundle, const char *service) {
    if (!tcc_tok_ok(bundle) || !tcc_tok_ok(service)) return TCC_MISSING;
    tcc_rec r[TCC_CAP];
    int n = tcc_load(path, r, TCC_CAP);
    int i = tcc_find(r, n, bundle, service);
    return i < 0 ? TCC_MISSING : r[i].state;
}

static int tcc_preflight(const char *path, const char *bundle, const char *service) {
    return tcc_state_at(path, bundle, service) == TCC_GRANTED;
}

static int tcc_request(const char *path, const char *bundle, const char *service) {
    if (!tcc_tok_ok(bundle) || !tcc_tok_ok(service)) return 0;
    tcc_rec r[TCC_CAP];
    int n = tcc_load(path, r, TCC_CAP);
    int i = tcc_find(r, n, bundle, service);
    int st = i < 0 ? TCC_MISSING : r[i].state;
    if (st == TCC_GRANTED) return 1;
    if (st == TCC_DENIED || st == TCC_PENDING) return 0;
    if (n >= TCC_CAP) return 0;
    snprintf(r[n].bundle, sizeof r[n].bundle, "%s", bundle);
    snprintf(r[n].service, sizeof r[n].service, "%s", service);
    r[n].state = TCC_PENDING;
    tcc_save(path, r, n + 1);
    return 0;
}

/* granted != 0 concede; 0 nega. Devolve 0 se gravou, -1 se o nome é inválido. */
static int tcc_decide(const char *path, const char *bundle, const char *service, int granted) {
    if (!tcc_tok_ok(bundle) || !tcc_tok_ok(service)) return -1;
    tcc_rec r[TCC_CAP];
    int n = tcc_load(path, r, TCC_CAP);
    int i = tcc_find(r, n, bundle, service);
    if (i < 0) {
        if (n >= TCC_CAP) return -1;
        i = n++;
        snprintf(r[i].bundle, sizeof r[i].bundle, "%s", bundle);
        snprintf(r[i].service, sizeof r[i].service, "%s", service);
    }
    r[i].state = granted ? TCC_GRANTED : TCC_DENIED;
    return tcc_save(path, r, n) == 0 ? 0 : -1;
}
