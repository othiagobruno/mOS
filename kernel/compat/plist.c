/*
 * mOS — property lists como árvore (kernel/include/plist.h). XML (o formato dos Info.plist feitos à mão e
 * das preferências que o mOS grava) e binário bplist00 (o que o Xcode gera). Os nós e as strings vêm de
 * blocos encadeados presos à raiz, para pl_free soltar tudo de uma vez.
 */
#include "plist.h"

typedef struct blk { struct blk *next; usize used, cap; u8 mem[]; } blk_t;
typedef struct { pl_t root; blk_t *blocks; } doc_t;    /* a raiz é o primeiro campo: pl_free acha o doc */

static void *doc_alloc(doc_t *d, usize n) {
    n = (n + 7) & ~(usize)7;
    blk_t *b = d->blocks;
    if (!b || b->used + n > b->cap) {
        usize cap = n > 16384 ? n : 16384;
        blk_t *nb = kmalloc(sizeof *nb + cap);
        if (!nb) return NULL;
        nb->next = d->blocks; nb->used = 0; nb->cap = cap;
        d->blocks = b = nb;
    }
    void *p = b->mem + b->used;
    b->used += n;
    memset(p, 0, n);
    return p;
}
static char *doc_strn(doc_t *d, const char *s, usize n) {
    char *o = doc_alloc(d, n + 1);
    if (!o) return NULL;
    memcpy(o, s, n);
    o[n] = 0;
    return o;
}
static pl_t *node_new(doc_t *d, u8 type) { pl_t *n = doc_alloc(d, sizeof *n); if (n) n->type = type; return n; }
static void add_child(pl_t *parent, pl_t **tail, pl_t *c) {
    if (*tail) (*tail)->next = c; else parent->child = c;
    *tail = c;
    parent->count++;
}

void pl_free(pl_t *root) {
    if (!root) return;
    doc_t *d = (doc_t *)root;
    for (blk_t *b = d->blocks, *nx; b; b = nx) { nx = b->next; kfree(b); }
    kfree(d);
}

pl_t *pl_get(const pl_t *dict, const char *key) {
    if (!dict || dict->type != PL_DICT) return NULL;
    for (pl_t *c = dict->child; c; c = c->next) if (c->key && !strcmp(c->key, key)) return c;
    return NULL;
}
const char *pl_str(const pl_t *dict, const char *key) {
    pl_t *v = pl_get(dict, key);
    return v && v->type == PL_STRING ? v->str : NULL;
}
pl_t *pl_at(const pl_t *arr, int i) {
    if (!arr || (arr->type != PL_ARRAY && arr->type != PL_DICT)) return NULL;
    pl_t *c = arr->child;
    while (c && i-- > 0) c = c->next;
    return c;
}
void pl_each_str(const pl_t *v, void (*fn)(const char *s, void *ctx), void *ctx) {
    if (!v) return;
    if (v->type == PL_STRING) { if (v->str && v->str[0]) fn(v->str, ctx); return; }
    if (v->type != PL_ARRAY) return;
    for (pl_t *c = v->child; c; c = c->next) if (c->type == PL_STRING && c->str && c->str[0]) fn(c->str, ctx);
}

/* ------------------------------------------------------------ XML */
typedef struct { const char *p, *end; doc_t *d; int depth; } xp_t;

static void xs_skip(xp_t *x) {
    for (;;) {
        while (x->p < x->end && (*x->p == ' ' || *x->p == '\t' || *x->p == '\r' || *x->p == '\n')) x->p++;
        if (x->end - x->p >= 4 && !memcmp(x->p, "<!--", 4)) {
            const char *q = x->p + 4;
            while (q + 3 <= x->end && memcmp(q, "-->", 3)) q++;
            x->p = q + 3 <= x->end ? q + 3 : x->end;
            continue;
        }
        if (x->end - x->p >= 2 && (!memcmp(x->p, "<?", 2) || !memcmp(x->p, "<!", 2))) {
            while (x->p < x->end && *x->p != '>') x->p++;
            if (x->p < x->end) x->p++;
            continue;
        }
        return;
    }
}

