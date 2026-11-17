/*
 * mOS libSystem — ícones e imagens do AppKit:
 *   .icns no NSImage (icon_core.c, teste userland/tests-appkit/icon_host.c);
 *   -[NSImage initWithContentsOfURL:], initByReferencingFile:, initByReferencingURL:, setName:/name e o registro de
 *   nomes do +imageNamed:; -[NSBundle imageForResource:], pathForImageResource:;
 *   imagens de sistema do +imageNamed: (NSImageName*: pastas, documentos, alertas, usuários, estados e os "Template"
 *   da interface e da Touch Bar), desenhadas aqui com o NSBezierPath a partir de uma descrição de caminho compacta.
 *   O desenho é do mOS (nenhuma arte da Apple);
 *   -[NSWorkspace iconForFile:], iconForFiles:, iconForFileType:, iconForContentType:: o ícone do bundle (.app,
 *   CFBundleIconFile), o ícone de documento do app que abre o tipo (CFBundleDocumentTypes/CFBundleTypeIconFile), ou
 *   pasta/documento genéricos.
 * Registro: cx_appkit_icons_init(), chamado do cx_appkit_input_init.
 */
#include "foundation.h"
#include "libc.h"
#include "sys.h"
#include "icon_core.h"
#include "../../kernel/include/compat_gui.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy);
id objc_getAssociatedObject(id obj, const void *key);
id cx_color(double r, double g, double b, double a);
extern id NSFontAttributeName, NSForegroundColorAttributeName;

typedef struct { double x, y; } P;
typedef struct { double w, h; } S;
typedef struct { double x, y, w, h; } R;
static cx_class_t *kls(const char *n) { return cx_class_named(n); }
static SEL sl(const char *n) { return sel_registerName(n); }
static id str(const char *s) { return cx_str(s); }

/* ================================================================ .icns */
u32 *cx_icns_argb(const u8 *d, u64 n, int want, int *w, int *h) {
    ic_pick k;
    if (!ic_pick_best(d, (long)n, want, &k)) return 0;
    if (k.kind == IC_PNG) {
        long need = SYS4(GUI_DECODE_IMAGE, d + k.off, k.len, 0, 0);
        if (need <= 8) return 0;
        u8 *out = malloc((usize)need);
        if (!out || SYS4(GUI_DECODE_IMAGE, d + k.off, k.len, out, need) != need) { free(out); return 0; }
        *w = (int)((u32 *)out)[0];
        *h = (int)((u32 *)out)[1];
        u32 *px = malloc((usize)*w * (usize)*h * 4);
        if (px) memcpy(px, out + 8, (usize)*w * (usize)*h * 4);
        free(out);
        return px;
    }
    long px_n = (long)k.side * k.side;
    u32 *px = malloc((usize)px_n * 4);
    u8 *scratch = malloc((usize)px_n * 4);
    int ok = px && scratch && (k.kind == IC_ARGB ? ic_decode_argb(d + k.off, k.len, k.side, px, scratch)
                                                 : ic_decode_rle(d + k.off, k.len, k.side, k.it32, k.mask_off ? d + k.mask_off : 0, k.mask_len, px, scratch));
    free(scratch);
    if (!ok) { free(px); return 0; }
    *w = *h = k.side;
    return px;
}

/* ================================================================ NSImage: URL, referência, nomes */
static id im_init_url(id self, SEL c, id url) {
    (void)c;
    id d = url ? MSG(id, (id)kls("NSData"), "dataWithContentsOfURL:", url) : 0;
    return d ? MSG(id, self, "initWithData:", d) : 0;
}
static id im_init_ref_file(id self, SEL c, id path) { (void)c; return path ? MSG(id, self, "initWithContentsOfFile:", path) : 0; }
static id g_names;   /* nome → NSImage (setName:) */
static char k_name;
static BOOL im_set_name(id self, SEL c, id name) {
    (void)c;
    if (!g_names) g_names = MSG0(id, MSG0(id, (id)kls("NSMutableDictionary"), "alloc"), "init");
    id old = objc_getAssociatedObject(self, &k_name);
    if (name && MSG(id, g_names, "objectForKey:", name) && MSG(id, g_names, "objectForKey:", name) != self) return NO;   /* nome já usado */
    if (old) MSG(void, g_names, "removeObjectForKey:", old);
    objc_setAssociatedObject(self, &k_name, name, 3);
    if (name) MSG(void, g_names, "setObject:forKey:", self, name);
    return YES;
}
static id im_name(id self, SEL c) { (void)c; return objc_getAssociatedObject(self, &k_name); }

/* ================================================================ desenho das imagens de sistema
 * Caminho em unidades do quadrado (0..1, y para cima): M x y, L x y, C x1 y1 x2 y2 x y, Z, A cx cy r a0 a1 horário,
 * O x y w h (oval), R x y w h (retângulo), Q x y w h raio (retângulo arredondado). */
static double g_s;
static const char *num(const char *p, double *v) {
    while (*p == ' ' || *p == ',') p++;
    int neg = 0;
    if (*p == '-') { neg = 1; p++; }
    double r = 0, f = 0.1;
    while (*p >= '0' && *p <= '9') r = r * 10 + (*p++ - '0');
    if (*p == '.') { p++; while (*p >= '0' && *p <= '9') { r += (*p++ - '0') * f; f /= 10; } }
    *v = neg ? -r : r;
    return p;
}
static P pt(double x, double y) { P p = {x * g_s, y * g_s}; return p; }
static id build_path(const char *d) {
    id path = MSG0(id, (id)kls("NSBezierPath"), "bezierPath");
    double v[7];
    for (const char *p = d; *p;) {
        char op = *p++;
        int n = op == 'M' || op == 'L' ? 2 : op == 'C' ? 6 : op == 'A' ? 6 : op == 'O' || op == 'R' ? 4 : op == 'Q' ? 5 : 0;
        if (op == ' ') continue;
        for (int i = 0; i < n; i++) p = num(p, &v[i]);
        switch (op) {
        case 'M': ((void (*)(id, SEL, P))objc_msgSend)(path, sl("moveToPoint:"), pt(v[0], v[1])); break;
        case 'L': ((void (*)(id, SEL, P))objc_msgSend)(path, sl("lineToPoint:"), pt(v[0], v[1])); break;
        case 'C': ((void (*)(id, SEL, P, P, P))objc_msgSend)(path, sl("curveToPoint:controlPoint1:controlPoint2:"), pt(v[4], v[5]), pt(v[0], v[1]), pt(v[2], v[3])); break;
        case 'Z': MSG0(void, path, "closePath"); break;
        case 'A': ((void (*)(id, SEL, P, double, double, double, BOOL))objc_msgSend)(path, sl("appendBezierPathWithArcWithCenter:radius:startAngle:endAngle:clockwise:"),
                                                                                   pt(v[0], v[1]), v[2] * g_s, v[3], v[4], (BOOL)(v[5] != 0)); break;
        case 'O': ((void (*)(id, SEL, R))objc_msgSend)(path, sl("appendBezierPathWithOvalInRect:"), (R){v[0] * g_s, v[1] * g_s, v[2] * g_s, v[3] * g_s}); break;
        case 'R': ((void (*)(id, SEL, R))objc_msgSend)(path, sl("appendBezierPathWithRect:"), (R){v[0] * g_s, v[1] * g_s, v[2] * g_s, v[3] * g_s}); break;
        case 'Q': ((void (*)(id, SEL, R, double, double))objc_msgSend)(path, sl("appendBezierPathWithRoundedRect:xRadius:yRadius:"),
                                                                      (R){v[0] * g_s, v[1] * g_s, v[2] * g_s, v[3] * g_s}, v[4] * g_s, v[4] * g_s); break;
        default: break;
        }
    }
    return path;
}
/* uma camada: 'f' preenche, 'e' preenche par-ímpar, 's' traça (lw em unidades), 't' texto (d = letras; lw = estilo:
 * 0 negrito, 1 itálico, 2 sublinhado, 3 riscado, 4 normal) */
