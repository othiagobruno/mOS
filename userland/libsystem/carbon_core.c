/* mOS libSystem — núcleo do Carbon (ver carbon_core.h). */
#include "carbon_core.h"

static void mcpy(void *d, const void *s, cc_size n) { unsigned char *a = d; const unsigned char *b = s; while (n--) *a++ = *b++; }

/* ================================================================ EventRef */
cc_event *cc_event_new(cc_u32 cls, cc_u32 kind, double time, unsigned attrs) {
    cc_event *e = cc_alloc(sizeof *e);
    if (!e) return 0;
    e->refs = 1; e->cls = cls; e->kind = kind; e->time = time; e->attrs = attrs; e->params = 0;
    return e;
}
static void free_params(cc_param *p) { while (p) { cc_param *n = p->next; cc_free(p); p = n; } }
void cc_event_release(cc_event *e) { if (!e || --e->refs > 0) return; free_params(e->params); cc_free(e); }
int cc_event_set(cc_event *e, cc_u32 name, cc_u32 type, cc_size size, const void *data) {
    cc_param *p = cc_alloc(sizeof *p + size);
    if (!p) return -1;
    p->name = name; p->type = type; p->size = size;
    if (size && data) mcpy(p->data, data, size);
    /* substitui o anterior do mesmo nome */
    cc_param **pp = &e->params;
    while (*pp) { if ((*pp)->name == name) { cc_param *old = *pp; *pp = old->next; cc_free(old); break; } pp = &(*pp)->next; }
    p->next = e->params;
    e->params = p;
    return 0;
}
cc_event *cc_event_copy(const cc_event *e) {
    cc_event *c = cc_event_new(e->cls, e->kind, e->time, e->attrs);
    if (!c) return 0;
    /* mantém a ordem: copia do fim para o começo */
    const cc_param *list[256]; int n = 0;
    for (const cc_param *p = e->params; p && n < 256; p = p->next) list[n++] = p;
    while (n--) if (cc_event_set(c, list[n]->name, list[n]->type, list[n]->size, list[n]->data)) { cc_event_release(c); return 0; }
    return c;
}
int cc_event_get(const cc_event *e, cc_u32 name, cc_u32 want, cc_u32 *out_type, cc_size cap, cc_size *out_size, void *out) {
    for (const cc_param *p = e->params; p; p = p->next) {
        if (p->name != name) continue;
        if (want != 0x2A2A2A2A /* typeWildCard '****' */ && want != p->type) return -1700;   /* errAECoercionFail */
        if (out_type) *out_type = p->type;
        if (out_size) *out_size = p->size;
        if (out) mcpy(out, p->data, p->size < cap ? p->size : cap);
        return 0;
    }
    return -9870;   /* eventParameterNotFoundErr */
}

/* ================================================================ tipos dos tratadores */
int cc_specs_add(cc_specs *s, const cc_spec *add, int n) {
    for (int i = 0; i < n; i++) {
        if (cc_specs_match(s, add[i].cls, add[i].kind)) continue;
        if (s->n == s->cap) {
            int nc = s->cap ? s->cap * 2 : 8;
            cc_spec *v = cc_alloc(sizeof *v * (cc_size)nc);
            if (!v) return -1;
            for (int k = 0; k < s->n; k++) v[k] = s->v[k];
            cc_free(s->v); s->v = v; s->cap = nc;
        }
        s->v[s->n++] = add[i];
    }
    return 0;
}
int cc_specs_remove(cc_specs *s, const cc_spec *rm, int n) {
    int rc = 0;
    for (int i = 0; i < n; i++) {
        int k = 0;
        while (k < s->n && !(s->v[k].cls == rm[i].cls && s->v[k].kind == rm[i].kind)) k++;
        if (k == s->n) { rc = -50; continue; }
        s->v[k] = s->v[--s->n];
    }
    return rc;
}
int cc_specs_match(const cc_specs *s, cc_u32 cls, cc_u32 kind) {
    for (int i = 0; i < s->n; i++) if (s->v[i].cls == cls && s->v[i].kind == kind) return 1;
    return 0;
}
void cc_specs_free(cc_specs *s) { cc_free(s->v); s->v = 0; s->n = s->cap = 0; }

