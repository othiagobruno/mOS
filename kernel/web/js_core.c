/* mOS — motor JavaScript "Brisa": núcleo (coletor, strings, objetos, propriedades, conversões, chamadas) */
#include "js_int.h"

jsstate J;
jsstr *A[A_COUNT];
bool js_exc_pending;

/* ============================================================ coletor de lixo (mark & sweep) */
static gch_t **gc_objs;
static u32 gc_n, gc_cap;
static usize gc_bytes, gc_since;
static usize gc_threshold = 6u << 20;
static gch_t **gc_work;
static u32 gc_wn, gc_wcap;
static jsstr *small_ints[256];
static jsstr *char_strs[128];

usize gc_heap_bytes(void) { return gc_bytes; }

void *gc_alloc(int gtype, usize size) {
    if (!J.gc_running && gc_since > gc_threshold && J.entry > 0) gc_collect();
    gch_t *h = kcalloc(1, size);
    if (!h) {
        if (J.entry > 0 && !J.gc_running) gc_collect();
        h = kcalloc(1, size);
        if (!h) panic("JS: memória esgotada (%u bytes)", (u32)size);
    }
    h->gtype = (u8)gtype;
    h->bsize = (u32)size;
    if (gc_n == gc_cap) {
        u32 nc = gc_cap ? gc_cap * 2 : 4096;
        gch_t **na = krealloc(gc_objs, sizeof(gch_t *) * nc);
        if (!na) panic("JS: memória esgotada (tabela do coletor)");
        gc_objs = na;
        gc_cap = nc;
    }
    gc_objs[gc_n++] = h;
    gc_bytes += size;
    gc_since += size;
    return h;
}

void gc_mark_ptr(void *p) {
    gch_t *h = p;
    if (!h || h->mark) return;
    h->mark = 1;
    if (h->gtype == GT_STR || h->gtype == GT_BIG) return;
    if (gc_wn == gc_wcap) {
        u32 nc = gc_wcap ? gc_wcap * 2 : 1024;
        gch_t **na = krealloc(gc_work, sizeof(gch_t *) * nc);
        if (!na) return;   /* sem memória: marcação incompleta seria perigosa, mas não há alternativa */
        gc_work = na;
        gc_wcap = nc;
    }
    gc_work[gc_wn++] = h;
}

void gc_mark_val(jsv v) {
    if (jv_isstr(v) || jv_isobj(v) || jv_isbig(v)) gc_mark_ptr((void *)(uptr)(v & JS_PTRMASK));
}

static void mark_obj(jsobj *o) {
    gc_mark_ptr(o->proto);
    for (u32 i = 0; i < o->nprops; i++) {
        gc_mark_ptr(o->props[i].k);
        gc_mark_val(o->props[i].v);
        gc_mark_val(o->props[i].set);
    }
    for (u32 i = 0; i < o->alen; i++) gc_mark_val(o->arr[i]);
    switch (o->cls) {
    case CL_FUNCTION:
        gc_mark_ptr(o->u.f.env); gc_mark_ptr(o->u.f.script); gc_mark_ptr(o->u.f.home); gc_mark_ptr(o->u.f.cenv);
        break;
    case CL_NATIVE: gc_mark_val(o->u.nat.data); gc_mark_val(o->u.nat.data2); break;
    case CL_BOUND: gc_mark_val(o->u.bound.target); gc_mark_val(o->u.bound.bthis); gc_mark_ptr(o->u.bound.args); break;
    case CL_REGEXP: gc_mark_ptr(o->u.re.src); gc_mark_ptr(o->u.re.flags); break;
    case CL_PROMISE: gc_mark_val(o->u.prom.value); gc_mark_ptr(o->u.prom.reactions); break;
    case CL_MAP: case CL_SET: gc_mark_ptr(o->u.map.keys); gc_mark_ptr(o->u.map.vals); break;
    case CL_ITER: gc_mark_val(o->u.it.target); break;
    case CL_GEN: js_coro_mark_owner(o); break;
    case CL_BOOLEAN: case CL_NUMBER: case CL_STRING: case CL_SYMBOL: case CL_BIGINT: gc_mark_val(o->u.prim); break;
    case CL_MODULE: js_module_mark(o); break;
    case CL_NODE: case CL_STYLE: case CL_CLASSLIST: case CL_EVENT: case CL_STORAGE: case CL_LOCATION: case CL_DATASET: case CL_HOST:
        gc_mark_val(o->u.host.extra);
        break;
    default: break;
    }
}

static void mark_env(jsenv *e) {
    gc_mark_ptr(e->parent);
    for (u32 i = 0; i < e->n; i++) { gc_mark_ptr(e->names[i]); gc_mark_val(e->vals[i]); }
    gc_mark_val(e->thisv);
    gc_mark_ptr(e->fnobj);
    gc_mark_val(e->newtarget);
}

static void mark_script(jsscript *s) {
    for (int i = 0; i < s->nstrs; i++) gc_mark_ptr(s->strs[i]);
    gc_mark_ptr(s->name);
}

static void gc_drain(void) {
    while (gc_wn) {
        gch_t *h = gc_work[--gc_wn];
        switch (h->gtype) {
        case GT_OBJ: mark_obj((jsobj *)h); break;
        case GT_ENV: mark_env((jsenv *)h); break;
        case GT_SCRIPT: mark_script((jsscript *)h); break;
        default: break;
        }
    }
}

/* ordenação dos objetos por endereço (para a varredura conservadora) */
static void sort_ptrs(gch_t **a, int n) {
    while (n > 16) {
        gch_t *pv = a[n / 2];
        int i = 0, j = n - 1;
        while (i <= j) {
            while (a[i] < pv) i++;
            while (a[j] > pv) j--;
            if (i <= j) { gch_t *t = a[i]; a[i] = a[j]; a[j] = t; i++; j--; }
        }
        if (j + 1 < n - i) { sort_ptrs(a, j + 1); a += i; n -= i; }
        else { sort_ptrs(a + i, n - i); n = j + 1; }
    }
    for (int i = 1; i < n; i++) {
        gch_t *x = a[i];
        int j = i - 1;
        while (j >= 0 && a[j] > x) { a[j + 1] = a[j]; j--; }
        a[j + 1] = x;
    }
}

static void consider(uptr w) {
    if (w < 0x1000) return;
    if (!gc_n || w < (uptr)gc_objs[0] || w >= (uptr)gc_objs[gc_n - 1] + gc_objs[gc_n - 1]->bsize) return;
    int lo = 0, hi = (int)gc_n - 1;
    while (lo < hi) {
        int mid = (lo + hi + 1) / 2;
        if ((uptr)gc_objs[mid] <= w) lo = mid; else hi = mid - 1;
    }
    gch_t *h = gc_objs[lo];
    if (w >= (uptr)h && w < (uptr)h + h->bsize) gc_mark_ptr(h);
}

static void scan_range(uptr lo, uptr hi) {
    for (uptr p = lo & ~(uptr)7; p < hi; p += 8) {
        uptr w = *(volatile uptr *)p;
        consider(w);
        if ((w & JS_TAG_MASK) == JS_TAG_STR || (w & JS_TAG_MASK) == JS_TAG_OBJ || (w & JS_TAG_MASK) == JS_TAG_BIG) consider(w & JS_PTRMASK);
    }
}

static void __attribute__((noinline)) scan_stack(void) {
    u64 regs[20];
    __asm__ volatile(
        "movq %%rbx, 0(%0)\n\tmovq %%rbp, 8(%0)\n\tmovq %%rdi, 16(%0)\n\tmovq %%rsi, 24(%0)\n\t"
        "movq %%r12, 32(%0)\n\tmovq %%r13, 40(%0)\n\tmovq %%r14, 48(%0)\n\tmovq %%r15, 56(%0)\n\t"
        "movq %%xmm6, 64(%0)\n\tmovq %%xmm7, 72(%0)\n\tmovq %%xmm8, 80(%0)\n\tmovq %%xmm9, 88(%0)\n\t"
        "movq %%xmm10, 96(%0)\n\tmovq %%xmm11, 104(%0)\n\tmovq %%xmm12, 112(%0)\n\tmovq %%xmm13, 120(%0)\n\t"
        "movq %%xmm14, 128(%0)\n\tmovq %%xmm15, 136(%0)\n\t"
        :: "r"(regs) : "memory");
    uptr sp;
    __asm__ volatile("movq %%rsp, %0" : "=r"(sp));
    for (int i = 0; i < 18; i++) { consider((uptr)regs[i]); consider((uptr)(regs[i] & JS_PTRMASK)); }
    if (J.stack_base && J.stack_base >= sp) scan_range(sp, J.stack_base);
    js_coro_scan_chain(scan_range);
}

