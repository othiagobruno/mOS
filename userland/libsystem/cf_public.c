/*
 * CoreFoundation que o OpenBot importava e openbot_gap.c devolvia 0.
 * Coleções que já existiam (CFSet, CFCharacterSet, CFLocale, CFUUID, timers) ficam nos arquivos delas.
 * Aqui: bag, calendário, fuso, formatadores, streams ligados, portas, soquete, plist, XML e o resto.
 */
#include "foundation.h"
#include "cf_public_core.inc"

void *calloc(unsigned long n, unsigned long s);
id objc_retain(id o);
void objc_release(id o);
id CFStringCreateWithCString(void *alloc, const char *s, unsigned enc);
long CFStringGetLength(id s);
id CFArrayCreateMutable(void *alloc, long cap, const void *cb);
void CFArrayAppendValue(id a, id v);
long CFArrayGetCount(id a);
id CFArrayGetValueAtIndex(id a, long i);
id CFDictionaryCreateMutable(void *alloc, long cap, const void *k, const void *v);
void CFDictionarySetValue(id d, id k, id v);
id CFDataCreate(void *alloc, const unsigned char *b, long n);
const unsigned char *CFDataGetBytePtr(id d);
long CFDataGetLength(id d);
id CFNumberCreate(void *alloc, long type, const void *p);
double CFDateGetAbsoluteTime(id d);
id CFDateCreate(void *alloc, double at);
id CFPreferencesCopyAppValue(id key, id app);
void *CFRunLoopSourceCreate(void *alloc, long order, void *ctx);
id CFLocaleGetIdentifier(id loc);
id CFTimeZoneCopySystem(void);
int socket(int domain, int type, int proto);
int close(int fd);
long recv(int fd, void *b, unsigned long n, int flags);
long send(int fd, const void *b, unsigned long n, int flags);
id cx_plist_parse(const unsigned char *b, unsigned long n);

#define ENC_UTF8 0x08000100u
static id S(const char *s) { return CFStringCreateWithCString(0, s ? s : "", ENC_UTF8); }
static const char *C(id o) { return o ? cx_cstr(o) : ""; }

/* ---- objetos com estado à parte (o -dealloc libera) ---- */
typedef struct { void *isa; void *p; } box_t;
static cx_class_t box_c, box_m;
static int box_ready;
static void box_free_payload(void *p);
static void box_dealloc(id self, SEL c) {
    (void)c;
    box_free_payload(((box_t *)self)->p);
    ((box_t *)self)->p = 0;
}
static id box_new(void *payload) {
    if (!box_ready) {
        box_ready = 1;
        cx_define_class(&box_c, &box_m, "__mOSCFBox", cx_class_named("NSObject"), 8);
        cx_add_method(&box_c, "dealloc", (void *)box_dealloc);
    }
    box_t *o = (box_t *)cx_alloc_instance(&box_c);
    o->p = payload;
    return (id)o;
}
static int is_box(id o) { return o && object_getClass(o) == &box_c; }

enum { K_BAG = 1, K_CAL, K_FMTN, K_FMTD, K_FD, K_MP, K_SOCK, K_RD, K_WR, K_TZ };
typedef struct { int k; } any_t;

/* ---- CFBag ---- */
typedef struct { int k; cf_bag bag; } bag_o;
u64 CFBagGetTypeID(void) { return 26; }
id CFBagCreate(void *alloc, const void **vals, long n, const void *cb) {
    (void)alloc; (void)cb;
    bag_o *b = calloc(1, sizeof *b);
    if (!b) return 0;
    b->k = K_BAG;
    for (long i = 0; i < n; i++) if (vals) cf_bag_add(&b->bag, vals[i]);
    return box_new(b);
}
static bag_o *as_bag(id o) { bag_o *b = is_box(o) ? ((box_t *)o)->p : 0; return b && b->k == K_BAG ? b : 0; }
long CFBagGetCount(id o) { bag_o *b = as_bag(o); return b ? cf_bag_count(&b->bag, 0) : 0; }
long CFBagGetCountOfValue(id o, const void *v) { bag_o *b = as_bag(o); return b ? cf_bag_count(&b->bag, v) : 0; }
unsigned char CFBagContainsValue(id o, const void *v) { return CFBagGetCountOfValue(o, v) > 0; }

/* ---- fuso: o objeto guarda o nome; CFTimeZoneCopySystem continua devolvendo a string ---- */
typedef struct { int k; char name[96]; } tz_o;
u64 CFTimeZoneGetTypeID(void) { return 27; }
static const char *tz_name_of(id tz) {
    if (!tz) return "UTC";
    tz_o *t = is_box(tz) ? ((box_t *)tz)->p : 0;
    if (t && t->k == K_TZ) return t->name;
    return C(tz);
}
id CFTimeZoneCreateWithName(void *alloc, id name, unsigned char try_abbr) {
    (void)alloc; (void)try_abbr;
    tz_o *t = calloc(1, sizeof *t);
    const char *n = C(name);
    if (!t) return 0;
    t->k = K_TZ;
    if (!n[0]) n = "UTC";
    memcpy(t->name, n, strlen(n) < 95 ? strlen(n) : 95);
    return box_new(t);
}
long CFTimeZoneGetSecondsFromGMT(id tz, double at) { return cf_tz_offset(tz_name_of(tz), at); }
id CFTimeZoneCopyAbbreviation(id tz, double at) { return S(cf_tz_abbr(tz_name_of(tz), at)); }

