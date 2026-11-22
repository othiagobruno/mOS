/*
 * mOS libSystem — libffi (/usr/lib/libffi.dylib): ffi_prep_cif(_var), ffi_call, closures e os ffi_type_* do
 * corpus (docs/lacunas/leonardo.md: o _ctypes do Python embutido em anki, calibre, dropbox, capcut...).
 * Código original do mOS (MIT); a API e o layout das estruturas públicas são os da libffi 3.x.
 *
 *   x86_64: System V. Agregados de até 16 bytes classificados por oito bytes (INTEGER/SSE; long double e o que
 *           estiver desalinhado vão para a memória), retorno em rax/rdx/xmm0/xmm1, x87 para long double e retorno
 *           indireto (rdi) acima de 16 bytes. A chamada é cx_ffi_x64_call (ffi_x64.S), irmã do cx_x64_call do
 *           NSInvocation, que também devolve xmm1 e st(0).
 *   arm64:  o PCS da Apple: HFA (1 a 4 float/double iguais) em v consecutivos, agregado de até 16 bytes em x,
 *           maior por ponteiro, retorno indireto em x8, pilha com o tamanho/alinhamento natural de cada argumento e
 *           os variádicos todos na pilha em casas de 8 bytes. A chamada é o cx_a64_call (ffi_arm64.S).
 *   closures: memória MAP_JIT com um trampolim que carrega o closure e salta para a entrada em assembly
 *           (cx_ffi_*_closure_entry), que guarda os registradores de argumento e chama o decodificador em C.
 *           Os dados do closure (cif, fun, user_data) ficam num cabeçalho antes do bloco devolvido ao app, então o
 *           sizeof(ffi_closure) que o app compilou não importa.
 * Teste: tools/ffi_mos_test.sh (no arm64 roda contra funções do próprio host).
 */
#include <stddef.h>
#include <stdint.h>

void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
void *mmap(void *addr, size_t len, int prot, int flags, int fd, long off);
int munmap(void *addr, size_t len);
void pthread_jit_write_protect_np(int enabled);
void sys_icache_invalidate(void *start, size_t len);

typedef struct ffi_type { size_t size; unsigned short alignment, type; struct ffi_type **elements; } ffi_type;
enum { T_VOID = 0, T_INT = 1, T_FLOAT = 2, T_DOUBLE = 3, T_LDOUBLE = 4, T_U8 = 5, T_S8 = 6, T_U16 = 7, T_S16 = 8, T_U32 = 9,
       T_S32 = 10, T_U64 = 11, T_S64 = 12, T_STRUCT = 13, T_PTR = 14, T_COMPLEX = 15 };
typedef enum { FFI_OK = 0, FFI_BAD_TYPEDEF, FFI_BAD_ABI, FFI_BAD_ARGTYPE } ffi_status;
typedef struct { int abi; unsigned nargs; ffi_type **arg_types; ffi_type *rtype; unsigned bytes; unsigned flags; } ffi_cif;
/* flags: o mOS guarda nele o número de argumentos fixos + 1 de uma chamada variádica (0 = não variádica) */

#define TYPE(name, sz, al, id) ffi_type ffi_type_##name = {sz, al, id, 0};
TYPE(void, 1, 1, T_VOID) TYPE(uint8, 1, 1, T_U8) TYPE(sint8, 1, 1, T_S8) TYPE(uint16, 2, 2, T_U16) TYPE(sint16, 2, 2, T_S16)
TYPE(uint32, 4, 4, T_U32) TYPE(sint32, 4, 4, T_S32) TYPE(uint64, 8, 8, T_U64) TYPE(sint64, 8, 8, T_S64)
TYPE(float, 4, 4, T_FLOAT) TYPE(double, 8, 8, T_DOUBLE) TYPE(pointer, 8, 8, T_PTR)
#if defined(__aarch64__)
TYPE(longdouble, 8, 8, T_DOUBLE)              /* no arm64 da Apple long double é double */
#else
TYPE(longdouble, 16, 16, T_LDOUBLE)
#endif
static ffi_type *cf_el[] = {&ffi_type_float, 0}, *cd_el[] = {&ffi_type_double, 0}, *cl_el[] = {&ffi_type_longdouble, 0};
ffi_type ffi_type_complex_float = {8, 4, T_COMPLEX, cf_el};
ffi_type ffi_type_complex_double = {16, 8, T_COMPLEX, cd_el};
#if defined(__aarch64__)
ffi_type ffi_type_complex_longdouble = {16, 8, T_COMPLEX, cl_el};
#else
ffi_type ffi_type_complex_longdouble = {32, 16, T_COMPLEX, cl_el};
#endif

