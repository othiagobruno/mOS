/*
 * mOS libSystem — os formatters da Foundation sobre a ICU (o trabalho está em fdn_fmt_core.c):
 * NSFormatter, NSDateFormatter, NSISO8601DateFormatter, NSNumberFormatter, NSByteCountFormatter,
 * NSDateComponentsFormatter, NSRelativeDateTimeFormatter, NSListFormatter, NSDateIntervalFormatter,
 * NSPersonNameComponentsFormatter, e as classes de valor NSDateInterval e NSPersonNameComponents.
 * CFDateFormatter e CFNumberFormatter são os mesmos objetos (toll-free).
 */
#include "foundation.h"
#include "libc.h"
#include "fdn_fmt_core.h"

double floor(double x);
double fabs(double x);
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
IMP cx_lookup(cx_class_t *cls, SEL sel);
char *strncpy(char *d, const char *s, size_t n);
char *strtok(char *s, const char *d);
char *strncat(char *d, const char *s, size_t n);
const char *fdn_locale_ident_of(id loc);
const char *fdn_timezone_name_of(id tz);
typedef struct { long loc, len; } ns_range_t;

#define KLASS(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
KLASS(NSFormatter) KLASS(NSDateFormatter) KLASS(NSISO8601DateFormatter) KLASS(NSNumberFormatter)
KLASS(NSByteCountFormatter) KLASS(NSDateComponentsFormatter) KLASS(NSRelativeDateTimeFormatter) KLASS(NSListFormatter)
KLASS(NSDateIntervalFormatter) KLASS(NSPersonNameComponentsFormatter) KLASS(NSDateInterval) KLASS(NSPersonNameComponents)

static id cls(const char *n) { return (id)cx_class_named(n); }
static const char *cs(id s) { return s ? cx_cstr(s) : ""; }
static id str(const char *s) { return cx_str(s ? s : ""); }
static double date_t(id d) { return d ? MSG0(double, d, "timeIntervalSince1970") : 0; }
static id date_new(double t) { return MSG(id, cls("NSDate"), "dateWithTimeIntervalSince1970:", t); }
static void setobj(id *slot, id v) { id o = *slot; *slot = v ? objc_retain(v) : 0; if (o) objc_release(o); }
static void setcopy(id *slot, id v) { id o = *slot; *slot = v ? MSG0(id, v, "copy") : 0; if (o) objc_release(o); }
static void super_dealloc(id self) {
    IMP sup = cx_lookup(cx_class_named("NSObject"), sel_registerName("dealloc"));
    if (sup) sup(self, sel_registerName("dealloc"));
}
static BOOL is_kind(id o, cx_class_t *k) { return o && cx_is_subclass(object_getClass(o), k); }
static id loc_or_current(id l) { return l ? l : MSG0(id, cls("NSLocale"), "currentLocale"); }
static BOOL yes_m(id k, SEL c) { (void)k; (void)c; return YES; }
static id nil_m(id k, SEL c) { (void)k; (void)c; return 0; }
static id self_copy(id self, SEL c) { (void)c; return objc_retain(self); }

/* ======================================================================== NSFormatter */

static id f_string_obj(id self, SEL c, id o) { (void)self; (void)c; (void)o; return 0; }
static id f_editing(id self, SEL c, id o) { (void)c; return MSG(id, self, "stringForObjectValue:", o); }
static id f_attr(id self, SEL c, id o, id attrs) {
    (void)c;
    id s = MSG(id, self, "stringForObjectValue:", o);
    if (!s) return 0;
    id a = MSG0(id, cls("NSAttributedString"), "alloc");
    return objc_autorelease(MSG(id, a, "initWithString:attributes:", s, attrs));
}
static BOOL f_get_obj(id self, SEL c, id *obj, id s, id *err) { (void)self; (void)c; (void)s; if (obj) *obj = 0; if (err) *err = 0; return NO; }
static BOOL f_partial(id self, SEL c, id s, id *ns, id *err) { (void)self; (void)c; (void)s; if (ns) *ns = 0; if (err) *err = 0; return YES; }
static BOOL f_partial2(id self, SEL c, id *s, ns_range_t *r, id orig, ns_range_t or, id *err) { (void)self; (void)c; (void)s; (void)r; (void)orig; (void)or; if (err) *err = 0; return YES; }
static void f_encode(id self, SEL c, id coder) { (void)self; (void)c; (void)coder; }
static id f_init_coder(id self, SEL c, id coder) { (void)c; (void)coder; return MSG0(id, self, "init"); }

/* ======================================================================== NSDateFormatter */

typedef struct {
    cx_class_t *isa;
    fdn_df f;
    id locale, tz, cal, default_date, two_digit;
    id sym[22];          /* símbolos trocados pelo app (índice UDateFormatSymbolType); nil = os do locale */
    int behavior, context, has_format;
    volatile int lock;
} df_t;
static void df_lock(df_t *d) { while (__atomic_exchange_n(&d->lock, 1, __ATOMIC_ACQUIRE)) { } }
static void df_unlock(df_t *d) { __atomic_store_n(&d->lock, 0, __ATOMIC_RELEASE); }

/* a configuração do objeto vira a do fdn_df; muda algo, descarta o UDateFormat */
static void df_sync(df_t *d) {
    char loc[96], tz[64], ci[24];
    strncpy(loc, fdn_locale_ident_of(d->locale), sizeof loc - 1); loc[95] = 0;
    strncpy(tz, fdn_timezone_name_of(d->tz ? d->tz : MSG0(id, cls("NSTimeZone"), "defaultTimeZone")), sizeof tz - 1); tz[63] = 0;
    if (d->cal) { strncpy(ci, cs(MSG0(id, d->cal, "calendarIdentifier")), sizeof ci - 1); ci[23] = 0; }
    else fdn_loc_info(loc, FDN_LN_CALENDAR_ID, ci, sizeof ci);
    int fw = d->cal ? (int)MSG0(u64, d->cal, "firstWeekday") : 0, md = d->cal ? (int)MSG0(u64, d->cal, "minimumDaysInFirstWeek") : 0;
    if (strcmp(loc, d->f.locale) || strcmp(tz, d->f.tz) || strcmp(ci, d->f.cal) || fw != d->f.first_weekday || md != d->f.min_days) {
        strcpy(d->f.locale, loc); strcpy(d->f.tz, tz); strcpy(d->f.cal, ci);
        d->f.first_weekday = fw; d->f.min_days = md;
        fdn_df_reset(&d->f);
    }
}
static void df_dirty(df_t *d) { fdn_df_reset(&d->f); }

static id df_init(id self, SEL c) {
    (void)c;
    df_t *d = (df_t *)self;
    fdn_df_init(&d->f);
    d->behavior = 1040;   /* NSDateFormatterBehavior10_4 */
    return self;
}
static id df_string(id self, SEL c, id date) {
    (void)c;
    df_t *d = (df_t *)self;
    char b[512];
    if (!date || !is_kind(date, (cx_class_t *)cls("NSDate"))) return 0;
    df_lock(d);
    df_sync(d);
    fdn_df_format(&d->f, date_t(date), b, sizeof b);
    df_unlock(d);
    return str(b);
}
static id df_string_obj(id self, SEL c, id o) { return o && is_kind(o, (cx_class_t *)cls("NSDate")) ? df_string(self, c, o) : 0; }
static id df_date_range(df_t *d, id s, ns_range_t *r) {
    double t;
    int32_t end = 0;
    if (!s) return 0;
    df_lock(d);
    df_sync(d);
    const char *u = cs(s);
    int ok = fdn_df_parse(&d->f, u, r ? (int32_t)r->loc : 0, &t, &end);
    df_unlock(d);
    if (!ok) return 0;
    /* -dateFromString: exige o texto inteiro (fora espaços no fim) */
    if (!r) {
        int32_t n16 = fdn_u16_from_utf8(u, -1, 0, 0);
        if (end < n16) {
            UChar *w = fdn_u16_dup(u, &n16);
            int trailing = 1;
            for (int32_t i = end; w && i < n16; i++) if (w[i] != ' ' && w[i] != '\t' && w[i] != '\n') trailing = 0;
            free(w);
            if (!trailing) return 0;
        }
    } else r->len = end - r->loc;
    return date_new(t);
}
static id df_date(id self, SEL c, id s) { (void)c; return df_date_range((df_t *)self, s, 0); }
static BOOL df_get_obj(id self, SEL c, id *obj, id s, id *err) {
    (void)c;
    id d = df_date_range((df_t *)self, s, 0);
    if (obj) *obj = d;
    if (!d && err) *err = str("Formato de data inválido");
    return d != 0;
}
static BOOL df_get_obj_range(id self, SEL c, id *obj, id s, ns_range_t *r, id *err) {
    (void)c;
    ns_range_t local = {0, 0};
    id d = df_date_range((df_t *)self, s, r ? r : &local);
    if (obj) *obj = d;
    if (!d && err) *err = MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:", str("NSCocoaErrorDomain"), (i64)2048, (id)0);
    return d != 0;
}
static id df_format(id self, SEL c, id x) {
    (void)c; (void)x;
    df_t *d = (df_t *)self;
    char b[256];
    df_lock(d);
    df_sync(d);
    fdn_df_pattern(&d->f, b, sizeof b);
    df_unlock(d);
    return str(b);
}
static id df_format0(id self, SEL c) { return df_format(self, c, 0); }
static void df_set_format(id self, SEL c, id fmt) {
    (void)c;
    df_t *d = (df_t *)self;
    df_lock(d);
    strncpy(d->f.pattern, cs(fmt), sizeof d->f.pattern - 1);
    d->f.pattern[sizeof d->f.pattern - 1] = 0;
    d->has_format = fmt != 0;
    df_dirty(d);
    df_unlock(d);
}
static u64 df_date_style(id self, SEL c) { (void)c; return (u64)((df_t *)self)->f.date_style; }
static u64 df_time_style(id self, SEL c) { (void)c; return (u64)((df_t *)self)->f.time_style; }
static void df_set_date_style(id self, SEL c, u64 v) { (void)c; df_t *d = (df_t *)self; df_lock(d); d->f.date_style = (int)v; d->f.pattern[0] = 0; d->has_format = 0; df_dirty(d); df_unlock(d); }
static void df_set_time_style(id self, SEL c, u64 v) { (void)c; df_t *d = (df_t *)self; df_lock(d); d->f.time_style = (int)v; d->f.pattern[0] = 0; d->has_format = 0; df_dirty(d); df_unlock(d); }
static id df_locale(id self, SEL c) { (void)c; return loc_or_current(((df_t *)self)->locale); }
static void df_set_locale(id self, SEL c, id l) { (void)c; df_t *d = (df_t *)self; df_lock(d); setobj(&d->locale, l); df_dirty(d); df_unlock(d); }
static id df_tz(id self, SEL c) { (void)c; df_t *d = (df_t *)self; return d->tz ? d->tz : MSG0(id, cls("NSTimeZone"), "defaultTimeZone"); }
static void df_set_tz(id self, SEL c, id tz) { (void)c; df_t *d = (df_t *)self; df_lock(d); setobj(&d->tz, tz); df_dirty(d); df_unlock(d); }
static id df_cal(id self, SEL c) {
    (void)c;
    df_t *d = (df_t *)self;
    if (d->cal) return d->cal;
    return MSG(id, loc_or_current(d->locale), "objectForKey:", cx_str("kCFLocaleCalendarKey"));
}
static void df_set_cal(id self, SEL c, id cal) { (void)c; df_t *d = (df_t *)self; df_lock(d); setcopy(&d->cal, cal); df_dirty(d); df_unlock(d); }
static BOOL df_lenient(id self, SEL c) { (void)c; return (BOOL)((df_t *)self)->f.lenient; }
static void df_set_lenient(id self, SEL c, BOOL v) { (void)c; df_t *d = (df_t *)self; d->f.lenient = v; df_dirty(d); }
static BOOL df_relative(id self, SEL c) { (void)c; return (BOOL)((df_t *)self)->f.relative; }
static void df_set_relative(id self, SEL c, BOOL v) { (void)c; df_t *d = (df_t *)self; d->f.relative = v; df_dirty(d); }
static id df_two_digit(id self, SEL c) {
    (void)c;
    df_t *d = (df_t *)self;
    if (d->two_digit) return d->two_digit;
    /* a ICU começa o século de dois dígitos 80 anos antes de agora */
    return date_new(cx_now() - 80.0 * 365.2425 * 86400.0);
}
static void df_set_two_digit(id self, SEL c, id v) {
    (void)c;
    df_t *d = (df_t *)self;
    setobj(&d->two_digit, v);
    d->f.has_two_digit = v != 0;
    d->f.two_digit_start = date_t(v);
    df_dirty(d);
}
static id df_default_date(id self, SEL c) { (void)c; return ((df_t *)self)->default_date; }
static void df_set_default_date(id self, SEL c, id v) { (void)c; setobj(&((df_t *)self)->default_date, v); }
static u64 df_behavior(id self, SEL c) { (void)c; return (u64)((df_t *)self)->behavior; }
static void df_set_behavior(id self, SEL c, u64 v) { (void)c; ((df_t *)self)->behavior = (int)v; }
static i64 df_context(id self, SEL c) { (void)c; return ((df_t *)self)->context; }
static void df_set_context(id self, SEL c, i64 v) { (void)c; ((df_t *)self)->context = (int)v; }
static BOOL df_gen_cal(id self, SEL c) { (void)self; (void)c; return NO; }
static void df_set_gen_cal(id self, SEL c, BOOL v) { (void)self; (void)c; (void)v; }
static void df_set_template(id self, SEL c, id tmpl) {
    (void)c;
    df_t *d = (df_t *)self;
    char p[256];
    df_lock(d);
    df_sync(d);
    if (fdn_df_template(d->f.locale, cs(tmpl), p, sizeof p) > 0) { strcpy(d->f.pattern, p); d->has_format = 1; df_dirty(d); }
    df_unlock(d);
}
static id df_s_from_template(id k, SEL c, id tmpl, u64 opts, id locale) {
    (void)k; (void)c; (void)opts;
    char p[256];
    return fdn_df_template(fdn_locale_ident_of(locale), cs(tmpl), p, sizeof p) > 0 ? str(p) : 0;
}
static id df_s_localized(id k, SEL c, id date, u64 ds, u64 ts) {
    (void)k; (void)c;
    id f = MSG0(id, MSG0(id, (id)&NSDateFormatter_c, "alloc"), "init");
    df_set_date_style(f, 0, ds);
    df_set_time_style(f, 0, ts);
    id r = df_string(f, 0, date);
    objc_release(f);
    return r;
}
static u64 g_default_behavior = 1040;
static u64 df_s_default_behavior(id k, SEL c) { (void)k; (void)c; return g_default_behavior; }
static void df_s_set_default_behavior(id k, SEL c, u64 v) { (void)k; (void)c; g_default_behavior = v; }

/* símbolos: os trocados pelo app ou os do locale/calendário */
static void each_add(const char *s, void *ctx) { cx_arr_add((id)ctx, cx_str(s)); }
static const int sym_kind[22] = {
    /* UDAT_ERAS..UDAT_STANDALONE_SHORTER_WEEKDAYS -> FDN_SYM_* */
    FDN_SYM_ERAS, FDN_SYM_MONTHS, FDN_SYM_SHORT_MONTHS, FDN_SYM_WEEKDAYS, FDN_SYM_SHORT_WEEKDAYS, FDN_SYM_AM,
    -1, FDN_SYM_LONG_ERAS, FDN_SYM_VERY_SHORT_MONTHS, FDN_SYM_VERY_SHORT_WEEKDAYS, FDN_SYM_SA_MONTHS,
    FDN_SYM_SA_SHORT_MONTHS, FDN_SYM_SA_VERY_SHORT_MONTHS, FDN_SYM_SA_WEEKDAYS, FDN_SYM_SA_SHORT_WEEKDAYS,
    FDN_SYM_SA_VERY_SHORT_WEEKDAYS, FDN_SYM_QUARTERS, FDN_SYM_SHORT_QUARTERS, FDN_SYM_SA_QUARTERS,
    FDN_SYM_SA_SHORT_QUARTERS, -1, -1,
};
static id df_syms(id self, int type) {
    df_t *d = (df_t *)self;
    if (d->sym[type]) return d->sym[type];
    df_sync(d);
    fdn_cal k;
    fdn_cal_init(&k, d->f.cal, d->f.locale, d->f.tz);
    id a = MSG0(id, cls("NSMutableArray"), "array");
    if (sym_kind[type] >= 0) fdn_cal_symbols(&k, sym_kind[type], each_add, a);
    return objc_autorelease(MSG0(id, a, "copy"));
}
static void df_set_syms(id self, int type, id arr) {
    df_t *d = (df_t *)self;
    setcopy(&d->sym[type], arr);
    df_lock(d);
    df_sync(d);
    /* aplica no UDateFormat (o fdn_df_format abre de novo depois de um reset, então guarda e reaplica lá) */
    df_dirty(d);
    df_unlock(d);
}
#define SYMS(get, set, type) \
    static id df_g_##get(id self, SEL c) { (void)c; return df_syms(self, type); } \
    static void df_s_##get(id self, SEL c, id v) { (void)c; df_set_syms(self, type, v); }
