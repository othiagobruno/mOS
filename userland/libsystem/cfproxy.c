/*
 * mOS — proxy do CFNetwork (CFProxySupport.h): as chaves kCFNetworkProxies* / kCFProxy* como CFString de verdade,
 * CFNetworkCopyProxiesForURL com as regras de mnet_proxy.c (exceções, PAC, HTTP/HTTPS, SOCKS, direto) e a leitura de
 * um dicionário de proxy (do sistema, CFNetworkCopySystemProxySettings em sysconfig.c, ou do app) para o mnet.
 * PAC: o resultado de um script já avaliado é lido aqui; avaliar JavaScript depende do JavaScriptCore (docs/rede-apps.md).
 */
#include "foundation.h"
#include "libc.h"
#include "mnet.h"

extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } cfconst_t;
#define K(var, text) \
    static cfconst_t var##_cs = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; \
    const void *var = &var##_cs;
/* CFProxySupport.h */
K(kCFProxyTypeKey, "kCFProxyTypeKey") K(kCFProxyHostNameKey, "kCFProxyHostNameKey")
K(kCFProxyPortNumberKey, "kCFProxyPortNumberKey") K(kCFProxyAutoConfigurationURLKey, "kCFProxyAutoConfigurationURLKey")
K(kCFProxyAutoConfigurationJavaScriptKey, "kCFProxyAutoConfigurationJavaScriptKey")
K(kCFProxyUsernameKey, "kCFProxyUsernameKey") K(kCFProxyPasswordKey, "kCFProxyPasswordKey")
K(kCFProxyAutoConfigurationHTTPResponseKey, "kCFProxyAutoConfigurationHTTPResponseKey")
K(kCFProxyTypeNone, "kCFProxyTypeNone") K(kCFProxyTypeHTTP, "kCFProxyTypeHTTP") K(kCFProxyTypeHTTPS, "kCFProxyTypeHTTPS")
K(kCFProxyTypeSOCKS, "kCFProxyTypeSOCKS") K(kCFProxyTypeFTP, "kCFProxyTypeFTP")
K(kCFProxyTypeAutoConfigurationURL, "kCFProxyTypeAutoConfigurationURL")
K(kCFProxyTypeAutoConfigurationJavaScript, "kCFProxyTypeAutoConfigurationJavaScript")
/* chaves do dicionário de configuração (os mesmos textos de kSCPropNetProxies*) */
K(kCFNetworkProxiesExceptionsList, "ExceptionsList") K(kCFNetworkProxiesExcludeSimpleHostnames, "ExcludeSimpleHostnames")
K(kCFNetworkProxiesFTPEnable, "FTPEnable") K(kCFNetworkProxiesFTPPassive, "FTPPassive")
K(kCFNetworkProxiesFTPPort, "FTPPort") K(kCFNetworkProxiesFTPProxy, "FTPProxy")
K(kCFNetworkProxiesGopherEnable, "GopherEnable") K(kCFNetworkProxiesGopherPort, "GopherPort") K(kCFNetworkProxiesGopherProxy, "GopherProxy")
K(kCFNetworkProxiesHTTPEnable, "HTTPEnable") K(kCFNetworkProxiesHTTPPort, "HTTPPort") K(kCFNetworkProxiesHTTPProxy, "HTTPProxy")
K(kCFNetworkProxiesHTTPSEnable, "HTTPSEnable") K(kCFNetworkProxiesHTTPSPort, "HTTPSPort") K(kCFNetworkProxiesHTTPSProxy, "HTTPSProxy")
K(kCFNetworkProxiesRTSPEnable, "RTSPEnable") K(kCFNetworkProxiesRTSPPort, "RTSPPort") K(kCFNetworkProxiesRTSPProxy, "RTSPProxy")
K(kCFNetworkProxiesSOCKSEnable, "SOCKSEnable") K(kCFNetworkProxiesSOCKSPort, "SOCKSPort") K(kCFNetworkProxiesSOCKSProxy, "SOCKSProxy")
K(kCFNetworkProxiesProxyAutoConfigEnable, "ProxyAutoConfigEnable")
K(kCFNetworkProxiesProxyAutoConfigURLString, "ProxyAutoConfigURLString")
K(kCFNetworkProxiesProxyAutoConfigJavaScript, "ProxyAutoConfigJavaScript")
K(kCFNetworkProxiesProxyAutoDiscoveryEnable, "ProxyAutoDiscoveryEnable")
/* CFStream: tipo de serviço */
K(kCFStreamNetworkServiceType, "kCFStreamNetworkServiceType") K(kCFStreamNetworkServiceTypeVoIP, "voip")
K(kCFStreamNetworkServiceTypeVideo, "video") K(kCFStreamNetworkServiceTypeBackground, "background")
K(kCFStreamNetworkServiceTypeVoice, "voice") K(kCFStreamNetworkServiceTypeResponsiveData, "responsiveData")
K(kCFStreamNetworkServiceTypeCallSignaling, "callSignaling") K(kCFStreamNetworkServiceTypeAVStreaming, "avStreaming")
K(kCFStreamNetworkServiceTypeResponsiveAV, "responsiveAV")

