/*
 * mOS libSystem — NSUndoManager completo, como no macOS:
 *  - pilhas de desfazer e refazer com grupos aninhados; groupsByEvent abre um grupo sozinho na primeira ação
 *    e o fecha no fim da volta do run loop (fila principal);
 *  - três jeitos de registrar: target/selector/objeto, bloco (registerUndoWithTarget:handler:) e
 *    prepareWithInvocationTarget: (um proxy grava a NSInvocation);
 *  - durante o undo as ações registradas vão para a pilha de refazer, durante o redo para a de desfazer, e uma
 *    ação nova fora disso limpa a de refazer;
 *  - isUndoing/isRedoing, levelsOfUndo, registro desligável com contador, nomes de ação e títulos de menu,
 *    removeAllActions(WithTarget:), ações descartáveis;
 *  - as notificações com object = o gerente: Checkpoint, DidOpen/WillClose/DidCloseUndoGroup,
 *    Will/DidUndoChange e Will/DidRedoChange (o NSDocument do AppKit acompanha as edições por elas).
 */
#include "foundation.h"
#include "libc.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void *_Block_copy(const void *b);
void _Block_release(const void *b);
IMP cx_lookup(cx_class_t *cls, SEL sel);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
void *dispatch_get_main_queue(void);

extern id NSUndoManagerDidCloseUndoGroupNotification, NSUndoManagerDidRedoChangeNotification, NSUndoManagerDidUndoChangeNotification;
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
#define UK(var, text) static struct { void *isa; u64 flags; const char *s; u64 n; } var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; const void *var = &var##_s;
UK(NSUndoManagerCheckpointNotification, "NSUndoManagerCheckpointNotification")
UK(NSUndoManagerWillUndoChangeNotification, "NSUndoManagerWillUndoChangeNotification")
UK(NSUndoManagerWillRedoChangeNotification, "NSUndoManagerWillRedoChangeNotification")
UK(NSUndoManagerDidOpenUndoGroupNotification, "NSUndoManagerDidOpenUndoGroupNotification")
UK(NSUndoManagerWillCloseUndoGroupNotification, "NSUndoManagerWillCloseUndoGroupNotification")
UK(NSUndoManagerGroupIsDiscardableKey, "NSUndoManagerGroupIsDiscardableKey")

cx_class_t NSUndoManager_c __asm__("OBJC_CLASS_$_NSUndoManager");
cx_class_t NSUndoManager_m __asm__("OBJC_METACLASS_$_NSUndoManager");
static cx_class_t prox_c, prox_m;

typedef struct { void *isa; int flags, reserved; void (*invoke)(void *, id); } blk_t;

/* uma ação: target + selector + objeto, ou bloco, ou invocação; ou um grupo aninhado */
typedef struct action {
    struct action *next;
    int kind;                 /* 0 selector, 1 bloco, 2 invocação, 3 grupo */
    id target, arg, inv;
    SEL sel;
    void *block;
    struct group *sub;
} action_t;
typedef struct group {
    action_t *head;           /* a mais recente primeiro: desfazer na ordem inversa do registro */
    id name;
    int discardable;
    struct group *next;       /* próximo grupo na pilha */
    struct group *parent;     /* grupo de fora, enquanto aberto */
} group_t;

typedef struct {
    cx_class_t *isa;
    group_t *undo, *redo;     /* pilhas (topo primeiro) */
    group_t *open;            /* o grupo aberto mais interno */
    int level, disabled, undoing, redoing, by_event, auto_open, close_pending;
    i64 levels;
    id modes, proxy_target;
    volatile int lock;
} um_t;
typedef struct { cx_class_t *isa; id mgr; } prox_t;

#define U(o) ((um_t *)(o))
static id cls(const char *n) { return (id)cx_class_named(n); }
static void post(id self, const void *name, id info) {
    id nc = MSG0(id, cls("NSNotificationCenter"), "defaultCenter");
    MSG(void, nc, "postNotificationName:object:userInfo:", (id)name, self, info);
}

