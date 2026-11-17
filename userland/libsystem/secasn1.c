/*
 * mOS libSystem — SecAsn1 (Security.framework): codificador e decodificador ASN.1 guiado por templates, no modelo do
 * NSS que o Security herdou (SecAsn1Template {kind, offset, sub, size}, SecAsn1Item {Length, Data}). Lê DER e BER
 * (comprimento indefinido e strings construídas), escreve DER (SET OF ordenado). A memória decodificada mora na arena do
 * coder e some no SecAsn1CoderRelease. É o que os apps usam para ler o recibo da loja e extensões de certificado.
 */
#include "libc.h"

void qsort(void *b, size_t n, size_t s, int (*cmp)(const void *, const void *));

typedef int OSStatus;
typedef struct { u32 kind, offset; const void *sub; u32 size; } tmpl_t;
typedef struct { size_t Length; u8 *Data; } item_t;
enum { errSecSuccess = 0, errSecParam = -50, errSecAllocate = -108, errSecDecode = -26275, errSecUnknownFormat = -25257 };

#define K_TAG       0xffu
#define K_TAGNUM    0x1fu
#define K_CLASS     0xc0u
#define K_CONS      0x20u
#define K_OPTIONAL  0x00100u
#define K_EXPLICIT  0x00200u
#define K_ANY       0x00400u
#define K_INLINE    0x00800u
#define K_POINTER   0x01000u
#define K_GROUP     0x02000u
#define K_DYNAMIC   0x04000u
#define K_SKIP      0x08000u
#define K_INNER     0x10000u
#define K_SAVE      0x20000u
#define K_MAYSTREAM 0x40000u
#define K_SKIPREST  0x80000u
#define K_CHOICE    0x100000u
#define K_NOSTREAM  0x200000u
#define K_DEBUG     0x400000u
#define K_SIGNED    0x800000u
enum { T_BOOL = 1, T_INT = 2, T_BITS = 3, T_OCTETS = 4, T_NULL = 5, T_OID = 6, T_ENUM = 10, T_UTF8 = 12, T_SEQ = 16, T_SET = 17,
       T_PRINT = 19, T_T61 = 20, T_IA5 = 22, T_UTC = 23, T_GEN = 24, T_VIS = 26, T_UNIV = 28, T_BMP = 30 };

/* ---- coder: arena de blocos */
typedef struct blk { struct blk *next; } blk_t;
typedef struct { blk_t *head; } coder_t;
static void *arena(coder_t *c, size_t n) {
    blk_t *b = malloc(sizeof *b + (n ? n : 1) + 15);
    if (!b) return 0;
    b->next = c->head; c->head = b;
    void *p = (void *)(((uintptr_t)(b + 1) + 15) & ~(uintptr_t)15);
    memset(p, 0, n);
    return p;
}
OSStatus SecAsn1CoderCreate(coder_t **out) {
    if (!out) return errSecParam;
    *out = calloc(1, sizeof(coder_t));
    return *out ? errSecSuccess : errSecAllocate;
}
OSStatus SecAsn1CoderRelease(coder_t *c) {
    if (!c) return errSecParam;
    for (blk_t *b = c->head, *n; b; b = n) { n = b->next; free(b); }
    free(c);
    return errSecSuccess;
}
void *SecAsn1Malloc(coder_t *c, size_t len) { return c ? arena(c, len) : 0; }
OSStatus SecAsn1AllocItem(coder_t *c, item_t *it, size_t len) {
    if (!c || !it) return errSecParam;
    it->Data = arena(c, len);
    if (!it->Data) return errSecAllocate;
    it->Length = len;
    return errSecSuccess;
}
OSStatus SecAsn1AllocCopy(coder_t *c, const void *src, size_t len, item_t *dst) {
    OSStatus r = SecAsn1AllocItem(c, dst, len);
    if (!r && len) memcpy(dst->Data, src, len);
    return r;
}
OSStatus SecAsn1AllocCopyItem(coder_t *c, const item_t *src, item_t *dst) {
    if (!src) return errSecParam;
    return SecAsn1AllocCopy(c, src->Data, src->Length, dst);
}
u8 SecAsn1OidCompare(const item_t *a, const item_t *b) {
    if (!a || !b) return 0;
    return a->Length == b->Length && (!a->Length || !memcmp(a->Data, b->Data, a->Length));
}

