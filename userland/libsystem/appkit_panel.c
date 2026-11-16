/*
 * mOS libSystem — NSOpenPanel / NSSavePanel de verdade (antes: sempre "cancelado").
 *
 * O painel é uma janela do window server com barra lateral (Início, Mesa, Documentos, Downloads, Aplicativos,
 * Volumes, Disco), caminho com "pasta acima", lista com colunas Nome / Modificado / Tamanho (clique no cabeçalho
 * ordena), campo "Salvar como" no salvar, "Nova pasta", accessoryView e os botões. As decisões (filtro de tipos,
 * pacotes, ocultos, seleção, digitar para achar, nome e extensão) ficam no núcleo testado no host
 * (appkit_panel_core.c, userland/tests-appkit/panel_host.c); este arquivo lê a pasta, desenha e fala com o app.
 *
 * Teclas: ↑ ↓ (⇧ estende), Home/End, PgUp/PgDn, ⌘↑ pasta acima, ⌘↓ / Enter abre, Esc cancela, ⌘⇧G ou "/" "~"
 * ir para a pasta, ⌘⇧H Início, ⌘⇧D Mesa, ⌘⇧N nova pasta, ⌘⇧. ocultos, letras = digitar para achar.
 * Delegate: panel:shouldEnableURL:, panel:validateURL:error:, panel:didChangeToDirectoryURL:,
 * panelSelectionDidChange:, panel:userEnteredFilename:confirmed:.
 * Registro: cx_appkit_panel_init(), chamado do cx_appkit_input_init depois do appkit_extra (o último registro vence).
 */
#include "foundation.h"
#include "sys.h"
#include "libc.h"
#include "appkit_panel_core.h"

id objc_msgSend_stret(id self, SEL op, ...);
void free(void *p);
char *getenv(const char *name);
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void *_Block_copy(const void *b);
void _Block_release(const void *b);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
void *dispatch_get_main_queue(void);
extern id NSApp;
id NSHomeDirectory(void);
id cx_color(double r, double g, double b, double a);

typedef struct { double x, y; } P;
typedef struct { double w, h; } S;
typedef struct { double x, y, w, h; } R;
void NSRectFill(R r);
void NSFrameRect(R r);

/* stat do macOS ($INODE64), igual ao de libc.c */
typedef struct {
    u32 st_dev; u16 st_mode, st_nlink; u64 st_ino; u32 st_uid, st_gid, st_rdev;
    i64 atime, atime_ns, mtime, mtime_ns, ctime, ctime_ns, btime, btime_ns;
    i64 st_size, st_blocks; i32 st_blksize; u32 st_flags, st_gen; i32 lspare; i64 qspare[2];
} pst_t;
/* stat(const char *, void *) vem do foundation.h */

#define K(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
K(_MOSFilePanelList) K(_MOSFilePanelSide) K(_MOSFilePanelTarget)
static cx_class_t *kls(const char *n) { return cx_class_named(n); }
static SEL sl(const char *n) { return sel_registerName(n); }

/* ---------------------------------------------------------------- chamadas tipadas (structs por valor) */
static id m_init_frame(id k, R r) { return ((id (*)(id, SEL, R))objc_msgSend)(MSG0(id, k, "alloc"), sl("initWithFrame:"), r); }
static void m_set_frame(id v, R r) { ((void (*)(id, SEL, R))objc_msgSend)(v, sl("setFrame:"), r); }
static void m_draw_at(id s, P p, id attrs) { ((void (*)(id, SEL, P, id))objc_msgSend)(s, sl("drawAtPoint:withAttributes:"), p, attrs); }
static S m_size(id s, id attrs) { return ((S (*)(id, SEL, id))objc_msgSend)(s, sl("sizeWithAttributes:"), attrs); }
static P m_loc(id view, id ev) {
    P w = ((P (*)(id, SEL))objc_msgSend)(ev, sl("locationInWindow"));
    return ((P (*)(id, SEL, P, id))objc_msgSend)(view, sl("convertPoint:fromView:"), w, (id)0);
}
static void m_bool(id o, const char *s, BOOL b) { ((void (*)(id, SEL, BOOL))objc_msgSend)(o, sl(s), b); }
static void m_long(id o, const char *s, i64 v) { ((void (*)(id, SEL, i64))objc_msgSend)(o, sl(s), v); }
static BOOL responds(id o, const char *s) { return o && cx_responds(o->isa, sl(s)); }

/* ---------------------------------------------------------------- estado por painel */
enum { SIDE_MAX = 8 };
typedef struct fpanel {
    struct fpanel *next;
    id obj;
    fp_model_t m;
    BOOL inited;
    /* propriedades (retidas) */
    id title, message, prompt, name_value, name_label, dir_url, types, ctypes, accessory, delegate, identifier, tag_names;
    BOOL can_create, ext_hidden, can_hide_ext, resolves_aliases, shows_tags, accessory_disclosed, can_download_ubiq;
    /* resultado */
    id url, urls;
    /* janela */
    id win, list, side, path, name_field, ok_btn, cancel_btn, new_btn, msg_label;
    double list_w, list_h, side_h, scroll;
    char side_path[SIDE_MAX][FP_PATH_MAX];
    char side_name[SIDE_MAX][32];
    int nside;
    BOOL running;
} fpanel_t;

static fpanel_t *g_panels, *g_active;
static char g_last_dir[FP_PATH_MAX];

static void keep(id *slot, id v) { objc_retain(v); id o = *slot; *slot = v; objc_release(o); }

static void home_dir(char *out) {
    const char *h = getenv("HOME");
    if (!h || !*h) { id s = NSHomeDirectory(); h = s ? cx_cstr(s) : "/"; }
    fp_normalize(h, "/", "/", out);
}

static fpanel_t *fpanel(id o) {
    for (fpanel_t *p = g_panels; p; p = p->next) if (p->obj == o) return p;
    fpanel_t *p = calloc(1, sizeof *p);
    if (!p) return 0;
    p->obj = o;
    char h[FP_PATH_MAX];
    home_dir(h);
    int open = cx_is_subclass(object_getClass(o), kls("NSOpenPanel"));
    fp_init(&p->m, open, h);
    if (g_last_dir[0]) strcpy(p->m.dir, g_last_dir);
    p->can_create = YES;
    p->resolves_aliases = YES;
    p->can_hide_ext = YES;
    p->next = g_panels;
    g_panels = p;
    return p;
}