typedef struct { char op; const char *d; float r, g, b, a, lw; } layer_t;
#define K 0, 0, 0, 1
static void paint(const layer_t *L) {
    id col = cx_color(L->r, L->g, L->b, L->a);
    if (L->op == 't') {
        int style = (int)L->lw;
        double size = g_s * 0.72;
        id font = style == 0 ? MSG(id, (id)kls("NSFont"), "boldSystemFontOfSize:", size) : MSG(id, (id)kls("NSFont"), "systemFontOfSize:", size);
        if (style == 1) {
            id fm = MSG0(id, (id)kls("NSFontManager"), "sharedFontManager");
            if (fm) font = MSG(id, fm, "convertFont:toHaveTrait:", font, (u64)1);
        }
        id attrs = MSG0(id, MSG0(id, (id)kls("NSMutableDictionary"), "alloc"), "init");
        if (font) MSG(void, attrs, "setObject:forKey:", font, NSFontAttributeName);
        MSG(void, attrs, "setObject:forKey:", col, NSForegroundColorAttributeName);
        id s = str(L->d);
        S z = ((S (*)(id, SEL, id))objc_msgSend)(s, sl("sizeWithAttributes:"), attrs);
        P at = {(g_s - z.w) / 2, (g_s - z.h) / 2};
        ((void (*)(id, SEL, P, id))objc_msgSend)(s, sl("drawAtPoint:withAttributes:"), at, attrs);
        objc_release(attrs);
        if (style == 2 || style == 3) {
            double y = style == 2 ? at.y + z.h * 0.12 : at.y + z.h * 0.45;
            id p = MSG0(id, (id)kls("NSBezierPath"), "bezierPath");
            ((void (*)(id, SEL, P))objc_msgSend)(p, sl("moveToPoint:"), (P){at.x, y});
            ((void (*)(id, SEL, P))objc_msgSend)(p, sl("lineToPoint:"), (P){at.x + z.w, y});
            MSG(void, p, "setLineWidth:", g_s * 0.06);
            MSG0(void, col, "setStroke");
            MSG0(void, p, "stroke");
        }
        return;
    }
    id p = build_path(L->d);
    if (L->op == 's') {
        MSG(void, p, "setLineWidth:", (double)(L->lw > 0 ? L->lw : 0.08) * g_s);
        MSG(void, p, "setLineCapStyle:", (i64)1);
        MSG(void, p, "setLineJoinStyle:", (i64)1);
        MSG0(void, col, "setStroke");
        MSG0(void, p, "stroke");
    } else {
        if (L->op == 'e') MSG(void, p, "setWindingRule:", (i64)1);
        MSG0(void, col, "setFill");
        MSG0(void, p, "fill");
    }
}

/* ---- glifos (preto; as imagens "Template") */
#define G(name, ...) static const layer_t name[] = {__VA_ARGS__, {0, 0, 0, 0, 0, 0, 0}};
G(gl_plus, {'s', "M .5 .2 L .5 .8 M .2 .5 L .8 .5", K, .1})
G(gl_minus, {'s', "M .2 .5 L .8 .5", K, .1})
G(gl_x, {'s', "M .27 .27 L .73 .73 M .73 .27 L .27 .73", K, .1})
G(gl_left, {'s', "M .62 .18 L .34 .5 L .62 .82", K, .1})
G(gl_right, {'s', "M .38 .18 L .66 .5 L .38 .82", K, .1})
G(gl_up, {'s', "M .18 .36 L .5 .66 L .82 .36", K, .1})
G(gl_down, {'s', "M .18 .64 L .5 .34 L .82 .64", K, .1})
G(gl_tri_r, {'f', "M .3 .18 L .8 .5 L .3 .82 Z", K, 0})
G(gl_tri_l, {'f', "M .7 .18 L .2 .5 L .7 .82 Z", K, 0})
G(gl_check, {'s', "M .2 .5 L .42 .26 L .82 .76", K, .11})
G(gl_mixed, {'s', "M .25 .5 L .75 .5", K, .11})
G(gl_gear, {'e', "O .2 .2 .6 .6 O .38 .38 .24 .24", K, 0}, {'s', "M .5 .08 L .5 .92 M .08 .5 L .92 .5 M .2 .2 L .8 .8 M .8 .2 L .2 .8", K, .12},
  {'f', "O .38 .38 .24 .24", 1, 1, 1, 0, 0})
