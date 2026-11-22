/*
 * CoreFoundation que o OpenBot importava e openbot_gap.c devolvia 0.
 * Coleções que já existiam (CFSet, CFCharacterSet, CFLocale, CFUUID, timers) ficam nos arquivos delas.
 * Aqui: bag, calendário, fuso, formatadores, streams ligados, portas, soquete, plist, XML e o resto.
 */
#include "foundation.h"
#include "cf_public_core.inc"
extern const char kCFTypeArrayCallBacks[];   /* cfcollections.c: callbacks de objeto CF (retain/isEqual:) */
extern const char kCFTypeDictionaryKeyCallBacks[];   /* cfcollections.c: callbacks de objeto CF (retain/isEqual:) */
extern const char kCFTypeDictionaryValueCallBacks[];   /* cfcollections.c: callbacks de objeto CF (retain/isEqual:) */

void *calloc(unsigned long n, unsigned long s);
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
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
id CFBagCreateMutable(void *alloc, long cap, const void *cb) { (void)cap; return CFBagCreate(alloc, 0, 0, cb); }
void CFBagAddValue(id o, const void *v) { bag_o *b = as_bag(o); if (b) cf_bag_add(&b->bag, v); }
void CFBagRemoveValue(id o, const void *v) { bag_o *b = as_bag(o); if (b) cf_bag_remove(&b->bag, v); }
void CFBagRemoveAllValues(id o) { bag_o *b = as_bag(o); if (b) b->bag.n = 0; }
/* cada valor aparece tantas vezes quanto foi adicionado, como no CF */
void CFBagGetValues(id o, const void **out) {
    bag_o *b = as_bag(o);
    if (!b || !out) return;
    long k = 0;
    for (int i = 0; i < b->bag.n; i++) for (int j = 0; j < b->bag.s[i].n; j++) out[k++] = b->bag.s[i].p;
}
const void *CFBagGetValue(id o, const void *v) { return CFBagGetCountOfValue(o, v) ? v : 0; }

static void box_free_payload(void *p) {
    any_t *a = p;
    if (!a) return;
    if (a->k == K_BAG) free(((bag_o *)a)->bag.s);
    free(a);
}
/* CFTimeZone e CFCalendar são os NSTimeZone/NSCalendar de fdn_i18n.c */

/* ---- attributed string (é o NSAttributedString) ---- */
u64 CFAttributedStringGetTypeID(void) { return 31; }
id CFAttributedStringGetString(id a) { return a ? MSG0(id, a, "string") : 0; }
long CFAttributedStringGetLength(id a) { return CFStringGetLength(CFAttributedStringGetString(a)); }

/* CFNumberFormatter e CFDateFormatter são os NSNumberFormatter/NSDateFormatter de fdn_formatters.c */

/* CFFileDescriptor e CFSocket estão em fdn_cfsocket.c (com a thread que sinaliza o run loop) */
int connect(int fd, const void *addr, unsigned len);

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
unsigned char CFMessagePortIsRemote(id o) { mp_o *m = as_mp(o); return m && m->remote; }
id CFMessagePortGetName(id o) { mp_o *m = as_mp(o); return m && m->name[0] ? objc_autorelease(cx_str(m->name)) : 0; }
/* só a porta local muda de nome, e não para um nome já usado por outra porta local válida */
unsigned char CFMessagePortSetName(id o, id name) {
    mp_o *m = as_mp(o);
    const char *n = C(name);
    if (!m || m->remote || !m->valid || !n[0]) return 0;
    for (int i = 0; i < MP_N; i++) if (mp_tab[i] && mp_tab[i] != m && mp_tab[i]->valid && !mp_tab[i]->remote && !strcmp(mp_tab[i]->name, n)) return 0;
    memset(m->name, 0, sizeof m->name);
    memcpy(m->name, n, strlen(n) < 127 ? strlen(n) : 127);
    return 1;
}
typedef struct { long version; void *info; void *retain, *release, *desc; } mp_ctx_t;
void CFMessagePortGetContext(id o, mp_ctx_t *ctx) {
    mp_o *m = as_mp(o);
    if (!ctx) return;
    memset(ctx, 0, sizeof *ctx);
    if (m) ctx->info = m->info;
}
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

/* streams (CFReadStream/CFWriteStream, pares de socket e ligado): cfnetstream.c */

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
        id a = CFArrayCreateMutable(0, c, &kCFTypeArrayCallBacks);
        for (i = 0; i < c; i++) {
            id v = deep_one(CFArrayGetValueAtIndex(o, i), leaves_mut);
            if (v) { CFArrayAppendValue(a, v); objc_release(v); }
        }
        return a;
    }
    if (is_class(o, "NSDictionary")) {
        nsdict_t *d = (nsdict_t *)o;
        id m = CFDictionaryCreateMutable(0, (long)d->n, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
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
