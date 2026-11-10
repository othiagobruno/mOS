/*
 * mOS libSystem — o que faltava para o bash 5.3, o readline 8.3, o ncurses 6.6 e o libintl do
 * Homebrew (docs/porte-brew.md): regex POSIX, fnmatch, tsearch, caracteres largos, locale "C.UTF-8",
 * usuários/grupos, terminal e temporizadores. Só existe UTF-8. O nome do locale segue cx_locale_id
 * (en_US ou pt_BR); a classificação de caracteres continua a do C.
 */
#include "sys.h"
extern int cx_errno;

size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
char *strchr(const char *s, int c);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
void *malloc(size_t n);
void *calloc(size_t n, size_t m);
void free(void *p);
int isatty(int fd);
int getpid(void);
int access(const char *p, int m);
const char *cx_locale_id(void);
double frexp(double x, int *e);
double ldexp(double x, int e);
int tolower(int c);

typedef int wchar_t_;
size_t mbrtowc(wchar_t_ *pwc, const char *s, size_t n, void *ps);
size_t wcrtomb(char *s, wchar_t_ wc, void *ps);

/* ------------------------------------------------------------ caracteres largos */
size_t wcslen(const wchar_t_ *s);
int wcscmp(const wchar_t_ *a, const wchar_t_ *b) {
    while (*a && *a == *b) a++, b++;
    return *a < *b ? -1 : *a > *b;
}
int wcsncmp(const wchar_t_ *a, const wchar_t_ *b, size_t n) {
    for (; n; n--, a++, b++) { if (*a != *b) return *a < *b ? -1 : 1; if (!*a) return 0; }
    return 0;
}
int wcscoll(const wchar_t_ *a, const wchar_t_ *b) { return wcscmp(a, b); }
wchar_t_ *wcsdup(const wchar_t_ *s) {
    size_t n = 0;
    while (s[n]) n++;
    wchar_t_ *d = malloc((n + 1) * sizeof *d);
    if (d) memcpy(d, s, (n + 1) * sizeof *d);
    return d;
}
wchar_t_ *wcscat(wchar_t_ *d, const wchar_t_ *s) { wchar_t_ *o = d; while (*d) d++; while ((*d++ = *s++)) {} return o; }
wchar_t_ *wmemcpy(wchar_t_ *d, const wchar_t_ *s, size_t n) { return memcpy(d, s, n * sizeof *d); }
wchar_t_ *wmemset(wchar_t_ *d, wchar_t_ c, size_t n) { for (size_t i = 0; i < n; i++) d[i] = c; return d; }
wchar_t_ btowc(int c) { return c >= 0 && c < 0x80 ? c : -1; }   /* WEOF = -1 */
int wctob(wchar_t_ c) { return c >= 0 && c < 0x80 ? c : -1; }

/* largura na tela: 0 para combinantes e sem largura, 2 para CJK/emoji, -1 para controle */
int wcwidth(wchar_t_ c) {
    if (c == 0) return 0;
    if (c < 32 || (c >= 0x7F && c < 0xA0)) return -1;
    if ((c >= 0x300 && c <= 0x36F) || (c >= 0x200B && c <= 0x200F) || (c >= 0xFE00 && c <= 0xFE0F) ||
        (c >= 0x1AB0 && c <= 0x1AFF) || (c >= 0x20D0 && c <= 0x20FF)) return 0;
    if ((c >= 0x1100 && c <= 0x115F) || (c >= 0x2E80 && c <= 0xA4CF) || (c >= 0xAC00 && c <= 0xD7A3) ||
        (c >= 0xF900 && c <= 0xFAFF) || (c >= 0xFE30 && c <= 0xFE4F) || (c >= 0xFF00 && c <= 0xFF60) ||
        (c >= 0xFFE0 && c <= 0xFFE6) || (c >= 0x1F300 && c <= 0x1FAFF) || (c >= 0x20000 && c <= 0x3FFFD)) return 2;
    return 1;
}
int wcswidth(const wchar_t_ *s, size_t n) {
    int w = 0;
    for (; n && *s; n--, s++) { int k = wcwidth(*s); if (k < 0) return -1; w += k; }
    return w;
}

size_t mbsnrtowcs(wchar_t_ *dst, const char **src, size_t nms, size_t len, void *ps) {
    const char *s = *src;
    size_t n = 0;
    while (!dst || n < len) {
        wchar_t_ wc;
        size_t k = mbrtowc(&wc, s, nms, ps);
        if (k == (size_t)-1) { *src = s; return (size_t)-1; }
        if (k == (size_t)-2) break;                       /* sequência cortada pelo limite nms */
        if (dst) dst[n] = wc;
        if (k == 0) { if (dst) *src = 0; return n; }
        s += k; nms -= k; n++;
    }
    if (dst) *src = s;
    return n;
}
size_t mbsrtowcs(wchar_t_ *dst, const char **src, size_t len, void *ps) { return mbsnrtowcs(dst, src, (size_t)-1, len, ps); }
size_t mbstowcs(wchar_t_ *dst, const char *src, size_t len) { const char *s = src; return mbsrtowcs(dst, &s, len, 0); }
size_t wcsnrtombs(char *dst, const wchar_t_ **src, size_t nwc, size_t len, void *ps) {
    const wchar_t_ *s = *src;
    size_t n = 0;
    char b[8];
    for (; nwc; nwc--, s++) {
        size_t k = wcrtomb(b, *s, ps);
        if (k == (size_t)-1) { if (dst) *src = s; return (size_t)-1; }
        if (dst && n + k > len) break;
        if (dst) memcpy(dst + n, b, k);
        if (!*s) { if (dst) *src = 0; return n; }
        n += k;
    }
    if (dst) *src = s;
    return n;
}
size_t wcsrtombs(char *dst, const wchar_t_ **src, size_t len, void *ps) { return wcsnrtombs(dst, src, (size_t)-1, len, ps); }
size_t wcstombs(char *dst, const wchar_t_ *src, size_t len) { const wchar_t_ *s = src; return wcsrtombs(dst, &s, len, 0); }

/* classes de wctype: só ASCII é classificado; acima de 0x7F letras contam como alpha/print */
enum { WC_ALNUM = 1, WC_ALPHA, WC_BLANK, WC_CNTRL, WC_DIGIT, WC_GRAPH, WC_LOWER, WC_PRINT, WC_PUNCT, WC_SPACE, WC_UPPER, WC_XDIGIT };
unsigned long wctype(const char *name) {
    static const char *const n[] = {"alnum", "alpha", "blank", "cntrl", "digit", "graph", "lower", "print", "punct", "space", "upper", "xdigit"};
    for (int i = 0; i < 12; i++) if (!strcmp(name, n[i])) return (unsigned long)(i + 1);
    return 0;
}
int iswctype(wchar_t_ c, unsigned long t) {
    if (c < 0) return 0;
    if (c >= 0x80) return t == WC_ALPHA || t == WC_ALNUM || t == WC_PRINT || t == WC_GRAPH;
    int up = c >= 'A' && c <= 'Z', lo = c >= 'a' && c <= 'z', dg = c >= '0' && c <= '9';
    int sp = c == ' ' || (c >= 9 && c <= 13), ct = c < 32 || c == 127;
    switch (t) {
    case WC_ALNUM: return up || lo || dg;
    case WC_ALPHA: return up || lo;
    case WC_BLANK: return c == ' ' || c == '\t';
    case WC_CNTRL: return ct;
    case WC_DIGIT: return dg;
    case WC_GRAPH: return c > 32 && c < 127;
    case WC_LOWER: return lo;
    case WC_PRINT: return c >= 32 && c < 127;
    case WC_PUNCT: return c > 32 && c < 127 && !up && !lo && !dg;
    case WC_SPACE: return sp;
    case WC_UPPER: return up;
    case WC_XDIGIT: return dg || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }
    return 0;
}

