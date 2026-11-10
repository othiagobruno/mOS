/* Comportamento dos 187 símbolos que fi_exports só resolvia no dyld.
 * Incluído por fi_swift.c depois de fi_abi.inc (mesmo TU: fi_pack, fi_parts, fi_dec_*).
 *
 * Valor Swift dos nossos tipos resilientes (FormatStyle, PersonName, AttributedString):
 * ponteiro de 8 bytes para um objeto com refcount. A metadata (Ma) publica esse tamanho
 * e a value witness table copia com retain / destrói com release. DateStyle e os
 * símbolos de campo continuam inteiros em rax, como o formatted(date:time:) já lia.
 * NSDecimal continua o struct C de 20 bytes (sret em rax, self indireto em r13).
 *
 * String de volta: fi_swstr em rax:rdx. Optional.none de String é 0,0 (habitante
 * extra; a string vazia é 0xE0 no último byte).
 */
#ifndef FI_BEHAVIOR_INC
#define FI_BEHAVIOR_INC

#define FI_MAGIC_STYLE 0xF15E0001u
#define FI_MAGIC_PERSON 0xF1504E01u
#define FI_MAGIC_ATTR 0xF1A77A01u
#define FI_MAGIC_OPT 0xF10D7001u
#define FI_MAGIC_RUN 0xF1A77A02u

enum {
    FI_KIND_DATE = 1,
    FI_KIND_ISO = 2,
    FI_KIND_INTERVAL = 3,
    FI_KIND_INT = 4,
    FI_KIND_DEC = 5,
    FI_KIND_PERCENT = 6,
    FI_KIND_LIST = 7,
    FI_KIND_CURRENCY = 8
};

enum { FI_F_YEAR = 0, FI_F_MONTH = 1, FI_F_DAY = 2, FI_F_HOUR = 3, FI_F_MINUTE = 4, FI_F_WEEKDAY = 5 };

typedef struct {
    void *init_buf;
    void *destroy;
    void *init_copy;
    void *assign_copy;
    void *init_take;
    void *assign_take;
    void *get_tag;
    void *store_tag;
    size_t size;
    size_t stride;
    uint32_t flags;
    uint32_t extra;
} fi_vwt;

typedef struct {
    const fi_vwt *vw;
    const void *desc;
} fi_md;

typedef struct fi_obj {
    int ref;
    uint32_t magic;
    uint8_t kind;
    uint8_t date_style;   /* 0xff = não definido */
    uint8_t time_style;
    uint8_t month_kind;   /* 0 omite, 1 dígitos, 2 abreviado, 3 longo */
    uint8_t day_on;
    uint8_t year_on;
    uint8_t hour_kind;    /* 0 omite, 1 24h, 2 AM/PM */
    uint8_t minute_on;
    uint8_t second_on;
    uint8_t weekday_kind; /* 0 omite, 1 abreviado, 2 longo */
    uint8_t symbols;
    uint8_t grouping;
    uint8_t sign_mode;    /* 0 automático, 1 sempre, 2 sempre inclusive o zero */
    uint8_t frac;         /* 255 = o padrão do tipo */
    uint8_t list_type;    /* 0 and, 1 or */
    uint8_t list_width;   /* 0 standard, 1 short, 2 narrow */
    uint8_t iso_custom;
    uint8_t iso_fields;
    uint8_t iso_date_sep; /* 0 traço, 1 omite */
    uint8_t iso_time_sep; /* 0 dois-pontos, 1 omite */
    uint8_t iso_dt_sep;   /* 0 T, 1 espaço */
    uint8_t notation;     /* 0 normal, 1 compactName */
    char locale[8];
    char currency[4];
} fi_obj;

typedef struct fi_person {
    int ref;
    uint32_t magic;
    char *prefix, *given, *middle, *family, *suffix;
} fi_person;

typedef struct fi_attr {
    int ref;
    uint32_t magic;
    char *text;
    int len;
    int nruns;
    mf_span runs[12];
    char hrefs[384];
} fi_attr;

typedef struct fi_run {
    int ref;
    uint32_t magic;
    fi_attr *attr;
    int index;
} fi_run;

typedef struct fi_opt {
    int ref;
    uint32_t magic;
    int allow_ext;
    int syntax;
    int failure;
    char lang[8];
} fi_opt;

static int fi_magic_ok(const void *p) {
    uint32_t m;
    if (!p) return 0;
    memcpy(&m, (const char *)p + sizeof(int), sizeof m);
    return m == FI_MAGIC_STYLE || m == FI_MAGIC_PERSON || m == FI_MAGIC_ATTR || m == FI_MAGIC_OPT || m == FI_MAGIC_RUN;
}

static void fi_obj_free(void *p) {
    fi_attr *a;
    fi_person *n;
    fi_run *r;
    if (!p || !fi_magic_ok(p)) return;
    a = p;
    if (a->magic == FI_MAGIC_ATTR) free(a->text);
    n = p;
    if (n->magic == FI_MAGIC_PERSON) {
        free(n->prefix); free(n->given); free(n->middle); free(n->family); free(n->suffix);
    }
    r = p;
    if (r->magic == FI_MAGIC_RUN && r->attr) {
        if (--r->attr->ref == 0) fi_obj_free(r->attr);
    }
    free(p);
}

void fi_retain(void *p) {
    int *ref;
    if (!fi_magic_ok(p)) return;
    ref = p;
    (*ref)++;
}

void fi_release(void *p) {
    int *ref;
    if (!fi_magic_ok(p)) return;
    ref = p;
    if (--(*ref) > 0) return;
    fi_obj_free(p);
}

static void *fi_alloc(size_t n, uint32_t magic) {
    char *p = calloc(1, n);
    int *ref;
    if (!p) return 0;
    ref = (int *)p;
    *ref = 1;
    memcpy(p + sizeof(int), &magic, sizeof magic);
    return p;
}

/* ---- value witnesses (C ABI: dest, src, metadata). Tamanho ≤ 16, inline. ---- */
void *fi_vw_copy(void *dest, const void *src, const fi_md *md) {
    size_t n = md && md->vw ? md->vw->size : 0;
    if (!dest || !src || !n) return dest;
    if (md->vw->flags & 0x10000u) fi_retain(*(void *const *)src);
    memcpy(dest, src, n);
    return dest;
}
void *fi_vw_assign(void *dest, const void *src, const fi_md *md) {
    if (md && md->vw && (md->vw->flags & 0x10000u)) fi_release(*(void **)dest);
    return fi_vw_copy(dest, src, md);
}
void *fi_vw_take(void *dest, void *src, const fi_md *md) {
    size_t n = md && md->vw ? md->vw->size : 0;
    if (dest && src && n) memcpy(dest, src, n);
    if (src && n && md && (md->vw->flags & 0x10000u)) memset(src, 0, n);
    return dest;
}
void *fi_vw_assign_take(void *dest, void *src, const fi_md *md) {
    if (md && md->vw && (md->vw->flags & 0x10000u)) fi_release(*(void **)dest);
    return fi_vw_take(dest, src, md);
}
void fi_vw_destroy(void *obj, const fi_md *md) {
    if (md && md->vw && (md->vw->flags & 0x10000u) && obj) {
        fi_release(*(void **)obj);
        *(void **)obj = 0;
    }
}
unsigned fi_vw_get_tag(const void *obj, unsigned empty, const fi_md *md) {
    (void)obj; (void)empty; (void)md;
    return 0;
}
void fi_vw_store_tag(void *obj, unsigned tag, unsigned empty, const fi_md *md) {
    (void)obj; (void)tag; (void)empty; (void)md;
}