/* ================================================================ HIShape */
static int push(cc_shape *s, cc_rect r) {
    if (r.w <= 0 || r.h <= 0) return 0;
    if (s->n == s->cap) {
        int nc = s->cap ? s->cap * 2 : 8;
        cc_rect *v = cc_alloc(sizeof *v * (cc_size)nc);
        if (!v) return -1;
        for (int k = 0; k < s->n; k++) v[k] = s->r[k];
        cc_free(s->r); s->r = v; s->cap = nc;
    }
    s->r[s->n++] = r;
    return 0;
}
void cc_shape_clear(cc_shape *s) { s->n = 0; }
int cc_shape_set_rect(cc_shape *s, cc_rect r) { s->n = 0; return push(s, r); }
int cc_shape_copy(cc_shape *d, const cc_shape *src) { d->n = 0; for (int i = 0; i < src->n; i++) if (push(d, src->r[i])) return -1; return 0; }
static int meets(cc_rect a, cc_rect b) { return a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h; }
static cc_rect isect(cc_rect a, cc_rect b) {
    double x0 = a.x > b.x ? a.x : b.x, y0 = a.y > b.y ? a.y : b.y;
    double x1 = a.x + a.w < b.x + b.w ? a.x + a.w : b.x + b.w, y1 = a.y + a.h < b.y + b.h ? a.y + a.h : b.y + b.h;
    cc_rect r = {x0, y0, x1 - x0, y1 - y0};
    return r;
}
/* a − b em até 4 pedaços disjuntos */
static int sub_rect(cc_shape *out, cc_rect a, cc_rect b) {
    if (!meets(a, b)) return push(out, a);
    cc_rect i = isect(a, b);
    cc_rect top = {a.x, a.y, a.w, i.y - a.y};
    cc_rect bot = {a.x, i.y + i.h, a.w, a.y + a.h - (i.y + i.h)};
    cc_rect left = {a.x, i.y, i.x - a.x, i.h};
    cc_rect right = {i.x + i.w, i.y, a.x + a.w - (i.x + i.w), i.h};
    return push(out, top) | push(out, bot) | push(out, left) | push(out, right);
}
int cc_shape_diff(cc_shape *out, const cc_shape *a, const cc_shape *b) {
    cc_shape cur = {0, 0, 0}, nxt = {0, 0, 0};
    if (cc_shape_copy(&cur, a)) return -1;
    for (int j = 0; j < b->n; j++) {
        nxt.n = 0;
        for (int i = 0; i < cur.n; i++) if (sub_rect(&nxt, cur.r[i], b->r[j])) { cc_free(cur.r); cc_free(nxt.r); return -1; }
        cc_shape t = cur; cur = nxt; nxt = t;
    }
    int rc = cc_shape_copy(out, &cur);
    cc_free(cur.r); cc_free(nxt.r);
    return rc;
}
int cc_shape_union(cc_shape *out, const cc_shape *a, const cc_shape *b) {
    cc_shape rest = {0, 0, 0};
    if (cc_shape_diff(&rest, b, a)) return -1;
    cc_shape tmp = {0, 0, 0};
    int rc = cc_shape_copy(&tmp, a);
    for (int i = 0; !rc && i < rest.n; i++) rc = push(&tmp, rest.r[i]);
    if (!rc) rc = cc_shape_copy(out, &tmp);
    cc_free(rest.r); cc_free(tmp.r);
    return rc;
}
int cc_shape_intersect(cc_shape *out, const cc_shape *a, const cc_shape *b) {
    cc_shape tmp = {0, 0, 0};
    int rc = 0;
    for (int i = 0; !rc && i < a->n; i++)
        for (int j = 0; !rc && j < b->n; j++) if (meets(a->r[i], b->r[j])) rc = push(&tmp, isect(a->r[i], b->r[j]));
    if (!rc) rc = cc_shape_copy(out, &tmp);
    cc_free(tmp.r);
    return rc;
}
int cc_shape_xor(cc_shape *out, const cc_shape *a, const cc_shape *b) {
    cc_shape p = {0, 0, 0}, q = {0, 0, 0};
    int rc = cc_shape_diff(&p, a, b) | cc_shape_diff(&q, b, a);
    for (int i = 0; !rc && i < q.n; i++) rc = push(&p, q.r[i]);
    if (!rc) rc = cc_shape_copy(out, &p);
    cc_free(p.r); cc_free(q.r);
    return rc;
}
int cc_shape_empty(const cc_shape *s) { return s->n == 0; }
cc_rect cc_shape_bounds(const cc_shape *s) {
    cc_rect b = {0, 0, 0, 0};
    if (!s->n) return b;
    double x0 = s->r[0].x, y0 = s->r[0].y, x1 = x0 + s->r[0].w, y1 = y0 + s->r[0].h;
    for (int i = 1; i < s->n; i++) {
        cc_rect r = s->r[i];
        if (r.x < x0) x0 = r.x;
        if (r.y < y0) y0 = r.y;
        if (r.x + r.w > x1) x1 = r.x + r.w;
        if (r.y + r.h > y1) y1 = r.y + r.h;
    }
    b.x = x0; b.y = y0; b.w = x1 - x0; b.h = y1 - y0;
    return b;
}
double cc_shape_area(const cc_shape *s) { double a = 0; for (int i = 0; i < s->n; i++) a += s->r[i].w * s->r[i].h; return a; }
int cc_shape_rectangular(const cc_shape *s) {
    if (s->n <= 1) return 1;
    cc_rect b = cc_shape_bounds(s);
    double d = b.w * b.h - cc_shape_area(s);
    return d < 1e-9 && d > -1e-9;   /* os pedaços são disjuntos: cobrem os limites só se a área bate */
}
int cc_shape_contains(const cc_shape *s, double x, double y) {
    for (int i = 0; i < s->n; i++) { cc_rect r = s->r[i]; if (x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h) return 1; }
    return 0;
}
int cc_shape_intersects_rect(const cc_shape *s, cc_rect r) { for (int i = 0; i < s->n; i++) if (meets(s->r[i], r)) return 1; return 0; }
void cc_shape_offset(cc_shape *s, double dx, double dy) { for (int i = 0; i < s->n; i++) { s->r[i].x += dx; s->r[i].y += dy; } }
/* encolhe (positivo) ou cresce (negativo): a região menos uma faixa de dx/dy em volta do complemento (erosão exata
 * para retângulos alinhados aos eixos) */
