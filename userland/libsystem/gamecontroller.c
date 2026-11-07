/*
 * mOS libSystem — GameController.framework e CoreHaptics.framework.
 *
 * GameController: os controles vêm do kernel (dev/gamepad.c: HID USB genérico, DualShock 4/DualSense, Xbox 360 e
 * Xbox One/Series) pelo GUI_DEVICE. Um timer de 1/120 s no run loop principal lê o estado, cria/remove os
 * GCController (com GCControllerDidConnect/DidDisconnect/DidBecomeCurrent) e atualiza os elementos do
 * GCExtendedGamepad, chamando os handlers (valueChanged/pressedChanged/touched, do gamepad e do
 * GCPhysicalInputProfile) na handlerQueue — a principal, por padrão. GCKeyboard e GCMouse são alimentados pelos
 * eventos do window server que o AppKit recebe (appkit.c chama cx_gc_keyboard_event/cx_gc_mouse_event).
 *
 * CoreHaptics: CHHapticEngine, CHHapticPattern (eventos, parâmetros, curvas e AHAP em dicionário, arquivo ou
 * dados JSON), players simples e avançados (pausa, seek, loop, taxa). O motor do Mac não vibra
 * (capabilitiesForHardware.supportsHaptics = NÃO, como num Mac de verdade); o motor criado por
 * -[GCDeviceHaptics createEngineWithLocality:] vibra o controle. Eventos de áudio (AudioContinuous, AudioCustom
 * com recurso registrado de WAV) são sintetizados e tocam no mixer do sistema (DEV_AUDIO_PLAY).
 */
#include "foundation.h"
#include "sys.h"
#include "../../kernel/include/compat_gui.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
const char *cx_cstr(id s);
id cx_str(const char *s);
void cx_arr_add(id a, id o);
int strcmp(const char *a, const char *b);
void *malloc(size_t n);
void *calloc(size_t n, size_t m);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
size_t strlen(const char *s);
int memcmp(const void *a, const void *b, size_t n);
int printf(const char *f, ...);
double sin(double), floor(double), fabs(double), pow(double, double);
void *_Block_copy(const void *b);
void _Block_release(const void *b);
void *dispatch_get_main_queue(void);
typedef void (*dispatch_function_t)(void *);
void dispatch_async_f(void *q, void *ctx, dispatch_function_t fn);
extern u8 *cx_read_file(const char *path, u64 *len);
id CFDataCreate(void *alloc, const u8 *bytes, long n);
double cx_now_seconds(void) __attribute__((weak));

static id cls(const char *n) { return (id)cx_class_named(n); }
static BOOL resp(id o, const char *s) { return o && cx_responds(object_getClass(o), sel_registerName(s)); }
static id ret(id o) { return o ? objc_retain(o) : 0; }
static void setobj(id *slot, id v) { if (*slot == v) return; if (v) objc_retain(v); if (*slot) objc_release(*slot); *slot = v; }
static void setblk(void **slot, void *b) { void *n = b ? _Block_copy(b) : 0; if (*slot) _Block_release(*slot); *slot = n; }
static id mdict(void) { return MSG0(id, cls("NSMutableDictionary"), "dictionary"); }
static void dset(id d, id k, id v) { if (d && k && v) MSG(void, d, "setObject:forKey:", v, k); }
static id dget(id d, const char *k) { return d && resp(d, "objectForKey:") ? MSG(id, d, "objectForKey:", cx_str(k)) : 0; }
static double num(id n, double dflt) { return n && resp(n, "doubleValue") ? MSG0(double, n, "doubleValue") : dflt; }
static id nsnum(double v) { return MSG(id, cls("NSNumber"), "numberWithDouble:", v); }
static BOOL streq(id a, id b) { return a == b || (a && b && cx_is_str(a) && cx_is_str(b) && !strcmp(cx_cstr(a), cx_cstr(b))); }
static u64 cnt(id a) { return a && resp(a, "count") ? MSG0(u64, a, "count") : 0; }
static id at(id a, u64 i) { return MSG(id, a, "objectAtIndex:", i); }
static id error_with(const char *domain, i64 code, const char *msg) {
    id info = mdict();
    dset(info, cx_str("NSLocalizedDescription"), cx_str(msg));
    return MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:", cx_str(domain), code, info);
}
static double now_s(void) { return SYS0(SYS_TIME) / 1000.0; }

typedef struct { void *isa; int flags, reserved; void *invoke; } blk_t;
#define BLK(b) ((blk_t *)(b))

/* ================================================================ nomes e notificações (constantes NSString) */
id GCControllerDidConnectNotification, GCControllerDidDisconnectNotification, GCControllerDidBecomeCurrentNotification,
   GCControllerDidStopBeingCurrentNotification, GCControllerUserCustomizationsDidChangeNotification,
   GCKeyboardDidConnectNotification, GCKeyboardDidDisconnectNotification, GCMouseDidConnectNotification,
   GCMouseDidDisconnectNotification, GCMouseDidBecomeCurrentNotification, GCMouseDidStopBeingCurrentNotification;
id GCInputButtonA, GCInputButtonB, GCInputButtonX, GCInputButtonY, GCInputLeftShoulder, GCInputRightShoulder,
   GCInputLeftTrigger, GCInputRightTrigger, GCInputLeftThumbstick, GCInputRightThumbstick,
   GCInputLeftThumbstickButton, GCInputRightThumbstickButton, GCInputDirectionPad, GCInputButtonMenu,
   GCInputButtonOptions, GCInputButtonHome, GCInputDualShockTouchpadButton, GCInputXboxPaddleOne, GCInputXboxPaddleTwo,
   GCInputXboxPaddleThree, GCInputXboxPaddleFour, GCInputButtonShare, GCInputLeftPaddle, GCInputRightPaddle;
id GCProductCategoryDualShock4, GCProductCategoryDualSense, GCProductCategoryXboxOne, GCProductCategoryMFi,
   GCProductCategoryHID, GCProductCategoryKeyboard, GCProductCategoryMouse, GCProductCategoryCoalesced;
id GCHapticsLocalityDefault, GCHapticsLocalityAll, GCHapticsLocalityHandles, GCHapticsLocalityLeftHandle,
   GCHapticsLocalityRightHandle, GCHapticsLocalityTriggers, GCHapticsLocalityLeftTrigger, GCHapticsLocalityRightTrigger;
id GCCurrentExtendedGamepadSnapshotDataVersion;
const float GCHapticDurationInfinite = 3.402823466e+38f;

/* ================================================================ elementos */
enum { EK_BUTTON, EK_AXIS, EK_DPAD };
cx_class_t GCControllerElement_c __asm__("OBJC_CLASS_$_GCControllerElement"), GCControllerElement_m __asm__("OBJC_METACLASS_$_GCControllerElement");
cx_class_t GCControllerButtonInput_c __asm__("OBJC_CLASS_$_GCControllerButtonInput"), GCControllerButtonInput_m __asm__("OBJC_METACLASS_$_GCControllerButtonInput");
cx_class_t GCControllerAxisInput_c __asm__("OBJC_CLASS_$_GCControllerAxisInput"), GCControllerAxisInput_m __asm__("OBJC_METACLASS_$_GCControllerAxisInput");
cx_class_t GCControllerDirectionPad_c __asm__("OBJC_CLASS_$_GCControllerDirectionPad"), GCControllerDirectionPad_m __asm__("OBJC_METACLASS_$_GCControllerDirectionPad");
cx_class_t GCControllerTouchpad_c __asm__("OBJC_CLASS_$_GCControllerTouchpad"), GCControllerTouchpad_m __asm__("OBJC_METACLASS_$_GCControllerTouchpad");
typedef struct {
    void *isa;
    int kind;
    id name, symbol, aliases;
    id collection;                 /* dpad dono de um eixo/botão de direção */
    id profile;                    /* GCExtendedGamepad/GCKeyboardInput/GCMouseInput */
    float value;
    BOOL pressed, touched, analog, bound_low;
    void *value_h, *pressed_h, *touched_h;
    id x, y, up, down, left, right;
    i64 keycode;                   /* GCKeyboardInput */
    int prefers_system;
} gcel_t;
#define EL(o) ((gcel_t *)(o))
static id el_new(cx_class_t *k, int kind, const char *name, const char *symbol, BOOL analog) {
    gcel_t *e = (gcel_t *)cx_alloc_instance(k);
    e->kind = kind;
    e->name = ret(cx_str(name));
    e->symbol = symbol ? ret(cx_str(symbol)) : 0;
    e->analog = analog;
    return (id)e;
}
static id el_name(id s, SEL c) { (void)c; return EL(s)->name; }
static id el_symbol(id s, SEL c) { (void)c; return EL(s)->symbol; }
static void el_set_symbol(id s, SEL c, id v) { (void)c; setobj(&EL(s)->symbol, v); }
static id el_unmapped(id s, SEL c) { (void)c; return EL(s)->symbol; }
static id el_collection(id s, SEL c) { (void)c; return EL(s)->collection; }
static BOOL el_analog(id s, SEL c) { (void)c; return EL(s)->analog; }
static BOOL el_bound(id s, SEL c) { (void)c; return EL(s)->bound_low; }
static id el_aliases(id s, SEL c) {
    (void)c;
    gcel_t *e = EL(s);
    if (!e->aliases) e->aliases = ret(MSG(id, cls("NSSet"), "setWithObject:", e->name));
    return e->aliases;
}
static i64 el_prefers(id s, SEL c) { (void)c; return EL(s)->prefers_system; }
static void el_set_prefers(id s, SEL c, i64 v) { (void)c; EL(s)->prefers_system = (int)v; }
static float el_value(id s, SEL c) { (void)c; return EL(s)->value; }
static BOOL el_pressed(id s, SEL c) { (void)c; return EL(s)->pressed; }
static BOOL el_touched(id s, SEL c) { (void)c; return EL(s)->touched || EL(s)->pressed; }
static void *el_value_h(id s, SEL c) { (void)c; return EL(s)->value_h; }
static void el_set_value_h(id s, SEL c, void *b) { (void)c; setblk(&EL(s)->value_h, b); }
static void *el_pressed_h(id s, SEL c) { (void)c; return EL(s)->pressed_h; }
static void el_set_pressed_h(id s, SEL c, void *b) { (void)c; setblk(&EL(s)->pressed_h, b); }
static void *el_touched_h(id s, SEL c) { (void)c; return EL(s)->touched_h; }
static void el_set_touched_h(id s, SEL c, void *b) { (void)c; setblk(&EL(s)->touched_h, b); }
static id el_x(id s, SEL c) { (void)c; return EL(s)->x; }
static id el_y(id s, SEL c) { (void)c; return EL(s)->y; }
static id el_up(id s, SEL c) { (void)c; return EL(s)->up; }
static id el_down(id s, SEL c) { (void)c; return EL(s)->down; }
static id el_left(id s, SEL c) { (void)c; return EL(s)->left; }
static id el_right(id s, SEL c) { (void)c; return EL(s)->right; }

/* ---- disparo de handlers, na fila do controlador (principal se nenhuma) */
typedef struct { void (*fn)(void *); void *a, *b, *blk; float f1, f2; int i1; i64 l1; } call_t;
static void *g_handler_queue;
static void run_call(void *p) {
    call_t *c = p;
    c->fn(c);
    free(c);
}
static void post_call(call_t *c) {
    void *q = g_handler_queue;
    if (!q || q == dispatch_get_main_queue()) { c->fn(c); return; }   /* c está na pilha de quem chamou */
    call_t *h = malloc(sizeof *h);
    *h = *c;
    dispatch_async_f(q, h, run_call);
}
static void call_button(void *p) { call_t *c = p; ((void (*)(void *, id, float, BOOL))BLK(c->blk)->invoke)(c->blk, (id)c->a, c->f1, (BOOL)c->i1); }
static void call_axis(void *p) { call_t *c = p; ((void (*)(void *, id, float))BLK(c->blk)->invoke)(c->blk, (id)c->a, c->f1); }
static void call_dpad(void *p) { call_t *c = p; ((void (*)(void *, id, float, float))BLK(c->blk)->invoke)(c->blk, (id)c->a, c->f1, c->f2); }
static void call_profile(void *p) { call_t *c = p; ((void (*)(void *, id, id))BLK(c->blk)->invoke)(c->blk, (id)c->a, (id)c->b); }
static void call_key(void *p) { call_t *c = p; ((void (*)(void *, id, id, i64, BOOL))BLK(c->blk)->invoke)(c->blk, (id)c->a, (id)c->b, c->l1, (BOOL)c->i1); }
static void fire_profile(id profile, id element);

/* muda o valor de um botão; devolve SIM se mudou */
static BOOL button_set(id b, float v, float threshold) {
    gcel_t *e = EL(b);
    if (e->value == v) return NO;
    BOOL was = e->pressed;
    e->value = v;
    e->pressed = v > threshold;
    if (e->value_h) { call_t c = {call_button, b, 0, e->value_h, v, 0, e->pressed}; post_call(&c); }
    if (e->pressed != was && e->pressed_h) { call_t c = {call_button, b, 0, e->pressed_h, v, 0, e->pressed}; post_call(&c); }
    if (e->pressed != was && e->touched_h) { call_t c = {call_button, b, 0, e->touched_h, v, 0, e->pressed}; post_call(&c); }
    return YES;
}
static BOOL axis_set(id a, float v) {
    gcel_t *e = EL(a);
    if (e->value == v) return NO;
    e->value = v;
    if (e->value_h) { call_t c = {call_axis, a, 0, e->value_h, v}; post_call(&c); }
    return YES;
}
static BOOL dpad_set(id d, float x, float y) {
    gcel_t *e = EL(d);
    BOOL ch = NO;
    ch |= axis_set(e->x, x);
    ch |= axis_set(e->y, y);
    ch |= button_set(e->up, y > 0 ? y : 0, 0.1f);
    ch |= button_set(e->down, y < 0 ? -y : 0, 0.1f);
    ch |= button_set(e->left, x < 0 ? -x : 0, 0.1f);
    ch |= button_set(e->right, x > 0 ? x : 0, 0.1f);
    if (ch && e->value_h) { call_t c = {call_dpad, d, 0, e->value_h, x, y}; post_call(&c); }
    return ch;
}
static id dpad_new(const char *name, const char *symbol, BOOL analog) {
    id d = el_new(&GCControllerDirectionPad_c, EK_DPAD, name, symbol, analog);
    gcel_t *e = EL(d);
    e->x = el_new(&GCControllerAxisInput_c, EK_AXIS, "X", 0, analog);
    e->y = el_new(&GCControllerAxisInput_c, EK_AXIS, "Y", 0, analog);
    e->up = el_new(&GCControllerButtonInput_c, EK_BUTTON, "Up", 0, analog);
    e->down = el_new(&GCControllerButtonInput_c, EK_BUTTON, "Down", 0, analog);
    e->left = el_new(&GCControllerButtonInput_c, EK_BUTTON, "Left", 0, analog);
    e->right = el_new(&GCControllerButtonInput_c, EK_BUTTON, "Right", 0, analog);
    id kids[6] = {e->x, e->y, e->up, e->down, e->left, e->right};
    for (int i = 0; i < 6; i++) EL(kids[i])->collection = d;
    return d;
}
/* -[GCControllerButtonInput setValue:] e -[GCControllerDirectionPad setValueForXAxis:yAxis:] (controles virtuais/snapshots) */
static void el_set_value(id s, SEL c, float v) {
    (void)c;
    gcel_t *e = EL(s);
    v = v < 0 ? (e->kind == EK_AXIS ? (v < -1 ? -1 : v) : 0) : v > 1 ? 1 : v;
    BOOL ch = e->kind == EK_AXIS ? axis_set(s, v) : button_set(s, v, e->analog ? 0.5f : 0.1f);
    if (ch && e->profile) fire_profile(e->profile, s);
}
static void dpad_set_xy(id s, SEL c, float x, float y) { (void)c; if (dpad_set(s, x, y) && EL(s)->profile) fire_profile(EL(s)->profile, s); }

