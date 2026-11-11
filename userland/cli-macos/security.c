/* mOS — security(1): o mínimo do /usr/bin/security do macOS que os apps Electron (VS Code, Cursor) e scripts chamam.
 * Fala com o Keychain do mOS (userland/libsystem/security.c) pela API antiga SecKeychain*, a mesma que o
 * security da Apple usa; o binário é Mach-O ligado com -undefined dynamic_lookup e os nomes resolvem no libSystem.
 *   security find-generic-password [-a conta] [-s serviço] [-g] [-w] [keychain...]
 *   security add-generic-password -a conta -s serviço [-w senha] [-U] [-T app] [-A] [keychain]
 *   security delete-generic-password [-a conta] [-s serviço] [keychain...]
 *   security find-internet-password [-a conta] [-s servidor] [-d -p -P -r -t ignorados] [-g] [-w] [keychain...]
 *   security add-internet-password -a conta -s servidor [-w senha] [-U] [keychain]
 *   security delete-internet-password [-a conta] [-s servidor] [keychain...]
 *   security find-certificate [-a] [-c nome] [-p] [keychain...]   raízes do sistema (as do kernel, cacert.pem da Mozilla)
 *   security dump-trust-settings [-s] [-d]
 *   security verify-cert -c arquivo [-r raiz] [-p política] [-q]
 *   security list-keychains | default-keychain | login-keychain [-d domínio] [-s ...]
 *   security show-keychain-info [keychain] | unlock-keychain [-p senha] [keychain] | lock-keychain [-a]
 * Saída e código de saída como os da Apple: o código é o OSStatus em 8 bits (item ausente -25300 -> 44, item
 * duplicado -25299 -> 45), erro de uso -> 2. O mOS tem um Keychain só: o nome de keychain na linha de comando é
 * aceito e ignorado, e os domínios (user/system/common) mostram o mesmo arquivo.
 * Os VS Code/Cursor chamam "find-certificate -a -p" (com e sem SystemRootCertificates.keychain) para juntar as CAs
 * do sistema às do Node; os scripts de credencial chamam find/add-generic-password. verify-cert só aceita um
 * certificado que seja uma raiz do sistema (ou uma -r) byte a byte: o mOS não verifica cadeia aqui e não finge.
 * Teste de host: tools/security-cli-teste.sh (tools/security_cli_test.c, Keychain falso em memória). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int32_t OSStatus;
typedef uint32_t u32;
typedef long CFIndex;
typedef struct { u32 tag; u32 length; void *data; } SecKeychainAttribute;
typedef struct { u32 count; SecKeychainAttribute *attr; } SecKeychainAttributeList;

OSStatus SecKeychainFindGenericPassword(void *kc, u32 sl, const char *svc, u32 al, const char *acc,
                                        u32 *pl, void **pw, void **item);
OSStatus SecKeychainAddGenericPassword(void *kc, u32 sl, const char *svc, u32 al, const char *acc,
                                       u32 pl, const void *pw, void **item);
OSStatus SecKeychainFindInternetPassword(void *kc, u32 srvl, const char *srv, u32 dl, const char *dom, u32 al,
                                         const char *acc, u32 pathl, const char *path, unsigned short port,
                                         u32 proto, u32 auth, u32 *pl, void **pw, void **item);
OSStatus SecKeychainAddInternetPassword(void *kc, u32 srvl, const char *srv, u32 dl, const char *dom, u32 al,
                                        const char *acc, u32 pathl, const char *path, unsigned short port,
                                        u32 proto, u32 auth, u32 pl, const void *pw, void **item);
OSStatus SecKeychainItemModifyAttributesAndData(void *item, const void *attrs, u32 len, const void *data);
OSStatus SecKeychainItemDelete(void *item);
OSStatus SecKeychainItemCopyContent(void *item, u32 *cls, SecKeychainAttributeList *al, u32 *len, void **data);
OSStatus SecKeychainItemFreeContent(SecKeychainAttributeList *al, void *data);
OSStatus SecKeychainGetPath(void *kc, u32 *len, char *path);
OSStatus SecTrustSettingsCopyCertificates(int domain, const void **certs);
void *SecCertificateCopyData(const void *cert);
CFIndex CFArrayGetCount(const void *a);
const void *CFArrayGetValueAtIndex(const void *a, CFIndex i);
CFIndex CFDataGetLength(const void *d);
const uint8_t *CFDataGetBytePtr(const void *d);
void CFRelease(const void *o);

#define errSecParam (-50)
#define errSecAllocate (-108)
#define errSecNoTrustSettings (-25263)
#define errSecDuplicateItem (-25299)
#define errSecItemNotFound (-25300)
#define USO 2
#define FOURCC(a, b, c, d) (((u32)(a) << 24) | ((u32)(b) << 16) | ((u32)(c) << 8) | (u32)(d))

static const char *mensagem(OSStatus st) {
    switch (st) {
    case errSecParam: return "One or more parameters passed to a function were not valid.";
    case errSecAllocate: return "Failed to allocate memory.";
    case errSecNoTrustSettings: return "No Trust Settings were found.";
    case errSecDuplicateItem: return "The specified item already exists in the keychain.";
    case errSecItemNotFound: return "The specified item could not be found in the keychain.";
    case -4: return "Function or operation not implemented.";
    case -25291: return "No keychain is available.";
    case -25308: return "User interaction is not allowed.";
    }
    return "Unknown error.";
}
/* código de saída do security da Apple: o OSStatus truncado em 8 bits */
static int saida(OSStatus st) {
    if (!st) return 0;
    int c = (int)((u32)st & 0xff);
    return c ? c : 1;
}
static int falha(const char *onde, OSStatus st) {
    fprintf(stderr, "security: %s: %s\n", onde, mensagem(st));
    return saida(st);
}