/* lê "<nome ...>" ou "<nome/>" ou "</nome>": devolve o nome em out; *close = fechamento, *empty = auto-fechada */
static bool xs_tag(xp_t *x, char *out, usize cap, bool *close, bool *empty) {
    xs_skip(x);
    if (x->p >= x->end || *x->p != '<') return false;
    x->p++;
    *close = x->p < x->end && *x->p == '/';
    if (*close) x->p++;
    usize o = 0;
    while (x->p < x->end && *x->p != '>' && *x->p != '/' && *x->p != ' ' && *x->p != '\t' && *x->p != '\n' && *x->p != '\r') {
        if (o + 1 < cap) out[o++] = *x->p;
        x->p++;
    }
    out[o] = 0;
    *empty = false;
    while (x->p < x->end && *x->p != '>') { if (*x->p == '/') *empty = true; x->p++; }
    if (x->p < x->end) x->p++;
    return true;
}

/* texto até "</...>", com as entidades do XML resolvidas */
static char *xs_text(xp_t *x, usize *len) {
    const char *s = x->p;
    while (x->p < x->end && *x->p != '<') x->p++;
    usize n = (usize)(x->p - s);
    char *o = doc_alloc(x->d, n + 1);
    if (!o) return NULL;
    usize k = 0;
    for (usize i = 0; i < n; i++) {
        if (s[i] == '&') {
            static const struct { const char *e; char c; } ent[] = {{"&amp;", '&'}, {"&lt;", '<'}, {"&gt;", '>'}, {"&quot;", '"'}, {"&apos;", '\''}};
            bool done = false;
            for (u32 e = 0; e < ARRAY_LEN(ent) && !done; e++) {
                usize el = strlen(ent[e].e);
                if (i + el <= n && !memcmp(s + i, ent[e].e, el)) { o[k++] = ent[e].c; i += el - 1; done = true; }
            }
            if (!done && i + 3 < n && s[i + 1] == '#') {
                u32 cp = 0;
                usize j = i + 2;
                bool hex = s[j] == 'x';
                if (hex) j++;
                for (; j < n && s[j] != ';'; j++) {
                    char c = s[j];
                    cp = hex ? cp * 16 + (u32)(c <= '9' ? c - '0' : (c | 32) - 'a' + 10) : cp * 10 + (u32)(c - '0');
                }
                if (j < n) {
                    char u[4];
                    int ul = 0;
                    if (cp < 0x80) u[ul++] = (char)cp;
                    else if (cp < 0x800) { u[ul++] = (char)(0xC0 | cp >> 6); u[ul++] = (char)(0x80 | (cp & 63)); }
                    else if (cp < 0x10000) { u[ul++] = (char)(0xE0 | cp >> 12); u[ul++] = (char)(0x80 | (cp >> 6 & 63)); u[ul++] = (char)(0x80 | (cp & 63)); }
                    else { u[ul++] = (char)(0xF0 | cp >> 18); u[ul++] = (char)(0x80 | (cp >> 12 & 63)); u[ul++] = (char)(0x80 | (cp >> 6 & 63)); u[ul++] = (char)(0x80 | (cp & 63)); }
                    if (k + (usize)ul <= n) { memcpy(o + k, u, (usize)ul); k += (usize)ul; }
                    i = j;
                    done = true;
                }
            }
            if (!done) o[k++] = s[i];
        } else o[k++] = s[i];
    }
    o[k] = 0;
    if (len) *len = k;
    return o;
}

static void xs_end(xp_t *x) { char t[32]; bool c, e; xs_tag(x, t, sizeof t, &c, &e); }

static pl_t *xs_value(xp_t *x, const char *tag, bool empty);

static pl_t *xs_container(xp_t *x, u8 type, bool empty) {
    pl_t *n = node_new(x->d, type), *tail = NULL;
    if (!n || empty) return n;
    if (++x->depth > 64) return n;
    const char *key = NULL;
    for (;;) {
        char t[32];
        bool close, emp;
        const char *save = x->p;
        if (!xs_tag(x, t, sizeof t, &close, &emp)) break;
        if (close) break;                              /* </dict> ou </array> */
        if (type == PL_DICT && !strcmp(t, "key")) {
            key = emp ? "" : xs_text(x, NULL);
            if (!emp) xs_end(x);
            continue;
        }
        pl_t *v = xs_value(x, t, emp);
        if (!v) { x->p = save; break; }
        if (type == PL_DICT) { if (!key) continue; v->key = key; key = NULL; }
        add_child(n, &tail, v);
    }
    x->depth--;
    return n;
}

