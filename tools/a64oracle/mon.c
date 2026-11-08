/*
 * mOS — oráculo do tradutor arm64: monitor bare-metal em EL1 para qemu-system-aarch64 -M virt.
 *
 * Roda o MESMO programa de teste do a64test em EL0 de verdade e reproduz exatamente a saída do
 * harness (tools/a64test.c): mesmos endereços (fz.h), mesmas syscalls, mesmo formato de dump,
 * mesmos textos e códigos de saída para UDF/BRK/HLT/FAULT. Com isso, a64cmp.ps1 compara byte a byte.
 *
 * Entrada: um blob carregado pelo QEMU em MON_BLOB (-device loader,force-raw=on):
 *   - ELF64 AArch64 estático → modo ELF (argv = {"prog"}; o a64cmp passa --argv0 prog ao a64test)
 *   - lote do fuzzer (fz_batch_hdr) → roda cada caso e grava os fz_result no arquivo hdr.outpath
 * E/S por semihosting (stdout/stderr/arquivos do host; QEMU com -semihosting-config target=native).
 *
 * Configuração de EL0 (escolhas que o tradutor também segue; ver docs/tradutor-arm64.md):
 *   - MMU 4 KiB, VA de 48 bits, TBI0 = 0 (endereço do guest = endereço do host, sem tag)
 *   - SCTLR_EL1 como o XNU (SCTLR_EL1_REQUIRED): SA0=1 (o QEMU não implementa a checagem), nAA=0, UCI=1,
 *     DZE=0 e UCT=0 (DC ZVA e CTR_EL0 caem aqui e são emulados com os valores do M1: bloco de
 *     64 B e FZ_CTR_EL0), nTWI=nTWE=0 (WFI/WFE caem aqui e continuam, como YIELD),
 *     EnIA/EnIB/EnDA/EnDB=0 (PAC "identidade": PAC* não mudam o ponteiro, AUT* sempre passam),
 *     UMA=0, EnRCTX=0, TSCXT=1, MSCEn=0 (MOPS não anunciado)
 *   - CPACR: FP/SIMD liberados, SVE/SME com trap (→ UDF)
 *   - CNTKCTL: CNTVCT/CNTPCT legíveis em EL0; timers de EL0 não
 */
#include <stdint.h>
#include "fz.h"

typedef uint64_t u64; typedef uint32_t u32; typedef uint8_t u8;

#define MON_BLOB     0x44000000ull
#define MON_BLOB_MAX 0x0C000000ull           /* até 192 MiB */
#define MON_PT_BASE  0x42000000ull           /* tabelas de página (16 MiB) */
#define MON_PT_END   0x43000000ull
#define MON_POOL     0x50000000ull           /* páginas físicas para o guest */
#define MON_POOL_END 0x70000000ull
#define MON_OUT      0x70000000ull           /* resultados do fuzz (256 MiB) */
#define MON_OUT_END  0x80000000ull
#define GICD         0x08000000ull
#define GICC         0x08010000ull

/* CTR_EL0 do Apple M1 (IminLine = DminLine = 64 B, L1Ip VIPT, CWG/ERG = 64 B; mesmo valor da frente INT) */
#ifndef FZ_CTR_EL0
#define FZ_CTR_EL0   0x0000000084448004ull
#endif

struct mon_ctx {
    u64 x[31], sp, elr, spsr, esr, far, fpcr, fpsr, tpidr, pad;
    u8 v[32][16];
} __attribute__((aligned(16)));
struct mon_ctx mon_ctx;

long semihost(long op, void *param);
__attribute__((noreturn)) void mon_resume(void);

#define SYSREG_R(r) ({ u64 _v; __asm__ volatile("mrs %0, " #r : "=r"(_v)); _v; })
#define SYSREG_W(r, v) __asm__ volatile("msr " #r ", %0" :: "r"((u64)(v)))
static inline void isb(void) { __asm__ volatile("isb" ::: "memory"); }
static inline void dsb(void) { __asm__ volatile("dsb sy" ::: "memory"); }
static inline void mmio_w32(u64 a, u32 v) { *(volatile u32 *)a = v; }
static inline u32 mmio_r32(u64 a) { return *(volatile u32 *)a; }

