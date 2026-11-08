/*
 * mOS — harness do tradutor arm64 no Windows (sem QEMU, sem mOS).
 *   a64test [--jit] [--trace] [--budget N] [--argv0 NOME] <programa.elf> [args...]
 *   a64test [--jit] [--budget N] --fuzz <lote.bin> <saida.bin>
 *
 * Modo ELF: carrega um ELF64 AArch64 estático (PT_LOAD nos endereços pedidos), monta a pilha num
 * endereço FIXO (tools/a64oracle/fz.h: o oráculo QEMU usa o mesmo layout, então sp/argv batem) e
 * roda com o motor do tradutor. Syscalls de teste:
 *   svc #0x80 com x16 (numeração do Darwin): 1 exit(x0)  4 write(fd=x0, buf=x1, len=x2) → x0
 *     20 getpid → 1   0x4000 dump
 *   svc #0x4001 = dump e svc #0x4002 = exit(0), sem usar registradores (testes de regressão)
 *   dump: imprime x0..x30, sp, nzcv, fpcr, fpsr, v0..v31 em formato fixo (o oráculo imprime igual)
 * Saídas anormais (mesmo texto e código no oráculo, ver tools/a64oracle/mon.c):
 *   UDF → "UDF <insn> pc=<pc>" + dump, 132   BRK → "BRK #<imm> pc=<pc>" + dump, 133
 *   HLT → "HLT pc=<pc>", 132                 FAULT → "FAULT pc=<pc> addr=<addr>" + dump, 139
 *   exceção do host fora de acesso à memória do guest (bug do motor) → "HOSTEXC ...", 140
 *
 * Modo --fuzz: roda cada caso de um lote (fz.h) — estado inicial, instruções na página de código
 * fixa, rascunho determinístico — e grava um fz_result por caso. Usado por tools/a64fuzz.c.
 */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <io.h>
#include <fcntl.h>
#include "../userland/a64/a64.h"
#include "a64oracle/fz.h"

static int g_trace;
static a64_cpu *g_cpu;
/* recuperação de falha: o contexto do host é capturado antes de entrar no motor e a VEH o
 * restaura (como um setjmp/longjmp, sem desenrolar SEH a partir da VEH) */
static CONTEXT g_resume __attribute__((aligned(16)));
static volatile int g_faulted;
#define GUEST_FAULTED() (g_faulted = 0, RtlCaptureContext(&g_resume), g_faulted)
static volatile int g_in_guest;
static volatile a64_u64 g_fault_addr;
static volatile DWORD g_fault_code;

a64_u64 a64_host_cntvct(void) {
    LARGE_INTEGER f, t;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&t);
    return (a64_u64)((double)t.QuadPart * (double)A64_CNTFRQ / (double)f.QuadPart);
}
void a64_host_icache_inval(a64_cpu *c, a64_u64 addr, a64_u64 len) { (void)c; a64_invalidate(addr, len); }
void a64_host_yield(a64_cpu *c) { (void)c; SwitchToThread(); }

static void dump(const a64_cpu *c) {
    for (int i = 0; i < 31; i++) printf("x%-2d %016llx%s", i, (unsigned long long)c->x[i], (i % 4 == 3) ? "\n" : "  ");
    printf("sp  %016llx\n", (unsigned long long)c->x[31]);
    printf("nzcv %08x fpcr %08x fpsr %08x\n", c->nzcv, c->fpcr, c->fpsr);
    for (int i = 0; i < 32; i++)
        printf("v%-2d %016llx%016llx%s", i, (unsigned long long)c->v[i].d[1], (unsigned long long)c->v[i].d[0], (i % 2 == 1) ? "\n" : "  ");
    fflush(stdout);
}

