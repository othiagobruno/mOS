/* Teste de host: String, Array e Dictionary do Foundation fazem o trabalho. */
#include <stdint.h>
#include <stdio.h>
typedef void *id;
id fdn_host_msg(id o, const char *sel, uintptr_t a, uintptr_t b, int n) {
    (void)o; (void)sel; (void)a; (void)b; (void)n; return 0;
}
id cx_str(const char *s) { return (id)s; }
id cx_str_n(const char *s, unsigned long n) { (void)n; return (id)s; }
const char *cx_cstr(id o) { return (const char *)o; }
id objc_retain(id o) { return o; }
void objc_release(id o) { (void)o; }
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

typedef struct { uint64_t w0, w1; } swstr;
int fdn_unpack(swstr s, char *buf, int cap);
swstr fdn_pack(const char *s, int n);
void fdn_ss_data_using(uint8_t out[16], uint64_t w0, uint64_t w1, uint64_t enc, int lossy);
swstr fdn_ss_data_init(const uint8_t *r, uint64_t enc);
void *fdn_dict_from_pairs(void **keys, void **vals, int n);
void *fdn_array_from_vals(void **vals, int n);

uint64_t $sSS10FoundationE8EncodingV5asciiACvgZ(void);
uint64_t $sSS10FoundationE8EncodingV9isoLatin1ACvgZ(void);
uint64_t $sSS10FoundationE8EncodingV13nonLossyASCIIACvgZ(void);
uint64_t $sSS10FoundationE8EncodingV7unicodeACvgZ(void);
uint64_t $sSS10FoundationE8EncodingV8rawValueSuvg(uint64_t self);
uint64_t $sSS10FoundationE8EncodingV8rawValueACSu_tcfC(uint64_t raw);
swstr $sSS10FoundationE7cString8encodingSSSgSPys4Int8VG_SSAAE8EncodingVtcfC(const char *p, uint64_t enc);
swstr $sSS10FoundationE14utf16CodeUnits5countSSSPys6UInt16VG_SitcfC(const uint16_t *u, int n);
swstr $sSS10FoundationE10contentsOfSSAA3URLVh_tKcfC(const char *path);
void *$sSD10FoundationE36_unconditionallyBridgeFromObjectiveCySDyxq_GSo12NSDictionaryCSgFZ(void *ns);

extern void $sSS10FoundationE6format_S2Sh_s7CVarArg_pdtcfC(void);
extern void *$sSS10FoundationE4data5using20allowLossyConversionAA4DataVSgSSAAE8EncodingV_SbtF(void);

static int fails;

static void eq(const char *what, const char *got, const char *want) {
    if (strcmp(got, want) != 0) {
        fprintf(stderr, "falha %s: [%s] esperado [%s]\n", what, got, want);
        fails++;
    }
}

static void unpack_eq(const char *what, swstr s, const char *want) {
    char buf[256];
    int n = fdn_unpack(s, buf, (int)sizeof buf);
    if (n < 0) { fprintf(stderr, "falha %s: unpack\n", what); fails++; return; }
    buf[n] = 0;
    eq(what, buf, want);
}

static swstr call_format(swstr fmt, long num, const char *text) {
    swstr r;
    void *fn = (void *)$sSS10FoundationE6format_S2Sh_s7CVarArg_pdtcfC;
    asm volatile(
        "movq %[w0], %%r13\n"
        "movq %[w1], %%r14\n"
        "movq %[num], %%rdx\n"
        "movq %[text], %%rcx\n"
        "call *%[fn]\n"
        : "=a"(r.w0), "=d"(r.w1)
        : [w0] "r"(fmt.w0), [w1] "r"(fmt.w1), [num] "r"(num), [text] "r"(text), [fn] "r"(fn)
        : "rcx", "r8", "r9", "r10", "r11", "r13", "r14", "memory");
    return r;
}