G(gl_refresh, {'s', "A .5 .5 .3 110 380 0", K, .09}, {'f', "M .64 .78 L .92 .64 L .74 .5 Z", K, 0})
G(gl_lock, {'f', "Q .22 .08 .56 .44 .06", K, 0}, {'s', "M .34 .52 L .34 .66 A .5 .66 .16 180 0 1 L .66 .52", K, .09})
G(gl_unlock, {'f', "Q .22 .08 .56 .44 .06", K, 0}, {'s', "M .66 .52 L .66 .7 A .82 .7 .16 180 0 1", K, .09})
G(gl_share, {'s', "M .36 .56 L .24 .56 L .24 .1 L .76 .1 L .76 .56 L .64 .56 M .5 .3 L .5 .9 M .36 .76 L .5 .9 L .64 .76", K, .08})
G(gl_search, {'s', "O .16 .34 .46 .46 M .56 .38 L .84 .12", K, .1})
G(gl_eye, {'s', "M .08 .5 C .3 .82 .7 .82 .92 .5 C .7 .18 .3 .18 .08 .5 Z", K, .08}, {'f', "O .39 .39 .22 .22", K, 0})
G(gl_house, {'s', "M .12 .5 L .5 .84 L .88 .5 M .24 .58 L .24 .14 L .76 .14 L .76 .58", K, .08}, {'f', "R .42 .14 .16 .26", K, 0})
G(gl_book, {'s', "M .5 .2 L .5 .8 C .4 .86 .2 .86 .1 .8 L .1 .2 C .2 .26 .4 .26 .5 .2 C .6 .26 .8 .26 .9 .2 L .9 .8 C .8 .86 .6 .86 .5 .8", K, .07})
G(gl_grid, {'f', "R .14 .56 .3 .3 R .56 .56 .3 .3 R .14 .14 .3 .3 R .56 .14 .3 .3", K, 0})
G(gl_list, {'f', "R .12 .7 .12 .1 R .3 .72 .58 .06 R .12 .45 .12 .1 R .3 .47 .58 .06 R .12 .2 .12 .1 R .3 .22 .58 .06", K, 0})
G(gl_columns, {'s', "R .1 .2 .8 .6 M .37 .2 L .37 .8 M .63 .2 L .63 .8", K, .07})
G(gl_flow, {'f', "R .34 .28 .32 .44", K, 0}, {'s', "M .1 .34 L .24 .38 L .24 .62 L .1 .66 Z M .9 .34 L .76 .38 L .76 .62 L .9 .66 Z", K, .06})
G(gl_slides, {'s', "R .1 .2 .8 .6", K, .07}, {'f', "M .42 .35 L .63 .5 L .42 .65 Z", K, 0})
G(gl_full_in, {'s', "M .14 .4 L .14 .14 L .4 .14 M .14 .14 L .38 .38 M .86 .6 L .86 .86 L .6 .86 M .86 .86 L .62 .62", K, .08})
G(gl_full_out, {'s', "M .4 .14 L .4 .4 L .14 .4 M .4 .4 L .14 .14 M .6 .86 L .6 .6 L .86 .6 M .6 .6 L .86 .86", K, .08})
G(gl_follow, {'f', "O .08 .08 .84 .84", K, 0}, {'s', "M .32 .5 L .66 .5 M .54 .37 L .67 .5 L .54 .63", 1, 1, 1, 1, .09})
G(gl_reveal, {'f', "O .08 .08 .84 .84", K, 0}, {'s', "O .3 .4 .24 .24 M .52 .42 L .68 .27", 1, 1, 1, 1, .07})
G(gl_invalid, {'f', "O .08 .08 .84 .84", K, 0}, {'s', "M .5 .4 L .5 .72", 1, 1, 1, 1, .1}, {'f', "O .45 .2 .1 .1", 1, 1, 1, 1, 0})
G(gl_stop_free, {'f', "O .08 .08 .84 .84", K, 0}, {'s', "M .34 .34 L .66 .66 M .66 .34 L .34 .66", 1, 1, 1, 1, .09})
G(gl_refresh_free, {'f', "O .08 .08 .84 .84", K, 0}, {'s', "A .5 .5 .2 110 380 0", 1, 1, 1, 1, .07}, {'f', "M .58 .66 L .78 .58 L .64 .46 Z", 1, 1, 1, 1, 0})
G(gl_info_t, {'s', "O .1 .1 .8 .8 M .5 .26 L .5 .54", K, .08}, {'f', "O .45 .64 .1 .1", K, 0})
G(gl_bt, {'s', "M .3 .3 L .7 .7 L .5 .88 L .5 .12 L .7 .3 L .3 .7", K, .08})
G(gl_play, {'f', "M .3 .14 L .84 .5 L .3 .86 Z", K, 0})
G(gl_pause, {'f', "R .24 .14 .18 .72 R .58 .14 .18 .72", K, 0})
G(gl_playpause, {'f', "M .08 .2 L .46 .5 L .08 .8 Z R .56 .2 .12 .6 R .76 .2 .12 .6", K, 0})
G(gl_rec, {'f', "O .2 .2 .6 .6", K, 0})
G(gl_recstop, {'f', "R .24 .24 .52 .52", K, 0})
G(gl_rew, {'f', "M .5 .2 L .08 .5 L .5 .8 Z M .92 .2 L .5 .5 L .92 .8 Z", K, 0})
G(gl_ff, {'f', "M .08 .2 L .5 .5 L .08 .8 Z M .5 .2 L .92 .5 L .5 .8 Z", K, 0})
G(gl_tostart, {'f', "R .16 .2 .12 .6 M .86 .2 L .34 .5 L .86 .8 Z", K, 0})
G(gl_toend, {'f', "R .72 .2 .12 .6 M .14 .2 L .66 .5 L .14 .8 Z", K, 0})
G(gl_skip_f, {'s', "A .5 .5 .32 200 -60 1", K, .08}, {'f', "M .56 .72 L .78 .92 L .82 .66 Z", K, 0})
G(gl_skip_b, {'s', "A .5 .5 .32 -20 240 0", K, .08}, {'f', "M .44 .72 L .22 .92 L .18 .66 Z", K, 0})
#define SPK {'f', "M .08 .38 L .24 .38 L .44 .2 L .44 .8 L .24 .62 L .08 .62 Z", K, 0}
G(gl_vol0, SPK)
G(gl_vol1, SPK, {'s', "A .44 .5 .16 -40 40 0", K, .07})
G(gl_vol2, SPK, {'s', "A .44 .5 .16 -40 40 0 M .66 .69 A .44 .5 .29 40 -40 1", K, .07})
G(gl_vol3, SPK, {'s', "A .44 .5 .16 -40 40 0 M .66 .69 A .44 .5 .29 40 -40 1 M .76 .77 A .44 .5 .42 40 -40 1", K, .07})
G(gl_mute, SPK, {'s', "M .6 .36 L .86 .64 M .86 .36 L .6 .64", K, .08})
G(gl_volup, SPK, {'s', "M .58 .5 L .9 .5 M .74 .34 L .74 .66", K, .08})
G(gl_voldown, SPK, {'s', "M .58 .5 L .9 .5", K, .08})
G(gl_mic, {'f', "Q .37 .4 .26 .48 .13", K, 0}, {'s', "M .24 .56 C .24 .24 .76 .24 .76 .56 M .5 .3 L .5 .12 M .36 .12 L .64 .12", K, .07})
G(gl_mic_mute, {'f', "Q .37 .4 .26 .48 .13", K, 0}, {'s', "M .24 .56 C .24 .24 .76 .24 .76 .56 M .5 .3 L .5 .12 M .36 .12 L .64 .12 M .16 .12 L .84 .88", K, .07})
G(gl_clock, {'s', "O .12 .12 .76 .76 M .5 .5 L .5 .74 M .5 .5 L .68 .4", K, .07})
G(gl_alarm, {'s', "O .14 .1 .72 .72 M .5 .46 L .5 .66 M .5 .46 L .66 .38 M .12 .76 L .26 .88 M .88 .76 L .74 .88", K, .07})
G(gl_history, {'s', "A .5 .5 .34 150 510 0 M .5 .5 L .5 .72 M .5 .5 L .66 .42", K, .07}, {'f', "M .08 .62 L .28 .5 L .12 .4 Z", K, 0})
G(gl_pencil, {'s', "M .16 .16 L .26 .42 L .68 .84 L .84 .68 L .42 .26 Z M .16 .16 L .42 .26 M .6 .76 L .76 .6", K, .07})
G(gl_compose, {'s', "M .5 .84 L .14 .84 L .14 .14 L .84 .14 L .84 .5 M .4 .4 L .44 .56 L .8 .92 L .92 .8 L .56 .44 Z", K, .07})
G(gl_trash, {'s', "M .18 .76 L .82 .76 M .4 .76 L .4 .87 L .6 .87 L .6 .76 M .26 .76 L .32 .12 L .68 .12 L .74 .76 M .42 .26 L .42 .62 M .58 .26 L .58 .62", K, .07})
G(gl_download, {'s', "M .5 .86 L .5 .34 M .3 .54 L .5 .34 L .7 .54 M .14 .3 L .14 .12 L .86 .12 L .86 .3", K, .08})
#define FOLD {'s', "M .08 .18 L .08 .8 L .38 .8 L .46 .7 L .92 .7 L .92 .18 Z M .08 .6 L .92 .6", K, .07}
G(gl_folder, FOLD)
G(gl_folder_copy, FOLD, {'s', "M .34 .4 L .64 .4 M .54 .31 L .66 .4 L .54 .49 M .3 .32 L .3 .48", K, .06})
G(gl_folder_move, FOLD, {'s', "M .32 .4 L .66 .4 M .55 .31 L .67 .4 L .55 .49", K, .06})
G(gl_folder_new, FOLD, {'s', "M .5 .28 L .5 .52 M .38 .4 L .62 .4", K, .07})
G(gl_mail, {'s', "R .1 .24 .8 .52 M .1 .76 L .5 .44 L .9 .76", K, .07})
G(gl_mail_new, {'s', "R .06 .16 .66 .46 M .06 .62 L .39 .36 L .72 .62 M .82 .62 L .82 .94 M .66 .78 L .98 .78", K, .07})
G(gl_globe, {'s', "O .1 .1 .8 .8 M .1 .5 L .9 .5 M .5 .1 C .28 .3 .28 .7 .5 .9 C .72 .7 .72 .3 .5 .1", K, .06})
G(gl_playhead, {'s', "M .5 .08 L .5 .76", K, .07}, {'f', "M .32 .92 L .68 .92 L .5 .72 Z", K, 0})
G(gl_rot_l, {'s', "A .54 .44 .3 70 300 0 M .54 .74 L .38 .74", K, .08}, {'f', "M .36 .9 L .2 .74 L .36 .58 Z", K, 0})
G(gl_rot_r, {'s', "A .46 .44 .3 110 -120 1 M .46 .74 L .62 .74", K, .08}, {'f', "M .64 .9 L .8 .74 L .64 .58 Z", K, 0})
G(gl_sidebar, {'s', "R .1 .2 .8 .6 M .38 .2 L .38 .8", K, .07})
G(gl_tag, {'s', "M .12 .5 L .46 .86 L .86 .86 L .86 .46 L .5 .1 Z", K, .07}, {'f', "O .64 .64 .1 .1", K, 0})
G(gl_bold, {'t', "B", K, 0})
G(gl_italic, {'t', "I", K, 1})
G(gl_under, {'t', "U", K, 2})
G(gl_strike, {'t', "S", K, 3})
G(gl_letter_a, {'t', "A", K, 4})
G(gl_al_left, {'f', "R .14 .74 .72 .07 R .14 .56 .46 .07 R .14 .38 .72 .07 R .14 .2 .46 .07", K, 0})
G(gl_al_center, {'f', "R .14 .74 .72 .07 R .27 .56 .46 .07 R .14 .38 .72 .07 R .27 .2 .46 .07", K, 0})
G(gl_al_right, {'f', "R .14 .74 .72 .07 R .4 .56 .46 .07 R .14 .38 .72 .07 R .4 .2 .46 .07", K, 0})
G(gl_al_just, {'f', "R .14 .74 .72 .07 R .14 .56 .72 .07 R .14 .38 .72 .07 R .14 .2 .46 .07", K, 0})
G(gl_textlist, {'f', "O .12 .7 .1 .1 R .3 .72 .58 .06 O .12 .45 .1 .1 R .3 .47 .58 .06 O .12 .2 .1 .1 R .3 .22 .58 .06", K, 0})
G(gl_textbox, {'s', "R .12 .12 .76 .76 M .3 .7 L .7 .7 M .5 .7 L .5 .26", K, .07})
#define USR(dx) {'f', "O " #dx " .5 .3 .3", K, 0}
G(gl_user, {'f', "O .35 .5 .3 .3 M .14 .1 C .14 .48 .86 .48 .86 .1 Z", K, 0})
G(gl_user_add, {'f', "O .26 .5 .3 .3 M .06 .1 C .06 .48 .76 .48 .76 .1 Z", K, 0}, {'s', "M .84 .52 L .84 .84 M .68 .68 L 1 .68", K, .07})
G(gl_users, {'f', "O .14 .48 .26 .26 M .02 .14 C .02 .44 .52 .44 .52 .14 Z O .56 .52 .3 .3 M .4 .14 C .4 .5 .98 .5 .98 .14 Z", K, 0})
G(gl_phone, {'f', "M .2 .86 C .06 .7 .24 .3 .66 .1 C .74 .08 .8 .12 .86 .2 L .74 .34 C .7 .38 .64 .38 .6 .34 C .5 .38 .42 .46 .38 .56 C .42 .6 .42 .66 .38 .7 L .26 .84 C .24 .86 .22 .87 .2 .86 Z", K, 0})
G(gl_video, {'f', "Q .06 .26 .58 .48 .08 M .68 .5 L .94 .3 L .94 .7 Z", K, 0})
G(gl_bucket, {'s', "M .18 .5 L .5 .82 L .8 .52 L .48 .2 Z M .3 .62 L .7 .62", K, .07}, {'f', "M .84 .38 C .78 .26 .74 .2 .8 .14 C .86 .2 .9 .26 .84 .38 Z", K, 0})
G(gl_add_detail, {'s', "O .1 .1 .8 .8 M .5 .28 L .5 .72 M .28 .5 L .72 .5", K, .08})
G(gl_path, {'s', "R .06 .4 .2 .2 R .4 .4 .2 .2 R .74 .4 .2 .2 M .26 .5 L .4 .5 M .6 .5 L .74 .5", K, .06})
G(gl_smart, {'f', "O .1 .1 .8 .8", K, 0}, {'e', "O .3 .3 .4 .4 O .42 .42 .16 .16", 1, 1, 1, 1, 0})