/* ---- getopt próprio (reentrante: o teste de host chama main várias vezes) */
typedef struct { int argc; char **argv; int i; const char *resto; const char *arg; } opcoes_t;
static int opcao(opcoes_t *o, const char *spec) {
    if (!o->resto || !*o->resto) {
        if (o->i >= o->argc) return -1;
        const char *a = o->argv[o->i];
        if (a[0] != '-' || !a[1]) return -1;
        if (!strcmp(a, "--")) { o->i++; return -1; }
        o->resto = a + 1;
        o->i++;
    }
    char c = *o->resto++;
    const char *s = strchr(spec, c);
    if (!s || c == ':') return '?';
    if (s[1] == ':') {
        if (*o->resto) { o->arg = o->resto; o->resto = 0; }
        else if (o->i < o->argc) o->arg = o->argv[o->i++];
        else if (s[2] == ':') o->arg = 0;               /* "::": valor opcional (o -w no fim pede a senha) */
        else return '?';
    }
    return c;
}

static const char *caminho_keychain(void) {
    static char b[1024];
    u32 n = sizeof b;
    if (SecKeychainGetPath(0, &n, b) != 0) return "login.keychain-db";
    return b;
}

/* ---- impressão no formato do security da Apple */
static int imprimivel(const uint8_t *b, u32 n) {
    for (u32 i = 0; i < n; i++) if (b[i] < 0x20 || b[i] > 0x7e) return 0;
    return 1;
}
/* texto ASCII: "texto"; senão 0xHEX  "texto com \ooo" (o \0 final de datas aparece como \000) */
static void blob(FILE *f, const uint8_t *b, u32 n) {
    if (imprimivel(b, n)) { fputc('"', f); fwrite(b, 1, n, f); fputc('"', f); return; }
    fputs("0x", f);
    for (u32 i = 0; i < n; i++) fprintf(f, "%02X", b[i]);
    fputs("  \"", f);
    for (u32 i = 0; i < n; i++) {
        if (b[i] >= 0x20 && b[i] <= 0x7e) fputc(b[i], f);
        else fprintf(f, "\\%03o", b[i]);
    }
    fputc('"', f);
}

