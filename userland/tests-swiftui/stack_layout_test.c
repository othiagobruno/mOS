/* Layout das stacks dentro do NSHostingView, GeometryProxy e sizeThatFits.
 * clang -std=c11 -Wall -Wextra -Werror -DSWIFTUI_LAYOUT_HOST -DSWIFTUI_HOSTING_HOST \
 *   userland/libsystem/swiftui_layout.c userland/libsystem/swiftui_hosting.c \
 *   userland/tests-swiftui/stack_layout_test.c -o /tmp/sui_layout
 */
#include <stdio.h>

typedef struct {
    const void *isa; int axis, align; double spacing; void *content; int n;
    double cw[8], ch[8], x[8], y[8], w[8], h[8], bound_w, bound_h;
} sui_stack;
typedef struct { const void *isa; void *root; double width, height; } sui_host;
typedef struct { const void *isa; void *content; struct { const void *isa; double w, h, x, y; } proxy; } sui_reader;
typedef struct { double w, h; } sui_size;
typedef struct { void *body; const char *proto; } sui_conf;

void * $s7SwiftUI6HStackV9alignment7spacing7contentACyxGAA17VerticalAlignmentV_12CoreGraphics7CGFloatVSgxyXEtcfC(int align, double spacing, void *content);
void * $s7SwiftUI6VStackV9alignment7spacing7contentACyxGAA19HorizontalAlignmentV_12CoreGraphics7CGFloatVSgxyXEtcfC(int align, double spacing, void *content);
void * $s7SwiftUI6ZStackV9alignment7contentACyxGAA9AlignmentV_xyXEtcfC(int align, void *content);
void * $s7SwiftUI4ViewP4body4BodyQzvgTq(void);
void * $s7SwiftUI14GeometryReaderV7contentACyxGxAA0C5ProxyVc_tcfC(void *content);
void * $s7SwiftUI13GeometryProxyV4sizeSo6CGSizeVvg(void);
void * $s7SwiftUI13GeometryProxyV5frame2inSo6CGRectVAA15CoordinateSpaceO_tF(int space);
void * $s7SwiftUI5ShapePAAE12sizeThatFitsySo6CGSizeVAA16ProposedViewSizeVF(sui_size *proposal);
void * $s7SwiftUI13NSHostingViewC04rootD0ACyxGx_tcfc(void *root, void *metadata);
extern sui_conf $s7SwiftUI6HStackVyxGAA4ViewAAMc;

#define arm(p) __asm__ volatile("mov %0, %%r13" :: "r"(p) : "r13", "memory")

static void kids(sui_stack *s) {
    s->n = 2;
    s->cw[0] = 10; s->ch[0] = 4;
    s->cw[1] = 20; s->ch[1] = 6;
}

int main(void) {
    int fails = 0;
    char mark = 'c';
    sui_stack *h = $s7SwiftUI6HStackV9alignment7spacing7contentACyxGAA17VerticalAlignmentV_12CoreGraphics7CGFloatVSgxyXEtcfC(-1, 8, &mark);
    sui_stack *v, *z;
    sui_host *host;
    sui_reader *gr;
    sui_size prop = { 80, 40 }, *fit, *sz, *fr;
    kids(h);
    arm(h);
    if ($s7SwiftUI4ViewP4body4BodyQzvgTq() != &mark) { fprintf(stderr, "FAIL body\n"); fails++; }
    if (h->x[0] != 0 || h->x[1] != 18 || h->y[0] != 0) { fprintf(stderr, "FAIL hstack %g %g\n", h->x[0], h->x[1]); fails++; }
    if ($s7SwiftUI6HStackVyxGAA4ViewAAMc.body != (void *)$s7SwiftUI4ViewP4body4BodyQzvgTq) { fprintf(stderr, "FAIL conformance\n"); fails++; }
    v = $s7SwiftUI6VStackV9alignment7spacing7contentACyxGAA19HorizontalAlignmentV_12CoreGraphics7CGFloatVSgxyXEtcfC(-1, 8, &mark);
    kids(v);
    arm(v);
    $s7SwiftUI4ViewP4body4BodyQzvgTq();
    if (v->y[0] != 0 || v->y[1] != 12 || v->x[1] != 0) { fprintf(stderr, "FAIL vstack\n"); fails++; }
    z = $s7SwiftUI6ZStackV9alignment7contentACyxGAA9AlignmentV_xyXEtcfC(0, &mark);
    kids(z);
    arm(z);
    $s7SwiftUI4ViewP4body4BodyQzvgTq();
    if (z->x[0] != 95 || z->y[0] != 98) { fprintf(stderr, "FAIL zstack %g %g\n", z->x[0], z->y[0]); fails++; }
    host = $s7SwiftUI13NSHostingViewC04rootD0ACyxGx_tcfc(h, 0);
    host->width = 100;
    host->height = 40;
    h->x[1] = -1;
    arm(host);
    if ($s7SwiftUI4ViewP4body4BodyQzvgTq() != h || h->bound_w != 100 || h->x[1] != 18) {
        fprintf(stderr, "FAIL hosting layout\n"); fails++;
    }
    gr = $s7SwiftUI14GeometryReaderV7contentACyxGxAA0C5ProxyVc_tcfC(&mark);
    arm(&gr->proxy);
    sz = $s7SwiftUI13GeometryProxyV4sizeSo6CGSizeVvg();
    fr = $s7SwiftUI13GeometryProxyV5frame2inSo6CGRectVAA15CoordinateSpaceO_tF(0);
    if (!sz || sz->w != 240 || sz->h != 200 || !fr || fr[1].w != 240) { fprintf(stderr, "FAIL geometry\n"); fails++; }
    fit = $s7SwiftUI5ShapePAAE12sizeThatFitsySo6CGSizeVAA16ProposedViewSizeVF(&prop);
    if (!fit || fit->w != 80 || fit->h != 40) { fprintf(stderr, "FAIL fit\n"); fails++; }
    fit = $s7SwiftUI5ShapePAAE12sizeThatFitsySo6CGSizeVAA16ProposedViewSizeVF(0);
    if (!fit || fit->w != 10 || fit->h != 10) { fprintf(stderr, "FAIL fit vazio\n"); fails++; }
    if (fails) { fprintf(stderr, "RESULT FAIL %d\n", fails); return 1; }
    printf("RESULT PASS\n");
    return 0;
}