SYMS(eras, setEraSymbols, 0) SYMS(months, setMonthSymbols, 1) SYMS(short_months, setShortMonthSymbols, 2)
SYMS(weekdays, setWeekdaySymbols, 3) SYMS(short_weekdays, setShortWeekdaySymbols, 4)
SYMS(long_eras, setLongEraSymbols, 7) SYMS(vs_months, setVeryShortMonthSymbols, 8)
SYMS(vs_weekdays, setVeryShortWeekdaySymbols, 9) SYMS(sa_months, setStandaloneMonthSymbols, 10)
SYMS(sa_short_months, setShortStandaloneMonthSymbols, 11) SYMS(sa_vs_months, setVeryShortStandaloneMonthSymbols, 12)
SYMS(sa_weekdays, setStandaloneWeekdaySymbols, 13) SYMS(sa_short_weekdays, setShortStandaloneWeekdaySymbols, 14)
SYMS(sa_vs_weekdays, setVeryShortStandaloneWeekdaySymbols, 15) SYMS(quarters, setQuarterSymbols, 16)
SYMS(short_quarters, setShortQuarterSymbols, 17) SYMS(sa_quarters, setStandaloneQuarterSymbols, 18)
SYMS(sa_short_quarters, setShortStandaloneQuarterSymbols, 19)
#undef SYMS
static id df_am(id self, SEL c) {
    (void)c;
    df_t *d = (df_t *)self;
    if (d->sym[20]) return d->sym[20];
    df_sync(d);
    fdn_cal k;
    fdn_cal_init(&k, d->f.cal, d->f.locale, d->f.tz);
    id a = MSG0(id, cls("NSMutableArray"), "array");
    fdn_cal_symbols(&k, FDN_SYM_AM, each_add, a);
    return MSG0(u64, a, "count") ? MSG(id, a, "objectAtIndex:", (u64)0) : str("AM");
}
static id df_pm(id self, SEL c) {
    (void)c;
    df_t *d = (df_t *)self;
    if (d->sym[21]) return d->sym[21];
    df_sync(d);
    fdn_cal k;
    fdn_cal_init(&k, d->f.cal, d->f.locale, d->f.tz);
    id a = MSG0(id, cls("NSMutableArray"), "array");
    fdn_cal_symbols(&k, FDN_SYM_PM, each_add, a);
    return MSG0(u64, a, "count") ? MSG(id, a, "objectAtIndex:", (u64)0) : str("PM");
}
static void df_set_am(id self, SEL c, id v) { (void)c; setcopy(&((df_t *)self)->sym[20], v); df_dirty((df_t *)self); }
static void df_set_pm(id self, SEL c, id v) { (void)c; setcopy(&((df_t *)self)->sym[21], v); df_dirty((df_t *)self); }
static id df_copy(id self, SEL c) {
    (void)c;
    df_t *o = (df_t *)self, *d = (df_t *)cx_alloc_instance(object_getClass(self));
    d->f = o->f;
    d->f.udat = 0;
    d->locale = o->locale ? objc_retain(o->locale) : 0;
    d->tz = o->tz ? objc_retain(o->tz) : 0;
    d->cal = o->cal ? objc_retain(o->cal) : 0;
    d->default_date = o->default_date ? objc_retain(o->default_date) : 0;
    d->two_digit = o->two_digit ? objc_retain(o->two_digit) : 0;
    for (int i = 0; i < 22; i++) d->sym[i] = o->sym[i] ? objc_retain(o->sym[i]) : 0;
    d->behavior = o->behavior; d->context = o->context; d->has_format = o->has_format;
    return (id)d;
}
static void df_dealloc(id self, SEL c) {
    (void)c;
    df_t *d = (df_t *)self;
    fdn_df_reset(&d->f);
    setobj(&d->locale, 0); setobj(&d->tz, 0); setobj(&d->cal, 0); setobj(&d->default_date, 0); setobj(&d->two_digit, 0);
    for (int i = 0; i < 22; i++) setobj(&d->sym[i], 0);
    super_dealloc(self);
}

/* ======================================================================== NSISO8601DateFormatter */

typedef struct { cx_class_t *isa; fdn_df f; id tz; u64 options; volatile int lock; } iso_t;
static void iso_sync(iso_t *s) {
    char p[96], tz[64];
    fdn_iso_pattern((unsigned long)s->options, p, sizeof p);
    strncpy(tz, s->tz ? fdn_timezone_name_of(s->tz) : "GMT", sizeof tz - 1); tz[63] = 0;
    if (strcmp(p, s->f.pattern) || strcmp(tz, s->f.tz)) {
        strcpy(s->f.pattern, p); strcpy(s->f.tz, tz);
        strcpy(s->f.locale, "en_US_POSIX"); strcpy(s->f.cal, "gregorian");
        fdn_df_reset(&s->f);
    }
}
static id iso_init(id self, SEL c) { (void)c; iso_t *s = (iso_t *)self; fdn_df_init(&s->f); s->options = 1907; return self; }
static id iso_string(id self, SEL c, id date) {
    (void)c;
    iso_t *s = (iso_t *)self;
    char b[128];
    if (!date) return 0;
    while (__atomic_exchange_n(&s->lock, 1, __ATOMIC_ACQUIRE)) { }
    iso_sync(s);
    fdn_df_format(&s->f, date_t(date), b, sizeof b);
    __atomic_store_n(&s->lock, 0, __ATOMIC_RELEASE);
    return str(b);
}
static id iso_date(id self, SEL c, id text) {
    (void)c;
    iso_t *s = (iso_t *)self;
    double t;
    int32_t end = 0;
    if (!text) return 0;
    while (__atomic_exchange_n(&s->lock, 1, __ATOMIC_ACQUIRE)) { }
    iso_sync(s);
    int ok = fdn_df_parse(&s->f, cs(text), 0, &t, &end);
    __atomic_store_n(&s->lock, 0, __ATOMIC_RELEASE);
    if (!ok || end != fdn_u16_from_utf8(cs(text), -1, 0, 0)) return 0;
    return date_new(t);
}
static id iso_string_obj(id self, SEL c, id o) { return o && is_kind(o, (cx_class_t *)cls("NSDate")) ? iso_string(self, c, o) : 0; }
static BOOL iso_get_obj(id self, SEL c, id *obj, id s, id *err) { id d = iso_date(self, c, s); if (obj) *obj = d; if (!d && err) *err = str("Data ISO 8601 inválida"); return d != 0; }
static id iso_tz(id self, SEL c) { (void)c; iso_t *s = (iso_t *)self; return s->tz ? s->tz : MSG(id, cls("NSTimeZone"), "timeZoneWithName:", cx_str("GMT")); }
static void iso_set_tz(id self, SEL c, id tz) { (void)c; setobj(&((iso_t *)self)->tz, tz); }
static u64 iso_options(id self, SEL c) { (void)c; return ((iso_t *)self)->options; }
static void iso_set_options(id self, SEL c, u64 o) { (void)c; ((iso_t *)self)->options = o; }
static id iso_s_string(id k, SEL c, id date, id tz, u64 opts) {
    (void)k; (void)c;
    id f = MSG0(id, MSG0(id, (id)&NSISO8601DateFormatter_c, "alloc"), "init");
    iso_set_tz(f, 0, tz);
    iso_set_options(f, 0, opts);
    id r = iso_string(f, 0, date);
    objc_release(f);
    return r;
}
static void iso_dealloc(id self, SEL c) { (void)c; iso_t *s = (iso_t *)self; fdn_df_reset(&s->f); setobj(&s->tz, 0); super_dealloc(self); }

/* ======================================================================== NSNumberFormatter */

/* propriedades de texto: as que vão para símbolo da ICU guardam o índice */
enum { NS_DECIMAL, NS_GROUPING, NS_PERCENT, NS_MINUS, NS_PLUS, NS_CUR_SYMBOL, NS_INTL_CUR, NS_CUR_DECIMAL,
       NS_EXPONENT, NS_PERMILL, NS_POS_INF, NS_NAN, NS_CUR_GROUPING, NS_POS_PREFIX, NS_POS_SUFFIX, NS_NEG_PREFIX,
       NS_NEG_SUFFIX, NS_PADDING, NS_CUR_CODE, NS_POS_FORMAT, NS_NEG_FORMAT, NS_NEG_INF, NS_NSTR };
static const int nstr_symbol[NS_NSTR] = {0, 1, 3, 6, 7, 8, 9, 10, 11, 12, 14, 15, 17, -1, -1, -1, -1, -1, -1, -1, -1, -1};
static const int nstr_text[NS_NSTR] = {-1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1, 0, 1, 2, 3, 4, 5, -1, -1, -1};
/* propriedades inteiras: atributo UNumberFormatAttribute */
enum { NI_MIN_INT, NI_MAX_INT, NI_MIN_FRAC, NI_MAX_FRAC, NI_GROUP_SIZE, NI_SEC_GROUP, NI_WIDTH, NI_PAD_POS,
       NI_ROUNDING, NI_USES_GROUP, NI_ALWAYS_DEC, NI_USES_SIG, NI_MIN_SIG, NI_MAX_SIG, NI_LENIENT, NI_NINT };
static const int nint_attr[NI_NINT] = {4, 3, 7, 6, 10, 15, 13, 14, 11, 1, 2, 16, 17, 18, 19};

typedef struct {
    cx_class_t *isa;
    int style;
    id locale;
    void *nf;
    char *s[NS_NSTR];
    int iv[NI_NINT];
    unsigned iset;
    double rounding_inc;
    int has_rounding_inc;
    id zero, nil_sym, minimum, maximum, multiplier;
    int gen_decimal, partial, behavior, has_multiplier;
    volatile int lock;
} nf_t;
static void nf_lock(nf_t *n) { while (__atomic_exchange_n(&n->lock, 1, __ATOMIC_ACQUIRE)) { } }
static void nf_unlock(nf_t *n) { __atomic_store_n(&n->lock, 0, __ATOMIC_RELEASE); }
static void nf_dirty(nf_t *n) { if (n->nf) fdn_nf_close(n->nf); n->nf = 0; }

static void *nf_get(nf_t *n) {
    if (n->nf) return n->nf;
    n->nf = fdn_nf_open(n->style, fdn_locale_ident_of(n->locale));
    if (!n->nf) return 0;
    if (n->s[NS_POS_FORMAT]) {
        char p[512];
        snprintf(p, sizeof p, "%s%s%s", n->s[NS_POS_FORMAT], n->s[NS_NEG_FORMAT] ? ";" : "", n->s[NS_NEG_FORMAT] ? n->s[NS_NEG_FORMAT] : "");
        fdn_nf_apply_pattern(n->nf, p);
    }
    if (n->s[NS_CUR_CODE]) fdn_nf_set_text(n->nf, 5, n->s[NS_CUR_CODE]);
    for (int i = 0; i < NI_NINT; i++) if (n->iset & (1u << i)) fdn_nf_set_attr(n->nf, nint_attr[i], n->iv[i]);
    if (n->has_rounding_inc) fdn_nf_set_dattr(n->nf, 12, n->rounding_inc);
    if (n->has_multiplier && n->multiplier) {
        double m = MSG0(double, n->multiplier, "doubleValue");
        if (m == floor(m)) fdn_nf_set_attr(n->nf, 9, (int)m);
    }
    for (int i = 0; i < NS_NSTR; i++) {
        if (!n->s[i]) continue;
        if (nstr_symbol[i] >= 0) fdn_nf_set_symbol(n->nf, nstr_symbol[i], n->s[i]);
        if (nstr_text[i] >= 0 && i != NS_CUR_CODE) fdn_nf_set_text(n->nf, nstr_text[i], n->s[i]);
    }
    return n->nf;
}
static id nf_init(id self, SEL c) { (void)c; nf_t *n = (nf_t *)self; n->style = 0; n->behavior = 1040; return self; }
static void nf_set_str(id self, int which, id v) {
    nf_t *n = (nf_t *)self;
    nf_lock(n);
    free(n->s[which]);
    n->s[which] = v ? strdup(cx_cstr(v)) : 0;
    nf_dirty(n);
    nf_unlock(n);
}
static id nf_get_str(id self, int which) {
    nf_t *n = (nf_t *)self;
    char b[256];
    if (n->s[which]) return str(n->s[which]);
    nf_lock(n);
    void *nf = nf_get(n);
    b[0] = 0;
    if (which == NS_POS_FORMAT || which == NS_NEG_FORMAT) {
        fdn_nf_pattern(nf, b, sizeof b);
        char *semi = strchr(b, ';');
        if (which == NS_POS_FORMAT) { if (semi) *semi = 0; }
        else {
            if (semi) memmove(b, semi + 1, strlen(semi + 1) + 1);
            else { char t[260]; snprintf(t, sizeof t, "-%s", b); strncpy(b, t, sizeof b - 1); b[255] = 0; }
        }
    } else if (which == NS_NEG_INF) {
        fdn_nf_format_double(nf, -__builtin_inf(), b, sizeof b);
    } else if (nstr_symbol[which] >= 0) fdn_nf_get_symbol(nf, nstr_symbol[which], b, sizeof b);
    else if (nstr_text[which] >= 0) fdn_nf_get_text(nf, nstr_text[which], b, sizeof b);
    nf_unlock(n);
    if (which == NS_CUR_CODE && !b[0]) return 0;
    return str(b);
}
static void nf_set_int(id self, int which, i64 v) { nf_t *n = (nf_t *)self; nf_lock(n); n->iv[which] = (int)v; n->iset |= 1u << which; nf_dirty(n); nf_unlock(n); }
static i64 nf_get_int(id self, int which) {
    nf_t *n = (nf_t *)self;
    if (n->iset & (1u << which)) return n->iv[which];
    nf_lock(n);
    int v = fdn_nf_get_attr(nf_get(n), nint_attr[which]);
    nf_unlock(n);
    return v;
}
#define NSTR(get, set, which) \
    static id nf_g_##get(id self, SEL c) { (void)c; return nf_get_str(self, which); } \
    static void nf_s_##get(id self, SEL c, id v) { (void)c; nf_set_str(self, which, v); }
NSTR(decimal, setDecimalSeparator, NS_DECIMAL) NSTR(grouping, setGroupingSeparator, NS_GROUPING)
NSTR(percent, setPercentSymbol, NS_PERCENT) NSTR(minus, setMinusSign, NS_MINUS) NSTR(plus, setPlusSign, NS_PLUS)
NSTR(cur_symbol, setCurrencySymbol, NS_CUR_SYMBOL) NSTR(intl_cur, setInternationalCurrencySymbol, NS_INTL_CUR)
NSTR(cur_decimal, setCurrencyDecimalSeparator, NS_CUR_DECIMAL) NSTR(exponent, setExponentSymbol, NS_EXPONENT)
NSTR(permill, setPerMillSymbol, NS_PERMILL) NSTR(pos_inf, setPositiveInfinitySymbol, NS_POS_INF)
NSTR(nan, setNotANumberSymbol, NS_NAN) NSTR(cur_grouping, setCurrencyGroupingSeparator, NS_CUR_GROUPING)
NSTR(pos_prefix, setPositivePrefix, NS_POS_PREFIX) NSTR(pos_suffix, setPositiveSuffix, NS_POS_SUFFIX)
NSTR(neg_prefix, setNegativePrefix, NS_NEG_PREFIX) NSTR(neg_suffix, setNegativeSuffix, NS_NEG_SUFFIX)
NSTR(padding, setPaddingCharacter, NS_PADDING) NSTR(cur_code, setCurrencyCode, NS_CUR_CODE)
NSTR(pos_format, setPositiveFormat, NS_POS_FORMAT) NSTR(neg_format, setNegativeFormat, NS_NEG_FORMAT)
NSTR(neg_inf, setNegativeInfinitySymbol, NS_NEG_INF)
#undef NSTR
#define NINT(get, which) \
    static i64 nf_gi_##get(id self, SEL c) { (void)c; return nf_get_int(self, which); } \
    static void nf_si_##get(id self, SEL c, i64 v) { (void)c; nf_set_int(self, which, v); }
