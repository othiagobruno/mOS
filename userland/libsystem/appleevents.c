/*
 * mOS — libSystem: Apple Event Manager (CoreServices/AE), NSAppleEventDescriptor e NSAppleEventManager.
 *
 * Descritores: ae_core.inc (escalares, listas, registros, eventos, coerções). A API C (AECreateDesc, AEPutParamDesc,
 * AECreateAppleEvent, AESend, AEInstallEventHandler...) e o NSAppleEventDescriptor usam a mesma árvore.
 * O AEDesc tem o layout da Apple (#pragma pack(2): tipo de 4 bytes e o ponteiro logo depois, 12 bytes).
 *
 * Entre processos: cada app (o processo principal, a partir de -[NSApplication finishLaunching]) atende o serviço
 * Mach com.mos.ae.<CFBundleIdentifier> e com.mos.ae.pid.<pid>. O pedido é o protocolo de svcproto.h com o evento
 * serializado (fio próprio do mOS: os dois lados são este libSystem); a resposta volta pela porta send-once. O
 * tratador roda no fio principal do app, como no macOS. Sem tratador, valem os padrões do AppKit: abrir app
 * ('aevt'/'oapp', 'rapp', 'misc'/'actv'), sair ('aevt'/'quit'), abrir documentos ('aevt'/'odoc') e URL ('GURL').
 * Alvo fechado: AESend inicia o app pelo Launch Services e espera o serviço (até 60 s), menos para 'quit'.
 *
 * Automação (TCC): mandar evento a outro app pede kTCCServiceAppleEvents+<bundle do alvo>; sem escolha, o diálogo
 * "“A” gostaria de controlar “B”" abre e o AESend espera a resposta (com kAENeverInteract: -1744 na hora). Negado:
 * errAEEventNotPermitted (-1743). AEDeterminePermissionToAutomateTarget faz a mesma conta.
 * Teste de host dos descritores: tools/astest.c.
 */
#include "foundation.h"
#include "libc.h"

typedef unsigned short u16_;
typedef long long i64_;
typedef short i16;
#include "ae_core.inc"

int cx_snprintf(char *buf, unsigned long n, const char *fmt, ...);
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void *objc_autoreleasePoolPush(void);
void objc_autoreleasePoolPop(void *p);
void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy);
id objc_getAssociatedObject(id obj, const void *key);
void *dispatch_get_main_queue(void);
void dispatch_sync_f(void *q, void *ctx, void (*fn)(void *));
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
int pthread_create(unsigned long *t, void *attr, void *(*fn)(void *), void *arg);
int pthread_detach(unsigned long t);
int pthread_main_np(void);
int getpid(void);
int usleep(unsigned us);
extern u32 bootstrap_port;
int bootstrap_check_in(u32 bp, const char *name, u32 *port);
int bootstrap_look_up(u32 bp, const char *name, u32 *port);
int mach_msg(void *msg, int option, u32 send_size, u32 rcv_size, u32 rcv_name, u32 timeout, u32 notify);
int mach_port_allocate(u32 task, u32 right, u32 *name);
int mach_port_mod_refs(u32 task, u32 name, u32 right, int delta);
int mach_port_deallocate(u32 task, u32 name);
int cx_ls_app_lookup(const char *key, int by_id, char *bundle, size_t bc, char *path, size_t pc, char *name, size_t nc);
int cx_ls_launch_app(const char *bundle);
int cx_tcc_state(const char *service);
int cx_tcc_request(const char *service);
double CFAbsoluteTimeGetCurrent(void);
id CFDateCreate(void *alloc, double at);
double CFDateGetAbsoluteTime(id d);

#include "ae_capi.inc"
#include "ae_build.inc"

#define AE_SVC_PREFIX "com.mos.ae."
#define AE_OP_EVENT 1u

/* ================================================================ tratadores */
typedef int (*AEEventHandlerProcPtr)(const AEDesc *ev, AEDesc *reply, void *refcon);
typedef struct { u32 cls, id; AEEventHandlerProcPtr proc; void *refcon; id target; SEL sel; } handler_t;
static handler_t g_h[128];
static int g_nh;
static volatile int g_hlock;
static void hl(void) { while (__sync_lock_test_and_set(&g_hlock, 1)) ; }
static void hu(void) { __sync_lock_release(&g_hlock); }
static int set_handler(u32 cls, u32 ev, AEEventHandlerProcPtr proc, void *refcon, id target, SEL sel) {
    hl();
    for (int i = 0; i < g_nh; i++)
        if (g_h[i].cls == cls && g_h[i].id == ev) { g_h[i] = (handler_t){cls, ev, proc, refcon, target, sel}; hu(); return noErr; }
    if (g_nh >= 128) { hu(); return memFullErr; }
    g_h[g_nh++] = (handler_t){cls, ev, proc, refcon, target, sel};
    hu();
    return noErr;
}
static void remove_handler(u32 cls, u32 ev) {
    hl();
    for (int i = 0; i < g_nh; i++) if (g_h[i].cls == cls && g_h[i].id == ev) { g_h[i] = g_h[--g_nh]; break; }
    hu();
}
static int find_handler(u32 cls, u32 ev, handler_t *out) {
    int best = -1, score = -1;
    hl();
    for (int i = 0; i < g_nh; i++) {
        int s = (g_h[i].cls == cls ? 2 : g_h[i].cls == AE_WILD ? 0 : -9) + (g_h[i].id == ev ? 1 : g_h[i].id == AE_WILD ? 0 : -9);
        if (s >= 0 && s > score) { score = s; best = i; }
    }
    if (best >= 0) *out = g_h[best];
    hu();
    return best >= 0;
}
void *NewAEEventHandlerUPP(void *proc) { return proc; }
void DisposeAEEventHandlerUPP(void *upp) { (void)upp; }
int AEInstallEventHandler(u32 cls, u32 ev, void *handler, void *refcon, u8 sys) {
    (void)sys;
    return handler ? set_handler(cls, ev, (AEEventHandlerProcPtr)handler, refcon, 0, 0) : paramErr;
}
int AERemoveEventHandler(u32 cls, u32 ev, void *handler, u8 sys) { (void)handler; (void)sys; remove_handler(cls, ev); return noErr; }
int AEGetEventHandler(u32 cls, u32 ev, void **handler, void **refcon, u8 sys) {
    (void)sys;
    handler_t h;
    if (!find_handler(cls, ev, &h) || !h.proc) return errAEEventNotHandled;
    if (handler) *handler = (void *)h.proc;
    if (refcon) *refcon = h.refcon;
    return noErr;
}

