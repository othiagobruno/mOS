/* mOS — UserNotifications.framework sobre o centro local (un_center.c).
 *
 * Classes: UNUserNotificationCenter, UNNotificationContent/UNMutableNotificationContent, UNNotificationRequest,
 * UNNotification, UNNotificationResponse/UNTextInputNotificationResponse, UNNotificationSound,
 * UNNotificationAttachment, UNNotificationAction/UNTextInputNotificationAction/UNNotificationActionIcon,
 * UNNotificationCategory, gatilhos (intervalo, calendário, push) e UNNotificationSettings.
 * As propriedades de cada objeto ficam num dicionário associado (somem com o objeto).
 *
 * Autorização: o serviço "userNotifications" do TCC, com o diálogo da Interface; .provisional autoriza sem
 * perguntar, como no macOS. Pedidos pendentes e entregues vão para
 * ~/Library/Application Support/com.mos.usernotifications/<bundle>.txt: o que venceu com o app fechado é entregue
 * quando ele abre. A entrega e os delegates rodam no fio principal (timer do dispatch); o banner é o da Interface
 * (SYS_DESKTOP MOS_DESK_BANNER_EX: título, subtítulo, corpo e até 4 ações, com resposta em texto e "fechar"),
 * MOS_DESK_TAKE_EX traz o clique e MOS_DESK_REMOVE tira da Central. Badge: MOS_DESK_BADGE.
 * Anexos: validados (existe, tipo imagem/áudio/vídeo, tamanho) e movidos para a pasta de anexos do app.
 * Não há APNs: registerForRemoteNotifications falha com UNErrorDomain. Teste de host: tests-appkit/un_host.c. */
#include "foundation.h"
#include "libc.h"
#include "sys.h"
#include "un_center.h"

void *_Block_copy(const void *b);
void _Block_release(const void *b);
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void *objc_autoreleasePoolPush(void);
void objc_autoreleasePoolPop(void *p);
void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy);
id objc_getAssociatedObject(id obj, const void *key);
int cx_tcc_state(const char *service);
int cx_tcc_request(const char *service);
void cx_tcc_request_async(const char *service, void (*cb)(int granted, void *ctx), void *ctx);
int cx_snprintf(char *buf, unsigned long n, const char *fmt, ...);
int pthread_main_np(void);
unsigned long pthread_self(void);
void *dispatch_get_main_queue(void);
void *dispatch_get_global_queue(long prio, unsigned long flags);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
void dispatch_after_f(u64 when, void *q, void *ctx, void (*fn)(void *));
u64 dispatch_time(u64 when, i64 delta);
int access(const char *path, int mode);
int rename(const char *a, const char *b);
int cx_ls_type_of_file(const char *path, char *out, size_t cap);
int cx_ls_conforms(const char *a, const char *b);
id NSHomeDirectory(void);

extern cx_class_t UNMutableNotificationContent_c __asm__("OBJC_CLASS_$_UNMutableNotificationContent");
extern cx_class_t UNMutableNotificationContent_m __asm__("OBJC_METACLASS_$_UNMutableNotificationContent");
extern cx_class_t UNNotificationRequest_c __asm__("OBJC_CLASS_$_UNNotificationRequest");
extern cx_class_t UNUserNotificationCenter_c __asm__("OBJC_CLASS_$_UNUserNotificationCenter");
extern cx_class_t UNNotificationAction_c __asm__("OBJC_CLASS_$_UNNotificationAction");
extern cx_class_t UNNotificationAttachment_c __asm__("OBJC_CLASS_$_UNNotificationAttachment");
extern cx_class_t UNNotificationCategory_c __asm__("OBJC_CLASS_$_UNNotificationCategory");
extern cx_class_t UNNotificationSound_c __asm__("OBJC_CLASS_$_UNNotificationSound");
extern cx_class_t UNTextInputNotificationResponse_c __asm__("OBJC_CLASS_$_UNTextInputNotificationResponse");
extern cx_class_t UNTextInputNotificationResponse_m __asm__("OBJC_METACLASS_$_UNTextInputNotificationResponse");
extern cx_class_t UNTextInputNotificationAction_c __asm__("OBJC_CLASS_$_UNTextInputNotificationAction");

cx_class_t UNNotificationContent_c __asm__("OBJC_CLASS_$_UNNotificationContent");
cx_class_t UNNotificationContent_m __asm__("OBJC_METACLASS_$_UNNotificationContent");
cx_class_t UNNotificationTrigger_c __asm__("OBJC_CLASS_$_UNNotificationTrigger");
cx_class_t UNNotificationTrigger_m __asm__("OBJC_METACLASS_$_UNNotificationTrigger");
cx_class_t UNTimeIntervalNotificationTrigger_c __asm__("OBJC_CLASS_$_UNTimeIntervalNotificationTrigger");
cx_class_t UNTimeIntervalNotificationTrigger_m __asm__("OBJC_METACLASS_$_UNTimeIntervalNotificationTrigger");
cx_class_t UNNotification_c __asm__("OBJC_CLASS_$_UNNotification");
cx_class_t UNNotification_m __asm__("OBJC_METACLASS_$_UNNotification");
cx_class_t UNNotificationResponse_c __asm__("OBJC_CLASS_$_UNNotificationResponse");
cx_class_t UNNotificationResponse_m __asm__("OBJC_METACLASS_$_UNNotificationResponse");
cx_class_t UNNotificationSettings_c __asm__("OBJC_CLASS_$_UNNotificationSettings");
cx_class_t UNNotificationSettings_m __asm__("OBJC_METACLASS_$_UNNotificationSettings");
cx_class_t UNNotificationActionIcon_c __asm__("OBJC_CLASS_$_UNNotificationActionIcon");
cx_class_t UNNotificationActionIcon_m __asm__("OBJC_METACLASS_$_UNNotificationActionIcon");
cx_class_t UNPushNotificationTrigger_c __asm__("OBJC_CLASS_$_UNPushNotificationTrigger");
cx_class_t UNPushNotificationTrigger_m __asm__("OBJC_METACLASS_$_UNPushNotificationTrigger");
cx_class_t UNCalendarNotificationTrigger_c __asm__("OBJC_CLASS_$_UNCalendarNotificationTrigger");
cx_class_t UNCalendarNotificationTrigger_m __asm__("OBJC_METACLASS_$_UNCalendarNotificationTrigger");
cx_class_t UNNotificationServiceExtension_c __asm__("OBJC_CLASS_$_UNNotificationServiceExtension");
cx_class_t UNNotificationServiceExtension_m __asm__("OBJC_METACLASS_$_UNNotificationServiceExtension");

static id cls(const char *n) { return (id)cx_class_named(n); }
static BOOL responds(id o, const char *sel) { return o && cx_responds(object_getClass(o), sel_registerName(sel)); }
static const char *cs(id s) { return s && cx_is_str(s) ? cx_cstr(s) : 0; }

/* ---------------------------------------------------------------- trava do centro (recursiva: o delegate pode chamar o centro) */
static volatile int g_lk;
static unsigned long g_owner;
static int g_depth;
static void lock(void) {
    unsigned long me = pthread_self();
    if (g_owner == me && g_depth) { g_depth++; return; }
    while (__sync_lock_test_and_set(&g_lk, 1)) ;
    g_owner = me;
    g_depth = 1;
}
static void unlock(void) {
    if (--g_depth) return;
    g_owner = 0;
    __sync_lock_release(&g_lk);
}

