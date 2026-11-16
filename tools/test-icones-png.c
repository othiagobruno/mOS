/* Host: decodifica os PNGs com o código real do kernel, sem iniciar o mOS.
 * cc -std=c11 -ffreestanding -ffunction-sections -fdata-sections -Ikernel/include
 *    tools/test-icones-png.c -Wl,--gc-sections -o /tmp/mos-icon-png
 * O chamador compara dimensões e hash dos pixels com um leitor independente. */
#include "../kernel/web/image.c"

extern void *malloc(usize n);
extern void *calloc(usize n, usize size);
extern void free(void *p);
extern void *fopen(const char *path, const char *mode);
extern usize fread(void *out, usize size, usize count, void *file);
extern int fseek(void *file, long offset, int whence);
extern long ftell(void *file);
extern int fclose(void *file);
extern int printf(const char *fmt, ...);

void *kmalloc(usize n) { return malloc(n); }
void *kcalloc(usize n, usize size) { return calloc(n, size); }
void kfree(void *p) { free(p); }

u32 color_premul(u32 color) {
    u32 a = C_A(color);
    return a << 24 | C_R(color) * a / 255 << 16 |
           C_G(color) * a / 255 << 8 | C_B(color) * a / 255;
}

surface_t *surface_new(int w, int h) {
    if (w < 1 || h < 1 || w > 2048 || h > 2048) return NULL;
    surface_t *s = calloc(1, sizeof(*s));
    if (!s) return NULL;
    s->px = calloc((usize)w * h, sizeof(u32));
    if (!s->px) { free(s); return NULL; }
    s->w = w; s->h = h; s->stride = w;
    s->clip = (rect_t){0, 0, w, h};
    return s;
}

int main(int argc, char **argv) {
    for (int i = 1; i < argc; i++) {
        void *file = fopen(argv[i], "rb");
        if (!file || fseek(file, 0, 2)) return 1;
        long length = ftell(file);
        if (length < 33 || length > 512 * 1024 || fseek(file, 0, 0)) return 2;
        u8 *data = malloc((usize)length);
        if (!data || fread(data, 1, (usize)length, file) != (usize)length) return 3;
        fclose(file);
        surface_t *s = decode_png(data, (usize)length);
        free(data);
        if (!s) return 4;
        u64 hash = 14695981039346656037ull;
        for (int y = 0; y < s->h; y++)
            for (int x = 0; x < s->w; x++) {
                hash ^= s->px[(usize)y * s->stride + x];
                hash *= 1099511628211ull;
            }
        printf("%d %d %llu\n", s->w, s->h, hash);
        free(s->px); free(s);
    }
    return 0;
}
