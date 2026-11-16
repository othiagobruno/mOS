/* mOS libSystem — núcleo dos Apple Events, sem CF nem ObjC: descritores (escalares, listas, registros, eventos),
 * coerções e o formato de fio entre processos. Incluído por appleevents.c e tools/aetest.c.
 * Quem inclui fornece u8/u32/i32/u64/i64, malloc/realloc/free/memcpy/memset/memcmp/strlen e snprintf.
 *
 * Tipos (os códigos de 4 letras do AEDataModel.h): 'utf8' 'TEXT' 'utxt' (UTF-16 na ordem da máquina) 'shor' 'long'
 * 'comp' (int64) 'magn' (uint32) 'sing' 'doub' 'bool' 'true' 'fals' 'enum' 'type' 'null' 'furl' 'bund' 'kpid'
 * 'ldt ' (data: segundos desde 1904, int64) 'list' 'reco' 'aevt' '****'.
 * Índices de lista começam em 1, como no Apple Event Manager. */

#define AE_FCC(a, b, c, d) ((u32)(a) << 24 | (u32)(b) << 16 | (u32)(c) << 8 | (u32)(d))
enum {
    AE_UTF8 = AE_FCC('u', 't', 'f', '8'), AE_TEXT = AE_FCC('T', 'E', 'X', 'T'), AE_UTXT = AE_FCC('u', 't', 'x', 't'),
    AE_SHOR = AE_FCC('s', 'h', 'o', 'r'), AE_LONG = AE_FCC('l', 'o', 'n', 'g'), AE_COMP = AE_FCC('c', 'o', 'm', 'p'),
    AE_MAGN = AE_FCC('m', 'a', 'g', 'n'), AE_SING = AE_FCC('s', 'i', 'n', 'g'), AE_DOUB = AE_FCC('d', 'o', 'u', 'b'),
    AE_BOOL = AE_FCC('b', 'o', 'o', 'l'), AE_TRUE = AE_FCC('t', 'r', 'u', 'e'), AE_FALS = AE_FCC('f', 'a', 'l', 's'),
    AE_ENUM = AE_FCC('e', 'n', 'u', 'm'), AE_TYPE = AE_FCC('t', 'y', 'p', 'e'), AE_NULL = AE_FCC('n', 'u', 'l', 'l'),
    AE_FURL = AE_FCC('f', 'u', 'r', 'l'), AE_BUND = AE_FCC('b', 'u', 'n', 'd'), AE_KPID = AE_FCC('k', 'p', 'i', 'd'),
    AE_LDT = AE_FCC('l', 'd', 't', ' '), AE_LIST = AE_FCC('l', 'i', 's', 't'), AE_RECO = AE_FCC('r', 'e', 'c', 'o'),
    AE_AEVT = AE_FCC('a', 'e', 'v', 't'), AE_WILD = AE_FCC('*', '*', '*', '*'), AE_DIRECT = AE_FCC('-', '-', '-', '-'),
    AE_ERRN = AE_FCC('e', 'r', 'r', 'n'), AE_ERRS = AE_FCC('e', 'r', 'r', 's'),
};
enum { AEK_SCALAR = 0, AEK_LIST, AEK_RECORD, AEK_EVENT };

typedef struct ae_node {
    u32 type;
    int kind;
    u8 *data;
    u32 n;
    struct ae_node **items;      /* lista; registro e parâmetros do evento (com keys) */
    u32 *keys;
    u32 count, cap;
    u32 ev_class, ev_id;         /* evento */
    i32 return_id, transaction_id;
    struct ae_node *target;
    struct ae_node **attrs;
    u32 *akeys, nattrs, capattrs;
} ae_node;

