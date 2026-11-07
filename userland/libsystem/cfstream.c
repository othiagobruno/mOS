/*
 * mOS libSystem — CFReadStream/CFWriteStream sobre NSInputStream/NSOutputStream (ponte gratuita, como na Apple:
 * o CFReadStreamRef é o próprio NSInputStream). O NSInputStream do mOS está em slack_frameworks.c. As funções de
 * agendamento (run loop, fila, cliente) só guardam o pedido: o mOS lê e escreve de forma síncrona.
 */
#include "foundation.h"
id objc_retain(id o);
void objc_release(id o);

#define SEL_(s) sel_registerName(s)
static int responds(id o, const char *s) {
    return o && ((signed char (*)(id, SEL, SEL))objc_msgSend)(o, SEL_("respondsToSelector:"), SEL_(s));
}

/* ---- leitura */
id CFReadStreamCreateWithFile(void *alloc, id url) {
    (void)alloc;
    id k = (id)(void *)cx_class_named("NSInputStream");
    if (!k || !url) return 0;
    id s = ((id (*)(id, SEL, id))objc_msgSend)(k, SEL_("inputStreamWithURL:"), url);
    return s ? objc_retain(s) : 0;                                         /* Create: +1 */
}
id CFReadStreamCreateWithBytesNoCopy(void *alloc, const void *b, long n, void *dealloc) {
    (void)alloc; (void)dealloc;
    id D = (id)(void *)cx_class_named("NSData"), k = (id)(void *)cx_class_named("NSInputStream");
    if (!D || !k) return 0;
    id d = ((id (*)(id, SEL, const void *, u64))objc_msgSend)(D, SEL_("dataWithBytes:length:"), b, (u64)(n > 0 ? n : 0));
    id s = ((id (*)(id, SEL, id))objc_msgSend)(k, SEL_("inputStreamWithData:"), d);
    return s ? objc_retain(s) : 0;
}
unsigned char CFReadStreamOpen(id s) { if (!s) return 0; ((void (*)(id, SEL))objc_msgSend)(s, SEL_("open")); return 1; }
void CFReadStreamClose(id s) { if (s) ((void (*)(id, SEL))objc_msgSend)(s, SEL_("close")); }
long CFReadStreamRead(id s, unsigned char *buf, long n) {
    if (!s || !buf || n <= 0) return n == 0 ? 0 : -1;
    return ((long (*)(id, SEL, unsigned char *, u64))objc_msgSend)(s, SEL_("read:maxLength:"), buf, (u64)n);
}
long CFReadStreamGetStatus(id s) { return s ? ((long (*)(id, SEL))objc_msgSend)(s, SEL_("streamStatus")) : 0; }
unsigned char CFReadStreamHasBytesAvailable(id s) { return s ? ((signed char (*)(id, SEL))objc_msgSend)(s, SEL_("hasBytesAvailable")) != 0 : 0; }
id CFReadStreamCopyError(id s) {
    id e = s && responds(s, "streamError") ? ((id (*)(id, SEL))objc_msgSend)(s, SEL_("streamError")) : 0;
    return e ? objc_retain(e) : 0;
}
id CFReadStreamCopyProperty(id s, id key) {
    id v = s ? ((id (*)(id, SEL, id))objc_msgSend)(s, SEL_("propertyForKey:"), key) : 0;
    return v ? objc_retain(v) : 0;
}
unsigned char CFReadStreamSetProperty(id s, id key, id v) {
    if (!s) return 0;
    return ((signed char (*)(id, SEL, id, id))objc_msgSend)(s, SEL_("setProperty:forKey:"), v, key) != 0;
}
unsigned char CFReadStreamSetClient(id s, unsigned long events, void *cb, void *ctx) { (void)s; (void)events; (void)cb; (void)ctx; return 1; }
void CFReadStreamScheduleWithRunLoop(id s, void *rl, id mode) { (void)s; (void)rl; (void)mode; }
void CFReadStreamUnscheduleFromRunLoop(id s, void *rl, id mode) { (void)s; (void)rl; (void)mode; }
void CFReadStreamSetDispatchQueue(id s, void *q) { (void)s; (void)q; }

/* ---- escrita: só com um NSOutputStream (o mOS ainda não cria um); sem ele, falha como um stream fechado */
unsigned char CFWriteStreamOpen(id s) { if (!s || !responds(s, "open")) return 0; ((void (*)(id, SEL))objc_msgSend)(s, SEL_("open")); return 1; }
void CFWriteStreamClose(id s) { if (s && responds(s, "close")) ((void (*)(id, SEL))objc_msgSend)(s, SEL_("close")); }
long CFWriteStreamWrite(id s, const unsigned char *b, long n) {
    if (!s || !responds(s, "write:maxLength:")) return -1;
    return ((long (*)(id, SEL, const unsigned char *, u64))objc_msgSend)(s, SEL_("write:maxLength:"), b, (u64)(n > 0 ? n : 0));
}
unsigned char CFWriteStreamCanAcceptBytes(id s) { return s && responds(s, "hasSpaceAvailable") && ((signed char (*)(id, SEL))objc_msgSend)(s, SEL_("hasSpaceAvailable")); }
long CFWriteStreamGetStatus(id s) { return s && responds(s, "streamStatus") ? ((long (*)(id, SEL))objc_msgSend)(s, SEL_("streamStatus")) : 7; }   /* kCFStreamStatusError */
id CFWriteStreamCopyError(id s) {
    id e = s && responds(s, "streamError") ? ((id (*)(id, SEL))objc_msgSend)(s, SEL_("streamError")) : 0;
    return e ? objc_retain(e) : 0;
}
unsigned char CFWriteStreamSetProperty(id s, id key, id v) {
    if (!s || !responds(s, "setProperty:forKey:")) return 0;
    return ((signed char (*)(id, SEL, id, id))objc_msgSend)(s, SEL_("setProperty:forKey:"), v, key) != 0;
}
unsigned char CFWriteStreamSetClient(id s, unsigned long events, void *cb, void *ctx) { (void)s; (void)events; (void)cb; (void)ctx; return 1; }
void CFWriteStreamScheduleWithRunLoop(id s, void *rl, id mode) { (void)s; (void)rl; (void)mode; }
void CFWriteStreamUnscheduleFromRunLoop(id s, void *rl, id mode) { (void)s; (void)rl; (void)mode; }
void CFWriteStreamSetDispatchQueue(id s, void *q) { (void)s; (void)q; }
