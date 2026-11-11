/*
 * Desenrolamento C++ no x86_64 (Itanium + Mach-O).
 *
 * _Unwind_RaiseException / _Unwind_Resume / GetIP / SetIP / GetGR / SetGR /
 * GetLanguageSpecificData / GetRegionStart leem o .eh_frame (ELF ou __eh_frame)
 * e, quando a imagem é Mach-O, o __unwind_info / __compact_unwind no formato
 * que o libunwind do LLVM usa (Apache-2.0 com a exceção LLVM). O arm64 não
 * define estes _Unwind_*: o script tools/build-libsystem-arm64.ps1 continua
 * ligando o libunwind, sem tocar no ABI do kernel.
 *
 * __gxx_personality_v0 lê a LSDA. __cxa_throw, begin/end_catch e
 * std::current_exception / std::rethrow_exception passam a funcionar.
 * Uma exceção ativa por processo (não há TLS no libSystem freestanding).
 * No arm64, _Unwind_* continua o libunwind do script arm64. A personality daqui
 * usa esses acessores e acha o handler. Não mexe em *_arm64.S nem darwin_arm64.c.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

void *malloc(size_t n);
void *calloc(size_t n, size_t m);
void free(void *p);
void abort(void);

#if defined(__x86_64__)
#define UW_NATIVE 1
#else
#define UW_NATIVE 0
#endif

enum { EXC_HDR = 128, UW_AT = 96 };
enum { UR_END = 5, UR_FOUND = 6, UR_INSTALL = 7, UR_CONT = 8 };
enum { UA_SEARCH = 1, UA_CLEANUP = 2, UA_HANDLER = 4 };
static const u64 k_cxx = 0x474E5543432B2B00ULL;

static u8 *hdr_of(void *obj) { return (u8 *)obj - EXC_HDR; }
static void *obj_of_uw(void *uw) { return (u8 *)uw + (EXC_HDR - UW_AT); }
static void *uw_of(void *obj) { return hdr_of(obj) + UW_AT; }
static void **slot_ti(void *obj) { return (void **)hdr_of(obj); }
static void (**slot_dtor(void *obj))(void *) { return (void (**)(void *))(hdr_of(obj) + 8); }
static long *slot_refs(void *obj) { return (long *)(hdr_of(obj) + 16); }
static int *slot_prop(void *obj) { return (int *)(hdr_of(obj) + 24); }

static void *g_caught;
static int g_uncaught;

static void retain(void *obj) { if (obj) ++*slot_refs(obj); }
static void destroy(void *obj) {
    void (*d)(void *) = *slot_dtor(obj);
    if (d) d(obj);
    free(hdr_of(obj));
}
static void release(void *obj) {
    if (!obj) return;
    if (--*slot_refs(obj) <= 0) destroy(obj);
}

static u64 rd_u(const u8 **p, int n) {
    u64 v = 0;
    for (int i = 0; i < n; i++) v |= (u64)(*p)[i] << (8 * i);
    *p += n;
    return v;
}
static u64 rd_uleb(const u8 **p) {
    u64 v = 0; int s = 0; u8 b;
    do { b = *(*p)++; v |= (u64)(b & 0x7f) << s; s += 7; } while (b & 0x80);
    return v;
}
static int64_t rd_sleb(const u8 **p) {
    u64 v = 0; int s = 0; u8 b;
    do { b = *(*p)++; v |= (u64)(b & 0x7f) << s; s += 7; } while (b & 0x80);
    if (s < 64 && (b & 0x40)) v |= ~0ULL << s;
    return (int64_t)v;
}

static u64 enc_read(u8 enc, const u8 **p, u64 base) {
    if (enc == 0xff) return 0;
    const u8 *at = *p;
    u64 v = 0;
    switch (enc & 0x0f) {
    case 0x00: v = rd_u(p, 8); break;
    case 0x01: v = rd_uleb(p); break;
    case 0x02: v = rd_u(p, 2); break;
    case 0x03: v = rd_u(p, 4); break;
    case 0x04: v = rd_u(p, 8); break;
    case 0x09: v = (u64)rd_sleb(p); break;
    case 0x0a: v = (u64)(int16_t)rd_u(p, 2); break;
    case 0x0b: v = (u64)(int32_t)rd_u(p, 4); break;
    case 0x0c: v = rd_u(p, 8); break;
    default: return 0;
    }
    switch (enc & 0x70) {
    case 0x10: v += (u64)at; break;
    case 0x30: v += base; break;
    default: break;
    }
    if ((enc & 0x80) && v) v = *(u64 *)v;
    return v;
}

#if UW_NATIVE
struct uw_ctx {
    u64 r[17]; const u8 *lsda; u64 region, func_end, cfa, text_base, data_base;
    int (*pers)(int, int, u64, void *, struct uw_ctx *);
    u32 compact_enc; u64 compact_func; int have_compact; int fp_fallback; int is_sig;
    const u8 *fde, *eh; u64 eh_len; const u8 *fx;
};

__attribute__((naked)) static void cx_uw_capture(u64 *r) {
    __asm__ volatile(
        "movq %rax, 0(%rdi)\n\t"
        "movq %rdx, 8(%rdi)\n\t"
        "movq %rcx, 16(%rdi)\n\t"
        "movq %rbx, 24(%rdi)\n\t"
        "movq %rsi, 32(%rdi)\n\t"
        "movq %rdi, 40(%rdi)\n\t"
        "movq %rbp, 48(%rdi)\n\t"
        "leaq 8(%rsp), %rax\n\t"
        "movq %rax, 56(%rdi)\n\t"
        "movq %r8, 64(%rdi)\n\t"
        "movq %r9, 72(%rdi)\n\t"
        "movq %r10, 80(%rdi)\n\t"
        "movq %r11, 88(%rdi)\n\t"
        "movq %r12, 96(%rdi)\n\t"
        "movq %r13, 104(%rdi)\n\t"
        "movq %r14, 112(%rdi)\n\t"
        "movq %r15, 120(%rdi)\n\t"
        "movq (%rsp), %rax\n\t"
        "movq %rax, 128(%rdi)\n\t"
        "ret\n\t");
}
__attribute__((naked)) static void cx_uw_install(u64 *r) {
    __asm__ volatile(
        "movq 128(%rdi), %r11\n\t"
        "movq 56(%rdi), %rsp\n\t"
        "movq 0(%rdi), %rax\n\t"
        "movq 8(%rdi), %rdx\n\t"
        "movq 16(%rdi), %rcx\n\t"
        "movq 24(%rdi), %rbx\n\t"
        "movq 32(%rdi), %rsi\n\t"
        "movq 48(%rdi), %rbp\n\t"
        "movq 64(%rdi), %r8\n\t"
        "movq 72(%rdi), %r9\n\t"
        "movq 80(%rdi), %r10\n\t"
        "movq 96(%rdi), %r12\n\t"
        "movq 104(%rdi), %r13\n\t"
        "movq 112(%rdi), %r14\n\t"
        "movq 120(%rdi), %r15\n\t"
        "movq 40(%rdi), %rdi\n\t"
        "jmp *%r11\n\t");
}

u64 _Unwind_GetGR(void *p, int n) { struct uw_ctx *c = p; return (n >= 0 && n < 17) ? c->r[n] : 0; }
void _Unwind_SetGR(void *p, int n, u64 v) { struct uw_ctx *c = p; if (n >= 0 && n < 17) c->r[n] = v; }
u64 _Unwind_GetIP(void *p) { return ((struct uw_ctx *)p)->r[16]; }
void _Unwind_SetIP(void *p, u64 ip) { ((struct uw_ctx *)p)->r[16] = ip; }
u64 _Unwind_GetLanguageSpecificData(void *p) { return (u64)((struct uw_ctx *)p)->lsda; }
u64 _Unwind_GetRegionStart(void *p) { return ((struct uw_ctx *)p)->region; }

static int cfi_step(const u8 *cie, const u8 *cie_end, const u8 *fde_init, const u8 *fde_end, u64 pc, struct uw_ctx *c, int apply) {
    const u8 *p = cie;
    if (p >= cie_end || *p++ != 1) return 0;
    u8 aug[8]; int na = 0;
    while (p < cie_end && *p) { if (na < 8) aug[na++] = *p; p++; }
    if (p < cie_end) p++;
    u64 code_align = rd_uleb(&p);
    if (!code_align) code_align = 1;
    int64_t data_align = rd_sleb(&p);
    if (!data_align) data_align = -8;
    if (p < cie_end) p++; /* return-address register, CIE version 1 */
    u8 fde_enc = 0, lsda_enc = 0xff;
    if (na && aug[0] == 'z') {
        u64 alen = rd_uleb(&p);
        const u8 *ae = p + alen;
        for (int i = 1; i < na && p < ae; i++) {
            if (aug[i] == 'R') fde_enc = *p++;
            else if (aug[i] == 'L') lsda_enc = *p++;
            else if (aug[i] == 'P') { u8 pe = *p++; enc_read(pe, &p, 0); }
            else if (aug[i] == 'S') {}
            else break;
        }
        p = ae;
    }
    const u8 *ip = fde_init;
    u64 loc, range;
    if (!fde_enc) {
        loc = (u64)(int32_t)rd_u(&ip, 4) + (u64)fde_init;
        range = rd_u(&ip, 4);
    } else {
        loc = enc_read(fde_enc, &ip, 0);
        range = enc_read(fde_enc & 0x0f, &ip, 0);
    }
    if (na && aug[0] == 'z') {
        u64 n = rd_uleb(&ip);
        const u8 *ls = ip;
        if (lsda_enc != 0xff) c->lsda = (const u8 *)enc_read(lsda_enc, &ls, 0);
        ip += n;
    }
    if (pc < loc || pc >= loc + range) return 0;
    c->region = loc;
    c->func_end = loc + range;
    enum { SAME, UNDEF, OFF, REG, VAL };
    u8 how[17], cfa_reg = 7; u64 offv[17]; int64_t cfa_off = 8;
    for (int i = 0; i < 17; i++) { how[i] = SAME; offv[i] = 0; }
    how[16] = OFF; offv[16] = (u64)(int64_t)-8;
    const u8 *ops[2] = {p, ip};
    const u8 *ends[2] = {cie_end, fde_end};
    u64 cur = loc;
    u8 sh[8][17], sreg[8]; int64_t soff[8]; int sp = 0;
    for (int pass = 0; pass < 2; pass++) {
        const u8 *q = ops[pass];
        while (q < ends[pass] && cur <= pc) {
            u8 op = *q++;
            u8 hi = op >> 6, lo = op & 0x3f;
            if (op == 0) break;
            if (hi == 1) { cur += (u64)lo * (code_align ? code_align : 1); continue; }
            if (hi == 2) { how[lo] = OFF; offv[lo] = (u64)((int64_t)rd_uleb(&q) * data_align); continue; }
            if (hi == 3) { how[lo] = SAME; continue; }
            switch (op) {
            case 0x02: cur += *q++; break;
            case 0x03: cur += rd_u(&q, 2); break;
            case 0x04: cur += rd_u(&q, 4); break;
            case 0x07: lo = *q++; how[lo] = UNDEF; break;
            case 0x08: lo = *q++; how[lo] = SAME; break;
            case 0x09: { u8 r = *q++; u64 o = rd_uleb(&q); how[r] = OFF; offv[r] = (u64)((int64_t)o * data_align); break; }
            case 0x0a: { if (sp < 8) { memcpy(sh[sp], how, 17); sreg[sp] = cfa_reg; soff[sp] = cfa_off; sp++; } break; }
            case 0x0b: { if (sp) { sp--; memcpy(how, sh[sp], 17); cfa_reg = sreg[sp]; cfa_off = soff[sp]; } break; }
            case 0x0c: cfa_reg = *q++; cfa_off = (int64_t)rd_uleb(&q); break;
            case 0x0d: cfa_reg = *q++; break;
            case 0x0e: cfa_off = (int64_t)rd_uleb(&q); break;
            case 0x12: { u8 r = *q++; rd_uleb(&q); how[r] = SAME; break; }
            case 0x2e: rd_uleb(&q); break;
            default:
                if (op >= 0x30 && op <= 0x3f) break;
                return 0;
            }
            if (cur > pc) break;
        }
    }
    u64 cfa = c->r[cfa_reg] + (u64)cfa_off;
    c->cfa = cfa;
    if (!apply) return 1;
    u64 nr[17];
    memcpy(nr, c->r, sizeof nr);
    for (int i = 0; i < 17; i++) {
        if (how[i] == OFF) nr[i] = *(u64 *)(cfa + (int64_t)offv[i]);
        else if (how[i] == VAL) nr[i] = cfa + (int64_t)offv[i];
        else if (how[i] == REG) nr[i] = c->r[offv[i]];
    }
    nr[7] = cfa;
    memcpy(c->r, nr, sizeof nr);
    return 1;
}

