/*
 * mOS libSystem — Carbon Event Manager (HIToolbox) de verdade:
 *   EventRef com parâmetros (CreateEvent, CopyEvent, Retain/ReleaseEvent, Get/SetEventParameter, Get/SetEventTime),
 *   alvos (aplicação, despachante, monitor) com tratadores (InstallEventHandler, Add/RemoveEventTypesToHandler,
 *   RemoveEventHandler, CallNextEventHandler), envio (SendEventToEventTarget/WithOptions, com propagação do despachante
 *   para a aplicação), fila principal (PostEventToQueue, ReceiveNextEvent, AcquireFirstMatchingEventInQueue,
 *   FindSpecificEventInQueue, GetNumEventsInQueue, RemoveEventFromQueue, Flush*), laços (Run/QuitEventLoop,
 *   InstallEventLoopTimer) e atalhos (RegisterEventHotKey).
 *   O AppKit repassa as teclas do app (cx_carbon_key): atalho registrado vira kEventHotKeyPressed/Released no alvo do
 *   atalho, e teclas e modificadores viram kEventRawKey* no alvo do monitor e da aplicação. Atalho global (com outro app
 *   na frente) depende do window server avisar o app; hoje só vale com o app na frente.
 * Núcleo (parâmetros, tipos): carbon_core.c, teste userland/tests-appkit/carbon_host.c.
 */
#include "foundation.h"
#include "sys.h"
#include "libc.h"
#include "carbon_core.h"

typedef int OSStatus;
#define noErr 0
#define paramErr (-50)
#define eventNotHandledErr (-9874)
#define eventLoopTimedOutErr (-9875)
#define eventInternalErr (-9868)
#define eventAlreadyPostedErr (-9860)
#define eventNotInQueueErr (-9862)
#define eventHotKeyExistsErr (-9878)
#define eventHotKeyInvalidErr (-9879)
void *cc_alloc(cc_size n) { return calloc(1, n); }
void cc_free(void *p) { free(p); }
u64 mach_absolute_time(void);
void *dispatch_get_main_queue(void);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
void dispatch_after_f(u64 when, void *q, void *ctx, void (*fn)(void *));
u64 dispatch_time(u64 when, long delta);
int CFRunLoopRunInMode(void *mode, double seconds, unsigned char ret_after_source);
void *CFRunLoopGetMain(void);
void CFRunLoopStop(void *rl);
extern void *kCFRunLoopDefaultMode;
extern id NSApp;

static double now_s(void) { return (double)mach_absolute_time() / 1e9; }
double GetCurrentEventTime(void) { return now_s(); }

/* ================================================================ EventRef */
OSStatus CreateEvent(void *alloc, u32 cls, u32 kind, double when, u32 attrs, cc_event **out) {
    (void)alloc;
    if (!out) return paramErr;
    *out = cc_event_new(cls, kind, when > 0 ? when : now_s(), attrs);
    return *out ? noErr : eventInternalErr;
}
cc_event *CopyEvent(cc_event *e) { return e ? cc_event_copy(e) : 0; }
cc_event *CopyEventAs(void *alloc, cc_event *e, u32 cls, u32 kind) { (void)alloc; cc_event *c = CopyEvent(e); if (c) { c->cls = cls; c->kind = kind; } return c; }
cc_event *RetainEvent(cc_event *e) { if (e) e->refs++; return e; }
u32 GetEventRetainCount(cc_event *e) { return e ? (u32)e->refs : 0; }
void ReleaseEvent(cc_event *e) { cc_event_release(e); }
u32 GetEventClass(cc_event *e) { return e ? e->cls : 0; }
u32 GetEventKind(cc_event *e) { return e ? e->kind : 0; }
double GetEventTime(cc_event *e) { return e ? e->time : 0; }
OSStatus SetEventTime(cc_event *e, double t) { if (!e) return paramErr; e->time = t; return noErr; }
OSStatus SetEventParameter(cc_event *e, u32 name, u32 type, u64 size, const void *data) {
    if (!e || (size && !data)) return paramErr;
    return cc_event_set(e, name, type, (cc_size)size, data) ? eventInternalErr : noErr;
}
OSStatus GetEventParameter(cc_event *e, u32 name, u32 type, u32 *out_type, u64 size, u64 *out_size, void *data) {
    if (!e) return paramErr;
    cc_size got = 0;
    int rc = cc_event_get(e, name, type, out_type, (cc_size)size, &got, data);
    if (out_size) *out_size = got;
    return rc;
}
OSStatus RemoveEventParameter(cc_event *e, u32 name) {
    if (!e) return paramErr;
    for (cc_param **pp = &e->params; *pp; pp = &(*pp)->next)
        if ((*pp)->name == name) { cc_param *p = *pp; *pp = p->next; cc_free(p); return noErr; }
    return -9870;
}