/* ================================================================ perfis e gamepads */
enum { E_A, E_B, E_X, E_Y, E_LB, E_RB, E_LT, E_RT, E_LSTICK, E_RSTICK, E_LSB, E_RSB, E_MENU, E_OPT, E_HOME, E_DPAD, E_TOUCH, E_COUNT };
cx_class_t GCPhysicalInputProfile_c __asm__("OBJC_CLASS_$_GCPhysicalInputProfile"), GCPhysicalInputProfile_m __asm__("OBJC_METACLASS_$_GCPhysicalInputProfile");
cx_class_t GCExtendedGamepad_c __asm__("OBJC_CLASS_$_GCExtendedGamepad"), GCExtendedGamepad_m __asm__("OBJC_METACLASS_$_GCExtendedGamepad");
cx_class_t GCMicroGamepad_c __asm__("OBJC_CLASS_$_GCMicroGamepad"), GCMicroGamepad_m __asm__("OBJC_METACLASS_$_GCMicroGamepad");
cx_class_t GCDualShockGamepad_c __asm__("OBJC_CLASS_$_GCDualShockGamepad"), GCDualShockGamepad_m __asm__("OBJC_METACLASS_$_GCDualShockGamepad");
cx_class_t GCDualSenseGamepad_c __asm__("OBJC_CLASS_$_GCDualSenseGamepad"), GCDualSenseGamepad_m __asm__("OBJC_METACLASS_$_GCDualSenseGamepad");
cx_class_t GCXboxGamepad_c __asm__("OBJC_CLASS_$_GCXboxGamepad"), GCXboxGamepad_m __asm__("OBJC_METACLASS_$_GCXboxGamepad");
cx_class_t GCController_c __asm__("OBJC_CLASS_$_GCController"), GCController_m __asm__("OBJC_METACLASS_$_GCController");
typedef struct {
    void *isa;
    id controller;
    id el[E_COUNT];
    void *value_h, *did_change_h;
    double ts;
    id buttons, axes, dpads, elements, touchpads, all_buttons, all_axes, all_dpads;
    BOOL micro_abs, micro_rot;
    id micro_dpad, micro_a, micro_x, micro_menu;   /* GCMicroGamepad: aponta para os do estendido */
    BOOL is_micro;
} gcprof_t;
#define PROF(o) ((gcprof_t *)(o))
static void fire_profile(id profile, id element) {
    gcprof_t *p = PROF(profile);
    p->ts = now_s();
    if (p->value_h) { call_t c = {call_profile, profile, element, p->value_h}; post_call(&c); }
    if (p->did_change_h) { call_t c = {call_profile, profile, element, p->did_change_h}; post_call(&c); }
}
static id profile_new(cx_class_t *k, id controller, BOOL sony) {
    gcprof_t *p = (gcprof_t *)cx_alloc_instance(k);
    p->controller = controller;
    struct { int i; const char *name, *sym; BOOL analog; } b[] = {
        {E_A, "Button A", sony ? "xmark.circle" : "a.circle", NO}, {E_B, "Button B", sony ? "circle.circle" : "b.circle", NO},
        {E_X, "Button X", sony ? "square.circle" : "x.circle", NO}, {E_Y, "Button Y", sony ? "triangle.circle" : "y.circle", NO},
        {E_LB, "Left Shoulder", "lb.rectangle.roundedbottom", NO}, {E_RB, "Right Shoulder", "rb.rectangle.roundedbottom", NO},
        {E_LT, "Left Trigger", "lt.rectangle.roundedtop", YES}, {E_RT, "Right Trigger", "rt.rectangle.roundedtop", YES},
        {E_LSB, "Left Thumbstick Button", "l.joystick.press.down", NO}, {E_RSB, "Right Thumbstick Button", "r.joystick.press.down", NO},
        {E_MENU, "Button Menu", "line.horizontal.3.circle", NO}, {E_OPT, "Button Options", "rectangle.fill.on.rectangle.fill.circle", NO},
        {E_HOME, "Button Home", "logo.playstation", NO}, {E_TOUCH, "Touchpad Button", "touchpad.button", NO},
    };
    for (size_t i = 0; i < sizeof b / sizeof *b; i++) {
        p->el[b[i].i] = el_new(&GCControllerButtonInput_c, EK_BUTTON, b[i].name, b[i].sym, b[i].analog);
        EL(p->el[b[i].i])->bound_low = b[i].analog;
    }
    p->el[E_LSTICK] = dpad_new("Left Thumbstick", "l.joystick", YES);
    p->el[E_RSTICK] = dpad_new("Right Thumbstick", "r.joystick", YES);
    p->el[E_DPAD] = dpad_new("Direction Pad", "dpad", NO);
    p->buttons = ret(mdict()); p->axes = ret(mdict()); p->dpads = ret(mdict()); p->elements = ret(mdict()); p->touchpads = ret(mdict());
    for (int i = 0; i < E_COUNT; i++) {
        id e = p->el[i];
        if (!e) continue;
        EL(e)->profile = (id)p;
        if (EL(e)->kind == EK_DPAD) {
            gcel_t *d = EL(e);
            id kids[6] = {d->x, d->y, d->up, d->down, d->left, d->right};
            for (int k = 0; k < 6; k++) EL(kids[k])->profile = (id)p;
            dset(p->dpads, EL(e)->name, e);
        } else dset(p->buttons, EL(e)->name, e);
        dset(p->elements, EL(e)->name, e);
    }
    p->micro_dpad = p->el[E_DPAD]; p->micro_a = p->el[E_A]; p->micro_x = p->el[E_X]; p->micro_menu = p->el[E_MENU];
    return (id)p;
}
#define PGET(name, idx) static id name(id s, SEL c) { (void)c; return PROF(s)->el[idx]; }
PGET(pr_a, E_A) PGET(pr_b, E_B) PGET(pr_x, E_X) PGET(pr_y, E_Y) PGET(pr_lb, E_LB) PGET(pr_rb, E_RB) PGET(pr_lt, E_LT) PGET(pr_rt, E_RT)
PGET(pr_ls, E_LSTICK) PGET(pr_rs, E_RSTICK) PGET(pr_lsb, E_LSB) PGET(pr_rsb, E_RSB) PGET(pr_menu, E_MENU) PGET(pr_opt, E_OPT)
PGET(pr_home, E_HOME) PGET(pr_dpad, E_DPAD) PGET(pr_touch, E_TOUCH)
static id pr_nil(id s, SEL c) { (void)s; (void)c; return 0; }
static id pr_controller(id s, SEL c) { (void)c; return PROF(s)->controller; }
static id pr_device(id s, SEL c) { (void)c; return PROF(s)->controller; }
static void *pr_value_h(id s, SEL c) { (void)c; return PROF(s)->value_h; }
static void pr_set_value_h(id s, SEL c, void *b) { (void)c; setblk(&PROF(s)->value_h, b); }
static void *pr_change_h(id s, SEL c) { (void)c; return PROF(s)->did_change_h; }
static void pr_set_change_h(id s, SEL c, void *b) { (void)c; setblk(&PROF(s)->did_change_h, b); }
static double pr_ts(id s, SEL c) { (void)c; return PROF(s)->ts; }
static id pr_buttons(id s, SEL c) { (void)c; return PROF(s)->buttons; }
static id pr_axes(id s, SEL c) { (void)c; return PROF(s)->axes; }
static id pr_dpads(id s, SEL c) { (void)c; return PROF(s)->dpads; }
static id pr_elements(id s, SEL c) { (void)c; return PROF(s)->elements; }
static id pr_touchpads(id s, SEL c) { (void)c; return PROF(s)->touchpads; }
static id pr_set_of(id d) { return MSG(id, cls("NSSet"), "setWithArray:", MSG0(id, d, "allValues")); }
static id pr_all_buttons(id s, SEL c) { (void)c; return pr_set_of(PROF(s)->buttons); }
static id pr_all_axes(id s, SEL c) { (void)c; return pr_set_of(PROF(s)->axes); }
static id pr_all_dpads(id s, SEL c) { (void)c; return pr_set_of(PROF(s)->dpads); }
static id pr_all_elements(id s, SEL c) { (void)c; return pr_set_of(PROF(s)->elements); }
static id pr_subscript(id s, SEL c, id key) { (void)c; return key ? MSG(id, PROF(s)->elements, "objectForKey:", key) : 0; }
static id pr_self(id s, SEL c) { (void)c; return s; }
static BOOL pr_micro_abs(id s, SEL c) { (void)c; return PROF(s)->micro_abs; }
static void pr_set_micro_abs(id s, SEL c, BOOL b) { (void)c; PROF(s)->micro_abs = b; }
static BOOL pr_micro_rot(id s, SEL c) { (void)c; return PROF(s)->micro_rot; }
static void pr_set_micro_rot(id s, SEL c, BOOL b) { (void)c; PROF(s)->micro_rot = b; }
static id pr_capture(id s, SEL c) { (void)c; return s; }
/* -[GCExtendedGamepad setStateFromExtendedGamepad:] */
static void pr_set_state(id s, SEL c, id other) {
    (void)c;
    if (!other || !object_getClass(other) || !cx_is_subclass(object_getClass(other), &GCPhysicalInputProfile_c)) return;
    gcprof_t *a = PROF(s), *b = PROF(other);
    for (int i = 0; i < E_COUNT; i++) {
        if (!a->el[i] || !b->el[i]) continue;
        if (EL(a->el[i])->kind == EK_DPAD) dpad_set(a->el[i], EL(EL(b->el[i])->x)->value, EL(EL(b->el[i])->y)->value);
        else button_set(a->el[i], EL(b->el[i])->value, EL(a->el[i])->analog ? 0.5f : 0.1f);
    }
}

/* ================================================================ GCController */
typedef struct {
    void *isa;
    int slot;                      /* índice do kernel; -1 = virtual/snapshot */
    id vendor, category;
    i64 player;
    id ext, micro, haptics, light, battery, motion;
    void *queue, *paused_h;
    u32 seq;
    BOOL snapshot, connected;
    u32 caps, kind;
} gcctl_t;
#define CTL(o) ((gcctl_t *)(o))
static id g_controllers;           /* NSMutableArray dos conectados */
static id g_current;
static id ctl_vendor(id s, SEL c) { (void)c; return CTL(s)->vendor; }
static id ctl_category(id s, SEL c) { (void)c; return CTL(s)->category; }
static i64 ctl_player(id s, SEL c) { (void)c; return CTL(s)->player; }
static void ctl_set_player(id s, SEL c, i64 p) {
    (void)c;
    CTL(s)->player = p;
    /* Xbox 360: o anel de luz mostra o jogador (o kernel acende ao conectar; aqui só guardamos) */
}
static id ctl_ext(id s, SEL c) { (void)c; return CTL(s)->ext; }
static id ctl_micro(id s, SEL c) { (void)c; return CTL(s)->micro; }
static id ctl_nil(id s, SEL c) { (void)s; (void)c; return 0; }
static id ctl_haptics(id s, SEL c) { (void)c; return CTL(s)->haptics; }
static id ctl_light(id s, SEL c) { (void)c; return CTL(s)->light; }
static id ctl_battery(id s, SEL c) { (void)c; return CTL(s)->battery; }
static id ctl_profile(id s, SEL c) { (void)c; return CTL(s)->ext; }
static BOOL ctl_attached(id s, SEL c) { (void)s; (void)c; return NO; }
static BOOL ctl_snapshot(id s, SEL c) { (void)c; return CTL(s)->snapshot; }
static void *ctl_queue(id s, SEL c) { (void)c; return CTL(s)->queue ? CTL(s)->queue : dispatch_get_main_queue(); }
static void ctl_set_queue(id s, SEL c, void *q) { (void)c; CTL(s)->queue = q; g_handler_queue = q; }
static void *ctl_paused(id s, SEL c) { (void)c; return CTL(s)->paused_h; }
static void ctl_set_paused(id s, SEL c, void *b) { (void)c; setblk(&CTL(s)->paused_h, b); }
static id ctl_capture(id s, SEL c);
static id ctl_controllers(id k, SEL c);
static id ctl_current(id k, SEL c) { (void)k; (void)c; ctl_controllers(0, 0); return g_current; }

cx_class_t GCDeviceHaptics_c __asm__("OBJC_CLASS_$_GCDeviceHaptics"), GCDeviceHaptics_m __asm__("OBJC_METACLASS_$_GCDeviceHaptics");
cx_class_t GCDeviceLight_c __asm__("OBJC_CLASS_$_GCDeviceLight"), GCDeviceLight_m __asm__("OBJC_METACLASS_$_GCDeviceLight");
cx_class_t GCDeviceBattery_c __asm__("OBJC_CLASS_$_GCDeviceBattery"), GCDeviceBattery_m __asm__("OBJC_METACLASS_$_GCDeviceBattery");
cx_class_t GCColor_c __asm__("OBJC_CLASS_$_GCColor"), GCColor_m __asm__("OBJC_METACLASS_$_GCColor");
typedef struct { void *isa; id controller; } gchap_t;
typedef struct { void *isa; id color; } gclight_t;
typedef struct { void *isa; float r, g, b; } gccolor_t;
static id col_init(id s, SEL c, float r, float g, float b) { (void)c; gccolor_t *o = (gccolor_t *)s; o->r = r; o->g = g; o->b = b; return s; }
static float col_r(id s, SEL c) { (void)c; return ((gccolor_t *)s)->r; }
static float col_g(id s, SEL c) { (void)c; return ((gccolor_t *)s)->g; }
static float col_b(id s, SEL c) { (void)c; return ((gccolor_t *)s)->b; }
static id light_color(id s, SEL c) { (void)c; return ((gclight_t *)s)->color; }
static void light_set_color(id s, SEL c, id v) { (void)c; setobj(&((gclight_t *)s)->color, v); }
static float bat_level(id s, SEL c) { (void)s; (void)c; return 1.0f; }
static i64 bat_state(id s, SEL c) { (void)s; (void)c; return 2; }       /* GCDeviceBatteryStateFull: alimentado pelo USB */
static id hap_localities(id s, SEL c) {
    (void)s; (void)c;
    id a = MSG0(id, cls("NSMutableSet"), "set");
    id l[4] = {GCHapticsLocalityDefault, GCHapticsLocalityAll, GCHapticsLocalityHandles, GCHapticsLocalityLeftHandle};
    for (int i = 0; i < 4; i++) MSG(void, a, "addObject:", l[i]);
    MSG(void, a, "addObject:", GCHapticsLocalityRightHandle);
    return a;
}
id cx_haptic_engine_for_pad(int slot, id locality);      /* CoreHaptics, abaixo */
static id hap_create_engine(id s, SEL c, id locality) {
    (void)c;
    id ctl = ((gchap_t *)s)->controller;
    return ctl ? cx_haptic_engine_for_pad(CTL(ctl)->slot, locality) : 0;
}

