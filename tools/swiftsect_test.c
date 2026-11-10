/* Seções __swift5_* registradas pelo dyld de userspace.
 *   gcc -std=gnu11 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter \
 *       -DMOS_LIBSYS_SYSCALL -I userland/libsystem -I . \
 *       -o /tmp/swiftsect_test tools/swiftsect_test.c \
 *       userland/libsystem/dyldimg.c userland/libsystem/swiftsect.c
 */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include "kernel/include/syscall.h"

static unsigned char img[0x2000];
static const void *hdrs[2];
static int nimg;

static void put32(unsigned char *p, unsigned v) {
    p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8);
    p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24);
}
static void put64(unsigned char *p, unsigned long long v) { put32(p, (unsigned)v); put32(p + 4, (unsigned)(v >> 32)); }

static void fill(int with_swift) {
    unsigned char *b = img;
    unsigned cmdsize = 72 + (with_swift ? 160 : 0);
    memset(img, 0, sizeof img);
    put32(b, 0xFEEDFACFu);
    put32(b + 4, 0x01000007u);
    put32(b + 12, 6);
    put32(b + 16, 1);
    put32(b + 20, cmdsize);
    put32(b + 32, 0x19);
    put32(b + 36, cmdsize);
    memcpy(b + 40, "__TEXT", 6);
    put64(b + 56, 0);
    put64(b + 64, sizeof img);
    put32(b + 96, with_swift ? 2 : 0);
    if (!with_swift) return;
    memcpy(b + 32 + 72, "__swift5_types", 14);
    memcpy(b + 32 + 72 + 16, "__TEXT", 6);
    put64(b + 32 + 72 + 32, 0x1000);
    put64(b + 32 + 72 + 40, 8);
    memcpy(b + 32 + 72 + 80, "__swift5_reflstr", 16);
    memcpy(b + 32 + 72 + 80 + 16, "__TEXT", 6);
    put64(b + 32 + 72 + 80 + 32, 0x1100);
    put64(b + 32 + 72 + 80 + 40, 4);
    img[0x1000] = 0x11;
    img[0x1100] = 0x22;
}

unsigned _dyld_image_count(void) { return (unsigned)nimg; }
const void *_dyld_get_image_header(unsigned i) { return i < (unsigned)nimg ? hdrs[i] : 0; }

void cx_swift_note(const void *mh, long long slide);
void cx_swift_forget(const void *mh);
void cx_swift_reset(void);
int cx_swift_count(void);
int cx_swift_lookup(const void *mh, const char *sect, void **addr, unsigned long long *size);
void cx_dyld_notify_add(unsigned first, unsigned n);
void cx_dyld_notify_remove(const void *mh, long long slide);

static int ok, total;
#define CHECK(c, w) do { total++; if (c) ok++; else fprintf(stderr, "FALHOU: %s\n", w); } while (0)

int main(void) {
    void *addr = 0;
    unsigned long long sz = 0;
    fill(0);
    hdrs[0] = img;
    nimg = 1;
    cx_swift_reset();
    cx_dyld_notify_add(0, 1);
    CHECK(cx_swift_count() == 0, "imagem sem __swift5_ não registra");

    fill(1);
    cx_swift_reset();
    cx_dyld_notify_add(0, 1);
    CHECK(cx_swift_count() == 2, "types e reflstr");
    CHECK(cx_swift_lookup(img, "__swift5_types", &addr, &sz) == 1, "acha types");
    CHECK(addr == img + 0x1000 && sz == 8 && ((unsigned char *)addr)[0] == 0x11, "ponteiro de types com slide");
    CHECK(cx_swift_lookup(img, "__swift5_reflstr", &addr, &sz) == 1 && ((unsigned char *)addr)[0] == 0x22, "reflstr");
    CHECK(cx_swift_lookup(img, "__swift5_proto", &addr, &sz) == 0, "proto ausente não entra");
    cx_dyld_notify_add(0, 1);
    CHECK(cx_swift_count() == 2, "registrar de novo não duplica");
    cx_dyld_notify_remove(img, 0);
    CHECK(cx_swift_count() == 0, "remove_image esquece as seções");
    printf("swiftsect: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