/* ---------------------------------------------------------------- propriedades (dicionário associado) */
static char k_props;
static id props(id o) {
    id d = objc_getAssociatedObject(o, &k_props);
    if (!d) {
        d = MSG0(id, MSG0(id, cls("NSMutableDictionary"), "alloc"), "init");
        objc_setAssociatedObject(o, &k_props, d, 1);
        objc_release(d);
    }
    return d;
}
static id pget(id o, const char *k) { id d = o ? objc_getAssociatedObject(o, &k_props) : 0; return d ? MSG(id, d, "objectForKey:", cx_str(k)) : 0; }
static void pset(id o, const char *k, id v) {
    id d = props(o);
    if (v) MSG(void, d, "setObject:forKey:", v, cx_str(k)); else MSG(void, d, "removeObjectForKey:", cx_str(k));
}
static void pset_copy(id o, const char *k, id v) { id c = v ? MSG0(id, v, "copy") : 0; pset(o, k, c); if (c) objc_release(c); }
static id num_u64(u64 v) { return MSG(id, cls("NSNumber"), "numberWithUnsignedLongLong:", v); }
static id num_dbl(double v) { return MSG(id, cls("NSNumber"), "numberWithDouble:", v); }
static u64 pget_u64(id o, const char *k) { id n = pget(o, k); return n ? MSG0(u64, n, "unsignedLongLongValue") : 0; }
static double pget_dbl(id o, const char *k) { id n = pget(o, k); return n ? MSG0(double, n, "doubleValue") : 0; }
/* chave da propriedade pelo seletor: "title" e "setTitle:" → "title" */
static void key_of(SEL s, char *out, size_t cap) {
    const char *n = s;
    if (!strncmp(n, "set", 3) && n[3] >= 'A' && n[3] <= 'Z') {
        size_t l = strlen(n + 3);
        if (l && n[3 + l - 1] == ':') l--;
        if (l >= cap) l = cap - 1;
        memcpy(out, n + 3, l);
        out[l] = 0;
        out[0] = (char)(out[0] + 32);
        return;
    }
    cx_snprintf(out, cap, "%s", n);
}
static id g_obj(id self, SEL c) { char k[64]; key_of(c, k, sizeof k); return pget(self, k); }
static id g_str(id self, SEL c) { char k[64]; key_of(c, k, sizeof k); id v = pget(self, k); return v ? v : cx_str(""); }
static id g_arr(id self, SEL c) { char k[64]; key_of(c, k, sizeof k); id v = pget(self, k); return v ? v : MSG0(id, cls("NSArray"), "array"); }
static id g_dict(id self, SEL c) { char k[64]; key_of(c, k, sizeof k); id v = pget(self, k); return v ? v : MSG0(id, cls("NSDictionary"), "dictionary"); }
static u64 g_u64(id self, SEL c) { char k[64]; key_of(c, k, sizeof k); return pget_u64(self, k); }
static double g_dbl(id self, SEL c) { char k[64]; key_of(c, k, sizeof k); return pget_dbl(self, k); }
static BOOL g_bool(id self, SEL c) { char k[64]; key_of(c, k, sizeof k); return pget_u64(self, k) ? YES : NO; }
static void s_copy(id self, SEL c, id v) { char k[64]; key_of(c, k, sizeof k); pset_copy(self, k, v); }
static void s_u64(id self, SEL c, u64 v) { char k[64]; key_of(c, k, sizeof k); pset(self, k, num_u64(v)); }
static void s_dbl(id self, SEL c, double v) { char k[64]; key_of(c, k, sizeof k); pset(self, k, num_dbl(v)); }

static id new_of(cx_class_t *k) { return MSG0(id, MSG0(id, (id)k, "alloc"), "init"); }   /* +1 */
static id copy_props_to(id src, cx_class_t *k) {                                         /* +1 */
    id o = new_of(k);
    id d = objc_getAssociatedObject(src, &k_props);
    if (d) { id m = MSG0(id, d, "mutableCopy"); objc_setAssociatedObject(o, &k_props, m, 1); objc_release(m); }
    return o;
}
static id ct_copy(id self, SEL c) { (void)c; return copy_props_to(self, &UNNotificationContent_c); }
static id ct_mcopy(id self, SEL c) { (void)c; return copy_props_to(self, &UNMutableNotificationContent_c); }
static id same_copy(id self, SEL c) { (void)c; return objc_retain(self); }       /* imutáveis */
static BOOL ct_equal(id self, SEL c, id o) {
    (void)c;
    if (self == o) return YES;
    id a = objc_getAssociatedObject(self, &k_props), b = o ? objc_getAssociatedObject(o, &k_props) : 0;
    if (!a || !b) return !a && !b;
    return MSG(BOOL, a, "isEqualToDictionary:", b);
}

/* ---------------------------------------------------------------- erros */
static id un_error(long code, const char *msg) {
    id ui = MSG(id, cls("NSDictionary"), "dictionaryWithObject:forKey:", cx_str(msg), cx_str("NSLocalizedDescription"));
    return MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:", cx_str("UNErrorDomain"), code, ui);
}
enum { UNErrNotAllowed = 1, UNErrAttachInvalidURL = 100, UNErrAttachType = 101, UNErrAttachSize = 102, UNErrAttachMove = 104,
       UNErrBadTrigger = 1401 };

/* ---------------------------------------------------------------- blocos */
typedef struct { void *isa; int flags, reserved; void *invoke; } blk_t;
typedef struct { void *blk; id a; id b; long n; int kind; } call_t;
static void call_run(void *ctx) {
    call_t *c = ctx;
    void *pool = objc_autoreleasePoolPush();
    blk_t *b = c->blk;
    if (c->kind == 0) ((void (*)(void *))b->invoke)(b);
    else if (c->kind == 1) ((void (*)(void *, id))b->invoke)(b, c->a);
    else if (c->kind == 2) ((void (*)(void *, BOOL, id))b->invoke)(b, (BOOL)c->n, c->a);
    _Block_release(b);
    objc_release(c->a);
    objc_release(c->b);
    free(c);
    objc_autoreleasePoolPop(pool);
}
/* como no macOS, o bloco de conclusão roda numa fila global, depois que o método voltou */
static void later(void *block, int kind, id a, long n) {
    if (!block) return;
    call_t *c = calloc(1, sizeof *c);
    if (!c) return;
    c->blk = _Block_copy(block);
    c->a = objc_retain(a);
    c->n = n;
    c->kind = kind;
    dispatch_async_f(dispatch_get_global_queue(0, 0), c, call_run);
}

/* ---------------------------------------------------------------- som, anexos, ações, categorias */
static id snd_make(const char *name, int critical, double vol) {
    id o = objc_autorelease(new_of(&UNNotificationSound_c));
    pset(o, "name", cx_str(name));
    if (critical) pset(o, "critical", num_u64(1));
    if (vol >= 0) pset(o, "volume", num_dbl(vol));
    return o;
}
static id snd_default(id k, SEL c) { (void)k; (void)c; return snd_make("default", 0, -1); }
static id snd_named(id k, SEL c, id name) { (void)k; (void)c; return snd_make(cs(name) ? cs(name) : "default", 0, -1); }
static id snd_crit(id k, SEL c) { (void)k; (void)c; return snd_make("default", 1, -1); }
static id snd_crit_vol(id k, SEL c, float v) { (void)k; (void)c; return snd_make("default", 1, v); }
static id snd_crit_named(id k, SEL c, id name) { (void)k; (void)c; return snd_make(cs(name) ? cs(name) : "default", 1, -1); }
static id snd_crit_named_vol(id k, SEL c, id name, float v) { (void)k; (void)c; return snd_make(cs(name) ? cs(name) : "default", 1, v); }

