/* Caminho CAMetalLayer → IOSurface da ABI, no host.
 * A syscall 62 cai em gpu_syscall (kernel/dev/gpu_abi.c, MOS_GPU_HOST).
 * Os .c do libSystem compilam com -I tools/hostinc para não misturar o
 * stdint do Linux com os typedefs de sys.h. Sem QEMU.
 */
#define MOS_GPU_HOST 1
#include "../kernel/include/mos_gpu.h"
#include "../kernel/dev/gpu_abi.c"
#include <stdio.h>
#include <string.h>

void mos_gpu_boot(void);
unsigned mos_gpu_caps(void);
int mos_gpu_op(unsigned op, void *st, unsigned n);
int mos_gpu_bind_iosurface(unsigned w, unsigned h, unsigned stride, void **px, unsigned *id, unsigned *bo);
void mos_gpu_unbind_iosurface(unsigned id, unsigned bo);
void host_spirv_test(int *fails);

static process_t proc = {.pid = 7};

long cx_syscall(long n, long a, long b, long c, long d, long e) {
    int blocked = 0;
    (void)d;
    (void)e;
    if (n != 62) return -38;
    return gpu_syscall(&proc, (unsigned long long)a, (unsigned long long)b, (unsigned long long)c, 0, &blocked);
}
static int fails;
static void check(int cond, const char *msg) {
    if (cond) { printf("ok  %s\n", msg); return; }
    printf("falha  %s\n", msg);
    fails++;
}

/* O que IOSurfaceCreate faz para o drawable BGRA do CAMetalLayer. */
static void test_layer(void) {
    void *px = 0;
    unsigned id = 0, bo = 0;
    mos_gpu_iosurface_t look;
    unsigned w = 8, h = 4, stride = 32;
    gpu_host_caps(0);
    mos_gpu_boot();
    check((mos_gpu_caps() & MOS_GPU_CAP_IOSURF) != 0, "info anuncia IOSurface");
    check(mos_gpu_bind_iosurface(w, h, stride, &px, &id, &bo) == 0 && px && id && bo, "bind do drawable");
    ((unsigned *)px)[0] = 0xAABBCCDDu;
    memset(&look, 0, sizeof look);
    look.id = id;
    check(mos_gpu_op(MOS_GPU_IOSURF_LOOKUP, &look, sizeof look) == 0, "lookup");
    check(look.id == id && look.bo == bo && look.width == w && look.height == h && look.stride == stride, "mesmo id e BO");
    check(look.format == MOS_GPU_FMT_BGRA8 && ((unsigned *)px)[0] == 0xAABBCCDDu, "pixels no mapeamento da ABI");
    mos_gpu_unbind_iosurface(id, bo);
    memset(&look, 0, sizeof look);
    look.id = id;
    check(mos_gpu_op(MOS_GPU_IOSURF_LOOKUP, &look, sizeof look) != 0, "unbind tira a superfície");
}

int main(void) {
    test_layer();
    host_spirv_test(&fails);
    if (fails) { printf("%d falhas\n", fails); return 1; }
    printf("host-metal-gpu ok\n");
    return 0;
}
