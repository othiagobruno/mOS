/*
 * mOS libSystem — CryptoKit P256.Signing (PrivateKey(pemRepresentation:), signature(for:) e
 * ECDSASignature.rawRepresentation), com a ABI Swift do CryptoKit resiliente da Apple.
 *
 * O resto do CryptoKit é o framework de userland/frameworks/CryptoKit (tools/build-cryptokit-macho.sh). O
 * carregador procura cada import primeiro no framework do disco e depois no libSystem
 * (kernel/compat/macho.c, resolve), então estes símbolos valem com ou sem o framework na imagem.
 *
 * Os tipos são resilientes para o app (ele só conhece o tamanho pela tabela de value witnesses):
 *   PrivateKey     = ponteiro para uma caixa imutável com contagem de referência (escalar d, 32 bytes)
 *   ECDSASignature = idem, com r||s (64 bytes)
 * Cópia = retain, destruição = release; sem extra inhabitants (Optional usa um byte de tag a mais, com o
 * mesmo algoritmo de swift_getEnumTagSinglePayloadGeneric).
 *
 * Convenção swiftcc x86_64: resultado indireto em rax, self em r13, erro em r12, argumentos em
 * rdi/rsi/rdx/rcx/r8/r9; genéricos (metadado, witness table) depois dos argumentos formais. Os trampolins
 * em assembly passam isso para a convenção C.
 *
 * P-256 de verdade: o PEM (PKCS#8 ou SEC1) é lido por ck_pem.c, o resumo é SHA-256 e a assinatura ECDSA
 * vem de kernel/net/pk.c (mos_ck_ec_sign, cryptokit.c). PEM inválido lança CryptoKitASN1Error
 * .invalidPEMDocument, como a Apple. O argumento genérico de signature(for:) é lido pelo metadado do tipo:
 * Data, [UInt8], ContiguousArray<UInt8>, ArraySlice<UInt8>, Unsafe(Mutable)RawBufferPointer e
 * Unsafe(Mutable)BufferPointer<UInt8>; outro DataProtocol lança CryptoKitASN1Error.invalidASN1Object.
 */
#if defined(__x86_64__)
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef uint8_t u8;
typedef uint32_t u32;
typedef uint64_t u64;

void *malloc(size_t n);
void free(void *p);
long write(int fd, const void *b, unsigned long n);
int mos_ck_hash(int alg, const void *data, size_t n, u8 *out);
int mos_ck_ec_sign(int bits, const u8 *priv, const u8 *digest, size_t dlen, u8 *rs);
__attribute__((swiftcall)) int mos_ck_p256_pem_priv(u64 w0, u64 w1, u8 *out);
int fdn_rep_view(const u8 r[16], const u8 **out, int64_t *n, u8 tmp[14]);
void fdn_rep_from_bytes(u8 r[16], const void *bytes, int64_t n);
void swift_release(void *o);
void objc_release(void *o);
void *swift_allocObject(const void *md, size_t size, size_t align_mask);
void swift_deallocObject(void *o, size_t size, size_t align_mask);

enum { CK_SHA256 = 3 };

/* ------------------------------------------------------------------ caixa imutável com contagem */
typedef struct { long rc; u32 n; u8 b[]; } ck_box;
static ck_box *box_new(const u8 *b, u32 n) {
    ck_box *x = malloc(sizeof *x + n);
    if (!x) return 0;
    x->rc = 1;
    x->n = n;
    memcpy(x->b, b, n);
    return x;
}
static ck_box *box_retain(ck_box *x) { if (x) __atomic_fetch_add(&x->rc, 1, __ATOMIC_RELAXED); return x; }
static void box_release(ck_box *x) {
    if (!x || __atomic_sub_fetch(&x->rc, 1, __ATOMIC_ACQ_REL)) return;
    memset(x->b, 0, x->n);                                 /* chave privada não fica no heap livre */
    free(x);
}

