/*
 * mOS — virtio-gpu (QEMU "-vga virtio"): primeira GPU com fila de comandos de verdade.
 *
 * Serve de bancada para a arquitetura dos drivers de GPU (Intel/AMD): o mOS cria um recurso na GPU,
 * prende a ele, como memória de apoio, a própria superfície onde o compositor desenha (g_screen, contígua
 * no mapa identidade) e a liga à saída de vídeo. A partir daí, apresentar um quadro não copia pixel
 * nenhum na CPU: só envia TRANSFER_TO_HOST_2D + RESOURCE_FLUSH do retângulo sujo.
 *
 * Transporte: virtio 1.x moderno sobre PCI (capacidades de fabricante 0x09: common, notify, device),
 * uma fila de controle (split ring), sem interrupções: cada lote é enviado e esperado por polling.
 * Qualquer falha antes da troca de saída mantém o framebuffer do UEFI, como antes.
 */
#include "gpu.h"
#include "gfx.h"
#include "syscall.h"

/* ---- comandos e respostas (virtio_gpu.h) */
enum {
    CMD_GET_DISPLAY_INFO = 0x0100, CMD_RESOURCE_CREATE_2D, CMD_RESOURCE_UNREF, CMD_SET_SCANOUT,
    CMD_RESOURCE_FLUSH, CMD_TRANSFER_TO_HOST_2D, CMD_RESOURCE_ATTACH_BACKING,
    RESP_OK_NODATA = 0x1100, RESP_OK_DISPLAY_INFO = 0x1101,
};
#define FORMAT_B8G8R8X8 2               /* bytes B,G,R,X = u32 0xXXRRGGBB, o formato das superfícies */

typedef struct PACKED { u32 type, flags; u64 fence; u32 ctx; u8 ring, pad[3]; } hdr_t;
typedef struct PACKED { u32 x, y, w, h; } vrect_t;
typedef struct PACKED { hdr_t h; struct PACKED { vrect_t r; u32 enabled, flags; } pm[16]; } display_info_t;
typedef struct PACKED { hdr_t h; u32 id, format, w, h2; } create_2d_t;
typedef struct PACKED { hdr_t h; u32 id, n; u64 addr; u32 len, pad; } attach_t;
typedef struct PACKED { hdr_t h; vrect_t r; u32 scanout, id; } set_scanout_t;
typedef struct PACKED { hdr_t h; vrect_t r; u32 id, pad; } flush_t;
typedef struct PACKED { hdr_t h; vrect_t r; u64 off; u32 id, pad; } transfer_t;

/* ---- virtqueue (split) */
#define VQ_MAX   64
#define SLOT     1024                   /* por comando no lote: pedido em +0, resposta em +RESP */
#define RESP     512
#define MAX_BATCH 4
typedef struct PACKED { u64 addr; u32 len; u16 flags, next; } vq_desc_t;
typedef struct PACKED { u16 flags, idx; u16 ring[VQ_MAX]; } vq_avail_t;
typedef struct PACKED { u16 flags, idx; struct PACKED { u32 id, len; } ring[VQ_MAX]; } vq_used_t;

static volatile u8 *common, *notify, *devcfg;
static u32 outs[MAX_DISPLAYS - 1];       /* saída virtio de cada monitor extra (monitor k = outs[k-1]) */
static int nouts;
static u32 notify_mul;
static vq_desc_t *desc;
static vq_avail_t *avail;
static volatile vq_used_t *used;
static u16 qsize, avail_idx;
static u8 *cmd; static u64 cmd_phys;
static bool ready, scanout_on, broken;
static u32 feats;                       /* bits de recurso da GPU (word 0): VIRGL, BLOB, CONTEXT_INIT */
static const u32 *bound_px;              /* superfície presa ao recurso */
static int res_w, res_h;

