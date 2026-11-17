/*
 * mOS libSystem — Open Scripting Architecture (OSA.framework, dentro do Carbon) sobre o NSAppleScript do mOS
 * (applescript.c, interpretador as_interp.inc).
 *
 * Cada OSAID é uma entrada de uma tabela do processo: um script (NSAppleScript) ou um valor (NSAppleEventDescriptor).
 * OSACompile/OSALoadScriptData/OSAGetScriptDataFromURL/OSAStoreFile: a forma "compilada" do mOS é o próprio texto
 * do script; um .scpt binário da Apple não é lido (errOSABadStorageType). OSAExecute/OSAExecuteEvent/OSADoEvent rodam
 * o script; OSACoerceToDesc e OSAScriptError devolvem o resultado e o último erro. OSASetSendProc/OSASetDefaultTarget:
 * cada Apple Event que o script manda passa pelo procedimento do app (gancho cx_osa_send_hook em applescript.c);
 * OSASetActiveProc é chamado antes de cada execução e a cada evento mandado (um erro dele cancela, como no macOS).
 * ASCopySourceAttributes: os estilos de realce do editor de scripts.
 */
#include "foundation.h"
#include "libc.h"

typedef int OSAError;
typedef u32 OSAID;
typedef struct { u64 location, length; } osa_range;
typedef struct __attribute__((packed, aligned(2))) { u32 descriptorType; void *dataHandle; } AEDesc;
int AECreateDesc(u32 type, const void *ptr, long size, AEDesc *result);
int AEDisposeDesc(AEDesc *d);
int AEDuplicateDesc(const AEDesc *src, AEDesc *result);
int AECoerceDesc(const AEDesc *d, u32 to, AEDesc *result);
long AEGetDescDataSize(const AEDesc *d);
int AEGetDescData(const AEDesc *d, void *ptr, long max);
int AEPutParamDesc(AEDesc *ev, u32 key, const AEDesc *d);
int AEPutAttributeDesc(AEDesc *ev, u32 key, const AEDesc *d);
int AEInitializeDesc(AEDesc *d);
id objc_retain(id o);
void objc_release(id o);
int open(const char *p, int fl, ...);
long read(int fd, void *b, unsigned long n);
long write(int fd, const void *b, unsigned long n);
int close(int fd);
typedef struct { u8 b[80]; } FSRef;
int FSRefMakePath(const FSRef *ref, u8 *path, u32 max);

#define FCC(a, b, c, d) ((u32)(a) << 24 | (u32)(b) << 16 | (u32)(c) << 8 | (u32)(d))
enum {
    noErr = 0, paramErr = -50, memFullErr = -108, fnfErr = -43, errAECoercionFail = -1700, errOSACantCoerce = -1700,
    errOSAInvalidID = -1751, errOSABadStorageType = -1752, errOSAScriptError = -1753, errOSABadSelector = -1754,
    errOSASourceNotAvailable = -1756, errOSACorruptData = -1702,
};
#define kOSANullScript 0u

static cx_class_t *kls(const char *n) { return cx_class_named(n); }

/* ================================================================ tabela de IDs */
typedef struct { id obj; int is_script; int modified; } osa_ent;
static osa_ent *g_tab;
static u32 g_n, g_cap;
static id g_last_error;             /* dicionário de erro do NSAppleScript (NSAppleScriptErrorNumber...) */
static OSAID put(id o, int script) {
    for (u32 i = 0; i < g_n; i++) if (!g_tab[i].obj) { g_tab[i].obj = objc_retain(o); g_tab[i].is_script = script; g_tab[i].modified = 0; return i + 1; }
    if (g_n == g_cap) {
        u32 nc = g_cap ? g_cap * 2 : 32;
        osa_ent *t = realloc(g_tab, nc * sizeof *t);
        if (!t) return kOSANullScript;
        g_tab = t; g_cap = nc;
    }
    g_tab[g_n].obj = objc_retain(o); g_tab[g_n].is_script = script; g_tab[g_n].modified = 0;
    return ++g_n;
}
static osa_ent *ent(OSAID i) { return i >= 1 && i <= g_n && g_tab[i - 1].obj ? &g_tab[i - 1] : 0; }
static void set_error(id info) { if (info) objc_retain(info); if (g_last_error) objc_release(g_last_error); g_last_error = info; }

