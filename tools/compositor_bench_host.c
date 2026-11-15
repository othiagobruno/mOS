/* mOS — medição de host do compositor (kernel/gfx/gfx.c real), sem VM. O tools/build.sh roda isto; à mão:
 *   zig cc -O2 -ffreestanding -fno-builtin -mno-red-zone -fshort-wchar -Ikernel/include -I. -std=gnu11 -w \
 *     -c kernel/gfx/gfx.c -o /tmp/gfx.o
 *   zig cc -O2 -Ikernel/include -I. -std=gnu11 -w tools/compositor_bench_host.c /tmp/gfx.o -lm -o /tmp/compbench
 *   /tmp/compbench [quadros]
 * Monta um quadro de tela cheia 1440x900 como o wm_compose: fundo, 4 janelas com sombra (a da frente com o
 * desfoque maior), vidro da barra lateral na janela da frente e a mistura de cada janela. Mede cada parte em 1
 * núcleo (par_for daqui é serial) e confere que a sombra de gfx_shadow_cached é pixel a pixel igual à direta. */
#include "gfx.h"
/* sem headers da libc: o kernel.h declara memcpy/memset com usize e conflitaria */
void *malloc(unsigned long); void *calloc(unsigned long, unsigned long); void free(void *);
int printf(const char *, ...); int atoi(const char *);
struct timespec { long tv_sec, tv_nsec; }; int clock_gettime(int, struct timespec *);
#define CLOCK_MONOTONIC 1

/* ---- o que o gfx.c pede do kernel ---- */
boot_info_t *g_boot;
void *kmalloc(usize n) { return malloc(n); }
void *kcalloc(usize a, usize b) { return calloc(a, b); }
void kfree(void *p) { free(p); }
float fsqrt(float x) { return __builtin_sqrtf(x); }
float fceil(float x) { return __builtin_ceilf(x); }
float ffloor(float x) { return __builtin_floorf(x); }
float fabsf_(float x) { return __builtin_fabsf(x); }
void par_for(int n, int grain, par_fn_t fn, void *ctx) { (void)grain; fn(ctx, 0, n, 0); }
int par_cpus(void) { return 1; }
void kprintf(const char *f, ...) { (void)f; }
void gpu_vsync_wait(void) {}
bool gpu_present(const u32 *px, int w, int h, int x, int y, int rw, int rh) { return false; }
void gpu_displays_changed(void) {}
void wm_damage(rect_t r) {}
void wm_displays_changed(const rect_t *a, const rect_t *b, int n) {}
void desktop_displays_changed(void) {}
void *vfs_lookup(const char *p) { return 0; }
int vfs_mkdir(const char *p) { return -1; }
int vfs_write_file(const char *p, const void *d, usize n) { return -1; }

static double now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e3 + t.tv_nsec / 1e6; }

typedef struct { int x, y, w, h; bool f; } win_t;
static const win_t W[] = { {120, 90, 760, 520, false}, {520, 160, 640, 460, false}, {300, 300, 820, 480, false}, {380, 140, 900, 600, true} };
#define NW ((int)(sizeof W / sizeof W[0]))

static void shadows(surface_t *s, bool cached) {
    for (int i = 0; i < NW; i++) {
        const win_t *w = &W[i];
        void (*sh)(surface_t *, int, int, int, int, float, float, int, int) = cached ? gfx_shadow_cached : gfx_shadow;
        sh(s, w->x, w->y, w->w, w->h, 10.0f, w->f ? 40 : 22, w->f ? 85 : 46, w->f ? 16 : 7);
        if (!w->f) sh(s, w->x, w->y, w->w, w->h, 10.0f, 3, 24, 1);
    }
}