/* tabela de strings internadas (fraca: entradas mortas são removidas na varredura) */
static jsstr **itab;
static u32 icap_, icount;
#define ITOMB ((jsstr *)(uptr)1)

static void intern_remove(jsstr *s) {
    if (!itab) return;
    u32 m = icap_ - 1;
    for (u32 i = s->hash & m, k = 0; k < icap_; i = (i + 1) & m, k++) {
        if (!itab[i]) return;
        if (itab[i] == s) { itab[i] = ITOMB; icount--; return; }
    }
}

static void free_obj(gch_t *h) {
    switch (h->gtype) {
    case GT_OBJ: {
        jsobj *o = (jsobj *)h;
        kfree(o->props);
        kfree(o->index);
        kfree(o->arr);
        if (o->cls == CL_REGEXP && o->u.re.prog) rx_unref(o->u.re.prog);
        if (o->cls == CL_GEN) js_coro_free(o->u.gen.co);
        if (o->cls == CL_MODULE) js_module_free(o);
        break;
    }
    case GT_ENV: {
        jsenv *e = (jsenv *)h;
        kfree(e->names); kfree(e->vals); kfree(e->vflags);
        break;
    }
    case GT_SCRIPT: {
        jsscript *s = (jsscript *)h;
        kfree(s->mreq); kfree(s->imports); kfree(s->exports);
        for (int i = 0; i < s->nrx; i++) rx_unref(s->rx[i]);
        kfree(s->rx);
        kfree(s->strs);
        arena_free(&s->arena);
        break;
    }
    case GT_STR:
        if (((jsstr *)h)->intern) intern_remove((jsstr *)h);
        break;
    }
    gc_bytes -= h->bsize;
    kfree(h);
}

void js_lib_mark(void);

void gc_collect(void) {
    if (J.gc_running) return;
    J.gc_running = true;
    u64 t0 = timer_ticks();
    sort_ptrs(gc_objs, (int)gc_n);
    /* raízes */
    for (int i = 0; i < A_COUNT; i++) gc_mark_ptr(A[i]);
    for (int i = 0; i < 256; i++) gc_mark_ptr(small_ints[i]);
    for (int i = 0; i < 128; i++) gc_mark_ptr(char_strs[i]);
    for (jsrealm *r = J.realms; r; r = r->next) js_mark_realm(r);
    for (int i = 0; i < J.nroots; i++) gc_mark_val(J.roots[i]);
    gc_mark_val(J.exc);
    gc_mark_val(J.retval);
    gc_mark_val(J.completion);
    gc_mark_val(J.newtarget);
    gc_mark_ptr(J.realm ? J.realm->p_gen : NULL);
    gc_mark_ptr(J.label);
    gc_mark_ptr(J.srcname);
    js_mark_pages();
    js_lib_mark();
    js_coro_gc_begin();
    if (J.entry > 0) scan_stack();
    gc_drain();
    /* pilhas de geradores suspensos: só as dos donos já alcançados (repete até estabilizar) */
    while (js_coro_scan(scan_range)) gc_drain();
    /* varredura */
    u32 k = 0;
    usize before = gc_bytes;
    for (u32 i = 0; i < gc_n; i++) {
        gch_t *h = gc_objs[i];
        if (h->mark) { h->mark = 0; gc_objs[k++] = h; }
        else free_obj(h);
    }
    gc_n = k;
    gc_since = 0;
    gc_threshold = MAX((usize)6 << 20, gc_bytes);
    J.gc_count++;
    J.gc_running = false;
    u64 dt = timer_ticks() - t0;
    if (dt > 30) kprintf("JS: coleta %u em %u ms (%u KiB -> %u KiB, %u objetos)\n", J.gc_count, (u32)dt,
                         (u32)(before / 1024), (u32)(gc_bytes / 1024), gc_n);
}

void gc_maybe(void) { if (gc_since > gc_threshold / 2) gc_collect(); }

usize js_heap_bytes(void) { return gc_bytes; }

/* ============================================================ strings */
static u32 hash_bytes(const char *s, int n) {
    u32 h = 2166136261u;
    for (int i = 0; i < n; i++) { h ^= (u8)s[i]; h *= 16777619u; }
    return h ? h : 1;
}

static jsstr *str_alloc(int len) {
    jsstr *s = gc_alloc(GT_STR, sizeof(jsstr) + (usize)len + 1);
    s->len = (u32)len;
    return s;
}

static void str_finish(jsstr *s) {
    u32 ul = 0;
    bool ascii = true;
    for (u32 i = 0; i < s->len; i++) {
        u8 b = (u8)s->d[i];
        if (b >= 0x80) ascii = false;
        if ((b & 0xC0) != 0x80) { ul++; if (b >= 0xF0) ul++; }
    }
    s->ulen = ul;
    s->ascii = ascii;
    s->hash = hash_bytes(s->d, (int)s->len);
}

jsstr *js_strn(const char *p, int len) {
    if (len < 0) len = (int)strlen(p);
    if (len == 1 && (u8)p[0] < 128 && A[A_empty]) {
        /* strings de 1 caractere ASCII são compartilhadas */
        u8 c = (u8)p[0];
        if (!char_strs[c]) char_strs[c] = js_intern(p, 1);
        return char_strs[c];
    }
    if (len == 0 && A[A_empty]) return A[A_empty];
    jsstr *s = str_alloc(len);
    memcpy(s->d, p, (usize)len);
    s->d[len] = 0;
    str_finish(s);
    return s;
}

jsstr *js_strz(const char *s) { return js_strn(s, (int)strlen(s)); }

static void itab_grow(void) {
    u32 nc = icap_ ? icap_ * 2 : 4096;
    jsstr **nt = kcalloc(nc, sizeof(jsstr *));
    if (!nt) return;
    for (u32 i = 0; i < icap_; i++) {
        jsstr *s = itab[i];
        if (!s || s == ITOMB) continue;
        u32 j = s->hash & (nc - 1);
        while (nt[j]) j = (j + 1) & (nc - 1);
        nt[j] = s;
    }
    kfree(itab);
    itab = nt;
    icap_ = nc;
}

jsstr *js_intern(const char *p, int len) {
    if (len < 0) len = (int)strlen(p);
    u32 h = hash_bytes(p, len);
    if (itab) {
        u32 m = icap_ - 1;
        for (u32 i = h & m, k = 0; k < icap_; i = (i + 1) & m, k++) {
            jsstr *s = itab[i];
            if (!s) break;
            if (s != ITOMB && s->hash == h && s->len == (u32)len && !memcmp(s->d, p, (usize)len)) return s;
        }
    }
    if ((icount + 1) * 2 >= icap_) itab_grow();
    jsstr *s = str_alloc(len);
    memcpy(s->d, p, (usize)len);
    s->d[len] = 0;
    str_finish(s);
    s->intern = 1;
    u32 m = icap_ - 1;
    u32 i = h & m;
    while (itab[i] && itab[i] != ITOMB) i = (i + 1) & m;
    itab[i] = s;
    icount++;
    return s;
}

jsstr *js_new_symbol(jsstr *desc) {
    int n = desc ? (int)desc->len : 0;
    jsstr *s = str_alloc(n);
    if (n) memcpy(s->d, desc->d, (usize)n);
    s->d[n] = 0;
    str_finish(s);
    s->sym = 1;
    return s;
}

jsstr *js_str_cat(jsstr *a, jsstr *b) {
    if (!a->len) return b;
    if (!b->len) return a;
    usize n = (usize)a->len + b->len;
    if (n > (64u << 20)) { js_throw_err(E_RANGE, "String grande demais"); return NULL; }
    jsstr *s = str_alloc((int)n);
    memcpy(s->d, a->d, a->len);
    memcpy(s->d + a->len, b->d, b->len);
    s->d[n] = 0;
    s->ulen = a->ulen + b->ulen;
    s->ascii = a->ascii && b->ascii;
    s->hash = hash_bytes(s->d, (int)n);
    return s;
}

jsstr *js_str_fmt(const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return js_strz(buf);
}

