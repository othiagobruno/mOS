/*
 * Runtime Swift do Grok, o mesmo de libswift-runtime.c (#82/#94).
 * Sem o prefixo Mach-O: o ELF exporta swift_allocObject, que o machocheck procura.
 */
#include <stdint.h>
#if !defined(__STDC_HOSTED__) || __STDC_HOSTED__ == 0
typedef uint8_t uint_least8_t;
typedef uint16_t uint_least16_t;
typedef uint32_t uint_least32_t;
typedef uint64_t uint_least64_t;
typedef int8_t int_least8_t;
typedef int16_t int_least16_t;
typedef int32_t int_least32_t;
typedef int64_t int_least64_t;
typedef int8_t int_fast8_t;
typedef int16_t int_fast16_t;
typedef int32_t int_fast32_t;
typedef int64_t int_fast64_t;
typedef uint8_t uint_fast8_t;
typedef uint16_t uint_fast16_t;
typedef uint32_t uint_fast32_t;
typedef uint64_t uint_fast64_t;
typedef int64_t intmax_t;
typedef uint64_t uintmax_t;
#endif
typedef int wchar_t;
#define _WCHAR_T
#include <stddef.h>
#include <stdatomic.h>
/* Runtime Swift do caminho de carga das imagens que o VS Code abre
 * (libmsalruntime e vsce-sign). Alocação, retenção, swift_once e a tabela
 * de testemunhas são de verdade. Ponteiro nativo: cabeçalho { metadata, contagem }.
 * Objeto de ponte (bridge) usa os 3 bits baixos livres do ABI.
 */

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
int posix_memalign(void **out, size_t align, size_t n);
void abort(void);

void *swift_allocObject(const void *metadata, size_t size, size_t align_mask);
void *swift_retain(void *object);
void swift_release(void *object);

void *swift_errorRetain(void *object);
void swift_errorRelease(void *object);

void *swift_errorRetain(void *object) { return swift_retain(object); }
void swift_errorRelease(void *object) { swift_release(object); }

typedef struct { void *object; void *buffer; } mos_box_t;

mos_box_t swift_allocBox(const void *type);
mos_box_t swift_allocBox(const void *type) {
    void *obj = swift_allocObject(type, 256, 15);
    mos_box_t box;
    box.object = obj;
    box.buffer = obj ? (char *)obj + 16 : 0;
    return box;
}

mos_box_t swift_allocError(const void *type, const void *witness, void *initial, int is_take);
mos_box_t swift_allocError(const void *type, const void *witness, void *initial, int is_take) {
    (void)witness;
    (void)is_take;
    mos_box_t box = swift_allocBox(type);
    if (box.buffer && initial) memcpy(box.buffer, initial, sizeof(void *));
    return box;
}

void swift_once(long *token, void (*fn)(void *), void *ctx);
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

const void *swift_getWitnessTable(const mos_conf_t *desc, const void *type, const void *const *args);
const void *swift_getWitnessTable(const mos_conf_t *desc, const void *type, const void *const *args) {
    (void)type;
    (void)args;
    if (!desc) return 0;
    const char *field = (const char *)&desc->witness_off;
    return (const void *)(field + desc->witness_off);
}

const void *swift_checkMetadataState(size_t request, const void *metadata);
const void *swift_checkMetadataState(size_t request, const void *metadata) {
    (void)request;
    return metadata;
}

void swift_deletedMethodError(void);
void swift_deletedMethodError(void) { abort(); }

typedef struct { uint32_t major, minor, patch; } mos_osver_t;
mos_osver_t _swift_stdlib_operatingSystemVersion(void);
mos_osver_t _swift_stdlib_operatingSystemVersion(void) {
    mos_osver_t v;
    v.major = 14;
    v.minor = 0;
    v.patch = 0;
    return v;
}

void swift_willThrow(void);
void swift_willThrow(void) {}

void *swift_dynamicCast(void *dest, void *src, const void *src_type, const void *dst_type, unsigned flags);
void *swift_dynamicCast(void *dest, void *src, const void *src_type, const void *dst_type, unsigned flags) {
    (void)flags;
    if (src_type != dst_type) return 0;
    if (dest && src) memcpy(dest, src, sizeof(void *));
    return dest ? dest : src;
}
