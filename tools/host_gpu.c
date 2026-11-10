/* SYS_GPU (62) no host: o mesmo gpu_syscall do kernel, no modo MOS_GPU_HOST. */
#define MOS_GPU_HOST 1
#include "../kernel/include/mos_gpu.h"
#include "../kernel/dev/gpu_abi.c"

long host_gpu_op(unsigned long long op, unsigned long long user, unsigned long long len, int *blocked) {
    static process_t proc = { .pid = 42, .mmap_next = 0 };
    return gpu_syscall(&proc, op, user, len, 0, blocked);
}
