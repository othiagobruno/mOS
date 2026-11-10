/* iostream, filesystem, thread, future, to_chars e os globais cin/cout/cerr.
 * Incluído no fim de libcxx_openbot.c (mesmo escopo). */
int rmdir(const char *p);

static const void *vt_os[10], *vt_osw[10], *vt_is[10], *vt_of[10], *vt_if[10];
static const void *vt_ss[15], *vt_oss[10], *vt_iss[10];
const void *vtt_of[4] __asm__("_ZTTNSt3__114basic_ofstreamIcNS_11char_traitsIcEEEE");
const void *vtt_if[4] __asm__("_ZTTNSt3__114basic_ifstreamIcNS_11char_traitsIcEEEE");
const void *vtt_ss[10] __asm__("_ZTTNSt3__118basic_stringstreamIcNS_11char_traitsIcEENS_9allocatorIcEEEE");
const void *vtt_oss[4] __asm__("_ZTTNSt3__119basic_ostringstreamIcNS_11char_traitsIcEENS_9allocatorIcEEEE");
const void *vtt_iss[4] __asm__("_ZTTNSt3__119basic_istringstreamIcNS_11char_traitsIcEENS_9allocatorIcEEEE");
static char *ios_of(void *s) { void **vt = *(void ***)s; return (char *)s + ((long *)vt)[-3]; }
static unsigned *ios_st(void *ios) { return (unsigned *)((char *)ios + 32); }
static unsigned *ios_fl(void *ios) { return (unsigned *)((char *)ios + 8); }
static long *ios_prec(void *ios) { return (long *)((char *)ios + 16); }
static long *ios_width(void *ios) { return (long *)((char *)ios + 24); }
static void **ios_buf(void *ios) { return (void **)((char *)ios + 40); }
static void **ios_tie(void *ios) { return (void **)((char *)ios + 136); }
static int *ios_fill(void *ios) { return (int *)((char *)ios + 144); }

static void ios_d2_fn(void *ios);
static void os_flush(void *os);
static long sb_xsgetn(void *sb, void *p, long n) {
    void **vt = *(void ***)sb;
    return ((long (*)(void *, void *, long))vt[8])(sb, p, n);
}
static long sb_xsputn(void *sb, const void *p, long n) {
    void **vt = *(void ***)sb;
    return ((long (*)(void *, const void *, long))vt[12])(sb, p, n);
}
static int sb_sync(void *sb) { void **vt = *(void ***)sb; return ((int (*)(void *))vt[6])(sb); }
static int sb_under(void *sb) { void **vt = *(void ***)sb; return ((int (*)(void *))vt[9])(sb); }
static int sb_uflow(void *sb) { void **vt = *(void ***)sb; return ((int (*)(void *))vt[10])(sb); }
static int sb_pback(void *sb, int c) { void **vt = *(void ***)sb; return ((int (*)(void *, int))vt[11])(sb, c); }
static void sb_seekoff(ob_fpos *r, void *sb, long long off, int dir, unsigned mode) {
    void **vt = *(void ***)sb;
    ((void (*)(ob_fpos *, void *, long long, int, unsigned))vt[4])(r, sb, off, dir, mode);
}

void ios_init(void *ios, void *sb) __asm__("_ZNSt3__18ios_base4initEPv");
void ios_init(void *ios, void *sb) {
    memset((char *)ios + 8, 0, 128);
    *ios_fl(ios) = 0x1002;
    *ios_prec(ios) = 6;
    *ios_st(ios) = sb ? 0 : 1;
    *ios_buf(ios) = sb;
    ((void **)ios)[6] = cx_locale_classic_imp();
}
void ios_clear(void *ios, unsigned st) __asm__("_ZNSt3__18ios_base5clearEj");
void ios_clear(void *ios, unsigned st) {
    *ios_st(ios) = st;
    if (*((unsigned *)((char *)ios + 36)) & st) ob_fail("ios_base::failure");
}
void ios_setbad(void *ios) __asm__("_ZNSt3__18ios_base33__set_badbit_and_consider_rethrowEv");
void ios_setbad(void *ios) { ios_clear(ios, *ios_st(ios) | 1); }
void ios_getloc(void **ret, const void *ios) __asm__("_ZNKSt3__18ios_base6getlocEv");
void ios_getloc(void **ret, const void *ios) { *ret = ((void **)ios)[6]; }
void ios_imbue(void **ret, void *ios, void *const *loc) __asm__("_ZNSt3__18ios_base5imbueERKNS_6localeE");
void ios_imbue(void **ret, void *ios, void *const *loc) {
    void *sb;
    *ret = ((void **)ios)[6];
    ((void **)ios)[6] = *loc;
    sb = *ios_buf(ios);
    if (sb) { void **vt = *(void ***)sb; ((void (*)(void *, void *const *))vt[2])(sb, loc); }
}
static void ios_d2_fn(void *ios) {
    free(*(void **)((char *)ios + 88));
    free(*(void **)((char *)ios + 112));
    *(void **)((char *)ios + 88) = 0;
    *(void **)((char *)ios + 112) = 0;
}
void ios_d2(void *ios) __asm__("_ZNSt3__19basic_iosIcNS_11char_traitsIcEEED2Ev");
void ios_d2(void *ios) { ios_d2_fn(ios); }
void iosw_d2(void *ios) __asm__("_ZNSt3__19basic_iosIwNS_11char_traitsIwEEED2Ev");
void iosw_d2(void *ios) { ios_d2_fn(ios); }

/* ---- streambuf de base (o comportamento da classe, não um stub) ---- */
static void sb_imbue(void *sb, void *const *loc) { (void)loc; ((void **)sb)[1] = loc ? *loc : 0; }
static void *sb_setbuf(void *sb, void *p, long n) { (void)p; (void)n; return sb; }
static void sb_seek_fail(ob_fpos *r, void *sb, long long off, int dir, unsigned mode) {
    (void)sb; (void)off; (void)dir; (void)mode; memset(r, 0, sizeof *r); r->off = -1;
}
static void sb_seekpos_fail(ob_fpos *r, void *sb, ob_fpos *pos, unsigned mode) {
    (void)sb; (void)pos; (void)mode; memset(r, 0, sizeof *r); r->off = -1;
}
static int sb_sync0(void *sb) { (void)sb; return 0; }
static long sb_show0(void *sb) { (void)sb; return 0; }
static int sb_under_eof(void *sb) { (void)sb; return -1; }
static int sb_uflow_base(void *sb) {
    int c = sb_under(sb);
    char **n, **e;
    if (c == -1) return -1;
    n = (char **)((char *)sb + 24); e = (char **)((char *)sb + 32);
    if (*n && *n < *e) return (unsigned char)*(*n)++;
    return c;
}
static int sb_pback_eof(void *sb, int c) { (void)sb; (void)c; return -1; }
static long sb_xsgetn_base(void *sb, char *p, long n) {
    long i = 0;
    while (i < n) { int c = sb_uflow(sb); if (c == -1) break; p[i++] = (char)c; }
    return i;
}
static long sb_xsputn_base(void *sb, const char *p, long n) {
    void **vt = *(void ***)sb;
    long i = 0;
    while (i < n) {
        int c = ((int (*)(void *, int))vt[13])(sb, (unsigned char)p[i]);
        if (c == -1) break;
        i++;
    }
    return i;
}
static int sb_over_eof(void *sb, int c) { (void)sb; (void)c; return -1; }
static void sb_ctor_body(void *sb, const void *vt) { memset(sb, 0, 64); *(const void **)sb = vt; ((void **)sb)[1] = cx_locale_classic_imp(); }
static void sb_dtor_body(void *sb) { (void)sb; }

/* filebuf: fd numa caixa no lugar do FILE* (offset 120). always_noconv em 402. */
static int *fb_box(void *fb) { return *(int **)((char *)fb + 120); }
static int fb_getfd(void *fb) { int *b = fb_box(fb); return b ? *b : -1; }
static void fb_close_fd(void *fb) {
    int *b = fb_box(fb);
    if (!b) return;
    close(*b);
    free(b);
    *(void **)((char *)fb + 120) = 0;
    *(char **)((char *)fb + 16) = 0; *(char **)((char *)fb + 24) = 0; *(char **)((char *)fb + 32) = 0;
}
static int fb_under(void *fb) {
    int fd = fb_getfd(fb);
    char *buf, **n = (char **)((char *)fb + 24), **e = (char **)((char *)fb + 32);
    ssize_t k;
    if (*n && *n < *e) return (unsigned char)**n;
    if (fd < 0) return -1;
    buf = *(char **)((char *)fb + 64);
    if (!buf) {
        buf = (char *)malloc(4096);
        *(char **)((char *)fb + 64) = buf;
        *(size_t *)((char *)fb + 96) = 4096;
        ((unsigned char *)fb)[400] = 1;
    }
    k = read(fd, buf, 4096);
    if (k <= 0) return -1;
    *(char **)((char *)fb + 16) = buf;
    *n = buf; *e = buf + k;
    return (unsigned char)*buf;
}
static int fb_over(void *fb, int c) {
    int fd = fb_getfd(fb);
    unsigned char b;
    if (c == -1) return 0;
    if (fd < 0) return -1;
    b = (unsigned char)c;
    *(char **)((char *)fb + 16) = 0; *(char **)((char *)fb + 24) = 0; *(char **)((char *)fb + 32) = 0;
    return write(fd, &b, 1) == 1 ? c : -1;
}
static long fb_xsputn(void *fb, const char *p, long n) {
    int fd = fb_getfd(fb);
    long off = 0;
    if (fd < 0) return 0;
    while (off < n) { ssize_t w = write(fd, p + off, (size_t)(n - off)); if (w <= 0) break; off += w; }
    return off;
}
static int fb_pback(void *fb, int c) {
    char **n = (char **)((char *)fb + 24), **b = (char **)((char *)fb + 16);
    if (!*n || *n == *b) return -1;
    (*n)--;
    if (c != -1) **n = (char)c;
    return (unsigned char)**n;
}
static int fb_sync(void *fb) { return fb_getfd(fb) < 0 ? -1 : 0; }
static void fb_seekoff(ob_fpos *r, void *fb, long long off, int dir, unsigned mode) {
    int fd = fb_getfd(fb);
    off_t w;
    (void)mode;
    memset(r, 0, sizeof *r); r->off = -1;
    if (fd < 0) return;
    *(char **)((char *)fb + 16) = *(char **)((char *)fb + 24) = *(char **)((char *)fb + 32) = 0;
    w = lseek(fd, (off_t)off, dir == 0 ? 0 : dir == 2 ? 2 : 1);
    if (w >= 0) r->off = (long long)w;
}
static void fb_seekpos(ob_fpos *r, void *fb, ob_fpos *pos, unsigned mode) { fb_seekoff(r, fb, pos ? pos->off : 0, 0, mode); }
static void fb_d1(void *fb);
static void fb_d0(void *fb) { fb_d1(fb); free(fb); }
static const void *vt_filebuf[16];
static void fb_ctor_body(void *fb) {
    memset(fb, 0, 408);
    *(const void **)fb = &vt_filebuf[2];
    ((void **)fb)[1] = cx_locale_classic_imp();
    ((unsigned char *)fb)[402] = 1;
}
static void fb_d1(void *fb) {
    char *ext = *(char **)((char *)fb + 64);
    fb_close_fd(fb);
    if (((unsigned char *)fb)[400] && ext) free(ext);
    *(char **)((char *)fb + 64) = 0;
}
void *fb_open(void *fb, const char *path, unsigned mode) __asm__("_ZNSt3__113basic_filebufIcNS_11char_traitsIcEEE4openEPKcj");
void *fb_open(void *fb, const char *path, unsigned mode) {
    int in = (mode & 8) != 0, out = (mode & 16) != 0, app = (mode & 1) != 0, trunc = (mode & 32) != 0;
    int fl = in && out ? MAC_O_RDWR : out ? MAC_O_WRONLY : 0;
    int fd, *box;
    if (out && !in && !app) trunc = 1;
    if (trunc) fl |= MAC_O_TRUNC;
    if (app) fl |= MAC_O_APPEND;
    if (out || trunc) fl |= MAC_O_CREAT;
    fb_close_fd(fb);
    fd = open(path, fl, 0644);
    if (fd < 0) return 0;
    if ((mode & 2) || app) lseek(fd, 0, 2);
    box = (int *)malloc(sizeof(int));
    *box = fd;
    *(int **)((char *)fb + 120) = box;
    *(unsigned *)((char *)fb + 392) = mode;
    *(unsigned *)((char *)fb + 396) = mode;
    return fb;
}
void *fb_close(void *fb) __asm__("_ZNSt3__113basic_filebufIcNS_11char_traitsIcEEE5closeEv");
void *fb_close(void *fb) { if (!fb_box(fb)) return 0; fb_close_fd(fb); return fb; }
void fb_c1(void *fb) __asm__("_ZNSt3__113basic_filebufIcNS_11char_traitsIcEEEC1Ev");
void fb_c1(void *fb) { fb_ctor_body(fb); }
void fb_d1_ex(void *fb) __asm__("_ZNSt3__113basic_filebufIcNS_11char_traitsIcEEED1Ev");
void fb_d1_ex(void *fb) { fb_d1(fb); }

