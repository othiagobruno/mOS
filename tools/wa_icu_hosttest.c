/* Teste de host das funções ICU do WhatsApp (userland/libsystem/wa_icu.c).
 * gcc -DWA_ICU_HOST -O1 tools/wa_icu_hosttest.c userland/libsystem/wa_icu.c -o /tmp/wa_icu && /tmp/wa_icu */
#include <stdio.h>
#include <string.h>
typedef unsigned short UChar;
UChar *u_strFromUTF8WithSub(UChar *d, int cap, int *plen, const char *s, int n, int sub, int *nsub, int *err);
char *u_strToUTF8WithSub(char *d, int cap, int *plen, const UChar *s, int n, int sub, int *nsub, int *err);
int u_tolower(int c);
int u_ispunct(int c);
int u_hasBinaryProperty(int c, int which);
int u_getIntPropertyValue(int c, int which);
static int fails, oks;
#define CHECK(c, m) do { if (c) oks++; else { printf("FALHOU %s\n", m); fails++; } } while (0)
int main(void) {
    UChar w[32];
    char b[64];
    int len = -1, subs = -1, err = 0;
    /* "Olá 😀" = O l á espaço U+1F600 -> 6 unidades UTF-16 */
    const char *s = "Ol\xC3\xA1 \xF0\x9F\x98\x80";
    u_strFromUTF8WithSub(w, 32, &len, s, -1, 0xFFFD, &subs, &err);
    CHECK(err == 0 && len == 6 && subs == 0 && w[2] == 0xE1 && w[4] == 0xD83D && w[5] == 0xDE00 && w[6] == 0, "UTF-8 -> UTF-16");
    err = 0;
    u_strToUTF8WithSub(b, 64, &len, w, 6, 0xFFFD, &subs, &err);
    CHECK(err == 0 && len == (int)strlen(s) && !strcmp(b, s), "UTF-16 -> UTF-8 ida e volta");
    err = 0;
    u_strFromUTF8WithSub(w, 32, &len, "a\xFF" "b\xC3", -1, 0xFFFD, &subs, &err);
    CHECK(err == 0 && len == 4 && subs == 2 && w[1] == 0xFFFD && w[3] == 0xFFFD, "UTF-8 inválido vira U+FFFD");
    err = 0;
    u_strFromUTF8WithSub(0, 0, &len, s, -1, 0xFFFD, 0, &err);
    CHECK(err == 15 && len == 6, "pre-flight devolve o tamanho e BUFFER_OVERFLOW");
    UChar lone[2] = {0xD800, 'x'};
    err = 0;
    u_strToUTF8WithSub(b, 64, &len, lone, 2, '?', &subs, &err);
    CHECK(err == 0 && subs == 1 && !strcmp(b, "?x"), "surrogate sozinho substituído");
    CHECK(u_tolower('A') == 'a' && u_tolower(0xC9) == 0xE9 && u_tolower(0x410) == 0x430 && u_tolower(0x391) == 0x3B1 && u_tolower(0x15A) == 0x15B, "u_tolower");
    CHECK(u_ispunct('!') && u_ispunct(0xBF) && u_ispunct(0x3002) && !u_ispunct('a') && !u_ispunct('+'), "u_ispunct");
    CHECK(u_hasBinaryProperty(0x1F600, 57) && u_hasBinaryProperty(0x1F600, 58) && !u_hasBinaryProperty('a', 57), "Emoji / Emoji_Presentation");
    CHECK(u_hasBinaryProperty(0x1F3FD, 59) && u_hasBinaryProperty(0x1F44D, 60) && u_hasBinaryProperty(0x200D, 61), "modificador, base e componente");
    CHECK(u_hasBinaryProperty(0x1F1E7, 62) && u_hasBinaryProperty(0x2764, 64) && u_hasBinaryProperty(0x20, 31), "regional, pictográfico, espaço");
    CHECK(u_getIntPropertyValue(0x4E2D, 0x1004) == 5 && u_getIntPropertyValue('a', 0x1004) == 4 && u_getIntPropertyValue(0xFF21, 0x1004) == 3, "East_Asian_Width");
    CHECK(u_getIntPropertyValue('A', 0x1005) == 1 && u_getIntPropertyValue('a', 0x1005) == 2 && u_getIntPropertyValue('5', 0x1005) == 9, "General_Category");
    printf("wa_icu: %d/%d ok\n", oks, oks + fails);
    return fails != 0;
}
