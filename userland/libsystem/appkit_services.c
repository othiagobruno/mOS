/*
 * mOS libSystem — Serviços do AppKit (menu Serviços, NSPerformService e o lado provedor), como no macOS:
 *   Descoberta: o NSServices do Info.plist de cada app nas pastas de aplicativos (NSSearchPathForDirectoriesInDomains),
 *     com NSMessage, NSMenuItem, NSSendTypes, NSReturnTypes, NSUserData e NSKeyEquivalent. NSUpdateDynamicServices relê.
 *   Menu Serviços (-[NSApp servicesMenu]/setServicesMenu:): um item por serviço, validado pela cadeia de responders
 *     com -validRequestorForSendType:returnType: (registerServicesMenuSendTypes:returnTypes: diz o que o app troca).
 *   Pedido: o requisitante escreve a seleção num pasteboard (writeSelectionToPasteboard:types:); NSPerformService leva o
 *     conteúdo (tipos e dados) dentro de um Apple Event 'mOSv'/'serv' ao app provedor (appleevents.c: serviço Mach do
 *     app, que o Launch Services abre se estiver fechado); a resposta traz o pasteboard devolvido, que vai para
 *     readSelectionFromPasteboard:.
 *   Provedor: -[NSApp setServicesProvider:] (ou NSRegisterServicesProvider) recebe -<NSMessage>:userData:error: com um
 *     pasteboard preenchido; o que ele escrever volta ao requisitante. Serviço do próprio app roda sem Apple Event.
 * Registro: cx_appkit_services_init(), chamado do cx_appkit_input_init.
 */
#include "foundation.h"
#include "sys.h"
#include "libc.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void NSBeep(void);
extern id NSApp;

#define K(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
K(_MOSServicesTarget) K(_MOSServicesHandler)

static cx_class_t *kls(const char *n) { return cx_class_named(n); }
static SEL sl(const char *n) { return sel_registerName(n); }
static BOOL responds(id o, const char *s) { return o && cx_responds(object_getClass(o), sl(s)); }
static id str(const char *s) { return cx_str(s); }

#define EV_CLASS 0x6D4F5376u   /* 'mOSv' */
#define EV_ID    0x73657276u   /* 'serv' */
#define KW_MSG   0x6D736720u   /* 'msg ' */
#define KW_UDAT  0x75646174u   /* 'udat' */
#define KW_PBRD  0x70627264u   /* 'pbrd' */
#define KW_ERRS  0x65727273u   /* 'errs' */
#define KW_TYPE  0x74797065u   /* 'type' */
#define KW_DATA  0x64617461u   /* 'data' */
#define KW_DIRECT 0x2D2D2D2Du  /* '----' */

static id g_services;          /* NSArray de NSDictionary (entrada do NSServices + "bundle") */
static id g_menu, g_provider, g_send_types, g_return_types;

