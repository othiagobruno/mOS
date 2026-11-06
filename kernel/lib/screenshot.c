/* mOS — captura de tela: grava o backbuffer do compositor como BMP de 32 bits em ~/Imagens.
 * Chame do laço do desktop (usa o VFS). Ex.: atalho Cmd+Shift+3 no desktop.c. */
#include "gfx.h"
#include "vfs.h"

static void put16(u8 *p, u16 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); }
static void put32(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }

/* devolve true e o caminho gravado (ex.: "/Users/Visitante/Imagens/Captura de Tela 2026-11-06 às 12.30.05.bmp") */
bool screenshot_save_rect(char *out_path, usize cap, rect_t r);
bool screenshot_save(char *out_path, usize cap) {
    surface_t *s = g_screen;
    if (!s) return false;
    return screenshot_save_rect(out_path, cap, rect_make(0, 0, s->w, s->h));
}

/* captura só um retângulo da tela (⌘⇧4 / ⌘⇧5: janela ou seleção) */
bool screenshot_save_rect(char *out_path, usize cap, rect_t r) {
    surface_t *s = g_screen;
    if (!s || !s->px || s->w <= 0 || s->h <= 0) return false;
    r = rect_intersect(r, rect_make(0, 0, s->w, s->h));
    if (r.w <= 0 || r.h <= 0) return false;
    u32 w = (u32)r.w, h = (u32)r.h, img = w * h * 4, total = 54 + img;
    u8 *bmp = kmalloc(total);
    if (!bmp) return false;
    memset(bmp, 0, 54);
    bmp[0] = 'B'; bmp[1] = 'M';
    put32(bmp + 2, total);
    put32(bmp + 10, 54);
    put32(bmp + 14, 40);                       /* BITMAPINFOHEADER */
    put32(bmp + 18, w);
    put32(bmp + 22, (u32)-(i32)h);             /* altura negativa: linhas de cima para baixo */
    put16(bmp + 26, 1);
    put16(bmp + 28, 32);
    put32(bmp + 34, img);
    put32(bmp + 38, 2835); put32(bmp + 42, 2835);   /* 72 dpi */
    u8 *dst = bmp + 54;
    for (u32 y = 0; y < h; y++) {
        const u32 *row = s->px + (usize)(y + (u32)r.y) * (u32)s->stride + (u32)r.x;
        for (u32 x = 0; x < w; x++, dst += 4) put32(dst, row[x] | 0xFF000000u);   /* BGRA, opaco */
    }
    datetime_t dt;
    rtc_now(&dt);
    char path[192];
    vfs_mkdir(HOME "/Imagens");
    snprintf(path, sizeof path, HOME "/Imagens/Captura de Tela %04d-%02d-%02d às %02d.%02d.%02d.bmp",
             dt.year, dt.month, dt.day, dt.hour, dt.minute, dt.second);
    bool ok = vfs_write_file(path, bmp, total) != NULL;
    kfree(bmp);
    if (ok && out_path) snprintf(out_path, cap, "%s", path);
    kprintf("captura de tela: %s (%ux%u)%s\n", path, w, h, ok ? "" : " ERRO");
    return ok;
}
