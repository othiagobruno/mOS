/* mOS — NSUserNotification e NSUserNotificationCenter (a API antiga do Foundation, ainda usada por apps e pelo
 * Electron antigo) sobre o mesmo centro do UserNotifications (un_center.c, origem CX_UN_SRC_LEGACY).
 *
 * deliverNotification: entrega agora; scheduleNotification: usa deliveryDate e deliveryRepeatInterval (dias, horas,
 * minutos, segundos e semanas viram intervalo; 1 mês ou 1 ano viram calendário). Os botões do banner saem de
 * hasActionButton/actionButtonTitle, hasReplyButton/responsePlaceholder e additionalActions; a resposta chega em
 * -userNotificationCenter:didActivateNotification: com activationType, response e additionalActivationAction.
 * O delegate decide em shouldPresentNotification: quando o app está na frente (sem ele, não aparece), e recebe
 * didDeliverNotification:. As classes são do Foundation (foundation_more.c); aqui ficam os métodos.
 * A autorização é a mesma do UserNotifications (TCC "userNotifications"): sem escolha, o primeiro envio pede. */
#include "foundation.h"
#include "libc.h"
#include "un_center.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void *objc_autoreleasePoolPush(void);
void objc_autoreleasePoolPop(void *p);
int cx_tcc_state(const char *service);
int cx_tcc_request(const char *service);
int cx_snprintf(char *buf, unsigned long n, const char *fmt, ...);
int cx_fdprintf(int fd, const char *fmt, ...);
void cx_un_kick(void);
void cx_un_lock(void);
void cx_un_unlock(void);
id cx_un_props(id o);

extern cx_class_t NSUserNotification_c __asm__("OBJC_CLASS_$_NSUserNotification");
extern cx_class_t NSUserNotificationCenter_c __asm__("OBJC_CLASS_$_NSUserNotificationCenter");
extern cx_class_t NSUserNotification_m __asm__("OBJC_METACLASS_$_NSUserNotification");
extern cx_class_t NSUserNotificationCenter_m __asm__("OBJC_METACLASS_$_NSUserNotificationCenter");
cx_class_t NSUserNotificationAction_c __asm__("OBJC_CLASS_$_NSUserNotificationAction");
cx_class_t NSUserNotificationAction_m __asm__("OBJC_METACLASS_$_NSUserNotificationAction");

static id cls(const char *n) { return (id)cx_class_named(n); }
static BOOL responds(id o, const char *sel) { return o && cx_responds(object_getClass(o), sel_registerName(sel)); }
static const char *cs(id s) { return s && cx_is_str(s) ? cx_cstr(s) : 0; }
static id pget(id o, const char *k) { return o ? MSG(id, cx_un_props(o), "objectForKey:", cx_str(k)) : 0; }
static void pset(id o, const char *k, id v) {
    id d = cx_un_props(o);
    if (v) MSG(void, d, "setObject:forKey:", v, cx_str(k)); else MSG(void, d, "removeObjectForKey:", cx_str(k));
}
static id num(long long v) { return MSG(id, cls("NSNumber"), "numberWithLongLong:", v); }
static long long pnum(id o, const char *k, long long dflt) { id n = pget(o, k); return n ? MSG0(long long, n, "longLongValue") : dflt; }

