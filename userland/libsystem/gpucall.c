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

static unsigned g_ctx, g_kind;

static int ensure_ctx(void) {
    mos_gpu_ctx_t c;
    unsigned caps;
    if (g_ctx) return 0;
    caps = mos_gpu_caps();
    memset(&c, 0, sizeof c);
    c.kind = (caps & MOS_GPU_CAP_VENUS) && !(caps & MOS_GPU_CAP_VIRGL) ? MOS_GPU_CTX_VENUS : MOS_GPU_CTX_VIRGL;
    if (mos_gpu_op(MOS_GPU_CTX_CREATE, &c, sizeof c)) return -MOS_ENOMEM;
    g_ctx = c.ctx;
    g_kind = c.kind;
    return 0;
}

static void put_u32(unsigned char *p, unsigned *o, unsigned v) {
    memcpy(p + *o, &v, 4);
    *o += 4;
}

static int bo_make(const void *src, unsigned len, unsigned *handle) {
    unsigned size;
    mos_gpu_bo_t b;
    mos_gpu_map_t m;
    int e;
    size = len ? (len + 4095u) & ~4095u : 4096u;
    if (size > (16u << 20) || (len && !src)) return -MOS_EINVAL;
    memset(&b, 0, sizeof b);
    b.size = size;
    b.flags = MOS_GPU_BO_READ | MOS_GPU_BO_WRITE;
    b.ctx = g_ctx;
    e = mos_gpu_op(MOS_GPU_BO_CREATE, &b, sizeof b);
    if (e) return e;
    memset(&m, 0, sizeof m);
    m.handle = b.handle;
    e = mos_gpu_op(MOS_GPU_BO_MAP, &m, sizeof m);
    if (e) {
        mos_gpu_op(MOS_GPU_BO_DESTROY, &b, sizeof b);
        return e;
    }
    if (len) memcpy((void *)(unsigned long)m.addr, src, len);
    mos_gpu_op(MOS_GPU_BO_UNMAP, &m, sizeof m);
    *handle = b.handle;
    return 0;
}

static void bo_drop(unsigned handle) {
    mos_gpu_bo_t b;
    memset(&b, 0, sizeof b);
    b.handle = handle;
    mos_gpu_op(MOS_GPU_BO_DESTROY, &b, sizeof b);
}

