/*
 * CallKit, CoreTelephony, CoreLocation e BackgroundTasks no libSystem.
 * O WhatsApp 2.26.45.13 importa as classes e as constantes. O miolo está em wa_small_core.c.
 */
#include "foundation.h"
#include "libc.h"
#include "wa_small_core.h"

void cx_mark_stub_class(cx_class_t *c);

static cx_box Gcx;
static bg_box Gbg;
static id Gprov, Gctl, Gobs, Gdel;
static int Gauth;

#define WAC(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); static cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N)
WAC(CXProvider);
WAC(CXProviderConfiguration);
WAC(CXCallController);
WAC(CXCallObserver);
WAC(CXCallUpdate);
WAC(CXCall);
WAC(CXHandle);
WAC(CXStartCallAction);
WAC(CXAnswerCallAction);
WAC(CXEndCallAction);
WAC(CXSetHeldCallAction);
WAC(CXSetMutedCallAction);
WAC(CXTransaction);
WAC(CTTelephonyNetworkInfo);
WAC(CLGeocoder);
WAC(CLPlacemark);
WAC(BGTaskScheduler);
WAC(BGAppRefreshTaskRequest);
WAC(BGProcessingTaskRequest);
extern cx_class_t CLLocationManager_c __asm__("OBJC_CLASS_$_CLLocationManager");

const char CXErrorDomainIncomingCall[] = "com.apple.CallKit.error.incomingcall";
const char BGTaskSchedulerErrorDomain[] = "BGTaskSchedulerErrorDomain";
const char CTRadioAccessTechnologyGPRS[] = "CTRadioAccessTechnologyGPRS";
const char CTRadioAccessTechnologyEdge[] = "CTRadioAccessTechnologyEdge";
const char CTRadioAccessTechnologyWCDMA[] = "CTRadioAccessTechnologyWCDMA";
const char CTRadioAccessTechnologyHSDPA[] = "CTRadioAccessTechnologyHSDPA";
const char CTRadioAccessTechnologyHSUPA[] = "CTRadioAccessTechnologyHSUPA";
const char CTRadioAccessTechnologyCDMA1x[] = "CTRadioAccessTechnologyCDMA1x";
const char CTRadioAccessTechnologyCDMAEVDORev0[] = "CTRadioAccessTechnologyCDMAEVDORev0";
const char CTRadioAccessTechnologyCDMAEVDORevA[] = "CTRadioAccessTechnologyCDMAEVDORevA";
const char CTRadioAccessTechnologyCDMAEVDORevB[] = "CTRadioAccessTechnologyCDMAEVDORevB";
const char CTRadioAccessTechnologyeHRPD[] = "CTRadioAccessTechnologyeHRPD";
const char CTRadioAccessTechnologyLTE[] = "CTRadioAccessTechnologyLTE";
const char CTRadioAccessTechnologyNRNSA[] = "CTRadioAccessTechnologyNRNSA";
const char CTRadioAccessTechnologyNR[] = "CTRadioAccessTechnologyNR";
const char CTServiceRadioAccessTechnologyDidChangeNotification[] = "CTServiceRadioAccessTechnologyDidChangeNotification";
const double kCLLocationAccuracyNearestTenMeters = 10.0;
const double kCLLocationAccuracyKilometer = 1000.0;
const double kCLLocationAccuracyThreeKilometers = 3000.0;

typedef struct { double latitude, longitude; } cl_coord_t;
cl_coord_t kCLLocationCoordinate2DInvalid;

int cx_tcc_state(const char *service);
int cx_tcc_request(const char *service);
void *dispatch_queue_create(const char *label, void *attr);
void wa_cx_deliver(void *queue, void *ctx, void (*fn)(void *));

static void call_block(void *blk, id err) {
    void (*inv)(void *, id);
    if (!blk) return;
    inv = *(void (**)(void *, id))((unsigned char *)blk + 16);
    if (inv) inv(blk, err);
}
static id make_err(const char *dom, long code, const char *msg) {
    id ui = MSG(id, (id)cx_class_named("NSDictionary"), "dictionaryWithObject:forKey:", cx_str(msg), cx_str("NSLocalizedDescription"));
    return MSG(id, (id)cx_class_named("NSError"), "errorWithDomain:code:userInfo:", cx_str(dom), code, ui);
}
static const char *cstr(id o) {
    const char *s = o ? cx_cstr(o) : 0;
    return s ? s : "";
}

