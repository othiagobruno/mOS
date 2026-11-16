/*
 * mOS PDF — núcleo de leitura (ISO 32000-1 e 32000-2), implementação própria.
 * Léxico, objetos, tabela xref clássica, xref stream e híbrida, cadeia /Prev (atualização incremental),
 * reparo por varredura quando a xref não confere, object streams, filtros FlateDecode/LZWDecode (com
 * Predictor 2 e PNG), ASCIIHex, ASCII85, RunLength e Crypt/Identity, e o security handler padrão
 * (R2..R6: RC4 40–128 bits, AES-128 e AES-256 com MD5/SHA-256/384/512). Árvore de páginas com herança.
 * Teste de host: tools/pdf_host.c (-DPDF_HOST, com -lz).
 */
#ifdef PDF_HOST
#include <stdlib.h>
#include <string.h>
#else
#include "sys.h"
#include "libc.h"
#endif
#include <stdint.h>
#define Z_SOLO 1
#include "../third_party/zlib/zlib.h"
#include "pdf_core.h"

typedef uint8_t u8_t;
typedef uint32_t u32_t;
typedef uint64_t u64_t;

/* ---------------------------------------------------------------- arena */
typedef struct pdf_chunk { struct pdf_chunk *next; size_t used, cap; } pdf_chunk;
typedef struct { uint8_t type; uint32_t off; uint32_t gen; uint32_t stm; pdf_obj *obj; uint8_t loading; } xref_ent;

struct pdf_doc {
    uint8_t *d;
    size_t n;
    pdf_chunk *arena;
    xref_ent *x;
    uint32_t nx;
    pdf_obj *trailer;
    pdf_obj **pages;
    int npages;
    /* criptografia */
    int encrypted, unlocked;
    int V, R, keylen, aes_str, aes_stm, encrypt_meta;
    uint8_t key[32];
    pdf_obj *encrypt;
    uint32_t encrypt_num;
    int32_t P;
    int major, minor;
};

void *pdf_alloc(pdf_doc *doc, size_t n) {
    n = (n + 15) & ~(size_t)15;
    pdf_chunk *c = doc->arena;
    if (!c || c->used + n > c->cap) {
        size_t cap = n > 65536 ? n : 65536;
        c = malloc(sizeof *c + cap);
        if (!c) return 0;
        c->next = doc->arena;
        c->used = 0;
        c->cap = cap;
        doc->arena = c;
    }
    void *p = (uint8_t *)(c + 1) + c->used;
    c->used += n;
    memset(p, 0, n);
    return p;
}
static pdf_obj *mk(pdf_doc *doc, int type) {
    pdf_obj *o = pdf_alloc(doc, sizeof *o);
    if (o) { o->type = (uint8_t)type; o->ext = doc; }   /* ext: o documento dono (a casca CF resolve referências com ele) */
    return o;
}
static char *dupstr(pdf_doc *doc, const char *s, size_t n) {
    char *c = pdf_alloc(doc, n + 1);
    if (c) { memcpy(c, s, n); c[n] = 0; }
    return c;
}

/* ---------------------------------------------------------------- léxico */
static int is_ws(int c) { return c == 0 || c == 9 || c == 10 || c == 12 || c == 13 || c == 32; }
static int is_delim(int c) { return c == '(' || c == ')' || c == '<' || c == '>' || c == '[' || c == ']' || c == '{' || c == '}' || c == '/' || c == '%'; }
static int hexv(int c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }

struct pdf_lexer {
    pdf_doc *doc;
    const uint8_t *d;
    size_t n, p;
    int content;                 /* conteúdo de página: palavras soltas são operadores */
    uint32_t objnum, objgen;     /* para decifrar strings do objeto atual */
    int decrypt;
};
static void skip_ws(pdf_lexer *L) {
    while (L->p < L->n) {
        int c = L->d[L->p];
        if (is_ws(c)) { L->p++; continue; }
        if (c == '%') { while (L->p < L->n && L->d[L->p] != 10 && L->d[L->p] != 13) L->p++; continue; }
        break;
    }
}
static void decrypt_string(pdf_doc *doc, pdf_obj *s, uint32_t num, uint32_t gen);
static pdf_obj *parse_obj(pdf_lexer *L, int depth);

static pdf_obj *lex_string(pdf_lexer *L) {
    size_t cap = 64, len = 0;
    uint8_t *b = malloc(cap);
    int depth = 1;
    L->p++;
    while (L->p < L->n && b) {
        int c = L->d[L->p++];
        if (len + 4 > cap) { uint8_t *nb = realloc(b, cap * 2); if (!nb) break; b = nb; cap *= 2; }
        if (c == '(') { depth++; b[len++] = '('; continue; }
        if (c == ')') { if (--depth == 0) break; b[len++] = ')'; continue; }
        if (c == '\\' && L->p < L->n) {
            int e = L->d[L->p++];
            switch (e) {
            case 'n': b[len++] = 10; break;
            case 'r': b[len++] = 13; break;
            case 't': b[len++] = 9; break;
            case 'b': b[len++] = 8; break;
            case 'f': b[len++] = 12; break;
            case 13: if (L->p < L->n && L->d[L->p] == 10) L->p++; break;   /* continuação de linha */
            case 10: break;
            default:
                if (e >= '0' && e <= '7') {
                    int v = e - '0';
                    for (int k = 0; k < 2 && L->p < L->n && L->d[L->p] >= '0' && L->d[L->p] <= '7'; k++) v = v * 8 + (L->d[L->p++] - '0');
                    b[len++] = (uint8_t)v;
                } else b[len++] = (uint8_t)e;
            }
            continue;
        }
        if (c == 13) { b[len++] = 10; if (L->p < L->n && L->d[L->p] == 10) L->p++; continue; }   /* fim de linha vira \n */
        b[len++] = (uint8_t)c;
    }
    pdf_obj *o = mk(L->doc, PO_STRING);
    if (o) {
        o->v.str = pdf_alloc(L->doc, len + 1);
        if (o->v.str && b) memcpy(o->v.str, b, len);
        o->n = (uint32_t)len;
    }
    free(b);
    return o;
}
static pdf_obj *lex_hex(pdf_lexer *L) {
    size_t cap = 32, len = 0;
    uint8_t *b = malloc(cap);
    int hi = -1;
    L->p++;
    while (L->p < L->n && b) {
        int c = L->d[L->p++];
        if (c == '>') break;
        int v = hexv(c);
        if (v < 0) continue;
        if (hi < 0) { hi = v; continue; }
        if (len + 1 > cap) { uint8_t *nb = realloc(b, cap * 2); if (!nb) break; b = nb; cap *= 2; }
        b[len++] = (uint8_t)(hi << 4 | v);
        hi = -1;
    }
    if (hi >= 0 && b) {
        if (len + 1 > cap) { uint8_t *nb = realloc(b, cap + 1); if (nb) { b = nb; cap++; } }
        if (len < cap) b[len++] = (uint8_t)(hi << 4);   /* dígito ímpar: completa com 0 */
    }
    pdf_obj *o = mk(L->doc, PO_STRING);
    if (o) {
        o->hex = 1;
        o->v.str = pdf_alloc(L->doc, len + 1);
        if (o->v.str && b) memcpy(o->v.str, b, len);
        o->n = (uint32_t)len;
    }
    free(b);
    return o;
}
static pdf_obj *lex_name(pdf_lexer *L) {
    char buf[256];
    size_t k = 0;
    L->p++;
    while (L->p < L->n && !is_ws(L->d[L->p]) && !is_delim(L->d[L->p])) {
        int c = L->d[L->p++];
        if (c == '#' && L->p + 1 < L->n && hexv(L->d[L->p]) >= 0 && hexv(L->d[L->p + 1]) >= 0) {
            c = hexv(L->d[L->p]) << 4 | hexv(L->d[L->p + 1]);
            L->p += 2;
        }
        if (k < sizeof buf - 1) buf[k++] = (char)c;
    }
    pdf_obj *o = mk(L->doc, PO_NAME);
    if (o) o->v.name = dupstr(L->doc, buf, k);
    return o;
}
/* número ou palavra (true, false, null, R, obj, stream, operadores do conteúdo) */
static pdf_obj *lex_word(pdf_lexer *L) {
    size_t s = L->p;
    while (L->p < L->n && !is_ws(L->d[L->p]) && !is_delim(L->d[L->p])) L->p++;
    size_t len = L->p - s;
    const char *w = (const char *)L->d + s;
    if (!len) { L->p++; return 0; }
    int num = 1, dot = 0, digits = 0;
    for (size_t i = 0; i < len; i++) {
        char c = w[i];
        if (c >= '0' && c <= '9') digits++;
        else if (c == '.' && !dot) dot = 1;
        else if ((c == '-' || c == '+') && i == 0) {}
        else { num = 0; break; }
    }
    if (num && digits) {
        if (!dot) {
            int64_t v = 0;
            int neg = 0;
            size_t i = 0;
            if (w[0] == '-' || w[0] == '+') { neg = w[0] == '-'; i = 1; }
            for (; i < len; i++) { v = v * 10 + (w[i] - '0'); if (v > (int64_t)1 << 53) break; }
            pdf_obj *o = mk(L->doc, PO_INT);
            if (o) o->v.i = neg ? -v : v;
            return o;
        }
        double v = 0, f = 0.1;
        int neg = 0, after = 0;
        for (size_t i = 0; i < len; i++) {
            char c = w[i];
            if (c == '-') neg = 1;
            else if (c == '.') after = 1;
            else if (c >= '0' && c <= '9') { if (after) { v += (c - '0') * f; f /= 10; } else v = v * 10 + (c - '0'); }
        }
        pdf_obj *o = mk(L->doc, PO_REAL);
        if (o) o->v.r = neg ? -v : v;
        return o;
    }
    if (len == 4 && !memcmp(w, "true", 4)) { pdf_obj *o = mk(L->doc, PO_BOOL); if (o) o->v.b = 1; return o; }
    if (len == 5 && !memcmp(w, "false", 5)) { pdf_obj *o = mk(L->doc, PO_BOOL); return o; }
    if (len == 4 && !memcmp(w, "null", 4)) return mk(L->doc, PO_NULL);
    pdf_obj *o = mk(L->doc, PO_OP);
    if (o) o->v.name = dupstr(L->doc, w, len);
    return o;
}
static int is_op(pdf_obj *o, const char *s) { return o && o->type == PO_OP && !strcmp(o->v.name, s); }

