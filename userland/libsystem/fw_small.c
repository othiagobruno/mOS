/*
 * mOS libSystem — frameworks pequenos que os apps do corpus ligam (docs/lacunas/natalia.md), com o comportamento que o
 * macOS tem quando o recurso não existe na máquina:
 *  - MetricKit: MXMetricManager com assinantes de verdade; o mOS não coleta métricas, então não há payloads passados.
 *  - SystemExtensions: pedidos de ativação falham com OSSystemExtensionErrorForbiddenBySystemPolicy (o mOS não carrega
 *    extensões de sistema); pedidos de propriedades devolvem a lista vazia.
 *  - OSAKit: OSAScript/OSALanguage sobre o NSAppleScript do mOS (applescript.c), com os erros nas chaves do OSAKit.
 *  - CoreLocation: CLRegion/CLCircularRegion (contém coordenada, por haversine), CLFloor, CLLocationSourceInformation.
 *  - LocalAuthentication: LARight sobre o LAContext do mOS (la_fw.c: sem biometria nem senha de aparelho).
 *  - DeviceCheck/App Attest, WatchConnectivity, PushKit: "não suportado" pelas APIs de cada um.
 *  - OSLog (OSLogEntry/OSLogEntryLog), EventKit (EKSource, EKEventStoreChangedNotification), CoreAnalytics.
 *  - constantes de UserNotifications, UniformTypeIdentifiers e OpenDirectory.
 */
#include "foundation.h"
#include "libc.h"
#include "la_eval.h"

id objc_autorelease(id o);
id objc_retain(id o);
void objc_release(id o);
void *os_log_create(const char *subsystem, const char *category);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
void *dispatch_get_main_queue(void);
void *_Block_copy(const void *b);
void _Block_release(const void *b);
double sqrt(double), sin(double), cos(double), atan2(double, double);

static id cls(const char *n) { return (id)cx_class_named(n); }
static id err(const char *dom, long code, const char *msg) {
    id ui = MSG(id, cls("NSDictionary"), "dictionaryWithObject:forKey:", cx_str(msg), cx_str("NSLocalizedDescription"));
    return MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:", cx_str(dom), code, ui);
}
#define DEF(name) cx_class_t name##_c __asm__("OBJC_CLASS_$_" #name); cx_class_t name##_m __asm__("OBJC_METACLASS_$_" #name);
static void def(cx_class_t *k, cx_class_t *m, const char *n, const char *super, u32 extra) {
    if (cx_class_named(n)) return;
    cx_class_t *s = cx_class_named(super);
    cx_define_class(k, m, n, s ? s : cx_class_named("NSObject"), extra);
}
/* estado de uma instância: um ponteiro para um struct, logo depois do isa */
#define ST(T, o) (*(T **)((u8 *)(o) + 8))

/* ---- constantes (CFString com o valor do macOS) */
extern cx_class_t NSConstantString_c __asm__("__CFConstantStringClassReference");
#define CK(sym, val) static nsstr_t sym##_o = { &NSConstantString_c, 0, (char *)val, sizeof(val) - 1 }; id sym = (id)&sym##_o;
CK(OSSystemExtensionErrorDomain, "OSSystemExtensionErrorDomain")
CK(EKEventStoreChangedNotification, "EKEventStoreChangedNotification")
CK(OSAScriptErrorMessageKey, "OSAScriptErrorMessageKey")
CK(OSAScriptErrorBriefMessageKey, "OSAScriptErrorBriefMessageKey")
CK(OSAScriptErrorNumberKey, "OSAScriptErrorNumberKey")
CK(OSAScriptErrorAppNameKey, "OSAScriptErrorAppNameKey")
CK(OSAScriptErrorRangeKey, "OSAScriptErrorRangeKey")
CK(UNNotificationAttachmentOptionsThumbnailClippingRectKey, "UNNotificationAttachmentOptionsThumbnailClippingRectKey")
CK(UNNotificationAttachmentOptionsThumbnailHiddenKey, "UNNotificationAttachmentOptionsThumbnailHiddenKey")
CK(kODAttributeTypeUserShell, "dsAttrTypeStandard:UserShell")
CK(kODAttributeTypeFullName, "dsAttrTypeStandard:RealName")
const double kCLLocationAccuracyReduced = 3000.0;
/* UTType *const: objetos UTType criados no início (a classe é de vscode_media.c) */
id UTTypeFramework, UTTypeResolvable, UTTypeTabSeparatedText;

