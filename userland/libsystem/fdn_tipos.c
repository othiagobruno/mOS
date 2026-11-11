/*
 * Overlay Swift do Foundation: Date, UUID, CocoaError, POSIXError, URLError, Notification
 * e NSNotFound. Os descritores, a metadata e os acessores estão em fdn_meta.S.
 *
 * Nenhum desses tipos é @frozen na Apple, então o app os trata como resilientes: pega a
 * metadata no acessor, aloca pelo tamanho da tabela de value witnesses e passa o valor
 * por endereço (self em r13, argumento em rdi/rsi..., resultado indireto em rax).
 * O __attribute__((swiftcall)) com swift_context / swift_indirect_result emite essa
 * convenção; o clang usa a mesma divisão de agregados em registradores que o Swift.
 *
 * Layout interno (só nós o lemos):
 *   Date               Double, segundos desde 2001-01-01 00:00:00 UTC (8 bytes, POD)
 *   UUID               uuid_t (16 bytes, POD)
 *   CocoaError.Code,
 *   URLError.Code      Int (8 bytes, POD)
 *   CocoaError, POSIXError, URLError
 *                      NSError, referência forte (8 bytes)
 *   Notification       NSNotification, referência forte (8 bytes)
 * Nenhum tem inabitante extra: Optional<T> usa o byte de tag depois do valor, com as
 * mesmas regras do runtime (getEnumTagSinglePayload / storeEnumTagSinglePayload).
 *
 * String devolvida: até 15 bytes, a forma pequena; maior, a forma imortal de literal
 * (countAndFlags = n | ASCII/NFC/tail-allocated, objeto = (bytes - 32) | 0x8000...),
 * com os bytes num bloco que nunca é liberado.
 *
 * O lado Objective-C (NSDate, NSUUID, NSError, NSNotification) fica em fdn_ns_*:
 * fdn_tipos_ns.c no libSystem, versões falsas em tools/fdn_tipos_test.c no teste de host.
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "fdn_tipos.h"

#define SW __attribute__((swiftcall))
#define CTX __attribute__((swift_context))
#define USED __attribute__((used))

/*
 * Resultado indireto: o Swift passa o endereço em rax. O swift_indirect_result do clang do
 * zig 0.17.1 marca o parâmetro como sret(ptr) de 8 bytes e o otimizador apaga a escrita nos
 * primeiros 8 bytes (visto no objdump). Por isso a entrada é um trampolim: rax vira o
 * primeiro argumento comum e os inteiros andam uma casa (até cinco; os Double não mudam).
 */
#if defined(__x86_64__)
#define SRET(sym, impl) __asm__(".text\n.p2align 4\n.globl \"" sym "\"\n\"" sym "\":\n" \
    "  movq %r8, %r9\n  movq %rcx, %r8\n  movq %rdx, %rcx\n  movq %rsi, %rdx\n" \
    "  movq %rdi, %rsi\n  movq %rax, %rdi\n  jmp " #impl "\n")
#else
#define SRET(sym, impl)
#endif

#define FDN_REF_1970 978307200.0
#define FDN_DISTANT_FUTURE 63113904000.0
#define FDN_DISTANT_PAST (-63114076800.0)

/* ---------------------------------------------------------------- value witnesses */

static const fdn_vwt *vwt_of(const void *md) { return ((const fdn_vwt *const *)md)[-1]; }

/* Optional e afins sem inabitante extra: a tag vai nos bytes depois do valor. */
static unsigned tag_bytes_for(size_t size, unsigned empty) {
    unsigned tags = 1;
    if (empty) {
        if (size >= 4) tags += 1;
        else {
            unsigned bits = (unsigned)size * 8u;
            tags += (empty + ((1u << bits) - 1u)) >> bits;
        }
    }
    return tags <= 1 ? 0 : tags < 256 ? 1 : tags < 65536 ? 2 : 4;
}

