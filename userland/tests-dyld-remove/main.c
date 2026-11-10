/* mOS — o callback de _dyld_register_func_for_remove_image no dlclose de um dylib sem ObjC.
 *   zig cc -target x86_64-macos -O1 -dynamiclib userland/tests-dyld-remove/librem.c -o librem.dylib
 *   zig cc -target x86_64-macos -O1 userland/tests-dyld-remove/main.c -o dyldrem \
 *       -Wl,-undefined,dynamic_lookup
 * No mOS, a saída é "dyld-remove: N/N ok". O host (Linux) não executa este Mach-O;
 * tools/dyld_remove_test.c liga o mesmo dlclose contra um SYS_DYLD simulado.
 */
#include <dlfcn.h>
#include <mach-o/dyld.h>
#include <stdio.h>
#include <string.h>

static int removed;
static const struct mach_header *removed_mh;
static intptr_t removed_slide;

static void on_remove(const struct mach_header *mh, intptr_t slide) {
    removed++;
    removed_mh = mh;
    removed_slide = slide;
}

static int ok, total;
#define CHECK(c, ...) do { total++; if (c) ok++; else { fprintf(stderr, "FALHOU: "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } } while (0)

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "librem.dylib";
    uint32_t n0, i, found = 0;
    const struct mach_header *mh = 0;
    intptr_t slide = 0;
    void *h, *h2;

    _dyld_register_func_for_remove_image(on_remove);
    n0 = _dyld_image_count();
    h = dlopen(path, RTLD_NOW);
    CHECK(h != NULL, "dlopen(%s): %s", path, dlerror());
    if (!h) { fprintf(stderr, "dyld-remove: %d/%d ok\n", ok, total); return 1; }
    for (i = n0; i < _dyld_image_count(); i++) {
        const char *nm = _dyld_get_image_name(i);
        if (nm && strstr(nm, "librem")) {
            mh = _dyld_get_image_header(i);
            slide = _dyld_get_image_vmaddr_slide(i);
            found = 1;
        }
    }
    CHECK(found && mh, "cabeçalho do librem na lista de imagens");
    CHECK(dlclose(h) == 0, "dlclose");
    CHECK(removed == 1, "remove_image chamado uma vez (%d)", removed);
    CHECK(removed_mh == mh, "remove_image recebeu o cabeçalho do dylib");
    CHECK(removed_slide == slide, "remove_image recebeu o slide (%ld vs %ld)", (long)removed_slide, (long)slide);

    removed = 0;
    h = dlopen(path, RTLD_NOW);
    h2 = dlopen(path, RTLD_NOW);
    CHECK(h && h2, "dois dlopen do mesmo dylib");
    CHECK(dlclose(h) == 0 && removed == 0, "o primeiro de dois dlclose não avisa (%d)", removed);
    CHECK(dlclose(h2) == 0 && removed == 1, "o segundo dlclose avisa remove_image (%d)", removed);

    fprintf(stderr, "dyld-remove: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