static void attach_dir(char *out, size_t cap) {
    id h = NSHomeDirectory();
    id mb = MSG0(id, cls("NSBundle"), "mainBundle");
    id bid = mb ? MSG0(id, mb, "bundleIdentifier") : 0;
    cx_snprintf(out, cap, "%s/Library/Application Support/com.mos.usernotifications/%s/Attachments", h ? cx_cstr(h) : "/tmp",
                cs(bid) ? cs(bid) : "com.mos.app");
}
static void mkdirs(const char *path) {
    id fm = MSG0(id, cls("NSFileManager"), "defaultManager");
    if (fm) ((BOOL (*)(id, SEL, id, BOOL, id, id *))objc_msgSend)(fm, sel_registerName("createDirectoryAtPath:withIntermediateDirectories:attributes:error:"),
                                                                 cx_str(path), YES, (id)0, (id *)0);
}
/* +attachmentWithIdentifier:URL:options:error: — valida e move o arquivo para a pasta do app, como o macOS */
static id att_make(id k, SEL c, id ident, id url, id opts, id *err) {
    (void)k; (void)c;
    if (err) *err = 0;
    id path = url && responds(url, "path") ? MSG0(id, url, "path") : 0;
    const char *p = cs(path);
    if (!p || access(p, 0)) { if (err) *err = un_error(UNErrAttachInvalidURL, "O arquivo do anexo não existe."); return 0; }
    char uti[200] = "";
    id hint = opts ? MSG(id, opts, "objectForKey:", cx_str("UNNotificationAttachmentOptionsTypeHintKey")) : 0;
    if (cs(hint)) cx_snprintf(uti, sizeof uti, "%s", cs(hint));
    else cx_ls_type_of_file(p, uti, sizeof uti);
    long long limit = cx_ls_conforms(uti, "public.image") ? 10LL << 20 : cx_ls_conforms(uti, "public.audio") ? 5LL << 20 :
                      cx_ls_conforms(uti, "public.movie") || cx_ls_conforms(uti, "public.audiovisual-content") ? 50LL << 20 : 0;
    if (!limit) { if (err) *err = un_error(UNErrAttachType, "Tipo de anexo não aceito (imagem, áudio ou vídeo)."); return 0; }
    id fm = MSG0(id, cls("NSFileManager"), "defaultManager");
    id attrs = fm ? ((id (*)(id, SEL, id, id *))objc_msgSend)(fm, sel_registerName("attributesOfItemAtPath:error:"), path, (id *)0) : 0;
    id sz = attrs ? MSG(id, attrs, "objectForKey:", cx_str("NSFileSize")) : 0;
    long long size = sz ? MSG0(long long, sz, "longLongValue") : 0;
    if (size > limit) { if (err) *err = un_error(UNErrAttachSize, "O anexo passa do tamanho aceito."); return 0; }
    char dir[600], dst[900];
    attach_dir(dir, sizeof dir);
    mkdirs(dir);
    id uuid = MSG0(id, MSG0(id, cls("NSUUID"), "UUID"), "UUIDString");
    const char *ext = strrchr(p, '.');
    cx_snprintf(dst, sizeof dst, "%s/%s%s", dir, cs(uuid) ? cs(uuid) : "anexo", ext && !strchr(ext, '/') ? ext : "");
    if (rename(p, dst)) {
        BOOL ok = fm ? ((BOOL (*)(id, SEL, id, id, id *))objc_msgSend)(fm, sel_registerName("copyItemAtPath:toPath:error:"), path, cx_str(dst), (id *)0) : NO;
        if (!ok) { if (err) *err = un_error(UNErrAttachMove, "Não foi possível guardar o anexo."); return 0; }
        ((BOOL (*)(id, SEL, id, id *))objc_msgSend)(fm, sel_registerName("removeItemAtPath:error:"), path, (id *)0);
    }
    id o = objc_autorelease(new_of(&UNNotificationAttachment_c));
    pset_copy(o, "identifier", cs(ident) && cs(ident)[0] ? ident : uuid);
    pset(o, "URL", MSG(id, cls("NSURL"), "fileURLWithPath:", cx_str(dst)));
    pset(o, "type", cx_str(uti));
    return o;
}
static id att_restore(const char *ident, const char *url, const char *type) {
    id o = objc_autorelease(new_of(&UNNotificationAttachment_c));
    pset(o, "identifier", cx_str(ident));
    pset(o, "URL", MSG(id, cls("NSURL"), "URLWithString:", cx_str(url)));
    pset(o, "type", cx_str(type));
    return o;
}

static id act_make(cx_class_t *k, id ident, id title, u64 opts, id icon) {
    id o = objc_autorelease(new_of(k));
    pset_copy(o, "identifier", ident);
    pset_copy(o, "title", title);
    pset(o, "options", num_u64(opts));
    if (icon) pset(o, "icon", icon);
    return o;
}
static id act_with(id k, SEL c, id ident, id title, u64 opts) { (void)c; return act_make((cx_class_t *)k, ident, title, opts, 0); }
static id act_with_icon(id k, SEL c, id ident, id title, u64 opts, id icon) { (void)c; return act_make((cx_class_t *)k, ident, title, opts, icon); }
static id tact_with(id k, SEL c, id ident, id title, u64 opts, id btn, id ph) {
    (void)c;
    id o = act_make((cx_class_t *)k, ident, title, opts, 0);
    pset_copy(o, "textInputButtonTitle", btn);
    pset_copy(o, "textInputPlaceholder", ph);
    return o;
}
static id tact_with_icon(id k, SEL c, id ident, id title, u64 opts, id icon, id btn, id ph) {
    id o = tact_with(k, c, ident, title, opts, btn, ph);
    if (icon) pset(o, "icon", icon);
    return o;
}
static id icon_make(const char *kind, id name) {
    id o = objc_autorelease(new_of(&UNNotificationActionIcon_c));
    pset_copy(o, kind, name);
    return o;
}
static id icon_template(id k, SEL c, id n) { (void)k; (void)c; return icon_make("templateImageName", n); }
static id icon_system(id k, SEL c, id n) { (void)k; (void)c; return icon_make("systemImageName", n); }

static id cat_full(id k, SEL c, id ident, id actions, id intents, id placeholder, id summary, u64 opts) {
    (void)c;
    id o = objc_autorelease(new_of((cx_class_t *)k));
    pset_copy(o, "identifier", ident);
    pset_copy(o, "actions", actions);
    pset_copy(o, "intentIdentifiers", intents);
    pset_copy(o, "hiddenPreviewsBodyPlaceholder", placeholder);
    pset_copy(o, "categorySummaryFormat", summary);
    pset(o, "options", num_u64(opts));
    return o;
}
static id cat_with(id k, SEL c, id ident, id actions, id intents, u64 opts) { return cat_full(k, c, ident, actions, intents, 0, 0, opts); }
static id cat_with_ph(id k, SEL c, id ident, id actions, id intents, id ph, u64 opts) { return cat_full(k, c, ident, actions, intents, ph, 0, opts); }
static id cat_with_sum(id k, SEL c, id ident, id actions, id intents, id ph, id sum, u64 opts) { return cat_full(k, c, ident, actions, intents, ph, sum, opts); }
static u64 cat_hash(id self, SEL c) { (void)c; id i = pget(self, "identifier"); return i ? MSG0(u64, i, "hash") : 0; }
static BOOL cat_equal(id self, SEL c, id o) {
    (void)c;
    if (self == o) return YES;
    if (!o || !cx_responds(object_getClass(o), sel_registerName("intentIdentifiers"))) return NO;
    id a = pget(self, "identifier"), b = pget(o, "identifier");
    return a && b && MSG(BOOL, a, "isEqualToString:", b);
}

/* ---------------------------------------------------------------- gatilhos */
static id trig_interval(id k, SEL c, double interval, BOOL repeats) {
    (void)c;
    /* no macOS: NSInternalInconsistencyException; aqui o pedido falha no add com UNErrorCodeNotificationInvalidNoDate */
    id o = objc_autorelease(new_of((cx_class_t *)k));
    pset(o, "timeInterval", num_dbl(interval));
    pset(o, "repeats", num_u64(repeats ? 1 : 0));
    return o;
}
static id trig_next_interval(id self, SEL c) {
    (void)c;
    id d = pget(self, "_fire");
    if (d) return MSG(id, cls("NSDate"), "dateWithTimeIntervalSince1970:", MSG0(double, d, "doubleValue"));
    return MSG(id, cls("NSDate"), "dateWithTimeIntervalSinceNow:", pget_dbl(self, "timeInterval"));
}
#define UNDEF_COMP 0x7fffffffffffffffLL
static int comp(id comps, const char *sel) {
    if (!comps || !responds(comps, sel)) return -1;
    long long v = MSG0(long long, comps, sel);
    return v == UNDEF_COMP ? -1 : (int)v;
}
static cx_un_cal_t cal_of(id comps) {
    cx_un_cal_t k = {comp(comps, "year"), comp(comps, "month"), comp(comps, "day"), comp(comps, "hour"), comp(comps, "minute"),
                     comp(comps, "second"), comp(comps, "weekday")};
    return k;
}
static id comps_of(const cx_un_cal_t *k) {
    id dc = objc_autorelease(new_of(cx_class_named("NSDateComponents")));
    static const char *const sel[7] = {"setYear:", "setMonth:", "setDay:", "setHour:", "setMinute:", "setSecond:", "setWeekday:"};
    int v[7] = {k->year, k->month, k->day, k->hour, k->minute, k->second, k->weekday};
    for (int i = 0; i < 7; i++) if (v[i] >= 0 && responds(dc, sel[i])) MSG(void, dc, sel[i], (long long)v[i]);
    return dc;
}
static int tz_offset(void) {
    id tz = MSG0(id, cls("NSTimeZone"), "localTimeZone");
    return tz && responds(tz, "secondsFromGMT") ? (int)MSG0(long long, tz, "secondsFromGMT") : 0;
}
static id trig_calendar(id k, SEL c, id comps, BOOL repeats) {
    (void)c;
    id o = objc_autorelease(new_of((cx_class_t *)k));
    pset_copy(o, "dateComponents", comps);
    pset(o, "repeats", num_u64(repeats ? 1 : 0));
    return o;
}
static id trig_next_calendar(id self, SEL c) {
    (void)c;
    cx_un_cal_t k = cal_of(pget(self, "dateComponents"));
    lock();
    cx_un_set_tz_offset(tz_offset());
    double t = cx_un_calendar_next(&k, cx_un_now(), (int)pget_u64(self, "repeats"));
    unlock();
    return t < 0 ? 0 : MSG(id, cls("NSDate"), "dateWithTimeIntervalSince1970:", t);
}
static id trig_push(id k, SEL c) { (void)c; id o = objc_autorelease(new_of((cx_class_t *)k)); pset(o, "repeats", num_u64(0)); return o; }
static BOOL is_a(id o, cx_class_t *k) { return o && MSG(BOOL, o, "isKindOfClass:", (id)k); }

