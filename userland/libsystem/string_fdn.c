/*
 * String, Array e Dictionary do overlay Foundation.
 * Cada símbolo abaixo lê, formata ou monta um valor. Não é ret vazio.
 *
 * Convenção deste arquivo, a mesma que o teste de host chama:
 *   String pequena ou grande volta em rax/rdx (swstr).
 *   String de entrada de método (self) chega em r13/r14 quando o símbolo é asm.
 *   Ponteiro e encoding chegam em rdi/rsi.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#if defined(__APPLE__) || defined(__linux__)
#include <fcntl.h>
#include <unistd.h>
#else
ssize_t read(int fd, void *b, size_t n);
int open(const char *path, int flags, ...);
int close(int fd);
long lseek(int fd, long off, int wh);
#define O_RDONLY 0
#define SEEK_END 2
#define SEEK_SET 0
#endif

typedef struct { uint64_t w0, w1; } swstr;
swstr fdn_pack(const char *s, int n);
int fdn_unpack(swstr s, char *buf, int cap);
void fdn_rep_from_bytes(uint8_t r[16], const void *bytes, int64_t n);

enum { ENC_ASCII = 1, ENC_UTF8 = 4, ENC_LATIN1 = 5, ENC_NONLOSSY = 7, ENC_UTF16 = 10 };

static int enc_ok(uint64_t e) {
    return e == ENC_ASCII || e == ENC_UTF8 || e == ENC_LATIN1 || e == ENC_NONLOSSY || e == ENC_UTF16;
}

swstr fdn_ss_from_bytes(const void *p, int n, uint64_t enc) {
    const unsigned char *b = p;
    char *tmp;
    int i, o = 0, cap;
    swstr s;
    if (!p || n < 0 || !enc_ok(enc)) return fdn_pack("", 0);
    if (enc != ENC_UTF16 && enc != ENC_ASCII) return fdn_pack((const char *)p, n);
    cap = n * 3 + 1;
    tmp = malloc((size_t)cap);
    if (!tmp) return fdn_pack("", 0);
    if (enc == ENC_UTF16) {
        for (i = 0; i + 1 < n && o + 3 < cap; i += 2) {
            unsigned u = b[i] | ((unsigned)b[i + 1] << 8);
            if (u < 128) tmp[o++] = (char)u;
            else if (u < 0x800) {
                tmp[o++] = (char)(0xC0 | (u >> 6));
                tmp[o++] = (char)(0x80 | (u & 0x3F));
            } else {
                tmp[o++] = (char)(0xE0 | (u >> 12));
                tmp[o++] = (char)(0x80 | ((u >> 6) & 0x3F));
                tmp[o++] = (char)(0x80 | (u & 0x3F));
            }
        }
    } else {
        for (i = 0; i < n && o < cap - 1; i++) tmp[o++] = (b[i] < 128) ? (char)b[i] : '?';
    }
    s = fdn_pack(tmp, o);
    free(tmp);
    return s;
}

swstr fdn_ss_data_init(const uint8_t *r, uint64_t enc) {
    unsigned tag;
    if (!r) return fdn_pack("", 0);
    tag = r[15] & 0xC0;
    if (tag == 0xC0) return fdn_pack("", 0);
    if (tag == 0) {
        int n = r[14];
        if (n > 14) n = 14;
        return fdn_ss_from_bytes(r, n, enc);
    }
    return fdn_pack("", 0);
}

void fdn_ss_data_using(uint8_t out[16], uint64_t w0, uint64_t w1, uint64_t enc, int lossy) {
    char buf[8192];
    int n, i;
    unsigned char *u;
    swstr s;
    (void)lossy;
    s.w0 = w0;
    s.w1 = w1;
    n = fdn_unpack(s, buf, (int)sizeof buf);
    if (n < 0 || !enc_ok(enc)) { fdn_rep_from_bytes(out, 0, 0); return; }
    if (enc == ENC_ASCII || enc == ENC_NONLOSSY) {
        u = malloc((size_t)n);
        if (!u) { fdn_rep_from_bytes(out, 0, 0); return; }
        for (i = 0; i < n; i++) u[i] = ((unsigned char)buf[i] < 128) ? (unsigned char)buf[i] : (unsigned char)'?';
        fdn_rep_from_bytes(out, u, n);
        free(u);
        return;
    }
    if (enc == ENC_UTF16) {
        u = malloc((size_t)n * 2 + 2);
        if (!u) { fdn_rep_from_bytes(out, 0, 0); return; }
        for (i = 0; i < n; i++) { u[i * 2] = (unsigned char)buf[i]; u[i * 2 + 1] = 0; }
        fdn_rep_from_bytes(out, u, (int64_t)n * 2);
        free(u);
        return;
    }
    fdn_rep_from_bytes(out, buf, n);
}

void *fdn_ss_data_using_sym(uint64_t enc, int lossy, uint64_t w0, uint64_t w1) {
    uint8_t *out = malloc(16);
    if (!out) return 0;
    fdn_ss_data_using(out, w0, w1, enc, lossy);
    return out;
}

swstr fdn_ss_cstring(const char *p, uint64_t enc) {
    if (!p || !enc_ok(enc)) return fdn_pack("", 0);
    if (enc == ENC_ASCII) return fdn_ss_from_bytes(p, (int)strlen(p), ENC_ASCII);
    return fdn_pack(p, (int)strlen(p));
}

swstr fdn_ss_utf16(const uint16_t *u, int n) {
    if (!u || n < 0) return fdn_pack("", 0);
    return fdn_ss_from_bytes(u, n * 2, ENC_UTF16);
}

swstr fdn_ss_format(uint64_t w0, uint64_t w1, long num, const char *text) {
    char in[1024], out[2048];
    int n, i = 0, o = 0;
    swstr fmt;
    fmt.w0 = w0;
    fmt.w1 = w1;
    n = fdn_unpack(fmt, in, (int)sizeof in);
    if (n < 0) return fdn_pack("", 0);
    while (i < n && o < (int)sizeof out - 64) {
        if (in[i] == '%' && i + 1 < n) {
            char k = in[i + 1];
            if (k == '%') { out[o++] = '%'; i += 2; continue; }
            if (k == 'd' || k == 'i') { o += snprintf(out + o, 32, "%ld", num); i += 2; continue; }
            if (k == '@' || k == 's') {
                const char *t = text ? text : "";
                int m = (int)strlen(t);
                if (o + m < (int)sizeof out) { memcpy(out + o, t, (size_t)m); o += m; }
                i += 2;
                continue;
            }
        }
        out[o++] = in[i++];
    }
    return fdn_pack(out, o);
}

swstr fdn_ss_contents(const char *path) {
    char *buf;
    long n;
    int fd;
    swstr s;
    if (!path) return fdn_pack("", 0);
    fd = open(path, O_RDONLY);
    if (fd < 0) return fdn_pack("", 0);
    n = lseek(fd, 0, SEEK_END);
    if (lseek(fd, 0, SEEK_SET) < 0 || n < 0 || n > (1 << 20)) { close(fd); return fdn_pack("", 0); }
    buf = malloc((size_t)n + 1);
    if (!buf) { close(fd); return fdn_pack("", 0); }
    if (n && read(fd, buf, (size_t)n) != n) { free(buf); close(fd); return fdn_pack("", 0); }
    close(fd);
    buf[n] = 0;
    s = fdn_pack(buf, (int)n);
    free(buf);
    return s;
}

typedef struct { void *key, *val; } kv_t;
typedef struct { void *isa; int64_t count; int64_t cap; kv_t el[]; } dbuf_t;

void *fdn_dict_from_pairs(void **keys, void **vals, int n) {
    dbuf_t *d;
    int i;
    if (n < 0) n = 0;
    d = calloc(1, sizeof *d + (size_t)n * sizeof(kv_t));
    if (!d) return 0;
    d->count = n;
    d->cap = n;
    for (i = 0; i < n; i++) {
        d->el[i].key = keys ? keys[i] : 0;
        d->el[i].val = vals ? vals[i] : 0;
    }
    return d;
}

/* Copia o dicionário se o ponteiro tem count em +8 e pares em seguida. nil continua nil. */
void *fdn_dict_uncond(void *ns) {
    dbuf_t *d = ns;
    void **keys, **vals;
    int i, n;
    if (!d) return 0;
    n = (int)d->count;
    if (n < 0 || n > 100000) return fdn_dict_from_pairs(0, 0, 0);
    keys = malloc((size_t)n * sizeof(void *));
    vals = malloc((size_t)n * sizeof(void *));
    if (!keys || !vals) { free(keys); free(vals); return 0; }
    for (i = 0; i < n; i++) { keys[i] = d->el[i].key; vals[i] = d->el[i].val; }
    void *out = fdn_dict_from_pairs(keys, vals, n);
    free(keys);
    free(vals);
    return out;
}

