/*
 * mOS — CFNetwork HTTP (docs/rede-apps.md, item 9b): CFHTTPMessage sobre mnet_msg.c, CFHTTPAuthentication (Basic e
 * Digest MD5/SHA-256 do mnet_auth.c) e o stream de pedido HTTP (CFReadStreamCreateForHTTPRequest e
 * ForStreamedHTTPRequest): um fio roda mnet_fetch e escreve o corpo num par do mnet_stream; o app lê o NSInputStream do
 * par, com kCFStreamPropertyHTTPResponseHeader pronto antes do primeiro byte.
 *
 * Como no CFNetwork: o stream HTTP não segue redirect sem kCFStreamPropertyHTTPShouldAutoredirect, não usa cookies nem
 * proxy do sistema (só kCFStreamPropertyHTTPProxy), e não responde a desafio de autenticação sozinho (o app monta um
 * CFHTTPAuthentication com a resposta 401/407 e aplica no pedido seguinte). gzip/deflate/br são decodificados.
 */
#include "foundation.h"
#include "libc.h"
#include "mnet.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void *objc_autoreleasePoolPush(void);
void objc_autoreleasePoolPop(void *p);
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
int pthread_detach(unsigned long t);
void os_unfair_lock_lock(void *l);
void os_unfair_lock_unlock(void *l);
unsigned char CFReadStreamOpen(id s);
long CFReadStreamRead(id s, u8 *b, long n);
void CFReadStreamClose(id s);
char *cx_proxy_for(id settings, const char *url);

typedef struct {
    id (*get)(void *ext, id key);
    int (*set)(void *ext, id key, id v);
    void (*release)(void *ext);
} cx_stream_ext_t;
id cx_netstream_new_input(mnet_stream_t *m, void *ext, const cx_stream_ext_t *ops);

static id cls(const char *n) { return (id)(void *)cx_class_named(n); }
static BOOL responds(id o, const char *s) { return o && MSG(BOOL, o, "respondsToSelector:", sel_registerName(s)); }
static const char *cs(id o) { return o && cx_is_str(o) ? cx_cstr(o) : 0; }
static id ns(const char *s) { return cx_str(s ? s : ""); }
static int str_eq(id a, const void *b) { return a && b && (a == (id)b || MSG(BOOL, a, "isEqual:", (id)b)); }
static int boolv(id v) { return v && responds(v, "boolValue") && MSG0(BOOL, v, "boolValue"); }
static char *dupc(const char *s) { return s ? mnet_strndup(s, strlen(s)) : 0; }
static id url_string(id url) {
    if (!url) return 0;
    if (cx_is_str(url)) return url;
    return responds(url, "absoluteString") ? MSG0(id, url, "absoluteString") : 0;
}

/* ---------------------------------------------------------------- constantes */
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } ks_t;
#define K(var, text) \
    static ks_t var##_cs = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; \
    const void *var = &var##_cs;
K(kCFHTTPVersion1_0, "HTTP/1.0") K(kCFHTTPVersion1_1, "HTTP/1.1") K(kCFHTTPVersion2_0, "HTTP/2.0") K(kCFHTTPVersion3_0, "HTTP/3.0")
K(kCFHTTPAuthenticationSchemeBasic, "Basic") K(kCFHTTPAuthenticationSchemeDigest, "Digest")
K(kCFHTTPAuthenticationSchemeNTLM, "NTLM") K(kCFHTTPAuthenticationSchemeKerberos, "Kerberos")
K(kCFHTTPAuthenticationSchemeNegotiate, "Negotiate") K(kCFHTTPAuthenticationSchemeNegotiate2, "Negotiate2")
K(kCFHTTPAuthenticationSchemeXMobileMeAuthToken, "X-MobileMe-AuthToken") K(kCFHTTPAuthenticationSchemeOAuth1, "OAuth1")
K(kCFHTTPAuthenticationUsername, "kCFHTTPAuthenticationUsername")
K(kCFHTTPAuthenticationPassword, "kCFHTTPAuthenticationPassword")
K(kCFHTTPAuthenticationAccountDomain, "kCFHTTPAuthenticationAccountDomain")
K(kCFStreamPropertyHTTPResponseHeader, "kCFStreamPropertyHTTPResponseHeader")
K(kCFStreamPropertyHTTPFinalURL, "kCFStreamPropertyHTTPFinalURL")
K(kCFStreamPropertyHTTPFinalRequest, "kCFStreamPropertyHTTPFinalRequest")
K(kCFStreamPropertyHTTPProxy, "kCFStreamPropertyHTTPProxy")
K(kCFStreamPropertyHTTPProxyHost, "HTTPProxy") K(kCFStreamPropertyHTTPProxyPort, "HTTPPort")
K(kCFStreamPropertyHTTPSProxyHost, "HTTPSProxy") K(kCFStreamPropertyHTTPSProxyPort, "HTTPSPort")
K(kCFStreamPropertyHTTPShouldAutoredirect, "kCFStreamPropertyHTTPShouldAutoredirect")
K(kCFStreamPropertyHTTPAttemptPersistentConnection, "kCFStreamPropertyHTTPAttemptPersistentConnection")
K(kCFStreamPropertyHTTPRequestBytesWrittenCount, "kCFStreamPropertyHTTPRequestBytesWrittenCount")
extern const void *kCFStreamPropertySSLSettings;

