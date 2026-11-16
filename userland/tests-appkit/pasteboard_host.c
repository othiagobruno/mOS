/* Teste de host do núcleo do NSPasteboard. Roda no Linux, sem VM:
 *   gcc -std=c11 -Wall -Wextra -DCX_PBK_HOST -Iuserland/libsystem \
 *       userland/libsystem/appkit_pb_core.c userland/tests-appkit/pasteboard_host.c \
 *       -o /tmp/appkit-pasteboard-test && /tmp/appkit-pasteboard-test */
#include "appkit_pb_core.h"
#include <stdio.h>
#include <string.h>

static int ok, total;
static void check(int c, const char *w) { total++; if (c) ok++; printf("%s %s\n", c ? "  ok  " : "  FALHA", w); }

int main(void) {
    check(!strcmp(pbk_canonical("NSStringPboardType"), "public.utf8-plain-text"), "NSStringPboardType = texto UTF-8");
    check(!strcmp(pbk_canonical("Apple URL pasteboard type"), "public.url"), "NSURLPboardType = public.url");
    check(!strcmp(pbk_canonical("NeXT TIFF v4.0 pasteboard type"), "public.tiff"), "TIFF legado");
    check(!strcmp(pbk_canonical("public.png"), "public.png"), "UTI moderna fica igual");
    check(!strcmp(pbk_canonical("com.exemplo.x"), "com.exemplo.x"), "tipo do app fica igual");
    check(pbk_same("NSRTFPboardType", "public.rtf") && !pbk_same("public.rtf", "public.html"), "mesmo tipo depois de canonizar");
    check(pbk_is_filenames("NSFilenamesPboardType") && !pbk_is_filenames("public.file-url"), "NSFilenamesPboardType");
    check(pbk_is_text("public.html") && pbk_is_text("NSStringPboardType") && !pbk_is_text("public.png"), "tipos de texto");
    char b[256];
    check(pbk_path_to_url("/Users/ana/Meu arquivo.txt", b, sizeof b) && !strcmp(b, "file:///Users/ana/Meu%20arquivo.txt"), "espaço vira %20");
    check(pbk_path_to_url("/a/ç#?", b, sizeof b) && !strcmp(b, "file:///a/%C3%A7%23%3F"), "UTF-8, # e ? escapados");
    check(!pbk_path_to_url("relativo", b, sizeof b), "caminho relativo não vira URL");
    check(!pbk_path_to_url("/muito/longo", b, 12), "não coube");
    check(pbk_url_to_path("file:///Users/ana/Meu%20arquivo.txt", b, sizeof b) && !strcmp(b, "/Users/ana/Meu arquivo.txt"), "volta para o caminho");
    check(pbk_url_to_path("file://localhost/tmp/x", b, sizeof b) && !strcmp(b, "/tmp/x"), "file://localhost/");
    check(pbk_url_to_path("file:///tmp/pasta/", b, sizeof b) && !strcmp(b, "/tmp/pasta"), "pasta sem barra final");
    check(pbk_url_to_path("file:///", b, sizeof b) && !strcmp(b, "/"), "a raiz");
    check(!pbk_url_to_path("https://exemplo.com/a", b, sizeof b), "URL http não é arquivo");
    check(pbk_url_to_path("file:///a/%C3%A7", b, sizeof b) && !strcmp(b, "/a/\xC3\xA7"), "decodifica UTF-8");
    printf("appkit-pasteboard: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
