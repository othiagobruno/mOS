/* Host: init(rootView:), setter e modify do NSHostingView.
 *   clang -std=c11 -Wall -Wextra -Werror -DSWIFTUI_HOSTING_HOST \
 *     userland/tests-swiftui/hosting_init_test.c -o /tmp/hosting_init && /tmp/hosting_init
 */
#define SWIFTUI_HOSTING_HOST 1
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef struct { const void *isa; void *root; } mos_hv;
typedef struct { uint32_t kind; const char *name; } mos_hv_desc;

extern mos_hv_desc $s7SwiftUI13NSHostingViewCMn;
extern void *$s7SwiftUI13NSHostingViewC04rootD0ACyxGx_tcfCTq;
void *$s7SwiftUI13NSHostingViewC04rootD0ACyxGx_tcfc(void *root, void *metadata);
void *$s7SwiftUI13NSHostingViewC04rootD0xvMTj(void);
void $s7SwiftUI13NSHostingViewC04rootD0xvsTj(void *root);

static void *hv_self;

static void *call_init(void *root) {
    return $s7SwiftUI13NSHostingViewC04rootD0ACyxGx_tcfc(root, 0);
}

static void call_set(void *self, void *root) {
    hv_self = self;
    __asm__ volatile("mov %0, %%r13" :: "r"(self) : "r13");
    $s7SwiftUI13NSHostingViewC04rootD0xvsTj(root);
}

static void *call_modify(void *self) {
    void *p;
    __asm__ volatile("mov %0, %%r13" :: "r"(self) : "r13");
    p = $s7SwiftUI13NSHostingViewC04rootD0xvMTj();
    return p;
}

int main(void) {
    int fails = 0;
    char a = 'a', b = 'b';
    mos_hv *h = call_init(&a);
    mos_hv *g;
    void *slot;
    if (!h) { fprintf(stderr, "FAIL init nil\n"); return 1; }
    if (h->isa != &$s7SwiftUI13NSHostingViewCMn) { fprintf(stderr, "FAIL isa\n"); fails++; }
    if (h->root != &a) { fprintf(stderr, "FAIL root inicial\n"); fails++; }
    if ($s7SwiftUI13NSHostingViewCMn.kind != 1 || strcmp($s7SwiftUI13NSHostingViewCMn.name, "NSHostingView") != 0) {
        fprintf(stderr, "FAIL descritor\n"); fails++;
    }
    if ($s7SwiftUI13NSHostingViewC04rootD0ACyxGx_tcfCTq != (void *)$s7SwiftUI13NSHostingViewC04rootD0ACyxGx_tcfc) {
        fprintf(stderr, "FAIL method descriptor\n"); fails++;
    }
    call_set(h, &b);
    if (h->root != &b) { fprintf(stderr, "FAIL setter\n"); fails++; }
    slot = call_modify(h);
    if (slot != &h->root || *(void **)slot != &b) { fprintf(stderr, "FAIL modify\n"); fails++; }
    call_set(0, &a);
    g = call_init(0);
    if (!g || g == h || g->root != 0) { fprintf(stderr, "FAIL segundo init\n"); fails++; }
    if (fails) { fprintf(stderr, "RESULT FAIL %d\n", fails); return 1; }
    printf("RESULT PASS\n");
    return 0;
}