static unsigned load_el(const uint8_t *p, size_t n) {
    unsigned v = 0;
    if (n > 4) n = 4;
    for (size_t i = 0; i < n; i++) v |= (unsigned)p[i] << (8 * i);
    return v;
}

static void store_el(uint8_t *p, unsigned v, size_t n) {
    for (size_t i = 0; i < n; i++) p[i] = i < 4 ? (uint8_t)(v >> (8 * i)) : 0;
}

unsigned fdn_enum_tag_get(const void *value, unsigned empty, size_t size) {
    const uint8_t *p = value;
    unsigned nb, extra;
    if (!empty) return 0;
    nb = tag_bytes_for(size, empty);
    extra = load_el(p + size, nb);
    if (!extra) return 0;
    {
        unsigned hi = size >= 4 ? 0 : (extra - 1u) << (size * 8u);
        unsigned lo = load_el(p, size);
        return (hi | lo) + 1u;
    }
}

void fdn_enum_tag_store(void *value, unsigned which, unsigned empty, size_t size) {
    uint8_t *p = value;
    unsigned nb = tag_bytes_for(size, empty);
    unsigned idx, payload, tag;
    if (which == 0) {
        if (nb) store_el(p + size, 0, nb);
        return;
    }
    idx = which - 1u;
    if (size >= 4) { tag = 1; payload = idx; }
    else {
        unsigned bits = (unsigned)size * 8u;
        tag = 1u + (idx >> bits);
        payload = idx & ((1u << bits) - 1u);
    }
    if (size) store_el(p, payload, size);
    if (nb) store_el(p + size, tag, nb);
}

static SW void *pod_copy(void *d, void *s, const void *md) { memmove(d, s, vwt_of(md)->size); return d; }
static SW void pod_destroy(void *o, const void *md) { (void)o; (void)md; }
static SW unsigned v_get_tag(const void *v, unsigned empty, const void *md) {
    return fdn_enum_tag_get(v, empty, vwt_of(md)->size);
}
static SW void v_store_tag(void *v, unsigned which, unsigned empty, const void *md) {
    fdn_enum_tag_store(v, which, empty, vwt_of(md)->size);
}

/* referência forte a objeto ObjC: copiar retém, destruir solta, mover é memcpy */
static SW void *obj_init_copy(void *d, void *s, const void *md) {
    void *o = *(void **)s;
    (void)md;
    fdn_ns_retain(o);
    *(void **)d = o;
    return d;
}
static SW void obj_destroy(void *p, const void *md) { (void)md; fdn_ns_release(*(void **)p); }
static SW void *obj_assign_copy(void *d, void *s, const void *md) {
    void *n = *(void **)s, *o = *(void **)d;
    (void)md;
    fdn_ns_retain(n);
    *(void **)d = n;
    fdn_ns_release(o);
    return d;
}
static SW void *obj_init_take(void *d, void *s, const void *md) { (void)md; *(void **)d = *(void **)s; return d; }
static SW void *obj_assign_take(void *d, void *s, const void *md) {
    void *o = *(void **)d;
    (void)md;
    *(void **)d = *(void **)s;
    fdn_ns_release(o);
    return d;
}

#define VW_ALIGN8 7u
#define VW_NONPOD 0x00010000u

const fdn_vwt fdn_vwt_pod8 = {
    pod_copy, pod_destroy, pod_copy, pod_copy, pod_copy, pod_copy, v_get_tag, v_store_tag,
    8, 8, VW_ALIGN8, 0
};
const fdn_vwt fdn_vwt_pod16 = {
    pod_copy, pod_destroy, pod_copy, pod_copy, pod_copy, pod_copy, v_get_tag, v_store_tag,
    16, 16, VW_ALIGN8, 0
};
const fdn_vwt fdn_vwt_obj = {
    obj_init_copy, obj_destroy, obj_init_copy, obj_assign_copy, obj_init_take, obj_assign_take,
    v_get_tag, v_store_tag, 8, 8, VW_ALIGN8 | VW_NONPOD, 0
};