/* ------------------------------------------------------------ locale (só "C" com UTF-8) */
typedef struct {
    char *decimal_point, *thousands_sep, *grouping, *int_curr_symbol, *currency_symbol, *mon_decimal_point,
         *mon_thousands_sep, *mon_grouping, *positive_sign, *negative_sign;
    char int_frac_digits, frac_digits, p_cs_precedes, p_sep_by_space, n_cs_precedes, n_sep_by_space,
         p_sign_posn, n_sign_posn, int_p_cs_precedes, int_n_cs_precedes, int_p_sep_by_space,
         int_n_sep_by_space, int_p_sign_posn, int_n_sign_posn;
} mac_lconv_t;
mac_lconv_t *localeconv(void) {
    static mac_lconv_t c = {".", "", "", "", "", "", "", "", "", "",
                            127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127};
    static mac_lconv_t en = {".", ",", "\3", "", "$", ".", ",", "\3", "", "-",
                             2, 2, 1, 0, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1};
    static mac_lconv_t pt = {",", ".", "\3", "", "R$", ",", ".", "\3", "", "-",
                             2, 2, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1};
    const char *id = cx_locale_id();
    if (id[0] == 'e') return &en;
    if (id[0] == 'p') return &pt;
    return &c;
}
static int cx_locale_obj;
void *newlocale(int mask, const char *name, void *base) { (void)mask; (void)name; (void)base; return &cx_locale_obj; }
void freelocale(void *l) { (void)l; }
void *uselocale(void *l) { (void)l; return (void *)-1; }               /* LC_GLOBAL_LOCALE */
const char *querylocale(int mask, void *l) { (void)mask; (void)l; return cx_locale_id(); }
/* nl_item do macOS: CODESET 0, D_T_FMT 1, D_FMT 2, T_FMT 3, RADIXCHAR 50, THOUSEP 51, YESEXPR 52, NOEXPR 53 */
char *nl_langinfo(int item) {
    switch (item) {
    case 0: return "UTF-8";
    case 1: return "%a %b %e %H:%M:%S %Y";
    case 2: return "%m/%d/%y";
    case 3: return "%H:%M:%S";
    case 50: return cx_locale_id()[0] == 'p' ? "," : ".";
    case 51: return cx_locale_id()[0] == 'e' ? "," : cx_locale_id()[0] == 'p' ? "." : "";
    case 52: return "^[yY]";
    case 53: return "^[nN]";
    }
    return "";
}

/* ------------------------------------------------------------ matemática */
long double frexpl(long double x, int *e) { return (long double)frexp((double)x, e); }
long double ldexpl(long double x, int e) { return (long double)ldexp((double)x, e); }
typedef struct { long long quot, rem; } cx_imaxdiv_t;
cx_imaxdiv_t imaxdiv(long long a, long long b) { cx_imaxdiv_t r = {a / b, a % b}; return r; }

/* ------------------------------------------------------------ usuários, grupos, serviços */
typedef struct {
    char *pw_name, *pw_passwd;
    unsigned pw_uid, pw_gid;
    long pw_change;
    char *pw_class, *pw_gecos, *pw_dir, *pw_shell;
    long pw_expire;
} mac_passwd_t;
mac_passwd_t *getpwuid(unsigned uid);
static int pw_iter;
void setpwent(void) { pw_iter = 0; }
void endpwent(void) { pw_iter = 0; }
mac_passwd_t *getpwent(void) { return pw_iter++ ? 0 : getpwuid(501); }
typedef struct { char *gr_name, *gr_passwd; unsigned gr_gid; char **gr_mem; } mac_group_t;
static int gr_iter;
void setgrent(void) { gr_iter = 0; }
void endgrent(void) { gr_iter = 0; }
mac_group_t *getgrent(void) {
    static char *mem[] = {0};
    static mac_group_t g = {"staff", "*", 20, mem};
    return gr_iter++ ? 0 : &g;
}
int getgroups(int n, unsigned *list) {
    if (n == 0) return 1;
    if (n < 1) return -1;
    list[0] = 20;
    return 1;
}
void setservent(int stay) { (void)stay; }
void endservent(void) {}
void *getservent(void) { return 0; }

/* ------------------------------------------------------------ processos, terminal e tempo */
/* getpgrp, tcsetpgrp, ttyname e cfgetospeed: em pty.c (SYS_TTY) */
int getdtablesize(void) { return 256; }                /* PROC_MAX_FDS do kernel (proc.h) */
/* alarm/setitimer(ITIMER_REAL): o kernel não tem temporizadores por processo; uma thread auxiliar dorme e
 * manda SIGALRM ao próprio processo. Cada armação nova aumenta a geração; a thread antiga vê e desiste. */
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
int pthread_detach(unsigned long t);
int usleep(unsigned us);
int kill(int pid, int s);
static volatile unsigned long itimer_gen;
static volatile long long itimer_deadline_ms, itimer_interval_ms;
static long long now_ms(void) { return (long long)cx_syscall(SYS_TIME, 0, 0, 0, 0, 0); }
static void *itimer_thread(void *arg) {
    unsigned long gen = (unsigned long)arg;
    {   /* a auxiliar bloqueia tudo: o SIGALRM tem de cair numa thread do programa, não nesta */
        int pthread_sigmask(int how, const void *s, void *o);
        unsigned all = 0xFFFFFFFFu;
        pthread_sigmask(1, &all, 0);
    }
    for (;;) {
        long long left = itimer_deadline_ms - now_ms();
        if (itimer_gen != gen) return 0;
        if (left > 0) { usleep((unsigned)(left > 50 ? 50 : left) * 1000); continue; }   /* acorda a cada 50 ms para ver a geração */
        if (itimer_gen != gen) return 0;
        kill(getpid(), 14);                                  /* SIGALRM */
        if (itimer_interval_ms <= 0) { if (itimer_gen == gen) itimer_deadline_ms = 0; return 0; }
        itimer_deadline_ms += itimer_interval_ms;
    }
}
static long long itimer_arm(long long first_ms, long long interval_ms) {
    long long old = itimer_deadline_ms ? itimer_deadline_ms - now_ms() : 0;
    unsigned long gen = ++itimer_gen;
    itimer_interval_ms = interval_ms;
    itimer_deadline_ms = first_ms > 0 ? now_ms() + first_ms : 0;
    if (first_ms > 0) {
        unsigned long th;
        if (pthread_create(&th, 0, itimer_thread, (void *)gen) == 0) pthread_detach(th);
    }
    return old > 0 ? old : 0;
}
unsigned alarm(unsigned s) {
    long long old = itimer_arm((long long)s * 1000, 0);
    return (unsigned)((old + 999) / 1000);
}
typedef struct { long tv_sec; int tv_usec; } cx_itv_tv_t;
typedef struct { cx_itv_tv_t it_interval, it_value; } cx_itimerval_t;
int setitimer(int which, const cx_itimerval_t *nv, cx_itimerval_t *ov) {
    if (which != 0) { if (ov) memset(ov, 0, sizeof *ov); return 0; }   /* só ITIMER_REAL; VIRTUAL/PROF não disparam */
    long long left = itimer_deadline_ms ? itimer_deadline_ms - now_ms() : 0;
    if (ov) {
        memset(ov, 0, sizeof *ov);
        if (left > 0) { ov->it_value.tv_sec = left / 1000; ov->it_value.tv_usec = (int)(left % 1000) * 1000; }
        ov->it_interval.tv_sec = itimer_interval_ms / 1000; ov->it_interval.tv_usec = (int)(itimer_interval_ms % 1000) * 1000;
    }
    if (nv) {
        long long first = (long long)nv->it_value.tv_sec * 1000 + nv->it_value.tv_usec / 1000;
        long long iv = (long long)nv->it_interval.tv_sec * 1000 + nv->it_interval.tv_usec / 1000;
        if (!first && (nv->it_value.tv_sec || nv->it_value.tv_usec)) first = 1;
        itimer_arm(first, iv);
    }
    return 0;
}
int getitimer(int which, cx_itimerval_t *v) { return setitimer(which, 0, v); }
int getrusage(int who, void *ru) { (void)who; memset(ru, 0, 144); return 0; }
const char *cx_at_path(int dirfd, const char *p, char *buf, size_t cap);
int faccessat(int fd, const char *path, int mode, int flag) {
    (void)flag;
    char t[1024];
    const char *q = cx_at_path(fd, path, t, sizeof t);   /* AT_FDCWD (-2), absoluto ou relativo ao dirfd */
    return q ? access(q, mode) : -1;
}
int fpurge(void *f) { (void)f; return 0; }
typedef struct { long tv_sec; int tv_usec; } cx_tv_t;
typedef struct { long tv_sec; long tv_nsec; } cx_ts_t;
int select(int nfds, void *rd, void *wr, void *ex, void *tv);
int pselect(int nfds, void *rd, void *wr, void *ex, const cx_ts_t *ts, const void *mask) {
    (void)mask;
    cx_tv_t tv, *tp = 0;
    if (ts) { tv.tv_sec = ts->tv_sec; tv.tv_usec = (int)(ts->tv_nsec / 1000); tp = &tv; }
    return select(nfds, rd, wr, ex, tp);
}

