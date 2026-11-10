/*
 * Escritor do binário OSSerialize. Sem objetos CF: o IOCFSerialize monta as
 * chamadas, e o teste de host confere os tags do cabeçalho da Apple.
 */
#ifdef MOS_OSSER_HOST
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
typedef uint8_t u8;
typedef uint32_t u32;
#else
#include "libc.h"
#endif
#include "osser.h"

enum {
    kOSSerializeDictionary = 0x01000000u,
    kOSSerializeString = 0x09000000u,
    kOSSerializeObject = 0x0c000000u,
    kOSSerializeEndCollection = 0x80000000u
};

static int grow(osser_t *s, u32 need) {
    if (s->n + need <= s->cap) return 1;
    u32 c = s->cap ? s->cap : 64;
    while (c < s->n + need) c *= 2;
    u8 *n = realloc(s->p, c);
    if (!n) return 0;
    s->p = n;
    s->cap = c;
    return 1;
}

void osser_init(osser_t *s) {
    memset(s, 0, sizeof *s);
    s->end = 1;
    grow(s, 4);
    if (s->p) memset(s->p, 0, 4);
    if (s->p) s->p[0] = 0xd3;
    s->n = 4;
}

void osser_free(osser_t *s) { free(s->p); s->p = 0; }

int osser_add(osser_t *s, const void *id, u32 key, const void *bits, u32 size) {
    if (id) {
        for (int i = 0; i < s->nseen; i++) {
            if (s->seen[i] == id) {
                u32 ref = kOSSerializeObject | (u32)i;
                if (s->end) { ref |= kOSSerializeEndCollection; s->end = 0; }
                if (!grow(s, 4)) return 0;
                memcpy(s->p + s->n, &ref, 4);
                s->n += 4;
                return 1;
            }
        }
        if (s->nseen < 128) s->seen[s->nseen++] = id;
    }
    if (s->end) { key |= kOSSerializeEndCollection; s->end = 0; }
    u32 align = (4u + size + 3u) & ~3u;
    if (!grow(s, align)) return 0;
    memset(s->p + s->n, 0, align);
    memcpy(s->p + s->n, &key, 4);
    if (size && bits) memcpy(s->p + s->n + 4, bits, size);
    s->n += align;
    return 1;
}

void osser_set_end(osser_t *s, int end) { s->end = end; }
const u8 *osser_bytes(osser_t *s) { return s->p; }
u32 osser_len(osser_t *s) { return s->n; }

int mos_osser_selftest(void) {
    osser_t s;
    osser_init(&s);
    static const char ab[] = "ab";
    int ok = osser_add(&s, ab, kOSSerializeString | 2u, ab, 2);
    static const u8 exp[] = {0xd3, 0, 0, 0, 0x02, 0, 0, 0x89, 'a', 'b', 0, 0};
    ok = ok && osser_len(&s) == sizeof exp && !memcmp(osser_bytes(&s), exp, sizeof exp);
    osser_free(&s);

    osser_init(&s);
    int mark = 1;
    ok = ok && osser_add(&s, &mark, kOSSerializeDictionary, 0, 0);
    static const u8 empty[] = {0xd3, 0, 0, 0, 0x00, 0, 0, 0x81};
    ok = ok && osser_len(&s) == sizeof empty && !memcmp(osser_bytes(&s), empty, sizeof empty);
    osser_free(&s);
    return ok ? 0 : 1;
}
