/*
 * mOS — libSystem: log unificado (os_log), syslog(3) e asl(3), todos mandando para o logd (userland/logd.c).
 *
 * os_log: o compilador monta, para cada chamada, o formato (em __oslogstring) e um buffer com os argumentos:
 *     byte 0 = resumo, byte 1 = nº de argumentos, depois cada um: descritor (tipo << 4 | privacidade), tamanho, dados.
 * Tipos: 0 escalar, 1 contagem (o '*' de largura/precisão), 2 string (ponteiro), 3 ponteiro (%P), 4 objeto (%@).
 * A formatação refaz o printf com cada argumento (as anotações %{public}, %{private}... saem; o mOS não esconde
 * dados privados). Níveis gravados: padrão, info, erro e falha; debug não (como o macOS sem perfil de debug).
 * OS_ACTIVITY_MODE=disable desliga tudo; OS_ACTIVITY_DT_MODE (o que o Xcode põe) também ecoa no stderr.
 *
 * A mensagem vai ao logd por Mach (LG_WRITE, só envio, sem esperar); o logd é iniciado sob demanda pelo launchd.
 * `log show` (/usr/bin/log) lê o store.
 */
#include "foundation.h"
#include "libc.h"
#include "../svcproto.h"

int cx_svc_call(const char *svc, const svc_body_t *in, const void *s1, unsigned n1, const void *s2, unsigned n2,
                unsigned give_port, int give_disp, void *rbuf, unsigned rcap, unsigned timeout_ms);
int vsnprintf(char *b, size_t n, const char *fmt, va_list ap);
const char *getprogname(void);
int sched_yield(void);

/* ---------------------------------------------------------------- objetos os_log_t */
typedef struct { char magic[8]; char *subsystem, *category; int disabled; } oslog_t;
oslog_t _os_log_default = { "oslog", 0, 0, 0 };          /* OS_LOG_DEFAULT = &_os_log_default */
oslog_t _os_log_disabled = { "oslog", 0, 0, 1 };         /* OS_LOG_DISABLED */
static oslog_t *logs[128];
static int nlogs;
static volatile int llk;

void *os_log_create(const char *subsystem, const char *category) {
    if (!subsystem) subsystem = "";
    if (!category) category = "";
    while (__sync_lock_test_and_set(&llk, 1)) sched_yield();
    oslog_t *r = 0;
    for (int i = 0; i < nlogs && !r; i++) if (!strcmp(logs[i]->subsystem, subsystem) && !strcmp(logs[i]->category, category)) r = logs[i];
    if (!r && (r = calloc(1, sizeof *r))) {
        memcpy(r->magic, "oslog", 6);
        r->subsystem = strdup(subsystem);
        r->category = strdup(category);
        if (nlogs < 128) logs[nlogs++] = r;
    }
    __sync_lock_release(&llk);
    return r ? r : &_os_log_default;
}

static int mode = -1;             /* 0 desligado, 1 normal, 2 normal + stderr */
static int log_mode(void) {
    if (mode < 0) {
        const char *m = getenv("OS_ACTIVITY_MODE");
        mode = m && !strcmp(m, "disable") ? 0 : getenv("OS_ACTIVITY_DT_MODE") ? 2 : 1;
    }
    return mode;
}
int os_log_type_enabled(void *log, unsigned char type) {
    oslog_t *l = log;
    if (!log_mode() || (l && l->disabled)) return 0;
    return type != 2;             /* OS_LOG_TYPE_DEBUG */
}

