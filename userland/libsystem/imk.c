/*
 * mOS libSystem — InputMethodKit: métodos de entrada de terceiros (IMKServer, IMKInputController, IMKCandidates).
 *
 * O método de entrada é um app à parte (como no macOS). A fonte escolhida fica em com.apple.HIToolbox
 * (appkit_carbon.c: TISSelectInputSource / cx_tis_selected_ime). Os dois lados conversam por Apple Events da classe
 * privada 'mOSi' (appleevents.c: síncronos, com resposta; sem o portão de automação, porque a escolha da fonte é o
 * consentimento):
 *   app em foco → método:  'key ' (a tecla e uma foto do texto: trecho em volta da seleção, seleção, composição,
 *                          retângulo do cursor na tela), 'actv'/'dact' (activateServer:/deactivateServer:), 'comm'
 *                          (commitComposition:). A resposta traz "handled" e a lista de operações no texto.
 *   método → app:          'ops ' sem resposta, para o que o método faz fora de uma tecla (escolha de candidato com o
 *                          mouse, commit pedido pelo próprio método).
 * No método, o IMKServer recebe os eventos e chama o controlador do cliente (um por processo cliente) com um
 * cliente intermediário (_MOSIMKClient, protocolo IMKTextInput) que responde pela foto e grava as operações
 * (insertText:replacementRange:, setMarkedText:selectionRange:replacementRange:). No app, as operações vão para o
 * NSTextInputClient do contexto (appkit_input.c chama cx_imk_handle/cx_imk_activate/cx_imk_commit).
 * IMKCandidates: painel de candidatos do próprio método (lista numerada perto do cursor; setas, números, enter, esc
 * e clique), com os candidatos de -candidates: do controlador.
 */
#include "foundation.h"
#include "libc.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy);
id objc_getAssociatedObject(id obj, const void *key);
IMP cx_lookup(cx_class_t *c, SEL s);
id cx_color(double r, double g, double b, double a);
int getpid(void);
const char *class_getName(cx_class_t *c);
const char *sel_getName(SEL s);
const char *cx_tis_selected_ime(void);
extern id NSFontAttributeName, NSForegroundColorAttributeName;

typedef struct { double x, y; } P;
typedef struct { double w, h; } S;
typedef struct { double x, y, w, h; } R;
typedef struct { u64 loc, len; } RG;
#define NOTFOUND 0x7fffffffffffffffull
static cx_class_t *kls(const char *n) { return cx_class_named(n); }
static SEL sl(const char *n) { return sel_registerName(n); }
static id str(const char *s) { return cx_str(s); }
static BOOL responds(id o, const char *s) { return o && cx_responds(object_getClass(o), sl(s)); }
static id mdict(void) { return MSG0(id, MSG0(id, (id)kls("NSMutableDictionary"), "alloc"), "init"); }
static id marr(void) { return MSG0(id, MSG0(id, (id)kls("NSMutableArray"), "alloc"), "init"); }
static void put(id d, const char *k, id v) { if (v) MSG(void, d, "setObject:forKey:", v, str(k)); }
static id get(id d, const char *k) { return d ? MSG(id, d, "objectForKey:", str(k)) : 0; }
static id num_i(i64 v) { return MSG(id, (id)kls("NSNumber"), "numberWithLongLong:", v); }
static id num_d(double v) { return MSG(id, (id)kls("NSNumber"), "numberWithDouble:", v); }
static i64 geti(id d, const char *k, i64 def) { id v = get(d, k); return v ? MSG0(i64, v, "longLongValue") : def; }
static double getd(id d, const char *k) { id v = get(d, k); return v ? MSG0(double, v, "doubleValue") : 0; }
static id range_obj(RG r) { id d = mdict(); put(d, "loc", num_i(r.loc == NOTFOUND ? -1 : (i64)r.loc)); put(d, "len", num_i((i64)r.len)); return objc_autorelease(d); }
static RG obj_range(id d) { RG r = {NOTFOUND, 0}; if (!d) return r; i64 l = geti(d, "loc", -1); r.loc = l < 0 ? NOTFOUND : (u64)l; r.len = (u64)geti(d, "len", 0); return r; }
static id attr_str(id s) { id a = MSG(id, MSG0(id, (id)kls("NSAttributedString"), "alloc"), "initWithString:", s ? s : str("")); return a ? objc_autorelease(a) : 0; }
static id item_at(id arr, u64 i) { return MSG(id, arr, "objectAtIndex:", i); }
static id plain(id s) { return s && !cx_is_str(s) && responds(s, "string") ? MSG0(id, s, "string") : s; }
#define FCC(a, b, c, d) ((u32)(a) << 24 | (u32)(b) << 16 | (u32)(c) << 8 | (u32)(d))
#define CLS_MOSI FCC('m', 'O', 'S', 'i')

/* plist ↔ dados do evento */
static id to_data(id plist) {
    return ((id (*)(id, SEL, id, u64, u64, id *))objc_msgSend)((id)kls("NSPropertyListSerialization"), sl("dataWithPropertyList:format:options:error:"), plist, (u64)100, (u64)0, (id *)0);
}
static id from_data(id data) {
    return data ? ((id (*)(id, SEL, id, u64, u64 *, id *))objc_msgSend)((id)kls("NSPropertyListSerialization"), sl("propertyListWithData:options:format:error:"), data, (u64)0, (u64 *)0, (id *)0) : 0;
}
static id event_with(u32 eid, id target, id plist) {
    id ev = ((id (*)(id, SEL, u32, u32, id, i16, i32))objc_msgSend)((id)kls("NSAppleEventDescriptor"),
        sl("appleEventWithEventClass:eventID:targetDescriptor:returnID:transactionID:"), CLS_MOSI, eid, target, (i16)-1, (i32)0);
    id data = plist ? to_data(plist) : 0;
    if (ev && data) {
        id p = MSG(id, (id)kls("NSAppleEventDescriptor"), "descriptorWithDescriptorType:data:", FCC('t', 'd', 't', 'a'), data);
        MSG(void, ev, "setParamDescriptor:forKeyword:", p, FCC('-', '-', '-', '-'));
    }
    return ev;
}
static id param_plist(id ev) {
    id p = ev ? MSG(id, ev, "paramDescriptorForKeyword:", FCC('-', '-', '-', '-')) : 0;
    return p ? from_data(MSG0(id, p, "data")) : 0;
}
static void reply_plist(id reply, id plist) {
    id data = to_data(plist);
    if (!reply || !data) return;
    id p = MSG(id, (id)kls("NSAppleEventDescriptor"), "descriptorWithDescriptorType:data:", FCC('t', 'd', 't', 'a'), data);
    MSG(void, reply, "setParamDescriptor:forKeyword:", p, FCC('-', '-', '-', '-'));
}

