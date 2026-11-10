/* Teste de host da ABI de GPU. Não sobe kernel, QEMU nem KVM. */
#include <stdio.h>
#include <string.h>
#define MOS_GPU_HOST 1
#include "../kernel/include/mos_gpu.h"
#include "../kernel/dev/gpu_abi.c"

static int fails;
static void expect(int cond, const char *what) {
    if (cond) return;
    fprintf(stderr, "falhou: %s\n", what);
    fails++;
}

int main(void) {
    process_t a = {.pid = 1}, b = {.pid = 2};
    int blocked = 0;
    gpu_host_caps(MOS_GPU_CAP_VIRGL | MOS_GPU_CAP_KMS | MOS_GPU_CAP_IOSURF);

    mos_gpu_info_t info;
    memset(&info, 0, sizeof info);
    expect(gpu_syscall(&a, MOS_GPU_INFO, (unsigned long long)&info, sizeof info, 0, &blocked) == 0, "info");
    expect(info.abi == MOS_GPU_ABI && (info.caps & MOS_GPU_CAP_VIRGL), "caps");

    mos_gpu_ctx_t ctx = {.kind = MOS_GPU_CTX_VIRGL};
    expect(gpu_syscall(&a, MOS_GPU_CTX_CREATE, (unsigned long long)&ctx, sizeof ctx, 0, &blocked) == 0, "ctx");
    expect(ctx.ctx != 0, "ctx id");
    mos_gpu_ctx_t ven = {.kind = MOS_GPU_CTX_VENUS};
    expect(gpu_syscall(&a, MOS_GPU_CTX_CREATE, (unsigned long long)&ven, sizeof ven, 0, &blocked) == 0, "venus sem dispositivo");
    expect(gpu_syscall(&a, MOS_GPU_CTX_DESTROY, (unsigned long long)&ven, sizeof ven, 0, &blocked) == 0, "venus fim");
    expect(info.caps & MOS_GPU_CAP_IOSURF, "cap iosurface");

    mos_gpu_bo_t bo = {.size = 4096, .flags = MOS_GPU_BO_READ | MOS_GPU_BO_WRITE | MOS_GPU_BO_SHARED | MOS_GPU_BO_GPU, .ctx = ctx.ctx};
    expect(gpu_syscall(&a, MOS_GPU_BO_CREATE, (unsigned long long)&bo, sizeof bo, 0, &blocked) == 0, "bo");
    mos_gpu_map_t map = {.handle = bo.handle};
    expect(gpu_syscall(&a, MOS_GPU_BO_MAP, (unsigned long long)&map, sizeof map, 0, &blocked) == 0, "map");
    ((unsigned *)(uintptr_t)map.addr)[0] = 0x11223344u;

    mos_gpu_fence_t fen = {0};
    expect(gpu_syscall(&a, MOS_GPU_FENCE_CREATE, (unsigned long long)&fen, sizeof fen, 0, &blocked) == 0, "fence");
    expect(gpu_syscall(&a, MOS_GPU_FENCE_WAIT, (unsigned long long)&fen, sizeof fen, 0, &blocked) == -MOS_EAGAIN, "wait cedo");
    unsigned cmd = 0x01020304u;
    mos_gpu_submit_t sub = {.ctx = ctx.ctx, .cmd_bytes = 4, .fence = fen.fence, .flags = MOS_GPU_SUBMIT_3D, .cmds = (unsigned long long)&cmd};
    expect(gpu_syscall(&a, MOS_GPU_SUBMIT, (unsigned long long)&sub, sizeof sub, 0, &blocked) == 0, "submit");
    expect(gpu_syscall(&a, MOS_GPU_FENCE_WAIT, (unsigned long long)&fen, sizeof fen, 0, &blocked) == 0 && fen.seq == 1, "fence sinalizada");

    expect(gpu_syscall(&a, MOS_GPU_BO_EXPORT, (unsigned long long)&bo, sizeof bo, 0, &blocked) == 0 && bo.token, "export");
    mos_gpu_bo_t imp = {.token = bo.token};
    expect(gpu_syscall(&b, MOS_GPU_BO_IMPORT, (unsigned long long)&imp, sizeof imp, 0, &blocked) == 0, "import");
    mos_gpu_map_t mapb = {.handle = imp.handle};
    expect(gpu_syscall(&b, MOS_GPU_BO_MAP, (unsigned long long)&mapb, sizeof mapb, 0, &blocked) == 0, "map b");
    expect(((unsigned *)(uintptr_t)mapb.addr)[0] == 0x11223344u, "pagina compartilhada");

    mos_gpu_iosurface_t io = {.bo = bo.handle, .width = 16, .height = 16, .stride = 64, .format = MOS_GPU_FMT_BGRA8};
    expect(gpu_syscall(&a, MOS_GPU_IOSURF_CREATE, (unsigned long long)&io, sizeof io, 0, &blocked) == 0, "iosurf");
    expect(gpu_syscall(&a, MOS_GPU_IOSURF_EXPORT, (unsigned long long)&io, sizeof io, 0, &blocked) == 0, "iosurf export");
    mos_gpu_iosurface_t look = {.token = io.token};
    expect(gpu_syscall(&b, MOS_GPU_IOSURF_LOOKUP, (unsigned long long)&look, sizeof look, 0, &blocked) == 0, "lookup");
    expect(look.width == 16 && look.bo != 0, "lookup campos");

    mos_gpu_kms_t mode = {.connector = MOS_GPU_CONN_INTERNAL, .width = 1280, .height = 800, .stride = 1280 * 4, .format = MOS_GPU_FMT_BGRA8, .bo = bo.handle};
    expect(gpu_syscall(&a, MOS_GPU_KMS_SET, (unsigned long long)&mode, sizeof mode, 0, &blocked) == 0, "kms set");
    mos_gpu_kms_t got = {.connector = MOS_GPU_CONN_INTERNAL};
    expect(gpu_syscall(&a, MOS_GPU_KMS_GET, (unsigned long long)&got, sizeof got, 0, &blocked) == 0 && got.width == 1280, "kms get");
    mos_gpu_kms_t ext = {.connector = MOS_GPU_CONN_EXTERNAL, .width = 640, .height = 480, .stride = 2560, .format = MOS_GPU_FMT_BGRA8};
    expect(gpu_syscall(&a, MOS_GPU_KMS_SET, (unsigned long long)&ext, sizeof ext, 0, &blocked) == 0, "kms externo no host");

    expect(gpu_syscall(&a, MOS_GPU_BO_DESTROY, (unsigned long long)&bo, sizeof bo, 0, &blocked) == -MOS_EBUSY, "bo ocupado");
    expect(gpu_syscall(&a, MOS_GPU_IOSURF_DESTROY, (unsigned long long)&io, sizeof io, 0, &blocked) == 0, "iosurf fim");
    expect(gpu_syscall(&a, MOS_GPU_BO_UNMAP, (unsigned long long)&map, sizeof map, 0, &blocked) == 0, "unmap");
    expect(gpu_syscall(&a, MOS_GPU_BO_DESTROY, (unsigned long long)&bo, sizeof bo, 0, &blocked) == 0, "bo fim");
    expect(gpu_syscall(&a, MOS_GPU_CTX_DESTROY, (unsigned long long)&ctx, sizeof ctx, 0, &blocked) == 0, "ctx fim");

    if (fails) { fprintf(stderr, "%d falha(s)\n", fails); return 1; }
    printf("gpu abi ok\n");
    return 0;
}
