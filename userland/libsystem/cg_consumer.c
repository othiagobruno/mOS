/*
 * mOS CoreGraphics — CGDataConsumer: destino de bytes do CGImageDestination e (próximo passo da
 * frente) do CGPDFContext. Três formas, como no macOS: callbacks do app, CFMutableData e URL de arquivo.
 */
#include "foundation.h"
#include "libc.h"

typedef size_t (*cg_put_fn)(void *info, const void *buf, size_t n);
typedef void (*cg_rel_fn)(void *info);
typedef struct { cg_put_fn put; cg_rel_fn release; } cg_consumer_cb;
typedef struct { cx_class_t *isa; u32 magic; void *info; cg_consumer_cb cb; id data; int fd; } cgcons_t;
#define CONS_MAGIC 0x43474443u

void CFDataAppendBytes(id d, const u8 *b, long n);
u8 CFURLGetFileSystemRepresentation(id url, u8 resolve, u8 *buf, long max);
id objc_retain(id o);
void objc_release(id o);
void cx_object_free(id o);
int open(const char *path, int flags, ...);
long write(int fd, const void *b, unsigned long n);
int close(int fd);

static cx_class_t C_c, C_m;
static int c_ready;
static void cons_dealloc(id self, SEL s) {
    (void)s;
    cgcons_t *c = (cgcons_t *)self;
    if (c->cb.release) c->cb.release(c->info);
    if (c->data) objc_release(c->data);
    if (c->fd >= 0) close(c->fd);
    c->magic = 0;
    cx_object_free(self);
}
static u64 cons_tid(id self, SEL s) { (void)self; (void)s; return 0x2A8; }
static cgcons_t *cons_new(void) {
    if (!c_ready) {
        cx_class_t *ns = cx_class_named("NSObject");
        if (!ns) return 0;
        cx_define_class(&C_c, &C_m, "CGDataConsumer", ns, (u32)(sizeof(cgcons_t) - sizeof(void *)));
        cx_add_method(&C_c, "dealloc", (void *)cons_dealloc);
        cx_add_method(&C_c, "_cfTypeID", (void *)cons_tid);
        c_ready = 1;
    }
    cgcons_t *c = (cgcons_t *)cx_alloc_instance(&C_c);
    if (!c) return 0;
    c->magic = CONS_MAGIC;
    c->fd = -1;
    return c;
}
u64 CGDataConsumerGetTypeID(void) { return 0x2A8; }
void *CGDataConsumerCreate(void *info, const cg_consumer_cb *cb) {
    if (!cb || !cb->put) return 0;
    cgcons_t *c = cons_new();
    if (!c) return 0;
    c->info = info;
    c->cb = *cb;
    return c;
}
void *CGDataConsumerCreateWithCFData(id data) {
    if (!data) return 0;
    cgcons_t *c = cons_new();
    if (!c) return 0;
    c->data = objc_retain(data);
    return c;
}
void *CGDataConsumerCreateWithURL(id url) {
    char path[1024];
    if (!url || !CFURLGetFileSystemRepresentation(url, 1, (u8 *)path, (long)sizeof path)) return 0;
    int fd = open(path, 577, 0644);          /* O_WRONLY | O_CREAT | O_TRUNC */
    if (fd < 0) return 0;
    cgcons_t *c = cons_new();
    if (!c) { close(fd); return 0; }
    c->fd = fd;
    return c;
}
void *CGDataConsumerRetain(void *c) { if (c) objc_retain((id)c); return c; }
void CGDataConsumerRelease(void *c) { if (c) objc_release((id)c); }

/* escreve n bytes; devolve quantos o destino aceitou */
long cx_consumer_put(void *consumer, const void *bytes, size_t n) {
    cgcons_t *c = consumer;
    if (!c || c->magic != CONS_MAGIC) return -1;
    if (c->data) { CFDataAppendBytes(c->data, bytes, (long)n); return (long)n; }
    if (c->fd >= 0) {
        size_t k = 0;
        while (k < n) {
            long w = write(c->fd, (const u8 *)bytes + k, n - k);
            if (w <= 0) break;
            k += (size_t)w;
        }
        return (long)k;
    }
    size_t k = 0;
    while (k < n) {                           /* o callback pode aceitar menos de uma vez */
        size_t w = c->cb.put(c->info, (const u8 *)bytes + k, n - k);
        if (!w) break;
        k += w;
    }
    return (long)k;
}
