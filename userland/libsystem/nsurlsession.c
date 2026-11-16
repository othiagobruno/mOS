/*
 * mOS — sistema de carga de URL do Foundation sobre o núcleo mnet (docs/rede-apps.md, etapa 4b):
 * NSURLSession, NSURLSessionConfiguration, tarefas de dados/envio/download, NSHTTPCookie e NSHTTPCookieStorage
 * (jar RFC 6265 do mnet, arquivo em ~/Library/Cookies), NSURLCache (RFC 9111, memória e disco), NSURLCredential,
 * NSURLCredentialStorage (permanente no Keychain), NSURLProtectionSpace, NSURLAuthenticationChallenge (Basic,
 * Digest e confiança do servidor), NSURLProtocol (classes do app interceptam pedidos), NSURLConnection e métricas.
 *
 * Fios: cada tarefa roda num pthread próprio chamando mnet_fetch (bloqueante). Tudo o que vai para o delegate ou
 * para os blocos de conclusão passa pela fila de delegate da sessão (serial, ou a principal quando o app deu a
 * mainQueue), na ordem. Decisões do delegate (resposta, redirect, desafio) fazem o fio da tarefa esperar o bloco.
 * NSURLRequest/NSHTTPURLResponse/NSURL/NSData são do Foundation (Eduardo) e aqui só entram pela API pública.
 */
#include "foundation.h"
#include "libc.h"
#include "mnet.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void *objc_autoreleasePoolPush(void);
void objc_autoreleasePoolPop(void *p);
void _Block_release(const void *b);
void objc_setAssociatedObject(id o, const void *k, id v, long policy);
id objc_getAssociatedObject(id o, const void *k);
void *dispatch_get_main_queue(void);
void *dispatch_queue_create(const char *label, void *attr);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
void *dispatch_semaphore_create(long v);
long dispatch_semaphore_wait(void *s, unsigned long long t);
long dispatch_semaphore_signal(void *s);
void dispatch_release(void *o);
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
int pthread_detach(unsigned long t);
void os_unfair_lock_lock(void *l);
void os_unfair_lock_unlock(void *l);
int open(const char *p, int fl, ...);
long read(int fd, void *b, unsigned long n);
long write(int fd, const void *b, unsigned long n);
int close(int fd);
int mkstemp(char *t);
int rename(const char *a, const char *b);
char *getenv(const char *n);
int mkdir(const char *p, int m);
unsigned char *CC_MD5(const void *d, unsigned n, unsigned char *md);
unsigned char *CC_SHA256(const void *d, unsigned n, unsigned char *md);
int SecItemAdd(id attrs, id *res);
int SecItemCopyMatching(id q, id *res);
int SecItemDelete(id q);
extern const void *kSecClass, *kSecClassInternetPassword, *kSecAttrAccount, *kSecValueData, *kSecReturnData;
void arc4random_buf(void *b, size_t n);

#define SEL_(s) sel_registerName(s)
extern cx_class_t NSObject_c __asm__("OBJC_CLASS_$_NSObject");
#define DISPATCH_TIME_FOREVER (~0ull)
static id retain(id o) { return o ? objc_retain(o) : 0; }
static void rel(id o) { if (o) objc_release(o); }
static id ar(id o) { return o ? objc_autorelease(o) : 0; }
static id ns(const char *s) { return s ? cx_str(s) : 0; }
static const char *cs(id o) { return o ? cx_cstr(o) : 0; }
static BOOL responds(id o, const char *sel) { return o && MSG(BOOL, o, "respondsToSelector:", SEL_(sel)); }
static id cls(const char *n) { return (id)(void *)cx_class_named(n); }
static id mutdict(void) { return MSG0(id, MSG0(id, cls("NSMutableDictionary"), "alloc"), "init"); }
static id mutarr(void) { return MSG0(id, MSG0(id, cls("NSMutableArray"), "alloc"), "init"); }
static void dset(id d, const char *k, id v) { if (!v) return; id key = ns(k); MSG(void, d, "setObject:forKey:", v, key); rel(key); }
static id dget(id d, const char *k) { if (!d) return 0; id key = ns(k); id v = MSG(id, d, "objectForKey:", key); rel(key); return v; }
static id data_bytes(const void *p, u64 n) { return MSG(id, cls("NSData"), "dataWithBytes:length:", p ? p : (const void *)"", n); }
static long long now_s(void) { return (long long)time(0); }
static id date_unix(long long t) { return MSG(id, cls("NSDate"), "dateWithTimeIntervalSince1970:", (double)t); }
static long long unix_of_date(id d) { return d ? (long long)MSG0(double, d, "timeIntervalSince1970") : -1; }
static char *dupc(const char *s) { return s ? mnet_strndup(s, strlen(s)) : 0; }
static id mutdata_new(void) { return MSG0(id, MSG0(id, cls("NSMutableData"), "alloc"), "init"); }

/* blocos: o layout do clang (invoke no offset 16) */
typedef struct { void *isa; int flags, reserved; void *invoke; void *desc; } blk_t;
#define BLK(b) ((blk_t *)(b))
static void call_v(void *b) { if (b) ((void (*)(void *))BLK(b)->invoke)(b); }
static void call_1(void *b, id a) { if (b) ((void (*)(void *, id))BLK(b)->invoke)(b, a); }

static void call_2_(void *b, id a, id c) { if (b) ((void (*)(void *, id, id))BLK(b)->invoke)(b, a, c); }
static void call_3(void *b, id a, id c, id d) { if (b) ((void (*)(void *, id, id, id))BLK(b)->invoke)(b, a, c, d); }
/* bloco nosso que o app chama (completionHandler dos métodos do delegate). Global: Block_copy devolve o mesmo. */
extern char _NSConcreteGlobalBlock[];
typedef struct reply {
    blk_t b;
    void *sem;
    long disp;
    id obj;                       /* credencial, pedido novo, resposta a guardar */
    int done;
} reply_t;
static struct { unsigned long reserved, size; } reply_desc = {0, sizeof(reply_t)};
static void reply_disp_obj(reply_t *r, long disp, id obj) {
    if (r->done) return;
    r->disp = disp; r->obj = retain(obj); r->done = 1;
    dispatch_semaphore_signal(r->sem);
}
static void inv_disp(void *b, long d) { reply_disp_obj((reply_t *)b, d, 0); }
static void inv_disp_obj(void *b, long d, id o) { reply_disp_obj((reply_t *)b, d, o); }
static void inv_obj(void *b, id o) { reply_disp_obj((reply_t *)b, 1, o); }
static reply_t *reply_new(void *invoke) {
    reply_t *r = calloc(1, sizeof *r);
    r->b.isa = _NSConcreteGlobalBlock;
    r->b.flags = 1 << 28;                                 /* BLOCK_IS_GLOBAL */
    r->b.invoke = invoke;
    r->b.desc = &reply_desc;
    r->sem = dispatch_semaphore_create(0);
    return r;
}
/* o bloco fica vivo enquanto a tarefa existir (o app pode guardar e chamar depois); os da tarefa saem no dealloc */

/* ---------------------------------------------------------------- erros */
static const char *err_text(int code) {
    switch (code) {
    case MNET_E_CANCELLED: return "cancelado";
    case MNET_E_BAD_URL: return "URL inválida";
    case MNET_E_TIMEOUT: return "A solicitação expirou.";
    case MNET_E_UNSUPPORTED: return "URL não suportada";
    case MNET_E_NO_HOST: return "Não foi possível encontrar um servidor com o nome especificado.";
    case MNET_E_CONNECT: return "Não foi possível conectar ao servidor.";
    case MNET_E_LOST: return "A conexão de rede foi perdida.";
    case MNET_E_REDIRECTS: return "redirecionamentos demais";
    case MNET_E_OFFLINE: return "A conexão com a internet parece estar offline.";
    case MNET_E_BAD_RESPONSE: return "resposta inválida do servidor";
    case MNET_E_DECODE: return "não foi possível decodificar os dados recebidos";
    case MNET_E_PARSE: return "não foi possível interpretar a resposta";
    case MNET_E_TLS: return "Ocorreu um erro de SSL e não é possível estabelecer uma conexão segura com o servidor.";
    case MNET_E_CERT_UNTRUSTED: return "O certificado deste servidor é inválido.";
    case -1008: return "recurso indisponível";
    case -1013: return "autenticação do usuário necessária";
    case -1014: return "zero byte resource";
    case -3000: return "não foi possível criar o arquivo do download";
    }
    return "erro desconhecido";
}
static id url_error(int code, const char *detail, id url) {
    id info = mutdict();
    char buf[400];
    const char *t = err_text(code);
    if (detail && *detail && code != MNET_E_CANCELLED) snprintf(buf, sizeof buf, "%s (%s)", t, detail);
    else snprintf(buf, sizeof buf, "%s", t);
    id d = ns(buf); dset(info, "NSLocalizedDescription", d); rel(d);
    if (url) {
        dset(info, "NSErrorFailingURLKey", url);
        id s = MSG0(id, url, "absoluteString");
        dset(info, "NSErrorFailingURLStringKey", s);
    }
    id dom = ns("NSURLErrorDomain");
    id e = MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:", dom, (long)code, info);
    rel(dom); rel(info);
    return retain(e);
}

/* ---------------------------------------------------------------- pedido do app -> forma C */
typedef struct {
    char *url, *method;
    mnet_hdrs_t hdrs;
    id body;                      /* NSData */
    id stream;                    /* NSInputStream */
    double timeout;
    unsigned long policy;
    int handle_cookies;
    char *main_doc;
} creq_t;
static void creq_free(creq_t *r) { free(r->url); free(r->method); mnet_hdrs_free(&r->hdrs); rel(r->body); rel(r->stream); free(r->main_doc); memset(r, 0, sizeof *r); }
static void creq_from(creq_t *o, id req, id extra_hdrs) {
    memset(o, 0, sizeof *o);
    id url = MSG0(id, req, "URL");
    o->url = dupc(url ? cs(MSG0(id, url, "absoluteString")) : "");
    id m = MSG0(id, req, "HTTPMethod");
    o->method = dupc(m ? cs(m) : "GET");
    /* cabeçalhos adicionais da configuração vêm primeiro; os do pedido valem por cima */
    id lists[2] = {extra_hdrs, MSG0(id, req, "allHTTPHeaderFields")};
    for (int l = 0; l < 2; l++) {
        id d = lists[l];
        if (!d) continue;
        id keys = MSG0(id, d, "allKeys");
        u64 n = keys ? MSG0(u64, keys, "count") : 0;
        for (u64 i = 0; i < n; i++) {
            id k = MSG(id, keys, "objectAtIndex:", i);
            id v = MSG(id, d, "objectForKey:", k);
            if (!k || !v) continue;
            if (MSG(BOOL, v, "isKindOfClass:", cls("NSString")) == NO) v = MSG0(id, v, "description");
            mnet_hdrs_set(&o->hdrs, cs(k), cs(v));
        }
    }
    o->body = retain(MSG0(id, req, "HTTPBody"));
    if (responds(req, "HTTPBodyStream")) o->stream = retain(MSG0(id, req, "HTTPBodyStream"));
    o->timeout = MSG0(double, req, "timeoutInterval");
    o->policy = responds(req, "cachePolicy") ? MSG0(unsigned long, req, "cachePolicy") : 0;
    o->handle_cookies = responds(req, "HTTPShouldHandleCookies") ? MSG0(BOOL, req, "HTTPShouldHandleCookies") : 1;
    if (responds(req, "mainDocumentURL")) { id md = MSG0(id, req, "mainDocumentURL"); if (md) o->main_doc = dupc(cs(MSG0(id, md, "absoluteString"))); }
}
static id hdrs_dict(const mnet_hdrs_t *h) {
    id d = mutdict();
    for (int i = 0; i < h->n; i++) {
        /* nomes repetidos juntam com ", " (Set-Cookie inclusive: é o que o CFNetwork faz em allHeaderFields) */
        id k = ns(h->v[i].name);
        id old = MSG(id, d, "objectForKey:", k);
        id k2 = 0;
        if (old) {
            for (int j = 0; j < i; j++) if (mnet_ieq(h->v[j].name, h->v[i].name)) { rel(k); k = ns(h->v[j].name); break; }
            char *joined = mnet_hdrs_join(h, h->v[i].name);
            id v = ns(joined); free(joined);
            MSG(void, d, "setObject:forKey:", v, k); rel(v);
        } else {
            id v = ns(h->v[i].value);
            MSG(void, d, "setObject:forKey:", v, k); rel(v);
        }
        rel(k); rel(k2);
    }
    return d;
}
static void dict_hdrs(id d, mnet_hdrs_t *h) {
    id keys = d ? MSG0(id, d, "allKeys") : 0;
    u64 n = keys ? MSG0(u64, keys, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id k = MSG(id, keys, "objectAtIndex:", i), v = MSG(id, d, "objectForKey:", k);
        if (k && v) mnet_hdrs_add(h, cs(k), strlen(cs(k)), cs(v), strlen(cs(v)));
    }
}
static id make_response(const char *url, int status, int version, const mnet_hdrs_t *h) {
    id u = MSG(id, cls("NSURL"), "URLWithString:", ns(url));
    id d = hdrs_dict(h);
    id ver = ns(version == 20 ? "HTTP/2" : version == 10 ? "HTTP/1.0" : "HTTP/1.1");
    id r = MSG(id, MSG0(id, cls("NSHTTPURLResponse"), "alloc"), "initWithURL:statusCode:HTTPVersion:headerFields:", u, (long)status, ver, d);
    rel(d); rel(ver);
    return r;
}
static void response_hdrs(id resp, mnet_hdrs_t *h, int *status) {
    *status = responds(resp, "statusCode") ? (int)MSG0(long, resp, "statusCode") : 200;
    if (responds(resp, "allHeaderFields")) dict_hdrs(MSG0(id, resp, "allHeaderFields"), h);
}

/* ================================================================ classes */
#define CLASS(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
CLASS(NSHTTPCookie) CLASS(NSHTTPCookieStorage) CLASS(NSURLCache) CLASS(NSCachedURLResponse)
CLASS(NSURLCredential) CLASS(NSURLCredentialStorage) CLASS(NSURLProtectionSpace) CLASS(NSURLAuthenticationChallenge)
CLASS(NSURLSession) CLASS(NSURLSessionConfiguration) CLASS(NSURLSessionTask) CLASS(NSURLSessionDataTask)
CLASS(NSURLSessionUploadTask) CLASS(NSURLSessionDownloadTask) CLASS(NSURLSessionStreamTask)
CLASS(NSURLSessionTaskMetrics) CLASS(NSURLSessionTaskTransactionMetrics)
CLASS(NSURLConnection) CLASS(NSURLProtocol) CLASS(MOSURLProtocolClient)
CLASS(NSURLSessionWebSocketTask) CLASS(NSURLSessionWebSocketMessage)
#define DEF(N, super) cx_define_class(&N##_c, &N##_m, #N, (super), (u32)(sizeof(N##_t) - 8))
#define M(N, s, f) cx_add_method(&N##_c, s, (void *)(f))
#define CM(N, s, f) cx_add_cmethod(&N##_c, s, (void *)(f))
static unsigned g_lock;                                   /* jar, armazenamentos e listas de tarefas */
static void LK(void) { os_unfair_lock_lock(&g_lock); }
static void ULK(void) { os_unfair_lock_unlock(&g_lock); }

/* constantes NSString exportadas (mesmo formato do KNS do foundation_more.c) */
extern cx_class_t NSConstantString_c __asm__("__CFConstantStringClassReference");
#define KNS(sym) static nsstr_t sym##_o = { &NSConstantString_c, 0, (char *)#sym, sizeof(#sym) - 1 }; id sym = (id)&sym##_o;
KNS(NSHTTPCookieManagerCookiesChangedNotification) KNS(NSHTTPCookieManagerAcceptPolicyChangedNotification)
KNS(NSURLCredentialStorageChangedNotification)

/* ================================================================ NSHTTPCookie */
typedef struct { cx_class_t *isa; mnet_cookie_t c; id props; } NSHTTPCookie_t;
#define CK(o) ((NSHTTPCookie_t *)(o))
static id cookie_from_c(const mnet_cookie_t *c) {
    NSHTTPCookie_t *o = (NSHTTPCookie_t *)cx_alloc_instance(&NSHTTPCookie_c);
    o->c = *c;
    o->c.next = 0;
    o->c.name = dupc(c->name); o->c.value = dupc(c->value); o->c.domain = dupc(c->domain); o->c.path = dupc(c->path);
    return (id)o;
}
static void cookie_dealloc(id s, SEL c) {
    NSHTTPCookie_t *o = CK(s);
    free(o->c.name); free(o->c.value); free(o->c.domain); free(o->c.path);
    rel(o->props);
    IMP sup = cx_lookup(&NSObject_c, c); if (sup) ((void (*)(id, SEL))sup)(s, c);
}
static BOOL truthy(id v) {
    if (!v) return NO;
    if (MSG(BOOL, v, "isKindOfClass:", cls("NSString"))) { const char *t = cs(v); return t && *t && !mnet_ieq(t, "FALSE") && strcmp(t, "0"); }
    return MSG0(BOOL, v, "boolValue");
}
static id cookie_init_props(id s, SEL c, id p) {
    (void)c;
    NSHTTPCookie_t *o = CK(s);
    id name = dget(p, "Name"), value = dget(p, "Value"), domain = dget(p, "Domain"), path = dget(p, "Path"), origin = dget(p, "OriginURL");
    if (!name || !value) { rel(s); return 0; }
    const char *dom = domain ? cs(domain) : 0;
    char hostbuf[256] = {0};
    if (!dom && origin) {
        id u = MSG(BOOL, origin, "isKindOfClass:", cls("NSURL")) ? origin : MSG(id, cls("NSURL"), "URLWithString:", origin);
        id h = u ? MSG0(id, u, "host") : 0;
        if (h) { snprintf(hostbuf, sizeof hostbuf, "%s", cs(h)); dom = hostbuf; }
    }
    if (!dom || !*dom) { rel(s); return 0; }
    memset(&o->c, 0, sizeof o->c);
    o->c.name = dupc(cs(name)); o->c.value = dupc(cs(value));
    o->c.host_only = dom[0] != '.';
    o->c.domain = dupc(dom[0] == '.' ? dom + 1 : dom);
    for (char *q = o->c.domain; *q; q++) *q = (char)mnet_lc((unsigned char)*q);
    o->c.path = dupc(path ? cs(path) : "/");
    o->c.secure = truthy(dget(p, "Secure"));
    o->c.http_only = truthy(dget(p, "HttpOnly"));
    id ss = dget(p, "SameSite");
    if (ss) { const char *t = cs(ss); o->c.same_site = mnet_ieq(t, "strict") ? MNET_SAMESITE_STRICT : mnet_ieq(t, "lax") ? MNET_SAMESITE_LAX : mnet_ieq(t, "none") ? MNET_SAMESITE_NONE : 0; }
    o->c.expires = -1;
    id ma = dget(p, "Max-Age"), ex = dget(p, "Expires");
    if (ma) { o->c.expires = now_s() + (long long)MSG0(long long, ma, "longLongValue"); o->c.persistent = 1; }
    else if (ex) {
        if (MSG(BOOL, ex, "isKindOfClass:", cls("NSDate"))) o->c.expires = unix_of_date(ex);
        else { const char *t = cs(MSG0(id, ex, "description")); o->c.expires = mnet_http_date(t, strlen(t)); }
        o->c.persistent = o->c.expires >= 0;
    }
    if (truthy(dget(p, "Discard"))) { o->c.persistent = 0; o->c.expires = -1; }
    o->c.created = now_s();
    o->props = retain(p);
    return s;
}
static id cookie_with_props(id k, SEL c, id p) { (void)k; return ar(cookie_init_props(cx_alloc_instance(&NSHTTPCookie_c), c, p)); }
static id cookie_name(id s, SEL c) { (void)c; return ar(ns(CK(s)->c.name)); }
static id cookie_value(id s, SEL c) { (void)c; return ar(ns(CK(s)->c.value)); }
static id cookie_domain(id s, SEL c) {
    (void)c;
    char b[300]; snprintf(b, sizeof b, "%s%s", CK(s)->c.host_only ? "" : ".", CK(s)->c.domain);
    return ar(ns(b));
}
static id cookie_path(id s, SEL c) { (void)c; return ar(ns(CK(s)->c.path)); }
static id cookie_expires(id s, SEL c) { (void)c; return CK(s)->c.persistent ? date_unix(CK(s)->c.expires) : 0; }
static BOOL cookie_secure(id s, SEL c) { (void)c; return CK(s)->c.secure; }
static BOOL cookie_httponly(id s, SEL c) { (void)c; return CK(s)->c.http_only; }
static BOOL cookie_session(id s, SEL c) { (void)c; return !CK(s)->c.persistent; }
static id cookie_samesite(id s, SEL c) {
    (void)c;
    int v = CK(s)->c.same_site;
    return v == MNET_SAMESITE_STRICT ? ar(ns("strict")) : v == MNET_SAMESITE_LAX ? ar(ns("lax")) : 0;
}
static unsigned long cookie_version(id s, SEL c) { (void)s; (void)c; return 0; }
static id cookie_nil(id s, SEL c) { (void)s; (void)c; return 0; }
static id cookie_props(id s, SEL c) {
    (void)c;
    NSHTTPCookie_t *o = CK(s);
    id d = mutdict();
    dset(d, "Name", ar(ns(o->c.name))); dset(d, "Value", ar(ns(o->c.value)));
    dset(d, "Domain", cookie_domain(s, c)); dset(d, "Path", ar(ns(o->c.path)));
    dset(d, "Version", ar(ns("0")));
    if (o->c.secure) dset(d, "Secure", ar(ns("TRUE")));
    if (o->c.http_only) dset(d, "HttpOnly", ar(ns("TRUE")));
    if (o->c.persistent) dset(d, "Expires", date_unix(o->c.expires)); else dset(d, "Discard", ar(ns("TRUE")));
    id ss = cookie_samesite(s, c); if (ss) dset(d, "SameSite", ss);
    return ar(d);
}
static id cookie_desc(id s, SEL c) {
    (void)c;
    char b[512];
    snprintf(b, sizeof b, "<NSHTTPCookie %s=%s; domain=%s%s; path=%s%s%s>", CK(s)->c.name, CK(s)->c.value,
             CK(s)->c.host_only ? "" : ".", CK(s)->c.domain, CK(s)->c.path, CK(s)->c.secure ? "; secure" : "",
             CK(s)->c.http_only ? "; HttpOnly" : "");
    return ar(ns(b));
}
static BOOL cookie_eq(id s, SEL c, id o) {
    (void)c;
    if (!o || object_getClass(o) != &NSHTTPCookie_c) return NO;
    return !strcmp(CK(s)->c.name, CK(o)->c.name) && mnet_ieq(CK(s)->c.domain, CK(o)->c.domain) && !strcmp(CK(s)->c.path, CK(o)->c.path);
}
static u64 cookie_hash(id s, SEL c) { (void)c; u64 h = 5381; for (const char *p = CK(s)->c.name; *p; p++) h = h * 33 + (u8)*p; return h; }
/* Set-Cookie juntados por ", " num dicionário: separa nas vírgulas seguidas de "token=" (Expires tem vírgula) */
static void split_set_cookie(const char *v, void (*fn)(void *, const char *, size_t), void *ctx) {
    const char *start = v, *p = v;
    while (*p) {
        if (*p == ',') {
            const char *q = p + 1;
            while (*q == ' ' || *q == '\t') q++;
            const char *t = q;
            while (*q && *q != '=' && *q != ';' && *q != ',' && *q != ' ') q++;
            if (*q == '=' && q > t) { fn(ctx, start, (size_t)(p - start)); start = p + 1; while (*start == ' ') start++; }
        }
        p++;
    }
    if (p > start) fn(ctx, start, (size_t)(p - start));
}
typedef struct { mnet_jar_t *jar; mnet_url_t *u; } sc_ctx_t;
static void sc_one(void *x, const char *s, size_t n) {
    sc_ctx_t *c = x;
    char *t = mnet_strndup(s, n);
    if (t) { mnet_jar_set(c->jar, t, c->u->scheme, c->u->host, c->u->target, 1, now_s()); free(t); }
}
static id cookies_with_fields(id k, SEL c, id fields, id url) {
    (void)k; (void)c;
    id out = mutarr();
    mnet_url_t u;
    if (!url || mnet_url_parse(cs(MSG0(id, url, "absoluteString")), &u)) return ar(out);
    mnet_jar_t tmp; mnet_jar_init(&tmp);
    sc_ctx_t ctx = {&tmp, &u};
    id keys = fields ? MSG0(id, fields, "allKeys") : 0;
    u64 n = keys ? MSG0(u64, keys, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id key = MSG(id, keys, "objectAtIndex:", i);
        if (!mnet_ieq(cs(key), "Set-Cookie")) continue;
        split_set_cookie(cs(MSG(id, fields, "objectForKey:", key)), sc_one, &ctx);
    }
    for (mnet_cookie_t *x = tmp.head; x; x = x->next) { id ck = cookie_from_c(x); MSG(void, out, "insertObject:atIndex:", ck, (u64)0); rel(ck); }
    mnet_jar_clear(&tmp);
    mnet_url_free(&u);
    return ar(out);
}
static id cookie_req_fields(id k, SEL c, id cookies) {
    (void)k; (void)c;
    id d = mutdict();
    u64 n = cookies ? MSG0(u64, cookies, "count") : 0;
    mnet_buf_t b = {0};
    for (u64 i = 0; i < n; i++) {
        id ck = MSG(id, cookies, "objectAtIndex:", i);
        if (b.n) mnet_buf_put(&b, "; ", 2);
        if (*CK(ck)->c.name) { mnet_buf_puts(&b, CK(ck)->c.name); mnet_buf_putc(&b, '='); }
        mnet_buf_puts(&b, CK(ck)->c.value);
    }
    if (b.n) { id v = ns((char *)b.p); dset(d, "Cookie", v); rel(v); }
    mnet_buf_free(&b);
    return ar(d);
}

