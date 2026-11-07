/*
 * mOS — ServiceManagement.framework (Mach-O, em /System/Library/Frameworks): SMJob* e os domínios do launchd que
 * binários da Apple referenciam (o Sparkle, para o instalador privilegiado). O mOS ainda não tem launchd de
 * serviços: submeter/remover um job falha (false + erro), como o macOS faz sem autorização, e o chamador desiste.
 * tools/build-frameworks-objc.ps1
 */
#include <stdio.h>

typedef const struct __CFString *CFStringRef;
typedef const struct __CFDictionary *CFDictionaryRef;
typedef const void *AuthorizationRef;
typedef const void *CFErrorRef;
typedef unsigned char Boolean;

/* constantes CFString como as do macOS: literais do clang (o CFSTR) na seção __cfstring */
#define CFSTR_DEF(name, text) const CFStringRef name = (CFStringRef)__builtin___CFStringMakeConstantString(text);

CFSTR_DEF(kSMDomainSystemLaunchd, "kSMDomainSystemLaunchd")
CFSTR_DEF(kSMDomainUserLaunchd, "kSMDomainUserLaunchd")

static void warn(void) {
    static int w;
    if (!w++) fprintf(stderr, "ServiceManagement[mOS]: sem serviços do launchd ainda (SMJob* falham)\n");
}

Boolean SMJobSubmit(CFStringRef domain, CFDictionaryRef job, AuthorizationRef auth, CFErrorRef *err) {
    (void)domain; (void)job; (void)auth; warn();
    if (err) *err = 0;
    return 0;
}
Boolean SMJobRemove(CFStringRef domain, CFStringRef label, AuthorizationRef auth, Boolean wait, CFErrorRef *err) {
    (void)domain; (void)label; (void)auth; (void)wait; warn();
    if (err) *err = 0;
    return 0;
}
Boolean SMJobBless(CFStringRef domain, CFStringRef label, AuthorizationRef auth, CFErrorRef *err) {
    (void)domain; (void)label; (void)auth; warn();
    if (err) *err = 0;
    return 0;
}
CFDictionaryRef SMJobCopyDictionary(CFStringRef domain, CFStringRef label) { (void)domain; (void)label; return 0; }
Boolean SMLoginItemSetEnabled(CFStringRef id, Boolean enabled) { (void)id; (void)enabled; warn(); return 0; }