/* ---------------------------------------------------------------- propriedades: o nome do seletor é a chave */
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
    if (!strncmp(n, "is", 2) && n[2] >= 'A' && n[2] <= 'Z') { cx_snprintf(out, cap, "%c%s", n[2] + 32, n + 3); return; }
    cx_snprintf(out, cap, "%s", n);
}
static id g_obj(id self, SEL c) { char k[64]; key_of(c, k, sizeof k); return pget(self, k); }
static void s_obj(id self, SEL c, id v) {
    char k[64];
    key_of(c, k, sizeof k);
    id cp = v && responds(v, "copyWithZone:") ? MSG0(id, v, "copy") : objc_retain(v);
    pset(self, k, cp);
    objc_release(cp);
}
static BOOL g_bool(id self, SEL c) { char k[64]; key_of(c, k, sizeof k); return pnum(self, k, 0) ? YES : NO; }
static void s_bool(id self, SEL c, BOOL v) { char k[64]; key_of(c, k, sizeof k); pset(self, k, num(v ? 1 : 0)); }
static long long g_act_type(id self, SEL c) { (void)c; return pnum(self, "activationType", 0); }
/* padrões da Apple: com botão de ação, "Show" e "Close" */
static BOOL g_has_action(id self, SEL c) { (void)c; return pnum(self, "hasActionButton", 1) ? YES : NO; }
static id g_action_title(id self, SEL c) { (void)c; id v = pget(self, "actionButtonTitle"); return v ? v : cx_str("Show"); }
static id g_other_title(id self, SEL c) { (void)c; id v = pget(self, "otherButtonTitle"); return v ? v : cx_str("Close"); }
static id note_init(id self, SEL c) {
    (void)c;
    if (!self) return 0;
    id uuid = MSG0(id, MSG0(id, cls("NSUUID"), "UUID"), "UUIDString");
    pset(self, "identifier", uuid);
    return self;
}
static id note_copy(id self, SEL c) {
    (void)c;
    id o = MSG0(id, MSG0(id, (id)object_getClass(self), "alloc"), "init");
    id m = MSG0(id, cx_un_props(self), "mutableCopy");
    id d = cx_un_props(o);
    MSG(void, d, "setDictionary:", m);
    objc_release(m);
    return o;
}
static id act_with(id k, SEL c, id ident, id title) {
    (void)c;
    id o = objc_autorelease(MSG0(id, MSG0(id, k, "alloc"), "init"));
    pset(o, "identifier", ident);
    pset(o, "title", title);
    return o;
}