/* ================================================================ NSAppleEventDescriptor */
static char k_node;
static cx_class_t Box_c, Box_m;
typedef struct { cx_class_t *isa; ae_node *n; } box_t;
static void box_dealloc(id self, SEL c) {
    (void)c;
    ae_free(((box_t *)self)->n);
    ((box_t *)self)->n = 0;
    void (*sup)(id, SEL) = (void (*)(id, SEL))cx_lookup(cx_class_named("NSObject"), sel_registerName("dealloc"));
    if (sup) sup(self, sel_registerName("dealloc"));
}
static cx_class_t *aed_class(void) { return cx_class_named("NSAppleEventDescriptor"); }
static ae_node *aed_node(id d) {
    id b = d ? objc_getAssociatedObject(d, &k_node) : 0;
    return b ? ((box_t *)b)->n : 0;
}
static void aed_attach(id d, ae_node *n) {
    id b = cx_alloc_instance(&Box_c);
    ((box_t *)b)->n = n;
    objc_setAssociatedObject(d, &k_node, b, 1);
    objc_release(b);
}
/* +1: um NSAppleEventDescriptor dono de n */
static id aed_new(ae_node *n) {
    if (!n) return 0;
    id d = MSG0(id, MSG0(id, (id)aed_class(), "alloc"), "init");
    if (!d) { ae_free(n); return 0; }
    aed_attach(d, n);
    return d;
}
static id aed_auto(ae_node *n) { id d = aed_new(n); return d ? objc_autorelease(d) : 0; }
id cx_aed_from_node_copy(const void *n) { return aed_auto(ae_copy((const ae_node *)n)); }