/* ---------------------------------------------------------------- pedido, notificação, resposta */
static id req_with(id k, SEL c, id ident, id content, id trigger) {
    (void)c;
    id o = objc_autorelease(new_of((cx_class_t *)k));
    pset_copy(o, "identifier", ident);
    id ct = content ? copy_props_to(content, &UNNotificationContent_c) : new_of(&UNNotificationContent_c);
    pset(o, "content", ct);
    objc_release(ct);
    if (trigger) pset(o, "trigger", trigger);
    return o;
}

/* conteúdo → registro C (as strings ficam no pool de autorelease) */
static id plist_text(id obj) {
    if (!obj || !MSG0(u64, obj, "count")) return 0;
    id data = ((id (*)(id, SEL, id, u64, u64, id *))objc_msgSend)(cls("NSPropertyListSerialization"),
              sel_registerName("dataWithPropertyList:format:options:error:"), obj, 100, 0, (id *)0);
    if (!data) return 0;
    return objc_autorelease(MSG(id, MSG0(id, cls("NSString"), "alloc"), "initWithData:encoding:", data, (u64)4));
}
static id plist_obj(const char *text) {
    if (!text || !text[0]) return 0;
    id data = MSG(id, cls("NSData"), "dataWithBytes:length:", (const void *)text, (u64)strlen(text));
    return ((id (*)(id, SEL, id, u64, u64 *, id *))objc_msgSend)(cls("NSPropertyListSerialization"),
            sel_registerName("propertyListWithData:options:format:error:"), data, 0, (u64 *)0, (id *)0);
}
static int req_to_c(id req, cx_un_req_t *r, int source) {
    memset(r, 0, sizeof *r);
    r->cal = (cx_un_cal_t){-1, -1, -1, -1, -1, -1, -1};
    r->badge = -1;
    r->source = source;
    id ct = pget(req, "content");
    r->ident = (char *)cs(pget(req, "identifier"));
    if (!r->ident) return -1;
    r->title = (char *)cs(pget(ct, "title"));
    r->subtitle = (char *)cs(pget(ct, "subtitle"));
    r->body = (char *)cs(pget(ct, "body"));
    r->category = (char *)cs(pget(ct, "categoryIdentifier"));
    r->thread = (char *)cs(pget(ct, "threadIdentifier"));
    r->target = (char *)cs(pget(ct, "targetContentIdentifier"));
    r->launch_image = (char *)cs(pget(ct, "launchImageName"));
    id snd = pget(ct, "sound");
    r->sound = snd ? (char *)cs(pget(snd, "name")) : 0;
    if (snd && !r->sound) r->sound = "default";
    id b = pget(ct, "badge");
    if (b) r->badge = (int)MSG0(long long, b, "longLongValue");
    r->interruption = (int)pget_u64(ct, "interruptionLevel");
    r->relevance = pget_dbl(ct, "relevanceScore");
    r->info = (char *)cs(plist_text(pget(ct, "userInfo")));
    id atts = pget(ct, "attachments");
    u64 na = atts ? MSG0(u64, atts, "count") : 0;
    if (na) {
        id m = MSG0(id, cls("NSMutableString"), "string");
        for (u64 i = 0; i < na; i++) {
            id a = MSG(id, atts, "objectAtIndex:", i);
            id u = pget(a, "URL");
            id us = u ? MSG0(id, u, "absoluteString") : 0;
            id line = MSG(id, cls("NSString"), "stringWithFormat:", cx_str("%@\t%@\t%@\n"), pget(a, "identifier") ? pget(a, "identifier") : cx_str(""),
                          us ? us : cx_str(""), pget(a, "type") ? pget(a, "type") : cx_str(""));
            MSG(void, m, "appendString:", line);
        }
        r->attach = (char *)cs(m);
    }
    id tg = pget(req, "trigger");
    if (tg && is_a(tg, &UNPushNotificationTrigger_c)) r->trig = CX_UN_TRIG_PUSH;
    else if (tg && is_a(tg, &UNCalendarNotificationTrigger_c)) {
        r->trig = CX_UN_TRIG_CALENDAR;
        r->cal = cal_of(pget(tg, "dateComponents"));
        r->repeats = (int)pget_u64(tg, "repeats");
    } else if (tg && is_a(tg, &UNTimeIntervalNotificationTrigger_c)) {
        r->trig = CX_UN_TRIG_INTERVAL;
        r->interval = pget_dbl(tg, "timeInterval");
        r->repeats = (int)pget_u64(tg, "repeats");
    }
    return 0;
}
static void put_str(id o, const char *k, const char *v) { if (v) pset(o, k, cx_str(v)); }
static id req_from_c(const cx_un_req_t *r) {
    id ct = new_of(&UNNotificationContent_c);
    put_str(ct, "title", r->title);
    put_str(ct, "subtitle", r->subtitle);
    put_str(ct, "body", r->body);
    put_str(ct, "categoryIdentifier", r->category);
    put_str(ct, "threadIdentifier", r->thread);
    put_str(ct, "targetContentIdentifier", r->target);
    put_str(ct, "launchImageName", r->launch_image);
    if (r->sound) pset(ct, "sound", snd_make(r->sound, 0, -1));
    if (r->badge >= 0) pset(ct, "badge", num_u64((u64)r->badge));
    if (r->interruption) pset(ct, "interruptionLevel", num_u64((u64)r->interruption));
    if (r->relevance) pset(ct, "relevanceScore", num_dbl(r->relevance));
    id info = plist_obj(r->info);
    if (info) pset(ct, "userInfo", info);
    if (r->attach && r->attach[0]) {
        id arr = MSG0(id, cls("NSMutableArray"), "array");
        const char *p = r->attach;
        while (*p) {
            const char *e = strchr(p, '\n');
            size_t n = e ? (size_t)(e - p) : strlen(p);
            char line[1200];
            if (n >= sizeof line) n = sizeof line - 1;
            memcpy(line, p, n);
            line[n] = 0;
            char *t1 = strchr(line, '\t'), *t2 = t1 ? strchr(t1 + 1, '\t') : 0;
            if (t1 && t2) { *t1 = 0; *t2 = 0; MSG(void, arr, "addObject:", att_restore(line, t1 + 1, t2 + 1)); }
            p += n + (e ? 1 : 0);
        }
        pset(ct, "attachments", arr);
    }
    id tg = 0;
    if (r->trig == CX_UN_TRIG_INTERVAL) {
        tg = trig_interval((id)&UNTimeIntervalNotificationTrigger_c, 0, r->interval, (BOOL)r->repeats);
        if (r->fire_at > 0) pset(tg, "_fire", num_dbl(r->fire_at));
    } else if (r->trig == CX_UN_TRIG_CALENDAR) tg = trig_calendar((id)&UNCalendarNotificationTrigger_c, 0, comps_of(&r->cal), (BOOL)r->repeats);
    id rq = objc_autorelease(new_of(&UNNotificationRequest_c));
    pset(rq, "identifier", cx_str(r->ident ? r->ident : ""));
    pset(rq, "content", ct);
    objc_release(ct);
    if (tg) pset(rq, "trigger", tg);
    return rq;
}
static id note_from_c(const cx_un_req_t *r) {
    id n = objc_autorelease(new_of(&UNNotification_c));
    pset(n, "request", req_from_c(r));
    pset(n, "date", MSG(id, cls("NSDate"), "dateWithTimeIntervalSince1970:", r->date > 0 ? r->date : cx_un_now()));
    return n;
}