/* ---- imagens coloridas */
#define FOLDER_BACK {'f', "M .04 .14 L .04 .82 Q .04 .76 .36 .1 .04 M .04 .14 L .96 .14 L .96 .72 L .44 .72 L .38 .8 L .04 .8 Z", .29, .57, .89, 1, 0}
#define FOLDER_FRONT {'f', "Q .04 .14 .92 .5 .05", .44, .72, .97, 1, 0}
G(ic_folder, FOLDER_BACK, FOLDER_FRONT)
G(ic_folder_smart, FOLDER_BACK, FOLDER_FRONT, {'e', "O .38 .22 .24 .24 O .45 .29 .1 .1", 1, 1, 1, 1, 0})
G(ic_folder_burn, FOLDER_BACK, FOLDER_FRONT, {'e', "O .36 .2 .28 .28 O .46 .3 .08 .08", 1, 1, 1, 1, 0})
#define DOC(dx, dy) {'f', "M " #dx " " #dy " L " #dx " .95 L .6 .95 L .8 .75 L .8 " #dy " Z", 1, 1, 1, 1, 0}
G(ic_doc, {'f', "M .18 .04 L .18 .96 L .6 .96 L .82 .74 L .82 .04 Z", 1, 1, 1, 1, 0},
  {'s', "M .18 .04 L .18 .96 L .6 .96 L .82 .74 L .82 .04 Z M .6 .96 L .6 .74 L .82 .74", .62, .62, .65, 1, .025})
