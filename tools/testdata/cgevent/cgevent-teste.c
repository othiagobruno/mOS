/* mOS — teste da entrada sintética global (CGEventPost -> GUI_DEVICE/DEV_INPUT_POST): clica num ponto da tela e deixa o
 * cursor em outro. Uso: cgevent-teste x y [xfinal yfinal]. Compilado com zig cc -target x86_64-macos (ver docs/compat-agentes.md). */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
typedef struct { double x, y; } CGPoint;
void *CGEventCreateMouseEvent(void *src, unsigned type, CGPoint p, unsigned button);
void CGEventPost(unsigned tap, void *e);
void CFRelease(const void *o);
void *CGDisplayCreateImage(unsigned d);
size_t CGImageGetWidth(void *i);
size_t CGImageGetHeight(void *i);
static void ev(unsigned type, double x, double y) {
    CGPoint p = {x, y};
    void *e = CGEventCreateMouseEvent(0, type, p, 0);
    CGEventPost(0, e);
    CFRelease(e);
}
int main(int argc, char **argv) {
    double x = argc > 2 ? atof(argv[1]) : 720, y = argc > 2 ? atof(argv[2]) : 510;
    double fx = argc > 4 ? atof(argv[3]) : 300, fy = argc > 4 ? atof(argv[4]) : 650;
    ev(5, x, y); usleep(200000);            /* kCGEventMouseMoved */
    ev(1, x, y); usleep(80000);             /* LeftMouseDown */
    ev(2, x, y); usleep(500000);            /* LeftMouseUp */
    ev(5, fx, fy);
    printf("cgevent-teste: clique em (%.0f, %.0f), cursor em (%.0f, %.0f)\n", x, y, fx, fy);
    void *img = CGDisplayCreateImage(1);
    printf("cgevent-teste: CGDisplayCreateImage -> %zu x %zu\n", img ? CGImageGetWidth(img) : 0, img ? CGImageGetHeight(img) : 0);
    return 0;
}