static void *memset_(void *d, int c, u64 n) { u8 *p = d; while (n--) *p++ = (u8)c; return d; }
static void *memcpy_(void *d, const void *s, u64 n) { u8 *p = d; const u8 *q = s; while (n--) *p++ = *q++; return d; }
/* o compilador pode emitir chamadas a memset/memcpy */
void *memset(void *d, int c, unsigned long n) { return memset_(d, c, n); }
void *memcpy(void *d, const void *s, unsigned long n) { return memcpy_(d, s, n); }

/* ---------- semihosting ---------- */
enum { SH_OPEN = 1, SH_CLOSE = 2, SH_WRITE = 5, SH_EXIT = 0x18 };
static long sh_out = -1, sh_err = -1;
static u64 sh_strlen(const char *s) { u64 n = 0; while (s[n]) n++; return n; }
static long sh_open(const char *name, long mode) {
    u64 p[3] = { (u64)(uintptr_t)name, (u64)mode, sh_strlen(name) };
    return semihost(SH_OPEN, p);
}
static void sh_write(long h, const void *b, u64 n) {
    while (n) {
        u64 p[3] = { (u64)h, (u64)(uintptr_t)b, n };
        long left = semihost(SH_WRITE, p);
        if (left < 0 || (u64)left >= n) return;
        b = (const u8 *)b + (n - (u64)left); n = (u64)left;
    }
}
static __attribute__((noreturn)) void sh_exit(long code) {
    u64 p[2] = { 0x20026, (u64)code };          /* ADP_Stopped_ApplicationExit */
    semihost(SH_EXIT, p);
    for (;;) __asm__ volatile("wfi");
}

/* ---------- saída formatada (buffer + flush) ---------- */
static char obuf[8192]; static u64 olen; static long ofd;
static void oflush(void) { if (olen) { sh_write(ofd, obuf, olen); olen = 0; } }
static void oc(char ch) { if (olen == sizeof obuf) oflush(); obuf[olen++] = ch; }
static void os(const char *s) { while (*s) oc(*s++); }
static void ohex(u64 v, int digits) { for (int i = digits - 1; i >= 0; i--) oc("0123456789abcdef"[(v >> (4 * i)) & 15]); }
static void ohexmin(u64 v) { int d = 1; while (d < 16 && (v >> (4 * d))) d++; ohex(v, d); }
static void odec(u64 v) { char t[24]; int n = 0; do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v); while (n) oc(t[--n]); }
static void oto(long fd) { if (ofd != fd) oflush(); ofd = fd; }

static void dump_ctx(void) {
    oto(sh_out);
    for (int i = 0; i < 31; i++) {
        oc('x'); odec((u64)i); if (i < 10) oc(' ');
        oc(' '); ohex(mon_ctx.x[i], 16); os((i % 4 == 3) ? "\n" : "  ");
    }
    os("sp  "); ohex(mon_ctx.sp, 16); oc('\n');
    os("nzcv "); ohex(mon_ctx.spsr & 0xF0000000u, 8); os(" fpcr "); ohex((u32)mon_ctx.fpcr, 8);
    os(" fpsr "); ohex((u32)mon_ctx.fpsr, 8); oc('\n');
    for (int i = 0; i < 32; i++) {
        u64 lo, hi;
        memcpy_(&lo, mon_ctx.v[i], 8); memcpy_(&hi, mon_ctx.v[i] + 8, 8);
        oc('v'); odec((u64)i); if (i < 10) oc(' ');
        oc(' '); ohex(hi, 16); ohex(lo, 16); os((i % 2 == 1) ? "\n" : "  ");
    }
    oflush();
}

__attribute__((noreturn)) void mon_bad(u64 esr, u64 elr, u64 far) {
    oto(sh_err); os("MONITOR: exceção em EL1 esr="); ohex(esr, 8); os(" elr="); ohex(elr, 16);
    os(" far="); ohex(far, 16); oc('\n'); oflush();
    sh_exit(250);
}

