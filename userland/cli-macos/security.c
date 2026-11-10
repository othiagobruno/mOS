/*
 * mOS — /usr/bin/security (Mach-O): os subcomandos do security(1) do macOS que os apps chamam.
 *   find-certificate -a -p [-c nome] [keychain ...]
 *       As raízes que o mOS confia (SecTrustSettingsCopyCertificates, domínio sistema: a lista da Mozilla do
 *       kernel) em PEM. É o que o @vscode/proxy-agent do VS Code e do Cursor lê
 *       (SystemRootCertificates.keychain e System.keychain) e o que o Node usa com --use-system-ca.
 *       Keychain do usuário (login.keychain*) não tem certificados no mOS: sai 44, como o macOS sem item.
 *   find-generic-password [-a conta] [-s serviço] [-w] / add-generic-password -a -s -w [-U]
 *   delete-generic-password [-a conta] [-s serviço]
 *       O Keychain do mOS (userland/libsystem/security.c, SecKeychain*).
 *   default-keychain, login-keychain, list-keychains [-d user|system]
 *       Os caminhos do macOS; o mOS tem um Keychain só, que atende a todos.
 * Códigos de saída do macOS: 0 ok, 1 erro de uso, 44 (errSecItemNotFound) item ausente, 45 duplicado.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef int OSStatus;
typedef unsigned int u32;
typedef const void *CFTypeRef;
/* libSystem do mOS (Security, CoreFoundation) */
OSStatus SecTrustSettingsCopyCertificates(int domain, CFTypeRef *certs);
CFTypeRef SecCertificateCopyData(CFTypeRef cert);
long CFArrayGetCount(CFTypeRef a);
CFTypeRef CFArrayGetValueAtIndex(CFTypeRef a, long i);
const unsigned char *CFDataGetBytePtr(CFTypeRef d);
long CFDataGetLength(CFTypeRef d);
void CFRelease(CFTypeRef o);
OSStatus SecKeychainFindGenericPassword(void *kc, u32 sl, const char *svc, u32 al, const char *acc, u32 *pl, void **pw, CFTypeRef *item);
OSStatus SecKeychainAddGenericPassword(void *kc, u32 sl, const char *svc, u32 al, const char *acc, u32 pl, const void *pw, CFTypeRef *item);
OSStatus SecKeychainItemModifyAttributesAndData(CFTypeRef item, const void *attrs, u32 len, const void *data);
OSStatus SecKeychainItemDelete(CFTypeRef item);
OSStatus SecKeychainItemFreeContent(void *attrs, void *data);

enum { errSecSuccess = 0, errSecDuplicateItem = -25299, errSecItemNotFound = -25300 };
enum { kSecTrustSettingsDomainSystem = 2 };
#define EXIT_NOTFOUND 44
#define EXIT_DUP 45

static const char *USER_KC = "/Users/Visitante/Library/Keychains/login.keychain-db";

static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

/* PEM com linhas de 64 colunas, como o security(1) */
static void print_pem(const unsigned char *d, long n) {
    char line[80];
    int col = 0;
    fputs("-----BEGIN CERTIFICATE-----\n", stdout);
    for (long i = 0; i < n; i += 3) {
        unsigned v = (unsigned)d[i] << 16 | (i + 1 < n ? (unsigned)d[i + 1] << 8 : 0) | (i + 2 < n ? d[i + 2] : 0);
        char q[4] = {b64[v >> 18 & 63], b64[v >> 12 & 63], i + 1 < n ? b64[v >> 6 & 63] : '=', i + 2 < n ? b64[v & 63] : '='};
        for (int k = 0; k < 4; k++) {
            line[col++] = q[k];
            if (col == 64) { line[col++] = '\n'; fwrite(line, 1, (size_t)col, stdout); col = 0; }
        }
    }
    if (col) { line[col++] = '\n'; fwrite(line, 1, (size_t)col, stdout); }
    fputs("-----END CERTIFICATE-----\n", stdout);
}

/* keychains que guardam as raízes do sistema; o resto (login) não tem certificados no mOS */
static int is_system_kc(const char *p) {
    return strstr(p, "SystemRootCertificates.keychain") || strstr(p, "/Library/Keychains/System.keychain") ||
           !strcmp(p, "System.keychain");
}

/* DER contém o texto (busca do -c no CN/organização, como o macOS faz por substring no rótulo) */
static int der_has(const unsigned char *d, long n, const char *s) {
    size_t l = strlen(s);
    if (!l) return 1;
    for (long i = 0; i + (long)l <= n; i++) if (!memcmp(d + i, s, l)) return 1;
    return 0;
}