typedef struct {
    id key;
    int kind;
    int idx;
    char uuid[48];
    char handle[80];
    char name[80];
    int flag;
    int handle_type;
    int video;
    int cap_hold, cap_group, cap_ungroup, cap_dtmf;
    int max_groups, max_per;
    unsigned mask;
    id href;
    int outgoing, on_hold, connected, ended;
} slot_t;
static slot_t Slots[48];
static slot_t *slot_of(id o) {
    int i;
    for (i = 0; i < 48; i++) if (Slots[i].key == o) return &Slots[i];
    return 0;
}
static slot_t *slot_new(id o) {
    slot_t *s = slot_of(o);
    int i;
    if (s) return s;
    for (i = 0; i < 48; i++) if (!Slots[i].key) {
        memset(&Slots[i], 0, sizeof Slots[i]);
        Slots[i].key = o;
        Slots[i].idx = -1;
        return &Slots[i];
    }
    return 0;
}

static const char *sel_for(int kind) {
    switch (kind) {
    case CX_START: return "provider:performStartCallAction:";
    case CX_ANSWER: return "provider:performAnswerCallAction:";
    case CX_END: return "provider:performEndCallAction:";
    case CX_HOLD: return "provider:performSetHeldCallAction:";
    case CX_MUTE: return "provider:performSetMutedCallAction:";
    default: return 0;
    }
}

static id action_obj(int kind, int idx) {
    cx_class_t *c = &CXStartCallAction_c;
    id o;
    slot_t *s;
    if (kind == CX_ANSWER) c = &CXAnswerCallAction_c;
    else if (kind == CX_END) c = &CXEndCallAction_c;
    else if (kind == CX_HOLD) c = &CXSetHeldCallAction_c;
    else if (kind == CX_MUTE) c = &CXSetMutedCallAction_c;
    o = cx_alloc_instance(c);
    s = o ? slot_new(o) : 0;
    if (!s) return 0;
    s->kind = kind;
    s->idx = idx;
    if (idx >= 0 && idx < Gcx.nacts) {
        memcpy(s->uuid, Gcx.acts[idx].uuid, sizeof s->uuid);
        memcpy(s->handle, Gcx.acts[idx].handle, sizeof s->handle);
    }
    return o;
}

static id Gq;
static id Gobsq;
static id Gobsdel;
static id Gprivq;

typedef struct { id del, a, b; const char *sel; } cx_job;
static cx_job Jobs[16];
static int Jobi;

static id provider_queue(void) {
    if (Gq) return Gq;
    if (!Gprivq) Gprivq = (id)dispatch_queue_create("com.mos.callkit.provider", 0);
    return Gprivq;
}
static void run_job(void *p) {
    cx_job *j = p;
    if (!j->del || !j->sel) return;
    if (!cx_responds(j->del->isa, sel_registerName(j->sel))) return;
    ((void (*)(id, SEL, id, id))objc_msgSend)(j->del, sel_registerName(j->sel), j->a, j->b);
}
static void post(id q, id del, id a, id b, const char *sel) {
    cx_job *j;
    if (!del || !sel) return;
    j = &Jobs[Jobi++ & 15];
    j->del = del;
    j->a = a;
    j->b = b;
    j->sel = sel;
    wa_cx_deliver(q ? q : provider_queue(), j, run_job);
}
static void tell_delegate(int kind, id action) {
    const char *sel;
    if (!Gdel || !Gcx.delegate) return;
    sel = sel_for(kind);
    post(Gq, Gdel, Gprov, action, sel);
}

