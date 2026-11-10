/* Métodos de UserNotifications (I23) em cima de un_center.c.
 * Autorização continua no TCC (userNotifications). O banner visível é o do
 * desktop (notify_tagged, syscall MOS_DESK_BANNER). O clique volta por MOS_DESK_TAKE
 * e chama didReceive. Não há APNs. */
#include "foundation.h"
#include "libc.h"
#include "un_center.h"
#include "../../kernel/include/syscall.h"

void *_Block_copy(const void *b);
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
int cx_tcc_state(const char *service);
extern cx_class_t _NSConcreteStackBlock_c __asm__("_NSConcreteStackBlock");

extern cx_class_t UNMutableNotificationContent_c __asm__("OBJC_CLASS_$_UNMutableNotificationContent");
extern cx_class_t UNMutableNotificationContent_m __asm__("OBJC_METACLASS_$_UNMutableNotificationContent");
extern cx_class_t UNNotificationRequest_c __asm__("OBJC_CLASS_$_UNNotificationRequest");
extern cx_class_t UNUserNotificationCenter_c __asm__("OBJC_CLASS_$_UNUserNotificationCenter");

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

enum { UN_SLOTS = 48 };

typedef struct {
    id obj;
    char title[96], subtitle[96], body[160], ident[64], action[80];
    double interval;
    int repeats, used;
    id content, trigger, ident_obj, note;
} un_slot_t;

static un_slot_t g_slot[UN_SLOTS];
static id g_delegate;
static void (*g_prev_hook)(double until);

static un_slot_t *slot_get(id o, int make) {
    un_slot_t *free_s = 0;
    if (!o) return 0;
    for (int i = 0; i < UN_SLOTS; i++) {
        if (g_slot[i].used && g_slot[i].obj == o) return &g_slot[i];
        if (!g_slot[i].used && !free_s) free_s = &g_slot[i];
    }
    if (!make || !free_s) return 0;
    memset(free_s, 0, sizeof *free_s);
    free_s->used = 1;
    free_s->obj = o;
    return free_s;
}

static void copy_field(char *d, int cap, id s) {
    const char *p;
    int i = 0;
    if (!s) { if (cap) d[0] = 0; return; }
    p = cx_cstr(s);
    if (!p || !strcmp(p, "(null)")) p = "";
    for (; i < cap - 1 && p[i]; i++) d[i] = p[i];
    d[i] = 0;
}

static id str_ar(const char *s) { return objc_autorelease(cx_str(s ? s : "")); }

static id ct_title(id self, SEL c) { un_slot_t *s = slot_get(self, 0); (void)c; return str_ar(s ? s->title : ""); }
static id ct_subtitle(id self, SEL c) { un_slot_t *s = slot_get(self, 0); (void)c; return str_ar(s ? s->subtitle : ""); }
static id ct_body(id self, SEL c) { un_slot_t *s = slot_get(self, 0); (void)c; return str_ar(s ? s->body : ""); }
static void ct_set_title(id self, SEL c, id v) { un_slot_t *s = slot_get(self, 1); (void)c; if (s) copy_field(s->title, (int)sizeof s->title, v); }
static void ct_set_subtitle(id self, SEL c, id v) { un_slot_t *s = slot_get(self, 1); (void)c; if (s) copy_field(s->subtitle, (int)sizeof s->subtitle, v); }
static void ct_set_body(id self, SEL c, id v) { un_slot_t *s = slot_get(self, 1); (void)c; if (s) copy_field(s->body, (int)sizeof s->body, v); }
static id ct_userinfo(id self, SEL c) { (void)self; (void)c; return objc_autorelease(MSG0(id, (id)cx_class_named("NSDictionary"), "dictionary")); }

static id trig_with(id cls, SEL c, double interval, BOOL repeats) {
    id o = cx_alloc_instance((cx_class_t *)cls);
    un_slot_t *s = slot_get(o, 1);
    (void)c;
    if (s) { s->interval = interval; s->repeats = repeats ? 1 : 0; }
    return objc_autorelease(o);
}
static double trig_interval(id self, SEL c) { un_slot_t *s = slot_get(self, 0); (void)c; return s ? s->interval : 0; }
static BOOL trig_repeats(id self, SEL c) { un_slot_t *s = slot_get(self, 0); (void)c; return s && s->repeats ? YES : NO; }

static id req_with(id cls, SEL c, id ident, id content, id trigger) {
    id o = cx_alloc_instance((cx_class_t *)cls);
    un_slot_t *s = slot_get(o, 1);
    (void)c;
    if (s) {
        copy_field(s->ident, (int)sizeof s->ident, ident);
        s->ident_obj = ident ? objc_retain(ident) : 0;
        s->content = content ? objc_retain(content) : 0;
        s->trigger = trigger ? objc_retain(trigger) : 0;
    }
    return objc_autorelease(o);
}
static id req_ident(id self, SEL c) { un_slot_t *s = slot_get(self, 0); (void)c; return str_ar(s ? s->ident : ""); }
static id req_content(id self, SEL c) { un_slot_t *s = slot_get(self, 0); (void)c; return s ? s->content : 0; }
static id req_trigger(id self, SEL c) { un_slot_t *s = slot_get(self, 0); (void)c; return s ? s->trigger : 0; }