G(ic_docs, {'f', "M .3 .14 L .3 .96 L .66 .96 L .86 .76 L .86 .14 Z", .93, .93, .95, 1, 0},
  {'s', "M .3 .14 L .3 .96 L .66 .96 L .86 .76 L .86 .14 Z", .62, .62, .65, 1, .025},
  {'f', "M .12 .04 L .12 .86 L .48 .86 L .68 .66 L .68 .04 Z", 1, 1, 1, 1, 0},
  {'s', "M .12 .04 L .12 .86 L .48 .86 L .68 .66 L .68 .04 Z M .48 .86 L .48 .66 L .68 .66", .62, .62, .65, 1, .025})
G(ic_app, {'f', "Q .06 .06 .88 .88 .2", .24, .46, .92, 1, 0}, {'s', "M .5 .76 L .26 .26 M .5 .76 L .74 .26 M .34 .42 L .66 .42", 1, 1, 1, 1, .07})
G(ic_computer, {'f', "Q .06 .3 .88 .6 .05", .2, .2, .22, 1, 0}, {'f', "R .1 .36 .8 .5", .35, .62, .95, 1, 0},
  {'f', "M .4 .3 L .6 .3 L .66 .12 L .34 .12 Z", .62, .62, .65, 1, 0})
G(ic_network, {'f', "O .06 .06 .88 .88", .3, .6, .94, 1, 0},
  {'s', "M .06 .5 L .94 .5 M .5 .06 C .26 .3 .26 .7 .5 .94 C .74 .7 .74 .3 .5 .06 M .14 .74 L .86 .74 M .14 .26 L .86 .26", 1, 1, 1, .9, .035})
G(ic_caution, {'f', "M .5 .94 L .96 .08 L .04 .08 Z", .99, .8, .1, 1, 0},
  {'f', "M .455 .7 L .545 .7 L .53 .34 L .47 .34 Z O .455 .17 .09 .09", .1, .1, .1, 1, 0})
G(ic_info, {'f', "O .04 .04 .92 .92", .2, .5, .95, 1, 0}, {'f', "R .44 .18 .12 .4 O .43 .66 .14 .14", 1, 1, 1, 1, 0})
G(ic_trash, {'f', "M .22 .78 L .28 .06 L .72 .06 L .78 .78 Z", .82, .83, .86, 1, 0},
  {'s', "M .22 .78 L .28 .06 L .72 .06 L .78 .78 Z M .16 .82 L .84 .82 M .4 .2 L .38 .7 M .6 .2 L .62 .7", .5, .5, .53, 1, .03})
G(ic_trash_full, {'f', "M .22 .78 L .28 .06 L .72 .06 L .78 .78 Z", .82, .83, .86, 1, 0},
  {'f', "M .24 .8 C .3 .98 .5 .9 .56 .98 C .64 .9 .74 .96 .76 .8 Z", 1, 1, 1, 1, 0},
  {'s', "M .22 .78 L .28 .06 L .72 .06 L .78 .78 Z M .16 .8 L .84 .8 M .4 .2 L .38 .7 M .6 .2 L .62 .7", .5, .5, .53, 1, .03})
#define GRAY_DISC {'f', "O .04 .04 .92 .92", .6, .62, .66, 1, 0}
G(ic_user, GRAY_DISC, {'f', "O .35 .48 .3 .3 M .17 .2 C .2 .5 .8 .5 .83 .2 C .65 .06 .35 .06 .17 .2 Z", 1, 1, 1, 1, 0})
G(ic_users, GRAY_DISC, {'f', "O .2 .46 .22 .22 M .08 .24 C .1 .46 .5 .46 .52 .24 Z O .52 .5 .26 .26 M .38 .22 C .42 .5 .9 .5 .9 .22 Z", 1, 1, 1, 1, 0})
G(ic_guest, GRAY_DISC, {'f', "O .35 .48 .3 .3 M .17 .2 C .2 .5 .8 .5 .83 .2 C .65 .06 .35 .06 .17 .2 Z", .85, .85, .87, 1, 0})
G(ic_avail, {'f', "O .1 .1 .8 .8", .2, .78, .3, 1, 0})
G(ic_partial, {'f', "O .1 .1 .8 .8", .98, .78, .1, 1, 0})
G(ic_unavail, {'f', "O .1 .1 .8 .8", .92, .25, .22, 1, 0})
G(ic_none, {'s', "O .14 .14 .72 .72", .55, .55, .58, 1, .1})
G(ic_gear, {'f', "O .04 .04 .92 .92", .55, .57, .62, 1, 0}, {'e', "O .24 .24 .52 .52 O .4 .4 .2 .2", 1, 1, 1, 1, 0},
  {'s', "M .5 .14 L .5 .86 M .14 .5 L .86 .5 M .25 .25 L .75 .75 M .75 .25 L .25 .75", 1, 1, 1, 1, .1}, {'f', "O .37 .37 .26 .26", .55, .57, .62, 1, 0})
G(ic_wheel, {'f', "M .5 .5 A .5 .5 .46 0 60 0 Z", .95, .2, .2, 1, 0}, {'f', "M .5 .5 A .5 .5 .46 60 120 0 Z", .98, .85, .1, 1, 0},
  {'f', "M .5 .5 A .5 .5 .46 120 180 0 Z", .3, .82, .25, 1, 0}, {'f', "M .5 .5 A .5 .5 .46 180 240 0 Z", .1, .78, .85, 1, 0},
  {'f', "M .5 .5 A .5 .5 .46 240 300 0 Z", .2, .35, .95, 1, 0}, {'f', "M .5 .5 A .5 .5 .46 300 360 0 Z", .8, .25, .85, 1, 0})
G(ic_font, {'f', "Q .06 .06 .88 .88 .14", 1, 1, 1, 1, 0}, {'s', "Q .06 .06 .88 .88 .14", .62, .62, .65, 1, .03}, {'t', "A", .15, .15, .17, 1, 0})
G(ic_cloud, {'f', "O .12 .26 .36 .36 O .32 .36 .4 .4 O .56 .26 .32 .32 R .3 .26 .44 .2", .35, .65, .95, 1, 0})

