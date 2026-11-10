/*
 * libc++ / libc++abi que o OpenBot importa e o libSystem ainda não exportava
 * (Sunshine, onnxruntime, sherpa-onnx, koffi). Comportamento real no layout
 * da libc++ x86_64 (string não-alternada, iostream com base virtual).
 *
 * Limitações, não feature flags:
 * - não há desenrolamento de pilha: throw continua abortando (libcxx.c);
 *   current_exception fica vazio e rethrow_exception aborta.
 * - __dynamic_cast resolve hint >= 0, tipo exato, base primária e bases do
 *   __vmi_class_type_info não virtuais. Base virtual sem hint devolve nulo.
 * - to_chars de long double usa a precisão de double.
 * - locale com nome diferente de "C"/"POSIX" aborta.
 */
#include "sys.h"
#include "libc.h"
#include "libcxx_openbot_pure.inc"

extern int cx_errno;
void *cxx_new(size_t n);
void cxx_del(void *p);
int pthread_mutex_init(void *m, const void *a);
int pthread_mutex_lock(void *m);
int pthread_mutex_unlock(void *m);
int pthread_mutex_trylock(void *m);
int pthread_cond_init(void *c, const void *a);
int pthread_cond_broadcast(void *c);
int pthread_cond_signal(void *c);
int pthread_cond_wait(void *c, void *m);
int pthread_cond_timedwait(void *c, void *m, const void *ts);
int pthread_detach(unsigned long t);
unsigned long long strtoull(const char *s, char **end, int base);
long sysconf(int name);
int clock_gettime(int clk, void *ts);
int chmod(const char *p, unsigned short mode);
int chdir(const char *p);
int mkdir(const char *p, int mode);
int unlink(const char *p);
int rename(const char *a, const char *b);
int stat(const char *p, void *st);
int lstat(const char *p, void *st);
void *opendir(const char *p);
void *readdir(void *d);
int closedir(void *d);
char *strerror(int e);
char *getcwd(char *b, size_t n);
void *cx_locale_classic_imp(void);
extern unsigned long long numpunct_char_id[] __asm__("_ZNSt3__18numpunctIcE2idE");

typedef struct { u32 st_dev; u16 st_mode, st_nlink; u64 st_ino; u32 st_uid, st_gid, st_rdev;
    i64 atime, atime_ns, mtime, mtime_ns, ctime, ctime_ns, btime, btime_ns;
    i64 st_size, st_blocks; i32 st_blksize; u32 st_flags, st_gen; i32 lspare; i64 qspare[2]; } ob_stat;
typedef struct { u64 d_ino, d_seekoff; u16 d_reclen, d_namlen; u8 d_type; char d_name[1024]; } ob_dent;
typedef struct { char st[128]; long long off; } ob_fpos;
typedef struct { void *sb; unsigned char failed; } ob_siter;
typedef struct { void *vt; const char *msg; } ob_err;
typedef struct { void *vt; const char *msg; int code; const void *cat; } ob_syserr;
typedef struct { void *m; unsigned char owns; } ob_ulock;

static void ob_fail(const char *what) {
    cx_puts_fd(2, "libc++[mOS]: ");
    cx_puts_fd(2, what ? what : "erro");
    cx_puts_fd(2, "\n");
    abort();
}
static char *ob_dup(const char *s) {
    size_t n = strlen(s ? s : "") + 1;
    char *d = (char *)malloc(n);
    memcpy(d, s ? s : "", n);
    return d;
}

/* ======================== string<char> ======================== */
static void str_need(const ob_str *s, size_t pos) { if (pos > ob_ssize(s)) ob_fail("std::out_of_range: basic_string"); }