/* ================================================================ lado do app (cliente) */
static id g_last_client;    /* o NSTextInputClient que mandou a última tecla (recebe as operações assíncronas) */
static void apply_ops(id client, id ops) {
    u64 n = ops ? MSG0(u64, ops, "count") : 0;
    for (u64 i = 0; client && i < n; i++) {
        id op = MSG(id, ops, "objectAtIndex:", i);
        const char *k = cx_cstr(get(op, "op"));
        id text = get(op, "text");
        RG repl = obj_range(get(op, "repl")), sel = obj_range(get(op, "sel"));
        if (!strcmp(k, "insert") && responds(client, "insertText:replacementRange:"))
            ((void (*)(id, SEL, id, RG))objc_msgSend)(client, sl("insertText:replacementRange:"), text ? text : str(""), repl);
        else if (!strcmp(k, "mark") && responds(client, "setMarkedText:selectedRange:replacementRange:"))
            ((void (*)(id, SEL, id, RG, RG))objc_msgSend)(client, sl("setMarkedText:selectedRange:replacementRange:"), text ? text : str(""), sel, repl);
        else if (!strcmp(k, "unmark") && responds(client, "unmarkText")) MSG0(void, client, "unmarkText");
    }
}
/* operações que o método manda fora de uma tecla */
static void cl_ops_handler(id self, SEL c, id ev, id reply) {
    (void)self; (void)c; (void)reply;
    id pl = param_plist(ev);
    apply_ops(g_last_client, get(pl, "ops"));
}
static void client_install(void) {
    static int done;
    if (done) return;
    done = 1;
    static cx_class_t H_c __asm__("OBJC_CLASS_$__MOSIMKClientEvents"), H_m __asm__("OBJC_METACLASS_$__MOSIMKClientEvents");
    if (!kls("_MOSIMKClientEvents")) cx_define_class(&H_c, &H_m, "_MOSIMKClientEvents", kls("NSObject"), 0);
    cx_add_method(&H_c, "handleOps:withReplyEvent:", (void *)cl_ops_handler);
    id h = MSG0(id, MSG0(id, (id)&H_c, "alloc"), "init");
    id aem = MSG0(id, (id)kls("NSAppleEventManager"), "sharedAppleEventManager");
    ((void (*)(id, SEL, id, SEL, u32, u32))objc_msgSend)(aem, sl("setEventHandler:andSelector:forEventClass:andEventID:"), h, sl("handleOps:withReplyEvent:"), CLS_MOSI, FCC('o', 'p', 's', ' '));
}
static id my_bundle_id(void) {
    id b = MSG0(id, MSG0(id, (id)kls("NSBundle"), "mainBundle"), "bundleIdentifier");
    return b ? b : str("");
}
/* foto do texto do cliente: até 1000 caracteres de cada lado da seleção */
static id snapshot(id client) {
    id d = mdict();
    put(d, "pid", num_i(getpid()));
    put(d, "bundle", my_bundle_id());
    RG sel = {0, 0}, mk = {NOTFOUND, 0};
    if (responds(client, "selectedRange")) sel = MSG0(RG, client, "selectedRange");
    if (responds(client, "markedRange")) mk = MSG0(RG, client, "markedRange");
    put(d, "sel", range_obj(sel));
    put(d, "marked", range_obj(mk));
    u64 from = sel.loc != NOTFOUND && sel.loc > 1000 ? sel.loc - 1000 : 0;
    RG want = {from, 2000 + sel.len}, actual = {0, 0};
    id text = 0;
    if (responds(client, "attributedSubstringForProposedRange:actualRange:"))
        text = ((id (*)(id, SEL, RG, RG *))objc_msgSend)(client, sl("attributedSubstringForProposedRange:actualRange:"), want, &actual);
    if (text) { put(d, "text", plain(text)); put(d, "offset", num_i((i64)actual.loc)); }
    if (responds(client, "attributedString")) put(d, "length", num_i((i64)MSG0(u64, MSG0(id, client, "attributedString"), "length")));
    if (responds(client, "firstRectForCharacterRange:actualRange:")) {
        RG at = sel.loc == NOTFOUND ? (RG){0, 0} : sel;
        R r = ((R (*)(id, SEL, RG, RG *))objc_msgSend)(client, sl("firstRectForCharacterRange:actualRange:"), at, (RG *)0);
        put(d, "rx", num_d(r.x)); put(d, "ry", num_d(r.y)); put(d, "rw", num_d(r.w)); put(d, "rh", num_d(r.h));
    }
    return objc_autorelease(d);
}
static id send_to_ime(const char *ime, u32 eid, id plist, int wait) {
    id target = MSG(id, (id)kls("NSAppleEventDescriptor"), "descriptorWithBundleIdentifier:", str(ime));
    id ev = event_with(eid, target, plist);
    if (!ev) return 0;
    id err = 0;
    id reply = ((id (*)(id, SEL, u64, double, id *))objc_msgSend)(ev, sl("sendEventWithOptions:timeout:error:"), (u64)(wait ? 3 : 1), wait ? 3.0 : 1.0, &err);
    return wait ? param_plist(reply) : 0;
}
/* tecla no app: 1 = o método tratou, 0 = não tratou, -1 = não há método de entrada escolhido */
int cx_imk_handle(id client, id e) {
    const char *ime = cx_tis_selected_ime();
    if (!ime || !client || !e) return -1;
    u64 type = MSG0(u64, e, "type");
    if (type != 10 && type != 12) return 0;                     /* keyDown, flagsChanged */
    u64 flags = MSG0(u64, e, "modifierFlags");
    if (type == 10 && (flags & (1ull << 20))) return 0;         /* ⌘: atalhos do app */
    client_install();
    g_last_client = client;
    id d = snapshot(client);
    put(d, "type", num_i((i64)type));
    put(d, "flags", num_i((i64)flags));
    put(d, "keyCode", num_i(MSG0(u16, e, "keyCode")));
    if (type == 10) {
        put(d, "chars", MSG0(id, e, "characters"));
        put(d, "charsIM", MSG0(id, e, "charactersIgnoringModifiers"));
        put(d, "repeat", num_i(MSG0(BOOL, e, "isARepeat")));
    }
    id r = send_to_ime(ime, FCC('k', 'e', 'y', ' '), d, 1);
    if (!r) return 0;                                           /* método fora do ar: a tecla segue para o app */
    apply_ops(client, get(r, "ops"));
    return geti(r, "handled", 0) != 0;
}
void cx_imk_activate(id client, int on) {
    const char *ime = cx_tis_selected_ime();
    if (!ime || !client) return;
    client_install();
    if (on) g_last_client = client;
    send_to_ime(ime, on ? FCC('a', 'c', 't', 'v') : FCC('d', 'a', 'c', 't'), snapshot(client), 0);
}
void cx_imk_commit(id client) {
    const char *ime = cx_tis_selected_ime();
    if (!ime || !client) return;
    id r = send_to_ime(ime, FCC('c', 'o', 'm', 'm'), snapshot(client), 1);
    if (r) apply_ops(client, get(r, "ops"));
}

/* ================================================================ lado do método de entrada */
#define CLS(n) cx_class_t n##_c __asm__("OBJC_CLASS_$_" #n), n##_m __asm__("OBJC_METACLASS_$_" #n);
CLS(IMKServer) CLS(IMKInputController) CLS(IMKCandidates) CLS(_MOSIMKClient) CLS(_MOSIMKCandidateView)