/* ---------- MMU ---------- */
#define PTE_VALID   1ull
#define PTE_TABLE   3ull
#define PTE_PAGE    3ull
#define PTE_AF      (1ull << 10)
#define PTE_SH_IS   (3ull << 8)
#define PTE_AP_EL0  (1ull << 6)               /* AP[2:1] = 01: EL1 e EL0 RW */
#define PTE_ATTR(i) ((u64)(i) << 2)
#define PTE_PXN     (1ull << 53)
#define PTE_UXN     (1ull << 54)
static u64 *l0;
static u64 pt_next = MON_PT_BASE, pool_next = MON_POOL;

static u64 *pt_alloc(void) {
    if (pt_next >= MON_PT_END) mon_bad(0xdead0001, 0, 0);
    u64 *p = (u64 *)(uintptr_t)pt_next; pt_next += 4096;
    memset_(p, 0, 4096); return p;
}
static u64 pool_page(void) {
    if (pool_next >= MON_POOL_END) mon_bad(0xdead0002, 0, 0);
    u64 p = pool_next; pool_next += 4096;
    memset_((void *)(uintptr_t)p, 0, 4096); return p;
}
static u64 *walk(u64 va, int create) {
    u64 *t = l0;
    for (int lvl = 0; lvl < 3; lvl++) {
        unsigned idx = (va >> (39 - 9 * lvl)) & 511;
        if (!(t[idx] & PTE_VALID)) {
            if (!create) return 0;
            t[idx] = (u64)(uintptr_t)pt_alloc() | PTE_TABLE;
        }
        t = (u64 *)(uintptr_t)(t[idx] & 0x0000FFFFFFFFF000ull);
    }
    return &t[(va >> 12) & 511];
}
/* mapeia [va, va+len) para EL0 RWX (EL1 RW, PXN) com páginas novas e zeradas */
static void map_user(u64 va, u64 len) {
    for (u64 a = va & ~0xFFFull; a < va + len; a += 4096) {
        u64 *e = walk(a, 1);
        if (*e & PTE_VALID) continue;
        *e = pool_page() | PTE_PAGE | PTE_AF | PTE_SH_IS | PTE_AP_EL0 | PTE_ATTR(1) | PTE_PXN;
    }
}
static void map_identity(void) {
    /* identidade: [0,1 GiB) dispositivo, [1,2 GiB) RAM normal; só EL1 */
    l0[0] = (u64)(uintptr_t)pt_alloc() | PTE_TABLE;
    u64 *l1 = (u64 *)(uintptr_t)(l0[0] & ~0xFFFull);
    l1[0] = 0x00000000ull | 1 | PTE_AF | PTE_ATTR(0) | PTE_PXN | PTE_UXN;
    l1[1] = 0x40000000ull | 1 | PTE_AF | PTE_SH_IS | PTE_ATTR(1) | PTE_UXN;
}
static void mmu_on(void) {
    SYSREG_W(mair_el1, 0xFF00ull);             /* attr0 = Device-nGnRnE, attr1 = Normal WB */
    u64 tcr = 16ull | (1ull << 8) | (1ull << 10) | (3ull << 12) | (0ull << 14)   /* T0SZ=16, WB, ISH, 4K */
            | (1ull << 23)                                                       /* EPD1 */
            | (2ull << 32);                                                      /* IPS = 40 bits */
    SYSREG_W(tcr_el1, tcr);
    SYSREG_W(ttbr0_el1, (u64)(uintptr_t)l0);
    isb();
    __asm__ volatile("tlbi vmalle1\n dsb ish" ::: "memory");
    isb();
    u64 sctlr = (1ull << 0) | (1ull << 2) | (1ull << 3) | (1ull << 4)                 /* M C SA SA0; nAA = 0 como no XNU */
              | (1ull << 11) | (1ull << 12) | (1ull << 20) | (1ull << 22) | (1ull << 23) /* EOS I TSCXT EIS SPAN */
              | (1ull << 26) | (1ull << 28) | (1ull << 29);                            /* UCI nTLSMD LSMAOE */
    SYSREG_W(sctlr_el1, sctlr);
    isb();
}