NINT(min_int, NI_MIN_INT) NINT(max_int, NI_MAX_INT) NINT(min_frac, NI_MIN_FRAC) NINT(max_frac, NI_MAX_FRAC)
NINT(group_size, NI_GROUP_SIZE) NINT(sec_group, NI_SEC_GROUP) NINT(width, NI_WIDTH) NINT(pad_pos, NI_PAD_POS)
NINT(rounding, NI_ROUNDING) NINT(uses_group, NI_USES_GROUP) NINT(always_dec, NI_ALWAYS_DEC)
NINT(uses_sig, NI_USES_SIG) NINT(min_sig, NI_MIN_SIG) NINT(max_sig, NI_MAX_SIG) NINT(lenient, NI_LENIENT)
#undef NINT
static BOOL nf_g_uses_group_b(id self, SEL c) { return nf_gi_uses_group(self, c) != 0; }
static void nf_s_uses_group_b(id self, SEL c, BOOL v) { nf_si_uses_group(self, c, v); }
static BOOL nf_g_always_dec_b(id self, SEL c) { return nf_gi_always_dec(self, c) != 0; }
static void nf_s_always_dec_b(id self, SEL c, BOOL v) { nf_si_always_dec(self, c, v); }
static BOOL nf_g_uses_sig_b(id self, SEL c) { return nf_gi_uses_sig(self, c) != 0; }
static void nf_s_uses_sig_b(id self, SEL c, BOOL v) { nf_si_uses_sig(self, c, v); }
static BOOL nf_g_lenient_b(id self, SEL c) { return nf_gi_lenient(self, c) != 0; }
static void nf_s_lenient_b(id self, SEL c, BOOL v) { nf_si_lenient(self, c, v); }
static u64 nf_style(id self, SEL c) { (void)c; return (u64)((nf_t *)self)->style; }
static void nf_set_style(id self, SEL c, u64 st) {
    (void)c;
    nf_t *n = (nf_t *)self;
    nf_lock(n);
    /* trocar o estilo zera os ajustes de dígitos e o formato, como o -setNumberStyle: do macOS */
    n->style = (int)st;
    n->iset = 0;
    free(n->s[NS_POS_FORMAT]); n->s[NS_POS_FORMAT] = 0;
    free(n->s[NS_NEG_FORMAT]); n->s[NS_NEG_FORMAT] = 0;
    nf_dirty(n);
    nf_unlock(n);
}
static id nf_locale(id self, SEL c) { (void)c; return loc_or_current(((nf_t *)self)->locale); }
static void nf_set_locale(id self, SEL c, id l) { (void)c; nf_t *n = (nf_t *)self; nf_lock(n); setobj(&n->locale, l); nf_dirty(n); nf_unlock(n); }
static id nf_rounding_inc(id self, SEL c) {
    (void)c;
    nf_t *n = (nf_t *)self;
    double v = n->has_rounding_inc ? n->rounding_inc : fdn_nf_get_dattr(nf_get(n), 12);
    return cx_num_d(v);
}
static void nf_set_rounding_inc(id self, SEL c, id v) {
    (void)c;
    nf_t *n = (nf_t *)self;
    nf_lock(n);
    n->has_rounding_inc = v != 0;
    n->rounding_inc = v ? MSG0(double, v, "doubleValue") : 0;
    nf_dirty(n);
    nf_unlock(n);
}
static id nf_multiplier(id self, SEL c) {
    (void)c;
    nf_t *n = (nf_t *)self;
    if (n->has_multiplier) return n->multiplier;
    return n->style == FDN_NS_PERCENT ? cx_num_i(100) : 0;
}
static void nf_set_multiplier(id self, SEL c, id v) { (void)c; nf_t *n = (nf_t *)self; nf_lock(n); setobj(&n->multiplier, v); n->has_multiplier = 1; nf_dirty(n); nf_unlock(n); }
#define NOBJ(get, field) \
    static id nf_go_##get(id self, SEL c) { (void)c; return ((nf_t *)self)->field; } \
    static void nf_so_##get(id self, SEL c, id v) { (void)c; setcopy(&((nf_t *)self)->field, v); }
NOBJ(zero, zero) NOBJ(nil_sym, nil_sym) NOBJ(minimum, minimum) NOBJ(maximum, maximum)
#undef NOBJ
static BOOL nf_gen_dec(id self, SEL c) { (void)c; return (BOOL)((nf_t *)self)->gen_decimal; }
static void nf_set_gen_dec(id self, SEL c, BOOL v) { (void)c; ((nf_t *)self)->gen_decimal = v; }
static BOOL nf_partial_b(id self, SEL c) { (void)c; return (BOOL)((nf_t *)self)->partial; }
static void nf_set_partial_b(id self, SEL c, BOOL v) { (void)c; ((nf_t *)self)->partial = v; }
static u64 nf_behavior(id self, SEL c) { (void)c; return (u64)((nf_t *)self)->behavior; }
static void nf_set_behavior(id self, SEL c, u64 v) { (void)c; ((nf_t *)self)->behavior = (int)v; }
static id nf_format(id self, SEL c) {
    (void)c;
    id p = nf_get_str(self, NS_POS_FORMAT), n = nf_get_str(self, NS_NEG_FORMAT);
    return MSG(id, cls("NSString"), "stringWithFormat:", cx_str("%@;%@"), p, n);
}
static void nf_set_format(id self, SEL c, id f) {
    (void)c;
    const char *s = cs(f);
    const char *semi = strchr(s, ';');
    if (semi) {
        nf_set_str(self, NS_POS_FORMAT, cx_str_n(s, (u64)(semi - s)));
        nf_set_str(self, NS_NEG_FORMAT, cx_str(semi + 1));
    } else nf_set_str(self, NS_POS_FORMAT, f);
}
static id nf_string(id self, SEL c, id num) {
    (void)c;
    nf_t *n = (nf_t *)self;
    char b[512];
    if (!num) return n->nil_sym;
    double d = MSG0(double, num, "doubleValue");
    if (d == 0 && n->zero) return n->zero;
    nf_lock(n);
    void *nf = nf_get(n);
    if (n->has_multiplier && n->multiplier) {
        double m = MSG0(double, n->multiplier, "doubleValue");
        if (m != floor(m)) d *= m;
    }
    const char *t = MSG0(const char *, num, "objCType");
    int integral = t && (*t == 'q' || *t == 'Q' || *t == 'i' || *t == 'I' || *t == 'l' || *t == 'L' || *t == 's' || *t == 'S' || *t == 'c' || *t == 'C' || *t == 'B');
    int is_decimal = is_kind(num, (cx_class_t *)cls("NSDecimalNumber"));
    if (is_decimal) {
        id ds = MSG0(id, num, "stringValue");
        if (!fdn_nf_format_decimal(nf, cs(ds), b, sizeof b)) fdn_nf_format_double(nf, d, b, sizeof b);
    } else if (integral && !(n->has_multiplier && n->multiplier && MSG0(double, n->multiplier, "doubleValue") != floor(MSG0(double, n->multiplier, "doubleValue"))))
        fdn_nf_format_int64(nf, MSG0(i64, num, "longLongValue"), b, sizeof b);
    else fdn_nf_format_double(nf, d, b, sizeof b);
    nf_unlock(n);
    return str(b);
}
static id nf_string_obj(id self, SEL c, id o) { return o && is_kind(o, (cx_class_t *)cls("NSNumber")) ? nf_string(self, c, o) : 0; }
static id nf_number_range(nf_t *n, id s, ns_range_t *r) {
    double v;
    int64_t iv = 0;
    int is_int = 0;
    int32_t end = 0, len16 = 0;
    if (!s) return 0;
    const char *u = cs(s);
    if (n->zero && !strcmp(u, cs(n->zero))) return cx_num_i(0);
    nf_lock(n);
    int ok = fdn_nf_parse(nf_get(n), u, &v, &iv, &is_int, &end, &len16);
    nf_unlock(n);
    if (!ok) return 0;
    if (r) { r->loc = 0; r->len = end; }
    else if (end != len16) {
        /* a Foundation aceita espaço no fim e exige o resto inteiro */
        UChar *w = fdn_u16_dup(u, &len16);
        int rest = 0;
        for (int32_t i = end; w && i < len16; i++) if (w[i] != ' ' && w[i] != 0xA0 && w[i] != 0x202F) rest = 1;
        free(w);
        if (rest) return 0;
    }
    if (n->has_multiplier && n->multiplier) {
        double m = MSG0(double, n->multiplier, "doubleValue");
        if (m != floor(m) && m != 0) { v /= m; is_int = v == floor(v); iv = (int64_t)v; }
    }
    if (n->minimum && v < MSG0(double, n->minimum, "doubleValue")) return 0;
    if (n->maximum && v > MSG0(double, n->maximum, "doubleValue")) return 0;
    if (n->gen_decimal) return MSG(id, cls("NSDecimalNumber"), "numberWithDouble:", v);
    return is_int ? cx_num_i(iv) : cx_num_d(v);
}
static id nf_number(id self, SEL c, id s) { (void)c; return nf_number_range((nf_t *)self, s, 0); }
static BOOL nf_get_obj(id self, SEL c, id *obj, id s, id *err) {
    (void)c;
    id n = nf_number_range((nf_t *)self, s, 0);
    if (obj) *obj = n;
    if (!n && err) *err = str("Número inválido");
    return n != 0;
}
static BOOL nf_get_obj_range(id self, SEL c, id *obj, id s, ns_range_t *r, id *err) {
    (void)c;
    id n = nf_number_range((nf_t *)self, s, r);
    if (obj) *obj = n;
    if (!n && err) *err = MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:", str("NSCocoaErrorDomain"), (i64)2048, (id)0);
    return n != 0;
}
static id nf_s_localized(id k, SEL c, id num, u64 style) {
    (void)k; (void)c;
    id f = MSG0(id, MSG0(id, (id)&NSNumberFormatter_c, "alloc"), "init");
    nf_set_style(f, 0, style);
    id r = nf_string(f, 0, num);
    objc_release(f);
    return r;
}
static u64 g_nf_default_behavior = 1040;
static u64 nf_s_default_behavior(id k, SEL c) { (void)k; (void)c; return g_nf_default_behavior; }
static void nf_s_set_default_behavior(id k, SEL c, u64 v) { (void)k; (void)c; g_nf_default_behavior = v; }
static id nf_copy(id self, SEL c) {
    (void)c;
    nf_t *o = (nf_t *)self, *n = (nf_t *)cx_alloc_instance(object_getClass(self));
    n->style = o->style;
    n->locale = o->locale ? objc_retain(o->locale) : 0;
    for (int i = 0; i < NS_NSTR; i++) n->s[i] = o->s[i] ? strdup(o->s[i]) : 0;
    memcpy(n->iv, o->iv, sizeof n->iv);
    n->iset = o->iset;
    n->rounding_inc = o->rounding_inc; n->has_rounding_inc = o->has_rounding_inc;
    n->zero = o->zero ? objc_retain(o->zero) : 0; n->nil_sym = o->nil_sym ? objc_retain(o->nil_sym) : 0;
    n->minimum = o->minimum ? objc_retain(o->minimum) : 0; n->maximum = o->maximum ? objc_retain(o->maximum) : 0;
    n->multiplier = o->multiplier ? objc_retain(o->multiplier) : 0; n->has_multiplier = o->has_multiplier;
    n->gen_decimal = o->gen_decimal; n->partial = o->partial; n->behavior = o->behavior;
    return (id)n;
}
static void nf_dealloc(id self, SEL c) {
    (void)c;
    nf_t *n = (nf_t *)self;
    nf_dirty(n);
    for (int i = 0; i < NS_NSTR; i++) free(n->s[i]);
    setobj(&n->locale, 0); setobj(&n->zero, 0); setobj(&n->nil_sym, 0); setobj(&n->minimum, 0);
    setobj(&n->maximum, 0); setobj(&n->multiplier, 0);
    super_dealloc(self);
}

/* ======================================================================== NSByteCountFormatter */

typedef struct { cx_class_t *isa; u64 allowed; i64 style; int nonnumeric, unit, count, actual, adaptive, zero_pad, context; } bc_t;
static id bc_init(id self, SEL c) {
    (void)c;
    bc_t *b = (bc_t *)self;
    b->nonnumeric = b->unit = b->count = b->adaptive = 1;
    return self;
}
static id bc_format(bc_t *b, i64 bytes) {
    char out[256];
    fdn_bytes_format(fdn_locale_ident_of(0), bytes, (unsigned)b->allowed, (int)b->style, b->nonnumeric, b->unit, b->count,
                     b->actual, b->adaptive, b->zero_pad, out, sizeof out);
    return str(out);
}
static id bc_string(id self, SEL c, i64 bytes) { (void)c; return bc_format((bc_t *)self, bytes); }
static id bc_string_obj(id self, SEL c, id o) { (void)c; return o && is_kind(o, (cx_class_t *)cls("NSNumber")) ? bc_format((bc_t *)self, MSG0(i64, o, "longLongValue")) : 0; }
static id bc_s_string(id k, SEL c, i64 bytes, i64 style) {
    (void)k; (void)c;
    bc_t tmp;
    memset(&tmp, 0, sizeof tmp);
    tmp.nonnumeric = tmp.unit = tmp.count = tmp.adaptive = 1;
    tmp.style = style;
    return bc_format(&tmp, bytes);
}
#define BCP(get, set, field, T) \
    static T bc_g_##get(id self, SEL c) { (void)c; return (T)((bc_t *)self)->field; } \
    static void bc_s_##get(id self, SEL c, T v) { (void)c; ((bc_t *)self)->field = v; }
BCP(allowed, setAllowedUnits, allowed, u64) BCP(style, setCountStyle, style, i64)
BCP(nonnumeric, setAllowsNonnumericFormatting, nonnumeric, BOOL) BCP(unit, setIncludesUnit, unit, BOOL)
BCP(count, setIncludesCount, count, BOOL) BCP(actual, setIncludesActualByteCount, actual, BOOL)
BCP(adaptive, setAdaptive, adaptive, BOOL) BCP(zero_pad, setZeroPadsFractionDigits, zero_pad, BOOL)
BCP(context, setFormattingContext, context, i64)
#undef BCP

/* ======================================================================== NSDateComponentsFormatter */

typedef struct {
    cx_class_t *isa;
    u64 allowed;
    i64 style, max_units, zero_behavior, context;
    int fractional, collapses, approx, remaining;
    id cal, ref;
} dcf_t;
static id dcf_init(id self, SEL c) { (void)c; return self; }

typedef struct { u64 unit; const char *icu; double secs; } dur_unit_t;
static const dur_unit_t dur_units[] = {
    {FDN_U_YEAR, "duration-year", 31556952.0}, {FDN_U_MONTH, "duration-month", 2629746.0},
    {FDN_U_WEEK_OF_MONTH, "duration-week", 604800.0}, {FDN_U_DAY, "duration-day", 86400.0},
    {FDN_U_HOUR, "duration-hour", 3600.0}, {FDN_U_MINUTE, "duration-minute", 60.0},
    {FDN_U_SECOND, "duration-second", 1.0}, {FDN_U_NANOSECOND, "duration-nanosecond", 1e-9},
};
#define NDU ((int)(sizeof dur_units / sizeof dur_units[0]))

static const char *phrase(const char *lang, int remaining) {
    static const char *const t[][3] = {
        {"pt", "Cerca de %s", "%s restantes"}, {"es", "Aproximadamente %s", "Quedan %s"}, {"fr", "Environ %s", "%s restantes"},
        {"de", "Etwa %s", "Noch %s"}, {"it", "Circa %s", "%s rimanenti"}, {"en", "About %s", "%s remaining"},
    };
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++) if (!strcmp(lang, t[i][0])) return t[i][remaining ? 2 : 1];
    return remaining ? "%s remaining" : "About %s";
}