static pl_t *xs_value(xp_t *x, const char *tag, bool empty) {
    if (!strcmp(tag, "dict")) return xs_container(x, PL_DICT, empty);
    if (!strcmp(tag, "array")) return xs_container(x, PL_ARRAY, empty);
    if (!strcmp(tag, "true") || !strcmp(tag, "false")) {
        pl_t *n = node_new(x->d, PL_BOOL);
        if (n) n->num = tag[0] == 't';
        if (!empty) xs_end(x);
        return n;
    }
    u8 type = !strcmp(tag, "string") ? PL_STRING : !strcmp(tag, "integer") ? PL_INT : !strcmp(tag, "real") ? PL_REAL :
              !strcmp(tag, "date") ? PL_DATE : !strcmp(tag, "data") ? PL_DATA : PL_NULL;
    pl_t *n = node_new(x->d, type == PL_NULL ? PL_STRING : type);
    if (!n) return NULL;
    char *txt = empty ? (char *)"" : xs_text(x, NULL);
    if (!empty) xs_end(x);
    if (n->type == PL_STRING) n->str = txt ? txt : "";
    else if (n->type == PL_INT && txt) {
        const char *p = txt;
        while (*p == ' ' || *p == '\n' || *p == '\t') p++;
        bool neg = *p == '-';
        if (neg || *p == '+') p++;
        i64 v = 0;
        while (*p >= '0' && *p <= '9') v = v * 10 + (*p++ - '0');
        n->num = neg ? -v : v;
    }
    return n;
}

static pl_t *parse_xml(doc_t *d, const u8 *data, usize n) {
    xp_t x = {(const char *)data, (const char *)data + n, d, 0};
    char t[32];
    bool close, empty;
    while (xs_tag(&x, t, sizeof t, &close, &empty)) {
        if (close) return NULL;
        if (!strcmp(t, "plist")) continue;
        return xs_value(&x, t, empty);
    }
    return NULL;
}

/* ------------------------------------------------------------ binário */
typedef struct { const u8 *b; usize n; u8 offsz, refsz; u64 nobj, tbl; doc_t *d; int depth; } bp_t;

static u64 be_uint(const u8 *p, u8 sz) { u64 v = 0; for (u8 i = 0; i < sz; i++) v = v << 8 | p[i]; return v; }

static bool bp_off(bp_t *bp, u64 idx, usize *off) {
    if (idx >= bp->nobj) return false;
    usize e = (usize)(bp->tbl + idx * bp->offsz);
    if (e + bp->offsz > bp->n) return false;
    *off = (usize)be_uint(bp->b + e, bp->offsz);
    return *off < bp->n;
}
/* contagem do marcador (nibble baixo; 0xF = inteiro a seguir); *p avança para os dados */
static bool bp_count(bp_t *bp, usize *p, u64 *cnt) {
    u8 m = bp->b[*p];
    *cnt = m & 0xF;
    (*p)++;
    if (*cnt != 0xF) return true;
    if (*p >= bp->n || (bp->b[*p] & 0xF0) != 0x10) return false;
    u8 sz = (u8)(1u << (bp->b[*p] & 0xF));
    if (*p + 1 + sz > bp->n || sz > 8) return false;
    *cnt = be_uint(bp->b + *p + 1, sz);
    *p += 1 + sz;
    return true;
}