/* ---------------------------------------------------------------- notificação <-> registro C */
static id plist_text(id obj) {
    id data = ((id (*)(id, SEL, id, u64, u64, id *))objc_msgSend)(cls("NSPropertyListSerialization"),
              sel_registerName("dataWithPropertyList:format:options:error:"), obj, 100, 0, (id *)0);
    return data ? objc_autorelease(MSG(id, MSG0(id, cls("NSString"), "alloc"), "initWithData:encoding:", data, (u64)4)) : 0;
}
static id plist_obj(const char *t) {
    if (!t || !t[0]) return 0;
    id data = MSG(id, cls("NSData"), "dataWithBytes:length:", (const void *)t, (u64)strlen(t));
    return ((id (*)(id, SEL, id, u64, u64 *, id *))objc_msgSend)(cls("NSPropertyListSerialization"),
            sel_registerName("propertyListWithData:options:format:error:"), data, 0, (u64 *)0, (id *)0);
}
/* o que não cabe no registro vai num plist em info: userInfo, botões e ações */
static const char *const extra_keys[] = {"hasActionButton", "actionButtonTitle", "otherButtonTitle", "hasReplyButton", "responsePlaceholder"};
static id extras_of(id n) {
    id d = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    for (unsigned i = 0; i < sizeof extra_keys / sizeof extra_keys[0]; i++) { id v = pget(n, extra_keys[i]); if (v) MSG(void, d, "setObject:forKey:", v, cx_str(extra_keys[i])); }
    id ui = pget(n, "userInfo");
    if (ui) MSG(void, d, "setObject:forKey:", ui, cx_str("userInfo"));
    id acts = pget(n, "additionalActions");
    u64 na = acts ? MSG0(u64, acts, "count") : 0;
    if (na) {
        id l = MSG0(id, cls("NSMutableArray"), "array");
        for (u64 i = 0; i < na; i++) {
            id a = MSG(id, acts, "objectAtIndex:", i);
            id pair = MSG0(id, cls("NSMutableArray"), "array");
            MSG(void, pair, "addObject:", pget(a, "identifier") ? pget(a, "identifier") : cx_str(""));
            MSG(void, pair, "addObject:", pget(a, "title") ? pget(a, "title") : cx_str(""));
            MSG(void, l, "addObject:", pair);
        }
        MSG(void, d, "setObject:forKey:", l, cx_str("additionalActions"));
    }
    return d;
}
static void apply_extras(id n, id d) {
    if (!d) return;
    for (unsigned i = 0; i < sizeof extra_keys / sizeof extra_keys[0]; i++) { id v = MSG(id, d, "objectForKey:", cx_str(extra_keys[i])); if (v) pset(n, extra_keys[i], v); }
    id ui = MSG(id, d, "objectForKey:", cx_str("userInfo"));
    if (ui) pset(n, "userInfo", ui);
    id l = MSG(id, d, "objectForKey:", cx_str("additionalActions"));
    u64 na = l ? MSG0(u64, l, "count") : 0;
    if (na) {
        id arr = MSG0(id, cls("NSMutableArray"), "array");
        for (u64 i = 0; i < na; i++) {
            id pair = MSG(id, l, "objectAtIndex:", i);
            id ai = MSG(id, pair, "objectAtIndex:", (u64)0), at = MSG(id, pair, "objectAtIndex:", (u64)1);
            id a = act_with((id)&NSUserNotificationAction_c, 0, ai, at);
            MSG(void, arr, "addObject:", a);
        }
        pset(n, "additionalActions", arr);
    }
}
static void cat_name(const char *ident, char *out, size_t cap) { cx_snprintf(out, cap, CX_UN_INTERNAL_CAT "%s", ident); }
/* os botões do banner: [ação] [responder] [adicionais...] */
/* as categorias internas de notificações que já saíram das listas são descartadas de tempos em tempos */
static void sweep_categories(void) {
    int internal = 0;
    for (int i = 0; i < cx_un_category_count(); i++)
        if (!strncmp(cx_un_category_at(i)->ident, CX_UN_INTERNAL_CAT, sizeof CX_UN_INTERNAL_CAT - 1)) internal++;
    if (internal < 128) return;
    for (int i = cx_un_category_count() - 1; i >= 0; i--) {
        const cx_un_category_t *k = cx_un_category_at(i);
        if (strncmp(k->ident, CX_UN_INTERNAL_CAT, sizeof CX_UN_INTERNAL_CAT - 1)) continue;
        const char *id_ = k->ident + sizeof CX_UN_INTERNAL_CAT - 1;
        if (cx_un_find(0, id_) || cx_un_find(1, id_)) continue;
        char name[200];
        cx_snprintf(name, sizeof name, "%s", k->ident);
        cx_un_category_t gone = {name, 0, 0, -1};
        cx_un_put_category(&gone);
    }
}
static void put_category(id n, const char *ident) {
    sweep_categories();
    char name[200];
    cat_name(ident, name, sizeof name);
    cx_un_action_t acts[4];
    int na = 0;
    memset(acts, 0, sizeof acts);
    if (pnum(n, "hasReplyButton", 0)) {
        acts[na].ident = "__reply"; acts[na].title = "Responder"; acts[na].text_input = 1;
        acts[na].button = "Enviar"; acts[na].placeholder = (char *)cs(pget(n, "responsePlaceholder"));
        na++;
    } else if (pnum(n, "hasActionButton", 1)) {
        acts[na].ident = "__action"; acts[na].title = (char *)(cs(pget(n, "actionButtonTitle")) ? cs(pget(n, "actionButtonTitle")) : "Show");
        na++;
    }
    id more = pget(n, "additionalActions");
    u64 nm = more ? MSG0(u64, more, "count") : 0;
    char ids[4][160];
    for (u64 i = 0; i < nm && na < 4; i++) {
        id a = MSG(id, more, "objectAtIndex:", i);
        cx_snprintf(ids[na], sizeof ids[na], "__more:%s", cs(pget(a, "identifier")) ? cs(pget(a, "identifier")) : "");
        acts[na].ident = ids[na];
        acts[na].title = (char *)(cs(pget(a, "title")) ? cs(pget(a, "title")) : "");
        na++;
    }
    cx_un_category_t cat = {name, 0, acts, na};
    cx_un_put_category(&cat);
}
static int comp(id comps, const char *sel) {
    if (!comps || !responds(comps, sel)) return 0;
    long long v = MSG0(long long, comps, sel);
    return v == 0x7fffffffffffffffLL ? 0 : (int)v;
}
/* -1 = não dá para repetir assim */
static int to_req(id n, cx_un_req_t *r, double first) {
    memset(r, 0, sizeof *r);
    r->cal = (cx_un_cal_t){-1, -1, -1, -1, -1, -1, -1};
    r->badge = -1;
    r->source = CX_UN_SRC_LEGACY;
    r->ident = (char *)cs(pget(n, "identifier"));
    if (!r->ident) return -1;
    r->title = (char *)cs(pget(n, "title"));
    r->subtitle = (char *)cs(pget(n, "subtitle"));
    r->body = (char *)cs(pget(n, "informativeText"));
    r->sound = (char *)cs(pget(n, "soundName"));
    if (r->sound && !strcmp(r->sound, "DefaultSoundName")) r->sound = "default";
    r->info = (char *)cs(plist_text(extras_of(n)));
    static char catbuf[200];
    cat_name(r->ident, catbuf, sizeof catbuf);
    r->category = catbuf;
    id rep = pget(n, "deliveryRepeatInterval");
    double now = cx_un_now();
    if (first <= now && !rep) return 0;                 /* agora */
    if (!rep) { r->trig = CX_UN_TRIG_INTERVAL; r->interval = first - now; return 0; }
    int mo = comp(rep, "month"), yr = comp(rep, "year");
    long long secs = comp(rep, "day") * 86400LL + comp(rep, "hour") * 3600LL + comp(rep, "minute") * 60LL + comp(rep, "second") +
                     (comp(rep, "weekOfYear") + comp(rep, "week")) * 604800LL;
    if (!mo && !yr && secs >= 60) {
        r->trig = CX_UN_TRIG_INTERVAL;
        r->repeats = 1;
        r->interval = (double)secs;
        r->fire_at = first > now ? first : now + (double)secs;
        return 0;
    }
    if ((mo == 1 && !yr && !secs) || (yr == 1 && !mo && !secs)) {
        /* todo mês no mesmo dia e hora / todo ano no mesmo dia: componentes da primeira entrega (hora local) */
        id cal = MSG0(id, cls("NSCalendar"), "currentCalendar");
        id date = MSG(id, cls("NSDate"), "dateWithTimeIntervalSince1970:", first > now ? first : now);
        u64 units = (1u << 3) | (1u << 4) | (1u << 5) | (1u << 6) | (1u << 7) | (1u << 8);   /* mês, dia, hora, minuto, segundo */
        id c = cal ? ((id (*)(id, SEL, u64, id))objc_msgSend)(cal, sel_registerName("components:fromDate:"), units, date) : 0;
        if (!c) return -1;
        r->trig = CX_UN_TRIG_CALENDAR;
        r->repeats = 1;
        r->cal.day = comp(c, "day");
        r->cal.hour = comp(c, "hour");
        r->cal.minute = comp(c, "minute");
        r->cal.second = comp(c, "second");
        if (yr) r->cal.month = comp(c, "month");
        return 0;
    }
    return -1;
}
static id from_req(const cx_un_req_t *r, int delivered) {
    id n = objc_autorelease(MSG0(id, MSG0(id, (id)&NSUserNotification_c, "alloc"), "init"));
    pset(n, "identifier", cx_str(r->ident));
    if (r->title) pset(n, "title", cx_str(r->title));
    if (r->subtitle) pset(n, "subtitle", cx_str(r->subtitle));
    if (r->body) pset(n, "informativeText", cx_str(r->body));
    if (r->sound) pset(n, "soundName", cx_str(!strcmp(r->sound, "default") ? "DefaultSoundName" : r->sound));
    apply_extras(n, plist_obj(r->info));
    if (delivered) {
        pset(n, "delivered", num(1));
        pset(n, "presented", num(r->token ? 1 : 0));
        pset(n, "actualDeliveryDate", MSG(id, cls("NSDate"), "dateWithTimeIntervalSince1970:", r->date));
    } else if (r->fire_at > 0) pset(n, "deliveryDate", MSG(id, cls("NSDate"), "dateWithTimeIntervalSince1970:", r->fire_at));
    return n;
}