/* ---------------------------------------------------------------- envio ao logd */
static void send_log(unsigned level, unsigned source, const char *subsys, const char *cat, const char *msg, size_t mlen) {
    char meta[384];
    const char *proc = getprogname();
    if (!proc) proc = "?";
    size_t a = strlen(proc), b = strlen(subsys), c = strlen(cat);
    if (a + b + c + 3 > sizeof meta) { b = 0; c = 0; if (a + 3 > sizeof meta) a = sizeof meta - 3; }
    memcpy(meta, proc, a); meta[a] = 0;
    memcpy(meta + a + 1, subsys, b); meta[a + 1 + b] = 0;
    memcpy(meta + a + b + 2, cat, c); meta[a + b + c + 2] = 0;
    if (mlen > SVC_MAX_MSG - 1024) mlen = SVC_MAX_MSG - 1024;
    svc_body_t q = { LG_WRITE, level, (unsigned)getpid(), source, 0, 0, 0 };
    cx_svc_call(LOGD_SERVICE, &q, meta, (unsigned)(a + b + c + 3), msg, (unsigned)mlen, 0, 0, 0, 0, 0);
    if (log_mode() == 2) {
        char line[1200];
        int n = snprintf(line, sizeof line, "%s[%d] %.*s\n", proc, getpid(), (int)(mlen < 1000 ? mlen : 1000), msg);
        if (n > 0) write(2, line, (size_t)(n < (int)sizeof line ? n : (int)sizeof line - 1));
    }
}

