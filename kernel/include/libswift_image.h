/*
 * Nomes das libswift* que a imagem padrão leva em /usr/lib/swift.
 * O carregador (kernel/compat/macho.c, ao lado de usrlib_alias) resolve o
 * basename para este caminho. Não há alias Homebrew: libz, libcurl e
 * libcompression ficam na lista usrlib_alias (frente STOR-SYSDYLIBS).
 *
 * Revisão do Kernel: macho.c chama mos_libswift_resolve.
 */
#pragma once

static const char *const mos_libswift_image[] = {
    "/usr/lib/swift/libswiftCore.dylib",
    "/usr/lib/swift/libswiftFoundation.dylib",
    "/usr/lib/swift/libswiftCoreFoundation.dylib",
    "/usr/lib/swift/libswiftDarwin.dylib",
    "/usr/lib/swift/libswiftDispatch.dylib",
    "/usr/lib/swift/libswiftIOKit.dylib",
    "/usr/lib/swift/libswiftObjectiveC.dylib",
    "/usr/lib/swift/libswiftXPC.dylib",
    "/usr/lib/swift/libswift_Concurrency.dylib",
    "/usr/lib/swift/libswiftUniformTypeIdentifiers.dylib",
    "/usr/lib/swift/libswiftCoreGraphics.dylib",
    "/usr/lib/swift/libswiftMetal.dylib",
};

/* 1 se req é uma libswift* da imagem. out recebe o caminho canônico. */
static int mos_libswift_resolve(const char *req, char *out, unsigned cap) {
    const char *base, *p, *b;
    unsigned i, n;
    if (!req || !out || !cap) return 0;
    base = req;
    for (p = req; *p; p++) if (*p == '/') base = p + 1;
    if (!*base) return 0;
    for (i = 0; i < sizeof mos_libswift_image / sizeof mos_libswift_image[0]; i++) {
        b = mos_libswift_image[i];
        for (p = b; *p; p++) if (*p == '/') b = p + 1;
        for (n = 0; base[n] && base[n] == b[n]; n++) {}
        if (base[n] || b[n]) continue;
        for (n = 0; mos_libswift_image[i][n]; n++) {}
        if (n + 1 > cap) return 0;
        for (n = 0; (out[n] = mos_libswift_image[i][n]); n++) {}
        return 1;
    }
    return 0;
}
