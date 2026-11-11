/*
 * mOS libSystem — funções de frameworks do sistema que o Electron/VS Code importa e o mOS ainda não tinha
 * (inventário do tools/machocheck.c): acessibilidade (AX*), impressão (PM*), HID/IOKit, energia (IOPS*),
 * CoreGraphics (janelas, eventos, captura de tela, PDF), Carbon/HIToolbox (atalhos globais, fontes de
 * entrada), LaunchServices (itens de login), Apple Events, SystemConfiguration e o Keychain antigo.
 *
 * Sem estas, o modo experimental do carregador encerrava o processo na primeira chamada. Cada função
 * responde como um Mac responderia na situação que o mOS de fato tem: sem permissão de acessibilidade
 * nem de gravação de tela, sem impressora, sem bateria (na tomada), sem dispositivos HID extras, rede
 * alcançável, teclado ABNT2. O Chromium tem caminho para todas essas respostas.
 */
#include "foundation.h"
#include "computeruse_win.h"

void *malloc(size_t n);
void *memset(void *d, int c, size_t n);
int strcmp(const char *a, const char *b);
double sin(double), cos(double);
id objc_retain(id o);
void objc_release(id o);
id CFArrayCreate(void *alloc, const id *vals, long n, const void *cb);
id CFRetain(id o);
id CFDictionaryCreateMutable(void *alloc, long cap, const void *kcb, const void *vcb);
void CFDictionarySetValue(id d, id key, id val);
id CFDictionaryGetValue(id d, id key);
u8 CFNumberGetValue(id n, long type, void *out);
int strncmp(const char *a, const char *b, size_t n);
/* Frente Sistemas: só estas quatro LSSharedFileList* chamam o armazenamento
 * de Instalação (loginitems.c). O restante deste arquivo não muda. */
int cx_login_add(const char *path);
int cx_login_remove(const char *path);
id cx_login_snapshot(unsigned *seed);
id CFNumberCreate(void *alloc, long type, const void *p);
size_t CGDisplayPixelsWide(u32 d);
size_t CGDisplayPixelsHigh(u32 d);
u32 CGMainDisplayID(void);
void *CGColorSpaceCreateDeviceRGB(void);
void *CGDataProviderCreateWithData(void *info, const void *data, size_t size, void (*release)(void *, const void *, size_t));
id CGColorCreateGenericRGB(double r, double g, double b, double a);

typedef int OSStatus;
typedef int kern_return_t;
typedef struct { double x, y; } cg_pt_t;
typedef struct { double x, y, w, h; } cg_rect_t;
typedef struct { double a, b, c, d, tx, ty; } cg_affine_t;
static id cls(const char *n) { return (id)cx_class_named(n); }
static id empty_array(void) { return CFArrayCreate(0, 0, 0, 0); }
/* objeto opaco "vivo" (CFRelease funciona nele) para APIs que devolvem uma referência */
static id token(void) { return objc_retain(MSG0(id, cls("NSObject"), "new")); }

/* ================================================================ AppKit */
void NSPerformService(id item, id pboard) { (void)item; (void)pboard; }
id NSAccessibilityRoleDescription(id role, id subrole) { (void)subrole; return role; }
id NSAccessibilityActionDescription(id action) { return action; }

/* ================================================================ CoreText: framesetter */
/* o framesetter só é usado para medir texto de diálogos: devolve o atributo e mede pela fonte do sistema */
id CTFramesetterCreateWithAttributedString(id astr) { return astr ? objc_retain(astr) : 0; }
typedef struct { double w, h; } cg_size_t;
cg_size_t CTFramesetterSuggestFrameSizeWithConstraints(id fs, struct { long loc, len; } range, id attrs,
                                                       cg_size_t constraints, struct { long loc, len; } *fit) {
    (void)range; (void)attrs;
    id s = fs && cx_responds(object_getClass(fs), sel_registerName("string")) ? MSG0(id, fs, "string") : fs;
    long n = s && cx_is_str(s) ? (long)MSG0(u64, s, "length") : 0;
    double cw = 7.0, lh = 16.0;      /* 13 pt da fonte do sistema */
    double maxw = constraints.w > 0 && constraints.w < 1e8 ? constraints.w : cw * (double)n;
    long per = (long)(maxw / cw);
    if (per < 1) per = 1;
    long lines = (n + per - 1) / per;
    if (lines < 1) lines = 1;
    cg_size_t r = {n < per ? cw * (double)n : maxw, lh * (double)lines};
    if (constraints.h > 0 && r.h > constraints.h) r.h = constraints.h;
    if (fit) { fit->loc = 0; fit->len = n; }
    return r;
}

/* ================================================================ ApplicationServices: acessibilidade */
int cx_tcc_preflight(const char *service);
int cx_tcc_request(const char *service);
id cx_ax_application(int pid);
id cx_ax_system(void);
int cx_ax_copy(id e, id attr, id *v);
int cx_ax_copy_values(id e, id attr, long index, long maxValues, id *values);
int cx_ax_names(id e, id *names);
int cx_ax_set(id e, id attr, id value);
int cx_ax_actions(id e, id *names);
int cx_ax_perform(id e, id action);
int cx_ax_param(id e, id attr, id param, id *v);
int cx_ax_param_names(id e, id *names);
u8 AXIsProcessTrustedWithOptions(id opts) {
    int prompt = 0;
    if (opts) {
        id key = cx_str("AXTrustedCheckOptionPrompt");
        id v = key ? MSG(id, opts, "objectForKey:", key) : 0;
        if (v) prompt = MSG0(signed char, v, "boolValue") ? 1 : 0;
        if (key) objc_release(key);
    }
    return prompt ? (u8)cx_tcc_request("kTCCServiceAccessibility") : (u8)cx_tcc_preflight("kTCCServiceAccessibility");
}
u64 AXUIElementGetTypeID(void) { return 0x4158; }
u64 AXValueGetTypeID(void) { return 0x4156; }
u64 AXTextMarkerGetTypeID(void) { return 0x4154; }
u64 AXTextMarkerRangeGetTypeID(void) { return 0x4152; }
id AXUIElementCreateApplication(int pid) { return cx_ax_application(pid); }
id AXUIElementCreateSystemWide(void) { return cx_ax_system(); }   /* claude-native-binding.node do Claude desktop */
OSStatus AXUIElementCopyAttributeValue(id e, id attr, id *v) { return cx_ax_copy(e, attr, v); }
OSStatus AXUIElementCopyAttributeValues(id e, id attr, long index, long maxValues, id *values) {
    return cx_ax_copy_values(e, attr, index, maxValues, values);
}
/* Vários atributos de uma vez. Atributo sem valor vira AXValue do tipo AXError, como no macOS; com a opção
 * kAXCopyMultipleAttributeOptionStopOnError (1) a primeira falha devolve o erro e nenhum array. */
