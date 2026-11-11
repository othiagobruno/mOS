/* mOS libSystem — chave privada P-256 em PEM para o CryptoKit (P256.Signing.PrivateKey(pemRepresentation:)).
 *
 * Aceita "BEGIN PRIVATE KEY" (PKCS#8 com id-ecPublicKey) e "BEGIN EC PRIVATE KEY" (SEC1). Procura no DER o
 * ECPrivateKey (SEQUENCE { INTEGER 1, OCTET STRING d, ... }) e devolve d com 32 bytes (zeros à esquerda se
 * vier menor). Se o DER traz o OID de uma curva e ela não é a prime256v1, recusa: chave de outra curva não
 * vira chave P-256. mos_ck_p256_pem_priv é o ponto de entrada do CryptoKit.swift (swiftcall, String nativa,
 * pequena ou ponte de NSString). O núcleo (ck_p256_pem_der) roda no teste de host (tools/agentes_host.c). */
typedef unsigned char u8;
typedef unsigned long size_t_;

static int b64v(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A';
    if (c >= 'a' && c <= 'z') return c - 'a' + 26;
    if (c >= '0' && c <= '9') return c - '0' + 52;
    if (c == '+' || c == '-') return 62;
    if (c == '/' || c == '_') return 63;
    return -1;
}
static int has(const char *s, long n, const char *w) {
    long wl = 0;
    while (w[wl]) wl++;
    for (long i = 0; i + wl <= n; i++) {
        long k = 0;
        while (k < wl && s[i + k] == w[k]) k++;
        if (k == wl) return (int)i + 1;
    }
    return 0;
}
/* lê um TLV DER; devolve o tamanho total ou -1 */
static long tlv(const u8 *p, long n, int *tag, const u8 **val, long *vlen) {
    if (n < 2) return -1;
    long h = 2, l = p[1];
    if (l & 0x80) {
        int k = (int)(l & 0x7F);
        if (k < 1 || k > 3 || n < 2 + k) return -1;
        l = 0;
        for (int i = 0; i < k; i++) l = (l << 8) | p[2 + i];
        h += k;
    }
    if (h + l > n) return -1;
    *tag = p[0];
    *val = p + h;
    *vlen = l;
    return h + l;
}
static const u8 OID_P256[] = {0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x03, 0x01, 0x07};
static const u8 OID_EC[] = {0x2A, 0x86, 0x48, 0xCE, 0x3D, 0x02, 0x01};

/* varre o DER: acha o ECPrivateKey e anota os OIDs de curva vistos */
static int scan(const u8 *p, long n, u8 *out, int *found, int *curve_ok, int *curve_bad, int depth) {
    while (n > 0 && depth < 8) {
        int tag;
        const u8 *v;
        long vl, t = tlv(p, n, &tag, &v, &vl);
        if (t < 0) return -1;
        if (tag == 0x06) {
            int is_p256 = vl == (long)sizeof OID_P256 && !__builtin_memcmp(v, OID_P256, sizeof OID_P256);
            int is_ec = vl == (long)sizeof OID_EC && !__builtin_memcmp(v, OID_EC, sizeof OID_EC);
            if (is_p256) *curve_ok = 1;
            else if (!is_ec && vl >= 5 && v[0] == 0x2B && v[1] == 0x81 && v[2] == 0x04) *curve_bad = 1;   /* secp384r1, secp521r1... */
        }
        if (tag == 0x30) {
            /* ECPrivateKey: INTEGER 1 seguido de OCTET STRING */
            int t1, t2;
            const u8 *v1, *v2;
            long l1, l2, a = tlv(v, vl, &t1, &v1, &l1);
            if (!*found && a > 0 && t1 == 0x02 && l1 == 1 && v1[0] == 1 && tlv(v + a, vl - a, &t2, &v2, &l2) > 0 && t2 == 0x04 &&
                l2 >= 1 && l2 <= 32) {
                for (int i = 0; i < 32; i++) out[i] = 0;
                for (long i = 0; i < l2; i++) out[32 - l2 + i] = v2[i];
                *found = 1;
            }
            if (scan(v, vl, out, found, curve_ok, curve_bad, depth + 1) < 0) return -1;
        } else if (tag == 0x04 || tag == 0xA0 || tag == 0xA1) {
            /* PKCS#8 embrulha o ECPrivateKey num OCTET STRING; [0] traz o OID da curva no SEC1 */
            if (vl > 2 && v[0] == 0x30) scan(v, vl, out, found, curve_ok, curve_bad, depth + 1);
            else if (tag == 0xA0) scan(v, vl, out, found, curve_ok, curve_bad, depth + 1);
        }
        p += t;
        n -= t;
    }
    return 0;
}