/* ================================================================ descoberta */
static void scan_app(id fm, id path, id out) {
    id plist = MSG(id, path, "stringByAppendingPathComponent:", str("Contents/Info.plist"));
    id info = MSG(id, (id)kls("NSDictionary"), "dictionaryWithContentsOfFile:", plist);
    if (!info) return;
    id list = MSG(id, info, "objectForKey:", str("NSServices"));
    id bundle = MSG(id, info, "objectForKey:", str("CFBundleIdentifier"));
    if (!list || !bundle || !responds(list, "count")) return;
    (void)fm;
    for (u64 i = 0, n = MSG0(u64, list, "count"); i < n; i++) {
        id e = MSG(id, list, "objectAtIndex:", i);
        if (!responds(e, "objectForKey:") || !MSG(id, e, "objectForKey:", str("NSMessage"))) continue;
        id m = MSG0(id, e, "mutableCopy");
        MSG(void, m, "setObject:forKey:", bundle, str("bundle"));
        MSG(void, m, "setObject:forKey:", path, str("path"));
        MSG(void, out, "addObject:", m);
        objc_release(m);
    }
}
static void scan_dir(id fm, id dir, id out, int depth) {
    id names = MSG(id, fm, "contentsOfDirectoryAtPath:error:", dir, (id *)0);
    for (u64 i = 0, n = names ? MSG0(u64, names, "count") : 0; i < n; i++) {
        id name = MSG(id, names, "objectAtIndex:", i);
        id full = MSG(id, dir, "stringByAppendingPathComponent:", name);
        if (MSG(BOOL, name, "hasSuffix:", str(".app"))) scan_app(fm, full, out);
        else if (depth < 1) {                       /* Utilitários e afins: um nível abaixo */
            BOOL isdir = NO;
            if (MSG(BOOL, fm, "fileExistsAtPath:isDirectory:", full, &isdir) && isdir) scan_dir(fm, full, out, depth + 1);
        }
    }
}
id NSSearchPathForDirectoriesInDomains(u64 dir, u64 mask, BOOL expand);
void NSUpdateDynamicServices(void) {
    id out = MSG0(id, (id)kls("NSMutableArray"), "array");
    id fm = MSG0(id, (id)kls("NSFileManager"), "defaultManager");
    id dirs = NSSearchPathForDirectoriesInDomains(1, 0xFFFF, YES);   /* NSApplicationDirectory, todos os domínios */
    for (u64 i = 0, n = dirs ? MSG0(u64, dirs, "count") : 0; i < n; i++) scan_dir(fm, MSG(id, dirs, "objectAtIndex:", i), out, 0);
    id mb = MSG0(id, (id)kls("NSBundle"), "mainBundle");
    id own = mb ? MSG0(id, mb, "bundlePath") : 0;
    if (own && MSG(BOOL, own, "hasSuffix:", str(".app"))) {   /* o próprio app, mesmo fora das pastas */
        BOOL seen = NO;
        for (u64 i = 0, n = MSG0(u64, out, "count"); i < n && !seen; i++) seen = MSG(BOOL, MSG(id, MSG(id, out, "objectAtIndex:", i), "objectForKey:", str("path")), "isEqual:", own);
        if (!seen) scan_app(fm, own, out);
    }
    if (g_services) objc_release(g_services);
    g_services = objc_retain(out);
    if (g_menu) MSG0(void, g_menu, "update");
}
static id services(void) { if (!g_services) NSUpdateDynamicServices(); return g_services; }

/* título do item no menu: NSMenuItem { default = "Título" } (o caminho "A/B" vira o último pedaço) */
static id service_title(id e) {
    id mi = MSG(id, e, "objectForKey:", str("NSMenuItem"));
    id t = mi && responds(mi, "objectForKey:") ? MSG(id, mi, "objectForKey:", str("default")) : 0;
    if (!t) t = MSG(id, e, "objectForKey:", str("NSMessage"));
    id parts = MSG(id, t, "componentsSeparatedByString:", str("/"));
    return parts && MSG0(u64, parts, "count") ? MSG0(id, parts, "lastObject") : t;
}
static id service_named(id title) {
    id list = services();
    for (u64 i = 0, n = MSG0(u64, list, "count"); i < n; i++) {
        id e = MSG(id, list, "objectAtIndex:", i);
        if (MSG(BOOL, service_title(e), "isEqualToString:", title)) return e;
        id mi = MSG(id, e, "objectForKey:", str("NSMenuItem"));
        id full = mi && responds(mi, "objectForKey:") ? MSG(id, mi, "objectForKey:", str("default")) : 0;
        if (full && MSG(BOOL, full, "isEqualToString:", title)) return e;
    }
    return 0;
}
static id arr_of(id e, const char *k) { id a = MSG(id, e, "objectForKey:", str(k)); return a && responds(a, "count") ? a : MSG0(id, (id)kls("NSArray"), "array"); }