static const char *category_for(const gui_gamepad_t *p) {
    if (p->kind == PAD_KIND_SONY) return p->pid == 0x0CE6 || p->pid == 0x0DF2 ? "DualSense" : "DualShock 4";
    if (p->kind == PAD_KIND_XBOXONE || p->kind == PAD_KIND_XBOX360) return "Xbox One";
    return "HID";
}
static id ctl_make(int slot, const gui_gamepad_t *p) {
    gcctl_t *c = (gcctl_t *)cx_alloc_instance(&GCController_c);
    c->slot = slot;
    c->player = -1;                                    /* GCControllerPlayerIndexUnset */
    c->vendor = ret(cx_str(p ? p->name : "Controle"));
    c->category = ret(cx_str(p ? category_for(p) : "MFi"));
    c->kind = p ? p->kind : 0;
    c->caps = p ? p->caps : 0;
    BOOL sony = p && p->kind == PAD_KIND_SONY;
    cx_class_t *k = sony ? (p->pid == 0x0CE6 || p->pid == 0x0DF2 ? &GCDualSenseGamepad_c : &GCDualShockGamepad_c)
                         : p && (p->kind == PAD_KIND_XBOXONE || p->kind == PAD_KIND_XBOX360) ? &GCXboxGamepad_c : &GCExtendedGamepad_c;
    c->ext = profile_new(k, (id)c, sony);
    c->micro = profile_new(&GCMicroGamepad_c, (id)c, sony);
    PROF(c->micro)->is_micro = YES;
    if (c->caps & PAD_CAP_RUMBLE) { c->haptics = cx_alloc_instance(&GCDeviceHaptics_c); ((gchap_t *)c->haptics)->controller = (id)c; }
    if (sony) {
        c->light = cx_alloc_instance(&GCDeviceLight_c);
        ((gclight_t *)c->light)->color = ret(col_init(cx_alloc_instance(&GCColor_c), 0, 0, 0, 1));
    }
    c->battery = cx_alloc_instance(&GCDeviceBattery_c);
    c->connected = YES;
    return (id)c;
}
/* estado do kernel → elementos; dispara os handlers do que mudou */
static void ctl_apply(id ctl, const gui_gamepad_t *p) {
    gcctl_t *c = CTL(ctl);
    gcprof_t *x = PROF(c->ext);
    BOOL menu_was = EL(x->el[E_MENU])->pressed;
    struct { int i; u32 bit; } map[] = {
        {E_A, PAD_A}, {E_B, PAD_B}, {E_X, PAD_X}, {E_Y, PAD_Y}, {E_LB, PAD_LB}, {E_RB, PAD_RB}, {E_LSB, PAD_LS}, {E_RSB, PAD_RS},
        {E_MENU, PAD_MENU}, {E_OPT, PAD_OPTIONS}, {E_HOME, PAD_HOME}, {E_TOUCH, PAD_TOUCHPAD},
    };
    for (size_t i = 0; i < sizeof map / sizeof *map; i++) {
        float v = (p->buttons & map[i].bit) ? 1.0f : 0.0f;
        if (button_set(x->el[map[i].i], v, 0.5f)) fire_profile(c->ext, x->el[map[i].i]);
    }
    if (button_set(x->el[E_LT], p->lt, 0.1f)) fire_profile(c->ext, x->el[E_LT]);
    if (button_set(x->el[E_RT], p->rt, 0.1f)) fire_profile(c->ext, x->el[E_RT]);
    if (dpad_set(x->el[E_LSTICK], p->lx, p->ly)) fire_profile(c->ext, x->el[E_LSTICK]);
    if (dpad_set(x->el[E_RSTICK], p->rx, p->ry)) fire_profile(c->ext, x->el[E_RSTICK]);
    float dx = (p->buttons & PAD_RIGHT) ? 1 : (p->buttons & PAD_LEFT) ? -1 : 0;
    float dy = (p->buttons & PAD_UP) ? 1 : (p->buttons & PAD_DOWN) ? -1 : 0;
    if (dpad_set(x->el[E_DPAD], dx, dy)) fire_profile(c->ext, x->el[E_DPAD]);
    /* GCMicroGamepad: direcional + A/X/Menu */
    gcprof_t *m = PROF(c->micro);
    if (dpad_set(m->el[E_DPAD], dx ? dx : p->lx, dy ? dy : p->ly)) fire_profile(c->micro, m->el[E_DPAD]);
    if (button_set(m->el[E_A], (p->buttons & PAD_A) ? 1 : 0, 0.5f)) fire_profile(c->micro, m->el[E_A]);
    if (button_set(m->el[E_X], (p->buttons & PAD_X) ? 1 : 0, 0.5f)) fire_profile(c->micro, m->el[E_X]);
    if (button_set(m->el[E_MENU], (p->buttons & PAD_MENU) ? 1 : 0, 0.5f)) fire_profile(c->micro, m->el[E_MENU]);
    /* o botão Menu também chama o controllerPausedHandler antigo */
    if (!menu_was && (p->buttons & PAD_MENU) && c->paused_h) {
        ((void (*)(void *, id))BLK(c->paused_h)->invoke)(c->paused_h, ctl);
    }
    c->seq = p->seq;
}
static void post_note(id name, id obj) { MSG(void, MSG0(id, cls("NSNotificationCenter"), "defaultCenter"), "postNotificationName:object:", name, obj); }
static void make_current(id ctl) {
    if (g_current == ctl) return;
    id old = g_current;
    g_current = ctl;
    if (old) post_note(GCControllerDidStopBeingCurrentNotification, old);
    if (ctl) post_note(GCControllerDidBecomeCurrentNotification, ctl);
}

/* ---- varredura periódica */
static id g_poller;
static BOOL g_first_scan_done;
static void poll_pads(void) {
    if (!g_controllers) g_controllers = ret(MSG0(id, cls("NSMutableArray"), "array"));
    for (int i = 0; i < 4; i++) {
        gui_gamepad_t p;
        memset(&p, 0, sizeof p);
        if (SYS3(GUI_DEVICE, DEV_PAD_GET, i, &p) != 1) break;
        id have = 0;
        for (u64 k = 0; k < cnt(g_controllers); k++) if (CTL(at(g_controllers, k))->slot == i) have = at(g_controllers, k);
        if (p.connected && !have) {
            id c = ctl_make(i, &p);
            MSG(void, g_controllers, "addObject:", c);
            objc_release(c);
            post_note(GCControllerDidConnectNotification, c);
            if (!g_current) make_current(c);
            ctl_apply(c, &p);
        } else if (!p.connected && have) {
            objc_retain(have);
            CTL(have)->connected = NO;
            MSG(void, g_controllers, "removeObject:", have);
            if (g_current == have) make_current(cnt(g_controllers) ? at(g_controllers, 0) : 0);
            post_note(GCControllerDidDisconnectNotification, have);
            objc_release(have);
        } else if (have && p.seq != CTL(have)->seq) {
            ctl_apply(have, &p);
            if (g_current != have && (p.buttons || p.lt > 0.1f || p.rt > 0.1f)) make_current(have);   /* o último usado vira o atual */
        }
    }
    g_first_scan_done = YES;
}
static void poller_tick(id s, SEL c, id t) { (void)s; (void)c; (void)t; poll_pads(); }
cx_class_t GCMOSPoller_c __asm__("OBJC_CLASS_$_GCMOSPoller"), GCMOSPoller_m __asm__("OBJC_METACLASS_$_GCMOSPoller");
static void start_poller(void) {
    if (g_poller) return;
    g_poller = cx_alloc_instance(&GCMOSPoller_c);
    MSG(id, cls("NSTimer"), "scheduledTimerWithTimeInterval:target:selector:userInfo:repeats:", 1.0 / 120, g_poller,
        sel_registerName("tick:"), (id)0, YES);
}
/* gancho do AppKit (ap_finish_launching): os controles já ligados aparecem logo depois do launch */
void cx_gc_app_launched(void) { start_poller(); }
static id ctl_controllers(id k, SEL c) {
    (void)k; (void)c;
    start_poller();
    if (!g_first_scan_done) poll_pads();
    return MSG(id, cls("NSArray"), "arrayWithArray:", g_controllers);
}
static void ctl_discovery(id k, SEL c, void *done) {
    (void)k; (void)c;
    start_poller();
    poll_pads();
    if (done) ((void (*)(void *))BLK(done)->invoke)(done);   /* sem controles sem fio para descobrir: termina já */
}
static void ctl_stop_discovery(id k, SEL c) { (void)k; (void)c; }
static BOOL ctl_should_bg(id k, SEL c) { (void)k; (void)c; return NO; }
static void ctl_set_should_bg(id k, SEL c, BOOL b) { (void)k; (void)c; (void)b; }
static BOOL ctl_supports_hid(id k, SEL c, id dev) { (void)k; (void)c; (void)dev; return NO; }
static id ctl_with_ext(id k, SEL c) {
    (void)k; (void)c;
    id ctl = ctl_make(-1, 0);
    CTL(ctl)->snapshot = YES;
    return objc_autorelease(ctl);
}
static id ctl_capture(id s, SEL c) {
    (void)c;
    id ctl = ctl_make(-1, 0);
    CTL(ctl)->snapshot = YES;
    setobj(&CTL(ctl)->vendor, CTL(s)->vendor);
    setobj(&CTL(ctl)->category, CTL(s)->category);
    pr_set_state(CTL(ctl)->ext, 0, CTL(s)->ext);
    return objc_autorelease(ctl);
}

