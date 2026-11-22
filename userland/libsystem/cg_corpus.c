/*
 * mOS libSystem — o que o corpus de apps pede do CoreGraphics de telas e eventos e não existia
 * (docs/lacunas/camila.md, ordenado por quantos apps importam):
 *   - CGDisplayMode como objeto CF de verdade (CFRetain/CFRelease/CFGetTypeID e o cast do Swift funcionam);
 *     a tela do mOS tem um modo só, o atual, que vem do tamanho da tela do kernel (cgdisplay.c);
 *   - lista de modos, troca de modo (só aceita o atual), captura de tela, transações de configuração;
 *   - tabela de gamma (guardada por processo, como o macOS restaura ao sair: o compositor não aplica gamma);
 *   - cursor (esconder/mostrar pelo NSCursor do appkit_cursor.c, mover pelo CGWarpMouseCursorPosition);
 *   - estado do mouse por fonte de evento, contadores, supressão de eventos locais (sem efeito: o mOS não suprime);
 *   - CGS privado que gerenciadores de janela e barras de menu usam: um Space só (id 1, tipo usuário);
 *   - IODisplayCreateInfoDictionary e o resto de IOKit de tela e de HID que vem junto nesses apps.
 * O IOKit do mOS não tem registro de dispositivos (mach.c): iteradores voltam vazios, entradas ausentes dão
 * kIOReturnNoDevice, e quem pergunta pela tela recebe o dicionário do EDID (displayservices.c).
 */
#include "foundation.h"
#include "sys.h"
#include "../../kernel/include/compat_gui.h"
#include "computeruse_win.h"

id objc_retain(id o);
void objc_release(id o);
void *memset(void *d, int c, size_t n);
void *memcpy(void *d, const void *s, size_t n);
size_t strlen(const char *s);
char *strncpy(char *d, const char *s, size_t n);
int strcmp(const char *a, const char *b);
void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void free(void *p);

typedef struct { double x, y; } cgpt_t;
typedef struct { double x, y, w, h; } cgrect_t;
typedef u32 CGDirectDisplayID;
typedef i32 CGError;
enum { kCGErrorSuccess = 0, kCGErrorFailure = 1000, kCGErrorIllegalArgument = 1001, kCGErrorRangeCheck = 1007 };
#define MAIN_DISPLAY 1u

/* ---- constantes CFString */
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } cg_cfconst_t;
#define CFCONST(var, text) \
    static cg_cfconst_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; \
    const void *var = &var##_s;
CFCONST(kCGDisplayShowDuplicateLowResolutionModes, "kCGDisplayShowDuplicateLowResolutionModes")
CFCONST(kCGDisplayModeIsSafeForHardware, "kCGDisplayModeIsSafeForHardware")
CFCONST(kCGDisplayModeIsInterlaced, "kCGDisplayModeIsInterlaced")
CFCONST(kCGDisplayModeIsStretched, "kCGDisplayModeIsStretched")
CFCONST(kCGDisplayModeIsTelevisionOutput, "kCGDisplayModeIsTelevisionOutput")
/* IO32BitDirectPixels: o modo do compositor (BGRA de 8 bits) */
CFCONST(cg_pixenc_32, "--------RRRRRRRRGGGGGGGGBBBBBBBB")
CFCONST(cg_ac_power, "AC Power")
CFCONST(cg_ioservice, "IOService")

/* ---- CGDisplayMode */
size_t CGDisplayPixelsWide(u32 d);
size_t CGDisplayPixelsHigh(u32 d);
typedef struct {
    cx_class_t *isa;
    u32 w, h;           /* pontos */
    u32 pw, ph;         /* pixels */
    double hz;
    u32 ioflags;
    i32 mode_id;
} cgmode_t;
cx_class_t CGDisplayModeMOS_c __asm__("OBJC_CLASS_$_CGDisplayModeMOS"),
    CGDisplayModeMOS_m __asm__("OBJC_METACLASS_$_CGDisplayModeMOS");
static int mode_ready;
#define CG_MODE_TYPEID 0x2B0
static u64 mode_tid(id self, SEL s) { (void)self; (void)s; return CG_MODE_TYPEID; }
static BOOL mode_equal(id self, SEL s, id o);
static u64 mode_hash(id self, SEL s) { (void)s; cgmode_t *m = (cgmode_t *)self; return (u64)m->pw * 31 + m->ph; }
static cgmode_t *mode_of(const void *p) {
    return p && mode_ready && cx_is_subclass(object_getClass((id)p), &CGDisplayModeMOS_c) ? (cgmode_t *)p : 0;
}
static BOOL mode_equal(id self, SEL s, id o) {
    (void)s;
    cgmode_t *a = (cgmode_t *)self, *b = mode_of(o);
    return b && a->w == b->w && a->h == b->h && a->pw == b->pw && a->ph == b->ph && a->hz == b->hz;
}
u64 CGDisplayModeGetTypeID(void) { return CG_MODE_TYPEID; }
/* o modo atual da tela principal, novo a cada chamada (+1), como o CGDisplayCopyDisplayMode */
void *cx_cg_current_mode(u32 d) {
    if (d != MAIN_DISPLAY && d != 0) return 0;
    if (!mode_ready) {
        cx_class_t *ns = cx_class_named("NSObject");
        if (!ns) return 0;
        cx_define_class(&CGDisplayModeMOS_c, &CGDisplayModeMOS_m, "CGDisplayModeMOS", ns, (u32)(sizeof(cgmode_t) - sizeof(void *)));
        cx_add_method(&CGDisplayModeMOS_c, "_cfTypeID", (void *)mode_tid);
        cx_add_method(&CGDisplayModeMOS_c, "isEqual:", (void *)mode_equal);
        cx_add_method(&CGDisplayModeMOS_c, "hash", (void *)mode_hash);
        mode_ready = 1;
    }
    cgmode_t *m = (cgmode_t *)cx_alloc_instance(&CGDisplayModeMOS_c);
    if (!m) return 0;
    m->w = m->pw = (u32)CGDisplayPixelsWide(MAIN_DISPLAY);
    m->h = m->ph = (u32)CGDisplayPixelsHigh(MAIN_DISPLAY);
    m->hz = 60.0;
    m->ioflags = 0x00000007 | 0x02000000;   /* kDisplayModeValidFlag|SafeFlag|DefaultFlag|NativeFlag */
    m->mode_id = 1;
    return m;
}
size_t cx_cg_mode_width(const void *mode) { cgmode_t *m = mode_of(mode); return m ? m->w : CGDisplayPixelsWide(MAIN_DISPLAY); }
size_t cx_cg_mode_height(const void *mode) { cgmode_t *m = mode_of(mode); return m ? m->h : CGDisplayPixelsHigh(MAIN_DISPLAY); }
size_t cx_cg_mode_pixel_width(const void *mode) { cgmode_t *m = mode_of(mode); return m ? m->pw : CGDisplayPixelsWide(MAIN_DISPLAY); }
size_t cx_cg_mode_pixel_height(const void *mode) { cgmode_t *m = mode_of(mode); return m ? m->ph : CGDisplayPixelsHigh(MAIN_DISPLAY); }
double cx_cg_mode_refresh(const void *mode) { cgmode_t *m = mode_of(mode); return m ? m->hz : 60.0; }
void *CGDisplayModeRetain(void *mode) { if (mode_of(mode)) objc_retain((id)mode); return mode; }
u32 CGDisplayModeGetIOFlags(void *mode) { cgmode_t *m = mode_of(mode); return m ? m->ioflags : 0; }
i32 CGDisplayModeGetIODisplayModeID(void *mode) { cgmode_t *m = mode_of(mode); return m ? m->mode_id : 0; }
u8 CGDisplayModeIsUsableForDesktopGUI(void *mode) { return mode_of(mode) != 0; }
id CGDisplayModeCopyPixelEncoding(void *mode) { return mode_of(mode) ? objc_retain((id)cg_pixenc_32) : 0; }
static int is_current(void *mode) {
    cgmode_t *m = mode_of(mode);
    return m && m->pw == CGDisplayPixelsWide(MAIN_DISPLAY) && m->ph == CGDisplayPixelsHigh(MAIN_DISPLAY);
}

