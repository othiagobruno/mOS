/*
 * Teste de host de userland/libsystem/fdn_tipos.c e fdn_meta.S (Date, UUID, erros, Notification,
 * LocalizedError). Chama os símbolos com o nome mangled e a convenção swiftcc, como um app chamaria:
 * self em r13 (swift_context), resultado indireto em rax (swift_indirect_result). O lado ObjC é falso.
 *
 *   zig cc -target x86_64-linux-gnu -O2 -I userland/libsystem userland/libsystem/fdn_tipos.c \
 *       userland/libsystem/fdn_meta.S tools/fdn_tipos_test.c -o /tmp/fdn_tipos_test
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>
#include "fdn_tipos.h"

#define SW __attribute__((swiftcall))
#define CTX __attribute__((swift_context))
#define OUT __attribute__((swift_indirect_result))

static int falhas, total;
#define CHECK(c, ...) do { total++; if (!(c)) { falhas++; printf("FALHA %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

/* ------------------------------------------------------------ lado ObjC falso */

typedef struct { int kind; int rc; double t; uint8_t u[16]; char s[64]; void *name, *info; } fake;
enum { K_STR = 1, K_DATE, K_UUID, K_NOTE, K_DICT };
static int retains, releases;
static fake *mk(int kind) { fake *f = calloc(1, sizeof *f); f->kind = kind; f->rc = 1; return f; }
void fdn_ns_retain(void *o) { if (o) { retains++; ((fake *)o)->rc++; } }
void fdn_ns_release(void *o) { if (o) { releases++; ((fake *)o)->rc--; } }
const char *fdn_ns_utf8(void *o) { return o ? ((fake *)o)->s : 0; }
void *fdn_ns_string(const char *s) { fake *f = mk(K_STR); snprintf(f->s, sizeof f->s, "%s", s); return f; }
static double agora = 1700000000.0;
double fdn_ns_now1970(void) { return agora; }
void fdn_ns_random(void *buf, size_t n) { for (size_t i = 0; i < n; i++) ((uint8_t *)buf)[i] = (uint8_t)(0xA5 ^ i * 37); }
void *fdn_ns_date_new(double t) { fake *f = mk(K_DATE); f->t = t; return f; }
double fdn_ns_date_t1970(void *d) { return ((fake *)d)->t; }
void *fdn_ns_uuid_new(const uint8_t b[16]) { fake *f = mk(K_UUID); memcpy(f->u, b, 16); return f; }
void fdn_ns_uuid_bytes(void *u, uint8_t out[16]) { memcpy(out, ((fake *)u)->u, 16); }
void *fdn_ns_note_new(void *name, void *object, void *info) { fake *f = mk(K_NOTE); (void)object; f->name = name; f->info = info; return f; }
void *fdn_ns_note_name(void *n) { return ((fake *)n)->name; }
void *fdn_ns_note_info(void *n) { return ((fake *)n)->info; }

/* NSURL falso: o texto absoluto, o caminho (só arquivo) e as partes lidas com o mesmo fdn_url_split */
typedef struct { int kind, rc; char abs[600], path[600], part[600]; int file; } furl;
enum { K_URL = 9 };
typedef struct { char pre[4096]; char path[4096]; char suf[4096]; } url_partes_t;
void fdn_url_split(const char *abs, url_partes_t *p);
void *fdn_ns_url_file(const char *path) {
    furl *u = calloc(1, sizeof *u);
    u->kind = K_URL; u->rc = 1; u->file = 1;
    snprintf(u->path, sizeof u->path, "%s", path);
    snprintf(u->abs, sizeof u->abs, "file://%s", path);
    return u;
}
void *fdn_ns_url_string(const char *s) {
    furl *u;
    if (!strncmp(s, "file://", 7)) return fdn_ns_url_file(s + 7);
    if (strchr(s, ' ')) return 0;                       /* o URLWithString: recusa espaço */
    u = calloc(1, sizeof *u);
    u->kind = K_URL; u->rc = 1;
    snprintf(u->abs, sizeof u->abs, "%s", s);
    return u;
}
const char *fdn_ns_url_part(void *o, int part) {
    furl *u = o;
    static url_partes_t pt;
    char *p, *e;
    switch (part) {
    case FDN_URL_PATH: if (u->file) return u->path; fdn_url_split(u->abs, &pt); return pt.path;
    case FDN_URL_ABS: case FDN_URL_REL: return u->abs;
    case FDN_URL_SCHEME:
        p = strchr(u->abs, ':');
        if (!p) return 0;
        snprintf(u->part, sizeof u->part, "%.*s", (int)(p - u->abs), u->abs);
        return u->part;
    case FDN_URL_HOST:
        p = strstr(u->abs, "://");
        if (!p || u->file) return 0;
        p += 3;
        e = p + strcspn(p, ":/?#");
        snprintf(u->part, sizeof u->part, "%.*s", (int)(e - p), p);
        return u->part;
    case FDN_URL_QUERY:
        p = strchr(u->abs, '?');
        if (!p) return 0;
        snprintf(u->part, sizeof u->part, "%.*s", (int)strcspn(p + 1, "#"), p + 1);
        return u->part;
    }
    return 0;
}
int fdn_ns_url_isfile(void *o) { return ((furl *)o)->file; }
long fdn_ns_url_port(void *o, int *has) {
    char *p = strstr(((furl *)o)->abs, "://");
    p = p ? strchr(p + 3, ':') : 0;
    *has = p != 0;
    return p ? atol(p + 1) : 0;
}
void *fdn_ns_url_file_op(void *o, int op) { (void)op; return fdn_ns_url_string(((furl *)o)->abs); }
int fdn_ns_isdir(const char *path) { return strcmp(path, "/tmp") == 0 || strcmp(path, "/tmp/") == 0; }
long fdn_ns_hash(void *o) { return (long)strlen(((furl *)o)->abs); }
int fdn_ns_equal(void *a, void *b) { return strcmp(((furl *)a)->abs, ((furl *)b)->abs) == 0; }

/* NSLocale / NSTimeZone falsos: o nome no campo s, o deslocamento em t */
const char *fdn_ns_loc_current_id(void) { return "pt_BR"; }
void *fdn_ns_loc_obj(const char *id) { return fdn_ns_string(id); }
const char *fdn_ns_loc_obj_id(void *l) { return ((fake *)l)->s; }
static fake *tz_new(const char *n, double off) { fake *f = mk(K_STR); snprintf(f->s, sizeof f->s, "%s", n); f->t = off; return f; }
void *fdn_ns_tz_current(void) { return tz_new("America/Sao_Paulo", -10800); }
void *fdn_ns_tz_with_name(const char *n) { return strchr(n, '/') || !strcmp(n, "UTC") ? tz_new(n, !strcmp(n, "UTC") ? 0 : -10800) : 0; }
void *fdn_ns_tz_with_abbr(const char *a) { return !strcmp(a, "GMT") ? tz_new("GMT", 0) : 0; }
void *fdn_ns_tz_with_offset(long s) { char b[32]; snprintf(b, sizeof b, "GMT%+ld", s / 3600); return tz_new(b, (double)s); }
const char *fdn_ns_tz_name(void *t) { return ((fake *)t)->s; }
long fdn_ns_tz_offset(void *t, double t1970) { (void)t1970; return (long)((fake *)t)->t; }
const char *fdn_ns_tz_abbr(void *t, double t1970) { (void)t1970; return ((fake *)t)->t == 0 ? "GMT" : "GMT-3"; }

/* ------------------------------------------------------------ símbolos */