/* enum sem payload em um byte: o byte é o índice do caso */
static SW unsigned e1_get_tag(const void *v, const void *md) { (void)md; return *(const uint8_t *)v; }
static SW void e1_project(void *v, const void *md) { (void)v; (void)md; }
static SW void e1_inject(void *v, unsigned tag, const void *md) { (void)md; *(uint8_t *)v = (uint8_t)tag; }

const fdn_evwt fdn_evwt_tag1 = {
    { pod_copy, pod_destroy, pod_copy, pod_copy, pod_copy, pod_copy, v_get_tag, v_store_tag,
      1, 1, 0x00200000u, 0 },
    e1_get_tag, e1_project, e1_inject
};

/* ---------------------------------------------------------------- String */

fdn_str fdn_str_make(const char *s, long n) {
    fdn_str r;
    int ascii = 1;
    if (!s) n = 0;
    if (n < 0) n = (long)strlen(s);
    for (long i = 0; i < n; i++)
        if ((unsigned char)s[i] >= 0x80) { ascii = 0; break; }
    if (n <= 15) {
        uint8_t b[16];
        memset(b, 0, sizeof b);
        if (n) memcpy(b, s, (size_t)n);
        b[15] = (uint8_t)((ascii ? 0xE0 : 0xA0) | n);
        memcpy(&r.w0, b, 8);
        memcpy(&r.w1, b + 8, 8);
        return r;
    }
    {
        /* imortal: o runtime não conta referências e o bloco fica para sempre */
        char *buf = malloc((size_t)n + 1);
        if (!buf) return fdn_str_make(s, 15);
        memcpy(buf, s, (size_t)n);
        buf[n] = 0;
        r.w0 = (uint64_t)n | (ascii ? 0xD000000000000000ULL : 0x1000000000000000ULL);
        r.w1 = ((uint64_t)(uintptr_t)buf - 32u) | 0x8000000000000000ULL;
        return r;
    }
}

/* Lê os bytes UTF-8: pequena, nativa ou literal (bytes depois do cabeçalho de 32) ou ponte de NSString. */
long fdn_str_read(fdn_str s, char *buf, long cap) {
    const char *p = 0;
    long n = 0;
    uint8_t small[16];
    if (cap <= 0) return -1;
    memcpy(small, &s.w0, 8);
    memcpy(small + 8, &s.w1, 8);
    if (s.w1 & (1ULL << 61)) {
        n = small[15] & 0x0F;
        p = (const char *)small;
    } else if (s.w1 & (1ULL << 62)) {
        p = fdn_ns_utf8((void *)(uintptr_t)(s.w1 & 0x0FFFFFFFFFFFFFFFULL));
        n = p ? (long)strlen(p) : 0;
    } else if (s.w0 & 0x1000000000000000ULL) {
        n = (long)(s.w0 & 0x0000FFFFFFFFFFFFULL);
        p = (const char *)(uintptr_t)((s.w1 & 0x0FFFFFFFFFFFFFFFULL) + 32u);
    } else {
        buf[0] = 0;
        return -1;
    }
    if (n >= cap) n = cap - 1;
    if (n > 0 && p) memcpy(buf, p, (size_t)n);
    buf[n > 0 ? n : 0] = 0;
    return n;
}

/* ---------------------------------------------------------------- Date */

static void civil_from_days(long long z, int *y, unsigned *m, unsigned *d) {
    long long era, yy;
    unsigned doe, yoe, doy, mp;
    z += 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = (unsigned)(z - era * 146097);
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    yy = (long long)yoe + era * 400;
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    mp = (5 * doy + 2) / 153;
    *d = doy - (153 * mp + 2) / 5 + 1;
    *m = mp < 10 ? mp + 3 : mp - 9;
    *y = (int)(yy + (*m <= 2));
}

