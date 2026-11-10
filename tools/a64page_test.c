/*
 * mOS — teste de host da página de 16 KiB do processo arm64 (docs/pagesize-arm64.md).
 *
 * Compila e roda no Linux, sem o kernel: as funções de userland/libsystem/a64page.h são as mesmas
 * que o libSystem arm64 chama, e o "kernel" abaixo reproduz o cursor do SYS_MMAP/SYS_MUNMAP
 * (kernel/proc/process.c, páginas de 4 KiB, +4096 no topo, alinhamento grande a partir de 2 MiB).
 *
 *   gcc -std=c11 -Wall -Wextra -Werror -O2 -o /tmp/a64page_test tools/a64page_test.c
 *   /tmp/a64page_test
 *
 * Não substitui o teste dentro do mOS (userland/tests-a64rt, argumento "pagina").
 */
#include "../userland/libsystem/a64page.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

static int falhas;
#define CHECK(nome, cond) do { \
    if (cond) printf("ok   %s\n", nome); \
    else { printf("FALHA %s (linha %d)\n", nome, __LINE__); falhas++; } \
} while (0)

/* ---- modelo do kernel: páginas de 4 KiB, igual ao SYS_MMAP/SYS_MUNMAP ---- */
#define KBASE 0x400100000000ULL
#define KSPAN (64ULL << 20)
static uint64_t k_next;
static uint8_t *k_page; /* 1 = página de 4 KiB presente */

static int k_idx(uint64_t a) {
    if (a < KBASE || a >= KBASE + KSPAN) return -1;
    return (int)((a - KBASE) / 4096);
}
static void k_reset(uint64_t next) {
    k_next = next ? next : KBASE;
    memset(k_page, 0, (size_t)(KSPAN / 4096));
}
static int k_all(uint64_t a, uint64_t n, int want) {
    if (a & 0xFFF) return 0;
    for (uint64_t p = a; p < a + n; p += 4096) {
        int i = k_idx(p);
        if (i < 0 || k_page[i] != want) return 0;
    }
    return 1;
}

/* espelha kernel/proc/process.c: b do SYS_MMAP e o recuo de mmap_next no SYS_MUNMAP */
static long ksys(long n, long a, long b, long c, long d, void *ctx) {
    (void)ctx;
    if (n == SYS_MMAP) {
        uint64_t len = ((uint64_t)a + 0xFFF) & ~0xFFFULL;
        if (b == 3 && c) {
            uint64_t at = (uint64_t)c & ~0xFFFULL;
            uint64_t end = ((uint64_t)c + (uint64_t)a + 0xFFF) & ~0xFFFULL;
            if (!a || at < KBASE || end > k_next || end < at) return -MOS_EINVAL;
            for (uint64_t p = at; p < end; p += 4096) {
                int i = k_idx(p);
                if (i < 0) return -MOS_ENOMEM;
                k_page[i] = 1;
            }
            return 0;
        }
        if (!k_next) k_next = KBASE;
        uint64_t al = 4096, at;
        if (b == 3 && !c && len >= (2ull << 20)) {
            al = 1ull << (63 - __builtin_clzll(len));
            if (al > (4ull << 30)) al = 4ull << 30;
        }
        if (b == 3 && !c) at = (k_next + al - 1) & ~(al - 1);
        else at = k_next;
        if (b != 0 && b != 1 && b != 3 && b != 4) return -MOS_EINVAL;
        if (b == 4 && (d & 0xFFF)) return -MOS_EINVAL;
        if (!len || at + len + 4096 > KBASE + KSPAN) return -MOS_ENOMEM;
        if (b != 1) {
            for (uint64_t p = at; p < at + len; p += 4096) k_page[k_idx(p)] = 1;
        }
        k_next = at + len + 4096;
        return (long)at;
    }
    if (n == SYS_MUNMAP) {
        uint64_t addr = (uint64_t)a, len = (uint64_t)b;
        if (addr < KBASE || (addr & 0xFFF) || addr + len > KBASE + KSPAN) return -MOS_EINVAL;
        if (c == MOS_MUNMAP_DISCARD) {
            for (uint64_t p = addr; p < addr + len; p += 4096) k_page[k_idx(p)] = 0;
            return 0;
        }
        for (uint64_t p = addr; p < addr + len; p += 4096) k_page[k_idx(p)] = 0;
        uint64_t end = (addr + len + 0xFFF) & ~0xFFFULL;
        if (len && addr < k_next && end + 4096 >= k_next && c != MOS_MUNMAP_KEEP) k_next = addr;
        return 0;
    }
    return -MOS_EINVAL;
}