/* ---------------------------------------------------------------- texto */
static id g_font, g_font_bold, g_font_small;
static id attrs_for(id font, id color) {
    id d = MSG0(id, (id)kls("NSMutableDictionary"), "dictionary");
    extern id NSFontAttributeName, NSForegroundColorAttributeName;
    MSG(void, d, "setObject:forKey:", font, NSFontAttributeName);
    MSG(void, d, "setObject:forKey:", color, NSForegroundColorAttributeName);
    return d;
}
static void fonts(void) {
    if (g_font) return;
    g_font = objc_retain(MSG(id, (id)kls("NSFont"), "systemFontOfSize:", 13.0));
    g_font_bold = objc_retain(MSG(id, (id)kls("NSFont"), "boldSystemFontOfSize:", 11.0));
    g_font_small = objc_retain(MSG(id, (id)kls("NSFont"), "systemFontOfSize:", 11.0));
}
/* encurta com reticências até caber em w pontos (corta em fronteira de UTF-8) */
static id fit(const char *s, double w, id attrs) {
    id full = cx_str(s);
    if (m_size(full, attrs).w <= w) return full;
    size_t n = strlen(s);
    char buf[FP_NAME_MAX + 8];
    if (n > FP_NAME_MAX) n = FP_NAME_MAX;
    while (n > 0) {
        n--;
        while (n > 0 && ((unsigned char)s[n] & 0xC0) == 0x80) n--;
        memcpy(buf, s, n);
        memcpy(buf + n, "\xE2\x80\xA6", 4);
        id t = cx_str(buf);
        if (m_size(t, attrs).w <= w) return t;
    }
    return cx_str("\xE2\x80\xA6");
}
static void fmt_date(long long t, char *out) {
    long long days = t >= 0 ? t / 86400 : (t - 86399) / 86400, sec = t - days * 86400;
    long long z = days + 719468, era = (z >= 0 ? z : z - 146096) / 146097;
    unsigned doe = (unsigned)(z - era * 146097), yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    long long y = (long long)yoe + era * 400;
    unsigned doy = doe - (365 * yoe + yoe / 4 - yoe / 100), mp = (5 * doy + 2) / 153, d = doy - (153 * mp + 2) / 5 + 1;
    unsigned mo = mp < 10 ? mp + 3 : mp - 9;
    if (mo <= 2) y++;
    int hh = (int)(sec / 3600), mi = (int)(sec / 60 % 60);
    char *o = out;
    o[0] = (char)('0' + d / 10); o[1] = (char)('0' + d % 10); o[2] = '/';
    o[3] = (char)('0' + mo / 10); o[4] = (char)('0' + mo % 10); o[5] = '/';
    long long yy = y < 0 ? 0 : y % 10000;
    o[6] = (char)('0' + yy / 1000); o[7] = (char)('0' + yy / 100 % 10); o[8] = (char)('0' + yy / 10 % 10); o[9] = (char)('0' + yy % 10);
    o[10] = ' ';
    o[11] = (char)('0' + hh / 10); o[12] = (char)('0' + hh % 10); o[13] = ':'; o[14] = (char)('0' + mi / 10); o[15] = (char)('0' + mi % 10);
    o[16] = 0;
}
/* tamanho como o Finder: base 1000, uma casa decimal abaixo de 10 */
static void fmt_size(long long b, char *out) {
    static const char *u[] = {"bytes", "KB", "MB", "GB", "TB"};
    if (b < 1000) { char t[24]; int n = 0; long long v = b; char r[24]; int k = 0; do { r[k++] = (char)('0' + v % 10); v /= 10; } while (v); while (k) t[n++] = r[--k]; t[n] = 0; strcpy(out, t); strcpy(out + n, b == 1 ? " byte" : " bytes"); return; }
    double v = (double)b;
    int i = 0;
    while (v >= 999.95 && i < 4) { v /= 1000; i++; }
    int tenths = (int)(v * 10 + 0.5), n = 0;
    char t[32];
    int whole = tenths / 10;
    char r[16]; int k = 0;
    do { r[k++] = (char)('0' + whole % 10); whole /= 10; } while (whole);
    while (k) t[n++] = r[--k];
    if (v < 10) { t[n++] = ','; t[n++] = (char)('0' + tenths % 10); }
    t[n++] = ' ';
    t[n] = 0;
    strcpy(out, t);
    strcpy(out + n, u[i]);
}

/* ---------------------------------------------------------------- disco */
static int is_dir_path(const char *p) { pst_t st; return stat(p, &st) == 0 && (st.st_mode & 0170000) == 0040000; }

static id url_for(const char *path, int dir) {
    return MSG(id, (id)kls("NSURL"), "fileURLWithPath:isDirectory:", cx_str(path), (BOOL)(dir ? YES : NO));
}

static void notify_selection(fpanel_t *p);
static void update_buttons(fpanel_t *p);

/* lê a pasta para o modelo; o delegate pode vetar entradas */
static void load_dir(fpanel_t *p, const char *dir) {
    char t[FP_PATH_MAX];
    if (!fp_normalize(dir, p->m.dir, p->m.home, t) || !is_dir_path(t)) return;
    fp_begin_dir(&p->m, t);
    id fm = MSG0(id, (id)kls("NSFileManager"), "defaultManager");
    id names = MSG(id, fm, "contentsOfDirectoryAtPath:error:", cx_str(t), (id)0);
    u64 n = names ? MSG0(u64, names, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id nm = MSG(id, names, "objectAtIndex:", i);
        const char *s = nm ? cx_cstr(nm) : 0;
        if (!s) continue;
        char full[FP_PATH_MAX];
        if (!fp_join(t, s, full)) continue;
        pst_t st;
        memset(&st, 0, sizeof st);
        int ok = stat(full, &st) == 0;
        fp_add_entry(&p->m, s, ok && (st.st_mode & 0170000) == 0040000, ok ? st.mtime : 0, ok ? st.st_size : 0);
    }
    if (responds(p->delegate, "panel:shouldEnableURL:")) {
        for (int i = 0; i < p->m.n; i++) {
            char full[FP_PATH_MAX];
            if (!fp_join(t, p->m.e[i].name, full)) continue;
            BOOL en = ((BOOL (*)(id, SEL, id, id))objc_msgSend)(p->delegate, sl("panel:shouldEnableURL:"), p->obj, url_for(full, p->m.e[i].is_dir));
            p->m.e[i].vetoed = !en;
        }
    }
    fp_end_dir(&p->m);
    p->scroll = 0;
    strcpy(g_last_dir, t);
    if (p->path) MSG(void, p->path, "setStringValue:", cx_str(t));
    if (p->list) m_bool(p->list, "setNeedsDisplay:", YES);
    if (p->side) m_bool(p->side, "setNeedsDisplay:", YES);
    if (p->running && responds(p->delegate, "panel:didChangeToDirectoryURL:"))
        ((void (*)(id, SEL, id, id))objc_msgSend)(p->delegate, sl("panel:didChangeToDirectoryURL:"), p->obj, url_for(t, 1));
    notify_selection(p);
}

/* tipos do app: allowedContentTypes (UTType: identificador + extensão preferida) ou allowedFileTypes (strings) */
static void apply_types(fpanel_t *p) {
    const char *list[128];
    id keepalive[128];
    int n = 0;
    id arr = p->ctypes && MSG0(u64, p->ctypes, "count") ? p->ctypes : p->types;
    u64 c = arr ? MSG0(u64, arr, "count") : 0;
    for (u64 i = 0; i < c && n < 126; i++) {
        id t = MSG(id, arr, "objectAtIndex:", i);
        if (!t) continue;
        if (cx_is_str(t)) { keepalive[n] = t; list[n++] = cx_cstr(t); continue; }
        if (responds(t, "identifier")) { id s = MSG0(id, t, "identifier"); if (s) { keepalive[n] = s; list[n++] = cx_cstr(s); } }
        if (responds(t, "preferredFilenameExtension")) { id s = MSG0(id, t, "preferredFilenameExtension"); if (s && cx_is_str(s)) { keepalive[n] = s; list[n++] = cx_cstr(s); } }
    }
    (void)keepalive;
    fp_set_types(&p->m, list, n);
}

/* ---------------------------------------------------------------- resultado */
static void collect_open(fpanel_t *p) {
    id a = cx_arr_new(kls("NSMutableArray"));
    int n = fp_selected_count(&p->m);
    for (int k = 0; k < n; k++) {
        fp_entry_t *e = fp_row_entry(&p->m, fp_selected_row(&p->m, k));
        char full[FP_PATH_MAX];
        if (e && fp_join(p->m.dir, e->name, full)) cx_arr_add(a, url_for(full, e->is_dir));
    }
    if (!n && p->m.can_dirs) cx_arr_add(a, url_for(p->m.dir, 1));
    keep(&p->urls, a);
    objc_release(a);
    keep(&p->url, MSG0(u64, p->urls, "count") ? MSG(id, p->urls, "objectAtIndex:", (u64)0) : 0);
}