/*
 * description de Date, como o swift-foundation: fora de [distantPast, distantFuture] o texto
 * fixo; senão time_t(timeIntervalSince1970) (trunca para zero) no gmtime, "%Y-%m-%d %H:%M:%S +0000".
 */
int fdn_date_text(double ref, char *buf, int cap) {
    double unix_s = ref + FDN_REF_1970;
    long long secs, days, rem;
    int y;
    unsigned m, d;
    if (!(ref >= FDN_DISTANT_PAST && ref <= FDN_DISTANT_FUTURE)) {
        static const char na[] = "<description unavailable>";
        int k = (int)sizeof na - 1;
        if (k >= cap) k = cap - 1;
        memcpy(buf, na, (size_t)k);
        buf[k] = 0;
        return k;
    }
    secs = (long long)unix_s;
    days = secs / 86400;
    rem = secs % 86400;
    if (rem < 0) { rem += 86400; days--; }
    civil_from_days(days, &y, &m, &d);
    {
        /* snprintf não está em todo lugar: monta à mão */
        char t[40];
        int k = 0, yy = y < 0 ? -y : y;
        char digits[12];
        int nd = 0;
        if (y < 0) t[k++] = '-';
        do { digits[nd++] = (char)('0' + yy % 10); yy /= 10; } while (yy && nd < 11);
        while (nd < 4) digits[nd++] = '0';
        while (nd) t[k++] = digits[--nd];
#define P2(v) do { t[k++] = (char)('0' + (v) / 10); t[k++] = (char)('0' + (v) % 10); } while (0)
        t[k++] = '-'; P2(m);
        t[k++] = '-'; P2(d);
        t[k++] = ' '; P2((int)(rem / 3600));
        t[k++] = ':'; P2((int)(rem / 60 % 60));
        t[k++] = ':'; P2((int)(rem % 60));
#undef P2
        memcpy(t + k, " +0000", 6);
        k += 6;
        if (k >= cap) k = cap - 1;
        memcpy(buf, t, (size_t)k);
        buf[k] = 0;
        return k;
    }
}

static double now_ref(void) { return fdn_ns_now1970() - FDN_REF_1970; }