/* ---------------------------------------------------------------- CFHTTPMessage */
typedef struct { mnet_msg_t m; char *final_url; } hm_t;
static cx_class_t HM_c, HM_m, HA_c, HA_m;
static hm_t *HM(id o) { return o && object_getClass(o) == &HM_c ? *(hm_t **)((u8 *)o + 8) : 0; }
static void hm_dealloc(id s, SEL c) {
    hm_t *h = *(hm_t **)((u8 *)s + 8);
    if (h) { mnet_msg_free(&h->m); free(h->final_url); free(h); }
    IMP sup = cx_lookup(cx_class_named("NSObject"), c);
    if (sup) ((void (*)(id, SEL))sup)(s, c);
}
static id hm_new(void) {
    id o = cx_alloc_instance(&HM_c);
    hm_t *h = o ? calloc(1, sizeof *h) : 0;
    if (!h) { if (o) objc_release(o); return 0; }
    *(hm_t **)((u8 *)o + 8) = h;
    return o;
}
static id hm_description(id s, SEL c) {
    (void)c;
    hm_t *h = HM(s);
    char ver[12], b[600];
    if (!h) return ns("<CFHTTPMessage>");
    mnet_msg_version(&h->m, ver);
    if (h->m.is_request) snprintf(b, sizeof b, "<CFHTTPMessage %p>{%s %s %s}", (void *)s, h->m.method ? h->m.method : "", h->m.target ? h->m.target : "", ver);
    else snprintf(b, sizeof b, "<CFHTTPMessage %p>{%s %d}", (void *)s, ver, h->m.status);
    return ns(b);
}
u64 CFHTTPMessageGetTypeID(void) { return 0x2a0; }
u64 CFHTTPAuthenticationGetTypeID(void) { return 0x2a1; }
static u64 hm_typeid(id s, SEL c) { (void)s; (void)c; return 0x2a0; }