/* ---------------------------------------------------------------- formatação do buffer do os_log */
typedef struct { const unsigned char *p, *end; } oarg_t;
static int next_arg(oarg_t *a, unsigned *type, const unsigned char **data, unsigned *size) {
    if (a->p + 2 > a->end) return 0;
    *type = a->p[0] >> 4;
    *size = a->p[1];
    *data = a->p + 2;
    if (*data + *size > a->end) return 0;
    a->p = *data + *size;
    return 1;
}
static long long rd_int(const unsigned char *d, unsigned n, int is_signed) {
    if (n == 1) return is_signed ? (long long)(signed char)d[0] : d[0];
    if (n == 2) { short v; memcpy(&v, d, 2); return is_signed ? v : (unsigned short)v; }
    if (n == 4) { int v; memcpy(&v, d, 4); return is_signed ? v : (unsigned)v; }
    long long v = 0;
    if (n >= 8) memcpy(&v, d, 8);
    return v;
}
static size_t format_oslog(char *out, size_t cap, const char *fmt, const unsigned char *buf, unsigned size) {
    oarg_t args = { buf ? buf + 2 : 0, buf && size >= 2 ? buf + size : 0 };
    size_t o = 0;
#define OUT(s, n) do { size_t _n = (n); if (o + _n >= cap) _n = cap - 1 - o; memcpy(out + o, (s), _n); o += _n; } while (0)
    for (const char *f = fmt ? fmt : ""; *f && o + 1 < cap; f++) {
        if (*f != '%') { out[o++] = *f; continue; }
        if (f[1] == '%') { out[o++] = '%'; f++; continue; }
        const char *start = f++;
        char annot[32] = "";
        if (*f == '{') {
            const char *e = strchr(f, '}');
            if (!e) break;
            size_t n = (size_t)(e - f - 1) < sizeof annot - 1 ? (size_t)(e - f - 1) : sizeof annot - 1;
            memcpy(annot, f + 1, n); annot[n] = 0;
            f = e + 1;
        }
        char spec[32];
        size_t sn = 0;
        spec[sn++] = '%';
        int width = -1, prec = -1, have_w = 0, have_p = 0;
        while (*f && strchr("-+ #0'", *f)) { if (sn < 20) spec[sn++] = *f; f++; }
        if (*f == '*') {
            unsigned t, n; const unsigned char *d;
            if (next_arg(&args, &t, &d, &n)) width = (int)rd_int(d, n, 1);
            have_w = 1; f++;
        } else while (*f >= '0' && *f <= '9') { if (sn < 20) spec[sn++] = *f; f++; }
        if (have_w && sn < 16) sn += (size_t)snprintf(spec + sn, sizeof spec - sn, "%d", width);
        if (*f == '.') {
            f++;
            if (*f == '*') {
                unsigned t, n; const unsigned char *d;
                if (next_arg(&args, &t, &d, &n)) prec = (int)rd_int(d, n, 1);
                have_p = 1; f++;
            } else { prec = 0; while (*f >= '0' && *f <= '9') prec = prec * 10 + (*f++ - '0'); have_p = 1; }
            if (sn < 16) sn += (size_t)snprintf(spec + sn, sizeof spec - sn, ".%d", prec < 0 ? 0 : prec);
        }
        int lng = 0;
        while (*f && strchr("hlqjztL", *f)) { if (*f == 'l' || *f == 'q' || *f == 'j' || *f == 'z' || *f == 't') lng++; f++; }
        char conv = *f;
        if (!conv) break;
        unsigned t = 0, n = 0;
        const unsigned char *d = 0;
        if (!next_arg(&args, &t, &d, &n)) { OUT(start, (size_t)(f - start + 1)); continue; }
        char tmp[512];
        int tn = 0;
        if (conv == 's' || conv == 'S') {
            const char *s = 0;
            if (n == 8) memcpy(&s, d, 8);
            if (!s) s = "(null)";
            spec[sn++] = 's'; spec[sn] = 0;
            tn = snprintf(tmp, sizeof tmp, spec, s);
        } else if (conv == '@') {
            id obj = 0;
            if (n == 8) memcpy(&obj, d, 8);
            const char *s = "(null)";
            if (obj) { id desc = MSG0(id, obj, "description"); const char *c = desc ? cx_cstr(desc) : 0; if (c) s = c; }
            tn = snprintf(tmp, sizeof tmp, "%s", s);
        } else if (conv == 'P') {
            tn = snprintf(tmp, sizeof tmp, "<%u bytes>", have_p && prec > 0 ? (unsigned)prec : 0);
        } else if (conv == 'f' || conv == 'e' || conv == 'g' || conv == 'a' || conv == 'F' || conv == 'E' || conv == 'G') {
            double v = 0;
            if (n == 8) memcpy(&v, d, 8); else if (n == 4) { float fv; memcpy(&fv, d, 4); v = fv; }
            spec[sn++] = conv; spec[sn] = 0;
            tn = snprintf(tmp, sizeof tmp, spec, v);
        } else if (conv == 'p') {
            long long v = rd_int(d, n, 0);
            tn = snprintf(tmp, sizeof tmp, "0x%llx", (unsigned long long)v);
        } else if (strchr("diouxXcCm", conv)) {
            int sg = conv == 'd' || conv == 'i';
            long long v = rd_int(d, n, sg);
            if (!strcmp(annot, "bool") || !strcmp(annot, "BOOL")) tn = snprintf(tmp, sizeof tmp, "%s", v ? (annot[0] == 'B' ? "YES" : "true") : (annot[0] == 'B' ? "NO" : "false"));
            else if (!strcmp(annot, "errno") || conv == 'm') tn = snprintf(tmp, sizeof tmp, "[%lld: %s]", v, strerror((int)v));
            else if (conv == 'c' || conv == 'C') { tmp[0] = (char)v; tn = 1; }
            else {
                spec[sn++] = 'l'; spec[sn++] = 'l'; spec[sn++] = conv; spec[sn] = 0;
                tn = snprintf(tmp, sizeof tmp, spec, v);
            }
        } else {
            OUT(start, (size_t)(f - start + 1));
            continue;
        }
        (void)lng; (void)t;
        if (tn > 0) OUT(tmp, (size_t)(tn < (int)sizeof tmp ? tn : (int)sizeof tmp - 1));
    }
    out[o] = 0;
    return o;
#undef OUT
}

