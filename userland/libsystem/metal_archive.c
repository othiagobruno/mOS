/* mOS libSystem — MTLBinaryArchive do Metal por software.
 *
 * No macOS o arquivo guarda o código de máquina dos pipelines para não compilar de novo. Aqui o shader roda no
 * interpretador (não há código de máquina a guardar), então o arquivo guarda o que foi adicionado (nome de cada
 * função e o tipo de pipeline) no formato próprio "MOSMTLBA1" e os pipelines continuam compilados na hora; um
 * pipeline criado com binaryArchives funciona igual. serializeToURL: grava, a url do descritor relê, e um arquivo
 * que não é deste formato dá MTLBinaryArchiveErrorInvalidFile, como no macOS. */
#include "metalint.h"

u64 mtl_dget(id o, const char *name);
id objc_retain(id o);
void objc_release(id o);
id cx_str(const char *s);
const char *cx_cstr(id s);
int open(const char *p, int flags, ...);
long read(int fd, void *b, unsigned long n);
long write(int fd, const void *b, unsigned long n);
int close(int fd);

typedef struct { MHDR; char **name; u8 *kind; u32 n; } mba_t;   /* kind: 1 compute, 2 render, 3 tile, 4 função */
static cx_class_t MBA_c, MBA_m;
static const char MAGIC[10] = "MOSMTLBA1";