static void ae_free(ae_node *d) {
    if (!d) return;
    for (u32 i = 0; i < d->count; i++) ae_free(d->items[i]);
    for (u32 i = 0; i < d->nattrs; i++) ae_free(d->attrs[i]);
    ae_free(d->target);
    free(d->items); free(d->keys); free(d->attrs); free(d->akeys); free(d->data);
    free(d);
}
static ae_node *ae_alloc(u32 type, int kind) {
    ae_node *d = calloc(1, sizeof *d);
    if (d) { d->type = type; d->kind = kind; }
    return d;
}
static ae_node *ae_scalar(u32 type, const void *data, u32 n) {
    ae_node *d = ae_alloc(type, AEK_SCALAR);
    if (!d) return 0;
    if (n) {
        d->data = malloc(n);
        if (!d->data) { free(d); return 0; }
        memcpy(d->data, data, n);
    }
    d->n = n;
    return d;
}
static ae_node *ae_text(const char *s) { return ae_scalar(AE_UTF8, s, (u32)strlen(s)); }
static ae_node *ae_i32(i32 v) { return ae_scalar(AE_LONG, &v, 4); }
static ae_node *ae_f64(double v) { return ae_scalar(AE_DOUB, &v, 8); }
static ae_node *ae_bool(int v) { u8 b = v ? 1 : 0; return ae_scalar(AE_BOOL, &b, 1); }
static ae_node *ae_code(u32 type, u32 code) { return ae_scalar(type, &code, 4); }
static ae_node *ae_list(void) { return ae_alloc(AE_LIST, AEK_LIST); }
static ae_node *ae_record(void) { return ae_alloc(AE_RECO, AEK_RECORD); }
static ae_node *ae_event(u32 cls, u32 id, ae_node *target, i32 rid, i32 tid) {
    ae_node *d = ae_alloc(AE_AEVT, AEK_EVENT);
    if (!d) return 0;
    d->ev_class = cls; d->ev_id = id; d->target = target; d->return_id = rid; d->transaction_id = tid;
    return d;
}
static ae_node *ae_copy(const ae_node *s) {
    if (!s) return 0;
    ae_node *d = ae_scalar(s->type, s->data, s->n);
    if (!d) return 0;
    d->kind = s->kind;
    d->ev_class = s->ev_class; d->ev_id = s->ev_id; d->return_id = s->return_id; d->transaction_id = s->transaction_id;
    d->target = ae_copy(s->target);
    if (s->count) {
        d->items = malloc(sizeof *d->items * s->count);
        d->keys = malloc(sizeof *d->keys * s->count);
        for (u32 i = 0; i < s->count; i++) { d->items[i] = ae_copy(s->items[i]); d->keys[i] = s->keys ? s->keys[i] : 0; }
        d->count = d->cap = s->count;
    }
    if (s->nattrs) {
        d->attrs = malloc(sizeof *d->attrs * s->nattrs);
        d->akeys = malloc(sizeof *d->akeys * s->nattrs);
        for (u32 i = 0; i < s->nattrs; i++) { d->attrs[i] = ae_copy(s->attrs[i]); d->akeys[i] = s->akeys[i]; }
        d->nattrs = d->capattrs = s->nattrs;
    }
    return d;
}
static int ae_grow(ae_node *d) {
    if (d->count < d->cap) return 0;
    u32 nc = d->cap ? d->cap * 2 : 4;
    ae_node **ni = realloc(d->items, sizeof *ni * nc);
    if (!ni) return -1;
    d->items = ni;
    u32 *nk = realloc(d->keys, sizeof *nk * nc);
    if (!nk) return -1;
    d->keys = nk;
    d->cap = nc;
    return 0;
}
static u32 ae_count(const ae_node *d) { return d && d->kind != AEK_SCALAR ? d->count : 0; }
/* lista: index 0 = no fim; 1..n substitui; n+1 = no fim. Toma posse de item. */
static int ae_list_put(ae_node *d, u32 index, ae_node *item) {
    if (!d || d->kind != AEK_LIST || !item) { ae_free(item); return -1; }
    if (index >= 1 && index <= d->count) { ae_free(d->items[index - 1]); d->items[index - 1] = item; return 0; }
    if (ae_grow(d)) { ae_free(item); return -1; }
    d->keys[d->count] = 0;
    d->items[d->count++] = item;
    return 0;
}
static int ae_list_insert(ae_node *d, u32 index, ae_node *item) {   /* insere antes de index (1..n+1) */
    if (!d || d->kind != AEK_LIST || !item || index < 1 || index > d->count + 1) { ae_free(item); return -1; }
    if (ae_grow(d)) { ae_free(item); return -1; }
    for (u32 i = d->count; i >= index; i--) { d->items[i] = d->items[i - 1]; d->keys[i] = d->keys[i - 1]; }
    d->items[index - 1] = item;
    d->keys[index - 1] = 0;
    d->count++;
    return 0;
}
static ae_node *ae_nth(const ae_node *d, u32 index) { return d && d->kind != AEK_SCALAR && index >= 1 && index <= d->count ? d->items[index - 1] : 0; }
static int ae_delete_nth(ae_node *d, u32 index) {
    if (!d || d->kind == AEK_SCALAR || index < 1 || index > d->count) return -1;
    ae_free(d->items[index - 1]);
    for (u32 i = index - 1; i + 1 < d->count; i++) { d->items[i] = d->items[i + 1]; d->keys[i] = d->keys[i + 1]; }
    d->count--;
    return 0;
}
/* registro ou parâmetros do evento */
static int ae_key_index(const ae_node *d, u32 key) {
    if (!d || (d->kind != AEK_RECORD && d->kind != AEK_EVENT)) return -1;
    for (u32 i = 0; i < d->count; i++) if (d->keys[i] == key) return (int)i;
    return -1;
}
static ae_node *ae_get_key(const ae_node *d, u32 key) { int i = ae_key_index(d, key); return i < 0 ? 0 : d->items[i]; }
static int ae_put_key(ae_node *d, u32 key, ae_node *item) {
    if (!d || (d->kind != AEK_RECORD && d->kind != AEK_EVENT) || !item) { ae_free(item); return -1; }
    int i = ae_key_index(d, key);
    if (i >= 0) { ae_free(d->items[i]); d->items[i] = item; return 0; }
    if (ae_grow(d)) { ae_free(item); return -1; }
    d->keys[d->count] = key;
    d->items[d->count++] = item;
    return 0;
}
static int ae_del_key(ae_node *d, u32 key) { int i = ae_key_index(d, key); return i < 0 ? -1 : ae_delete_nth(d, (u32)i + 1); }
static ae_node *ae_get_attr(const ae_node *d, u32 key) {
    if (!d || d->kind != AEK_EVENT) return 0;
    for (u32 i = 0; i < d->nattrs; i++) if (d->akeys[i] == key) return d->attrs[i];
    return 0;
}
static int ae_put_attr(ae_node *d, u32 key, ae_node *item) {
    if (!d || d->kind != AEK_EVENT || !item) { ae_free(item); return -1; }
    for (u32 i = 0; i < d->nattrs; i++) if (d->akeys[i] == key) { ae_free(d->attrs[i]); d->attrs[i] = item; return 0; }
    if (d->nattrs == d->capattrs) {
        u32 nc = d->capattrs ? d->capattrs * 2 : 4;
        ae_node **na = realloc(d->attrs, sizeof *na * nc);
        u32 *nk = na ? realloc(d->akeys, sizeof *nk * nc) : 0;
        if (!na || !nk) { if (na) d->attrs = na; ae_free(item); return -1; }
        d->attrs = na; d->akeys = nk; d->capattrs = nc;
    }
    d->akeys[d->nattrs] = key;
    d->attrs[d->nattrs++] = item;
    return 0;
}