/* fábricas */
static id c_null(id cls, SEL c) { (void)cls; (void)c; return aed_auto(ae_scalar(AE_NULL, 0, 0)); }
static id c_with_string(id cls, SEL c, id s) { (void)cls; (void)c; return s && cx_is_str(s) ? aed_auto(ae_utxt_of(cx_cstr(s))) : 0; }
static id c_with_bool(id cls, SEL c, BOOL b) { (void)cls; (void)c; return aed_auto(ae_bool(b)); }
static id c_with_i32(id cls, SEL c, i32 v) { (void)cls; (void)c; return aed_auto(ae_i32(v)); }
static id c_with_double(id cls, SEL c, double v) { (void)cls; (void)c; return aed_auto(ae_f64(v)); }
static id c_with_enum(id cls, SEL c, u32 v) { (void)cls; (void)c; return aed_auto(ae_code(AE_ENUM, v)); }
static id c_with_type(id cls, SEL c, u32 v) { (void)cls; (void)c; return aed_auto(ae_code(AE_TYPE, v)); }
static id c_with_data(id cls, SEL c, u32 type, id data) {
    (void)cls; (void)c;
    const void *b = data ? MSG0(const void *, data, "bytes") : 0;
    u64 n = data ? MSG0(u64, data, "length") : 0;
    return aed_auto(type == AE_LIST ? ae_list() : type == AE_RECO ? ae_record() : ae_scalar(type, b, (u32)n));
}
static id c_with_bytes(id cls, SEL c, u32 type, const void *b, u64 n) { (void)cls; (void)c; return aed_auto(ae_scalar(type, b, (u32)n)); }
static id c_list(id cls, SEL c) { (void)cls; (void)c; return aed_auto(ae_list()); }
static id c_record(id cls, SEL c) { (void)cls; (void)c; return aed_auto(ae_record()); }
static id c_bundle(id cls, SEL c, id bid) { (void)cls; (void)c; const char *s = bid && cx_is_str(bid) ? cx_cstr(bid) : ""; return aed_auto(ae_scalar(AE_BUND, s, (u32)strlen(s))); }
static id c_pid(id cls, SEL c, int pid) { (void)cls; (void)c; return aed_auto(ae_scalar(AE_KPID, &pid, 4)); }
static id c_current(id cls, SEL c) { (void)cls; (void)c; int pid = getpid(); return aed_auto(ae_scalar(AE_KPID, &pid, 4)); }
static id c_app_url(id cls, SEL c, id url) {
    (void)cls; (void)c;
    id p = url ? MSG0(id, url, "path") : 0;
    char bid[200];
    if (p && cx_is_str(p) && !cx_ls_app_lookup(cx_cstr(p), 0, bid, sizeof bid, 0, 0, 0, 0)) return aed_auto(ae_scalar(AE_BUND, bid, (u32)strlen(bid)));
    return 0;
}
static id c_file_url(id cls, SEL c, id url) {
    (void)cls; (void)c;
    id s = url ? MSG0(id, url, "absoluteString") : 0;
    const char *t = s && cx_is_str(s) ? cx_cstr(s) : 0;
    return t ? aed_auto(ae_scalar(AE_FURL, t, (u32)strlen(t))) : 0;
}
/* datas: 'ldt ' = segundos desde 1904-01-01 (int64) */
#define LDT_FROM_ABS 3061152000.0
static id c_date(id cls, SEL c, id date) {
    (void)cls; (void)c;
    if (!date) return 0;
    i64 v = (i64)(CFDateGetAbsoluteTime(date) + LDT_FROM_ABS);
    return aed_auto(ae_scalar(AE_LDT, &v, 8));
}
static id c_event(id cls, SEL c, u32 ecls, u32 eid, id target, i16 rid, i32 tid) {
    (void)cls; (void)c;
    ae_node *t = aed_node(target);
    i32 r = rid == -1 ? __sync_fetch_and_add(&g_next_rid, 1) : rid;
    return aed_auto(ae_event(ecls, eid, t ? ae_copy(t) : 0, r, tid));
}
/* initWithAEDescNoCopy: / descriptorWithAEDesc...: (o desc vira do objeto) */
static id i_init_desc(id self, SEL c, const AEDesc *d) {
    (void)c;
    ae_node *n = desc_node(d);
    aed_attach(self, n ? ae_copy(n) : ae_scalar(AE_NULL, 0, 0));
    return self;
}
static id i_init_desc_nocopy(id self, SEL c, const AEDesc *d) {
    (void)c;
    ae_node *n = desc_node(d);
    aed_attach(self, n ? n : ae_scalar(AE_NULL, 0, 0));
    if (d) ((AEDesc *)d)->dataHandle = 0;
    return self;
}
/* acessores */
static u32 i_type(id self, SEL c) { (void)c; ae_node *n = aed_node(self); return n ? n->type : AE_NULL; }
static id i_data(id self, SEL c) {
    (void)c;
    ae_node *n = aed_node(self);
    if (!n) return 0;
    if (n->kind != AEK_SCALAR) {                      /* o fio do mOS, para quem quer bytes de uma lista */
        u8 *b; usize len;
        if (ae_serialize(n, &b, &len)) return 0;
        id d = MSG(id, (id)cx_class_named("NSData"), "dataWithBytes:length:", (const void *)b, (u64)len);
        free(b);
        return d;
    }
    return MSG(id, (id)cx_class_named("NSData"), "dataWithBytes:length:", (const void *)n->data, (u64)n->n);
}
static id i_string(id self, SEL c) {
    (void)c;
    ae_node *t = ae_coerce(aed_node(self), AE_UTF8);
    if (!t) return 0;
    char *s = ae_utf8_of(t);
    ae_free(t);
    id r = s ? cx_str(s) : 0;
    free(s);
    return r;
}
static i32 i_i32(id self, SEL c) { (void)c; ae_node *t = ae_coerce(aed_node(self), AE_LONG); i32 v = 0; if (t) memcpy(&v, t->data, 4); ae_free(t); return v; }
static double i_double(id self, SEL c) { (void)c; ae_node *t = ae_coerce(aed_node(self), AE_DOUB); double v = 0; if (t) memcpy(&v, t->data, 8); ae_free(t); return v; }
static BOOL i_bool(id self, SEL c) { (void)c; ae_node *t = ae_coerce(aed_node(self), AE_BOOL); BOOL v = t && t->data[0]; ae_free(t); return v; }
static u32 i_code(id self, SEL c) { (void)c; ae_node *n = aed_node(self); u32 v = 0; if (n && n->kind == AEK_SCALAR && n->n == 4) memcpy(&v, n->data, 4); return v; }
static id i_file_url(id self, SEL c) {
    (void)c;
    ae_node *n = aed_node(self);
    if (!n || n->kind != AEK_SCALAR) return 0;
    char *s = malloc(n->n + 1);
    if (!s) return 0;
    memcpy(s, n->data, n->n);
    s[n->n] = 0;
    id u = 0;
    if (n->type == AE_FURL) u = MSG(id, (id)cx_class_named("NSURL"), "URLWithString:", cx_str(s));
    else if (ae_is_text(n->type) && s[0] == '/') u = MSG(id, (id)cx_class_named("NSURL"), "fileURLWithPath:", cx_str(s));
    free(s);
    return u;
}
static id i_date(id self, SEL c) {
    (void)c;
    ae_node *n = aed_node(self);
    if (!n || n->type != AE_LDT || n->n != 8) return 0;
    i64 v;
    memcpy(&v, n->data, 8);
    return objc_autorelease(CFDateCreate(0, (double)v - LDT_FROM_ABS));
}
static i64 i_count(id self, SEL c) { (void)c; return (i64)ae_count(aed_node(self)); }
static id i_at(id self, SEL c, i64 i) {
    (void)c;
    ae_node *it = ae_nth(aed_node(self), (u32)i);
    return it ? aed_auto(ae_copy(it)) : 0;
}
static void i_insert(id self, SEL c, id d, i64 i) {
    (void)c;
    ae_node *n = aed_node(self), *it = aed_node(d);
    if (!n || !it || n->kind != AEK_LIST) return;
    if (i <= 0 || i > (i64)n->count) ae_list_put(n, 0, ae_copy(it));
    else ae_list_insert(n, (u32)i, ae_copy(it));
}
static void i_remove_at(id self, SEL c, i64 i) { (void)c; ae_delete_nth(aed_node(self), (u32)i); }
static void i_set_kw(id self, SEL c, id d, u32 kw) {
    (void)c;
    ae_node *n = aed_node(self), *it = aed_node(d);
    if (n && it) ae_put_key(n, kw, ae_copy(it));
}
static id i_for_kw(id self, SEL c, u32 kw) { (void)c; ae_node *it = ae_get_key(aed_node(self), kw); return it ? aed_auto(ae_copy(it)) : 0; }
static void i_remove_kw(id self, SEL c, u32 kw) { (void)c; ae_del_key(aed_node(self), kw); }
static u32 i_kw_at(id self, SEL c, i64 i) { (void)c; ae_node *n = aed_node(self); return n && n->keys && i >= 1 && i <= (i64)n->count ? n->keys[i - 1] : 0; }
static id i_attr(id self, SEL c, u32 kw) {
    (void)c;
    int owned;
    ae_node *a = attr_of(aed_node(self), kw, &owned);
    if (!a) return 0;
    return aed_auto(owned ? a : ae_copy(a));
}
static void i_set_attr(id self, SEL c, id d, u32 kw) {
    (void)c;
    ae_node *e = aed_node(self), *it = aed_node(d);
    if (!e || !it || e->kind != AEK_EVENT) return;
    if (kw == K_ADDR) { ae_free(e->target); e->target = ae_copy(it); }
    else ae_put_attr(e, kw, ae_copy(it));
}
static u32 i_ev_class(id self, SEL c) { (void)c; ae_node *n = aed_node(self); return n && n->kind == AEK_EVENT ? n->ev_class : 0; }
static u32 i_ev_id(id self, SEL c) { (void)c; ae_node *n = aed_node(self); return n && n->kind == AEK_EVENT ? n->ev_id : 0; }
static i16 i_rid(id self, SEL c) { (void)c; ae_node *n = aed_node(self); return n && n->kind == AEK_EVENT ? (i16)n->return_id : 0; }
static i32 i_tid(id self, SEL c) { (void)c; ae_node *n = aed_node(self); return n && n->kind == AEK_EVENT ? n->transaction_id : 0; }
static BOOL i_is_record(id self, SEL c) { (void)c; ae_node *n = aed_node(self); return n && n->kind == AEK_RECORD; }
static id i_coerce(id self, SEL c, u32 to) { (void)c; ae_node *t = ae_coerce(aed_node(self), to); return t ? aed_auto(t) : 0; }
static const AEDesc *i_aedesc(id self, SEL c) {
    (void)c;
    /* um AEDesc que vive com o objeto (o nó continua do objeto) */
    static char k_desc;
    id holder = objc_getAssociatedObject(self, &k_desc);
    if (!holder) {
        holder = MSG(id, (id)cx_class_named("NSMutableData"), "dataWithLength:", (u64)sizeof(AEDesc));
        objc_setAssociatedObject(self, &k_desc, holder, 1);
    }
    AEDesc *d = MSG0(AEDesc *, holder, "mutableBytes");
    ae_node *n = aed_node(self);
    d->descriptorType = n ? n->type : AE_NULL;
    d->dataHandle = n;
    return d;
}
static id i_copy(id self, SEL c) { (void)c; ae_node *n = aed_node(self); return aed_new(n ? ae_copy(n) : ae_scalar(AE_NULL, 0, 0)); }
static id i_description(id self, SEL c) {
    (void)c;
    ae_node *n = aed_node(self);
    char b[160];
    u32 t = n ? n->type : AE_NULL;
    if (n && n->kind == AEK_EVENT)
        cx_snprintf(b, sizeof b, "<NSAppleEventDescriptor: '%c%c%c%c'\\'%c%c%c%c'{%u parâmetros}>", n->ev_class >> 24, n->ev_class >> 16 & 255,
                    n->ev_class >> 8 & 255, n->ev_class & 255, n->ev_id >> 24, n->ev_id >> 16 & 255, n->ev_id >> 8 & 255, n->ev_id & 255, n->count);
    else cx_snprintf(b, sizeof b, "<NSAppleEventDescriptor: '%c%c%c%c' (%u)>", t >> 24, t >> 16 & 255, t >> 8 & 255, t & 255, n ? n->n : 0);
    return cx_str(b);
}