/* ---------- ELF ---------- */
static u64 load_elf(void) {
    const u8 *b = (const u8 *)(uintptr_t)MON_BLOB;
    u64 entry; memcpy_(&entry, b + 24, 8);
    u64 phoff; memcpy_(&phoff, b + 32, 8);
    uint16_t phent, phnum; memcpy_(&phent, b + 54, 2); memcpy_(&phnum, b + 56, 2);
    for (int pass = 0; pass < 1; pass++)
        for (int i = 0; i < phnum; i++) {
            const u8 *ph = b + phoff + (u64)i * phent;
            u32 type; memcpy_(&type, ph, 4);
            if (type != 1) continue;
            u64 off, va, fsz, msz;
            memcpy_(&off, ph + 8, 8); memcpy_(&va, ph + 16, 8); memcpy_(&fsz, ph + 32, 8); memcpy_(&msz, ph + 40, 8);
            if (va < FZ_IMG_LO || va + msz > FZ_STACK_VA + FZ_STACK_SZ) {
                oto(sh_err); os("a64test: PT_LOAD em "); ohexmin(va); os(" fora da faixa dos testes\n"); oflush(); sh_exit(2);
            }
            u64 lo = va & ~0xFFFFull, hi = (va + msz + 0xFFFF) & ~0xFFFFull;   /* mesmos blocos de 64 KiB do a64test */
            map_user(lo, hi - lo);
            (void)off; (void)fsz;
        }
    return entry;
}
static void copy_elf_segments(void) {
    const u8 *b = (const u8 *)(uintptr_t)MON_BLOB;
    u64 phoff; memcpy_(&phoff, b + 32, 8);
    uint16_t phent, phnum; memcpy_(&phent, b + 54, 2); memcpy_(&phnum, b + 56, 2);
    for (int i = 0; i < phnum; i++) {
        const u8 *ph = b + phoff + (u64)i * phent;
        u32 type; memcpy_(&type, ph, 4);
        if (type != 1) continue;
        u64 off, va, fsz, msz;
        memcpy_(&off, ph + 8, 8); memcpy_(&va, ph + 16, 8); memcpy_(&fsz, ph + 32, 8); memcpy_(&msz, ph + 40, 8);
        memcpy_((void *)(uintptr_t)va, b + off, fsz);
        memset_((void *)(uintptr_t)(va + fsz), 0, msz - fsz);
    }
}

/* ---------- execução em EL0 ---------- */
static int mode_fuzz;
static fz_batch_hdr *bh;
static fz_case *cases;
static fz_result *results;
static u32 cur, cur_n;
static u64 timer_ticks;

static u32 read_insn(u64 pc) { return *(volatile u32 *)(uintptr_t)pc; }
static int pc_in_code(u64 pc) { return pc >= FZ_CODE_VA && pc < FZ_CODE_VA + FZ_CODE_SZ; }

static void start_case(void);
static __attribute__((noreturn)) void finish_fuzz(void);

/* DC ZVA emulado com bloco de 64 B; devolve 0 se o endereço não é gravável em EL0 */
static int dczva(u64 va) {
    va &= ~63ull;
    __asm__ volatile("at s1e0w, %0" :: "r"(va));
    isb();
    u64 par = SYSREG_R(par_el1);
    if (par & 1) return 0;
    for (int i = 0; i < 8; i++) ((volatile u64 *)(uintptr_t)va)[i] = 0;
    return 1;
}

static void elf_report(int kind, u64 info, u32 insn) {
    oto(sh_err);
    switch (kind) {
    case FZK_UDF: os("UDF "); ohex(insn, 8); os(" pc="); ohex(mon_ctx.elr, 16); oc('\n'); oflush(); dump_ctx(); sh_exit(132);
    case FZK_BRK: os("BRK #"); ohexmin(info); os(" pc="); ohex(mon_ctx.elr, 16); oc('\n'); oflush(); dump_ctx(); sh_exit(133);
    case FZK_HLT: os("HLT pc="); ohex(mon_ctx.elr, 16); oc('\n'); oflush(); sh_exit(132);
    case FZK_FAULT: os("FAULT pc="); ohex(mon_ctx.elr, 16); os(" addr="); ohex(info, 16); oc('\n'); oflush(); dump_ctx(); sh_exit(139);
    default: os("OTHER esr="); ohex(info, 8); os(" pc="); ohex(mon_ctx.elr, 16); oc('\n'); oflush(); dump_ctx(); sh_exit(141);
    }
}