/* ================================================================ MetricKit */
DEF(MXMetricManager) DEF(MXDiagnostic) DEF(MXCrashDiagnostic) DEF(MXHangDiagnostic) DEF(MXCPUExceptionDiagnostic)
DEF(MXDiskWriteExceptionDiagnostic) DEF(MXDiagnosticPayload) DEF(MXMetricPayload) DEF(MXHistogramBucket)
static id g_mx, g_mx_subs;
static id mx_shared(id k, SEL c) {
    (void)k; (void)c;
    if (!g_mx) { g_mx = objc_retain(MSG0(id, MSG0(id, (id)&MXMetricManager_c, "alloc"), "init")); g_mx_subs = objc_retain(MSG0(id, cls("NSMutableArray"), "array")); }
    return g_mx;
}
static void mx_add(id s, SEL c, id sub) { (void)s; (void)c; if (sub && !MSG(BOOL, g_mx_subs, "containsObject:", sub)) MSG(void, g_mx_subs, "addObject:", sub); }
static void mx_remove(id s, SEL c, id sub) { (void)s; (void)c; if (sub) MSG(void, g_mx_subs, "removeObject:", sub); }
static id mx_empty(id s, SEL c) { (void)s; (void)c; return MSG0(id, cls("NSArray"), "array"); }   /* nenhum payload coletado */
static void *mx_log(id k, SEL c, id cat) { (void)k; (void)c; return os_log_create("com.apple.metrickit.log", cat ? cx_cstr(cat) : ""); }
static BOOL mx_launch(id k, SEL c, id task, id *e) { (void)k; (void)c; (void)task; if (e) *e = 0; return YES; }
static id mx_json(id s, SEL c) { (void)s; (void)c; return MSG(id, cls("NSData"), "dataWithBytes:length:", "{}", (u64)2); }
static id mx_dict(id s, SEL c) { (void)s; (void)c; return MSG0(id, cls("NSDictionary"), "dictionary"); }
static id mx_nil(id s, SEL c) { (void)s; (void)c; return 0; }

/* ================================================================ SystemExtensions */
DEF(OSSystemExtensionManager) DEF(OSSystemExtensionRequest) DEF(OSSystemExtensionProperties)
typedef struct { id ident; void *queue; id delegate; int kind; } sxreq_t;   /* kind: 0 ativar, 1 desativar, 2 propriedades */
static id g_sx;
static id sx_shared(id k, SEL c) { (void)k; (void)c; if (!g_sx) g_sx = objc_retain(MSG0(id, MSG0(id, (id)&OSSystemExtensionManager_c, "alloc"), "init")); return g_sx; }
static id sx_make(int kind, id ident, void *queue) {
    id r = cx_alloc_instance(&OSSystemExtensionRequest_c);
    sxreq_t *s = calloc(1, sizeof *s);
    s->ident = ident ? objc_retain(ident) : 0; s->queue = queue; s->kind = kind;
    ST(sxreq_t, r) = s;
    return objc_autorelease(r);
}
static id sx_act(id k, SEL c, id ident, void *q) { (void)k; (void)c; return sx_make(0, ident, q); }
static id sx_deact(id k, SEL c, id ident, void *q) { (void)k; (void)c; return sx_make(1, ident, q); }
static id sx_props(id k, SEL c, id ident, void *q) { (void)k; (void)c; return sx_make(2, ident, q); }
static id sx_ident(id s, SEL c) { (void)c; sxreq_t *r = ST(sxreq_t, s); return r ? r->ident : 0; }
static id sx_delegate(id s, SEL c) { (void)c; sxreq_t *r = ST(sxreq_t, s); return r ? r->delegate : 0; }
static void sx_set_delegate(id s, SEL c, id d) { (void)c; sxreq_t *r = ST(sxreq_t, s); if (r) r->delegate = d; }   /* fraco, como na Apple */
static void sx_deliver(void *p) {
    id req = p;
    sxreq_t *r = ST(sxreq_t, req);
    id d = r ? r->delegate : 0;
    if (d && r->kind == 2 && cx_responds(object_getClass(d), sel_registerName("request:foundProperties:")))
        MSG(void, d, "request:foundProperties:", req, MSG0(id, cls("NSArray"), "array"));
    else if (d && cx_responds(object_getClass(d), sel_registerName("request:didFailWithError:")))
        MSG(void, d, "request:didFailWithError:", req,
            err("OSSystemExtensionErrorDomain", 10 /* ForbiddenBySystemPolicy */, "o mOS não carrega extensões de sistema"));
    objc_release(req);
}
static void sx_submit(id s, SEL c, id req) {
    (void)s; (void)c;
    sxreq_t *r = req ? ST(sxreq_t, req) : 0;
    if (!r) return;
    dispatch_async_f(r->queue ? r->queue : dispatch_get_main_queue(), objc_retain(req), sx_deliver);
}

