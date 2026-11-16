/*
 * mOS — libSystem: Security.framework (Keychain, assinatura de código, confiança, chaves, SecureTransport).
 *
 * Por quê (medido com tools/machocheck.c nos sete apps-alvo, docs/compat-apps.md): VS Code, Cursor, Claude, Slack e
 * Grok Bot (Electron) guardam a chave de criptografia de cookies/safeStorage no Keychain ("<App> Safe Storage",
 * fuse EnableCookieEncryption ligado no Claude/Slack/Grok); o Codex CLI guarda o login com o crate keyring
 * (SecItem* e SecKeychain*GenericPassword); o rendezvous Mach do Chromium chama SecCodeCopyGuestWithAttributes para
 * validar o par. Ao todo ~120 funções e ~180 constantes kSec* que o mOS não tinha.
 *
 * O que funciona DE VERDADE:
 *  - Keychain de senhas (kSecClassGenericPassword e kSecClassInternetPassword): SecItemAdd/CopyMatching/Update/
 *    Delete (kSecReturnData/Attributes/Ref, kSecMatchLimitOne/All) e a API antiga SecKeychainFindGenericPassword/
 *    AddGenericPassword/Find e AddInternetPassword/ItemCopyContent/ItemModifyAttributesAndData/ItemDelete/
 *    ItemFreeContent/GetPath (o /usr/bin/security de userland/cli-macos/security.c usa essas). Os itens ficam em
 *    $HOME/Library/Keychains/mos-keychain.txt. A versão 2 do arquivo é ciphertext (HMAC em modo
 *    contador + MAC). A chave são 32 bytes de RDSEED/RDRAND (getentropy falha sem essa fonte), em mos-keychain.key (0600, pasta
 *    0700). Um arquivo versão 2 antigo, cifrado com a chave derivada do login, é aberto uma vez
 *    e regravado com a chave nova; se essa regravação falha, o arquivo original fica no lugar.
 *    A versão 1 (texto) ainda é lida e, na gravação seguinte, vira versão 2.
 *  - SecCertificateCreateWithData/CopyData (o certificado é o DER num CFData), SecCopyErrorMessageString.
 * O que responde "indisponível" com o erro que a API documenta (regra de chrome.c: nunca 0/sucesso sem preencher):
 *  - assinatura de código: o mOS não verifica assinaturas; todo código é "não assinado" (errSecCSUnsigned). As
 *    funções que só criam objetos (SecCodeCopySelf, SecStaticCodeCreateWithPath, SecRequirementCreateWithString)
 *    funcionam, e SecCodeCopySigningInformation devolve um dicionário só com o identificador (como o macOS faz
 *    para código não assinado). O rendezvous do Chromium está em modo "só validar", então isso não o bloqueia.
 *  - SecTrust: avaliação devolve "não confiável"; o Chromium/Electron usa o próprio verificador (Chrome Root
 *    Store). As confianças do SecureTransport (securetransport.c), já validadas pelo kernel, avaliam como válidas.
 *  - SecIdentityCreateWithCertificate acha um item kSecClassIdentity ("idnt") cujo segredo é o DER do
 *    certificado. SecItemAdd dessa classe grava o DER e, em kSecAttrGeneric, os bytes da chave privada.
 *    SecIdentityCopyPrivateKey devolve esses bytes como SecKey (seckey.c). SecKey é real em seckey.c (RSA e EC).
 *    PKCS12 e transforms de assinatura: fase 2 de docs/servicos-seguranca-plano.md. O SecureTransport é real: securetransport.c.
 *  - Authorization: criar/liberar funciona; pedir direitos = errAuthorizationDenied.
 * Os stubs antigos de chrome.c/mach.c/darwin.c que este arquivo substitui foram tirados de lá.
 */
typedef unsigned char u8;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef unsigned long size_t;
typedef long ssize_t;
typedef void *id_;
typedef int OSStatus;
typedef unsigned char Boolean;

void *malloc(size_t n);
void *realloc(void *p, size_t n);
void *calloc(size_t a, size_t b);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
char *getenv(const char *name);
int cx_snprintf(char *buf, size_t n, const char *fmt, ...);
int open(const char *path, int flags, ...);
ssize_t read(int fd, void *b, size_t n);
ssize_t write(int fd, const void *b, size_t n);
int close(int fd);
int mkdir(const char *p, unsigned mode);
int rename(const char *a, const char *b);
int unlink(const char *p);
#define MAC_O_RDONLY 0
#define MAC_O_WRONLY 1
#define MAC_O_CREAT 0x200
#define MAC_O_TRUNC 0x400

/* ---- ponte com o Foundation do mOS (CFStringRef = NSString*, CFDictionaryRef = NSDictionary*...) */
typedef struct cx_class cx_class_t;
cx_class_t *cx_class_named(const char *name);
id_ objc_msgSend(id_ self, void *op, ...);
void *sel_registerName(const char *name);
id_ objc_retain(id_ o);
void objc_release(id_ o);
#define MSG0(T, o, s) ((T (*)(id_, void *))objc_msgSend)((id_)(o), sel_registerName(s))
#define MSG1(T, o, s, a) ((T (*)(id_, void *, __typeof__(a)))objc_msgSend)((id_)(o), sel_registerName(s), (a))
#define CLS(n) ((id_)cx_class_named(n))

id_ CFDictionaryGetValue(id_ d, id_ key);
id_ CFDictionaryCreateMutable(void *alloc, long cap, const void *kcb, const void *vcb);
void CFDictionarySetValue(id_ d, id_ key, id_ val);
id_ CFArrayCreateMutable(void *alloc, long cap, const void *cb);
void CFArrayAppendValue(id_ a, id_ v);
long CFArrayGetCount(id_ a);
id_ CFArrayGetValueAtIndex(id_ a, long i);
u8 CFEqual(id_ a, id_ b);
u8 CFBooleanGetValue(id_ b);
id_ CFRetain(id_ o);
void CFRelease(id_ o);
u64 CFGetTypeID(id_ o);
u64 CFStringGetTypeID(void);
u64 CFDataGetTypeID(void);
long CFDataGetLength(id_ d);
id_ CFDataCreateMutable(void *alloc, long cap);
void CFDataAppendBytes(id_ d, const u8 *b, long n);
const u8 *CFDataGetBytePtr(id_ d);
id_ cx_data_new(const void *b, u64 n);          /* cfdata.c: NSData +1 */
id_ CFDateCreate(void *alloc, double at);
double CFAbsoluteTimeGetCurrent(void);
extern void *kCFBooleanTrue;

static const char *utf8(id_ s) { return s ? MSG0(const char *, s, "UTF8String") : 0; }
/* NSString +1 a partir de bytes UTF-8 (não necessariamente terminados em 0) */
static id_ str_n(const char *s, u64 n) {
    char *t = malloc(n + 1);
    if (!t) return 0;
    memcpy(t, s, n);
    t[n] = 0;
    id_ r = MSG1(id_, CLS("NSString"), "stringWithUTF8String:", t);
    free(t);
    return r ? objc_retain(r) : 0;
}

/* ---- constantes CFString no formato de @"..." (o mesmo de darwin.c/chrome.c) */
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } cfconst_t;
#define K(var, text) \
    static cfconst_t var##_cs = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; \
    const void *var = &var##_cs;
#include "security_consts.inc"
/* kSecAttrServer não é importado pelos apps-alvo, mas é chave de kSecClassInternetPassword */
static cfconst_t kSecAttrServer_cs = {cx_cfconst_isa, 0x7c8, "srvr", 4};
static const void *kSecAttrServer_ = &kSecAttrServer_cs;
const void *kSecRandomDefault = 0;              /* SecRandomRef: NULL = gerador padrão, como no macOS */

#define K_(v) ((id_)(v))

/* ---- códigos de erro (SecBase.h, CSCommon.h, Authorization.h) */
#define errSecSuccess 0
#define errSecUnimplemented (-4)
#define errSecParam (-50)
#define errSecAllocate (-108)
#define errSecNotAvailable (-25291)
#define errSecDuplicateItem (-25299)
#define errSecItemNotFound (-25300)
#define errSecNoTrustSettings (-25263)
#define errSecNoDefaultKeychain (-25307)
#define errSecInteractionNotAllowed (-25308)
#define errSecCSUnsigned (-67062)
#define errSecCSStaticCodeNotFound (-67068)
#define errAuthorizationDenied (-60005)
#define errAuthorizationSuccess 0

/* ================================================================ Keychain de senhas
 * Um item = classe (genp/inet) + atributos de texto + segredo. A identidade do item (para SecItemAdd recusar
 * duplicatas, como o macOS) é classe + serviço/servidor + conta. */
/* os 7 primeiros são os do formato antigo; os seguintes são opcionais no arquivo (uma linha antiga para antes deles) */
enum { F_SVCE, F_ACCT, F_LABL, F_DESC, F_GENA, F_SRVR, F_AGRP, F_ATAG, F_KCLS, F_KTYP, F_KLBL, F_SUBJ, F_ISSR, F_SLNR, NFIELD };
#define NFIELD_V1 7
static int field_binary(int i) { return i == F_GENA || i == F_ATAG || i == F_KLBL || i == F_SUBJ || i == F_ISSR || i == F_SLNR; }
static const void *const *field_key[NFIELD];
typedef struct {
    int used;
    u32 id;
    char cls[5];
    u8 *f[NFIELD]; u32 fl[NFIELD];   /* bytes e tamanho de cada atributo (NULL = ausente) */
    u8 *data; u32 dlen;
    double cdat, mdat;
} kitem_t;
static kitem_t *items;
static int nitems, capitems;
static u32 next_id = 1;
static int loaded;
static int kc_blocked; /* versão 2 com MAC ruim: não sobrescreve o arquivo */
#include "kcseal.inc"
static volatile int klock;
static void lock(void) { while (__sync_lock_test_and_set(&klock, 1)) ; }
static void unlock(void) { __sync_lock_release(&klock); }

static void init_keys(void) {
    field_key[F_SVCE] = &kSecAttrService; field_key[F_ACCT] = &kSecAttrAccount; field_key[F_LABL] = &kSecAttrLabel;
    field_key[F_DESC] = &kSecAttrDescription; field_key[F_GENA] = &kSecAttrGeneric; field_key[F_SRVR] = &kSecAttrServer_;
    field_key[F_AGRP] = &kSecAttrAccessGroup; field_key[F_ATAG] = &kSecAttrApplicationTag; field_key[F_KCLS] = &kSecAttrKeyClass;
    field_key[F_KTYP] = &kSecAttrKeyType; field_key[F_KLBL] = &kSecAttrApplicationLabel; field_key[F_SUBJ] = &kSecAttrSubject;
    field_key[F_ISSR] = &kSecAttrIssuer; field_key[F_SLNR] = &kSecAttrSerialNumber;
}

static int keychain_path(char *buf, size_t n, int mk) {
    const char *home = getenv("HOME");
    if (!home || !*home) home = "/Usuarios/visitante";
    if (mk) {                                   /* como mkdir -p: cria cada nível que faltar */
        cx_snprintf(buf, n, "%s/Library/Keychains/", home);
        for (char *p = buf + 1; *p; p++)
            if (*p == '/') { *p = 0; mkdir(buf, 0700); *p = '/'; }
    }
    cx_snprintf(buf, n, "%s/Library/Keychains/mos-keychain.txt", home);
    return 1;
}

