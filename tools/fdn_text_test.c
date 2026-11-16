/* Teste de host do fdn_text_core.c (busca com opções, comparação, caixa, fronteiras) com a ICU do Linux. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "fdn_text_core.h"
static int ok, bad;
static UChar *U(const char *s, int32_t *n) { return fdn_u16_dup(s, n); }
static void eq(const char *w, long a, long b) { if (a == b) ok++; else { bad++; printf("FALHOU %s: %ld (queria %ld)\n", w, a, b); } }
static void find(const char *w, const char *h, const char *nd, unsigned o, long wl, long wn) {
    int32_t hn, nn, l = -1, n = -1; UChar *a = U(h, &hn), *b = U(nd, &nn);
    int f = tx_search(a, hn, b, nn, o, &l, &n);
    if (!f) { l = -1; n = 0; }
    if (l == wl && n == wn) ok++; else { bad++; printf("FALHOU busca %s: %ld,%ld (queria %ld,%ld)\n", w, (long)l, (long)n, wl, wn); }
    free(a); free(b);
}
static int cmpu(const char *x, const char *y, unsigned o, const char *loc) { int32_t an, bn; UChar *a = U(x, &an), *b = U(y, &bn); int r = tx_compare(a, an, b, bn, o, loc); free(a); free(b); return r; }
static void cased(const char *w, int kind, const char *s, const char *loc, const char *want) {
    int32_t n, m; UChar *a = U(s, &n); UChar *r = tx_case(kind, a, n, loc, &m); char *u = fdn_utf8_dup(r, m);
    if (!strcmp(u, want)) ok++; else { bad++; printf("FALHOU caixa %s: %s (queria %s)\n", w, u, want); }
    free(a); free(r); free(u);
}
static int digit(void *c, uint32_t x) { (void)c; return x >= '0' && x <= '9'; }
static int emoji(void *c, uint32_t x) { (void)c; return x == 0x1F600; }
static int nseg; static int seg(void *c, int32_t a, int32_t b) { (void)c; (void)a; (void)b; nseg++; return 0; }
int main(void) {
    find("simples", "hello world", "world", 0, 6, 5);
    find("caixa", "Hello World", "WORLD", TX_CASE, 6, 5);
    find("sem caixa", "Hello World", "WORLD", 0, -1, 0);
    find("acento", "café com leite", "cafe", TX_DIACRITIC, 0, 4);
    find("sem acento", "café com leite", "cafe", 0, -1, 0);
    find("canônico", "cafe\xcc\x81", "caf\xc3\xa9", 0, 0, 5);
    find("literal", "cafe\xcc\x81", "caf\xc3\xa9", TX_LITERAL, -1, 0);
    find("base sozinha", "cafe\xcc\x81", "cafe", 0, -1, 0);
    find("ß", "Straße", "STRASSE", TX_CASE, 0, 6);
    find("para trás", "abcabc", "abc", TX_BACKWARDS, 3, 3);
    find("âncora", "abcabc", "bc", TX_ANCHORED, -1, 0);
    find("âncora trás", "abcabc", "bc", TX_ANCHORED | TX_BACKWARDS, 4, 2);
    find("largura", "\xef\xbc\xa1\xef\xbc\xa2", "AB", TX_WIDTH, 0, 2);
    find("regex", "a1b22c333", "[0-9]+", TX_REGEX, 1, 1);
    find("regex trás", "a1b22c333", "[0-9]+", TX_REGEX | TX_BACKWARDS, 6, 3);
    find("emoji", "x\xf0\x9f\x98\x80y", "y", 0, 3, 1);
    eq("cmp", cmpu("a", "b", 0, 0), -1); eq("cmp caixa", cmpu("ABC", "abc", TX_CASE, 0), 0);
    eq("cmp num", cmpu("file10", "file9", TX_NUMERIC, 0), 1); eq("cmp sem num", cmpu("file10", "file9", 0, 0), -1);
    eq("cmp locale", cmpu("\xc3\xa9", "f", 0, "fr"), -1); eq("cmp ascii", cmpu("\xc3\xa9", "f", TX_LITERAL, 0), 1);
    eq("cmp locale caixa", cmpu("a", "A", TX_CASE, "en"), 0); eq("cmp locale num", cmpu("x2", "x10", TX_NUMERIC, "en"), -1);
    eq("forced", cmpu("a", "A", TX_CASE | TX_FORCED, "en") != 0, 1);
    cased("upper", 0, "straße ı", "", "STRASSE I");
    cased("turco", 0, "i", "tr", "\xc4\xb0");
    cased("title", 2, "hello wORLD", "", "Hello World");
    cased("lower", 1, "ÁRVORE", "", "árvore");
    int32_t n, st, len; UChar *s = U("ae\xcc\x81" "b\xf0\x9f\x87\xa7\xf0\x9f\x87\xb7", &n);
    tx_composed(s, n, 2, &st, &len); eq("grafema é st", st, 1); eq("grafema é len", len, 2);
    tx_composed(s, n, 6, &st, &len); eq("bandeira st", st, 4); eq("bandeira len", len, 4);
    free(s);
    int32_t a, e, c; s = U("um\r\ndois\ntres", &n);
    tx_line(s, n, 5, 0, 0, &a, &e, &c); eq("linha start", a, 4); eq("linha end", e, 9); eq("linha cend", c, 8);
    tx_line(s, n, 0, 3, 0, &a, &e, &c); eq("crlf end", e, 4); eq("crlf cend", c, 2);
    tx_line(s, n, 11, 0, 0, &a, &e, &c); eq("ultima start", a, 9); eq("ultima end", e, 13);
    free(s);
    s = U("Olá, mundo! Tudo bem?", &n); nseg = 0; tx_segments(s, n, 1, "pt", seg, 0); eq("palavras", nseg, 4);
    nseg = 0; tx_segments(s, n, 3, "pt", seg, 0); eq("frases", nseg, 2); free(s);
    int32_t m; s = U("Ação", &n); UChar *f = tx_fold(s, n, TX_CASE | TX_DIACRITIC, &m); char *u = fdn_utf8_dup(f, m);
    if (!strcmp(u, "acao")) ok++; else { bad++; printf("FALHOU fold: %s\n", u); } free(s); free(f); free(u);
    s = U("Привет", &n); f = tx_transform(s, n, "Any-Latin", 0, &m); u = f ? fdn_utf8_dup(f, m) : strdup("(nil)");
    if (!strcmp(u, "Privet")) ok++; else { bad++; printf("FALHOU translit: %s\n", u); } free(s); free(f); free(u);
    /* partes do NSString: unidade UTF-16 sobre UTF-8 */
    { const char *t = "a\xf0\x9f\x98\x80" "b"; UChar ch = 0; size_t tot = 0;
      eq("u8 a", tx_utf8_unit_at(t, strlen(t), 0, &ch, &tot) && ch == 'a', 1);
      eq("u8 alto", tx_utf8_unit_at(t, strlen(t), 1, &ch, &tot) && ch == 0xD83D, 1);
      eq("u8 baixo", tx_utf8_unit_at(t, strlen(t), 2, &ch, &tot) && ch == 0xDE00, 1);
      eq("u8 b", tx_utf8_unit_at(t, strlen(t), 3, &ch, &tot) && ch == 'b', 1);
      eq("u8 fora", tx_utf8_unit_at(t, strlen(t), 4, &ch, &tot), 0); eq("u8 total", (long)tot, 4); }
    /* rangeOfCharacterFromSet */
    { int32_t l, m; s = U("ab\xf0\x9f\x98\x80" "c1", &n);
      eq("set dígito", tx_find_set(s, 0, n, 0, digit, 0, &l, &m) && l == 5 && m == 1, 1);
      eq("set astral", tx_find_set(s, 0, n, 0, emoji, 0, &l, &m) && l == 2 && m == 2, 1);
      eq("set astral trás", tx_find_set(s, 0, n, TX_BACKWARDS, emoji, 0, &l, &m) && l == 2 && m == 2, 1);
      eq("set âncora falha", tx_find_set(s, 0, n, TX_ANCHORED, digit, 0, &l, &m), 0);
      eq("set âncora trás", tx_find_set(s, 0, n, TX_ANCHORED | TX_BACKWARDS, digit, 0, &l, &m) && l == 5, 1);
      free(s); }
    /* enumerate */
    { int cnt; int32_t *v; s = U("um\ndois\r\ntres", &n);
      v = tx_enum(s, n, 0, n, 0, "", &cnt); eq("linhas", cnt, 3);
      eq("linha 2", v[4] == 3 && v[5] == 7 && v[6] == 3 && v[7] == 9, 1); eq("última", v[8] == 9 && v[9] == 13 && v[11] == 13, 1); free(v);
      v = tx_enum(s, n, 3, 7, 0, "", &cnt); eq("linhas na faixa", cnt == 1 && v[0] == 3 && v[1] == 7, 1); free(v); free(s);
      s = U("Oi, tudo bem?", &n); v = tx_enum(s, n, 0, n, 3, "pt", &cnt); eq("palavras", cnt, 3);
      eq("palavra 1 envolve", v[0] == 0 && v[1] == 2 && v[2] == 0 && v[3] == 4, 1);
      eq("palavra 3 envolve fim", v[8] == 9 && v[9] == 12 && v[10] == 9 && v[11] == 13, 1); free(v); free(s);
      s = U("ae\xcc\x81" "b", &n); v = tx_enum(s, n, 0, n, 2, "", &cnt); eq("grafemas", cnt, 3); eq("grafema é", v[4] == 1 && v[5] == 3, 1); free(v); free(s); }
    /* replace */
    { int32_t on, k; UChar *t, *p, *o; int32_t tn, pn; char *uu;
      s = U("a-b-c", &n); t = U("-", &tn); p = U("+-", &pn);
      o = tx_replace(s, n, 0, n, t, tn, p, pn, 0, &on, &k); uu = fdn_utf8_dup(o, on);
      eq("troca todas", k, 2); eq("troca texto", strcmp(uu, "a+-b+-c"), 0); free(o); free(uu);
      o = tx_replace(s, n, 2, 3, t, tn, p, pn, 0, &on, &k); uu = fdn_utf8_dup(o, on);
      eq("troca na faixa", k == 1 && !strcmp(uu, "a-b+-c"), 1); free(o); free(uu);
      o = tx_replace(s, n, 0, n, t, tn, p, pn, TX_ANCHORED | TX_BACKWARDS, &on, &k); eq("âncora trás sem fim", k, 0); free(o);
      free(s); free(t); free(p);
      s = U("ABC abc", &n); t = U("b", &tn); p = U("_", &pn);
      o = tx_replace(s, n, 0, n, t, tn, p, pn, TX_CASE, &on, &k); uu = fdn_utf8_dup(o, on);
      eq("troca sem caixa", k == 2 && !strcmp(uu, "A_C a_c"), 1); free(o); free(uu); free(s); free(t); free(p);
      s = U("x12y345", &n); t = U("([0-9]+)", &tn); p = U("<$1>", &pn);
      o = tx_replace(s, n, 0, n, t, tn, p, pn, TX_REGEX, &on, &k); uu = fdn_utf8_dup(o, on);
      eq("regex $1", k == 2 && !strcmp(uu, "x<12>y<345>"), 1); free(o); free(uu);
      o = tx_replace(s, n, 3, 4, t, tn, p, pn, TX_REGEX, &on, &k); uu = fdn_utf8_dup(o, on);
      if (k == 1 && !strcmp(uu, "x12y<345>")) ok++; else { bad++; printf("FALHOU regex faixa: %d %s\n", k, uu); } free(o); free(uu);
      free(s); free(t); free(p); }
    printf("fdn_text: %d/%d\n", ok, ok + bad);
    return bad != 0;
}