/* 0 = ok (32 bytes em out); -1 = não é PEM/DER de chave privada P-256 */
int ck_p256_pem_der(const char *pem, long n, u8 out[32]) {
    int b = has(pem, n, "-----BEGIN ");
    if (!b) return -1;
    const char *s = pem + b - 1;
    long rest = n - (b - 1);
    int ok_hdr = has(s, rest, "-----BEGIN PRIVATE KEY-----") == 1 || has(s, rest, "-----BEGIN EC PRIVATE KEY-----") == 1;
    if (!ok_hdr) return -1;
    if (has(s, rest, "Proc-Type:")) return -1;                      /* PEM cifrado (DEK-Info) não entra */
    long i = 0;
    while (i < rest && s[i] != '\n') i++;                            /* depois da linha BEGIN */
    int e = has(s + i, rest - i, "-----END ");
    if (!e) return -1;
    long body_n = e - 1;
    const char *body = s + i;
    u8 der[2048];
    long o = 0;
    unsigned acc = 0;
    int bits = 0;
    for (long k = 0; k < body_n; k++) {
        int c = (unsigned char)body[k];
        if (c == '=') break;
        int v = b64v(c);
        if (v < 0) continue;                                          /* quebras de linha e espaços */
        acc = (acc << 6) | (unsigned)v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            if (o >= (long)sizeof der) return -1;
            der[o++] = (u8)(acc >> bits);
        }
    }
    int found = 0, curve_ok = 0, curve_bad = 0;
    if (scan(der, o, out, &found, &curve_ok, &curve_bad, 0) < 0) return -1;
    if (!found || curve_bad) return -1;
    int nz = 0;
    for (int k = 0; k < 32; k++) nz |= out[k];
    return nz ? 0 : -1;
}

#ifndef CK_PEM_HOST
/* String do Swift em dois registradores (o mesmo layout que swiftui_model.h lê): pequena (até 15 bytes no próprio
 * valor), nativa (bytes depois do cabeçalho de 32) ou ponte de NSString (bit 62: o objeto, lido com UTF8String). */
typedef unsigned long long u64;
void *objc_msgSend(void *self, void *op, ...);
void *sel_registerName(const char *name);
static const char *swift_str(u64 w0, u64 w1, long *n, char small[16]) {
    unsigned char *b = (unsigned char *)small;
    for (int i = 0; i < 8; i++) { b[i] = (unsigned char)(w0 >> (8 * i)); b[8 + i] = (unsigned char)(w1 >> (8 * i)); }
    if ((b[15] & 0xF0) == 0xE0) { *n = b[15] & 0x0F; return small; }
    if (w1 & (1ULL << 62)) {                                          /* ponte: NSString */
        void *obj = (void *)(w1 & 0x00FFFFFFFFFFFFFFULL);
        const char *c = obj ? ((const char *(*)(void *, void *))objc_msgSend)(obj, sel_registerName("UTF8String")) : 0;
        long k = 0;
        while (c && c[k]) k++;
        *n = k;
        return c;
    }
    *n = (long)(w0 & 0xFFFFFFFFFFFFULL);
    return (const char *)(unsigned long)(w1 & 0x00FFFFFFFFFFFFFFULL) + 32;
}
__attribute__((swiftcall)) int mos_ck_p256_pem_priv(u64 w0, u64 w1, u8 *out) {
    char small[16];
    long n = 0;
    const char *s = swift_str(w0, w1, &n, small);
    if (!s || !out) return -1;
    return ck_p256_pem_der(s, n, out);
}
#endif