static int find_certificate(int argc, char **argv) {
    int all = 0, pem = 0, nkc = 0, sys = 0;
    const char *name = 0;
    for (int i = 0; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "-a")) all = 1;
        else if (!strcmp(a, "-p")) pem = 1;
        else if ((!strcmp(a, "-c") || !strcmp(a, "-e")) && i + 1 < argc) name = argv[++i];
        else if (!strcmp(a, "-Z") || !strcmp(a, "-m")) { /* hash/e-mail: só o PEM é impresso */ }
        else if (a[0] == '-') { fprintf(stderr, "security: opção desconhecida %s\n", a); return 2; }
        else { nkc++; if (is_system_kc(a)) sys = 1; }
    }
    if (!nkc) sys = 1;                       /* lista de busca padrão inclui as raízes */
    if (!sys) {
        fprintf(stderr, "security: SecKeychainSearchCopyNext: The specified item could not be found in the keychain.\n");
        return EXIT_NOTFOUND;
    }
    CFTypeRef certs = 0;
    if (SecTrustSettingsCopyCertificates(kSecTrustSettingsDomainSystem, &certs) != errSecSuccess || !certs) {
        fprintf(stderr, "security: SecKeychainSearchCopyNext: The specified item could not be found in the keychain.\n");
        return EXIT_NOTFOUND;
    }
    long n = CFArrayGetCount(certs), shown = 0;
    for (long i = 0; i < n; i++) {
        CFTypeRef data = SecCertificateCopyData(CFArrayGetValueAtIndex(certs, i));
        if (!data) continue;
        const unsigned char *d = CFDataGetBytePtr(data);
        long len = CFDataGetLength(data);
        if (!name || der_has(d, len, name)) {
            if (pem) print_pem(d, len);
            else printf("keychain: \"/System/Library/Keychains/SystemRootCertificates.keychain\"\nclass: 0x80001000\n");
            shown++;
        }
        CFRelease(data);
        if (shown && !all) break;
    }
    CFRelease(certs);
    if (!shown) {
        fprintf(stderr, "security: SecKeychainSearchCopyNext: The specified item could not be found in the keychain.\n");
        return EXIT_NOTFOUND;
    }
    return 0;
}

typedef struct { const char *acc, *svc, *pw; int w, upd; } gp_args;

static int parse_gp(int argc, char **argv, gp_args *g) {
    memset(g, 0, sizeof *g);
    for (int i = 0; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "-a") && i + 1 < argc) g->acc = argv[++i];
        else if (!strcmp(a, "-s") && i + 1 < argc) g->svc = argv[++i];
        else if (!strcmp(a, "-w")) {
            /* add: -w <senha>; find: -w sem valor (imprime só a senha) */
            if (i + 1 < argc && argv[i + 1][0] != '-') g->pw = argv[++i];
            g->w = 1;
        } else if (!strcmp(a, "-U")) g->upd = 1;
        else if ((!strcmp(a, "-l") || !strcmp(a, "-D") || !strcmp(a, "-j") || !strcmp(a, "-c") || !strcmp(a, "-C") ||
                  !strcmp(a, "-G") || !strcmp(a, "-T")) && i + 1 < argc) i++;   /* rótulo, tipo, comentário, app: ignorados */
        else if (!strcmp(a, "-g") || !strcmp(a, "-A")) { }
        else if (a[0] != '-') { /* keychain: o mOS tem um só */ }
        else { fprintf(stderr, "security: opção desconhecida %s\n", a); return -1; }
    }
    return 0;
}

static u32 slen(const char *s) { return s ? (u32)strlen(s) : 0; }

static int find_generic(int argc, char **argv) {
    gp_args g;
    if (parse_gp(argc, argv, &g)) return 2;
    u32 pl = 0;
    void *pw = 0;
    CFTypeRef item = 0;
    OSStatus st = SecKeychainFindGenericPassword(0, slen(g.svc), g.svc, slen(g.acc), g.acc, &pl, &pw, &item);
    if (st != errSecSuccess) {
        fprintf(stderr, "security: SecKeychainSearchCopyNext: The specified item could not be found in the keychain.\n");
        return EXIT_NOTFOUND;
    }
    if (g.w) {
        fwrite(pw, 1, pl, stdout);
        fputc('\n', stdout);
    } else {
        printf("keychain: \"%s\"\nversion: 512\nclass: \"genp\"\nattributes:\n", USER_KC);
        printf("    \"acct\"<blob>=\"%s\"\n    \"svce\"<blob>=\"%s\"\n", g.acc ? g.acc : "", g.svc ? g.svc : "");
    }
    SecKeychainItemFreeContent(0, pw);
    if (item) CFRelease(item);
    return 0;
}