static id cx_cfg_init(id self, SEL c, id name) {
    slot_t *s = slot_new(self);
    (void)c;
    if (s) {
        memcpy(s->name, cstr(name), sizeof s->name - 1);
        s->max_groups = 2;
        s->max_per = 5;
        s->mask = 1u << CX_HANDLE_PHONE;
        s->video = 0;
    }
    return self;
}
static void push_cfg(id cfg) {
    slot_t *s = slot_of(cfg);
    cx_cfg c;
    if (!s) return;
    memset(&c, 0, sizeof c);
    c.max_groups = s->max_groups;
    c.max_per = s->max_per;
    c.video = s->video;
    c.handle_mask = s->mask;
    wa_cx_set_config(&Gcx, &c);
}
static id cx_prov_init(id self, SEL c, id cfg) {
    (void)c;
    Gprov = self;
    wa_cx_set_provider(&Gcx, 1, Gdel ? 1 : 0);
    push_cfg(cfg);
    return self;
}
static void cx_set_del(id self, SEL c, id del, id q) {
    (void)c; (void)self;
    Gdel = del;
    Gq = q;
    wa_cx_set_provider(&Gcx, 1, del ? 1 : 0);
    wa_cx_set_delegate_queue(&Gcx, q ? 1 : 0);
}
static void upd_from(slot_t *s, cx_update *u);
static void notify_call(const char *uuid);
static void cx_incoming(id self, SEL c, id uuid, id update, void *done) {
    cx_update u;
    int rc;
    (void)c; (void)self;
    rc = wa_cx_report_incoming(&Gcx, cstr(uuid), 0);
    if (rc == CX_OK) {
        upd_from(slot_of(update), &u);
        wa_cx_apply_update(&Gcx, cstr(uuid), &u);
        notify_call(cstr(uuid));
    }
    call_block(done, rc == CX_OK ? 0 : make_err(CXErrorDomainIncomingCall, rc, rc == CX_ERR_LIMIT ? "a transação passa do limite de chamadas" : "chamada de entrada não registrada"));
}
static id cx_ctl_init(id self, SEL c) { (void)c; Gctl = self; return self; }
static void cx_request(id self, SEL c, id tx, void *done) {
    unsigned long n, i;
    int any = 0, rc = CX_OK;
    (void)self; (void)c;
    if (!Gcx.provider) {
        call_block(done, make_err(CXErrorDomainIncomingCall, CX_ERR_NO_PROVIDER, "não há CXProvider"));
        return;
    }
    n = tx ? MSG0(unsigned long, tx, "count") : 0;
    if (n == 0 && tx) {
        slot_t *s = slot_of(tx);
        if (s && s->kind) {
            int idx = -1;
            rc = wa_cx_submit_full(&Gcx, s->kind, s->uuid[0] ? s->uuid : "local", s->handle, s->handle_type, s->flag, s->flag, s->video, &idx);
            if (rc == CX_OK) { s->idx = idx; tell_delegate(s->kind, tx); any = 1; }
        }
    }
    for (i = 0; i < n; i++) {
        id act = MSG(id, tx, "objectAtIndex:", i);
        slot_t *s = slot_of(act);
        int idx = -1;
        if (!s) continue;
        rc = wa_cx_submit_full(&Gcx, s->kind, s->uuid[0] ? s->uuid : "local", s->handle, s->handle_type, s->flag, s->flag, s->video, &idx);
        if (rc != CX_OK) break;
        s->idx = idx;
        tell_delegate(s->kind, act);
        any = 1;
    }
    if (!any && rc == CX_OK) rc = CX_ERR_BAD;
    call_block(done, rc == CX_OK ? 0 : make_err(CXErrorDomainIncomingCall, rc,
        rc == CX_ERR_NO_PROVIDER ? "não há CXProvider" :
        rc == CX_ERR_LIMIT ? "a transação passa do limite de chamadas" :
        rc == CX_ERR_HANDLE ? "o tipo de CXHandle não está em supportedHandleTypes" :
        rc == CX_ERR_VIDEO ? "a configuração não aceita vídeo" :
        "transação vazia ou inválida"));
}
static id act_init(id self, SEL c, id uuid) {
    slot_t *s = slot_new(self);
    (void)c;
    if (!s) return self;
    memcpy(s->uuid, cstr(uuid), sizeof s->uuid - 1);
    if (self->isa == &CXStartCallAction_c) s->kind = CX_START;
    else if (self->isa == &CXAnswerCallAction_c) s->kind = CX_ANSWER;
    else if (self->isa == &CXEndCallAction_c) s->kind = CX_END;
    else if (self->isa == &CXSetHeldCallAction_c) s->kind = CX_HOLD;
    else if (self->isa == &CXSetMutedCallAction_c) s->kind = CX_MUTE;
    return self;
}
static id act_uuid(id self, SEL c) { slot_t *s = slot_of(self); (void)c; return cx_str(s ? s->uuid : ""); }
static void act_fulfill(id self, SEL c) { slot_t *s = slot_of(self); (void)c; if (s && s->idx >= 0) wa_cx_fulfill(&Gcx, s->idx); }
static void act_fail(id self, SEL c) { slot_t *s = slot_of(self); (void)c; if (s && s->idx >= 0) wa_cx_fail(&Gcx, s->idx); }
static void act_set_held(id self, SEL c, BOOL v) { slot_t *s = slot_of(self); (void)c; if (s) s->flag = v ? 1 : 0; }
static void act_set_muted(id self, SEL c, BOOL v) { slot_t *s = slot_of(self); (void)c; if (s) s->flag = v ? 1 : 0; }
static id TxArr[16];
static id TxOwn[16];
static id tx_init2(id self, SEL c, id act) {
    id arr = MSG0(id, (id)cx_class_named("NSMutableArray"), "array");
    int i;
    (void)c;
    slot_new(self);
    if (arr && act) MSG(void, arr, "addObject:", act);
    for (i = 0; i < 16; i++) if (!TxOwn[i] || TxOwn[i] == self) { TxOwn[i] = self; TxArr[i] = arr; break; }
    return self;
}
static unsigned long tx_count(id self, SEL c) {
    int i;
    (void)c;
    for (i = 0; i < 16; i++) if (TxOwn[i] == self && TxArr[i]) return MSG0(unsigned long, TxArr[i], "count");
    return 0;
}
static id tx_at(id self, SEL c, unsigned long i) {
    int k;
    (void)c;
    for (k = 0; k < 16; k++) if (TxOwn[k] == self && TxArr[k]) return MSG(id, TxArr[k], "objectAtIndex:", i);
    return 0;
}