static LONG CALLBACK veh(EXCEPTION_POINTERS *e) {
    DWORD code = e->ExceptionRecord->ExceptionCode;
    if (!g_in_guest) return EXCEPTION_CONTINUE_SEARCH;
    if (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_DATATYPE_MISALIGNMENT ||
        code == EXCEPTION_IN_PAGE_ERROR || code == EXCEPTION_INT_DIVIDE_BY_ZERO ||
        code == EXCEPTION_INT_OVERFLOW || code == EXCEPTION_ILLEGAL_INSTRUCTION ||
        code == EXCEPTION_PRIV_INSTRUCTION || code == EXCEPTION_BREAKPOINT) {
        g_fault_code = code;
        g_fault_addr = (code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_IN_PAGE_ERROR) &&
                       e->ExceptionRecord->NumberParameters >= 2 ? e->ExceptionRecord->ExceptionInformation[1] : 0;
        CONTEXT *x = e->ContextRecord;
        if (g_cpu) {
            a64_hostctx h;
            memset(&h, 0, sizeof h);
            h.gpr[0] = x->Rax; h.gpr[1] = x->Rcx; h.gpr[2] = x->Rdx; h.gpr[3] = x->Rbx;
            h.gpr[4] = x->Rsp; h.gpr[5] = x->Rbp; h.gpr[6] = x->Rsi; h.gpr[7] = x->Rdi;
            h.gpr[8] = x->R8; h.gpr[9] = x->R9; h.gpr[10] = x->R10; h.gpr[11] = x->R11;
            h.gpr[12] = x->R12; h.gpr[13] = x->R13; h.gpr[14] = x->R14; h.gpr[15] = x->R15;
            h.rip = x->Rip; h.rflags = x->EFlags;
            memcpy(h.xmm, &x->Xmm0, sizeof h.xmm);
#ifdef A64_HAVE_JIT
            a64_jit_recover(g_cpu, &h);
#endif
        }
        g_in_guest = 0;
        g_faulted = 1;
        *x = g_resume;
        return EXCEPTION_CONTINUE_EXECUTION;
    }
    return EXCEPTION_CONTINUE_SEARCH;
}

static int map_fixed(a64_u64 va, a64_u64 len) {
    for (a64_u64 a = va & ~0xFFFFull; a < va + len; a += 0x10000) {
        MEMORY_BASIC_INFORMATION mbi;
        if (VirtualQuery((void *)a, &mbi, sizeof mbi) && mbi.State != MEM_FREE) continue;
        if (!VirtualAlloc((void *)a, 0x10000, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)) {
            fprintf(stderr, "a64test: VirtualAlloc(%llx) falhou (%lu)\n", (unsigned long long)a, GetLastError());
            return 0;
        }
    }
    return 1;
}

static int load_elf(const char *path, a64_u64 *entry) {
    FILE *f = fopen(path, "rb");
    if (!f) { fprintf(stderr, "a64test: não abriu %s\n", path); return 0; }
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *b = malloc(n);
    if (fread(b, 1, n, f) != (size_t)n) { fclose(f); return 0; }
    fclose(f);
    if (n < 64 || memcmp(b, "\x7f" "ELF", 4) || b[4] != 2 || *(a64_u16 *)(b + 18) != 183) {
        fprintf(stderr, "a64test: %s não é ELF64 AArch64\n", path); return 0;
    }
    *entry = *(a64_u64 *)(b + 24);
    a64_u64 phoff = *(a64_u64 *)(b + 32);
    a64_u16 phent = *(a64_u16 *)(b + 54), phnum = *(a64_u16 *)(b + 56);
    for (int i = 0; i < phnum; i++) {
        unsigned char *ph = b + phoff + (a64_u64)i * phent;
        if (*(a64_u32 *)ph != 1) continue;
        a64_u64 off = *(a64_u64 *)(ph + 8), va = *(a64_u64 *)(ph + 16);
        a64_u64 fsz = *(a64_u64 *)(ph + 32), msz = *(a64_u64 *)(ph + 40);
        if (va < FZ_IMG_LO || va + msz > FZ_STACK_VA + FZ_STACK_SZ) {
            fprintf(stderr, "a64test: PT_LOAD em %llx fora da faixa dos testes\n", (unsigned long long)va); return 0;
        }
        a64_u64 lo = va & ~0xFFFFull, hi = (va + msz + 0xFFFF) & ~0xFFFFull;
        if (!map_fixed(lo, hi - lo)) return 0;
        memcpy((void *)va, b + off, fsz);
        memset((void *)(va + fsz), 0, msz - fsz);
    }
    free(b);
    return 1;
}