extern const char kCFTypeArrayCallBacks[];
id CFArrayCreate(void *alloc, const id *vals, long n, const void *cb);
id CGDisplayCopyAllDisplayModes(u32 d, id options) {
    (void)options;
    id m = (id)cx_cg_current_mode(d);
    if (!m) return 0;
    id a = CFArrayCreate(0, &m, 1, &kCFTypeArrayCallBacks);
    objc_release(m);
    return a;
}
CGError CGDisplaySetDisplayMode(u32 d, void *mode, id options) {
    (void)options;
    if (d != MAIN_DISPLAY) return kCGErrorIllegalArgument;
    return is_current(mode) ? kCGErrorSuccess : kCGErrorIllegalArgument;   /* o mOS tem um modo só */
}

/* ---- captura exclusiva da tela (jogos, emuladores em tela cheia): o compositor continua desenhando o desktop;
 * a janela do app cobre a tela com o nível CGShieldingWindowLevel */
static int g_captured;
CGError CGDisplayCapture(u32 d) { if (d != MAIN_DISPLAY) return kCGErrorIllegalArgument; g_captured = 1; return kCGErrorSuccess; }
CGError CGDisplayCaptureWithOptions(u32 d, u32 opts) { (void)opts; return CGDisplayCapture(d); }
CGError CGDisplayRelease(u32 d) { if (d != MAIN_DISPLAY) return kCGErrorIllegalArgument; g_captured = 0; return kCGErrorSuccess; }
CGError CGCaptureAllDisplays(void) { g_captured = 1; return kCGErrorSuccess; }
CGError CGCaptureAllDisplaysWithOptions(u32 opts) { (void)opts; g_captured = 1; return kCGErrorSuccess; }
CGError CGReleaseAllDisplays(void) { g_captured = 0; return kCGErrorSuccess; }
u32 CGDisplayIsCaptured(u32 d) { return d == MAIN_DISPLAY && g_captured; }
u32 CGShieldingWindowID(u32 d) { (void)d; return 0; }

/* ---- transações de configuração: só o modo atual, sem espelhamento, origem (0,0) */
typedef struct { u32 magic; int bad; } cgconfig_t;
#define CONFIG_MAGIC 0x43474346u
static cgconfig_t *config_of(void *r) { cgconfig_t *c = r; return c && c->magic == CONFIG_MAGIC ? c : 0; }
CGError CGBeginDisplayConfiguration(void **ref) {
    if (!ref) return kCGErrorIllegalArgument;
    cgconfig_t *c = calloc(1, sizeof *c);
    if (!c) return kCGErrorFailure;
    c->magic = CONFIG_MAGIC;
    *ref = c;
    return kCGErrorSuccess;
}
static CGError config_end(void *ref) {
    cgconfig_t *c = config_of(ref);
    if (!c) return kCGErrorIllegalArgument;
    int bad = c->bad;
    c->magic = 0;
    free(c);
    return bad ? kCGErrorIllegalArgument : kCGErrorSuccess;
}
CGError CGCompleteDisplayConfiguration(void *ref, u32 option) { (void)option; return config_end(ref); }
CGError CGCancelDisplayConfiguration(void *ref) { config_end(ref); return kCGErrorSuccess; }
CGError CGConfigureDisplayWithDisplayMode(void *ref, u32 d, void *mode, id options) {
    (void)options;
    cgconfig_t *c = config_of(ref);
    if (!c) return kCGErrorIllegalArgument;
    if (d != MAIN_DISPLAY || !is_current(mode)) { c->bad = 1; return kCGErrorIllegalArgument; }
    return kCGErrorSuccess;
}
CGError CGConfigureDisplayMirrorOfDisplay(void *ref, u32 d, u32 master) {
    cgconfig_t *c = config_of(ref);
    if (!c) return kCGErrorIllegalArgument;
    if (master == 0) return kCGErrorSuccess;     /* kCGNullDirectDisplay: desfazer espelho, que não existe */
    (void)d;
    c->bad = 1;
    return kCGErrorIllegalArgument;
}
CGError CGConfigureDisplayOrigin(void *ref, u32 d, i32 x, i32 y) {
    cgconfig_t *c = config_of(ref);
    if (!c) return kCGErrorIllegalArgument;
    if (d != MAIN_DISPLAY || x != 0 || y != 0) { c->bad = 1; return kCGErrorIllegalArgument; }
    return kCGErrorSuccess;
}
CGError CGConfigureDisplayStereoOperation(void *ref, u32 d, u32 stereo, u32 force) {
    (void)d; (void)force;
    if (!config_of(ref)) return kCGErrorIllegalArgument;
    return stereo ? kCGErrorRangeCheck : kCGErrorSuccess;
}
void CGRestorePermanentDisplayConfiguration(void);
u32 CGDisplayIsInHWMirrorSet(u32 d) { (void)d; return 0; }
u32 CGDisplayPrimaryDisplay(u32 d) { return d; }
u32 CGDisplayIsAlwaysInMirrorSet(u32 d) { (void)d; return 0; }
u32 CGDisplayIsStereo(u32 d) { (void)d; return 0; }

/* ---- gamma: tabela por processo. O compositor não aplica (sem LUT de hardware exposta), mas ler devolve o que foi
 * gravado, e restaurar volta à identidade, que é o que os apps conferem. */
