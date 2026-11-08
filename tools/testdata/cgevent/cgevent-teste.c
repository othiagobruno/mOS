/* mOS — teste da entrada sintética global (CGEventPost -> GUI_DEVICE/DEV_INPUT_POST) e da captura de tela
 * (CGDisplayCreateImage -> DEV_SCREEN_CAPTURE): clica num ponto da tela e deixa o cursor em outro.
 * Uso: cgevent-teste x y [xfinal yfinal [espera]]. Com "espera", antes do clique aguarda o pixel do alvo ficar escuro
 * (um botão preto, como o "Sign in with ChatGPT" do Codex), lendo a tela a cada segundo (até 15 min): não depende do
 * relógio do kernel, que sob WHPX anda devagar. Compilado com zig cc -target x86_64-macos -Wl,-undefined,dynamic_lookup. */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
typedef struct { double x, y; } CGPoint;
typedef struct { double x, y, w, h; } CGRect;
void *CGEventCreateMouseEvent(void *src, unsigned type, CGPoint p, unsigned button);
void CGEventPost(unsigned tap, void *e);
void CFRelease(const void *o);
void *CGDisplayCreateImage(unsigned d);
void *CGDisplayCreateImageForRect(unsigned d, CGRect r);
size_t CGImageGetWidth(void *i);
size_t CGImageGetHeight(void *i);
void *CGImageGetDataProvider(void *img);
void *CGDataProviderCopyData(void *p);
const unsigned char *CFDataGetBytePtr(void *d);

static void ev(unsigned type, double x, double y) {
    CGPoint p = {x, y};
    void *e = CGEventCreateMouseEvent(0, type, p, 0);
    CGEventPost(0, e);
    CFRelease(e);
}
static int want_r = -1, want_g, want_b;              /* 6º argumento RRGGBB: espera essa cor (± 50 por canal) */
static int near(int a, int b) { return a > b - 50 && a < b + 50; }
static int dark_at(double x, double y) {
    CGRect r = {x, y, 1, 1};
    void *img = CGDisplayCreateImageForRect(1, r);
    if (!img) return 0;
    void *d = CGDataProviderCopyData(CGImageGetDataProvider(img));
    const unsigned char *p = d ? CFDataGetBytePtr(d) : 0;
    int dark = p && (want_r >= 0 ? near(p[2], want_r) && near(p[1], want_g) && near(p[0], want_b) : p[0] + p[1] + p[2] < 450);
    if (d) CFRelease(d);
    CFRelease(img);
    return dark;
}
int main(int argc, char **argv) {
    double x = argc > 2 ? atof(argv[1]) : 720, y = argc > 2 ? atof(argv[2]) : 510;
    double fx = argc > 4 ? atof(argv[3]) : 300, fy = argc > 4 ? atof(argv[4]) : 650;
    if (argc > 6) { unsigned c = (unsigned)strtoul(argv[6], 0, 16); want_r = (int)(c >> 16 & 255); want_g = (int)(c >> 8 & 255); want_b = (int)(c & 255); }
    if (argc > 5) {
        int i = 0;
        while (i < 900 && !dark_at(x, y)) { sleep(1); i++; }
        printf("cgevent-teste: alvo %s em (%.0f, %.0f) depois de %d leituras\n", i < 900 ? "escuro" : "nunca escureceu", x, y, i);
        sleep(3);
    }
    ev(5, x, y); usleep(200000);            /* kCGEventMouseMoved */
    ev(1, x, y); usleep(80000);             /* LeftMouseDown */
    ev(2, x, y); usleep(500000);            /* LeftMouseUp */
    ev(5, fx, fy);
    printf("cgevent-teste: clique em (%.0f, %.0f), cursor em (%.0f, %.0f)\n", x, y, fx, fy);
    void *img = CGDisplayCreateImage(1);
    printf("cgevent-teste: CGDisplayCreateImage -> %zu x %zu\n", img ? CGImageGetWidth(img) : 0, img ? CGImageGetHeight(img) : 0);
    return 0;
}
