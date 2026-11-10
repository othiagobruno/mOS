/* Empacota argumentos no SysV x86_64 e chama. O NSInvocation (foundation_extra.c) usa isto
 * para passar o que não cabe em 6 registradores inteiros ou 8 XMM. Sem estado global. */
#if !defined(__aarch64__)
#include "ffi_x64.h"

void *memcpy(void *, const void *, unsigned long);
void *memset(void *, int, unsigned long);

_Static_assert(__builtin_offsetof(cx_x64_frame_t, nstack) == 48, "quadro: nstack");
_Static_assert(__builtin_offsetof(cx_x64_frame_t, stack) == 56, "quadro: stack");
_Static_assert(__builtin_offsetof(cx_x64_frame_t, v) == 64, "quadro: xmm");
_Static_assert(__builtin_offsetof(cx_x64_frame_t, rax) == 192, "quadro: rax");
_Static_assert(__builtin_offsetof(cx_x64_frame_t, rdx) == 200, "quadro: rdx");
_Static_assert(__builtin_offsetof(cx_x64_frame_t, rv) == 208, "quadro: xmm0");

static const char *skipq(const char *t) {
    while (*t == 'r' || *t == 'n' || *t == 'N' || *t == 'o' || *t == 'O' || *t == 'R' || *t == 'V' || *t == 'A' || *t == 'j') t++;
    return t;
}
static const char *skip_num(const char *t) { while (*t >= '0' && *t <= '9') t++; return t; }

static const char *enc_one(const char *t, unsigned long long *sz, unsigned long long *al) {
    unsigned long long s = 8, a = 8;
    t = skipq(t);
    switch (*t) {
    case 'c': case 'C': case 'B': s = a = 1; t++; break;
    case 's': case 'S': s = a = 2; t++; break;
    case 'i': case 'I': case 'f': s = a = 4; t++; break;
    case 'l': case 'L': case 'q': case 'Q': case 'd': case '@': case '#': case ':': case '*': case '?':
        s = a = 8; t++;
        if (t[-1] == '@' && *t == '"') { t++; while (*t && *t != '"') t++; if (*t) t++; }
        if (t[-1] == '@' && *t == '?') t++;
        break;
    case '^': {
        unsigned long long is, ia;
        t = enc_one(t + 1, &is, &ia);
        s = a = 8;
        break;
    }
    case 'D': s = 16; a = 16; t++; break;
    case 'b': t = skip_num(t + 1); s = a = 4; break;
    case 'v': s = 0; a = 1; t++; break;
    case '[': {
        unsigned long long n = 0, es = 0, ea = 1;
        t++;
        while (*t >= '0' && *t <= '9') n = n * 10 + (unsigned)(*t++ - '0');
        t = enc_one(t, &es, &ea);
        if (*t == ']') t++;
        s = es * n; a = ea ? ea : 1;
        break;
    }
    case '{': case '(': {
        char close = *t == '{' ? '}' : ')';
        int uni = *t == '(';
        s = 0; a = 1; t++;
        while (*t && *t != '=' && *t != close) t++;
        if (*t == '=') t++;
        while (*t && *t != close) {
            unsigned long long fs, fa;
            const char *p;
            if (*t == '"') { t++; while (*t && *t != '"') t++; if (*t) t++; continue; }
            p = t;
            t = enc_one(t, &fs, &fa);
            if (t == p) { if (*t) t++; else break; }
            if (fa > a) a = fa;
            if (!fa) fa = 1;
            if (uni) { if (fs > s) s = fs; }
            else s = (s + fa - 1) / fa * fa + fs;
        }
        if (*t) t++;
        if (!a) a = 1;
        s = (s + a - 1) / a * a;
        break;
    }
    default: if (*t) t++; break;
    }
    t = skip_num(t);
    *sz = s;
    *al = a ? a : 1;
    return t;
}

static void stack_put(unsigned char *stack, unsigned long long *sp, const void *p, unsigned long long n, unsigned long long align) {
    unsigned long long slot;
    if (align < 8) align = 8;
    *sp = (*sp + align - 1) & ~(align - 1);
    slot = n < 8 ? 8 : (n + 7) & ~7ull;
    if (*sp + slot <= 1024 && p) {
        memset(stack + *sp, 0, slot);
        memcpy(stack + *sp, p, n < slot ? n : slot);
    }
    *sp += slot;
}

static void put_int(cx_x64_frame_t *f, int *ni, unsigned char *stack, unsigned long long *sp, unsigned long long val) {
    if (*ni < 6) f->x[(*ni)++] = val;
    else stack_put(stack, sp, &val, 8, 8);
}