/* ================================================================ requisitante */
/* primeiro da cadeia de responders (a partir do primeiro respondedor da janela-chave) que aceita os tipos */
static id find_requestor(id send, id ret) {
    id win = MSG0(id, NSApp, "keyWindow");
    id r = win ? MSG0(id, win, "firstResponder") : 0;
    for (int guard = 0; r && guard < 64; guard++) {
        if (responds(r, "validRequestorForSendType:returnType:")) {
            id v = MSG(id, r, "validRequestorForSendType:returnType:", send, ret);
            if (v) return v;
        }
        r = responds(r, "nextResponder") ? MSG0(id, r, "nextResponder") : 0;
    }
    id d = MSG0(id, NSApp, "delegate");
    if (d && responds(d, "validRequestorForSendType:returnType:")) return MSG(id, d, "validRequestorForSendType:returnType:", send, ret);
    return 0;
}
/* requisitante para o serviço: um dos tipos de envio (ou nenhum) com um dos de retorno (ou nenhum) */
static id requestor_for(id e, id *send_out, id *ret_out) {
    id sends = arr_of(e, "NSSendTypes"), rets = arr_of(e, "NSReturnTypes");
    u64 ns = MSG0(u64, sends, "count"), nr = MSG0(u64, rets, "count");
    for (u64 i = 0; i <= ns; i++)
        for (u64 j = 0; j <= nr; j++) {
            if ((i == ns && ns) || (j == nr && nr)) continue;   /* com tipos declarados, um deles é obrigatório */
            id s = i < ns ? MSG(id, sends, "objectAtIndex:", i) : 0, r = j < nr ? MSG(id, rets, "objectAtIndex:", j) : 0;
            if (!s && !r) continue;
            if (g_send_types && s && !MSG(BOOL, g_send_types, "containsObject:", s)) continue;
            if (g_return_types && r && !MSG(BOOL, g_return_types, "containsObject:", r)) continue;
            id req = find_requestor(s, r);
            if (req) { if (send_out) *send_out = s; if (ret_out) *ret_out = r; return req; }
        }
    return 0;
}

/* pasteboard ↔ descritor: lista de registros { type: texto, data: bytes } */
static id pb_to_desc(id pb) {
    id list = MSG0(id, (id)kls("NSAppleEventDescriptor"), "listDescriptor");
    id types = MSG0(id, pb, "types");
    for (u64 i = 0, n = types ? MSG0(u64, types, "count") : 0; i < n; i++) {
        id t = MSG(id, types, "objectAtIndex:", i);
        id d = MSG(id, pb, "dataForType:", t);
        if (!d) continue;
        id rec = MSG0(id, (id)kls("NSAppleEventDescriptor"), "recordDescriptor");
        id td = MSG(id, (id)kls("NSAppleEventDescriptor"), "descriptorWithString:", t);
        id dd = MSG(id, (id)kls("NSAppleEventDescriptor"), "descriptorWithDescriptorType:data:", (u32)0x62696E20 /* 'bin ' */, d);
        MSG(void, rec, "setDescriptor:forKeyword:", td, (u32)KW_TYPE);
        MSG(void, rec, "setDescriptor:forKeyword:", dd, (u32)KW_DATA);
        MSG(void, list, "insertDescriptor:atIndex:", rec, (i64)0);   /* 0 = no fim */
    }
    return list;
}
static void desc_to_pb(id list, id pb) {
    i64 n = list ? MSG0(i64, list, "numberOfItems") : 0;
    id types = MSG0(id, (id)kls("NSMutableArray"), "array"), datas = MSG0(id, (id)kls("NSMutableArray"), "array");
    for (i64 i = 1; i <= n; i++) {                   /* listas de Apple Event contam de 1 */
        id rec = MSG(id, list, "descriptorAtIndex:", i);
        id td = MSG(id, rec, "descriptorForKeyword:", (u32)KW_TYPE), dd = MSG(id, rec, "descriptorForKeyword:", (u32)KW_DATA);
        id t = td ? MSG0(id, td, "stringValue") : 0, d = dd ? MSG0(id, dd, "data") : 0;
        if (!t || !d) continue;
        MSG(void, types, "addObject:", t); MSG(void, datas, "addObject:", d);
    }
    MSG(i64, pb, "declareTypes:owner:", types, (id)0);
    for (u64 i = 0, m = MSG0(u64, types, "count"); i < m; i++) {
        id d = MSG(id, datas, "objectAtIndex:", i), t = MSG(id, types, "objectAtIndex:", i);
        MSG(BOOL, pb, "setData:forType:", d, t);
    }
}