/* ================================================================ NSAppleEventManager */
/* o evento atual: os tratadores rodam sempre no fio principal (dispatch_event), sem TLS (__thread não vale no mOS) */
static ae_node *t_cur_event, *t_cur_reply;
static id aem_shared(id cls, SEL c) {
    (void)c;
    static id g;
    if (!g) g = objc_retain(MSG0(id, MSG0(id, cls, "alloc"), "init"));
    return g;
}
static void aem_set(id self, SEL c, id h, SEL s, u32 cls, u32 ev) { (void)self; (void)c; set_handler(cls, ev, 0, 0, h, s); }
static void aem_remove(id self, SEL c, u32 cls, u32 ev) { (void)self; (void)c; remove_handler(cls, ev); }
static id aem_current(id self, SEL c) { (void)self; (void)c; return t_cur_event ? aed_auto(ae_copy(t_cur_event)) : 0; }
static id aem_current_reply(id self, SEL c) { (void)self; (void)c; return t_cur_reply ? aed_auto(ae_copy(t_cur_reply)) : 0; }
/* suspender: o mOS responde quando o tratador termina; o id devolvido é o próprio evento atual */
static void *aem_suspend(id self, SEL c) { (void)self; (void)c; return t_cur_event; }
static void aem_resume(id self, SEL c, void *sid) { (void)self; (void)c; (void)sid; }
static id aem_reply_for(id self, SEL c, void *sid) { (void)self; (void)c; (void)sid; return aem_current_reply(0, 0); }
static id aem_event_for(id self, SEL c, void *sid) { (void)self; (void)c; (void)sid; return aem_current(0, 0); }
static int aem_dispatch_raw(id self, SEL c, const AEDesc *ev, AEDesc *reply, void *refcon);