id AXValueCreate(u32 type, const void *p);
long CFArrayGetCount(id a);
id CFArrayGetValueAtIndex(id a, long i);
id CFArrayCreateMutable(void *alloc, long cap, const void *cb);
void CFArrayAppendValue(id a, id v);
OSStatus AXUIElementCopyMultipleAttributeValues(id e, id attrs, u32 options, id *values) {
    if (!values) return -25201;                  /* kAXErrorIllegalArgument */
    *values = 0;
    if (!e || !attrs) return -25201;
    long n = CFArrayGetCount(attrs);
    id out = CFArrayCreateMutable(0, n, 0);
    for (long i = 0; i < n; i++) {
        id v = 0;
        OSStatus err = cx_ax_copy(e, CFArrayGetValueAtIndex(attrs, i), &v);
        if (err || !v) {
            if (options & 1) { objc_release(out); return err ? err : -25212; }
            int32_t code = err ? err : -25212;   /* kAXErrorNoValue */
            v = AXValueCreate(5, &code);
        }
        CFArrayAppendValue(out, v);
        if (v) objc_release(v);
    }
    *values = out;
    return 0;
}
OSStatus AXUIElementCopyAttributeNames(id e, id *names) { return cx_ax_names(e, names); }
OSStatus AXUIElementCopyParameterizedAttributeNames(id e, id *names) { return cx_ax_param_names(e, names); }
OSStatus AXUIElementCopyParameterizedAttributeValue(id e, id attr, id p, id *v) { return cx_ax_param(e, attr, p, v); }
OSStatus AXUIElementSetAttributeValue(id e, id attr, id v) { return cx_ax_set(e, attr, v); }
OSStatus AXUIElementCopyActionNames(id e, id *names) { return cx_ax_actions(e, names); }
OSStatus AXUIElementPerformAction(id e, id action) { return cx_ax_perform(e, action); }
/* AXValue: caixa de CGPoint/CGSize/CGRect/CFRange (tipos 1..4) */
typedef struct { void *isa; u32 type; u8 data[32]; } axval_t;
static cx_class_t axv_c, axv_m;
/* 5 = kAXValueTypeAXError: um AXError (int32) */
static int axv_size(u32 t) { return t == 3 ? 32 : t == 1 || t == 2 || t == 4 ? 16 : t == 5 ? 4 : 0; }
id AXValueCreate(u32 type, const void *p) {
    if (!axv_c.vtable) cx_define_class(&axv_c, &axv_m, "__mOSAXValue", cx_class_named("NSObject"), 40);
    int n = axv_size(type);
    if (!n || !p) return 0;
    axval_t *v = (axval_t *)cx_alloc_instance(&axv_c);
    v->type = type;
    for (int i = 0; i < n; i++) v->data[i] = ((const u8 *)p)[i];
    return (id)v;
}
u32 AXValueGetType(id v) { return v && object_getClass(v) == &axv_c ? ((axval_t *)v)->type : 0; }
u8 AXValueGetValue(id v, u32 type, void *out) {
    if (!v || object_getClass(v) != &axv_c || ((axval_t *)v)->type != type || !out) return 0;
    for (int i = 0; i < axv_size(type); i++) ((u8 *)out)[i] = ((axval_t *)v)->data[i];
    return 1;
}
/* marcadores de texto: bytes opacos */
id AXTextMarkerCreate(void *alloc, const u8 *bytes, long len) { (void)alloc; return MSG(id, cls("NSData"), "dataWithBytes:length:", bytes, (u64)len); }
const u8 *AXTextMarkerGetBytePtr(id m) { return m ? MSG0(const u8 *, m, "bytes") : 0; }
long AXTextMarkerGetLength(id m) { return m ? (long)MSG0(u64, m, "length") : 0; }
id AXTextMarkerRangeCreate(void *alloc, id a, id b) { (void)alloc; id v[2] = {a, b}; return CFArrayCreate(0, v, 2, 0); }
id AXTextMarkerRangeCopyStartMarker(id r) { return r ? CFRetain(MSG(id, r, "objectAtIndex:", (u64)0)) : 0; }
id AXTextMarkerRangeCopyEndMarker(id r) { return r ? CFRetain(MSG(id, r, "objectAtIndex:", (u64)1)) : 0; }
u8 UAZoomEnabled(void) { return 0; }
OSStatus UAZoomChangeFocus(const void *a, const void *b, int type) { (void)a; (void)b; (void)type; return 0; }

/* ---- processos (Process Manager) */
OSStatus GetProcessInformation(const void *psn, void *info) { (void)psn; (void)info; return -600; /* procNotFound */ }
OSStatus TransformProcessType(const void *psn, u32 type) { (void)psn; (void)type; return 0; }

