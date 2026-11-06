/*
 * mOS libSystem — entrada de eventos do canvas Swift (_CXSwiftCanvas, criado por mos_canvas em swiftbridge.c)
 * para o OpenSwiftUI: mouse down/up/arrasto, botão direito, rolagem e teclado.
 *
 * O canvas guarda o seu estado numa tabela privada de swiftbridge.c; aqui fica uma tabela própria
 * view -> (handler, ctx). Os métodos de evento são acrescentados à classe do canvas na primeira ligação;
 * um canvas sem handler registrado aqui repassa o evento ao IMP original (mos_canvas_input continua valendo).
 */
#include "foundation.h"
#include "libc.h"
#include "../sdk/swift/MOSBridge/MOSBridge_input.h"

typedef struct { double x, y; } in_pt_t;
extern id objc_retain(id);
extern void objc_release(id);

typedef struct { id view; mos_input_handler_t fn; void *ctx; } input_slot_t;
static input_slot_t slots[64];
static int nslots;
static cx_class_t *hooked_cls;
/* IMPs originais do canvas (para canvases sem handler daqui) */
static IMP orig_down, orig_up, orig_drag, orig_rdown, orig_scroll, orig_key;

static input_slot_t *slot_of(id v) {
    for (int i = 0; i < nslots; i++) if (slots[i].view == v) return &slots[i];
    return 0;
}

/* posição do evento na view (origem no topo: o canvas é invertido) */
static in_pt_t event_point(id self, id e) {
    in_pt_t p = MSG0(in_pt_t, e, "locationInWindow");
    return MSG(in_pt_t, self, "convertPoint:fromView:", p, (id)0);
}

static void deliver(id self, id e, int kind, int button) {
    input_slot_t *s = slot_of(self);
    if (!s || !s->fn) return;
    mos_input_event_t ev;
    memset(&ev, 0, sizeof ev);
    ev.kind = kind;
    ev.button = button;
    ev.modifiers = (unsigned long)MSG0(u64, e, "modifierFlags");
    ev.click_count = (int)MSG0(i64, e, "clickCount");
    if (kind == MOS_INPUT_KEY_DOWN) {
        ev.key_code = (int)MSG0(u16, e, "keyCode");
        id ch = MSG0(id, e, "characters");
        ev.characters = ch ? cx_cstr(ch) : "";
    } else {
        in_pt_t p = event_point(self, e);
        ev.x = p.x; ev.y = p.y;
        ev.characters = "";
        if (kind == MOS_INPUT_SCROLL) {
            ev.delta_x = MSG0(double, e, "deltaX");
            ev.delta_y = MSG0(double, e, "scrollingDeltaY");
        }
    }
    s->fn(s->ctx, &ev);
}

#define FORWARD(orig) do { if (orig) ((void (*)(id, SEL, id))orig)(self, c, e); } while (0)
static void in_down(id self, SEL c, id e) { if (slot_of(self)) deliver(self, e, MOS_INPUT_MOUSE_DOWN, 0); else FORWARD(orig_down); }
static void in_up(id self, SEL c, id e) { if (slot_of(self)) deliver(self, e, MOS_INPUT_MOUSE_UP, 0); else FORWARD(orig_up); }
static void in_drag(id self, SEL c, id e) { if (slot_of(self)) deliver(self, e, MOS_INPUT_MOUSE_DRAGGED, 0); else FORWARD(orig_drag); }
static void in_rdown(id self, SEL c, id e) { if (slot_of(self)) deliver(self, e, MOS_INPUT_RIGHT_MOUSE_DOWN, 1); else FORWARD(orig_rdown); }
static void in_scroll(id self, SEL c, id e) { if (slot_of(self)) deliver(self, e, MOS_INPUT_SCROLL, 0); else FORWARD(orig_scroll); }
static void in_key(id self, SEL c, id e) { if (slot_of(self)) deliver(self, e, MOS_INPUT_KEY_DOWN, 0); else FORWARD(orig_key); }

/* troca o método na própria classe (class_replaceMethod limpa os caches de métodos); devolve o IMP anterior,
 * ou o herdado da superclasse quando a classe ainda não definia o seletor */
extern IMP class_replaceMethod(cx_class_t *c, SEL s, IMP imp, const char *types);
static IMP replace(cx_class_t *k, const char *name, void *imp) {
    SEL s = sel_registerName(name);
    IMP old = class_replaceMethod(k, s, (IMP)imp, "v@:@");
    if (!old && k->super) old = cx_lookup(k->super, s);
    return old;
}
static void hook_class(cx_class_t *k) {
    if (hooked_cls == k) return;
    hooked_cls = k;
    orig_down = replace(k, "mouseDown:", (void *)in_down);
    orig_up = replace(k, "mouseUp:", (void *)in_up);
    orig_drag = replace(k, "mouseDragged:", (void *)in_drag);
    orig_rdown = replace(k, "rightMouseDown:", (void *)in_rdown);
    orig_scroll = replace(k, "scrollWheel:", (void *)in_scroll);
    orig_key = replace(k, "keyDown:", (void *)in_key);
}

void mos_canvas_events(void *view, mos_input_handler_t handler, void *ctx) {
    id v = (id)view;
    if (!v) return;
    hook_class((cx_class_t *)v->isa);
    input_slot_t *s = slot_of(v);
    if (!s && nslots < 64) { s = &slots[nslots++]; s->view = v; }
    if (!s) return;
    s->fn = handler;
    s->ctx = ctx;
    mos_canvas_focus(view);
}

void mos_canvas_focus(void *view) {
    id v = (id)view;
    id w = v ? MSG0(id, v, "window") : 0;
    if (w) MSG(BOOL, w, "makeFirstResponder:", v);
}