/* ---------------------------------------------------------------- centro */
static id g_center, g_delegate;           /* o delegate é fraco no macOS (o app o guarda) */
static int g_provisional;
static id center_current(id k, SEL c) {
    (void)c;
    if (!g_center) g_center = objc_retain(new_of((cx_class_t *)k));
    return g_center;
}
static void sync_auth(void) {
    int st = cx_tcc_state("userNotifications");
    cx_un_set_auth(st == 2 ? CX_UN_AUTHORIZED : st == 1 ? CX_UN_DENIED : g_provisional ? CX_UN_PROVISIONAL : CX_UN_NOT_DETERMINED);
}
typedef struct { void *blk; } req_ctx_t;
static void auth_done(int granted, void *ctx) {
    req_ctx_t *r = ctx;
    later(r->blk, 2, 0, granted ? 1 : 0);
    _Block_release(r->blk);
    free(r);
}
/* requestAuthorizationWithOptions: .provisional (1 << 6) autoriza em silêncio quando ainda não há escolha */
static void center_request(id self, SEL c, u64 opts, void *block) {
    (void)self; (void)c;
    int st = cx_tcc_state("userNotifications");
    if ((opts & (1u << 6)) && st != 1 && st != 2) {
        g_provisional = 1;
        later(block, 2, 0, 1);
        return;
    }
    if (!block) { cx_tcc_request("userNotifications"); return; }
    req_ctx_t *r = calloc(1, sizeof *r);
    if (!r) return;
    r->blk = _Block_copy(block);
    cx_tcc_request_async("userNotifications", auth_done, r);
}
static void center_set_delegate(id self, SEL c, id d) { (void)self; (void)c; g_delegate = d; }
static id center_delegate(id self, SEL c) { (void)self; (void)c; return g_delegate; }

static void schedule(void);
static void flush_main(void *ctx) {
    (void)ctx;
    void *pool = objc_autoreleasePoolPush();
    lock();
    sync_auth();
    cx_un_set_tz_offset(tz_offset());
    cx_un_flush();
    unlock();
    schedule();
    objc_autoreleasePoolPop(pool);
}
static void center_add(id self, SEL c, id req, void *block) {
    (void)self; (void)c;
    void *pool = objc_autoreleasePoolPush();
    cx_un_req_t r;
    if (!req || req_to_c(req, &r, CX_UN_SRC_UN)) { later(block, 1, un_error(UNErrNotAllowed, "Pedido inválido."), 0); objc_autoreleasePoolPop(pool); return; }
    if (r.trig == CX_UN_TRIG_PUSH) { later(block, 1, un_error(UNErrNotAllowed, "O mOS não tem notificações remotas (APNs)."), 0); objc_autoreleasePoolPop(pool); return; }
    lock();
    sync_auth();
    cx_un_set_tz_offset(tz_offset());
    int rc = cx_un_add(&r);
    unlock();
    id err = rc == CX_UN_OK ? 0 : rc == CX_UN_ERR_DENIED ? un_error(UNErrNotAllowed, "O app não tem permissão para notificações.")
                                                           : un_error(UNErrBadTrigger, "O gatilho não tem próxima data.");
    later(block, 1, err, 0);
    if (rc == CX_UN_OK) dispatch_async_f(dispatch_get_main_queue(), 0, flush_main);
    objc_autoreleasePoolPop(pool);
}
static id list_of(int delivered, int source) {
    id arr = MSG0(id, cls("NSMutableArray"), "array");
    lock();
    for (int i = 0; i < cx_un_count(delivered); i++) {
        const cx_un_req_t *r = cx_un_at(delivered, i);
        if (r->source != source) continue;
        MSG(void, arr, "addObject:", delivered ? note_from_c(r) : req_from_c(r));
    }
    unlock();
    return arr;
}
static void center_get_pending(id self, SEL c, void *block) { (void)self; (void)c; later(block, 1, list_of(0, CX_UN_SRC_UN), 0); }
static void center_get_delivered(id self, SEL c, void *block) { (void)self; (void)c; later(block, 1, list_of(1, CX_UN_SRC_UN), 0); }
static void remove_ids(id ids, int delivered) {
    u64 n = ids ? MSG0(u64, ids, "count") : 0;
    lock();
    for (u64 i = 0; i < n; i++) {
        const char *s = cs(MSG(id, ids, "objectAtIndex:", i));
        if (!s) continue;
        if (delivered) cx_un_remove_delivered(s, CX_UN_SRC_UN); else cx_un_remove_pending(s, CX_UN_SRC_UN);
    }
    unlock();
}
static void center_rm_pending(id self, SEL c, id ids) { (void)self; (void)c; remove_ids(ids, 0); }
static void center_rm_delivered(id self, SEL c, id ids) { (void)self; (void)c; remove_ids(ids, 1); }
static void center_rm_all_pending(id self, SEL c) { (void)self; (void)c; lock(); cx_un_remove_pending(0, CX_UN_SRC_UN); unlock(); }
static void center_rm_all_delivered(id self, SEL c) { (void)self; (void)c; lock(); cx_un_remove_delivered(0, CX_UN_SRC_UN); unlock(); }

static id g_categories;
static void center_set_categories(id self, SEL c, id set) {
    (void)self; (void)c;
    void *pool = objc_autoreleasePoolPush();
    id all = set ? MSG0(id, set, "allObjects") : 0;
    u64 n = all ? MSG0(u64, all, "count") : 0;
    cx_un_category_t *cats = n ? calloc(n, sizeof *cats) : 0;
    for (u64 i = 0; cats && i < n; i++) {
        id cat = MSG(id, all, "objectAtIndex:", i);
        cats[i].ident = (char *)cs(pget(cat, "identifier"));
        cats[i].options = (int)pget_u64(cat, "options");
        id acts = pget(cat, "actions");
        u64 na = acts ? MSG0(u64, acts, "count") : 0;
        cats[i].actions = na ? calloc(na, sizeof *cats[i].actions) : 0;
        for (u64 k = 0; cats[i].actions && k < na; k++) {
            id a = MSG(id, acts, "objectAtIndex:", k);
            cx_un_action_t *d = &cats[i].actions[cats[i].nactions++];
            d->ident = (char *)cs(pget(a, "identifier"));
            d->title = (char *)cs(pget(a, "title"));
            d->options = (int)pget_u64(a, "options");
            d->text_input = is_a(a, &UNTextInputNotificationAction_c);
            d->button = (char *)cs(pget(a, "textInputButtonTitle"));
            d->placeholder = (char *)cs(pget(a, "textInputPlaceholder"));
        }
    }
    lock();
    cx_un_set_categories(cats, (int)n);
    id old = g_categories;
    g_categories = set ? MSG0(id, set, "copy") : 0;
    unlock();
    objc_release(old);
    for (u64 i = 0; cats && i < n; i++) free(cats[i].actions);
    free(cats);
    objc_autoreleasePoolPop(pool);
}
static void center_get_categories(id self, SEL c, void *block) {
    (void)self; (void)c;
    lock();
    id s = g_categories ? objc_autorelease(objc_retain(g_categories)) : MSG0(id, cls("NSSet"), "set");
    unlock();
    later(block, 1, s, 0);
}
static void center_get_settings(id self, SEL c, void *block) {
    (void)self; (void)c;
    cx_un_settings_t st;
    lock();
    sync_auth();
    cx_un_settings(&st);
    unlock();
    id o = objc_autorelease(new_of(&UNNotificationSettings_c));
    pset(o, "authorizationStatus", num_u64((u64)st.authorization));
    pset(o, "soundSetting", num_u64((u64)st.sound));
    pset(o, "badgeSetting", num_u64((u64)st.badge));
    pset(o, "alertSetting", num_u64((u64)st.alert));
    pset(o, "notificationCenterSetting", num_u64((u64)st.notification_center));
    pset(o, "lockScreenSetting", num_u64((u64)st.lock_screen));
    pset(o, "alertStyle", num_u64((u64)st.alert_style));
    pset(o, "showPreviewsSetting", num_u64(0));                  /* UNShowPreviewsSettingAlways */
    pset(o, "providesAppNotificationSettings", num_u64(0));
    /* sem CarPlay, alertas críticos, anúncio por voz, resumo agendado nem mensagens diretas: notSupported (0) */
    later(block, 1, o, 0);
}
static long center_auth_status(id self, SEL c) { (void)self; (void)c; lock(); sync_auth(); int a = cx_un_auth(); unlock(); return a; }
static void center_set_badge(id self, SEL c, long n, void *block) {
    (void)self; (void)c;
    lock();
    cx_un_set_badge((int)n);
    unlock();
    later(block, 1, 0, 0);
}
static BOOL center_no(id self, SEL c) { (void)self; (void)c; return NO; }
/* sem APNs: o delegate do app recebe a falha */
static void center_register_remote(id self, SEL c) {
    (void)self; (void)c;
    id app = cx_class_named("NSApplication") ? MSG0(id, cls("NSApplication"), "sharedApplication") : 0;
    id d = app ? MSG0(id, app, "delegate") : 0;
    id err = un_error(3000, "O mOS não tem APNs; nenhum token foi emitido.");
    if (d && responds(d, "application:didFailToRegisterForRemoteNotificationsWithError:"))
        MSG(void, d, "application:didFailToRegisterForRemoteNotificationsWithError:", app, err);
}