/* ---- impressão (PM*): sem impressora; o diálogo do Chromium mostra "nenhuma impressora" */
#define kPMNoDefaultPrinter (-30872)
#define kPMGeneralError (-30870)
OSStatus PMRelease(void *o) { if (o) objc_release((id)o); return 0; }
OSStatus PMPrinterCreateFromPrinterID(id pid, void **out) { (void)pid; if (out) *out = 0; return kPMNoDefaultPrinter; }
#define PM_FAIL1(name, T1) OSStatus name(T1 a) { (void)a; return kPMGeneralError; }
#define PM_FAIL2(name, T1, T2) OSStatus name(T1 a, T2 b) { (void)a; (void)b; return kPMGeneralError; }
#define PM_FAIL3(name, T1, T2, T3) OSStatus name(T1 a, T2 b, T3 c) { (void)a; (void)b; (void)c; return kPMGeneralError; }
#define PM_OUT0(name) OSStatus name(void *a, void **out) { (void)a; if (out) *out = 0; return kPMGeneralError; }
PM_FAIL2(PMGetAdjustedPaperRect, void *, void *)
PM_FAIL2(PMGetAdjustedPageRect, void *, void *)
PM_FAIL3(PMPrinterGetIndexedPrinterResolution, void *, u32, void *)
PM_FAIL2(PMPrinterGetPrinterResolutionCount, void *, u32 *)
PM_FAIL2(PMGetOrientation, void *, u16 *)
id PMPrinterGetID(void *p) { (void)p; return 0; }
PM_FAIL2(PMPrintSettingsSetJobName, void *, id)
PM_OUT0(PMSessionCopyDestinationLocation)
PM_OUT0(PMSessionCopyDestinationFormat)
PM_FAIL2(PMSessionGetDestinationType, void *, u16 *)
PM_FAIL3(PMPageFormatCreateDataRepresentation, void *, void **, u32)
PM_FAIL3(PMPrintSettingsCreateDataRepresentation, void *, void **, u32)
PM_OUT0(PMSessionGetCurrentPrinter)
PM_OUT0(PMPaperGetPPDPaperName)
PM_FAIL2(PMPaperGetHeight, void *, double *)
PM_FAIL2(PMPaperGetWidth, void *, double *)
OSStatus PMPaperCreateCustom(void *p, id id_, id name, double w, double h, const void *margins, void **out) {
    (void)p; (void)id_; (void)name; (void)w; (void)h; (void)margins; if (out) *out = 0; return kPMGeneralError;
}
PM_FAIL2(PMPaperGetMargins, void *, void *)
PM_OUT0(PMGetPageFormatPaper)
PM_OUT0(PMPrinterGetPaperList)
PM_FAIL2(PMSetDuplex, void *, u32)
PM_FAIL2(PMSetCollate, void *, u8)
PM_FAIL3(PMSetCopies, void *, u32, u8)
OSStatus PMSessionSetDestination(void *s, void *ps, u16 type, id fmt, id loc) { (void)s; (void)ps; (void)type; (void)fmt; (void)loc; return kPMGeneralError; }
PM_FAIL2(PMSessionSetCurrentPMPrinter, void *, void *)
OSStatus PMPrinterSetOutputResolution(void *p, void *ps, const void *res) { (void)p; (void)ps; (void)res; return kPMGeneralError; }
PM_FAIL3(PMSetLastPage, void *, u32, u8)
PM_FAIL3(PMSetFirstPage, void *, u32, u8)
PM_FAIL3(PMSessionValidatePageFormat, void *, void *, u8 *)
PM_FAIL3(PMSetOrientation, void *, u16, u8)
PM_FAIL2(PMCopyPageFormat, void *, void *)
PM_FAIL2(PMCreatePageFormatWithPMPaper, void **, void *)
OSStatus PMPrintSettingsSetValue(void *ps, id key, id v, u8 locked) { (void)ps; (void)key; (void)v; (void)locked; return kPMGeneralError; }
PM_FAIL2(PMCopyPrintSettings, void *, void *)
PM_FAIL3(PMSessionValidatePrintSettings, void *, void *, u8 *)
PM_FAIL2(PMPageFormatCreateWithDataRepresentation, id, void **)
PM_FAIL2(PMPrintSettingsCreateWithDataRepresentation, id, void **)
PM_FAIL3(PMSessionBeginCGDocumentNoDialog, void *, void *, void *)
PM_FAIL2(PMSessionGetCGGraphicsContext, void *, void **)
PM_FAIL3(PMSessionBeginPageNoDialog, void *, void *, const void *)
PM_FAIL1(PMSessionEndPageNoDialog, void *)
PM_FAIL1(PMSessionEndDocumentNoDialog, void *)

/* ================================================================ CoreGraphics */
/* níveis de janela do macOS (CGWindowLevelKey -> nível) */
int CGWindowLevelForKey(int key) {
    switch (key) {
    case 0: return (int)0x80000000;   /* base */
    case 1: return -2147483623;       /* minimum */
    case 2: return -2147483603;       /* desktop */
    case 3: return 0;                 /* backstop */
    case 4: return 0;                 /* normal */
    case 5: return 3;                 /* floating */
    case 6: return 3;                 /* torn-off menu */
    case 7: return 8;                 /* dock */
    case 8: return 24;                /* main menu */
    case 9: return 25;                /* status */
    case 10: return 8;                /* modal panel */
    case 11: return 101;              /* pop-up menu */
    case 12: return 102;              /* dragging */
    case 13: return 1000;             /* screensaver */
    case 14: return 2147483630;       /* maximum */
    case 15: return 200;              /* overlay */
    case 16: return 500;              /* help */
    case 17: return 20;               /* utility */
    case 18: return -2147483602;      /* desktop icon */
    case 19: return 1001;             /* cursor */
    case 20: return 1500;             /* assistive tech high */
    }
    return 0;
}
void *CGEventCreateFromData(void *alloc, id data) { (void)alloc; (void)data; return 0; }
int CGAssociateMouseAndMouseCursorPosition(u8 connected) { (void)connected; return 0; }
double CGEventSourceSecondsSinceLastEventType(int state, u32 type) { (void)state; (void)type; return 0.0; }
/* CGEventTapCreate / CGEventTapEnable: cgevent.c. Sem kTCCServiceAccessibility o tap é NULL
 * e CGEventPost não entra na fila. */