size_t str_find(const ob_str *s, char c, size_t pos) __asm__("_ZNKSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE4findEcm");
size_t str_find(const ob_str *s, char c, size_t pos) { return ob_sfind(s, c, pos); }
size_t str_rfind(const ob_str *s, char c, size_t pos) __asm__("_ZNKSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE5rfindEcm");
size_t str_rfind(const ob_str *s, char c, size_t pos) { return ob_srfind(s, c, pos); }
int str_cmp_c(const ob_str *s, const char *p) __asm__("_ZNKSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7compareEPKc");
int str_cmp_c(const ob_str *s, const char *p) { return ob_scmp_n(s, p, strlen(p ? p : "")); }
int str_cmp_pc(const ob_str *s, size_t pos, size_t n, const char *p) __asm__("_ZNKSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7compareEmmPKc");
int str_cmp_pc(const ob_str *s, size_t pos, size_t n, const char *p) {
    size_t sn = ob_ssize(s), pn;
    str_need(s, pos);
    if (n > sn - pos) n = sn - pos;
    pn = strlen(p ? p : "");
    { size_t m = n < pn ? n : pn; int c = m ? memcmp(ob_scdata(s) + pos, p, m) : 0; if (c) return c; return n < pn ? -1 : n > pn ? 1 : 0; }
}
int str_cmp_pcn(const ob_str *s, size_t pos, size_t n, const char *p, size_t pn) __asm__("_ZNKSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7compareEmmPKcm");
int str_cmp_pcn(const ob_str *s, size_t pos, size_t n, const char *p, size_t pn) {
    size_t sn = ob_ssize(s);
    str_need(s, pos);
    if (n > sn - pos) n = sn - pos;
    { size_t m = n < pn ? n : pn; int c = m ? memcmp(ob_scdata(s) + pos, p ? p : "", m) : 0; if (c) return c; return n < pn ? -1 : n > pn ? 1 : 0; }
}
ob_str *str_erase(ob_str *s, size_t pos, size_t n) __asm__("_ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE5eraseEmm");
ob_str *str_erase(ob_str *s, size_t pos, size_t n) {
    size_t sn = ob_ssize(s);
    str_need(s, pos);
    if (n > sn - pos) n = sn - pos;
    memmove(ob_sdata(s) + pos, ob_sdata(s) + pos + n, sn - pos - n);
    ob_sset(s, sn - n);
    return s;
}
ob_str *str_app_c(ob_str *s, const char *p) __asm__("_ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6appendEPKc");
ob_str *str_app_c(ob_str *s, const char *p) { ob_sappend_n(s, p ? p : "", strlen(p ? p : "")); return s; }
ob_str *str_app_sub(ob_str *s, const ob_str *o, size_t pos, size_t n) __asm__("_ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6appendERKS5_mm");
ob_str *str_app_sub(ob_str *s, const ob_str *o, size_t pos, size_t n) {
    size_t on = ob_ssize(o);
    str_need(o, pos);
    if (n > on - pos) n = on - pos;
    ob_sappend_n(s, ob_scdata(o) + pos, n);
    return s;
}
ob_str *str_as_c(ob_str *s, const char *p) __asm__("_ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6assignEPKc");
ob_str *str_as_c(ob_str *s, const char *p) { ob_sassign_n(s, p ? p : "", strlen(p ? p : "")); return s; }
ob_str *str_as_cn(ob_str *s, const char *p, size_t n) __asm__("_ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6assignEPKcm");
ob_str *str_as_cn(ob_str *s, const char *p, size_t n) { ob_sassign_n(s, p, n); return s; }
ob_str *str_as_sub(ob_str *s, const ob_str *o, size_t pos, size_t n) __asm__("_ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6assignERKS5_mm");
ob_str *str_as_sub(ob_str *s, const ob_str *o, size_t pos, size_t n) {
    size_t on = ob_ssize(o);
    str_need(o, pos);
    if (n > on - pos) n = on - pos;
    ob_sassign_n(s, ob_scdata(o) + pos, n);
    return s;
}
char *str_ins_it(ob_str *s, const char *pos, char c) __asm__("_ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6insertENS_11__wrap_iterIPKcEEc");
char *str_ins_it(ob_str *s, const char *pos, char c) {
    size_t i = pos ? (size_t)(pos - ob_scdata(s)) : 0;
    str_need(s, i);
    ob_sreserve(s, ob_ssize(s) + 1);
    memmove(ob_sdata(s) + i + 1, ob_sdata(s) + i, ob_ssize(s) - i);
    ob_sdata(s)[i] = c;
    ob_sset(s, ob_ssize(s) + 1);
    return ob_sdata(s) + i;
}
ob_str *str_ins_c(ob_str *s, size_t pos, const char *p) __asm__("_ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6insertEmPKc");
ob_str *str_ins_c(ob_str *s, size_t pos, const char *p) {
    size_t n = strlen(p ? p : ""), sn = ob_ssize(s);
    char *tmp;
    str_need(s, pos);
    tmp = (char *)malloc(n ? n : 1);
    memcpy(tmp, p ? p : "", n);
    ob_sreserve(s, sn + n);
    memmove(ob_sdata(s) + pos + n, ob_sdata(s) + pos, sn - pos);
    memcpy(ob_sdata(s) + pos, tmp, n);
    ob_sset(s, sn + n);
    free(tmp);
    return s;
}
ob_str *str_resize(ob_str *s, size_t n, char c) __asm__("_ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE6resizeEmc");
ob_str *str_resize(ob_str *s, size_t n, char c) {
    size_t sn = ob_ssize(s);
    if (n < sn) ob_sset(s, n);
    else if (n > sn) { ob_sreserve(s, n); memset(ob_sdata(s) + sn, c, n - sn); ob_sset(s, n); }
    return s;
}
ob_str *str_rep_c(ob_str *s, size_t pos, size_t n, const char *p) __asm__("_ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7replaceEmmPKc");
ob_str *str_rep_c(ob_str *s, size_t pos, size_t n, const char *p) { str_erase(s, pos, n); return str_ins_c(s, pos, p); }
ob_str *str_rep_cn(ob_str *s, size_t pos, size_t n, const char *p, size_t pn) __asm__("_ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7replaceEmmPKcm");
ob_str *str_rep_cn(ob_str *s, size_t pos, size_t n, const char *p, size_t pn) {
    size_t sn;
    char *tmp;
    str_erase(s, pos, n);
    sn = ob_ssize(s);
    tmp = (char *)malloc(pn ? pn : 1);
    memcpy(tmp, p ? p : "", pn);
    ob_sreserve(s, sn + pn);
    memmove(ob_sdata(s) + pos + pn, ob_sdata(s) + pos, sn - pos);
    memcpy(ob_sdata(s) + pos, tmp, pn);
    ob_sset(s, sn + pn);
    free(tmp);
    return s;
}
void str_reserve(ob_str *s, size_t n) __asm__("_ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEE7reserveEm");
void str_reserve(ob_str *s, size_t n) { ob_sreserve(s, n); }
void str_ctor(ob_str *d, const ob_str *s) __asm__("_ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEC1ERKS5_");
void str_ctor(ob_str *d, const ob_str *s) { ob_scopy(d, s); }
void str_ctor_sub(ob_str *d, const ob_str *s, size_t pos, size_t n, char alloc) __asm__("_ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEC1ERKS5_mmRKS4_");
void str_ctor_sub(ob_str *d, const ob_str *s, size_t pos, size_t n, char alloc) {
    size_t sn = ob_ssize(s);
    (void)alloc;
    str_need(s, pos);
    if (n > sn - pos) n = sn - pos;
    ob_sempty(d);
    ob_sassign_n(d, ob_scdata(s) + pos, n);
}
void str_dtor(ob_str *s) __asm__("_ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEED1Ev");
void str_dtor(ob_str *s) { ob_sfree(s); }
ob_str *str_eq(ob_str *d, const ob_str *s) __asm__("_ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEaSERKS5_");
ob_str *str_eq(ob_str *d, const ob_str *s) { if (d != s) ob_sassign_n(d, ob_scdata(s), ob_ssize(s)); return d; }
ob_str *str_eq_c(ob_str *d, char c) __asm__("_ZNSt3__112basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEaSEc");
ob_str *str_eq_c(ob_str *d, char c) { ob_sassign_n(d, &c, 1); return d; }