/* ---- cliente intermediário (IMKTextInput) */
typedef struct {
    id info;            /* a foto que veio do app */
    id text;            /* NSMutableString: o trecho, atualizado pelas operações */
    i64 offset, length, pid;
    RG sel, marked;
    id ops;             /* operações gravadas durante uma tecla */
    int in_event;
} clst;
static char k_cl;
static clst *CL(id o) { id b = objc_getAssociatedObject(o, &k_cl); return b ? MSG0(void *, b, "pointerValue") : 0; }
static id client_new(i64 pid) {
    id o = MSG0(id, MSG0(id, (id)&_MOSIMKClient_c, "alloc"), "init");
    clst *c = calloc(1, sizeof *c);
    if (!o || !c) { free(c); return 0; }
    c->pid = pid;
    c->text = MSG0(id, MSG0(id, (id)kls("NSMutableString"), "alloc"), "init");
    c->sel.loc = 0; c->marked.loc = NOTFOUND;
    objc_setAssociatedObject(o, &k_cl, MSG(id, (id)kls("NSValue"), "valueWithPointer:", (void *)c), 1);
    return o;
}
static void client_load(id o, id info) {
    clst *c = CL(o);
    if (!c) return;
    if (c->info) objc_release(c->info);
    c->info = objc_retain(info);
    id t = get(info, "text");
    MSG(void, c->text, "setString:", t ? t : str(""));
    c->offset = geti(info, "offset", 0);
    c->length = geti(info, "length", c->offset + (i64)MSG0(u64, c->text, "length"));
    c->sel = obj_range(get(info, "sel"));
    if (c->sel.loc == NOTFOUND) c->sel.loc = (u64)c->length;
    c->marked = obj_range(get(info, "marked"));
}
static void client_flush_async(id o);
static void record(id o, const char *op, id text, RG sel, RG repl) {
    clst *c = CL(o);
    if (!c) return;
    id d = mdict();
    put(d, "op", str(op));
    put(d, "text", plain(text));
    put(d, "sel", range_obj(sel));
    put(d, "repl", range_obj(repl));
    if (!c->ops) c->ops = marr();
    MSG(void, c->ops, "addObject:", d);
    objc_release(d);
    if (!c->in_event) client_flush_async(o);
}
/* atualiza a foto como o texto do app vai ficar */
static void local_replace(clst *c, RG at, id s) {
    i64 len = s ? (i64)MSG0(u64, s, "length") : 0;
    i64 lo = (i64)at.loc - c->offset, tlen = (i64)MSG0(u64, c->text, "length");
    if (lo < 0) lo = 0;
    if (lo > tlen) lo = tlen;
    i64 rl = (i64)at.len;
    if (lo + rl > tlen) rl = tlen - lo;
    ((void (*)(id, SEL, RG, id))objc_msgSend)(c->text, sl("replaceCharactersInRange:withString:"), (RG){(u64)lo, (u64)rl}, s ? s : str(""));
    c->length += len - rl;
}
static RG target_range(clst *c, RG repl) {
    if (repl.loc != NOTFOUND) return repl;
    if (c->marked.loc != NOTFOUND) return c->marked;
    return c->sel;
}
static void cl_insert(id self, SEL cmd, id s, RG repl) {
    (void)cmd;
    clst *c = CL(self);
    if (!c) return;
    id p = plain(s);
    RG at = target_range(c, repl);
    local_replace(c, at, p);
    u64 len = p ? MSG0(u64, p, "length") : 0;
    c->sel = (RG){at.loc + len, 0};
    c->marked = (RG){NOTFOUND, 0};
    record(self, "insert", p, (RG){0, 0}, repl);
}
static void cl_set_marked(id self, SEL cmd, id s, RG sel, RG repl) {
    (void)cmd;
    clst *c = CL(self);
    if (!c) return;
    id p = plain(s);
    RG at = target_range(c, repl);
    local_replace(c, at, p);
    u64 len = p ? MSG0(u64, p, "length") : 0;
    c->marked = len ? (RG){at.loc, len} : (RG){NOTFOUND, 0};
    c->sel = (RG){at.loc + (sel.loc == NOTFOUND ? len : sel.loc), sel.len};
    record(self, "mark", p, sel, repl);
}
static RG cl_selected(id self, SEL cmd) { (void)cmd; clst *c = CL(self); RG z = {0, 0}; return c ? c->sel : z; }
static RG cl_marked(id self, SEL cmd) { (void)cmd; clst *c = CL(self); RG z = {NOTFOUND, 0}; return c ? c->marked : z; }
static i64 cl_length(id self, SEL cmd) { (void)cmd; clst *c = CL(self); return c ? c->length : 0; }
static id cl_substring(id self, SEL cmd, RG r) {
    (void)cmd;
    clst *c = CL(self);
    if (!c) return 0;
    i64 tlen = (i64)MSG0(u64, c->text, "length"), lo = (i64)r.loc - c->offset;
    if (r.loc == NOTFOUND || lo < 0 || lo > tlen) return 0;
    i64 n = (i64)r.len;
    if (lo + n > tlen) n = tlen - lo;
    id s = ((id (*)(id, SEL, RG))objc_msgSend)(c->text, sl("substringWithRange:"), (RG){(u64)lo, (u64)n});
    id a = MSG(id, MSG0(id, (id)kls("NSAttributedString"), "alloc"), "initWithString:", s);
    return a ? objc_autorelease(a) : 0;
}
static id cl_attr_substring(id self, SEL cmd, RG r, RG *actual) { if (actual) *actual = r; return cl_substring(self, cmd, r); }
static id cl_string_from(id self, SEL cmd, RG r, RG *actual) { id a = cl_attr_substring(self, cmd, r, actual); return a ? MSG0(id, a, "string") : 0; }
static R caret(clst *c) { R r = {getd(c->info, "rx"), getd(c->info, "ry"), getd(c->info, "rw"), getd(c->info, "rh")}; if (r.h <= 0) r.h = 18; return r; }
static R cl_first_rect(id self, SEL cmd, RG r, RG *actual) { (void)cmd; if (actual) *actual = r; clst *c = CL(self); R z = {0, 0, 0, 0}; return c ? caret(c) : z; }
/* atributos do caractere: a linha de base e a altura da linha, com o retângulo da linha na tela */
static id cl_attrs_at(id self, SEL cmd, u64 idx, R *line) {
    (void)cmd; (void)idx;
    clst *c = CL(self);
    R r = c ? caret(c) : (R){0, 0, 0, 18};
    if (line) *line = r;
    id d = mdict();
    P base = {r.x, r.y};
    put(d, "IMKBaseline", MSG(id, (id)kls("NSValue"), "valueWithPoint:", base));
    put(d, "IMKLineHeight", num_d(r.h));
    return objc_autorelease(d);
}
static u64 cl_char_index(id self, SEL cmd, P pt, i64 tracking, BOOL *in_marked) {
    (void)cmd; (void)pt; (void)tracking;
    clst *c = CL(self);
    if (in_marked) *in_marked = c && c->marked.loc != NOTFOUND;
    return c ? c->sel.loc : 0;
}
static id cl_valid_attrs(id self, SEL cmd) { (void)self; (void)cmd; return MSG0(id, (id)kls("NSArray"), "array"); }
static id cl_bundle(id self, SEL cmd) { (void)cmd; clst *c = CL(self); id b = c ? get(c->info, "bundle") : 0; return b ? b : str(""); }
static id cl_unique(id self, SEL cmd) {
    (void)cmd;
    clst *c = CL(self);
    char buf[64];
    snprintf(buf, sizeof buf, "%lld", (long long)(c ? c->pid : 0));
    return str(buf);
}
static i64 cl_window_level(id self, SEL cmd) { (void)self; (void)cmd; return 0; }
static BOOL cl_yes(id self, SEL cmd) { (void)self; (void)cmd; return YES; }
static BOOL cl_supports(id self, SEL cmd, u32 tag) { (void)self; (void)cmd; (void)tag; return NO; }
static void cl_noop1(id self, SEL cmd, id a) { (void)self; (void)cmd; (void)a; }
static void client_flush_async(id o) {
    clst *c = CL(o);
    if (!c || !c->ops || !MSG0(u64, c->ops, "count")) return;
    id d = mdict();
    put(d, "ops", c->ops);
    id target = MSG(id, (id)kls("NSAppleEventDescriptor"), "descriptorWithProcessIdentifier:", (int)c->pid);
    id ev = event_with(FCC('o', 'p', 's', ' '), target, d);
    objc_release(d);
    objc_release(c->ops);
    c->ops = 0;
    if (ev) ((id (*)(id, SEL, u64, double, id *))objc_msgSend)(ev, sl("sendEventWithOptions:timeout:error:"), (u64)1, 1.0, (id *)0);
}