/* init() */
SRET("$s10Foundation4DateVACycfC", fdn_date_init);
static SW USED void fdn_date_init(double *out) { *out = now_ref(); }
/* static var now */
SRET("$s10Foundation4DateV3nowACvgZ", fdn_date_now);
static SW USED void fdn_date_now(double *out) { *out = now_ref(); }
/* init(timeIntervalSince1970:) */
SRET("$s10Foundation4DateV21timeIntervalSince1970ACSd_tcfC", fdn_date_init1970);
static SW USED void fdn_date_init1970(double *out, double t) { *out = t - FDN_REF_1970; }
/* init(timeIntervalSinceReferenceDate:) */
SRET("$s10Foundation4DateV026timeIntervalSinceReferenceB0ACSd_tcfC", fdn_date_initref);
static SW USED void fdn_date_initref(double *out, double t) { *out = t; }
/* init(timeIntervalSinceNow:) */
SRET("$s10Foundation4DateV20timeIntervalSinceNowACSd_tcfC", fdn_date_initnow);
static SW USED void fdn_date_initnow(double *out, double t) { *out = now_ref() + t; }
/* init(timeInterval:since:) */
SRET("$s10Foundation4DateV12timeInterval5sinceACSd_ACtcfC", fdn_date_initsince);
static SW USED void fdn_date_initsince(double *out, double t, const double *since) { *out = *since + t; }
/* var timeIntervalSince1970 */
SW double fdn_date_get1970(const double *self CTX) __asm__("$s10Foundation4DateV21timeIntervalSince1970Sdvg");
SW double fdn_date_get1970(const double *self CTX) { return *self + FDN_REF_1970; }
/* var timeIntervalSinceReferenceDate */
SW double fdn_date_getref(const double *self CTX) __asm__("$s10Foundation4DateV026timeIntervalSinceReferenceB0Sdvg");
SW double fdn_date_getref(const double *self CTX) { return *self; }
/* static var timeIntervalSinceReferenceDate (agora) */
SW double fdn_date_getref_now(void) __asm__("$s10Foundation4DateV026timeIntervalSinceReferenceB0SdvgZ");
SW double fdn_date_getref_now(void) { return now_ref(); }
/* static let timeIntervalBetween1970AndReferenceDate */
SW double fdn_date_between(void) __asm__("$s10Foundation4DateV035timeIntervalBetween1970AndReferenceB0SdvgZ");
SW double fdn_date_between(void) { return FDN_REF_1970; }
/* var timeIntervalSinceNow */
SW double fdn_date_getsincenow(const double *self CTX) __asm__("$s10Foundation4DateV20timeIntervalSinceNowSdvg");
SW double fdn_date_getsincenow(const double *self CTX) { return *self - now_ref(); }
/* timeIntervalSince(_:) */
SW double fdn_date_since(const double *other, const double *self CTX) __asm__("$s10Foundation4DateV17timeIntervalSinceySdACF");
SW double fdn_date_since(const double *other, const double *self CTX) { return *self - *other; }
/* distance(to:) (Strideable) */
SW double fdn_date_distance(const double *other, const double *self CTX) __asm__("$s10Foundation4DateV8distance2toSdAC_tF");
SW double fdn_date_distance(const double *other, const double *self CTX) { return *other - *self; }
/* addingTimeInterval(_:) e advanced(by:) */
SRET("$s10Foundation4DateV18addingTimeIntervalyACSdF", fdn_date_adding);
static SW USED void fdn_date_adding(double *out, double t, const double *self CTX) { *out = *self + t; }
SRET("$s10Foundation4DateV8advanced2byACSd_tF", fdn_date_advanced);
static SW USED void fdn_date_advanced(double *out, double t, const double *self CTX) { *out = *self + t; }
/* mutating addTimeInterval(_:) */
SW void fdn_date_add(double t, double *self CTX) __asm__("$s10Foundation4DateV15addTimeIntervalyySdF");
SW void fdn_date_add(double t, double *self CTX) { *self += t; }
/* static func + / - (Date, TimeInterval) */
SRET("$s10Foundation4DateV1poiyA2C_SdtFZ", fdn_date_plus);
static SW USED void fdn_date_plus(double *out, const double *d, double t) { *out = *d + t; }
SRET("$s10Foundation4DateV1soiyA2C_SdtFZ", fdn_date_minus);
static SW USED void fdn_date_minus(double *out, const double *d, double t) { *out = *d - t; }
/* static var distantPast / distantFuture */
SRET("$s10Foundation4DateV11distantPastACvgZ", fdn_date_past);
static SW USED void fdn_date_past(double *out) { *out = FDN_DISTANT_PAST; }
SRET("$s10Foundation4DateV13distantFutureACvgZ", fdn_date_future);
static SW USED void fdn_date_future(double *out) { *out = FDN_DISTANT_FUTURE; }
/* static func == < > (Date, Date) */
SW _Bool fdn_date_eq(const double *a, const double *b) __asm__("$s10Foundation4DateV2eeoiySbAC_ACtFZ");
SW _Bool fdn_date_eq(const double *a, const double *b) { return *a == *b; }
SW _Bool fdn_date_lt(const double *a, const double *b) __asm__("$s10Foundation4DateV1loiySbAC_ACtFZ");
SW _Bool fdn_date_lt(const double *a, const double *b) { return *a < *b; }
SW _Bool fdn_date_gt(const double *a, const double *b) __asm__("$s10Foundation4DateV1goiySbAC_ACtFZ");
SW _Bool fdn_date_gt(const double *a, const double *b) { return *a > *b; }
/* compare(_:) -> ComparisonResult (NSOrderedAscending -1, same 0, descending 1) */
SW long fdn_date_compare(const double *other, const double *self CTX) __asm__("$s10Foundation4DateV7compareySo18NSComparisonResultVACF");
SW long fdn_date_compare(const double *other, const double *self CTX) {
    return *self < *other ? -1 : *self > *other ? 1 : 0;
}
/* var description */
SW fdn_str fdn_date_desc(const double *self CTX) __asm__("$s10Foundation4DateV11descriptionSSvg");
SW fdn_str fdn_date_desc(const double *self CTX) {
    char buf[48];
    int n = fdn_date_text(*self, buf, (int)sizeof buf);
    return fdn_str_make(buf, n);
}
/* pontes com NSDate */
SW void *fdn_date_to_ns(const double *self CTX) __asm__("$s10Foundation4DateV19_bridgeToObjectiveCSo6NSDateCyF");
SW void *fdn_date_to_ns(const double *self CTX) { return fdn_ns_date_new(*self + FDN_REF_1970); }
SRET("$s10Foundation4DateV36_unconditionallyBridgeFromObjectiveCyACSo6NSDateCSgFZ", fdn_date_from_ns);
static SW USED void fdn_date_from_ns(double *out, void *ns) {
    /* nil vira Date() como na Apple (o force-unwrap fica do lado do compilador) */
    *out = ns ? fdn_ns_date_t1970(ns) - FDN_REF_1970 : now_ref();
}

