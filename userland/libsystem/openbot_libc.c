/*
 * Nomes pequenos do libSystem que o onnxruntime e o cua-driver importam.
 * vsnprintf_l ignora o locale: setlocale neste sistema é sempre "C".
 * nftw anda com opendir/lstat. FTW_PHYS não segue link. O limite de fd não abre
 * descritor extra (o readdir daqui não usa um fd por pasta).
 */
#include "sys.h"

void *malloc(size_t n);
void free(void *p);
size_t strlen(const char *s);
char *strrchr(const char *s, int c);
typedef __builtin_va_list va_list;
int vsnprintf(char *buf, size_t n, const char *fmt, va_list ap);


int vsnprintf_l(char *buf, size_t n, void *loc, const char *fmt, va_list ap) {
    (void)loc;
    return vsnprintf(buf, n, fmt, ap);
}

static double mul_fix(double a, double b, double c, double d, double *imag) {
    double ac = a * c, bd = b * d, ad = a * d, bc = b * c;
    double real = ac - bd;
    *imag = ad + bc;
    if (!__builtin_isnan(real) || !__builtin_isnan(*imag)) return real;
    int recalc = 0;
    double a2 = a, b2 = b, c2 = c, d2 = d;
    if (__builtin_isinf(a) || __builtin_isinf(b)) {
        a2 = __builtin_copysign(__builtin_isinf(a) ? 1.0 : 0.0, a);
        b2 = __builtin_copysign(__builtin_isinf(b) ? 1.0 : 0.0, b);
        if (__builtin_isnan(c)) c2 = __builtin_copysign(0.0, c);
        if (__builtin_isnan(d)) d2 = __builtin_copysign(0.0, d);
        recalc = 1;
    }
    if (__builtin_isinf(c) || __builtin_isinf(d)) {
        c2 = __builtin_copysign(__builtin_isinf(c) ? 1.0 : 0.0, c);
        d2 = __builtin_copysign(__builtin_isinf(d) ? 1.0 : 0.0, d);
        if (__builtin_isnan(a)) a2 = __builtin_copysign(0.0, a);
        if (__builtin_isnan(b)) b2 = __builtin_copysign(0.0, b);
        recalc = 1;
    }
    if (!recalc && (__builtin_isinf(ac) || __builtin_isinf(bd) || __builtin_isinf(ad) || __builtin_isinf(bc))) {
        if (__builtin_isnan(a)) a2 = __builtin_copysign(0.0, a);
        if (__builtin_isnan(b)) b2 = __builtin_copysign(0.0, b);
        if (__builtin_isnan(c)) c2 = __builtin_copysign(0.0, c);
        if (__builtin_isnan(d)) d2 = __builtin_copysign(0.0, d);
        recalc = 1;
    }
    if (recalc) {
        real = __builtin_huge_val() * (a2 * c2 - b2 * d2);
        *imag = __builtin_huge_val() * (a2 * d2 + b2 * c2);
    }
    return real;
}

double _Complex __muldc3(double a, double b, double c, double d) {
    double imag, real = mul_fix(a, b, c, d, &imag);
    double _Complex z;
    __real__ z = real;
    __imag__ z = imag;
    return z;
}
float _Complex __mulsc3(float a, float b, float c, float d) {
    double imag, real = mul_fix((double)a, (double)b, (double)c, (double)d, &imag);
    float _Complex z;
    __real__ z = (float)real;
    __imag__ z = (float)imag;
    return z;
}

/* ---- divisão complexa (compiler-rt): (a+bi)/(c+di), com o algoritmo de Smith para evitar overflow/underflow */
double fabs(double);
double _Complex __divdc3(double a, double b, double c, double d) {
    double _Complex z;
    if (fabs(c) >= fabs(d)) {
        double r = d / c, den = c + d * r;
        __real__ z = (a + b * r) / den;
        __imag__ z = (b - a * r) / den;
    } else {
        double r = c / d, den = c * r + d;
        __real__ z = (a * r + b) / den;
        __imag__ z = (b * r - a) / den;
    }
    return z;
}
float _Complex __divsc3(float a, float b, float c, float d) {
    double _Complex q = __divdc3(a, b, c, d);
    float _Complex z;
    __real__ z = (float)__real__ q;
    __imag__ z = (float)__imag__ q;
    return z;
}
long double _Complex __divxc3(long double a, long double b, long double c, long double d) {
    long double _Complex z;
    long double C = c < 0 ? -c : c, D = d < 0 ? -d : d;
    if (C >= D) {
        long double r = d / c, den = c + d * r;
        __real__ z = (a + b * r) / den;
        __imag__ z = (b - a * r) / den;
    } else {
        long double r = c / d, den = c * r + d;
        __real__ z = (a * r + b) / den;
        __imag__ z = (b * r - a) / den;
    }
    return z;
}
long double _Complex __mulxc3(long double a, long double b, long double c, long double d) {
    long double _Complex z;
    __real__ z = a * c - b * d;
    __imag__ z = a * d + b * c;
    return z;
}

