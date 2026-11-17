/*
 * mOS — libSystem: AppleScript para os apps e para o /usr/bin/osascript.
 *
 * NSAppleScript (initWithSource:, initWithContentsOfURL:error:, compileAndReturnError:, executeAndReturnError:,
 * executeAppleEvent:error:) e NSUserAppleScriptTask rodam o interpretador de as_interp.inc. Aqui fica o lado do
 * sistema (as_host): "tell application" vira Apple Event de verdade (appleevents.c: o app alvo recebe pelo serviço
 * Mach dele, com o portão de automação do TCC); display dialog / alert e choose from list são um NSAlert; display
 * notification é o banner da Interface; do shell script roda /bin/sh -c (o primeiro shell POSIX que existir);
 * keystroke / key code são CGEvents (precisam de Acessibilidade, como no macOS); the clipboard é o NSPasteboard;
 * id/version/running de application vêm do Launch Services.
 *
 * Valores: o resultado volta como NSAppleEventDescriptor (texto 'utf8', inteiro 'long', real 'doub', booleano,
 * lista, registro com 'usrf', arquivo 'furl', data 'ldt ', missing value 'msng'). Erros: o dicionário com
 * NSAppleScriptErrorNumber, NSAppleScriptErrorMessage, NSAppleScriptErrorBriefMessage e NSAppleScriptErrorRange.
 *
 * SPI do osascript: cx_osascript_run (userland/cli-macos/osascript.c).
 * Teste de host do interpretador e dos descritores: tools/astest.c.
 */
#include "foundation.h"
#include "libc.h"
#include "sys.h"

typedef short i16;
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunused-function"
#include "ae_core.inc"
#include "as_interp.inc"
#pragma clang diagnostic pop

int cx_snprintf(char *buf, unsigned long n, const char *fmt, ...);
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void *objc_autoreleasePoolPush(void);
void objc_autoreleasePoolPop(void *p);
int getpid(void);
int usleep(unsigned us);
int access(const char *path, int mode);
int open(const char *path, int flags, ...);
int close(int fd);
long read(int fd, void *b, unsigned long n);
long write(int fd, const void *b, unsigned long n);
int unlink(const char *path);
int pipe(int *fds);
int waitpid(int pid, int *st, int opt);
int posix_spawn(int *pid, const char *f, const void *fa, const void *at, char *const *av, char *const *ev);
int posix_spawn_file_actions_init(void *a);
int posix_spawn_file_actions_destroy(void *a);
int posix_spawn_file_actions_adddup2(void *a, int x, int y);
int posix_spawn_file_actions_addclose(void *a, int x);
int posix_spawn_file_actions_addopen(void *a, int fd, const char *path, int flags, unsigned mode);
extern char **environ;
int pthread_main_np(void);
void *dispatch_get_main_queue(void);
void dispatch_after_f(u64 when, void *q, void *ctx, void (*fn)(void *));
u64 dispatch_time(u64 when, i64 delta);
double CFAbsoluteTimeGetCurrent(void);
id CFDateCreate(void *alloc, double at);
void CFRelease(id o);
id NSHomeDirectory(void);
id NSTemporaryDirectory(void);
id NSSearchPathForDirectoriesInDomains(unsigned long dir, unsigned long mask, unsigned char expand);
int cx_ls_app_lookup(const char *key, int by_id, char *bundle, size_t bc, char *path, size_t pc, char *name, size_t nc);
int cx_ls_launch_app(const char *bundle);
int cx_open_target(const char *target, const char *app, int by_ident, int timeout_ms);
int cx_tcc_preflight(const char *service);
int cx_tcc_request(const char *service);
const void *cx_aed_node(id d);
id cx_aed_from_node_copy(const void *n);
int cx_ae_send_node(const void *ev, void **reply, int mode, long timeout_ms);
int (*cx_osa_send_hook)(const void *ev_node, int to_self, void **reply_node);
int cx_ae_app_running(const char *bundle);
void *CGEventCreateKeyboardEvent(void *src, u16 key, u8 down);
void CGEventSetFlags(void *e, u64 f);
void CGEventKeyboardSetUnicodeString(void *e, u64 n, const u16 *s);
void CGEventPost(u32 tap, void *e);

static id cls(const char *n) { return (id)cx_class_named(n); }
static BOOL responds(id o, const char *sel) { return o && cx_responds(object_getClass(o), sel_registerName(sel)); }
static char *dup_c(const char *s) { return as_strdup(s ? s : ""); }
static char *dup_ns(id s) { return s && cx_is_str(s) ? dup_c(cx_cstr(s)) : 0; }