static inline u8  c8(u32 o)  { return mmio_r8(common, o); }
static inline u16 c16(u32 o) { return mmio_r16(common, o); }
static inline u32 c32(u32 o) { return mmio_r32(common, o); }
static inline void w8(u32 o, u8 v)   { mmio_w8(common, o, v); }
static inline void w16(u32 o, u16 v) { mmio_w16(common, o, v); }
static inline void w32(u32 o, u32 v) { mmio_w32(common, o, v); }
static inline void w64(u32 o, u64 v) { w32(o, (u32)v); w32(o + 4, (u32)(v >> 32)); }

/* envia n comandos (cada um: pedido de rlen[i] bytes já escrito em cmd + i*SLOT) e espera todos */
static bool submit(int n, const u32 *rlen) {
    for (int i = 0; i < n; i++) {
        vq_desc_t *d0 = &desc[2 * i], *d1 = &desc[2 * i + 1];
        d0->addr = cmd_phys + (u64)i * SLOT;       d0->len = rlen[i]; d0->flags = 1; d0->next = (u16)(2 * i + 1);
        d1->addr = cmd_phys + (u64)i * SLOT + RESP; d1->len = SLOT - RESP;     d1->flags = 2; d1->next = 0;
        memset(cmd + i * SLOT + RESP, 0, sizeof(hdr_t));
        avail->ring[(avail_idx + i) % qsize] = (u16)(2 * i);
    }
    __asm__ volatile("mfence" ::: "memory");
    avail_idx += (u16)n;
    *(volatile u16 *)((u8 *)avail + 2) = avail_idx;   /* avail->idx (offset 2), sem endereço de campo packed */
    __asm__ volatile("mfence" ::: "memory");
    mmio_w16(notify, 0, 0);                          /* fila 0 */
    for (u64 spin = 0; used->idx != avail_idx; spin++) {
        if (spin > 400000000ull) { kprintf("virtio-gpu: a GPU não respondeu, desligando\n"); broken = true; return false; }
        __asm__ volatile("pause");
    }
    for (int i = 0; i < n; i++) {
        u32 t = ((hdr_t *)(cmd + i * SLOT + RESP))->type;
        if (t != RESP_OK_NODATA && t != RESP_OK_DISPLAY_INFO) {
            kprintf("virtio-gpu: comando %x recusado (resposta %x)\n", ((hdr_t *)(cmd + i * SLOT))->type, t);
            return false;
        }
    }
    return true;
}

static void *req(int i, u32 type, usize size) {
    void *p = cmd + i * SLOT;
    memset(p, 0, size);
    ((hdr_t *)p)->type = type;
    return p;
}