static int fde_match(const u8 *fde, const u8 *eh, u64 eh_len, u64 pc, struct uw_ctx *c, int apply) {
    u32 len = (u32)rd_u(&fde, 4);
    if (len == 0xffffffffu) return 0;
    const u8 *end = fde + len;
    if (end > eh + eh_len) return 0;
    u32 id = (u32)rd_u(&fde, 4);
    if (id == 0) return 0;
    const u8 *cie = (fde - 4) - id;
    if (cie < eh) return 0;
    u32 clen = (u32)rd_u(&cie, 4);
    const u8 *cie_end = cie + clen;
    u32 cid = (u32)rd_u(&cie, 4);
    if (cid != 0) return 0;
    return cfi_step(cie, cie_end, fde, end, pc, c, apply);
}

extern const char __ehdr_start[] __attribute__((weak));
extern const char _end[] __attribute__((weak));

static int elf_eh(u64 pc, const u8 **eh, u64 *eh_len, const u8 **hdr, u64 *hdr_len) {
    *eh = 0; *eh_len = 0; *hdr = 0; *hdr_len = 0;
    if (!__ehdr_start || !__ehdr_start[0]) return 0;
    const u8 *e = (const u8 *)__ehdr_start;
    if (e[0] != 0x7f) return 0;
    u64 phoff = *(const u64 *)(e + 32);
    u16 phentsize = *(const u16 *)(e + 54), phnum = *(const u16 *)(e + 56);
    u64 lo = (u64)e, hi = _end ? (u64)_end : lo;
    if (pc < lo || (hi > lo && pc >= hi)) return 0;
    for (u16 i = 0; i < phnum; i++) {
        const u8 *ph = e + phoff + (u64)i * phentsize;
        u32 type = *(const u32 *)ph;
        if (type != 0x6474e550) continue;
        u64 off = *(const u64 *)(ph + 8), sz = *(const u64 *)(ph + 32);
        *hdr = e + off; *hdr_len = sz;
        return 1;
    }
    return 0;
}