typedef struct { const void *md; uint64_t state; } resp;
#define MA(nome, sym) SW resp nome(uint64_t req) __asm__(sym)
MA(ma_date, "$s10Foundation4DateVMa");
MA(ma_uuid, "$s10Foundation4UUIDVMa");
MA(ma_cocoa, "$s10Foundation10CocoaErrorVMa");
MA(ma_cocoa_code, "$s10Foundation10CocoaErrorV4CodeVMa");
MA(ma_posix, "$s10Foundation10POSIXErrorVMa");
MA(ma_url, "$s10Foundation8URLErrorVMa");
MA(ma_url_code, "$s10Foundation8URLErrorV4CodeVMa");
MA(ma_note, "$s10Foundation12NotificationVMa");
extern const char d_date[] __asm__("$s10Foundation4DateVMn");
extern const char d_uuid[] __asm__("$s10Foundation4UUIDVMn");
extern const char d_cocoa[] __asm__("$s10Foundation10CocoaErrorVMn");
extern const char d_cocoa_code[] __asm__("$s10Foundation10CocoaErrorV4CodeVMn");
extern const char d_posix[] __asm__("$s10Foundation10POSIXErrorVMn");
extern const char d_url[] __asm__("$s10Foundation8URLErrorVMn");
extern const char d_url_code[] __asm__("$s10Foundation8URLErrorV4CodeVMn");
extern const char d_note[] __asm__("$s10Foundation12NotificationVMn");
extern const char d_mod[] __asm__("$s10FoundationMXM");
extern const char d_le[] __asm__("$s10Foundation14LocalizedErrorMp");
extern const char le_base[] __asm__("$s10Foundation14LocalizedErrorPs0C0Tb");
extern const char le_q_desc[] __asm__("$s10Foundation14LocalizedErrorP16errorDescriptionSSSgvgTq");
extern const char le_q_fail[] __asm__("$s10Foundation14LocalizedErrorP13failureReasonSSSgvgTq");
extern const char le_q_rec[] __asm__("$s10Foundation14LocalizedErrorP18recoverySuggestionSSSgvgTq");
extern const char le_q_help[] __asm__("$s10Foundation14LocalizedErrorP10helpAnchorSSSgvgTq");
SW fdn_str le_def_desc(void *self CTX) __asm__("$s10Foundation14LocalizedErrorPAAE16errorDescriptionSSSgvg");
SW fdn_str le_def_help(void *self CTX) __asm__("$s10Foundation14LocalizedErrorPAAE10helpAnchorSSSgvg");
SW fdn_str le_tj_desc(const void *selfmd, void **wt, void *self CTX) __asm__("$s10Foundation14LocalizedErrorP16errorDescriptionSSSgvgTj");
SW fdn_str le_tj_help(const void *selfmd, void **wt, void *self CTX) __asm__("$s10Foundation14LocalizedErrorP10helpAnchorSSSgvgTj");
/* no libSystem o $ss5ErrorMp vem do sys_zerados.c (ou do libswiftCore); aqui, um marcador */
const char sym_error_mp[16] __asm__("$ss5ErrorMp") = "Error";

SW void date_init(OUT double *o) __asm__("$s10Foundation4DateVACycfC");
SW void date_now(OUT double *o) __asm__("$s10Foundation4DateV3nowACvgZ");
SW void date_init1970(OUT double *o, double t) __asm__("$s10Foundation4DateV21timeIntervalSince1970ACSd_tcfC");
SW void date_initref(OUT double *o, double t) __asm__("$s10Foundation4DateV026timeIntervalSinceReferenceB0ACSd_tcfC");
SW void date_initnow(OUT double *o, double t) __asm__("$s10Foundation4DateV20timeIntervalSinceNowACSd_tcfC");
SW void date_initsince(OUT double *o, double t, const double *since) __asm__("$s10Foundation4DateV12timeInterval5sinceACSd_ACtcfC");
SW double date_get1970(const double *s CTX) __asm__("$s10Foundation4DateV21timeIntervalSince1970Sdvg");
SW double date_getref(const double *s CTX) __asm__("$s10Foundation4DateV026timeIntervalSinceReferenceB0Sdvg");
SW double date_getref_now(void) __asm__("$s10Foundation4DateV026timeIntervalSinceReferenceB0SdvgZ");
SW double date_between(void) __asm__("$s10Foundation4DateV035timeIntervalBetween1970AndReferenceB0SdvgZ");
SW double date_sincenow(const double *s CTX) __asm__("$s10Foundation4DateV20timeIntervalSinceNowSdvg");
SW double date_since(const double *o, const double *s CTX) __asm__("$s10Foundation4DateV17timeIntervalSinceySdACF");
SW double date_distance(const double *o, const double *s CTX) __asm__("$s10Foundation4DateV8distance2toSdAC_tF");
SW void date_adding(OUT double *o, double t, const double *s CTX) __asm__("$s10Foundation4DateV18addingTimeIntervalyACSdF");
SW void date_advanced(OUT double *o, double t, const double *s CTX) __asm__("$s10Foundation4DateV8advanced2byACSd_tF");
SW void date_add(double t, double *s CTX) __asm__("$s10Foundation4DateV15addTimeIntervalyySdF");
SW void date_plus(OUT double *o, const double *d, double t) __asm__("$s10Foundation4DateV1poiyA2C_SdtFZ");
SW void date_minus(OUT double *o, const double *d, double t) __asm__("$s10Foundation4DateV1soiyA2C_SdtFZ");
SW void date_past(OUT double *o) __asm__("$s10Foundation4DateV11distantPastACvgZ");
SW void date_future(OUT double *o) __asm__("$s10Foundation4DateV13distantFutureACvgZ");
SW _Bool date_eq(const double *a, const double *b) __asm__("$s10Foundation4DateV2eeoiySbAC_ACtFZ");
SW _Bool date_lt(const double *a, const double *b) __asm__("$s10Foundation4DateV1loiySbAC_ACtFZ");
SW _Bool date_gt(const double *a, const double *b) __asm__("$s10Foundation4DateV1goiySbAC_ACtFZ");
SW long date_compare(const double *o, const double *s CTX) __asm__("$s10Foundation4DateV7compareySo18NSComparisonResultVACF");
SW fdn_str date_desc(const double *s CTX) __asm__("$s10Foundation4DateV11descriptionSSvg");
SW void *date_to_ns(const double *s CTX) __asm__("$s10Foundation4DateV19_bridgeToObjectiveCSo6NSDateCyF");
SW void date_from_ns(OUT double *o, void *ns) __asm__("$s10Foundation4DateV36_unconditionallyBridgeFromObjectiveCyACSo6NSDateCSgFZ");

typedef struct { uint8_t b[16]; } U;
typedef struct { uint64_t lo, hi; } U2;
SW void uuid_init(OUT U *o) __asm__("$s10Foundation4UUIDVACycfC");
SW void uuid_init_tuple(OUT U *o, uint64_t lo, uint64_t hi) __asm__("$s10Foundation4UUIDV4uuidACs5UInt8V_A15Ft_tcfC");
SW U2 uuid_get_tuple(const U *s CTX) __asm__("$s10Foundation4UUIDV4uuids5UInt8V_A15Ftvg");
SW void uuid_init_str(OUT uint8_t *o, uint64_t w0, uint64_t w1) __asm__("$s10Foundation4UUIDV10uuidStringACSgSSh_tcfC");
SW fdn_str uuid_string(const U *s CTX) __asm__("$s10Foundation4UUIDV10uuidStringSSvg");
SW fdn_str uuid_desc(const U *s CTX) __asm__("$s10Foundation4UUIDV11descriptionSSvg");
SW _Bool uuid_eq(const U *a, const U *b) __asm__("$s10Foundation4UUIDV2eeoiySbAC_ACtFZ");
SW void *uuid_to_ns(const U *s CTX) __asm__("$s10Foundation4UUIDV19_bridgeToObjectiveCSo6NSUUIDCyF");
SW void uuid_from_ns(OUT U *o, void *ns) __asm__("$s10Foundation4UUIDV36_unconditionallyBridgeFromObjectiveCyACSo6NSUUIDCSgFZ");

