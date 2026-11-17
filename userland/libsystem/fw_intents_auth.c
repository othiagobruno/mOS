/*
 * mOS libSystem — Intents e AuthenticationServices que os apps do corpus ligam (docs/lacunas/natalia.md).
 * As classes de dados guardam as propriedades num dicionário da instância: o getter e o setter tiram a chave do próprio
 * seletor ("displayString" / "setDisplayString:"), e os inits/fábricas de objetos ("initWithIdentifier:displayString:",
 * "+successWithResolvedString:") gravam cada argumento na chave do seu pedaço do seletor. Assim o que o app grava volta
 * igual. O comportamento do sistema é o do macOS sem o serviço:
 *  - Siri/Atalhos não existem no mOS: INVocabulary e INRelevantShortcutStore aceitam e guardam; INFocusStatusCenter
 *    responde "negado" (não há Foco);
 *  - Sign in with Apple, passkeys e chaves de segurança não existem: ASAuthorizationController entrega
 *    authorizationController:didCompleteWithError: (ASAuthorizationErrorNotHandled) pelo delegate; o provedor de Apple
 *    ID diz CredentialNotFound; o gerenciador de credenciais do navegador diz "negado".
 */
#include "foundation.h"
#include "libc.h"

id objc_autorelease(id o);
id objc_retain(id o);
void objc_release(id o);
const char *sel_getName(SEL s);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
void *dispatch_get_main_queue(void);

static id cls(const char *n) { return (id)cx_class_named(n); }
#define ST(o) (*(id *)((u8 *)(o) + 8))
static id bag(id s) {
    if (!ST(s)) ST(s) = objc_retain(MSG0(id, cls("NSMutableDictionary"), "dictionary"));
    return ST(s);
}
static void key_of(const char *sel, char *out, size_t cap) {    /* "setFooBar:" -> "fooBar"; "fooBar" -> "fooBar" */
    const char *p = sel;
    size_t n = 0;
    if (!strncmp(p, "set", 3) && p[3] >= 'A' && p[3] <= 'Z') {
        p += 3;
        int acronym = p[1] >= 'A' && p[1] <= 'Z';               /* setURL: -> URL, como no KVC */
        out[n++] = acronym ? *p : (char)(*p + 32);
        p++;
    }
    while (*p && *p != ':' && n + 1 < cap) out[n++] = *p++;
    out[n] = 0;
}
static id g_get(id s, SEL c) { char k[96]; key_of(sel_getName(c), k, sizeof k); return MSG(id, bag(s), "objectForKey:", cx_str(k)); }
static void g_set(id s, SEL c, id v) {
    char k[96];
    key_of(sel_getName(c), k, sizeof k);
    if (v) MSG(void, bag(s), "setObject:forKey:", v, cx_str(k)); else MSG(void, bag(s), "removeObjectForKey:", cx_str(k));
}
static BOOL g_getb(id s, SEL c) { id v = g_get(s, c); return v && MSG0(BOOL, v, "boolValue"); }
static void g_setb(id s, SEL c, BOOL b) { g_set(s, c, cx_num_i(b)); }
static long g_getl(id s, SEL c) { id v = g_get(s, c); return v ? MSG0(long, v, "longValue") : 0; }
static void g_setl(id s, SEL c, long l) { g_set(s, c, cx_num_i(l)); }
/* pedaços de um seletor de init/fábrica -> chaves: "initWithIdentifier:displayString:" -> identifier, displayString;
 * "successWithResolvedString:" -> kind = success, resolvedString */
static void store_args(id obj, const char *sel, id *args, int n) {
    char piece[96], k[96];
    int ai = 0;
    const char *p = sel;
    while (*p && ai < n) {
        size_t m = 0;
        while (*p && *p != ':' && m + 1 < sizeof piece) piece[m++] = *p++;
        piece[m] = 0;
        if (*p == ':') p++;
        const char *w = strstr(piece, "With");
        const char *src = w ? w + 4 : piece;
        if (!strncmp(piece, "init", 4) && !w) src = piece + 4;
        size_t j = 0;
        for (; src[j] && j + 1 < sizeof k; j++) k[j] = j == 0 && src[j] >= 'A' && src[j] <= 'Z' ? (char)(src[j] + 32) : src[j];
        k[j] = 0;
        if (args[ai]) MSG(void, bag(obj), "setObject:forKey:", args[ai], cx_str(k));
        ai++;
    }
}
static int nargs(const char *sel) { int n = 0; for (; *sel; sel++) n += *sel == ':'; return n; }
static id g_init(id s, SEL c, id a, id b, id d, id e) { id v[4] = {a, b, d, e}; store_args(s, sel_getName(c), v, nargs(sel_getName(c))); return s; }
static id g_new(id k, SEL c, id a, id b, id d, id e) {
    id o = MSG0(id, MSG0(id, k, "alloc"), "init");
    const char *sel = sel_getName(c);
    char kind[48];
    size_t i = 0;
    while (sel[i] && sel[i] != ':' && i + 1 < sizeof kind && strncmp(sel + i, "With", 4)) { kind[i] = sel[i]; i++; }
    kind[i] = 0;
    MSG(void, bag(o), "setObject:forKey:", cx_str(kind), cx_str("_kind"));
    id v[4] = {a, b, d, e};
    store_args(o, sel, v, nargs(sel));
    return objc_autorelease(o);
}
static id g_kind(id k, SEL c) { return g_new(k, c, 0, 0, 0, 0); }   /* +needsValue, +notRequired, +unsupported */
static id g_dealloc_bag(id s) { if (ST(s)) { objc_release(ST(s)); ST(s) = 0; } return s; }
static void g_dealloc(id s, SEL c) {
    (void)c;
    g_dealloc_bag(s);
    cx_class_t *sup = cx_class_named("NSObject");
    ((void (*)(id, SEL))cx_lookup(sup, sel_registerName("dealloc")))(s, sel_registerName("dealloc"));
}

