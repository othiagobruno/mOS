/* Host: cc -std=c11 -ffreestanding -ffunction-sections -fdata-sections
 * -Ikernel/include tools/test-icones-dinamicos.c -Wl,--gc-sections -lm -o /tmp/mos-icon-live
 * Exercita o carregador real com VFS e desenho instrumentados. */
#include "../kernel/ui/icons_tahoe.c"
extern float sinf(float);
extern float cosf(float);
extern int puts(const char *);
static datetime_t current;
static u8 header[24] = {0x89, 'P', 'N', 'G', 13, 10, 26, 10, 0, 0, 0, 13, 'I', 'H', 'D', 'R', 0, 0, 0, 64, 0, 0, 0, 64};
static vnode_t node = {.data = header, .size = 24};
static surface_t decoded, target;
static char last_path[128], last_text[16];
static int lines, texts, freed, blits;
static float end_x[2], end_y[2];
static bool missing;
void rtc_now(datetime_t *out) { *out = current; }
float fsin(float x) { return sinf(x); }
float fcos(float x) { return cosf(x); }
vnode_t *vfs_lookup(const char *path) { snprintf(last_path, sizeof last_path, "%s", path); return missing ? NULL : &node; }
bool vfs_ensure(vnode_t *n) { return n != NULL; }
surface_t *image_decode(const u8 *d, usize n) { return d && n ? &decoded : NULL; }
void surface_free(surface_t *s) { if (s == &decoded) freed++; }
void gfx_blit_scaled(surface_t *s, const surface_t *im, int x, int y, int w, int h, int a) {
    (void)s; (void)im; (void)x; (void)y; (void)w; (void)h; (void)a; blits++;
}
font_t *font_get(int weight) { (void)weight; return NULL; }
rect_t rect_make(int x, int y, int w, int h) { return (rect_t){x, y, w, h}; }
void text_draw_center(surface_t *s, font_t *f, float z, rect_t r, u32 c, const char *text) {
    (void)s; (void)f; (void)z; (void)r; (void)c;
    snprintf(last_text, sizeof last_text, "%s", text); texts++;
}
void gfx_line(surface_t *s, float x, float y, float ex, float ey, float w, u32 c) {
    (void)s; (void)x; (void)y; (void)w; (void)c;
    if (lines < 2) { end_x[lines] = ex; end_y[lines] = ey; } lines++;
}
void gfx_circle(surface_t *s, float x, float y, float r, u32 c) { (void)s; (void)x; (void)y; (void)r; (void)c; }
int main(void) {
    current.day = 1;
    if (!original_icon("calendario", &target, 0, 0, 64) || strcmp(last_text, "1") || strcmp(last_path, "/Sistema/Biblioteca/Icones/calendario-base-64.png")) return 1;
    current.day = 31;
    if (!original_icon("calendario", &target, 0, 0, 64) || strcmp(last_text, "31") || texts != 4) return 2;
    current.hour = 3; current.minute = 0; lines = 0;
    if (!original_icon("relogio", &target, 0, 0, 64) || lines != 2 || end_x[0] <= 32 || end_y[1] >= 32) return 3;
    current.hour = 9; current.minute = 30; lines = 0;
    if (!original_icon("relogio", &target, 0, 0, 64) || lines != 2 || end_x[0] >= 32 || end_y[1] <= 32 || strcmp(last_path, "/Sistema/Biblioteca/Icones/relogio-base-64.png")) return 4;
    if (freed != 4 || blits != 4) return 5;
    missing = true;
    if (original_icon("calendario", &target, 0, 0, 64)) return 6;
    missing = false; header[19] = 63;
    if (original_icon("relogio", &target, 0, 0, 64)) return 7;
    puts("Calendário 1/31, relógio 03:00/09:30, liberação e recuperação: OK");
    return 0;
}
