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

/* prioridade Mach base de cada classe (índice de smp_qos_from_darwin), como no XNU:
 * interativo 47, iniciado 37, padrão 31, utilidade 20, fundo 4 */
static inline int smp_qos_base_pri(int qidx) {
    static const unsigned char t[5] = {47, 37, 31, 20, 4};
    return qidx >= 0 && qidx < 5 ? t[qidx] : 31;
}

/* uso recente com meia-vida de 100 ms: cada 100 ms inteiros desde `at` dividem por 2 */
static inline unsigned smp_usage_decay(unsigned usage, unsigned long long at, unsigned long long now) {
    unsigned long long n = now > at ? (now - at) / 100 : 0;
    return n >= 32 ? 0 : usage >> n;
}

/* prioridade efetiva: a base menos 1 a cada 8 ms de uso recente, até 16 (timeshare do Mach).
 * Fixa (pri_fixed) não decai. Sempre em 0..127. */
static inline int smp_eff_pri(int base, unsigned usage, int fixed) {
    int pen = fixed ? 0 : (int)(usage / 8 > 16 ? 16 : usage / 8);
    int e = base - pen;
    if (e < 0) e = 0;
    if (e > 127) e = 127;
    return e;
}