/* stringbuf */
static ob_str *sbuf_str(void *sb) { return (ob_str *)((char *)sb + 64); }
static void sbuf_rebase(void *sb, char *old) {
    ob_str *s = sbuf_str(sb);
    char *d = ob_sdata(s);
    size_t n = ob_ssize(s), cap = ob_scap(s);
    unsigned mode = *(unsigned *)((char *)sb + 96);
    char **g0 = (char **)((char *)sb + 16), **g1 = (char **)((char *)sb + 24), **g2 = (char **)((char *)sb + 32);
    char **p0 = (char **)((char *)sb + 40), **p1 = (char **)((char *)sb + 48), **p2 = (char **)((char *)sb + 56);
    long dlt = d - old;
    if (mode & 8) {
        if (!*g0) { *g0 = d; *g1 = d; *g2 = d + n; }
        else { *g0 += dlt; *g1 += dlt; *g2 = d + n; }
    }
    if (mode & 16) {
        if (!*p0) { *p0 = d; *p1 = d + n; *p2 = d + cap; }
        else { *p0 += dlt; *p1 += dlt; *p2 = d + cap; }
    }
    *(char **)((char *)sb + 88) = d + n;
}
static int sbuf_under(void *sb) {
    char **n = (char **)((char *)sb + 24), **e = (char **)((char *)sb + 32);
    if (!*(char **)((char *)sb + 16)) sbuf_rebase(sb, ob_sdata(sbuf_str(sb)));
    if (*n && *n < *e) return (unsigned char)**n;
    return -1;
}
static int sbuf_over(void *sb, int c) {
    ob_str *s = sbuf_str(sb);
    char *old = ob_sdata(s);
    if (c != -1) { char ch = (char)c; ob_sappend_n(s, &ch, 1); }
    sbuf_rebase(sb, old);
    return c == -1 ? 0 : c;
}
static int sbuf_pback(void *sb, int c) {
    char **n = (char **)((char *)sb + 24), **b = (char **)((char *)sb + 16);
    if (!*n || *n == *b) return -1;
    (*n)--;
    if (c != -1) **n = (char)c;
    return (unsigned char)**n;
}
static void sbuf_seekoff(ob_fpos *r, void *sb, long long off, int dir, unsigned mode) {
    ob_str *s = sbuf_str(sb);
    size_t n = ob_ssize(s);
    long long base, np;
    memset(r, 0, sizeof *r); r->off = -1;
    if (!*(char **)((char *)sb + 16) && !*(char **)((char *)sb + 40)) sbuf_rebase(sb, ob_sdata(sbuf_str(sb)));
    if (dir == 0) base = 0;
    else if (dir == 2) base = (long long)n;
    else {
        char *cur = (mode & 16) ? *(char **)((char *)sb + 48) : *(char **)((char *)sb + 24);
        base = cur ? cur - ob_sdata(s) : 0;
    }
    np = base + off;
    if (np < 0 || (size_t)np > n) return;
    if (mode & 8) *(char **)((char *)sb + 24) = ob_sdata(s) + np;
    if (mode & 16) *(char **)((char *)sb + 48) = ob_sdata(s) + np;
    r->off = np;
}
static void sbuf_seekpos(ob_fpos *r, void *sb, ob_fpos *pos, unsigned mode) { sbuf_seekoff(r, sb, pos ? pos->off : 0, 0, mode); }
static void sbuf_d1(void *sb) { ob_sfree(sbuf_str(sb)); }
static void sbuf_d0(void *sb) { sbuf_d1(sb); free(sb); }
/* vt_sbuf_arr é a vtable real */
void sbuf_str_get(ob_str *ret, void *sb) __asm__("_ZNKSt3__115basic_stringbufIcNS_11char_traitsIcEENS_9allocatorIcEEE3strEv");
void sbuf_str_get(ob_str *ret, void *sb) { ob_scopy(ret, sbuf_str(sb)); }

/* fd streambuf de cin/cout/cerr: 64 bytes padrão + int fd */
static int fd_over(void *sb, int c) {
    int fd = *(int *)((char *)sb + 64);
    unsigned char b;
    if (c == -1) return 0;
    b = (unsigned char)c;
    return write(fd, &b, 1) == 1 ? c : -1;
}
static long fd_putn(void *sb, const char *p, long n) {
    int fd = *(int *)((char *)sb + 64);
    long off = 0;
    while (off < n) { ssize_t w = write(fd, p + off, (size_t)(n - off)); if (w <= 0) break; off += w; }
    return off;
}
static int fd_under(void *sb) {
    int fd = *(int *)((char *)sb + 64);
    unsigned char b;
    return read(fd, &b, 1) == 1 ? b : -1;
}
static long fd_getn(void *sb, char *p, long n) {
    long i = 0;
    while (i < n) { int c = fd_under(sb); if (c < 0) break; p[i++] = (char)c; }
    return i;
}

static void os_d1(void *o);
static void os_d0(void *o);
static void os_d2(void *o, void **vtt);
static void is_d1(void *o);
static void is_d0(void *o);
static void is_d2(void *o, void **vtt);
static void iosm_d1(void *o);
static void iosm_d0(void *o);
static void iosm_d2(void *o, void **vtt);
static void of_d1(void *o);
static void of_d0(void *o);
static void of_d2(void *o, void **vtt);
static void if_d1(void *o);
static void if_d0(void *o);
static void if_d2(void *o, void **vtt);
static void ss_d1(void *o);
static void ss_d0(void *o);
static void ss_d2(void *o, void **vtt);
static void oss_d1(void *o);
static void oss_d2(void *o, void **vtt);
static void iss_d1(void *o);
static void iss_d2(void *o, void **vtt);
static void os_thunk(void *ios);
static void is_thunk(void *ios);
static void iosm_thunk(void *ios);
static void iosm_nv(void *p);
static void osw_d1(void *o);

static const void *vt_sb[16] = {
    0, (void *)&ti_sb, (void *)sb_dtor_body, (void *)sb_dtor_body, (void *)sb_imbue, (void *)sb_setbuf,
    (void *)sb_seek_fail, (void *)sb_seekpos_fail, (void *)sb_sync0, (void *)sb_show0, (void *)sb_xsgetn_base,
    (void *)sb_under_eof, (void *)sb_uflow_base, (void *)sb_pback_eof, (void *)sb_xsputn_base, (void *)sb_over_eof
};
static const void *vt_sbw[16] = {
    0, (void *)&ti_sbw, (void *)sb_dtor_body, (void *)sb_dtor_body, (void *)sb_imbue, (void *)sb_setbuf,
    (void *)sb_seek_fail, (void *)sb_seekpos_fail, (void *)sb_sync0, (void *)sb_show0, (void *)sb_xsgetn_base,
    (void *)sb_under_eof, (void *)sb_uflow_base, (void *)sb_pback_eof, (void *)sb_xsputn_base, (void *)sb_over_eof
};
static const void *vt_fd[16] = {
    0, (void *)&ti_sb, (void *)sb_dtor_body, (void *)sb_dtor_body, (void *)sb_imbue, (void *)sb_setbuf,
    (void *)sb_seek_fail, (void *)sb_seekpos_fail, (void *)sb_sync0, (void *)sb_show0, (void *)fd_getn,
    (void *)fd_under, (void *)fd_under, (void *)sb_pback_eof, (void *)fd_putn, (void *)fd_over
};
static const void *vt_filebuf[16] = {
    0, (void *)&ti_sb, (void *)fb_d1, (void *)fb_d0, (void *)sb_imbue, (void *)sb_setbuf,
    (void *)fb_seekoff, (void *)fb_seekpos, (void *)fb_sync, (void *)sb_show0, (void *)sb_xsgetn_base,
    (void *)fb_under, (void *)sb_uflow_base, (void *)fb_pback, (void *)fb_xsputn, (void *)fb_over
};
static const void *vt_sbuf_arr[16] __attribute__((used)) = {
    0, (void *)&ti_sb, (void *)sbuf_d1, (void *)sbuf_d0, (void *)sb_imbue, (void *)sb_setbuf,
    (void *)sbuf_seekoff, (void *)sbuf_seekpos, (void *)sb_sync0, (void *)sb_show0, (void *)sb_xsgetn_base,
    (void *)sbuf_under, (void *)sb_uflow_base, (void *)sbuf_pback, (void *)sb_xsputn_base, (void *)sbuf_over
};
/* vt_sbuf_arr é a vtable real */

static void os_d2(void *o, void **vtt) {
    void *p = vtt[0];
    long off = ((long *)p)[-3];
    *(void **)o = p;
    *(void **)((char *)o + off) = vtt[1];
}
static void os_d1(void *o) {
    *(void **)o = (void *)&vt_os[3];
    *(void **)((char *)o + 8) = (void *)&vt_os[8];
    ios_d2_fn((char *)o + 8);
}
static void os_d0(void *o) { os_d1(o); free(o); }
static void os_thunk(void *ios) { void **vt = *(void ***)ios; os_d1((char *)ios + ((long *)vt)[-3]); }
static void osw_d1(void *o) {
    *(void **)o = (void *)&vt_osw[3];
    *(void **)((char *)o + 8) = (void *)&vt_osw[8];
    ios_d2_fn((char *)o + 8);
}
static void is_d2(void *o, void **vtt) { os_d2(o, vtt); }
static void is_d1(void *o) {
    *(void **)o = (void *)&vt_is[3];
    *(void **)((char *)o + 16) = (void *)&vt_is[8];
    ios_d2_fn((char *)o + 16);
}
static void is_d0(void *o) { is_d1(o); free(o); }
static void is_thunk(void *ios) { void **vt = *(void ***)ios; is_d1((char *)ios + ((long *)vt)[-3]); }
static void iosm_d2(void *o, void **vtt) {
    void *p = vtt[0];
    long off = ((long *)p)[-3];
    *(void **)o = p;
    *(void **)((char *)o + off) = vtt[5];
    *(void **)((char *)o + 16) = vtt[6];
}
static void iosm_d1(void *o) { ios_d2_fn((char *)o + 24); }
static void iosm_d0(void *o) { iosm_d1(o); free(o); }
static void iosm_thunk(void *ios) { void **vt = *(void ***)ios; iosm_d1((char *)ios + ((long *)vt)[-3]); }
static void iosm_nv(void *p) { iosm_d1((char *)p - 16); }

static void of_d2(void *o, void **vtt) {
    void *p = vtt[0];
    long off = ((long *)p)[-3];
    *(void **)o = p;
    *(void **)((char *)o + off) = vtt[3];
    fb_d1((char *)o + 8);
    os_d2(o, vtt + 1);
}
static void of_d1(void *o) { of_d2(o, (void **)vtt_of); ios_d2_fn((char *)o + 416); }
static void of_d0(void *o) { of_d1(o); free(o); }
static void of_thunk(void *ios) { void **vt = *(void ***)ios; of_d1((char *)ios + ((long *)vt)[-3]); }
static void if_d2(void *o, void **vtt) {
    void *p = vtt[0];
    long off = ((long *)p)[-3];
    *(void **)o = p;
    *(void **)((char *)o + off) = vtt[3];
    fb_d1((char *)o + 16);
    is_d2(o, vtt + 1);
}
static void if_d1(void *o) { if_d2(o, (void **)vtt_if); ios_d2_fn((char *)o + 424); }
static void if_d0(void *o) { if_d1(o); free(o); }
static void if_thunk(void *ios) { void **vt = *(void ***)ios; if_d1((char *)ios + ((long *)vt)[-3]); }
static void ss_d2(void *o, void **vtt) {
    void *p = vtt[0];
    long off = ((long *)p)[-3];
    *(void **)o = p;
    *(void **)((char *)o + off) = vtt[8];
    *(void **)((char *)o + 16) = vtt[9];
    sbuf_d1((char *)o + 24);
    iosm_d2(o, vtt + 1);
}
static void ss_d1(void *o) { ss_d2(o, (void **)vtt_ss); ios_d2_fn((char *)o + 128); }
static void ss_d0(void *o) { ss_d1(o); free(o); }
static void oss_d2(void *o, void **vtt) {
    void *p = vtt[0];
    long off = ((long *)p)[-3];
    *(void **)o = p;
    *(void **)((char *)o + off) = vtt[3];
    sbuf_d1((char *)o + 8);
    os_d2(o, vtt + 1);
}
static void oss_d1(void *o) { oss_d2(o, (void **)vtt_oss); ios_d2_fn((char *)o + 112); }
static void iss_d2(void *o, void **vtt) {
    void *p = vtt[0];
    long off = ((long *)p)[-3];
    *(void **)o = p;
    *(void **)((char *)o + off) = vtt[3];
    sbuf_d1((char *)o + 16);
    is_d2(o, vtt + 1);
}
static void iss_d1(void *o) { iss_d2(o, (void **)vtt_iss); ios_d2_fn((char *)o + 120); }