typedef struct { u32 tag; const char *nome; const char *tipo; } atributo_t;
static const atributo_t ATR_GENP[] = {
    {7, "0x00000007", "blob"}, {8, "0x00000008", "blob"}, {FOURCC('a','c','c','t'), "acct", "blob"},
    {FOURCC('c','d','a','t'), "cdat", "timedate"}, {FOURCC('c','r','t','r'), "crtr", "uint32"},
    {FOURCC('c','u','s','i'), "cusi", "sint32"}, {FOURCC('d','e','s','c'), "desc", "blob"},
    {FOURCC('g','e','n','a'), "gena", "blob"}, {FOURCC('i','c','m','t'), "icmt", "blob"},
    {FOURCC('i','n','v','i'), "invi", "sint32"}, {FOURCC('m','d','a','t'), "mdat", "timedate"},
    {FOURCC('n','e','g','a'), "nega", "sint32"}, {FOURCC('p','r','o','t'), "prot", "blob"},
    {FOURCC('s','c','r','p'), "scrp", "sint32"}, {FOURCC('s','v','c','e'), "svce", "blob"},
    {FOURCC('t','y','p','e'), "type", "uint32"},
};
static const atributo_t ATR_INET[] = {
    {7, "0x00000007", "blob"}, {8, "0x00000008", "blob"}, {FOURCC('a','c','c','t'), "acct", "blob"},
    {FOURCC('a','t','y','p'), "atyp", "blob"}, {FOURCC('c','d','a','t'), "cdat", "timedate"},
    {FOURCC('c','r','t','r'), "crtr", "uint32"}, {FOURCC('c','u','s','i'), "cusi", "sint32"},
    {FOURCC('d','e','s','c'), "desc", "blob"}, {FOURCC('i','c','m','t'), "icmt", "blob"},
    {FOURCC('i','n','v','i'), "invi", "sint32"}, {FOURCC('m','d','a','t'), "mdat", "timedate"},
    {FOURCC('n','e','g','a'), "nega", "sint32"}, {FOURCC('p','a','t','h'), "path", "blob"},
    {FOURCC('p','o','r','t'), "port", "uint32"}, {FOURCC('p','r','o','t'), "prot", "blob"},
    {FOURCC('p','t','c','l'), "ptcl", "uint32"}, {FOURCC('s','c','r','p'), "scrp", "sint32"},
    {FOURCC('s','d','m','n'), "sdmn", "blob"}, {FOURCC('s','r','v','r'), "srvr", "blob"},
    {FOURCC('t','y','p','e'), "type", "uint32"},
};
#define NATR(a) (sizeof(a) / sizeof((a)[0]))

/* keychain/version/class/attributes de um item; com senha != NULL, devolve o segredo (+tamanho) */
static OSStatus mostra_item(void *item, int inet, void **senha, u32 *slen) {
    const atributo_t *t = inet ? ATR_INET : ATR_GENP;
    u32 n = inet ? (u32)NATR(ATR_INET) : (u32)NATR(ATR_GENP);
    SecKeychainAttribute at[24];
    SecKeychainAttributeList al = {n, at};
    for (u32 i = 0; i < n; i++) { at[i].tag = t[i].tag; at[i].length = 0; at[i].data = 0; }
    u32 cls = 0, len = 0;
    void *data = 0;
    OSStatus st = SecKeychainItemCopyContent(item, &cls, &al, senha ? &len : 0, senha ? &data : 0);
    if (st) return st;
    printf("keychain: \"%s\"\n", caminho_keychain());
    printf("version: 512\n");
    printf("class: \"%c%c%c%c\"\n", (char)(cls >> 24), (char)(cls >> 16), (char)(cls >> 8), (char)cls);
    printf("attributes:\n");
    for (u32 i = 0; i < n; i++) {
        printf("    %s%s%s<%s>=", t[i].nome[0] == '0' ? "" : "\"", t[i].nome, t[i].nome[0] == '0' ? " " : "\"", t[i].tipo);
        if (at[i].data) blob(stdout, at[i].data, at[i].length);
        else if (at[i].tag == FOURCC('p','o','r','t')) printf("0x00000000");
        else printf("<NULL>");
        printf("\n");
    }
    SecKeychainItemFreeContent(&al, 0);
    if (senha) { *senha = data; *slen = len; }
    return 0;
}