/* ------------------------------------------------------------------ value witnesses */
typedef struct {
    void *(*init_buffer)(void *dest, void *src, const void *md);
    void (*destroy)(void *obj, const void *md);
    void *(*init_copy)(void *dest, void *src, const void *md);
    void *(*assign_copy)(void *dest, void *src, const void *md);
    void *(*init_take)(void *dest, void *src, const void *md);
    void *(*assign_take)(void *dest, void *src, const void *md);
    unsigned (*get_tag_sp)(const void *v, unsigned empty, const void *md);
    void (*store_tag_sp)(void *v, unsigned which, unsigned empty, const void *md);
    size_t size, stride;
    u32 flags, xi;
    /* enum witnesses (HasEnumWitnesses) */
    unsigned (*get_enum_tag)(const void *v, const void *md);
    void (*project_enum)(void *v, const void *md);
    void (*inject_enum)(void *v, unsigned tag, const void *md);
} ck_vwt;
enum { VW_NON_POD = 0x10000, VW_ENUM_WITNESSES = 0x200000 };

/* byte(s) de tag depois do valor quando o tipo não tem extra inhabitants (getEnumTagCounts do runtime) */
static unsigned tag_bytes(size_t size, unsigned empty) {
    unsigned tags = 1;
    if (empty) {
        if (size >= 4) tags += 1;
        else { unsigned bits = (unsigned)size * 8; tags += (empty + ((1u << bits) - 1)) >> bits; }
    }
    return tags <= 1 ? 0 : tags < 256 ? 1 : tags < 65536 ? 2 : 4;
}
static unsigned ld(const u8 *p, unsigned n) { unsigned v = 0; for (unsigned i = 0; i < n; i++) v |= (unsigned)p[i] << (8 * i); return v; }
static void st(u8 *p, unsigned v, unsigned n) { for (unsigned i = 0; i < n; i++) p[i] = (u8)(v >> (8 * i)); }
static unsigned tag_get(const u8 *v, unsigned empty, size_t size) {
    unsigned tb = empty ? tag_bytes(size, empty) : 0;
    if (!tb) return 0;
    unsigned t = ld(v + size, tb);
    if (!t) return 0;
    unsigned hi = size >= 4 ? 0 : (t - 1) << (size * 8);
    unsigned lo = size ? ld(v, size >= 4 ? 4 : (unsigned)size) : 0;
    return (hi | lo) + 1;
}
static void tag_store(u8 *v, unsigned which, unsigned empty, size_t size) {
    unsigned tb = empty ? tag_bytes(size, empty) : 0;
    if (!which) { if (tb) st(v + size, 0, tb); return; }
    unsigned ci = which - 1, pi, ti;
    if (size >= 4) { ti = 1; pi = ci; }
    else { ti = 1 + (ci >> (size * 8)); pi = ci & ((1u << (size * 8)) - 1); }
    if (size) {
        st(v, pi, size >= 4 ? 4 : (unsigned)size);
        if (size > 4) memset(v + 4, 0, size - 4);
    }
    if (tb) st(v + size, ti, tb);
}

/* tipo-caixa: 8 bytes, cabe no buffer inline */
static void *bx_init_buffer(void *d, void *s, const void *md) { (void)md; *(ck_box **)d = box_retain(*(ck_box **)s); return d; }
static void bx_destroy(void *o, const void *md) { (void)md; box_release(*(ck_box **)o); }
static void *bx_assign_copy(void *d, void *s, const void *md) {
    (void)md;
    ck_box *old = *(ck_box **)d;
    *(ck_box **)d = box_retain(*(ck_box **)s);
    box_release(old);
    return d;
}
static void *bx_init_take(void *d, void *s, const void *md) { (void)md; *(ck_box **)d = *(ck_box **)s; return d; }
static void *bx_assign_take(void *d, void *s, const void *md) {
    (void)md;
    ck_box *old = *(ck_box **)d;
    *(ck_box **)d = *(ck_box **)s;
    box_release(old);
    return d;
}
static unsigned bx_get_tag(const void *v, unsigned e, const void *md) { (void)md; return tag_get(v, e, 8); }
static void bx_store_tag(void *v, unsigned w, unsigned e, const void *md) { (void)md; tag_store(v, w, e, 8); }
static const ck_vwt vw_box = {bx_init_buffer, bx_destroy, bx_init_buffer, bx_assign_copy, bx_init_take, bx_assign_take,
                              bx_get_tag, bx_store_tag, 8, 8, 7 | VW_NON_POD, 0, 0, 0, 0};