bool js_str_eq(jsstr *a, jsstr *b) {
    if (a == b) return true;
    if (a->sym || b->sym) return false;
    if (a->intern && b->intern) return false;
    return a->hash == b->hash && a->len == b->len && !memcmp(a->d, b->d, a->len);
}

int js_utf8_put(char *o, u32 cp) {
    if (cp < 0x80) { o[0] = (char)cp; return 1; }
    if (cp < 0x800) { o[0] = (char)(0xC0 | cp >> 6); o[1] = (char)(0x80 | (cp & 63)); return 2; }
    if (cp < 0x10000) { o[0] = (char)(0xE0 | cp >> 12); o[1] = (char)(0x80 | ((cp >> 6) & 63)); o[2] = (char)(0x80 | (cp & 63)); return 3; }
    o[0] = (char)(0xF0 | cp >> 18); o[1] = (char)(0x80 | ((cp >> 12) & 63)); o[2] = (char)(0x80 | ((cp >> 6) & 63)); o[3] = (char)(0x80 | (cp & 63));
    return 4;
}

u32 js_utf8_get(const char *s, int len, int *i) {
    u8 b = (u8)s[*i];
    if (b < 0x80) { (*i)++; return b; }
    int n = b >= 0xF0 ? 3 : b >= 0xE0 ? 2 : b >= 0xC0 ? 1 : 0;
    u32 cp = n == 3 ? (b & 7u) : n == 2 ? (b & 15u) : (b & 31u);
    if (!n) { (*i)++; return 0xFFFD; }
    int k = *i + 1;
    for (int j = 0; j < n; j++, k++) {
        if (k >= len || ((u8)s[k] & 0xC0) != 0x80) { *i = k; return 0xFFFD; }
        cp = cp << 6 | ((u8)s[k] & 63u);
    }
    *i = k;
    return cp;
}

/* compara por unidades UTF-16 (code points fora do BMP comparados como pares) */
int js_str_cmp(jsstr *a, jsstr *b) {
    if (a->ascii && b->ascii) {
        u32 n = MIN(a->len, b->len);
        int c = memcmp(a->d, b->d, n);
        if (c) return c < 0 ? -1 : 1;
        return a->len < b->len ? -1 : a->len > b->len ? 1 : 0;
    }
    int i = 0, j = 0;
    while (i < (int)a->len && j < (int)b->len) {
        u32 x = js_utf8_get(a->d, (int)a->len, &i), y = js_utf8_get(b->d, (int)b->len, &j);
        if (x >= 0x10000) x = 0xD800 + ((x - 0x10000) >> 10);
        if (y >= 0x10000) y = 0xD800 + ((y - 0x10000) >> 10);
        if (x != y) return x < y ? -1 : 1;
    }
    if (i < (int)a->len) return 1;
    if (j < (int)b->len) return -1;
    return 0;
}

u32 js_str_unit(jsstr *s, int ui) {
    if (ui < 0 || ui >= (int)s->ulen) return 0xFFFFFFFF;
    if (s->ascii) return (u8)s->d[ui];
    int i = 0, u = 0;
    while (i < (int)s->len) {
        u32 cp = js_utf8_get(s->d, (int)s->len, &i);
        if (cp >= 0x10000) {
            if (u == ui) return 0xD800 + ((cp - 0x10000) >> 10);
            if (u + 1 == ui) return 0xDC00 + ((cp - 0x10000) & 0x3FF);
            u += 2;
        } else {
            if (u == ui) return cp;
            u++;
        }
    }
    return 0xFFFFFFFF;
}

int js_str_boff(jsstr *s, int ui) {
    if (ui <= 0) return 0;
    if (ui >= (int)s->ulen) return (int)s->len;
    if (s->ascii) return ui;
    int i = 0, u = 0;
    while (i < (int)s->len && u < ui) {
        u8 b = (u8)s->d[i];
        int prev = i;
        u32 cp = js_utf8_get(s->d, (int)s->len, &i);
        (void)b;
        u += cp >= 0x10000 ? 2 : 1;
        if (u > ui) return prev; /* no meio de um par: arredonda para baixo */
    }
    return i;
}

int js_str_uidx(jsstr *s, int boff) {
    if (s->ascii) return boff;
    int i = 0, u = 0;
    while (i < boff && i < (int)s->len) {
        u32 cp = js_utf8_get(s->d, (int)s->len, &i);
        u += cp >= 0x10000 ? 2 : 1;
    }
    return u;
}

jsstr *js_str_usub(jsstr *s, int ua, int ub) {
    if (ua < 0) ua = 0;
    if (ub > (int)s->ulen) ub = (int)s->ulen;
    if (ua >= ub) return A[A_empty];
    if (ua == 0 && ub == (int)s->ulen) return s;
    if (s->ascii) return js_strn(s->d + ua, ub - ua);
    /* trata limites no meio de pares substitutos convertendo por unidades */
    u16 buf[256];
    int n = ub - ua;
    u16 *u = n <= 256 ? buf : kmalloc(sizeof(u16) * (usize)n);
    if (!u) return A[A_empty];
    for (int i = 0; i < n; i++) u[i] = (u16)js_str_unit(s, ua + i);
    jsstr *r = js_str_from_units(u, n);
    if (u != buf) kfree(u);
    return r;
}

jsstr *js_str_from_units(const u16 *u, int n) {
    char tmp[512];
    char *o = n * 3 + 1 <= (int)sizeof tmp ? tmp : kmalloc((usize)n * 3 + 1);
    if (!o) return A[A_empty];
    int k = 0;
    for (int i = 0; i < n; i++) {
        u32 c = u[i];
        if (c >= 0xD800 && c < 0xDC00 && i + 1 < n && u[i + 1] >= 0xDC00 && u[i + 1] < 0xE000) {
            c = 0x10000 + ((c - 0xD800) << 10) + (u[i + 1] - 0xDC00);
            i++;
        }
        k += js_utf8_put(o + k, c);
    }
    jsstr *r = js_strn(o, k);
    if (o != tmp) kfree(o);
    return r;
}

bool js_str_isindex(jsstr *s, u32 *idx) {
    if (!s->len || s->len > 10 || s->sym) return false;
    if (s->d[0] == '0' && s->len > 1) return false;
    u64 v = 0;
    for (u32 i = 0; i < s->len; i++) {
        if (s->d[i] < '0' || s->d[i] > '9') return false;
        v = v * 10 + (u64)(s->d[i] - '0');
    }
    if (v >= 0xFFFFFFFFull) return false;
    *idx = (u32)v;
    return true;
}

static jsstr *idx_str(u32 i) {
    if (i < 256) {
        if (!small_ints[i]) { char b[8]; snprintf(b, sizeof b, "%u", i); small_ints[i] = js_intern(b, -1); }
        return small_ints[i];
    }
    char b[16];
    snprintf(b, sizeof b, "%u", i);
    return js_strz(b);
}

/* ============================================================ objetos */
jsobj *js_newobj_cls(int cls, jsobj *proto) {
    jsobj *o = gc_alloc(GT_OBJ, sizeof(jsobj));
    o->cls = (u8)cls;
    o->proto = proto;
    return o;
}

jsobj *js_newobj(jsobj *proto) { return js_newobj_cls(CL_OBJECT, proto); }

void js_arr_reserve(jsobj *a, u32 n) {
    if (n <= a->acap) return;
    u32 nc = a->acap ? a->acap : 4;
    while (nc < n) nc *= 2;
    jsv *na = krealloc(a->arr, sizeof(jsv) * nc);
    if (!na) panic("JS: memória esgotada (vetor)");
    a->arr = na;
    a->acap = nc;
}

jsobj *js_newarr(u32 n) {
    jsobj *a = js_newobj_cls(CL_ARRAY, J.realm ? J.realm->p_array : NULL);
    if (n) {
        js_arr_reserve(a, n);
        for (u32 i = 0; i < n; i++) a->arr[i] = JS_UNDEF;
        a->alen = n;
    }
    return a;
}

jsobj *js_arr_from(jsv *v, int n) {
    jsobj *a = js_newarr(0);
    if (n > 0) {
        js_arr_reserve(a, (u32)n);
        memcpy(a->arr, v, sizeof(jsv) * (usize)n);
        a->alen = (u32)n;
    }
    return a;
}

void js_arr_push(jsobj *a, jsv v) {
    js_arr_reserve(a, a->alen + 1);
    a->arr[a->alen++] = v;
}