static void oslog_impl(void *log, unsigned char type, const char *fmt, void *buf, unsigned size) {
    oslog_t *l = log;
    if (!os_log_type_enabled(log, type)) return;
    char *msg = malloc(8192);
    if (!msg) return;
    size_t n = format_oslog(msg, 8192, fmt, buf, size);
    send_log(type, 0, l && l->subsystem ? l->subsystem : "", l && l->category ? l->category : "", msg, n);
    free(msg);
}
void _os_log_impl(void *dso, void *log, unsigned char type, const char *fmt, void *buf, unsigned size) { (void)dso; oslog_impl(log, type, fmt, buf, size); }
void _os_log_error_impl(void *dso, void *log, unsigned char type, const char *fmt, void *buf, unsigned size) { (void)dso; oslog_impl(log, type, fmt, buf, size); }
void _os_log_debug_impl(void *dso, void *log, unsigned char type, const char *fmt, void *buf, unsigned size) { (void)dso; oslog_impl(log, type, fmt, buf, size); }
void _os_log_fault_impl(void *dso, void *log, unsigned char type, const char *fmt, void *buf, unsigned size) { (void)dso; oslog_impl(log, type, fmt, buf, size); }

/* ---------------------------------------------------------------- syslog(3) */
static char sl_ident[64];
static int sl_opt, sl_mask = 0xff;
#define LOG_PERROR 0x20
static unsigned prio_type(int p) { p &= 7; return p <= 3 ? 16 : p == 6 ? 1 : p == 7 ? 2 : 0; }
static void sl_send(int prio, const char *msg, size_t n) {
    if (!(sl_mask & (1 << (prio & 7)))) return;
    send_log(prio_type(prio), 1u | (unsigned)(prio & 7) << 8, sl_ident[0] ? sl_ident : "", "", msg, n);
    if (sl_opt & LOG_PERROR) { write(2, msg, n); write(2, "\n", 1); }
}
/* %m = strerror(errno), antes do vsnprintf */
static const char *fix_m(const char *fmt, char *out, size_t cap) {
    if (!strstr(fmt, "%m")) return fmt;
    const char *es = strerror(cx_errno);
    size_t o = 0;
    for (const char *f = fmt; *f && o + 1 < cap; f++) {
        if (f[0] == '%' && f[1] == '%') { if (o + 2 < cap) { out[o++] = '%'; out[o++] = '%'; } f++; continue; }
        if (f[0] == '%' && f[1] == 'm') { for (const char *e = es; *e && o + 1 < cap; e++) out[o++] = *e == '%' ? ' ' : *e; f++; continue; }
        out[o++] = *f;
    }
    out[o] = 0;
    return out;
}
void vsyslog(int prio, const char *fmt, va_list ap) {
    char f2[512], b[2048];
    int n = vsnprintf(b, sizeof b, fix_m(fmt ? fmt : "", f2, sizeof f2), ap);
    if (n < 0) return;
    sl_send(prio, b, (size_t)(n < (int)sizeof b ? n : (int)sizeof b - 1));
}
void syslog(int prio, const char *fmt, ...) { va_list ap; va_start(ap, fmt); vsyslog(prio, fmt, ap); va_end(ap); }
void syslog_darwin(int prio, const char *fmt, ...) __asm__("syslog$DARWIN_EXTSN");
void syslog_darwin(int prio, const char *fmt, ...) { va_list ap; va_start(ap, fmt); vsyslog(prio, fmt, ap); va_end(ap); }
void openlog(const char *ident, int opt, int fac) {
    (void)fac;
    sl_opt = opt;
    if (ident) { size_t n = strlen(ident); if (n >= sizeof sl_ident) n = sizeof sl_ident - 1; memcpy(sl_ident, ident, n); sl_ident[n] = 0; }
}
void closelog(void) { sl_ident[0] = 0; sl_opt = 0; }
int setlogmask(int mask) { int old = sl_mask; if (mask) sl_mask = mask; return old; }