/* ================================================================ valores: as_val <-> descritor */
static void pct_encode(as_sb *b, const char *p) {
    static const char hex[] = "0123456789ABCDEF";
    for (const u8 *s = (const u8 *)p; *s; s++) {
        u8 ch = *s;
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || strchr("/-._~!$&'()*+,;=:@", ch)) sb_add(b, (const char *)s, 1);
        else { char e[3] = {'%', hex[ch >> 4], hex[ch & 15]}; sb_add(b, e, 3); }
    }
}
static int hexv(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
static char *furl_to_path(const u8 *d, u32 n) {
    char *s = malloc(n + 1);
    if (!s) return 0;
    memcpy(s, d, n);
    s[n] = 0;
    const char *p = s;
    if (!strncmp(p, "file://", 7)) { p += 7; if (!strncmp(p, "localhost", 9)) p += 9; }
    char *o = malloc(strlen(p) + 1), *w = o;
    if (!o) { free(s); return 0; }
    for (; *p; p++) {
        if (*p == '%' && hexv(p[1]) >= 0 && hexv(p[2]) >= 0) { *w++ = (char)(hexv(p[1]) * 16 + hexv(p[2])); p += 2; }
        else *w++ = *p;
    }
    *w = 0;
    free(s);
    return o;
}
static ae_node *file_node(const char *path) {
    as_sb b = {0};
    sb_str(&b, "file://");
    pct_encode(&b, path);
    ae_node *n = ae_scalar(AE_FURL, b.b, (u32)b.n);
    free(b.b);
    return n;
}
#define LDT_FROM_ABS 3061152000.0
static ae_node *val_to_ae(const as_val *v) {
    switch (v->kind) {
    case AV_BOOL: return ae_bool(v->num != 0);
    case AV_NUM:
        if (v->is_int && v->num >= -2147483648.0 && v->num <= 2147483647.0) return ae_i32((i32)v->num);
        return ae_f64(v->num);
    case AV_TEXT: case AV_APP: return ae_text(v->s ? v->s : "");
    case AV_CONST: return ae_text(v->s ? v->s : "");
    case AV_FILE: return file_node(v->s ? v->s : "");
    case AV_DATE: { i64 t = (i64)(v->num + LDT_FROM_ABS); return ae_scalar(AE_LDT, &t, 8); }
    case AV_LIST: {
        ae_node *l = ae_list();
        for (int i = 0; l && i < v->n; i++) ae_list_put(l, 0, val_to_ae(&v->items[i]));
        return l;
    }
    case AV_RECORD: {                                     /* registro do usuário: 'usrf' = {chave, valor, ...} */
        ae_node *r = ae_record(), *u = ae_list();
        for (int i = 0; r && u && i < v->n; i++) { ae_list_put(u, 0, ae_text(v->keys[i])); ae_list_put(u, 0, val_to_ae(&v->items[i])); }
        if (r && u) ae_put_key(r, AE_FCC('u', 's', 'r', 'f'), u); else ae_free(u);
        return r;
    }
    default: return ae_code(AE_TYPE, AE_FCC('m', 's', 'n', 'g'));
    }
}
static as_val ae_to_val(const ae_node *d) {
    as_val v = as_missing();
    if (!d) return v;
    if (d->kind == AEK_LIST) {
        v.kind = AV_LIST;
        for (u32 i = 0; i < d->count; i++) as_list_add(&v, ae_to_val(d->items[i]));
        return v;
    }
    if (d->kind == AEK_RECORD || d->kind == AEK_EVENT) {
        v.kind = AV_RECORD;
        ae_node *u = ae_get_key(d, AE_FCC('u', 's', 'r', 'f'));
        for (u32 i = 0; u && u->kind == AEK_LIST && i + 1 < u->count; i += 2) {
            char *k = ae_utf8_of(u->items[i]);
            as_rec_set(&v, k ? k : "", ae_to_val(u->items[i + 1]));
            free(k);
        }
        for (u32 i = 0; i < d->count; i++) {               /* chaves de 4 letras: «class xxxx» */
            if (d->keys && d->keys[i] == AE_FCC('u', 's', 'r', 'f')) continue;
            u32 k = d->keys ? d->keys[i] : 0;
            char kn[16] = {(char)(k >> 24), (char)(k >> 16), (char)(k >> 8), (char)k, 0};
            as_rec_set(&v, kn, ae_to_val(d->items[i]));
        }
        return v;
    }
    u32 t = d->type;
    if (ae_is_text(t)) { char *s = ae_utf8_of(d); return as_txt_take(s ? s : as_strdup("")); }
    if (t == AE_BOOL || t == AE_FCC('t', 'r', 'u', 'e') || t == AE_FCC('f', 'a', 'l', 's')) {
        int b = t == AE_FCC('t', 'r', 'u', 'e') || (t == AE_BOOL && d->n && d->data[0]);
        return as_boolv(b);
    }
    if (t == AE_FURL) { as_val f = {AV_FILE, 0, 0, furl_to_path(d->data, d->n), 0, 0, 0}; return f; }
    if (t == AE_LDT && d->n == 8) { i64 x; memcpy(&x, d->data, 8); v.kind = AV_DATE; v.num = (double)x - LDT_FROM_ABS; return v; }
    if (t == AE_NULL) return v;
    if ((t == AE_TYPE || t == AE_ENUM) && d->n == 4) {
        u32 c;
        memcpy(&c, d->data, 4);
        if (c == AE_FCC('m', 's', 'n', 'g')) return v;
        char s[24];
        snprintf(s, sizeof s, "«constant %c%c%c%c»", c >> 24, c >> 16 & 255, c >> 8 & 255, c & 255);
        as_val k = {AV_CONST, 0, 0, as_strdup(s), 0, 0, 0};
        return k;
    }
    double num;
    if (ae_num_of(d, &num) == 0) return t == AE_LONG || t == AE_FCC('s', 'h', 'o', 'r') || t == AE_FCC('c', 'o', 'm', 'p') ? as_num(num) : as_real(num);
    ae_node *s = ae_coerce(d, AE_UTF8);
    if (s) { char *x = ae_utf8_of(s); ae_free(s); return as_txt_take(x ? x : as_strdup("")); }
    return v;
}

/* ================================================================ as_host do mOS */
typedef struct { int is_cli; const char *me; } host_t;
static volatile int g_dialog_gen;

/* nome do app ou identificador → identificador do pacote; 0 = achou */
static int resolve_app(const char *app, char *bundle, usize bc, char *path, usize pc, char *name, usize nc) {
    if (!cx_ls_app_lookup(app, 0, bundle, bc, path, pc, name, nc)) return 0;
    if (strchr(app, '.') && !cx_ls_app_lookup(app, 1, bundle, bc, path, pc, name, nc)) return 0;
    usize l = strlen(app);
    if (l > 4 && !strcmp(app + l - 4, ".app")) {          /* "Editor de Texto.app" */
        char base[256];
        snprintf(base, sizeof base, "%.*s", (int)(l - 4), app);
        if (!cx_ls_app_lookup(base, 0, bundle, bc, path, pc, name, nc)) return 0;
    }
    return -1;
}
static const char *err_text(int e) {
    switch (e) {
    case -600: return "O aplicativo não está aberto.";
    case -1708: return "O aplicativo não entende a mensagem.";
    case -1712: return "O aplicativo demorou demais para responder.";
    case -1743: return "Sem autorização para mandar Apple Events para o aplicativo.";
    case -1744: return "Mandar Apple Events para o aplicativo pede a autorização do usuário.";
    case -10814: return "Não foi possível achar o aplicativo.";
    case -128: return "O usuário cancelou.";
    }
    return 0;
}
/* o 'odoc'/'pdoc' leva uma lista de 'furl' */
static ae_node *docs_param(const as_val *d) {
    ae_node *l = ae_list();
    const as_val *items = d->kind == AV_LIST ? d->items : d;
    int n = d->kind == AV_LIST ? d->n : 1;
    for (int i = 0; l && i < n; i++) {
        const as_val *it = &items[i];
        char *t = it->kind == AV_FILE || it->kind == AV_TEXT ? dup_c(it->s) : as_to_text(it);
        if (!t) continue;
        if (strstr(t, "://")) ae_list_put(l, 0, ae_scalar(AE_FURL, t, (u32)strlen(t)));
        else ae_list_put(l, 0, file_node(t));
        free(t);
    }
    return l;
}
static int h_send(void *ctx, const char *app, unsigned ecls, unsigned eid, const as_val *direct, as_val *result, char *err, int errcap) {
    (void)ctx;
    *result = as_missing();
    char bundle[256] = "", path[512] = "", name[256] = "";
    int self = !app || !app[0];
    if (!self && resolve_app(app, bundle, sizeof bundle, path, sizeof path, name, sizeof name)) {
        /* o Finder do mOS é a Interface: abrir documentos e pastas vai direto ao Launch Services */
        if (!strcmp(app, "Finder")) {
            if (ecls == AE_FCC('a', 'e', 'v', 't') && (eid == AE_FCC('o', 'd', 'o', 'c') || eid == AE_FCC('p', 'd', 'o', 'c')) && direct) {
                const as_val *items = direct->kind == AV_LIST ? direct->items : direct;
                int n = direct->kind == AV_LIST ? direct->n : 1;
                for (int i = 0; i < n; i++) {
                    char *t = items[i].kind == AV_FILE || items[i].kind == AV_TEXT ? dup_c(items[i].s) : as_to_text(&items[i]);
                    int r = t ? cx_open_target(t, 0, 0, 10000) : -1;
                    if (r) { snprintf(err, (usize)errcap, "Não foi possível abrir “%s”.", t ? t : ""); free(t); return -43; }
                    free(t);
                }
                return 0;
            }
            if (ecls == AE_FCC('G', 'U', 'R', 'L') && direct && direct->kind == AV_TEXT)
                return cx_open_target(direct->s, 0, 0, 10000) ? (snprintf(err, (usize)errcap, "Não foi possível abrir a URL."), -1708) : 0;
            if (ecls == AE_FCC('m', 'i', 's', 'c') || eid == AE_FCC('o', 'a', 'p', 'p') || eid == AE_FCC('r', 'a', 'p', 'p') || eid == AE_FCC('q', 'u', 'i', 't')) return 0;
        }
        snprintf(err, (usize)errcap, "Não foi possível achar o aplicativo “%s”.", app);
        return -10814;
    }
    /* launch: abre sem mandar evento (e sem trazer para a frente, quando já está aberto) */
    if (ecls == AE_FCC('a', 's', 'c', 'r') && eid == AE_FCC('n', 'o', 'o', 'p')) {
        if (self || cx_ae_app_running(bundle)) return 0;
        if (cx_ls_launch_app(bundle)) { snprintf(err, (usize)errcap, "Não foi possível abrir “%s”.", app); return -10810; }
        for (int w = 0; w < 600 && !cx_ae_app_running(bundle); w++) usleep(100000);
        return 0;
    }
    int pid = getpid();
    ae_node *target = self ? ae_scalar(AE_KPID, &pid, 4) : ae_scalar(AE_BUND, bundle, (u32)strlen(bundle));
    ae_node *ev = ae_event(ecls, eid, target, 0, 0);
    if (!ev) return -108;
    if (direct) {
        if (ecls == AE_FCC('a', 'e', 'v', 't') && (eid == AE_FCC('o', 'd', 'o', 'c') || eid == AE_FCC('p', 'd', 'o', 'c'))) ae_put_key(ev, AE_DIRECT, docs_param(direct));
        else ae_put_key(ev, AE_DIRECT, val_to_ae(direct));
    }
    void *reply = 0;
    int rc = 1;
    /* OSA (carbon_osa.c): o procedimento de envio e o alvo padrão do app que hospeda o script; 1 = envio normal */
    if (cx_osa_send_hook) rc = cx_osa_send_hook(ev, self, &reply);
    if (rc == 1) rc = cx_ae_send_node(ev, &reply, 3 | 0x20, 120000);  /* kAEWaitReply | kAECanInteract */
    ae_free(ev);
    ae_node *r = reply;
    if (!rc && r) {
        ae_node *en = ae_get_key(r, AE_ERRN);
        double code = 0;
        if (en) ae_num_of(en, &code);
        if (code) {
            ae_node *es = ae_get_key(r, AE_ERRS);
            char *m = es ? ae_utf8_of(es) : 0;
            const char *dflt = err_text((int)code);
            snprintf(err, (usize)errcap, "%s", m && m[0] ? m : dflt ? dflt : "O aplicativo respondeu com erro.");
            free(m);
            ae_free(r);
            return (int)code;
        }
        ae_node *d = ae_get_key(r, AE_DIRECT);
        if (d) *result = ae_to_val(d);
    }
    ae_free(r);
    if (rc) {
        const char *t = err_text(rc);
        snprintf(err, (usize)errcap, "%s (%s)", t ? t : "O Apple Event falhou.", name[0] ? name : app && app[0] ? app : "este script");
    }
    return rc;
}

/* do shell script: o primeiro shell POSIX; a saída padrão é o resultado e o erro padrão é a mensagem do erro */
static const char *posix_shell(void) {
    static const char *const sh[] = {"/bin/sh", "/bin/zsh", "/bin/bash", "/usr/local/bin/bash", "/usr/bin/bash", 0};
    for (int i = 0; sh[i]; i++) if (access(sh[i], 1) == 0) return sh[i];
    return 0;
}
static char *read_all(int fd) {
    as_sb b = {0};
    char buf[4096];
    long n;
    while ((n = read(fd, buf, sizeof buf)) > 0) sb_add(&b, buf, (usize)n);
    return b.b ? b.b : as_strdup("");
}
static int h_shell(void *ctx, const char *cmd, char **out, int *status) {
    (void)ctx;
    *out = 0;
    *status = 0;
    const char *sh = posix_shell();
    if (!sh) { *out = as_strdup("sh: não há shell POSIX (/bin/sh) neste sistema"); *status = 127; return 0; }
    static int seq;
    char errpath[128];
    cx_snprintf(errpath, sizeof errpath, "/tmp/.osascript-err-%d-%d", getpid(), __sync_fetch_and_add(&seq, 1));
    int fds[2];
    if (pipe(fds)) return -1;
    void *fa = 0;
    posix_spawn_file_actions_init(&fa);
    posix_spawn_file_actions_addopen(&fa, 0, "/dev/null", 0, 0);
    posix_spawn_file_actions_adddup2(&fa, fds[1], 1);
    posix_spawn_file_actions_addclose(&fa, fds[0]);
    posix_spawn_file_actions_addclose(&fa, fds[1]);
    posix_spawn_file_actions_addopen(&fa, 2, errpath, MAC_O_WRONLY | MAC_O_CREAT | MAC_O_TRUNC, 0600);
    char *const av[] = {(char *)"sh", (char *)"-c", (char *)cmd, 0};
    int pid = 0;
    int r = posix_spawn(&pid, sh, &fa, 0, av, environ);
    posix_spawn_file_actions_destroy(&fa);
    close(fds[1]);
    if (r) { close(fds[0]); unlink(errpath); return -1; }
    char *o = read_all(fds[0]);
    close(fds[0]);
    int st = 0;
    while (waitpid(pid, &st, 0) < 0) { if (cx_errno != 4) break; }       /* EINTR */
    int code = (st & 0x7f) ? 128 + (st & 0x7f) : (st >> 8) & 0xff;
    if (code) {
        int ef = open(errpath, 0);
        char *e = ef >= 0 ? read_all(ef) : as_strdup("");
        if (ef >= 0) close(ef);
        free(o);
        o = e;
    }
    unlink(errpath);
    *out = o;
    *status = code;
    return 0;
}

/* diálogos: NSAlert modal (os botões da direita para a esquerda, como no AppleScript) */
typedef struct { int gen; } giveup_t;
static void give_up(void *ctx) {
    giveup_t *g = ctx;
    if (g->gen == g_dialog_gen) { id app = MSG0(id, cls("NSApplication"), "sharedApplication"); if (app) MSG(void, app, "stopModalWithCode:", (i64)-1002); }
    free(g);
}
static id make_alert(const char *text, const char *title) {
    MSG0(id, cls("NSApplication"), "sharedApplication");
    id al = MSG0(id, MSG0(id, cls("NSAlert"), "alloc"), "init");
    if (!al) return 0;
    /* com título: o título é a mensagem em negrito e o texto vai embaixo, como o diálogo do macOS */
    if (title && title[0]) { MSG(void, al, "setMessageText:", cx_str(title)); MSG(void, al, "setInformativeText:", cx_str(text)); }
    else MSG(void, al, "setMessageText:", cx_str(text));
    return al;
}
static i64 run_alert(id al, int giving_up) {
    int gen = __sync_add_and_fetch(&g_dialog_gen, 1);
    if (giving_up > 0) {
        giveup_t *g = malloc(sizeof *g);
        if (g) { g->gen = gen; dispatch_after_f(dispatch_time(0, (i64)giving_up * 1000000000LL), dispatch_get_main_queue(), g, give_up); }
    }
    i64 r = MSG0(i64, al, "runModal");
    __sync_add_and_fetch(&g_dialog_gen, 1);
    return r;
}
typedef struct { double x, y, w, h; } rect_t;                 /* = NSRect */
static rect_t rect4(double x, double y, double w, double h) { rect_t r = {x, y, w, h}; return r; }
static int h_dialog(void *ctx, const char *text, const char *title, const char **buttons, int nb, int def, int giving_up,
                    const char *answer_in, char **answer_out) {
    (void)ctx;
    void *pool = objc_autoreleasePoolPush();
    id al = make_alert(text, title);
    if (!al) { objc_autoreleasePoolPop(pool); return -1; }
    for (int i = nb - 1; i >= 0; i--) {
        id b = MSG(id, al, "addButtonWithTitle:", cx_str(buttons[i]));
        if (b && responds(b, "setKeyEquivalent:")) {
            const char *k = i == def ? "\r" : lower_eq(buttons[i], "Cancel") || lower_eq(buttons[i], "Cancelar") ? "\033" : "";
            MSG(void, b, "setKeyEquivalent:", cx_str(k));
        }
    }
    id field = 0;
    if (answer_out) {
        field = MSG(id, MSG0(id, cls("NSTextField"), "alloc"), "initWithFrame:", rect4(0, 0, 300, 24));
        if (field) {
            MSG(void, field, "setStringValue:", cx_str(answer_in ? answer_in : ""));
            MSG(void, al, "setAccessoryView:", field);
        }
    }
    i64 r = run_alert(al, giving_up);
    int idx;
    if (r == -1002) idx = -2;
    else {
        idx = nb - 1 - (int)(r - 1000);
        if (idx < 0 || idx >= nb) idx = -1;
        else if (lower_eq(buttons[idx], "Cancel") || lower_eq(buttons[idx], "Cancelar")) idx = -1;
    }
    if (answer_out) *answer_out = field ? dup_ns(MSG0(id, field, "stringValue")) : 0;
    if (field) objc_release(field);
    objc_release(al);
    objc_autoreleasePoolPop(pool);
    return idx;
}
static char *h_choose(void *ctx, const char **items, int n, const char *prompt) {
    (void)ctx;
    void *pool = objc_autoreleasePoolPush();
    id al = make_alert(prompt && prompt[0] ? prompt : "Escolha um item:", 0);
    char *res = 0;
    if (al) {
        MSG(id, al, "addButtonWithTitle:", cx_str("OK"));
        MSG(id, al, "addButtonWithTitle:", cx_str("Cancelar"));
        id pop = MSG(id, MSG0(id, cls("NSPopUpButton"), "alloc"), "initWithFrame:pullsDown:", rect4(0, 0, 300, 26), (BOOL)NO);
        for (int i = 0; pop && i < n; i++) MSG(void, pop, "addItemWithTitle:", cx_str(items[i]));
        if (pop) MSG(void, al, "setAccessoryView:", pop);
        i64 r = run_alert(al, 0);
        if (r == 1000 && pop) res = dup_ns(MSG0(id, pop, "titleOfSelectedItem"));
        else if (r == 1000 && n) res = dup_c(items[0]);
        if (pop) objc_release(pop);
        objc_release(al);
    }
    objc_autoreleasePoolPop(pool);
    return res;
}
static void h_notify(void *ctx, const char *text, const char *title, const char *subtitle, const char *sound) {
    host_t *h = ctx;
    (void)sound;
    as_sb body = {0};
    if (subtitle && subtitle[0]) { sb_str(&body, subtitle); sb_str(&body, "\n"); }
    sb_str(&body, text ? text : "");
    const char *t = title && title[0] ? title : h && h->me ? h->me : "AppleScript";
    SYS4(SYS_DESKTOP, MOS_DESK_BANNER, t, body.b ? body.b : "", 0);
    free(body.b);
}
static char *h_clip_get(void *ctx) {
    (void)ctx;
    id pb = MSG0(id, cls("NSPasteboard"), "generalPasteboard");
    id s = pb ? MSG(id, pb, "stringForType:", cx_str("public.utf8-plain-text")) : 0;
    return s ? dup_ns(s) : as_strdup("");
}
static void h_clip_set(void *ctx, const char *text) {
    (void)ctx;
    id pb = MSG0(id, cls("NSPasteboard"), "generalPasteboard");
    if (!pb) return;
    MSG0(i64, pb, "clearContents");
    MSG(BOOL, pb, "setString:forType:", cx_str(text ? text : ""), cx_str("public.utf8-plain-text"));
}
/* UTF-8 → UTF-16 */
static int utf16_of(const char *s, u16 *out, int cap) {
    int n = 0;
    const u8 *p = (const u8 *)s;
    while (*p && n < cap - 1) {
        u32 c = *p++;
        if (c >= 0xC0) {
            int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : 1;
            c &= 0x3F >> extra;
            while (extra-- && (*p & 0xC0) == 0x80) c = c << 6 | (*p++ & 0x3F);
        }
        if (c >= 0x10000) { if (n + 2 >= cap) break; c -= 0x10000; out[n++] = (u16)(0xD800 + (c >> 10)); out[n++] = (u16)(0xDC00 + (c & 0x3FF)); }
        else out[n++] = (u16)c;
    }
    return n;
}
static int h_keys(void *ctx, const char *text, int keycode, unsigned mods) {
    (void)ctx;
    if (!cx_tcc_preflight("kTCCServiceAccessibility")) { cx_tcc_request("kTCCServiceAccessibility"); return 1002; }
    if (text) {
        u16 u[512];
        int n = utf16_of(text, u, 512);
        void *d = CGEventCreateKeyboardEvent(0, 0, 1), *up = CGEventCreateKeyboardEvent(0, 0, 0);
        if (!d || !up) return -108;
        CGEventSetFlags(d, mods);
        CGEventSetFlags(up, mods);
        CGEventKeyboardSetUnicodeString(d, (u64)n, u);
        CGEventPost(0, d);
        CGEventPost(0, up);
        CFRelease((id)d);
        CFRelease((id)up);
        return 0;
    }
    void *d = CGEventCreateKeyboardEvent(0, (u16)keycode, 1), *up = CGEventCreateKeyboardEvent(0, (u16)keycode, 0);
    if (!d || !up) return -108;
    CGEventSetFlags(d, mods);
    CGEventSetFlags(up, mods);
    CGEventPost(0, d);
    CGEventPost(0, up);
    CFRelease((id)d);
    CFRelease((id)up);
    return 0;
}
static id running_named(const char *name) {
    id apps = MSG0(id, cls("NSRunningApplication"), "runningApplications");
    u64 n = apps ? MSG0(u64, apps, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id a = MSG(id, apps, "objectAtIndex:", i);
        id nm = MSG0(id, a, "localizedName");
        if (nm && cx_is_str(nm) && lower_eq(cx_cstr(nm), name)) return a;
    }
    return 0;
}
static char *h_app_info(void *ctx, const char *app, const char *what) {
    (void)ctx;
    char bundle[256], path[512], name[256];
    if (!strcmp(what, "file-exists")) return access(app, 0) == 0 ? as_strdup("true") : 0;
    if (!strcmp(what, "name-for-id")) return cx_ls_app_lookup(app, 1, 0, 0, 0, 0, name, sizeof name) ? 0 : as_strdup(name);
    if (!strcmp(what, "frontmost")) {                    /* app (processo) na frente? */
        id ws = MSG0(id, cls("NSWorkspace"), "sharedWorkspace");
        id f = ws ? MSG0(id, ws, "frontmostApplication") : 0;
        id nm = f ? MSG0(id, f, "localizedName") : 0;
        return as_strdup(nm && cx_is_str(nm) && lower_eq(cx_cstr(nm), app) ? "true" : "false");
    }
    if (resolve_app(app, bundle, sizeof bundle, path, sizeof path, name, sizeof name)) {
        if (!strcmp(what, "running")) return as_strdup(running_named(app) ? "true" : "false");
        return 0;
    }
    if (!strcmp(what, "id")) return as_strdup(bundle);
    if (!strcmp(what, "name")) return as_strdup(name);
    if (!strcmp(what, "path")) return as_strdup(path);
    if (!strcmp(what, "running")) return as_strdup(cx_ae_app_running(bundle) || running_named(name) ? "true" : "false");
    if (!strcmp(what, "version")) {
        id b = MSG(id, cls("NSBundle"), "bundleWithPath:", cx_str(path));
        id v = b ? MSG(id, b, "objectForInfoDictionaryKey:", cx_str("CFBundleShortVersionString")) : 0;
        if (!v || !cx_is_str(v)) v = b ? MSG(id, b, "objectForInfoDictionaryKey:", cx_str("CFBundleVersion")) : 0;
        return v && cx_is_str(v) ? dup_ns(v) : as_strdup("");
    }
    return 0;
}
/* path to <pasta>: caminhos POSIX com a barra final (pastas) */
static char *h_path_to(void *ctx, const char *which) {
    host_t *h = ctx;
    static const struct { const char *w; unsigned long dir, dom; } t[] = {
        {"desktop", 12, 1}, {"desktop folder", 12, 1}, {"documents folder", 9, 1}, {"downloads folder", 15, 1},
        {"movies folder", 17, 1}, {"music folder", 18, 1}, {"pictures folder", 19, 1}, {"library folder", 5, 1},
        {"application support", 14, 1}, {"application support folder", 14, 1}, {"applications folder", 1, 2},
        {"caches folder", 13, 1}, {"preferences folder", 5, 1},
    };
    char out[1024];
    if (lower_eq(which, "home folder") || lower_eq(which, "home")) {
        id hd = NSHomeDirectory();
        cx_snprintf(out, sizeof out, "%s/", hd ? cx_cstr(hd) : "");
        return as_strdup(out);
    }
    if (lower_eq(which, "temporary items") || lower_eq(which, "temporary items folder")) {
        id td = NSTemporaryDirectory();
        const char *s = td ? cx_cstr(td) : "/tmp/";
        usize l = strlen(s);
        cx_snprintf(out, sizeof out, "%s%s", s, l && s[l - 1] == '/' ? "" : "/");
        return as_strdup(out);
    }
    if (lower_eq(which, "me") || lower_eq(which, "current application")) {
        if (h && h->me && h->me[0] == '/') return as_strdup(h->me);
        id mb = MSG0(id, cls("NSBundle"), "mainBundle");
        id p = mb ? MSG0(id, mb, "bundlePath") : 0;
        return p ? dup_ns(p) : 0;
    }
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++) {
        if (!lower_eq(which, t[i].w)) continue;
        id a = NSSearchPathForDirectoriesInDomains(t[i].dir, t[i].dom, 1);
        id p = a && MSG0(u64, a, "count") ? MSG(id, a, "objectAtIndex:", (u64)0) : 0;
        if (!p || !cx_is_str(p)) return 0;
        cx_snprintf(out, sizeof out, "%s%s", cx_cstr(p), lower_eq(which, "preferences folder") ? "/Preferences/" : "/");
        return as_strdup(out);
    }
    return 0;
}
static char **h_processes(void *ctx, int *n) {
    (void)ctx;
    *n = 0;
    id apps = MSG0(id, cls("NSRunningApplication"), "runningApplications");
    u64 c = apps ? MSG0(u64, apps, "count") : 0;
    char **out = calloc(c ? c : 1, sizeof *out);
    if (!out) return 0;
    for (u64 i = 0; i < c; i++) {
        id nm = MSG0(id, MSG(id, apps, "objectAtIndex:", i), "localizedName");
        if (nm && cx_is_str(nm) && cx_cstr(nm)[0]) out[(*n)++] = dup_ns(nm);
    }
    return out;
}
static int h_frontmost(void *ctx, const char *process) {
    (void)ctx;
    id a = running_named(process);
    if (!a) return -1;
    MSG(BOOL, a, "activateWithOptions:", (u64)2);              /* NSApplicationActivateIgnoringOtherApps */
    return 0;
}
static void h_log(void *ctx, const char *text) {
    (void)ctx;
    write(2, text, strlen(text));
    write(2, "\n", 1);
}
static void h_delay(void *ctx, double s) {
    (void)ctx;
    if (s <= 0) return;
    /* no fio principal de um app, a janela continua viva durante o delay */
    if (pthread_main_np() && cx_class_named("NSRunLoop")) {
        id rl = MSG0(id, cls("NSRunLoop"), "currentRunLoop");
        id until = MSG(id, cls("NSDate"), "dateWithTimeIntervalSinceNow:", s);
        if (rl && until) { MSG(void, rl, "runUntilDate:", until); return; }
    }
    usleep((unsigned)(s * 1e6));
}
static double h_now(void *ctx) { (void)ctx; return CFAbsoluteTimeGetCurrent(); }