/* vtables: [vbase, top, rtti, D1, D0 | vcall, top, rtti, thunk D1, thunk D0] */
static const void *vt_os[10] __attribute__((used)) = {
    (void *)8, 0, (void *)&ti_os, (void *)os_d1, (void *)os_d0,
    (void *)-8, (void *)-8, (void *)&ti_os, (void *)os_thunk, (void *)os_thunk
};
static const void *vt_osw[10] __attribute__((used)) = {
    (void *)8, 0, (void *)&ti_os, (void *)osw_d1, (void *)osw_d1,
    (void *)-8, (void *)-8, (void *)&ti_os, (void *)os_thunk, (void *)os_thunk
};
static const void *vt_is[10] __attribute__((used)) = {
    (void *)16, 0, (void *)&ti_is, (void *)is_d1, (void *)is_d0,
    (void *)-16, (void *)-16, (void *)&ti_is, (void *)is_thunk, (void *)is_thunk
};
static const void *vt_of[10] __attribute__((used)) = {
    (void *)416, 0, (void *)&ti_os, (void *)of_d1, (void *)of_d0,
    (void *)-416, (void *)-416, (void *)&ti_os, (void *)of_thunk, (void *)of_thunk
};
static const void *vt_if[10] __attribute__((used)) = {
    (void *)424, 0, (void *)&ti_is, (void *)if_d1, (void *)if_d0,
    (void *)-424, (void *)-424, (void *)&ti_is, (void *)if_thunk, (void *)if_thunk
};
/* stringstream: 3 componentes de 5. P=&[3] O=&[8] I=&[13] */
static const void *vt_ss[15] __attribute__((used)) = {
    (void *)128, 0, (void *)&ti_iosm, (void *)ss_d1, (void *)ss_d0,
    (void *)112, (void *)-16, (void *)&ti_iosm, (void *)iosm_nv, (void *)iosm_nv,
    (void *)-128, (void *)-128, (void *)&ti_iosm, (void *)iosm_thunk, (void *)iosm_thunk
};
static const void *vt_oss[10] __attribute__((used)) = {
    (void *)112, 0, (void *)&ti_os, (void *)oss_d1, (void *)oss_d1,
    (void *)-112, (void *)-112, (void *)&ti_os, (void *)os_thunk, (void *)os_thunk
};
static const void *vt_iss[10] __attribute__((used)) = {
    (void *)120, 0, (void *)&ti_is, (void *)iss_d1, (void *)iss_d1,
    (void *)-120, (void *)-120, (void *)&ti_is, (void *)is_thunk, (void *)is_thunk
};
static const void *vt_bios[4] __attribute__((used)) = { 0, (void *)&ti_os, (void *)ios_d2, (void *)ios_d2 };
void length_d2(void *e) __asm__("_ZNSt12length_errorD2Ev");
void length_d2(void *e) { (void)e; }
void oor_d2(void *e) __asm__("_ZNSt12out_of_rangeD2Ev");
void oor_d2(void *e) { (void)e; }
const void *vtt_of[4] = {
    (void *)&vt_of[3], (void *)&vt_of[3], (void *)&vt_of[8], (void *)&vt_of[8]
};
const void *vtt_if[4] = {
    (void *)&vt_if[3], (void *)&vt_if[3], (void *)&vt_if[8], (void *)&vt_if[8]
};
const void *vtt_ss[10] = {
    (void *)&vt_ss[3], (void *)&vt_ss[3], (void *)&vt_ss[3], (void *)&vt_ss[13], (void *)&vt_ss[8],
    (void *)&vt_ss[13], (void *)&vt_ss[13], (void *)&vt_ss[8], (void *)&vt_ss[13], (void *)&vt_ss[8]
};
const void *vtt_oss[4] = {
    (void *)&vt_oss[3], (void *)&vt_oss[3], (void *)&vt_oss[8], (void *)&vt_oss[8]
};
const void *vtt_iss[4] = {
    (void *)&vt_iss[3], (void *)&vt_iss[3], (void *)&vt_iss[8], (void *)&vt_iss[8]
};

/* As vtables acima são as exportadas. Os ponteiros internos usam os mesmos símbolos. */
/* vt_sbuf_arr é a vtable real */

void sb_c2(void *sb) __asm__("_ZNSt3__115basic_streambufIcNS_11char_traitsIcEEEC2Ev");
void sb_c2(void *sb) { sb_ctor_body(sb, &vt_sb[2]); }
void sb_d2(void *sb) __asm__("_ZNSt3__115basic_streambufIcNS_11char_traitsIcEEED2Ev");
void sb_d2(void *sb) { sb_dtor_body(sb); }
void sb_imbue_ex(void *sb, void *const *loc) __asm__("_ZNSt3__115basic_streambufIcNS_11char_traitsIcEEE5imbueERKNS_6localeE");
void sb_imbue_ex(void *sb, void *const *loc) { sb_imbue(sb, loc); }
void *sb_setbuf_ex(void *sb, char *p, long n) __asm__("_ZNSt3__115basic_streambufIcNS_11char_traitsIcEEE6setbufEPcl");
void *sb_setbuf_ex(void *sb, char *p, long n) { return sb_setbuf(sb, p, n); }
void sb_seekoff_ex(ob_fpos *r, void *sb, long long off, int dir, unsigned mode) __asm__("_ZNSt3__115basic_streambufIcNS_11char_traitsIcEEE7seekoffExNS_8ios_base7seekdirEj");
void sb_seekoff_ex(ob_fpos *r, void *sb, long long off, int dir, unsigned mode) { sb_seek_fail(r, sb, off, dir, mode); }
void sb_seekpos_ex(ob_fpos *r, void *sb, ob_fpos *pos, unsigned mode) __asm__("_ZNSt3__115basic_streambufIcNS_11char_traitsIcEEE7seekposENS_4fposI11__mbstate_tEEj");
void sb_seekpos_ex(ob_fpos *r, void *sb, ob_fpos *pos, unsigned mode) { sb_seekpos_fail(r, sb, pos, mode); }
int sb_sync_ex(void *sb) __asm__("_ZNSt3__115basic_streambufIcNS_11char_traitsIcEEE4syncEv");
int sb_sync_ex(void *sb) { return sb_sync0(sb); }
long sb_show_ex(void *sb) __asm__("_ZNSt3__115basic_streambufIcNS_11char_traitsIcEEE9showmanycEv");
long sb_show_ex(void *sb) { return sb_show0(sb); }
long sb_xsgetn_ex(void *sb, char *p, long n) __asm__("_ZNSt3__115basic_streambufIcNS_11char_traitsIcEEE6xsgetnEPcl");
long sb_xsgetn_ex(void *sb, char *p, long n) { return sb_xsgetn_base(sb, p, n); }
int sb_under_ex(void *sb) __asm__("_ZNSt3__115basic_streambufIcNS_11char_traitsIcEEE9underflowEv");
int sb_under_ex(void *sb) { return sb_under_eof(sb); }
int sb_uflow_ex(void *sb) __asm__("_ZNSt3__115basic_streambufIcNS_11char_traitsIcEEE5uflowEv");
int sb_uflow_ex(void *sb) { return sb_uflow_base(sb); }
int sb_pback_ex(void *sb, int c) __asm__("_ZNSt3__115basic_streambufIcNS_11char_traitsIcEEE9pbackfailEi");
int sb_pback_ex(void *sb, int c) { return sb_pback_eof(sb, c); }
long sb_xsputn_ex(void *sb, const char *p, long n) __asm__("_ZNSt3__115basic_streambufIcNS_11char_traitsIcEEE6xsputnEPKcl");
long sb_xsputn_ex(void *sb, const char *p, long n) { return sb_xsputn_base(sb, p, n); }
int sb_over_ex(void *sb, int c) __asm__("_ZNSt3__115basic_streambufIcNS_11char_traitsIcEEE8overflowEi");
int sb_over_ex(void *sb, int c) { return sb_over_eof(sb, c); }

void sbw_c2(void *sb) __asm__("_ZNSt3__115basic_streambufIwNS_11char_traitsIwEEEC2Ev");
void sbw_c2(void *sb) { sb_ctor_body(sb, &vt_sbw[2]); }
void sbw_d2(void *sb) __asm__("_ZNSt3__115basic_streambufIwNS_11char_traitsIwEEED2Ev");
void sbw_d2(void *sb) { sb_dtor_body(sb); }
void sbw_imbue(void *sb, void *const *loc) __asm__("_ZNSt3__115basic_streambufIwNS_11char_traitsIwEEE5imbueERKNS_6localeE");
void sbw_imbue(void *sb, void *const *loc) { sb_imbue(sb, loc); }
void *sbw_setbuf(void *sb, uint32_t *p, long n) __asm__("_ZNSt3__115basic_streambufIwNS_11char_traitsIwEEE6setbufEPwl");
void *sbw_setbuf(void *sb, uint32_t *p, long n) { return sb_setbuf(sb, p, n); }
long sbw_xsgetn(void *sb, uint32_t *p, long n) __asm__("_ZNSt3__115basic_streambufIwNS_11char_traitsIwEEE6xsgetnEPwl");
long sbw_xsgetn(void *sb, uint32_t *p, long n) { (void)p; (void)n; (void)sb; return 0; }
void sbw_seekoff(ob_fpos *r, void *sb, long long off, int dir, unsigned mode) __asm__("_ZNSt3__115basic_streambufIwNS_11char_traitsIwEEE7seekoffExNS_8ios_base7seekdirEj");
void sbw_seekoff(ob_fpos *r, void *sb, long long off, int dir, unsigned mode) { sb_seek_fail(r, sb, off, dir, mode); }
void sbw_seekpos(ob_fpos *r, void *sb, ob_fpos *pos, unsigned mode) __asm__("_ZNSt3__115basic_streambufIwNS_11char_traitsIwEEE7seekposENS_4fposI11__mbstate_tEEj");
void sbw_seekpos(ob_fpos *r, void *sb, ob_fpos *pos, unsigned mode) { sb_seekpos_fail(r, sb, pos, mode); }
int sbw_uflow(void *sb) __asm__("_ZNSt3__115basic_streambufIwNS_11char_traitsIwEEE5uflowEv");
int sbw_uflow(void *sb) { (void)sb; return -1; }
int sbw_pback(void *sb, int c) __asm__("_ZNSt3__115basic_streambufIwNS_11char_traitsIwEEE9pbackfailEi");
int sbw_pback(void *sb, int c) { (void)sb; (void)c; return -1; }
long sbw_show(void *sb) __asm__("_ZNSt3__115basic_streambufIwNS_11char_traitsIwEEE9showmanycEv");
long sbw_show(void *sb) { (void)sb; return 0; }
int sbw_under(void *sb) __asm__("_ZNSt3__115basic_streambufIwNS_11char_traitsIwEEE9underflowEv");
int sbw_under(void *sb) { (void)sb; return -1; }

/* stringbuf ctor é chamado pelo construtor inline do stringstream (modo 24 = in|out). */
void sbuf_c1(void *sb, unsigned mode) __asm__("_ZNSt3__115basic_stringbufIcNS_11char_traitsIcEENS_9allocatorIcEEEC1Ej");
void sbuf_c1(void *sb, unsigned mode) {
    memset(sb, 0, 104);
    *(const void **)sb = &vt_sbuf_arr[2];
    ((void **)sb)[1] = cx_locale_classic_imp();
    *(unsigned *)((char *)sb + 96) = mode;
}

void os_d1_ex(void *o) __asm__("_ZNSt3__113basic_ostreamIcNS_11char_traitsIcEEED1Ev");
void os_d1_ex(void *o) { os_d1(o); }
void os_d0_ex(void *o) __asm__("_ZNSt3__113basic_ostreamIcNS_11char_traitsIcEEED0Ev");
void os_d0_ex(void *o) { os_d0(o); }
void os_d2_ex(void *o, void **vtt) __asm__("_ZNSt3__113basic_ostreamIcNS_11char_traitsIcEEED2Ev");
void os_d2_ex(void *o, void **vtt) { os_d2(o, vtt); }
void os_thunk_d1(void *p) __asm__("_ZTv0_n24_NSt3__113basic_ostreamIcNS_11char_traitsIcEEED1Ev");
void os_thunk_d1(void *p) { os_thunk(p); }
void os_thunk_d0(void *p) __asm__("_ZTv0_n24_NSt3__113basic_ostreamIcNS_11char_traitsIcEEED0Ev");
void os_thunk_d0(void *p) { os_thunk(p); }
void osw_d1_ex(void *o) __asm__("_ZNSt3__113basic_ostreamIwNS_11char_traitsIwEEED1Ev");
void osw_d1_ex(void *o) { osw_d1(o); }
void is_d1_ex(void *o) __asm__("_ZNSt3__113basic_istreamIcNS_11char_traitsIcEEED1Ev");
void is_d1_ex(void *o) { is_d1(o); }
void is_d0_ex(void *o) __asm__("_ZNSt3__113basic_istreamIcNS_11char_traitsIcEEED0Ev");
void is_d0_ex(void *o) { is_d0(o); }
void is_d2_ex(void *o, void **vtt) __asm__("_ZNSt3__113basic_istreamIcNS_11char_traitsIcEEED2Ev");
void is_d2_ex(void *o, void **vtt) { is_d2(o, vtt); }
void is_thunk_d1(void *p) __asm__("_ZTv0_n24_NSt3__113basic_istreamIcNS_11char_traitsIcEEED1Ev");
void is_thunk_d1(void *p) { is_thunk(p); }
void is_thunk_d0(void *p) __asm__("_ZTv0_n24_NSt3__113basic_istreamIcNS_11char_traitsIcEEED0Ev");
void is_thunk_d0(void *p) { is_thunk(p); }
void iosm_d1_ex(void *o) __asm__("_ZNSt3__114basic_iostreamIcNS_11char_traitsIcEEED1Ev");
void iosm_d1_ex(void *o) { iosm_d1(o); }
void iosm_d0_ex(void *o) __asm__("_ZNSt3__114basic_iostreamIcNS_11char_traitsIcEEED0Ev");
void iosm_d0_ex(void *o) { iosm_d0(o); }
void iosm_d2_ex(void *o, void **vtt) __asm__("_ZNSt3__114basic_iostreamIcNS_11char_traitsIcEEED2Ev");
void iosm_d2_ex(void *o, void **vtt) { iosm_d2(o, vtt); }
void iosm_thunk_d1(void *p) __asm__("_ZTv0_n24_NSt3__114basic_iostreamIcNS_11char_traitsIcEEED1Ev");
void iosm_thunk_d1(void *p) { iosm_thunk(p); }
void iosm_thunk_d0(void *p) __asm__("_ZTv0_n24_NSt3__114basic_iostreamIcNS_11char_traitsIcEEED0Ev");
void iosm_thunk_d0(void *p) { iosm_thunk(p); }
void iosm_nv_d1(void *p) __asm__("_ZThn16_NSt3__114basic_iostreamIcNS_11char_traitsIcEEED1Ev");
void iosm_nv_d1(void *p) { iosm_nv(p); }
void iosm_nv_d0(void *p) __asm__("_ZThn16_NSt3__114basic_iostreamIcNS_11char_traitsIcEEED0Ev");
void iosm_nv_d0(void *p) { iosm_nv(p); }