static int ba_add_name(mba_t *a, id fn, u8 kind) {
    id nm = fn ? MSG0(id, fn, "name") : 0;
    const char *s = nm ? cx_cstr(nm) : 0;
    if (!s) return 1;
    for (u32 i = 0; i < a->n; i++) if (a->kind[i] == kind && !strcmp(a->name[i], s)) return 1;
    char **n2 = realloc(a->name, sizeof *n2 * (a->n + 1));
    if (!n2) return 0;
    a->name = n2;
    u8 *k2 = realloc(a->kind, a->n + 1);
    if (!k2) return 0;
    a->kind = k2;
    a->name[a->n] = strdup(s);
    a->kind[a->n++] = kind;
    return 1;
}
static void ba_err(id *err, long code, const char *msg) {
    if (!err) return;
    id info = MSG(id, (id)cx_class_named("NSDictionary"), "dictionaryWithObject:forKey:", cx_str(msg), cx_str("NSLocalizedDescription"));
    *err = MSG(id, (id)cx_class_named("NSError"), "errorWithDomain:code:userInfo:", cx_str("MTLBinaryArchiveDomain"), code, info);
}
static const char *url_path(id url) { id p = url ? MSG0(id, url, "path") : 0; return p ? cx_cstr(p) : 0; }
static BOOL ba_load(mba_t *a, const char *path, id *err) {
    int fd = open(path, 0);
    if (fd < 0) { ba_err(err, 1 /* InvalidFile */, "arquivo do binary archive não abre"); return NO; }
    char hdr[10];
    u32 n = 0;
    BOOL ok = read(fd, hdr, 10) == 10 && !memcmp(hdr, MAGIC, 10) && read(fd, &n, 4) == 4 && n < 100000;
    for (u32 i = 0; ok && i < n; i++) {
        u8 kind; u16 len;
        char buf[1024];
        if (read(fd, &kind, 1) != 1 || read(fd, &len, 2) != 2 || len >= sizeof buf || read(fd, buf, len) != len) { ok = NO; break; }
        buf[len] = 0;
        char **n2 = realloc(a->name, sizeof *n2 * (a->n + 1));
        u8 *k2 = realloc(a->kind, a->n + 1);
        if (!n2 || !k2) { ok = NO; break; }
        a->name = n2; a->kind = k2;
        a->name[a->n] = strdup(buf);
        a->kind[a->n++] = kind;
    }
    close(fd);
    if (!ok) ba_err(err, 1, "o arquivo não é um binary archive do mOS");
    return ok;
}
static id dev_new_ba(id s, SEL c, id desc, id *err) {
    (void)c;
    mba_t *a = (mba_t *)cx_alloc_instance(&MBA_c);
    a->dev = s;
    const char *p = url_path(desc ? (id)mtl_dget(desc, "url") : 0);
    if (p && !ba_load(a, p, err)) { objc_release((id)a); return 0; }
    return (id)a;
}
static BOOL ba_add_compute(id s, SEL c, id d, id *err) { (void)c; if (!ba_add_name((mba_t *)s, d ? (id)mtl_dget(d, "computeFunction") : 0, 1)) { ba_err(err, 4, "sem memória"); return NO; } return YES; }
static BOOL ba_add_render(id s, SEL c, id d, id *err) {
    (void)c;
    mba_t *a = (mba_t *)s;
    if (!ba_add_name(a, d ? (id)mtl_dget(d, "vertexFunction") : 0, 2) || !ba_add_name(a, d ? (id)mtl_dget(d, "fragmentFunction") : 0, 2)) { ba_err(err, 4, "sem memória"); return NO; }
    return YES;
}
static BOOL ba_add_tile(id s, SEL c, id d, id *err) { (void)c; if (!ba_add_name((mba_t *)s, d ? (id)mtl_dget(d, "tileFunction") : 0, 3)) { ba_err(err, 4, "sem memória"); return NO; } return YES; }
static BOOL ba_add_fn(id s, SEL c, id d, id lib, id *err) {
    (void)c; (void)lib;
    id nm = d ? (id)mtl_dget(d, "name") : 0;
    mba_t *a = (mba_t *)s;
    const char *str = nm ? cx_cstr(nm) : 0;
    if (!str) return YES;
    char **n2 = realloc(a->name, sizeof *n2 * (a->n + 1));
    u8 *k2 = realloc(a->kind, a->n + 1);
    if (!n2 || !k2) { ba_err(err, 4, "sem memória"); return NO; }
    a->name = n2; a->kind = k2;
    a->name[a->n] = strdup(str);
    a->kind[a->n++] = 4;
    return YES;
}
static BOOL ba_serialize(id s, SEL c, id url, id *err) {
    (void)c;
    mba_t *a = (mba_t *)s;
    const char *p = url_path(url);
    if (!p) { ba_err(err, 2 /* InvalidURL? */, "url inválida"); return NO; }
    int fd = open(p, 0x241 /* O_WRONLY | O_CREAT | O_TRUNC */, 0644);
    if (fd < 0) { ba_err(err, 3, "não foi possível gravar o binary archive"); return NO; }
    BOOL ok = write(fd, MAGIC, 10) == 10 && write(fd, &a->n, 4) == 4;
    for (u32 i = 0; ok && i < a->n; i++) {
        u16 len = (u16)strlen(a->name[i]);
        ok = write(fd, &a->kind[i], 1) == 1 && write(fd, &len, 2) == 2 && write(fd, a->name[i], len) == len;
    }
    close(fd);
    if (!ok) ba_err(err, 3, "erro ao gravar o binary archive");
    return ok;
}
static id ba_dev(id s, SEL c) { (void)c; return ((mba_t *)s)->dev; }
static void ba_dealloc(id s, SEL c) {
    mba_t *a = (mba_t *)s;
    for (u32 i = 0; i < a->n; i++) free(a->name[i]);
    free(a->name);
    free(a->kind);
    IMP sup = cx_lookup(cx_class_named("NSObject"), c);
    if (sup) ((void (*)(id, SEL))sup)(s, c);
}
void mtl_archive_register(cx_class_t *obj, cx_class_t *dev) {
    cx_define_class(&MBA_c, &MBA_m, "MTLSoftBinaryArchive", obj, sizeof(mba_t) - sizeof(mobj_t));
    cx_add_method(&MBA_c, "dealloc", (void *)ba_dealloc);
    cx_add_method(&MBA_c, "device", (void *)ba_dev);
    cx_add_method(&MBA_c, "addComputePipelineFunctionsWithDescriptor:error:", (void *)ba_add_compute);
    cx_add_method(&MBA_c, "addRenderPipelineFunctionsWithDescriptor:error:", (void *)ba_add_render);
    cx_add_method(&MBA_c, "addTileRenderPipelineFunctionsWithDescriptor:error:", (void *)ba_add_tile);
    cx_add_method(&MBA_c, "addFunctionWithDescriptor:library:error:", (void *)ba_add_fn);
    cx_add_method(&MBA_c, "serializeToURL:error:", (void *)ba_serialize);
    cx_add_method(dev, "newBinaryArchiveWithDescriptor:error:", (void *)dev_new_ba);
}