int cc_shape_inset(cc_shape *s, double dx, double dy) {
    if (!s->n) return 0;
    if (dx <= 0 && dy <= 0) {   /* crescer: cada pedaço cresce e as sobreposições se juntam pela união */
        cc_shape acc = {0, 0, 0}, one = {0, 0, 0};
        int rc = 0;
        for (int i = 0; !rc && i < s->n; i++) {
            cc_rect r = s->r[i];
            r.x += dx; r.y += dy; r.w -= 2 * dx; r.h -= 2 * dy;
            cc_shape_set_rect(&one, r);
            rc = cc_shape_union(&acc, &acc, &one);
        }
        if (!rc) rc = cc_shape_copy(s, &acc);
        cc_free(acc.r); cc_free(one.r);
        return rc;
    }
    /* encolher: tira de cada pedaço a vizinhança do que está fora da região (o complemento dentro dos limites, com borda) */
    cc_rect b = cc_shape_bounds(s);
    cc_rect big = {b.x - dx - 1, b.y - dy - 1, b.w + 2 * dx + 2, b.h + 2 * dy + 2};
    cc_shape outside = {0, 0, 0}, frame = {0, 0, 0}, grown = {0, 0, 0}, one = {0, 0, 0};
    int rc = cc_shape_set_rect(&frame, big) | cc_shape_diff(&outside, &frame, s);
    for (int i = 0; !rc && i < outside.n; i++) {
        cc_rect r = outside.r[i];
        r.x -= dx; r.y -= dy; r.w += 2 * dx; r.h += 2 * dy;
        cc_shape_set_rect(&one, r);
        rc = cc_shape_union(&grown, &grown, &one);
    }
    if (!rc) rc = cc_shape_diff(s, s, &grown);
    cc_free(outside.r); cc_free(frame.r); cc_free(grown.r); cc_free(one.r);
    return rc;
}

/* ================================================================ trilhas */
cc_thumb cc_track_thumb(int kind, double length, double minv, double maxv, double value, double viewsize) {
    cc_thumb t = {0, 0};
    double range = maxv - minv;
    if (length <= 0) return t;
    if (value < minv) value = minv;
    if (value > maxv) value = maxv;
    double frac = range > 0 ? (value - minv) / range : 0;
    if (kind == 2) { t.pos = 0; t.len = length * frac; return t; }
    if (kind == 1) t.len = length < 15 ? length : 15;
    else {
        t.len = range + viewsize > 0 ? length * viewsize / (range + viewsize) : length;
        if (t.len < 16) t.len = length < 16 ? length : 16;
        if (t.len > length) t.len = length;
    }
    t.pos = (length - t.len) * frac;
    return t;
}
int cc_track_hit(int kind, double length, double minv, double maxv, double value, double viewsize, double at) {
    if (at < 0 || at > length) return 0;
    cc_thumb t = cc_track_thumb(kind, length, minv, maxv, value, viewsize);
    if (kind == 2) return 0;
    if (at >= t.pos && at <= t.pos + t.len) return 129;
    return at < t.pos ? 22 : 23;
}