/* ================================================================ GCKeyboard e GCMouse */
cx_class_t GCKeyboard_c __asm__("OBJC_CLASS_$_GCKeyboard"), GCKeyboard_m __asm__("OBJC_METACLASS_$_GCKeyboard");
cx_class_t GCKeyboardInput_c __asm__("OBJC_CLASS_$_GCKeyboardInput"), GCKeyboardInput_m __asm__("OBJC_METACLASS_$_GCKeyboardInput");
cx_class_t GCMouse_c __asm__("OBJC_CLASS_$_GCMouse"), GCMouse_m __asm__("OBJC_METACLASS_$_GCMouse");
cx_class_t GCMouseInput_c __asm__("OBJC_CLASS_$_GCMouseInput"), GCMouseInput_m __asm__("OBJC_METACLASS_$_GCMouseInput");
typedef struct { void *isa; id input; id vendor; void *queue; } gcdev_t;
typedef struct {
    void *isa;
    id keys;                        /* NSMutableDictionary keyCode → botão */
    void *key_h, *value_h;
    int pressed_count;
    id device;
} gckbd_t;
typedef struct {
    void *isa;
    id left, right, middle, scroll, aux;
    void *moved_h, *value_h;
    id device;
    int lx, ly;
    BOOL have_last;
} gcmouse_t;
static id g_keyboard, g_mouse;
/* keyCode do mac (kVK_*) → uso HID (GCKeyCode) */
static i64 hid_from_mac(u16 k) {
    static const u8 t[128] = {
        0x04, 0x16, 0x07, 0x09, 0x0B, 0x0A, 0x1D, 0x1B, 0x06, 0x19, 0x64, 0x05, 0x14, 0x1A, 0x08, 0x15,   /* 0x00 a s d f h g z x c v § b q w e r */
        0x1C, 0x17, 0x1E, 0x1F, 0x20, 0x21, 0x23, 0x22, 0x2E, 0x26, 0x24, 0x2D, 0x25, 0x27, 0x30, 0x12,   /* 0x10 y t 1 2 3 4 6 5 = 9 7 - 8 0 ] o */
        0x18, 0x2F, 0x0C, 0x13, 0x28, 0x0F, 0x0D, 0x34, 0x0E, 0x33, 0x31, 0x36, 0x38, 0x11, 0x10, 0x37,   /* 0x20 u [ i p ret l j ' k ; \ , / n m . */
        0x2B, 0x2C, 0x35, 0x2A, 0x00, 0x29, 0x00, 0xE3, 0xE1, 0x39, 0xE2, 0xE0, 0xE5, 0xE6, 0xE4, 0x00,   /* 0x30 tab spc ` del . esc . cmd shift caps opt ctrl rshift ropt rctrl */
        0x6C, 0x63, 0x00, 0x55, 0x00, 0x57, 0x00, 0x53, 0x80, 0x81, 0x7F, 0x54, 0x58, 0x00, 0x56, 0x6D,
        0x6E, 0x67, 0x62, 0x59, 0x5A, 0x5B, 0x5C, 0x5D, 0x5E, 0x5F, 0x6F, 0x60, 0x61, 0x00, 0x00, 0x00,
        0x3E, 0x3F, 0x40, 0x3D, 0x41, 0x42, 0x00, 0x44, 0x00, 0x68, 0x6B, 0x69, 0x00, 0x43, 0x00, 0x45,   /* 0x60 F5 F6 F7 F3 F8 F9 . F11 */
        0x00, 0x6A, 0x75, 0x4A, 0x4B, 0x4C, 0x3C, 0x4D, 0x3B, 0x4E, 0x3A, 0x50, 0x4F, 0x51, 0x52, 0x00,   /* 0x70 . F15 help home pgup fwddel F4 end F2 pgdn F1 left right down up */
    };
    return k < 128 ? t[k] : 0;
}
static id kbd_button(gckbd_t *k, i64 code) {
    id key = MSG(id, cls("NSNumber"), "numberWithLongLong:", code);
    id b = MSG(id, k->keys, "objectForKey:", key);
    if (!b) {
        char n[24];
        snprintf(n, sizeof n, "Key %lld", (long long)code);
        b = el_new(&GCControllerButtonInput_c, EK_BUTTON, n, "keyboard", NO);
        EL(b)->keycode = code;
        EL(b)->profile = 0;
        MSG(void, k->keys, "setObject:forKey:", b, key);
        objc_release(b);
    }
    return b;
}
static void ensure_keyboard(void) {
    if (g_keyboard) return;
    gcdev_t *d = (gcdev_t *)cx_alloc_instance(&GCKeyboard_c);
    gckbd_t *in = (gckbd_t *)cx_alloc_instance(&GCKeyboardInput_c);
    in->keys = ret(mdict());
    in->device = (id)d;
    d->input = (id)in;
    d->vendor = ret(cx_str("Teclado"));
    g_keyboard = (id)d;
}
static void ensure_mouse(void) {
    if (g_mouse) return;
    gcdev_t *d = (gcdev_t *)cx_alloc_instance(&GCMouse_c);
    gcmouse_t *in = (gcmouse_t *)cx_alloc_instance(&GCMouseInput_c);
    in->left = el_new(&GCControllerButtonInput_c, EK_BUTTON, "Left Button", "computermouse", NO);
    in->right = el_new(&GCControllerButtonInput_c, EK_BUTTON, "Right Button", "computermouse", NO);
    in->middle = el_new(&GCControllerButtonInput_c, EK_BUTTON, "Middle Button", "computermouse", NO);
    in->scroll = dpad_new("Scroll", "computermouse", YES);
    in->device = (id)d;
    d->input = (id)in;
    d->vendor = ret(cx_str("Mouse"));
    g_mouse = (id)d;
}
void cx_gc_keyboard_event(u16 mac_keycode, u64 mods, int down) {
    (void)mods;
    if (!g_keyboard) return;                          /* ninguém pediu o GCKeyboard: nada a fazer */
    gckbd_t *k = (gckbd_t *)((gcdev_t *)g_keyboard)->input;
    i64 code = hid_from_mac(mac_keycode);
    if (!code) return;
    id b = kbd_button(k, code);
    BOOL was = EL(b)->pressed;
    if (button_set(b, down ? 1 : 0, 0.5f) && was != EL(b)->pressed) {
        k->pressed_count += down ? 1 : -1;
        if (k->pressed_count < 0) k->pressed_count = 0;
        if (k->key_h) { call_t c = {call_key, (void *)k, b, k->key_h, 0, 0, down, code}; post_call(&c); }
        if (k->value_h) { call_t c = {call_profile, (void *)k, b, k->value_h}; post_call(&c); }
    }
}
void cx_gc_mouse_event(int type, int x, int y, int buttons, int scroll) {
    if (!g_mouse) return;
    gcmouse_t *m = (gcmouse_t *)((gcdev_t *)g_mouse)->input;
    if (type == GEV_MOUSE_MOVE) {
        if (m->have_last && m->moved_h) {
            float dx = (float)(x - m->lx), dy = (float)(m->ly - y);
            ((void (*)(void *, id, float, float))BLK(m->moved_h)->invoke)(m->moved_h, (id)m, dx, dy);
        }
        m->lx = x; m->ly = y; m->have_last = YES;
    }
    if (type == GEV_MOUSE_DOWN) button_set(m->left, 1, 0.5f);
    if (type == GEV_MOUSE_UP) { button_set(m->left, 0, 0.5f); button_set(m->right, 0, 0.5f); }
    if (type == GEV_RIGHT_DOWN) button_set(m->right, 1, 0.5f);
    if (type == GEV_SCROLL) { dpad_set(m->scroll, 0, (float)-scroll); dpad_set(m->scroll, 0, 0); }
    (void)buttons;
}
static id kb_coalesced(id k, SEL c) { (void)k; (void)c; ensure_keyboard(); return g_keyboard; }
static id ms_current(id k, SEL c) { (void)k; (void)c; ensure_mouse(); return g_mouse; }
static id ms_mice(id k, SEL c) { (void)k; (void)c; ensure_mouse(); return MSG(id, cls("NSArray"), "arrayWithObject:", g_mouse); }
static id dev_input(id s, SEL c) { (void)c; return ((gcdev_t *)s)->input; }
static id dev_vendor(id s, SEL c) { (void)c; return ((gcdev_t *)s)->vendor; }
static id dev_category_kbd(id s, SEL c) { (void)s; (void)c; return GCProductCategoryKeyboard; }
static id dev_category_mouse(id s, SEL c) { (void)s; (void)c; return GCProductCategoryMouse; }
static void *dev_queue(id s, SEL c) { (void)c; return ((gcdev_t *)s)->queue ? ((gcdev_t *)s)->queue : dispatch_get_main_queue(); }
static void dev_set_queue(id s, SEL c, void *q) { (void)c; ((gcdev_t *)s)->queue = q; }
static void *ki_key_h(id s, SEL c) { (void)c; return ((gckbd_t *)s)->key_h; }
static void ki_set_key_h(id s, SEL c, void *b) { (void)c; setblk(&((gckbd_t *)s)->key_h, b); }
static void *ki_value_h(id s, SEL c) { (void)c; return ((gckbd_t *)s)->value_h; }
static void ki_set_value_h(id s, SEL c, void *b) { (void)c; setblk(&((gckbd_t *)s)->value_h, b); }
static BOOL ki_any(id s, SEL c) { (void)c; return ((gckbd_t *)s)->pressed_count > 0; }
static id ki_button(id s, SEL c, i64 code) { (void)c; return kbd_button((gckbd_t *)s, code); }
static id ki_device(id s, SEL c) { (void)c; return ((gckbd_t *)s)->device; }
static id mi_left(id s, SEL c) { (void)c; return ((gcmouse_t *)s)->left; }
static id mi_right(id s, SEL c) { (void)c; return ((gcmouse_t *)s)->right; }
static id mi_middle(id s, SEL c) { (void)c; return ((gcmouse_t *)s)->middle; }
static id mi_scroll(id s, SEL c) { (void)c; return ((gcmouse_t *)s)->scroll; }
static id mi_aux(id s, SEL c) { (void)s; (void)c; return MSG0(id, cls("NSArray"), "array"); }
static void *mi_moved_h(id s, SEL c) { (void)c; return ((gcmouse_t *)s)->moved_h; }
static void mi_set_moved_h(id s, SEL c, void *b) { (void)c; setblk(&((gcmouse_t *)s)->moved_h, b); }
static id mi_device(id s, SEL c) { (void)c; return ((gcmouse_t *)s)->device; }

/* ================================================================ CoreHaptics */
id CHHapticEventTypeHapticTransient, CHHapticEventTypeHapticContinuous, CHHapticEventTypeAudioContinuous, CHHapticEventTypeAudioCustom;
id CHHapticEventParameterIDHapticIntensity, CHHapticEventParameterIDHapticSharpness, CHHapticEventParameterIDAttackTime,
   CHHapticEventParameterIDDecayTime, CHHapticEventParameterIDReleaseTime, CHHapticEventParameterIDSustained,
   CHHapticEventParameterIDAudioVolume, CHHapticEventParameterIDAudioPitch, CHHapticEventParameterIDAudioPan,
   CHHapticEventParameterIDAudioBrightness;
id CHHapticDynamicParameterIDHapticIntensityControl, CHHapticDynamicParameterIDHapticSharpnessControl,
   CHHapticDynamicParameterIDHapticAttackTimeControl, CHHapticDynamicParameterIDHapticDecayTimeControl,
   CHHapticDynamicParameterIDHapticReleaseTimeControl, CHHapticDynamicParameterIDAudioVolumeControl,
   CHHapticDynamicParameterIDAudioPanControl, CHHapticDynamicParameterIDAudioBrightnessControl,
   CHHapticDynamicParameterIDAudioPitchControl, CHHapticDynamicParameterIDAudioAttackTimeControl,
   CHHapticDynamicParameterIDAudioDecayTimeControl, CHHapticDynamicParameterIDAudioReleaseTimeControl;
id CHHapticPatternKeyVersion, CHHapticPatternKeyPattern, CHHapticPatternKeyEvent, CHHapticPatternKeyEventType,
   CHHapticPatternKeyTime, CHHapticPatternKeyEventDuration, CHHapticPatternKeyEventWaveformPath,
   CHHapticPatternKeyEventParameters, CHHapticPatternKeyParameter, CHHapticPatternKeyParameterID,
   CHHapticPatternKeyParameterValue, CHHapticPatternKeyParameterCurve, CHHapticPatternKeyParameterCurveControlPoints,
   CHHapticPatternKeyEventWaveformUseVolumeEnvelope, CHHapticPatternKeyEventWaveformLoopEnabled;
id CHHapticAudioResourceKeyUseVolumeEnvelope, CHHapticAudioResourceKeyLoopEnabled;
id CHHapticErrorDomain;
const double CHHapticTimeImmediate = 0.0;

cx_class_t CHHapticEngine_c __asm__("OBJC_CLASS_$_CHHapticEngine"), CHHapticEngine_m __asm__("OBJC_METACLASS_$_CHHapticEngine");
cx_class_t CHHapticEvent_c __asm__("OBJC_CLASS_$_CHHapticEvent"), CHHapticEvent_m __asm__("OBJC_METACLASS_$_CHHapticEvent");
cx_class_t CHHapticEventParameter_c __asm__("OBJC_CLASS_$_CHHapticEventParameter"), CHHapticEventParameter_m __asm__("OBJC_METACLASS_$_CHHapticEventParameter");
cx_class_t CHHapticDynamicParameter_c __asm__("OBJC_CLASS_$_CHHapticDynamicParameter"), CHHapticDynamicParameter_m __asm__("OBJC_METACLASS_$_CHHapticDynamicParameter");
cx_class_t CHHapticParameterCurve_c __asm__("OBJC_CLASS_$_CHHapticParameterCurve"), CHHapticParameterCurve_m __asm__("OBJC_METACLASS_$_CHHapticParameterCurve");
cx_class_t CHHapticParameterCurveControlPoint_c __asm__("OBJC_CLASS_$_CHHapticParameterCurveControlPoint"), CHHapticParameterCurveControlPoint_m __asm__("OBJC_METACLASS_$_CHHapticParameterCurveControlPoint");
cx_class_t CHHapticPattern_c __asm__("OBJC_CLASS_$_CHHapticPattern"), CHHapticPattern_m __asm__("OBJC_METACLASS_$_CHHapticPattern");
cx_class_t CHHapticPatternPlayerMOS_c __asm__("OBJC_CLASS_$_CHHapticPatternPlayerMOS"), CHHapticPatternPlayerMOS_m __asm__("OBJC_METACLASS_$_CHHapticPatternPlayerMOS");
cx_class_t CHHapticDeviceCapabilityMOS_c __asm__("OBJC_CLASS_$_CHHapticDeviceCapabilityMOS"), CHHapticDeviceCapabilityMOS_m __asm__("OBJC_METACLASS_$_CHHapticDeviceCapabilityMOS");
cx_class_t CHHapticParameterAttributesMOS_c __asm__("OBJC_CLASS_$_CHHapticParameterAttributesMOS"), CHHapticParameterAttributesMOS_m __asm__("OBJC_METACLASS_$_CHHapticParameterAttributesMOS");

/* ---- valores: evento, parâmetro e curva */
typedef struct { void *isa; id pid; float value; double time; } chparam_t;      /* parâmetro de evento ou dinâmico */
typedef struct { void *isa; double time; float value; } chpoint_t;
typedef struct { void *isa; id pid; double time; id points; } chcurve_t;
typedef struct { void *isa; id type; id params; double time, duration; u64 resource; } chevent_t;
static id prm_init(id s, SEL c, id pid, float v) { (void)c; chparam_t *p = (chparam_t *)s; p->pid = ret(pid); p->value = v; return s; }
static id prm_init_time(id s, SEL c, id pid, float v, double t) { (void)c; chparam_t *p = (chparam_t *)s; p->pid = ret(pid); p->value = v; p->time = t; return s; }
static id prm_id(id s, SEL c) { (void)c; return ((chparam_t *)s)->pid; }
static float prm_value(id s, SEL c) { (void)c; return ((chparam_t *)s)->value; }
static void prm_set_value(id s, SEL c, float v) { (void)c; ((chparam_t *)s)->value = v; }
static double prm_time(id s, SEL c) { (void)c; return ((chparam_t *)s)->time; }
static void prm_set_time(id s, SEL c, double t) { (void)c; ((chparam_t *)s)->time = t; }
static id cp_init(id s, SEL c, double t, float v) { (void)c; chpoint_t *p = (chpoint_t *)s; p->time = t; p->value = v; return s; }
static double cp_time(id s, SEL c) { (void)c; return ((chpoint_t *)s)->time; }
static void cp_set_time(id s, SEL c, double t) { (void)c; ((chpoint_t *)s)->time = t; }
static float cp_value(id s, SEL c) { (void)c; return ((chpoint_t *)s)->value; }
static void cp_set_value(id s, SEL c, float v) { (void)c; ((chpoint_t *)s)->value = v; }
static id cv_init(id s, SEL c, id pid, id points, double t) { (void)c; chcurve_t *p = (chcurve_t *)s; p->pid = ret(pid); p->points = ret(points); p->time = t; return s; }
static id cv_id(id s, SEL c) { (void)c; return ((chcurve_t *)s)->pid; }
static id cv_points(id s, SEL c) { (void)c; return ((chcurve_t *)s)->points; }
static double cv_time(id s, SEL c) { (void)c; return ((chcurve_t *)s)->time; }
static void cv_set_time(id s, SEL c, double t) { (void)c; ((chcurve_t *)s)->time = t; }
static id ev_init(id s, SEL c, id type, id params, double t) {
    (void)c;
    chevent_t *e = (chevent_t *)s;
    /* tipo canônico: as comparações abaixo são por ponteiro, e o tipo pode vir de um AHAP */
    id known[4] = {CHHapticEventTypeHapticTransient, CHHapticEventTypeHapticContinuous, CHHapticEventTypeAudioContinuous, CHHapticEventTypeAudioCustom};
    for (int k = 0; k < 4; k++) if (streq(type, known[k])) type = known[k];
    e->type = ret(type); e->params = ret(params ? params : MSG0(id, cls("NSArray"), "array")); e->time = t;
    e->duration = type == CHHapticEventTypeHapticTransient ? 0.03 : 1.0;
    return s;
}
static id ev_init_dur(id s, SEL c, id type, id params, double t, double d) { ev_init(s, c, type, params, t); ((chevent_t *)s)->duration = d; return s; }
static id ev_init_res(id s, SEL c, u64 res, id params, double t) { ev_init(s, c, CHHapticEventTypeAudioCustom, params, t); ((chevent_t *)s)->resource = res; ((chevent_t *)s)->duration = 0; return s; }
static id ev_init_res_dur(id s, SEL c, u64 res, id params, double t, double d) { ev_init_res(s, c, res, params, t); ((chevent_t *)s)->duration = d; return s; }
static id ev_type(id s, SEL c) { (void)c; return ((chevent_t *)s)->type; }
static id ev_params(id s, SEL c) { (void)c; return ((chevent_t *)s)->params; }
static double ev_time(id s, SEL c) { (void)c; return ((chevent_t *)s)->time; }
static void ev_set_time(id s, SEL c, double t) { (void)c; ((chevent_t *)s)->time = t; }
static double ev_duration(id s, SEL c) { (void)c; return ((chevent_t *)s)->duration; }
static void ev_set_duration(id s, SEL c, double d) { (void)c; ((chevent_t *)s)->duration = d; }
static float ev_param(chevent_t *e, id pid, float dflt) {
    for (u64 i = 0; i < cnt(e->params); i++) {
        chparam_t *p = (chparam_t *)at(e->params, i);
        if (streq(p->pid, pid)) return p->value;
    }
    return dflt;
}