/* ================================================================ alvos e tratadores */
typedef OSStatus (*handler_fn)(void *call, cc_event *ev, void *data);
typedef struct handler { handler_fn fn; void *data; cc_specs specs; struct target *target; struct handler *next; } handler;
typedef struct target { handler *list; struct target *parent; } target;
static target g_app, g_dispatcher, g_monitor;
static int g_targets_ready;
static void targets_init(void) { if (g_targets_ready) return; g_targets_ready = 1; g_dispatcher.parent = &g_app; }
void *GetApplicationEventTarget(void) { targets_init(); return &g_app; }
void *GetEventDispatcherTarget(void) { targets_init(); return &g_dispatcher; }
void *GetEventMonitorTarget(void) { targets_init(); return &g_monitor; }
void *NewEventHandlerUPP(void *fn) { return fn; }
void DisposeEventHandlerUPP(void *upp) { (void)upp; }
void *NewEventLoopTimerUPP(void *fn) { return fn; }
void DisposeEventLoopTimerUPP(void *upp) { (void)upp; }
void *NewEventComparatorUPP(void *fn) { return fn; }
void DisposeEventComparatorUPP(void *upp) { (void)upp; }
OSStatus InstallEventHandler(target *t, handler_fn fn, u32 n, const cc_spec *types, void *data, handler **out) {
    targets_init();
    if (!t || !fn) return paramErr;
    handler *h = calloc(1, sizeof *h);
    if (!h) return eventInternalErr;
    h->fn = fn; h->data = data; h->target = t;
    if (n && types && cc_specs_add(&h->specs, types, (int)n)) { free(h); return eventInternalErr; }
    h->next = t->list;            /* o mais novo é chamado primeiro */
    t->list = h;
    if (out) *out = h;
    return noErr;
}
OSStatus AddEventTypesToHandler(handler *h, u32 n, const cc_spec *types) {
    if (!h) return paramErr;
    return cc_specs_add(&h->specs, types, (int)n) ? eventInternalErr : noErr;
}
OSStatus RemoveEventTypesFromHandler(handler *h, u32 n, const cc_spec *types) {
    if (!h) return paramErr;
    return cc_specs_remove(&h->specs, types, (int)n);
}
OSStatus RemoveEventHandler(handler *h) {
    if (!h) return paramErr;
    target *t = h->target;
    for (handler **pp = &t->list; *pp; pp = &(*pp)->next)
        if (*pp == h) { *pp = h->next; cc_specs_free(&h->specs); free(h); return noErr; }
    return paramErr;
}
OSStatus SetEventHandlerDescription(handler *h, void *desc) { (void)h; (void)desc; return noErr; }

/* chamada em andamento: de onde CallNextEventHandler continua */
typedef struct { target *t; handler *at; u32 opts; } call_ref;
#define kEventTargetDontPropagate 1
#define kEventTargetSendToAllHandlers 2
static cc_event *g_current[32];
static int g_depth;
cc_event *GetCurrentEvent(void) { return g_depth ? g_current[g_depth - 1] : 0; }
static OSStatus run_from(target *t, handler *start, cc_event *ev, u32 opts) {
    OSStatus last = eventNotHandledErr;
    for (target *cur = t; cur; cur = (opts & kEventTargetDontPropagate) ? 0 : cur->parent) {
        for (handler *h = cur == t ? start : cur->list; h; h = h->next) {
            if (!cc_specs_match(&h->specs, ev->cls, ev->kind)) continue;
            call_ref cr = {cur, h, opts};
            OSStatus rc = h->fn(&cr, ev, h->data);
            if (rc != eventNotHandledErr && !(opts & kEventTargetSendToAllHandlers)) return rc;
            if (rc != eventNotHandledErr) last = rc;
        }
    }
    return last;
}
OSStatus SendEventToEventTargetWithOptions(cc_event *ev, target *t, u32 opts) {
    targets_init();
    if (!ev || !t) return paramErr;
    if (g_depth < 32) g_current[g_depth] = ev;
    g_depth++;
    RetainEvent(ev);
    OSStatus rc = run_from(t, t->list, ev, opts);
    ReleaseEvent(ev);
    g_depth--;
    return rc;
}
OSStatus SendEventToEventTarget(cc_event *ev, target *t) { return SendEventToEventTargetWithOptions(ev, t, 0); }
OSStatus CallNextEventHandler(call_ref *cr, cc_event *ev) {
    if (!cr || !ev) return paramErr;
    return run_from(cr->t, cr->at->next, ev, cr->opts);
}