static pdf_obj *parse_obj(pdf_lexer *L, int depth) {
    skip_ws(L);
    if (L->p >= L->n || depth > 64) return 0;
    int c = L->d[L->p];
    if (c == '(') {
        pdf_obj *s = lex_string(L);
        if (s && L->decrypt) decrypt_string(L->doc, s, L->objnum, L->objgen);
        return s;
    }
    if (c == '<' && L->p + 1 < L->n && L->d[L->p + 1] == '<') {
        L->p += 2;
        size_t cap = 8;
        uint32_t cnt = 0;
        char **keys = malloc(cap * sizeof *keys);
        pdf_obj **vals = malloc(cap * sizeof *vals);
        for (;;) {
            skip_ws(L);
            if (L->p >= L->n) break;
            if (L->d[L->p] == '>' && L->p + 1 < L->n && L->d[L->p + 1] == '>') { L->p += 2; break; }
            if (L->d[L->p] != '/') {                          /* lixo dentro do dicionário: pula um objeto */
                pdf_obj *junk = parse_obj(L, depth + 1);
                if (!junk) { L->p++; }
                continue;
            }
            pdf_obj *k = lex_name(L);
            pdf_obj *v = parse_obj(L, depth + 1);
            if (!k) continue;
            if (!v || v->type == PO_OP) v = mk(L->doc, PO_NULL);
            if (cnt == cap) {
                cap *= 2;
                char **nk = realloc(keys, cap * sizeof *nk);
                pdf_obj **nv = realloc(vals, cap * sizeof *nv);
                if (!nk || !nv) { free(nk ? nk : keys); free(nv ? nv : vals); return 0; }
                keys = nk; vals = nv;
            }
            keys[cnt] = k->v.name;
            vals[cnt++] = v;
        }
        pdf_obj *o = mk(L->doc, PO_DICT);
        if (o) {
            o->n = cnt;
            o->v.dict.keys = pdf_alloc(L->doc, (cnt ? cnt : 1) * sizeof(char *));
            o->v.dict.vals = pdf_alloc(L->doc, (cnt ? cnt : 1) * sizeof(pdf_obj *));
            if (o->v.dict.keys && o->v.dict.vals) { memcpy(o->v.dict.keys, keys, cnt * sizeof(char *)); memcpy(o->v.dict.vals, vals, cnt * sizeof(pdf_obj *)); }
        }
        free(keys);
        free(vals);
        return o;
    }
    if (c == '<') {
        pdf_obj *s = lex_hex(L);
        if (s && L->decrypt) decrypt_string(L->doc, s, L->objnum, L->objgen);
        return s;
    }
    if (c == '/') return lex_name(L);
    if (c == '[') {
        L->p++;
        size_t cap = 8;
        uint32_t cnt = 0;
        pdf_obj **items = malloc(cap * sizeof *items);
        for (;;) {
            skip_ws(L);
            if (L->p >= L->n) break;
            if (L->d[L->p] == ']') { L->p++; break; }
            pdf_obj *v = parse_obj(L, depth + 1);
            if (!v) { if (L->p < L->n && L->d[L->p] != ']') L->p++; continue; }
            /* "n g R" dentro do array */
            if (!L->content && v->type == PO_OP && !strcmp(v->v.name, "R") && cnt >= 2 && items[cnt - 1]->type == PO_INT && items[cnt - 2]->type == PO_INT) {
                pdf_obj *r = mk(L->doc, PO_REF);
                if (r) { r->v.ref.num = (uint32_t)items[cnt - 2]->v.i; r->v.ref.gen = (uint32_t)items[cnt - 1]->v.i; }
                cnt -= 2;
                v = r;
            }
            if (cnt == cap) { cap *= 2; pdf_obj **ni = realloc(items, cap * sizeof *ni); if (!ni) break; items = ni; }
            items[cnt++] = v;
        }
        pdf_obj *o = mk(L->doc, PO_ARRAY);
        if (o) {
            o->n = cnt;
            o->v.arr = pdf_alloc(L->doc, (cnt ? cnt : 1) * sizeof(pdf_obj *));
            if (o->v.arr) memcpy(o->v.arr, items, cnt * sizeof(pdf_obj *));
        }
        free(items);
        return o;
    }
    if (c == ']' || c == '>' || c == ')' || c == '{' || c == '}') { L->p++; return mk(L->doc, PO_NULL); }
    pdf_obj *w = lex_word(L);
    /* referência "n g R" fora de array (valor de dicionário) */
    if (w && w->type == PO_INT && !L->content) {
        size_t save = L->p;
        skip_ws(L);
        size_t a = L->p;
        while (L->p < L->n && L->d[L->p] >= '0' && L->d[L->p] <= '9') L->p++;
        if (L->p > a) {
            size_t gend = L->p;
            skip_ws(L);
            if (L->p < L->n && L->d[L->p] == 'R' && (L->p + 1 >= L->n || is_ws(L->d[L->p + 1]) || is_delim(L->d[L->p + 1]))) {
                uint32_t gen = 0;
                for (size_t i = a; i < gend; i++) gen = gen * 10 + (L->d[i] - '0');
                L->p++;
                pdf_obj *r = mk(L->doc, PO_REF);
                if (r) { r->v.ref.num = (uint32_t)w->v.i; r->v.ref.gen = gen; }
                return r;
            }
        }
        L->p = save;
    }
    return w;
}

pdf_lexer *pdf_lex_begin(pdf_doc *doc, const uint8_t *d, size_t n) {
    pdf_lexer *L = calloc(1, sizeof *L);
    if (!L) return 0;
    L->doc = doc; L->d = d; L->n = n; L->content = 1;
    return L;
}
pdf_obj *pdf_lex_next(pdf_lexer *L) {
    for (;;) {
        skip_ws(L);
        if (L->p >= L->n) return 0;
        pdf_obj *o = parse_obj(L, 0);
        if (o) return o;
    }
}
size_t pdf_lex_pos(pdf_lexer *L) { return L->p; }
void pdf_lex_end(pdf_lexer *L) { free(L); }
/* BI <chave valor>* ID <dados> EI: o dicionário em forma abreviada fica como está (o renderizador expande) */
pdf_obj *pdf_lex_inline_image(pdf_lexer *L) {
    pdf_obj *d = mk(L->doc, PO_DICT);
    if (!d) return 0;
    size_t cap = 16;
    char **keys = malloc(cap * sizeof *keys);
    pdf_obj **vals = malloc(cap * sizeof *vals);
    uint32_t cnt = 0;
    for (;;) {
        skip_ws(L);
        if (L->p >= L->n) break;
        if (L->d[L->p] == 'I' && L->p + 1 < L->n && L->d[L->p + 1] == 'D' && (L->p + 2 >= L->n || is_ws(L->d[L->p + 2]))) { L->p += 2; break; }
        if (L->d[L->p] != '/') { pdf_obj *j = parse_obj(L, 1); if (!j) L->p++; continue; }
        pdf_obj *k = lex_name(L), *v = parse_obj(L, 1);
        if (!k || !v) continue;
        if (cnt == cap) { cap *= 2; keys = realloc(keys, cap * sizeof *keys); vals = realloc(vals, cap * sizeof *vals); if (!keys || !vals) break; }
        keys[cnt] = k->v.name; vals[cnt++] = v;
    }
    if (L->p < L->n && is_ws(L->d[L->p])) L->p++;           /* um branco depois de ID */
    d->n = cnt;
    d->v.dict.keys = pdf_alloc(L->doc, (cnt ? cnt : 1) * sizeof(char *));
    d->v.dict.vals = pdf_alloc(L->doc, (cnt ? cnt : 1) * sizeof(pdf_obj *));
    if (keys && vals && d->v.dict.keys && d->v.dict.vals) { memcpy(d->v.dict.keys, keys, cnt * sizeof(char *)); memcpy(d->v.dict.vals, vals, cnt * sizeof(pdf_obj *)); }
    free(keys);
    free(vals);
    /* fim dos dados: branco + "EI" + branco/fim (o primeiro que aparece; imagens com o padrão nos dados são raras) */
    size_t s = L->p, e = s;
    while (e + 2 <= L->n) {
        if (L->d[e] == 'E' && L->d[e + 1] == 'I' && (e == s || is_ws(L->d[e - 1])) && (e + 2 == L->n || is_ws(L->d[e + 2]) || is_delim(L->d[e + 2]))) break;
        e++;
    }
    if (e + 2 > L->n) e = L->n;
    size_t de = e;
    if (de > s && is_ws(L->d[de - 1])) de--;
    pdf_obj *st = mk(L->doc, PO_STREAM);
    if (st) {
        st->v.stream.dict = d;
        st->v.stream.data = pdf_alloc(L->doc, de - s + 1);
        if (st->v.stream.data) memcpy(st->v.stream.data, L->d + s, de - s);
        st->v.stream.dlen = de - s;
        st->v.stream.done = 0;
        st->v.stream.off = 0; st->v.stream.len = de - s;
    }
    L->p = e + 2 <= L->n ? e + 2 : L->n;
    return st;
}

/* ---------------------------------------------------------------- acesso */
pdf_obj *pdf_dict_get_raw(pdf_obj *dict, const char *key) {
    if (dict && dict->type == PO_STREAM) dict = dict->v.stream.dict;
    if (!dict || dict->type != PO_DICT) return 0;
    for (uint32_t i = 0; i < dict->n; i++) if (!strcmp(dict->v.dict.keys[i], key)) return dict->v.dict.vals[i];
    return 0;
}
pdf_obj *pdf_dict_get(pdf_doc *doc, pdf_obj *dict, const char *key) { return pdf_resolve(doc, pdf_dict_get_raw(dict, key)); }
pdf_obj *pdf_array_get(pdf_doc *doc, pdf_obj *a, uint32_t i) {
    a = pdf_resolve(doc, a);
    if (!a || a->type != PO_ARRAY || i >= a->n) return 0;
    return pdf_resolve(doc, a->v.arr[i]);
}
int pdf_num(pdf_obj *o, double *out) {
    if (!o) return 0;
    if (o->type == PO_INT) { *out = (double)o->v.i; return 1; }
    if (o->type == PO_REAL) { *out = o->v.r; return 1; }
    return 0;
}
const char *pdf_name(pdf_obj *o) { return o && o->type == PO_NAME ? o->v.name : 0; }
pdf_obj *pdf_stream_dict(pdf_obj *s) { return s && s->type == PO_STREAM ? s->v.stream.dict : 0; }
pdf_obj *pdf_trailer(pdf_doc *doc) { return doc->trailer; }
pdf_obj *pdf_catalog(pdf_doc *doc) { return pdf_dict_get(doc, doc->trailer, "Root"); }
pdf_obj *pdf_info(pdf_doc *doc) { return pdf_dict_get(doc, doc->trailer, "Info"); }
int pdf_version(const pdf_doc *doc, int *ma, int *mi) { *ma = doc->major; *mi = doc->minor; return 1; }
int pdf_is_encrypted(const pdf_doc *doc) { return doc->encrypted; }
int pdf_is_unlocked(const pdf_doc *doc) { return !doc->encrypted || doc->unlocked; }
uint32_t pdf_permissions(const pdf_doc *doc) { return doc->encrypted ? (uint32_t)doc->P : 0xFFFFFFFFu; }

/* ---------------------------------------------------------------- objetos indiretos */
static pdf_obj *load_object(pdf_doc *doc, uint32_t num);
pdf_obj *pdf_resolve(pdf_doc *doc, pdf_obj *o) {
    for (int guard = 0; o && o->type == PO_REF && guard < 32; guard++) o = load_object(doc, o->v.ref.num);
    return o && o->type == PO_REF ? 0 : o;
}
pdf_obj *pdf_get_object(pdf_doc *doc, uint32_t num) { return pdf_resolve(doc, load_object(doc, num)); }

