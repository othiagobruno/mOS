/* Host (Linux): o dlclose real do libSystem chama remove_image quando o SYS_DYLD simulado
 * diz que caiu a última referência, e não chama se o dylib tem seção __objc_.
 *
 *   gcc -std=gnu11 -Wall -Wextra -Wno-unused-function -Wno-unused-parameter \
 *       -DMOS_LIBSYS_SYSCALL -I userland/libsystem -I . \
 *       -o /tmp/dyld_remove_test tools/dyld_remove_test.c \
 *       userland/libsystem/dlfcn.c userland/libsystem/dyldimg.c
 *
 * O dylib é um cabeçalho Mach-O na memória (o Linux não executa o binário de
 * userland/tests-dyld-remove). O caminho do dlclose é o do libSystem.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "kernel/include/syscall.h"

#define MAXIMG 8
static int nimg = 1, refs[MAXIMG];
static const void *hdrs[MAXIMG];
static char names[MAXIMG][64];

static unsigned char plain_mh[32 + 72];
static unsigned char objc_mh[32 + 72 + 80];

static void put32(unsigned char *p, unsigned v) { p[0] = (unsigned char)v; p[1] = (unsigned char)(v >> 8); p[2] = (unsigned char)(v >> 16); p[3] = (unsigned char)(v >> 24); }
static void put64(unsigned char *p, unsigned long long v) { put32(p, (unsigned)v); put32(p + 4, (unsigned)(v >> 32)); }

static void fill_text(unsigned char *b, unsigned cmdsize, unsigned nsects) {
    memset(b, 0, 32 + cmdsize);
    put32(b, 0xFEEDFACFu);
    put32(b + 4, 0x01000007u);
    put32(b + 12, 6);
    put32(b + 16, 1);
    put32(b + 20, cmdsize);
    put32(b + 32, 0x19);
    put32(b + 36, cmdsize);
    memcpy(b + 40, "__TEXT", 6);
    put64(b + 56, 0);
    put32(b + 96, nsects);
}

long cx_syscall(long n, long a, long b, long c, long d, long e) {
    (void)e;
    if (n != SYS_DYLD) return 0;
    if (a == MOS_DYLD_COUNT) return nimg;
    if (a == MOS_DYLD_IMAGE) {
        unsigned i = (unsigned)b;
        if (i >= (unsigned)nimg) return 0;
        if (c && d) {
            size_t L = strlen(names[i]);
            if (L + 1 > (size_t)d) L = (size_t)d - 1;
            memcpy((void *)c, names[i], L);
            ((char *)c)[L] = 0;
        }
        return (long)hdrs[i];
    }
    if (a == MOS_DYLD_OPEN) {
        const char *path = (const char *)b;
        mos_dyld_open_t *o = (mos_dyld_open_t *)c;
        int i;
        memset(o, 0, sizeof *o);
        for (i = 1; i < nimg; i++) if (!strcmp(names[i], path)) break;
        if (i == nimg) {
            if (nimg >= MAXIMG) return -22;
            snprintf(names[nimg], sizeof names[nimg], "%s", path);
            hdrs[nimg] = strstr(path, "objc") ? objc_mh : plain_mh;
            refs[nimg] = 1;
            o->first = (unsigned long long)nimg;
            o->nnew = 1;
            return nimg++;
        }
        refs[i]++;
        return i;
    }
    if (a == MOS_DYLD_CLOSE) {
        unsigned i = (unsigned)b;
        if (i >= (unsigned)nimg || refs[i] == 0) return 0;
        refs[i]--;
        return refs[i] == 0 ? 1 : 0;
    }
    if (a == MOS_DYLD_ERROR) return 0;
    return 0;
}

int *__error(void) { static int e; return &e; }
char **environ;
char **NXArgv;
int NXArgc;
unsigned long long cx_image_base;
int cx_snprintf(char *d, unsigned long n, const char *fmt, ...) {
    va_list ap; int r;
    va_start(ap, fmt);
    r = vsnprintf(d, n, fmt, ap);
    va_end(ap);
    return r;
}
const char *cx_path(const char *in, char *buf, unsigned long cap) { snprintf(buf, cap, "%s", in); return buf; }
void cx_objc_boot(unsigned long long sects, unsigned long long n) { (void)sects; (void)n; }
int cx_unwind_disabled(void) { return 1; }
int cx_ls_unwind_sections(const void *a, void *info) { (void)a; (void)info; return 0; }

void _dyld_register_func_for_remove_image(void (*fn)(const void *, long long));
void *dlopen(const char *path, int mode);
int dlclose(void *h);
long _dyld_get_image_vmaddr_slide(unsigned i);
const void *_dyld_get_image_header(unsigned i);

static int removed, ok, total;
static const void *removed_mh;
static long long removed_slide;
#define CHECK(c, what) do { total++; if (c) ok++; else { fprintf(stderr, "FALHOU: %s\n", what); } } while (0)

static void on_remove(const void *mh, long long slide) {
    removed++;
    removed_mh = mh;
    removed_slide = slide;
}

int main(void) {
    void *h, *h2;
    const void *mh;
    long slide;
    fill_text(plain_mh, 72, 0);
    fill_text(objc_mh, 72 + 80, 1);
    memcpy(objc_mh + 32 + 72, "__objc_imageinfo", 16);
    hdrs[0] = plain_mh;
    snprintf(names[0], sizeof names[0], "%s", "/dyldrem");

    _dyld_register_func_for_remove_image(0);
    _dyld_register_func_for_remove_image(on_remove);

    h = dlopen("/librem.dylib", 2);
    CHECK(h != 0, "dlopen do dylib sem ObjC");
    mh = _dyld_get_image_header(1);
    slide = _dyld_get_image_vmaddr_slide(1);
    CHECK(mh == plain_mh, "cabeçalho do librem");
    CHECK(dlclose(h) == 0, "dlclose");
    CHECK(removed == 1, "remove_image uma vez no dlclose");
    CHECK(removed_mh == plain_mh, "callback recebeu o cabeçalho do dylib");
    CHECK(removed_slide == slide, "callback recebeu o slide");

    removed = 0;
    h = dlopen("/librem.dylib", 2);
    h2 = dlopen("/librem.dylib", 2);
    CHECK(h && h2, "dois dlopen");
    CHECK(dlclose(h) == 0 && removed == 0, "o primeiro de dois dlclose não avisa");
    CHECK(dlclose(h2) == 0 && removed == 1, "o segundo dlclose avisa");

    removed = 0;
    h = dlopen("/libobjc.dylib", 2);
    CHECK(h != 0, "dlopen do dylib com __objc_imageinfo");
    CHECK(dlclose(h) == 0 && removed == 0, "ObjC não recebe remove_image");

    printf("dyld-remove: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
