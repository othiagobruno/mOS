/* Host: a regra de plataforma do LC_BUILD_VERSION (kernel/include/macho_buildver.h). */
#include <stdio.h>
#include "../kernel/include/macho_buildver.h"
int main(void) {
    int n = 0, ok = 0;
#define C(x) do { n++; if (x) ok++; else fprintf(stderr, "falha %s\n", #x); } while (0)
    C(mos_build_version_platform(0) == MOS_PLATFORM_MACCATALYST);
    C(mos_build_version_platform(6) == 6);
    C(mos_build_version_platform(1) == 1);
    C(mos_build_version_platform(2) == 2);
    printf("bvplat: %d/%d ok\n", ok, n);
    return ok == n ? 0 : 1;
}