/* ------------------------------------------------------------ tsearch (árvore binária simples) */
typedef struct cx_tnode { const void *key; struct cx_tnode *l, *r; } cx_tnode;
void *tsearch(const void *key, void **root, int (*cmp)(const void *, const void *)) {
    if (!root) return 0;
    cx_tnode **p = (cx_tnode **)root;
    while (*p) {
        int c = cmp(key, (*p)->key);
        if (!c) return *p;
        p = c < 0 ? &(*p)->l : &(*p)->r;
    }
    cx_tnode *n = calloc(1, sizeof *n);
    if (!n) return 0;
    n->key = key;
    *p = n;
    return n;
}
void *tfind(const void *key, void *const *root, int (*cmp)(const void *, const void *)) {
    if (!root) return 0;
    cx_tnode *n = *(cx_tnode *const *)root;
    while (n) {
        int c = cmp(key, n->key);
        if (!c) return n;
        n = c < 0 ? n->l : n->r;
    }
    return 0;
}
void *tdelete(const void *key, void **root, int (*cmp)(const void *, const void *)) {
    if (!root || !*root) return 0;
    cx_tnode **p = (cx_tnode **)root, *parent = (cx_tnode *)1;   /* POSIX: raiz removida devolve não-NULL */
    while (*p) {
        int c = cmp(key, (*p)->key);
        if (!c) break;
        parent = *p;
        p = c < 0 ? &(*p)->l : &(*p)->r;
    }
    if (!*p) return 0;
    cx_tnode *d = *p;
    if (!d->l) *p = d->r;
    else if (!d->r) *p = d->l;
    else {
        cx_tnode **m = &d->r;
        while ((*m)->l) m = &(*m)->l;
        cx_tnode *s = *m;
        *m = s->r;
        s->l = d->l; s->r = d->r;
        *p = s;
    }
    free(d);
    return parent;
}
static void twalk_r(const cx_tnode *n, void (*act)(const void *, int, int), int depth) {
    if (!n) return;
    if (!n->l && !n->r) { act(n, 3, depth); return; }       /* leaf */
    act(n, 0, depth);                                       /* preorder */
    twalk_r(n->l, act, depth + 1);
    act(n, 1, depth);                                       /* postorder */
    twalk_r(n->r, act, depth + 1);
    act(n, 2, depth);                                       /* endorder */
}
void twalk(const void *root, void (*act)(const void *, int, int)) { twalk_r(root, act, 0); }

/* ------------------------------------------------------------ fnmatch */
#define FNM_NOESCAPE 0x01
#define FNM_PATHNAME 0x02
#define FNM_PERIOD   0x04
#define FNM_CASEFOLD 0x10
static int fold(int c, int f) { return (f & FNM_CASEFOLD) ? tolower(c) : c; }
/* [classe]: devolve o fim da classe (depois do ']') e *hit; NULL se a classe não fecha */
static const char *bracket(const char *p, int c, int flags, int *hit) {
    int neg = 0, ok = 0;
    if (*p == '!' || *p == '^') { neg = 1; p++; }
    const char *start = p;
    while (*p && (*p != ']' || p == start)) {
        int lo = (unsigned char)*p;
        if (lo == '\\' && !(flags & FNM_NOESCAPE) && p[1]) lo = (unsigned char)*++p;
        else if (lo == '[' && p[1] == ':') {
            const char *e = p + 2;
            while (*e && !(e[0] == ':' && e[1] == ']')) e++;
            if (*e) {
                char nm[16]; size_t n = (size_t)(e - p - 2);
                if (n < sizeof nm) { memcpy(nm, p + 2, n); nm[n] = 0; if (iswctype(c, wctype(nm))) ok = 1; }
                p = e + 2;
                continue;
            }
        }
        p++;
        int hi = lo;
        if (*p == '-' && p[1] && p[1] != ']') {
            hi = (unsigned char)p[1];
            if (hi == '\\' && !(flags & FNM_NOESCAPE) && p[2]) { hi = (unsigned char)p[2]; p++; }
            p += 2;
        }
        if (fold(c, flags) >= fold(lo, flags) && fold(c, flags) <= fold(hi, flags)) ok = 1;
    }
    if (*p != ']') return 0;
    *hit = ok ^ neg;
    return p + 1;
}
static int fnm(const char *p, const char *s, int flags, const char *sstart) {
    for (;;) {
        int pc = (unsigned char)*p, sc = (unsigned char)*s;
        int lead = sc == '.' && (flags & FNM_PERIOD) && (s == sstart || ((flags & FNM_PATHNAME) && s[-1] == '/'));
        switch (pc) {
        case 0: return sc ? 1 : 0;
        case '?':
            if (!sc || (sc == '/' && (flags & FNM_PATHNAME)) || lead) return 1;
            p++; s++; continue;
        case '*':
            while (*p == '*') p++;
            if (lead) return 1;
            if (!*p) return (flags & FNM_PATHNAME) && strchr(s, '/') ? 1 : 0;
            for (; *s; s++) {
                if (!fnm(p, s, flags & ~FNM_PERIOD, sstart)) return 0;
                if (*s == '/' && (flags & FNM_PATHNAME)) return 1;
            }
            return fnm(p, s, flags, sstart);
        case '[': {
            if (!sc || (sc == '/' && (flags & FNM_PATHNAME)) || lead) return 1;
            int hit = 0;
            const char *e = bracket(p + 1, sc, flags, &hit);
            if (!e) { if (sc != '[') return 1; p++; s++; continue; }   /* '[' sem fim é literal */
            if (!hit) return 1;
            p = e; s++; continue;
        }
        case '\\':
            if (!(flags & FNM_NOESCAPE) && p[1]) pc = (unsigned char)*++p;
            /* fallthrough */
        default:
            if (fold(pc, flags) != fold(sc, flags)) return 1;
            p++; s++; continue;
        }
    }
}
int fnmatch(const char *pat, const char *s, int flags) { return fnm(pat, s, flags, s); }

/* ------------------------------------------------------------ regex POSIX (BRE e ERE)
 * Compila para uma árvore e casa por retrocesso. Cobre o que o bash ([[ =~ ]]), o readline e os
 * scripts do Homebrew usam: . [] [^] [[:classe:]] * + ? {m,n} ^ $ | () e grupos com regmatch_t. */
#define REG_EXTENDED 1
#define REG_ICASE    2
#define REG_NOSUB    4
#define REG_NEWLINE  8
#define REG_NOTBOL   1
#define REG_NOTEOL   2
#define REG_NOMATCH  1
#define REG_BADPAT   2
#define REG_EPAREN   8
#define REG_EBRACK   7
#define REG_ESPACE  12
#define REG_BADRPT  13

typedef struct { long re_magic; size_t re_nsub; const char *re_endp; void *re_g; } mac_regex_t;
typedef struct { long long rm_so, rm_eo; } mac_regmatch_t;

enum { N_CHAR, N_ANY, N_SET, N_BOL, N_EOL, N_CAT, N_ALT, N_REP, N_GROUP, N_EMPTY, N_BACKREF };
typedef struct rnode {
    int kind;
    int c;                       /* N_CHAR; N_GROUP/N_BACKREF: número do grupo */
    int min, max;                /* N_REP (max -1 = sem limite) */
    unsigned char set[32];       /* N_SET: bitmap dos 256 bytes */
    struct rnode *a, *b;
} rnode;
typedef struct {
    const char *p;
    int ext, icase, newline, err;
    int ngroups;
} rparse;