/* ======================== string<wchar_t> ======================== */
static void wgrow(ob_wstr *s, size_t old_cap, size_t delta, size_t old_sz, size_t n_copy, size_t n_del, size_t n_add) {
    size_t cap, sec;
    uint32_t *p, *old = ob_wdata(s);
    (void)old_cap;
    if (delta > (size_t)-1 / 4) ob_fail("std::length_error: basic_string");
    cap = old_cap + delta;
    if (cap < old_sz - n_del + n_add) cap = old_sz - n_del + n_add;
    p = (uint32_t *)malloc((cap + 1) * sizeof(uint32_t));
    if (n_copy) memcpy(p, old, n_copy * sizeof(uint32_t));
    sec = old_sz - n_del - n_copy;
    if (sec) memcpy(p + n_copy + n_add, old + n_copy + n_del, sec * sizeof(uint32_t));
    if (ob_wlong(s)) free(s->data);
    s->w0 = (cap + 1) | 1;
    s->size = old_sz - n_del + n_add;
    s->data = p;
    p[s->size] = 0;
}
void w_grow_rep(ob_wstr *s, size_t oc, size_t dc, size_t os, size_t nc, size_t nd, size_t na, const uint32_t *p) __asm__("_ZNSt3__112basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEE21__grow_by_and_replaceEmmmmmmPKw");
void w_grow_rep(ob_wstr *s, size_t oc, size_t dc, size_t os, size_t nc, size_t nd, size_t na, const uint32_t *p) {
    wgrow(s, oc, dc, os, nc, nd, na);
    if (na && p) memcpy(ob_wdata(s) + nc, p, na * sizeof(uint32_t));
}
void w_grow(ob_wstr *s, size_t oc, size_t dc, size_t os, size_t nc, size_t nd, size_t na) __asm__("_ZNSt3__112basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEE9__grow_byEmmmmmm");
void w_grow(ob_wstr *s, size_t oc, size_t dc, size_t os, size_t nc, size_t nd, size_t na) { wgrow(s, oc, dc, os, nc, nd, na); }
ob_wstr *w_app(ob_wstr *s, const uint32_t *p, size_t n) __asm__("_ZNSt3__112basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEE6appendEPKwm");
ob_wstr *w_app(ob_wstr *s, const uint32_t *p, size_t n) { ob_wappend_n(s, p, n); return s; }
ob_wstr *w_app_c(ob_wstr *s, size_t n, uint32_t c) __asm__("_ZNSt3__112basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEE6appendEmw");
ob_wstr *w_app_c(ob_wstr *s, size_t n, uint32_t c) {
    size_t i, len = ob_wsize(s);
    ob_wreserve(s, len + n);
    for (i = 0; i < n; i++) ob_wdata(s)[len + i] = c;
    ob_wset(s, len + n);
    return s;
}
ob_wstr *w_resize(ob_wstr *s, size_t n, uint32_t c) __asm__("_ZNSt3__112basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEE6resizeEmw");
ob_wstr *w_resize(ob_wstr *s, size_t n, uint32_t c) {
    size_t sn = ob_wsize(s);
    if (n < sn) ob_wset(s, n);
    else if (n > sn) w_app_c(s, n - sn, c);
    return s;
}
void w_reserve(ob_wstr *s, size_t n) __asm__("_ZNSt3__112basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEE7reserveEm");
void w_reserve(ob_wstr *s, size_t n) { ob_wreserve(s, n); }
void w_push(ob_wstr *s, uint32_t c) __asm__("_ZNSt3__112basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEE9push_backEw");
void w_push(ob_wstr *s, uint32_t c) { ob_wappend_n(s, &c, 1); }
ob_wstr *w_eq(ob_wstr *d, const ob_wstr *s) __asm__("_ZNSt3__112basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEEaSERKS5_");
ob_wstr *w_eq(ob_wstr *d, const ob_wstr *s) {
    if (d != s) { size_t n = ob_wsize(s); ob_wreserve(d, n); memcpy(ob_wdata(d), ob_wdata((ob_wstr *)s), (n + 1) * 4); ob_wset(d, n); }
    return d;
}
ob_wstr *w_eq_c(ob_wstr *d, uint32_t c) __asm__("_ZNSt3__112basic_stringIwNS_11char_traitsIwEENS_9allocatorIwEEEaSEw");
ob_wstr *w_eq_c(ob_wstr *d, uint32_t c) { ob_wreserve(d, 1); ob_wdata(d)[0] = c; ob_wset(d, 1); return d; }

/* ======================== exceções e RTTI ======================== */
extern const void *vt_class_ti[] __asm__("_ZTVN10__cxxabiv117__class_type_infoE");
extern const void *vt_si_ti[] __asm__("_ZTVN10__cxxabiv120__si_class_type_infoE");
extern const void *vt_vmi_ti[] __asm__("_ZTVN10__cxxabiv121__vmi_class_type_infoE");
extern const char *rt_what(const ob_err *e);
static void err_free(ob_err *e) { if (e && e->msg) { free((void *)e->msg); e->msg = 0; } }
static const char *logic_what(const ob_err *e) { return e->msg ? e->msg : "std::logic_error"; }
static const char *bad_what(const void *e) { (void)e; return "std::bad_alloc"; }
static const char *bcast_what(const void *e) { (void)e; return "std::bad_cast"; }
static const char *bex_what(const void *e) { (void)e; return "std::bad_exception"; }
static void err_d1(void *e) { err_free(e); }
static void err_d0(void *e) { err_free(e); free(e); }
static void noop_d(void *e) { (void)e; }
static void del_d(void *e) { free(e); }

const char *logic_what_ex(const ob_err *e) __asm__("_ZNKSt11logic_error4whatEv");
const char *logic_what_ex(const ob_err *e) { return logic_what(e); }
const char *bex_what_ex(const void *e) __asm__("_ZNKSt13bad_exception4whatEv");
const char *bex_what_ex(const void *e) { return bex_what(e); }
const char *bada_what_ex(const void *e) __asm__("_ZNKSt9bad_alloc4whatEv");
const char *bada_what_ex(const void *e) { return bad_what(e); }

static const void *vt_logic[5] = {0, 0, (void *)err_d1, (void *)err_d0, (void *)logic_what};
static const void *vt_runtime[5] = {0, 0, (void *)err_d1, (void *)err_d0, (void *)logic_what};
const void *vt_inval[5] __asm__("_ZTVSt16invalid_argument") = {0, 0, (void *)err_d1, (void *)err_d0, (void *)logic_what};
const void *vt_bvar[5] __asm__("_ZTVSt18bad_variant_access") = {0, 0, (void *)noop_d, (void *)del_d, (void *)bex_what};
const void *vt_bopt[5] __asm__("_ZTVSt19bad_optional_access") = {0, 0, (void *)noop_d, (void *)del_d, (void *)bex_what};
static const void *vt_bcast[5] = {0, 0, (void *)noop_d, (void *)del_d, (void *)bcast_what};
const void *vt_bada[5] __asm__("_ZTVSt9bad_alloc") = {0, 0, (void *)noop_d, (void *)del_d, (void *)bad_what};
const void *vt_sys[5] __asm__("_ZTVNSt3__112system_errorE") = {0, 0, (void *)err_d1, (void *)err_d0, (void *)logic_what};