/* o ifstream/ofstream/stringstream não exportam D1; o VTT e a vtable apontam para eles. */

static void os_fail(void *os) { ios_clear(ios_of(os), *ios_st(ios_of(os)) | 4); }
static void os_flush(void *os) {
    void *sb = *ios_buf(ios_of(os));
    if (!sb || sb_sync(sb)) ios_setbad(ios_of(os));
}
typedef struct { unsigned char ok; unsigned char pad[7]; void *os; } ob_sent;
static void sent_c(ob_sent *s, void *os) {
    char *ios = ios_of(os);
    void *tie;
    s->os = os;
    if (*ios_st(ios) & 5) { s->ok = 0; os_fail(os); return; }
    tie = *ios_tie(ios);
    if (tie) os_flush(tie);
    s->ok = 1;
}
static void sent_d(ob_sent *s) { if (s->ok && (*ios_fl(ios_of(s->os)) & 0x2000)) os_flush(s->os); }
void sent_c1(ob_sent *s, void *os) __asm__("_ZNSt3__113basic_ostreamIcNS_11char_traitsIcEEE6sentryC1ERS3_");
void sent_c1(ob_sent *s, void *os) { sent_c(s, os); }
void sent_c2(ob_sent *s, void *os) __asm__("_ZNSt3__113basic_ostreamIcNS_11char_traitsIcEEE6sentryC2ERS3_");
void sent_c2(ob_sent *s, void *os) { sent_c(s, os); }
void sent_d1(ob_sent *s) __asm__("_ZNSt3__113basic_ostreamIcNS_11char_traitsIcEEE6sentryD1Ev");
void sent_d1(ob_sent *s) { sent_d(s); }
void sent_d2(ob_sent *s) __asm__("_ZNSt3__113basic_ostreamIcNS_11char_traitsIcEEE6sentryD2Ev");
void sent_d2(ob_sent *s) { sent_d(s); }
void sentw_c1(ob_sent *s, void *os) __asm__("_ZNSt3__113basic_ostreamIwNS_11char_traitsIwEEE6sentryC1ERS3_");
void sentw_c1(ob_sent *s, void *os) { sent_c(s, os); }
void sentw_c2(ob_sent *s, void *os) __asm__("_ZNSt3__113basic_ostreamIwNS_11char_traitsIwEEE6sentryC2ERS3_");
void sentw_c2(ob_sent *s, void *os) { sent_c(s, os); }
void sentw_d1(ob_sent *s) __asm__("_ZNSt3__113basic_ostreamIwNS_11char_traitsIwEEE6sentryD1Ev");
void sentw_d1(ob_sent *s) { sent_d(s); }
void sentw_d2(ob_sent *s) __asm__("_ZNSt3__113basic_ostreamIwNS_11char_traitsIwEEE6sentryD2Ev");
void sentw_d2(ob_sent *s) { sent_d(s); }

static void sput_raw(void *os, const char *p, long n) {
    void *sb = *ios_buf(ios_of(os));
    long w;
    if (!sb) { ios_setbad(ios_of(os)); return; }
    w = sb_xsputn(sb, p, n);
    if (w != n) ios_setbad(ios_of(os));
}
static void sput_pad(void *os, const char *s, long n, int after_sign) {
    char *ios = ios_of(os);
    long w = *ios_width(ios), pad = w > n ? w - n : 0, i;
    unsigned fl = *ios_fl(ios);
    int fill = *ios_fill(ios);
    char fb[8];
    if (fill == -1 || fill == 0) fill = ' ';
    *ios_width(ios) = 0;
    if (pad && (fl & 0x10) && after_sign) {
        sput_raw(os, s, after_sign);
        for (i = 0; i < pad; i++) { fb[0] = (char)fill; sput_raw(os, fb, 1); }
        sput_raw(os, s + after_sign, n - after_sign);
    } else if (pad && (fl & 0x20)) {
        sput_raw(os, s, n);
        for (i = 0; i < pad; i++) { fb[0] = (char)fill; sput_raw(os, fb, 1); }
    } else {
        for (i = 0; i < pad; i++) { fb[0] = (char)fill; sput_raw(os, fb, 1); }
        sput_raw(os, s, n);
    }
}
static void fmt_u(char *b, int *n, unsigned long long v, int base, int upper) {
    char tmp[72];
    int k = 0;
    const char *dig = upper ? "0123456789ABCDEF" : "0123456789abcdef";
    if (!v) tmp[k++] = '0';
    while (v) { tmp[k++] = dig[v % (unsigned)base]; v /= (unsigned)base; }
    while (k) b[(*n)++] = tmp[--k];
}
static void *os_int(void *os, unsigned long long v, int neg, int bits_signed) {
    ob_sent s;
    char b[80];
    int n = 0, base = 10, sign = 0;
    unsigned fl;
    sent_c(&s, os);
    if (!s.ok) return os;
    fl = *ios_fl(ios_of(os));
    if (fl & 0x40) base = 8;
    else if (fl & 0x8) base = 16;
    if (bits_signed && neg) { b[n++] = '-'; sign = 1; }
    else if (bits_signed && (fl & 0x800)) { b[n++] = '+'; sign = 1; }
    if ((fl & 0x200) && base == 16) { b[n++] = '0'; b[n++] = (fl & 0x4000) ? 'X' : 'x'; sign = n; }
    else if ((fl & 0x200) && base == 8 && v) { b[n++] = '0'; sign = n; }
    fmt_u(b, &n, v, base, (fl & 0x4000) != 0);
    sput_pad(os, b, n, sign);
    sent_d(&s);
    return os;
}
static void *os_bool(void *os, int v) {
    ob_sent s;
    sent_c(&s, os);
    if (!s.ok) return os;
    if (*ios_fl(ios_of(os)) & 1) sput_pad(os, v ? "true" : "false", v ? 4 : 5, 0);
    else sput_pad(os, v ? "1" : "0", 1, 0);
    sent_d(&s);
    return os;
}
static void *os_float(void *os, double v) {
    ob_sent s;
    char b[128];
    ob_tcr t;
    unsigned fl;
    int fmt = 3, prec, has = 1;
    sent_c(&s, os);
    if (!s.ok) return os;
    fl = *ios_fl(ios_of(os));
    prec = (int)*ios_prec(ios_of(os));
    if (fl & 0x4) fmt = 2;
    else if (fl & 0x100) fmt = 1;
    t = ob_to_chars_f(b, b + sizeof b, v, fmt, prec, has);
    if (t.ec) sput_pad(os, "?", 1, 0);
    else sput_pad(os, b, (long)(t.ptr - b), 0);
    sent_d(&s);
    return os;
}
void *os_put(void *os, char c) __asm__("_ZNSt3__113basic_ostreamIcNS_11char_traitsIcEEE3putEc");
void *os_put(void *os, char c) { ob_sent s; sent_c(&s, os); if (s.ok) sput_raw(os, &c, 1); sent_d(&s); return os; }
void *os_flush_ex(void *os) __asm__("_ZNSt3__113basic_ostreamIcNS_11char_traitsIcEEE5flushEv");
void *os_flush_ex(void *os) { ob_sent s; sent_c(&s, os); if (s.ok) os_flush(os); sent_d(&s); return os; }
void *os_write(void *os, const char *p, long n) __asm__("_ZNSt3__113basic_ostreamIcNS_11char_traitsIcEEE5writeEPKcl");
void *os_write(void *os, const char *p, long n) { ob_sent s; sent_c(&s, os); if (s.ok) sput_raw(os, p, n); sent_d(&s); return os; }
void *os_ptr(void *os, const void *p) __asm__("_ZNSt3__113basic_ostreamIcNS_11char_traitsIcEEElsEPKv");
void *os_ptr(void *os, const void *p) { return os_int(os, (unsigned long long)(uintptr_t)p, 0, 0); }
void *os_sbuf(void *os, void *sb) __asm__("_ZNSt3__113basic_ostreamIcNS_11char_traitsIcEEElsEPNS_15basic_streambufIcS2_EE");
void *os_sbuf(void *os, void *sb) {
    ob_sent s; char buf[256]; long n; int any = 0;
    sent_c(&s, os);
    if (!s.ok) return os;
    if (!sb) { os_fail(os); sent_d(&s); return os; }
    while ((n = sb_xsgetn(sb, buf, (long)sizeof buf)) > 0) { sput_raw(os, buf, n); any = 1; }
    if (!any) os_fail(os);
    sent_d(&s);
    return os;
}
void *os_b(void *os, int v) __asm__("_ZNSt3__113basic_ostreamIcNS_11char_traitsIcEEElsEb");
void *os_b(void *os, int v) { return os_bool(os, v); }
void *os_d(void *os, double v) __asm__("_ZNSt3__113basic_ostreamIcNS_11char_traitsIcEEElsEd");
void *os_d(void *os, double v) { return os_float(os, v); }
void *os_e(void *os, long double v) __asm__("_ZNSt3__113basic_ostreamIcNS_11char_traitsIcEEElsEe");
void *os_e(void *os, long double v) { return os_float(os, (double)v); } /* long double na precisão de double */
void *os_f(void *os, float v) __asm__("_ZNSt3__113basic_ostreamIcNS_11char_traitsIcEEElsEf");
void *os_f(void *os, float v) { return os_float(os, (double)v); }
void *os_i(void *os, int v) __asm__("_ZNSt3__113basic_ostreamIcNS_11char_traitsIcEEElsEi");
void *os_i(void *os, int v) { return os_int(os, v < 0 ? (unsigned long long)-(long long)v : (unsigned)v, v < 0, 1); }
void *os_j(void *os, unsigned v) __asm__("_ZNSt3__113basic_ostreamIcNS_11char_traitsIcEEElsEj");
void *os_j(void *os, unsigned v) { return os_int(os, v, 0, 0); }
void *os_l(void *os, long v) __asm__("_ZNSt3__113basic_ostreamIcNS_11char_traitsIcEEElsEl");
void *os_l(void *os, long v) { return os_int(os, v < 0 ? (unsigned long long)-v : (unsigned long)v, v < 0, 1); }
void *os_m(void *os, unsigned long v) __asm__("_ZNSt3__113basic_ostreamIcNS_11char_traitsIcEEElsEm");
void *os_m(void *os, unsigned long v) { return os_int(os, v, 0, 0); }
void *os_s(void *os, short v) __asm__("_ZNSt3__113basic_ostreamIcNS_11char_traitsIcEEElsEs");
void *os_s(void *os, short v) { return os_i(os, v); }
void *os_t(void *os, unsigned short v) __asm__("_ZNSt3__113basic_ostreamIcNS_11char_traitsIcEEElsEt");
void *os_t(void *os, unsigned short v) { return os_int(os, v, 0, 0); }
void *os_x(void *os, long long v) __asm__("_ZNSt3__113basic_ostreamIcNS_11char_traitsIcEEElsEx");
void *os_x(void *os, long long v) { return os_int(os, v < 0 ? (unsigned long long)-v : (unsigned long long)v, v < 0, 1); }
void *os_y(void *os, unsigned long long v) __asm__("_ZNSt3__113basic_ostreamIcNS_11char_traitsIcEEElsEy");
void *os_y(void *os, unsigned long long v) { return os_int(os, v, 0, 0); }

static void *osw_wrap(void *os, const char *s, long n) {
    uint32_t tmp[128];
    long i, k = n > 128 ? 128 : n;
    void *sb = *ios_buf(ios_of(os));
    for (i = 0; i < k; i++) tmp[i] = (unsigned char)s[i];
    if (sb) sb_xsputn(sb, tmp, k);
    return os;
}
void *osw_put(void *os, uint32_t c) __asm__("_ZNSt3__113basic_ostreamIwNS_11char_traitsIwEEE3putEw");
void *osw_put(void *os, uint32_t c) { ob_sent s; char b[4]; sent_c(&s, os); if (s.ok) { b[0] = (char)c; osw_wrap(os, b, 1); } sent_d(&s); return os; }
void *osw_flush(void *os) __asm__("_ZNSt3__113basic_ostreamIwNS_11char_traitsIwEEE5flushEv");
void *osw_flush(void *os) { return os_flush_ex(os); }
void *osw_write(void *os, const uint32_t *p, long n) __asm__("_ZNSt3__113basic_ostreamIwNS_11char_traitsIwEEE5writeEPKwl");
void *osw_write(void *os, const uint32_t *p, long n) {
    ob_sent s; void *sb;
    sent_c(&s, os);
    if (s.ok) { sb = *ios_buf(ios_of(os)); if (sb) sb_xsputn(sb, p, n); }
    sent_d(&s);
    return os;
}
void *osw_sbuf(void *os, void *sb) __asm__("_ZNSt3__113basic_ostreamIwNS_11char_traitsIwEEElsEPNS_15basic_streambufIwS2_EE");
void *osw_sbuf(void *os, void *sb) {
    ob_sent s; uint32_t buf[64]; long n; int any = 0; void *dst;
    sent_c(&s, os);
    if (!s.ok) return os;
    if (!sb) { os_fail(os); sent_d(&s); return os; }
    dst = *ios_buf(ios_of(os));
    while ((n = sb_xsgetn(sb, buf, 64)) > 0) { if (dst) sb_xsputn(dst, buf, n); any = 1; }
    if (!any) os_fail(os);
    sent_d(&s);
    return os;
}
void *osw_b(void *os, int v) __asm__("_ZNSt3__113basic_ostreamIwNS_11char_traitsIwEEElsEb");
void *osw_b(void *os, int v) { return os_bool(os, v); }
void *osw_d(void *os, double v) __asm__("_ZNSt3__113basic_ostreamIwNS_11char_traitsIwEEElsEd");
void *osw_d(void *os, double v) { return os_float(os, v); }
void *osw_e(void *os, long double v) __asm__("_ZNSt3__113basic_ostreamIwNS_11char_traitsIwEEElsEe");
void *osw_e(void *os, long double v) { return os_float(os, (double)v); }
void *osw_f(void *os, float v) __asm__("_ZNSt3__113basic_ostreamIwNS_11char_traitsIwEEElsEf");
void *osw_f(void *os, float v) { return os_float(os, v); }
void *osw_i(void *os, int v) __asm__("_ZNSt3__113basic_ostreamIwNS_11char_traitsIwEEElsEi");
void *osw_i(void *os, int v) { return os_i(os, v); }
void *osw_j(void *os, unsigned v) __asm__("_ZNSt3__113basic_ostreamIwNS_11char_traitsIwEEElsEj");
void *osw_j(void *os, unsigned v) { return os_j(os, v); }
void *osw_l(void *os, long v) __asm__("_ZNSt3__113basic_ostreamIwNS_11char_traitsIwEEElsEl");
void *osw_l(void *os, long v) { return os_l(os, v); }
void *osw_m(void *os, unsigned long v) __asm__("_ZNSt3__113basic_ostreamIwNS_11char_traitsIwEEElsEm");
void *osw_m(void *os, unsigned long v) { return os_m(os, v); }
void *osw_s(void *os, short v) __asm__("_ZNSt3__113basic_ostreamIwNS_11char_traitsIwEEElsEs");
void *osw_s(void *os, short v) { return os_s(os, v); }
void *osw_t(void *os, unsigned short v) __asm__("_ZNSt3__113basic_ostreamIwNS_11char_traitsIwEEElsEt");
void *osw_t(void *os, unsigned short v) { return os_t(os, v); }
void *osw_x(void *os, long long v) __asm__("_ZNSt3__113basic_ostreamIwNS_11char_traitsIwEEElsEx");
void *osw_x(void *os, long long v) { return os_x(os, v); }
void *osw_y(void *os, unsigned long long v) __asm__("_ZNSt3__113basic_ostreamIwNS_11char_traitsIwEEElsEy");
void *osw_y(void *os, unsigned long long v) { return os_y(os, v); }