static size_t align_up(size_t v, size_t a) { return a ? (v + a - 1) / a * a : v; }
static ffi_status init_type(ffi_type *t) {
    if (!t) return FFI_BAD_TYPEDEF;
    if (t->type == T_STRUCT) {
        if (!t->elements || !t->elements[0]) return FFI_BAD_TYPEDEF;
        size_t sz = 0, al = 1;
        for (ffi_type **e = t->elements; *e; e++) {
            if (init_type(*e)) return FFI_BAD_TYPEDEF;
            sz = align_up(sz, (*e)->alignment) + (*e)->size;
            if ((*e)->alignment > al) al = (*e)->alignment;
        }
        t->size = align_up(sz, al);
        t->alignment = (unsigned short)al;
    } else if (t->type == T_COMPLEX) {
        if (!t->elements || !t->elements[0] || init_type(t->elements[0])) return FFI_BAD_TYPEDEF;
        t->size = t->elements[0]->size * 2;
        t->alignment = t->elements[0]->alignment;
    } else if (t->type == T_INT) {
        t->size = 4; t->alignment = 4;
    }
    return t->size && t->alignment ? FFI_OK : FFI_BAD_TYPEDEF;
}
static ffi_status prep(ffi_cif *cif, int abi, unsigned nfixed, unsigned n, ffi_type *rt, ffi_type **at, int var) {
    if (!cif || !rt || (n && !at)) return FFI_BAD_TYPEDEF;
    if (abi <= 0 || abi > 8) return FFI_BAD_ABI;
    if (rt->type != T_VOID && init_type(rt)) return FFI_BAD_TYPEDEF;
    for (unsigned i = 0; i < n; i++) {
        if (!at[i] || at[i]->type == T_VOID) return FFI_BAD_ARGTYPE;
        if (init_type(at[i])) return FFI_BAD_TYPEDEF;
    }
    cif->abi = abi; cif->nargs = n; cif->arg_types = at; cif->rtype = rt;
    size_t b = 0;
    for (unsigned i = 0; i < n; i++) b = align_up(b, at[i]->alignment > 8 ? at[i]->alignment : 8) + align_up(at[i]->size, 8);
    cif->bytes = (unsigned)align_up(b, 16);
    cif->flags = var ? nfixed + 1 : 0;
    return FFI_OK;
}
ffi_status ffi_prep_cif(ffi_cif *cif, int abi, unsigned n, ffi_type *rt, ffi_type **at) { return prep(cif, abi, n, n, rt, at, 0); }
ffi_status ffi_prep_cif_var(ffi_cif *cif, int abi, unsigned nfixed, unsigned n, ffi_type *rt, ffi_type **at) {
    if (nfixed > n) return FFI_BAD_ARGTYPE;
    for (unsigned i = nfixed; i < n; i++)            /* promoções do C: float vira double, inteiros pequenos viram int */
        if (at && at[i] && (at[i]->type == T_FLOAT || at[i]->type == T_U8 || at[i]->type == T_S8 || at[i]->type == T_U16 || at[i]->type == T_S16))
            return FFI_BAD_ARGTYPE;
    return prep(cif, abi, nfixed, n, rt, at, 1);
}
ffi_status ffi_get_struct_offsets(int abi, ffi_type *t, size_t *off) {
    (void)abi;
    if (!t || t->type != T_STRUCT) return FFI_BAD_TYPEDEF;
    if (init_type(t)) return FFI_BAD_TYPEDEF;
    size_t o = 0;
    int i = 0;
    for (ffi_type **e = t->elements; *e; e++, i++) {
        o = align_up(o, (*e)->alignment);
        if (off) off[i] = o;
        o += (*e)->size;
    }
    return FFI_OK;
}
__attribute__((unused)) static int is_int(int t) { return t == T_INT || (t >= T_U8 && t <= T_S64) || t == T_PTR; }
/* valor inteiro estendido para 64 bits, como o chamador do x86_64 e do arm64 deixa no registrador */
static uint64_t widen(int t, const void *p) {
    switch (t) {
    case T_U8: return *(const uint8_t *)p;
    case T_S8: return (uint64_t)(int64_t)*(const int8_t *)p;
    case T_U16: return *(const uint16_t *)p;
    case T_S16: return (uint64_t)(int64_t)*(const int16_t *)p;
    case T_U32: return *(const uint32_t *)p;
    case T_S32: case T_INT: return (uint64_t)(int64_t)*(const int32_t *)p;
    default: return *(const uint64_t *)p;
    }
}