/* ================================================================ NSHTTPCookieStorage */
typedef struct { cx_class_t *isa; mnet_jar_t *jar; int own, policy, loaded; char path[512]; } NSHTTPCookieStorage_t;
#define CS(o) ((NSHTTPCookieStorage_t *)(o))
static id g_shared_cookies;
static void storage_save(NSHTTPCookieStorage_t *st) {
    if (!st->path[0]) return;
    mnet_buf_t b = {0};
    LK(); mnet_jar_save(st->jar, &b, 0); ULK();
    char tmp[560]; snprintf(tmp, sizeof tmp, "%s.novo", st->path);
    int fd = open(tmp, 0x601, 0600);                       /* O_WRONLY | O_CREAT | O_TRUNC */
    if (fd >= 0) {
        long w = b.n ? write(fd, b.p, b.n) : 0;
        close(fd);
        if (w == (long)b.n) rename(tmp, st->path);
    }
    mnet_buf_free(&b);
}
static void storage_changed(id s) {
    if (!s) return;
    storage_save(CS(s));
    id center = MSG0(id, cls("NSNotificationCenter"), "defaultCenter");
    if (center) MSG(void, center, "postNotificationName:object:", NSHTTPCookieManagerCookiesChangedNotification, s);
}
static id cookies_shared(id k, SEL c) {
    (void)k; (void)c;
    if (g_shared_cookies) return g_shared_cookies;
    LK();
    if (!g_shared_cookies) {
        NSHTTPCookieStorage_t *st = (NSHTTPCookieStorage_t *)cx_alloc_instance(&NSHTTPCookieStorage_c);
        st->jar = cx_mnet_jar();
        const char *home = getenv("HOME");
        if (home) {
            char d[512];
            snprintf(d, sizeof d, "%s/Library", home); mkdir(d, 0755);
            snprintf(d, sizeof d, "%s/Library/Cookies", home); mkdir(d, 0700);
            snprintf(st->path, sizeof st->path, "%s/Library/Cookies/mos-cookies.txt", home);
            int fd = open(st->path, 0);
            if (fd >= 0) {
                mnet_buf_t b = {0}; char t[8192]; long n;
                while ((n = read(fd, t, sizeof t)) > 0) mnet_buf_put(&b, t, (size_t)n);
                close(fd);
                if (b.n) mnet_jar_load(st->jar, (char *)b.p, b.n, now_s());
                mnet_buf_free(&b);
            }
        }
        __sync_synchronize();
        g_shared_cookies = (id)st;
    }
    ULK();
    return g_shared_cookies;
}
static id storage_new_private(void) {
    NSHTTPCookieStorage_t *st = (NSHTTPCookieStorage_t *)cx_alloc_instance(&NSHTTPCookieStorage_c);
    st->jar = calloc(1, sizeof *st->jar);
    mnet_jar_init(st->jar);
    st->own = 1;
    return (id)st;
}
static id cookies_group(id k, SEL c, id ident) { (void)ident; return cookies_shared(k, c); }   /* um contêiner só no mOS */
static void storage_dealloc(id s, SEL c) {
    if (CS(s)->own) { mnet_jar_clear(CS(s)->jar); free(CS(s)->jar); }
    IMP sup = cx_lookup(&NSObject_c, c); if (sup) ((void (*)(id, SEL))sup)(s, c);
}
static id storage_cookies(id s, SEL c) {
    (void)c;
    id out = mutarr();
    LK();
    mnet_jar_expire(CS(s)->jar, now_s());
    for (mnet_cookie_t *x = CS(s)->jar->head; x; x = x->next) { id ck = cookie_from_c(x); MSG(void, out, "addObject:", ck); rel(ck); }
    ULK();
    return ar(out);
}
static id storage_for_url(id s, SEL c, id url) {
    (void)c;
    id out = mutarr();
    mnet_url_t u;
    if (!url || mnet_url_parse(cs(MSG0(id, url, "absoluteString")), &u)) return ar(out);
    LK();
    long long now = now_s();
    mnet_jar_expire(CS(s)->jar, now);
    for (mnet_cookie_t *x = CS(s)->jar->head; x; x = x->next) {
        if (x->host_only ? !mnet_ieq(u.host, x->domain) : !mnet_domain_match(u.host, x->domain)) continue;
        if (!mnet_path_match(u.target, x->path) && strcmp(x->path, "/")) continue;
        if (x->secure && strcmp(u.scheme, "https") && strcmp(u.scheme, "wss")) continue;
        id ck = cookie_from_c(x); MSG(void, out, "addObject:", ck); rel(ck);
    }
    ULK();
    mnet_url_free(&u);
    return ar(out);
}
static void storage_set(id s, SEL c, id ck) {
    (void)c;
    if (!ck || object_getClass(ck) != &NSHTTPCookie_c || CS(s)->policy == 1) return;
    LK(); mnet_jar_insert(CS(s)->jar, &CK(ck)->c, now_s()); ULK();
    storage_changed(s);
}
static void storage_set_many(id s, SEL c, id cookies, id url, id main) {
    (void)url;
    u64 n = cookies ? MSG0(u64, cookies, "count") : 0;
    if (CS(s)->policy == 1) return;
    const char *mh = 0; mnet_url_t mu; int have_main = 0;
    if (CS(s)->policy == 2 && main && !mnet_url_parse(cs(MSG0(id, main, "absoluteString")), &mu)) { have_main = 1; mh = mu.host; }
    LK();
    for (u64 i = 0; i < n; i++) {
        id ck = MSG(id, cookies, "objectAtIndex:", i);
        if (object_getClass(ck) != &NSHTTPCookie_c) continue;
        if (mh && !mnet_domain_match(mh, CK(ck)->c.domain) && !mnet_domain_match(CK(ck)->c.domain, mh)) continue;
        mnet_jar_insert(CS(s)->jar, &CK(ck)->c, now_s());
    }
    ULK();
    if (have_main) mnet_url_free(&mu);
    (void)c;
    if (n) storage_changed(s);
}
static void storage_del(id s, SEL c, id ck) {
    (void)c;
    if (!ck || object_getClass(ck) != &NSHTTPCookie_c) return;
    LK(); int r = mnet_jar_remove(CS(s)->jar, CK(ck)->c.name, CK(ck)->c.domain, CK(ck)->c.path); ULK();
    if (r) storage_changed(s);
}
static void storage_remove_since(id s, SEL c, id date) {
    (void)c;
    long long t = unix_of_date(date);
    LK();
    for (mnet_cookie_t *x = CS(s)->jar->head, *nx; x; x = nx) { nx = x->next; if (x->created >= t) mnet_jar_remove(CS(s)->jar, x->name, x->domain, x->path); }
    ULK();
    storage_changed(s);
}
static unsigned long storage_policy(id s, SEL c) { (void)c; return (unsigned long)CS(s)->policy; }
static void storage_set_policy(id s, SEL c, unsigned long p) {
    (void)c;
    CS(s)->policy = (int)p;
    id center = MSG0(id, cls("NSNotificationCenter"), "defaultCenter");
    if (center) MSG(void, center, "postNotificationName:object:", NSHTTPCookieManagerAcceptPolicyChangedNotification, s);
}
static id storage_sorted(id s, SEL c, id descs) { return MSG(id, storage_cookies(s, c), "sortedArrayUsingDescriptors:", descs); }
static void storage_store_task(id s, SEL c, id cookies, id task);
static void storage_get_task(id s, SEL c, id task, void *block);

/* ================================================================ credenciais */
typedef struct { cx_class_t *isa; id user, password, trust, identity, certs; unsigned long persistence; } NSURLCredential_t;
#define CRED(o) ((NSURLCredential_t *)(o))
static id cred_init(id s, SEL c, id u, id p, unsigned long pers) { (void)c; CRED(s)->user = retain(MSG0(id, u, "copy")); CRED(s)->password = retain(MSG0(id, p, "copy")); CRED(s)->persistence = pers; return s; }
static id cred_with(id k, SEL c, id u, id p, unsigned long pers) { (void)k; return ar(cred_init(cx_alloc_instance(&NSURLCredential_c), c, u, p, pers)); }
static id cred_init_trust(id s, SEL c, id t) { (void)c; CRED(s)->trust = retain(t); CRED(s)->persistence = 0; return s; }
static id cred_for_trust(id k, SEL c, id t) { (void)k; return ar(cred_init_trust(cx_alloc_instance(&NSURLCredential_c), c, t)); }
static id cred_init_ident(id s, SEL c, id ident, id certs, unsigned long pers) { (void)c; CRED(s)->identity = retain(ident); CRED(s)->certs = retain(certs); CRED(s)->persistence = pers; return s; }
static id cred_with_ident(id k, SEL c, id ident, id certs, unsigned long pers) { (void)k; return ar(cred_init_ident(cx_alloc_instance(&NSURLCredential_c), c, ident, certs, pers)); }
static id cred_user(id s, SEL c) { (void)c; return CRED(s)->user; }
static id cred_pass(id s, SEL c) { (void)c; return CRED(s)->password; }
static BOOL cred_has_pass(id s, SEL c) { (void)c; return CRED(s)->password != 0; }
static unsigned long cred_pers(id s, SEL c) { (void)c; return CRED(s)->persistence; }
static id cred_identity(id s, SEL c) { (void)c; return CRED(s)->identity; }
static id cred_certs(id s, SEL c) { (void)c; return CRED(s)->certs; }
static void cred_dealloc(id s, SEL c) {
    rel(CRED(s)->user); rel(CRED(s)->password); rel(CRED(s)->trust); rel(CRED(s)->identity); rel(CRED(s)->certs);
    IMP sup = cx_lookup(&NSObject_c, c); if (sup) ((void (*)(id, SEL))sup)(s, c);
}

typedef struct { cx_class_t *isa; id host, protocol, realm, method, trust, proxy_type, dn; long port; int proxy; } NSURLProtectionSpace_t;
#define PS(o) ((NSURLProtectionSpace_t *)(o))
static id space_init(id s, SEL c, id host, long port, id proto, id realm, id method) {
    (void)c;
    PS(s)->host = retain(host); PS(s)->port = port; PS(s)->protocol = retain(proto); PS(s)->realm = retain(realm);
    PS(s)->method = retain(method ? method : ns("NSURLAuthenticationMethodDefault"));
    return s;
}
static id space_init_proxy(id s, SEL c, id host, long port, id type, id realm, id method) {
    space_init(s, c, host, port, 0, realm, method);
    PS(s)->proxy = 1; PS(s)->proxy_type = retain(type);
    return s;
}
static id space_host(id s, SEL c) { (void)c; return PS(s)->host; }
static long space_port(id s, SEL c) { (void)c; return PS(s)->port; }
static id space_proto(id s, SEL c) { (void)c; return PS(s)->protocol; }
static id space_realm(id s, SEL c) { (void)c; return PS(s)->realm; }
static id space_method(id s, SEL c) { (void)c; return PS(s)->method; }
static id space_trust(id s, SEL c) { (void)c; return PS(s)->trust; }
static BOOL space_is_proxy(id s, SEL c) { (void)c; return PS(s)->proxy; }
static id space_proxy_type(id s, SEL c) { (void)c; return PS(s)->proxy_type; }
static BOOL space_secure(id s, SEL c) { (void)c; const char *p = cs(PS(s)->protocol); return p && (!strcmp(p, "https") || !strcmp(p, "wss")); }
static id space_dn(id s, SEL c) { (void)c; return PS(s)->dn; }
static void space_dealloc(id s, SEL c) {
    rel(PS(s)->host); rel(PS(s)->protocol); rel(PS(s)->realm); rel(PS(s)->method); rel(PS(s)->trust); rel(PS(s)->proxy_type); rel(PS(s)->dn);
    IMP sup = cx_lookup(&NSObject_c, c); if (sup) ((void (*)(id, SEL))sup)(s, c);
}
static int space_same(id a, id b) {
    if (!a || !b) return 0;
    const char *ha = cs(PS(a)->host), *hb = cs(PS(b)->host), *ra = cs(PS(a)->realm), *rb = cs(PS(b)->realm);
    const char *pa = cs(PS(a)->protocol), *pb = cs(PS(b)->protocol);
    return ha && hb && mnet_ieq(ha, hb) && PS(a)->port == PS(b)->port && PS(a)->proxy == PS(b)->proxy &&
           (!ra || !rb || !strcmp(ra, rb)) && (!pa || !pb || !strcmp(pa, pb));
}
static BOOL space_eq(id s, SEL c, id o) { (void)c; return o && object_getClass(o) == &NSURLProtectionSpace_c && space_same(s, o); }

typedef struct { cx_class_t *isa; id space, proposed, failure, error, sender; long prev; } NSURLAuthenticationChallenge_t;
#define CH(o) ((NSURLAuthenticationChallenge_t *)(o))
static id chal_init(id s, SEL c, id space, id cred, long prev, id resp, id err, id sender) {
    (void)c;
    CH(s)->space = retain(space); CH(s)->proposed = retain(cred); CH(s)->prev = prev;
    CH(s)->failure = retain(resp); CH(s)->error = retain(err); CH(s)->sender = retain(sender);
    return s;
}
static id chal_space(id s, SEL c) { (void)c; return CH(s)->space; }
static id chal_proposed(id s, SEL c) { (void)c; return CH(s)->proposed; }
static long chal_prev(id s, SEL c) { (void)c; return CH(s)->prev; }
static id chal_failure(id s, SEL c) { (void)c; return CH(s)->failure; }
static id chal_error(id s, SEL c) { (void)c; return CH(s)->error; }
static id chal_sender(id s, SEL c) { (void)c; return CH(s)->sender; }
static void chal_dealloc(id s, SEL c) {
    rel(CH(s)->space); rel(CH(s)->proposed); rel(CH(s)->failure); rel(CH(s)->error); rel(CH(s)->sender);
    IMP sup = cx_lookup(&NSObject_c, c); if (sup) ((void (*)(id, SEL))sup)(s, c);
}

/* NSURLCredentialStorage: por sessão em memória; permanente no Keychain (senha de internet: servidor + conta) */
typedef struct { cx_class_t *isa; } NSURLCredentialStorage_t;
typedef struct cred_ent { id space, cred; int is_default; struct cred_ent *next; } cred_ent_t;
static cred_ent_t *g_creds;
static id g_cred_storage;
static id credstore_shared(id k, SEL c) { (void)k; (void)c; if (!g_cred_storage) g_cred_storage = cx_alloc_instance(&NSURLCredentialStorage_c); return g_cred_storage; }
static id kc_query(id space, id user) {
    id q = mutdict();
    MSG(void, q, "setObject:forKey:", (id)kSecClassInternetPassword, (id)kSecClass);
    id srvr = ns("srvr"); MSG(void, q, "setObject:forKey:", PS(space)->host, srvr); rel(srvr);
    if (user) MSG(void, q, "setObject:forKey:", user, (id)kSecAttrAccount);
    return q;
}
static void credstore_set(id s, SEL c, id cred, id space, int dflt) {
    (void)s; (void)c;
    if (!cred || !space) return;
    LK();
    for (cred_ent_t **pp = &g_creds; *pp;) {
        cred_ent_t *e = *pp;
        int same_user = CRED(e->cred)->user && CRED(cred)->user && !strcmp(cs(CRED(e->cred)->user), cs(CRED(cred)->user));
        if (space_same(e->space, space) && (same_user || (dflt && e->is_default))) { *pp = e->next; rel(e->space); rel(e->cred); free(e); continue; }
        if (dflt && space_same(e->space, space)) e->is_default = 0;
        pp = &e->next;
    }
    cred_ent_t *e = calloc(1, sizeof *e);
    e->space = retain(space); e->cred = retain(cred); e->is_default = dflt || !g_creds; e->next = g_creds;
    g_creds = e;
    ULK();
    if (CRED(cred)->persistence == 2 && CRED(cred)->user && CRED(cred)->password) {      /* Permanent */
        id q = kc_query(space, CRED(cred)->user);
        SecItemDelete(q);
        const char *pw = cs(CRED(cred)->password);
        MSG(void, q, "setObject:forKey:", data_bytes(pw, strlen(pw)), (id)kSecValueData);
        SecItemAdd(q, 0);
        rel(q);
    }
}
static void credstore_set_cred(id s, SEL c, id cred, id space) { credstore_set(s, c, cred, space, 0); }
static void credstore_set_default(id s, SEL c, id cred, id space) { credstore_set(s, c, cred, space, 1); }
static id credstore_default(id s, SEL c, id space) {
    (void)s; (void)c;
    id found = 0;
    LK();
    for (cred_ent_t *e = g_creds; e; e = e->next) if (space_same(e->space, space) && (e->is_default || !found)) found = e->cred;
    ULK();
    if (found || !space) return found;
    /* Keychain */
    id q = kc_query(space, 0), res = 0;
    id yes = MSG(id, cls("NSNumber"), "numberWithBool:", (BOOL)YES);
    MSG(void, q, "setObject:forKey:", yes, (id)kSecReturnData);
    id ret_attr = ns("r_Attributes"); MSG(void, q, "setObject:forKey:", yes, ret_attr); rel(ret_attr);
    if (!SecItemCopyMatching(q, &res) && res) {
        id acct = MSG(id, res, "objectForKey:", (id)kSecAttrAccount), data = MSG(id, res, "objectForKey:", (id)kSecValueData);
        if (acct && data) {
            id pw = MSG(id, MSG0(id, cls("NSString"), "alloc"), "initWithData:encoding:", data, (u64)4);
            found = cred_with(0, 0, acct, pw, 2);
            rel(pw);
        }
        rel(res);
    }
    rel(q);
    return found;
}
static id credstore_for_space(id s, SEL c, id space) {
    (void)s; (void)c;
    id d = mutdict();
    LK();
    for (cred_ent_t *e = g_creds; e; e = e->next)
        if (space_same(e->space, space) && CRED(e->cred)->user) MSG(void, d, "setObject:forKey:", e->cred, CRED(e->cred)->user);
    ULK();
    return ar(d);
}
static id credstore_all(id s, SEL c) {
    (void)s; (void)c;
    id d = mutdict();
    LK();
    for (cred_ent_t *e = g_creds; e; e = e->next) {
        id inner = MSG(id, d, "objectForKey:", e->space);
        if (!inner) { inner = mutdict(); MSG(void, d, "setObject:forKey:", inner, e->space); rel(inner); }
        if (CRED(e->cred)->user) MSG(void, inner, "setObject:forKey:", e->cred, CRED(e->cred)->user);
    }
    ULK();
    return ar(d);
}
static void credstore_remove(id s, SEL c, id cred, id space) {
    (void)s; (void)c;
    LK();
    for (cred_ent_t **pp = &g_creds; *pp;) {
        cred_ent_t *e = *pp;
        if (space_same(e->space, space) && (e->cred == cred || (CRED(cred)->user && CRED(e->cred)->user && !strcmp(cs(CRED(cred)->user), cs(CRED(e->cred)->user))))) {
            *pp = e->next; rel(e->space); rel(e->cred); free(e); continue;
        }
        pp = &e->next;
    }
    ULK();
    if (CRED(cred)->user) { id q = kc_query(space, CRED(cred)->user); SecItemDelete(q); rel(q); }
}
static void credstore_remove_opts(id s, SEL c, id cred, id space, id opts) { (void)opts; credstore_remove(s, c, cred, space); }

/* ================================================================ NSURLCache (RFC 9111, memória + disco) */
typedef struct cache_ent {
    char *key, *vary;
    int status, version;
    mnet_hdrs_t hdrs, req_hdrs;
    uint8_t *body; size_t n;
    long long req_time, resp_time;
    unsigned long policy;            /* 0 permitido, 1 só memória, 2 não */
    id user_info;
    struct cache_ent *next;
} cache_ent_t;
typedef struct { cx_class_t *isa; cache_ent_t *head; u64 mem_cap, disk_cap, mem_use, disk_use; char dir[512]; int loaded; } NSURLCache_t;
#define UC(o) ((NSURLCache_t *)(o))
static id g_shared_cache;
static u64 fnv(const char *s) { u64 h = 1469598103934665603ull; for (; *s; s++) { h ^= (u8)*s; h *= 1099511628211ull; } return h; }
static void ent_free(cache_ent_t *e) { free(e->key); free(e->vary); mnet_hdrs_free(&e->hdrs); mnet_hdrs_free(&e->req_hdrs); free(e->body); rel(e->user_info); free(e); }
static void ent_path(NSURLCache_t *c, cache_ent_t *e, char *out, size_t n) {
    snprintf(out, n, "%s/%016llx-%016llx.mosc", c->dir, (unsigned long long)fnv(e->key), (unsigned long long)fnv(e->vary ? e->vary : ""));
}
/* arquivo: "MOSC1\n" status versão req_time resp_time \n url \n vary(base16) \n N cabeçalhos "nome\tvalor" \n
   M cabeçalhos do pedido \n tamanho \n corpo */