static int handle_svc(a64_cpu *c, int *done) {
    if (c->exit_info == FZ_SVC_DUMP) { dump(c); return 0; }
    if (c->exit_info == FZ_SVC_EXIT0) { *done = 1; fflush(stdout); return 0; }
    switch (c->x[16]) {
    case 1: *done = 1; fflush(stdout); return (int)c->x[0];
    case 4: {
        fflush(stdout);
        DWORD w = 0;
        HANDLE h = GetStdHandle(c->x[0] == 2 ? STD_ERROR_HANDLE : STD_OUTPUT_HANDLE);
        WriteFile(h, (void *)c->x[1], (DWORD)c->x[2], &w, NULL);
        c->x[0] = w; c->nzcv &= ~A64_C;
        return 0;
    }
    case 20: c->x[0] = 1; c->nzcv &= ~A64_C; return 0;
    case 0x4000: dump(c); return 0;
    default:
        fflush(stdout);
        fprintf(stderr, "a64test: syscall %llu não suportada (pc=%llx)\n", (unsigned long long)c->x[16], (unsigned long long)c->pc);
        c->x[0] = 78; c->nzcv |= A64_C;   /* ENOSYS */
        return 0;
    }
}

/* ------------------------------------------------------------------------------------------ */
/* modo fuzz                                                                                   */

static volatile LONG g_watch_gen;       /* geração do caso corrente (o vigia só para o caso dele) */
static volatile LONG g_watch_busy;
static DWORD WINAPI watchdog(LPVOID p) {
    (void)p;
    LONG seen = -1; DWORD since = 0;
    for (;;) {
        Sleep(20);
        LONG g = g_watch_gen;
        if (!g_watch_busy) { seen = -1; continue; }
        if (g != seen) { seen = g; since = GetTickCount(); continue; }
        if (GetTickCount() - since > 300 && g_cpu) a64_request_stop(g_cpu);
    }
    return 0;
}

/* tudo o que estiver livre abaixo de 16 GiB vira reserva sem acesso: um ponteiro aleatório do
 * guest que caia ali falha como no oráculo, em vez de escrever na memória do harness */
static void reserve_low(void) {
    a64_u64 a = 0x10000;
    while (a < 0x400000000ull) {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQuery((void *)a, &mbi, sizeof mbi)) break;
        a64_u64 base = (a64_u64)mbi.BaseAddress, end = base + mbi.RegionSize;
        if (mbi.State == MEM_FREE) {
            a64_u64 lo = (a + 0xFFFF) & ~0xFFFFull, hi = end & ~0xFFFFull;
            if (hi > 0x400000000ull) hi = 0x400000000ull;
            if (hi > lo) VirtualAlloc((void *)lo, hi - lo, MEM_RESERVE, PAGE_NOACCESS);
        }
        a = end > a ? end : a + 0x10000;
    }
}

static void st_to_cpu(a64_cpu *c, const fz_state *s) {
    memset(c, 0, sizeof *c);
    memcpy(c->x, s->x, sizeof s->x);
    c->x[31] = s->sp;
    c->nzcv = s->nzcv & 0xF0000000u;
    c->fpcr = s->fpcr; c->fpsr = s->fpsr;
    c->pstate = s->pstate & (A64_PS_DIT | A64_PS_SSBS);
    c->tpidr_el0 = s->tpidr;
    memcpy(c->v, s->v, sizeof s->v);
}
static void cpu_to_st(fz_state *s, const a64_cpu *c) {
    memset(s, 0, sizeof *s);
    memcpy(s->x, c->x, sizeof s->x);
    s->sp = c->x[31];
    s->nzcv = c->nzcv; s->fpcr = c->fpcr; s->fpsr = c->fpsr;
    s->pstate = c->pstate & (A64_PS_DIT | A64_PS_SSBS);
    s->tpidr = c->tpidr_el0;
    memcpy(s->v, c->v, sizeof s->v);
}