/* ================================================================ x86_64 System V */
#if !defined(__aarch64__)
enum { C_NONE, C_INT, C_SSE, C_MEM };
static int merge(int a, int b) {
    if (a == b) return a;
    if (a == C_NONE) return b;
    if (b == C_NONE) return a;
    if (a == C_MEM || b == C_MEM) return C_MEM;
    if (a == C_INT || b == C_INT) return C_INT;
    return C_SSE;
}
static void classify_at(const ffi_type *t, size_t off, int cls[2]) {
    if (t->type == T_STRUCT) {
        size_t o = off;
        for (ffi_type **e = t->elements; *e; e++) {
            o = align_up(o, (*e)->alignment);
            classify_at(*e, o, cls);
            o += (*e)->size;
        }
        return;
    }
    if (t->type == T_COMPLEX) {
        classify_at(t->elements[0], off, cls);
        classify_at(t->elements[0], off + t->elements[0]->size, cls);
        return;
    }
    if (off % t->alignment) { cls[0] = cls[1] = C_MEM; return; }   /* campo desalinhado (struct empacotada) */
    if (t->type == T_LDOUBLE) { cls[0] = cls[1] = C_MEM; return; }
    int c = t->type == T_FLOAT || t->type == T_DOUBLE ? C_SSE : C_INT;
    size_t first = off / 8, last = (off + t->size - 1) / 8;
    for (size_t k = first; k <= last && k < 2; k++) cls[k] = merge(cls[k], c);
}
/* devolve o número de oito bytes em registradores (0 = memória) e as classes */
static int classify(const ffi_type *t, int cls[2]) {
    cls[0] = cls[1] = C_NONE;
    if (t->type == T_LDOUBLE) return 0;
    if (t->type != T_STRUCT && t->type != T_COMPLEX) { cls[0] = (t->type == T_FLOAT || t->type == T_DOUBLE) ? C_SSE : C_INT; return 1; }
    if (t->size > 16) return 0;
    classify_at(t, 0, cls);
    int n = t->size > 8 ? 2 : 1;
    for (int k = 0; k < n; k++) { if (cls[k] == C_MEM) return 0; if (cls[k] == C_NONE) cls[k] = C_SSE; }
    return n;
}
typedef struct {
    uint64_t x[6], nstack;
    uint8_t *stack;
    uint8_t v[8][16];
    uint64_t rax, rdx;
    uint8_t rv[16], rv1[16], st0[16];
    uint64_t nsse, x87;
} ffi_x64_frame_t;
_Static_assert(offsetof(ffi_x64_frame_t, v) == 64 && offsetof(ffi_x64_frame_t, rax) == 192 && offsetof(ffi_x64_frame_t, rv) == 208 &&
               offsetof(ffi_x64_frame_t, rv1) == 224 && offsetof(ffi_x64_frame_t, st0) == 240 && offsetof(ffi_x64_frame_t, nsse) == 256 &&
               offsetof(ffi_x64_frame_t, x87) == 264, "quadro de cx_ffi_x64_call (ffi_x64.S)");
void cx_ffi_x64_call(void *fn, ffi_x64_frame_t *f);