/* ---------------------------------------------------------------- centro */
static id g_center, g_delegate;
static id center_default(id k, SEL c) { (void)c; if (!g_center) g_center = objc_retain(MSG0(id, MSG0(id, k, "alloc"), "init")); return g_center; }
static void center_set_delegate(id self, SEL c, id d) { (void)self; (void)c; g_delegate = d; }
static id center_delegate(id self, SEL c) { (void)self; (void)c; return g_delegate; }
static void sync_auth(void) {
    int st = cx_tcc_state("userNotifications");
    if (st == 0) cx_tcc_request("userNotifications");   /* a API antiga não pede antes: o primeiro envio pergunta */
    cx_un_set_auth(st == 2 ? CX_UN_AUTHORIZED : st == 1 ? CX_UN_DENIED : CX_UN_NOT_DETERMINED);
}
static void add(id n, double first) {
    if (!n) return;
    void *pool = objc_autoreleasePoolPush();
    cx_un_lock();
    sync_auth();
    cx_un_req_t r;
    if (to_req(n, &r, first) == 0) {
        put_category(n, r.ident);
        if (cx_un_add(&r) != CX_UN_OK) cx_fdprintf(2, "[notificação] não entregue: o app não tem permissão (Ajustes > Notificações)\n");
    } else cx_fdprintf(2, "[notificação] deliveryRepeatInterval sem equivalente (use dias, horas, semanas, 1 mês ou 1 ano)\n");
    cx_un_unlock();
    cx_un_kick();
    objc_autoreleasePoolPop(pool);
}
static void center_deliver(id self, SEL c, id n) { (void)self; (void)c; add(n, 0); }
static void center_schedule(id self, SEL c, id n) {
    (void)self; (void)c;
    id d = n ? pget(n, "deliveryDate") : 0;
    add(n, d ? MSG0(double, d, "timeIntervalSince1970") : 0);
}
static id list(int delivered) {
    id arr = MSG0(id, cls("NSMutableArray"), "array");
    cx_un_lock();
    for (int i = 0; i < cx_un_count(delivered); i++) {
        const cx_un_req_t *r = cx_un_at(delivered, i);
        if (r->source == CX_UN_SRC_LEGACY) MSG(void, arr, "addObject:", from_req(r, delivered));
    }
    cx_un_unlock();
    return arr;
}
static id center_delivered(id self, SEL c) { (void)self; (void)c; return list(1); }
static id center_scheduled(id self, SEL c) { (void)self; (void)c; return list(0); }
static void drop_cat(const char *ident) {
    char name[200];
    cat_name(ident, name, sizeof name);
    cx_un_category_t gone = {name, 0, 0, -1};
    cx_un_put_category(&gone);
}
static void center_set_scheduled(id self, SEL c, id arr) {
    (void)self; (void)c;
    cx_un_lock();
    cx_un_remove_pending(0, CX_UN_SRC_LEGACY);
    cx_un_unlock();
    u64 n = arr ? MSG0(u64, arr, "count") : 0;
    for (u64 i = 0; i < n; i++) center_schedule(0, 0, MSG(id, arr, "objectAtIndex:", i));
}
static void center_rm_scheduled(id self, SEL c, id n) {
    (void)self; (void)c;
    const char *id_ = n ? cs(pget(n, "identifier")) : 0;
    if (!id_) return;
    cx_un_lock();
    cx_un_remove_pending(id_, CX_UN_SRC_LEGACY);
    cx_un_unlock();
}
static void center_rm_delivered(id self, SEL c, id n) {
    (void)self; (void)c;
    const char *id_ = n ? cs(pget(n, "identifier")) : 0;
    if (!id_) return;
    cx_un_lock();
    cx_un_remove_delivered(id_, CX_UN_SRC_LEGACY);
    if (!cx_un_find(0, id_)) drop_cat(id_);
    cx_un_unlock();
}
static void center_rm_all_delivered(id self, SEL c) { (void)self; (void)c; cx_un_lock(); cx_un_remove_delivered(0, CX_UN_SRC_LEGACY); cx_un_unlock(); }