static void logic_fill(ob_err *e, const void **vt, const char *m) { e->vt = (void *)(vt + 2); e->msg = ob_dup(m ? m : ""); }
void logic_c1(ob_err *e, const char *m) __asm__("_ZNSt11logic_errorC1EPKc");
void logic_c1(ob_err *e, const char *m) { logic_fill(e, vt_logic, m); }
void logic_c1s(ob_err *e, const ob_str *s) __asm__("_ZNSt11logic_errorC1ERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE");
void logic_c1s(ob_err *e, const ob_str *s) { logic_fill(e, vt_logic, ob_scdata(s)); }
void logic_c2s(ob_err *e, const ob_str *s) __asm__("_ZNSt11logic_errorC2ERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE");
void logic_c2s(ob_err *e, const ob_str *s) { logic_c1s(e, s); }
void logic_c2copy(ob_err *e, const ob_err *o) __asm__("_ZNSt11logic_errorC2ERKS_");
void logic_c2copy(ob_err *e, const ob_err *o) { logic_fill(e, vt_logic, o->msg); }
void logic_d1(ob_err *e) __asm__("_ZNSt11logic_errorD1Ev");
void logic_d1(ob_err *e) { err_free(e); }
void logic_d2(ob_err *e) __asm__("_ZNSt11logic_errorD2Ev");
void logic_d2(ob_err *e) { err_free(e); }
void bex_d2(void *e) __asm__("_ZNSt13bad_exceptionD2Ev");
void bex_d2(void *e) { (void)e; }
void rt_c1(ob_err *e, const char *m) __asm__("_ZNSt13runtime_errorC1EPKc");
void rt_c1(ob_err *e, const char *m) { logic_fill(e, vt_runtime, m); }
void rt_c1s(ob_err *e, const ob_str *s) __asm__("_ZNSt13runtime_errorC1ERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE");
void rt_c1s(ob_err *e, const ob_str *s) { logic_fill(e, vt_runtime, ob_scdata(s)); }
void rt_c1copy(ob_err *e, const ob_err *o) __asm__("_ZNSt13runtime_errorC1ERKS_");
void rt_c1copy(ob_err *e, const ob_err *o) { logic_fill(e, vt_runtime, o->msg); }
void rt_c2s(ob_err *e, const ob_str *s) __asm__("_ZNSt13runtime_errorC2ERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE");
void rt_c2s(ob_err *e, const ob_str *s) { rt_c1s(e, s); }
void rt_c2copy(ob_err *e, const ob_err *o) __asm__("_ZNSt13runtime_errorC2ERKS_");
void rt_c2copy(ob_err *e, const ob_err *o) { rt_c1copy(e, o); }
void rt_d1(ob_err *e) __asm__("_ZNSt13runtime_errorD1Ev");
void rt_d1(ob_err *e) { err_free(e); }
ob_err *rt_eq(ob_err *d, const ob_err *s) __asm__("_ZNSt13runtime_erroraSERKS_");
ob_err *rt_eq(ob_err *d, const ob_err *s) { err_free(d); d->msg = ob_dup(s->msg); d->vt = s->vt; return d; }
void inval_d1(void *e) __asm__("_ZNSt16invalid_argumentD1Ev");
void inval_d1(void *e) { err_free(e); }
void bopt_d1(void *e) __asm__("_ZNSt19bad_optional_accessD1Ev");
void bopt_d1(void *e) { (void)e; }

static const void *sys_cat(void);
static void ec_set(void *ec, int err) {
    if (!ec) { if (err) ob_fail(strerror(err)); return; }
    ((int *)ec)[0] = err;
    ((const void **)ec)[1] = sys_cat();
}
void ec_message(ob_str *ret, const void *ec) __asm__("_ZNKSt3__110error_code7messageEv");
void ec_message(ob_str *ret, const void *ec) {
    const void *cat = ((const void **)ec)[1];
    int ev = ((const int *)ec)[0];
    ob_sempty(ret);
    if (cat) {
        void **vt = *(void ***)cat;
        ((void (*)(ob_str *, const void *, int))vt[6])(ret, cat, ev);
    }
}
static const char *cat_sys_name(const void *c) { (void)c; return "system"; }
static const char *cat_fut_name(const void *c) { (void)c; return "future"; }
static void cat_msg(ob_str *ret, const void *c, int ev) { (void)c; ob_sempty(ret); ob_sassign_n(ret, strerror(ev), strlen(strerror(ev))); }
static unsigned long cat_def(const void *c, int ev) { (void)c; return (unsigned)ev; }
static unsigned char cat_eq(const void *c, int a, const void *b) { (void)c; (void)a; (void)b; return 0; }
static void cat_d2(void *c) { (void)c; }
static const void *vt_syscat[9] = {0, 0, (void *)cat_d2, (void *)cat_d2, (void *)cat_sys_name, (void *)cat_def, (void *)cat_eq, (void *)cat_eq, (void *)cat_msg};
static const void *vt_futcat[9] = {0, 0, (void *)cat_d2, (void *)cat_d2, (void *)cat_fut_name, (void *)cat_def, (void *)cat_eq, (void *)cat_eq, (void *)cat_msg};
static const void *g_syscat = &vt_syscat[2];
static const void *g_futcat = &vt_futcat[2];
static const void *sys_cat(void) { return &g_syscat; }
const void *system_category(void) __asm__("_ZNSt3__115system_categoryEv");
const void *system_category(void) { return &g_syscat; }
const void *future_category(void) __asm__("_ZNSt3__115future_categoryEv");
const void *future_category(void) { return &g_futcat; }
void cat_dtor(void *c) __asm__("_ZNSt3__114error_categoryD2Ev");
void cat_dtor(void *c) { (void)c; }