static id ct_info(id self, SEL c) { (void)c; return self; }
static id ct_nil(id self, SEL c) { (void)self; (void)c; return 0; }
static id ct_empty(id self, SEL c) {
    (void)self; (void)c;
    return MSG0(id, (id)cx_class_named("NSDictionary"), "dictionary");
}

static long cl_status(id self, SEL c) {
    (void)self; (void)c;
    Gauth = wa_cl_auth(cx_tcc_state("kTCCServiceLocation"));
    return Gauth;
}
static void cl_ask(id self, SEL c) {
    (void)c; (void)self;
    cx_tcc_request("kTCCServiceLocation");
    Gauth = wa_cl_auth(cx_tcc_state("kTCCServiceLocation"));
}
static id Gcldel;
static id cl_loc(id self, SEL c) { (void)self; (void)c; return 0; }
static id cl_get_del(id self, SEL c) { (void)self; (void)c; return Gcldel; }
static void cl_fail_to(id del, id mgr) {
    id err;
    int code = wa_cl_error(Gauth);
    if (!del || !cx_responds(del->isa, sel_registerName("locationManager:didFailWithError:"))) return;
    err = make_err("kCLErrorDomain", code, code == CL_ERR_DENIED ? "localização negada" : "localização desconhecida: o mOS não tem GPS");
    ((void (*)(id, SEL, id, id))objc_msgSend)(del, sel_registerName("locationManager:didFailWithError:"), mgr, err);
}
static void cl_set_del(id self, SEL c, id d) { (void)self; (void)c; Gcldel = d; }
static void cl_start(id self, SEL c) {
    (void)c;
    Gauth = wa_cl_auth(cx_tcc_state("kTCCServiceLocation"));
    cl_fail_to(Gcldel, self);
}
static void cl_geocode(id self, SEL c, id a, void *done) {
    (void)self; (void)c; (void)a;
    call_block(done, make_err("kCLErrorDomain", CL_ERR_UNKNOWN, "sem GPS para geocodificar"));
}

cl_coord_t CLLocationCoordinate2DMake(double lat, double lon) {
    cl_coord_t c;
    c.latitude = lat;
    c.longitude = lon;
    return c;
}
int CLLocationCoordinate2DIsValid(cl_coord_t c) { return wa_cl_coord_valid(c.latitude, c.longitude); }

static id bg_shared(id cls, SEL c) {
    static id s;
    (void)c;
    if (!s) s = cx_alloc_instance((cx_class_t *)cls);
    return s;
}
static void bg_reg(id self, SEL c, id ident, id q, void *handler) {
    (void)self; (void)c; (void)q; (void)handler;
    wa_bg_register(&Gbg, cstr(ident));
}
static BOOL wa_bg_submit_m(id self, SEL c, id req, id *err) {
    slot_t *s = slot_of(req);
    int code = BG_ERR_NOT_PERMITTED;
    (void)self; (void)c;
    wa_bg_submit(&Gbg, s ? s->uuid : "", s ? s->kind : 0, &code);
    if (err) *err = make_err(BGTaskSchedulerErrorDomain, code, code == BG_ERR_UNAVAILABLE ? "o mOS não executa tarefa em segundo plano" : "tarefa não registrada");
    return NO;
}
static id bg_req_init(id self, SEL c, id ident) {
    slot_t *s = slot_new(self);
    (void)c;
    if (s) {
        memcpy(s->uuid, cstr(ident), sizeof s->uuid - 1);
        s->kind = (self->isa == &BGProcessingTaskRequest_c) ? BG_PROCESSING : BG_REFRESH;
    }
    return self;
}