SW long nsnotfound(void) __asm__("$s10Foundation10NSNotFoundSivg");
SW void *cocoa_nserror(void *const *s CTX) __asm__("$s10Foundation10CocoaErrorV03_nsC0So7NSErrorCvg");
SW void *posix_nserror(void *const *s CTX) __asm__("$s10Foundation10POSIXErrorV8_nsErrorSo7NSErrorCvg");
SW void *url_nserror(void *const *s CTX) __asm__("$s10Foundation8URLErrorV8_nsErrorSo7NSErrorCvg");
SW void cocoa_init_ns(OUT void **o, void *e) __asm__("$s10Foundation10CocoaErrorV8_nsErrorACSo7NSErrorC_tcfC");
SW int posix_ecanceled(void) __asm__("$s10Foundation10POSIXErrorV9ECANCELED6Darwin0B4CodeOvgZ");
SW long cocoa_code_raw(const long *s CTX) __asm__("$s10Foundation10CocoaErrorV4CodeV8rawValueSivg");
SW void cocoa_code_init(OUT long *o, long v) __asm__("$s10Foundation10CocoaErrorV4CodeV8rawValueAESi_tcfC");
SW long url_code_raw(const long *s CTX) __asm__("$s10Foundation8URLErrorV4CodeV8rawValueSivg");
SW void c_nosuch(OUT long *o) __asm__("$s10Foundation10CocoaErrorV14fileNoSuchFileAC4CodeVvgZ");
SW void c_code_nosuch(OUT long *o) __asm__("$s10Foundation10CocoaErrorV4CodeV14fileNoSuchFileAEvgZ");
SW void c_cancel(OUT long *o) __asm__("$s10Foundation10CocoaErrorV4CodeV13userCancelledAEvgZ");
SW void c_readunk(OUT long *o) __asm__("$s10Foundation10CocoaErrorV15fileReadUnknownAC4CodeVvgZ");
SW void u_timeout(OUT long *o) __asm__("$s10Foundation8URLErrorV8timedOutAC4CodeVvgZ");
SW void u_code_cancel(OUT long *o) __asm__("$s10Foundation8URLErrorV4CodeV9cancelledAEvgZ");
SW void u_code_offline(OUT long *o) __asm__("$s10Foundation8URLErrorV4CodeV22notConnectedToInternetAEvgZ");
SW void u_code_badresp(OUT long *o) __asm__("$s10Foundation8URLErrorV4CodeV17badServerResponseAEvgZ");

SW void note_from_ns(OUT void **o, void *ns) __asm__("$s10Foundation12NotificationV36_unconditionallyBridgeFromObjectiveCyACSo14NSNotificationCSgFZ");
SW void *note_to_ns(void *const *s CTX) __asm__("$s10Foundation12NotificationV19_bridgeToObjectiveCSo14NSNotificationCyF");
SW void *note_name(void *const *s CTX) __asm__("$s10Foundation12NotificationV4nameSo18NSNotificationNameavg");
SW uint64_t note_info(void *const *s CTX) __asm__("$s10Foundation12NotificationV8userInfoSDys11AnyHashableVypGSgvg");

/* ------------------------------------------------------------ ajudas */

static const void *rel(const char *campo) {
    int32_t off;
    memcpy(&off, campo, 4);
    return campo + off;
}
static uint32_t u32(const char *p) { uint32_t v; memcpy(&v, p, 4); return v; }

static void str_eq(fdn_str s, const char *want, const char *onde) {
    char buf[256];
    long n = fdn_str_read(s, buf, sizeof buf);
    CHECK(n == (long)strlen(want) && strcmp(buf, want) == 0, "%s: \"%s\" (%ld) != \"%s\"", onde, buf, n, want);
}

static void check_struct(const char *desc, resp (SW *ma)(uint64_t), const fdn_vwt *vwt, const char *nome, const char *parent) {
    resp r = ma(0);
    const uintptr_t *md = r.md;
    CHECK(r.state == 0, "%s: estado %llu", nome, (unsigned long long)r.state);
    CHECK(md[0] == 0x200, "%s: kind %lx", nome, (unsigned long)md[0]);
    CHECK((const char *)md[1] == desc, "%s: descritor", nome);
    CHECK((const fdn_vwt *)md[-1] == vwt, "%s: vwt", nome);
    CHECK(u32(desc) == 0x51, "%s: flags %x", nome, u32(desc));
    CHECK(rel(desc + 4) == parent, "%s: parent", nome);
    CHECK(strcmp(rel(desc + 8), nome) == 0, "%s: nome %s", nome, (const char *)rel(desc + 8));
    CHECK(rel(desc + 12) == (const void *)ma, "%s: acessor", nome);
    CHECK(u32(desc + 16) == 0 && u32(desc + 20) == 0 && u32(desc + 24) == 2, "%s: campos", nome);
    CHECK(vwt->size == vwt->stride && (vwt->flags & 0xff) == 7 && vwt->extra_inhabitants == 0, "%s: layout", nome);
}