static rnode *rn(int kind) { rnode *n = calloc(1, sizeof *n); if (n) n->kind = kind; return n; }
static void rfree(rnode *n) { if (!n) return; rfree(n->a); rfree(n->b); free(n); }
static void setbit(rnode *n, int c) { n->set[(unsigned char)c >> 3] |= (unsigned char)(1u << (c & 7)); }

static rnode *r_alt(rparse *P);
static rnode *r_set(rparse *P) {
    rnode *n = rn(N_SET);
    if (!n) { P->err = REG_ESPACE; return 0; }
    int neg = 0;
    if (*P->p == '^') { neg = 1; P->p++; }
    const char *start = P->p;
    while (*P->p && (*P->p != ']' || P->p == start)) {
        int lo = (unsigned char)*P->p;
        if (lo == '[' && (P->p[1] == ':' || P->p[1] == '.' || P->p[1] == '=')) {
            char kind = P->p[1];
            const char *e = P->p + 2;
            while (*e && !(e[0] == kind && e[1] == ']')) e++;
            if (!*e) { P->err = REG_EBRACK; return n; }
            char nm[16]; size_t len = (size_t)(e - P->p - 2);
            if (kind == ':' && len < sizeof nm) {
                memcpy(nm, P->p + 2, len); nm[len] = 0;
                unsigned long t = wctype(nm);
                for (int c = 0; c < 256; c++) if (iswctype(c, t)) setbit(n, c);
            } else if (len == 1) setbit(n, (unsigned char)P->p[2]);
            P->p = e + 2;
            continue;
        }
        P->p++;
        int hi = lo;
        if (*P->p == '-' && P->p[1] && P->p[1] != ']') { hi = (unsigned char)P->p[1]; P->p += 2; }
        for (int c = lo; c <= hi; c++) {
            setbit(n, c);
            if (P->icase) { if (c >= 'a' && c <= 'z') setbit(n, c - 32); else if (c >= 'A' && c <= 'Z') setbit(n, c + 32); }
        }
    }
    if (*P->p != ']') { P->err = REG_EBRACK; return n; }
    P->p++;
    if (neg) { for (int i = 0; i < 32; i++) n->set[i] = (unsigned char)~n->set[i]; if (P->newline) n->set['\n' >> 3] &= (unsigned char)~(1u << ('\n' & 7)); }
    return n;
}
static rnode *r_atom(rparse *P) {
    const char *p = P->p;
    int c = (unsigned char)*p;
    if (P->ext ? c == '(' : (c == '\\' && p[1] == '(')) {
        P->p += P->ext ? 1 : 2;
        int g = ++P->ngroups;
        rnode *in = r_alt(P);
        if (P->ext ? *P->p != ')' : (P->p[0] != '\\' || P->p[1] != ')')) { P->err = REG_EPAREN; return in; }
        P->p += P->ext ? 1 : 2;
        rnode *n = rn(N_GROUP);
        if (!n) { P->err = REG_ESPACE; return in; }
        n->c = g; n->a = in;
        return n;
    }
    if (c == '.') { P->p++; rnode *n = rn(N_ANY); return n; }
    if (c == '[') { P->p++; return r_set(P); }
    if (c == '^') { P->p++; return rn(N_BOL); }
    if (c == '$') {
        /* BRE: '$' só é âncora no fim da expressão ou antes de \) e | */
        if (P->ext || !p[1] || (p[1] == '\\' && (p[2] == ')' || p[2] == '|'))) { P->p++; return rn(N_EOL); }
    }
    if (c == '\\' && p[1]) {
        P->p += 2;
        int e = (unsigned char)p[1];
        if (e >= '1' && e <= '9') { rnode *n = rn(N_BACKREF); if (n) n->c = e - '0'; return n; }
        rnode *n = rn(N_CHAR);
        if (n) n->c = e == 'n' ? '\n' : e == 't' ? '\t' : e;
        return n;
    }
    P->p++;
    rnode *n = rn(N_CHAR);
    if (n) n->c = c;
    return n;
}
static int r_isrep(rparse *P) {
    const char *p = P->p;
    if (*p == '*') return 1;
    if (P->ext) return *p == '+' || *p == '?' || *p == '{';
    return p[0] == '\\' && (p[1] == '{' || p[1] == '+' || p[1] == '?');
}
static rnode *r_piece(rparse *P) {
    rnode *a = r_atom(P);
    while (!P->err && a && r_isrep(P)) {
        int mn = 0, mx = -1;
        if (!P->ext && *P->p == '\\') P->p++;
        char op = *P->p++;
        if (op == '+') mn = 1;
        else if (op == '?') mx = 1;
        else if (op == '{') {
            mn = 0;
            while (*P->p >= '0' && *P->p <= '9') mn = mn * 10 + (*P->p++ - '0');
            mx = mn;
            if (*P->p == ',') {
                P->p++;
                if (*P->p >= '0' && *P->p <= '9') { mx = 0; while (*P->p >= '0' && *P->p <= '9') mx = mx * 10 + (*P->p++ - '0'); }
                else mx = -1;
            }
            if (!P->ext && *P->p == '\\') P->p++;
            if (*P->p != '}') { P->err = REG_BADRPT; return a; }
            P->p++;
        }
        rnode *r = rn(N_REP);
        if (!r) { P->err = REG_ESPACE; return a; }
        r->a = a; r->min = mn; r->max = mx;
        a = r;
    }
    return a;
}
static int r_branch_end(rparse *P) {
    const char *p = P->p;
    if (!*p) return 1;
    if (P->ext) return *p == '|' || *p == ')';
    return p[0] == '\\' && (p[1] == '|' || p[1] == ')');
}
static rnode *r_branch(rparse *P) {
    rnode *seq = 0;
    while (!P->err && !r_branch_end(P)) {
        rnode *x = r_piece(P);
        if (!x) { if (!P->err) P->err = REG_ESPACE; break; }
        if (!seq) seq = x;
        else { rnode *c = rn(N_CAT); if (!c) { P->err = REG_ESPACE; rfree(x); break; } c->a = seq; c->b = x; seq = c; }
    }
    return seq ? seq : rn(N_EMPTY);
}
static rnode *r_alt(rparse *P) {
    rnode *a = r_branch(P);
    while (!P->err && (P->ext ? *P->p == '|' : (P->p[0] == '\\' && P->p[1] == '|'))) {
        P->p += P->ext ? 1 : 2;
        rnode *b = r_branch(P);
        rnode *n = rn(N_ALT);
        if (!n) { P->err = REG_ESPACE; rfree(b); break; }
        n->a = a; n->b = b;
        a = n;
    }
    return a;
}