/* ================================================================ fila */
typedef struct qitem { cc_event *ev; struct qitem *next; } qitem;
typedef struct { qitem *head; } queue;
static queue g_main_q;
void *GetMainEventQueue(void) { return &g_main_q; }
void *GetCurrentEventQueue(void) { return &g_main_q; }
static int in_queue(queue *q, cc_event *e) { for (qitem *i = q->head; i; i = i->next) if (i->ev == e) return 1; return 0; }
static void deliver_posted(void *ctx) {
    cc_event *e = ctx;
    if (in_queue(&g_main_q, e)) {   /* ninguém tirou com ReceiveNextEvent: vai ao despachante, como no Carbon */
        for (qitem **pp = &g_main_q.head; *pp; pp = &(*pp)->next)
            if ((*pp)->ev == e) { qitem *it = *pp; *pp = it->next; free(it); break; }
        SendEventToEventTarget(e, GetEventDispatcherTarget());
        ReleaseEvent(e);           /* a referência da fila */
    }
    ReleaseEvent(e);               /* a do dispatch */
}
OSStatus PostEventToQueue(queue *q, cc_event *e, i16 prio) {
    if (!q || !e) return paramErr;
    if (in_queue(q, e)) return eventAlreadyPostedErr;
    qitem *it = calloc(1, sizeof *it);
    if (!it) return eventInternalErr;
    it->ev = RetainEvent(e);
    if (prio == 1 /* kEventPriorityHigh */ || !q->head) { it->next = q->head; q->head = it; }
    else { qitem *t = q->head; while (t->next) t = t->next; t->next = it; }
    if (q == &g_main_q) { RetainEvent(e); dispatch_async_f(dispatch_get_main_queue(), e, deliver_posted); }
    return noErr;
}
static int matches(cc_event *e, u32 n, const cc_spec *list) {
    if (!n || !list) return 1;
    for (u32 i = 0; i < n; i++) if (list[i].cls == e->cls && list[i].kind == e->kind) return 1;
    return 0;
}
static qitem *find(queue *q, u32 n, const cc_spec *list) { for (qitem *i = q->head; i; i = i->next) if (matches(i->ev, n, list)) return i; return 0; }
static void unlink_item(queue *q, qitem *it) { for (qitem **pp = &q->head; *pp; pp = &(*pp)->next) if (*pp == it) { *pp = it->next; free(it); return; } }
OSStatus ReceiveNextEvent(u32 n, const cc_spec *list, double timeout, u8 pull, cc_event **out) {
    if (out) *out = 0;
    double deadline = timeout < 0 ? 1e300 : now_s() + timeout;
    for (;;) {
        qitem *it = find(&g_main_q, n, list);
        if (it) {
            cc_event *e = it->ev;
            if (pull) unlink_item(&g_main_q, it);   /* a referência da fila passa a quem chamou */
            if (out) *out = e;
            return noErr;
        }
        double left = deadline - now_s();
        if (left <= 0) return eventLoopTimedOutErr;
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, left > 0.05 ? 0.05 : left, 1);
    }
}
cc_event *AcquireFirstMatchingEventInQueue(queue *q, u32 n, const cc_spec *list, u32 opts) {
    (void)opts;
    qitem *it = q ? find(q, n, list) : 0;
    return it ? RetainEvent(it->ev) : 0;
}
typedef u8 (*comparator_fn)(cc_event *e, void *data);
cc_event *FindSpecificEventInQueue(queue *q, comparator_fn cmp, void *data) {
    if (!q || !cmp) return 0;
    for (qitem *i = q->head; i; i = i->next) if (cmp(i->ev, data)) return i->ev;
    return 0;
}
i32 GetNumEventsInQueue(queue *q) { i32 n = 0; for (qitem *i = q ? q->head : 0; i; i = i->next) n++; return n; }
u8 IsEventInQueue(queue *q, cc_event *e) { return q && in_queue(q, e); }
OSStatus RemoveEventFromQueue(queue *q, cc_event *e) {
    if (!q || !e) return paramErr;
    for (qitem *i = q->head; i; i = i->next) if (i->ev == e) { unlink_item(q, i); ReleaseEvent(e); return noErr; }
    return eventNotInQueueErr;
}
OSStatus FlushEventsMatchingListFromQueue(queue *q, u32 n, const cc_spec *list) {
    if (!q) return paramErr;
    qitem *it;
    while ((it = find(q, n, list))) { cc_event *e = it->ev; unlink_item(q, it); ReleaseEvent(e); }
    return noErr;
}
OSStatus FlushEventQueue(queue *q) { return FlushEventsMatchingListFromQueue(q, 0, 0); }
OSStatus FlushSpecificEventsFromQueue(queue *q, comparator_fn cmp, void *data) {
    if (!q || !cmp) return paramErr;
    for (qitem **pp = &q->head; *pp;) {
        if (cmp((*pp)->ev, data)) { qitem *it = *pp; *pp = it->next; ReleaseEvent(it->ev); free(it); }
        else pp = &(*pp)->next;
    }
    return noErr;
}
/* ⌘. ou Esc na fila de teclas do Carbon */
u8 CheckEventQueueForUserCancel(void) {
    for (qitem *i = g_main_q.head; i; i = i->next) {
        if (i->ev->cls != 'keyb' || i->ev->kind != 1) continue;
        u32 code = 0, mods = 0;
        cc_event_get(i->ev, 'kcod', 'magn', 0, 4, 0, &code);
        cc_event_get(i->ev, 'kmod', 'magn', 0, 4, 0, &mods);
        if (code == 53 || (code == 47 && (mods & 0x100))) return 1;
    }
    return 0;
}

