/* Teste de host do fdn_encoding_core.c: UTF-8/16/32, ASCII, NonLossyASCII e as tabelas da ICU (Mac, Windows, ISO, CJK). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fdn_encoding_core.h"
static int ok, bad;
static void eq(const char *w, long a, long b) { if (a == b) ok++; else { bad++; printf("FALHOU %s: %ld (queria %ld)\n", w, a, b); } }
/* texto UTF-8 -> codificação -> bytes esperados -> volta igual */
static void ida_volta(const char *w, const char *txt, uint32_t enc, const char *want, int wn) {
    int32_t n, used = 0, m = 0; UChar *u = fdn_u16_dup(txt, &n);
    uint8_t buf[256];
    int32_t k = enc_encode(u, n, enc, 0, 0, buf, sizeof buf, &used);
    if (k != n || used != wn || memcmp(buf, want, (size_t)wn)) { bad++; printf("FALHOU %s: codificou %d/%d unidades, %d bytes (queria %d)\n", w, k, n, used, wn); }
    else ok++;
    UChar *v = enc_decode((const uint8_t *)want, wn, enc, &m);
    char *back = v ? fdn_utf8_dup(v, m) : 0;
    if (back && !strcmp(back, txt)) ok++; else { bad++; printf("FALHOU %s volta: %s\n", w, back ? back : "(nil)"); }
    free(u); free(v); free(back);
}
int main(void) {
    ida_volta("utf8", "café 😀", ENC_UTF8, "caf\xc3\xa9 \xf0\x9f\x98\x80", 10);
    ida_volta("macroman", "café", ENC_MACROMAN, "caf\x8e", 4);
    ida_volta("cp1252 euro", "€5", ENC_WIN1252, "\x80" "5", 2);
    ida_volta("latin1", "ñ", ENC_LATIN1, "\xf1", 1);
    ida_volta("cp1251", "Да", 0x0502, "\xc4\xe0", 2);
    ida_volta("shift-jis", "日本", 0x0420, "\x93\xfa\x96\x7b", 4);
    ida_volta("euc-jp", "日本", 0x0920, "\xc6\xfc\xcb\xdc", 4);
    ida_volta("utf16be", "A😀", ENC_UTF16BE, "\x00\x41\xd8\x3d\xde\x00", 6);
    ida_volta("utf32le", "A", ENC_UTF32LE, "A\0\0\0", 4);
    ida_volta("nonlossy", "a\\é€", ENC_NONLOSSY, "a\\\\\\351\\u20ac", 13);
    ida_volta("iso-2022-jp", "日", 0x0820, "\x1b$BF|\x1b(B", 8);
    int32_t n, m, used; UChar *u; uint8_t buf[64]; UChar *v;
    /* sem perda: para no caractere que não existe; com perda: troca pelo byte */
    u = fdn_u16_dup("ab€c", &n);
    eq("ascii para", enc_encode(u, n, ENC_ASCII, 0, 0, buf, sizeof buf, &used), 2); eq("ascii bytes", used, 2);
    eq("ascii perda", enc_encode(u, n, ENC_ASCII, '?', 0, buf, sizeof buf, &used), 4); eq("ascii ?", buf[2], '?');
    eq("latin1 sem €", enc_lossless(u, n, ENC_LATIN1), 0); eq("1252 com €", enc_lossless(u, n, ENC_WIN1252), 1);
    eq("só mede", enc_encode(u, n, ENC_UTF8, 0, 0, 0, 0, &used), 4); eq("mede bytes", used, 6);
    eq("buffer curto", enc_encode(u, n, ENC_UTF8, 0, 0, buf, 3, &used), 2);
    free(u);
    /* BOM */
    u = fdn_u16_dup("A", &n);
    enc_encode(u, n, ENC_UTF16, 0, 1, buf, sizeof buf, &used); eq("bom16", used == 4 && buf[0] == 0xFF && buf[1] == 0xFE && buf[2] == 'A', 1);
    free(u);
    v = enc_decode((const uint8_t *)"\xfe\xff\x00\x42", 4, ENC_UTF16, &m); eq("bom be", v && m == 1 && v[0] == 'B', 1); free(v);
    v = enc_decode((const uint8_t *)"\xef\xbb\xbfx", 4, ENC_UTF8, &m); eq("bom utf8 sai", v && m == 1 && v[0] == 'x', 1); free(v);
    /* inválidos */
    eq("utf8 inválido", enc_decode((const uint8_t *)"\xc3(", 2, ENC_UTF8, &m) == 0, 1);
    eq("utf8 longo demais", enc_decode((const uint8_t *)"\xc0\xaf", 2, ENC_UTF8, &m) == 0, 1);
    eq("utf8 substituto", enc_decode((const uint8_t *)"\xed\xa0\x80", 3, ENC_UTF8, &m) == 0, 1);
    eq("ascii alto", enc_decode((const uint8_t *)"\x80", 1, ENC_ASCII, &m) == 0, 1);
    eq("sjis inválido", enc_decode((const uint8_t *)"\x81", 1, 0x0420, &m) == 0, 1);
    eq("utf16 ímpar", enc_decode((const uint8_t *)"abc", 3, ENC_UTF16LE, &m) == 0, 1);
    /* tabelas */
    eq("disponível macroman", enc_available(ENC_MACROMAN), 1); eq("indisponível", enc_available(0x0B01), 0);
    eq("nome", strcmp(enc_name(ENC_MACROMAN), "Western (Mac OS Roman)"), 0);
    eq("codepage", enc_windows_codepage(ENC_WIN1252), 1252); eq("de codepage", enc_from_windows_codepage(932), 0x0420);
    eq("mac compatível", enc_mac_compatible(0x0502), 7);
    eq("máximo utf8", enc_max_bytes(ENC_UTF8, 10), 30);
    int cnt = 0; for (const uint32_t *l = enc_list(); *l != ENC_INVALID; l++) cnt++;
    eq("lista", cnt > 60, 1);
    /* detecção */
    uint32_t sug[1] = { ENC_WIN1252 };
    eq("detecta bom", enc_detect((const uint8_t *)"\xff\xfe" "a\0", 4, 0, 0, 0), ENC_UTF16);
    eq("detecta utf8", enc_detect((const uint8_t *)"caf\xc3\xa9", 5, 0, 0, 0), ENC_UTF8);
    eq("detecta sugestão", enc_detect((const uint8_t *)"caf\xe9", 4, sug, 1, 0), ENC_WIN1252);
    eq("não detecta", enc_detect((const uint8_t *)"caf\xe9", 4, 0, 0, 0), ENC_INVALID);
    printf("fdn_encoding: %d/%d\n", ok, ok + bad);
    return bad != 0;
}