static fi_vwt fi_vwt_make(size_t size, int cow) {
    fi_vwt w;
    memset(&w, 0, sizeof w);
    w.init_buf = (void *)fi_vw_copy;
    w.destroy = (void *)fi_vw_destroy;
    w.init_copy = (void *)fi_vw_copy;
    w.assign_copy = (void *)fi_vw_assign;
    w.init_take = (void *)fi_vw_take;
    w.assign_take = (void *)fi_vw_assign_take;
    w.get_tag = (void *)fi_vw_get_tag;
    w.store_tag = (void *)fi_vw_store_tag;
    w.size = size;
    w.stride = (size + 7u) & ~(size_t)7u;
    w.flags = 7u | (cow ? 0x10000u : 0u);
    return w;
}

fi_vwt fi_vwt_pod8 = {
    (void *)fi_vw_copy, (void *)fi_vw_destroy, (void *)fi_vw_copy, (void *)fi_vw_assign,
    (void *)fi_vw_take, (void *)fi_vw_assign_take, (void *)fi_vw_get_tag, (void *)fi_vw_store_tag,
    8, 8, 7u, 0
};
fi_vwt fi_vwt_pod16 = {
    (void *)fi_vw_copy, (void *)fi_vw_destroy, (void *)fi_vw_copy, (void *)fi_vw_assign,
    (void *)fi_vw_take, (void *)fi_vw_assign_take, (void *)fi_vw_get_tag, (void *)fi_vw_store_tag,
    16, 16, 7u, 0
};
fi_vwt fi_vwt_cow = {
    (void *)fi_vw_copy, (void *)fi_vw_destroy, (void *)fi_vw_copy, (void *)fi_vw_assign,
    (void *)fi_vw_take, (void *)fi_vw_assign_take, (void *)fi_vw_get_tag, (void *)fi_vw_store_tag,
    8, 8, 7u | 0x10000u, 0
};

static void fi_vwt_init(void) {
    (void)fi_vwt_make;
}

size_t fi_md_size(const void *md) {
    const fi_md *m = md;
    fi_vwt_init();
    return m && m->vw ? m->vw->size : 0;
}
int fi_md_cow(const void *md) {
    const fi_md *m = md;
    return m && m->vw && (m->vw->flags & 0x10000u) ? 1 : 0;
}
int fi_desc_kind(const void *desc) {
    uint32_t k;
    if (!desc) return -1;
    memcpy(&k, desc, sizeof k);
    return (int)(k & 31u);
}

/* ---- locale: C string "pt_BR" entra; objeto Swift opaco cai em en ---- */
static void fi_read_locale(const void *p, char *out, size_t cap) {
    const unsigned char *s;
    int n = 0;
    snprintf(out, cap, "en");
    if (!p) return;
    s = p;
    if (s[0] < 'A' || (s[0] > 'Z' && s[0] < 'a') || s[0] > 'z') return;
    for (; n < 16 && s[n]; n++) {
        unsigned char c = s[n];
        int ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
        if (!ok) return;
    }
    if (n < 2 || n > 7) return;
    snprintf(out, cap, "%s", (const char *)s);
}

static fi_obj *fi_obj_new(uint8_t kind) {
    fi_obj *o = fi_alloc(sizeof *o, FI_MAGIC_STYLE);
    if (!o) return 0;
    o->kind = kind;
    o->date_style = 0xff;
    o->time_style = 0xff;
    o->frac = 255;
    o->grouping = 1;
    memcpy(o->locale, "en", 3);
    return o;
}
static fi_obj *fi_obj_clone(const fi_obj *s) {
    fi_obj *o;
    if (!s || s->magic != FI_MAGIC_STYLE) return fi_obj_new(FI_KIND_DATE);
    o = fi_alloc(sizeof *o, FI_MAGIC_STYLE);
    if (!o) return 0;
    {
        int ref = o->ref;
        memcpy(o, s, sizeof *o);
        o->ref = ref;
        o->magic = FI_MAGIC_STYLE;
    }
    return o;
}

fi_obj *fi_style_date_default(void) {
    fi_obj *o = fi_obj_new(FI_KIND_DATE);
    return o;
}
fi_obj *fi_style_iso_default(void) {
    fi_obj *o = fi_obj_new(FI_KIND_ISO);
    return o;
}
fi_obj *fi_style_dec_default(void) {
    fi_obj *o = fi_obj_new(FI_KIND_DEC);
    return o;
}
fi_obj *fi_style_int_locale(const void *locale) {
    fi_obj *o = fi_obj_new(FI_KIND_INT);
    if (o) fi_read_locale(locale, o->locale, sizeof o->locale);
    return o;
}
fi_obj *fi_style_percent_locale(const void *locale) {
    fi_obj *o = fi_obj_new(FI_KIND_PERCENT);
    if (!o) return 0;
    fi_read_locale(locale, o->locale, sizeof o->locale);
    o->frac = 0;
    return o;
}
fi_obj *fi_style_list(unsigned long long type, unsigned long long width) {
    fi_obj *o = fi_obj_new(FI_KIND_LIST);
    if (!o) return 0;
    o->list_type = type ? 1 : 0;
    o->list_width = width > 2 ? 2 : (uint8_t)width;
    return o;
}
fi_obj *fi_style_currency(unsigned long long w0, unsigned long long w1, const void *locale) {
    fi_obj *o = fi_obj_new(FI_KIND_CURRENCY);
    char code[16];
    int n;
    if (!o) return 0;
    fi_read_locale(locale, o->locale, sizeof o->locale);
    n = fi_read_small(w0, w1, code, sizeof code);
    if (n < 0) snprintf(code, sizeof code, "USD");
    if (n > 3) n = 3;
    memcpy(o->currency, code, (size_t)(n < 0 ? 3 : n));
    if (!o->currency[0]) memcpy(o->currency, "USD", 3);
    o->frac = 2;
    return o;
}
fi_obj *fi_style_field(fi_obj *self, int field, unsigned long long arg) {
    fi_obj *o = fi_obj_clone(self);
    if (!o) return 0;
    if (o->kind != FI_KIND_DATE && o->kind != FI_KIND_INTERVAL) o->kind = FI_KIND_DATE;
    o->symbols = 1;
    if (field == FI_F_YEAR) o->year_on = arg ? 1 : 0;
    else if (field == FI_F_MONTH) o->month_kind = (uint8_t)(arg > 3 ? 2 : arg);
    else if (field == FI_F_DAY) o->day_on = arg ? 1 : 0;
    else if (field == FI_F_HOUR) o->hour_kind = (uint8_t)(arg > 2 ? 2 : arg);
    else if (field == FI_F_MINUTE) o->minute_on = arg ? 1 : 0;
    else if (field == FI_F_WEEKDAY) o->weekday_kind = (uint8_t)(arg > 2 ? 1 : arg);
    return o;
}
fi_obj *fi_style_iso_part(fi_obj *self, int bit) {
    fi_obj *o = fi_obj_clone(self);
    if (!o) return 0;
    o->kind = FI_KIND_ISO;
    o->iso_custom = 1;
    o->iso_fields = (uint8_t)(o->iso_fields | (1u << bit));
    return o;
}
fi_obj *fi_style_iso_sep(fi_obj *self, int which, unsigned long long val) {
    fi_obj *o = fi_obj_clone(self);
    if (!o) return 0;
    o->kind = FI_KIND_ISO;
    if (which == 0) o->iso_date_sep = val ? 1 : 0;
    else if (which == 1) o->iso_time_sep = val ? 1 : 0;
    else if (which == 2) o->iso_dt_sep = val ? 1 : 0;
    else o->iso_date_sep = val ? 1 : 0;
    return o;
}
fi_obj *fi_style_iso_full(unsigned long long dsep, unsigned long long dtsep, unsigned long long tsep,
                          unsigned long long tzsep, unsigned long long frac) {
    fi_obj *o = fi_style_iso_default();
    (void)tzsep;
    if (!o) return 0;
    o->iso_date_sep = dsep ? 1 : 0;
    o->iso_dt_sep = dtsep ? 1 : 0;
    o->iso_time_sep = tsep ? 1 : 0;
    o->frac = frac ? 1 : 0;
    return o;
}
fi_obj *fi_style_date_init(unsigned long long date_raw, unsigned long long time_raw, const void *locale) {
    fi_obj *o = fi_obj_new(FI_KIND_DATE);
    if (!o) return 0;
    o->date_style = date_raw > 4 ? 0 : (uint8_t)date_raw;
    o->time_style = time_raw > 3 ? 0 : (uint8_t)time_raw;
    fi_read_locale(locale, o->locale, sizeof o->locale);
    return o;
}
fi_obj *fi_style_sign(fi_obj *self, unsigned long long strategy) {
    fi_obj *o = fi_obj_clone(self);
    if (!o) return 0;
    if (o->kind != FI_KIND_INT && o->kind != FI_KIND_DEC) o->kind = FI_KIND_INT;
    o->sign_mode = strategy >= 2 ? 2 : (strategy ? 1 : 0);
    return o;
}
fi_obj *fi_style_precision(fi_obj *self, unsigned long long frac) {
    fi_obj *o = fi_obj_clone(self);
    if (!o) return 0;
    if (!o->kind) o->kind = FI_KIND_PERCENT;
    o->frac = frac > 6 ? 6 : (uint8_t)frac;
    return o;
}
fi_obj *fi_style_notation(fi_obj *self, unsigned long long notation) {
    fi_obj *o = fi_obj_clone(self);
    if (!o) return 0;
    if (!o->kind) o->kind = FI_KIND_DEC;
    o->notation = notation == 1 ? 1 : 0;
    return o;
}
fi_obj *fi_style_locale(fi_obj *self, const void *locale) {
    fi_obj *o = fi_obj_clone(self);
    if (!o) return 0;
    fi_read_locale(locale, o->locale, sizeof o->locale);
    return o;
}
unsigned long long fi_hour_from_ampm(unsigned long long ampm) { return ampm ? 2 : 1; }
unsigned long long fi_sign_pack(unsigned long long including_zero) { return including_zero ? 2 : 1; }