/* ================================================================ despacho local */
typedef struct { ae_node *ev, *reply; int rc; } disp_t;
static void activate_self(void) {
    id app = cx_class_named("NSApplication") ? MSG0(id, (id)cx_class_named("NSApplication"), "sharedApplication") : 0;
    if (app) MSG(void, app, "activateIgnoringOtherApps:", (BOOL)YES);
}
int cx_openurl_deliver_default(const char *url);   /* openurl.c: documento ou URL sem tratador */
static int default_handler(ae_node *ev, ae_node *reply) {
    (void)reply;
    u32 cls = ev->ev_class, id_ = ev->ev_id;
    if ((cls == AE_FCC('a', 'e', 'v', 't') && (id_ == AE_FCC('o', 'a', 'p', 'p') || id_ == AE_FCC('r', 'a', 'p', 'p'))) ||
        (cls == AE_FCC('m', 'i', 's', 'c') && id_ == AE_FCC('a', 'c', 't', 'v'))) {
        activate_self();
        return noErr;
    }
    if (cls == AE_FCC('a', 'e', 'v', 't') && id_ == AE_FCC('q', 'u', 'i', 't')) {
        id app = cx_class_named("NSApplication") ? MSG0(id, (id)cx_class_named("NSApplication"), "sharedApplication") : 0;
        if (!app) return errAEEventNotHandled;
        MSG(void, app, "terminate:", (id)0);
        return noErr;
    }
    if ((cls == AE_FCC('a', 'e', 'v', 't') && id_ == AE_FCC('o', 'd', 'o', 'c')) || (cls == AE_FCC('G', 'U', 'R', 'L') && id_ == AE_FCC('G', 'U', 'R', 'L'))) {
        ae_node *d = ae_get_key(ev, AE_DIRECT);
        if (!d) return errAEDescNotFound;
        int n = d->kind == AEK_LIST ? (int)d->count : 1, done = 0;
        for (int i = 0; i < n; i++) {
            ae_node *it = d->kind == AEK_LIST ? d->items[i] : d;
            ae_node *t = it->type == AE_FURL ? ae_copy(it) : ae_coerce(it, AE_UTF8);
            if (!t) continue;
            char *s = malloc(t->n + 8);
            if (s) {
                memcpy(s, t->data, t->n);
                s[t->n] = 0;
                if (s[0] == '/') {                           /* caminho POSIX: vira file:// */
                    char *u = malloc(t->n + 8);
                    if (u) { cx_snprintf(u, t->n + 8, "file://%s", s); free(s); s = u; }
                }
                if (cx_openurl_deliver_default(s) == 0) done++;
                free(s);
            }
            ae_free(t);
        }
        if (cls == AE_FCC('a', 'e', 'v', 't')) activate_self();
        return done ? noErr : errAEEventNotHandled;
    }
    return errAEEventNotHandled;
}
static void run_handler(void *ctx) {
    disp_t *d = ctx;
    handler_t h;
    ae_node *pe = t_cur_event, *pr = t_cur_reply;
    t_cur_event = d->ev;
    t_cur_reply = d->reply;
    void *pool = objc_autoreleasePoolPush();
    if (!find_handler(d->ev->ev_class, d->ev->ev_id, &h)) d->rc = default_handler(d->ev, d->reply);
    else if (h.proc) {
        AEDesc e = {d->ev->type, d->ev}, r = {d->reply ? d->reply->type : AE_NULL, d->reply};
        d->rc = h.proc(&e, &r, h.refcon);
        if (r.dataHandle != d->reply) { ae_free(d->reply); d->reply = r.dataHandle; }   /* o tratador trocou a resposta */
    } else {
        id ev = aed_new(ae_copy(d->ev)), rp = d->reply ? aed_new(ae_copy(d->reply)) : 0;
        ((void (*)(id, SEL, id, id))objc_msgSend)(h.target, h.sel, ev, rp);
        if (rp) { ae_free(d->reply); d->reply = ae_copy(aed_node(rp)); objc_release(rp); }   /* o que o tratador pôs na resposta */
        objc_release(ev);
        d->rc = noErr;
    }
    objc_autoreleasePoolPop(pool);
    t_cur_event = pe;
    t_cur_reply = pr;
}
/* roda no fio principal; devolve o código (e a resposta preenchida) */
static int dispatch_event(ae_node *ev, ae_node **reply) {
    disp_t d = {ev, *reply, 0};
    if (pthread_main_np()) run_handler(&d);
    else dispatch_sync_f(dispatch_get_main_queue(), &d, run_handler);
    *reply = d.reply;
    if (d.rc && d.reply && !ae_get_key(d.reply, K_ERRN)) ae_put_key(d.reply, K_ERRN, ae_i32(d.rc));
    return d.rc;
}
/* para openurl.c: tratador registrado do app para 'odoc' (urls file://) ou 'GURL'; 1 = tratou */
int cx_ae_deliver(int is_doc, const char *const *urls, int n) {
    u32 cls = is_doc ? AE_FCC('a', 'e', 'v', 't') : AE_FCC('G', 'U', 'R', 'L'), id_ = is_doc ? AE_FCC('o', 'd', 'o', 'c') : AE_FCC('G', 'U', 'R', 'L');
    handler_t h;
    if (!find_handler(cls, id_, &h)) return 0;
    int pid = getpid();
    ae_node *ev = ae_event(cls, id_, ae_scalar(AE_KPID, &pid, 4), __sync_fetch_and_add(&g_next_rid, 1), 0);
    if (is_doc) {
        ae_node *l = ae_list();
        for (int i = 0; i < n; i++) ae_list_put(l, 0, ae_scalar(AE_FURL, urls[i], (u32)strlen(urls[i])));
        ae_put_key(ev, AE_DIRECT, l);
    } else ae_put_key(ev, AE_DIRECT, ae_text(n ? urls[0] : ""));
    ae_node *reply = ae_event(AE_FCC('a', 'e', 'v', 't'), AE_FCC('a', 'n', 's', 'r'), 0, ev->return_id, 0);
    disp_t d = {ev, reply, 0};
    run_handler(&d);                                     /* openurl.c já chama no fio principal */
    ae_free(d.reply);
    ae_free(ev);
    return 1;
}

/* ================================================================ recepção (serviço Mach do app) */
typedef struct { u32 bits, size, remote, local, voucher, id; } mhdr_t;
typedef struct { u32 op, a, b, c; u64 q; u32 n1, n2; } body_t;   /* = svc_body_t */
#define SVC_MSG_ID 0x6d4f5331u
#define SVC_REPLY_ID 0x6d4f5332u
#define AE_MAX_MSG (1u << 20)
static void reply_to(u32 port, int rc, const u8 *data, usize n) {
    usize size = (sizeof(mhdr_t) + sizeof(body_t) + n + 3) & ~(usize)3;
    u8 *m = calloc(1, size);
    if (!m) return;
    mhdr_t *h = (mhdr_t *)m;
    h->bits = 18;                                         /* MOVE_SEND_ONCE */
    h->size = (u32)size;
    h->remote = port;
    h->id = SVC_REPLY_ID;
    body_t *b = (body_t *)(m + sizeof *h);
    b->op = (u32)rc;
    b->n1 = (u32)n;
    if (n) memcpy(b + 1, data, n);
    mach_msg(m, 1 | 0x10, (u32)size, 0, 0, 5000, 0);
    free(m);
}
static void *ae_listen(void *arg) {
    u32 port = (u32)(u64)arg;
    u8 *m = malloc(AE_MAX_MSG + 256);
    if (!m) return 0;
    for (;;) {
        mhdr_t *h = (mhdr_t *)m;
        memset(h, 0, sizeof *h);
        int r = mach_msg(m, 2, 0, AE_MAX_MSG + 256, port, 0, 0);
        if (r) { usleep(100000); continue; }
        body_t *b = (body_t *)(m + sizeof *h);
        if (h->id != SVC_MSG_ID || h->size < sizeof *h + sizeof *b + b->n1 || b->op != AE_OP_EVENT) {
            if (h->remote) reply_to(h->remote, paramErr, 0, 0);
            continue;
        }
        ae_node *ev = ae_parse((const u8 *)(b + 1), b->n1);
        if (!ev || ev->kind != AEK_EVENT) { ae_free(ev); if (h->remote) reply_to(h->remote, errAECorruptData, 0, 0); continue; }
        u32 rport = h->remote;
        ae_node *reply = rport ? ae_event(AE_FCC('a', 'e', 'v', 't'), AE_FCC('a', 'n', 's', 'r'), 0, ev->return_id, ev->transaction_id) : 0;
        int rc = dispatch_event(ev, &reply);
        ae_free(ev);
        if (rport) {
            u8 *wire = 0;
            usize wn = 0;
            if (reply && ae_serialize(reply, &wire, &wn)) wn = 0;
            reply_to(rport, rc, wire, wn);
            free(wire);
        }
        ae_free(reply);
    }
    return 0;
}
void cx_ae_listen(void) {
    static int started;
    if (started++) return;
    char svc[220];
    id mb = MSG0(id, (id)cx_class_named("NSBundle"), "mainBundle");
    id ident = mb ? MSG0(id, mb, "bundleIdentifier") : 0;
    const char *bid = ident && cx_is_str(ident) ? cx_cstr(ident) : 0;
    for (int k = 0; k < 2; k++) {
        if (k == 0) { if (!bid || !bid[0]) continue; cx_snprintf(svc, sizeof svc, AE_SVC_PREFIX "%s", bid); }
        else cx_snprintf(svc, sizeof svc, AE_SVC_PREFIX "pid.%d", getpid());
        u32 port = 0;
        if (bootstrap_check_in(bootstrap_port, svc, &port) || !port) continue;
        unsigned long t;
        if (pthread_create(&t, 0, ae_listen, (void *)(u64)port) == 0) pthread_detach(t);
    }
}