static int g_err;
static uint64_t g_mmap(uint64_t hint, uint64_t len, int prot, int flags, long long off) {
    a64_mmap_adj_t adj = a64_mmap_adjust(hint, len, flags, off);
    if (adj.err) { g_err = adj.err; return 0; }
    int anon = (flags & 0x1000) != 0;
    int fixed = (flags & 0x10) != 0;
    if (fixed && anon) {
        long r = a64_map_fixed_anon(ksys, 0, hint, adj.len, prot);
        if (r < 0) { g_err = (int)-r; return 0; }
        g_err = 0;
        return (uint64_t)r;
    }
    if (!anon) {
        long al = a64_align_cursor(ksys, 0);
        if (al < 0) { g_err = (int)-al; return 0; }
        long at = ksys(SYS_MMAP, (long)adj.len, 4, 3, off, 0);
        if (at < 0) { g_err = (int)-at; return 0; }
        long sl = a64_reclaim_slop(ksys, 0, (uint64_t)at, adj.len);
        if (sl < 0) { g_err = (int)-sl; return 0; }
        g_err = 0;
        return (uint64_t)at;
    }
    unsigned long long at = 0;
    long rc = a64_map_fresh(ksys, 0, adj.len, prot ? 3 : 1, &at);
    if (rc < 0) { g_err = (int)-rc; return 0; }
    g_err = 0;
    return at;
}
static int g_prot(uint64_t addr, uint64_t len, int prot) {
    a64_span_t sp = a64_span(addr, len);
    if (sp.err) return sp.err;
    if (!sp.len) return 0;
    long r = !prot ? ksys(SYS_MUNMAP, (long)sp.addr, (long)sp.len, MOS_MUNMAP_KEEP, 0, 0)
                   : ksys(SYS_MMAP, (long)sp.len, 3, (long)sp.addr, 0, 0);
    return r < 0 ? (int)-r : 0;
}
static int g_unmap(uint64_t addr, uint64_t len) {
    a64_span_t sp = a64_span(addr, len);
    if (sp.err) return sp.err;
    if (!sp.len) return 0;
    long r = ksys(SYS_MUNMAP, (long)sp.addr, (long)sp.len, 0, 0, 0);
    return r < 0 ? (int)-r : 0;
}
static int g_advise(uint64_t addr, uint64_t len) {
    a64_span_t sp = a64_span(addr, len);
    if (sp.err) return sp.err;
    if (!sp.len) return 0;
    long r = ksys(SYS_MUNMAP, (long)sp.addr, (long)sp.len, MOS_MUNMAP_DISCARD, 0, 0);
    return r < 0 ? (int)-r : 0;
}

static void testa_constantes(void) {
    _Static_assert(A64_GUEST_PAGE == 16384, "pagina guest");
    _Static_assert(A64_GUEST_PAGE_SHIFT == 14, "shift");
    CHECK("A64_GUEST_PAGE_END arredonda o fim para 16 KiB",
          A64_GUEST_PAGE_END(0x4000, 0x100) == 0x8000 &&
          A64_GUEST_PAGE_END(0x4000, 0x4000) == 0x8000 &&
          A64_GUEST_PAGE_END(0x4000, 0) == 0x4000 &&
          A64_GUEST_PAGE_END(0x4100, 0x10) == 0x8000);
    int ok = 1;
#define CONF(n, v) \
        if (!strcmp(n, "hw.pagesize") && (unsigned long long)(v) != 16384) ok = 0; \
        if (!strcmp(n, "hw.pagesize32") && (unsigned long long)(v) != 16384) ok = 0; \
        if (!strcmp(n, "vm.pagesize") && (unsigned long long)(v) != 16384) ok = 0;
    A64RT_SYSCTL_INTS(CONF)
#undef CONF
    CHECK("sysctl arm64 hw.pagesize/hw.pagesize32/vm.pagesize = 16384", ok);
    CHECK("guarda 0 fica 0 e 100 sobe a 16 KiB", a64_guard_round(0) == 0 && a64_guard_round(100) == 16384 &&
          a64_guard_round(16384) == 16384 && a64_guard_round(20000) == 32768);
    unsigned long long ra = 0x1500, rs = 4096;
    a64_region_guest(&ra, &rs, 7);
    CHECK("regiao sintetica de 4 KiB vira a pagina guest", ra == 0 && rs == 16384);
    ra = 0x4000; rs = 4096;
    a64_region_guest(&ra, &rs, 3);
    CHECK("faixa registrada nao e esticada", ra == 0x4000 && rs == 4096);
    unsigned long long db, de;
    a64_dealloc_span(0x4100, 0x10, &db, &de);
    CHECK("deallocate desce o inicio e sobe o fim para 16 KiB", db == 0x4000 && de == 0x8000);
    a64_span_t sp = a64_span(0x4000, 1);
    CHECK("protect de 1 byte alinhado cobre a pagina guest", sp.err == 0 && sp.len == 16384);
    sp = a64_span(0x4100, 16384);
    CHECK("protect desalinhado e EINVAL", sp.err == MOS_EINVAL);
}