/* enum sem payload de 1 byte (CryptoKitASN1Error) e enum vazio (os espaços de nome P256, Signing) */
static void *b1_copy(void *d, void *s, const void *md) { (void)md; memcpy(d, s, 1); return d; }
static void b1_destroy(void *o, const void *md) { (void)o; (void)md; }   /* POD: nada a destruir */
static unsigned b1_get_tag(const void *v, unsigned e, const void *md) { (void)md; return tag_get(v, e, 1); }
static void b1_store_tag(void *v, unsigned w, unsigned e, const void *md) { (void)md; tag_store(v, w, e, 1); }
static unsigned b1_enum_tag(const void *v, const void *md) { (void)md; return *(const u8 *)v; }
static void b1_project(void *v, const void *md) { (void)v; (void)md; }   /* sem payload: não há o que projetar */
static void b1_inject(void *v, unsigned tag, const void *md) { (void)md; *(u8 *)v = (u8)tag; }
static const ck_vwt vw_err = {b1_copy, b1_destroy, b1_copy, b1_copy, b1_copy, b1_copy, b1_get_tag, b1_store_tag,
                              1, 1, 0 | VW_ENUM_WITNESSES, 0, b1_enum_tag, b1_project, b1_inject};
static void *b0_copy(void *d, void *s, const void *md) { (void)s; (void)md; return d; }   /* tamanho 0 */
static unsigned b0_get_tag(const void *v, unsigned e, const void *md) { (void)md; return tag_get(v, e, 0); }
static void b0_store_tag(void *v, unsigned w, unsigned e, const void *md) { (void)md; tag_store(v, w, e, 0); }
static unsigned b0_enum_tag(const void *v, const void *md) { (void)v; (void)md; return 0; }
static void b0_inject(void *v, unsigned tag, const void *md) { (void)v; (void)tag; (void)md; }   /* nenhum caso */
static const ck_vwt vw_empty = {b0_copy, b1_destroy, b0_copy, b0_copy, b0_copy, b0_copy, b0_get_tag, b0_store_tag,
                                0, 1, 0 | VW_ENUM_WITNESSES, 0, b0_enum_tag, b1_project, b0_inject};

/* ------------------------------------------------------------------ metadados e acessores */
typedef struct { const ck_vwt *vw; u64 kind; const void *desc; } ck_md;
extern const char ck_d_p256[], ck_d_signing[], ck_d_pk[], ck_d_sig[], ck_d_asn1[];
static const ck_md md_p256 = {&vw_empty, 0x201, ck_d_p256};
static const ck_md md_signing = {&vw_empty, 0x201, ck_d_signing};
static const ck_md md_pk = {&vw_box, 0x200, ck_d_pk};
static const ck_md md_sig = {&vw_box, 0x200, ck_d_sig};
static const ck_md md_asn1 = {&vw_err, 0x201, ck_d_asn1};
typedef struct { const void *md; size_t state; } ck_resp;   /* MetadataResponse: rax = metadado, rdx = 0 (completo) */
ck_resp ck_acc_p256(size_t req) __asm__("$s9CryptoKit4P256OMa");
ck_resp ck_acc_p256(size_t req) { (void)req; return (ck_resp){&md_p256.kind, 0}; }
ck_resp ck_acc_signing(size_t req) __asm__("$s9CryptoKit4P256O7SigningOMa");
ck_resp ck_acc_signing(size_t req) { (void)req; return (ck_resp){&md_signing.kind, 0}; }
ck_resp ck_acc_pk(size_t req) __asm__("$s9CryptoKit4P256O7SigningO10PrivateKeyVMa");
ck_resp ck_acc_pk(size_t req) { (void)req; return (ck_resp){&md_pk.kind, 0}; }
ck_resp ck_acc_sig(size_t req) __asm__("$s9CryptoKit4P256O7SigningO14ECDSASignatureVMa");
ck_resp ck_acc_sig(size_t req) { (void)req; return (ck_resp){&md_sig.kind, 0}; }
/* CryptoKitASN1Error não é importado por nenhum app: acessor, descritor e conformidade ficam com nome interno */
ck_resp ck_acc_asn1(size_t req);
ck_resp ck_acc_asn1(size_t req) { (void)req; return (ck_resp){&md_asn1.kind, 0}; }

