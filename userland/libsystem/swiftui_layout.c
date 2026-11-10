/* HStack, VStack, ZStack, GeometryReader e View.body.
 * O body de um NSHostingView posiciona o root quando ele é uma dessas stacks,
 * no retângulo width x height da view (240x200 na criação).
 */
#ifdef SWIFTUI_LAYOUT_HOST
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#else
#include "libc.h"
#endif

typedef struct { uint32_t kind; const char *name; } sui_desc;

typedef struct {
    const void *isa;
    int axis;
    int align;
    double spacing;
    void *content;
    int n;
    double cw[8], ch[8];
    double x[8], y[8], w[8], h[8];
    double bound_w, bound_h;
} sui_stack;

typedef struct {
    const void *isa;
    void *root;
    double width, height;
} sui_host;

typedef struct {
    const void *isa;
    double w, h, x, y;
} sui_proxy;

typedef struct {
    const void *isa;
    void *content;
    sui_proxy proxy;
} sui_reader;

typedef struct { double w, h; } sui_size;

extern sui_desc $s7SwiftUI13NSHostingViewCMn;

sui_desc $s7SwiftUI6HStackVMn = { 7, "HStack" };
sui_desc $s7SwiftUI6VStackVMn = { 8, "VStack" };
sui_desc $s7SwiftUI6ZStackVMn = { 9, "ZStack" };
sui_desc $s7SwiftUI13GeometryProxyVMn = { 10, "GeometryProxy" };
sui_desc $s7SwiftUI14GeometryReaderVMn = { 11, "GeometryReader" };
/* Layouts das stacks (13..15): o body não os trata como stack montada. */
sui_desc $s7SwiftUI13_HStackLayoutVMn = { 13, "_HStackLayout" };
sui_desc $s7SwiftUI13_VStackLayoutVMn = { 14, "_VStackLayout" };
sui_desc $s7SwiftUI13_ZStackLayoutVMn = { 15, "_ZStackLayout" };
static sui_desc sui_proposed_desc = { 12, "_ProposedSize" };

typedef struct { void *body; const char *proto; } sui_conf;
void *$s7SwiftUI4ViewP4body4BodyQzvgTq(void);
sui_conf $s7SwiftUI6HStackVyxGAA4ViewAAMc = { (void *)$s7SwiftUI4ViewP4body4BodyQzvgTq, "View" };
sui_conf $s7SwiftUI6VStackVyxGAA4ViewAAMc = { (void *)$s7SwiftUI4ViewP4body4BodyQzvgTq, "View" };
sui_conf $s7SwiftUI6ZStackVyxGAA4ViewAAMc = { (void *)$s7SwiftUI4ViewP4body4BodyQzvgTq, "View" };
sui_conf $s7SwiftUI14GeometryReaderVyxGAA4ViewAAMc = { (void *)$s7SwiftUI4ViewP4body4BodyQzvgTq, "View" };

void *$s7SwiftUI6HStackVMa(void) { return &$s7SwiftUI6HStackVMn; }
void *$s7SwiftUI6ZStackVMa(void) { return &$s7SwiftUI6ZStackVMn; }
void *$s7SwiftUI13GeometryProxyVMa(void) { return &$s7SwiftUI13GeometryProxyVMn; }
void *$s7SwiftUI14GeometryReaderVMa(void) { return &$s7SwiftUI14GeometryReaderVMn; }
void *$s7SwiftUI13_ProposedSizeVMa(void) { return &sui_proposed_desc; }

static void *sui_r13(void) {
    void *p;
    __asm__ volatile("mov %%r13, %0" : "=r"(p));
    return p;
}

static void sui_place(sui_stack *s) {
    int i;
    double cursor = 0;
    double cross = 0;
    double bw = s->bound_w > 0 ? s->bound_w : 200;
    double bh = s->bound_h > 0 ? s->bound_h : 200;
    if (s->n > 8) s->n = 8;
    for (i = 0; i < s->n; i++) {
        if (s->axis == 2) {
            cross = s->cw[i] > cross ? s->cw[i] : cross;
        } else if (s->axis == 0) {
            if (s->ch[i] > cross) cross = s->ch[i];
        } else if (s->cw[i] > cross) cross = s->cw[i];
    }
    for (i = 0; i < s->n; i++) {
        if (s->axis == 2) {
            s->w[i] = s->cw[i];
            s->h[i] = s->ch[i];
            s->x[i] = (bw - s->w[i]) / 2;
            s->y[i] = (bh - s->h[i]) / 2;
            continue;
        }
        if (s->axis == 0) {
            s->w[i] = s->cw[i];
            s->h[i] = s->ch[i];
            s->x[i] = cursor;
            s->y[i] = s->align < 0 ? 0 : s->align > 0 ? bh - s->h[i] : (cross - s->h[i]) / 2;
            cursor += s->w[i] + s->spacing;
        } else {
            s->w[i] = s->cw[i];
            s->h[i] = s->ch[i];
            s->y[i] = cursor;
            s->x[i] = s->align < 0 ? 0 : s->align > 0 ? bw - s->w[i] : (cross - s->w[i]) / 2;
            cursor += s->h[i] + s->spacing;
        }
    }
}