/* valores por unidade (já decompostos) -> texto, aplicando as regras de zero, contagem máxima e frases */
static id dcf_render(dcf_t *f, double *vals, int negative) {
    u64 allowed = f->allowed ? f->allowed : (FDN_U_YEAR | FDN_U_MONTH | FDN_U_DAY | FDN_U_HOUR | FDN_U_MINUTE | FDN_U_SECOND);
    int style = (int)f->style;
    i64 zb = f->zero_behavior;
    if (zb == 0) zb = style == 0 ? 1 /* DropLeading */ : 14 /* DropAll */;
    int idx[NDU], n = 0;
    for (int i = 0; i < NDU; i++) if (allowed & dur_units[i].unit) idx[n++] = i;
    if (!n) return 0;
    /* collapsesLargestUnit: "1 min 30 s" vira "90 s" */
    if (f->collapses) {
        int first = -1;
        for (int k = 0; k < n; k++) if (vals[idx[k]] != 0) { first = k; break; }
        if (first >= 0 && first + 1 < n && vals[idx[first]] == 1) {
            double ratio = dur_units[idx[first]].secs / dur_units[idx[first + 1]].secs;
            vals[idx[first + 1]] += ratio;
            vals[idx[first]] = 0;
        }
    }
    int keep[NDU];
    for (int k = 0; k < n; k++) keep[k] = 1;
    int first_nz = -1, last_nz = -1;
    for (int k = 0; k < n; k++) if (vals[idx[k]] != 0) { if (first_nz < 0) first_nz = k; last_nz = k; }
    if (!(zb & 0x10000)) {
        for (int k = 0; k < n; k++) {
            if (vals[idx[k]] != 0) continue;
            if ((zb & 1) && (first_nz < 0 || k < first_nz)) keep[k] = 0;
            if ((zb & 2) && first_nz >= 0 && k > first_nz && k < last_nz) keep[k] = 0;
            if ((zb & 4) && last_nz >= 0 && k > last_nz) keep[k] = 0;
        }
        /* posicional sempre mostra pelo menos minutos e segundos ("0:05") */
        if (style == 0) {
            int nkeep = 0;
            for (int k = 0; k < n; k++) nkeep += keep[k];
            if (nkeep < 2) for (int k = n - 1, add = 2 - nkeep; k >= 0 && add > 0; k--) if (!keep[k]) { keep[k] = 1; add--; }
        }
        int any = 0;
        for (int k = 0; k < n; k++) any |= keep[k];
        if (!any) keep[n - 1] = 1;   /* tudo zero: "0 s" na menor unidade */
    }
    /* maximumUnitCount: corta e arredonda a última unidade que fica */
    if (f->max_units > 0) {
        int seen = 0, cut = -1;
        for (int k = 0; k < n; k++) if (keep[k]) { if (++seen == f->max_units) { cut = k; break; } }
        if (cut >= 0) {
            double rest = 0;
            for (int k = cut + 1; k < n; k++) { rest += vals[idx[k]] * dur_units[idx[k]].secs; keep[k] = 0; }
            double add = rest / dur_units[idx[cut]].secs;
            vals[idx[cut]] = f->fractional ? vals[idx[cut]] + add : floor(vals[idx[cut]] + add + 0.5);
        }
    }
    fdn_dur_part parts[NDU];
    int m = 0;
    for (int k = 0; k < n; k++) if (keep[k]) { parts[m].unit = dur_units[idx[k]].icu; parts[m].value = vals[idx[k]]; m++; }
    char out[512], lang[16];
    const char *loc = fdn_locale_ident_of(f->cal ? MSG0(id, f->cal, "locale") : 0);
    fdn_duration_format(loc, style, parts, m, negative, out, sizeof out);
    fdn_locale_part(loc, FDN_LP_LANGUAGE, lang, sizeof lang);
    if (f->approx || f->remaining) {
        char tmp[600];
        if (f->approx) { snprintf(tmp, sizeof tmp, phrase(lang, 0), out); strncpy(out, tmp, sizeof out - 1); out[511] = 0; }
        if (f->remaining) { snprintf(tmp, sizeof tmp, phrase(lang, 1), out); strncpy(out, tmp, sizeof out - 1); out[511] = 0; }
    }
    return str(out);
}
static fdn_cal dcf_fcal(dcf_t *f) {
    fdn_cal k;
    if (f->cal) {
        fdn_cal_init(&k, cs(MSG0(id, f->cal, "calendarIdentifier")), fdn_locale_ident_of(MSG0(id, f->cal, "locale")),
                     fdn_timezone_name_of(MSG0(id, f->cal, "timeZone")));
    } else fdn_cal_init(&k, "gregorian", fdn_locale_ident_of(0), 0);
    return k;
}
static id dcf_between(dcf_t *f, double a, double b) {
    u64 allowed = f->allowed ? f->allowed : (FDN_U_YEAR | FDN_U_MONTH | FDN_U_DAY | FDN_U_HOUR | FDN_U_MINUTE | FDN_U_SECOND);
    int negative = b < a;
    if (negative) { double t = a; a = b; b = t; }
    fdn_cal k = dcf_fcal(f);
    fdn_comps x;
    fdn_cal_difference(&k, allowed & ~(u64)FDN_U_NANOSECOND, a, b, &x);
    double vals[NDU];
    for (int i = 0; i < NDU; i++) {
        int64_t *p = fdn_comps_field(&x, dur_units[i].unit);
        vals[i] = p && *p != FDN_UNDEF ? (double)*p : 0;
    }
    if (allowed & FDN_U_WEEK_OF_MONTH) {
        /* semanas: o calendário devolve semanas dentro do mês; aqui são semanas inteiras de 7 dias */
        int64_t *wm = fdn_comps_field(&x, FDN_U_WEEK_OF_MONTH);
        vals[2] = wm && *wm != FDN_UNDEF ? (double)*wm : 0;
    }
    /* a fração do último segundo */
    if (allowed & FDN_U_SECOND) {
        double whole = 0;
        double acc = b - a;
        for (int i = 0; i < NDU; i++) if (allowed & dur_units[i].unit && dur_units[i].unit != FDN_U_SECOND) whole += vals[i] * dur_units[i].secs;
        if (!(allowed & (FDN_U_YEAR | FDN_U_MONTH))) vals[6] = acc - whole + vals[6] * 0;
        if (!f->fractional) vals[6] = floor(vals[6] + 1e-9);
    }
    return dcf_render(f, vals, negative);
}
static id dcf_string_ti(id self, SEL c, double ti) {
    (void)c;
    dcf_t *f = (dcf_t *)self;
    double ref = f->ref ? date_t(f->ref) : cx_now();
    return dcf_between(f, ref, ref + ti);
}
static id dcf_string_dates(id self, SEL c, id a, id b) { (void)c; return a && b ? dcf_between((dcf_t *)self, date_t(a), date_t(b)) : 0; }
static id dcf_string_comps(id self, SEL c, id comps) {
    (void)c;
    dcf_t *f = (dcf_t *)self;
    double vals[NDU];
    int neg = 0;
    if (!comps) return 0;
    u64 present = 0;
    for (int i = 0; i < NDU; i++) {
        i64 v = MSG(i64, comps, "valueForComponent:", dur_units[i].unit);
        vals[i] = v == FDN_UNDEF ? 0 : (double)v;
        if (v != FDN_UNDEF) present |= dur_units[i].unit;
        if (v < 0 && v != FDN_UNDEF) neg = 1;
    }
    if (neg) for (int i = 0; i < NDU; i++) vals[i] = fabs(vals[i]);
    u64 saved = f->allowed;
    if (!f->allowed) f->allowed = present ? present : (u64)FDN_U_SECOND;
    id r = dcf_render(f, vals, neg);
    f->allowed = saved;
    return r;
}
static id dcf_string_obj(id self, SEL c, id o) {
    if (!o) return 0;
    if (is_kind(o, (cx_class_t *)cls("NSNumber"))) return dcf_string_ti(self, c, MSG0(double, o, "doubleValue"));
    if (is_kind(o, (cx_class_t *)cls("NSDateComponents"))) return dcf_string_comps(self, c, o);
    return 0;
}
static id dcf_s_localized(id k, SEL c, id comps, i64 style) {
    (void)k; (void)c;
    dcf_t tmp;
    memset(&tmp, 0, sizeof tmp);
    tmp.style = style;
    return dcf_string_comps((id)&tmp, 0, comps);
}
#define DCFP(get, set, field, T) \
    static T dcf_g_##get(id self, SEL c) { (void)c; return (T)((dcf_t *)self)->field; } \
    static void dcf_s_##get(id self, SEL c, T v) { (void)c; ((dcf_t *)self)->field = v; }
DCFP(allowed, setAllowedUnits, allowed, u64) DCFP(style, setUnitsStyle, style, i64) DCFP(max_units, setMaximumUnitCount, max_units, i64)
DCFP(zero, setZeroFormattingBehavior, zero_behavior, i64) DCFP(context, setFormattingContext, context, i64)
DCFP(fractional, setAllowsFractionalUnits, fractional, BOOL) DCFP(collapses, setCollapsesLargestUnit, collapses, BOOL)
DCFP(approx, setIncludesApproximationPhrase, approx, BOOL) DCFP(remaining, setIncludesTimeRemainingPhrase, remaining, BOOL)
#undef DCFP
static id dcf_cal(id self, SEL c) { (void)c; dcf_t *f = (dcf_t *)self; return f->cal ? f->cal : MSG0(id, cls("NSCalendar"), "currentCalendar"); }
static void dcf_set_cal(id self, SEL c, id v) { (void)c; setcopy(&((dcf_t *)self)->cal, v); }
static id dcf_ref(id self, SEL c) { (void)c; return ((dcf_t *)self)->ref; }
static void dcf_set_ref(id self, SEL c, id v) { (void)c; setobj(&((dcf_t *)self)->ref, v); }
static BOOL dcf_get_obj(id self, SEL c, id *obj, id s, id *err) { (void)self; (void)c; (void)s; if (obj) *obj = 0; if (err) *err = 0; return NO; }
static void dcf_dealloc(id self, SEL c) { (void)c; dcf_t *f = (dcf_t *)self; setobj(&f->cal, 0); setobj(&f->ref, 0); super_dealloc(self); }

/* ======================================================================== NSRelativeDateTimeFormatter */

typedef struct { cx_class_t *isa; i64 dt_style, units_style, context; id cal, locale; } rel_t;
static id rel_init(id self, SEL c) { (void)c; return self; }
static const char *rel_loc(rel_t *r) { return fdn_locale_ident_of(r->locale ? r->locale : r->cal ? MSG0(id, r->cal, "locale") : 0); }
static int rel_width(rel_t *r) { return r->units_style == 2 ? 1 : r->units_style == 3 ? 2 : 0; }
static id rel_unit(rel_t *r, double v, int unit) {
    char b[256];
    fdn_reldate(rel_loc(r), v, unit, rel_width(r), r->dt_style == 1, r->units_style == 1, b, sizeof b);
    return str(b);
}
/* a maior unidade não nula entre as duas datas (ano, mês, semana, dia, hora, minuto, segundo) */
static id rel_between(rel_t *r, double ref, double date) {
    fdn_cal k;
    fdn_comps x;
    if (r->cal) fdn_cal_init(&k, cs(MSG0(id, r->cal, "calendarIdentifier")), rel_loc(r), fdn_timezone_name_of(MSG0(id, r->cal, "timeZone")));
    else fdn_cal_init(&k, "gregorian", rel_loc(r), 0);
    int neg = date < ref;
    fdn_cal_difference(&k, FDN_U_YEAR | FDN_U_MONTH | FDN_U_WEEK_OF_MONTH | FDN_U_DAY | FDN_U_HOUR | FDN_U_MINUTE | FDN_U_SECOND,
                       neg ? date : ref, neg ? ref : date, &x);
    static const struct { int off, unit; } order[] = {
        {0, 0 /* YEAR */}, {1, 2 /* MONTH */}, {2, 3 /* WEEK */}, {3, 4 /* DAY */}, {4, 5 /* HOUR */}, {5, 6 /* MINUTE */}, {6, 7 /* SECOND */},
    };
    int64_t v[7] = {x.year, x.month, x.week_of_month, x.day, x.hour, x.minute, x.second};
    for (int i = 0; i < 7; i++) {
        if (v[i] != FDN_UNDEF && v[i] != 0) return rel_unit(r, neg ? -(double)v[i] : (double)v[i], order[i].unit);
    }
    return rel_unit(r, 0, 7);
}
static id rel_for_date(id self, SEL c, id date, id ref) { (void)c; return date && ref ? rel_between((rel_t *)self, date_t(ref), date_t(date)) : 0; }
static id rel_for_ti(id self, SEL c, double ti) { (void)c; double now = cx_now(); return rel_between((rel_t *)self, now, now + ti); }
static id rel_for_comps(id self, SEL c, id comps) {
    (void)c;
    rel_t *r = (rel_t *)self;
    static const struct { u64 u; int unit; } order[] = {{FDN_U_YEAR, 0}, {FDN_U_MONTH, 2}, {FDN_U_WEEK_OF_MONTH, 3}, {FDN_U_DAY, 4},
                                                       {FDN_U_HOUR, 5}, {FDN_U_MINUTE, 6}, {FDN_U_SECOND, 7}};
    if (!comps) return 0;
    for (size_t i = 0; i < sizeof order / sizeof order[0]; i++) {
        i64 v = MSG(i64, comps, "valueForComponent:", order[i].u);
        if (order[i].u == FDN_U_WEEK_OF_MONTH && (v == FDN_UNDEF || v == 0)) v = MSG(i64, comps, "valueForComponent:", (u64)FDN_U_WEEK_OF_YEAR);
        if (v != FDN_UNDEF && v != 0) return rel_unit(r, (double)v, order[i].unit);
    }
    return rel_unit(r, 0, 7);
}
static id rel_string_obj(id self, SEL c, id o) { return o && is_kind(o, (cx_class_t *)cls("NSDate")) ? rel_for_date(self, c, o, date_new(cx_now())) : 0; }
#define RELP(get, set, field) \
    static i64 rel_g_##get(id self, SEL c) { (void)c; return ((rel_t *)self)->field; } \
    static void rel_s_##get(id self, SEL c, i64 v) { (void)c; ((rel_t *)self)->field = v; }
RELP(dt, setDateTimeStyle, dt_style) RELP(units, setUnitsStyle, units_style) RELP(ctx, setFormattingContext, context)
#undef RELP
static id rel_cal(id self, SEL c) { (void)c; rel_t *r = (rel_t *)self; return r->cal ? r->cal : MSG0(id, cls("NSCalendar"), "currentCalendar"); }
static void rel_set_cal(id self, SEL c, id v) { (void)c; setcopy(&((rel_t *)self)->cal, v); }
static id rel_locale(id self, SEL c) { (void)c; return loc_or_current(((rel_t *)self)->locale); }
static void rel_set_locale(id self, SEL c, id v) { (void)c; setobj(&((rel_t *)self)->locale, v); }
static void rel_dealloc(id self, SEL c) { (void)c; rel_t *r = (rel_t *)self; setobj(&r->cal, 0); setobj(&r->locale, 0); super_dealloc(self); }

/* ======================================================================== NSListFormatter */

typedef struct { cx_class_t *isa; id locale, item; } lf_t;
static id lf_join(const char *loc, id items, id itemfmt) {
    u64 n = items ? MSG0(u64, items, "count") : 0;
    if (!n) return str("");
    const char **v = malloc(sizeof(char *) * n);
    char **own = malloc(sizeof(char *) * n);
    char *out = malloc(16384);
    if (!v || !own || !out) { free(v); free(own); free(out); return 0; }
    for (u64 i = 0; i < n; i++) {
        id o = MSG(id, items, "objectAtIndex:", i);
        id s = itemfmt ? MSG(id, itemfmt, "stringForObjectValue:", o) : 0;
        if (!s) s = cx_is_str(o) ? o : MSG0(id, o, "description");
        own[i] = strdup(cs(s));
        v[i] = own[i];
    }
    fdn_list_format(loc, 0, 0, v, (int)n, out, 16384);
    id r = str(out);
    for (u64 i = 0; i < n; i++) free(own[i]);
    free(v); free(own); free(out);
    return r;
}
static id lf_from_items(id self, SEL c, id items) { (void)c; lf_t *l = (lf_t *)self; return lf_join(fdn_locale_ident_of(l->locale), items, l->item); }
static id lf_string_obj(id self, SEL c, id o) { return o && is_kind(o, (cx_class_t *)cls("NSArray")) ? lf_from_items(self, c, o) : 0; }
static id lf_s_join(id k, SEL c, id items) { (void)k; (void)c; return lf_join(fdn_locale_ident_of(0), items, 0); }
static id lf_locale(id self, SEL c) { (void)c; return loc_or_current(((lf_t *)self)->locale); }
static void lf_set_locale(id self, SEL c, id v) { (void)c; setobj(&((lf_t *)self)->locale, v); }
static id lf_item(id self, SEL c) { (void)c; return ((lf_t *)self)->item; }
static void lf_set_item(id self, SEL c, id v) { (void)c; setcopy(&((lf_t *)self)->item, v); }
static void lf_dealloc(id self, SEL c) { (void)c; lf_t *l = (lf_t *)self; setobj(&l->locale, 0); setobj(&l->item, 0); super_dealloc(self); }

/* ======================================================================== NSDateInterval */

typedef struct { cx_class_t *isa; double start, dur; } dint_t;
static id di_init(id self, SEL c) { (void)c; dint_t *d = (dint_t *)self; d->start = cx_now(); d->dur = 0; return self; }
static id di_init_dur(id self, SEL c, id start, double dur) {
    (void)c;
    dint_t *d = (dint_t *)self;
    if (dur < 0) { objc_release(self); return 0; }   /* a Foundation lança; aqui devolve nil */
    d->start = date_t(start);
    d->dur = dur;
    return self;
}
static id di_init_end(id self, SEL c, id start, id end) { return di_init_dur(self, c, start, date_t(end) - date_t(start)); }
static id di_start(id self, SEL c) { (void)c; return date_new(((dint_t *)self)->start); }
static id di_end(id self, SEL c) { (void)c; dint_t *d = (dint_t *)self; return date_new(d->start + d->dur); }
static double di_duration(id self, SEL c) { (void)c; return ((dint_t *)self)->dur; }
static void di_set_start(id self, SEL c, id v) { (void)c; ((dint_t *)self)->start = date_t(v); }
static void di_set_end(id self, SEL c, id v) { (void)c; dint_t *d = (dint_t *)self; d->dur = date_t(v) - d->start; }
static void di_set_duration(id self, SEL c, double v) { (void)c; ((dint_t *)self)->dur = v; }
static i64 di_compare(id self, SEL c, id o) {
    (void)c;
    dint_t *a = (dint_t *)self, *b = (dint_t *)o;
    if (a->start != b->start) return a->start < b->start ? -1 : 1;
    if (a->dur != b->dur) return a->dur < b->dur ? -1 : 1;
    return 0;
}
static BOOL di_equal_di(id self, SEL c, id o) { return o && is_kind(o, &NSDateInterval_c) && di_compare(self, c, o) == 0; }
static BOOL di_intersects(id self, SEL c, id o) {
    (void)c;
    dint_t *a = (dint_t *)self, *b = (dint_t *)o;
    return o && a->start <= b->start + b->dur && b->start <= a->start + a->dur;
}
static id di_intersection(id self, SEL c, id o) {
    if (!di_intersects(self, c, o)) return 0;
    dint_t *a = (dint_t *)self, *b = (dint_t *)o;
    double s = a->start > b->start ? a->start : b->start;
    double e1 = a->start + a->dur, e2 = b->start + b->dur;
    double e = e1 < e2 ? e1 : e2;
    dint_t *r = (dint_t *)cx_alloc_instance(&NSDateInterval_c);
    r->start = s; r->dur = e - s;
    return objc_autorelease((id)r);
}
static BOOL di_contains(id self, SEL c, id date) { (void)c; dint_t *d = (dint_t *)self; double t = date_t(date); return t >= d->start && t <= d->start + d->dur; }
static id di_copy(id self, SEL c) { (void)c; dint_t *o = (dint_t *)self, *d = (dint_t *)cx_alloc_instance(&NSDateInterval_c); d->start = o->start; d->dur = o->dur; return (id)d; }
static u64 di_hash(id self, SEL c) { (void)c; dint_t *d = (dint_t *)self; u64 a, b; memcpy(&a, &d->start, 8); memcpy(&b, &d->dur, 8); return a ^ (b * 31); }
static id di_desc(id self, SEL c) {
    (void)c;
    id s = MSG0(id, di_start(self, 0), "description"), e = MSG0(id, di_end(self, 0), "description");
    char b[160];
    snprintf(b, sizeof b, "%s to %s", cs(s), cs(e));
    return str(b);
}
static void di_encode(id self, SEL c, id coder) {
    (void)c;
    dint_t *d = (dint_t *)self;
    MSG(void, coder, "encodeDouble:forKey:", d->start - 978307200.0, cx_str("NS.startDate"));
    MSG(void, coder, "encodeDouble:forKey:", d->dur, cx_str("NS.duration"));
}
static id di_init_coder(id self, SEL c, id coder) {
    (void)c;
    dint_t *d = (dint_t *)self;
    d->start = MSG(double, coder, "decodeDoubleForKey:", cx_str("NS.startDate")) + 978307200.0;
    d->dur = MSG(double, coder, "decodeDoubleForKey:", cx_str("NS.duration"));
    return self;
}

