/* mOS — compressor de memória: codificador de página e zona de armazenamento.
 *
 * Codificador (implementação própria, por palavras de 32 bits, na linha dos compressores de
 * memória de Wilson e Kaplan): dicionário de 16 palavras, índice pela parte alta da palavra.
 * Cada passo vira uma marca de 2 bits num fluxo de bits (LSB primeiro):
 *   0 + 0                  uma palavra zero
 *   0 + 1 + n              corrida de n+2 zeros
 *   1 + idx(4)             igual a uma entrada do dicionário
 *   2 + idx(4) + 10 bits   os 22 bits altos iguais aos de uma entrada
 *   3 + 0 + 32 bits        literal
 *   3 + 1 + d(10) + n      cópia de n+2 palavras que começam d palavras atrás (texto, estruturas repetidas)
 * n: 4 bits, ou 15 e mais 10 bits. Só o literal e a marca 2 mudam o dicionário; o decodificador refaz o mesmo.
 * A cópia acha o par de palavras numa tabela de hash de 1024 posições.
 * Página toda zero não chega aqui (swap.c a guarda sem bytes).
 *
 * Zona: classes de 64 B a 1984 B (múltiplos de 64). Cada página da zona é de uma classe só,
 * com um cabeçalho de 32 B (encadeamento das páginas com espaço livre, máscara de objetos ocupados)
 * e até 63 objetos. A página volta ao PMM quando o último objeto sai. Saída comprimida maior que
 * VMC_MAX não é guardada: guardar 2 KiB em página própria não economiza nada.
 *
 * Sem trava própria: quem chama (swap.c) serializa.
 */
#ifdef MOS_HOST_TEST
#include "host_shim.h"
#include "vmcomp.h"
u64 vmc_host_page_alloc(void);
void vmc_host_page_free(u64 p);
#define zpage_alloc() vmc_host_page_alloc()
#define zpage_free(p) vmc_host_page_free(p)
#else
#include "kernel.h"
#include "vmcomp.h"
#define zpage_alloc() pmm_alloc(1)
#define zpage_free(p) pmm_free((p), 1)
#endif

/* ------------------------------------------------------------ codificador */
typedef struct { u8 *out; u32 cap, len; u64 acc; u32 nbits; int over; } bw_t;

static inline void bw_put(bw_t *w, u64 v, u32 n) {
    w->acc |= v << w->nbits;
    w->nbits += n;
    while (w->nbits >= 8) {
        if (w->len >= w->cap) { w->over = 1; w->nbits = 0; w->acc = 0; return; }
        w->out[w->len++] = (u8)w->acc;
        w->acc >>= 8;
        w->nbits -= 8;
    }
}

typedef struct { const u8 *in; u32 len, pos; u64 acc; u32 nbits; int bad; } br_t;

static inline u32 br_get(br_t *r, u32 n) {
    while (r->nbits < n) {
        if (r->pos >= r->len) { r->bad = 1; return 0; }
        r->acc |= (u64)r->in[r->pos++] << r->nbits;
        r->nbits += 8;
    }
    u32 v = (u32)(r->acc & ((1ULL << n) - 1));
    r->acc >>= n;
    r->nbits -= n;
    return v;
}

static inline u32 dict_idx(u32 w) { return ((w >> 10) * 2654435761u) >> 28; }
static inline u32 pair_hash(u32 a, u32 b) { return ((a * 2654435761u) ^ (b * 0x85EBCA6Bu)) >> 22; }   /* 10 bits */

/* comprimento: até 14 em 4 bits; 15 = mais 10 bits */
static inline void put_len(bw_t *w, u32 v) {
    if (v < 15) bw_put(w, v, 4);
    else { bw_put(w, 15, 4); bw_put(w, v - 15, 10); }
}
static inline u32 get_len(br_t *r) {
    u32 v = br_get(r, 4);
    return v < 15 ? v : 15 + br_get(r, 10);
}

u32 vmc_compress(const void *page, u8 *out, u32 cap) {
    const u32 *in = page;
    u32 dict[16] = {0};
    u16 head[1024];
    bw_t w = {out, cap, 0, 0, 0, 0};
    memset(head, 0xFF, sizeof head);
    for (u32 i = 0; i < VMC_WORDS;) {
        u32 x = in[i];
        if (!x) {                                         /* zero, ou corrida de zeros */
            u32 r = 1;
            while (i + r < VMC_WORDS && !in[i + r]) r++;
            if (r == 1) bw_put(&w, 0, 3);
            else { bw_put(&w, 4, 3); put_len(&w, r - 2); }
            i += r;
        } else {
            u32 len = 0, off = 0;
            if (i + 1 < VMC_WORDS) {                      /* referência para trás: par de palavras já visto */
                u32 h = pair_hash(x, in[i + 1]), c = head[h];
                head[h] = (u16)i;
                if (c != 0xFFFF && in[c] == x && in[c + 1] == in[i + 1]) {
                    len = 2;
                    while (i + len < VMC_WORDS && in[c + len] == in[i + len]) len++;
                    off = i - c;
                }
            }
            if (len) {
                bw_put(&w, 3 | (1u << 2) | ((u64)off << 3), 13);
                put_len(&w, len - 2);
                for (u32 k = 1; k < len && i + k + 1 < VMC_WORDS; k++)
                    head[pair_hash(in[i + k], in[i + k + 1])] = (u16)(i + k);
                i += len;
                if (w.over) return 0;
                continue;
            }
            u32 k = dict_idx(x);
            if (dict[k] == x) bw_put(&w, 1 | (k << 2), 6);
            else if ((dict[k] >> 10) == (x >> 10)) { bw_put(&w, 2 | (k << 2) | ((u64)(x & 0x3FF) << 6), 16); dict[k] = x; }
            else { bw_put(&w, 3, 3); bw_put(&w, x, 32); dict[k] = x; }
            i++;
        }
        if (w.over) return 0;
    }
    if (w.nbits) {
        if (w.len >= w.cap) return 0;
        w.out[w.len++] = (u8)w.acc;
    }
    return w.len;
}