/* ---------------------------------------------------------------- coerções */
static int ae_is_text(u32 t) { return t == AE_UTF8 || t == AE_TEXT || t == AE_UTXT; }
/* texto em UTF-8 (malloc, com NUL) de um descritor de texto */
static char *ae_utf8_of(const ae_node *d) {
    if (!d || !ae_is_text(d->type)) return 0;
    if (d->type != AE_UTXT) {
        char *s = malloc(d->n + 1);
        if (!s) return 0;
        if (d->n) memcpy(s, d->data, d->n);
        s[d->n] = 0;
        return s;
    }
    u32 units = d->n / 2, o = 0;
    char *s = malloc(units * 3 + 1);
    if (!s) return 0;
    const u8 *b = d->data;
    for (u32 i = 0; i < units; i++) {
        u32 c = (u32)b[2 * i] | (u32)b[2 * i + 1] << 8;
        if (c >= 0xd800 && c < 0xdc00 && i + 1 < units) {
            u32 lo = (u32)b[2 * i + 2] | (u32)b[2 * i + 3] << 8;
            c = 0x10000 + ((c - 0xd800) << 10) + (lo - 0xdc00);
            i++;                                      /* 2 unidades viram 4 bytes: cabe em units * 3 */
        }
        if (c < 0x80) s[o++] = (char)c;
        else if (c < 0x800) { s[o++] = (char)(0xc0 | c >> 6); s[o++] = (char)(0x80 | (c & 63)); }
        else if (c < 0x10000) { s[o++] = (char)(0xe0 | c >> 12); s[o++] = (char)(0x80 | ((c >> 6) & 63)); s[o++] = (char)(0x80 | (c & 63)); }
        else { s[o++] = (char)(0xf0 | c >> 18); s[o++] = (char)(0x80 | ((c >> 12) & 63)); s[o++] = (char)(0x80 | ((c >> 6) & 63)); s[o++] = (char)(0x80 | (c & 63)); }
    }
    s[o] = 0;
    return s;
}
static ae_node *ae_utxt_of(const char *s) {
    usize l = strlen(s), o = 0;
    u8 *b = malloc(l * 4 + 2);
    if (!b) return 0;
    for (usize i = 0; i < l;) {
        u32 c = (u8)s[i];
        int extra = c >= 0xf0 ? 3 : c >= 0xe0 ? 2 : c >= 0xc0 ? 1 : 0;
        c &= extra == 3 ? 0x07 : extra == 2 ? 0x0f : extra == 1 ? 0x1f : 0x7f;
        i++;
        while (extra-- && i < l) c = c << 6 | ((u8)s[i++] & 0x3f);
        if (c >= 0x10000) { c -= 0x10000; u32 hi = 0xd800 + (c >> 10); b[o++] = (u8)hi; b[o++] = (u8)(hi >> 8); c = 0xdc00 + (c & 0x3ff); }
        b[o++] = (u8)c; b[o++] = (u8)(c >> 8);
    }
    ae_node *d = ae_scalar(AE_UTXT, b, (u32)o);
    free(b);
    return d;
}
/* número de um descritor numérico ou booleano; 0 se não for número */
static int ae_num_of(const ae_node *d, double *out) {
    if (!d || d->kind != AEK_SCALAR) return 0;
    switch (d->type) {
    case AE_SHOR: if (d->n >= 2) { i16 v; memcpy(&v, d->data, 2); *out = v; return 1; } return 0;
    case AE_LONG: case AE_ENUM: if (d->n >= 4) { i32 v; memcpy(&v, d->data, 4); *out = v; return 1; } return 0;
    case AE_MAGN: if (d->n >= 4) { u32 v; memcpy(&v, d->data, 4); *out = v; return 1; } return 0;
    case AE_COMP: if (d->n >= 8) { i64 v; memcpy(&v, d->data, 8); *out = (double)v; return 1; } return 0;
    case AE_SING: if (d->n >= 4) { float v; memcpy(&v, d->data, 4); *out = v; return 1; } return 0;
    case AE_DOUB: if (d->n >= 8) { double v; memcpy(&v, d->data, 8); *out = v; return 1; } return 0;
    case AE_BOOL: if (d->n >= 1) { *out = d->data[0] ? 1 : 0; return 1; } return 0;
    case AE_TRUE: *out = 1; return 1;
    case AE_FALS: *out = 0; return 1;
    default: return 0;
    }
}
static int ae_parse_num(const char *s, double *out) {
    while (*s == ' ' || *s == '\t') s++;
    const char *p = s;
    int neg = 0, any = 0;
    double v = 0, f = 0.1;
    if (*p == '-' || *p == '+') { neg = *p == '-'; p++; }
    while (*p >= '0' && *p <= '9') { v = v * 10 + (*p - '0'); p++; any = 1; }
    if (*p == '.' || *p == ',') { p++; while (*p >= '0' && *p <= '9') { v += f * (*p - '0'); f /= 10; p++; any = 1; } }
    if (*p == 'e' || *p == 'E') {
        p++;
        int en = 0, eneg = 0;
        if (*p == '-' || *p == '+') { eneg = *p == '-'; p++; }
        while (*p >= '0' && *p <= '9') en = en * 10 + (*p++ - '0');
        while (en--) v = eneg ? v / 10 : v * 10;
    }
    while (*p == ' ') p++;
    if (!any || *p) return 0;
    *out = neg ? -v : v;
    return 1;
}
static void ae_fmt_num(double v, char *b, usize cap) {
    if (v == (double)(i64)v && v > -1e15 && v < 1e15) snprintf(b, cap, "%lld", (long long)v);
    else snprintf(b, cap, "%.15g", v);
}
/* nova cópia coagida para `to`, ou NULL (errAECoercionFail) */
static ae_node *ae_coerce(const ae_node *d, u32 to) {
    if (!d) return 0;
    if (to == d->type || to == AE_WILD) return ae_copy(d);
    if (d->kind == AEK_LIST && to != AE_LIST) {             /* lista de um item vira o item */
        if (d->count == 1) return ae_coerce(d->items[0], to);
        return 0;
    }
    if (to == AE_LIST) { ae_node *l = ae_list(); ae_list_put(l, 0, ae_copy(d)); return l; }
    if (d->kind != AEK_SCALAR) return 0;
    double v;
    if (ae_is_text(to)) {
        char *s = 0, buf[64];
        if (ae_is_text(d->type)) s = ae_utf8_of(d);
        else if (d->type == AE_FURL || d->type == AE_BUND) { s = malloc(d->n + 1); if (s) { memcpy(s, d->data, d->n); s[d->n] = 0; } }
        else if (d->type == AE_TRUE || d->type == AE_FALS || d->type == AE_BOOL) {
            ae_num_of(d, &v);
            snprintf(buf, sizeof buf, "%s", v ? "true" : "false");
            s = malloc(strlen(buf) + 1);
            if (s) memcpy(s, buf, strlen(buf) + 1);
        } else if (ae_num_of(d, &v)) {
            ae_fmt_num(v, buf, sizeof buf);
            s = malloc(strlen(buf) + 1);
            if (s) memcpy(s, buf, strlen(buf) + 1);
        } else if ((d->type == AE_TYPE || d->type == AE_ENUM) && d->n == 4) {
            u32 c;
            memcpy(&c, d->data, 4);
            snprintf(buf, sizeof buf, "%c%c%c%c", (char)(c >> 24), (char)(c >> 16), (char)(c >> 8), (char)c);
            s = malloc(5);
            if (s) memcpy(s, buf, 5);
        }
        if (!s) return 0;
        ae_node *r = to == AE_UTXT ? ae_utxt_of(s) : ae_scalar(to, s, (u32)strlen(s));
        free(s);
        return r;
    }
    int have = ae_num_of(d, &v);
    if (!have && ae_is_text(d->type)) {
        char *s = ae_utf8_of(d);
        if (s && (to == AE_BOOL || to == AE_TRUE || to == AE_FALS)) {
            if (!strcmp(s, "true") || !strcmp(s, "yes")) { v = 1; have = 1; }
            else if (!strcmp(s, "false") || !strcmp(s, "no")) { v = 0; have = 1; }
        }
        if (s && !have) have = ae_parse_num(s, &v);
        free(s);
    }
    if (!have) return 0;
    switch (to) {
    case AE_SHOR: { if (v != (double)(i16)v) return 0; i16 x = (i16)v; return ae_scalar(to, &x, 2); }
    case AE_LONG: { if (v != (double)(i32)v) return 0; i32 x = (i32)v; return ae_scalar(to, &x, 4); }
    case AE_MAGN: { if (v < 0 || v != (double)(u32)v) return 0; u32 x = (u32)v; return ae_scalar(to, &x, 4); }
    case AE_COMP: { if (v != (double)(i64)v) return 0; i64 x = (i64)v; return ae_scalar(to, &x, 8); }
    case AE_SING: { float x = (float)v; return ae_scalar(to, &x, 4); }
    case AE_DOUB: return ae_scalar(to, &v, 8);
    case AE_BOOL: return ae_bool(v != 0);
    case AE_TRUE: case AE_FALS: return ae_scalar(v != 0 ? AE_TRUE : AE_FALS, 0, 0);
    default: return 0;
    }
}