static int keyfile_path(char *buf, size_t n) {
    const char *home = getenv("HOME");
    if (!home || !*home) home = "/Usuarios/visitante";
    cx_snprintf(buf, n, "%s/Library/Keychains/mos-keychain.key", home);
    return 1;
}

/* formato do arquivo: "mOS-keychain 1\n" e uma linha por item:
 *   classe id cdat mdat dados f0 f1 ... f6\n   (campos binários em hex; "-" = ausente) */
static const char HEX[] = "0123456789abcdef";
static void put_hex(char **o, const u8 *b, u32 n, int present) {
    if (!present) { *(*o)++ = '-'; return; }
    if (!n) { *(*o)++ = '='; return; }          /* presente e vazio */
    for (u32 i = 0; i < n; i++) { *(*o)++ = HEX[b[i] >> 4]; *(*o)++ = HEX[b[i] & 15]; }
}
static int hexv(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; }
static int get_hex(const char **p, const char *e, u8 **out, u32 *n) {
    const char *s = *p;
    while (s < e && *s == ' ') s++;
    const char *t = s;
    while (t < e && *t != ' ' && *t != '\n') t++;
    *p = t;
    if (t == s) return 0;
    if (*s == '-' && t - s == 1) { *out = 0; *n = 0; return 1; }
    if (*s == '=' && t - s == 1) { *out = malloc(1); *n = 0; return *out != 0; }
    u32 len = (u32)((t - s) / 2);
    u8 *b = malloc(len ? len : 1);
    if (!b) return 0;
    for (u32 i = 0; i < len; i++) {
        int h = hexv(s[2 * i]), l = hexv(s[2 * i + 1]);
        if (h < 0 || l < 0) { free(b); return 0; }
        b[i] = (u8)(h << 4 | l);
    }
    *out = b; *n = len;
    return 1;
}
static u64 get_num(const char **p, const char *e) {
    const char *s = *p;
    while (s < e && *s == ' ') s++;
    u64 v = 0;
    while (s < e && *s >= '0' && *s <= '9') v = v * 10 + (u64)(*s++ - '0');
    *p = s;
    return v;
}

static kitem_t *new_item(void) {
    if (nitems == capitems) {
        int nc = capitems ? capitems * 2 : 16;
        kitem_t *ni = realloc(items, (size_t)nc * sizeof *ni);
        if (!ni) return 0;
        items = ni; capitems = nc;
    }
    kitem_t *it = &items[nitems++];
    for (size_t i = 0; i < sizeof *it; i++) ((u8 *)it)[i] = 0;
    it->used = 1;
    return it;
}
static void free_item(kitem_t *it) {
    for (int i = 0; i < NFIELD; i++) free(it->f[i]);
    free(it->data);
    it->used = 0;
}

static void load(void) {
    if (loaded) return;
    loaded = 1;
    init_keys();
    char path[512];
    keychain_path(path, sizeof path, 0);
    int fd = open(path, MAC_O_RDONLY);
    if (fd < 0) return;
    char *buf = 0;
    size_t len = 0, cap = 0;
    for (;;) {
        if (len + 4096 > cap) { cap = (len + 4096) * 2; char *nb = realloc(buf, cap); if (!nb) break; buf = nb; }
        ssize_t k = read(fd, buf + len, cap - len);
        if (k <= 0) break;
        len += (size_t)k;
    }
    close(fd);
    if (!buf) return;
    if (len >= 15 && !memcmp(buf, "mOS-keychain 2\n", 15)) {
        u8 key[32];
        char kpath[512];
        u8 *plain = 0;
        u32 pn = 0;
        keyfile_path(kpath, sizeof kpath);
        int kr = kc_key_read(kpath, key);
        int r = kr == 0 ? kc_open(key, (const u8 *)buf, (u32)len, &plain, &pn) : -1;
        if (r != 0) {
            u8 old[32];
            char login[96];
            kc_login(login, sizeof login);
            kc_key_from(login, old);
            r = kc_open(old, (const u8 *)buf, (u32)len, &plain, &pn);
            memset(old, 0, sizeof old);
            if (r == 0 && plain) {
                if (kr != 0) kr = kc_key_create(kpath, key);
                if (kr == 0) {
                    u8 *sealed = 0;
                    u32 sn = 0;
                    if (kc_seal(key, plain, pn, &sealed, &sn) == 0 && sealed) {
                        if (kc_replace_file(path, sealed, sn) != 0) { /* original conservado */ }
                        memset(sealed, 0, sn);
                        free(sealed);
                    }
                }
            }
        }
        memset(key, 0, sizeof key);
        free(buf);
        if (r != 0 || !plain) { kc_blocked = 1; return; }
        buf = (char *)plain;
        len = pn;
    }
    const char *p = buf, *e = buf + len;
    if (len < 15 || memcmp(p, "mOS-keychain 1\n", 15)) { free(buf); return; }
    p += 15;
    while (p < e) {
        const char *eol = p;
        while (eol < e && *eol != '\n') eol++;
        if (eol - p > 5 && p[4] == ' ') {
            kitem_t *it = new_item();
            if (!it) break;
            memcpy(it->cls, p, 4);
            const char *q = p + 5;
            it->id = (u32)get_num(&q, eol);
            it->cdat = (double)get_num(&q, eol);
            it->mdat = (double)get_num(&q, eol);
            int ok = get_hex(&q, eol, &it->data, &it->dlen);
            for (int i = 0; ok && i < NFIELD; i++) {
                if (i >= NFIELD_V1) {                       /* linha de antes dos campos de chave/certificado */
                    const char *t = q;
                    while (t < eol && *t == ' ') t++;
                    if (t >= eol) break;
                }
                ok = get_hex(&q, eol, &it->f[i], &it->fl[i]);
            }
            if (!ok || !it->id) { free_item(it); nitems--; }
            else if (it->id >= next_id) next_id = it->id + 1;
        }
        p = eol + 1;
    }
    free(buf);
}

/* grava num arquivo temporário e troca por rename (o arquivo nunca fica pela metade) */
static OSStatus save(void) {
    if (kc_blocked) return errSecNotAvailable;
    char path[512], tmp[520];
    keychain_path(path, sizeof path, 1);
    cx_snprintf(tmp, sizeof tmp, "%s.tmp", path);
    size_t need = 16;
    for (int i = 0; i < nitems; i++) {
        if (!items[i].used) continue;
        need += 80 + 2 * (size_t)items[i].dlen;
        for (int j = 0; j < NFIELD; j++) need += 2 + 2 * (size_t)items[i].fl[j];
    }
    char *out = malloc(need), *o = out;
    if (!out) return errSecAllocate;
    memcpy(o, "mOS-keychain 1\n", 15); o += 15;
    for (int i = 0; i < nitems; i++) {
        kitem_t *it = &items[i];
        if (!it->used) continue;
        o += cx_snprintf(o, 80, "%.4s %u %llu %llu ", it->cls, it->id, (u64)it->cdat, (u64)it->mdat);
        put_hex(&o, it->data, it->dlen, it->data != 0);
        for (int j = 0; j < NFIELD; j++) { *o++ = ' '; put_hex(&o, it->f[j], it->fl[j], it->f[j] != 0); }
        *o++ = '\n';
    }
    size_t plain_n = (size_t)(o - out);
    u8 key[32];
    char kpath[512];
    u8 *sealed = 0;
    u32 sn = 0;
    keyfile_path(kpath, sizeof kpath);
    int sealed_ok = kc_key_ensure(kpath, key);
    if (sealed_ok == 0) sealed_ok = kc_seal(key, (const u8 *)out, (u32)plain_n, &sealed, &sn);
    else sealed_ok = -1;
    memset(key, 0, sizeof key);
    memset(out, 0, plain_n);
    free(out);
    if (sealed_ok != 0 || !sealed) return errSecAllocate;
    int fd = open(tmp, MAC_O_WRONLY | MAC_O_CREAT | MAC_O_TRUNC, 0600);
    if (fd < 0) { free(sealed); return errSecNotAvailable; }
    size_t total = sn, done = 0;
    while (done < total) {
        ssize_t k = write(fd, sealed + done, total - done);
        if (k <= 0) break;
        done += (size_t)k;
    }
    close(fd);
    memset(sealed, 0, sn);
    free(sealed);
    if (done != total) { unlink(tmp); return errSecNotAvailable; }
    if (rename(tmp, path) != 0) { unlink(tmp); return errSecNotAvailable; }
    return errSecSuccess;
}

/* ---- consulta: classe + atributos de um dicionário CF */
typedef struct {
    char cls[5];
    id_ f[NFIELD];          /* valores pedidos (NSString ou NSData), NULL = não filtra */
    u8 *ref; u32 refn;      /* kSecValueRef de chave/certificado: o item tem de ter estes bytes */
    int has_ref;
} query_t;
static int ref_bytes(id_ ref, u8 **out, u32 *n);
static int is_blob_class(const char *c);
u64 CFArrayGetTypeID(void);
/* bytes de um valor de atributo: CFString (UTF-8) ou CFData */
u64 CFNumberGetTypeID(void);
static const u8 *val_bytes(id_ v, u32 *n) {
    if (!v) { *n = 0; return 0; }
    if (CFGetTypeID(v) == CFNumberGetTypeID()) v = MSG0(id_, v, "stringValue");   /* "42", "73", "1"... */
    if (CFGetTypeID(v) == CFDataGetTypeID()) { *n = (u32)CFDataGetLength(v); return CFDataGetBytePtr(v); }
    const char *s = utf8(v);
    *n = s ? (u32)strlen(s) : 0;
    return (const u8 *)s;
}
static int cls_of(id_ c, char out[5]) {
    const char *s = c ? utf8(c) : 0;
    if (!s || strlen(s) != 4) return 0;
    memcpy(out, s, 5);
    return 1;
}
static void parse_query(id_ q, query_t *x) {
    for (size_t i = 0; i < sizeof *x; i++) ((u8 *)x)[i] = 0;
    if (!q) return;
    cls_of(CFDictionaryGetValue(q, K_(kSecClass)), x->cls);
    for (int i = 0; i < NFIELD; i++) x->f[i] = CFDictionaryGetValue(q, (id_)*field_key[i]);
    id_ r = CFDictionaryGetValue(q, K_(kSecValueRef));
    /* os bytes ficam num buffer só, válido enquanto o lock do Keychain estiver com quem consultou */
    static u8 *qref;
    if (r && is_blob_class(x->cls)) {
        u8 *b = 0;
        u32 n = 0;
        if (ref_bytes(r, &b, &n) == 0) {
            if (qref) free(qref);
            qref = b;
            x->ref = b; x->refn = n; x->has_ref = 1;
        } else x->has_ref = -1;
    }
}
/* bytes guardados de uma referência: certificado = DER; chave = formato externo (PKCS#1 / X9.63) */
u64 SecKeyGetTypeID(void);
id_ SecKeyCopyExternalRepresentation(id_ key, id_ *err);
static int ref_bytes(id_ ref, u8 **out, u32 *n) {
    id_ d = 0;
    if (CFGetTypeID(ref) == CFDataGetTypeID()) d = CFRetain(ref);
    else if (CFGetTypeID(ref) == SecKeyGetTypeID()) d = SecKeyCopyExternalRepresentation(ref, 0);
    if (!d) return -1;
    *n = (u32)CFDataGetLength(d);
    *out = malloc(*n ? *n : 1);
    if (*out) memcpy(*out, CFDataGetBytePtr(d), *n);
    CFRelease(d);
    return *out ? 0 : -1;
}
static int is_password_class(const char *c) { return !strcmp(c, "genp") || !strcmp(c, "inet"); }
static int is_identity_class(const char *c) { return !strcmp(c, "idnt"); }
static int is_blob_class(const char *c) { return !strcmp(c, "keys") || !strcmp(c, "cert"); }
static int is_stored_class(const char *c) { return is_password_class(c) || is_identity_class(c) || is_blob_class(c); }
static int matches(kitem_t *it, query_t *x) {
    if (!it->used || strcmp(it->cls, x->cls)) return 0;
    if (x->has_ref < 0) return 0;
    if (x->has_ref && (it->dlen != x->refn || memcmp(it->data, x->ref, x->refn))) return 0;
    for (int i = 0; i < NFIELD; i++) {
        if (!x->f[i]) continue;
        u32 n;
        const u8 *b = val_bytes(x->f[i], &n);
        if (!it->f[i] || it->fl[i] != n || (n && memcmp(it->f[i], b, n))) return 0;
    }
    return 1;
}
static int same_identity(kitem_t *it, const char *cls, query_t *x) {
    if (!it->used || strcmp(it->cls, cls)) return 0;
    static const int idf[] = {F_SVCE, F_SRVR, F_ACCT};
    for (int k = 0; k < 3; k++) {
        int i = idf[k];
        u32 n;
        const u8 *b = val_bytes(x->f[i], &n);
        int have = x->f[i] != 0;
        if (have != (it->f[i] != 0)) return 0;
        if (have && (it->fl[i] != n || (n && memcmp(it->f[i], b, n)))) return 0;
    }
    return 1;
}
static void set_field(kitem_t *it, int i, const u8 *b, u32 n) {
    free(it->f[i]);
    it->f[i] = malloc(n ? n : 1);
    if (it->f[i] && n) memcpy(it->f[i], b, n);
    it->fl[i] = it->f[i] ? n : 0;
}
static void set_data(kitem_t *it, const u8 *b, u32 n) {
    free(it->data);
    it->data = malloc(n ? n : 1);
    if (it->data && n) memcpy(it->data, b, n);
    it->dlen = it->data ? n : 0;
}