/* -w: só a senha (hex se não for texto); -g: "password: ..." em stderr depois dos atributos */
static void imprime_senha_w(const uint8_t *b, u32 n) {
    if (imprimivel(b, n)) fwrite(b, 1, n, stdout);
    else for (u32 i = 0; i < n; i++) printf("%02x", b[i]);
    printf("\n");
}
static void imprime_senha_g(const uint8_t *b, u32 n) {
    fputs("password: ", stderr);
    blob(stderr, b, n);
    fputc('\n', stderr);
}

typedef struct { const char *conta, *servico, *senha; int g, w, atualiza, pede; } item_args_t;

static void *procura(int inet, item_args_t *a, OSStatus *st) {
    void *item = 0;
    const char *s = a->servico, *c = a->conta;
    u32 sl = s ? (u32)strlen(s) : 0, cl = c ? (u32)strlen(c) : 0;
    if (inet) *st = SecKeychainFindInternetPassword(0, sl, s, 0, 0, cl, c, 0, 0, 0, 0, 0, 0, 0, &item);
    else *st = SecKeychainFindGenericPassword(0, sl, s, cl, c, 0, 0, &item);
    return *st ? 0 : item;
}

/* adicionar: -w traz a senha (ou, no fim, pede); na busca, -w só pede a senha na saída */
static int le_opcoes_item(int argc, char **argv, const char *spec, item_args_t *a, int extra_inet, int adicionar) {
    opcoes_t o = {argc, argv, 1, 0, 0};
    int c;
    memset(a, 0, sizeof *a);
    while ((c = opcao(&o, spec)) != -1) {
        switch (c) {
        case 'a': a->conta = o.arg; break;
        case 's': a->servico = o.arg; break;
        case 'g': a->g = 1; break;
        case 'w':
            if (adicionar) { a->senha = o.arg; a->pede = !o.arg; }
            else a->w = 1;
            break;
        case 'U': a->atualiza = 1; break;
        case 'T': case 'A': case 'l': case 'j': case 'D': case 'c': case 'C': case 'G': break;   /* ACL, rótulo, comentário: sem efeito */
        case 'd': case 'p': case 'P': case 'r': case 't':
            if (!extra_inet) return USO;
            break;
        default: return USO;
        }
    }
    return 0;
}

static int cmd_find_password(int argc, char **argv, int inet) {
    item_args_t a;
    if (le_opcoes_item(argc, argv, inet ? "a:s:d:p:P:r:t:gw" : "a:s:gw", &a, inet, 0)) return USO;
    OSStatus st;
    void *item = procura(inet, &a, &st);
    if (!item) return falha("SecKeychainSearchCopyNext", st);
    void *senha = 0;
    u32 n = 0;
    if (a.w) {
        st = SecKeychainItemCopyContent(item, 0, 0, &n, &senha);
        if (!st) imprime_senha_w(senha, n);
    } else {
        st = mostra_item(item, inet, a.g ? &senha : 0, &n);
        if (!st && a.g) imprime_senha_g(senha, n);
    }
    if (senha) SecKeychainItemFreeContent(0, senha);
    CFRelease(item);
    return st ? falha("SecKeychainItemCopyContent", st) : 0;
}

/* -w no fim sem valor: a senha vem da entrada padrão (uma linha), como o prompt do macOS */
static char *le_senha(void) {
    static char b[4096];
    fputs("password data for new item: ", stderr);
    if (!fgets(b, sizeof b, stdin)) return 0;
    b[strcspn(b, "\r\n")] = 0;
    return b;
}