static int fonte_tem(const char *path, const char *agulha) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char buf[512];
    int ok = 0;
    while (fgets(buf, sizeof buf, f)) if (strstr(buf, agulha)) { ok = 1; break; }
    fclose(f);
    return ok;
}

static void testa_x86_inalterado(void) {
    CHECK("hw.pagesize do x86_64 no darwin.c continua 4096",
          fonte_tem("userland/libsystem/darwin.c", "{\"hw.pagesize\", 8, 4096, 0}"));
    CHECK("vm_page_size do x86_64 no mach.c continua 4096",
          fonte_tem("userland/libsystem/mach.c", "vm_page_size = 4096"));
}

static void testa_mmap(void) {
    k_reset(KBASE);
    uint64_t a = g_mmap(0, 100000, 3, 0x1002, 0);
    uint64_t len = (100000 + 16383u) & ~16383u;
    CHECK("mmap anonimo de 100000 alinha a 16 KiB e cobre o pedido",
          g_err == 0 && a && (a & 16383) == 0 && k_all(a, len, 1) && k_all(a + len, 4096, 0));
    uint64_t b = g_mmap(0, 4096, 3, 0x1002, 0);
    CHECK("segundo mmap cola no fim, tambem em 16 KiB",
          g_err == 0 && b == a + len && (b & 16383) == 0 && k_all(b, 16384, 1));
    CHECK("os dois nao se sobrepoem", a + len == b);

    k_reset(KBASE + 4096); /* cursor do kernel deixado desalinhado, como depois de um rt_alloc */
    uint64_t c = g_mmap(0, 16384, 3, 0x1002, 0);
    CHECK("cursor 4 KiB desalinhado ainda devolve 16 KiB",
          g_err == 0 && c && (c & 16383) == 0 && k_all(c, 16384, 1));
    uint64_t d = g_mmap(0, 8192, 3, 0x1002, 0);
    CHECK("o seguinte continua alinhado", g_err == 0 && d == c + 16384 && k_all(d, 16384, 1));

    k_reset(KBASE);
    uint64_t z = g_mmap(0, 65536, 0, 0x1002, 0);
    CHECK("PROT_NONE reserva 16 KiB sem paginas", g_err == 0 && z && (z & 16383) == 0 && k_all(z, 65536, 0));

    uint64_t f = g_mmap(0, 100, 3, 0, 16384); /* arquivo, deslocamento alinhado */
    CHECK("mmap de arquivo alinha o endereco e o comprimento",
          g_err == 0 && f && (f & 16383) == 0 && k_all(f, 16384, 1));
    g_mmap(0, 100, 3, 0, 4096);
    CHECK("deslocamento de arquivo fora de 16 KiB e EINVAL", g_err == MOS_EINVAL);
    g_mmap(0, 0, 3, 0x1002, 0);
    CHECK("comprimento 0 e EINVAL", g_err == MOS_EINVAL);

    k_reset(KBASE);
    uint64_t big = g_mmap(0, 2u << 20, 3, 0x1002, 0);
    CHECK("faixa de 2 MiB respeita o alinhamento grande do kernel e o de 16 KiB",
          g_err == 0 && big && (big & ((2u << 20) - 1)) == 0 && (big & 16383) == 0 && k_all(big, 2u << 20, 1));
}