jsv js_arr_get(jsobj *a, u32 i) {
    if (i < a->alen && a->arr[i] != JS_HOLE) return a->arr[i];
    return JS_UNDEF;
}

void js_arr_set(jsobj *a, u32 i, jsv v) {
    if (i >= a->alen) {
        js_arr_reserve(a, i + 1);
        for (u32 k = a->alen; k < i; k++) a->arr[k] = JS_HOLE;
        a->alen = i + 1;
    }
    a->arr[i] = v;
}

void js_arr_setlen(jsobj *a, u32 n) {
    if (n < a->alen) { a->alen = n; return; }
    js_arr_reserve(a, n);
    for (u32 k = a->alen; k < n; k++) a->arr[k] = JS_HOLE;
    a->alen = n;
}

bool js_isarray(jsv v) { return jv_iscls(v, CL_ARRAY); }

static void index_rebuild(jsobj *o) {
    if (o->nprops <= 8) { kfree(o->index); o->index = NULL; o->icap = 0; return; }
    u32 nc = 16;
    while (nc < o->nprops * 2) nc *= 2;
    if (nc != o->icap) {
        kfree(o->index);
        o->index = kmalloc(sizeof(u32) * nc);
        o->icap = nc;
    }
    if (!o->index) { o->icap = 0; return; }
    memset(o->index, 0, sizeof(u32) * nc);
    for (u32 i = 0; i < o->nprops; i++) {
        u32 j = o->props[i].k->hash & (nc - 1);
        while (o->index[j]) j = (j + 1) & (nc - 1);
        o->index[j] = i + 1;
    }
}

jsprop *js_ownprop(jsobj *o, jsstr *k) {
    if (o->index) {
        u32 m = o->icap - 1;
        for (u32 j = k->hash & m;; j = (j + 1) & m) {
            u32 x = o->index[j];
            if (!x) return NULL;
            jsprop *p = &o->props[x - 1];
            if (p->k == k || (p->k->hash == k->hash && js_str_eq(p->k, k))) return p;
        }
    }
    for (u32 i = 0; i < o->nprops; i++) {
        jsstr *pk = o->props[i].k;
        if (pk == k || (pk->hash == k->hash && js_str_eq(pk, k))) return &o->props[i];
    }
    return NULL;
}

jsprop *js_findprop(jsobj *o, jsstr *k, jsobj **owner) {
    for (; o; o = o->proto) {
        jsprop *p = js_ownprop(o, k);
        if (p) { if (owner) *owner = o; return p; }
    }
    return NULL;
}

jsprop *js_addprop(jsobj *o, jsstr *k) {
    if (o->nprops == o->cprops) {
        u32 nc = o->cprops ? o->cprops * 2 : 4;
        jsprop *np = krealloc(o->props, sizeof(jsprop) * nc);
        if (!np) panic("JS: memória esgotada (propriedades)");
        o->props = np;
        o->cprops = nc;
    }
    jsprop *p = &o->props[o->nprops++];
    p->k = k;
    p->v = JS_UNDEF;
    p->set = JS_UNDEF;
    p->flags = 0;
    if (o->nprops > 8) {
        if (!o->index || o->nprops * 2 > o->icap) index_rebuild(o);
        else {
            u32 m = o->icap - 1, j = k->hash & m;
            while (o->index[j]) j = (j + 1) & m;
            o->index[j] = o->nprops;
        }
    }
    return p;
}

void js_defprop(jsobj *o, jsstr *k, jsv v, u32 flags) {
    jsprop *p = js_ownprop(o, k);
    if (!p) p = js_addprop(o, k);
    p->v = v;
    p->set = JS_UNDEF;
    p->flags = flags;
}

void js_defn(jsobj *o, const char *name, jsv v, u32 flags) { js_defprop(o, js_intern(name, -1), v, flags); }

void js_defaccessor(jsobj *o, jsstr *k, jsv getter, jsv setter, u32 flags) {
    jsprop *p = js_ownprop(o, k);
    if (!p) { p = js_addprop(o, k); p->v = JS_UNDEF; p->set = JS_UNDEF; }
    else if (!(p->flags & PF_ACCESSOR)) { p->v = JS_UNDEF; p->set = JS_UNDEF; }
    if (getter != JS_HOLE) p->v = getter;
    if (setter != JS_HOLE) p->set = setter;
    p->flags = flags | PF_ACCESSOR;
}

bool js_delprop(jsobj *o, jsstr *k) {
    jsprop *p = js_ownprop(o, k);
    if (!p) return true;
    if (p->flags & PF_NOCONF) return false;
    u32 i = (u32)(p - o->props);
    memmove(&o->props[i], &o->props[i + 1], sizeof(jsprop) * (o->nprops - i - 1));
    o->nprops--;
    index_rebuild(o);
    return true;
}

jsobj *js_newnative(jsnative fn, const char *name, int nargs, int magic) {
    jsobj *f = js_newobj_cls(CL_NATIVE, J.realm ? J.realm->p_function : NULL);
    f->u.nat.fn = fn;
    f->u.nat.magic = magic;
    f->u.nat.data = JS_UNDEF;
    f->u.nat.data2 = JS_UNDEF;
    js_defprop(f, A[A_length], jv_int(nargs), PF_NOENUM | PF_RO);
    js_defprop(f, A[A_name], jv_str(js_intern(name, -1)), PF_NOENUM | PF_RO);
    return f;
}

jsobj *js_defnative(jsobj *o, const char *name, jsnative fn, int nargs, int magic) {
    jsobj *f = js_newnative(fn, name, nargs, magic);
    js_defn(o, name, jv_obj(f), PF_NOENUM);
    return f;
}

jsobj *js_defgetter(jsobj *o, const char *name, jsnative fn, int magic) {
    jsobj *f = js_newnative(fn, name, 0, magic);
    js_defaccessor(o, js_intern(name, -1), jv_obj(f), JS_UNDEF, PF_NOENUM);
    return f;
}

jsobj *js_newctor(jsnative fn, const char *name, int nargs, jsobj *proto) {
    jsobj *f = js_newnative(fn, name, nargs, 0);
    f->is_class = 1;   /* construtor nativo */
    if (proto) {
        js_defprop(f, A[A_prototype], jv_obj(proto), PF_NOENUM | PF_RO | PF_NOCONF);
        js_defprop(proto, A[A_constructor], jv_obj(f), PF_NOENUM);
    }
    return f;
}

/* ============================================================ acesso a propriedades */
static jsobj *proto_of_prim(jsv o) {
    jsrealm *r = J.realm;
    if (jv_isstr(o)) return jv_s(o)->sym ? r->p_symbol : r->p_string;
    if (jv_isnum(o)) return r->p_number;
    if (jv_isbool(o)) return r->p_boolean;
    if (jv_isbig(o)) return r->p_bigint;
    return NULL;
}

jsobj *js_getproto(jsv v) {
    if (jv_isobj(v)) return jv_o(v)->proto;
    return proto_of_prim(v);
}

static bool is_host(int cls) { return cls >= CL_NODE && cls <= CL_HOST; }

static jsv str_char(jsstr *s, u32 i) {
    if (s->ascii) return jv_str(js_strn(s->d + i, 1));
    return jv_str(js_str_usub(s, (int)i, (int)i + 1));
}

jsv js_get_with_this(jsv o, jsstr *k, jsv thisv) {
    jsobj *ob;
    u32 idx;
    if (jv_isobj(o)) ob = jv_o(o);
    else if (jv_isstr(o)) {
        jsstr *s = jv_s(o);
        if (!s->sym) {
            if (k == A[A_length]) return jv_int((int)s->ulen);
            if (js_str_isindex(k, &idx)) return idx < s->ulen ? str_char(s, idx) : JS_UNDEF;
        }
        ob = proto_of_prim(o);
    } else if (jv_isnum(o) || jv_isbool(o) || jv_isbig(o)) ob = proto_of_prim(o);
    else {
        char kb[80];
        return js_throw_err(E_TYPE, "Não é possível ler propriedades de %s (lendo '%s')", o == JS_NULL ? "null" : "undefined",
                            js_cstr(jv_str(k), kb, sizeof kb));
    }
    for (int depth = 0; ob && depth < 10000; ob = ob->proto, depth++) {
        switch (ob->cls) {
        case CL_ARRAY: case CL_ARGUMENTS:
            if (k == A[A_length] || (k->len == 6 && js_str_eq(k, A[A_length]))) return jv_int((int)ob->alen);
            if (js_str_isindex(k, &idx)) {
                if (idx < ob->alen && ob->arr[idx] != JS_HOLE) return ob->arr[idx];
            }
            break;
        case CL_STRING: {
            jsstr *s = jv_s(ob->u.prim);
            if (js_str_eq(k, A[A_length])) return jv_int((int)s->ulen);
            if (js_str_isindex(k, &idx) && idx < s->ulen) return str_char(s, idx);
            break;
        }
        default:
            if (is_host(ob->cls)) {
                bool found = false;
                jsv r = js_dom_get(ob, k, &found);
                if (found) return r;
            }
        }
        jsprop *p = js_ownprop(ob, k);
        if (p) {
            if (p->flags & PF_ACCESSOR) {
                if (!js_iscallable(p->v)) return JS_UNDEF;
                return js_call(p->v, thisv, 0, NULL);
            }
            return p->v;
        }
    }
    return JS_UNDEF;
}