/* referência a um item (SecKeychainItemRef): um NSString "mOS-keychain-item:<id>" — é um objeto CF de verdade
 * (CFRetain/CFRelease funcionam) e sobrevive a o item ser regravado. */
static id_ item_ref(kitem_t *it) {
    char b[48];
    int n = cx_snprintf(b, sizeof b, "mOS-keychain-item:%u", it->id);
    return str_n(b, (u64)n);
}
static kitem_t *item_from_ref(id_ ref) {
    const char *s = ref ? utf8(ref) : 0;
    if (!s || strlen(s) < 18 || memcmp(s, "mOS-keychain-item:", 18)) return 0;
    u32 id = 0;
    for (s += 18; *s >= '0' && *s <= '9'; s++) id = id * 10 + (u32)(*s - '0');
    for (int i = 0; i < nitems; i++) if (items[i].used && items[i].id == id) return &items[i];
    return 0;
}

/* kSecReturnRef de chaves e certificados: o SecKeyRef / SecCertificateRef de verdade */
id_ cx_seckey_import(const u8 *der, u32 n, int priv);
static int is_blob_class(const char *c);
int cx_cert_fields(const u8 *der, u32 n, const u8 **subj, u32 *sl, const u8 **issr, u32 *il, const u8 **ser, u32 *snl,
                   char *label, u32 lcap);
int cx_seckey_describe(const u8 *ext, u32 n, int *priv, int *type, u8 klbl[20]);
static id_ item_object(kitem_t *it) {
    if (!strcmp(it->cls, "cert")) return cx_data_new(it->data, it->dlen);
    if (!strcmp(it->cls, "keys")) {
        int priv = it->f[F_KCLS] && it->fl[F_KCLS] == 1 && it->f[F_KCLS][0] == '1';
        return cx_seckey_import(it->data, it->dlen, priv);
    }
    return item_ref(it);
}
static id_ item_attrs(kitem_t *it, int with_data, int with_ref) {
    id_ d = CFDictionaryCreateMutable(0, 0, 0, 0);
    id_ c = str_n(it->cls, 4);
    CFDictionarySetValue(d, K_(kSecClass), c);
    CFRelease(c);
    for (int i = 0; i < NFIELD; i++) {
        if (!it->f[i]) continue;
        /* gena (genérico) é binário; os demais são texto */
        id_ v = field_binary(i) ? cx_data_new(it->f[i], it->fl[i]) : str_n((const char *)it->f[i], it->fl[i]);
        if (v) { CFDictionarySetValue(d, (id_)*field_key[i], v); CFRelease(v); }
    }
    id_ cd = CFDateCreate(0, it->cdat), md = CFDateCreate(0, it->mdat);
    if (cd) { CFDictionarySetValue(d, K_(kSecAttrCreationDate), cd); CFRelease(cd); }
    if (md) { CFDictionarySetValue(d, K_(kSecAttrModificationDate), md); CFRelease(md); }
    if (with_data) { id_ v = cx_data_new(it->data, it->dlen); CFDictionarySetValue(d, K_(kSecValueData), v); CFRelease(v); }
    if (with_ref) { id_ r = item_object(it); if (r) { CFDictionarySetValue(d, K_(kSecValueRef), r); CFRelease(r); } }
    return d;
}
static int flag(id_ q, const void *key) { id_ v = CFDictionaryGetValue(q, K_(key)); return v && CFBooleanGetValue(v); }

OSStatus SecItemCopyMatching(id_ query, id_ *result) {
    if (result) *result = 0;
    query_t x;
    lock(); load();
    parse_query(query, &x);
    if (!x.cls[0]) { unlock(); return errSecParam; }
    if (!is_stored_class(x.cls)) { unlock(); return errSecItemNotFound; }
    int rd = flag(query, kSecReturnData), ra = flag(query, kSecReturnAttributes), rr = flag(query, kSecReturnRef);
    int rp = flag(query, kSecReturnPersistentRef);
    id_ lim = CFDictionaryGetValue(query, K_(kSecMatchLimit));
    int all = lim && CFEqual(lim, K_(kSecMatchLimitAll));
    id_ arr = all ? CFArrayCreateMutable(0, 0, 0) : 0, one = 0;
    int found = 0;
    for (int i = 0; i < nitems; i++) {
        kitem_t *it = &items[i];
        if (!matches(it, &x)) continue;
        found++;
        id_ r = 0;
        if (ra || (rd + rr + rp) > 1) r = item_attrs(it, rd, rr || rp);
        else if (rd) r = cx_data_new(it->data, it->dlen);
        else if (rr) r = item_object(it);
        else if (rp) r = item_ref(it);
        if (all) { if (r) { CFArrayAppendValue(arr, r); CFRelease(r); } }
        else { one = r; break; }
    }
    unlock();
    if (!found) { if (arr) CFRelease(arr); return errSecItemNotFound; }
    if (result) *result = all ? arr : one;
    else { if (arr) CFRelease(arr); if (one) CFRelease(one); }
    return errSecSuccess;
}

OSStatus SecItemAdd(id_ attrs, id_ *result) {
    if (result) *result = 0;
    query_t x;
    lock(); load();
    parse_query(attrs, &x);
    if (!x.cls[0]) { unlock(); return errSecParam; }
    if (!is_stored_class(x.cls)) { unlock(); return errSecUnimplemented; }
    u32 idn = 0;
    const u8 *idb = val_bytes(CFDictionaryGetValue(attrs, K_(kSecValueData)), &idn);
    /* chave ou certificado: os bytes vêm do kSecValueRef (ou de kSecValueData) e os atributos de busca
     * (classe/tipo/rótulo da chave; emissor/assunto/série do certificado) são preenchidos aqui, como o macOS faz */
    const u8 *auto_b[NFIELD] = {0};
    u32 auto_n[NFIELD] = {0};
    u8 kcls_s[2], ktyp_s[3], klbl[20];
    char clabel[128];
    if (is_blob_class(x.cls)) {
        if (x.has_ref == -1) { unlock(); return errSecParam; }
        if (x.has_ref) { idb = x.ref; idn = x.refn; }
        if (!idb || idn < 2) { unlock(); return errSecParam; }
        if (!strcmp(x.cls, "cert")) {
            if (cx_cert_fields(idb, idn, &auto_b[F_SUBJ], &auto_n[F_SUBJ], &auto_b[F_ISSR], &auto_n[F_ISSR],
                               &auto_b[F_SLNR], &auto_n[F_SLNR], clabel, sizeof clabel)) { unlock(); return errSecParam; }
            if (clabel[0]) { auto_b[F_LABL] = (const u8 *)clabel; auto_n[F_LABL] = (u32)strlen(clabel); }
        } else {
            int priv, type;
            if (cx_seckey_describe(idb, idn, &priv, &type, klbl)) { unlock(); return errSecParam; }
            kcls_s[0] = priv ? '1' : '0';
            auto_b[F_KCLS] = kcls_s; auto_n[F_KCLS] = 1;
            ktyp_s[0] = type == 1 ? '4' : '7'; ktyp_s[1] = type == 1 ? '2' : '3';
            auto_b[F_KTYP] = ktyp_s; auto_n[F_KTYP] = 2;
            auto_b[F_KLBL] = klbl; auto_n[F_KLBL] = 20;
        }
        for (int i = 0; i < nitems; i++) {
            kitem_t *it = &items[i];
            if (it->used && !strcmp(it->cls, x.cls) && it->dlen == idn && !memcmp(it->data, idb, idn)) { unlock(); return errSecDuplicateItem; }
        }
    } else if (is_identity_class(x.cls)) {
        if (!idb || idn < 2 || idb[0] != 0x30) { unlock(); return errSecParam; }
        for (int i = 0; i < nitems; i++) {
            kitem_t *it = &items[i];
            if (it->used && is_identity_class(it->cls) && it->dlen == idn && it->data && !memcmp(it->data, idb, idn)) {
                unlock();
                return errSecDuplicateItem;
            }
        }
    } else {
        for (int i = 0; i < nitems; i++) if (same_identity(&items[i], x.cls, &x)) { unlock(); return errSecDuplicateItem; }
    }
    kitem_t *it = new_item();
    if (!it) { unlock(); return errSecAllocate; }
    memcpy(it->cls, x.cls, 5);
    it->id = next_id++;
    it->cdat = it->mdat = CFAbsoluteTimeGetCurrent();
    for (int i = 0; i < NFIELD; i++) {
        if (x.f[i]) { u32 n; const u8 *b = val_bytes(x.f[i], &n); set_field(it, i, b, n); }
        else if (auto_b[i]) set_field(it, i, auto_b[i], auto_n[i]);
    }
    if (is_blob_class(x.cls)) set_data(it, idb, idn);
    else {
        u32 n = 0;
        const u8 *b = val_bytes(CFDictionaryGetValue(attrs, K_(kSecValueData)), &n);
        set_data(it, b, n);
    }
    OSStatus st = save();
    if (st == errSecSuccess && result) {
        int rd = flag(attrs, kSecReturnData), ra = flag(attrs, kSecReturnAttributes), rr = flag(attrs, kSecReturnRef);
        if (ra) *result = item_attrs(it, rd, rr);
        else if (rd) *result = cx_data_new(it->data, it->dlen);
        else if (rr) *result = item_object(it);
    }
    if (st != errSecSuccess) { free_item(it); nitems--; }
    unlock();
    return st;
}