static sui_stack *sui_new_stack(sui_desc *d, int axis, int align, double spacing, void *content) {
    sui_stack *s = (sui_stack *)calloc(1, sizeof *s);
    if (!s) return 0;
    s->isa = d;
    s->axis = axis;
    s->align = align;
    s->spacing = spacing < 0 ? 8 : spacing;
    s->content = content;
    s->bound_w = 200;
    s->bound_h = 200;
    return s;
}

void *$s7SwiftUI6HStackV9alignment7spacing7contentACyxGAA17VerticalAlignmentV_12CoreGraphics7CGFloatVSgxyXEtcfC(int align, double spacing, void *content) {
    return sui_new_stack(&$s7SwiftUI6HStackVMn, 0, align, spacing, content);
}
void *$s7SwiftUI6VStackV9alignment7spacing7contentACyxGAA19HorizontalAlignmentV_12CoreGraphics7CGFloatVSgxyXEtcfC(int align, double spacing, void *content) {
    return sui_new_stack(&$s7SwiftUI6VStackVMn, 1, align, spacing, content);
}
void *$s7SwiftUI6ZStackV9alignment7contentACyxGAA9AlignmentV_xyXEtcfC(int align, void *content) {
    (void)align;
    return sui_new_stack(&$s7SwiftUI6ZStackVMn, 2, 0, 0, content);
}

void *$s7SwiftUI4ViewP4body4BodyQzvgTq(void) {
    void *self = sui_r13();
    sui_desc *d;
    sui_stack *s;
    sui_host *h;
    if (!self) return 0;
    d = *(sui_desc **)self;
    if (!d) return 0;
    if (d->kind == 1) {
        h = (sui_host *)self;
        if (!h->root) return 0;
        d = *(sui_desc **)h->root;
        if (d && (d->kind == 7 || d->kind == 8 || d->kind == 9)) {
            s = (sui_stack *)h->root;
            s->bound_w = h->width;
            s->bound_h = h->height;
            sui_place(s);
        }
        return h->root;
    }
    if (d->kind == 7 || d->kind == 8 || d->kind == 9) {
        s = (sui_stack *)self;
        sui_place(s);
        return s->content;
    }
    if (d->kind == 11) return ((sui_reader *)self)->content;
    return 0;
}

void *$s7SwiftUI14GeometryReaderV7contentACyxGxAA0C5ProxyVc_tcfC(void *content) {
    sui_reader *r = (sui_reader *)calloc(1, sizeof *r);
    if (!r) return 0;
    r->isa = &$s7SwiftUI14GeometryReaderVMn;
    r->content = content;
    r->proxy.isa = &$s7SwiftUI13GeometryProxyVMn;
    r->proxy.w = 240;
    r->proxy.h = 200;
    return r;
}

void *$s7SwiftUI13GeometryProxyV4sizeSo6CGSizeVvg(void) {
    sui_proxy *p = (sui_proxy *)sui_r13();
    static sui_size sz;
    sz.w = p ? p->w : 0;
    sz.h = p ? p->h : 0;
    return &sz;
}

void *$s7SwiftUI13GeometryProxyV5frame2inSo6CGRectVAA15CoordinateSpaceO_tF(int space) {
    sui_proxy *p = (sui_proxy *)sui_r13();
    static sui_size rect[2];
    (void)space;
    if (!p) return 0;
    rect[0].w = p->x;
    rect[0].h = p->y;
    rect[1].w = p->w;
    rect[1].h = p->h;
    return rect;
}

static sui_size sui_fit(sui_size *proposal) {
    sui_size r;
    if (!proposal || proposal->w < 0 || proposal->h < 0) { r.w = 10; r.h = 10; return r; }
    r = *proposal;
    return r;
}

void *$s7SwiftUI5ShapeP12sizeThatFitsySo6CGSizeVAA16ProposedViewSizeVFTq(sui_size *proposal) {
    static sui_size r;
    r = sui_fit(proposal);
    return &r;
}
void *$s7SwiftUI5ShapePAAE12sizeThatFitsySo6CGSizeVAA16ProposedViewSizeVF(sui_size *proposal) {
    return $s7SwiftUI5ShapeP12sizeThatFitsySo6CGSizeVAA16ProposedViewSizeVFTq(proposal);
}