static void elf_svc(u32 imm) {
    struct mon_ctx *c = &mon_ctx;
    if (imm == FZ_SVC_DUMP) { dump_ctx(); return; }
    if (imm == FZ_SVC_EXIT0) { sh_exit(0); }
    switch (c->x[16]) {
    case 1: oflush(); sh_exit((long)(int)c->x[0]);
    case 4: {
        long h = c->x[0] == 2 ? sh_err : sh_out;
        oto(h); oflush();
        u64 n = (u32)c->x[2];
        /* o buffer do guest é acessível de EL1 (PAN = 0); confere o mapeamento antes */
        u64 ok = 1;
        for (u64 a = c->x[1] & ~0xFFFull; n && a < c->x[1] + n; a += 4096) {
            __asm__ volatile("at s1e0r, %0" :: "r"(a)); isb();
            if (SYSREG_R(par_el1) & 1) { ok = 0; break; }
        }
        if (ok) sh_write(h, (const void *)(uintptr_t)c->x[1], n);
        c->x[0] = ok ? n : 0; c->spsr &= ~(1ull << 29);
        return;
    }
    case 20: c->x[0] = 1; c->spsr &= ~(1ull << 29); return;
    case 0x4000: dump_ctx(); return;
    default:
        oto(sh_err); os("a64test: syscall "); odec(c->x[16]); os(" não suportada (pc="); ohexmin(c->elr); os(")\n"); oflush();
        c->x[0] = 78; c->spsr |= (1ull << 29);
        return;
    }
}

static void timer_off(void) { SYSREG_W(cntv_ctl_el0, 2); isb(); }

/* classifica a exceção vinda de EL0; devolve 1 se foi tratada e o guest continua */
static int classify(int vec, int *kind, u64 *info, u32 *insn) {
    struct mon_ctx *c = &mon_ctx;
    *info = 0; *insn = 0;
    if (vec == 1) {                     /* IRQ: só o timer do orçamento */
        u32 iar = mmio_r32(GICC + 0x0C);
        mmio_w32(GICC + 0x10, iar);
        timer_off();
        *kind = FZK_BUDGET; return 0;
    }
    if (vec != 0) { *kind = FZK_OTHER; *info = c->esr; return 0; }
    u32 ec = (u32)(c->esr >> 26) & 0x3F;
    u32 iss = (u32)c->esr & 0x1FFFFFF;
    switch (ec) {
    case 0x15: *kind = FZK_SVC; *info = iss & 0xFFFF; *insn = read_insn(c->elr - 4); return 0;
    case 0x3C: *kind = FZK_BRK; *info = iss & 0xFFFF; *insn = read_insn(c->elr); return 0;
    case 0x01: c->elr += 4; return 1;  /* WFI/WFE → YIELD */
    case 0x18: {
        u32 op0 = (iss >> 20) & 3, op2 = (iss >> 17) & 7, op1 = (iss >> 14) & 7, crn = (iss >> 10) & 15;
        u32 rt = (iss >> 5) & 31, crm = (iss >> 1) & 15, rd = iss & 1;
        if (op0 == 3 && op1 == 3 && crn == 0 && crm == 0 && op2 == 1 && rd) {   /* MRS CTR_EL0 */
            if (rt != 31) c->x[rt] = FZ_CTR_EL0;
            c->elr += 4; return 1;
        }
        if (op0 == 1 && op1 == 3 && crn == 7 && crm == 4 && op2 == 1 && !rd) {  /* DC ZVA */
            u64 va = rt == 31 ? 0 : c->x[rt];
            if (!dczva(va)) { *kind = FZK_FAULT; *info = va; *insn = read_insn(c->elr); return 0; }
            c->elr += 4; return 1;
        }
        *kind = FZK_UDF; *insn = read_insn(c->elr); return 0;
    }
    case 0x00: case 0x07: case 0x19: case 0x1D: {
        *insn = read_insn(c->elr);
        *kind = ((*insn & 0xFFE0001Fu) == 0xD4400000u) ? FZK_HLT : FZK_UDF;
        if (*kind == FZK_HLT) *info = (*insn >> 5) & 0xFFFF;
        return 0;
    }
    case 0x20: case 0x22:               /* instruction abort / PC desalinhado */
        *kind = FZK_FAULT; *info = c->far; return 0;
    case 0x24:
        *kind = FZK_FAULT; *info = (iss & (1u << 10)) ? 0 : c->far; *insn = read_insn(c->elr); return 0;
    case 0x26:                          /* SP desalinhado */
        *kind = FZK_FAULT; *info = c->sp; *insn = read_insn(c->elr); return 0;
    default:
        *kind = FZK_OTHER; *info = c->esr; if (pc_in_code(c->elr) || !mode_fuzz) *insn = read_insn(c->elr); return 0;
    }
}