/* ======================================================================== NSDateIntervalFormatter */

typedef struct { cx_class_t *isa; i64 ds, ts; id tmpl, locale, cal, tz; } dif_t;
static id dif_init(id self, SEL c) { (void)c; dif_t *f = (dif_t *)self; f->ds = 2; f->ts = 1; return self; }
static id dif_between(dif_t *f, double a, double b) {
    char sk[64] = "", out[512];
    if (f->tmpl) snprintf(sk, sizeof sk, "%s", cs(f->tmpl));
    else {
        static const char *const dsk[] = {"", "yMd", "yMMMd", "yMMMMd", "yMMMMEEEEd"};
        static const char *const tsk[] = {"", "jm", "jms", "jmsz", "jmszzzz"};
        snprintf(sk, sizeof sk, "%s%s", dsk[f->ds >= 0 && f->ds <= 4 ? f->ds : 0], tsk[f->ts >= 0 && f->ts <= 4 ? f->ts : 0]);
    }
    if (!sk[0]) return str("");
    const char *tz = fdn_timezone_name_of(f->tz ? f->tz : f->cal ? MSG0(id, f->cal, "timeZone") : MSG0(id, cls("NSTimeZone"), "defaultTimeZone"));
    char loc[160];
    const char *base = fdn_locale_ident_of(f->locale);
    if (f->cal) fdn_loc_with_keyword(base, "calendar", cs(MSG0(id, f->cal, "calendarIdentifier")), loc, sizeof loc);
    else snprintf(loc, sizeof loc, "%s", base);
    fdn_interval_format(loc, sk, tz, a, b, out, sizeof out);
    return str(out);
}
static id dif_from_dates(id self, SEL c, id a, id b) { (void)c; return a && b ? dif_between((dif_t *)self, date_t(a), date_t(b)) : 0; }
static id dif_from_interval(id self, SEL c, id di) {
    (void)c;
    if (!di) return 0;
    dint_t *d = (dint_t *)di;
    return dif_between((dif_t *)self, d->start, d->start + d->dur);
}
#define DIFP(get, set, field) \
    static i64 dif_g_##get(id self, SEL c) { (void)c; return ((dif_t *)self)->field; } \
    static void dif_s_##get(id self, SEL c, i64 v) { (void)c; ((dif_t *)self)->field = v; }
DIFP(ds, setDateStyle, ds) DIFP(ts, setTimeStyle, ts)
#undef DIFP
#define DIFO(get, set, field, setter) \
    static id dif_go_##get(id self, SEL c) { (void)c; return ((dif_t *)self)->field; } \
    static void dif_so_##get(id self, SEL c, id v) { (void)c; setter(&((dif_t *)self)->field, v); }
DIFO(tmpl, setDateTemplate, tmpl, setcopy) DIFO(locale, setLocale, locale, setobj) DIFO(cal, setCalendar, cal, setcopy) DIFO(tz, setTimeZone, tz, setobj)
#undef DIFO
static void dif_dealloc(id self, SEL c) { (void)c; dif_t *f = (dif_t *)self; setobj(&f->tmpl, 0); setobj(&f->locale, 0); setobj(&f->cal, 0); setobj(&f->tz, 0); super_dealloc(self); }

/* ======================================================================== NSPersonNameComponents (+ Formatter) */

enum { PN_PREFIX, PN_GIVEN, PN_MIDDLE, PN_FAMILY, PN_SUFFIX, PN_NICK, PN_N };
typedef struct { cx_class_t *isa; id v[PN_N]; id phonetic; } pn_t;
static const char *const pn_keys[PN_N] = {"NS.namePrefix", "NS.givenName", "NS.middleName", "NS.familyName", "NS.nameSuffix", "NS.nickname"};
#define PNP(get, set, i) \
    static id pn_g_##get(id self, SEL c) { (void)c; return ((pn_t *)self)->v[i]; } \
    static void pn_s_##get(id self, SEL c, id x) { (void)c; setcopy(&((pn_t *)self)->v[i], x); }
PNP(prefix, setNamePrefix, PN_PREFIX) PNP(given, setGivenName, PN_GIVEN) PNP(middle, setMiddleName, PN_MIDDLE)
PNP(family, setFamilyName, PN_FAMILY) PNP(suffix, setNameSuffix, PN_SUFFIX) PNP(nick, setNickname, PN_NICK)
#undef PNP
static id pn_phonetic(id self, SEL c) { (void)c; return ((pn_t *)self)->phonetic; }
static void pn_set_phonetic(id self, SEL c, id x) { (void)c; setcopy(&((pn_t *)self)->phonetic, x); }
static int str_eq(id a, id b) { if (!a || !b) return a == b; return MSG(BOOL, a, "isEqualToString:", b); }
static BOOL pn_equal(id self, SEL c, id o) {
    (void)c;
    if (self == o) return YES;
    if (!is_kind(o, &NSPersonNameComponents_c)) return NO;
    pn_t *a = (pn_t *)self, *b = (pn_t *)o;
    for (int i = 0; i < PN_N; i++) if (!str_eq(a->v[i], b->v[i])) return NO;
    if ((a->phonetic != 0) != (b->phonetic != 0)) return NO;
    return !a->phonetic || MSG(BOOL, a->phonetic, "isEqual:", b->phonetic);
}
static u64 pn_hash(id self, SEL c) { (void)c; pn_t *p = (pn_t *)self; u64 h = 0; for (int i = 0; i < PN_N; i++) h = h * 31 + (p->v[i] ? MSG0(u64, p->v[i], "hash") : 0); return h; }
static id pn_copy(id self, SEL c) {
    (void)c;
    pn_t *o = (pn_t *)self, *p = (pn_t *)cx_alloc_instance(&NSPersonNameComponents_c);
    for (int i = 0; i < PN_N; i++) p->v[i] = o->v[i] ? objc_retain(o->v[i]) : 0;
    p->phonetic = o->phonetic ? MSG0(id, o->phonetic, "copy") : 0;
    return (id)p;
}
static void pn_encode(id self, SEL c, id coder) {
    (void)c;
    pn_t *p = (pn_t *)self;
    for (int i = 0; i < PN_N; i++) if (p->v[i]) MSG(void, coder, "encodeObject:forKey:", p->v[i], cx_str(pn_keys[i]));
    if (p->phonetic) MSG(void, coder, "encodeObject:forKey:", p->phonetic, cx_str("NS.phoneticRepresentation"));
}
static id pn_init_coder(id self, SEL c, id coder) {
    (void)c;
    pn_t *p = (pn_t *)self;
    for (int i = 0; i < PN_N; i++) setobj(&p->v[i], MSG(id, coder, "decodeObjectForKey:", cx_str(pn_keys[i])));
    setobj(&p->phonetic, MSG(id, coder, "decodeObjectForKey:", cx_str("NS.phoneticRepresentation")));
    return self;
}
static id pn_desc(id self, SEL c) {
    (void)c;
    pn_t *p = (pn_t *)self;
    char b[400];
    int o = snprintf(b, sizeof b, "<NSPersonNameComponents>");
    static const char *const names[PN_N] = {"namePrefix", "givenName", "middleName", "familyName", "nameSuffix", "nickname"};
    for (int i = 0; i < PN_N; i++) if (p->v[i]) o += snprintf(b + o, sizeof b - (size_t)o, " %s: %s", names[i], cs(p->v[i]));
    return str(b);
}
static void pn_dealloc(id self, SEL c) { (void)c; pn_t *p = (pn_t *)self; for (int i = 0; i < PN_N; i++) setobj(&p->v[i], 0); setobj(&p->phonetic, 0); super_dealloc(self); }

typedef struct { cx_class_t *isa; i64 style; int phonetic; id locale; } pnf_t;
static int family_first(const char *loc) {
    char l[16];
    fdn_locale_part(loc, FDN_LP_LANGUAGE, l, sizeof l);
    return !strcmp(l, "ja") || !strcmp(l, "zh") || !strcmp(l, "ko") || !strcmp(l, "hu") || !strcmp(l, "vi");
}
static int cjk(const char *loc) {
    char l[16];
    fdn_locale_part(loc, FDN_LP_LANGUAGE, l, sizeof l);
    return !strcmp(l, "ja") || !strcmp(l, "zh") || !strcmp(l, "ko");
}
static void initial(const char *s, char *out) {
    /* primeiro caractere (UTF-8 inteiro) */
    int n = 1;
    unsigned char c0 = (unsigned char)s[0];
    if (c0 >= 0xF0) n = 4; else if (c0 >= 0xE0) n = 3; else if (c0 >= 0xC0) n = 2;
    memcpy(out, s, (size_t)n);
    out[n] = 0;
}
static id pn_format(id comps, i64 style, int phonetic, const char *loc) {
    if (!comps) return 0;
    pn_t *p = (pn_t *)comps;
    if (phonetic && p->phonetic) p = (pn_t *)p->phonetic;
    const char *v[PN_N];
    for (int i = 0; i < PN_N; i++) v[i] = p->v[i] ? cs(p->v[i]) : "";
    char b[512] = "";
    const char *sep = cjk(loc) ? "" : " ";
    int ff = family_first(loc);
    const char *seq[6];
    int n = 0;
    switch (style) {
    case 1: /* curto: o apelido ou o primeiro nome */
        if (v[PN_NICK][0]) seq[n++] = v[PN_NICK];
        else if (v[PN_GIVEN][0]) seq[n++] = v[PN_GIVEN];
        else seq[n++] = v[PN_FAMILY];
        break;
    case 4: { /* iniciais */
        char a[8] = "", f[8] = "";
        if (v[PN_GIVEN][0]) initial(v[PN_GIVEN], a);
        if (v[PN_FAMILY][0]) initial(v[PN_FAMILY], f);
        snprintf(b, sizeof b, "%s%s", ff ? f : a, ff ? a : f);
        return str(b);
    }
    case 3: /* longo: prefixo, nome, meio, sobrenome, sufixo */
        if (ff) { seq[n++] = v[PN_FAMILY]; seq[n++] = v[PN_GIVEN]; seq[n++] = v[PN_MIDDLE]; }
        else { seq[n++] = v[PN_PREFIX]; seq[n++] = v[PN_GIVEN]; seq[n++] = v[PN_MIDDLE]; seq[n++] = v[PN_FAMILY]; seq[n++] = v[PN_SUFFIX]; }
        break;
    default: /* padrão e médio: nome e sobrenome */
        if (ff) { seq[n++] = v[PN_FAMILY]; seq[n++] = v[PN_GIVEN]; }
        else { seq[n++] = v[PN_GIVEN]; seq[n++] = v[PN_FAMILY]; }
        if (style == 0 && !v[PN_GIVEN][0] && !v[PN_FAMILY][0] && v[PN_NICK][0]) seq[n++] = v[PN_NICK];
        break;
    }
    int o = 0;
    for (int i = 0; i < n; i++) {
        if (!seq[i][0]) continue;
        o += snprintf(b + o, sizeof b - (size_t)o, "%s%s", o ? sep : "", seq[i]);
        if (o >= (int)sizeof b) break;
    }
    return str(b);
}
static id pnf_string(id self, SEL c, id comps) { (void)c; pnf_t *f = (pnf_t *)self; return pn_format(comps, f->style, f->phonetic, fdn_locale_ident_of(f->locale)); }
static id pnf_string_obj(id self, SEL c, id o) { return is_kind(o, &NSPersonNameComponents_c) ? pnf_string(self, c, o) : 0; }
static id pnf_annotated(id self, SEL c, id comps) {
    id s = pnf_string(self, c, comps);
    return s ? objc_autorelease(MSG(id, MSG0(id, cls("NSAttributedString"), "alloc"), "initWithString:", s)) : 0;
}
static id pnf_from_string(id self, SEL c, id s) {
    (void)c;
    pnf_t *f = (pnf_t *)self;
    if (!s) return 0;
    const char *t = cs(s);
    pn_t *p = (pn_t *)cx_alloc_instance(&NSPersonNameComponents_c);
    char *buf = strdup(t), *w[16];
    int n = 0;
    for (char *q = strtok(buf, " \t"); q && n < 16; q = strtok(0, " \t")) w[n++] = q;
    if (n == 1) p->v[family_first(fdn_locale_ident_of(f->locale)) ? PN_FAMILY : PN_GIVEN] = objc_retain(cx_str(w[0]));
    else if (n >= 2) {
        int ff = family_first(fdn_locale_ident_of(f->locale));
        p->v[ff ? PN_FAMILY : PN_GIVEN] = objc_retain(cx_str(w[0]));
        p->v[ff ? PN_GIVEN : PN_FAMILY] = objc_retain(cx_str(w[n - 1]));
        if (n > 2) {
            char mid[256] = "";
            for (int i = 1; i < n - 1; i++) { if (mid[0]) strncat(mid, " ", sizeof mid - strlen(mid) - 1); strncat(mid, w[i], sizeof mid - strlen(mid) - 1); }
            p->v[PN_MIDDLE] = objc_retain(cx_str(mid));
        }
    }
    free(buf);
    return objc_autorelease((id)p);
}
static BOOL pnf_get_obj(id self, SEL c, id *obj, id s, id *err) { id p = pnf_from_string(self, c, s); if (obj) *obj = p; if (err) *err = 0; return p != 0; }
static id pnf_s_localized(id k, SEL c, id comps, i64 style, u64 opts) { (void)k; (void)c; return pn_format(comps, style, (opts & 2) != 0, fdn_locale_ident_of(0)); }
static i64 pnf_style(id self, SEL c) { (void)c; return ((pnf_t *)self)->style; }
static void pnf_set_style(id self, SEL c, i64 v) { (void)c; ((pnf_t *)self)->style = v; }
static BOOL pnf_phonetic(id self, SEL c) { (void)c; return (BOOL)((pnf_t *)self)->phonetic; }
static void pnf_set_phonetic(id self, SEL c, BOOL v) { (void)c; ((pnf_t *)self)->phonetic = v; }
static id pnf_locale(id self, SEL c) { (void)c; return loc_or_current(((pnf_t *)self)->locale); }
static void pnf_set_locale(id self, SEL c, id v) { (void)c; setobj(&((pnf_t *)self)->locale, v); }

/* ======================================================================== CF (toll-free) */

typedef struct { long location, length; } cf_range_t;
#define CF_EPOCH 978307200.0
static id cfret(id o) { return o ? objc_retain(o) : 0; }