static id make_note(const char *ident, const char *title, const char *body) {
    id o = cx_alloc_instance(&UNNotification_c);
    un_slot_t *s = slot_get(o, 1);
    if (s) {
        int i;
        for (i = 0; ident && ident[i] && i < (int)sizeof s->ident - 1; i++) s->ident[i] = ident[i];
        s->ident[i] = 0;
        for (i = 0; title && title[i] && i < (int)sizeof s->title - 1; i++) s->title[i] = title[i];
        s->title[i] = 0;
        for (i = 0; body && body[i] && i < (int)sizeof s->body - 1; i++) s->body[i] = body[i];
        s->body[i] = 0;
    }
    return o;
}
static void copy_buf(char *d, int cap, const char *s) {
    int i = 0;
    if (!s) s = "";
    for (; i < cap - 1 && s[i]; i++) d[i] = s[i];
    d[i] = 0;
}

static id note_req(id self, SEL c) {
    un_slot_t *s = slot_get(self, 0);
    id content, req;
    un_slot_t *cs;
    (void)c;
    if (!s) return 0;
    if (s->ident_obj) return s->ident_obj;
    content = cx_alloc_instance(&UNMutableNotificationContent_c);
    cs = slot_get(content, 1);
    if (cs) {
        copy_buf(cs->title, (int)sizeof cs->title, s->title);
        copy_buf(cs->body, (int)sizeof cs->body, s->body);
    }
    req = req_with((id)&UNNotificationRequest_c, 0, objc_autorelease(cx_str(s->ident)), content, 0);
    s->content = content;
    s->ident_obj = objc_retain(req);
    return s->ident_obj;
}

static id make_response(const char *ident, const char *action, id note) {
    id o = cx_alloc_instance(&UNNotificationResponse_c);
    un_slot_t *s = slot_get(o, 1);
    if (s) {
        int i;
        for (i = 0; action && action[i] && i < (int)sizeof s->action - 1; i++) s->action[i] = action[i];
        s->action[i] = 0;
        for (i = 0; ident && ident[i] && i < (int)sizeof s->ident - 1; i++) s->ident[i] = ident[i];
        s->ident[i] = 0;
        s->note = note ? objc_retain(note) : 0;
    }
    return o;
}
static id resp_note(id self, SEL c) { un_slot_t *s = slot_get(self, 0); (void)c; return s ? s->note : 0; }
static id resp_action(id self, SEL c) { un_slot_t *s = slot_get(self, 0); (void)c; return str_ar(s ? s->action : ""); }

typedef struct { unsigned long reserved, size; void (*copy)(void *, const void *); void (*dispose)(const void *); } un_desc_t;
typedef struct { unsigned long opts; int called; } un_out_t;

/* O descritor fica no layout da Apple: um _Block_copy copia o ponteiro `out`
 * e o invoke grava no mesmo lugar, esteja o bloco na pilha ou no heap. */
typedef struct {
    void *isa;
    int flags, reserved;
    void (*invoke)(void *, unsigned long);
    un_desc_t *desc;
    un_out_t *out;
} opt_blk_t;

typedef struct {
    void *isa;
    int flags, reserved;
    void (*invoke)(void *);
    un_desc_t *desc;
    un_out_t *out;
} void_blk_t;

typedef struct {
    void *isa;
    int flags, reserved;
    void (*invoke)(void *, id);
} err_blk_t;

static void opt_invoke(void *self, unsigned long opts) {
    opt_blk_t *b = self;
    if (!b->out) return;
    b->out->opts = opts;
    b->out->called = 1;
}
static void void_invoke(void *self) {
    void_blk_t *b = self;
    if (b->out) b->out->called = 1;
}

static void call_err(void *block, id err) {
    err_blk_t *b = block;
    if (b && b->invoke) b->invoke(block, err);
}

static id un_error(void) {
    cx_class_t *k = cx_class_named("NSError");
    return k ? objc_autorelease(cx_alloc_instance(k)) : cx_str("UNError");
}

static void sync_auth(void) {
    int st = cx_tcc_state("userNotifications");
    if (st == 2) cx_un_set_auth(CX_UN_AUTHORIZED);
    else if (st == 1) cx_un_set_auth(CX_UN_DENIED);
    else cx_un_set_auth(CX_UN_NOT_DETERMINED);
}