/* ================================================================ OSAKit */
DEF(OSALanguage) DEF(OSAScript) DEF(OSAScriptController) DEF(OSAScriptView)
typedef struct { id source; id ns; } osa_t;
static id g_osa_as;
static id osa_lang_named(id k, SEL c, id name) {
    (void)k; (void)c;
    if (!name || strcmp(cx_cstr(name), "AppleScript")) return 0;             /* JavaScript for Automation não existe no mOS */
    if (!g_osa_as) g_osa_as = objc_retain(MSG0(id, MSG0(id, (id)&OSALanguage_c, "alloc"), "init"));
    return g_osa_as;
}
static id osa_lang_default(id k, SEL c) { return osa_lang_named(k, c, cx_str("AppleScript")); }
static id osa_lang_all(id k, SEL c) { return MSG(id, cls("NSArray"), "arrayWithObject:", osa_lang_default(k, c)); }
static id osa_lang_name(id s, SEL c) { (void)s; (void)c; return cx_str("AppleScript"); }
static id osa_init_src(id s, SEL c, id src) {
    (void)c;
    osa_t *o = calloc(1, sizeof *o);
    o->source = src ? objc_retain(src) : 0;
    o->ns = objc_retain(MSG(id, MSG0(id, cls("NSAppleScript"), "alloc"), "initWithSource:", src ? src : cx_str("")));
    ST(osa_t, s) = o;
    return s;
}
static id osa_init_src_lang(id s, SEL c, id src, id lang) { (void)lang; return osa_init_src(s, c, src); }
static id osa_source(id s, SEL c) { (void)c; osa_t *o = ST(osa_t, s); return o ? o->source : 0; }
/* erro do NSAppleScript -> chaves do OSAKit (mensagem, número, intervalo) */
static id osa_err(id ns) {
    if (!ns) return 0;
    id d = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    id m = MSG(id, ns, "objectForKey:", cx_str("NSAppleScriptErrorMessage"));
    id b = MSG(id, ns, "objectForKey:", cx_str("NSAppleScriptErrorBriefMessage"));
    id n = MSG(id, ns, "objectForKey:", cx_str("NSAppleScriptErrorNumber"));
    id r = MSG(id, ns, "objectForKey:", cx_str("NSAppleScriptErrorRange"));
    id a = MSG(id, ns, "objectForKey:", cx_str("NSAppleScriptErrorAppName"));
    if (m) MSG(void, d, "setObject:forKey:", m, OSAScriptErrorMessageKey);
    if (b) MSG(void, d, "setObject:forKey:", b, OSAScriptErrorBriefMessageKey);
    if (n) MSG(void, d, "setObject:forKey:", n, OSAScriptErrorNumberKey);
    if (r) MSG(void, d, "setObject:forKey:", r, OSAScriptErrorRangeKey);
    if (a) MSG(void, d, "setObject:forKey:", a, OSAScriptErrorAppNameKey);
    return d;
}
static BOOL osa_compile(id s, SEL c, id *e) {
    (void)c;
    osa_t *o = ST(osa_t, s);
    id ne = 0;
    BOOL ok = o && MSG(BOOL, o->ns, "compileAndReturnError:", &ne);
    if (e) *e = ok ? 0 : osa_err(ne);
    return ok;
}
static BOOL osa_compiled(id s, SEL c) { (void)c; osa_t *o = ST(osa_t, s); return o && MSG0(BOOL, o->ns, "isCompiled"); }
static id osa_exec(id s, SEL c, id *e) {
    (void)c;
    osa_t *o = ST(osa_t, s);
    id ne = 0;
    id r = o ? MSG(id, o->ns, "executeAndReturnError:", &ne) : 0;
    if (e) *e = r ? 0 : osa_err(ne);
    return r;
}
static id osa_exec_ev(id s, SEL c, id ev, id *e) {
    (void)c;
    osa_t *o = ST(osa_t, s);
    id ne = 0;
    id r = o ? MSG(id, o->ns, "executeAppleEvent:error:", ev, &ne) : 0;
    if (e) *e = r ? 0 : osa_err(ne);
    return r;
}