static size_t find_bytes(const uint8_t *h, size_t hn, const char *s, size_t from) {
    size_t sl = strlen(s);
    for (size_t i = from; i + sl <= hn; i++) if (h[i] == (uint8_t)s[0] && !memcmp(h + i, s, sl)) return i;
    return (size_t)-1;
}
/* "N G obj <objeto> [stream ... endstream] endobj" a partir de off */
static pdf_obj *parse_indirect(pdf_doc *doc, size_t off, uint32_t want, uint32_t *gotnum) {
    pdf_lexer L = {doc, doc->d, doc->n, off, 0, 0, 0, 0};
    pdf_obj *a = (skip_ws(&L), lex_word(&L));
    skip_ws(&L);
    pdf_obj *b = lex_word(&L);
    skip_ws(&L);
    pdf_obj *k = lex_word(&L);
    if (!a || !b || !k || a->type != PO_INT || b->type != PO_INT || !is_op(k, "obj")) return 0;
    if (want != (uint32_t)-1 && (uint32_t)a->v.i != want) return 0;
    if (gotnum) *gotnum = (uint32_t)a->v.i;
    L.objnum = (uint32_t)a->v.i;
    L.objgen = (uint32_t)b->v.i;
    L.decrypt = doc->encrypted && doc->unlocked && L.objnum != doc->encrypt_num;
    pdf_obj *o = parse_obj(&L, 0);
    if (!o) return 0;
    skip_ws(&L);
    if (o->type == PO_DICT && L.p + 6 <= doc->n && !memcmp(doc->d + L.p, "stream", 6)) {
        L.p += 6;
        if (L.p < doc->n && doc->d[L.p] == 13) L.p++;
        if (L.p < doc->n && doc->d[L.p] == 10) L.p++;
        size_t start = L.p;
        double len = -1;
        pdf_obj *lo = pdf_dict_get_raw(o, "Length");
        if (lo && lo->type == PO_REF) {
            /* comprimento indireto: carrega sem recursão infinita */
            pdf_obj *lv = load_object(doc, lo->v.ref.num);
            if (!pdf_num(lv, &len)) len = -1;
        } else if (!pdf_num(lo, &len)) len = -1;
        size_t slen;
        int ok = len >= 0 && start + (size_t)len <= doc->n;
        if (ok) {
            size_t e = start + (size_t)len, q = e;
            while (q < doc->n && is_ws(doc->d[q])) q++;
            ok = q + 9 <= doc->n && !memcmp(doc->d + q, "endstream", 9);
        }
        if (ok) slen = (size_t)len;
        else {
            size_t e = find_bytes(doc->d, doc->n, "endstream", start);
            if (e == (size_t)-1) e = doc->n;
            slen = e - start;
            while (slen && (doc->d[start + slen - 1] == 10 || doc->d[start + slen - 1] == 13)) slen--;
        }
        pdf_obj *s = mk(doc, PO_STREAM);
        if (!s) return 0;
        s->v.stream.dict = o;
        s->v.stream.off = start;
        s->v.stream.len = slen;
        s->v.stream.num = L.objnum;
        s->v.stream.gen = L.objgen;
        return s;
    }
    return o;
}
static pdf_obj *objstm_object(pdf_doc *doc, uint32_t stmnum, uint32_t idx, uint32_t want);
static pdf_obj *load_object(pdf_doc *doc, uint32_t num) {
    if (num >= doc->nx) return 0;
    xref_ent *e = &doc->x[num];
    if (e->obj) return e->obj;
    if (e->loading || !e->type) return 0;
    e->loading = 1;
    pdf_obj *o = 0;
    if (e->type == 1) {
        o = e->off < doc->n ? parse_indirect(doc, e->off, num, 0) : 0;
        if (!o) {                                       /* deslocamento errado: procura "num 0 obj" */
            char pat[32];
            int k = 0;
            uint32_t v = num;
            char tmp[16];
            int t = 0;
            do { tmp[t++] = (char)('0' + v % 10); v /= 10; } while (v);
            while (t) pat[k++] = tmp[--t];
            pat[k] = 0;
            size_t pos = 0;
            while ((pos = find_bytes(doc->d, doc->n, pat, pos)) != (size_t)-1) {
                if ((pos == 0 || is_ws(doc->d[pos - 1])) && (o = parse_indirect(doc, pos, num, 0))) break;
                pos++;
            }
        }
    } else if (e->type == 2) o = objstm_object(doc, e->off, e->stm, num);
    e->loading = 0;
    e->obj = o;
    return o;
}

/* ---------------------------------------------------------------- filtros */
static int inflate_all(const uint8_t *in, size_t n, uint8_t **out, size_t *on) {
    z_stream z;
    memset(&z, 0, sizeof z);
    int raw = !(n >= 2 && (in[0] & 0x0F) == 8 && ((in[0] << 8 | in[1]) % 31) == 0);
    if (inflateInit2_(&z, raw ? -15 : 15, ZLIB_VERSION, (int)sizeof(z_stream)) != Z_OK) return -1;
    size_t cap = n * 4 + 1024, len = 0;
    uint8_t *b = malloc(cap);
    if (!b) { inflateEnd(&z); return -1; }
    z.next_in = (Bytef *)in;
    z.avail_in = (uInt)n;
    for (;;) {
        if (len == cap) {
            if (cap > ((size_t)1 << 31)) break;
            uint8_t *nb = realloc(b, cap * 2);
            if (!nb) break;
            b = nb;
            cap *= 2;
        }
        z.next_out = b + len;
        z.avail_out = (uInt)(cap - len);
        int r = inflate(&z, Z_NO_FLUSH);
        len = cap - z.avail_out;
        if (r == Z_STREAM_END) break;
        if (r != Z_OK) break;                         /* dados corrompidos: fica o que saiu (como os leitores fazem) */
        if (z.avail_in == 0 && z.avail_out != 0) break;
    }
    inflateEnd(&z);
    *out = b;
    *on = len;
    return 0;
}
static int lzw_decode(const uint8_t *in, size_t n, int early, uint8_t **out, size_t *on) {
    uint16_t prefix[4096];
    uint8_t suffix[4096], stack[4097];
    size_t cap = n * 3 + 1024, len = 0;
    uint8_t *b = malloc(cap);
    if (!b) return -1;
    uint32_t acc = 0;
    int bits = 0, size = 9, next = 258, old = -1;
    size_t ip = 0;
    uint8_t first = 0;
    for (int i = 0; i < 256; i++) { prefix[i] = 0xFFFF; suffix[i] = (uint8_t)i; }
    for (;;) {
        while (bits < size && ip < n) { acc = acc << 8 | in[ip++]; bits += 8; }
        if (bits < size) break;
        int code = (int)((acc >> (bits - size)) & ((1u << size) - 1));
        bits -= size;
        if (code == 256) { size = 9; next = 258; old = -1; continue; }
        if (code == 257) break;
        int sp = 0, c = code;
        if (old < 0) {
            if (code > 255) break;
            if (len == cap) { uint8_t *nb = realloc(b, cap * 2); if (!nb) break; b = nb; cap *= 2; }
            b[len++] = (uint8_t)code;
            old = code;
            first = (uint8_t)code;
            continue;
        }
        if (code > next) break;
        if (code == next) { stack[sp++] = first; c = old; }
        while (c > 255) { if (sp >= 4096) break; stack[sp++] = suffix[c]; c = prefix[c]; }
        first = (uint8_t)c;
        stack[sp++] = first;
        if (len + (size_t)sp > cap) { while (len + (size_t)sp > cap) cap *= 2; uint8_t *nb = realloc(b, cap); if (!nb) break; b = nb; }
        while (sp) b[len++] = stack[--sp];
        if (next < 4096) { prefix[next] = (uint16_t)old; suffix[next] = first; next++; }
        if (next + early >= (1 << size) && size < 12) size++;
        old = code;
    }
    *out = b;
    *on = len;
    return 0;
}
static int ahx_decode(const uint8_t *in, size_t n, uint8_t **out, size_t *on) {
    uint8_t *b = malloc(n / 2 + 2);
    size_t len = 0;
    int hi = -1;
    if (!b) return -1;
    for (size_t i = 0; i < n; i++) {
        if (in[i] == '>') break;
        int v = hexv(in[i]);
        if (v < 0) continue;
        if (hi < 0) hi = v;
        else { b[len++] = (uint8_t)(hi << 4 | v); hi = -1; }
    }
    if (hi >= 0) b[len++] = (uint8_t)(hi << 4);
    *out = b;
    *on = len;
    return 0;
}
static int a85_decode(const uint8_t *in, size_t n, uint8_t **out, size_t *on) {
    uint8_t *b = malloc(n + 8);
    size_t len = 0;
    uint32_t t = 0;
    int k = 0;
    if (!b) return -1;
    size_t i = 0;
    if (n >= 2 && in[0] == '<' && in[1] == '~') i = 2;
    for (; i < n; i++) {
        int c = in[i];
        if (c == '~') break;
        if (is_ws(c)) continue;
        if (c == 'z' && k == 0) { memset(b + len, 0, 4); len += 4; continue; }
        if (c < '!' || c > 'u') continue;
        t = t * 85 + (uint32_t)(c - '!');
        if (++k == 5) {
            b[len++] = (uint8_t)(t >> 24); b[len++] = (uint8_t)(t >> 16); b[len++] = (uint8_t)(t >> 8); b[len++] = (uint8_t)t;
            t = 0; k = 0;
        }
    }
    if (k > 1) {
        for (int j = k; j < 5; j++) t = t * 85 + 84;
        for (int j = 0; j < k - 1; j++) b[len++] = (uint8_t)(t >> (24 - 8 * j));
    }
    *out = b;
    *on = len;
    return 0;
}
static int rl_decode(const uint8_t *in, size_t n, uint8_t **out, size_t *on) {
    size_t cap = n * 2 + 128, len = 0, i = 0;
    uint8_t *b = malloc(cap);
    if (!b) return -1;
    while (i < n) {
        int c = in[i++];
        if (c == 128) break;
        size_t k = c < 128 ? (size_t)c + 1 : (size_t)(257 - c);
        if (len + k > cap) { while (len + k > cap) cap *= 2; uint8_t *nb = realloc(b, cap); if (!nb) break; b = nb; }
        if (c < 128) { if (i + k > n) k = n - i; memcpy(b + len, in + i, k); i += k; }
        else { if (i >= n) break; memset(b + len, in[i++], k); }
        len += k;
    }
    *out = b;
    *on = len;
    return 0;
}
static uint8_t paeth3(int a, int b, int c) {
    int p = a + b - c, pa = p > a ? p - a : a - p, pb = p > b ? p - b : b - p, pc = p > c ? p - c : c - p;
    return (uint8_t)(pa <= pb && pa <= pc ? a : pb <= pc ? b : c);
}
/* Predictor 2 (TIFF) e 10..15 (PNG, um byte de filtro por linha) */
static int unpredict(pdf_doc *doc, pdf_obj *parms, uint8_t **buf, size_t *len) {
    double pv = 1, colors = 1, bpc = 8, cols = 1;
    pdf_num(pdf_dict_get(doc, parms, "Predictor"), &pv);
    if (pv < 2) return 0;
    pdf_num(pdf_dict_get(doc, parms, "Colors"), &colors);
    pdf_num(pdf_dict_get(doc, parms, "BitsPerComponent"), &bpc);
    pdf_num(pdf_dict_get(doc, parms, "Columns"), &cols);
    size_t bpp = ((size_t)colors * (size_t)bpc + 7) / 8, row = ((size_t)cols * (size_t)colors * (size_t)bpc + 7) / 8;
    if (!row || !bpp) return 0;
    uint8_t *d = *buf;
    if (pv == 2) {
        if (bpc != 8) return 0;
        for (size_t y = 0; (y + 1) * row <= *len; y++)
            for (size_t i = bpp; i < row; i++) d[y * row + i] = (uint8_t)(d[y * row + i] + d[y * row + i - bpp]);
        return 0;
    }
    size_t nrows = *len / (row + 1);
    uint8_t *o = malloc(nrows * row + 1), *prev = calloc(row, 1);
    if (!o || !prev) { free(o); free(prev); return -1; }
    for (size_t y = 0; y < nrows; y++) {
        const uint8_t *in = d + y * (row + 1);
        int f = in[0];
        uint8_t *cur = o + y * row;
        for (size_t i = 0; i < row; i++) {
            int a = i >= bpp ? cur[i - bpp] : 0, b = prev[i], c = i >= bpp ? prev[i - bpp] : 0, x = in[1 + i];
            switch (f) {
            case 1: cur[i] = (uint8_t)(x + a); break;
            case 2: cur[i] = (uint8_t)(x + b); break;
            case 3: cur[i] = (uint8_t)(x + ((a + b) >> 1)); break;
            case 4: cur[i] = (uint8_t)(x + paeth3(a, b, c)); break;
            default: cur[i] = (uint8_t)x; break;
            }
        }
        memcpy(prev, cur, row);
    }
    free(prev);
    free(*buf);
    *buf = o;
    *len = nrows * row;
    return 0;
}
static const char *abbrev_filter(const char *f) {
    static const char *const m[][2] = {{"Fl", "FlateDecode"}, {"LZW", "LZWDecode"}, {"AHx", "ASCIIHexDecode"}, {"A85", "ASCII85Decode"},
                                       {"RL", "RunLengthDecode"}, {"DCT", "DCTDecode"}, {"CCF", "CCITTFaxDecode"}};
    for (unsigned i = 0; i < sizeof m / sizeof m[0]; i++) if (!strcmp(f, m[i][0])) return m[i][1];
    return f;
}
int pdf_decode_filter(pdf_doc *doc, const char *f, pdf_obj *parms, const uint8_t *in, size_t n, uint8_t **out, size_t *on) {
    f = abbrev_filter(f);
    int r;
    if (!strcmp(f, "FlateDecode")) r = inflate_all(in, n, out, on);
    else if (!strcmp(f, "LZWDecode")) {
        double early = 1;
        pdf_num(pdf_dict_get(doc, parms, "EarlyChange"), &early);
        r = lzw_decode(in, n, early ? 1 : 0, out, on);
    } else if (!strcmp(f, "ASCIIHexDecode")) return ahx_decode(in, n, out, on);
    else if (!strcmp(f, "ASCII85Decode")) return a85_decode(in, n, out, on);
    else if (!strcmp(f, "RunLengthDecode")) return rl_decode(in, n, out, on);
    else if (!strcmp(f, "Crypt")) { *out = malloc(n ? n : 1); if (!*out) return -1; memcpy(*out, in, n); *on = n; return 0; }
    else return 1;                                    /* filtro de imagem: quem chama decide */
    if (r == 0 && parms) r = unpredict(doc, parms, out, on);
    return r;
}