static int fuzz_main(int engine, a64_u64 budget, const char *inpath, const char *outpath) {
    FILE *f = fopen(inpath, "rb");
    if (!f) { fprintf(stderr, "a64test: não abriu %s\n", inpath); return 2; }
    fz_batch_hdr hdr;
    if (fread(&hdr, sizeof hdr, 1, f) != 1 || hdr.magic != FZ_BATCH_MAGIC ||
        hdr.case_size != sizeof(fz_case) || hdr.res_size != sizeof(fz_result)) {
        fprintf(stderr, "a64test: lote inválido\n"); return 2;
    }
    fz_case *cases = malloc((size_t)hdr.ncases * sizeof(fz_case));
    fz_result *res = calloc(hdr.ncases ? hdr.ncases : 1, sizeof(fz_result));
    if (fread(cases, sizeof(fz_case), hdr.ncases, f) != hdr.ncases) { fprintf(stderr, "a64test: lote truncado\n"); return 2; }
    fclose(f);
    if (!budget) budget = hdr.budget ? hdr.budget : 100000;

    if (!VirtualAlloc((void *)FZ_CODE_VA, FZ_CODE_SZ, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE) ||
        !VirtualAlloc((void *)FZ_SCR_VA, 0x10000, MEM_RESERVE, PAGE_NOACCESS) ||
        !VirtualAlloc((void *)FZ_SCR_VA, FZ_SCR_SZ, MEM_COMMIT, PAGE_READWRITE)) {
        fprintf(stderr, "a64test: não mapeou a página de código/rascunho (%lu)\n", GetLastError()); return 2;
    }
    reserve_low();
    a64_cpu *c = _aligned_malloc(sizeof *c, 64);
    memset(c, 0, sizeof *c);
    g_cpu = c;
    if (engine == A64_ENGINE_JIT) CreateThread(NULL, 0, watchdog, NULL, 0, NULL);

    volatile a64_u32 *code = (volatile a64_u32 *)FZ_CODE_VA;
    for (unsigned i = 0; i < FZ_CODE_SZ / 4; i++) code[i] = FZ_TRAP_INSN;
    for (a64_u32 ci = 0; ci < hdr.ncases; ci++) {
        fz_case *k = &cases[ci];
        fz_result *r = &res[ci];
        unsigned n = k->ninsn > FZ_MAXINSN ? FZ_MAXINSN : k->ninsn;
        volatile a64_u32 *at = code + FZ_CASE_OFF / 4;
        for (unsigned i = 0; i < n; i++) at[i] = k->insn[i];
        at[n] = FZ_END_INSN;
        /* só as palavras do caso mudam (o resto da página é sempre brk #0xA64) */
        a64_invalidate(FZ_CODE_VA + FZ_CASE_OFF, 4ull * (n + 1));
        fz_fill((volatile a64_u64 *)FZ_SCR_VA, k->scr_seed);
        st_to_cpu(c, &k->st);
        c->pc = FZ_CODE_VA + FZ_CASE_OFF;
        int rr;
        g_fault_code = 0;
        if (GUEST_FAULTED()) {
            rr = A64_EXIT_FAULT;
            c->exit_info = g_fault_addr;
            c->exit_insn = 0;
        } else {
            g_in_guest = 1;
            if (engine == A64_ENGINE_JIT) {
                InterlockedIncrement(&g_watch_gen); g_watch_busy = 1;
                for (;;) {
                    rr = a64_run(c, engine);
                    if (rr == A64_EXIT_ICACHE) { a64_invalidate(c->exit_info & ~63ull, 64); continue; }
                    if (rr == A64_EXIT_YIELD) continue;
                    break;
                }
                g_watch_busy = 0;
            } else {
                rr = a64_interp_run(c, budget);
            }
            g_in_guest = 0;
        }
        g_watch_busy = 0;
        memset(r, 0, sizeof *r);
        r->pc = c->pc;
        r->insn = c->exit_insn;
        switch (rr) {
        case A64_EXIT_SVC:
            r->info = c->exit_info & 0xFFFF;
            r->kind = (r->info == FZ_SVC_DUMP && c->pc == FZ_CODE_VA + FZ_CASE_OFF + 4 * n + 4) ? FZK_END : FZK_SVC;
            break;
        case A64_EXIT_BRK: r->kind = FZK_BRK; r->info = c->exit_info & 0xFFFF; break;
        case A64_EXIT_UDF: r->kind = FZK_UDF; break;
        case A64_EXIT_HLT: r->kind = FZK_HLT; r->info = c->exit_info & 0xFFFF; break;
        case A64_EXIT_FAULT:
            r->kind = FZK_FAULT; r->info = c->exit_info;
            if (g_fault_code && g_fault_code != EXCEPTION_ACCESS_VIOLATION && g_fault_code != EXCEPTION_IN_PAGE_ERROR &&
                g_fault_code != EXCEPTION_DATATYPE_MISALIGNMENT) { r->kind = FZK_OTHER; r->info = 0x8000000000000000ull | g_fault_code; }
            if (r->kind == FZK_FAULT && !r->insn) { a64_u64 p = c->pc; if (p >= FZ_CODE_VA && p < FZ_CODE_VA + FZ_CODE_SZ) r->insn = *(a64_u32 *)p; }
            break;
        case A64_EXIT_STOP: r->kind = FZK_BUDGET; break;
        default: r->kind = FZK_OTHER; r->info = (a64_u64)rr; break;
        }
        cpu_to_st(&r->st, c);
        r->scr_hash = fz_hash((const volatile a64_u8 *)FZ_SCR_VA, FZ_SCR_SZ);
        for (unsigned i = 0; i <= n; i++) at[i] = FZ_TRAP_INSN;
        a64_invalidate(FZ_CODE_VA + FZ_CASE_OFF, 4ull * (n + 1));
    }
    FILE *o = fopen(outpath, "wb");
    if (!o) { fprintf(stderr, "a64test: não criou %s\n", outpath); return 2; }
    fwrite(res, sizeof(fz_result), hdr.ncases, o);
    fclose(o);
    return 0;
}

