/*
 * mOS — teste do Security.framework do libSystem (userland/libsystem/security.c) e do CFData (cfdata.c).
 * Compila igual no macOS (clang main.c -framework Security -framework CoreFoundation); no mOS:
 *   .\tools\build-sectest.ps1      e rode no harness (machotest) ou no Terminal do mOS: SecTest.app
 * Com o argumento "reler", só confere que o item gravado por uma execução anterior continua lá (outro processo
 * lendo o arquivo do Keychain). Imprime "sectest: N/N ok".
 *
 * Os protótipos são declarados aqui (o SDK do mOS não tem os cabeçalhos do Security), com os tipos da Apple.
 */
#include <stdio.h>
#include <string.h>

typedef const void *CFTypeRef;
typedef const void *CFStringRef, *CFDataRef, *CFDictionaryRef, *CFArrayRef, *CFAllocatorRef;
typedef void *CFMutableDictionaryRef;
typedef int OSStatus;
typedef unsigned char Boolean;
typedef long CFIndex;
typedef struct { CFIndex location, length; } CFRange;
typedef void *SecKeychainItemRef, *SecCodeRef;

extern const CFStringRef kSecClass, kSecClassGenericPassword, kSecClassInternetPassword, kSecAttrService, kSecAttrAccount,
    kSecAttrLabel, kSecValueData, kSecReturnData, kSecReturnAttributes, kSecReturnRef, kSecMatchLimit, kSecMatchLimitAll,
    kSecMatchLimitOne, kSecAttrCreationDate, kSecAttrModificationDate;
extern const void *kCFBooleanTrue;
extern const void *kCFTypeDictionaryKeyCallBacks, *kCFTypeDictionaryValueCallBacks;

OSStatus SecItemAdd(CFDictionaryRef, CFTypeRef *);
OSStatus SecItemCopyMatching(CFDictionaryRef, CFTypeRef *);
OSStatus SecItemUpdate(CFDictionaryRef, CFDictionaryRef);
OSStatus SecItemDelete(CFDictionaryRef);
OSStatus SecKeychainFindGenericPassword(CFTypeRef, unsigned, const char *, unsigned, const char *, unsigned *, void **, SecKeychainItemRef *);
OSStatus SecKeychainAddGenericPassword(CFTypeRef, unsigned, const char *, unsigned, const char *, unsigned, const void *, SecKeychainItemRef *);
OSStatus SecKeychainItemModifyAttributesAndData(SecKeychainItemRef, const void *, unsigned, const void *);
OSStatus SecKeychainItemDelete(SecKeychainItemRef);
OSStatus SecKeychainItemFreeContent(void *, void *);
OSStatus SecCodeCopySelf(unsigned, SecCodeRef *);
OSStatus SecCodeCheckValidity(SecCodeRef, unsigned, CFTypeRef);
CFStringRef SecCopyErrorMessageString(OSStatus, void *);

CFMutableDictionaryRef CFDictionaryCreateMutable(CFAllocatorRef, CFIndex, const void *, const void *);
void CFDictionarySetValue(CFMutableDictionaryRef, const void *, const void *);
const void *CFDictionaryGetValue(CFDictionaryRef, const void *);
CFIndex CFArrayGetCount(CFArrayRef);
CFDataRef CFDataCreate(CFAllocatorRef, const unsigned char *, CFIndex);
void *CFDataCreateMutable(CFAllocatorRef, CFIndex);
void CFDataAppendBytes(void *, const unsigned char *, CFIndex);
void CFDataReplaceBytes(void *, CFRange, const unsigned char *, CFIndex);
void CFDataSetLength(void *, CFIndex);
CFIndex CFDataGetLength(CFDataRef);
const unsigned char *CFDataGetBytePtr(CFDataRef);
CFStringRef CFStringCreateWithCString(CFAllocatorRef, const char *, unsigned);
Boolean CFStringGetCString(CFStringRef, char *, CFIndex, unsigned);
unsigned long CFGetTypeID(CFTypeRef);
unsigned long CFDataGetTypeID(void);
void CFRelease(CFTypeRef);
#define UTF8 0x08000100u