typedef struct {
    const char *str, *end;
    int icase, newline, notbol, noteol;
    mac_regmatch_t *m;
    int nm;
    int steps;
} rmatch;
typedef int (*rcont)(rmatch *M, const char *s, void *ctx);
static int rm_node(rmatch *M, rnode *n, const char *s, rcont k, void *ctx);
static int eqc(rmatch *M, int a, int b) {
    if (!M->icase) return a == b;
    if (a >= 'A' && a <= 'Z') a += 32;
    if (b >= 'A' && b <= 'Z') b += 32;
    return a == b;
}
typedef struct { rnode *n; rcont k; void *ctx; } cat_ctx;
static int cat_k(rmatch *M, const char *s, void *c) { cat_ctx *x = c; return rm_node(M, x->n, s, x->k, x->ctx); }
typedef struct { rnode *n; int left; int count; const char *last; rcont k; void *ctx; } rep_ctx;
static int rep_try(rmatch *M, rep_ctx *x, const char *s);
static int rep_k(rmatch *M, const char *s, void *c) {
    rep_ctx *x = c;
    if (s == x->last && x->count >= x->n->min) return 0;   /* repetição vazia: evita laço infinito */
    rep_ctx y = *x;
    y.count++; y.last = s;
    return rep_try(M, &y, s);
}
static int rep_try(rmatch *M, rep_ctx *x, const char *s) {
    if (x->n->max < 0 || x->count < x->n->max)            /* guloso: tenta mais uma primeiro */
        if (rm_node(M, x->n->a, s, rep_k, x)) return 1;
    if (x->count >= x->n->min) return x->k(M, s, x->ctx);
    return 0;
}
typedef struct { int g; const char *start; long long oso, oeo; rcont k; void *ctx; } grp_ctx;
static int grp_k(rmatch *M, const char *s, void *c) {
    grp_ctx *x = c;
    long long so = 0, eo = 0;
    if (x->g < M->nm) { so = M->m[x->g].rm_so; eo = M->m[x->g].rm_eo; M->m[x->g].rm_so = x->start - M->str; M->m[x->g].rm_eo = s - M->str; }
    if (x->k(M, s, x->ctx)) return 1;
    if (x->g < M->nm) { M->m[x->g].rm_so = so; M->m[x->g].rm_eo = eo; }
    return 0;
}
static int rm_node(rmatch *M, rnode *n, const char *s, rcont k, void *ctx) {
    if (++M->steps > 2000000) return 0;                    /* teto de retrocesso */
    switch (n->kind) {
    case N_EMPTY: return k(M, s, ctx);
    case N_CHAR: return s < M->end && eqc(M, (unsigned char)*s, n->c) && k(M, s + 1, ctx);
    case N_ANY: return s < M->end && !(M->newline && *s == '\n') && k(M, s + 1, ctx);
    case N_SET: { int c = (unsigned char)*s; return s < M->end && (n->set[c >> 3] >> (c & 7) & 1) && k(M, s + 1, ctx); }
    case N_BOL: return ((s == M->str && !M->notbol) || (M->newline && s > M->str && s[-1] == '\n')) && k(M, s, ctx);
    case N_EOL: return ((s == M->end && !M->noteol) || (M->newline && s < M->end && *s == '\n')) && k(M, s, ctx);
    case N_CAT: { cat_ctx c = {n->b, k, ctx}; return rm_node(M, n->a, s, cat_k, &c); }
    case N_ALT: return rm_node(M, n->a, s, k, ctx) || rm_node(M, n->b, s, k, ctx);
    case N_REP: { rep_ctx x = {n, 0, 0, 0, k, ctx}; x.last = 0; return rep_try(M, &x, s); }
    case N_GROUP: { grp_ctx x = {n->c, s, 0, 0, k, ctx}; return rm_node(M, n->a, s, grp_k, &x); }
    case N_BACKREF: {
        if (n->c >= M->nm || M->m[n->c].rm_so < 0) return 0;
        long long len = M->m[n->c].rm_eo - M->m[n->c].rm_so;
        if (M->end - s < len) return 0;
        for (long long i = 0; i < len; i++) if (!eqc(M, (unsigned char)s[i], (unsigned char)M->str[M->m[n->c].rm_so + i])) return 0;
        return k(M, s + len, ctx);
    }
    }
    return 0;
}
static int final_k(rmatch *M, const char *s, void *c) { *(const char **)c = s; (void)M; return 1; }

int regcomp(mac_regex_t *re, const char *pat, int flags) {
    rparse P = {pat, flags & REG_EXTENDED, flags & REG_ICASE, flags & REG_NEWLINE, 0, 0};
    rnode *root = r_alt(&P);
    if (!P.err && *P.p) P.err = P.ext && *P.p == ')' ? REG_EPAREN : REG_BADPAT;
    if (P.err || !root) { rfree(root); return P.err ? P.err : REG_ESPACE; }
    struct { rnode *root; int flags; } *g = malloc(sizeof *g);
    if (!g) { rfree(root); return REG_ESPACE; }
    g->root = root; g->flags = flags;
    re->re_magic = 0xF00D;
    re->re_nsub = (size_t)P.ngroups;
    re->re_endp = P.p;
    re->re_g = g;
    return 0;
}
int regexec(const mac_regex_t *re, const char *str, size_t nmatch, mac_regmatch_t *pm, int eflags) {
    if (!re || re->re_magic != 0xF00D) return REG_BADPAT;
    struct { rnode *root; int flags; } *g = re->re_g;
    mac_regmatch_t local[10];
    size_t nm = re->re_nsub + 1;
    mac_regmatch_t *m = nm <= 10 ? local : malloc(nm * sizeof *m);
    if (!m) return REG_ESPACE;
    rmatch M = {str, str + strlen(str), g->flags & REG_ICASE, g->flags & REG_NEWLINE, eflags & REG_NOTBOL, eflags & REG_NOTEOL, m, (int)nm, 0};
    int rc = REG_NOMATCH;
    for (const char *s = str; ; s++) {
        for (size_t i = 0; i < nm; i++) m[i].rm_so = m[i].rm_eo = -1;
        const char *e = 0;
        M.steps = 0;
        if (rm_node(&M, g->root, s, final_k, &e)) {
            m[0].rm_so = s - str; m[0].rm_eo = e - str;
            rc = 0;
            break;
        }
        if (!*s) break;
    }
    if (!rc && pm && !(g->flags & REG_NOSUB))
        for (size_t i = 0; i < nmatch; i++) pm[i] = i < nm ? m[i] : (mac_regmatch_t){-1, -1};
    if (m != local) free(m);
    return rc;
}
void regfree(mac_regex_t *re) {
    if (!re || re->re_magic != 0xF00D) return;
    struct { rnode *root; int flags; } *g = re->re_g;
    rfree(g->root);
    free(g);
    re->re_magic = 0;
}
size_t regerror(int code, const mac_regex_t *re, char *buf, size_t len) {
    (void)re;
    const char *m = code == REG_NOMATCH ? "regexec() failed to match" : code == REG_EPAREN ? "parentheses not balanced"
                  : code == REG_EBRACK ? "brackets ([ ]) not balanced" : code == REG_BADRPT ? "invalid repetition count(s)"
                  : code == REG_ESPACE ? "out of memory" : "invalid regular expression";
    size_t n = strlen(m) + 1;
    if (buf && len) { size_t c = n < len ? n : len; memcpy(buf, m, c - 1); buf[c - 1] = 0; }
    return n;
}

/* ------------------------------------------------------------ diversos */
int mkfifo(const char *path, unsigned short mode) { (void)path; (void)mode; cx_errno = 45; return -1; }   /* ENOTSUP: sem FIFOs nomeados */
void sync(void) {}
typedef struct { unsigned s_addr; } cx_in_addr_t;
char *inet_ntoa(cx_in_addr_t a) {
    static char b[16];
    unsigned char *p = (unsigned char *)&a.s_addr;
    int o = 0;
    for (int i = 0; i < 4; i++) {
        unsigned v = p[i];
        if (v >= 100) b[o++] = (char)('0' + v / 100);
        if (v >= 10) b[o++] = (char)('0' + v / 10 % 10);
        b[o++] = (char)('0' + v % 10);
        if (i < 3) b[o++] = '.';
    }
    b[o] = 0;
    return b;
}

/* ============================================================ coreutils 9.11 e portable-ruby 3.4.5 */
int chdir(const char *p);
int fork(void);
int symlink(const char *t, const char *p);
long readlink(const char *p, char *b, size_t n);
int execve(const char *path, char *const argv[], char *const envp[]);
extern char **environ;
int open(const char *p, int flags, ...);
long read(int fd, void *b, size_t n);
int close(int fd);
int clock_gettime(int clk, cx_ts_t *ts);
char *getlogin(void);
size_t strlcpy(char *d, const char *s, size_t n);
const void *_dyld_get_image_header(unsigned i);
int usleep(unsigned us);

/* half (binary16) -> float: o compiler-rt do macOS exporta isto pelo libSystem */
float __extendhfsf2(unsigned short h) {
    unsigned s = (unsigned)(h >> 15) << 31, e = (h >> 10) & 0x1F, m = h & 0x3FF, f;
    if (e == 0) {
        if (!m) f = s;
        else { e = 127 - 15 + 1; while (!(m & 0x400)) { m <<= 1; e--; } m &= 0x3FF; f = s | e << 23 | m << 13; }
    } else if (e == 31) f = s | 0x7F800000u | m << 13;
    else f = s | (e + 127 - 15) << 23 | m << 13;
    union { unsigned u; float x; } v = {f};
    return v.x;
}