/* captura de tela: sem permissão (o macOS devolve NULL nesse caso) */
void *CGWindowListCreateImageFromArray(cg_rect_t r, id arr, u32 img) { (void)r; (void)arr; (void)img; return 0; }
/* CGDisplayStream: cgdisplaystream.c, quadros reais via DEV_SCREEN_CAPTURE. */
/* kCGWindowListOptionOnScreenOnly=1, AboveWindow=2, BelowWindow=4, IncludingWindow=8 */
extern const void *kCGWindowNumber, *kCGWindowOwnerPID, *kCGWindowOwnerName, *kCGWindowName;
extern const void *kCGWindowBounds, *kCGWindowLayer, *kCGWindowIsOnscreen, *kCGWindowSharingState;
extern void *kCFBooleanTrue, *kCFBooleanFalse;
id CGRectCreateDictionaryRepresentation(cg_rect_t r);
static id cg_num(long long v) { return CFNumberCreate(0, 4 /* kCFNumberSInt64Type */, &v); }
static void cg_put_num(id d, const void *key, long long v) {
    id n = cg_num(v);
    CFDictionarySetValue(d, (id)key, n);
    if (n) objc_release(n);
}
id CGWindowListCopyWindowInfo(u32 opt, u32 rel) {
    /* No macOS recente a lista das janelas dos outros apps exige gravação de tela. Sem a concessão do TCC, vazia. */
    if (!cx_tcc_preflight("kTCCServiceScreenCapture")) return empty_array();
    gui_cgwindow_t all[48];
    int n = 0, rel_i = -1;
    for (int i = 0; i < 48; i++) {
        if (!cx_cg_window_at(i, &all[n])) break;
        if (rel && all[n].number == rel) rel_i = n;
        n++;
    }
    id tmp[48];
    int got = 0;
    for (int i = 0; i < n && got < 48; i++) {
        gui_cgwindow_t *w = &all[i];
        int on = opt & 1;
        if ((opt & 6) && rel) on = 1;
        if (on && !w->onscreen) continue;
        if ((opt & 2) && rel) {
            if (rel_i < 0 || i > rel_i) continue;
            if (i == rel_i && !(opt & 8)) continue;
        }
        if ((opt & 4) && rel) {
            if (rel_i < 0 || i < rel_i) continue;
            if (i == rel_i && !(opt & 8)) continue;
        }
        id d = CFDictionaryCreateMutable(0, 8, 0, 0);
        cg_put_num(d, kCGWindowNumber, (long long)w->number);
        cg_put_num(d, kCGWindowOwnerPID, w->pid);
        cg_put_num(d, kCGWindowLayer, w->layer);
        cg_put_num(d, kCGWindowSharingState, 1);   /* kCGWindowSharingReadOnly: o compositor deixa ler os pixels */
        CFDictionarySetValue(d, (id)kCGWindowIsOnscreen, (id)(w->onscreen ? kCFBooleanTrue : kCFBooleanFalse));
        id owner = cx_str(w->owner);
        id name = cx_str(w->name);
        CFDictionarySetValue(d, (id)kCGWindowOwnerName, owner);
        CFDictionarySetValue(d, (id)kCGWindowName, name);
        objc_release(owner);
        objc_release(name);
        cg_rect_t b = {(double)w->x, (double)w->y, (double)w->w, (double)w->h};
        id bounds = CGRectCreateDictionaryRepresentation(b);
        CFDictionarySetValue(d, (id)kCGWindowBounds, bounds);
        objc_release(bounds);
        tmp[got++] = d;
    }
    id arr = CFArrayCreate(0, tmp, got, 0);
    for (int i = 0; i < got; i++) objc_release(tmp[i]);
    return arr;
}
id CGWindowListCreateDescriptionFromArray(id arr) { (void)arr; return empty_array(); }
size_t CGDisplayModeGetWidth(void *m) { (void)m; return CGDisplayPixelsWide(CGMainDisplayID()); }
size_t CGDisplayModeGetHeight(void *m) { (void)m; return CGDisplayPixelsHigh(CGMainDisplayID()); }
u32 CGDisplayIDToOpenGLDisplayMask(u32 d) { (void)d; return 1; }
void *CGDirectDisplayCopyCurrentMetalDevice(u32 d) { (void)d; return 0; }   /* sem GPU */
/* CGRect*DictionaryRepresentation está em cgdict.c (mesmo layout de quatro doubles). */
/* cores e espaços de cor */
id CGColorGetConstantColor(id name) {
    const char *n = name ? cx_cstr(name) : "";
    static id black, white, clear;
    if (!strcmp(n, "kCGColorWhite")) { if (!white) white = CGColorCreateGenericRGB(1, 1, 1, 1); return white; }
    if (!strcmp(n, "kCGColorClear")) { if (!clear) clear = CGColorCreateGenericRGB(0, 0, 0, 0); return clear; }
    if (!black) black = CGColorCreateGenericRGB(0, 0, 0, 1);
    return black;
}
id CGColorCreateCopyByMatchingToColorSpace(void *space, int intent, id color, id opts) { (void)space; (void)intent; (void)opts; return color ? objc_retain(color) : 0; }
void *CGColorSpaceCreateICCBased(size_t n, const double *range, void *profile, void *alt) { (void)n; (void)range; (void)profile; (void)alt; return CGColorSpaceCreateDeviceRGB(); }
id CGColorSpaceCopyICCData(void *space) { (void)space; return 0; }
/* fontes CG: o Skia usa CTFont; com NULL ele não segue o caminho de CGFont */
void CGFontRelease(void *f) { if (f) objc_release((id)f); }
u16 CGFontGetGlyphWithGlyphName(void *f, id name) { (void)f; (void)name; return 0; }
void CGContextSetFont(void *c, void *f) { (void)c; (void)f; }
void CGContextSetFontSize(void *c, double s) { (void)c; (void)s; }
void CGContextShowGlyphsAtPositions(void *c, const u16 *g, const cg_pt_t *p, size_t n) { (void)c; (void)g; (void)p; (void)n; }
void CGContextClipToMask(void *c, cg_rect_t r, void *mask) { (void)c; (void)r; (void)mask; }
void *CGImageMaskCreate(size_t w, size_t h, size_t bpc, size_t bpp, size_t bpr, void *prov, const double *dec, u8 interp) {
    (void)w; (void)h; (void)bpc; (void)bpp; (void)bpr; (void)prov; (void)dec; (void)interp; return 0;
}
void CGContextConcatCTM(void *c, cg_affine_t t);
void CGContextRotateCTM(void *c, double a) { cg_affine_t t = {cos(a), sin(a), -sin(a), cos(a), 0, 0}; CGContextConcatCTM(c, t); }
cg_rect_t CGContextConvertRectToDeviceSpace(void *c, cg_rect_t r) { (void)c; return r; }
/* WindowServer privado (CGS*): sucesso sem efeito */
int CGSSetWindowAlpha(int cid, int wid, float a) { (void)cid; (void)wid; (void)a; return 0; }
int CGSSetWindowWarp(int cid, int wid, int w, int h, const void *mesh) { (void)cid; (void)wid; (void)w; (void)h; (void)mesh; return 0; }
int CGSSetWindowTransform(int cid, int wid, cg_affine_t t) { (void)cid; (void)wid; (void)t; return 0; }
int CGSSetWindowCaptureExcludeShape(int cid, int wid, void *region) { (void)cid; (void)wid; (void)region; return 0; }
void *CGRegionCreateWithRect(cg_rect_t r) { (void)r; return token(); }
/* PDF: sem gerador de PDF; o "Salvar como PDF" do Chromium informa a falha */
void *CGDataConsumerCreateWithCFData(id data) { (void)data; return 0; }
void *CGPDFContextCreate(void *consumer, const cg_rect_t *box, id info) { (void)consumer; (void)box; (void)info; return 0; }
void CGPDFContextClose(void *c) { (void)c; }
void CGContextBeginPage(void *c, const cg_rect_t *box) { (void)c; (void)box; }
void CGContextEndPage(void *c) { (void)c; }
void CGContextDrawPDFPage(void *c, void *page) { (void)c; (void)page; }
static void prov_release(void *info, const void *data, size_t n) { (void)data; (void)n; if (info) objc_release((id)info); }
void *CGDataProviderCreateWithCFData(id data) {
    if (!data) return 0;
    objc_retain(data);
    return CGDataProviderCreateWithData(data, MSG0(const void *, data, "bytes"), (size_t)MSG0(u64, data, "length"), prov_release);
}
int CGPDFPageGetRotationAngle(void *p) { (void)p; return 0; }

