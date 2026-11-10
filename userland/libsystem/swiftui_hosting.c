/* NSHostingView que o swift_addon do Claude e o SkyComputerUseService do ChatGPT
 * importam na partida. swiftcc x86_64: o self vai em r13; o rootView entra em rdi.
 * O descritor nominal e o method descriptor são dados, não funções vazias.
 */
#ifdef SWIFTUI_HOSTING_HOST
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#else
#include "libc.h"
#endif

typedef struct {
    const void *isa;
    void *root;
    double width;
    double height;
} mos_hv;

typedef struct {
    uint32_t kind;
    const char *name;
} mos_hv_desc;

static const char mos_hv_name[] = "NSHostingView";

mos_hv_desc $s7SwiftUI13NSHostingViewCMn = { 1, mos_hv_name };

static void *mos_hv_self(void) {
    void *p;
    __asm__ volatile("mov %%r13, %0" : "=r"(p));
    return p;
}

void *$s7SwiftUI13NSHostingViewC04rootD0ACyxGx_tcfc(void *root, void *metadata) {
    mos_hv *h;
    (void)metadata;
    h = (mos_hv *)calloc(1, sizeof *h);
    if (!h) return 0;
    h->isa = &$s7SwiftUI13NSHostingViewCMn;
    h->root = root;
    h->width = 240;
    h->height = 200;
    return h;
}

void *$s7SwiftUI13NSHostingViewC04rootD0ACyxGx_tcfCTq = (void *)$s7SwiftUI13NSHostingViewC04rootD0ACyxGx_tcfc;

void *$s7SwiftUI13NSHostingViewC04rootD0xvMTj(void) {
    mos_hv *h = (mos_hv *)mos_hv_self();
    if (!h) return 0;
    return &h->root;
}

void $s7SwiftUI13NSHostingViewC04rootD0xvsTj(void *root) {
    mos_hv *h = (mos_hv *)mos_hv_self();
    if (!h) return;
    h->root = root;
}

/* UIHostingController (o WhatsApp é Catalyst): guarda o rootView e monta um
 * NSHostingView com ele, que é a view do controller. */
typedef struct {
    const void *isa;
    void *root;
    mos_hv *view;
    void *coder;
} mos_hc;

mos_hv_desc $s7SwiftUI19UIHostingControllerCMn = { 16, "UIHostingController" };

static mos_hc *mos_hc_new(void *root, void *coder) {
    mos_hc *c = (mos_hc *)calloc(1, sizeof *c);
    if (!c) return 0;
    c->isa = &$s7SwiftUI19UIHostingControllerCMn;
    c->root = root;
    c->coder = coder;
    c->view = (mos_hv *)$s7SwiftUI13NSHostingViewC04rootD0ACyxGx_tcfc(root, 0);
    return c;
}

void *$s7SwiftUI19UIHostingControllerC8rootViewACyxGx_tcfc(void *root, void *metadata) {
    (void)metadata;
    return mos_hc_new(root, 0);
}

void *$s7SwiftUI19UIHostingControllerC8rootViewACyxGx_tcfC(void *root, void *metadata) {
    return $s7SwiftUI19UIHostingControllerC8rootViewACyxGx_tcfc(root, metadata);
}

/* init?(coder:rootView:): com o rootView dado o controller sempre nasce. */
void *$s7SwiftUI19UIHostingControllerC5coder8rootViewACyxGSgSo7NSCoderC_xtcfc(void *coder, void *root) {
    return mos_hc_new(root, coder);
}

void *$s7SwiftUI19UIHostingControllerC8rootViewACyxGx_tcfCTq = (void *)$s7SwiftUI19UIHostingControllerC8rootViewACyxGx_tcfC;

/* NSHostingController (AppKit): o mesmo controller, com a view hospedeira como view. */
mos_hv_desc $s7SwiftUI19NSHostingControllerCMn = { 16, "NSHostingController" };

void *$s7SwiftUI19NSHostingControllerC8rootViewACyxGx_tcfc(void *root, void *metadata) {
    mos_hc *c;
    (void)metadata;
    c = mos_hc_new(root, 0);
    if (c) c->isa = &$s7SwiftUI19NSHostingControllerCMn;
    return c;
}

void *$s7SwiftUI19NSHostingControllerC8rootViewACyxGx_tcfC(void *root, void *metadata) {
    return $s7SwiftUI19NSHostingControllerC8rootViewACyxGx_tcfc(root, metadata);
}
void *$s7SwiftUI19UIHostingControllerC5coder8rootViewACyxGSgSo7NSCoderC_xtcfCTq = (void *)$s7SwiftUI19UIHostingControllerC5coder8rootViewACyxGSgSo7NSCoderC_xtcfc;
