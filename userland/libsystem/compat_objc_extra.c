/*
 * mOS libSystem — métodos ObjC que o Sparkle (atualizador do Codex desktop) e o Codex (runtime OWL) pedem e o mOS
 * ainda não tinha (o log mostrava "ainda não implementado (devolvendo nil)"). Cada um só entra se a classe ainda não
 * o tiver, para não pisar em implementações mais completas dos outros arquivos. Chamado por cx_framework_classes_init.
 */
#include "foundation.h"

size_t strlen(const char *s);
char *strchr(const char *s, int c);
int strncmp(const char *a, const char *b, size_t n);
void *memcpy(void *d, const void *s, size_t n);

static void add(const char *cls, const char *sel, void *fn, int meta) {
    cx_class_t *c = cx_class_named(cls);
    if (!c) return;
    if (meta) c = (cx_class_t *)object_getClass((id)c);
    if (cx_lookup(c, sel_registerName(sel))) return;
    cx_add_method(c, sel, fn);
}

/* -[NSURL scheme]: o que vem antes do primeiro ':' do absoluteString (só letras, dígitos, + - .) */
static id url_scheme(id self, SEL c) {
    (void)c;
    id s = MSG0(id, self, "absoluteString");
    const char *t = s ? cx_cstr(s) : 0;
    if (!t) return 0;
    u64 i = 0;
    while (t[i] && t[i] != ':' && t[i] != '/' && t[i] != '?' && t[i] != '#') i++;
    return t[i] == ':' && i ? cx_str_n(t, i) : 0;
}

/* NSBundle: igualdade por ponteiro (o mOS não duplica bundles) e +bundleWithIdentifier: entre o principal e os
 * frameworks já carregados */
static BOOL bundle_is_equal_to(id self, SEL c, id o) { (void)c; return self == o || (o && MSG(BOOL, self, "isEqual:", o)); }
static id bundle_with_ident(id cls, SEL c, id ident) {
    (void)c;
    if (!ident) return 0;
    id main = MSG0(id, cls, "mainBundle");
    id mid = main ? MSG0(id, main, "bundleIdentifier") : 0;
    if (mid && MSG(BOOL, mid, "isEqualToString:", ident)) return main;
    id all = cx_lookup((cx_class_t *)object_getClass(cls), sel_registerName("allFrameworks")) ? MSG0(id, cls, "allFrameworks") : 0;
    u64 n = all ? MSG0(u64, all, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id b = MSG(id, all, "objectAtIndex:", i);
        id bi = MSG0(id, b, "bundleIdentifier");
        if (bi && MSG(BOOL, bi, "isEqualToString:", ident)) return b;
    }
    return 0;
}

/* +[NSCharacterSet characterSetWithCharactersInString:]: um conjunto que guarda os caracteres (subclasse própria) */
static cx_class_t strset_c, strset_m;
static int strset_inited;
extern id objc_autorelease(id);
typedef struct { cx_class_t *isa; id chars; } strset_t;
static BOOL strset_member(id self, SEL c, unsigned short ch) {
    (void)c;
    strset_t *s = (strset_t *)self;
    u64 n = s->chars ? MSG0(u64, s->chars, "length") : 0;
    for (u64 i = 0; i < n; i++) if (MSG(unsigned short, s->chars, "characterAtIndex:", i) == ch) return YES;
    return NO;
}
static BOOL strset_long_member(id self, SEL c, u32 ch) { return ch < 0x10000 && strset_member(self, c, (unsigned short)ch); }
static id cs_with_string(id cls, SEL c, id str) {
    (void)cls; (void)c;
    if (!strset_inited++) cx_define_class(&strset_c, &strset_m, "_MOSStringCharacterSet", cx_class_named("NSCharacterSet"), 8);
    strset_t *s = (strset_t *)cx_alloc_instance(&strset_c);
    s->chars = str ? MSG0(id, str, "copy") : 0;
    return objc_autorelease((id)s);
}

/* NSURLSession com delegate: a sessão compartilhada do mOS (os blocos de conclusão funcionam; os callbacks do
 * delegate ainda não são chamados) e a configuração "em segundo plano" = a padrão */
static id sess_with_cfg_delegate(id cls, SEL c, id cfg, id delegate, id queue) {
    (void)c; (void)cfg; (void)delegate; (void)queue;
    return MSG0(id, cls, "sharedSession");
}
static id cfg_background(id cls, SEL c, id ident) { (void)c; (void)ident; return MSG0(id, cls, "defaultSessionConfiguration"); }

static void noop1(id self, SEL c, long v) { (void)self; (void)c; (void)v; }
static void noop_obj(id self, SEL c, id v) { (void)self; (void)c; (void)v; }
static void noop_bool(id self, SEL c, BOOL v) { (void)self; (void)c; (void)v; }
static id ret_nil(id self, SEL c) { (void)self; (void)c; return 0; }
static long ret_zero(id self, SEL c) { (void)self; (void)c; return 0; }
static id ret_self(id self, SEL c) { (void)c; return self; }

/* codificação percentual (RFC 3986). Os conjuntos URL*AllowedCharacterSet são de string (acima), e o "permitido" de um
 * conjunto sem characterIsMember: é o não reservado (letras, dígitos, - . _ ~) */