static as_host make_host(host_t *ctx) {
    as_host h = {ctx, h_send, h_shell, h_dialog, h_notify, h_clip_get, h_clip_set, h_keys, h_app_info, h_path_to,
                 h_processes, h_frontmost, h_log, h_delay, h_choose, h_now};
    return h;
}

/* ================================================================ osascript (SPI) */
/* roda src; imprime o resultado em out (malloc; formato do osascript: -s h legível, -s s código) e devolve 0, ou o
 * número do erro com a mensagem em err ("linha:coluna: execution error: ... (n)") */
int cx_osascript_run(const char *src, const char *const *argv, int argc, int source_form, const char *me,
                     char **out, char *err, int errcap) {
    host_t hc = {1, me};
    as_host h = make_host(&hc);
    as_val *args = argc > 0 ? calloc((usize)argc, sizeof *args) : 0;
    for (int i = 0; i < argc; i++) args[i] = as_txt(argv[i]);
    as_val res;
    char msg[400] = "";
    int line = 0, col = 0;
    void *pool = objc_autoreleasePoolPush();
    int rc = as_run_ex(src, &h, 0, args, argc, &res, msg, sizeof msg, &line, &col);
    objc_autoreleasePoolPop(pool);
    for (int i = 0; i < argc; i++) as_free(&args[i]);
    free(args);
    *out = 0;
    if (rc) {
        snprintf(err, (usize)errcap, "%d:%d: %s: %s (%d)", line, col, rc == -2741 ? "syntax error" : "execution error", msg, rc);
        return rc;
    }
    if (res.kind != AV_MISSING || source_form) {
        as_sb s = {0};
        as_repr(&res, &s, source_form);
        *out = s.b ? s.b : as_strdup("");
    }
    as_free(&res);
    return 0;
}

