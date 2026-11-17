/* mOS — teste de host do núcleo do CoreServices antigo (userland/libsystem/cs_legacy_core.inc): UCCollator sobre a ICU,
 * conversão de texto (TextToUnicode, UnicodeToText, TEC com caractere partido entre chamadas), Memory Manager e a
 * tabela de erros. Usa a ICU do Linux pela mesma tabela do libSystem (FDN_ICU_HOST).
 *   sh tools/cs-legacy-hosttest.sh   -> "cs_legacy: N/N ok" */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fdn_encoding_core.h"
#include "cs_legacy_core.inc"

static int ok, total;
static void check(int c, const char *w) { total++; if (c) ok++; else printf("FALHA: %s\n", w); }
static int u16(const char *s, cs_unichar *o) { int32_t n = 0; UChar *u = fdn_u16_dup(s, &n); memcpy(o, u, (size_t)n * 2); free(u); return n; }

int main(void) {
    cs_unichar a[64], b[64];
    int an, bn, order = 9;
    unsigned char eq = 9;
    cs_collator *k = 0;
    /* collation: padrão (diferencia caixa e acento), sem caixa, sem acento, números */
    check(cs_collator_new("pt_BR", 0, &k) == 0 && k, "UCCreateCollator pt_BR");
    an = u16("maçã", a); bn = u16("macarrão", b);
    check(cs_collate(k, a, an, b, bn, &eq, &order) == 0 && order == -1 && !eq, "maçã < macarrão (ç e ã contam como c e a no 1º nível)");
    an = u16("Ana", a); bn = u16("ana", b);
    check(cs_collate(k, a, an, b, bn, &eq, &order) == 0 && !eq && order == 1, "padrão: Ana != ana (maiúscula depois)");
    cs_collator_free(k);
    check(cs_collator_new("pt_BR", kUCCollateCaseInsensitiveMask, &k) == 0 && cs_collate(k, a, an, b, bn, &eq, &order) == 0 && eq && order == 0,
          "sem caixa: Ana == ana");
    an = u16("José", a); bn = u16("jose", b);
    check(cs_collate(k, a, an, b, bn, &eq, &order) == 0 && !eq, "sem caixa ainda vê o acento");
    cs_collator_free(k);
    check(cs_collator_new("pt_BR", kUCCollateCaseInsensitiveMask | kUCCollateDiacritInsensitiveMask, &k) == 0 &&
          cs_collate(k, a, an, b, bn, &eq, &order) == 0 && eq, "sem caixa e sem acento: José == jose");
    cs_collator_free(k);
    cs_collator_new("en", kUCCollateDigitsAsNumberMask, &k);
    an = u16("arquivo 10", a); bn = u16("arquivo 9", b);
    check(cs_collate(k, a, an, b, bn, &eq, &order) == 0 && order == 1, "números: 10 > 9");
    /* chaves: comparar chaves = comparar texto */
    unsigned ka[256], kb[256];
    unsigned long ga = 0, gb = 0;
    check(cs_collation_key(k, a, an, 256, &ga, ka) == 0 && cs_collation_key(k, b, bn, 256, &gb, kb) == 0 && ga && gb, "UCGetCollationKey");
    cs_compare_keys(ka, ga, kb, gb, &eq, &order);
    check(order == 1 && !eq, "chaves na mesma ordem do texto");
    check(cs_collation_key(k, a, an, 2, &ga, ka) == kUCOutputBufferTooSmall && ga > 2, "chave sem espaço = kUCOutputBufferTooSmall + tamanho");
    cs_collator_free(k);
    cs_collator_new("en", 0, &k);
    check(cs_collate(k, 0, 1, b, bn, &eq, &order) == cs_paramErr, "texto NULL = paramErr");
    cs_collator_free(k);

    /* texto -> Unicode */
    cs_unichar out[64];
    unsigned long rd = 0, ol = 0;
    check(cs_to_unicode(0 /* MacRoman */, (const uint8_t *)"caf\x8e", 4, &rd, out, sizeof out, &ol) == 0 && rd == 4 && ol == 8 && out[3] == 0xe9,
          "MacRoman -> Unicode");
    check(cs_to_unicode(cs_text_encoding(0x0100, 0, 2) /* UTF-8 */, (const uint8_t *)"a\xc3", 2, &rd, out, sizeof out, &ol) == kTECPartialCharErr &&
          rd == 1 && ol == 2, "UTF-8 partido: lê 1 byte e avisa kTECPartialCharErr");
    check(cs_to_unicode(0x0500, (const uint8_t *)"abcdef", 6, &rd, out, 6, &ol) == kTECOutputBufferFullStatus && rd == 3 && ol == 6,
          "saída cheia: 3 caracteres e 3 bytes lidos");
    check(cs_to_unicode(0x7777, (const uint8_t *)"x", 1, &rd, out, sizeof out, &ol) == kTextUnsupportedEncodingErr, "codificação desconhecida");
    /* Unicode -> texto */
    uint8_t o8[64];
    an = u16("€5 ü", a);
    check(cs_from_unicode(0x0500 /* cp1252 */, a, (unsigned long)an * 2, 0, &rd, o8, sizeof o8, &ol) == 0 && ol == 4 && o8[0] == 0x80 && o8[3] == 0xfc,
          "Unicode -> Windows-1252");
    an = u16("日本", a);
    check(cs_from_unicode(0x0201 /* Latin-1 */, a, (unsigned long)an * 2, 0, &rd, o8, sizeof o8, &ol) == kTECUnmappableElementErr,
          "sem equivalente = kTECUnmappableElementErr");
    check(cs_from_unicode(0x0201, a, (unsigned long)an * 2, 1, &rd, o8, sizeof o8, &ol) == 0 && ol == 2 && o8[0] == '?', "com fallback vira '?'");
    /* TEC: Shift-JIS -> UTF-8 em pedaços, com o caractere partido entre as chamadas */
    cs_tec t = {CS_TEC_MAGIC, 0x0A01 /* ShiftJIS */, cs_text_encoding(0x0100, 0, 2), {0}, 0};
    const uint8_t sj[] = {0x93, 0xfa, 0x96, 0x7b};            /* 日本 */
    uint8_t res[32];
    unsigned long w1 = 0, w2 = 0, r1 = 0, r2 = 0;
    cs_status s1 = cs_tec_convert(&t, sj, 3, &r1, res, sizeof res, &w1);
    cs_status s2 = cs_tec_convert(&t, sj + 3, 1, &r2, res + w1, sizeof res - w1, &w2);
    check(s1 == 0 && s2 == 0 && r1 == 3 && r2 == 1 && w1 + w2 == 6 && !memcmp(res, "\xe6\x97\xa5\xe6\x9c\xac", 6), "TEC Shift-JIS em pedaços -> UTF-8");

    /* Memory Manager */
    char **h = cs_new_handle(10, 1);
    check(h && cs_handle_size(h) == 10 && (*h)[9] == 0, "NewHandleClear(10)");
    memcpy(*h, "0123456789", 10);
    cs_set_handle_size(h, 20);
    check(cs_memerr == 0 && cs_handle_size(h) == 20 && !memcmp(*h, "0123456789", 10) && (*h)[15] == 0, "SetHandleSize cresce e mantém");
    cs_master_of(h)->state |= (signed char)0x80;
    cs_set_handle_size(h, 40);
    check(cs_memerr == -117 && cs_handle_size(h) == 20, "travado não cresce (memLockedErr)");
    cs_dispose_handle(h);
    char *p = cs_new_ptr(33, 1);
    check(p && cs_ptr_size(p) == 33 && p[32] == 0 && ((unsigned long)p & 15) == 0, "NewPtrClear: tamanho e alinhamento");
    cs_dispose_ptr(p);
    check(cs_err_index(-43) >= 0 && !strcmp(cs_errs[cs_err_index(-43)].name, "fnfErr") && cs_err_index(12345) < 0, "tabela de erros");
    printf("cs_legacy: %d/%d %s\n", ok, total, ok == total ? "ok" : "FALHOU");
    return ok != total;
}