bool virtio_gpu_probe(gpu_info_t *g) {
    pci_dev_t *d = g->pci;
    g->maker = "virtio";
    g->family = "virtio-gpu";
    g->arch = "2D";
    g->aperture = pci_bar_addr(d, 0);               /* virtio-vga: o framebuffer do UEFI fica no BAR0 */
    g->pipe = -1;
    if (!(pci_read(d, 0x04) & (1u << 20))) return false;   /* sem lista de capacidades */
    pci_enable_bus_master(d);
    for (u8 p = pci_read(d, 0x34) & 0xFC, guard = 0; p && guard < 48; guard++) {
        u32 h = pci_read(d, p);
        u8 next = (h >> 8) & 0xFC, type = h >> 24, bar = pci_read(d, p + 4) & 0xFF;
        if ((h & 0xFF) == 0x09 && bar < 6) {
            u64 base = pci_bar_addr(d, bar);
            u32 off = pci_read(d, p + 8), len = pci_read(d, p + 12);
            if (base && len) {
                vmm_map_mmio(base + off, len);
                volatile u8 *a = (volatile u8 *)(base + off);
                if (type == 1) common = a;
                else if (type == 2) { notify = a; notify_mul = pci_read(d, p + 16); }
                else if (type == 4) devcfg = a;
            }
        }
        p = next;
    }
    if (!common || !notify) { kprintf("virtio-gpu: capacidades virtio 1.x ausentes\n"); return false; }

    w8(0x14, 0);                                    /* reset */
    for (int i = 0; i < 1000000 && c8(0x14); i++) __asm__ volatile("pause");
    w8(0x14, 1); w8(0x14, 1 | 2);                   /* ACKNOWLEDGE | DRIVER */
    w32(0x00, 0);
    u32 dev0 = c32(0x04);
    feats = 0;
    if (dev0 & 1) feats |= 1;                       /* VIRTIO_GPU_F_VIRGL */
    if (dev0 & (1u << 3)) feats |= 1u << 3;         /* RESOURCE_BLOB (venus) */
    if (dev0 & (1u << 4)) feats |= 1u << 4;         /* CONTEXT_INIT (venus) */
    w32(0x00, 1);
    if (!(c32(0x04) & 1)) { kprintf("virtio-gpu: sem VIRTIO_F_VERSION_1\n"); w8(0x14, 0x80); return false; }
    w32(0x08, 0); w32(0x0C, feats);
    w32(0x08, 1); w32(0x0C, 1);                     /* VERSION_1 */
    w8(0x14, 1 | 2 | 8);
    if (!(c8(0x14) & 8)) { kprintf("virtio-gpu: recursos recusados\n"); w8(0x14, 0x80); return false; }

    w16(0x16, 0);                                   /* fila de controle */
    u16 max = c16(0x18);
    qsize = MIN(max, (u16)VQ_MAX);
    if (qsize < 2 * MAX_BATCH) { kprintf("virtio-gpu: fila pequena demais (%u)\n", max); w8(0x14, 0x80); return false; }
    w16(0x18, qsize);
    w16(0x1A, 0xFFFF);                              /* sem vetor MSI-X */
    u64 dp, ap, up;
    desc = dma_alloc(4096, &dp);
    avail = dma_alloc(4096, &ap);
    used = dma_alloc(4096, &up);
    cmd = dma_alloc(4096, &cmd_phys);
    if (!desc || !avail || !used || !cmd) { w8(0x14, 0x80); return false; }
    w64(0x20, dp); w64(0x28, ap); w64(0x30, up);
    notify += (u32)c16(0x1E) * notify_mul;
    avail->flags = 1;                               /* sem interrupções: polling */
    w16(0x1C, 1);
    w8(0x14, 1 | 2 | 4 | 8);                        /* DRIVER_OK */
    ready = true;

    req(0, CMD_GET_DISPLAY_INFO, sizeof(hdr_t));
    u32 len = sizeof(hdr_t);
    if (!submit(1, &len)) { ready = false; return false; }
    display_info_t *di = (display_info_t *)(cmd + RESP);
    g->mode_w = di->pm[0].r.w; g->mode_h = di->pm[0].r.h;
    u32 nscan = devcfg ? MIN(mmio_r32(devcfg, 8), 16u) : 1;   /* virtio_gpu_config.num_scanouts */
    kprintf("gpu: virtio-gpu pronto (fila de %u, %u saída(s), saída 0 %s %ux%u); a apresentação passa a ser sem cópia\n", qsize,
            nscan, di->pm[0].enabled ? "ligada" : "desligada", di->pm[0].r.w, di->pm[0].r.h);

    /* monitores extras: as saídas 1.. que o host diz estarem ligadas */
    for (u32 i = 1; i < nscan && nouts < MAX_DISPLAYS - 1; i++) {
        if (!di->pm[i].enabled || !di->pm[i].r.w || !di->pm[i].r.h) continue;
        char name[48];
        snprintf(name, sizeof name, "Monitor externo %u (virtio)", i);
        if (display_add((int)di->pm[i].r.w, (int)di->pm[i].r.h, name)) outs[nouts++] = i;
    }
    /* teste sem janela (QEMU -display none não liga as saídas extras): \mOS\Sistema\Biblioteca\monitor-extra
     * com "LxA" força a saída 1 com esse tamanho */
    if (!nouts && nscan > 1)
        for (u32 k = 0; k < g_boot->file_count; k++) {
            boot_file_t *bf = &g_boot->files[k];
            usize n = strlen(bf->path);
            if (n < 14 || strcmp(bf->path + n - 14, "/monitor-extra") != 0 || !bf->data || bf->size == ~0ULL) continue;
            char txt[24] = {0};
            memcpy(txt, bf->data, MIN(bf->size, (u64)sizeof txt - 1));
            char *e;
            int fw = (int)strtol(txt, &e, 10), fh = (*e == 'x' || *e == 'X') ? (int)strtol(e + 1, NULL, 10) : 0;
            if (display_add(fw, fh, "Monitor externo 1 (virtio, forçado)")) outs[nouts++] = 1;
            break;
        }
    return true;
}