/* ================================================================ NSAppleScript */
cx_class_t NSAppleScript_c __asm__("OBJC_CLASS_$_NSAppleScript");
cx_class_t NSAppleScript_m __asm__("OBJC_METACLASS_$_NSAppleScript");
cx_class_t NSUserAppleScriptTask_c __asm__("OBJC_CLASS_$_NSUserAppleScriptTask");
cx_class_t NSUserAppleScriptTask_m __asm__("OBJC_METACLASS_$_NSUserAppleScriptTask");
typedef struct { void *isa; id source; id url; int compiled; } scr_t;
static u32 g_scr_base, g_ut_base;                            /* onde começam os campos (tamanho da superclasse) */
static scr_t *scr(id o) { return (scr_t *)((u8 *)o + g_scr_base - sizeof(void *)); }

static id str_key(const char *k) { return cx_str(k); }
static id err_dict(int num, const char *msg, int line, int col, const char *src) {
    id d = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    id n = MSG(id, cls("NSNumber"), "numberWithInt:", num);
    MSG(void, d, "setObject:forKey:", n, str_key("NSAppleScriptErrorNumber"));
    MSG(void, d, "setObject:forKey:", cx_str(msg), str_key("NSAppleScriptErrorMessage"));
    MSG(void, d, "setObject:forKey:", cx_str(msg), str_key("NSAppleScriptErrorBriefMessage"));
    /* faixa: do começo da linha:coluna até o fim da linha */
    u64 loc = 0, len = 0;
    if (src && line > 0) {
        const char *p = src;
        for (int l = 1; l < line && *p; p++) if (*p == '\n' || *p == '\r') l++;
        loc = (u64)(p - src) + (u64)(col > 0 ? col - 1 : 0);
        const char *e = src + loc;
        while (*e && *e != '\n' && *e != '\r') e++;
        len = (u64)(e - (src + loc));
    }
    typedef struct { u64 location, length; } nsrange_t;
    nsrange_t rg = {loc, len};
    id rv = MSG(id, cls("NSValue"), "valueWithRange:", rg);
    if (rv) MSG(void, d, "setObject:forKey:", rv, str_key("NSAppleScriptErrorRange"));
    return d;
}
static id scr_init_source(id self, SEL c, id src) {
    (void)c;
    if (!self) return 0;
    if (!src || !cx_is_str(src)) { objc_release(self); return 0; }
    scr(self)->source = MSG0(id, src, "copy");
    return self;
}
static char *read_text(const char *path, int *bad) {
    *bad = 0;
    int fd = open(path, 0);
    if (fd < 0) return 0;
    char *s = read_all(fd);
    close(fd);
    /* script compilado (.scpt binário) não é texto: o mOS só roda o fonte */
    if (s && (!strncmp(s, "FasdUAS", 7) || memchr(s, 0, strlen(s) < 16 ? strlen(s) : 16))) *bad = 1;
    return s;
}
static id scr_init_url(id self, SEL c, id url, id *errinfo) {
    (void)c;
    if (errinfo) *errinfo = 0;
    id p = url ? MSG0(id, url, "path") : 0;
    int bad = 0;
    char *s = p && cx_is_str(p) ? read_text(cx_cstr(p), &bad) : 0;
    if (!s || bad) {
        if (errinfo) *errinfo = err_dict(bad ? -1751 : -43, bad ? "Scripts compilados (.scpt) não rodam no mOS; use o texto do script (.applescript)." : "O arquivo do script não existe.", 0, 0, 0);
        free(s);
        objc_release(self);
        return 0;
    }
    scr(self)->source = objc_retain(cx_str(s));
    scr(self)->url = objc_retain(url);
    free(s);
    return self;
}
static void scr_dealloc(id self, SEL c) {
    (void)c;
    objc_release(scr(self)->source);
    objc_release(scr(self)->url);
    void (*sup)(id, SEL) = (void (*)(id, SEL))cx_lookup(cx_class_named("NSObject"), sel_registerName("dealloc"));
    if (sup) sup(self, sel_registerName("dealloc"));
}
static id scr_source(id self, SEL c) { (void)c; return scr(self)->source; }
static BOOL scr_is_compiled(id self, SEL c) { (void)c; return scr(self)->compiled ? YES : NO; }
/* análise sem efeito: um host sem ações; só o erro de sintaxe conta */
static int null_send(void *x, const char *a, unsigned c, unsigned i, const as_val *d, as_val *r, char *e, int n) { (void)x; (void)a; (void)c; (void)i; (void)d; (void)e; (void)n; *r = as_missing(); return 0; }
static BOOL scr_compile(id self, SEL c, id *errinfo) {
    (void)c;
    if (errinfo) *errinfo = 0;
    const char *src = cx_cstr(scr(self)->source);
    as_ctx *x = calloc(1, sizeof *x);
    if (!x) return NO;
    as_host h = {0};
    h.send = null_send;
    x->h = &h;
    int rc = 0;
    if (lex(x, src) == 0) {
        prescan_handlers(x);
        x->failed = 0;
        x->pos = 0;
        static const char *const none[] = {0};
        block(x, 0, none);
        if (x->failed && x->errnum == -2741) rc = -2741;
    } else rc = x->errnum ? x->errnum : -2741;
    if (rc && errinfo) *errinfo = err_dict(rc, x->err, x->err_line, x->err_col, src);
    for (int i = 0; i < x->n; i++) free(x->tk[i].s);
    free(x->tk);
    for (int i = 0; i < x->nvars; i++) { free(x->vars[i].name); as_free(&x->vars[i].v); }
    for (int i = 0; i < x->nhandlers; i++) { free(x->handlers[i].name); for (int k = 0; k < x->handlers[i].np; k++) free(x->handlers[i].params[k]); free(x->handlers[i].params); }
    as_free(&x->retval);
    as_free(&x->result);
    free(x);
    if (!rc) scr(self)->compiled = 1;
    return rc ? NO : YES;
}
static id run_script(id self, const char *call, const as_val *args, int nargs, id *errinfo) {
    if (errinfo) *errinfo = 0;
    const char *src = cx_cstr(scr(self)->source);
    id mb = MSG0(id, cls("NSBundle"), "mainBundle");
    id nm = mb ? MSG(id, mb, "objectForInfoDictionaryKey:", cx_str("CFBundleName")) : 0;
    host_t hc = {0, nm && cx_is_str(nm) ? cx_cstr(nm) : 0};
    as_host h = make_host(&hc);
    as_val res;
    char msg[400] = "";
    int line = 0, col = 0;
    int rc = as_run_ex(src, &h, call, args, nargs, &res, msg, sizeof msg, &line, &col);
    if (rc) {
        if (errinfo) *errinfo = err_dict(rc, msg, line, col, src);
        return 0;
    }
    scr(self)->compiled = 1;
    ae_node *n = val_to_ae(&res);
    if (res.kind == AV_MISSING) { ae_free(n); n = ae_scalar(AE_NULL, 0, 0); }
    as_free(&res);
    id d = cx_aed_from_node_copy(n);
    ae_free(n);
    return d;
}
static id scr_execute(id self, SEL c, id *errinfo) { (void)c; return run_script(self, 0, 0, 0, errinfo); }
/* executeAppleEvent: 'aevt'/'oapp' roda o script; 'ascr'/'psbr' chama o handler 'snam' com a lista '----';
 * os outros eventos vão para o handler de mesmo nome (open, quit, reopen) */
