/* O que o machocheck do OpenBot ainda marca em sistema depois da main.
 * DNS (#73) e o unwinding C++ ficam nas outras frentes. gap_stub devolve 0. */
#include "foundation.h"
#include "libc.h"
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; unsigned long long flags; const char *s; unsigned long long n; } gap_cf_t;
#define GAPCF(var, text) \
    static gap_cf_t var##_box = {(void *)cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; \
    const void *var = &var##_box;
GAPCF(kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly, "kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly")
GAPCF(AVNumberOfChannelsKey, "AVNumberOfChannelsKey")
/* CGImageDestination (PNG/JPEG do OpenBot) está em imageio.c. */

/* As CF* que estavam aqui estão em cf_public.c, vscode_cf.c, cfrunloop.c e slack_frameworks.c.
 * As 19 pontes Swift de Foundation estão em foundation_swift_data.c. */