static int ok, total;
static void check(int cond, const char *what) {
    total++;
    if (cond) ok++;
    printf("%s %s\n", cond ? "  ok  " : "  FALHA", what);
}
static CFStringRef S(const char *s) { return CFStringCreateWithCString(0, s, UTF8); }
static CFMutableDictionaryRef Q(CFStringRef cls, const char *svc, const char *acc) {
    CFMutableDictionaryRef d = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(d, kSecClass, cls);
    if (svc) CFDictionarySetValue(d, kSecAttrService, S(svc));
    if (acc) CFDictionarySetValue(d, kSecAttrAccount, S(acc));
    return d;
}
static int data_is(CFTypeRef d, const char *s) {
    return d && CFGetTypeID(d) == CFDataGetTypeID() && CFDataGetLength(d) == (CFIndex)strlen(s) &&
           !memcmp(CFDataGetBytePtr(d), s, strlen(s));
}

int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "reler")) {
        CFMutableDictionaryRef q = Q(kSecClassGenericPassword, "mOS Teste Persistente", "visitante");
        CFDictionarySetValue(q, kSecReturnData, kCFBooleanTrue);
        CFTypeRef r = 0;
        check(SecItemCopyMatching(q, &r) == 0 && data_is(r, "sobrevive"), "item de outro processo continua no Keychain");
        SecItemDelete(Q(kSecClassGenericPassword, "mOS Teste Persistente", 0));
        printf("sectest: %d/%d ok\n", ok, total);
        return ok == total ? 0 : 1;
    }

    /* ---- CFData */
    void *m = CFDataCreateMutable(0, 0);
    CFDataAppendBytes(m, (const unsigned char *)"abc", 3);
    CFDataAppendBytes(m, (const unsigned char *)"def", 3);
    check(data_is(m, "abcdef"), "CFDataAppendBytes");
    CFDataReplaceBytes(m, (CFRange){1, 2}, (const unsigned char *)"XYZW", 4);
    check(data_is(m, "aXYZWdef"), "CFDataReplaceBytes (crescendo)");
    CFDataSetLength(m, 3);
    check(data_is(m, "aXY"), "CFDataSetLength");

    /* limpa sobras de execuções anteriores */
    SecItemDelete(Q(kSecClassGenericPassword, "mOS Teste", 0));
    SecItemDelete(Q(kSecClassGenericPassword, "Code Safe Storage", 0));

    /* ---- SecItem: add / duplicate / copy / update / delete */
    CFMutableDictionaryRef a = Q(kSecClassGenericPassword, "mOS Teste", "alice");
    CFDictionarySetValue(a, kSecValueData, CFDataCreate(0, (const unsigned char *)"segredo1", 8));
    CFDictionarySetValue(a, kSecAttrLabel, S("rótulo ç"));
    check(SecItemAdd(a, 0) == 0, "SecItemAdd");
    check(SecItemAdd(a, 0) == -25299, "SecItemAdd duplicado = errSecDuplicateItem");
    CFMutableDictionaryRef b = Q(kSecClassGenericPassword, "mOS Teste", "bob");
    CFDictionarySetValue(b, kSecValueData, CFDataCreate(0, (const unsigned char *)"segredo2", 8));
    check(SecItemAdd(b, 0) == 0, "SecItemAdd (segunda conta)");

    CFMutableDictionaryRef q = Q(kSecClassGenericPassword, "mOS Teste", "alice");
    CFDictionarySetValue(q, kSecReturnData, kCFBooleanTrue);
    CFTypeRef r = 0;
    check(SecItemCopyMatching(q, &r) == 0 && data_is(r, "segredo1"), "SecItemCopyMatching kSecReturnData");

    CFMutableDictionaryRef qa = Q(kSecClassGenericPassword, "mOS Teste", "alice");
    CFDictionarySetValue(qa, kSecReturnAttributes, kCFBooleanTrue);
    CFDictionarySetValue(qa, kSecReturnData, kCFBooleanTrue);
    r = 0;
    int st = SecItemCopyMatching(qa, &r);
    char buf[64] = "";
    CFStringRef lab = r ? CFDictionaryGetValue(r, kSecAttrLabel) : 0;
    if (lab) CFStringGetCString(lab, buf, sizeof buf, UTF8);
    check(st == 0 && r && !strcmp(buf, "rótulo ç") && data_is(CFDictionaryGetValue(r, kSecValueData), "segredo1") &&
          CFDictionaryGetValue(r, kSecAttrCreationDate), "SecItemCopyMatching atributos + dados (UTF-8, datas)");

    CFMutableDictionaryRef qall = Q(kSecClassGenericPassword, "mOS Teste", 0);
    CFDictionarySetValue(qall, kSecMatchLimit, kSecMatchLimitAll);
    CFDictionarySetValue(qall, kSecReturnAttributes, kCFBooleanTrue);
    r = 0;
    check(SecItemCopyMatching(qall, &r) == 0 && r && CFArrayGetCount(r) == 2, "kSecMatchLimitAll devolve as 2 contas");

    CFMutableDictionaryRef ch = CFDictionaryCreateMutable(0, 0, 0, 0);
    CFDictionarySetValue(ch, kSecValueData, CFDataCreate(0, (const unsigned char *)"novo", 4));
    check(SecItemUpdate(Q(kSecClassGenericPassword, "mOS Teste", "alice"), ch) == 0, "SecItemUpdate");
    r = 0;
    check(SecItemCopyMatching(q, &r) == 0 && data_is(r, "novo"), "valor atualizado");
    check(SecItemCopyMatching(Q(kSecClassInternetPassword, "mOS Teste", "alice"), &r) == -25300, "outra classe = errSecItemNotFound");

    check(SecItemDelete(Q(kSecClassGenericPassword, "mOS Teste", "alice")) == 0, "SecItemDelete");
    r = 0;
    check(SecItemCopyMatching(q, &r) == -25300, "apagado = errSecItemNotFound");
    check(SecItemDelete(Q(kSecClassGenericPassword, "mOS Teste", 0)) == 0, "SecItemDelete do resto");

    /* ---- API antiga, como o OSCrypt do Chromium/Electron ("<App> Safe Storage") */
    unsigned len = 0;
    void *pw = 0;
    SecKeychainItemRef item = 0;
    check(SecKeychainFindGenericPassword(0, 17, "Code Safe Storage", 4, "Code", &len, &pw, &item) == -25300, "Safe Storage ainda não existe");
    check(SecKeychainAddGenericPassword(0, 17, "Code Safe Storage", 4, "Code", 16, "0123456789abcdef", &item) == 0 && item,
          "SecKeychainAddGenericPassword");
    check(SecKeychainFindGenericPassword(0, 17, "Code Safe Storage", 4, "Code", &len, &pw, &item) == 0 && len == 16 &&
          !memcmp(pw, "0123456789abcdef", 16), "SecKeychainFindGenericPassword");
    SecKeychainItemFreeContent(0, pw);
    check(SecKeychainItemModifyAttributesAndData(item, 0, 3, "xyz") == 0, "SecKeychainItemModifyAttributesAndData");
    CFMutableDictionaryRef qs = Q(kSecClassGenericPassword, "Code Safe Storage", "Code");
    CFDictionarySetValue(qs, kSecReturnData, kCFBooleanTrue);
    r = 0;
    check(SecItemCopyMatching(qs, &r) == 0 && data_is(r, "xyz"), "SecItem enxerga o item da API antiga");
    check(SecKeychainItemDelete(item) == 0, "SecKeychainItemDelete");
    check(SecKeychainFindGenericPassword(0, 17, "Code Safe Storage", 4, "Code", &len, 0, 0) == -25300, "apagado pela API antiga");

    /* ---- assinatura de código: tudo "não assinado" */
    SecCodeRef self = 0;
    check(SecCodeCopySelf(0, &self) == 0 && self, "SecCodeCopySelf");
    check(SecCodeCheckValidity(self, 0, 0) == -67062, "SecCodeCheckValidity = errSecCSUnsigned");
    CFStringRef msg = SecCopyErrorMessageString(-25300, 0);
    check(msg && CFStringGetCString(msg, buf, sizeof buf, UTF8) && strstr(buf, "could not be found"), "SecCopyErrorMessageString");

    /* ---- deixa um item para a execução "reler" (outro processo) */
    CFMutableDictionaryRef p = Q(kSecClassGenericPassword, "mOS Teste Persistente", "visitante");
    CFDictionarySetValue(p, kSecValueData, CFDataCreate(0, (const unsigned char *)"sobrevive", 9));
    SecItemDelete(Q(kSecClassGenericPassword, "mOS Teste Persistente", 0));
    check(SecItemAdd(p, 0) == 0, "item persistente gravado");

    printf("sectest: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