int main(int argc, char **argv) {
    int frames = argc > 1 ? atoi(argv[1]) : 30;
    surface_t *scr = surface_new(1440, 900), *ref = surface_new(1440, 900), *bg = surface_new(1440, 900);
    surface_t *win[NW];
    gfx_grad_v(bg, 0, 0, 1440, 900, RGB(120, 110, 100), RGB(60, 60, 80));
    for (int i = 0; i < NW; i++) { win[i] = surface_new(W[i].w, W[i].h); gfx_clear(win[i], 0); gfx_round_rect(win[i], 0, 0, W[i].w, W[i].h, 10, RGB(240, 240, 242)); }
    glass_t g = {20, 300, RGBA(246, 246, 248, 236), 3, 10, 70, 6, 0, 0};
    int fails = 0;

    /* 1) igualdade: sombra em cache == sombra direta, em quadros inteiros e com clip parcial */
    rect_t clips[] = { {0, 0, 1440, 900}, {400, 120, 300, 200}, {0, 0, 37, 900}, {1000, 600, 440, 300} };
    for (unsigned c = 0; c < sizeof clips / sizeof clips[0]; c++)
        for (int pass = 0; pass < 2; pass++) {   /* pass 1 = cache já quente */
            gfx_blit_rect(ref, bg, rect_make(0, 0, 1440, 900), 0, 0);
            gfx_blit_rect(scr, bg, rect_make(0, 0, 1440, 900), 0, 0);
            surface_clip(ref, clips[c]); surface_clip(scr, clips[c]);
            shadows(ref, false); shadows(scr, true);
            surface_reset_clip(ref); surface_reset_clip(scr);
            if (memcmp(ref->px, scr->px, (unsigned long)1440 * 900 * 4)) { fails++; printf("FALHA sombra em cache difere (clip %u, passada %d)\n", c, pass); }
        }
    if (!fails) printf("ok sombra em cache igual à direta (4 recortes, frio e quente)\n");

    /* 2) vidro em cache: acerto devolve o mesmo resultado; um pixel do fundo diferente (na margem que o vidro lê)
     * obriga a recalcular */
    {
        unsigned long n = (unsigned long)1440 * 900 * 4;
        gfx_blit_rect(ref, bg, rect_make(0, 0, 1440, 900), 0, 0);
        gfx_glass(ref, 500, 100, 220, 600, 10, &g);                       /* falta: calcula e guarda */
        gfx_blit_rect(scr, bg, rect_make(0, 0, 1440, 900), 0, 0);
        double t0 = now_ms();
        gfx_glass(scr, 500, 100, 220, 600, 10, &g);                       /* acerto */
        double t1 = now_ms();
        if (memcmp(ref->px, scr->px, n)) { fails++; printf("FALHA vidro em cache difere do calculado\n"); }
        else printf("ok vidro em cache igual ao calculado\n");
        gfx_blit_rect(scr, bg, rect_make(0, 0, 1440, 900), 0, 0);
        gfx_fill(scr, 466, 80, 30, 30, RGB(255, 0, 0));                  /* fora do vidro, dentro da margem lida */
        double t2 = now_ms();
        gfx_glass(scr, 500, 100, 220, 600, 10, &g);
        double t3 = now_ms();
        if (!memcmp(ref->px + 100 * 1440, scr->px + 100 * 1440, (unsigned long)1440 * 600 * 4)) { fails++; printf("FALHA vidro reaproveitado com o fundo mudado\n"); }
        else printf("ok fundo mudado recalcula o vidro\n");
        printf("bench: vidro 220x600: calculado %.2f ms, em cache %.2f ms\n", t3 - t2, t1 - t0);
    }

    /* 3) tempo por parte, quadro inteiro */
    double t_bg = 0, t_sh = 0, t_shc = 0, t_gl = 0, t_bl = 0;
    for (int f = 0; f < frames; f++) {
        double t0 = now_ms();
        gfx_blit_rect(scr, bg, rect_make(0, 0, 1440, 900), 0, 0);
        double t1 = now_ms(); shadows(scr, false);
        double t2 = now_ms(); shadows(scr, true);
        double t3 = now_ms(); gfx_glass(scr, W[3].x, W[3].y, 220, W[3].h, 10, &g);
        double t4 = now_ms(); for (int i = 0; i < NW; i++) gfx_blend(scr, win[i], W[i].x, W[i].y, 255);
        double t5 = now_ms();
        t_bg += t1 - t0; t_sh += t2 - t1; t_shc += t3 - t2; t_gl += t4 - t3; t_bl += t5 - t4;
    }
    printf("bench: %d quadros 1440x900, 1 núcleo, ms/quadro: fundo %.2f, sombras diretas %.2f, sombras em cache %.2f (%.1fx), vidro %.2f, janelas %.2f\n",
           frames, t_bg / frames, t_sh / frames, t_shc / frames, t_shc > 0 ? t_sh / t_shc : 0, t_gl / frames, t_bl / frames);
    printf("compositor-bench: %s\n", fails ? "FALHOU" : "ok");
    return fails != 0;
}