static int alert(const char *msg, const char *info, const char *b1, const char *b2) {
    id al = MSG0(id, MSG0(id, (id)kls("NSAlert"), "alloc"), "init");
    MSG(void, al, "setMessageText:", cx_str(msg));
    if (info) MSG(void, al, "setInformativeText:", cx_str(info));
    MSG(id, al, "addButtonWithTitle:", cx_str(b1));
    if (b2) MSG(id, al, "addButtonWithTitle:", cx_str(b2));
    i64 r = MSG0(i64, al, "runModal");
    objc_release(al);
    return (int)(r - 1000);
}

static BOOL validate_url(fpanel_t *p, id url) {
    if (!responds(p->delegate, "panel:validateURL:error:")) return YES;
    id err = 0;
    BOOL ok = ((BOOL (*)(id, SEL, id, id, id *))objc_msgSend)(p->delegate, sl("panel:validateURL:error:"), p->obj, url, &err);
    if (!ok && err && responds(err, "localizedDescription")) {
        id d = MSG0(id, err, "localizedDescription");
        if (d) alert(cx_cstr(d), 0, "OK", 0);
    }
    return ok;
}

static void finish(fpanel_t *p, i64 code) { (void)p; m_long(NSApp, "stopModalWithCode:", code); }

/* botão principal (Abrir/Salvar) ou Enter */
static void confirm(fpanel_t *p) {
    if (!p->m.open_panel) {
        id nf = p->name_field ? MSG0(id, p->name_field, "stringValue") : p->name_value;
        const char *nm = nf ? cx_cstr(nf) : "";
        if (nm[0] == '/' || nm[0] == '~') {    /* caminho digitado: vai para a pasta e fica com o nome */
            char t[FP_PATH_MAX], par[FP_PATH_MAX];
            if (fp_normalize(nm, p->m.dir, p->m.home, t)) {
                if (is_dir_path(t)) { load_dir(p, t); if (p->name_field) MSG(void, p->name_field, "setStringValue:", cx_str("")); return; }
                if (fp_parent(t, par) && is_dir_path(par)) { load_dir(p, par); nm = fp_basename(t); MSG(void, p->name_field, "setStringValue:", cx_str(nm)); nf = MSG0(id, p->name_field, "stringValue"); nm = cx_cstr(nf); }
            }
        }
        switch (fp_validate_name(nm)) {
        case FP_NAME_OK: break;
        case FP_NAME_EMPTY: return;
        case FP_NAME_SLASH: case FP_NAME_COLON: alert("O nome não pode ter \"/\" nem \":\".", "Escolha outro nome.", "OK", 0); return;
        case FP_NAME_DOT: alert("Nomes que começam com \".\" ficam ocultos.", "Escolha um nome que não comece com ponto.", "OK", 0); return;
        case FP_NAME_LONG: alert("O nome é longo demais.", "Use até 255 bytes.", "OK", 0); return;
        default: alert("Esse nome é reservado pelo sistema.", "Escolha outro nome.", "OK", 0); return;
        }
        char fin[FP_NAME_MAX + 40];
        if (!fp_save_name(&p->m, nm, fin, sizeof fin)) return;
        if (responds(p->delegate, "panel:userEnteredFilename:confirmed:")) {
            id r = ((id (*)(id, SEL, id, id, BOOL))objc_msgSend)(p->delegate, sl("panel:userEnteredFilename:confirmed:"), p->obj, cx_str(fin), YES);
            if (!r) return;
            const char *rs = cx_cstr(r);
            if (strlen(rs) < sizeof fin) strcpy(fin, rs);
        }
        int ex = fp_find(&p->m, fin);
        if (ex >= 0) {
            if (p->m.e[ex].is_dir && !p->m.e[ex].is_pkg) { alert("Já existe uma pasta com esse nome.", "Escolha outro nome.", "OK", 0); return; }
            char q[FP_NAME_MAX + 64];
            size_t l = strlen(p->m.e[ex].name);
            memcpy(q, "\xE2\x80\x9C", 3); memcpy(q + 3, p->m.e[ex].name, l); strcpy(q + 3 + l, "\xE2\x80\x9D j\xC3\xA1 existe. Deseja substitu\xC3\xAD-lo?");
            if (alert(q, "Substituir apaga o conteúdo atual.", "Cancelar", "Substituir") != 1) return;
        }
        char full[FP_PATH_MAX];
        if (!fp_join(p->m.dir, fin, full)) return;
        id u = url_for(full, 0);
        if (!validate_url(p, u)) return;
        keep(&p->name_value, cx_str(fin));
        keep(&p->url, u);
        id a = cx_arr_new(kls("NSMutableArray")); cx_arr_add(a, u); keep(&p->urls, a); objc_release(a);
        finish(p, 1);
        return;
    }
    /* abrir: uma pasta selecionada sem canChooseDirectories entra nela */
    if (fp_selected_count(&p->m) == 1) {
        int r = fp_selected_row(&p->m, 0);
        fp_entry_t *e = fp_row_entry(&p->m, r);
        if (fp_activate(&p->m, r) == FP_ACT_ENTER_DIR && !p->m.can_dirs) {
            char full[FP_PATH_MAX];
            if (fp_join(p->m.dir, e->name, full)) load_dir(p, full);
            return;
        }
    }
    if (!fp_can_confirm(&p->m, 0)) return;
    collect_open(p);
    u64 n = MSG0(u64, p->urls, "count");
    for (u64 i = 0; i < n; i++) if (!validate_url(p, MSG(id, p->urls, "objectAtIndex:", i))) return;
    finish(p, 1);
}

static void notify_selection(fpanel_t *p) {
    if (p->m.open_panel) collect_open(p);
    update_buttons(p);
    if (p->running && responds(p->delegate, "panelSelectionDidChange:")) MSG(void, p->delegate, "panelSelectionDidChange:", p->obj);
}

static void update_buttons(fpanel_t *p) {
    if (!p->ok_btn) return;
    id nf = p->name_field ? MSG0(id, p->name_field, "stringValue") : 0;
    BOOL en = (BOOL)fp_can_confirm(&p->m, nf ? cx_cstr(nf) : 0);
    m_bool(p->ok_btn, "setEnabled:", en);
}

/* ---------------------------------------------------------------- pequeno diálogo de texto (ir para a pasta, nova pasta) */
static id g_prompt_field;
static void tg_prompt_ok(id self, SEL c, id s) { (void)self; (void)c; (void)s; m_long(NSApp, "stopModalWithCode:", 1); }
static void tg_prompt_cancel(id self, SEL c, id s) { (void)self; (void)c; (void)s; m_long(NSApp, "stopModalWithCode:", 0); }
static id target_obj(void);
static int ask_text(const char *label, const char *initial, const char *okt, char *out) {
    id w = ((id (*)(id, SEL, R, u64, u64, BOOL))objc_msgSend)(MSG0(id, (id)kls("NSPanel"), "alloc"),
        sl("initWithContentRect:styleMask:backing:defer:"), (R){0, 0, 380, 112}, (u64)1, (u64)2, NO);
    MSG(void, w, "setTitle:", cx_str(""));
    id cv = MSG0(id, w, "contentView");
    id lb = MSG(id, (id)kls("NSTextField"), "labelWithString:", cx_str(label));
    m_set_frame(lb, (R){20, 78, 340, 18});
    MSG(void, cv, "addSubview:", lb);
    id tf = MSG(id, (id)kls("NSTextField"), "textFieldWithString:", cx_str(initial));
    m_set_frame(tf, (R){20, 48, 340, 24});
    MSG(void, cv, "addSubview:", tf);
    id ok = MSG(id, (id)kls("NSButton"), "buttonWithTitle:target:action:", cx_str(okt), target_obj(), sl("cxPromptOK:"));
    m_set_frame(ok, (R){272, 12, 88, 24});
    MSG(void, ok, "setKeyEquivalent:", cx_str("\r"));
    MSG(void, cv, "addSubview:", ok);
    id ca = MSG(id, (id)kls("NSButton"), "buttonWithTitle:target:action:", cx_str("Cancelar"), target_obj(), sl("cxPromptCancel:"));
    m_set_frame(ca, (R){176, 12, 88, 24});
    MSG(void, cv, "addSubview:", ca);
    MSG0(void, w, "center");
    MSG(void, w, "makeFirstResponder:", tf);
    g_prompt_field = tf;
    i64 r = MSG(i64, NSApp, "runModalForWindow:", w);
    id s = MSG0(id, tf, "stringValue");
    const char *cs = s ? cx_cstr(s) : "";
    size_t l = strlen(cs);
    if (l >= FP_PATH_MAX) l = FP_PATH_MAX - 1;
    memcpy(out, cs, l); out[l] = 0;
    MSG(void, w, "orderOut:", (id)0);
    MSG0(void, w, "close");
    g_prompt_field = 0;
    return r == 1 && l > 0;
}