/* ---- padrão */
typedef struct { void *isa; id events, params, curves; double duration; } chpattern_t;
static double pattern_duration(chpattern_t *p) {
    double d = 0;
    for (u64 i = 0; i < cnt(p->events); i++) {
        chevent_t *e = (chevent_t *)at(p->events, i);
        double end = e->time + e->duration;
        if (end > d) d = end;
    }
    for (u64 i = 0; i < cnt(p->curves); i++) {
        chcurve_t *cv = (chcurve_t *)at(p->curves, i);
        u64 n = cnt(cv->points);
        if (n) { double end = cv->time + ((chpoint_t *)at(cv->points, n - 1))->time; if (end > d) d = end; }
    }
    return d;
}
static id pat_init(id s, SEL c, id events, id params, id *err) {
    (void)c;
    if (err) *err = 0;
    chpattern_t *p = (chpattern_t *)s;
    p->events = ret(events ? events : MSG0(id, cls("NSArray"), "array"));
    p->params = ret(params ? params : MSG0(id, cls("NSArray"), "array"));
    p->curves = ret(MSG0(id, cls("NSArray"), "array"));
    p->duration = pattern_duration(p);
    return s;
}
static id pat_init_curves(id s, SEL c, id events, id curves, id *err) {
    pat_init(s, c, events, 0, err);
    chpattern_t *p = (chpattern_t *)s;
    setobj(&p->curves, curves ? curves : MSG0(id, cls("NSArray"), "array"));
    p->duration = pattern_duration(p);
    return s;
}
/* AHAP: {"Version": 1, "Pattern": [{"Event": {...}}, {"Parameter": {...}}, {"ParameterCurve": {...}}]} */
static id pat_init_dict(id s, SEL c, id dict, id *err) {
    (void)c;
    id list = dget(dict, "Pattern");
    if (!list) { if (err) *err = error_with("com.apple.CoreHaptics", -4810, "padrão AHAP sem a chave Pattern"); objc_release(s); return 0; }
    id events = MSG0(id, cls("NSMutableArray"), "array"), params = MSG0(id, cls("NSMutableArray"), "array"), curves = MSG0(id, cls("NSMutableArray"), "array");
    for (u64 i = 0; i < cnt(list); i++) {
        id item = at(list, i), e, p, cv;
        if ((e = dget(item, "Event"))) {
            id ps = MSG0(id, cls("NSMutableArray"), "array"), lst = dget(e, "EventParameters");
            for (u64 k = 0; k < cnt(lst); k++) {
                id q = at(lst, k);
                id prm = prm_init(cx_alloc_instance(&CHHapticEventParameter_c), 0, dget(q, "ParameterID"), (float)num(dget(q, "ParameterValue"), 0));
                MSG(void, ps, "addObject:", prm); objc_release(prm);
            }
            id type = dget(e, "EventType");
            id ev = ev_init(cx_alloc_instance(&CHHapticEvent_c), 0, type, ps, num(dget(e, "Time"), 0));
            if (dget(e, "EventDuration")) ((chevent_t *)ev)->duration = num(dget(e, "EventDuration"), 1);
            MSG(void, events, "addObject:", ev); objc_release(ev);
        } else if ((p = dget(item, "Parameter"))) {
            id prm = prm_init_time(cx_alloc_instance(&CHHapticDynamicParameter_c), 0, dget(p, "ParameterID"), (float)num(dget(p, "ParameterValue"), 0), num(dget(p, "Time"), 0));
            MSG(void, params, "addObject:", prm); objc_release(prm);
        } else if ((cv = dget(item, "ParameterCurve"))) {
            id pts = MSG0(id, cls("NSMutableArray"), "array"), lst = dget(cv, "ParameterCurveControlPoints");
            for (u64 k = 0; k < cnt(lst); k++) {
                id q = at(lst, k);
                id pt = cp_init(cx_alloc_instance(&CHHapticParameterCurveControlPoint_c), 0, num(dget(q, "Time"), 0), (float)num(dget(q, "ParameterValue"), 0));
                MSG(void, pts, "addObject:", pt); objc_release(pt);
            }
            id curve = cv_init(cx_alloc_instance(&CHHapticParameterCurve_c), 0, dget(cv, "ParameterID"), pts, num(dget(cv, "Time"), 0));
            MSG(void, curves, "addObject:", curve); objc_release(curve);
        }
    }
    pat_init(s, 0, events, params, err);
    setobj(&((chpattern_t *)s)->curves, curves);
    ((chpattern_t *)s)->duration = pattern_duration((chpattern_t *)s);
    return s;
}
static id parse_ahap_data(id data, id *err) {
    id js = cls("NSJSONSerialization");
    id d = js && data ? ((id (*)(id, SEL, id, u64, id *))objc_msgSend)(js, sel_registerName("JSONObjectWithData:options:error:"), data, 0, err) : 0;
    return d;
}
static id pat_init_url(id s, SEL c, id url, id *err) {
    id path = url && resp(url, "path") ? MSG0(id, url, "path") : url;
    u64 n = 0;
    u8 *b = path ? cx_read_file(cx_cstr(path), &n) : 0;
    if (!b) { if (err) *err = error_with("com.apple.CoreHaptics", -4851, "arquivo AHAP não encontrado"); objc_release(s); return 0; }
    id data = objc_autorelease(CFDataCreate(0, b, (long)n));
    free(b);
    id d = parse_ahap_data(data, err);
    if (!d) { objc_release(s); return 0; }
    return pat_init_dict(s, c, d, err);
}
static double pat_duration(id s, SEL c) { (void)c; return ((chpattern_t *)s)->duration; }
static id pat_export(id s, SEL c, id *err) {
    (void)c;
    if (err) *err = 0;
    chpattern_t *p = (chpattern_t *)s;
    id list = MSG0(id, cls("NSMutableArray"), "array");
    for (u64 i = 0; i < cnt(p->events); i++) {
        chevent_t *e = (chevent_t *)at(p->events, i);
        id ed = mdict(), ps = MSG0(id, cls("NSMutableArray"), "array");
        dset(ed, cx_str("EventType"), e->type);
        dset(ed, cx_str("Time"), nsnum(e->time));
        if (e->type != CHHapticEventTypeHapticTransient) dset(ed, cx_str("EventDuration"), nsnum(e->duration));
        for (u64 k = 0; k < cnt(e->params); k++) {
            chparam_t *q = (chparam_t *)at(e->params, k);
            id pd = mdict();
            dset(pd, cx_str("ParameterID"), q->pid);
            dset(pd, cx_str("ParameterValue"), nsnum(q->value));
            MSG(void, ps, "addObject:", pd);
        }
        dset(ed, cx_str("EventParameters"), ps);
        id w = mdict();
        dset(w, cx_str("Event"), ed);
        MSG(void, list, "addObject:", w);
    }
    id d = mdict();
    dset(d, cx_str("Version"), nsnum(1));
    dset(d, cx_str("Pattern"), list);
    return d;
}

/* ---- motor e players
 * Cada player guarda o padrão e o instante de início (relógio do motor, em segundos). Um timer de 5 ms
 * percorre os players ativos: eventos hápticos viram vibração do controle (motor forte ~ intensidade,
 * fraco ~ nitidez); eventos de áudio são sintetizados em blocos e mandados ao mixer. */
typedef struct {
    void *isa;
    int slot;                       /* controle que vibra; -1 = motor do Mac (sem atuador háptico) */
    BOOL running, muted_audio, muted_haptics, haptics_only, auto_shutdown, is_reset;
    void *stopped_h, *reset_h, *finished_h;
    id players;                     /* players ativos */
    id resources;                   /* id → NSData (PCM 48 kHz estéreo i16) */
    u64 next_res;
    double t0;                      /* início do relógio do motor */
    float last_s, last_w;           /* última vibração pedida (só reenviamos quando muda ou para renovar) */
    double last_sent;
} chengine_t;
typedef struct {
    void *isa;
    id engine, pattern;
    BOOL playing, paused, muted, loop, advanced;
    double start;                   /* relógio do motor em que o tempo 0 do padrão toca */
    double paused_at, offset, rate, loop_end;
    void *completion_h;
    int next_event;
    float intensity_ctl, sharpness_ctl, volume_ctl, pitch_ctl;
    double audio_until;             /* até onde já mandamos áudio */
} chplayer_t;
static double eng_time(chengine_t *e) { return now_s() - e->t0; }
static id g_engines;
static id g_haptic_timer;
static void rumble(int slot, float strong, float weak, double secs) {
    if (slot < 0) return;
    int s = (int)(strong * 255), w = (int)(weak * 255);
    s = s < 0 ? 0 : s > 255 ? 255 : s;
    w = w < 0 ? 0 : w > 255 ? 255 : w;
    u32 ms = secs <= 0 ? 0 : secs > 10 ? 10000 : (u32)(secs * 1000);
    SYS4(GUI_DEVICE, DEV_PAD_RUMBLE, slot, (u32)s | (u32)w << 8, ms ? ms : 1);
}
/* bloco de áudio sintetizado: tom com envelope (AudioContinuous) ou recurso registrado (AudioCustom) */
static void play_audio_event(chengine_t *eng, chplayer_t *pl, chevent_t *ev) {
    if (eng->muted_audio || eng->haptics_only || pl->muted) return;
    float vol = ev_param(ev, CHHapticEventParameterIDAudioVolume, 0.75f) * pl->volume_ctl;
    float pan = ev_param(ev, CHHapticEventParameterIDAudioPan, 0);
    float pitch = ev_param(ev, CHHapticEventParameterIDAudioPitch, 0) + pl->pitch_ctl;
    float bright = ev_param(ev, CHHapticEventParameterIDAudioBrightness, 0.5f);
    if (ev->type == CHHapticEventTypeAudioCustom) {
        id key = MSG(id, cls("NSNumber"), "numberWithUnsignedLongLong:", ev->resource);
        id data = MSG(id, eng->resources, "objectForKey:", key);
        if (!data) return;
        const i16 *src = MSG0(const i16 *, data, "bytes");
        u64 frames = MSG0(u64, data, "length") / 4;
        if (ev->duration > 0 && (u64)(ev->duration * 48000) < frames) frames = (u64)(ev->duration * 48000);
        i16 *buf = malloc(frames * 4 + 4);
        if (!buf) return;
        for (u64 i = 0; i < frames * 2; i++) { int v = (int)(src[i] * vol); buf[i] = (i16)(v > 32767 ? 32767 : v < -32768 ? -32768 : v); }
        for (u64 o = 0; o < frames;) { long n = SYS3(GUI_DEVICE, DEV_AUDIO_PLAY, buf + o * 2, frames - o); if (n <= 0) break; o += (u64)n; }
        free(buf);
        return;
    }
    double dur = ev->duration > 0 ? ev->duration : 0.1;
    if (dur > 4) dur = 4;
    u64 frames = (u64)(dur * 48000);
    i16 *buf = malloc(frames * 4 + 4);
    if (!buf) return;
    double f = 440.0 * pow(2.0, pitch);                    /* AudioPitch: -1..1 = uma oitava para baixo/cima */
    float attack = ev_param(ev, CHHapticEventParameterIDAttackTime, 0.01f), release = ev_param(ev, CHHapticEventParameterIDReleaseTime, 0.05f);
    float l = vol * (pan <= 0 ? 1 : 1 - pan), r = vol * (pan >= 0 ? 1 : 1 + pan);
    for (u64 i = 0; i < frames; i++) {
        double t = i / 48000.0, env = 1;
        if (attack > 0 && t < attack) env = t / attack;
        if (release > 0 && dur - t < release) env *= (dur - t) / release;
        double ph = 2 * 3.14159265358979 * f * t;
        double s = sin(ph) * (1 - bright * 0.5) + sin(3 * ph) * bright * 0.25;   /* brilho = harmônicos */
        buf[i * 2] = (i16)(s * env * l * 12000);
        buf[i * 2 + 1] = (i16)(s * env * r * 12000);
    }
    for (u64 o = 0; o < frames;) { long n = SYS3(GUI_DEVICE, DEV_AUDIO_PLAY, buf + o * 2, frames - o); if (n <= 0) break; o += (u64)n; }
    free(buf);
}
static void player_finish(chengine_t *eng, chplayer_t *pl, id err) {
    pl->playing = NO;
    if (pl->completion_h) ((void (*)(void *, id))BLK(pl->completion_h)->invoke)(pl->completion_h, err);
    (void)eng;
}
static void haptic_tick(id s, SEL c, id t) {
    (void)s; (void)c; (void)t;
    for (u64 ei = 0; ei < cnt(g_engines); ei++) {
        chengine_t *eng = (chengine_t *)at(g_engines, ei);
        if (!eng->running) continue;
        double now = eng_time(eng);
        float strong = 0, weak = 0;
        BOOL any = NO;
        for (u64 pi = 0; pi < cnt(eng->players); pi++) {
            chplayer_t *pl = (chplayer_t *)at(eng->players, pi);
            if (!pl->playing || pl->paused) continue;
            chpattern_t *pat = (chpattern_t *)pl->pattern;
            double pt = (now - pl->start) * (pl->rate > 0 ? pl->rate : 1) + pl->offset;
            if (pt < 0) { any = YES; continue; }
            double end = pl->loop && pl->loop_end > 0 ? pl->loop_end : pat->duration;
            if (pt >= end) {
                if (pl->loop) { pl->start = now; pl->offset = 0; pl->next_event = 0; continue; }
                player_finish(eng, pl, 0);
                continue;
            }
            any = YES;
            for (u64 k = 0; k < cnt(pat->events); k++) {
                chevent_t *ev = (chevent_t *)at(pat->events, k);
                BOOL haptic = ev->type == CHHapticEventTypeHapticTransient || ev->type == CHHapticEventTypeHapticContinuous;
                double dur = ev->type == CHHapticEventTypeHapticTransient ? 0.03 : ev->duration;
                if (haptic && pt >= ev->time && pt < ev->time + dur && !pl->muted && !eng->muted_haptics) {
                    float in = ev_param(ev, CHHapticEventParameterIDHapticIntensity, 1) * pl->intensity_ctl;
                    float sh = ev_param(ev, CHHapticEventParameterIDHapticSharpness, 0.5f) + pl->sharpness_ctl;
                    sh = sh < 0 ? 0 : sh > 1 ? 1 : sh;
                    float st = in * (1 - sh * 0.6f), wk = in * (0.4f + sh * 0.6f);
                    if (st > strong) strong = st;
                    if (wk > weak) weak = wk;
                }

                if (!haptic && (int)k >= pl->next_event && pt >= ev->time) { play_audio_event(eng, pl, ev); }
            }
            /* próximo evento de áudio ainda não tocado */
            int ne = 0;
            for (u64 k = 0; k < cnt(pat->events); k++) if (((chevent_t *)at(pat->events, k))->time <= pt) ne = (int)k + 1;
            if (ne > pl->next_event) pl->next_event = ne;
        }
        if (eng->slot >= 0 && (strong != eng->last_s || weak != eng->last_w || ((strong > 0 || weak > 0) && now - eng->last_sent > 0.2))) {
            rumble(eng->slot, strong, weak, (strong > 0 || weak > 0) ? 0.3 : 0);
            eng->last_s = strong; eng->last_w = weak; eng->last_sent = now;
        }
        if (!any && eng->finished_h) {
            void *h = eng->finished_h;
            eng->finished_h = 0;
            i64 r = ((i64 (*)(void *, id))BLK(h)->invoke)(h, (id)0);   /* CHHapticEngineFinishedAction */
            _Block_release(h);
            if (r == 1 /* StopEngine */) eng->running = NO;
        }
    }
}
cx_class_t CHHapticMOSTicker_c __asm__("OBJC_CLASS_$_CHHapticMOSTicker"), CHHapticMOSTicker_m __asm__("OBJC_METACLASS_$_CHHapticMOSTicker");
static void ensure_haptic_timer(void) {
    if (g_haptic_timer) return;
    if (!g_engines) g_engines = ret(MSG0(id, cls("NSMutableArray"), "array"));
    id tk = cx_alloc_instance(&CHHapticMOSTicker_c);
    g_haptic_timer = ret(MSG(id, cls("NSTimer"), "scheduledTimerWithTimeInterval:target:selector:userInfo:repeats:", 0.005, tk, sel_registerName("tick:"), (id)0, YES));
}
static id eng_init_slot(id s, int slot) {
    chengine_t *e = (chengine_t *)s;
    e->slot = slot;
    e->players = ret(MSG0(id, cls("NSMutableArray"), "array"));
    e->resources = ret(mdict());
    e->next_res = 1;
    e->t0 = now_s();
    ensure_haptic_timer();
    MSG(void, g_engines, "addObject:", s);
    return s;
}
static id eng_init(id s, SEL c, id *err) { (void)c; if (err) *err = 0; return eng_init_slot(s, -1); }
static id eng_init_session(id s, SEL c, id session, id *err) { (void)session; return eng_init(s, c, err); }
id cx_haptic_engine_for_pad(int slot, id locality) {
    (void)locality;
    return objc_autorelease(eng_init_slot(cx_alloc_instance(&CHHapticEngine_c), slot));
}
typedef struct { void *isa; BOOL haptics; } chcap_t;
typedef struct { void *isa; float min, max, dflt; } chattr_t;
static BOOL cap_haptics(id s, SEL c) { (void)c; return ((chcap_t *)s)->haptics; }
static BOOL cap_audio(id s, SEL c) { (void)s; (void)c; return YES; }
static id cap_attrs(id s, SEL c, id pid, id type, id *err) {
    (void)s; (void)c; (void)type;
    if (err) *err = 0;
    chattr_t *a = (chattr_t *)cx_alloc_instance(&CHHapticParameterAttributesMOS_c);
    a->min = 0; a->max = 1; a->dflt = 0.5f;
    if (streq(pid, CHHapticEventParameterIDAudioPitch) || streq(pid, CHHapticEventParameterIDAudioPan)) { a->min = -1; a->dflt = 0; }
    if (streq(pid, CHHapticEventParameterIDHapticIntensity) || streq(pid, CHHapticEventParameterIDAudioVolume)) a->dflt = 1;
    return objc_autorelease((id)a);
}
static float attr_min(id s, SEL c) { (void)c; return ((chattr_t *)s)->min; }
static float attr_max(id s, SEL c) { (void)c; return ((chattr_t *)s)->max; }
static float attr_dflt(id s, SEL c) { (void)c; return ((chattr_t *)s)->dflt; }
/* +[CHHapticEngine capabilitiesForHardware]: Mac sem atuador háptico, mas com áudio */
static id eng_caps(id k, SEL c) { (void)k; (void)c; chcap_t *o = (chcap_t *)cx_alloc_instance(&CHHapticDeviceCapabilityMOS_c); o->haptics = NO; return objc_autorelease((id)o); }
static id eng_caps_inst(id s, SEL c) { (void)c; chcap_t *o = (chcap_t *)cx_alloc_instance(&CHHapticDeviceCapabilityMOS_c); o->haptics = ((chengine_t *)s)->slot >= 0; return objc_autorelease((id)o); }
static BOOL eng_start(id s, SEL c, id *err) { (void)c; if (err) *err = 0; chengine_t *e = (chengine_t *)s; if (!e->running) { e->running = YES; e->t0 = now_s(); } return YES; }
static void eng_start_cb(id s, SEL c, void *h) { eng_start(s, c, 0); if (h) ((void (*)(void *, id))BLK(h)->invoke)(h, (id)0); }
static void eng_stop_cb(id s, SEL c, void *h) {
    (void)c;
    chengine_t *e = (chengine_t *)s;
    e->running = NO;
    for (u64 i = 0; i < cnt(e->players); i++) ((chplayer_t *)at(e->players, i))->playing = NO;
    rumble(e->slot, 0, 0, 0);
    if (h) ((void (*)(void *, id))BLK(h)->invoke)(h, (id)0);
    if (e->stopped_h) ((void (*)(void *, i64))BLK(e->stopped_h)->invoke)(e->stopped_h, 0 /* ApplicationSuspended não; parado pelo app */);
}
static void eng_stop(id s, SEL c) { eng_stop_cb(s, c, 0); }
static double eng_current(id s, SEL c) { (void)c; return eng_time((chengine_t *)s); }
static void *eng_stopped_h(id s, SEL c) { (void)c; return ((chengine_t *)s)->stopped_h; }
static void eng_set_stopped_h(id s, SEL c, void *b) { (void)c; setblk(&((chengine_t *)s)->stopped_h, b); }
static void *eng_reset_h(id s, SEL c) { (void)c; return ((chengine_t *)s)->reset_h; }
static void eng_set_reset_h(id s, SEL c, void *b) { (void)c; setblk(&((chengine_t *)s)->reset_h, b); }
#define EBOOL(get, set, field) \
    static BOOL get(id s, SEL c) { (void)c; return ((chengine_t *)s)->field; } \
    static void set(id s, SEL c, BOOL b) { (void)c; ((chengine_t *)s)->field = b; }
