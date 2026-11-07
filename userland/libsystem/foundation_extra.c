/*
 * mOS libSystem — classes da Foundation que o Electron/VS Code importa e que o mOS ainda não tinha
 * (inventário do tools/machocheck.c). Sem elas o carregador ligava a classe a NSObject e toda mensagem
 * caía no vazio:
 *   NSMutableData (subclasse do NSData do mOS {isa, bytes, len} + capacidade), NSTimeZone (UTC, como o
 *   resto do mOS), NSCalendar/NSDateComponents (gregoriano), NSEnumerator, NSOrderedSet/NSMutableOrderedSet
 *   (sobre um NSMutableArray, com encaminhamento do resto), NSMethodSignature/NSInvocation (invoke pela
 *   ABI SysV: inteiros e ponteiros nos registradores de inteiros, float/double nos XMM, até 6 + 8) e o
 *   typeinfo OBJC_EHTYPE_$_NSException para @catch (NSException *).
 */
#include "foundation.h"

void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void *realloc(void *p, size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memmove(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
double floor(double x);
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
const char *NSGetSizeAndAlignment(const char *type, u64 *size, u64 *align);

#define KLASS(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
KLASS(NSMutableData) KLASS(NSTimeZone) KLASS(NSCalendar) KLASS(NSDateComponents) KLASS(NSEnumerator)
KLASS(NSOrderedSet) KLASS(NSMutableOrderedSet) KLASS(NSMethodSignature) KLASS(NSInvocation)
static id cls(const char *n) { return (id)cx_class_named(n); }
typedef struct { long loc, len; } ns_range_t;

/* ================================================================ NSMutableData */
typedef struct { cx_class_t *isa; u8 *bytes; u64 len; u64 cap; } mdata_t;
static void md_reserve(mdata_t *d, u64 n) {
    if (n <= d->cap && d->bytes) return;
    u64 c = d->cap ? d->cap : 16;
    while (c < n) c *= 2;
    u8 *b = realloc(d->bytes, c);
    if (!b) return;
    d->bytes = b;
    d->cap = c;
}
static id md_new(id k, u64 cap) {
    mdata_t *d = (mdata_t *)cx_alloc_instance((cx_class_t *)k);
    md_reserve(d, cap ? cap : 1);
    return (id)d;
}
static id md_data(id k, SEL c) { (void)c; return objc_autorelease(md_new(k, 0)); }
static id md_with_capacity(id k, SEL c, u64 n) { (void)c; return objc_autorelease(md_new(k, n)); }
static id md_with_length(id k, SEL c, u64 n) { (void)c; mdata_t *d = (mdata_t *)md_new(k, n); memset(d->bytes, 0, n); d->len = n; return objc_autorelease((id)d); }
static id md_with_bytes(id k, SEL c, const void *b, u64 n) { (void)c; mdata_t *d = (mdata_t *)md_new(k, n); if (n) memcpy(d->bytes, b, n); d->len = n; return objc_autorelease((id)d); }
static id md_with_data(id k, SEL c, id src) {
    return md_with_bytes(k, c, src ? MSG0(const void *, src, "bytes") : 0, src ? MSG0(u64, src, "length") : 0);
}
static id md_init(id self, SEL c) { (void)c; md_reserve((mdata_t *)self, 1); return self; }
static id md_init_capacity(id self, SEL c, u64 n) { (void)c; md_reserve((mdata_t *)self, n ? n : 1); return self; }
static id md_init_length(id self, SEL c, u64 n) { (void)c; mdata_t *d = (mdata_t *)self; md_reserve(d, n ? n : 1); memset(d->bytes, 0, n); d->len = n; return self; }
static id md_init_bytes(id self, SEL c, const void *b, u64 n) { (void)c; mdata_t *d = (mdata_t *)self; md_reserve(d, n ? n : 1); if (n) memcpy(d->bytes, b, n); d->len = n; return self; }
static id md_init_data(id self, SEL c, id src) { return md_init_bytes(self, c, src ? MSG0(const void *, src, "bytes") : 0, src ? MSG0(u64, src, "length") : 0); }
static void *md_mbytes(id self, SEL c) { (void)c; mdata_t *d = (mdata_t *)self; md_reserve(d, 1); return d->bytes; }
static void md_set_length(id self, SEL c, u64 n) {
    (void)c;
    mdata_t *d = (mdata_t *)self;
    md_reserve(d, n ? n : 1);
    if (n > d->len) memset(d->bytes + d->len, 0, n - d->len);
    d->len = n;
}
static void md_increase(id self, SEL c, u64 n) { md_set_length(self, c, ((mdata_t *)self)->len + n); }
static void md_append_bytes(id self, SEL c, const void *b, u64 n) {
    (void)c;
    mdata_t *d = (mdata_t *)self;
    if (!n) return;
    md_reserve(d, d->len + n);
    memmove(d->bytes + d->len, b, n);
    d->len += n;
}
static void md_append_data(id self, SEL c, id o) { if (o) md_append_bytes(self, c, MSG0(const void *, o, "bytes"), MSG0(u64, o, "length")); }
static void md_set_data(id self, SEL c, id o) { ((mdata_t *)self)->len = 0; md_append_data(self, c, o); }
static void md_replace_len(id self, SEL c, ns_range_t r, const void *b, u64 n) {
    (void)c;
    mdata_t *d = (mdata_t *)self;
    u64 lo = (u64)r.loc, hi = lo + (u64)r.len;
    if (lo > d->len) lo = d->len;
    if (hi > d->len) hi = d->len;
    u64 nl = d->len - (hi - lo) + n;
    md_reserve(d, nl ? nl : 1);
    memmove(d->bytes + lo + n, d->bytes + hi, d->len - hi);
    if (n) memcpy(d->bytes + lo, b, n);
    d->len = nl;
}
static void md_replace(id self, SEL c, ns_range_t r, const void *b) { md_replace_len(self, c, r, b, (u64)r.len); }
static void md_reset(id self, SEL c, ns_range_t r) {
    (void)c;
    mdata_t *d = (mdata_t *)self;
    if ((u64)(r.loc + r.len) > d->len) md_set_length(self, c, (u64)(r.loc + r.len));
    memset(d->bytes + r.loc, 0, (u64)r.len);
}
static id md_copy(id self, SEL c) { (void)c; mdata_t *d = (mdata_t *)self; return objc_retain(MSG(id, cls("NSData"), "dataWithBytes:length:", d->bytes ? d->bytes : (u8 *)"", d->len)); }
static id md_mcopy(id self, SEL c) { (void)c; mdata_t *d = (mdata_t *)self; return objc_retain(md_with_bytes((id)&NSMutableData_c, 0, d->bytes, d->len)); }
static void md_dealloc(id self, SEL c) {
    (void)c;
    free(((mdata_t *)self)->bytes);
    ((mdata_t *)self)->bytes = 0;
    IMP sup = cx_lookup(cx_class_named("NSObject"), sel_registerName("dealloc"));
    if (sup) sup(self, sel_registerName("dealloc"));
}

/* ================================================================ NSTimeZone (UTC) */
typedef struct { cx_class_t *isa; id name; i64 offset; } tz_t;
static id tz_make(const char *name, i64 off) {
    tz_t *t = (tz_t *)cx_alloc_instance(&NSTimeZone_c);
    t->name = objc_retain(cx_str(name));
    t->offset = off;
    return (id)t;
}
static id tz_utc(void) { static id u; if (!u) u = tz_make("GMT", 0); return u; }
static id tz_local(id k, SEL c) { (void)k; (void)c; return tz_utc(); }
static id tz_named(id k, SEL c, id name) {
    (void)k; (void)c;
    const char *n = name ? cx_cstr(name) : "GMT";
    return objc_autorelease(tz_make(n, 0));
}
static id tz_for_seconds(id k, SEL c, i64 s) {
    (void)k; (void)c;
    char n[16];
    long h = (long)(s / 3600), m = (long)((s < 0 ? -s : s) % 3600 / 60);
    snprintf(n, sizeof n, "GMT%+03ld%02ld", h, m);
    return objc_autorelease(tz_make(s ? n : "GMT", s));
}
static void tz_reset(id k, SEL c) { (void)k; (void)c; }
static id tz_name(id self, SEL c) { (void)c; return ((tz_t *)self)->name; }
static id tz_abbrev(id self, SEL c) { (void)c; return ((tz_t *)self)->offset ? ((tz_t *)self)->name : cx_str("GMT"); }
static i64 tz_seconds(id self, SEL c) { (void)c; return ((tz_t *)self)->offset; }
static i64 tz_seconds_for(id self, SEL c, id date) { (void)date; return tz_seconds(self, c); }
static BOOL tz_dst(id self, SEL c) { (void)self; (void)c; return NO; }
static id tz_known(id k, SEL c) { (void)k; (void)c; id a = cx_arr_new(0); cx_arr_add(a, cx_str("GMT")); cx_arr_add(a, cx_str("America/Sao_Paulo")); return objc_autorelease(a); }

/* ================================================================ calendário gregoriano */
static void civil_from_days(i64 z, i64 *y, int *m, int *d) {
    z += 719468;
    i64 era = (z >= 0 ? z : z - 146096) / 146097;
    u64 doe = (u64)(z - era * 146097);
    u64 yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    i64 yy = (i64)yoe + era * 400;
    u64 doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    u64 mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y = yy + (*m <= 2);
}
static i64 days_from_civil(i64 y, int m, int d) {
    y -= m <= 2;
    i64 era = (y >= 0 ? y : y - 399) / 400;
    u64 yoe = (u64)(y - era * 400);
    u64 doy = (u64)((153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1);
    u64 doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + (i64)doe - 719468;
}
#define NSUndefinedDateComponent 0x7fffffffffffffffLL
typedef struct { cx_class_t *isa; i64 era, year, month, day, hour, minute, second, nanosecond, weekday, weekOfYear, yearForWeek; id tz; } dc_t;
static id dc_alloc_init(id self, SEL c) {
    (void)c;
    dc_t *d = (dc_t *)self;
    d->era = d->year = d->month = d->day = d->hour = d->minute = d->second = d->nanosecond = d->weekday = d->weekOfYear = d->yearForWeek = NSUndefinedDateComponent;
    return self;
}
#define DC_PROP(field, getter, setter) \
    static i64 dc_get_##field(id self, SEL c) { (void)c; return ((dc_t *)self)->field; } \
    static void dc_set_##field(id self, SEL c, i64 v) { (void)c; ((dc_t *)self)->field = v; }
DC_PROP(era, era, setEra) DC_PROP(year, year, setYear) DC_PROP(month, month, setMonth) DC_PROP(day, day, setDay)
DC_PROP(hour, hour, setHour) DC_PROP(minute, minute, setMinute) DC_PROP(second, second, setSecond)
DC_PROP(nanosecond, nanosecond, setNanosecond) DC_PROP(weekday, weekday, setWeekday)
DC_PROP(weekOfYear, weekOfYear, setWeekOfYear) DC_PROP(yearForWeek, yearForWeekOfYear, setYearForWeekOfYear)
static id dc_tz(id self, SEL c) { (void)c; return ((dc_t *)self)->tz; }
static void dc_set_tz(id self, SEL c, id tz) { (void)c; ((dc_t *)self)->tz = tz ? objc_retain(tz) : 0; }

typedef struct { cx_class_t *isa; id ident; id tz; } cal_t;
static id cal_make(const char *ident) {
    cal_t *k = (cal_t *)cx_alloc_instance(&NSCalendar_c);
    k->ident = objc_retain(cx_str(ident));
    k->tz = tz_utc();
    return (id)k;
}
static id cal_current(id k, SEL c) { (void)k; (void)c; static id g; if (!g) g = cal_make("gregorian"); return g; }
static id cal_with_ident(id k, SEL c, id ident) { (void)k; (void)c; return objc_autorelease(cal_make(ident ? cx_cstr(ident) : "gregorian")); }
static id cal_init_ident(id self, SEL c, id ident) { (void)c; ((cal_t *)self)->ident = objc_retain(ident ? ident : cx_str("gregorian")); ((cal_t *)self)->tz = tz_utc(); return self; }
static id cal_ident(id self, SEL c) { (void)c; return ((cal_t *)self)->ident; }
static id cal_get_tz(id self, SEL c) { (void)c; return ((cal_t *)self)->tz; }
static void cal_set_tz(id self, SEL c, id tz) { (void)c; ((cal_t *)self)->tz = tz ? objc_retain(tz) : tz_utc(); }
static u64 cal_first_weekday(id self, SEL c) { (void)self; (void)c; return 1; }
static i64 cal_offset(id self) { cal_t *k = (cal_t *)self; return k->tz ? MSG0(i64, k->tz, "secondsFromGMT") : 0; }
static id cal_components(id self, SEL c, u64 units, id date) {
    (void)c; (void)units;
    double t = date ? MSG0(double, date, "timeIntervalSince1970") : 0;
    t += (double)cal_offset(self);
    double fl = floor(t);
    i64 secs = (i64)fl;
    i64 days = secs >= 0 ? secs / 86400 : -((-secs + 86399) / 86400);
    i64 rem = secs - days * 86400;
    dc_t *d = (dc_t *)dc_alloc_init(cx_alloc_instance(&NSDateComponents_c), 0);
    int m, dd;
    civil_from_days(days, &d->year, &m, &dd);
    d->era = 1; d->month = m; d->day = dd;
    d->hour = rem / 3600; d->minute = rem % 3600 / 60; d->second = rem % 60;
    d->nanosecond = (i64)((t - fl) * 1e9);
    d->weekday = ((days % 7) + 7 + 4) % 7 + 1;       /* 1970-01-01 foi quinta (5); domingo = 1 */
    return objc_autorelease((id)d);
}
static id cal_components_tz(id self, SEL c, id tz, id date) { (void)tz; return cal_components(self, c, ~0ull, date); }
static id cal_date_from(id self, SEL c, id comps) {
    (void)c;
    dc_t *d = (dc_t *)comps;
    i64 y = d->year == NSUndefinedDateComponent ? 2001 : d->year;
    i64 mo = d->month == NSUndefinedDateComponent ? 1 : d->month;
    i64 dy = d->day == NSUndefinedDateComponent ? 1 : d->day;
    i64 h = d->hour == NSUndefinedDateComponent ? 0 : d->hour;
    i64 mi = d->minute == NSUndefinedDateComponent ? 0 : d->minute;
    i64 s = d->second == NSUndefinedDateComponent ? 0 : d->second;
    /* meses fora de 1..12 normalizam como no macOS */
    y += (mo - 1) >= 0 ? (mo - 1) / 12 : -((12 - mo) / 12);
    mo = ((mo - 1) % 12 + 12) % 12 + 1;
    double t = (double)(days_from_civil(y, (int)mo, 1) + dy - 1) * 86400.0 + (double)(h * 3600 + mi * 60 + s);
    if (d->nanosecond != NSUndefinedDateComponent) t += (double)d->nanosecond / 1e9;
    t -= (double)cal_offset(self);
    return MSG(id, cls("NSDate"), "dateWithTimeIntervalSince1970:", t);
}
static id cal_by_adding(id self, SEL c, id comps, id date, u64 opts) {
    (void)opts;
    id now = cal_components(self, c, ~0ull, date);
    dc_t *a = (dc_t *)comps, *b = (dc_t *)now;
#define ADD(f) if (a->f != NSUndefinedDateComponent) b->f += a->f;
    ADD(year) ADD(month) ADD(day) ADD(hour) ADD(minute) ADD(second)
#undef ADD
    return cal_date_from(self, c, now);
}
static i64 cal_component(id self, SEL c, u64 unit, id date) {
    dc_t *d = (dc_t *)cal_components(self, c, ~0ull, date);
    switch (unit) {
    case 2: return d->era; case 4: return d->year; case 8: return d->month; case 16: return d->day;
    case 32: return d->hour; case 64: return d->minute; case 128: return d->second; case 512: return d->weekday;
    case 32768: return d->nanosecond;
    }
    return 0;
}

/* ================================================================ NSEnumerator */
typedef struct { cx_class_t *isa; id items; u64 i; } en_t;
id cx_enumerator_new(id array) {
    en_t *e = (en_t *)cx_alloc_instance(&NSEnumerator_c);
    e->items = array ? objc_retain(MSG0(id, array, "copy")) : 0;
    return objc_autorelease((id)e);
}
static id en_next(id self, SEL c) {
    (void)c;
    en_t *e = (en_t *)self;
    if (!e->items || e->i >= MSG0(u64, e->items, "count")) return 0;
    return MSG(id, e->items, "objectAtIndex:", e->i++);
}
static id en_all(id self, SEL c) {
    (void)c;
    en_t *e = (en_t *)self;
    id a = cx_arr_new(0);
    u64 n = e->items ? MSG0(u64, e->items, "count") : 0;
    for (; e->i < n; e->i++) cx_arr_add(a, MSG(id, e->items, "objectAtIndex:", e->i));
    return objc_autorelease(a);
}
/* fast enumeration: {state, itemsPtr, mutationsPtr, extra[5]} */
typedef struct { u64 state; id *items; u64 *mutations; u64 extra[5]; } fe_state_t;
static u64 en_fast(id self, SEL c, fe_state_t *st, id *buf, u64 len) {
    (void)c;
    u64 k = 0;
    id o;
    while (k < len && (o = en_next(self, 0))) buf[k++] = o;
    st->items = buf;
    st->mutations = &st->extra[0];
    return k;
}

/* ---- NSDirectoryEnumerator: -[NSFileManager enumeratorAtURL:includingPropertiesForKeys:options:errorHandler:] e
 * -enumeratorAtPath:. A árvore é lida inteira na criação (pré-ordem; diretórios antes do conteúdo), como uma lista para o
 * NSEnumerator. Opções: SkipsSubdirectoryDescendants (1), SkipsHiddenFiles (4). -skipDescendants ainda não poda. */
KLASS(NSDirectoryEnumerator)
KLASS(NSCache)
int cx_snprintf(char *buf, unsigned long n, const char *fmt, ...);
static void de_walk(id out, const char *dir, const char *rel, u64 opts, int asurl, int depth) {
    if (depth > 64) return;
    void *d = opendir(dir);
    if (!d) return;
    u8 *e;
    while ((e = readdir(d))) {
        const char *nm = (const char *)e + 21;           /* o nome no dirent do mOS (como o fm_contents) */
        if (!strcmp(nm, ".") || !strcmp(nm, "..")) continue;
        if ((opts & 4) && nm[0] == '.') continue;
        char full[1024], r[1024];
        cx_snprintf(full, sizeof full, "%s/%s", dir, nm);
        cx_snprintf(r, sizeof r, rel[0] ? "%s/%s" : "%s%s", rel, nm);
        cx_arr_add(out, asurl ? MSG(id, cls("NSURL"), "fileURLWithPath:", cx_str(full)) : cx_str(r));
        if (!(opts & 1)) {
            void *sub = opendir(full);
            if (sub) { closedir(sub); de_walk(out, full, r, opts, asurl, depth + 1); }
        }
    }
    closedir(d);
}
static id de_new(const char *root, u64 opts, int asurl) {
    id items = cx_arr_new(0);
    de_walk(items, root, "", opts, asurl, 0);
    en_t *e = (en_t *)cx_alloc_instance(&NSDirectoryEnumerator_c);
    e->items = items;
    return objc_autorelease((id)e);
}
static id fm_enum_url(id self, SEL c, id url, id keys, u64 opts, void *handler) {
    (void)self; (void)c; (void)keys; (void)handler;
    id p = url ? MSG0(id, url, "path") : 0;
    return p ? de_new(cx_cstr(p), opts, 1) : 0;
}
static id fm_enum_path(id self, SEL c, id path) { (void)self; (void)c; return path ? de_new(cx_cstr(path), 0, 0) : 0; }
/* -createDirectoryAtURL:withIntermediateDirectories:attributes:error: (o WhatsApp cria o contêiner na partida) */
static BOOL fm_mkdir_url(id self, SEL c, id url, BOOL inter, id attrs, id *err) {
    (void)c;
    id p = url ? MSG0(id, url, "path") : 0;
    if (!p) { if (err) *err = 0; return NO; }
    return MSG(BOOL, self, "createDirectoryAtPath:withIntermediateDirectories:attributes:error:", p, inter, attrs, err);
}
static void de_skip(id self, SEL c) { (void)self; (void)c; }
static u64 de_level(id self, SEL c) { (void)self; (void)c; return 0; }

/* ================================================================ NSOrderedSet / NSMutableOrderedSet */
typedef struct { cx_class_t *isa; id arr; } os_t;
static id os_arr(id self) { os_t *o = (os_t *)self; if (!o->arr) o->arr = objc_retain(cx_arr_new(0)); return o->arr; }
static void os_add(id self, SEL c, id v) { (void)c; if (v && !MSG(BOOL, os_arr(self), "containsObject:", v)) MSG(void, os_arr(self), "addObject:", v); }
static id os_new_from(id k, id src) {
    id s = cx_alloc_instance((cx_class_t *)k);
    u64 n = src ? MSG0(u64, src, "count") : 0;
    for (u64 i = 0; i < n; i++) os_add(s, 0, MSG(id, src, "objectAtIndex:", i));
    if (!n) os_arr(s);
    return s;
}
static id os_empty(id k, SEL c) { (void)c; return objc_autorelease(os_new_from(k, 0)); }
static id os_with_array(id k, SEL c, id a) { (void)c; return objc_autorelease(os_new_from(k, a)); }
static id os_with_object(id k, SEL c, id o) { (void)c; id s = os_new_from(k, 0); os_add(s, 0, o); return objc_autorelease(s); }
static id os_init(id self, SEL c) { (void)c; os_arr(self); return self; }
static id os_init_array(id self, SEL c, id a) {
    (void)c;
    u64 n = a ? MSG0(u64, a, "count") : 0;
    for (u64 i = 0; i < n; i++) os_add(self, 0, MSG(id, a, "objectAtIndex:", i));
    os_arr(self);
    return self;
}
static id os_array(id self, SEL c) { (void)c; return MSG0(id, os_arr(self), "copy"); }
static id os_set(id self, SEL c) { (void)c; return MSG(id, cls("NSSet"), "setWithArray:", os_arr(self)); }
static void os_insert(id self, SEL c, id v, u64 i) { (void)c; if (v && !MSG(BOOL, os_arr(self), "containsObject:", v)) MSG(void, os_arr(self), "insertObject:atIndex:", v, i); }
static void os_add_array(id self, SEL c, id a) { u64 n = a ? MSG0(u64, a, "count") : 0; for (u64 i = 0; i < n; i++) os_add(self, c, MSG(id, a, "objectAtIndex:", i)); }
static id os_copy(id self, SEL c) { (void)c; return os_new_from((id)&NSOrderedSet_c, os_arr(self)); }
static id os_mcopy(id self, SEL c) { (void)c; return os_new_from((id)&NSMutableOrderedSet_c, os_arr(self)); }
static id os_enum(id self, SEL c) { (void)c; return cx_enumerator_new(os_arr(self)); }
static BOOL os_is_equal(id self, SEL c, id o) {
    (void)c;
    if (self == o) return YES;
    if (!o || !cx_is_subclass(object_getClass(o), &NSOrderedSet_c)) return NO;
    return MSG(BOOL, os_arr(self), "isEqualToArray:", os_arr(o));
}
/* o resto (count, objectAtIndex:, indexOfObject:, containsObject:, removeObject:, fast enumeration...)
 * vai para o array */
static id os_forward(id self, SEL c, SEL s) { (void)c; (void)s; return os_arr(self); }

/* ================================================================ NSMethodSignature / NSInvocation */
#define SIG_MAX 16
typedef struct { cx_class_t *isa; char *types; u32 nargs; const char *arg[SIG_MAX]; const char *ret; u64 retlen; u64 frame; } msig_t;
static const char *skip_quals(const char *t) { while (*t && (*t == 'r' || *t == 'n' || *t == 'N' || *t == 'o' || *t == 'O' || *t == 'R' || *t == 'V')) t++; return t; }
static const char *next_type(const char *t) {
    u64 s, a;
    t = NSGetSizeAndAlignment(t, &s, &a);
    while (*t == '+' || *t == '-' || (*t >= '0' && *t <= '9')) t++;   /* offsets dos tipos antigos */
    return t;
}
static id sig_with_types(id k, SEL c, const char *types) {
    (void)k; (void)c;
    if (!types) return 0;
    msig_t *m = (msig_t *)cx_alloc_instance(&NSMethodSignature_c);
    size_t n = strlen(types);
    m->types = malloc(n + 1);
    memcpy(m->types, types, n + 1);
    const char *t = m->types;
    m->ret = skip_quals(t);
    u64 s, a;
    NSGetSizeAndAlignment(m->ret, &s, &a);
    m->retlen = s;
    t = next_type(t);
    while (*t && m->nargs < SIG_MAX) { m->arg[m->nargs++] = skip_quals(t); t = next_type(t); }
    m->frame = 8ull * m->nargs;
    return objc_autorelease((id)m);
}
static u64 sig_nargs(id self, SEL c) { (void)c; return ((msig_t *)self)->nargs; }
static const char *sig_arg(id self, SEL c, u64 i) { (void)c; msig_t *m = (msig_t *)self; return i < m->nargs ? m->arg[i] : 0; }
static const char *sig_ret(id self, SEL c) { (void)c; return ((msig_t *)self)->ret; }
static u64 sig_retlen(id self, SEL c) { (void)c; return ((msig_t *)self)->retlen; }
static u64 sig_frame(id self, SEL c) { (void)c; return ((msig_t *)self)->frame; }
static BOOL sig_oneway(id self, SEL c) { (void)c; return ((msig_t *)self)->types[0] == 'V'; }

typedef struct { cx_class_t *isa; id sig; u8 args[SIG_MAX][32]; u8 ret[32]; BOOL retained; } inv_t;
static id inv_with_sig(id k, SEL c, id sig) {
    (void)k; (void)c;
    if (!sig) return 0;
    inv_t *v = (inv_t *)cx_alloc_instance(&NSInvocation_c);
    v->sig = objc_retain(sig);
    return objc_autorelease((id)v);
}
static id inv_sig(id self, SEL c) { (void)c; return ((inv_t *)self)->sig; }
static u64 arg_size(inv_t *v, u64 i) {
    msig_t *m = (msig_t *)v->sig;
    u64 s = 8, a;
    if (i < m->nargs) NSGetSizeAndAlignment(m->arg[i], &s, &a);
    return s > 32 ? 32 : s;
}
static void inv_set_arg(id self, SEL c, const void *p, u64 i) { (void)c; inv_t *v = (inv_t *)self; if (i < SIG_MAX && p) memcpy(v->args[i], p, arg_size(v, i)); }
static void inv_get_arg(id self, SEL c, void *p, u64 i) { (void)c; inv_t *v = (inv_t *)self; if (i < SIG_MAX && p) memcpy(p, v->args[i], arg_size(v, i)); }
static void inv_set_target(id self, SEL c, id t) { (void)c; memcpy(((inv_t *)self)->args[0], &t, 8); }
static id inv_target(id self, SEL c) { (void)c; id t; memcpy(&t, ((inv_t *)self)->args[0], 8); return t; }
static void inv_set_sel(id self, SEL c, SEL s) { (void)c; memcpy(((inv_t *)self)->args[1], &s, 8); }
static SEL inv_sel(id self, SEL c) { (void)c; SEL s; memcpy(&s, ((inv_t *)self)->args[1], 8); return s; }
static void inv_get_ret(id self, SEL c, void *p) { (void)c; inv_t *v = (inv_t *)self; if (p) memcpy(p, v->ret, ((msig_t *)v->sig)->retlen > 32 ? 32 : ((msig_t *)v->sig)->retlen); }
static void inv_set_ret(id self, SEL c, const void *p) { (void)c; inv_t *v = (inv_t *)self; if (p) memcpy(v->ret, p, ((msig_t *)v->sig)->retlen > 32 ? 32 : ((msig_t *)v->sig)->retlen); }
static void inv_retain_args(id self, SEL c) { (void)c; ((inv_t *)self)->retained = YES; }
typedef struct { u64 a, b; } ret2_t;
typedef ret2_t (*int_fn_t)(u64, u64, u64, u64, u64, u64, double, double, double, double, double, double, double, double);
typedef double (*dbl_fn_t)(u64, u64, u64, u64, u64, u64, double, double, double, double, double, double, double, double);
typedef float (*flt_fn_t)(u64, u64, u64, u64, u64, u64, double, double, double, double, double, double, double, double);
static int is_float_type(const char *t) { return *t == 'f' || *t == 'd'; }
static void inv_invoke_imp(inv_t *v, IMP imp) {
    msig_t *m = (msig_t *)v->sig;
    u64 ig[6] = {0, 0, 0, 0, 0, 0};
    double fx[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int ni = 0, nf = 0;
    int sret = m->retlen > 16 && (*m->ret == '{' || *m->ret == '(');
    if (sret) ig[ni++] = (u64)(usize)v->ret;
    for (u32 i = 0; i < m->nargs; i++) {
        const char *t = m->arg[i];
        if (is_float_type(t)) {
            double d;
            if (*t == 'f') { float f; memcpy(&f, v->args[i], 4); u64 bits = 0; memcpy(&bits, &f, 4); memcpy(&d, &bits, 8); }
            else memcpy(&d, v->args[i], 8);
            if (nf < 8) fx[nf++] = d;
        } else {
            u64 s = arg_size(v, i);
            for (u64 off = 0; off < s && ni < 6; off += 8) { u64 w = 0; memcpy(&w, v->args[i] + off, s - off < 8 ? s - off : 8); ig[ni++] = w; }
        }
    }
    if (*m->ret == 'd') { double r = ((dbl_fn_t)(void *)imp)(ig[0], ig[1], ig[2], ig[3], ig[4], ig[5], fx[0], fx[1], fx[2], fx[3], fx[4], fx[5], fx[6], fx[7]); memcpy(v->ret, &r, 8); }
    else if (*m->ret == 'f') { float r = ((flt_fn_t)(void *)imp)(ig[0], ig[1], ig[2], ig[3], ig[4], ig[5], fx[0], fx[1], fx[2], fx[3], fx[4], fx[5], fx[6], fx[7]); memcpy(v->ret, &r, 4); }
    else { ret2_t r = ((int_fn_t)(void *)imp)(ig[0], ig[1], ig[2], ig[3], ig[4], ig[5], fx[0], fx[1], fx[2], fx[3], fx[4], fx[5], fx[6], fx[7]); if (!sret) { memcpy(v->ret, &r.a, 8); memcpy(v->ret + 8, &r.b, 8); } }
}
static void inv_invoke(id self, SEL c) {
    (void)c;
    inv_t *v = (inv_t *)self;
    id target = inv_target(self, 0);
    SEL s = inv_sel(self, 0);
    if (!target || !s) { memset(v->ret, 0, sizeof v->ret); return; }
    IMP imp = cx_lookup(object_getClass(target), s);
    if (!imp) { memset(v->ret, 0, sizeof v->ret); return; }
    inv_invoke_imp(v, imp);
}
static void inv_invoke_with(id self, SEL c, id target) { inv_set_target(self, c, target); inv_invoke(self, c); }

/* ================================================================ typeinfo de @catch (NSException *) */
extern void *objc_ehtype_vtable[];
struct { void **vtable; const char *name; cx_class_t *cls; } OBJC_EHTYPE_NSException __asm__("OBJC_EHTYPE_$_NSException") = {0, "NSException", 0};

/* ================================================================ registro */
#define IM(k, s, f) cx_add_method(&k##_c, s, (void *)(f))
#define CM_(k, s, f) cx_add_cmethod(&k##_c, s, (void *)(f))
/* -[NSString initWithBytesNoCopy:length:encoding:freeWhenDone:]: copia (o NSString do mOS guarda os próprios bytes) */
static id s_init_nocopy(id self, SEL c, void *b, u64 n, u64 enc, BOOL freeit) {
    (void)c;
    id r = MSG(id, self, "initWithBytes:length:encoding:", (const void *)b, n, enc);
    if (freeit && b) free(b);
    return r;
}
/* -[NSArray initWithObjects:...] (lista terminada em nil) */
static id a_init_objs_va(id self, SEL c, id first, ...) {
    (void)c;
    id buf[256]; u64 n = 0;
    va_list ap; va_start(ap, first);
    for (id o = first; o && n < 256; o = va_arg(ap, id)) buf[n++] = o;
    va_end(ap);
    return MSG(id, self, "initWithObjects:count:", (id *)buf, n);
}
/* contêiner de App Group (Catalyst/sandbox): ~/Library/Group Containers/<grupo>, criado na primeira vez */
id NSHomeDirectory(void);
static id fm_group_container(id self, SEL c, id group) {
    (void)c;
    if (!group) return 0;
    char p[1024];
    cx_snprintf(p, sizeof p, "%s/Library/Group Containers/%s", cx_cstr(NSHomeDirectory()), cx_cstr(group));
    MSG(BOOL, self, "createDirectoryAtPath:withIntermediateDirectories:attributes:error:", cx_str(p), (BOOL)1, (id)0, (id *)0);
    return MSG(id, cls("NSURL"), "fileURLWithPath:", cx_str(p));
}
static id th_ret_addrs(id self, SEL c) { (void)self; (void)c; return MSG0(id, cls("NSArray"), "array"); }
static id th_name(id self, SEL c) { (void)c; return cx_str(MSG0(BOOL, self, "isMainThread") ? "main" : ""); }
static void th_set_name(id self, SEL c, id n) { (void)self; (void)c; (void)n; }
static id pd_with_length(id k, SEL c, u64 n) { (void)k; return md_with_length(cls("NSMutableData"), c, n); }
static id pd_with_capacity(id k, SEL c, u64 n) { (void)k; (void)n; return md_with_length(cls("NSMutableData"), c, 0); }
static BOOL pd_yes(id self, SEL c) { (void)self; (void)c; return 1; }
static BOOL pd_no(id self, SEL c) { (void)self; (void)c; return 0; }
static void pd_noop(id self, SEL c) { (void)self; (void)c; }
/* ---- lacunas da partida do WhatsApp (Catalyst): preferências, locale, bundle, caminhos */
static id s_data_enc(id self, SEL c, u64 enc) {
    (void)c;
    id D = cls("NSData");
    if (enc == 10 || enc == 0x94000100ul || enc == 0x90000100ul) {         /* UTF-16 (Unicode: BOM + nativo; LE; BE) */
        u64 n = MSG0(u64, self, "length"), bom = enc == 10 ? 1 : 0;
        unsigned short *b = malloc((n + bom) * 2 + 2);
        if (!b) return 0;
        if (bom) b[0] = 0xFEFF;
        for (u64 i = 0; i < n; i++) {
            unsigned short ch = MSG(unsigned short, self, "characterAtIndex:", i);
            b[i + bom] = enc == 0x90000100ul ? (unsigned short)((ch >> 8) | (ch << 8)) : ch;
        }
        id r = MSG(id, D, "dataWithBytes:length:", (const void *)b, (n + bom) * 2);
        free(b);
        return r;
    }
    const char *u = MSG0(const char *, self, "UTF8String");
    return MSG(id, D, "dataWithBytes:length:", (const void *)(u ? u : ""), (u64)(u ? strlen(u) : 0));
}
static id s_data_enc_lossy(id self, SEL c, u64 enc, BOOL lossy) { (void)lossy; return s_data_enc(self, c, enc); }
static id s_append_ext(id self, SEL c, id ext) {
    (void)c;
    if (!ext || !MSG0(u64, ext, "length")) return self;
    char b[2048];
    cx_snprintf(b, sizeof b, "%s.%s", cx_cstr(self), cx_cstr(ext));
    return cx_str(b);
}
static id a_adding_array(id self, SEL c, id other) {
    (void)c;
    id m = MSG0(id, self, "mutableCopy");
    if (other) MSG(void, m, "addObjectsFromArray:", other);
    return objc_autorelease(m);
}
static id n_with_ulong(id k, SEL c, u64 v) { (void)c; return MSG(id, k, "numberWithUnsignedLongLong:", v); }
static id d_init_cap(id self, SEL c, u64 n) { (void)c; (void)n; return MSG0(id, self, "init"); }
static void ku_set_class(id self, SEL c, id k, id name) { (void)self; (void)c; (void)k; (void)name; }
static id b_localizations(id self, SEL c) { (void)c; return MSG0(id, self, "preferredLocalizations"); }
static id loc_with_ident(id k, SEL c, id ident) { (void)c; (void)ident; return MSG0(id, k, "currentLocale"); }
static id loc_init_ident(id self, SEL c, id ident) { (void)c; (void)ident; id r = MSG0(id, (id)self->isa, "currentLocale"); return objc_retain(r); }
id NSSearchPathForDirectoriesInDomains(unsigned long dir, unsigned long mask, unsigned char expand);
static id fm_urls_for_dir(id self, SEL c, u64 dir, u64 mask) {
    (void)self; (void)c;
    id paths = NSSearchPathForDirectoriesInDomains(dir, mask, 1);
    id out = MSG0(id, cls("NSMutableArray"), "array");
    u64 n = paths ? MSG0(u64, paths, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id p = MSG(id, paths, "objectAtIndex:", i);
        id u = MSG(id, cls("NSURL"), "fileURLWithPath:", p);
        MSG(void, out, "addObject:", u);
    }
    return out;
}
static id fm_url_for_dir(id self, SEL c, u64 dir, u64 dom, id near, BOOL create, id *err) {
    (void)near;
    id a = fm_urls_for_dir(self, c, dir, dom ? dom : 1);
    id u = MSG0(u64, a, "count") ? MSG(id, a, "objectAtIndex:", (u64)0) : 0;
    if (u && create) { id up = MSG0(id, u, "path"); MSG(BOOL, self, "createDirectoryAtPath:withIntermediateDirectories:attributes:error:", up, (BOOL)1, (id)0, (id *)0); }
    if (!u && err) *err = 0;
    return u;
}
/* ---- segunda leva da partida do WhatsApp */
/* NSCache: um dicionário com nome e limites (o mOS não descarta nada sozinho) */
typedef struct { void *isa; id dict, name, delegate; u64 count_limit, cost_limit; } fx_cache_t;
static id ca_init(id self, SEL c) { (void)c; fx_cache_t *k = (fx_cache_t *)self; k->dict = objc_retain(MSG0(id, cls("NSMutableDictionary"), "dictionary")); return self; }
static id ca_get(id self, SEL c, id key) { (void)c; fx_cache_t *k = (fx_cache_t *)self; return k->dict && key ? MSG(id, k->dict, "objectForKey:", key) : 0; }
static void ca_set(id self, SEL c, id v, id key) {
    (void)c; fx_cache_t *k = (fx_cache_t *)self;
    if (!k->dict) ca_init(self, c);
    if (!key) return;
    if (v) MSG(void, k->dict, "setObject:forKey:", v, key); else MSG(void, k->dict, "removeObjectForKey:", key);
}
static void ca_set_cost(id self, SEL c, id v, id key, u64 cost) { (void)cost; ca_set(self, c, v, key); }
static void ca_del(id self, SEL c, id key) { (void)c; fx_cache_t *k = (fx_cache_t *)self; if (k->dict && key) MSG(void, k->dict, "removeObjectForKey:", key); }
static void ca_clear(id self, SEL c) { (void)c; fx_cache_t *k = (fx_cache_t *)self; if (k->dict) MSG0(void, k->dict, "removeAllObjects"); }
static id ca_name(id self, SEL c) { (void)c; fx_cache_t *k = (fx_cache_t *)self; return k->name ? k->name : cx_str(""); }
static void ca_set_name(id self, SEL c, id n) { (void)c; fx_cache_t *k = (fx_cache_t *)self; id o = k->name; k->name = n ? MSG0(id, n, "copy") : 0; if (o) objc_release(o); }
static u64 ca_count_limit(id self, SEL c) { (void)c; return ((fx_cache_t *)self)->count_limit; }
static void ca_set_count_limit(id self, SEL c, u64 v) { (void)c; ((fx_cache_t *)self)->count_limit = v; }
static u64 ca_cost_limit(id self, SEL c) { (void)c; return ((fx_cache_t *)self)->cost_limit; }
static void ca_set_cost_limit(id self, SEL c, u64 v) { (void)c; ((fx_cache_t *)self)->cost_limit = v; }
static id ca_delegate(id self, SEL c) { (void)c; return ((fx_cache_t *)self)->delegate; }
static void ca_set_delegate(id self, SEL c, id d) { (void)c; ((fx_cache_t *)self)->delegate = d; }
static BOOL ca_evicts(id self, SEL c) { (void)self; (void)c; return 1; }
static void ca_set_evicts(id self, SEL c, BOOL b) { (void)self; (void)c; (void)b; }
void objc_msgSendSuper(void);
static void ca_dealloc(id self, SEL c) {
    fx_cache_t *k = (fx_cache_t *)self;
    if (k->dict) objc_release(k->dict);
    if (k->name) objc_release(k->name);
    struct { id r; cx_class_t *s; } sup = {self, cx_class_named("NSObject")};
    ((void (*)(void *, SEL))objc_msgSendSuper)(&sup, c);
}
/* NSDate */
static BOOL dt_eq(id self, SEL c, id o) {
    (void)c;
    if (!o) return 0;
    return MSG0(double, self, "timeIntervalSince1970") == MSG0(double, o, "timeIntervalSince1970");
}
static id dt_adding(id self, SEL c, double s) { (void)c; double t = MSG0(double, self, "timeIntervalSince1970") + s; return MSG(id, (id)self->isa, "dateWithTimeIntervalSince1970:", t); }
/* caminhos e arquivos */
static id s_del_ext(id self, SEL c) {
    (void)c;
    const char *s = cx_cstr(self);
    u64 n = strlen(s), dot = n;
    for (u64 i = n; i > 0; i--) { if (s[i - 1] == '/') break; if (s[i - 1] == '.') { dot = i - 1; break; } }
    if (dot == 0 || dot == n || s[dot - 1] == '/') return self;
    char b[2048];
    if (dot >= sizeof b) return self;
    memcpy(b, s, dot); b[dot] = 0;
    return cx_str(b);
}
static BOOL s_write_url(id self, SEL c, id url, BOOL atom, u64 enc, id *err) {
    (void)c;
    id p = url ? MSG0(id, url, "path") : 0;
    if (!p) { if (err) *err = 0; return 0; }
    return MSG(BOOL, self, "writeToFile:atomically:encoding:error:", p, atom, enc, err);
}
static id fm_contents_url(id self, SEL c, id url, id keys, u64 opts, id *err) {
    (void)c; (void)keys;
    id p = url ? MSG0(id, url, "path") : 0;
    if (!p) { if (err) *err = 0; return 0; }
    id names = MSG(id, self, "contentsOfDirectoryAtPath:error:", p, err);
    if (!names) return 0;
    id out = MSG0(id, cls("NSMutableArray"), "array");
    u64 n = MSG0(u64, names, "count");
    for (u64 i = 0; i < n; i++) {
        id nm = MSG(id, names, "objectAtIndex:", i);
        if ((opts & 4) && cx_cstr(nm)[0] == '.') continue;                  /* NSDirectoryEnumerationSkipsHiddenFiles */
        id u = MSG(id, url, "URLByAppendingPathComponent:", nm);
        MSG(void, out, "addObject:", u);
    }
    return out;
}
static void d_get_bytes(id self, SEL c, void *buf, u64 n) {
    (void)c;
    u64 len = MSG0(u64, self, "length");
    const void *b = MSG0(const void *, self, "bytes");
    if (buf && b) memcpy(buf, b, n < len ? n : len);
}
typedef struct { u64 loc, len; } fx_rng_t;
static void d_get_bytes_range(id self, SEL c, void *buf, fx_rng_t r) {
    (void)c;
    u64 len = MSG0(u64, self, "length");
    const u8 *b = MSG0(const u8 *, self, "bytes");
    if (!buf || !b || r.loc >= len) return;
    memcpy(buf, b + r.loc, r.loc + r.len <= len ? r.len : len - r.loc);
}
static id x_init_self3(id self, SEL c, u64 a, u64 b, u64 n) { (void)c; (void)a; (void)b; (void)n; return self; }
static id x_init_self2(id self, SEL c, u64 a, u64 n) { (void)c; (void)a; (void)n; return self; }
static id ht_with_opts(id k, SEL c, u64 o) { (void)c; (void)o; return MSG0(id, k, "weakObjectsHashTable"); }
/* -rangeOfString:options: (NSCaseInsensitiveSearch 1, NSBackwardsSearch 4, NSAnchoredSearch 8; o resto é literal) */
typedef struct { u64 loc, len; } fx_range_t;
static unsigned short fx_fold(unsigned short ch) { return ch >= 'A' && ch <= 'Z' ? (unsigned short)(ch + 32) : ch; }
static fx_range_t s_range_opts(id self, SEL c, id pat, u64 opts) {
    (void)c;
    fx_range_t nf = {0x7fffffffffffffffull, 0};
    if (!pat) return nf;
    u64 n = MSG0(u64, self, "length"), m = MSG0(u64, pat, "length");
    if (!m || m > n) return nf;
    unsigned short *h = malloc((n + m) * 2), *p = h + n;
    if (!h) return nf;
    for (u64 i = 0; i < n; i++) h[i] = MSG(unsigned short, self, "characterAtIndex:", i);
    for (u64 i = 0; i < m; i++) p[i] = MSG(unsigned short, pat, "characterAtIndex:", i);
    int ci = opts & 1, back = opts & 4, anch = opts & 8;
    fx_range_t r = nf;
    for (u64 k = 0; k <= n - m; k++) {
        u64 at = back ? n - m - k : k;
        u64 j = 0;
        for (; j < m; j++) { unsigned short a = h[at + j], b = p[j]; if (ci ? fx_fold(a) != fx_fold(b) : a != b) break; }
        if (j == m) { r.loc = at; r.len = m; break; }
        if (anch) break;
    }
    free(h);
    return r;
}
/* -stringByPaddingToLength:withString:startingAtIndex: */
static id s_pad(id self, SEL c, u64 len, id pad, u64 start) {
    (void)c;
    u64 n = MSG0(u64, self, "length");
    if (len <= n) return MSG(id, self, "substringToIndex:", len);
    u64 pn = pad ? MSG0(u64, pad, "length") : 0;
    if (!pn) return self;
    unsigned short *b = malloc(len * 2 + 2);
    if (!b) return self;
    for (u64 i = 0; i < n; i++) b[i] = MSG(unsigned short, self, "characterAtIndex:", i);
    for (u64 i = n, k = start % pn; i < len; i++, k = (k + 1) % pn) b[i] = MSG(unsigned short, pad, "characterAtIndex:", k);
    id r = MSG(id, cls("NSString"), "stringWithCharacters:length:", (const unsigned short *)b, len);
    free(b);
    return r;
}
/* +[NSLocale componentsFromLocaleIdentifier:] / CFLocaleCreateComponentsFromLocaleIdentifier: "ll", "ll_CC",
 * "ll_Scrp_CC" ou com '-'; @calendar=... é ignorado */
static id loc_components(id ident) {
    id d = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    const char *s = ident ? cx_cstr(ident) : "";
    char part[3][16] = {{0}}; int np = 0, k = 0;
    for (const char *q = s; *q && *q != '@' && np < 3; q++) {
        if (*q == '_' || *q == '-') { part[np][k] = 0; np++; k = 0; continue; }
        if (k < 15) part[np][k++] = *q;
    }
    if (np < 3) { part[np][k] = 0; np++; }
    if (part[0][0]) { id v = cx_str(part[0]); MSG(void, d, "setObject:forKey:", v, cx_str("kCFLocaleLanguageCodeKey")); }
    for (int i = 1; i < np; i++) {
        if (!part[i][0]) continue;
        id v = cx_str(part[i]);
        MSG(void, d, "setObject:forKey:", v, cx_str(strlen(part[i]) == 4 ? "kCFLocaleScriptCodeKey" : "kCFLocaleCountryCodeKey"));
    }
    return d;
}
static id loc_cmp_m(id k, SEL c, id ident) { (void)k; (void)c; return loc_components(ident); }
static id loc_nil(id self, SEL c) { (void)self; (void)c; return 0; }
id CFLocaleCreateComponentsFromLocaleIdentifier(void *alloc, id ident) { (void)alloc; return objc_retain(loc_components(ident)); }
/* classes que sobem depois do cx_foundation_extra_init (nsurl.c, foundation_more.c): chamado por cx_foundation_gaps */
void cx_foundation_extra_late(void) {
    cx_class_t *ku = cx_class_named("NSKeyedUnarchiver"), *lo = cx_class_named("NSLocale"), *s = cx_class_named("NSString");
    if (ku) { cx_add_cmethod(ku, "setClass:forClassName:", (void *)ku_set_class); cx_add_method(ku, "setClass:forClassName:", (void *)ku_set_class); }
    if (lo) { cx_add_cmethod(lo, "localeWithLocaleIdentifier:", (void *)loc_with_ident); cx_add_method(lo, "initWithLocaleIdentifier:", (void *)loc_init_ident);
              cx_add_cmethod(lo, "componentsFromLocaleIdentifier:", (void *)loc_cmp_m); cx_add_method(lo, "scriptCode", (void *)loc_nil);
              cx_add_method(lo, "variantCode", (void *)loc_nil); }
    { cx_class_t *mt = cx_class_named("NSMapTable"), *ht = cx_class_named("NSHashTable");
      if (mt) { cx_add_method(mt, "initWithKeyOptions:valueOptions:capacity:", (void *)x_init_self3); }
      if (ht) { cx_add_method(ht, "initWithOptions:capacity:", (void *)x_init_self2); cx_add_cmethod(ht, "hashTableWithOptions:", (void *)ht_with_opts); } }
    if (s) { cx_add_method(s, "rangeOfString:options:", (void *)s_range_opts); cx_add_method(s, "stringByPaddingToLength:withString:startingAtIndex:", (void *)s_pad); }
}
void cx_foundation_extra_init(void) {
    cx_class_t *ob = cx_class_named("NSObject"), *data = cx_class_named("NSData");
    OBJC_EHTYPE_NSException.vtable = objc_ehtype_vtable + 2;
    { cx_class_t *s = cx_class_named("NSString"), *a = cx_class_named("NSArray");
      if (s) cx_add_method(s, "initWithBytesNoCopy:length:encoding:freeWhenDone:", (void *)s_init_nocopy);
      if (a) cx_add_method(a, "initWithObjects:", (void *)a_init_objs_va);
      cx_class_t *fmc = cx_class_named("NSFileManager"), *thc = cx_class_named("NSThread"), *pdc = cx_class_named("NSPurgeableData");
      if (fmc) cx_add_method(fmc, "containerURLForSecurityApplicationGroupIdentifier:", (void *)fm_group_container);
      if (thc) { cx_add_cmethod(thc, "callStackReturnAddresses", (void *)th_ret_addrs); cx_add_cmethod(thc, "callStackSymbols", (void *)th_ret_addrs);
                 cx_add_method(thc, "name", (void *)th_name); cx_add_method(thc, "setName:", (void *)th_set_name); }
      /* NSPurgeableData: um NSMutableData que nunca é descartado */
      if (pdc) { cx_add_cmethod(pdc, "dataWithLength:", (void *)pd_with_length); cx_add_cmethod(pdc, "dataWithCapacity:", (void *)pd_with_capacity); }
      if (data) { cx_add_method(data, "beginContentAccess", (void *)pd_yes); cx_add_method(data, "endContentAccess", (void *)pd_noop);
                  cx_add_method(data, "discardContentIfPossible", (void *)pd_noop); cx_add_method(data, "isContentDiscarded", (void *)pd_no); }
      { cx_class_t *ns = cx_class_named("NSNumber"), *md = cx_class_named("NSMutableDictionary"),
                   *bu = cx_class_named("NSBundle");
        if (s) { cx_add_method(s, "dataUsingEncoding:", (void *)s_data_enc); cx_add_method(s, "dataUsingEncoding:allowLossyConversion:", (void *)s_data_enc_lossy);
                 cx_add_method(s, "stringByAppendingPathExtension:", (void *)s_append_ext); }
        if (a) cx_add_method(a, "arrayByAddingObjectsFromArray:", (void *)a_adding_array);
        if (ns) cx_add_cmethod(ns, "numberWithUnsignedLong:", (void *)n_with_ulong);
        if (md) cx_add_method(md, "initWithCapacity:", (void *)d_init_cap);
        if (bu) cx_add_method(bu, "localizations", (void *)b_localizations);
        if (fmc) { cx_add_method(fmc, "URLsForDirectory:inDomains:", (void *)fm_urls_for_dir);
                   cx_add_method(fmc, "URLForDirectory:inDomain:appropriateForURL:create:error:", (void *)fm_url_for_dir); } }
      { cx_class_t *dt = cx_class_named("NSDate"), *fm2 = cx_class_named("NSFileManager");
        if (dt) { cx_add_method(dt, "isEqualToDate:", (void *)dt_eq); cx_add_method(dt, "dateByAddingTimeInterval:", (void *)dt_adding); }
        if (s) { cx_add_method(s, "stringByDeletingPathExtension", (void *)s_del_ext); cx_add_method(s, "writeToURL:atomically:encoding:error:", (void *)s_write_url); }
        if (fm2) cx_add_method(fm2, "contentsOfDirectoryAtURL:includingPropertiesForKeys:options:error:", (void *)fm_contents_url);
        if (data) { cx_add_method(data, "getBytes:length:", (void *)d_get_bytes); cx_add_method(data, "getBytes:range:", (void *)d_get_bytes_range); }
        if (ob && !cx_class_named("NSCache")) {
            cx_define_class(&NSCache_c, &NSCache_m, "NSCache", ob, sizeof(fx_cache_t) - 8);
            IM(NSCache, "init", ca_init); IM(NSCache, "objectForKey:", ca_get); IM(NSCache, "setObject:forKey:", ca_set);
            IM(NSCache, "setObject:forKey:cost:", ca_set_cost); IM(NSCache, "removeObjectForKey:", ca_del);
            IM(NSCache, "removeAllObjects", ca_clear); IM(NSCache, "name", ca_name); IM(NSCache, "setName:", ca_set_name);
            IM(NSCache, "countLimit", ca_count_limit); IM(NSCache, "setCountLimit:", ca_set_count_limit);
            IM(NSCache, "totalCostLimit", ca_cost_limit); IM(NSCache, "setTotalCostLimit:", ca_set_cost_limit);
            IM(NSCache, "delegate", ca_delegate); IM(NSCache, "setDelegate:", ca_set_delegate);
            IM(NSCache, "evictsObjectsWithDiscardedContent", ca_evicts); IM(NSCache, "setEvictsObjectsWithDiscardedContent:", ca_set_evicts);
            IM(NSCache, "dealloc", ca_dealloc);
        } }
    }
    OBJC_EHTYPE_NSException.cls = cx_class_named("NSException");
    if (data && !cx_class_named("NSMutableData")) {
        cx_define_class(&NSMutableData_c, &NSMutableData_m, "NSMutableData", data, 8);
        CM_(NSMutableData, "data", md_data); CM_(NSMutableData, "new", md_data);
        CM_(NSMutableData, "dataWithCapacity:", md_with_capacity); CM_(NSMutableData, "dataWithLength:", md_with_length);
        CM_(NSMutableData, "dataWithBytes:length:", md_with_bytes); CM_(NSMutableData, "dataWithData:", md_with_data);
        IM(NSMutableData, "init", md_init); IM(NSMutableData, "initWithCapacity:", md_init_capacity);
        IM(NSMutableData, "initWithLength:", md_init_length); IM(NSMutableData, "initWithBytes:length:", md_init_bytes);
        IM(NSMutableData, "initWithData:", md_init_data); IM(NSMutableData, "mutableBytes", md_mbytes);
        IM(NSMutableData, "setLength:", md_set_length); IM(NSMutableData, "increaseLengthBy:", md_increase);
        IM(NSMutableData, "appendBytes:length:", md_append_bytes); IM(NSMutableData, "appendData:", md_append_data);
        IM(NSMutableData, "setData:", md_set_data); IM(NSMutableData, "replaceBytesInRange:withBytes:", md_replace);
        IM(NSMutableData, "replaceBytesInRange:withBytes:length:", md_replace_len); IM(NSMutableData, "resetBytesInRange:", md_reset);
        IM(NSMutableData, "copy", md_copy); IM(NSMutableData, "mutableCopy", md_mcopy); IM(NSMutableData, "dealloc", md_dealloc);
    }
    if (!cx_class_named("NSTimeZone")) {
        cx_define_class(&NSTimeZone_c, &NSTimeZone_m, "NSTimeZone", ob, 16);
        CM_(NSTimeZone, "localTimeZone", tz_local); CM_(NSTimeZone, "systemTimeZone", tz_local); CM_(NSTimeZone, "defaultTimeZone", tz_local);
        CM_(NSTimeZone, "timeZoneWithName:", tz_named); CM_(NSTimeZone, "timeZoneWithAbbreviation:", tz_named);
        CM_(NSTimeZone, "timeZoneForSecondsFromGMT:", tz_for_seconds); CM_(NSTimeZone, "resetSystemTimeZone", tz_reset);
        CM_(NSTimeZone, "knownTimeZoneNames", tz_known);
        IM(NSTimeZone, "name", tz_name); IM(NSTimeZone, "description", tz_name); IM(NSTimeZone, "abbreviation", tz_abbrev);
        IM(NSTimeZone, "secondsFromGMT", tz_seconds); IM(NSTimeZone, "secondsFromGMTForDate:", tz_seconds_for);
        IM(NSTimeZone, "isDaylightSavingTime", tz_dst); IM(NSTimeZone, "isDaylightSavingTimeForDate:", tz_dst);
    }
    if (!cx_class_named("NSDateComponents")) {
        cx_define_class(&NSDateComponents_c, &NSDateComponents_m, "NSDateComponents", ob, (u32)(sizeof(dc_t) - 8));
        IM(NSDateComponents, "init", dc_alloc_init);
#define REG(field, getter, setter) IM(NSDateComponents, #getter, dc_get_##field); IM(NSDateComponents, #setter ":", dc_set_##field);
        REG(era, era, setEra) REG(year, year, setYear) REG(month, month, setMonth) REG(day, day, setDay)
        REG(hour, hour, setHour) REG(minute, minute, setMinute) REG(second, second, setSecond)
        REG(nanosecond, nanosecond, setNanosecond) REG(weekday, weekday, setWeekday)
        REG(weekOfYear, weekOfYear, setWeekOfYear) REG(yearForWeek, yearForWeekOfYear, setYearForWeekOfYear)
#undef REG
        IM(NSDateComponents, "timeZone", dc_tz); IM(NSDateComponents, "setTimeZone:", dc_set_tz);
    }
    if (!cx_class_named("NSCalendar")) {
        cx_define_class(&NSCalendar_c, &NSCalendar_m, "NSCalendar", ob, 16);
        CM_(NSCalendar, "currentCalendar", cal_current); CM_(NSCalendar, "autoupdatingCurrentCalendar", cal_current);
        CM_(NSCalendar, "calendarWithIdentifier:", cal_with_ident);
        IM(NSCalendar, "initWithCalendarIdentifier:", cal_init_ident); IM(NSCalendar, "calendarIdentifier", cal_ident);
        IM(NSCalendar, "timeZone", cal_get_tz); IM(NSCalendar, "setTimeZone:", cal_set_tz); IM(NSCalendar, "firstWeekday", cal_first_weekday);
        IM(NSCalendar, "components:fromDate:", cal_components); IM(NSCalendar, "componentsInTimeZone:fromDate:", cal_components_tz);
        IM(NSCalendar, "dateFromComponents:", cal_date_from); IM(NSCalendar, "dateByAddingComponents:toDate:options:", cal_by_adding);
        IM(NSCalendar, "component:fromDate:", cal_component);
    }
    if (!cx_class_named("NSEnumerator")) {
        cx_define_class(&NSEnumerator_c, &NSEnumerator_m, "NSEnumerator", ob, 16);
        IM(NSEnumerator, "nextObject", en_next); IM(NSEnumerator, "allObjects", en_all);
        IM(NSEnumerator, "countByEnumeratingWithState:objects:count:", en_fast);
    }
    if (!cx_class_named("NSDirectoryEnumerator")) {
        cx_define_class(&NSDirectoryEnumerator_c, &NSDirectoryEnumerator_m, "NSDirectoryEnumerator", &NSEnumerator_c, 0);
        IM(NSDirectoryEnumerator, "skipDescendants", de_skip); IM(NSDirectoryEnumerator, "level", de_level);
        cx_class_t *fm = cx_class_named("NSFileManager");
        if (fm) {
            cx_add_method(fm, "enumeratorAtURL:includingPropertiesForKeys:options:errorHandler:", (void *)fm_enum_url);
            cx_add_method(fm, "enumeratorAtPath:", (void *)fm_enum_path);
            cx_add_method(fm, "createDirectoryAtURL:withIntermediateDirectories:attributes:error:", (void *)fm_mkdir_url);
        }
    }
    if (!cx_class_named("NSOrderedSet")) {
        cx_define_class(&NSOrderedSet_c, &NSOrderedSet_m, "NSOrderedSet", ob, 8);
        cx_define_class(&NSMutableOrderedSet_c, &NSMutableOrderedSet_m, "NSMutableOrderedSet", &NSOrderedSet_c, 0);
        CM_(NSOrderedSet, "orderedSet", os_empty); CM_(NSOrderedSet, "orderedSetWithArray:", os_with_array);
        CM_(NSOrderedSet, "orderedSetWithObject:", os_with_object);
        IM(NSOrderedSet, "init", os_init); IM(NSOrderedSet, "initWithArray:", os_init_array);
        IM(NSOrderedSet, "array", os_array); IM(NSOrderedSet, "set", os_set); IM(NSOrderedSet, "copy", os_copy);
        IM(NSOrderedSet, "mutableCopy", os_mcopy); IM(NSOrderedSet, "objectEnumerator", os_enum); IM(NSOrderedSet, "isEqual:", os_is_equal);
        IM(NSOrderedSet, "forwardingTargetForSelector:", os_forward);
        CM_(NSMutableOrderedSet, "orderedSetWithCapacity:", os_empty);
        IM(NSMutableOrderedSet, "initWithCapacity:", os_init);
        IM(NSMutableOrderedSet, "addObject:", os_add); IM(NSMutableOrderedSet, "insertObject:atIndex:", os_insert);
        IM(NSMutableOrderedSet, "addObjectsFromArray:", os_add_array);
    }
    if (!cx_class_named("NSMethodSignature")) {
        cx_define_class(&NSMethodSignature_c, &NSMethodSignature_m, "NSMethodSignature", ob, (u32)(sizeof(msig_t) - 8));
        CM_(NSMethodSignature, "signatureWithObjCTypes:", sig_with_types);
        IM(NSMethodSignature, "numberOfArguments", sig_nargs); IM(NSMethodSignature, "getArgumentTypeAtIndex:", sig_arg);
        IM(NSMethodSignature, "methodReturnType", sig_ret); IM(NSMethodSignature, "methodReturnLength", sig_retlen);
        IM(NSMethodSignature, "frameLength", sig_frame); IM(NSMethodSignature, "isOneway", sig_oneway);
        cx_define_class(&NSInvocation_c, &NSInvocation_m, "NSInvocation", ob, (u32)(sizeof(inv_t) - 8));
        CM_(NSInvocation, "invocationWithMethodSignature:", inv_with_sig);
        IM(NSInvocation, "methodSignature", inv_sig); IM(NSInvocation, "setTarget:", inv_set_target); IM(NSInvocation, "target", inv_target);
        IM(NSInvocation, "setSelector:", inv_set_sel); IM(NSInvocation, "selector", inv_sel);
        IM(NSInvocation, "setArgument:atIndex:", inv_set_arg); IM(NSInvocation, "getArgument:atIndex:", inv_get_arg);
        IM(NSInvocation, "getReturnValue:", inv_get_ret); IM(NSInvocation, "setReturnValue:", inv_set_ret);
        IM(NSInvocation, "retainArguments", inv_retain_args); IM(NSInvocation, "invoke", inv_invoke);
        IM(NSInvocation, "invokeWithTarget:", inv_invoke_with);
    }
}