static void go_to_folder(fpanel_t *p, const char *initial) {
    char in[FP_PATH_MAX], t[FP_PATH_MAX];
    if (!ask_text("Ir para a pasta:", initial, "Ir", in)) return;
    if (!fp_normalize(in, p->m.dir, p->m.home, t)) return;
    if (is_dir_path(t)) { load_dir(p, t); return; }
    char par[FP_PATH_MAX];
    if (fp_parent(t, par) && is_dir_path(par)) {   /* um arquivo: abre a pasta dele e seleciona */
        load_dir(p, par);
        int r = fp_select_name(&p->m, fp_basename(t));
        if (r >= 0) notify_selection(p);
        return;
    }
    alert("A pasta não existe.", in, "OK", 0);
}

static void new_folder(fpanel_t *p) {
    char base[FP_NAME_MAX], in[FP_PATH_MAX], full[FP_PATH_MAX];
    fp_new_folder_name(&p->m, "pasta sem título", base, sizeof base);
    if (!ask_text("Nome da nova pasta:", base, "Criar", in)) return;
    if (fp_validate_name(in) != FP_NAME_OK) { alert("Esse nome não pode ser usado.", "Não use \"/\", \":\" nem ponto no começo.", "OK", 0); return; }
    if (fp_find(&p->m, in) >= 0) { alert("Já existe um item com esse nome.", in, "OK", 0); return; }
    if (!fp_join(p->m.dir, in, full)) return;
    id fm = MSG0(id, (id)kls("NSFileManager"), "defaultManager");
    BOOL ok = MSG(BOOL, fm, "createDirectoryAtPath:withIntermediateDirectories:attributes:error:", cx_str(full), (BOOL)NO, (id)0, (id)0);
    if (!ok) { alert("Não foi possível criar a pasta.", full, "OK", 0); return; }
    char cur[FP_PATH_MAX];
    strcpy(cur, p->m.dir);
    load_dir(p, cur);
    if (fp_select_name(&p->m, in) >= 0) notify_selection(p);
}

/* ---------------------------------------------------------------- lista */
#define ROW_H 20.0
#define HEAD_H 22.0
static double col_date_x(fpanel_t *p) { return p->list_w - 250; }
static double col_size_x(fpanel_t *p) { return p->list_w - 90; }
static int page_rows(fpanel_t *p) { int n = (int)((p->list_h - HEAD_H) / ROW_H); return n < 1 ? 1 : n; }

static void reveal(fpanel_t *p, int row) {
    if (row < 0) return;
    double top = row * ROW_H, vis = p->list_h - HEAD_H;
    if (top < p->scroll) p->scroll = top;
    else if (top + ROW_H > p->scroll + vis) p->scroll = top + ROW_H - vis;
    if (p->scroll < 0) p->scroll = 0;
}

static void list_draw(id self, SEL c, R dirty) {
    (void)c; (void)dirty;
    fpanel_t *p = g_active;
    if (!p) return;
    fonts();
    double W = p->list_w, H = p->list_h;
    MSG0(void, cx_color(1, 1, 1, 1), "setFill");
    NSRectFill((R){0, 0, W, H});
    id a_norm = attrs_for(g_font, cx_color(0.1, 0.1, 0.1, 1)), a_sel = attrs_for(g_font, cx_color(1, 1, 1, 1));
    id a_off = attrs_for(g_font, cx_color(0.1, 0.1, 0.1, 0.35)), a_dim = attrs_for(g_font_small, cx_color(0.35, 0.35, 0.38, 1));
    id a_dim_sel = attrs_for(g_font_small, cx_color(1, 1, 1, 0.9));
    int first = (int)(p->scroll / ROW_H), last = (int)((p->scroll + H - HEAD_H) / ROW_H) + 1;
    if (last > p->m.nrows) last = p->m.nrows;
    for (int r = first; r < last; r++) {
        double y = HEAD_H + r * ROW_H - p->scroll;
        fp_entry_t *e = fp_row_entry(&p->m, r);
        BOOL sel = p->m.sel && p->m.sel[r];
        if (sel) { MSG0(void, cx_color(0.0, 0.40, 0.86, 1), "setFill"); NSRectFill((R){0, y, W, ROW_H}); }
        else if (r & 1) { MSG0(void, cx_color(0.96, 0.96, 0.97, 1), "setFill"); NSRectFill((R){0, y, W, ROW_H}); }
        BOOL nav = e->is_dir && (!e->is_pkg || p->m.pkgs_as_dirs);
        /* marcador: pasta (aba + corpo azul) ou folha de documento */
        double ix = 8, iy = y + 4;
        if (nav) {
            MSG0(void, sel ? cx_color(1, 1, 1, 0.95) : cx_color(0.33, 0.63, 0.95, e->enabled ? 1 : 0.4), "setFill");
            NSRectFill((R){ix, iy + 1, 6, 2});
            NSRectFill((R){ix, iy + 3, 15, 9});
        } else {
            MSG0(void, sel ? cx_color(1, 1, 1, 0.95) : cx_color(1, 1, 1, 1), "setFill");
            NSRectFill((R){ix + 2, iy, 11, 13});
            MSG0(void, sel ? cx_color(1, 1, 1, 0.95) : cx_color(0.55, 0.55, 0.58, e->enabled ? 1 : 0.4), "setFill");
            NSFrameRect((R){ix + 2, iy, 11, 13});
            if (e->is_pkg) NSRectFill((R){ix + 5, iy + 4, 5, 5});
        }
        id an = sel ? a_sel : e->enabled ? a_norm : a_off;
        m_draw_at(fit(e->name, col_date_x(p) - 38, an), (P){30, y + 2}, an);
        char buf[40];
        id ad = sel ? a_dim_sel : a_dim;
        if (e->mtime > 0) { fmt_date(e->mtime, buf); m_draw_at(cx_str(buf), (P){col_date_x(p) + 6, y + 4}, ad); }
        if (nav) strcpy(buf, "--"); else fmt_size(e->size, buf);
        m_draw_at(cx_str(buf), (P){col_size_x(p) + 6, y + 4}, ad);
    }
    if (!p->m.nrows) {
        id a = attrs_for(g_font, cx_color(0.5, 0.5, 0.52, 1));
        m_draw_at(cx_str("Pasta vazia"), (P){W / 2 - 36, HEAD_H + 24}, a);
    }
    /* cabeçalho por cima das linhas */
    MSG0(void, cx_color(0.95, 0.95, 0.96, 1), "setFill");
    NSRectFill((R){0, 0, W, HEAD_H});
    MSG0(void, cx_color(0, 0, 0, 0.12), "setFill");
    NSRectFill((R){0, HEAD_H - 1, W, 1});
    NSRectFill((R){col_date_x(p), 3, 1, HEAD_H - 6});
    NSRectFill((R){col_size_x(p), 3, 1, HEAD_H - 6});
    static const char *heads[] = {"Nome", "Modificado", "Tamanho"};
    static const int keys[] = {FP_SORT_NAME, FP_SORT_DATE, FP_SORT_SIZE};
    double hx[3] = {30, col_date_x(p) + 6, col_size_x(p) + 6};
    id ah = attrs_for(g_font_bold, cx_color(0.25, 0.25, 0.28, 1));
    for (int i = 0; i < 3; i++) {
        char t[32];
        strcpy(t, heads[i]);
        if (p->m.sort_key == keys[i]) strcat(t, p->m.sort_desc ? " \xE2\x96\xBE" : " \xE2\x96\xB4");
        m_draw_at(cx_str(t), (P){hx[i], 4}, ah);
    }
    /* borda */
    MSG0(void, cx_color(0, 0, 0, 0.15), "setFill");
    NSFrameRect((R){0, 0, W, H});
}