static id scr_execute_event(id self, SEL c, id event, id *errinfo) {
    (void)c;
    const ae_node *e = event ? cx_aed_node(event) : 0;
    if (errinfo) *errinfo = 0;
    if (!e || e->kind != AEK_EVENT) { if (errinfo) *errinfo = err_dict(-1704, "O descritor não é um Apple Event.", 0, 0, 0); return 0; }
    const char *call = 0;
    char *name = 0;
    as_val args[16];
    int na = 0;
    u32 ec = e->ev_class, ei = e->ev_id;
    if (ec == AE_FCC('a', 's', 'c', 'r') && ei == AE_FCC('p', 's', 'b', 'r')) {
        ae_node *sn = ae_get_key(e, AE_FCC('s', 'n', 'a', 'm'));
        name = sn ? ae_utf8_of(sn) : 0;
        if (!name) { if (errinfo) *errinfo = err_dict(-1701, "Falta o nome do handler ('snam').", 0, 0, 0); return 0; }
        for (char *p = name; *p; p++) if (*p >= 'A' && *p <= 'Z') *p += 32;
        call = name;
        ae_node *d = ae_get_key(e, AE_DIRECT);
        if (d && d->kind == AEK_LIST) for (u32 i = 0; i < d->count && na < 16; i++) args[na++] = ae_to_val(d->items[i]);
        else if (d) args[na++] = ae_to_val(d);
    } else if (ec == AE_FCC('a', 'e', 'v', 't')) {
        if (ei == AE_FCC('o', 'd', 'o', 'c')) call = "open";
        else if (ei == AE_FCC('q', 'u', 'i', 't')) call = "quit";
        else if (ei == AE_FCC('r', 'a', 'p', 'p')) call = "reopen";
        else if (ei == AE_FCC('p', 'd', 'o', 'c')) call = "print";
        ae_node *d = call ? ae_get_key(e, AE_DIRECT) : 0;
        if (d) args[na++] = ae_to_val(d);
    }
    id r = run_script(self, call, args, na, errinfo);
    for (int i = 0; i < na; i++) as_free(&args[i]);
    free(name);
    return r;
}