/* texto UTF-8 de um AEDesc (TEXT, utf8, utxt ou o que coagir para utf8); devolve malloc ou 0 */
static char *desc_text(const AEDesc *d) {
    AEDesc u;
    AEInitializeDesc(&u);
    if (!d || AECoerceDesc(d, FCC('u', 't', 'f', '8'), &u)) return 0;
    long n = AEGetDescDataSize(&u);
    char *s = malloc((usize)n + 1);
    if (s) { AEGetDescData(&u, s, n); s[n] = 0; }
    AEDisposeDesc(&u);
    return s;
}
static id script_from_text(const char *src) {
    return MSG(id, MSG0(id, (id)kls("NSAppleScript"), "alloc"), "initWithSource:", cx_str(src));
}
/* o binário compilado da Apple começa com "FasdUAS"; o mOS só guarda e lê texto */
static int is_apple_binary(const char *p, long n) { return n >= 7 && !memcmp(p, "FasdUAS", 7); }

/* ================================================================ procedimentos do app */
typedef int (*osa_active_fn)(long refcon);
typedef int (*osa_send_fn)(const AEDesc *ev, AEDesc *reply, i32 mode, i16 prio, i32 timeout, void *idle, void *filter, long refcon);
static osa_active_fn g_active;
static long g_active_ref;
static osa_send_fn g_send;
static long g_send_ref;
static AEDesc g_target;
static int g_has_target;
extern int (*cx_osa_send_hook)(const void *ev_node, int to_self, void **reply_node);
static int active_check(void) { return g_active ? g_active(g_active_ref) : noErr; }
/* applescript.c chama isto para cada evento do script: alvo padrão e procedimento de envio do app */
static int osa_hook(const void *ev_node, int to_self, void **reply_node) {
    int e = active_check();
    if (e) return e;
    AEDesc ev = {FCC('a', 'e', 'v', 't'), (void *)ev_node};
    if (to_self && g_has_target) AEPutAttributeDesc(&ev, FCC('a', 'd', 'd', 'r'), &g_target);
    if (!g_send) return 1;                       /* 1: segue o envio normal do mOS */
    AEDesc reply;
    AEInitializeDesc(&reply);
    e = g_send(&ev, &reply, 3 /* kAEWaitReply */ | 0x20, 0, -1, 0, 0, g_send_ref);
    *reply_node = reply.dataHandle;              /* a resposta passa a ser do interpretador */
    return e;
}
static void hook_update(void) { cx_osa_send_hook = (g_send || g_has_target || g_active) ? osa_hook : 0; }

OSAError OSASetActiveProc(void *ci, osa_active_fn fn, long ref) { (void)ci; g_active = fn; g_active_ref = ref; hook_update(); return noErr; }
OSAError OSASetSendProc(void *ci, osa_send_fn fn, long ref) { (void)ci; g_send = fn; g_send_ref = ref; hook_update(); return noErr; }
OSAError OSASetDefaultTarget(void *ci, const AEDesc *target) {
    (void)ci;
    if (g_has_target) AEDisposeDesc(&g_target);
    g_has_target = 0;
    if (target && target->dataHandle && target->descriptorType != FCC('n', 'u', 'l', 'l')) {
        if (AEDuplicateDesc(target, &g_target)) return memFullErr;
        g_has_target = 1;
    }
    hook_update();
    return noErr;
}