#define DEF(name) cx_class_t name##_c __asm__("OBJC_CLASS_$_" #name); cx_class_t name##_m __asm__("OBJC_METACLASS_$_" #name);
static cx_class_t *def(cx_class_t *k, cx_class_t *m, const char *n, const char *super) {
    cx_class_t *e = cx_class_named(n);
    if (e) return e;
    cx_class_t *s = cx_class_named(super);
    cx_define_class(k, m, n, s ? s : cx_class_named("NSObject"), 8);
    cx_add_method(k, "dealloc", (void *)g_dealloc);
    return k;
}
static void props(cx_class_t *k, const char *const *names) {
    char set[100];
    for (int i = 0; names[i]; i++) {
        const char *n = names[i];
        if (n[0] == '?') {                                      /* BOOL */
            n++;
            cx_add_method(k, n, (void *)g_getb);
            snprintf(set, sizeof set, "set%c%s:", n[0] >= 'a' && n[0] <= 'z' ? n[0] - 32 : n[0], n + 1);
            cx_add_method(k, set, (void *)g_setb);
        } else if (n[0] == '#') {                               /* NSInteger */
            n++;
            cx_add_method(k, n, (void *)g_getl);
            snprintf(set, sizeof set, "set%c%s:", n[0] >= 'a' && n[0] <= 'z' ? n[0] - 32 : n[0], n + 1);
            cx_add_method(k, set, (void *)g_setl);
        } else {
            cx_add_method(k, n, (void *)g_get);
            snprintf(set, sizeof set, "set%c%s:", n[0] >= 'a' && n[0] <= 'z' ? n[0] - 32 : n[0], n + 1);
            cx_add_method(k, set, (void *)g_set);
        }
    }
}
static void inits(cx_class_t *k, const char *const *sels) { for (int i = 0; sels[i]; i++) cx_add_method(k, sels[i], (void *)g_init); }
static void news(cx_class_t *k, const char *const *sels) {
    for (int i = 0; sels[i]; i++) cx_add_cmethod(k, sels[i], strchr(sels[i], ':') ? (void *)g_new : (void *)g_kind);
}

extern cx_class_t NSConstantString_c __asm__("__CFConstantStringClassReference");
#define CK(sym, val) static nsstr_t sym##_o = { &NSConstantString_c, 0, (char *)val, sizeof(val) - 1 }; id sym = (id)&sym##_o;
CK(ASAuthorizationScopeEmail, "email")
CK(ASAuthorizationScopeFullName, "full_name")
CK(ASExtensionErrorDomain, "ASExtensionErrorDomain")
CK(ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransportUSB, "usb")
CK(ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransportNFC, "nfc")
CK(ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransportBluetooth, "ble")
id ASAuthorizationAllSupportedPublicKeyCredentialDescriptorTransports(void) {
    id v[3] = {ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransportUSB, ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransportNFC,
               ASAuthorizationSecurityKeyPublicKeyCredentialDescriptorTransportBluetooth};
    return MSG(id, cls("NSArray"), "arrayWithObjects:count:", v, (u64)3);
}