jsv js_getk(jsv o, jsstr *k) { return js_get_with_this(o, k, o); }
jsv js_getn(jsv o, const char *name) { return js_getk(o, js_intern(name, -1)); }

jsv js_getidx(jsv o, u32 i) {
    if (jv_isobj(o)) {
        jsobj *ob = jv_o(o);
        if ((ob->cls == CL_ARRAY || ob->cls == CL_ARGUMENTS) && i < ob->alen && ob->arr[i] != JS_HOLE) return ob->arr[i];
    }
    return js_getk(o, idx_str(i));
}

static bool num_index(jsv key, u32 *idx) {
    if (!jv_isnum(key)) return false;
    double d = jv_n(key);
    if (!(d >= 0 && d < 4294967295.0)) return false;
    u32 i = (u32)d;
    if ((double)i != d) return false;
    *idx = i;
    return true;
}

jsstr *js_tokey(jsv key) {
    if (jv_isstr(key)) return jv_s(key);
    u32 i;
    if (num_index(key, &i)) return idx_str(i);
    if (jv_isobj(key)) {
        jsv p = js_toprim(key, 2);
        if (p == JS_EXC) return NULL;
        if (jv_isstr(p)) return jv_s(p);
        return js_tostr(p);
    }
    return js_tostr(key);
}

jsv js_get(jsv o, jsv key) {
    u32 i;
    if (num_index(key, &i)) {
        if (jv_isobj(o)) {
            jsobj *ob = jv_o(o);
            if ((ob->cls == CL_ARRAY || ob->cls == CL_ARGUMENTS) && i < ob->alen && ob->arr[i] != JS_HOLE) return ob->arr[i];
        } else if (jv_isstr(o) && !jv_s(o)->sym) {
            jsstr *s = jv_s(o);
            return i < s->ulen ? str_char(s, i) : JS_UNDEF;
        }
    }
    jsstr *k = js_tokey(key);
    if (!k) return JS_EXC;
    return js_getk(o, k);
}

static int set_on(jsobj *ob, jsstr *k, jsv v, jsv recv) {
    u32 idx;
    switch (ob->cls) {
    case CL_ARRAY:
        if (js_str_eq(k, A[A_length])) {
            double d = js_tonum(v);
            if (js_pending()) return -1;
            u32 n = (u32)d;
            if ((double)n != d) { js_throw_err(E_RANGE, "Comprimento de vetor inválido"); return -1; }
            if (!ob->frozen) js_arr_setlen(ob, n);
            return 0;
        }
        /* fallthrough */
    case CL_ARGUMENTS:
        if (js_str_isindex(k, &idx) && idx < (1u << 26)) {
            if (ob->frozen || (ob->noext && idx >= ob->alen)) return 0;
            js_arr_set(ob, idx, v);
            return 0;
        }
        break;
    default:
        if (is_host(ob->cls)) {
            int r = js_dom_set(ob, k, v);
            if (r) return r < 0 ? -1 : 0;
        }
    }
    jsprop *p = js_ownprop(ob, k);
    if (p) {
        if (p->flags & PF_ACCESSOR) {
            if (!js_iscallable(p->set)) return 0;
            jsv a[1] = {v};
            return js_call(p->set, recv, 1, a) == JS_EXC ? -1 : 0;
        }
        if (p->flags & PF_RO || ob->frozen) return 0;
        p->v = v;
        return 0;
    }
    for (jsobj *q = ob->proto; q; q = q->proto) {
        jsprop *pp = js_ownprop(q, k);
        if (!pp) continue;
        if (pp->flags & PF_ACCESSOR) {
            if (!js_iscallable(pp->set)) return 0;
            jsv a[1] = {v};
            return js_call(pp->set, recv, 1, a) == JS_EXC ? -1 : 0;
        }
        if (pp->flags & PF_RO) return 0;
        break;
    }
    if (ob->noext) return 0;
    p = js_addprop(ob, k);
    p->v = v;
    return 0;
}

int js_setk(jsv o, jsstr *k, jsv v) {
    if (!jv_isobj(o)) {
        if (jv_isnullish(o)) {
            char kb[80];
            js_throw_err(E_TYPE, "Não é possível definir propriedades de %s (definindo '%s')", o == JS_NULL ? "null" : "undefined",
                         js_cstr(jv_str(k), kb, sizeof kb));
            return -1;
        }
        return 0;
    }
    return set_on(jv_o(o), k, v, o);
}

int js_set(jsv o, jsv key, jsv v) {
    u32 i;
    if (jv_isobj(o) && num_index(key, &i)) {
        jsobj *ob = jv_o(o);
        if ((ob->cls == CL_ARRAY || ob->cls == CL_ARGUMENTS) && !ob->frozen && i < (1u << 26) && !(ob->noext && i >= ob->alen)) {
            js_arr_set(ob, i, v);
            return 0;
        }
    }
    jsstr *k = js_tokey(key);
    if (!k) return -1;
    return js_setk(o, k, v);
}

int js_setn(jsv o, const char *name, jsv v) { return js_setk(o, js_intern(name, -1), v); }
int js_setidx(jsv o, u32 i, jsv v) { return js_set(o, jv_num(i), v); }

int js_has(jsv o, jsv key) {
    if (!jv_isobj(o)) { js_throw_err(E_TYPE, "Operador 'in' usado em um valor que não é objeto"); return -1; }
    jsstr *k = js_tokey(key);
    if (!k) return -1;
    u32 idx;
    for (jsobj *ob = jv_o(o); ob; ob = ob->proto) {
        if (ob->cls == CL_ARRAY || ob->cls == CL_ARGUMENTS) {
            if (js_str_eq(k, A[A_length])) return 1;
            if (js_str_isindex(k, &idx) && idx < ob->alen && ob->arr[idx] != JS_HOLE) return 1;
        }
        if (ob->cls == CL_STRING) {
            jsstr *s = jv_s(ob->u.prim);
            if (js_str_eq(k, A[A_length])) return 1;
            if (js_str_isindex(k, &idx) && idx < s->ulen) return 1;
        }
        if (is_host(ob->cls)) {
            bool found = false;
            js_dom_get(ob, k, &found);
            if (js_pending()) return -1;
            if (found) return 1;
        }
        if (js_ownprop(ob, k)) return 1;
    }
    return 0;
}

int js_delete(jsv o, jsv key) {
    if (!jv_isobj(o)) {
        if (jv_isnullish(o)) { js_throw_err(E_TYPE, "Não é possível apagar propriedades de %s", o == JS_NULL ? "null" : "undefined"); return -1; }
        return 1;
    }
    jsobj *ob = jv_o(o);
    jsstr *k = js_tokey(key);
    if (!k) return -1;
    if (ob->frozen) return 0;
    u32 idx;
    if ((ob->cls == CL_ARRAY || ob->cls == CL_ARGUMENTS) && js_str_isindex(k, &idx)) {
        if (idx < ob->alen) ob->arr[idx] = JS_HOLE;
        return 1;
    }
    return js_delprop(ob, k) ? 1 : 0;
}

static void push_unique(jsobj *out, jsstr *k, u32 start) {
    for (u32 i = start; i < out->alen; i++) if (js_str_eq(jv_s(out->arr[i]), k)) return;
    js_arr_push(out, jv_str(k));
}