/* ---- IMKServer */
static id g_server;
static char k_srv_name, k_srv_bundle, k_srv_ctrl, k_srv_dlgcls, k_srv_ctrls, k_srv_dead;
static id g_cur_ctrl, g_cur_client;
static id srv_controller_for(id srv, id info, id *client_out) {
    i64 pid = geti(info, "pid", 0);
    id ctrls = objc_getAssociatedObject(srv, &k_srv_ctrls);
    if (!ctrls) { ctrls = mdict(); objc_setAssociatedObject(srv, &k_srv_ctrls, ctrls, 1); objc_release(ctrls); }
    id key = num_i(pid);
    id pair = MSG(id, ctrls, "objectForKey:", key);
    if (!pair) {
        id client = client_new(pid);
        client_load(client, info);
        id cname = objc_getAssociatedObject(srv, &k_srv_ctrl);
        cx_class_t *k = cname ? kls(cx_cstr(cname)) : 0;
        if (!k) k = &IMKInputController_c;
        id dname = objc_getAssociatedObject(srv, &k_srv_dlgcls);
        cx_class_t *dk = dname ? kls(cx_cstr(dname)) : 0;
        id dlg = dk ? MSG0(id, MSG0(id, (id)dk, "alloc"), "init") : 0;
        id ctrl = ((id (*)(id, SEL, id, id, id))objc_msgSend)(MSG0(id, (id)k, "alloc"), sl("initWithServer:delegate:client:"), srv, dlg, client);
        id two[2] = {ctrl, client};
        pair = MSG(id, (id)kls("NSArray"), "arrayWithObjects:count:", two, (u64)2);
        MSG(void, ctrls, "setObject:forKey:", pair, key);
        if (ctrl) objc_release(ctrl);
        if (dlg) objc_release(dlg);
        objc_release(client);
    }
    id ctrl = MSG(id, pair, "objectAtIndex:", (u64)0), client = MSG(id, pair, "objectAtIndex:", (u64)1);
    client_load(client, info);
    if (client_out) *client_out = client;
    return ctrl;
}
static id make_event(id info) {
    u64 type = (u64)geti(info, "type", 10);
    id chars = get(info, "chars"), im = get(info, "charsIM");
    return ((id (*)(id, SEL, u64, P, u64, double, i64, id, id, id, BOOL, u16))objc_msgSend)((id)kls("NSEvent"),
        sl("keyEventWithType:location:modifierFlags:timestamp:windowNumber:context:characters:charactersIgnoringModifiers:isARepeat:keyCode:"),
        type, (P){0, 0}, (u64)geti(info, "flags", 0), 0.0, (i64)0, (id)0, chars ? chars : str(""), im ? im : str(""),
        (BOOL)(geti(info, "repeat", 0) != 0), (u16)geti(info, "keyCode", 0));
}
static void begin_event(id client) { clst *c = CL(client); if (c) { c->in_event = 1; if (c->ops) { objc_release(c->ops); c->ops = 0; } } }
static id end_event(id client, BOOL handled) {
    clst *c = CL(client);
    id d = mdict();
    put(d, "handled", num_i(handled));
    if (c && c->ops) { put(d, "ops", c->ops); objc_release(c->ops); c->ops = 0; }
    if (c) c->in_event = 0;
    return objc_autorelease(d);
}
static void srv_key(id self, SEL cmd, id ev, id reply) {
    (void)self; (void)cmd;
    id info = param_plist(ev), client = 0;
    id ctrl = g_server ? srv_controller_for(g_server, info, &client) : 0;
    if (!ctrl) { reply_plist(reply, end_event(0, NO)); return; }
    g_cur_ctrl = ctrl; g_cur_client = client;
    begin_event(client);
    id e = make_event(info);
    BOOL handled = e ? MSG(BOOL, ctrl, "handleEvent:client:", e, client) : NO;
    reply_plist(reply, end_event(client, handled));
}
static void srv_activate(id self, SEL cmd, id ev, id reply) {
    (void)self; (void)cmd;
    id info = param_plist(ev), client = 0;
    id ctrl = g_server ? srv_controller_for(g_server, info, &client) : 0;
    if (!ctrl) return;
    u32 eid = MSG0(u32, ev, "eventID");
    g_cur_ctrl = ctrl; g_cur_client = client;
    begin_event(client);
    if (eid == FCC('a', 'c', 't', 'v')) MSG(void, ctrl, "activateServer:", client);
    else if (eid == FCC('d', 'a', 'c', 't')) MSG(void, ctrl, "deactivateServer:", client);
    else MSG(void, ctrl, "commitComposition:", client);
    id r = end_event(client, YES);
    if (eid == FCC('c', 'o', 'm', 'm')) reply_plist(reply, r);
    else { clst *c = CL(client); if (c && get(r, "ops")) { c->ops = objc_retain(get(r, "ops")); client_flush_async(client); } }
}
static id srv_init_name(id self, SEL cmd, id name, id bundle_id) {
    (void)cmd;
    objc_setAssociatedObject(self, &k_srv_name, name, 3);
    objc_setAssociatedObject(self, &k_srv_bundle, bundle_id, 3);
    id mb = MSG0(id, (id)kls("NSBundle"), "mainBundle");
    id cc = MSG(id, mb, "objectForInfoDictionaryKey:", str("InputMethodServerControllerClass"));
    id dc = MSG(id, mb, "objectForInfoDictionaryKey:", str("InputMethodServerDelegateClass"));
    if (cc) objc_setAssociatedObject(self, &k_srv_ctrl, cc, 3);
    if (dc) objc_setAssociatedObject(self, &k_srv_dlgcls, dc, 3);
    g_server = self;
    id aem = MSG0(id, (id)kls("NSAppleEventManager"), "sharedAppleEventManager");
    static const struct { u32 id_; const char *sel; } ev[] = {
        {FCC('k', 'e', 'y', ' '), "_mosHandleKey:withReplyEvent:"}, {FCC('a', 'c', 't', 'v'), "_mosHandleState:withReplyEvent:"},
        {FCC('d', 'a', 'c', 't'), "_mosHandleState:withReplyEvent:"}, {FCC('c', 'o', 'm', 'm'), "_mosHandleState:withReplyEvent:"},
    };
    for (unsigned i = 0; i < sizeof ev / sizeof ev[0]; i++)
        ((void (*)(id, SEL, id, SEL, u32, u32))objc_msgSend)(aem, sl("setEventHandler:andSelector:forEventClass:andEventID:"), self, sl(ev[i].sel), CLS_MOSI, ev[i].id_);
    return self;
}
static id srv_init_classes(id self, SEL cmd, id name, cx_class_t *ctrl, cx_class_t *dlg) {
    id r = srv_init_name(self, cmd, name, MSG0(id, MSG0(id, (id)kls("NSBundle"), "mainBundle"), "bundleIdentifier"));
    if (ctrl) objc_setAssociatedObject(r, &k_srv_ctrl, str(class_getName(ctrl)), 3);
    if (dlg) objc_setAssociatedObject(r, &k_srv_dlgcls, str(class_getName(dlg)), 3);
    return r;
}
static id srv_bundle(id self, SEL cmd) { (void)self; (void)cmd; return MSG0(id, (id)kls("NSBundle"), "mainBundle"); }
static BOOL srv_dead(id self, SEL cmd) { (void)cmd; id v = objc_getAssociatedObject(self, &k_srv_dead); return v && MSG0(BOOL, v, "boolValue"); }
static void srv_set_dead(id self, SEL cmd, BOOL v) { (void)cmd; objc_setAssociatedObject(self, &k_srv_dead, num_i(v), 1); }
static BOOL srv_palette_term(id self, SEL cmd) { (void)self; (void)cmd; return YES; }