static int objc_present(const char *ident, const char *title, const char *body, void *ctx) {
    static un_desc_t desc = { 0, sizeof(opt_blk_t), 0, 0 };
    opt_blk_t blk;
    un_out_t out;
    id center, note;
    SEL sel;
    (void)ctx;
    if (!g_delegate) return CX_UN_PRESENT_BANNER | CX_UN_PRESENT_LIST;
    sel = sel_registerName("userNotificationCenter:willPresentNotification:withCompletionHandler:");
    if (!cx_responds(object_getClass(g_delegate), sel)) return CX_UN_PRESENT_BANNER | CX_UN_PRESENT_LIST;
    center = MSG0(id, (id)&UNUserNotificationCenter_c, "currentNotificationCenter");
    note = make_note(ident, title, body);
    memset(&out, 0, sizeof out);
    out.opts = CX_UN_PRESENT_BANNER | CX_UN_PRESENT_LIST;
    memset(&blk, 0, sizeof blk);
    blk.isa = &_NSConcreteStackBlock_c;
    blk.invoke = opt_invoke;
    blk.desc = &desc;
    blk.out = &out;
    ((void (*)(id, SEL, id, id, void *))objc_msgSend)(g_delegate, sel, center, note, &blk);
    return out.called ? (int)out.opts : (CX_UN_PRESENT_BANNER | CX_UN_PRESENT_LIST);
}

static void objc_receive(const char *ident, const char *action, void *ctx) {
    static un_desc_t desc = { 0, sizeof(void_blk_t), 0, 0 };
    void_blk_t blk;
    un_out_t out;
    id center, note, resp;
    SEL sel;
    (void)ctx;
    if (!g_delegate) return;
    sel = sel_registerName("userNotificationCenter:didReceiveNotificationResponse:withCompletionHandler:");
    if (!cx_responds(object_getClass(g_delegate), sel)) return;
    center = MSG0(id, (id)&UNUserNotificationCenter_c, "currentNotificationCenter");
    note = make_note(ident, "", "");
    resp = make_response(ident, action, note);
    memset(&out, 0, sizeof out);
    memset(&blk, 0, sizeof blk);
    blk.isa = &_NSConcreteStackBlock_c;
    blk.invoke = void_invoke;
    blk.desc = &desc;
    blk.out = &out;
    ((void (*)(id, SEL, id, id, void *))objc_msgSend)(g_delegate, sel, center, resp, &blk);
    if (!out.called) void_invoke(&blk);
}

static void kernel_banner(const char *title, const char *body, int token, void *ctx) {
    (void)ctx;
    SYS4(SYS_DESKTOP, MOS_DESK_BANNER, title ? title : "", body ? body : "", token);
}

static void poll_clicks(void) {
    for (int n = 0; n < 8; n++) {
        int token = 0;
        long r = SYS2(SYS_DESKTOP, MOS_DESK_TAKE, &token);
        if (r <= 0) return;
        cx_un_click_token(token);
    }
}

static void un_runloop(double until) {
    cx_un_flush();
    poll_clicks();
    if (g_prev_hook) g_prev_hook(until);
}

void cx_un_install_runloop(void) {
    if (cx_runloop_hook == un_runloop) return;
    g_prev_hook = cx_runloop_hook;
    cx_runloop_hook = un_runloop;
}

static void un_set_delegate(id self, SEL c, id d) {
    (void)self; (void)c;
    if (g_delegate) objc_release(g_delegate);
    g_delegate = d ? objc_retain(d) : 0;
}
static id un_delegate(id self, SEL c) { (void)self; (void)c; return g_delegate; }

static void un_add(id self, SEL c, id req, void *block) {
    un_slot_t *rs, *cs, *ts;
    int rc;
    (void)self; (void)c;
    sync_auth();
    if (!req) { call_err(block, un_error()); return; }
    rs = slot_get(req, 0);
    cs = rs && rs->content ? slot_get(rs->content, 0) : 0;
    ts = rs && rs->trigger ? slot_get(rs->trigger, 0) : 0;
    rc = cx_un_add(rs ? rs->ident : "",
                   cs ? cs->title : "",
                   cs ? cs->subtitle : "",
                   cs ? cs->body : "",
                   ts ? ts->interval : 0,
                   ts ? ts->repeats : 0);
    call_err(block, rc == CX_UN_OK ? 0 : un_error());
    poll_clicks();
}