static void is_sent(unsigned char *ok, void *is, int noskip) {
    char *ios = ios_of(is);
    void *sb;
    *ok = 0;
    if (*ios_st(ios) & 5) { os_fail(is); return; }
    if (*ios_tie(ios)) os_flush(*ios_tie(ios));
    sb = *ios_buf(ios);
    if (!noskip && (*ios_fl(ios) & 0x1000) && sb) {
        for (;;) {
            int c = sb_under(sb);
            if (c < 0) { ios_clear(ios, *ios_st(ios) | 6); return; }
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '\f' && c != '\v') break;
            sb_uflow(sb);
        }
    }
    *ok = 1;
}
void is_sent_c(unsigned char *s, void *is, int noskip) __asm__("_ZNSt3__113basic_istreamIcNS_11char_traitsIcEEE6sentryC1ERS3_b");
void is_sent_c(unsigned char *s, void *is, int noskip) { is_sent(s, is, noskip); }
int is_get(void *is) __asm__("_ZNSt3__113basic_istreamIcNS_11char_traitsIcEEE3getEv");
int is_get(void *is) {
    unsigned char ok; void *sb; int c;
    is_sent(&ok, is, 1);
    if (!ok) return -1;
    sb = *ios_buf(ios_of(is));
    c = sb ? sb_uflow(sb) : -1;
    if (c < 0) ios_clear(ios_of(is), *ios_st(ios_of(is)) | 6);
    return c;
}
int is_peek(void *is) __asm__("_ZNSt3__113basic_istreamIcNS_11char_traitsIcEEE4peekEv");
int is_peek(void *is) {
    unsigned char ok; void *sb; int c;
    is_sent(&ok, is, 1);
    if (!ok) return -1;
    sb = *ios_buf(ios_of(is));
    c = sb ? sb_under(sb) : -1;
    if (c < 0) ios_clear(ios_of(is), *ios_st(ios_of(is)) | 2);
    return c;
}
void *is_read(void *is, char *p, long n) __asm__("_ZNSt3__113basic_istreamIcNS_11char_traitsIcEEE4readEPcl");
void *is_read(void *is, char *p, long n) {
    unsigned char ok; void *sb; long i = 0;
    is_sent(&ok, is, 1);
    if (!ok) return is;
    sb = *ios_buf(ios_of(is));
    if (sb) i = sb_xsgetn(sb, p, n);
    if (i < n) ios_clear(ios_of(is), *ios_st(ios_of(is)) | 6);
    return is;
}
void *is_seek_pos(void *is, ob_fpos *pos) __asm__("_ZNSt3__113basic_istreamIcNS_11char_traitsIcEEE5seekgENS_4fposI11__mbstate_tEE");
void *is_seek_pos(void *is, ob_fpos *pos) {
    void *sb = *ios_buf(ios_of(is));
    ob_fpos r;
    if (!sb) { os_fail(is); return is; }
    sb_seekoff(&r, sb, pos ? pos->off : 0, 0, 8);
    if (r.off < 0) os_fail(is);
    else *ios_st(ios_of(is)) &= ~2u;
    return is;
}
void *is_seek_off(void *is, long long off, int dir) __asm__("_ZNSt3__113basic_istreamIcNS_11char_traitsIcEEE5seekgExNS_8ios_base7seekdirE");
void *is_seek_off(void *is, long long off, int dir) {
    void *sb = *ios_buf(ios_of(is));
    ob_fpos r;
    if (!sb) { os_fail(is); return is; }
    sb_seekoff(&r, sb, off, dir, 8);
    if (r.off < 0) os_fail(is);
    return is;
}
void is_tell(ob_fpos *r, void *is) __asm__("_ZNSt3__113basic_istreamIcNS_11char_traitsIcEEE5tellgEv");
void is_tell(ob_fpos *r, void *is) {
    void *sb = *ios_buf(ios_of(is));
    memset(r, 0, sizeof *r); r->off = -1;
    if (!sb || (*ios_st(ios_of(is)) & 5)) return;
    sb_seekoff(r, sb, 0, 1, 8);
}
static void *is_num(void *is, void *out, int kind) {
    unsigned char ok; void *sb; char buf[128]; int i = 0, c; char *end; long long v; double d;
    is_sent(&ok, is, 0);
    if (!ok) return is;
    sb = *ios_buf(ios_of(is));
    if (!sb) { os_fail(is); return is; }
    c = sb_under(sb);
    if (c == '+' || c == '-') { buf[i++] = (char)c; sb_uflow(sb); c = sb_under(sb); }
    if (kind == 0) {
        while (i < 120 && c >= '0' && c <= '9') { buf[i++] = (char)c; sb_uflow(sb); c = sb_under(sb); }
        buf[i] = 0;
        if (i == 0 || (i == 1 && (buf[0] == '+' || buf[0] == '-'))) { os_fail(is); return is; }
        v = strtoll(buf, &end, 10);
        if (out) *(int *)out = (int)v;
    } else {
        while (i < 120 && c > 0 && c != ' ' && c != '\n' && c != '\t') { buf[i++] = (char)c; sb_uflow(sb); c = sb_under(sb); }
        buf[i] = 0;
        d = strtod(buf, &end);
        if (end == buf) { os_fail(is); return is; }
        if (kind == 1) *(float *)out = (float)d;
    }
    return is;
}
void *is_rb(void *is, int *o) __asm__("_ZNSt3__113basic_istreamIcNS_11char_traitsIcEEErsERb");
void *is_rb(void *is, int *o) { int v = 0; is_num(is, &v, 0); if (o) *o = v != 0; return is; }
void *is_rf(void *is, float *o) __asm__("_ZNSt3__113basic_istreamIcNS_11char_traitsIcEEErsERf");
void *is_rf(void *is, float *o) { return is_num(is, o, 1); }
void *is_ri(void *is, int *o) __asm__("_ZNSt3__113basic_istreamIcNS_11char_traitsIcEEErsERi");
void *is_ri(void *is, int *o) { return is_num(is, o, 0); }

/* cin / cout / cerr. O locale é o imp clássico, o mesmo de locale(). */
extern struct { long refs; } cx_locale_imp;
static struct { void *vt; void *loc; void *g0, *g1, *g2, *p0, *p1, *p2; int fd; int pad; } sb_cout = {
    (void *)&vt_fd[2], &cx_locale_imp, 0, 0, 0, 0, 0, 0, 1, 0
};
static struct { void *vt; void *loc; void *g0, *g1, *g2, *p0, *p1, *p2; int fd; int pad; } sb_cerr = {
    (void *)&vt_fd[2], &cx_locale_imp, 0, 0, 0, 0, 0, 0, 2, 0
};
static struct { void *vt; void *loc; void *g0, *g1, *g2, *p0, *p1, *p2; int fd; int pad; } sb_cin = {
    (void *)&vt_fd[2], &cx_locale_imp, 0, 0, 0, 0, 0, 0, 0, 0
};
struct ob_ostream {
    void *vt, *iosvt; unsigned flags, padf; long prec, width; unsigned state, exc;
    void *rdbuf, *loc, *fn, *idx; unsigned long esz, ecap; void *iarr; unsigned long isz, icap;
    void *parr; unsigned long psz, pcap; void *tie; int fill, pad2;
};
struct ob_istream {
    void *vt; long gc; void *iosvt; unsigned flags, padf; long prec, width; unsigned state, exc;
    void *rdbuf, *loc, *fn, *idx; unsigned long esz, ecap; void *iarr; unsigned long isz, icap;
    void *parr; unsigned long psz, pcap; void *tie; int fill, pad2;
};
_Static_assert(sizeof(struct ob_ostream) == 160, "ostream");
_Static_assert(sizeof(struct ob_istream) == 168, "istream");
struct ob_ostream cout_obj __asm__("_ZNSt3__14coutE") = {
    (void *)&vt_os[3], (void *)&vt_os[8], 0x1002, 0, 6, 0, 0, 0,
    &sb_cout, &cx_locale_imp, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, -1, 0
};
struct ob_ostream cerr_obj __asm__("_ZNSt3__14cerrE") = {
    (void *)&vt_os[3], (void *)&vt_os[8], 0x1002 | 0x2000, 0, 6, 0, 0, 0,
    &sb_cerr, &cx_locale_imp, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, &cout_obj, -1, 0
};
struct ob_istream cin_obj __asm__("_ZNSt3__13cinE") = {
    (void *)&vt_is[3], 0, (void *)&vt_is[8], 0x1002, 0, 6, 0, 0, 0,
    &sb_cin, &cx_locale_imp, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, &cout_obj, -1, 0
};

/* time_put::put — subconjunto de strftime usado por logs */
static void tput_ch(ob_siter *it, char c) {
    void **vt;
    if (!it->sb || it->failed) { it->failed = 1; return; }
    vt = *(void ***)it->sb;
    if (((int (*)(void *, int))vt[13])(it->sb, (unsigned char)c) < 0) it->failed = 1;
}
void tput_put(ob_siter *ret, const void *self, ob_siter *it, void *ios, int fill, const int *tm, const char *b, const char *e) __asm__("_ZNKSt3__18time_putIcNS_19ostreambuf_iteratorIcNS_11char_traitsIcEEEEE3putES4_RNS_8ios_baseEcPK2tmPKcSC_");
void tput_put(ob_siter *ret, const void *self, ob_siter *it, void *ios, int fill, const int *tm, const char *b, const char *e) {
    ob_siter cur = it ? *it : (ob_siter){0, 1};
    (void)self; (void)ios; (void)fill;
    while (b && b < e) {
        if (*b != '%' || b + 1 >= e) { tput_ch(&cur, *b++); continue; }
        b++;
        {
            char tmp[16]; int n = 0, v = 0;
            char spec = *b++;
            if (spec == 'Y') v = tm ? tm[5] + 1900 : 0;
            else if (spec == 'm') v = tm ? tm[4] + 1 : 0;
            else if (spec == 'd') v = tm ? tm[3] : 0;
            else if (spec == 'H') v = tm ? tm[2] : 0;
            else if (spec == 'M') v = tm ? tm[1] : 0;
            else if (spec == 'S') v = tm ? tm[0] : 0;
            else if (spec == '%') { tput_ch(&cur, '%'); continue; }
            else { tput_ch(&cur, spec); continue; }
            fmt_u(tmp, &n, (unsigned long long)v, 10, 0);
            if (spec != 'Y' && n == 1) tput_ch(&cur, '0');
            { int k; for (k = 0; k < n; k++) tput_ch(&cur, tmp[k]); }
        }
    }
    if (ret) *ret = cur;
}