OSStatus SecItemUpdate(id_ query, id_ changes) {
    query_t x, c;
    lock(); load();
    parse_query(query, &x);
    if (!x.cls[0]) { unlock(); return errSecParam; }
    if (!is_stored_class(x.cls)) { unlock(); return errSecItemNotFound; }
    parse_query(changes, &c);
    id_ nd = CFDictionaryGetValue(changes, K_(kSecValueData));
    int n = 0;
    for (int i = 0; i < nitems; i++) {
        kitem_t *it = &items[i];
        if (!matches(it, &x)) continue;
        for (int j = 0; j < NFIELD; j++) if (c.f[j]) { u32 l; const u8 *b = val_bytes(c.f[j], &l); set_field(it, j, b, l); }
        if (nd) { u32 l; const u8 *b = val_bytes(nd, &l); set_data(it, b, l); }
        it->mdat = CFAbsoluteTimeGetCurrent();
        n++;
    }
    OSStatus st = n ? save() : errSecItemNotFound;
    unlock();
    return st;
}

OSStatus SecItemDelete(id_ query) {
    query_t x;
    lock(); load();
    parse_query(query, &x);
    if (!x.cls[0]) { unlock(); return errSecParam; }
    if (!is_stored_class(x.cls)) { unlock(); return errSecItemNotFound; }
    int n = 0;
    for (int i = 0; i < nitems; i++) if (matches(&items[i], &x)) { free_item(&items[i]); n++; }
    OSStatus st = n ? save() : errSecItemNotFound;
    unlock();
    return st;
}

/* ---- API antiga (SecKeychain*): o Chromium (KeychainPassword / OSCrypt) e o crate keyring ainda usam */
static void q_from_c(query_t *x, const char *cls, const char *svc, u32 sl, const char *acc, u32 al, int svcfield) {
    for (size_t i = 0; i < sizeof *x; i++) ((u8 *)x)[i] = 0;
    memcpy(x->cls, cls, 5);
    if (svc) x->f[svcfield] = str_n(svc, sl);
    if (acc) x->f[F_ACCT] = str_n(acc, al);
}
static void q_free(query_t *x) { for (int i = 0; i < NFIELD; i++) if (x->f[i]) CFRelease(x->f[i]); }

/* a busca e a gravação da API antiga, para as duas classes de senha: genp (serviço) e inet (servidor) */
static OSStatus find_password(const char *cls, int keyf, const char *svc, u32 sl, const char *acc, u32 al,
                              u32 *pl, void **pw, id_ *item) {
    if (pl) *pl = 0;
    if (pw) *pw = 0;
    if (item) *item = 0;
    query_t x;
    q_from_c(&x, cls, svc, sl, acc, al, keyf);
    lock(); load();
    OSStatus st = errSecItemNotFound;
    for (int i = 0; i < nitems; i++) {
        kitem_t *it = &items[i];
        if (!matches(it, &x)) continue;
        if (pw) {
            *pw = malloc(it->dlen ? it->dlen : 1);           /* liberado por SecKeychainItemFreeContent */
            if (!*pw) { st = errSecAllocate; break; }
            memcpy(*pw, it->data, it->dlen);
        }
        if (pl) *pl = it->dlen;
        if (item) *item = item_ref(it);
        st = errSecSuccess;
        break;
    }
    unlock();
    q_free(&x);
    return st;
}
static OSStatus add_password(const char *cls, int keyf, const char *svc, u32 sl, const char *acc, u32 al,
                             u32 pl, const void *pw, id_ *item) {
    if (item) *item = 0;
    query_t x;
    q_from_c(&x, cls, svc, sl, acc, al, keyf);
    lock(); load();
    OSStatus st = errSecSuccess;
    for (int i = 0; i < nitems; i++) if (same_identity(&items[i], cls, &x)) { st = errSecDuplicateItem; break; }
    kitem_t *it = 0;
    if (st == errSecSuccess && !(it = new_item())) st = errSecAllocate;
    if (st == errSecSuccess) {
        memcpy(it->cls, cls, 5);
        it->id = next_id++;
        it->cdat = it->mdat = CFAbsoluteTimeGetCurrent();
        if (svc) { set_field(it, keyf, (const u8 *)svc, sl); set_field(it, F_LABL, (const u8 *)svc, sl); }
        if (acc) set_field(it, F_ACCT, (const u8 *)acc, al);
        set_data(it, pw, pw ? pl : 0);
        st = save();
        if (st == errSecSuccess) { if (item) *item = item_ref(it); }
        else { free_item(it); nitems--; }
    }
    unlock();
    q_free(&x);
    return st;
}

OSStatus SecKeychainFindGenericPassword(void *kc, u32 sl, const char *svc, u32 al, const char *acc, u32 *pl, void **pw, id_ *item) {
    (void)kc;
    return find_password("genp", F_SVCE, svc, sl, acc, al, pl, pw, item);
}

OSStatus SecKeychainAddGenericPassword(void *kc, u32 sl, const char *svc, u32 al, const char *acc, u32 pl, const void *pw, id_ *item) {
    (void)kc;
    return add_password("genp", F_SVCE, svc, sl, acc, al, pl, pw, item);
}

/* senha de internet: o item guarda servidor e conta. Domínio de segurança, caminho, porta, protocolo e tipo de
 * autenticação não são gravados no mOS-keychain, então não filtram a busca (o /usr/bin/security os mostra vazios). */
OSStatus SecKeychainFindInternetPassword(void *kc, u32 srvl, const char *srv, u32 dl, const char *dom, u32 al, const char *acc,
                                         u32 pathl, const char *path, unsigned short port, u32 proto, u32 auth,
                                         u32 *pl, void **pw, id_ *item) {
    (void)kc; (void)dl; (void)dom; (void)pathl; (void)path; (void)port; (void)proto; (void)auth;
    return find_password("inet", F_SRVR, srv, srvl, acc, al, pl, pw, item);
}
OSStatus SecKeychainAddInternetPassword(void *kc, u32 srvl, const char *srv, u32 dl, const char *dom, u32 al, const char *acc,
                                        u32 pathl, const char *path, unsigned short port, u32 proto, u32 auth,
                                        u32 pl, const void *pw, id_ *item) {
    (void)kc; (void)dl; (void)dom; (void)pathl; (void)path; (void)port; (void)proto; (void)auth;
    return add_password("inet", F_SRVR, srv, srvl, acc, al, pl, pw, item);
}

/* SecKeychainItemCopyContent: classe, atributos pedidos (tags de SecKeychainAttributeList) e o segredo.
 * Atributo que o item não tem volta com length 0 e data NULL. Datas no formato do macOS: "AAAAMMDDhhmmssZ\0". */
typedef struct { u32 tag; u32 length; void *data; } kcattr_t;
typedef struct { u32 count; kcattr_t *attr; } kcattrlist_t;
#define FOURCC(a, b, c, d) (((u32)(a) << 24) | ((u32)(b) << 16) | ((u32)(c) << 8) | (u32)(d))
static int tag_field(u32 tag) {
    switch (tag) {
    case FOURCC('s', 'v', 'c', 'e'): return F_SVCE;
    case FOURCC('a', 'c', 'c', 't'): return F_ACCT;
    case FOURCC('l', 'a', 'b', 'l'): case 7: return F_LABL;
    case FOURCC('d', 'e', 's', 'c'): return F_DESC;
    case FOURCC('g', 'e', 'n', 'a'): return F_GENA;
    case FOURCC('s', 'r', 'v', 'r'): return F_SRVR;
    }
    return -1;
}
static void timedate(double at, char out[16]) {
    long long s = (long long)at + 978307200LL;              /* 2001-01-01 -> 1970-01-01 */
    long long d = s / 86400, r = s % 86400;
    if (r < 0) { r += 86400; d--; }
    /* dias -> data civil (algoritmo de Howard Hinnant) */
    d += 719468;
    long long era = (d >= 0 ? d : d - 146096) / 146097;
    long long doe = d - era * 146097;
    long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long y = yoe + era * 400, doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153;
    long long dd = doy - (153 * mp + 2) / 5 + 1, mm = mp < 10 ? mp + 3 : mp - 9;
    if (mm <= 2) y++;
    cx_snprintf(out, 16, "%04d%02d%02d%02d%02d%02dZ", (int)y, (int)mm, (int)dd, (int)(r / 3600), (int)(r / 60 % 60), (int)(r % 60));
}
OSStatus SecKeychainItemCopyContent(id_ item, u32 *cls, void *attrs, u32 *len, void **data) {
    kcattrlist_t *al = attrs;
    if (len) *len = 0;
    if (data) *data = 0;
    if (al) for (u32 i = 0; i < al->count; i++) { al->attr[i].length = 0; al->attr[i].data = 0; }
    lock(); load();
    kitem_t *it = item_from_ref(item);
    OSStatus st = it ? errSecSuccess : errSecItemNotFound;
    if (it && cls) *cls = FOURCC(it->cls[0], it->cls[1], it->cls[2], it->cls[3]);
    for (u32 i = 0; it && al && i < al->count && st == errSecSuccess; i++) {
        kcattr_t *a = &al->attr[i];
        const u8 *b = 0;
        u32 n = 0;
        char td[16];
        int f = tag_field(a->tag);
        if (f >= 0 && it->f[f]) { b = it->f[f]; n = it->fl[f]; }
        else if (a->tag == FOURCC('c', 'd', 'a', 't') || a->tag == FOURCC('m', 'd', 'a', 't')) {
            timedate(a->tag == FOURCC('c', 'd', 'a', 't') ? it->cdat : it->mdat, td);
            b = (const u8 *)td;
            n = 16;                                         /* o \0 faz parte do valor, como no macOS */
        }
        if (!b) continue;
        a->data = malloc(n ? n : 1);
        if (!a->data) { st = errSecAllocate; break; }
        memcpy(a->data, b, n);
        a->length = n;
    }
    if (it && st == errSecSuccess && data) {
        *data = malloc(it->dlen ? it->dlen : 1);
        if (!*data) st = errSecAllocate;
        else { memcpy(*data, it->data, it->dlen); if (len) *len = it->dlen; }
    }
    unlock();
    return st;
}

/* caminho do arquivo do Keychain (há um só; kc é ignorado) */
OSStatus SecKeychainGetPath(void *kc, u32 *io_len, char *path) {
    (void)kc;
    if (!io_len || !path) return errSecParam;
    char b[512];
    keychain_path(b, sizeof b, 0);
    u32 n = (u32)strlen(b);
    if (n + 1 > *io_len) { *io_len = n + 1; return errSecParam; }
    memcpy(path, b, n + 1);
    *io_len = n;
    return errSecSuccess;
}

OSStatus SecKeychainItemModifyAttributesAndData(id_ item, const void *attrs, u32 len, const void *data) {
    (void)attrs;                                         /* SecKeychainAttributeList: só o segredo é trocado */
    lock(); load();
    kitem_t *it = item_from_ref(item);
    OSStatus st = errSecItemNotFound;
    if (it) {
        if (data) set_data(it, data, len);
        it->mdat = CFAbsoluteTimeGetCurrent();
        st = save();
    }
    unlock();
    return st;
}

OSStatus SecKeychainItemDelete(id_ item) {
    lock(); load();
    kitem_t *it = item_from_ref(item);
    OSStatus st = errSecItemNotFound;
    if (it) { free_item(it); st = save(); }
    unlock();
    return st;
}