static void hex_put(mnet_buf_t *b, const char *s) { static const char h[] = "0123456789abcdef"; for (; *s; s++) { mnet_buf_putc(b, h[(u8)*s >> 4]); mnet_buf_putc(b, h[(u8)*s & 15]); } }
static void disk_write(NSURLCache_t *c, cache_ent_t *e) {
    if (!c->dir[0] || e->policy != 0 || !c->disk_cap) return;
    mnet_buf_t b = {0};
    mnet_buf_puts(&b, "MOSC1\n");
    mnet_buf_putu(&b, (unsigned)e->status); mnet_buf_putc(&b, ' '); mnet_buf_putu(&b, (unsigned)e->version); mnet_buf_putc(&b, ' ');
    mnet_buf_putu(&b, (unsigned long long)e->req_time); mnet_buf_putc(&b, ' '); mnet_buf_putu(&b, (unsigned long long)e->resp_time); mnet_buf_putc(&b, '\n');
    mnet_buf_puts(&b, e->key); mnet_buf_putc(&b, '\n');
    hex_put(&b, e->vary ? e->vary : ""); mnet_buf_putc(&b, '\n');
    const mnet_hdrs_t *hs[2] = {&e->hdrs, &e->req_hdrs};
    for (int k = 0; k < 2; k++) {
        mnet_buf_putu(&b, (unsigned)hs[k]->n); mnet_buf_putc(&b, '\n');
        for (int i = 0; i < hs[k]->n; i++) { mnet_buf_puts(&b, hs[k]->v[i].name); mnet_buf_putc(&b, '\t'); mnet_buf_puts(&b, hs[k]->v[i].value); mnet_buf_putc(&b, '\n'); }
    }
    mnet_buf_putu(&b, (unsigned long long)e->n); mnet_buf_putc(&b, '\n');
    mnet_buf_put(&b, e->body, e->n);
    char p[600], t[620];
    ent_path(c, e, p, sizeof p);
    snprintf(t, sizeof t, "%s.novo", p);
    int fd = open(t, 0x601, 0600);
    if (fd >= 0) { long w = write(fd, b.p, b.n); close(fd); if (w == (long)b.n) { rename(t, p); c->disk_use += b.n; } }
    mnet_buf_free(&b);
}
static int hexv(int ch) { return ch >= '0' && ch <= '9' ? ch - '0' : (ch | 32) - 'a' + 10; }
static char *line_at(char **p, char *end) { char *s = *p, *nl = s; while (nl < end && *nl != '\n') nl++; if (nl >= end) return 0; *nl = 0; *p = nl + 1; return s; }
static cache_ent_t *disk_read(const char *path) {
    int fd = open(path, 0);
    if (fd < 0) return 0;
    mnet_buf_t b = {0}; char t[16384]; long n;
    while ((n = read(fd, t, sizeof t)) > 0) mnet_buf_put(&b, t, (size_t)n);
    close(fd);
    cache_ent_t *e = 0;
    char *p = (char *)b.p, *end = p ? p + b.n : 0, *l;
    if (!p || !(l = line_at(&p, end)) || strcmp(l, "MOSC1")) goto out;
    e = calloc(1, sizeof *e);
    if (!(l = line_at(&p, end))) goto bad;
    { long long v[4] = {0}; int k = 0; for (char *q = l; *q && k < 4; ) { while (*q == ' ') q++; while (*q >= '0' && *q <= '9') v[k] = v[k] * 10 + (*q++ - '0'); k++; }
      e->status = (int)v[0]; e->version = (int)v[1]; e->req_time = v[2]; e->resp_time = v[3]; }
    if (!(l = line_at(&p, end))) goto bad;
    e->key = dupc(l);
    if (!(l = line_at(&p, end))) goto bad;
    { size_t hl = strlen(l) / 2; e->vary = malloc(hl + 1); for (size_t i = 0; i < hl; i++) e->vary[i] = (char)(hexv(l[2 * i]) << 4 | hexv(l[2 * i + 1])); e->vary[hl] = 0; }
    mnet_hdrs_t *hs[2] = {&e->hdrs, &e->req_hdrs};
    for (int k = 0; k < 2; k++) {
        if (!(l = line_at(&p, end))) goto bad;
        int cnt = atoi(l);
        for (int i = 0; i < cnt; i++) {
            if (!(l = line_at(&p, end))) goto bad;
            char *tab = strchr(l, '\t');
            if (!tab) goto bad;
            mnet_hdrs_add(hs[k], l, (size_t)(tab - l), tab + 1, strlen(tab + 1));
        }
    }
    if (!(l = line_at(&p, end))) goto bad;
    e->n = (size_t)strtoll(l, 0, 10);
    if ((size_t)(end - p) < e->n) goto bad;
    e->body = malloc(e->n ? e->n : 1);
    memcpy(e->body, p, e->n);
    goto out;
bad:
    if (e) { ent_free(e); e = 0; }
out:
    mnet_buf_free(&b);
    return e;
}
static void cache_trim(NSURLCache_t *c) {
    /* memória: tira os mais velhos (fim da lista) até caber */
    while (c->mem_use > c->mem_cap && c->head) {
        cache_ent_t **pp = &c->head;
        while ((*pp)->next) pp = &(*pp)->next;
        cache_ent_t *e = *pp; *pp = 0;
        c->mem_use -= e->n;
        ent_free(e);
    }
}
static cache_ent_t *cache_lookup(NSURLCache_t *c, const char *key, const mnet_hdrs_t *req) {
    for (cache_ent_t *e = c->head; e; e = e->next) {
        if (strcmp(e->key, key)) continue;
        char *vk = mnet_cache_vary_key(&e->hdrs, req);
        int ok = !strcmp(vk ? vk : "", e->vary ? e->vary : "");
        free(vk);
        if (ok) return e;
    }
    /* disco: a chave do Vary depende dos cabeçalhos guardados; procura o arquivo sem Vary e o com o Vary atual */
    if (!c->dir[0]) return 0;
    char p[600];
    cache_ent_t probe = {0};
    probe.key = (char *)key; probe.vary = "";
    ent_path(c, &probe, p, sizeof p);
    cache_ent_t *e = disk_read(p);
    if (e && mnet_hdrs_get(&e->hdrs, "vary")) {
        char *vk = mnet_cache_vary_key(&e->hdrs, req);
        probe.vary = vk ? vk : "";
        ent_free(e); e = 0;
        ent_path(c, &probe, p, sizeof p);
        e = disk_read(p);
        if (e && strcmp(e->vary, probe.vary)) { ent_free(e); e = 0; }
        free(vk);
    }
    if (!e || strcmp(e->key, key)) { if (e) ent_free(e); return 0; }
    e->next = c->head; c->head = e; c->mem_use += e->n;
    cache_trim(c);
    return c->head == e ? e : 0;
}
static void cache_put(NSURLCache_t *c, cache_ent_t *e) {
    if (e->policy == 2 || (e->n > c->mem_cap / 20 && e->n > c->disk_cap / 20)) { ent_free(e); return; }   /* > 5%: não guarda */
    for (cache_ent_t **pp = &c->head; *pp; pp = &(*pp)->next)
        if (!strcmp((*pp)->key, e->key) && !strcmp((*pp)->vary ? (*pp)->vary : "", e->vary ? e->vary : "")) {
            cache_ent_t *o = *pp; *pp = o->next; c->mem_use -= o->n; ent_free(o); break;
        }
    disk_write(c, e);
    if (e->n <= c->mem_cap / 20) { e->next = c->head; c->head = e; c->mem_use += e->n; cache_trim(c); }
    else ent_free(e);
}
static void cache_remove_key(NSURLCache_t *c, const char *key) {
    for (cache_ent_t **pp = &c->head; *pp;) {
        cache_ent_t *e = *pp;
        if (!strcmp(e->key, key)) {
            char p[600]; ent_path(c, e, p, sizeof p); unlink(p);
            *pp = e->next; c->mem_use -= e->n; ent_free(e); continue;
        }
        pp = &e->next;
    }
    char p[600]; cache_ent_t probe = {0}; probe.key = (char *)key; probe.vary = ""; ent_path(c, &probe, p, sizeof p); unlink(p);
}
static const char *req_key(id req) { id u = MSG0(id, req, "URL"); return u ? cs(MSG0(id, u, "absoluteString")) : 0; }
static id cache_init(id s, SEL c, u64 mem, u64 disk, id path) {
    (void)c;
    UC(s)->mem_cap = mem; UC(s)->disk_cap = disk;
    const char *p = path ? cs(path) : 0;
    if (p && *p) {
        if (p[0] == '/') snprintf(UC(s)->dir, sizeof UC(s)->dir, "%s", p);
        else {
            const char *home = getenv("HOME");
            if (home) snprintf(UC(s)->dir, sizeof UC(s)->dir, "%s/Library/Caches/%s", home, p);
        }
        char tmp[512];
        snprintf(tmp, sizeof tmp, "%s", UC(s)->dir);
        for (char *q = tmp + 1; *q; q++) if (*q == '/') { *q = 0; mkdir(tmp, 0755); *q = '/'; }
        mkdir(tmp, 0700);
    }
    return s;
}
static id cache_init_dir(id s, SEL c, u64 mem, u64 disk, id url) { return cache_init(s, c, mem, disk, url ? MSG0(id, url, "path") : 0); }
static id cache_shared(id k, SEL c) {
    (void)k; (void)c;
    if (g_shared_cache) return g_shared_cache;
    LK();
    if (!g_shared_cache) {
        id cc = cx_alloc_instance(&NSURLCache_c);
        id bid = 0, b = MSG0(id, cls("NSBundle"), "mainBundle");
        if (b) bid = MSG0(id, b, "bundleIdentifier");
        char dir[300];
        snprintf(dir, sizeof dir, "%s/mos-urlcache", bid ? cs(bid) : "mOS");
        id d = ns(dir);
        ULK();
        cache_init(cc, 0, 4u << 20, 20u << 20, d);             /* 4 MB / 20 MB, como o padrão do macOS */
        LK();
        rel(d);
        if (!g_shared_cache) g_shared_cache = cc; else rel(cc);
    }
    ULK();
    return g_shared_cache;
}
static void cache_set_shared(id k, SEL c, id cache) { (void)k; (void)c; LK(); id old = g_shared_cache; g_shared_cache = retain(cache); ULK(); rel(old); }
static u64 cache_memcap(id s, SEL c) { (void)c; return UC(s)->mem_cap; }
static u64 cache_diskcap(id s, SEL c) { (void)c; return UC(s)->disk_cap; }
static void cache_set_memcap(id s, SEL c, u64 v) { (void)c; LK(); UC(s)->mem_cap = v; cache_trim(UC(s)); ULK(); }
static void cache_set_diskcap(id s, SEL c, u64 v) { (void)c; UC(s)->disk_cap = v; }
static u64 cache_memuse(id s, SEL c) { (void)c; return UC(s)->mem_use; }
static u64 cache_diskuse(id s, SEL c) { (void)c; return UC(s)->disk_use; }

typedef struct { cx_class_t *isa; id resp, data, info; unsigned long policy; } NSCachedURLResponse_t;
#define CR(o) ((NSCachedURLResponse_t *)(o))
static id cached_init4(id s, SEL c, id resp, id data, id info, unsigned long pol) { (void)c; CR(s)->resp = retain(resp); CR(s)->data = retain(data); CR(s)->info = retain(info); CR(s)->policy = pol; return s; }
static id cached_init(id s, SEL c, id resp, id data) { return cached_init4(s, c, resp, data, 0, 0); }
static id cached_resp(id s, SEL c) { (void)c; return CR(s)->resp; }
static id cached_data(id s, SEL c) { (void)c; return CR(s)->data; }
static id cached_info(id s, SEL c) { (void)c; return CR(s)->info; }
static unsigned long cached_policy(id s, SEL c) { (void)c; return CR(s)->policy; }
static void cached_dealloc(id s, SEL c) { rel(CR(s)->resp); rel(CR(s)->data); rel(CR(s)->info); IMP sup = cx_lookup(&NSObject_c, c); if (sup) ((void (*)(id, SEL))sup)(s, c); }
static id cached_from_ent(const cache_ent_t *e) {
    id resp = make_response(e->key, e->status, e->version, &e->hdrs);
    id data = data_bytes(e->body, e->n);
    id o = cached_init4(cx_alloc_instance(&NSCachedURLResponse_c), 0, resp, data, e->user_info, e->policy);
    rel(resp);
    return o;
}
static id cache_get(id s, SEL c, id req) {
    (void)c;
    const char *k = req ? req_key(req) : 0;
    if (!k) return 0;
    mnet_hdrs_t rh = {0};
    dict_hdrs(MSG0(id, req, "allHTTPHeaderFields"), &rh);
    LK();
    cache_ent_t *e = cache_lookup(UC(s), k, &rh);
    id out = e ? cached_from_ent(e) : 0;
    ULK();
    mnet_hdrs_free(&rh);
    return ar(out);
}
static void cache_store(id s, SEL c, id cached, id req) {
    (void)c;
    const char *k = req ? req_key(req) : 0;
    if (!k || !cached) return;
    cache_ent_t *e = calloc(1, sizeof *e);
    e->key = dupc(k);
    id resp = CR(cached)->resp;
    response_hdrs(resp, &e->hdrs, &e->status);
    e->version = 11;
    dict_hdrs(MSG0(id, req, "allHTTPHeaderFields"), &e->req_hdrs);
    e->vary = mnet_cache_vary_key(&e->hdrs, &e->req_hdrs);
    id d = CR(cached)->data;
    e->n = d ? (size_t)MSG0(u64, d, "length") : 0;
    e->body = malloc(e->n ? e->n : 1);
    if (e->n) memcpy(e->body, MSG0(const void *, d, "bytes"), e->n);
    e->req_time = e->resp_time = now_s();
    e->policy = CR(cached)->policy;
    e->user_info = retain(CR(cached)->info);
    LK(); cache_put(UC(s), e); ULK();
}
static void cache_remove(id s, SEL c, id req) { (void)c; const char *k = req ? req_key(req) : 0; if (!k) return; LK(); cache_remove_key(UC(s), k); ULK(); }
static void cache_remove_all(id s, SEL c) {
    (void)c;
    LK();
    for (cache_ent_t *e = UC(s)->head, *n; e; e = n) { n = e->next; char p[600]; ent_path(UC(s), e, p, sizeof p); unlink(p); ent_free(e); }
    UC(s)->head = 0; UC(s)->mem_use = 0;
    ULK();
    if (UC(s)->dir[0]) {
        /* os arquivos que não estão na memória: apaga o diretório inteiro de entradas */
        void *d = opendir(UC(s)->dir);
        if (d) {
            void *ent;
            while ((ent = readdir(d))) {
                const char *nm = (const char *)ent + 21;                    /* dirent do macOS: d_name no offset 21 */
                size_t l = strlen(nm);
                if (l > 5 && !strcmp(nm + l - 5, ".mosc")) { char p[800]; snprintf(p, sizeof p, "%s/%s", UC(s)->dir, nm); unlink(p); }
            }
            closedir(d);
        }
        UC(s)->disk_use = 0;
    }
}
static void cache_remove_since(id s, SEL c, id date) {
    (void)c;
    long long t = unix_of_date(date);
    LK();
    for (cache_ent_t **pp = &UC(s)->head; *pp;) {
        cache_ent_t *e = *pp;
        if (e->resp_time >= t) { char p[600]; ent_path(UC(s), e, p, sizeof p); unlink(p); *pp = e->next; UC(s)->mem_use -= e->n; ent_free(e); continue; }
        pp = &e->next;
    }
    ULK();
}

/* ================================================================ configuração */
typedef struct {
    cx_class_t *isa;
    id identifier, headers, cookies, cache, creds, proxy, protocols, shared_container;
    double t_req, t_res;
    unsigned long policy, accept_policy, service_type, tls_min, tls_max, multipath;
    long max_per_host;
    BOOL set_cookies, cellular, expensive, constrained, waits, discretionary, pipelining, launch_events, ephemeral, background;
} NSURLSessionConfiguration_t;
#define CF(o) ((NSURLSessionConfiguration_t *)(o))
static id cfg_new(int kind) {
    NSURLSessionConfiguration_t *c = (NSURLSessionConfiguration_t *)cx_alloc_instance(&NSURLSessionConfiguration_c);
    c->t_req = 60; c->t_res = 7 * 86400; c->max_per_host = 6;
    c->set_cookies = YES; c->cellular = c->expensive = c->constrained = YES; c->accept_policy = 2;
    c->tls_min = 0x0303; c->tls_max = 0x0304;
    c->ephemeral = kind == 1; c->background = kind == 2;
    c->cookies = retain(kind == 1 ? storage_new_private() : cookies_shared(0, 0));
    if (kind == 1) rel(c->cookies);
    if (kind == 1) { c->cache = cx_alloc_instance(&NSURLCache_c); cache_init(c->cache, 0, 4u << 20, 0, 0); }
    else c->cache = retain(cache_shared(0, 0));
    c->creds = retain(credstore_shared(0, 0));
    return (id)c;
}
static id cfg_default(id k, SEL c) { (void)k; (void)c; return ar(cfg_new(0)); }
static id cfg_ephemeral(id k, SEL c) { (void)k; (void)c; return ar(cfg_new(1)); }
static id cfg_background(id k, SEL c, id ident) { (void)k; (void)c; id o = cfg_new(2); CF(o)->identifier = retain(MSG0(id, ident, "copy")); return ar(o); }
static id cfg_copy(id s, SEL c) {
    (void)c;
    NSURLSessionConfiguration_t *o = (NSURLSessionConfiguration_t *)cx_alloc_instance(&NSURLSessionConfiguration_c), *x = CF(s);
    memcpy((u8 *)o + 8, (u8 *)x + 8, sizeof *o - 8);
    id *ids[] = {&o->identifier, &o->headers, &o->cookies, &o->cache, &o->creds, &o->proxy, &o->protocols, &o->shared_container};
    for (unsigned i = 0; i < sizeof ids / sizeof *ids; i++) *ids[i] = retain(*ids[i]);
    return (id)o;
}
static id cfg_copyz(id s, SEL c, void *z) { (void)z; return cfg_copy(s, c); }
static void cfg_dealloc(id s, SEL c) {
    NSURLSessionConfiguration_t *x = CF(s);
    rel(x->identifier); rel(x->headers); rel(x->cookies); rel(x->cache); rel(x->creds); rel(x->proxy); rel(x->protocols); rel(x->shared_container);
    IMP sup = cx_lookup(&NSObject_c, c); if (sup) ((void (*)(id, SEL))sup)(s, c);
}
#define CFG_ID(get, set, f) \
    static id get(id s, SEL c) { (void)c; return CF(s)->f; } \
    static void set(id s, SEL c, id v) { (void)c; id o = CF(s)->f; CF(s)->f = retain(v); rel(o); }
#define CFG_V(get, set, f, T) \
    static T get(id s, SEL c) { (void)c; return CF(s)->f; } \
    static void set(id s, SEL c, T v) { (void)c; CF(s)->f = v; }
CFG_ID(cfg_ident, cfg_set_ident, identifier) CFG_ID(cfg_headers, cfg_set_headers, headers) CFG_ID(cfg_cookies, cfg_set_cookies_store, cookies)
CFG_ID(cfg_cache, cfg_set_cache, cache) CFG_ID(cfg_creds, cfg_set_creds, creds) CFG_ID(cfg_proxy, cfg_set_proxy, proxy)
CFG_ID(cfg_protocols, cfg_set_protocols, protocols) CFG_ID(cfg_container, cfg_set_container, shared_container)
CFG_V(cfg_treq, cfg_set_treq, t_req, double) CFG_V(cfg_tres, cfg_set_tres, t_res, double)
CFG_V(cfg_policy, cfg_set_policy, policy, unsigned long) CFG_V(cfg_accept, cfg_set_accept, accept_policy, unsigned long)
CFG_V(cfg_service, cfg_set_service, service_type, unsigned long) CFG_V(cfg_tlsmin, cfg_set_tlsmin, tls_min, unsigned long)
CFG_V(cfg_tlsmax, cfg_set_tlsmax, tls_max, unsigned long) CFG_V(cfg_mptcp, cfg_set_mptcp, multipath, unsigned long)
CFG_V(cfg_maxhost, cfg_set_maxhost, max_per_host, long) CFG_V(cfg_setck, cfg_set_setck, set_cookies, BOOL)
CFG_V(cfg_cell, cfg_set_cell, cellular, BOOL) CFG_V(cfg_exp, cfg_set_exp, expensive, BOOL) CFG_V(cfg_cons, cfg_set_cons, constrained, BOOL)
CFG_V(cfg_waits, cfg_set_waits, waits, BOOL) CFG_V(cfg_disc, cfg_set_disc, discretionary, BOOL) CFG_V(cfg_pipe, cfg_set_pipe, pipelining, BOOL)
CFG_V(cfg_launch, cfg_set_launch, launch_events, BOOL)

/* proxy para a URL: o dicionário da configuração ou, sem ele, o do sistema (exceções, HTTP/HTTPS, SOCKS: cfproxy.c) */
char *cx_proxy_for(id settings, const char *url);
static char *cfg_proxy_for(id cfg, const char *url) { return cx_proxy_for(cfg ? CF(cfg)->proxy : 0, url); }

/* ================================================================ sessão e tarefas */
typedef struct task task_t;
typedef struct wsq { id msg; void *blk; struct wsq *next; } wsq_t;
typedef struct wsx {
    wsq_t *send_head, *send_tail;      /* mensagens a mandar + blocos de conclusão */
    wsq_t *recv_head, *recv_tail;      /* mensagens recebidas ainda sem leitor */
    wsq_t *wait_head, *wait_tail;      /* receiveMessage esperando mensagem */
    wsq_t *pong_head, *pong_tail;      /* sendPing esperando pong */
    int pings_pending;
    int close_code, want_close; id close_reason, my_reason; long max_msg;
    char *protocols;
    int open, done;
} wsx_t;
typedef struct {
    cx_class_t *isa;
    id cfg, delegate, op_queue, desc;
    void *queue;                       /* fila serial dos callbacks */
    int invalid, finishing, is_shared;
    task_t *tasks;                     /* em andamento */
    unsigned long next_id;
} NSURLSession_t;
#define SS(o) ((NSURLSession_t *)(o))
enum { T_DATA, T_UPLOAD, T_DOWNLOAD, T_STREAM, T_WS };
enum { ST_RUNNING, ST_SUSPENDED, ST_CANCELING, ST_COMPLETED };
struct task {
    cx_class_t *isa;
    id session, orig_req, cur_req, response, error, desc, delegate, metrics, earliest;
    void *handler;                     /* bloco de conclusão (ou NULL: delegate) */
    int kind, state, started, cancel_flag, resume_data_wanted;
    volatile int cancel;
    long long sent, recv, to_send, to_recv;
    float priority;
    unsigned long ident;
    id upload_data, upload_file;       /* corpo do envio */
    int upload_fd;
    id resume_data_in;                 /* download retomado */
    long long resume_offset;
    id body;                           /* NSMutableData (bloco de conclusão) */
    char tmp_path[600]; int tmp_fd;    /* download */
    reply_t **replies; int nreplies;   /* blocos nossos entregues ao app */
    void *resume_block;                /* cancelByProducingResumeData: */
    void *suspend_sem;
    id protocol_obj;                   /* NSURLProtocol do app atendendo esta tarefa */
    int cache_hit;
    struct wsx *wsx;                   /* NSURLSessionWebSocketTask */
    task_t *next;
};
typedef task_t NSURLSessionTask_t;
typedef task_t NSURLSessionDataTask_t;
typedef task_t NSURLSessionUploadTask_t;
typedef task_t NSURLSessionDownloadTask_t;
typedef task_t NSURLSessionStreamTask_t;
typedef task_t NSURLSessionWebSocketTask_t;
#define TK(o) ((task_t *)(o))

