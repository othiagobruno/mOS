/*
 * mOS — teste de host de userland/libsystem/rand48.c. O objeto entra com o prefixo mos_; como o gerador de 48 bits
 * é o mesmo da glibc (a=0x5DEECE66D, c=0xB), as saídas devem bater bit a bit. Rodar: tools/rand48_hosttest.sh
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void mos_srand48(long);
double mos_drand48(void);
long mos_lrand48(void), mos_mrand48(void);
double mos_erand48(unsigned short[3]);
long mos_nrand48(unsigned short[3]), mos_jrand48(unsigned short[3]);
unsigned short *mos_seed48(unsigned short[3]);
void mos_lcong48(unsigned short[7]);

static int falhas, total;
#define CHECK(c, ...) do { total++; if (!(c)) { falhas++; printf("FALHA %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

int main(void) {
    /* drand48/lrand48/mrand48 após a mesma semente: bit a bit com a glibc */
    for (long seed = 1; seed <= 5; seed++) {
        mos_srand48(seed); srand48(seed);
        for (int k = 0; k < 500; k++) {
            if (mos_drand48() != drand48()) { CHECK(0, "drand48 seed=%ld k=%d", seed, k); break; }
        }
        mos_srand48(seed); srand48(seed);
        for (int k = 0; k < 500; k++) {
            if (mos_lrand48() != lrand48()) { CHECK(0, "lrand48 seed=%ld k=%d", seed, k); break; }
            if (mos_mrand48() != mrand48()) { CHECK(0, "mrand48 seed=%ld k=%d", seed, k); break; }
        }
    }
    CHECK(1, "drand48/lrand48/mrand48 ok");

    /* erand48/nrand48/jrand48 com estado explícito xsubi */
    unsigned short xa[3] = {0x330E, 0xABCD, 0x1234}, xb[3] = {0x330E, 0xABCD, 0x1234};
    int ok = 1;
    for (int k = 0; k < 500; k++) {
        if (mos_erand48(xa) != erand48(xb)) { ok = 0; CHECK(0, "erand48 k=%d", k); break; }
        if (memcmp(xa, xb, 6)) { ok = 0; CHECK(0, "erand48 estado divergiu k=%d", k); break; }
    }
    CHECK(ok, "erand48 + estado");
    unsigned short na[3] = {1, 2, 3}, nb[3] = {1, 2, 3};
    ok = 1;
    for (int k = 0; k < 500; k++) {
        if (mos_nrand48(na) != nrand48(nb)) { ok = 0; CHECK(0, "nrand48 k=%d", k); break; }
        if (mos_jrand48(na) != jrand48(nb)) { ok = 0; CHECK(0, "jrand48 k=%d", k); break; }
    }
    CHECK(ok, "nrand48/jrand48");

    /* seed48: semeia pelos 3 shorts e devolve o estado anterior */
    unsigned short s1[3] = {0x1111, 0x2222, 0x3333};
    unsigned short c1[3] = {0x1111, 0x2222, 0x3333};
    mos_seed48(s1); seed48(c1);
    ok = 1;
    for (int k = 0; k < 500; k++) if (mos_drand48() != drand48()) { ok = 0; CHECK(0, "seed48 k=%d", k); break; }
    CHECK(ok, "seed48");

    /* lcong48: define estado + multiplicador + incremento */
    unsigned short p1[7] = {0xAAAA, 0xBBBB, 0xCCCC, 0x1234, 0x5678, 0x9ABC, 0x000D};
    unsigned short q1[7] = {0xAAAA, 0xBBBB, 0xCCCC, 0x1234, 0x5678, 0x9ABC, 0x000D};
    mos_lcong48(p1); lcong48(q1);
    ok = 1;
    for (int k = 0; k < 500; k++) if (mos_lrand48() != lrand48()) { ok = 0; CHECK(0, "lcong48 k=%d", k); break; }
    CHECK(ok, "lcong48");

    printf("rand48: %d/%d ok\n", total - falhas, total);
    return falhas != 0;
}