/* ================================================================ CoreLocation (regiões) */
DEF(CLRegion) DEF(CLCircularRegion) DEF(CLFloor) DEF(CLLocationSourceInformation)
typedef struct { double lat, lon, radius; id ident; BOOL entry, exit; long level; BOOL sim, acc; } cl_t;
static cl_t *cl(id s) { cl_t *r = ST(cl_t, s); if (!r) { r = calloc(1, sizeof *r); r->entry = r->exit = YES; ST(cl_t, s) = r; } return r; }
/* CLLocationCoordinate2D {double, double} vai em xmm0/xmm1 (SysV) */
static id clr_init(id s, SEL c, double lat, double lon, double radius, id ident) {
    (void)c;
    cl_t *r = cl(s);
    r->lat = lat; r->lon = lon; r->radius = radius < 0 ? 0 : radius; r->ident = ident ? objc_retain(ident) : 0;
    return s;
}
typedef struct { double lat, lon; } coord_t;
static coord_t clr_center(id s, SEL c) { (void)c; cl_t *r = cl(s); coord_t k = {r->lat, r->lon}; return k; }
static double clr_radius(id s, SEL c) { (void)c; return cl(s)->radius; }
static id clr_ident(id s, SEL c) { (void)c; return cl(s)->ident; }
static BOOL clr_entry(id s, SEL c) { (void)c; return cl(s)->entry; }
static BOOL clr_exit(id s, SEL c) { (void)c; return cl(s)->exit; }
static void clr_set_entry(id s, SEL c, BOOL v) { (void)c; cl(s)->entry = v; }
static void clr_set_exit(id s, SEL c, BOOL v) { (void)c; cl(s)->exit = v; }
static BOOL clr_contains(id s, SEL c, double lat, double lon) {
    (void)c;
    cl_t *r = cl(s);
    const double R = 6371008.8, d2r = 3.14159265358979323846 / 180.0;
    double dl = (lat - r->lat) * d2r, dg = (lon - r->lon) * d2r;
    double a = sin(dl / 2) * sin(dl / 2) + cos(r->lat * d2r) * cos(lat * d2r) * sin(dg / 2) * sin(dg / 2);
    return 2 * R * atan2(sqrt(a), sqrt(1 - a)) <= r->radius;
}
static long clf_level(id s, SEL c) { (void)c; return cl(s)->level; }
static id cls_init(id s, SEL c, BOOL sim, BOOL acc) { (void)c; cl_t *r = cl(s); r->sim = sim; r->acc = acc; return s; }
static BOOL cls_sim(id s, SEL c) { (void)c; return cl(s)->sim; }
static BOOL cls_acc(id s, SEL c) { (void)c; return cl(s)->acc; }