/* ---------------------------------------------------------------- do centro para o app (fio principal) */
static int app_active(void) {
    cx_class_t *k = cx_class_named("NSApplication");
    id app = k ? MSG0(id, (id)k, "sharedApplication") : 0;
    return app && responds(app, "isActive") && MSG0(BOOL, app, "isActive");
}
int cx_nsun_present(const cx_un_req_t *r) {
    id n = from_req(r, 1);
    int show = 1;
    if (app_active()) {
        show = 0;
        if (g_delegate && responds(g_delegate, "userNotificationCenter:shouldPresentNotification:"))
            show = MSG(BOOL, g_delegate, "userNotificationCenter:shouldPresentNotification:", g_center, n) ? 1 : 0;
    }
    pset(n, "presented", num(show));
    if (g_delegate && responds(g_delegate, "userNotificationCenter:didDeliverNotification:"))
        MSG(void, g_delegate, "userNotificationCenter:didDeliverNotification:", g_center, n);
    /* entregue sempre aparece na lista de entregues (deliveredNotifications), como no macOS */
    return show ? (CX_UN_PRESENT_BANNER | CX_UN_PRESENT_LIST | CX_UN_PRESENT_SOUND) : CX_UN_PRESENT_LIST;
}
void cx_nsun_receive(const cx_un_req_t *r, const char *action, const char *text) {
    if (!strcmp(action, CX_UN_ACTION_DISMISS)) return;
    id n = from_req(r, 1);
    long long type = 1;                                  /* NSUserNotificationActivationTypeContentsClicked */
    if (!strcmp(action, "__action")) type = 2;           /* ActionButtonClicked */
    else if (!strcmp(action, "__reply")) {
        type = 3;                                        /* Replied */
        id s = MSG(id, MSG0(id, cls("NSAttributedString"), "alloc"), "initWithString:", cx_str(text ? text : ""));
        pset(n, "response", s);
        objc_release(s);
    } else if (!strncmp(action, "__more:", 7)) {
        type = 4;                                        /* AdditionalActionClicked */
        id more = pget(n, "additionalActions");
        u64 nm = more ? MSG0(u64, more, "count") : 0;
        for (u64 i = 0; i < nm; i++) {
            id a = MSG(id, more, "objectAtIndex:", i);
            if (cs(pget(a, "identifier")) && !strcmp(cs(pget(a, "identifier")), action + 7)) pset(n, "additionalActivationAction", a);
        }
    }
    pset(n, "activationType", num(type));
    if (g_delegate && responds(g_delegate, "userNotificationCenter:didActivateNotification:"))
        MSG(void, g_delegate, "userNotificationCenter:didActivateNotification:", g_center, n);
}

