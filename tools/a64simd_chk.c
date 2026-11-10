/*
 * Conferência no Linux do JIT de AdvSIMD/FP contra o interpretador (oráculo interno).
 * Cada caso é uma instrução seguida de BRK. Os dois motores rodam em cópias do mesmo
 * a64_cpu. O harness oficial (tools/a64test.c) é Windows e não roda nesta VM.
 *
 * Compilar:
 *   gcc -std=gnu11 -O2 -g -fno-strict-aliasing -mcx16 -DA64_HAVE_JIT -I userland/a64 \
 *       -o a64simd_chk tools/a64simd_chk.c userland/a64/*.c
 */
#define _GNU_SOURCE
#include "a64.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/mman.h>

static const char *cur_name;
static void on_sig(int sig) {
    const char *s = sig == SIGSEGV ? "SIGSEGV" : sig == SIGILL ? "SIGILL" : "sinal";
    fprintf(stderr, "falha fatal (%s) em %s\n", s, cur_name ? cur_name : "?");
    _exit(128 + sig);
}

void *a64_host_jit_alloc(a64_u64 n) {
    void *p = mmap(NULL, (size_t)n, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return p == MAP_FAILED ? NULL : p;
}
int a64_host_jit_wprotect(a64_u64 page, int protect) { (void)page; (void)protect; return 0; }
a64_u64 a64_host_cntvct(void) { return 0; }
void a64_host_icache_inval(a64_cpu *c, a64_u64 addr, a64_u64 len) { (void)c; (void)addr; (void)len; }
void a64_host_yield(a64_cpu *c) { (void)c; }

static a64_u32 *code;
static a64_cpu IA, JA;
static int ncase, nbad, nnat_ok, nnat_bad, nshown;

static a64_u32 fbs_key(a64_u32 insn) {
    a64_u32 msk = 0xFFE0FC00u;
    if (((insn >> 25) & 7) == 7 && (insn & (1u << 21)) &&
        (((insn & 0x7F20FC00u) == 0x1E200000u) || ((insn & 0x5F207C00u) == 0x1E204000u) ||
         ((insn & 0x0C00u) == 0x0800u)))
        msk = 0xFFFFFC00u;
    return insn & msk;
}
static a64_u32 dk[9000];
static a64_u64 dn[9000];
static int nd;
static void on_dump(a64_u32 key, a64_u64 count, void *arg) {
    (void)arg;
    if (nd < 9000) { dk[nd] = key; dn[nd] = count; nd++; }
}
static a64_u64 fbs_count(a64_u32 key) {
    nd = 0;
    a64_jit_fbstat_dump(on_dump, NULL, 9000);
    for (int i = 0; i < nd; i++) if (dk[i] == key) return dn[i];
    return 0;
}

static void prep(a64_u32 fpcr) {
    memset(&IA, 0, sizeof IA);
    IA.fpcr = fpcr;
    IA.pc = (a64_u64)(uintptr_t)code;
}

static void show(const char *name, a64_u32 insn) {
    if (nshown >= 24) return;
    nshown++;
    fprintf(stderr, "DIVERGE %s insn=%08x exit i=%u j=%u fpsr i=%08x j=%08x x0 i=%llx j=%llx\n",
            name, insn, IA.exit_reason, JA.exit_reason, IA.fpsr, JA.fpsr,
            (unsigned long long)IA.x[0], (unsigned long long)JA.x[0]);
    fprintf(stderr, "  Vi d0=%016llx %016llx  Vj d0=%016llx %016llx\n",
            (unsigned long long)IA.v[0].d[0], (unsigned long long)IA.v[0].d[1],
            (unsigned long long)JA.v[0].d[0], (unsigned long long)JA.v[0].d[1]);
    fprintf(stderr, "  Vi d1=%016llx %016llx  Vj d1=%016llx %016llx\n",
            (unsigned long long)IA.v[1].d[0], (unsigned long long)IA.v[1].d[1],
            (unsigned long long)JA.v[1].d[0], (unsigned long long)JA.v[1].d[1]);
}

/* Roda a instrução. native=1 exige que o fbstat dessa chave não suba (só o BRK cai). */
static void run1(const char *name, a64_u32 insn, int native) {
    cur_name = name;
    ncase++;
    code[0] = insn;
    code[1] = 0xD4200000u;
    a64_invalidate((a64_u64)(uintptr_t)code, 8);
    memcpy(&JA, &IA, sizeof IA);
    a64_u64 before = fbs_count(fbs_key(insn));
    int ri = a64_interp_run(&IA, 4);
    int rj = a64_jit_run(&JA);
    (void)ri; (void)rj;
    int same = memcmp(IA.x, JA.x, sizeof IA.x) == 0 && memcmp(IA.v, JA.v, sizeof IA.v) == 0 &&
               IA.nzcv == JA.nzcv && IA.fpcr == JA.fpcr && IA.fpsr == JA.fpsr &&
               IA.pc == JA.pc && IA.exit_reason == JA.exit_reason;
    int brk = IA.exit_reason == A64_EXIT_BRK && JA.exit_reason == A64_EXIT_BRK;
    a64_u64 after = fbs_count(fbs_key(insn));
    int fell = after != before;
    if (!same || !brk) { nbad++; show(name, insn); }
    if (native && brk && same) {
        if (fell) { nnat_bad++; if (nshown < 24) { nshown++; fprintf(stderr, "CAIU %s insn=%08x\n", name, insn); } }
        else nnat_ok++;
    }
}

int main(void) {
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_sig;
    sigaction(SIGSEGV, &sa, NULL);
    sigaction(SIGILL, &sa, NULL);
    sigaction(SIGFPE, &sa, NULL);
    alarm(60);
    code = mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (code == MAP_FAILED) { fprintf(stderr, "mmap\n"); return 1; }
    a64_jit_fbstat(1);

    /* --- deslocamento imediato de 64 bits (o maior fallback do WhatsApp) --- */
    static const a64_u64 lanes[] = {
        0, 1, 0xffffffffffffffffull, 0x8000000000000000ull, 0x7fffffffffffffffull,
        0x8000000000000001ull, 5, (a64_u64)-5, 0x0000000100000000ull, 0x4000000000000000ull
    };
    static const struct { const char *n; a64_u32 w; int dst, src; } sh[] = {
        { "sshr2d1", 0x4f7f0420u, 0, 1 }, { "sshr2d63", 0x4f410420u, 0, 1 }, { "sshr2d64", 0x4f400420u, 0, 1 },
        { "srshr2d2", 0x4f7e2420u, 0, 1 }, { "srsra2d64", 0x4f403420u, 0, 1 }, { "ushr2d7", 0x6f790420u, 0, 1 },
        { "sshrd1", 0x5f7f0420u, 0, 1 }, { "sshrd64", 0x5f400420u, 0, 1 }, { "ssrad3", 0x5f7d1420u, 0, 1 },
    };
    for (unsigned s = 0; s < sizeof sh / sizeof sh[0]; s++) {
        for (unsigned i = 0; i < sizeof lanes / sizeof lanes[0]; i++) {
            prep(0);
            IA.v[sh[s].src].d[0] = lanes[i];
            IA.v[sh[s].src].d[1] = lanes[(i + 3) % (sizeof lanes / sizeof lanes[0])];
            IA.v[sh[s].dst].d[0] = 0x1111111111111111ull;
            IA.v[sh[s].dst].d[1] = 0x2222222222222222ull;
            run1(sh[s].n, sh[s].w, 1);
        }
    }
    prep(0);
    IA.v[3].d[0] = 0x8000000000000000ull; IA.v[3].d[1] = 9;
    IA.v[2].d[0] = 3; IA.v[2].d[1] = 0xffffffffffffffffull;
    run1("ssra2d4", 0x4f7c1462u, 1);

    /* --- USHL / SSHL, uma contagem diferente por lane --- */
    static const a64_u8 counts[] = { 0, 1, 7, 8, 15, 16, 31, 32, 63, 64, 127, 0xFF, 0xF8, 0xC1, 0xC0, 0x80 };
    static const a64_u8 values[] = { 0, 1, 0x7F, 0x80, 0xFF, 0x0F, 0xF0, 0x55, 0xAA, 2, 0x40, 0xC0, 3, 0x01, 0xFE, 0x10 };
    prep(0);
    for (int i = 0; i < 16; i++) { IA.v[1].b[i] = values[i]; IA.v[2].b[i] = counts[i]; }
    run1("sshl16b", 0x4e224420u, 1);
    prep(0);
    for (int i = 0; i < 8; i++) { IA.v[1].b[i] = values[i]; IA.v[2].b[i] = counts[i]; }
    run1("ushl8b", 0x2e224420u, 1);
    prep(0);
    for (int i = 0; i < 8; i++) {
        IA.v[1].h[i] = (a64_u16)(0x8000u + values[i] * 16);
        IA.v[2].h[i] = counts[i];
    }
    run1("sshl8h", 0x4e624420u, 1);
    prep(0);
    a64_u32 sv[4] = { 0x80000000u, 0x7fffffffu, 1, 0xffffffffu };
    a64_u32 sc[4] = { 1, 31, 0xFFu, 32 };
    for (int i = 0; i < 4; i++) { IA.v[1].s[i] = sv[i]; IA.v[2].s[i] = sc[i]; }
    run1("ushl4s", 0x6ea24420u, 1);
    prep(0);
    IA.v[1].d[0] = 0x8000000000000000ull; IA.v[1].d[1] = 0xffffffffffffffffull;
    IA.v[2].d[0] = 1; IA.v[2].d[1] = (a64_u64)(a64_u8)-4;
    run1("ushl2d", 0x6ee24420u, 1);
    prep(0);
    IA.v[1].d[0] = 0x4000000000000001ull; IA.v[1].d[1] = 5;
    IA.v[2].d[0] = 63; IA.v[2].d[1] = (a64_u64)(a64_u8)-63;
    run1("sshl2d", 0x4ee24420u, 1);
    prep(0);
    IA.v[1].d[0] = 0x8000000000000001ull; IA.v[2].d[0] = (a64_u64)(a64_u8)-1;
    run1("ushld", 0x7ee24420u, 1);
    prep(0);
    IA.v[1].d[0] = 0x8000000000000000ull; IA.v[2].d[0] = 1;
    run1("sshld", 0x5ee24420u, 1);

    /* --- CNT, REV, ABS, NEG, pares --- */
    prep(0);
    for (int i = 0; i < 16; i++) IA.v[1].b[i] = (a64_u8)(i * 17);
    run1("cnt16", 0x4e205820u, 1);
    prep(0);
    for (int i = 0; i < 8; i++) IA.v[1].b[i] = (a64_u8)(0xF0 | i);
    run1("cnt8", 0x0e205820u, 1);
    static const a64_u32 revs[] = { 0x4e200820u, 0x4ea00820u, 0x4e600820u, 0x6e200820u, 0x6e600820u, 0x4e201820u };
    static const char *revn[] = { "rev64b", "rev64s", "rev64h", "rev32b", "rev32h", "rev16b" };
    for (int r = 0; r < 6; r++) {
        prep(0);
        for (int i = 0; i < 16; i++) IA.v[1].b[i] = (a64_u8)i;
        run1(revn[r], revs[r], 1);
    }
    prep(0);
    for (int i = 0; i < 16; i++) IA.v[1].b[i] = (a64_u8)(0x70 + i);
    run1("abs16", 0x4e20b820u, 1);
    prep(0);
    for (int i = 0; i < 8; i++) IA.v[1].h[i] = (a64_u16)(0x7FF0u + i * 3);
    run1("abs8h", 0x4e60b820u, 1);
    prep(0);
    IA.v[1].s[0] = 0x80000000u; IA.v[1].s[1] = 1; IA.v[1].s[2] = 0xffffffffu; IA.v[1].s[3] = 0;
    run1("abs4s", 0x4ea0b820u, 1);
    prep(0);
    IA.v[1].d[0] = 0x8000000000000000ull; IA.v[1].d[1] = 0xfffffffffffffffeull;
    run1("abs2d", 0x4ee0b820u, 1);
    prep(0);
    IA.v[1].d[0] = 0x8000000000000000ull; IA.v[1].d[1] = 0xdeadbeefcafebabeull;
    run1("absd", 0x5ee0b820u, 1);
    prep(0);
    IA.v[1].s[0] = 1; IA.v[1].s[1] = 0x80000000u; IA.v[1].s[2] = 0; IA.v[1].s[3] = 5;
    run1("neg4s", 0x6ea0b820u, 1);
    prep(0);
    a64_u32 a4[4] = { 1, 5, 0xffffffffu, 9 };
    a64_u32 b4[4] = { 3, 0, 2, 0x80000000u };
    for (int i = 0; i < 4; i++) { IA.v[1].s[i] = a4[i]; IA.v[2].s[i] = b4[i]; }
    run1("umaxp4s", 0x6ea2a420u, 1);
    run1("sminp4s", 0x4ea2ac20u, 1);
    prep(0);
    for (int i = 0; i < 16; i++) { IA.v[1].b[i] = (a64_u8)(i + 1); IA.v[2].b[i] = (a64_u8)(200 - i); }
    run1("uminp16b", 0x6e22ac20u, 1);
    prep(0);
    for (int i = 0; i < 8; i++) { IA.v[1].h[i] = (a64_u16)(0x8000 + i); IA.v[2].h[i] = (a64_u16)(i * 1000); }
    run1("smaxp8h", 0x4e62a420u, 1);
    prep(0);
    IA.v[1].s[0] = 4; IA.v[1].s[1] = 9; IA.v[2].s[0] = 1; IA.v[2].s[1] = 8;
    IA.v[1].s[2] = 0xFFFFFFFFu; IA.v[2].s[2] = 0xFFFFFFFFu; /* altos devem ser ignorados */
    run1("umaxp2s", 0x2ea2a420u, 1);
    prep(0);
    for (int i = 0; i < 8; i++) { IA.v[1].b[i] = (a64_u8)(0xF0 + i); IA.v[2].b[i] = (a64_u8)i; }
    run1("sminp8b", 0x0e22ac20u, 1);

    /* --- FCVT* para GPR: normais nativos; NaN, inf, denormal, FZ e fora da faixa só conferem estado --- */
    static const a64_u64 fpbits[] = {
        0, 0x8000000000000000ull, 0x3ff0000000000000ull, 0xbff0000000000000ull,
        0x3fe0000000000000ull, 0xbfe0000000000000ull, 0x3ff8000000000000ull, 0xbff8000000000000ull,
        0x4004000000000000ull, 0xc004000000000000ull, 0x3ff0000000000001ull,
        1, 0x7ff0000000000000ull, 0xfff0000000000000ull, 0x7ff8000000000001ull, 0x7ff0000000000001ull,
        0x43e0000000000000ull, 0xc3e0000000000000ull, 0x43d0000000000000ull, 0x4330000000000000ull
    };
    static const struct { const char *n; a64_u32 w; int sreg; int single; int uns; } cv[] = {
        { "fcvtns_xd", 0x9e600000u, 0, 0, 0 }, { "fcvtns_xs", 0x9e200000u, 0, 1, 0 },
        { "fcvtnu_xd", 0x9e610021u, 1, 0, 1 }, { "fcvtms_xd", 0x9e700000u, 0, 0, 0 },
        { "fcvtmu_ws", 0x1e310000u, 0, 1, 1 }, { "fcvtps_xd", 0x9e680000u, 0, 0, 0 },
        { "fcvtpu_xs", 0x9e290000u, 0, 1, 1 }, { "fcvtzs_xd", 0x9e780000u, 0, 0, 0 },
        { "fcvtzu_ws", 0x1e390000u, 0, 1, 1 }, { "fcvtas_xd", 0x9e640000u, 0, 0, 0 },
        { "fcvtau_wd", 0x1e650000u, 0, 0, 1 }, { "fcvtas_xs", 0x9e240062u, 3, 1, 0 },
    };
    for (unsigned c = 0; c < sizeof cv / sizeof cv[0]; c++) {
        for (unsigned i = 0; i < sizeof fpbits / sizeof fpbits[0]; i++) {
            int nasty = i >= 11;
            prep(0);
            if (cv[c].single) {
                static const a64_u32 sb[] = {
                    0, 0x80000000u, 0x3f800000u, 0xbf800000u, 0x3f000000u, 0xbf000000u,
                    0x3fc00000u, 0xbfc00000u, 0x40200000u, 0xc0200000u, 0x3f800001u,
                    1, 0x7f800000u, 0xff800000u, 0x7fc00001u, 0x7f800001u,
                    0x4f000000u, 0xcf000000u, 0x4e800000u, 0x4f000000u
                };
                IA.v[cv[c].sreg].s[0] = sb[i];
            } else IA.v[cv[c].sreg].d[0] = fpbits[i];
            int neg = cv[c].single ? (IA.v[cv[c].sreg].s[0] >> 31) : (int)(fpbits[i] >> 63);
            /* negativo sem sinal satura e levanta IOC: fica no interpretador */
            run1(cv[c].n, cv[c].w, !nasty && !(cv[c].uns && neg));
        }
        prep(A64_FPCR_FZ);
        IA.v[cv[c].sreg].d[0] = 0x3ff0000000000000ull;
        if (cv[c].single) IA.v[cv[c].sreg].s[0] = 0x3f800000u;
        run1(cv[c].n, cv[c].w, 0);
    }

    /* --- SCVTF / UCVTF SIMD e GPR --- */
    prep(0);
    IA.v[1].d[0] = 0x8000000000000001ull; IA.v[1].d[1] = 0xffffffffffffffffull;
    run1("ucvtf_d", 0x7e61d820u, 1);
    prep(0);
    IA.v[1].d[0] = (a64_u64)-3; IA.v[1].d[1] = 0x7fffffffffffffffull;
    run1("scvtf_d", 0x5e61d820u, 1);
    prep(0);
    IA.v[1].s[0] = 0xffffffffu; IA.v[1].s[1] = 0;
    run1("scvtf_s", 0x5e21d820u, 1);
    prep(0);
    IA.v[1].s[0] = 0x80000000u;
    run1("ucvtf_s", 0x7e21d820u, 1);
    prep(0);
    IA.v[1].d[0] = 1; IA.v[1].d[1] = 0x8000000000000000ull;
    run1("scvtf_2d", 0x4e61d820u, 1);
    run1("ucvtf_2d", 0x6e61d820u, 1);
    prep(0);
    IA.v[1].s[0] = 0x80000000u; IA.v[1].s[1] = 2; IA.v[1].s[2] = 0xffffffffu; IA.v[1].s[3] = 1000000;
    run1("scvtf_4s", 0x4e21d820u, 1);
    prep(0);
    IA.v[1].s[0] = 0xffffffffu; IA.v[1].s[1] = 0x80000000u;
    IA.v[1].s[2] = 0x11111111u;
    run1("ucvtf_2s", 0x2e21d820u, 1);
    prep(A64_FPCR_FZ | (1u << A64_FPCR_RMODE_SHIFT));
    IA.v[1].d[0] = 3;
    run1("scvtf_d_rp", 0x5e61d820u, 0);
    prep(0);
    IA.x[0] = 0x8000000000000001ull;
    run1("ucvtf_x", 0x9e630000u, 1);
    prep(0);
    IA.x[0] = (a64_u64)-7;
    run1("scvtf_x", 0x9e620000u, 1);

    /* --- FRINT escalar e vetorial, FABS/FNEG vetorial, FMAX/FMIN --- */
    static const struct { const char *n; a64_u32 w; int sreg; int single; int vec; } fr[] = {
        { "frintn_d", 0x1e644020u, 1, 0, 0 }, { "frintp_s", 0x1e24c020u, 1, 1, 0 },
        { "frintm_d", 0x1e654020u, 1, 0, 0 }, { "frintz_s", 0x1e25c020u, 1, 1, 0 },
        { "frinta_d", 0x1e664020u, 1, 0, 0 }, { "frintx_d", 0x1e674020u, 1, 0, 0 },
        { "frinti_s", 0x1e27c020u, 1, 1, 0 },
        { "frintn_2d", 0x4e618820u, 1, 0, 1 }, { "frintm_4s", 0x4e219820u, 1, 1, 1 },
        { "frintp_2d", 0x4ee18820u, 1, 0, 1 }, { "frintz_4s", 0x4ea19820u, 1, 1, 1 },
        { "frinti_2d", 0x6ee19820u, 1, 0, 1 }, { "frintn_4s", 0x4e218820u, 1, 1, 1 },
    };
    static const a64_u64 nice_d[] = {
        0x3ff0000000000000ull, 0xbff0000000000000ull, 0x3fe0000000000000ull, 0xbfe0000000000000ull,
        0x3ff8000000000000ull, 0x4004000000000000ull, 0xc004000000000000ull, 0x8000000000000000ull
    };
    static const a64_u32 nice_s[] = {
        0x3f800000u, 0xbf800000u, 0x3f000000u, 0xbf000000u, 0x3fc00000u, 0x40200000u, 0xc0200000u, 0x80000000u
    };
    for (unsigned f = 0; f < sizeof fr / sizeof fr[0]; f++) {
        prep(0);
        if (fr[f].single) {
            for (int i = 0; i < (fr[f].vec ? 4 : 1); i++) IA.v[fr[f].sreg].s[i] = nice_s[i];
        } else {
            IA.v[fr[f].sreg].d[0] = nice_d[f % 8];
            IA.v[fr[f].sreg].d[1] = nice_d[(f + 3) % 8];
        }
        run1(fr[f].n, fr[f].w, 1);
        prep((2u << A64_FPCR_RMODE_SHIFT)); /* RM: FRINTI/X seguem o FPCR; os outros não */
        if (fr[f].single) IA.v[fr[f].sreg].s[0] = 0x3fc00000u;
        else IA.v[fr[f].sreg].d[0] = 0x3ff8000000000000ull;
        run1(fr[f].n, fr[f].w, 1);
        prep(0);
        if (fr[f].single) IA.v[fr[f].sreg].s[0] = 0x7fc00000u;
        else IA.v[fr[f].sreg].d[0] = 0x7ff8000000000001ull;
        run1(fr[f].n, fr[f].w, 0);
        prep(A64_FPCR_FZ);
        if (fr[f].single) IA.v[fr[f].sreg].s[0] = 0x3f800000u;
        else IA.v[fr[f].sreg].d[0] = 0x3ff0000000000000ull;
        run1(fr[f].n, fr[f].w, 0);
    }
    prep(0);
    IA.v[1].d[0] = 0xbff0000000000000ull; IA.v[1].d[1] = 0x7ff0000000000000ull;
    run1("fabs_2d", 0x4ee0f820u, 1);
    prep(0);
    IA.v[1].s[0] = 0x3f800000u; IA.v[1].s[1] = 0xbf800000u; IA.v[1].s[2] = 0x7fc00000u; IA.v[1].s[3] = 0;
    run1("fneg_4s", 0x6ea0f820u, 1);
    run1("fabs_4s", 0x4ea0f820u, 1);
    prep(0);
    IA.v[1].d[0] = 0x3ff0000000000000ull; IA.v[1].d[1] = 0x8000000000000000ull;
    run1("fneg_2d", 0x6ee0f820u, 1);

    static const a64_u32 fm[] = { 0x1e624820u, 0x1e625820u, 0x1e226820u, 0x1e627820u };
    static const char *fmn[] = { "fmax_d", "fmin_d", "fmaxnm_s", "fminnm_d" };
    static const int fmsingle[] = { 0, 0, 1, 0 };
    for (int k = 0; k < 4; k++) {
        prep(0);
        if (fmsingle[k]) { IA.v[1].s[0] = 0x3f800000u; IA.v[2].s[0] = 0x40000000u; }
        else { IA.v[1].d[0] = 0x3ff0000000000000ull; IA.v[2].d[0] = 0x4000000000000000ull; }
        run1(fmn[k], fm[k], 1);
        prep(0);
        if (fmsingle[k]) { IA.v[1].s[0] = 0x80000000u; IA.v[2].s[0] = 0; }
        else { IA.v[1].d[0] = 0x8000000000000000ull; IA.v[2].d[0] = 0; }
        run1(fmn[k], fm[k], 1);
        prep(0);
        if (fmsingle[k]) { IA.v[1].s[0] = 0x7fc00000u; IA.v[2].s[0] = 0x3f800000u; }
        else { IA.v[1].d[0] = 0x7ff8000000000001ull; IA.v[2].d[0] = 0x3ff0000000000000ull; }
        run1(fmn[k], fm[k], 0);
    }
    prep(0);
    IA.v[1].d[0] = 0x3ff0000000000000ull; IA.v[1].d[1] = 0x4000000000000000ull;
    IA.v[2].d[0] = 0x4008000000000000ull; IA.v[2].d[1] = 0x3ff0000000000000ull;
    run1("fadd_2d", 0x4e62d420u, 1);

    printf("casos=%d divergencias=%d nativos_ok=%d nativos_que_cairam=%d\n", ncase, nbad, nnat_ok, nnat_bad);
    return (nbad || nnat_bad) ? 1 : 0;
}