int main(void) {
    /* ---- descritores e metadata */
    CHECK(u32(d_mod) == 0 && strcmp(rel(d_mod + 8), "Foundation") == 0, "módulo");
    check_struct(d_date, ma_date, &fdn_vwt_pod8, "Date", d_mod);
    check_struct(d_uuid, ma_uuid, &fdn_vwt_pod16, "UUID", d_mod);
    check_struct(d_cocoa, ma_cocoa, &fdn_vwt_obj, "CocoaError", d_mod);
    check_struct(d_cocoa_code, ma_cocoa_code, &fdn_vwt_pod8, "Code", d_cocoa);
    check_struct(d_posix, ma_posix, &fdn_vwt_obj, "POSIXError", d_mod);
    check_struct(d_url, ma_url, &fdn_vwt_obj, "URLError", d_mod);
    check_struct(d_url_code, ma_url_code, &fdn_vwt_pod8, "Code", d_url);
    check_struct(d_note, ma_note, &fdn_vwt_obj, "Notification", d_mod);
    CHECK(fdn_vwt_pod16.size == 16 && fdn_vwt_obj.flags == (7 | 0x10000), "tamanhos");

    /* ---- LocalizedError */
    CHECK(u32(d_le) == 0x30043, "LE flags %x", u32(d_le));
    CHECK(rel(d_le + 4) == d_mod && strcmp(rel(d_le + 8), "LocalizedError") == 0, "LE nome");
    CHECK(u32(d_le + 12) == 1 && u32(d_le + 16) == 5 && u32(d_le + 20) == 0, "LE contagens");
    CHECK(u32(d_le + 24) == 0x80 && strcmp(rel(d_le + 28), "x") == 0, "LE assinatura");
    {
        int32_t off;
        memcpy(&off, d_le + 32, 4);
        CHECK(off & 1, "LE ponteiro indireto");
        CHECK(*(const void *const *)(d_le + 32 + (off & ~3)) == (const void *)sym_error_mp, "LE Error");
    }
    CHECK(le_base == d_le + 36 && le_q_desc == d_le + 44 && le_q_fail == d_le + 52 && le_q_rec == d_le + 60 && le_q_help == d_le + 68, "LE Tq/Tb");
    CHECK(u32(le_base) == 0 && u32(le_q_desc) == 0x13 && u32(le_q_help) == 0x13, "LE kinds");
    CHECK(rel(le_q_desc + 4) == (const void *)le_def_desc && rel(le_q_help + 4) == (const void *)le_def_help, "LE padrões");
    {
        fdn_str s = le_def_desc(0);
        CHECK(s.w0 == 0 && s.w1 == 0, "padrão = nil");
        /* tabela de testemunhas do app: [conf, Error, desc, fail, rec, help] */
        void *wt[6] = { 0, 0, (void *)le_def_desc, 0, 0, (void *)le_def_help };
        s = le_tj_desc(0, wt, 0);
        CHECK(s.w0 == 0 && s.w1 == 0, "Tj desc");
        s = le_tj_help(0, wt, 0);
        CHECK(s.w0 == 0 && s.w1 == 0, "Tj help");
    }

    /* ---- enum tag (Optional e enums maiores sem inabitante extra) */
    {
        uint8_t v[24];
        for (unsigned empty = 1; empty <= 300; empty += 299)
            for (unsigned w = 0; w <= empty; w++) {
                memset(v, 0x5a, sizeof v);
                fdn_vwt_pod16.store_enum_tag_single_payload(v, w, empty, ma_uuid(0).md);
                CHECK(fdn_vwt_pod16.get_enum_tag_single_payload(v, empty, ma_uuid(0).md) == w, "tag %u/%u", w, empty);
            }
        memset(v, 0, sizeof v);
        fdn_enum_tag_store(v, 1, 1, 8);
        CHECK(v[8] == 1 && fdn_enum_tag_get(v, 1, 8) == 1, "Optional nil: byte 8");
        CHECK(fdn_enum_tag_get(v, 0, 8) == 0, "sem casos vazios");
        /* tamanho < 4: a tag guarda os bits altos do índice */
        memset(v, 0, sizeof v);
        fdn_enum_tag_store(v, 300, 400, 1);
        CHECK(fdn_enum_tag_get(v, 400, 1) == 300, "payload de 1 byte");
    }

    /* ---- String */
    {
        fdn_str s = fdn_str_make("abc", 3);
        CHECK(s.w1 >> 56 == 0xE3, "pequena ASCII");
        str_eq(s, "abc", "pequena");
        s = fdn_str_make("12345678901234567890", -1);
        CHECK(s.w0 == (0xD000000000000000ULL | 20), "grande: countAndFlags %llx", (unsigned long long)s.w0);
        CHECK(s.w1 >> 60 == 8, "grande: imortal");
        CHECK(strcmp((const char *)(uintptr_t)((s.w1 & 0x0FFFFFFFFFFFFFFFULL) + 32), "12345678901234567890") == 0, "grande: bytes + 32");
        str_eq(s, "12345678901234567890", "grande");
        s = fdn_str_make("ação", -1);
        CHECK(s.w1 >> 56 == (0xA0 | 6), "pequena não ASCII");
        s = fdn_str_make("ação ação ação ação", -1);
        CHECK(s.w0 >> 60 == 1, "grande não ASCII");
        str_eq(s, "ação ação ação ação", "grande não ASCII");
        {
            fake *ns = fdn_ns_string("ponte de NSString");
            fdn_str b = { 17, 0x4000000000000000ULL | (uint64_t)(uintptr_t)ns };
            str_eq(b, "ponte de NSString", "ponte");
        }
    }

    /* ---- Date */
    {
        double d, e, f;
        date_init1970(&d, 1e9);
        CHECK(date_get1970(&d) == 1e9, "1970");
        CHECK(date_getref(&d) == 1e9 - 978307200.0, "ref");
        str_eq(date_desc(&d), "2001-09-09 01:46:40 +0000", "description");
        date_initref(&e, 0);
        str_eq(date_desc(&e), "2001-01-01 00:00:00 +0000", "ref 0");
        date_past(&f);
        CHECK(date_getref(&f) == -63114076800.0, "distantPast");
        str_eq(date_desc(&f), "0000-12-30 00:00:00 +0000", "distantPast description");
        date_future(&f);
        CHECK(date_getref(&f) == 63113904000.0, "distantFuture");
        str_eq(date_desc(&f), "4001-01-01 00:00:00 +0000", "distantFuture description");
        date_initref(&f, 1e12);
        str_eq(date_desc(&f), "<description unavailable>", "fora do intervalo");
        date_init1970(&f, -1.5);
        str_eq(date_desc(&f), "1969-12-31 23:59:59 +0000", "antes de 1970 (trunca)");
        CHECK(date_since(&e, &d) == 1e9 - 978307200.0, "timeIntervalSince");
        CHECK(date_distance(&e, &d) == -(1e9 - 978307200.0), "distance");
        CHECK(date_lt(&e, &d) && !date_lt(&d, &e) && date_gt(&d, &e) && !date_eq(&d, &e) && date_eq(&d, &d), "comparações");
        CHECK(date_compare(&e, &d) == 1 && date_compare(&d, &e) == -1 && date_compare(&d, &d) == 0, "compare");
        date_adding(&f, 10, &e);
        CHECK(date_getref(&f) == 10, "adding");
        date_advanced(&f, -5, &e);
        CHECK(date_getref(&f) == -5, "advanced");
        date_add(2.5, &f);
        CHECK(date_getref(&f) == -2.5, "addTimeInterval");
        date_plus(&f, &e, 7);
        CHECK(date_getref(&f) == 7, "+");
        date_minus(&f, &e, 7);
        CHECK(date_getref(&f) == -7, "-");
        date_init(&f);
        CHECK(date_get1970(&f) == agora, "init()");
        date_now(&f);
        CHECK(date_get1970(&f) == agora, "now");
        CHECK(date_getref_now() == agora - 978307200.0 && date_between() == 978307200.0, "estáticos");
        date_initnow(&f, 60);
        CHECK(date_get1970(&f) == agora + 60 && date_sincenow(&f) == 60, "sinceNow");
        date_initsince(&f, 30, &e);
        CHECK(date_getref(&f) == 30, "timeInterval:since:");
        {
            fake *ns = date_to_ns(&d);
            CHECK(ns && ns->kind == K_DATE && ns->t == 1e9 && ns->rc == 1, "ponte para NSDate");
            date_from_ns(&f, ns);
            CHECK(date_get1970(&f) == 1e9, "ponte de NSDate");
            date_from_ns(&f, 0);
            CHECK(date_get1970(&f) == agora, "ponte de nil");
        }
        /* a VWT copia pelo tamanho da metadata */
        fdn_vwt_pod8.init_copy(&f, &d, ma_date(0).md);
        CHECK(f == d, "vwt copy");
    }

    /* ---- UUID */
    {
        U a, b;
        uint8_t opt[17];
        char txt[37];
        uuid_init(&a);
        CHECK((a.b[6] >> 4) == 4 && (a.b[8] & 0xC0) == 0x80, "UUID v4");
        fdn_str s = fdn_str_make("E621E1F8-C36C-495A-93FC-0C247A3E6E5F", 36);
        uuid_init_str(opt, s.w0, s.w1);
        CHECK(opt[16] == 0 && fdn_vwt_pod16.get_enum_tag_single_payload(opt, 1, ma_uuid(0).md) == 0, "uuidString válida");
        CHECK(opt[0] == 0xE6 && opt[15] == 0x5F, "bytes");
        memcpy(&b, opt, 16);
        str_eq(uuid_string(&b), "E621E1F8-C36C-495A-93FC-0C247A3E6E5F", "uuidString");
        str_eq(uuid_desc(&b), "E621E1F8-C36C-495A-93FC-0C247A3E6E5F", "description");
        s = fdn_str_make("e621e1f8-c36c-495a-93fc-0c247a3e6e5f", 36);
        uuid_init_str(opt, s.w0, s.w1);
        CHECK(opt[16] == 0 && memcmp(opt, b.b, 16) == 0, "minúsculas");
        s = fdn_str_make("E621E1F8C36C-495A-93FC-0C247A3E6E5F-", 36);
        uuid_init_str(opt, s.w0, s.w1);
        CHECK(opt[16] == 1 && fdn_vwt_pod16.get_enum_tag_single_payload(opt, 1, ma_uuid(0).md) == 1, "inválida = nil");
        s = fdn_str_make("nada", 4);
        uuid_init_str(opt, s.w0, s.w1);
        CHECK(opt[16] == 1, "curta = nil");
        CHECK(uuid_eq(&b, &b) && !uuid_eq(&a, &b), "==");
        U2 t = uuid_get_tuple(&b);
        uuid_init_tuple(&a, t.lo, t.hi);
        CHECK(memcmp(&a, &b, 16) == 0, "tupla ida e volta");
        fake *ns = uuid_to_ns(&b);
        CHECK(ns && ns->kind == K_UUID && memcmp(ns->u, b.b, 16) == 0, "ponte para NSUUID");
        uuid_from_ns(&a, ns);
        CHECK(memcmp(&a, &b, 16) == 0, "ponte de NSUUID");
        fdn_uuid_format(b.b, txt);
        CHECK(fdn_uuid_parse(txt, 36, a.b) == 0 && memcmp(&a, &b, 16) == 0, "parse/format");
    }

    /* ---- erros */
    {
        long c;
        fake *err = mk(K_STR);
        void *box = err;
        int r0 = retains;
        CHECK(nsnotfound() == 0x7fffffffffffffffL, "NSNotFound");
        CHECK(cocoa_nserror(&box) == err && retains == r0 + 1, "_nsError retém");
        CHECK(posix_nserror(&box) == err && url_nserror(&box) == err, "_nsError");
        cocoa_init_ns(&box, err);
        CHECK(box == err, "init(_nsError:)");
        CHECK(posix_ecanceled() == 89, "ECANCELED");
        c_nosuch(&c); CHECK(c == 4, "fileNoSuchFile");
        c_code_nosuch(&c); CHECK(c == 4 && cocoa_code_raw(&c) == 4, "Code.fileNoSuchFile");
        c_cancel(&c); CHECK(c == 3072, "userCancelled");
        c_readunk(&c); CHECK(c == 256, "fileReadUnknown");
        cocoa_code_init(&c, 640); CHECK(cocoa_code_raw(&c) == 640, "Code(rawValue:)");
        u_timeout(&c); CHECK(c == -1001 && url_code_raw(&c) == -1001, "timedOut");
        u_code_cancel(&c); CHECK(c == -999, "cancelled");
        u_code_offline(&c); CHECK(c == -1009, "notConnectedToInternet");
        u_code_badresp(&c); CHECK(c == -1011, "badServerResponse");
        /* VWT de referência: cópia retém, destruir solta, mover não mexe */
        {
            void *x = err, *y = 0, *z = 0;
            int rr = retains, rl = releases;
            const void *md = ma_cocoa(0).md;
            fdn_vwt_obj.init_copy(&y, &x, md);
            CHECK(y == err && retains == rr + 1, "obj copy");
            fdn_vwt_obj.init_take(&z, &y, md);
            CHECK(z == err && retains == rr + 1 && releases == rl, "obj take");
            fdn_vwt_obj.destroy(&z, md);
            CHECK(releases == rl + 1, "obj destroy");
            y = mk(K_STR);
            fdn_vwt_obj.assign_copy(&y, &x, md);
            CHECK(y == err && releases == rl + 2 && retains == rr + 2, "obj assign");
        }
    }

    /* ---- Notification */
    {
        void *n = 0;
        fake *nm = fdn_ns_string("MinhaNota");
        fake *dict = mk(K_DICT);
        fake *ns = fdn_ns_note_new(nm, 0, dict);
        note_from_ns(&n, ns);
        CHECK(n == ns && ns->rc == 2, "ponte de NSNotification retém");
        CHECK(note_to_ns(&n) == ns && ns->rc == 3, "ponte para NSNotification");
        CHECK(note_name(&n) == nm && nm->rc == 2, "name");
        CHECK(note_info(&n) == (0x4000000000000000ULL | (uint64_t)(uintptr_t)dict) && dict->rc == 2, "userInfo em ponte");
        ns->info = 0;
        CHECK(note_info(&n) == 0, "userInfo nil");
        note_from_ns(&n, 0);
        CHECK(n && ((fake *)n)->kind == K_NOTE && strcmp(((fake *)((fake *)n)->name)->s, "") == 0, "nil vira nome vazio");
    }

    /* ---- URL */
    {
        extern const char d_urlt[] __asm__("$s10Foundation3URLVMn");
        extern const char d_hint[] __asm__("$s10Foundation3URLV13DirectoryHintOMn");
        extern const uint32_t wc_is __asm__("$s10Foundation3URLV13DirectoryHintO02isC0yA2EmFWC");
        extern const uint32_t wc_infer __asm__("$s10Foundation3URLV13DirectoryHintO13inferFromPathyA2EmFWC");
        SW resp ma_urlt(uint64_t) __asm__("$s10Foundation3URLVMa");
        SW resp ma_hint(uint64_t) __asm__("$s10Foundation3URLV13DirectoryHintOMa");
        SW void url_init_string(OUT void **o, uint64_t w0, uint64_t w1) __asm__("$s10Foundation3URLV6stringACSgSSh_tcfC");
        SW void url_init_file(OUT void **o, uint64_t w0, uint64_t w1) __asm__("$s10Foundation3URLV15fileURLWithPathACSSh_tcfC");
        SW void url_init_file_dir(OUT void **o, uint64_t w0, uint64_t w1, _Bool d) __asm__("$s10Foundation3URLV15fileURLWithPath11isDirectoryACSSh_SbtcfC");
        SW void url_init_filepath(OUT void **o, uint64_t w0, uint64_t w1, const uint8_t *hint, void *const *base) __asm__("$s10Foundation3URLV8filePath13directoryHint10relativeToACSS_AC09DirectoryF0OACSgtcfC");
        SW fdn_str url_path(void *const *s CTX) __asm__("$s10Foundation3URLV4pathSSvg");
        SW fdn_str url_abs(void *const *s CTX) __asm__("$s10Foundation3URLV14absoluteStringSSvg");
        SW fdn_str url_last(void *const *s CTX) __asm__("$s10Foundation3URLV17lastPathComponentSSvg");
        SW fdn_str url_ext(void *const *s CTX) __asm__("$s10Foundation3URLV13pathExtensionSSvg");
        SW fdn_str url_scheme(void *const *s CTX) __asm__("$s10Foundation3URLV6schemeSSSgvg");
        SW fdn_str url_host(void *const *s CTX) __asm__("$s10Foundation3URLV4hostSSSgvg");
        SW fdn_str url_query(void *const *s CTX) __asm__("$s10Foundation3URLV5querySSSgvg");
        SW fdn_str url_user(void *const *s CTX) __asm__("$s10Foundation3URLV4userSSSgvg");
        typedef struct { long v; _Bool none; } oi;
        SW oi url_port(void *const *s CTX) __asm__("$s10Foundation3URLV4portSiSgvg");
        SW _Bool url_isfile(void *const *s CTX) __asm__("$s10Foundation3URLV06isFileB0Sbvg");
        SW _Bool url_hasdir(void *const *s CTX) __asm__("$s10Foundation3URLV16hasDirectoryPathSbvg");
        SW _Bool url_eq(void *const *a, void *const *b) __asm__("$s10Foundation3URLV2eeoiySbAC_ACtFZ");
        SW void url_append(OUT void **o, uint64_t w0, uint64_t w1, void *const *s CTX) __asm__("$s10Foundation3URLV22appendingPathComponentyACSSF");
        SW void url_append_dir(OUT void **o, uint64_t w0, uint64_t w1, _Bool d, void *const *s CTX) __asm__("$s10Foundation3URLV22appendingPathComponent_11isDirectoryACSS_SbtF");
        SW void url_append_ext(OUT void **o, uint64_t w0, uint64_t w1, void *const *s CTX) __asm__("$s10Foundation3URLV22appendingPathExtensionyACSSF");
        SW void url_del_last(OUT void **o, void *const *s CTX) __asm__("$s10Foundation3URLV25deletingLastPathComponentACyF");
        SW void url_del_ext(OUT void **o, void *const *s CTX) __asm__("$s10Foundation3URLV21deletingPathExtensionACyF");
        SW void url_std_file(OUT void **o, void *const *s CTX) __asm__("$s10Foundation3URLV016standardizedFileB0ACvg");
        SW void url_m_append(uint64_t w0, uint64_t w1, void **s CTX) __asm__("$s10Foundation3URLV19appendPathComponentyySSF");
        SW void url_m_del_last(void **s CTX) __asm__("$s10Foundation3URLV23deleteLastPathComponentyyF");
        SW _Bool url_start(void *const *s CTX) __asm__("$s10Foundation3URLV36startAccessingSecurityScopedResourceSbyF");
        void *u = 0, *v = 0, *w = 0;
        uint8_t opt[9];
        fdn_str s, n;
        resp r = ma_urlt(0);
        CHECK(((const uintptr_t *)r.md)[1] == (uintptr_t)d_urlt && ((const fdn_vwt *const *)r.md)[-1] == &fdn_vwt_obj, "URL metadata");
        CHECK(strcmp(rel(d_urlt + 8), "URL") == 0, "URL nome");
        r = ma_hint(0);
        CHECK(((const uintptr_t *)r.md)[0] == 0x201 && u32(d_hint) == 0x52 && u32(d_hint + 24) == 4, "DirectoryHint enum");
        CHECK(rel(d_hint + 4) == d_urlt && strcmp(rel(d_hint + 8), "DirectoryHint") == 0, "DirectoryHint pai");
        {
            const fdn_evwt *ev = ((const fdn_evwt *const *)r.md)[-1];
            uint8_t h = 9;
            CHECK(ev->base.size == 1 && (ev->base.flags & 0x200000), "evwt");
            ev->destructive_inject_enum_tag(&h, wc_infer, r.md);
            CHECK(h == 3 && ev->get_enum_tag(&h, r.md) == wc_infer && wc_is == 0, "inject/get");
        }

        s = fdn_str_make("https://user@exemplo.com:8443/a/b/arquivo.tar.gz?x=1#f", -1);
        url_init_string((void **)opt, s.w0, s.w1);
        CHECK(opt[8] == 0, "URL(string:) some");
        memcpy(&u, opt, 8);
        str_eq(url_last(&u), "arquivo.tar.gz", "last");
        str_eq(url_ext(&u), "gz", "ext");
        str_eq(url_scheme(&u), "https", "scheme");
        str_eq(url_host(&u), "user@exemplo.com", "host (falso)");
        str_eq(url_query(&u), "x=1", "query");
        n = url_user(&u);
        CHECK(n.w0 == 0 && n.w1 == 0, "user nil");
        CHECK(url_port(&u).v == 8443 && !url_port(&u).none, "port");
        CHECK(!url_isfile(&u) && !url_hasdir(&u), "não é arquivo");
        url_del_ext(&v, &u);
        str_eq(url_abs(&v), "https://user@exemplo.com:8443/a/b/arquivo.tar?x=1#f", "deletingPathExtension");
        url_del_last(&v, &u);
        str_eq(url_abs(&v), "https://user@exemplo.com:8443/a/b/?x=1#f", "deletingLastPathComponent");
        s = fdn_str_make("c d", 3);
        url_append(&v, s.w0, s.w1, &u);
        str_eq(url_abs(&v), "https://user@exemplo.com:8443/a/b/arquivo.tar.gz/c%20d?x=1#f", "appending com percent-encoding");
        s = fdn_str_make("", 0);
        url_init_string((void **)opt, s.w0, s.w1);
        CHECK(opt[8] == 1, "URL(string: \"\") = nil");
        s = fdn_str_make("http://exemplo.com", -1);
        url_init_string((void **)opt, s.w0, s.w1);
        memcpy(&w, opt, 8);
        s = fdn_str_make("x", 1);
        url_append(&v, s.w0, s.w1, &w);
        str_eq(url_abs(&v), "http://exemplo.com/x", "append em caminho vazio");
        url_del_last(&v, &w);
        CHECK(v == w, "deletingLast em caminho vazio devolve o mesmo");
        n = url_port(&w).none ? fdn_str_make("nil", 3) : fdn_str_make("?", 1);
        str_eq(n, "nil", "port nil");

        s = fdn_str_make("/Users/eu/Documentos/./notas/../texto.md", -1);
        url_init_file(&u, s.w0, s.w1);
        CHECK(url_isfile(&u), "fileURLWithPath");
        url_std_file(&v, &u);
        str_eq(url_path(&v), "/Users/eu/Documentos/texto.md", "standardizedFileURL");
        str_eq(url_last(&v), "texto.md", "last arquivo");
        s = fdn_str_make("bak", 3);
        url_append_ext(&w, s.w0, s.w1, &v);
        str_eq(url_path(&w), "/Users/eu/Documentos/texto.md.bak", "appendingPathExtension");
        url_m_del_last(&w);
        str_eq(url_path(&w), "/Users/eu/Documentos/", "deleteLastPathComponent");
        CHECK(url_hasdir(&w), "hasDirectoryPath");
        s = fdn_str_make("sub", 3);
        url_m_append(s.w0, s.w1, &w);
        str_eq(url_path(&w), "/Users/eu/Documentos/sub", "appendPathComponent");
        url_append_dir(&v, s.w0, s.w1, 1, &w);
        str_eq(url_path(&v), "/Users/eu/Documentos/sub/sub/", "appending isDirectory");
        s = fdn_str_make("/", 1);
        url_init_file(&u, s.w0, s.w1);
        s = fdn_str_make("tmp", 3);
        url_append(&v, s.w0, s.w1, &u);
        str_eq(url_path(&v), "/tmp/", "append de diretório existente ganha barra");
        url_del_last(&w, &u);
        str_eq(url_path(&w), "/../", "deletingLast de /");
        s = fdn_str_make("/tmp", 4);
        url_init_file_dir(&u, s.w0, s.w1, 1);
        str_eq(url_path(&u), "/tmp/", "isDirectory: true");
        CHECK(url_eq(&u, &v) && !url_eq(&u, &w), "==");
        {
            uint8_t hint = 3, base[9];
            void *b = fdn_ns_url_file("/base/dir/");
            memcpy(base, &b, 8);
            base[8] = 0;
            s = fdn_str_make("rel/arq.txt", -1);
            url_init_filepath(&v, s.w0, s.w1, &hint, (void *const *)base);
            str_eq(url_path(&v), "/base/dir/rel/arq.txt", "filePath relativeTo");
            hint = 0;
            url_init_filepath(&v, s.w0, s.w1, &hint, 0);
            str_eq(url_path(&v), "rel/arq.txt/", "filePath isDirectory");
        }
        CHECK(!url_start(&u), "sem escopo de segurança");
        {
            char out[64];
            extern int fdn_path_op(const char *p, int op, const char *arg, int dir, char *out);
            CHECK(fdn_path_op("/a/.hidden", 3, "", 0, out) == -1, "sem extensão em arquivo oculto");
            CHECK(fdn_path_op("a/../../b", 4, "", 0, out) == 0 && strcmp(out, "../b") == 0, "standardize relativo: %s", out);
        }
    }

    /* ---- Locale e TimeZone */
    {
        SW void loc_current(OUT void **o) __asm__("$s10Foundation6LocaleV7currentACvgZ");
        SW void loc_init(OUT void **o, uint64_t w0, uint64_t w1) __asm__("$s10Foundation6LocaleV10identifierACSS_tcfC");
        SW fdn_str loc_id(void *const *s CTX) __asm__("$s10Foundation6LocaleV10identifierSSvg");
        SW fdn_str loc_lang(void *const *s CTX) __asm__("$s10Foundation6LocaleV12languageCodeSSSgvg");
        SW fdn_str loc_region(void *const *s CTX) __asm__("$s10Foundation6LocaleV10regionCodeSSSgvg");
        SW fdn_str loc_cur(void *const *s CTX) __asm__("$s10Foundation6LocaleV12currencyCodeSSSgvg");
        SW fdn_str loc_dec(void *const *s CTX) __asm__("$s10Foundation6LocaleV16decimalSeparatorSSSgvg");
        SW _Bool loc_metric(void *const *s CTX) __asm__("$s10Foundation6LocaleV16usesMetricSystemSbvg");
        SW _Bool loc_eq(void *const *a, void *const *b) __asm__("$s10Foundation6LocaleV2eeoiySbAC_ACtFZ");
        SW void loc_reg(OUT void **o, void *const *s CTX) __asm__("$s10Foundation6LocaleV6regionAC6RegionVSgvg");
        SW fdn_str reg_id(void *const *s CTX) __asm__("$s10Foundation6LocaleV6RegionV10identifierSSvg");
        SW void *loc_to_ns(void *const *s CTX) __asm__("$s10Foundation6LocaleV19_bridgeToObjectiveCSo8NSLocaleCyF");
        SW resp ma_loc(uint64_t) __asm__("$s10Foundation6LocaleVMa");
        SW resp ma_tz(uint64_t) __asm__("$s10Foundation8TimeZoneVMa");
        SW void tz_current(OUT void **o) __asm__("$s10Foundation8TimeZoneV7currentACvgZ");
        SW void tz_secs(OUT void **o, long s) __asm__("$s10Foundation8TimeZoneV14secondsFromGMTACSgSi_tcfC");
        SW void tz_ident(OUT void **o, uint64_t w0, uint64_t w1) __asm__("$s10Foundation8TimeZoneV10identifierACSgSSh_tcfC");
        SW fdn_str tz_id(void *const *s CTX) __asm__("$s10Foundation8TimeZoneV10identifierSSvg");
        SW long tz_secs_for(const double *d, void *const *s CTX) __asm__("$s10Foundation8TimeZoneV14secondsFromGMT3forSiAA4DateV_tF");
        SW fdn_str tz_abbr_for(const double *d, void *const *s CTX) __asm__("$s10Foundation8TimeZoneV12abbreviation3forSSSgAA4DateV_tF");
        SW _Bool tz_eq(void *const *a, void *const *b) __asm__("$s10Foundation8TimeZoneV2eeoiySbAC_ACtFZ");
        void *l = 0, *m = 0, *t = 0;
        uint8_t opt[9];
        fdn_str s, n;
        char out[32];
        CHECK(((const fdn_vwt *const *)ma_loc(0).md)[-1] == &fdn_vwt_obj && ((const fdn_vwt *const *)ma_tz(0).md)[-1] == &fdn_vwt_obj, "metadata");
        loc_current(&l);
        str_eq(loc_id(&l), "pt_BR", "Locale.current");
        str_eq(loc_lang(&l), "pt", "languageCode");
        str_eq(loc_region(&l), "BR", "regionCode");
        str_eq(loc_cur(&l), "BRL", "currencyCode");
        str_eq(loc_dec(&l), ",", "decimalSeparator pt");
        CHECK(loc_metric(&l), "métrico");
        s = fdn_str_make("en_US_POSIX", -1);
        loc_init(&m, s.w0, s.w1);
        str_eq(loc_id(&m), "en_US_POSIX", "identifier preservado");
        str_eq(loc_region(&m), "US", "região de en_US_POSIX");
        str_eq(loc_dec(&m), ".", "decimalSeparator en");
        CHECK(!loc_metric(&m) && !loc_eq(&l, &m) && loc_eq(&m, &m), "EUA / ==");
        loc_reg((void **)opt, &m);
        CHECK(opt[8] == 0, "region some");
        memcpy(&t, opt, 8);
        str_eq(reg_id(&t), "US", "Region.identifier");
        s = fdn_str_make("zh-Hans", -1);
        loc_init(&m, s.w0, s.w1);
        n = loc_region(&m);
        CHECK(n.w0 == 0 && n.w1 == 0, "sem região = nil");
        loc_reg((void **)opt, &m);
        CHECK(opt[8] == 1, "region nil");
        CHECK(fdn_loc_part("zh_Hans_CN", FDN_LOC_REGION, out, sizeof out) == 0 && !strcmp(out, "CN"), "script pulado");
        CHECK(fdn_loc_part("es_419", FDN_LOC_REGION, out, sizeof out) == 0 && !strcmp(out, "419"), "região numérica");
        CHECK(fdn_loc_part("de_DE", FDN_LOC_CURRENCY, out, sizeof out) == 0 && !strcmp(out, "EUR"), "euro");
        CHECK(fdn_loc_part("de_CH@currency=EUR", FDN_LOC_CURRENCY, out, sizeof out) == 0 && !strcmp(out, "EUR"), "currency=");
        CHECK(loc_to_ns(&l) != 0, "ponte NSLocale");

        tz_current(&t);
        str_eq(tz_id(&t), "America/Sao_Paulo", "TimeZone.current");
        {
            double d = 0;
            CHECK(tz_secs_for(&d, &t) == -10800, "secondsFromGMT(for:)");
            str_eq(tz_abbr_for(&d, &t), "GMT-3", "abbreviation(for:)");
        }
        tz_secs((void **)opt, 3600);
        CHECK(opt[8] == 0, "secondsFromGMT some");
        tz_secs((void **)opt, 19 * 3600);
        CHECK(opt[8] == 1, "secondsFromGMT fora = nil");
        s = fdn_str_make("UTC", 3);
        tz_ident((void **)opt, s.w0, s.w1);
        CHECK(opt[8] == 0, "identifier UTC");
        memcpy(&m, opt, 8);
        str_eq(tz_id(&m), "UTC", "UTC");
        CHECK(!tz_eq(&m, &t) && tz_eq(&t, &t), "TimeZone ==");
        s = fdn_str_make("Nada", 4);
        tz_ident((void **)opt, s.w0, s.w1);
        CHECK(opt[8] == 1, "identifier inválido = nil");
    }

    /* ---- CharacterSet */
    {
        typedef fdn_cset *cs;
        SW resp ma_cs(uint64_t) __asm__("$s10Foundation12CharacterSetVMa");
        SW void cs_ws(OUT cs *o) __asm__("$s10Foundation12CharacterSetV11whitespacesACvgZ");
        SW void cs_wsnl(OUT cs *o) __asm__("$s10Foundation12CharacterSetV22whitespacesAndNewlinesACvgZ");
        SW void cs_digits(OUT cs *o) __asm__("$s10Foundation12CharacterSetV13decimalDigitsACvgZ");
        SW void cs_alnum(OUT cs *o) __asm__("$s10Foundation12CharacterSetV13alphanumericsACvgZ");
        SW void cs_query(OUT cs *o) __asm__("$s10Foundation12CharacterSetV15urlQueryAllowedACvgZ");
        SW void cs_vazio(OUT cs *o) __asm__("$s10Foundation12CharacterSetVACycfC");
        SW void cs_init_str(OUT cs *o, uint64_t w0, uint64_t w1) __asm__("$s10Foundation12CharacterSetV12charactersInACSSh_tcfC");
        SW void cs_init_range(OUT cs *o, uint64_t r) __asm__("$s10Foundation12CharacterSetV12charactersInACSNys7UnicodeO6ScalarVG_tcfC");
        SW _Bool cs_contains(uint32_t u, cs const *s CTX) __asm__("$s10Foundation12CharacterSetV8containsySbs7UnicodeO6ScalarVF");
        SW _Bool cs_superset(cs const *o, cs const *s CTX) __asm__("$s10Foundation12CharacterSetV10isSuperset2ofSbAC_tF");
        SW void cs_union(OUT cs *o, cs const *b, cs const *s CTX) __asm__("$s10Foundation12CharacterSetV5unionyA2CF");
        SW void cs_sub(OUT cs *o, cs const *b, cs const *s CTX) __asm__("$s10Foundation12CharacterSetV11subtractingyA2CF");
        SW void cs_inv(OUT cs *o, cs const *s CTX) __asm__("$s10Foundation12CharacterSetV8invertedACvg");
        SW void cs_form_union(cs const *b, cs *s CTX) __asm__("$s10Foundation12CharacterSetV9formUnionyyACF");
        SW void cs_insert(uint64_t w0, uint64_t w1, cs *s CTX) __asm__("$s10Foundation12CharacterSetV6insert12charactersInySS_tF");
        SW void cs_remove(uint64_t w0, uint64_t w1, cs *s CTX) __asm__("$s10Foundation12CharacterSetV6remove12charactersInySS_tF");
        SW void cs_invert(cs *s CTX) __asm__("$s10Foundation12CharacterSetV6invertyyF");
        cs a = 0, b = 0, c = 0, d = 0;
        fdn_str s;
        const void *md = ma_cs(0).md;
        CHECK(((const fdn_vwt *const *)md)[-1] == &fdn_vwt_cset, "CharacterSet metadata");
        cs_ws(&a);
        CHECK(cs_contains(' ', &a) && cs_contains('\t', &a) && cs_contains(0x3000, &a) && !cs_contains('\n', &a) && !cs_contains('a', &a), "whitespaces");
        cs_wsnl(&b);
        CHECK(cs_contains('\n', &b) && cs_contains(0x2028, &b) && cs_superset(&a, &b) && !cs_superset(&b, &a), "whitespacesAndNewlines");
        cs_digits(&c);
        CHECK(cs_contains('7', &c) && cs_contains(0x667, &c) && !cs_contains('a', &c), "decimalDigits");
        cs_alnum(&d);
        CHECK(cs_contains('Z', &d) && cs_contains(0xE9, &d) && cs_contains('3', &d) && cs_contains(0x4E2D, &d) && !cs_contains('-', &d), "alphanumerics");
        cs_query(&d);
        CHECK(cs_contains('?', &d) && cs_contains('=', &d) && !cs_contains(' ', &d) && !cs_contains('#', &d) && !cs_contains(0xE9, &d), "urlQueryAllowed");
        s = fdn_str_make("abcé😀", -1);
        cs_init_str(&a, s.w0, s.w1);
        CHECK(cs_contains('b', &a) && cs_contains(0xE9, &a) && cs_contains(0x1F600, &a) && !cs_contains('d', &a), "charactersIn String");
        cs_init_range(&b, (uint64_t)'m' << 32 | 'k');
        CHECK(cs_contains('k', &b) && cs_contains('l', &b) && cs_contains('m', &b) && !cs_contains('n', &b), "charactersIn ClosedRange");
        cs_union(&c, &b, &a);
        CHECK(cs_contains('a', &c) && cs_contains('l', &c) && cs_superset(&a, &c), "union");
        cs_sub(&d, &b, &c);
        CHECK(cs_contains('a', &d) && !cs_contains('l', &d), "subtracting");
        cs_inv(&d, &a);
        CHECK(!cs_contains('a', &d) && cs_contains('z', &d) && cs_contains(0x10FFFF, &d) && cs_contains(0, &d), "inverted");
        /* mutating com cópia compartilhada: a outra cópia não muda */
        {
            cs e = 0;
            fdn_vwt_cset.init_copy(&e, &a, md);
            CHECK(e == a, "copia compartilha o bloco");
            cs_form_union(&b, &e);
            CHECK(cs_contains('l', &e) && !cs_contains('l', &a), "formUnion copy-on-write");
            s = fdn_str_make("xyz", 3);
            cs_insert(s.w0, s.w1, &e);
            CHECK(cs_contains('y', &e), "insert(charactersIn:)");
            s = fdn_str_make("ab", 2);
            cs_remove(s.w0, s.w1, &e);
            CHECK(!cs_contains('a', &e) && cs_contains('c', &e), "remove(charactersIn:)");
            cs_invert(&e);
            CHECK(cs_contains('a', &e) && !cs_contains('c', &e), "invert");
            fdn_vwt_cset.destroy(&e, md);
            CHECK(cs_contains('a', &a), "o original continua");
        }
        cs_vazio(&a);
        CHECK(!cs_contains('a', &a) && !cs_contains(0, &a), "vazio");
    }

    /* ---- DateInterval, IndexPath, URLQueryItem */
    {
        typedef struct { double start, duration; } dint;
        SW void di_init_end(OUT dint *o, const double *a, const double *b) __asm__("$s10Foundation12DateIntervalV5start3endAcA0B0V_AGtcfC");
        SW void di_init_dur(OUT dint *o, const double *a, double d) __asm__("$s10Foundation12DateIntervalV5start8durationAcA0B0V_SdtcfC");
        SW void di_start(OUT double *o, const dint *s CTX) __asm__("$s10Foundation12DateIntervalV5startAA0B0Vvg");
        SW void di_end(OUT double *o, const dint *s CTX) __asm__("$s10Foundation12DateIntervalV3endAA0B0Vvg");
        SW _Bool di_contains(const double *d, const dint *s CTX) __asm__("$s10Foundation12DateIntervalV8containsySbAA0B0VF");
        typedef void *ip;
        SW void ip_index(OUT ip *o, long i) __asm__("$s10Foundation9IndexPathV5indexACSi_tcfC");
        SW void ip_array(OUT ip *o, const void *arr) __asm__("$s10Foundation9IndexPathV12arrayLiteralACSid_tcfC");
        SW long ip_count(ip const *s CTX) __asm__("$s10Foundation9IndexPathV5countSivg");
        SW long ip_get(long i, ip const *s CTX) __asm__("$s10Foundation9IndexPathVyS2icig");
        SW void ip_appending(OUT ip *o, long v, ip const *s CTX) __asm__("$s10Foundation9IndexPathV9appendingyACSiF");
        SW void ip_drop(OUT ip *o, ip const *s CTX) __asm__("$s10Foundation9IndexPathV8dropLastACyF");
        SW _Bool ip_eq(ip const *a, ip const *b) __asm__("$s10Foundation9IndexPathV2eeoiySbAC_ACtFZ");
        SW _Bool ip_lt(ip const *a, ip const *b) __asm__("$s10Foundation9IndexPathV1loiySbAC_ACtFZ");
        SW long ip_compare(ip const *o, ip const *s CTX) __asm__("$s10Foundation9IndexPathV7compareySo18NSComparisonResultVACF");
        SW void qi_init(OUT void **o, uint64_t a, uint64_t b, uint64_t c, uint64_t d) __asm__("$s10Foundation12URLQueryItemV4name5valueACSSh_SSSghtcfC");
        SW fdn_str qi_name(void *const *s CTX) __asm__("$s10Foundation12URLQueryItemV4nameSSvg");
        SW fdn_str qi_value(void *const *s CTX) __asm__("$s10Foundation12URLQueryItemV5valueSSSgvg");
        SW resp ma_ip(uint64_t) __asm__("$s10Foundation9IndexPathVMa");
        dint di;
        double a = 100, b = 160, x;
        di_init_end(&di, &a, &b);
        CHECK(di.start == 100 && di.duration == 60, "DateInterval(start:end:)");
        di_end(&x, &di);
        CHECK(x == 160, "end");
        di_start(&x, &di);
        CHECK(x == 100, "start");
        CHECK(di_contains(&a, &di) && di_contains(&b, &di) && !di_contains(&(double){ 161 }, &di), "contains");
        di_init_dur(&di, &a, 5);
        CHECK(di.duration == 5, "start:duration:");

        ip p = 0, q = 0, r = 0;
        struct { void *isa; long rc, count, cap; long el[3]; } arr = { 0, 1, 3, 6, { 2, 5, 9 } };
        ip_array(&p, &arr);
        CHECK(ip_count(&p) == 3 && ip_get(1, &p) == 5 && ip_get(2, &p) == 9, "IndexPath arrayLiteral");
        ip_appending(&q, 4, &p);
        CHECK(ip_count(&q) == 4 && ip_get(3, &q) == 4 && ip_count(&p) == 3, "appending");
        ip_drop(&r, &q);
        CHECK(ip_eq(&r, &p) && !ip_eq(&q, &p) && ip_lt(&p, &q) && ip_compare(&p, &q) == 1, "dropLast / == / <");
        ip_index(&r, 7);
        CHECK(ip_count(&r) == 1 && ip_get(0, &r) == 7, "init(index:)");
        {
            const void *md = ma_ip(0).md;
            ip c = 0;
            fdn_vwt_box.init_copy(&c, &p, md);
            CHECK(c == p && ((fdn_box *)p)->rc == 2, "box copy");
            fdn_vwt_box.destroy(&c, md);
            CHECK(((fdn_box *)p)->rc == 1, "box destroy");
        }

        void *qi = 0;
        fdn_str n = fdn_str_make("chave", 5), v = fdn_str_make("um valor bem comprido aqui", -1);
        qi_init(&qi, n.w0, n.w1, v.w0, v.w1);
        str_eq(qi_name(&qi), "chave", "URLQueryItem.name");
        str_eq(qi_value(&qi), "um valor bem comprido aqui", "URLQueryItem.value");
        qi_init(&qi, n.w0, n.w1, 0, 0);
        v = qi_value(&qi);
        CHECK(v.w0 == 0 && v.w1 == 0, "value nil");
    }

    printf("fdn_tipos: %d/%d\n", total - falhas, total);
    return falhas ? 1 : 0;
}