static int cmd_add_password(int argc, char **argv, int inet) {
    item_args_t a;
    if (le_opcoes_item(argc, argv, inet ? "a:s:w::UT:Al:j:D:c:C:d:p:P:r:t:" : "a:s:w::UT:Al:j:D:c:C:G:", &a, inet, 1)) return USO;
    if (!a.servico && !a.conta) return USO;
    const char *pw = a.senha;
    if (a.pede && !(pw = le_senha())) return falha("SecKeychainItemCreateFromContent (<default>)", errSecParam);
    if (!pw) pw = "";
    u32 pl = (u32)strlen(pw);
    OSStatus st;
    if (a.atualiza) {
        void *item = procura(inet, &a, &st);
        if (item) {
            st = SecKeychainItemModifyAttributesAndData(item, 0, pl, pw);
            CFRelease(item);
            return st ? falha("SecKeychainItemModifyAttributesAndData", st) : 0;
        }
    }
    const char *s = a.servico ? a.servico : "", *c = a.conta ? a.conta : "";
    if (inet) st = SecKeychainAddInternetPassword(0, (u32)strlen(s), s, 0, 0, (u32)strlen(c), c, 0, 0, 0, 0, 0, pl, pw, 0);
    else st = SecKeychainAddGenericPassword(0, (u32)strlen(s), s, (u32)strlen(c), c, pl, pw, 0);
    return st ? falha("SecKeychainItemCreateFromContent (<default>)", st) : 0;
}

static int cmd_delete_password(int argc, char **argv, int inet) {
    item_args_t a;
    if (le_opcoes_item(argc, argv, inet ? "a:s:d:p:P:r:t:" : "a:s:", &a, inet, 0)) return USO;
    OSStatus st;
    void *item = procura(inet, &a, &st);
    if (!item) return falha("SecKeychainSearchCopyNext", st);
    st = mostra_item(item, inet, 0, 0);
    if (!st) st = SecKeychainItemDelete(item);
    CFRelease(item);
    if (st) return falha("SecKeychainItemDelete", st);
    printf("password has been deleted.\n");
    return 0;
}

/* ---- certificados: DER mínimo para achar o CN do sujeito */
static int tlv(const uint8_t **p, const uint8_t *e, int *tag, const uint8_t **v, size_t *n) {
    const uint8_t *q = *p;
    if (e - q < 2) return 0;
    *tag = *q++;
    size_t len = *q++;
    if (len & 0x80) {
        int k = (int)(len & 0x7f);
        if (k < 1 || k > 4 || e - q < k) return 0;
        len = 0;
        while (k--) len = (len << 8) | *q++;
    }
    if ((size_t)(e - q) < len) return 0;
    *v = q;
    *n = len;
    *p = q + len;
    return 1;
}
/* nome do certificado como o rótulo do macOS: CN do sujeito; sem CN, OU; sem OU, O */
static int nome_cert(const uint8_t *der, size_t n, char *out, size_t cap) {
    const uint8_t *p = der, *e = der + n, *v, *tbs, *te;
    size_t len;
    int tag;
    out[0] = 0;
    if (!tlv(&p, e, &tag, &v, &len) || tag != 0x30) return 0;
    p = v; e = v + len;
    if (!tlv(&p, e, &tag, &tbs, &len) || tag != 0x30) return 0;
    te = tbs + len;
    p = tbs;
    int campo = 0;
    const uint8_t *sujeito = 0;
    size_t slen = 0;
    while (p < te && tlv(&p, te, &tag, &v, &len)) {
        if (campo == 0 && tag == 0xa0) continue;                 /* versão [0] */
        campo++;
        if (campo == 5) { sujeito = v; slen = len; break; }       /* serial, alg, emissor, validade, sujeito */
    }
    if (!sujeito) return 0;
    static const uint8_t oids[3][3] = {{0x55, 4, 3}, {0x55, 4, 11}, {0x55, 4, 10}};
    for (int k = 0; k < 3; k++) {
        const uint8_t *q = sujeito, *qe = sujeito + slen, *set, *seq, *oid, *val;
        size_t l1, l2, l3, l4;
        int t1, t2, t3, t4;
        while (q < qe && tlv(&q, qe, &t1, &set, &l1)) {
            const uint8_t *r = set, *re = set + l1;
            while (r < re && tlv(&r, re, &t2, &seq, &l2)) {
                const uint8_t *s = seq, *se = seq + l2;
                if (!tlv(&s, se, &t3, &oid, &l3) || t3 != 6 || l3 != 3 || memcmp(oid, oids[k], 3)) continue;
                if (!tlv(&s, se, &t4, &val, &l4)) continue;
                size_t o = 0;
                if (t4 == 0x1e) {                                  /* BMPString: UCS-2, só o byte baixo se ASCII */
                    for (size_t i = 0; i + 1 < l4 && o + 1 < cap; i += 2) out[o++] = val[i] ? '?' : (char)val[i + 1];
                } else {
                    for (size_t i = 0; i < l4 && o + 1 < cap; i++) out[o++] = (char)val[i];
                }
                out[o] = 0;
                return 1;
            }
        }
    }
    return 0;
}