/* attrs: a lista que SecKeychainItemCopyContent preencheu (cada data é liberado) */
OSStatus SecKeychainItemFreeContent(void *attrs, void *data) {
    kcattrlist_t *al = attrs;
    if (al) for (u32 i = 0; i < al->count; i++) { free(al->attr[i].data); al->attr[i].data = 0; al->attr[i].length = 0; }
    free(data);
    return errSecSuccess;
}
u64 SecKeychainItemGetTypeID(void) { return CFStringGetTypeID(); }   /* a referência é um CFString (item_ref) */
/* domínio padrão: há um Keychain só, e as funções acima aceitam kc = NULL; o objeto é só um marcador */
OSStatus SecKeychainCopyDomainDefault(int domain, id_ *kc) {
    (void)domain;
    if (kc) *kc = str_n("mOS-keychain", 12);
    return errSecSuccess;
}

/* ================================================================ assinatura de código
 * O mOS não verifica assinaturas: todo código é "não assinado". Os objetos SecCode/SecStaticCode/SecRequirement
 * são NSString (caminho, ou o texto do requisito), que CFRelease aceita. */
OSStatus SecCodeCopySelf(u32 flags, id_ *code) { (void)flags; if (!code) return errSecParam; *code = str_n("mOS-code:self", 13); return errSecSuccess; }
OSStatus SecCodeCopyGuestWithAttributes(id_ host, id_ attrs, u32 flags, id_ *guest) {
    (void)host; (void)attrs; (void)flags;
    if (!guest) return errSecParam;
    *guest = str_n("mOS-code:guest", 14);
    return errSecSuccess;
}
OSStatus SecCodeCopyStaticCode(id_ code, u32 flags, id_ *st) { (void)flags; if (!st) return errSecParam; *st = code ? CFRetain(code) : 0; return code ? errSecSuccess : errSecParam; }
OSStatus SecStaticCodeCreateWithPath(id_ url, u32 flags, id_ *st) {
    (void)flags;
    if (!st) return errSecParam;
    *st = 0;
    if (!url) return errSecParam;
    *st = CFRetain(url);
    return errSecSuccess;
}
OSStatus SecCodeCheckValidity(id_ code, u32 flags, id_ req) { (void)code; (void)flags; (void)req; return errSecCSUnsigned; }
OSStatus SecCodeCheckValidityWithErrors(id_ code, u32 flags, id_ req, id_ *err) { if (err) *err = 0; return SecCodeCheckValidity(code, flags, req); }
OSStatus SecStaticCodeCheckValidity(id_ code, u32 flags, id_ req) { (void)code; (void)flags; (void)req; return errSecCSUnsigned; }
OSStatus SecStaticCodeCheckValidityWithErrors(id_ code, u32 flags, id_ req, id_ *err) { if (err) *err = 0; return errSecCSUnsigned; }
/* requisito designado: o macOS dá um a todo app distribuído (até aos assinados ad hoc). Devolver "não assinado" aqui
 * fazia o Squirrel (autoUpdater do Electron) lançar NSException ao criar o SQRLUpdater (SQRLCodeSignature); o Electron
 * tem @try/@catch em volta, mas o mOS ainda não desenrola exceções, e o VS Code abortava logo depois de abrir o workbench,
 * ao configurar o feed de atualização. As verificações (SecCodeCheckValidity...) continuam dizendo "não assinado". */
OSStatus SecCodeCopyDesignatedRequirement(id_ code, u32 flags, id_ *req) {
    (void)code; (void)flags;
    if (!req) return errSecParam;
    *req = str_n("designated => identifier \"mOS.unsigned\"", 39);
    return errSecSuccess;
}
OSStatus SecCodeCopyPath(id_ code, u32 flags, id_ *path) {
    (void)code; (void)flags;
    if (path) *path = 0;
    return errSecCSStaticCodeNotFound;
}
/* código não assinado no macOS: sucesso, com um dicionário sem identidade de assinatura */
OSStatus SecCodeCopySigningInformation(id_ code, u32 flags, id_ *info) {
    (void)code; (void)flags;
    if (!info) return errSecParam;
    *info = CFDictionaryCreateMutable(0, 0, 0, 0);
    return errSecSuccess;
}
OSStatus SecRequirementCreateWithString(id_ text, u32 flags, id_ *req) { (void)flags; if (!req) return errSecParam; *req = text ? CFRetain(text) : 0; return text ? errSecSuccess : errSecParam; }
OSStatus SecRequirementCreateWithStringAndErrors(id_ text, u32 flags, id_ *errors, id_ *req) {
    if (errors) *errors = 0;
    return SecRequirementCreateWithString(text, flags, req);
}
OSStatus SecRequirementCreateWithData(id_ data, u32 flags, id_ *req) { (void)flags; if (!req) return errSecParam; *req = data ? CFRetain(data) : 0; return data ? errSecSuccess : errSecParam; }
OSStatus SecRequirementCopyData(id_ req, u32 flags, id_ *data) {
    (void)flags;
    if (!data) return errSecParam;
    u32 n;
    const u8 *b = val_bytes(req, &n);
    *data = cx_data_new(b, n);
    return errSecSuccess;
}
id_ SecTaskCopySigningIdentifier(id_ task, id_ *err) { (void)task; if (err) *err = 0; return 0; }
u32 SecTaskGetCodeSignStatus(id_ task) { (void)task; return 0; }   /* sem CS_VALID */
OSStatus SecTrustedApplicationCreateFromPath(const char *path, id_ *app) {
    if (!app) return errSecParam;
    *app = str_n(path ? path : "", path ? strlen(path) : 0);
    return errSecSuccess;
}
OSStatus SecAccessCopyMatchingACLList(id_ acc, id_ tag, id_ *list) { (void)acc; (void)tag; if (list) *list = 0; return errSecUnimplemented; }
OSStatus SecAddSharedWebCredential(id_ fqdn, id_ acc, id_ pw, void *block) { (void)fqdn; (void)acc; (void)pw; (void)block; return errSecUnimplemented; }

/* SecAccessControl: aceito e ignorado (o mOS não tem biometria nem Secure Enclave); é um marcador CF */
id_ SecAccessControlCreateWithFlags(void *alloc, id_ prot, u64 flags, id_ *err) {
    (void)alloc; (void)prot; (void)flags;
    if (err) *err = 0;
    return str_n("mOS-access-control", 18);
}
u64 SecAccessControlGetTypeID(void) { return 0x5EC0A; }

/* ================================================================ certificados e confiança */
#define TID_SEC_CERT 0x5EC01
#define TID_SEC_POLICY 0x5EC02
#define TID_SEC_KEY 0x5EC03
#define TID_SEC_IDENTITY 0x5EC04
u64 SecCertificateGetTypeID(void) { return TID_SEC_CERT; }
u64 SecPolicyGetTypeID(void) { return TID_SEC_POLICY; }
u64 SecKeyGetTypeID(void) { return TID_SEC_KEY; }   /* objetos em seckey.c */
u64 SecIdentityGetTypeID(void) { return TID_SEC_IDENTITY; }

/* o certificado é o próprio DER (CFData) */
id_ SecCertificateCreateWithData(void *alloc, id_ der) {
    (void)alloc;
    if (!der || CFGetTypeID(der) != CFDataGetTypeID() || CFDataGetLength(der) < 4) return 0;
    const u8 *b = CFDataGetBytePtr(der);
    if (b[0] != 0x30) return 0;                       /* DER de um X.509 começa com SEQUENCE */
    return cx_data_new(b, (u64)CFDataGetLength(der));
}
id_ SecCertificateCopyData(id_ cert) { return cert ? cx_data_new(CFDataGetBytePtr(cert), (u64)CFDataGetLength(cert)) : 0; }

/* SecPolicy: um CFString "mOS-policy:<tipo>:<host>" (CFGetTypeID dele é o de CFString; SecPolicyGetTypeID
 * continua distinto, como os apps só comparam com o resultado de SecPolicyCreate*). tipo: ssl, sslc (cliente),
 * x509, rev. */
static id_ policy_new(const char *kind, id_ host) {
    char b[300];
    const char *h = host ? utf8(host) : 0;
    int n = cx_snprintf(b, sizeof b, "mOS-policy:%s:%s", kind, h ? h : "");
    return str_n(b, (u64)n);
}
id_ SecPolicyCreateSSL(Boolean server, id_ host) { return policy_new(server ? "ssl" : "sslc", host); }
id_ SecPolicyCreateBasicX509(void) { return policy_new("x509", 0); }
id_ SecPolicyCreateRevocation(u64 flags) { (void)flags; return policy_new("rev", 0); }
extern const void *kSecPolicyAppleSSL, *kSecPolicyAppleX509Basic, *kSecPolicyAppleRevocation, *kSecPolicyName,
    *kSecPolicyClient, *kSecPolicyOid;
id_ SecPolicyCreateWithProperties(id_ type, id_ props) {
    id_ name = props ? CFDictionaryGetValue(props, K_(kSecPolicyName)) : 0;
    id_ cl = props ? CFDictionaryGetValue(props, K_(kSecPolicyClient)) : 0;
    if (type && CFEqual(type, K_(kSecPolicyAppleSSL))) return policy_new(cl && CFBooleanGetValue(cl) ? "sslc" : "ssl", name);
    if (type && CFEqual(type, K_(kSecPolicyAppleRevocation))) return policy_new("rev", 0);
    return policy_new("x509", 0);
}
static int policy_kind(id_ p, char *host, size_t hn) {   /* -1 = não é política; 0 x509, 1 ssl, 2 sslc, 3 rev */
    const char *s = p ? utf8(p) : 0;
    if (host && hn) host[0] = 0;
    if (!s || memcmp(s, "mOS-policy:", 11)) return -1;
    s += 11;
    int k = !memcmp(s, "sslc:", 5) ? 2 : !memcmp(s, "ssl:", 4) ? 1 : !memcmp(s, "rev:", 4) ? 3 : 0;
    const char *c = s;
    while (*c && *c != ':') c++;
    if (*c == ':' && host) { size_t i = 0; for (c++; *c && i + 1 < hn; c++) host[i++] = *c; host[i] = 0; }
    return k;
}
id_ SecPolicyCopyProperties(id_ policy) {
    id_ d = CFDictionaryCreateMutable(0, 0, 0, 0);
    char host[256];
    int k = policy_kind(policy, host, sizeof host);
    CFDictionarySetValue(d, K_(kSecPolicyOid), k == 1 || k == 2 ? K_(kSecPolicyAppleSSL) : k == 3 ? K_(kSecPolicyAppleRevocation)
                                                                                             : K_(kSecPolicyAppleX509Basic));
    if (host[0]) { id_ h = str_n(host, strlen(host)); CFDictionarySetValue(d, K_(kSecPolicyName), h); CFRelease(h); }
    if (k == 2) CFDictionarySetValue(d, K_(kSecPolicyClient), (id_)kCFBooleanTrue);
    return d;
}