void ffi_call(ffi_cif *cif, void (*fn)(void), void *rvalue, void **avalue) {
    ffi_x64_frame_t f;
    memset(&f, 0, sizeof f);
    uint8_t *stk = cif->bytes ? calloc(1, cif->bytes + 16) : 0;
    size_t so = 0;
    int gi = 0, si = 0;
    ffi_type *rt = cif->rtype;
    int rcls[2], rn = rt->type == T_VOID ? -1 : classify(rt, rcls);
    uint8_t rtmp[32];
    void *rdst = rvalue ? rvalue : rtmp;
    if (rn == 0 && rt->type != T_LDOUBLE) f.x[gi++] = (uint64_t)rdst;    /* retorno indireto: ponteiro em rdi */
    for (unsigned i = 0; i < cif->nargs; i++) {
        ffi_type *t = cif->arg_types[i];
        const uint8_t *p = avalue[i];
        int cls[2], n = classify(t, cls), ng = 0, ns = 0;
        for (int k = 0; k < n; k++) { if (cls[k] == C_INT) ng++; else ns++; }
        if (n && gi + ng <= 6 && si + ns <= 8) {
            if (t->type != T_STRUCT && t->type != T_COMPLEX) {
                if (cls[0] == C_INT) f.x[gi++] = widen(t->type, p);
                else { memcpy(f.v[si++], p, t->size); }
            } else
                for (int k = 0; k < n; k++) {
                    size_t len = t->size - (size_t)k * 8 < 8 ? t->size - (size_t)k * 8 : 8;
                    if (cls[k] == C_INT) { uint64_t w = 0; memcpy(&w, p + k * 8, len); f.x[gi++] = w; }
                    else memcpy(f.v[si++], p + k * 8, len);
                }
        } else {
            size_t al = t->alignment > 8 ? t->alignment : 8;
            so = align_up(so, al);
            if (stk) {
                if (n == 1 && cls[0] == C_INT && t->type != T_STRUCT) { uint64_t w = widen(t->type, p); memcpy(stk + so, &w, 8); }
                else memcpy(stk + so, p, t->size);
            }
            so += align_up(t->size, 8);
        }
    }
    f.nstack = align_up(so, 16);
    f.stack = stk;
    f.nsse = (uint64_t)si;
    f.x87 = rt->type == T_LDOUBLE;
    cx_ffi_x64_call((void *)fn, &f);
    free(stk);
    if (!rvalue || rt->type == T_VOID) return;
    if (rt->type == T_LDOUBLE) { memcpy(rvalue, f.st0, 16); return; }
    if (rn == 0) return;                         /* o chamado já escreveu em rvalue */
    if (rt->type != T_STRUCT && rt->type != T_COMPLEX) {
        if (rcls[0] == C_INT) { uint64_t w = widen(rt->type, &f.rax); memcpy(rvalue, &w, 8); }   /* ffi_arg */
        else memcpy(rvalue, f.rv, rt->size);
        return;
    }
    uint8_t out[16];
    int ig = 0, is = 0;
    for (int k = 0; k < rn; k++) {
        if (rcls[k] == C_INT) memcpy(out + k * 8, ig++ ? (void *)&f.rdx : (void *)&f.rax, 8);
        else memcpy(out + k * 8, is++ ? f.rv1 : f.rv, 8);
    }
    memcpy(rvalue, out, rt->size);
}

/* ---- closures: o trampolim põe o cabeçalho em r10 e salta para cx_ffi_x64_closure_entry */
typedef struct { ffi_cif *cif; void (*fun)(ffi_cif *, void *, void **, void *); void *ud; uint64_t magic; } clo_meta_t;
typedef struct { uint64_t x[6]; uint8_t v[8][16]; uint64_t rax, rdx; uint8_t rv[16], rv1[16], st0[16]; } ffi_x64_regs_t;
_Static_assert(offsetof(ffi_x64_regs_t, v) == 48 && offsetof(ffi_x64_regs_t, rax) == 176 && offsetof(ffi_x64_regs_t, rv) == 192 &&
               offsetof(ffi_x64_regs_t, rv1) == 208 && offsetof(ffi_x64_regs_t, st0) == 224, "quadro de cx_ffi_x64_closure_entry");