#define CFK(var, text) static struct { void *isa; u64 flags; const char *s; u64 n; } var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; const void *var = &var##_s;
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
CFK(kCFDateFormatterIsLenient, "kCFDateFormatterIsLenientKey")
CFK(kCFDateFormatterTimeZone, "kCFDateFormatterTimeZoneKey")
CFK(kCFDateFormatterCalendarName, "kCFDateFormatterCalendarNameKey")
CFK(kCFDateFormatterDefaultFormat, "kCFDateFormatterDefaultFormatKey")
CFK(kCFDateFormatterTwoDigitStartDate, "kCFDateFormatterTwoDigitStartDateKey")
CFK(kCFDateFormatterDefaultDate, "kCFDateFormatterDefaultDateKey")
CFK(kCFDateFormatterCalendar, "kCFDateFormatterCalendarKey")
CFK(kCFDateFormatterEraSymbols, "kCFDateFormatterEraSymbolsKey")
CFK(kCFDateFormatterMonthSymbols, "kCFDateFormatterMonthSymbolsKey")
CFK(kCFDateFormatterShortMonthSymbols, "kCFDateFormatterShortMonthSymbolsKey")
CFK(kCFDateFormatterWeekdaySymbols, "kCFDateFormatterWeekdaySymbolsKey")
CFK(kCFDateFormatterShortWeekdaySymbols, "kCFDateFormatterShortWeekdaySymbolsKey")
CFK(kCFDateFormatterAMSymbol, "kCFDateFormatterAMSymbolKey")
CFK(kCFDateFormatterPMSymbol, "kCFDateFormatterPMSymbolKey")
CFK(kCFDateFormatterLongEraSymbols, "kCFDateFormatterLongEraSymbolsKey")
CFK(kCFDateFormatterVeryShortMonthSymbols, "kCFDateFormatterVeryShortMonthSymbolsKey")
CFK(kCFDateFormatterStandaloneMonthSymbols, "kCFDateFormatterStandaloneMonthSymbolsKey")
CFK(kCFDateFormatterShortStandaloneMonthSymbols, "kCFDateFormatterShortStandaloneMonthSymbolsKey")
CFK(kCFDateFormatterVeryShortStandaloneMonthSymbols, "kCFDateFormatterVeryShortStandaloneMonthSymbolsKey")
CFK(kCFDateFormatterVeryShortWeekdaySymbols, "kCFDateFormatterVeryShortWeekdaySymbolsKey")
CFK(kCFDateFormatterStandaloneWeekdaySymbols, "kCFDateFormatterStandaloneWeekdaySymbolsKey")
CFK(kCFDateFormatterShortStandaloneWeekdaySymbols, "kCFDateFormatterShortStandaloneWeekdaySymbolsKey")
CFK(kCFDateFormatterVeryShortStandaloneWeekdaySymbols, "kCFDateFormatterVeryShortStandaloneWeekdaySymbolsKey")
CFK(kCFDateFormatterQuarterSymbols, "kCFDateFormatterQuarterSymbolsKey")
CFK(kCFDateFormatterShortQuarterSymbols, "kCFDateFormatterShortQuarterSymbolsKey")
CFK(kCFDateFormatterStandaloneQuarterSymbols, "kCFDateFormatterStandaloneQuarterSymbolsKey")
CFK(kCFDateFormatterShortStandaloneQuarterSymbols, "kCFDateFormatterShortStandaloneQuarterSymbolsKey")
CFK(kCFDateFormatterGregorianStartDate, "kCFDateFormatterGregorianStartDateKey")
CFK(kCFDateFormatterDoesRelativeDateFormattingKey, "kCFDateFormatterDoesRelativeDateFormattingKey")
CFK(kCFNumberFormatterCurrencyCode, "kCFNumberFormatterCurrencyCodeKey")
CFK(kCFNumberFormatterDecimalSeparator, "kCFNumberFormatterDecimalSeparatorKey")
CFK(kCFNumberFormatterCurrencyDecimalSeparator, "kCFNumberFormatterCurrencyDecimalSeparatorKey")
CFK(kCFNumberFormatterAlwaysShowDecimalSeparator, "kCFNumberFormatterAlwaysShowDecimalSeparatorKey")
CFK(kCFNumberFormatterGroupingSeparator, "kCFNumberFormatterGroupingSeparatorKey")
CFK(kCFNumberFormatterUseGroupingSeparator, "kCFNumberFormatterUseGroupingSeparatorKey")
CFK(kCFNumberFormatterPercentSymbol, "kCFNumberFormatterPercentSymbolKey")
CFK(kCFNumberFormatterZeroSymbol, "kCFNumberFormatterZeroSymbolKey")
CFK(kCFNumberFormatterNaNSymbol, "kCFNumberFormatterNaNSymbolKey")
CFK(kCFNumberFormatterInfinitySymbol, "kCFNumberFormatterInfinitySymbolKey")
CFK(kCFNumberFormatterMinusSign, "kCFNumberFormatterMinusSignKey")
CFK(kCFNumberFormatterPlusSign, "kCFNumberFormatterPlusSignKey")
CFK(kCFNumberFormatterCurrencySymbol, "kCFNumberFormatterCurrencySymbolKey")
CFK(kCFNumberFormatterExponentSymbol, "kCFNumberFormatterExponentSymbolKey")
CFK(kCFNumberFormatterMinIntegerDigits, "kCFNumberFormatterMinIntegerDigitsKey")
CFK(kCFNumberFormatterMaxIntegerDigits, "kCFNumberFormatterMaxIntegerDigitsKey")
CFK(kCFNumberFormatterMinFractionDigits, "kCFNumberFormatterMinFractionDigitsKey")
CFK(kCFNumberFormatterMaxFractionDigits, "kCFNumberFormatterMaxFractionDigitsKey")
CFK(kCFNumberFormatterGroupingSize, "kCFNumberFormatterGroupingSizeKey")
CFK(kCFNumberFormatterSecondaryGroupingSize, "kCFNumberFormatterSecondaryGroupingSizeKey")
CFK(kCFNumberFormatterRoundingMode, "kCFNumberFormatterRoundingModeKey")
CFK(kCFNumberFormatterRoundingIncrement, "kCFNumberFormatterRoundingIncrementKey")
CFK(kCFNumberFormatterFormatWidth, "kCFNumberFormatterFormatWidthKey")
CFK(kCFNumberFormatterPaddingPosition, "kCFNumberFormatterPaddingPositionKey")
CFK(kCFNumberFormatterPaddingCharacter, "kCFNumberFormatterPaddingCharacterKey")
CFK(kCFNumberFormatterDefaultFormat, "kCFNumberFormatterDefaultFormatKey")
CFK(kCFNumberFormatterMultiplier, "kCFNumberFormatterMultiplierKey")
CFK(kCFNumberFormatterPositivePrefix, "kCFNumberFormatterPositivePrefixKey")
CFK(kCFNumberFormatterPositiveSuffix, "kCFNumberFormatterPositiveSuffixKey")
CFK(kCFNumberFormatterNegativePrefix, "kCFNumberFormatterNegativePrefixKey")
CFK(kCFNumberFormatterNegativeSuffix, "kCFNumberFormatterNegativeSuffixKey")
CFK(kCFNumberFormatterPerMillSymbol, "kCFNumberFormatterPerMillSymbolKey")
CFK(kCFNumberFormatterInternationalCurrencySymbol, "kCFNumberFormatterInternationalCurrencySymbolKey")
CFK(kCFNumberFormatterCurrencyGroupingSeparator, "kCFNumberFormatterCurrencyGroupingSeparatorKey")
CFK(kCFNumberFormatterIsLenient, "kCFNumberFormatterIsLenientKey")
CFK(kCFNumberFormatterUseSignificantDigits, "kCFNumberFormatterUseSignificantDigitsKey")
CFK(kCFNumberFormatterMinSignificantDigits, "kCFNumberFormatterMinSignificantDigitsKey")
CFK(kCFNumberFormatterMaxSignificantDigits, "kCFNumberFormatterMaxSignificantDigitsKey")
CFK(kCFNumberFormatterMinGroupingDigits, "kCFNumberFormatterMinGroupingDigitsKey")

u64 CFDateFormatterGetTypeID(void) { return 33; }
id CFDateFormatterCreate(void *alloc, id locale, long ds, long ts) {
    (void)alloc;
    id f = MSG0(id, cx_alloc_instance(&NSDateFormatter_c), "init");
    df_set_locale(f, 0, locale);
    df_set_date_style(f, 0, (u64)ds);
    df_set_time_style(f, 0, (u64)ts);
    return f;
}
id CFDateFormatterCreateISO8601Formatter(void *alloc, unsigned long opts) {
    (void)alloc;
    id f = MSG0(id, cx_alloc_instance(&NSISO8601DateFormatter_c), "init");
    iso_set_options(f, 0, opts);
    return f;
}
id CFDateFormatterGetLocale(id f) { return df_locale(f, 0); }
long CFDateFormatterGetDateStyle(id f) { return (long)df_date_style(f, 0); }
long CFDateFormatterGetTimeStyle(id f) { return (long)df_time_style(f, 0); }
id CFDateFormatterGetFormat(id f) { return is_kind(f, &NSISO8601DateFormatter_c) ? 0 : df_format0(f, 0); }
void CFDateFormatterSetFormat(id f, id fmt) { df_set_format(f, 0, fmt); }
id CFDateFormatterCreateStringWithDate(void *alloc, id f, id date) {
    (void)alloc;
    return cfret(is_kind(f, &NSISO8601DateFormatter_c) ? iso_string(f, 0, date) : df_string(f, 0, date));
}
id CFDateFormatterCreateStringWithAbsoluteTime(void *alloc, id f, double at) { return CFDateFormatterCreateStringWithDate(alloc, f, date_new(at + CF_EPOCH)); }
id CFDateFormatterCreateDateFromString(void *alloc, id f, id s, cf_range_t *rangep) {
    (void)alloc;
    if (is_kind(f, &NSISO8601DateFormatter_c)) return cfret(iso_date(f, 0, s));
    ns_range_t r = {rangep ? rangep->location : 0, rangep ? rangep->length : 0};
    id d = df_date_range((df_t *)f, s, rangep ? &r : 0);
    if (d && rangep) rangep->length = r.len;
    return cfret(d);
}
unsigned char CFDateFormatterGetAbsoluteTimeFromString(id f, id s, cf_range_t *rangep, double *atp) {
    id d = CFDateFormatterCreateDateFromString(0, f, s, rangep);
    if (!d) return 0;
    if (atp) *atp = date_t(d) - CF_EPOCH;
    objc_release(d);
    return 1;
}
id CFDateFormatterCreateDateFormatFromTemplate(void *alloc, id tmpl, unsigned long opts, id locale) {
    (void)alloc;
    return cfret(df_s_from_template(0, 0, tmpl, opts, locale));
}
static int cfkey(id key, const void *k) { return key == (id)k || (key && !strcmp(cs(key), ((const char *const *)k)[2])); }
void CFDateFormatterSetProperty(id f, id key, id v) {
    if (cfkey(key, kCFDateFormatterIsLenient)) df_set_lenient(f, 0, v ? MSG0(BOOL, v, "boolValue") : NO);
    else if (cfkey(key, kCFDateFormatterTimeZone)) df_set_tz(f, 0, v);
    else if (cfkey(key, kCFDateFormatterCalendar)) df_set_cal(f, 0, v);
    else if (cfkey(key, kCFDateFormatterCalendarName)) df_set_cal(f, 0, MSG(id, cls("NSCalendar"), "calendarWithIdentifier:", v));
    else if (cfkey(key, kCFDateFormatterTwoDigitStartDate)) df_set_two_digit(f, 0, v);
    else if (cfkey(key, kCFDateFormatterDefaultDate)) df_set_default_date(f, 0, v);
    else if (cfkey(key, kCFDateFormatterDoesRelativeDateFormattingKey)) df_set_relative(f, 0, v ? MSG0(BOOL, v, "boolValue") : NO);
    else if (cfkey(key, kCFDateFormatterAMSymbol)) df_set_am(f, 0, v);
    else if (cfkey(key, kCFDateFormatterPMSymbol)) df_set_pm(f, 0, v);
    else if (cfkey(key, kCFDateFormatterEraSymbols)) df_s_eras(f, 0, v);
    else if (cfkey(key, kCFDateFormatterMonthSymbols)) df_s_months(f, 0, v);
    else if (cfkey(key, kCFDateFormatterShortMonthSymbols)) df_s_short_months(f, 0, v);
    else if (cfkey(key, kCFDateFormatterWeekdaySymbols)) df_s_weekdays(f, 0, v);
    else if (cfkey(key, kCFDateFormatterShortWeekdaySymbols)) df_s_short_weekdays(f, 0, v);
}
id CFDateFormatterCopyProperty(id f, id key) {
    id r = 0;
    if (cfkey(key, kCFDateFormatterIsLenient)) r = MSG(id, cls("NSNumber"), "numberWithBool:", df_lenient(f, 0));
    else if (cfkey(key, kCFDateFormatterTimeZone)) r = df_tz(f, 0);
    else if (cfkey(key, kCFDateFormatterCalendar)) r = df_cal(f, 0);
    else if (cfkey(key, kCFDateFormatterCalendarName)) r = MSG0(id, df_cal(f, 0), "calendarIdentifier");
    else if (cfkey(key, kCFDateFormatterDefaultFormat)) r = df_format0(f, 0);
    else if (cfkey(key, kCFDateFormatterTwoDigitStartDate)) r = df_two_digit(f, 0);
    else if (cfkey(key, kCFDateFormatterDefaultDate)) r = df_default_date(f, 0);
    else if (cfkey(key, kCFDateFormatterDoesRelativeDateFormattingKey)) r = MSG(id, cls("NSNumber"), "numberWithBool:", df_relative(f, 0));
    else if (cfkey(key, kCFDateFormatterAMSymbol)) r = df_am(f, 0);
    else if (cfkey(key, kCFDateFormatterPMSymbol)) r = df_pm(f, 0);
    else if (cfkey(key, kCFDateFormatterEraSymbols)) r = df_g_eras(f, 0);
    else if (cfkey(key, kCFDateFormatterLongEraSymbols)) r = df_g_long_eras(f, 0);
    else if (cfkey(key, kCFDateFormatterMonthSymbols)) r = df_g_months(f, 0);
    else if (cfkey(key, kCFDateFormatterShortMonthSymbols)) r = df_g_short_months(f, 0);
    else if (cfkey(key, kCFDateFormatterVeryShortMonthSymbols)) r = df_g_vs_months(f, 0);
    else if (cfkey(key, kCFDateFormatterStandaloneMonthSymbols)) r = df_g_sa_months(f, 0);
    else if (cfkey(key, kCFDateFormatterShortStandaloneMonthSymbols)) r = df_g_sa_short_months(f, 0);
    else if (cfkey(key, kCFDateFormatterVeryShortStandaloneMonthSymbols)) r = df_g_sa_vs_months(f, 0);
    else if (cfkey(key, kCFDateFormatterWeekdaySymbols)) r = df_g_weekdays(f, 0);
    else if (cfkey(key, kCFDateFormatterShortWeekdaySymbols)) r = df_g_short_weekdays(f, 0);
    else if (cfkey(key, kCFDateFormatterVeryShortWeekdaySymbols)) r = df_g_vs_weekdays(f, 0);
    else if (cfkey(key, kCFDateFormatterStandaloneWeekdaySymbols)) r = df_g_sa_weekdays(f, 0);
    else if (cfkey(key, kCFDateFormatterShortStandaloneWeekdaySymbols)) r = df_g_sa_short_weekdays(f, 0);
    else if (cfkey(key, kCFDateFormatterVeryShortStandaloneWeekdaySymbols)) r = df_g_sa_vs_weekdays(f, 0);
    else if (cfkey(key, kCFDateFormatterQuarterSymbols)) r = df_g_quarters(f, 0);
    else if (cfkey(key, kCFDateFormatterShortQuarterSymbols)) r = df_g_short_quarters(f, 0);
    else if (cfkey(key, kCFDateFormatterStandaloneQuarterSymbols)) r = df_g_sa_quarters(f, 0);
    else if (cfkey(key, kCFDateFormatterShortStandaloneQuarterSymbols)) r = df_g_sa_short_quarters(f, 0);
    return cfret(r);
}

u64 CFNumberFormatterGetTypeID(void) { return 32; }
id CFNumberFormatterCreate(void *alloc, id locale, long style) {
    (void)alloc;
    id f = MSG0(id, cx_alloc_instance(&NSNumberFormatter_c), "init");
    nf_set_locale(f, 0, locale);
    nf_set_style(f, 0, (u64)style);
    return f;
}
id CFNumberFormatterGetLocale(id f) { return nf_locale(f, 0); }
long CFNumberFormatterGetStyle(id f) { return (long)nf_style(f, 0); }
id CFNumberFormatterGetFormat(id f) { return nf_get_str(f, NS_POS_FORMAT); }
void CFNumberFormatterSetFormat(id f, id fmt) { nf_set_format(f, 0, fmt); }
id CFNumberFormatterCreateStringWithNumber(void *alloc, id f, id num) { (void)alloc; return cfret(nf_string(f, 0, num)); }
/* CFNumberType: 1..4 SInt8..64, 5 Float32, 6 Float64, 7 char, 8 short, 9 int, 10 long, 11 longlong, 12 float,
 * 13 double, 14 CFIndex, 15 NSInteger, 16 CGFloat */