/* ---- calendário gregoriano ---- */
typedef struct { int k; char ident[40]; id tz; } cal_o;
static void box_free_payload(void *p) {
    any_t *a = p;
    if (!a) return;
    if (a->k == K_BAG) free(((bag_o *)a)->bag.s);
    if (a->k == K_CAL && ((cal_o *)a)->tz) objc_release(((cal_o *)a)->tz);
    free(a);
}
u64 CFCalendarGetTypeID(void) { return 28; }
static cal_o *as_cal(id o) { cal_o *c = is_box(o) ? ((box_t *)o)->p : 0; return c && c->k == K_CAL ? c : 0; }
id CFCalendarCreateWithIdentifier(void *alloc, id ident) {
    (void)alloc;
    cal_o *c = calloc(1, sizeof *c);
    const char *n = C(ident);
    if (!c) return 0;
    c->k = K_CAL;
    if (!n[0]) n = "gregorian";
    memcpy(c->ident, n, strlen(n) < 39 ? strlen(n) : 39);
    c->tz = objc_retain(CFTimeZoneCopySystem());
    return box_new(c);
}
id CFCalendarCopyCurrent(void) { return CFCalendarCreateWithIdentifier(0, cx_str("gregorian")); }
id CFCalendarGetIdentifier(id cal) { cal_o *c = as_cal(cal); return c ? S(c->ident) : 0; }
id CFCalendarCopyTimeZone(id cal) { cal_o *c = as_cal(cal); return c && c->tz ? objc_retain(c->tz) : CFTimeZoneCopySystem(); }
void CFCalendarSetTimeZone(id cal, id tz) {
    cal_o *c = as_cal(cal);
    if (!c || !tz) return;
    if (c->tz) objc_release(c->tz);
    c->tz = objc_retain(tz);
}
static double cal_abs(id cal, double at, int to_local) {
    long off = CFTimeZoneGetSecondsFromGMT(as_cal(cal) ? as_cal(cal)->tz : 0, at);
    return to_local ? at + off : at - off;
}
enum {
    U_ERA = 1 << 1, U_YEAR = 1 << 2, U_MONTH = 1 << 3, U_DAY = 1 << 4,
    U_HOUR = 1 << 5, U_MIN = 1 << 6, U_SEC = 1 << 7, U_WEEK = 1 << 8, U_WDAY = 1 << 9
};
unsigned char CFCalendarDecomposeAbsoluteTime(id cal, double at, unsigned long units, int *a, int *b, int *c, int *d, int *e, int *f) {
    int y, mo, da, h, mi, s, wd, n = 0;
    int *out[6];
    cf_ymdhms_from_abs(cal_abs(cal, at, 1), &y, &mo, &da, &h, &mi, &s, &wd);
    out[0] = a; out[1] = b; out[2] = c; out[3] = d; out[4] = e; out[5] = f;
    if (units & U_ERA) { if (out[n]) *out[n] = 1; n++; }
    if (units & U_YEAR) { if (out[n]) *out[n] = y; n++; }
    if (units & U_MONTH) { if (out[n]) *out[n] = mo; n++; }
    if (units & U_DAY) { if (out[n]) *out[n] = da; n++; }
    if (units & U_HOUR) { if (out[n]) *out[n] = h; n++; }
    if (units & U_MIN) { if (out[n]) *out[n] = mi; n++; }
    if (units & U_SEC && n < 6) { if (out[n]) *out[n] = s; n++; }
    if (units & U_WDAY && n < 6) { if (out[n]) *out[n] = wd; }
    return 1;
}
unsigned char CFCalendarComposeAbsoluteTime(id cal, double *at, unsigned long units, int a, int b, int c, int d, int e, int f) {
    int y = 2001, mo = 1, da = 1, h = 0, mi = 0, s = 0, n = 0;
    int v[6] = {a, b, c, d, e, f};
    if (units & U_ERA) n++;
    if (units & U_YEAR) y = v[n++];
    if (units & U_MONTH) mo = v[n++];
    if (units & U_DAY) da = v[n++];
    if (units & U_HOUR) h = v[n++];
    if (units & U_MIN) mi = v[n++];
    if (units & U_SEC && n < 6) s = v[n];
    if (mo < 1) mo = 1;
    if (da < 1) da = 1;
    if (at) *at = cal_abs(cal, cf_abs_from_ymdhms(y, mo, da, h, mi, s), 0);
    return 1;
}

/* ---- attributed string (é o NSAttributedString) ---- */
u64 CFAttributedStringGetTypeID(void) { return 31; }
id CFAttributedStringGetString(id a) { return a ? MSG0(id, a, "string") : 0; }
long CFAttributedStringGetLength(id a) { return CFStringGetLength(CFAttributedStringGetString(a)); }