void cx_ffi_x64_closure_entry(void);
#define MAXA 64
/* devolve 1 (rax/rdx/xmm0/xmm1) ou 2 (x87) */
int cx_ffi_x64_closure_inner(clo_meta_t *m, ffi_x64_regs_t *r, uint8_t *stack) {
    ffi_cif *cif = m->cif;
    ffi_type *rt = cif->rtype;
    void *av[MAXA];
    uint8_t tmp[MAXA][16] __attribute__((aligned(16)));
    uint8_t ret[32] __attribute__((aligned(16)));
    memset(ret, 0, sizeof ret);
    int gi = 0, si = 0;
    size_t so = 0;
    int rcls[2], rn = rt->type == T_VOID ? -1 : classify(rt, rcls);
    void *rvalue = ret;
    if (rn == 0 && rt->type != T_LDOUBLE) rvalue = (void *)r->x[gi++];
    unsigned n = cif->nargs < MAXA ? cif->nargs : MAXA;
    for (unsigned i = 0; i < n; i++) {
        ffi_type *t = cif->arg_types[i];
        int cls[2], k = classify(t, cls), ng = 0, ns = 0;
        for (int q = 0; q < k; q++) { if (cls[q] == C_INT) ng++; else ns++; }
        if (k && gi + ng <= 6 && si + ns <= 8) {
            memset(tmp[i], 0, 16);
            for (int q = 0; q < k; q++) {
                if (cls[q] == C_INT) memcpy(tmp[i] + q * 8, &r->x[gi++], 8);
                else memcpy(tmp[i] + q * 8, r->v[si++], 8);
            }
            av[i] = tmp[i];
        } else {
            so = align_up(so, t->alignment > 8 ? t->alignment : 8);
            av[i] = stack + so;
            so += align_up(t->size, 8);
        }
    }
    m->fun(cif, rvalue, av, m->ud);
    if (rt->type == T_LDOUBLE) { memcpy(r->st0, ret, 16); return 2; }
    if (rn == 0) { r->rax = (uint64_t)rvalue; return 1; }
    if (rn < 0) return 1;
    if (rt->type != T_STRUCT && rt->type != T_COMPLEX) {
        if (rcls[0] == C_INT) r->rax = widen(rt->type, ret);
        else memcpy(r->rv, ret, 16);
        return 1;
    }
    int ig = 0, is = 0;
    for (int q = 0; q < rn; q++) {
        if (rcls[q] == C_INT) memcpy(ig++ ? (void *)&r->rdx : (void *)&r->rax, ret + q * 8, 8);
        else memcpy(is++ ? r->rv1 : r->rv, ret + q * 8, 8);
    }
    return 1;
}
#define TRAMP_SIZE 24
static void write_tramp(uint8_t *code, clo_meta_t *m) {
    uint64_t a = (uint64_t)m, e = (uint64_t)cx_ffi_x64_closure_entry;
    code[0] = 0x49; code[1] = 0xBA; memcpy(code + 2, &a, 8);     /* movabs r10, meta */
    code[10] = 0x49; code[11] = 0xBB; memcpy(code + 12, &e, 8);  /* movabs r11, entrada */
    code[20] = 0x41; code[21] = 0xFF; code[22] = 0xE3;           /* jmp r11 */
    code[23] = 0xCC;
}

/* ================================================================ arm64 (PCS da Apple) */
#else
typedef struct {
    uint64_t x[8], x8, nstack;
    uint8_t *stack;
    uint64_t pad;
    uint8_t v[8][16];
    uint64_t rx[2];
    uint8_t rv[4][16];
} cx_a64_frame_t;
_Static_assert(offsetof(cx_a64_frame_t, v) == 96 && offsetof(cx_a64_frame_t, rx) == 224 && offsetof(cx_a64_frame_t, rv) == 240,
               "quadro de ffi_arm64.S");