/* Descritores de contexto (ABI Swift: ponteiro relativo = destino - endereço do campo).
 * flags: tipo 0 módulo, 17 struct, 18 enum; 0x40 = único. */
__asm__(
    ".section .rodata\n"
    ".p2align 2\n"
    "ck_d_mod:\n"
    "  .long 0\n  .long 0\n  .long ck_n_mod - .\n"
    ".globl \"$s9CryptoKit4P256OMn\"\n"
    "\"$s9CryptoKit4P256OMn\":\n"
    "ck_d_p256:\n"
    "  .long 0x52\n  .long ck_d_mod - .\n  .long ck_n_p256 - .\n  .long \"$s9CryptoKit4P256OMa\" - .\n"
    "  .long 0\n  .long 0\n  .long 0\n"
    ".globl \"$s9CryptoKit4P256O7SigningOMn\"\n"
    "\"$s9CryptoKit4P256O7SigningOMn\":\n"
    "ck_d_signing:\n"
    "  .long 0x52\n  .long ck_d_p256 - .\n  .long ck_n_signing - .\n  .long \"$s9CryptoKit4P256O7SigningOMa\" - .\n"
    "  .long 0\n  .long 0\n  .long 0\n"
    ".globl \"$s9CryptoKit4P256O7SigningO10PrivateKeyVMn\"\n"
    "\"$s9CryptoKit4P256O7SigningO10PrivateKeyVMn\":\n"
    "ck_d_pk:\n"
    "  .long 0x51\n  .long ck_d_signing - .\n  .long ck_n_pk - .\n"
    "  .long \"$s9CryptoKit4P256O7SigningO10PrivateKeyVMa\" - .\n  .long 0\n  .long 0\n  .long 0\n"
    ".globl \"$s9CryptoKit4P256O7SigningO14ECDSASignatureVMn\"\n"
    "\"$s9CryptoKit4P256O7SigningO14ECDSASignatureVMn\":\n"
    "ck_d_sig:\n"
    "  .long 0x51\n  .long ck_d_signing - .\n  .long ck_n_sig - .\n"
    "  .long \"$s9CryptoKit4P256O7SigningO14ECDSASignatureVMa\" - .\n  .long 0\n  .long 0\n  .long 0\n"
    "ck_d_asn1:\n"
    "  .long 0x52\n  .long ck_d_mod - .\n  .long ck_n_asn1 - .\n  .long ck_acc_asn1 - .\n"
    "  .long 0\n  .long 0\n  .long 8\n"
    /* conformidade CryptoKitASN1Error: Error (o protocolo é o símbolo do runtime, por um slot indireto) */
    "ck_conf_asn1:\n"
    "  .long (ck_error_proto_slot - .) + 1\n"
    "  .long ck_d_asn1 - .\n"
    "  .long ck_wt_asn1 - .\n"
    "  .long 0\n"
    "ck_n_mod: .asciz \"CryptoKit\"\n"
    "ck_n_p256: .asciz \"P256\"\n"
    "ck_n_signing: .asciz \"Signing\"\n"
    "ck_n_pk: .asciz \"PrivateKey\"\n"
    "ck_n_sig: .asciz \"ECDSASignature\"\n"
    "ck_n_asn1: .asciz \"CryptoKitASN1Error\"\n"
    /* dados com endereço absoluto: fora do .rodata (relocação RELATIVE do static-pie) */
    ".data\n"
    ".p2align 3\n"
    "ck_error_proto_slot:\n"
    "  .quad \"$ss5ErrorMp\"\n"
    /* witness table: descritor, _domain, _code, _userInfo, _getEmbeddedNSError (ordem dos requisitos de Error) */
    "ck_wt_asn1:\n"
    "  .quad ck_conf_asn1\n"
    "  .quad ck_asn1_domain\n"
    "  .quad ck_asn1_code_tramp\n"
    "  .quad ck_asn1_nil\n"
    "  .quad ck_asn1_nil\n"
    ".text\n");