static const u8 *hdr_fde(const u8 *hdr, u64 pc) {
    if (!hdr || hdr[0] != 1) return 0;
    const u8 *p = hdr + 4;
    u64 frame = enc_read(hdr[1], &p, (u64)hdr);
    u64 count = enc_read(hdr[2], &p, (u64)hdr);
    u8 tent = hdr[3];
    if ((tent & 0x0f) != 0x0b || !count) {
        (void)frame;
        return 0;
    }
    const u8 *tab = p;
    u64 lo = 0, hi = count;
    while (lo + 1 < hi) {
        u64 mid = (lo + hi) / 2;
        const u8 *e = tab + mid * 8;
        u64 start = enc_read(tent, &e, (u64)hdr);
        if (start <= pc) lo = mid; else hi = mid;
    }
    const u8 *e = tab + lo * 8;
    u64 start = enc_read(tent, &e, (u64)hdr);
    u64 fde = enc_read(tent, &e, (u64)hdr);
    if (start > pc || !fde) return 0;
    return (const u8 *)fde;
}

struct dyld_uw { const void *mh, *dwarf; unsigned long dwarf_len; const void *compact; unsigned long compact_len; };
__attribute__((weak)) int _dyld_find_unwind_sections(void *addr, struct dyld_uw *info);
__attribute__((weak)) int cx_unwind_disabled(void);

static u32 ru32(const u8 *p) { return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }
static u16 ru16(const u8 *p) { return (u16)(p[0] | (p[1] << 8)); }

enum {
    UW_HAS_LSDA = 0x40000000u,
    UW_PERS_MASK = 0x30000000u,
    X64_MODE = 0x0F000000u,
    X64_RBP = 0x01000000u,
    X64_IMMD = 0x02000000u,
    X64_IND = 0x03000000u,
    X64_DWARF = 0x04000000u,
    REG_NONE = 0, REG_RBX = 1, REG_R12 = 2, REG_R13 = 3, REG_R14 = 4, REG_R15 = 5, REG_RBP = 6
};

static void set_dwarf_reg(struct uw_ctx *c, int uwreg, u64 v) {
    int map[7] = {-1, 3, 12, 13, 14, 15, 6};
    if (uwreg > 0 && uwreg < 7 && map[uwreg] >= 0) c->r[map[uwreg]] = v;
}

static int step_compact(u32 enc, u64 func, struct uw_ctx *c) {
    u32 mode = enc & X64_MODE;
    if (mode == X64_DWARF) return 0;
    if (mode == X64_RBP) {
        u32 off = (enc >> 16) & 0xff;
        u32 locs = enc & 0x7fff;
        u64 saved = c->r[6] - 8 * off;
        for (int i = 0; i < 5; i++) {
            int reg = locs & 7;
            if (reg) set_dwarf_reg(c, reg, *(u64 *)saved);
            saved += 8;
            locs >>= 3;
        }
        u64 rbp = c->r[6];
        c->r[6] = *(u64 *)rbp;
        c->r[16] = *(u64 *)(rbp + 8);
        c->r[7] = rbp + 16;
        return 1;
    }
    if (mode == X64_IMMD || mode == X64_IND) {
        u32 stack_enc = (enc >> 16) & 0xff;
        u32 adj = (enc >> 13) & 7;
        u32 nreg = (enc >> 10) & 7;
        u32 perm = enc & 0x3ff;
        u32 stack = stack_enc * 8;
        if (mode == X64_IND) {
            u32 subl = *(u32 *)(func + stack_enc);
            stack = subl + 8 * adj;
        }
        u32 per[6] = {0};
        switch (nreg) {
        case 6: per[0] = perm / 120; perm -= per[0] * 120; per[1] = perm / 24; perm -= per[1] * 24; per[2] = perm / 6; perm -= per[2] * 6; per[3] = perm / 2; perm -= per[3] * 2; per[4] = perm; per[5] = 0; break;
        case 5: per[0] = perm / 120; perm -= per[0] * 120; per[1] = perm / 24; perm -= per[1] * 24; per[2] = perm / 6; perm -= per[2] * 6; per[3] = perm / 2; perm -= per[3] * 2; per[4] = perm; break;
        case 4: per[0] = perm / 60; perm -= per[0] * 60; per[1] = perm / 12; perm -= per[1] * 12; per[2] = perm / 3; perm -= per[2] * 3; per[3] = perm; break;
        case 3: per[0] = perm / 20; perm -= per[0] * 20; per[1] = perm / 4; perm -= per[1] * 4; per[2] = perm; break;
        case 2: per[0] = perm / 5; perm -= per[0] * 5; per[1] = perm; break;
        case 1: per[0] = perm; break;
        default: break;
        }
        int saved_r[6]; int used[7] = {0};
        for (u32 i = 0; i < nreg; i++) {
            u32 ren = 0;
            for (int u = 1; u < 7; u++) if (!used[u]) {
                if (ren == per[i]) { saved_r[i] = u; used[u] = 1; break; }
                ren++;
            }
        }
        u64 at = c->r[7] + stack - 8 - 8 * (u64)nreg;
        for (u32 i = 0; i < nreg; i++) { set_dwarf_reg(c, saved_r[i], *(u64 *)at); at += 8; }
        c->r[16] = *(u64 *)at;
        c->r[7] = at + 8;
        return 1;
    }
    return 0;
}