bool vmc_decompress(const u8 *in, u32 len, void *page) {
    u32 *o = page;
    u32 dict[16] = {0};
    br_t r = {in, len, 0, 0, 0, 0};
    for (u32 i = 0; i < VMC_WORDS;) {
        u32 tag = br_get(&r, 2);
        if (tag == 0) {
            u32 n = br_get(&r, 1) ? get_len(&r) + 2 : 1;
            if (r.bad || i + n > VMC_WORDS) return false;
            for (u32 k = 0; k < n; k++) o[i + k] = 0;
            i += n;
        } else if (tag == 1) {
            u32 k = br_get(&r, 4);
            if (r.bad) return false;
            o[i++] = dict[k];
        } else if (tag == 2) {
            u32 k = br_get(&r, 4), lo = br_get(&r, 10);
            if (r.bad) return false;
            dict[k] = (dict[k] & ~0x3FFu) | lo;
            o[i++] = dict[k];
        } else if (br_get(&r, 1)) {
            u32 off = br_get(&r, 10), n = get_len(&r) + 2;
            if (r.bad || !off || off > i || i + n > VMC_WORDS) return false;
            for (u32 k = 0; k < n; k++) o[i + k] = o[i - off + k];
            i += n;
        } else {
            u32 x = br_get(&r, 32);
            if (r.bad) return false;
            dict[dict_idx(x)] = x;
            o[i++] = x;
        }
    }
    return r.pos == len;
}

/* ------------------------------------------------------------ zona */
#define ZHDR 32u
#define NCLS (VMC_MAX / 64)            /* 31 classes: 64, 128, ..., 1984 */
typedef struct zpg { struct zpg *prev, *next; u64 used_mask; u16 cls, used, nobj, onlist; } zpg_t;
_Static_assert(sizeof(zpg_t) == ZHDR, "cabeçalho da página da zona");

static zpg_t *partial[NCLS];
static u64 zpages, zbytes, zlimit = ~0ULL;

static inline u32 cls_of(u32 len) { return (len + 63) / 64 - 1; }
static inline u32 cls_size(u32 c) { return (c + 1) * 64; }

static void list_add(zpg_t *g) {
    g->prev = NULL;
    g->next = partial[g->cls];
    if (g->next) g->next->prev = g;
    partial[g->cls] = g;
    g->onlist = 1;
}
static void list_del(zpg_t *g) {
    if (g->prev) g->prev->next = g->next;
    else partial[g->cls] = g->next;
    if (g->next) g->next->prev = g->prev;
    g->prev = g->next = NULL;
    g->onlist = 0;
}

void vmc_zone_limit(u64 pages) { zlimit = pages; }

void *vmc_zalloc(u32 len) {
    if (!len || len > VMC_MAX) return NULL;
    u32 c = cls_of(len);
    zpg_t *g = partial[c];
    if (!g) {
        if (zpages >= zlimit) return NULL;
        u64 ph = zpage_alloc();
        if (!ph) return NULL;
        g = (zpg_t *)ph;
        memset(g, 0, sizeof *g);
        g->cls = (u16)c;
        g->nobj = (u16)((4096 - ZHDR) / cls_size(c));
        zpages++;
        list_add(g);
    }
    u32 i = (u32)__builtin_ctzll(~g->used_mask);
    g->used_mask |= 1ULL << i;
    g->used++;
    zbytes += cls_size(c);
    if (g->used == g->nobj) list_del(g);
    return (u8 *)g + ZHDR + i * cls_size(c);
}

void vmc_zfree(void *obj) {
    if (!obj) return;
    zpg_t *g = (zpg_t *)((u64)obj & ~0xFFFULL);
    u32 sz = cls_size(g->cls);
    u32 i = (u32)(((u8 *)obj - (u8 *)g - ZHDR) / sz);
    if (i >= g->nobj || !(g->used_mask & (1ULL << i))) {
        kprintf("vmcomp: objeto %p fora da zona ou liberado duas vezes\n", obj);
        return;
    }
    g->used_mask &= ~(1ULL << i);
    g->used--;
    zbytes -= sz;
    if (!g->used) {
        if (g->onlist) list_del(g);
        zpages--;
        zpage_free((u64)g);
        return;
    }
    if (!g->onlist) list_add(g);
}

u64 vmc_zone_pages(void) { return zpages; }
u64 vmc_zone_bytes(void) { return zbytes; }