static void *sess_queue(id s) {
    if (SS(s)->queue) return SS(s)->queue;
    id mq = MSG0(id, cls("NSOperationQueue"), "mainQueue");
    SS(s)->queue = SS(s)->op_queue && SS(s)->op_queue == mq ? dispatch_get_main_queue()
                                                           : dispatch_queue_create("com.mos.NSURLSession.delegate", 0);
    return SS(s)->queue;
}
/* roda fn(ctx) na fila do delegate; wait = espera terminar (o fio da tarefa não é a fila) */
typedef struct { void (*fn)(void *); void *ctx; void *sem; } hop_t;
static void hop_run(void *x) {
    hop_t *h = x;
    void *pool = objc_autoreleasePoolPush();
    h->fn(h->ctx);
    objc_autoreleasePoolPop(pool);
    if (h->sem) dispatch_semaphore_signal(h->sem); else free(h);
}
static void on_queue(id sess, void (*fn)(void *), void *ctx, int wait) {
    hop_t *h = calloc(1, sizeof *h);
    h->fn = fn; h->ctx = ctx;
    if (wait) h->sem = dispatch_semaphore_create(0);
    dispatch_async_f(sess_queue(sess), h, hop_run);
    if (wait) { dispatch_semaphore_wait(h->sem, DISPATCH_TIME_FOREVER); dispatch_release(h->sem); free(h); }
}
static id task_delegate_for(task_t *t, const char *sel) {
    if (t->delegate && responds(t->delegate, sel)) return t->delegate;
    id d = SS(t->session)->delegate;
    return d && responds(d, sel) ? d : 0;
}
static reply_t *task_reply(task_t *t, void *invoke) {
    reply_t *r = reply_new(invoke);
    LK();
    t->replies = realloc(t->replies, (size_t)(t->nreplies + 1) * sizeof *t->replies);
    t->replies[t->nreplies++] = r;
    ULK();
    return r;
}
static void reply_wait(reply_t *r) { dispatch_semaphore_wait(r->sem, DISPATCH_TIME_FOREVER); }

/* ---- objetos das tarefas */
static id task_alloc_kind(id sess, int kind, id req) {
    cx_class_t *k = kind == T_WS ? &NSURLSessionWebSocketTask_c : kind == T_UPLOAD ? &NSURLSessionUploadTask_c : kind == T_DOWNLOAD ? &NSURLSessionDownloadTask_c :
                    kind == T_STREAM ? &NSURLSessionStreamTask_c : &NSURLSessionDataTask_c;
    task_t *t = (task_t *)cx_alloc_instance(k);
    t->kind = kind;
    t->state = ST_SUSPENDED;
    t->session = retain(sess);
    t->orig_req = retain(MSG0(id, req, "copy"));
    rel(t->orig_req);
    t->cur_req = retain(t->orig_req);
    t->to_recv = -1; t->to_send = -1;
    t->priority = 0.5f;
    t->upload_fd = -1; t->tmp_fd = -1;
    LK(); t->ident = ++SS(sess)->next_id; ULK();
    return (id)t;
}
static void task_dealloc(id s, SEL c) {
    task_t *t = TK(s);
    rel(t->session); rel(t->orig_req); rel(t->cur_req); rel(t->response); rel(t->error); rel(t->desc); rel(t->delegate);
    rel(t->metrics); rel(t->earliest); rel(t->upload_data); rel(t->upload_file); rel(t->resume_data_in); rel(t->body); rel(t->protocol_obj);
    if (t->handler) _Block_release(t->handler);
    if (t->resume_block) _Block_release(t->resume_block);
    for (int i = 0; i < t->nreplies; i++) { rel(t->replies[i]->obj); dispatch_release(t->replies[i]->sem); free(t->replies[i]); }
    free(t->replies);
    if (t->suspend_sem) dispatch_release(t->suspend_sem);
    if (t->wsx) {
        wsq_t **qs[4] = {&t->wsx->send_head, &t->wsx->recv_head, &t->wsx->wait_head, &t->wsx->pong_head};
        for (int i = 0; i < 4; i++) for (wsq_t *e = *qs[i], *n; e; e = n) { n = e->next; rel(e->msg); if (e->blk) _Block_release(e->blk); free(e); }
        rel(t->wsx->close_reason); rel(t->wsx->my_reason); free(t->wsx->protocols); free(t->wsx);
    }
    IMP sup = cx_lookup(&NSObject_c, c); if (sup) ((void (*)(id, SEL))sup)(s, c);
}
static unsigned long task_ident(id s, SEL c) { (void)c; return TK(s)->ident; }
static id task_orig(id s, SEL c) { (void)c; return TK(s)->orig_req; }
static id task_cur(id s, SEL c) { (void)c; return TK(s)->cur_req; }
static id task_resp(id s, SEL c) { (void)c; return TK(s)->response; }
static id task_err(id s, SEL c) { (void)c; return TK(s)->error; }
static long task_state(id s, SEL c) { (void)c; return TK(s)->state; }
static long long task_recv(id s, SEL c) { (void)c; return TK(s)->recv; }
static long long task_sent(id s, SEL c) { (void)c; return TK(s)->sent; }
static long long task_to_send(id s, SEL c) { (void)c; return TK(s)->to_send; }
static long long task_to_recv(id s, SEL c) { (void)c; return TK(s)->to_recv; }
static float task_prio(id s, SEL c) { (void)c; return TK(s)->priority; }
static void task_set_prio(id s, SEL c, float p) { (void)c; TK(s)->priority = p; }
static id task_descr(id s, SEL c) { (void)c; return TK(s)->desc; }
static void task_set_descr(id s, SEL c, id d) { (void)c; id o = TK(s)->desc; TK(s)->desc = retain(MSG0(id, d, "copy")); rel(TK(s)->desc); rel(o); }
static id task_delegate(id s, SEL c) { (void)c; return TK(s)->delegate; }
static void task_set_delegate(id s, SEL c, id d) { (void)c; id o = TK(s)->delegate; TK(s)->delegate = retain(d); rel(o); }
static id task_earliest(id s, SEL c) { (void)c; return TK(s)->earliest; }
static void task_set_earliest(id s, SEL c, id d) { (void)c; id o = TK(s)->earliest; TK(s)->earliest = retain(d); rel(o); }
static id task_progress(id s, SEL c) {
    (void)c;
    id p = MSG(id, cls("NSProgress"), "progressWithTotalUnitCount:", (long long)(TK(s)->to_recv > 0 ? TK(s)->to_recv : -1));
    if (p && responds(p, "setCompletedUnitCount:")) MSG(void, p, "setCompletedUnitCount:", TK(s)->recv);
    return p;
}

/* ---- corpo do envio */
static long body_read_cb(void *ctx, uint8_t *b, size_t n) {
    task_t *t = ctx;
    if (t->upload_fd >= 0) return read(t->upload_fd, b, n);
    id st = t->upload_data;                                  /* NSInputStream */
    if (!st) return 0;
    long k = MSG(long, st, "read:maxLength:", b, (u64)n);
    return k < 0 ? -1 : k;
}
static int body_rewind_cb(void *ctx) {
    task_t *t = ctx;
    if (t->upload_fd >= 0) { close(t->upload_fd); t->upload_fd = open(cs(MSG0(id, t->upload_file, "path")), 0); return t->upload_fd < 0; }
    /* fluxo: pede um novo ao delegate (URLSession:task:needNewBodyStream:) */
    id d = task_delegate_for(t, "URLSession:task:needNewBodyStream:");
    if (!d) return 1;
    reply_t *r = task_reply(t, (void *)inv_obj);
    MSG(void, d, "URLSession:task:needNewBodyStream:", t->session, (id)t, (void *)r);
    reply_wait(r);
    if (!r->obj) return 1;
    rel(t->upload_data); t->upload_data = retain(r->obj);
    MSG0(void, t->upload_data, "open");
    return 0;
}

/* ---- trabalho de uma tarefa (fio próprio) */
typedef struct {
    task_t *t;
    creq_t rq;
    mnet_resp_t r;
    int disp;                          /* 1 entrega, 0 cancela, 2 vira download */
    int auth_retry, auth_fail_count;
    char *auth_header;                 /* Authorization calculado */
    int tls_cancel;
    long long fetch_start, resp_end;
    int status;
    mnet_hdrs_t store_hdrs;            /* resposta para o cache */
    mnet_buf_t cache_body; int cacheable;
    cache_ent_t *revalidating;         /* entrada usada no pedido condicional */
    int from_304;
} job_t;

typedef struct { task_t *t; id a, b, c; long long x, y, z; int i; } ev_t;
static ev_t *ev_new(task_t *t) { ev_t *e = calloc(1, sizeof *e); e->t = t; return e; }
static void ev_free(ev_t *e) { rel(e->a); rel(e->b); rel(e->c); free(e); }

static void ev_data(void *x) {
    ev_t *e = x;
    id d = task_delegate_for(e->t, "URLSession:dataTask:didReceiveData:");
    if (d) MSG(void, d, "URLSession:dataTask:didReceiveData:", e->t->session, (id)e->t, e->a);
    ev_free(e);
}
static void ev_write(void *x) {
    ev_t *e = x;
    id d = task_delegate_for(e->t, "URLSession:downloadTask:didWriteData:totalBytesWritten:totalBytesExpectedToWrite:");
    if (d) MSG(void, d, "URLSession:downloadTask:didWriteData:totalBytesWritten:totalBytesExpectedToWrite:", e->t->session, (id)e->t, e->x, e->y, e->z);
    ev_free(e);
}
static void ev_sent(void *x) {
    ev_t *e = x;
    id d = task_delegate_for(e->t, "URLSession:task:didSendBodyData:totalBytesSent:totalBytesExpectedToSend:");
    if (d) MSG(void, d, "URLSession:task:didSendBodyData:totalBytesSent:totalBytesExpectedToSend:", e->t->session, (id)e->t, e->x, e->y, e->z);
    ev_free(e);
}
static void ev_resumed(void *x) {
    ev_t *e = x;
    id d = task_delegate_for(e->t, "URLSession:downloadTask:didResumeAtOffset:expectedTotalBytes:");
    if (d) MSG(void, d, "URLSession:downloadTask:didResumeAtOffset:expectedTotalBytes:", e->t->session, (id)e->t, e->x, e->y);
    ev_free(e);
}
/* espera enquanto a tarefa estiver suspensa (suspend com a transferência em curso) */
static void wait_suspended(task_t *t) {
    while (t->state == ST_SUSPENDED && !t->cancel) usleep(20000);
}
static int use_handler(task_t *t) { return t->handler != 0; }

static int j_body(void *ctx, const uint8_t *d, size_t n) {
    job_t *j = ctx;
    task_t *t = j->t;
    wait_suspended(t);
    if (t->cancel) return 1;
    t->recv += (long long)n;
    if (j->cacheable) mnet_buf_put(&j->cache_body, d, n);
    if (t->kind == T_DOWNLOAD || j->disp == 2) {
        if (t->tmp_fd < 0) return 1;
        if (write(t->tmp_fd, d, n) != (long)n) return 1;
        if (!use_handler(t)) { ev_t *e = ev_new(t); e->x = (long long)n; e->y = t->recv + t->resume_offset; e->z = t->to_recv; on_queue(t->session, ev_write, e, 0); }
        return 0;
    }
    if (use_handler(t)) { MSG(void, t->body, "appendBytes:length:", d, (u64)n); return 0; }
    ev_t *e = ev_new(t);
    e->a = retain(data_bytes(d, n));
    on_queue(t->session, ev_data, e, 0);
    return 0;
}
static void j_sent(void *ctx, long long sent, long long total) {
    job_t *j = ctx;
    task_t *t = j->t;
    long long delta = sent - t->sent;
    t->sent = sent; t->to_send = total;
    if (!use_handler(t) || task_delegate_for(t, "URLSession:task:didSendBodyData:totalBytesSent:totalBytesExpectedToSend:")) {
        ev_t *e = ev_new(t); e->x = delta; e->y = sent; e->z = total; on_queue(t->session, ev_sent, e, 0);
    }
}

/* ---- autenticação HTTP (Basic e Digest, RFC 7617 / RFC 7616) */
static void b64(const uint8_t *in, size_t n, mnet_buf_t *o) {
    static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for (size_t i = 0; i < n; i += 3) {
        u32 v = (u32)in[i] << 16 | (i + 1 < n ? (u32)in[i + 1] << 8 : 0) | (i + 2 < n ? in[i + 2] : 0);
        mnet_buf_putc(o, T[v >> 18 & 63]); mnet_buf_putc(o, T[v >> 12 & 63]);
        mnet_buf_putc(o, i + 1 < n ? T[v >> 6 & 63] : '='); mnet_buf_putc(o, i + 2 < n ? T[v & 63] : '=');
    }
}
/* lê o parâmetro p de um desafio (realm="x", nonce=...) */
static int chal_param(const char *s, const char *p, char *out, size_t on) {
    size_t pl = strlen(p);
    for (const char *q = s; *q; q++) {
        if ((q == s || q[-1] == ' ' || q[-1] == ',') && mnet_ieqn(q, p, pl) && q[pl] == '=') {
            q += pl + 1;
            size_t k = 0;
            if (*q == '"') { q++; while (*q && *q != '"' && k + 1 < on) { if (*q == '\\' && q[1]) q++; out[k++] = *q++; } }
            else while (*q && *q != ',' && *q != ' ' && k + 1 < on) out[k++] = *q++;
            out[k] = 0;
            return 1;
        }
    }
    return 0;
}
static void hexs(const uint8_t *d, size_t n, char *o) { static const char h[] = "0123456789abcdef"; for (size_t i = 0; i < n; i++) { o[2 * i] = h[d[i] >> 4]; o[2 * i + 1] = h[d[i] & 15]; } o[2 * n] = 0; }
static void hashs(int sha, const char *s, char *out) {
    uint8_t md[32];
    if (sha) { CC_SHA256(s, (unsigned)strlen(s), md); hexs(md, 32, out); }
    else { CC_MD5(s, (unsigned)strlen(s), md); hexs(md, 16, out); }
}
static char *auth_value(const char *scheme_line, const char *user, const char *pass, const char *method, const char *uri, int nc) {
    mnet_buf_t o = {0};
    if (mnet_ieqn(scheme_line, "basic", 5)) {
        mnet_buf_t up = {0};
        mnet_buf_puts(&up, user); mnet_buf_putc(&up, ':'); mnet_buf_puts(&up, pass);
        mnet_buf_puts(&o, "Basic "); b64(up.p, up.n, &o);
        mnet_buf_free(&up);
        return (char *)o.p;
    }
    if (!mnet_ieqn(scheme_line, "digest", 6)) return 0;
    char realm[256] = "", nonce[256] = "", opaque[256] = "", qop[64] = "", alg[32] = "MD5";
    chal_param(scheme_line + 6, "realm", realm, sizeof realm);
    chal_param(scheme_line + 6, "nonce", nonce, sizeof nonce);
    int has_opaque = chal_param(scheme_line + 6, "opaque", opaque, sizeof opaque);
    chal_param(scheme_line + 6, "qop", qop, sizeof qop);
    chal_param(scheme_line + 6, "algorithm", alg, sizeof alg);
    int sha = mnet_ieqn(alg, "SHA-256", 7);
    int sess = strstr(alg, "-sess") || strstr(alg, "-SESS");
    if (!sha && !mnet_ieqn(alg, "MD5", 3)) return 0;
    char cnonce[33]; uint8_t rnd[16]; arc4random_buf(rnd, sizeof rnd); hexs(rnd, sizeof rnd, cnonce);
    char ha1[65], ha2[65], resp[65], ncs[9], buf[1600];
    snprintf(buf, sizeof buf, "%s:%s:%s", user, realm, pass); hashs(sha, buf, ha1);
    if (sess) { snprintf(buf, sizeof buf, "%s:%s:%s", ha1, nonce, cnonce); hashs(sha, buf, ha1); }
    snprintf(buf, sizeof buf, "%s:%s", method, uri); hashs(sha, buf, ha2);
    snprintf(ncs, sizeof ncs, "%08x", nc);
    int use_qop = qop[0] && mnet_tok_has(qop, "auth");
    if (use_qop) snprintf(buf, sizeof buf, "%s:%s:%s:%s:auth:%s", ha1, nonce, ncs, cnonce, ha2);
    else snprintf(buf, sizeof buf, "%s:%s:%s", ha1, nonce, ha2);
    hashs(sha, buf, resp);
    snprintf(buf, sizeof buf, "Digest username=\"%s\", realm=\"%s\", nonce=\"%s\", uri=\"%s\", algorithm=%s, response=\"%s\"",
             user, realm, nonce, uri, alg, resp);
    mnet_buf_puts(&o, buf);
    if (use_qop) { snprintf(buf, sizeof buf, ", qop=auth, nc=%s, cnonce=\"%s\"", ncs, cnonce); mnet_buf_puts(&o, buf); }
    if (has_opaque) { snprintf(buf, sizeof buf, ", opaque=\"%s\"", opaque); mnet_buf_puts(&o, buf); }
    return (char *)o.p;
}
/* escolhe o melhor desafio suportado: Digest SHA-256 > Digest MD5 > Basic */
static char *pick_challenge(const mnet_hdrs_t *h, const char *name, const char **method_name) {
    const char *best = 0; int score = 0;
    for (int i = 0; i < h->n; i++) {
        if (!mnet_ieq(h->v[i].name, name)) continue;
        const char *v = h->v[i].value;
        int sc = 0;
        if (mnet_ieqn(v, "digest", 6)) { char a[32] = "MD5"; chal_param(v + 6, "algorithm", a, sizeof a); sc = mnet_ieqn(a, "SHA-256", 7) ? 3 : mnet_ieqn(a, "MD5", 3) ? 2 : 0; }
        else if (mnet_ieqn(v, "basic", 5)) sc = 1;
        if (sc > score) { score = sc; best = v; }
    }
    if (!best) return 0;
    *method_name = score == 1 ? "NSURLAuthenticationMethodHTTPBasic" : "NSURLAuthenticationMethodHTTPDigest";
    return dupc(best);
}
typedef struct { task_t *t; id chal; reply_t *r; int session_level; } chal_ev_t;
static void ev_challenge(void *x) {
    chal_ev_t *c = x;
    if (c->session_level) MSG(void, SS(c->t->session)->delegate, "URLSession:didReceiveChallenge:completionHandler:", c->t->session, c->chal, (void *)c->r);
    else {
        id d = task_delegate_for(c->t, "URLSession:task:didReceiveChallenge:completionHandler:");
        MSG(void, d, "URLSession:task:didReceiveChallenge:completionHandler:", c->t->session, (id)c->t, c->chal, (void *)c->r);
    }
}
/* pergunta ao delegate: devolve a disposição (0 usar, 1 padrão, 2 cancelar, 3 rejeitar) e a credencial */
static long ask_challenge(task_t *t, id chal, int session_ok, id *cred) {
    int sess = 0;
    if (!task_delegate_for(t, "URLSession:task:didReceiveChallenge:completionHandler:")) {
        if (!session_ok || !SS(t->session)->delegate || !responds(SS(t->session)->delegate, "URLSession:didReceiveChallenge:completionHandler:")) return 1;
        sess = 1;
    }
    chal_ev_t ce = {t, chal, task_reply(t, (void *)inv_disp_obj), sess};
    on_queue(t->session, ev_challenge, &ce, 0);
    reply_wait(ce.r);
    *cred = ce.r->obj;
    return ce.r->disp;
}
static id new_space(const char *host, int port, const char *proto, const char *realm, const char *method, int proxy) {
    id s = cx_alloc_instance(&NSURLProtectionSpace_c);
    space_init(s, 0, ar(ns(host)), port, proto ? ar(ns(proto)) : 0, realm && *realm ? ar(ns(realm)) : 0, ar(ns(method)));
    PS(s)->proxy = proxy;
    return s;
}
static int j_tls(void *ctx, const char *host, int port, void *trust) {
    job_t *j = ctx;
    task_t *t = j->t;
    if (!task_delegate_for(t, "URLSession:task:didReceiveChallenge:completionHandler:") &&
        !(SS(t->session)->delegate && responds(SS(t->session)->delegate, "URLSession:didReceiveChallenge:completionHandler:"))) return 0;
    id space = new_space(host, port, "https", 0, "NSURLAuthenticationMethodServerTrust", 0);
    PS(space)->trust = retain((id)trust);
    id chal = MSG(id, cx_alloc_instance(&NSURLAuthenticationChallenge_c), "initWithProtectionSpace:proposedCredential:previousFailureCount:failureResponse:error:sender:",
                  space, (id)0, (long)0, (id)0, (id)0, (id)0);
    id cred = 0;
    long disp = ask_challenge(t, chal, 1, &cred);
    rel(chal); rel(space);
    /* o kernel já validou a cadeia, o nome e as datas: usar a credencial ou o padrão seguem; cancelar para */
    if (disp == 2) { j->tls_cancel = 1; return 1; }
    return 0;
}
static void rel_trust(void *t) { rel((id)t); }