/* chama -<NSMessage>:userData:error: no provedor local */
static BOOL run_local(id provider, id message, id pb, id udata, id *err_out) {
    if (!provider || !message) return NO;
    const char *m = cx_cstr(message);
    char sel[256];
    snprintf(sel, sizeof sel, "%s:userData:error:", m);
    SEL s = sl(sel);
    if (!cx_responds(object_getClass(provider), s)) return NO;
    id err = 0;
    ((void (*)(id, SEL, id, id, id *))objc_msgSend)(provider, s, pb, udata, &err);
    if (err_out) *err_out = err;
    return err == 0;
}
static BOOL is_own(id e) {
    id mb = MSG0(id, (id)kls("NSBundle"), "mainBundle");
    id me = mb ? MSG0(id, mb, "bundleIdentifier") : 0;
    id b = MSG(id, e, "objectForKey:", str("bundle"));
    return me && MSG(BOOL, me, "isEqual:", b);
}
/* NSPerformService: o serviço pelo título; o pasteboard vai e volta pelo Apple Event */
BOOL NSPerformService(id item_name, id pb) {
    id e = item_name ? service_named(item_name) : 0;
    if (!e || !pb) return NO;
    id message = MSG(id, e, "objectForKey:", str("NSMessage")), udata = MSG(id, e, "objectForKey:", str("NSUserData"));
    if (is_own(e)) return run_local(g_provider, message, pb, udata, 0);
    id bundle = MSG(id, e, "objectForKey:", str("bundle"));
    id target = MSG(id, (id)kls("NSAppleEventDescriptor"), "descriptorWithBundleIdentifier:", bundle);
    id ev = ((id (*)(id, SEL, u32, u32, id, short, int))objc_msgSend)((id)kls("NSAppleEventDescriptor"),
        sl("appleEventWithEventClass:eventID:targetDescriptor:returnID:transactionID:"), EV_CLASS, EV_ID, target, (short)-1, 0);
    if (!ev) return NO;
    id md = MSG(id, (id)kls("NSAppleEventDescriptor"), "descriptorWithString:", message);
    MSG(void, ev, "setParamDescriptor:forKeyword:", md, (u32)KW_MSG);
    if (udata && MSG(BOOL, udata, "isKindOfClass:", (id)kls("NSString"))) {
        id ud = MSG(id, (id)kls("NSAppleEventDescriptor"), "descriptorWithString:", udata);
        MSG(void, ev, "setParamDescriptor:forKeyword:", ud, (u32)KW_UDAT);
    }
    id pd = pb_to_desc(pb);
    MSG(void, ev, "setParamDescriptor:forKeyword:", pd, (u32)KW_PBRD);
    id err = 0;
    id reply = ((id (*)(id, SEL, u64, double, id *))objc_msgSend)(ev, sl("sendEventWithOptions:timeout:error:"), (u64)3 /* kAEWaitReply */, 30.0, &err);
    if (!reply || err) return NO;
    id es = MSG(id, reply, "paramDescriptorForKeyword:", (u32)KW_ERRS);
    if (es) return NO;
    id back = MSG(id, reply, "paramDescriptorForKeyword:", (u32)KW_DIRECT);
    if (back) desc_to_pb(back, pb);
    return YES;
}

