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
 *    AddGenericPassword/ItemModifyAttributesAndData/ItemDelete/ItemFreeContent. Os itens ficam em
 *    $HOME/Library/Keychains/mos-keychain.txt. A versão 2 do arquivo é ciphertext (HMAC em modo
 *    contador + MAC). A chave sai do nome de login, não de uma senha: ver kcseal.inc.
 *    A versão 1 (texto) ainda é lida e, na gravação seguinte, vira versão 2.
 *  - SecCertificateCreateWithData/CopyData (o certificado é o DER num CFData), SecCopyErrorMessageString.
 * O que responde "indisponível" com o erro que a API documenta (regra de chrome.c: nunca 0/sucesso sem preencher):
 *  - assinatura de código: o mOS não verifica assinaturas; todo código é "não assinado" (errSecCSUnsigned). As
 *    funções que só criam objetos (SecCodeCopySelf, SecStaticCodeCreateWithPath, SecRequirementCreateWithString)
 *    funcionam, e SecCodeCopySigningInformation devolve um dicionário só com o identificador (como o macOS faz
 *    para código não assinado). O rendezvous do Chromium está em modo "só validar", então isso não o bloqueia.
 *  - SecTrust: avaliação devolve "não confiável"; o Chromium/Electron usa o próprio verificador (Chrome Root
 *    Store). As confianças do SecureTransport (securetransport.c), já validadas pelo kernel, avaliam como válidas.
 *  - SecKey, SecIdentity, PKCS12 e transforms: errSecUnimplemented / NULL. O SecureTransport é real: securetransport.c.
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
enum { F_SVCE, F_ACCT, F_LABL, F_DESC, F_GENA, F_SRVR, F_AGRP, NFIELD };
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
    field_key[F_AGRP] = &kSecAttrAccessGroup;
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
        char login[96];
        u8 *plain = 0;
        u32 pn = 0;
        kc_login(login, sizeof login);
        kc_key_from(login, key);
        int r = kc_open(key, (const u8 *)buf, (u32)len, &plain, &pn);
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
            for (int i = 0; ok && i < NFIELD; i++) ok = get_hex(&q, eol, &it->f[i], &it->fl[i]);
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
    char login[96];
    u8 *sealed = 0;
    u32 sn = 0;
    kc_login(login, sizeof login);
    kc_key_from(login, key);
    int sealed_ok = kc_seal(key, (const u8 *)out, (u32)plain_n, &sealed, &sn);
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
    if (rename(tmp, path) != 0) { unlink(path); if (rename(tmp, path) != 0) return errSecNotAvailable; }
    return errSecSuccess;
}

