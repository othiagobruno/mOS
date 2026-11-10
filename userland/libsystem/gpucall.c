/* Cliente de SYS_GPU. Sem tabela local e sem número próprio: mos_gpu.h e syscall.h. */
#include "gpucall.h"
#include "libc.h"

typedef char mos_sys_gpu_is_62[(SYS_GPU == 62) ? 1 : -1];

static int g_probed;
static unsigned g_caps;

static void probe(void) {
    mos_gpu_info_t info;
    if (g_probed) return;
    g_probed = 1;
    memset(&info, 0, sizeof info);
    if (cx_syscall(SYS_GPU, MOS_GPU_INFO, (long)&info, (long)sizeof info, 0, 0) == 0)
        g_caps = info.caps;
}

void mos_gpu_boot(void) { probe(); }

unsigned mos_gpu_caps(void) {
    probe();
    return g_caps;
}

int mos_gpu_op(unsigned op, void *st, unsigned n) {
    if (!st || !n) return -MOS_EINVAL;
    probe();
    return (int)cx_syscall(SYS_GPU, op, (long)st, (long)n, 0, 0);
}

int mos_gpu_bind_iosurface(unsigned w, unsigned h, unsigned stride, void **px, unsigned *id, unsigned *bo) {
    unsigned long long bytes, size;
    mos_gpu_bo_t b;
    mos_gpu_map_t map;
    mos_gpu_iosurface_t sf;
    if (!px || !id || !bo || !w || !h || stride < w * 4u) return -MOS_EINVAL;
    if (!(mos_gpu_caps() & MOS_GPU_CAP_IOSURF)) return -MOS_ENOENT;
    bytes = (unsigned long long)stride * h;
    size = (bytes + 4095ull) & ~4095ull;
    memset(&b, 0, sizeof b);
    b.size = size;
    b.flags = MOS_GPU_BO_READ | MOS_GPU_BO_WRITE | MOS_GPU_BO_SHARED;
    if (mos_gpu_op(MOS_GPU_BO_CREATE, &b, sizeof b)) return -MOS_ENOMEM;
    memset(&map, 0, sizeof map);
    map.handle = b.handle;
    if (mos_gpu_op(MOS_GPU_BO_MAP, &map, sizeof map)) {
        mos_gpu_op(MOS_GPU_BO_DESTROY, &b, sizeof b);
        return -MOS_ENOMEM;
    }
    memset(&sf, 0, sizeof sf);
    sf.bo = b.handle;
    sf.width = w;
    sf.height = h;
    sf.stride = stride;
    sf.format = MOS_GPU_FMT_BGRA8;
    if (mos_gpu_op(MOS_GPU_IOSURF_CREATE, &sf, sizeof sf)) {
        mos_gpu_op(MOS_GPU_BO_UNMAP, &map, sizeof map);
        mos_gpu_op(MOS_GPU_BO_DESTROY, &b, sizeof b);
        return -MOS_ENOMEM;
    }
    *px = (void *)(unsigned long)map.addr;
    *id = sf.id;
    *bo = b.handle;
    return 0;
}

void mos_gpu_unbind_iosurface(unsigned id, unsigned bo) {
    mos_gpu_iosurface_t sf;
    mos_gpu_map_t map;
    mos_gpu_bo_t b;
    memset(&sf, 0, sizeof sf);
    sf.id = id;
    mos_gpu_op(MOS_GPU_IOSURF_DESTROY, &sf, sizeof sf);
    memset(&map, 0, sizeof map);
    map.handle = bo;
    mos_gpu_op(MOS_GPU_BO_UNMAP, &map, sizeof map);
    memset(&b, 0, sizeof b);
    b.handle = bo;
    mos_gpu_op(MOS_GPU_BO_DESTROY, &b, sizeof b);
}