/* ---- leitura de TLV */
typedef struct { u8 tag; int cons; const u8 *start, *cont; size_t clen; const u8 *next; } tlv_t;
static int skip_indef(const u8 *p, const u8 *end, const u8 **after, int depth);
static int read_tlv(const u8 *p, const u8 *end, tlv_t *t) {
    if (p >= end) return -1;
    t->start = p;
    u8 b = *p++;
    if ((b & K_TAGNUM) == K_TAGNUM) {                                            /* número de tag alto: nenhum template casa */
        while (p < end && (*p & 0x80)) p++;
        if (p >= end) return -1;
        p++;
        b = (u8)((b & (K_CLASS | K_CONS)) | 0x1f);
    }
    t->tag = b; t->cons = (b & K_CONS) != 0;
    if (p >= end) return -1;
    u8 l = *p++;
    if (l == 0x80) {                                                             /* BER indefinido */
        if (!t->cons) return -1;
        const u8 *after;
        if (skip_indef(p, end, &after, 0) < 0) return -1;
        t->cont = p; t->clen = (size_t)(after - 2 - p); t->next = after;
        return 0;
    }
    size_t len = l;
    if (l & 0x80) {
        int n = l & 0x7f;
        if (n > 8 || end - p < n) return -1;
        len = 0;
        for (int i = 0; i < n; i++) len = (len << 8) | *p++;
    }
    if ((size_t)(end - p) < len) return -1;
    t->cont = p; t->clen = len; t->next = p + len;
    return 0;
}
/* acha o fim (depois do 00 00) de um conteúdo indefinido */
static int skip_indef(const u8 *p, const u8 *end, const u8 **after, int depth) {
    if (depth > 64) return -1;
    while (p < end) {
        if (end - p >= 2 && p[0] == 0 && p[1] == 0) { *after = p + 2; return 0; }
        tlv_t t;
        if (read_tlv(p, end, &t) < 0) return -1;
        p = t.next;
    }
    return -1;
}

/* ---- decodificação */
typedef const tmpl_t *(*chooser_fn)(void *arg, u8 enc, const char *buf, size_t len, void *dest);
static int dec_array(coder_t *c, const tmpl_t *arr, const u8 **pp, const u8 *end, u8 *base, int opt, int depth);
static int dec_entry(coder_t *c, const tmpl_t *t, const u8 **pp, const u8 *end, u8 *base, int is_head, int opt, int depth);

