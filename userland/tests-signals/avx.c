/*
 * mOS — teste: os registradores AVX inteiros (as metades altas dos YMM) sobrevivem a um tratador de sinal que os suja.
 * O kernel guarda na moldura do sinal a imagem FXSAVE (só os 128 bits de baixo) e, com XSAVE, também o cabeçalho e as
 * metades altas (usigframe_t.xext em kernel/proc/process.c). Compila igual no macOS:
 *   zig cc -target x86_64-macos -O1 -mavx2 userland/tests-signals/avx.c -o SinalAVX
 * A thread principal põe um padrão em ymm8..ymm15 e o confere num laço só de assembly (sem syscall: o sinal chega na
 * volta de uma interrupção do temporizador, no meio do laço). Outra thread manda SIGUSR1 para ela sem parar; o tratador
 * enche os 16 YMM de uns. Imprime "sinal-avx: ok" ou "sinal-avx: FALHA".
 */
#include <pthread.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

static volatile int tratados;
static volatile int fim;
static pthread_t principal;

static void tratador(int s) {
    (void)s;
    __asm__ volatile(
        "vpcmpeqd %%ymm0, %%ymm0, %%ymm0\n\t"
        "vmovdqa %%ymm0, %%ymm1\n\t"  "vmovdqa %%ymm0, %%ymm2\n\t"  "vmovdqa %%ymm0, %%ymm3\n\t"
        "vmovdqa %%ymm0, %%ymm4\n\t"  "vmovdqa %%ymm0, %%ymm5\n\t"  "vmovdqa %%ymm0, %%ymm6\n\t"
        "vmovdqa %%ymm0, %%ymm7\n\t"  "vmovdqa %%ymm0, %%ymm8\n\t"  "vmovdqa %%ymm0, %%ymm9\n\t"
        "vmovdqa %%ymm0, %%ymm10\n\t" "vmovdqa %%ymm0, %%ymm11\n\t" "vmovdqa %%ymm0, %%ymm12\n\t"
        "vmovdqa %%ymm0, %%ymm13\n\t" "vmovdqa %%ymm0, %%ymm14\n\t" "vmovdqa %%ymm0, %%ymm15\n\t"
        ::: "xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7",
            "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15");
    tratados++;
}

static void *atirador(void *arg) {
    (void)arg;
    while (!fim) {
        pthread_kill(principal, SIGUSR1);
        usleep(3000);
    }
    return 0;
}

static long agora_ms(void) { struct timeval tv; gettimeofday(&tv, 0); return tv.tv_sec * 1000L + tv.tv_usec / 1000; }

int main(void) {
    setvbuf(stdout, 0, _IOLBF, 0);
    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = tratador;
    sigaction(SIGUSR1, &sa, 0);
    principal = pthread_self();
    /* padrão diferente em cada metade de cada registrador */
    static uint64_t padrao[8][4] __attribute__((aligned(32)));
    for (int r = 0; r < 8; r++)
        for (int q = 0; q < 4; q++) padrao[r][q] = 0x0123456789ABCDEFull ^ ((uint64_t)(r * 4 + q) * 0x1111111111111111ull);
    pthread_t t;
    pthread_create(&t, 0, atirador, 0);
    int falhas = 0, voltas = 0;
    long t0 = agora_ms();
    while (agora_ms() - t0 < 8000 && tratados < 40) {
        int ruim = 0;
        /* carrega, gira ~2^22 vezes conferindo, e devolve 1 se algum dos 8 registradores mudou */
        __asm__ volatile(
            "vmovdqa   0(%1), %%ymm8\n\t"  "vmovdqa  32(%1), %%ymm9\n\t"
            "vmovdqa  64(%1), %%ymm10\n\t" "vmovdqa  96(%1), %%ymm11\n\t"
            "vmovdqa 128(%1), %%ymm12\n\t" "vmovdqa 160(%1), %%ymm13\n\t"
            "vmovdqa 192(%1), %%ymm14\n\t" "vmovdqa 224(%1), %%ymm15\n\t"
            "mov $0x400000, %%ecx\n\t"
            "1:\n\t"
            "vpcmpeqq   0(%1), %%ymm8,  %%ymm0\n\t"  "vpcmpeqq  32(%1), %%ymm9,  %%ymm1\n\t"
            "vpand %%ymm1, %%ymm0, %%ymm0\n\t"
            "vpcmpeqq  64(%1), %%ymm10, %%ymm1\n\t"  "vpand %%ymm1, %%ymm0, %%ymm0\n\t"
            "vpcmpeqq  96(%1), %%ymm11, %%ymm1\n\t"  "vpand %%ymm1, %%ymm0, %%ymm0\n\t"
            "vpcmpeqq 128(%1), %%ymm12, %%ymm1\n\t"  "vpand %%ymm1, %%ymm0, %%ymm0\n\t"
            "vpcmpeqq 160(%1), %%ymm13, %%ymm1\n\t"  "vpand %%ymm1, %%ymm0, %%ymm0\n\t"
            "vpcmpeqq 192(%1), %%ymm14, %%ymm1\n\t"  "vpand %%ymm1, %%ymm0, %%ymm0\n\t"
            "vpcmpeqq 224(%1), %%ymm15, %%ymm1\n\t"  "vpand %%ymm1, %%ymm0, %%ymm0\n\t"
            "vpcmpeqd %%ymm1, %%ymm1, %%ymm1\n\t"
            "vptest %%ymm1, %%ymm0\n\t"              /* CF = 1 se ymm0 tem todos os bits de ymm1 (tudo igual) */
            "jnc 2f\n\t"
            "dec %%ecx\n\t"
            "jnz 1b\n\t"
            "xor %0, %0\n\t"
            "jmp 3f\n\t"
            "2: mov $1, %0\n\t"
            "3:\n\t"
            "vzeroupper\n\t"
            : "=r"(ruim)
            : "r"(padrao)
            : "rcx", "cc", "memory", "xmm0", "xmm1", "xmm8", "xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15");
        voltas++;
        if (ruim) falhas++;
    }
    fim = 1;
    pthread_join(t, 0);
    printf("sinal-avx: %d tratador(es) em %d volta(s), %d volta(s) com YMM alterado\n", tratados, voltas, falhas);
    printf("sinal-avx: %s\n", tratados > 0 && falhas == 0 ? "ok" : tratados == 0 ? "INCONCLUSIVO (nenhum sinal chegou)" : "FALHA");
    return falhas ? 1 : 0;
}