static void free_group(group_t *g);
static void free_action(action_t *a) {
    if (a->target) objc_release(a->target);
    if (a->arg) objc_release(a->arg);
    if (a->inv) objc_release(a->inv);
    if (a->block) _Block_release(a->block);
    if (a->sub) free_group(a->sub);
    free(a);
}
static void free_group(group_t *g) {
    for (action_t *a = g->head, *n; a; a = n) { n = a->next; free_action(a); }
    if (g->name) objc_release(g->name);
    free(g);
}
static void free_stack(group_t **st) { for (group_t *g = *st, *n; g; g = n) { n = g->next; free_group(g); } *st = 0; }
static int group_empty(const group_t *g) {
    for (const action_t *a = g->head; a; a = a->next) if (a->kind != 3 || !group_empty(a->sub)) return 0;
    return 1;
}
static void trim(um_t *u, group_t **st) {
    if (u->levels <= 0) return;
    i64 n = 0;
    for (group_t **pp = st; *pp; pp = &(*pp)->next) {
        if (++n > u->levels) { group_t *rest = *pp; *pp = 0; free_stack(&rest); return; }
    }
}

/* ---- grupos */
static void close_group(id self);
static void auto_close(void *ctx) {
    id self = ctx;
    um_t *u = U(self);
    u->close_pending = 0;
    if (u->auto_open && u->level == 1) { u->auto_open = 0; close_group(self); }
    post(self, NSUndoManagerCheckpointNotification, 0);
    objc_release(self);
}
static void open_group(id self) {
    um_t *u = U(self);
    group_t *g = calloc(1, sizeof *g);
    if (!g) return;
    g->parent = u->open;
    u->open = g;
    u->level++;
    post(self, NSUndoManagerDidOpenUndoGroupNotification, 0);
}
/* o grupo pronto vai para a pilha certa: refazer durante o undo, desfazer nos outros casos */
static void push_done(um_t *u, group_t *g) {
    group_t **st = u->undoing ? &u->redo : &u->undo;
    g->next = *st;
    *st = g;
    trim(u, st);
}
static void close_group(id self) {
    um_t *u = U(self);
    group_t *g = u->open;
    if (!g) return;
    id info = 0;
    if (g->discardable) {
        info = MSG0(id, cls("NSMutableDictionary"), "dictionary");
        id yes = MSG(id, cls("NSNumber"), "numberWithBool:", (BOOL)1);
        MSG(void, info, "setObject:forKey:", yes, (id)NSUndoManagerGroupIsDiscardableKey);
    }
    post(self, NSUndoManagerWillCloseUndoGroupNotification, info);
    u->open = g->parent;
    u->level--;
    g->parent = 0;
    if (group_empty(g)) free_group(g);
    else if (u->open) {
        /* grupo aninhado: vira uma ação do de fora */
        action_t *a = calloc(1, sizeof *a);
        if (a) { a->kind = 3; a->sub = g; a->next = u->open->head; u->open->head = a; if (g->discardable) u->open->discardable = 1; }
        else free_group(g);
    } else push_done(u, g);
    post(self, NSUndoManagerDidCloseUndoGroupNotification, info);
}
/* registro: abre o grupo do evento se preciso e põe a ação no grupo aberto */
static void record(id self, action_t *a) {
    um_t *u = U(self);
    if (u->disabled) { free_action(a); return; }
    if (!u->open) {
        if (!u->by_event) { free_action(a); return; }   /* sem grupo e sem grupos por evento: o macOS lança */
        open_group(self);
        u->auto_open = 1;
        if (!u->close_pending) { u->close_pending = 1; objc_retain(self); dispatch_async_f(dispatch_get_main_queue(), self, auto_close); }
    }
    a->next = u->open->head;
    u->open->head = a;
    /* ação nova fora do undo/redo apaga o que dava para refazer */
    if (!u->undoing && !u->redoing) free_stack(&u->redo);
}

static void run_group(group_t *g);
static void run_action(action_t *a) {
    switch (a->kind) {
    case 0: ((void (*)(id, SEL, id))objc_msgSend)(a->target, a->sel, a->arg); break;
    case 1: ((blk_t *)a->block)->invoke(a->block, a->target); break;
    case 2: MSG0(void, a->inv, "invoke"); break;
    case 3: run_group(a->sub); break;
    }
}
static void run_group(group_t *g) { for (action_t *a = g->head; a; a = a->next) run_action(a); }

