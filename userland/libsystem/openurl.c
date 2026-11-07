/*
 * mOS libSystem — Launch Services, UTIs e NSWorkspace para os apps, e a chegada de URLs e documentos vindos de
 * outros apps (os eventos kAEGetURL e kAEOpenDocuments do macOS).
 *
 * O banco fica no kernel (kernel/compat/launchsvc.c, syscall SYS_LS): ele lê o Info.plist de cada .app em
 * /Aplicativos (CFBundleDocumentTypes, CFBundleURLTypes, UTExportedTypeDeclarations), guarda as escolhas do
 * usuário (~/Library/Preferences/com.apple.LaunchServices/com.apple.launchservices.secure.plist) e abre: apps do
 * sistema direto, apps do macOS por uma mensagem Mach. Aqui ficam as APIs: LSCopyDefaultHandlerForURLScheme,
 * LSSetDefaultRoleHandlerForContentType, UTTypeCreatePreferredIdentifierForTag, -[NSWorkspace openURL:],
 * setDefaultApplicationAtURL:toOpenURLsWithScheme:..., URLForApplicationToOpenContentType: etc. É assim que o
 * Chrome vira o navegador padrão ("Definir como padrão" → o sistema pergunta) e que um descompactador instalado
 * passa a abrir os .rar/.7z, ou os .zip quando o usuário (ou o próprio app) escolhe.
 *
 * Entrega: o processo principal de cada app (o que não tem --type=, para não contar os helpers do Chromium)
 * registra no bootstrap o serviço "com.mos.abrir-url.<CFBundleIdentifier>" ao terminar de abrir
 * (-[NSApplication finishLaunching] → cx_openurl_listen). Uma thread espera mensagens nele (texto embutido, id
 * OU_MSG_ID). Uma URL file://... é um documento: vai para -application:openURLs:, -application:openFiles: ou
 * -application:openFile: do delegate (Electron: 'open-file'), ou para o tratador de 'aevt'/'odoc'. As outras viram
 * o evento 'GURL'/'GURL' para o tratador registrado com -[NSAppleEventManager setEventHandler:...] (Electron:
 * handleURLEvent:withReplyEvent:, que emite 'open-url'); sem tratador, vão para -application:openURLs:.
 *
 * O registro de log mostra só o esquema e o host: a URL de volta do login leva um token.
 */