static int lookup_compact(u64 pc, const u8 *base, const u8 *sec, struct uw_ctx *c, const u8 **fde_hint, const u8 *eh) {
    if (!sec || ru32(sec) != 1) return 0;
    u32 index_off = ru32(sec + 20), index_n = ru32(sec + 24);
    if (index_n < 2) return 0;
    u64 target = pc - (u64)base;
    u32 lo = 0, hi = index_n, last = index_n - 1;
    while (lo < hi) {
        u32 mid = (lo + hi) / 2;
        u32 fo = ru32(sec + index_off + mid * 12);
        if (fo <= target) {
            u32 nx = ru32(sec + index_off + (mid + 1) * 12);
            if (mid == last || nx > target) { lo = mid; break; }
            lo = mid + 1;
        } else hi = mid;
    }
    u32 page_off = ru32(sec + index_off + lo * 12 + 4);
    u32 lsda_off = ru32(sec + index_off + lo * 12 + 8);
    u32 lsda_end = ru32(sec + index_off + (lo + 1) * 12 + 8);
    u32 first = ru32(sec + index_off + lo * 12);
    u32 next_fn = ru32(sec + index_off + (lo + 1) * 12);
    const u8 *page = sec + page_off;
    u32 kind = ru32(page);
    u32 enc = 0; u64 func = 0;
    if (kind == 2) {
        u16 eoff = ru16(page + 4), ec = ru16(page + 6);
        u32 a = 0, b = ec;
        while (a < b) {
            u32 mid = (a + b) / 2;
            u32 fo = ru32(page + eoff + mid * 8);
            if (fo <= target) {
                if (mid + 1 == ec || ru32(page + eoff + (mid + 1) * 8) > target) { a = mid; break; }
                a = mid + 1;
            } else b = mid;
        }
        enc = ru32(page + eoff + a * 8 + 4);
        func = (u64)base + ru32(page + eoff + a * 8);
        if (pc < func) return 0;
    } else if (kind == 3) {
        u16 eoff = ru16(page + 4), ec = ru16(page + 6);
        u32 rel = (u32)(target - first);
        u32 a = 0, b = ec, last_e = ec ? ec - 1 : 0;
        while (a < b) {
            u32 mid = (a + b) / 2;
            u32 fo = ru32(page + eoff + mid * 4) & 0x00ffffffu;
            if (fo <= rel) {
                u32 nx = (mid == last_e) ? 0xffffffffu : (ru32(page + eoff + (mid + 1) * 4) & 0x00ffffffu);
                if (mid == last_e || nx > rel) { a = mid; break; }
                a = mid + 1;
            } else b = mid;
        }
        u32 ent = ru32(page + eoff + a * 4);
        func = (u64)base + first + (ent & 0x00ffffffu);
        u16 ei = (u16)(ent >> 24);
        u32 common_off = ru32(sec + 4), common_n = ru32(sec + 8);
        if (ei < common_n) enc = ru32(sec + common_off + ei * 4);
        else enc = ru32(page + ru16(page + 8) + (ei - common_n) * 4);
        if (pc < func) return 0;
    } else return 0;
    c->region = func;
    c->lsda = 0;
    c->pers = 0;
    if (enc & UW_HAS_LSDA) {
        const u8 *ls = sec + lsda_off;
        u32 n = (lsda_end - lsda_off) / 8;
        u32 fo = (u32)(func - (u64)base);
        for (u32 i = 0; i < n; i++) if (ru32(ls + i * 8) == fo) { c->lsda = base + ru32(ls + i * 8 + 4); break; }
    }
    u32 pi = (enc & UW_PERS_MASK) >> 28;
    if (pi) {
        pi--;
        u32 poff = ru32(sec + 12);
        int32_t delta = (int32_t)ru32(sec + poff + pi * 4);
        const u8 *slot = base + (u32)delta;
        c->pers = (int (*)(int, int, u64, void *, struct uw_ctx *))*(void **)slot;
    }
    c->compact_enc = enc;
    c->compact_func = func;
    c->have_compact = (enc & X64_MODE) != X64_DWARF;
    if (c->have_compact) {
        u32 mode = enc & X64_MODE;
        if (mode == X64_RBP) c->cfa = c->r[6] + 16;
        else if (mode == X64_IMMD || mode == X64_IND) {
            u32 stack_enc = (enc >> 16) & 0xff;
            u32 adj = (enc >> 13) & 7;
            u32 stack = stack_enc * 8;
            if (mode == X64_IND) stack = *(u32 *)(func + stack_enc) + 8 * adj;
            c->cfa = c->r[7] + stack;
        }
    }
    if ((enc & X64_MODE) == X64_DWARF && eh) *fde_hint = eh + (enc & 0x00ffffffu);
    (void)next_fn;
    return 1;
}

/* Quadro que o kernel monta em sig_deliver (process.c): magic no início, mcontext logo depois. */
#define MOS_SIGFRAME_MAGIC 0x6D4F535349474E4CULL
extern void cx_sigtramp(void) __attribute__((weak));

static int sig_magic(u64 a) {
    if (a < 0x10000 || (a & 7)) return 0;
    return *(volatile u64 *)a == MOS_SIGFRAME_MAGIC;
}

static int note_sig(struct uw_ctx *c) {
    c->is_sig = 0;
    if (cx_sigtramp && c->region == (u64)(uintptr_t)cx_sigtramp) c->is_sig = 1;
    if (sig_magic(c->cfa) || sig_magic(c->r[7] + 8)) c->is_sig = 1;
    return 1;
}