/* ---- API */
static id um_init(id self, SEL c) {
    (void)c;
    U(self)->by_event = 1;
    U(self)->modes = objc_retain(MSG(id, cls("NSArray"), "arrayWithObject:", cx_str("kCFRunLoopDefaultMode")));
    return self;
}
static void um_reg(id self, SEL c, id target, SEL sel, id arg) {
    (void)c;
    if (!target || !sel) return;
    action_t *a = calloc(1, sizeof *a);
    if (!a) return;
    a->kind = 0; a->target = objc_retain(target); a->sel = sel; a->arg = arg ? objc_retain(arg) : 0;
    record(self, a);
}
static void um_reg_block(id self, SEL c, id target, void *handler) {
    (void)c;
    if (!handler) return;
    action_t *a = calloc(1, sizeof *a);
    if (!a) return;
    a->kind = 1; a->target = target ? objc_retain(target) : 0; a->block = _Block_copy(handler);
    record(self, a);
}
static id um_prepare(id self, SEL c, id target) {
    (void)c;
    um_t *u = U(self);
    id old = u->proxy_target;
    u->proxy_target = target ? objc_retain(target) : 0;
    if (old) objc_release(old);
    prox_t *p = (prox_t *)cx_alloc_instance(&prox_c);
    p->mgr = self;
    return objc_autorelease((id)p);
}
/* o proxy: a assinatura vem do alvo; a invocação gravada vira a ação */
static id prox_sig(id self, SEL c, SEL sel) {
    (void)c;
    id t = U(((prox_t *)self)->mgr)->proxy_target;
    return t ? MSG(id, t, "methodSignatureForSelector:", sel) : 0;
}
static void prox_fwd(id self, SEL c, id inv) {
    (void)c;
    id mgr = ((prox_t *)self)->mgr;
    id t = U(mgr)->proxy_target;
    if (!t || !inv) return;
    MSG(void, inv, "setTarget:", t);
    MSG0(void, inv, "retainArguments");
    action_t *a = calloc(1, sizeof *a);
    if (!a) return;
    a->kind = 2; a->inv = objc_retain(inv); a->target = objc_retain(t);
    record(mgr, a);
}
static BOOL prox_responds(id self, SEL c, SEL sel) {
    (void)c;
    id t = U(((prox_t *)self)->mgr)->proxy_target;
    return t && cx_responds(object_getClass(t), sel);
}

static void um_begin(id self, SEL c) {
    (void)c;
    post(self, NSUndoManagerCheckpointNotification, 0);
    open_group(self);
}
static void um_end(id self, SEL c) {
    (void)c;
    um_t *u = U(self);
    if (u->level <= 0) return;   /* fechar sem abrir: o macOS lança */
    if (u->auto_open && u->level == 1) u->auto_open = 0;
    close_group(self);
}
static i64 um_level(id self, SEL c) { (void)c; return U(self)->level; }
static BOOL um_by_event(id self, SEL c) { (void)c; return (BOOL)U(self)->by_event; }
static void um_set_by_event(id self, SEL c, BOOL v) { (void)c; U(self)->by_event = v; }
static void um_disable(id self, SEL c) { (void)c; U(self)->disabled++; }
static void um_enable(id self, SEL c) { (void)c; if (U(self)->disabled > 0) U(self)->disabled--; }
static BOOL um_reg_enabled(id self, SEL c) { (void)c; return U(self)->disabled == 0; }
static i64 um_levels(id self, SEL c) { (void)c; return U(self)->levels; }
static void um_set_levels(id self, SEL c, i64 v) { (void)c; U(self)->levels = v < 0 ? 0 : v; trim(U(self), &U(self)->undo); trim(U(self), &U(self)->redo); }
static BOOL um_undoing(id self, SEL c) { (void)c; return (BOOL)U(self)->undoing; }
static BOOL um_redoing(id self, SEL c) { (void)c; return (BOOL)U(self)->redoing; }