/* ---- IMKInputController (base) */
static char k_ic_server, k_ic_delegate, k_ic_client, k_ic_marks;
static id ic_init(id self, SEL cmd, id server, id delegate, id client) {
    (void)cmd;
    objc_setAssociatedObject(self, &k_ic_server, server, 0);
    objc_setAssociatedObject(self, &k_ic_delegate, delegate, 1);
    objc_setAssociatedObject(self, &k_ic_client, client, 0);
    return self;
}
static id ic_server(id self, SEL cmd) { (void)cmd; return objc_getAssociatedObject(self, &k_ic_server); }
static id ic_delegate(id self, SEL cmd) { (void)cmd; return objc_getAssociatedObject(self, &k_ic_delegate); }
static void ic_set_delegate(id self, SEL cmd, id d) { (void)cmd; objc_setAssociatedObject(self, &k_ic_delegate, d, 1); }
static id ic_client(id self, SEL cmd) { (void)cmd; return objc_getAssociatedObject(self, &k_ic_client); }
static BOOL implemented(id self, const char *sel) {
    IMP mine = cx_lookup(object_getClass(self), sl(sel)), base = cx_lookup(&IMKInputController_c, sl(sel));
    return mine && mine != base;
}
/* teclas sem texto: o seletor de edição correspondente (didCommandBySelector:client:) */
static const char *command_for(u16 code, u64 flags) {
    switch (code) {
    case 36: case 76: return "insertNewline:";
    case 48: return (flags & (1ull << 17)) ? "insertBacktab:" : "insertTab:";
    case 51: return "deleteBackward:";
    case 117: return "deleteForward:";
    case 53: return "cancelOperation:";
    case 123: return "moveLeft:";
    case 124: return "moveRight:";
    case 125: return "moveDown:";
    case 126: return "moveUp:";
    case 115: return "moveToBeginningOfLine:";
    case 119: return "moveToEndOfLine:";
    case 116: return "pageUp:";
    case 121: return "pageDown:";
    default: return 0;
    }
}
/* o despacho padrão do IMK: handleEvent → inputText:key:modifiers:client: → inputText:client: / didCommandBySelector */
static BOOL ic_handle(id self, SEL cmd, id e, id client) {
    (void)cmd;
    if (!e) return NO;
    u64 type = MSG0(u64, e, "type");
    u64 mask = MSG(u64, self, "recognizedEvents:", client);
    if (!(mask & (1ull << type))) return NO;
    if (type != 10) return NO;
    id chars = MSG0(id, e, "characters");
    u64 flags = MSG0(u64, e, "modifierFlags");
    u16 code = MSG0(u16, e, "keyCode");
    if (implemented(self, "inputText:key:modifiers:client:"))
        return ((BOOL (*)(id, SEL, id, i64, u64, id))objc_msgSend)(self, sl("inputText:key:modifiers:client:"), chars, (i64)code, flags, client);
    const char *cmdsel = command_for(code, flags);
    if (cmdsel && implemented(self, "didCommandBySelector:client:"))
        return MSG(BOOL, self, "didCommandBySelector:client:", sl(cmdsel), client);
    if (!cmdsel && chars && MSG0(u64, chars, "length") && implemented(self, "inputText:client:"))
        return MSG(BOOL, self, "inputText:client:", chars, client);
    return NO;
}
static u64 ic_recognized(id self, SEL cmd, id client) { (void)self; (void)cmd; (void)client; return (1ull << 10) | (1ull << 12); }   /* keyDown, flagsChanged */
static void ic_noop_client(id self, SEL cmd, id client) { (void)self; (void)cmd; (void)client; }
static id ic_nil_client(id self, SEL cmd, id client) { (void)self; (void)cmd; (void)client; return 0; }
static id ic_nil(id self, SEL cmd) { (void)self; (void)cmd; return 0; }
static void ic_noop(id self, SEL cmd) { (void)self; (void)cmd; }
/* commit padrão: o texto composto vai para o cliente */
static void ic_commit(id self, SEL cmd, id sender) {
    (void)cmd;
    id client = sender ? sender : ic_client(self, 0);
    id s = MSG(id, self, "composedString:", client);
    if (s && MSG0(u64, plain(s), "length"))
        ((void (*)(id, SEL, id, RG))objc_msgSend)(client, sl("insertText:replacementRange:"), s, (RG){NOTFOUND, 0});
}
static void ic_update(id self, SEL cmd) {
    (void)cmd;
    id client = ic_client(self, 0);
    id s = MSG(id, self, "composedString:", client);
    RG sel = MSG0(RG, self, "selectionRange"), repl = MSG0(RG, self, "replacementRange");
    ((void (*)(id, SEL, id, RG, RG))objc_msgSend)(client, sl("setMarkedText:selectionRange:replacementRange:"), s ? s : str(""), sel, repl);
}
static void ic_cancel(id self, SEL cmd) {
    (void)cmd;
    id client = ic_client(self, 0);
    ((void (*)(id, SEL, id, RG, RG))objc_msgSend)(client, sl("setMarkedText:selectionRange:replacementRange:"), str(""), (RG){0, 0}, (RG){NOTFOUND, 0});
}
static RG ic_sel_range(id self, SEL cmd) {
    (void)cmd;
    id s = MSG(id, self, "composedString:", ic_client(self, 0));
    RG r = {s ? MSG0(u64, plain(s), "length") : 0, 0};
    return r;
}
static RG ic_repl_range(id self, SEL cmd) { (void)self; (void)cmd; RG r = {NOTFOUND, 0}; return r; }
static id ic_comp_attrs(id self, SEL cmd, RG r) { (void)self; (void)cmd; (void)r; return MSG0(id, (id)kls("NSDictionary"), "dictionary"); }
static id ic_mark(id self, SEL cmd, u64 style, RG r) {
    (void)self; (void)cmd; (void)r;
    id d = mdict();
    put(d, "NSUnderline", num_i(style == 1 ? 2 : 1));   /* seleção: traço duplo; o resto: simples */
    return objc_autorelease(d);
}
static void ic_set_value(id self, SEL cmd, id value, i64 tag, id client) {
    (void)cmd; (void)tag; (void)client;
    id marks = objc_getAssociatedObject(self, &k_ic_marks);
    if (!marks) { marks = mdict(); objc_setAssociatedObject(self, &k_ic_marks, marks, 1); objc_release(marks); }
    if (value) MSG(void, marks, "setObject:forKey:", value, num_i(tag));
}
static id ic_modes(id self, SEL cmd, id client) { (void)self; (void)cmd; (void)client; return MSG0(id, (id)kls("NSDictionary"), "dictionary"); }
static void ic_show_prefs(id self, SEL cmd, id sender) {
    (void)self; (void)cmd; (void)sender;
    extern id NSApp;
    if (NSApp && responds(NSApp, "orderFrontStandardAboutPanel:")) MSG(void, NSApp, "activateIgnoringOtherApps:", (BOOL)YES);
}
static void ic_do_command(id self, SEL cmd, SEL s, id info) { (void)info; (void)cmd; if (responds(self, sel_getName(s))) MSG(void, self, sel_getName(s), (id)0); }