/* ACL de acesso no processo (cli.c). ACL default de diretório não existe no FAT: ENOENT/ENOTSUP. */
void *cx_acl_get_path(const char *path, int follow);
int cx_acl_set_path(const char *path, void *a, int follow);
void *acl_get_fd(int fd);
int acl_set_fd(int fd, void *a);
void *acl_get_file(const char *p, int t) { if (t == 1) { cx_errno = 2; return 0; } return cx_acl_get_path(p, 1); }
void *acl_get_link_np(const char *p, int t) { if (t == 1) { cx_errno = 2; return 0; } return cx_acl_get_path(p, 0); }
void *acl_get_fd_np(int fd, int t) { if (t == 1) { cx_errno = 2; return 0; } return acl_get_fd(fd); }
int acl_set_file(const char *p, int t, void *a) { if (t == 1) { cx_errno = 45; return -1; } return cx_acl_set_path(p, a, 1); }

int clock_getres(int clk, cx_ts_t *ts) { (void)clk; if (ts) { ts->tv_sec = 0; ts->tv_nsec = 1000000; } return 0; }   /* relógio do kernel: 1 ms */
int clock_settime(int clk, const cx_ts_t *ts) { (void)clk; (void)ts; cx_errno = 1; return -1; }
int settimeofday(const void *tv, const void *tz) { (void)tv; (void)tz; cx_errno = 1; return -1; }
long gethostid(void) { return 0; }
int fdatasync(int fd) { (void)fd; return 0; }
void setutxent(void) {}
void endutxent(void) {}
void *getutxent(void) { return 0; }            /* sem registro de logins (who/users ficam vazios) */
int utmpxname(const char *f) { (void)f; return 0; }

/* *at(): AT_FDCWD (-2) ou caminho absoluto; um dirfd de verdade vira o caminho dele (SYS_FDPATH) */
const char *cx_at_path(int dirfd, const char *p, char *buf, size_t cap) {   /* também usado por foundationsys.c e extra.c */
    if (dirfd == -2 || p[0] == '/') return p;
    char d[1024];
    if (cx_syscall(SYS_FDPATH, dirfd, (long)d, sizeof d, 0, 0) < 0) { cx_errno = 9; return 0; }
    size_t n = strlen(d);
    if (n + strlen(p) + 2 > cap) { cx_errno = 63; return 0; }
    memcpy(buf, d, n);
    buf[n] = '/';
    strlcpy(buf + n + 1, p, cap - n - 1);
    return buf;
}
int fchownat(int fd, const char *p, unsigned u, unsigned g, int fl) { (void)fd; (void)p; (void)u; (void)g; (void)fl; return 0; }
int mkfifoat(int fd, const char *p, unsigned short m) { (void)fd; (void)p; (void)m; cx_errno = 45; return -1; }
int mknodat(int fd, const char *p, unsigned short m, int dev) { (void)fd; (void)p; (void)m; (void)dev; cx_errno = 1; return -1; }
long readlinkat(int fd, const char *p, char *b, size_t n) { char t[1024]; const char *q = cx_at_path(fd, p, t, sizeof t); return q ? readlink(q, b, n) : -1; }
int symlinkat(const char *tgt, int fd, const char *p) { char t[1024]; const char *q = cx_at_path(fd, p, t, sizeof t); return q ? symlink(tgt, q) : -1; }
int fchdir(int fd) {
    char d[1024];
    long r = cx_syscall(SYS_FDPATH, fd, (long)d, sizeof d, 0, 0);
    if (r < 0) { cx_errno = (int)-r; return -1; }
    return chdir(d);
}

/* sigset_t do macOS: 32 bits */
int sigdelset(unsigned *s, int n) { if (n < 1 || n > 32) { cx_errno = 22; return -1; } *s &= ~(1u << (n - 1)); return 0; }
int sigfillset(unsigned *s) { *s = 0xFFFFFFFFu; return 0; }
int sigismember(const unsigned *s, int n) { if (n < 1 || n > 32) { cx_errno = 22; return -1; } return (*s >> (n - 1)) & 1; }
/* sigsuspend, sigwait, sigpending, sigprocmask: libc.c (SYS_SIG) */

/* strmode: "drwxr-xr-x " (10 caracteres, espaço e NUL), como o ls -l */
void strmode(unsigned short mode, char *p) {
    unsigned t = mode & 0170000;
    *p++ = t == 0040000 ? 'd' : t == 0120000 ? 'l' : t == 0020000 ? 'c' : t == 0060000 ? 'b' : t == 0010000 ? 'p' : t == 0140000 ? 's' : '-';
    static const char rwx[] = "rwxrwxrwx";
    for (int i = 0; i < 9; i++) p[i] = (mode >> (8 - i)) & 1 ? rwx[i] : '-';
    if (mode & 04000) p[2] = p[2] == 'x' ? 's' : 'S';
    if (mode & 02000) p[5] = p[5] == 'x' ? 's' : 'S';
    if (mode & 01000) p[8] = p[8] == 'x' ? 't' : 'T';
    p[9] = ' ';
    p[10] = 0;
}
/* vfork: o pai fica suspenso e o filho usa o mesmo pml4 até exec ou _exit.
 * A tabela de descritores é a do pai até exec ou _exit. Com outras threads o kernel devolve EINVAL. */
int vfork(void) {
    long r = SYS0(SYS_VFORK);
    if (r < 0) { cx_errno = (int)-r; return -1; }
    return (int)r;
}

/* execl/execle: variádicas sobre o execve */
int execl(const char *path, const char *a0, ...) {
    char *av[64];
    int n = 0;
    __builtin_va_list ap;
    __builtin_va_start(ap, a0);
    for (const char *s = a0; s && n < 63; s = __builtin_va_arg(ap, const char *)) av[n++] = (char *)s;
    __builtin_va_end(ap);
    av[n] = 0;
    return execve(path, av, environ);
}
int execle(const char *path, const char *a0, ...) {
    char *av[64];
    int n = 0;
    __builtin_va_list ap;
    __builtin_va_start(ap, a0);
    for (const char *s = a0; s && n < 63; s = __builtin_va_arg(ap, const char *)) av[n++] = (char *)s;
    av[n] = 0;
    char *const *ev = __builtin_va_arg(ap, char *const *);
    __builtin_va_end(ap);
    return execve(path, av, ev);
}

/* times(): sem contagem de CPU por processo aqui; devolve o relógio em ticks de 100 Hz */
typedef struct { unsigned long tms_utime, tms_stime, tms_cutime, tms_cstime; } cx_tms_t;
unsigned long times(cx_tms_t *t) {
    if (t) memset(t, 0, sizeof *t);
    cx_ts_t ts;
    clock_gettime(6, &ts);                         /* CLOCK_MONOTONIC_RAW */
    return (unsigned long)ts.tv_sec * 100 + (unsigned long)ts.tv_nsec / 10000000;
}
/* truncate: o kernel grava arquivos inteiros; lê o começo e regrava com o tamanho pedido (o resto zerado) */
int truncate(const char *path, long long len) {
    if (len < 0) { cx_errno = 22; return -1; }
    char *buf = len ? calloc(1, (size_t)len) : 0;
    if (len && !buf) { cx_errno = 12; return -1; }
    int fd = open(path, 0);
    if (fd < 0) { free(buf); return -1; }
    long got = 0, k;
    while (got < len && (k = read(fd, buf + got, (size_t)(len - got))) > 0) got += k;
    close(fd);
    long r = cx_syscall(SYS_WRITEFILE, (long)path, (long)buf, (long)len, 0, 0);
    free(buf);
    if (r < 0) { cx_errno = (int)-r; return -1; }
    return 0;
}
char *crypt(const char *k, const char *salt) { (void)k; (void)salt; cx_errno = 45; return 0; }
void *gethostbyname(const char *n) { (void)n; return 0; }     /* o ruby usa getaddrinfo; estes são legados */
void *gethostbyaddr(const void *a, unsigned l, int t) { (void)a; (void)l; (void)t; return 0; }
int getlogin_r(char *buf, size_t n) { const char *s = getlogin(); if (!s) s = "visitante"; if (strlen(s) + 1 > n) return 34; strlcpy(buf, s, n); return 0; }
int setreuid(unsigned r, unsigned e) { (void)r; (void)e; return 0; }
int setregid(unsigned r, unsigned e) { (void)r; (void)e; return 0; }
int setruid(unsigned r) { (void)r; return 0; }
int setrgid(unsigned r) { (void)r; return 0; }
/* ucontext: o ruby usa as próprias corrotinas em assembly no x86_64; estas só falham */
int getcontext(void *u) { (void)u; cx_errno = 45; return -1; }
int setcontext(const void *u) { (void)u; cx_errno = 45; return -1; }
void makecontext(void *u, void (*f)(void), int argc, ...) { (void)u; (void)f; (void)argc; }
int proc_regionfilename(int pid, unsigned long long addr, void *buf, unsigned size) { (void)pid; (void)addr; (void)buf; (void)size; return 0; }
int vm_region_recurse_64(unsigned task, unsigned long long *addr, unsigned long long *size, unsigned *depth, void *info, unsigned *cnt) {
    (void)task; (void)addr; (void)size; (void)depth; (void)info; (void)cnt;
    return 1;                                       /* KERN_INVALID_ADDRESS: o ruby para de varrer as regiões */
}
long __syscall(long n, ...) { (void)n; cx_errno = 78; return -1; }   /* ENOSYS */
const void *_NSGetMachExecuteHeader(void) { return _dyld_get_image_header(0); }
void CFStringNormalize(void *s, long form) { (void)s; (void)form; }   /* só NFC/ASCII no mOS: nada a fazer */

