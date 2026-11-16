/* mOS — leitor de QR Code (ISO/IEC 18004) para o CIDetector: acha os três padrões de localização numa imagem em tons de
 * cinza, corrige a perspectiva (padrão de alinhamento quando há), lê formato e versão (BCH), corrige erros
 * (Reed-Solomon em GF(256)) e decodifica os modos numérico, alfanumérico, byte, kanji (bytes Shift_JIS) e ECI.
 * Teste: tools/test-qrread.sh (gera com qrgen.c, desenha, gira e suja a imagem e lê de volta). Código original (MIT). */
#pragma once
#include <stdint.h>

typedef struct {
    uint8_t text[8192]; int len;   /* conteúdo (byte: como veio; kanji: Shift_JIS) */
    int kanji, eci;                /* kanji: houve modo kanji; eci: último designador ECI (-1 nenhum) */
    int version, level, mask;      /* level: 0 L, 1 M, 2 Q, 3 H */
    uint8_t data[3706]; int ndata; /* palavras de dados já corrigidas (errorCorrectedPayload) */
    double corners[4][2];          /* cantos externos: superior esquerdo, superior direito, inferior esquerdo, inferior direito (pixels, y para baixo) */
    int corrected;                 /* bytes corrigidos pelo Reed-Solomon */
} mos_qr_result;
/* gray: w*h, linha 0 em cima. Devolve 1 se leu um código. */
int mos_qr_read(const uint8_t *gray, int w, int h, mos_qr_result *out);
/* só a parte dos dados: palavras de dados já corrigidas de uma versão; 0 ok */
int mos_qr_parse_data(const uint8_t *data, int n, int version, mos_qr_result *out);