/* ================================================================ LocalAuthentication (LARight) */
DEF(LARight) DEF(LAAuthenticationRequirement) DEF(LABiometryFallbackRequirement)
typedef struct { void *isa; int flags, reserved; void (*invoke)(void *, id); } la_done_t;
static id la_req_default(id k, SEL c) { (void)c; return objc_autorelease(MSG0(id, MSG0(id, k, "alloc"), "init")); }
static void lar_authorize(id s, SEL c, id reason, void *blk) {
    (void)s; (void)c; (void)reason;
    int code = 0;
    const char *dom = 0;
    int ok = cx_la_evaluate_policy(2 /* deviceOwnerAuthentication */, &code, &dom);
    la_done_t *b = blk;
    if (b && b->invoke) b->invoke(blk, ok ? 0 : err(dom ? dom : "com.apple.LocalAuthentication", code, "o mOS não tem biometria nem senha de aparelho"));
}
static long lar_state(id s, SEL c) { (void)s; (void)c; return 2; }            /* LARightStateNotAuthorized */

/* ================================================================ DeviceCheck, WatchConnectivity, PushKit */
DEF(DCAppAttestService) DEF(WCSession) DEF(PKPushRegistry)
static id g_dc, g_wc;
static id dc_shared(id k, SEL c) { (void)k; (void)c; if (!g_dc) g_dc = objc_retain(MSG0(id, MSG0(id, (id)&DCAppAttestService_c, "alloc"), "init")); return g_dc; }
static BOOL no_(id s, SEL c) { (void)s; (void)c; return NO; }
typedef struct { void *isa; int flags, reserved; void (*invoke)(void *, id, id); } dc_done_t;
static void dc_fail(id s, SEL c, void *blk) {           /* generateKeyWithCompletionHandler: (DCErrorFeatureUnsupported = 1) */
    (void)s; (void)c;
    dc_done_t *b = blk;
    if (b && b->invoke) b->invoke(blk, 0, err("com.apple.devicecheck.error", 1, "App Attest não existe no mOS"));
}
static void dc_fail2(id s, SEL c, id a, id d, void *blk) { (void)a; (void)d; dc_fail(s, c, blk); }
static id wc_default(id k, SEL c) { (void)k; (void)c; if (!g_wc) g_wc = objc_retain(MSG0(id, MSG0(id, (id)&WCSession_c, "alloc"), "init")); return g_wc; }
typedef struct { void *queue; id delegate; id types; } pk_t;
static id pk_init(id s, SEL c, void *q) { (void)c; pk_t *p = calloc(1, sizeof *p); p->queue = q; ST(pk_t, s) = p; return s; }
static id pk_delegate(id s, SEL c) { (void)c; pk_t *p = ST(pk_t, s); return p ? p->delegate : 0; }
static void pk_set_delegate(id s, SEL c, id d) { (void)c; pk_t *p = ST(pk_t, s); if (p) p->delegate = d; }
static id pk_types(id s, SEL c) { (void)c; pk_t *p = ST(pk_t, s); return p ? p->types : 0; }
static void pk_set_types(id s, SEL c, id t) { (void)c; pk_t *p = ST(pk_t, s); if (p) { if (p->types) objc_release(p->types); p->types = t ? objc_retain(t) : 0; } }
static id pk_token(id s, SEL c, id type) { (void)s; (void)c; (void)type; return 0; }   /* sem serviço de push: nunca há token */

/* ================================================================ OSLog, EventKit */
DEF(OSLogEntry) DEF(OSLogEntryLog) DEF(EKSource)
/* CoreAnalytics: o mOS não envia análises; o bloco que montaria o evento não é chamado */
void AnalyticsSendEventLazy(id name, void *block) { (void)name; (void)block; }