id objc_retain(id o);
void objc_release(id o);
id CFDictionaryCreateMutable(void *alloc, long cap, const void *k, const void *v);
void CFDictionarySetValue(id d, const void *k, const void *v);
id CFArrayCreateMutable(void *alloc, long cap, const void *cb);
void CFArrayAppendValue(id a, const void *v);
id CFNumberCreate(void *alloc, long type, const void *p);
id CFNetworkCopySystemProxySettings(void);

static id dget(id d, const void *k) { return d ? ((id (*)(id, SEL, id))objc_msgSend)(d, sel_registerName("objectForKey:"), (id)k) : 0; }
static long long num(id v) {
    if (!v) return 0;
    if (MSG(BOOL, v, "respondsToSelector:", sel_registerName("longLongValue"))) return MSG0(long long, v, "longLongValue");
    return 0;
}
static void cpy(char *d, size_t n, id s) {
    const char *c = s ? cx_cstr(s) : 0;
    size_t l = c ? strlen(c) : 0;
    if (l >= n) l = n - 1;
    if (l) memcpy(d, c, l);
    d[l] = 0;
}
/* dicionário (sistema ou app) -> configuração do mnet */
void cx_proxy_cfg_from_dict(id d, mnet_proxy_cfg_t *c) {
    memset(c, 0, sizeof *c);
    if (!d) return;
    c->http_on = num(dget(d, kCFNetworkProxiesHTTPEnable)) != 0;
    c->https_on = num(dget(d, kCFNetworkProxiesHTTPSEnable)) != 0;
    c->socks_on = num(dget(d, kCFNetworkProxiesSOCKSEnable)) != 0;
    c->pac_on = num(dget(d, kCFNetworkProxiesProxyAutoConfigEnable)) != 0;
    c->exclude_simple = num(dget(d, kCFNetworkProxiesExcludeSimpleHostnames)) != 0;
    cpy(c->http_host, sizeof c->http_host, dget(d, kCFNetworkProxiesHTTPProxy));
    cpy(c->https_host, sizeof c->https_host, dget(d, kCFNetworkProxiesHTTPSProxy));
    cpy(c->socks_host, sizeof c->socks_host, dget(d, kCFNetworkProxiesSOCKSProxy));
    c->http_port = (int)num(dget(d, kCFNetworkProxiesHTTPPort));
    c->https_port = (int)num(dget(d, kCFNetworkProxiesHTTPSPort));
    c->socks_port = (int)num(dget(d, kCFNetworkProxiesSOCKSPort));
    id pu = dget(d, kCFNetworkProxiesProxyAutoConfigURLString), pj = dget(d, kCFNetworkProxiesProxyAutoConfigJavaScript);
    if (pu) { const char *s = cx_cstr(pu); c->pac_url = mnet_strndup(s, strlen(s)); }
    if (pj) { const char *s = cx_cstr(pj); c->pac_js = mnet_strndup(s, strlen(s)); }
    id ex = dget(d, kCFNetworkProxiesExceptionsList);
    u64 n = ex && MSG(BOOL, ex, "respondsToSelector:", sel_registerName("count")) ? MSG0(u64, ex, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id e = MSG(id, ex, "objectAtIndex:", i);
        if (e) mnet_proxy_add_exception(c, cx_cstr(e));
    }
}
static id entry(const mnet_proxy_t *p, const mnet_proxy_cfg_t *c) {
    id d = CFDictionaryCreateMutable(0, 0, 0, 0);
    const void *type = p->type == MNET_PX_HTTP ? kCFProxyTypeHTTP : p->type == MNET_PX_HTTPS ? kCFProxyTypeHTTPS :
                       p->type == MNET_PX_SOCKS ? kCFProxyTypeSOCKS : p->type == MNET_PX_PAC_URL ? kCFProxyTypeAutoConfigurationURL :
                       p->type == MNET_PX_PAC_JS ? kCFProxyTypeAutoConfigurationJavaScript : kCFProxyTypeNone;
    CFDictionarySetValue(d, kCFProxyTypeKey, type);
    if (p->type == MNET_PX_HTTP || p->type == MNET_PX_HTTPS || p->type == MNET_PX_SOCKS) {
        id h = cx_str(p->host);
        CFDictionarySetValue(d, kCFProxyHostNameKey, h); objc_release(h);
        long long pv = p->port;
        id n = CFNumberCreate(0, 4, &pv);
        if (n) { CFDictionarySetValue(d, kCFProxyPortNumberKey, n); objc_release(n); }
    } else if (p->type == MNET_PX_PAC_URL) {
        id s = cx_str(p->host);
        id u = MSG(id, (id)(void *)cx_class_named("NSURL"), "URLWithString:", s);
        objc_release(s);
        if (u) CFDictionarySetValue(d, kCFProxyAutoConfigurationURLKey, u);
    } else if (p->type == MNET_PX_PAC_JS && c && c->pac_js) {
        id s = cx_str(c->pac_js);
        CFDictionarySetValue(d, kCFProxyAutoConfigurationJavaScriptKey, s); objc_release(s);
    }
    return d;
}
id CFNetworkCopyProxiesForURL(id url, id settings) {
    mnet_proxy_cfg_t c;
    cx_proxy_cfg_from_dict(settings, &c);
    mnet_proxy_t list[8];
    const char *u = url ? cx_cstr(MSG0(id, url, "absoluteString")) : "";
    int n = mnet_proxy_for_url(&c, u ? u : "", list, 8);
    id a = CFArrayCreateMutable(0, 0, 0);
    for (int i = 0; i < n; i++) { id d = entry(&list[i], &c); CFArrayAppendValue(a, d); objc_release(d); }
    mnet_proxy_cfg_free(&c);
    return a;
}
/* resultado de FindProxyForURL já calculado -> lista do CFNetwork (usado pela avaliação de PAC) */
id cx_proxies_from_pac_result(const char *res) {
    mnet_proxy_t list[8];
    int n = mnet_proxy_parse_pac_result(res, list, 8);
    id a = CFArrayCreateMutable(0, 0, 0);
    for (int i = 0; i < n; i++) { id d = entry(&list[i], 0); CFArrayAppendValue(a, d); objc_release(d); }
    return a;
}
/* proxy escolhido para uma URL, no formato do mnet ("http://h:p", "socks5://h:p"); NULL = direto.
   settings NULL = o do sistema. PAC ainda não avaliado: cai nas entradas manuais seguintes. */