#define GAMMA_CAP 1024
typedef float CGGammaValue;
static CGGammaValue g_gamma[3][GAMMA_CAP];
static u32 g_gamma_n;     /* 0 = identidade */
static float formula_v[9] = {0, 1, 1, 0, 1, 1, 0, 1, 1};   /* min, max, gamma de R, G, B */
u32 CGDisplayGammaTableCapacity(u32 d) { (void)d; return GAMMA_CAP; }
CGError CGGetDisplayTransferByTable(u32 d, u32 cap, CGGammaValue *r, CGGammaValue *g, CGGammaValue *b, u32 *count) {
    if (d != MAIN_DISPLAY) return kCGErrorIllegalArgument;
    u32 n = g_gamma_n ? g_gamma_n : 256;
    if (n > cap) n = cap;
    for (u32 i = 0; i < n; i++) {
        float id_v = n > 1 ? (float)i / (float)(n - 1) : 0.f;
        if (r) r[i] = g_gamma_n ? g_gamma[0][i] : id_v;
        if (g) g[i] = g_gamma_n ? g_gamma[1][i] : id_v;
        if (b) b[i] = g_gamma_n ? g_gamma[2][i] : id_v;
    }
    if (count) *count = n;
    return kCGErrorSuccess;
}
CGError CGSetDisplayTransferByTable(u32 d, u32 n, const CGGammaValue *r, const CGGammaValue *g, const CGGammaValue *b) {
    if (d != MAIN_DISPLAY) return kCGErrorIllegalArgument;
    if (n > GAMMA_CAP) return kCGErrorRangeCheck;
    if (n && (!r || !g || !b)) return kCGErrorIllegalArgument;
    for (u32 i = 0; i < n; i++) { g_gamma[0][i] = r[i]; g_gamma[1][i] = g[i]; g_gamma[2][i] = b[i]; }
    g_gamma_n = n;
    return kCGErrorSuccess;
}
CGError CGGetDisplayTransferByFormula(u32 d, CGGammaValue *rmin, CGGammaValue *rmax, CGGammaValue *rg,
                                      CGGammaValue *gmin, CGGammaValue *gmax, CGGammaValue *gg,
                                      CGGammaValue *bmin, CGGammaValue *bmax, CGGammaValue *bg) {
    if (d != MAIN_DISPLAY) return kCGErrorIllegalArgument;
    CGGammaValue *o[9] = {rmin, rmax, rg, gmin, gmax, gg, bmin, bmax, bg};
    for (int i = 0; i < 9; i++) if (o[i]) *o[i] = formula_v[i];
    return kCGErrorSuccess;
}
CGError CGSetDisplayTransferByFormula(u32 d, CGGammaValue rmin, CGGammaValue rmax, CGGammaValue rg,
                                      CGGammaValue gmin, CGGammaValue gmax, CGGammaValue gg,
                                      CGGammaValue bmin, CGGammaValue bmax, CGGammaValue bg) {
    if (d != MAIN_DISPLAY) return kCGErrorIllegalArgument;
    float v[9] = {rmin, rmax, rg, gmin, gmax, gg, bmin, bmax, bg};
    for (int i = 0; i < 9; i++) formula_v[i] = v[i];
    return kCGErrorSuccess;
}
CGError CGSetDisplayTransferByByteTable(u32 d, u32 n, const u8 *r, const u8 *g, const u8 *b) {
    if (d != MAIN_DISPLAY) return kCGErrorIllegalArgument;
    if (n > GAMMA_CAP) return kCGErrorRangeCheck;
    if (n && (!r || !g || !b)) return kCGErrorIllegalArgument;
    for (u32 i = 0; i < n; i++) { g_gamma[0][i] = r[i] / 255.f; g_gamma[1][i] = g[i] / 255.f; g_gamma[2][i] = b[i] / 255.f; }
    g_gamma_n = n;
    return kCGErrorSuccess;
}
void CGDisplayRestoreColorSyncSettings(void) {
    g_gamma_n = 0;
    static const float id9[9] = {0, 1, 1, 0, 1, 1, 0, 1, 1};
    for (int i = 0; i < 9; i++) formula_v[i] = id9[i];
}
void CGRestorePermanentDisplayConfiguration(void) { CGDisplayRestoreColorSyncSettings(); g_captured = 0; }

/* ---- cursor */
i32 CGWarpMouseCursorPosition(cgpt_t p);
CGError CGDisplayMoveCursorToPoint(u32 d, cgpt_t p) {
    if (d != MAIN_DISPLAY) return kCGErrorIllegalArgument;
    return CGWarpMouseCursorPosition(p);     /* a tela principal começa em (0,0) */
}
CGError CGDisplayHideCursor(u32 d) { (void)d; MSG0(void, (id)cx_class_named("NSCursor"), "hide"); return kCGErrorSuccess; }
CGError CGDisplayShowCursor(u32 d) { (void)d; MSG0(void, (id)cx_class_named("NSCursor"), "unhide"); return kCGErrorSuccess; }
u32 CGCursorIsVisible(void) { return 1; }
u32 CGCursorIsDrawnInFramebuffer(void) { return 0; }
/* deslocamento desde a última chamada, da posição do mouse que o AppKit acompanha */
typedef struct { double x, y; } nspt_t;
void CGGetLastMouseDelta(i32 *dx, i32 *dy) {
    static nspt_t last; static int have;
    nspt_t p = ((nspt_t (*)(id, SEL))objc_msgSend)((id)cx_class_named("NSEvent"), sel_registerName("mouseLocation"));
    i32 x = have ? (i32)(p.x - last.x) : 0, y = have ? (i32)(last.y - p.y) : 0;   /* y do CG cresce para baixo */
    last = p; have = 1;
    if (dx) *dx = x;
    if (dy) *dy = y;
}

/* ---- estado das fontes de evento */
u8 CGEventSourceButtonState(i32 state, u32 button) {
    (void)state;
    if (button > 31) return 0;
    u64 b = MSG0(u64, (id)cx_class_named("NSEvent"), "pressedMouseButtons");
    return (b >> button) & 1;
}
static u32 g_counters[32];
void cx_cg_count_event(u32 type) { if (type < 32) g_counters[type]++; }
u32 CGEventSourceCounterForEventType(i32 state, u32 type) {
    (void)state;
    if (type == 0xFFFFFFFFu) { u32 t = 0; for (int i = 0; i < 32; i++) t += g_counters[i]; return t; }  /* kCGAnyInputEventType */
    return type < 32 ? g_counters[type] : 0;
}
CGError CGSetLocalEventsSuppressionInterval(double s) { (void)s; return kCGErrorSuccess; }
CGError CGSetLocalEventsFilterDuringSuppressionState(u32 filter, u32 state) { (void)filter; (void)state; return kCGErrorSuccess; }
CGError CGEnableEventStateCombining(u32 on) { (void)on; return kCGErrorSuccess; }
CGError CGAssociateMouseAndMouseCursorPosition(u32 connected);
void CGEventSourceSetLocalEventsFilterDuringSuppressionState(void *src, u32 filter, u32 state) { (void)src; (void)filter; (void)state; }
u32 CGEventSourceGetLocalEventsFilterDuringSuppressionState(void *src, u32 state) { (void)src; (void)state; return 0; }
static u32 g_kbd_type = 40;     /* ANSI */
void CGEventSourceSetKeyboardType(void *src, u32 t) { (void)src; g_kbd_type = t; }
u32 CGEventSourceGetKeyboardType(void *src) { (void)src; return g_kbd_type; }
double CGEventSourceGetLocalEventsSuppressionInterval(void *src) { (void)src; return 0.25; }
i32 CGEventSourceGetSourceStateID(void *src) { (void)src; return 0; }     /* kCGEventSourceStatePrivate */
i64 CGEventSourceGetUserData(void *src) { (void)src; return 0; }
u64 CGEventSourceGetTypeID(void) { return 0x2B1; }
int cx_tcc_preflight(const char *service);
int cx_tcc_request(const char *service);
u8 CGPreflightPostEventAccess(void) { return (u8)cx_tcc_preflight("kTCCServiceAccessibility"); }
u8 CGRequestPostEventAccess(void) { return (u8)cx_tcc_request("kTCCServiceAccessibility"); }
void *CGEventSourceCreate(i32 state);
void *CGEventCreateSourceFromEvent(void *e) { return e ? CGEventSourceCreate(1) : 0; }   /* kCGEventSourceStateCombinedSessionState */
void *CGEventTapCreate(int tap, int place, int opts, u64 mask, void *cb, void *info);
/* tap só dos eventos de um processo: o mOS não filtra por pid; o tap de sessão vê os dele também */
void *CGEventTapCreateForPid(i32 pid, int place, int opts, u64 mask, void *cb, void *info) {
    (void)pid;
    return CGEventTapCreate(1, place, opts, mask, cb, info);   /* kCGSessionEventTap */
}
void *CGEventTapCreateForPSN(void *psn, int place, int opts, u64 mask, void *cb, void *info) {
    (void)psn;
    return CGEventTapCreate(1, place, opts, mask, cb, info);
}
CGError CGGetEventTapList(u32 max, void *list, u32 *count) { (void)max; (void)list; if (count) *count = 0; return kCGErrorSuccess; }
void *CGEventCopyIOHIDEvent(void *e) { (void)e; return 0; }
id CGColorSpaceCopyICCData(void *space);
id CGColorSpaceCopyICCProfile(void *space) { return CGColorSpaceCopyICCData(space); }