static id cs_url_query(id cls, SEL c) { return cs_with_string(cls, c, cx_str("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._~!$'()*+,;=:@/?")); }
static id cs_url_path(id cls, SEL c) { return cs_with_string(cls, c, cx_str("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._~!$&'()*+,;=:@/")); }
static id cs_url_host(id cls, SEL c) { return cs_with_string(cls, c, cx_str("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-._~!$&'()*+,;=:[]")); }
static id cs_url_frag(id cls, SEL c) { return cs_url_query(cls, c); }
static int hexv(int ch) { return ch >= '0' && ch <= '9' ? ch - '0' : ch >= 'a' && ch <= 'f' ? ch - 'a' + 10 : ch >= 'A' && ch <= 'F' ? ch - 'A' + 10 : -1; }
void *malloc(size_t n);
void free(void *p);
static id str_remove_pct(id self, SEL c) {
    (void)c;
    const char *s = cx_cstr(self);
    u64 n = strlen(s), o = 0;
    char *b = malloc(n + 1);
    if (!b) return 0;
    for (u64 i = 0; i < n; i++) {
        if (s[i] == '%') {
            int h = i + 2 < n ? hexv(s[i + 1]) : -1, l = h >= 0 ? hexv(s[i + 2]) : -1;
            if (h < 0 || l < 0) { free(b); return 0; }   /* sequência inválida: nil, como na Apple */
            b[o++] = (char)(h << 4 | l); i += 2;
        } else b[o++] = s[i];
    }
    id r = cx_str_n(b, o);
    free(b);
    return r;
}
static id str_add_pct(id self, SEL c, id allowed) {
    (void)c;
    const char *s = cx_cstr(self);
    u64 n = strlen(s), o = 0;
    char *b = malloc(n * 3 + 1);
    if (!b) return 0;
    int has = allowed && cx_lookup((cx_class_t *)object_getClass(allowed), sel_registerName("characterIsMember:"));
    static const char hx[] = "0123456789ABCDEF";
    for (u64 i = 0; i < n; i++) {
        unsigned char ch = (unsigned char)s[i];
        int ok = ch < 0x80 && (has ? MSG(BOOL, allowed, "characterIsMember:", (unsigned short)ch)
                                   : ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || strchr("-._~", ch)));
        if (ok) b[o++] = (char)ch;
        else { b[o++] = '%'; b[o++] = hx[ch >> 4]; b[o++] = hx[ch & 15]; }
    }
    id r = cx_str_n(b, o);
    free(b);
    return r;
}

void cx_compat_objc_extra_init(void) {
    add("NSString", "stringByRemovingPercentEncoding", (void *)str_remove_pct, 0);
    add("NSString", "stringByAddingPercentEncodingWithAllowedCharacters:", (void *)str_add_pct, 0);
    add("NSCharacterSet", "URLQueryAllowedCharacterSet", (void *)cs_url_query, 1);
    add("NSCharacterSet", "URLPathAllowedCharacterSet", (void *)cs_url_path, 1);
    add("NSCharacterSet", "URLHostAllowedCharacterSet", (void *)cs_url_host, 1);
    add("NSCharacterSet", "URLFragmentAllowedCharacterSet", (void *)cs_url_frag, 1);
    add("NSURL", "scheme", (void *)url_scheme, 0);
    add("NSBundle", "isEqualTo:", (void *)bundle_is_equal_to, 0);
    add("NSBundle", "bundleWithIdentifier:", (void *)bundle_with_ident, 1);
    add("NSCharacterSet", "characterSetWithCharactersInString:", (void *)cs_with_string, 1);
    if (!strset_inited++) cx_define_class(&strset_c, &strset_m, "_MOSStringCharacterSet", cx_class_named("NSCharacterSet"), 8);
    cx_add_method(&strset_c, "characterIsMember:", (void *)strset_member);
    cx_add_method(&strset_c, "longCharacterIsMember:", (void *)strset_long_member);
    add("NSURLSession", "sessionWithConfiguration:delegate:delegateQueue:", (void *)sess_with_cfg_delegate, 1);
    add("NSURLSessionConfiguration", "backgroundSessionConfigurationWithIdentifier:", (void *)cfg_background, 1);
    add("NSMutableURLRequest", "setNetworkServiceType:", (void *)noop1, 0);
    add("NSURLRequest", "networkServiceType", (void *)ret_zero, 0);
    /* processo: o mOS não encerra apps automaticamente */
    add("NSProcessInfo", "enableAutomaticTermination:", (void *)noop_obj, 0);
    add("NSProcessInfo", "disableAutomaticTermination:", (void *)noop_obj, 0);
    /* janelas: aparência e abas automáticas (o mOS não tem abas de janela; a aparência vem do tema do sistema) */
    add("NSWindow", "setAllowsAutomaticWindowTabbing:", (void *)noop_bool, 1);
    add("NSWindow", "titleVisibility", (void *)ret_zero, 0);
    add("NSWindow", "setMovable:", (void *)noop_bool, 0);
    add("NSWindow", "appearance", (void *)ret_nil, 0);
    add("NSWindow", "nextResponder", (void *)ret_nil, 0);
    add("NSResponder", "setNextResponder:", (void *)noop_obj, 0);
    add("NSVisualEffectView", "setMaterial:", (void *)noop1, 0);
    add("NSVisualEffectView", "setBlendingMode:", (void *)noop1, 0);
    add("NSVisualEffectView", "setState:", (void *)noop1, 0);
    add("NSView", "setAppearance:", (void *)noop_obj, 0);
    add("NSView", "setClipsToBounds:", (void *)noop_bool, 0);
    add("NSFileManager", "ubiquityIdentityToken", (void *)ret_nil, 0);   /* sem iCloud: nil, como sem conta */
    (void)ret_self;
    { extern void cx_screencapturekit_init(void); cx_screencapturekit_init(); }   /* screencapturekit.c */
}