EBOOL(eng_muted_audio, eng_set_muted_audio, muted_audio)
EBOOL(eng_muted_haptics, eng_set_muted_haptics, muted_haptics)
EBOOL(eng_haptics_only, eng_set_haptics_only, haptics_only)
EBOOL(eng_auto_shutdown, eng_set_auto_shutdown, auto_shutdown)
static BOOL eng_plays_audio_only(id s, SEL c) { (void)c; return ((chengine_t *)s)->slot < 0; }
static void eng_notify_finished(id s, SEL c, void *h) { (void)c; setblk(&((chengine_t *)s)->finished_h, h); }
static id player_new(id eng, id pattern, BOOL adv) {
    chplayer_t *p = (chplayer_t *)cx_alloc_instance(&CHHapticPatternPlayerMOS_c);
    p->engine = eng; p->pattern = ret(pattern); p->advanced = adv; p->rate = 1;
    p->intensity_ctl = 1; p->volume_ctl = 1;
    MSG(void, ((chengine_t *)eng)->players, "addObject:", (id)p);
    return objc_autorelease((id)p);
}
static id eng_player(id s, SEL c, id pattern, id *err) { (void)c; if (err) *err = 0; return pattern ? player_new(s, pattern, NO) : 0; }
static id eng_adv_player(id s, SEL c, id pattern, id *err) { (void)c; if (err) *err = 0; return pattern ? player_new(s, pattern, YES) : 0; }
static BOOL eng_play_url(id s, SEL c, id url, id *err) {
    (void)c;
    id pat = pat_init_url(cx_alloc_instance(&CHHapticPattern_c), 0, url, err);
    if (!pat) return NO;
    chplayer_t *p = (chplayer_t *)player_new(s, objc_autorelease(pat), NO);
    eng_start(s, 0, 0);
    p->playing = YES; p->start = eng_time((chengine_t *)s);
    return YES;
}
static BOOL eng_play_data(id s, SEL c, id data, id *err) {
    (void)c;
    id d = parse_ahap_data(data, err);
    if (!d) return NO;
    id pat = pat_init_dict(cx_alloc_instance(&CHHapticPattern_c), 0, d, err);
    if (!pat) return NO;
    chplayer_t *p = (chplayer_t *)player_new(s, objc_autorelease(pat), NO);
    eng_start(s, 0, 0);
    p->playing = YES; p->start = eng_time((chengine_t *)s);
    return YES;
}
/* registerAudioResource:options:error: — WAV PCM 16 bits (mono/estéreo, qualquer taxa) convertido para 48 kHz estéreo */
static u64 eng_register_audio(id s, SEL c, id url, id opts, id *err) {
    (void)c; (void)opts;
    chengine_t *e = (chengine_t *)s;
    id path = url && resp(url, "path") ? MSG0(id, url, "path") : url;
    u64 n = 0;
    u8 *b = path ? cx_read_file(cx_cstr(path), &n) : 0;
    if (!b || n < 44 || memcmp(b, "RIFF", 4) || memcmp(b + 8, "WAVE", 4)) {
        free(b);
        if (err) *err = error_with("com.apple.CoreHaptics", -4851, "recurso de áudio inválido (use WAV PCM de 16 bits)");
        return 0;
    }
    u32 rate = 44100, ch = 2, bits = 16;
    const u8 *data = 0;
    u32 dlen = 0;
    for (u64 o = 12; o + 8 <= n;) {
        u32 sz = (u32)(b[o + 4] | b[o + 5] << 8 | b[o + 6] << 16 | (u32)b[o + 7] << 24);
        if (!memcmp(b + o, "fmt ", 4) && o + 24 <= n) { ch = b[o + 10] | b[o + 11] << 8; rate = b[o + 12] | b[o + 13] << 8 | b[o + 14] << 16 | (u32)b[o + 15] << 24; bits = b[o + 22] | b[o + 23] << 8; }
        if (!memcmp(b + o, "data", 4)) { data = b + o + 8; dlen = (u32)(o + 8 + sz <= n ? sz : n - o - 8); break; }
        o += 8 + sz + (sz & 1);
    }
    if (!data || bits != 16 || !ch || !rate) { free(b); if (err) *err = error_with("com.apple.CoreHaptics", -4851, "WAV sem dados PCM de 16 bits"); return 0; }
    u64 in_frames = dlen / (2 * ch), out_frames = in_frames * 48000 / rate;
    i16 *out = malloc(out_frames * 4 + 4);
    const i16 *src = (const i16 *)data;
    for (u64 i = 0; i < out_frames; i++) {
        u64 j = i * rate / 48000;
        if (j >= in_frames) j = in_frames - 1;
        out[i * 2] = src[j * ch];
        out[i * 2 + 1] = src[j * ch + (ch > 1 ? 1 : 0)];
    }
    id nd = MSG(id, cls("NSData"), "dataWithBytes:length:", out, out_frames * 4);
    free(out);
    free(b);
    u64 rid = e->next_res++;
    id rk = MSG(id, cls("NSNumber"), "numberWithUnsignedLongLong:", rid);
    MSG(void, e->resources, "setObject:forKey:", nd, rk);
    if (err) *err = 0;
    return rid;
}
static BOOL eng_unregister_audio(id s, SEL c, u64 rid, id *err) {
    (void)c;
    if (err) *err = 0;
    id rk = MSG(id, cls("NSNumber"), "numberWithUnsignedLongLong:", rid);
    MSG(void, ((chengine_t *)s)->resources, "removeObjectForKey:", rk);
    return YES;
}
/* players */
#define PL(o) ((chplayer_t *)(o))
static BOOL pl_start(id s, SEL c, double t, id *err) {
    (void)c;
    if (err) *err = 0;
    chplayer_t *p = PL(s);
    chengine_t *e = (chengine_t *)p->engine;
    if (!e->running) eng_start(p->engine, 0, 0);
    double now = eng_time(e);
    p->start = t > now ? t : now;
    p->offset = 0; p->next_event = 0; p->paused = NO; p->playing = YES;
    return YES;
}
static BOOL pl_stop(id s, SEL c, double t, id *err) { (void)c; (void)t; if (err) *err = 0; PL(s)->playing = NO; return YES; }
static BOOL pl_cancel(id s, SEL c, id *err) { (void)c; if (err) *err = 0; PL(s)->playing = NO; return YES; }
static BOOL pl_send(id s, SEL c, id params, double t, id *err) {
    (void)c; (void)t;
    if (err) *err = 0;
    chplayer_t *p = PL(s);
    for (u64 i = 0; i < cnt(params); i++) {
        chparam_t *q = (chparam_t *)at(params, i);
        if (streq(q->pid, CHHapticDynamicParameterIDHapticIntensityControl)) p->intensity_ctl = q->value;
        else if (streq(q->pid, CHHapticDynamicParameterIDHapticSharpnessControl)) p->sharpness_ctl = q->value;
        else if (streq(q->pid, CHHapticDynamicParameterIDAudioVolumeControl)) p->volume_ctl = q->value;
        else if (streq(q->pid, CHHapticDynamicParameterIDAudioPitchControl)) p->pitch_ctl = q->value;
    }
    return YES;
}
static BOOL pl_schedule(id s, SEL c, id curve, double t, id *err) {
    (void)c; (void)t;
    if (err) *err = 0;
    chcurve_t *cv = (chcurve_t *)curve;
    u64 n = cnt(cv->points);
    if (!n) return YES;
    chparam_t q = {0, cv->pid, ((chpoint_t *)at(cv->points, n - 1))->value};
    if (streq(cv->pid, CHHapticDynamicParameterIDHapticIntensityControl)) PL(s)->intensity_ctl = q.value;
    else if (streq(cv->pid, CHHapticDynamicParameterIDAudioVolumeControl)) PL(s)->volume_ctl = q.value;
    return YES;
}
static BOOL pl_muted(id s, SEL c) { (void)c; return PL(s)->muted; }
static void pl_set_muted(id s, SEL c, BOOL b) { (void)c; PL(s)->muted = b; }
static BOOL pl_pause(id s, SEL c, double t, id *err) {
    (void)c; (void)t;
    if (err) *err = 0;
    chplayer_t *p = PL(s);
    chengine_t *e = (chengine_t *)p->engine;
    if (p->playing && !p->paused) { p->offset = (eng_time(e) - p->start) * p->rate + p->offset; p->paused = YES; }
    return YES;
}
static BOOL pl_resume(id s, SEL c, double t, id *err) {
    (void)c;
    if (err) *err = 0;
    chplayer_t *p = PL(s);
    chengine_t *e = (chengine_t *)p->engine;
    if (p->paused) { double now = eng_time(e); p->start = t > now ? t : now; p->paused = NO; }
    return YES;
}
static BOOL pl_seek(id s, SEL c, double off, id *err) {
    (void)c;
    if (err) *err = 0;
    chplayer_t *p = PL(s);
    p->offset = off; p->start = eng_time((chengine_t *)p->engine); p->next_event = 0;
    return YES;
}
static BOOL pl_loop(id s, SEL c) { (void)c; return PL(s)->loop; }
static void pl_set_loop(id s, SEL c, BOOL b) { (void)c; PL(s)->loop = b; }
static double pl_loop_end(id s, SEL c) { (void)c; return PL(s)->loop_end; }
static void pl_set_loop_end(id s, SEL c, double v) { (void)c; PL(s)->loop_end = v; }
static float pl_rate(id s, SEL c) { (void)c; return (float)PL(s)->rate; }
static void pl_set_rate(id s, SEL c, float r) { (void)c; PL(s)->rate = r > 0 ? r : 1; }
static void *pl_completion(id s, SEL c) { (void)c; return PL(s)->completion_h; }
static void pl_set_completion(id s, SEL c, void *b) { (void)c; setblk(&PL(s)->completion_h, b); }
static double pl_current(id s, SEL c) {
    (void)c;
    chplayer_t *p = PL(s);
    return p->paused ? p->offset : (eng_time((chengine_t *)p->engine) - p->start) * p->rate + p->offset;
}

