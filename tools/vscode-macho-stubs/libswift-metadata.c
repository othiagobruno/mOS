/* Metadados e buscas do runtime Swift que o VS Code importa.
 * Error, UInt8 e Optional vêm do SwiftMetadata.swift (swift-frontend).
 * A tupla vazia e o value witness de Builtin.BridgeObject seguem o layout
 * do libswiftCore 6.4 (ponteiro de 8 bytes, kind 0x301 para ()).
 */
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#if !defined(MOS_HOST_TEST)
#define MOS_SWIFT __attribute__((swiftcall))
#define MOS_EXPORT __attribute__((visibility("default")))
#else
#define MOS_SWIFT
#define MOS_EXPORT
#endif

typedef MOS_SWIFT void *(*mos_vw_init_buf)(void *dest, void *src, void *self);
typedef MOS_SWIFT void (*mos_vw_destroy)(void *object, void *self);
typedef MOS_SWIFT void *(*mos_vw_copy)(void *dest, void *src, void *self);
typedef MOS_SWIFT unsigned (*mos_vw_get_tag)(const void *obj, unsigned empty_cases, void *self);
typedef MOS_SWIFT void (*mos_vw_store_tag)(void *obj, unsigned which, unsigned empty_cases, void *self);

typedef struct {
    mos_vw_init_buf initializeBufferWithCopyOfBuffer;
    mos_vw_destroy destroy;
    mos_vw_copy initializeWithCopy;
    mos_vw_copy assignWithCopy;
    mos_vw_copy initializeWithTake;
    mos_vw_copy assignWithTake;
    mos_vw_get_tag getEnumTagSinglePayload;
    mos_vw_store_tag storeEnumTagSinglePayload;
    size_t size;
    size_t stride;
    uint32_t flags;
    uint32_t extraInhabitantCount;
} mos_vwt_t;

static MOS_SWIFT void *mos_copy_ptr(void *dest, void *src, void *self) {
    (void)self;
    memcpy(dest, src, sizeof(void *));
    return dest;
}
static MOS_SWIFT void mos_noop_destroy(void *object, void *self) {
    (void)object;
    (void)self;
}
static MOS_SWIFT unsigned mos_ptr_get_tag(const void *obj, unsigned empty_cases, void *self) {
    (void)self;
    uintptr_t v = *(const uintptr_t *)obj;
    if (empty_cases && v == 0) return empty_cases;
    return 0;
}
static MOS_SWIFT void mos_ptr_store_tag(void *obj, unsigned which, unsigned empty_cases, void *self) {
    (void)self;
    (void)empty_cases;
    if (which != 0) *(uintptr_t *)obj = 0;
}
static MOS_SWIFT void *mos_copy_empty(void *dest, void *src, void *self) {
    (void)src;
    (void)self;
    return dest;
}
static MOS_SWIFT unsigned mos_empty_get_tag(const void *obj, unsigned empty_cases, void *self) {
    (void)obj;
    (void)self;
    (void)empty_cases;
    return 0;
}
static MOS_SWIFT void mos_empty_store_tag(void *obj, unsigned which, unsigned empty_cases, void *self) {
    (void)obj;
    (void)which;
    (void)empty_cases;
    (void)self;
}

/* $sBOWV: 8 bytes, alinhamento 8, não-POD, 4096 extra inhabitants. */
MOS_EXPORT const mos_vwt_t _mos_bowv __asm__("_$sBOWV") = {
    mos_copy_ptr, mos_noop_destroy, mos_copy_ptr, mos_copy_ptr,
    mos_copy_ptr, mos_copy_ptr, mos_ptr_get_tag, mos_ptr_store_tag,
    8, 8, 0x00010007u, 0x00001000u
};

MOS_EXPORT const mos_vwt_t _mos_ytwv __asm__("_$sytWV") = {
    mos_copy_empty, mos_noop_destroy, mos_copy_empty, mos_copy_empty,
    mos_copy_empty, mos_copy_empty, mos_empty_get_tag, mos_empty_store_tag,
    0, 1, 0, 0
};

/* $sytN: metadado da tupla vazia. kind 0x301, zero elementos. */
typedef struct {
    const mos_vwt_t *vw;
    uint64_t kind;
    uint64_t num_elements;
    const char *labels;
} mos_tuple_md_t;

MOS_EXPORT const mos_tuple_md_t _mos_ytn __asm__("_$sytN") = {
    &_mos_ytwv, 0x301, 0, 0
};

#if !defined(MOS_HOST_TEST)
extern const void _mos_uint8_n __asm__("_$ss5UInt8VN");
extern const void _mos_error_mp __asm__("_$ss5ErrorMp");
extern const void _mos_sq_mn __asm__("_$sSqMn");
#else
static const char _mos_uint8_n;
static const char _mos_error_mp;
static const char _mos_sq_mn;
#endif