static void pem(const uint8_t *b, size_t n) {
    static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    printf("-----BEGIN CERTIFICATE-----\n");
    int col = 0;
    for (size_t i = 0; i < n; i += 3) {
        u32 v = (u32)b[i] << 16 | (i + 1 < n ? (u32)b[i + 1] << 8 : 0) | (i + 2 < n ? b[i + 2] : 0);
        char q[4] = {B64[v >> 18 & 63], B64[v >> 12 & 63], i + 1 < n ? B64[v >> 6 & 63] : '=', i + 2 < n ? B64[v & 63] : '='};
        for (int k = 0; k < 4; k++) {
            putchar(q[k]);
            if (++col == 64) { putchar('\n'); col = 0; }
        }
    }
    if (col) putchar('\n');
    printf("-----END CERTIFICATE-----\n");
}

#define SISTEMA "/System/Library/Keychains/SystemRootCertificates.keychain"
static const void *raizes(void) {
    const void *arr = 0;
    if (SecTrustSettingsCopyCertificates(2, &arr) != 0) return 0;     /* kSecTrustSettingsDomainSystem */
    return arr;
}

static int cmd_find_certificate(int argc, char **argv) {
    opcoes_t o = {argc, argv, 1, 0, 0};
    int c, todos = 0, p = 0;
    const char *nome = 0;
    while ((c = opcao(&o, "ac:pm")) != -1) {
        switch (c) {
        case 'a': todos = 1; break;
        case 'c': nome = o.arg; break;
        case 'p': p = 1; break;
        case 'm': break;
        default: return USO;
        }
    }
    const void *arr = raizes();
    CFIndex n = arr ? CFArrayGetCount(arr) : 0, achou = 0;
    for (CFIndex i = 0; i < n; i++) {
        void *d = SecCertificateCopyData(CFArrayGetValueAtIndex(arr, i));
        if (!d) continue;
        const uint8_t *der = CFDataGetBytePtr(d);
        size_t len = (size_t)CFDataGetLength(d);
        char cn[256];
        nome_cert(der, len, cn, sizeof cn);
        if (!nome || strstr(cn, nome)) {
            achou++;
            if (p) pem(der, len);
            else {
                printf("keychain: \"%s\"\n", SISTEMA);
                printf("version: 256\n");
                printf("class: 0x80001000 \n");
                printf("attributes:\n");
                printf("    \"alis\"<blob>=");
                blob(stdout, (const uint8_t *)cn, (u32)strlen(cn));
                printf("\n    \"labl\"<blob>=");
                blob(stdout, (const uint8_t *)cn, (u32)strlen(cn));
                printf("\n");
            }
        }
        CFRelease(d);
        if (achou && !todos) break;
    }
    if (arr) CFRelease(arr);
    return achou ? 0 : falha("SecKeychainSearchCopyNext", errSecItemNotFound);
}

static int cmd_dump_trust_settings(int argc, char **argv) {
    opcoes_t o = {argc, argv, 1, 0, 0};
    int c, dominio = 0;
    while ((c = opcao(&o, "sd")) != -1) {
        if (c == 's') dominio = 2;
        else if (c == 'd') dominio = 1;
        else return USO;
    }
    const void *arr = 0;
    OSStatus st = SecTrustSettingsCopyCertificates(dominio, &arr);
    if (st) {
        fprintf(stderr, "SecTrustSettingsCopyCertificates: %s\n", mensagem(st));
        return 1;
    }
    CFIndex n = CFArrayGetCount(arr);
    printf("Number of trusted certs = %ld\n", (long)n);
    for (CFIndex i = 0; i < n; i++) {
        void *d = SecCertificateCopyData(CFArrayGetValueAtIndex(arr, i));
        char cn[256] = "";
        if (d) { nome_cert(CFDataGetBytePtr(d), (size_t)CFDataGetLength(d), cn, sizeof cn); CFRelease(d); }
        printf("Cert %ld: %s\n", (long)i, cn);
        printf("   Number of trust settings : 0\n");
    }
    CFRelease(arr);
    return 0;
}