/* os ck_d_* acima são os mesmos rótulos que os metadados citam */
__asm__(".globl ck_d_p256\n.globl ck_d_signing\n.globl ck_d_pk\n.globl ck_d_sig\n.globl ck_d_asn1\n");

/* ------------------------------------------------------------------ CryptoKitASN1Error: Error */
enum { ASN1_INVALID_OBJECT = 3, ASN1_INVALID_PEM = 7 };   /* ordem dos casos do enum da Apple */
typedef struct { u64 w0, w1; } ck_str;
/* _domain: String pequena "CryptoKit" (cabe nos 15 bytes da forma inline do Swift) */
ck_str ck_asn1_domain(void) {
    static const char d[] = "CryptoKit";
    ck_str s = {0, 0};
    u8 b[16] = {0};
    memcpy(b, d, sizeof d - 1);
    b[15] = (u8)(0xE0 | (sizeof d - 1));
    memcpy(&s.w0, b, 8);
    memcpy(&s.w1, b + 8, 8);
    return s;
}
long ck_asn1_code(const u8 *self) { return self ? *self : 0; }
void *ck_asn1_nil(void) { return 0; }   /* _userInfo e _getEmbeddedNSError: nil, como o padrão do Swift */
__asm__(".text\n.globl ck_asn1_code_tramp\nck_asn1_code_tramp:\n  movq %r13, %rdi\n  jmp ck_asn1_code\n");
extern const void *const ck_wt_asn1[5];

/* Caixa de erro (o que vai em r12): objeto Swift com o tipo e a witness table de Error depois do cabeçalho e o
 * valor em +48, como o SwiftError. O destrutor fica nos dois lugares que os runtimes leem: na palavra
 * apontada (runtime do libSystem, swift_refcount.c) e 16 bytes antes (HeapMetadata do runtime da Apple). */
static void ck_errbox_destroy(void *o) { swift_deallocObject(o, 56, 7); }
static const struct { void (*destroy)(void *); const void *vwt; void (*destroy_ls)(void *); } ck_errbox_md = {
    ck_errbox_destroy, &vw_err, ck_errbox_destroy};
static void *throw_asn1(u8 code, const char *why) {
    if (why) {
        static const char p[] = "CryptoKit (mOS): ";
        write(2, p, sizeof p - 1);
        write(2, why, strlen(why));
        write(2, "\n", 1);
    }
    u8 *o = swift_allocObject(&ck_errbox_md.destroy_ls, 56, 7);
    if (!o) return 0;
    *(const void **)(o + 16) = &md_asn1.kind;
    *(const void *const **)(o + 24) = ck_wt_asn1;
    o[48] = code;
    return o;
}

