/* gcc -Wall -Wextra -I kernel/include -o /tmp/libswift_image_test tools/libswift_image_test.c */
#include <stdio.h>
#include <string.h>
#include "libswift_image.h"

static int ok, total;
#define CHECK(c, w) do { total++; if (c) ok++; else fprintf(stderr, "FALHOU: %s\n", w); } while (0)

int main(void) {
    char out[128];
    unsigned i, n = sizeof mos_libswift_image / sizeof mos_libswift_image[0];
    CHECK(n == 12, "doze libswift*");
    CHECK(mos_libswift_resolve("/usr/lib/swift/libswiftCore.dylib", out, sizeof out) == 1, "caminho canônico");
    CHECK(strcmp(out, "/usr/lib/swift/libswiftCore.dylib") == 0, "o canônico não muda");
    CHECK(mos_libswift_resolve("/usr/lib/libswiftCore.dylib", out, sizeof out) == 1, "basename fora de swift/");
    CHECK(strcmp(out, "/usr/lib/swift/libswiftCore.dylib") == 0, "vai para /usr/lib/swift");
    CHECK(mos_libswift_resolve("/usr/local/opt/swift/lib/libswiftFoundation.dylib", out, sizeof out) == 1, "não fica no Homebrew");
    CHECK(strcmp(out, "/usr/lib/swift/libswiftFoundation.dylib") == 0, "Foundation na imagem");
    CHECK(mos_libswift_resolve("/usr/lib/libz.1.dylib", out, sizeof out) == 0, "libz não é desta lista");
    CHECK(mos_libswift_resolve("/usr/lib/libcurl.4.dylib", out, sizeof out) == 0, "libcurl não é desta lista");
    CHECK(mos_libswift_resolve("/usr/lib/libcompression.dylib", out, sizeof out) == 0, "libcompression não é desta lista");
    CHECK(mos_libswift_resolve("libswiftMetal.dylib", out, sizeof out) == 1, "nome solto");
    CHECK(strcmp(out, "/usr/lib/swift/libswiftMetal.dylib") == 0, "Metal na imagem");
    for (i = 0; i < n; i++) {
        const char *p = mos_libswift_image[i];
        CHECK(strncmp(p, "/usr/lib/swift/libswift", 22) == 0, p);
        CHECK(strstr(p, ".dylib") != NULL, p);
        CHECK(strstr(p, "/usr/local") == NULL, "sem /usr/local");
        CHECK(strstr(p, "Homebrew") == NULL && strstr(p, "homebrew") == NULL, "sem Homebrew");
        CHECK(mos_libswift_resolve(p, out, sizeof out) == 1 && strcmp(out, p) == 0, p);
    }
    printf("libswift-image: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