/* cria um recurso com a superfície do compositor como memória de apoio e aponta cada saída para o pedaço
 * dela que é o seu monitor (display_scanout: no espelho, todas mostram o principal). A cada mudança de
 * arranjo é um recurso novo, porque a superfície muda de tamanho e de lugar; o anterior é liberado. */
static u32 res_id;
static bool rebind;

void gpu_displays_changed(void) { rebind = true; }

static bool bind_screen(const u32 *px, int w, int h) {
    u32 id = res_id + 1, old = res_id;
    create_2d_t *c = req(0, CMD_RESOURCE_CREATE_2D, sizeof *c);
    c->id = id; c->format = FORMAT_B8G8R8X8; c->w = (u32)w; c->h2 = (u32)h;
    attach_t *a = req(1, CMD_RESOURCE_ATTACH_BACKING, sizeof *a);
    a->id = id; a->n = 1; a->addr = (u64)px; a->len = (u32)w * (u32)h * 4;
    u32 lens[2] = { sizeof *c, sizeof *a };
    if (!submit(2, lens)) return false;
    for (int k = 0; k <= nouts; k++) {               /* saída k do mOS = scanout 0 (k = 0) ou outs[k-1] */
        rect_t r;
        set_scanout_t *s = req(0, CMD_SET_SCANOUT, sizeof *s);
        s->scanout = k ? outs[k - 1] : 0;
        if (display_scanout(k, &r)) { s->r.x = (u32)r.x; s->r.y = (u32)r.y; s->r.w = (u32)r.w; s->r.h = (u32)r.h; s->id = id; }
        u32 l = sizeof *s;
        if (!submit(1, &l)) { if (!k) return false; kprintf("gpu: saída %u recusou o monitor\n", s->scanout); continue; }
        if (!k) scanout_on = true;
    }
    if (old) {
        hdr_t *u = req(0, CMD_RESOURCE_UNREF, sizeof(hdr_t) + 8);
        *(u32 *)(u + 1) = old;
        u32 l = sizeof(hdr_t) + 8;
        submit(1, &l);
    }
    res_id = id;
    bound_px = px; res_w = w; res_h = h;
    kprintf("gpu: saída de vídeo agora vem do recurso %u (%dx%d, %d monitor(es)) preso à superfície do compositor\n",
            id, w, h, display_count());
    return true;
}

bool gpu_present(const u32 *px, int w, int h, int x, int y, int rw, int rh) {
    if (!ready || broken || !px || rw <= 0 || rh <= 0) return false;
    if (!scanout_on || rebind || px != bound_px || w != res_w || h != res_h) {
        if (!pmm_is_ram((u64)px, (u64)w * h * 4) || !bind_screen(px, w, h)) { if (!scanout_on) broken = true; return false; }
        rebind = false;
        x = 0; y = 0; rw = w; rh = h;               /* recurso novo: a área inteira */
    }
    transfer_t *t = req(0, CMD_TRANSFER_TO_HOST_2D, sizeof *t);
    t->r.x = (u32)x; t->r.y = (u32)y; t->r.w = (u32)rw; t->r.h = (u32)rh;
    t->off = ((u64)y * (u64)w + (u64)x) * 4;
    t->id = res_id;
    flush_t *f = req(1, CMD_RESOURCE_FLUSH, sizeof *f);
    f->r = t->r;
    f->id = res_id;
    u32 lens[2] = { sizeof *t, sizeof *f };
    return submit(2, lens);
}