id CFHTTPMessageCreateRequest(void *alloc, id method, id url, id version) {
    (void)alloc;
    id o = hm_new();
    if (!o) return 0;
    if (mnet_msg_init_request(&HM(o)->m, cs(method), cs(url_string(url)), cs(version))) { objc_release(o); return 0; }
    return o;
}
id CFHTTPMessageCreateResponse(void *alloc, long status, id desc, id version) {
    (void)alloc;
    id o = hm_new();
    if (!o) return 0;
    if (mnet_msg_init_response(&HM(o)->m, (int)status, cs(desc), cs(version))) { objc_release(o); return 0; }
    return o;
}
id CFHTTPMessageCreateEmpty(void *alloc, unsigned char is_request) {
    (void)alloc;
    id o = hm_new();
    if (o) mnet_msg_init_empty(&HM(o)->m, is_request != 0);
    return o;
}
id CFHTTPMessageCreateCopy(void *alloc, id msg) {
    (void)alloc;
    hm_t *src = HM(msg);
    id o = src ? hm_new() : 0;
    if (!o) return 0;
    if (mnet_msg_copy(&HM(o)->m, &src->m)) { objc_release(o); return 0; }
    HM(o)->final_url = dupc(src->final_url);
    return o;
}
unsigned char CFHTTPMessageIsRequest(id msg) { hm_t *h = HM(msg); return h && h->m.is_request; }
id CFHTTPMessageCopyVersion(id msg) {
    hm_t *h = HM(msg);
    char v[12];
    if (!h) return 0;
    mnet_msg_version(&h->m, v);
    return objc_retain(ns(v));
}
id CFHTTPMessageCopyBody(id msg) {
    hm_t *h = HM(msg);
    if (!h || !h->m.body.n) return 0;
    return objc_retain(MSG(id, cls("NSData"), "dataWithBytes:length:", h->m.body.p, (u64)h->m.body.n));
}
void CFHTTPMessageSetBody(id msg, id data) {
    hm_t *h = HM(msg);
    if (!h) return;
    h->m.body.n = 0;
    u64 n = data ? MSG0(u64, data, "length") : 0;
    if (n) mnet_buf_put(&h->m.body, MSG0(const void *, data, "bytes"), (size_t)n);
}
id CFHTTPMessageCopyHeaderFieldValue(id msg, id name) {
    hm_t *h = HM(msg);
    const char *n = cs(name);
    if (!h || !n) return 0;
    char *v = mnet_hdrs_join(&h->m.hdrs, n);
    if (!v) return 0;
    id s = ns(v);
    free(v);
    return objc_retain(s);
}
id CFHTTPMessageCopyAllHeaderFields(id msg) {
    hm_t *h = HM(msg);
    if (!h) return 0;
    id d = MSG0(id, MSG0(id, cls("NSMutableDictionary"), "alloc"), "init");
    for (int i = 0; i < h->m.hdrs.n; i++) {
        int seen = 0;
        for (int k = 0; k < i && !seen; k++) seen = mnet_ieq(h->m.hdrs.v[k].name, h->m.hdrs.v[i].name);
        if (seen) continue;
        char *v = mnet_hdrs_join(&h->m.hdrs, h->m.hdrs.v[i].name);
        MSG(void, d, "setObject:forKey:", ns(v ? v : ""), ns(h->m.hdrs.v[i].name));
        free(v);
    }
    return d;
}
void CFHTTPMessageSetHeaderFieldValue(id msg, id name, id value) {
    hm_t *h = HM(msg);
    const char *n = cs(name);
    if (!h || !n) return;
    const char *v = cs(value);
    if (!v) mnet_hdrs_del(&h->m.hdrs, n);
    else mnet_hdrs_set(&h->m.hdrs, n, v);
}
unsigned char CFHTTPMessageAppendBytes(id msg, const u8 *b, long n) {
    hm_t *h = HM(msg);
    if (!h || n < 0) return 0;
    return mnet_msg_append(&h->m, b, (size_t)n) == 0;
}
unsigned char CFHTTPMessageIsHeaderComplete(id msg) { hm_t *h = HM(msg); return h && h->m.head_done; }
id CFHTTPMessageCopySerializedMessage(id msg) {
    hm_t *h = HM(msg);
    mnet_buf_t b = {0};
    if (!h || !h->m.head_done || mnet_msg_serialize(&h->m, &b)) { mnet_buf_free(&b); return 0; }
    id d = MSG(id, cls("NSData"), "dataWithBytes:length:", b.p ? (const void *)b.p : "", (u64)b.n);
    mnet_buf_free(&b);
    return objc_retain(d);
}
id CFHTTPMessageCopyRequestURL(id msg) {
    hm_t *h = HM(msg);
    if (!h || !h->m.is_request || !h->m.target) return 0;
    id u = MSG(id, cls("NSURL"), "URLWithString:", ns(h->m.target));
    return u ? objc_retain(u) : 0;
}
id CFHTTPMessageCopyRequestMethod(id msg) { hm_t *h = HM(msg); return h && h->m.is_request && h->m.method ? objc_retain(ns(h->m.method)) : 0; }
long CFHTTPMessageGetResponseStatusCode(id msg) { hm_t *h = HM(msg); return h && !h->m.is_request ? h->m.status : 0; }
id CFHTTPMessageCopyResponseStatusLine(id msg) {
    hm_t *h = HM(msg);
    if (!h || h->m.is_request || !h->m.head_done) return 0;
    char v[12], b[300];
    mnet_msg_version(&h->m, v);
    const char *why = h->m.reason && *h->m.reason ? h->m.reason : mnet_status_text(h->m.status);
    snprintf(b, sizeof b, "%s %03d%s%s", v, h->m.status, *why ? " " : "", why);
    return objc_retain(ns(b));
}