/* ---- formatador de número ---- */
typedef struct { int k; char loc[32]; int style; } fmt_o;
u64 CFNumberFormatterGetTypeID(void) { return 32; }
id CFNumberFormatterCreate(void *alloc, id locale, long style) {
    (void)alloc;
    fmt_o *f = calloc(1, sizeof *f);
    const char *id = locale ? C(CFLocaleGetIdentifier(locale)) : "pt_BR";
    if (!f) return 0;
    f->k = K_FMTN;
    f->style = (int)style;
    if (!id[0]) id = "pt_BR";
    memcpy(f->loc, id, strlen(id) < 31 ? strlen(id) : 31);
    return box_new(f);
}
static fmt_o *as_fmt(id o, int k) { fmt_o *f = is_box(o) ? ((box_t *)o)->p : 0; return f && f->k == k ? f : 0; }
id CFNumberFormatterCreateStringWithNumber(id fmt, id num) {
    fmt_o *f = as_fmt(fmt, K_FMTN);
    char dec = ',', grp = '.', buf[64];
    double d = num ? MSG0(double, num, "doubleValue") : 0;
    long long iv = num ? MSG0(long long, num, "longLongValue") : 0;
    int frac = (d == (double)iv) ? -1 : 2;
    cf_locale_seps(f ? f->loc : "pt_BR", &dec, &grp);
    if (f && f->style == 3) { d *= 100; iv = (long long)d; frac = -1; }
    cf_fmt_grouped(f && f->style == 3 ? (long long)d : iv, frac, d, dec, grp, buf, (int)sizeof buf);
    if (f && f->style == 3) {
        unsigned long n = strlen(buf);
        if (n + 1 < sizeof buf) { buf[n] = '%'; buf[n + 1] = 0; }
    }
    if (f && f->style == 2) {
        char tmp[80];
        const char *pre = !strncmp(f->loc, "en", 2) ? "$" : "R$ ";
        memcpy(tmp, pre, strlen(pre));
        memcpy(tmp + strlen(pre), buf, strlen(buf) + 1);
        return S(tmp);
    }
    return S(buf);
}
id CFNumberFormatterCreateNumberFromString(id fmt, id str) {
    fmt_o *f = as_fmt(fmt, K_FMTN);
    char dec = ',', grp = '.';
    const char *s = C(str);
    char tmp[64];
    int n = 0, saw = 0;
    double v = 0, place = 0;
    int neg = 0;
    cf_locale_seps(f ? f->loc : "pt_BR", &dec, &grp);
    if (*s == '-') { neg = 1; s++; }
    for (; *s && n < 60; s++) {
        if (*s == grp || *s == ' ') continue;
        if (*s == dec) { saw = 1; place = 0.1; continue; }
        if (*s < '0' || *s > '9') break;
        if (!saw) v = v * 10 + (*s - '0');
        else { v += (*s - '0') * place; place *= 0.1; }
        tmp[n++] = *s;
    }
    (void)tmp;
    if (neg) v = -v;
    return CFNumberCreate(0, 6 /* kCFNumberFloat64Type */, &v);
}