/* ---- resposta, redirect */
typedef struct { task_t *t; id resp; reply_t *r; } resp_ev_t;
static void ev_response(void *x) {
    resp_ev_t *e = x;
    id d = task_delegate_for(e->t, "URLSession:dataTask:didReceiveResponse:completionHandler:");
    MSG(void, d, "URLSession:dataTask:didReceiveResponse:completionHandler:", e->t->session, (id)e->t, e->resp, (void *)e->r);
}
static int j_response(void *ctx, const mnet_resp_t *r) {
    job_t *j = ctx;
    task_t *t = j->t;
    j->status = r->status;
    /* 401/407: tenta credencial antes de entregar a resposta */
    if (r->status == 401 || r->status == 407) {
        const char *mname = 0;
        char *ch = pick_challenge(&r->hdrs, r->status == 401 ? "www-authenticate" : "proxy-authenticate", &mname);
        if (ch) {
            mnet_url_t u; char realm[256] = "";
            chal_param(strchr(ch, ' ') ? strchr(ch, ' ') : ch, "realm", realm, sizeof realm);
            int okurl = !mnet_url_parse(r->url, &u);
            id space = new_space(okurl ? u.host : "", okurl ? u.port : 0, okurl ? u.scheme : "http", realm, mname, r->status == 407);
            id store = CF(SS(t->session)->cfg)->creds;
            id cred = 0;
            /* 1: usuário:senha da própria URL na primeira tentativa */
            if (!j->auth_fail_count && okurl && u.userinfo) {
                char *col = strchr(u.userinfo, ':');
                if (col) { *col = 0; cred = cred_with(0, 0, ar(ns(u.userinfo)), ar(ns(col + 1)), 0); *col = ':'; }
            }
            if (!cred && !j->auth_fail_count && store) cred = credstore_default(store, 0, space);
            if (!cred) {
                id resp_obj = make_response(r->url, r->status, r->version, &r->hdrs);
                id chal = MSG(id, cx_alloc_instance(&NSURLAuthenticationChallenge_c), "initWithProtectionSpace:proposedCredential:previousFailureCount:failureResponse:error:sender:",
                              space, store ? credstore_default(store, 0, space) : (id)0, (long)j->auth_fail_count, resp_obj, (id)0, (id)0);
                id c2 = 0;
                long disp = ask_challenge(t, chal, 0, &c2);
                rel(chal); rel(resp_obj);
                if (disp == 0) cred = c2;
                else if (disp == 1 && store && !j->auth_fail_count) cred = credstore_default(store, 0, space);
                else if (disp == 2) { rel(space); free(ch); if (okurl) mnet_url_free(&u); t->cancel = 1; return 1; }
            }
            if (cred && CRED(cred)->user && CRED(cred)->password && j->auth_fail_count < 3) {
                const char *path = okurl ? u.target : "/";
                free(j->auth_header);
                j->auth_header = auth_value(ch, cs(CRED(cred)->user), cs(CRED(cred)->password), j->rq.method, path, j->auth_fail_count + 1);
                if (j->auth_header) {
                    j->auth_retry = r->status;
                    j->auth_fail_count++;
                    if (store && CRED(cred)->persistence != 0) credstore_set(store, 0, cred, space, 1);
                    rel(space); free(ch); if (okurl) mnet_url_free(&u);
                    return 1;                                         /* refaz o pedido com Authorization */
                }
            }
            rel(space); free(ch); if (okurl) mnet_url_free(&u);
        }
    }
    id resp = make_response(r->url, r->status, r->version, &r->hdrs);
    LK(); rel(t->response); t->response = resp; ULK();
    t->to_recv = r->expected_len;
    if (mnet_hdrs_get(&r->hdrs, "content-encoding")) t->to_recv = -1;
    if (t->kind == T_DOWNLOAD && t->resume_offset && r->status != 206) t->resume_offset = 0;
    j->disp = 1;
    if (t->kind == T_DATA && !use_handler(t) && task_delegate_for(t, "URLSession:dataTask:didReceiveResponse:completionHandler:")) {
        resp_ev_t e = {t, resp, task_reply(t, (void *)inv_disp)};
        on_queue(t->session, ev_response, &e, 0);
        reply_wait(e.r);
        j->disp = (int)e.r->disp;
        if (j->disp == 0) { t->cancel = 1; return 1; }
    }
    if (t->kind == T_DOWNLOAD || j->disp == 2) {
        const char *tmpd = cs(MSG0(id, cls("NSString"), "string"));
        (void)tmpd;
        id td = 0;
        { extern id NSTemporaryDirectory(void); td = NSTemporaryDirectory(); }
        snprintf(t->tmp_path, sizeof t->tmp_path, "%s/CFNetworkDownload_XXXXXX", td ? cs(td) : "/tmp");
        if (t->resume_offset && t->resume_data_in) {
            id info = MSG(id, cls("NSJSONSerialization"), "JSONObjectWithData:options:error:", t->resume_data_in, (u64)0, (id *)0);
            id p = info ? dget(info, "NSURLSessionResumeInfoLocalPath") : 0;
            if (p) snprintf(t->tmp_path, sizeof t->tmp_path, "%s", cs(p));
            t->tmp_fd = open(t->tmp_path, 0x0009, 0600);                  /* O_WRONLY | O_APPEND */
            ev_t *e = ev_new(t); e->x = t->resume_offset; e->y = t->to_recv >= 0 ? t->to_recv + t->resume_offset : -1;
            if (!use_handler(t)) on_queue(t->session, ev_resumed, e, 0); else ev_free(e);
        } else t->tmp_fd = mkstemp(t->tmp_path);
        if (t->tmp_fd < 0) return 1;
        if (t->to_recv >= 0 && t->resume_offset) t->to_recv += t->resume_offset;
    }
    /* cache: guarda se a resposta permite (o corpo é juntado em j_body) */
    j->cacheable = mnet_cache_storable(j->rq.method, r->status, &j->rq.hdrs, &r->hdrs) && CF(SS(t->session)->cfg)->cache && !j->revalidating;
    if (j->cacheable) { mnet_hdrs_free(&j->store_hdrs); for (int i = 0; i < r->hdrs.n; i++) mnet_hdrs_add(&j->store_hdrs, r->hdrs.v[i].name, strlen(r->hdrs.v[i].name), r->hdrs.v[i].value, strlen(r->hdrs.v[i].value)); }
    return 0;
}
typedef struct { task_t *t; id resp, req; reply_t *r; } redir_ev_t;
static void ev_redirect(void *x) {
    redir_ev_t *e = x;
    id d = task_delegate_for(e->t, "URLSession:task:willPerformHTTPRedirection:newRequest:completionHandler:");
    MSG(void, d, "URLSession:task:willPerformHTTPRedirection:newRequest:completionHandler:", e->t->session, (id)e->t, e->resp, e->req, (void *)e->r);
}
static int j_redirect(void *ctx, const mnet_resp_t *r, char **url) {
    job_t *j = ctx;
    task_t *t = j->t;
    id d = task_delegate_for(t, "URLSession:task:willPerformHTTPRedirection:newRequest:completionHandler:");
    id nu = MSG(id, cls("NSURL"), "URLWithString:", ar(ns(*url)));
    id nr = MSG0(id, t->cur_req, "mutableCopy");
    MSG(void, nr, "setURL:", nu);
    int to_get = (r->status == 303 && !mnet_ieq(j->rq.method, "HEAD")) || ((r->status == 301 || r->status == 302) && mnet_ieq(j->rq.method, "POST"));
    if (to_get) { MSG(void, nr, "setHTTPMethod:", ar(ns("GET"))); MSG(void, nr, "setHTTPBody:", (id)0); }
    if (!d || (use_handler(t) && !t->delegate && !SS(t->session)->delegate)) {
        LK(); rel(t->cur_req); t->cur_req = nr; ULK();
        return 1;
    }
    id resp = make_response(r->url, r->status, r->version, &r->hdrs);
    redir_ev_t e = {t, resp, nr, task_reply(t, (void *)inv_obj)};
    on_queue(t->session, ev_redirect, &e, 0);
    reply_wait(e.r);
    rel(resp);
    if (!e.r->obj) { rel(nr); return 0; }                         /* nil: entrega o 3xx */
    id u2 = MSG0(id, e.r->obj, "URL");
    free(*url);
    *url = dupc(u2 ? cs(MSG0(id, u2, "absoluteString")) : "");
    LK(); rel(t->cur_req); t->cur_req = retain(e.r->obj); ULK();
    rel(nr);
    return 1;
}

/* ---- conclusão */
static void task_finish_list(task_t *t) {
    NSURLSession_t *s = SS(t->session);
    LK();
    for (task_t **pp = &s->tasks; *pp; pp = &(*pp)->next) if (*pp == t) { *pp = t->next; break; }
    ULK();
}
static void ev_complete(void *x) {
    task_t *t = x;
    id sess = t->session;
    if (t->kind == T_DOWNLOAD && !t->error && t->tmp_path[0]) {
        id loc = MSG(id, cls("NSURL"), "fileURLWithPath:", ar(ns(t->tmp_path)));
        if (t->handler) call_3(t->handler, loc, t->response, 0);
        else {
            id d = task_delegate_for(t, "URLSession:downloadTask:didFinishDownloadingToURL:");
            if (d) MSG(void, d, "URLSession:downloadTask:didFinishDownloadingToURL:", sess, (id)t, loc);
        }
        unlink(t->tmp_path);                                      /* o app tinha de mover o arquivo durante o callback */
    } else if (t->handler) {
        if (t->kind == T_DOWNLOAD) call_3(t->handler, 0, t->response, t->error);
        else call_3(t->handler, t->error ? 0 : t->body, t->error ? (t->response ? t->response : 0) : t->response, t->error);
    } else if (t->kind == T_DOWNLOAD && t->tmp_path[0] && !t->resume_data_wanted) unlink(t->tmp_path);
    if (t->resume_block) {
        id rd = 0;
        if (t->kind == T_DOWNLOAD && t->tmp_path[0] && t->recv + t->resume_offset > 0) {
            id info = mutdict();
            dset(info, "NSURLSessionDownloadURL", MSG0(id, MSG0(id, t->orig_req, "URL"), "absoluteString"));
            dset(info, "NSURLSessionResumeInfoLocalPath", ar(ns(t->tmp_path)));
            dset(info, "NSURLSessionResumeBytesReceived", MSG(id, cls("NSNumber"), "numberWithLongLong:", t->recv + t->resume_offset));
            id resp = t->response;
            if (resp && responds(resp, "valueForHTTPHeaderField:")) {
                id et = MSG(id, resp, "valueForHTTPHeaderField:", ar(ns("ETag")));
                id lm = MSG(id, resp, "valueForHTTPHeaderField:", ar(ns("Last-Modified")));
                if (et) dset(info, "NSURLSessionResumeEntityTag", et);
                if (lm) dset(info, "NSURLSessionResumeServerDownloadDate", lm);
            }
            rd = MSG(id, cls("NSJSONSerialization"), "dataWithJSONObject:options:error:", info, (u64)0, (id *)0);
            rel(info);
        }
        call_1(t->resume_block, rd);
    }
    if (!t->handler || task_delegate_for(t, "URLSession:task:didCompleteWithError:")) {
        id d = task_delegate_for(t, "URLSession:task:didFinishCollectingMetrics:");
        if (d && t->metrics) MSG(void, d, "URLSession:task:didFinishCollectingMetrics:", sess, (id)t, t->metrics);
        d = task_delegate_for(t, "URLSession:task:didCompleteWithError:");
        if (d) MSG(void, d, "URLSession:task:didCompleteWithError:", sess, (id)t, t->error);
    }
    task_finish_list(t);
    /* finishTasksAndInvalidate: a última tarefa avisa o delegate e solta a sessão */
    NSURLSession_t *s = SS(sess);
    if (s->finishing && !s->tasks && !s->invalid) {
        s->invalid = 1;
        if (s->delegate && responds(s->delegate, "URLSession:didBecomeInvalidWithError:"))
            MSG(void, s->delegate, "URLSession:didBecomeInvalidWithError:", sess, (id)0);
        rel(s->delegate); s->delegate = 0;
    }
    rel((id)t);
}

typedef struct { cx_class_t *isa; id task_interval, txs; long redirects; } NSURLSessionTaskMetrics_t;
typedef struct { cx_class_t *isa; id req, resp, fetch_start, resp_end, proto; long resource_fetch_type; BOOL reused, proxy; } NSURLSessionTaskTransactionMetrics_t;
static id metrics_tx(id s, SEL c) { (void)c; return ((NSURLSessionTaskMetrics_t *)s)->txs; }
static id metrics_interval(id s, SEL c) { (void)c; return ((NSURLSessionTaskMetrics_t *)s)->task_interval; }
static long metrics_redirects(id s, SEL c) { (void)c; return ((NSURLSessionTaskMetrics_t *)s)->redirects; }
#define TXM(o) ((NSURLSessionTaskTransactionMetrics_t *)(o))
static id txm_req(id s, SEL c) { (void)c; return TXM(s)->req; }
static id txm_resp(id s, SEL c) { (void)c; return TXM(s)->resp; }
static id txm_fetch(id s, SEL c) { (void)c; return TXM(s)->fetch_start; }
static id txm_end(id s, SEL c) { (void)c; return TXM(s)->resp_end; }
static id txm_proto(id s, SEL c) { (void)c; return TXM(s)->proto; }
static BOOL txm_reused(id s, SEL c) { (void)c; return TXM(s)->reused; }
static BOOL txm_proxy(id s, SEL c) { (void)c; return TXM(s)->proxy; }
static long txm_type(id s, SEL c) { (void)c; return TXM(s)->resource_fetch_type; }
static void make_metrics(task_t *t, job_t *j, double t0, double t1) {
    NSURLSessionTaskMetrics_t *m = (NSURLSessionTaskMetrics_t *)cx_alloc_instance(&NSURLSessionTaskMetrics_c);
    m->redirects = j->r.redirects;
    id dic = cls("NSDateInterval");
    id d0 = MSG(id, cls("NSDate"), "dateWithTimeIntervalSince1970:", t0), d1 = MSG(id, cls("NSDate"), "dateWithTimeIntervalSince1970:", t1);
    if (dic) m->task_interval = MSG(id, MSG0(id, dic, "alloc"), "initWithStartDate:endDate:", d0, d1);
    NSURLSessionTaskTransactionMetrics_t *x = (NSURLSessionTaskTransactionMetrics_t *)cx_alloc_instance(&NSURLSessionTaskTransactionMetrics_c);
    x->req = retain(t->cur_req); x->resp = retain(t->response); x->fetch_start = retain(d0); x->resp_end = retain(d1);
    x->proto = j->r.alpn[0] ? retain(ns(j->r.alpn)) : retain(ns(j->r.version == 10 ? "http/1.0" : "http/1.1"));
    rel(x->proto);
    x->reused = (BOOL)j->r.reused;
    x->resource_fetch_type = t->cache_hit ? 2 : 1;                /* LocalCache : NetworkLoad */
    m->txs = MSG(id, cls("NSArray"), "arrayWithObject:", (id)x);
    m->txs = retain(m->txs);
    rel((id)x);
    rel(t->metrics);
    t->metrics = (id)m;
}

/* ---- NSURLProtocol: classes registradas (globais e da configuração) */
static id g_protocols;                                           /* NSMutableArray, a última registrada primeiro */
typedef struct { cx_class_t *isa; id req, cached, client; task_t *task; job_t *job; } NSURLProtocol_t;
typedef struct { cx_class_t *isa; task_t *t; job_t *j; void *done; int finished; } MOSURLProtocolClient_t;
#define PR(o) ((NSURLProtocol_t *)(o))
#define PC(o) ((MOSURLProtocolClient_t *)(o))
static id find_protocol(task_t *t, id req) {
    id lists[2] = {CF(SS(t->session)->cfg)->protocols, g_protocols};
    for (int l = 0; l < 2; l++) {
        id a = lists[l];
        u64 n = a ? MSG0(u64, a, "count") : 0;
        for (u64 i = 0; i < n; i++) {
            id k = MSG(id, a, "objectAtIndex:", i);
            if (responds(k, "canInitWithRequest:") && MSG(BOOL, k, "canInitWithRequest:", req)) return k;
        }
    }
    return 0;
}
static void pc_resp(id s, SEL c, id proto, id resp, unsigned long pol) {
    (void)c; (void)proto; (void)pol;
    task_t *t = PC(s)->t;
    LK(); rel(t->response); t->response = retain(resp); ULK();
    t->to_recv = responds(resp, "expectedContentLength") ? MSG0(long long, resp, "expectedContentLength") : -1;
    if (t->kind == T_DOWNLOAD) { snprintf(t->tmp_path, sizeof t->tmp_path, "/tmp/CFNetworkDownload_XXXXXX"); t->tmp_fd = mkstemp(t->tmp_path); }
    PC(s)->j->disp = 1;
}
static void pc_data(id s, SEL c, id proto, id data) {
    (void)c; (void)proto;
    u64 n = data ? MSG0(u64, data, "length") : 0;
    if (n) j_body(PC(s)->j, MSG0(const uint8_t *, data, "bytes"), n);
}
static void pc_finish(id s, SEL c, id proto) { (void)c; (void)proto; if (!PC(s)->finished) { PC(s)->finished = 1; dispatch_semaphore_signal(PC(s)->done); } }
static void pc_fail(id s, SEL c, id proto, id err) {
    (void)c; (void)proto;
    task_t *t = PC(s)->t;
    LK(); rel(t->error); t->error = retain(err); ULK();
    pc_finish(s, c, proto);
}
static void pc_redirect(id s, SEL c, id proto, id req, id resp) {
    (void)c; (void)proto; (void)resp;
    task_t *t = PC(s)->t;
    LK(); rel(t->cur_req); t->cur_req = retain(req); ULK();
}
static void pc_cached(id s, SEL c, id proto, id cached) {
    pc_resp(s, c, proto, CR(cached)->resp, 0);
    pc_data(s, c, proto, CR(cached)->data);
    pc_finish(s, c, proto);
}
static void pc_chal(id s, SEL c, id proto, id chal) { (void)s; (void)c; (void)proto; (void)chal; }
static int run_protocol(task_t *t, job_t *j, id pk) {
    MOSURLProtocolClient_t *cl = (MOSURLProtocolClient_t *)cx_alloc_instance(&MOSURLProtocolClient_c);
    cl->t = t; cl->j = j; cl->done = dispatch_semaphore_create(0);
    id req = t->cur_req;
    if (responds(pk, "canonicalRequestForRequest:")) req = MSG(id, pk, "canonicalRequestForRequest:", req);
    id p = MSG(id, MSG0(id, pk, "alloc"), "initWithRequest:cachedResponse:client:", req, (id)0, (id)cl);
    if (!p) { rel((id)cl); return -1; }
    rel(t->protocol_obj); t->protocol_obj = p;
    MSG0(void, p, "startLoading");
    while (dispatch_semaphore_wait(cl->done, 50ull * 1000000ull) && !t->cancel) {}
    if (t->cancel && !cl->finished) MSG0(void, p, "stopLoading");
    else MSG0(void, p, "stopLoading");
    dispatch_release(cl->done);
    rel((id)cl);
    return t->cancel ? MNET_E_CANCELLED : 0;
}
static id proto_init(id s, SEL c, id req, id cached, id client) { (void)c; PR(s)->req = retain(req); PR(s)->cached = retain(cached); PR(s)->client = retain(client); return s; }
static id proto_init_task(id s, SEL c, id task, id cached, id client) { proto_init(s, c, TK(task)->cur_req, cached, client); PR(s)->task = TK(task); return s; }
static id proto_req(id s, SEL c) { (void)c; return PR(s)->req; }
static id proto_cached(id s, SEL c) { (void)c; return PR(s)->cached; }
static id proto_client(id s, SEL c) { (void)c; return PR(s)->client; }
static id proto_task(id s, SEL c) { (void)c; return (id)PR(s)->task; }
static void proto_dealloc(id s, SEL c) { rel(PR(s)->req); rel(PR(s)->cached); rel(PR(s)->client); IMP sup = cx_lookup(&NSObject_c, c); if (sup) ((void (*)(id, SEL))sup)(s, c); }
static BOOL proto_can(id k, SEL c, id r) { (void)k; (void)c; (void)r; return NO; }
static id proto_canon(id k, SEL c, id r) { (void)k; (void)c; return r; }
static BOOL proto_equiv(id k, SEL c, id a, id b) { (void)k; (void)c; return MSG(BOOL, a, "isEqual:", b); }
static void proto_noop(id s, SEL c) { (void)s; (void)c; }
static BOOL proto_register(id k, SEL c, id pk) {
    (void)k; (void)c;
    LK();
    if (!g_protocols) g_protocols = mutarr();
    MSG(void, g_protocols, "removeObject:", pk);
    MSG(void, g_protocols, "insertObject:atIndex:", pk, (u64)0);
    ULK();
    return YES;
}
static void proto_unregister(id k, SEL c, id pk) { (void)k; (void)c; LK(); if (g_protocols) MSG(void, g_protocols, "removeObject:", pk); ULK(); }
static const char g_prop_key = 0;
static id prop_dict(id req, int create) {
    id d = objc_getAssociatedObject(req, &g_prop_key);
    if (!d && create) { d = mutdict(); objc_setAssociatedObject(req, &g_prop_key, d, 1); rel(d); }
    return d;
}
static id proto_prop(id k, SEL c, id key, id req) { (void)k; (void)c; id d = prop_dict(req, 0); return d ? MSG(id, d, "objectForKey:", key) : 0; }
static void proto_set_prop(id k, SEL c, id v, id key, id req) { (void)k; (void)c; MSG(void, prop_dict(req, 1), "setObject:forKey:", v, key); }
static void proto_del_prop(id k, SEL c, id key, id req) { (void)k; (void)c; id d = prop_dict(req, 0); if (d) MSG(void, d, "removeObjectForKey:", key); }

/* ---- cache na tarefa: devolve 1 se a resposta saiu do cache */
static int serve_cached(task_t *t, job_t *j, cache_ent_t *e) {
    id resp = make_response(e->key, e->status, e->version, &e->hdrs);
    LK(); rel(t->response); t->response = resp; ULK();
    t->to_recv = (long long)e->n;
    t->cache_hit = 1;
    j->r.status = e->status; j->r.version = e->version;
    mnet_resp_t fake = {0};
    fake.status = e->status; fake.version = e->version; fake.url = e->key; fake.expected_len = (long long)e->n;
    fake.hdrs = e->hdrs;
    j->revalidating = e;                                         /* marca: a resposta veio do cache, não regrava */
    if (j_response(j, &fake)) return -1;
    if (e->n) j_body(j, e->body, e->n);
    return 1;
}