void sys_c2(ob_syserr *e, int code, const void *cat) __asm__("_ZNSt3__112system_errorC2ENS_10error_codeE");
void sys_c2(ob_syserr *e, int code, const void *cat) { e->vt = (void *)&vt_sys[2]; e->msg = ob_dup(strerror(code)); e->code = code; e->cat = cat; }
void sys_c2w(ob_syserr *e, int code, const void *cat, const char *w) __asm__("_ZNSt3__112system_errorC2ENS_10error_codeEPKc");
void sys_c2w(ob_syserr *e, int code, const void *cat, const char *w) { e->vt = (void *)&vt_sys[2]; e->msg = ob_dup(w ? w : ""); e->code = code; e->cat = cat; }
void sys_d2(ob_syserr *e) __asm__("_ZNSt3__112system_errorD2Ev");
void sys_d2(ob_syserr *e) { err_free((ob_err *)e); }
void fut_c1(ob_syserr *e, int code, const void *cat) __asm__("_ZNSt3__112future_errorC1ENS_10error_codeE");
void fut_c1(ob_syserr *e, int code, const void *cat) { sys_c2(e, code, cat); }
void fut_d1(ob_syserr *e) __asm__("_ZNSt3__112future_errorD1Ev");
void fut_d1(ob_syserr *e) { err_free((ob_err *)e); }
void rx_c1(ob_err *e, int ty) __asm__("_ZNSt3__111regex_errorC1ENS_15regex_constants10error_typeE");
void rx_c1(ob_err *e, int ty) { logic_fill(e, vt_runtime, "regex_error"); ((int *)e)[4] = ty; }
void rx_d1(ob_err *e) __asm__("_ZNSt3__111regex_errorD1Ev");
void rx_d1(ob_err *e) { err_free(e); }
void bcast_c1(void **e) __asm__("_ZNSt8bad_castC1Ev");
void bcast_c1(void **e) { *e = (void *)&vt_bcast[2]; }
void bcast_c2(void **e) __asm__("_ZNSt8bad_castC2Ev");
void bcast_c2(void **e) { bcast_c1(e); }
void bcast_d1(void *e) __asm__("_ZNSt8bad_castD1Ev");
void bcast_d1(void *e) { (void)e; }
void bcast_d2(void *e) __asm__("_ZNSt8bad_castD2Ev");
void bcast_d2(void *e) { (void)e; }
void bada_c1(void **e) __asm__("_ZNSt9bad_allocC1Ev");
void bada_c1(void **e) { *e = (void *)&vt_bada[2]; }
void bada_c2(void **e) __asm__("_ZNSt9bad_allocC2Ev");
void bada_c2(void **e) { bada_c1(e); }
void bada_d1(void *e) __asm__("_ZNSt9bad_allocD1Ev");
void bada_d1(void *e) { (void)e; }
void bada_d2(void *e) __asm__("_ZNSt9bad_allocD2Ev");
void bada_d2(void *e) { (void)e; }
void throw_bad_alloc(void) __asm__("_ZSt17__throw_bad_allocv");
void throw_bad_alloc(void) { ob_fail("std::bad_alloc"); }
void vec_throw_len(const void *v) __asm__("_ZNKSt3__120__vector_base_commonILb1EE20__throw_length_errorEv");
void vec_throw_len(const void *v) { (void)v; ob_fail("std::length_error: vector"); }

/* exception_ptr: ponteiro com refcount. Sem unwinding não há exceção corrente. */
typedef struct { long refs; void *obj; } ob_ep;
static void ep_hold(ob_ep *e) { if (e) __atomic_add_fetch(&e->refs, 1, __ATOMIC_RELAXED); }
static void ep_drop(ob_ep *e) { if (e && __atomic_sub_fetch(&e->refs, 1, __ATOMIC_ACQ_REL) == 0) free(e); }
void ep_from(ob_ep **ret, void *p) __asm__("_ZNSt13exception_ptr31__from_native_exception_pointerEPv");
void ep_from(ob_ep **ret, void *p) { ob_ep *e = 0; if (p) { e = (ob_ep *)calloc(1, sizeof *e); e->refs = 1; e->obj = p; } *ret = e; }
void ep_copy(ob_ep **d, ob_ep *const *s) __asm__("_ZNSt13exception_ptrC1ERKS_");
void ep_copy(ob_ep **d, ob_ep *const *s) { *d = *s; ep_hold(*d); }
void ep_dtor(ob_ep **d) __asm__("_ZNSt13exception_ptrD1Ev");
void ep_dtor(ob_ep **d) { ep_drop(*d); *d = 0; }
ob_ep **ep_eq(ob_ep **d, ob_ep *const *s) __asm__("_ZNSt13exception_ptraSERKS_");
ob_ep **ep_eq(ob_ep **d, ob_ep *const *s) { if (*d != *s) { ep_hold(*s); ep_drop(*d); *d = *s; } return d; }
void cur_exc(ob_ep **ret) __asm__("_ZSt17current_exceptionv");
void cur_exc(ob_ep **ret) { *ret = 0; } /* stub de unwinding: não há exceção viva */
void rethrow_exc(ob_ep **e) __asm__("_ZSt17rethrow_exceptionSt13exception_ptr");
void rethrow_exc(ob_ep **e) { (void)e; ob_fail("std::rethrow_exception (sem desenrolamento)"); }
int uncaught1(void) __asm__("_ZSt18uncaught_exceptionv");
int uncaught1(void) { return 0; }
int uncaughtn(void) __asm__("_ZSt19uncaught_exceptionsv");
int uncaughtn(void) { return 0; }
char std_nothrow __asm__("_ZSt7nothrow");
void *cxa_init_primary(void *obj, void *ti, void (*dtor)(void *)) __asm__("__cxa_init_primary_exception");
void *cxa_init_primary(void *obj, void *ti, void (*dtor)(void *)) {
    void **h = (void **)((char *)obj - 128);
    h[0] = ti; h[1] = (void *)dtor; h[2] = (void *)1;
    return obj;
}
char *cxa_demangle(const char *name, char *buf, size_t *len, int *st) __asm__("__cxa_demangle");
char *cxa_demangle(const char *name, char *buf, size_t *len, int *st) {
    char *r = ob_demangle(name);
    size_t n;
    if (!r) { if (st) *st = -2; return 0; }
    n = strlen(r) + 1;
    if (buf && len && *len >= n) { memcpy(buf, r, n); free(r); if (st) *st = 0; if (len) *len = n; return buf; }
    if (len) *len = n;
    if (st) *st = 0;
    return r;
}