/* ================================================================ IOKit */
#define kIOReturnSuccess 0
#define kIOReturnUnsupported ((int)0xE00002C7)
#define kIOReturnNotFound ((int)0xE00002F0)
typedef u32 io_object_t;
id IORegistryEntrySearchCFProperty(io_object_t e, const char *plane, id key, void *alloc, u32 opts) { (void)e; (void)plane; (void)key; (void)alloc; (void)opts; return 0; }
kern_return_t IORegistryEntryGetParentEntry(io_object_t e, const char *plane, io_object_t *p) { (void)e; (void)plane; if (p) *p = 0; return kIOReturnNotFound; }
id IORegistryEntryIDMatching(u64 id_) { (void)id_; return CFDictionaryCreateMutable(0, 0, 0, 0); }
id IOBSDNameMatching(u32 port, u32 opts, const char *name) { (void)port; (void)opts; (void)name; return CFDictionaryCreateMutable(0, 0, 0, 0); }
kern_return_t IOCreatePlugInInterfaceForService(io_object_t s, id type, id iface, void ***plugin, i32 *score) { (void)s; (void)type; (void)iface; if (plugin) *plugin = 0; if (score) *score = 0; return kIOReturnUnsupported; }
kern_return_t IOServiceAddMatchingNotification(void *port, const char *type, id match, void *cb, void *ref, io_object_t *it) {
    (void)port; (void)type; (void)cb; (void)ref;
    if (match) objc_release(match);         /* a função consome a referência do dicionário */
    if (it) *it = 0;
    return kIOReturnSuccess;
}
kern_return_t IOServiceAddInterestNotification(void *port, io_object_t s, const char *type, void *cb, void *ref, io_object_t *n) { (void)port; (void)s; (void)type; (void)cb; (void)ref; if (n) *n = 0; return kIOReturnSuccess; }
void IONotificationPortSetDispatchQueue(void *port, void *q) { (void)port; (void)q; }
kern_return_t IORegistryEntryGetRegistryEntryID(io_object_t e, u64 *id_) { if (id_) *id_ = e; return e ? kIOReturnSuccess : kIOReturnNotFound; }
kern_return_t IORegistryEntryCreateIterator(io_object_t e, const char *plane, u32 opts, io_object_t *it) { (void)e; (void)plane; (void)opts; if (it) *it = 0; return kIOReturnSuccess; }
u8 IOObjectConformsTo(io_object_t o, const char *name) { (void)o; (void)name; return 0; }
kern_return_t IORegistryEntryCreateCFProperties(io_object_t e, id *props, void *alloc, u32 opts) { (void)e; (void)alloc; (void)opts; if (props) *props = CFDictionaryCreateMutable(0, 0, 0, 0); return kIOReturnSuccess; }
kern_return_t IORegistryEntryGetName(io_object_t e, char *name) { (void)e; if (name) name[0] = 0; return kIOReturnSuccess; }
kern_return_t IOServiceClose(u32 conn) { (void)conn; return kIOReturnSuccess; }
/* HID: nenhum dispositivo além de teclado/mouse do sistema; o WebHID/gamepads ficam vazios */
int IOHIDCheckAccess(u32 type) { (void)type; return 1; /* kIOHIDAccessTypeDenied */ }
void *IOHIDManagerCreate(void *alloc, u32 opts) { (void)alloc; (void)opts; return token(); }
u64 IOHIDManagerGetTypeID(void) { return 0x48494D; }
void IOHIDManagerSetDeviceMatchingMultiple(void *m, id arr) { (void)m; (void)arr; }
int IOHIDManagerOpen(void *m, u32 opts) { (void)m; (void)opts; return kIOReturnSuccess; }
int IOHIDManagerClose(void *m, u32 opts) { (void)m; (void)opts; return kIOReturnSuccess; }
void IOHIDManagerScheduleWithRunLoop(void *m, void *rl, id mode) { (void)m; (void)rl; (void)mode; }
void IOHIDManagerUnscheduleFromRunLoop(void *m, void *rl, id mode) { (void)m; (void)rl; (void)mode; }
void IOHIDManagerRegisterInputValueCallback(void *m, void *cb, void *ctx) { (void)m; (void)cb; (void)ctx; }
void IOHIDManagerRegisterDeviceRemovalCallback(void *m, void *cb, void *ctx) { (void)m; (void)cb; (void)ctx; }
void IOHIDManagerRegisterDeviceMatchingCallback(void *m, void *cb, void *ctx) { (void)m; (void)cb; (void)ctx; }
void *IOHIDDeviceCreate(void *alloc, io_object_t s) { (void)alloc; (void)s; return 0; }
int IOHIDDeviceOpen(void *d, u32 o) { (void)d; (void)o; return kIOReturnUnsupported; }
int IOHIDDeviceClose(void *d, u32 o) { (void)d; (void)o; return kIOReturnSuccess; }
void IOHIDDeviceScheduleWithRunLoop(void *d, void *rl, id mode) { (void)d; (void)rl; (void)mode; }
void IOHIDDeviceUnscheduleFromRunLoop(void *d, void *rl, id mode) { (void)d; (void)rl; (void)mode; }
void IOHIDDeviceRegisterInputReportCallback(void *d, u8 *rep, long len, void *cb, void *ctx) { (void)d; (void)rep; (void)len; (void)cb; (void)ctx; }
int IOHIDDeviceSetReport(void *d, int type, long id_, const u8 *r, long len) { (void)d; (void)type; (void)id_; (void)r; (void)len; return kIOReturnUnsupported; }
int IOHIDDeviceGetReport(void *d, int type, long id_, u8 *r, long *len) { (void)d; (void)type; (void)id_; (void)r; if (len) *len = 0; return kIOReturnUnsupported; }
id IOHIDDeviceGetProperty(void *d, id key) { (void)d; (void)key; return 0; }
io_object_t IOHIDDeviceGetService(void *d) { (void)d; return 0; }
id IOHIDDeviceCopyMatchingElements(void *d, id match, u32 opts) { (void)d; (void)match; (void)opts; return empty_array(); }
u32 IOHIDElementGetUsage(void *e) { (void)e; return 0; }
u32 IOHIDElementGetUsagePage(void *e) { (void)e; return 0; }
void *IOHIDElementGetParent(void *e) { (void)e; return 0; }
int IOHIDElementGetType(void *e) { (void)e; return 0; }
u32 IOHIDElementGetReportSize(void *e) { (void)e; return 0; }
u32 IOHIDElementGetReportID(void *e) { (void)e; return 0; }
long IOHIDElementGetLogicalMax(void *e) { (void)e; return 0; }
long IOHIDElementGetLogicalMin(void *e) { (void)e; return 0; }
void *IOHIDElementGetDevice(void *e) { (void)e; return 0; }
long IOHIDValueGetIntegerValue(void *v) { (void)v; return 0; }
const u8 *IOHIDValueGetBytePtr(void *v) { (void)v; return 0; }
long IOHIDValueGetLength(void *v) { (void)v; return 0; }
void *IOHIDValueGetElement(void *v) { (void)v; return 0; }
/* energia: sem bateria (máquina na tomada) */
id IOPSCopyPowerSourcesInfo(void) { return 0; }
id IOPSCopyPowerSourcesList(id blob) { (void)blob; return empty_array(); }
id IOPSGetPowerSourceDescription(id blob, id ps) { (void)blob; (void)ps; return 0; }
void *IOPSNotificationCreateRunLoopSource(void *cb, void *ctx) { (void)cb; (void)ctx; return 0; }
int IOPMAssertionCreateWithDescription(id type, id name, id details, id human, id loc, double timeout, id action, u32 *out) {
    (void)type; (void)name; (void)details; (void)human; (void)loc; (void)timeout; (void)action;
    static u32 next = 1;
    if (out) *out = next++;
    return kIOReturnSuccess;
}