/* ================================================================ Intents */
DEF(INIntent) DEF(INIntentResponse) DEF(INIntentResolutionResult) DEF(INStringResolutionResult) DEF(INObjectResolutionResult)
DEF(INEnumResolutionResult) DEF(INIntegerResolutionResult) DEF(INDateComponentsRangeResolutionResult) DEF(INPersonResolutionResult)
DEF(INTimeIntervalResolutionResult) DEF(INSendMessageRecipientResolutionResult) DEF(INStartCallCallCapabilityResolutionResult)
DEF(INStartCallContactResolutionResult) DEF(INObject) DEF(INObjectCollection) DEF(INSpeakableString) DEF(INMessage)
DEF(INCallRecord) DEF(INFocusStatusCenter) DEF(INVocabulary) DEF(INRelevantShortcut) DEF(INRelevantShortcutStore)
DEF(INRelevanceProvider) DEF(INDateRelevanceProvider) DEF(INExtension) DEF(INSendMessageIntentDonationMetadata)
DEF(INSendMessageIntentResponse) DEF(INSearchForMessagesIntentResponse) DEF(INSetMessageAttributeIntent)
DEF(INSetMessageAttributeIntentResponse) DEF(INStartAudioCallIntent) DEF(INStartAudioCallIntentResponse)
DEF(INStartVideoCallIntent) DEF(INStartVideoCallIntentResponse) DEF(INStartCallIntentResponse)
static id resp_init(id s, SEL c, long code, id activity) {
    (void)c;
    MSG(void, bag(s), "setObject:forKey:", cx_num_i(code), cx_str("code"));
    if (activity) MSG(void, bag(s), "setObject:forKey:", activity, cx_str("userActivity"));
    return s;
}
static id res_value(id k, SEL c, long v) { id o = g_new(k, c, 0, 0, 0, 0); MSG(void, bag(o), "setObject:forKey:", cx_num_i(v), cx_str("resolvedValue")); return o; }
static id res_interval(id k, SEL c, double v) { id o = g_new(k, c, 0, 0, 0, 0); MSG(void, bag(o), "setObject:forKey:", cx_num_d(v), cx_str("resolvedTimeInterval")); return o; }
static void in_set_image(id s, SEL c, id img, id param) {
    (void)c;
    char k[128];
    snprintf(k, sizeof k, "image.%s", param ? cx_cstr(param) : "");
    if (img) MSG(void, bag(s), "setObject:forKey:", img, cx_str(k)); else MSG(void, bag(s), "removeObjectForKey:", cx_str(k));
}
static id in_image(id s, SEL c, id param) {
    (void)c;
    char k[128];
    snprintf(k, sizeof k, "image.%s", param ? cx_cstr(param) : "");
    return MSG(id, bag(s), "objectForKey:", cx_str(k));
}
static id g_focus, g_vocab, g_shortcuts;
static id shared_of(id *slot, cx_class_t *k) { if (!*slot) *slot = objc_retain(MSG0(id, MSG0(id, (id)k, "alloc"), "init")); return *slot; }
static id focus_center(id k, SEL c) { (void)k; (void)c; return shared_of(&g_focus, &INFocusStatusCenter_c); }
static long focus_auth(id s, SEL c) { (void)s; (void)c; return 2; }            /* INFocusStatusAuthorizationStatusDenied */
typedef struct { void *isa; int flags, reserved; void (*invoke)(void *, long); } blk_l_t;
typedef struct { void *isa; int flags, reserved; void (*invoke)(void *, id); } blk_id_t;
static void focus_request(id s, SEL c, void *b) { (void)s; (void)c; if (b) ((blk_l_t *)b)->invoke(b, 2); }
static id vocab_shared(id k, SEL c) { (void)k; (void)c; return shared_of(&g_vocab, &INVocabulary_c); }
static void vocab_set(id s, SEL c, id strings, long type) {
    (void)c;
    char k[32];
    snprintf(k, sizeof k, "type.%ld", type);
    if (strings) MSG(void, bag(s), "setObject:forKey:", strings, cx_str(k)); else MSG(void, bag(s), "removeObjectForKey:", cx_str(k));
}
static void vocab_clear(id s, SEL c) { (void)c; MSG0(void, bag(s), "removeAllObjects"); }
static id shortcuts_store(id k, SEL c) { (void)k; (void)c; return shared_of(&g_shortcuts, &INRelevantShortcutStore_c); }
static void shortcuts_set(id s, SEL c, id list, void *b) {
    (void)c;
    if (list) MSG(void, bag(s), "setObject:forKey:", list, cx_str("relevantShortcuts"));
    if (b) ((blk_id_t *)b)->invoke(b, 0);
}
static id ext_handler(id s, SEL c, id intent) { (void)c; (void)intent; return s; }

