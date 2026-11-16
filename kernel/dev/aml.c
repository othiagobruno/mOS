/* mOS — interpretador AML (ACPI 6.5, capítulos 19 e 20)
 *
 * Executa o bytecode direto da tabela (sem árvore intermediária). Na carga, o código de nível de tabela
 * roda no escopo raiz: Name/Method/Device/OperationRegion/Field criam nós; corpos de Method só guardam
 * o ponteiro. Métodos rodam com 8 Locals e 7 Args; nomes criados dentro de um método somem no retorno.
 * Strings, Buffers e Packages são blocos com contagem de referências: ler um nome compartilha o bloco
 * (Index/CreateField enxergam o objeto real); Store em Local ou em nome copia, como a especificação pede.
 */
#include "aml.h"
#ifdef AML_HOST
#include <string.h>
#else
#include "kernel.h"
#endif

/* ------------------------------------------------------------------ estado */
#define MAX_DEPTH   48
#define MAX_SEGS    32
#define LOOP_MAX    (1u << 22)

enum { ST_OK = 0, ST_RET, ST_BREAK, ST_CONT, ST_ERR };

typedef struct frame {
    aml_val_t loc[8], arg[7];
    aml_node_t *mnode;
    aml_node_t **dyn; u32 ndyn, capdyn;
} frame_t;

typedef struct ex {
    const u8 *p, *end;
    aml_node_t *scope;
    frame_t *f;              /* NULL durante a carga da tabela */
    aml_val_t ret;
    int err;
} ex_t;

static aml_node_t *g_root;
static u64 g_mask = ~0ull;   /* inteiros de 32 bits se a DSDT tiver revisão < 2 */
static u32 g_bits = 64;
static u32 g_nodes, g_errs, g_depth;
static u32 g_ddb;

typedef struct { u8 root, up, n; const u8 *segs; } aml_name_t;

static void *zalloc(u32 n) { void *p = aml_os_alloc(n ? n : 1); if (p) memset(p, 0, n ? n : 1); return p; }

static int fail(ex_t *x, int code, const char *why) {
    if (x && !x->err) x->err = code;
    g_errs++;
    if (g_errs < 64) aml_os_log("aml: %s\n", why);
    return ST_ERR;
}

const char *aml_strerror(int e) {
    switch (e) {
    case AML_OK: return "ok";
    case AML_ENOTFOUND: return "nome não encontrado";
    case AML_EINVAL: return "bytecode inválido";
    case AML_ENOMEM: return "sem memória";
    case AML_EFAULT: return "acesso de região falhou";
    case AML_EDEPTH: return "profundidade demais";
    case AML_ETYPE: return "tipo incompatível";
    }
    return "erro";
}
u32 aml_errors(void) { return g_errs; }
u32 aml_int_bits(void) { return g_bits; }
u32 aml_node_count(void) { return g_nodes; }
aml_node_t *aml_root(void) { return g_root; }

/* ------------------------------------------------------------------ valores */
static aml_blob_t *blob_new(u8 type, u32 len) {
    u32 extra = type == AML_PKG ? len * (u32)sizeof(aml_val_t) : len + 1;
    aml_blob_t *b = zalloc((u32)sizeof(aml_blob_t) + extra);
    if (!b) return NULL;
    b->refs = 1; b->len = len;
    if (type == AML_PKG) b->el = (aml_val_t *)(b + 1); else b->d = (u8 *)(b + 1);
    return b;
}

static void blob_put(aml_blob_t *b);
void aml_val_free(aml_val_t *v) {
    if (!v) return;
    if (v->type == AML_STR || v->type == AML_BUF || v->type == AML_PKG ||
        (v->type == AML_REF && (v->rkind == AML_REF_PKG || v->rkind == AML_REF_BUF || v->rkind == AML_REF_STR)))
        blob_put(v->b);
    memset(v, 0, sizeof *v);
}
static void blob_put(aml_blob_t *b) {
    if (!b || --b->refs) return;
    if (b->el) for (u32 i = 0; i < b->len; i++) aml_val_free(&b->el[i]);
    aml_os_free(b);
}
static bool has_blob(const aml_val_t *v) {
    return v->type == AML_STR || v->type == AML_BUF || v->type == AML_PKG ||
           (v->type == AML_REF && (v->rkind == AML_REF_PKG || v->rkind == AML_REF_BUF || v->rkind == AML_REF_STR));
}
static void v_dup(aml_val_t *d, const aml_val_t *s) { *d = *s; if (has_blob(s)) d->b->refs++; }
aml_val_t aml_int(u64 v) { aml_val_t r; memset(&r, 0, sizeof r); r.type = AML_INT; r.i = v; return r; }
static void v_int(aml_val_t *d, u64 v) { memset(d, 0, sizeof *d); d->type = AML_INT; d->i = v & g_mask; }
static int v_str(aml_val_t *d, const char *s, u32 n) {
    memset(d, 0, sizeof *d);
    aml_blob_t *b = blob_new(AML_STR, n);
    if (!b) return AML_ENOMEM;
    memcpy(b->d, s, n); b->d[n] = 0;
    d->type = AML_STR; d->b = b; return AML_OK;
}
static int v_buf(aml_val_t *d, const u8 *s, u32 n) {
    memset(d, 0, sizeof *d);
    aml_blob_t *b = blob_new(AML_BUF, n);
    if (!b) return AML_ENOMEM;
    if (s) memcpy(b->d, s, n);
    d->type = AML_BUF; d->b = b; return AML_OK;
}
static void v_ref_node(aml_val_t *d, aml_node_t *n) { memset(d, 0, sizeof *d); d->type = AML_REF; d->rkind = AML_REF_NODE; d->node = n; }

/* cópia profunda (Store/CopyObject) */
static int v_copy(aml_val_t *d, const aml_val_t *s) {
    if (s->type == AML_STR) return v_str(d, (const char *)s->b->d, s->b->len);
    if (s->type == AML_BUF) return v_buf(d, s->b->d, s->b->len);
    if (s->type == AML_PKG) {
        memset(d, 0, sizeof *d);
        aml_blob_t *b = blob_new(AML_PKG, s->b->len);
        if (!b) return AML_ENOMEM;
        d->type = AML_PKG; d->b = b;
        for (u32 i = 0; i < s->b->len; i++) if (v_copy(&b->el[i], &s->b->el[i])) return AML_ENOMEM;
        return AML_OK;
    }
    v_dup(d, s);
    return AML_OK;
}

/* ------------------------------------------------------------------ namespace */
static aml_node_t *node_new(aml_node_t *parent, const char *seg, u8 type) {
    aml_node_t *n = zalloc(sizeof *n);
    if (!n) return NULL;
    memcpy(n->name, seg, 4);
    n->type = type;
    n->parent = parent;
    if (parent) {   /* mantém a ordem de definição */
        aml_node_t **pp = &parent->child;
        while (*pp) pp = &(*pp)->next;
        *pp = n;
    }
    g_nodes++;
    return n;
}

static void node_free_tree(aml_node_t *n) {
    while (n->child) { aml_node_t *c = n->child; n->child = c->next; node_free_tree(c); }
    aml_val_free(&n->v);
    if (n->type == AML_BUFFIELD && n->u.bf.buf) blob_put(n->u.bf.buf);
    g_nodes--;
    aml_os_free(n);
}
static void node_unlink(aml_node_t *n) {
    if (!n->parent) return;
    for (aml_node_t **pp = &n->parent->child; *pp; pp = &(*pp)->next)
        if (*pp == n) { *pp = n->next; break; }
    n->parent = NULL;
}

static aml_node_t *child_find(aml_node_t *s, const u8 *seg) {
    for (aml_node_t *c = s ? s->child : NULL; c; c = c->next)
        if (!memcmp(c->name, seg, 4)) return c;
    return NULL;
}
static aml_node_t *unalias(aml_node_t *n) {
    for (int i = 0; n && n->type == AML_ALIAS && i < 8; i++) n = n->u.alias;
    return n;
}

static aml_node_t *ns_resolve(aml_node_t *scope, const aml_name_t *nm) {
    aml_node_t *s = nm->root ? g_root : scope;
    for (int i = 0; i < nm->up; i++) { if (!s->parent) return NULL; s = s->parent; }
    if (!nm->n) return s;
    if (!nm->root && !nm->up && nm->n == 1) {              /* regra de busca subindo os escopos */
        for (aml_node_t *t = s; t; t = t->parent) { aml_node_t *c = child_find(t, nm->segs); if (c) return unalias(c); }
        return NULL;
    }
    for (int i = 0; i < nm->n && s; i++) s = unalias(child_find(s, nm->segs + 4 * i));
    return s;
}

/* cria (ou devolve, se já existe) o nó do nome; o prefixo tem de existir */
static aml_node_t *ns_create(ex_t *x, const aml_name_t *nm, u8 type, bool *existed) {
    aml_node_t *s = nm->root ? g_root : x->scope;
    if (existed) *existed = false;
    for (int i = 0; i < nm->up; i++) { if (!s->parent) return NULL; s = s->parent; }
    if (!nm->n) return s;
    for (int i = 0; i + 1 < nm->n; i++) {
        aml_node_t *c = unalias(child_find(s, nm->segs + 4 * i));
        if (!c) return NULL;
        s = c;
    }
    const u8 *last = nm->segs + 4 * (nm->n - 1);
    aml_node_t *c = child_find(s, last);
    if (c) { if (existed) *existed = true; return c; }
    c = node_new(s, (const char *)last, type);
    if (c && x->f) {                                        /* objeto dinâmico do método */
        c->dynamic = 1;
        frame_t *f = x->f;
        if (f->ndyn == f->capdyn) {
            u32 nc = f->capdyn ? f->capdyn * 2 : 8;
            aml_node_t **nd = zalloc(nc * (u32)sizeof(*nd));
            if (!nd) return c;
            if (f->dyn) { memcpy(nd, f->dyn, f->ndyn * sizeof(*nd)); aml_os_free(f->dyn); }
            f->dyn = nd; f->capdyn = nc;
        }
        f->dyn[f->ndyn++] = c;
    }
    return c;
}

static int parse_cpath(const char *path, u8 *segs, aml_name_t *nm) {
    memset(nm, 0, sizeof *nm);
    const char *p = path;
    if (*p == '\\') { nm->root = 1; p++; }
    while (*p == '^') { nm->up++; p++; }
    while (*p && nm->n < MAX_SEGS) {
        u8 *s = segs + 4 * nm->n;
        int k = 0;
        while (*p && *p != '.' && k < 4) s[k++] = (u8)*p++;
        while (k < 4) s[k++] = '_';
        nm->n++;
        if (*p == '.') p++;
        else if (*p) return AML_EINVAL;
    }
    nm->segs = segs;
    return AML_OK;
}

aml_node_t *aml_find(aml_node_t *scope, const char *path) {
    u8 segs[4 * MAX_SEGS];
    aml_name_t nm;
    if (!g_root || parse_cpath(path, segs, &nm)) return NULL;
    if (!scope) scope = g_root;
    if (!nm.root && !nm.up) {   /* relativo exato, sem a regra de busca */
        aml_node_t *s = scope;
        for (int i = 0; i < nm.n && s; i++) s = unalias(child_find(s, segs + 4 * i));
        return s;
    }
    return ns_resolve(scope, &nm);
}

void aml_path(aml_node_t *n, char *buf, u32 size) {
    aml_node_t *st[64]; int k = 0;
    for (aml_node_t *t = n; t && t != g_root && k < 64; t = t->parent) st[k++] = t;
    u32 o = 0;
    if (size) buf[o++] = '\\';
    for (int i = k - 1; i >= 0 && o + 5 < size; i--) {
        memcpy(buf + o, st[i]->name, 4); o += 4;
        if (i) buf[o++] = '.';
    }
    if (o < size) buf[o] = 0; else if (size) buf[size - 1] = 0;
}

static void walk_rec(aml_node_t *n, int d, int (*cb)(aml_node_t *, int, void *), void *ctx, int *stop) {
    for (aml_node_t *c = n->child; c && !*stop; c = c->next) {
        int r = cb(c, d, ctx);
        if (r < 0) { *stop = 1; return; }
        if (r == 0) walk_rec(c, d + 1, cb, ctx, stop);
    }
}
/* cb devolve 0 = desce, 1 = não desce, <0 = para */
void aml_walk(aml_node_t *start, int (*cb)(aml_node_t *n, int depth, void *ctx), void *ctx) {
    int stop = 0;
    walk_rec(start ? start : g_root, 0, cb, ctx, &stop);
}

/* ------------------------------------------------------------------ decodificação */
static bool is_lead(u8 c) { return (c >= 'A' && c <= 'Z') || c == '_' || c == '\\' || c == '^' || c == 0x2E || c == 0x2F; }

static int pkg_end(ex_t *x, const u8 **end) {
    const u8 *s = x->p;
    if (s >= x->end) return fail(x, AML_EINVAL, "PkgLength fora da tabela");
    u8 lead = *x->p++;
    u32 nb = lead >> 6, len;
    if (!nb) len = lead & 0x3F;
    else {
        len = lead & 0x0F;
        for (u32 i = 0; i < nb; i++) len |= (u32)(*x->p++) << (4 + 8 * i);
    }
    *end = s + len;
    if (*end > x->end || *end < x->p) return fail(x, AML_EINVAL, "PkgLength inválido");
    return ST_OK;
}
static u32 pkg_raw(ex_t *x) {
    u8 lead = *x->p++;
    u32 nb = lead >> 6, len;
    if (!nb) return lead & 0x3F;
    len = lead & 0x0F;
    for (u32 i = 0; i < nb; i++) len |= (u32)(*x->p++) << (4 + 8 * i);
    return len;
}

static int parse_name(ex_t *x, aml_name_t *nm) {
    memset(nm, 0, sizeof *nm);
    if (x->p < x->end && *x->p == '\\') { nm->root = 1; x->p++; }
    else while (x->p < x->end && *x->p == '^') { nm->up++; x->p++; }
    if (x->p >= x->end) return fail(x, AML_EINVAL, "nome truncado");
    u8 c = *x->p;
    if (c == 0x00) { x->p++; nm->n = 0; }
    else if (c == 0x2E) { x->p++; nm->n = 2; }
    else if (c == 0x2F) { x->p++; nm->n = *x->p++; }
    else nm->n = 1;
    nm->segs = x->p;
    x->p += 4 * nm->n;
    if (x->p > x->end) return fail(x, AML_EINVAL, "nome truncado");
    return ST_OK;
}