/* ================================================================ Carbon / HIToolbox */
u32 GetCurrentKeyModifiers(void) { return 0; }
/* atalhos globais: registrados mas nunca disparam (o servidor de janelas não tem esse recurso) */
OSStatus RegisterEventHotKey(u32 code, u32 mods, u64 hkid, void *target, u32 opts, void **out) { (void)code; (void)mods; (void)hkid; (void)target; (void)opts; if (out) *out = (void *)token(); return 0; }
OSStatus UnregisterEventHotKey(void *ref) { if (ref) objc_release((id)ref); return 0; }
static u8 app_target_storage[16];
void *GetApplicationEventTarget(void) { return app_target_storage; }
OSStatus InstallEventHandler(void *target, void *handler, u32 n, const void *types, void *data, void **out) { (void)target; (void)handler; (void)n; (void)types; (void)data; if (out) *out = (void *)token(); return 0; }
OSStatus RemoveEventHandler(void *ref) { if (ref) objc_release((id)ref); return 0; }
OSStatus GetEventParameter(void *ev, u32 name, u32 type, u32 *outType, u64 size, u64 *outSize, void *data) { (void)ev; (void)name; (void)type; (void)outType; (void)size; (void)outSize; (void)data; return -9870; /* eventParameterNotFoundErr */ }
void ReleaseEvent(void *ev) { (void)ev; }
OSStatus ReceiveNextEvent(u32 n, const void *types, double timeout, u8 pull, void **out) { (void)n; (void)types; (void)timeout; (void)pull; if (out) *out = 0; return -9875; /* eventLoopTimedOutErr */ }
OSStatus AEProcessEvent(void *ev) { (void)ev; return 0; }
/* Entrada segura (campos de senha): contador como no Carbon; Enable/Disable em pares. */
static volatile int g_secure_input;
OSStatus EnableSecureEventInput(void) { __sync_fetch_and_add(&g_secure_input, 1); return 0; }
OSStatus DisableSecureEventInput(void) {
    int v = g_secure_input;
    while (v > 0 && !__sync_bool_compare_and_swap(&g_secure_input, v, v - 1)) v = g_secure_input;
    return 0;
}
u8 IsSecureEventInputEnabled(void) { return g_secure_input > 0; }
OSStatus TSMSetDocumentProperty(void *doc, u32 tag, u32 size, void *data) { (void)doc; (void)tag; (void)size; (void)data; return 0; }
OSStatus TSMRemoveDocumentProperty(void *doc, u32 tag) { (void)doc; (void)tag; return 0; }
u8 LMGetKbdLast(void) { return 0; }
u8 LMGetKbdType(void) { return 40; /* teclado ANSI genérico */ }
/* fontes de entrada (TIS): um teclado ABNT2, como o layout padrão do mOS */
static id tis_source(void) {
    static id src;
    if (!src) {
        src = CFDictionaryCreateMutable(0, 4, 0, 0);
        CFDictionarySetValue(src, cx_str("TISPropertyInputSourceID"), cx_str("com.apple.keylayout.Brazilian-ABNT2"));
        CFDictionarySetValue(src, cx_str("kTISPropertyLocalizedName"), cx_str("Brasileiro – ABNT2"));
        id lang = cx_str("pt");
        CFDictionarySetValue(src, cx_str("kTISPropertyInputSourceLanguages"), CFArrayCreate(0, &lang, 1, 0));
    }
    return src;
}
id TISCopyCurrentKeyboardInputSource(void) { return objc_retain(tis_source()); }
id TISCopyCurrentKeyboardLayoutInputSource(void) { return objc_retain(tis_source()); }
id TISCopyCurrentASCIICapableKeyboardLayoutInputSource(void) { return objc_retain(tis_source()); }
id TISCreateASCIICapableInputSourceList(void) { id s = tis_source(); return CFArrayCreate(0, &s, 1, 0); }
/* uchr não existe aqui: NULL faz o Chromium usar a tabela de teclas própria */
void *TISGetInputSourceProperty(id src, id key) { return src && key ? (void *)CFDictionaryGetValue(src, key) : 0; }