void cx_a64_call(void *fn, cx_a64_frame_t *f);
/* HFA: 1 a 4 membros float ou double iguais (struct aninhada e complexo contam); devolve o número e o tipo */
static int hfa_walk(const ffi_type *t, int *base, int *n) {
    if (t->type == T_FLOAT || t->type == T_DOUBLE) {
        if (*base && *base != t->type) return 0;
        *base = t->type;
        return ++*n <= 4;
    }
    if (t->type == T_COMPLEX) return hfa_walk(t->elements[0], base, n) && hfa_walk(t->elements[0], base, n);
    if (t->type != T_STRUCT) return 0;
    for (ffi_type **e = t->elements; *e; e++) if (!hfa_walk(*e, base, n)) return 0;
    return 1;
}
static int hfa(const ffi_type *t, int *base) {
    if (t->type != T_STRUCT && t->type != T_COMPLEX) return 0;
    int b = 0, n = 0;
    if (!hfa_walk(t, &b, &n) || !n) return 0;
    size_t esz = b == T_FLOAT ? 4 : 8;
    if (t->size != esz * (size_t)n) return 0;      /* com preenchimento não é HFA */
    *base = b;
    return n;
}
typedef struct { uint8_t *stk; size_t so, cap; } a64stk_t;
static void *stk_slot(a64stk_t *s, size_t sz, size_t al) {
    s->so = align_up(s->so, al);
    if (s->so + sz > s->cap) return 0;
    void *p = s->stk + s->so;
    s->so += sz;
    return p;
}
void ffi_call(ffi_cif *cif, void (*fn)(void), void *rvalue, void **avalue) {
    cx_a64_frame_t f;
    memset(&f, 0, sizeof f);
    size_t cap = (size_t)cif->bytes + 16 * (cif->nargs + 1);
    a64stk_t s = {calloc(1, cap), 0, cap};
    void **copies = calloc(cif->nargs + 1, sizeof(void *));
    unsigned nfixed = cif->flags ? cif->flags - 1 : cif->nargs;
    int gi = 0, vi = 0;
    ffi_type *rt = cif->rtype;
    uint8_t rtmp[64];
    void *rdst = rvalue ? rvalue : rtmp;
    int rbase = 0, rh = rt->type == T_VOID ? 0 : hfa(rt, &rbase);
    if ((rt->type == T_STRUCT || rt->type == T_COMPLEX) && !rh && rt->size > 16) f.x8 = (uint64_t)rdst;
    for (unsigned i = 0; i < cif->nargs; i++) {
        ffi_type *t = cif->arg_types[i];
        const uint8_t *p = avalue[i];
        if (i >= nfixed) {                           /* variádico: pilha, casas de 8 bytes */
            void *slot = stk_slot(&s, align_up(t->size > 16 ? 8 : t->size, 8), 8);
            if (!slot) continue;
            if (is_int(t->type)) { uint64_t w = widen(t->type, p); memcpy(slot, &w, 8); }
            else if (t->size > 16) {
                void *c = malloc(t->size);
                if (c) memcpy(c, p, t->size);
                copies[i] = c;
                memcpy(slot, &c, 8);
            } else memcpy(slot, p, t->size);
            continue;
        }
        int base = 0, h = hfa(t, &base);
        if (t->type == T_FLOAT || t->type == T_DOUBLE) {
            if (vi < 8) memcpy(f.v[vi++], p, t->size);
            else { void *slot = stk_slot(&s, t->size, t->size); if (slot) memcpy(slot, p, t->size); }
        } else if (h) {
            size_t esz = base == T_FLOAT ? 4 : 8;
            if (vi + h <= 8) for (int k = 0; k < h; k++) memcpy(f.v[vi++], p + k * esz, esz);
            else { vi = 8; void *slot = stk_slot(&s, t->size, t->alignment); if (slot) memcpy(slot, p, t->size); }
        } else if (is_int(t->type)) {
            if (gi < 8) f.x[gi++] = widen(t->type, p);
            else { void *slot = stk_slot(&s, t->size, t->size); if (slot) memcpy(slot, p, t->size); }
        } else if (t->size > 16) {                   /* cópia e ponteiro */
            void *c = malloc(t->size);
            if (c) memcpy(c, p, t->size);
            copies[i] = c;
            if (gi < 8) f.x[gi++] = (uint64_t)c;
            else { void *slot = stk_slot(&s, 8, 8); if (slot) memcpy(slot, &c, 8); }
        } else {
            int n = t->size > 8 ? 2 : 1;
            if (gi + n <= 8) {
                uint64_t w[2] = {0, 0};
                memcpy(w, p, t->size);
                for (int k = 0; k < n; k++) f.x[gi++] = w[k];
            } else {
                gi = 8;
                void *slot = stk_slot(&s, align_up(t->size, 8), t->alignment > 8 ? t->alignment : 8);
                if (slot) memcpy(slot, p, t->size);
            }
        }
    }
    f.nstack = align_up(s.so, 16);
    f.stack = s.stk;
    cx_a64_call((void *)fn, &f);
    free(s.stk);
    for (unsigned i = 0; i < cif->nargs; i++) free(copies[i]);
    free(copies);
    if (!rvalue || rt->type == T_VOID) return;
    if (rt->type == T_FLOAT || rt->type == T_DOUBLE) { memcpy(rvalue, f.rv[0], rt->size); return; }
    if (is_int(rt->type)) { uint64_t w = widen(rt->type, &f.rx[0]); memcpy(rvalue, &w, 8); return; }
    if (rh) { size_t esz = rbase == T_FLOAT ? 4 : 8; for (int k = 0; k < rh; k++) memcpy((uint8_t *)rvalue + k * esz, f.rv[k], esz); return; }
    if (rt->size <= 16) memcpy(rvalue, f.rx, rt->size);
}