/* ================================================================ envio */
/* o alvo: bundle (via 'bund' ou o nome do app) ou pid; *self quando é este processo */
static int target_service(ae_node *t, char *svc, usize cap, char *bundle, usize bcap, int *self) {
    *self = 0;
    bundle[0] = 0;
    if (!t) { *self = 1; return 0; }
    if (t->type == AE_KPID && t->n == 4) {
        int pid;
        memcpy(&pid, t->data, 4);
        if (pid == getpid() || pid == 0) { *self = 1; return 0; }
        cx_snprintf(svc, cap, AE_SVC_PREFIX "pid.%d", pid);
        return 0;
    }
    if (t->type == AE_FCC('p', 's', 'n', ' ')) { *self = 1; return 0; }   /* kCurrentProcess */
    if (t->type == AE_BUND || ae_is_text(t->type)) {
        ae_node *s = t->type == AE_BUND ? ae_copy(t) : ae_coerce(t, AE_UTF8);
        if (!s) return procNotFound;
        usize n = s->n < bcap - 1 ? s->n : bcap - 1;
        memcpy(bundle, s->data, n);
        bundle[n] = 0;
        ae_free(s);
        id mb = MSG0(id, (id)cx_class_named("NSBundle"), "mainBundle");
        id me = mb ? MSG0(id, mb, "bundleIdentifier") : 0;
        if (me && cx_is_str(me) && !strcmp(cx_cstr(me), bundle)) { *self = 1; return 0; }
        cx_snprintf(svc, cap, AE_SVC_PREFIX "%s", bundle);
        return 0;
    }
    return procNotFound;
}
/* automação: kTCCServiceAppleEvents+<alvo>; ask = 0 só consulta */
static int automation_ok(const char *bundle, int ask, long wait_ms) {
    if (!bundle[0]) return noErr;                         /* alvo por pid sem bundle: não há a quem perguntar */
    char svc[260];
    cx_snprintf(svc, sizeof svc, "kTCCServiceAppleEvents+%s", bundle);
    int st = cx_tcc_state(svc);
    if (st == 2) return noErr;
    if (st == 1) return errAEEventNotPermitted;
    if (!ask) return errAEEventWouldRequireUserConsent;
    cx_tcc_request(svc);
    for (long w = 0; wait_ms < 0 || w < wait_ms; w += 200) {
        usleep(200000);
        st = cx_tcc_state(svc);
        if (st == 2) return noErr;
        if (st == 1 || st == 0) return errAEEventNotPermitted;
    }
    return errAEEventWouldRequireUserConsent;
}
int AEDeterminePermissionToAutomateTarget(const AEDesc *target, u32 cls, u32 id_, u8 ask) {
    (void)cls; (void)id_;
    char svc[260], bundle[200];
    int self;
    int r = target_service(desc_node(target), svc, sizeof svc, bundle, sizeof bundle, &self);
    if (r) return r;
    if (self) return noErr;
    u32 port = 0;
    if (bootstrap_look_up(bootstrap_port, svc, &port) || !port) return procNotFound;
    mach_port_deallocate(0x103, port);
    return automation_ok(bundle, ask, -1);
}
static long ticks_to_ms(long t) { return t == kNoTimeOut ? -1 : t == kAEDefaultTimeout || t == 0 ? 120000 : t * 1000 / 60; }