/* NSUserAppleScriptTask: roda o arquivo e chama o bloco (NSAppleEventDescriptor, NSError) numa fila global */
typedef struct { void *isa; int flags, reserved; void *invoke; } blk_t;
void *_Block_copy(const void *b);
void _Block_release(const void *b);
void *dispatch_get_global_queue(long prio, unsigned long flags);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
typedef struct { id script; id event; void *done; } task_job_t;
typedef struct { void *isa; id url; } utask_t;
static utask_t *ut(id o) { return (utask_t *)((u8 *)o + g_ut_base - sizeof(void *)); }
static id ut_init(id self, SEL c, id url, id *err) {
    (void)c;
    if (err) *err = 0;
    id p = url ? MSG0(id, url, "path") : 0;
    if (!p || !cx_is_str(p) || access(cx_cstr(p), 0)) {
        if (err) *err = MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:", cx_str("NSCocoaErrorDomain"), (i64)260, (id)0);
        objc_release(self);
        return 0;
    }
    ut(self)->url = objc_retain(url);
    return self;
}
static id ut_url(id self, SEL c) { (void)c; return ut(self)->url; }
static void ut_dealloc(id self, SEL c) {
    (void)c;
    objc_release(ut(self)->url);
    void (*sup)(id, SEL) = (void (*)(id, SEL))cx_lookup(cx_class_named("NSObject"), sel_registerName("dealloc"));
    if (sup) sup(self, sel_registerName("dealloc"));
}
static void ut_job(void *ctx) {
    task_job_t *j = ctx;
    void *pool = objc_autoreleasePoolPush();
    id info = 0, res = j->event ? scr_execute_event(j->script, 0, j->event, &info) : scr_execute(j->script, 0, &info);
    id err = 0;
    if (!res) {
        id num = info ? MSG(id, info, "objectForKey:", cx_str("NSAppleScriptErrorNumber")) : 0;
        err = MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:", cx_str("NSOSStatusErrorDomain"), (i64)(num ? MSG0(int, num, "intValue") : -1), info);
    }
    if (j->done) { ((void (*)(void *, id, id))((blk_t *)j->done)->invoke)(j->done, res, err); _Block_release(j->done); }
    objc_release(j->script);
    objc_release(j->event);
    free(j);
    objc_autoreleasePoolPop(pool);
}
static void ut_execute(id self, SEL c, id event, void *done) {
    (void)c;
    id info = 0;
    id s = scr_init_url(MSG0(id, cls("NSAppleScript"), "alloc"), 0, ut(self)->url, &info);
    if (!s) {
        id err = MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:", cx_str("NSOSStatusErrorDomain"), (i64)-43, info);
        if (done) ((void (*)(void *, id, id))((blk_t *)done)->invoke)(done, (id)0, err);
        return;
    }
    task_job_t *j = calloc(1, sizeof *j);
    if (!j) { objc_release(s); return; }
    j->script = s;
    j->event = objc_retain(event);
    j->done = done ? _Block_copy(done) : 0;
    dispatch_async_f(dispatch_get_global_queue(0, 0), j, ut_job);
}