#include "foundation.h"
#include "sys.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void *objc_autoreleasePoolPush(void);
void objc_autoreleasePoolPop(void *p);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
int strncasecmp(const char *a, const char *b, size_t n);
size_t strlen(const char *s);
char *strchr(const char *s, int c);
void *malloc(size_t n);
void *calloc(size_t n, size_t m);
void *realloc(void *p, size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
char *strdup(const char *s);
extern int NXArgc;
extern char **NXArgv;
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
int pthread_detach(unsigned long t);
typedef void (*dispatch_function_t)(void *);
void dispatch_async_f(void *q, void *ctx, dispatch_function_t fn);
void *dispatch_get_main_queue(void);
int cx_fdprintf(int fd, const char *fmt, ...);
extern u32 bootstrap_port;
int bootstrap_check_in(u32 bp, const char *name, u32 *port);
int mach_msg(void *msg, int option, u32 send_size, u32 rcv_size, u32 rcv_name, u32 timeout, u32 notify);

#define OU_PREFIX  "com.mos.abrir-url."
#define OU_MSG_ID  0x6d4f5355            /* 'mOSU' (o kernel manda com o mesmo id) */
#define OU_MAX_URL 16384
#define MSG_RCV  0x2
#define FOURCC(a, b, c, d) (((u32)(a) << 24) | ((u32)(b) << 16) | ((u32)(c) << 8) | (u32)(d))
#define K_GURL FOURCC('G', 'U', 'R', 'L')   /* kInternetEventClass e kAEGetURL */
#define K_AEVT FOURCC('a', 'e', 'v', 't')   /* kCoreEventClass */
#define K_ODOC FOURCC('o', 'd', 'o', 'c')   /* kAEOpenDocuments */
#define K_DIRECT FOURCC('-', '-', '-', '-') /* keyDirectObject */

typedef struct { u32 bits, size, remote, local, voucher; int id; } ou_hdr_t;
typedef struct { ou_hdr_t h; u32 len; char url[OU_MAX_URL]; u8 trailer[64]; } ou_msg_t;
typedef int OSStatus;
#define kLSApplicationNotFoundErr (-10814)
#define LS_ROLE_ALL_U 0xFFFFFFFFu
void *memchr(const void *s, int c, size_t n);

static BOOL responds(id o, const char *sel) { return o && cx_responds(object_getClass(o), sel_registerName(sel)); }
static int is_arr(id o) { return responds(o, "objectAtIndex:") && responds(o, "count"); }
static id cls(const char *n) { return (id)cx_class_named(n); }

/* ================================================================ o banco do kernel */
static long ls_call(int op, mos_ls_req_t *rq) { return cx_syscall(SYS_LS, op, (long)rq, 0, 0, 0); }

static mos_ls_req_t *rq_new(const char *k, const char *k2, const char *k3) {
    mos_ls_req_t *rq = calloc(1, sizeof *rq);
    if (!rq) return 0;
    if (k) snprintf(rq->key, sizeof rq->key, "%s", k);
    if (k2) snprintf(rq->key2, sizeof rq->key2, "%s", k2);
    if (k3) snprintf(rq->key3, sizeof rq->key3, "%s", k3);
    return rq;
}

/* uma linha "id\tcaminho\tnome" */
typedef struct { char id[128], path[400], name[128]; } ls_line_t;
static int parse_lines(const char *out, ls_line_t *v, int max) {
    int n = 0;
    for (const char *p = out; *p && n < max;) {
        const char *e = strchr(p, '\n');
        size_t l = e ? (size_t)(e - p) : strlen(p);
        const char *t1 = memchr(p, '\t', l), *t2 = t1 ? memchr(t1 + 1, '\t', l - (size_t)(t1 + 1 - p)) : 0;
        if (t1 && t2) {
            snprintf(v[n].id, sizeof v[n].id, "%.*s", (int)(t1 - p), p);
            snprintf(v[n].path, sizeof v[n].path, "%.*s", (int)(t2 - t1 - 1), t1 + 1);
            snprintf(v[n].name, sizeof v[n].name, "%.*s", (int)(l - (size_t)(t2 + 1 - p)), t2 + 1);
            n++;
        }
        p += l + (e ? 1 : 0);
    }
    return n;
}
static int ls_lines(int op, const char *key, u32 roles, ls_line_t *v, int max) {
    mos_ls_req_t *rq = rq_new(key, 0, 0);
    if (!rq) return 0;
    rq->roles = roles;
    long r = ls_call(op, rq);
    int n = r >= 0 ? parse_lines(rq->out, v, max) : 0;
    free(rq);
    return n;
}
static int ls_one(int op, const char *key, u32 roles, ls_line_t *v) { return ls_lines(op, key, roles, v, 1); }
/* resposta de texto (UTI, descrição) em out */
static int ls_text(int op, const char *k, const char *k2, const char *k3, char *out, size_t cap) {
    mos_ls_req_t *rq = rq_new(k, k2, k3);
    if (!rq) return -1;
    long r = ls_call(op, rq);
    if (r >= 0 && out) snprintf(out, cap, "%s", rq->out);
    free(rq);
    return (int)r;
}
static int ls_open_c(const char *target, const char *app, int app_is_path) {
    mos_ls_req_t *rq = rq_new(target ? target : "", app ? app : "", 0);
    if (!rq) return -1;
    rq->flags = app_is_path ? 1 : 0;
    long r = ls_call(MOS_LS_OPEN, rq);
    free(rq);
    return r == 0 ? 0 : -1;
}

/* SPI para o resto do libSystem (UTType, vscode_frameworks.c) */
int cx_ls_conforms(const char *a, const char *b) {
    if (!a || !b) return 0;
    mos_ls_req_t *rq = rq_new(a, b, 0);
    if (!rq) return 0;
    long r = ls_call(MOS_LS_CONFORMS, rq);
    free(rq);
    return r == 1;
}
int cx_ls_type_for_tag(const char *cls_, const char *tag, const char *conf, char *out, size_t cap) {
    return ls_text(MOS_LS_TYPE_FOR_TAG, tag, cls_, conf, out, cap) >= 0;
}
int cx_ls_tag_for_type(const char *uti, const char *cls_, char *out, size_t cap) {
    char all[1024];
    if (ls_text(MOS_LS_TAG_FOR_TYPE, uti, cls_, 0, all, sizeof all) <= 0) return 0;
    char *nl = strchr(all, '\n');
    if (nl) *nl = 0;
    snprintf(out, cap, "%s", all);
    return out[0] != 0;
}
int cx_ls_type_info(const char *uti, char *desc, size_t dcap, char *parents, size_t pcap) {
    char out[2048];
    if (ls_text(MOS_LS_TYPE_INFO, uti, 0, 0, out, sizeof out) < 0) return 0;
    char *nl = strchr(out, '\n');
    if (nl) *nl = 0;
    if (desc) snprintf(desc, dcap, "%s", out);
    if (parents) snprintf(parents, pcap, "%s", nl ? nl + 1 : "");
    return 1;
}
int cx_ls_type_of_file(const char *path, char *out, size_t cap) { return ls_text(MOS_LS_TYPE_OF_FILE, path, 0, 0, out, cap) >= 0; }

/* ================================================================ texto, URLs e caminhos */
static void lower_copy(char *d, size_t cap, const char *s) {
    size_t i = 0;
    for (; s[i] && i + 1 < cap; i++) d[i] = (s[i] >= 'A' && s[i] <= 'Z') ? (char)(s[i] + 32) : s[i];
    d[i] = 0;
}
static int url_scheme(const char *u, char *out, size_t cap) {
    if (!u || !((u[0] >= 'a' && u[0] <= 'z') || (u[0] >= 'A' && u[0] <= 'Z'))) return 0;
    size_t i = 0;
    for (; u[i] && u[i] != ':'; i++) {
        char ch = u[i];
        int ok = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '+' || ch == '-' || ch == '.';
        if (!ok || i + 1 >= cap) return 0;
        out[i] = (ch >= 'A' && ch <= 'Z') ? (char)(ch + 32) : ch;
    }
    if (u[i] != ':' || i < 2) return 0;
    out[i] = 0;
    return 1;
}
static void redact(const char *url, char *out, size_t cap) {
    size_t o = 0, i = 0;
    const char *sep = strstr(url, "://");
    size_t stop = sep ? (size_t)(sep - url) + 3 : 0;
    int file = !strncmp(url, "file:", 5);
    for (; url[i] && o + 1 < cap; i++) {
        if (i >= stop && (url[i] == '?' || url[i] == '#' || (!file && url[i] == '/'))) break;
        out[o++] = url[i];
    }
    out[o] = 0;
    if (url[i] && o + 5 < cap) { out[o++] = '/'; out[o++] = (char)0xe2; out[o++] = (char)0x80; out[o++] = (char)0xa6; out[o] = 0; }
}
static void ou_log(const char *what, const char *url, const char *who) {
    char shown[200];
    redact(url ? url : "", shown, sizeof shown);
    cx_fdprintf(2, "[abrir-url] %s %s%s%s\n", what, shown, who ? " -> " : "", who ? who : "");
}
static const char *url_c(id u) {
    if (!u) return 0;
    id s = cx_is_str(u) ? u : (responds(u, "absoluteString") ? MSG0(id, u, "absoluteString") : 0);
    return s && cx_is_str(s) ? cx_cstr(s) : 0;
}
/* caminho de um NSURL de arquivo ou NSString */
static const char *path_c(id u) {
    if (!u) return 0;
    if (cx_is_str(u)) return cx_cstr(u);
    id p = responds(u, "path") ? MSG0(id, u, "path") : 0;
    return p && cx_is_str(p) ? cx_cstr(p) : 0;
}
static id file_url(const char *path) { return MSG(id, cls("NSURL"), "fileURLWithPath:", cx_str(path)); }
static id app_url(const ls_line_t *l) {
    if (l->path[0]) return file_url(l->path);
    char p[300];   /* app do sistema: um caminho no estilo do macOS, para quem só olha o nome */
    snprintf(p, sizeof p, "/System/Applications/%s.app", l->name);
    return file_url(p);
}
static id ls_error(void) {
    return MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:", cx_str("NSOSStatusErrorDomain"), (i64)kLSApplicationNotFoundErr, (id)0);
}
typedef struct { void *isa; int flags, reserved; void *invoke; } ou_block_t;
static void call_done(void *done, BOOL ok) {
    if (done) ((void (*)(void *, id, id))((ou_block_t *)done)->invoke)(done, (id)0, ok ? (id)0 : ls_error());
}
static void call_done_err(void *done, BOOL ok) {
    if (done) ((void (*)(void *, id))((ou_block_t *)done)->invoke)(done, ok ? (id)0 : ls_error());
}

/* .app indicado por NSURL/caminho, identificador ou nome → identificador no banco (registra o .app se preciso) */
static int app_from_path(const char *p, ls_line_t *l) { return p && ls_one(MOS_LS_APP_BY_PATH, p, 0, l) > 0; }
static int app_from_obj(id app, ls_line_t *l) {
    const char *p = path_c(app);
    if (!p) return 0;
    if (app_from_path(p, l)) return 1;
    /* "/System/Applications/Editor de Texto.app" de um app do sistema */
    const char *b = strrchr(p, '/');
    char nm[160];
    snprintf(nm, sizeof nm, "%s", b ? b + 1 : p);
    return ls_one(MOS_LS_APP_BY_NAME, nm, 0, l) > 0;
}

static int open_url_c(const char *url) {
    if (!url) return -1;
    int r = ls_open_c(url, 0, 0);
    ou_log(r == 0 ? "aberta" : "ninguém atende", url, 0);
    return r;
}

/* SPI do mOS para o /usr/bin/open: alvo = URL, caminho (.app ou arquivo) ou 0; app = -a <nome>/-b <id>. 0 = ok */
int cx_open_target(const char *target, const char *app, int by_ident, int timeout_ms) {
    (void)timeout_ms;
    ls_line_t a;
    int have = 0;
    if (app && app[0]) {
        if (app[0] == '/') have = app_from_path(app, &a);
        else have = ls_one(by_ident ? MOS_LS_APP_BY_ID : MOS_LS_APP_BY_NAME, app, 0, &a) > 0;
        if (!have) { cx_fdprintf(2, "Unable to find application named '%s'\n", app); return -1; }
    }
    if (!target) return ls_open_c("", have ? a.id : 0, 0);
    char sch[48], abs[1024];
    if (target[0] != '/' && url_scheme(target, sch, sizeof sch)) return ls_open_c(target, have ? a.id : 0, 0);
    if (target[0] != '/') {
        char cwd[800];
        if (!getcwd(cwd, sizeof cwd)) cwd[0] = 0;
        snprintf(abs, sizeof abs, "%s/%s", cwd, target);
        target = abs;
    }
    if (access(target, 0) != 0) { cx_fdprintf(2, "The file %s does not exist.\n", target); return -1; }
    int r = ls_open_c(target, have ? a.id : 0, 0);
    if (r != 0 && !have) cx_fdprintf(2, "No application knows how to open %s.\n", target);
    return r;
}

/* ================================================================ NSWorkspace */
static BOOL ws_open_url(id self, SEL c, id u) { (void)self; (void)c; return open_url_c(url_c(u)) == 0; }
static void ws_open_url_cfg(id self, SEL c, id u, id cfg, void *done) { (void)self; (void)c; (void)cfg; call_done(done, open_url_c(url_c(u)) == 0); }
static BOOL ws_open_url_opts(id self, SEL c, id u, u64 opts, id cfg, id *err) {
    (void)self; (void)c; (void)opts; (void)cfg;
    BOOL ok = open_url_c(url_c(u)) == 0;
    if (!ok && err) *err = ls_error();
    return ok;
}
static BOOL open_urls_with(id urls, const ls_line_t *a) {
    u64 n = is_arr(urls) ? MSG0(u64, urls, "count") : 0;
    BOOL ok = n > 0;
    for (u64 i = 0; i < n; i++) {
        const char *u = url_c(MSG(id, urls, "objectAtIndex:", i));
        if (!u || ls_open_c(u, a ? a->id : 0, 0) != 0) ok = NO;
    }
    if (!n && a) ok = ls_open_c("", a->id, 0) == 0;
    return ok;
}
/* vscode_frameworks.c: LSOpenFromURLSpec */
int cx_ls_open_urls(id app, id urls) {
    ls_line_t a;
    if (app && !app_from_obj(app, &a)) return kLSApplicationNotFoundErr;
    return open_urls_with(urls, app ? &a : 0) ? 0 : kLSApplicationNotFoundErr;
}
static BOOL ws_open_urls_bundle(id self, SEL c, id urls, id ident, u64 opts, id desc, id *launch_ids) {
    (void)self; (void)c; (void)opts; (void)desc;
    if (launch_ids) *launch_ids = 0;
    ls_line_t a;
    const char *b = ident && cx_is_str(ident) ? cx_cstr(ident) : 0;
    if (b && ls_one(MOS_LS_APP_BY_ID, b, 0, &a) <= 0) return NO;
    return open_urls_with(urls, b ? &a : 0);
}
static void ws_open_urls_app(id self, SEL c, id urls, id app, id cfg, void *done) {
    (void)self; (void)c; (void)cfg;
    ls_line_t a;
    call_done(done, app_from_obj(app, &a) && open_urls_with(urls, &a));
}
static void ws_open_app_url(id self, SEL c, id app, id cfg, void *done) {
    (void)self; (void)c; (void)cfg;
    ls_line_t a;
    call_done(done, app_from_obj(app, &a) && ls_open_c("", a.id, 0) == 0);
}
static id ws_launch_app_url(id self, SEL c, id app, u64 opts, id cfg, id *err) {
    (void)self; (void)c; (void)opts; (void)cfg;
    ls_line_t a;
    if (app_from_obj(app, &a) && ls_open_c("", a.id, 0) == 0) return MSG0(id, cls("NSRunningApplication"), "currentApplication");
    if (err) *err = ls_error();
    return 0;
}
static BOOL ws_launch_name(id self, SEL c, id name) {
    (void)self; (void)c;
    ls_line_t a;
    const char *n = name && cx_is_str(name) ? cx_cstr(name) : 0;
    if (!n) return NO;
    if (n[0] == '/') return app_from_path(n, &a) && ls_open_c("", a.id, 0) == 0;
    return ls_one(MOS_LS_APP_BY_NAME, n, 0, &a) > 0 && ls_open_c("", a.id, 0) == 0;
}
static BOOL ws_launch_bundle(id self, SEL c, id ident, u64 opts, id desc, id *lid) {
    (void)self; (void)c; (void)opts; (void)desc;
    if (lid) *lid = 0;
    ls_line_t a;
    return ident && cx_is_str(ident) && ls_one(MOS_LS_APP_BY_ID, cx_cstr(ident), 0, &a) > 0 && ls_open_c("", a.id, 0) == 0;
}
/* -openFile:, -openFile:withApplication:, -openFile:withApplication:andDeactivate: */
static BOOL ws_open_file(id self, SEL c, id path) { (void)self; (void)c; const char *p = path_c(path); return p && ls_open_c(p, 0, 0) == 0; }
static BOOL ws_open_file_app(id self, SEL c, id path, id app) {
    (void)self; (void)c;
    const char *p = path_c(path), *an = app && cx_is_str(app) ? cx_cstr(app) : 0;
    if (!p) return NO;
    if (!an) return ls_open_c(p, 0, 0) == 0;
    ls_line_t a;
    if (an[0] == '/' ? !app_from_path(an, &a) : ls_one(MOS_LS_APP_BY_NAME, an, 0, &a) <= 0) return NO;
    return ls_open_c(p, a.id, 0) == 0;
}
static BOOL ws_open_file_app_deact(id self, SEL c, id path, id app, BOOL d) { (void)d; return ws_open_file_app(self, c, path, app); }

/* quem abre: URL de arquivo pelo tipo do arquivo; outras pelo esquema */
static int handlers_for_url(const char *s, ls_line_t *v, int max, int only_default) {
    char sch[48];
    if (!s || !url_scheme(s, sch, sizeof sch)) {
        if (s && s[0] == '/') return ls_lines(only_default ? MOS_LS_DEFAULT_FILE : MOS_LS_ALL_FILE, s, 0, v, max);
        return 0;
    }
    if (!strcmp(sch, "file")) {
        id u = MSG(id, cls("NSURL"), "URLWithString:", cx_str(s));
        const char *p = path_c(u);
        return p ? ls_lines(only_default ? MOS_LS_DEFAULT_FILE : MOS_LS_ALL_FILE, p, 0, v, max) : 0;
    }
    return ls_lines(only_default ? MOS_LS_DEFAULT_SCHEME : MOS_LS_ALL_SCHEME, sch, 0, v, max);
}
static id lines_to_urls(ls_line_t *v, int n) {
    id arr = cx_arr_new(cx_class_named("NSMutableArray"));
    for (int i = 0; i < n; i++) cx_arr_add(arr, app_url(&v[i]));
    return arr;
}
static id ws_app_for_url(id self, SEL c, id u) {
    (void)self; (void)c;
    ls_line_t a;
    return handlers_for_url(url_c(u), &a, 1, 1) ? app_url(&a) : 0;
}
static id ws_apps_for_url(id self, SEL c, id u) {
    (void)self; (void)c;
    ls_line_t v[32];
    return lines_to_urls(v, handlers_for_url(url_c(u), v, 32, 0));
}
/* UTType (objeto) ou NSString com o identificador */
static const char *uti_of(id t) {
    if (!t) return 0;
    if (cx_is_str(t)) return cx_cstr(t);
    id s = responds(t, "identifier") ? MSG0(id, t, "identifier") : 0;
    return s && cx_is_str(s) ? cx_cstr(s) : 0;
}
static id ws_app_for_type(id self, SEL c, id t) {
    (void)self; (void)c;
    ls_line_t a;
    const char *u = uti_of(t);
    return u && ls_one(MOS_LS_DEFAULT_TYPE, u, 0, &a) > 0 ? app_url(&a) : 0;
}
static id ws_apps_for_type(id self, SEL c, id t) {
    (void)self; (void)c;
    ls_line_t v[32];
    const char *u = uti_of(t);
    return lines_to_urls(v, u ? ls_lines(MOS_LS_ALL_TYPE, u, 0, v, 32) : 0);
}
static id ws_apps_for_ident(id self, SEL c, id ident) {
    (void)self; (void)c;
    ls_line_t a;
    int n = ident && cx_is_str(ident) ? ls_one(MOS_LS_APP_BY_ID, cx_cstr(ident), 0, &a) : 0;
    return lines_to_urls(&a, n);
}
static id ws_app_for_ident(id self, SEL c, id ident) {
    (void)self; (void)c;
    ls_line_t a;
    return ident && cx_is_str(ident) && ls_one(MOS_LS_APP_BY_ID, cx_cstr(ident), 0, &a) > 0 ? app_url(&a) : 0;
}
static id ws_path_for_ident(id self, SEL c, id ident) {
    id u = ws_app_for_ident(self, c, ident);
    return u ? MSG0(id, u, "path") : 0;
}
static id ws_path_for_name(id self, SEL c, id name) {
    (void)self; (void)c;
    ls_line_t a;
    if (!name || !cx_is_str(name) || ls_one(MOS_LS_APP_BY_NAME, cx_cstr(name), 0, &a) <= 0) return 0;
    return MSG0(id, app_url(&a), "path");
}
/* -setDefaultApplicationAtURL:toOpenURLsWithScheme:completionHandler: (macOS 12): para http/https o sistema pergunta */
static void ws_set_default_scheme(id self, SEL c, id app, id scheme, void *done) {
    (void)self; (void)c;
    ls_line_t a;
    BOOL ok = NO;
    if (app_from_obj(app, &a) && scheme && cx_is_str(scheme)) {
        mos_ls_req_t *rq = rq_new(cx_cstr(scheme), a.id, 0);
        ok = rq && ls_call(MOS_LS_SET_SCHEME, rq) == 0;
        free(rq);
    }
    call_done_err(done, ok);
}
static BOOL set_type_default(const char *uti, const char *ident, u32 roles) {
    mos_ls_req_t *rq = rq_new(uti, ident, 0);
    if (!rq) return NO;
    rq->roles = roles;
    BOOL ok = ls_call(MOS_LS_SET_TYPE, rq) == 0;
    free(rq);
    return ok;
}
static void ws_set_default_type(id self, SEL c, id app, id type, void *done) {
    (void)self; (void)c;
    ls_line_t a;
    const char *u = uti_of(type);
    call_done_err(done, u && app_from_obj(app, &a) && set_type_default(u, a.id, LS_ROLE_ALL_U));
}
static void ws_set_default_file(id self, SEL c, id app, id file, void *done) {
    (void)self; (void)c;
    ls_line_t a;
    char uti[256];
    const char *p = path_c(file);
    call_done_err(done, p && cx_ls_type_of_file(p, uti, sizeof uti) && app_from_obj(app, &a) && set_type_default(uti, a.id, LS_ROLE_ALL_U));
}
/* -typeOfFile:error: (o UTI como NSString) e as consultas de tipo da NSWorkspace */
static id ws_type_of_file(id self, SEL c, id path, id *err) {
    (void)self; (void)c;
    char uti[256];
    const char *p = path_c(path);
    if (p && cx_ls_type_of_file(p, uti, sizeof uti)) { if (err) *err = 0; return cx_str(uti); }
    if (err) *err = MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:", cx_str("NSCocoaErrorDomain"), (i64)260, (id)0);
    return 0;
}
static id ws_type_desc(id self, SEL c, id t) {
    (void)self; (void)c;
    char d[256];
    const char *u = uti_of(t);
    return u && cx_ls_type_info(u, d, sizeof d, 0, 0) ? cx_str(d) : 0;
}
static BOOL ws_type_conforms(id self, SEL c, id a, id b) { (void)self; (void)c; return cx_ls_conforms(uti_of(a), uti_of(b)); }
static id ws_pref_ext(id self, SEL c, id t) {
    (void)self; (void)c;
    char e[64];
    const char *u = uti_of(t);
    return u && cx_ls_tag_for_type(u, "public.filename-extension", e, sizeof e) ? cx_str(e) : 0;
}
static BOOL ws_ext_valid(id self, SEL c, id ext, id t) {
    (void)self; (void)c;
    char u[256];
    const char *e = ext && cx_is_str(ext) ? cx_cstr(ext) : 0, *want = uti_of(t);
    return e && want && cx_ls_type_for_tag("public.filename-extension", e, 0, u, sizeof u) && cx_ls_conforms(u, want);
}
static BOOL ws_reveal(id self, SEL c, id path, id root) {
    (void)self; (void)c; (void)root;
    const char *p = path_c(path);
    if (!p) return NO;
    char dir[1024];
    snprintf(dir, sizeof dir, "%s", p);
    char *s = strrchr(dir, '/');
    if (s && s != dir) *s = 0;
    return ls_open_c(dir, "com.apple.finder", 0) == 0;
}
static void ws_activate_reveal(id self, SEL c, id urls) {
    u64 n = is_arr(urls) ? MSG0(u64, urls, "count") : 0;
    if (n) ws_reveal(self, c, MSG(id, urls, "objectAtIndex:", (u64)0), 0);
}
/* -[NSFileManager displayNameAtPath:]: o nome do item sem ".app" (o Chrome mostra "Abrir o Slack?") */
static id fm_display_name(id self, SEL c, id path) {
    (void)self; (void)c;
    const char *p = path && cx_is_str(path) ? cx_cstr(path) : "";
    size_t l = strlen(p);
    while (l > 1 && p[l - 1] == '/') l--;
    size_t b = l;
    while (b > 0 && p[b - 1] != '/') b--;
    size_t n = l - b;
    if (n > 4 && !strncasecmp(p + l - 4, ".app", 4)) n -= 4;
    return cx_str_n(p + b, n);
}

/* ================================================================ LaunchServices (C) */
static id cf_str(const char *s) { return objc_retain(cx_str(s)); }
static id one_or_all_urls(int op, const char *key, u32 roles, int all) {
    ls_line_t v[32];
    int n = ls_lines(op, key, roles, v, all ? 32 : 1);
    if (!n) return 0;
    return all ? objc_retain(lines_to_urls(v, n)) : objc_retain(app_url(&v[0]));
}
id LSCopyDefaultApplicationURLForURL(id url, u32 roles, id *err) {
    ls_line_t a;
    (void)roles;
    if (!handlers_for_url(url_c(url), &a, 1, 1)) { if (err) *err = objc_retain(ls_error()); return 0; }
    if (err) *err = 0;
    return objc_retain(app_url(&a));
}
id LSCopyApplicationURLsForURL(id url, u32 roles) {
    ls_line_t v[32];
    (void)roles;
    int n = handlers_for_url(url_c(url), v, 32, 0);
    return n ? objc_retain(lines_to_urls(v, n)) : 0;
}
id LSCopyDefaultApplicationURLForContentType(id type, u32 roles, id *err) {
    const char *u = uti_of(type);
    id r = u ? one_or_all_urls(MOS_LS_DEFAULT_TYPE, u, roles, 0) : 0;
    if (err) *err = r ? 0 : objc_retain(ls_error());
    return r;
}
id LSCopyApplicationURLsForBundleIdentifier(id ident, id *err) {
    const char *i = ident && cx_is_str(ident) ? cx_cstr(ident) : 0;
    id r = i ? one_or_all_urls(MOS_LS_APP_BY_ID, i, 0, 1) : 0;
    if (err) *err = r ? 0 : objc_retain(ls_error());
    return r;
}
id LSCopyDefaultHandlerForURLScheme(id scheme) {
    ls_line_t a;
    char sch[48];
    if (!scheme || !cx_is_str(scheme)) return 0;
    lower_copy(sch, sizeof sch, cx_cstr(scheme));
    return ls_one(MOS_LS_DEFAULT_SCHEME, sch, 0, &a) > 0 ? cf_str(a.id) : 0;
}
id LSCopyAllHandlersForURLScheme(id scheme) {
    ls_line_t v[32];
    char sch[48];
    if (!scheme || !cx_is_str(scheme)) return 0;
    lower_copy(sch, sizeof sch, cx_cstr(scheme));
    int n = ls_lines(MOS_LS_ALL_SCHEME, sch, 0, v, 32);
    if (!n) return 0;
    id arr = cx_arr_new(cx_class_named("NSMutableArray"));
    for (int i = 0; i < n; i++) cx_arr_add(arr, cx_str(v[i].id));
    return objc_retain(arr);
}
OSStatus LSSetDefaultHandlerForURLScheme(id scheme, id ident) {
    if (!scheme || !ident || !cx_is_str(scheme) || !cx_is_str(ident)) return -50;
    mos_ls_req_t *rq = rq_new(cx_cstr(scheme), cx_cstr(ident), 0);
    long r = rq ? ls_call(MOS_LS_SET_SCHEME, rq) : -1;
    free(rq);
    return r == 0 ? 0 : kLSApplicationNotFoundErr;
}
id LSCopyDefaultRoleHandlerForContentType(id type, u32 roles) {
    ls_line_t a;
    const char *u = uti_of(type);
    return u && ls_one(MOS_LS_DEFAULT_TYPE, u, roles, &a) > 0 ? cf_str(a.id) : 0;
}
id LSCopyAllRoleHandlersForContentType(id type, u32 roles) {
    ls_line_t v[32];
    const char *u = uti_of(type);
    int n = u ? ls_lines(MOS_LS_ALL_TYPE, u, roles, v, 32) : 0;
    if (!n) return 0;
    id arr = cx_arr_new(cx_class_named("NSMutableArray"));
    for (int i = 0; i < n; i++) cx_arr_add(arr, cx_str(v[i].id));
    return objc_retain(arr);
}
OSStatus LSSetDefaultRoleHandlerForContentType(id type, u32 roles, id ident) {
    const char *u = uti_of(type);
    if (!u || !ident || !cx_is_str(ident)) return -50;
    return set_type_default(u, cx_cstr(ident), roles) ? 0 : kLSApplicationNotFoundErr;
}
OSStatus LSRegisterURL(id url, u8 update) {
    (void)update;
    ls_line_t a;
    return app_from_path(path_c(url), &a) ? 0 : kLSApplicationNotFoundErr;
}
OSStatus LSOpenCFURLRef(id url, id *launched) {
    if (launched) *launched = 0;
    const char *s = url_c(url);
    if (!s) return -50;
    if (!strncmp(s, "file:", 5)) { const char *p = path_c(url); return p && ls_open_c(p, 0, 0) == 0 ? 0 : kLSApplicationNotFoundErr; }
    return open_url_c(s) == 0 ? 0 : kLSApplicationNotFoundErr;
}
OSStatus LSGetApplicationForURL(id url, u32 roles, void *fsref, id *app) {
    (void)fsref;
    id r = LSCopyDefaultApplicationURLForURL(url, roles, 0);
    if (app) *app = r; else if (r) objc_release(r);
    return r ? 0 : kLSApplicationNotFoundErr;
}
OSStatus LSCanURLAcceptURL(id target, id app, u32 roles, u32 flags, u8 *accepts) {
    (void)roles; (void)flags;
    ls_line_t v[32], a;
    *accepts = 0;
    if (!app_from_obj(app, &a)) return kLSApplicationNotFoundErr;
    int n = handlers_for_url(url_c(target), v, 32, 0);
    for (int i = 0; i < n; i++) if (!strcmp(v[i].id, a.id)) *accepts = 1;
    return 0;
}

/* ================================================================ UTType em C (CoreServices) */
id kUTTagClassFilenameExtension, kUTTagClassMIMEType;
id UTTypeCreatePreferredIdentifierForTag(id cls_, id tag, id conf) {
    char out[256];
    const char *t = tag && cx_is_str(tag) ? cx_cstr(tag) : 0, *c = cls_ && cx_is_str(cls_) ? cx_cstr(cls_) : 0;
    if (!t || !cx_ls_type_for_tag(c, t, conf && cx_is_str(conf) ? cx_cstr(conf) : 0, out, sizeof out)) return 0;
    return cf_str(out);
}
id UTTypeCreateAllIdentifiersForTag(id cls_, id tag, id conf) {
    id one = UTTypeCreatePreferredIdentifierForTag(cls_, tag, conf);
    if (!one) return 0;
    id arr = objc_retain(MSG(id, cls("NSArray"), "arrayWithObject:", one));
    objc_release(one);
    return arr;
}
id UTTypeCopyPreferredTagWithClass(id uti, id cls_) {
    char out[256];
    const char *u = uti && cx_is_str(uti) ? cx_cstr(uti) : 0;
    return u && cx_ls_tag_for_type(u, cls_ && cx_is_str(cls_) ? cx_cstr(cls_) : 0, out, sizeof out) ? cf_str(out) : 0;
}
id UTTypeCopyDescription(id uti) {
    char d[256];
    const char *u = uti && cx_is_str(uti) ? cx_cstr(uti) : 0;
    return u && cx_ls_type_info(u, d, sizeof d, 0, 0) ? cf_str(d) : 0;
}
u8 UTTypeEqual(id a, id b) { return a && b && cx_is_str(a) && cx_is_str(b) && !strcasecmp(cx_cstr(a), cx_cstr(b)); }
u8 UTTypeIsDynamic(id uti) { return uti && cx_is_str(uti) && !strncmp(cx_cstr(uti), "dyn.", 4); }
u8 UTTypeIsDeclared(id uti) { return uti && cx_is_str(uti) && cx_ls_type_info(cx_cstr(uti), 0, 0, 0, 0) && !UTTypeIsDynamic(uti); }

/* ================================================================ recepção: Apple Events */
typedef struct { u32 cls, ev; id h; SEL s; } aeh_t;
static aeh_t g_aeh[16];
static int g_naeh;
static id g_cur_event;
static cx_class_t URLEvent_c, URLEvent_m;
static u32 g_ev_off;                          /* onde fica a URL (NSString) na instância */

static void aem_set(id self, SEL c, id h, SEL s, u32 cls, u32 ev) {
    (void)self; (void)c;
    for (int i = 0; i < g_naeh; i++)
        if (g_aeh[i].cls == cls && g_aeh[i].ev == ev) { g_aeh[i].h = h; g_aeh[i].s = s; return; }
    if (g_naeh < (int)(sizeof g_aeh / sizeof g_aeh[0])) g_aeh[g_naeh++] = (aeh_t){cls, ev, h, s};
}
static void aem_remove(id self, SEL c, u32 cls, u32 ev) {
    (void)self; (void)c;
    for (int i = 0; i < g_naeh; i++)
        if (g_aeh[i].cls == cls && g_aeh[i].ev == ev) { g_aeh[i] = g_aeh[--g_naeh]; return; }
}
static id aem_current(id self, SEL c) { (void)self; (void)c; return g_cur_event; }

static id *ev_slot(id e) { return (id *)((u8 *)e + g_ev_off); }
static id ev_param(id self, SEL c, u32 kw) {
    (void)c;
    if (kw != K_DIRECT || !*ev_slot(self)) return 0;
    return MSG(id, (id)cx_class_named("NSAppleEventDescriptor"), "descriptorWithString:", *ev_slot(self));
}
static u32 ev_gurl(id self, SEL c) { (void)self; (void)c; return K_GURL; }
static u32 ev_type(id self, SEL c) { (void)self; (void)c; return FOURCC('a', 'e', 'v', 't'); }
static id ev_nil(id self, SEL c, u32 kw) { (void)self; (void)c; (void)kw; return 0; }
static i64 ev_count(id self, SEL c) { (void)self; (void)c; return 1; }
static id ev_str(id self, SEL c) { (void)self; (void)c; return 0; }
static void ev_dealloc(id self, SEL c) {
    (void)c;
    if (*ev_slot(self)) objc_release(*ev_slot(self));
    void (*sup)(id, SEL) = (void (*)(id, SEL))cx_lookup(cx_class_named("NSAppleEventDescriptor"), sel_registerName("dealloc"));
    if (sup) sup(self, sel_registerName("dealloc"));
}


/* -[NSApplication delegate] recebendo documentos: o caminho moderno (openURLs:), o de vários (openFiles:) e o antigo */
static int deliver_docs(id url_obj_, const char *path) {
    id app = MSG0(id, cls("NSApplication"), "sharedApplication");
    id d = app ? MSG0(id, app, "delegate") : 0;
    if (d && responds(d, "application:openURLs:")) {
        { id one = MSG(id, cls("NSArray"), "arrayWithObject:", url_obj_); MSG(void, d, "application:openURLs:", app, one); }
        return 1;
    }
    if (d && path && responds(d, "application:openFiles:")) {
        { id one = MSG(id, cls("NSArray"), "arrayWithObject:", cx_str(path)); MSG(void, d, "application:openFiles:", app, one); }
        return 1;
    }
    if (d && path && responds(d, "application:openFile:")) { MSG(BOOL, d, "application:openFile:", app, cx_str(path)); return 1; }
    /* apps de documentos sem delegate que trate: o NSDocumentController abre */
    id dc = cx_class_named("NSDocumentController") ? MSG0(id, cls("NSDocumentController"), "sharedDocumentController") : 0;
    if (dc && responds(dc, "openDocumentWithContentsOfURL:display:completionHandler:")) {
        MSG(void, dc, "openDocumentWithContentsOfURL:display:completionHandler:", url_obj_, (BOOL)YES, (void *)0);
        return 1;
    }
    return 0;
}

static int deliver_event(u32 cls_, u32 ev, id payload) {
    for (int i = 0; i < g_naeh; i++) {
        if (g_aeh[i].cls != cls_ || g_aeh[i].ev != ev || !g_aeh[i].h) continue;
        id e = cx_alloc_instance(&URLEvent_c);
        *ev_slot(e) = objc_retain(payload);
        id prev = g_cur_event;
        g_cur_event = e;
        ((void (*)(id, SEL, id, id))objc_msgSend)(g_aeh[i].h, g_aeh[i].s, e, (id)0);
        g_cur_event = prev;
        objc_release(e);
        return 1;
    }
    return 0;
}

/* na thread principal: a URL ou o documento chega ao app como no macOS */
static void deliver_main(void *ctx) {
    char *url = ctx;
    void *pool = objc_autoreleasePoolPush();
    id u = MSG(id, cls("NSURL"), "URLWithString:", cx_str(url));
    int done = 0, doc = !strncmp(url, "file:", 5);
    if (doc) {
        done = deliver_docs(u, path_c(u));
        if (!done) done = deliver_event(K_AEVT, K_ODOC, cx_str(url));
    } else {
        done = deliver_event(K_GURL, K_GURL, cx_str(url));
        if (!done) {
            id app = MSG0(id, cls("NSApplication"), "sharedApplication");
            id d = app ? MSG0(id, app, "delegate") : 0;
            if (d && u && responds(d, "application:openURLs:")) {
                { id one = MSG(id, cls("NSArray"), "arrayWithObject:", u); MSG(void, d, "application:openURLs:", app, one); }
                done = 1;
            }
        }
    }
    ou_log(done ? (doc ? "documento recebido" : "recebida") : "recebida sem tratador no app", url, 0);
    objc_autoreleasePoolPop(pool);
    free(url);
}

static void *listen_thread(void *arg) {
    u32 port = (u32)(u64)arg;
    ou_msg_t *m = malloc(sizeof *m);
    if (!m) return 0;
    for (;;) {
        memset(&m->h, 0, sizeof m->h);
        int r = mach_msg(&m->h, MSG_RCV, 0, (u32)sizeof *m, port, 0, 0);
        if (r != 0) { usleep(200000); continue; }
        if (m->h.id != OU_MSG_ID || m->len >= OU_MAX_URL || m->h.size < sizeof(ou_hdr_t) + 4 + m->len) continue;
        char *s = malloc(m->len + 1);
        if (!s) continue;
        memcpy(s, m->url, m->len);
        s[m->len] = 0;
        dispatch_async_f(dispatch_get_main_queue(), s, deliver_main);
    }
    return 0;
}

/* -[NSApplication finishLaunching] (appkit.c): o processo principal do app passa a receber URLs e documentos */
void cx_openurl_listen(void) {
    static int started;
    if (started) return;
    for (int i = 1; NXArgv && i < NXArgc; i++) if (NXArgv[i] && !strncmp(NXArgv[i], "--type=", 7)) return;   /* helper do Chromium */
    id mb = MSG0(id, cls("NSBundle"), "mainBundle");
    id ident = mb ? MSG0(id, mb, "bundleIdentifier") : 0;
    if (!ident || !cx_is_str(ident) || !cx_cstr(ident)[0]) return;
    started = 1;
    char svc[200];
    snprintf(svc, sizeof svc, OU_PREFIX "%s", cx_cstr(ident));
    u32 port = 0;
    int r = bootstrap_check_in(bootstrap_port, svc, &port);
    if (r != 0 || !port) { cx_fdprintf(2, "[abrir-url] serviço %s indisponível (%d)\n", svc, r); return; }
    unsigned long t;
    if (pthread_create(&t, 0, listen_thread, (void *)(u64)port) == 0) pthread_detach(t);
}

void cx_openurl_init(void) {
    kUTTagClassFilenameExtension = cx_str("public.filename-extension");
    kUTTagClassMIMEType = cx_str("public.mime-type");
    cx_class_t *ws = cx_class_named("NSWorkspace");
    if (ws) {
        static const struct { const char *sel; void *fn; } m[] = {
            {"openURL:", (void *)ws_open_url},
            {"openURL:configuration:completionHandler:", (void *)ws_open_url_cfg},
            {"openURL:options:configuration:error:", (void *)ws_open_url_opts},
            {"openURLs:withAppBundleIdentifier:options:additionalEventParamDescriptor:launchIdentifiers:", (void *)ws_open_urls_bundle},
            {"openURLs:withApplicationAtURL:configuration:completionHandler:", (void *)ws_open_urls_app},
            {"openApplicationAtURL:configuration:completionHandler:", (void *)ws_open_app_url},
            {"launchApplicationAtURL:options:configuration:error:", (void *)ws_launch_app_url},
            {"launchApplication:", (void *)ws_launch_name},
            {"launchAppWithBundleIdentifier:options:additionalEventParamDescriptor:launchIdentifier:", (void *)ws_launch_bundle},
            {"openFile:", (void *)ws_open_file},
            {"openFile:withApplication:", (void *)ws_open_file_app},
            {"openFile:withApplication:andDeactivate:", (void *)ws_open_file_app_deact},
            {"URLForApplicationToOpenURL:", (void *)ws_app_for_url},
            {"URLsForApplicationsToOpenURL:", (void *)ws_apps_for_url},
            {"URLForApplicationToOpenContentType:", (void *)ws_app_for_type},
            {"URLsForApplicationsToOpenContentType:", (void *)ws_apps_for_type},
            {"URLForApplicationWithBundleIdentifier:", (void *)ws_app_for_ident},
            {"URLsForApplicationsWithBundleIdentifier:", (void *)ws_apps_for_ident},
            {"absolutePathForAppBundleWithIdentifier:", (void *)ws_path_for_ident},
            {"fullPathForApplication:", (void *)ws_path_for_name},
            {"setDefaultApplicationAtURL:toOpenURLsWithScheme:completionHandler:", (void *)ws_set_default_scheme},
            {"setDefaultApplicationAtURL:toOpenContentType:completionHandler:", (void *)ws_set_default_type},
            {"setDefaultApplicationAtURL:toOpenContentTypeOfFileAtURL:completionHandler:", (void *)ws_set_default_file},
            {"setDefaultApplicationAtURL:toOpenFileAtURL:completionHandler:", (void *)ws_set_default_file},
            {"typeOfFile:error:", (void *)ws_type_of_file},
            {"localizedDescriptionForType:", (void *)ws_type_desc},
            {"type:conformsToType:", (void *)ws_type_conforms},
            {"preferredFilenameExtensionForType:", (void *)ws_pref_ext},
            {"filenameExtension:isValidForType:", (void *)ws_ext_valid},
            {"selectFile:inFileViewerRootedAtPath:", (void *)ws_reveal},
            {"activateFileViewerSelectingURLs:", (void *)ws_activate_reveal},
        };
        for (unsigned i = 0; i < sizeof m / sizeof m[0]; i++) cx_add_method(ws, m[i].sel, m[i].fn);
    }
    cx_class_t *fm = cx_class_named("NSFileManager");
    if (fm && !cx_responds(fm, sel_registerName("displayNameAtPath:"))) cx_add_method(fm, "displayNameAtPath:", (void *)fm_display_name);
    cx_class_t *aem = cx_class_named("NSAppleEventManager");
    if (aem) {
        cx_add_method(aem, "setEventHandler:andSelector:forEventClass:andEventID:", (void *)aem_set);
        cx_add_method(aem, "removeEventHandlerForEventClass:andEventID:", (void *)aem_remove);
        cx_add_method(aem, "currentAppleEvent", (void *)aem_current);
    }
    cx_class_t *aed = cx_class_named("NSAppleEventDescriptor");
    if (aed) {
        g_ev_off = (cx_instance_size(aed) + 7) & ~7u;
        cx_define_class(&URLEvent_c, &URLEvent_m, "MOSURLAppleEvent", aed, 8 + (g_ev_off - cx_instance_size(aed)));
        cx_add_method(&URLEvent_c, "paramDescriptorForKeyword:", (void *)ev_param);
        cx_add_method(&URLEvent_c, "attributeDescriptorForKeyword:", (void *)ev_nil);
        cx_add_method(&URLEvent_c, "eventClass", (void *)ev_gurl);
        cx_add_method(&URLEvent_c, "eventID", (void *)ev_gurl);
        cx_add_method(&URLEvent_c, "descriptorType", (void *)ev_type);
        cx_add_method(&URLEvent_c, "numberOfItems", (void *)ev_count);
        cx_add_method(&URLEvent_c, "stringValue", (void *)ev_str);
        cx_add_method(&URLEvent_c, "dealloc", (void *)ev_dealloc);
    }
}