u32 virtio_gpu_caps(void) {
    u32 c = 0;
    if (!ready || broken) return 0;
    if (feats & 1) c |= MOS_GPU_CAP_VIRGL;
    if ((feats & (1u << 3)) && (feats & (1u << 4))) c |= MOS_GPU_CAP_VENUS;
    return c;
}

static int xmit(void *pkt, u32 n) {
    if (!ready || broken || n > RESP - 16) return -MOS_EINVAL;
    memcpy(cmd, pkt, n);
    u32 len = n;
    if (!submit(1, &len)) return -MOS_EIO;
    return 0;
}

int virtio_gpu_ctx(unsigned id, unsigned kind, int create) {
    if (!ready || broken) return -MOS_ENXIO;
    if (create && kind == MOS_GPU_CTX_VENUS && !(virtio_gpu_caps() & MOS_GPU_CAP_VENUS)) return -MOS_ENXIO;
    if (create && kind == MOS_GPU_CTX_VIRGL && !(feats & 1)) return -MOS_ENXIO;
    u8 pkt[128];
    memset(pkt, 0, sizeof pkt);
    hdr_t *h = (hdr_t *)pkt;
    h->type = create ? 0x0200 : 0x0201;
    if (create) {
        u32 *w = (u32 *)(pkt + sizeof *h);
        w[0] = 4;                                      /* nlen do nome curto */
        w[1] = kind == MOS_GPU_CTX_VENUS ? 4u : 1u;    /* context_init: venus=4, virgl=1 */
        memcpy(pkt + sizeof *h + 8, kind == MOS_GPU_CTX_VENUS ? "ven" : "v3d", 4);
    }
    h->ctx = id;
    return xmit(pkt, create ? (u32)(sizeof *h + 8 + 64) : (u32)sizeof *h);
}

int virtio_gpu_bo(unsigned ctx, unsigned id, unsigned long long size, int create) {
    if (!ready || broken) return -MOS_ENXIO;
    if (!create) {
        u8 pkt[32];
        memset(pkt, 0, sizeof pkt);
        ((hdr_t *)pkt)->type = CMD_RESOURCE_UNREF;
        *(u32 *)(pkt + sizeof(hdr_t)) = id;
        return xmit(pkt, sizeof(hdr_t) + 4);
    }
    u8 pkt[64];
    memset(pkt, 0, sizeof pkt);
    hdr_t *h = (hdr_t *)pkt;
    h->type = 0x0204;                                  /* RESOURCE_CREATE_3D */
    h->ctx = ctx;
    u32 *w = (u32 *)(pkt + sizeof *h);
    w[0] = id;
    w[1] = 0;                                          /* PIPE_BUFFER */
    w[2] = 1;                                          /* B8G8R8A8 */
    w[3] = 0;
    w[4] = (u32)(size > 0xFFFFFFFFull ? 0xFFFFFFFFu : size);
    w[5] = 1; w[6] = 1; w[7] = 1; w[8] = 0; w[9] = 0; w[10] = 0;
    int rc = xmit(pkt, sizeof *h + 11 * 4);
    if (rc) return rc;
    memset(pkt, 0, sizeof pkt);
    h = (hdr_t *)pkt;
    h->type = 0x0202;                                  /* CTX_ATTACH_RESOURCE */
    h->ctx = ctx;
    *(u32 *)(pkt + sizeof *h) = id;
    return xmit(pkt, sizeof *h + 8);
}

int virtio_gpu_submit(unsigned ctx, const void *cmds, unsigned n, unsigned fence_id) {
    if (!ready || broken) return -MOS_ENXIO;
    if (!cmds || !n || n > 400) return -MOS_EINVAL;
    u8 pkt[512];
    memset(pkt, 0, sizeof pkt);
    hdr_t *h = (hdr_t *)pkt;
    h->type = 0x0207;                                  /* SUBMIT_3D */
    h->ctx = ctx;
    if (fence_id) { h->flags = 1; h->fence = fence_id; }
    u32 *w = (u32 *)(pkt + sizeof *h);
    w[0] = n;
    memcpy(pkt + sizeof *h + 8, cmds, n);
    return xmit(pkt, (u32)(sizeof *h + 8 + n));
}