static void *ws_main(void *arg);
static void *task_main(void *arg) {
    task_t *t = arg;
    if (t->kind == T_WS) return ws_main(arg);
    void *pool = objc_autoreleasePoolPush();
    job_t *j = calloc(1, sizeof *j);
    j->t = t;
    NSURLSession_t *s = SS(t->session);
    id cfg = s->cfg;
    double t0 = cx_now();
    int err = 0;
    char msg[256] = {0};
    if (t->handler && t->kind != T_DOWNLOAD) t->body = mutdata_new();
    /* earliestBeginDate */
    if (t->earliest) { double w = MSG0(double, t->earliest, "timeIntervalSinceNow"); if (w > 0) usleep((unsigned)(w * 1e6)); }
    for (;;) {
        creq_free(&j->rq);
        creq_from(&j->rq, t->cur_req, CF(cfg)->headers);
        if (!j->rq.timeout || j->rq.timeout <= 0) j->rq.timeout = CF(cfg)->t_req;
        /* NSURLProtocol do app */
        id pk = find_protocol(t, t->cur_req);
        if (pk) { err = run_protocol(t, j, pk); break; }
        /* cache */
        id cache = CF(cfg)->cache;
        unsigned long pol = j->rq.policy ? j->rq.policy : CF(cfg)->policy;
        cache_ent_t *ce = 0;
        if (cache && mnet_ieq(j->rq.method, "GET") && pol != 1 && pol != 4) {
            LK(); ce = cache_lookup(UC(cache), j->rq.url, &j->rq.hdrs); ULK();
            if (ce && (pol == 2 || pol == 3)) { int r = serve_cached(t, j, ce); err = r < 0 ? MNET_E_CANCELLED : 0; break; }
            if (!ce && pol == 3) { err = -1008; break; }
            if (ce && pol == 0 && !mnet_cache_req_revalidate(&j->rq.hdrs) &&
                mnet_cache_fresh_left(&ce->hdrs, ce->status, ce->req_time, ce->resp_time, now_s()) > 0) {
                int r = serve_cached(t, j, ce); err = r < 0 ? MNET_E_CANCELLED : 0; break;
            }
            if (ce && mnet_cache_validators(&ce->hdrs, &j->rq.hdrs)) j->revalidating = ce;
        }
        if (j->auth_header) mnet_hdrs_set(&j->rq.hdrs, j->auth_retry == 407 ? "Proxy-Authorization" : "Authorization", j->auth_header);
        if (t->kind == T_DOWNLOAD && t->resume_offset) {
            char rg[48]; snprintf(rg, sizeof rg, "bytes=%lld-", t->resume_offset);
            mnet_hdrs_set(&j->rq.hdrs, "Range", rg);
            id info = MSG(id, cls("NSJSONSerialization"), "JSONObjectWithData:options:error:", t->resume_data_in, (u64)0, (id *)0);
            id et = info ? dget(info, "NSURLSessionResumeEntityTag") : 0;
            if (et) mnet_hdrs_set(&j->rq.hdrs, "If-Range", cs(et));
        }
        mnet_req_t q = {0};
        q.method = j->rq.method; q.url = j->rq.url; q.hdrs = &j->rq.hdrs;
        if (t->upload_fd >= 0 || (t->upload_data && !MSG(BOOL, t->upload_data, "isKindOfClass:", cls("NSData")))) {
            q.body_read = body_read_cb; q.body_rewind = body_rewind_cb; q.body_stream_len = t->to_send;
        } else {
            id b = t->upload_data ? t->upload_data : j->rq.body;
            if (b) { q.body = MSG0(const uint8_t *, b, "bytes"); q.body_len = (size_t)MSG0(u64, b, "length"); t->to_send = (long long)q.body_len; }
            else if (j->rq.stream) { rel(t->upload_data); t->upload_data = retain(j->rq.stream); MSG0(void, t->upload_data, "open"); q.body_read = body_read_cb; q.body_rewind = body_rewind_cb; q.body_stream_len = -1; }
        }
        q.timeout_ms = (int)(j->rq.timeout * 1000);
        q.total_timeout_ms = CF(cfg)->t_res > 0 && CF(cfg)->t_res < 2e6 ? (int)(CF(cfg)->t_res * 1000) : 0;
        q.max_redirects = 0;
        q.on_redirect = j_redirect;
        q.on_response = j_response; q.on_body = j_body; q.on_sent = j_sent;
        q.on_tls = j_tls; q.release_trust = rel_trust;
        q.cancel = &t->cancel;
        q.ctx = j;
        id store = CF(cfg)->cookies;
        q.no_cookies = !CF(cfg)->set_cookies || !j->rq.handle_cookies || !store || object_getClass(store) != &NSHTTPCookieStorage_c;
        if (!q.no_cookies) { q.jar = CS(store)->jar; q.no_store_cookies = CS(store)->policy == 1; }
        char *px = cfg_proxy_for(cfg, j->rq.url);
        q.proxy = px;
        j->auth_retry = 0;
        mnet_resp_free(&j->r);
        err = mnet_fetch(cx_mnet_client(), &q, &j->r, msg, sizeof msg);
        free(px);
        if (!q.no_cookies && mnet_hdrs_get(&j->r.hdrs, "set-cookie")) storage_changed(store);
        if (err == MNET_E_CANCELLED && j->auth_retry && !t->cancel) continue;       /* 401/407 com credencial nova */
        if (err == MNET_E_CANCELLED && j->tls_cancel) break;
        /* 304: completa com a entrada guardada */
        if (!err && j->revalidating && j->status == 304) {
            LK(); mnet_cache_merge_304(&j->revalidating->hdrs, &j->r.hdrs); j->revalidating->resp_time = now_s(); ULK();
            cache_ent_t *e = j->revalidating; j->revalidating = 0;
            rel(t->response); t->response = 0;
            if (t->body) MSG(void, t->body, "setLength:", (u64)0);
            serve_cached(t, j, e);
            j->revalidating = 0;
        }
        break;
    }
    /* guarda no cache */
    if (!err && j->cacheable && !t->cancel && CF(cfg)->cache) {
        cache_ent_t *e = calloc(1, sizeof *e);
        e->key = dupc(j->r.url ? j->r.url : j->rq.url);
        e->status = j->r.status; e->version = j->r.version;
        e->hdrs = j->store_hdrs; memset(&j->store_hdrs, 0, sizeof j->store_hdrs);
        for (int i = 0; i < j->rq.hdrs.n; i++) mnet_hdrs_add(&e->req_hdrs, j->rq.hdrs.v[i].name, strlen(j->rq.hdrs.v[i].name), j->rq.hdrs.v[i].value, strlen(j->rq.hdrs.v[i].value));
        e->vary = mnet_cache_vary_key(&e->hdrs, &e->req_hdrs);
        e->body = j->cache_body.p; e->n = j->cache_body.n; memset(&j->cache_body, 0, sizeof j->cache_body);
        e->resp_time = now_s(); e->req_time = (long long)t0;
        /* o delegate pode trocar ou recusar (dataTask:willCacheResponse:completionHandler:) */
        id d = t->kind == T_DATA ? task_delegate_for(t, "URLSession:dataTask:willCacheResponse:completionHandler:") : 0;
        if (d) {
            id cached = cached_from_ent(e);
            reply_t *r = task_reply(t, (void *)inv_obj);
            MSG(void, d, "URLSession:dataTask:willCacheResponse:completionHandler:", t->session, (id)t, cached, (void *)r);
            reply_wait(r);
            rel(cached);
            if (!r->obj) { ent_free(e); e = 0; }
            else e->policy = CR(r->obj)->policy;
        }
        if (e) { LK(); cache_put(UC(CF(cfg)->cache), e); ULK(); }
    }
    if (t->tmp_fd >= 0) { close(t->tmp_fd); t->tmp_fd = -1; }
    if (t->upload_fd >= 0) { close(t->upload_fd); t->upload_fd = -1; }
    if (err) {
        if (t->cancel || err == MNET_E_CANCELLED) err = MNET_E_CANCELLED;
        id url = MSG0(id, t->cur_req, "URL");
        LK(); if (!t->error) t->error = url_error(err, msg, url); ULK();
    }
    make_metrics(t, j, t0, cx_now());
    mnet_resp_free(&j->r); creq_free(&j->rq); mnet_hdrs_free(&j->store_hdrs); mnet_buf_free(&j->cache_body);
    free(j->auth_header); free(j);
    t->state = ST_COMPLETED;
    on_queue(t->session, ev_complete, t, 0);                     /* ev_complete solta a referência do fio */
    objc_autoreleasePoolPop(pool);
    return 0;
}

/* ---- ações da tarefa */
static void task_start(task_t *t) {
    NSURLSession_t *s = SS(t->session);
    LK();
    t->next = s->tasks; s->tasks = t;
    ULK();
    retain((id)t);                                               /* o fio segura a tarefa até ev_complete */
    unsigned long th;
    if (pthread_create(&th, 0, task_main, t)) {
        t->error = url_error(MNET_E_UNKNOWN, "sem fio", 0);
        t->state = ST_COMPLETED;
        on_queue(t->session, ev_complete, t, 0);
        return;
    }
    pthread_detach(th);
}
static void task_resume(id self, SEL c) {
    (void)c;
    task_t *t = TK(self);
    if (t->state == ST_COMPLETED || t->state == ST_CANCELING) return;
    if (SS(t->session)->invalid) return;
    t->state = ST_RUNNING;
    if (!t->started) { t->started = 1; task_start(t); }
}
static void task_suspend(id self, SEL c) { (void)c; if (TK(self)->state == ST_RUNNING) TK(self)->state = ST_SUSPENDED; }
static void task_cancel(id self, SEL c) {
    (void)c;
    task_t *t = TK(self);
    if (t->state == ST_COMPLETED) return;
    t->cancel = 1;
    int was_started = t->started;
    t->state = ST_CANCELING;
    if (!was_started) {                                          /* nunca rodou: completa com -999 */
        t->started = 1;
        t->error = url_error(MNET_E_CANCELLED, 0, MSG0(id, t->orig_req, "URL"));
        retain((id)t);
        LK(); t->next = SS(t->session)->tasks; SS(t->session)->tasks = t; ULK();
        t->state = ST_COMPLETED;
        on_queue(t->session, ev_complete, t, 0);
    }
}
static void dl_cancel_resume(id self, SEL c, void *block) {
    task_t *t = TK(self);
    t->resume_block = block ? _Block_copy(block) : 0;
    t->resume_data_wanted = 1;
    task_cancel(self, c);
}

/* ---- sessão */
static id g_shared_session;
static id sess_make(id cfg, id dlg, id queue) {
    NSURLSession_t *s = (NSURLSession_t *)cx_alloc_instance(&NSURLSession_c);
    s->cfg = cfg ? MSG0(id, cfg, "copy") : cfg_new(0);
    s->delegate = retain(dlg);                                   /* a sessão segura o delegate até invalidar */
    s->op_queue = retain(queue);
    return (id)s;
}
static id sess_shared(id k, SEL c) {
    (void)k; (void)c;
    if (g_shared_session) return g_shared_session;
    id s = sess_make(0, 0, 0);
    SS(s)->is_shared = 1;
    LK(); if (!g_shared_session) g_shared_session = s; else rel(s); ULK();
    return g_shared_session;
}
static id sess_with_cfg(id k, SEL c, id cfg) { (void)k; (void)c; return ar(sess_make(cfg, 0, 0)); }
static id sess_with_dlg(id k, SEL c, id cfg, id dlg, id q) { (void)k; (void)c; return ar(sess_make(cfg, dlg, q)); }
static void sess_dealloc(id self, SEL c) {
    NSURLSession_t *s = SS(self);
    rel(s->cfg); rel(s->delegate); rel(s->op_queue); rel(s->desc);
    if (s->queue && s->queue != dispatch_get_main_queue()) dispatch_release(s->queue);
    IMP sup = cx_lookup(&NSObject_c, c); if (sup) ((void (*)(id, SEL))sup)(self, c);
}
static id sess_cfg(id s, SEL c) { (void)c; return ar(MSG0(id, SS(s)->cfg, "copy")); }
static id sess_dlg(id s, SEL c) { (void)c; return SS(s)->delegate; }
static id sess_queue_obj(id s, SEL c) {
    (void)c;
    if (!SS(s)->op_queue) SS(s)->op_queue = retain(MSG0(id, MSG0(id, cls("NSOperationQueue"), "alloc"), "init"));
    return SS(s)->op_queue;
}
static id sess_desc(id s, SEL c) { (void)c; return SS(s)->desc; }
static void sess_set_desc(id s, SEL c, id d) { (void)c; id o = SS(s)->desc; SS(s)->desc = retain(MSG0(id, d, "copy")); rel(SS(s)->desc); rel(o); }
static id req_for_url(id url) { return MSG(id, cls("NSURLRequest"), "requestWithURL:", url); }
static id mk_task(id sess, int kind, id req, void *handler) {
    if (SS(sess)->invalid || SS(sess)->finishing) {
        /* sessão invalidada: o macOS lança exceção; aqui devolve uma tarefa que completa com cancelado */
    }
    id t = task_alloc_kind(sess, kind, req);
    if (handler) TK(t)->handler = _Block_copy(handler);
    id d = SS(sess)->delegate;
    if (d && responds(d, "URLSession:didCreateTask:")) MSG(void, d, "URLSession:didCreateTask:", sess, t);
    return ar(t);
}
static id s_data_req(id s, SEL c, id r) { (void)c; return mk_task(s, T_DATA, r, 0); }
static id s_data_url(id s, SEL c, id u) { (void)c; return mk_task(s, T_DATA, req_for_url(u), 0); }
static id s_data_req_h(id s, SEL c, id r, void *h) { (void)c; return mk_task(s, T_DATA, r, h); }
static id s_data_url_h(id s, SEL c, id u, void *h) { (void)c; return mk_task(s, T_DATA, req_for_url(u), h); }
static id s_up_data(id s, SEL c, id r, id d) { (void)c; id t = mk_task(s, T_UPLOAD, r, 0); TK(t)->upload_data = retain(d); TK(t)->to_send = d ? (long long)MSG0(u64, d, "length") : 0; return t; }
static id s_up_data_h(id s, SEL c, id r, id d, void *h) { (void)c; id t = mk_task(s, T_UPLOAD, r, h); TK(t)->upload_data = retain(d); TK(t)->to_send = d ? (long long)MSG0(u64, d, "length") : 0; return t; }
static id s_up_file_any(id s, id r, id f, void *h) {
    id t = mk_task(s, T_UPLOAD, r, h);
    TK(t)->upload_file = retain(f);
    const char *p = f ? cs(MSG0(id, f, "path")) : 0;
    TK(t)->upload_fd = p ? open(p, 0) : -1;
    struct { long long a[2]; } st;
    (void)st;
    if (TK(t)->upload_fd >= 0) {
        long long end = 0; char b[65536]; long k;
        while ((k = read(TK(t)->upload_fd, b, sizeof b)) > 0) end += k;   /* tamanho para Content-Length */
        close(TK(t)->upload_fd);
        TK(t)->upload_fd = open(p, 0);
        TK(t)->to_send = end;
    }
    return t;
}
static id s_up_file(id s, SEL c, id r, id f) { (void)c; return s_up_file_any(s, r, f, 0); }
static id s_up_file_h(id s, SEL c, id r, id f, void *h) { (void)c; return s_up_file_any(s, r, f, h); }
static id s_up_stream(id s, SEL c, id r) { (void)c; return mk_task(s, T_UPLOAD, r, 0); }
static id s_dl_req(id s, SEL c, id r) { (void)c; return mk_task(s, T_DOWNLOAD, r, 0); }
static id s_dl_url(id s, SEL c, id u) { (void)c; return mk_task(s, T_DOWNLOAD, req_for_url(u), 0); }
static id s_dl_req_h(id s, SEL c, id r, void *h) { (void)c; return mk_task(s, T_DOWNLOAD, r, h); }
static id s_dl_url_h(id s, SEL c, id u, void *h) { (void)c; return mk_task(s, T_DOWNLOAD, req_for_url(u), h); }
static id s_dl_resume_any(id s, id rd, void *h) {
    id info = rd ? MSG(id, cls("NSJSONSerialization"), "JSONObjectWithData:options:error:", rd, (u64)0, (id *)0) : 0;
    id us = info ? dget(info, "NSURLSessionDownloadURL") : 0;
    id u = us ? MSG(id, cls("NSURL"), "URLWithString:", us) : 0;
    id t = mk_task(s, T_DOWNLOAD, req_for_url(u), h);
    id got = info ? dget(info, "NSURLSessionResumeBytesReceived") : 0;
    TK(t)->resume_data_in = retain(rd);
    TK(t)->resume_offset = got ? MSG0(long long, got, "longLongValue") : 0;
    return t;
}
static id s_dl_resume(id s, SEL c, id rd) { (void)c; return s_dl_resume_any(s, rd, 0); }
static id s_dl_resume_h(id s, SEL c, id rd, void *h) { (void)c; return s_dl_resume_any(s, rd, h); }
static id s_tasks_array(id s, int kind_mask) {
    id a = mutarr();
    LK();
    for (task_t *t = SS(s)->tasks; t; t = t->next) if (kind_mask & (1 << t->kind)) MSG(void, a, "addObject:", (id)t);
    ULK();
    return a;
}
typedef struct { id s; void *b; int all; } gt_t;
static void ev_get_tasks(void *x) {
    gt_t *g = x;
    id d = s_tasks_array(g->s, 1 << T_DATA), u = s_tasks_array(g->s, 1 << T_UPLOAD), w = s_tasks_array(g->s, 1 << T_DOWNLOAD);
    if (g->all) { id a = s_tasks_array(g->s, 0xF); call_1(g->b, a); rel(a); }
    else call_3(g->b, d, u, w);
    rel(d); rel(u); rel(w);
    _Block_release(g->b); rel(g->s); free(g);
}
static void s_get_tasks(id s, SEL c, void *b) { (void)c; gt_t *g = calloc(1, sizeof *g); g->s = retain(s); g->b = _Block_copy(b); on_queue(s, ev_get_tasks, g, 0); }
static void s_get_all(id s, SEL c, void *b) { (void)c; gt_t *g = calloc(1, sizeof *g); g->s = retain(s); g->b = _Block_copy(b); g->all = 1; on_queue(s, ev_get_tasks, g, 0); }
static void ev_invalid(void *x) {
    id s = x;
    if (SS(s)->delegate && responds(SS(s)->delegate, "URLSession:didBecomeInvalidWithError:"))
        MSG(void, SS(s)->delegate, "URLSession:didBecomeInvalidWithError:", s, (id)0);
    rel(SS(s)->delegate); SS(s)->delegate = 0;
    rel(s);
}
static void s_invalidate_cancel(id s, SEL c) {
    (void)c;
    if (SS(s)->is_shared || SS(s)->invalid) return;              /* a sessão compartilhada não se invalida */
    LK();
    for (task_t *t = SS(s)->tasks; t; t = t->next) t->cancel = 1;
    int none = !SS(s)->tasks;
    ULK();
    SS(s)->finishing = 1;
    if (none) { SS(s)->invalid = 1; retain(s); on_queue(s, ev_invalid, s, 0); }
}
static void s_finish_invalidate(id s, SEL c) {
    (void)c;
    if (SS(s)->is_shared || SS(s)->invalid) return;
    SS(s)->finishing = 1;
    LK(); int none = !SS(s)->tasks; ULK();
    if (none) { SS(s)->invalid = 1; retain(s); on_queue(s, ev_invalid, s, 0); }
}
typedef struct { id s; void *b; int reset; } rs_t;
static void ev_reset(void *x) {
    rs_t *r = x;
    if (r->reset) {
        id st = CF(SS(r->s)->cfg)->cookies;
        if (st && CS(st)->own) { LK(); mnet_jar_clear(CS(st)->jar); ULK(); }
        id ca = CF(SS(r->s)->cfg)->cache;
        if (ca) cache_remove_all(ca, 0);
    }
    mnet_client_close_idle(cx_mnet_client());
    call_v(r->b);
    _Block_release(r->b); rel(r->s); free(r);
}
static void s_reset(id s, SEL c, void *b) { (void)c; rs_t *r = calloc(1, sizeof *r); r->s = retain(s); r->b = _Block_copy(b); r->reset = 1; on_queue(s, ev_reset, r, 0); }
static void s_flush(id s, SEL c, void *b) {
    (void)c;
    id st = CF(SS(s)->cfg)->cookies;
    if (st && object_getClass(st) == &NSHTTPCookieStorage_c) storage_save(CS(st));
    rs_t *r = calloc(1, sizeof *r); r->s = retain(s); r->b = _Block_copy(b); on_queue(s, ev_reset, r, 0);
}

/* storeCookies:forTask: / getCookiesForTask:completionHandler: (precisam da URL da tarefa) */
static void storage_store_task(id s, SEL c, id cookies, id task) {
    id url = MSG0(id, TK(task)->cur_req, "URL");
    storage_set_many(s, c, cookies, url, url);
}
static void storage_get_task(id s, SEL c, id task, void *block) {
    id a = storage_for_url(s, c, MSG0(id, TK(task)->cur_req, "URL"));
    call_1(block, a);
}
static void cache_store_task(id s, SEL c, id cached, id task) { cache_store(s, c, cached, TK(task)->cur_req); }
static void cache_get_task(id s, SEL c, id task, void *block) { call_1(block, cache_get(s, c, TK(task)->cur_req)); }
static void cache_remove_task(id s, SEL c, id task) { cache_remove(s, c, TK(task)->cur_req); }