/* ---------------------------------------------------------------- delegate: apresentar e responder (fio principal) */
typedef struct { void *isa; int flags, reserved; void (*invoke)(void *, u64); void *desc; u64 *out; int *called; } opt_blk_t;
typedef struct { void *isa; int flags, reserved; void (*invoke)(void *); void *desc; int *called; } void_blk_t;
typedef struct { unsigned long reserved, size; } blk_desc_t;
extern cx_class_t _NSConcreteStackBlock_c __asm__("_NSConcreteStackBlock");
static void opt_invoke(void *b, u64 v) { opt_blk_t *x = b; if (x->out) *x->out = v; if (x->called) *x->called = 1; }
static void void_invoke(void *b) { void_blk_t *x = b; if (x->called) *x->called = 1; }
static int app_active(void) {
    cx_class_t *k = cx_class_named("NSApplication");
    id app = k ? MSG0(id, (id)k, "sharedApplication") : 0;
    return app && responds(app, "isActive") && MSG0(BOOL, app, "isActive");
}
int cx_nsun_present(const cx_un_req_t *r);                  /* nsusernotification.c */
void cx_nsun_receive(const cx_un_req_t *r, const char *action, const char *text);
/* app em segundo plano: o sistema mostra tudo sem perguntar; na frente: o delegate decide (sem ele, nada aparece) */
static int un_present(const cx_un_req_t *r, void *ctx) {
    (void)ctx;
    if (r->source == CX_UN_SRC_LEGACY) return cx_nsun_present(r);
    if (!app_active()) return CX_UN_PRESENT_ALL;
    SEL sel = sel_registerName("userNotificationCenter:willPresentNotification:withCompletionHandler:");
    if (!g_delegate || !cx_responds(object_getClass(g_delegate), sel)) return 0;
    static blk_desc_t desc = {0, sizeof(opt_blk_t)};
    u64 opts = 0;
    int called = 0;
    opt_blk_t blk = {&_NSConcreteStackBlock_c, 0, 0, opt_invoke, &desc, &opts, &called};
    ((void (*)(id, SEL, id, id, void *))objc_msgSend)(g_delegate, sel, g_center, note_from_c(r), &blk);
    return called ? (int)opts : 0;
}
static void un_receive(const cx_un_req_t *r, const char *action, const char *text, void *ctx) {
    (void)ctx;
    if (r->source == CX_UN_SRC_LEGACY) { cx_nsun_receive(r, action, text); return; }
    SEL sel = sel_registerName("userNotificationCenter:didReceiveNotificationResponse:withCompletionHandler:");
    if (!g_delegate || !cx_responds(object_getClass(g_delegate), sel)) return;
    id resp = objc_autorelease(new_of(text ? &UNTextInputNotificationResponse_c : &UNNotificationResponse_c));
    pset(resp, "notification", note_from_c(r));
    pset(resp, "actionIdentifier", cx_str(action));
    if (text) pset(resp, "userText", cx_str(text));
    static blk_desc_t desc = {0, sizeof(void_blk_t)};
    int called = 0;
    void_blk_t blk = {&_NSConcreteStackBlock_c, 0, 0, void_invoke, &desc, &called};
    ((void (*)(id, SEL, id, id, void *))objc_msgSend)(g_delegate, sel, g_center, resp, &blk);
}

/* ---------------------------------------------------------------- Interface (banner, Central, badge) */
static int g_live;                         /* há banners desta sessão esperando clique */
static double g_last_banner;
static void banner(const cx_un_req_t *r, const cx_un_category_t *cat, int sound, void *ctx) {
    (void)ctx;
    char acts[1200] = "";
    usize n = 0;
    for (int i = 0; cat && i < cat->nactions && i < 4; i++) {
        int o = cat->actions[i].options, d = 0;
        if (o & 2) d |= MOS_DESK_ACT_DESTRUCTIVE;
        if (cat->actions[i].text_input) d |= MOS_DESK_ACT_TEXT;
        if (o & 4) d |= MOS_DESK_ACT_FOREGROUND;
        n += (usize)cx_snprintf(acts + n, sizeof acts - n, "%s\t%d\n", cat->actions[i].title ? cat->actions[i].title : "", d);
        if (n >= sizeof acts) { n = sizeof acts - 1; break; }
    }
    id mb = MSG0(id, cls("NSBundle"), "mainBundle");
    id nm = mb ? MSG(id, mb, "objectForInfoDictionaryKey:", cx_str("CFBundleName")) : 0;
    const char *title = r->title && r->title[0] ? r->title : cs(nm) ? cs(nm) : "";
    mos_desk_banner_t b = {(u64)title, (u64)(r->subtitle ? r->subtitle : ""), (u64)(r->body ? r->body : ""), (u64)acts, r->token,
                           (cat && (cat->options & 1) ? MOS_DESK_FLAG_DISMISS : 0) | (sound ? 0 : MOS_DESK_FLAG_SILENT)};
    SYS2(SYS_DESKTOP, MOS_DESK_BANNER_EX, &b);
    g_live = 1;
    g_last_banner = cx_un_now();
}
static void unbanner(int token, void *ctx) { (void)ctx; SYS2(SYS_DESKTOP, MOS_DESK_REMOVE, token); }
static void badge(int count, void *ctx) {
    (void)ctx;
    char b[24] = "";
    if (count > 0) cx_snprintf(b, sizeof b, "%d", count);
    SYS2(SYS_DESKTOP, MOS_DESK_BADGE, b);
}
static int poll_events(void) {
    int got = 0;
    for (int n = 0; n < 16; n++) {
        mos_desk_event_t ev;
        memset(&ev, 0, sizeof ev);
        if (SYS2(SYS_DESKTOP, MOS_DESK_TAKE_EX, &ev) <= 0) break;
        ev.text[sizeof ev.text - 1] = 0;
        lock();
        cx_un_event(ev.token, ev.action, ev.text);
        unlock();
        got = 1;
    }
    return got;
}
static int any_token(void) {
    int live = 0;
    lock();
    for (int i = 0; i < cx_un_count(1); i++) if (cx_un_at(1, i)->token) { live = 1; break; }
    unlock();
    return live;
}