static void un_rm_pending_ids(id self, SEL c, id ids) {
    u64 n, i;
    (void)self; (void)c;
    if (!ids) return;
    n = MSG0(u64, ids, "count");
    for (i = 0; i < n; i++) {
        id s = MSG(id, ids, "objectAtIndex:", i);
        un_slot_t tmp;
        copy_field(tmp.ident, (int)sizeof tmp.ident, s);
        cx_un_remove_pending(tmp.ident);
    }
}
static void un_rm_all_pending(id self, SEL c) { (void)self; (void)c; cx_un_remove_pending(0); }
static void un_rm_delivered_ids(id self, SEL c, id ids) {
    u64 n, i;
    (void)self; (void)c;
    if (!ids) return;
    n = MSG0(u64, ids, "count");
    for (i = 0; i < n; i++) {
        id s = MSG(id, ids, "objectAtIndex:", i);
        char ident[64];
        copy_field(ident, (int)sizeof ident, s);
        cx_un_remove_delivered(ident);
    }
}
static void un_rm_all_delivered(id self, SEL c) { (void)self; (void)c; cx_un_remove_delivered(0); }

static void add_if_missing(cx_class_t *k, const char *sel, void *imp) {
    if (k && !cx_responds(k, sel_registerName(sel))) cx_add_method(k, sel, imp);
}

void cx_usernotifications_init(void) {
    cx_class_t *obj = cx_class_named("NSObject");
    cx_define_class(&UNNotificationContent_c, &UNNotificationContent_m, "UNNotificationContent", obj, 0);
    cx_define_class(&UNNotificationTrigger_c, &UNNotificationTrigger_m, "UNNotificationTrigger", obj, 0);
    cx_define_class(&UNTimeIntervalNotificationTrigger_c, &UNTimeIntervalNotificationTrigger_m,
                    "UNTimeIntervalNotificationTrigger", &UNNotificationTrigger_c, 0);
    cx_define_class(&UNNotification_c, &UNNotification_m, "UNNotification", obj, 0);
    cx_define_class(&UNNotificationResponse_c, &UNNotificationResponse_m, "UNNotificationResponse", obj, 0);
    /* A classe mutável já nasce como filha de NSObject em vscode_fwclasses.c.
     * O conteúdo da Apple herda de UNNotificationContent: os getters ficam na mãe. */
    UNMutableNotificationContent_c.super = &UNNotificationContent_c;
    UNMutableNotificationContent_m.super = &UNNotificationContent_m;

    cx_add_method(&UNNotificationContent_c, "title", (void *)ct_title);
    cx_add_method(&UNNotificationContent_c, "subtitle", (void *)ct_subtitle);
    cx_add_method(&UNNotificationContent_c, "body", (void *)ct_body);
    cx_add_method(&UNNotificationContent_c, "userInfo", (void *)ct_userinfo);
    cx_add_method(&UNMutableNotificationContent_c, "setTitle:", (void *)ct_set_title);
    cx_add_method(&UNMutableNotificationContent_c, "setSubtitle:", (void *)ct_set_subtitle);
    cx_add_method(&UNMutableNotificationContent_c, "setBody:", (void *)ct_set_body);
    cx_add_cmethod(&UNTimeIntervalNotificationTrigger_c, "triggerWithTimeInterval:repeats:", (void *)trig_with);
    cx_add_method(&UNTimeIntervalNotificationTrigger_c, "timeInterval", (void *)trig_interval);
    cx_add_method(&UNTimeIntervalNotificationTrigger_c, "repeats", (void *)trig_repeats);
    cx_add_cmethod(&UNNotificationRequest_c, "requestWithIdentifier:content:trigger:", (void *)req_with);
    cx_add_method(&UNNotificationRequest_c, "identifier", (void *)req_ident);
    cx_add_method(&UNNotificationRequest_c, "content", (void *)req_content);
    cx_add_method(&UNNotificationRequest_c, "trigger", (void *)req_trigger);
    cx_add_method(&UNNotification_c, "request", (void *)note_req);
    cx_add_method(&UNNotificationResponse_c, "notification", (void *)resp_note);
    cx_add_method(&UNNotificationResponse_c, "actionIdentifier", (void *)resp_action);

    add_if_missing(&UNUserNotificationCenter_c, "setDelegate:", (void *)un_set_delegate);
    add_if_missing(&UNUserNotificationCenter_c, "delegate", (void *)un_delegate);
    add_if_missing(&UNUserNotificationCenter_c, "addNotificationRequest:withCompletionHandler:", (void *)un_add);
    add_if_missing(&UNUserNotificationCenter_c, "removePendingNotificationRequestsWithIdentifiers:", (void *)un_rm_pending_ids);
    add_if_missing(&UNUserNotificationCenter_c, "removeAllPendingNotificationRequests", (void *)un_rm_all_pending);
    add_if_missing(&UNUserNotificationCenter_c, "removeDeliveredNotificationsWithIdentifiers:", (void *)un_rm_delivered_ids);
    add_if_missing(&UNUserNotificationCenter_c, "removeAllDeliveredNotifications", (void *)un_rm_all_delivered);

    cx_un_set_delegate(objc_present, objc_receive, 0);
    cx_un_set_banner_sink(kernel_banner, 0);
}