/* ================================================================ NSURLConnection (sobre uma sessão própria) */
typedef struct { cx_class_t *isa; id req, delegate, task, session, queue; int started, cancelled; } NSURLConnection_t;
#define UCN(o) ((NSURLConnection_t *)(o))
CLASS(MOSURLConnectionAdapter) CLASS(MOSChallengeSender)
typedef struct { cx_class_t *isa; id conn; } MOSURLConnectionAdapter_t;
typedef struct { cx_class_t *isa; void *block; } MOSChallengeSender_t;
#define ADC(o) (((MOSURLConnectionAdapter_t *)(o))->conn)
static void blk_disp(void *b, long d) { if (b) ((void (*)(void *, long))BLK(b)->invoke)(b, d); }
static void blk_disp_obj(void *b, long d, id o) { if (b) ((void (*)(void *, long, id))BLK(b)->invoke)(b, d, o); }
static void ad_resp(id s, SEL c, id sess, id task, id resp, void *h) {
    (void)c; (void)sess; (void)task;
    id d = UCN(ADC(s))->delegate;
    if (!UCN(ADC(s))->cancelled && responds(d, "connection:didReceiveResponse:")) MSG(void, d, "connection:didReceiveResponse:", ADC(s), resp);
    blk_disp(h, UCN(ADC(s))->cancelled ? 0 : 1);
}
static void ad_data(id s, SEL c, id sess, id task, id data) {
    (void)c; (void)sess; (void)task;
    id d = UCN(ADC(s))->delegate;
    if (!UCN(ADC(s))->cancelled && responds(d, "connection:didReceiveData:")) MSG(void, d, "connection:didReceiveData:", ADC(s), data);
}
static void ad_redirect(id s, SEL c, id sess, id task, id resp, id req, void *h) {
    (void)c; (void)sess; (void)task;
    id d = UCN(ADC(s))->delegate;
    id out = req;
    if (responds(d, "connection:willSendRequest:redirectResponse:")) out = MSG(id, d, "connection:willSendRequest:redirectResponse:", ADC(s), req, resp);
    if (BLK(h)) ((void (*)(void *, id))BLK(h)->invoke)(h, out);
}
static void ad_sent(id s, SEL c, id sess, id task, long long n, long long tot, long long exp) {
    (void)c; (void)sess; (void)task;
    id d = UCN(ADC(s))->delegate;
    if (responds(d, "connection:didSendBodyData:totalBytesWritten:totalBytesExpectedToWrite:"))
        MSG(void, d, "connection:didSendBodyData:totalBytesWritten:totalBytesExpectedToWrite:", ADC(s), (long)n, (long)tot, (long)exp);
}
static void ad_done(id s, SEL c, id sess, id task, id err) {
    (void)c; (void)sess; (void)task;
    id conn = ADC(s), d = UCN(conn)->delegate;
    if (UCN(conn)->cancelled) return;                            /* cancel do NSURLConnection não avisa o delegate */
    if (err) { if (responds(d, "connection:didFailWithError:")) MSG(void, d, "connection:didFailWithError:", conn, err); }
    else if (responds(d, "connectionDidFinishLoading:")) MSG(void, d, "connectionDidFinishLoading:", conn);
}
static void ad_cache(id s, SEL c, id sess, id task, id cached, void *h) {
    (void)c; (void)sess; (void)task;
    id d = UCN(ADC(s))->delegate, out = cached;
    if (responds(d, "connection:willCacheResponse:")) out = MSG(id, d, "connection:willCacheResponse:", ADC(s), cached);
    if (BLK(h)) ((void (*)(void *, id))BLK(h)->invoke)(h, out);
}
/* NSURLAuthenticationChallengeSender: cada método chama o bloco da sessão com a disposição equivalente */
#define SND(o) (((MOSChallengeSender_t *)(o))->block)
static void snd_use(id s, SEL c, id cred, id ch) { (void)c; (void)ch; blk_disp_obj(SND(s), 0, cred); SND(s) = 0; }
static void snd_without(id s, SEL c, id ch) { (void)c; (void)ch; blk_disp_obj(SND(s), 1, 0); SND(s) = 0; }
static void snd_cancel(id s, SEL c, id ch) { (void)c; (void)ch; blk_disp_obj(SND(s), 2, 0); SND(s) = 0; }
static void snd_default(id s, SEL c, id ch) { (void)c; (void)ch; blk_disp_obj(SND(s), 1, 0); SND(s) = 0; }
static void snd_reject(id s, SEL c, id ch) { (void)c; (void)ch; blk_disp_obj(SND(s), 3, 0); SND(s) = 0; }
static void ad_chal(id s, SEL c, id sess, id task, id chal, void *h) {
    (void)c; (void)sess; (void)task;
    id d = UCN(ADC(s))->delegate;
    if (!responds(d, "connection:willSendRequestForAuthenticationChallenge:")) { blk_disp_obj(h, 1, 0); return; }
    id snd = cx_alloc_instance(&MOSChallengeSender_c);
    SND(snd) = h;
    id ch2 = MSG(id, cx_alloc_instance(&NSURLAuthenticationChallenge_c), "initWithProtectionSpace:proposedCredential:previousFailureCount:failureResponse:error:sender:",
                 CH(chal)->space, CH(chal)->proposed, CH(chal)->prev, CH(chal)->failure, CH(chal)->error, snd);
    MSG(void, d, "connection:willSendRequestForAuthenticationChallenge:", ADC(s), ch2);
    rel(ch2); rel(snd);
}
static void ad_sess_chal(id s, SEL c, id sess, id chal, void *h) { ad_chal(s, c, sess, 0, chal, h); }
static id conn_init3(id s, SEL c, id req, id dlg, BOOL start) {
    (void)c;
    UCN(s)->req = retain(MSG0(id, req, "copy")); rel(UCN(s)->req);
    UCN(s)->delegate = retain(dlg);                              /* o NSURLConnection segura o delegate até terminar */
    if (start) MSG0(void, s, "start");
    return s;
}
static id conn_init2(id s, SEL c, id req, id dlg) { return conn_init3(s, c, req, dlg, YES); }
static id conn_with(id k, SEL c, id req, id dlg) { (void)k; return ar(conn_init3(cx_alloc_instance(&NSURLConnection_c), c, req, dlg, YES)); }
static void conn_start(id s, SEL c) {
    (void)c;
    if (UCN(s)->started) return;
    UCN(s)->started = 1;
    id ad = cx_alloc_instance(&MOSURLConnectionAdapter_c);
    ADC(ad) = s;
    id q = UCN(s)->queue ? UCN(s)->queue : MSG0(id, cls("NSOperationQueue"), "mainQueue");
    UCN(s)->session = retain(sess_make(0, ad, q));
    rel(ad);
    UCN(s)->task = retain(s_data_req(UCN(s)->session, 0, UCN(s)->req));
    retain(s);                                                  /* vivo até o fim, como no macOS */
    MSG0(void, UCN(s)->task, "resume");
    s_finish_invalidate(UCN(s)->session, 0);
}
static void conn_cancel(id s, SEL c) { (void)c; UCN(s)->cancelled = 1; if (UCN(s)->task) task_cancel(UCN(s)->task, 0); }
static void conn_set_queue(id s, SEL c, id q) { (void)c; id o = UCN(s)->queue; UCN(s)->queue = retain(q); rel(o); }
static void conn_sched(id s, SEL c, id rl, id mode) { (void)s; (void)c; (void)rl; (void)mode; }
static id conn_req(id s, SEL c) { (void)c; return UCN(s)->req; }
static id conn_cur(id s, SEL c) { (void)c; return UCN(s)->task ? TK(UCN(s)->task)->cur_req : UCN(s)->req; }
static BOOL conn_can(id k, SEL c, id r) { (void)k; (void)c; id u = MSG0(id, r, "URL"); const char *sch = u ? cs(MSG0(id, u, "scheme")) : 0; return sch && (mnet_ieq(sch, "http") || mnet_ieq(sch, "https")); }
static void conn_dealloc(id s, SEL c) {
    rel(UCN(s)->req); rel(UCN(s)->delegate); rel(UCN(s)->task); rel(UCN(s)->session); rel(UCN(s)->queue);
    IMP sup = cx_lookup(&NSObject_c, c); if (sup) ((void (*)(id, SEL))sup)(s, c);
}
/* ponte do fim: o delegate solta a conexão (o retain do start) */
static void ad_done_rel(id s, SEL c, id sess, id task, id err) {
    ad_done(s, c, sess, task, err);
    id conn = ADC(s);
    rel(UCN(conn)->delegate); UCN(conn)->delegate = 0;
    rel(conn);
}
/* síncrono: espera o bloco de conclusão (que roda na fila serial da sessão, não neste fio) */
typedef struct { blk_t b; void *sem; id data, resp, err; } sync_blk_t;
static void sync_inv(void *b, id data, id resp, id err) {
    sync_blk_t *x = b;
    x->data = retain(data); x->resp = retain(resp); x->err = retain(err);
    dispatch_semaphore_signal(x->sem);
}
static id conn_send_sync(id k, SEL c, id req, id *resp, id *err) {
    (void)k; (void)c;
    sync_blk_t *b = calloc(1, sizeof *b);
    b->b.isa = _NSConcreteGlobalBlock; b->b.flags = 1 << 28; b->b.invoke = (void *)sync_inv; b->b.desc = &reply_desc;
    b->sem = dispatch_semaphore_create(0);
    id sess = sess_make(0, 0, 0);
    id t = s_data_req_h(sess, 0, req, b);
    MSG0(void, t, "resume");
    dispatch_semaphore_wait(b->sem, DISPATCH_TIME_FOREVER);
    s_finish_invalidate(sess, 0);
    rel(sess);
    if (resp) *resp = ar(b->resp); else rel(b->resp);
    if (err) *err = ar(b->err); else rel(b->err);
    id out = b->err ? 0 : ar(b->data);
    if (b->err) rel(b->data);
    /* o bloco é global e pode ainda estar guardado pela tarefa: não libera a estrutura */
    return out;
}
/* assíncrono: o bloco do app roda na NSOperationQueue pedida */
typedef struct { blk_t b; id queue; void *app; id data, resp, err; } async_blk_t;
typedef struct { blk_t b; async_blk_t *a; } async_op_t;
static void async_op_inv(void *b) {
    async_blk_t *a = ((async_op_t *)b)->a;
    call_3(a->app, a->resp, a->data, a->err);                    /* (response, data, error) */
    _Block_release(a->app); rel(a->queue); rel(a->data); rel(a->resp); rel(a->err);
}
static void async_inv(void *b, id data, id resp, id err) {
    async_blk_t *a = b;
    a->data = retain(data); a->resp = retain(resp); a->err = retain(err);
    async_op_t *op = calloc(1, sizeof *op);
    op->b.isa = _NSConcreteGlobalBlock; op->b.flags = 1 << 28; op->b.invoke = (void *)async_op_inv; op->b.desc = &reply_desc;
    op->a = a;
    if (a->queue) MSG(void, a->queue, "addOperationWithBlock:", (void *)op);
    else async_op_inv(op);
}
static void conn_send_async(id k, SEL c, id req, id queue, void *h) {
    (void)k; (void)c;
    async_blk_t *a = calloc(1, sizeof *a);
    a->b.isa = _NSConcreteGlobalBlock; a->b.flags = 1 << 28; a->b.invoke = (void *)async_inv; a->b.desc = &reply_desc;
    a->queue = retain(queue); a->app = h ? _Block_copy(h) : 0;
    id sess = sess_make(0, 0, 0);
    id t = s_data_req_h(sess, 0, req, a);
    MSG0(void, t, "resume");
    s_finish_invalidate(sess, 0);
    rel(sess);
}

/* ================================================================ NSURLSessionWebSocketTask */
typedef struct { cx_class_t *isa; long type; id data, string; } NSURLSessionWebSocketMessage_t;
#define WM(o) ((NSURLSessionWebSocketMessage_t *)(o))
static id wm_init_data(id s, SEL c, id d) { (void)c; WM(s)->type = 0; WM(s)->data = retain(MSG0(id, d, "copy")); rel(WM(s)->data); return s; }
static id wm_init_str(id s, SEL c, id t) { (void)c; WM(s)->type = 1; WM(s)->string = retain(MSG0(id, t, "copy")); rel(WM(s)->string); return s; }
static long wm_type(id s, SEL c) { (void)c; return WM(s)->type; }
static id wm_data(id s, SEL c) { (void)c; return WM(s)->data; }
static id wm_string(id s, SEL c) { (void)c; return WM(s)->string; }
static void wm_dealloc(id s, SEL c) { rel(WM(s)->data); rel(WM(s)->string); IMP sup = cx_lookup(&NSObject_c, c); if (sup) ((void (*)(id, SEL))sup)(s, c); }
static id wm_new(int type, const uint8_t *d, size_t n) {
    id m = cx_alloc_instance(&NSURLSessionWebSocketMessage_c);
    if (type == MNET_WS_TEXT) { WM(m)->type = 1; WM(m)->string = cx_str_n((const char *)d, n); }
    else { WM(m)->type = 0; WM(m)->data = retain(data_bytes(d, n)); }
    return m;
}

static void q_push(wsq_t **h, wsq_t **t, id msg, void *blk) {
    wsq_t *e = calloc(1, sizeof *e);
    e->msg = retain(msg); e->blk = blk ? _Block_copy(blk) : 0;
    if (*t) (*t)->next = e; else *h = e;
    *t = e;
}
static wsq_t *q_pop(wsq_t **h, wsq_t **t) { wsq_t *e = *h; if (e) { *h = e->next; if (!*h) *t = 0; } return e; }
static wsx_t *wsx_of(task_t *t) { if (!t->wsx) t->wsx = calloc(1, sizeof(wsx_t)); return t->wsx; }
static id s_ws_any(id s, id req, id protocols) {
    id t = mk_task(s, T_WS, req, 0);
    wsx_t *x = wsx_of(TK(t));
    x->max_msg = 1 << 20;                                       /* padrão do macOS: 1 MiB */
    u64 n = protocols ? MSG0(u64, protocols, "count") : 0;
    mnet_buf_t b = {0};
    for (u64 i = 0; i < n; i++) { if (i) mnet_buf_put(&b, ", ", 2); mnet_buf_puts(&b, cs(MSG(id, protocols, "objectAtIndex:", i))); }
    x->protocols = (char *)b.p;
    return t;
}
static id s_ws_url(id s, SEL c, id u) { (void)c; return s_ws_any(s, req_for_url(u), 0); }
static id s_ws_url_p(id s, SEL c, id u, id p) { (void)c; return s_ws_any(s, req_for_url(u), p); }
static id s_ws_req(id s, SEL c, id r) {
    (void)c;
    id p = MSG(id, r, "valueForHTTPHeaderField:", ar(ns("Sec-WebSocket-Protocol")));
    id arr = 0;
    if (p) arr = MSG(id, p, "componentsSeparatedByString:", ar(ns(",")));
    id t = s_ws_any(s, r, 0);
    if (arr) {
        mnet_buf_t b = {0}; u64 n = MSG0(u64, arr, "count");
        for (u64 i = 0; i < n; i++) {
            const char *v = cs(MSG(id, arr, "objectAtIndex:", i));
            while (*v == ' ') v++;
            if (b.n) mnet_buf_put(&b, ", ", 2);
            mnet_buf_puts(&b, v);
        }
        free(TK(t)->wsx->protocols); TK(t)->wsx->protocols = (char *)b.p;
    }
    return t;
}
static id ws_err(int code, const char *txt) {
    id info = mutdict(); dset(info, "NSLocalizedDescription", ar(ns(txt)));
    id e = MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:", ar(ns("NSPOSIXErrorDomain")), (long)code, info);
    rel(info);
    return retain(e);
}
static void ws_send(id self, SEL c, id msg, void *blk) {
    (void)c;
    task_t *t = TK(self);
    wsx_t *x = wsx_of(t);
    LK(); q_push(&x->send_head, &x->send_tail, msg, blk); ULK();
}
static void ws_receive(id self, SEL c, void *blk) {
    (void)c;
    task_t *t = TK(self);
    wsx_t *x = wsx_of(t);
    LK(); q_push(&x->wait_head, &x->wait_tail, 0, blk); ULK();
}
static void ws_ping(id self, SEL c, void *blk) {
    (void)c;
    wsx_t *x = wsx_of(TK(self));
    LK(); q_push(&x->pong_head, &x->pong_tail, 0, blk); x->pings_pending++; ULK();
}
static void ws_cancel_close(id self, SEL c, long code, id reason) {
    (void)c;
    task_t *t = TK(self);
    wsx_t *x = wsx_of(t);
    LK(); x->want_close = (int)(code ? code : 1000); rel(x->my_reason); x->my_reason = retain(reason); ULK();
    if (!t->started) task_cancel(self, 0);
}
static long ws_close_code(id s, SEL c) { (void)c; return TK(s)->wsx ? TK(s)->wsx->close_code : 0; }
static id ws_close_reason(id s, SEL c) { (void)c; return TK(s)->wsx ? TK(s)->wsx->close_reason : 0; }
static long ws_max(id s, SEL c) { (void)c; return wsx_of(TK(s))->max_msg; }
static void ws_set_max(id s, SEL c, long v) { (void)c; wsx_of(TK(s))->max_msg = v; }
/* eventos na fila do delegate */
typedef struct { task_t *t; void *blk; id a, b; long code; int kind; } wsev_t;
static void wsev_run(void *p) {
    wsev_t *e = p;
    task_t *t = e->t;
    switch (e->kind) {
    case 0: call_1(e->blk, e->a); break;                                     /* send concluído (erro ou nil) */
    case 1: call_2_(e->blk, e->a, e->b); break;                              /* receive: mensagem, erro */
    case 2: call_1(e->blk, e->a); break;                                     /* pong */
    case 3: { id d = task_delegate_for(t, "URLSession:webSocketTask:didOpenWithProtocol:");
              if (d) MSG(void, d, "URLSession:webSocketTask:didOpenWithProtocol:", t->session, (id)t, e->a); break; }
    case 4: { id d = task_delegate_for(t, "URLSession:webSocketTask:didCloseWithCode:reason:");
              if (d) MSG(void, d, "URLSession:webSocketTask:didCloseWithCode:reason:", t->session, (id)t, e->code, e->a); break; }
    }
    if (e->blk) _Block_release(e->blk);
    rel(e->a); rel(e->b);
    free(e);
}
static void wsev(task_t *t, int kind, void *blk, id a, id b, long code) {
    wsev_t *e = calloc(1, sizeof *e);
    e->t = t; e->kind = kind; e->blk = blk; e->a = retain(a); e->b = retain(b); e->code = code;
    on_queue(t->session, wsev_run, e, 0);
}
typedef struct { task_t *t; wsx_t *x; } wsctx_t;
static int wsc_msg(void *c, int op, const uint8_t *d, size_t n) {
    wsctx_t *w = c;
    id m = wm_new(op, d, n);
    LK();
    wsq_t *wait = q_pop(&w->x->wait_head, &w->x->wait_tail);
    if (!wait) q_push(&w->x->recv_head, &w->x->recv_tail, m, 0);
    ULK();
    if (wait) { wsev(w->t, 1, wait->blk, m, 0, 0); free(wait); }
    rel(m);
    w->t->recv += (long long)n;
    return 0;
}
static void wsc_pong(void *c, const uint8_t *d, size_t n) {
    (void)d; (void)n;
    wsctx_t *w = c;
    LK(); wsq_t *e = q_pop(&w->x->pong_head, &w->x->pong_tail); ULK();
    if (e) { wsev(w->t, 2, e->blk, 0, 0, 0); free(e); }
}
static void wsc_close(void *c, int code, const char *why) {
    wsctx_t *w = c;
    w->x->close_code = code;
    rel(w->x->close_reason);
    w->x->close_reason = why && *why ? retain(data_bytes(why, strlen(why))) : 0;
}
static void wsc_rand(void *c, uint8_t *b, size_t n) { (void)c; arc4random_buf(b, n); }
static void *ws_main(void *arg) {
    task_t *t = arg;
    void *pool = objc_autoreleasePoolPush();
    wsx_t *x = wsx_of(t);
    creq_t rq; creq_from(&rq, t->cur_req, CF(SS(t->session)->cfg)->headers);
    /* http(s):// também é aceito, como no macOS */
    char *url = rq.url;
    if (!strncmp(url, "http://", 7) || !strncmp(url, "https://", 8)) {
        mnet_buf_t b = {0}; mnet_buf_puts(&b, url[4] == 's' ? "wss" : "ws"); mnet_buf_puts(&b, strchr(url, ':')); free(rq.url); rq.url = url = (char *)b.p;
    }
    wsctx_t ctx = {t, x};
    mnet_wsconn_t w; memset(&w, 0, sizeof w);
    w.ws.ctx = &ctx; w.ws.on_message = wsc_msg; w.ws.on_pong = wsc_pong; w.ws.on_close = wsc_close; w.ws.random = wsc_rand;
    w.ws.max_msg = x->max_msg > 0 ? (size_t)x->max_msg : 0;
    mnet_req_t q = {0};
    q.url = url; q.hdrs = &rq.hdrs; q.timeout_ms = (int)((rq.timeout > 0 ? rq.timeout : CF(SS(t->session)->cfg)->t_req) * 1000);
    q.cancel = &t->cancel;
    id store = CF(SS(t->session)->cfg)->cookies;
    q.no_cookies = !CF(SS(t->session)->cfg)->set_cookies || !store || object_getClass(store) != &NSHTTPCookieStorage_c;
    if (!q.no_cookies) q.jar = CS(store)->jar;
    char *px = cfg_proxy_for(SS(t->session)->cfg, url);
    q.proxy = px;
    char msg[200];
    int r = mnet_ws_open(cx_mnet_client(), &q, x->protocols, &w, msg, sizeof msg);
    free(px);
    if (w.resp.status) { id resp = make_response(url, w.resp.status, 11, &w.resp.hdrs); LK(); rel(t->response); t->response = resp; ULK(); }
    if (r) {
        int code = r == MNET_E_BAD_RESPONSE && w.resp.status && w.resp.status != 101 ? MNET_E_BAD_RESPONSE : r;
        t->error = url_error(code, msg, MSG0(id, t->cur_req, "URL"));
    } else {
        x->open = 1;
        wsev(t, 3, 0, w.protocol ? ar(ns(w.protocol)) : 0, 0, 0);
        int alive = 1;
        while (alive) {
            /* envios pendentes */
            for (;;) {
                LK(); wsq_t *e = q_pop(&x->send_head, &x->send_tail); ULK();
                if (!e) break;
                int ok = 0;
                if (!w.ws.close_sent) {
                    if (WM(e->msg)->type == 1) { const char *s = cs(WM(e->msg)->string); ok = !mnet_ws_send(&w.ws, MNET_WS_TEXT, s, strlen(s)); }
                    else { id d = WM(e->msg)->data; ok = !mnet_ws_send(&w.ws, MNET_WS_BINARY, d ? MSG0(const void *, d, "bytes") : "", d ? (size_t)MSG0(u64, d, "length") : 0); }
                    if (ok) ok = !mnet_wsconn_flush(&w, q.timeout_ms);
                    if (ok) t->sent += WM(e->msg)->type == 1 ? (long long)strlen(cs(WM(e->msg)->string)) : (long long)MSG0(u64, WM(e->msg)->data, "length");
                }
                wsev(t, 0, e->blk, ok ? 0 : ar(ws_err(57, "Socket is not connected")), 0, 0);
                rel(e->msg); free(e);
            }
            LK(); int pings = x->pings_pending; x->pings_pending = 0; int wc = x->want_close; ULK();
            while (pings-- > 0) mnet_ws_ping(&w.ws, "mOS", 3);
            if ((wc || t->cancel) && !w.ws.close_sent) {
                const char *why = x->my_reason ? (const char *)MSG0(const void *, x->my_reason, "bytes") : 0;
                char rb[124] = {0};
                if (why) { size_t l = (size_t)MSG0(u64, x->my_reason, "length"); if (l > 123) l = 123; memcpy(rb, why, l); }
                mnet_ws_close(&w.ws, wc ? wc : 1001, rb);
            }
            if (w.ws.out.n && mnet_wsconn_flush(&w, q.timeout_ms) == -1) break;
            if (w.ws.close_recv) break;
            long k = mnet_wsconn_poll(&w, 100);
            if (k < 0) { if (!w.ws.close_recv && !w.ws.close_sent) t->error = url_error(MNET_E_LOST, w.ws.err, MSG0(id, t->cur_req, "URL")); break; }
            if (w.ws.close_sent && !w.ws.close_recv) {
                static const int grace = 3000;                    /* espera o eco do close */
                long long t0 = cx_mnet_io()->now_ms(0);
                while (!w.ws.close_recv && cx_mnet_io()->now_ms(0) - t0 < grace) if (mnet_wsconn_poll(&w, 100) < 0) break;
                break;
            }
        }
        if (w.ws.out.n) mnet_wsconn_flush(&w, 1000);
        if (!x->close_code) x->close_code = w.ws.close_code ? w.ws.close_code : 1006;
        wsev(t, 4, 0, x->close_reason, 0, x->close_code);
        if (t->cancel && !t->error) t->error = url_error(MNET_E_CANCELLED, 0, MSG0(id, t->cur_req, "URL"));
    }
    /* quem ainda espera mensagem recebe erro */
    for (;;) {
        LK(); wsq_t *e = q_pop(&x->wait_head, &x->wait_tail); ULK();
        if (!e) break;
        wsev(t, 1, e->blk, 0, ar(ws_err(57, "Socket is not connected")), 0);
        free(e);
    }
    mnet_wsconn_free(&w);
    creq_free(&rq);
    t->state = ST_COMPLETED;
    on_queue(t->session, ev_complete, t, 0);
    objc_autoreleasePoolPop(pool);
    return 0;
}
static void cx_ws_register(void) {
    cx_class_t *ob = &NSObject_c;
    DEF(NSURLSessionWebSocketTask, &NSURLSessionTask_c);
    M(NSURLSessionWebSocketTask, "sendMessage:completionHandler:", ws_send);
    M(NSURLSessionWebSocketTask, "receiveMessageWithCompletionHandler:", ws_receive);
    M(NSURLSessionWebSocketTask, "sendPingWithPongReceiveHandler:", ws_ping);
    M(NSURLSessionWebSocketTask, "cancelWithCloseCode:reason:", ws_cancel_close);
    M(NSURLSessionWebSocketTask, "closeCode", ws_close_code); M(NSURLSessionWebSocketTask, "closeReason", ws_close_reason);
    M(NSURLSessionWebSocketTask, "maximumMessageSize", ws_max); M(NSURLSessionWebSocketTask, "setMaximumMessageSize:", ws_set_max);
    DEF(NSURLSessionWebSocketMessage, ob);
    M(NSURLSessionWebSocketMessage, "initWithData:", wm_init_data); M(NSURLSessionWebSocketMessage, "initWithString:", wm_init_str);
    M(NSURLSessionWebSocketMessage, "type", wm_type); M(NSURLSessionWebSocketMessage, "data", wm_data);
    M(NSURLSessionWebSocketMessage, "string", wm_string); M(NSURLSessionWebSocketMessage, "dealloc", wm_dealloc);
}