static BOOL yes_imp(id self, SEL c) { (void)self; (void)c; return YES; }

static void after_change(fpanel_t *p) {
    reveal(p, p->m.cursor);
    m_bool(p->list, "setNeedsDisplay:", YES);
    notify_selection(p);
}

static void enter_row(fpanel_t *p, int r) {
    fp_entry_t *e = fp_row_entry(&p->m, r);
    int act = fp_activate(&p->m, r);
    if (act == FP_ACT_ENTER_DIR) {
        char full[FP_PATH_MAX];
        if (fp_join(p->m.dir, e->name, full)) load_dir(p, full);
    } else if (act == FP_ACT_CONFIRM) confirm(p);
}

static void list_mouse_down(id self, SEL c, id ev) {
    (void)c;
    fpanel_t *p = g_active;
    if (!p) return;
    if (p->win) MSG(void, p->win, "makeFirstResponder:", self);
    P pt = m_loc(self, ev);
    if (pt.y < HEAD_H) {
        int key = pt.x >= col_size_x(p) ? FP_SORT_SIZE : pt.x >= col_date_x(p) ? FP_SORT_DATE : FP_SORT_NAME;
        fp_set_sort(&p->m, key, p->m.sort_key == key ? !p->m.sort_desc : key != FP_SORT_NAME);
        after_change(p);
        return;
    }
    int r = (int)((pt.y - HEAD_H + p->scroll) / ROW_H);
    if (r >= p->m.nrows) r = -1;
    u64 mods = MSG0(u64, ev, "modifierFlags");
    int fm = ((mods & (1u << 17)) ? FP_MOD_SHIFT : 0) | ((mods & (1u << 20)) ? FP_MOD_CMD : 0);
    i64 clicks = MSG0(i64, ev, "clickCount");
    fp_click(&p->m, r, fm);
    after_change(p);
    if (clicks >= 2 && r >= 0 && !fm) enter_row(p, r);
}

static void list_scroll(id self, SEL c, id ev) {
    (void)self; (void)c;
    fpanel_t *p = g_active;
    if (!p) return;
    double dy = MSG0(double, ev, "scrollingDeltaY");
    double max = p->m.nrows * ROW_H - (p->list_h - HEAD_H);
    p->scroll -= dy * ROW_H;
    if (p->scroll > max) p->scroll = max;
    if (p->scroll < 0) p->scroll = 0;
    m_bool(p->list, "setNeedsDisplay:", YES);
}

static void up_dir(fpanel_t *p) {
    char par[FP_PATH_MAX], cur[FP_PATH_MAX];
    strcpy(cur, p->m.dir);
    if (!fp_parent(cur, par)) return;
    load_dir(p, par);
    if (fp_select_name(&p->m, fp_basename(cur)) >= 0) after_change(p);
}

static void cancel_panel(fpanel_t *p) { finish(p, 0); }

/* atalhos com ⌘⇧; devolve 1 se tratou */
static int panel_shortcut(fpanel_t *p, u16 code, u64 mods) {
    BOOL cmd = (mods & (1u << 20)) != 0, shift = (mods & (1u << 17)) != 0;
    if (!cmd) return 0;
    if (code == 126) { up_dir(p); return 1; }                                   /* ⌘↑ */
    if (code == 125) { if (p->m.cursor >= 0) enter_row(p, p->m.cursor); return 1; } /* ⌘↓ */
    if (!shift) return 0;
    char t[FP_PATH_MAX];
    switch (code) {
    case 5: go_to_folder(p, p->m.dir); return 1;                                 /* ⌘⇧G */
    case 4: load_dir(p, p->m.home); return 1;                                    /* ⌘⇧H */
    case 2: if (fp_join(p->m.home, "Desktop", t)) load_dir(p, t); return 1;      /* ⌘⇧D */
    case 45: if (p->can_create) new_folder(p); return 1;                         /* ⌘⇧N */
    case 47: p->m.show_hidden = !p->m.show_hidden; fp_refresh(&p->m); after_change(p); return 1; /* ⌘⇧. */
    }
    return 0;
}

static void list_key_down(id self, SEL c, id ev) {
    (void)self; (void)c;
    fpanel_t *p = g_active;
    if (!p) return;
    u16 code = MSG0(u16, ev, "keyCode");
    u64 mods = MSG0(u64, ev, "modifierFlags");
    BOOL shift = (mods & (1u << 17)) != 0;
    if (panel_shortcut(p, code, mods)) return;
    int k = -1;
    switch (code) {
    case 126: k = FP_KEY_UP; break; case 125: k = FP_KEY_DOWN; break;
    case 115: k = FP_KEY_HOME; break; case 119: k = FP_KEY_END; break;
    case 116: k = FP_KEY_PAGE_UP; break; case 121: k = FP_KEY_PAGE_DOWN; break;
    case 36: case 76:
        if (fp_selected_count(&p->m) == 1 && fp_activate(&p->m, p->m.cursor) == FP_ACT_ENTER_DIR && (p->m.open_panel ? !p->m.can_dirs : 1)) enter_row(p, p->m.cursor);
        else confirm(p);
        return;
    case 53: cancel_panel(p); return;
    }
    if (k >= 0) { fp_key(&p->m, k, shift, page_rows(p)); after_change(p); return; }
    if (mods & ((1u << 20) | (1u << 18))) return;
    id ch = MSG0(id, ev, "characters");
    const char *s = ch ? cx_cstr(ch) : "";
    if ((s[0] == '/' || s[0] == '~') && !p->m.ntyped) { go_to_folder(p, s); return; }
    if ((unsigned char)s[0] >= 0x20 && s[0] != 0x7F) {
        double now = MSG0(double, ev, "timestamp");
        if (fp_type_ahead(&p->m, s, now) >= 0) after_change(p);
    }
}