/* ---- NXEventStatus (IOKit/hidsystem, legado) */
typedef u32 NXEventHandle;
NXEventHandle NXOpenEventStatus(void) { return 1; }
void NXCloseEventStatus(NXEventHandle h) { (void)h; }
double NXClickTime(NXEventHandle h) { (void)h; return 0.5; }
void NXGetClickSpace(NXEventHandle h, void *area) { (void)h; if (area) { i32 *a = area; a[0] = 4; a[1] = 4; } }
double NXKeyRepeatInterval(NXEventHandle h) { (void)h; return 1.0 / 12; }
double NXKeyRepeatThreshold(NXEventHandle h) { (void)h; return 0.5; }

/* ---- CGDisplayStream com fila de dispatch: a entrega continua na thread de captura (cgdisplaystream.c);
 * o handler é o mesmo bloco */
void *CGDisplayStreamCreate(unsigned display, size_t w, size_t h, int fmt, id props, void *handler);
void *CGDisplayStreamCreateWithDispatchQueue(unsigned display, size_t w, size_t h, int fmt, id props, void *queue, void *handler) {
    (void)queue;
    return CGDisplayStreamCreate(display, w, h, fmt, props, handler);
}
u64 CGDisplayStreamGetTypeID(void) { return 0x2B2; }
u64 CGDisplayStreamUpdateGetTypeID(void) { return 0x2B3; }
size_t CGDisplayStreamUpdateGetDropCount(void *u) { (void)u; return 0; }
void CGDisplayStreamUpdateGetMovedRectsDelta(void *u, double *dx, double *dy) { (void)u; if (dx) *dx = 0; if (dy) *dy = 0; }

/* ---- CGS privado (window server): um Space só */
u32 CGSMainConnectionID(void);
u32 CGSDefaultConnectionForThread(void) { return CGSMainConnectionID(); }
#define MOS_SPACE 1
u64 CGSGetActiveSpace(u32 cid) { (void)cid; return MOS_SPACE; }
i32 CGSSpaceGetType(u32 cid, u64 sid) { (void)cid; (void)sid; return 0; }     /* kCGSSpaceUser */
id cx_num_i(i64 v);
static id num_array1(i64 v) {
    id n = cx_num_i(v);
    return CFArrayCreate(0, &n, 1, &kCFTypeArrayCallBacks);
}
id CGSCopySpacesForWindows(u32 cid, i32 mask, id wids) {
    (void)cid; (void)mask;
    long n = wids ? MSG0(long, wids, "count") : 0;
    if (n > 0) return num_array1(MOS_SPACE);
    return CFArrayCreate(0, 0, 0, &kCFTypeArrayCallBacks);
}
id CGSCopySpaces(u32 cid, i32 mask) { (void)cid; (void)mask; return num_array1(MOS_SPACE); }
static id space_dict(void) {
    id d = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary");
    MSG(void, d, "setObject:forKey:", cx_num_i(MOS_SPACE), cx_str("ManagedSpaceID"));
    MSG(void, d, "setObject:forKey:", cx_num_i(MOS_SPACE), cx_str("id64"));
    MSG(void, d, "setObject:forKey:", cx_num_i(0), cx_str("type"));
    MSG(void, d, "setObject:forKey:", cx_str("mOS-space-1"), cx_str("uuid"));
    return d;
}
id CGSCopyManagedDisplaySpaces(u32 cid) {
    (void)cid;
    id sp = space_dict();
    id disp = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary");
    MSG(void, disp, "setObject:forKey:", cx_str("Main"), cx_str("Display Identifier"));
    MSG(void, disp, "setObject:forKey:", sp, cx_str("Current Space"));
    id spaces = MSG(id, (id)cx_class_named("NSArray"), "arrayWithObject:", sp);
    MSG(void, disp, "setObject:forKey:", spaces, cx_str("Spaces"));
    return CFArrayCreate(0, &disp, 1, &kCFTypeArrayCallBacks);
}
id CGSCopyManagedDisplayForSpace(u32 cid, u64 sid) { (void)cid; (void)sid; return objc_retain(cx_str("Main")); }
id CGSCopyActiveMenuBarDisplayIdentifier(u32 cid) { (void)cid; return objc_retain(cx_str("Main")); }
u64 CGSManagedDisplayGetCurrentSpace(u32 cid, id display) { (void)cid; (void)display; return MOS_SPACE; }
CGError CGSAddWindowsToSpaces(u32 cid, id wids, id sids) { (void)cid; (void)wids; (void)sids; return kCGErrorSuccess; }
CGError CGSRemoveWindowsFromSpaces(u32 cid, id wids, id sids) { (void)cid; (void)wids; (void)sids; return kCGErrorSuccess; }
CGError CGSSetWindowBackgroundBlurRadius(u32 cid, u32 wid, i32 radius) { (void)cid; (void)wid; (void)radius; return kCGErrorSuccess; }
CGError CGSSetWindowBackgroundBlurRadiusStyle(u32 cid, u32 wid, i32 radius, i32 style) { (void)cid; (void)wid; (void)radius; (void)style; return kCGErrorSuccess; }
CGError CGSShutdownServerConnections(void) { return kCGErrorSuccess; }
CGError CGSGetScreenRectForWindow(u32 cid, u32 wid, cgrect_t *out) {
    (void)cid;
    if (!out) return kCGErrorIllegalArgument;
    gui_cgwindow_t w;
    for (int i = 0; cx_cg_window_at(i, &w); i++)
        if (w.number == wid) { out->x = w.x; out->y = w.y; out->w = w.w; out->h = w.h; return kCGErrorSuccess; }
    return kCGErrorIllegalArgument;
}
CGError CGSGetWindowBounds(u32 cid, u32 wid, cgrect_t *out) { return CGSGetScreenRectForWindow(cid, wid, out); }
CGError CGSGetWindowLevel(u32 cid, u32 wid, i32 *level) {
    (void)cid;
    if (!level) return kCGErrorIllegalArgument;
    gui_cgwindow_t w;
    for (int i = 0; cx_cg_window_at(i, &w); i++)
        if (w.number == wid) { *level = w.layer; return kCGErrorSuccess; }
    *level = 0;
    return kCGErrorIllegalArgument;
}
CGError CGSGetWindowOwner(u32 cid, u32 wid, u32 *owner) { (void)wid; if (owner) *owner = cid; return kCGErrorSuccess; }
CGError CGSGetConnectionPSN(u32 cid, void *psn) { (void)cid; if (psn) memset(psn, 0, 8); return kCGErrorSuccess; }
u8 CGSEventIsAppUnresponsive(u32 cid, const void *psn) { (void)cid; (void)psn; return 0; }
/* atalhos simbólicos do sistema (Cmd-Tab, Spotlight...): o mOS não tem tabela; ficam "habilitados" e o pedido de
 * desligar é lembrado, para que o app leia de volta o que gravou */
