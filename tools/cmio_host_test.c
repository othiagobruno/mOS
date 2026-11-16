/* Host: userland/libsystem/cmio.c (parte DAL) com o CMSimpleQueue do openbot_cm.c. → "cmio: N/N ok" */
#include <stdio.h>
#include <string.h>
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
const void *CMSimpleQueueDequeue(void *);
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
    printf("cmio: %d/%d ok\n", total - fails, total);
    return fails != 0;
}