/* ---------------------------------------------------------------- criptografia */
static void decrypt_buf(pdf_doc *doc, uint32_t num, uint32_t gen, int aes, uint8_t *b, size_t n, size_t *outn);
static int obj_key(pdf_doc *doc, uint32_t num, uint32_t gen, int aes, uint8_t *k);
const uint8_t *pdf_stream_raw(pdf_doc *doc, pdf_obj *s, size_t *len) {
    if (!s || s->type != PO_STREAM) return 0;
    if (s->v.stream.data && s->v.stream.done >= 1) { *len = s->v.stream.dlen; return s->v.stream.data; }
    if (s->v.stream.off && s->v.stream.off + s->v.stream.len <= doc->n) {
        uint8_t *b = pdf_alloc(doc, s->v.stream.len + 16);
        if (!b) return 0;
        memcpy(b, doc->d + s->v.stream.off, s->v.stream.len);
        size_t n = s->v.stream.len;
        int crypt_identity = 0;
        pdf_obj *flt = pdf_dict_get(doc, s, "Filter");
        if (flt && flt->type == PO_ARRAY && flt->n && pdf_name(pdf_resolve(doc, flt->v.arr[0])) && !strcmp(pdf_name(pdf_resolve(doc, flt->v.arr[0])), "Crypt")) crypt_identity = 1;
        if (flt && pdf_name(flt) && !strcmp(pdf_name(flt), "Crypt")) crypt_identity = 1;
        const char *type = pdf_name(pdf_dict_get(doc, s, "Type"));
        int is_xref = type && !strcmp(type, "XRef");
        int is_meta = type && !strcmp(type, "Metadata") && !doc->encrypt_meta;
        if (doc->encrypted && doc->unlocked && !is_xref && !is_meta && !crypt_identity && s->v.stream.num != doc->encrypt_num)
            decrypt_buf(doc, s->v.stream.num, s->v.stream.gen, doc->aes_stm, b, n, &n);
        s->v.stream.data = b;
        s->v.stream.dlen = n;
        s->v.stream.done = 1;
        *len = n;
        return b;
    }
    if (s->v.stream.data) { *len = s->v.stream.dlen; return s->v.stream.data; }   /* imagem inline */
    return 0;
}
const uint8_t *pdf_stream_data(pdf_doc *doc, pdf_obj *s, size_t *len, const char **image_filter) {
    if (image_filter) *image_filter = 0;
    if (!s || s->type != PO_STREAM) return 0;
    size_t n;
    const uint8_t *raw = pdf_stream_raw(doc, s, &n);
    if (!raw) return 0;
    pdf_obj *f = pdf_dict_get(doc, s, "Filter");
    if (!f) f = pdf_dict_get(doc, s, "F");
    if (f && f->type == PO_STRING) f = 0;            /* /F de arquivo externo: sem suporte, dado cru */
    pdf_obj *p = pdf_dict_get(doc, s, "DecodeParms");
    if (!p) p = pdf_dict_get(doc, s, "DP");
    if (!f) { *len = n; return raw; }
    int nf = f->type == PO_ARRAY ? (int)f->n : 1;
    uint8_t *cur = 0;
    size_t cn = n;
    const uint8_t *src = raw;
    for (int i = 0; i < nf; i++) {
        pdf_obj *fi = f->type == PO_ARRAY ? pdf_resolve(doc, f->v.arr[i]) : f;
        pdf_obj *pi = p && p->type == PO_ARRAY ? pdf_resolve(doc, i < (int)p->n ? p->v.arr[i] : 0) : p;
        const char *fn = pdf_name(fi);
        if (!fn) break;
        uint8_t *o = 0;
        size_t on = 0;
        int r = pdf_decode_filter(doc, fn, pi && pi->type == PO_DICT ? pi : 0, src, cn, &o, &on);
        if (r == 1) { if (image_filter) *image_filter = abbrev_filter(fn); break; }
        if (r < 0) { free(o); break; }
        free(cur);
        cur = o;
        src = cur;
        cn = on;
    }
    if (!cur) { *len = cn; return src; }
    uint8_t *keep = pdf_alloc(doc, cn + 1);
    if (keep) memcpy(keep, cur, cn);
    free(cur);
    *len = cn;
    return keep;
}