/* ---- raiz quadrada complexa (libm). csqrt(z) = sqrt((|z|+Re)/2) + i·sign(Im)·sqrt((|z|-Re)/2) */
double sqrt(double), hypot(double, double), copysign(double, double);
double _Complex csqrt(double _Complex z) {
    double x = __real__ z, y = __imag__ z;
    double _Complex r;
    if (y == 0) {
        if (x >= 0) { __real__ r = sqrt(x); __imag__ r = copysign(0.0, y); }
        else { __real__ r = 0; __imag__ r = copysign(sqrt(-x), y); }
        return r;
    }
    double m = hypot(x, y);
    double re = sqrt((m + x) * 0.5);
    double im = sqrt((m - x) * 0.5);
    __real__ r = re;
    __imag__ r = copysign(im, y);
    return r;
}
float _Complex csqrtf(float _Complex z) {
    double _Complex d;
    __real__ d = (double)__real__ z;
    __imag__ d = (double)__imag__ z;
    d = csqrt(d);
    float _Complex o;
    __real__ o = (float)__real__ d;
    __imag__ o = (float)__imag__ d;
    return o;
}
long double _Complex csqrtl(long double _Complex z) {
    double _Complex r;
    __real__ r = (double)__real__ z;
    __imag__ r = (double)__imag__ z;
    r = csqrt(r);
    long double _Complex o;
    __real__ o = (long double)__real__ r;
    __imag__ o = (long double)__imag__ r;
    return o;
}

typedef struct {
    u32 st_dev; u16 st_mode, st_nlink; u64 st_ino; u32 st_uid, st_gid, st_rdev;
    i64 atime, atime_ns, mtime, mtime_ns, ctime, ctime_ns, btime, btime_ns;
    i64 st_size, st_blocks; i32 st_blksize; u32 st_flags, st_gen; i32 lspare; i64 qspare[2];
} mac_stat_t;
typedef struct {
    u64 d_ino, d_seekoff; u16 d_reclen, d_namlen; u8 d_type; char d_name[1024];
} mac_dirent_t;
int lstat(const char *path, mac_stat_t *m);
int stat(const char *path, mac_stat_t *m);
void *opendir(const char *path);
mac_dirent_t *readdir(void *d);
int closedir(void *d);

struct FTW { int base; int level; };
#define FTW_F 0
#define FTW_D 1
#define FTW_DNR 2
#define FTW_DP 3
#define FTW_NS 4
#define FTW_SL 5
#define FTW_PHYS 1
#define FTW_DEPTH 8

static void memcpy_path(char *d, const char *s, size_t n) { for (size_t i = 0; i < n; i++) d[i] = s[i]; }
static int path_base(const char *path) {
    const char *s = strrchr(path, '/');
    if (!s || s == path) return 0;
    return (int)(s - path) + 1;
}
static int walk(const char *path, int (*fn)(const char *, const mac_stat_t *, int, struct FTW *), int flags, int level) {
    mac_stat_t st;
    struct FTW info = {path_base(path), level};
    if (lstat(path, &st) != 0) return fn(path, &st, FTW_NS, &info);
    unsigned mode = st.st_mode & 0170000;
    int link = mode == 0120000;
    int dir = mode == 0040000;
    if (link && !(flags & FTW_PHYS)) {
        mac_stat_t followed;
        if (stat(path, &followed) == 0) {
            st = followed;
            link = 0;
            dir = (st.st_mode & 0170000) == 0040000;
        }
    }
    int kind = link ? FTW_SL : dir ? FTW_D : FTW_F;
    if (!((flags & FTW_DEPTH) && dir && !link)) {
        int rc = fn(path, &st, kind, &info);
        if (rc) return rc;
    }
    if (dir && !link) {
        void *dp = opendir(path);
        if (!dp) return fn(path, &st, FTW_DNR, &info);
        mac_dirent_t *ent;
        while ((ent = readdir(dp))) {
            char child[1280];
            size_t lp = strlen(path), ln = strlen(ent->d_name);
            if (lp + ln + 2 >= sizeof child) continue;
            memcpy_path(child, path, lp);
            if (lp && path[lp - 1] != '/') child[lp++] = '/';
            for (size_t i = 0; i < ln; i++) child[lp++] = ent->d_name[i];
            child[lp] = 0;
            int rc = walk(child, fn, flags, level + 1);
            if (rc) { closedir(dp); return rc; }
        }
        closedir(dp);
        if (flags & FTW_DEPTH) {
            int rc = fn(path, &st, FTW_DP, &info);
            if (rc) return rc;
        }
    }
    return 0;
}

int nftw(const char *path, int (*fn)(const char *, const mac_stat_t *, int, struct FTW *), int nopen, int flags) {
    (void)nopen;
    if (!path || !fn) return -1;
    return walk(path, fn, flags, 0);
}