/* ---- closures: x17 = cabeçalho, entrada cx_ffi_a64_closure_entry */
typedef struct { ffi_cif *cif; void (*fun)(ffi_cif *, void *, void **, void *); void *ud; uint64_t magic; } clo_meta_t;
typedef struct { uint64_t x[8], x8, pad; uint8_t v[8][16]; uint64_t rx[2]; uint8_t rv[4][16]; } ffi_a64_regs_t;
_Static_assert(offsetof(ffi_a64_regs_t, v) == 80 && offsetof(ffi_a64_regs_t, rx) == 208 && offsetof(ffi_a64_regs_t, rv) == 224 &&
               sizeof(ffi_a64_regs_t) == 288, "quadro de cx_ffi_a64_closure_entry");
void cx_ffi_a64_closure_entry(void);
#define MAXA 64
void cx_ffi_a64_closure_inner(clo_meta_t *m, ffi_a64_regs_t *r, uint8_t *stack) {
    ffi_cif *cif = m->cif;
    ffi_type *rt = cif->rtype;
    void *av[MAXA];
    uint8_t tmp[MAXA][32] __attribute__((aligned(16)));
    uint8_t ret[64] __attribute__((aligned(16)));
    memset(ret, 0, sizeof ret);
    unsigned nfixed = cif->flags ? cif->flags - 1 : cif->nargs;
    int gi = 0, vi = 0;
    size_t so = 0;
    int rbase = 0, rh = rt->type == T_VOID ? 0 : hfa(rt, &rbase);
    void *rvalue = ret;
    if ((rt->type == T_STRUCT || rt->type == T_COMPLEX) && !rh && rt->size > 16) rvalue = (void *)r->x8;
    unsigned n = cif->nargs < MAXA ? cif->nargs : MAXA;
    for (unsigned i = 0; i < n; i++) {
        ffi_type *t = cif->arg_types[i];
        if (i >= nfixed) {
            so = align_up(so, 8);
            if (t->size > 16) { void *c; memcpy(&c, stack + so, 8); av[i] = c; }
            else av[i] = stack + so;
            so += align_up(t->size > 16 ? 8 : t->size, 8);
            continue;
        }
        int base = 0, h = hfa(t, &base);
        if (t->type == T_FLOAT || t->type == T_DOUBLE) {
            if (vi < 8) av[i] = r->v[vi++];
            else { so = align_up(so, t->size); av[i] = stack + so; so += t->size; }
        } else if (h) {
            size_t esz = base == T_FLOAT ? 4 : 8;
            if (vi + h <= 8) { for (int k = 0; k < h; k++) memcpy(tmp[i] + k * esz, r->v[vi++], esz); av[i] = tmp[i]; }
            else { vi = 8; so = align_up(so, t->alignment); av[i] = stack + so; so += t->size; }
        } else if (is_int(t->type)) {
            if (gi < 8) av[i] = &r->x[gi++];          /* little-endian: os bytes baixos vêm primeiro */
            else { so = align_up(so, t->size); av[i] = stack + so; so += t->size; }
        } else if (t->size > 16) {
            void *c;
            if (gi < 8) c = (void *)r->x[gi++];
            else { so = align_up(so, 8); memcpy(&c, stack + so, 8); so += 8; }
            av[i] = c;
        } else {
            int k = t->size > 8 ? 2 : 1;
            if (gi + k <= 8) { memcpy(tmp[i], &r->x[gi], (size_t)k * 8); gi += k; av[i] = tmp[i]; }
            else { gi = 8; so = align_up(so, t->alignment > 8 ? t->alignment : 8); av[i] = stack + so; so += align_up(t->size, 8); }
        }
    }
    m->fun(cif, rvalue, av, m->ud);
    if (rt->type == T_VOID) return;
    if (rt->type == T_FLOAT || rt->type == T_DOUBLE) { memset(r->rv[0], 0, 16); memcpy(r->rv[0], ret, rt->size); return; }
    if (is_int(rt->type)) { r->rx[0] = widen(rt->type, ret); return; }
    if (rh) { size_t esz = rbase == T_FLOAT ? 4 : 8; for (int k = 0; k < rh; k++) { memset(r->rv[k], 0, 16); memcpy(r->rv[k], ret + k * esz, esz); } return; }
    if (rt->size <= 16) { r->rx[0] = r->rx[1] = 0; memcpy(r->rx, ret, rt->size); }
}
#define TRAMP_SIZE 32
static void write_tramp(uint8_t *code, clo_meta_t *m) {
    uint32_t ins[4] = {0x58000091u,           /* ldr x17, #16 (cabeçalho) */
                       0x580000B0u,           /* ldr x16, #20 (entrada) */
                       0xD61F0200u,           /* br x16 */
                       0xD503201Fu};          /* nop */
    uint64_t a = (uint64_t)m, e = (uint64_t)cx_ffi_a64_closure_entry;
    memcpy(code, ins, 16);
    memcpy(code + 16, &a, 8);
    memcpy(code + 24, &e, 8);
}
#endif

