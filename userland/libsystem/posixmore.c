/*
 * mOS libSystem — o que faltava para o bash 5.3, o readline 8.3, o ncurses 6.6 e o libintl do
 * Homebrew (docs/porte-brew.md): regex POSIX, fnmatch, tsearch, caracteres largos, locale "C.UTF-8",
 * usuários/grupos, terminal e temporizadores. Só existe UTF-8; as funções de locale devolvem o C.
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
    static mac_lconv_t lc = {".", "", "", "", "", "", "", "", "", "",
                             127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127, 127};
    return &lc;
}
static int cx_locale_obj;
void *newlocale(int mask, const char *name, void *base) { (void)mask; (void)name; (void)base; return &cx_locale_obj; }
void freelocale(void *l) { (void)l; }
void *uselocale(void *l) { (void)l; return (void *)-1; }               /* LC_GLOBAL_LOCALE */
const char *querylocale(int mask, void *l) { (void)mask; (void)l; return "C"; }
/* nl_item do macOS: CODESET 0, D_T_FMT 1, D_FMT 2, T_FMT 3, RADIXCHAR 50, THOUSEP 51, YESEXPR 52, NOEXPR 53 */
char *nl_langinfo(int item) {
    switch (item) {
    case 0: return "UTF-8";
    case 1: return "%a %b %e %H:%M:%S %Y";
    case 2: return "%m/%d/%y";
    case 3: return "%H:%M:%S";
    case 50: return ".";
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
int getpgrp(void) { return getpid(); }
int tcsetpgrp(int fd, int pg) { (void)fd; (void)pg; return 0; }
char *ttyname(int fd) { return isatty(fd) ? "/dev/ttys000" : 0; }
unsigned long cfgetospeed(const void *t) { (void)t; return 38400; }
int getdtablesize(void) { return 32; }                 /* a tabela de fds do kernel tem 32 entradas */
unsigned alarm(unsigned s) { (void)s; return 0; }       /* sem SIGALRM ainda: o alarme nunca dispara */
int setitimer(int which, const void *nv, void *ov) { (void)which; (void)nv; if (ov) memset(ov, 0, 32); return 0; }
int getrusage(int who, void *ru) { (void)who; memset(ru, 0, 144); return 0; }
int faccessat(int fd, const char *path, int mode, int flag) {
    (void)flag;
    if (fd != -2 && path[0] != '/') { cx_errno = 9; return -1; }   /* só AT_FDCWD (-2) ou caminho absoluto */
    return access(path, mode);
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
