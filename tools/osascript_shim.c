/* mOS — shim de host do osascript(1): o interpretador de verdade (as_interp.inc) com um as_host mínimo (shell real
 * pelo popen, log no stderr, sem apps). Usado por tools/osascripttest.sh. */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
typedef uint8_t u8;
typedef uint32_t u32;
typedef int32_t i32;
typedef int16_t i16;
typedef uint64_t u64;
typedef int64_t i64;
typedef size_t usize;
#include "ae_core.inc"
#include "as_interp.inc"

static int s_send(void *x, const char *app, unsigned c, unsigned i, const as_val *d, as_val *r, char *err, int cap) {
    (void)x; (void)c; (void)i; (void)d;
    *r = as_missing();
    snprintf(err, (size_t)cap, "Não foi possível achar o aplicativo “%s”.", app);
    return -10814;
}
static int s_shell(void *x, const char *cmd, char **out, int *st) {
    (void)x;
    char full[4096];
    snprintf(full, sizeof full, "( %s ) 2>&1", cmd);
    FILE *p = popen(full, "r");
    if (!p) return -1;
    size_t cap = 1024, n = 0;
    char *b = malloc(cap);
    size_t r;
    while ((r = fread(b + n, 1, cap - n - 1, p)) > 0) { n += r; if (n + 1 >= cap) b = realloc(b, cap *= 2); }
    b[n] = 0;
    int s = pclose(p);
    *st = WIFEXITED(s) ? WEXITSTATUS(s) : 1;
    *out = b;
    return 0;
}
static void s_log(void *x, const char *t) { (void)x; fprintf(stderr, "%s\n", t); }
static double s_now(void *x) { (void)x; return 816436800.0; }
void cx_tcc_set_identity(const char *b) { (void)b; }
int cx_osascript_run(const char *src, const char *const *argv, int argc, int source_form, const char *me,
                     char **out, char *err, int errcap) {
    (void)me;
    as_host h = {0};
    h.send = s_send; h.shell = s_shell; h.log = s_log; h.now = s_now;
    as_val *args = argc > 0 ? calloc((usize)argc, sizeof *args) : 0;
    for (int i = 0; i < argc; i++) args[i] = as_txt(argv[i]);
    as_val res;
    char msg[400] = "";
    int line = 0, col = 0;
    int rc = as_run_ex(src, &h, 0, args, argc, &res, msg, sizeof msg, &line, &col);
    for (int i = 0; i < argc; i++) as_free(&args[i]);
    free(args);
    *out = 0;
    if (rc) { snprintf(err, (usize)errcap, "%d:%d: %s: %s (%d)", line, col, rc == -2741 ? "syntax error" : "execution error", msg, rc); return rc; }
    if (res.kind != AV_MISSING || source_form) { as_sb s = {0}; as_repr(&res, &s, source_form); *out = s.b ? s.b : as_strdup(""); }
    as_free(&res);
    return 0;
}