/* ================================================================ alocação dos closures */
/* bloco MAP_JIT: [clo_meta_t | preenchimento até 64][área do app (closure e trampolim)] */
#define CLO_MAGIC 0x6d6f7366666963ull
#define HDR 64
typedef struct { size_t len; } clo_len_t;
void *ffi_closure_alloc(size_t size, void **code) {
    size_t len = align_up(HDR + (size > 64 ? size : 64), 4096);
    uint8_t *b = mmap(0, len, 7, 0x1002 | 0x800, -1, 0);    /* PROT_RWX, MAP_ANON|MAP_PRIVATE|MAP_JIT */
    if (b == (void *)-1 || !b) return 0;
    clo_meta_t *m = (clo_meta_t *)b;
    pthread_jit_write_protect_np(0);                         /* página MAP_JIT: escrita só com a proteção aberta */
    m->magic = CLO_MAGIC;
    ((clo_len_t *)(b + sizeof(clo_meta_t)))->len = len;
    pthread_jit_write_protect_np(1);
    if (code) *code = b + HDR;
    return b + HDR;
}
void ffi_closure_free(void *p) {
    if (!p) return;
    uint8_t *b = (uint8_t *)p - HDR;
    clo_meta_t *m = (clo_meta_t *)b;
    if (m->magic != CLO_MAGIC) return;
    munmap(b, ((clo_len_t *)(b + sizeof(clo_meta_t)))->len);
}
ffi_status ffi_prep_closure_loc(void *closure, ffi_cif *cif, void (*fun)(ffi_cif *, void *, void **, void *), void *ud, void *codeloc) {
    if (!closure || !cif || !fun) return FFI_BAD_TYPEDEF;
    clo_meta_t *m = (clo_meta_t *)((uint8_t *)closure - HDR);
    if (m->magic != CLO_MAGIC) return FFI_BAD_TYPEDEF;      /* só closures do ffi_closure_alloc */
    uint8_t *code = codeloc ? codeloc : closure;
    pthread_jit_write_protect_np(0);
    m->cif = cif; m->fun = fun; m->ud = ud;
    write_tramp(code, m);
    pthread_jit_write_protect_np(1);
    sys_icache_invalidate(code, TRAMP_SIZE);
    return FFI_OK;
}
ffi_status ffi_prep_closure(void *closure, ffi_cif *cif, void (*fun)(ffi_cif *, void *, void **, void *), void *ud) {
    return ffi_prep_closure_loc(closure, cif, fun, ud, closure);
}
/* closures em tabela (FFI_EXEC_TRAMPOLINE_TABLE do Darwin): o código é o próprio bloco */
void *ffi_find_closure_for_code_np(void *code) { return code; }
size_t ffi_tramp_is_supported(void) { return 0; }
/* chamadas "raw"/java: o mesmo que ffi_call com os argumentos já em ponteiros */
void ffi_raw_call(ffi_cif *cif, void (*fn)(void), void *rvalue, void *raw) {
    void *av[MAXA];
    uint8_t *p = raw;
    for (unsigned i = 0; i < cif->nargs && i < MAXA; i++) { av[i] = p; p += align_up(cif->arg_types[i]->size, 8); }
    ffi_call(cif, fn, rvalue, av);
}
size_t ffi_raw_size(ffi_cif *cif) {
    size_t s = 0;
    for (unsigned i = 0; i < cif->nargs; i++) s += align_up(cif->arg_types[i]->size, 8);
    return s;
}