static void put_fp(cx_x64_frame_t *f, int *nf, unsigned char *stack, unsigned long long *sp, const void *p, unsigned nb) {
    if (*nf < 8) {
        memset(f->v[*nf], 0, 16);
        if (p) memcpy(f->v[*nf], p, nb);
        (*nf)++;
    } else {
        unsigned char slot[8];
        memset(slot, 0, 8);
        if (p) memcpy(slot, p, nb < 8 ? nb : 8);
        stack_put(stack, sp, slot, 8, 8);
    }
}

static unsigned long long load_int(const char *t, const void *p, unsigned long long s) {
    unsigned long long v = 0;
    if (!p) return 0;
    if (*t == 'c') { signed char c; memcpy(&c, p, 1); return (unsigned long long)(long long)c; }
    if (*t == 's') { short h; memcpy(&h, p, 2); return (unsigned long long)(long long)h; }
    if (*t == 'C' || *t == 'B') { unsigned char c; memcpy(&c, p, 1); return c; }
    if (*t == 'S') { unsigned short h; memcpy(&h, p, 2); return h; }
    if (*t == 'i') { int w; memcpy(&w, p, 4); return (unsigned long long)(long long)w; }
    if (*t == 'I') { unsigned w; memcpy(&w, p, 4); return w; }
    if (s > 8) s = 8;
    memcpy(&v, p, s ? s : 8);
    return v;
}

void cx_sysv_invoke(void *fn, const char *rettype, unsigned long long retlen, const char **types,
                    const void *const *vals, unsigned nargs, void *ret) {
    cx_x64_frame_t f;
    unsigned char stk[1024] __attribute__((aligned(16)));
    unsigned long long sp = 0;
    int ni = 0, nf = 0, sret;
    const char *rt;
    unsigned i;
    if (!fn) return;
    memset(&f, 0, sizeof f);
    memset(stk, 0, sizeof stk);
    f.stack = stk;
    rt = skipq(rettype ? rettype : "v");
    sret = retlen > 16 && (*rt == '{' || *rt == '(');
    if (sret) put_int(&f, &ni, stk, &sp, (unsigned long long)(unsigned long)ret);
    for (i = 0; i < nargs; i++) {
        const char *t = skipq(types && types[i] ? types[i] : "q");
        const void *p = vals ? vals[i] : 0;
        unsigned long long s = 8, a = 8;
        enc_one(t, &s, &a);
        if (*t == 'f' || *t == 'd') { put_fp(&f, &nf, stk, &sp, p, *t == 'f' ? 4 : 8); continue; }
        if (*t == 'D') {
            unsigned char mem[16];
            memset(mem, 0, 16);
            if (p) memcpy(mem, p, 16);
            stack_put(stk, &sp, mem, 16, 16);
            continue;
        }
        if (*t == '{' || *t == '(' || *t == '[') {
            unsigned long long off, n = s > 32 ? 32 : s;
            /* se não cabe inteiro nos registradores que sobram, vai todo para a pilha (SysV não parte) */
            if ((unsigned)(6 - ni) < (n + 7) / 8) {
                stack_put(stk, &sp, p, n, a > 8 ? a : 8);
                ni = 6;
                continue;
            }
            for (off = 0; off < n; off += 8) {
                unsigned long long w = 0, chunk = n - off < 8 ? n - off : 8;
                if (p) memcpy(&w, (const unsigned char *)p + off, chunk);
                put_int(&f, &ni, stk, &sp, w);
            }
            continue;
        }
        put_int(&f, &ni, stk, &sp, load_int(t, p, s));
    }
    f.nstack = (sp + 15) & ~15ull;
    cx_x64_call(fn, &f);
    if (!ret || sret || *rt == 'v') return;
    if (*rt == 'f') memcpy(ret, f.rv, 4);
    else if (*rt == 'd') memcpy(ret, f.rv, 8);
    else if (*rt == 'D') {
        /* limitação: o retorno long double está em st(0), não em xmm0/rax */
        unsigned long long n = retlen && retlen < 16 ? retlen : 16;
        memset(ret, 0, n);
    } else {
        unsigned char tmp[16];
        unsigned long long n = retlen ? retlen : 8;
        memcpy(tmp, &f.rax, 8);
        memcpy(tmp + 8, &f.rdx, 8);
        if (n > 16) n = 16;
        memcpy(ret, tmp, n);
    }
}
#endif
