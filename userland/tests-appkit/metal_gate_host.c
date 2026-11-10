/* Host: o helper de GPU recebe o device; o navegador e os outros helpers não. */
#include <stdio.h>
#include "../libsystem/metal_gate.c"

static int fails;
static void expect(int cond, const char *m) {
    if (!cond) { fprintf(stderr, "falha: %s\n", m); fails++; }
}

int main(void) {
    const char *browser[] = { "/Apps/Cursor.app/Contents/MacOS/Cursor",
        "/Apps/Cursor.app/Contents/Frameworks/Electron Framework.framework/Electron Framework", 0 };
    const char *argv_b[] = { "Cursor", 0 };
    expect(cx_chromium_metal_ok(browser, argv_b) == 0, "navegador sem device");

    const char *gpu[] = { "/Apps/Cursor.app/Contents/Frameworks/Cursor Helper (GPU).app/Contents/MacOS/Cursor Helper (GPU)",
        "/Apps/Cursor.app/Contents/Frameworks/Electron Framework.framework/Electron Framework", 0 };
    expect(cx_chromium_metal_ok(gpu, argv_b) == 1, "helper GPU pelo caminho");

    const char *helper[] = { "/Apps/Code.app/Contents/Frameworks/Code Helper.app/Contents/MacOS/Code Helper",
        "/Apps/Code.app/Contents/Frameworks/Electron Framework.framework/Electron Framework", 0 };
    const char *argv_g[] = { "Code Helper", "--type=gpu", "--gpu-preferences=x", 0 };
    expect(cx_chromium_metal_ok(helper, argv_g) == 1, "helper pelo --type=gpu");

    const char *argv_r[] = { "Code Helper", "--type=renderer", 0 };
    expect(cx_chromium_metal_ok(helper, argv_r) == 0, "renderer sem device");
    const char *argv_u[] = { "Code Helper", "--type=utility", 0 };
    expect(cx_chromium_metal_ok(helper, argv_u) == 0, "utility sem device");

    const char *plain[] = { "/bin/sh", 0 };
    expect(cx_chromium_metal_ok(plain, argv_b) == 1, "app que não é Chromium");
    expect(cx_chromium_metal_ok(0, 0) == 1, "sem imagens");

    if (fails) return 1;
    printf("metal-gate: 6/6 ok\n");
    return 0;
}