static void testa_fixed_mprotect_guarda(void) {
    k_reset(KBASE);
    uint64_t base = g_mmap(0, 65536, 3, 0x1002, 0);
    CHECK("reserva de 64 KiB", g_err == 0 && base && k_all(base, 65536, 1));

    uint64_t mid = g_mmap(base + 16384, 1, 0, 0x1010 | 0x1000, 0); /* MAP_FIXED|MAP_ANON, PROT_NONE, 1 byte */
    CHECK("MAP_FIXED PROT_NONE de 1 byte derruba a pagina guest do meio",
          g_err == 0 && mid == base + 16384 && k_all(base, 16384, 1) && k_all(mid, 16384, 0) &&
          k_all(base + 32768, 32768, 1));

    g_mmap(base + 4096, 16384, 3, 0x1010 | 0x1000, 0);
    CHECK("MAP_FIXED desalinhado e EINVAL e nao mexe nas paginas",
          g_err == MOS_EINVAL && k_all(base, 16384, 1) && k_all(mid, 16384, 0));

    int e = g_prot(base + 32768, 100, 0);
    CHECK("mprotect parcial PROT_NONE cobre os 16 KiB e poupa o vizinho",
          e == 0 && k_all(base + 32768, 16384, 0) && k_all(base + 49152, 16384, 1) && k_all(base, 16384, 1));
    e = g_prot(base + 32768 + 4096, 4096, 3);
    CHECK("mprotect no meio da pagina guest e EINVAL", e == MOS_EINVAL && k_all(base + 32768, 16384, 0));
    e = g_prot(base + 32768, 1, 3);
    CHECK("mprotect de volta a RW religa a pagina guest inteira", e == 0 && k_all(base + 32768, 16384, 1));

    e = g_unmap(base + 16384, 100);
    CHECK("munmap de 100 bytes solta a pagina guest e nao o vizinho",
          e == 0 && k_all(mid, 16384, 0) && k_all(base, 16384, 1) && k_all(base + 32768, 16384, 1));
    e = g_unmap(base + 8, 16384);
    CHECK("munmap desalinhado e EINVAL", e == MOS_EINVAL && k_all(base, 16384, 1));
    e = g_unmap(base, 0);
    CHECK("munmap de comprimento 0 e sucesso", e == 0);

    k_reset(KBASE);
    uint64_t w = g_mmap(0, 65536, 3, 0x1002, 0);
    e = g_advise(w + 16384, 1);
    CHECK("madvise de 1 byte descarta a pagina guest inteira",
          e == 0 && k_all(w, 16384, 1) && k_all(w + 16384, 16384, 0) && k_all(w + 32768, 32768, 1));
    e = g_advise(w + 4096, 4096);
    CHECK("madvise desalinhado e EINVAL", e == MOS_EINVAL && k_all(w, 16384, 1));

    /* pilha de thread: guarda de 16 KiB no começo, o resto presente */
    k_reset(KBASE);
    uint64_t guard = a64_guard_round(16384), stk = a64_guard_round(512u << 10);
    uint64_t s = g_mmap(0, guard + stk, 3, 0x1002, 0);
    e = g_prot(s, guard, 0);
    CHECK("guarda de pilha de 16 KiB falta e o resto da pilha fica mapeado",
          e == 0 && (s & 16383) == 0 && k_all(s, guard, 0) && k_all(s + guard, stk, 1));
    uint64_t top = s + guard + stk;
    CHECK("topo e tamanho batem com pthread_get_stackaddr/size", top - stk == s + guard && stk == (512u << 10));
}

int main(void) {
    k_page = calloc(1, (size_t)(KSPAN / 4096));
    if (!k_page) { fprintf(stderr, "sem memoria\n"); return 1; }
    printf("a64page_test: pagina guest %u sobre host 4096\n", A64_GUEST_PAGE);
    testa_constantes();
    testa_x86_inalterado();
    testa_mmap();
    testa_fixed_mprotect_guarda();
    free(k_page);
    printf("a64page_test: %d falhas\n", falhas);
    return falhas ? 1 : 0;
}