static void name_str(const aml_name_t *nm, char *buf, u32 size) {
    u32 o = 0;
    if (nm->root && o + 1 < size) buf[o++] = '\\';
    for (int i = 0; i < nm->up && o + 1 < size; i++) buf[o++] = '^';
    for (int i = 0; i < nm->n && o + 6 < size; i++) {
        memcpy(buf + o, nm->segs + 4 * i, 4); o += 4;
        if (i + 1 < nm->n) buf[o++] = '.';
    }
    buf[o < size ? o : size - 1] = 0;
}

/* ------------------------------------------------------------------ conversões */
static u32 int_bytes(void) { return g_bits / 8; }

static int hexval(u8 c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static int deref_read(ex_t *x, const aml_val_t *r, aml_val_t *out);

/* conversão implícita para inteiro (19.3.5.7): String em hexadecimal, Buffer little-endian */
static int to_int(ex_t *x, const aml_val_t *v, u64 *out) {
    switch (v->type) {
    case AML_INT: *out = v->i & g_mask; return ST_OK;
    case AML_UNINIT: *out = 0; return ST_OK;
    case AML_STR: {
        u64 r = 0; const u8 *s = v->b->d; u32 i = 0;
        while (i < v->b->len && (s[i] == ' ' || s[i] == '\t')) i++;
        if (i + 1 < v->b->len && s[i] == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X')) i += 2;
        for (u32 k = 0; i < v->b->len && k < g_bits / 4; i++, k++) { int h = hexval(s[i]); if (h < 0) break; r = (r << 4) | (u64)h; }
        *out = r & g_mask; return ST_OK;
    }
    case AML_BUF: {
        u64 r = 0; u32 n = v->b->len < int_bytes() ? v->b->len : int_bytes();
        for (u32 i = 0; i < n; i++) r |= (u64)v->b->d[i] << (8 * i);
        *out = r; return ST_OK;
    }
    case AML_REF: {
        if (v->rkind == AML_REF_NODE) {
            aml_node_t *n = v->node;
            if (n->type != AML_INT && n->type != AML_STR && n->type != AML_BUF && n->type != AML_FIELD &&
                n->type != AML_BUFFIELD && n->type != AML_UNINIT && n->type != AML_METHOD)
                return fail(x, AML_ETYPE, "objeto não converte para inteiro");
        }
        aml_val_t t; int st = deref_read(x, v, &t);
        if (st) return st;
        if (t.type == AML_REF) { aml_val_free(&t); return fail(x, AML_ETYPE, "referência dupla"); }
        st = to_int(x, &t, out);
        aml_val_free(&t);
        return st;
    }
    }
    return fail(x, AML_ETYPE, "Package não converte para inteiro");
}

/* ToInteger explícito: "0x..." hexadecimal, senão decimal */
static int to_int_explicit(ex_t *x, const aml_val_t *v, u64 *out) {
    if (v->type != AML_STR) return to_int(x, v, out);
    const u8 *s = v->b->d; u32 i = 0, n = v->b->len; u64 r = 0;
    while (i < n && (s[i] == ' ' || s[i] == '\t')) i++;
    if (i + 1 < n && s[i] == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X')) {
        for (i += 2; i < n; i++) { int h = hexval(s[i]); if (h < 0) break; r = (r << 4) | (u64)h; }
    } else {
        for (; i < n && s[i] >= '0' && s[i] <= '9'; i++) r = r * 10 + (u64)(s[i] - '0');
    }
    *out = r & g_mask;
    return ST_OK;
}

static int to_buf(ex_t *x, const aml_val_t *v, aml_val_t *out) {
    switch (v->type) {
    case AML_BUF: v_dup(out, v); return ST_OK;
    case AML_INT: case AML_UNINIT: {
        u8 b[8]; u64 i = v->type == AML_INT ? v->i : 0;
        for (int k = 0; k < 8; k++) b[k] = (u8)(i >> (8 * k));
        return v_buf(out, b, int_bytes()) ? fail(x, AML_ENOMEM, "sem memória") : ST_OK;
    }
    case AML_STR:   /* com o NUL, como o ACPICA (há AML que depende disso) */
        return v_buf(out, v->b->d, v->b->len ? v->b->len + 1 : 0) ? fail(x, AML_ENOMEM, "sem memória") : ST_OK;
    case AML_REF: {
        aml_val_t t; int st = deref_read(x, v, &t);
        if (st) return st;
        if (t.type == AML_REF) { aml_val_free(&t); return fail(x, AML_ETYPE, "referência dupla"); }
        st = to_buf(x, &t, out);
        aml_val_free(&t);
        return st;
    }
    }
    return fail(x, AML_ETYPE, "Package não converte para Buffer");
}

static u32 put_hex(char *d, u64 v, int digits) {
    static const char H[] = "0123456789ABCDEF";
    for (int i = digits - 1; i >= 0; i--) { d[i] = H[v & 15]; v >>= 4; }
    return (u32)digits;
}
static u32 put_dec(char *d, u64 v) {
    char t[24]; int k = 0;
    do { t[k++] = (char)('0' + v % 10); v /= 10; } while (v);
    for (int i = 0; i < k; i++) d[i] = t[k - 1 - i];
    return (u32)k;
}

/* mode 0 = implícita (hex fixo / bytes "XX XX"), 1 = ToHexString, 2 = ToDecimalString */
static int to_str(ex_t *x, const aml_val_t *v, aml_val_t *out, int mode) {
    char tmp[64];
    switch (v->type) {
    case AML_STR: v_dup(out, v); return ST_OK;
    case AML_INT: case AML_UNINIT: {
        u64 i = v->type == AML_INT ? v->i : 0; u32 n;
        if (mode == 2) n = put_dec(tmp, i);
        else if (mode == 1) { tmp[0] = '0'; tmp[1] = 'x'; n = 2 + put_hex(tmp + 2, i, (int)int_bytes() * 2); }
        else n = put_hex(tmp, i, (int)int_bytes() * 2);
        return v_str(out, tmp, n) ? fail(x, AML_ENOMEM, "sem memória") : ST_OK;
    }
    case AML_BUF: {
        u32 len = v->b->len, cap = len * 5 + 1;
        char *s = zalloc(cap); u32 o = 0;
        if (!s) return fail(x, AML_ENOMEM, "sem memória");
        for (u32 i = 0; i < len; i++) {
            if (mode == 2) { if (i) s[o++] = ','; o += put_dec(s + o, v->b->d[i]); }
            else if (mode == 1) { if (i) s[o++] = ','; s[o++] = '0'; s[o++] = 'x'; o += put_hex(s + o, v->b->d[i], 2); }
            else { if (i) s[o++] = ' '; o += put_hex(s + o, v->b->d[i], 2); }
        }
        int r = v_str(out, s, o);
        aml_os_free(s);
        return r ? fail(x, AML_ENOMEM, "sem memória") : ST_OK;
    }
    case AML_REF: {
        aml_val_t t; int st = deref_read(x, v, &t);
        if (st) return st;
        if (t.type == AML_REF) { aml_val_free(&t); return fail(x, AML_ETYPE, "referência dupla"); }
        st = to_str(x, &t, out, mode);
        aml_val_free(&t);
        return st;
    }
    }
    return fail(x, AML_ETYPE, "Package não converte para String");
}

int aml_val_to_int(const aml_val_t *v, u64 *out) {
    ex_t x; memset(&x, 0, sizeof x);
    return to_int(&x, v, out) ? (x.err ? x.err : AML_ETYPE) : AML_OK;
}

/* ------------------------------------------------------------------ regiões e campos */
static int eval_node(ex_t *x, aml_node_t *n, const aml_val_t *args, int nargs, aml_val_t *ret);

static bool is_root_bridge(aml_node_t *d) {
    return aml_device_match(d, "PNP0A03") || aml_device_match(d, "PNP0A08");
}

/* segmento/barramento/dispositivo/função da região PCI_Config (como o AcpiHwDerivePciId) */
static int pci_locate(ex_t *x, aml_node_t *r) {
    aml_node_t *chain[32]; int k = 0;
    for (aml_node_t *t = r->parent; t && t != g_root && k < 32; t = t->parent)
        if (t->type == AML_DEVICE) chain[k++] = t;
    int rb = -1;
    for (int i = 0; i < k; i++) if (is_root_bridge(chain[i])) { rb = i; break; }
    if (rb < 0) return fail(x, AML_ENOTFOUND, "região PCI sem ponte raiz");
    u64 seg = 0, bus = 0, adr = 0;
    aml_eval_int(chain[rb], "_SEG", &seg);
    aml_eval_int(chain[rb], "_BBN", &bus);
    u8 dev = 0, fn = 0;
    if (rb == 0) { if (!aml_eval_int(chain[0], "_ADR", &adr)) { dev = (u8)(adr >> 16); fn = (u8)adr; } }
    else {
        for (int i = rb - 1; i >= 0; i--) {
            adr = 0;
            aml_eval_int(chain[i], "_ADR", &adr);
            dev = (u8)(adr >> 16); fn = (u8)adr;
            if (i > 0) {   /* ponte PCI-PCI intermediária: o barramento de baixo é o secundário dela */
                u64 hdr = 0, sec = 0;
                aml_os_pci((u16)seg, (u8)bus, dev, fn, 0x0E, 8, &hdr, false);
                if ((hdr & 0x7F) == 1 || (hdr & 0x7F) == 2) {
                    aml_os_pci((u16)seg, (u8)bus, dev, fn, 0x19, 8, &sec, false);
                    bus = sec;
                }
            }
        }
    }
    r->u.r.seg = (u16)seg; r->u.r.bus = (u8)bus; r->u.r.dev = dev; r->u.r.fn = fn; r->u.r.pci_ok = 1;
    return ST_OK;
}

static int region_rw(ex_t *x, aml_node_t *r, u64 off, u32 bits, u64 *v, bool write) {
    if (!r || r->type != AML_REGION) return fail(x, AML_ETYPE, "campo sem OperationRegion");
    u64 a = r->u.r.base + off;
    bool ok;
    if (!write) *v = 0;
    switch (r->u.r.space) {
    case AML_AS_MEM: ok = aml_os_mem(a, bits, v, write); break;
    case AML_AS_IO: ok = aml_os_io((u16)a, bits, v, write); break;
    case AML_AS_PCI:
        if (!r->u.r.pci_ok && pci_locate(x, r)) return ST_ERR;
        ok = aml_os_pci(r->u.r.seg, r->u.r.bus, r->u.r.dev, r->u.r.fn, (u16)a, bits, v, write);
        break;
    case AML_AS_EC: case AML_AS_CMOS: {
        ok = true;
        for (u32 i = 0; i < bits / 8 && ok; i++) {
            u8 b = write ? (u8)(*v >> (8 * i)) : 0;
            ok = r->u.r.space == AML_AS_EC ? aml_os_ec((u8)(a + i), &b, write) : aml_os_cmos((u8)(a + i), &b, write);
            if (!write) *v |= (u64)b << (8 * i);
        }
        break;
    }
    default: ok = aml_os_region_other(r->u.r.space, a, bits, v, write); break;
    }
    if (!ok) return fail(x, AML_EFAULT, "acesso à OperationRegion falhou");
    return ST_OK;
}

static u32 field_width(aml_node_t *f) {
    u32 at = f->u.f.flags & 0x0F;
    switch (at) {
    case 1: return 1; case 2: return 2; case 3: return 4; case 4: return 8; case 5: return 1;
    }
    /* AnyAcc: a menor unidade alinhada que contém o campo inteiro */
    u32 bo = f->u.f.bitoff, bl = f->u.f.bitlen;
    for (u32 w = 1; w <= 8; w <<= 1) if (bo / (w * 8) == (bo + bl - 1) / (w * 8)) return w;
    return 1;
}

static int field_read(ex_t *x, aml_node_t *f, aml_val_t *out);
static int field_write_bytes(ex_t *x, aml_node_t *f, const u8 *src, u32 srclen);

static int field_write_int(ex_t *x, aml_node_t *f, u64 v) {
    u8 b[8];
    for (int i = 0; i < 8; i++) b[i] = (u8)(v >> (8 * i));
    return field_write_bytes(x, f, b, 8);
}

static int unit_rw(ex_t *x, aml_node_t *f, u64 byteoff, u32 w, u64 *v, bool write) {
    switch (f->u.f.kind) {
    case 1: /* IndexField */
        if (field_write_int(x, f->u.f.idx, byteoff)) return ST_ERR;
        if (write) return field_write_int(x, f->u.f.dat, *v);
        {
            aml_val_t t; if (field_read(x, f->u.f.dat, &t)) return ST_ERR;
            int st = to_int(x, &t, v); aml_val_free(&t); return st;
        }
    case 2: /* BankField */
        if (field_write_int(x, f->u.f.bank, f->u.f.bankval)) return ST_ERR;
        return region_rw(x, f->u.f.reg, byteoff, w * 8, v, write);
    default:
        return region_rw(x, f->u.f.reg, byteoff, w * 8, v, write);
    }
}

static int field_read(ex_t *x, aml_node_t *f, aml_val_t *out) {
    u32 bo = f->u.f.bitoff, bl = f->u.f.bitlen, w = field_width(f), ub = w * 8;
    u32 nbytes = (bl + 7) / 8;
    u8 *dst = zalloc(nbytes < 8 ? 8 : nbytes);
    if (!dst) return fail(x, AML_ENOMEM, "sem memória");
    bool gl = (f->u.f.flags & 0x10) != 0;
    if (gl) aml_os_global_lock(true);
    int st = ST_OK;
    for (u64 u = (bo / ub) * (u64)ub; u < (u64)bo + bl; u += ub) {
        u64 val = 0;
        if ((st = unit_rw(x, f, u / 8, w, &val, false))) break;
        u64 lo = u > bo ? u : bo, hi = u + ub < (u64)bo + bl ? u + ub : (u64)bo + bl;
        for (u64 b = lo; b < hi; b++)
            if ((val >> (b - u)) & 1) dst[(b - bo) / 8] |= (u8)(1u << ((b - bo) % 8));
    }
    if (gl) aml_os_global_lock(false);
    if (!st) {
        if (bl <= g_bits) {
            u64 r = 0;
            for (u32 i = 0; i < 8; i++) r |= (u64)dst[i] << (8 * i);
            v_int(out, r);
        } else if (v_buf(out, dst, nbytes)) st = fail(x, AML_ENOMEM, "sem memória");
    }
    aml_os_free(dst);
    return st;
}

static int field_write_bytes(ex_t *x, aml_node_t *f, const u8 *src, u32 srclen) {
    u32 bo = f->u.f.bitoff, bl = f->u.f.bitlen, w = field_width(f), ub = w * 8;
    u32 rule = (f->u.f.flags >> 5) & 3;
    bool gl = (f->u.f.flags & 0x10) != 0;
    if (gl) aml_os_global_lock(true);
    int st = ST_OK;
    for (u64 u = (bo / ub) * (u64)ub; u < (u64)bo + bl; u += ub) {
        u64 lo = u > bo ? u : bo, hi = u + ub < (u64)bo + bl ? u + ub : (u64)bo + bl;
        u64 mask = 0, bits = 0;
        for (u64 b = lo; b < hi; b++) {
            u64 sb = b - bo;
            mask |= 1ull << (b - u);
            if (sb / 8 < srclen && ((src[sb / 8] >> (sb % 8)) & 1)) bits |= 1ull << (b - u);
        }
        u64 full = ub == 64 ? ~0ull : ((1ull << ub) - 1);
        u64 base = 0;
        if (mask != full) {
            if (rule == 0) { if ((st = unit_rw(x, f, u / 8, w, &base, false))) break; }
            else if (rule == 1) base = full;
        }
        u64 nv = ((base & ~mask) | bits) & full;
        if ((st = unit_rw(x, f, u / 8, w, &nv, true))) break;
    }
    if (gl) aml_os_global_lock(false);
    return st;
}

static int bytes_of(ex_t *x, const aml_val_t *v, aml_val_t *tmp, const u8 **p, u32 *n) {
    if (v->type == AML_INT || v->type == AML_UNINIT) {
        if (to_buf(x, v, tmp)) return ST_ERR;
        /* inteiro: 8 bytes mesmo com tabela de 32 bits (os altos ficam 0) */
    } else if (v->type == AML_BUF) v_dup(tmp, v);
    else if (v->type == AML_STR) { if (v_buf(tmp, v->b->d, v->b->len)) return fail(x, AML_ENOMEM, "sem memória"); }
    else if (v->type == AML_REF) {
        aml_val_t t; if (deref_read(x, v, &t)) return ST_ERR;
        int st = bytes_of(x, &t, tmp, p, n); aml_val_free(&t); return st;
    } else return fail(x, AML_ETYPE, "valor não cabe num campo");
    *p = tmp->b->d; *n = tmp->b->len;
    return ST_OK;
}

static int bf_read(ex_t *x, aml_node_t *f, aml_val_t *out) {
    aml_blob_t *b = f->u.bf.buf;
    u32 bo = f->u.bf.bitoff, bl = f->u.bf.bitlen;
    if (!b || (u64)bo + bl > (u64)b->len * 8) return fail(x, AML_EFAULT, "BufferField além do Buffer");
    u32 nb = (bl + 7) / 8;
    u8 *d = zalloc(nb < 8 ? 8 : nb);
    if (!d) return fail(x, AML_ENOMEM, "sem memória");
    for (u32 i = 0; i < bl; i++) { u32 s = bo + i; if ((b->d[s / 8] >> (s % 8)) & 1) d[i / 8] |= (u8)(1u << (i % 8)); }
    int st = ST_OK;
    if (bl <= g_bits) { u64 r = 0; for (u32 i = 0; i < 8; i++) r |= (u64)d[i] << (8 * i); v_int(out, r); }
    else if (v_buf(out, d, nb)) st = fail(x, AML_ENOMEM, "sem memória");
    aml_os_free(d);
    return st;
}
static int bf_write(ex_t *x, aml_node_t *f, const aml_val_t *v) {
    aml_blob_t *b = f->u.bf.buf;
    u32 bo = f->u.bf.bitoff, bl = f->u.bf.bitlen;
    if (!b || (u64)bo + bl > (u64)b->len * 8) return fail(x, AML_EFAULT, "BufferField além do Buffer");
    aml_val_t tmp; const u8 *s; u32 n;
    memset(&tmp, 0, sizeof tmp);
    if (bytes_of(x, v, &tmp, &s, &n)) return ST_ERR;
    for (u32 i = 0; i < bl; i++) {
        u32 t = bo + i;
        bool bit = i / 8 < n && ((s[i / 8] >> (i % 8)) & 1);
        if (bit) b->d[t / 8] |= (u8)(1u << (t % 8)); else b->d[t / 8] &= (u8)~(1u << (t % 8));
    }
    aml_val_free(&tmp);
    return ST_OK;
}

/* valor de um nó como fonte de dados */
static int read_node(ex_t *x, aml_node_t *n, aml_val_t *out) {
    memset(out, 0, sizeof *out);
    switch (n->type) {
    case AML_INT: case AML_STR: case AML_BUF: case AML_PKG: v_dup(out, &n->v); return ST_OK;
    case AML_UNINIT: if (n->v.type) { v_dup(out, &n->v); return ST_OK; } v_ref_node(out, n); return ST_OK;
    case AML_FIELD: return field_read(x, n, out);
    case AML_BUFFIELD: return bf_read(x, n, out);
    case AML_METHOD: return eval_node(x, n, NULL, 0, out);
    default: v_ref_node(out, n); return ST_OK;
    }
}

static int deref_read(ex_t *x, const aml_val_t *r, aml_val_t *out) {
    memset(out, 0, sizeof *out);
    if (r->type != AML_REF) { v_dup(out, r); return ST_OK; }
    switch (r->rkind) {
    case AML_REF_NODE: return read_node(x, r->node, out);
    case AML_REF_PKG: if (r->idx >= r->b->len) return fail(x, AML_EFAULT, "Index além do Package");
        v_dup(out, &r->b->el[r->idx]); return ST_OK;
    case AML_REF_BUF: if (r->idx >= r->b->len) return fail(x, AML_EFAULT, "Index além do Buffer");
        v_int(out, r->b->d[r->idx]); return ST_OK;
    case AML_REF_STR: if (r->idx >= r->b->len) return fail(x, AML_EFAULT, "Index além da String");
        v_int(out, r->b->d[r->idx]); return ST_OK;
    case AML_REF_SLOT: v_dup(out, r->slot); return ST_OK;
    }
    return fail(x, AML_ETYPE, "referência inválida");
}

/* ------------------------------------------------------------------ destinos (SuperName/Target) */
enum { T_NONE, T_NODE, T_SLOT_LOCAL, T_SLOT_ARG, T_DEBUG, T_REF };
typedef struct { u8 kind; aml_node_t *node; aml_val_t *slot; aml_val_t ref; } tgt_t;

static int eval(ex_t *x, aml_val_t *out);

static void tgt_free(tgt_t *t) { if (t->kind == T_REF) aml_val_free(&t->ref); }

#define TG_MUST 1
#define TG_NOCALL 2
static int get_target(ex_t *x, tgt_t *t, int flags) {
    bool must_exist = (flags & TG_MUST) != 0;
    memset(t, 0, sizeof *t);
    if (x->p >= x->end) return fail(x, AML_EINVAL, "destino truncado");
    u8 op = *x->p;
    if (op == 0x00) { x->p++; t->kind = T_NONE; return ST_OK; }
    if (op >= 0x60 && op <= 0x67) {
        x->p++;
        if (!x->f) return fail(x, AML_EINVAL, "Local fora de método");
        t->kind = T_SLOT_LOCAL; t->slot = &x->f->loc[op - 0x60]; return ST_OK;
    }
    if (op >= 0x68 && op <= 0x6E) {
        x->p++;
        if (!x->f) return fail(x, AML_EINVAL, "Arg fora de método");
        t->kind = T_SLOT_ARG; t->slot = &x->f->arg[op - 0x68]; return ST_OK;
    }
    if (op == 0x5B && x->p + 1 < x->end && x->p[1] == 0x31) { x->p += 2; t->kind = T_DEBUG; return ST_OK; }
    if (is_lead(op)) {
        aml_name_t nm;
        if (parse_name(x, &nm)) return ST_ERR;
        aml_node_t *n = ns_resolve(x->scope, &nm);
        if (!n) {
            if (!must_exist) { t->kind = T_NONE; return ST_OK; }
            char b[80]; name_str(&nm, b, sizeof b);
            char m[120]; u32 o = 0; const char *pre = "destino não existe: ";
            while (*pre) m[o++] = *pre++;
            for (u32 i = 0; b[i] && o < sizeof m - 1; i++) m[o++] = b[i];
            m[o] = 0;
            return fail(x, AML_ENOTFOUND, m);
        }
        if (n->type == AML_METHOD && !(flags & TG_NOCALL)) {   /* invocação que devolve referência (Type6) */
            aml_val_t args[7]; int na = n->u.m.flags & 7;
            memset(args, 0, sizeof args);
            for (int i = 0; i < na; i++) if (eval(x, &args[i])) { for (int k = 0; k < i; k++) aml_val_free(&args[k]); return ST_ERR; }
            aml_val_t r; int st = eval_node(x, n, args, na, &r);
            for (int i = 0; i < na; i++) aml_val_free(&args[i]);
            if (st) return st;
            if (r.type != AML_REF) { aml_val_free(&r); return fail(x, AML_ETYPE, "método não devolveu referência"); }
            t->kind = T_REF; t->ref = r; return ST_OK;
        }
        t->kind = T_NODE; t->node = n; return ST_OK;
    }
    if (op == 0x83) {   /* DerefOf(ref) como destino: guarda através da referência */
        x->p++;
        aml_val_t r;
        if (eval(x, &r)) return ST_ERR;
        if (r.type == AML_STR) {
            aml_node_t *n = aml_find(x->scope, (const char *)r.b->d);
            aml_val_free(&r);
            if (!n) return fail(x, AML_ENOTFOUND, "DerefOf de nome inexistente");
            t->kind = T_NODE; t->node = n; return ST_OK;
        }
        if (r.type != AML_REF) { aml_val_free(&r); return fail(x, AML_ETYPE, "DerefOf sem referência"); }
        t->kind = T_REF; t->ref = r; return ST_OK;
    }
    if (op == 0x71 || op == 0x88) {
        aml_val_t r;
        if (eval(x, &r)) return ST_ERR;
        if (r.type != AML_REF) { aml_val_free(&r); return fail(x, AML_ETYPE, "destino não é referência"); }
        t->kind = T_REF; t->ref = r; return ST_OK;
    }
    return fail(x, AML_EINVAL, "destino com opcode inválido");
}

static int tgt_read(ex_t *x, tgt_t *t, aml_val_t *out) {
    memset(out, 0, sizeof *out);
    switch (t->kind) {
    case T_NODE: return read_node(x, t->node, out);
    case T_SLOT_LOCAL: v_dup(out, t->slot); return ST_OK;
    case T_SLOT_ARG:
        if (t->slot->type == AML_REF) return deref_read(x, t->slot, out);
        v_dup(out, t->slot); return ST_OK;
    case T_REF: return deref_read(x, &t->ref, out);
    }
    return fail(x, AML_EINVAL, "leitura de destino vazio");
}

static void debug_print(const aml_val_t *v) {
    switch (v->type) {
    case AML_INT: aml_os_log("aml debug: 0x%llx\n", (unsigned long long)v->i); break;
    case AML_STR: aml_os_log("aml debug: \"%s\"\n", (const char *)v->b->d); break;
    case AML_BUF: aml_os_log("aml debug: Buffer(%u)\n", v->b->len); break;
    case AML_PKG: aml_os_log("aml debug: Package(%u)\n", v->b->len); break;
    default: aml_os_log("aml debug: objeto tipo %u\n", v->type); break;
    }
}

static int store_node(ex_t *x, aml_node_t *n, const aml_val_t *v);

static int store_ref(ex_t *x, const aml_val_t *r, const aml_val_t *v) {
    switch (r->rkind) {
    case AML_REF_NODE: return store_node(x, r->node, v);
    case AML_REF_PKG: {
        if (r->idx >= r->b->len) return fail(x, AML_EFAULT, "Index além do Package");
        aml_val_t c; if (v_copy(&c, v)) return fail(x, AML_ENOMEM, "sem memória");
        aml_val_free(&r->b->el[r->idx]); r->b->el[r->idx] = c; return ST_OK;
    }
    case AML_REF_BUF: case AML_REF_STR: {
        if (r->idx >= r->b->len) return fail(x, AML_EFAULT, "Index além do Buffer");
        u64 i; if (to_int(x, v, &i)) return ST_ERR;
        r->b->d[r->idx] = (u8)i; return ST_OK;
    }
    case AML_REF_SLOT: {
        aml_val_t c; if (v_copy(&c, v)) return fail(x, AML_ENOMEM, "sem memória");
        aml_val_free(r->slot); *r->slot = c; return ST_OK;
    }
    }
    return fail(x, AML_ETYPE, "referência inválida");
}

static int store_node(ex_t *x, aml_node_t *n, const aml_val_t *v) {
    aml_val_t c;
    switch (n->type) {
    case AML_FIELD: {
        aml_val_t tmp; const u8 *s; u32 len; memset(&tmp, 0, sizeof tmp);
        if (bytes_of(x, v, &tmp, &s, &len)) return ST_ERR;
        int st = field_write_bytes(x, n, s, len);
        aml_val_free(&tmp);
        return st;
    }
    case AML_BUFFIELD: return bf_write(x, n, v);
    case AML_INT: { u64 i; if (to_int(x, v, &i)) return ST_ERR; n->v.i = i & g_mask; return ST_OK; }
    case AML_STR:
        if (to_str(x, v, &c, 0)) return ST_ERR;
        aml_val_free(&n->v); n->v = c; return ST_OK;
    case AML_BUF: {   /* copia para dentro do Buffer existente (CreateField continua válido) */
        aml_val_t b; if (to_buf(x, v, &b)) return ST_ERR;
        aml_blob_t *t = n->v.b;
        if (!t->len) { aml_val_free(&n->v); n->v = b; return ST_OK; }
        u32 k = b.b->len < t->len ? b.b->len : t->len;
        memcpy(t->d, b.b->d, k);
        memset(t->d + k, 0, t->len - k);
        aml_val_free(&b);
        return ST_OK;
    }
    case AML_PKG: case AML_UNINIT:
        if (v->type == AML_REF && v->rkind == AML_REF_NODE && n->type == AML_UNINIT) { v_dup(&c, v); }
        else if (v_copy(&c, v)) return fail(x, AML_ENOMEM, "sem memória");
        aml_val_free(&n->v); n->v = c;
        n->type = c.type == AML_INT || c.type == AML_STR || c.type == AML_BUF || c.type == AML_PKG ? c.type : AML_UNINIT;
        return ST_OK;
    }
    return fail(x, AML_ETYPE, "Store num objeto que não guarda dado");
}

static int store(ex_t *x, const aml_val_t *v, tgt_t *t) {
    aml_val_t c;
    switch (t->kind) {
    case T_NONE: return ST_OK;
    case T_DEBUG: debug_print(v); return ST_OK;
    case T_NODE: return store_node(x, t->node, v);
    case T_SLOT_ARG:
        if (t->slot->type == AML_REF) return store_ref(x, t->slot, v);
        /* fallthrough */
    case T_SLOT_LOCAL:
        if (v->type == AML_REF) v_dup(&c, v);
        else if (v_copy(&c, v)) return fail(x, AML_ENOMEM, "sem memória");
        aml_val_free(t->slot); *t->slot = c; return ST_OK;
    case T_REF: return store_ref(x, &t->ref, v);
    }
    return ST_OK;
}

/* ------------------------------------------------------------------ métodos */
static int exec_list(ex_t *x);

static void frame_end(frame_t *f) {
    for (int i = 0; i < 8; i++) aml_val_free(&f->loc[i]);
    for (int i = 0; i < 7; i++) aml_val_free(&f->arg[i]);
    for (u32 i = f->ndyn; i-- > 0;) {   /* em ordem inversa: filhos antes do pai */
        aml_node_t *n = f->dyn[i];
        if (n->parent) node_unlink(n);
        node_free_tree(n);
    }
    if (f->dyn) aml_os_free(f->dyn);
}

static int call_method(ex_t *caller, aml_node_t *m, const aml_val_t *args, int nargs, aml_val_t *ret) {
    memset(ret, 0, sizeof *ret);
    if (m->u.m.native) {
        aml_val_t a[7]; memset(a, 0, sizeof a);
        for (int i = 0; i < nargs && i < 7; i++) v_dup(&a[i], &args[i]);
        int r = m->u.m.native(a, nargs, ret);
        for (int i = 0; i < 7; i++) aml_val_free(&a[i]);
        return r ? fail(caller, r, "método nativo falhou") : ST_OK;
    }
    if (g_depth >= MAX_DEPTH) return fail(caller, AML_EDEPTH, "recursão de métodos demais");
    frame_t *f = zalloc(sizeof *f);
    if (!f) return fail(caller, AML_ENOMEM, "sem memória");
    f->mnode = m;
    for (int i = 0; i < nargs && i < 7; i++) v_dup(&f->arg[i], &args[i]);
    ex_t x; memset(&x, 0, sizeof x);
    x.p = m->u.m.code; x.end = m->u.m.code + m->u.m.len; x.scope = m; x.f = f;
    g_depth++;
    int st = exec_list(&x);
    g_depth--;
    if (st == ST_RET) { *ret = x.ret; memset(&x.ret, 0, sizeof x.ret); st = ST_OK; }
    else if (st == ST_ERR) {
        if (caller && !caller->err) caller->err = x.err ? x.err : AML_EINVAL;
        if (g_errs < 64) { char b[96]; aml_path(m, b, sizeof b); aml_os_log("aml: erro em %s\n", b); }
    } else st = ST_OK;
    /* uma referência a Local do próprio método não pode sair dele */
    if (ret->type == AML_REF && ret->rkind == AML_REF_SLOT) {
        aml_val_t t; v_dup(&t, ret->slot); v_copy(ret, &t); aml_val_free(&t);
    }
    aml_val_free(&x.ret);
    frame_end(f);
    aml_os_free(f);
    return st;
}

static int eval_node(ex_t *x, aml_node_t *n, const aml_val_t *args, int nargs, aml_val_t *ret) {
    if (n->type == AML_METHOD) return call_method(x, n, args, nargs, ret);
    return read_node(x, n, ret);
}

/* ------------------------------------------------------------------ operadores */
static int eval_int(ex_t *x, u64 *out) {
    aml_val_t v;
    if (eval(x, &v)) return ST_ERR;
    int st = to_int(x, &v, out);
    aml_val_free(&v);
    return st;
}

static int cmp_vals(ex_t *x, const aml_val_t *a0, const aml_val_t *b0, int *res) {
    aml_val_t a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    if (a0->type == AML_REF) { if (deref_read(x, a0, &a)) return ST_ERR; } else v_dup(&a, a0);
    int st = ST_OK;
    if (a.type == AML_INT || a.type == AML_UNINIT) {
        u64 ai = a.type == AML_INT ? a.i : 0, bi;
        if ((st = to_int(x, b0, &bi)) == ST_OK) *res = ai < bi ? -1 : ai > bi ? 1 : 0;
    } else if (a.type == AML_STR || a.type == AML_BUF) {
        st = a.type == AML_STR ? to_str(x, b0, &b, 0) : to_buf(x, b0, &b);
        if (!st) {
            u32 la = a.b->len, lb = b.b->len, n = la < lb ? la : lb;
            int c = memcmp(a.b->d, b.b->d, n);
            *res = c < 0 ? -1 : c > 0 ? 1 : la < lb ? -1 : la > lb ? 1 : 0;
        }
    } else st = fail(x, AML_ETYPE, "comparação com tipo inválido");
    aml_val_free(&a); aml_val_free(&b);
    return st;
}

static int concat(ex_t *x, const aml_val_t *a0, const aml_val_t *b0, aml_val_t *out) {
    aml_val_t a, b, ca, cb; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    memset(&ca, 0, sizeof ca); memset(&cb, 0, sizeof cb);
    int st = ST_OK;
    if (deref_read(x, a0, &a) || deref_read(x, b0, &b)) { st = ST_ERR; goto out; }
    if (a.type == AML_INT || a.type == AML_UNINIT) {
        u64 ai = a.type == AML_INT ? a.i : 0, bi;
        if ((st = to_int(x, &b, &bi))) goto out;
        u8 buf[16]; u32 w = int_bytes();
        for (u32 i = 0; i < w; i++) { buf[i] = (u8)(ai >> (8 * i)); buf[w + i] = (u8)(bi >> (8 * i)); }
        if (v_buf(out, buf, 2 * w)) st = fail(x, AML_ENOMEM, "sem memória");
    } else if (a.type == AML_BUF) {
        if ((st = to_buf(x, &b, &cb))) goto out;
        if (v_buf(out, NULL, a.b->len + cb.b->len)) { st = fail(x, AML_ENOMEM, "sem memória"); goto out; }
        memcpy(out->b->d, a.b->d, a.b->len); memcpy(out->b->d + a.b->len, cb.b->d, cb.b->len);
    } else {
        if (a.type == AML_STR) v_dup(&ca, &a);
        else if ((st = v_str(&ca, "[Objeto]", 8) ? fail(x, AML_ENOMEM, "sem memória") : ST_OK)) goto out;
        if (b.type == AML_INT || b.type == AML_STR || b.type == AML_BUF) { if ((st = to_str(x, &b, &cb, 0))) goto out; }
        else if (v_str(&cb, "[Objeto]", 8)) { st = fail(x, AML_ENOMEM, "sem memória"); goto out; }
        aml_blob_t *nb = blob_new(AML_STR, ca.b->len + cb.b->len);
        if (!nb) { st = fail(x, AML_ENOMEM, "sem memória"); goto out; }
        memcpy(nb->d, ca.b->d, ca.b->len); memcpy(nb->d + ca.b->len, cb.b->d, cb.b->len);
        nb->d[nb->len] = 0;
        out->type = AML_STR; out->b = nb;
    }
out:
    aml_val_free(&a); aml_val_free(&b); aml_val_free(&ca); aml_val_free(&cb);
    return st;
}

/* ConcatenateResTemplate: junta dois ResourceTemplates tirando a EndTag do primeiro */
static int concat_res(ex_t *x, const aml_val_t *a0, const aml_val_t *b0, aml_val_t *out) {
    aml_val_t a, b; memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);
    int st = ST_OK;
    if (to_buf(x, a0, &a) || to_buf(x, b0, &b)) { st = ST_ERR; goto out; }
    u32 la = a.b->len >= 2 && a.b->d[a.b->len - 2] == 0x79 ? a.b->len - 2 : a.b->len;
    u32 lb = b.b->len >= 2 && b.b->d[b.b->len - 2] == 0x79 ? b.b->len - 2 : b.b->len;
    if (v_buf(out, NULL, la + lb + 2)) { st = fail(x, AML_ENOMEM, "sem memória"); goto out; }
    memcpy(out->b->d, a.b->d, la); memcpy(out->b->d + la, b.b->d, lb);
    out->b->d[la + lb] = 0x79;
    u8 sum = 0; for (u32 i = 0; i < la + lb + 1; i++) sum += out->b->d[i];
    out->b->d[la + lb + 1] = (u8)(0 - sum);
out:
    aml_val_free(&a); aml_val_free(&b);
    return st;
}

static bool match_op(ex_t *x, u8 op, const aml_val_t *el, const aml_val_t *obj, int *st) {
    if (op == 0) return true;
    if (el->type != AML_INT && el->type != AML_STR && el->type != AML_BUF) return false;
    int c = 0;
    if (cmp_vals(x, el, obj, &c)) { *st = ST_ERR; return false; }
    switch (op) {
    case 1: return c == 0; case 2: return c <= 0; case 3: return c < 0; case 4: return c >= 0; case 5: return c > 0;
    }
    return false;
}

static u8 obj_type(const aml_val_t *v, aml_node_t *n) {
    if (n) {
        if (n->type == AML_UNINIT) return n->v.type <= AML_PKG ? n->v.type : 0;
        if (n->type == AML_SCOPE) return 0;
        return n->type <= AML_DEBUG ? n->type : 0;
    }
    if (v->type == AML_REF && v->rkind == AML_REF_NODE) return obj_type(NULL, v->node);
    return v->type <= AML_PKG ? v->type : 0;
}

static int make_package(ex_t *x, u32 count, const u8 *end, aml_val_t *out) {
    memset(out, 0, sizeof *out);
    u32 avail = 0;
    aml_blob_t *b = blob_new(AML_PKG, count);
    if (!b) return fail(x, AML_ENOMEM, "sem memória");
    out->type = AML_PKG; out->b = b;
    const u8 *save_end = x->end;
    x->end = end;
    while (x->p < end) {
        aml_val_t e; memset(&e, 0, sizeof e);
        if (is_lead(*x->p)) {   /* nome dentro do Package: referência ao objeto (ou o caminho em texto) */
            aml_name_t nm;
            if (parse_name(x, &nm)) { x->end = save_end; return ST_ERR; }
            aml_node_t *n = ns_resolve(x->scope, &nm);
            if (n) v_ref_node(&e, n);
            else { char s[96]; name_str(&nm, s, sizeof s); u32 k = 0; while (s[k]) k++; v_str(&e, s, k); }
        } else if (eval(x, &e)) { x->end = save_end; return ST_ERR; }
        if (avail < count) b->el[avail] = e; else aml_val_free(&e);
        avail++;
    }
    x->end = save_end;
    return ST_OK;
}

static int set_target_int(ex_t *x, u64 v, aml_val_t *out) {
    tgt_t t;
    v_int(out, v);
    if (get_target(x, &t, TG_MUST)) return ST_ERR;
    int st = store(x, out, &t);
    tgt_free(&t);
    return st;
}
static int set_target_val(ex_t *x, aml_val_t *out) {
    tgt_t t;
    if (get_target(x, &t, TG_MUST)) return ST_ERR;
    int st = store(x, out, &t);
    tgt_free(&t);
    return st;
}

static int binop(ex_t *x, u8 op, aml_val_t *out) {
    u64 a, b, r = 0;
    if (eval_int(x, &a) || eval_int(x, &b)) return ST_ERR;
    switch (op) {
    case 0x72: r = a + b; break;
    case 0x74: r = a - b; break;
    case 0x77: r = a * b; break;
    case 0x79: r = b >= g_bits ? 0 : a << b; break;
    case 0x7A: r = b >= g_bits ? 0 : a >> b; break;
    case 0x7B: r = a & b; break;
    case 0x7C: r = ~(a & b); break;
    case 0x7D: r = a | b; break;
    case 0x7E: r = ~(a | b); break;
    case 0x7F: r = a ^ b; break;
    case 0x85: if (!b) return fail(x, AML_EINVAL, "Mod por zero"); r = a % b; break;
    }
    return set_target_int(x, r & g_mask, out);
}

/* ------------------------------------------------------------------ criação de objetos */
static int def_field_list(ex_t *x, const u8 *end, u8 kind, aml_node_t *reg, aml_node_t *idx, aml_node_t *dat,
                          aml_node_t *bank, u64 bankval, u8 flags) {
    u32 bit = 0;
    u8 attr = 0;
    while (x->p < end) {
        u8 c = *x->p;
        if (c == 0x00) { x->p++; bit += pkg_raw(x); continue; }
        if (c == 0x01) { flags = (u8)((flags & 0xF0) | (x->p[1] & 0x0F)); attr = x->p[2]; x->p += 3; continue; }
        if (c == 0x03) { flags = (u8)((flags & 0xF0) | (x->p[1] & 0x0F)); attr = x->p[2]; x->p += 4; continue; }
        if (c == 0x02) {   /* ConnectField: NameString ou BufferData (GPIO/GenericSerialBus) */
            x->p++;
            if (*x->p == 0x11) { aml_val_t b; if (eval(x, &b)) return ST_ERR; aml_val_free(&b); }
            else { aml_name_t nm; if (parse_name(x, &nm)) return ST_ERR; }
            continue;
        }
        if (x->p + 4 > end) return fail(x, AML_EINVAL, "FieldList truncada");
        aml_name_t nm; nm.root = 0; nm.up = 0; nm.n = 1; nm.segs = x->p;
        x->p += 4;
        u32 len = pkg_raw(x);
        bool ex;
        aml_node_t *n = ns_create(x, &nm, AML_FIELD, &ex);
        if (!n) return fail(x, AML_ENOTFOUND, "escopo do campo não existe");
        if (ex) aml_val_free(&n->v);
        n->type = AML_FIELD;
        n->u.f.kind = kind; n->u.f.flags = flags; n->u.f.attr = attr;
        n->u.f.reg = reg; n->u.f.idx = idx; n->u.f.dat = dat; n->u.f.bank = bank; n->u.f.bankval = bankval;
        n->u.f.bitoff = bit; n->u.f.bitlen = len;
        bit += len;
    }
    return ST_OK;
}

static int def_create_field(ex_t *x, u8 op) {
    aml_val_t src; u64 index, nbits = 0;
    if (eval(x, &src)) return ST_ERR;
    if (src.type == AML_REF) { aml_val_t t; if (deref_read(x, &src, &t)) { aml_val_free(&src); return ST_ERR; } aml_val_free(&src); src = t; }
    if (src.type == AML_STR) { aml_val_t t; if (v_buf(&t, src.b->d, src.b->len)) { aml_val_free(&src); return fail(x, AML_ENOMEM, "sem memória"); } aml_val_free(&src); src = t; }
    if (src.type != AML_BUF) { aml_val_free(&src); return fail(x, AML_ETYPE, "CreateField fora de Buffer"); }
    if (eval_int(x, &index)) { aml_val_free(&src); return ST_ERR; }
    u32 bitoff, bitlen;
    switch (op) {
    case 0x8A: bitoff = (u32)index * 8; bitlen = 32; break;
    case 0x8B: bitoff = (u32)index * 8; bitlen = 16; break;
    case 0x8C: bitoff = (u32)index * 8; bitlen = 8; break;
    case 0x8D: bitoff = (u32)index; bitlen = 1; break;
    case 0x8F: bitoff = (u32)index * 8; bitlen = 64; break;
    default:
        if (eval_int(x, &nbits)) { aml_val_free(&src); return ST_ERR; }
        bitoff = (u32)index; bitlen = (u32)nbits; break;
    }
    aml_name_t nm;
    if (parse_name(x, &nm)) { aml_val_free(&src); return ST_ERR; }
    bool ex;
    aml_node_t *n = ns_create(x, &nm, AML_BUFFIELD, &ex);
    if (!n) { aml_val_free(&src); return fail(x, AML_ENOTFOUND, "escopo do BufferField não existe"); }
    if (ex) { aml_val_free(&n->v); if (n->type == AML_BUFFIELD && n->u.bf.buf) blob_put(n->u.bf.buf); }
    n->type = AML_BUFFIELD;
    n->u.bf.buf = src.b;          /* o nó fica com a referência do bloco */
    n->u.bf.bitoff = bitoff; n->u.bf.bitlen = bitlen;
    if ((u64)bitoff + bitlen > (u64)src.b->len * 8) aml_os_log("aml: BufferField além do Buffer\n");
    return ST_OK;
}

static int exec_body(ex_t *x, aml_node_t *scope, const u8 *end) {
    ex_t y = *x;
    y.scope = scope; y.end = end;
    int st = exec_list(&y);
    if (st == ST_RET) { x->ret = y.ret; }
    if (st == ST_ERR && !x->f) {   /* carga: pula o resto do corpo e segue a tabela */
        x->p = end;
        x->err = 0;
        return ST_OK;
    }
    if (st == ST_ERR && !x->err) x->err = y.err;
    x->p = end;
    return st;
}

/* ------------------------------------------------------------------ avaliação */
static int eval(ex_t *x, aml_val_t *out) {
    memset(out, 0, sizeof *out);
    if (x->p >= x->end) return fail(x, AML_EINVAL, "fim inesperado do bytecode");
    const u8 *start = x->p;
    u8 op = *x->p++;
    aml_val_t a, b; aml_name_t nm; tgt_t t;
    u64 i1, i2; const u8 *end; int st, cmp;
    memset(&a, 0, sizeof a); memset(&b, 0, sizeof b);

    switch (op) {
    case 0x00: v_int(out, 0); return ST_OK;
    case 0x01: v_int(out, 1); return ST_OK;
    case 0xFF: v_int(out, ~0ull); return ST_OK;
    case 0x0A: v_int(out, x->p[0]); x->p += 1; return ST_OK;
    case 0x0B: v_int(out, (u64)x->p[0] | (u64)x->p[1] << 8); x->p += 2; return ST_OK;
    case 0x0C: v_int(out, (u64)x->p[0] | (u64)x->p[1] << 8 | (u64)x->p[2] << 16 | (u64)x->p[3] << 24); x->p += 4; return ST_OK;
    case 0x0E: { u64 v = 0; for (int i = 0; i < 8; i++) v |= (u64)x->p[i] << (8 * i); x->p += 8; v_int(out, v); return ST_OK; }
    case 0x0D: {
        const u8 *s = x->p; while (x->p < x->end && *x->p) x->p++;
        if (x->p >= x->end) return fail(x, AML_EINVAL, "String sem NUL");
        u32 n = (u32)(x->p - s); x->p++;
        return v_str(out, (const char *)s, n) ? fail(x, AML_ENOMEM, "sem memória") : ST_OK;
    }
    case 0x11: {   /* Buffer */
        if (pkg_end(x, &end)) return ST_ERR;
        const u8 *se = x->end; x->end = end;
        st = eval_int(x, &i1);
        x->end = se;
        if (st) return ST_ERR;
        u32 init = (u32)(end - x->p), sz = (u32)i1 > init ? (u32)i1 : init;
        if (sz > (1u << 24)) return fail(x, AML_EINVAL, "Buffer grande demais");
        if (v_buf(out, NULL, sz)) return fail(x, AML_ENOMEM, "sem memória");
        memcpy(out->b->d, x->p, init);
        x->p = end;
        return ST_OK;
    }
    case 0x12: {   /* Package */
        if (pkg_end(x, &end)) return ST_ERR;
        u32 n = *x->p++;
        st = make_package(x, n, end, out);
        x->p = end;
        return st;
    }
    case 0x13: {   /* VarPackage */
        if (pkg_end(x, &end)) return ST_ERR;
        const u8 *se = x->end; x->end = end;
        st = eval_int(x, &i1);
        x->end = se;
        if (st) return ST_ERR;
        if (i1 > 65536) return fail(x, AML_EINVAL, "VarPackage grande demais");
        st = make_package(x, (u32)i1, end, out);
        x->p = end;
        return st;
    }
    case 0x60: case 0x61: case 0x62: case 0x63: case 0x64: case 0x65: case 0x66: case 0x67:
        if (!x->f) return fail(x, AML_EINVAL, "Local fora de método");
        v_dup(out, &x->f->loc[op - 0x60]); return ST_OK;
    case 0x68: case 0x69: case 0x6A: case 0x6B: case 0x6C: case 0x6D: case 0x6E:
        if (!x->f) return fail(x, AML_EINVAL, "Arg fora de método");
        v_dup(out, &x->f->arg[op - 0x68]); return ST_OK;

    case 0x70:   /* Store */
        if (eval(x, &a)) return ST_ERR;
        if (get_target(x, &t, TG_MUST)) { aml_val_free(&a); return ST_ERR; }
        st = store(x, &a, &t);
        tgt_free(&t);
        *out = a;
        return st;
    case 0x71:   /* RefOf */
        if (get_target(x, &t, TG_MUST | TG_NOCALL)) return ST_ERR;
        if (t.kind == T_NODE) v_ref_node(out, t.node);
        else if (t.kind == T_SLOT_LOCAL || t.kind == T_SLOT_ARG) {
            if (t.kind == T_SLOT_ARG && t.slot->type == AML_REF) v_dup(out, t.slot);
            else { out->type = AML_REF; out->rkind = AML_REF_SLOT; out->slot = t.slot; }
        } else if (t.kind == T_REF) { *out = t.ref; return ST_OK; }
        else return fail(x, AML_EINVAL, "RefOf inválido");
        return ST_OK;
    case 0x72: case 0x74: case 0x77: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D: case 0x7E: case 0x7F: case 0x85:
        return binop(x, op, out);
    case 0x73:   /* Concatenate */
        if (eval(x, &a)) return ST_ERR;
        if (eval(x, &b)) { aml_val_free(&a); return ST_ERR; }
        st = concat(x, &a, &b, out);
        aml_val_free(&a); aml_val_free(&b);
        if (st) return st;
        return set_target_val(x, out);
    case 0x84:   /* ConcatenateResTemplate */
        if (eval(x, &a)) return ST_ERR;
        if (eval(x, &b)) { aml_val_free(&a); return ST_ERR; }
        st = concat_res(x, &a, &b, out);
        aml_val_free(&a); aml_val_free(&b);
        if (st) return st;
        return set_target_val(x, out);
    case 0x75: case 0x76:   /* Increment / Decrement */
        if (get_target(x, &t, TG_MUST)) return ST_ERR;
        if (tgt_read(x, &t, &a) || to_int(x, &a, &i1)) { aml_val_free(&a); tgt_free(&t); return ST_ERR; }
        aml_val_free(&a);
        v_int(out, op == 0x75 ? i1 + 1 : i1 - 1);
        st = store(x, out, &t);
        tgt_free(&t);
        return st;
    case 0x78:   /* Divide */
        if (eval_int(x, &i1) || eval_int(x, &i2)) return ST_ERR;
        if (!i2) return fail(x, AML_EINVAL, "Divide por zero");
        v_int(&a, i1 % i2);
        if (get_target(x, &t, TG_MUST)) return ST_ERR;
        st = store(x, &a, &t); tgt_free(&t);
        if (st) return st;
        return set_target_int(x, i1 / i2, out);
    case 0x80:   /* Not */
        if (eval_int(x, &i1)) return ST_ERR;
        return set_target_int(x, ~i1 & g_mask, out);
    case 0x81: case 0x82: {   /* FindSetLeftBit / FindSetRightBit */
        if (eval_int(x, &i1)) return ST_ERR;
        u64 r = 0;
        if (i1) {
            if (op == 0x81) { for (int k = 63; k >= 0; k--) if ((i1 >> k) & 1) { r = (u64)k + 1; break; } }
            else { for (int k = 0; k < 64; k++) if ((i1 >> k) & 1) { r = (u64)k + 1; break; } }
        }
        return set_target_int(x, r, out);
    }
    case 0x83:   /* DerefOf */
        if (eval(x, &a)) return ST_ERR;
        if (a.type == AML_STR) {
            aml_node_t *n = aml_find(x->scope, (const char *)a.b->d);
            aml_val_free(&a);
            if (!n) return fail(x, AML_ENOTFOUND, "DerefOf de nome inexistente");
            return read_node(x, n, out);
        }
        if (a.type != AML_REF) { *out = a; return ST_OK; }   /* tolerância: valor já resolvido */
        st = deref_read(x, &a, out);
        aml_val_free(&a);
        return st;
    case 0x86:   /* Notify */
        if (get_target(x, &t, TG_MUST)) return ST_ERR;
        if (eval_int(x, &i1)) { tgt_free(&t); return ST_ERR; }
        if (t.kind == T_NODE) aml_os_notify(t.node, i1);
        else if (t.kind == T_REF && t.ref.rkind == AML_REF_NODE) aml_os_notify(t.ref.node, i1);
        tgt_free(&t);
        return ST_OK;
    case 0x87: {   /* SizeOf */
        if (get_target(x, &t, TG_MUST)) return ST_ERR;
        st = tgt_read(x, &t, &a);
        tgt_free(&t);
        if (st) return st;
        if (a.type == AML_REF) { aml_val_t d; st = deref_read(x, &a, &d); aml_val_free(&a); if (st) return st; a = d; }
        if (a.type == AML_STR || a.type == AML_BUF || a.type == AML_PKG) v_int(out, a.b->len);
        else { aml_val_free(&a); return fail(x, AML_ETYPE, "SizeOf de tipo sem tamanho"); }
        aml_val_free(&a);
        return ST_OK;
    }
    case 0x88:   /* Index */
        if (eval(x, &a)) return ST_ERR;
        if (a.type == AML_REF) { aml_val_t d; st = deref_read(x, &a, &d); aml_val_free(&a); if (st) return st; a = d; }
        if (eval_int(x, &i1)) { aml_val_free(&a); return ST_ERR; }
        if (a.type != AML_PKG && a.type != AML_BUF && a.type != AML_STR) { aml_val_free(&a); return fail(x, AML_ETYPE, "Index de tipo inválido"); }
        if (i1 >= a.b->len) { aml_val_free(&a); return fail(x, AML_EFAULT, "Index além do fim"); }
        out->type = AML_REF;
        out->rkind = a.type == AML_PKG ? AML_REF_PKG : a.type == AML_BUF ? AML_REF_BUF : AML_REF_STR;
        out->b = a.b; out->idx = (u32)i1;   /* herda a referência de a */
        if (get_target(x, &t, TG_MUST)) return ST_ERR;
        st = store(x, out, &t);
        tgt_free(&t);
        return st;
    case 0x89: {   /* Match */
        if (eval(x, &a)) return ST_ERR;
        if (a.type == AML_REF) { aml_val_t d; st = deref_read(x, &a, &d); aml_val_free(&a); if (st) return st; a = d; }
        u8 o1 = *x->p++;
        aml_val_t m1, m2; memset(&m1, 0, sizeof m1); memset(&m2, 0, sizeof m2);
        if (eval(x, &m1)) { aml_val_free(&a); return ST_ERR; }
        u8 o2 = *x->p++;
        if (eval(x, &m2) || eval_int(x, &i1)) { aml_val_free(&a); aml_val_free(&m1); aml_val_free(&m2); return ST_ERR; }
        if (a.type != AML_PKG) { aml_val_free(&a); aml_val_free(&m1); aml_val_free(&m2); return fail(x, AML_ETYPE, "Match fora de Package"); }
        u64 r = g_mask; st = ST_OK;
        for (u64 k = i1; k < a.b->len; k++) {
            aml_val_t *e = &a.b->el[k];
            if (match_op(x, o1, e, &m1, &st) && match_op(x, o2, e, &m2, &st)) { r = k; break; }
            if (st) break;
        }
        aml_val_free(&a); aml_val_free(&m1); aml_val_free(&m2);
        v_int(out, r);
        return st;
    }
    case 0x8E: {   /* ObjectType */
        if (get_target(x, &t, TG_NOCALL)) return ST_ERR;
        u8 ty = 0;
        if (t.kind == T_NODE) ty = obj_type(NULL, t.node);
        else if (t.kind == T_SLOT_LOCAL || t.kind == T_SLOT_ARG) ty = obj_type(t.slot, NULL);
        else if (t.kind == T_REF) { aml_val_t d; if (!deref_read(x, &t.ref, &d)) { ty = obj_type(&d, NULL); aml_val_free(&d); } }
        else if (t.kind == T_DEBUG) ty = AML_DEBUG;
        tgt_free(&t);
        v_int(out, ty);
        return ST_OK;
    }
    case 0x90: case 0x91:   /* LAnd / LOr */
        if (eval_int(x, &i1) || eval_int(x, &i2)) return ST_ERR;
        v_int(out, (op == 0x90 ? (i1 && i2) : (i1 || i2)) ? g_mask : 0);
        return ST_OK;
    case 0x92:   /* LNot (e LNotEqual/LLessEqual/LGreaterEqual) */
        if (x->p < x->end && (*x->p == 0x93 || *x->p == 0x94 || *x->p == 0x95)) {
            u8 o = *x->p++;
            if (eval(x, &a)) return ST_ERR;
            if (eval(x, &b)) { aml_val_free(&a); return ST_ERR; }
            st = cmp_vals(x, &a, &b, &cmp);
            aml_val_free(&a); aml_val_free(&b);
            if (st) return st;
            bool r = o == 0x93 ? cmp != 0 : o == 0x94 ? cmp <= 0 : cmp >= 0;
            v_int(out, r ? g_mask : 0);
            return ST_OK;
        }
        if (eval_int(x, &i1)) return ST_ERR;
        v_int(out, i1 ? 0 : g_mask);
        return ST_OK;
    case 0x93: case 0x94: case 0x95:
        if (eval(x, &a)) return ST_ERR;
        if (eval(x, &b)) { aml_val_free(&a); return ST_ERR; }
        st = cmp_vals(x, &a, &b, &cmp);
        aml_val_free(&a); aml_val_free(&b);
        if (st) return st;
        v_int(out, (op == 0x93 ? cmp == 0 : op == 0x94 ? cmp > 0 : cmp < 0) ? g_mask : 0);
        return ST_OK;
    case 0x96:   /* ToBuffer */
        if (eval(x, &a)) return ST_ERR;
        st = to_buf(x, &a, out); aml_val_free(&a);
        if (st) return st;
        return set_target_val(x, out);
    case 0x97: case 0x98:   /* ToDecimalString / ToHexString */
        if (eval(x, &a)) return ST_ERR;
        st = to_str(x, &a, out, op == 0x97 ? 2 : 1); aml_val_free(&a);
        if (st) return st;
        return set_target_val(x, out);
    case 0x99:   /* ToInteger */
        if (eval(x, &a)) return ST_ERR;
        st = to_int_explicit(x, &a, &i1); aml_val_free(&a);
        if (st) return st;
        return set_target_int(x, i1, out);
    case 0x9C: {   /* ToString(Buffer, Length) */
        if (eval(x, &a)) return ST_ERR;
        if (eval_int(x, &i1)) { aml_val_free(&a); return ST_ERR; }
        if (to_buf(x, &a, &b)) { aml_val_free(&a); return ST_ERR; }
        aml_val_free(&a);
        u32 n = 0; while (n < b.b->len && b.b->d[n] && (i1 == g_mask || n < i1)) n++;
        st = v_str(out, (const char *)b.b->d, n) ? fail(x, AML_ENOMEM, "sem memória") : ST_OK;
        aml_val_free(&b);
        if (st) return st;
        return set_target_val(x, out);
    }
    case 0x9D: {   /* CopyObject */
        if (eval(x, &a)) return ST_ERR;
        if (get_target(x, &t, TG_MUST)) { aml_val_free(&a); return ST_ERR; }
        if (t.kind == T_NODE && (t.node->type <= AML_PKG)) {
            aml_val_t c; if (v_copy(&c, &a)) { aml_val_free(&a); return fail(x, AML_ENOMEM, "sem memória"); }
            aml_val_free(&t.node->v); t.node->v = c;
            t.node->type = c.type <= AML_PKG ? c.type : AML_UNINIT;
            st = ST_OK;
        } else st = store(x, &a, &t);
        tgt_free(&t);
        *out = a;
        return st;
    }
    case 0x9E: {   /* Mid */
        if (eval(x, &a)) return ST_ERR;
        if (a.type == AML_REF) { aml_val_t d; st = deref_read(x, &a, &d); aml_val_free(&a); if (st) return st; a = d; }
        if (eval_int(x, &i1) || eval_int(x, &i2)) { aml_val_free(&a); return ST_ERR; }
        if (a.type == AML_INT) { aml_val_t c; if (to_buf(x, &a, &c)) return ST_ERR; a = c; }
        if (a.type != AML_STR && a.type != AML_BUF) { aml_val_free(&a); return fail(x, AML_ETYPE, "Mid de tipo inválido"); }
        u32 len = a.b->len, s0 = i1 > len ? len : (u32)i1, n = i2 > len - s0 ? len - s0 : (u32)i2;
        st = (a.type == AML_STR ? v_str(out, (const char *)a.b->d + s0, n) : v_buf(out, a.b->d + s0, n)) ? fail(x, AML_ENOMEM, "sem memória") : ST_OK;
        aml_val_free(&a);
        if (st) return st;
        return set_target_val(x, out);
    }
    case 0x8A: case 0x8B: case 0x8C: case 0x8D: case 0x8F:
        return def_create_field(x, op);

    /* ---- objetos com nome ---- */
    case 0x06: {   /* Alias */
        aml_name_t src, dst;
        if (parse_name(x, &src) || parse_name(x, &dst)) return ST_ERR;
        aml_node_t *s = ns_resolve(x->scope, &src);
        bool ex;
        aml_node_t *n = ns_create(x, &dst, AML_ALIAS, &ex);
        if (!n) return fail(x, AML_ENOTFOUND, "escopo do Alias não existe");
        if (!ex) { n->u.alias = s; if (!s) aml_os_log("aml: Alias para nome inexistente\n"); }
        return ST_OK;
    }
    case 0x08: {   /* Name */
        if (parse_name(x, &nm)) return ST_ERR;
        bool ex;
        aml_node_t *n = ns_create(x, &nm, AML_UNINIT, &ex);
        if (!n) { char s[80]; name_str(&nm, s, sizeof s); aml_os_log("aml: escopo de Name(%s) não existe\n", s); return fail(x, AML_ENOTFOUND, "Name sem escopo"); }
        if (eval(x, &a)) return ST_ERR;
        if (ex && n->type != AML_UNINIT && n->type > AML_PKG) { aml_val_free(&a); return ST_OK; }
        aml_val_free(&n->v);
        n->v = a;
        n->type = a.type <= AML_PKG ? a.type : AML_UNINIT;
        return ST_OK;
    }
    case 0x10: {   /* Scope */
        if (pkg_end(x, &end) || parse_name(x, &nm)) return ST_ERR;
        aml_node_t *n = ns_resolve(x->scope, &nm);
        if (!n) {
            if (!x->f) {   /* SSDT abrindo escopo que ninguém declarou: cria, como os interpretadores tolerantes */
                bool ex; n = ns_create(x, &nm, AML_SCOPE, &ex);
            }
            if (!n) { char s[80]; name_str(&nm, s, sizeof s); aml_os_log("aml: Scope(%s) inexistente\n", s); x->p = end; return x->f ? fail(x, AML_ENOTFOUND, "Scope inexistente") : ST_OK; }
        }
        return exec_body(x, n, end);
    }
    case 0x14: {   /* Method */
        if (pkg_end(x, &end) || parse_name(x, &nm)) return ST_ERR;
        u8 fl = *x->p++;
        bool ex;
        aml_node_t *n = ns_create(x, &nm, AML_METHOD, &ex);
        if (!n) { char s[80]; name_str(&nm, s, sizeof s); aml_os_log("aml: escopo de Method(%s) não existe\n", s); x->p = end; return x->f ? fail(x, AML_ENOTFOUND, "Method sem escopo") : ST_OK; }
        if (ex) aml_val_free(&n->v);
        n->type = AML_METHOD;
        n->u.m.code = x->p; n->u.m.len = (u32)(end - x->p); n->u.m.flags = fl; n->u.m.native = NULL;
        x->p = end;
        return ST_OK;
    }
    case 0x15: {   /* External */
        if (parse_name(x, &nm)) return ST_ERR;
        x->p += 2;
        return ST_OK;
    }

    /* ---- controle de fluxo ---- */
    case 0xA0: {   /* If / Else */
        if (pkg_end(x, &end)) return ST_ERR;
        const u8 *se = x->end; x->end = end;
        st = eval_int(x, &i1);
        x->end = se;
        if (st) return ST_ERR;
        if (i1) {
            ex_t y = *x; y.end = end;
            st = exec_list(&y);
            x->ret = y.ret; if (st == ST_ERR) x->err = y.err;
            x->p = end;
            if (x->p < x->end && *x->p == 0xA1) {   /* pula o Else */
                x->p++;
                const u8 *ee; if (pkg_end(x, &ee)) return ST_ERR;
                x->p = ee;
            }
            if (st == ST_ERR && !x->f) { x->err = 0; return ST_OK; }
            return st;
        }
        x->p = end;
        if (x->p < x->end && *x->p == 0xA1) {
            x->p++;
            const u8 *ee; if (pkg_end(x, &ee)) return ST_ERR;
            ex_t y = *x; y.end = ee;
            st = exec_list(&y);
            x->ret = y.ret; if (st == ST_ERR) x->err = y.err;
            x->p = ee;
            if (st == ST_ERR && !x->f) { x->err = 0; return ST_OK; }
            return st;
        }
        return ST_OK;
    }
    case 0xA1: {   /* Else solto (If já tratado): ignora */
        if (pkg_end(x, &end)) return ST_ERR;
        x->p = end;
        return ST_OK;
    }
    case 0xA2: {   /* While */
        if (pkg_end(x, &end)) return ST_ERR;
        const u8 *pred = x->p;
        for (u32 it = 0;; it++) {
            if (it >= LOOP_MAX) return fail(x, AML_EINVAL, "While sem fim");
            x->p = pred;
            const u8 *se = x->end; x->end = end;
            st = eval_int(x, &i1);
            x->end = se;
            if (st) return ST_ERR;
            if (!i1) break;
            ex_t y = *x; y.end = end;
            st = exec_list(&y);
            if (st == ST_BREAK) break;
            if (st == ST_RET) { x->ret = y.ret; x->p = end; return ST_RET; }
            if (st == ST_ERR) { x->err = y.err; x->p = end; return ST_ERR; }
        }
        x->p = end;
        return ST_OK;
    }
    case 0xA3: case 0xCC: return ST_OK;   /* Noop, BreakPoint */
    case 0xA4:   /* Return */
        if (x->p < x->end && eval(x, &a)) return ST_ERR;
        aml_val_free(&x->ret);
        if (a.type == AML_REF && a.rkind == AML_REF_SLOT) { aml_val_t c; v_dup(&c, a.slot); x->ret = c; }
        else x->ret = a;
        return ST_RET;
    case 0xA5: return ST_BREAK;
    case 0x9F: return ST_CONT;

    case 0x5B: {
        u8 e = *x->p++;
        switch (e) {
        case 0x01: {   /* Mutex */
            if (parse_name(x, &nm)) return ST_ERR;
            u8 sync = *x->p++;
            bool ex; aml_node_t *n = ns_create(x, &nm, AML_MUTEX, &ex);
            if (!n) return x->f ? fail(x, AML_ENOTFOUND, "Mutex sem escopo") : ST_OK;
            n->type = AML_MUTEX; n->u.mx.sync = sync;
            return ST_OK;
        }
        case 0x02: {   /* Event */
            if (parse_name(x, &nm)) return ST_ERR;
            bool ex; aml_node_t *n = ns_create(x, &nm, AML_EVENT, &ex);
            if (!n) return x->f ? fail(x, AML_ENOTFOUND, "Event sem escopo") : ST_OK;
            n->type = AML_EVENT;
            return ST_OK;
        }
        case 0x12: {   /* CondRefOf */
            if (get_target(x, &t, TG_NOCALL)) return ST_ERR;
            bool found = t.kind != T_NONE;
            aml_val_t r; memset(&r, 0, sizeof r);
            if (found) {
                if (t.kind == T_NODE) v_ref_node(&r, t.node);
                else if (t.kind == T_REF) { v_dup(&r, &t.ref); }
                else if (t.kind == T_SLOT_LOCAL || t.kind == T_SLOT_ARG) { r.type = AML_REF; r.rkind = AML_REF_SLOT; r.slot = t.slot; }
            }
            tgt_free(&t);
            if (get_target(x, &t, TG_MUST)) { aml_val_free(&r); return ST_ERR; }
            if (found && t.kind != T_NONE) store(x, &r, &t);
            tgt_free(&t);
            aml_val_free(&r);
            v_int(out, found ? g_mask : 0);
            return ST_OK;
        }
        case 0x13: return def_create_field(x, 0x13);
        case 0x1F: {   /* LoadTable(Signature, OEMID, OEMTableID, RootPath, ParameterPath, ParameterData) */
            aml_val_t s[6]; memset(s, 0, sizeof s);
            for (int i = 0; i < 6; i++) if (eval(x, &s[i])) { for (int k = 0; k < i; k++) aml_val_free(&s[k]); return ST_ERR; }
            char sig[5] = {0}, oem[7] = {0}, oemt[9] = {0};
            if (s[0].type == AML_STR) memcpy(sig, s[0].b->d, s[0].b->len < 4 ? s[0].b->len : 4);
            if (s[1].type == AML_STR) memcpy(oem, s[1].b->d, s[1].b->len < 6 ? s[1].b->len : 6);
            if (s[2].type == AML_STR) memcpy(oemt, s[2].b->d, s[2].b->len < 8 ? s[2].b->len : 8);
            const void *tb = aml_os_find_table(sig, oem, oemt, 0);
            u64 ok = 0;
            if (tb && aml_load_table(tb) == AML_OK) ok = g_mask;
            for (int i = 0; i < 6; i++) aml_val_free(&s[i]);
            v_int(out, ok);
            return ST_OK;
        }
        case 0x20: {   /* Load(Region/Field, DDBHandle) */
            if (parse_name(x, &nm)) return ST_ERR;
            aml_node_t *n = ns_resolve(x->scope, &nm);
            u64 ok = 0;
            if (n && n->type == AML_REGION && (n->u.r.space == AML_AS_MEM) && n->u.r.len >= 36 && n->u.r.len < (1u << 24)) {
                u8 *tb = zalloc((u32)n->u.r.len);
                bool good = tb != NULL;
                for (u64 i = 0; good && i < n->u.r.len; i++) { u64 v; good = aml_os_mem(n->u.r.base + i, 8, &v, false); if (good) tb[i] = (u8)v; }
                if (good && aml_load_table(tb) == AML_OK) ok = ++g_ddb;   /* a tabela carregada fica viva (métodos apontam para ela) */
                else if (tb) aml_os_free(tb);
            } else if (n && n->type == AML_BUF) {
                u8 *tb = zalloc(n->v.b->len);
                if (tb) { memcpy(tb, n->v.b->d, n->v.b->len); if (aml_load_table(tb) == AML_OK) ok = ++g_ddb; else aml_os_free(tb); }
            } else aml_os_log("aml: Load de objeto não suportado\n");
            return set_target_int(x, ok, out);
        }
        case 0x21: if (eval_int(x, &i1)) return ST_ERR; aml_os_stall_us(i1 > 100 ? 100 : i1); return ST_OK;   /* Stall */
        case 0x22: if (eval_int(x, &i1)) return ST_ERR; aml_os_sleep_ms(i1); return ST_OK;                     /* Sleep */
        case 0x23: {   /* Acquire */
            if (get_target(x, &t, TG_MUST)) return ST_ERR;
            x->p += 2;
            aml_node_t *n = t.kind == T_NODE ? t.node : t.kind == T_REF && t.ref.rkind == AML_REF_NODE ? t.ref.node : NULL;
            tgt_free(&t);
            if (!n || n->type != AML_MUTEX) return fail(x, AML_ETYPE, "Acquire fora de Mutex");
            if (n->name[0] == '_' && n->name[1] == 'G' && n->name[2] == 'L' && n->parent == g_root && !n->u.mx.depth) aml_os_global_lock(true);
            n->u.mx.depth++;
            v_int(out, 0);
            return ST_OK;
        }
        case 0x27: {   /* Release */
            if (get_target(x, &t, TG_MUST)) return ST_ERR;
            aml_node_t *n = t.kind == T_NODE ? t.node : t.kind == T_REF && t.ref.rkind == AML_REF_NODE ? t.ref.node : NULL;
            tgt_free(&t);
            if (!n || n->type != AML_MUTEX) return fail(x, AML_ETYPE, "Release fora de Mutex");
            if (n->u.mx.depth) {
                n->u.mx.depth--;
                if (!n->u.mx.depth && n->name[0] == '_' && n->name[1] == 'G' && n->name[2] == 'L' && n->parent == g_root) aml_os_global_lock(false);
            }
            return ST_OK;
        }
        case 0x24: case 0x26: {   /* Signal / Reset */
            if (get_target(x, &t, TG_MUST)) return ST_ERR;
            aml_node_t *n = t.kind == T_NODE ? t.node : NULL;
            tgt_free(&t);
            if (!n || n->type != AML_EVENT) return fail(x, AML_ETYPE, "Signal/Reset fora de Event");
            if (e == 0x24) n->u.ev.count++; else n->u.ev.count = 0;
            return ST_OK;
        }
        case 0x25: {   /* Wait(Event, Timeout) */
            if (get_target(x, &t, TG_MUST)) return ST_ERR;
            aml_node_t *n = t.kind == T_NODE ? t.node : NULL;
            tgt_free(&t);
            if (eval_int(x, &i1)) return ST_ERR;
            if (!n || n->type != AML_EVENT) return fail(x, AML_ETYPE, "Wait fora de Event");
            if (n->u.ev.count) { n->u.ev.count--; v_int(out, 0); }
            else { if (i1 && i1 != 0xFFFF) aml_os_sleep_ms(i1 > 10 ? 10 : i1); v_int(out, g_mask); }
            return ST_OK;
        }
        case 0x28: {   /* FromBCD */
            if (eval_int(x, &i1)) return ST_ERR;
            u64 r = 0, m = 1;
            for (; i1; i1 >>= 4, m *= 10) r += (i1 & 15) * m;
            return set_target_int(x, r, out);
        }
        case 0x29: {   /* ToBCD */
            if (eval_int(x, &i1)) return ST_ERR;
            u64 r = 0; int s = 0;
            for (; i1 && s < 64; i1 /= 10, s += 4) r |= (i1 % 10) << s;
            return set_target_int(x, r, out);
        }
        case 0x2A: if (get_target(x, &t, TG_MUST)) return ST_ERR; tgt_free(&t); return ST_OK;   /* Unload */
        case 0x30: v_int(out, 0x20251212); return ST_OK;   /* Revision */
        case 0x31: return ST_OK;   /* Debug como fonte: objeto vazio */
        case 0x32: {   /* Fatal */
            u8 ty = *x->p++; u32 code = (u32)x->p[0] | (u32)x->p[1] << 8 | (u32)x->p[2] << 16 | (u32)x->p[3] << 24; x->p += 4;
            if (eval_int(x, &i1)) return ST_ERR;
            aml_os_log("aml: Fatal tipo %u código 0x%x arg 0x%llx\n", ty, code, (unsigned long long)i1);
            return ST_OK;
        }
        case 0x33: v_int(out, aml_os_timer_100ns()); return ST_OK;   /* Timer */
        case 0x80: {   /* OperationRegion */
            if (parse_name(x, &nm)) return ST_ERR;
            u8 sp = *x->p++;
            if (eval_int(x, &i1) || eval_int(x, &i2)) return ST_ERR;
            bool ex; aml_node_t *n = ns_create(x, &nm, AML_REGION, &ex);
            if (!n) return x->f ? fail(x, AML_ENOTFOUND, "OperationRegion sem escopo") : ST_OK;
            n->type = AML_REGION; n->u.r.space = sp; n->u.r.base = i1; n->u.r.len = i2; n->u.r.pci_ok = 0;
            return ST_OK;
        }
        case 0x81: {   /* Field */
            if (pkg_end(x, &end) || parse_name(x, &nm)) return ST_ERR;
            aml_node_t *r = ns_resolve(x->scope, &nm);
            u8 fl = *x->p++;
            if (!r || r->type != AML_REGION) { char s[80]; name_str(&nm, s, sizeof s); aml_os_log("aml: Field de região inexistente %s\n", s); x->p = end; return x->f ? fail(x, AML_ENOTFOUND, "Field sem região") : ST_OK; }
            const u8 *se = x->end; x->end = end;
            st = def_field_list(x, end, 0, r, NULL, NULL, NULL, 0, fl);
            x->end = se; x->p = end;
            return st;
        }
        case 0x86: {   /* IndexField */
            aml_name_t ni, nd;
            if (pkg_end(x, &end) || parse_name(x, &ni) || parse_name(x, &nd)) return ST_ERR;
            aml_node_t *in = ns_resolve(x->scope, &ni), *dn = ns_resolve(x->scope, &nd);
            u8 fl = *x->p++;
            if (!in || !dn || in->type != AML_FIELD || dn->type != AML_FIELD) { x->p = end; return x->f ? fail(x, AML_ENOTFOUND, "IndexField sem campos") : ST_OK; }
            const u8 *se = x->end; x->end = end;
            st = def_field_list(x, end, 1, NULL, in, dn, NULL, 0, fl);
            x->end = se; x->p = end;
            return st;
        }
        case 0x87: {   /* BankField */
            aml_name_t nr, nb;
            if (pkg_end(x, &end) || parse_name(x, &nr) || parse_name(x, &nb)) return ST_ERR;
            aml_node_t *r = ns_resolve(x->scope, &nr), *bk = ns_resolve(x->scope, &nb);
            const u8 *se = x->end; x->end = end;
            st = eval_int(x, &i1);
            if (st) { x->end = se; return ST_ERR; }
            u8 fl = *x->p++;
            if (!r || !bk || r->type != AML_REGION || bk->type != AML_FIELD) { x->end = se; x->p = end; return x->f ? fail(x, AML_ENOTFOUND, "BankField sem região") : ST_OK; }
            st = def_field_list(x, end, 2, r, NULL, NULL, bk, i1, fl);
            x->end = se; x->p = end;
            return st;
        }
        case 0x88: {   /* DataTableRegion(Name, Signature, OEMID, OEMTableID) */
            if (parse_name(x, &nm)) return ST_ERR;
            aml_val_t s[3]; memset(s, 0, sizeof s);
            for (int i = 0; i < 3; i++) if (eval(x, &s[i])) { for (int k = 0; k < i; k++) aml_val_free(&s[k]); return ST_ERR; }
            char sig[5] = {0}, oem[7] = {0}, oemt[9] = {0};
            if (s[0].type == AML_STR) memcpy(sig, s[0].b->d, s[0].b->len < 4 ? s[0].b->len : 4);
            if (s[1].type == AML_STR) memcpy(oem, s[1].b->d, s[1].b->len < 6 ? s[1].b->len : 6);
            if (s[2].type == AML_STR) memcpy(oemt, s[2].b->d, s[2].b->len < 8 ? s[2].b->len : 8);
            for (int i = 0; i < 3; i++) aml_val_free(&s[i]);
            const u8 *tb = aml_os_find_table(sig, oem, oemt, 0);
            bool ex; aml_node_t *n = ns_create(x, &nm, AML_REGION, &ex);
            if (!n) return x->f ? fail(x, AML_ENOTFOUND, "DataTableRegion sem escopo") : ST_OK;
            n->type = AML_REGION; n->u.r.space = AML_AS_MEM;
            n->u.r.base = (u64)tb;
            n->u.r.len = tb ? ((u64)tb[4] | (u64)tb[5] << 8 | (u64)tb[6] << 16 | (u64)tb[7] << 24) : 0;
            return ST_OK;
        }
        case 0x82: case 0x83: case 0x84: case 0x85: {   /* Device, Processor, PowerResource, ThermalZone */
            if (pkg_end(x, &end) || parse_name(x, &nm)) return ST_ERR;
            u8 ty = e == 0x82 ? AML_DEVICE : e == 0x83 ? AML_PROCESSOR : e == 0x84 ? AML_POWERRES : AML_THERMAL;
            bool ex; aml_node_t *n = ns_create(x, &nm, ty, &ex);
            if (!n) { char s[80]; name_str(&nm, s, sizeof s); aml_os_log("aml: escopo de %s não existe\n", s); x->p = end; return x->f ? fail(x, AML_ENOTFOUND, "objeto sem escopo") : ST_OK; }
            if (!ex || n->type == AML_SCOPE || n->type == AML_UNINIT) n->type = ty;
            if (e == 0x83) {
                n->u.cpu.id = x->p[0];
                n->u.cpu.pblk = (u32)x->p[1] | (u32)x->p[2] << 8 | (u32)x->p[3] << 16 | (u32)x->p[4] << 24;
                n->u.cpu.plen = x->p[5];
                x->p += 6;
            } else if (e == 0x84) {
                n->u.pr.level = x->p[0]; n->u.pr.order = (u16)(x->p[1] | x->p[2] << 8);
                x->p += 3;
            }
            return exec_body(x, n, end);
        }
        }
        x->p = start;
        return fail(x, AML_EINVAL, "opcode estendido desconhecido");
    }
    }

    if (is_lead(op)) {   /* nome: objeto ou chamada de método */
        x->p = start;
        if (parse_name(x, &nm)) return ST_ERR;
        aml_node_t *n = ns_resolve(x->scope, &nm);
        if (!n) {
            char s[80]; name_str(&nm, s, sizeof s);
            char m[120]; u32 o = 0; const char *pre = "nome não existe: ";
            while (*pre) m[o++] = *pre++;
            for (u32 i = 0; s[i] && o < sizeof m - 1; i++) m[o++] = s[i];
            m[o] = 0;
            return fail(x, AML_ENOTFOUND, m);
        }
        if (n->type == AML_METHOD) {
            aml_val_t args[7]; int na = n->u.m.flags & 7;
            memset(args, 0, sizeof args);
            for (int i = 0; i < na; i++) if (eval(x, &args[i])) { for (int k = 0; k < i; k++) aml_val_free(&args[k]); return ST_ERR; }
            st = call_method(x, n, args, na, out);
            for (int i = 0; i < na; i++) aml_val_free(&args[i]);
            return st;
        }
        return read_node(x, n, out);
    }
    x->p = start;
    char m[48]; u32 o = 0; const char *pre = "opcode desconhecido 0x";
    while (*pre) m[o++] = *pre++;
    o += put_hex(m + o, op, 2); m[o] = 0;
    return fail(x, AML_EINVAL, m);
}

static int exec_list(ex_t *x) {
    while (x->p < x->end) {
        aml_val_t v;
        int st = eval(x, &v);
        if (st == ST_RET) return ST_RET;
        aml_val_free(&v);
        if (st) return st;
    }
    return ST_OK;
}

/* ------------------------------------------------------------------ API */
static const char *const osi_yes[] = {
    "Windows 2000", "Windows 2001", "Windows 2001 SP1", "Windows 2001.1", "Windows 2001 SP2",
    "Windows 2001.1 SP1", "Windows 2006", "Windows 2006.1", "Windows 2006 SP1", "Windows 2006 SP2",
    "Windows 2009", "Windows 2012", "Windows 2013", "Windows 2015", "Windows 2016", "Windows 2017",
    "Windows 2017.2", "Windows 2018", "Windows 2018.2", "Windows 2019", "Windows 2020", "Windows 2021",
    "Windows 2022", "Extended Address Space Descriptor", "Module Device", "Processor Device",
    "3.0 Thermal Model", "3.0 _SCP Extensions", "Processor Aggregator Device", NULL
};
static int native_osi(aml_val_t *a, int n, aml_val_t *r) {
    v_int(r, 0);
    if (n < 1 || a[0].type != AML_STR) return AML_OK;
    for (int i = 0; osi_yes[i]; i++) {
        u32 k = 0; while (osi_yes[i][k]) k++;
        if (k == a[0].b->len && !memcmp(osi_yes[i], a[0].b->d, k)) { v_int(r, g_mask); break; }
    }
    return AML_OK;
}

void aml_init(void) {
    if (g_root) return;
    g_root = node_new(NULL, "\\___", AML_SCOPE);
    static const char *const sc[] = { "_GPE", "_PR_", "_SB_", "_SI_", "_TZ_" };
    for (int i = 0; i < 5; i++) node_new(g_root, sc[i], AML_SCOPE);
    aml_node_t *n = node_new(g_root, "_OSI", AML_METHOD);
    n->u.m.flags = 1; n->u.m.native = native_osi;
    n = node_new(g_root, "_OS_", AML_STR); v_str(&n->v, "Microsoft Windows NT", 20);
    n = node_new(g_root, "_REV", AML_INT); n->v = aml_int(2);
    n = node_new(g_root, "_GL_", AML_MUTEX);
}

int aml_load_table(const void *sdt) {
    const u8 *t = sdt;
    if (!t) return AML_EINVAL;
    aml_init();
    u32 len = (u32)t[4] | (u32)t[5] << 8 | (u32)t[6] << 16 | (u32)t[7] << 24;
    if (len < 36) return AML_EINVAL;
    if (!memcmp(t, "DSDT", 4)) {
        if (t[8] < 2) { g_mask = 0xFFFFFFFFull; g_bits = 32; } else { g_mask = ~0ull; g_bits = 64; }
    }
    ex_t x; memset(&x, 0, sizeof x);
    x.p = t + 36; x.end = t + len; x.scope = g_root;
    u32 e0 = g_errs;
    while (x.p < x.end) {
        aml_val_t v;
        int st = eval(&x, &v);
        aml_val_free(&v);
        if (st == ST_ERR) {   /* um termo de nível de tabela quebrado não derruba o resto se der para seguir */
            aml_os_log("aml: erro no nível da tabela %c%c%c%c (offset %u)\n", t[0], t[1], t[2], t[3], (u32)(x.p - t));
            return AML_EINVAL;
        }
    }
    return g_errs > e0 ? AML_OK : AML_OK;
}

int aml_eval(aml_node_t *n, const aml_val_t *args, int nargs, aml_val_t *ret) {
    ex_t x; memset(&x, 0, sizeof x);
    aml_val_t dummy;
    if (!ret) ret = &dummy;
    memset(ret, 0, sizeof *ret);
    if (!n) return AML_ENOTFOUND;
    int st = eval_node(&x, n, args, nargs, ret);
    if (ret == &dummy) aml_val_free(&dummy);
    if (st) { aml_val_free(ret); return x.err ? x.err : AML_EINVAL; }
    return AML_OK;
}

int aml_eval_path(aml_node_t *scope, const char *path, const aml_val_t *args, int nargs, aml_val_t *ret) {
    aml_node_t *n = aml_find(scope, path);
    if (!n) { if (ret) memset(ret, 0, sizeof *ret); return AML_ENOTFOUND; }
    return aml_eval(n, args, nargs, ret);
}

int aml_eval_int(aml_node_t *scope, const char *path, u64 *out) {
    aml_val_t v;
    int r = aml_eval_path(scope, path, NULL, 0, &v);
    if (r) return r;
    r = aml_val_to_int(&v, out);
    aml_val_free(&v);
    return r;
}

int aml_call_int1(aml_node_t *scope, const char *path, u64 arg, u64 *out) {
    aml_val_t a = aml_int(arg), v;
    int r = aml_eval_path(scope, path, &a, 1, &v);
    if (r) return r;
    if (out) r = aml_val_to_int(&v, out);
    aml_val_free(&v);
    return r;
}

/* EISA ID comprimido (ACPI 6.1.5): "PNP0C0C" <-> 0x0C0CD041 */
static void eisa_str(u32 id, char *s) {
    static const char H[] = "0123456789ABCDEF";
    u32 v = ((id & 0xFF) << 24) | ((id & 0xFF00) << 8) | ((id >> 8) & 0xFF00) | (id >> 24);
    s[0] = (char)('@' + ((v >> 26) & 31)); s[1] = (char)('@' + ((v >> 21) & 31)); s[2] = (char)('@' + ((v >> 16) & 31));
    s[3] = H[(v >> 12) & 15]; s[4] = H[(v >> 8) & 15]; s[5] = H[(v >> 4) & 15]; s[6] = H[v & 15]; s[7] = 0;
}
static bool id_match(const aml_val_t *v, const char *hid) {
    u32 n = 0; while (hid[n]) n++;
    if (v->type == AML_STR) return v->b->len == n && !memcmp(v->b->d, hid, n);
    if (v->type == AML_INT) { char s[8]; eisa_str((u32)v->i, s); return n == 7 && !memcmp(s, hid, 7); }
    return false;
}
bool aml_device_match(aml_node_t *dev, const char *hid) {
    aml_val_t v;
    if (aml_eval_path(dev, "_HID", NULL, 0, &v) == AML_OK) {
        bool m = id_match(&v, hid);
        aml_val_free(&v);
        if (m) return true;
    }
    if (aml_eval_path(dev, "_CID", NULL, 0, &v) == AML_OK) {
        bool m = false;
        if (v.type == AML_PKG) { for (u32 i = 0; i < v.b->len && !m; i++) m = id_match(&v.b->el[i], hid); }
        else m = id_match(&v, hid);
        aml_val_free(&v);
        return m;
    }
    return false;
}

int aml_device_sta(aml_node_t *dev, u64 *sta) {
    *sta = 0x0F;
    aml_node_t *s = aml_find(dev, "_STA");
    if (!s) return AML_OK;
    return aml_eval_int(dev, "_STA", sta);
}

typedef struct { u32 ini, present, devs; } ini_ctx_t;
static int ini_cb(aml_node_t *n, int depth, void *c) {
    (void)depth;
    ini_ctx_t *ctx = c;
    if (n->type != AML_DEVICE && n->type != AML_PROCESSOR && n->type != AML_THERMAL && n->type != AML_POWERRES) {
        return n->type == AML_SCOPE ? 0 : 1;
    }
    ctx->devs++;
    u64 sta = 0x0F;
    if (aml_device_sta(n, &sta) != AML_OK) sta = 0;
    if (!(sta & 1)) return (sta & 8) ? 0 : 1;   /* ausente: só desce se "funcionando" */
    ctx->present++;
    if (aml_find(n, "_INI")) { aml_val_t r; if (aml_eval_path(n, "_INI", NULL, 0, &r) == AML_OK) { ctx->ini++; aml_val_free(&r); } }
    return 0;
}

int aml_init_devices(void) {
    ini_ctx_t c = {0, 0, 0};
    aml_val_t r;
    if (aml_find(g_root, "\\_SB._INI") && aml_eval_path(g_root, "\\_SB._INI", NULL, 0, &r) == AML_OK) { aml_val_free(&r); c.ini++; }
    aml_walk(aml_find(g_root, "\\_SB"), ini_cb, &c);
    aml_walk(aml_find(g_root, "\\_TZ"), ini_cb, &c);
    aml_walk(aml_find(g_root, "\\_PR"), ini_cb, &c);
    aml_os_log("aml: %u dispositivos, %u presentes, %u _INI\n", c.devs, c.present, c.ini);
    return (int)c.present;
}

typedef struct { u8 space; u32 n; } reg_ctx_t;
static int reg_cb(aml_node_t *n, int depth, void *c) {
    (void)depth;
    reg_ctx_t *ctx = c;
    if (n->type != AML_DEVICE && n->type != AML_SCOPE && n->type != AML_PROCESSOR && n->type != AML_THERMAL) return 1;
    bool has_region = false;
    for (aml_node_t *k = n->child; k; k = k->next) if (k->type == AML_REGION && k->u.r.space == ctx->space) { has_region = true; break; }
    if (has_region && aml_find(n, "_REG")) {
        aml_val_t a[2] = { aml_int(ctx->space), aml_int(1) }, r;
        if (aml_eval_path(n, "_REG", a, 2, &r) == AML_OK) { aml_val_free(&r); ctx->n++; }
    }
    return 0;
}
void aml_run_reg(u8 space) {
    reg_ctx_t c = { space, 0 };
    aml_walk(g_root, reg_cb, &c);
    if (c.n) aml_os_log("aml: _REG(%u) em %u dispositivos\n", space, c.n);
}

/* ------------------------------------------------------------------ recursos (_CRS) */
int aml_res_walk(const u8 *buf, u32 len, int (*cb)(const aml_res_t *r, void *ctx), void *ctx) {
    u32 i = 0;
    while (i < len) {
        aml_res_t r; memset(&r, 0, sizeof r);
        u8 tag = buf[i];
        if (!(tag & 0x80)) {   /* descritor pequeno */
            u8 ty = (tag >> 3) & 0x0F, l = tag & 7;
            const u8 *d = buf + i + 1;
            if (i + 1 + l > len) return AML_EINVAL;
            if (ty == 0x0F) return AML_OK;   /* EndTag */
            if (ty == 0x04 && l >= 2) {      /* IRQ */
                u16 mask = (u16)(d[0] | d[1] << 8);
                r.kind = AML_RES_IRQ; r.flags = l >= 3 ? d[2] : 1;
                for (int k = 0; k < 16; k++) if (mask & (1u << k)) r.irq[r.count++] = (u32)k;
            } else if (ty == 0x05 && l >= 2) {   /* DMA */
                r.kind = AML_RES_DMA;
                for (int k = 0; k < 8; k++) if (d[0] & (1u << k)) r.irq[r.count++] = (u32)k;
            } else if (ty == 0x08 && l >= 7) {   /* IO */
                r.kind = AML_RES_IO; r.min = (u64)(d[1] | d[2] << 8); r.max = (u64)(d[3] | d[4] << 8); r.len = d[6];
            } else if (ty == 0x09 && l >= 3) {   /* Fixed IO */
                r.kind = AML_RES_IO; r.min = r.max = (u64)(d[0] | d[1] << 8) & 0x3FF; r.len = d[2];
            } else r.kind = AML_RES_OTHER;
            int s = cb(&r, ctx);
            if (s) return s;
            i += 1u + l;
            continue;
        }
        if (i + 3 > len) return AML_EINVAL;
        u16 l = (u16)(buf[i + 1] | buf[i + 2] << 8);
        const u8 *d = buf + i + 3;
        if (i + 3 + l > len) return AML_EINVAL;
        u8 ty = tag & 0x7F;
#define R16(o) ((u64)(d[o] | d[(o) + 1] << 8))
#define R32(o) ((u64)d[o] | (u64)d[(o) + 1] << 8 | (u64)d[(o) + 2] << 16 | (u64)d[(o) + 3] << 24)
#define R64(o) (R32(o) | R32((o) + 4) << 32)
        switch (ty) {
        case 0x05: r.kind = AML_RES_MEM; r.min = R32(1); r.max = R32(5); r.len = R32(13); break;     /* Memory32 */
        case 0x06: r.kind = AML_RES_MEM; r.min = R32(1); r.len = R32(5); r.max = r.min + r.len - 1; break; /* Memory32Fixed */
        case 0x07: r.kind = AML_RES_ADDR; r.type_spec = d[0]; r.min = R32(7); r.max = R32(11); r.len = R32(19); break;   /* DWord */
        case 0x08: r.kind = AML_RES_ADDR; r.type_spec = d[0]; r.min = R16(5); r.max = R16(7); r.len = R16(11); break;    /* Word */
        case 0x0A: r.kind = AML_RES_ADDR; r.type_spec = d[0]; r.min = R64(11); r.max = R64(19); r.len = R64(35); break;  /* QWord */
        case 0x09: {   /* Extended IRQ */
            r.kind = AML_RES_IRQ;
            u8 f = d[0];
            r.flags = (u8)(((f >> 1) & 1) | ((f >> 2) & 1) << 3 | ((f >> 3) & 1) << 4);
            u8 n = d[1];
            for (u8 k = 0; k < n && k < 16; k++) r.irq[r.count++] = (u32)R32(2 + 4 * k);
            break;
        }
        case 0x0C: {   /* GPIO Connection */
            r.kind = AML_RES_GPIO;
            r.flags = d[1];                     /* 0 = interrupção, 1 = E/S */
            u16 pt = (u16)R16(11), pe = (u16)R16(14);   /* tabela de pinos: offset a partir do início do descritor */
            for (u32 o = pt; o + 1 < pe && r.count < 16; o += 2) {
                u32 at = o - 3;
                if (at + 1 < l) r.irq[r.count++] = (u32)(d[at] | d[at + 1] << 8);
            }
            break;
        }
        default: r.kind = AML_RES_OTHER; break;
        }
#undef R16
#undef R32
#undef R64
        int s = cb(&r, ctx);
        if (s) return s;
        i += 3u + l;
    }
    return AML_OK;
}
