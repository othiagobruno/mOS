/* Teste de host do kernel/dev/uac_core.c: descritores UAC1/UAC2, faixas, escolha e conversão para 48 kHz estéreo. */
#include <stdio.h>
#include <string.h>
#include "../kernel/dev/uac_core.h"
static int fails;
#define CHECK(c) do { if (!(c)) { printf("FALHOU %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
int main(void) {
    /* AC UAC1: HEADER bcdADC 1.00 */
    uint8_t ac1[9] = {9, 0x24, 0x01, 0x00, 0x01, 0x1E, 0x00, 1, 1};
    uint8_t clk = 9;
    CHECK(uac_parse_ac(ac1, 9, &clk) == 0x0100 && clk == 0);
    /* AC UAC2: HEADER 2.00 + CLOCK_SOURCE id 5 */
    uint8_t ac2[9 + 8] = {9, 0x24, 0x01, 0x00, 0x02, 0x03, 0x40, 0, 0, 8, 0x24, 0x0A, 5, 0x01, 0x07, 0, 0};
    CHECK(uac_parse_ac(ac2, sizeof ac2, &clk) == 0x0200 && clk == 5);
    /* AS UAC1: GENERAL (PCM) + TYPE I mono 16 bits, taxas 16000 e 48000 */
    uint8_t as1[7 + 14] = {7, 0x24, 0x01, 2, 1, 0x01, 0x00, 14, 0x24, 0x02, 1, 1, 2, 16, 2, 0x80, 0x3E, 0x00, 0x80, 0xBB, 0x00};
    uac_alt_info a;
    memset(&a, 0, sizeof a);
    CHECK(uac_parse_as(as1, sizeof as1, 0x0100, &a) == 0 && a.channels == 1 && a.subslot == 2 && a.bits == 16 && a.nrates == 2 && a.rates[1] == 48000 && a.terminal == 2);
    CHECK(uac_pick_rate(&a) == 48000);
    a.rates[1] = 44100;
    CHECK(uac_pick_rate(&a) == 44100);
    /* contínua 8000..96000 */
    uint8_t as1c[7 + 14] = {7, 0x24, 0x01, 2, 1, 0x01, 0x00, 14, 0x24, 0x02, 1, 2, 3, 24, 0, 0x40, 0x1F, 0x00, 0x00, 0x77, 0x01};
    memset(&a, 0, sizeof a);
    CHECK(uac_parse_as(as1c, sizeof as1c, 0x0100, &a) == 0 && a.continuous && a.rates[0] == 8000 && a.rates[1] == 96000 && uac_pick_rate(&a) == 48000);
    /* AS não PCM é recusado */
    as1[5] = 0x02;
    CHECK(uac_parse_as(as1, sizeof as1, 0x0100, &a) == -1);
    /* AS UAC2: GENERAL (Type I, PCM, 2 canais) + FORMAT_TYPE I 3 bytes / 24 bits */
    uint8_t as2[16 + 6] = {16, 0x24, 0x01, 3, 0, 1, 1, 0, 0, 0, 2, 3, 0, 0, 0, 0, 6, 0x24, 0x02, 1, 3, 24};
    memset(&a, 0, sizeof a);
    CHECK(uac_parse_as(as2, sizeof as2, 0x0200, &a) == 0 && a.channels == 2 && a.subslot == 3 && a.bits == 24);
    /* faixas: 44100 fixa e 48000..96000 passo 48000 */
    uint8_t rg[2 + 24] = {2, 0, 0x44, 0xAC, 0, 0, 0x44, 0xAC, 0, 0, 0, 0, 0, 0, 0x80, 0xBB, 0, 0, 0x00, 0x77, 0x01, 0, 0x80, 0xBB, 0, 0};
    uint32_t rates[8]; int cont = 1;
    CHECK(uac_parse_ranges(rg, sizeof rg, rates, 8, &cont) == 3 && rates[0] == 44100 && rates[1] == 48000 && rates[2] == 96000 && !cont);
    /* nota: 48k/16 bits/estéreo > 44.1k > 48k com 6 canais */
    uac_alt_info s1 = {0}, s2 = {0};
    s1.bits = 16; s1.subslot = 2; s1.channels = 2; s2 = s1; s2.channels = 6;
    CHECK(uac_score(&s1, 48000) > uac_score(&s1, 44100) && uac_score(&s1, 48000) > uac_score(&s2, 48000));

    /* conversão: 48 kHz mono 16 bits -> estéreo igual (um quadro de atraso) */
    uac_rs r; uac_rs_init(&r, 48000);
    int16_t in[48], out[2 * 600];
    for (int i = 0; i < 48; i++) in[i] = (int16_t)(i * 100);
    uint32_t n = uac_convert(&r, (const uint8_t *)in, sizeof in, 1, 2, out, 200);
    CHECK(n == 48 && out[0] == 0 && out[1] == 0 && out[2 * 10] == 900 && out[2 * 10 + 1] == 900);
    n = uac_convert(&r, (const uint8_t *)in, sizeof in, 1, 2, out, 200);
    CHECK(n == 48 && out[0] == 4700 && out[2] == 0);   /* o último do pacote anterior vem primeiro */
    /* 44.1 kHz: 441 quadros viram ~480 */
    uac_rs_init(&r, 44100);
    int16_t ramp[441]; uint32_t tot = 0;
    for (int i = 0; i < 441; i++) ramp[i] = 1000;
    for (int k = 0; k < 10; k++) tot += uac_convert(&r, (const uint8_t *)ramp, sizeof ramp, 1, 2, out, 600);
    CHECK(tot >= 4790 && tot <= 4810);
    CHECK(out[10] == 1000);
    /* 24 bits estéreo: os 16 de cima de cada canal */
    uint8_t s24[2 * 2 * 3] = {0x00, 0x34, 0x12, 0x00, 0xCD, 0xAB, 0x00, 0x34, 0x12, 0x00, 0xCD, 0xAB};
    uac_rs_init(&r, 48000);
    n = uac_convert(&r, s24, sizeof s24, 2, 3, out, 10);
    CHECK(n == 2 && out[2] == 0x1234 && out[3] == (int16_t)0xABCD);
    printf(fails ? "uac_core: %d falhas\n" : "uac_core: ok\n", fails);
    return fails != 0;
}