/* ---- consulta: classe + atributos de um dicionário CF */
typedef struct {
    char cls[5];
    id_ f[NFIELD];          /* valores pedidos (NSString ou NSData), NULL = não filtra */
} query_t;
/* bytes de um valor de atributo: CFString (UTF-8) ou CFData */
static const u8 *val_bytes(id_ v, u32 *n) {
    if (!v) { *n = 0; return 0; }
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
}
static int is_password_class(const char *c) { return !strcmp(c, "genp") || !strcmp(c, "inet"); }
static int matches(kitem_t *it, query_t *x) {
    if (!it->used || strcmp(it->cls, x->cls)) return 0;
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

static id_ item_attrs(kitem_t *it, int with_data, int with_ref) {
    id_ d = CFDictionaryCreateMutable(0, 0, 0, 0);
    id_ c = str_n(it->cls, 4);
    CFDictionarySetValue(d, K_(kSecClass), c);
    CFRelease(c);
    for (int i = 0; i < NFIELD; i++) {
        if (!it->f[i]) continue;
        /* gena (genérico) é binário; os demais são texto */
        id_ v = i == F_GENA ? cx_data_new(it->f[i], it->fl[i]) : str_n((const char *)it->f[i], it->fl[i]);
        if (v) { CFDictionarySetValue(d, (id_)*field_key[i], v); CFRelease(v); }
    }
    id_ cd = CFDateCreate(0, it->cdat), md = CFDateCreate(0, it->mdat);
    if (cd) { CFDictionarySetValue(d, K_(kSecAttrCreationDate), cd); CFRelease(cd); }
    if (md) { CFDictionarySetValue(d, K_(kSecAttrModificationDate), md); CFRelease(md); }
    if (with_data) { id_ v = cx_data_new(it->data, it->dlen); CFDictionarySetValue(d, K_(kSecValueData), v); CFRelease(v); }
    if (with_ref) { id_ r = item_ref(it); CFDictionarySetValue(d, K_(kSecValueRef), r); CFRelease(r); }
    return d;
}
static int flag(id_ q, const void *key) { id_ v = CFDictionaryGetValue(q, K_(key)); return v && CFBooleanGetValue(v); }

OSStatus SecItemCopyMatching(id_ query, id_ *result) {
    if (result) *result = 0;
    query_t x;
    lock(); load();
    parse_query(query, &x);
    if (!x.cls[0]) { unlock(); return errSecParam; }
    if (!is_password_class(x.cls)) { unlock(); return errSecItemNotFound; }   /* sem chaves/certificados guardados */
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
        else if (rr || rp) r = item_ref(it);
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
    if (!is_password_class(x.cls)) { unlock(); return errSecUnimplemented; }   /* chaves/certificados: ainda não */
    for (int i = 0; i < nitems; i++) if (same_identity(&items[i], x.cls, &x)) { unlock(); return errSecDuplicateItem; }
    kitem_t *it = new_item();
    if (!it) { unlock(); return errSecAllocate; }
    memcpy(it->cls, x.cls, 5);
    it->id = next_id++;
    it->cdat = it->mdat = CFAbsoluteTimeGetCurrent();
    for (int i = 0; i < NFIELD; i++) if (x.f[i]) { u32 n; const u8 *b = val_bytes(x.f[i], &n); set_field(it, i, b, n); }
    u32 n = 0;
    const u8 *b = val_bytes(CFDictionaryGetValue(attrs, K_(kSecValueData)), &n);
    set_data(it, b, n);
    OSStatus st = save();
    if (st == errSecSuccess && result) {
        int rd = flag(attrs, kSecReturnData), ra = flag(attrs, kSecReturnAttributes), rr = flag(attrs, kSecReturnRef);
        if (ra) *result = item_attrs(it, rd, rr);
        else if (rd) *result = cx_data_new(it->data, it->dlen);
        else if (rr) *result = item_ref(it);
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
    if (!is_password_class(x.cls)) { unlock(); return errSecItemNotFound; }
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
    if (!is_password_class(x.cls)) { unlock(); return errSecItemNotFound; }
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

OSStatus SecKeychainFindGenericPassword(void *kc, u32 sl, const char *svc, u32 al, const char *acc, u32 *pl, void **pw, id_ *item) {
    (void)kc;
    if (pl) *pl = 0;
    if (pw) *pw = 0;
    if (item) *item = 0;
    query_t x;
    q_from_c(&x, "genp", svc, sl, acc, al, F_SVCE);
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

OSStatus SecKeychainAddGenericPassword(void *kc, u32 sl, const char *svc, u32 al, const char *acc, u32 pl, const void *pw, id_ *item) {
    (void)kc;
    if (item) *item = 0;
    query_t x;
    q_from_c(&x, "genp", svc, sl, acc, al, F_SVCE);
    lock(); load();
    OSStatus st = errSecSuccess;
    for (int i = 0; i < nitems; i++) if (same_identity(&items[i], "genp", &x)) { st = errSecDuplicateItem; break; }
    kitem_t *it = 0;
    if (st == errSecSuccess && !(it = new_item())) st = errSecAllocate;
    if (st == errSecSuccess) {
        memcpy(it->cls, "genp", 5);
        it->id = next_id++;
        it->cdat = it->mdat = CFAbsoluteTimeGetCurrent();
        if (svc) { set_field(it, F_SVCE, (const u8 *)svc, sl); set_field(it, F_LABL, (const u8 *)svc, sl); }
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

OSStatus SecKeychainItemFreeContent(void *attrs, void *data) { (void)attrs; free(data); return errSecSuccess; }
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
u64 SecKeyGetTypeID(void) { return TID_SEC_KEY; }
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
id_ SecCertificateCopySubjectSummary(id_ cert) { (void)cert; return 0; }
OSStatus SecCertificateCopyCommonName(id_ cert, id_ *cn) { (void)cert; if (cn) *cn = 0; return errSecUnimplemented; }
id_ SecCertificateCopyKey(id_ cert) { (void)cert; return 0; }
id_ SecCertificateCopyNormalizedIssuerSequence(id_ cert) { (void)cert; return 0; }
id_ SecCertificateCopySerialNumberData(id_ cert, id_ *err) { (void)cert; if (err) *err = 0; return 0; }
id_ SecCertificateCopyValues(id_ cert, id_ keys, id_ *err) { (void)cert; (void)keys; if (err) *err = 0; return 0; }

id_ SecPolicyCreateSSL(Boolean server, id_ host) { (void)server; (void)host; return str_n("mOS-policy:ssl", 14); }
id_ SecPolicyCreateBasicX509(void) { return str_n("mOS-policy:x509", 15); }
id_ SecPolicyCreateRevocation(u64 flags) { (void)flags; return str_n("mOS-policy:revocation", 21); }
id_ SecPolicyCopyProperties(id_ policy) { (void)policy; return CFDictionaryCreateMutable(0, 0, 0, 0); }

/* SecTrust: guarda os certificados; a avaliação diz "não confiável" (o mOS não tem raízes do sistema) */
typedef struct { id_ certs; } trust_box_t;
static void trust_unmark(id_ t);
OSStatus SecTrustCreateWithCertificates(id_ certs, id_ policies, id_ *trust) {
    (void)policies;
    if (!trust) return errSecParam;
    *trust = 0;
    if (!certs) return errSecParam;
    id_ arr = CFArrayCreateMutable(0, 0, 0);
    if (CFGetTypeID(certs) == CFDataGetTypeID()) CFArrayAppendValue(arr, certs);   /* um certificado só */
    else for (long i = 0; i < CFArrayGetCount(certs); i++) CFArrayAppendValue(arr, CFArrayGetValueAtIndex(certs, i));
    trust_unmark(arr);                                /* endereço reaproveitado de uma confiança antiga */
    *trust = arr;
    return errSecSuccess;
}
long SecTrustGetCertificateCount(id_ trust) { return trust ? CFArrayGetCount(trust) : 0; }
id_ SecTrustGetCertificateAtIndex(id_ trust, long i) { return trust && i >= 0 && i < CFArrayGetCount(trust) ? CFArrayGetValueAtIndex(trust, i) : 0; }
id_ SecTrustCopyCertificateChain(id_ trust) {
    if (!trust) return 0;
    id_ arr = CFArrayCreateMutable(0, 0, 0);
    for (long i = 0; i < CFArrayGetCount(trust); i++) CFArrayAppendValue(arr, CFArrayGetValueAtIndex(trust, i));
    return arr;
}
id_ SecTrustCopyKey(id_ trust) { (void)trust; return 0; }
id_ SecTrustCopyResult(id_ trust) { (void)trust; return 0; }
OSStatus SecTrustSetPolicies(id_ trust, id_ p) { (void)trust; (void)p; return errSecSuccess; }
OSStatus SecTrustSetAnchorCertificates(id_ trust, id_ a) { (void)trust; (void)a; return errSecSuccess; }
OSStatus SecTrustSetAnchorCertificatesOnly(id_ trust, Boolean only) { (void)trust; (void)only; return errSecSuccess; }
OSStatus SecTrustSetVerifyDate(id_ trust, id_ d) { (void)trust; (void)d; return errSecSuccess; }
OSStatus SecTrustSetNetworkFetchAllowed(id_ trust, Boolean on) { (void)trust; (void)on; return errSecSuccess; }
OSStatus SecTrustSetOCSPResponse(id_ trust, id_ r) { (void)trust; (void)r; return errSecSuccess; }
#define kSecTrustResultUnspecified 4
#define kSecTrustResultRecoverableTrustFailure 5
/* confianças que o SecureTransport devolveu (SSLCopyPeerTrust) depois de o kernel validar a cadeia, as datas e o
 * nome: essas avaliam como válidas; as demais continuam "não confiável" (o mOS não reavalia cadeias aqui) */
static id_ g_verified[64];
static unsigned g_nverified;
void cx_trust_mark_verified(id_ t) { g_verified[g_nverified++ % 64] = t; }
static void trust_unmark(id_ t) { for (int i = 0; i < 64; i++) if (g_verified[i] == t) g_verified[i] = 0; }
static int trust_verified(id_ t) { for (int i = 0; t && i < 64; i++) if (g_verified[i] == t) return 1; return 0; }
OSStatus SecTrustEvaluate(id_ trust, u32 *result) {
    if (result) *result = trust_verified(trust) ? kSecTrustResultUnspecified : kSecTrustResultRecoverableTrustFailure;
    return errSecSuccess;
}
OSStatus SecTrustGetTrustResult(id_ trust, u32 *result) { return SecTrustEvaluate(trust, result); }
Boolean SecTrustEvaluateWithError(id_ trust, id_ *err) { if (err) *err = 0; return trust_verified(trust) ? 1 : 0; }
OSStatus SecTrustEvaluateAsync(id_ trust, void *queue, void *block) { (void)trust; (void)queue; (void)block; return errSecUnimplemented; }
OSStatus SecTrustEvaluateAsyncWithError(id_ trust, void *queue, void *block) { (void)trust; (void)queue; (void)block; return errSecUnimplemented; }

/* ================================================================ chaves, identidades, PKCS#12, transforms */
id_ SecKeyCreateRandomKey(id_ params, id_ *err) { (void)params; if (err) *err = 0; return 0; }
OSStatus SecKeyGeneratePair(id_ params, id_ *pub, id_ *priv) { (void)params; if (pub) *pub = 0; if (priv) *priv = 0; return errSecUnimplemented; }
id_ SecKeyCreateWithData(id_ data, id_ attrs, id_ *err) { (void)data; (void)attrs; if (err) *err = 0; return 0; }
id_ SecKeyCopyPublicKey(id_ key) { (void)key; return 0; }
id_ SecKeyCopyAttributes(id_ key) { (void)key; return 0; }
id_ SecKeyCopyExternalRepresentation(id_ key, id_ *err) { (void)key; if (err) *err = 0; return 0; }
id_ SecKeyCreateSignature(id_ key, id_ alg, id_ data, id_ *err) { (void)key; (void)alg; (void)data; if (err) *err = 0; return 0; }
Boolean SecKeyVerifySignature(id_ key, id_ alg, id_ data, id_ sig, id_ *err) { (void)key; (void)alg; (void)data; (void)sig; if (err) *err = 0; return 0; }
id_ SecKeyCreateEncryptedData(id_ key, id_ alg, id_ data, id_ *err) { (void)key; (void)alg; (void)data; if (err) *err = 0; return 0; }
id_ SecKeyCreateDecryptedData(id_ key, id_ alg, id_ data, id_ *err) { (void)key; (void)alg; (void)data; if (err) *err = 0; return 0; }
id_ SecKeyCopyKeyExchangeResult(id_ key, id_ alg, id_ pub, id_ params, id_ *err) { (void)key; (void)alg; (void)pub; (void)params; if (err) *err = 0; return 0; }
Boolean SecKeyIsAlgorithmSupported(id_ key, long op, id_ alg) { (void)key; (void)op; (void)alg; return 0; }
size_t SecKeyGetBlockSize(id_ key) { (void)key; return 0; }
OSStatus SecKeyRawSign(id_ k, u32 pad, const u8 *d, size_t dl, u8 *sig, size_t *sl) { (void)k; (void)pad; (void)d; (void)dl; (void)sig; if (sl) *sl = 0; return errSecUnimplemented; }
OSStatus SecKeyRawVerify(id_ k, u32 pad, const u8 *d, size_t dl, const u8 *sig, size_t sl) { (void)k; (void)pad; (void)d; (void)dl; (void)sig; (void)sl; return errSecUnimplemented; }
OSStatus SecKeyEncrypt(id_ k, u32 pad, const u8 *p, size_t pl, u8 *c, size_t *cl) { (void)k; (void)pad; (void)p; (void)pl; (void)c; if (cl) *cl = 0; return errSecUnimplemented; }
OSStatus SecKeyDecrypt(id_ k, u32 pad, const u8 *c, size_t cl, u8 *p, size_t *pl) { (void)k; (void)pad; (void)c; (void)cl; (void)p; if (pl) *pl = 0; return errSecUnimplemented; }
OSStatus SecIdentityCopyCertificate(id_ ident, id_ *cert) { (void)ident; if (cert) *cert = 0; return errSecUnimplemented; }
OSStatus SecIdentityCopyPrivateKey(id_ ident, id_ *key) { (void)ident; if (key) *key = 0; return errSecUnimplemented; }
id_ SecIdentityCopyPreferred(id_ name, id_ usage, id_ names) { (void)name; (void)usage; (void)names; return 0; }
OSStatus SecIdentitySetPreferred(id_ ident, id_ name, id_ usage) { (void)ident; (void)name; (void)usage; return errSecUnimplemented; }
OSStatus SecPKCS12Import(id_ data, id_ opts, id_ *items_) { (void)data; (void)opts; if (items_) *items_ = 0; return errSecUnimplemented; }
OSStatus SecItemImport(id_ data, id_ ext, u32 *fmt, u32 *type, u32 flags, const void *params, void *kc, id_ *out) {
    (void)data; (void)ext; (void)fmt; (void)type; (void)flags; (void)params; (void)kc;
    if (out) *out = 0;
    return errSecUnimplemented;
}
id_ SecDigestTransformCreate(id_ type, long len, id_ *err) { (void)type; (void)len; if (err) *err = 0; return 0; }
id_ SecSignTransformCreate(id_ key, id_ *err) { (void)key; if (err) *err = 0; return 0; }
id_ SecVerifyTransformCreate(id_ key, id_ sig, id_ *err) { (void)key; (void)sig; if (err) *err = 0; return 0; }
id_ SecTransformCreateGroupTransform(void) { return 0; }
id_ SecTransformCreateReadTransformWithReadStream(id_ stream) { (void)stream; return 0; }
id_ SecTransformConnectTransforms(id_ a, id_ an, id_ b, id_ bn, id_ grp, id_ *err) { (void)a; (void)an; (void)b; (void)bn; (void)grp; if (err) *err = 0; return 0; }
Boolean SecTransformSetAttribute(id_ t, id_ k, id_ v, id_ *err) { (void)t; (void)k; (void)v; if (err) *err = 0; return 0; }
id_ SecTransformExecute(id_ t, id_ *err) { (void)t; if (err) *err = 0; return 0; }

/* ================================================================ sec_protocol (Network.framework)
 * O SecureTransport (SSL*) e as raízes do sistema (SecTrustSettingsCopy*) estão em securetransport.c. */
void *sec_trust_copy_ref(void *t) { (void)t; return 0; }
void sec_protocol_options_set_verify_block(void *o, void *b, void *q) { (void)o; (void)b; (void)q; }
void sec_protocol_options_append_tls_ciphersuite(void *o, int s) { (void)o; (void)s; }
void sec_protocol_options_add_pre_shared_key(void *o, void *psk, void *id) { (void)o; (void)psk; (void)id; }

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