/* ================================================================ CoreServices */
OSStatus UCKeyTranslate(const void *layout, u16 code, u16 action, u32 mods, u32 type, u32 opts, u32 *dead, long max, long *len, u16 *out) {
    (void)layout; (void)code; (void)action; (void)mods; (void)type; (void)opts; (void)dead; (void)max; (void)out;
    if (len) *len = 0;
    return -50;   /* paramErr: sem layout uchr */
}
/* Itens de login: uma linha (caminho absoluto) em ~/Library/Preferences/com.mos.loginitems.
 * Quem reescreve o arquivo é loginitems.c. O init deixa de subir o caminho que saiu.
 * openSystemSettingsLoginItems continua em slack_frameworks.c, sem janela (Interface). */
static const char *login_url_path(id url) {
    const char *s;
    if (!url) return 0;
    if (cx_responds(object_getClass(url), sel_registerName("path"))) {
        id p = MSG0(id, url, "path");
        if (cx_is_str(p) && ((nsstr_t *)p)->s && ((nsstr_t *)p)->s[0] == '/') return ((nsstr_t *)p)->s;
    }
    s = cx_cstr(url);
    if (s && s[0] == '/') return s;
    if (s && !strncmp(s, "file://", 7)) return s + 7;
    return 0;
}
static id login_file_url(const char *path) {
    id u;
    if (!path) return 0;
    u = MSG(id, cls("NSURL"), "fileURLWithPath:", cx_str(path));
    return u ? objc_retain(u) : 0;
}
/* Só a lista de itens de login tem armazenamento. A de favoritos (kLSSharedFileListFavoriteItems, a barra lateral do
 * Finder) não existe no mOS: a lista vem vazia e inserir nela falha, em vez de virar item de login. */