/* o grupo do evento ainda aberto entra na pilha antes de desfazer, como no macOS */
static void close_event_group(id self) {
    um_t *u = U(self);
    if (u->auto_open && u->level == 1) { u->auto_open = 0; close_group(self); }
}
static BOOL um_can_undo(id self, SEL c) {
    (void)c;
    um_t *u = U(self);
    if (u->undo) return YES;
    return u->open && u->level == 1 && u->auto_open && !group_empty(u->open);
}
static BOOL um_can_redo(id self, SEL c) { (void)c; post(self, NSUndoManagerCheckpointNotification, 0); return U(self)->redo != 0; }
static void um_undo_nested(id self, SEL c) {
    (void)c;
    um_t *u = U(self);
    close_event_group(self);
    group_t *g = u->undo;
    if (!g) return;
    post(self, NSUndoManagerCheckpointNotification, 0);
    post(self, NSUndoManagerWillUndoChangeNotification, 0);
    u->undo = g->next;
    g->next = 0;
    u->undoing = 1;
    open_group(self);
    run_group(g);
    if (u->open && !u->open->name && g->name) u->open->name = objc_retain(g->name);
    close_group(self);
    u->undoing = 0;
    free_group(g);
    post(self, NSUndoManagerDidUndoChangeNotification, 0);
}
static void um_undo(id self, SEL c) {
    um_t *u = U(self);
    close_event_group(self);
    if (u->level > 0) return;   /* com grupo aberto pelo app, o macOS lança */
    um_undo_nested(self, c);
}
static void um_redo(id self, SEL c) {
    (void)c;
    um_t *u = U(self);
    close_event_group(self);
    group_t *g = u->redo;
    if (!g || u->level > 0) return;
    post(self, NSUndoManagerCheckpointNotification, 0);
    post(self, NSUndoManagerWillRedoChangeNotification, 0);
    u->redo = g->next;
    g->next = 0;
    u->redoing = 1;
    open_group(self);
    run_group(g);
    if (u->open && !u->open->name && g->name) u->open->name = objc_retain(g->name);
    close_group(self);
    u->redoing = 0;
    free_group(g);
    post(self, NSUndoManagerDidRedoChangeNotification, 0);
}
static void um_remove_all(id self, SEL c) {
    (void)c;
    um_t *u = U(self);
    free_stack(&u->undo);
    free_stack(&u->redo);
    while (u->open) { group_t *g = u->open; u->open = g->parent; free_group(g); }
    u->level = 0;
    u->auto_open = 0;
}
/* tira as ações de um alvo (em todos os grupos, inclusive aninhados) */
static void strip_group(group_t *g, id target) {
    for (action_t **pp = &g->head; *pp;) {
        action_t *a = *pp;
        if (a->kind == 3) strip_group(a->sub, target);
        if (a->kind != 3 && a->target == target) { *pp = a->next; free_action(a); continue; }
        pp = &a->next;
    }
}
static void strip_stack(group_t **st, id target) {
    for (group_t **pp = st; *pp;) {
        strip_group(*pp, target);
        if (group_empty(*pp)) { group_t *g = *pp; *pp = g->next; free_group(g); continue; }
        pp = &(*pp)->next;
    }
}
static void um_remove_target(id self, SEL c, id target) {
    (void)c;
    um_t *u = U(self);
    strip_stack(&u->undo, target);
    strip_stack(&u->redo, target);
    for (group_t *g = u->open; g; g = g->parent) strip_group(g, target);
}
static void um_set_action_name(id self, SEL c, id name) {
    (void)c;
    um_t *u = U(self);
    group_t *g = u->open ? u->open : u->undoing ? u->redo : u->undo;
    if (!g) return;
    id o = g->name;
    g->name = name ? MSG0(id, name, "copy") : 0;
    if (o) objc_release(o);
}
static void um_set_discardable(id self, SEL c, BOOL v) {
    (void)c;
    um_t *u = U(self);
    group_t *g = u->open ? u->open : u->undo;
    if (g) g->discardable = v;
}
static BOOL um_undo_discardable(id self, SEL c) { (void)c; group_t *g = U(self)->undo; return g ? (BOOL)g->discardable : NO; }
static BOOL um_redo_discardable(id self, SEL c) { (void)c; group_t *g = U(self)->redo; return g ? (BOOL)g->discardable : NO; }
static id um_undo_name(id self, SEL c) {
    (void)c;
    um_t *u = U(self);
    group_t *g = u->undo ? u->undo : (u->open && u->auto_open ? u->open : 0);
    return g && g->name ? g->name : cx_str("");
}
static id um_redo_name(id self, SEL c) { (void)c; group_t *g = U(self)->redo; return g && g->name ? g->name : cx_str(""); }
static int pt(void) { const char *l = cx_locale_id(); return l[0] == 'p'; }
static id menu_title(const char *verb_en, const char *verb_pt, id name) {
    const char *v = pt() ? verb_pt : verb_en;
    const char *n = name ? cx_cstr(name) : "";
    char b[300];
    if (n[0]) snprintf(b, sizeof b, "%s %s", v, n);
    else snprintf(b, sizeof b, "%s", v);
    return cx_str(b);
}
static id um_undo_title(id self, SEL c) { return menu_title("Undo", "Desfazer", um_undo_name(self, c)); }
static id um_redo_title(id self, SEL c) { return menu_title("Redo", "Refazer", um_redo_name(self, c)); }
static id um_undo_title_for(id self, SEL c, id name) { (void)self; (void)c; return menu_title("Undo", "Desfazer", name); }
static id um_redo_title_for(id self, SEL c, id name) { (void)self; (void)c; return menu_title("Redo", "Refazer", name); }
static id um_modes(id self, SEL c) { (void)c; return U(self)->modes; }
static void um_set_modes(id self, SEL c, id m) { (void)c; id o = U(self)->modes; U(self)->modes = m ? MSG0(id, m, "copy") : 0; if (o) objc_release(o); }
static void um_dealloc(id self, SEL c) {
    (void)c;
    um_remove_all(self, 0);
    um_t *u = U(self);
    if (u->modes) objc_release(u->modes);
    if (u->proxy_target) objc_release(u->proxy_target);
    IMP sup = cx_lookup(cx_class_named("NSObject"), sel_registerName("dealloc"));
    if (sup) sup(self, sel_registerName("dealloc"));
}