void cx_applescript_init(void) {
    static int done;
    if (done++) return;
    cx_class_t *ob = cx_class_named("NSObject");
    g_scr_base = cx_instance_size(ob);
    cx_define_class(&NSAppleScript_c, &NSAppleScript_m, "NSAppleScript", ob, sizeof(scr_t) - sizeof(void *));
    cx_add_method(&NSAppleScript_c, "initWithSource:", (void *)scr_init_source);
    cx_add_method(&NSAppleScript_c, "initWithContentsOfURL:error:", (void *)scr_init_url);
    cx_add_method(&NSAppleScript_c, "source", (void *)scr_source);
    cx_add_method(&NSAppleScript_c, "isCompiled", (void *)scr_is_compiled);
    cx_add_method(&NSAppleScript_c, "compileAndReturnError:", (void *)scr_compile);
    cx_add_method(&NSAppleScript_c, "executeAndReturnError:", (void *)scr_execute);
    cx_add_method(&NSAppleScript_c, "executeAppleEvent:error:", (void *)scr_execute_event);
    cx_add_method(&NSAppleScript_c, "dealloc", (void *)scr_dealloc);
    cx_class_t *st = cx_class_named("NSUserScriptTask");
    g_ut_base = cx_instance_size(st ? st : ob);
    cx_define_class(&NSUserAppleScriptTask_c, &NSUserAppleScriptTask_m, "NSUserAppleScriptTask", st ? st : ob, sizeof(utask_t) - sizeof(void *));
    cx_add_method(&NSUserAppleScriptTask_c, "initWithURL:error:", (void *)ut_init);
    cx_add_method(&NSUserAppleScriptTask_c, "scriptURL", (void *)ut_url);
    cx_add_method(&NSUserAppleScriptTask_c, "executeWithAppleEvent:completionHandler:", (void *)ut_execute);
    cx_add_method(&NSUserAppleScriptTask_c, "dealloc", (void *)ut_dealloc);
}