typedef struct {
    uint64_t kind;
    const void *description;
    const void *payload;
} mos_opt_md_t;

static const void *mos_optional_metadata(const void *payload) {
    mos_opt_md_t *md = malloc(sizeof *md);
    if (!md) return 0;
    md->kind = 0x202;
    md->description = &_mos_sq_mn;
    md->payload = payload;
    return md;
}

const void *mos_demangle_type(const char *s, size_t n, const void *const *args, size_t *used);

const void *mos_demangle_type(const char *s, size_t n, const void *const *args, size_t *used) {
    if (n >= 2 && s[0] == '$' && s[1] == 's') {
        size_t inner = 0;
        const void *t = mos_demangle_type(s + 2, n - 2, args, &inner);
        if (used) *used = inner + 2;
        return t;
    }
    if (n >= 2 && s[0] == 'y' && s[1] == 't') {
        if (used) *used = 2;
        return &_mos_ytn.kind;
    }
    if (n >= 8 && memcmp(s, "s5UInt8V", 8) == 0) {
        if (used) *used = 8;
        return &_mos_uint8_n;
    }
    if (n >= 8 && memcmp(s, "s5ErrorP", 8) == 0) {
        if (used) *used = 8;
        return &_mos_error_mp;
    }
    if (n >= 1 && s[0] == 'x') {
        if (used) *used = 1;
        return args ? args[0] : 0;
    }
    if (n >= 2 && s[0] == 'S' && s[1] == 'q') {
        size_t inner_used = 0;
        const void *inner = mos_demangle_type(s + 2, n - 2, args, &inner_used);
        if (!inner) return 0;
        if (used) *used = 2 + inner_used;
        return mos_optional_metadata(inner);
    }
    return 0;
}

MOS_EXPORT MOS_SWIFT const void *swift_getTypeByMangledNameInContext(
    const char *name, size_t len, const void *context, const void *const *args)
    __asm__("_swift_getTypeByMangledNameInContext");
MOS_EXPORT MOS_SWIFT const void *swift_getTypeByMangledNameInContext(
    const char *name, size_t len, const void *context, const void *const *args) {
    (void)context;
    return mos_demangle_type(name, len, args, 0);
}

MOS_EXPORT MOS_SWIFT const void *swift_getTypeByMangledNameInContextInMetadataState(
    size_t state, const char *name, size_t len, const void *context, const void *const *args)
    __asm__("_swift_getTypeByMangledNameInContextInMetadataState");
MOS_EXPORT MOS_SWIFT const void *swift_getTypeByMangledNameInContextInMetadataState(
    size_t state, const char *name, size_t len, const void *context, const void *const *args) {
    (void)state;
    return swift_getTypeByMangledNameInContext(name, len, context, args);
}

typedef struct {
    uint32_t flags;
    int32_t default_impl;
} mos_req_t;

static size_t mos_req_index(const void *base, const void *req) {
    return ((uintptr_t)req - (uintptr_t)base) / sizeof(mos_req_t);
}

typedef const void *(*mos_assoc_fn)(const void *type, void *wtable);
typedef const void *(*mos_conf_fn)(const void *type, void *wtable, const void *assoc);

MOS_EXPORT MOS_SWIFT const void *swift_getAssociatedTypeWitness(
    size_t request, void *wtable, const void *conforming, const void *req_base, const void *assoc)
    __asm__("_swift_getAssociatedTypeWitness");
MOS_EXPORT MOS_SWIFT const void *swift_getAssociatedTypeWitness(
    size_t request, void *wtable, const void *conforming, const void *req_base, const void *assoc) {
    (void)request;
    if (!wtable || !req_base || !assoc) return 0;
    const mos_req_t *req = assoc;
    const void *witness = ((const void **)wtable)[mos_req_index(req_base, assoc)];
    if ((req->flags & 0x0F) == 7 && witness) {
        return ((mos_assoc_fn)witness)(conforming, wtable);
    }
    return witness;
}

MOS_EXPORT MOS_SWIFT const void *swift_getAssociatedConformanceWitness(
    void *wtable, const void *conforming, const void *req_base, const void *assoc)
    __asm__("_swift_getAssociatedConformanceWitness");
MOS_EXPORT MOS_SWIFT const void *swift_getAssociatedConformanceWitness(
    void *wtable, const void *conforming, const void *req_base, const void *assoc) {
    if (!wtable || !req_base || !assoc) return 0;
    const mos_req_t *req = assoc;
    const void *witness = ((const void **)wtable)[mos_req_index(req_base, assoc)];
    if ((req->flags & 0x0F) == 8 && witness)
        return ((mos_conf_fn)witness)(conforming, wtable, 0);
    return witness;
}