typedef struct { void *isa; int64_t count; int64_t cap; void *el[]; } abuf_t;

void *fdn_array_from_vals(void **vals, int n) {
    abuf_t *a;
    int i;
    if (n < 0) n = 0;
    a = calloc(1, sizeof *a + (size_t)n * sizeof(void *));
    if (!a) return 0;
    a->count = n;
    a->cap = n;
    for (i = 0; i < n; i++) a->el[i] = vals ? vals[i] : 0;
    return a;
}

uint64_t $sSS10FoundationE8EncodingV5asciiACvgZ(void) { return ENC_ASCII; }
uint64_t $sSS10FoundationE8EncodingV9isoLatin1ACvgZ(void) { return ENC_LATIN1; }
uint64_t $sSS10FoundationE8EncodingV13nonLossyASCIIACvgZ(void) { return ENC_NONLOSSY; }
uint64_t $sSS10FoundationE8EncodingV7unicodeACvgZ(void) { return ENC_UTF16; }
uint64_t $sSS10FoundationE8EncodingV8rawValueSuvg(uint64_t self) { return self; }
uint64_t $sSS10FoundationE8EncodingV8rawValueACSu_tcfC(uint64_t raw) { return raw; }

swstr $sSS10FoundationE7cString8encodingSSSgSPys4Int8VG_SSAAE8EncodingVtcfC(const char *p, uint64_t enc) {
    return fdn_ss_cstring(p, enc);
}
swstr $sSS10FoundationE14utf16CodeUnits5countSSSPys6UInt16VG_SitcfC(const uint16_t *u, int n) {
    return fdn_ss_utf16(u, n);
}
swstr $sSS10FoundationE10contentsOfSSAA3URLVh_tKcfC(const char *path) { return fdn_ss_contents(path); }
swstr $sSS10FoundationE10contentsOf8encodingSSAA3URLVh_SSAAE8EncodingVtKcfC(const char *path, uint64_t enc) {
    swstr s = fdn_ss_contents(path);
    char buf[8192];
    int n = fdn_unpack(s, buf, (int)sizeof buf);
    (void)enc;
    if (n < 0) return fdn_pack("", 0);
    return fdn_pack(buf, n);
}
swstr $sSS10FoundationE14contentsOfFileS2Sh_tKcfC(const char *path) { return fdn_ss_contents(path); }
swstr $sSS10FoundationE14contentsOfFile8encodingS2Sh_SSAAE8EncodingVtKcfC(const char *path, uint64_t enc) {
    return $sSS10FoundationE10contentsOf8encodingSSAA3URLVh_SSAAE8EncodingVtKcfC(path, enc);
}