static int ti_has_vt(const void *ti, const void *vt) { return ti && *(const void **)ti == vt; }
static int ti_pub_base(const void *ti, const void *dst, long *off, int depth) {
    unsigned n, i;
    const char *b;
    if (!ti || depth > 16) return 0;
    if (ti == dst) return 1;
    if (ti_has_vt(ti, &vt_si_ti[2])) {
        const void *base = ((const void **)ti)[2];
        if (ti_pub_base(base, dst, off, depth + 1)) return 1;
    } else if (ti_has_vt(ti, &vt_vmi_ti[2])) {
        n = ((const unsigned *)ti)[5];
        b = (const char *)ti + 24;
        for (i = 0; i < n && i < 32; i++) {
            const void *base = *(const void **)b;
            long of = *(const long *)(b + 8);
            b += 16;
            if (of & 1) continue;                 /* base virtual: sem o ajuste em tempo de execução */
            if ((of & 2) == 0) continue;          /* não pública */
            if (base == dst || ti_pub_base(base, dst, off, depth + 1)) { *off += of >> 8; return 1; }
        }
    }
    return 0;
}
void *dynamic_cast_fn(const void *src, const void *src_ti, const void *dst_ti, long src2dst) __asm__("__dynamic_cast");
void *dynamic_cast_fn(const void *src, const void *src_ti, const void *dst_ti, long src2dst) {
    void **vt;
    long top, off = 0;
    const void *md;
    (void)src_ti;
    if (!src) return 0;
    if (src2dst >= 0) return (char *)src + src2dst;
    if (src2dst == -2) return 0;
    vt = *(void ***)src;
    if (!vt) return 0;
    top = ((long *)vt)[-2];
    md = ((const void **)vt)[-1];
    if (md == dst_ti) return (char *)src + top;
    if (ti_pub_base(md, dst_ti, &off, 0)) return (char *)src + top + off;
    return 0;
}

/* typeinfo: o nome é o símbolo ELF (um '_' a menos que o do Mach-O). */
const void *ti_rx[2] __asm__("_ZTINSt3__111regex_errorE") = { (void *)&vt_class_ti[2], "NSt3__111regex_errorE" };
const void *ti_bwp[2] __asm__("_ZTINSt3__112bad_weak_ptrE") = { (void *)&vt_class_ti[2], "NSt3__112bad_weak_ptrE" };
const void *ti_fut[2] __asm__("_ZTINSt3__112future_errorE") = { (void *)&vt_class_ti[2], "NSt3__112future_errorE" };
const void *ti_sys[2] __asm__("_ZTINSt3__112system_errorE") = { (void *)&vt_class_ti[2], "NSt3__112system_errorE" };
const void *ti_is[2] __asm__("_ZTINSt3__113basic_istreamIcNS_11char_traitsIcEEEE") = { (void *)&vt_class_ti[2], "NSt3__113basic_istreamIcNS_11char_traitsIcEEEE" };
const void *ti_os[2] __asm__("_ZTINSt3__113basic_ostreamIcNS_11char_traitsIcEEEE") = { (void *)&vt_class_ti[2], "NSt3__113basic_ostreamIcNS_11char_traitsIcEEEE" };
const void *ti_u8[2] __asm__("_ZTINSt3__114__codecvt_utf8IwEE") = { (void *)&vt_class_ti[2], "NSt3__114__codecvt_utf8IwEE" };
const void *ti_iosm[2] __asm__("_ZTINSt3__114basic_iostreamIcNS_11char_traitsIcEEEE") = { (void *)&vt_class_ti[2], "NSt3__114basic_iostreamIcNS_11char_traitsIcEEEE" };
const void *ti_ecat[2] __asm__("_ZTINSt3__114error_categoryE") = { (void *)&vt_class_ti[2], "NSt3__114error_categoryE" };
const void *ti_sb[2] __asm__("_ZTINSt3__115basic_streambufIcNS_11char_traitsIcEEEE") = { (void *)&vt_class_ti[2], "NSt3__115basic_streambufIcNS_11char_traitsIcEEEE" };
const void *ti_sbw[2] __asm__("_ZTINSt3__115basic_streambufIwNS_11char_traitsIwEEEE") = { (void *)&vt_class_ti[2], "NSt3__115basic_streambufIwNS_11char_traitsIwEEEE" };
const void *ti_assoc[2] __asm__("_ZTINSt3__117__assoc_sub_stateE") = { (void *)&vt_class_ti[2], "NSt3__117__assoc_sub_stateE" };
const void *ti_swc[2] __asm__("_ZTINSt3__119__shared_weak_countE") = { (void *)&vt_class_ti[2], "NSt3__119__shared_weak_countE" };
const void *ti_fserr[2] __asm__("_ZTINSt3__14__fs10filesystem16filesystem_errorE") = { (void *)&vt_class_ti[2], "NSt3__14__fs10filesystem16filesystem_errorE" };
const void *ti_cvt[2] __asm__("_ZTINSt3__17codecvtIwc11__mbstate_tEE") = { (void *)&vt_class_ti[2], "NSt3__17codecvtIwc11__mbstate_tEE" };
const void *ti_logic[2] __asm__("_ZTISt11logic_error") = { (void *)&vt_class_ti[2], "St11logic_error" };
const void *ti_len[2] __asm__("_ZTISt12length_error") = { (void *)&vt_class_ti[2], "St12length_error" };
const void *ti_oor[2] __asm__("_ZTISt12out_of_range") = { (void *)&vt_class_ti[2], "St12out_of_range" };
const void *ti_bex[2] __asm__("_ZTISt13bad_exception") = { (void *)&vt_class_ti[2], "St13bad_exception" };
const void *ti_rt[2] __asm__("_ZTISt13runtime_error") = { (void *)&vt_class_ti[2], "St13runtime_error" };
const void *ti_bvar[2] __asm__("_ZTISt18bad_variant_access") = { (void *)&vt_class_ti[2], "St18bad_variant_access" };
const void *ti_bopt[2] __asm__("_ZTISt19bad_optional_access") = { (void *)&vt_class_ti[2], "St19bad_optional_access" };
const void *ti_ban[2] __asm__("_ZTISt20bad_array_new_length") = { (void *)&vt_class_ti[2], "St20bad_array_new_length" };
const void *ti_bc[2] __asm__("_ZTISt8bad_cast") = { (void *)&vt_class_ti[2], "St8bad_cast" };
const void *ti_ba[2] __asm__("_ZTISt9bad_alloc") = { (void *)&vt_class_ti[2], "St9bad_alloc" };
const void *ti_ex[2] __asm__("_ZTISt9exception") = { (void *)&vt_class_ti[2], "St9exception" };
const void *ti_c[2] __asm__("_ZTIc") = { (void *)&vt_class_ti[2], "c" };
const void *ti_h[2] __asm__("_ZTIh") = { (void *)&vt_class_ti[2], "h" };
const void *ti_i[2] __asm__("_ZTIi") = { (void *)&vt_class_ti[2], "i" };
const void *ti_v[2] __asm__("_ZTIv") = { (void *)&vt_class_ti[2], "v" };
static int ti_nop(const void *t) { (void)t; return 0; }
const void *vt_enum_ti[8] __asm__("_ZTVN10__cxxabiv116__enum_type_infoE") = {0, 0, (void *)noop_d, (void *)noop_d, (void *)ti_nop, (void *)ti_nop, (void *)ti_nop, (void *)ti_nop};
const void *vt_ptr_ti[8] __asm__("_ZTVN10__cxxabiv119__pointer_type_infoE") = {0, 0, (void *)noop_d, (void *)noop_d, (void *)ti_nop, (void *)ti_nop, (void *)ti_nop, (void *)ti_nop};
const void *vt_fn_ti[8] __asm__("_ZTVN10__cxxabiv120__function_type_infoE") = {0, 0, (void *)noop_d, (void *)noop_d, (void *)ti_nop, (void *)ti_nop, (void *)ti_nop, (void *)ti_nop};