/* SecTrust: o objeto é o CFArray dos certificados; o estado de cada um fica numa tabela ao lado */
typedef struct {
    id_ t;
    int policy;                 /* 0 x509, 1 ssl, 2 sslc */
    char host[256];
    id_ anchors;                /* CFArray retido ou NULL */
    int anchors_only;           /* SecTrustSetAnchorCertificates liga; SetAnchorCertificatesOnly(false) desliga */
    int has_date; double date;  /* CFAbsoluteTime */
    int evaluated; u32 result; int xerr; char msg[160];
    id_ policies;
} tstate_t;
static tstate_t g_ts[128];
static volatile int g_tslock;
static void ts_lock(void) { while (__sync_lock_test_and_set(&g_tslock, 1)) ; }
static void ts_unlock(void) { __sync_lock_release(&g_tslock); }
static tstate_t *ts_get(id_ t, int create) {
    tstate_t *free_ = 0;
    for (int i = 0; i < 128; i++) {
        if (g_ts[i].t == t) return &g_ts[i];
        if (!g_ts[i].t && !free_) free_ = &g_ts[i];
    }
    if (!create) return 0;
    if (!free_) {                                  /* tabela cheia: recicla a entrada mais antiga */
        static int rr;
        free_ = &g_ts[rr++ % 128];
        if (free_->anchors) CFRelease(free_->anchors);
        if (free_->policies) CFRelease(free_->policies);
    }
    for (size_t i = 0; i < sizeof *free_; i++) ((u8 *)free_)[i] = 0;
    free_->t = t;
    return free_;
}
static void ts_apply_policies(tstate_t *st, id_ policies) {
    st->policy = 0;
    st->host[0] = 0;
    if (!policies) return;
    long n = CFGetTypeID(policies) == CFArrayGetTypeID() ? CFArrayGetCount(policies) : -1;
    for (long i = n < 0 ? -1 : 0; i < (n < 0 ? 0 : n); i++) {
        id_ p = n < 0 ? policies : CFArrayGetValueAtIndex(policies, i);
        char h[256];
        int k = policy_kind(p, h, sizeof h);
        if (k == 1 || k == 2) {
            st->policy = k;
            memcpy(st->host, h, sizeof h);
        }
    }
    if (st->policies) CFRelease(st->policies);
    st->policies = CFRetain(policies);
}
static void trust_unmark(id_ t);
OSStatus SecTrustCreateWithCertificates(id_ certs, id_ policies, id_ *trust) {
    if (!trust) return errSecParam;
    *trust = 0;
    if (!certs) return errSecParam;
    id_ arr = CFArrayCreateMutable(0, 0, 0);
    if (CFGetTypeID(certs) == CFDataGetTypeID()) CFArrayAppendValue(arr, certs);   /* um certificado só */
    else for (long i = 0; i < CFArrayGetCount(certs); i++) CFArrayAppendValue(arr, CFArrayGetValueAtIndex(certs, i));
    trust_unmark(arr);                                /* endereço reaproveitado de uma confiança antiga */
    ts_lock();
    tstate_t *st = ts_get(arr, 1);
    ts_apply_policies(st, policies);
    ts_unlock();
    *trust = arr;
    return errSecSuccess;
}
OSStatus SecTrustCreateWithCertificatesAndPolicies(id_ certs, id_ policies, id_ *trust) { return SecTrustCreateWithCertificates(certs, policies, trust); }
u64 CFArrayGetTypeID(void);
/* o SecTrust do mOS é o CFArray dos certificados (acima): CFGetTypeID(trust) == SecTrustGetTypeID() */
u64 SecTrustGetTypeID(void) { return CFArrayGetTypeID(); }
long SecTrustGetCertificateCount(id_ trust) { return trust ? CFArrayGetCount(trust) : 0; }
id_ SecTrustGetCertificateAtIndex(id_ trust, long i) { return trust && i >= 0 && i < CFArrayGetCount(trust) ? CFArrayGetValueAtIndex(trust, i) : 0; }
id_ SecTrustCopyCertificateChain(id_ trust) {
    if (!trust) return 0;
    id_ arr = CFArrayCreateMutable(0, 0, 0);
    for (long i = 0; i < CFArrayGetCount(trust); i++) CFArrayAppendValue(arr, CFArrayGetValueAtIndex(trust, i));
    return arr;
}
static void ts_dirty(tstate_t *st) { if (st) st->evaluated = 0; }
OSStatus SecTrustSetPolicies(id_ trust, id_ p) {
    if (!trust) return errSecParam;
    ts_lock(); tstate_t *st = ts_get(trust, 1); ts_apply_policies(st, p); ts_dirty(st); ts_unlock();
    return errSecSuccess;
}
OSStatus SecTrustCopyPolicies(id_ trust, id_ *out) {
    if (!trust || !out) return errSecParam;
    ts_lock();
    tstate_t *st = ts_get(trust, 0);
    id_ p = st && st->policies ? st->policies : 0;
    if (p) CFRetain(p);
    ts_unlock();
    if (!p) { id_ a = CFArrayCreateMutable(0, 0, 0); id_ b = SecPolicyCreateBasicX509(); CFArrayAppendValue(a, b); CFRelease(b); p = a; }
    else if (CFGetTypeID(p) != CFArrayGetTypeID()) { id_ a = CFArrayCreateMutable(0, 0, 0); CFArrayAppendValue(a, p); CFRelease(p); p = a; }
    *out = p;
    return errSecSuccess;
}
OSStatus SecTrustSetAnchorCertificates(id_ trust, id_ a) {
    if (!trust) return errSecParam;
    ts_lock();
    tstate_t *st = ts_get(trust, 1);
    if (st->anchors) CFRelease(st->anchors);
    st->anchors = a ? CFRetain(a) : 0;
    st->anchors_only = a != 0;                         /* como o macOS: âncoras próprias desligam as do sistema */
    ts_dirty(st);
    ts_unlock();
    return errSecSuccess;
}
OSStatus SecTrustCopyCustomAnchorCertificates(id_ trust, id_ *out) {
    if (!trust || !out) return errSecParam;
    ts_lock();
    tstate_t *st = ts_get(trust, 0);
    *out = st && st->anchors ? CFRetain(st->anchors) : 0;
    ts_unlock();
    return errSecSuccess;
}
OSStatus SecTrustSetAnchorCertificatesOnly(id_ trust, Boolean only) {
    if (!trust) return errSecParam;
    ts_lock(); tstate_t *st = ts_get(trust, 1); st->anchors_only = only != 0; ts_dirty(st); ts_unlock();
    return errSecSuccess;
}
double CFDateGetAbsoluteTime(id_ d);
OSStatus SecTrustSetVerifyDate(id_ trust, id_ d) {
    if (!trust || !d) return errSecParam;
    ts_lock(); tstate_t *st = ts_get(trust, 1); st->has_date = 1; st->date = CFDateGetAbsoluteTime(d); ts_dirty(st); ts_unlock();
    return errSecSuccess;
}
double SecTrustGetVerifyTime(id_ trust) {
    ts_lock();
    tstate_t *st = ts_get(trust, 0);
    double v = st && st->has_date ? st->date : CFAbsoluteTimeGetCurrent();
    ts_unlock();
    return v;
}
/* rede: o mOS não busca intermediários nem OCSP/CRL pela rede (AIA); o valor só é guardado */
static Boolean g_netfetch = 1;
OSStatus SecTrustSetNetworkFetchAllowed(id_ trust, Boolean on) { if (!trust) return errSecParam; g_netfetch = on; return errSecSuccess; }
OSStatus SecTrustGetNetworkFetchAllowed(id_ trust, Boolean *on) { if (!trust || !on) return errSecParam; *on = g_netfetch; return errSecSuccess; }
OSStatus SecTrustSetOCSPResponse(id_ trust, id_ r) { (void)r; return trust ? errSecSuccess : errSecParam; }
OSStatus SecTrustSetSignedCertificateTimestamps(id_ trust, id_ s) { (void)s; return trust ? errSecSuccess : errSecParam; }
#define kSecTrustResultInvalid 0
#define kSecTrustResultProceed 1
#define kSecTrustResultDeny 3
#define kSecTrustResultUnspecified 4
#define kSecTrustResultRecoverableTrustFailure 5
#define kSecTrustResultFatalTrustFailure 6
/* confianças que o SecureTransport devolveu (SSLCopyPeerTrust) depois de o kernel validar a cadeia, as datas e o
 * nome: essas avaliam como válidas sem refazer a conta (a cadeia que o kernel viu pode ter vindo incompleta) */
static id_ g_verified[64];
static unsigned g_nverified;
void cx_trust_mark_verified(id_ t) { g_verified[g_nverified++ % 64] = t; }
void cx_trust_set_host(id_ t, const char *host) {
    if (!t) return;
    ts_lock();
    tstate_t *st = ts_get(t, 1);
    st->policy = 1;
    unsigned n = 0;
    while (host && host[n] && n + 1 < sizeof st->host) { st->host[n] = host[n]; n++; }
    st->host[n] = 0;
    ts_dirty(st);
    ts_unlock();
}
/* Exceções (SecTrustCopyExceptions/SetExceptions): o app guarda o "confiar neste certificado" do usuário.
 * O dado é "mOS-exc" seguido do DER do certificado folha; vale só para a confiança cuja folha é esse DER. */
static id_ g_excepted[64];
static unsigned g_nexcepted;
static const char trust_exc_tag[8] = "mOS-exc";
static id_ trust_leaf(id_ trust) {
    id_ c = trust && CFArrayGetCount(trust) > 0 ? CFArrayGetValueAtIndex(trust, 0) : 0;
    return c && CFGetTypeID(c) == CFDataGetTypeID() ? c : 0;
}
static void trust_exc_clear(id_ t) { for (int i = 0; t && i < 64; i++) if (g_excepted[i] == t) g_excepted[i] = 0; }
static int trust_excepted(id_ t) { for (int i = 0; t && i < 64; i++) if (g_excepted[i] == t) return 1; return 0; }
id_ SecTrustCopyExceptions(id_ trust) {
    id_ leaf = trust_leaf(trust);
    if (!leaf) return 0;
    long n = CFDataGetLength(leaf);
    id_ d = CFDataCreateMutable(0, 0);
    CFDataAppendBytes(d, (const u8 *)trust_exc_tag, (long)sizeof trust_exc_tag);
    CFDataAppendBytes(d, CFDataGetBytePtr(leaf), n);
    return d;
}
Boolean SecTrustSetExceptions(id_ trust, id_ exceptions) {
    if (!trust) return 0;
    trust_exc_clear(trust);
    ts_lock(); ts_dirty(ts_get(trust, 0)); ts_unlock();
    if (!exceptions) return 0;
    id_ leaf = trust_leaf(trust);
    if (!leaf || CFGetTypeID(exceptions) != CFDataGetTypeID()) return 0;
    long n = CFDataGetLength(leaf), m = CFDataGetLength(exceptions);
    const u8 *e = CFDataGetBytePtr(exceptions);
    if (m != n + (long)sizeof trust_exc_tag || memcmp(e, trust_exc_tag, sizeof trust_exc_tag) ||
        memcmp(e + sizeof trust_exc_tag, CFDataGetBytePtr(leaf), (size_t)n))
        return 0;
    g_excepted[g_nexcepted++ % 64] = trust;
    return 1;
}
static void trust_unmark(id_ t) {
    trust_exc_clear(t);
    for (int i = 0; i < 64; i++) if (g_verified[i] == t) g_verified[i] = 0;
    ts_lock();
    tstate_t *st = ts_get(t, 0);
    if (st) {
        if (st->anchors) CFRelease(st->anchors);
        if (st->policies) CFRelease(st->policies);
        for (size_t i = 0; i < sizeof *st; i++) ((u8 *)st)[i] = 0;
    }
    ts_unlock();
}
static int trust_verified(id_ t) { for (int i = 0; t && i < 64; i++) if (g_verified[i] == t) return 1; return 0; }
extern int cx_sectrust_eval(const u8 *const *ders, const u32 *lens, unsigned n, const u8 *const *anc, const u32 *alens,
                            unsigned na, int use_system, int policy, const char *host, long long at_unix, char *err,
                            unsigned errlen);