/* ================================================================ laços e temporizadores */
static int g_loop;
void *GetMainEventLoop(void) { return &g_loop; }
void *GetCurrentEventLoop(void) { return &g_loop; }
void *GetCFRunLoopFromEventLoop(void *l) { (void)l; return CFRunLoopGetMain(); }
OSStatus RunCurrentEventLoop(double timeout) {
    double deadline = timeout < 0 ? 1e300 : now_s() + timeout;
    while (now_s() < deadline) {
        int r = CFRunLoopRunInMode(kCFRunLoopDefaultMode, timeout < 0 ? 1e10 : deadline - now_s(), 0);
        if (r == 2) return noErr;   /* kCFRunLoopRunStopped: QuitEventLoop */
    }
    return eventLoopTimedOutErr;
}
OSStatus QuitEventLoop(void *l) { (void)l; CFRunLoopStop(CFRunLoopGetMain()); return noErr; }
void RunApplicationEventLoop(void) { MSG0(void, NSApp, "run"); }
void QuitApplicationEventLoop(void) { MSG(void, NSApp, "stop:", (id)0); }
typedef void (*timer_fn)(void *timer, void *data);
/* cada disparo agendado leva a geração do temporizador; reagendar ou remover troca a geração e o disparo velho não roda.
 * Removido, o temporizador é liberado pelo último disparo pendente. */
typedef struct { timer_fn fn; void *data; double interval; u32 gen; int alive, pending; } evtimer;
typedef struct { evtimer *t; u32 gen; } shot;
static void timer_fire(void *ctx);
static void schedule(evtimer *t, double after) {
    shot *s = malloc(sizeof *s);
    if (!s) return;
    s->t = t; s->gen = t->gen;
    t->pending++;
    dispatch_after_f(dispatch_time(0, (long)((after > 0 ? after : 0) * 1e9)), dispatch_get_main_queue(), s, timer_fire);
}
static void timer_fire(void *ctx) {
    shot *s = ctx;
    evtimer *t = s->t;
    u32 gen = s->gen;
    free(s);
    t->pending--;
    if (t->alive && gen == t->gen) {
        t->fn(t, t->data);
        if (t->alive && gen == t->gen && t->interval > 0) schedule(t, t->interval);
    }
    if (!t->alive && !t->pending) free(t);
}
OSStatus InstallEventLoopTimer(void *loop, double delay, double interval, timer_fn fn, void *data, evtimer **out) {
    (void)loop;
    if (!fn) return paramErr;
    evtimer *t = calloc(1, sizeof *t);
    if (!t) return eventInternalErr;
    t->fn = fn; t->data = data; t->interval = interval; t->alive = 1;
    schedule(t, delay);
    if (out) *out = t;
    return noErr;
}
OSStatus RemoveEventLoopTimer(evtimer *t) {
    if (!t || !t->alive) return paramErr;
    t->alive = 0; t->gen++;
    if (!t->pending) free(t);
    return noErr;
}
OSStatus SetEventLoopTimerNextFireTime(evtimer *t, double next) {
    if (!t || !t->alive) return paramErr;
    t->gen++;
    if (next >= 0 && next < 1e300) schedule(t, next);   /* kEventDurationForever (-1): só para */
    return noErr;
}