static int add_generic(int argc, char **argv) {
    gp_args g;
    if (parse_gp(argc, argv, &g)) return 2;
    if (!g.acc || !g.svc) { fprintf(stderr, "security: add-generic-password precisa de -a e -s\n"); return 2; }
    const char *pw = g.pw ? g.pw : "";
    CFTypeRef item = 0;
    OSStatus st = SecKeychainAddGenericPassword(0, slen(g.svc), g.svc, slen(g.acc), g.acc, slen(pw), pw, &item);
    if (st == errSecDuplicateItem && g.upd) {
        st = SecKeychainFindGenericPassword(0, slen(g.svc), g.svc, slen(g.acc), g.acc, 0, 0, &item);
        if (st == errSecSuccess) st = SecKeychainItemModifyAttributesAndData(item, 0, slen(pw), pw);
    }
    if (item) CFRelease(item);
    if (st == errSecDuplicateItem) {
        fprintf(stderr, "security: SecKeychainItemCreateFromContent (<default>): The specified item already exists in the keychain.\n");
        return EXIT_DUP;
    }
    if (st != errSecSuccess) { fprintf(stderr, "security: erro %d ao gravar no Keychain\n", st); return 1; }
    return 0;
}

static int delete_generic(int argc, char **argv) {
    gp_args g;
    if (parse_gp(argc, argv, &g)) return 2;
    CFTypeRef item = 0;
    OSStatus st = SecKeychainFindGenericPassword(0, slen(g.svc), g.svc, slen(g.acc), g.acc, 0, 0, &item);
    if (st == errSecSuccess) st = SecKeychainItemDelete(item);
    if (item) CFRelease(item);
    if (st != errSecSuccess) {
        fprintf(stderr, "security: SecKeychainSearchCopyNext: The specified item could not be found in the keychain.\n");
        return EXIT_NOTFOUND;
    }
    printf("password has been deleted.\n");
    return 0;
}

static int list_keychains(int argc, char **argv) {
    int system = 0;
    for (int i = 0; i < argc; i++) if (!strcmp(argv[i], "-d") && i + 1 < argc && !strcmp(argv[i + 1], "system")) system = 1;
    if (system) printf("    \"/Library/Keychains/System.keychain\"\n");
    else printf("    \"%s\"\n    \"/Library/Keychains/System.keychain\"\n", USER_KC);
    return 0;
}

static void usage(void) {
    fprintf(stderr,
            "uso: security <comando> [opções]\n"
            "  find-certificate -a -p [-c nome] [keychain ...]\n"
            "  find-generic-password [-a conta] [-s serviço] [-w]\n"
            "  add-generic-password -a conta -s serviço -w senha [-U]\n"
            "  delete-generic-password [-a conta] [-s serviço]\n"
            "  default-keychain | login-keychain | list-keychains [-d user|system]\n");
}

int main(int argc, char **argv) {
    int i = 1;
    while (i < argc && argv[i][0] == '-' && strcmp(argv[i], "-h")) i++;   /* -q, -v, -i: globais, ignorados */
    if (i >= argc || !strcmp(argv[i], "-h") || !strcmp(argv[i], "help")) { usage(); return i >= argc ? 1 : 0; }
    const char *cmd = argv[i];
    int n = argc - i - 1;
    char **rest = argv + i + 1;
    if (!strcmp(cmd, "find-certificate")) return find_certificate(n, rest);
    if (!strcmp(cmd, "find-generic-password")) return find_generic(n, rest);
    if (!strcmp(cmd, "add-generic-password")) return add_generic(n, rest);
    if (!strcmp(cmd, "delete-generic-password")) return delete_generic(n, rest);
    if (!strcmp(cmd, "default-keychain") || !strcmp(cmd, "login-keychain")) { printf("    \"%s\"\n", USER_KC); return 0; }
    if (!strcmp(cmd, "list-keychains")) return list_keychains(n, rest);
    fprintf(stderr, "security: comando desconhecido no mOS: %s\n", cmd);
    usage();
    return 1;
}