/* ---------------------------------------------------------------- CFHTTPAuthentication */
typedef struct {
    char *challenge;                     /* o valor do WWW-Authenticate/Proxy-Authenticate escolhido */
    int scheme;                          /* 1 Basic, 2 Digest MD5, 3 Digest SHA-256 */
    int proxy;
    char realm[256];
    char *url;                           /* URL do pedido que levou o desafio, se conhecida */
    int nc;
    unsigned lock;
} ha_t;
static ha_t *HA(id o) { return o && object_getClass(o) == &HA_c ? *(ha_t **)((u8 *)o + 8) : 0; }
static void ha_dealloc(id s, SEL c) {
    ha_t *a = *(ha_t **)((u8 *)s + 8);
    if (a) { free(a->challenge); free(a->url); free(a); }
    IMP sup = cx_lookup(cx_class_named("NSObject"), c);
    if (sup) ((void (*)(id, SEL))sup)(s, c);
}
/* desafio de um esquema pedido ("Basic"/"Digest") ou o melhor que o mnet entende */
static const char *pick(const mnet_hdrs_t *hd, const char *field, const char *want, int *score) {
    if (!want) return mnet_auth_pick(hd, field, score);
    size_t wl = strlen(want);
    for (int i = 0; i < hd->n; i++) {
        if (!mnet_ieq(hd->v[i].name, field)) continue;
        const char *v = hd->v[i].value;
        if (!mnet_ieqn(v, want, wl) || (v[wl] && v[wl] != ' ')) continue;
        if (mnet_ieqn(want, "basic", 5)) { *score = 1; return v; }
        char alg[32] = "MD5";
        mnet_auth_param(v, "algorithm", alg, sizeof alg);
        *score = mnet_ieqn(alg, "SHA-256", 7) ? 3 : mnet_ieqn(alg, "MD5", 3) ? 2 : 0;
        return *score ? v : 0;
    }
    *score = 0;
    return 0;
}
static id auth_from(hm_t *h, const char *want) {
    id o = cx_alloc_instance(&HA_c);
    ha_t *a = o ? calloc(1, sizeof *a) : 0;
    if (!a) { if (o) objc_release(o); return 0; }
    *(ha_t **)((u8 *)o + 8) = a;
    if (!h || h->m.is_request) return o;                      /* inválido */
    a->proxy = h->m.status == 407;
    int sc = 0;
    const char *ch = pick(&h->m.hdrs, a->proxy ? "proxy-authenticate" : "www-authenticate", want, &sc);
    if (ch && sc) {
        a->challenge = dupc(ch);
        a->scheme = sc;
        mnet_auth_param(ch, "realm", a->realm, sizeof a->realm);
    }
    a->url = dupc(h->final_url);
    return o;
}
id CFHTTPAuthenticationCreateFromResponse(void *alloc, id response) { (void)alloc; return auth_from(HM(response), 0); }
typedef struct { long domain; int error; } cf_serr_t;
enum { E_AUTH_TYPE_UNSUPPORTED = -1000, E_AUTH_BAD_PARAMETERS = -1001 };          /* CFStreamErrorHTTPAuthentication */
unsigned char CFHTTPAuthenticationIsValid(id auth, cf_serr_t *err) {
    ha_t *a = HA(auth);
    int ok = a && a->challenge;
    if (err) { err->domain = ok ? 0 : 4; err->error = ok ? 0 : (a && !a->challenge ? E_AUTH_TYPE_UNSUPPORTED : E_AUTH_BAD_PARAMETERS); }
    return (unsigned char)ok;
}
static int same_origin(const char *a, const char *b) {
    mnet_url_t x, y;
    if (mnet_url_parse(a, &x)) return 0;
    if (mnet_url_parse(b, &y)) { mnet_url_free(&x); return 0; }
    int r = mnet_ieq(x.host, y.host) && x.port == y.port && !strcmp(x.scheme, y.scheme);
    mnet_url_free(&x); mnet_url_free(&y);
    return r;
}
unsigned char CFHTTPAuthenticationAppliesToRequest(id auth, id request) {
    ha_t *a = HA(auth);
    hm_t *r = HM(request);
    if (!a || !a->challenge || !r || !r->m.is_request) return 0;
    if (a->proxy || !a->url) return 1;
    return (unsigned char)same_origin(a->url, r->m.target ? r->m.target : "");
}
unsigned char CFHTTPAuthenticationRequiresOrderedRequests(id auth) { ha_t *a = HA(auth); return a && a->scheme >= 2; }
unsigned char CFHTTPAuthenticationRequiresUserNameAndPassword(id auth) { ha_t *a = HA(auth); return a && a->challenge; }
unsigned char CFHTTPAuthenticationRequiresAccountDomain(id auth) { (void)auth; return 0; }      /* só NTLM pede */
id CFHTTPAuthenticationCopyRealm(id auth) { ha_t *a = HA(auth); return a && a->challenge ? objc_retain(ns(a->realm)) : 0; }
id CFHTTPAuthenticationCopyMethod(id auth) {
    ha_t *a = HA(auth);
    if (!a || !a->challenge) return 0;
    return objc_retain((id)(a->scheme == 1 ? kCFHTTPAuthenticationSchemeBasic : kCFHTTPAuthenticationSchemeDigest));
}
/* Digest: o parâmetro domain= (URIs separadas por espaço, RFC 7616 §3.3); senão a URL do pedido */
id CFHTTPAuthenticationCopyDomains(id auth) {
    ha_t *a = HA(auth);
    if (!a || !a->challenge) return 0;
    id arr = MSG0(id, MSG0(id, cls("NSMutableArray"), "alloc"), "init");
    char dom[1024];
    if (a->scheme >= 2 && mnet_auth_param(a->challenge, "domain", dom, sizeof dom)) {
        for (char *p = dom; *p;) {
            while (*p == ' ') p++;
            char *e = p;
            while (*e && *e != ' ') e++;
            if (e > p) {
                char save = *e; *e = 0;
                char *abs = a->url ? mnet_url_resolve(a->url, p) : 0;
                id u = MSG(id, cls("NSURL"), "URLWithString:", ns(abs ? abs : p));
                free(abs);
                if (u) MSG(void, arr, "addObject:", u);
                *e = save;
            }
            p = e;
        }
    } else if (a->url) {
        id u = MSG(id, cls("NSURL"), "URLWithString:", ns(a->url));
        if (u) MSG(void, arr, "addObject:", u);
    }
    return arr;
}
static int apply(hm_t *r, ha_t *a, const char *user, const char *pass) {
    if (!r || !r->m.is_request || !a || !a->challenge || !user || !pass) return 0;
    char *uri = mnet_msg_request_target(&r->m);
    os_unfair_lock_lock(&a->lock);
    int nc = ++a->nc;
    os_unfair_lock_unlock(&a->lock);
    char *v = mnet_auth_value(a->challenge, user, pass, r->m.method ? r->m.method : "GET", uri ? uri : "/", nc, 0);
    free(uri);
    if (!v) return 0;
    mnet_hdrs_set(&r->m.hdrs, a->proxy ? "Proxy-Authorization" : "Authorization", v);
    free(v);
    return 1;
}
unsigned char CFHTTPMessageApplyCredentials(id request, id auth, id user, id pass, cf_serr_t *err) {
    int ok = apply(HM(request), HA(auth), cs(user), cs(pass));
    if (err) { err->domain = ok ? 0 : 4; err->error = ok ? 0 : E_AUTH_BAD_PARAMETERS; }
    return (unsigned char)ok;
}
unsigned char CFHTTPMessageApplyCredentialDictionary(id request, id auth, id dict, cf_serr_t *err) {
    id u = dict ? MSG(id, dict, "objectForKey:", (id)kCFHTTPAuthenticationUsername) : 0;
    id p = dict ? MSG(id, dict, "objectForKey:", (id)kCFHTTPAuthenticationPassword) : 0;
    return CFHTTPMessageApplyCredentials(request, auth, u, p, err);
}
/* forma antiga: desafio tirado da resposta na hora; scheme NULL = o melhor */
unsigned char CFHTTPMessageAddAuthentication(id request, id failed, id user, id pass, id scheme, unsigned char for_proxy) {
    hm_t *f = HM(failed);
    if (!f) return 0;
    if (for_proxy != (f->m.status == 407)) return 0;
    id a = auth_from(f, cs(scheme));
    int ok = apply(HM(request), HA(a), cs(user), cs(pass));
    objc_release(a);
    return (unsigned char)ok;
}