/* item do menu: escreve a seleção, pede o serviço e lê a resposta de volta no requisitante */
static void tgt_perform(id self, SEL c, id item) {
    (void)self; (void)c;
    id title = MSG0(id, item, "title");
    id e = service_named(title);
    id send = 0, ret = 0;
    id req = e ? requestor_for(e, &send, &ret) : 0;
    if (!req) { NSBeep(); return; }
    id pb = MSG0(id, (id)kls("NSPasteboard"), "pasteboardWithUniqueName");
    if (send) {
        id types = MSG(id, (id)kls("NSArray"), "arrayWithObject:", send);
        if (!MSG(BOOL, req, "writeSelectionToPasteboard:types:", pb, types)) { NSBeep(); return; }
    }
    if (!NSPerformService(title, pb)) { NSBeep(); return; }
    if (ret && responds(req, "readSelectionFromPasteboard:")) MSG(BOOL, req, "readSelectionFromPasteboard:", pb);
}
static BOOL tgt_validate(id self, SEL c, id item) {
    (void)self; (void)c;
    id e = service_named(MSG0(id, item, "title"));
    return e && requestor_for(e, 0, 0) != 0;
}
static id g_target;
static id target(void) { if (!g_target) g_target = cx_alloc_instance(&_MOSServicesTarget_c); return g_target; }

/* menu Serviços: refeito a cada abertura (menuNeedsUpdate:) com os serviços conhecidos, em ordem alfabética */
static void fill_menu(id menu) {
    MSG0(void, menu, "removeAllItems");
    id list = services();
    id titles = MSG0(id, (id)kls("NSMutableArray"), "array");
    for (u64 i = 0, n = MSG0(u64, list, "count"); i < n; i++) {
        id t = service_title(MSG(id, list, "objectAtIndex:", i));
        if (!MSG(BOOL, titles, "containsObject:", t)) MSG(void, titles, "addObject:", t);
    }
    MSG(void, titles, "sortUsingSelector:", sl("localizedCaseInsensitiveCompare:"));
    for (u64 i = 0, n = MSG0(u64, titles, "count"); i < n; i++) {
        id t = MSG(id, titles, "objectAtIndex:", i);
        id e = service_named(t);
        id ke = MSG(id, e, "objectForKey:", str("NSKeyEquivalent"));
        id k = ke && responds(ke, "objectForKey:") ? MSG(id, ke, "objectForKey:", str("default")) : 0;
        id mi = MSG(id, menu, "addItemWithTitle:action:keyEquivalent:", t, sl("_cxPerformService:"), k ? k : str(""));
        MSG(void, mi, "setTarget:", target());
        if (k) MSG(void, mi, "setKeyEquivalentModifierMask:", (u64)((1u << 20) | (1u << 17)));   /* ⌘⇧ como no macOS */
    }
    if (!MSG0(u64, titles, "count")) {
        id mi = MSG(id, menu, "addItemWithTitle:action:keyEquivalent:", str("Nenhum Serviço Disponível"), (SEL)0, str(""));
        MSG(void, mi, "setEnabled:", (BOOL)NO);
    }
}
static void tgt_menu_update(id self, SEL c, id menu) { (void)self; (void)c; fill_menu(menu); }
static id ap_services_menu(id self, SEL c) {
    (void)self; (void)c;
    if (!g_menu) {
        g_menu = objc_retain(MSG(id, MSG0(id, (id)kls("NSMenu"), "alloc"), "initWithTitle:", str("Serviços")));
        objc_release(g_menu);
        MSG(void, g_menu, "setDelegate:", target());
        fill_menu(g_menu);
    }
    return g_menu;
}
static void ap_set_services_menu(id self, SEL c, id m) {
    (void)self; (void)c;
    if (m == g_menu) return;
    if (m) objc_retain(m);
    if (g_menu) objc_release(g_menu);
    g_menu = m;
    if (m) { MSG(void, m, "setDelegate:", target()); fill_menu(m); }
}
static void ap_register_types(id self, SEL c, id send, id ret) {
    (void)self; (void)c;
    if (g_send_types) objc_release(g_send_types);
    if (g_return_types) objc_release(g_return_types);
    g_send_types = send ? MSG0(id, send, "copy") : 0;       /* copy já vem retido */
    g_return_types = ret ? MSG0(id, ret, "copy") : 0;
}
static id ap_provider(id self, SEL c) { (void)self; (void)c; return g_provider; }
static void ap_set_provider(id self, SEL c, id p) { (void)self; (void)c; if (p) objc_retain(p); if (g_provider) objc_release(g_provider); g_provider = p; }
void NSRegisterServicesProvider(id provider, id name) { (void)name; ap_set_provider(0, 0, provider); }
void NSUnregisterServicesProvider(id name) { (void)name; ap_set_provider(0, 0, 0); }
static BOOL g_shows = YES;
BOOL NSShowsServicesMenuItem(id item) { (void)item; return g_shows; }
i64 NSSetShowsServicesMenuItem(id item, BOOL show) { (void)item; g_shows = show; return 0; }