fi_swstr fi_currency_code(const fi_obj *o) {
    if (!o || o->magic != FI_MAGIC_STYLE || !o->currency[0]) return fi_small("USD", 3);
    return fi_small(o->currency, (int)strlen(o->currency));
}
fi_swstr fi_currency_locale(const fi_obj *o) {
    if (!o || o->magic != FI_MAGIC_STYLE) return fi_small("en", 2);
    return fi_small(o->locale, (int)strlen(o->locale));
}

/* ---- datas ---- */
static const char *fi_wday_short(const char *lang, int wd) {
    static const char *en[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    static const char *pt[] = {"dom.", "seg.", "ter.", "qua.", "qui.", "sex.", "sáb."};
    static const char *de[] = {"So.", "Mo.", "Di.", "Mi.", "Do.", "Fr.", "Sa."};
    static const char *fr[] = {"dim.", "lun.", "mar.", "mer.", "jeu.", "ven.", "sam."};
    const char **t = en;
    if (mf_is(lang, "pt")) t = pt;
    else if (mf_is(lang, "de")) t = de;
    else if (mf_is(lang, "fr")) t = fr;
    if (wd < 0 || wd > 6) wd = 0;
    return t[wd];
}

static int fi_render_symbols(const fi_obj *st, int y, int m, int d, int hh, int mm, int ss, char *buf, size_t cap) {
    char lang[8];
    int standard = !st->hour_kind && !st->minute_on && !st->second_on;
    mf_lang(st->locale, lang, sizeof lang);
    if (st->weekday_kind == 2 && st->month_kind == 3 && st->day_on && st->year_on && standard)
        return mf_date(st->locale, y, m, d, MF_DATE_COMPLETE, buf, cap);
    if (!st->weekday_kind && st->month_kind == 3 && st->day_on && st->year_on && standard)
        return mf_date(st->locale, y, m, d, MF_DATE_LONG, buf, cap);
    if (!st->weekday_kind && st->month_kind == 2 && st->day_on && st->year_on && standard)
        return mf_date(st->locale, y, m, d, MF_DATE_ABBREV, buf, cap);
    if (!st->weekday_kind && st->month_kind == 1 && st->day_on && st->year_on && standard)
        return mf_date(st->locale, y, m, d, MF_DATE_NUMERIC, buf, cap);
    {
        char tmp[160];
        int o = 0;
        tmp[0] = 0;
        if (st->weekday_kind) {
            const char *w = st->weekday_kind == 2 ? mf_wday(lang, mf_weekday(y, m, d)) : fi_wday_short(lang, mf_weekday(y, m, d));
            o += snprintf(tmp + o, sizeof tmp - (size_t)o, "%s", w);
        }
        if (st->month_kind || st->day_on || st->year_on) {
            char part[80];
            part[0] = 0;
            if (st->month_kind == 1 && st->day_on && st->year_on)
                mf_date(st->locale, y, m, d, MF_DATE_NUMERIC, part, sizeof part);
            else if (mf_is(lang, "en")) {
                if (st->month_kind >= 2) snprintf(part, sizeof part, "%s", mf_month(lang, m, st->month_kind == 3));
                if (st->day_on) snprintf(part + strlen(part), sizeof part - strlen(part), "%s%d", part[0] ? " " : "", d);
                if (st->year_on) snprintf(part + strlen(part), sizeof part - strlen(part), "%s%d", part[0] ? ", " : "", y);
            } else if (st->day_on && st->month_kind >= 2 && st->year_on) {
                mf_date(st->locale, y, m, d, st->month_kind == 3 ? MF_DATE_LONG : MF_DATE_ABBREV, part, sizeof part);
            } else {
                if (st->day_on) snprintf(part, sizeof part, "%d", d);
                if (st->month_kind >= 2) snprintf(part + strlen(part), sizeof part - strlen(part), "%s%s", part[0] ? " " : "", mf_month(lang, m, st->month_kind == 3));
                else if (st->month_kind == 1) snprintf(part + strlen(part), sizeof part - strlen(part), "%s%d", part[0] ? "/" : "", m);
                if (st->year_on) snprintf(part + strlen(part), sizeof part - strlen(part), "%s%d", part[0] ? " " : "", y);
            }
            if (part[0]) {
                if (o) o += snprintf(tmp + o, sizeof tmp - (size_t)o, ", ");
                o += snprintf(tmp + o, sizeof tmp - (size_t)o, "%s", part);
            }
        }
        if (st->hour_kind || st->minute_on) {
            char tim[40];
            int h = hh, min = st->minute_on ? mm : 0;
            if (st->hour_kind == 2 || (st->hour_kind == 0 && mf_is(lang, "en"))) {
                int h12 = h % 12; if (h12 == 0) h12 = 12;
                if (st->second_on) snprintf(tim, sizeof tim, "%d:%02d:%02d %s", h12, min, ss, h < 12 ? "AM" : "PM");
                else snprintf(tim, sizeof tim, "%d:%02d %s", h12, min, h < 12 ? "AM" : "PM");
            } else if (st->second_on) snprintf(tim, sizeof tim, "%02d:%02d:%02d", h, min, ss);
            else snprintf(tim, sizeof tim, "%02d:%02d", h, min);
            if (o) o += snprintf(tmp + o, sizeof tmp - (size_t)o, ", ");
            snprintf(tmp + o, sizeof tmp - (size_t)o, "%s", tim);
        }
        return mf_put(buf, cap, tmp);
    }
}

static fi_swstr fi_pack_date_obj(const fi_obj *st, unsigned long long bits) {
    int y, m, d, hh, mm, ss;
    char buf[160];
    int ds, ts;
    fi_parts(bits, &y, &m, &d, &hh, &mm, &ss);
    if (st && st->magic == FI_MAGIC_STYLE && st->kind == FI_KIND_ISO) {
        int fields = st->iso_custom ? st->iso_fields : 0x3f;
        char sep = st->iso_date_sep ? 0 : '-';
        char tsep = st->iso_time_sep ? 0 : ':';
        char dt = st->iso_dt_sep ? ' ' : 'T';
        int o = 0;
        buf[0] = 0;
        if (fields & 1) o += snprintf(buf + o, sizeof buf - (size_t)o, "%04d", y);
        if (fields & 2) o += snprintf(buf + o, sizeof buf - (size_t)o, "%s%02d", (sep && (fields & 1)) ? "-" : "", m);
        if (fields & 4) o += snprintf(buf + o, sizeof buf - (size_t)o, "%s%02d", (sep && (fields & (1 | 2))) ? "-" : "", d);
        if (fields & (8 | 16 | 32)) {
            if (o && (fields & 7)) buf[o++] = dt;
            if (fields & 8) o += snprintf(buf + o, sizeof buf - (size_t)o, "%02d", hh);
            if (fields & 16) o += snprintf(buf + o, sizeof buf - (size_t)o, "%s%02d", (tsep && (fields & 8)) ? ":" : "", mm);
            if (fields & 32) o += snprintf(buf + o, sizeof buf - (size_t)o, "%s%02d", (tsep && (fields & (8 | 16))) ? ":" : "", ss);
            snprintf(buf + o, sizeof buf - (size_t)o, "Z");
        }
        (void)sep;
        return fi_pack(buf, (int)strlen(buf));
    }
    if (st && st->magic == FI_MAGIC_STYLE && st->symbols)
        return fi_render_symbols(st, y, m, d, hh, mm, ss, buf, sizeof buf) < 0 ? fi_small("", 0) : fi_pack(buf, (int)strlen(buf));
    ds = -1;
    ts = 0;
    if (!st || st->magic != FI_MAGIC_STYLE || (st->date_style == 0xff && st->time_style == 0xff && st->kind == FI_KIND_DATE)) {
        ds = MF_DATE_NUMERIC;
        ts = 1;
    } else if (st->magic == FI_MAGIC_STYLE) {
        if (st->date_style != 0xff && st->date_style != 0) ds = (int)st->date_style - 1;
        if (st->time_style != 0xff && st->time_style > 0) ts = st->time_style > 2 ? 2 : (int)st->time_style;
    }
    if (mf_datetime(st && st->locale[0] ? st->locale : "en", y, m, d, hh, mm, ss, ds, ts, buf, sizeof buf) < 0)
        return fi_small("", 0);
    return fi_pack(buf, (int)strlen(buf));
}

fi_swstr fi_date_format_obj(unsigned long long bits, fi_obj *st) { return fi_pack_date_obj(st, bits); }

fi_swstr fi_iso_format_styled(unsigned long long bits, fi_obj *st) {
    if (!st || st->magic != FI_MAGIC_STYLE || st->kind != FI_KIND_ISO) return fi_iso_format(bits);
    return fi_pack_date_obj(st, bits);
}

fi_swstr fi_range_format_obj(const void *range, fi_obj *st) {
    double a = 0, b = 0;
    unsigned long long ba, bb;
    fi_swstr sa, sb;
    char as[80], bs[80], both[180];
    if (!range) return fi_small("", 0);
    memcpy(&a, range, sizeof a);
    memcpy(&b, (const char *)range + 8, sizeof b);
    memcpy(&ba, &a, sizeof ba);
    memcpy(&bb, &b, sizeof bb);
    sa = fi_pack_date_obj(st, ba);
    sb = fi_pack_date_obj(st, bb);
    if (fi_read_small(sa.w0, sa.w1, as, sizeof as) < 0) as[0] = 0;
    if (fi_read_small(sb.w0, sb.w1, bs, sizeof bs) < 0) bs[0] = 0;
    snprintf(both, sizeof both, "%s – %s", as, bs);
    return fi_pack(both, (int)strlen(both));
}

/* ---- números ---- */
static long long fi_round_even(double v) {
    int neg = v < 0;
    double x = neg ? -v : v;
    long long n = (long long)x;
    double frac = x - (double)n;
    if (frac > 0.5) n++;
    else if (frac == 0.5 && (n & 1)) n++;
    return neg ? -n : n;
}

static void fi_apply_sign(char *buf, size_t cap, int sign_mode, int neg, int zero) {
    if (neg || !sign_mode) return;
    if (zero && sign_mode == 1) return;
    if (cap < 2) return;
    memmove(buf + 1, buf, strlen(buf) + 1);
    buf[0] = '+';
}

static fi_swstr fi_pack_int(const fi_obj *st, long long v) {
    char buf[64];
    const char *loc = st && st->locale[0] ? st->locale : "en";
    int grouping = !st || st->grouping;
    int neg = v < 0;
    int zero = v == 0;
    if (mf_integer(loc, neg ? v : (v < 0 ? v : v), grouping, buf, sizeof buf) < 0) return fi_small("", 0);
    if (!neg) fi_apply_sign(buf, sizeof buf, st ? st->sign_mode : 0, 0, zero);
    return fi_pack(buf, (int)strlen(buf));
}

static void fi_compact(const char *locale, double v, char *buf, size_t cap) {
    char lang[8];
    double x = v < 0 ? -v : v;
    const char *suf = "";
    double s = x;
    int neg = v < 0;
    mf_lang(locale, lang, sizeof lang);
    if (x >= 1e9) { s = x / 1e9; suf = mf_is(lang, "pt") ? " bi" : mf_is(lang, "de") ? " Mrd." : mf_is(lang, "fr") ? " Md" : "B"; }
    else if (x >= 1e6) { s = x / 1e6; suf = mf_is(lang, "pt") ? " mi" : mf_is(lang, "de") ? " Mio." : "M"; }
    else if (x >= 1e3) { s = x / 1e3; suf = mf_is(lang, "pt") ? " mil" : mf_is(lang, "de") ? " Tsd." : mf_is(lang, "fr") ? " k" : "K"; }
    if (suf[0]) {
        char num[32];
        mf_decimal(locale, neg ? -s : s, (s == (double)(long long)s) ? 0 : 1, 0, num, sizeof num);
        snprintf(buf, cap, "%s%s", num, suf);
    } else mf_decimal(locale, v, 0, 1, buf, cap);
}

static fi_swstr fi_pack_double(const fi_obj *st, double v) {
    char buf[64];
    const char *loc = st && st->locale[0] ? st->locale : "en";
    uint8_t kind = st && st->magic == FI_MAGIC_STYLE ? st->kind : FI_KIND_DEC;
    int frac = st ? st->frac : 255;
    if (kind == FI_KIND_PERCENT) {
        double p = v * 100.0;
        int f = frac == 255 ? 0 : frac;
        if (f == 0) snprintf(buf, sizeof buf, "%lld%%", (long long)fi_round_even(p));
        else {
            char num[48];
            mf_decimal(loc, p, f, 1, num, sizeof num);
            snprintf(buf, sizeof buf, "%s%%", num);
        }
        return fi_pack(buf, (int)strlen(buf));
    }
    if (kind == FI_KIND_CURRENCY || (st && st->currency[0] && kind == FI_KIND_DEC && st->frac == 2 && st->notation == 0 && st->kind == FI_KIND_CURRENCY)) {
        char num[48];
        const char *code = st && st->currency[0] ? st->currency : "USD";
        mf_decimal(loc, v < 0 ? -v : v, 2, 1, num, sizeof num);
        {
            char lang[8];
            mf_lang(loc, lang, sizeof lang);
            if (!strcmp(code, "EUR") && (mf_is(lang, "de") || mf_is(lang, "fr")))
                snprintf(buf, sizeof buf, "%s%s €", v < 0 ? "-" : "", num);
            else if (!strcmp(code, "BRL"))
                snprintf(buf, sizeof buf, "%sR$ %s", v < 0 ? "-" : "", num);
            else
                snprintf(buf, sizeof buf, "%s$%s", v < 0 ? "-" : "", num);
        }
        return fi_pack(buf, (int)strlen(buf));
    }
    if (st && st->notation == 1) {
        fi_compact(loc, v, buf, sizeof buf);
        return fi_pack(buf, (int)strlen(buf));
    }
    if (frac == 255) frac = (v == (double)(long long)v) ? 0 : 2;
    if (mf_decimal(loc, v, frac, st ? st->grouping : 1, buf, sizeof buf) < 0) return fi_small("", 0);
    return fi_pack(buf, (int)strlen(buf));
}

fi_swstr fi_int_format_obj(long long v, fi_obj *st) {
    if (st && st->magic == FI_MAGIC_STYLE && (st->kind == FI_KIND_PERCENT || st->kind == FI_KIND_CURRENCY || st->notation == 1))
        return fi_pack_double(st, (double)v);
    return fi_pack_int(st, v);
}
fi_swstr fi_float_format_obj(unsigned long long bits, fi_obj *st) {
    double v;
    memcpy(&v, &bits, sizeof v);
    if (st && st->magic == FI_MAGIC_STYLE && (st->kind == FI_KIND_PERCENT || st->kind == FI_KIND_CURRENCY || st->notation == 1))
        return fi_pack_double(st, v);
    if (st && st->magic == FI_MAGIC_STYLE && st->kind == FI_KIND_INT && st->frac == 255)
        return fi_pack_int(st, (long long)fi_round_even(v));
    return fi_pack_double(st, v);
}

/* ---- listas ---- */
static int fi_join_list(const char *locale, const char **items, int n, int type, int width, char *out, size_t cap) {
    char lang[8];
    const char *last;
    size_t o = 0;
    int i;
    mf_lang(locale, lang, sizeof lang);
    if (n < 0) n = 0;
    if (width == 2) {
        last = 0;
    } else if (type == 1) {
        if (mf_is(lang, "en") && n >= 3) last = ", or ";
        else if (mf_is(lang, "pt")) last = " ou ";
        else if (mf_is(lang, "de")) last = " oder ";
        else if (mf_is(lang, "fr")) last = " ou ";
        else last = " or ";
    } else if (width == 1 && mf_is(lang, "en")) last = " & ";
    else return mf_list(locale, items, n, out, cap);
    for (i = 0; i < n; i++) {
        const char *sep = "";
        if (i > 0 && i == n - 1 && last) sep = last;
        else if (i > 0) sep = ", ";
        {
            size_t ns = strlen(sep), ni = strlen(items[i] ? items[i] : "");
            if (o + ns + ni + 1 > cap) return -1;
            memcpy(out + o, sep, ns); o += ns;
            memcpy(out + o, items[i] ? items[i] : "", ni); o += ni;
        }
    }
    out[o] = 0;
    return (int)o;
}

fi_swstr fi_list_format(const char *locale, const char **items, int n, int type, int width) {
    char buf[256];
    if (fi_join_list(locale ? locale : "en", items, n, type, width, buf, sizeof buf) < 0) return fi_small("", 0);
    return fi_pack(buf, (int)strlen(buf));
}

fi_swstr fi_seq_format_obj(const void *seq, fi_obj *st) {
    const char *items[8];
    char storage_buf[8][32];
    int n = 0, i;
    const char *loc = st && st->locale[0] ? st->locale : "en";
    int type = st ? st->list_type : 0;
    int width = st ? st->list_width : 0;
    size_t count = 0, cap = 0;
    if (!seq) return fi_small("", 0);
    memcpy(&count, (const char *)seq + 16, sizeof count);
    memcpy(&cap, (const char *)seq + 24, sizeof cap);
    if (count == 0 || count > 8 || cap < count || cap > 64) return fi_small("", 0);
    for (i = 0; i < (int)count; i++) {
        unsigned long long w0, w1;
        memcpy(&w0, (const char *)seq + 32 + i * 16, 8);
        memcpy(&w1, (const char *)seq + 32 + i * 16 + 8, 8);
        if (fi_read_small(w0, w1, storage_buf[n], 32) < 0) return fi_small("", 0);
        items[n] = storage_buf[n];
        n++;
    }
    return fi_list_format(loc, items, n, type, width);
}

/* ---- strings ---- */
static int fi_fold(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A' + 'a';
    return c;
}
static int fi_cmp_bytes(const char *a, const char *b, int mode) {
    const unsigned char *x = (const unsigned char *)(a ? a : "");
    const unsigned char *y = (const unsigned char *)(b ? b : "");
    while (*x || *y) {
        if ((mode == 1 || mode == 2) && *x >= '0' && *x <= '9' && *y >= '0' && *y <= '9' && mode == 2) {
            unsigned long long nx = 0, ny = 0;
            while (*x == '0') x++;
            while (*y == '0') y++;
            while (*x >= '0' && *x <= '9' && nx < 1000000000000ULL) nx = nx * 10 + (unsigned)(*x++ - '0');
            while (*y >= '0' && *y <= '9' && ny < 1000000000000ULL) ny = ny * 10 + (unsigned)(*y++ - '0');
            if (nx != ny) return nx < ny ? -1 : 1;
            continue;
        }
        {
            int cx = mode ? fi_fold(*x) : *x;
            int cy = mode ? fi_fold(*y) : *y;
            if (cx != cy) return cx < cy ? -1 : 1;
            if (*x) x++;
            if (*y) y++;
        }
    }
    return 0;
}
int fi_str_cmp_cstr(const char *a, const char *b, int mode) { return fi_cmp_bytes(a, b, mode); }
int fi_str_cmp_sw(unsigned long long a0, unsigned long long a1, unsigned long long b0, unsigned long long b1, int mode) {
    char as[64], bs[64];
    if (fi_read_small(a0, a1, as, sizeof as) < 0) as[0] = 0;
    if (fi_read_small(b0, b1, bs, sizeof bs) < 0) bs[0] = 0;
    return fi_cmp_bytes(as, bs, mode);
}

static int fi_fmt_into(const char *fmt, const long long *ints, int ni, const char **strs, int ns, char *out, size_t cap) {
    size_t o = 0;
    int ii = 0, si = 0;
    const char *p;
    if (!fmt) fmt = "";
    for (p = fmt; *p && o + 1 < cap;) {
        if (*p != '%') { out[o++] = *p++; continue; }
        p++;
        if (*p == '%') { out[o++] = '%'; p++; continue; }
        if (*p == 's' || *p == '@') {
            const char *s = si < ns && strs && strs[si] ? strs[si] : "";
            size_t n = strlen(s);
            si++;
            p++;
            if (o + n >= cap) return -1;
            memcpy(out + o, s, n); o += n;
            continue;
        }
        {
            int prec = -1;
            int lng = 0;
            while (*p == 'l') { lng++; p++; }
            if (*p == '.') {
                p++;
                prec = 0;
                while (*p >= '0' && *p <= '9') prec = prec * 10 + (*p++ - '0');
            }
            if (*p == 'd' || *p == 'i' || *p == 'u') {
                char tmp[32];
                long long v = ii < ni ? ints[ii++] : 0;
                snprintf(tmp, sizeof tmp, "%lld", v);
                {
                    size_t n = strlen(tmp);
                    if (o + n >= cap) return -1;
                    memcpy(out + o, tmp, n); o += n;
                }
                p++;
                (void)lng;
                continue;
            }
            if (*p == 'f') {
                char tmp[64];
                double d = ii < ni ? (double)ints[ii++] : 0;
                int f = prec < 0 ? 6 : (prec > 6 ? 6 : prec);
                mf_decimal("en", d, f, 0, tmp, sizeof tmp);
                {
                    size_t n = strlen(tmp);
                    if (o + n >= cap) return -1;
                    memcpy(out + o, tmp, n); o += n;
                }
                p++;
                continue;
            }
            out[o++] = '%';
        }
    }
    out[o] = 0;
    return (int)o;
}

typedef struct { int kind; long long i; double d; unsigned long long w0, w1; const char *cstr; } fi_arg;

static fi_swstr fi_format_go(const char *fmt, const fi_arg *args, int argc) {
    long long ints[8];
    const char *strs[8];
    char tmp[8][64];
    int ni = 0, ns = 0, i;
    char out[192];
    for (i = 0; i < argc && i < 8; i++) {
        if (!args) break;
        if (args[i].kind == 2) {
            if (args[i].cstr) strs[ns++] = args[i].cstr;
            else {
                fi_read_small(args[i].w0, args[i].w1, tmp[ns], 64);
                strs[ns] = tmp[ns];
                ns++;
            }
        } else ints[ni++] = args[i].kind == 1 ? (long long)args[i].d : args[i].i;
    }
    if (fi_fmt_into(fmt, ints, ni, strs, ns, out, sizeof out) < 0) return fi_small("", 0);
    return fi_pack(out, (int)strlen(out));
}

fi_swstr fi_str_format_args(const char *fmt, const fi_arg *args, int argc) { return fi_format_go(fmt, args, argc); }

fi_swstr fi_str_format(unsigned long long w0, unsigned long long w1, long argc, const void *argv) {
    char fmt[96];
    if (fi_read_small(w0, w1, fmt, sizeof fmt) < 0) fmt[0] = 0;
    if (argc < 0 || argc > 8 || !argv) argc = 0;
    return fi_format_go(fmt, argv, (int)argc);
}
fi_swstr fi_str_append_format(unsigned long long s0, unsigned long long s1, unsigned long long f0, unsigned long long f1,
                              long argc, const void *argv) {
    char self[64], rest[160], both[192];
    fi_swstr add = fi_str_format(f0, f1, argc, argv);
    if (fi_read_small(s0, s1, self, sizeof self) < 0) self[0] = 0;
    if (fi_read_small(add.w0, add.w1, rest, sizeof rest) < 0) rest[0] = 0;
    snprintf(both, sizeof both, "%s%s", self, rest);
    return fi_pack(both, (int)strlen(both));
}

/* ---- pessoa ---- */
fi_person *fi_person_new(void) { return fi_alloc(sizeof(fi_person), FI_MAGIC_PERSON); }
static char **fi_person_slot(fi_person *p, int field) {
    if (field == 0) return &p->prefix;
    if (field == 1) return &p->given;
    if (field == 2) return &p->middle;
    if (field == 3) return &p->family;
    return &p->suffix;
}
fi_swstr fi_person_get(fi_person *p, int field) {
    char *s;
    if (!p || p->magic != FI_MAGIC_PERSON) return (fi_swstr){0, 0};
    s = *fi_person_slot(p, field);
    if (!s) return (fi_swstr){0, 0};
    return fi_pack(s, (int)strlen(s));
}
void fi_person_set(fi_person **slot, int field, unsigned long long w0, unsigned long long w1) {
    fi_person *p;
    char buf[96];
    char **dest;
    int n;
    if (!slot) return;
    p = *slot;
    if (!p || p->magic != FI_MAGIC_PERSON) {
        p = fi_person_new();
        *slot = p;
    }
    if (!p) return;
    if (p->ref > 1) {
        fi_person *c = fi_alloc(sizeof *c, FI_MAGIC_PERSON);
        if (!c) return;
        c->prefix = p->prefix ? strdup(p->prefix) : 0;
        c->given = p->given ? strdup(p->given) : 0;
        c->middle = p->middle ? strdup(p->middle) : 0;
        c->family = p->family ? strdup(p->family) : 0;
        c->suffix = p->suffix ? strdup(p->suffix) : 0;
        fi_release(p);
        *slot = p = c;
    }
    n = fi_read_small(w0, w1, buf, sizeof buf);
    dest = fi_person_slot(p, field);
    free(*dest);
    if (n < 0 || (w0 == 0 && w1 == 0)) { *dest = 0; return; }
    *dest = strdup(buf);
}
fi_swstr fi_person_formatted(fi_person *p) {
    char buf[192];
    const char *g, *f;
    if (!p || p->magic != FI_MAGIC_PERSON) return fi_small("", 0);
    g = p->given; f = p->family;
    if (!g && !f) {
        const char *alt = p->prefix ? p->prefix : p->middle ? p->middle : p->suffix;
        return alt ? fi_pack(alt, (int)strlen(alt)) : fi_small("", 0);
    }
    snprintf(buf, sizeof buf, "%s%s%s", g ? g : "", (g && f) ? " " : "", f ? f : "");
    return fi_pack(buf, (int)strlen(buf));
}
/* STUB justificado: NSPersonNameComponents não é uma classe do libSystem (só o formatter). */
void *fi_person_bridge_objc(fi_person *p) { (void)p; return 0; }
fi_person *fi_person_from_objc(void *obj) { (void)obj; return 0; }

/* ---- attributed string ---- */
static fi_attr *fi_attr_new(const char *s, int n) {
    fi_attr *a = fi_alloc(sizeof *a, FI_MAGIC_ATTR);
    if (!a) return 0;
    if (n < 0) n = s ? (int)strlen(s) : 0;
    a->text = calloc(1, (size_t)n + 1);
    if (!a->text) { free(a); return 0; }
    if (s && n) memcpy(a->text, s, (size_t)n);
    a->len = n;
    a->runs[0].kind = MF_TEXT;
    a->runs[0].len = n;
    a->runs[0].href = -1;
    a->nruns = 1;
    return a;
}
fi_attr *fi_attr_empty(void) { return fi_attr_new("", 0); }
fi_attr *fi_attr_from_sw(unsigned long long w0, unsigned long long w1) {
    char buf[96];
    int n = fi_read_small(w0, w1, buf, sizeof buf);
    if (n < 0) n = 0;
    return fi_attr_new(buf, n);
}
fi_attr *fi_attr_from_cstr(const char *s, int n) { return fi_attr_new(s, n); }
const char *fi_attr_plain(const fi_attr *a) { return a && a->magic == FI_MAGIC_ATTR && a->text ? a->text : ""; }
int fi_attr_nruns(const fi_attr *a) { return a && a->magic == FI_MAGIC_ATTR ? a->nruns : 0; }
int fi_attr_run_kind(const fi_attr *a, int i) {
    if (!a || a->magic != FI_MAGIC_ATTR || i < 0 || i >= a->nruns) return -1;
    return a->runs[i].kind;
}

fi_attr *fi_attr_concat(fi_attr *a, fi_attr *b) {
    const char *as = fi_attr_plain(a), *bs = fi_attr_plain(b);
    size_t na = strlen(as), nb = strlen(bs);
    char *buf = malloc(na + nb + 1);
    fi_attr *o;
    if (!buf) return fi_attr_empty();
    memcpy(buf, as, na);
    memcpy(buf + na, bs, nb + 1);
    o = fi_attr_new(buf, (int)(na + nb));
    free(buf);
    return o;
}
void fi_attr_append(fi_attr **slot, fi_attr *other) {
    fi_attr *n;
    if (!slot) return;
    n = fi_attr_concat(*slot, other);
    fi_release(*slot);
    *slot = n;
}
fi_attr *fi_attr_characters(fi_attr *a) { fi_retain(a); return a; }
fi_swstr fi_attr_as_string(fi_attr *a) {
    const char *s = fi_attr_plain(a);
    return fi_pack(s, (int)strlen(s));
}
fi_attr *fi_attr_markdown(const char *src, const fi_opt *opt) {
    char plain[192], hrefs[192];
    mf_span sp[12];
    int ns = mf_markdown(src ? src : "", plain, sizeof plain, sp, 12, hrefs, sizeof hrefs);
    fi_attr *a;
    int i;
    if (ns < 0) {
        if (opt && opt->failure == 1) return 0;
        return fi_attr_new(src ? src : "", -1);
    }
    a = fi_attr_new(plain, (int)strlen(plain));
    if (!a) return 0;
    a->nruns = ns > 12 ? 12 : ns;
    for (i = 0; i < a->nruns; i++) a->runs[i] = sp[i];
    snprintf(a->hrefs, sizeof a->hrefs, "%s", hrefs);
    return a;
}
fi_opt *fi_mdopt_new(unsigned long long allow, unsigned long long syntax, unsigned long long failure,
                     unsigned long long l0, unsigned long long l1) {
    fi_opt *o = fi_alloc(sizeof *o, FI_MAGIC_OPT);
    char lang[16];
    if (!o) return 0;
    o->allow_ext = allow ? 1 : 0;
    o->syntax = (int)syntax;
    o->failure = (int)failure;
    if (fi_read_small(l0, l1, lang, sizeof lang) > 0) snprintf(o->lang, sizeof o->lang, "%s", lang);
    return o;
}
fi_run *fi_attr_runs(fi_attr *a) {
    fi_run *r = fi_alloc(sizeof *r, FI_MAGIC_RUN);
    if (!r) return 0;
    fi_retain(a);
    r->attr = a;
    r->index = 0;
    return r;
}
typedef struct { long long a, b; } fi_pair;
fi_pair fi_run_bounds(fi_run *r) {
    fi_pair p;
    p.a = 0;
    p.b = 0;
    if (r && r->magic == FI_MAGIC_RUN && r->attr && r->index >= 0 && r->index < r->attr->nruns) {
        p.a = r->attr->runs[r->index].start;
        p.b = p.a + r->attr->runs[r->index].len;
    }
    return p;
}
fi_pair fi_nsrange_make(unsigned long long lo, unsigned long long hi) {
    fi_pair p;
    if (hi >= lo && hi - lo < 100000000ull) { p.a = (long long)lo; p.b = (long long)(hi - lo); }
    else { p.a = 0; p.b = 0; }
    return p;
}
fi_attr *fi_attr_slice(fi_attr *a, unsigned long long lo, unsigned long long hi) {
    const char *s = fi_attr_plain(a);
    int n = (int)strlen(s);
    int start, end;
    if (lo > (unsigned long long)n) return fi_attr_new(s, n);
    start = (int)lo;
    end = (hi > (unsigned long long)n || hi < lo) ? n : (int)hi;
    return fi_attr_new(s + start, end - start);
}
fi_swstr fi_attr_member(fi_attr *a, const void *key);
fi_swstr fi_run_member(fi_run *r, const void *key) {
    if (!r || r->magic != FI_MAGIC_RUN) return (fi_swstr){0, 0};
    return fi_attr_member(r->attr, key);
}

const char fi_link_key[] = "link";
fi_swstr fi_attr_member(fi_attr *a, const void *key) {
    int i;
    if (!a || a->magic != FI_MAGIC_ATTR) return (fi_swstr){0, 0};
    if (key != fi_link_key && !(key && !strcmp(key, "link"))) return (fi_swstr){0, 0};
    for (i = 0; i < a->nruns; i++) if (a->runs[i].kind == MF_LINK && a->runs[i].href >= 0)
        return fi_pack(a->hrefs + a->runs[i].href, a->runs[i].href_len);
    return (fi_swstr){0, 0};
}

/* ---- decimal a mais. fi_dec_from_str é o parser do abi, no mesmo TU. ---- */
void fi_dec_from_uint(unsigned long long v, void *out) {
    char b[48];
    fi_nsdec d;
    snprintf(b, sizeof b, "%llu", v);
    if (fi_dec_from_str(&d, b)) fi_dec_zero(&d);
    if (out) memcpy(out, &d, sizeof d);
}

long long fi_dec_exponent(const void *d) {
    fi_nsdec x;
    if (!d) return 0;
    memcpy(&x, d, sizeof x);
    return (long long)(signed char)(x._exponent & 0xff);
}
int fi_dec_isnan(const void *d) {
    fi_nsdec x;
    if (!d) return 0;
    memcpy(&x, d, sizeof x);
    return x._exponent == -128;
}
void fi_dec_negate(void *d) {
    fi_nsdec x;
    char buf[80];
    if (!d) return;
    memcpy(&x, d, sizeof x);
    if (fi_dec_format(&x, buf, sizeof buf)) return;
    if (!strcmp(buf, "0") || !strcmp(buf, "NaN")) return;
    x._isNegative = x._isNegative ? 0 : 1;
    memcpy(d, &x, sizeof x);
}
void fi_dec_significand(const void *d, void *out) {
    fi_nsdec x;
    if (!d || !out) return;
    memcpy(&x, d, sizeof x);
    x._exponent = 0;
    memcpy(out, &x, sizeof x);
}
void fi_dec_pow(const void *base, long long exp, void *out) {
    unsigned char acc[32], cur[32], one[32];
    long long n;
    int inv = 0;
    if (!out) return;
    if (exp < 0) { inv = 1; n = -exp; }
    else n = exp;
    fi_dec_from_int(1, one);
    memcpy(acc, one, 20);
    if (base) memcpy(cur, base, 20);
    else fi_dec_from_int(0, cur);
    if (n == 0) {
        char z[32];
        z[0] = 0;
        if (base) fi_dec_format(base, z, sizeof z);
        /* 0^0 → NaN, pelo mesmo caminho de 0/0. Base nula não é um decimal. */
        if (base && z[0] == '0') { fi_dec_binop(base, base, out, 3); return; }
        memcpy(out, one, 20);
        return;
    }
    while (n) {
        unsigned char next[32];
        if (n & 1) { fi_dec_binop(acc, cur, next, 2); memcpy(acc, next, 20); }
        n >>= 1;
        if (n) { fi_dec_binop(cur, cur, next, 2); memcpy(cur, next, 20); }
    }
    if (inv) {
        unsigned char q[32];
        fi_dec_binop(one, acc, q, 3);
        memcpy(out, q, 20);
    } else memcpy(out, acc, 20);
}

static int fi_dec_parse_locale_str(const char *s, const char *locale, char *out, size_t cap) {
    char group, dec, tmp[80];
    int j = 0, i;
    if (!s || !s[0]) return -1;
    mf_seps(locale ? locale : "en", &group, &dec);
    for (i = 0; s[i] && j + 1 < (int)sizeof tmp; i++) {
        char c = s[i];
        if (c == group) continue;
        if (c == dec) c = '.';
        if (c == ' ') continue;
        tmp[j++] = c;
    }
    tmp[j] = 0;
    if (!j) return -1;
    for (i = 0; tmp[i]; i++) {
        if (tmp[i] == '-' || tmp[i] == '+' || tmp[i] == '.' || (tmp[i] >= '0' && tmp[i] <= '9')) continue;
        return -1;
    }
    snprintf(out, cap, "%s", tmp);
    return 0;
}

int fi_dec_from_locale(const char *s, const char *locale, void *out, int *is_nil) {
    char norm[80];
    fi_nsdec d;
    if (is_nil) *is_nil = 1;
    if (!out) return -1;
    memset(out, 0, 21);
    if (fi_dec_parse_locale_str(s, locale, norm, sizeof norm) || fi_dec_from_str(&d, norm)) {
        ((unsigned char *)out)[20] = 1;
        return -1;
    }
    memcpy(out, &d, sizeof d);
    ((unsigned char *)out)[20] = 0;
    if (is_nil) *is_nil = 0;
    return 0;
}

void fi_dec_locale_entry(unsigned long long w0, unsigned long long w1, const void *locale, void *out) {
    char buf[80], loc[8];
    int nil = 1;
    fi_read_locale(locale, loc, sizeof loc);
    if (fi_read_small(w0, w1, buf, sizeof buf) < 0) buf[0] = 0;
    fi_dec_from_locale(buf, loc, out, &nil);
}

fi_swstr fi_meas_format(const void *meas) {
    double v = 0;
    const char *unit = "";
    char buf[64], num[48];
    if (meas) {
        memcpy(&v, meas, sizeof v);
        unit = *(const char *const *)((const char *)meas + 8);
        if (!unit) unit = "";
        else {
            const unsigned char *u = (const unsigned char *)unit;
            int i, ok = u[0] && u[0] < 128;
            for (i = 0; ok && unit[i] && i < 8; i++) if (unit[i] < 32 || unit[i] > 126) ok = 0;
            if (!ok || !unit[0] || strlen(unit) > 8) unit = "";
        }
    }
    if (mf_decimal("en", v, (v == (double)(long long)v) ? 0 : 2, 1, num, sizeof num) < 0) num[0] = 0;
    if (unit[0]) snprintf(buf, sizeof buf, "%s %s", num, unit);
    else snprintf(buf, sizeof buf, "%s", num);
    return fi_pack(buf, (int)strlen(buf));
}

static void *fi_plist_tab_k[32];
static long fi_plist_tab_v[32];
void fi_plist_set_format(void *enc, long fmt) {
    int i, free_i = -1;
    if (!enc) return;
    for (i = 0; i < 32; i++) {
        if (fi_plist_tab_k[i] == enc) { fi_plist_tab_v[i] = fmt; return; }
        if (!fi_plist_tab_k[i] && free_i < 0) free_i = i;
    }
    if (free_i >= 0) { fi_plist_tab_k[free_i] = enc; fi_plist_tab_v[free_i] = fmt; }
}
long fi_plist_get_format(void *enc) {
    int i;
    for (i = 0; i < 32; i++) if (fi_plist_tab_k[i] == enc) return fi_plist_tab_v[i];
    return 0;
}

/* STUB justificado: o NSDecimalNumber do libSystem guarda double, não o layout de 20 bytes. */
void *fi_dec_bridge_objc(const void *d) { (void)d; return 0; }
void fi_dec_from_objc(void *obj, void *out) {
    (void)obj;
    if (out) memset(out, 0, 20);
}

/* Slot de materialize para KeyPath que o compilador escreve e a gente não interpreta. */
unsigned char fi_member_slot[16];

#ifndef FI_HOST
void *cx_class_named(const char *name);
void *objc_msgSend(void *, void *, ...);
void *sel_registerName(const char *);
#endif

void *fi_nsattr_from_plain(const char *plain) {
#ifndef FI_HOST
    void *cls = cx_class_named("NSAttributedString");
    void *strcls = cx_class_named("NSString");
    if (cls && strcls) {
        void *str = objc_msgSend(strcls, sel_registerName("stringWithUTF8String:"), plain ? plain : "");
        void *obj = str ? objc_msgSend(objc_msgSend(cls, sel_registerName("alloc")), sel_registerName("initWithString:"), str) : 0;
        if (obj) return obj;
    }
#endif
    return fi_attr_from_cstr(plain ? plain : "", plain ? (int)strlen(plain) : 0);
}

void *fi_nsattr_from_sw(unsigned long long w0, unsigned long long w1) {
    char buf[96];
    int n = fi_read_small(w0, w1, buf, sizeof buf);
    if (n < 0) buf[0] = 0;
    return fi_nsattr_from_plain(buf);
}

void *fi_nsattr_markdown_sw(unsigned long long w0, unsigned long long w1, fi_opt *opt) {
    char buf[192];
    fi_attr *a;
    void *o;
    int n = fi_read_small(w0, w1, buf, sizeof buf);
    if (n < 0) buf[0] = 0;
    a = fi_attr_markdown(buf, opt);
    if (!a) return 0;
    o = fi_nsattr_from_plain(fi_attr_plain(a));
    fi_release(a);
    return o;
}

#endif