enum { X_OK = 0, X_PARSE, X_UNKNOWN_ISSUER, X_SELF_SIGNED, X_UNTRUSTED_ROOT, X_EXPIRED, X_NOT_YET_VALID, X_BAD_SIG,
       X_UNSUPPORTED, X_NOT_CA, X_PATHLEN, X_NAME, X_KEY_USAGE, X_WEAK_KEY, X_CRITICAL_EXT, X_TOO_LONG, X_NO_MEM };
#define errSecNotTrusted (-67843)
#define errSecCertificateExpired (-67818)
#define errSecCertificateNotValidYet (-67819)
#define errSecHostNameMismatch (-67602)
#define errSecInvalidSignature (-67688)
#define errSecInvalidExtendedKeyUsage (-67609)
#define errSecUnknownCriticalExtensionFlag (-67603)
static OSStatus xerr_status(int e) {
    switch (e) {
    case X_EXPIRED: return errSecCertificateExpired;
    case X_NOT_YET_VALID: return errSecCertificateNotValidYet;
    case X_NAME: return errSecHostNameMismatch;
    case X_BAD_SIG: return errSecInvalidSignature;
    case X_KEY_USAGE: return errSecInvalidExtendedKeyUsage;
    case X_CRITICAL_EXT: return errSecUnknownCriticalExtensionFlag;
    default: return errSecNotTrusted;
    }
}
int cx_ts_anchors(const u8 **ders, u32 *lens, int max, u8 ***own);
int cx_ts_denied(const u8 *der, u32 n);
/* avalia e guarda o resultado (kSecTrustResult*) no estado */
static void trust_run(id_ trust, tstate_t *snap) {
    long n = trust ? CFArrayGetCount(trust) : 0;
    snap->result = kSecTrustResultRecoverableTrustFailure;
    snap->xerr = X_UNKNOWN_ISSUER;
    snap->msg[0] = 0;
    if (trust_verified(trust)) { snap->result = kSecTrustResultUnspecified; snap->xerr = 0; return; }
    if (n <= 0 || n > 10) { snap->result = kSecTrustResultInvalid; snap->xerr = X_PARSE; return; }
    const u8 *ders[10]; u32 lens[10];
    for (long i = 0; i < n; i++) {
        id_ c = CFArrayGetValueAtIndex(trust, i);
        if (!c || CFGetTypeID(c) != CFDataGetTypeID() || CFDataGetLength(c) < 4) { snap->result = kSecTrustResultInvalid; snap->xerr = X_PARSE; return; }
        ders[i] = CFDataGetBytePtr(c);
        lens[i] = (u32)CFDataGetLength(c);
    }
    const u8 *anc[64]; u32 alens[64];
    unsigned na = 0;
    if (snap->anchors) {
        long m = CFGetTypeID(snap->anchors) == CFArrayGetTypeID() ? CFArrayGetCount(snap->anchors) : 0;
        for (long i = 0; i < m && na < 64; i++) {
            id_ c = CFArrayGetValueAtIndex(snap->anchors, i);
            if (c && CFGetTypeID(c) == CFDataGetTypeID()) { anc[na] = CFDataGetBytePtr(c); alens[na] = (u32)CFDataGetLength(c); na++; }
        }
    }
    /* âncoras e negações do usuário/admin (SecTrustSettings), exceto com "só minhas âncoras" */
    u8 **own = 0;
    int nown = 0;
    if (!snap->anchors_only) {
        nown = cx_ts_anchors(anc + na, alens + na, (int)(64 - na), &own);
        na += (unsigned)nown;
    }
    for (long i = 0; i < n; i++)
        if (cx_ts_denied(ders[i], lens[i])) {
            for (int j = 0; j < nown; j++) free(own[j]);
            free(own);
            snap->result = kSecTrustResultDeny;
            snap->xerr = X_UNTRUSTED_ROOT;
            cx_snprintf(snap->msg, sizeof snap->msg, "certificado marcado como \"Nunca confiar\" nos ajustes de confiança");
            return;
        }
    long long at = snap->has_date ? (long long)(snap->date + 978307200.0) : -1;
    int e = cx_sectrust_eval(ders, lens, (unsigned)n, anc, alens, na, !snap->anchors_only, snap->policy,
                             snap->host[0] ? snap->host : 0, at, snap->msg, sizeof snap->msg);
    for (int j = 0; j < nown; j++) free(own[j]);
    free(own);
    snap->xerr = e;
    snap->result = e == X_OK ? kSecTrustResultUnspecified
                 : e == X_PARSE || e == X_NO_MEM ? kSecTrustResultFatalTrustFailure : kSecTrustResultRecoverableTrustFailure;
}
static void trust_eval_cached(id_ trust, u32 *result, int *xerr, char *msg, size_t mn) {
    tstate_t snap;
    ts_lock();
    tstate_t *st = ts_get(trust, 1);
    snap = *st;
    if (snap.anchors) CFRetain(snap.anchors);
    ts_unlock();
    if (!snap.evaluated) trust_run(trust, &snap);
    if (snap.result != kSecTrustResultUnspecified && trust_excepted(trust)) { snap.result = kSecTrustResultProceed; snap.xerr = 0; }
    ts_lock();
    st = ts_get(trust, 1);
    st->evaluated = 1; st->result = snap.result; st->xerr = snap.xerr;
    memcpy(st->msg, snap.msg, sizeof st->msg);
    ts_unlock();
    if (snap.anchors) CFRelease(snap.anchors);
    if (result) *result = snap.result;
    if (xerr) *xerr = snap.xerr;
    if (msg && mn) { size_t i = 0; for (; i + 1 < mn && snap.msg[i]; i++) msg[i] = snap.msg[i]; msg[i] = 0; }
}
OSStatus SecTrustEvaluate(id_ trust, u32 *result) {
    if (!trust) return errSecParam;
    ts_lock(); ts_dirty(ts_get(trust, 1)); ts_unlock();
    trust_eval_cached(trust, result, 0, 0, 0);
    return errSecSuccess;
}
OSStatus SecTrustGetTrustResult(id_ trust, u32 *result) {
    if (!trust || !result) return errSecParam;
    ts_lock(); tstate_t *st = ts_get(trust, 0); int ev = st && st->evaluated; u32 r = ev ? st->result : 0; ts_unlock();
    *result = ev ? r : kSecTrustResultInvalid;
    return errSecSuccess;
}
id_ CFErrorCreate(void *alloc, id_ domain, long code, id_ info);
extern const void *kCFErrorDomainOSStatus, *kCFErrorLocalizedDescriptionKey;
static id_ trust_error(int xerr, const char *msg) {
    id_ info = CFDictionaryCreateMutable(0, 0, 0, 0);
    id_ m = str_n(msg && msg[0] ? msg : "certificado não confiável", strlen(msg && msg[0] ? msg : "certificado não confiável"));
    CFDictionarySetValue(info, K_(kCFErrorLocalizedDescriptionKey), m);
    CFRelease(m);
    id_ e = CFErrorCreate(0, K_(kCFErrorDomainOSStatus), xerr_status(xerr), info);
    CFRelease(info);
    return e;
}
Boolean SecTrustEvaluateWithError(id_ trust, id_ *err) {
    if (err) *err = 0;
    if (!trust) return 0;
    u32 result = 0;
    int xerr = 0;
    char msg[160];
    ts_lock(); ts_dirty(ts_get(trust, 1)); ts_unlock();
    trust_eval_cached(trust, &result, &xerr, msg, sizeof msg);
    int ok = result == kSecTrustResultUnspecified || result == kSecTrustResultProceed;
    if (!ok && err) *err = trust_error(xerr, msg);
    return (Boolean)ok;
}
extern const void *kSecTrustResultValue, *kSecTrustEvaluationDate, *kSecPropertyKeyType, *kSecPropertyKeyValue,
    *kSecPropertyTypeError;
id_ cx_num_i(long long v);
id_ SecTrustCopyResult(id_ trust) {
    if (!trust) return 0;
    u32 r = 0;
    SecTrustGetTrustResult(trust, &r);
    if (r == kSecTrustResultInvalid) trust_eval_cached(trust, &r, 0, 0, 0);
    id_ d = CFDictionaryCreateMutable(0, 0, 0, 0);
    id_ n = cx_num_i(r);
    CFDictionarySetValue(d, K_(kSecTrustResultValue), n);
    objc_release(n);
    id_ dt = CFDateCreate(0, CFAbsoluteTimeGetCurrent());
    CFDictionarySetValue(d, K_(kSecTrustEvaluationDate), dt);
    CFRelease(dt);
    return d;
}
id_ SecTrustCopyProperties(id_ trust) {
    if (!trust) return 0;
    u32 r = 0;
    char msg[160];
    trust_eval_cached(trust, &r, 0, msg, sizeof msg);
    id_ a = CFArrayCreateMutable(0, 0, 0);
    if (r != kSecTrustResultUnspecified && r != kSecTrustResultProceed) {
        id_ d = CFDictionaryCreateMutable(0, 0, 0, 0);
        id_ m = str_n(msg, strlen(msg));
        CFDictionarySetValue(d, K_(kSecPropertyKeyType), K_(kSecPropertyTypeError));
        CFDictionarySetValue(d, K_(kSecPropertyKeyValue), m);
        CFRelease(m);
        CFArrayAppendValue(a, d);
        CFRelease(d);
    }
    return a;
}
/* assíncrono: avalia na fila pedida e chama o bloco lá (como o trustd, mas sem processo à parte) */
void *_Block_copy(const void *b);
void _Block_release(const void *b);
void dispatch_async_f(void *queue, void *ctx, void (*fn)(void *));
typedef struct { void *isa; int flags, reserved; void *invoke; } blk_t;
typedef struct { id_ trust; void *block; int with_error; } async_t;
static void trust_async_run(void *p) {
    async_t *a = p;
    blk_t *b = a->block;
    if (a->with_error) {
        id_ err = 0;
        Boolean ok = SecTrustEvaluateWithError(a->trust, &err);
        ((void (*)(void *, id_, Boolean, id_))b->invoke)(b, a->trust, ok, err);
        if (err) CFRelease(err);
    } else {
        u32 r = 0;
        SecTrustEvaluate(a->trust, &r);
        ((void (*)(void *, id_, u32))b->invoke)(b, a->trust, r);
    }
    _Block_release(a->block);
    CFRelease(a->trust);
    free(a);
}
static OSStatus trust_async(id_ trust, void *queue, void *block, int with_error) {
    if (!trust || !queue || !block) return errSecParam;
    async_t *a = malloc(sizeof *a);
    if (!a) return errSecAllocate;
    a->trust = CFRetain(trust);
    a->block = _Block_copy(block);
    a->with_error = with_error;
    dispatch_async_f(queue, a, trust_async_run);
    return errSecSuccess;
}
OSStatus SecTrustEvaluateAsync(id_ trust, void *queue, void *block) { return trust_async(trust, queue, block, 0); }
OSStatus SecTrustEvaluateAsyncWithError(id_ trust, void *queue, void *block) { return trust_async(trust, queue, block, 1); }
id_ SecTrustCopyKey(id_ trust);
id_ SecTrustCopyPublicKey(id_ trust);