typedef struct { const char *name; const layer_t *g; int tmpl; double side; } named_t;
static const named_t g_named[] = {
    {"NSAddTemplate", gl_plus, 1, 18}, {"NSRemoveTemplate", gl_minus, 1, 18}, {"NSActionTemplate", gl_gear, 1, 18},
    {"NSRefreshTemplate", gl_refresh, 1, 18}, {"NSStopProgressTemplate", gl_x, 1, 18}, {"NSGoBackTemplate", gl_left, 1, 18},
    {"NSGoForwardTemplate", gl_right, 1, 18}, {"NSGoLeftTemplate", gl_left, 1, 18}, {"NSGoRightTemplate", gl_right, 1, 18},
    {"NSLeftFacingTriangleTemplate", gl_tri_l, 1, 10}, {"NSRightFacingTriangleTemplate", gl_tri_r, 1, 10},
    {"NSMenuOnStateTemplate", gl_check, 1, 12}, {"NSMenuMixedStateTemplate", gl_mixed, 1, 12},
    {"NSLockLockedTemplate", gl_lock, 1, 18}, {"NSLockUnlockedTemplate", gl_unlock, 1, 18}, {"NSShareTemplate", gl_share, 1, 18},
    {"NSQuickLookTemplate", gl_eye, 1, 18}, {"NSHomeTemplate", gl_house, 1, 18}, {"NSBookmarksTemplate", gl_book, 1, 18},
    {"NSIconViewTemplate", gl_grid, 1, 18}, {"NSListViewTemplate", gl_list, 1, 18}, {"NSColumnViewTemplate", gl_columns, 1, 18},
    {"NSFlowViewTemplate", gl_flow, 1, 18}, {"NSSlideshowTemplate", gl_slides, 1, 18}, {"NSPathTemplate", gl_path, 1, 18},
    {"NSEnterFullScreenTemplate", gl_full_in, 1, 18}, {"NSExitFullScreenTemplate", gl_full_out, 1, 18},
    {"NSFollowLinkFreestandingTemplate", gl_follow, 1, 14}, {"NSRevealFreestandingTemplate", gl_reveal, 1, 14},
    {"NSInvalidDataFreestandingTemplate", gl_invalid, 1, 14}, {"NSStopProgressFreestandingTemplate", gl_stop_free, 1, 14},
    {"NSRefreshFreestandingTemplate", gl_refresh_free, 1, 14}, {"NSSmartBadgeTemplate", gl_smart, 1, 14},
    {"NSBluetoothTemplate", gl_bt, 1, 18}, {"NSIChatTheaterTemplate", gl_video, 1, 18},
    {"NSTouchBarAddDetailTemplate", gl_add_detail, 1, 18}, {"NSTouchBarAddTemplate", gl_plus, 1, 18}, {"NSTouchBarAlarmTemplate", gl_alarm, 1, 18},
    {"NSTouchBarAudioInputMuteTemplate", gl_mic_mute, 1, 18}, {"NSTouchBarAudioInputTemplate", gl_mic, 1, 18},
    {"NSTouchBarAudioOutputMuteTemplate", gl_mute, 1, 18}, {"NSTouchBarAudioOutputVolumeHighTemplate", gl_vol3, 1, 18},
    {"NSTouchBarAudioOutputVolumeLowTemplate", gl_vol1, 1, 18}, {"NSTouchBarAudioOutputVolumeMediumTemplate", gl_vol2, 1, 18},
    {"NSTouchBarAudioOutputVolumeOffTemplate", gl_vol0, 1, 18}, {"NSTouchBarBookmarksTemplate", gl_book, 1, 18},
    {"NSTouchBarColorPickerFill", gl_bucket, 1, 18}, {"NSTouchBarColorPickerFont", gl_letter_a, 1, 18}, {"NSTouchBarColorPickerStroke", gl_pencil, 1, 18},
    {"NSTouchBarCommunicationAudioTemplate", gl_phone, 1, 18}, {"NSTouchBarCommunicationVideoTemplate", gl_video, 1, 18},
    {"NSTouchBarComposeTemplate", gl_compose, 1, 18}, {"NSTouchBarDeleteTemplate", gl_trash, 1, 18}, {"NSTouchBarDownloadTemplate", gl_download, 1, 18},
    {"NSTouchBarEnterFullScreenTemplate", gl_full_in, 1, 18}, {"NSTouchBarExitFullScreenTemplate", gl_full_out, 1, 18},
    {"NSTouchBarFastForwardTemplate", gl_ff, 1, 18}, {"NSTouchBarFolderCopyToTemplate", gl_folder_copy, 1, 18},
    {"NSTouchBarFolderMoveToTemplate", gl_folder_move, 1, 18}, {"NSTouchBarFolderTemplate", gl_folder, 1, 18},
    {"NSTouchBarGetInfoTemplate", gl_info_t, 1, 18}, {"NSTouchBarGoBackTemplate", gl_left, 1, 18}, {"NSTouchBarGoDownTemplate", gl_down, 1, 18},
    {"NSTouchBarGoForwardTemplate", gl_right, 1, 18}, {"NSTouchBarGoUpTemplate", gl_up, 1, 18}, {"NSTouchBarHistoryTemplate", gl_history, 1, 18},
    {"NSTouchBarIconViewTemplate", gl_grid, 1, 18}, {"NSTouchBarListViewTemplate", gl_list, 1, 18}, {"NSTouchBarMailTemplate", gl_mail, 1, 18},
    {"NSTouchBarNewFolderTemplate", gl_folder_new, 1, 18}, {"NSTouchBarNewMessageTemplate", gl_mail_new, 1, 18},
    {"NSTouchBarOpenInBrowserTemplate", gl_globe, 1, 18}, {"NSTouchBarPauseTemplate", gl_pause, 1, 18}, {"NSTouchBarPlayheadTemplate", gl_playhead, 1, 18},
    {"NSTouchBarPlayPauseTemplate", gl_playpause, 1, 18}, {"NSTouchBarPlayTemplate", gl_play, 1, 18}, {"NSTouchBarQuickLookTemplate", gl_eye, 1, 18},
    {"NSTouchBarRecordStartTemplate", gl_rec, 1, 18}, {"NSTouchBarRecordStopTemplate", gl_recstop, 1, 18}, {"NSTouchBarRefreshTemplate", gl_refresh, 1, 18},
    {"NSTouchBarRemoveTemplate", gl_minus, 1, 18}, {"NSTouchBarRewindTemplate", gl_rew, 1, 18}, {"NSTouchBarRotateLeftTemplate", gl_rot_l, 1, 18},
    {"NSTouchBarRotateRightTemplate", gl_rot_r, 1, 18}, {"NSTouchBarSearchTemplate", gl_search, 1, 18}, {"NSTouchBarShareTemplate", gl_share, 1, 18},
    {"NSTouchBarSidebarTemplate", gl_sidebar, 1, 18}, {"NSTouchBarSkipAheadTemplate", gl_skip_f, 1, 18}, {"NSTouchBarSkipBackTemplate", gl_skip_b, 1, 18},
    {"NSTouchBarSkipToEndTemplate", gl_toend, 1, 18}, {"NSTouchBarSkipToStartTemplate", gl_tostart, 1, 18}, {"NSTouchBarSlideshowTemplate", gl_slides, 1, 18},
    {"NSTouchBarTagIconTemplate", gl_tag, 1, 18}, {"NSTouchBarTextBoldTemplate", gl_bold, 1, 18}, {"NSTouchBarTextBoxTemplate", gl_textbox, 1, 18},
    {"NSTouchBarTextCenterAlignTemplate", gl_al_center, 1, 18}, {"NSTouchBarTextItalicTemplate", gl_italic, 1, 18},
    {"NSTouchBarTextJustifiedAlignTemplate", gl_al_just, 1, 18}, {"NSTouchBarTextLeftAlignTemplate", gl_al_left, 1, 18},
    {"NSTouchBarTextListTemplate", gl_textlist, 1, 18}, {"NSTouchBarTextRightAlignTemplate", gl_al_right, 1, 18},
    {"NSTouchBarTextStrikethroughTemplate", gl_strike, 1, 18}, {"NSTouchBarTextUnderlineTemplate", gl_under, 1, 18},
    {"NSTouchBarUserAddTemplate", gl_user_add, 1, 18}, {"NSTouchBarUserGroupTemplate", gl_users, 1, 18}, {"NSTouchBarUserTemplate", gl_user, 1, 18},
    {"NSTouchBarVolumeDownTemplate", gl_voldown, 1, 18}, {"NSTouchBarVolumeUpTemplate", gl_volup, 1, 18},
    {"NSFolder", ic_folder, 0, 32}, {"NSFolderSmart", ic_folder_smart, 0, 32}, {"NSFolderBurnable", ic_folder_burn, 0, 32},
    {"NSMultipleDocuments", ic_docs, 0, 32}, {"NSComputer", ic_computer, 0, 32}, {"NSNetwork", ic_network, 0, 32},
    {"NSCaution", ic_caution, 0, 32}, {"NSInfo", ic_info, 0, 32}, {"NSTrashEmpty", ic_trash, 0, 32}, {"NSTrashFull", ic_trash_full, 0, 32},
    {"NSUser", ic_user, 0, 32}, {"NSUserGroup", ic_users, 0, 32}, {"NSEveryone", ic_users, 0, 32}, {"NSUserGuest", ic_guest, 0, 32},
    {"NSUserAccounts", ic_users, 0, 32}, {"NSStatusAvailable", ic_avail, 0, 16}, {"NSStatusPartiallyAvailable", ic_partial, 0, 16},
    {"NSStatusUnavailable", ic_unavail, 0, 16}, {"NSStatusNone", ic_none, 0, 16}, {"NSAdvanced", ic_gear, 0, 32},
    {"NSPreferencesGeneral", ic_gear, 0, 32}, {"NSColorPanel", ic_wheel, 0, 32}, {"NSFontPanel", ic_font, 0, 32},
    {"NSBonjour", ic_network, 0, 32}, {"NSDotMac", ic_cloud, 0, 32}, {"NSMobileMe", ic_cloud, 0, 32},
    {"_mOSGenericDocument", ic_doc, 0, 32}, {"_mOSGenericApplication", ic_app, 0, 32},
};
/* desenha numa imagem nova; o lado em pixels acompanha a tela (2× para nitidez) e o tamanho fica em pontos */
static id render(const named_t *n) {
    double scale = 2, side = n->side;
    id img = ((id (*)(id, SEL, S))objc_msgSend)(MSG0(id, (id)kls("NSImage"), "alloc"), sl("initWithSize:"), (S){side * scale, side * scale});
    if (!img) return 0;
    MSG0(void, img, "lockFocus");
    g_s = side * scale;
    for (const layer_t *L = n->g; L->op; L++) paint(L);
    MSG0(void, img, "unlockFocus");
    ((void (*)(id, SEL, S))objc_msgSend)(img, sl("setSize:"), (S){side, side});
    MSG(void, img, "setTemplate:", (BOOL)(n->tmpl != 0));
    return img;
}
static id g_cache;
static id system_image(const char *name) {
    for (unsigned i = 0; i < sizeof g_named / sizeof g_named[0]; i++)
        if (!strcmp(g_named[i].name, name)) {
            if (!g_cache) g_cache = MSG0(id, MSG0(id, (id)kls("NSMutableDictionary"), "alloc"), "init");
            id key = str(name), img = MSG(id, g_cache, "objectForKey:", key);
            if (!img) { img = render(&g_named[i]); if (img) { MSG(void, g_cache, "setObject:forKey:", img, key); objc_release(img); } }
            return img;
        }
    return 0;
}