/* ================================================================ registro */
static BOOL yes_(id s, SEL c) { (void)s; (void)c; return YES; }
void cx_urlsession_init(void) {
    cx_class_t *ob = &NSObject_c;
    DEF(NSHTTPCookie, ob);
    CM(NSHTTPCookie, "cookieWithProperties:", cookie_with_props); M(NSHTTPCookie, "initWithProperties:", cookie_init_props);
    CM(NSHTTPCookie, "cookiesWithResponseHeaderFields:forURL:", cookies_with_fields);
    CM(NSHTTPCookie, "requestHeaderFieldsWithCookies:", cookie_req_fields);
    M(NSHTTPCookie, "name", cookie_name); M(NSHTTPCookie, "value", cookie_value); M(NSHTTPCookie, "domain", cookie_domain);
    M(NSHTTPCookie, "path", cookie_path); M(NSHTTPCookie, "expiresDate", cookie_expires); M(NSHTTPCookie, "isSecure", cookie_secure);
    M(NSHTTPCookie, "isHTTPOnly", cookie_httponly); M(NSHTTPCookie, "isSessionOnly", cookie_session);
    M(NSHTTPCookie, "sameSitePolicy", cookie_samesite); M(NSHTTPCookie, "version", cookie_version);
    M(NSHTTPCookie, "comment", cookie_nil); M(NSHTTPCookie, "commentURL", cookie_nil); M(NSHTTPCookie, "portList", cookie_nil);
    M(NSHTTPCookie, "properties", cookie_props); M(NSHTTPCookie, "description", cookie_desc);
    M(NSHTTPCookie, "isEqual:", cookie_eq); M(NSHTTPCookie, "hash", cookie_hash); M(NSHTTPCookie, "dealloc", cookie_dealloc);
    DEF(NSHTTPCookieStorage, ob);
    CM(NSHTTPCookieStorage, "sharedHTTPCookieStorage", cookies_shared);
    CM(NSHTTPCookieStorage, "sharedCookieStorageForGroupContainerIdentifier:", cookies_group);
    M(NSHTTPCookieStorage, "cookies", storage_cookies); M(NSHTTPCookieStorage, "cookiesForURL:", storage_for_url);
    M(NSHTTPCookieStorage, "setCookie:", storage_set); M(NSHTTPCookieStorage, "setCookies:forURL:mainDocumentURL:", storage_set_many);
    M(NSHTTPCookieStorage, "deleteCookie:", storage_del); M(NSHTTPCookieStorage, "removeCookiesSinceDate:", storage_remove_since);
    M(NSHTTPCookieStorage, "cookieAcceptPolicy", storage_policy); M(NSHTTPCookieStorage, "setCookieAcceptPolicy:", storage_set_policy);
    M(NSHTTPCookieStorage, "sortedCookiesUsingDescriptors:", storage_sorted);
    M(NSHTTPCookieStorage, "storeCookies:forTask:", storage_store_task); M(NSHTTPCookieStorage, "getCookiesForTask:completionHandler:", storage_get_task);
    M(NSHTTPCookieStorage, "dealloc", storage_dealloc);
    DEF(NSURLCredential, ob);
    CM(NSURLCredential, "credentialWithUser:password:persistence:", cred_with); M(NSURLCredential, "initWithUser:password:persistence:", cred_init);
    CM(NSURLCredential, "credentialForTrust:", cred_for_trust); M(NSURLCredential, "initWithTrust:", cred_init_trust);
    CM(NSURLCredential, "credentialWithIdentity:certificates:persistence:", cred_with_ident);
    M(NSURLCredential, "initWithIdentity:certificates:persistence:", cred_init_ident);
    M(NSURLCredential, "user", cred_user); M(NSURLCredential, "password", cred_pass); M(NSURLCredential, "hasPassword", cred_has_pass);
    M(NSURLCredential, "persistence", cred_pers); M(NSURLCredential, "identity", cred_identity); M(NSURLCredential, "certificates", cred_certs);
    M(NSURLCredential, "dealloc", cred_dealloc);
    DEF(NSURLCredentialStorage, ob);
    CM(NSURLCredentialStorage, "sharedCredentialStorage", credstore_shared);
    M(NSURLCredentialStorage, "credentialsForProtectionSpace:", credstore_for_space); M(NSURLCredentialStorage, "allCredentials", credstore_all);
    M(NSURLCredentialStorage, "setCredential:forProtectionSpace:", credstore_set_cred);
    M(NSURLCredentialStorage, "setDefaultCredential:forProtectionSpace:", credstore_set_default);
    M(NSURLCredentialStorage, "defaultCredentialForProtectionSpace:", credstore_default);
    M(NSURLCredentialStorage, "removeCredential:forProtectionSpace:", credstore_remove);
    M(NSURLCredentialStorage, "removeCredential:forProtectionSpace:options:", credstore_remove_opts);
    DEF(NSURLProtectionSpace, ob);
    M(NSURLProtectionSpace, "initWithHost:port:protocol:realm:authenticationMethod:", space_init);
    M(NSURLProtectionSpace, "initWithProxyHost:port:type:realm:authenticationMethod:", space_init_proxy);
    M(NSURLProtectionSpace, "host", space_host); M(NSURLProtectionSpace, "port", space_port); M(NSURLProtectionSpace, "protocol", space_proto);
    M(NSURLProtectionSpace, "realm", space_realm); M(NSURLProtectionSpace, "authenticationMethod", space_method);
    M(NSURLProtectionSpace, "serverTrust", space_trust); M(NSURLProtectionSpace, "isProxy", space_is_proxy);
    M(NSURLProtectionSpace, "proxyType", space_proxy_type); M(NSURLProtectionSpace, "receivesCredentialSecurely", space_secure);
    M(NSURLProtectionSpace, "distinguishedNames", space_dn); M(NSURLProtectionSpace, "isEqual:", space_eq);
    M(NSURLProtectionSpace, "dealloc", space_dealloc);
    DEF(NSURLAuthenticationChallenge, ob);
    M(NSURLAuthenticationChallenge, "initWithProtectionSpace:proposedCredential:previousFailureCount:failureResponse:error:sender:", chal_init);
    M(NSURLAuthenticationChallenge, "protectionSpace", chal_space); M(NSURLAuthenticationChallenge, "proposedCredential", chal_proposed);
    M(NSURLAuthenticationChallenge, "previousFailureCount", chal_prev); M(NSURLAuthenticationChallenge, "failureResponse", chal_failure);
    M(NSURLAuthenticationChallenge, "error", chal_error); M(NSURLAuthenticationChallenge, "sender", chal_sender);
    M(NSURLAuthenticationChallenge, "dealloc", chal_dealloc);
    DEF(NSURLCache, ob);
    CM(NSURLCache, "sharedURLCache", cache_shared); CM(NSURLCache, "setSharedURLCache:", cache_set_shared);
    M(NSURLCache, "initWithMemoryCapacity:diskCapacity:diskPath:", cache_init);
    M(NSURLCache, "initWithMemoryCapacity:diskCapacity:directoryURL:", cache_init_dir);
    M(NSURLCache, "cachedResponseForRequest:", cache_get); M(NSURLCache, "storeCachedResponse:forRequest:", cache_store);
    M(NSURLCache, "removeCachedResponseForRequest:", cache_remove); M(NSURLCache, "removeAllCachedResponses", cache_remove_all);
    M(NSURLCache, "removeCachedResponsesSinceDate:", cache_remove_since);
    M(NSURLCache, "memoryCapacity", cache_memcap); M(NSURLCache, "diskCapacity", cache_diskcap);
    M(NSURLCache, "setMemoryCapacity:", cache_set_memcap); M(NSURLCache, "setDiskCapacity:", cache_set_diskcap);
    M(NSURLCache, "currentMemoryUsage", cache_memuse); M(NSURLCache, "currentDiskUsage", cache_diskuse);
    M(NSURLCache, "storeCachedResponse:forDataTask:", cache_store_task); M(NSURLCache, "getCachedResponseForDataTask:completionHandler:", cache_get_task);
    M(NSURLCache, "removeCachedResponseForDataTask:", cache_remove_task);
    DEF(NSCachedURLResponse, ob);
    M(NSCachedURLResponse, "initWithResponse:data:", cached_init); M(NSCachedURLResponse, "initWithResponse:data:userInfo:storagePolicy:", cached_init4);
    M(NSCachedURLResponse, "response", cached_resp); M(NSCachedURLResponse, "data", cached_data);
    M(NSCachedURLResponse, "userInfo", cached_info); M(NSCachedURLResponse, "storagePolicy", cached_policy);
    M(NSCachedURLResponse, "dealloc", cached_dealloc);

    DEF(NSURLSessionConfiguration, ob);
    CM(NSURLSessionConfiguration, "defaultSessionConfiguration", cfg_default);
    CM(NSURLSessionConfiguration, "ephemeralSessionConfiguration", cfg_ephemeral);
    CM(NSURLSessionConfiguration, "backgroundSessionConfigurationWithIdentifier:", cfg_background);
    CM(NSURLSessionConfiguration, "backgroundSessionConfiguration:", cfg_background);
    M(NSURLSessionConfiguration, "copy", cfg_copy); M(NSURLSessionConfiguration, "copyWithZone:", cfg_copyz);
    M(NSURLSessionConfiguration, "dealloc", cfg_dealloc);
#define CP(get, set, gn, sn) M(NSURLSessionConfiguration, gn, get); M(NSURLSessionConfiguration, sn, set);
    CP(cfg_ident, cfg_set_ident, "identifier", "setIdentifier:")
    CP(cfg_headers, cfg_set_headers, "HTTPAdditionalHeaders", "setHTTPAdditionalHeaders:")
    CP(cfg_cookies, cfg_set_cookies_store, "HTTPCookieStorage", "setHTTPCookieStorage:")
    CP(cfg_cache, cfg_set_cache, "URLCache", "setURLCache:")
    CP(cfg_creds, cfg_set_creds, "URLCredentialStorage", "setURLCredentialStorage:")
    CP(cfg_proxy, cfg_set_proxy, "connectionProxyDictionary", "setConnectionProxyDictionary:")
    CP(cfg_protocols, cfg_set_protocols, "protocolClasses", "setProtocolClasses:")
    CP(cfg_container, cfg_set_container, "sharedContainerIdentifier", "setSharedContainerIdentifier:")
    CP(cfg_treq, cfg_set_treq, "timeoutIntervalForRequest", "setTimeoutIntervalForRequest:")
    CP(cfg_tres, cfg_set_tres, "timeoutIntervalForResource", "setTimeoutIntervalForResource:")
    CP(cfg_policy, cfg_set_policy, "requestCachePolicy", "setRequestCachePolicy:")
    CP(cfg_accept, cfg_set_accept, "HTTPCookieAcceptPolicy", "setHTTPCookieAcceptPolicy:")
    CP(cfg_service, cfg_set_service, "networkServiceType", "setNetworkServiceType:")
    CP(cfg_tlsmin, cfg_set_tlsmin, "TLSMinimumSupportedProtocolVersion", "setTLSMinimumSupportedProtocolVersion:")
    CP(cfg_tlsmax, cfg_set_tlsmax, "TLSMaximumSupportedProtocolVersion", "setTLSMaximumSupportedProtocolVersion:")
    CP(cfg_tlsmin, cfg_set_tlsmin, "TLSMinimumSupportedProtocol", "setTLSMinimumSupportedProtocol:")
    CP(cfg_tlsmax, cfg_set_tlsmax, "TLSMaximumSupportedProtocol", "setTLSMaximumSupportedProtocol:")
    CP(cfg_mptcp, cfg_set_mptcp, "multipathServiceType", "setMultipathServiceType:")
    CP(cfg_maxhost, cfg_set_maxhost, "HTTPMaximumConnectionsPerHost", "setHTTPMaximumConnectionsPerHost:")
    CP(cfg_setck, cfg_set_setck, "HTTPShouldSetCookies", "setHTTPShouldSetCookies:")
    CP(cfg_cell, cfg_set_cell, "allowsCellularAccess", "setAllowsCellularAccess:")
    CP(cfg_exp, cfg_set_exp, "allowsExpensiveNetworkAccess", "setAllowsExpensiveNetworkAccess:")
    CP(cfg_cons, cfg_set_cons, "allowsConstrainedNetworkAccess", "setAllowsConstrainedNetworkAccess:")
    CP(cfg_waits, cfg_set_waits, "waitsForConnectivity", "setWaitsForConnectivity:")
    CP(cfg_disc, cfg_set_disc, "isDiscretionary", "setDiscretionary:")
    CP(cfg_pipe, cfg_set_pipe, "HTTPShouldUsePipelining", "setHTTPShouldUsePipelining:")
    CP(cfg_launch, cfg_set_launch, "sessionSendsLaunchEvents", "setSessionSendsLaunchEvents:")
#undef CP

    DEF(NSURLSession, ob);
    CM(NSURLSession, "sharedSession", sess_shared); CM(NSURLSession, "sessionWithConfiguration:", sess_with_cfg);
    CM(NSURLSession, "sessionWithConfiguration:delegate:delegateQueue:", sess_with_dlg);
    M(NSURLSession, "configuration", sess_cfg); M(NSURLSession, "delegate", sess_dlg); M(NSURLSession, "delegateQueue", sess_queue_obj);
    M(NSURLSession, "sessionDescription", sess_desc); M(NSURLSession, "setSessionDescription:", sess_set_desc);
    M(NSURLSession, "dataTaskWithRequest:", s_data_req); M(NSURLSession, "dataTaskWithURL:", s_data_url);
    M(NSURLSession, "dataTaskWithRequest:completionHandler:", s_data_req_h); M(NSURLSession, "dataTaskWithURL:completionHandler:", s_data_url_h);
    M(NSURLSession, "uploadTaskWithRequest:fromData:", s_up_data); M(NSURLSession, "uploadTaskWithRequest:fromData:completionHandler:", s_up_data_h);
    M(NSURLSession, "uploadTaskWithRequest:fromFile:", s_up_file); M(NSURLSession, "uploadTaskWithRequest:fromFile:completionHandler:", s_up_file_h);
    M(NSURLSession, "uploadTaskWithStreamedRequest:", s_up_stream);
    M(NSURLSession, "downloadTaskWithRequest:", s_dl_req); M(NSURLSession, "downloadTaskWithURL:", s_dl_url);
    M(NSURLSession, "downloadTaskWithRequest:completionHandler:", s_dl_req_h); M(NSURLSession, "downloadTaskWithURL:completionHandler:", s_dl_url_h);
    M(NSURLSession, "downloadTaskWithResumeData:", s_dl_resume); M(NSURLSession, "downloadTaskWithResumeData:completionHandler:", s_dl_resume_h);
    M(NSURLSession, "getTasksWithCompletionHandler:", s_get_tasks); M(NSURLSession, "getAllTasksWithCompletionHandler:", s_get_all);
    M(NSURLSession, "invalidateAndCancel", s_invalidate_cancel); M(NSURLSession, "finishTasksAndInvalidate", s_finish_invalidate);
    M(NSURLSession, "resetWithCompletionHandler:", s_reset); M(NSURLSession, "flushWithCompletionHandler:", s_flush);
    M(NSURLSession, "dealloc", sess_dealloc);
    M(NSURLSession, "webSocketTaskWithURL:", s_ws_url); M(NSURLSession, "webSocketTaskWithURL:protocols:", s_ws_url_p);
    M(NSURLSession, "webSocketTaskWithRequest:", s_ws_req);

    DEF(NSURLSessionTask, ob);
    M(NSURLSessionTask, "taskIdentifier", task_ident); M(NSURLSessionTask, "originalRequest", task_orig);
    M(NSURLSessionTask, "currentRequest", task_cur); M(NSURLSessionTask, "response", task_resp); M(NSURLSessionTask, "error", task_err);
    M(NSURLSessionTask, "state", task_state); M(NSURLSessionTask, "countOfBytesReceived", task_recv);
    M(NSURLSessionTask, "countOfBytesSent", task_sent); M(NSURLSessionTask, "countOfBytesExpectedToSend", task_to_send);
    M(NSURLSessionTask, "countOfBytesExpectedToReceive", task_to_recv);
    M(NSURLSessionTask, "priority", task_prio); M(NSURLSessionTask, "setPriority:", task_set_prio);
    M(NSURLSessionTask, "taskDescription", task_descr); M(NSURLSessionTask, "setTaskDescription:", task_set_descr);
    M(NSURLSessionTask, "delegate", task_delegate); M(NSURLSessionTask, "setDelegate:", task_set_delegate);
    M(NSURLSessionTask, "earliestBeginDate", task_earliest); M(NSURLSessionTask, "setEarliestBeginDate:", task_set_earliest);
    M(NSURLSessionTask, "progress", task_progress);
    M(NSURLSessionTask, "resume", task_resume); M(NSURLSessionTask, "suspend", task_suspend); M(NSURLSessionTask, "cancel", task_cancel);
    M(NSURLSessionTask, "dealloc", task_dealloc);
    DEF(NSURLSessionDataTask, &NSURLSessionTask_c);
    DEF(NSURLSessionUploadTask, &NSURLSessionDataTask_c);
    DEF(NSURLSessionDownloadTask, &NSURLSessionTask_c);
    M(NSURLSessionDownloadTask, "cancelByProducingResumeData:", dl_cancel_resume);
    DEF(NSURLSessionStreamTask, &NSURLSessionTask_c);
    cx_ws_register();
    DEF(NSURLSessionTaskMetrics, ob);
    M(NSURLSessionTaskMetrics, "transactionMetrics", metrics_tx); M(NSURLSessionTaskMetrics, "taskInterval", metrics_interval);
    M(NSURLSessionTaskMetrics, "redirectCount", metrics_redirects);
    DEF(NSURLSessionTaskTransactionMetrics, ob);
    M(NSURLSessionTaskTransactionMetrics, "request", txm_req); M(NSURLSessionTaskTransactionMetrics, "response", txm_resp);
    M(NSURLSessionTaskTransactionMetrics, "fetchStartDate", txm_fetch); M(NSURLSessionTaskTransactionMetrics, "responseEndDate", txm_end);
    M(NSURLSessionTaskTransactionMetrics, "networkProtocolName", txm_proto);
    M(NSURLSessionTaskTransactionMetrics, "isReusedConnection", txm_reused); M(NSURLSessionTaskTransactionMetrics, "isProxyConnection", txm_proxy);
    M(NSURLSessionTaskTransactionMetrics, "resourceFetchType", txm_type);

    DEF(NSURLProtocol, ob);
    CM(NSURLProtocol, "registerClass:", proto_register); CM(NSURLProtocol, "unregisterClass:", proto_unregister);
    CM(NSURLProtocol, "canInitWithRequest:", proto_can); CM(NSURLProtocol, "canonicalRequestForRequest:", proto_canon);
    CM(NSURLProtocol, "requestIsCacheEquivalent:toRequest:", proto_equiv);
    CM(NSURLProtocol, "propertyForKey:inRequest:", proto_prop); CM(NSURLProtocol, "setProperty:forKey:inRequest:", proto_set_prop);
    CM(NSURLProtocol, "removePropertyForKey:inRequest:", proto_del_prop);
    M(NSURLProtocol, "initWithRequest:cachedResponse:client:", proto_init); M(NSURLProtocol, "initWithTask:cachedResponse:client:", proto_init_task);
    M(NSURLProtocol, "request", proto_req); M(NSURLProtocol, "cachedResponse", proto_cached); M(NSURLProtocol, "client", proto_client);
    M(NSURLProtocol, "task", proto_task); M(NSURLProtocol, "startLoading", proto_noop); M(NSURLProtocol, "stopLoading", proto_noop);
    M(NSURLProtocol, "dealloc", proto_dealloc);
    DEF(MOSURLProtocolClient, ob);
    M(MOSURLProtocolClient, "URLProtocol:didReceiveResponse:cacheStoragePolicy:", pc_resp);
    M(MOSURLProtocolClient, "URLProtocol:didLoadData:", pc_data); M(MOSURLProtocolClient, "URLProtocolDidFinishLoading:", pc_finish);
    M(MOSURLProtocolClient, "URLProtocol:didFailWithError:", pc_fail);
    M(MOSURLProtocolClient, "URLProtocol:wasRedirectedToRequest:redirectResponse:", pc_redirect);
    M(MOSURLProtocolClient, "URLProtocol:cachedResponseIsValid:", pc_cached);
    M(MOSURLProtocolClient, "URLProtocol:didReceiveAuthenticationChallenge:", pc_chal);
    M(MOSURLProtocolClient, "URLProtocol:didCancelAuthenticationChallenge:", pc_chal);
    M(MOSURLProtocolClient, "conformsToProtocol:", yes_);

    DEF(NSURLConnection, ob);
    CM(NSURLConnection, "sendSynchronousRequest:returningResponse:error:", conn_send_sync);
    CM(NSURLConnection, "sendAsynchronousRequest:queue:completionHandler:", conn_send_async);
    CM(NSURLConnection, "connectionWithRequest:delegate:", conn_with); CM(NSURLConnection, "canHandleRequest:", conn_can);
    M(NSURLConnection, "initWithRequest:delegate:", conn_init2); M(NSURLConnection, "initWithRequest:delegate:startImmediately:", conn_init3);
    M(NSURLConnection, "start", conn_start); M(NSURLConnection, "cancel", conn_cancel);
    M(NSURLConnection, "setDelegateQueue:", conn_set_queue); M(NSURLConnection, "scheduleInRunLoop:forMode:", conn_sched);
    M(NSURLConnection, "unscheduleFromRunLoop:forMode:", conn_sched);
    M(NSURLConnection, "originalRequest", conn_req); M(NSURLConnection, "currentRequest", conn_cur);
    M(NSURLConnection, "dealloc", conn_dealloc);
    DEF(MOSURLConnectionAdapter, ob);
    M(MOSURLConnectionAdapter, "URLSession:dataTask:didReceiveResponse:completionHandler:", ad_resp);
    M(MOSURLConnectionAdapter, "URLSession:dataTask:didReceiveData:", ad_data);
    M(MOSURLConnectionAdapter, "URLSession:task:willPerformHTTPRedirection:newRequest:completionHandler:", ad_redirect);
    M(MOSURLConnectionAdapter, "URLSession:task:didSendBodyData:totalBytesSent:totalBytesExpectedToSend:", ad_sent);
    M(MOSURLConnectionAdapter, "URLSession:task:didCompleteWithError:", ad_done_rel);
    M(MOSURLConnectionAdapter, "URLSession:dataTask:willCacheResponse:completionHandler:", ad_cache);
    M(MOSURLConnectionAdapter, "URLSession:task:didReceiveChallenge:completionHandler:", ad_chal);
    (void)ad_sess_chal;
    DEF(MOSChallengeSender, ob);
    M(MOSChallengeSender, "useCredential:forAuthenticationChallenge:", snd_use);
    M(MOSChallengeSender, "continueWithoutCredentialForAuthenticationChallenge:", snd_without);
    M(MOSChallengeSender, "cancelAuthenticationChallenge:", snd_cancel);
    M(MOSChallengeSender, "performDefaultHandlingForAuthenticationChallenge:", snd_default);
    M(MOSChallengeSender, "rejectProtectionSpaceAndContinueWithChallenge:", snd_reject);
}