/* tag esperada (sem o bit de construído) de um template; -1 = qualquer */
static int want_tag(const tmpl_t *t, const void *sub, int depth) {
    u32 k = t->kind;
    if (depth > 32) return -2;
    if (k & K_ANY) return -1;
    if (k & K_TAG) return (int)(k & K_TAG & ~K_CONS);
    if (k & K_GROUP) return T_SEQ;                                               /* SEQUENCE OF sem tag = 0x10 */
    if ((k & (K_INLINE | K_POINTER)) && sub) {
        const tmpl_t *s = sub;
        if (s->kind & K_CHOICE) return -1;
        return want_tag(s, s->sub, depth + 1);
    }
    return -1;
}
static int put_leaf(coder_t *c, u32 ukind, const tlv_t *t, item_t *it) {
    const u8 *d = t->cont;
    size_t n = t->clen;
    u8 *buf = 0;
    if (t->cons) {                                                               /* string BER construída: junta os pedaços */
        buf = arena(c, n);
        if (!buf) return -1;
        size_t o = 0;
        const u8 *p = t->cont, *e = t->cont + t->clen;
        while (p < e) {
            tlv_t s;
            if (read_tlv(p, e, &s) < 0) return -1;
            if (s.cons) {
                item_t sub;
                if (put_leaf(c, ukind, &s, &sub) < 0) return -1;
                memcpy(buf + o, sub.Data, sub.Length); o += sub.Length;
            } else { memcpy(buf + o, s.cont, s.clen); o += s.clen; }
            p = s.next;
        }
        d = buf; n = o;
    }
    u32 tn = ukind & K_TAGNUM;
    if ((ukind & K_CLASS) == 0 && tn == T_BITS) {
        if (!n) return -1;
        u8 unused = d[0];
        if (unused > 7) return -1;
        it->Data = arena(c, n - 1);
        if (!it->Data) return -1;
        memcpy(it->Data, d + 1, n - 1);
        it->Length = (n - 1) * 8 - (n > 1 ? unused : 0);
        return 0;
    }
    if ((ukind & K_CLASS) == 0 && (tn == T_INT || tn == T_ENUM) && !(ukind & K_SIGNED))
        while (n > 1 && d[0] == 0) { d++; n--; }                                 /* inteiro sem sinal: tira zeros à esquerda */
    it->Data = arena(c, n);
    if (!it->Data) return -1;
    if (n) memcpy(it->Data, d, n);
    it->Length = n;
    return 0;
}
/* decodifica o conteúdo de um elemento cuja tag já casou; desc descreve o tipo de fato (cabeça de array ou campo) */
static int dec_content(coder_t *c, const tmpl_t *desc, u32 ukind, const void *sub, int is_head, const tlv_t *t, u8 *area, int depth) {
    if (depth > 48) return -1;
    if (ukind & K_GROUP) {                                                       /* SEQUENCE OF / SET OF: vetor de ponteiros com NULL no fim */
        const tmpl_t *es = sub;
        if (!es) return -1;
        size_t cnt = 0, cap = 8;
        void **v = malloc(cap * sizeof *v);
        if (!v) return -1;
        const u8 *q = t->cont, *e = t->cont + t->clen;
        while (q < e) {
            if (e - q >= 2 && q[0] == 0 && q[1] == 0) break;
            u8 *elt = arena(c, es->size ? es->size : sizeof(item_t));
            if (!elt || dec_array(c, es, &q, e, elt, 0, depth + 1) != 0) { free(v); return -1; }
            if (cnt + 1 >= cap) { cap *= 2; void **nv = realloc(v, cap * sizeof *v); if (!nv) { free(v); return -1; } v = nv; }
            v[cnt++] = elt;
        }
        void **out = arena(c, (cnt + 1) * sizeof *out);
        if (!out) { free(v); return -1; }
        memcpy(out, v, cnt * sizeof *out);
        out[cnt] = 0;
        free(v);
        *(void ***)area = out;
        return 0;
    }
    u32 tn = ukind & K_TAGNUM;
    if (is_head && (ukind & K_CLASS) == 0 && (tn == T_SEQ || tn == T_SET) && !(ukind & (K_INLINE | K_POINTER))) {
        const u8 *q = t->cont, *e = t->cont + t->clen;
        const tmpl_t *f = desc + 1;
        if (tn == T_SET) {                                                       /* SET: campos em qualquer ordem */
            int nf = 0;
            while (f[nf].kind) nf++;
            u8 done[64] = {0};
            while (q < e && !(e - q >= 2 && !q[0] && !q[1])) {
                int hit = 0;
                for (int i = 0; i < nf && i < 64 && !hit; i++) {
                    if (done[i]) continue;
                    const u8 *q2 = q;
                    if (dec_entry(c, &f[i], &q2, e, area, 0, 1, depth + 1) == 0) { done[i] = 1; q = q2; hit = 1; }
                }
                if (!hit) return -1;
            }
            for (int i = 0; i < nf && i < 64; i++) if (!done[i] && !(f[i].kind & (K_OPTIONAL | K_SKIPREST))) return -1;
            return 0;
        }
        for (; f->kind; f++) {
            if (f->kind & K_SKIPREST) return 0;
            int r = dec_entry(c, f, &q, e, area, 0, 0, depth + 1);
            if (r < 0) return -1;
        }
        return 0;
    }
    return put_leaf(c, ukind, t, (item_t *)area);
}
/* um campo (ou a cabeça de um array); 0 = ok, 1 = opcional ausente, -1 = erro */
static int dec_entry(coder_t *c, const tmpl_t *t, const u8 **pp, const u8 *end, u8 *base, int is_head, int opt, int depth) {
    if (depth > 48) return -1;
    u32 k = t->kind;
    u8 *dest = base + t->offset;
    opt |= (k & K_OPTIONAL) != 0;
    if (k & K_SKIPREST) { *pp = end; return 0; }
    if (*pp >= end || (end - *pp >= 2 && (*pp)[0] == 0 && (*pp)[1] == 0)) return opt ? 1 : -1;
    const void *sub = t->sub;
    if (k & K_DYNAMIC) {
        sub = ((chooser_fn)t->sub)(base, 0, (const char *)*pp, (size_t)(end - *pp), dest);
        if (!sub) return -1;
    }
    /* INLINE/POINTER sem tag própria: o subtemplate manda */
    if ((k & (K_INLINE | K_POINTER)) && !(k & (K_TAG | K_EXPLICIT | K_ANY))) {
        const tmpl_t *s = sub;
        if (!s) return -1;
        if (k & K_POINTER) {
            u8 *obj = arena(c, s->size ? s->size : sizeof(item_t));
            if (!obj) return -1;
            int r = dec_array(c, s, pp, end, obj, opt, depth + 1);
            if (r == 0) *(void **)dest = obj;
            return r;
        }
        return dec_array(c, s, pp, end, dest, opt, depth + 1);
    }
    tlv_t tl;
    if (read_tlv(*pp, end, &tl) < 0) return -1;
    int w = want_tag(t, sub, 0);
    if (w == -2) return -1;
    if (w >= 0 && (tl.tag & ~K_CONS) != (u8)w) return opt ? 1 : -1;
    if (k & K_SAVE) {                                                            /* guarda o TLV inteiro e não consome */
        item_t *it = (item_t *)dest;
        size_t n = (size_t)(tl.next - tl.start);
        it->Data = arena(c, n);
        if (!it->Data) return -1;
        memcpy(it->Data, tl.start, n); it->Length = n;
        return 0;
    }
    if (k & K_SKIP) { *pp = tl.next; return 0; }
    if (k & K_ANY) {
        item_t *it = (item_t *)dest;
        const u8 *src = (k & K_INNER) ? tl.cont : tl.start;
        size_t n = (k & K_INNER) ? tl.clen : (size_t)(tl.next - tl.start);
        if (k & K_POINTER) { item_t *o = arena(c, sizeof *o); if (!o) return -1; *(item_t **)dest = o; it = o; }
        it->Data = arena(c, n);
        if (!it->Data) return -1;
        if (n) memcpy(it->Data, src, n);
        it->Length = n;
        *pp = tl.next;
        return 0;
    }
    if (k & K_EXPLICIT) {
        const tmpl_t *s = sub;
        if (!s || !tl.cons) return -1;
        u8 *tgt = dest;
        if (k & K_POINTER) { tgt = arena(c, s->size ? s->size : sizeof(item_t)); if (!tgt) return -1; *(void **)dest = tgt; }
        const u8 *q = tl.cont;
        if (dec_array(c, s, &q, tl.cont + tl.clen, tgt, 0, depth + 1) != 0) return -1;
        *pp = tl.next;
        return 0;
    }
    int r;
    if ((k & (K_INLINE | K_POINTER)) && sub) {                                   /* tag implícita sobre o subtemplate */
        const tmpl_t *s = sub;
        u8 *tgt = dest;
        if (k & K_POINTER) { tgt = arena(c, s->size ? s->size : sizeof(item_t)); if (!tgt) return -1; *(void **)dest = tgt; }
        r = dec_content(c, s, s->kind, s->sub, 1, &tl, tgt + s->offset, depth + 1);
    } else if ((k & K_CLASS) && !(k & K_GROUP) && sub) {                         /* [n] IMPLICIT com o tipo de baixo no sub */
        const tmpl_t *s = sub;
        r = dec_content(c, s, s->kind, s->sub, 1, &tl, dest + s->offset, depth + 1);
    } else {
        r = dec_content(c, t, k, sub, is_head, &tl, dest, depth + 1);
    }
    if (r < 0) return -1;
    *pp = tl.next;
    return 0;
}
static int dec_array(coder_t *c, const tmpl_t *arr, const u8 **pp, const u8 *end, u8 *base, int opt, int depth) {
    if (depth > 48) return -1;
    if (arr->kind & K_CHOICE) {
        int *sel = (int *)(base + arr->offset);
        for (const tmpl_t *a = arr + 1; a->kind; a++) {
            const u8 *q = *pp;
            if (dec_entry(c, a, &q, end, base, 0, 1, depth + 1) == 0) { *sel = (int)a->size; *pp = q; return 0; }
        }
        return opt || (arr->kind & K_OPTIONAL) ? 1 : -1;
    }
    return dec_entry(c, arr, pp, end, base, 1, opt, depth);
}
OSStatus SecAsn1Decode(coder_t *c, const void *src, size_t len, const tmpl_t *templates, void *dest) {
    if (!c || !src || !templates || !dest) return errSecParam;
    /* o NSS zera o destino só onde decodifica; os apps já passam a struct zerada */
    const u8 *p = src;
    int r = dec_array(c, templates, &p, (const u8 *)src + len, dest, 0, 0);
    return r == 0 ? errSecSuccess : errSecDecode;
}
OSStatus SecAsn1DecodeData(coder_t *c, const item_t *src, const tmpl_t *templates, void *dest) {
    if (!src) return errSecParam;
    return SecAsn1Decode(c, src->Data, src->Length, templates, dest);
}

