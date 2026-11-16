/*
 * mOS libSystem — libquarantine (API privada da Apple que navegadores, instaladores e o Homebrew usam) sobre o xattr
 * com.apple.quarantine real (fsmeta.c). Formato do valor: "<flags hex 4>;<hora Unix hex 8>;<agente>;<UUID>".
 * Escrito a partir do formato público do atributo; os nomes das funções são os exportados pelo macOS.
 */
#include "sys.h"
#include "libc.h"

#define SET(e) (cx_errno = (e))
void *calloc(size_t n, size_t s);
void free(void *p);
long getxattr(const char *p, const char *n, void *v, size_t s, u32 pos, int o);
long fgetxattr(int fd, const char *n, void *v, size_t s, u32 pos, int o);
int setxattr(const char *p, const char *n, const void *v, size_t s, u32 pos, int o);
int fsetxattr(int fd, const char *n, const void *v, size_t s, u32 pos, int o);
long time(long *t);
void arc4random_buf(void *b, size_t n);

#define QTN_NOT_QUARANTINED (-1)
#define QTN_NAME "com.apple.quarantine"
const char *_qtn_xattr_name = QTN_NAME;

typedef struct _qtn_file_s {
    u32 flags;
    u64 ts;
    char agent[128], bundle[128], ident[40];
} qtn_file_s;

qtn_file_s *_qtn_file_alloc(void) { return calloc(1, sizeof(qtn_file_s)); }
void _qtn_file_free(qtn_file_s *q) { free(q); }
qtn_file_s *_qtn_file_clone(qtn_file_s *q) { qtn_file_s *c = _qtn_file_alloc(); if (c && q) *c = *q; return c; }