/* ---------------------------------------------------------------- UUID */

typedef struct { uint8_t b[16]; } fdn_uuid;

static int hexv(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

int fdn_uuid_parse(const char *s, long n, uint8_t out[16]) {
    int k = 0;
    if (n != 36) return -1;
    for (long i = 0; i < 36; i++) {
        if (i == 8 || i == 13 || i == 18 || i == 23) {
            if (s[i] != '-') return -1;
            continue;
        }
        {
            int hi = hexv((unsigned char)s[i]), lo;
            if (hi < 0 || i + 1 >= 36) return -1;
            lo = hexv((unsigned char)s[++i]);
            if (lo < 0) return -1;
            out[k++] = (uint8_t)(hi << 4 | lo);
        }
    }
    return k == 16 ? 0 : -1;
}

void fdn_uuid_format(const uint8_t u[16], char out[37]) {
    static const char hx[] = "0123456789ABCDEF";
    int k = 0;
    for (int i = 0; i < 16; i++) {
        if (i == 4 || i == 6 || i == 8 || i == 10) out[k++] = '-';
        out[k++] = hx[u[i] >> 4];
        out[k++] = hx[u[i] & 15];
    }
    out[36] = 0;
}

/* init(): versão 4, aleatória */
SRET("$s10Foundation4UUIDVACycfC", fdn_uuid_init);
static SW USED void fdn_uuid_init(fdn_uuid *out) {
    fdn_ns_random(out->b, 16);
    out->b[6] = (uint8_t)((out->b[6] & 0x0F) | 0x40);
    out->b[8] = (uint8_t)((out->b[8] & 0x3F) | 0x80);
}
/* init(uuid: uuid_t): a tupla de 16 UInt8 vem em dois registradores */
SRET("$s10Foundation4UUIDV4uuidACs5UInt8V_A15Ft_tcfC", fdn_uuid_init_tuple);
static SW USED void fdn_uuid_init_tuple(fdn_uuid *out, fdn_uuid u) { *out = u; }
/* var uuid: uuid_t */
SW fdn_uuid fdn_uuid_get_tuple(const fdn_uuid *self CTX) __asm__("$s10Foundation4UUIDV4uuids5UInt8V_A15Ftvg");
SW fdn_uuid fdn_uuid_get_tuple(const fdn_uuid *self CTX) { return *self; }
/* init?(uuidString:): Optional<UUID> tem 17 bytes, tag 0 = some, 1 = none */
SRET("$s10Foundation4UUIDV10uuidStringACSgSSh_tcfC", fdn_uuid_init_str);
static SW USED void fdn_uuid_init_str(uint8_t *out, fdn_str s) {
    char buf[64];
    long n = fdn_str_read(s, buf, (long)sizeof buf);
    if (n == 36 && fdn_uuid_parse(buf, n, out) == 0) fdn_enum_tag_store(out, 0, 1, 16);
    else fdn_enum_tag_store(out, 1, 1, 16);
}
/* var uuidString */
SW fdn_str fdn_uuid_string(const fdn_uuid *self CTX) __asm__("$s10Foundation4UUIDV10uuidStringSSvg");
SW fdn_str fdn_uuid_string(const fdn_uuid *self CTX) {
    char buf[37];
    fdn_uuid_format(self->b, buf);
    return fdn_str_make(buf, 36);
}
/* var description (CustomStringConvertible): o mesmo texto */
SW fdn_str fdn_uuid_desc(const fdn_uuid *self CTX) __asm__("$s10Foundation4UUIDV11descriptionSSvg");
SW fdn_str fdn_uuid_desc(const fdn_uuid *self CTX) { return fdn_uuid_string(self); }
/* static func == */
SW _Bool fdn_uuid_eq(const fdn_uuid *a, const fdn_uuid *b) __asm__("$s10Foundation4UUIDV2eeoiySbAC_ACtFZ");
SW _Bool fdn_uuid_eq(const fdn_uuid *a, const fdn_uuid *b) { return memcmp(a->b, b->b, 16) == 0; }
/* pontes com NSUUID */
SW void *fdn_uuid_to_ns(const fdn_uuid *self CTX) __asm__("$s10Foundation4UUIDV19_bridgeToObjectiveCSo6NSUUIDCyF");
SW void *fdn_uuid_to_ns(const fdn_uuid *self CTX) { return fdn_ns_uuid_new(self->b); }
SRET("$s10Foundation4UUIDV36_unconditionallyBridgeFromObjectiveCyACSo6NSUUIDCSgFZ", fdn_uuid_from_ns);
static SW USED void fdn_uuid_from_ns(fdn_uuid *out, void *ns) {
    if (ns) fdn_ns_uuid_bytes(ns, out->b);
    else fdn_uuid_init(out);
}

/* ---------------------------------------------------------------- NSNotFound */

SW long fdn_nsnotfound(void) __asm__("$s10Foundation10NSNotFoundSivg");
SW long fdn_nsnotfound(void) { return 0x7fffffffffffffffL; }

/* ---------------------------------------------------------------- erros com NSError dentro */

/* var _nsError (CocoaError, POSIXError, URLError): +1 para o chamador */
SW void *fdn_cocoa_nserror(void *const *self CTX) __asm__("$s10Foundation10CocoaErrorV03_nsC0So7NSErrorCvg");
SW void *fdn_cocoa_nserror(void *const *self CTX) { fdn_ns_retain(*self); return *self; }
SW void *fdn_posix_nserror(void *const *self CTX) __asm__("$s10Foundation10POSIXErrorV8_nsErrorSo7NSErrorCvg");
SW void *fdn_posix_nserror(void *const *self CTX) { fdn_ns_retain(*self); return *self; }
SW void *fdn_url_nserror(void *const *self CTX) __asm__("$s10Foundation8URLErrorV8_nsErrorSo7NSErrorCvg");
SW void *fdn_url_nserror(void *const *self CTX) { fdn_ns_retain(*self); return *self; }

/* init(_nsError:): o argumento é consumido (+1), fica no valor */
SRET("$s10Foundation10CocoaErrorV8_nsErrorACSo7NSErrorC_tcfC", fdn_cocoa_init_ns);
static SW USED void fdn_cocoa_init_ns(void **out, void *err) { *out = err; }
SRET("$s10Foundation10POSIXErrorV8_nsErrorACSo7NSErrorC_tcfC", fdn_posix_init_ns);
static SW USED void fdn_posix_init_ns(void **out, void *err) { *out = err; }
SRET("$s10Foundation8URLErrorV8_nsErrorACSo7NSErrorC_tcfC", fdn_url_init_ns);
static SW USED void fdn_url_init_ns(void **out, void *err) { *out = err; }

/* POSIXError.ECANCELED: POSIXErrorCode é enum C de Int32, volta direto */
SW int fdn_posix_ecanceled(void) __asm__("$s10Foundation10POSIXErrorV9ECANCELED6Darwin0B4CodeOvgZ");
SW int fdn_posix_ecanceled(void) { return 89; }

/* Code: RawRepresentable de Int */
SW long fdn_cocoa_code_raw(const long *self CTX) __asm__("$s10Foundation10CocoaErrorV4CodeV8rawValueSivg");
SW long fdn_cocoa_code_raw(const long *self CTX) { return *self; }
SRET("$s10Foundation10CocoaErrorV4CodeV8rawValueAESi_tcfC", fdn_cocoa_code_init);
static SW USED void fdn_cocoa_code_init(long *out, long v) { *out = v; }
SW long fdn_url_code_raw(const long *self CTX) __asm__("$s10Foundation8URLErrorV4CodeV8rawValueSivg");
SW long fdn_url_code_raw(const long *self CTX) { return *self; }
SRET("$s10Foundation8URLErrorV4CodeV8rawValueAESi_tcfC", fdn_url_code_init);
static SW USED void fdn_url_code_init(long *out, long v) { *out = v; }

/* Constantes de código: o getter estático em CocoaError/URLError e em Code devolvem o mesmo Code. */
#define CODE(fn, sym, v) \
    SRET(sym, fn); \
    static SW USED void fn(long *out) { *out = (v); }
#include "fdn_codigos.inc"
#undef CODE

/* ---------------------------------------------------------------- Notification */

/* _unconditionallyBridgeFromObjectiveC(NSNotification?): nil vira Notification(name: "") */
SRET("$s10Foundation12NotificationV36_unconditionallyBridgeFromObjectiveCyACSo14NSNotificationCSgFZ", fdn_note_from_ns);
static SW USED void fdn_note_from_ns(void **out, void *ns) {
    if (ns) { fdn_ns_retain(ns); *out = ns; }
    else *out = fdn_ns_note_new(fdn_ns_string(""), 0, 0);
}
SW void *fdn_note_to_ns(void *const *self CTX) __asm__("$s10Foundation12NotificationV19_bridgeToObjectiveCSo14NSNotificationCyF");
SW void *fdn_note_to_ns(void *const *self CTX) { fdn_ns_retain(*self); return *self; }
/* var name: NSNotification.Name (struct C com um NSString forte; +1) */
SW void *fdn_note_name(void *const *self CTX) __asm__("$s10Foundation12NotificationV4nameSo18NSNotificationNameavg");
SW void *fdn_note_name(void *const *self CTX) {
    void *n = fdn_ns_note_name(*self);
    fdn_ns_retain(n);
    return n;
}
/*
 * var userInfo: [AnyHashable: Any]?. Dicionário Swift em ponte com Cocoa: o próprio NSDictionary
 * com o bit 62 (_objectPointerIsObjCBit do x86_64), +1. nil = 0.
 */
SW uint64_t fdn_note_userinfo(void *const *self CTX) __asm__("$s10Foundation12NotificationV8userInfoSDys11AnyHashableVypGSgvg");
SW uint64_t fdn_note_userinfo(void *const *self CTX) {
    void *d = fdn_ns_note_info(*self);
    if (!d) return 0;
    fdn_ns_retain(d);
    return (uint64_t)(uintptr_t)d | 0x4000000000000000ULL;
}