/* getattrlist/fgetattrlist: só ATTR_VOL_CAPABILITIES (o ruby e o Foundation perguntam se o volume diferencia
 * maiúsculas). Os volumes do mOS (FAT32, APFS/HFS+ só leitura) preservam o caso mas não o diferenciam. */
typedef struct { unsigned short bitmapcount, reserved; unsigned commonattr, volattr, dirattr, fileattr, forkattr; } cx_attrlist_t;
static int vol_caps(const cx_attrlist_t *al, void *buf, size_t n) {
    if (!al || al->commonattr || al->dirattr || al->fileattr || al->forkattr ||
        (al->volattr & ~0x80020000u) != 0 || !(al->volattr & 0x20000u)) { cx_errno = 45; return -1; }
    if (n < 4 + 32) { cx_errno = 34; return -1; }
    unsigned *o = buf;
    o[0] = 4 + 32;                                 /* tamanho do que foi devolvido */
    memset(o + 1, 0, 32);
    o[1] = 0x200 | 0x2;                            /* FORMAT: CASE_PRESERVING | SYMBOLICLINKS (links do libSystem) */
    o[5] = 0x100 | 0x200 | 0x2;                    /* valid: CASE_SENSITIVE, CASE_PRESERVING, SYMBOLICLINKS */
    return 0;
}
/* atributos de objeto: ATTR_CMN_OBJTAG (0x10: tipo do sistema de arquivos; aqui VT_NON, então o Dir.empty?
 * do ruby não usa o atalho do HFS e cai no readdir) e ATTR_DIR_ENTRYCOUNT (dirattr 0x2: entradas da pasta). */
void *opendir(const char *p);
void *readdir(void *d);
int closedir(void *d);
static int obj_attrs(const char *path, const cx_attrlist_t *al, void *buf, size_t n) {
    if (al->volattr || al->fileattr || al->forkattr || (al->commonattr & ~0x10u) || (al->dirattr & ~0x2u) ||
        (!al->commonattr && !al->dirattr)) { cx_errno = 45; return -1; }
    unsigned *o = buf;
    size_t need = 4 + (al->commonattr ? 4 : 0) + (al->dirattr ? 4 : 0);
    if (n < need) { cx_errno = 34; return -1; }
    int k = 1;
    if (al->commonattr & 0x10) o[k++] = 0;                 /* VT_NON */
    if (al->dirattr & 0x2) {
        void *d = opendir(path);
        if (!d) return -1;
        unsigned cnt = 0;
        while (readdir(d)) cnt++;                         /* o readdir do mOS não devolve "." nem ".." */
        closedir(d);
        o[k++] = cnt;
    }
    o[0] = (unsigned)need;
    return 0;
}
int fgetattrlist(int fd, void *al, void *buf, size_t n, unsigned opt) {
    (void)opt;
    const cx_attrlist_t *a = al;
    if (a && a->volattr) return vol_caps(al, buf, n);
    char p[1024];
    long r = cx_syscall(SYS_FDPATH, fd, (long)p, sizeof p, 0, 0);
    if (r < 0) { cx_errno = (int)-r; return -1; }
    return obj_attrs(p, al, buf, n);
}
int cx_getattrlist(const char *p, void *al, void *buf, size_t n, unsigned opt) {
    (void)opt;
    if (access(p, 0)) return -1;
    const cx_attrlist_t *a = al;
    if (a && a->volattr) return vol_caps(al, buf, n);
    return obj_attrs(p, al, buf, n);
}

/* ============================================================ sed 4.10, grep 3.12, tar 1.35, xz, curl 8.21, git 2.55 */
int execvp(const char *file, char *const argv[]);
int strcoll(const char *a, const char *b);
char *strdup(const char *s);
void *realloc(void *p, size_t n);
void *opendir(const char *p);
void *readdir(void *d);
int closedir(void *d);
int stat(const char *p, void *st);
void qsort(void *b, size_t n, size_t sz, int (*cmp)(const void *, const void *));
int snprintf(char *b, size_t n, const char *fmt, ...);

int execlp(const char *file, const char *a0, ...) {
    char *av[64];
    int n = 0;
    __builtin_va_list ap;
    __builtin_va_start(ap, a0);
    for (const char *s = a0; s && n < 63; s = __builtin_va_arg(ap, const char *)) av[n++] = (char *)s;
    __builtin_va_end(ap);
    av[n] = 0;
    return execvp(file, av);
}
char *stpncpy(char *d, const char *s, size_t n) {
    size_t i = 0;
    for (; i < n && s[i]; i++) d[i] = s[i];
    char *end = d + i;
    for (; i < n; i++) d[i] = 0;
    return end;
}
int rpmatch(const char *r) {
    if (!r || !*r) return -1;
    if (*r == 'y' || *r == 'Y' || *r == 's' || *r == 'S') return 1;   /* "sim" também */
    if (*r == 'n' || *r == 'N') return 0;
    return -1;
}
int optreset;                                    /* getopt do BSD: o getopt_long daqui recomeça sozinho com optind = 1 */
int getopt_long(int argc, char *const argv[], const char *s, const void *lo, int *idx);
int getopt(int argc, char *const argv[], const char *s) { return getopt_long(argc, argv, s, 0, 0); }
/* openlog/closelog: oslog.c */
int acl_delete_def_file(const char *p) { (void)p; cx_errno = 45; return -1; }
void *acl_from_text(const char *t) { (void)t; cx_errno = 45; return 0; }
char *acl_to_text(void *a, long *len) { (void)a; if (len) *len = 0; cx_errno = 45; return 0; }

/* alphasort: dirent do macOS ($INODE64) tem o nome no byte 21 */
int alphasort(const void *a, const void *b) {
    const char *x = (const char *)(*(void *const *)a) + 21, *y = (const char *)(*(void *const *)b) + 21;
    return strcoll(x, y);
}

