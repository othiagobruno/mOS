/* mOS — teste de host do driver de usuário da GPU AMD (userland/libsystem/amdws.c). O mos_gpu_op daqui faz o papel do
 * kernel (INFO com MOS_GPU_CAP_AMD, contexto, BOs com VA, mapeamento, fences) e passa cada IB enviado pelo validador
 * real do kernel (kernel/dev/amdgpu/pm4check.c), com os BOs da lista do lote. Sem GPU.
 *   gcc -O1 -DMOS_HOST_TEST -I kernel/dev/amdgpu -c kernel/dev/amdgpu/pm4check.c -o /tmp/pm4check.o
 *   gcc -std=gnu11 -O1 -fno-builtin -I tools/hostinc -I userland/libsystem tools/amdws_host.c userland/libsystem/amdws.c \
 *       /tmp/pm4check.o -o /tmp/amdws && /tmp/amdws */
#include "amdws.h"
#include "libc.h"
/* interface do validador do kernel (kernel/dev/amdgpu/pm4check.h), compilado à parte com -DMOS_HOST_TEST */
typedef unsigned int u32;
typedef struct { amd_u64 va, size; } amd_pm4_bo_t;
typedef struct {
    const amd_pm4_bo_t *bo; u32 nbo;
    struct { amd_u64 va; u32 ndw; } ib2[16];
    u32 nib2;
    u32 bad_dw;
} amd_pm4_ctx_t;
int amd_pm4_check(amd_pm4_ctx_t *c, const u32 *ib, u32 ndw, _Bool is_ib2);
const char *amd_pm4_error(int e);
enum { PM4E_VA = -5, PM4E_REG = -4 };

int printf(const char *, ...);
void *malloc(unsigned long);
void *calloc(unsigned long, unsigned long);
void free(void *);
static int pass, total;
#define CHECK(c, ...) do { total++; if (c) pass++; else { printf("FALHA %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ---- kernel de mentira */
typedef struct { unsigned handle; amd_u64 va, size; void *mem; unsigned flags; int used; } kbo_t;
static kbo_t kbo[64];
static int amd_cap = 1, nsub, last_err, last_ring;
static amd_u32 last_ndw;
static amd_u64 next_va = 0x100000000ull;
static kbo_t *kbo_of(unsigned h) { for (int i = 0; i < 64; i++) if (kbo[i].used && kbo[i].handle == h) return &kbo[i]; return 0; }
int mos_gpu_op(unsigned op, void *st, unsigned n) {
    (void)n;
    switch (op) {
    case MOS_GPU_INFO: {
        mos_gpu_info_t *in = st;
        in->abi = 3; in->caps = amd_cap ? MOS_GPU_CAP_AMD : 0; in->amd_family = 9; in->num_cu = 56;
        in->vram_bytes = 8ull << 30; in->gtt_bytes = 4ull << 30;
        return 0;
    }
    case MOS_GPU_CTX_CREATE: { mos_gpu_ctx_t *c = st; if (c->kind != MOS_GPU_CTX_AMD) return -MOS_EINVAL; c->ctx = 7; return 0; }
    case MOS_GPU_CTX_DESTROY: return 0;
    case MOS_GPU_BO_CREATE: {
        mos_gpu_bo_t *b = st;
        if (b->size & 4095) return -MOS_EINVAL;
        for (int i = 0; i < 64; i++) if (!kbo[i].used) {
            kbo[i] = (kbo_t){(unsigned)i + 1, next_va, b->size, calloc(1, b->size), b->flags, 1};
            next_va += b->size + 0x10000;
            b->handle = kbo[i].handle;
            b->gpu_va = b->ctx == 7 ? kbo[i].va : 0;
            return 0;
        }
        return -MOS_ENOMEM;
    }
    case MOS_GPU_BO_DESTROY: { kbo_t *b = kbo_of(((mos_gpu_bo_t *)st)->handle); if (!b) return -MOS_ENOENT; free(b->mem); b->used = 0; return 0; }
    case MOS_GPU_BO_MAP: { mos_gpu_map_t *m = st; kbo_t *b = kbo_of(m->handle); if (!b) return -MOS_ENOENT; m->addr = (amd_u64)(unsigned long)b->mem; return 0; }
    case MOS_GPU_BO_UNMAP: return 0;
    case MOS_GPU_FENCE_CREATE: ((mos_gpu_fence_t *)st)->fence = 3; return 0;
    case MOS_GPU_FENCE_WAIT: case MOS_GPU_FENCE_DESTROY: return 0;
    case MOS_GPU_SUBMIT: {
        mos_gpu_submit_t *s = st;
        if (s->ctx != 7 || !(s->flags & MOS_GPU_SUBMIT_AMD)) return -MOS_EINVAL;
        const unsigned char *buf = (const unsigned char *)(unsigned long)s->cmds;
        mos_gpu_amd_batch_t h;
        memcpy(&h, buf, sizeof h);
        if (h.version != MOS_GPU_AMD_BATCH_VER || !h.nib || h.nib > 16 || h.nbo > 256 || h.pad) return -MOS_EINVAL;
        if (sizeof h + h.nib * sizeof(mos_gpu_amd_ib_t) + h.nbo * 4 != s->cmd_bytes) return -MOS_EINVAL;
        static amd_pm4_bo_t ab[256];
        const unsigned char *hp = buf + sizeof h + h.nib * sizeof(mos_gpu_amd_ib_t);
        for (unsigned i = 0; i < h.nbo; i++) { unsigned hd; memcpy(&hd, hp + 4 * i, 4); kbo_t *b = kbo_of(hd); if (!b) return -MOS_EINVAL; ab[i].va = b->va; ab[i].size = b->size; }
        for (unsigned k = 0; k < h.nib; k++) {
            mos_gpu_amd_ib_t ib;
            memcpy(&ib, buf + sizeof h + k * sizeof ib, sizeof ib);
            kbo_t *b = kbo_of(ib.bo);
            if (!b || (amd_u64)(ib.offset_dw + ib.size_dw) * 4 > b->size) return -MOS_EINVAL;
            int listed = 0;
            for (unsigned i = 0; i < h.nbo; i++) if (ab[i].va == b->va) listed = 1;
            if (!listed) return -MOS_EINVAL;
            amd_pm4_ctx_t c;
            memset(&c, 0, sizeof c);
            c.bo = ab; c.nbo = h.nbo;
            last_err = amd_pm4_check(&c, (const u32 *)b->mem + ib.offset_dw, ib.size_dw, 0);
            last_ring = (int)ib.ring;
            last_ndw = ib.size_dw;
            if (last_err) { printf("validador: %s no dword %u\n", amd_pm4_error(last_err), c.bad_dw); return -MOS_EINVAL; }
        }
        nsub++;
        return 0;
    }
    }
    return -MOS_EINVAL;
}