/* ================================================================ compilar, carregar, guardar */
OSAError OSACompile(void *ci, const AEDesc *source, i32 mode, OSAID *io) {
    (void)ci; (void)mode;
    if (!source || !io) return paramErr;
    char *src = desc_text(source);
    if (!src) return errOSACantCoerce;
    id s = script_from_text(src);
    free(src);
    if (!s) return memFullErr;
    id info = 0;
    BOOL ok = ((BOOL (*)(id, SEL, id *))objc_msgSend)(s, sel_registerName("compileAndReturnError:"), &info);
    if (!ok) { set_error(info); objc_release(s); return errOSAScriptError; }
    osa_ent *old = ent(*io);
    if (old && old->is_script) {                   /* recompila no mesmo ID */
        objc_release(old->obj); old->obj = s; old->modified = 1;
        return noErr;
    }
    *io = put(s, 1);
    objc_release(s);
    return *io ? noErr : memFullErr;
}
static char *read_file(const char *path, long *len) {
    int fd = open(path, 0);
    if (fd < 0) return 0;
    usize cap = 4096, n = 0;
    char *b = malloc(cap + 1);
    for (long r; b && (r = read(fd, b + n, cap - n)) > 0;) {
        n += (usize)r;
        if (n == cap) { char *nb = realloc(b, cap * 2 + 1); if (!nb) { free(b); b = 0; break; } b = nb; cap *= 2; }
    }
    close(fd);
    if (b) { b[n] = 0; *len = (long)n; }
    return b;
}
static const char *url_path(id url) { return url ? cx_cstr(MSG0(id, url, "path")) : 0; }
OSAError OSAGetScriptDataFromURL(id url, u8 *storable, i32 mode, AEDesc *out) {
    (void)mode;
    if (!url || !out) return paramErr;
    long n = 0;
    char *b = read_file(url_path(url), &n);
    if (!b) return fnfErr;
    int bin = is_apple_binary(b, n);
    int e = AECreateDesc(bin ? FCC('s', 'c', 'p', 't') : FCC('u', 't', 'f', '8'), b, n, out);
    free(b);
    if (storable) *storable = !bin;
    return e;
}
OSAError OSALoadScriptData(void *ci, const AEDesc *data, id from_url, i32 mode, OSAID *out) {
    (void)ci; (void)mode;
    if (!out) return paramErr;
    AEDesc tmp;
    AEInitializeDesc(&tmp);
    if ((!data || !data->dataHandle) && from_url) {
        int e = OSAGetScriptDataFromURL(from_url, 0, 0, &tmp);
        if (e) return e;
        data = &tmp;
    }
    if (!data) return paramErr;
    long n = AEGetDescDataSize(data);
    char *raw = malloc((usize)n + 1);
    if (!raw) { AEDisposeDesc(&tmp); return memFullErr; }
    AEGetDescData(data, raw, n);
    raw[n] = 0;
    int bin = is_apple_binary(raw, n);
    free(raw);
    if (bin) { AEDisposeDesc(&tmp); return errOSABadStorageType; }
    AEDesc src = *data;
    src.descriptorType = data->descriptorType == FCC('s', 'c', 'p', 't') ? FCC('u', 't', 'f', '8') : data->descriptorType;
    *out = kOSANullScript;
    int e = OSACompile(ci, &src, mode, out);
    AEDisposeDesc(&tmp);
    return e;
}
static id source_of(osa_ent *e) { return e && e->is_script ? MSG0(id, e->obj, "source") : 0; }
OSAError OSAStoreFile(void *ci, OSAID sid, u32 type, i32 mode, const FSRef *file) {
    (void)ci; (void)type; (void)mode;
    osa_ent *e = ent(sid);
    if (!e) return errOSAInvalidID;
    id src = source_of(e);
    if (!src) return errOSASourceNotAvailable;
    char path[1024];
    if (!file || FSRefMakePath(file, (u8 *)path, sizeof path)) return fnfErr;
    const char *s = cx_cstr(src);
    int fd = open(path, 0x601 /* O_WRONLY|O_CREAT|O_TRUNC */, 0644);
    if (fd < 0) return fnfErr;
    long len = (long)strlen(s), w = write(fd, s, (unsigned long)len);
    close(fd);
    if (w != len) return -34;   /* dskFulErr */
    e->modified = 0;
    return noErr;
}
OSAError OSADispose(void *ci, OSAID i) {
    (void)ci;
    osa_ent *e = ent(i);
    if (!e) return i == kOSANullScript ? noErr : errOSAInvalidID;
    objc_release(e->obj);
    e->obj = 0;
    return noErr;
}