int mos_gpu_compute_submit(const mos_gpu_compute_in_t *in, unsigned *fence_out, int *on_device) {
    unsigned char batch[400];
    unsigned handles[6];
    unsigned nbo, ncmd, nreloc, at, i, total;
    unsigned caps, virgl, venus;
    const void *shader;
    unsigned shader_len;
    static const unsigned shader_pad;
    mos_gpu_fence_t fen;
    mos_gpu_submit_t sub;
    int e;
    if (fence_out) *fence_out = 0;
    if (on_device) *on_device = 0;
    if (!in || !in->nbind || in->nbind > 5) return -MOS_EINVAL;
    if (!in->local_x || !in->local_y || !in->local_z) return -MOS_EINVAL;
    if (!in->groups_x || !in->groups_y || !in->groups_z) return -MOS_EINVAL;
    if (in->groups_x > 65535 || in->groups_y > 65535 || in->groups_z > 65535) return -MOS_EINVAL;
    for (i = 0; i < in->nbind; i++) {
        const mos_gpu_bind_in_t *b = &in->binds[i];
        if (!b->bytes || !b->length || !b->range || b->slot > 31) return -MOS_EINVAL;
        if ((unsigned long long)b->offset + b->range > b->length) return -MOS_EINVAL;
    }
    shader = in->shader;
    shader_len = in->shader_len;
    if (!shader || !shader_len) {
        shader = &shader_pad;
        shader_len = 4;
    }
    e = ensure_ctx();
    if (e) return e;
    nbo = 0;
    for (i = 0; i < in->nbind; i++) {
        e = bo_make(in->binds[i].bytes, in->binds[i].length, &handles[nbo]);
        if (e) {
            while (nbo) bo_drop(handles[--nbo]);
            return e;
        }
        nbo++;
    }
    e = bo_make(shader, shader_len, &handles[nbo]);
    if (e) {
        while (nbo) bo_drop(handles[--nbo]);
        return e;
    }
    nbo++;
    ncmd = in->nbind + 2;
    nreloc = in->nbind + 1;
    at = 0;
    put_u32(batch, &at, MOS_GPU_BATCH_VER);
    put_u32(batch, &at, g_kind);
    put_u32(batch, &at, 0);
    put_u32(batch, &at, ncmd);
    put_u32(batch, &at, nbo);
    put_u32(batch, &at, nreloc);
    put_u32(batch, &at, 0);
    put_u32(batch, &at, 0);
    for (i = 0; i < nbo; i++) put_u32(batch, &at, handles[i]);
    for (i = 0; i < in->nbind; i++) {
        put_u32(batch, &at, i);
        put_u32(batch, &at, 20);
        put_u32(batch, &at, i);
        put_u32(batch, &at, in->binds[i].offset);
    }
    put_u32(batch, &at, in->nbind);
    put_u32(batch, &at, 16);
    put_u32(batch, &at, in->nbind);
    put_u32(batch, &at, 0);
    caps = mos_gpu_caps();
    virgl = g_kind == MOS_GPU_CTX_VIRGL;
    venus = g_kind == MOS_GPU_CTX_VENUS;
    for (i = 0; i < in->nbind; i++) {
        put_u32(batch, &at, MOS_GPU_CMD_BIND);
        put_u32(batch, &at, 32);
        put_u32(batch, &at, virgl ? MOS_GPU_VIRGL_SET_SHADER_BUFFERS : 0);
        put_u32(batch, &at, venus ? MOS_GPU_VENUS_BIND : 0);
        put_u32(batch, &at, in->binds[i].slot);
        put_u32(batch, &at, i);
        put_u32(batch, &at, in->binds[i].offset);
        put_u32(batch, &at, in->binds[i].range);
    }
    put_u32(batch, &at, MOS_GPU_CMD_SHADER);
    put_u32(batch, &at, 40);
    put_u32(batch, &at, virgl ? MOS_GPU_VIRGL_BIND_SHADER : 0);
    put_u32(batch, &at, venus ? MOS_GPU_VENUS_SHADER : 0);
    put_u32(batch, &at, in->nbind);
    put_u32(batch, &at, 0);
    put_u32(batch, &at, shader_len);
    put_u32(batch, &at, in->local_x);
    put_u32(batch, &at, in->local_y);
    put_u32(batch, &at, in->local_z);
    put_u32(batch, &at, MOS_GPU_CMD_DISPATCH);
    put_u32(batch, &at, 32);
    put_u32(batch, &at, virgl ? MOS_GPU_VIRGL_LAUNCH_GRID : 0);
    put_u32(batch, &at, venus ? MOS_GPU_VENUS_DISPATCH : 0);
    put_u32(batch, &at, in->groups_x);
    put_u32(batch, &at, in->groups_y);
    put_u32(batch, &at, in->groups_z);
    put_u32(batch, &at, 0);
    total = at;
    memcpy(batch + 8, &total, 4);
    if (total > 400 || (total & 3)) {
        while (nbo) bo_drop(handles[--nbo]);
        return -MOS_EINVAL;
    }
    if (in->batch_out && in->batch_n && in->batch_cap >= total) {
        memcpy(in->batch_out, batch, total);
        *in->batch_n = total;
    }
    memset(&fen, 0, sizeof fen);
    e = mos_gpu_op(MOS_GPU_FENCE_CREATE, &fen, sizeof fen);
    if (e) {
        while (nbo) bo_drop(handles[--nbo]);
        return e;
    }
    memset(&sub, 0, sizeof sub);
    sub.ctx = g_ctx;
    sub.cmd_bytes = total;
    sub.fence = fen.fence;
    sub.flags = MOS_GPU_SUBMIT_3D;
    sub.cmds = (unsigned long long)(unsigned long)batch;
    e = mos_gpu_op(MOS_GPU_SUBMIT, &sub, sizeof sub);
    if (!e) {
        fen.timeout_ms = ~0u;
        e = mos_gpu_op(MOS_GPU_FENCE_WAIT, &fen, sizeof fen);
    }
    if (fence_out && !e) *fence_out = fen.fence;
    if (on_device && !e) {
        *on_device = (g_kind == MOS_GPU_CTX_VIRGL && (caps & MOS_GPU_CAP_VIRGL)) ||
                     (g_kind == MOS_GPU_CTX_VENUS && (caps & MOS_GPU_CAP_VENUS));
    }
    while (nbo) bo_drop(handles[--nbo]);
    return e;
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