/* ======================== filesystem ======================== */
static void path_c(char *b, size_t cap, const ob_str *p) {
    size_t n = ob_ssize(p);
    if (n >= cap) n = cap - 1;
    memcpy(b, ob_scdata(p), n); b[n] = 0;
}
static int path_stat(const ob_str *p, ob_stat *st, int lnk) {
    char b[1024];
    path_c(b, sizeof b, p);
    return lnk ? lstat(b, st) : stat(b, st);
}
static signed char mode_type(unsigned mode) {
    switch (mode & 0170000) {
    case 0040000: return 2; case 0100000: return 1; case 0120000: return 3;
    case 0060000: return 4; case 0020000: return 5; case 0010000: return 6; case 0140000: return 7;
    default: return 8;
    }
}
static void set_fst(void *ret, signed char ty, unsigned perms) {
    memset(ret, 0, 8);
    *(signed char *)ret = ty;
    *(unsigned *)((char *)ret + 4) = perms;
}
void fs_filename(ob_str *r, const ob_str *p) __asm__("_ZNKSt3__14__fs10filesystem4path10__filenameEv");
void fs_filename(ob_str *r, const ob_str *p) { ob_path_filename(r, p); }
void fs_ext(ob_str *r, const ob_str *p) __asm__("_ZNKSt3__14__fs10filesystem4path11__extensionEv");
void fs_ext(ob_str *r, const ob_str *p) { ob_path_extension(r, p); }
void fs_rootn(ob_str *r, const ob_str *p) __asm__("_ZNKSt3__14__fs10filesystem4path11__root_nameEv");
void fs_rootn(ob_str *r, const ob_str *p) { ob_path_root_name(r, p); }
void fs_parent(ob_str *r, const ob_str *p) __asm__("_ZNKSt3__14__fs10filesystem4path13__parent_pathEv");
void fs_parent(ob_str *r, const ob_str *p) { ob_path_parent(r, p); }
void fs_rel(ob_str *r, const ob_str *p) __asm__("_ZNKSt3__14__fs10filesystem4path15__relative_pathEv");
void fs_rel(ob_str *r, const ob_str *p) { ob_path_relative(r, p); }
void fs_rootd(ob_str *r, const ob_str *p) __asm__("_ZNKSt3__14__fs10filesystem4path16__root_directoryEv");
void fs_rootd(ob_str *r, const ob_str *p) { ob_path_root_dir(r, p); }
void fs_normal(ob_str *r, const ob_str *p) __asm__("_ZNKSt3__14__fs10filesystem4path16lexically_normalEv");
void fs_normal(ob_str *r, const ob_str *p) { ob_path_normal(r, p); }
void fs_lexrel(ob_str *r, const ob_str *p, const ob_str *b) __asm__("_ZNKSt3__14__fs10filesystem4path18lexically_relativeERKS2_");
void fs_lexrel(ob_str *r, const ob_str *p, const ob_str *b) { ob_path_relative_to(r, p, b); }
void fs_begin(ob_piter *r, const ob_str *p) __asm__("_ZNKSt3__14__fs10filesystem4path5beginEv");
void fs_begin(ob_piter *r, const ob_str *p) { ob_path_begin(r, p); }
void fs_end(ob_piter *r, const ob_str *p) __asm__("_ZNKSt3__14__fs10filesystem4path3endEv");
void fs_end(ob_piter *r, const ob_str *p) { ob_path_end(r, p); }
int fs_cmp(const ob_str *p, const char *s, size_t n) __asm__("_ZNKSt3__14__fs10filesystem4path9__compareENS_17basic_string_viewIcNS_11char_traitsIcEEEE");
int fs_cmp(const ob_str *p, const char *s, size_t n) { return ob_scmp_n(p, s, n); }
void piter_inc(ob_piter *it) __asm__("_ZNSt3__14__fs10filesystem4path8iterator11__incrementEv");
void piter_inc(ob_piter *it) { ob_piter_inc(it); }
ob_str *path_repl(ob_str *p, const ob_str *ext) __asm__("_ZNSt3__14__fs10filesystem4path17replace_extensionERKS2_");
ob_str *path_repl(ob_str *p, const ob_str *ext) {
    ob_str cur, stem;
    const char *s; size_t n, dot = (size_t)-1, i;
    ob_sempty(&cur); ob_sempty(&stem);
    ob_path_filename(&cur, p);
    s = ob_scdata(&cur); n = ob_ssize(&cur);
    for (i = 1; i < n; i++) if (s[i] == '.') dot = i;
    if (dot != (size_t)-1) {
        ob_path_parent(&stem, p);
        if (ob_ssize(&stem)) { ob_sappend_n(&stem, "/", 1); ob_sappend_n(&stem, s, dot); }
        else ob_sassign_n(&stem, s, dot);
        ob_sassign_n(p, ob_scdata(&stem), ob_ssize(&stem));
    }
    if (ext && ob_ssize(ext)) {
        if (ob_scdata(ext)[0] != '.') ob_sappend_n(p, ".", 1);
        ob_sappend_n(p, ob_scdata(ext), ob_ssize(ext));
    }
    ob_sfree(&cur); ob_sfree(&stem);
    return p;
}
void fs_abs(ob_str *r, const ob_str *p, void *ec) __asm__("_ZNSt3__14__fs10filesystem10__absoluteERKNS1_4pathEPNS_10error_codeE");
void fs_abs(ob_str *r, const ob_str *p, void *ec) {
    char cwd[1024];
    ob_sempty(r);
    if (ob_ssize(p) && ob_scdata(p)[0] == '/') ob_scopy(r, p);
    else {
        if (!getcwd(cwd, sizeof cwd)) { ec_set(ec, cx_errno ? cx_errno : 2); return; }
        ob_sassign_n(r, cwd, strlen(cwd));
        if (!ob_ssize(r) || ob_scdata(r)[ob_ssize(r) - 1] != '/') ob_sappend_n(r, "/", 1);
        ob_sappend_n(r, ob_scdata(p), ob_ssize(p));
    }
    ec_set(ec, 0);
}
void fs_canonical(ob_str *r, const ob_str *p, void *ec) __asm__("_ZNSt3__14__fs10filesystem11__canonicalERKNS1_4pathEPNS_10error_codeE");
void fs_canonical(ob_str *r, const ob_str *p, void *ec) {
    ob_str abs, norm;
    ob_stat st;
    ob_sempty(&abs); ob_sempty(&norm); ob_sempty(r);
    fs_abs(&abs, p, ec);
    if (ec && ((int *)ec)[0]) { ob_sfree(&abs); return; }
    ob_path_normal(&norm, &abs);
    if (path_stat(&norm, &st, 0) < 0) { ec_set(ec, cx_errno ? cx_errno : 2); ob_sfree(&abs); ob_sfree(&norm); return; }
    ob_scopy(r, &norm);
    ec_set(ec, 0);
    ob_sfree(&abs); ob_sfree(&norm);
}
void fs_weak(ob_str *r, const ob_str *p, void *ec) __asm__("_ZNSt3__14__fs10filesystem18__weakly_canonicalERKNS1_4pathEPNS_10error_codeE");
void fs_weak(ob_str *r, const ob_str *p, void *ec) {
    ob_str abs;
    ob_sempty(&abs); ob_sempty(r);
    fs_abs(&abs, p, ec);
    if (ec && ((int *)ec)[0]) { ob_sfree(&abs); return; }
    ob_path_normal(r, &abs);
    ec_set(ec, 0);
    ob_sfree(&abs);
}
static int copy_fd(const char *a, const char *b, int overwrite) {
    int in, out; char buf[4096]; ssize_t n; ob_stat st;
    if (!overwrite && stat(b, &st) == 0) return 0;
    in = open(a, 0);
    if (in < 0) return -1;
    out = open(b, MAC_O_WRONLY | MAC_O_CREAT | MAC_O_TRUNC, 0644);
    if (out < 0) { close(in); return -1; }
    while ((n = read(in, buf, sizeof buf)) > 0) { if (write(out, buf, (size_t)n) != n) { close(in); close(out); return -1; } }
    close(in); close(out);
    return n < 0 ? -1 : 1;
}
int fs_copy_file(const ob_str *a, const ob_str *b, unsigned opt, void *ec) __asm__("_ZNSt3__14__fs10filesystem11__copy_fileERKNS1_4pathES4_NS1_12copy_optionsEPNS_10error_codeE");
int fs_copy_file(const ob_str *a, const ob_str *b, unsigned opt, void *ec) {
    char pa[1024], pb[1024]; int r;
    path_c(pa, sizeof pa, a); path_c(pb, sizeof pb, b);
    r = copy_fd(pa, pb, (opt & 2) != 0 || (opt & 1) == 0);
    if (r < 0) { ec_set(ec, cx_errno ? cx_errno : 5); return 0; }
    ec_set(ec, 0);
    return r > 0;
}
unsigned long long fs_size(const ob_str *p, void *ec) __asm__("_ZNSt3__14__fs10filesystem11__file_sizeERKNS1_4pathEPNS_10error_codeE");
unsigned long long fs_size(const ob_str *p, void *ec) {
    ob_stat st;
    if (path_stat(p, &st, 0) < 0) { ec_set(ec, cx_errno ? cx_errno : 2); return (unsigned long long)-1; }
    if ((st.st_mode & 0170000) == 0040000) { ec_set(ec, 21); return (unsigned long long)-1; }
    ec_set(ec, 0);
    return (unsigned long long)st.st_size;
}
void fs_perm(const ob_str *p, unsigned perms, unsigned opt, void *ec) __asm__("_ZNSt3__14__fs10filesystem13__permissionsERKNS1_4pathENS1_5permsENS1_12perm_optionsEPNS_10error_codeE");
void fs_perm(const ob_str *p, unsigned perms, unsigned opt, void *ec) {
    char b[1024]; ob_stat st; unsigned mode = perms & 07777;
    path_c(b, sizeof b, p);
    if (opt & 2) { if (stat(b, &st) == 0) mode = (st.st_mode & 07777) | mode; }
    else if (opt & 4) { if (stat(b, &st) == 0) mode = (st.st_mode & 07777) & ~mode; }
    if (chmod(b, (unsigned short)mode) < 0) { ec_set(ec, cx_errno ? cx_errno : 13); return; }
    ec_set(ec, 0);
}
void fs_cwd(const ob_str *p, void *ec) __asm__("_ZNSt3__14__fs10filesystem14__current_pathERKNS1_4pathEPNS_10error_codeE");
void fs_cwd(const ob_str *p, void *ec) {
    char b[1024];
    path_c(b, sizeof b, p);
    if (chdir(b) < 0) { ec_set(ec, cx_errno ? cx_errno : 2); return; }
    ec_set(ec, 0);
}
void fs_sym(void *ret, const ob_str *p, void *ec) __asm__("_ZNSt3__14__fs10filesystem16__symlink_statusERKNS1_4pathEPNS_10error_codeE");
void fs_sym(void *ret, const ob_str *p, void *ec) {
    ob_stat st;
    if (path_stat(p, &st, 1) < 0) { set_fst(ret, cx_errno == 2 ? -1 : 0, 0); ec_set(ec, cx_errno == 2 ? 0 : cx_errno); return; }
    set_fst(ret, mode_type(st.st_mode), st.st_mode & 07777);
    ec_set(ec, 0);
}
void fs_status(void *ret, const ob_str *p, void *ec) __asm__("_ZNSt3__14__fs10filesystem8__statusERKNS1_4pathEPNS_10error_codeE");
void fs_status(void *ret, const ob_str *p, void *ec) {
    ob_stat st;
    if (path_stat(p, &st, 0) < 0) { set_fst(ret, cx_errno == 2 ? -1 : 0, 0); ec_set(ec, cx_errno == 2 ? 0 : cx_errno); return; }
    set_fst(ret, mode_type(st.st_mode), st.st_mode & 07777);
    ec_set(ec, 0);
}
unsigned long long fs_nlink(const ob_str *p, void *ec) __asm__("_ZNSt3__14__fs10filesystem17__hard_link_countERKNS1_4pathEPNS_10error_codeE");
unsigned long long fs_nlink(const ob_str *p, void *ec) {
    ob_stat st;
    if (path_stat(p, &st, 0) < 0) { ec_set(ec, cx_errno ? cx_errno : 2); return (unsigned long long)-1; }
    ec_set(ec, 0);
    return st.st_nlink ? st.st_nlink : 1;
}
int fs_mkdir(const ob_str *p, void *ec) __asm__("_ZNSt3__14__fs10filesystem18__create_directoryERKNS1_4pathEPNS_10error_codeE");
int fs_mkdir(const ob_str *p, void *ec) {
    char b[1024]; ob_stat st;
    path_c(b, sizeof b, p);
    if (stat(b, &st) == 0) { ec_set(ec, (st.st_mode & 0170000) == 0040000 ? 0 : 20); return 0; }
    if (mkdir(b, 0755) < 0) { ec_set(ec, cx_errno ? cx_errno : 13); return 0; }
    ec_set(ec, 0);
    return 1;
}
int fs_mkdirs(const ob_str *p, void *ec) __asm__("_ZNSt3__14__fs10filesystem20__create_directoriesERKNS1_4pathEPNS_10error_codeE");
int fs_mkdirs(const ob_str *p, void *ec) {
    char b[1024]; size_t i, n;
    path_c(b, sizeof b, p);
    n = strlen(b);
    for (i = 1; i < n; i++) if (b[i] == '/') { b[i] = 0; mkdir(b, 0755); b[i] = '/'; }
    if (mkdir(b, 0755) < 0 && cx_errno != 17) { ec_set(ec, cx_errno ? cx_errno : 13); return 0; }
    ec_set(ec, 0);
    return 1;
}
void fs_tmpdir(ob_str *r, void *ec) __asm__("_ZNSt3__14__fs10filesystem21__temp_directory_pathEPNS_10error_codeE");
void fs_tmpdir(ob_str *r, void *ec) {
    const char *t = getenv("TMPDIR");
    ob_stat st;
    if (!t || !t[0]) t = "/tmp";
    ob_sempty(r);
    if (stat(t, &st) < 0 || (st.st_mode & 0170000) != 0040000) { ec_set(ec, 2); return; }
    ob_sassign_n(r, t, strlen(t));
    ec_set(ec, 0);
}
int fs_remove(const ob_str *p, void *ec) __asm__("_ZNSt3__14__fs10filesystem8__removeERKNS1_4pathEPNS_10error_codeE");
int fs_remove(const ob_str *p, void *ec) {
    char b[1024]; ob_stat st; int dir;
    path_c(b, sizeof b, p);
    if (lstat(b, &st) < 0) { ec_set(ec, cx_errno == 2 ? 0 : cx_errno); return 0; }
    dir = (st.st_mode & 0170000) == 0040000;
    if ((dir ? rmdir(b) : unlink(b)) < 0) { ec_set(ec, cx_errno ? cx_errno : 13); return 0; }
    ec_set(ec, 0);
    return 1;
}
void fs_rename(const ob_str *a, const ob_str *b, void *ec) __asm__("_ZNSt3__14__fs10filesystem8__renameERKNS1_4pathES4_PNS_10error_codeE");
void fs_rename(const ob_str *a, const ob_str *b, void *ec) {
    char pa[1024], pb[1024];
    path_c(pa, sizeof pa, a); path_c(pb, sizeof pb, b);
    if (rename(pa, pb) < 0) { ec_set(ec, cx_errno ? cx_errno : 5); return; }
    ec_set(ec, 0);
}
void fs_copy(const ob_str *a, const ob_str *b, unsigned opt, void *ec) __asm__("_ZNSt3__14__fs10filesystem6__copyERKNS1_4pathES4_NS1_12copy_optionsEPNS_10error_codeE");
void fs_copy(const ob_str *a, const ob_str *b, unsigned opt, void *ec) {
    ob_stat st;
    if (path_stat(a, &st, 0) < 0) { ec_set(ec, cx_errno ? cx_errno : 2); return; }
    if ((st.st_mode & 0170000) == 0040000) {
        if (opt & 8) fs_mkdir(b, ec);
        else ec_set(ec, 21);
        return;
    }
    fs_copy_file(a, b, opt, ec);
}