jsobj *js_keylist(jsobj *o, bool only_enum, bool include_proto) {
    jsobj *out = js_newarr(0);
    bool first = true;
    for (jsobj *ob = o; ob; ob = ob->proto) {
        u32 start = first ? out->alen : 0;
        if (ob->cls == CL_ARRAY || ob->cls == CL_ARGUMENTS) {
            for (u32 i = 0; i < ob->alen; i++) if (ob->arr[i] != JS_HOLE) js_arr_push(out, jv_str(idx_str(i)));
        } else if (ob->cls == CL_STRING) {
            jsstr *s = jv_s(ob->u.prim);
            for (u32 i = 0; i < s->ulen; i++) js_arr_push(out, jv_str(idx_str(i)));
        }
        /* chaves inteiras primeiro, em ordem crescente */
        u32 nidx = 0, mark = out->alen;
        for (u32 i = 0; i < ob->nprops; i++) {
            jsprop *p = &ob->props[i];
            u32 idx;
            if (p->k->sym || (only_enum && (p->flags & PF_NOENUM))) continue;
            if (js_str_isindex(p->k, &idx)) {
                js_arr_push(out, jv_str(p->k));
                nidx++;
            }
        }
        if (nidx > 1) {
            for (u32 i = mark + 1; i < out->alen; i++) {
                jsv x = out->arr[i];
                u32 xi; js_str_isindex(jv_s(x), &xi);
                u32 j = i;
                while (j > mark) {
                    u32 yi; js_str_isindex(jv_s(out->arr[j - 1]), &yi);
                    if (yi <= xi) break;
                    out->arr[j] = out->arr[j - 1];
                    j--;
                }
                out->arr[j] = x;
            }
        }
        for (u32 i = 0; i < ob->nprops; i++) {
            jsprop *p = &ob->props[i];
            u32 idx;
            if (p->k->sym || (only_enum && (p->flags & PF_NOENUM))) continue;
            if (js_str_isindex(p->k, &idx)) continue;
            if (first) js_arr_push(out, jv_str(p->k));
            else push_unique(out, p->k, 0);
        }
        (void)start;
        if (!include_proto) break;
        first = false;
    }
    return out;
}

u32 js_lengthof(jsv o) {
    if (jv_isobj(o) && (jv_o(o)->cls == CL_ARRAY || jv_o(o)->cls == CL_ARGUMENTS)) return jv_o(o)->alen;
    jsv l = js_getk(o, A[A_length]);
    if (l == JS_EXC) return 0;
    double d = js_tonum(l);
    if (!(d > 0)) return 0;
    if (d > 4294967295.0) return 0xFFFFFFFF;
    return (u32)d;
}

/* ============================================================ conversões */
jsstr *js_num_tostr(double d) {
    if (d >= 0 && d < 256 && (double)(int)d == d && !(d == 0 && 1 / d < 0)) return idx_str((u32)d);
    char b[64];
    js_numfmt(d, b, sizeof b);
    return js_strz(b);
}

jsv js_toprim(jsv v, int hint) {
    if (!jv_isobj(v)) return v;
    jsobj *o = jv_o(v);
    if (hint == 0 && o->cls == CL_DATE) hint = 2;
    jsstr *order[2] = { hint == 2 ? A[A_toString] : A[A_valueOf], hint == 2 ? A[A_valueOf] : A[A_toString] };
    for (int i = 0; i < 2; i++) {
        jsv f = js_getk(v, order[i]);
        if (f == JS_EXC) return JS_EXC;
        if (js_iscallable(f)) {
            jsv r = js_call(f, v, 0, NULL);
            if (r == JS_EXC) return JS_EXC;
            if (!jv_isobj(r)) return r;
        }
    }
    return js_throw_err(E_TYPE, "Não é possível converter o objeto em valor primitivo");
}

double js_tonum(jsv v) {
    if (jv_isnum(v)) return jv_n(v);
    if (v == JS_UNDEF) return jv_n(JS_NAN_BITS);
    if (v == JS_NULL || v == JS_FALSE) return 0;
    if (v == JS_TRUE) return 1;
    if (jv_isstr(v)) {
        if (jv_s(v)->sym) { js_throw_err(E_TYPE, "Não é possível converter um Symbol em número"); return jv_n(JS_NAN_BITS); }
        return js_str_to_number(jv_s(v));
    }
    if (jv_isobj(v)) {
        jsv p = js_toprim(v, 1);
        if (p == JS_EXC) return jv_n(JS_NAN_BITS);
        return js_tonum(p);
    }
    if (jv_isbig(v)) { js_throw_err(E_TYPE, "Não é possível converter um BigInt em número"); return jv_n(JS_NAN_BITS); }
    return jv_n(JS_NAN_BITS);
}

jsv js_tonumv(jsv v) {
    if (jv_isnum(v)) return v;
    double d = js_tonum(v);
    if (js_pending()) return JS_EXC;
    return jv_num(d);
}

jsstr *js_tostr(jsv v) {
    if (jv_isstr(v)) {
        if (jv_s(v)->sym) { js_throw_err(E_TYPE, "Não é possível converter um Symbol em string"); return NULL; }
        return jv_s(v);
    }
    if (jv_isnum(v)) return js_num_tostr(jv_n(v));
    if (jv_isbig(v)) return js_big_tostr(jv_b(v), 10);
    switch (v) {
    case JS_UNDEF: return A[A_undefined];
    case JS_NULL: return A[A_null];
    case JS_TRUE: return A[A_true];
    case JS_FALSE: return A[A_false];
    }
    if (jv_isobj(v)) {
        jsv p = js_toprim(v, 2);
        if (p == JS_EXC) return NULL;
        return js_tostr(p);
    }
    return A[A_undefined];
}

jsv js_tostrv(jsv v) {
    if (jv_isstr(v) && !jv_s(v)->sym) return v;
    jsstr *s = js_tostr(v);
    return s ? jv_str(s) : JS_EXC;
}

bool js_truthy(jsv v) {
    if (jv_isnum(v)) { double d = jv_n(v); return d == d && d != 0; }
    if (jv_isstr(v)) return jv_s(v)->sym || jv_s(v)->len > 0;
    if (jv_isobj(v)) return true;
    if (jv_isbig(v)) return jv_b(v)->n != 0;
    return v == JS_TRUE;
}

jsv js_toobj(jsv v) {
    if (jv_isobj(v)) return v;
    if (jv_isnullish(v)) return js_throw_err(E_TYPE, "Não é possível converter %s em objeto", v == JS_NULL ? "null" : "undefined");
    jsrealm *r = J.realm;
    int cls = jv_isstr(v) ? (jv_s(v)->sym ? CL_SYMBOL : CL_STRING) : jv_isnum(v) ? CL_NUMBER : jv_isbig(v) ? CL_BIGINT : CL_BOOLEAN;
    jsobj *o = js_newobj_cls(cls, proto_of_prim(v));
    (void)r;
    o->u.prim = v;
    return jv_obj(o);
}

double js_tointeger(jsv v) {
    double d = js_tonum(v);
    if (d != d) return 0;
    if (!m_isfinite(d)) return d;
    return m_trunc(d);
}

u32 js_touint32(jsv v) {
    double d = jv_isnum(v) ? jv_n(v) : js_tonum(v);
    if (d != d || !m_isfinite(d)) return 0;
    if (d >= 0 && d < 4294967296.0) return (u32)(u64)d;
    if (d < 0 && d > -2147483649.0) return (u32)(i32)(i64)d;
    d = m_trunc(d);
    d = m_fmod(d, 4294967296.0);
    if (d < 0) d += 4294967296.0;
    return (u32)(u64)d;
}

i32 js_toint32(jsv v) { return (i32)js_touint32(v); }

jsstr *js_typeof(jsv v) {
    if (jv_isnum(v)) return A[A_number];
    if (jv_isstr(v)) return jv_s(v)->sym ? A[A_symbol] : A[A_string];
    if (v == JS_UNDEF) return A[A_undefined];
    if (v == JS_NULL) return A[A_object];
    if (jv_isbool(v)) return A[A_boolean];
    if (jv_isbig(v)) return A[A_bigint];
    if (jv_isobj(v)) return js_iscallable(v) ? A[A_function] : A[A_object];
    return A[A_undefined];
}

bool js_strict_eq(jsv a, jsv b) {
    if (jv_isnum(a) && jv_isnum(b)) return jv_n(a) == jv_n(b);
    if (jv_isbig(a) && jv_isbig(b)) return js_big_cmp(jv_b(a), jv_b(b)) == 0;
    if (jv_isstr(a) && jv_isstr(b)) return js_str_eq(jv_s(a), jv_s(b));
    return a == b;
}