/* ---- verify-cert: arquivo PEM ou DER; vale o primeiro certificado */
static uint8_t *le_cert(const char *arq, size_t *n) {
    FILE *f = fopen(arq, "rb");
    if (!f) return 0;
    size_t cap = 1 << 16, len = 0;
    uint8_t *b = malloc(cap);
    size_t k;
    while (b && (k = fread(b + len, 1, cap - len, f)) > 0) {
        len += k;
        if (len == cap) { uint8_t *nb = realloc(b, cap *= 2); if (!nb) { free(b); b = 0; } else b = nb; }
    }
    fclose(f);
    if (!b) return 0;
    const char *ini = "-----BEGIN CERTIFICATE-----";
    uint8_t *s = 0;
    for (size_t i = 0; i + strlen(ini) <= len; i++) if (!memcmp(b + i, ini, strlen(ini))) { s = b + i + strlen(ini); break; }
    if (!s) { *n = len; return b; }                              /* DER */
    uint8_t *der = malloc(len), *o = der;
    u32 acc = 0;
    int bits = 0;
    for (; der && s < b + len && *s != '-'; s++) {
        int v = *s >= 'A' && *s <= 'Z' ? *s - 'A' : *s >= 'a' && *s <= 'z' ? *s - 'a' + 26
              : *s >= '0' && *s <= '9' ? *s - '0' + 52 : *s == '+' ? 62 : *s == '/' ? 63 : -1;
        if (v < 0) continue;
        acc = acc << 6 | (u32)v;
        bits += 6;
        if (bits >= 8) { bits -= 8; *o++ = (uint8_t)(acc >> bits); }
    }
    free(b);
    *n = der ? (size_t)(o - der) : 0;
    return der;
}

static int cmd_verify_cert(int argc, char **argv) {
    opcoes_t o = {argc, argv, 1, 0, 0};
    int c, quieto = 0;
    const char *arq = 0, *raiz = 0;
    while ((c = opcao(&o, "c:r:p:k:n:qlLi")) != -1) {
        switch (c) {
        case 'c': if (!arq) arq = o.arg; break;
        case 'r': raiz = o.arg; break;
        case 'q': quieto = 1; break;
        case 'p': case 'k': case 'n': case 'l': case 'L': case 'i': break;
        default: return USO;
        }
    }
    if (!arq) return USO;
    size_t n = 0, rn = 0;
    uint8_t *der = le_cert(arq, &n);
    if (!der || !n) { fprintf(stderr, "Error reading cert file %s\n", arq); free(der); return 1; }
    int ok = 0;
    uint8_t *r = raiz ? le_cert(raiz, &rn) : 0;
    if (r && rn == n && !memcmp(r, der, n)) ok = 1;
    free(r);
    const void *arr = ok ? 0 : raizes();
    for (CFIndex i = 0, m = arr ? CFArrayGetCount(arr) : 0; i < m && !ok; i++) {
        void *d = SecCertificateCopyData(CFArrayGetValueAtIndex(arr, i));
        if (!d) continue;
        if ((size_t)CFDataGetLength(d) == n && !memcmp(CFDataGetBytePtr(d), der, n)) ok = 1;
        CFRelease(d);
    }
    if (arr) CFRelease(arr);
    free(der);
    if (!ok) {
        fprintf(stderr, "Cert Verify Result: CSSMERR_TP_NOT_TRUSTED\n");
        return 1;
    }
    if (!quieto) printf("...certificate verification successful.\n");
    return 0;
}