/* ------------------------------------------------------------------------------------------ */

int main(int argc, char **argv) {
    int engine = A64_ENGINE_INTERP, ai = 1;
    a64_u64 budget = 0;
    const char *argv0 = NULL, *fuzz_in = NULL, *fuzz_out = NULL;
    for (; ai < argc && argv[ai][0] == '-'; ai++) {
        if (!strcmp(argv[ai], "--jit")) engine = A64_ENGINE_JIT;
        else if (!strcmp(argv[ai], "--trace")) g_trace = 1;
        else if (!strcmp(argv[ai], "--budget") && ai + 1 < argc) budget = strtoull(argv[++ai], NULL, 0);
        else if (!strcmp(argv[ai], "--argv0") && ai + 1 < argc) argv0 = argv[++ai];
        else if (!strcmp(argv[ai], "--fuzz") && ai + 2 < argc) { fuzz_in = argv[++ai]; fuzz_out = argv[++ai]; }
        else { fprintf(stderr, "a64test: opção desconhecida %s\n", argv[ai]); return 2; }
    }
    /* saída crua: o dump do printf e o write do guest têm que sair só com LF, como no oráculo */
    _setmode(_fileno(stdout), _O_BINARY);
    _setmode(_fileno(stderr), _O_BINARY);
    AddVectoredExceptionHandler(1, veh);
    if (fuzz_in) return fuzz_main(engine, budget, fuzz_in, fuzz_out);
    if (ai >= argc) { fprintf(stderr, "uso: a64test [--jit] [--trace] [--budget N] [--argv0 NOME] programa.elf [args]\n"
                                      "     a64test [--jit] [--budget N] --fuzz lote.bin saida.bin\n"); return 2; }
    a64_u64 entry;
    if (!load_elf(argv[ai], &entry)) return 2;

    /* pilha de 8 MiB em endereço fixo; argc/argv no formato do kernel do Darwin para main(argc, argv) via x0/x1 */
    if (!VirtualAlloc((void *)FZ_STACK_VA, FZ_STACK_SZ, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE)) {
        fprintf(stderr, "a64test: não alocou a pilha em %llx (%lu)\n", (unsigned long long)FZ_STACK_VA, GetLastError()); return 2;
    }
    int gargc = argc - ai;
    const char **gv = malloc(sizeof(char *) * (gargc + 1));
    for (int i = 0; i < gargc; i++) gv[i] = argv[ai + i];
    if (argv0) gv[0] = argv0;
    a64_u64 gargv;
    a64_u64 sp = fz_stack_layout(FZ_STACK_VA, gargc, gv, &gargv);

    a64_cpu *c = _aligned_malloc(sizeof *c, 64);
    memset(c, 0, sizeof *c);
    c->pc = entry;
    c->x[31] = sp;
    c->x[0] = (a64_u64)gargc;
    c->x[1] = gargv;
    c->x[30] = 0;               /* retornar de _start sem exit → pc 0 → FAULT */
    g_cpu = c;

    int done = 0, status = 0;
    a64_u64 steps = 0;
    if (GUEST_FAULTED()) {
        fflush(stdout);
        if (g_fault_code != EXCEPTION_ACCESS_VIOLATION && g_fault_code != EXCEPTION_IN_PAGE_ERROR &&
            g_fault_code != EXCEPTION_DATATYPE_MISALIGNMENT) {
            fprintf(stderr, "HOSTEXC %08lx pc=%016llx\n", (unsigned long)g_fault_code, (unsigned long long)c->pc);
            dump(c);
            return 140;
        }
        fprintf(stderr, "FAULT pc=%016llx addr=%016llx\n", (unsigned long long)c->pc, (unsigned long long)g_fault_addr);
        dump(c);
        return 139;
    }
    while (!done) {
        int r;
        g_in_guest = 1;
        if (g_trace || budget) {
            if (g_trace) fprintf(stderr, "%016llx: %08x\n", (unsigned long long)c->pc, *(a64_u32 *)c->pc);
            r = a64_interp_step(c);
            if (r <= A64_BRANCH) { if (budget && ++steps >= budget) { g_in_guest = 0; fprintf(stderr, "a64test: orçamento esgotado\n"); dump(c); return 3; } continue; }
            if (r == A64_EXIT_ICACHE) { a64_invalidate(c->exit_info & ~63ull, 64); continue; }
            if (r == A64_EXIT_YIELD) continue;
        } else {
            r = a64_run(c, engine);
        }
        g_in_guest = 0;
        if (r != A64_EXIT_SVC) fflush(stdout);
        switch (r) {
        case A64_EXIT_SVC: status = handle_svc(c, &done); break;
        case A64_EXIT_BRK: fprintf(stderr, "BRK #%llx pc=%016llx\n", (unsigned long long)c->exit_info, (unsigned long long)c->pc); dump(c); return 133;
        case A64_EXIT_UDF: fprintf(stderr, "UDF %08x pc=%016llx\n", c->exit_insn, (unsigned long long)c->pc); dump(c); return 132;
        case A64_EXIT_HLT: fprintf(stderr, "HLT pc=%016llx\n", (unsigned long long)c->pc); return 132;
        case A64_EXIT_FAULT: fprintf(stderr, "FAULT pc=%016llx addr=%016llx\n", (unsigned long long)c->pc, (unsigned long long)c->exit_info); dump(c); return 139;
        case A64_EXIT_STOP: break;
        case A64_EXIT_ICACHE: a64_invalidate(c->exit_info & ~63ull, 64); break;
        case A64_EXIT_YIELD: break;
        default: fprintf(stderr, "a64test: saída %d inesperada\n", r); return 4;
        }
    }
    return status;
}