/* MD5 (RFC 1321) */
static uint32_t rol32(uint32_t x, int c) { return x << c | x >> (32 - c); }
void pdf_md5(const uint8_t *msg, size_t len, uint8_t out[16]) {
    static const uint32_t K[64] = {
        0xd76aa478, 0xe8c7b756, 0x242070db, 0xc1bdceee, 0xf57c0faf, 0x4787c62a, 0xa8304613, 0xfd469501, 0x698098d8, 0x8b44f7af, 0xffff5bb1,
        0x895cd7be, 0x6b901122, 0xfd987193, 0xa679438e, 0x49b40821, 0xf61e2562, 0xc040b340, 0x265e5a51, 0xe9b6c7aa, 0xd62f105d, 0x02441453,
        0xd8a1e681, 0xe7d3fbc8, 0x21e1cde6, 0xc33707d6, 0xf4d50d87, 0x455a14ed, 0xa9e3e905, 0xfcefa3f8, 0x676f02d9, 0x8d2a4c8a, 0xfffa3942,
        0x8771f681, 0x6d9d6122, 0xfde5380c, 0xa4beea44, 0x4bdecfa9, 0xf6bb4b60, 0xbebfbc70, 0x289b7ec6, 0xeaa127fa, 0xd4ef3085, 0x04881d05,
        0xd9d4d039, 0xe6db99e5, 0x1fa27cf8, 0xc4ac5665, 0xf4292244, 0x432aff97, 0xab9423a7, 0xfc93a039, 0x655b59c3, 0x8f0ccc92, 0xffeff47d,
        0x85845dd1, 0x6fa87e4f, 0xfe2ce6e0, 0xa3014314, 0x4e0811a1, 0xf7537e82, 0xbd3af235, 0x2ad7d2bb, 0xeb86d391};
    static const int R[64] = {7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 7, 12, 17, 22, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20, 5, 9, 14, 20,
                              4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 4, 11, 16, 23, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21, 6, 10, 15, 21};
    uint32_t h[4] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476};
    size_t nl = ((len + 8) / 64 + 1) * 64;
    uint8_t *m = calloc(nl, 1);
    if (!m) { memset(out, 0, 16); return; }
    memcpy(m, msg, len);
    m[len] = 0x80;
    uint64_t bits = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++) m[nl - 8 + i] = (uint8_t)(bits >> (8 * i));
    for (size_t off = 0; off < nl; off += 64) {
        uint32_t w[16], a = h[0], b = h[1], c = h[2], d = h[3];
        for (int i = 0; i < 16; i++) w[i] = (uint32_t)m[off + i * 4] | (uint32_t)m[off + i * 4 + 1] << 8 | (uint32_t)m[off + i * 4 + 2] << 16 | (uint32_t)m[off + i * 4 + 3] << 24;
        for (int i = 0; i < 64; i++) {
            uint32_t f;
            int g;
            if (i < 16) { f = (b & c) | (~b & d); g = i; }
            else if (i < 32) { f = (d & b) | (~d & c); g = (5 * i + 1) % 16; }
            else if (i < 48) { f = b ^ c ^ d; g = (3 * i + 5) % 16; }
            else { f = c ^ (b | ~d); g = (7 * i) % 16; }
            uint32_t t = d;
            d = c; c = b;
            b = b + rol32(a + f + K[i] + w[g], R[i]);
            a = t;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d;
    }
    free(m);
    for (int i = 0; i < 4; i++) for (int k = 0; k < 4; k++) out[i * 4 + k] = (uint8_t)(h[i] >> (8 * k));
}
/* SHA-256 e SHA-512/384 (FIPS 180-4) */
static uint32_t ror32(uint32_t x, int n) { return x >> n | x << (32 - n); }
static uint64_t ror64(uint64_t x, int n) { return x >> n | x << (64 - n); }
void pdf_sha256(const uint8_t *msg, size_t len, uint8_t out[32]) {
    static const uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
        0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
        0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
        0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
    uint32_t h[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    size_t nl = ((len + 8) / 64 + 1) * 64;
    uint8_t *m = calloc(nl, 1);
    if (!m) { memset(out, 0, 32); return; }
    memcpy(m, msg, len);
    m[len] = 0x80;
    uint64_t bits = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++) m[nl - 1 - i] = (uint8_t)(bits >> (8 * i));
    for (size_t off = 0; off < nl; off += 64) {
        uint32_t w[64];
        for (int i = 0; i < 16; i++) w[i] = (uint32_t)m[off + i * 4] << 24 | (uint32_t)m[off + i * 4 + 1] << 16 | (uint32_t)m[off + i * 4 + 2] << 8 | m[off + i * 4 + 3];
        for (int i = 16; i < 64; i++) {
            uint32_t s0 = ror32(w[i - 15], 7) ^ ror32(w[i - 15], 18) ^ (w[i - 15] >> 3), s1 = ror32(w[i - 2], 17) ^ ror32(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 64; i++) {
            uint32_t S1 = ror32(e, 6) ^ ror32(e, 11) ^ ror32(e, 25), ch = (e & f) ^ (~e & g), t1 = hh + S1 + ch + K[i] + w[i];
            uint32_t S0 = ror32(a, 2) ^ ror32(a, 13) ^ ror32(a, 22), mj = (a & b) ^ (a & c) ^ (b & c), t2 = S0 + mj;
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    free(m);
    for (int i = 0; i < 8; i++) for (int k = 0; k < 4; k++) out[i * 4 + k] = (uint8_t)(h[i] >> (24 - 8 * k));
}
static void sha512_core(const uint8_t *msg, size_t len, uint64_t *h, int outlen, uint8_t *out) {
    static const uint64_t K[80] = {
        0x428a2f98d728ae22ULL, 0x7137449123ef65cdULL, 0xb5c0fbcfec4d3b2fULL, 0xe9b5dba58189dbbcULL, 0x3956c25bf348b538ULL, 0x59f111f1b605d019ULL,
        0x923f82a4af194f9bULL, 0xab1c5ed5da6d8118ULL, 0xd807aa98a3030242ULL, 0x12835b0145706fbeULL, 0x243185be4ee4b28cULL, 0x550c7dc3d5ffb4e2ULL,
        0x72be5d74f27b896fULL, 0x80deb1fe3b1696b1ULL, 0x9bdc06a725c71235ULL, 0xc19bf174cf692694ULL, 0xe49b69c19ef14ad2ULL, 0xefbe4786384f25e3ULL,
        0x0fc19dc68b8cd5b5ULL, 0x240ca1cc77ac9c65ULL, 0x2de92c6f592b0275ULL, 0x4a7484aa6ea6e483ULL, 0x5cb0a9dcbd41fbd4ULL, 0x76f988da831153b5ULL,
        0x983e5152ee66dfabULL, 0xa831c66d2db43210ULL, 0xb00327c898fb213fULL, 0xbf597fc7beef0ee4ULL, 0xc6e00bf33da88fc2ULL, 0xd5a79147930aa725ULL,
        0x06ca6351e003826fULL, 0x142929670a0e6e70ULL, 0x27b70a8546d22ffcULL, 0x2e1b21385c26c926ULL, 0x4d2c6dfc5ac42aedULL, 0x53380d139d95b3dfULL,
        0x650a73548baf63deULL, 0x766a0abb3c77b2a8ULL, 0x81c2c92e47edaee6ULL, 0x92722c851482353bULL, 0xa2bfe8a14cf10364ULL, 0xa81a664bbc423001ULL,
        0xc24b8b70d0f89791ULL, 0xc76c51a30654be30ULL, 0xd192e819d6ef5218ULL, 0xd69906245565a910ULL, 0xf40e35855771202aULL, 0x106aa07032bbd1b8ULL,
        0x19a4c116b8d2d0c8ULL, 0x1e376c085141ab53ULL, 0x2748774cdf8eeb99ULL, 0x34b0bcb5e19b48a8ULL, 0x391c0cb3c5c95a63ULL, 0x4ed8aa4ae3418acbULL,
        0x5b9cca4f7763e373ULL, 0x682e6ff3d6b2b8a3ULL, 0x748f82ee5defb2fcULL, 0x78a5636f43172f60ULL, 0x84c87814a1f0ab72ULL, 0x8cc702081a6439ecULL,
        0x90befffa23631e28ULL, 0xa4506cebde82bde9ULL, 0xbef9a3f7b2c67915ULL, 0xc67178f2e372532bULL, 0xca273eceea26619cULL, 0xd186b8c721c0c207ULL,
        0xeada7dd6cde0eb1eULL, 0xf57d4f7fee6ed178ULL, 0x06f067aa72176fbaULL, 0x0a637dc5a2c898a6ULL, 0x113f9804bef90daeULL, 0x1b710b35131c471bULL,
        0x28db77f523047d84ULL, 0x32caab7b40c72493ULL, 0x3c9ebe0a15c9bebcULL, 0x431d67c49c100d4cULL, 0x4cc5d4becb3e42b6ULL, 0x597f299cfc657e2aULL,
        0x5fcb6fab3ad6faecULL, 0x6c44198c4a475817ULL};
    size_t nl = ((len + 16) / 128 + 1) * 128;
    uint8_t *m = calloc(nl, 1);
    if (!m) { memset(out, 0, (size_t)outlen); return; }
    memcpy(m, msg, len);
    m[len] = 0x80;
    uint64_t bits = (uint64_t)len * 8;
    for (int i = 0; i < 8; i++) m[nl - 1 - i] = (uint8_t)(bits >> (8 * i));
    for (size_t off = 0; off < nl; off += 128) {
        uint64_t w[80];
        for (int i = 0; i < 16; i++) { w[i] = 0; for (int k = 0; k < 8; k++) w[i] = w[i] << 8 | m[off + i * 8 + k]; }
        for (int i = 16; i < 80; i++) {
            uint64_t s0 = ror64(w[i - 15], 1) ^ ror64(w[i - 15], 8) ^ (w[i - 15] >> 7), s1 = ror64(w[i - 2], 19) ^ ror64(w[i - 2], 61) ^ (w[i - 2] >> 6);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }
        uint64_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
        for (int i = 0; i < 80; i++) {
            uint64_t S1 = ror64(e, 14) ^ ror64(e, 18) ^ ror64(e, 41), ch = (e & f) ^ (~e & g), t1 = hh + S1 + ch + K[i] + w[i];
            uint64_t S0 = ror64(a, 28) ^ ror64(a, 34) ^ ror64(a, 39), mj = (a & b) ^ (a & c) ^ (b & c), t2 = S0 + mj;
            hh = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
        }
        h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
    }
    free(m);
    for (int i = 0; i < outlen; i++) out[i] = (uint8_t)(h[i / 8] >> (56 - 8 * (i % 8)));
}
void pdf_sha512(const uint8_t *d, size_t n, uint8_t out[64]) {
    uint64_t h[8] = {0x6a09e667f3bcc908ULL, 0xbb67ae8584caa73bULL, 0x3c6ef372fe94f82bULL, 0xa54ff53a5f1d36f1ULL,
                     0x510e527fade682d1ULL, 0x9b05688c2b3e6c1fULL, 0x1f83d9abfb41bd6bULL, 0x5be0cd19137e2179ULL};
    sha512_core(d, n, h, 64, out);
}
void pdf_sha384(const uint8_t *d, size_t n, uint8_t out[48]) {
    uint64_t h[8] = {0xcbbb9d5dc1059ed8ULL, 0x629a292a367cd507ULL, 0x9159015a3070dd17ULL, 0x152fecd8f70e5939ULL,
                     0x67332667ffc00b31ULL, 0x8eb44a8768581511ULL, 0xdb0c2e0d64f98fa7ULL, 0x47b5481dbefa4fa4ULL};
    sha512_core(d, n, h, 48, out);
}
void pdf_rc4(const uint8_t *key, size_t kl, uint8_t *buf, size_t n) {
    uint8_t S[256];
    for (int i = 0; i < 256; i++) S[i] = (uint8_t)i;
    for (int i = 0, j = 0; i < 256; i++) {
        j = (j + S[i] + key[i % kl]) & 255;
        uint8_t t = S[i]; S[i] = S[j]; S[j] = t;
    }
    for (size_t k = 0, i = 0, j = 0; k < n; k++) {
        i = (i + 1) & 255;
        j = (j + S[i]) & 255;
        uint8_t t = S[i]; S[i] = S[j]; S[j] = t;
        buf[k] ^= S[(S[i] + S[j]) & 255];
    }
}
/* AES (FIPS 197), 128 e 256 bits */
static const uint8_t sbox[256] = {
    0x63, 0x7c, 0x77, 0x7b, 0xf2, 0x6b, 0x6f, 0xc5, 0x30, 0x01, 0x67, 0x2b, 0xfe, 0xd7, 0xab, 0x76, 0xca, 0x82, 0xc9, 0x7d, 0xfa, 0x59, 0x47, 0xf0,
    0xad, 0xd4, 0xa2, 0xaf, 0x9c, 0xa4, 0x72, 0xc0, 0xb7, 0xfd, 0x93, 0x26, 0x36, 0x3f, 0xf7, 0xcc, 0x34, 0xa5, 0xe5, 0xf1, 0x71, 0xd8, 0x31, 0x15,
    0x04, 0xc7, 0x23, 0xc3, 0x18, 0x96, 0x05, 0x9a, 0x07, 0x12, 0x80, 0xe2, 0xeb, 0x27, 0xb2, 0x75, 0x09, 0x83, 0x2c, 0x1a, 0x1b, 0x6e, 0x5a, 0xa0,
    0x52, 0x3b, 0xd6, 0xb3, 0x29, 0xe3, 0x2f, 0x84, 0x53, 0xd1, 0x00, 0xed, 0x20, 0xfc, 0xb1, 0x5b, 0x6a, 0xcb, 0xbe, 0x39, 0x4a, 0x4c, 0x58, 0xcf,
    0xd0, 0xef, 0xaa, 0xfb, 0x43, 0x4d, 0x33, 0x85, 0x45, 0xf9, 0x02, 0x7f, 0x50, 0x3c, 0x9f, 0xa8, 0x51, 0xa3, 0x40, 0x8f, 0x92, 0x9d, 0x38, 0xf5,
    0xbc, 0xb6, 0xda, 0x21, 0x10, 0xff, 0xf3, 0xd2, 0xcd, 0x0c, 0x13, 0xec, 0x5f, 0x97, 0x44, 0x17, 0xc4, 0xa7, 0x7e, 0x3d, 0x64, 0x5d, 0x19, 0x73,
    0x60, 0x81, 0x4f, 0xdc, 0x22, 0x2a, 0x90, 0x88, 0x46, 0xee, 0xb8, 0x14, 0xde, 0x5e, 0x0b, 0xdb, 0xe0, 0x32, 0x3a, 0x0a, 0x49, 0x06, 0x24, 0x5c,
    0xc2, 0xd3, 0xac, 0x62, 0x91, 0x95, 0xe4, 0x79, 0xe7, 0xc8, 0x37, 0x6d, 0x8d, 0xd5, 0x4e, 0xa9, 0x6c, 0x56, 0xf4, 0xea, 0x65, 0x7a, 0xae, 0x08,
    0xba, 0x78, 0x25, 0x2e, 0x1c, 0xa6, 0xb4, 0xc6, 0xe8, 0xdd, 0x74, 0x1f, 0x4b, 0xbd, 0x8b, 0x8a, 0x70, 0x3e, 0xb5, 0x66, 0x48, 0x03, 0xf6, 0x0e,
    0x61, 0x35, 0x57, 0xb9, 0x86, 0xc1, 0x1d, 0x9e, 0xe1, 0xf8, 0x98, 0x11, 0x69, 0xd9, 0x8e, 0x94, 0x9b, 0x1e, 0x87, 0xe9, 0xce, 0x55, 0x28, 0xdf,
    0x8c, 0xa1, 0x89, 0x0d, 0xbf, 0xe6, 0x42, 0x68, 0x41, 0x99, 0x2d, 0x0f, 0xb0, 0x54, 0xbb, 0x16};
static uint8_t inv_sbox[256];
static uint8_t xt(uint8_t x) { return (uint8_t)(x << 1 ^ ((x >> 7) * 0x1b)); }
static uint8_t gmul(uint8_t a, uint8_t b) { uint8_t r = 0; while (b) { if (b & 1) r ^= a; a = xt(a); b >>= 1; } return r; }
static int aes_expand(const uint8_t *key, int bits, uint8_t *rk) {
    int nk = bits / 32, nr = nk + 6, total = 4 * (nr + 1);
    uint8_t rcon = 1;
    memcpy(rk, key, (size_t)nk * 4);
    for (int i = nk; i < total; i++) {
        uint8_t t[4];
        memcpy(t, rk + (i - 1) * 4, 4);
        if (i % nk == 0) {
            uint8_t u = t[0];
            t[0] = sbox[t[1]] ^ rcon; t[1] = sbox[t[2]]; t[2] = sbox[t[3]]; t[3] = sbox[u];
            rcon = xt(rcon);
        } else if (nk > 6 && i % nk == 4) for (int k = 0; k < 4; k++) t[k] = sbox[t[k]];
        for (int k = 0; k < 4; k++) rk[i * 4 + k] = rk[(i - nk) * 4 + k] ^ t[k];
    }
    return nr;
}
static void aes_enc_block(const uint8_t *rk, int nr, uint8_t *s) {
    for (int i = 0; i < 16; i++) s[i] ^= rk[i];
    for (int r = 1; r <= nr; r++) {
        uint8_t t[16];
        for (int i = 0; i < 16; i++) t[i] = sbox[s[i]];
        for (int c = 0; c < 4; c++) for (int rr = 0; rr < 4; rr++) s[c * 4 + rr] = t[((c + rr) % 4) * 4 + rr];
        if (r != nr)
            for (int c = 0; c < 4; c++) {
                uint8_t *p = s + c * 4, a0 = p[0], a1 = p[1], a2 = p[2], a3 = p[3];
                p[0] = (uint8_t)(xt(a0) ^ xt(a1) ^ a1 ^ a2 ^ a3); p[1] = (uint8_t)(a0 ^ xt(a1) ^ xt(a2) ^ a2 ^ a3);
                p[2] = (uint8_t)(a0 ^ a1 ^ xt(a2) ^ xt(a3) ^ a3); p[3] = (uint8_t)(xt(a0) ^ a0 ^ a1 ^ a2 ^ xt(a3));
            }
        for (int i = 0; i < 16; i++) s[i] ^= rk[r * 16 + i];
    }
}
static void aes_dec_block(const uint8_t *rk, int nr, uint8_t *s) {
    if (!inv_sbox[1]) for (int i = 0; i < 256; i++) inv_sbox[sbox[i]] = (uint8_t)i;
    for (int i = 0; i < 16; i++) s[i] ^= rk[nr * 16 + i];
    for (int r = nr - 1; r >= 0; r--) {
        uint8_t t[16];
        for (int c = 0; c < 4; c++) for (int rr = 0; rr < 4; rr++) t[((c + rr) % 4) * 4 + rr] = s[c * 4 + rr];
        for (int i = 0; i < 16; i++) s[i] = inv_sbox[t[i]] ^ rk[r * 16 + i];
        if (r)
            for (int c = 0; c < 4; c++) {
                uint8_t *p = s + c * 4, a0 = p[0], a1 = p[1], a2 = p[2], a3 = p[3];
                p[0] = gmul(a0, 14) ^ gmul(a1, 11) ^ gmul(a2, 13) ^ gmul(a3, 9);
                p[1] = gmul(a0, 9) ^ gmul(a1, 14) ^ gmul(a2, 11) ^ gmul(a3, 13);
                p[2] = gmul(a0, 13) ^ gmul(a1, 9) ^ gmul(a2, 14) ^ gmul(a3, 11);
                p[3] = gmul(a0, 11) ^ gmul(a1, 13) ^ gmul(a2, 9) ^ gmul(a3, 14);
            }
    }
}
int pdf_aes_cbc_decrypt(const uint8_t *key, int bits, const uint8_t *iv, const uint8_t *in, size_t n, uint8_t *out, size_t *on, int unpad) {
    uint8_t rk[240], prev[16], blk[16];
    int nr = aes_expand(key, bits, rk);
    memcpy(prev, iv, 16);
    size_t m = n / 16 * 16;
    for (size_t i = 0; i < m; i += 16) {
        memcpy(blk, in + i, 16);
        aes_dec_block(rk, nr, blk);
        for (int k = 0; k < 16; k++) out[i + k] = blk[k] ^ prev[k];
        memcpy(prev, in + i, 16);
    }
    *on = m;
    if (unpad && m) {
        int p = out[m - 1];
        if (p >= 1 && p <= 16) *on = m - (size_t)p;
    }
    return 0;
}
void pdf_aes_cbc_encrypt(const uint8_t *key, int bits, const uint8_t *iv, const uint8_t *in, size_t n, uint8_t *out) {
    uint8_t rk[240], prev[16];
    int nr = aes_expand(key, bits, rk);
    memcpy(prev, iv, 16);
    for (size_t i = 0; i + 16 <= n; i += 16) {
        for (int k = 0; k < 16; k++) out[i + k] = in[i + k] ^ prev[k];
        aes_enc_block(rk, nr, out + i);
        memcpy(prev, out + i, 16);
    }
}

static const uint8_t PAD[32] = {0x28, 0xBF, 0x4E, 0x5E, 0x4E, 0x75, 0x8A, 0x41, 0x64, 0x00, 0x4E, 0x56, 0xFF, 0xFA, 0x01, 0x08,
                                0x2E, 0x2E, 0x00, 0xB6, 0xD0, 0x68, 0x3E, 0x80, 0x2F, 0x0C, 0xA9, 0xFE, 0x64, 0x53, 0x69, 0x7A};
static int obj_key(pdf_doc *doc, uint32_t num, uint32_t gen, int aes, uint8_t *k) {
    if (doc->R >= 5) { memcpy(k, doc->key, 32); return 32; }
    uint8_t b[32 + 5 + 4];
    int n = doc->keylen;
    memcpy(b, doc->key, (size_t)n);
    b[n] = (uint8_t)num; b[n + 1] = (uint8_t)(num >> 8); b[n + 2] = (uint8_t)(num >> 16); b[n + 3] = (uint8_t)gen; b[n + 4] = (uint8_t)(gen >> 8);
    int L = n + 5;
    if (aes) { memcpy(b + L, "sAlT", 4); L += 4; }
    uint8_t h[16];
    pdf_md5(b, (size_t)L, h);
    int kl = n + 5 < 16 ? n + 5 : 16;
    memcpy(k, h, (size_t)kl);
    return kl;
}
static void decrypt_buf(pdf_doc *doc, uint32_t num, uint32_t gen, int aes, uint8_t *b, size_t n, size_t *outn) {
    uint8_t k[32];
    int kl = obj_key(doc, num, gen, aes, k);
    if (!aes) { pdf_rc4(k, (size_t)kl, b, n); *outn = n; return; }
    if (n < 16) { *outn = 0; return; }
    size_t on = 0;
    uint8_t iv[16];
    memcpy(iv, b, 16);
    pdf_aes_cbc_decrypt(k, kl == 32 ? 256 : 128, iv, b + 16, n - 16, b, &on, 1);
    *outn = on;
}
static void decrypt_string(pdf_doc *doc, pdf_obj *s, uint32_t num, uint32_t gen) {
    if (!doc->encrypted || !doc->unlocked || !s->n) return;
    size_t on = s->n;
    decrypt_buf(doc, num, gen, doc->aes_str, s->v.str, s->n, &on);
    s->n = (uint32_t)on;
}
static const uint8_t *str_bytes(pdf_obj *o, uint32_t *n) {
    if (!o || o->type != PO_STRING) { *n = 0; return 0; }
    *n = o->n;
    return o->v.str;
}
/* R2..R4: chave do arquivo a partir da senha (algoritmo 2) */
static void key_r4(pdf_doc *doc, const uint8_t *pw, size_t pl, uint8_t *key) {
    uint8_t buf[32 + 128 + 4 + 64 + 4];
    size_t k = 0;
    uint32_t on, idn;
    const uint8_t *O = str_bytes(pdf_dict_get(doc, doc->encrypt, "O"), &on);
    pdf_obj *ida = pdf_dict_get(doc, doc->trailer, "ID");
    const uint8_t *id0 = str_bytes(pdf_array_get(doc, ida, 0), &idn);
    if (pl > 32) pl = 32;
    memcpy(buf, pw, pl);
    memcpy(buf + pl, PAD, 32 - pl);
    k = 32;
    if (O) { memcpy(buf + k, O, on < 32 ? on : 32); k += 32; }
    buf[k++] = (uint8_t)doc->P; buf[k++] = (uint8_t)(doc->P >> 8); buf[k++] = (uint8_t)(doc->P >> 16); buf[k++] = (uint8_t)((uint32_t)doc->P >> 24);
    if (id0 && idn <= 64) { memcpy(buf + k, id0, idn); k += idn; }
    if (doc->R >= 4 && !doc->encrypt_meta) { buf[k++] = 0xFF; buf[k++] = 0xFF; buf[k++] = 0xFF; buf[k++] = 0xFF; }
    uint8_t h[16];
    pdf_md5(buf, k, h);
    if (doc->R >= 3) for (int i = 0; i < 50; i++) pdf_md5(h, (size_t)doc->keylen, h);
    memcpy(key, h, (size_t)doc->keylen);
}
static int check_user_r4(pdf_doc *doc, const uint8_t *key) {
    uint32_t un, idn;
    const uint8_t *U = str_bytes(pdf_dict_get(doc, doc->encrypt, "U"), &un);
    if (!U || un < 16) return 0;
    if (doc->R == 2) {
        uint8_t t[32];
        memcpy(t, PAD, 32);
        pdf_rc4(key, (size_t)doc->keylen, t, 32);
        return !memcmp(t, U, 32 < un ? 32 : un);
    }
    pdf_obj *ida = pdf_dict_get(doc, doc->trailer, "ID");
    const uint8_t *id0 = str_bytes(pdf_array_get(doc, ida, 0), &idn);
    uint8_t buf[32 + 64], h[16];
    memcpy(buf, PAD, 32);
    size_t k = 32;
    if (id0 && idn <= 64) { memcpy(buf + 32, id0, idn); k += idn; }
    pdf_md5(buf, k, h);
    for (int i = 0; i < 20; i++) {
        uint8_t kk[16];
        for (int j = 0; j < doc->keylen; j++) kk[j] = (uint8_t)(key[j] ^ i);
        pdf_rc4(kk, (size_t)doc->keylen, h, 16);
    }
    return !memcmp(h, U, 16);
}
/* senha de dono (algoritmo 7): recupera a de usuário do /O */
static int owner_r4(pdf_doc *doc, const uint8_t *pw, size_t pl, uint8_t *userpw) {
    uint8_t p[32], h[16];
    uint32_t on;
    const uint8_t *O = str_bytes(pdf_dict_get(doc, doc->encrypt, "O"), &on);
    if (!O || on < 32) return 0;
    if (pl > 32) pl = 32;
    memcpy(p, pw, pl);
    memcpy(p + pl, PAD, 32 - pl);
    pdf_md5(p, 32, h);
    if (doc->R >= 3) for (int i = 0; i < 50; i++) pdf_md5(h, 16, h);
    memcpy(userpw, O, 32);
    if (doc->R == 2) pdf_rc4(h, (size_t)doc->keylen, userpw, 32);
    else
        for (int i = 19; i >= 0; i--) {
            uint8_t kk[16];
            for (int j = 0; j < doc->keylen; j++) kk[j] = (uint8_t)(h[j] ^ i);
            pdf_rc4(kk, (size_t)doc->keylen, userpw, 32);
        }
    return 1;
}
/* R6: hash do algoritmo 2.B (ISO 32000-2) */
static void hash_r6(const uint8_t *pw, size_t pl, const uint8_t *salt, const uint8_t *udata, int r6, uint8_t *out) {
    uint8_t K[64];
    size_t kl = 32;
    {
        uint8_t t[127 + 8 + 48];
        size_t n = 0;
        memcpy(t, pw, pl); n = pl;
        memcpy(t + n, salt, 8); n += 8;
        if (udata) { memcpy(t + n, udata, 48); n += 48; }
        pdf_sha256(t, n, K);
    }
    if (!r6) { memcpy(out, K, 32); return; }
    size_t cap = 64 * (127 + 64 + 48);
    uint8_t *K1 = malloc(cap), *E = malloc(cap);
    if (!K1 || !E) { free(K1); free(E); memcpy(out, K, 32); return; }
    for (int round = 0;; round++) {
        size_t one = pl + kl + (udata ? 48 : 0), n = 0;
        for (int r = 0; r < 64; r++) {
            memcpy(K1 + n, pw, pl); n += pl;
            memcpy(K1 + n, K, kl); n += kl;
            if (udata) { memcpy(K1 + n, udata, 48); n += 48; }
        }
        (void)one;
        pdf_aes_cbc_encrypt(K, 128, K + 16, K1, n, E);
        int sum = 0;
        for (int i = 0; i < 16; i++) sum += E[i];
        int m = sum % 3;
        if (m == 0) { pdf_sha256(E, n, K); kl = 32; }
        else if (m == 1) { pdf_sha384(E, n, K); kl = 48; }
        else { pdf_sha512(E, n, K); kl = 64; }
        if (round >= 63 && E[n - 1] <= round - 32) break;
    }
    free(K1);
    free(E);
    memcpy(out, K, 32);
}
static int unlock_r6(pdf_doc *doc, const uint8_t *pw, size_t pl) {
    uint32_t un, on, uen, oen;
    const uint8_t *U = str_bytes(pdf_dict_get(doc, doc->encrypt, "U"), &un), *O = str_bytes(pdf_dict_get(doc, doc->encrypt, "O"), &on);
    const uint8_t *UE = str_bytes(pdf_dict_get(doc, doc->encrypt, "UE"), &uen), *OE = str_bytes(pdf_dict_get(doc, doc->encrypt, "OE"), &oen);
    if (!U || !O || un < 48 || on < 48) return 0;
    if (pl > 127) pl = 127;
    int r6 = doc->R == 6;
    uint8_t h[32], iv[16] = {0}, k[32];
    size_t got;
    hash_r6(pw, pl, O + 32, U, r6, h);                /* dono */
    if (!memcmp(h, O, 32) && OE && oen >= 32) {
        hash_r6(pw, pl, O + 40, U, r6, k);
        pdf_aes_cbc_decrypt(k, 256, iv, OE, 32, doc->key, &got, 0);
        return 1;
    }
    hash_r6(pw, pl, U + 32, 0, r6, h);                /* usuário */
    if (!memcmp(h, U, 32) && UE && uen >= 32) {
        hash_r6(pw, pl, U + 40, 0, r6, k);
        pdf_aes_cbc_decrypt(k, 256, iv, UE, 32, doc->key, &got, 0);
        return 1;
    }
    return 0;
}
static void reload_after_unlock(pdf_doc *doc) {
    /* objetos já lidos (antes da chave) precisam ser relidos decifrados, salvo o próprio /Encrypt */
    for (uint32_t i = 0; i < doc->nx; i++) if (i != doc->encrypt_num) doc->x[i].obj = 0;
    doc->npages = -1;
}
int pdf_unlock(pdf_doc *doc, const char *password) {
    if (!doc->encrypted) return 1;
    if (doc->unlocked) return 1;
    const uint8_t *pw = (const uint8_t *)(password ? password : "");
    size_t pl = strlen((const char *)pw);
    int ok = 0;
    if (doc->R >= 5) ok = unlock_r6(doc, pw, pl);
    else {
        uint8_t key[16];
        key_r4(doc, pw, pl, key);
        if (check_user_r4(doc, key)) { memcpy(doc->key, key, 16); ok = 1; }
        else {
            uint8_t up[32];
            if (owner_r4(doc, pw, pl, up)) {
                key_r4(doc, up, 32, key);
                if (check_user_r4(doc, key)) { memcpy(doc->key, key, 16); ok = 1; }
            }
        }
    }
    if (!ok) return 0;
    doc->unlocked = 1;
    reload_after_unlock(doc);
    return 1;
}
static void setup_encrypt(pdf_doc *doc) {
    pdf_obj *er = pdf_dict_get_raw(doc->trailer, "Encrypt");
    if (!er) return;
    if (er->type == PO_REF) doc->encrypt_num = er->v.ref.num;
    pdf_obj *e = pdf_resolve(doc, er);
    if (!e || e->type != PO_DICT) return;
    const char *flt = pdf_name(pdf_dict_get(doc, e, "Filter"));
    doc->encrypted = 1;
    doc->encrypt = e;
    if (!flt || strcmp(flt, "Standard")) return;     /* handler de certificado: não destrava */
    double v = 0, r = 0, len = 40, p = 0;
    pdf_num(pdf_dict_get(doc, e, "V"), &v);
    pdf_num(pdf_dict_get(doc, e, "R"), &r);
    pdf_num(pdf_dict_get(doc, e, "Length"), &len);
    pdf_num(pdf_dict_get(doc, e, "P"), &p);
    doc->V = (int)v; doc->R = (int)r; doc->P = (int32_t)(int64_t)p;
    doc->keylen = doc->R == 2 ? 5 : (int)(len / 8);
    if (doc->keylen < 5) doc->keylen = 5;
    if (doc->keylen > 16) doc->keylen = 16;
    pdf_obj *em = pdf_dict_get(doc, e, "EncryptMetadata");
    doc->encrypt_meta = !em || em->type != PO_BOOL || em->v.b;
    if (doc->V >= 4) {
        pdf_obj *cf = pdf_dict_get(doc, e, "CF");
        const char *stmf = pdf_name(pdf_dict_get(doc, e, "StmF")), *strf = pdf_name(pdf_dict_get(doc, e, "StrF"));
        for (int which = 0; which < 2; which++) {
            const char *nm = which ? strf : stmf;
            pdf_obj *f = nm && strcmp(nm, "Identity") ? pdf_dict_get(doc, cf, nm) : 0;
            const char *cfm = pdf_name(pdf_dict_get(doc, f, "CFM"));
            int aes = cfm && (!strcmp(cfm, "AESV2") || !strcmp(cfm, "AESV3"));
            if (which) doc->aes_str = aes; else doc->aes_stm = aes;
            double fl = 0;
            if (f && pdf_num(pdf_dict_get(doc, f, "Length"), &fl) && doc->R < 5) { int k = fl > 40 ? (int)(fl / 8) : (int)fl; if (k >= 5 && k <= 16) doc->keylen = k; }
        }
    }
    pdf_unlock(doc, "");
}

/* ---------------------------------------------------------------- xref */
static int grow_x(pdf_doc *doc, uint32_t need) {
    if (need <= doc->nx) return 1;
    if (need > 8000000) return 0;
    xref_ent *nx = realloc(doc->x, need * sizeof *nx);
    if (!nx) return 0;
    memset(nx + doc->nx, 0, (need - doc->nx) * sizeof *nx);
    doc->x = nx;
    doc->nx = need;
    return 1;
}
/* a primeira seção lida (a mais nova) ganha: só grava entradas ainda vazias */
static void set_x(pdf_doc *doc, uint32_t num, int type, uint32_t a, uint32_t b, int *seen) {
    if (!grow_x(doc, num + 1)) return;
    xref_ent *e = &doc->x[num];
    if (seen[0] && (e->type || e->stm == 0xFFFFFFFFu)) return;
    (void)seen;
    if (e->type || e->stm == 0xFFFFFFFFu) return;
    if (type == 0) { e->stm = 0xFFFFFFFFu; return; }   /* livre: marca para não ser preenchida por seções velhas */
    e->type = (uint8_t)type;
    if (type == 1) { e->off = a; e->gen = b; }
    else { e->off = a; e->stm = b; }
}
static pdf_obj *read_xref_table(pdf_doc *doc, size_t p) {
    pdf_lexer L = {doc, doc->d, doc->n, p + 4, 0, 0, 0, 0};
    int seen[1] = {0};
    for (;;) {
        skip_ws(&L);
        if (L.p + 7 <= doc->n && !memcmp(doc->d + L.p, "trailer", 7)) { L.p += 7; break; }
        pdf_obj *a = lex_word(&L);
        skip_ws(&L);
        pdf_obj *b = lex_word(&L);
        if (!a || !b || a->type != PO_INT || b->type != PO_INT) return 0;
        uint32_t start = (uint32_t)a->v.i, cnt = (uint32_t)b->v.i;
        for (uint32_t i = 0; i < cnt; i++) {
            skip_ws(&L);
            if (L.p + 18 > doc->n) return 0;
            const uint8_t *e = doc->d + L.p;
            uint32_t off = 0, gen = 0;
            for (int k = 0; k < 10; k++) off = off * 10 + (uint32_t)(e[k] - '0');
            for (int k = 11; k < 16; k++) gen = gen * 10 + (uint32_t)(e[k] - '0');
            char t = (char)e[17];
            L.p += 18;
            if (start + i == 0 && t == 'f') continue;
            set_x(doc, start + i, t == 'n' ? 1 : 0, off, gen, seen);
        }
    }
    return parse_obj(&L, 0);
}
static pdf_obj *read_xref_stream(pdf_doc *doc, size_t p) {
    pdf_obj *s = parse_indirect(doc, p, (uint32_t)-1, 0);
    if (!s || s->type != PO_STREAM) return 0;
    pdf_obj *w = pdf_dict_get(doc, s, "W"), *idx = pdf_dict_get(doc, s, "Index");
    double size = 0;
    pdf_num(pdf_dict_get(doc, s, "Size"), &size);
    if (!w || w->type != PO_ARRAY || w->n < 3) return 0;
    int W[3];
    for (int i = 0; i < 3; i++) { double v = 0; pdf_num(pdf_array_get(doc, w, (uint32_t)i), &v); W[i] = (int)v; }
    size_t len;
    const uint8_t *d = pdf_stream_data(doc, s, &len, 0);
    if (!d) return 0;
    int seen[1] = {0};
    size_t rec = (size_t)(W[0] + W[1] + W[2]), pos = 0;
    uint32_t nsec = idx && idx->type == PO_ARRAY ? idx->n / 2 : 1;
    for (uint32_t sct = 0; sct < nsec; sct++) {
        double st = 0, cnt = size;
        if (idx && idx->type == PO_ARRAY) { pdf_num(pdf_array_get(doc, idx, sct * 2), &st); pdf_num(pdf_array_get(doc, idx, sct * 2 + 1), &cnt); }
        for (uint32_t i = 0; i < (uint32_t)cnt && pos + rec <= len; i++, pos += rec) {
            uint64_t f[3] = {W[0] ? 0 : 1, 0, 0};
            size_t q = pos;
            for (int k = 0; k < 3; k++) {
                if (!W[k]) continue;
                f[k] = 0;
                for (int b = 0; b < W[k]; b++) f[k] = f[k] << 8 | d[q++];
            }
            set_x(doc, (uint32_t)st + i, (int)f[0], (uint32_t)f[1], (uint32_t)f[2], seen);
        }
    }
    return s->v.stream.dict;
}
static size_t find_startxref(pdf_doc *doc) {
    size_t from = doc->n > 2048 ? doc->n - 2048 : 0, at = (size_t)-1, p = from;
    while ((p = find_bytes(doc->d, doc->n, "startxref", p)) != (size_t)-1) at = p++;
    if (at == (size_t)-1) return (size_t)-1;
    pdf_lexer L = {doc, doc->d, doc->n, at + 9, 0, 0, 0, 0};
    skip_ws(&L);
    pdf_obj *o = lex_word(&L);
    return o && o->type == PO_INT && (size_t)o->v.i < doc->n ? (size_t)o->v.i : (size_t)-1;
}
static int read_xref_chain(pdf_doc *doc, size_t p) {
    int guard = 0;
    pdf_obj *first = 0;
    while (p != (size_t)-1 && p < doc->n && guard++ < 256) {
        pdf_lexer L = {doc, doc->d, doc->n, p, 0, 0, 0, 0};
        skip_ws(&L);
        pdf_obj *tr;
        if (L.p + 4 <= doc->n && !memcmp(doc->d + L.p, "xref", 4)) {
            tr = read_xref_table(doc, L.p);
            if (!tr) return first != 0;
            double xs;
            if (pdf_num(pdf_dict_get_raw(tr, "XRefStm"), &xs)) read_xref_stream(doc, (size_t)xs);   /* híbrido */
        } else tr = read_xref_stream(doc, L.p);
        if (!tr) return first != 0;
        if (!first) first = tr;
        double prev;
        p = pdf_num(pdf_dict_get_raw(tr, "Prev"), &prev) && (size_t)prev != p ? (size_t)prev : (size_t)-1;
    }
    doc->trailer = first;
    return first != 0;
}
/* reparo: varre "N G obj" no arquivo; trailer = o último dicionário com /Root (trailer ou xref stream) */
static int repair(pdf_doc *doc) {
    for (uint32_t i = 0; i < doc->nx; i++) memset(&doc->x[i], 0, sizeof doc->x[i]);
    pdf_obj *root_trailer = 0;
    for (size_t p = 0; p + 7 < doc->n; p++) {
        if (doc->d[p] < '0' || doc->d[p] > '9' || (p && !is_ws(doc->d[p - 1]) && !is_delim(doc->d[p - 1]))) {
            if (doc->d[p] == 't' && p + 7 <= doc->n && !memcmp(doc->d + p, "trailer", 7)) {
                pdf_lexer L = {doc, doc->d, doc->n, p + 7, 0, 0, 0, 0};
                pdf_obj *t = parse_obj(&L, 0);
                if (t && t->type == PO_DICT && pdf_dict_get_raw(t, "Root")) root_trailer = t;
            }
            continue;
        }
        size_t q = p;
        uint32_t num = 0;
        while (q < doc->n && doc->d[q] >= '0' && doc->d[q] <= '9') num = num * 10 + (uint32_t)(doc->d[q++] - '0');
        if (q >= doc->n || !is_ws(doc->d[q])) continue;
        while (q < doc->n && is_ws(doc->d[q])) q++;
        size_t g0 = q;
        while (q < doc->n && doc->d[q] >= '0' && doc->d[q] <= '9') q++;
        if (q == g0 || q >= doc->n) continue;
        while (q < doc->n && is_ws(doc->d[q])) q++;
        if (q + 3 > doc->n || memcmp(doc->d + q, "obj", 3)) continue;
        if (!grow_x(doc, num + 1)) continue;
        doc->x[num].type = 1;                       /* a última definição no arquivo é a que vale */
        doc->x[num].off = (uint32_t)p;
        doc->x[num].obj = 0;
        p = q + 2;
    }
    /* xref streams e object streams achados na varredura */
    for (uint32_t i = 0; i < doc->nx; i++) {
        if (doc->x[i].type != 1) continue;
        pdf_obj *o = load_object(doc, i);
        const char *t = pdf_name(pdf_dict_get(doc, o, "Type"));
        if (t && !strcmp(t, "XRef") && pdf_dict_get_raw(o, "Root")) root_trailer = o->v.stream.dict;
        if (t && !strcmp(t, "ObjStm")) {
            double nn = 0, first = 0;
            pdf_num(pdf_dict_get(doc, o, "N"), &nn);
            pdf_num(pdf_dict_get(doc, o, "First"), &first);
            size_t len;
            const uint8_t *d = pdf_stream_data(doc, o, &len, 0);
            pdf_lexer L = {doc, d, len, 0, 0, 0, 0, 0};
            for (uint32_t k = 0; d && k < (uint32_t)nn; k++) {
                skip_ws(&L);
                pdf_obj *a = lex_word(&L);
                skip_ws(&L);
                pdf_obj *b = lex_word(&L);
                if (!a || !b || a->type != PO_INT) break;
                uint32_t on = (uint32_t)a->v.i;
                if (grow_x(doc, on + 1) && !doc->x[on].type) { doc->x[on].type = 2; doc->x[on].off = i; doc->x[on].stm = k; }
            }
        }
    }
    if (!root_trailer) {
        /* sem trailer: procura um catálogo e monta um */
        for (uint32_t i = 0; i < doc->nx; i++) {
            pdf_obj *o = doc->x[i].type ? load_object(doc, i) : 0;
            const char *t = pdf_name(pdf_dict_get(doc, o, "Type"));
            if (t && !strcmp(t, "Catalog")) {
                root_trailer = mk(doc, PO_DICT);
                if (!root_trailer) return 0;
                root_trailer->n = 1;
                root_trailer->v.dict.keys = pdf_alloc(doc, sizeof(char *));
                root_trailer->v.dict.vals = pdf_alloc(doc, sizeof(pdf_obj *));
                root_trailer->v.dict.keys[0] = dupstr(doc, "Root", 4);
                pdf_obj *r = mk(doc, PO_REF);
                r->v.ref.num = i;
                root_trailer->v.dict.vals[0] = r;
                break;
            }
        }
    }
    doc->trailer = root_trailer;
    return root_trailer != 0;
}
static pdf_obj *objstm_object(pdf_doc *doc, uint32_t stmnum, uint32_t idx, uint32_t want) {
    pdf_obj *s = load_object(doc, stmnum);
    if (!s || s->type != PO_STREAM) return 0;
    double nn = 0, first = 0;
    pdf_num(pdf_dict_get(doc, s, "N"), &nn);
    pdf_num(pdf_dict_get(doc, s, "First"), &first);
    size_t len;
    const uint8_t *d = pdf_stream_data(doc, s, &len, 0);
    if (!d) return 0;
    pdf_lexer L = {doc, d, len, 0, 0, 0, 0, 0};
    uint32_t off = 0;
    int found = 0;
    for (uint32_t k = 0; k < (uint32_t)nn; k++) {
        skip_ws(&L);
        pdf_obj *a = lex_word(&L);
        skip_ws(&L);
        pdf_obj *b = lex_word(&L);
        if (!a || !b || a->type != PO_INT || b->type != PO_INT) break;
        if ((uint32_t)a->v.i == want || (k == idx && !found)) { off = (uint32_t)b->v.i; found = 1; if ((uint32_t)a->v.i == want) break; }
    }
    if (!found || (size_t)first + off >= len) return 0;
    L.p = (size_t)first + off;
    return parse_obj(&L, 0);                        /* objetos de object stream não são cifrados à parte */
}

/* ---------------------------------------------------------------- páginas */
static void collect_pages(pdf_doc *doc, pdf_obj *node, int depth, int *cap) {
    if (!node || depth > 64) return;
    const char *t = pdf_name(pdf_dict_get(doc, node, "Type"));
    pdf_obj *kids = pdf_dict_get(doc, node, "Kids");
    if (kids && kids->type == PO_ARRAY && (!t || strcmp(t, "Page"))) {
        for (uint32_t i = 0; i < kids->n; i++) {
            pdf_obj *k = pdf_resolve(doc, kids->v.arr[i]);
            if (k == node) continue;
            int dup = 0;                               /* ciclos: a mesma página/nó duas vezes */
            for (int j = 0; j < doc->npages && !dup; j++) dup = doc->pages[j] == k;
            if (!dup) collect_pages(doc, k, depth + 1, cap);
        }
        return;
    }
    if (node->type != PO_DICT) return;
    if (doc->npages == *cap) {
        int nc = *cap ? *cap * 2 : 16;
        pdf_obj **np = realloc(doc->pages, (size_t)nc * sizeof *np);
        if (!np) return;
        doc->pages = np;
        *cap = nc;
    }
    doc->pages[doc->npages++] = node;
}
static void ensure_pages(pdf_doc *doc) {
    if (doc->npages >= 0) return;
    doc->npages = 0;
    free(doc->pages);
    doc->pages = 0;
    int cap = 0;
    collect_pages(doc, pdf_dict_get(doc, pdf_catalog(doc), "Pages"), 0, &cap);
}
int pdf_page_count(pdf_doc *doc) { ensure_pages(doc); return doc->npages; }
pdf_obj *pdf_page(pdf_doc *doc, int i) { ensure_pages(doc); return i >= 0 && i < doc->npages ? doc->pages[i] : 0; }
pdf_obj *pdf_page_attr(pdf_doc *doc, pdf_obj *page, const char *key) {
    for (int g = 0; page && g < 64; g++) {
        pdf_obj *v = pdf_dict_get(doc, page, key);
        if (v) return v;
        page = pdf_dict_get(doc, page, "Parent");
    }
    return 0;
}
int pdf_page_contents(pdf_doc *doc, pdf_obj *page, uint8_t **out, size_t *len) {
    pdf_obj *c = pdf_dict_get(doc, page, "Contents");
    *out = 0;
    *len = 0;
    if (!c) return 0;
    int n = c->type == PO_ARRAY ? (int)c->n : 1;
    size_t cap = 0, l = 0;
    uint8_t *b = 0;
    for (int i = 0; i < n; i++) {
        pdf_obj *s = c->type == PO_ARRAY ? pdf_resolve(doc, c->v.arr[i]) : c;
        size_t sl;
        const uint8_t *d = pdf_stream_data(doc, s, &sl, 0);
        if (!d) continue;
        if (l + sl + 1 > cap) { cap = (l + sl + 1) * 2; uint8_t *nb = realloc(b, cap); if (!nb) break; b = nb; }
        memcpy(b + l, d, sl);
        l += sl;
        b[l++] = 10;                                  /* os streams se juntam com um separador */
    }
    *out = b;
    *len = l;
    return 0;
}

/* ---------------------------------------------------------------- abrir/fechar */
pdf_doc *pdf_open(const uint8_t *d, size_t n) {
    if (!d || n < 8) return 0;
    size_t hdr = find_bytes(d, n < 1024 ? n : 1024, "%PDF-", 0);
    if (hdr == (size_t)-1) return 0;
    pdf_doc *doc = calloc(1, sizeof *doc);
    if (!doc) return 0;
    doc->d = malloc(n);
    if (!doc->d) { free(doc); return 0; }
    memcpy(doc->d, d, n);
    doc->n = n;
    doc->npages = -1;
    doc->major = d[hdr + 5] - '0';
    doc->minor = hdr + 7 < n && d[hdr + 6] == '.' ? d[hdr + 7] - '0' : 0;
    size_t sx = find_startxref(doc);
    int ok = sx != (size_t)-1 && read_xref_chain(doc, sx);
    if (ok && !pdf_catalog(doc)) ok = 0;
    if (!ok && !repair(doc)) { pdf_close(doc); return 0; }
    /* versão do catálogo pode subir a do cabeçalho */
    const char *cv = pdf_name(pdf_dict_get(doc, pdf_catalog(doc), "Version"));
    if (cv && cv[0] >= '1' && cv[0] <= '9' && cv[1] == '.') {
        int ma = cv[0] - '0', mi = cv[2] - '0';
        if (ma > doc->major || (ma == doc->major && mi > doc->minor)) { doc->major = ma; doc->minor = mi; }
    }
    setup_encrypt(doc);
    if (pdf_page_count(doc) == 0 && !doc->encrypted) {
        /* xref enganosa (aponta para páginas que não existem): tenta o reparo */
        pdf_obj *keep = doc->trailer;
        if (repair(doc)) { doc->npages = -1; setup_encrypt(doc); }
        else doc->trailer = keep;
    }
    return doc;
}
void pdf_close(pdf_doc *doc) {
    if (!doc) return;
    pdf_chunk *c = doc->arena;
    while (c) { pdf_chunk *n = c->next; free(c); c = n; }
    free(doc->x);
    free(doc->pages);
    free(doc->d);
    free(doc);
}