/* timer do fio principal: a próxima entrega e, com banners vivos, os cliques */
static volatile int g_armed;
static double g_armed_for;
static void tick(void *ctx) {
    (void)ctx;
    g_armed = 0;
    void *pool = objc_autoreleasePoolPush();
    poll_events();
    lock();
    sync_auth();
    cx_un_set_tz_offset(tz_offset());
    cx_un_flush();
    unlock();
    g_live = any_token();
    schedule();
    objc_autoreleasePoolPop(pool);
}
static void schedule(void) {
    lock();
    double next = cx_un_next_fire(), now = cx_un_now();
    unlock();
    double wait = -1;
    if (next > 0) wait = next - now;
    /* cliques: 0,3 s logo depois de um banner (9 s na tela), 1 s enquanto houver entradas desta sessão na Central */
    double poll = now - g_last_banner < 15 ? 0.3 : 1.0;
    if (g_live && (wait < 0 || wait > poll)) wait = poll;
    if (wait < 0) return;
    if (wait < 0.01) wait = 0.01;
    if (g_armed && g_armed_for <= now + wait + 0.005) return;
    g_armed = 1;
    g_armed_for = now + wait;
    dispatch_after_f(dispatch_time(0, (i64)(wait * 1e9)), dispatch_get_main_queue(), 0, tick);
}
void cx_un_kick(void) { dispatch_async_f(dispatch_get_main_queue(), 0, flush_main); }   /* nsusernotification.c */
void cx_un_lock(void) { lock(); }
void cx_un_unlock(void) { unlock(); }
id cx_un_props(id o) { return props(o); }

/* NSRunLoop fora do AppKit (ferramentas): o mesmo trabalho a cada volta do laço */
static void (*g_prev_hook)(double until);
static void un_runloop(double until) {
    if (pthread_main_np()) {
        poll_events();
        lock();
        cx_un_flush();
        unlock();
    }
    if (g_prev_hook) g_prev_hook(until);
}
void cx_un_install_runloop(void) {
    if (cx_runloop_hook == un_runloop) return;
    g_prev_hook = cx_runloop_hook;
    cx_runloop_hook = un_runloop;
}

/* ---------------------------------------------------------------- extensão de serviço (sem payload remoto) */
static void ext_receive(id self, SEL c, id req, void *block) {
    (void)self; (void)c;
    id ct = req ? pget(req, "content") : 0;
    if (block) ((void (*)(void *, id))((blk_t *)block)->invoke)(block, ct);
}
static void ext_expire(id self, SEL c) { (void)self; (void)c; }

static void store_open(void) {
    id h = NSHomeDirectory();
    id mb = MSG0(id, cls("NSBundle"), "mainBundle");
    id bid = mb ? MSG0(id, mb, "bundleIdentifier") : 0;
    if (!cs(bid) || !cs(bid)[0]) return;               /* ferramenta sem pacote: só memória */
    char dir[600], path[800];
    cx_snprintf(dir, sizeof dir, "%s/Library/Application Support/com.mos.usernotifications", h ? cx_cstr(h) : "/tmp");
    mkdirs(dir);
    cx_snprintf(path, sizeof path, "%s/%s.txt", dir, cs(bid));
    lock();
    cx_un_set_store(path);
    unlock();
}

/* ---------------------------------------------------------------- registro */
typedef struct { const char *sel; void *imp; } m_t;
static void add_all(cx_class_t *k, const m_t *m, unsigned n) { for (unsigned i = 0; i < n; i++) cx_add_method(k, m[i].sel, m[i].imp); }
static void add_all_c(cx_class_t *k, const m_t *m, unsigned n) { for (unsigned i = 0; i < n; i++) cx_add_cmethod(k, m[i].sel, m[i].imp); }
#define N(a) (sizeof(a) / sizeof((a)[0]))