/* ================================================================ executar */
static OSAError finish_run(id result, id info, OSAID *out) {
    if (!result) { set_error(info); if (out) *out = kOSANullScript; return errOSAScriptError; }
    if (out) *out = put(result, 0);
    return noErr;
}
OSAError OSAExecute(void *ci, OSAID sid, OSAID ctx, i32 mode, OSAID *out) {
    (void)ci; (void)ctx; (void)mode;
    osa_ent *e = ent(sid);
    if (!e || !e->is_script) return errOSAInvalidID;
    int a = active_check();
    if (a) return a;
    id info = 0;
    id r = ((id (*)(id, SEL, id *))objc_msgSend)(e->obj, sel_registerName("executeAndReturnError:"), &info);
    return finish_run(r, info, out);
}
static id event_obj(const AEDesc *ev) {
    return MSG(id, MSG0(id, (id)kls("NSAppleEventDescriptor"), "alloc"), "initWithAEDesc:", ev);
}
/* o evento vai ao script do contexto (o último compilado, quando não há contexto) */
static osa_ent *context_script(OSAID ctx) {
    osa_ent *e = ent(ctx);
    if (e && e->is_script) return e;
    for (u32 i = g_n; i > 0; i--) if (g_tab[i - 1].obj && g_tab[i - 1].is_script) return &g_tab[i - 1];
    return 0;
}
OSAError OSAExecuteEvent(void *ci, const AEDesc *event, OSAID ctx, i32 mode, OSAID *out) {
    (void)ci; (void)mode;
    osa_ent *e = context_script(ctx);
    if (!e || !event) return errOSAInvalidID;
    int a = active_check();
    if (a) return a;
    id ev = event_obj(event), info = 0;
    id r = ((id (*)(id, SEL, id, id *))objc_msgSend)(e->obj, sel_registerName("executeAppleEvent:error:"), ev, &info);
    objc_release(ev);
    return finish_run(r, info, out);
}
static id err_val(const char *key) { return g_last_error ? MSG(id, g_last_error, "objectForKey:", cx_str(key)) : 0; }
OSAError OSADoEvent(void *ci, const AEDesc *event, OSAID ctx, i32 mode, AEDesc *reply) {
    OSAID rid = kOSANullScript;
    OSAError e = OSAExecuteEvent(ci, event, ctx, mode, &rid);
    if (!reply || !reply->dataHandle) { OSADispose(ci, rid); return e; }
    if (e == noErr) {
        osa_ent *v = ent(rid);
        const AEDesc *d = v ? MSG0(const AEDesc *, v->obj, "aeDesc") : 0;
        if (d) AEPutParamDesc(reply, FCC('-', '-', '-', '-'), d);
    } else if (e == errOSAScriptError) {
        /* como no macOS: o erro do script vai na resposta ('errn' e 'errs') e o OSADoEvent dá certo */
        id num = err_val("NSAppleScriptErrorNumber"), msg = err_val("NSAppleScriptErrorMessage");
        i32 n = num ? MSG0(int, num, "intValue") : -2700;
        AEDesc dn, ds;
        AECreateDesc(FCC('l', 'o', 'n', 'g'), &n, 4, &dn);
        AEPutParamDesc(reply, FCC('e', 'r', 'r', 'n'), &dn);
        AEDisposeDesc(&dn);
        if (msg) {
            const char *m = cx_cstr(msg);
            AECreateDesc(FCC('u', 't', 'f', '8'), m, (long)strlen(m), &ds);
            AEPutParamDesc(reply, FCC('e', 'r', 'r', 's'), &ds);
            AEDisposeDesc(&ds);
        }
        e = noErr;
    }
    OSADispose(ci, rid);
    return e;
}

/* ================================================================ resultados, informações e erros */
OSAError OSACoerceToDesc(void *ci, OSAID vid, u32 type, i32 mode, AEDesc *out) {
    (void)ci; (void)mode;
    if (!out) return paramErr;
    osa_ent *e = ent(vid);
    if (!e) return errOSAInvalidID;
    if (e->is_script) {                            /* um script vira o texto dele */
        id src = source_of(e);
        const char *s = src ? cx_cstr(src) : "";
        AEDesc t;
        AECreateDesc(FCC('u', 't', 'f', '8'), s, (long)strlen(s), &t);
        int r = type == FCC('*', '*', '*', '*') ? (AEDuplicateDesc(&t, out)) : AECoerceDesc(&t, type, out);
        AEDisposeDesc(&t);
        return r ? errOSACantCoerce : noErr;
    }
    const AEDesc *d = MSG0(const AEDesc *, e->obj, "aeDesc");
    if (!d) return errOSACantCoerce;
    int r = type == FCC('*', '*', '*', '*') ? AEDuplicateDesc(d, out) : AECoerceDesc(d, type, out);
    return r ? errOSACantCoerce : noErr;
}
OSAError OSAGetScriptInfo(void *ci, OSAID sid, u32 sel, i32 *out) {
    (void)ci;
    if (!out) return paramErr;
    osa_ent *e = ent(sid);
    if (!e) return errOSAInvalidID;
    switch (sel) {
    case FCC('m', 'o', 'd', 'i'): *out = e->modified; break;                       /* kOSAScriptIsModified */
    case FCC('c', 's', 'c', 'r'): *out = e->is_script; break;                      /* kOSAScriptIsTypeCompiledScript */
    case FCC('v', 'a', 'l', 'u'): *out = !e->is_script; break;                     /* kOSAScriptIsTypeScriptValue */
    case FCC('c', 'n', 't', 'x'): *out = e->is_script; break;                      /* kOSAScriptIsTypeScriptContext */
    case FCC('b', 'e', 's', 't'): *out = (i32)(e->is_script ? FCC('s', 'c', 'p', 't') : MSG0(u32, e->obj, "descriptorType")); break;
    case FCC('g', 's', 'r', 'c'): *out = e->is_script; break;                      /* kOSACanGetSource */
    default: return errOSABadSelector;
    }
    return noErr;
}
OSAError OSAScriptError(void *ci, u32 sel, u32 type, AEDesc *out) {
    (void)ci;
    if (!out) return paramErr;
    AEInitializeDesc(out);
    AEDesc t;
    int r;
    if (sel == FCC('e', 'r', 'r', 'n')) {                                          /* kOSAErrorNumber */
        id num = err_val("NSAppleScriptErrorNumber");
        i32 n = num ? MSG0(int, num, "intValue") : 0;
        AECreateDesc(FCC('l', 'o', 'n', 'g'), &n, 4, &t);
    } else if (sel == FCC('e', 'r', 'r', 's') || sel == FCC('e', 'r', 'r', 'b')) { /* kOSAErrorMessage / BriefMessage */
        id m = err_val(sel == FCC('e', 'r', 'r', 's') ? "NSAppleScriptErrorMessage" : "NSAppleScriptErrorBriefMessage");
        if (!m) m = err_val("NSAppleScriptErrorMessage");
        const char *s = m ? cx_cstr(m) : "";
        AECreateDesc(FCC('u', 't', 'f', '8'), s, (long)strlen(s), &t);
    } else if (sel == FCC('e', 'r', 'n', 'g')) {                                   /* kOSAErrorRange: 'srcs'/'srce' */
        id rv = err_val("NSAppleScriptErrorRange");
        osa_range rg = {0, 0};
        if (rv) rg = MSG0(osa_range, rv, "rangeValue");
        i16 a = (i16)rg.location, b = (i16)(rg.location + rg.length);
        AECreateDesc(FCC('r', 'e', 'c', 'o'), 0, 0, &t);
        AEDesc da, db;
        AECreateDesc(FCC('s', 'h', 'o', 'r'), &a, 2, &da);
        AECreateDesc(FCC('s', 'h', 'o', 'r'), &b, 2, &db);
        AEPutParamDesc(&t, FCC('s', 'r', 'c', 's'), &da);
        AEPutParamDesc(&t, FCC('s', 'r', 'c', 'e'), &db);
        AEDisposeDesc(&da); AEDisposeDesc(&db);
    } else return errOSABadSelector;
    r = type == FCC('*', '*', '*', '*') || type == t.descriptorType ? AEDuplicateDesc(&t, out) : AECoerceDesc(&t, type, out);
    AEDisposeDesc(&t);
    return r ? errOSACantCoerce : noErr;
}