static id num_from_type(long type, const void *p) {
    switch (type) {
    case 1: case 7: return cx_num_i(*(const signed char *)p);
    case 2: case 8: return cx_num_i(*(const short *)p);
    case 3: case 9: return cx_num_i(*(const int *)p);
    case 4: case 10: case 11: case 14: case 15: return cx_num_i(*(const i64 *)p);
    case 5: case 12: return cx_num_d(*(const float *)p);
    case 6: case 13: case 16: return cx_num_d(*(const double *)p);
    }
    return 0;
}
static void num_to_type(id n, long type, void *p) {
    switch (type) {
    case 1: case 7: *(signed char *)p = (signed char)MSG0(i64, n, "longLongValue"); break;
    case 2: case 8: *(short *)p = (short)MSG0(i64, n, "longLongValue"); break;
    case 3: case 9: *(int *)p = (int)MSG0(i64, n, "longLongValue"); break;
    case 4: case 10: case 11: case 14: case 15: *(i64 *)p = MSG0(i64, n, "longLongValue"); break;
    case 5: case 12: *(float *)p = (float)MSG0(double, n, "doubleValue"); break;
    case 6: case 13: case 16: *(double *)p = MSG0(double, n, "doubleValue"); break;
    }
}
id CFNumberFormatterCreateStringWithValue(void *alloc, id f, long type, const void *p) { (void)alloc; id n = num_from_type(type, p); return n ? cfret(nf_string(f, 0, n)) : 0; }
id CFNumberFormatterCreateNumberFromString(void *alloc, id f, id s, cf_range_t *rangep, unsigned long opts) {
    (void)alloc;
    nf_t *n = (nf_t *)f;
    ns_range_t r = {0, 0};
    int saved_parse_int = 0;
    if (opts & 1 /* kCFNumberFormatterParseIntegersOnly */) { saved_parse_int = 1; nf_lock(n); fdn_nf_set_attr(nf_get(n), 0, 1); nf_unlock(n); }
    id num = nf_number_range(n, s, rangep ? &r : 0);
    if (saved_parse_int) { nf_lock(n); fdn_nf_set_attr(nf_get(n), 0, 0); nf_unlock(n); }
    if (num && rangep) rangep->length = r.len;
    return cfret(num);
}
unsigned char CFNumberFormatterGetValueFromString(id f, id s, cf_range_t *rangep, long type, void *p) {
    id num = CFNumberFormatterCreateNumberFromString(0, f, s, rangep, (type >= 5 && type != 7 && type <= 6) || type == 12 || type == 13 || type == 16 ? 0 : 1);
    if (!num) return 0;
    num_to_type(num, type, p);
    objc_release(num);
    return 1;
}
unsigned char CFNumberFormatterGetDecimalInfoForCurrencyCode(id code, int32_t *digits, double *rounding) {
    double r;
    int d = fdn_currency_digits(cs(code), &r);
    if (d < 0) return 0;
    if (digits) *digits = d;
    if (rounding) *rounding = r;
    return 1;
}
static const struct { const void *const *key; int str, ival; } nf_keys[] = {
    {&kCFNumberFormatterCurrencyCode, NS_CUR_CODE, -1}, {&kCFNumberFormatterDecimalSeparator, NS_DECIMAL, -1},
    {&kCFNumberFormatterCurrencyDecimalSeparator, NS_CUR_DECIMAL, -1}, {&kCFNumberFormatterGroupingSeparator, NS_GROUPING, -1},
    {&kCFNumberFormatterPercentSymbol, NS_PERCENT, -1}, {&kCFNumberFormatterNaNSymbol, NS_NAN, -1},
    {&kCFNumberFormatterInfinitySymbol, NS_POS_INF, -1}, {&kCFNumberFormatterMinusSign, NS_MINUS, -1},
    {&kCFNumberFormatterPlusSign, NS_PLUS, -1}, {&kCFNumberFormatterCurrencySymbol, NS_CUR_SYMBOL, -1},
    {&kCFNumberFormatterExponentSymbol, NS_EXPONENT, -1}, {&kCFNumberFormatterPaddingCharacter, NS_PADDING, -1},
    {&kCFNumberFormatterPositivePrefix, NS_POS_PREFIX, -1}, {&kCFNumberFormatterPositiveSuffix, NS_POS_SUFFIX, -1},
    {&kCFNumberFormatterNegativePrefix, NS_NEG_PREFIX, -1}, {&kCFNumberFormatterNegativeSuffix, NS_NEG_SUFFIX, -1},
    {&kCFNumberFormatterPerMillSymbol, NS_PERMILL, -1}, {&kCFNumberFormatterInternationalCurrencySymbol, NS_INTL_CUR, -1},
    {&kCFNumberFormatterCurrencyGroupingSeparator, NS_CUR_GROUPING, -1},
    {&kCFNumberFormatterAlwaysShowDecimalSeparator, -1, NI_ALWAYS_DEC}, {&kCFNumberFormatterUseGroupingSeparator, -1, NI_USES_GROUP},
    {&kCFNumberFormatterMinIntegerDigits, -1, NI_MIN_INT}, {&kCFNumberFormatterMaxIntegerDigits, -1, NI_MAX_INT},
    {&kCFNumberFormatterMinFractionDigits, -1, NI_MIN_FRAC}, {&kCFNumberFormatterMaxFractionDigits, -1, NI_MAX_FRAC},
    {&kCFNumberFormatterGroupingSize, -1, NI_GROUP_SIZE}, {&kCFNumberFormatterSecondaryGroupingSize, -1, NI_SEC_GROUP},
    {&kCFNumberFormatterRoundingMode, -1, NI_ROUNDING}, {&kCFNumberFormatterFormatWidth, -1, NI_WIDTH},
    {&kCFNumberFormatterPaddingPosition, -1, NI_PAD_POS}, {&kCFNumberFormatterIsLenient, -1, NI_LENIENT},
    {&kCFNumberFormatterUseSignificantDigits, -1, NI_USES_SIG}, {&kCFNumberFormatterMinSignificantDigits, -1, NI_MIN_SIG},
    {&kCFNumberFormatterMaxSignificantDigits, -1, NI_MAX_SIG},
};
void CFNumberFormatterSetProperty(id f, id key, id v) {
    for (size_t i = 0; i < sizeof nf_keys / sizeof nf_keys[0]; i++) {
        if (!cfkey(key, *nf_keys[i].key)) continue;
        if (nf_keys[i].str >= 0) nf_set_str(f, nf_keys[i].str, v);
        else nf_set_int(f, nf_keys[i].ival, v ? MSG0(i64, v, "longLongValue") : 0);
        return;
    }
    if (cfkey(key, kCFNumberFormatterZeroSymbol)) nf_so_zero(f, 0, v);
    else if (cfkey(key, kCFNumberFormatterRoundingIncrement)) nf_set_rounding_inc(f, 0, v);
    else if (cfkey(key, kCFNumberFormatterMultiplier)) nf_set_multiplier(f, 0, v);
}
id CFNumberFormatterCopyProperty(id f, id key) {
    for (size_t i = 0; i < sizeof nf_keys / sizeof nf_keys[0]; i++) {
        if (!cfkey(key, *nf_keys[i].key)) continue;
        if (nf_keys[i].str >= 0) return cfret(nf_get_str(f, nf_keys[i].str));
        return cfret(cx_num_i(nf_get_int(f, nf_keys[i].ival)));
    }
    if (cfkey(key, kCFNumberFormatterZeroSymbol)) return cfret(nf_go_zero(f, 0));
    if (cfkey(key, kCFNumberFormatterRoundingIncrement)) return cfret(nf_rounding_inc(f, 0));
    if (cfkey(key, kCFNumberFormatterMultiplier)) return cfret(nf_multiplier(f, 0));
    if (cfkey(key, kCFNumberFormatterDefaultFormat)) return cfret(nf_get_str(f, NS_POS_FORMAT));
    return 0;
}

/* ======================================================================== registro */

#define CM(cl, s, f) cx_add_cmethod(&cl##_c, s, (void *)(f))
#define IM(cl, s, f) cx_add_method(&cl##_c, s, (void *)(f))
#define DEFC(cl, super, T) cx_define_class(&cl##_c, &cl##_m, #cl, super, (u32)(sizeof(T) - 8))