/* directory_iterator: shared_ptr {ptr, cntrl}. cntrl no layout __shared_weak_count. */
typedef struct {
    void *vt; long owners, weak; void *dir; char path[512]; char name[256]; int alive;
} ob_dir;
static void dir_zero(ob_dir *d) { if (d->dir) closedir(d->dir); d->dir = 0; free(d); }
static void dir_dtor(void *d) { (void)d; }
static const void *vt_dir[6] = {0, 0, (void *)dir_dtor, (void *)dir_zero, (void *)dir_zero, (void *)dir_zero};
static int dir_pull(ob_dir *d) {
    ob_dent *e;
    if (!d->dir) return 0;
    for (;;) {
        e = readdir(d->dir);
        if (!e) { d->alive = 0; return 0; }
        if (e->d_name[0] == '.' && (e->d_name[1] == 0 || (e->d_name[1] == '.' && e->d_name[2] == 0))) continue;
        memcpy(d->name, e->d_name, sizeof d->name - 1);
        d->name[sizeof d->name - 1] = 0;
        d->alive = 1;
        return 1;
    }
}
static void dir_entry_fill(void *ret, ob_dir *d) {
    memset(ret, 0, 80);
    ob_sassign_n((ob_str *)ret, d->path, strlen(d->path));
    if (ob_ssize((ob_str *)ret) && ob_scdata((ob_str *)ret)[ob_ssize((ob_str *)ret) - 1] != '/') ob_sappend_n((ob_str *)ret, "/", 1);
    ob_sappend_n((ob_str *)ret, d->name, strlen(d->name));
}
void dir_deref(void *ret, void **it) __asm__("_ZNKSt3__14__fs10filesystem18directory_iterator13__dereferenceEv");
void dir_deref(void *ret, void **it) {
    ob_dir *d = it ? it[0] : 0;
    memset(ret, 0, 80);
    if (d && d->alive) dir_entry_fill(ret, d);
}
void dir_inc(void **it, void *ec) __asm__("_ZNSt3__14__fs10filesystem18directory_iterator11__incrementEPNS_10error_codeE");
void dir_inc(void **it, void *ec) {
    ob_dir *d = it ? it[0] : 0;
    if (!d || !dir_pull(d)) {
        if (it && it[1]) {
            ob_dir *c = (ob_dir *)((char *)it[1] - 0); /* o controle é o próprio objeto */
            (void)c;
            if (it[0] && __atomic_sub_fetch(&((ob_dir *)it[0])->owners, 1, __ATOMIC_ACQ_REL) == -1) dir_zero(it[0]);
            it[0] = 0; it[1] = 0;
        }
    }
    ec_set(ec, 0);
}
void dir_ctor(void **it, const ob_str *p, void *ec, unsigned opt) __asm__("_ZNSt3__14__fs10filesystem18directory_iteratorC1ERKNS1_4pathEPNS_10error_codeENS1_17directory_optionsE");
void dir_ctor(void **it, const ob_str *p, void *ec, unsigned opt) {
    char b[512];
    ob_dir *d;
    (void)opt;
    it[0] = it[1] = 0;
    path_c(b, sizeof b, p);
    d = (ob_dir *)calloc(1, sizeof *d);
    if (!d) { ec_set(ec, 12); return; }
    d->vt = (void *)&vt_dir[2];
    d->owners = 0;
    d->weak = 0;
    memcpy(d->path, b, strlen(b) + 1);
    d->dir = opendir(b);
    if (!d->dir) { int err = cx_errno ? cx_errno : 2; free(d); ec_set(ec, err); return; }
    if (!dir_pull(d)) { closedir(d->dir); free(d); ec_set(ec, 0); return; }
    it[0] = d; it[1] = d;
    ec_set(ec, 0);
}

/* ======================== thread, future, números ======================== */
int mutex_try(void *m) __asm__("_ZNSt3__15mutex8try_lockEv");
int mutex_try(void *m) { return pthread_mutex_trylock(m) == 0; }
void cv_all(void *cv) __asm__("_ZNSt3__118condition_variable10notify_allEv");
void cv_all(void *cv) { pthread_cond_broadcast(cv); }
void cv_timed(void *cv, ob_ulock *lk, const long long *tp) __asm__("_ZNSt3__118condition_variable15__do_timed_waitERNS_11unique_lockINS_5mutexEEENS_6chrono10time_pointINS5_12system_clockENS5_8durationIxNS_5ratioILl1ELl1000000000EEEEEEE");
void cv_timed(void *cv, ob_ulock *lk, const long long *tp) {
    long ts[2];
    if (!lk || !lk->owns) ob_fail("condition_variable::wait");
    ts[0] = (long)(*tp / 1000000000LL);
    ts[1] = (long)(*tp % 1000000000LL);
    if (*tp < 0) { ts[0] = 0; ts[1] = 0; }
    pthread_cond_timedwait(cv, lk->m, ts);
}
static const unsigned SM_WR = 1u << 31, SM_RD = ~(1u << 31);
void sm_ctor(void *m) __asm__("_ZNSt3__119__shared_mutex_baseC1Ev");
void sm_ctor(void *m) {
    memset(m, 0, 168);
    pthread_mutex_init(m, 0);
    pthread_cond_init((char *)m + 64, 0);
    pthread_cond_init((char *)m + 112, 0);
}
static unsigned *sm_st(void *m) { return (unsigned *)((char *)m + 160); }
void sm_lock(void *m) __asm__("_ZNSt3__119__shared_mutex_base4lockEv");
void sm_lock(void *m) {
    pthread_mutex_lock(m);
    while (*sm_st(m) & SM_WR) pthread_cond_wait((char *)m + 64, m);
    *sm_st(m) |= SM_WR;
    while (*sm_st(m) & SM_RD) pthread_cond_wait((char *)m + 112, m);
    pthread_mutex_unlock(m);
}
void sm_unlock(void *m) __asm__("_ZNSt3__119__shared_mutex_base6unlockEv");
void sm_unlock(void *m) {
    pthread_mutex_lock(m);
    *sm_st(m) &= ~SM_WR;
    pthread_cond_broadcast((char *)m + 64);
    pthread_mutex_unlock(m);
}
void sm_lock_sh(void *m) __asm__("_ZNSt3__119__shared_mutex_base11lock_sharedEv");
void sm_lock_sh(void *m) {
    unsigned n;
    pthread_mutex_lock(m);
    while ((*sm_st(m) & SM_WR) || (*sm_st(m) & SM_RD) == SM_RD) pthread_cond_wait((char *)m + 64, m);
    n = (*sm_st(m) & SM_RD) + 1;
    *sm_st(m) = (*sm_st(m) & ~SM_RD) | n;
    pthread_mutex_unlock(m);
}
void sm_unlock_sh(void *m) __asm__("_ZNSt3__119__shared_mutex_base13unlock_sharedEv");
void sm_unlock_sh(void *m) {
    unsigned n;
    pthread_mutex_lock(m);
    n = (*sm_st(m) & SM_RD) - 1;
    *sm_st(m) = (*sm_st(m) & ~SM_RD) | n;
    if (*sm_st(m) & SM_WR) { if (!n) pthread_cond_signal((char *)m + 112); }
    else pthread_cond_broadcast((char *)m + 64);
    pthread_mutex_unlock(m);
}
unsigned hw_conc(void) __asm__("_ZNSt3__16thread20hardware_concurrencyEv");
unsigned hw_conc(void) { long n = sysconf(58); return n > 0 ? (unsigned)n : 1; }
void th_detach(unsigned long *t) __asm__("_ZNSt3__16thread6detachEv");
void th_detach(unsigned long *t) {
    if (!t || !*t) ob_fail("std::system_error: thread::detach");
    pthread_detach(*t);
    *t = 0;
}
long long steady_now(void) __asm__("_ZNSt3__16chrono12steady_clock3nowEv");
long long steady_now(void) {
    long ts[2] = {0, 0};
    clock_gettime(6, ts);
    return (long long)ts[0] * 1000000000LL + ts[1];
}
long long from_time(long t) __asm__("_ZNSt3__16chrono12system_clock11from_time_tEl");
long long from_time(long t) { return (long long)t * 1000000; }

/* promise<void> / future<void>: estado de 144 bytes (shared_count + exception_ptr + mutex + cond + flags). */
typedef struct {
    void *vt; long owners; void *eptr;
    char mx[64]; char cv[48]; unsigned flags;
} ob_assoc;
enum { FUT_READY = 4, FUT_ATT = 2 };
static void assoc_free(ob_assoc *a) { free(a); }
static const void *vt_assoc_arr[6] __attribute__((used)) = {
    0, (void *)&ti_assoc, (void *)assoc_free, (void *)assoc_free, (void *)assoc_free, (void *)assoc_free
};
static ob_assoc *assoc_new(void) {
    ob_assoc *a = (ob_assoc *)calloc(1, sizeof *a);
    a->vt = (void *)&vt_assoc_arr[2];
    a->owners = 0;
    pthread_mutex_init(a->mx, 0);
    pthread_cond_init(a->cv, 0);
    return a;
}
static void assoc_release(ob_assoc *a) {
    if (a && __atomic_sub_fetch(&a->owners, 1, __ATOMIC_ACQ_REL) == -1) assoc_free(a);
}
void assoc_setv(ob_assoc *a) __asm__("_ZNSt3__117__assoc_sub_state9set_valueEv");
void assoc_setv(ob_assoc *a) {
    pthread_mutex_lock(a->mx);
    a->flags |= FUT_READY;
    pthread_cond_broadcast(a->cv);
    pthread_mutex_unlock(a->mx);
}
void assoc_sete(ob_assoc *a, void **ep) __asm__("_ZNSt3__117__assoc_sub_state13set_exceptionESt13exception_ptr");
void assoc_sete(ob_assoc *a, void **ep) {
    pthread_mutex_lock(a->mx);
    a->eptr = ep ? *ep : 0;
    a->flags |= FUT_READY;
    pthread_cond_broadcast(a->cv);
    pthread_mutex_unlock(a->mx);
}
void assoc_wait(ob_assoc *a) __asm__("_ZNSt3__117__assoc_sub_state4waitEv");
void assoc_wait(ob_assoc *a) {
    pthread_mutex_lock(a->mx);
    while ((a->flags & FUT_READY) == 0) pthread_cond_wait(a->cv, a->mx);
    pthread_mutex_unlock(a->mx);
}
void assoc_zero(ob_assoc *a) __asm__("_ZNSt3__117__assoc_sub_state16__on_zero_sharedEv");
void assoc_zero(ob_assoc *a) { assoc_free(a); }
void fut_ctor(void **self, ob_assoc *a) __asm__("_ZNSt3__16futureIvEC1EPNS_17__assoc_sub_stateE");
void fut_ctor(void **self, ob_assoc *a) { if (a) __atomic_add_fetch(&a->owners, 1, __ATOMIC_RELAXED); *self = a; }
void fut_dtor(void **self) __asm__("_ZNSt3__16futureIvED1Ev");
void fut_dtor(void **self) { assoc_release(*self); *self = 0; }
void prom_ctor(void **self) __asm__("_ZNSt3__17promiseIvEC1Ev");
void prom_ctor(void **self) { *self = assoc_new(); }
void prom_dtor(void **self) __asm__("_ZNSt3__17promiseIvED1Ev");
void prom_dtor(void **self) {
    ob_assoc *a = *self;
    if (a && (a->flags & FUT_ATT) && !(a->flags & FUT_READY)) assoc_setv(a);
    assoc_release(a);
    *self = 0;
}
void prom_get(void **ret, void **self) __asm__("_ZNSt3__17promiseIvE10get_futureEv");
void prom_get(void **ret, void **self) {
    ob_assoc *a = *self;
    if (!a || (a->flags & FUT_ATT)) ob_fail("std::future_error");
    a->flags |= FUT_ATT;
    __atomic_add_fetch(&a->owners, 1, __ATOMIC_RELAXED);
    *ret = a;
}
void prom_setv(void **self) __asm__("_ZNSt3__17promiseIvE9set_valueEv");
void prom_setv(void **self) { if (*self) assoc_setv(*self); }
void prom_sete(void **self, void **ep) __asm__("_ZNSt3__17promiseIvE13set_exceptionESt13exception_ptr");
void prom_sete(void **self, void **ep) { if (*self) assoc_sete(*self, ep); }
void swc_d2(void *s) __asm__("_ZNSt3__114__shared_countD2Ev");
void swc_d2(void *s) { (void)s; }