/* ---------------------------------------------------------------- registro */
void cx_nsusernotification_init(void) {
    static int done;
    if (done++) return;
    /* As duas classes nascem aqui: o AppKit (cx_appkit_init) chama este init antes do resto do Foundation. */
    cx_class_t *ob = cx_class_named("NSObject");
    cx_define_class(&NSUserNotification_c, &NSUserNotification_m, "NSUserNotification", ob, 0);
    cx_define_class(&NSUserNotificationCenter_c, &NSUserNotificationCenter_m, "NSUserNotificationCenter", ob, 0);
    cx_class_t *n = &NSUserNotification_c;
    cx_add_method(n, "init", (void *)note_init);
    static const char *const objs[] = {"title", "subtitle", "informativeText", "soundName", "userInfo", "identifier", "deliveryDate",
                                       "deliveryTimeZone", "deliveryRepeatInterval", "contentImage", "responsePlaceholder",
                                       "additionalActions", "response", "actualDeliveryDate", "additionalActivationAction"};
    for (unsigned i = 0; i < sizeof objs / sizeof objs[0]; i++) {
        cx_add_method(n, objs[i], (void *)g_obj);
        char set[80];
        cx_snprintf(set, sizeof set, "set%c%s:", objs[i][0] - 32, objs[i] + 1);
        cx_add_method(n, sel_registerName(set), (void *)s_obj);
    }
    cx_add_method(n, "hasActionButton", (void *)g_has_action);
    cx_add_method(n, "setHasActionButton:", (void *)s_bool);
    cx_add_method(n, "hasReplyButton", (void *)g_bool);
    cx_add_method(n, "setHasReplyButton:", (void *)s_bool);
    cx_add_method(n, "actionButtonTitle", (void *)g_action_title);
    cx_add_method(n, "setActionButtonTitle:", (void *)s_obj);
    cx_add_method(n, "otherButtonTitle", (void *)g_other_title);
    cx_add_method(n, "setOtherButtonTitle:", (void *)s_obj);
    cx_add_method(n, "isPresented", (void *)g_bool);
    cx_add_method(n, "isRemote", (void *)g_bool);
    cx_add_method(n, "isDelivered", (void *)g_bool);
    cx_add_method(n, "activationType", (void *)g_act_type);
    cx_add_method(n, "copyWithZone:", (void *)note_copy);
    cx_add_method(n, "copy", (void *)note_copy);

    cx_define_class(&NSUserNotificationAction_c, &NSUserNotificationAction_m, "NSUserNotificationAction", cx_class_named("NSObject"), 0);
    cx_add_cmethod(&NSUserNotificationAction_c, "actionWithIdentifier:title:", (void *)act_with);
    cx_add_method(&NSUserNotificationAction_c, "identifier", (void *)g_obj);
    cx_add_method(&NSUserNotificationAction_c, "title", (void *)g_obj);

    cx_class_t *c = &NSUserNotificationCenter_c;
    cx_add_cmethod(c, "defaultUserNotificationCenter", (void *)center_default);
    cx_add_method(c, "setDelegate:", (void *)center_set_delegate);
    cx_add_method(c, "delegate", (void *)center_delegate);
    cx_add_method(c, "deliverNotification:", (void *)center_deliver);
    cx_add_method(c, "scheduleNotification:", (void *)center_schedule);
    cx_add_method(c, "scheduledNotifications", (void *)center_scheduled);
    cx_add_method(c, "setScheduledNotifications:", (void *)center_set_scheduled);
    cx_add_method(c, "removeScheduledNotification:", (void *)center_rm_scheduled);
    cx_add_method(c, "deliveredNotifications", (void *)center_delivered);
    cx_add_method(c, "removeDeliveredNotification:", (void *)center_rm_delivered);
    cx_add_method(c, "removeAllDeliveredNotifications", (void *)center_rm_all_delivered);
}