static int hexv(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
int _qtn_file_init_with_data(qtn_file_s *q, const void *data, size_t n) {
    const char *s = data;
    size_t i = 0;
    if (!q || !data || !n) return 22;
    memset(q, 0, sizeof *q);
    for (; i < n && s[i] != ';'; i++) { int v = hexv(s[i]); if (v < 0) return 22; q->flags = q->flags << 4 | (u32)v; }
    if (i >= n) return 0;
    for (i++; i < n && s[i] != ';'; i++) { int v = hexv(s[i]); if (v < 0) return 22; q->ts = q->ts << 4 | (u64)v; }
    size_t k = 0;
    for (i++; i < n && s[i] != ';' && s[i]; i++) if (k + 1 < sizeof q->agent) q->agent[k++] = s[i];
    q->agent[k] = 0;
    k = 0;
    for (i++; i < n && s[i] != ';' && s[i]; i++) if (k + 1 < sizeof q->ident) q->ident[k++] = s[i];
    q->ident[k] = 0;
    return 0;
}
static int load(qtn_file_s *q, long n, const char *buf) {
    if (n < 0) return cx_errno == 93 ? QTN_NOT_QUARANTINED : cx_errno;
    return _qtn_file_init_with_data(q, buf, (size_t)n);
}
int _qtn_file_init_with_path(qtn_file_s *q, const char *path) {
    char buf[512];
    return load(q, getxattr(path, QTN_NAME, buf, sizeof buf, 0, 0), buf);
}
int _qtn_file_init_with_fd(qtn_file_s *q, int fd) {
    char buf[512];
    return load(q, fgetxattr(fd, QTN_NAME, buf, sizeof buf, 0, 0), buf);
}
int qtn_file_init_with_mount_point(qtn_file_s *q, char mp[1024]) { return _qtn_file_init_with_path(q, mp); }

static void put_hex(char *o, u64 v, int digits) {
    static const char hx[] = "0123456789abcdef";
    for (int i = digits - 1; i >= 0; i--) { o[i] = hx[v & 15]; v >>= 4; }
}
int _qtn_file_to_data(qtn_file_s *q, void *buf, size_t *len) {
    char t[256];
    if (!q || !len) return 22;
    if (!q->ident[0]) {                                  /* UUID novo, como o LaunchServices grava */
        u8 r[16];
        arc4random_buf(r, 16);
        r[6] = (u8)((r[6] & 0x0f) | 0x40); r[8] = (u8)((r[8] & 0x3f) | 0x80);
        static const char hx[] = "0123456789ABCDEF";
        int o = 0;
        for (int i = 0; i < 16; i++) { if (i == 4 || i == 6 || i == 8 || i == 10) q->ident[o++] = '-'; q->ident[o++] = hx[r[i] >> 4]; q->ident[o++] = hx[r[i] & 15]; }
        q->ident[o] = 0;
    }
    if (!q->ts) q->ts = (u64)time(0);
    put_hex(t, q->flags, 4); t[4] = ';';
    put_hex(t + 5, q->ts, 8); t[13] = ';';
    size_t k = 14;
    for (const char *s = q->agent; *s && k < 200; s++) t[k++] = *s;
    t[k++] = ';';
    for (const char *s = q->ident; *s && k < 250; s++) t[k++] = *s;
    if (!buf) { *len = k; return 0; }
    if (*len < k) { *len = k; return 34; }
    memcpy(buf, t, k);
    *len = k;
    return 0;
}
int _qtn_file_apply_to_path(qtn_file_s *q, const char *path) {
    char t[256];
    size_t n = sizeof t;
    int e = _qtn_file_to_data(q, t, &n);
    if (e) return e;
    return setxattr(path, QTN_NAME, t, n, 0, 0) < 0 ? cx_errno : 0;
}
int _qtn_file_apply_to_fd(qtn_file_s *q, int fd) {
    char t[256];
    size_t n = sizeof t;
    int e = _qtn_file_to_data(q, t, &n);
    if (e) return e;
    return fsetxattr(fd, QTN_NAME, t, n, 0, 0) < 0 ? cx_errno : 0;
}
int qtn_file_apply_to_mount_point(qtn_file_s *q, const char *mp) { return _qtn_file_apply_to_path(q, mp); }
int _qtn_file_set_flags(qtn_file_s *q, u32 f) { if (!q) return 22; q->flags = f; return 0; }
u32 _qtn_file_get_flags(qtn_file_s *q) { return q ? q->flags : 0; }
int _qtn_file_set_timestamp(qtn_file_s *q, long ts) { if (!q) return 22; q->ts = (u64)ts; return 0; }
long _qtn_file_get_timestamp(qtn_file_s *q) { return q ? (long)q->ts : 0; }
int _qtn_file_set_agent_name(qtn_file_s *q, const char *s) { if (!q || !s) return 22; strlcpy(q->agent, s, sizeof q->agent); return 0; }
const char *_qtn_file_get_agent_name(qtn_file_s *q) { return q && q->agent[0] ? q->agent : 0; }
int _qtn_file_set_agent_bundle_identifier(qtn_file_s *q, const char *s) { if (!q || !s) return 22; strlcpy(q->bundle, s, sizeof q->bundle); return 0; }
const char *_qtn_file_get_agent_bundle_identifier(qtn_file_s *q) { return q && q->bundle[0] ? q->bundle : 0; }
int _qtn_file_set_identifier(qtn_file_s *q, const char *s) { if (!q || !s) return 22; strlcpy(q->ident, s, sizeof q->ident); return 0; }
const char *_qtn_file_get_identifier(qtn_file_s *q) { return q && q->ident[0] ? q->ident : 0; }
const char *_qtn_error(int err) {
    if (err == QTN_NOT_QUARANTINED) return "File is not quarantined";
    if (err == 22) return "Invalid argument";
    if (err == 34) return "Buffer too small";
    if (err == 1) return "Operation not permitted";
    return "Quarantine error";
}

/* quarentena de processo: arquivos que o processo cria herdam o atributo (o Safari e o Chrome usam). Guardado aqui;
 * o open(O_CREAT) do libSystem aplica (cx_qtn_on_create). */
typedef struct _qtn_proc_t { u32 flags; char ident[128]; } qtn_proc_s;
static qtn_proc_s g_proc;
static int g_proc_on;
qtn_proc_s *qtn_proc_alloc(void) { return calloc(1, sizeof(qtn_proc_s)); }
void qtn_proc_free(qtn_proc_s *p) { free(p); }
void qtn_proc_set_identifier(qtn_proc_s *p, const char *id) { if (p && id) strlcpy(p->ident, id, sizeof p->ident); }
void qtn_proc_set_flags(qtn_proc_s *p, unsigned f) { if (p) p->flags = f; }
int qtn_proc_init_with_data(qtn_proc_s *p, void *d, size_t n) { (void)d; (void)n; if (!p) return 22; memset(p, 0, sizeof *p); return 0; }
int qtn_proc_apply_to_self(qtn_proc_s *p) { if (!p) return 22; g_proc = *p; g_proc_on = 1; return 0; }
void cx_qtn_on_create(const char *path) {
    if (!g_proc_on || !path) return;
    qtn_file_s q;
    memset(&q, 0, sizeof q);
    q.flags = g_proc.flags | 0x1;                      /* baixado por este processo */
    strlcpy(q.agent, g_proc.ident[0] ? g_proc.ident : "mOS", sizeof q.agent);
    _qtn_file_apply_to_path(&q, path);
}

/* os mesmos nomes sem o "_" (o header da Apple os define como macros, mas há binários que ligam os dois) */
qtn_file_s *qtn_file_alloc(void) { return _qtn_file_alloc(); }
void qtn_file_free(qtn_file_s *q) { _qtn_file_free(q); }
qtn_file_s *qtn_file_clone(qtn_file_s *q) { return _qtn_file_clone(q); }
int qtn_file_init_with_path(qtn_file_s *q, const char *p) { return _qtn_file_init_with_path(q, p); }
int qtn_file_init_with_fd(qtn_file_s *q, int fd) { return _qtn_file_init_with_fd(q, fd); }
int qtn_file_init_with_data(qtn_file_s *q, const void *d, size_t n) { return _qtn_file_init_with_data(q, d, n); }
int qtn_file_to_data(qtn_file_s *q, void *b, size_t *n) { return _qtn_file_to_data(q, b, n); }
int qtn_file_apply_to_path(qtn_file_s *q, const char *p) { return _qtn_file_apply_to_path(q, p); }
int qtn_file_apply_to_fd(qtn_file_s *q, int fd) { return _qtn_file_apply_to_fd(q, fd); }
int qtn_file_set_flags(qtn_file_s *q, u32 f) { return _qtn_file_set_flags(q, f); }
u32 qtn_file_get_flags(qtn_file_s *q) { return _qtn_file_get_flags(q); }
const char *qtn_error(int e) { return _qtn_error(e); }
