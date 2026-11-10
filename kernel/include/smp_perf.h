/* Escolhas puras do SMP (QoS Darwin, faixas de trava, máscara de shootdown). Sem estado. */
#pragma once

/* QOS_CLASS_* do Darwin (pthread/qos.h): 0x21 interativo, 0x19 iniciado, 0x15 padrão,
 * 0x11 utilidade, 0x09 fundo, 0 sem classe. Índice 0 corre primeiro. */
static inline int smp_qos_from_darwin(unsigned q) {
    switch (q) {
    case 0x21: return 0;
    case 0x19: return 1;
    case 0x15: case 0x00: return 2;
    case 0x11: return 3;
    case 0x09: return 4;
    default: return 2;
    }
}

/* 16 faixas de 2 MiB (VA>>21). Intervalo maior que a tabela trava as 16. */
static inline unsigned smp_stripes(unsigned long long a, unsigned long long len) {
    unsigned long long e;
    unsigned m = 0;
    unsigned long long x;
    if (!len) return 0;
    e = a + len - 1;
    if (e < a || (e - a) >= (16ull << 21)) return 0xFFFFu;
    for (x = a & ~((1ull << 21) - 1); x <= e; x += 1ull << 21) {
        m |= 1u << ((x >> 21) & 15);
        if (x + (1ull << 21) < x) break;
    }
    return m;
}

/* CPUs que recebem o IPI: as que têm o espaço em cache (PCID) ou o CR3 atual, menos self. */
static inline unsigned long long smp_shoot_mask(unsigned ncpu, int self,
                                                const unsigned char *cached,
                                                const unsigned char *cur,
                                                const unsigned char *online) {
    unsigned long long m = 0;
    unsigned i;
    if (!cached || !cur || !online) return 0;
    for (i = 0; i < ncpu && i < 64; i++) {
        if ((int)i == self || !online[i]) continue;
        if (cached[i] || cur[i]) m |= 1ull << i;
    }
    return m;
}