/* ---------------------------------------------------------------- fio (entre processos do mOS) */
typedef struct { u8 *b; usize n, cap; int err; } ae_buf;
static void ab_put(ae_buf *s, const void *d, usize n) {
    if (s->err) return;
    if (s->n + n > s->cap) {
        usize nc = s->cap ? s->cap * 2 : 256;
        while (nc < s->n + n) nc *= 2;
        u8 *nb = realloc(s->b, nc);
        if (!nb) { s->err = 1; return; }
        s->b = nb; s->cap = nc;
    }
    memcpy(s->b + s->n, d, n);
    s->n += n;
}
static void ab_u32(ae_buf *s, u32 v) { ab_put(s, &v, 4); }
static void ae_write(ae_buf *s, const ae_node *d) {
    if (!d) { ab_u32(s, 0); return; }
    ab_u32(s, d->type);
    ab_u32(s, (u32)d->kind);
    ab_u32(s, d->n);
    if (d->n) ab_put(s, d->data, d->n);
    ab_u32(s, d->count);
    for (u32 i = 0; i < d->count; i++) { ab_u32(s, d->keys ? d->keys[i] : 0); ae_write(s, d->items[i]); }
    if (d->kind == AEK_EVENT) {
        ab_u32(s, d->ev_class); ab_u32(s, d->ev_id); ab_u32(s, (u32)d->return_id); ab_u32(s, (u32)d->transaction_id);
        ae_write(s, d->target);
        ab_u32(s, d->nattrs);
        for (u32 i = 0; i < d->nattrs; i++) { ab_u32(s, d->akeys[i]); ae_write(s, d->attrs[i]); }
    }
}
static int ae_serialize(const ae_node *d, u8 **out, usize *n) {
    ae_buf s = {0};
    ab_u32(&s, AE_FCC('m', 'O', 'S', 'E'));
    ae_write(&s, d);
    if (s.err) { free(s.b); return -1; }
    *out = s.b;
    *n = s.n;
    return 0;
}
typedef struct { const u8 *p, *end; int depth; } ae_rd;
static int ar_u32(ae_rd *r, u32 *v) { if (r->end - r->p < 4) return -1; memcpy(v, r->p, 4); r->p += 4; return 0; }
static ae_node *ae_read(ae_rd *r, int *ok) {
    u32 type, kind, n, count;
    if (ar_u32(r, &type)) { *ok = 0; return 0; }
    if (!type) return 0;                                 /* nulo (alvo ausente) */
    if (++r->depth > 64 || ar_u32(r, &kind) || kind > AEK_EVENT || ar_u32(r, &n) || (usize)(r->end - r->p) < n) { *ok = 0; return 0; }
    ae_node *d = ae_scalar(type, r->p, n);
    if (!d) { *ok = 0; return 0; }
    d->kind = (int)kind;
    r->p += n;
    if (ar_u32(r, &count) || count > 1u << 20) { *ok = 0; ae_free(d); return 0; }
    for (u32 i = 0; i < count && *ok; i++) {
        u32 key;
        if (ar_u32(r, &key)) { *ok = 0; break; }
        ae_node *it = ae_read(r, ok);
        if (!*ok || !it) { ae_free(it); *ok = 0; break; }
        if (ae_grow(d)) { ae_free(it); *ok = 0; break; }
        d->keys[d->count] = key;
        d->items[d->count++] = it;
    }
    if (*ok && kind == AEK_EVENT) {
        u32 rid = 0, tid = 0, na;
        if (ar_u32(r, &d->ev_class) || ar_u32(r, &d->ev_id) || ar_u32(r, &rid) || ar_u32(r, &tid)) *ok = 0;
        d->return_id = (i32)rid; d->transaction_id = (i32)tid;
        if (*ok) d->target = ae_read(r, ok);
        if (*ok && (ar_u32(r, &na) || na > 4096)) *ok = 0;
        for (u32 i = 0; *ok && i < na; i++) {
            u32 key;
            if (ar_u32(r, &key)) { *ok = 0; break; }
            ae_node *it = ae_read(r, ok);
            if (!*ok || !it) { ae_free(it); *ok = 0; break; }
            ae_put_attr(d, key, it);
        }
    }
    r->depth--;
    if (!*ok) { ae_free(d); return 0; }
    return d;
}
static ae_node *ae_parse(const u8 *b, usize n) {
    ae_rd r = {b, b + n, 0};
    u32 magic;
    int ok = 1;
    if (ar_u32(&r, &magic) || magic != AE_FCC('m', 'O', 'S', 'E')) return 0;
    ae_node *d = ae_read(&r, &ok);
    if (!ok || r.p != r.end) { ae_free(d); return 0; }
    return d;
}
