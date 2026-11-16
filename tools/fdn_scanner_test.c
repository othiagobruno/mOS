/* Teste de host do fdn_scanner_core.c (NSScanner): skip padrão e nil, caixa, conjuntos, números, posição UTF-16. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <float.h>
#include "fdn_scanner_core.h"
static int ok, bad;
static void eq(const char *w, long long a, long long b) { if (a == b) ok++; else { bad++; printf("FALHOU %s: %lld (queria %lld)\n", w, a, b); } }
static void eqd(const char *w, double a, double b) { if (a == b) ok++; else { bad++; printf("FALHOU %s: %.17g (queria %.17g)\n", w, a, b); } }
static void eqs(const char *w, const scn_t *sc, int32_t st, int32_t n, const char *want) {
    char *u = fdn_utf8_dup(sc->s + st, n);
    if (!strcmp(u, want)) ok++; else { bad++; printf("FALHOU %s: \"%s\" (queria \"%s\")\n", w, u, want); }
    free(u);
}
/* whitespaceAndNewlineCharacterSet (o padrão de charactersToBeSkipped) */
static int ws(void *c, uint32_t x) { (void)c; return x == ' ' || x == '\t' || x == '\n' || x == '\r' || x == 0xA0 || x == 0x2028 || x == 0x3000; }
static int digit(void *c, uint32_t x) { (void)c; return x >= '0' && x <= '9'; }
static int letter(void *c, uint32_t x) { (void)c; return (x >= 'a' && x <= 'z') || (x >= 'A' && x <= 'Z') || x == 0x1F600; }
static int brace(void *c, uint32_t x) { (void)c; return x == '{' || x == '%'; }
static UChar *buf;
static scn_t mk(const char *s, int skip) {
    int32_t n; free(buf); buf = fdn_u16_dup(s, &n);
    scn_t sc = { buf, n, 0, skip ? ws : 0, 0, 0, '.' };
    return sc;
}
static UChar *U(const char *s, int32_t *n) { static UChar *k[8]; static int z; free(k[z & 7]); return k[z++ & 7] = fdn_u16_dup(s, n); }
int main(void) {
    int32_t st, n, m; long long v; unsigned long long u; double d; UChar *t;
    /* skip padrão: espaços e quebras antes de cada leitura */
    scn_t sc = mk("  \n hello 42", 1);
    t = U("hello", &m);
    eq("padrão scanString", scn_string(&sc, t, m, &st, &n), 1); eqs("padrão texto", &sc, st, n, "hello");
    eq("padrão scanInt", scn_i64(&sc, LLONG_MIN, LLONG_MAX, &v), 1); eq("padrão valor", v, 42);
    eq("padrão fim", scn_at_end(&sc), 1);
    /* skip nil: espaço é caractere comum */
    sc = mk(" 7", 0);
    eq("nil não pula int", scn_i64(&sc, LLONG_MIN, LLONG_MAX, &v), 0); eq("nil posição intacta", sc.pos, 0);
    eq("nil isAtEnd", scn_at_end(&sc), 0);
    /* caso do OpenSwiftUI (Text+Localized): skip nil, scanUpToCharacters, scanCharacter, scanInt */
    sc = mk("abc %1$@ x", 0);
    eq("upto chars", scn_upto_chars(&sc, brace, 0, &st, &n), 1); eqs("upto texto", &sc, st, n, "abc ");
    eq("upto pos", sc.pos, 4);
    sc.pos += 1;                     /* scanCharacter: o '%' */
    eq("int depois do %", scn_i64(&sc, LLONG_MIN, LLONG_MAX, &v), 1); eq("int valor", v, 1); eq("int pos", sc.pos, 6);
    t = U("$", &m);
    eq("string $", scn_string(&sc, t, m, &st, &n), 1);
    eq("@ não é dígito", scn_i64(&sc, LLONG_MIN, LLONG_MAX, &v), 0);
    /* com skip padrão o mesmo texto pula o espaço antes do %: */
    sc = mk("abc %1$@", 1);
    eq("padrão upto", scn_upto_chars(&sc, brace, 0, &st, &n), 1); eqs("padrão upto texto", &sc, st, n, "abc ");
    /* caseSensitive NO (padrão) e YES */
    sc = mk("HeLLo world", 1); t = U("hello", &m);
    eq("sem caixa", scn_string(&sc, t, m, &st, &n), 1); eqs("sem caixa texto", &sc, st, n, "HeLLo");
    sc = mk("HeLLo", 1); sc.case_sensitive = 1;
    eq("com caixa falha", scn_string(&sc, t, m, &st, &n), 0); eq("com caixa pos", sc.pos, 0);
    sc = mk("Straße!", 1); t = U("STRASSE", &m);
    eq("ß dobrado", scn_string(&sc, t, m, &st, &n), 1); eqs("ß texto", &sc, st, n, "Straße");
    /* scanUpToString */
    sc = mk("key = value", 1); t = U(" = ", &m);
    eq("upto string", scn_upto_string(&sc, t, m, &st, &n), 1); eqs("upto string texto", &sc, st, n, "key");
    t = U("=", &m);
    eq("upto achado na posição", scn_upto_string(&sc, t, m, &st, &n), 0);
    t = U("zzz", &m); sc = mk("tail", 1);
    eq("upto sem achar vai ao fim", scn_upto_string(&sc, t, m, &st, &n), 1); eq("fim", sc.pos, 4);
    sc = mk("ABCxyz", 1); sc.case_sensitive = 0; t = U("X", &m);
    eq("upto sem caixa", scn_upto_string(&sc, t, m, &st, &n), 1); eqs("upto sem caixa texto", &sc, st, n, "ABC");
    /* conjuntos com escalar fora do BMP: posição em UTF-16 */
    sc = mk("ab\xf0\x9f\x98\x80" "c1", 1);
    eq("chars astral", scn_chars(&sc, letter, 0, &st, &n), 1); eq("astral len utf16", n, 5); eq("astral pos", sc.pos, 5);
    eq("chars dígito", scn_chars(&sc, digit, 0, &st, &n), 1); eq("dígito pos", sc.pos, 6);
    /* falha não move a posição nem consome o skip */
    sc = mk("   x", 1);
    eq("int falha", scn_i64(&sc, LLONG_MIN, LLONG_MAX, &v), 0); eq("falha pos 0", sc.pos, 0);
    /* inteiros: sinal, saturação */
    sc = mk("-123 +9 99999999999 -99999999999", 1);
    scn_i64(&sc, INT_MIN, INT_MAX, &v); eq("negativo", v, -123);
    scn_i64(&sc, INT_MIN, INT_MAX, &v); eq("positivo", v, 9);
    scn_i64(&sc, INT_MIN, INT_MAX, &v); eq("satura max", v, INT_MAX); eq("consumiu dígitos", sc.pos, 19);
    scn_i64(&sc, INT_MIN, INT_MAX, &v); eq("satura min", v, INT_MIN);
    sc = mk("-9223372036854775808", 1);
    scn_i64(&sc, LLONG_MIN, LLONG_MAX, &v); eq("llmin exato", v, LLONG_MIN);
    sc = mk("-", 1); eq("só sinal", scn_i64(&sc, LLONG_MIN, LLONG_MAX, &v), 0);
    sc = mk("18446744073709551615 99999999999999999999", 1);
    scn_u64(&sc, &u); eq("u64 max", (long long)u, (long long)~0ULL);
    scn_u64(&sc, &u); eq("u64 satura", (long long)u, (long long)~0ULL);
    /* hexadecimal */
    sc = mk("0xFF ff 0xg 0x1FFFFFFFF", 1);
    scn_hex(&sc, 0xFFFFFFFFull, &u); eq("hex 0x", (long long)u, 255);
    scn_hex(&sc, 0xFFFFFFFFull, &u); eq("hex sem prefixo", (long long)u, 255);
    scn_hex(&sc, 0xFFFFFFFFull, &u); eq("0x sem dígito lê 0", (long long)u, 0); eq("para no x", sc.pos, 9);
    sc.pos = 12;
    scn_hex(&sc, 0xFFFFFFFFull, &u); eq("hex satura 32", (long long)u, 0xFFFFFFFFll);
    /* double */
    sc = mk("3.25 -.5 1e3 2.5e-2 7. 1e .x", 1);
    scn_double(&sc, &d); eqd("3.25", d, 3.25);
    scn_double(&sc, &d); eqd("-.5", d, -0.5);
    scn_double(&sc, &d); eqd("1e3", d, 1000);
    scn_double(&sc, &d); eqd("2.5e-2", d, 0.025);
    scn_double(&sc, &d); eqd("7.", d, 7);
    scn_double(&sc, &d); eqd("1e sem expoente", d, 1); eq("para no e", sc.s[sc.pos], 'e');
    sc.pos++;
    eq(".x falha", scn_double(&sc, &d), 0);
    sc = mk("3,5", 1); sc.dec_sep = ',';
    scn_double(&sc, &d); eqd("vírgula do locale", d, 3.5);
    /* hex double */
    sc = mk("0x1.8p3 -0x10 1p-1 0x.8", 1);
    scn_hex_double(&sc, &d); eqd("0x1.8p3", d, 12);
    scn_hex_double(&sc, &d); eqd("-0x10", d, -16);
    scn_hex_double(&sc, &d); eqd("1p-1", d, 0.5);
    scn_hex_double(&sc, &d); eqd("0x.8", d, 0.5);
    /* prefixo ASCII para o NSDecimal */
    char a[64];
    sc = mk("  12,75x", 1); sc.dec_sep = ',';
    int32_t from; scn_ascii_prefix(&sc, a, sizeof a, &from);
    eq("ascii from", from, 2); eq("ascii troca sep", strcmp(a, "12.75x"), 0);
    printf("fdn_scanner: %d/%d\n", ok, ok + bad);
    return bad != 0;
}
