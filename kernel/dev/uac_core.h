/* mOS — núcleo da classe de áudio USB (UAC 1.0 e 2.0) para microfones e headsets: descritores das interfaces
 * AudioControl/AudioStreaming, escolha da alternate setting, faixas de taxa do UAC2 e conversão do PCM do dispositivo
 * para 48 kHz estéreo 16 bits (o anel de captura do sound.c). Sem dependência do kernel: tools/uac_core_test.c.
 * Código original do mOS (MIT). */
#pragma once
#include <stdint.h>

#define UAC_MAX_RATES 16
typedef struct {
    uint8_t alt;              /* bAlternateSetting */
    uint8_t terminal;         /* bTerminalLink */
    uint8_t channels, subslot, bits;
    uint8_t nrates;
    uint32_t rates[UAC_MAX_RATES];   /* UAC1: lista (ou {mín, máx} contínua); UAC2: vem do relógio */
    int continuous;
} uac_alt_info;
/* versão (0x0100 ou 0x0200) e o id do relógio (UAC2) a partir dos descritores da AudioControl */
uint16_t uac_parse_ac(const uint8_t *d, uint32_t len, uint8_t *clock_id);
/* descritores de uma alternate setting da AudioStreaming; 0 ok (PCM Type I) */
int uac_parse_as(const uint8_t *d, uint32_t len, uint16_t version, uac_alt_info *out);
/* resposta de GET RANGE da taxa (UAC2): wNumSubRanges e trios {mín, máx, passo} de 32 bits; devolve quantas taxas */
int uac_parse_ranges(const uint8_t *d, uint32_t len, uint32_t *rates, int max, int *continuous);
/* nota de uma combinação: mais alta é melhor (48 kHz, 16 bits, até 2 canais) */
int uac_score(const uac_alt_info *a, uint32_t rate);
/* taxa escolhida para uma alt: 48000 se possível, senão a mais perto */
uint32_t uac_pick_rate(const uac_alt_info *a);

/* conversão: PCM inteiro little-endian (subslot 2/3/4 bytes, 1..8 canais) -> 48 kHz estéreo i16, reamostragem linear */
typedef struct { uint32_t in_rate; uint64_t pos; int16_t last_l, last_r; int have_last; } uac_rs;
void uac_rs_init(uac_rs *r, uint32_t rate);
uint32_t uac_convert(uac_rs *r, const uint8_t *src, uint32_t bytes, int channels, int subslot, int16_t *out, uint32_t cap);