/* ================================================================ AuthenticationServices */
DEF(ASAuthorizationRequest) DEF(ASAuthorizationAppleIDRequest) DEF(ASAuthorizationAppleIDProvider) DEF(ASAuthorizationAppleIDCredential)
DEF(ASAuthorizationSecurityKeyPublicKeyCredentialProvider) DEF(ASAuthorizationSecurityKeyPublicKeyCredentialAssertionRequest)
DEF(ASAuthorizationSecurityKeyPublicKeyCredentialRegistrationRequest) DEF(ASAuthorizationSecurityKeyPublicKeyCredentialAssertion)
DEF(ASAuthorizationSecurityKeyPublicKeyCredentialRegistration) DEF(ASAuthorizationSecurityKeyPublicKeyCredentialDescriptor)
DEF(ASAuthorizationPublicKeyCredentialParameters) DEF(ASAuthorizationWebBrowserPublicKeyCredentialManager)
DEF(ASCredentialServiceIdentifier) DEF(ASPasswordCredential) DEF(ASPasswordCredentialIdentity) DEF(ASPasswordCredentialRequest)
DEF(ASPasskeyAssertionCredential) DEF(ASPasskeyCredentialIdentity) DEF(ASPasskeyCredentialRequest) DEF(ASPasskeyRegistrationCredential)
DEF(ASCredentialProviderViewController) DEF(ASCredentialIdentityStore) DEF(ASCredentialIdentityStoreState) DEF(ASSettingsHelper)
DEF(ASWebAuthenticationSessionRequest)
static id err(const char *dom, long code, const char *msg) {
    id ui = MSG(id, cls("NSDictionary"), "dictionaryWithObject:forKey:", cx_str(msg), cx_str("NSLocalizedDescription"));
    return MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:", cx_str(dom), code, ui);
}
static id appleid_request(id s, SEL c) { (void)s; (void)c; return objc_autorelease(MSG0(id, MSG0(id, (id)&ASAuthorizationAppleIDRequest_c, "alloc"), "init")); }
typedef struct { void *isa; int flags, reserved; void (*invoke)(void *, long, id); } blk_lid_t;
static void appleid_state(id s, SEL c, id user, void *b) { (void)s; (void)c; (void)user; if (b) ((blk_lid_t *)b)->invoke(b, 2 /* NotFound */, 0); }
static id sk_assert_req(id s, SEL c, id challenge) {
    (void)c;
    id r = MSG0(id, MSG0(id, (id)&ASAuthorizationSecurityKeyPublicKeyCredentialAssertionRequest_c, "alloc"), "init");
    MSG(void, bag(r), "setObject:forKey:", challenge ? challenge : MSG0(id, cls("NSData"), "data"), cx_str("challenge"));
    id rp = MSG(id, bag(s), "objectForKey:", cx_str("relyingPartyIdentifier"));
    if (rp) MSG(void, bag(r), "setObject:forKey:", rp, cx_str("relyingPartyIdentifier"));
    return objc_autorelease(r);
}
static id sk_reg_req(id s, SEL c, id challenge, id display, id name, id user) {
    (void)c;
    id r = MSG0(id, MSG0(id, (id)&ASAuthorizationSecurityKeyPublicKeyCredentialRegistrationRequest_c, "alloc"), "init");
    id v[4] = {challenge, display, name, user};
    store_args(r, "challenge:displayName:name:userID:", v, 4);
    id rp = MSG(id, bag(s), "objectForKey:", cx_str("relyingPartyIdentifier"));
    if (rp) MSG(void, bag(r), "setObject:forKey:", rp, cx_str("relyingPartyIdentifier"));
    return objc_autorelease(r);
}
static id params_init(id s, SEL c, long alg) { (void)c; MSG(void, bag(s), "setObject:forKey:", cx_num_i(alg), cx_str("algorithm")); return s; }
static id svc_init(id s, SEL c, id ident, long type) {
    (void)c;
    if (ident) MSG(void, bag(s), "setObject:forKey:", ident, cx_str("identifier"));
    MSG(void, bag(s), "setObject:forKey:", cx_num_i(type), cx_str("type"));
    return s;
}
static long browser_state(id s, SEL c) { (void)s; (void)c; return 1; }        /* ...AuthorizationStateDenied */
static void browser_request(id s, SEL c, void *b) { (void)s; (void)c; if (b) ((blk_l_t *)b)->invoke(b, 1); }
static id g_ids;
static id ids_shared(id k, SEL c) { (void)k; (void)c; return shared_of(&g_ids, &ASCredentialIdentityStore_c); }
static void ids_state(id s, SEL c, void *b) {
    (void)s; (void)c;
    id st = objc_autorelease(MSG0(id, MSG0(id, (id)&ASCredentialIdentityStoreState_c, "alloc"), "init"));
    if (b) ((blk_id_t *)b)->invoke(b, st);                                       /* isEnabled = NO: nenhum provedor */
}
typedef struct { void *isa; int flags, reserved; void (*invoke)(void *, BOOL, id); } blk_bid_t;
static void ids_save(id s, SEL c, id list, void *b) {
    (void)s; (void)c; (void)list;
    if (b) ((blk_bid_t *)b)->invoke(b, NO, err("ASExtensionErrorDomain", 0, "o mOS não tem preenchimento automático de senhas"));
}
static void settings_open(id k, SEL c, void *b) {
    (void)k; (void)c;
    if (b) ((blk_id_t *)b)->invoke(b, err("ASExtensionErrorDomain", 0, "os ajustes de provedores de credenciais não existem no mOS"));
}
/* ASWebAuthenticationSessionRequest: o navegador completa ou cancela; o delegate do pedido fica sabendo */
static void wasr_complete(id s, SEL c, id url) {
    (void)c;
    id d = MSG(id, bag(s), "objectForKey:", cx_str("delegate"));
    if (url) MSG(void, bag(s), "setObject:forKey:", url, cx_str("callbackURL"));
    if (d && cx_responds(object_getClass(d), sel_registerName("authenticationSessionRequest:didCompleteWithCallbackURL:")))
        MSG(void, d, "authenticationSessionRequest:didCompleteWithCallbackURL:", s, url);
}
static void wasr_cancel(id s, SEL c, id e) {
    (void)c;
    id d = MSG(id, bag(s), "objectForKey:", cx_str("delegate"));
    if (d && cx_responds(object_getClass(d), sel_registerName("authenticationSessionRequest:didCancelWithError:")))
        MSG(void, d, "authenticationSessionRequest:didCancelWithError:", s, e);
}
/* ASAuthorizationController (classe de vscode_fwclasses.c): sem provedor no mOS, a autorização falha pelo delegate */
static void ac_deliver(void *p) {
    id ctl = p;
    id d = MSG(id, bag(ctl), "objectForKey:", cx_str("delegate"));
    if (d && cx_responds(object_getClass(d), sel_registerName("authorizationController:didCompleteWithError:")))
        MSG(void, d, "authorizationController:didCompleteWithError:", ctl,
            err("com.apple.AuthenticationServices.AuthorizationError", 1003 /* NotHandled */,
                "o mOS não tem Sign in with Apple, passkeys nem chaves de segurança"));
    objc_release(ctl);
}
static void ac_perform(id s, SEL c) { (void)c; dispatch_async_f(dispatch_get_main_queue(), objc_retain(s), ac_deliver); }
static id ac_init(id s, SEL c, id reqs) { (void)c; if (reqs) MSG(void, bag(s), "setObject:forKey:", reqs, cx_str("authorizationRequests")); return s; }
static void ac_cancel(id s, SEL c) { (void)s; (void)c; }