#define IM(s, f) cx_add_method(&NSUndoManager_c, s, (void *)(f))
void cx_fdn_undo_init(void) {
    cx_class_t *ob = cx_class_named("NSObject");
    if (!ob || cx_class_named("NSUndoManager") == &NSUndoManager_c) return;
    cx_define_class(&prox_c, &prox_m, "__mOSUndoInvocationProxy", ob, (u32)(sizeof(prox_t) - 8));
    cx_add_method(&prox_c, "methodSignatureForSelector:", (void *)prox_sig);
    cx_add_method(&prox_c, "forwardInvocation:", (void *)prox_fwd);
    cx_add_method(&prox_c, "respondsToSelector:", (void *)prox_responds);
    cx_define_class(&NSUndoManager_c, &NSUndoManager_m, "NSUndoManager", ob, (u32)(sizeof(um_t) - 8));
    IM("init", um_init);
    IM("registerUndoWithTarget:selector:object:", um_reg); IM("registerUndoWithTarget:handler:", um_reg_block);
    IM("prepareWithInvocationTarget:", um_prepare);
    IM("beginUndoGrouping", um_begin); IM("endUndoGrouping", um_end); IM("groupingLevel", um_level);
    IM("groupsByEvent", um_by_event); IM("setGroupsByEvent:", um_set_by_event);
    IM("disableUndoRegistration", um_disable); IM("enableUndoRegistration", um_enable);
    IM("isUndoRegistrationEnabled", um_reg_enabled);
    IM("levelsOfUndo", um_levels); IM("setLevelsOfUndo:", um_set_levels);
    IM("isUndoing", um_undoing); IM("isRedoing", um_redoing);
    IM("canUndo", um_can_undo); IM("canRedo", um_can_redo);
    IM("undo", um_undo); IM("redo", um_redo); IM("undoNestedGroup", um_undo_nested);
    IM("removeAllActions", um_remove_all); IM("removeAllActionsWithTarget:", um_remove_target);
    IM("setActionName:", um_set_action_name); IM("undoActionName", um_undo_name); IM("redoActionName", um_redo_name);
    IM("setActionIsDiscardable:", um_set_discardable);
    IM("undoActionIsDiscardable", um_undo_discardable); IM("redoActionIsDiscardable", um_redo_discardable);
    IM("undoMenuItemTitle", um_undo_title); IM("redoMenuItemTitle", um_redo_title);
    IM("undoMenuTitleForUndoActionName:", um_undo_title_for); IM("redoMenuTitleForUndoActionName:", um_redo_title_for);
    IM("runLoopModes", um_modes); IM("setRunLoopModes:", um_set_modes);
    IM("dealloc", um_dealloc);
}