/* glob(3) do macOS: glob_t = {gl_pathc, gl_matchc, gl_offs, gl_flags, gl_pathv, ...funções} */
typedef struct { size_t gl_pathc; int gl_matchc; size_t gl_offs; int gl_flags; char **gl_pathv; void *fns[6]; } mac_glob_t;
#define GLOB_APPEND   0x0001
#define GLOB_DOOFFS   0x0002
#define GLOB_ERR      0x0004
#define GLOB_MARK     0x0008
#define GLOB_NOCHECK  0x0010
#define GLOB_NOSORT   0x0020
#define GLOB_NOESCAPE 0x2000
#define GLOB_NOSPACE  (-1)
#define GLOB_NOMATCH  (-3)
static int glob_add(mac_glob_t *g, const char *p, int mark) {
    size_t n = g->gl_offs + g->gl_pathc;
    char **v = realloc(g->gl_pathv, (n + 2) * sizeof *v);
    if (!v) return GLOB_NOSPACE;
    g->gl_pathv = v;
    size_t l = strlen(p);
    char *s = malloc(l + 2);
    if (!s) return GLOB_NOSPACE;
    memcpy(s, p, l + 1);
    if (mark && l && s[l - 1] != '/') { s[l] = '/'; s[l + 1] = 0; }
    v[n] = s;
    v[n + 1] = 0;
    g->gl_pathc++;
    g->gl_matchc++;
    return 0;
}
static int has_magic(const char *s, size_t n) { for (size_t i = 0; i < n; i++) if (s[i] == '*' || s[i] == '?' || s[i] == '[') return 1; return 0; }
static int is_dir_path(const char *p) {
    unsigned char st[144];
    return stat(p, st) == 0 && ((*(unsigned short *)(st + 4)) & 0170000) == 0040000;   /* st_mode no offset 4 */
}
/* expande base + o resto do padrão (pat), componente por componente */
static int glob_walk(const char *base, const char *pat, int flags, mac_glob_t *g) {
    while (*pat == '/') pat++;
    if (!*pat) return glob_add(g, base[0] ? base : "/", (flags & GLOB_MARK) && is_dir_path(base));
    const char *end = strchr(pat, '/');
    size_t n = end ? (size_t)(end - pat) : strlen(pat);
    char comp[256], next[1024];
    if (n >= sizeof comp) return 0;
    memcpy(comp, pat, n); comp[n] = 0;
    const char *rest = end ? end : "";
    if (!has_magic(comp, n)) {
        snprintf(next, sizeof next, "%s%s%s", base, base[0] && base[strlen(base) - 1] != '/' ? "/" : "", comp);
        if (!*rest) { unsigned char st[144]; return stat(next, st) == 0 ? glob_add(g, next, (flags & GLOB_MARK) && is_dir_path(next)) : 0; }
        return glob_walk(next, rest, flags, g);
    }
    void *d = opendir(base[0] ? base : ".");
    if (!d) return 0;
    const unsigned char *e;
    int rc = 0;
    while (!rc && (e = readdir(d))) {
        const char *name = (const char *)e + 21;
        if (!strcmp(name, ".") || !strcmp(name, "..")) continue;
        if (fnmatch(comp, name, FNM_PERIOD | ((flags & GLOB_NOESCAPE) ? FNM_NOESCAPE : 0))) continue;
        if (base[0]) snprintf(next, sizeof next, "%s%s%s", base, base[strlen(base) - 1] == '/' ? "" : "/", name);
        else snprintf(next, sizeof next, "%s", name);
        if (*rest) { if (is_dir_path(next)) rc = glob_walk(next, rest, flags, g); }
        else rc = glob_add(g, next, (flags & GLOB_MARK) && is_dir_path(next));
    }
    closedir(d);
    return rc;
}
static int glob_cmp(const void *a, const void *b) { return strcoll(*(char *const *)a, *(char *const *)b); }
int glob(const char *pat, int flags, int (*errf)(const char *, int), mac_glob_t *g) {
    (void)errf;
    if (!(flags & GLOB_APPEND)) {
        g->gl_pathc = 0; g->gl_matchc = 0; g->gl_pathv = 0;
        if (!(flags & GLOB_DOOFFS)) g->gl_offs = 0;
        if (g->gl_offs) { g->gl_pathv = calloc(g->gl_offs + 1, sizeof(char *)); if (!g->gl_pathv) return GLOB_NOSPACE; }
    }
    g->gl_flags = flags;
    g->gl_matchc = 0;
    size_t first = g->gl_offs + g->gl_pathc;
    int rc = glob_walk(pat[0] == '/' ? "/" : "", pat, flags, g);
    if (rc) return rc;
    if (!g->gl_matchc) {
        if (flags & GLOB_NOCHECK) return glob_add(g, pat, 0);
        return GLOB_NOMATCH;
    }
    if (!(flags & GLOB_NOSORT)) qsort(g->gl_pathv + first, g->gl_offs + g->gl_pathc - first, sizeof(char *), glob_cmp);
    return 0;
}
void globfree(mac_glob_t *g) {
    if (g->gl_pathv) {
        for (size_t i = g->gl_offs; i < g->gl_offs + g->gl_pathc; i++) free(g->gl_pathv[i]);
        free(g->gl_pathv);
    }
    g->gl_pathv = 0;
    g->gl_pathc = 0;
}

/* LDAP.framework (o libcurl suporta ldap://): sem servidor LDAP no mOS, tudo falha com LDAP_NOT_SUPPORTED */
#define LDAP_NOT_SUPPORTED (-12)
void *ldap_init(const char *h, int p) { (void)h; (void)p; return 0; }
int ldap_set_option(void *ld, int o, const void *v) { (void)ld; (void)o; (void)v; return LDAP_NOT_SUPPORTED; }
int ldap_get_option(void *ld, int o, void *v) { (void)ld; (void)o; (void)v; return LDAP_NOT_SUPPORTED; }
int ldap_simple_bind_s(void *ld, const char *who, const char *pw) { (void)ld; (void)who; (void)pw; return LDAP_NOT_SUPPORTED; }
int ldap_search_s(void *ld, const char *b, int s, const char *f, char **a, int ao, void **res) { (void)ld; (void)b; (void)s; (void)f; (void)a; (void)ao; if (res) *res = 0; return LDAP_NOT_SUPPORTED; }
int ldap_unbind_s(void *ld) { (void)ld; return 0; }
char *ldap_err2string(int e) { (void)e; return "LDAP não suportado no mOS"; }
void *ldap_first_entry(void *ld, void *r) { (void)ld; (void)r; return 0; }
void *ldap_next_entry(void *ld, void *e) { (void)ld; (void)e; return 0; }
char *ldap_first_attribute(void *ld, void *e, void **b) { (void)ld; (void)e; if (b) *b = 0; return 0; }
char *ldap_next_attribute(void *ld, void *e, void *b) { (void)ld; (void)e; (void)b; return 0; }
char *ldap_get_dn(void *ld, void *e) { (void)ld; (void)e; return 0; }
void **ldap_get_values_len(void *ld, void *e, const char *a) { (void)ld; (void)e; (void)a; return 0; }
void ldap_value_free_len(void **v) { (void)v; }
void ldap_memfree(void *p) { (void)p; }
int ldap_msgfree(void *m) { (void)m; return 0; }
int ldap_url_parse(const char *u, void **d) { (void)u; if (d) *d = 0; return LDAP_NOT_SUPPORTED; }
void ldap_free_urldesc(void *d) { (void)d; }
void ber_free(void *b, int f) { (void)b; (void)f; }
void ber_memvfree(void **v) { (void)v; }

/* libicucore (IDN do libcurl): nomes ASCII passam em minúsculas; com bytes acima de 0x7F o
 * pedido falha (U_UNSUPPORTED_ERROR) e o curl informa que não converte o nome */
static int cx_uidna_obj;
void *uidna_openUTS46(unsigned opt, int *err) { (void)opt; (void)err; return &cx_uidna_obj; }
void uidna_close(void *u) { (void)u; }
static int idna_copy(const char *name, int len, char *dest, int cap, int *err) {
    if (len < 0) len = (int)strlen(name);
    for (int i = 0; i < len; i++) if ((unsigned char)name[i] >= 0x80) { *err = 16; return 0; }   /* U_UNSUPPORTED_ERROR */
    if (len >= cap) { *err = 15; return len; }                                                    /* U_BUFFER_OVERFLOW_ERROR */
    for (int i = 0; i < len; i++) dest[i] = (char)tolower((unsigned char)name[i]);
    dest[len] = 0;
    return len;
}
int uidna_nameToASCII_UTF8(void *u, const char *name, int len, char *dest, int cap, void *info, int *err) { (void)u; (void)info; return idna_copy(name, len, dest, cap, err); }
int uidna_nameToUnicodeUTF8(void *u, const char *name, int len, char *dest, int cap, void *info, int *err) { (void)u; (void)info; return idna_copy(name, len, dest, cap, err); }

/* ptsname_r (o ruby 4 importa para PTY.spawn): em pty.c, com os pseudoterminais do kernel */