static pl_t *bp_obj(bp_t *bp, u64 idx) {
    usize p;
    if (!bp_off(bp, idx, &p) || ++bp->depth > 64) { bp->depth--; return NULL; }
    u8 m = bp->b[p], hi = m >> 4;
    pl_t *n = NULL;
    if (hi == 0x0) {
        n = node_new(bp->d, (m == 8 || m == 9) ? PL_BOOL : PL_NULL);
        if (n) n->num = m == 9;
    } else if (hi == 0x1) {
        u8 sz = (u8)(1u << (m & 0xF));
        n = node_new(bp->d, PL_INT);
        if (n && sz <= 8 && p + 1 + sz <= bp->n) n->num = (i64)be_uint(bp->b + p + 1, sz);
    } else if (hi == 0x2 || hi == 0x3 || hi == 0x4) {
        n = node_new(bp->d, hi == 0x2 ? PL_REAL : hi == 0x3 ? PL_DATE : PL_DATA);
    } else if (hi == 0x5 || hi == 0x6) {
        u64 cnt;
        n = node_new(bp->d, PL_STRING);
        if (n) n->str = "";
        if (n && bp_count(bp, &p, &cnt)) {
            if (hi == 0x5 && p + cnt <= bp->n) n->str = doc_strn(bp->d, (const char *)bp->b + p, (usize)cnt);
            else if (hi == 0x6 && p + cnt * 2 <= bp->n && cnt < 65536) {
                char *o = doc_alloc(bp->d, (usize)cnt * 3 + 1);
                usize k = 0;
                for (u64 i = 0; o && i < cnt; i++) {
                    u32 c = (u32)bp->b[p + i * 2] << 8 | bp->b[p + i * 2 + 1];
                    if (c >= 0xD800 && c < 0xDC00 && i + 1 < cnt) {
                        u32 lo = (u32)bp->b[p + i * 2 + 2] << 8 | bp->b[p + i * 2 + 3];
                        c = 0x10000 + ((c - 0xD800) << 10) + (lo - 0xDC00);
                        i++;
                    }
                    if (c < 0x80) o[k++] = (char)c;
                    else if (c < 0x800) { o[k++] = (char)(0xC0 | c >> 6); o[k++] = (char)(0x80 | (c & 63)); }
                    else if (c < 0x10000) { o[k++] = (char)(0xE0 | c >> 12); o[k++] = (char)(0x80 | (c >> 6 & 63)); o[k++] = (char)(0x80 | (c & 63)); }
                    else { o[k++] = (char)(0xF0 | c >> 18); o[k++] = (char)(0x80 | (c >> 12 & 63)); o[k++] = (char)(0x80 | (c >> 6 & 63)); o[k++] = (char)(0x80 | (c & 63)); }
                }
                if (o) { o[k] = 0; n->str = o; }
            }
        }
    } else if (hi == 0xA || hi == 0xD) {
        u64 cnt;
        n = node_new(bp->d, hi == 0xA ? PL_ARRAY : PL_DICT);
        pl_t *tail = NULL;
        if (n && bp_count(bp, &p, &cnt) && cnt < 100000) {
            usize refs = (usize)(cnt * bp->refsz * (hi == 0xD ? 2 : 1));
            if (p + refs <= bp->n) {
                for (u64 i = 0; i < cnt; i++) {
                    u64 vref = be_uint(bp->b + p + (hi == 0xD ? cnt + i : i) * bp->refsz, bp->refsz);
                    pl_t *v = bp_obj(bp, vref);
                    if (!v) continue;
                    if (hi == 0xD) {
                        pl_t *k = bp_obj(bp, be_uint(bp->b + p + i * bp->refsz, bp->refsz));
                        if (!k || k->type != PL_STRING) continue;
                        v->key = k->str;
                    }
                    add_child(n, &tail, v);
                }
            }
        }
    } else {
        n = node_new(bp->d, PL_NULL);
    }
    bp->depth--;
    return n;
}

static pl_t *parse_bin(doc_t *d, const u8 *b, usize n) {
    if (n < 40) return NULL;
    const u8 *t = b + n - 32;
    bp_t bp = {b, n, t[6], t[7], be_uint(t + 8, 8), be_uint(t + 24, 8), d, 0};
    u64 top = be_uint(t + 16, 8);
    if (!bp.offsz || bp.offsz > 8 || !bp.refsz || bp.refsz > 8 || bp.tbl >= n) return NULL;
    return bp_obj(&bp, top);
}

pl_t *pl_parse(const u8 *data, usize n) {
    if (!data || !n) return NULL;
    doc_t *d = kcalloc(1, sizeof *d);
    if (!d) return NULL;
    pl_t *v = n >= 8 && !memcmp(data, "bplist00", 8) ? parse_bin(d, data, n) : parse_xml(d, data, n);
    if (!v) { for (blk_t *b = d->blocks, *nx; b; b = nx) { nx = b->next; kfree(b); } kfree(d); return NULL; }
    d->root = *v;                 /* copia o valor de topo para a raiz (os filhos continuam nos blocos) */
    d->root.next = NULL;
    d->root.key = NULL;
    return &d->root;
}