/* ---- keychains: há um só */
static int cmd_lista(int argc, char **argv) {
    opcoes_t o = {argc, argv, 1, 0, 0};
    int c, define = 0;
    while ((c = opcao(&o, "d:s")) != -1) {
        if (c == 's') define = 1;
        else if (c != 'd') return USO;
    }
    if (define) return 0;                                        /* -s: trocar a lista não muda nada no mOS */
    printf("    \"%s\"\n", caminho_keychain());
    return 0;
}
static int cmd_sem_efeito(int argc, char **argv, const char *spec) {
    opcoes_t o = {argc, argv, 1, 0, 0};
    int c;
    while ((c = opcao(&o, spec)) != -1) if (c == '?') return USO;
    return 0;
}
static int cmd_show_keychain_info(int argc, char **argv) {
    if (cmd_sem_efeito(argc, argv, "")) return USO;
    fprintf(stderr, "Keychain \"%s\" no-timeout\n", caminho_keychain());
    return 0;
}

typedef struct { const char *nome; const char *uso; } comando_t;
static const comando_t COMANDOS[] = {
    {"list-keychains", "Display or manipulate the keychain search list."},
    {"default-keychain", "Display or set the default keychain."},
    {"login-keychain", "Display or set the login keychain."},
    {"show-keychain-info", "Show the settings for keychain."},
    {"unlock-keychain", "Unlock the specified keychain."},
    {"lock-keychain", "Lock the specified keychain."},
    {"add-generic-password", "Add a generic password item."},
    {"add-internet-password", "Add an internet password item."},
    {"find-generic-password", "Find a generic password item."},
    {"find-internet-password", "Find an internet password item."},
    {"delete-generic-password", "Delete a generic password item."},
    {"delete-internet-password", "Delete an internet password item."},
    {"find-certificate", "Find a certificate item."},
    {"dump-trust-settings", "Display contents of trust settings."},
    {"verify-cert", "Verify certificate(s)."},
};
static void uso(FILE *f) {
    fprintf(f, "Usage: security [-h] command [command-options]\n");
    for (size_t i = 0; i < sizeof COMANDOS / sizeof COMANDOS[0]; i++)
        fprintf(f, "    %-28s %s\n", COMANDOS[i].nome, COMANDOS[i].uso);
}

int main(int argc, char **argv) {
    int i = 1;
    while (i < argc && argv[i][0] == '-' && argv[i][1]) {          /* -q, -v, -i: opções globais sem efeito */
        if (!strcmp(argv[i], "-h")) { uso(stdout); return 0; }
        i++;
    }
    if (i >= argc) { uso(stderr); return USO; }
    const char *c = argv[i];
    int n = argc - i;
    char **v = argv + i;
    int r;
    if (!strcmp(c, "find-generic-password")) r = cmd_find_password(n, v, 0);
    else if (!strcmp(c, "find-internet-password")) r = cmd_find_password(n, v, 1);
    else if (!strcmp(c, "add-generic-password")) r = cmd_add_password(n, v, 0);
    else if (!strcmp(c, "add-internet-password")) r = cmd_add_password(n, v, 1);
    else if (!strcmp(c, "delete-generic-password")) r = cmd_delete_password(n, v, 0);
    else if (!strcmp(c, "delete-internet-password")) r = cmd_delete_password(n, v, 1);
    else if (!strcmp(c, "find-certificate")) r = cmd_find_certificate(n, v);
    else if (!strcmp(c, "dump-trust-settings")) r = cmd_dump_trust_settings(n, v);
    else if (!strcmp(c, "verify-cert")) r = cmd_verify_cert(n, v);
    else if (!strcmp(c, "list-keychains")) r = cmd_lista(n, v);
    else if (!strcmp(c, "default-keychain") || !strcmp(c, "login-keychain")) r = cmd_lista(n, v);
    else if (!strcmp(c, "show-keychain-info")) r = cmd_show_keychain_info(n, v);
    else if (!strcmp(c, "unlock-keychain")) r = cmd_sem_efeito(n, v, "up:");
    else if (!strcmp(c, "lock-keychain")) r = cmd_sem_efeito(n, v, "a");
    else {
        fprintf(stderr, "security: unknown command \"%s\"\n", c);
        uso(stderr);
        return USO;
    }
    if (r == USO) {
        for (size_t k = 0; k < sizeof COMANDOS / sizeof COMANDOS[0]; k++)
            if (!strcmp(COMANDOS[k].nome, c)) fprintf(stderr, "usage: security %s ... (see security(1))\n", c);
    }
    fflush(stdout);
    return r;
}
