/* Teste de host do escalonador (QoS, faixas, máscara de IPI) e do cache compartilhado do JIT. */
#include "../kernel/include/smp_perf.h"
#include "../userland/a64/jit_share.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int fails;

static void expect(int cond, const char *msg) {
    if (!cond) { fprintf(stderr, "falhou: %s\n", msg); fails++; }
}

int main(void) {
    unsigned char cached[4] = {0, 1, 0, 0};
    unsigned char cur[4] = {1, 0, 0, 1};
    unsigned char on[4] = {1, 1, 1, 0};
    unsigned long long mask;
    jit_share_tab *tab;
    unsigned char host[16], out[16], guest[8] = {1, 2, 3, 4, 5, 6, 7, 8};
    uint32_t glen = 0, hlen = 0;
    uint64_t eh = 0;
    int i;

    expect(smp_qos_from_darwin(0x21) == 0, "interativo");
    expect(smp_qos_from_darwin(0x19) == 1, "iniciado");
    expect(smp_qos_from_darwin(0x15) == 2, "padrao");
    expect(smp_qos_from_darwin(0) == 2, "sem classe");
    expect(smp_qos_from_darwin(0x11) == 3, "utilidade");
    expect(smp_qos_from_darwin(0x09) == 4, "fundo");

    expect(smp_stripes(0x200000, 0x1000) == (1u << 1), "uma faixa");
    expect(smp_stripes(0, 0) == 0, "vazio");
    expect(smp_stripes(0, 1ull << 30) == 0xFFFFu, "intervalo grande");

    mask = smp_shoot_mask(4, 0, cached, cur, on);
    expect(mask == (1ull << 1), "so a cpu com o pcid em cache");
    cur[0] = 0;
    cached[3] = 1;
    mask = smp_shoot_mask(4, 1, cached, cur, on);
    expect((mask & (1ull << 1)) == 0, "self de fora");
    expect((mask & (1ull << 3)) == 0, "cpu offline de fora");

    tab = calloc(1, JIT_SHARE_BYTES);
    expect(tab != NULL, "tabela");
    expect(sizeof(jit_share_tab) <= JIT_SHARE_BYTES, "cabe em 64 KiB");
    expect(jit_share_executable() == 0, "cache nao executavel");
    for (i = 0; i < 16; i++) host[i] = (unsigned char)(0x90 + i);
    jit_share_bind(tab, JIT_SHARE_BYTES);
    jit_share_publish(0xabc, 0x1000, 0x4000, jit_share_hash(guest, 8), host, 16, 8);
    jit_share_bind(tab, JIT_SHARE_BYTES);
    expect(jit_share_peek(0xabc, 0x1000, 0x4000, &glen, &eh, out, sizeof out, &hlen) == 1, "acerto");
    expect(glen == 8 && hlen == 16 && eh == jit_share_hash(guest, 8), "metadados");
    expect(memcmp(out, host, 16) == 0, "bytes");
    expect(jit_share_peek(0xabc, 0x1000, 0x5000, &glen, &eh, out, sizeof out, &hlen) == 0, "outro pc");
    jit_share_invalidate(0x4000, 0x4004);
    expect(jit_share_peek(0xabc, 0x1000, 0x4000, &glen, &eh, out, sizeof out, &hlen) == 0, "invalidado");
    free(tab);

    if (fails) { fprintf(stderr, "%d falhas\n", fails); return 1; }
    printf("smpperf_host: ok\n");
    return 0;
}