/* ---- IMKCandidates: painel numerado perto do cursor */
typedef struct { id ctrl, list, panel, view, keys, attrs; i64 sel, type; BOOL visible, dismiss; } cand_t;
static char k_cand, k_cv_owner;
static cand_t *CD(id o) { id b = objc_getAssociatedObject(o, &k_cand); return b ? MSG0(void *, b, "pointerValue") : 0; }
static id cand_init(id self, SEL cmd, id server, i64 type) {
    (void)cmd; (void)server;
    cand_t *c = calloc(1, sizeof *c);
    if (!c) return 0;
    c->type = type; c->dismiss = YES;
    objc_setAssociatedObject(self, &k_cand, MSG(id, (id)kls("NSValue"), "valueWithPointer:", (void *)c), 1);
    return self;
}
static const char *row_key(cand_t *c, i64 i, char *buf) {
    id k = c->keys && i < (i64)MSG0(u64, c->keys, "count") ? MSG(id, c->keys, "objectAtIndex:", (u64)i) : 0;
    if (k) snprintf(buf, 16, "%s", cx_cstr(k)); else snprintf(buf, 16, "%lld", (long long)(i + 1) % 10);
    return buf;
}
static void cv_draw(id self, SEL cmd, R dirty) {
    (void)cmd; (void)dirty;
    id owner = objc_getAssociatedObject(self, &k_cv_owner);
    cand_t *c = owner ? CD(owner) : 0;
    R b = ((R (*)(id, SEL))objc_msgSend)(self, sl("bounds"));
    MSG0(void, cx_color(0.98, 0.98, 0.99, 1), "setFill");
    ((void (*)(id, SEL, R))objc_msgSend)((id)kls("NSBezierPath"), sl("fillRect:"), b);
    u64 n = c && c->list ? MSG0(u64, c->list, "count") : 0;
    id font = MSG(id, (id)kls("NSFont"), "systemFontOfSize:", 15.0);
    for (u64 i = 0; i < n && i < 9; i++) {
        R row = {0, b.h - 24.0 * (double)(i + 1), b.w, 24};
        if ((i64)i == c->sel) {
            MSG0(void, cx_color(0.2, 0.5, 0.95, 1), "setFill");
            ((void (*)(id, SEL, R))objc_msgSend)((id)kls("NSBezierPath"), sl("fillRect:"), row);
        }
        char kb[16], line[512];
        id s = plain(MSG(id, c->list, "objectAtIndex:", i));
        snprintf(line, sizeof line, "%s. %s", row_key(c, (i64)i, kb), s ? cx_cstr(s) : "");
        id at = mdict();
        MSG(void, at, "setObject:forKey:", font, NSFontAttributeName);
        MSG(void, at, "setObject:forKey:", (i64)i == c->sel ? cx_color(1, 1, 1, 1) : cx_color(0.1, 0.1, 0.12, 1), NSForegroundColorAttributeName);
        ((void (*)(id, SEL, P, id))objc_msgSend)(str(line), sl("drawAtPoint:withAttributes:"), (P){8, row.y + 3}, at);
        objc_release(at);
    }
}
static void cand_choose(id self, i64 i);
static void cv_mouse(id self, SEL cmd, id e) {
    (void)cmd;
    id owner = objc_getAssociatedObject(self, &k_cv_owner);
    P w = MSG0(P, e, "locationInWindow");
    P p = ((P (*)(id, SEL, P, id))objc_msgSend)(self, sl("convertPoint:fromView:"), w, (id)0);
    R b = ((R (*)(id, SEL))objc_msgSend)(self, sl("bounds"));
    i64 i = (i64)((b.h - p.y) / 24);
    if (owner && i >= 0) cand_choose(owner, i);
}
static void cand_reload(id self) {
    cand_t *c = CD(self);
    if (!c) return;
    id ctrl = g_cur_ctrl;
    id list = ctrl && responds(ctrl, "candidates:") ? MSG(id, ctrl, "candidates:", g_cur_client) : 0;
    if (c->list) objc_release(c->list);
    c->list = list ? objc_retain(list) : 0;
    c->ctrl = ctrl;
    if (c->sel >= (i64)(list ? MSG0(u64, list, "count") : 0)) c->sel = 0;
}
static void cand_place(id self) {
    cand_t *c = CD(self);
    u64 n = c->list ? MSG0(u64, c->list, "count") : 0;
    if (n > 9) n = 9;
    double w = 220, h = 24.0 * (double)(n ? n : 1);
    clst *cl = g_cur_client ? CL(g_cur_client) : 0;
    R at = cl ? caret(cl) : (R){200, 400, 0, 18};
    R f = {at.x, at.y - h - 4, w, h};
    if (!c->panel) {
        c->panel = ((id (*)(id, SEL, R, u64, u64, BOOL))objc_msgSend)(MSG0(id, (id)kls("NSPanel"), "alloc"),
            sl("initWithContentRect:styleMask:backing:defer:"), f, (u64)(1u << 7), (u64)2, NO);   /* sem borda, não ativa */
        MSG(void, c->panel, "setReleasedWhenClosed:", (BOOL)NO);
        MSG(void, c->panel, "setLevel:", (i64)101);   /* acima dos menus, como o painel do sistema */
        c->view = ((id (*)(id, SEL, R))objc_msgSend)(MSG0(id, (id)&_MOSIMKCandidateView_c, "alloc"), sl("initWithFrame:"), (R){0, 0, w, h});
        objc_setAssociatedObject(c->view, &k_cv_owner, self, 0);
        MSG(void, c->panel, "setContentView:", c->view);
    }
    ((void (*)(id, SEL, R, BOOL))objc_msgSend)(c->panel, sl("setFrame:display:"), f, (BOOL)YES);
    MSG(void, c->view, "setNeedsDisplay:", (BOOL)YES);
}
static void cand_show(id self, SEL cmd, u64 hint) {
    (void)cmd; (void)hint;
    cand_t *c = CD(self);
    if (!c) return;
    cand_reload(self);
    if (!c->list || !MSG0(u64, c->list, "count")) { if (c->panel) MSG(void, c->panel, "orderOut:", (id)0); c->visible = NO; return; }
    cand_place(self);
    MSG(void, c->panel, "orderFront:", (id)0);
    c->visible = YES;
    if (c->ctrl && responds(c->ctrl, "candidateSelectionChanged:"))
    {
        id cur = plain(item_at(c->list, (u64)c->sel));
        MSG(void, c->ctrl, "candidateSelectionChanged:", attr_str(cur));
    }
}
static void cand_hide(id self, SEL cmd) { (void)cmd; cand_t *c = CD(self); if (c && c->panel) MSG(void, c->panel, "orderOut:", (id)0); if (c) c->visible = NO; }
static BOOL cand_visible(id self, SEL cmd) { (void)cmd; cand_t *c = CD(self); return c && c->visible; }
static void cand_update(id self, SEL cmd) { (void)cmd; cand_t *c = CD(self); if (c && c->visible) cand_show(self, 0, 0); else cand_reload(self); }
static void cand_choose(id self, i64 i) {
    cand_t *c = CD(self);
    if (!c || !c->list || i < 0 || i >= (i64)MSG0(u64, c->list, "count")) return;
    c->sel = i;
    id s = plain(MSG(id, c->list, "objectAtIndex:", (u64)i));
    id a = MSG(id, MSG0(id, (id)kls("NSAttributedString"), "alloc"), "initWithString:", s);
    if (c->ctrl && responds(c->ctrl, "candidateSelected:")) MSG(void, c->ctrl, "candidateSelected:", a);
    if (a) objc_release(a);
    if (c->dismiss) cand_hide(self, 0);
}
static void cand_move(id self, i64 d) {
    cand_t *c = CD(self);
    u64 n = c && c->list ? MSG0(u64, c->list, "count") : 0;
    if (!n) return;
    c->sel = (c->sel + d + (i64)n) % (i64)n;
    if (c->view) MSG(void, c->view, "setNeedsDisplay:", (BOOL)YES);
    if (c->ctrl && responds(c->ctrl, "candidateSelectionChanged:")) {
        id cur = plain(item_at(c->list, (u64)c->sel));
        MSG(void, c->ctrl, "candidateSelectionChanged:", attr_str(cur));
    }
}
static void cand_up(id self, SEL cmd, id s) { (void)cmd; (void)s; cand_move(self, -1); }
static void cand_down(id self, SEL cmd, id s) { (void)cmd; (void)s; cand_move(self, 1); }
static void cand_interpret(id self, SEL cmd, id events) {
    (void)cmd;
    cand_t *c = CD(self);
    u64 n = events ? MSG0(u64, events, "count") : 0;
    for (u64 i = 0; c && i < n; i++) {
        id e = MSG(id, events, "objectAtIndex:", i);
        u16 code = MSG0(u16, e, "keyCode");
        id ch = MSG0(id, e, "charactersIgnoringModifiers");
        const char *s = ch ? cx_cstr(ch) : "";
        if (code == 126) cand_move(self, -1);
        else if (code == 125) cand_move(self, 1);
        else if (code == 36 || code == 76) cand_choose(self, c->sel);
        else if (code == 53) cand_hide(self, 0);
        else if (s[0] >= '1' && s[0] <= '9' && !s[1]) cand_choose(self, s[0] - '1');
        else if (c->keys) {
            u64 kn = MSG0(u64, c->keys, "count");
            for (u64 k = 0; k < kn; k++) { id kk = item_at(c->keys, k); if (!strcmp(cx_cstr(kk), s)) { cand_choose(self, (i64)k); break; } }
        }
    }
}
static id cand_selected(id self, SEL cmd) {
    (void)cmd;
    cand_t *c = CD(self);
    if (!c || !c->list || c->sel >= (i64)MSG0(u64, c->list, "count")) return 0;
    id cur = plain(item_at(c->list, (u64)c->sel));
    return attr_str(cur);
}
static i64 cand_selected_id(id self, SEL cmd) { (void)cmd; cand_t *c = CD(self); return c ? c->sel : 0; }
static BOOL cand_select_id(id self, SEL cmd, i64 i) { (void)cmd; cand_t *c = CD(self); if (!c) return NO; c->sel = i; if (c->view) MSG(void, c->view, "setNeedsDisplay:", (BOOL)YES); return YES; }
static void cand_set_keys(id self, SEL cmd, id keys) { (void)cmd; cand_t *c = CD(self); if (!c) return; if (c->keys) objc_release(c->keys); c->keys = keys ? objc_retain(keys) : 0; }
static id cand_keys(id self, SEL cmd) { (void)cmd; cand_t *c = CD(self); return c ? c->keys : 0; }
static void cand_set_dismiss(id self, SEL cmd, BOOL v) { (void)cmd; cand_t *c = CD(self); if (c) c->dismiss = v; }
static BOOL cand_dismiss(id self, SEL cmd) { (void)cmd; cand_t *c = CD(self); return c ? c->dismiss : YES; }
static i64 cand_type(id self, SEL cmd) { (void)cmd; cand_t *c = CD(self); return c ? c->type : 0; }
static void cand_set_type(id self, SEL cmd, i64 t) { (void)cmd; cand_t *c = CD(self); if (c) c->type = t; }
static void cand_set_data(id self, SEL cmd, id list) { (void)cmd; cand_t *c = CD(self); if (!c) return; if (c->list) objc_release(c->list); c->list = list ? objc_retain(list) : 0; }
static void cand_clear(id self, SEL cmd) { (void)cmd; cand_t *c = CD(self); if (c) c->sel = 0; }
static void cand_set_attrs(id self, SEL cmd, id a) { (void)cmd; cand_t *c = CD(self); if (!c) return; if (c->attrs) objc_release(c->attrs); c->attrs = a ? objc_retain(a) : 0; }
static id cand_attrs(id self, SEL cmd) { (void)cmd; cand_t *c = CD(self); return c ? c->attrs : 0; }
static R cand_frame(id self, SEL cmd) { (void)cmd; cand_t *c = CD(self); R z = {0, 0, 0, 0}; return c && c->panel ? ((R (*)(id, SEL))objc_msgSend)(c->panel, sl("frame")) : z; }