void cx_intents_auth_init(void) {
    cx_class_t *k;
    /* ---- Intents */
    k = def(&INIntent_c, &INIntent_m, "INIntent", "NSObject");
    props(k, (const char *const[]){"identifier", "intentDescription", "suggestedInvocationPhrase", "donationMetadata", "#shortcutAvailability", 0});
    cx_add_method(k, "setImage:forParameterNamed:", (void *)in_set_image);
    cx_add_method(k, "imageForParameterNamed:", (void *)in_image);
    k = def(&INIntentResponse_c, &INIntentResponse_m, "INIntentResponse", "NSObject");
    props(k, (const char *const[]){"userActivity", "#code", 0});
    cx_add_method(k, "initWithCode:userActivity:", (void *)resp_init);
    struct { cx_class_t *c, *m; const char *n, *s; } resp[] = {
        {&INSendMessageIntentResponse_c, &INSendMessageIntentResponse_m, "INSendMessageIntentResponse", "INIntentResponse"},
        {&INSearchForMessagesIntentResponse_c, &INSearchForMessagesIntentResponse_m, "INSearchForMessagesIntentResponse", "INIntentResponse"},
        {&INSetMessageAttributeIntentResponse_c, &INSetMessageAttributeIntentResponse_m, "INSetMessageAttributeIntentResponse", "INIntentResponse"},
        {&INStartAudioCallIntentResponse_c, &INStartAudioCallIntentResponse_m, "INStartAudioCallIntentResponse", "INIntentResponse"},
        {&INStartVideoCallIntentResponse_c, &INStartVideoCallIntentResponse_m, "INStartVideoCallIntentResponse", "INIntentResponse"},
        {&INStartCallIntentResponse_c, &INStartCallIntentResponse_m, "INStartCallIntentResponse", "INIntentResponse"},
        {&INSetMessageAttributeIntent_c, &INSetMessageAttributeIntent_m, "INSetMessageAttributeIntent", "INIntent"},
        {&INStartAudioCallIntent_c, &INStartAudioCallIntent_m, "INStartAudioCallIntent", "INIntent"},
        {&INStartVideoCallIntent_c, &INStartVideoCallIntent_m, "INStartVideoCallIntent", "INIntent"},
    };
    for (unsigned i = 0; i < sizeof resp / sizeof resp[0]; i++) {
        k = def(resp[i].c, resp[i].m, resp[i].n, resp[i].s);
        props(k, (const char *const[]){"messages", "contacts", "callRecords", "identifiers", "#attribute", "#destinationType", 0});
        inits(k, (const char *const[]){"initWithContacts:", "initWithIdentifiers:", 0});
    }
    k = def(&INIntentResolutionResult_c, &INIntentResolutionResult_m, "INIntentResolutionResult", "NSObject");
    news(k, (const char *const[]){"needsValue", "notRequired", "unsupported", 0});
    props(k, (const char *const[]){"_kind", 0});
    struct { cx_class_t *c, *m; const char *n; } rr[] = {
        {&INStringResolutionResult_c, &INStringResolutionResult_m, "INStringResolutionResult"},
        {&INObjectResolutionResult_c, &INObjectResolutionResult_m, "INObjectResolutionResult"},
        {&INEnumResolutionResult_c, &INEnumResolutionResult_m, "INEnumResolutionResult"},
        {&INIntegerResolutionResult_c, &INIntegerResolutionResult_m, "INIntegerResolutionResult"},
        {&INDateComponentsRangeResolutionResult_c, &INDateComponentsRangeResolutionResult_m, "INDateComponentsRangeResolutionResult"},
        {&INPersonResolutionResult_c, &INPersonResolutionResult_m, "INPersonResolutionResult"},
        {&INTimeIntervalResolutionResult_c, &INTimeIntervalResolutionResult_m, "INTimeIntervalResolutionResult"},
        {&INSendMessageRecipientResolutionResult_c, &INSendMessageRecipientResolutionResult_m, "INSendMessageRecipientResolutionResult"},
        {&INStartCallCallCapabilityResolutionResult_c, &INStartCallCallCapabilityResolutionResult_m, "INStartCallCallCapabilityResolutionResult"},
        {&INStartCallContactResolutionResult_c, &INStartCallContactResolutionResult_m, "INStartCallContactResolutionResult"},
    };
    for (unsigned i = 0; i < sizeof rr / sizeof rr[0]; i++) {
        k = def(rr[i].c, rr[i].m, rr[i].n, i >= 7 && i <= 9 ? (i == 7 ? "INPersonResolutionResult" : "INEnumResolutionResult") : "INIntentResolutionResult");
        news(k, (const char *const[]){"successWithResolvedString:", "successWithResolvedObject:", "successWithResolvedPerson:",
                                      "successWithResolvedDateComponentsRange:", "disambiguationWithStringsToDisambiguate:",
                                      "disambiguationWithObjectsToDisambiguate:", "disambiguationWithPeopleToDisambiguate:",
                                      "confirmationRequiredWithStringToConfirm:", "confirmationRequiredWithObjectToConfirm:",
                                      "confirmationRequiredWithPersonToConfirm:", 0});
        cx_add_cmethod(k, "successWithResolvedValue:", (void *)res_value);
        cx_add_cmethod(k, "confirmationRequiredWithValueToConfirm:", (void *)res_value);
        cx_add_cmethod(k, "successWithResolvedTimeInterval:", (void *)res_interval);
        props(k, (const char *const[]){"resolvedString", "resolvedObject", "resolvedPerson", "resolvedValue", "stringsToDisambiguate",
                                       "objectsToDisambiguate", "stringToConfirm", "objectToConfirm", 0});
    }
    k = def(&INObject_c, &INObject_m, "INObject", "NSObject");
    props(k, (const char *const[]){"identifier", "displayString", "pronunciationHint", "subtitleString", "displayImage", "alternativeSpeakableMatches", 0});
    inits(k, (const char *const[]){"initWithIdentifier:displayString:", "initWithIdentifier:displayString:pronunciationHint:",
                                   "initWithIdentifier:displayString:subtitleString:displayImage:", 0});
    k = def(&INObjectCollection_c, &INObjectCollection_m, "INObjectCollection", "NSObject");
    props(k, (const char *const[]){"items", "sections", "allItems", "?usesIndexedCollation", 0});
    inits(k, (const char *const[]){"initWithItems:", "initWithSections:", 0});
    k = def(&INSpeakableString_c, &INSpeakableString_m, "INSpeakableString", "NSObject");
    props(k, (const char *const[]){"spokenPhrase", "vocabularyIdentifier", "pronunciationHint", 0});
    inits(k, (const char *const[]){"initWithSpokenPhrase:", "initWithVocabularyIdentifier:spokenPhrase:pronunciationHint:", 0});
    k = def(&INMessage_c, &INMessage_m, "INMessage", "NSObject");
    props(k, (const char *const[]){"identifier", "content", "dateSent", "sender", "recipients", "groupName", "conversationIdentifier", 0});
    inits(k, (const char *const[]){"initWithIdentifier:content:dateSent:sender:recipients:", 0});
    k = def(&INCallRecord_c, &INCallRecord_m, "INCallRecord", "NSObject");
    props(k, (const char *const[]){"identifier", "dateCreated", "callDuration", "participants", "?unseen", 0});
    k = def(&INFocusStatusCenter_c, &INFocusStatusCenter_m, "INFocusStatusCenter", "NSObject");
    cx_add_cmethod(k, "defaultCenter", (void *)focus_center);
    cx_add_method(k, "authorizationStatus", (void *)focus_auth);
    cx_add_method(k, "requestAuthorizationWithCompletionHandler:", (void *)focus_request);
    props(k, (const char *const[]){"focusStatus", 0});
    k = def(&INVocabulary_c, &INVocabulary_m, "INVocabulary", "NSObject");
    cx_add_cmethod(k, "sharedVocabulary", (void *)vocab_shared);
    cx_add_method(k, "setVocabularyStrings:ofType:", (void *)vocab_set);
    cx_add_method(k, "setVocabulary:ofType:", (void *)vocab_set);
    cx_add_method(k, "removeAllVocabularyStrings", (void *)vocab_clear);
    k = def(&INRelevantShortcutStore_c, &INRelevantShortcutStore_m, "INRelevantShortcutStore", "NSObject");
    cx_add_cmethod(k, "defaultStore", (void *)shortcuts_store);
    cx_add_method(k, "setRelevantShortcuts:completionHandler:", (void *)shortcuts_set);
    k = def(&INRelevantShortcut_c, &INRelevantShortcut_m, "INRelevantShortcut", "NSObject");
    props(k, (const char *const[]){"shortcut", "relevanceProviders", "watchTemplate", "widgetKind", "#shortcutRole", 0});
    inits(k, (const char *const[]){"initWithShortcut:", 0});
    k = def(&INRelevanceProvider_c, &INRelevanceProvider_m, "INRelevanceProvider", "NSObject");
    k = def(&INDateRelevanceProvider_c, &INDateRelevanceProvider_m, "INDateRelevanceProvider", "INRelevanceProvider");
    props(k, (const char *const[]){"startDate", "endDate", 0});
    inits(k, (const char *const[]){"initWithStartDate:endDate:", 0});
    k = def(&INExtension_c, &INExtension_m, "INExtension", "NSObject");
    cx_add_method(k, "handlerForIntent:", (void *)ext_handler);
    k = def(&INSendMessageIntentDonationMetadata_c, &INSendMessageIntentDonationMetadata_m, "INSendMessageIntentDonationMetadata", "NSObject");
    props(k, (const char *const[]){"?mentionsCurrentUser", "?replyToCurrentUser", "?notifyRecipientAnyway", "#recipientCount", 0});

    /* ---- AuthenticationServices */
    k = def(&ASAuthorizationRequest_c, &ASAuthorizationRequest_m, "ASAuthorizationRequest", "NSObject");
    props(k, (const char *const[]){"provider", 0});
    k = def(&ASAuthorizationAppleIDRequest_c, &ASAuthorizationAppleIDRequest_m, "ASAuthorizationAppleIDRequest", "ASAuthorizationRequest");
    props(k, (const char *const[]){"requestedScopes", "nonce", "state", "user", "#requestedOperation", 0});
    k = def(&ASAuthorizationAppleIDProvider_c, &ASAuthorizationAppleIDProvider_m, "ASAuthorizationAppleIDProvider", "NSObject");
    cx_add_method(k, "createRequest", (void *)appleid_request);
    cx_add_method(k, "getCredentialStateForUserID:completion:", (void *)appleid_state);
    k = def(&ASAuthorizationAppleIDCredential_c, &ASAuthorizationAppleIDCredential_m, "ASAuthorizationAppleIDCredential", "NSObject");
    props(k, (const char *const[]){"user", "state", "authorizedScopes", "authorizationCode", "identityToken", "email", "fullName", "#realUserStatus", 0});
    k = def(&ASAuthorizationSecurityKeyPublicKeyCredentialProvider_c, &ASAuthorizationSecurityKeyPublicKeyCredentialProvider_m,
            "ASAuthorizationSecurityKeyPublicKeyCredentialProvider", "NSObject");
    props(k, (const char *const[]){"relyingPartyIdentifier", 0});
    inits(k, (const char *const[]){"initWithRelyingPartyIdentifier:", 0});
    cx_add_method(k, "createCredentialAssertionRequestWithChallenge:", (void *)sk_assert_req);
    cx_add_method(k, "createCredentialRegistrationRequestWithChallenge:displayName:name:userID:", (void *)sk_reg_req);
    const char *const req_props[] = {"challenge", "relyingPartyIdentifier", "displayName", "name", "userID", "allowedCredentials",
                                     "excludedCredentials", "credentialParameters", "userVerificationPreference", "attestationPreference",
                                     "residentKeyPreference", 0};
    k = def(&ASAuthorizationSecurityKeyPublicKeyCredentialAssertionRequest_c, &ASAuthorizationSecurityKeyPublicKeyCredentialAssertionRequest_m,
            "ASAuthorizationSecurityKeyPublicKeyCredentialAssertionRequest", "ASAuthorizationRequest");
    props(k, req_props);
    k = def(&ASAuthorizationSecurityKeyPublicKeyCredentialRegistrationRequest_c, &ASAuthorizationSecurityKeyPublicKeyCredentialRegistrationRequest_m,
            "ASAuthorizationSecurityKeyPublicKeyCredentialRegistrationRequest", "ASAuthorizationRequest");
    props(k, req_props);
    const char *const cred_props[] = {"rawClientDataJSON", "credentialID", "rawAuthenticatorData", "signature", "userID",
                                      "rawAttestationObject", "appID", "transports", "user", "password", "relyingParty",
                                      "serviceIdentifier", "recordIdentifier", "credentialIdentity", "clientDataHash", 0};
    struct { cx_class_t *c, *m; const char *n; } cr[] = {
        {&ASAuthorizationSecurityKeyPublicKeyCredentialAssertion_c, &ASAuthorizationSecurityKeyPublicKeyCredentialAssertion_m, "ASAuthorizationSecurityKeyPublicKeyCredentialAssertion"},
        {&ASAuthorizationSecurityKeyPublicKeyCredentialRegistration_c, &ASAuthorizationSecurityKeyPublicKeyCredentialRegistration_m, "ASAuthorizationSecurityKeyPublicKeyCredentialRegistration"},
        {&ASPasskeyAssertionCredential_c, &ASPasskeyAssertionCredential_m, "ASPasskeyAssertionCredential"},
        {&ASPasskeyRegistrationCredential_c, &ASPasskeyRegistrationCredential_m, "ASPasskeyRegistrationCredential"},
        {&ASPasskeyCredentialIdentity_c, &ASPasskeyCredentialIdentity_m, "ASPasskeyCredentialIdentity"},
        {&ASPasskeyCredentialRequest_c, &ASPasskeyCredentialRequest_m, "ASPasskeyCredentialRequest"},
        {&ASPasswordCredentialRequest_c, &ASPasswordCredentialRequest_m, "ASPasswordCredentialRequest"},
        {&ASPasswordCredential_c, &ASPasswordCredential_m, "ASPasswordCredential"},
        {&ASPasswordCredentialIdentity_c, &ASPasswordCredentialIdentity_m, "ASPasswordCredentialIdentity"},
    };
    for (unsigned i = 0; i < sizeof cr / sizeof cr[0]; i++) {
        k = def(cr[i].c, cr[i].m, cr[i].n, "NSObject");
        props(k, cred_props);
        inits(k, (const char *const[]){"initWithUser:password:", "initWithServiceIdentifier:user:recordIdentifier:",
                                       "initWithRelyingParty:userName:credentialID:userHandle:recordIdentifier:",
                                       "initWithUserHandle:relyingParty:signature:clientDataHash:authenticatorData:credentialID:",
                                       "initWithRelyingParty:clientDataHash:credentialID:attestationObject:", 0});
        cx_add_cmethod(k, "credentialWithUser:password:", (void *)g_new);
        cx_add_cmethod(k, "identityWithServiceIdentifier:user:recordIdentifier:", (void *)g_new);
    }
    k = def(&ASAuthorizationSecurityKeyPublicKeyCredentialDescriptor_c, &ASAuthorizationSecurityKeyPublicKeyCredentialDescriptor_m,
            "ASAuthorizationSecurityKeyPublicKeyCredentialDescriptor", "NSObject");
    props(k, (const char *const[]){"credentialID", "transports", 0});
    inits(k, (const char *const[]){"initWithCredentialID:transports:", 0});
    k = def(&ASAuthorizationPublicKeyCredentialParameters_c, &ASAuthorizationPublicKeyCredentialParameters_m, "ASAuthorizationPublicKeyCredentialParameters", "NSObject");
    props(k, (const char *const[]){"#algorithm", 0});
    cx_add_method(k, "initWithAlgorithm:", (void *)params_init);
    k = def(&ASAuthorizationWebBrowserPublicKeyCredentialManager_c, &ASAuthorizationWebBrowserPublicKeyCredentialManager_m,
            "ASAuthorizationWebBrowserPublicKeyCredentialManager", "NSObject");
    cx_add_method(k, "authorizationStateForPlatformCredentials", (void *)browser_state);
    cx_add_method(k, "requestAuthorizationForPublicKeyCredentials:", (void *)browser_request);
    k = def(&ASCredentialServiceIdentifier_c, &ASCredentialServiceIdentifier_m, "ASCredentialServiceIdentifier", "NSObject");
    props(k, (const char *const[]){"identifier", "#type", 0});
    cx_add_method(k, "initWithIdentifier:type:", (void *)svc_init);
    def(&ASCredentialProviderViewController_c, &ASCredentialProviderViewController_m, "ASCredentialProviderViewController", "NSViewController");
    k = def(&ASCredentialIdentityStore_c, &ASCredentialIdentityStore_m, "ASCredentialIdentityStore", "NSObject");
    cx_add_cmethod(k, "sharedStore", (void *)ids_shared);
    cx_add_method(k, "getCredentialIdentityStoreStateWithCompletion:", (void *)ids_state);
    cx_add_method(k, "saveCredentialIdentities:completion:", (void *)ids_save);
    cx_add_method(k, "replaceCredentialIdentitiesWithIdentities:completion:", (void *)ids_save);
    cx_add_method(k, "removeCredentialIdentities:completion:", (void *)ids_save);
    k = def(&ASCredentialIdentityStoreState_c, &ASCredentialIdentityStoreState_m, "ASCredentialIdentityStoreState", "NSObject");
    props(k, (const char *const[]){"?isEnabled", "?supportsIncrementalUpdates", 0});
    k = def(&ASSettingsHelper_c, &ASSettingsHelper_m, "ASSettingsHelper", "NSObject");
    cx_add_cmethod(k, "openCredentialProviderAppSettingsWithCompletionHandler:", (void *)settings_open);
    cx_add_cmethod(k, "openVerificationCodeAppSettingsWithCompletionHandler:", (void *)settings_open);
    k = def(&ASWebAuthenticationSessionRequest_c, &ASWebAuthenticationSessionRequest_m, "ASWebAuthenticationSessionRequest", "NSObject");
    props(k, (const char *const[]){"URL", "callbackURLScheme", "UUID", "delegate", "callback", "additionalHeaderFields", "?shouldUseEphemeralSession", 0});
    cx_add_method(k, "completeWithCallbackURL:", (void *)wasr_complete);
    cx_add_method(k, "cancelWithError:", (void *)wasr_cancel);
    /* ASAuthorizationController: estado no dicionário (o slot depois do isa da classe de vscode_fwclasses.c) */
    cx_class_t *ac = cx_class_named("ASAuthorizationController");
    if (ac) {
        cx_add_method(ac, "initWithAuthorizationRequests:", (void *)ac_init);
        props(ac, (const char *const[]){"authorizationRequests", "delegate", "presentationContextProvider", "customAuthorizationMethods", 0});
        cx_add_method(ac, "performRequests", (void *)ac_perform);
        cx_add_method(ac, "performAutoFillAssistedRequests", (void *)ac_perform);
        cx_add_method(ac, "cancel", (void *)ac_cancel);
    }
}