/* ---- codificação DER */
typedef struct { u8 *p; size_t n, cap; int err; } buf_t;
static void put(buf_t *b, const void *d, size_t n) {
    if (b->err) return;
    if (b->n + n > b->cap) {
        size_t nc = b->cap ? b->cap * 2 : 256;
        while (nc < b->n + n) nc *= 2;
        u8 *np = realloc(b->p, nc);
        if (!np) { b->err = 1; return; }
        b->p = np; b->cap = nc;
    }
    if (n) memcpy(b->p + b->n, d, n);
    b->n += n;
}
static void put_hdr(buf_t *b, u8 tag, size_t len) {
    u8 h[10];
    int n = 0;
    h[n++] = tag;
    if (len < 0x80) h[n++] = (u8)len;
    else {
        int k = 0;
        for (size_t l = len; l; l >>= 8) k++;
        h[n++] = (u8)(0x80 | k);
        for (int i = k - 1; i >= 0; i--) h[n++] = (u8)(len >> (8 * i));
    }
    put(b, h, (size_t)n);
}
static void wrap(buf_t *b, u8 tag, const buf_t *in) { put_hdr(b, tag, in->n); put(b, in->p, in->n); }
static int enc_array(const tmpl_t *arr, const u8 *base, buf_t *out, int depth);
static int enc_entry(const tmpl_t *t, const u8 *base, int is_head, buf_t *out, int depth);
static int absent(u32 k, const void *sub, const u8 *dest) {
    if (k & (K_POINTER | K_GROUP)) return *(void *const *)dest == 0;
    if ((k & (K_INLINE | K_EXPLICIT)) && !(k & K_ANY)) {                         /* olha o tipo de baixo: folha vazia = ausente */
        const tmpl_t *s = sub;
        if (!s || (k & K_DYNAMIC)) return 0;
        return absent(s->kind, s->sub, dest + s->offset) && !(s->kind & (K_CHOICE | K_INLINE | K_EXPLICIT)) &&
               !(((s->kind & K_TAGNUM) == T_SEQ || (s->kind & K_TAGNUM) == T_SET) && !(s->kind & K_CLASS) && !(s->kind & K_GROUP));
    }
    const item_t *it = (const item_t *)dest;
    return !it->Data && !it->Length;
}
static int cmp_der(const void *a, const void *b) {
    const buf_t *x = a, *y = b;
    size_t n = x->n < y->n ? x->n : y->n;
    int r = memcmp(x->p, y->p, n);
    return r ? r : (x->n > y->n) - (x->n < y->n);
}
/* conteúdo (sem o cabeçalho) de um elemento; devolve a tag natural em *tag */
static int enc_content(const tmpl_t *desc, u32 ukind, const void *sub, int is_head, const u8 *area, buf_t *c, u8 *tag, int depth) {
    if (depth > 48) return -1;
    u32 tn = ukind & K_TAGNUM;
    if (ukind & K_GROUP) {
        const tmpl_t *es = sub;
        void *const *v = *(void *const *const *)area;
        int set = (ukind & K_CLASS) == 0 && (ukind & K_TAGNUM) == T_SET;
        *tag = (u8)(set ? (T_SET | K_CONS) : (T_SEQ | K_CONS));
        size_t cnt = 0;
        while (v && v[cnt]) cnt++;
        buf_t *parts = cnt ? calloc(cnt, sizeof *parts) : 0;
        if (cnt && !parts) return -1;
        int bad = 0;
        for (size_t i = 0; i < cnt; i++) if (enc_array(es, v[i], &parts[i], depth + 1) < 0 || parts[i].err) bad = 1;
        if (!bad && set && cnt > 1) qsort(parts, cnt, sizeof *parts, cmp_der);
        for (size_t i = 0; i < cnt; i++) { if (!bad) put(c, parts[i].p, parts[i].n); free(parts[i].p); }
        free(parts);
        return bad ? -1 : 0;
    }
    if (is_head && (ukind & K_CLASS) == 0 && (tn == T_SEQ || tn == T_SET)) {
        *tag = (u8)(tn | K_CONS);
        if (tn == T_SET) {                                                       /* DER: campos do SET ordenados pela codificação */
            int nf = 0;
            while (desc[1 + nf].kind) nf++;
            buf_t *parts = nf ? calloc((size_t)nf, sizeof *parts) : 0;
            int bad = nf && !parts;
            for (int i = 0; i < nf && !bad; i++) if (enc_entry(&desc[1 + i], area, 0, &parts[i], depth + 1) < 0 || parts[i].err) bad = 1;
            if (!bad && nf > 1) qsort(parts, (size_t)nf, sizeof *parts, cmp_der);
            for (int i = 0; i < nf; i++) { if (!bad) put(c, parts[i].p, parts[i].n); free(parts[i].p); }
            free(parts);
            return bad ? -1 : 0;
        }
        for (const tmpl_t *f = desc + 1; f->kind; f++)
            if (enc_entry(f, area, 0, c, depth + 1) < 0) return -1;
        return 0;
    }
    const item_t *it = (const item_t *)area;
    *tag = (u8)(ukind & K_TAG);
    if ((ukind & K_CLASS) == 0) {
        if (tn == T_BITS) {
            size_t bytes = (it->Length + 7) / 8;
            u8 unused = (u8)((8 - it->Length % 8) % 8);
            put(c, &unused, 1);
            put(c, it->Data, bytes);
            return 0;
        }
        if (tn == T_INT || tn == T_ENUM) {
            const u8 *d = it->Data;
            size_t n = it->Length;
            if (!n) { u8 z = 0; put(c, &z, 1); return 0; }
            if (!(ukind & K_SIGNED)) {
                while (n > 1 && d[0] == 0 && !(d[1] & 0x80)) { d++; n--; }
                if (d[0] & 0x80) { u8 z = 0; put(c, &z, 1); }
            }
            put(c, d, n);
            return 0;
        }
        if (tn == T_BOOL) { u8 v = it->Length && it->Data[0] ? 0xff : 0; put(c, &v, 1); return 0; }
        if (tn == T_NULL) return 0;
    }
    put(c, it->Data, it->Length);
    return 0;
}
static int enc_entry(const tmpl_t *t, const u8 *base, int is_head, buf_t *out, int depth) {
    if (depth > 48) return -1;
    u32 k = t->kind;
    const u8 *dest = base + t->offset;
    if (k & (K_SKIP | K_SKIPREST | K_SAVE)) return 0;
    if ((k & K_OPTIONAL) && absent(k, t->sub, dest)) return 0;
    const void *sub = t->sub;
    if (k & K_DYNAMIC) {
        sub = ((chooser_fn)t->sub)((void *)base, 1, 0, 0, (void *)dest);
        if (!sub) return -1;
    }
    if ((k & (K_INLINE | K_POINTER)) && !(k & (K_TAG | K_EXPLICIT | K_ANY))) {
        const u8 *obj = (k & K_POINTER) ? *(const u8 *const *)dest : dest;
        if (!obj) return -1;
        return enc_array(sub, obj, out, depth + 1);
    }
    if (k & K_ANY) {
        const item_t *it = (k & K_POINTER) ? *(const item_t *const *)dest : (const item_t *)dest;
        if (!it) return -1;
        if (k & K_INNER) { buf_t tmp = {it->Data, it->Length, it->Length, 0}; wrap(out, (u8)(k & K_TAG), &tmp); }
        else put(out, it->Data, it->Length);
        return 0;
    }
    if (k & K_EXPLICIT) {
        const u8 *obj = (k & K_POINTER) ? *(const u8 *const *)dest : dest;
        if (!obj) return -1;
        buf_t in = {0};
        int r = enc_array(sub, obj, &in, depth + 1);
        if (r == 0 && !in.err) wrap(out, (u8)((k & K_TAG) | K_CONS), &in);
        free(in.p);
        return r == 0 && !in.err ? 0 : -1;
    }
    buf_t c = {0};
    u8 tag = 0;
    int r;
    if ((k & (K_INLINE | K_POINTER)) && sub) {
        const tmpl_t *s = sub;
        const u8 *obj = (k & K_POINTER) ? *(const u8 *const *)dest : dest;
        if (!obj) return -1;
        r = enc_content(s, s->kind, s->sub, 1, obj + s->offset, &c, &tag, depth + 1);
    } else if ((k & K_CLASS) && !(k & K_GROUP) && sub) {
        const tmpl_t *s = sub;
        r = enc_content(s, s->kind, s->sub, 1, dest + s->offset, &c, &tag, depth + 1);
    } else {
        r = enc_content(t, k, sub, is_head, dest, &c, &tag, depth + 1);
    }
    if (r == 0 && !c.err) {
        u8 final = (k & K_TAG) ? (u8)(k & K_TAG) : tag;
        if (tag & K_CONS) final |= K_CONS;                                       /* implícito sobre SEQUENCE continua construído */
        if ((k & K_TAG) && ((k & K_TAGNUM) == T_SEQ || (k & K_TAGNUM) == T_SET) && !(k & K_CLASS)) final |= K_CONS;
        wrap(out, final, &c);
    }
    free(c.p);
    return r == 0 && !c.err ? 0 : -1;
}
static int enc_array(const tmpl_t *arr, const u8 *base, buf_t *out, int depth) {
    if (depth > 48 || !arr) return -1;
    if (arr->kind & K_CHOICE) {
        int sel = *(const int *)(base + arr->offset);
        for (const tmpl_t *a = arr + 1; a->kind; a++)
            if ((int)a->size == sel) return enc_entry(a, base, 0, out, depth + 1);
        return -1;
    }
    return enc_entry(arr, base, 1, out, depth);
}
OSStatus SecAsn1EncodeItem(coder_t *c, const void *src, const tmpl_t *templates, item_t *dest) {
    if (!c || !src || !templates || !dest) return errSecParam;
    buf_t b = {0};
    int r = enc_array(templates, src, &b, 0);
    OSStatus st = errSecUnknownFormat;
    if (r == 0 && !b.err) st = SecAsn1AllocCopy(c, b.p, b.n, dest);
    free(b.p);
    return st;
}

