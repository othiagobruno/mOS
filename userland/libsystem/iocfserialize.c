/*
 * IOCFSerialize(objeto, opções) devolve um CFData com o binário OSSerialize.
 * As opções são aceitas e ignoradas: o consumidor do IOKit lê o formato binário.
 */
#include "foundation.h"
#include "libc.h"

id CFDataCreate(void *alloc, const u8 *bytes, long n);
u64 CFGetTypeID(id o);
u8 CFBooleanGetValue(id b);
u8 CFNumberGetValue(id n, long type, void *out);
u8 CFNumberIsFloatType(id n);

typedef struct { void *isa; u8 *bytes; u64 len; } iocf_data_t;

enum {
    kOSSerializeDictionary = 0x01000000u,
    kOSSerializeArray = 0x02000000u,
    kOSSerializeNumber = 0x04000000u,
    kOSSerializeSymbol = 0x08000000u,
    kOSSerializeString = 0x09000000u,
    kOSSerializeData = 0x0a000000u,
    kOSSerializeBoolean = 0x0b000000u
};

#include "osser.h"

static int ser(osser_t *s, id o);

static int ser_dict(osser_t *s, id o) {
    nsdict_t *d = (nsdict_t *)o;
    if (!osser_add(s, o, kOSSerializeDictionary | (u32)d->n, 0, 0)) return 0;
    for (u64 i = 0; i < d->n; i++) {
        const char *k = cx_cstr(d->keys[i]);
        if (!k) k = "";
        u32 len = (u32)strlen(k) + 1;
        if (!osser_add(s, d->keys[i], kOSSerializeSymbol | len, k, len)) return 0;
        osser_set_end(s, i + 1 == d->n);
        if (!ser(s, d->vals[i])) return 0;
    }
    return 1;
}

static int ser_array(osser_t *s, id o) {
    nsarr_t *a = (nsarr_t *)o;
    if (!osser_add(s, o, kOSSerializeArray | (u32)a->n, 0, 0)) return 0;
    for (u64 i = 0; i < a->n; i++) {
        osser_set_end(s, i + 1 == a->n);
        if (!ser(s, a->items[i])) return 0;
    }
    return 1;
}

static int ser(osser_t *s, id o) {
    if (!o) return 0;
    u64 t = CFGetTypeID(o);
    if (t == 18) return ser_dict(s, o);
    if (t == 19) return ser_array(s, o);
    if (t == 21) return osser_add(s, o, kOSSerializeBoolean | (CFBooleanGetValue(o) ? 1u : 0u), 0, 0);
    if (t == 22) {
        u64 v = 0;
        if (CFNumberIsFloatType(o)) {
            double d = 0;
            CFNumberGetValue(o, 6, &d);
            memcpy(&v, &d, 8);
        } else {
            CFNumberGetValue(o, 4, &v);
        }
        return osser_add(s, o, kOSSerializeNumber | 64u, &v, 8);
    }
    if (t == 7) {
        const char *c = cx_cstr(o);
        if (!c) c = "";
        u32 n = (u32)strlen(c);
        return osser_add(s, o, kOSSerializeString | n, c, n);
    }
    if (t == 20) {
        iocf_data_t *d = (iocf_data_t *)o;
        return osser_add(s, o, kOSSerializeData | (u32)d->len, d->bytes, (u32)d->len);
    }
    return 0;
}

id IOCFSerialize(id obj, u32 options) {
    (void)options;
    if (!obj) return 0;
    osser_t s;
    osser_init(&s);
    if (!osser_bytes(&s) || !ser(&s, obj)) { osser_free(&s); return 0; }
    id data = CFDataCreate(0, osser_bytes(&s), (long)osser_len(&s));
    osser_free(&s);
    return data;
}