int main(void) {
    char buf[64];
    if ($sSS10FoundationE8EncodingV5asciiACvgZ() != 1) { fprintf(stderr, "ascii\n"); fails++; }
    if ($sSS10FoundationE8EncodingV9isoLatin1ACvgZ() != 5) { fprintf(stderr, "latin1\n"); fails++; }
    if ($sSS10FoundationE8EncodingV13nonLossyASCIIACvgZ() != 7) { fprintf(stderr, "nonlossy\n"); fails++; }
    if ($sSS10FoundationE8EncodingV7unicodeACvgZ() != 10) { fprintf(stderr, "unicode\n"); fails++; }
    if ($sSS10FoundationE8EncodingV8rawValueACSu_tcfC(10) != 10) { fprintf(stderr, "raw init\n"); fails++; }
    if ($sSS10FoundationE8EncodingV8rawValueSuvg(10) != 10) { fprintf(stderr, "raw get\n"); fails++; }

    unpack_eq("cstring", $sSS10FoundationE7cString8encodingSSSgSPys4Int8VG_SSAAE8EncodingVtcfC("olá", 4), "olá");
    unpack_eq("ascii", $sSS10FoundationE7cString8encodingSSSgSPys4Int8VG_SSAAE8EncodingVtcfC("A\xC3", 1), "A?");

    uint16_t u[] = { 'A', 0xE9 };
    unpack_eq("utf16", $sSS10FoundationE14utf16CodeUnits5countSSSPys6UInt16VG_SitcfC(u, 2), "Aé");

    swstr fmt = fdn_pack("n=%d s=%@", 9);
    unpack_eq("format", call_format(fmt, 7, "oi"), "n=7 s=oi");

    FILE *f = fopen("/tmp/mos-string-fdn.txt", "wb");
    if (!f || fwrite("arquivo", 1, 7, f) != 7) { fprintf(stderr, "grava\n"); return 1; }
    fclose(f);
    unpack_eq("contents", $sSS10FoundationE10contentsOfSSAA3URLVh_tKcfC("/tmp/mos-string-fdn.txt"), "arquivo");
    unpack_eq("ausente", $sSS10FoundationE10contentsOfSSAA3URLVh_tKcfC("/tmp/mos-string-fdn-nao-existe"), "");

    void *k = (void *)(uintptr_t)1, *v = (void *)(uintptr_t)2;
    void *keys[] = { k };
    void *vals[] = { v };
    typedef struct { void *isa; int64_t count; int64_t cap; void *key, *val; } dlt;
    void *src = fdn_dict_from_pairs(keys, vals, 1);
    void *cpy = $sSD10FoundationE36_unconditionallyBridgeFromObjectiveCySDyxq_GSo12NSDictionaryCSgFZ(src);
    dlt *d = cpy;
    if (!d || d->count != 1 || d->key != k || d->val != v) { fprintf(stderr, "dict\n"); fails++; }
    if ($sSD10FoundationE36_unconditionallyBridgeFromObjectiveCySDyxq_GSo12NSDictionaryCSgFZ(0) != 0) {
        fprintf(stderr, "dict nil\n"); fails++;
    }
    void *arr = fdn_array_from_vals(vals, 1);
    dlt *a = arr;
    if (!a || a->count != 1 || a->key != v) { fprintf(stderr, "array\n"); fails++; }

    swstr hi = fdn_pack("hi", 2);
    uint8_t rep[16];
    fdn_ss_data_using(rep, hi.w0, hi.w1, 4, 0);
    if (rep[15] != 0 || rep[14] != 2 || rep[0] != 'h' || rep[1] != 'i') {
        fprintf(stderr, "data using %02x %02x %02x %02x\n", rep[0], rep[1], rep[14], rep[15]);
        fails++;
    }
    unpack_eq("data init", fdn_ss_data_init(rep, 4), "hi");

    void *(*data_sym)(uint64_t, int, uint64_t, uint64_t) =
        (void *(*)(uint64_t, int, uint64_t, uint64_t))$sSS10FoundationE4data5using20allowLossyConversionAA4DataVSgSSAAE8EncodingV_SbtF;
    uint8_t *via;
    asm volatile(
        "movq %[w0], %%r13\n"
        "movq %[w1], %%r14\n"
        "movq %[enc], %%rdi\n"
        "xor %%rsi, %%rsi\n"
        "call *%[fn]\n"
        : "=a"(via)
        : [w0] "r"(hi.w0), [w1] "r"(hi.w1), [enc] "r"((uint64_t)4), [fn] "r"(data_sym)
        : "rcx", "rdx", "r8", "r9", "r10", "r11", "r13", "r14", "memory");
    if (!via || via[0] != 'h' || via[14] != 2) { fprintf(stderr, "data simbolo\n"); fails++; }

    if (fails) return 1;
    puts("string-fdn: ok");
    (void)buf;
    return 0;
}