/* ================================================================ registro */
static void def(cx_class_t *k, cx_class_t *m, const char *n, const char *sup) { if (!cx_class_named(n)) cx_define_class(k, m, n, kls(sup), 0); }
#define A(k, s, f) cx_add_method(&k##_c, s, (void *)(f))
void cx_imk_init(void) {
    def(&IMKServer_c, &IMKServer_m, "IMKServer", "NSObject");
    def(&IMKInputController_c, &IMKInputController_m, "IMKInputController", "NSObject");
    def(&IMKCandidates_c, &IMKCandidates_m, "IMKCandidates", "NSResponder");
    def(&_MOSIMKClient_c, &_MOSIMKClient_m, "_MOSIMKClient", "NSObject");
    def(&_MOSIMKCandidateView_c, &_MOSIMKCandidateView_m, "_MOSIMKCandidateView", "NSView");

    A(IMKServer, "initWithName:bundleIdentifier:", srv_init_name); A(IMKServer, "initWithName:controllerClass:delegateClass:", srv_init_classes);
    A(IMKServer, "bundle", srv_bundle); A(IMKServer, "paletteWillTerminate", srv_palette_term);
    A(IMKServer, "lastKeyEventWasDeadKey", srv_dead); A(IMKServer, "setLastKeyEventWasDeadKey:", srv_set_dead);
    A(IMKServer, "_mosHandleKey:withReplyEvent:", srv_key); A(IMKServer, "_mosHandleState:withReplyEvent:", srv_activate);

    A(IMKInputController, "initWithServer:delegate:client:", ic_init); A(IMKInputController, "server", ic_server);
    A(IMKInputController, "delegate", ic_delegate); A(IMKInputController, "setDelegate:", ic_set_delegate);
    A(IMKInputController, "client", ic_client); A(IMKInputController, "handleEvent:client:", ic_handle);
    A(IMKInputController, "recognizedEvents:", ic_recognized); A(IMKInputController, "activateServer:", ic_noop_client);
    A(IMKInputController, "deactivateServer:", ic_noop_client); A(IMKInputController, "commitComposition:", ic_commit);
    A(IMKInputController, "updateComposition", ic_update); A(IMKInputController, "cancelComposition", ic_cancel);
    A(IMKInputController, "composedString:", ic_nil_client); A(IMKInputController, "originalString:", ic_nil_client);
    A(IMKInputController, "candidates:", ic_nil_client); A(IMKInputController, "candidateSelected:", ic_noop_client);
    A(IMKInputController, "candidateSelectionChanged:", ic_noop_client); A(IMKInputController, "menu", ic_nil);
    A(IMKInputController, "selectionRange", ic_sel_range); A(IMKInputController, "replacementRange", ic_repl_range);
    A(IMKInputController, "compositionAttributesAtRange:", ic_comp_attrs); A(IMKInputController, "mark:forStyle:atRange:", ic_mark);
    A(IMKInputController, "setValue:forTag:client:", ic_set_value); A(IMKInputController, "modes:", ic_modes);
    A(IMKInputController, "showPreferences:", ic_show_prefs); A(IMKInputController, "hidePalettes", ic_noop);
    A(IMKInputController, "inputControllerWillClose", ic_noop); A(IMKInputController, "doCommandBySelector:commandDictionary:", ic_do_command);

    A(_MOSIMKClient, "insertText:replacementRange:", cl_insert); A(_MOSIMKClient, "setMarkedText:selectionRange:replacementRange:", cl_set_marked);
    A(_MOSIMKClient, "selectedRange", cl_selected); A(_MOSIMKClient, "markedRange", cl_marked); A(_MOSIMKClient, "length", cl_length);
    A(_MOSIMKClient, "attributedSubstringFromRange:", cl_substring); A(_MOSIMKClient, "attributedSubstringForProposedRange:actualRange:", cl_attr_substring);
    A(_MOSIMKClient, "stringFromRange:actualRange:", cl_string_from); A(_MOSIMKClient, "firstRectForCharacterRange:actualRange:", cl_first_rect);
    A(_MOSIMKClient, "attributesForCharacterIndex:lineHeightRectangle:", cl_attrs_at);
    A(_MOSIMKClient, "characterIndexForPoint:tracking:inMarkedRange:", cl_char_index);
    A(_MOSIMKClient, "validAttributesForMarkedText", cl_valid_attrs); A(_MOSIMKClient, "bundleIdentifier", cl_bundle);
    A(_MOSIMKClient, "uniqueClientIdentifierString", cl_unique); A(_MOSIMKClient, "windowLevel", cl_window_level);
    A(_MOSIMKClient, "supportsUnicode", cl_yes); A(_MOSIMKClient, "supportsProperty:", cl_supports);
    A(_MOSIMKClient, "overrideKeyboardWithKeyboardNamed:", cl_noop1); A(_MOSIMKClient, "selectInputMode:", cl_noop1);

    A(_MOSIMKCandidateView, "drawRect:", cv_draw); A(_MOSIMKCandidateView, "mouseDown:", cv_mouse);
    A(IMKCandidates, "initWithServer:panelType:", cand_init); A(IMKCandidates, "show:", cand_show); A(IMKCandidates, "hide", cand_hide);
    A(IMKCandidates, "isVisible", cand_visible); A(IMKCandidates, "updateCandidates", cand_update);
    A(IMKCandidates, "moveUp:", cand_up); A(IMKCandidates, "moveDown:", cand_down); A(IMKCandidates, "interpretKeyEvents:", cand_interpret);
    A(IMKCandidates, "selectedCandidateString", cand_selected); A(IMKCandidates, "selectedCandidate", cand_selected_id);
    A(IMKCandidates, "selectCandidateWithIdentifier:", cand_select_id); A(IMKCandidates, "selectCandidate:", cand_select_id);
    A(IMKCandidates, "setSelectionKeys:", cand_set_keys); A(IMKCandidates, "selectionKeys", cand_keys);
    A(IMKCandidates, "setDismissesAutomatically:", cand_set_dismiss); A(IMKCandidates, "dismissesAutomatically", cand_dismiss);
    A(IMKCandidates, "panelType", cand_type); A(IMKCandidates, "setPanelType:", cand_set_type);
    A(IMKCandidates, "setCandidateData:", cand_set_data); A(IMKCandidates, "clearSelection", cand_clear);
    A(IMKCandidates, "setAttributes:", cand_set_attrs); A(IMKCandidates, "attributes", cand_attrs); A(IMKCandidates, "candidateFrame", cand_frame);
}