/* manda e espera; *reply_out recebe a resposta (ou NULL) */
static int send_event(ae_node *ev, ae_node **reply_out, int mode, long timeout_ms) {
    *reply_out = 0;
    char svc[260], bundle[200];
    int self;
    int r = target_service(ev->target, svc, sizeof svc, bundle, sizeof bundle, &self);
    if (r) return r;
    int want_reply = (mode & 3) == kAEWaitReply;
    if (self) {
        ae_node *reply = want_reply ? ae_event(AE_FCC('a', 'e', 'v', 't'), AE_FCC('a', 'n', 's', 'r'), 0, ev->return_id, 0) : 0;
        ae_node *copy = ae_copy(ev);
        dispatch_event(copy, &reply);
        ae_free(copy);
        *reply_out = reply;
        return noErr;
    }
    int interact = (mode & 0x30) != kAENeverInteract;
    /* 'mOSi': teclas para o método de entrada que o usuário escolheu nas fontes de entrada (imk.c); a escolha já é
     * o consentimento, como no macOS, onde o método de entrada não passa pela automação */
    r = ev->ev_class == AE_FCC('m', 'O', 'S', 'i') ? noErr : automation_ok(bundle, interact, timeout_ms < 0 ? -1 : timeout_ms);
    if (r) return r;
    u32 port = 0;
    if (bootstrap_look_up(bootstrap_port, svc, &port) || !port) {
        /* fechado: 'quit' não abre o app; o resto o inicia e espera o serviço */
        if (ev->ev_class == AE_FCC('a', 'e', 'v', 't') && ev->ev_id == AE_FCC('q', 'u', 'i', 't')) return noErr;
        if (!bundle[0] || cx_ls_launch_app(bundle)) return procNotFound;
        for (int w = 0; w < 600 && (bootstrap_look_up(bootstrap_port, svc, &port) || !port); w++) usleep(100000);
        if (!port) return procNotFound;
    }
    u8 *wire;
    usize wn;
    if (ae_serialize(ev, &wire, &wn)) { mach_port_deallocate(0x103, port); return memFullErr; }
    usize size = (sizeof(mhdr_t) + sizeof(body_t) + wn + 3) & ~(usize)3;
    if (size > AE_MAX_MSG) { free(wire); mach_port_deallocate(0x103, port); return memFullErr; }
    usize cap = want_reply ? AE_MAX_MSG + 256 : size;
    u8 *m = calloc(1, cap);
    if (!m) { free(wire); mach_port_deallocate(0x103, port); return memFullErr; }
    mhdr_t *h = (mhdr_t *)m;
    h->bits = 19;                                         /* COPY_SEND */
    h->size = (u32)size;
    h->remote = port;
    h->id = SVC_MSG_ID;
    body_t *b = (body_t *)(m + sizeof *h);
    b->op = AE_OP_EVENT;
    b->n1 = (u32)wn;
    memcpy(b + 1, wire, wn);
    free(wire);
    int kr;
    if (want_reply) {
        u32 rp = 0;
        if (mach_port_allocate(0x103, 1, &rp)) { free(m); mach_port_deallocate(0x103, port); return memFullErr; }
        h->bits |= 21u << 8;                              /* MAKE_SEND_ONCE */
        h->local = rp;
        u32 to = timeout_ms < 0 ? 0 : (u32)timeout_ms;
        kr = mach_msg(m, 1 | 2 | 0x10 | (timeout_ms < 0 ? 0 : 0x100), (u32)size, (u32)cap, rp, to, 0);
        mach_port_mod_refs(0x103, rp, 1, -1);
        mach_port_deallocate(0x103, port);
        if (kr == 0x10004003) { free(m); return errAETimeout; }        /* MACH_RCV_TIMED_OUT */
        if (kr) { free(m); return procNotFound; }
        body_t *rb = (body_t *)(m + sizeof *h);
        int rc = (int)rb->op;
        if (rb->n1) *reply_out = ae_parse((const u8 *)(rb + 1), rb->n1);
        free(m);
        (void)rc;                                         /* o erro do tratador vai no 'errn' da resposta */
        return noErr;
    }
    kr = mach_msg(m, 1 | 0x10, (u32)size, 0, 0, 10000, 0);
    mach_port_deallocate(0x103, port);
    free(m);
    return kr ? procNotFound : noErr;
}
int AESendMessage(const AEDesc *event, AEDesc *reply, int mode, long timeout) {
    if (reply) AEInitializeDesc(reply);
    ae_node *ev = desc_node(event);
    if (!ev || ev->kind != AEK_EVENT) return errAENotAEDesc;
    if ((mode & 3) == 0) mode |= kAEWaitReply;
    ae_node *r = 0;
    int rc = send_event(ev, &r, mode, ticks_to_ms(timeout));
    if (reply && r) desc_set(reply, r); else ae_free(r);
    return rc;
}
int AESend(const AEDesc *event, AEDesc *reply, int mode, i16 prio, long timeout, void *idle, void *filter) {
    (void)prio; (void)idle; (void)filter;
    return AESendMessage(event, reply, mode, timeout);
}
int AEGetTheCurrentEvent(AEDesc *ev) {
    if (!ev) return paramErr;
    AEInitializeDesc(ev);
    if (t_cur_event) desc_set(ev, ae_copy(t_cur_event));
    return noErr;
}
int AESetTheCurrentEvent(const AEDesc *ev) { (void)ev; return noErr; }
int AEResumeTheCurrentEvent(const AEDesc *ev, const AEDesc *reply, void *dispatcher, void *refcon) {
    (void)ev; (void)reply; (void)dispatcher; (void)refcon;
    return noErr;
}
int AESuspendTheCurrentEvent(const AEDesc *ev) { (void)ev; return noErr; }

/* -[NSAppleEventDescriptor sendEventWithOptions:timeout:error:] */
id CFErrorCreate(void *alloc, id domain, long code, id info);
extern const void *kCFErrorDomainOSStatus;
static id i_send(id self, SEL c, u64 opts, double timeout, id *err) {
    (void)c;
    if (err) *err = 0;
    ae_node *ev = aed_node(self), *r = 0;
    if (!ev || ev->kind != AEK_EVENT) { if (err) *err = CFErrorCreate(0, (id)kCFErrorDomainOSStatus, errAENotAEDesc, 0); return 0; }
    int mode = (int)opts;
    if ((mode & 3) == 0) mode |= kAEWaitReply;
    long ms = timeout < 0 ? (timeout == -2 ? -1 : 120000) : (long)(timeout * 1000);
    int rc = send_event(ev, &r, mode, ms);
    if (!rc && r) {
        ae_node *e = ae_get_key(r, K_ERRN);
        i32 code = 0;
        if (e) { ae_node *x = ae_coerce(e, AE_LONG); if (x) memcpy(&code, x->data, 4); ae_free(x); }
        if (code) rc = code;
    }
    if (rc) {
        if (err) *err = objc_autorelease(CFErrorCreate(0, (id)kCFErrorDomainOSStatus, rc, 0));
        ae_free(r);
        return 0;
    }
    return r ? aed_auto(r) : aed_auto(ae_scalar(AE_NULL, 0, 0));
}
static int aem_dispatch_raw(id self, SEL c, const AEDesc *ev, AEDesc *reply, void *refcon) {
    (void)self; (void)c; (void)refcon;
    ae_node *e = desc_node(ev);
    if (!e || e->kind != AEK_EVENT) return errAENotAEDesc;
    ae_node *r = reply ? ae_copy(desc_node(reply)) : 0;
    ae_node *copy = ae_copy(e);
    int rc = dispatch_event(copy, &r);
    ae_free(copy);
    if (reply) { ae_free(desc_node(reply)); desc_set(reply, r); } else ae_free(r);
    return rc;
}