bool js_samevalue(jsv a, jsv b, bool zero) {
    if (jv_isnum(a) && jv_isnum(b)) {
        double x = jv_n(a), y = jv_n(b);
        if (x != x) return y != y;
        if (!zero && x == 0 && y == 0) return (1 / x > 0) == (1 / y > 0);
        return x == y;
    }
    return js_strict_eq(a, b);
}

int js_loose_eq(jsv a, jsv b) {
    for (int guard = 0; guard < 8; guard++) {
        if (jv_isnum(a) && jv_isnum(b)) return jv_n(a) == jv_n(b);
        if (jv_isstr(a) && jv_isstr(b)) return js_str_eq(jv_s(a), jv_s(b));
        if (jv_isnullish(a) || jv_isnullish(b)) return jv_isnullish(a) && jv_isnullish(b);
        if (jv_isobj(a) && jv_isobj(b)) return a == b;
        if (jv_isbig(a) && jv_isbig(b)) return js_big_cmp(jv_b(a), jv_b(b)) == 0;
        if (jv_isbig(a) && jv_isnum(b)) return js_big_cmp_num(jv_b(a), jv_n(b)) == 0;
        if (jv_isnum(a) && jv_isbig(b)) return js_big_cmp_num(jv_b(b), jv_n(a)) == 0;
        if (jv_isbig(a) && jv_isstr(b) && !jv_s(b)->sym) { b = js_big_from_str_value(jv_s(b)); if (b == JS_EXC) return -1; if (b == JS_UNDEF) return 0; continue; }
        if (jv_isstr(a) && !jv_s(a)->sym && jv_isbig(b)) { a = js_big_from_str_value(jv_s(a)); if (a == JS_EXC) return -1; if (a == JS_UNDEF) return 0; continue; }
        if (jv_isbool(a)) { a = jv_int(a == JS_TRUE); continue; }
        if (jv_isbool(b)) { b = jv_int(b == JS_TRUE); continue; }
        if (jv_issym(a) || jv_issym(b)) return a == b;
        if (jv_isnum(a) && jv_isstr(b)) { b = jv_num(js_str_to_number(jv_s(b))); continue; }
        if (jv_isstr(a) && jv_isnum(b)) { a = jv_num(js_str_to_number(jv_s(a))); continue; }
        if (jv_isobj(a)) { a = js_toprim(a, 0); if (a == JS_EXC) return -1; continue; }
        if (jv_isobj(b)) { b = js_toprim(b, 0); if (b == JS_EXC) return -1; continue; }
        return 0;
    }
    return 0;
}

const char *js_cstr(jsv v, char *buf, int cap) {
    if (jv_isstr(v)) {
        jsstr *s = jv_s(v);
        int n = MIN((int)s->len, cap - 1);
        if (s->sym) { snprintf(buf, cap, "Symbol(%s)", s->d); return buf; }
        memcpy(buf, s->d, (usize)n);
        buf[n] = 0;
        return buf;
    }
    if (jv_isnum(v)) { js_numfmt(jv_n(v), buf, cap); return buf; }
    if (jv_isbig(v)) { jsstr *s = js_big_tostr(jv_b(v), 10); snprintf(buf, (usize)cap, "%sn", s->d); return buf; }
    switch (v) {
    case JS_UNDEF: return "undefined";
    case JS_NULL: return "null";
    case JS_TRUE: return "true";
    case JS_FALSE: return "false";
    }
    if (jv_isobj(v)) {
        jsobj *o = jv_o(v);
        if (o->cls == CL_FUNCTION || o->cls == CL_NATIVE || o->cls == CL_BOUND) {
            jsprop *p = js_ownprop(o, A[A_name]);
            if (p && jv_isstr(p->v) && jv_s(p->v)->len) { snprintf(buf, cap, "function %s", jv_s(p->v)->d); return buf; }
            return "function";
        }
        if (o->cls == CL_ARRAY) return "[object Array]";
        if (o->cls == CL_ERROR) {
            jsprop *p = js_ownprop(o, A[A_message]);
            if (p && jv_isstr(p->v)) { snprintf(buf, cap, "Error: %s", jv_s(p->v)->d); return buf; }
        }
        return "[object Object]";
    }
    return "?";
}

/* ============================================================ exceções */
jsv js_throw(jsv v) {
    J.exc = v;
    js_exc_pending = true;
    return JS_EXC;
}

jsv js_catch(void) {
    jsv e = J.exc;
    J.exc = JS_UNDEF;
    js_exc_pending = false;
    return e;
}

static const char *err_names[E_COUNT] = {"Error", "TypeError", "RangeError", "SyntaxError", "ReferenceError", "EvalError", "URIError"};

jsobj *js_make_err(int type, jsstr *msg) {
    jsrealm *r = J.realm;
    jsobj *e = js_newobj_cls(CL_ERROR, r ? r->p_err[type] : NULL);
    if (msg) js_defprop(e, A[A_message], jv_str(msg), PF_NOENUM);
    if (J.line > 0) js_defprop(e, A[A_lineNumber], jv_int(J.line), PF_NOENUM);
    char st[200];
    snprintf(st, sizeof st, "%s: %s\n    em %s:%d", err_names[type], msg ? msg->d : "", J.srcname ? J.srcname->d : "script", J.line);
    js_defprop(e, A[A_stack], jv_str(js_strz(st)), PF_NOENUM);
    return e;
}

jsv js_throw_err(int type, const char *fmt, ...) {
    char buf[400];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    return js_throw(jv_obj(js_make_err(type, js_strz(buf))));
}

jsstr *js_exc_string(jsv e) {
    bool saved = js_exc_pending;
    jsv sexc = J.exc;
    js_exc_pending = false;
    jsstr *r = NULL;
    if (jv_isobj(e) && jv_o(e)->cls == CL_ERROR) {
        jsv n = js_getk(e, A[A_name]);
        jsv m = n == JS_EXC ? JS_EXC : js_getk(e, A[A_message]);
        if (n != JS_EXC && m != JS_EXC) {
            jsstr *ns = jv_isstr(n) ? jv_s(n) : A[A_empty];
            jsstr *ms = jv_isstr(m) ? jv_s(m) : A[A_empty];
            r = ms->len ? js_str_fmt("%s: %s", ns->d, ms->d) : ns;
        }
    }
    if (!r && !js_exc_pending) {
        jsstr *s = jv_isobj(e) ? NULL : js_tostr(e);
        if (!s && jv_isobj(e)) {
            jsv t = js_invoke(e, A[A_toString], 0, NULL);
            if (t != JS_EXC && jv_isstr(t)) s = jv_s(t);
        }
        r = s;
    }
    if (!r) { char b[100]; r = js_strz(js_cstr(e, b, sizeof b)); }
    js_exc_pending = saved;
    J.exc = sexc;
    return r;
}

/* ============================================================ chamadas */
bool js_iscallable(jsv v) {
    if (!jv_isobj(v)) return false;
    int c = jv_o(v)->cls;
    return c == CL_FUNCTION || c == CL_NATIVE || c == CL_BOUND;
}

bool js_isctor(jsv v) {
    if (!jv_isobj(v)) return false;
    jsobj *o = jv_o(v);
    if (o->cls == CL_FUNCTION) return !o->is_arrow && !o->is_method && !(o->u.f.fi && (o->u.f.fi->is_gen || o->u.f.fi->is_async));
    if (o->cls == CL_NATIVE) return o->is_class;
    if (o->cls == CL_BOUND) return js_isctor(o->u.bound.target);
    return false;
}

static jsv call_native(jsobj *fo, jsv thisv, int argc, jsv *argv, int flags) {
    if (!js_check_stack()) return JS_EXC;
    J.calldepth++;
    jsv r = fo->u.nat.fn(thisv, argc, argv, fo, flags);
    J.calldepth--;
    return r;
}