void cx_usernotifications_init(void) {
    static int done;
    if (done++) return;
    cx_class_t *obj = cx_class_named("NSObject");
    cx_define_class(&UNNotificationContent_c, &UNNotificationContent_m, "UNNotificationContent", obj, 0);
    cx_define_class(&UNNotificationTrigger_c, &UNNotificationTrigger_m, "UNNotificationTrigger", obj, 0);
    cx_define_class(&UNTimeIntervalNotificationTrigger_c, &UNTimeIntervalNotificationTrigger_m, "UNTimeIntervalNotificationTrigger", &UNNotificationTrigger_c, 0);
    cx_define_class(&UNCalendarNotificationTrigger_c, &UNCalendarNotificationTrigger_m, "UNCalendarNotificationTrigger", &UNNotificationTrigger_c, 0);
    cx_define_class(&UNPushNotificationTrigger_c, &UNPushNotificationTrigger_m, "UNPushNotificationTrigger", &UNNotificationTrigger_c, 0);
    cx_define_class(&UNNotification_c, &UNNotification_m, "UNNotification", obj, 0);
    cx_define_class(&UNNotificationResponse_c, &UNNotificationResponse_m, "UNNotificationResponse", obj, 0);
    cx_define_class(&UNNotificationSettings_c, &UNNotificationSettings_m, "UNNotificationSettings", obj, 0);
    cx_define_class(&UNNotificationActionIcon_c, &UNNotificationActionIcon_m, "UNNotificationActionIcon", obj, 0);
    cx_define_class(&UNNotificationServiceExtension_c, &UNNotificationServiceExtension_m, "UNNotificationServiceExtension", obj, 0);
    /* as classes de fachada (vscode_fwclasses.c) nascem filhas de NSObject: a hierarquia da Apple */
    UNMutableNotificationContent_c.super = &UNNotificationContent_c;
    UNMutableNotificationContent_m.super = &UNNotificationContent_m;
    UNTextInputNotificationResponse_c.super = &UNNotificationResponse_c;
    UNTextInputNotificationResponse_m.super = &UNNotificationResponse_m;

    static const char *const ct_str[] = {"title", "subtitle", "body", "categoryIdentifier", "threadIdentifier", "launchImageName", "summaryArgument"};
    for (unsigned i = 0; i < N(ct_str); i++) cx_add_method(&UNNotificationContent_c, ct_str[i], (void *)g_str);
    static const m_t ct[] = {
        {"badge", (void *)g_obj}, {"sound", (void *)g_obj}, {"userInfo", (void *)g_dict}, {"attachments", (void *)g_arr},
        {"targetContentIdentifier", (void *)g_obj}, {"interruptionLevel", (void *)g_u64}, {"relevanceScore", (void *)g_dbl},
        {"filterCriteria", (void *)g_obj}, {"summaryArgumentCount", (void *)g_u64}, {"copyWithZone:", (void *)ct_copy},
        {"copy", (void *)ct_copy}, {"mutableCopyWithZone:", (void *)ct_mcopy}, {"mutableCopy", (void *)ct_mcopy},
        {"isEqual:", (void *)ct_equal}, {"contentByUpdatingWithProvider:error:", (void *)same_copy},
    };
    add_all(&UNNotificationContent_c, ct, N(ct));
    static const char *const ct_set[] = {"setTitle:", "setSubtitle:", "setBody:", "setCategoryIdentifier:", "setThreadIdentifier:",
                                         "setLaunchImageName:", "setSummaryArgument:", "setBadge:", "setSound:", "setUserInfo:",
                                         "setAttachments:", "setTargetContentIdentifier:", "setFilterCriteria:"};
    for (unsigned i = 0; i < N(ct_set); i++) cx_add_method(&UNMutableNotificationContent_c, ct_set[i], (void *)s_copy);
    cx_add_method(&UNMutableNotificationContent_c, "setInterruptionLevel:", (void *)s_u64);
    cx_add_method(&UNMutableNotificationContent_c, "setSummaryArgumentCount:", (void *)s_u64);
    cx_add_method(&UNMutableNotificationContent_c, "setRelevanceScore:", (void *)s_dbl);
    cx_add_method(&UNMutableNotificationContent_c, "copyWithZone:", (void *)ct_copy);
    cx_add_method(&UNMutableNotificationContent_c, "copy", (void *)ct_copy);

    cx_add_cmethod(&UNNotificationRequest_c, "requestWithIdentifier:content:trigger:", (void *)req_with);
    static const m_t rq[] = {{"identifier", (void *)g_str}, {"content", (void *)g_obj}, {"trigger", (void *)g_obj},
                             {"copyWithZone:", (void *)same_copy}, {"copy", (void *)same_copy}};
    add_all(&UNNotificationRequest_c, rq, N(rq));

    cx_add_method(&UNNotificationTrigger_c, "repeats", (void *)g_bool);
    cx_add_method(&UNNotificationTrigger_c, "copyWithZone:", (void *)same_copy);
    cx_add_cmethod(&UNTimeIntervalNotificationTrigger_c, "triggerWithTimeInterval:repeats:", (void *)trig_interval);
    cx_add_method(&UNTimeIntervalNotificationTrigger_c, "timeInterval", (void *)g_dbl);
    cx_add_method(&UNTimeIntervalNotificationTrigger_c, "nextTriggerDate", (void *)trig_next_interval);
    cx_add_cmethod(&UNCalendarNotificationTrigger_c, "triggerWithDateMatchingComponents:repeats:", (void *)trig_calendar);
    cx_add_method(&UNCalendarNotificationTrigger_c, "dateComponents", (void *)g_obj);
    cx_add_method(&UNCalendarNotificationTrigger_c, "nextTriggerDate", (void *)trig_next_calendar);
    cx_add_cmethod(&UNPushNotificationTrigger_c, "trigger", (void *)trig_push);

    cx_add_method(&UNNotification_c, "request", (void *)g_obj);
    cx_add_method(&UNNotification_c, "date", (void *)g_obj);
    cx_add_method(&UNNotificationResponse_c, "notification", (void *)g_obj);
    cx_add_method(&UNNotificationResponse_c, "actionIdentifier", (void *)g_str);
    cx_add_method(&UNNotificationResponse_c, "targetScene", (void *)g_obj);
    cx_add_method(&UNTextInputNotificationResponse_c, "userText", (void *)g_str);

    static const m_t snd_c[] = {{"defaultSound", (void *)snd_default}, {"soundNamed:", (void *)snd_named},
                                {"defaultCriticalSound", (void *)snd_crit}, {"defaultCriticalSoundWithAudioVolume:", (void *)snd_crit_vol},
                                {"criticalSoundNamed:", (void *)snd_crit_named}, {"criticalSoundNamed:withAudioVolume:", (void *)snd_crit_named_vol},
                                {"defaultRingtoneSound", (void *)snd_default}, {"ringtoneSoundNamed:", (void *)snd_named}};
    add_all_c(&UNNotificationSound_c, snd_c, N(snd_c));
    cx_add_method(&UNNotificationSound_c, "copyWithZone:", (void *)same_copy);

    cx_add_cmethod(&UNNotificationAttachment_c, "attachmentWithIdentifier:URL:options:error:", (void *)att_make);
    static const m_t att[] = {{"identifier", (void *)g_str}, {"URL", (void *)g_obj}, {"type", (void *)g_str}, {"copyWithZone:", (void *)same_copy}};
    add_all(&UNNotificationAttachment_c, att, N(att));

    cx_add_cmethod(&UNNotificationAction_c, "actionWithIdentifier:title:options:", (void *)act_with);
    cx_add_cmethod(&UNNotificationAction_c, "actionWithIdentifier:title:options:icon:", (void *)act_with_icon);
    static const m_t act[] = {{"identifier", (void *)g_str}, {"title", (void *)g_str}, {"options", (void *)g_u64}, {"icon", (void *)g_obj},
                              {"copyWithZone:", (void *)same_copy}};
    add_all(&UNNotificationAction_c, act, N(act));
    cx_add_cmethod(&UNTextInputNotificationAction_c, "actionWithIdentifier:title:options:textInputButtonTitle:textInputPlaceholder:", (void *)tact_with);
    cx_add_cmethod(&UNTextInputNotificationAction_c, "actionWithIdentifier:title:options:icon:textInputButtonTitle:textInputPlaceholder:", (void *)tact_with_icon);
    cx_add_method(&UNTextInputNotificationAction_c, "textInputButtonTitle", (void *)g_str);
    cx_add_method(&UNTextInputNotificationAction_c, "textInputPlaceholder", (void *)g_str);
    cx_add_cmethod(&UNNotificationActionIcon_c, "iconWithTemplateImageName:", (void *)icon_template);
    cx_add_cmethod(&UNNotificationActionIcon_c, "iconWithSystemImageName:", (void *)icon_system);

    cx_add_cmethod(&UNNotificationCategory_c, "categoryWithIdentifier:actions:intentIdentifiers:options:", (void *)cat_with);
    cx_add_cmethod(&UNNotificationCategory_c, "categoryWithIdentifier:actions:intentIdentifiers:hiddenPreviewsBodyPlaceholder:options:", (void *)cat_with_ph);
    cx_add_cmethod(&UNNotificationCategory_c, "categoryWithIdentifier:actions:intentIdentifiers:hiddenPreviewsBodyPlaceholder:categorySummaryFormat:options:", (void *)cat_with_sum);
    static const m_t cat[] = {{"identifier", (void *)g_str}, {"actions", (void *)g_arr}, {"intentIdentifiers", (void *)g_arr},
                              {"options", (void *)g_u64}, {"hiddenPreviewsBodyPlaceholder", (void *)g_obj},
                              {"categorySummaryFormat", (void *)g_obj}, {"hash", (void *)cat_hash}, {"isEqual:", (void *)cat_equal},
                              {"copyWithZone:", (void *)same_copy}};
    add_all(&UNNotificationCategory_c, cat, N(cat));

    static const char *const st[] = {"authorizationStatus", "soundSetting", "badgeSetting", "alertSetting", "notificationCenterSetting",
                                     "lockScreenSetting", "carPlaySetting", "alertStyle", "showPreviewsSetting", "criticalAlertSetting",
                                     "announcementSetting", "timeSensitiveSetting", "scheduledDeliverySetting", "directMessagesSetting"};
    for (unsigned i = 0; i < N(st); i++) cx_add_method(&UNNotificationSettings_c, st[i], (void *)g_u64);
    cx_add_method(&UNNotificationSettings_c, "providesAppNotificationSettings", (void *)g_bool);

    cx_add_cmethod(&UNUserNotificationCenter_c, "currentNotificationCenter", (void *)center_current);
    static const m_t ce[] = {
        {"requestAuthorizationWithOptions:completionHandler:", (void *)center_request}, {"authorizationStatus", (void *)center_auth_status},
        {"setDelegate:", (void *)center_set_delegate}, {"delegate", (void *)center_delegate},
        {"addNotificationRequest:withCompletionHandler:", (void *)center_add},
        {"getPendingNotificationRequestsWithCompletionHandler:", (void *)center_get_pending},
        {"getDeliveredNotificationsWithCompletionHandler:", (void *)center_get_delivered},
        {"removePendingNotificationRequestsWithIdentifiers:", (void *)center_rm_pending},
        {"removeAllPendingNotificationRequests", (void *)center_rm_all_pending},
        {"removeDeliveredNotificationsWithIdentifiers:", (void *)center_rm_delivered},
        {"removeAllDeliveredNotifications", (void *)center_rm_all_delivered},
        {"setNotificationCategories:", (void *)center_set_categories},
        {"getNotificationCategoriesWithCompletionHandler:", (void *)center_get_categories},
        {"getNotificationSettingsWithCompletionHandler:", (void *)center_get_settings},
        {"setBadgeCount:withCompletionHandler:", (void *)center_set_badge},
        {"supportsContentExtensions", (void *)center_no}, {"registerForRemoteNotifications", (void *)center_register_remote},
    };
    add_all(&UNUserNotificationCenter_c, ce, N(ce));

    cx_add_method(&UNNotificationServiceExtension_c, "didReceiveNotificationRequest:withContentHandler:", (void *)ext_receive);
    cx_add_method(&UNNotificationServiceExtension_c, "serviceExtensionTimeWillExpire", (void *)ext_expire);

    cx_un_set_delegate(un_present, un_receive, 0);
    static const cx_un_sink_t sink = {banner, unbanner, badge, 0};
    cx_un_set_sink(&sink);
    cx_un_set_auto_flush(0);
    store_open();
    /* o que venceu com o app fechado: entregue na primeira volta do laço principal */
    dispatch_async_f(dispatch_get_main_queue(), 0, flush_main);
    { extern void cx_nsusernotification_init(void); cx_nsusernotification_init(); }
}