/* ================================================================ registro */
void cx_appleevents_init(void) {
    static int done;
    if (done++) return;
    cx_define_class(&Box_c, &Box_m, "__MOSAEDescBox", cx_class_named("NSObject"), 8);
    cx_add_method(&Box_c, "dealloc", (void *)box_dealloc);
    cx_class_t *d = aed_class();
    if (d) {
        static const struct { const char *s; void *f; } cm[] = {
            {"nullDescriptor", (void *)c_null}, {"descriptorWithString:", (void *)c_with_string},
            {"descriptorWithBoolean:", (void *)c_with_bool}, {"descriptorWithInt32:", (void *)c_with_i32},
            {"descriptorWithDouble:", (void *)c_with_double}, {"descriptorWithEnumCode:", (void *)c_with_enum},
            {"descriptorWithTypeCode:", (void *)c_with_type}, {"descriptorWithDescriptorType:data:", (void *)c_with_data},
            {"descriptorWithDescriptorType:bytes:length:", (void *)c_with_bytes}, {"listDescriptor", (void *)c_list},
            {"recordDescriptor", (void *)c_record}, {"descriptorWithBundleIdentifier:", (void *)c_bundle},
            {"descriptorWithProcessIdentifier:", (void *)c_pid}, {"currentProcessDescriptor", (void *)c_current},
            {"descriptorWithApplicationURL:", (void *)c_app_url}, {"descriptorWithFileURL:", (void *)c_file_url},
            {"descriptorWithDate:", (void *)c_date},
            {"appleEventWithEventClass:eventID:targetDescriptor:returnID:transactionID:", (void *)c_event},
        };
        for (unsigned i = 0; i < sizeof cm / sizeof cm[0]; i++) cx_add_cmethod(d, cm[i].s, cm[i].f);
        static const struct { const char *s; void *f; } im[] = {
            {"initWithAEDescNoCopy:", (void *)i_init_desc_nocopy}, {"initWithAEDesc:", (void *)i_init_desc},
            {"descriptorType", (void *)i_type}, {"data", (void *)i_data}, {"stringValue", (void *)i_string},
            {"int32Value", (void *)i_i32}, {"doubleValue", (void *)i_double}, {"booleanValue", (void *)i_bool},
            {"enumCodeValue", (void *)i_code}, {"typeCodeValue", (void *)i_code}, {"fileURLValue", (void *)i_file_url},
            {"dateValue", (void *)i_date}, {"numberOfItems", (void *)i_count}, {"descriptorAtIndex:", (void *)i_at},
            {"insertDescriptor:atIndex:", (void *)i_insert}, {"removeDescriptorAtIndex:", (void *)i_remove_at},
            {"setDescriptor:forKeyword:", (void *)i_set_kw}, {"descriptorForKeyword:", (void *)i_for_kw},
            {"removeDescriptorWithKeyword:", (void *)i_remove_kw}, {"keywordForDescriptorAtIndex:", (void *)i_kw_at},
            {"setParamDescriptor:forKeyword:", (void *)i_set_kw}, {"paramDescriptorForKeyword:", (void *)i_for_kw},
            {"removeParamDescriptorWithKeyword:", (void *)i_remove_kw}, {"setAttributeDescriptor:forKeyword:", (void *)i_set_attr},
            {"attributeDescriptorForKeyword:", (void *)i_attr}, {"eventClass", (void *)i_ev_class}, {"eventID", (void *)i_ev_id},
            {"returnID", (void *)i_rid}, {"transactionID", (void *)i_tid}, {"isRecordDescriptor", (void *)i_is_record},
            {"coerceToDescriptorType:", (void *)i_coerce}, {"aeDesc", (void *)i_aedesc},
            {"sendEventWithOptions:timeout:error:", (void *)i_send}, {"copyWithZone:", (void *)i_copy}, {"copy", (void *)i_copy},
            {"description", (void *)i_description},
        };
        for (unsigned i = 0; i < sizeof im / sizeof im[0]; i++) cx_add_method(d, im[i].s, im[i].f);
    }
    cx_class_t *m = cx_class_named("NSAppleEventManager");
    if (m) {
        cx_add_cmethod(m, "sharedAppleEventManager", (void *)aem_shared);
        cx_add_method(m, "setEventHandler:andSelector:forEventClass:andEventID:", (void *)aem_set);
        cx_add_method(m, "removeEventHandlerForEventClass:andEventID:", (void *)aem_remove);
        cx_add_method(m, "currentAppleEvent", (void *)aem_current);
        cx_add_method(m, "currentReplyAppleEvent", (void *)aem_current_reply);
        cx_add_method(m, "suspendCurrentAppleEvent", (void *)aem_suspend);
        cx_add_method(m, "resumeWithSuspensionID:", (void *)aem_resume);
        cx_add_method(m, "replyAppleEventForSuspensionID:", (void *)aem_reply_for);
        cx_add_method(m, "appleEventForSuspensionID:", (void *)aem_event_for);
        cx_add_method(m, "dispatchRawAppleEvent:withRawReply:handlerRefCon:", (void *)aem_dispatch_raw);
    }
}

/* ================================================================ SPIs para applescript.c (NSAppleScript, osascript) */
const void *cx_aed_node(id d) { return aed_node(d); }
/* manda um evento (o nó é só lido); *reply recebe a resposta (dono: quem chamou, ae_free). timeout em ms (-1 = sem) */
int cx_ae_send_node(const void *ev, void **reply, int mode, long timeout_ms) {
    ae_node *r = 0;
    const ae_node *e = ev;
    if (!e || e->kind != AEK_EVENT) return errAENotAEDesc;
    int rc = send_event((ae_node *)e, &r, mode, timeout_ms);
    if (reply) *reply = r; else ae_free(r);
    return rc;
}
/* o app está aberto (tem o serviço de Apple Events)? */
int cx_ae_app_running(const char *bundle) {
    char svc[260];
    u32 port = 0;
    if (!bundle || !bundle[0]) return 0;
    cx_snprintf(svc, sizeof svc, AE_SVC_PREFIX "%s", bundle);
    if (bootstrap_look_up(bootstrap_port, svc, &port) || !port) return 0;
    mach_port_deallocate(0x103, port);
    return 1;
}