/* ASCopySourceAttributes: um dicionário de atributos (fonte e cor) por estilo de fonte do AppleScript, na ordem dos
 * kASSourceStyle*: texto não compilado, normal, palavra da linguagem, comentário, operador, variável, referência a
 * aplicativo/valor, nome de classe, propriedade, enumerador, comando, parâmetro, nome de evento/handler */
OSAError ASCopySourceAttributes(void *ci, id *out) {
    (void)ci;
    if (!out) return paramErr;
    static const struct { double r, g, b; int bold, italic; } st[] = {
        {0.55, 0.00, 0.55, 0, 1}, {0, 0, 0, 0, 0}, {0, 0, 0, 1, 0}, {0.30, 0.30, 0.30, 0, 1}, {0, 0, 0, 0, 0},
        {0.20, 0.45, 0.10, 0, 0}, {0, 0, 0, 0, 0}, {0.00, 0.00, 0.80, 0, 1}, {0.50, 0.00, 0.50, 0, 0},
        {0.30, 0.00, 0.50, 0, 0}, {0.00, 0.00, 0.80, 1, 0}, {0.00, 0.00, 0.80, 0, 0}, {0.20, 0.45, 0.10, 0, 0},
    };
    id arr = MSG0(id, MSG0(id, (id)kls("NSMutableArray"), "alloc"), "init");
    id font = MSG(id, (id)kls("NSFont"), "userFixedPitchFontOfSize:", 12.0);
    id fm = MSG0(id, (id)kls("NSFontManager"), "sharedFontManager");
    for (unsigned i = 0; i < sizeof st / sizeof st[0]; i++) {
        id f = font;
        if (fm && st[i].bold) f = MSG(id, fm, "convertFont:toHaveTrait:", f, (u64)2);
        if (fm && st[i].italic) f = MSG(id, fm, "convertFont:toHaveTrait:", f, (u64)1);
        id col = MSG(id, (id)kls("NSColor"), "colorWithCalibratedRed:green:blue:alpha:", st[i].r, st[i].g, st[i].b, 1.0);
        id d = MSG0(id, MSG0(id, (id)kls("NSMutableDictionary"), "alloc"), "init");
        if (f) MSG(void, d, "setObject:forKey:", f, cx_str("NSFont"));
        if (col) MSG(void, d, "setObject:forKey:", col, cx_str("NSColor"));
        MSG(void, arr, "addObject:", d);
        objc_release(d);
    }
    *out = arr;
    return noErr;
}