/* ================================================================ ícones de bundles e documentos */
static id load_icns(id path) {
    if (!path) return 0;
    id img = MSG(id, MSG0(id, (id)kls("NSImage"), "alloc"), "initWithContentsOfFile:", path);
    if (img) { ((void (*)(id, SEL, S))objc_msgSend)(img, sl("setSize:"), (S){32, 32}); objc_autorelease(img); }
    return img;
}
static id bundle_resource_icns(id bundle, id name) {
    if (!bundle || !name || !cx_is_str(name)) return 0;
    id base = MSG0(id, name, "stringByDeletingPathExtension");
    id p = MSG(id, bundle, "pathForResource:ofType:", base, str("icns"));
    if (!p) p = MSG(id, bundle, "pathForResource:ofType:", name, (id)0);
    return load_icns(p);
}
static id app_icon_at(id path) {
    id b = MSG(id, (id)kls("NSBundle"), "bundleWithPath:", path);
    id info = b ? MSG0(id, b, "infoDictionary") : 0;
    id file = info ? MSG(id, info, "objectForKey:", str("CFBundleIconFile")) : 0;
    id img = bundle_resource_icns(b, file);
    if (!img && info) img = bundle_resource_icns(b, MSG(id, info, "objectForKey:", str("CFBundleIconName")));
    return img ? img : system_image("_mOSGenericApplication");
}
static BOOL arr_has(id arr, id s, BOOL ci) {
    if (!arr || !s) return NO;
    i64 n = MSG0(i64, arr, "count");
    for (i64 i = 0; i < n; i++) {
        id v = MSG(id, arr, "objectAtIndex:", i);
        if (v && cx_is_str(v) && (ci ? MSG(i64, v, "caseInsensitiveCompare:", s) == 0 : MSG(BOOL, v, "isEqualToString:", s))) return YES;
    }
    return NO;
}
/* o ícone de documento que o app declara para a extensão ou o tipo */
static id doc_icon_from_app(id app_url, id ext, id uti) {
    if (!app_url) return 0;
    id b = MSG(id, (id)kls("NSBundle"), "bundleWithURL:", app_url);
    id types = b ? MSG(id, b, "objectForInfoDictionaryKey:", str("CFBundleDocumentTypes")) : 0;
    i64 n = types ? MSG0(i64, types, "count") : 0;
    for (i64 i = 0; i < n; i++) {
        id t = MSG(id, types, "objectAtIndex:", i);
        if (!(ext && arr_has(MSG(id, t, "objectForKey:", str("CFBundleTypeExtensions")), ext, YES)) &&
            !(uti && arr_has(MSG(id, t, "objectForKey:", str("LSItemContentTypes")), uti, NO)))
            continue;
        id img = bundle_resource_icns(b, MSG(id, t, "objectForKey:", str("CFBundleTypeIconFile")));
        if (img) return img;
    }
    return 0;
}
static id ws_icon_for_file(id self, SEL c, id path) {
    (void)c;
    if (!path || !cx_is_str(path)) return system_image("_mOSGenericDocument");
    id fm = MSG0(id, (id)kls("NSFileManager"), "defaultManager");
    BOOL dir = NO;
    if (!((BOOL (*)(id, SEL, id, BOOL *))objc_msgSend)(fm, sl("fileExistsAtPath:isDirectory:"), path, &dir)) return system_image("_mOSGenericDocument");
    id ext = MSG0(id, path, "pathExtension");
    if (dir) {
        if (ext && (MSG(BOOL, ext, "isEqualToString:", str("app")) || MSG(BOOL, ext, "isEqualToString:", str("appex")))) return app_icon_at(path);
        if (!strcmp(cx_cstr(path), "/")) return system_image("NSComputer");
        /* pasta com ícone próprio de bundle (ex.: .framework, .bundle com CFBundleIconFile) */
        id b = ext && MSG0(i64, ext, "length") ? MSG(id, (id)kls("NSBundle"), "bundleWithPath:", path) : 0;
        id f = b ? MSG(id, b, "objectForInfoDictionaryKey:", str("CFBundleIconFile")) : 0;
        id img = bundle_resource_icns(b, f);
        return img ? img : system_image("NSFolder");
    }
    if (ext && MSG(BOOL, ext, "isEqualToString:", str("icns"))) { id img = load_icns(path); if (img) return img; }
    id url = MSG(id, (id)kls("NSURL"), "fileURLWithPath:", path);
    id ws = self ? self : MSG0(id, (id)kls("NSWorkspace"), "sharedWorkspace");
    id uti = ((id (*)(id, SEL, id, id *))objc_msgSend)(ws, sl("typeOfFile:error:"), path, (id *)0);
    id img = doc_icon_from_app(MSG(id, ws, "URLForApplicationToOpenURL:", url), ext, uti);
    return img ? img : system_image("_mOSGenericDocument");
}
static id ws_icon_for_files(id self, SEL c, id paths) {
    i64 n = paths ? MSG0(i64, paths, "count") : 0;
    if (n == 1) return ws_icon_for_file(self, c, MSG(id, paths, "objectAtIndex:", (i64)0));
    return system_image("NSMultipleDocuments");
}
static id ws_icon_for_uti(id self, SEL c, id uti, id ext) {
    (void)c;
    id ws = self ? self : MSG0(id, (id)kls("NSWorkspace"), "sharedWorkspace");
    if (uti && (MSG(BOOL, uti, "isEqualToString:", str("public.folder")) || MSG(BOOL, uti, "isEqualToString:", str("public.directory"))))
        return system_image("NSFolder");
    if (uti && (MSG(BOOL, uti, "isEqualToString:", str("com.apple.application-bundle")) || MSG(BOOL, uti, "isEqualToString:", str("com.apple.application"))))
        return system_image("_mOSGenericApplication");
    if (!ext && uti) ext = MSG(id, ws, "preferredFilenameExtensionForType:", uti);
    id app = 0;
    if (uti) {
        id t = MSG(id, (id)kls("UTType"), "typeWithIdentifier:", uti);
        if (t) app = MSG(id, ws, "URLForApplicationToOpenContentType:", t);
    }
    id img = doc_icon_from_app(app, ext, uti);
    return img ? img : system_image("_mOSGenericDocument");
}
static id ws_icon_for_type(id self, SEL c, id type) {
    if (!type || !cx_is_str(type)) return system_image("_mOSGenericDocument");
    if (strchr(cx_cstr(type), '.')) return ws_icon_for_uti(self, c, type, 0);   /* já é UTI */
    const char *t = cx_cstr(type);
    if (!strcmp(t, "'fldr'") || !strcmp(t, "fold")) return system_image("NSFolder");
    extern id UTTypeCreatePreferredIdentifierForTag(id cls_, id tag, id conf);
    id uti = UTTypeCreatePreferredIdentifierForTag(str("public.filename-extension"), type, (id)0);
    return ws_icon_for_uti(self, c, uti, type);
}
static id ws_icon_for_content_type(id self, SEL c, id uttype) {
    id ident = uttype ? MSG0(id, uttype, "identifier") : 0;
    return ws_icon_for_uti(self, c, ident, uttype ? MSG0(id, uttype, "preferredFilenameExtension") : 0);
}