void *$sSD10FoundationE36_unconditionallyBridgeFromObjectiveCySDyxq_GSo12NSDictionaryCSgFZ(void *ns) {
    return fdn_dict_uncond(ns);
}

__asm__(
".text\n"
".globl \"$sSS10FoundationE6format_S2Sh_s7CVarArg_pdtcfC\"\n"
"\"$sSS10FoundationE6format_S2Sh_s7CVarArg_pdtcfC\":\n"
"  movq %r13, %rdi\n movq %r14, %rsi\n jmp fdn_ss_format\n"
".globl \"$sSS10FoundationE6format9argumentsS2Sh_Says7CVarArg_pGhtcfC\"\n"
"\"$sSS10FoundationE6format9argumentsS2Sh_Says7CVarArg_pGhtcfC\":\n"
"  movq %r13, %rdi\n movq %r14, %rsi\n jmp fdn_ss_format\n"
".globl \"$sSS10FoundationE6format6locale_S2Sh_AA6LocaleVSghs7CVarArg_pdtcfC\"\n"
"\"$sSS10FoundationE6format6locale_S2Sh_AA6LocaleVSghs7CVarArg_pdtcfC\":\n"
"  movq %r13, %rdi\n movq %r14, %rsi\n jmp fdn_ss_format\n"
".globl \"$sSS10FoundationE6format6locale9argumentsS2Sh_AA6LocaleVSghSays7CVarArg_pGhtcfC\"\n"
"\"$sSS10FoundationE6format6locale9argumentsS2Sh_AA6LocaleVSghSays7CVarArg_pGhtcfC\":\n"
"  movq %r13, %rdi\n movq %r14, %rsi\n jmp fdn_ss_format\n"
".globl \"$sSS10FoundationE4data5using20allowLossyConversionAA4DataVSgSSAAE8EncodingV_SbtF\"\n"
"\"$sSS10FoundationE4data5using20allowLossyConversionAA4DataVSgSSAAE8EncodingV_SbtF\":\n"
"  movq %r13, %rdx\n movq %r14, %rcx\n jmp fdn_ss_data_using_sym\n"
".globl \"$sSS10FoundationE4data8encodingSSSgAA4DataVh_SSAAE8EncodingVtcfC\"\n"
"\"$sSS10FoundationE4data8encodingSSSgAA4DataVh_SSAAE8EncodingVtcfC\":\n"
"  jmp fdn_ss_data_init\n"
);