/* ---- templates padrão (SecAsn1Templates.h) */
#define IT sizeof(item_t)
#define LEAF(name, kind) const tmpl_t kSecAsn1##name##Template[] = {{(kind), 0, 0, IT}}; \
    const tmpl_t kSecAsn1PointerTo##name##Template[] = {{K_POINTER, 0, kSecAsn1##name##Template, 0}}; \
    const tmpl_t kSecAsn1SequenceOf##name##Template[] = {{K_GROUP | T_SEQ, 0, kSecAsn1##name##Template, 0}}; \
    const tmpl_t kSecAsn1SetOf##name##Template[] = {{K_GROUP | T_SET, 0, kSecAsn1##name##Template, 0}};
LEAF(Any, K_ANY)
LEAF(BitString, T_BITS)
LEAF(BMPString, T_BMP)
LEAF(Boolean, T_BOOL)
LEAF(Enumerated, T_ENUM)
LEAF(GeneralizedTime, T_GEN)
LEAF(IA5String, T_IA5)
LEAF(Integer, T_INT | K_SIGNED)
LEAF(UnsignedInteger, T_INT)
LEAF(Null, T_NULL)
LEAF(ObjectID, T_OID)
LEAF(OctetString, T_OCTETS)
LEAF(PrintableString, T_PRINT)
LEAF(T61String, T_T61)
LEAF(TeletexString, T_T61)
LEAF(UniversalString, T_UNIV)
LEAF(UTCTime, T_UTC)
LEAF(UTF8String, T_UTF8)
LEAF(VisibleString, T_VIS)
const tmpl_t kSecAsn1SkipTemplate[] = {{K_SKIP, 0, 0, 0}};
const tmpl_t kSecAsn1AnyContentsTemplate[] = {{K_ANY | K_INNER, 0, 0, IT}};