char *cx_proxy_for(id settings, const char *url) {
    id sys = 0;
    if (!settings) settings = sys = CFNetworkCopySystemProxySettings();
    mnet_proxy_cfg_t c;
    cx_proxy_cfg_from_dict(settings, &c);
    if (sys) objc_release(sys);
    mnet_proxy_t list[8];
    int n = mnet_proxy_for_url(&c, url, list, 8);
    char *out = 0;
    for (int i = 0; i < n && !out; i++) {
        if (list[i].type == MNET_PX_NONE) break;
        if (list[i].type != MNET_PX_HTTP && list[i].type != MNET_PX_HTTPS && list[i].type != MNET_PX_SOCKS) continue;
        mnet_buf_t b = {0};
        mnet_buf_puts(&b, list[i].type == MNET_PX_SOCKS ? "socks5://" : "http://");
        int v6 = strchr(list[i].host, ':') != 0;
        if (v6) mnet_buf_putc(&b, '[');
        mnet_buf_puts(&b, list[i].host);
        if (v6) mnet_buf_putc(&b, ']');
        mnet_buf_putc(&b, ':'); mnet_buf_putu(&b, (unsigned)list[i].port);
        out = (char *)b.p;
    }
    mnet_proxy_cfg_free(&c);
    return out;
}