jsv js_call(jsv f, jsv thisv, int argc, jsv *argv) {
    if (!js_iscallable(f)) {
        char b[80];
        return js_throw_err(E_TYPE, "%s não é uma função", js_cstr(f, b, sizeof b));
    }
    jsobj *fo = jv_o(f);
    switch (fo->cls) {
    case CL_FUNCTION:
        if (fo->is_class) return js_throw_err(E_TYPE, "O construtor de classe não pode ser chamado sem 'new'");
        return js_call_function(fo, thisv, argc, argv, JS_UNDEF);
    case CL_NATIVE:
        return call_native(fo, thisv, argc, argv, 0);
    case CL_BOUND: {
        jsobj *ba = fo->u.bound.args;
        int nb = ba ? (int)ba->alen : 0;
        if (!nb) return js_call(fo->u.bound.target, fo->u.bound.bthis, argc, argv);
        int n = nb + argc;
        if (n > 1024) return js_throw_err(E_RANGE, "Argumentos demais");
        jsv tmp[n];
        memcpy(tmp, ba->arr, sizeof(jsv) * (usize)nb);
        if (argc) memcpy(tmp + nb, argv, sizeof(jsv) * (usize)argc);
        return js_call(fo->u.bound.target, fo->u.bound.bthis, n, tmp);
    }
    }
    return JS_UNDEF;
}

jsv js_construct(jsv f, int argc, jsv *argv, jsv newtarget) {
    if (!js_isctor(f)) {
        char b[80];
        return js_throw_err(E_TYPE, "%s não é um construtor", js_cstr(f, b, sizeof b));
    }
    if (newtarget == JS_UNDEF) newtarget = f;
    jsobj *fo = jv_o(f);
    if (fo->cls == CL_BOUND) {
        jsobj *ba = fo->u.bound.args;
        int nb = ba ? (int)ba->alen : 0;
        int n = nb + argc;
        if (n > 1024) return js_throw_err(E_RANGE, "Argumentos demais");
        jsv tmp[n > 0 ? n : 1];
        if (nb) memcpy(tmp, ba->arr, sizeof(jsv) * (usize)nb);
        if (argc) memcpy(tmp + nb, argv, sizeof(jsv) * (usize)argc);
        if (newtarget == f) newtarget = fo->u.bound.target;
        return js_construct(fo->u.bound.target, n, tmp, newtarget);
    }
    if (fo->cls == CL_FUNCTION) return js_call_function(fo, JS_UNDEF, argc, argv, newtarget);
    /* nativo */
    jsv pr = js_getk(newtarget, A[A_prototype]);
    if (pr == JS_EXC) return JS_EXC;
    jsobj *proto = jv_isobj(pr) ? jv_o(pr) : J.realm->p_object;
    jsobj *o = js_newobj(proto);
    jsv saved = J.newtarget;
    J.newtarget = newtarget;
    jsv r = call_native(fo, jv_obj(o), argc, argv, NF_NEW);
    J.newtarget = saved;
    if (r == JS_EXC) return JS_EXC;
    return jv_isobj(r) ? r : jv_obj(o);
}

jsv js_invoke(jsv obj, jsstr *name, int argc, jsv *argv) {
    jsv f = js_getk(obj, name);
    if (f == JS_EXC) return JS_EXC;
    if (!js_iscallable(f)) {
        char b[80];
        return js_throw_err(E_TYPE, "%s não é uma função", js_cstr(jv_str(name), b, sizeof b));
    }
    return js_call(f, obj, argc, argv);
}

int js_instanceof(jsv v, jsv ctor) {
    if (!js_iscallable(ctor)) { js_throw_err(E_TYPE, "O lado direito de 'instanceof' não é chamável"); return -1; }
    jsobj *c = jv_o(ctor);
    if (c->cls == CL_BOUND) return js_instanceof(v, c->u.bound.target);
    if (!jv_isobj(v)) return 0;
    jsv p = js_getk(ctor, A[A_prototype]);
    if (p == JS_EXC) return -1;
    if (!jv_isobj(p)) { js_throw_err(E_TYPE, "O 'prototype' do construtor não é um objeto"); return -1; }
    for (jsobj *o = jv_o(v)->proto; o; o = o->proto) if (o == jv_o(p)) return 1;
    return 0;
}

/* ============================================================ entrada / orçamento / pilha */
#define JS_STACK_LIMIT (600u * 1024)
#define JS_STACK_LIMIT_OF() (J.stack_limit ? J.stack_limit : JS_STACK_LIMIT)

void js_enter(void *frame) {
    if (J.entry++ == 0) {
        J.stack_base = (uptr)frame + 1024;
        J.stack_limit = JS_STACK_LIMIT;
        J.cur_coro = NULL;
        J.calldepth = 0;
        J.interrupted = false;
    }
}

void js_leave(void) {
    if (J.entry > 0) J.entry--;
    if (J.entry == 0) { J.stack_base = 0; J.interrupted = false; }
}

bool js_check_stack(void) {
    uptr sp;
    __asm__ volatile("movq %%rsp, %0" : "=r"(sp));
    if ((J.stack_base && J.stack_base - sp > JS_STACK_LIMIT_OF()) || J.calldepth > 2000) {
        js_throw_err(E_RANGE, "Tamanho máximo da pilha de chamadas excedido");
        return false;
    }
    return true;
}

void js_set_budget(u32 ms) { J.deadline = timer_ticks() + ms; J.steps = 0; }

bool js_tick_budget(void) {
    if (J.interrupted) { if (!js_pending()) js_throw_err(E_ERROR, "Tempo de execução do script esgotado"); return false; }
    if ((++J.steps & 1023) != 0) return true;
    if (J.deadline && timer_ticks() > J.deadline) {
        J.interrupted = true;
        js_throw_err(E_ERROR, "Tempo de execução do script esgotado");
        return false;
    }
    return true;
}

/* ============================================================ reinos */
void js_mark_realm(jsrealm *r) {
    jsobj **ps[] = {&r->global, &r->p_object, &r->p_function, &r->p_array, &r->p_string, &r->p_number, &r->p_boolean,
                    &r->p_symbol, &r->p_regexp, &r->p_date, &r->p_promise, &r->p_map, &r->p_set, &r->p_iter,
                    &r->p_arguments, &r->p_error, &r->p_gen, &r->p_response, &r->p_xhr, &r->c_object, &r->c_array, &r->c_function, &r->c_promise,
                    &r->c_regexp, &r->c_symbol, &r->c_string, &r->c_number, &r->p_node, &r->p_elem, &r->p_doc,
                    &r->p_text, &r->p_event, &r->p_style, &r->p_classlist, &r->p_storage, &r->p_location, &r->jobs};
    for (u32 i = 0; i < ARRAY_LEN(ps); i++) gc_mark_ptr(*ps[i]);
    for (int i = 0; i < E_COUNT; i++) { gc_mark_ptr(r->p_err[i]); gc_mark_ptr(r->c_err[i]); }
    gc_mark_ptr(r->p_asyncgen); gc_mark_ptr(r->p_bigint); gc_mark_ptr(r->modules); gc_mark_ptr(r->modjobs);
    gc_mark_ptr(r->glex);
}

void js_core_init(void) {
    if (A[A_length]) return;
#define JS_ATOM_INIT(n) A[A_##n] = js_intern(#n, -1);
    JS_ATOMS(JS_ATOM_INIT)
    A[A_empty] = js_intern("", 0);
    A[A_neg_infinity] = js_intern("-Infinity", -1);
    A[A_sym_iterator] = js_new_symbol(js_intern("Symbol.iterator", -1));
    A[A_sym_hasInstance] = js_new_symbol(js_intern("Symbol.hasInstance", -1));
    A[A_sym_toPrimitive] = js_new_symbol(js_intern("Symbol.toPrimitive", -1));
    A[A_sym_toStringTag] = js_new_symbol(js_intern("Symbol.toStringTag", -1));
    A[A_sym_asyncIterator] = js_new_symbol(js_intern("Symbol.asyncIterator", -1));
    for (u32 i = 0; i < 256; i++) idx_str(i);
    J.exc = J.retval = J.completion = J.newtarget = JS_UNDEF;
}

jsrealm *js_realm_new(void) {
    js_core_init();
    jsrealm *r = kcalloc(1, sizeof(jsrealm));
    if (!r) return NULL;
    jsrealm *saved = J.realm;
    r->next = J.realms;
    J.realms = r;
    J.realm = r;
    js_lib_init(r);
    J.realm = saved ? saved : r;
    return r;
}

void js_realm_free(jsrealm *r) {
    for (jsrealm **pp = &J.realms; *pp; pp = &(*pp)->next) if (*pp == r) { *pp = r->next; break; }
    if (J.realm == r) J.realm = J.realms;
    kfree(r);
    if (J.entry == 0) gc_collect();
}