/* ---------------------------------------------------------------- asl(3) (obsoleto, mas ainda importado) */
typedef struct { char magic[4]; int n; char *k[32], *v[32]; } aslmsg_t;
typedef struct { char magic[4]; char ident[64]; int opts, filter; } aslclient_t;
void *asl_open(const char *ident, const char *facility, unsigned opts) {
    (void)facility;
    aslclient_t *c = calloc(1, sizeof *c);
    if (!c) return 0;
    memcpy(c->magic, "aslc", 4);
    if (ident) strlcpy(c->ident, ident, sizeof c->ident);
    c->opts = (int)opts;
    c->filter = 0xff;
    return c;
}
void asl_close(void *c) { free(c); }
void *asl_new(unsigned type) { (void)type; aslmsg_t *m = calloc(1, sizeof *m); if (m) memcpy(m->magic, "aslm", 4); return m; }
int asl_set(void *msg, const char *key, const char *val) {
    aslmsg_t *m = msg;
    if (!m || !key) return -1;
    for (int i = 0; i < m->n; i++) if (!strcmp(m->k[i], key)) { free(m->v[i]); m->v[i] = strdup(val ? val : ""); return 0; }
    if (m->n == 32) return -1;
    m->k[m->n] = strdup(key);
    m->v[m->n++] = strdup(val ? val : "");
    return 0;
}
const char *asl_get(void *msg, const char *key) {
    aslmsg_t *m = msg;
    if (!m || !key || memcmp(m->magic, "aslm", 4)) return 0;
    for (int i = 0; i < m->n; i++) if (!strcmp(m->k[i], key)) return m->v[i];
    return 0;
}
int asl_unset(void *msg, const char *key) { aslmsg_t *m = msg; if (!m || !key) return -1; for (int i = 0; i < m->n; i++) if (!strcmp(m->k[i], key)) { free(m->k[i]); free(m->v[i]); m->k[i] = m->k[m->n - 1]; m->v[i] = m->v[m->n - 1]; m->n--; return 0; } return 0; }
void asl_free(void *msg) {
    aslmsg_t *m = msg;
    if (!m) return;
    if (!memcmp(m->magic, "aslm", 4)) for (int i = 0; i < m->n; i++) { free(m->k[i]); free(m->v[i]); }
    free(m);
}
void asl_release(void *o) { if (o && !memcmp(o, "aslm", 4)) asl_free(o); else if (o && !memcmp(o, "aslc", 4)) asl_close(o); }
int asl_vlog(void *client, void *msg, int level, const char *fmt, va_list ap) {
    aslclient_t *c = client;
    char b[2048], f2[512];
    int n = vsnprintf(b, sizeof b, fix_m(fmt ? fmt : "", f2, sizeof f2), ap);
    if (n < 0) return -1;
    const char *sender = asl_get(msg, "Sender");
    const char *fac = asl_get(msg, "Facility");
    if (c && c->ident[0] && !sender) sender = c->ident;
    send_log(prio_type(level), 1u | (unsigned)(level & 7) << 8, fac ? fac : "", sender ? sender : "", b, (size_t)(n < (int)sizeof b ? n : (int)sizeof b - 1));
    if (c && (c->opts & 1 /* ASL_OPT_STDERR */)) { write(2, b, strlen(b)); write(2, "\n", 1); }
    return 0;
}
int asl_log(void *client, void *msg, int level, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = asl_vlog(client, msg, level, fmt, ap); va_end(ap); return r; }
int asl_log_message(int level, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int r = asl_vlog(0, 0, level, fmt, ap); va_end(ap); return r; }
int asl_send(void *client, void *msg) {
    const char *t = asl_get(msg, "Message"), *l = asl_get(msg, "Level");
    int level = l ? atoi(l) : 5;
    return asl_log(client, msg, level, "%s", t ? t : "");
}
int asl_set_filter(void *client, int f) { aslclient_t *c = client; int old = c ? c->filter : 0xff; if (c) c->filter = f; return old; }
int asl_add_log_file(void *client, int fd) { (void)client; (void)fd; return 0; }
int asl_add_output_file(void *client, int fd, const char *mf, const char *tf, int filter, int encoding) { (void)client; (void)fd; (void)mf; (void)tf; (void)filter; (void)encoding; return 0; }
int asl_remove_log_file(void *client, int fd) { (void)client; (void)fd; return 0; }