/* ------------------------------------------------------------------ bytes de um DataProtocol pelo metadado */
static const u8 *rel(const void *field) {
    int32_t o;
    memcpy(&o, field, 4);
    if (!o) return 0;
    const u8 *t = (const u8 *)field + (o & ~1);
    if (o & 1) t = *(const u8 *const *)t;                 /* ponteiro relativo indireto (outro módulo) */
    return t;
}
/* nome do tipo e do módulo do descritor nominal de um metadado de struct/enum */
static int type_names(const void *md, const char **name, const char **module) {
    u64 kind = *(const u64 *)md;
    if (kind != 0x200 && kind != 0x201 && kind != 0x202) return 0;
    const u8 *d = *(const u8 *const *)((const u8 *)md + 8);
    if (!d) return 0;
    *name = (const char *)rel(d + 8);
    const u8 *p = rel(d + 4);
    for (int depth = 0; p && depth < 8; depth++) {
        u32 flags;
        memcpy(&flags, p, 4);
        if ((flags & 0x1f) == 0) { *module = (const char *)rel(p + 8); return *name && *module; }
        p = rel(p + 4);
    }
    return 0;
}
static int is_type(const void *md, const char *module, const char *name) {
    const char *n = 0, *m = 0;
    return md && type_names(md, &n, &m) && !strcmp(n, name) && !strcmp(m, module);
}
/* primeiro argumento genérico de Array/ArraySlice/UnsafeBufferPointer é UInt8? */
static int elem_is_u8(const void *md) {
    const void *e = *(const void *const *)((const u8 *)md + 16);
    return is_type(e, "Swift", "UInt8");
}
/* 0 = achou (p, n); -1 = tipo que este leitor não conhece */
static int dp_bytes(const void *val, const void *md, const u8 **p, int64_t *n, u8 tmp[14]) {
    *p = 0;
    *n = 0;
    if (is_type(md, "Foundation", "Data")) return fdn_rep_view(val, p, n, tmp) ? 0 : -1;
    if ((is_type(md, "Swift", "Array") || is_type(md, "Swift", "ContiguousArray")) && elem_is_u8(md)) {
        const u8 *obj = (const u8 *)(*(const u64 *)val & 0x00FFFFFFFFFFFFF8ULL);
        if (!obj) return 0;
        *n = *(const int64_t *)(obj + 16);                 /* _ContiguousArrayStorage: count em +16, elementos em +32 */
        *p = obj + 32;
        return 0;
    }
    if (is_type(md, "Swift", "ArraySlice") && elem_is_u8(md)) {
        const u64 *s = val;                                /* owner, base, startIndex, (endIndex << 1) | nativo */
        int64_t lo = (int64_t)s[2], hi = (int64_t)(s[3] >> 1);
        *p = (const u8 *)s[1] + lo;
        *n = hi > lo ? hi - lo : 0;
        return 0;
    }
    if (is_type(md, "Swift", "UnsafeRawBufferPointer") || is_type(md, "Swift", "UnsafeMutableRawBufferPointer")) {
        const u64 *s = val;                                /* início, fim */
        *p = (const u8 *)s[0];
        *n = s[0] && s[1] > s[0] ? (int64_t)(s[1] - s[0]) : 0;
        return 0;
    }
    if ((is_type(md, "Swift", "UnsafeBufferPointer") || is_type(md, "Swift", "UnsafeMutableBufferPointer")) && elem_is_u8(md)) {
        const u64 *s = val;                                /* início, contagem */
        *p = (const u8 *)s[0];
        *n = s[0] ? (int64_t)s[1] : 0;
        return 0;
    }
    return -1;
}