/* Restaura o contexto interrompido a partir do mcontext do Darwin (mos_thread_state_t). */
static int step_sig(struct uw_ctx *c) {
    u64 fa = 0;
    if (sig_magic(c->cfa)) fa = c->cfa;
    else if (sig_magic(c->r[7] + 8)) fa = c->r[7] + 8;
    else return 0;
    /* usigframe: magic+uctx (16) + siginfo (104) + ucontext (56) = 176; mcontext.__ss depois de 16. */
    const u8 *ss = (const u8 *)(uintptr_t)fa + 176 + 16;
    static const int to_dwarf[17] = {0, 3, 2, 1, 5, 4, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
    u64 v[17];
    for (int i = 0; i < 17; i++) v[i] = *(const u64 *)(ss + (u64)i * 8);
    for (int i = 0; i < 17; i++) c->r[to_dwarf[i]] = v[i];
    c->have_compact = 0;
    c->fde = 0;
    c->fp_fallback = 0;
    c->is_sig = 0;
    c->fx = ss + 21 * 8 + 8; /* fpu_reserved[2] + FXSAVE, xmm0 em +160 */
    return 1;
}

/* FDEs registrados em tempo de execução (__register_frame, uw_register.c): código gerado por JIT (V8, o
 * wasm do Chromium, LLVM ORC) descreve as próprias funções com um FDE solto, como o libunwind da Apple
 * aceita. Cada entrada guarda o FDE e o intervalo [CIE, fim do FDE] que fde_match usa como limite. */
enum { UW_DYN_MAX = 4096 };
static struct { const u8 *fde, *lo; u64 len; } uw_dyn[UW_DYN_MAX];
static u32 uw_dyn_n;
static int uw_dyn_lock;
static void uw_dyn_acquire(void) { while (__atomic_exchange_n(&uw_dyn_lock, 1, __ATOMIC_ACQUIRE)) {} }
static void uw_dyn_release(void) { __atomic_store_n(&uw_dyn_lock, 0, __ATOMIC_RELEASE); }
/* 0 = registrou; -1 = não é um FDE (CIE, tamanho 0 ou 64 bits) ou a tabela encheu */
int mos_uw_dyn_add(const void *p) {
    const u8 *fde = p, *q = p;
    if (!fde) return -1;
    u32 len = (u32)rd_u(&q, 4);
    if (!len || len == 0xffffffffu) return -1;
    u32 id = (u32)rd_u(&q, 4);
    if (!id) return -1;
    const u8 *cie = (fde + 4) - id;
    const u8 *cq = cie;
    u32 clen = (u32)rd_u(&cq, 4);
    if (!clen || clen == 0xffffffffu) return -1;
    const u8 *lo = cie < fde ? cie : fde;
    const u8 *hi_f = fde + 4 + len, *hi_c = cie + 4 + clen;
    const u8 *hi = hi_f > hi_c ? hi_f : hi_c;
    int r = -1;
    uw_dyn_acquire();
    for (u32 i = 0; i < uw_dyn_n; i++) if (uw_dyn[i].fde == fde) { r = 0; goto out; }
    if (uw_dyn_n < UW_DYN_MAX) {
        uw_dyn[uw_dyn_n].fde = fde;
        uw_dyn[uw_dyn_n].lo = lo;
        uw_dyn[uw_dyn_n].len = (u64)(hi - lo);
        uw_dyn_n++;
        r = 0;
    }
out:
    uw_dyn_release();
    return r;
}
void mos_uw_dyn_remove(const void *fde) {
    uw_dyn_acquire();
    for (u32 i = 0; i < uw_dyn_n; i++)
        if (uw_dyn[i].fde == fde) { uw_dyn[i] = uw_dyn[--uw_dyn_n]; break; }
    uw_dyn_release();
}
static int uw_dyn_find(u64 look, struct uw_ctx *c) {
    int ok = 0;
    uw_dyn_acquire();
    for (u32 i = 0; i < uw_dyn_n && !ok; i++) {
        if (fde_match(uw_dyn[i].fde, uw_dyn[i].lo, uw_dyn[i].len, look, c, 0)) {
            c->fde = uw_dyn[i].fde; c->eh = uw_dyn[i].lo; c->eh_len = uw_dyn[i].len;
            ok = 1;
        }
    }
    uw_dyn_release();
    return ok;
}

static int load_frame(struct uw_ctx *c) {
    u64 pc = c->r[16];
    /* A linha do CIE vale para a instrução do call, não para o endereço de retorno. */
    u64 look = pc ? pc - 1 : 0;
    c->lsda = 0; c->region = 0; c->func_end = 0; c->pers = 0; c->have_compact = 0; c->fde = 0; c->fp_fallback = 0; c->is_sig = 0; c->fx = 0;
    if (cx_unwind_disabled && cx_unwind_disabled()) return 0;
    struct dyld_uw info;
    memset(&info, 0, sizeof info);
    if (_dyld_find_unwind_sections && _dyld_find_unwind_sections((void *)pc, &info)) {
        c->text_base = c->data_base = (u64)info.mh;
        const u8 *hint = 0;
        int k = 0;
        if (info.compact) k = lookup_compact(look, (const u8 *)info.mh, (const u8 *)info.compact, c, &hint, (const u8 *)info.dwarf);
        if (c->have_compact) return note_sig(c);
        const u8 *fde = hint;
        if (!fde && info.dwarf) {
            const u8 *q = (const u8 *)info.dwarf;
            const u8 *end = q + info.dwarf_len;
            while (q + 4 < end) {
                u32 len = (u32)rd_u(&q, 4);
                if (!len) break;
                const u8 *body = q;
                if (fde_match(body - 4, (const u8 *)info.dwarf, info.dwarf_len, look, c, 0)) {
                    c->fde = body - 4; c->eh = (const u8 *)info.dwarf; c->eh_len = info.dwarf_len;
                    return note_sig(c);
                }
                q = body + len;
            }
        } else if (fde && info.dwarf) {
            if (fde_match(fde, (const u8 *)info.dwarf, info.dwarf_len, look, c, 0)) {
                c->fde = fde; c->eh = (const u8 *)info.dwarf; c->eh_len = info.dwarf_len;
                return note_sig(c);
            }
        }
        return k ? note_sig(c) : 0;
    }
    const u8 *eh = 0, *gh = 0; u64 el = 0, gl = 0;
    if (elf_eh(look, &eh, &el, &gh, &gl)) {
    c->text_base = c->data_base = (u64)__ehdr_start;
    const u8 *fde = hdr_fde(gh, look);
    if (fde && fde_match(fde, (const u8 *)__ehdr_start, (u64)(_end - __ehdr_start), look, c, 0)) {
        c->fde = fde; c->eh = (const u8 *)__ehdr_start; c->eh_len = (u64)(_end - __ehdr_start);
        return note_sig(c);
    }
    }
    if (uw_dyn_n && uw_dyn_find(look, c)) return note_sig(c);
    u64 rbp = c->r[6], rsp = c->r[7];
    if (!rbp || (rbp & 7) || rbp < rsp || rbp - rsp > 0x100000) return 0;
    c->fp_fallback = 1;
    c->cfa = rbp + 16;
    return note_sig(c);
}

static int step_frame(struct uw_ctx *c) {
    if (c->is_sig && step_sig(c)) return 1;
    u64 pc = c->r[16] ? c->r[16] - 1 : 0;
    if (c->fp_fallback) {
        u64 rbp = c->r[6];
        c->r[16] = *(u64 *)(rbp + 8);
        c->r[7] = rbp + 16;
        c->r[6] = *(u64 *)rbp;
        return 1;
    }
    if (c->have_compact) return step_compact(c->compact_enc, c->compact_func, c);
    if (c->fde) return fde_match(c->fde, c->eh, c->eh_len, pc, c, 1);
    return 0;
}

struct uw_exc { u64 cls; void (*cleanup)(int, void *); u64 private_1, private_2; };
int __gxx_personality_v0(int, int, u64, void *, void *);
static void unwind_loop(void *exc, int resume) {
    struct uw_ctx start;
    memset(&start, 0, sizeof start);
    cx_uw_capture(start.r);
    struct uw_exc *ex = exc;
    u64 handler_region = resume ? ex->private_1 : 0;
    int phase2 = resume;
    struct uw_ctx cur = start;
    for (int guard = 0; guard < 128; guard++) {
        u64 ip0 = cur.r[16];
        if (!ip0 || !load_frame(&cur)) abort();
        int (*pers)(int, int, u64, void *, struct uw_ctx *) = cur.pers;
        if (!pers && cur.lsda) pers = (int (*)(int, int, u64, void *, struct uw_ctx *))(void *)__gxx_personality_v0;
        if (pers) {
            int act = phase2 ? UA_CLEANUP : UA_SEARCH;
            if (phase2 && cur.region == handler_region) act |= UA_HANDLER;
            int rc = pers(1, act, ((u64 *)exc)[0], exc, &cur);
            if (!phase2 && rc == UR_FOUND) {
                handler_region = cur.region;
                ex->private_1 = cur.region;
                phase2 = 1;
                cur = start;
                continue;
            }
            if (phase2 && rc == UR_INSTALL) cx_uw_install(cur.r);
            if (rc != UR_CONT && rc != UR_FOUND) abort();
        }
        if (!step_frame(&cur) || cur.r[16] == ip0) abort();
    }
    abort();
}

int _Unwind_RaiseException(struct uw_exc *e) { unwind_loop(e, 0); return UR_END; }
void _Unwind_Resume(struct uw_exc *e) { unwind_loop(e, 1); }

void _Unwind_DeleteException(struct uw_exc *e) {
    if (e && e->cleanup) e->cleanup(1, e);
}

u64 _Unwind_GetCFA(void *p) { return ((struct uw_ctx *)p)->cfa; }
u64 _Unwind_GetDataRelBase(void *p) { return ((struct uw_ctx *)p)->data_base; }
u64 _Unwind_GetTextRelBase(void *p) { return ((struct uw_ctx *)p)->text_base; }
u64 _Unwind_GetIPInfo(void *p, int *before) {
    if (before) *before = 0;
    return ((struct uw_ctx *)p)->r[16];
}

int _Unwind_Backtrace(int (*cb)(void *, void *), void *arg) {
    struct uw_ctx cur;
    memset(&cur, 0, sizeof cur);
    cx_uw_capture(cur.r);
    for (int guard = 0; guard < 128; guard++) {
        if (!cur.r[16] || !load_frame(&cur)) return 5;
        int rc = cb(&cur, arg);
        if (rc) return rc;
        u64 ip0 = cur.r[16];
        if (!step_frame(&cur) || cur.r[16] == ip0) return 5;
    }
    return 5;
}

#else
int _Unwind_RaiseException(void *);
void _Unwind_Resume(void *);
u64 _Unwind_GetIP(void *);
void _Unwind_SetIP(void *, u64);
void _Unwind_SetGR(void *, int, u64);
u64 _Unwind_GetLanguageSpecificData(void *);
u64 _Unwind_GetRegionStart(void *);
#endif

int __gxx_personality_v0(int ver, int actions, u64 cls, void *exc, void *ctx) {
    (void)cls;
    if (ver != 1 || !ctx) return 3;
    const u8 *p = (const u8 *)_Unwind_GetLanguageSpecificData(ctx);
    if (!p) return UR_CONT;
    u64 lpstart = _Unwind_GetRegionStart(ctx);
    u8 start_enc = *p++;
    if (start_enc != 0xff) lpstart = enc_read(start_enc, &p, 0);
    u8 tenc = *p++;
    const u8 *types = 0;
    if (tenc != 0xff) { u64 off = rd_uleb(&p); types = p + off; }
    u8 csenc = *p++;
    u64 cslen = rd_uleb(&p);
    const u8 *cs = p;
    const u8 *action_tab = p + cslen;
    u64 ip = _Unwind_GetIP(ctx);
    if (ip) ip--;
    u64 rel = ip - lpstart;
    int found = 0; u64 pad = 0; u64 act_off = 0;
    const u8 *q = cs;
    while (q < action_tab) {
        u64 start = enc_read(csenc, &q, 0);
        u64 len = enc_read(csenc, &q, 0);
        u64 lpad = enc_read(csenc, &q, 0);
        u64 act = rd_uleb(&q);
        if (rel >= start && rel < start + len) { found = 1; pad = lpad; act_off = act; break; }
    }
    if (!found || !pad) return UR_CONT;
    if (act_off == 0) {
        if (!(actions & UA_CLEANUP)) return UR_CONT;
        _Unwind_SetGR(ctx, 0, (u64)exc);
        _Unwind_SetGR(ctx, 1, 0);
        _Unwind_SetIP(ctx, lpstart + pad);
        return UR_INSTALL;
    }
    const u8 *ap = action_tab + (act_off - 1);
    void *obj = obj_of_uw(exc);
    void *have = *slot_ti(obj);
    int saw_cleanup = 0, matched = 0;
    int64_t chosen = 0;
    for (int guard = 0; guard < 32; guard++) {
        int64_t filter = rd_sleb(&ap);
        int64_t disp = rd_sleb(&ap);
        if (filter == 0) saw_cleanup = 1;
        else if (filter > 0) {
            void *need = 0;
            if (types && tenc != 0xff) {
                int sz = ((tenc & 0x0f) == 0 || (tenc & 0x0f) == 4 || (tenc & 0x0f) == 0x0c) ? 8 : ((tenc & 0x0f) == 2 || (tenc & 0x0f) == 0x0a) ? 2 : 4;
                const u8 *tp = types - (u64)filter * (u64)sz;
                const u8 *save = tp;
                need = (void *)enc_read(tenc, &save, _Unwind_GetRegionStart(ctx));
            }
            int ok = !need;
            if (!ok && have) {
                if (have == need) ok = 1;
                else {
                    const char *a = ((const char **)have)[1], *b = ((const char **)need)[1];
                    if (a && b && strcmp(a, b) == 0) ok = 1;
                }
            }
            if (ok) { matched = 1; chosen = filter; break; }
        }
        if (disp == 0) break;
        ap += disp;
    }
    if (actions & UA_SEARCH) return matched ? UR_FOUND : UR_CONT;
    if ((actions & UA_HANDLER) && matched) {
        _Unwind_SetGR(ctx, 0, (u64)exc);
        _Unwind_SetGR(ctx, 1, (u64)chosen);
        _Unwind_SetIP(ctx, lpstart + pad);
        return UR_INSTALL;
    }
    if ((actions & UA_CLEANUP) && saw_cleanup && !(actions & UA_HANDLER)) {
        _Unwind_SetGR(ctx, 0, (u64)exc);
        _Unwind_SetGR(ctx, 1, 0);
        _Unwind_SetIP(ctx, lpstart + pad);
        return UR_INSTALL;
    }
    return UR_CONT;
}


void __cxa_throw(void *obj, void *tinfo, void (*dtor)(void *)) {
    if (!*slot_refs(obj)) *slot_refs(obj) = 1;
    *slot_ti(obj) = tinfo;
    *slot_dtor(obj) = dtor;
    *slot_prop(obj) = 0;
    u64 *uw = (u64 *)uw_of(obj);
    uw[0] = k_cxx;
    uw[1] = 0;
    uw[2] = 0;
    uw[3] = 0;
    g_uncaught++;
#if UW_NATIVE
    _Unwind_RaiseException((struct uw_exc *)uw);
#else
    _Unwind_RaiseException(uw);
#endif
    abort();
}

void *__cxa_begin_catch(void *exc) {
    void *obj = obj_of_uw(exc);
    *slot_prop(obj) = 0;
    if (g_uncaught) g_uncaught--;
    g_caught = obj;
    return obj;
}
void __cxa_end_catch(void) {
    void *obj = g_caught;
    if (!obj) return;
    g_caught = 0;
    if (*slot_prop(obj)) return;
    release(obj);
}
void __cxa_rethrow(void) {
    void *obj = g_caught;
    if (!obj) abort();
    *slot_prop(obj) = 1;
    g_uncaught++;
    g_caught = 0;
#if UW_NATIVE
    _Unwind_Resume((struct uw_exc *)uw_of(obj));
#else
    _Unwind_Resume(uw_of(obj));
#endif
    abort();
}

static void rethrow_obj(void *obj) {
    if (!obj) abort();
    *slot_prop(obj) = 1;
    g_uncaught++;
#if UW_NATIVE
    _Unwind_RaiseException((struct uw_exc *)uw_of(obj));
#else
    _Unwind_RaiseException(uw_of(obj));
#endif
    abort();
}

typedef struct { void *p; } ep_s;
void ep_default(ep_s *d) __asm__("_ZNSt13exception_ptrC1Ev");
void ep_default(ep_s *d) { d->p = 0; }
void ep_default2(ep_s *d) __asm__("_ZNSt13exception_ptrC2Ev");
void ep_default2(ep_s *d) { d->p = 0; }
void ep_from(ep_s *ret, void *p) __asm__("_ZNSt13exception_ptr31__from_native_exception_pointerEPv");
void ep_from(ep_s *ret, void *p) { if (p) retain(p); ret->p = p; }
void ep_copy(ep_s *d, const ep_s *s) __asm__("_ZNSt13exception_ptrC1ERKS_");
void ep_copy(ep_s *d, const ep_s *s) { d->p = s->p; retain(d->p); }
void ep_copy2(ep_s *d, const ep_s *s) __asm__("_ZNSt13exception_ptrC2ERKS_");
void ep_copy2(ep_s *d, const ep_s *s) { ep_copy(d, s); }
void ep_dtor(ep_s *d) __asm__("_ZNSt13exception_ptrD1Ev");
void ep_dtor(ep_s *d) { release(d->p); d->p = 0; }
void ep_dtor2(ep_s *d) __asm__("_ZNSt13exception_ptrD2Ev");
void ep_dtor2(ep_s *d) { ep_dtor(d); }
ep_s *ep_eq(ep_s *d, const ep_s *s) __asm__("_ZNSt13exception_ptraSERKS_");
ep_s *ep_eq(ep_s *d, const ep_s *s) {
    if (d->p != s->p) { retain(s->p); release(d->p); d->p = s->p; }
    return d;
}
void cur_exc(ep_s *ret) __asm__("_ZSt17current_exceptionv");
void cur_exc(ep_s *ret) { ret->p = g_caught; retain(ret->p); }
void rethrow_exc(ep_s *e) __asm__("_ZSt17rethrow_exceptionSt13exception_ptr");
void rethrow_exc(ep_s *e) { rethrow_obj(e ? e->p : 0); }
int uncaught1(void) __asm__("_ZSt18uncaught_exceptionv");
int uncaught1(void) { return g_uncaught > 0; }
int uncaughtn(void) __asm__("_ZSt19uncaught_exceptionsv");
int uncaughtn(void) { return g_uncaught; }

#if defined(__x86_64__)
/*
 * API pública da libunwind (Apple SDK: unw_context_t = 128 qwords, unw_cursor_t = 140).
 * O cursor guarda o uw_ctx do motor acima; não há um segundo interpretador de CFI.
 * Registradores no contexto: DWARF / UNW_X86_64_* em data[0..16], XMM0..15 em seguida (16 bytes cada).
 */
enum {
    UNW_ESUCCESS = 0, UNW_EUNSPEC = -6540, UNW_EBADREG = -6542, UNW_EINVAL = -6547, UNW_ENOINFO = -6549
};
enum { UNW_REG_IP = -1, UNW_REG_SP = -2, UNW_X86_64_XMM0 = 17 };

struct unw_cur {
    struct uw_ctx ctx;
    u8 xmm[16][16];
    int xmm_ok, inited, at_end;
};
_Static_assert(sizeof(struct unw_cur) <= 140 * 8, "cursor maior que o da Apple");

struct cx_dl {
    const char *fname; void *fbase; const char *sname; void *saddr;
};
__attribute__((weak)) int dladdr(const void *addr, void *info);

static int addr_space_tag;
void *unw_local_addr_space = &addr_space_tag;

__attribute__((naked)) int __unw_getcontext(void *ctx) {
    __asm__ volatile(
        "movq %rax, 0(%rdi)\n\t"
        "movq %rdx, 8(%rdi)\n\t"
        "movq %rcx, 16(%rdi)\n\t"
        "movq %rbx, 24(%rdi)\n\t"
        "movq %rsi, 32(%rdi)\n\t"
        "movq %rdi, 40(%rdi)\n\t"
        "movq %rbp, 48(%rdi)\n\t"
        "leaq 8(%rsp), %rax\n\t"
        "movq %rax, 56(%rdi)\n\t"
        "movq %r8, 64(%rdi)\n\t"
        "movq %r9, 72(%rdi)\n\t"
        "movq %r10, 80(%rdi)\n\t"
        "movq %r11, 88(%rdi)\n\t"
        "movq %r12, 96(%rdi)\n\t"
        "movq %r13, 104(%rdi)\n\t"
        "movq %r14, 112(%rdi)\n\t"
        "movq %r15, 120(%rdi)\n\t"
        "movq (%rsp), %rax\n\t"
        "movq %rax, 128(%rdi)\n\t"
        "movdqu %xmm0, 136(%rdi)\n\t"
        "movdqu %xmm1, 152(%rdi)\n\t"
        "movdqu %xmm2, 168(%rdi)\n\t"
        "movdqu %xmm3, 184(%rdi)\n\t"
        "movdqu %xmm4, 200(%rdi)\n\t"
        "movdqu %xmm5, 216(%rdi)\n\t"
        "movdqu %xmm6, 232(%rdi)\n\t"
        "movdqu %xmm7, 248(%rdi)\n\t"
        "movdqu %xmm8, 264(%rdi)\n\t"
        "movdqu %xmm9, 280(%rdi)\n\t"
        "movdqu %xmm10, 296(%rdi)\n\t"
        "movdqu %xmm11, 312(%rdi)\n\t"
        "movdqu %xmm12, 328(%rdi)\n\t"
        "movdqu %xmm13, 344(%rdi)\n\t"
        "movdqu %xmm14, 360(%rdi)\n\t"
        "movdqu %xmm15, 376(%rdi)\n\t"
        "xorl %eax, %eax\n\t"
        "ret\n\t");
}

static int gpr_index(int reg) {
    if (reg == UNW_REG_IP) return 16;
    if (reg == UNW_REG_SP) return 7;
    if (reg >= 0 && reg <= 16) return reg;
    return -1;
}

int unw_init_local(void *cursor, void *context) {
    if (!cursor || !context) return UNW_EINVAL;
    struct unw_cur *u = cursor;
    memset(u, 0, sizeof *u);
    memcpy(u->ctx.r, context, sizeof u->ctx.r);
    memcpy(u->xmm, (u8 *)context + 17 * 8, sizeof u->xmm);
    u->xmm_ok = 1;
    u->inited = 1;
    load_frame(&u->ctx);
    return UNW_ESUCCESS;
}

int unw_init_local2(void *cursor, void *context, int flags) {
    (void)flags;
    return unw_init_local(cursor, context);
}

int unw_step(void *cursor) {
    struct unw_cur *u = cursor;
    if (!u || !u->inited) return UNW_EINVAL;
    if (u->at_end) return 0;
    u->xmm_ok = 0;
    u->ctx.fx = 0;
    u64 ip0 = u->ctx.r[16];
    if (!step_frame(&u->ctx) || !u->ctx.r[16] || u->ctx.r[16] == ip0) {
        u->at_end = 1;
        return 0;
    }
    const u8 *fx = u->ctx.fx;
    if (!load_frame(&u->ctx)) {
        u->at_end = 1;
        return 0;
    }
    if (fx) u->ctx.fx = fx;
    return 1;
}

int unw_get_reg(void *cursor, int reg, u64 *val) {
    struct unw_cur *u = cursor;
    if (!u || !u->inited || !val) return UNW_EINVAL;
    int i = gpr_index(reg);
    if (i < 0) return UNW_EBADREG;
    *val = u->ctx.r[i];
    return UNW_ESUCCESS;
}

int unw_set_reg(void *cursor, int reg, u64 val) {
    struct unw_cur *u = cursor;
    if (!u || !u->inited) return UNW_EINVAL;
    int i = gpr_index(reg);
    if (i < 0) return UNW_EBADREG;
    u->ctx.r[i] = val;
    return UNW_ESUCCESS;
}

int unw_is_fpreg(void *cursor, int reg) {
    (void)cursor;
    return reg >= UNW_X86_64_XMM0 && reg < UNW_X86_64_XMM0 + 16;
}

int unw_get_fpreg(void *cursor, int reg, double *val) {
    struct unw_cur *u = cursor;
    if (!u || !u->inited || !val) return UNW_EINVAL;
    if (!unw_is_fpreg(cursor, reg)) return UNW_EBADREG;
    int n = reg - UNW_X86_64_XMM0;
    if (u->xmm_ok) {
        memcpy(val, u->xmm[n], sizeof *val);
        return UNW_ESUCCESS;
    }
    if (u->ctx.fx) {
        memcpy(val, u->ctx.fx + 160 + n * 16, sizeof *val);
        return UNW_ESUCCESS;
    }
    return UNW_ENOINFO;
}

int unw_set_fpreg(void *cursor, int reg, double val) {
    struct unw_cur *u = cursor;
    if (!u || !u->inited) return UNW_EINVAL;
    if (!unw_is_fpreg(cursor, reg)) return UNW_EBADREG;
    int n = reg - UNW_X86_64_XMM0;
    memset(u->xmm[n], 0, 16);
    memcpy(u->xmm[n], &val, sizeof val);
    u->xmm_ok = 1;
    return UNW_ESUCCESS;
}

int unw_resume(void *cursor) {
    struct unw_cur *u = cursor;
    if (!u || !u->inited || !u->ctx.r[16]) return UNW_EUNSPEC;
    cx_uw_install(u->ctx.r);
    return UNW_EUNSPEC;
}

int unw_get_proc_info(void *cursor, void *info) {
    struct unw_cur *u = cursor;
    if (!u || !u->inited || !info) return UNW_EINVAL;
    if (!u->ctx.r[16]) return UNW_ENOINFO;
    u64 *w = info;
    /* start, end, lsda, handler, gp, flags, depois format/size (uint32) e unwind_info/extra */
    w[0] = u->ctx.region;
    w[1] = u->ctx.func_end;
    w[2] = (u64)u->ctx.lsda;
    w[3] = (u64)(uintptr_t)u->ctx.pers;
    w[4] = 0;
    w[5] = 0;
    u32 *fmt = (u32 *)(w + 6);
    fmt[0] = u->ctx.have_compact ? u->ctx.compact_enc : 0;
    fmt[1] = u->ctx.fde ? 1 : 0;
    u64 *tail = (u64 *)(fmt + 2);
    tail[0] = u->ctx.fde ? (u64)u->ctx.fde : 0;
    tail[1] = u->ctx.text_base;
    return u->ctx.region ? UNW_ESUCCESS : UNW_ENOINFO;
}

int unw_get_proc_name(void *cursor, char *buf, size_t len, u64 *off) {
    struct unw_cur *u = cursor;
    if (!u || !u->inited) return UNW_EINVAL;
    if (buf && len) buf[0] = 0;
    if (!dladdr || !u->ctx.r[16]) return UNW_ENOINFO;
    struct cx_dl inf;
    memset(&inf, 0, sizeof inf);
    u64 ip = u->ctx.r[16] - 1;
    if (!dladdr((void *)ip, &inf) || !inf.sname) return UNW_ENOINFO;
    if (buf && len) {
        size_t n = 0;
        while (inf.sname[n] && n + 1 < len) { buf[n] = inf.sname[n]; n++; }
        buf[n] = 0;
    }
    if (off) *off = inf.saddr ? ip - (u64)inf.saddr : 0;
    return UNW_ESUCCESS;
}

int unw_is_signal_frame(void *cursor) {
    struct unw_cur *u = cursor;
    if (!u || !u->inited) return UNW_EINVAL;
    return u->ctx.is_sig ? 1 : 0;
}

const char *unw_regname(void *cursor, int reg) {
    (void)cursor;
    static const char *gpr[] = {
        "rax", "rdx", "rcx", "rbx", "rsi", "rdi", "rbp", "rsp",
        "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15", "rip"
    };
    if (reg == UNW_REG_IP) return "ip";
    if (reg == UNW_REG_SP) return "sp";
    if (reg >= 0 && reg <= 16) return gpr[reg];
    return "???";
}
#endif