static id h_init(id self, SEL c, long type, id value) {
    slot_t *s = slot_new(self);
    (void)c;
    if (s) {
        s->handle_type = (int)type;
        memcpy(s->handle, cstr(value), sizeof s->handle - 1);
    }
    return self;
}
static long h_type(id self, SEL c) { slot_t *s = slot_of(self); (void)c; return s ? s->handle_type : 0; }
static id h_value(id self, SEL c) { slot_t *s = slot_of(self); (void)c; return cx_str(s ? s->handle : ""); }
static BOOL h_eq(id self, SEL c, id other) {
    slot_t *a = slot_of(self), *b = slot_of(other);
    (void)c;
    if (!a || !b || !other || other->isa != &CXHandle_c) return NO;
    return wa_cx_handle_equal(a->handle_type, a->handle, b->handle_type, b->handle) ? YES : NO;
}
static unsigned long h_hash(id self, SEL c) {
    slot_t *s = slot_of(self);
    (void)c;
    return s ? wa_cx_handle_hash(s->handle_type, s->handle) : 0;
}
static void upd_from(slot_t *s, cx_update *u) {
    memset(u, 0, sizeof *u);
    u->has_video = -1;
    u->hold = u->group = u->ungroup = u->dtmf = -1;
    if (!s) return;
    if (s->href) {
        slot_t *h = slot_of(s->href);
        u->has_handle = 1;
        if (h) {
            u->handle_type = h->handle_type;
            memcpy(u->handle, h->handle, sizeof u->handle);
        }
    }
    if (s->name[0]) { u->has_name = 1; memcpy(u->name, s->name, sizeof u->name); }
    if (s->video >= 0) u->has_video = s->video;
    u->hold = s->cap_hold;
    u->group = s->cap_group;
    u->ungroup = s->cap_ungroup;
    u->dtmf = s->cap_dtmf;
}
static id call_obj(const cx_call *c) {
    int i;
    id o;
    slot_t *s;
    if (!c) return 0;
    for (i = 0; i < 48; i++) {
        s = &Slots[i];
        if (s->key && s->key->isa == &CXCall_c && !strcmp(s->uuid, c->uuid)) return s->key;
    }
    o = cx_alloc_instance(&CXCall_c);
    s = o ? slot_new(o) : 0;
    if (!s) return o;
    memcpy(s->uuid, c->uuid, sizeof s->uuid);
    return o;
}
static void notify_call(const char *uuid) {
    const cx_call *c = wa_cx_find_call(&Gcx, uuid);
    id o;
    if (!Gobsdel || !c) return;
    o = call_obj(c);
    post(Gobsq, Gobsdel, Gobs, o, "callObserver:callChanged:");
}
static id upd_init(id self, SEL c) {
    slot_t *s = slot_new(self);
    (void)c;
    if (s) {
        s->video = -1;
        s->cap_hold = s->cap_group = s->cap_ungroup = s->cap_dtmf = -1;
    }
    return self;
}
static void upd_handle(id self, SEL c, id h) { slot_t *s = slot_of(self); (void)c; if (s) s->href = h; }
static id upd_handle_get(id self, SEL c) { slot_t *s = slot_of(self); (void)c; return s ? s->href : 0; }
static void upd_name(id self, SEL c, id n) { slot_t *s = slot_of(self); (void)c; if (s) memcpy(s->name, cstr(n), sizeof s->name - 1); }
static id upd_name_get(id self, SEL c) { slot_t *s = slot_of(self); (void)c; return cx_str(s ? s->name : ""); }
static void upd_video(id self, SEL c, BOOL v) { slot_t *s = slot_of(self); (void)c; if (s) s->video = v ? 1 : 0; }
static BOOL upd_video_get(id self, SEL c) { slot_t *s = slot_of(self); (void)c; return s && s->video == 1; }
static void upd_cap(id self, SEL c, BOOL v) {
    slot_t *s = slot_of(self);
    const char *n = (const char *)c;
    int bit = v ? 1 : 0;
    if (!s || !n) return;
    if (!strcmp(n, "setSupportsHolding:")) s->cap_hold = bit;
    else if (!strcmp(n, "setSupportsGrouping:")) s->cap_group = bit;
    else if (!strcmp(n, "setSupportsUngrouping:")) s->cap_ungroup = bit;
    else if (!strcmp(n, "setSupportsDTMF:")) s->cap_dtmf = bit;
}
static void cx_report_updated(id self, SEL c, id uuid, id update) {
    cx_update u;
    (void)self; (void)c;
    upd_from(slot_of(update), &u);
    if (wa_cx_apply_update(&Gcx, cstr(uuid), &u) == CX_OK) notify_call(cstr(uuid));
}
static id cx_call_uuid(id self, SEL c) { slot_t *s = slot_of(self); (void)c; return cx_str(s ? s->uuid : ""); }
static BOOL cx_call_flag(id self, SEL c) {
    slot_t *s = slot_of(self);
    const cx_call *live = s ? wa_cx_find_call(&Gcx, s->uuid) : 0;
    const char *n = (const char *)c;
    if (!live || !n) return NO;
    if (!strcmp(n, "isOutgoing")) return live->outgoing ? YES : NO;
    if (!strcmp(n, "isOnHold")) return live->on_hold ? YES : NO;
    if (!strcmp(n, "hasConnected")) return live->has_connected ? YES : NO;
    if (!strcmp(n, "hasEnded")) return live->has_ended ? YES : NO;
    return NO;
}
static BOOL upd_cap_get(id self, SEL c) {
    slot_t *s = slot_of(self);
    const char *n = (const char *)c;
    int flag = -1;
    if (!s || !n) return NO;
    if (!strcmp(n, "supportsHolding")) flag = s->cap_hold;
    else if (!strcmp(n, "supportsGrouping")) flag = s->cap_group;
    else if (!strcmp(n, "supportsUngrouping")) flag = s->cap_ungroup;
    else if (!strcmp(n, "supportsDTMF")) flag = s->cap_dtmf;
    return wa_cx_cap_on(flag) ? YES : NO;
}
static id obs_init(id self, SEL c) { (void)c; Gobs = self; return self; }
static void obs_set(id self, SEL c, id del, id q) { (void)self; (void)c; Gobsdel = del; Gobsq = q; wa_cx_set_observer(&Gcx, del ? 1 : 0, q ? 2 : 0); }
static id obs_calls(id self, SEL c) {
    id arr = MSG0(id, (id)cx_class_named("NSMutableArray"), "array");
    int i, n = wa_cx_call_count(&Gcx);
    (void)self; (void)c;
    for (i = 0; i < n; i++) {
        id o = call_obj(wa_cx_call_at(&Gcx, i));
        if (arr && o) MSG(void, arr, "addObject:", o);
    }
    return arr;
}
static void cfg_groups(id self, SEL c, long v) { slot_t *s = slot_of(self); (void)c; if (s) s->max_groups = (int)v; push_cfg(self); }
static long cfg_groups_get(id self, SEL c) { slot_t *s = slot_of(self); (void)c; return s ? s->max_groups : 0; }
static void cfg_per(id self, SEL c, long v) { slot_t *s = slot_of(self); (void)c; if (s) s->max_per = (int)v; push_cfg(self); }
static long cfg_per_get(id self, SEL c) { slot_t *s = slot_of(self); (void)c; return s ? s->max_per : 0; }
static void cfg_video(id self, SEL c, BOOL v) { slot_t *s = slot_of(self); (void)c; if (s) s->video = v ? 1 : 0; push_cfg(self); }
static BOOL cfg_video_get(id self, SEL c) { slot_t *s = slot_of(self); (void)c; return s && s->video ? YES : NO; }
static id cfg_name(id self, SEL c) { slot_t *s = slot_of(self); (void)c; return cx_str(s ? s->name : ""); }
static void cfg_types(id self, SEL c, id set) {
    slot_t *s = slot_of(self);
    id arr;
    unsigned long i, n;
    unsigned mask = 0;
    (void)c;
    arr = set ? MSG0(id, set, "allObjects") : 0;
    n = arr ? MSG0(unsigned long, arr, "count") : 0;
    for (i = 0; i < n; i++) {
        id num = MSG(id, arr, "objectAtIndex:", i);
        long t = num ? MSG0(long, num, "integerValue") : 0;
        if (t > 0 && t < 31) mask |= 1u << t;
    }
    if (s) s->mask = mask;
    push_cfg(self);
}
static void def(cx_class_t *c, cx_class_t *m, const char *name) {
    cx_define_class(c, m, name, cx_class_named("NSObject"), 0);
    cx_mark_stub_class(c);
}