/* ---------------------------------------------------------------- barra lateral */
static void side_setup(fpanel_t *p) {
    static const struct { const char *sub, *name; } favs[] = {
        {"", "Início"}, {"Desktop", "Mesa"}, {"Documents", "Documentos"}, {"Downloads", "Downloads"},
    };
    p->nside = 0;
    for (unsigned i = 0; i < sizeof favs / sizeof favs[0] && p->nside < SIDE_MAX; i++) {
        char t[FP_PATH_MAX];
        if (!(favs[i].sub[0] ? fp_join(p->m.home, favs[i].sub, t) : (strcpy(t, p->m.home), 1))) continue;
        if (!is_dir_path(t)) continue;
        strcpy(p->side_path[p->nside], t); strcpy(p->side_name[p->nside], favs[i].name); p->nside++;
    }
    static const struct { const char *path, *name; } locs[] = { {"/Applications", "Aplicativos"}, {"/Volumes", "Volumes"}, {"/", "Disco"} };
    for (unsigned i = 0; i < sizeof locs / sizeof locs[0] && p->nside < SIDE_MAX; i++) {
        if (!is_dir_path(locs[i].path)) continue;
        strcpy(p->side_path[p->nside], locs[i].path); strcpy(p->side_name[p->nside], locs[i].name); p->nside++;
    }
}
static double side_row_y(int i) { return 28 + i * 24.0; }
static void side_draw(id self, SEL c, R dirty) {
    (void)self; (void)c; (void)dirty;
    fpanel_t *p = g_active;
    if (!p) return;
    fonts();
    MSG0(void, cx_color(0.92, 0.92, 0.93, 1), "setFill");
    NSRectFill((R){0, 0, 150, p->side_h});
    id ah = attrs_for(g_font_bold, cx_color(0.45, 0.45, 0.48, 1));
    m_draw_at(cx_str("Locais"), (P){12, 8}, ah);
    id a = attrs_for(g_font, cx_color(0.1, 0.1, 0.1, 1));
    for (int i = 0; i < p->nside; i++) {
        double y = side_row_y(i);
        if (!strcmp(p->side_path[i], p->m.dir)) { MSG0(void, cx_color(0, 0, 0, 0.1), "setFill"); NSRectFill((R){6, y, 138, 22}); }
        MSG0(void, cx_color(0.33, 0.63, 0.95, 1), "setFill");
        NSRectFill((R){14, y + 7, 5, 2});
        NSRectFill((R){14, y + 9, 13, 8});
        m_draw_at(fit(p->side_name[i], 104, a), (P){34, y + 3}, a);
    }
}
static void side_mouse_down(id self, SEL c, id ev) {
    (void)c;
    fpanel_t *p = g_active;
    if (!p) return;
    P pt = m_loc(self, ev);
    for (int i = 0; i < p->nside; i++) {
        double y = side_row_y(i);
        if (pt.y >= y && pt.y < y + 22) { load_dir(p, p->side_path[i]); if (p->list) MSG(void, p->win, "makeFirstResponder:", p->list); return; }
    }
}

/* ---------------------------------------------------------------- alvo dos botões */
static id g_target;
static id target_obj(void) { if (!g_target) g_target = cx_alloc_instance(&_MOSFilePanelTarget_c); return g_target; }
static void tg_ok(id self, SEL c, id s) { (void)self; (void)c; (void)s; if (g_active) confirm(g_active); }
static void tg_cancel(id self, SEL c, id s) { (void)self; (void)c; (void)s; if (g_active) cancel_panel(g_active); }
static void tg_up(id self, SEL c, id s) { (void)self; (void)c; (void)s; if (g_active) up_dir(g_active); }
static void tg_new(id self, SEL c, id s) { (void)self; (void)c; (void)s; if (g_active) new_folder(g_active); }
static void tg_text(id self, SEL c, id note) { (void)self; (void)c; (void)note; if (g_active) update_buttons(g_active); }

/* ---------------------------------------------------------------- janela */
static void build_window(fpanel_t *p) {
    fonts();
    BOOL save = !p->m.open_panel;
    double W = 680, acc_h = 0, msg_h = p->message ? 26 : 0, name_h = save ? 36 : 0;
    if (p->accessory) {
        R af = ((R (*)(id, SEL))objc_msgSend_stret)(p->accessory, sl("frame"));
        acc_h = af.h + 12;
        if (af.w + 40 > W) W = af.w + 40;
    }
    double list_area = 300, H = 48 + acc_h + list_area + 40 + name_h + msg_h;
    id w = ((id (*)(id, SEL, R, u64, u64, BOOL))objc_msgSend)(MSG0(id, (id)kls("NSPanel"), "alloc"),
        sl("initWithContentRect:styleMask:backing:defer:"), (R){0, 0, W, H}, (u64)(1 | 2 | 8), (u64)2, NO);
    const char *tt = p->title ? cx_cstr(p->title) : save ? "Salvar" : "Abrir";
    MSG(void, w, "setTitle:", cx_str(tt));
    MSG(void, w, "setReleasedWhenClosed:", (BOOL)NO);
    id cv = MSG0(id, w, "contentView");
    double y = H;
    if (p->message) {
        y -= msg_h;
        id ml = MSG(id, (id)kls("NSTextField"), "labelWithString:", p->message);
        m_set_frame(ml, (R){16, y + 4, W - 32, 18});
        MSG(void, cv, "addSubview:", ml);
        p->msg_label = ml;
    }
    if (save) {
        y -= name_h;
        const char *lab = p->name_label ? cx_cstr(p->name_label) : "Salvar como:";
        id lb = MSG(id, (id)kls("NSTextField"), "labelWithString:", cx_str(lab));
        m_set_frame(lb, (R){16, y + 10, 100, 18});
        MSG(void, cv, "addSubview:", lb);
        id nf = MSG(id, (id)kls("NSTextField"), "textFieldWithString:", p->name_value ? p->name_value : cx_str(""));
        m_set_frame(nf, (R){118, y + 6, W - 134 - 160, 24});
        MSG(void, nf, "setDelegate:", target_obj());
        MSG(void, cv, "addSubview:", nf);
        p->name_field = nf;
    }
    /* caminho e "pasta acima" */
    y -= 40;
    id up = MSG(id, (id)kls("NSButton"), "buttonWithTitle:target:action:", cx_str("\xE2\x86\x91"), target_obj(), sl("cxPanelUp:"));
    m_set_frame(up, (R){160, y + 8, 32, 24});
    MSG(void, cv, "addSubview:", up);
    id pl = MSG(id, (id)kls("NSTextField"), "labelWithString:", cx_str(p->m.dir));
    m_set_frame(pl, (R){200, y + 12, W - 216, 18});
    MSG(void, cv, "addSubview:", pl);
    p->path = pl;
    /* lista e barra lateral */
    double ly = 48 + acc_h;
    double lh = y - ly;
    p->side_h = lh + 40;
    p->side = m_init_frame((id)&_MOSFilePanelSide_c, (R){0, ly, 150, p->side_h});
    MSG(void, cv, "addSubview:", p->side);
    p->list_w = W - 160 - 12;
    p->list_h = lh - 4;
    p->list = m_init_frame((id)&_MOSFilePanelList_c, (R){160, ly + 2, p->list_w, p->list_h});
    MSG(void, cv, "addSubview:", p->list);
    if (p->accessory) {
        R af = ((R (*)(id, SEL))objc_msgSend_stret)(p->accessory, sl("frame"));
        m_set_frame(p->accessory, (R){(W - af.w) / 2, 48, af.w, af.h});
        MSG(void, cv, "addSubview:", p->accessory);
    }
    /* botões */
    const char *okt = p->prompt ? cx_cstr(p->prompt) : save ? "Salvar" : "Abrir";
    p->ok_btn = MSG(id, (id)kls("NSButton"), "buttonWithTitle:target:action:", cx_str(okt), target_obj(), sl("cxPanelOK:"));
    m_set_frame(p->ok_btn, (R){W - 108, 12, 92, 24});
    MSG(void, p->ok_btn, "setKeyEquivalent:", cx_str("\r"));
    MSG(void, cv, "addSubview:", p->ok_btn);
    p->cancel_btn = MSG(id, (id)kls("NSButton"), "buttonWithTitle:target:action:", cx_str("Cancelar"), target_obj(), sl("cxPanelCancel:"));
    m_set_frame(p->cancel_btn, (R){W - 208, 12, 92, 24});
    MSG(void, p->cancel_btn, "setKeyEquivalent:", cx_str("\033"));
    MSG(void, cv, "addSubview:", p->cancel_btn);
    if (p->can_create) {
        p->new_btn = MSG(id, (id)kls("NSButton"), "buttonWithTitle:target:action:", cx_str("Nova pasta"), target_obj(), sl("cxPanelNew:"));
        m_set_frame(p->new_btn, (R){16, 12, 104, 24});
        MSG(void, cv, "addSubview:", p->new_btn);
    }
    p->win = w;
    side_setup(p);
    MSG0(void, w, "center");
}