/* ================================================================ provedor (Apple Event 'mOSv'/'serv') */
static void hd_handle(id self, SEL c, id ev, id reply) {
    (void)self; (void)c;
    id md = MSG(id, ev, "paramDescriptorForKeyword:", (u32)KW_MSG);
    id ud = MSG(id, ev, "paramDescriptorForKeyword:", (u32)KW_UDAT);
    id pd = MSG(id, ev, "paramDescriptorForKeyword:", (u32)KW_PBRD);
    id message = md ? MSG0(id, md, "stringValue") : 0, udata = ud ? MSG0(id, ud, "stringValue") : 0;
    id pb = MSG0(id, (id)kls("NSPasteboard"), "pasteboardWithUniqueName");
    if (pd) desc_to_pb(pd, pb);
    id err = 0;
    if (!run_local(g_provider, message, pb, udata, &err)) {
        id es = MSG(id, (id)kls("NSAppleEventDescriptor"), "descriptorWithString:", err && responds(err, "description") ? MSG0(id, err, "description") : str("serviço indisponível"));
        MSG(void, reply, "setParamDescriptor:forKeyword:", es, (u32)KW_ERRS);
        return;
    }
    id back = pb_to_desc(pb);
    MSG(void, reply, "setParamDescriptor:forKeyword:", back, (u32)KW_DIRECT);
}

void *dispatch_get_main_queue(void);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
static void install_handler(void *ctx) {
    (void)ctx;
    id mgr = MSG0(id, (id)kls("NSAppleEventManager"), "sharedAppleEventManager");
    if (!mgr) return;
    static id handler;
    if (!handler) handler = cx_alloc_instance(&_MOSServicesHandler_c);
    ((void (*)(id, SEL, id, SEL, u32, u32))objc_msgSend)(mgr, sl("setEventHandler:andSelector:forEventClass:andEventID:"), handler,
        sl("handleServiceEvent:withReplyEvent:"), EV_CLASS, EV_ID);
}
void cx_appkit_services_init(void) {
    cx_class_t *obj = kls("NSObject"), *app = kls("NSApplication");
    if (!obj || !app) return;
    if (!kls("_MOSServicesTarget")) {
        cx_define_class(&_MOSServicesTarget_c, &_MOSServicesTarget_m, "_MOSServicesTarget", obj, 0);
        cx_add_method(&_MOSServicesTarget_c, "_cxPerformService:", (void *)tgt_perform);
        cx_add_method(&_MOSServicesTarget_c, "validateMenuItem:", (void *)tgt_validate);
        cx_add_method(&_MOSServicesTarget_c, "menuNeedsUpdate:", (void *)tgt_menu_update);
        cx_define_class(&_MOSServicesHandler_c, &_MOSServicesHandler_m, "_MOSServicesHandler", obj, 0);
        cx_add_method(&_MOSServicesHandler_c, "handleServiceEvent:withReplyEvent:", (void *)hd_handle);
    }
    cx_add_method(app, "servicesMenu", (void *)ap_services_menu);
    cx_add_method(app, "setServicesMenu:", (void *)ap_set_services_menu);
    cx_add_method(app, "registerServicesMenuSendTypes:returnTypes:", (void *)ap_register_types);
    cx_add_method(app, "servicesProvider", (void *)ap_provider);
    cx_add_method(app, "setServicesProvider:", (void *)ap_set_provider);
    /* o tratador do pedido de serviço fica instalado em todo app (sem provedor, responde com erro); vai para a fila
     * principal porque o NSAppleEventManager termina de se registrar depois dos inits do AppKit (appleevents.c) */
    dispatch_async_f(dispatch_get_main_queue(), 0, install_handler);
}
