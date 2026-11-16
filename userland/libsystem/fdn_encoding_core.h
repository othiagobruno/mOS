/*
 * mOS libSystem — codificações de texto do CFString/NSString (CFStringEncoding) em UTF-16, com os conversores
 * da ICU para as tabelas (Mac, Windows, ISO, DOS, CJK) e código próprio para UTF-8/16/32, ASCII e o
 * NonLossyASCII. Testado no host por tools/fdn_encoding_test.c.
 */
#ifndef FDN_ENCODING_CORE_H
#define FDN_ENCODING_CORE_H
#include "fdn_icu.h"

enum {
    ENC_MACROMAN = 0, ENC_ASCII = 0x0600, ENC_LATIN1 = 0x0201, ENC_WIN1252 = 0x0500, ENC_NONLOSSY = 0x0BFF,
    ENC_UTF8 = 0x08000100, ENC_UTF16 = 0x0100, ENC_UTF16BE = 0x10000100, ENC_UTF16LE = 0x14000100,
    ENC_UTF32 = 0x0c000100, ENC_UTF32BE = 0x18000100, ENC_UTF32LE = 0x1c000100, ENC_INVALID = 0xffffffffu
};

int enc_available(uint32_t cf);
/* lista das disponíveis, terminada em ENC_INVALID (como CFStringGetListOfAvailableEncodings) */
const uint32_t *enc_list(void);
const char *enc_name(uint32_t cf);           /* "Western (Mac OS Roman)"… ; NULL se desconhecida */
int32_t enc_max_bytes(uint32_t cf, int32_t units);   /* CFStringGetMaximumSizeForEncoding */
uint32_t enc_windows_codepage(uint32_t cf);  /* 0 se não houver */
uint32_t enc_from_windows_codepage(uint32_t cp);
uint32_t enc_mac_compatible(uint32_t cf);    /* CFStringGetMostCompatibleMacStringEncoding */

/* bytes -> UTF-16 (malloc). NULL se os bytes não forem válidos nessa codificação (ou ela não existir).
 * As de "Unicode" (UTF16/UTF32) olham a BOM e, sem ela, usam a ordem da máquina; a BOM do UTF-8 sai. */
UChar *enc_decode(const uint8_t *b, int32_t n, uint32_t cf, int32_t *on);
/* UTF-16 -> bytes, como o CFStringGetBytes: converte [0, n) até onde der. loss = byte no lugar do caractere
 * que não existe na codificação (0 = parar nele); bom = escrever a BOM (só UTF16/UTF32). Com buf NULL só mede.
 * Devolve quantas unidades UTF-16 converteu; *used = bytes. */
int32_t enc_encode(const UChar *u, int32_t n, uint32_t cf, uint8_t loss, int bom, uint8_t *buf, int32_t max, int32_t *used);
/* a codificação toda sem perda? */
int enc_lossless(const UChar *u, int32_t n, uint32_t cf);
/* adivinha a codificação de um arquivo: BOM, UTF-8 válido; depois as sugeridas em ordem. ENC_INVALID se nada */
uint32_t enc_detect(const uint8_t *b, int32_t n, const uint32_t *sugg, int nsugg, int only_sugg);

#endif