static void teardown(fpanel_t *p) {
    if (!p->win) return;
    if (p->accessory) MSG0(void, p->accessory, "removeFromSuperview");
    MSG(void, p->win, "orderOut:", (id)0);
    MSG0(void, p->win, "close");
    objc_release(p->win);
    p->win = p->list = p->side = p->path = p->name_field = p->ok_btn = p->cancel_btn = p->new_btn = p->msg_label = 0;
}

static i64 run_panel(fpanel_t *p) {
    if (p->running) return 0;
    fpanel_t *prev = g_active;
    g_active = p;
    p->m.allow_other_types = p->m.allow_other_types;   /* já ajustado pelo setter */
    apply_types(p);
    char start[FP_PATH_MAX];
    strcpy(start, p->m.dir);
    if (p->dir_url) {
        id path = MSG0(id, p->dir_url, "path");
        if (path) fp_normalize(cx_cstr(path), "/", p->m.home, start);
    }
    if (!is_dir_path(start)) strcpy(start, p->m.home);
    if (!is_dir_path(start)) strcpy(start, "/");
    build_window(p);
    p->running = YES;
    load_dir(p, start);
    MSG(void, p->win, "makeFirstResponder:", p->name_field ? p->name_field : p->list);
    update_buttons(p);
    i64 r = MSG(i64, NSApp, "runModalForWindow:", p->win);
    p->running = NO;
    if (r != 1) r = 0;
    if (!r) { keep(&p->url, 0); id a = cx_arr_new(kls("NSMutableArray")); keep(&p->urls, a); objc_release(a); }
    keep(&p->dir_url, url_for(p->m.dir, 1));
    teardown(p);
    g_active = prev;
    return r;
}

/* ---------------------------------------------------------------- métodos */
#define P_ (fpanel(self))
static i64 pn_run(id self, SEL c) { (void)c; fpanel_t *p = P_; return p ? run_panel(p) : 0; }
typedef struct { id panel; void *blk; } pend_t;
static void pn_async(void *ctx) {
    pend_t *d = ctx;
    i64 r = pn_run(d->panel, 0);
    if (d->blk) { void (*inv)(void *, i64) = *(void **)((u8 *)d->blk + 16); inv(d->blk, r); _Block_release(d->blk); }
    objc_release(d->panel);
    free(d);
}
static void pn_begin(id self, SEL c, void *blk) {
    (void)c;
    pend_t *d = calloc(1, sizeof *d);
    if (!d) return;
    d->panel = objc_retain(self);
    d->blk = blk ? _Block_copy(blk) : 0;
    dispatch_async_f(dispatch_get_main_queue(), d, pn_async);
}
static void pn_begin_sheet(id self, SEL c, id win, void *blk) { (void)win; pn_begin(self, c, blk); }
static id pn_url(id self, SEL c) { (void)c; fpanel_t *p = P_; return p ? p->url : 0; }
static id pn_urls(id self, SEL c) { (void)c; fpanel_t *p = P_; if (!p) return 0; if (!p->urls) { id a = cx_arr_new(kls("NSMutableArray")); keep(&p->urls, a); objc_release(a); } return p->urls; }
static id pn_filename(id self, SEL c) { id u = pn_url(self, c); return u ? MSG0(id, u, "path") : 0; }
static id pn_filenames(id self, SEL c) {
    id a = cx_arr_new(kls("NSMutableArray")), us = pn_urls(self, c);
    u64 n = us ? MSG0(u64, us, "count") : 0;
    for (u64 i = 0; i < n; i++) { id pth = MSG0(id, MSG(id, us, "objectAtIndex:", i), "path"); if (pth) cx_arr_add(a, pth); }
    return objc_autorelease(a);
}
static void pn_ok(id self, SEL c, id s) { (void)c; (void)s; fpanel_t *p = P_; if (p && p->running && g_active == p) confirm(p); }
static void pn_cancel(id self, SEL c, id s) { (void)c; (void)s; fpanel_t *p = P_; if (p && p->running) finish(p, 0); }
static void pn_validate_cols(id self, SEL c) {
    (void)c; fpanel_t *p = P_;
    if (!p || !p->running) return;
    char cur[FP_PATH_MAX]; strcpy(cur, p->m.dir); load_dir(p, cur);
}

#define OBJ(fn, field, extra) \
    static id g_##fn(id self, SEL c) { (void)c; fpanel_t *p = P_; return p ? p->field : 0; } \
    static void s_##fn(id self, SEL c, id v) { (void)c; fpanel_t *p = P_; if (!p) return; keep(&p->field, v); extra; }
#define BOOLP(fn, expr_get, stmt_set) \
    static BOOL g_##fn(id self, SEL c) { (void)c; fpanel_t *p = P_; return p ? (BOOL)(expr_get) : NO; } \
    static void s_##fn(id self, SEL c, BOOL v) { (void)c; fpanel_t *p = P_; if (!p) return; stmt_set; if (p->running) { fp_refresh(&p->m); after_change(p); } }

OBJ(title, title, if (p->win) MSG(void, p->win, "setTitle:", v))
OBJ(message, message, if (p->msg_label) MSG(void, p->msg_label, "setStringValue:", v ? v : cx_str("")))
OBJ(prompt, prompt, if (p->ok_btn && v) MSG(void, p->ok_btn, "setTitle:", v))
OBJ(name_label, name_label, (void)0)
OBJ(name_value, name_value, if (p->name_field) MSG(void, p->name_field, "setStringValue:", v ? v : cx_str("")))
OBJ(dir_url, dir_url, if (p->running && v) { id pth = MSG0(id, v, "path"); if (pth) load_dir(p, cx_cstr(pth)); })
OBJ(types, types, apply_types(p); if (p->running) { fp_refresh(&p->m); after_change(p); })
OBJ(ctypes, ctypes, apply_types(p); if (p->running) { fp_refresh(&p->m); after_change(p); })
OBJ(accessory, accessory, (void)0)
OBJ(delegate_, delegate, (void)0)
OBJ(identifier, identifier, (void)0)
OBJ(tag_names, tag_names, (void)0)
BOOLP(can_files, p->m.can_files, p->m.can_files = v)
BOOLP(can_dirs, p->m.can_dirs, p->m.can_dirs = v)
BOOLP(multi, p->m.multi, p->m.multi = v)
BOOLP(hidden, p->m.show_hidden, p->m.show_hidden = v)
BOOLP(pkgs, p->m.pkgs_as_dirs, p->m.pkgs_as_dirs = v)
BOOLP(other_types, p->m.allow_other_types, p->m.allow_other_types = v)
BOOLP(can_create, p->can_create, p->can_create = v)
BOOLP(ext_hidden, p->ext_hidden, p->ext_hidden = v)
BOOLP(can_hide_ext, p->can_hide_ext, p->can_hide_ext = v)
BOOLP(resolves, p->resolves_aliases, p->resolves_aliases = v)
BOOLP(tags, p->shows_tags, p->shows_tags = v)
BOOLP(acc_disc, p->accessory_disclosed, p->accessory_disclosed = v)
BOOLP(ubiq, p->can_download_ubiq, p->can_download_ubiq = v)

static id pn_directory(id self, SEL c) { (void)c; fpanel_t *p = P_; return p ? cx_str(p->m.dir) : 0; }
static void pn_set_directory(id self, SEL c, id path) { if (path) s_dir_url(self, c, url_for(cx_cstr(path), 1)); }
static id pn_required_type(id self, SEL c) { id t = g_types(self, c); return t && MSG0(u64, t, "count") ? MSG(id, t, "objectAtIndex:", (u64)0) : 0; }
static void pn_set_required_type(id self, SEL c, id t) {
    id a = cx_arr_new(kls("NSMutableArray"));
    if (t) cx_arr_add(a, t);
    s_types(self, c, a);
    objc_release(a);
}
static void pn_set_legacy(id self, id dir, id file, id types) {
    if (dir) pn_set_directory(self, 0, dir);
    if (file) s_name_value(self, 0, file);
    if (types) s_types(self, 0, types);
}
static i64 pn_run_dir_file_types(id self, SEL c, id dir, id file, id types) { (void)c; pn_set_legacy(self, dir, file, types); return pn_run(self, 0); }
static i64 pn_run_dir_file(id self, SEL c, id dir, id file) { (void)c; pn_set_legacy(self, dir, file, 0); return pn_run(self, 0); }
static i64 pn_run_types(id self, SEL c, id types) { (void)c; pn_set_legacy(self, 0, 0, types); return pn_run(self, 0); }
static id pn_new(id k, SEL c) { (void)c; return objc_autorelease(cx_alloc_instance((cx_class_t *)k)); }
static id pn_text_field(id self, SEL c) { (void)c; fpanel_t *p = P_; return p ? p->name_field : 0; }