/* ================================================================ chaves, identidades, PKCS#12, transforms */
/* SecKey*: seckey.c (núcleo em seckey_core.inc) */
static kitem_t *identity_from_ref(id_ ref) {
    kitem_t *it = item_from_ref(ref);
    return it && is_identity_class(it->cls) ? it : 0;
}
OSStatus SecIdentityCreateWithCertificate(void *kc, id_ cert, id_ *ident) {
    (void)kc;
    if (ident) *ident = 0;
    if (!cert || CFGetTypeID(cert) != CFDataGetTypeID()) return errSecParam;
    const u8 *b = CFDataGetBytePtr(cert);
    long n = CFDataGetLength(cert);
    if (!b || n < 2 || b[0] != 0x30) return errSecParam;
    lock(); load();
    kitem_t *found = 0;
    for (int i = 0; i < nitems; i++) {
        kitem_t *it = &items[i];
        if (!it->used || !is_identity_class(it->cls)) continue;
        if (it->dlen == (u32)n && it->data && !memcmp(it->data, b, (size_t)n)) { found = it; break; }
    }
    id_ ref = found ? item_ref(found) : 0;
    unlock();
    if (!found) return errSecItemNotFound;
    if (ident) *ident = ref;
    else if (ref) CFRelease(ref);
    return errSecSuccess;
}
OSStatus SecIdentityCopyCertificate(id_ ident, id_ *cert) {
    if (cert) *cert = 0;
    lock(); load();
    kitem_t *it = identity_from_ref(ident);
    id_ c = it && it->data && it->dlen ? cx_data_new(it->data, it->dlen) : 0;
    unlock();
    if (!it) return errSecParam;
    if (!c) return errSecItemNotFound;
    if (cert) *cert = c;
    else CFRelease(c);
    return errSecSuccess;
}
/* A chave privada são os bytes de kSecAttrGeneric (PKCS#1, PKCS#8, SEC1 ou X9.63); vira um SecKey de seckey.c. */
id_ cx_seckey_import(const u8 *der, u32 n, int priv);
#define errSecInvalidKeyRef (-67712)
OSStatus SecIdentityCopyPrivateKey(id_ ident, id_ *key) {
    if (key) *key = 0;
    lock(); load();
    kitem_t *it = identity_from_ref(ident);
    int has = it && it->f[F_GENA] && it->fl[F_GENA];
    u32 n = has ? it->fl[F_GENA] : 0;
    u8 *copy = has ? malloc(n) : 0;
    if (copy) memcpy(copy, it->f[F_GENA], n);
    unlock();
    if (!it) return errSecParam;
    if (!copy) return errSecItemNotFound;
    id_ k = cx_seckey_import(copy, n, 1);
    for (u32 i = 0; i < n; i++) copy[i] = 0;
    free(copy);
    if (!k) return errSecInvalidKeyRef;
    if (key) *key = k;
    else objc_release(k);
    return errSecSuccess;
}
id_ SecIdentityCopyPreferred(id_ name, id_ usage, id_ names) { (void)name; (void)usage; (void)names; return 0; }
OSStatus SecIdentitySetPreferred(id_ ident, id_ name, id_ usage) { (void)ident; (void)name; (void)usage; return errSecUnimplemented; }
/* SecPKCS12Import, SecItemImport e SecItemExport: seccert.c */
id_ SecDigestTransformCreate(id_ type, long len, id_ *err) { (void)type; (void)len; if (err) *err = 0; return 0; }
id_ SecSignTransformCreate(id_ key, id_ *err) { (void)key; if (err) *err = 0; return 0; }
id_ SecVerifyTransformCreate(id_ key, id_ sig, id_ *err) { (void)key; (void)sig; if (err) *err = 0; return 0; }
id_ SecTransformCreateGroupTransform(void) { return 0; }
id_ SecTransformCreateReadTransformWithReadStream(id_ stream) { (void)stream; return 0; }
id_ SecTransformConnectTransforms(id_ a, id_ an, id_ b, id_ bn, id_ grp, id_ *err) { (void)a; (void)an; (void)b; (void)bn; (void)grp; if (err) *err = 0; return 0; }

/* SecEncodeTransform/SecDecodeTransform em Base64 e Base32 (RFC 4648). O transform é um CFMutableDictionary com os
 * atributos (INPUT, EncodeType/DecodeType) e a marca "mOS.transform"; SecTransformExecute roda a conversão sobre o
 * CFData de INPUT. Outros tipos (zlib) e os transforms de digest/assinatura devolvem NULL com o CFError. */
id_ CFErrorCreate(void *alloc, id_ domain, long code, id_ info);
static cfconst_t tr_marca_cs = {cx_cfconst_isa, 0x7c8, "mOS.transform", 13};
static cfconst_t tr_dom_cs = {cx_cfconst_isa, 0x7c8, "com.apple.security.transforms.error", 35};
static void tr_erro(id_ *err, long code) { if (err) *err = CFErrorCreate(0, (id_)&tr_dom_cs, code, 0); }
static int tr_tipo(id_ t) {                             /* 64, 32 ou 0 */
    if (t && CFEqual(t, K_(kSecBase64Encoding))) return 64;
    if (t && CFEqual(t, K_(kSecBase32Encoding))) return 32;
    return 0;
}
static id_ tr_novo(id_ attr, id_ type, id_ *err) {
    if (err) *err = 0;
    if (!tr_tipo(type)) { tr_erro(err, errSecUnimplemented); return 0; }
    id_ d = CFDictionaryCreateMutable(0, 0, 0, 0);
    CFDictionarySetValue(d, (id_)&tr_marca_cs, attr);
    CFDictionarySetValue(d, attr, type);
    return d;
}
u64 CFDictionaryGetTypeID(void);
u64 CFArrayGetTypeID(void);
#define errSecDecode (-26275)
static int tr_eh(id_ t) { return t && CFGetTypeID(t) == CFDictionaryGetTypeID() && CFDictionaryGetValue(t, (id_)&tr_marca_cs) != 0; }
id_ SecEncodeTransformCreate(id_ type, id_ *err) { return tr_novo(K_(kSecEncodeTypeAttribute), type, err); }
id_ SecDecodeTransformCreate(id_ type, id_ *err) { return tr_novo(K_(kSecDecodeTypeAttribute), type, err); }
Boolean SecTransformSetAttribute(id_ t, id_ k, id_ v, id_ *err) {
    if (err) *err = 0;
    if (!tr_eh(t) || !k) { tr_erro(err, errSecParam); return 0; }
    CFDictionarySetValue(t, k, v);
    return 1;
}
static const char b64a[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
static const char b32a[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
static int dval(const char *alfa, int c) {
    for (int i = 0; alfa[i]; i++) if (alfa[i] == c) return i;
    if (alfa == b32a && c >= 'a' && c <= 'z') return c - 'a';
    return -1;
}
static id_ tr_codifica(const u8 *in, long n, int bits) {
    id_ out = CFDataCreateMutable(0, 0);
    const char *alfa = bits == 64 ? b64a : b32a;
    int w = bits == 64 ? 6 : 5, grupo = bits == 64 ? 4 : 8;
    unsigned long acc = 0;
    int nb = 0;
    long emitidos = 0;
    for (long i = 0; i < n; i++) {
        acc = acc << 8 | in[i];
        nb += 8;
        while (nb >= w) { u8 c = (u8)alfa[(acc >> (nb - w)) & ((1u << w) - 1)]; CFDataAppendBytes(out, &c, 1); emitidos++; nb -= w; }
    }
    if (nb) { u8 c = (u8)alfa[(acc << (w - nb)) & ((1u << w) - 1)]; CFDataAppendBytes(out, &c, 1); emitidos++; }
    while (emitidos % grupo) { u8 c = '='; CFDataAppendBytes(out, &c, 1); emitidos++; }
    return out;
}
static id_ tr_decodifica(const u8 *in, long n, int bits) {
    id_ out = CFDataCreateMutable(0, 0);
    const char *alfa = bits == 64 ? b64a : b32a;
    int w = bits == 64 ? 6 : 5;
    unsigned long acc = 0;
    int nb = 0;
    for (long i = 0; i < n; i++) {
        int c = in[i];
        if (c == '=') break;
        if (c == ' ' || c == '\n' || c == '\r' || c == '\t') continue;
        int v = dval(alfa, c);
        if (v < 0) { CFRelease(out); return 0; }
        acc = acc << w | (unsigned)v;
        nb += w;
        if (nb >= 8) { u8 b = (u8)(acc >> (nb - 8)); CFDataAppendBytes(out, &b, 1); nb -= 8; }
    }
    return out;
}
id_ SecTransformExecute(id_ t, id_ *err) {
    if (err) *err = 0;
    if (!tr_eh(t)) { tr_erro(err, errSecParam); return 0; }
    id_ attr = CFDictionaryGetValue(t, (id_)&tr_marca_cs);
    int bits = tr_tipo(CFDictionaryGetValue(t, attr));
    id_ in = CFDictionaryGetValue(t, K_(kSecTransformInputAttributeName));
    if (!in || CFGetTypeID(in) != CFDataGetTypeID()) { tr_erro(err, errSecParam); return 0; }
    const u8 *b = CFDataGetBytePtr(in);
    long n = CFDataGetLength(in);
    id_ r = CFEqual(attr, K_(kSecEncodeTypeAttribute)) ? tr_codifica(b, n, bits) : tr_decodifica(b, n, bits);
    if (!r) tr_erro(err, errSecDecode);
    return r;
}

/* ================================================================ sec_protocol (Network.framework)
 * O SecureTransport (SSL*) e as raízes do sistema (SecTrustSettingsCopy*) estão em securetransport.c. */
/* sec_trust_copy_ref, sec_protocol_options_* e sec_protocol_metadata_*: nwnet.c, com o Network.framework (Patricia) */

/* ================================================================ Authorization (sem elevação de privilégio) */
OSStatus AuthorizationCreate(const void *rights, const void *env, u32 flags, void **auth) {
    (void)env; (void)flags;
    if (auth) *auth = calloc(1, 16);                  /* referência opaca */
    /* pedir direitos já na criação (kAuthorizationFlagExtendRights com itens) é negado */
    if (rights && *(const u32 *)rights && (flags & 2)) return errAuthorizationDenied;
    return errAuthorizationSuccess;
}
OSStatus AuthorizationFree(void *auth, u32 flags) { (void)flags; free(auth); return errAuthorizationSuccess; }
OSStatus AuthorizationCopyRights(void *auth, const void *rights, const void *env, u32 flags, void **out) {
    (void)auth; (void)rights; (void)env; (void)flags;
    if (out) *out = 0;
    return errAuthorizationDenied;
}

/* ================================================================ mensagens de erro */
id_ SecCopyErrorMessageString(OSStatus st, void *reserved) {
    (void)reserved;
    const char *m;
    switch (st) {
    case errSecSuccess: m = "No error."; break;
    case errSecUnimplemented: m = "Function or operation not implemented."; break;
    case errSecParam: m = "One or more parameters passed to a function were not valid."; break;
    case errSecAllocate: m = "Failed to allocate memory."; break;
    case errSecNotAvailable: m = "No keychain is available."; break;
    case errSecDuplicateItem: m = "The specified item already exists in the keychain."; break;
    case errSecItemNotFound: m = "The specified item could not be found in the keychain."; break;
    case errSecNoTrustSettings: m = "No Trust Settings were found."; break;
    case errSecInteractionNotAllowed: m = "User interaction is not allowed."; break;
    case errSecCSUnsigned: m = "code object is not signed at all"; break;
    case errAuthorizationDenied: m = "The authorization was denied."; break;
    default: {
        char b[48];
        int n = cx_snprintf(b, sizeof b, "OSStatus %d", st);
        return str_n(b, (u64)n);
    }
    }
    return str_n(m, strlen(m));
}
