/* ct_layout.c no host, com a ICU do Linux e sem ela (CT_LAYOUT_NOICU=1):
 *   gcc -O1 -DCT_LAYOUT_HOST -Iuserland/libsystem tools/ct_layout_host.c userland/libsystem/ct_layout.c -ldl -o /tmp/ct_layout_host
 *   /tmp/ct_layout_host && CT_LAYOUT_NOICU=1 /tmp/ct_layout_host simples */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "ct_layout.h"
static int fails;
#define CHECK(c, m) do { int ok_ = (c); printf("%s %s\n", ok_ ? "ok  " : "FAIL", m); if (!ok_) fails++; } while (0)
static long u16of(const char *u8, uint16_t *o) {
    long k = 0;
    for (const unsigned char *p = (const unsigned char *)u8; *p;) {
        uint32_t c = *p, ex = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
        c = ex == 3 ? c & 7 : ex == 2 ? c & 15 : ex == 1 ? c & 31 : c;
        p++;
        for (uint32_t e = 0; e < ex && *p; e++, p++) c = c << 6 | (*p & 63);
        if (c >= 0x10000) { c -= 0x10000; o[k++] = (uint16_t)(0xD800 + (c >> 10)); o[k++] = (uint16_t)(0xDC00 + (c & 1023)); }
        else o[k++] = (uint16_t)c;
    }
    return k;
}
/* posições de quebra como string "i,j,..." */
static void list(const uint8_t *v, long n, int want, char *out) {
    out[0] = 0;
    for (long i = 1; i <= n; i++) if (v[i] == want || (want == 3 && v[i])) sprintf(out + strlen(out), "%s%ld", out[0] ? "," : "", i);
}
/* texto visual de uma linha inteira */
static void visual(const char *u8, int para, char *out) {
    uint16_t s[256]; uint8_t lv[256]; int map[256];
    long n = u16of(u8, s);
    int pl = ctl_bidi_levels(s, n, para, lv);
    ctl_reorder_line(s, lv, 0, n, pl, map);
    long k = 0;
    for (long i = 0; i < n; i++) {
        uint32_t c = s[map[i]];
        if (c < 0x80) out[k++] = (char)c;
        else if (c < 0x800) { out[k++] = (char)(0xC0 | c >> 6); out[k++] = (char)(0x80 | (c & 63)); }
        else { out[k++] = (char)(0xE0 | c >> 12); out[k++] = (char)(0x80 | (c >> 6 & 63)); out[k++] = (char)(0x80 | (c & 63)); }
    }
    out[k] = 0;
}
int main(int argc, char **argv) {
    int simple = argc > 1;
    printf("ICU: %s\n", ctl_has_icu() ? "sim" : "não");
    uint16_t s[256]; uint8_t b[257]; char got[512];
    long n = u16of("Hello world, this is fine.\nNext", s);
    ctl_line_breaks(s, n, b);
    list(b, n, 1, got);
    CHECK(!strcmp(got, "6,13,18,21,31"), "quebra permitida depois dos espaços e no fim");
    if (strcmp(got, "6,13,18,21,31")) printf("     %s\n", got);
    list(b, n, 2, got);
    CHECK(!strcmp(got, "27"), "quebra obrigatória depois do \\n");
    if (!simple) {
        n = u16of("http://a.b/c-d e", s);
        ctl_line_breaks(s, n, b);
        CHECK(b[15] == 1 && b[5] == 0 && b[6] == 0, "URL: não quebra em \"://\"; quebra no espaço");
        n = u16of("漢字かな", s);
        ctl_line_breaks(s, n, b);
        list(b, n, 3, got);
        CHECK(!strcmp(got, "1,2,3,4"), "CJK: quebra entre ideogramas");
        n = u16of("(abc) 123-456", s);
        ctl_line_breaks(s, n, b);
        CHECK(b[1] == 0 && b[6] == 1, "não quebra depois de \"(\"");
    }
    n = u16of("e\xCC\x81x\xF0\x9F\x91\xA8\xE2\x80\x8D\xF0\x9F\x91\xA9" "a", s);   /* e + acento, x, homem ZWJ mulher, a */
    ctl_clusters(s, n, b);
    list(b, n, 3, got);
    CHECK(!strcmp(got, "2,3,8,9"), "grafemas: e+acento, x, família com ZWJ, a");
    if (strcmp(got, "2,3,8,9")) printf("     %s\n", got);
    visual("abc \xD7\xA9\xD7\x9C\xD7\x95\xD7\x9D def", 0, got);   /* abc שלום def */
    CHECK(!strcmp(got, "abc \xD7\x9D\xD7\x95\xD7\x9C\xD7\xA9 def"), "bidi: hebraico invertido dentro de LTR");
    visual("\xD7\xA9\xD7\x9C\xD7\x95\xD7\x9D 123 abc", -1, got);   /* שלום 123 abc, parágrafo RTL pelo primeiro forte */
    CHECK(!strcmp(got, "abc 123 \xD7\x9D\xD7\x95\xD7\x9C\xD7\xA9"), "bidi: parágrafo RTL, número e latino mantêm a ordem interna");
    if (strcmp(got, "abc 123 \xD7\x9D\xD7\x95\xD7\x9C\xD7\xA9")) printf("     %s\n", got);
    if (!simple) {
        visual("a \xE2\x80\xAB\xD7\x90 b\xE2\x80\xAC c", 0, got);    /* RLE ... PDF: embedding */
        CHECK(strstr(got, "b") < strstr(got, "\xD7\x90"), "bidi: embedding RLE/PDF");
        visual("abc \xE2\x81\xA7\xD7\x90\xD7\x91 1\xE2\x81\xA9 d", 0, got);   /* RLI ... PDI: isolate */
        CHECK(strstr(got, "1") < strstr(got, "\xD7\x91"), "bidi: isolate RLI/PDI");
    }
    printf("%d falha(s)\n", fails);
    return fails ? 1 : 0;
}
