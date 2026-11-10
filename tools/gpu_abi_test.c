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
    {
        unsigned char lote[160];
        unsigned *w;
        mos_gpu_batch_hdr_t *bh;
        mos_gpu_cmd_hdr_t *ch;
        mos_gpu_dispatch_t *dp;
        mos_gpu_reloc_t *rc;
        unsigned n;
        memset(lote, 0, sizeof lote);
        bh = (mos_gpu_batch_hdr_t *)lote;
        bh->version = MOS_GPU_BATCH_VER;
        bh->kind = MOS_GPU_CTX_VIRGL;
        bh->ncmd = 1;
        bh->nbo = 1;
        bh->nreloc = 1;
        w = (unsigned *)(lote + sizeof *bh);
        w[0] = bo.handle;
        rc = (mos_gpu_reloc_t *)(w + 1);
        ch = (mos_gpu_cmd_hdr_t *)(rc + 1);
        ch->op = MOS_GPU_CMD_DISPATCH;
        ch->bytes = (unsigned)(sizeof *ch + sizeof *dp);
        ch->virgl = MOS_GPU_VIRGL_LAUNCH_GRID;
        dp = (mos_gpu_dispatch_t *)(ch + 1);
        dp->x = dp->y = dp->z = 1;
        rc->cmd = 0;
        rc->off = (unsigned)(sizeof *ch + 12);
        rc->bo_index = 0;
        rc->bo_off = 0;
        dp->pad = 0; /* a reloc aponta para o pad, que vale o índice 0 */
        n = (unsigned)((unsigned char *)(dp + 1) - lote);
        bh->bytes = n;
        {
            unsigned bad = 0x01020304u;
            mos_gpu_submit_t ruim = {.ctx = ctx.ctx, .cmd_bytes = 4, .fence = fen.fence, .flags = MOS_GPU_SUBMIT_3D, .cmds = (unsigned long long)&bad};
            mos_gpu_submit_t sub = {.ctx = ctx.ctx, .cmd_bytes = n, .fence = fen.fence, .flags = MOS_GPU_SUBMIT_3D, .cmds = (unsigned long long)lote};
            expect(gpu_syscall(&a, MOS_GPU_SUBMIT, (unsigned long long)&ruim, sizeof ruim, 0, &blocked) == -MOS_EINVAL, "lote cru");
            expect(gpu_syscall(&a, MOS_GPU_FENCE_WAIT, (unsigned long long)&fen, sizeof fen, 0, &blocked) == -MOS_EAGAIN, "fence intacta");
            expect(gpu_syscall(&a, MOS_GPU_SUBMIT, (unsigned long long)&sub, sizeof sub, 0, &blocked) == 0, "submit");
            expect(gpu_syscall(&a, MOS_GPU_FENCE_WAIT, (unsigned long long)&fen, sizeof fen, 0, &blocked) == -MOS_EAGAIN, "fence espera o irq");
            {
                thread_t w1, w2;
                int b1 = 0, b2 = 0;
                memset(&w1, 0, sizeof w1);
                memset(&w2, 0, sizeof w2);
                fen.timeout_ms = 5;
                expect(gpu_syscall(&a, MOS_GPU_FENCE_WAIT, (unsigned long long)&fen, sizeof fen, &w1, &b1) == 0 && w1.state == 3, "waiter 1");
                expect(gpu_syscall(&a, MOS_GPU_FENCE_WAIT, (unsigned long long)&fen, sizeof fen, &w2, &b2) == 0 && w2.state == 3, "waiter 2");
                gpu_host_irq();
                expect(w1.state == 0 && w2.state == 0 && gpu_host_waiter_rax(&w1) == 0 && gpu_host_waiter_rax(&w2) == 0, "irq acorda os dois");
            }
            {
                unsigned char dev[64];
                unsigned dn = 0, w0 = 0, grid = 0;
                expect(gpu_batch_translate(a.pid, MOS_GPU_CTX_VIRGL, lote, n, dev, sizeof dev, &dn) == 0, "traduz virgl");
                memcpy(&w0, dev, 4);
                memcpy(&grid, dev + 16, 4);
                expect(w0 == (35u | (8u << 16)) && grid == 1 && dn == 36, "launch_grid");
            }
        }
    }
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

    {
        mos_gpu_ctx_t vc = {.kind = MOS_GPU_CTX_VENUS};
        mos_gpu_bo_t vb = {.size = 4096, .flags = MOS_GPU_BO_READ | MOS_GPU_BO_WRITE};
        unsigned char lote[128], dev[64];
        mos_gpu_batch_hdr_t *bh;
        mos_gpu_cmd_hdr_t *ch;
        mos_gpu_bind_t *bd;
        unsigned n, dn = 0, op = 0, res = 0;
        thread_t th;
        int blk;
        mos_gpu_fence_t fw;
        expect(gpu_syscall(&a, MOS_GPU_CTX_CREATE, (unsigned long long)&vc, sizeof vc, 0, &blocked) == 0, "ctx venus");
        expect(gpu_syscall(&a, MOS_GPU_BO_CREATE, (unsigned long long)&vb, sizeof vb, 0, &blocked) == 0, "bo venus");
        memset(lote, 0, sizeof lote);
        bh = (mos_gpu_batch_hdr_t *)lote;
        bh->version = MOS_GPU_BATCH_VER;
        bh->kind = MOS_GPU_CTX_VENUS;
        bh->ncmd = 1;
        bh->nbo = 1;
        ((unsigned *)(bh + 1))[0] = vb.handle;
        ch = (mos_gpu_cmd_hdr_t *)((unsigned *)(bh + 1) + 1);
        ch->op = MOS_GPU_CMD_BIND;
        ch->bytes = (unsigned)(sizeof *ch + sizeof *bd);
        ch->venus = MOS_GPU_VENUS_BIND;
        bd = (mos_gpu_bind_t *)(ch + 1);
        bd->slot = 2;
        bd->bo_index = 0;
        bd->offset = 0;
        bd->range = 64;
        n = (unsigned)((unsigned char *)(bd + 1) - lote);
        bh->bytes = n;
        expect(gpu_batch_translate(a.pid, MOS_GPU_CTX_VENUS, lote, n, dev, sizeof dev, &dn) == 0, "traduz venus");
        memcpy(&op, dev + 4, 4);
        memcpy(&res, dev + 12, 4);
        expect(op == MOS_GPU_VENUS_BIND && res == vb.handle && dev[0] == 5, "venus handle real");
        {
            unsigned char lote3[256], dev3[128];
            mos_gpu_batch_hdr_t *h3;
            unsigned char *p;
            mos_gpu_cmd_hdr_t *c1, *c2, *c3;
            mos_gpu_shader_t *sh;
            mos_gpu_dispatch_t *dp;
            unsigned n3, dn3 = 0, w0 = 0, wsh = 0, wgrid = 0;
            memset(lote3, 0, sizeof lote3);
            h3 = (mos_gpu_batch_hdr_t *)lote3;
            h3->version = MOS_GPU_BATCH_VER;
            h3->kind = MOS_GPU_CTX_VENUS;
            h3->ncmd = 3;
            h3->nbo = 1;
            ((unsigned *)(h3 + 1))[0] = vb.handle;
            p = (unsigned char *)((unsigned *)(h3 + 1) + 1);
            c1 = (mos_gpu_cmd_hdr_t *)p;
            c1->op = MOS_GPU_CMD_BIND; c1->bytes = (unsigned)(sizeof *c1 + sizeof(mos_gpu_bind_t)); c1->venus = MOS_GPU_VENUS_BIND;
            ((mos_gpu_bind_t *)(c1 + 1))->slot = 1;
            ((mos_gpu_bind_t *)(c1 + 1))->range = 32;
            p += c1->bytes;
            c2 = (mos_gpu_cmd_hdr_t *)p;
            c2->op = MOS_GPU_CMD_SHADER; c2->bytes = (unsigned)(sizeof *c2 + sizeof *sh); c2->venus = MOS_GPU_VENUS_SHADER;
            sh = (mos_gpu_shader_t *)(c2 + 1);
            sh->length = 64; sh->local_x = 8; sh->local_y = 1; sh->local_z = 1;
            p += c2->bytes;
            c3 = (mos_gpu_cmd_hdr_t *)p;
            c3->op = MOS_GPU_CMD_DISPATCH; c3->bytes = (unsigned)(sizeof *c3 + sizeof *dp); c3->venus = MOS_GPU_VENUS_DISPATCH;
            dp = (mos_gpu_dispatch_t *)(c3 + 1);
            dp->x = 4; dp->y = 1; dp->z = 1;
            p = (unsigned char *)(dp + 1);
            n3 = (unsigned)(p - lote3);
            h3->bytes = n3;
            expect(gpu_batch_translate(a.pid, MOS_GPU_CTX_VENUS, lote3, n3, dev3, sizeof dev3, &dn3) == 0, "venus compute");
            memcpy(&w0, dev3 + 4, 4);
            memcpy(&wsh, dev3 + 28, 4);
            memcpy(&wgrid, dev3 + dn3 - 12, 4);
            expect(w0 == MOS_GPU_VENUS_BIND && wsh == MOS_GPU_VENUS_SHADER && wgrid == 4 && dn3 == 6 * 4 + 8 * 4 + 5 * 4, "tres comandos");
        }
        memset(&fw, 0, sizeof fw);
        expect(gpu_syscall(&a, MOS_GPU_FENCE_CREATE, (unsigned long long)&fw, sizeof fw, 0, &blocked) == 0, "fence espera");
        fw.timeout_ms = 0;
        blk = 0;
        memset(&th, 0, sizeof th);
        expect(gpu_syscall(&a, MOS_GPU_FENCE_WAIT, (unsigned long long)&fw, sizeof fw, &th, &blk) == -MOS_EAGAIN && !blk, "espera zero");
        fw.timeout_ms = 5;
        expect(gpu_syscall(&a, MOS_GPU_FENCE_WAIT, (unsigned long long)&fw, sizeof fw, &th, &blk) == 0 && blk == 1 && th.state == 3, "bloqueou");
        {
            thread_t th2;
            int blk2 = 0;
            memset(&th2, 0, sizeof th2);
            expect(gpu_syscall(&a, MOS_GPU_FENCE_WAIT, (unsigned long long)&fw, sizeof fw, &th2, &blk2) == 0 && blk2 == 1 && th2.state == 3, "segundo waiter");
            expect(gpu_syscall(&a, MOS_GPU_FENCE_SIGNAL, (unsigned long long)&fw, sizeof fw, &th, &blk) == 0 && th.state == 0 && th2.state == 0, "dois acordam");
            fw.seq = 0;
            expect(gpu_syscall(&a, MOS_GPU_FENCE_CREATE, (unsigned long long)&fw, sizeof fw, 0, &blocked) == 0, "fence nova");
            fw.timeout_ms = 5;
            blk = 0;
            memset(&th, 0, sizeof th);
            memset(&th2, 0, sizeof th2);
        }
        expect(gpu_syscall(&a, MOS_GPU_FENCE_SIGNAL, (unsigned long long)&fw, sizeof fw, &th, &blk) == 0 && th.state == 0 && fw.seq == 1, "acordou");
        fw.seq = 0;
        fw.timeout_ms = 5;
        blk = 0;
        expect(gpu_syscall(&a, MOS_GPU_FENCE_CREATE, (unsigned long long)&fw, sizeof fw, 0, &blocked) == 0, "fence prazo");
        expect(gpu_syscall(&a, MOS_GPU_FENCE_WAIT, (unsigned long long)&fw, sizeof fw, &th, &blk) == 0 && blk == 1, "bloqueou prazo");
        gpu_host_fence_expire();
        expect(th.state == 0 && gpu_host_wake_rax() == -MOS_ETIMEDOUT && fw.seq == 0, "prazo");
        expect(gpu_syscall(&a, MOS_GPU_FENCE_CREATE, (unsigned long long)&fw, sizeof fw, 0, &blocked) == 0, "fence destroy");
        fw.timeout_ms = ~0u;
        blk = 0;
        expect(gpu_syscall(&a, MOS_GPU_FENCE_WAIT, (unsigned long long)&fw, sizeof fw, &th, &blk) == 0 && blk == 1, "bloqueou sem prazo");
        expect(gpu_syscall(&a, MOS_GPU_FENCE_DESTROY, (unsigned long long)&fw, sizeof fw, &th, &blk) == 0 && gpu_host_wake_rax() == -MOS_ENOENT, "destruir acorda");
        expect(gpu_syscall(&a, MOS_GPU_BO_DESTROY, (unsigned long long)&vb, sizeof vb, 0, &blocked) == 0, "bo venus fim");
        expect(gpu_syscall(&a, MOS_GPU_CTX_DESTROY, (unsigned long long)&vc, sizeof vc, 0, &blocked) == 0, "venus fim 2");
    }

    if (fails) { fprintf(stderr, "%d falha(s)\n", fails); return 1; }
    printf("gpu abi ok\n");
    return 0;
}
