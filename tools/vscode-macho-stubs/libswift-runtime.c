/* Runtime Swift do caminho de carga das imagens que o VS Code abre
 * (libmsalruntime e vsce-sign). Alocação, retenção, swift_once e a tabela
 * de testemunhas são de verdade. Ponteiro nativo: cabeçalho { metadata, contagem }.
 * Objeto de ponte (bridge) usa os 3 bits baixos livres do ABI.
 */
#include <stdatomic.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
    const void *metadata;
    _Atomic uint64_t refcount;
} mos_heap_t;

#define MOS_SPARE 7u

void *swift_allocObject(const void *metadata, size_t size, size_t align_mask) __asm__("_swift_allocObject");
void *swift_retain(void *object) __asm__("_swift_retain");
void swift_release(void *object) __asm__("_swift_release");

static size_t mos_align(size_t n, size_t align) {
    return (n + align - 1) & ~(align - 1);
}

void *swift_allocObject(const void *metadata, size_t size, size_t align_mask) {
    size_t align = align_mask + 1;
    if (align < sizeof(void *)) align = sizeof(void *);
    if (size < sizeof(mos_heap_t)) size = sizeof(mos_heap_t);
    size = mos_align(size, align);
    void *mem = 0;
    if (posix_memalign(&mem, align, size) != 0) return 0;
    memset(mem, 0, size);
    mos_heap_t *obj = mem;
    obj->metadata = metadata;
    atomic_store(&obj->refcount, 1);
    return obj;
}

void *swift_retain(void *object) {
    if (!object) return 0;
    mos_heap_t *obj = object;
    atomic_fetch_add(&obj->refcount, 1);
    return object;
}

void swift_release(void *object) {
    if (!object) return;
    mos_heap_t *obj = object;
    uint64_t prev = atomic_fetch_sub(&obj->refcount, 1);
    if (prev == 1) free(obj);
}

void *swift_bridgeObjectRetain(void *object) __asm__("_swift_bridgeObjectRetain");
void swift_bridgeObjectRelease(void *object) __asm__("_swift_bridgeObjectRelease");

void *swift_bridgeObjectRetain(void *object) {
    uintptr_t raw = (uintptr_t)object;
    void *native = (void *)(raw & ~(uintptr_t)MOS_SPARE);
    if (native) swift_retain(native);
    return object;
}

void swift_bridgeObjectRelease(void *object) {
    uintptr_t raw = (uintptr_t)object;
    void *native = (void *)(raw & ~(uintptr_t)MOS_SPARE);
    if (native) swift_release(native);
}

void *swift_errorRetain(void *object) __asm__("_swift_errorRetain");
void swift_errorRelease(void *object) __asm__("_swift_errorRelease");

void *swift_errorRetain(void *object) { return swift_retain(object); }
void swift_errorRelease(void *object) { swift_release(object); }

typedef struct { void *object; void *buffer; } mos_box_t;

mos_box_t swift_allocBox(const void *type) __asm__("_swift_allocBox");
mos_box_t swift_allocBox(const void *type) {
    void *obj = swift_allocObject(type, 256, 15);
    mos_box_t box;
    box.object = obj;
    box.buffer = obj ? (char *)obj + sizeof(mos_heap_t) : 0;
    return box;
}

mos_box_t swift_allocError(const void *type, const void *witness, void *initial, int is_take) __asm__("_swift_allocError");
mos_box_t swift_allocError(const void *type, const void *witness, void *initial, int is_take) {
    (void)witness;
    (void)is_take;
    mos_box_t box = swift_allocBox(type);
    if (box.buffer && initial) memcpy(box.buffer, initial, sizeof(void *));
    return box;
}

void swift_once(long *token, void (*fn)(void *), void *ctx) __asm__("_swift_once");
void swift_once(long *token, void (*fn)(void *), void *ctx) {
    long cur = atomic_load((_Atomic long *)token);
    if (cur == 1) return;
    long expected = 0;
    if (atomic_compare_exchange_strong((_Atomic long *)token, &expected, 2)) {
        if (fn) fn(ctx);
        atomic_store((_Atomic long *)token, 1);
        return;
    }
    while (atomic_load((_Atomic long *)token) != 1) {}
}

typedef struct {
    int32_t protocol_off;
    int32_t type_off;
    int32_t witness_off;
    uint32_t flags;
} mos_conf_t;

const void *swift_getWitnessTable(const mos_conf_t *desc, const void *type, const void *const *args) __asm__("_swift_getWitnessTable");
const void *swift_getWitnessTable(const mos_conf_t *desc, const void *type, const void *const *args) {
    (void)type;
    (void)args;
    if (!desc) return 0;
    const char *field = (const char *)&desc->witness_off;
    return (const void *)(field + desc->witness_off);
}

const void *swift_checkMetadataState(size_t request, const void *metadata) __asm__("_swift_checkMetadataState");
const void *swift_checkMetadataState(size_t request, const void *metadata) {
    (void)request;
    return metadata;
}

void swift_deletedMethodError(void) __asm__("_swift_deletedMethodError");
void swift_deletedMethodError(void) { abort(); }

typedef struct { uint32_t major, minor, patch; } mos_osver_t;
mos_osver_t _swift_stdlib_operatingSystemVersion(void) __asm__("__swift_stdlib_operatingSystemVersion");
mos_osver_t _swift_stdlib_operatingSystemVersion(void) {
    mos_osver_t v;
    v.major = 14;
    v.minor = 0;
    v.patch = 0;
    return v;
}

void swift_willThrow(void) __asm__("_swift_willThrow");
void swift_willThrow(void) {}

void *swift_dynamicCast(void *dest, void *src, const void *src_type, const void *dst_type, unsigned flags) __asm__("_swift_dynamicCast");
void *swift_dynamicCast(void *dest, void *src, const void *src_type, const void *dst_type, unsigned flags) {
    (void)flags;
    if (src_type != dst_type) return 0;
    if (dest && src) memcpy(dest, src, sizeof(void *));
    return dest ? dest : src;
}