/* ================================================================ registro */
static void K(cx_class_t *k, cx_class_t *m, const char *name, cx_class_t *super, u32 size) { if (!cx_class_named(name)) cx_define_class(k, m, name, super, size); }
#define A(k, s, f) cx_add_method(k, s, (void *)(f))
#define AC(k, s, f) cx_add_cmethod(k, s, (void *)(f))
static id S(const char *s) { return ret(cx_str(s)); }
void cx_gamecontroller_init(void) {
    cx_class_t *obj = cx_class_named("NSObject");
    GCControllerDidConnectNotification = S("GCControllerDidConnectNotification");
    GCControllerDidDisconnectNotification = S("GCControllerDidDisconnectNotification");
    GCControllerDidBecomeCurrentNotification = S("GCControllerDidBecomeCurrentNotification");
    GCControllerDidStopBeingCurrentNotification = S("GCControllerDidStopBeingCurrentNotification");
    GCControllerUserCustomizationsDidChangeNotification = S("GCControllerUserCustomizationsDidChangeNotification");
    GCKeyboardDidConnectNotification = S("GCKeyboardDidConnectNotification");
    GCKeyboardDidDisconnectNotification = S("GCKeyboardDidDisconnectNotification");
    GCMouseDidConnectNotification = S("GCMouseDidConnectNotification");
    GCMouseDidDisconnectNotification = S("GCMouseDidDisconnectNotification");
    GCMouseDidBecomeCurrentNotification = S("GCMouseDidBecomeCurrentNotification");
    GCMouseDidStopBeingCurrentNotification = S("GCMouseDidStopBeingCurrentNotification");
    GCInputButtonA = S("Button A"); GCInputButtonB = S("Button B"); GCInputButtonX = S("Button X"); GCInputButtonY = S("Button Y");
    GCInputLeftShoulder = S("Left Shoulder"); GCInputRightShoulder = S("Right Shoulder");
    GCInputLeftTrigger = S("Left Trigger"); GCInputRightTrigger = S("Right Trigger");
    GCInputLeftThumbstick = S("Left Thumbstick"); GCInputRightThumbstick = S("Right Thumbstick");
    GCInputLeftThumbstickButton = S("Left Thumbstick Button"); GCInputRightThumbstickButton = S("Right Thumbstick Button");
    GCInputDirectionPad = S("Direction Pad"); GCInputButtonMenu = S("Button Menu"); GCInputButtonOptions = S("Button Options");
    GCInputButtonHome = S("Button Home"); GCInputDualShockTouchpadButton = S("Touchpad Button"); GCInputButtonShare = S("Button Share");
    GCInputXboxPaddleOne = S("Paddle 1"); GCInputXboxPaddleTwo = S("Paddle 2"); GCInputXboxPaddleThree = S("Paddle 3"); GCInputXboxPaddleFour = S("Paddle 4");
    GCInputLeftPaddle = S("Left Paddle"); GCInputRightPaddle = S("Right Paddle");
    GCProductCategoryDualShock4 = S("DualShock 4"); GCProductCategoryDualSense = S("DualSense"); GCProductCategoryXboxOne = S("Xbox One");
    GCProductCategoryMFi = S("MFi"); GCProductCategoryHID = S("HID"); GCProductCategoryKeyboard = S("Keyboard");
    GCProductCategoryMouse = S("Mouse"); GCProductCategoryCoalesced = S("Coalesced");
    GCHapticsLocalityDefault = S("Default"); GCHapticsLocalityAll = S("All"); GCHapticsLocalityHandles = S("Handles");
    GCHapticsLocalityLeftHandle = S("Left Handle"); GCHapticsLocalityRightHandle = S("Right Handle");
    GCHapticsLocalityTriggers = S("Triggers"); GCHapticsLocalityLeftTrigger = S("Left Trigger"); GCHapticsLocalityRightTrigger = S("Right Trigger");
    GCCurrentExtendedGamepadSnapshotDataVersion = 0;

    /* elementos */
    K(&GCControllerElement_c, &GCControllerElement_m, "GCControllerElement", obj, (u32)(sizeof(gcel_t) - 8));
    cx_class_t *el = &GCControllerElement_c;
    A(el, "localizedName", el_name); A(el, "sfSymbolsName", el_symbol); A(el, "setSfSymbolsName:", el_set_symbol);
    A(el, "unmappedLocalizedName", el_name); A(el, "unmappedSfSymbolsName", el_unmapped); A(el, "collection", el_collection);
    A(el, "isAnalog", el_analog); A(el, "isBoundToSystemGesture", el_bound); A(el, "aliases", el_aliases);
    A(el, "preferredSystemGestureState", el_prefers); A(el, "setPreferredSystemGestureState:", el_set_prefers);
    K(&GCControllerButtonInput_c, &GCControllerButtonInput_m, "GCControllerButtonInput", el, 0);
    cx_class_t *bt = &GCControllerButtonInput_c;
    A(bt, "value", el_value); A(bt, "isPressed", el_pressed); A(bt, "isTouched", el_touched); A(bt, "setValue:", el_set_value);
    A(bt, "valueChangedHandler", el_value_h); A(bt, "setValueChangedHandler:", el_set_value_h);
    A(bt, "pressedChangedHandler", el_pressed_h); A(bt, "setPressedChangedHandler:", el_set_pressed_h);
    A(bt, "touchedChangedHandler", el_touched_h); A(bt, "setTouchedChangedHandler:", el_set_touched_h);
    K(&GCControllerAxisInput_c, &GCControllerAxisInput_m, "GCControllerAxisInput", el, 0);
    cx_class_t *ax = &GCControllerAxisInput_c;
    A(ax, "value", el_value); A(ax, "setValue:", el_set_value);
    A(ax, "valueChangedHandler", el_value_h); A(ax, "setValueChangedHandler:", el_set_value_h);
    K(&GCControllerDirectionPad_c, &GCControllerDirectionPad_m, "GCControllerDirectionPad", el, 0);
    cx_class_t *dp = &GCControllerDirectionPad_c;
    A(dp, "xAxis", el_x); A(dp, "yAxis", el_y); A(dp, "up", el_up); A(dp, "down", el_down); A(dp, "left", el_left); A(dp, "right", el_right);
    A(dp, "valueChangedHandler", el_value_h); A(dp, "setValueChangedHandler:", el_set_value_h); A(dp, "setValueForXAxis:yAxis:", dpad_set_xy);
    K(&GCControllerTouchpad_c, &GCControllerTouchpad_m, "GCControllerTouchpad", el, 0);

    /* perfis */
    K(&GCPhysicalInputProfile_c, &GCPhysicalInputProfile_m, "GCPhysicalInputProfile", obj, (u32)(sizeof(gcprof_t) - 8));
    cx_class_t *pp = &GCPhysicalInputProfile_c;
    A(pp, "device", pr_device); A(pp, "lastEventTimestamp", pr_ts); A(pp, "buttons", pr_buttons); A(pp, "axes", pr_axes);
    A(pp, "dpads", pr_dpads); A(pp, "elements", pr_elements); A(pp, "touchpads", pr_touchpads); A(pp, "allButtons", pr_all_buttons);
    A(pp, "allAxes", pr_all_axes); A(pp, "allDpads", pr_all_dpads); A(pp, "allElements", pr_all_elements); A(pp, "allTouchpads", pr_all_axes);
    A(pp, "objectForKeyedSubscript:", pr_subscript); A(pp, "valueDidChangeHandler", pr_change_h); A(pp, "setValueDidChangeHandler:", pr_set_change_h);
    A(pp, "capture", pr_capture); A(pp, "hasRemappedElements", ctl_attached); A(pp, "setStateFromPhysicalInput:", pr_set_state);
    K(&GCExtendedGamepad_c, &GCExtendedGamepad_m, "GCExtendedGamepad", pp, 0);
    cx_class_t *ex = &GCExtendedGamepad_c;
    A(ex, "controller", pr_controller); A(ex, "valueChangedHandler", pr_value_h); A(ex, "setValueChangedHandler:", pr_set_value_h);
    A(ex, "buttonA", pr_a); A(ex, "buttonB", pr_b); A(ex, "buttonX", pr_x); A(ex, "buttonY", pr_y);
    A(ex, "leftShoulder", pr_lb); A(ex, "rightShoulder", pr_rb); A(ex, "leftTrigger", pr_lt); A(ex, "rightTrigger", pr_rt);
    A(ex, "leftThumbstick", pr_ls); A(ex, "rightThumbstick", pr_rs); A(ex, "leftThumbstickButton", pr_lsb); A(ex, "rightThumbstickButton", pr_rsb);
    A(ex, "buttonMenu", pr_menu); A(ex, "buttonOptions", pr_opt); A(ex, "buttonHome", pr_home); A(ex, "dpad", pr_dpad);
    A(ex, "saveSnapshot", pr_self); A(ex, "setStateFromExtendedGamepad:", pr_set_state);
    K(&GCDualShockGamepad_c, &GCDualShockGamepad_m, "GCDualShockGamepad", ex, 0);
    A(&GCDualShockGamepad_c, "touchpadButton", pr_touch); A(&GCDualShockGamepad_c, "touchpadPrimary", pr_nil); A(&GCDualShockGamepad_c, "touchpadSecondary", pr_nil);
    K(&GCDualSenseGamepad_c, &GCDualSenseGamepad_m, "GCDualSenseGamepad", ex, 0);
    A(&GCDualSenseGamepad_c, "touchpadButton", pr_touch); A(&GCDualSenseGamepad_c, "touchpadPrimary", pr_nil); A(&GCDualSenseGamepad_c, "touchpadSecondary", pr_nil);
    A(&GCDualSenseGamepad_c, "leftTrigger", pr_lt); A(&GCDualSenseGamepad_c, "rightTrigger", pr_rt);
    K(&GCXboxGamepad_c, &GCXboxGamepad_m, "GCXboxGamepad", ex, 0);
    A(&GCXboxGamepad_c, "paddleButton1", pr_nil); A(&GCXboxGamepad_c, "paddleButton2", pr_nil); A(&GCXboxGamepad_c, "paddleButton3", pr_nil);
    A(&GCXboxGamepad_c, "paddleButton4", pr_nil); A(&GCXboxGamepad_c, "buttonShare", pr_nil);
    K(&GCMicroGamepad_c, &GCMicroGamepad_m, "GCMicroGamepad", pp, 0);
    cx_class_t *mg = &GCMicroGamepad_c;
    A(mg, "controller", pr_controller); A(mg, "valueChangedHandler", pr_value_h); A(mg, "setValueChangedHandler:", pr_set_value_h);
    A(mg, "dpad", pr_dpad); A(mg, "buttonA", pr_a); A(mg, "buttonX", pr_x); A(mg, "buttonMenu", pr_menu);
    A(mg, "reportsAbsoluteDpadValues", pr_micro_abs); A(mg, "setReportsAbsoluteDpadValues:", pr_set_micro_abs);
    A(mg, "allowsRotation", pr_micro_rot); A(mg, "setAllowsRotation:", pr_set_micro_rot); A(mg, "saveSnapshot", pr_self);

    /* dispositivos */
    K(&GCDeviceHaptics_c, &GCDeviceHaptics_m, "GCDeviceHaptics", obj, (u32)(sizeof(gchap_t) - 8));
    A(&GCDeviceHaptics_c, "supportedLocalities", hap_localities); A(&GCDeviceHaptics_c, "createEngineWithLocality:", hap_create_engine);
    K(&GCColor_c, &GCColor_m, "GCColor", obj, (u32)(sizeof(gccolor_t) - 8));
    A(&GCColor_c, "initWithRed:green:blue:", col_init); A(&GCColor_c, "red", col_r); A(&GCColor_c, "green", col_g); A(&GCColor_c, "blue", col_b);
    K(&GCDeviceLight_c, &GCDeviceLight_m, "GCDeviceLight", obj, (u32)(sizeof(gclight_t) - 8));
    A(&GCDeviceLight_c, "color", light_color); A(&GCDeviceLight_c, "setColor:", light_set_color);
    K(&GCDeviceBattery_c, &GCDeviceBattery_m, "GCDeviceBattery", obj, 0);
    A(&GCDeviceBattery_c, "batteryLevel", bat_level); A(&GCDeviceBattery_c, "batteryState", bat_state);

    K(&GCController_c, &GCController_m, "GCController", obj, (u32)(sizeof(gcctl_t) - 8));
    cx_class_t *gc = &GCController_c;
    AC(gc, "controllers", ctl_controllers); AC(gc, "current", ctl_current);
    AC(gc, "startWirelessControllerDiscoveryWithCompletionHandler:", ctl_discovery); AC(gc, "stopWirelessControllerDiscovery", ctl_stop_discovery);
    AC(gc, "shouldMonitorBackgroundEvents", ctl_should_bg); AC(gc, "setShouldMonitorBackgroundEvents:", ctl_set_should_bg);
    AC(gc, "supportsHIDDevice:", ctl_supports_hid); AC(gc, "controllerWithExtendedGamepad", ctl_with_ext); AC(gc, "controllerWithMicroGamepad", ctl_with_ext);
    A(gc, "vendorName", ctl_vendor); A(gc, "productCategory", ctl_category); A(gc, "playerIndex", ctl_player); A(gc, "setPlayerIndex:", ctl_set_player);
    A(gc, "extendedGamepad", ctl_ext); A(gc, "microGamepad", ctl_micro); A(gc, "gamepad", ctl_nil); A(gc, "motion", ctl_nil);
    A(gc, "haptics", ctl_haptics); A(gc, "light", ctl_light); A(gc, "battery", ctl_battery); A(gc, "physicalInputProfile", ctl_profile);
    A(gc, "isAttachedToDevice", ctl_attached); A(gc, "isSnapshot", ctl_snapshot); A(gc, "handlerQueue", ctl_queue); A(gc, "setHandlerQueue:", ctl_set_queue);
    A(gc, "controllerPausedHandler", ctl_paused); A(gc, "setControllerPausedHandler:", ctl_set_paused); A(gc, "capture", ctl_capture);
    K(&GCMOSPoller_c, &GCMOSPoller_m, "GCMOSPoller", obj, 0);
    A(&GCMOSPoller_c, "tick:", poller_tick);

    K(&GCKeyboard_c, &GCKeyboard_m, "GCKeyboard", obj, (u32)(sizeof(gcdev_t) - 8));
    AC(&GCKeyboard_c, "coalescedKeyboard", kb_coalesced);
    A(&GCKeyboard_c, "keyboardInput", dev_input); A(&GCKeyboard_c, "vendorName", dev_vendor); A(&GCKeyboard_c, "productCategory", dev_category_kbd);
    A(&GCKeyboard_c, "handlerQueue", dev_queue); A(&GCKeyboard_c, "setHandlerQueue:", dev_set_queue);
    K(&GCKeyboardInput_c, &GCKeyboardInput_m, "GCKeyboardInput", obj, (u32)(sizeof(gckbd_t) - 8));
    A(&GCKeyboardInput_c, "keyChangedHandler", ki_key_h); A(&GCKeyboardInput_c, "setKeyChangedHandler:", ki_set_key_h);
    A(&GCKeyboardInput_c, "valueDidChangeHandler", ki_value_h); A(&GCKeyboardInput_c, "setValueDidChangeHandler:", ki_set_value_h);
    A(&GCKeyboardInput_c, "isAnyKeyPressed", ki_any); A(&GCKeyboardInput_c, "buttonForKeyCode:", ki_button); A(&GCKeyboardInput_c, "device", ki_device);
    K(&GCMouse_c, &GCMouse_m, "GCMouse", obj, (u32)(sizeof(gcdev_t) - 8));
    AC(&GCMouse_c, "current", ms_current); AC(&GCMouse_c, "mice", ms_mice);
    A(&GCMouse_c, "mouseInput", dev_input); A(&GCMouse_c, "vendorName", dev_vendor); A(&GCMouse_c, "productCategory", dev_category_mouse);
    A(&GCMouse_c, "handlerQueue", dev_queue); A(&GCMouse_c, "setHandlerQueue:", dev_set_queue);
    K(&GCMouseInput_c, &GCMouseInput_m, "GCMouseInput", obj, (u32)(sizeof(gcmouse_t) - 8));
    A(&GCMouseInput_c, "leftButton", mi_left); A(&GCMouseInput_c, "rightButton", mi_right); A(&GCMouseInput_c, "middleButton", mi_middle);
    A(&GCMouseInput_c, "scroll", mi_scroll); A(&GCMouseInput_c, "auxiliaryButtons", mi_aux); A(&GCMouseInput_c, "device", mi_device);
    A(&GCMouseInput_c, "mouseMovedHandler", mi_moved_h); A(&GCMouseInput_c, "setMouseMovedHandler:", mi_set_moved_h);

    /* ---- CoreHaptics */
    CHHapticEventTypeHapticTransient = S("HapticTransient"); CHHapticEventTypeHapticContinuous = S("HapticContinuous");
    CHHapticEventTypeAudioContinuous = S("AudioContinuous"); CHHapticEventTypeAudioCustom = S("AudioCustom");
    CHHapticEventParameterIDHapticIntensity = S("HapticIntensity"); CHHapticEventParameterIDHapticSharpness = S("HapticSharpness");
    CHHapticEventParameterIDAttackTime = S("AttackTime"); CHHapticEventParameterIDDecayTime = S("DecayTime");
    CHHapticEventParameterIDReleaseTime = S("ReleaseTime"); CHHapticEventParameterIDSustained = S("Sustained");
    CHHapticEventParameterIDAudioVolume = S("AudioVolume"); CHHapticEventParameterIDAudioPitch = S("AudioPitch");
    CHHapticEventParameterIDAudioPan = S("AudioPan"); CHHapticEventParameterIDAudioBrightness = S("AudioBrightness");
    CHHapticDynamicParameterIDHapticIntensityControl = S("HapticIntensityControl"); CHHapticDynamicParameterIDHapticSharpnessControl = S("HapticSharpnessControl");
    CHHapticDynamicParameterIDHapticAttackTimeControl = S("HapticAttackTimeControl"); CHHapticDynamicParameterIDHapticDecayTimeControl = S("HapticDecayTimeControl");
    CHHapticDynamicParameterIDHapticReleaseTimeControl = S("HapticReleaseTimeControl"); CHHapticDynamicParameterIDAudioVolumeControl = S("AudioVolumeControl");
    CHHapticDynamicParameterIDAudioPanControl = S("AudioPanControl"); CHHapticDynamicParameterIDAudioBrightnessControl = S("AudioBrightnessControl");
    CHHapticDynamicParameterIDAudioPitchControl = S("AudioPitchControl"); CHHapticDynamicParameterIDAudioAttackTimeControl = S("AudioAttackTimeControl");
    CHHapticDynamicParameterIDAudioDecayTimeControl = S("AudioDecayTimeControl"); CHHapticDynamicParameterIDAudioReleaseTimeControl = S("AudioReleaseTimeControl");
    CHHapticPatternKeyVersion = S("Version"); CHHapticPatternKeyPattern = S("Pattern"); CHHapticPatternKeyEvent = S("Event");
    CHHapticPatternKeyEventType = S("EventType"); CHHapticPatternKeyTime = S("Time"); CHHapticPatternKeyEventDuration = S("EventDuration");
    CHHapticPatternKeyEventWaveformPath = S("EventWaveformPath"); CHHapticPatternKeyEventParameters = S("EventParameters");
    CHHapticPatternKeyParameter = S("Parameter"); CHHapticPatternKeyParameterID = S("ParameterID"); CHHapticPatternKeyParameterValue = S("ParameterValue");
    CHHapticPatternKeyParameterCurve = S("ParameterCurve"); CHHapticPatternKeyParameterCurveControlPoints = S("ParameterCurveControlPoints");
    CHHapticPatternKeyEventWaveformUseVolumeEnvelope = S("EventWaveformUseVolumeEnvelope"); CHHapticPatternKeyEventWaveformLoopEnabled = S("EventWaveformLoopEnabled");
    CHHapticAudioResourceKeyUseVolumeEnvelope = S("UseVolumeEnvelope"); CHHapticAudioResourceKeyLoopEnabled = S("LoopEnabled");
    CHHapticErrorDomain = S("com.apple.CoreHaptics");

    K(&CHHapticEventParameter_c, &CHHapticEventParameter_m, "CHHapticEventParameter", obj, (u32)(sizeof(chparam_t) - 8));
    A(&CHHapticEventParameter_c, "initWithParameterID:value:", prm_init); A(&CHHapticEventParameter_c, "parameterID", prm_id);
    A(&CHHapticEventParameter_c, "value", prm_value); A(&CHHapticEventParameter_c, "setValue:", prm_set_value);
    K(&CHHapticDynamicParameter_c, &CHHapticDynamicParameter_m, "CHHapticDynamicParameter", obj, (u32)(sizeof(chparam_t) - 8));
    A(&CHHapticDynamicParameter_c, "initWithParameterID:value:relativeTime:", prm_init_time); A(&CHHapticDynamicParameter_c, "parameterID", prm_id);
    A(&CHHapticDynamicParameter_c, "value", prm_value); A(&CHHapticDynamicParameter_c, "setValue:", prm_set_value);
    A(&CHHapticDynamicParameter_c, "relativeTime", prm_time); A(&CHHapticDynamicParameter_c, "setRelativeTime:", prm_set_time);
    K(&CHHapticParameterCurveControlPoint_c, &CHHapticParameterCurveControlPoint_m, "CHHapticParameterCurveControlPoint", obj, (u32)(sizeof(chpoint_t) - 8));
    A(&CHHapticParameterCurveControlPoint_c, "initWithRelativeTime:value:", cp_init); A(&CHHapticParameterCurveControlPoint_c, "relativeTime", cp_time);
    A(&CHHapticParameterCurveControlPoint_c, "setRelativeTime:", cp_set_time); A(&CHHapticParameterCurveControlPoint_c, "value", cp_value);
    A(&CHHapticParameterCurveControlPoint_c, "setValue:", cp_set_value);
    K(&CHHapticParameterCurve_c, &CHHapticParameterCurve_m, "CHHapticParameterCurve", obj, (u32)(sizeof(chcurve_t) - 8));
    A(&CHHapticParameterCurve_c, "initWithParameterID:controlPoints:relativeTime:", cv_init); A(&CHHapticParameterCurve_c, "parameterID", cv_id);
    A(&CHHapticParameterCurve_c, "controlPoints", cv_points); A(&CHHapticParameterCurve_c, "relativeTime", cv_time); A(&CHHapticParameterCurve_c, "setRelativeTime:", cv_set_time);
    K(&CHHapticEvent_c, &CHHapticEvent_m, "CHHapticEvent", obj, (u32)(sizeof(chevent_t) - 8));
    cx_class_t *he = &CHHapticEvent_c;
    A(he, "initWithEventType:parameters:relativeTime:", ev_init); A(he, "initWithEventType:parameters:relativeTime:duration:", ev_init_dur);
    A(he, "initWithAudioResourceID:parameters:relativeTime:", ev_init_res); A(he, "initWithAudioResourceID:parameters:relativeTime:duration:", ev_init_res_dur);
    A(he, "type", ev_type); A(he, "eventParameters", ev_params); A(he, "relativeTime", ev_time); A(he, "setRelativeTime:", ev_set_time);
    A(he, "duration", ev_duration); A(he, "setDuration:", ev_set_duration);
    K(&CHHapticPattern_c, &CHHapticPattern_m, "CHHapticPattern", obj, (u32)(sizeof(chpattern_t) - 8));
    cx_class_t *hp = &CHHapticPattern_c;
    A(hp, "initWithEvents:parameters:error:", pat_init); A(hp, "initWithEvents:parameterCurves:error:", pat_init_curves);
    A(hp, "initWithDictionary:error:", pat_init_dict); A(hp, "initWithContentsOfURL:error:", pat_init_url);
    A(hp, "duration", pat_duration); A(hp, "exportDictionaryAndReturnError:", pat_export);
    K(&CHHapticDeviceCapabilityMOS_c, &CHHapticDeviceCapabilityMOS_m, "CHHapticDeviceCapabilityMOS", obj, (u32)(sizeof(chcap_t) - 8));
    A(&CHHapticDeviceCapabilityMOS_c, "supportsHaptics", cap_haptics); A(&CHHapticDeviceCapabilityMOS_c, "supportsAudio", cap_audio);
    A(&CHHapticDeviceCapabilityMOS_c, "attributesForEventParameter:eventType:error:", cap_attrs);
    A(&CHHapticDeviceCapabilityMOS_c, "attributesForDynamicParameter:error:", cap_attrs);
    K(&CHHapticParameterAttributesMOS_c, &CHHapticParameterAttributesMOS_m, "CHHapticParameterAttributesMOS", obj, (u32)(sizeof(chattr_t) - 8));
    A(&CHHapticParameterAttributesMOS_c, "minValue", attr_min); A(&CHHapticParameterAttributesMOS_c, "maxValue", attr_max);
    A(&CHHapticParameterAttributesMOS_c, "defaultValue", attr_dflt);
    K(&CHHapticEngine_c, &CHHapticEngine_m, "CHHapticEngine", obj, (u32)(sizeof(chengine_t) - 8));
    cx_class_t *en = &CHHapticEngine_c;
    AC(en, "capabilitiesForHardware", eng_caps);
    A(en, "initAndReturnError:", eng_init); A(en, "initWithAudioSession:error:", eng_init_session);
    A(en, "startAndReturnError:", eng_start); A(en, "startWithCompletionHandler:", eng_start_cb);
    A(en, "stopWithCompletionHandler:", eng_stop_cb); A(en, "stop", eng_stop); A(en, "currentTime", eng_current);
    A(en, "stoppedHandler", eng_stopped_h); A(en, "setStoppedHandler:", eng_set_stopped_h);
    A(en, "resetHandler", eng_reset_h); A(en, "setResetHandler:", eng_set_reset_h);
    A(en, "isMutedForAudio", eng_muted_audio); A(en, "setIsMutedForAudio:", eng_set_muted_audio);
    A(en, "isMutedForHaptics", eng_muted_haptics); A(en, "setIsMutedForHaptics:", eng_set_muted_haptics);
    A(en, "playsHapticsOnly", eng_haptics_only); A(en, "setPlaysHapticsOnly:", eng_set_haptics_only);
    A(en, "playsAudioOnly", eng_plays_audio_only);
    A(en, "isAutoShutdownEnabled", eng_auto_shutdown); A(en, "setAutoShutdownEnabled:", eng_set_auto_shutdown);
    A(en, "notifyWhenPlayersFinished:", eng_notify_finished);
    A(en, "createPlayerWithPattern:error:", eng_player); A(en, "createAdvancedPlayerWithPattern:error:", eng_adv_player);
    A(en, "playPatternFromURL:error:", eng_play_url); A(en, "playPatternFromData:error:", eng_play_data);
    A(en, "registerAudioResource:options:error:", eng_register_audio); A(en, "unregisterAudioResource:error:", eng_unregister_audio);
    A(en, "capabilities", eng_caps_inst);
    K(&CHHapticPatternPlayerMOS_c, &CHHapticPatternPlayerMOS_m, "CHHapticPatternPlayerMOS", obj, (u32)(sizeof(chplayer_t) - 8));
    cx_class_t *pl = &CHHapticPatternPlayerMOS_c;
    A(pl, "startAtTime:error:", pl_start); A(pl, "stopAtTime:error:", pl_stop); A(pl, "cancelAndReturnError:", pl_cancel);
    A(pl, "sendParameters:atTime:error:", pl_send); A(pl, "scheduleParameterCurve:atTime:error:", pl_schedule);
    A(pl, "isMuted", pl_muted); A(pl, "setIsMuted:", pl_set_muted);
    A(pl, "pauseAtTime:error:", pl_pause); A(pl, "resumeAtTime:error:", pl_resume); A(pl, "seekToOffset:error:", pl_seek);
    A(pl, "loopEnabled", pl_loop); A(pl, "setLoopEnabled:", pl_set_loop); A(pl, "loopEnd", pl_loop_end); A(pl, "setLoopEnd:", pl_set_loop_end);
    A(pl, "playbackRate", pl_rate); A(pl, "setPlaybackRate:", pl_set_rate); A(pl, "currentTime", pl_current);
    A(pl, "completionHandler", pl_completion); A(pl, "setCompletionHandler:", pl_set_completion);
    K(&CHHapticMOSTicker_c, &CHHapticMOSTicker_m, "CHHapticMOSTicker", obj, 0);
    A(&CHHapticMOSTicker_c, "tick:", haptic_tick);
}