void rx_exec_c(const void *self, void *st) __asm__("_ZNKSt3__123__match_any_but_newlineIcE6__execERNS_7__stateIcEE");
void rx_exec_c(const void *self, void *st) {
    const char **cur = (const char **)((char *)st + 16);
    const char **last = (const char **)((char *)st + 24);
    const void **node = (const void **)((char *)st + 80);
    if (*cur != *last && **cur != '\n') { *(int *)st = -995; (*cur)++; *node = *(const void **)((const char *)self + 8); }
    else { *(int *)st = -993; *node = 0; }
}
void rx_exec_w(const void *self, void *st) __asm__("_ZNKSt3__123__match_any_but_newlineIwE6__execERNS_7__stateIwEE");
void rx_exec_w(const void *self, void *st) {
    const uint32_t **cur = (const uint32_t **)((char *)st + 16);
    const uint32_t **last = (const uint32_t **)((char *)st + 24);
    const void **node = (const void **)((char *)st + 80);
    if (*cur != *last && **cur != (uint32_t)'\n') { *(int *)st = -995; (*cur)++; *node = *(const void **)((const char *)self + 8); }
    else { *(int *)st = -993; *node = 0; }
}

void rd_ctor(unsigned *self, const ob_str *tok) __asm__("_ZNSt3__113random_deviceC1ERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEE");
void rd_ctor(unsigned *self, const ob_str *tok) { (void)tok; *self = (unsigned)steady_now() | 1u; }
void rd_dtor(unsigned *self) __asm__("_ZNSt3__113random_deviceD1Ev");
void rd_dtor(unsigned *self) { (void)self; }
unsigned rd_call(unsigned *self) __asm__("_ZNSt3__113random_deviceclEv");
unsigned rd_call(unsigned *self) {
    unsigned x = *self;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    if (!x) x = 0xA5A5u;
    *self = x;
    return x;
}

void sort_c(char *a, char *b, void *c) __asm__("_ZNSt3__16__sortIRNS_6__lessIccEEPcEEvT0_S5_T_");
void sort_c(char *a, char *b, void *c) { (void)c; ob_sort_c(a, b); }
void sort_f(float *a, float *b, void *c) __asm__("_ZNSt3__16__sortIRNS_6__lessIffEEPfEEvT0_S5_T_");
void sort_f(float *a, float *b, void *c) { (void)c; ob_sort_f(a, b); }
void sort_i(int *a, int *b, void *c) __asm__("_ZNSt3__16__sortIRNS_6__lessIiiEEPiEEvT0_S5_T_");
void sort_i(int *a, int *b, void *c) { (void)c; ob_sort_i(a, b); }
void sort_m(unsigned long *a, unsigned long *b, void *c) __asm__("_ZNSt3__16__sortIRNS_6__lessImmEEPmEEvT0_S5_T_");
void sort_m(unsigned long *a, unsigned long *b, void *c) { (void)c; ob_sort_m(a, b); }
void sort_x(long long *a, long long *b, void *c) __asm__("_ZNSt3__16__sortIRNS_6__lessIxxEEPxEEvT0_S5_T_");
void sort_x(long long *a, long long *b, void *c) { (void)c; ob_sort_x(a, b); }

ob_tcr tc_d(char *a, char *b, double v) __asm__("_ZNSt3__18to_charsEPcS0_d");
ob_tcr tc_d(char *a, char *b, double v) { return ob_to_chars_f(a, b, v, 0, 6, 0); }
ob_tcr tc_df(char *a, char *b, double v, int fmt) __asm__("_ZNSt3__18to_charsEPcS0_dNS_12chars_formatE");
ob_tcr tc_df(char *a, char *b, double v, int fmt) { return ob_to_chars_f(a, b, v, fmt, 6, 0); }
ob_tcr tc_dfp(char *a, char *b, double v, int fmt, int p) __asm__("_ZNSt3__18to_charsEPcS0_dNS_12chars_formatEi");
ob_tcr tc_dfp(char *a, char *b, double v, int fmt, int p) { return ob_to_chars_f(a, b, v, fmt, p, 1); }
ob_tcr tc_e(char *a, char *b, long double v) __asm__("_ZNSt3__18to_charsEPcS0_e");
ob_tcr tc_e(char *a, char *b, long double v) { return ob_to_chars_f(a, b, (double)v, 0, 6, 0); }
ob_tcr tc_ef(char *a, char *b, long double v, int fmt) __asm__("_ZNSt3__18to_charsEPcS0_eNS_12chars_formatE");
ob_tcr tc_ef(char *a, char *b, long double v, int fmt) { return ob_to_chars_f(a, b, (double)v, fmt, 6, 0); }
ob_tcr tc_efp(char *a, char *b, long double v, int fmt, int p) __asm__("_ZNSt3__18to_charsEPcS0_eNS_12chars_formatEi");
ob_tcr tc_efp(char *a, char *b, long double v, int fmt, int p) { return ob_to_chars_f(a, b, (double)v, fmt, p, 1); }
ob_tcr tc_f(char *a, char *b, float v) __asm__("_ZNSt3__18to_charsEPcS0_f");
ob_tcr tc_f(char *a, char *b, float v) { return ob_to_chars_f(a, b, (double)v, 0, 6, 0); }
ob_tcr tc_ff(char *a, char *b, float v, int fmt) __asm__("_ZNSt3__18to_charsEPcS0_fNS_12chars_formatE");
ob_tcr tc_ff(char *a, char *b, float v, int fmt) { return ob_to_chars_f(a, b, (double)v, fmt, 6, 0); }
ob_tcr tc_ffp(char *a, char *b, float v, int fmt, int p) __asm__("_ZNSt3__18to_charsEPcS0_fNS_12chars_formatEi");
ob_tcr tc_ffp(char *a, char *b, float v, int fmt, int p) { return ob_to_chars_f(a, b, (double)v, fmt, p, 1); }

static void to_str_ll(ob_str *r, long long v, int uns) {
    char b[32]; int n = 0;
    unsigned long long u = uns ? (unsigned long long)v : (v < 0 ? (unsigned long long)-v : (unsigned long long)v);
    ob_sempty(r);
    if (!uns && v < 0) b[n++] = '-';
    fmt_u(b, &n, u, 10, 0);
    ob_sassign_n(r, b, (size_t)n);
}
void to_str_f(ob_str *r, float v) __asm__("_ZNSt3__19to_stringEf");
void to_str_f(ob_str *r, float v) { char b[64]; ob_tcr t = ob_to_chars_f(b, b + 64, (double)v, 3, 6, 1); ob_sempty(r); if (!t.ec) ob_sassign_n(r, b, (size_t)(t.ptr - b)); }
void to_str_j(ob_str *r, unsigned v) __asm__("_ZNSt3__19to_stringEj");
void to_str_j(ob_str *r, unsigned v) { to_str_ll(r, (long long)v, 1); }
void to_str_l(ob_str *r, long v) __asm__("_ZNSt3__19to_stringEl");
void to_str_l(ob_str *r, long v) { to_str_ll(r, v, 0); }
void to_str_m(ob_str *r, unsigned long v) __asm__("_ZNSt3__19to_stringEm");
void to_str_m(ob_str *r, unsigned long v) { to_str_ll(r, (long long)v, 1); }
void to_str_x(ob_str *r, long long v) __asm__("_ZNSt3__19to_stringEx");
void to_str_x(ob_str *r, long long v) { to_str_ll(r, v, 0); }
void to_str_y(ob_str *r, unsigned long long v) __asm__("_ZNSt3__19to_stringEy");
void to_str_y(ob_str *r, unsigned long long v) { to_str_ll(r, (long long)v, 1); }

static void st_fail(int k) { ob_fail(k == OB_RANGE ? "std::out_of_range" : "std::invalid_argument"); }
double st_d(const ob_str *s, size_t *idx) __asm__("_ZNSt3__14stodERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEEPm");
double st_d(const ob_str *s, size_t *idx) { double v = 0; int k = ob_stod(ob_scdata(s), ob_ssize(s), idx, &v); if (k) st_fail(k); return v; }
float st_f(const ob_str *s, size_t *idx) __asm__("_ZNSt3__14stofERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEEPm");
float st_f(const ob_str *s, size_t *idx) { return (float)st_d(s, idx); }
int st_i(const ob_str *s, size_t *idx, int base) __asm__("_ZNSt3__14stoiERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEEPmi");
int st_i(const ob_str *s, size_t *idx, int base) { long long v = 0; int k = ob_stoi(ob_scdata(s), ob_ssize(s), idx, base, &v, -2147483648LL, 2147483647LL); if (k) st_fail(k); return (int)v; }
long st_l(const ob_str *s, size_t *idx, int base) __asm__("_ZNSt3__14stolERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEEPmi");
long st_l(const ob_str *s, size_t *idx, int base) { long long v = 0; int k = ob_stoi(ob_scdata(s), ob_ssize(s), idx, base, &v, (long long)((unsigned long long)1 << 63), (long long)(((unsigned long long)1 << 63) - 1)); if (k) st_fail(k); return (long)v; }
long long st_ll(const ob_str *s, size_t *idx, int base) __asm__("_ZNSt3__15stollERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEEPmi");
long long st_ll(const ob_str *s, size_t *idx, int base) { long long v = 0; int k = ob_stoi(ob_scdata(s), ob_ssize(s), idx, base, &v, (long long)((unsigned long long)1 << 63), (long long)(((unsigned long long)1 << 63) - 1)); if (k) st_fail(k); return v; }
unsigned long st_ul(const ob_str *s, size_t *idx, int base) __asm__("_ZNSt3__15stoulERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEEPmi");
unsigned long st_ul(const ob_str *s, size_t *idx, int base) { long long v = 0; int k = ob_stoi(ob_scdata(s), ob_ssize(s), idx, base, &v, 0, (long long)(((unsigned long long)1 << 63) - 1)); if (k) st_fail(k); return (unsigned long)v; }
unsigned long long st_ull(const ob_str *s, size_t *idx, int base) __asm__("_ZNSt3__16stoullERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEEPmi");
unsigned long long st_ull(const ob_str *s, size_t *idx, int base) {
    char buf[128], *end = 0; size_t i = 0, n = ob_ssize(s); unsigned long long v;
    const char *s0 = ob_scdata(s);
    while (i < n && (s0[i] == ' ' || s0[i] == '\t')) i++;
    if (i >= n || n - i >= sizeof buf) st_fail(OB_INVAL);
    memcpy(buf, s0 + i, n - i); buf[n - i] = 0;
    v = strtoull(buf, &end, base);
    if (end == buf) st_fail(OB_INVAL);
    if (idx) *idx = i + (size_t)(end - buf);
    return v;
}
void *std_align(size_t al, size_t sz, void **ptr, size_t *space) __asm__("_ZNSt3__15alignEmmRPvRm");
void *std_align(size_t al, size_t sz, void **ptr, size_t *space) { return ob_align(al, sz, ptr, space); }

/* O símbolo da vtable no Itanium é o address point (depois de vbase/offset/RTTI). */
__asm__(
    ".globl _ZTVNSt3__113basic_ostreamIcNS_11char_traitsIcEEEE\n.set _ZTVNSt3__113basic_ostreamIcNS_11char_traitsIcEEEE, vt_os+24\n"
    ".globl _ZTVNSt3__113basic_ostreamIwNS_11char_traitsIwEEEE\n.set _ZTVNSt3__113basic_ostreamIwNS_11char_traitsIwEEEE, vt_osw+24\n"
    ".globl _ZTVNSt3__113basic_istreamIcNS_11char_traitsIcEEEE\n.set _ZTVNSt3__113basic_istreamIcNS_11char_traitsIcEEEE, vt_is+24\n"
    ".globl _ZTVNSt3__114basic_ofstreamIcNS_11char_traitsIcEEEE\n.set _ZTVNSt3__114basic_ofstreamIcNS_11char_traitsIcEEEE, vt_of+24\n"
    ".globl _ZTVNSt3__114basic_ifstreamIcNS_11char_traitsIcEEEE\n.set _ZTVNSt3__114basic_ifstreamIcNS_11char_traitsIcEEEE, vt_if+24\n"
    ".globl _ZTVNSt3__118basic_stringstreamIcNS_11char_traitsIcEENS_9allocatorIcEEEE\n.set _ZTVNSt3__118basic_stringstreamIcNS_11char_traitsIcEENS_9allocatorIcEEEE, vt_ss+24\n"
    ".globl _ZTVNSt3__119basic_ostringstreamIcNS_11char_traitsIcEENS_9allocatorIcEEEE\n.set _ZTVNSt3__119basic_ostringstreamIcNS_11char_traitsIcEENS_9allocatorIcEEEE, vt_oss+24\n"
    ".globl _ZTVNSt3__119basic_istringstreamIcNS_11char_traitsIcEENS_9allocatorIcEEEE\n.set _ZTVNSt3__119basic_istringstreamIcNS_11char_traitsIcEENS_9allocatorIcEEEE, vt_iss+24\n"
    ".globl _ZTVNSt3__115basic_stringbufIcNS_11char_traitsIcEENS_9allocatorIcEEEE\n.set _ZTVNSt3__115basic_stringbufIcNS_11char_traitsIcEENS_9allocatorIcEEEE, vt_sbuf_arr+16\n"
    ".globl _ZTVNSt3__117__assoc_sub_stateE\n.set _ZTVNSt3__117__assoc_sub_stateE, vt_assoc_arr+16\n"
    ".globl _ZTVNSt3__19basic_iosIcNS_11char_traitsIcEEEE\n.set _ZTVNSt3__19basic_iosIcNS_11char_traitsIcEEEE, vt_bios+16\n"
);
