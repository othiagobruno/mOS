/* Commpage no endereço do Darwin e páginas anônimas que sobrevivem à pressão de memória. */
#include "mos.h"

static int fails;
static void line(const char *s) { puts(s); puts("\n"); }
#define CHECK(c, s) do { if (c) line("  ok  " s); else { fails++; line("  FALHOU  " s); } } while (0)

void _start(void) {
    volatile unsigned char *cp = (volatile unsigned char *)0xFFFFFC000ULL;
    int sig = cp[0] == 'c' && cp[1] == 'o' && cp[2] == 'm' && cp[9] == '6';
    if (!sig) {
        char b[48];
        int n = 0;
        const char *h = "0123456789abcdef";
        for (int i = 0; i < 16; i++) { b[n++] = h[cp[i] >> 4]; b[n++] = h[cp[i] & 15]; b[n++] = ' '; }
        b[n++] = '\n'; b[n] = 0;
        puts("  commpage bytes: "); puts(b);
    }
    CHECK(sig, "commpage legível em 0xFFFFFC000");
    CHECK(cp[0x025] == 14, "commpage anuncia página de 16 KiB");

    enum { N = 8 };
    u64 len = (u64)N << 12;
    volatile unsigned char *p = (volatile unsigned char *)sys3(SYS_MMAP, len, 3, 0);
    CHECK((i64)p > 0, "mmap anônimo sob demanda");
    if ((i64)p > 0) {
        for (int i = 0; i < N; i++) p[(u64)i << 12] = (unsigned char)(0x40 + i);
        int fk = fork();
        if (fk == 0) {
            p[0] = 0x99;
            exit(p[4096] == 0x41 ? 0 : 3);
        }
        int st = fk > 0 ? wait(fk) : -1;
        CHECK(st == 0, "filho COW leu a página seguinte");
        CHECK(p[0] == 0x40, "escrita do filho não vazou para o pai");
        for (int i = 0; i < N; i++)
            if (p[(u64)i << 12] != (unsigned char)(0x40 + i) && i) fails++;
    }

    /* Grande o bastante para cruzar o piso de páginas livres quando o QEMU sobe com pouca RAM. */
    u64 big = 32ull << 20;
    volatile unsigned char *b = (volatile unsigned char *)sys3(SYS_MMAP, big, 3, 0);
    CHECK((i64)b > 0, "mmap de 32 MiB");
    if ((i64)b > 0) {
        u64 pages = big >> 12;
        for (u64 i = 0; i < pages; i++) b[i << 12] = (unsigned char)(i * 17);
        int bad = 0;
        for (u64 i = 0; i < pages; i++)
            if (b[i << 12] != (unsigned char)(i * 17)) bad++;
        CHECK(bad == 0, "as páginas anônimas voltaram com o mesmo byte");
    }
    puts(fails ? "swaptest: FALHOU\n" : "swaptest: TODOS OS TESTES OK\n");
    exit(fails ? 1 : 0);
}