#define HOTKEYS 256
static u8 hk_off[HOTKEYS];
CGError CGSSetSymbolicHotKeyEnabled(i32 hk, u8 on) { if (hk >= 0 && hk < HOTKEYS) hk_off[hk] = !on; return kCGErrorSuccess; }
u8 CGSIsSymbolicHotKeyEnabled(i32 hk) { return hk >= 0 && hk < HOTKEYS ? !hk_off[hk] : 0; }
CGError CGSGetSymbolicHotKeyValue(i32 hk, u16 *key_equiv, u16 *vkey, u32 *mods) {
    (void)hk;
    if (key_equiv) *key_equiv = 0xFFFF;
    if (vkey) *vkey = 0xFFFF;
    if (mods) *mods = 0;
    return kCGErrorFailure;
}
static i32 g_hk_mode;
CGError CGSSetGlobalHotKeyOperatingMode(u32 cid, i32 mode) { (void)cid; g_hk_mode = mode; return kCGErrorSuccess; }
CGError CGSGetGlobalHotKeyOperatingMode(u32 cid, i32 *mode) { (void)cid; if (mode) *mode = g_hk_mode; return kCGErrorSuccess; }
static id g_conn_props;
CGError CGSSetConnectionProperty(u32 cid, u32 target, id key, id value) {
    (void)cid; (void)target;
    if (!key) return kCGErrorIllegalArgument;
    if (!g_conn_props) g_conn_props = objc_retain(MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary"));
    if (value) MSG(void, g_conn_props, "setObject:forKey:", value, key);
    else MSG(void, g_conn_props, "removeObjectForKey:", key);
    return kCGErrorSuccess;
}
CGError CGSCopyConnectionProperty(u32 cid, u32 target, id key, id *value) {
    (void)cid; (void)target;
    if (!value) return kCGErrorIllegalArgument;
    id v = g_conn_props && key ? MSG(id, g_conn_props, "objectForKey:", key) : 0;
    *value = v ? objc_retain(v) : 0;
    return kCGErrorSuccess;
}

/* ---- IOKit de tela */
typedef i32 kern_return_t;
typedef u32 io_object_t;
#define kIOReturnSuccess      0
#define kIOReturnBadArgument  ((kern_return_t)0xe00002c2)
#define kIOReturnNoDevice     ((kern_return_t)0xe00002c0)
#define kIOReturnUnsupported  ((kern_return_t)0xe00002c7)
#define kIOReturnNotFound     ((kern_return_t)0xe00002f0)
/* framebuffer da tela principal: um nome fixo diferente de MACH_PORT_NULL, para os apps que testam o retorno */
#define MOS_FB_SERVICE 0x4D4F5331u
u32 CGDisplayIOServicePort(u32 d) { return d == MAIN_DISPLAY ? MOS_FB_SERVICE : 0; }
id CoreDisplay_DisplayCreateInfoDictionary(unsigned cg_id);
id CFDictionaryCreateMutable(void *a, long cap, const void *kcb, const void *vcb);
extern const char kCFTypeDictionaryKeyCallBacks[], kCFTypeDictionaryValueCallBacks[];
id IODisplayCreateInfoDictionary(io_object_t fb, u32 options) {
    (void)options;
    id d = fb == MOS_FB_SERVICE || fb == 0 ? CoreDisplay_DisplayCreateInfoDictionary(MAIN_DISPLAY) : 0;
    if (d) return d;
    /* sem EDID (QEMU, GPU sem driver): o mínimo que os apps leem, com o nome localizado */
    d = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    id names = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary");
    MSG(void, names, "setObject:forKey:", cx_str("Tela do mOS"), cx_str("en_US"));
    MSG(void, d, "setObject:forKey:", names, cx_str("DisplayProductName"));
    MSG(void, d, "setObject:forKey:", cx_num_i(0), cx_str("DisplayVendorID"));
    MSG(void, d, "setObject:forKey:", cx_num_i(0), cx_str("DisplayProductID"));
    MSG(void, d, "setObject:forKey:", cx_num_i(0), cx_str("DisplaySerialNumber"));
    return d;
}
kern_return_t IODisplayGetFloatParameter(io_object_t s, u32 opts, id key, float *value) {
    (void)opts;
    if (!value || !key) return kIOReturnBadArgument;
    if (s != MOS_FB_SERVICE) return kIOReturnUnsupported;
    if (!strcmp(cx_cstr(key), "brightness")) { *value = 1.0f; return kIOReturnSuccess; }
    return kIOReturnUnsupported;
}
kern_return_t IODisplaySetFloatParameter(io_object_t s, u32 opts, id key, float value) { (void)s; (void)opts; (void)key; (void)value; return kIOReturnUnsupported; }
kern_return_t IODisplayGetIntegerRangeParameter(io_object_t s, u32 opts, id key, i32 *v, i32 *min, i32 *max) {
    (void)s; (void)opts; (void)key; (void)v; (void)min; (void)max;
    return kIOReturnUnsupported;
}
kern_return_t IODisplaySetIntegerParameter(io_object_t s, u32 opts, id key, i32 v) { (void)s; (void)opts; (void)key; (void)v; return kIOReturnUnsupported; }

/* ---- IOKit: registro vazio (mach.c) */
kern_return_t IORegistryEntryGetChildIterator(io_object_t e, const char *plane, io_object_t *it) { (void)e; (void)plane; if (it) *it = 0; return kIOReturnSuccess; }
kern_return_t IORegistryEntryGetParentIterator(io_object_t e, const char *plane, io_object_t *it) { (void)e; (void)plane; if (it) *it = 0; return kIOReturnSuccess; }
kern_return_t IORegistryEntryGetChildEntry(io_object_t e, const char *plane, io_object_t *child) { (void)e; (void)plane; if (child) *child = 0; return kIOReturnNoDevice; }
kern_return_t IORegistryEntryGetPath(io_object_t e, const char *plane, char *path) {
    if (!path) return kIOReturnBadArgument;
    if (e != MOS_FB_SERVICE) { path[0] = 0; return kIOReturnBadArgument; }
    const char *p = plane && plane[0] ? plane : "IOService";
    size_t n = strlen(p);
    if (n > 400) n = 400;
    memcpy(path, p, n);
    memcpy(path + n, ":/mOSFramebuffer", 17);
    return kIOReturnSuccess;
}
kern_return_t IORegistryEntryInPlane(io_object_t e, const char *plane) { (void)plane; return e == MOS_FB_SERVICE; }
u32 IOIteratorIsValid(io_object_t it) { (void)it; return 1; }
void IOIteratorReset(io_object_t it) { (void)it; }
u32 IOObjectIsEqualTo(io_object_t a, io_object_t b) { return a == b; }
kern_return_t IOObjectGetClass(io_object_t o, char *name) {
    if (!name) return kIOReturnBadArgument;
    const char *c = o == MOS_FB_SERVICE ? "IOFramebuffer" : o ? "IOService" : "";
    strncpy(name, c, 128);
    return o ? kIOReturnSuccess : kIOReturnBadArgument;
}
id IOObjectCopyClass(io_object_t o) { return o ? objc_retain(o == MOS_FB_SERVICE ? cx_str("IOFramebuffer") : (id)cg_ioservice) : 0; }
id IOObjectCopySuperclassForClass(id cls) { (void)cls; return 0; }
u32 IOObjectGetRetainCount(io_object_t o) { return o ? 1 : 0; }
u32 IOObjectGetKernelRetainCount(io_object_t o) { return o ? 1 : 0; }
u32 IOObjectGetUserRetainCount(io_object_t o) { return o ? 1 : 0; }
u32 IONotificationPortGetMachPort(void *p) { (void)p; return 0; }
void IODispatchCalloutFromMessage(void *unused, void *msg, void *ref) { (void)unused; (void)msg; (void)ref; }
kern_return_t IOKitWaitQuiet(u32 main, void *timeout) { (void)main; (void)timeout; return kIOReturnSuccess; }
kern_return_t IOServiceAuthorize(io_object_t s, u32 opts) { (void)s; (void)opts; return kIOReturnSuccess; }
kern_return_t IOServiceWaitQuiet(io_object_t s, void *timeout) { (void)s; (void)timeout; return kIOReturnSuccess; }
kern_return_t IOServiceGetBusyState(io_object_t s, u32 *busy) { (void)s; if (busy) *busy = 0; return kIOReturnSuccess; }
id IOServiceNameMatching(const char *name) {
    id d = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    if (d && name) MSG(void, d, "setObject:forKey:", cx_str(name), cx_str("IONameMatch"));
    return d;
}
kern_return_t IODestroyPlugInInterface(void **iface) { (void)iface; return kIOReturnSuccess; }
kern_return_t IOConnectCallScalarMethod(u32 conn, u32 sel, const u64 *in, u32 nin, u64 *out, u32 *nout) {
    (void)conn; (void)sel; (void)in; (void)nin; (void)out;
    if (nout) *nout = 0;
    return kIOReturnUnsupported;
}
kern_return_t IOConnectCallAsyncScalarMethod(u32 conn, u32 sel, u32 wake, u64 *ref, u32 nref, const u64 *in, u32 nin, u64 *out, u32 *nout) {
    (void)conn; (void)sel; (void)wake; (void)ref; (void)nref; (void)in; (void)nin; (void)out;
    if (nout) *nout = 0;
    return kIOReturnUnsupported;
}
kern_return_t IOConnectCallAsyncStructMethod(u32 conn, u32 sel, u32 wake, u64 *ref, u32 nref, const void *in, size_t nin, void *out, size_t *nout) {
    (void)conn; (void)sel; (void)wake; (void)ref; (void)nref; (void)in; (void)nin; (void)out;
    if (nout) *nout = 0;
    return kIOReturnUnsupported;
}
kern_return_t IOConnectSetNotificationPort(u32 conn, u32 type, u32 port, uintptr_t ref) { (void)conn; (void)type; (void)port; (void)ref; return kIOReturnUnsupported; }
kern_return_t IOConnectMapMemory(u32 conn, u32 type, u32 task, u64 *addr, u64 *size, u32 opts) {
    (void)conn; (void)type; (void)task; (void)opts;
    if (addr) *addr = 0;
    if (size) *size = 0;
    return kIOReturnUnsupported;
}
kern_return_t IOConnectMapMemory64(u32 conn, u32 type, u32 task, u64 *addr, u64 *size, u32 opts) { return IOConnectMapMemory(conn, type, task, addr, size, opts); }
kern_return_t IOConnectUnmapMemory(u32 conn, u32 type, u32 task, u64 addr) { (void)conn; (void)type; (void)task; (void)addr; return kIOReturnSuccess; }
kern_return_t IOConnectUnmapMemory64(u32 conn, u32 type, u32 task, u64 addr) { (void)conn; (void)type; (void)task; (void)addr; return kIOReturnSuccess; }
kern_return_t IOConnectAddRef(u32 conn) { (void)conn; return kIOReturnSuccess; }
kern_return_t IOConnectRelease(u32 conn) { (void)conn; return kIOReturnSuccess; }

/* ---- IOPowerSources: o mOS não lê bateria (sem ACPI _BST no userland); ligado na tomada */
id IOPSGetProvidingPowerSourceType(id snapshot) { (void)snapshot; return (id)cg_ac_power; }
double IOPSGetTimeRemainingEstimate(void) { return -2.0; }     /* kIOPSTimeRemainingUnlimited */
id IOPSCopyExternalPowerAdapterDetails(void) { return 0; }
i32 IOPSGetPercentRemaining(i32 *pct, u8 *charging, u8 *full) {
    if (pct) *pct = 100;
    if (charging) *charging = 0;
    if (full) *full = 1;
    return kIOReturnSuccess;
}
i32 IOPSGetBatteryWarningLevel(void) { return 1; }     /* kIOPSLowBatteryWarningNone */
u32 IOPSNotificationCreateRunLoopSourceWithContext(void) { return 0; }
u32 IOPSCreateLimitedPowerNotification(void *cb, void *ctx) { (void)cb; (void)ctx; return 0; }

/* ---- IOHID: sem dispositivos HID para o app (o teclado e o mouse chegam como eventos do window server) */
typedef struct { cx_class_t *isa; } hidobj_t;
u64 IOHIDElementGetTypeID(void) { return 0x2C0; }
u64 IOHIDDeviceGetTypeID(void) { return 0x2C1; }
u64 IOHIDValueGetTypeID(void) { return 0x2C2; }
u32 IOHIDElementGetCookie(void *e) { (void)e; return 0; }
id IOHIDElementGetChildren(void *e) {
    (void)e;
    static id empty;
    if (!empty) empty = objc_retain(MSG0(id, (id)cx_class_named("NSArray"), "array"));
    return empty;
}
id IOHIDElementGetName(void *e) { (void)e; return 0; }
u32 IOHIDElementGetCollectionType(void *e) { (void)e; return 0; }
u32 IOHIDElementIsArray(void *e) { (void)e; return 0; }
u32 IOHIDElementIsRelative(void *e) { (void)e; return 0; }
u32 IOHIDElementIsVirtual(void *e) { (void)e; return 0; }
u32 IOHIDElementHasNullState(void *e) { (void)e; return 0; }
long IOHIDElementGetPhysicalMin(void *e) { (void)e; return 0; }
long IOHIDElementGetPhysicalMax(void *e) { (void)e; return 0; }
u32 IOHIDElementGetUnit(void *e) { (void)e; return 0; }
u32 IOHIDElementGetUnitExponent(void *e) { (void)e; return 0; }
u32 IOHIDElementGetReportCount(void *e) { (void)e; return 0; }
id IOHIDElementGetProperty(void *e, id key) { (void)e; (void)key; return 0; }
u8 IOHIDElementSetProperty(void *e, id key, id v) { (void)e; (void)key; (void)v; return 0; }
kern_return_t IOHIDDeviceGetValue(void *dev, void *elem, void **value) {
    (void)dev; (void)elem;
    if (value) *value = 0;
    return kIOReturnNoDevice;
}
kern_return_t IOHIDDeviceGetValueWithCallback(void *dev, void *elem, void **value, double t, void *cb, void *ctx) {
    (void)t; (void)cb; (void)ctx;
    return IOHIDDeviceGetValue(dev, elem, value);
}
kern_return_t IOHIDDeviceSetValue(void *dev, void *elem, void *value) { (void)dev; (void)elem; (void)value; return kIOReturnNoDevice; }
void IOHIDDeviceRegisterInputValueCallback(void *dev, void *cb, void *ctx) { (void)dev; (void)cb; (void)ctx; }
void IOHIDDeviceSetInputValueMatching(void *dev, id matching) { (void)dev; (void)matching; }
void IOHIDDeviceSetInputValueMatchingMultiple(void *dev, id multiple) { (void)dev; (void)multiple; }
u8 IOHIDDeviceConformsTo(void *dev, u32 page, u32 usage) { (void)dev; (void)page; (void)usage; return 0; }
u8 IOHIDDeviceSetProperty(void *dev, id key, id v) { (void)dev; (void)key; (void)v; return 0; }
void IOHIDDeviceSetDispatchQueue(void *dev, void *q) { (void)dev; (void)q; }
void IOHIDDeviceActivate(void *dev) { (void)dev; }
void IOHIDDeviceCancel(void *dev) { (void)dev; }
void IOHIDDeviceSetCancelHandler(void *dev, void *h) { (void)dev; (void)h; }
u64 IOHIDValueGetTimeStamp(void *v) { (void)v; return 0; }
double IOHIDValueGetScaledValue(void *v, u32 type) { (void)v; (void)type; return 0; }
void *IOHIDValueCreateWithIntegerValue(void *a, void *elem, u64 ts, long v) { (void)a; (void)elem; (void)ts; (void)v; return 0; }
void IOHIDManagerSetInputValueMatching(void *m, id matching) { (void)m; (void)matching; }
void IOHIDManagerSetInputValueMatchingMultiple(void *m, id multiple) { (void)m; (void)multiple; }
void IOHIDManagerSetDispatchQueue(void *m, void *q) { (void)m; (void)q; }
void IOHIDManagerActivate(void *m) { (void)m; }
void IOHIDManagerCancel(void *m) { (void)m; }
void IOHIDManagerSetCancelHandler(void *m, void *h) { (void)m; (void)h; }
id IOHIDManagerGetProperty(void *m, id key) { (void)m; (void)key; return 0; }
u8 IOHIDManagerSetProperty(void *m, id key, id v) { (void)m; (void)key; (void)v; return 0; }
/* Caps Lock: o estado real vem do modificador que o AppKit acompanha (NSEventModifierFlagCapsLock) */
kern_return_t IOHIDGetModifierLockState(u32 handle, i32 sel, u8 *state) {
    (void)handle;
    if (!state) return kIOReturnBadArgument;
    if (sel != 1) { *state = 0; return kIOReturnSuccess; }    /* kIOHIDCapsLockState */
    u64 f = MSG0(u64, (id)cx_class_named("NSEvent"), "modifierFlags");
    *state = (f & (1u << 16)) != 0;
    return kIOReturnSuccess;
}
kern_return_t IOHIDSetModifierLockState(u32 handle, i32 sel, u8 state) { (void)handle; (void)sel; (void)state; return kIOReturnSuccess; }
kern_return_t IOHIDGetParameter(u32 handle, id key, u64 max, void *bytes, u64 *size) {
    (void)handle; (void)key; (void)max; (void)bytes;
    if (size) *size = 0;
    return kIOReturnUnsupported;
}
kern_return_t IOHIDSetParameter(u32 handle, id key, const void *bytes, u64 size) { (void)handle; (void)key; (void)bytes; (void)size; return kIOReturnSuccess; }
kern_return_t IOHIDCopyCFTypeParameter(u32 handle, id key, id *v) { (void)handle; (void)key; if (v) *v = 0; return kIOReturnUnsupported; }
kern_return_t IOHIDSetCFTypeParameter(u32 handle, id key, id v) { (void)handle; (void)key; (void)v; return kIOReturnSuccess; }
kern_return_t IOHIDPostEvent(u32 handle, u32 type, i32 loc, const void *data, u32 ver, u32 flags, u32 opts) {
    (void)handle; (void)type; (void)loc; (void)data; (void)ver; (void)flags; (void)opts;
    return kIOReturnUnsupported;
}
void *IOHIDEventSystemClientCreate(void *allocator);
void *IOHIDEventSystemClientCreateSimpleClient(void *allocator) { return IOHIDEventSystemClientCreate(allocator); }
void *IOHIDEventSystemClientCreateWithType(void *allocator, i32 type, id attrs) { (void)type; (void)attrs; return IOHIDEventSystemClientCreate(allocator); }
id IOHIDServiceClientCopyProperty(void *service, id key);
u8 IOHIDServiceClientSetProperty(void *service, id key, id v) { (void)service; (void)key; (void)v; return 0; }
id IOHIDServiceClientGetRegistryID(void *service) {
    id v = service ? IOHIDServiceClientCopyProperty(service, cx_str("RegistryID")) : 0;
    if (v) { objc_release(v); return v; }     /* regra Get: o serviço mantém a referência (as propriedades são estáticas) */
    return service ? cx_num_i((i64)(uintptr_t)service) : 0;
}
u8 IOHIDServiceClientConformsTo(void *service, u32 page, u32 usage) {
    if (!service) return 0;
    id p = IOHIDServiceClientCopyProperty(service, cx_str("PrimaryUsagePage"));
    id u = IOHIDServiceClientCopyProperty(service, cx_str("PrimaryUsage"));
    u8 ok = p && u && MSG0(long, p, "longValue") == (long)page && MSG0(long, u, "longValue") == (long)usage;
    if (p) objc_release(p);
    if (u) objc_release(u);
    return ok;
}
void IOHIDEventSystemClientScheduleWithRunLoop(void *c, void *rl, id mode) { (void)c; (void)rl; (void)mode; }
void IOHIDEventSystemClientUnscheduleWithRunLoop(void *c, void *rl, id mode) { (void)c; (void)rl; (void)mode; }
void IOHIDEventSystemClientRegisterEventCallback(void *c, void *cb, void *target, void *ref) { (void)c; (void)cb; (void)target; (void)ref; }
void IOHIDEventSystemClientSetDispatchQueue(void *c, void *q) { (void)c; (void)q; }
void IOHIDEventSystemClientActivate(void *c) { (void)c; }
void IOHIDEventSystemClientCancel(void *c) { (void)c; }

/* ---- CGS: lista de janelas do compositor (a mesma do CGWindowListCopyWindowInfo, DEV_WINDOW_LIST) */
CFCONST(kCGWindowMemoryUsage, "kCGWindowMemoryUsage")
int getpid(void);
static int cgs_list(int only_pid, int onscreen, int menubar, u32 max, u32 *out, i32 *count) {
    gui_cgwindow_t w;
    u32 n = 0;
    int self = getpid();
    for (int i = 0; cx_cg_window_at(i, &w); i++) {
        if (only_pid && w.pid != self) continue;
        if (onscreen && !w.onscreen) continue;
        if (menubar && w.layer != 24) continue;   /* kCGMainMenuWindowLevel */
        if (out && n < max) out[n] = w.number;
        n++;
    }
    if (count) *count = (i32)(out && n > max ? max : n);
    return kCGErrorSuccess;
}
CGError CGSGetWindowCount(u32 cid, u32 target, i32 *count) { (void)cid; (void)target; return cgs_list(0, 0, 0, 0, 0, count); }
CGError CGSGetOnScreenWindowCount(u32 cid, u32 target, i32 *count) { (void)cid; (void)target; return cgs_list(0, 1, 0, 0, 0, count); }
CGError CGSGetWindowList(u32 cid, u32 target, i32 max, u32 *list, i32 *count) { (void)cid; (void)target; return cgs_list(0, 0, 0, max > 0 ? (u32)max : 0, list, count); }
CGError CGSGetOnScreenWindowList(u32 cid, u32 target, i32 max, u32 *list, i32 *count) { (void)cid; (void)target; return cgs_list(0, 1, 0, max > 0 ? (u32)max : 0, list, count); }
CGError CGSGetProcessMenuBarWindowList(u32 cid, u32 target, i32 max, u32 *list, i32 *count) { (void)cid; (void)target; return cgs_list(1, 0, 1, max > 0 ? (u32)max : 0, list, count); }
/* captura de janelas pelo CGS: a mesma do CGWindowListCreateImage (cgevent.c), uma imagem por janela */
void *CGWindowListCreateImage(cgrect_t r, u32 opt, u32 win, u32 imgopt);
id CGSHWCaptureWindowList(u32 cid, u32 *wids, i32 n, u32 opts) {
    (void)cid; (void)opts;
    id a = CFArrayCreate(0, 0, 0, &kCFTypeArrayCallBacks);
    cgrect_t null_r = {1.0 / 0.0, 1.0 / 0.0, 0, 0};           /* CGRectNull: o tamanho da janela */
    for (i32 i = 0; wids && i < n; i++) {
        void *img = CGWindowListCreateImage(null_r, 8, wids[i], 0);   /* kCGWindowListOptionIncludingWindow */
        if (img) { MSG(void, a, "addObject:", (id)img); objc_release((id)img); }
    }
    return a;
}
CGError CGSCopyWindowProperty(u32 cid, u32 wid, id key, id *out) {
    (void)cid;
    if (!out) return kCGErrorIllegalArgument;
    *out = 0;
    gui_cgwindow_t w;
    if (!key || strcmp(cx_cstr(key), "kCGSWindowTitle")) return kCGErrorIllegalArgument;
    for (int i = 0; cx_cg_window_at(i, &w); i++)
        if (w.number == wid) { *out = objc_retain(cx_str(w.name)); return kCGErrorSuccess; }
    return kCGErrorIllegalArgument;
}
static u32 g_cursor_seed = 1;
u32 CGSCurrentCursorSeed(void) { return g_cursor_seed; }
/* OpenGL: a máscara 1 é a tela principal */
u32 CGOpenGLDisplayMaskToDisplayID(u32 mask) { return mask & 1 ? MAIN_DISPLAY : 0; }
/* contexto de desenho direto na tela (CGDisplayCapture): o mOS não dá acesso ao framebuffer; NULL é "indisponível" */
void *CGDisplayGetDrawingContext(u32 d) { (void)d; return 0; }
double CGEventSourceGetPixelsPerLine(void *src) { (void)src; return 10.0; }
void CGEventSourceSetPixelsPerLine(void *src, double v) { (void)src; (void)v; }
void CGEventPost(u32 tap, void *e);
void CGEventTapPostEvent(void *proxy, void *e) { (void)proxy; CGEventPost(0, e); }   /* reinjeta pela fila do HID */
cgpt_t CGEventGetLocation(void *e);
/* y para cima, a partir da borda de baixo da tela principal */
cgpt_t CGEventGetUnflippedLocation(void *e) {
    cgpt_t p = CGEventGetLocation(e);
    p.y = (double)CGDisplayPixelsHigh(MAIN_DISPLAY) - p.y;
    return p;
}
void *CGEventCreateMouseEvent(void *src, u32 type, cgpt_t loc, u32 button);
void CFRelease(const void *o);
/* CGPostMouseEvent (10.0): posição, mover o cursor?, quantos botões, estado do botão 1 */
CGError CGPostMouseEvent(cgpt_t p, u32 move, u32 count, u32 down, ...) {
    (void)count;
    static u32 was_down;
    u32 type = down && !was_down ? 1 : !down && was_down ? 2 : down ? 6 : 5;   /* LeftMouseDown/Up/Dragged/Moved */
    was_down = down;
    void *e = CGEventCreateMouseEvent(0, type, p, 0);
    if (!e) return kCGErrorFailure;
    CGEventPost(0, e);
    CFRelease(e);
    if (move) CGWarpMouseCursorPosition(p);
    return kCGErrorSuccess;
}

/* ---- IOKit: o que sobrou do corpus */
kern_return_t IORegistryEntrySetCFProperty(io_object_t e, id key, id v) { (void)e; (void)key; (void)v; return kIOReturnUnsupported; }
kern_return_t IORegistryEntrySetCFProperties(io_object_t e, id props) { (void)e; (void)props; return kIOReturnUnsupported; }
u64 IOHIDEventGetSenderID(void *ev) { (void)ev; return 0; }
kern_return_t IOPMSleepSystem(u32 port) { (void)port; return kIOReturnUnsupported; }
u32 IOPMFindPowerManagement(u32 main) { (void)main; return 0; }
kern_return_t IOPMCopyAssertionsStatus(id *out) {
    if (!out) return kIOReturnBadArgument;
    *out = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    return kIOReturnSuccess;
}
void *IOHIDValueCreateWithBytes(void *alloc, void *elem, u64 ts, const u8 *bytes, long len) { (void)alloc; (void)elem; (void)ts; (void)bytes; (void)len; return 0; }
kern_return_t IOHIDSetAccelerationWithKey(u32 h, id key, double v) { (void)h; (void)key; (void)v; return kIOReturnSuccess; }
kern_return_t IOHIDGetAccelerationWithKey(u32 h, id key, double *v) { (void)h; (void)key; if (v) *v = 0.6875; return kIOReturnSuccess; }   /* padrão do macOS */
/* acesso a monitorar a entrada (TCC kTCCServiceListenEvent) */
u8 IOHIDRequestAccess(u32 type) { (void)type; return (u8)cx_tcc_request("kTCCServiceListenEvent"); }
void *IOHIDEventSystemClientCopyServiceForRegistryID(void *client, u64 id_) { (void)client; (void)id_; return 0; }
