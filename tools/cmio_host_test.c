/* Host: userland/libsystem/cmio.c (parte DAL) com CoreMedia de mentira e uma câmera UVC falsa (DEV_CAMERA_*). → "cmio: N/N ok"
 * gcc -std=gnu11 -O1 -c tools/cmio_host_test.c; gcc -std=gnu11 -DMOS_LIBSYS_SYSCALL -I userland/libsystem/kshim -I userland/libsystem
 *   -I kernel/include -fno-builtin -c userland/libsystem/cmio.c; ligar com -lpthread -Wl,--unresolved-symbols=ignore-all (tools/cmio_test.sh) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
typedef int32_t i32; typedef uint32_t u32; typedef uint64_t u64; typedef int64_t i64; typedef uint8_t u8; typedef uint16_t u16; typedef int16_t i16;
#include "../kernel/include/compat_gui.h"
typedef int OSStatus;
typedef struct { unsigned sel, scope, elem; } paddr;
#define FCC(a, b, c, d) ((unsigned)(a) << 24 | (unsigned)(b) << 16 | (unsigned)(c) << 8 | (unsigned)(d))
OSStatus CMIOObjectGetPropertyDataSize(unsigned, const paddr *, unsigned, const void *, unsigned *);
OSStatus CMIOObjectGetPropertyData(unsigned, const paddr *, unsigned, const void *, unsigned, unsigned *, void *);
OSStatus CMIOObjectSetPropertyData(unsigned, const paddr *, unsigned, const void *, unsigned, const void *);
OSStatus CMIOObjectAddPropertyListener(unsigned, const paddr *, void *, void *);
OSStatus CMIOObjectCreate(void *, unsigned, unsigned, unsigned *);
OSStatus CMIOObjectsPublishedAndDied(void *, unsigned, unsigned, const unsigned *, unsigned, const unsigned *);
OSStatus CMIOStreamCopyBufferQueue(unsigned, void *, void *, void **);
OSStatus CMIODeviceStartStream(unsigned, unsigned), CMIODeviceStopStream(unsigned, unsigned);
int cx_cmio_stream_push(unsigned, void *);
OSStatus CMIOObjectIsPropertySettable(unsigned, const paddr *, unsigned char *);
/* CoreMedia de mentira */
typedef struct { const void *e[64]; int h, t; } q_t;
int CMSimpleQueueCreate(void *a, int cap, void **o) { (void)a; (void)cap; *o = calloc(1, sizeof(q_t)); return 0; }
int CMSimpleQueueEnqueue(void *q, const void *e) { q_t *x = q; if (x->t - x->h >= 64) return -1; x->e[x->t++ % 64] = e; return 0; }
const void *CMSimpleQueueDequeue(void *q) { q_t *x = q; return x->h == x->t ? 0 : x->e[x->h++ % 64]; }
int CMAudioClockCreate(void *a, void **o) { (void)a; *o = (void *)1; return 0; }
typedef struct { long long v; int ts; unsigned fl; long long ep; } cmt_t;
typedef struct { unsigned codec; int w, h; } fd_t;
typedef struct { void *img; fd_t *fd; cmt_t pts; } sb_t;
int CMVideoFormatDescriptionCreate(void *a, unsigned codec, int w, int h, void *ext, void **o) { (void)a; (void)ext; fd_t *f = calloc(1, sizeof *f); f->codec = codec; f->w = w; f->h = h; *o = f; return 0; }
int CMSampleBufferCreateForImageBuffer(void *a, void *img, unsigned char r, void *cb, void *ref, void *fd, const void *t, void **o) {
    (void)a; (void)r; (void)cb; (void)ref; sb_t *s = calloc(1, sizeof *s); s->img = img; s->fd = fd; memcpy(&s->pts, (const char *)t + sizeof(cmt_t), sizeof(cmt_t)); *o = s; return 0;
}
static long long clk;
cmt_t CMClockGetTime(void *c) { (void)c; cmt_t t = {clk += 1000, 1000000000, 1, 0}; return t; }
void *CMClockGetHostTimeClock(void) { return (void *)1; }
static unsigned pb_fmt; static int pb_w;
void *cx_avcap_frame_pb(unsigned fourcc, const unsigned char *d, unsigned len, int w, int h, unsigned fmt) { (void)fourcc; (void)d; (void)len; (void)h; pb_fmt = fmt; pb_w = w; return calloc(1, 16); }
void *cx_str(const char *s) { return strdup(s); }
void *objc_autoreleasePoolPush(void) { return 0; }
void objc_autoreleasePoolPop(void *p) { (void)p; }
/* câmera falsa */
static gui_camera_info_t cam; static int has_cam, frames_left, opened, closed; static unsigned long long open_fi;
long cx_syscall(long n, long a, long b, long c, long d, long e) {
    (void)e;
    if (n != 79) return -1;
    switch (a) {
    case DEV_CAMERA_INFO: if (!has_cam || b) return 0; cam.cur = cam.mode[0]; memcpy((void *)c, &cam, sizeof cam); return 1;
    case DEV_CAMERA_OPEN: opened++; open_fi = (unsigned long long)d; cam.cur_interval = (unsigned)(open_fi >> 32); return 0;
    case DEV_CAMERA_READ: if (!frames_left) return 0; frames_left--; memset((void *)c, 128, 16); return 16;
    case DEV_CAMERA_CLOSE: closed++; return 0;
    }
    return -1;
}
void *objc_retain(void *o) { return o; }
void objc_release(void *o) { (void)o; }
void *_Block_copy(const void *b) { return (void *)b; }
void _Block_release(const void *b) { (void)b; }
static int fails, total, notes, altered;
static unsigned last_sel;
static void expect(int c, const char *m) { total++; if (!c) { printf("FALHOU: %s\n", m); fails++; } }
static OSStatus lis(unsigned obj, unsigned n, const paddr *a, void *ref) { (void)obj; (void)n; (void)ref; notes++; last_sel = a->sel; return 0; }
static void alt(unsigned s, void *sb, void *ref) { (void)s; (void)sb; (void)ref; altered++; }
int main(void) {
    paddr devs = {FCC('d', 'e', 'v', '#'), FCC('g', 'l', 'o', 'b'), 0}, yes = {FCC('y', 'e', 's', ' '), FCC('g', 'l', 'o', 'b'), 0};
    unsigned sz = 99, v = 1, got = 0, list[8];
    expect(CMIOObjectGetPropertyDataSize(1, &devs, 0, 0, &sz) == 0 && sz == 0, "sem câmera: lista vazia");
    CMIOObjectAddPropertyListener(1, &yes, (void *)lis, 0);
    CMIOObjectAddPropertyListener(1, &devs, (void *)lis, 0);
    expect(CMIOObjectSetPropertyData(1, &yes, 0, 0, 4, &v) == 0 && notes == 1 && last_sel == yes.sel, "AllowScreenCaptureDevices avisa o ouvinte");
    v = 0; CMIOObjectGetPropertyData(1, &yes, 0, 0, 4, &got, &v);
    expect(got == 4 && v == 1, "e lê de volta");
    expect(CMIOObjectSetPropertyData(1, &devs, 0, 0, 4, &v) == (OSStatus)FCC('u', 'n', 'o', 'p'), "lista de dispositivos só se lê");
    unsigned dev = 0, st = 0;
    CMIOObjectCreate(0, 1, FCC('a', 'd', 'e', 'v'), &dev);
    CMIOObjectCreate(0, dev, FCC('a', 's', 't', 'r'), &st);
    unsigned pub[2] = {dev, st};
    CMIOObjectsPublishedAndDied(0, 1, 2, pub, 0, 0);
    CMIOObjectGetPropertyData(1, &devs, 0, 0, sizeof list, &got, list);
    paddr stm = {FCC('s', 't', 'm', '#'), FCC('g', 'l', 'o', 'b'), 0};
    unsigned sl[4]; unsigned sg = 0;
    CMIOObjectGetPropertyData(dev, &stm, 0, 0, sizeof sl, &sg, sl);
    expect(got == 4 && list[0] == dev && notes == 2 && sg == 4 && sl[0] == st, "plug-in publica dispositivo e stream");
    void *q = 0; int frame = 7;
    expect(CMIOStreamCopyBufferQueue(st, (void *)alt, 0, &q) == 0 && q, "fila do stream");
    expect(cx_cmio_stream_push(st, &frame) != 0, "parado: não entrega");
    CMIODeviceStartStream(dev, st);
    expect(cx_cmio_stream_push(st, &frame) == 0 && CMSimpleQueueDequeue(q) == &frame && altered == 1, "rodando: quadro na fila e aviso");
    CMIODeviceStopStream(dev, st);
    CMIOObjectsPublishedAndDied(0, 1, 0, 0, 2, pub);
    CMIOObjectGetPropertyDataSize(1, &devs, 0, 0, &sz);
    expect(sz == 0 && CMIOObjectGetPropertyDataSize(dev, &stm, 0, 0, &sz) == (OSStatus)FCC('!', 'o', 'b', 'j'), "removido: some e vira objeto inválido");
    /* câmera USB: aparece na lista com um stream de entrada */
    int before = notes;
    snprintf(cam.name, sizeof cam.name, "Webcam");
    snprintf(cam.path, sizeof cam.path, "2");
    cam.vid = 0x046d; cam.pid = 0x0825; cam.nmode = 2;
    cam.mode[0] = (gui_camera_mode_t){0x32595559, 4, 2, 333333, 1000000, 333333, 16};
    cam.mode[1] = (gui_camera_mode_t){0x3231564e, 4, 2, 333333, 333333, 333333, 12};
    has_cam = 1;
    CMIOObjectGetPropertyDataSize(1, &devs, 0, 0, &sz);
    unsigned cdev = 0, cst = 0;
    CMIOObjectGetPropertyData(1, &devs, 0, 0, sizeof list, &got, list);
    if (got == 4) cdev = list[0];
    CMIOObjectGetPropertyData(cdev, &stm, 0, 0, sizeof sl, &sg, sl);
    if (sg == 4) cst = sl[0];
    expect(sz == 4 && cdev && cst && notes == before + 1, "câmera publicada e ouvinte de dev# avisado");
    paddr tran = {FCC('t', 'r', 'a', 'n'), FCC('g', 'l', 'o', 'b'), 0}, sdir = {FCC('s', 'd', 'i', 'r'), FCC('g', 'l', 'o', 'b'), 0};
    unsigned tv = 0, dir = 9;
    CMIOObjectGetPropertyData(cdev, &tran, 0, 0, 4, &got, &tv);
    CMIOObjectGetPropertyData(cst, &sdir, 0, 0, 4, &got, &dir);
    expect(tv == FCC('u', 's', 'b', ' ') && dir == 1, "transporte USB e stream de entrada");
    paddr nfrm = {FCC('n', 'f', 'r', 'm'), FCC('g', 'l', 'o', 'b'), 0}, nfrs = {FCC('n', 'f', 'r', '#'), FCC('g', 'l', 'o', 'b'), 0};
    double r = 0, rs[4];
    CMIOObjectGetPropertyData(cst, &nfrm, 0, 0, 8, &got, &r);
    unsigned rsz = 0;
    CMIOObjectGetPropertyData(cst, &nfrs, 0, 0, sizeof rs, &rsz, rs);
    expect(r > 29.9 && r < 30.1 && rsz == 16 && rs[1] > 9.9 && rs[1] < 10.1, "30 qps, faixa 30 a 10");
    unsigned char settable = 0;
    CMIOObjectIsPropertySettable(cst, &nfrm, &settable);
    double want = 12;
    expect(settable && CMIOObjectSetPropertyData(cst, &nfrm, 0, 0, 8, &want) == 0, "taxa ajustável");
    paddr pft = {FCC('p', 'f', 't', ' '), FCC('g', 'l', 'o', 'b'), 0};
    fd_t *fd = 0;
    CMIOObjectGetPropertyData(cst, &pft, 0, 0, sizeof fd, &got, &fd);
    expect(fd && fd->codec == FCC('2', 'v', 'u', 'y') && fd->w == 4, "formato atual 2vuy 4x2");
    void *cq = 0;
    CMIOStreamCopyBufferQueue(cst, 0, 0, &cq);
    frames_left = 2;
    CMIODeviceStartStream(cdev, cst);
    expect(opened == 1 && (unsigned)open_fi == 0x32595559 && (open_fi >> 32) == 833333, "abre o modo pedido a 12 qps");
    sb_t *got_sb = 0;
    for (int i = 0; i < 2000 && !got_sb; i++) { got_sb = (sb_t *)CMSimpleQueueDequeue(cq); if (!got_sb) usleep(1000); }
    expect(got_sb && got_sb->fd && got_sb->fd->codec == FCC('2', 'v', 'u', 'y') && pb_fmt == FCC('2', 'v', 'u', 'y') && pb_w == 4, "quadro na fila em 2vuy");
    expect(CMIOObjectSetPropertyData(cst, &nfrm, 0, 0, 8, &want) == (int)FCC('n', 'o', 't', 'r'), "rodando: não troca a taxa");
    CMIODeviceStopStream(cdev, cst);
    expect(closed == 1, "parar fecha a câmera");
    has_cam = 0;
    CMIOObjectGetPropertyDataSize(1, &devs, 0, 0, &sz);
    expect(sz == 0 && notes == before + 2, "câmera desligada some e avisa");
    printf("cmio: %d/%d ok\n", total - fails, total);
    return fails != 0;
}