/* ------------------------------------------------------------------ as três funções */
/* String do Swift recebida com +1 (argumento de init): solta a referência que não é imortal nem pequena */
static void str_release(u64 w1) {
    if (w1 >> 63) return;                                  /* imortal (literal) ou pequena */
    void *obj = (void *)(uintptr_t)(w1 & 0x0FFFFFFFFFFFFFFFULL);
    if (!obj) return;
    if (w1 & (1ULL << 62)) objc_release(obj);              /* ponte de NSString */
    else swift_release(obj);                               /* armazenamento nativo */
}
void *ck_pk_init_pem(void *out, u64 w0, u64 w1) {
    u8 d[32];
    int r = mos_ck_p256_pem_priv(w0, w1, d);
    str_release(w1);
    if (r) return throw_asn1(ASN1_INVALID_PEM, 0);
    ck_box *b = box_new(d, 32);
    memset(d, 0, sizeof d);
    if (!b) return throw_asn1(ASN1_INVALID_OBJECT, "sem memória para a chave");
    *(ck_box **)out = b;
    return 0;
}
void *ck_pk_sign(void *out, const void *val, const void *md, ck_box *const *self) {
    const u8 *p;
    int64_t n;
    u8 tmp[14], dig[32], rs[64];
    ck_box *k = self ? *self : 0;
    if (!k || k->n != 32) return throw_asn1(ASN1_INVALID_OBJECT, "chave P-256 inválida");
    if (dp_bytes(val, md, &p, &n, tmp)) {
        const char *nm = "?", *mo = "?";
        type_names(md, &nm, &mo);
        (void)mo;
        static const char m[] = "P256.Signing.PrivateKey.signature(for:) não lê este DataProtocol: ";
        write(2, m, sizeof m - 1);
        write(2, nm, strlen(nm));
        write(2, "\n", 1);
        return throw_asn1(ASN1_INVALID_OBJECT, 0);
    }
    if (mos_ck_hash(CK_SHA256, p, (size_t)n, dig)) return throw_asn1(ASN1_INVALID_OBJECT, "SHA-256 falhou");
    if (mos_ck_ec_sign(256, k->b, dig, 32, rs)) return throw_asn1(ASN1_INVALID_OBJECT, "ECDSA P-256 falhou");
    ck_box *s = box_new(rs, 64);
    if (!s) return throw_asn1(ASN1_INVALID_OBJECT, "sem memória para a assinatura");
    *(ck_box **)out = s;
    return 0;
}
/* Data com r||s (64 bytes, a forma "raw" da Apple), devolvido em rax:rdx com +1 */
ck_str ck_sig_raw(ck_box *const *self) {
    u8 r[16];
    ck_str s;
    ck_box *b = self ? *self : 0;
    fdn_rep_from_bytes(r, b ? b->b : 0, b ? b->n : 0);
    memcpy(&s.w0, r, 8);
    memcpy(&s.w1, r + 8, 8);
    return s;
}

/* trampolins swiftcc -> C */
__asm__(
    ".text\n"
    /* PrivateKey.init(pemRepresentation: String) throws: rax = resultado, rdi:rsi = String, r12 = erro */
    ".globl \"$s9CryptoKit4P256O7SigningO10PrivateKeyV17pemRepresentationAGSS_tKcfC\"\n"
    "\"$s9CryptoKit4P256O7SigningO10PrivateKeyV17pemRepresentationAGSS_tKcfC\":\n"
    "  pushq %rax\n"
    "  movq %rsi, %rdx\n"
    "  movq %rdi, %rsi\n"
    "  movq %rax, %rdi\n"
    "  call ck_pk_init_pem\n"
    "  movq %rax, %r12\n"
    "  popq %rax\n"
    "  ret\n"
    /* PrivateKey.signature<D: DataProtocol>(for: D) throws: rax = resultado, rdi = &D, rsi = metadado D,
     * rdx = witness table, r13 = &self, r12 = erro */
    ".globl \"$s9CryptoKit4P256O7SigningO10PrivateKeyV9signature3forAE14ECDSASignatureVx_tK10Foundation12DataProtocolRzlF\"\n"
    "\"$s9CryptoKit4P256O7SigningO10PrivateKeyV9signature3forAE14ECDSASignatureVx_tK10Foundation12DataProtocolRzlF\":\n"
    "  pushq %rax\n"
    "  movq %rsi, %rdx\n"
    "  movq %rdi, %rsi\n"
    "  movq %rax, %rdi\n"
    "  movq %r13, %rcx\n"
    "  call ck_pk_sign\n"
    "  movq %rax, %r12\n"
    "  popq %rax\n"
    "  ret\n"
    /* ECDSASignature.rawRepresentation: Data { get }: r13 = &self, Data em rax:rdx */
    ".globl \"$s9CryptoKit4P256O7SigningO14ECDSASignatureV17rawRepresentation10Foundation4DataVvg\"\n"
    "\"$s9CryptoKit4P256O7SigningO14ECDSASignatureV17rawRepresentation10Foundation4DataVvg\":\n"
    "  movq %r13, %rdi\n"
    "  jmp ck_sig_raw\n");
#endif