static void m(cx_class_t *k, const char *s, void *f) { cx_add_method(k, s, f); }
static void cm(cx_class_t *k, const char *s, void *f) { cx_add_cmethod(k, s, f); }
void cx_fw_small_init(void) {
    def(&MXMetricManager_c, &MXMetricManager_m, "MXMetricManager", "NSObject", 8);
    cm(&MXMetricManager_c, "sharedManager", (void *)mx_shared);
    cm(&MXMetricManager_c, "makeLogHandleWithCategory:", (void *)mx_log);
    cm(&MXMetricManager_c, "extendLaunchMeasurementForTaskID:error:", (void *)mx_launch);
    cm(&MXMetricManager_c, "finishExtendedLaunchMeasurementForTaskID:error:", (void *)mx_launch);
    m(&MXMetricManager_c, "addSubscriber:", (void *)mx_add);
    m(&MXMetricManager_c, "removeSubscriber:", (void *)mx_remove);
    m(&MXMetricManager_c, "pastPayloads", (void *)mx_empty);
    m(&MXMetricManager_c, "pastDiagnosticPayloads", (void *)mx_empty);
    def(&MXDiagnostic_c, &MXDiagnostic_m, "MXDiagnostic", "NSObject", 8);
    def(&MXCrashDiagnostic_c, &MXCrashDiagnostic_m, "MXCrashDiagnostic", "MXDiagnostic", 8);
    def(&MXHangDiagnostic_c, &MXHangDiagnostic_m, "MXHangDiagnostic", "MXDiagnostic", 8);
    def(&MXCPUExceptionDiagnostic_c, &MXCPUExceptionDiagnostic_m, "MXCPUExceptionDiagnostic", "MXDiagnostic", 8);
    def(&MXDiskWriteExceptionDiagnostic_c, &MXDiskWriteExceptionDiagnostic_m, "MXDiskWriteExceptionDiagnostic", "MXDiagnostic", 8);
    def(&MXDiagnosticPayload_c, &MXDiagnosticPayload_m, "MXDiagnosticPayload", "NSObject", 8);
    def(&MXMetricPayload_c, &MXMetricPayload_m, "MXMetricPayload", "NSObject", 8);
    def(&MXHistogramBucket_c, &MXHistogramBucket_m, "MXHistogramBucket", "NSObject", 8);
    cx_class_t *json[] = {&MXDiagnostic_c, &MXDiagnosticPayload_c, &MXMetricPayload_c};
    for (int i = 0; i < 3; i++) { m(json[i], "JSONRepresentation", (void *)mx_json); m(json[i], "dictionaryRepresentation", (void *)mx_dict); }
    static const char *diag_props[] = {"callStackTree", "exceptionType", "exceptionCode", "signal", "terminationReason",
                                       "virtualMemoryRegionInfo", "exceptionReason", "hangDuration", "totalCPUTime",
                                       "totalSampledTime", "totalWritesCaused", "metaData", "applicationVersion", 0};
    for (int i = 0; diag_props[i]; i++) m(&MXDiagnostic_c, diag_props[i], (void *)mx_nil);
    static const char *pay_props[] = {"crashDiagnostics", "hangDiagnostics", "cpuExceptionDiagnostics", "diskWriteExceptionDiagnostics",
                                      "appLaunchDiagnostics", "timeStampBegin", "timeStampEnd", 0};
    for (int i = 0; pay_props[i]; i++) m(&MXDiagnosticPayload_c, pay_props[i], (void *)mx_nil);

    def(&OSSystemExtensionManager_c, &OSSystemExtensionManager_m, "OSSystemExtensionManager", "NSObject", 8);
    cm(&OSSystemExtensionManager_c, "sharedManager", (void *)sx_shared);
    m(&OSSystemExtensionManager_c, "submitRequest:", (void *)sx_submit);
    def(&OSSystemExtensionRequest_c, &OSSystemExtensionRequest_m, "OSSystemExtensionRequest", "NSObject", 8);
    cm(&OSSystemExtensionRequest_c, "activationRequestForExtension:queue:", (void *)sx_act);
    cm(&OSSystemExtensionRequest_c, "deactivationRequestForExtension:queue:", (void *)sx_deact);
    cm(&OSSystemExtensionRequest_c, "propertiesRequestForExtension:queue:", (void *)sx_props);
    m(&OSSystemExtensionRequest_c, "identifier", (void *)sx_ident);
    m(&OSSystemExtensionRequest_c, "delegate", (void *)sx_delegate);
    m(&OSSystemExtensionRequest_c, "setDelegate:", (void *)sx_set_delegate);
    def(&OSSystemExtensionProperties_c, &OSSystemExtensionProperties_m, "OSSystemExtensionProperties", "NSObject", 8);
    static const char *sxp[] = {"URL", "bundleIdentifier", "bundleVersion", "bundleShortVersion", 0};
    for (int i = 0; sxp[i]; i++) m(&OSSystemExtensionProperties_c, sxp[i], (void *)mx_nil);
    m(&OSSystemExtensionProperties_c, "isEnabled", (void *)no_);
    m(&OSSystemExtensionProperties_c, "isAwaitingUserApproval", (void *)no_);
    m(&OSSystemExtensionProperties_c, "isUninstalling", (void *)no_);

    def(&OSALanguage_c, &OSALanguage_m, "OSALanguage", "NSObject", 8);
    cm(&OSALanguage_c, "languageForName:", (void *)osa_lang_named);
    cm(&OSALanguage_c, "defaultLanguage", (void *)osa_lang_default);
    cm(&OSALanguage_c, "availableLanguages", (void *)osa_lang_all);
    m(&OSALanguage_c, "name", (void *)osa_lang_name);
    def(&OSAScript_c, &OSAScript_m, "OSAScript", "NSObject", 8);
    m(&OSAScript_c, "initWithSource:", (void *)osa_init_src);
    m(&OSAScript_c, "initWithSource:language:", (void *)osa_init_src_lang);
    m(&OSAScript_c, "source", (void *)osa_source);
    m(&OSAScript_c, "compileAndReturnError:", (void *)osa_compile);
    m(&OSAScript_c, "isCompiled", (void *)osa_compiled);
    m(&OSAScript_c, "executeAndReturnError:", (void *)osa_exec);
    m(&OSAScript_c, "executeAppleEvent:error:", (void *)osa_exec_ev);
    def(&OSAScriptController_c, &OSAScriptController_m, "OSAScriptController", "NSController", 8);
    def(&OSAScriptView_c, &OSAScriptView_m, "OSAScriptView", "NSTextView", 8);

    def(&CLRegion_c, &CLRegion_m, "CLRegion", "NSObject", 8);
    def(&CLCircularRegion_c, &CLCircularRegion_m, "CLCircularRegion", "CLRegion", 8);
    m(&CLCircularRegion_c, "initWithCenter:radius:identifier:", (void *)clr_init);
    m(&CLCircularRegion_c, "center", (void *)clr_center);
    m(&CLCircularRegion_c, "radius", (void *)clr_radius);
    m(&CLCircularRegion_c, "containsCoordinate:", (void *)clr_contains);
    m(&CLRegion_c, "identifier", (void *)clr_ident);
    m(&CLRegion_c, "notifyOnEntry", (void *)clr_entry);
    m(&CLRegion_c, "notifyOnExit", (void *)clr_exit);
    m(&CLRegion_c, "setNotifyOnEntry:", (void *)clr_set_entry);
    m(&CLRegion_c, "setNotifyOnExit:", (void *)clr_set_exit);
    def(&CLFloor_c, &CLFloor_m, "CLFloor", "NSObject", 8);
    m(&CLFloor_c, "level", (void *)clf_level);
    def(&CLLocationSourceInformation_c, &CLLocationSourceInformation_m, "CLLocationSourceInformation", "NSObject", 8);
    m(&CLLocationSourceInformation_c, "initWithSoftwareSimulationState:andExternalAccessoryState:", (void *)cls_init);
    m(&CLLocationSourceInformation_c, "isSimulatedBySoftware", (void *)cls_sim);
    m(&CLLocationSourceInformation_c, "isProducedByAccessory", (void *)cls_acc);

    def(&LARight_c, &LARight_m, "LARight", "NSObject", 8);
    m(&LARight_c, "authorizeWithLocalizedReason:completion:", (void *)lar_authorize);
    m(&LARight_c, "state", (void *)lar_state);
    def(&LAAuthenticationRequirement_c, &LAAuthenticationRequirement_m, "LAAuthenticationRequirement", "NSObject", 8);
    cm(&LAAuthenticationRequirement_c, "defaultRequirement", (void *)la_req_default);
    cm(&LAAuthenticationRequirement_c, "biometryRequirement", (void *)la_req_default);
    cm(&LAAuthenticationRequirement_c, "biometryCurrentSetRequirement", (void *)la_req_default);
    def(&LABiometryFallbackRequirement_c, &LABiometryFallbackRequirement_m, "LABiometryFallbackRequirement", "NSObject", 8);
    cm(&LABiometryFallbackRequirement_c, "defaultRequirement", (void *)la_req_default);
    cm(&LABiometryFallbackRequirement_c, "devicePasscodeRequirement", (void *)la_req_default);

    def(&DCAppAttestService_c, &DCAppAttestService_m, "DCAppAttestService", "NSObject", 8);
    cm(&DCAppAttestService_c, "sharedService", (void *)dc_shared);
    m(&DCAppAttestService_c, "isSupported", (void *)no_);
    m(&DCAppAttestService_c, "generateKeyWithCompletionHandler:", (void *)dc_fail);
    m(&DCAppAttestService_c, "attestKey:clientDataHash:completionHandler:", (void *)dc_fail2);
    m(&DCAppAttestService_c, "generateAssertion:clientDataHash:completionHandler:", (void *)dc_fail2);
    def(&WCSession_c, &WCSession_m, "WCSession", "NSObject", 8);
    cm(&WCSession_c, "isSupported", (void *)no_);
    cm(&WCSession_c, "defaultSession", (void *)wc_default);
    m(&WCSession_c, "isPaired", (void *)no_);
    m(&WCSession_c, "isReachable", (void *)no_);
    def(&PKPushRegistry_c, &PKPushRegistry_m, "PKPushRegistry", "NSObject", 8);
    m(&PKPushRegistry_c, "initWithQueue:", (void *)pk_init);
    m(&PKPushRegistry_c, "delegate", (void *)pk_delegate);
    m(&PKPushRegistry_c, "setDelegate:", (void *)pk_set_delegate);
    m(&PKPushRegistry_c, "desiredPushTypes", (void *)pk_types);
    m(&PKPushRegistry_c, "setDesiredPushTypes:", (void *)pk_set_types);
    m(&PKPushRegistry_c, "pushTokenForType:", (void *)pk_token);

    def(&OSLogEntry_c, &OSLogEntry_m, "OSLogEntry", "NSObject", 8);
    def(&OSLogEntryLog_c, &OSLogEntryLog_m, "OSLogEntryLog", "OSLogEntry", 8);
    static const char *le[] = {"composedMessage", "date", "subsystem", "category", "process", "sender", 0};
    for (int i = 0; le[i]; i++) m(&OSLogEntry_c, le[i], (void *)mx_nil);
    def(&EKSource_c, &EKSource_m, "EKSource", "NSObject", 8);
    m(&EKSource_c, "title", (void *)mx_nil);
    m(&EKSource_c, "sourceIdentifier", (void *)mx_nil);

    id ut = cls("UTType");
    if (ut) {
        UTTypeFramework = objc_retain(MSG(id, ut, "typeWithIdentifier:", cx_str("com.apple.framework")));
        UTTypeResolvable = objc_retain(MSG(id, ut, "typeWithIdentifier:", cx_str("com.apple.resolvable")));
        UTTypeTabSeparatedText = objc_retain(MSG(id, ut, "typeWithIdentifier:", cx_str("public.utf8-tab-separated-values-text")));
    }
}