__attribute__((noreturn)) void mon_exception(int vec) {
    int kind; u64 info; u32 insn;
    if (classify(vec, &kind, &info, &insn)) mon_resume();
    if (!mode_fuzz) {
        if (kind == FZK_SVC) { elf_svc((u32)info); mon_resume(); }
        if (kind == FZK_BUDGET) mon_resume();
        elf_report(kind, info, insn);
    }
    /* fuzz: grava o resultado do caso corrente */
    timer_off();
    struct mon_ctx *c = &mon_ctx;
    fz_result *r = &results[cur];
    memset_(r, 0, sizeof *r);
    r->pc = c->elr;
    r->info = info;
    r->insn = insn;
    r->kind = (u32)kind;
    if (kind == FZK_SVC && info == FZ_SVC_DUMP && c->elr == FZ_CODE_VA + FZ_CASE_OFF + 4ull * cur_n + 4) r->kind = FZK_END;
    if (kind == FZK_FAULT && !pc_in_code(c->elr)) r->insn = 0;
    if (kind == FZK_FAULT && pc_in_code(c->elr) && !r->insn) r->insn = read_insn(c->elr);
    memcpy_(r->st.x, c->x, sizeof r->st.x);
    r->st.sp = c->sp;
    r->st.nzcv = (u32)c->spsr & 0xF0000000u;
    r->st.fpcr = (u32)c->fpcr; r->st.fpsr = (u32)c->fpsr;
    r->st.pstate = (u32)c->spsr & ((1u << 24) | (1u << 12));
    r->st.tpidr = c->tpidr;
    memcpy_(r->st.v, c->v, sizeof r->st.v);
    r->scr_hash = fz_hash((const volatile u8 *)(uintptr_t)FZ_SCR_VA, FZ_SCR_SZ);
    volatile u32 *at = (volatile u32 *)(uintptr_t)(FZ_CODE_VA + FZ_CASE_OFF);
    for (u32 i = 0; i <= cur_n; i++) at[i] = FZ_TRAP_INSN;
    cur++;
    if (cur >= bh->ncases) finish_fuzz();
    start_case();
    for (;;) ;
}

static void start_case(void) {
    fz_case *k = &cases[cur];
    struct mon_ctx *c = &mon_ctx;
    cur_n = k->ninsn > FZ_MAXINSN ? FZ_MAXINSN : k->ninsn;
    volatile u32 *at = (volatile u32 *)(uintptr_t)(FZ_CODE_VA + FZ_CASE_OFF);
    for (u32 i = 0; i < cur_n; i++) at[i] = k->insn[i];
    at[cur_n] = FZ_END_INSN;
    fz_fill((volatile u64 *)(uintptr_t)FZ_SCR_VA, k->scr_seed);
    memcpy_(c->x, k->st.x, sizeof c->x);
    c->sp = k->st.sp;
    c->elr = FZ_CODE_VA + FZ_CASE_OFF;
    c->spsr = (k->st.nzcv & 0xF0000000u) | (k->st.pstate & ((1u << 24) | (1u << 12)));   /* EL0t, DAIF = 0 */
    c->fpcr = k->st.fpcr; c->fpsr = k->st.fpsr; c->tpidr = k->st.tpidr;
    memcpy_(c->v, k->st.v, sizeof c->v);
    __asm__ volatile("dsb ish\n ic iallu\n dsb ish\n clrex\n isb" ::: "memory");
    SYSREG_W(cntv_tval_el0, timer_ticks);
    SYSREG_W(cntv_ctl_el0, 1);
    isb();
    mon_resume();
}

