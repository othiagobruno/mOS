/* mOS libSystem — leitor de NIBArchive (ver nibarchive.h) */
#include "nibarchive.h"

static int ns_eq(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return *a == *b; }

static int rd_var(const unsigned char *b, unsigned long size, unsigned long *o, unsigned *out) {
    unsigned v = 0, shift = 0;
    while (*o < size && shift < 35) {
        unsigned char c = b[(*o)++];
        v |= (unsigned)(c & 0x7F) << shift;
        if (c & 0x80) { *out = v; return 1; }
        shift += 7;
    }
    return 0;
}
static unsigned rd32(const unsigned char *b) { return b[0] | b[1] << 8 | b[2] << 16 | (unsigned)b[3] << 24; }

int nib_parse(nib_t *n, const unsigned char *b, unsigned long size, void *(*alloc)(unsigned long)) {
    static const char magic[] = "NIBArchive";
    if (size < 50) return 0;
    for (int i = 0; i < 10; i++) if (b[i] != (unsigned char)magic[i]) return 0;
    unsigned h[8];
    for (int i = 0; i < 8; i++) h[i] = rd32(b + 18 + i * 4);
    n->buf = b; n->size = size;
    n->nobj = h[0]; n->nkey = h[2]; n->nval = h[4]; n->ncls = h[6];
    if (n->nobj > 1000000 || n->nkey > 1000000 || n->nval > 4000000 || n->ncls > 100000) return 0;
    n->obj = alloc((n->nobj + 1) * sizeof(nib_obj_t));
    n->key = alloc((n->nkey + 1) * sizeof(char *));
    n->val = alloc((n->nval + 1) * sizeof(nib_val_t));
    n->cls = alloc((n->ncls + 1) * sizeof(char *));
    if (!n->obj || !n->key || !n->val || !n->cls) return 0;
    unsigned long o = h[1];
    for (unsigned i = 0; i < n->nobj; i++)
        if (!rd_var(b, size, &o, &n->obj[i].cls) || !rd_var(b, size, &o, &n->obj[i].first) || !rd_var(b, size, &o, &n->obj[i].count)) return 0;
    /* chaves: comprimento + bytes (sem terminador); copiamos com terminador */
    o = h[3];
    for (unsigned i = 0; i < n->nkey; i++) {
        unsigned l;
        if (!rd_var(b, size, &o, &l) || o + l > size) return 0;
        char *s = alloc(l + 1);
        if (!s) return 0;
        for (unsigned k = 0; k < l; k++) s[k] = (char)b[o + k];
        s[l] = 0;
        n->key[i] = s;
        o += l;
    }
    o = h[5];
    for (unsigned i = 0; i < n->nval; i++) {
        nib_val_t *v = &n->val[i];
        if (!rd_var(b, size, &o, &v->key) || o >= size) return 0;
        v->type = b[o++];
        v->i = 0; v->d = 0; v->data = 0; v->len = 0; v->obj = 0;
        switch (v->type) {
        case NV_I8: if (o + 1 > size) return 0; v->i = (signed char)b[o]; o += 1; break;
        case NV_I16: if (o + 2 > size) return 0; v->i = (short)(b[o] | b[o + 1] << 8); o += 2; break;
        case NV_I32: if (o + 4 > size) return 0; v->i = (int)rd32(b + o); o += 4; break;
        case NV_I64: if (o + 8 > size) return 0; v->i = (long long)((unsigned long long)rd32(b + o) | (unsigned long long)rd32(b + o + 4) << 32); o += 8; break;
        case NV_TRUE: v->i = 1; break;
        case NV_FALSE: v->i = 0; break;
        case NV_FLOAT: { if (o + 4 > size) return 0; union { unsigned u; float f; } x; x.u = rd32(b + o); v->d = x.f; o += 4; break; }
        case NV_DOUBLE: { if (o + 8 > size) return 0; union { unsigned long long u; double d; } x; x.u = (unsigned long long)rd32(b + o) | (unsigned long long)rd32(b + o + 4) << 32; v->d = x.d; o += 8; break; }
        case NV_DATA: { unsigned l; if (!rd_var(b, size, &o, &l) || o + l > size) return 0; v->data = b + o; v->len = l; o += l; break; }
        case NV_NIL: break;
        case NV_OBJ: if (o + 4 > size) return 0; v->obj = rd32(b + o); o += 4; break;
        default: return 0;
        }
    }
    /* classes: comprimento (com o NUL), nº de extras, extras u32, nome */
    o = h[7];
    for (unsigned i = 0; i < n->ncls; i++) {
        unsigned l, extra;
        if (!rd_var(b, size, &o, &l) || !rd_var(b, size, &o, &extra)) return 0;
        o += (unsigned long)extra * 4;
        if (o + l > size) return 0;
        n->cls[i] = (const char *)(b + o);
        o += l;
    }
    return 1;
}
const char *nib_class(const nib_t *n, unsigned o) { return o < n->nobj && n->obj[o].cls < n->ncls ? n->cls[n->obj[o].cls] : "?"; }
const nib_val_t *nib_get(const nib_t *n, unsigned o, const char *key) {
    if (o >= n->nobj) return 0;
    const nib_obj_t *ob = &n->obj[o];
    for (unsigned i = 0; i < ob->count && ob->first + i < n->nval; i++) {
        const nib_val_t *v = &n->val[ob->first + i];
        if (v->key < n->nkey && ns_eq(n->key[v->key], key)) return v;
    }
    return 0;
}
double nib_num(const nib_val_t *v) {
    if (!v) return 0;
    if (v->type == NV_FLOAT || v->type == NV_DOUBLE) return v->d;
    return (double)v->i;
}