void cx_fdn_formatters_init(void) {
    cx_class_t *ob = cx_class_named("NSObject");
    if (!ob || cx_class_named("NSFormatter") == &NSFormatter_c) return;
    typedef struct { cx_class_t *isa; } base_t;

    DEFC(NSFormatter, ob, base_t);
    CM(NSFormatter, "supportsSecureCoding", yes_m);
    IM(NSFormatter, "stringForObjectValue:", f_string_obj); IM(NSFormatter, "editingStringForObjectValue:", f_editing);
    IM(NSFormatter, "attributedStringForObjectValue:withDefaultAttributes:", f_attr);
    IM(NSFormatter, "getObjectValue:forString:errorDescription:", f_get_obj);
    IM(NSFormatter, "isPartialStringValid:newEditingString:errorDescription:", f_partial);
    IM(NSFormatter, "isPartialStringValid:proposedSelectedRange:originalString:originalSelectedRange:errorDescription:", f_partial2);
    IM(NSFormatter, "copyWithZone:", self_copy); IM(NSFormatter, "copy", self_copy);
    IM(NSFormatter, "encodeWithCoder:", f_encode); IM(NSFormatter, "initWithCoder:", f_init_coder);

    DEFC(NSDateFormatter, &NSFormatter_c, df_t);
    CM(NSDateFormatter, "localizedStringFromDate:dateStyle:timeStyle:", df_s_localized);
    CM(NSDateFormatter, "dateFormatFromTemplate:options:locale:", df_s_from_template);
    CM(NSDateFormatter, "defaultFormatterBehavior", df_s_default_behavior);
    CM(NSDateFormatter, "setDefaultFormatterBehavior:", df_s_set_default_behavior);
    IM(NSDateFormatter, "init", df_init);
    IM(NSDateFormatter, "stringFromDate:", df_string); IM(NSDateFormatter, "dateFromString:", df_date);
    IM(NSDateFormatter, "stringForObjectValue:", df_string_obj);
    IM(NSDateFormatter, "getObjectValue:forString:errorDescription:", df_get_obj);
    IM(NSDateFormatter, "getObjectValue:forString:range:error:", df_get_obj_range);
    IM(NSDateFormatter, "dateFormat", df_format0); IM(NSDateFormatter, "setDateFormat:", df_set_format);
    IM(NSDateFormatter, "dateStyle", df_date_style); IM(NSDateFormatter, "setDateStyle:", df_set_date_style);
    IM(NSDateFormatter, "timeStyle", df_time_style); IM(NSDateFormatter, "setTimeStyle:", df_set_time_style);
    IM(NSDateFormatter, "locale", df_locale); IM(NSDateFormatter, "setLocale:", df_set_locale);
    IM(NSDateFormatter, "timeZone", df_tz); IM(NSDateFormatter, "setTimeZone:", df_set_tz);
    IM(NSDateFormatter, "calendar", df_cal); IM(NSDateFormatter, "setCalendar:", df_set_cal);
    IM(NSDateFormatter, "isLenient", df_lenient); IM(NSDateFormatter, "setLenient:", df_set_lenient);
    IM(NSDateFormatter, "doesRelativeDateFormatting", df_relative); IM(NSDateFormatter, "setDoesRelativeDateFormatting:", df_set_relative);
    IM(NSDateFormatter, "twoDigitStartDate", df_two_digit); IM(NSDateFormatter, "setTwoDigitStartDate:", df_set_two_digit);
    IM(NSDateFormatter, "defaultDate", df_default_date); IM(NSDateFormatter, "setDefaultDate:", df_set_default_date);
    IM(NSDateFormatter, "formatterBehavior", df_behavior); IM(NSDateFormatter, "setFormatterBehavior:", df_set_behavior);
    IM(NSDateFormatter, "formattingContext", df_context); IM(NSDateFormatter, "setFormattingContext:", df_set_context);
    IM(NSDateFormatter, "generatesCalendarDates", df_gen_cal); IM(NSDateFormatter, "setGeneratesCalendarDates:", df_set_gen_cal);
    IM(NSDateFormatter, "setLocalizedDateFormatFromTemplate:", df_set_template);
#define SYMR(get, set) IM(NSDateFormatter, #set + 3, df_g_##get); IM(NSDateFormatter, #set ":", df_s_##get);
    IM(NSDateFormatter, "eraSymbols", df_g_eras); IM(NSDateFormatter, "setEraSymbols:", df_s_eras);
    IM(NSDateFormatter, "monthSymbols", df_g_months); IM(NSDateFormatter, "setMonthSymbols:", df_s_months);
    IM(NSDateFormatter, "shortMonthSymbols", df_g_short_months); IM(NSDateFormatter, "setShortMonthSymbols:", df_s_short_months);
    IM(NSDateFormatter, "weekdaySymbols", df_g_weekdays); IM(NSDateFormatter, "setWeekdaySymbols:", df_s_weekdays);
    IM(NSDateFormatter, "shortWeekdaySymbols", df_g_short_weekdays); IM(NSDateFormatter, "setShortWeekdaySymbols:", df_s_short_weekdays);
    IM(NSDateFormatter, "longEraSymbols", df_g_long_eras); IM(NSDateFormatter, "setLongEraSymbols:", df_s_long_eras);
    IM(NSDateFormatter, "veryShortMonthSymbols", df_g_vs_months); IM(NSDateFormatter, "setVeryShortMonthSymbols:", df_s_vs_months);
    IM(NSDateFormatter, "veryShortWeekdaySymbols", df_g_vs_weekdays); IM(NSDateFormatter, "setVeryShortWeekdaySymbols:", df_s_vs_weekdays);
    IM(NSDateFormatter, "standaloneMonthSymbols", df_g_sa_months); IM(NSDateFormatter, "setStandaloneMonthSymbols:", df_s_sa_months);
    IM(NSDateFormatter, "shortStandaloneMonthSymbols", df_g_sa_short_months); IM(NSDateFormatter, "setShortStandaloneMonthSymbols:", df_s_sa_short_months);
    IM(NSDateFormatter, "veryShortStandaloneMonthSymbols", df_g_sa_vs_months); IM(NSDateFormatter, "setVeryShortStandaloneMonthSymbols:", df_s_sa_vs_months);
    IM(NSDateFormatter, "standaloneWeekdaySymbols", df_g_sa_weekdays); IM(NSDateFormatter, "setStandaloneWeekdaySymbols:", df_s_sa_weekdays);
    IM(NSDateFormatter, "shortStandaloneWeekdaySymbols", df_g_sa_short_weekdays); IM(NSDateFormatter, "setShortStandaloneWeekdaySymbols:", df_s_sa_short_weekdays);
    IM(NSDateFormatter, "veryShortStandaloneWeekdaySymbols", df_g_sa_vs_weekdays); IM(NSDateFormatter, "setVeryShortStandaloneWeekdaySymbols:", df_s_sa_vs_weekdays);
    IM(NSDateFormatter, "quarterSymbols", df_g_quarters); IM(NSDateFormatter, "setQuarterSymbols:", df_s_quarters);
    IM(NSDateFormatter, "shortQuarterSymbols", df_g_short_quarters); IM(NSDateFormatter, "setShortQuarterSymbols:", df_s_short_quarters);
    IM(NSDateFormatter, "standaloneQuarterSymbols", df_g_sa_quarters); IM(NSDateFormatter, "setStandaloneQuarterSymbols:", df_s_sa_quarters);
    IM(NSDateFormatter, "shortStandaloneQuarterSymbols", df_g_sa_short_quarters); IM(NSDateFormatter, "setShortStandaloneQuarterSymbols:", df_s_sa_short_quarters);
#undef SYMR
    IM(NSDateFormatter, "AMSymbol", df_am); IM(NSDateFormatter, "setAMSymbol:", df_set_am);
    IM(NSDateFormatter, "PMSymbol", df_pm); IM(NSDateFormatter, "setPMSymbol:", df_set_pm);
    IM(NSDateFormatter, "copyWithZone:", df_copy); IM(NSDateFormatter, "copy", df_copy); IM(NSDateFormatter, "dealloc", df_dealloc);

    DEFC(NSISO8601DateFormatter, &NSFormatter_c, iso_t);
    CM(NSISO8601DateFormatter, "stringFromDate:timeZone:formatOptions:", iso_s_string);
    IM(NSISO8601DateFormatter, "init", iso_init);
    IM(NSISO8601DateFormatter, "stringFromDate:", iso_string); IM(NSISO8601DateFormatter, "dateFromString:", iso_date);
    IM(NSISO8601DateFormatter, "stringForObjectValue:", iso_string_obj);
    IM(NSISO8601DateFormatter, "getObjectValue:forString:errorDescription:", iso_get_obj);
    IM(NSISO8601DateFormatter, "timeZone", iso_tz); IM(NSISO8601DateFormatter, "setTimeZone:", iso_set_tz);
    IM(NSISO8601DateFormatter, "formatOptions", iso_options); IM(NSISO8601DateFormatter, "setFormatOptions:", iso_set_options);
    IM(NSISO8601DateFormatter, "dealloc", iso_dealloc);

    DEFC(NSNumberFormatter, &NSFormatter_c, nf_t);
    CM(NSNumberFormatter, "localizedStringFromNumber:numberStyle:", nf_s_localized);
    CM(NSNumberFormatter, "defaultFormatterBehavior", nf_s_default_behavior);
    CM(NSNumberFormatter, "setDefaultFormatterBehavior:", nf_s_set_default_behavior);
    IM(NSNumberFormatter, "init", nf_init);
    IM(NSNumberFormatter, "stringFromNumber:", nf_string); IM(NSNumberFormatter, "numberFromString:", nf_number);
    IM(NSNumberFormatter, "stringForObjectValue:", nf_string_obj);
    IM(NSNumberFormatter, "getObjectValue:forString:errorDescription:", nf_get_obj);
    IM(NSNumberFormatter, "getObjectValue:forString:range:error:", nf_get_obj_range);
    IM(NSNumberFormatter, "numberStyle", nf_style); IM(NSNumberFormatter, "setNumberStyle:", nf_set_style);
    IM(NSNumberFormatter, "locale", nf_locale); IM(NSNumberFormatter, "setLocale:", nf_set_locale);
    IM(NSNumberFormatter, "format", nf_format); IM(NSNumberFormatter, "setFormat:", nf_set_format);
#define NSR(get, set) IM(NSNumberFormatter, set ":", nf_s_##get);
    IM(NSNumberFormatter, "decimalSeparator", nf_g_decimal); NSR(decimal, "setDecimalSeparator")
    IM(NSNumberFormatter, "groupingSeparator", nf_g_grouping); NSR(grouping, "setGroupingSeparator")
    IM(NSNumberFormatter, "percentSymbol", nf_g_percent); NSR(percent, "setPercentSymbol")
    IM(NSNumberFormatter, "minusSign", nf_g_minus); NSR(minus, "setMinusSign")
    IM(NSNumberFormatter, "plusSign", nf_g_plus); NSR(plus, "setPlusSign")
    IM(NSNumberFormatter, "currencySymbol", nf_g_cur_symbol); NSR(cur_symbol, "setCurrencySymbol")
    IM(NSNumberFormatter, "internationalCurrencySymbol", nf_g_intl_cur); NSR(intl_cur, "setInternationalCurrencySymbol")
    IM(NSNumberFormatter, "currencyDecimalSeparator", nf_g_cur_decimal); NSR(cur_decimal, "setCurrencyDecimalSeparator")
    IM(NSNumberFormatter, "exponentSymbol", nf_g_exponent); NSR(exponent, "setExponentSymbol")
    IM(NSNumberFormatter, "perMillSymbol", nf_g_permill); NSR(permill, "setPerMillSymbol")
    IM(NSNumberFormatter, "positiveInfinitySymbol", nf_g_pos_inf); NSR(pos_inf, "setPositiveInfinitySymbol")
    IM(NSNumberFormatter, "negativeInfinitySymbol", nf_g_neg_inf); NSR(neg_inf, "setNegativeInfinitySymbol")
    IM(NSNumberFormatter, "notANumberSymbol", nf_g_nan); NSR(nan, "setNotANumberSymbol")
    IM(NSNumberFormatter, "currencyGroupingSeparator", nf_g_cur_grouping); NSR(cur_grouping, "setCurrencyGroupingSeparator")
    IM(NSNumberFormatter, "positivePrefix", nf_g_pos_prefix); NSR(pos_prefix, "setPositivePrefix")
    IM(NSNumberFormatter, "positiveSuffix", nf_g_pos_suffix); NSR(pos_suffix, "setPositiveSuffix")
    IM(NSNumberFormatter, "negativePrefix", nf_g_neg_prefix); NSR(neg_prefix, "setNegativePrefix")
    IM(NSNumberFormatter, "negativeSuffix", nf_g_neg_suffix); NSR(neg_suffix, "setNegativeSuffix")
    IM(NSNumberFormatter, "paddingCharacter", nf_g_padding); NSR(padding, "setPaddingCharacter")
    IM(NSNumberFormatter, "currencyCode", nf_g_cur_code); NSR(cur_code, "setCurrencyCode")
    IM(NSNumberFormatter, "positiveFormat", nf_g_pos_format); NSR(pos_format, "setPositiveFormat")
    IM(NSNumberFormatter, "negativeFormat", nf_g_neg_format); NSR(neg_format, "setNegativeFormat")
#undef NSR
#define NIR(get, g, s) IM(NSNumberFormatter, g, nf_gi_##get); IM(NSNumberFormatter, s ":", nf_si_##get);
    NIR(min_int, "minimumIntegerDigits", "setMinimumIntegerDigits") NIR(max_int, "maximumIntegerDigits", "setMaximumIntegerDigits")
    NIR(min_frac, "minimumFractionDigits", "setMinimumFractionDigits") NIR(max_frac, "maximumFractionDigits", "setMaximumFractionDigits")
    NIR(group_size, "groupingSize", "setGroupingSize") NIR(sec_group, "secondaryGroupingSize", "setSecondaryGroupingSize")
    NIR(width, "formatWidth", "setFormatWidth") NIR(pad_pos, "paddingPosition", "setPaddingPosition")
    NIR(rounding, "roundingMode", "setRoundingMode") NIR(min_sig, "minimumSignificantDigits", "setMinimumSignificantDigits")
    NIR(max_sig, "maximumSignificantDigits", "setMaximumSignificantDigits")
#undef NIR
    IM(NSNumberFormatter, "usesGroupingSeparator", nf_g_uses_group_b); IM(NSNumberFormatter, "setUsesGroupingSeparator:", nf_s_uses_group_b);
    IM(NSNumberFormatter, "alwaysShowsDecimalSeparator", nf_g_always_dec_b); IM(NSNumberFormatter, "setAlwaysShowsDecimalSeparator:", nf_s_always_dec_b);
    IM(NSNumberFormatter, "usesSignificantDigits", nf_g_uses_sig_b); IM(NSNumberFormatter, "setUsesSignificantDigits:", nf_s_uses_sig_b);
    IM(NSNumberFormatter, "isLenient", nf_g_lenient_b); IM(NSNumberFormatter, "setLenient:", nf_s_lenient_b);
    IM(NSNumberFormatter, "roundingIncrement", nf_rounding_inc); IM(NSNumberFormatter, "setRoundingIncrement:", nf_set_rounding_inc);
    IM(NSNumberFormatter, "multiplier", nf_multiplier); IM(NSNumberFormatter, "setMultiplier:", nf_set_multiplier);
    IM(NSNumberFormatter, "zeroSymbol", nf_go_zero); IM(NSNumberFormatter, "setZeroSymbol:", nf_so_zero);
    IM(NSNumberFormatter, "nilSymbol", nf_go_nil_sym); IM(NSNumberFormatter, "setNilSymbol:", nf_so_nil_sym);
    IM(NSNumberFormatter, "minimum", nf_go_minimum); IM(NSNumberFormatter, "setMinimum:", nf_so_minimum);
    IM(NSNumberFormatter, "maximum", nf_go_maximum); IM(NSNumberFormatter, "setMaximum:", nf_so_maximum);
    IM(NSNumberFormatter, "generatesDecimalNumbers", nf_gen_dec); IM(NSNumberFormatter, "setGeneratesDecimalNumbers:", nf_set_gen_dec);
    IM(NSNumberFormatter, "isPartialStringValidationEnabled", nf_partial_b);
    IM(NSNumberFormatter, "setPartialStringValidationEnabled:", nf_set_partial_b);
    IM(NSNumberFormatter, "formatterBehavior", nf_behavior); IM(NSNumberFormatter, "setFormatterBehavior:", nf_set_behavior);
    IM(NSNumberFormatter, "copyWithZone:", nf_copy); IM(NSNumberFormatter, "copy", nf_copy); IM(NSNumberFormatter, "dealloc", nf_dealloc);

    DEFC(NSByteCountFormatter, &NSFormatter_c, bc_t);
    CM(NSByteCountFormatter, "stringFromByteCount:countStyle:", bc_s_string);
    IM(NSByteCountFormatter, "init", bc_init);
    IM(NSByteCountFormatter, "stringFromByteCount:", bc_string); IM(NSByteCountFormatter, "stringForObjectValue:", bc_string_obj);
#define BCR(get, g, s) IM(NSByteCountFormatter, g, bc_g_##get); IM(NSByteCountFormatter, s ":", bc_s_##get);
    BCR(allowed, "allowedUnits", "setAllowedUnits") BCR(style, "countStyle", "setCountStyle")
    BCR(nonnumeric, "allowsNonnumericFormatting", "setAllowsNonnumericFormatting") BCR(unit, "includesUnit", "setIncludesUnit")
    BCR(count, "includesCount", "setIncludesCount") BCR(actual, "includesActualByteCount", "setIncludesActualByteCount")
    BCR(adaptive, "isAdaptive", "setAdaptive") BCR(zero_pad, "zeroPadsFractionDigits", "setZeroPadsFractionDigits")
    BCR(context, "formattingContext", "setFormattingContext")
#undef BCR

    DEFC(NSDateComponentsFormatter, &NSFormatter_c, dcf_t);
    CM(NSDateComponentsFormatter, "localizedStringFromDateComponents:unitsStyle:", dcf_s_localized);
    IM(NSDateComponentsFormatter, "init", dcf_init);
    IM(NSDateComponentsFormatter, "stringFromTimeInterval:", dcf_string_ti);
    IM(NSDateComponentsFormatter, "stringFromDate:toDate:", dcf_string_dates);
    IM(NSDateComponentsFormatter, "stringFromDateComponents:", dcf_string_comps);
    IM(NSDateComponentsFormatter, "stringForObjectValue:", dcf_string_obj);
    IM(NSDateComponentsFormatter, "getObjectValue:forString:errorDescription:", dcf_get_obj);
#define DCR(get, g, s) IM(NSDateComponentsFormatter, g, dcf_g_##get); IM(NSDateComponentsFormatter, s ":", dcf_s_##get);
    DCR(allowed, "allowedUnits", "setAllowedUnits") DCR(style, "unitsStyle", "setUnitsStyle")
    DCR(max_units, "maximumUnitCount", "setMaximumUnitCount") DCR(zero, "zeroFormattingBehavior", "setZeroFormattingBehavior")
    DCR(context, "formattingContext", "setFormattingContext") DCR(fractional, "allowsFractionalUnits", "setAllowsFractionalUnits")
    DCR(collapses, "collapsesLargestUnit", "setCollapsesLargestUnit") DCR(approx, "includesApproximationPhrase", "setIncludesApproximationPhrase")
    DCR(remaining, "includesTimeRemainingPhrase", "setIncludesTimeRemainingPhrase")
#undef DCR
    IM(NSDateComponentsFormatter, "calendar", dcf_cal); IM(NSDateComponentsFormatter, "setCalendar:", dcf_set_cal);
    IM(NSDateComponentsFormatter, "referenceDate", dcf_ref); IM(NSDateComponentsFormatter, "setReferenceDate:", dcf_set_ref);
    IM(NSDateComponentsFormatter, "dealloc", dcf_dealloc);

    DEFC(NSRelativeDateTimeFormatter, &NSFormatter_c, rel_t);
    IM(NSRelativeDateTimeFormatter, "init", rel_init);
    IM(NSRelativeDateTimeFormatter, "localizedStringForDate:relativeToDate:", rel_for_date);
    IM(NSRelativeDateTimeFormatter, "localizedStringFromTimeInterval:", rel_for_ti);
    IM(NSRelativeDateTimeFormatter, "localizedStringFromDateComponents:", rel_for_comps);
    IM(NSRelativeDateTimeFormatter, "stringForObjectValue:", rel_string_obj);
    IM(NSRelativeDateTimeFormatter, "dateTimeStyle", rel_g_dt); IM(NSRelativeDateTimeFormatter, "setDateTimeStyle:", rel_s_dt);
    IM(NSRelativeDateTimeFormatter, "unitsStyle", rel_g_units); IM(NSRelativeDateTimeFormatter, "setUnitsStyle:", rel_s_units);
    IM(NSRelativeDateTimeFormatter, "formattingContext", rel_g_ctx); IM(NSRelativeDateTimeFormatter, "setFormattingContext:", rel_s_ctx);
    IM(NSRelativeDateTimeFormatter, "calendar", rel_cal); IM(NSRelativeDateTimeFormatter, "setCalendar:", rel_set_cal);
    IM(NSRelativeDateTimeFormatter, "locale", rel_locale); IM(NSRelativeDateTimeFormatter, "setLocale:", rel_set_locale);
    IM(NSRelativeDateTimeFormatter, "dealloc", rel_dealloc);

    DEFC(NSListFormatter, &NSFormatter_c, lf_t);
    CM(NSListFormatter, "localizedStringByJoiningStrings:", lf_s_join);
    IM(NSListFormatter, "stringFromItems:", lf_from_items); IM(NSListFormatter, "stringForObjectValue:", lf_string_obj);
    IM(NSListFormatter, "locale", lf_locale); IM(NSListFormatter, "setLocale:", lf_set_locale);
    IM(NSListFormatter, "itemFormatter", lf_item); IM(NSListFormatter, "setItemFormatter:", lf_set_item);
    IM(NSListFormatter, "dealloc", lf_dealloc);

    DEFC(NSDateInterval, ob, dint_t);
    CM(NSDateInterval, "supportsSecureCoding", yes_m);
    IM(NSDateInterval, "init", di_init); IM(NSDateInterval, "initWithStartDate:duration:", di_init_dur);
    IM(NSDateInterval, "initWithStartDate:endDate:", di_init_end);
    IM(NSDateInterval, "startDate", di_start); IM(NSDateInterval, "setStartDate:", di_set_start);
    IM(NSDateInterval, "endDate", di_end); IM(NSDateInterval, "setEndDate:", di_set_end);
    IM(NSDateInterval, "duration", di_duration); IM(NSDateInterval, "setDuration:", di_set_duration);
    IM(NSDateInterval, "compare:", di_compare); IM(NSDateInterval, "isEqualToDateInterval:", di_equal_di);
    IM(NSDateInterval, "isEqual:", di_equal_di); IM(NSDateInterval, "hash", di_hash);
    IM(NSDateInterval, "intersectsDateInterval:", di_intersects); IM(NSDateInterval, "intersectionWithDateInterval:", di_intersection);
    IM(NSDateInterval, "containsDate:", di_contains); IM(NSDateInterval, "copy", di_copy); IM(NSDateInterval, "copyWithZone:", di_copy);
    IM(NSDateInterval, "description", di_desc);
    IM(NSDateInterval, "encodeWithCoder:", di_encode); IM(NSDateInterval, "initWithCoder:", di_init_coder);

    DEFC(NSDateIntervalFormatter, &NSFormatter_c, dif_t);
    IM(NSDateIntervalFormatter, "init", dif_init);
    IM(NSDateIntervalFormatter, "stringFromDate:toDate:", dif_from_dates);
    IM(NSDateIntervalFormatter, "stringFromDateInterval:", dif_from_interval);
    IM(NSDateIntervalFormatter, "dateStyle", dif_g_ds); IM(NSDateIntervalFormatter, "setDateStyle:", dif_s_ds);
    IM(NSDateIntervalFormatter, "timeStyle", dif_g_ts); IM(NSDateIntervalFormatter, "setTimeStyle:", dif_s_ts);
    IM(NSDateIntervalFormatter, "dateTemplate", dif_go_tmpl); IM(NSDateIntervalFormatter, "setDateTemplate:", dif_so_tmpl);
    IM(NSDateIntervalFormatter, "locale", dif_go_locale); IM(NSDateIntervalFormatter, "setLocale:", dif_so_locale);
    IM(NSDateIntervalFormatter, "calendar", dif_go_cal); IM(NSDateIntervalFormatter, "setCalendar:", dif_so_cal);
    IM(NSDateIntervalFormatter, "timeZone", dif_go_tz); IM(NSDateIntervalFormatter, "setTimeZone:", dif_so_tz);
    IM(NSDateIntervalFormatter, "dealloc", dif_dealloc);

    DEFC(NSPersonNameComponents, ob, pn_t);
    CM(NSPersonNameComponents, "supportsSecureCoding", yes_m);
    IM(NSPersonNameComponents, "namePrefix", pn_g_prefix); IM(NSPersonNameComponents, "setNamePrefix:", pn_s_prefix);
    IM(NSPersonNameComponents, "givenName", pn_g_given); IM(NSPersonNameComponents, "setGivenName:", pn_s_given);
    IM(NSPersonNameComponents, "middleName", pn_g_middle); IM(NSPersonNameComponents, "setMiddleName:", pn_s_middle);
    IM(NSPersonNameComponents, "familyName", pn_g_family); IM(NSPersonNameComponents, "setFamilyName:", pn_s_family);
    IM(NSPersonNameComponents, "nameSuffix", pn_g_suffix); IM(NSPersonNameComponents, "setNameSuffix:", pn_s_suffix);
    IM(NSPersonNameComponents, "nickname", pn_g_nick); IM(NSPersonNameComponents, "setNickname:", pn_s_nick);
    IM(NSPersonNameComponents, "phoneticRepresentation", pn_phonetic);
    IM(NSPersonNameComponents, "setPhoneticRepresentation:", pn_set_phonetic);
    IM(NSPersonNameComponents, "isEqual:", pn_equal); IM(NSPersonNameComponents, "hash", pn_hash);
    IM(NSPersonNameComponents, "copy", pn_copy); IM(NSPersonNameComponents, "copyWithZone:", pn_copy);
    IM(NSPersonNameComponents, "description", pn_desc);
    IM(NSPersonNameComponents, "encodeWithCoder:", pn_encode); IM(NSPersonNameComponents, "initWithCoder:", pn_init_coder);
    IM(NSPersonNameComponents, "dealloc", pn_dealloc);

    DEFC(NSPersonNameComponentsFormatter, &NSFormatter_c, pnf_t);
    CM(NSPersonNameComponentsFormatter, "localizedStringFromPersonNameComponents:style:options:", pnf_s_localized);
    IM(NSPersonNameComponentsFormatter, "stringFromPersonNameComponents:", pnf_string);
    IM(NSPersonNameComponentsFormatter, "stringForObjectValue:", pnf_string_obj);
    IM(NSPersonNameComponentsFormatter, "annotatedStringFromPersonNameComponents:", pnf_annotated);
    IM(NSPersonNameComponentsFormatter, "personNameComponentsFromString:", pnf_from_string);
    IM(NSPersonNameComponentsFormatter, "getObjectValue:forString:errorDescription:", pnf_get_obj);
    IM(NSPersonNameComponentsFormatter, "style", pnf_style); IM(NSPersonNameComponentsFormatter, "setStyle:", pnf_set_style);
    IM(NSPersonNameComponentsFormatter, "isPhonetic", pnf_phonetic); IM(NSPersonNameComponentsFormatter, "setPhonetic:", pnf_set_phonetic);
    IM(NSPersonNameComponentsFormatter, "locale", pnf_locale); IM(NSPersonNameComponentsFormatter, "setLocale:", pnf_set_locale);
    (void)nil_m;
}