void cx_wa_frameworks_init(void) {
    unsigned long long nanb = 0x7ff8000000000000ULL;
    double nanv;
    memcpy(&nanv, &nanb, sizeof nanv);
    kCLLocationCoordinate2DInvalid.latitude = nanv;
    kCLLocationCoordinate2DInvalid.longitude = nanv;
    wa_cx_box_init(&Gcx);
    wa_bg_init(&Gbg);
    def(&CXProvider_c, &CXProvider_m, "CXProvider");
    def(&CXProviderConfiguration_c, &CXProviderConfiguration_m, "CXProviderConfiguration");
    def(&CXCallController_c, &CXCallController_m, "CXCallController");
    def(&CXCallObserver_c, &CXCallObserver_m, "CXCallObserver");
    def(&CXCallUpdate_c, &CXCallUpdate_m, "CXCallUpdate");
    def(&CXCall_c, &CXCall_m, "CXCall");
    def(&CXHandle_c, &CXHandle_m, "CXHandle");
    def(&CXStartCallAction_c, &CXStartCallAction_m, "CXStartCallAction");
    def(&CXAnswerCallAction_c, &CXAnswerCallAction_m, "CXAnswerCallAction");
    def(&CXEndCallAction_c, &CXEndCallAction_m, "CXEndCallAction");
    def(&CXSetHeldCallAction_c, &CXSetHeldCallAction_m, "CXSetHeldCallAction");
    def(&CXSetMutedCallAction_c, &CXSetMutedCallAction_m, "CXSetMutedCallAction");
    def(&CXTransaction_c, &CXTransaction_m, "CXTransaction");
    def(&CTTelephonyNetworkInfo_c, &CTTelephonyNetworkInfo_m, "CTTelephonyNetworkInfo");
    def(&CLGeocoder_c, &CLGeocoder_m, "CLGeocoder");
    def(&CLPlacemark_c, &CLPlacemark_m, "CLPlacemark");
    def(&BGTaskScheduler_c, &BGTaskScheduler_m, "BGTaskScheduler");
    def(&BGAppRefreshTaskRequest_c, &BGAppRefreshTaskRequest_m, "BGAppRefreshTaskRequest");
    def(&BGProcessingTaskRequest_c, &BGProcessingTaskRequest_m, "BGProcessingTaskRequest");
    cx_add_method(&CXProviderConfiguration_c, "initWithLocalizedName:", (void *)cx_cfg_init);
    cx_add_method(&CXProvider_c, "initWithConfiguration:", (void *)cx_prov_init);
    cx_add_method(&CXProvider_c, "setDelegate:queue:", (void *)cx_set_del);
    cx_add_method(&CXProvider_c, "reportNewIncomingCallWithUUID:update:completion:", (void *)cx_incoming);
    cx_add_method(&CXProvider_c, "reportCallWithUUID:updated:", (void *)cx_report_updated);
    cx_add_method(&CXProviderConfiguration_c, "localizedName", (void *)cfg_name);
    cx_add_method(&CXProviderConfiguration_c, "setMaximumCallGroups:", (void *)cfg_groups);
    cx_add_method(&CXProviderConfiguration_c, "maximumCallGroups", (void *)cfg_groups_get);
    cx_add_method(&CXProviderConfiguration_c, "setMaximumCallsPerCallGroup:", (void *)cfg_per);
    cx_add_method(&CXProviderConfiguration_c, "maximumCallsPerCallGroup", (void *)cfg_per_get);
    cx_add_method(&CXProviderConfiguration_c, "setSupportsVideo:", (void *)cfg_video);
    cx_add_method(&CXProviderConfiguration_c, "supportsVideo", (void *)cfg_video_get);
    cx_add_method(&CXProviderConfiguration_c, "setSupportedHandleTypes:", (void *)cfg_types);
    cx_add_method(&CXHandle_c, "initWithType:value:", (void *)h_init);
    cx_add_method(&CXHandle_c, "type", (void *)h_type);
    cx_add_method(&CXHandle_c, "value", (void *)h_value);
    cx_add_method(&CXHandle_c, "isEqual:", (void *)h_eq);
    cx_add_method(&CXHandle_c, "hash", (void *)h_hash);
    cx_add_method(&CXCallUpdate_c, "init", (void *)upd_init);
    cx_add_method(&CXCallUpdate_c, "setRemoteHandle:", (void *)upd_handle);
    cx_add_method(&CXCallUpdate_c, "remoteHandle", (void *)upd_handle_get);
    cx_add_method(&CXCallUpdate_c, "setLocalizedCallerName:", (void *)upd_name);
    cx_add_method(&CXCallUpdate_c, "localizedCallerName", (void *)upd_name_get);
    cx_add_method(&CXCallUpdate_c, "setHasVideo:", (void *)upd_video);
    cx_add_method(&CXCallUpdate_c, "hasVideo", (void *)upd_video_get);
    cx_add_method(&CXCallUpdate_c, "setSupportsHolding:", (void *)upd_cap);
    cx_add_method(&CXCallUpdate_c, "setSupportsGrouping:", (void *)upd_cap);
    cx_add_method(&CXCallUpdate_c, "setSupportsUngrouping:", (void *)upd_cap);
    cx_add_method(&CXCallUpdate_c, "setSupportsDTMF:", (void *)upd_cap);
    cx_add_method(&CXCallUpdate_c, "supportsHolding", (void *)upd_cap_get);
    cx_add_method(&CXCallUpdate_c, "supportsGrouping", (void *)upd_cap_get);
    cx_add_method(&CXCallUpdate_c, "supportsUngrouping", (void *)upd_cap_get);
    cx_add_method(&CXCallUpdate_c, "supportsDTMF", (void *)upd_cap_get);
    cx_add_method(&CXCall_c, "UUID", (void *)cx_call_uuid);
    cx_add_method(&CXCall_c, "isOutgoing", (void *)cx_call_flag);
    cx_add_method(&CXCall_c, "isOnHold", (void *)cx_call_flag);
    cx_add_method(&CXCall_c, "hasConnected", (void *)cx_call_flag);
    cx_add_method(&CXCall_c, "hasEnded", (void *)cx_call_flag);
    cx_add_method(&CXCallObserver_c, "init", (void *)obs_init);
    cx_add_method(&CXCallObserver_c, "setDelegate:queue:", (void *)obs_set);
    cx_add_method(&CXCallObserver_c, "calls", (void *)obs_calls);
    cx_add_method(&CXCallController_c, "init", (void *)cx_ctl_init);
    cx_add_method(&CXCallController_c, "requestTransaction:completion:", (void *)cx_request);
    cx_add_method(&CXStartCallAction_c, "initWithCallUUID:", (void *)act_init);
    cx_add_method(&CXAnswerCallAction_c, "initWithCallUUID:", (void *)act_init);
    cx_add_method(&CXEndCallAction_c, "initWithCallUUID:", (void *)act_init);
    cx_add_method(&CXSetHeldCallAction_c, "initWithCallUUID:", (void *)act_init);
    cx_add_method(&CXSetMutedCallAction_c, "initWithCallUUID:", (void *)act_init);
    cx_add_method(&CXStartCallAction_c, "callUUID", (void *)act_uuid);
    cx_add_method(&CXStartCallAction_c, "fulfill", (void *)act_fulfill);
    cx_add_method(&CXStartCallAction_c, "fail", (void *)act_fail);
    cx_add_method(&CXAnswerCallAction_c, "fulfill", (void *)act_fulfill);
    cx_add_method(&CXAnswerCallAction_c, "fail", (void *)act_fail);
    cx_add_method(&CXEndCallAction_c, "fulfill", (void *)act_fulfill);
    cx_add_method(&CXEndCallAction_c, "fail", (void *)act_fail);
    cx_add_method(&CXSetHeldCallAction_c, "setOnHold:", (void *)act_set_held);
    cx_add_method(&CXSetHeldCallAction_c, "fulfill", (void *)act_fulfill);
    cx_add_method(&CXSetMutedCallAction_c, "setMuted:", (void *)act_set_muted);
    cx_add_method(&CXSetMutedCallAction_c, "fulfill", (void *)act_fulfill);
    cx_add_method(&CXTransaction_c, "initWithAction:", (void *)tx_init2);
    cx_add_method(&CXTransaction_c, "count", (void *)tx_count);
    cx_add_method(&CXTransaction_c, "objectAtIndex:", (void *)tx_at);
    cx_add_method(&CTTelephonyNetworkInfo_c, "init", (void *)ct_info);
    cx_add_method(&CTTelephonyNetworkInfo_c, "subscriberCellularProvider", (void *)ct_nil);
    cx_add_method(&CTTelephonyNetworkInfo_c, "dataServiceIdentifier", (void *)ct_nil);
    cx_add_method(&CTTelephonyNetworkInfo_c, "currentRadioAccessTechnology", (void *)ct_nil);
    cx_add_method(&CTTelephonyNetworkInfo_c, "serviceSubscriberCellularProviders", (void *)ct_empty);
    cx_add_method(&CTTelephonyNetworkInfo_c, "serviceCurrentRadioAccessTechnology", (void *)ct_empty);
    cx_add_cmethod(&CLLocationManager_c, "authorizationStatus", (void *)cl_status);
    cx_add_method(&CLLocationManager_c, "authorizationStatus", (void *)cl_status);
    cx_add_method(&CLLocationManager_c, "requestWhenInUseAuthorization", (void *)cl_ask);
    cx_add_method(&CLLocationManager_c, "setDelegate:", (void *)cl_set_del);
    cx_add_method(&CLLocationManager_c, "delegate", (void *)cl_get_del);
    cx_add_method(&CLLocationManager_c, "location", (void *)cl_loc);
    cx_add_method(&CLLocationManager_c, "requestLocation", (void *)cl_start);
    cx_add_method(&CLLocationManager_c, "startUpdatingLocation", (void *)cl_start);
    cx_add_method(&CLGeocoder_c, "reverseGeocodeLocation:completionHandler:", (void *)cl_geocode);
    cx_add_method(&CLGeocoder_c, "geocodeAddressString:completionHandler:", (void *)cl_geocode);
    cx_add_cmethod(&BGTaskScheduler_c, "sharedScheduler", (void *)bg_shared);
    cx_add_method(&BGTaskScheduler_c, "registerForTaskWithIdentifier:usingQueue:launchHandler:", (void *)bg_reg);
    cx_add_method(&BGTaskScheduler_c, "submitTaskRequest:error:", (void *)wa_bg_submit_m);
    cx_add_method(&BGAppRefreshTaskRequest_c, "initWithIdentifier:", (void *)bg_req_init);
    cx_add_method(&BGProcessingTaskRequest_c, "initWithIdentifier:", (void *)bg_req_init);
    (void)Gctl;
    (void)Gobs;
    (void)action_obj;
}