/* ======================== locale, codecvt, facets ======================== */
typedef struct { long once; int id; int pad; } ob_locid;
ob_locid id_ctype_c __asm__("_ZNSt3__15ctypeIcE2idE");
ob_locid id_ctype_w __asm__("_ZNSt3__15ctypeIwE2idE");
ob_locid id_cvt_cc __asm__("_ZNSt3__17codecvtIcc11__mbstate_tE2idE");
ob_locid id_cvt_wc __asm__("_ZNSt3__17codecvtIwc11__mbstate_tE2idE");
ob_locid id_col_c __asm__("_ZNSt3__17collateIcE2idE");
ob_locid id_col_w __asm__("_ZNSt3__17collateIwE2idE");
ob_locid id_tput __asm__("_ZNSt3__18time_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEE2idE");
static int g_idnext = 10;
int locid_get(ob_locid *id) __asm__("_ZNSt3__16locale2id5__getEv");
int locid_get(ob_locid *id) {
    int v = id->id;
    if (!v) { v = __sync_add_and_fetch(&g_idnext, 1); id->id = v; }
    return v;
}
static void facet_zero(void *f) { (void)f; }
void facet_on_zero(void *f) __asm__("_ZNSt3__16locale5facet16__on_zero_sharedEv");
void facet_on_zero(void *f) { (void)f; }
static const void *vt_facet_min[5] = {0, 0, (void *)noop_d, (void *)noop_d, (void *)facet_zero};
static struct { void *vt; long refs; } facet_ctype_c = { (void *)&vt_facet_min[2], 0x3fffffff };
static struct { void *vt; long refs; } facet_ctype_w = { (void *)&vt_facet_min[2], 0x3fffffff };
static struct { void *vt; long refs; } facet_col_c = { (void *)&vt_facet_min[2], 0x3fffffff };
static struct { void *vt; long refs; } facet_col_w = { (void *)&vt_facet_min[2], 0x3fffffff };
static struct { void *vt; long refs; } facet_tput = { (void *)&vt_facet_min[2], 0x3fffffff };