void cx_appkit_panel_init(void) {
    cx_class_t *view = kls("NSView"), *obj = kls("NSObject");
    cx_define_class(&_MOSFilePanelList_c, &_MOSFilePanelList_m, "_MOSFilePanelList", view, 0);
    cx_add_method(&_MOSFilePanelList_c, "drawRect:", (void *)list_draw);
    cx_add_method(&_MOSFilePanelList_c, "isFlipped", (void *)yes_imp);
    cx_add_method(&_MOSFilePanelList_c, "acceptsFirstResponder", (void *)yes_imp);
    cx_add_method(&_MOSFilePanelList_c, "mouseDown:", (void *)list_mouse_down);
    cx_add_method(&_MOSFilePanelList_c, "keyDown:", (void *)list_key_down);
    cx_add_method(&_MOSFilePanelList_c, "scrollWheel:", (void *)list_scroll);
    cx_define_class(&_MOSFilePanelSide_c, &_MOSFilePanelSide_m, "_MOSFilePanelSide", view, 0);
    cx_add_method(&_MOSFilePanelSide_c, "drawRect:", (void *)side_draw);
    cx_add_method(&_MOSFilePanelSide_c, "isFlipped", (void *)yes_imp);
    cx_add_method(&_MOSFilePanelSide_c, "mouseDown:", (void *)side_mouse_down);
    cx_define_class(&_MOSFilePanelTarget_c, &_MOSFilePanelTarget_m, "_MOSFilePanelTarget", obj, 0);
    cx_add_method(&_MOSFilePanelTarget_c, "cxPanelOK:", (void *)tg_ok);
    cx_add_method(&_MOSFilePanelTarget_c, "cxPanelCancel:", (void *)tg_cancel);
    cx_add_method(&_MOSFilePanelTarget_c, "cxPanelUp:", (void *)tg_up);
    cx_add_method(&_MOSFilePanelTarget_c, "cxPanelNew:", (void *)tg_new);
    cx_add_method(&_MOSFilePanelTarget_c, "cxPromptOK:", (void *)tg_prompt_ok);
    cx_add_method(&_MOSFilePanelTarget_c, "cxPromptCancel:", (void *)tg_prompt_cancel);
    cx_add_method(&_MOSFilePanelTarget_c, "controlTextDidChange:", (void *)tg_text);

    cx_class_t *sp = kls("NSSavePanel"), *op = kls("NSOpenPanel");
    if (!sp || !op) return;
    cx_add_cmethod(sp, "savePanel", (void *)pn_new);
    cx_add_cmethod(op, "openPanel", (void *)pn_new);
    static const struct { const char *sel; void *imp; } m[] = {
        {"runModal", (void *)pn_run}, {"beginWithCompletionHandler:", (void *)pn_begin},
        {"beginSheetModalForWindow:completionHandler:", (void *)pn_begin_sheet},
        {"runModalForDirectory:file:types:", (void *)pn_run_dir_file_types}, {"runModalForDirectory:file:", (void *)pn_run_dir_file},
        {"runModalForTypes:", (void *)pn_run_types},
        {"URL", (void *)pn_url}, {"URLs", (void *)pn_urls}, {"filename", (void *)pn_filename}, {"filenames", (void *)pn_filenames},
        {"ok:", (void *)pn_ok}, {"cancel:", (void *)pn_cancel}, {"validateVisibleColumns", (void *)pn_validate_cols},
        {"directory", (void *)pn_directory}, {"setDirectory:", (void *)pn_set_directory},
        {"requiredFileType", (void *)pn_required_type}, {"setRequiredFileType:", (void *)pn_set_required_type},
        {"_nameFieldTextField", (void *)pn_text_field},
        {"title", (void *)g_title}, {"setTitle:", (void *)s_title}, {"message", (void *)g_message}, {"setMessage:", (void *)s_message},
        {"prompt", (void *)g_prompt}, {"setPrompt:", (void *)s_prompt}, {"nameFieldLabel", (void *)g_name_label}, {"setNameFieldLabel:", (void *)s_name_label},
        {"nameFieldStringValue", (void *)g_name_value}, {"setNameFieldStringValue:", (void *)s_name_value},
        {"directoryURL", (void *)g_dir_url}, {"setDirectoryURL:", (void *)s_dir_url},
        {"allowedFileTypes", (void *)g_types}, {"setAllowedFileTypes:", (void *)s_types},
        {"allowedContentTypes", (void *)g_ctypes}, {"setAllowedContentTypes:", (void *)s_ctypes},
        {"accessoryView", (void *)g_accessory}, {"setAccessoryView:", (void *)s_accessory},
        {"delegate", (void *)g_delegate_}, {"setDelegate:", (void *)s_delegate_},
        {"identifier", (void *)g_identifier}, {"setIdentifier:", (void *)s_identifier},
        {"tagNames", (void *)g_tag_names}, {"setTagNames:", (void *)s_tag_names},
        {"showsHiddenFiles", (void *)g_hidden}, {"setShowsHiddenFiles:", (void *)s_hidden},
        {"treatsFilePackagesAsDirectories", (void *)g_pkgs}, {"setTreatsFilePackagesAsDirectories:", (void *)s_pkgs},
        {"allowsOtherFileTypes", (void *)g_other_types}, {"setAllowsOtherFileTypes:", (void *)s_other_types},
        {"canCreateDirectories", (void *)g_can_create}, {"setCanCreateDirectories:", (void *)s_can_create},
        {"isExtensionHidden", (void *)g_ext_hidden}, {"setExtensionHidden:", (void *)s_ext_hidden},
        {"canSelectHiddenExtension", (void *)g_can_hide_ext}, {"setCanSelectHiddenExtension:", (void *)s_can_hide_ext},
        {"showsTagField", (void *)g_tags}, {"setShowsTagField:", (void *)s_tags},
        {"isAccessoryViewDisclosed", (void *)g_acc_disc}, {"setAccessoryViewDisclosed:", (void *)s_acc_disc},
    };
    for (unsigned i = 0; i < sizeof m / sizeof m[0]; i++) cx_add_method(sp, m[i].sel, m[i].imp);
    static const struct { const char *sel; void *imp; } mo[] = {
        {"canChooseFiles", (void *)g_can_files}, {"setCanChooseFiles:", (void *)s_can_files},
        {"canChooseDirectories", (void *)g_can_dirs}, {"setCanChooseDirectories:", (void *)s_can_dirs},
        {"allowsMultipleSelection", (void *)g_multi}, {"setAllowsMultipleSelection:", (void *)s_multi},
        {"resolvesAliases", (void *)g_resolves}, {"setResolvesAliases:", (void *)s_resolves},
        {"canDownloadUbiquitousContents", (void *)g_ubiq}, {"setCanDownloadUbiquitousContents:", (void *)s_ubiq},
        {"canResolveUbiquitousConflicts", (void *)g_ubiq}, {"setCanResolveUbiquitousConflicts:", (void *)s_ubiq},
    };
    for (unsigned i = 0; i < sizeof mo / sizeof mo[0]; i++) cx_add_method(op, mo[i].sel, mo[i].imp);
}