/* ---------------------------------------------------------------- stream do pedido HTTP */
typedef struct {
    unsigned lock;
    int refs;
    mnet_msg_t req;
    id body_stream;                      /* ForStreamedHTTPRequest */
    mnet_stream_t *w;                    /* escritor do par (do fio) */
    id resp;                             /* CFHTTPMessage da resposta */
    char *final_url;
    int autoredirect, persistent, started;
    id proxy, ssl;
    long long sent;
    volatile int cancel;
} job_t;
static void job_unref(job_t *j) {
    os_unfair_lock_lock(&j->lock);
    int left = --j->refs;
    os_unfair_lock_unlock(&j->lock);
    if (left) return;
    mnet_msg_free(&j->req);
    if (j->body_stream) objc_release(j->body_stream);
    if (j->resp) objc_release(j->resp);
    if (j->proxy) objc_release(j->proxy);
    if (j->ssl) objc_release(j->ssl);
    free(j->final_url);
    mnet_stream_free(j->w);
    free(j);
}
static int j_resp(void *ctx, const mnet_resp_t *r) {
    job_t *j = ctx;
    id o = hm_new();
    if (!o) return 1;
    hm_t *h = HM(o);
    mnet_msg_init_response(&h->m, r->status, 0, r->version == 20 ? "HTTP/2.0" : r->version == 10 ? "HTTP/1.0" : "HTTP/1.1");
    for (int i = 0; i < r->hdrs.n; i++)
        mnet_hdrs_add(&h->m.hdrs, r->hdrs.v[i].name, strlen(r->hdrs.v[i].name), r->hdrs.v[i].value, strlen(r->hdrs.v[i].value));
    h->final_url = dupc(r->url ? r->url : j->req.target);
    os_unfair_lock_lock(&j->lock);
    id old = j->resp;
    j->resp = o;
    free(j->final_url);
    j->final_url = dupc(h->final_url);
    os_unfair_lock_unlock(&j->lock);
    if (old) objc_release(old);
    return mnet_stream_peer_gone(j->w);
}
static int j_body(void *ctx, const uint8_t *d, size_t n) {
    job_t *j = ctx;
    size_t off = 0;
    while (off < n) {
        long k = mnet_stream_write(j->w, d + off, n - off);
        if (k <= 0) return 1;                                     /* o app fechou o stream */
        off += (size_t)k;
    }
    return 0;
}
static int j_redirect(void *ctx, const mnet_resp_t *r, char **url) { (void)r; (void)url; return ((job_t *)ctx)->autoredirect; }
static void j_sent(void *ctx, long long sent, long long total) { (void)total; ((job_t *)ctx)->sent = sent; }
static long j_body_read(void *ctx, uint8_t *b, size_t n) {
    job_t *j = ctx;
    long k = CFReadStreamRead(j->body_stream, b, (long)n);
    return k < 0 ? -1 : k;
}
static int j_rewind(void *ctx) { (void)ctx; return -1; }       /* corpo em fluxo não volta */
static void err_map(int e, int *dom, long *code) {
    switch (e) {
    case MNET_E_NO_HOST: *dom = MNET_SD_NETDB; *code = 8; return;
    case MNET_E_CONNECT: *dom = MNET_SD_POSIX; *code = 61; return;
    case MNET_E_TIMEOUT: *dom = MNET_SD_POSIX; *code = 60; return;
    case MNET_E_LOST: *dom = MNET_SD_POSIX; *code = 54; return;
    case MNET_E_TLS: *dom = MNET_SD_SSL; *code = -9806; return;
    case MNET_E_CERT_UNTRUSTED: *dom = MNET_SD_SSL; *code = -9807; return;
    case MNET_E_REDIRECTS: *dom = MNET_SD_HTTP; *code = -2; return;     /* kCFStreamErrorHTTPRedirectionLoop */
    case MNET_E_BAD_URL: case MNET_E_UNSUPPORTED: *dom = MNET_SD_HTTP; *code = -3; return;   /* kCFStreamErrorHTTPBadURL */
    case MNET_E_BAD_RESPONSE: case MNET_E_PARSE: case MNET_E_DECODE: *dom = MNET_SD_HTTP; *code = -1; return;   /* ParseFailure */
    }
    *dom = MNET_SD_POSIX; *code = 5;                                    /* EIO */
}
/* dicionário de proxy do stream: as chaves de host bastam (no do sistema há também as *Enable) */
static char *proxy_for(job_t *j, const char *url) {
    id d = MSG0(id, MSG0(id, cls("NSMutableDictionary"), "alloc"), "init");
    if (j->proxy) MSG(void, d, "addEntriesFromDictionary:", j->proxy);
    static const char *const pairs[][2] = {{"HTTPProxy", "HTTPEnable"}, {"HTTPSProxy", "HTTPSEnable"}, {"SOCKSProxy", "SOCKSEnable"}};
    for (int i = 0; i < 3; i++)
        if (MSG(id, d, "objectForKey:", ns(pairs[i][0])) && !MSG(id, d, "objectForKey:", ns(pairs[i][1])))
        {
            id one = MSG(id, cls("NSNumber"), "numberWithInt:", 1);
            MSG(void, d, "setObject:forKey:", one, ns(pairs[i][1]));
        }
    char *p = cx_proxy_for(d, url);                                   /* dicionário vazio = direto */
    objc_release(d);
    return p;
}
static void *job_thread(void *arg) {
    job_t *j = arg;
    void *pool = objc_autoreleasePoolPush();
    mnet_req_t q;
    mnet_resp_t r;
    char msg[256] = "";
    memset(&q, 0, sizeof q);
    memset(&r, 0, sizeof r);
    q.method = j->req.method ? j->req.method : "GET";
    q.url = j->req.target ? j->req.target : "";
    q.hdrs = &j->req.hdrs;
    if (j->body_stream) {
        CFReadStreamOpen(j->body_stream);
        q.body_read = j_body_read; q.body_rewind = j_rewind;
        const char *cl = mnet_hdrs_get(&j->req.hdrs, "content-length");
        q.body_stream_len = cl ? strtoll(cl, 0, 10) : -1;
    } else if (j->req.body.n) { q.body = j->req.body.p; q.body_len = j->req.body.n; }
    q.max_redirects = j->autoredirect ? 0 : -1;
    q.no_cookies = 1;
    q.no_reuse = !j->persistent;
    q.ctx = j;
    q.on_response = j_resp; q.on_body = j_body; q.on_redirect = j_redirect; q.on_sent = j_sent;
    q.cancel = &j->cancel;
    char *px = proxy_for(j, q.url);
    q.proxy = px;
    int e = mnet_fetch(cx_mnet_client(), &q, &r, msg, sizeof msg);
    free(px);
    if (j->body_stream) CFReadStreamClose(j->body_stream);
    if (e && e != MNET_E_CANCELLED && !mnet_stream_peer_gone(j->w)) {
        int dom; long code;
        err_map(e, &dom, &code);
        mnet_stream_fail_peer(j->w, dom, code, msg);
    }
    mnet_resp_free(&r);
    mnet_stream_close(j->w);                                          /* fim do corpo: o leitor chega ao fim */
    objc_autoreleasePoolPop(pool);
    job_unref(j);
    return 0;
}
static void job_open(void *ctx, mnet_stream_t *s) {
    (void)s;
    job_t *j = ctx;
    unsigned long t;
    os_unfair_lock_lock(&j->lock);
    int go = !j->started;
    j->started = 1;
    if (go) j->refs++;
    os_unfair_lock_unlock(&j->lock);
    if (!go) return;
    mnet_stream_open(j->w);
    if (pthread_create(&t, 0, job_thread, j)) {
        mnet_stream_fail_peer(j->w, MNET_SD_POSIX, 35, "sem fio");
        mnet_stream_close(j->w);
        job_unref(j);
        return;
    }
    pthread_detach(t);
}
static id job_get(void *ext, id key) {
    job_t *j = ext;
    id v = 0;
    os_unfair_lock_lock(&j->lock);
    if (str_eq(key, kCFStreamPropertyHTTPResponseHeader)) v = j->resp ? objc_retain(j->resp) : 0;
    else if (str_eq(key, kCFStreamPropertyHTTPFinalURL) && j->final_url) {
        char *u = dupc(j->final_url);
        os_unfair_lock_unlock(&j->lock);
        v = MSG(id, cls("NSURL"), "URLWithString:", ns(u));
        free(u);
        return v;
    } else if (str_eq(key, kCFStreamPropertyHTTPFinalRequest)) {
        id o = hm_new();
        if (o && !mnet_msg_copy(&HM(o)->m, &j->req)) {
            if (j->final_url) { free(HM(o)->m.target); HM(o)->m.target = dupc(j->final_url); }
            v = o;
        } else if (o) objc_release(o);
    } else if (str_eq(key, kCFStreamPropertyHTTPRequestBytesWrittenCount)) {
        os_unfair_lock_unlock(&j->lock);
        return MSG(id, cls("NSNumber"), "numberWithLongLong:", j->sent);
    } else if (str_eq(key, kCFStreamPropertyHTTPShouldAutoredirect)) {
        os_unfair_lock_unlock(&j->lock);
        return MSG(id, cls("NSNumber"), "numberWithBool:", (BOOL)j->autoredirect);
    } else if (str_eq(key, kCFStreamPropertyHTTPAttemptPersistentConnection)) {
        os_unfair_lock_unlock(&j->lock);
        return MSG(id, cls("NSNumber"), "numberWithBool:", (BOOL)j->persistent);
    } else if (str_eq(key, kCFStreamPropertyHTTPProxy)) v = j->proxy ? objc_retain(j->proxy) : 0;
    else if (str_eq(key, kCFStreamPropertySSLSettings)) v = j->ssl ? objc_retain(j->ssl) : 0;
    os_unfair_lock_unlock(&j->lock);
    return v ? objc_autorelease(v) : 0;
}
static int job_set(void *ext, id key, id v) {
    job_t *j = ext;
    int r = -1;
    os_unfair_lock_lock(&j->lock);
    int before = !j->started;
    if (str_eq(key, kCFStreamPropertyHTTPShouldAutoredirect)) { if (before) j->autoredirect = boolv(v); r = before; }
    else if (str_eq(key, kCFStreamPropertyHTTPAttemptPersistentConnection)) { if (before) j->persistent = boolv(v); r = before; }
    else if (str_eq(key, kCFStreamPropertyHTTPProxy)) {
        if (before) { id old = j->proxy; j->proxy = v ? objc_retain(v) : 0; if (old) objc_release(old); }
        r = before;
    } else if (str_eq(key, kCFStreamPropertySSLSettings)) {
        /* a verificação da cadeia fica sempre ligada no motor TLS do mOS; o resto vale */
        if (before) { id old = j->ssl; j->ssl = v ? objc_retain(v) : 0; if (old) objc_release(old); }
        r = before;
    } else if (str_eq(key, kCFStreamPropertyHTTPResponseHeader) || str_eq(key, kCFStreamPropertyHTTPFinalURL) ||
               str_eq(key, kCFStreamPropertyHTTPFinalRequest) || str_eq(key, kCFStreamPropertyHTTPRequestBytesWrittenCount)) r = 0;
    os_unfair_lock_unlock(&j->lock);
    return r;
}
static void job_release(void *ext) {
    job_t *j = ext;
    j->cancel = 1;
    job_unref(j);
}
static const cx_stream_ext_t job_ops = {job_get, job_set, job_release};
static id http_stream(hm_t *req, id body_stream) {
    if (!req || !req->m.is_request) return 0;
    job_t *j = calloc(1, sizeof *j);
    mnet_stream_t *r = 0, *w = 0;
    if (!j || mnet_msg_copy(&j->req, &req->m) || mnet_stream_pair(256 * 1024, &r, &w)) { free(j); return 0; }
    j->refs = 1; j->w = w; j->persistent = 1;
    if (body_stream) j->body_stream = objc_retain(body_stream);
    mnet_stream_set_on_open(r, job_open, j);
    return cx_netstream_new_input(r, j, &job_ops);
}
id CFReadStreamCreateForHTTPRequest(void *alloc, id request) { (void)alloc; return http_stream(HM(request), 0); }
id CFReadStreamCreateForStreamedHTTPRequest(void *alloc, id headers, id body) {
    (void)alloc;
    return body ? http_stream(HM(headers), body) : 0;
}
extern unsigned char CFReadStreamSetProperty(id s, id key, id v);
void CFHTTPReadStreamSetRedirectsAutomatically(id s, unsigned char yes) {
    id v = MSG(id, cls("NSNumber"), "numberWithBool:", (BOOL)(yes != 0));
    CFReadStreamSetProperty(s, (id)kCFStreamPropertyHTTPShouldAutoredirect, v);
}
void CFHTTPReadStreamSetProxy(id s, id host, long port) {
    id d = MSG0(id, MSG0(id, cls("NSMutableDictionary"), "alloc"), "init");
    if (host) {
        id p = MSG(id, cls("NSNumber"), "numberWithLong:", port);
        MSG(void, d, "setObject:forKey:", host, (id)kCFStreamPropertyHTTPProxyHost);
        MSG(void, d, "setObject:forKey:", p, (id)kCFStreamPropertyHTTPProxyPort);
        MSG(void, d, "setObject:forKey:", host, (id)kCFStreamPropertyHTTPSProxyHost);
        MSG(void, d, "setObject:forKey:", p, (id)kCFStreamPropertyHTTPSProxyPort);
    }
    CFReadStreamSetProperty(s, (id)kCFStreamPropertyHTTPProxy, d);
    objc_release(d);
}

/* ---------------------------------------------------------------- registro */
void cx_cfhttp_init(void) {
    static int done;
    if (done) return;
    done = 1;
    cx_class_t *obj = cx_class_named("NSObject");
    cx_define_class(&HM_c, &HM_m, "__mOSCFHTTPMessage", obj, 8);
    cx_add_method(&HM_c, "dealloc", (void *)hm_dealloc);
    cx_add_method(&HM_c, "description", (void *)hm_description);
    cx_add_method(&HM_c, "_cfTypeID", (void *)hm_typeid);
    cx_define_class(&HA_c, &HA_m, "__mOSCFHTTPAuthentication", obj, 8);
    cx_add_method(&HA_c, "dealloc", (void *)ha_dealloc);
}