/* ---- formatador de data ---- */
u64 CFDateFormatterGetTypeID(void) { return 33; }
id CFDateFormatterCreate(void *alloc, id locale, long date_style, long time_style) {
    (void)alloc;
    fmt_o *f = calloc(1, sizeof *f);
    const char *id = locale ? C(CFLocaleGetIdentifier(locale)) : "pt_BR";
    if (!f) return 0;
    f->k = K_FMTD;
    f->style = (int)((date_style & 7) | ((time_style & 7) << 4));
    if (!id[0]) id = "pt_BR";
    memcpy(f->loc, id, strlen(id) < 31 ? strlen(id) : 31);
    return box_new(f);
}
id CFDateFormatterCreateStringWithDate(id fmt, id date) {
    fmt_o *f = as_fmt(fmt, K_FMTD);
    int ds = f ? (f->style & 7) : 1;
    int ts = f ? ((f->style >> 4) & 7) : 1;
    double at = date ? CFDateGetAbsoluteTime(date) : 0;
    id tz = CFTimeZoneCopySystem();
    long off = CFTimeZoneGetSecondsFromGMT(tz, at);
    int y, mo, d, h, mi, s, wd;
    char buf[80];
    int o = 0;
    static const char *mon[] = {"", "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    if (tz) objc_release(tz);
    cf_ymdhms_from_abs(at + off, &y, &mo, &d, &h, &mi, &s, &wd);
    buf[0] = 0;
    if (ds) {
        if (ds >= 3) o += snprintf(buf + o, sizeof buf - (size_t)o, "%d %s %d", d, mon[mo], y);
        else o += snprintf(buf + o, sizeof buf - (size_t)o, "%04d-%02d-%02d", y, mo, d);
    }
    if (ts) {
        if (o && o + 1 < (int)sizeof buf) buf[o++] = ' ';
        snprintf(buf + o, sizeof buf - (size_t)o, "%02d:%02d:%02d", h, mi, s);
    }
    return S(buf);
}

/* ---- descritor de arquivo ---- */
typedef void (*fd_cb_f)(void *fd, unsigned long flags, void *info);
typedef struct { int k; int fd; int close_on; int valid; unsigned long enabled; fd_cb_f cb; void *info; } fd_o;
u64 CFFileDescriptorGetTypeID(void) { return 35; }
id CFFileDescriptorCreate(void *alloc, int fd, unsigned char close_on, void *cb, const void *ctx) {
    (void)alloc;
    fd_o *f = calloc(1, sizeof *f);
    if (!f) return 0;
    f->k = K_FD; f->fd = fd; f->close_on = close_on; f->valid = 1; f->cb = (fd_cb_f)cb;
    f->info = ctx ? *(void **)((char *)ctx + sizeof(long)) : 0;
    return box_new(f);
}
static fd_o *as_fd(id o);
void CFFileDescriptorEnableCallBacks(id o, unsigned long flags) { fd_o *f = as_fd(o); if (f) f->enabled |= flags; }
static void fd_perform(void *info) {
    fd_o *f = info;
    if (f && f->valid && f->enabled && f->cb) f->cb(f, f->enabled, f->info);
}
void *CFFileDescriptorCreateRunLoopSource(void *alloc, id fd, long order) {
    struct {
        long version; void *info; const void *(*retain)(const void *); void (*release)(const void *);
        void *copyDescription, *equal, *hash;
        void (*schedule)(void *, void *, void *); void (*cancel)(void *, void *, void *);
        void (*perform)(void *);
    } ctx;
    memset(&ctx, 0, sizeof ctx);
    ctx.info = as_fd(fd);
    ctx.perform = fd_perform;
    return CFRunLoopSourceCreate(alloc, order, &ctx);
}
static fd_o *as_fd(id o) { fd_o *f = is_box(o) ? ((box_t *)o)->p : 0; return f && f->k == K_FD ? f : 0; }
int CFFileDescriptorGetNativeDescriptor(id o) { fd_o *f = as_fd(o); return f ? f->fd : -1; }
void CFFileDescriptorInvalidate(id o) {
    fd_o *f = as_fd(o);
    if (!f || !f->valid) return;
    if (f->close_on) close(f->fd);
    f->valid = 0;
}

/* ---- CFMessagePort: entrega no mesmo processo, pelo nome ---- */
typedef id (*mp_cb_f)(void *local, long msgid, id data, void *info);
typedef void (*mp_inv_f)(void *port, void *info);
typedef struct { int k; char name[128]; int valid, remote; mp_cb_f cb; void *info; void *queue; mp_inv_f inv; } mp_o;
enum { MP_N = 32 };
static mp_o *mp_tab[MP_N];
u64 CFMessagePortGetTypeID(void) { return 36; }
static mp_o *as_mp(id o) { mp_o *m = is_box(o) ? ((box_t *)o)->p : 0; return m && m->k == K_MP ? m : 0; }
id CFMessagePortCreateLocal(void *alloc, id name, void *cb, void *ctx, unsigned char *should_free) {
    (void)alloc;
    mp_o *m; int i;
    const char *n = C(name);
    if (should_free) *should_free = 0;
    for (i = 0; i < MP_N; i++) if (mp_tab[i] && mp_tab[i]->valid && !mp_tab[i]->remote && !strcmp(mp_tab[i]->name, n)) return 0;
    m = calloc(1, sizeof *m);
    if (!m) return 0;
    m->k = K_MP; m->valid = 1; m->cb = (mp_cb_f)cb;
    m->info = ctx ? *(void **)((char *)ctx + sizeof(long)) : 0;
    memcpy(m->name, n, strlen(n) < 127 ? strlen(n) : 127);
    for (i = 0; i < MP_N; i++) if (!mp_tab[i]) { mp_tab[i] = m; break; }
    return box_new(m);
}
id CFMessagePortCreateRemote(void *alloc, id name) {
    (void)alloc;
    mp_o *m = calloc(1, sizeof *m);
    const char *n = C(name);
    if (!m) return 0;
    m->k = K_MP; m->valid = 1; m->remote = 1;
    memcpy(m->name, n, strlen(n) < 127 ? strlen(n) : 127);
    return box_new(m);
}
void CFMessagePortInvalidate(id o) {
    mp_o *m = as_mp(o);
    int i;
    if (!m || !m->valid) return;
    m->valid = 0;
    if (m->inv) m->inv(o, m->info);
    for (i = 0; i < MP_N; i++) if (mp_tab[i] == m) mp_tab[i] = 0;
}
unsigned char CFMessagePortIsValid(id o) { mp_o *m = as_mp(o); return m && m->valid; }
void CFMessagePortSetDispatchQueue(id o, void *q) { mp_o *m = as_mp(o); if (m) m->queue = q; }
void CFMessagePortSetInvalidationCallBack(id o, void *cb) { mp_o *m = as_mp(o); if (m) m->inv = (mp_inv_f)cb; }
long CFMessagePortSendRequest(id remote, long msgid, id data, double send_to, double recv_to, id mode, id *reply) {
    mp_o *r = as_mp(remote);
    int i;
    (void)send_to; (void)recv_to; (void)mode;
    if (reply) *reply = 0;
    if (!r || !r->valid) return 3;
    for (i = 0; i < MP_N; i++) {
        mp_o *l = mp_tab[i];
        if (l && l->valid && !l->remote && !strcmp(l->name, r->name)) {
            id ans = l->cb ? l->cb(0, msgid, data, l->info) : 0;
            if (reply) *reply = ans;
            return 0;
        }
    }
    return 2;
}
void *CFMessagePortCreateRunLoopSource(void *alloc, id port, long order) {
    struct {
        long version; void *info; const void *(*retain)(const void *); void (*release)(const void *);
        void *copyDescription, *equal, *hash;
        void (*schedule)(void *, void *, void *); void (*cancel)(void *, void *, void *);
        void (*perform)(void *);
    } ctx;
    (void)port;
    memset(&ctx, 0, sizeof ctx);
    return CFRunLoopSourceCreate(alloc, order, &ctx);
}

/* ---- CFSocket: o descritor nativo vem de socket() ---- */
typedef void (*sock_cb_f)(void *s, unsigned long type, void *addr, const void *data, void *info);
typedef struct { int k; int fd; int valid; unsigned long flags; sock_cb_f cb; void *info; } sock_o;
u64 CFSocketGetTypeID(void) { return 37; }
id CFSocketCreate(void *alloc, int family, int type, int proto, unsigned long flags, void *cb, const void *ctx) {
    (void)alloc;
    sock_o *s = calloc(1, sizeof *s);
    int fd;
    if (!s) return 0;
    fd = socket(family, type, proto);
    s->k = K_SOCK; s->fd = fd; s->valid = fd >= 0; s->flags = flags; s->cb = (sock_cb_f)cb;
    s->info = ctx ? *(void **)((char *)ctx + sizeof(long)) : 0;
    return box_new(s);
}
static void sock_bridge(void *info, int kind, const void *data, int n) {
    sock_o *s = info;
    (void)n;
    if (s && s->cb) s->cb(s, (unsigned long)kind, 0, data, s->info);
}
static void sock_perform(void *info) {
    sock_o *s = info;
    if (s && s->valid) cf_sock_service(s->fd, (unsigned)s->flags, sock_bridge, s);
}
static sock_o *as_sock(id o);
void *CFSocketCreateRunLoopSource(void *alloc, id sock, long order) {
    struct {
        long version; void *info; const void *(*retain)(const void *); void (*release)(const void *);
        void *copyDescription, *equal, *hash;
        void (*schedule)(void *, void *, void *); void (*cancel)(void *, void *, void *);
        void (*perform)(void *);
    } ctx;
    memset(&ctx, 0, sizeof ctx);
    ctx.info = as_sock(sock);
    ctx.perform = sock_perform;
    return CFRunLoopSourceCreate(alloc, order, &ctx);
}
long recv(int fd, void *b, unsigned long n, int flags);
int connect(int fd, const void *addr, unsigned len);
long send(int fd, const void *b, unsigned long n, int flags);
int CFSocketConnectToAddress(id sock, id addr, double timeout) {
    sock_o *s = as_sock(sock);
    (void)timeout;
    if (!s || !s->valid || !addr) return -1;
    return connect(s->fd, CFDataGetBytePtr(addr), (unsigned)CFDataGetLength(addr));
}
void CFSocketSendData(id sock, id addr, id data, double timeout) {
    sock_o *s = as_sock(sock);
    (void)addr; (void)timeout;
    if (!s || !s->valid || !data) return;
    send(s->fd, CFDataGetBytePtr(data), (unsigned long)CFDataGetLength(data), 0);
}
void CFSocketSetSocketFlags(id sock, unsigned long flags) { sock_o *s = as_sock(sock); if (s) s->flags = flags; }
static sock_o *as_sock(id o) { sock_o *s = is_box(o) ? ((box_t *)o)->p : 0; return s && s->k == K_SOCK ? s : 0; }
int CFSocketGetNative(id o) { sock_o *s = as_sock(o); return s ? s->fd : -1; }
void CFSocketInvalidate(id o) {
    sock_o *s = as_sock(o);
    if (!s || !s->valid) return;
    if (s->fd >= 0) close(s->fd);
    s->valid = 0; s->fd = -1;
}
unsigned char CFSocketIsValid(id o) { sock_o *s = as_sock(o); return s && s->valid; }

/* ---- par de streams sobre um anel ---- */
typedef struct { cf_ring g; int refs; } ring_s;
typedef struct { int k; ring_s *r; int fd; } stream_o;
static cx_class_t st_c, st_m;
static int st_ready;
u64 CFReadStreamGetTypeID(void) { return 38; }
u64 CFWriteStreamGetTypeID(void) { return 39; }
static void st_dealloc(id self, SEL c) {
    stream_o *s = ((box_t *)self)->p;
    (void)c;
    if (s && s->r && --s->r->refs == 0) free(s->r);
    if (s) s->r = 0;
    free(s);
    ((box_t *)self)->p = 0;
}
static long st_read(id self, SEL c, unsigned char *buf, unsigned long n) {
    stream_o *s = ((box_t *)self)->p;
    (void)c;
    if (s && s->k == K_RD && s->r) return cf_ring_read(&s->r->g, buf, (int)n);
    if (s && s->fd >= 0) return (long)recv(s->fd, buf, n, 0);
    return -1;
}
static long st_write(id self, SEL c, const unsigned char *buf, unsigned long n) {
    stream_o *s = ((box_t *)self)->p;
    (void)c;
    if (s && s->k == K_WR && s->r) return cf_ring_write(&s->r->g, buf, (int)n);
    if (s && s->fd >= 0) return (long)send(s->fd, buf, n, 0);
    return -1;
}
static signed char st_has_r(id self, SEL c) { stream_o *s = ((box_t *)self)->p; (void)c; return s && s->r && s->r->g.n > 0; }
static signed char st_has_w(id self, SEL c) { stream_o *s = ((box_t *)self)->p; (void)c; return s && s->r && s->r->g.n < CF_RING; }
static void st_open(id self, SEL c) { (void)self; (void)c; }
static void st_close(id self, SEL c) {
    stream_o *s = ((box_t *)self)->p;
    (void)c;
    if (!s || !s->r) return;
    if (s->k == K_WR) s->r->g.closed_w = 1;
    else s->r->g.closed_r = 1;
}
static unsigned long st_status(id self, SEL c) { (void)self; (void)c; return 2; }
void CFStreamCreateBoundPair(void *alloc, id *rd, id *wr, long bufsz) {
    ring_s *g; stream_o *a, *b;
    (void)alloc; (void)bufsz;
    if (rd) *rd = 0;
    if (wr) *wr = 0;
    if (!st_ready) {
        st_ready = 1;
        cx_define_class(&st_c, &st_m, "__mOSCFStream", cx_class_named("NSObject"), 8);
        cx_add_method(&st_c, "dealloc", (void *)st_dealloc);
        cx_add_method(&st_c, "read:maxLength:", (void *)st_read);
        cx_add_method(&st_c, "write:maxLength:", (void *)st_write);
        cx_add_method(&st_c, "hasBytesAvailable", (void *)st_has_r);
        cx_add_method(&st_c, "hasSpaceAvailable", (void *)st_has_w);
        cx_add_method(&st_c, "open", (void *)st_open);
        cx_add_method(&st_c, "close", (void *)st_close);
        cx_add_method(&st_c, "streamStatus", (void *)st_status);
    }
    if (!rd && !wr) return;
    g = calloc(1, sizeof *g);
    a = calloc(1, sizeof *a);
    b = calloc(1, sizeof *b);
    if (!g || !a || !b) { free(g); free(a); free(b); return; }
    a->k = K_RD; b->k = K_WR; a->r = b->r = g; a->fd = b->fd = -1; g->refs = 2;
    if (rd) *rd = box_new(a); else free(a);
    if (wr) *wr = box_new(b); else free(b);
    /* box_new usa box_c, não st_c: o read: precisa estar na classe do objeto. */
    if (rd && *rd) ((box_t *)*rd)->isa = &st_c;
    if (wr && *wr) ((box_t *)*wr)->isa = &st_c;
}
static id stream_on_fd(int fd, int kind) {
    stream_o *s;
    id o;
    if (!st_ready) CFStreamCreateBoundPair(0, 0, 0, 0);
    s = calloc(1, sizeof *s);
    if (!s) return 0;
    s->k = kind; s->fd = fd; s->r = 0;
    o = box_new(s);
    if (o) ((box_t *)o)->isa = &st_c;
    return o;
}
void CFStreamCreatePairWithSocket(void *alloc, int fd, id *rd, id *wr) {
    (void)alloc;
    if (rd) *rd = stream_on_fd(fd, K_RD);
    if (wr) *wr = stream_on_fd(fd, K_WR);
}
void CFStreamCreatePairWithSocketToHost(void *alloc, id host, int port, id *rd, id *wr) {
    typedef struct { int flags, family, socktype, protocol; unsigned addrlen; char *canon; void *addr; void *next; } ai_t;
    int getaddrinfo(const char *, const char *, const ai_t *, ai_t **);
    void freeaddrinfo(ai_t *);
    ai_t hint, *res = 0;
    char svc[16];
    int fd = -1;
    (void)alloc;
    if (rd) *rd = 0;
    if (wr) *wr = 0;
    memset(&hint, 0, sizeof hint);
    hint.family = 2; hint.socktype = 1;
    snprintf(svc, sizeof svc, "%d", port);
    if (getaddrinfo(C(host), svc, &hint, &res) || !res) return;
    fd = socket(res->family, res->socktype ? res->socktype : 1, res->protocol);
    if (fd >= 0 && connect(fd, res->addr, res->addrlen)) { close(fd); fd = -1; }
    freeaddrinfo(res);
    if (fd < 0) return;
    CFStreamCreatePairWithSocket(alloc, fd, rd, wr);
}

/* ---- bundle, erro, notificação ---- */
u64 CFBundleGetTypeID(void) { return 43; }
id CFBundleCopyResourceURL(id bundle, id name, id ext, id sub) {
    id path, url;
    if (!bundle) return 0;
    path = MSG(id, bundle, "pathForResource:ofType:inDirectory:", name, ext, sub);
    if (!path) return 0;
    url = MSG(id, (id)cx_class_named("NSURL"), "fileURLWithPath:", path);
    return url ? objc_retain(url) : 0;
}
id CFErrorCopyFailureReason(id e) {
    id info, v;
    if (!e) return 0;
    if (cx_responds(object_getClass(e), sel_registerName("localizedFailureReason"))) {
        v = MSG0(id, e, "localizedFailureReason");
        if (v) return objc_retain(v);
    }
    info = MSG0(id, e, "userInfo");
    v = info ? MSG(id, info, "objectForKey:", cx_str("NSLocalizedFailureReason")) : 0;
    return v ? objc_retain(v) : 0;
}
u64 CFNotificationCenterGetTypeID(void) { return 46; }

/* ---- XML ---- */
id CFXMLCreateStringByEscapingEntities(void *alloc, id str, id entities) {
    char buf[1024];
    (void)alloc; (void)entities;
    cf_xml_escape(C(str), buf, (int)sizeof buf);
    return S(buf);
}
id CFXMLCreateStringByUnescapingEntities(void *alloc, id str, id entities) {
    char buf[1024];
    (void)alloc; (void)entities;
    cf_xml_unescape(C(str), buf, (int)sizeof buf);
    return S(buf);
}

/* ---- property list: XML que o cx_plist_parse relê; cópia funda; stream ---- */
static void pb_add(char **b, unsigned long *n, unsigned long *cap, const char *s) {
    unsigned long k = strlen(s);
    if (*n + k + 1 > *cap) {
        unsigned long c = (*cap ? *cap * 2 : 256);
        while (c < *n + k + 1) c *= 2;
        char *nb = realloc(*b, c);
        if (!nb) return;
        *b = nb; *cap = c;
    }
    memcpy(*b + *n, s, k);
    *n += k;
    (*b)[*n] = 0;
}
static int is_class(id o, const char *name) {
    cx_class_t *k = cx_class_named(name);
    return k && o && cx_is_subclass(object_getClass(o), k);
}
static void plist_xml(id o, char **b, unsigned long *n, unsigned long *cap, int depth) {
    char tmp[128], esc[512];
    if (!o || depth > 32) { pb_add(b, n, cap, "<string></string>"); return; }
    if (is_class(o, "NSString") || cx_is_str(o)) {
        cf_xml_escape(C(o), esc, (int)sizeof esc);
        pb_add(b, n, cap, "<string>"); pb_add(b, n, cap, esc); pb_add(b, n, cap, "</string>");
        return;
    }
    if (is_class(o, "NSNumber")) {
        typedef struct { void *isa; unsigned kind; unsigned pad; double d; long long i; } num_t;
        num_t *num = (num_t *)o;
        if (num->kind == 2) { pb_add(b, n, cap, num->i ? "<true/>" : "<false/>"); return; }
        if (num->kind == 1) snprintf(tmp, sizeof tmp, "<real>%g</real>", num->d);
        else snprintf(tmp, sizeof tmp, "<integer>%lld</integer>", num->i);
        pb_add(b, n, cap, tmp);
        return;
    }
    if (is_class(o, "NSData")) {
        long len = CFDataGetLength(o);
        const unsigned char *p = CFDataGetBytePtr(o);
        int i;
        pb_add(b, n, cap, "<data>");
        for (i = 0; i < len; i += 48) {
            char chunk[80];
            int m = (int)(len - i) > 48 ? 48 : (int)(len - i);
            cf_b64(p + i, m, chunk, (int)sizeof chunk);
            pb_add(b, n, cap, chunk);
        }
        pb_add(b, n, cap, "</data>");
        return;
    }
    if (is_class(o, "NSArray")) {
        long i, c = CFArrayGetCount(o);
        pb_add(b, n, cap, "<array>");
        for (i = 0; i < c; i++) plist_xml(CFArrayGetValueAtIndex(o, i), b, n, cap, depth + 1);
        pb_add(b, n, cap, "</array>");
        return;
    }
    if (is_class(o, "NSDictionary")) {
        nsdict_t *d = (nsdict_t *)o;
        unsigned long i;
        pb_add(b, n, cap, "<dict>");
        for (i = 0; i < d->n; i++) {
            cf_xml_escape(C(d->keys[i]), esc, (int)sizeof esc);
            pb_add(b, n, cap, "<key>"); pb_add(b, n, cap, esc); pb_add(b, n, cap, "</key>");
            plist_xml(d->vals[i], b, n, cap, depth + 1);
        }
        pb_add(b, n, cap, "</dict>");
        return;
    }
    pb_add(b, n, cap, "<string></string>");
}
static int bp_add_obj(cf_bpw *w, id o, int depth);
static int bp_add_obj(cf_bpw *w, id o, int depth) {
    if (!o || depth > 32) return cf_bpw_str(w, "");
    if (is_class(o, "NSString") || cx_is_str(o)) return cf_bpw_str(w, C(o));
    if (is_class(o, "NSNumber")) {
        typedef struct { void *isa; unsigned kind; unsigned pad; double d; long long i; } num_t;
        num_t *num = (num_t *)o;
        if (num->kind == 2) return cf_bpw_bool(w, num->i != 0);
        return cf_bpw_int(w, num->i);
    }
    if (is_class(o, "NSData")) return cf_bpw_data(w, CFDataGetBytePtr(o), (int)CFDataGetLength(o));
    if (is_class(o, "NSArray")) {
        long i, c = CFArrayGetCount(o);
        int *ix = c ? (int *)calloc((size_t)c, sizeof(int)) : 0;
        int id0 = 0, k;
        for (i = 0; i < c; i++) ix[i] = bp_add_obj(w, CFArrayGetValueAtIndex(o, i), depth + 1);
        id0 = cf_bpw_arr(w, ix, (int)c);
        free(ix);
        (void)k;
        return id0;
    }
    if (is_class(o, "NSDictionary")) {
        nsdict_t *d = (nsdict_t *)o;
        int n = (int)d->n, i;
        int *ks = n ? (int *)calloc((size_t)n, sizeof(int)) : 0;
        int *vs = n ? (int *)calloc((size_t)n, sizeof(int)) : 0;
        int id0;
        for (i = 0; i < n; i++) {
            ks[i] = cf_bpw_str(w, C(d->keys[i]));
            vs[i] = bp_add_obj(w, d->vals[i], depth + 1);
        }
        id0 = cf_bpw_dict(w, ks, vs, n);
        free(ks); free(vs);
        return id0;
    }
    return cf_bpw_str(w, "");
}
static id plist_bplist(id plist) {
    cf_bpw *w = cf_bpw_new();
    unsigned char *b = 0; int n = 0; int top;
    id d;
    if (!w) return 0;
    top = bp_add_obj(w, plist, 0);
    if (cf_bpw_finish(w, top, &b, &n)) { cf_bpw_free(w); return 0; }
    d = b ? CFDataCreate(0, b, n) : 0;
    free(b);
    cf_bpw_free(w);
    return d;
}
static id plist_data(id plist, long format) {
    if (format == 200) return plist_bplist(plist);
    char *b = 0; unsigned long n = 0, cap = 0;
    pb_add(&b, &n, &cap, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<plist version=\"1.0\">");
    plist_xml(plist, &b, &n, &cap, 0);
    pb_add(&b, &n, &cap, "</plist>\n");
    id d = b ? CFDataCreate(0, (const unsigned char *)b, (long)n) : 0;
    free(b);
    return d;
}
id CFPropertyListCreateData(void *alloc, id plist, long format, unsigned long opt, id *err) {
    (void)alloc; (void)opt;
    if (err) *err = 0;
    return plist_data(plist, format);
}
long CFPropertyListWrite(id plist, id stream, long format, unsigned long opt, id *err) {
    id data; const unsigned char *p; long n, w;
    (void)opt;
    if (err) *err = 0;
    data = plist_data(plist, format);
    if (!data) return 0;
    p = CFDataGetBytePtr(data);
    n = CFDataGetLength(data);
    w = stream ? MSG(long, stream, "write:maxLength:", p, (unsigned long)n) : -1;
    objc_release(data);
    return w < 0 ? 0 : w;
}
id CFPropertyListCreateWithStream(void *alloc, id stream, long len, unsigned long opt, long *fmt, id *err) {
    unsigned char *buf; long got = 0, cap, r;
    (void)alloc; (void)opt;
    if (err) *err = 0;
    if (fmt) *fmt = 100;
    if (!stream) return 0;
    MSG0(void, stream, "open");
    cap = len > 0 ? len : 4096;
    buf = malloc((size_t)cap + 1);
    if (!buf) return 0;
    for (;;) {
        if (len > 0 && got >= len) break;
        if (got + 1024 > cap) {
            unsigned char *nb = realloc(buf, (size_t)cap * 2 + 1);
            if (!nb) break;
            buf = nb; cap *= 2;
        }
        r = MSG(long, stream, "read:maxLength:", buf + got, (unsigned long)(cap - got));
        if (r <= 0) break;
        got += r;
        if (len > 0 && got >= len) break;
    }
    buf[got] = 0;
    id pl = cx_plist_parse(buf, (unsigned long)got);
    free(buf);
    return pl ? objc_retain(pl) : 0;
}
static id deep_one(id o, int leaves_mut) {
    (void)leaves_mut;
    if (!o) return 0;
    if (cx_is_str(o) || is_class(o, "NSString")) return S(C(o));
    if (is_class(o, "NSData")) return CFDataCreate(0, CFDataGetBytePtr(o), CFDataGetLength(o));
    if (is_class(o, "NSNumber")) {
        double d = MSG0(double, o, "doubleValue");
        return CFNumberCreate(0, 6, &d);
    }
    if (is_class(o, "NSArray")) {
        long i, c = CFArrayGetCount(o);
        id a = CFArrayCreateMutable(0, c, 0);
        for (i = 0; i < c; i++) {
            id v = deep_one(CFArrayGetValueAtIndex(o, i), leaves_mut);
            if (v) { CFArrayAppendValue(a, v); objc_release(v); }
        }
        return a;
    }
    if (is_class(o, "NSDictionary")) {
        nsdict_t *d = (nsdict_t *)o;
        id m = CFDictionaryCreateMutable(0, (long)d->n, 0, 0);
        unsigned long i;
        for (i = 0; i < d->n; i++) {
            id v = deep_one(d->vals[i], leaves_mut);
            if (v) { CFDictionarySetValue(m, d->keys[i], v); objc_release(v); }
        }
        return m;
    }
    return objc_retain(o);
}
id CFPropertyListCreateDeepCopy(void *alloc, id plist, unsigned long mut) {
    (void)alloc;
    return deep_one(plist, mut >= 2);
}
