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