/* ================================================================ atalhos e teclas do AppKit */
typedef struct { u32 signature, id; } hotkey_id;
typedef struct hotkey { u32 code, mods; hotkey_id hid; target *t; int down; struct hotkey *next; } hotkey;
static hotkey *g_hot;
static u32 g_symbolic_mode, g_symbolic_depth;
OSStatus RegisterEventHotKey(u32 code, u32 mods, hotkey_id hid, target *t, u32 opts, hotkey **out) {
    (void)opts;
    targets_init();
    for (hotkey *h = g_hot; h; h = h->next) if (h->code == code && h->mods == mods) return eventHotKeyExistsErr;
    hotkey *h = calloc(1, sizeof *h);
    if (!h) return eventInternalErr;
    h->code = code; h->mods = mods & 0x1B00; h->hid = hid; h->t = t ? t : &g_app;
    h->next = g_hot; g_hot = h;
    if (out) *out = h;
    return noErr;
}
OSStatus UnregisterEventHotKey(hotkey *hk) {
    for (hotkey **pp = &g_hot; *pp; pp = &(*pp)->next) if (*pp == hk) { *pp = hk->next; free(hk); return noErr; }
    return eventHotKeyInvalidErr;
}
/* PushSymbolicHotKeyMode: os atalhos simbólicos do sistema (⌘Tab...) são do window server; o modo fica guardado */
void *PushSymbolicHotKeyMode(u32 mode) { g_symbolic_mode = mode; return (void *)(u64)(++g_symbolic_depth); }
void PopSymbolicHotKeyMode(void *token) {
    if ((u64)token == g_symbolic_depth && g_symbolic_depth) g_symbolic_depth--;
    if (!g_symbolic_depth) g_symbolic_mode = 0;
}
u32 GetSymbolicHotKeyMode(void) { return g_symbolic_mode; }
static u32 carbon_mods(u64 ns) {
    u32 c = 0;
    if (ns & (1u << 20)) c |= 0x100;
    if (ns & (1u << 17)) c |= 0x200;
    if (ns & (1u << 16)) c |= 0x400;
    if (ns & (1u << 19)) c |= 0x800;
    if (ns & (1u << 18)) c |= 0x1000;
    return c;
}
static void send_raw(u32 kind, u16 code, u32 mods) {
    cc_event *e = cc_event_new('keyb', kind, now_s(), 0);
    if (!e) return;
    u32 c32 = code;
    cc_event_set(e, 'kcod', 'magn', 4, &c32);
    cc_event_set(e, 'kmod', 'magn', 4, &mods);
    SendEventToEventTarget(e, &g_monitor);
    SendEventToEventTarget(e, &g_app);
    cc_event_release(e);
}
/* chamado pelo AppKit em cada tecla: 0 descida, 1 subida, 2 modificadores. Devolve 1 se um atalho tomou a tecla. */
int cx_carbon_key(int phase, u16 code, u64 nsmods) {
    targets_init();
    u32 mods = carbon_mods(nsmods);
    if (phase == 2) { send_raw(4 /* kEventRawKeyModifiersChanged */, 0, mods); return 0; }
    int taken = 0;
    for (hotkey *h = g_hot; h; h = h->next) {
        if (h->code != code) continue;
        if (phase == 0 && h->mods != (mods & 0x1B00)) continue;
        if (phase == 1 && !h->down) continue;
        h->down = phase == 0;
        cc_event *e = cc_event_new('keyb', phase == 0 ? 5 : 6, now_s(), 0);   /* kEventHotKeyPressed / Released */
        if (!e) continue;
        cc_event_set(e, '----', 'hkid', sizeof h->hid, &h->hid);
        SendEventToEventTarget(e, h->t);
        cc_event_release(e);
        taken = 1;
    }
    if (!taken) send_raw(phase == 0 ? 1 : 3 /* kEventRawKeyDown / Up */, code, mods);
    return taken;
}