/* procura um pacote tipo 3 no IB; devolve o índice do cabeçalho ou -1 */
static int find_pkt(const amd_u32 *ib, amd_u32 n, amd_u32 op, amd_u32 first_body) {
    for (amd_u32 p = 0; p < n;) {
        amd_u32 h = ib[p];
        if (h >> 30 != 3) return -1;
        amd_u32 cnt = (h >> 16) & 0x3FFF, o = (h >> 8) & 0xFF;
        if (o == 0x10 && cnt == 0x3FFF) { p++; continue; }
        if (o == op && (first_body == ~0u || ib[p + 1] == first_body)) return (int)p;
        p += cnt + 2;
    }
    return -1;
}

int main(void) {
    amdws_t w;
    amd_cap = 0;
    CHECK(amdws_open(&w) == -MOS_ENOENT, "sem MOS_GPU_CAP_AMD: -MOS_ENOENT");
    amd_cap = 1;
    CHECK(amdws_open(&w) == 0 && w.ctx == 7 && w.family == 9 && w.num_cu == 56, "abre o contexto AMD (família %u, %u CUs)", w.family, w.num_cu);
    amdbo_t code, data;
    CHECK(amdws_bo_new(&w, &code, 1000, MOS_GPU_BO_EXEC | MOS_GPU_BO_GPU) == 0 && code.va && code.map && code.size == 4096, "BO de código com VA");
    CHECK(amdws_bo_new(&w, &data, 8192, 0) == 0 && data.va, "BO de dados com VA");
    amdcs_t cs;
    CHECK(amdcs_init(&w, &cs, MOS_GPU_AMD_RING_COMPUTE, 4096) == 0, "fluxo de comandos num BO");
    amdcs_use(&cs, &code);
    amdcs_use(&cs, &data);
    amdcs_use(&cs, &data);
    CHECK(cs.nbo == 3, "lista de BOs sem repetição (%u)", cs.nbo);
    /* um despacho completo: invalida caches, programa o shader, despacha, avisa no fim e espera */
    amdcs_acquire_all(&cs);
    amd_dispatch_t d;
    memset(&d, 0, sizeof d);
    d.code_va = code.va;
    d.rsrc1 = 0x002C0041;
    d.rsrc2 = 0x00000090;
    d.user[0] = (amd_u32)data.va; d.user[1] = (amd_u32)(data.va >> 32); d.nuser = 2;
    d.local[0] = 64; d.local[1] = 1; d.local[2] = 1;
    d.groups[0] = 16; d.groups[1] = 2;
    d.lds_bytes = 1000;
    amdcs_dispatch(&cs, &d);
    amdcs_release_mem(&cs, 1, data.va + 4096, 0xC0FFEE);
    amdcs_wait_mem(&cs, data.va + 4096, 0xC0FFEE, 0xFFFFFFFFu);
    amd_u32 vals[3] = {1, 2, 3};
    amdcs_write_data(&cs, data.va + 16, vals, 3);
    unsigned fence = 0;
    CHECK(amdws_fence_new(&fence) == 0 && fence, "fence");
    int e = amdws_submit(&w, &cs, fence);
    CHECK(e == 0 && nsub == 1 && last_ring == MOS_GPU_AMD_RING_COMPUTE, "o lote passa no validador do kernel (%d)", e);
    CHECK(last_ndw % 8 == 0, "IB completo em blocos de 8 dwords (%u)", last_ndw);
    /* conteúdo dos pacotes */
    const amd_u32 *ib = cs.dw;
    int p = find_pkt(ib, cs.n, 0x76, (AMD_COMPUTE_PGM_LO - 0xB000) / 4);
    CHECK(p >= 0 && ib[p + 2] == (amd_u32)(code.va >> 8) && ib[p + 3] == (amd_u32)(code.va >> 40), "COMPUTE_PGM_LO/HI = VA >> 8");
    p = find_pkt(ib, cs.n, 0x76, (AMD_COMPUTE_PGM_RSRC1 - 0xB000) / 4);
    CHECK(p >= 0 && ib[p + 2] == 0x002C0041 && ((ib[p + 3] >> 1) & 0x1F) == 2 && ((ib[p + 3] >> 15) & 0x1FF) == 2 && (ib[p + 3] & 0x80),
          "RSRC2: 2 SGPRs de usuário, LDS em 2 blocos, bits do chamador mantidos (%08x)", p >= 0 ? ib[p + 3] : 0);
    p = find_pkt(ib, cs.n, 0x76, (AMD_COMPUTE_START_X - 0xB000) / 4);
    CHECK(p >= 0 && ib[p + 5] == 64 && ib[p + 6] == 1, "NUM_THREAD_X/Y");
    p = find_pkt(ib, cs.n, 0x76, (AMD_COMPUTE_USER_DATA_0 - 0xB000) / 4);
    CHECK(p >= 0 && ib[p + 2] == (amd_u32)data.va, "USER_DATA_0 = endereço do buffer");
    p = find_pkt(ib, cs.n, 0x15, ~0u);
    CHECK(p >= 0 && ib[p + 1] == 16 && ib[p + 2] == 2 && ib[p + 3] == 1 && (ib[p + 4] & 1), "DISPATCH_DIRECT 16x2x1");
    p = find_pkt(ib, cs.n, 0x49, ~0u);
    CHECK(p >= 0 && (ib[p + 1] & 0x3F) == 0x2F && ((ib[p + 2] >> 24) & 7) == 0 && ib[p + 5] == 0xC0FFEE, "RELEASE_MEM CS_DONE sem interrupção");
    /* o que o validador recusa não sai daqui: endereço fora dos BOs do lote */
    amdcs_reset(&cs);
    amdcs_write_data(&cs, 0xDEAD0000ull, vals, 1);
    CHECK(amdws_submit(&w, &cs, 0) == -MOS_EINVAL && last_err == PM4E_VA, "WRITE_DATA fora dos BOs: o kernel recusa");
    amdcs_reset(&cs);
    /* registradores de contexto e UCONFIG do lado do usuário */
    amd_u32 one = 4;
    amdcs_set_uconfig(&cs, 0x30908, &one, 1);           /* VGT_PRIMITIVE_TYPE */
    amdcs_set_ctx(&cs, 0x28C70, &one, 1);
    CHECK(amdws_submit(&w, &cs, 0) == 0, "SET_CONTEXT_REG e SET_UCONFIG_REG de usuário aceitos");
    amdcs_reset(&cs);
    amdcs_set_uconfig(&cs, 0x30800, &one, 1);           /* GRBM_GFX_INDEX: do kernel */
    CHECK(amdws_submit(&w, &cs, 0) == -MOS_EINVAL && last_err == PM4E_REG, "GRBM_GFX_INDEX recusado");
    amdcs_free(&w, &cs);
    amdws_bo_free(&w, &code);
    amdws_bo_free(&w, &data);
    amdws_close(&w);
    printf("amdws: %d/%d ok\n", pass, total);
    return pass != total;
}