static __attribute__((noreturn)) void finish_fuzz(void) {
    long h = sh_open(bh->outpath, 5);   /* "wb" */
    if (h < 0) { oto(sh_err); os("MONITOR: não abriu "); os(bh->outpath); oc('\n'); oflush(); sh_exit(3); }
    sh_write(h, results, (u64)bh->ncases * sizeof(fz_result));
    u64 p[1] = { (u64)h }; semihost(SH_CLOSE, p);
    sh_exit(0);
}

static void gic_init(void) {
    mmio_w32(GICD + 0x000, 1);                      /* GICD_CTLR */
    mmio_w32(GICD + 0x100, 1u << 27);               /* ISENABLER0: PPI 27 (timer virtual) */
    *(volatile u8 *)(uintptr_t)(GICD + 0x400 + 27) = 0x80;
    mmio_w32(GICC + 0x004, 0xF0);                   /* GICC_PMR */
    mmio_w32(GICC + 0x000, 1);                      /* GICC_CTLR */
}

void mon_main(void) {
    sh_out = sh_open(":tt", 4);
    sh_err = sh_open(":tt", 8);
    ofd = sh_out;
    l0 = pt_alloc();
    map_identity();
    SYSREG_W(cntkctl_el1, 3);                       /* EL0PCTEN | EL0VCTEN */
    SYSREG_W(tpidrro_el0, 0);
    SYSREG_W(tpidr_el0, 0);
    u64 magic; memcpy_(&magic, (const void *)(uintptr_t)MON_BLOB, 8);
    if (magic == FZ_BATCH_MAGIC) {
        mode_fuzz = 1;
        bh = (fz_batch_hdr *)(uintptr_t)MON_BLOB;
        if (bh->case_size != sizeof(fz_case) || bh->res_size != sizeof(fz_result) ||
            (u64)bh->ncases * sizeof(fz_result) > MON_OUT_END - MON_OUT) {
            oto(sh_err); os("MONITOR: lote inválido\n"); oflush(); sh_exit(2);
        }
        cases = (fz_case *)(uintptr_t)(MON_BLOB + sizeof(fz_batch_hdr));
        results = (fz_result *)(uintptr_t)MON_OUT;
        map_user(FZ_CODE_VA, FZ_CODE_SZ);
        map_user(FZ_SCR_VA, FZ_SCR_SZ);
        mmu_on();
        volatile u32 *code = (volatile u32 *)(uintptr_t)FZ_CODE_VA;
        for (u64 i = 0; i < FZ_CODE_SZ / 4; i++) code[i] = FZ_TRAP_INSN;
        gic_init();
        timer_ticks = SYSREG_R(cntfrq_el0) / 4;      /* 250 ms por caso */
        if (!bh->ncases) finish_fuzz();
        cur = 0;
        start_case();
    }
    if ((magic & 0xFFFFFFFFull) != 0x464C457Full) { oto(sh_err); os("MONITOR: blob não é ELF nem lote\n"); oflush(); sh_exit(2); }
    u64 entry = load_elf();
    map_user(FZ_STACK_VA, FZ_STACK_SZ);
    mmu_on();
    copy_elf_segments();
    static const char *const argv[1] = { "prog" };
    u64 gargv;
    u64 sp = fz_stack_layout(FZ_STACK_VA, 1, argv, &gargv);
    struct mon_ctx *c = &mon_ctx;
    memset_(c, 0, sizeof *c);
    c->x[0] = 1; c->x[1] = gargv; c->sp = sp; c->elr = entry; c->spsr = 0;
    __asm__ volatile("dsb ish\n ic iallu\n dsb ish\n clrex\n isb" ::: "memory");
    mon_resume();
}