static int utf8_decode(const unsigned char **p, const unsigned char *e, uint32_t *out) {
    const unsigned char *s = *p;
    unsigned c;
    int need, i;
    if (s >= e) return 1;
    c = *s;
    if (c < 0x80) { *out = c; *p = s + 1; return 0; }
    if ((c & 0xe0) == 0xc0) { need = 2; c &= 0x1f; }
    else if ((c & 0xf0) == 0xe0) { need = 3; c &= 0x0f; }
    else if ((c & 0xf8) == 0xf0) { need = 4; c &= 0x07; }
    else return 2;
    if (s + need > e) return 1;
    for (i = 1; i < need; i++) {
        if ((s[i] & 0xc0) != 0x80) return 2;
        c = (c << 6) | (s[i] & 0x3f);
    }
    *out = c; *p = s + need; return 0;
}
static int utf8_encode(uint32_t c, char **p, char *e) {
    char *s = *p;
    if (c < 0x80) { if (s >= e) return 1; *s++ = (char)c; }
    else if (c < 0x800) { if (s + 2 > e) return 1; *s++ = (char)(0xc0 | (c >> 6)); *s++ = (char)(0x80 | (c & 0x3f)); }
    else if (c < 0x10000) { if (s + 3 > e) return 1; *s++ = (char)(0xe0 | (c >> 12)); *s++ = (char)(0x80 | ((c >> 6) & 0x3f)); *s++ = (char)(0x80 | (c & 0x3f)); }
    else { if (s + 4 > e) return 1; *s++ = (char)(0xf0 | (c >> 18)); *s++ = (char)(0x80 | ((c >> 12) & 0x3f)); *s++ = (char)(0x80 | ((c >> 6) & 0x3f)); *s++ = (char)(0x80 | (c & 0x3f)); }
    *p = s; return 0;
}
int u8_out(const void *self, void *st, const uint32_t *fb, const uint32_t *fe, const uint32_t **fn, char *tb, char *te, char **tn) __asm__("_ZNKSt3__114__codecvt_utf8IwE6do_outER11__mbstate_tPKwS5_RS5_PcS7_RS7_");
int u8_out(const void *self, void *st, const uint32_t *fb, const uint32_t *fe, const uint32_t **fn, char *tb, char *te, char **tn) {
    (void)self; (void)st;
    while (fb != fe) { if (utf8_encode(*fb, &tb, te)) break; fb++; }
    *fn = fb; *tn = tb; return fb == fe ? 0 : 1;
}
int u8_in(const void *self, void *st, const char *fb, const char *fe, const char **fn, uint32_t *tb, uint32_t *te, uint32_t **tn) __asm__("_ZNKSt3__114__codecvt_utf8IwE5do_inER11__mbstate_tPKcS5_RS5_PwS7_RS7_");
int u8_in(const void *self, void *st, const char *fb, const char *fe, const char **fn, uint32_t *tb, uint32_t *te, uint32_t **tn) {
    const unsigned char *p = (const unsigned char *)fb;
    int rc = 0;
    (void)self; (void)st;
    while (p != (const unsigned char *)fe && tb != te) {
        uint32_t c; int k = utf8_decode(&p, (const unsigned char *)fe, &c);
        if (k == 1) { rc = 1; break; }
        if (k == 2) { rc = 2; break; }
        *tb++ = c;
    }
    *fn = (const char *)p; *tn = tb; return rc;
}
int u8_unshift(const void *self, void *st, char *b, char *e, char **n) __asm__("_ZNKSt3__114__codecvt_utf8IwE10do_unshiftER11__mbstate_tPcS4_RS4_");
int u8_unshift(const void *self, void *st, char *b, char *e, char **n) { (void)self; (void)st; (void)e; *n = b; return 0; }
int u8_enc(const void *self) __asm__("_ZNKSt3__114__codecvt_utf8IwE11do_encodingEv");
int u8_enc(const void *self) { (void)self; return 0; }
int u8_max(const void *self) __asm__("_ZNKSt3__114__codecvt_utf8IwE13do_max_lengthEv");
int u8_max(const void *self) { (void)self; return 4; }
int u8_noconv(const void *self) __asm__("_ZNKSt3__114__codecvt_utf8IwE16do_always_noconvEv");
int u8_noconv(const void *self) { (void)self; return 0; }
int u8_len(const void *self, void *st, const char *b, const char *e, unsigned long mx) __asm__("_ZNKSt3__114__codecvt_utf8IwE9do_lengthER11__mbstate_tPKcS5_m");
int u8_len(const void *self, void *st, const char *b, const char *e, unsigned long mx) {
    const unsigned char *p = (const unsigned char *)b;
    unsigned long n = 0;
    (void)self; (void)st;
    while (n < mx && p != (const unsigned char *)e) {
        uint32_t c; if (utf8_decode(&p, (const unsigned char *)e, &c)) break; n++;
    }
    return (int)n;
}
static const void *vt_cvt_wc[12] = {0, (void *)&ti_cvt, (void *)noop_d, (void *)noop_d, (void *)facet_zero,
    (void *)u8_out, (void *)u8_in, (void *)u8_unshift, (void *)u8_enc, (void *)u8_noconv, (void *)u8_len, (void *)u8_max};
static struct { void *vt; long refs; } facet_cvt = { (void *)&vt_cvt_wc[2], 0x3fffffff };
void cvt_ctor(void *f, unsigned long refs) __asm__("_ZNSt3__17codecvtIwc11__mbstate_tEC2Em");
void cvt_ctor(void *f, unsigned long refs) { ((void **)f)[0] = (void *)&vt_cvt_wc[2]; ((long *)f)[1] = (long)refs; }
void cvt_dtor(void *f) __asm__("_ZNSt3__17codecvtIwc11__mbstate_tED2Ev");
void cvt_dtor(void *f) { (void)f; }
const void *cx_locale_facet(void *id) {
    if (id == &id_ctype_c) return &facet_ctype_c;
    if (id == &id_ctype_w) return &facet_ctype_w;
    if (id == &id_cvt_wc || id == &id_cvt_cc) return &facet_cvt;
    if (id == &id_col_c) return &facet_col_c;
    if (id == &id_col_w) return &facet_col_w;
    if (id == &id_tput) return &facet_tput;
    if (id == (void *)numpunct_char_id) return 0;
    return 0;
}
void loc_name(ob_str *ret, const void *l) __asm__("_ZNKSt3__16locale4nameEv");
void loc_name(ob_str *ret, const void *l) { (void)l; ob_sempty(ret); ob_sassign_n(ret, "C", 1); }
int loc_has(const void *l, void *id) __asm__("_ZNKSt3__16locale9has_facetERNS0_2idE");
int loc_has(const void *l, void *id) {
    (void)l;
    return id == (void *)numpunct_char_id || cx_locale_facet(id) != 0;
}
void loc_install(void **self, void *const *base, void *facet, long refs) __asm__("_ZNSt3__16locale14__install_ctorERKS0_PNS0_5facetEl");
void loc_install(void **self, void *const *base, void *facet, long refs) {
    (void)refs;
    *self = base ? *base : cx_locale_classic_imp();
    if (facet) ((long *)facet)[1] += 1;          /* segura o facet; o locale clássico continua o padrão */
}
void loc_global(void **ret, void *const *loc) __asm__("_ZNSt3__16locale6globalERKS0_");
void loc_global(void **ret, void *const *loc) { *ret = cx_locale_classic_imp(); (void)loc; }
void loc_classic(void **ret) __asm__("_ZNSt3__16locale7classicEv");
void loc_classic(void **ret) { *ret = cx_locale_classic_imp(); }
void loc_byname(void **self, const char *n) __asm__("_ZNSt3__16localeC1EPKc");
void loc_byname(void **self, const char *n) {
    *self = cx_locale_classic_imp();
    if (n && n[0] && strcmp(n, "C") && strcmp(n, "POSIX")) ob_fail("std::runtime_error: locale");
}
void **loc_assign(void **d, void *const *s) __asm__("_ZNSt3__16localeaSERKS0_");
void **loc_assign(void **d, void *const *s) { *d = *s; return d; }
void get_classname(ob_str *ret, const char *n, int u) __asm__("_ZNSt3__115__get_classnameEPKcb");
void get_classname(ob_str *ret, const char *n, int u) { (void)u; ob_sempty(ret); ob_sassign_n(ret, n && n[0] ? n : "*", n && n[0] ? strlen(n) : 1); }
void get_collname(ob_str *ret, const char *n) __asm__("_ZNSt3__120__get_collation_nameEPKc");
void get_collname(ob_str *ret, const char *n) { ob_sempty(ret); ob_sassign_n(ret, n && n[0] ? n : "C", n && n[0] ? strlen(n) : 1); }

#include "libcxx_openbot_io.inc"