/* ================================================================ +imageNamed: (depois dos recursos do app) */
id cx_named_image_more(id cls, id name) {
    (void)cls;
    if (g_names) { id img = MSG(id, g_names, "objectForKey:", name); if (img) return img; }
    const char *n = cx_cstr(name);
    if (!strcmp(n, "NSApplicationIcon")) {
        id mb = MSG0(id, (id)kls("NSBundle"), "mainBundle");
        return app_icon_at(MSG0(id, mb, "bundlePath"));
    }
    return system_image(n);
}
static id bundle_image(id self, SEL c, id name) {
    (void)c;
    if (!name) return 0;
    static const char *types[] = {"png", "tiff", "tif", "jpg", "jpeg", "gif", "bmp", "icns", "pdf", 0};
    for (int i = 0; types[i]; i++) {
        id p = MSG(id, self, "pathForResource:ofType:", MSG0(id, name, "stringByDeletingPathExtension"),
                   MSG0(i64, MSG0(id, name, "pathExtension"), "length") ? MSG0(id, name, "pathExtension") : str(types[i]));
        if (p) {
            id img = MSG(id, MSG0(id, (id)kls("NSImage"), "alloc"), "initWithContentsOfFile:", p);
            if (img) return objc_autorelease(img);
        }
    }
    return 0;
}
static id bundle_image_path(id self, SEL c, id name) {
    (void)c;
    static const char *types[] = {"png", "tiff", "tif", "jpg", "jpeg", "gif", "bmp", "icns", "pdf", 0};
    for (int i = 0; name && types[i]; i++) {
        id p = MSG(id, self, "pathForResource:ofType:", name, str(types[i]));
        if (p) return p;
    }
    return 0;
}

void cx_appkit_icons_init(void) {
    cx_class_t *im = kls("NSImage"), *ws = kls("NSWorkspace"), *b = kls("NSBundle");
    if (im) {
        cx_add_method(im, "initWithContentsOfURL:", (void *)im_init_url);
        cx_add_method(im, "initByReferencingURL:", (void *)im_init_url);
        cx_add_method(im, "initByReferencingFile:", (void *)im_init_ref_file);
        cx_add_method(im, "setName:", (void *)im_set_name);
        cx_add_method(im, "name", (void *)im_name);
    }
    if (ws) {
        cx_add_method(ws, "iconForFile:", (void *)ws_icon_for_file);
        cx_add_method(ws, "iconForFiles:", (void *)ws_icon_for_files);
        cx_add_method(ws, "iconForFileType:", (void *)ws_icon_for_type);
        cx_add_method(ws, "iconForContentType:", (void *)ws_icon_for_content_type);
    }
    if (b) {
        cx_add_method(b, "imageForResource:", (void *)bundle_image);
        cx_add_method(b, "pathForImageResource:", (void *)bundle_image_path);
    }
}