static id g_fav_list;
id LSSharedFileListCreate(void *alloc, id type, id opts) {
    (void)alloc; (void)opts;
    const char *t = type ? cx_cstr(type) : 0;
    if (t && !strcmp(t, "com.apple.LSSharedFileList.FavoriteItems")) {
        if (!g_fav_list) g_fav_list = token();
        return objc_retain(g_fav_list);
    }
    return token();
}
id LSSharedFileListCopySnapshot(id list, u32 *seed) {
    if (list && list == g_fav_list) { if (seed) *seed = 1; return objc_retain(MSG0(id, cls("NSArray"), "array")); }
    return cx_login_snapshot(seed);
}
id LSSharedFileListInsertItemURL(id list, id after, id name, void *icon, id url, id props, id remove) {
    if (list && list == g_fav_list) return 0;
    const char *path = login_url_path(url);
    (void)after; (void)name; (void)icon; (void)props; (void)remove;
    if (cx_login_add(path) != 0) return 0;
    return login_file_url(path);
}
OSStatus LSSharedFileListItemRemove(id list, id item) {
    if (list && list == g_fav_list) return -43;                /* fnfErr: não há item de favorito */
    const char *path = login_url_path(item);
    return cx_login_remove(path) == 0 ? 0 : -50;
}
id LSSharedFileListItemCopyResolvedURL(id item, u32 flags, id *err) {
    (void)flags;
    if (err) *err = 0;
    return login_file_url(login_url_path(item));
}
id LSSharedFileListItemCopyProperty(id item, id name) { (void)item; (void)name; return 0; }
static OSStatus ls_open(id url) {
    id ws = MSG0(id, cls("NSWorkspace"), "sharedWorkspace");
    if (!ws || !url || !cx_responds(object_getClass(ws), sel_registerName("openURL:"))) return -10814;
    return MSG(BOOL, ws, "openURL:", url) ? 0 : -10814;
}
typedef struct { id appURL; id itemURLs; const void *passThruParams; u32 launchFlags; void *asyncRefCon; } ls_spec_t;
OSStatus LSOpenFromURLSpec(const ls_spec_t *spec, id *launched) {
    if (launched) *launched = 0;
    if (!spec) return -50;
    extern int cx_ls_open_urls(id app, id urls);   /* openurl.c: com o app pedido (appURL) ou o padrão de cada item */
    if (spec->appURL || spec->itemURLs) return cx_ls_open_urls(spec->appURL, spec->itemURLs);
    return -10814;   /* kLSApplicationNotFoundErr */
}
/* LSCopyDefaultApplicationURLForURL e as outras consultas de quem abre o quê: openurl.c */
/* LSSetDefaultHandlerForURLScheme: openurl.c (banco do Launch Services no kernel) */
/* FSEventStreamSetExclusionPaths: fsevents.c */
/* Apple Events: registro aceito; não há outro app para conversar */
typedef struct { u32 type; void *data; } ae_desc_t;
OSStatus AEInstallEventHandler(u32 cls_, u32 id_, void *handler, i64 refcon, u8 sys) { (void)cls_; (void)id_; (void)handler; (void)refcon; (void)sys; return 0; }
OSStatus AERemoveEventHandler(u32 cls_, u32 id_, void *handler, u8 sys) { (void)cls_; (void)id_; (void)handler; (void)sys; return 0; }
OSStatus AEDisposeDesc(ae_desc_t *d) { if (d) { d->type = 0x6E756C6C; /* 'null' */ d->data = 0; } return 0; }
OSStatus AECreateDesc(u32 type, const void *data, long size, ae_desc_t *out) { (void)data; (void)size; if (out) { out->type = type; out->data = 0; } return 0; }
OSStatus AECreateAppleEvent(u32 cls_, u32 id_, const ae_desc_t *target, i16 ret, i32 tx, ae_desc_t *out) { (void)cls_; (void)id_; (void)target; (void)ret; (void)tx; if (out) { out->type = 0x61657674; /* 'aevt' */ out->data = 0; } return 0; }
OSStatus AEPutParamDesc(ae_desc_t *ev, u32 key, const ae_desc_t *d) { (void)ev; (void)key; (void)d; return 0; }
OSStatus AEGetParamDesc(const ae_desc_t *ev, u32 key, u32 type, ae_desc_t *out) { (void)ev; (void)key; (void)type; if (out) { out->type = 0x6E756C6C; out->data = 0; } return -1701; /* errAEDescNotFound */ }
OSStatus AESendMessage(const ae_desc_t *ev, ae_desc_t *reply, i32 mode, long timeout) { (void)ev; (void)mode; (void)timeout; if (reply) { reply->type = 0x6E756C6C; reply->data = 0; } return -1708; /* errAEEventNotHandled */ }

/* ================================================================ SystemConfiguration */
/* SCNetworkReachability*: wa_net.c */
id SCDynamicStoreKeyCreateProxies(void *alloc) { (void)alloc; return objc_retain(cx_str("State:/Network/Global/Proxies")); }

/* ================================================================ Security (Keychain antigo, SSL legado) */
#define errSecUnimplemented (-4)
OSStatus SecTrustedApplicationCreateFromRequirement(const char *req, void **app) { (void)req; if (app) *app = 0; return errSecUnimplemented; }
OSStatus SSLCopyDistinguishedNames(void *ctx, id *names) { (void)ctx; if (names) *names = 0; return 0; }
OSStatus SSLSetCertificateAuthorities(void *ctx, id certs, u8 replace) { (void)ctx; (void)certs; (void)replace; return 0; }
OSStatus SSLSetClientSideAuthenticate(void *ctx, int auth) { (void)ctx; (void)auth; return 0; }
OSStatus SecCertificateAddToKeychain(void *cert, void *kc) { (void)cert; (void)kc; return 0; }
OSStatus SecItemExport(id item, u32 fmt, u32 flags, const void *params, id *out) { (void)item; (void)fmt; (void)flags; (void)params; if (out) *out = 0; return errSecUnimplemented; }
/* o mOS tem um só keychain (security.c): abrir/criar devolvem uma referência a ele */
OSStatus SecKeychainOpen(const char *path, void **kc) { (void)path; if (kc) *kc = (void *)token(); return 0; }
OSStatus SecKeychainCreate(const char *path, u32 plen, const void *pw, u8 prompt, void *access, void **kc) { (void)path; (void)plen; (void)pw; (void)prompt; (void)access; if (kc) *kc = (void *)token(); return 0; }
OSStatus SecKeychainDelete(void *kc) { (void)kc; return 0; }
OSStatus SecKeychainUnlock(void *kc, u32 plen, const void *pw, u8 use) { (void)kc; (void)plen; (void)pw; (void)use; return 0; }
OSStatus SecKeychainSetSettings(void *kc, const void *settings) { (void)kc; (void)settings; return 0; }
OSStatus SecKeychainItemCreateCopy(void *item, void *kc, void *access, void **out) { (void)kc; (void)access; if (out) *out = item ? (void *)objc_retain((id)item) : 0; return item ? 0 : -50; }
/* sessão de login com acesso gráfico e TTY */
OSStatus SessionGetInfo(u32 session, u32 *sid, u32 *attrs) { (void)session; if (sid) *sid = 100000; if (attrs) *attrs = 0x0010 | 0x0020; return 0; }
