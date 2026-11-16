/*
 * mOS — PAC (Proxy Auto-Config): avalia FindProxyForURL(url, host) com o JavaScriptCore do mOS
 * (JavaScriptCore.framework, carregado por dlopen; motor Brisa do Fabio). As funções que o PAC espera
 * (isPlainHostName, dnsDomainIs, shExpMatch, isInNet, weekdayRange, dateRange, timeRange...) são escritas aqui em JS;
 * dnsResolve, myIpAddress e o relógio são nativas. Também as extensões "Ex" (IPv6) da Microsoft.
 *
 * API: CFNetworkCopyProxiesForAutoConfigurationScript, CFNetworkExecuteProxyAutoConfigurationScript e
 * ...URL (com CFRunLoopSource, como no macOS), e cx_pac_result/cx_pac_result_url para cfproxy.c.
 * Com -DPAC_HOST o mesmo arquivo liga direto no motor para o teste de host (tools/pac_host.sh).
 */
#include <stdint.h>
#include <stddef.h>
#ifdef PAC_HOST
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <pthread.h>
#else
#include "foundation.h"
#include "libc.h"
#endif
#include "mnet.h"

typedef const struct OpaqueJSContext *JSContextRef;
typedef struct OpaqueJSContext *JSGlobalContextRef;
typedef struct OpaqueJSString *JSStringRef;
typedef const struct OpaqueJSValue *JSValueRef;
typedef struct OpaqueJSValue *JSObjectRef;
typedef JSValueRef (*JSCb)(JSContextRef, JSObjectRef, JSObjectRef, size_t, const JSValueRef[], JSValueRef *);

typedef struct {
    JSGlobalContextRef (*ctx_create)(void *);
    void (*ctx_release)(JSGlobalContextRef);
    JSObjectRef (*global)(JSContextRef);
    JSStringRef (*str_create)(const char *);
    void (*str_release)(JSStringRef);
    size_t (*str_max)(JSStringRef);
    size_t (*str_utf8)(JSStringRef, char *, size_t);
    JSValueRef (*eval)(JSContextRef, JSStringRef, JSObjectRef, JSStringRef, int, JSValueRef *);
    JSObjectRef (*make_fn)(JSContextRef, JSStringRef, JSCb);
    void (*set_prop)(JSContextRef, JSObjectRef, JSStringRef, JSValueRef, unsigned, JSValueRef *);
    JSStringRef (*to_str)(JSContextRef, JSValueRef, JSValueRef *);
    JSValueRef (*mk_str)(JSContextRef, JSStringRef);
    JSValueRef (*mk_bool)(JSContextRef, _Bool);
    JSValueRef (*mk_undef)(JSContextRef);
    JSValueRef (*mk_null)(JSContextRef);
    _Bool (*is_undef)(JSContextRef, JSValueRef);
} jsapi_t;
static jsapi_t J;
static int j_state;                                     /* 0 não tentado, 1 pronto, -1 sem motor */

#ifdef PAC_HOST
JSGlobalContextRef JSGlobalContextCreate(void *); void JSGlobalContextRelease(JSGlobalContextRef); JSObjectRef JSContextGetGlobalObject(JSContextRef);
JSStringRef JSStringCreateWithUTF8CString(const char *); void JSStringRelease(JSStringRef); size_t JSStringGetMaximumUTF8CStringSize(JSStringRef);
size_t JSStringGetUTF8CString(JSStringRef, char *, size_t); JSValueRef JSEvaluateScript(JSContextRef, JSStringRef, JSObjectRef, JSStringRef, int, JSValueRef *);
JSObjectRef JSObjectMakeFunctionWithCallback(JSContextRef, JSStringRef, JSCb); void JSObjectSetProperty(JSContextRef, JSObjectRef, JSStringRef, JSValueRef, unsigned, JSValueRef *);
JSStringRef JSValueToStringCopy(JSContextRef, JSValueRef, JSValueRef *); JSValueRef JSValueMakeString(JSContextRef, JSStringRef);
JSValueRef JSValueMakeBoolean(JSContextRef, _Bool); JSValueRef JSValueMakeUndefined(JSContextRef); JSValueRef JSValueMakeNull(JSContextRef);
_Bool JSValueIsUndefined(JSContextRef, JSValueRef);
static pthread_mutex_t g_mu = PTHREAD_MUTEX_INITIALIZER;
static void LK(void) { pthread_mutex_lock(&g_mu); }
static void ULK(void) { pthread_mutex_unlock(&g_mu); }
static int load_js(void) {
    J.ctx_create = JSGlobalContextCreate; J.ctx_release = JSGlobalContextRelease; J.global = JSContextGetGlobalObject;
    J.str_create = JSStringCreateWithUTF8CString; J.str_release = JSStringRelease; J.str_max = JSStringGetMaximumUTF8CStringSize;
    J.str_utf8 = JSStringGetUTF8CString; J.eval = JSEvaluateScript; J.make_fn = JSObjectMakeFunctionWithCallback;
    J.set_prop = JSObjectSetProperty; J.to_str = JSValueToStringCopy; J.mk_str = JSValueMakeString; J.mk_bool = JSValueMakeBoolean;
    J.mk_undef = JSValueMakeUndefined; J.mk_null = JSValueMakeNull; J.is_undef = JSValueIsUndefined;
    return 1;
}
static char *host_my_ip(void) { return strdup("192.168.15.40"); }
char *(*pac_my_ip_hook)(void) = host_my_ip;            /* o teste troca */
static char *resolve4(const char *h) {
    struct addrinfo hint = {0}, *r = 0; hint.ai_family = AF_INET;
    if (getaddrinfo(h, 0, &hint, &r) || !r) return 0;
    char b[64]; inet_ntop(AF_INET, &((struct sockaddr_in *)r->ai_addr)->sin_addr, b, sizeof b);
    freeaddrinfo(r);
    return strdup(b);
}
static void now_parts(int gmt, int p[7]) { time_t t = time(0); struct tm tm; if (gmt) gmtime_r(&t, &tm); else localtime_r(&t, &tm);
    p[0] = tm.tm_year + 1900; p[1] = tm.tm_mon; p[2] = tm.tm_mday; p[3] = tm.tm_wday; p[4] = tm.tm_hour; p[5] = tm.tm_min; p[6] = tm.tm_sec; }
#else
void *dlopen(const char *p, int m);
void *dlsym(void *h, const char *n);
void os_unfair_lock_lock(void *l);
void os_unfair_lock_unlock(void *l);
static unsigned g_lk;
static void LK(void) { os_unfair_lock_lock(&g_lk); }
static void ULK(void) { os_unfair_lock_unlock(&g_lk); }
static int load_js(void) {
    void *h = dlopen("/System/Library/Frameworks/JavaScriptCore.framework/Versions/A/JavaScriptCore", 1);
    if (!h) h = dlopen("/System/Library/Frameworks/JavaScriptCore.framework/JavaScriptCore", 1);
    if (!h) return 0;
#define S(f, n) if (!(*(void **)&J.f = dlsym(h, n))) return 0;
    S(ctx_create, "JSGlobalContextCreate") S(ctx_release, "JSGlobalContextRelease") S(global, "JSContextGetGlobalObject")
    S(str_create, "JSStringCreateWithUTF8CString") S(str_release, "JSStringRelease") S(str_max, "JSStringGetMaximumUTF8CStringSize")
    S(str_utf8, "JSStringGetUTF8CString") S(eval, "JSEvaluateScript") S(make_fn, "JSObjectMakeFunctionWithCallback")
    S(set_prop, "JSObjectSetProperty") S(to_str, "JSValueToStringCopy") S(mk_str, "JSValueMakeString") S(mk_bool, "JSValueMakeBoolean")
    S(mk_undef, "JSValueMakeUndefined") S(mk_null, "JSValueMakeNull") S(is_undef, "JSValueIsUndefined")
#undef S
    return 1;
}
typedef struct {
    char bsd[16]; char driver[48]; unsigned index, flags; int loopback, wifi, link, active;
    unsigned char mac[6]; unsigned ip, mask; unsigned char ll6[16], global6[16]; int has_ll6, has_g6, prefix6;
    unsigned long long rx_bytes, tx_bytes, rx_packets, tx_packets;
} cx_netif_t;
int cx_netif_list(cx_netif_t *out, int max);
const char *inet_ntop(int af, const void *src, char *dst, unsigned n);
struct mos_ai { int flags, family, socktype, protocol; unsigned addrlen; char *canon; void *addr; struct mos_ai *next; };
int getaddrinfo(const char *, const char *, const void *, void **);
void freeaddrinfo(void *);
static char *mos_my_ip(void) {
    cx_netif_t f[9];
    int n = cx_netif_list(f, 9);
    for (int i = 0; i < n; i++)
        if (!f[i].loopback && f[i].active && f[i].ip) {
            char b[20]; unsigned ip = f[i].ip;
            snprintf(b, sizeof b, "%u.%u.%u.%u", ip >> 24, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255);
            return mnet_strndup(b, strlen(b));
        }
    return mnet_strndup("127.0.0.1", 9);
}
char *(*pac_my_ip_hook)(void) = mos_my_ip;
static char *resolve4(const char *h) {
    struct mos_ai hint, *r = 0;
    memset(&hint, 0, sizeof hint); hint.family = 2;
    if (getaddrinfo(h, 0, &hint, (void **)&r) || !r) return 0;
    char b[64];
    inet_ntop(2, (const unsigned char *)r->addr + 4, b, sizeof b);
    freeaddrinfo(r);
    return mnet_strndup(b, strlen(b));
}
typedef struct { int sec, min, hour, mday, mon, year, wday, yday, isdst; long gmtoff; const char *zone; } mtm_t;
void *gmtime_r(const long *t, mtm_t *tm);
void *localtime_r(const long *t, mtm_t *tm);
static void now_parts(int gmt, int p[7]) {
    long t = time(0); mtm_t tm;
    if (gmt) gmtime_r(&t, &tm); else localtime_r(&t, &tm);
    p[0] = tm.year + 1900; p[1] = tm.mon; p[2] = tm.mday; p[3] = tm.wday; p[4] = tm.hour; p[5] = tm.min; p[6] = tm.sec;
}
#endif

static char *js2c(JSContextRef c, JSValueRef v) {
    JSStringRef s = J.to_str(c, v, 0);
    if (!s) return 0;
    size_t n = J.str_max(s);
    char *b = malloc(n + 1);
    if (b) J.str_utf8(s, b, n + 1);
    J.str_release(s);
    return b;
}
static JSValueRef mkstr(JSContextRef c, const char *s) {
    JSStringRef js = J.str_create(s);
    JSValueRef v = J.mk_str(c, js);
    J.str_release(js);
    return v;
}
/* ---- nativas */
static JSValueRef n_dns(JSContextRef c, JSObjectRef f, JSObjectRef t, size_t argc, const JSValueRef a[], JSValueRef *ex) {
    (void)f; (void)t; (void)ex;
    if (argc < 1) return J.mk_null(c);
    char *h = js2c(c, a[0]);
    char *ip = h ? resolve4(h) : 0;
    free(h);
    if (!ip) return J.mk_null(c);
    JSValueRef v = mkstr(c, ip);
    free(ip);
    return v;
}
static JSValueRef n_myip(JSContextRef c, JSObjectRef f, JSObjectRef t, size_t argc, const JSValueRef a[], JSValueRef *ex) {
    (void)f; (void)t; (void)argc; (void)a; (void)ex;
    char *ip = pac_my_ip_hook();
    JSValueRef v = mkstr(c, ip ? ip : "127.0.0.1");
    free(ip);
    return v;
}
static JSValueRef n_time(JSContextRef c, JSObjectRef f, JSObjectRef t, size_t argc, const JSValueRef a[], JSValueRef *ex) {
    (void)f; (void)t; (void)ex;
    int gmt = 0;
    if (argc > 0) { char *g = js2c(c, a[0]); gmt = g && !strcmp(g, "GMT"); free(g); }
    int p[7]; now_parts(gmt, p);
    char b[64];
    snprintf(b, sizeof b, "%d %d %d %d %d %d %d", p[0], p[1], p[2], p[3], p[4], p[5], p[6]);
    return mkstr(c, b);
}

/* ---- funções do PAC em JS (implementação do mOS, sem expressões regulares) */
static const char PRELUDE[] =
    "function isPlainHostName(h){return (''+h).indexOf('.')<0;}\n"
    "function dnsDomainIs(h,d){h=(''+h).toLowerCase();d=(''+d).toLowerCase();return h.length>=d.length&&h.substring(h.length-d.length)==d;}\n"
    "function localHostOrDomainIs(h,f){h=(''+h).toLowerCase();f=(''+f).toLowerCase();if(h==f)return true;return h.indexOf('.')<0&&f.indexOf(h+'.')==0;}\n"
    "function __mosDigits(s){if(s.length<1||s.length>3)return false;for(var i=0;i<s.length;i++){var c=s.charCodeAt(i);if(c<48||c>57)return false;}return true;}\n"
    "function __mosIp(s){s=''+s;var p=s.split('.');if(p.length!=4)return null;var o=[];for(var i=0;i<4;i++){if(!__mosDigits(p[i]))return null;var n=parseInt(p[i],10);if(n>255)return null;o.push(n);}return o;}\n"
    "function isValidIpAddress(s){return __mosIp(s)!=null;}\n"
    "function isResolvable(h){return dnsResolve(h)!=null;}\n"
    "function isResolvableEx(h){return dnsResolve(h)!=null;}\n"
    "function dnsResolveEx(h){var r=dnsResolve(h);return r==null?'':r;}\n"
    "function myIpAddressEx(){return myIpAddress();}\n"
    "function dnsDomainLevels(h){var n=0;h=''+h;for(var i=0;i<h.length;i++)if(h.charAt(i)=='.')n++;return n;}\n"
    "function convert_addr(s){var a=__mosIp(s);if(a==null)return 0;return ((a[0]*256+a[1])*256+a[2])*256+a[3];}\n"
    "function isInNet(h,pat,mask){var ip=__mosIp(h);if(ip==null){var r=dnsResolve(h);if(r==null)return false;ip=__mosIp(r);}\n"
    "  var p=__mosIp(pat),m=__mosIp(mask);if(ip==null||p==null||m==null)return false;\n"
    "  for(var i=0;i<4;i++){if((ip[i]&m[i])!=(p[i]&m[i]))return false;}return true;}\n"
    "function isInNetEx(h,prefix){var sl=(''+prefix).indexOf('/');if(sl<0)return false;var net=prefix.substring(0,sl),bits=parseInt(prefix.substring(sl+1),10);\n"
    "  var ip=__mosIp(h),p=__mosIp(net);if(ip==null||p==null)return false;for(var i=0;i<4;i++){var b=bits-8*i;var mk=b>=8?255:b<=0?0:(256-Math.pow(2,8-b));\n"
    "  if((ip[i]&mk)!=(p[i]&mk))return false;}return true;}\n"
    "function shExpMatch(s,e){s=''+s;e=''+e;var memo={};function m(i,j){var k=i+','+j;if(memo[k]!==undefined)return memo[k];var r;\n"
    "  if(j==e.length)r=(i==s.length);else{var c=e.charAt(j);if(c=='*')r=m(i,j+1)||(i<s.length&&m(i+1,j));\n"
    "  else r=i<s.length&&(c=='?'||c==s.charAt(i))&&m(i+1,j+1);}memo[k]=r;return r;}return m(0,0);}\n"
    "function sortIpAddressList(l){var a=(''+l).split(';');a.sort();return a.join(';');}\n"
    "function getClientVersion(){return '1.0';}\n"
    "function __mosNow(gmt){var p=__mosTime(gmt?'GMT':'').split(' ');var o=[];for(var i=0;i<7;i++)o.push(parseInt(p[i],10));return o;}\n"
    "var __mosDays=['SUN','MON','TUE','WED','THU','FRI','SAT'];\n"
    "var __mosMon=['JAN','FEB','MAR','APR','MAY','JUN','JUL','AUG','SEP','OCT','NOV','DEC'];\n"
    "function __mosIdx(a,v){v=(''+v).toUpperCase();for(var i=0;i<a.length;i++)if(a[i]==v)return i;return -1;}\n"
    "function weekdayRange(){var a=arguments,n=a.length,gmt=n>0&&a[n-1]=='GMT';if(gmt)n--;var d=__mosNow(gmt)[3];\n"
    "  var w1=__mosIdx(__mosDays,a[0]);if(w1<0)return false;if(n<2)return d==w1;var w2=__mosIdx(__mosDays,a[1]);if(w2<0)return false;\n"
    "  return w1<=w2?(d>=w1&&d<=w2):(d>=w1||d<=w2);}\n"
    "function dateRange(){var a=[],n=arguments.length,i,gmt=false;for(i=0;i<n;i++)a.push(arguments[i]);if(n>0&&a[n-1]=='GMT'){gmt=true;a.pop();}\n"
    "  var t=__mosNow(gmt),y=t[0],mo=t[1],d=t[2];\n"
    "  function kind(v){if(typeof v=='number')return v>31?'y':'d';if(__mosIdx(__mosMon,v)>=0)return 'm';var x=parseInt(v,10);return x>31?'y':'d';}\n"
    "  function val(v){var k=kind(v);return k=='m'?__mosIdx(__mosMon,v):parseInt(v,10);}\n"
    "  if(a.length==1){var k=kind(a[0]);return k=='y'?y==val(a[0]):k=='m'?mo==val(a[0]):d==val(a[0]);}\n"
    "  var h=a.length/2,s=a.slice(0,h),e=a.slice(h),cur=[],lo=[],hi=[];\n"
    "  for(i=0;i<h;i++){var k2=kind(s[i]);cur.push(k2=='y'?y:k2=='m'?mo:d);lo.push(val(s[i]));hi.push(val(e[i]));}\n"
    "  function cmp(p,q){for(var j=0;j<p.length;j++){if(p[j]!=q[j])return p[j]<q[j]?-1:1;}return 0;}\n"
    "  if(cmp(lo,hi)<=0)return cmp(cur,lo)>=0&&cmp(cur,hi)<=0;return cmp(cur,lo)>=0||cmp(cur,hi)<=0;}\n"
    "function timeRange(){var a=[],n=arguments.length,i,gmt=false;for(i=0;i<n;i++)a.push(parseInt(arguments[i],10));if(n>0&&arguments[n-1]=='GMT'){gmt=true;a.pop();}\n"
    "  var t=__mosNow(gmt),cur=t[4]*3600+t[5]*60+t[6],lo,hi;\n"
    "  if(a.length==1)return t[4]==a[0];\n"
    "  if(a.length==2){lo=a[0]*3600;hi=a[1]*3600-1;}else if(a.length==4){lo=a[0]*3600+a[1]*60;hi=a[2]*3600+a[3]*60-1;}\n"
    "  else if(a.length==6){lo=a[0]*3600+a[1]*60+a[2];hi=a[3]*3600+a[4]*60+a[5];}else return false;\n"
    "  return lo<=hi?(cur>=lo&&cur<=hi):(cur>=lo||cur<=hi);}\n"
    "function alert(m){}\n";

/* resultado de FindProxyForURL(url, host); NULL + *err em falha */
static void q_append(mnet_buf_t *b, const char *s) {
    mnet_buf_putc(b, '"');
    for (; *s; s++) {
        unsigned char c = (unsigned char)*s;
        if (c == '"' || c == '\\') { mnet_buf_putc(b, '\\'); mnet_buf_putc(b, c); }
        else if (c < 0x20) { char e[8]; snprintf(e, sizeof e, "\\u%04x", c); mnet_buf_puts(b, e); }
        else mnet_buf_putc(b, c);
    }
    mnet_buf_putc(b, '"');
}
char *cx_pac_result(const char *script, const char *url, char **err) {
    if (err) *err = 0;
    LK();
    if (!j_state) j_state = load_js() ? 1 : -1;
    if (j_state < 0) { ULK(); if (err) *err = mnet_strndup("JavaScriptCore indisponível", 27); return 0; }
    mnet_url_t u;
    char host[256] = "";
    if (!mnet_url_parse(url, &u)) { memcpy(host, u.host, sizeof host); mnet_url_free(&u); }
    JSGlobalContextRef c = J.ctx_create(0);
    JSObjectRef g = J.global(c);
    struct { const char *n; JSCb f; } nat[] = {{"dnsResolve", n_dns}, {"myIpAddress", n_myip}, {"__mosTime", n_time}};
    for (unsigned i = 0; i < 3; i++) {
        JSStringRef nm = J.str_create(nat[i].n);
        J.set_prop(c, g, nm, (JSValueRef)J.make_fn(c, nm, nat[i].f), 0, 0);
        J.str_release(nm);
    }
    char *out = 0;
    JSValueRef ex = 0;
    JSStringRef s1 = J.str_create(PRELUDE);
    J.eval(c, s1, 0, 0, 1, &ex);
    J.str_release(s1);
    if (!ex) {
        JSStringRef s2 = J.str_create(script ? script : "");
        J.eval(c, s2, 0, 0, 1, &ex);
        J.str_release(s2);
    }
    if (!ex) {
        /* FindProxyForURLEx (IPv6) tem preferência quando o script o define */
        mnet_buf_t b = {0};
        mnet_buf_puts(&b, "(typeof FindProxyForURLEx=='function'?FindProxyForURLEx:FindProxyForURL)(");
        q_append(&b, url); mnet_buf_putc(&b, ','); q_append(&b, host); mnet_buf_putc(&b, ')');
        JSStringRef s3 = J.str_create((char *)b.p);
        JSValueRef v = J.eval(c, s3, 0, 0, 1, &ex);
        J.str_release(s3);
        mnet_buf_free(&b);
        if (!ex && v && !J.is_undef(c, v)) out = js2c(c, v);
        else if (!ex) out = mnet_strndup("DIRECT", 6);
    }
    if (ex && err) { char *m = js2c(c, ex); *err = m ? m : mnet_strndup("erro no script PAC", 18); }
    J.ctx_release(c);
    ULK();
    return out;
}

#ifndef PAC_HOST
/* ---- arquivo PAC por URL, com cache de 5 minutos */
static struct { char *url; char *text; long long at; } g_cache[4];
static int fetch_sink(void *ctx, const uint8_t *d, size_t n) { return mnet_buf_put(ctx, d, n); }
static char *pac_fetch(const char *pac_url, char **err) {
    mnet_client_t *cl = cx_mnet_client();
    long long now = cl->io->now_ms(cl->io->env);
    LK();
    for (int i = 0; i < 4; i++)
        if (g_cache[i].url && !strcmp(g_cache[i].url, pac_url) && now - g_cache[i].at < 300000) {
            char *t = mnet_strndup(g_cache[i].text, strlen(g_cache[i].text));
            ULK();
            return t;
        }
    ULK();
    if (!strncmp(pac_url, "file://", 7)) {
        mnet_url_t u;
        int fd = -1;
        const char *path = pac_url + 7;
        (void)u;
        fd = open(path, 0);
        if (fd < 0) { if (err) *err = mnet_strndup("arquivo PAC não encontrado", 27); return 0; }
        mnet_buf_t b = {0}; char t[4096]; long k;
        while ((k = read(fd, t, sizeof t)) > 0) mnet_buf_put(&b, t, (size_t)k);
        close(fd);
        mnet_buf_putc(&b, 0);
        return (char *)b.p;
    }
    mnet_buf_t body = {0};
    mnet_req_t q = {0};
    q.url = pac_url; q.timeout_ms = 15000; q.max_redirects = 5; q.no_cookies = 1; q.ctx = &body; q.on_body = fetch_sink;
    mnet_resp_t r;
    char msg[160];
    int e = mnet_fetch(cl, &q, &r, msg, sizeof msg);
    int status = r.status;
    mnet_resp_free(&r);
    if (e || status != 200) { mnet_buf_free(&body); if (err) *err = mnet_strndup("não foi possível baixar o arquivo PAC", 37); return 0; }
    mnet_buf_putc(&body, 0);
    LK();
    int slot = 0;
    for (int i = 0; i < 4; i++) if (!g_cache[i].url || g_cache[i].at < g_cache[slot].at) slot = i;
    free(g_cache[slot].url); free(g_cache[slot].text);
    g_cache[slot].url = mnet_strndup(pac_url, strlen(pac_url));
    g_cache[slot].text = mnet_strndup((char *)body.p, strlen((char *)body.p));
    g_cache[slot].at = now;
    ULK();
    return (char *)body.p;
}
char *cx_pac_result_url(const char *pac_url, const char *url, char **err) {
    char *script = pac_fetch(pac_url, err);
    if (!script) return 0;
    char *r = cx_pac_result(script, url, err);
    free(script);
    return r;
}

/* ---- API do CFNetwork */
id objc_retain(id o);
void objc_release(id o);
id cx_proxies_from_pac_result(const char *res);
id CFErrorCreate(void *alloc, id domain, long code, id info);
static id pac_error(const char *m) {
    id info = MSG0(id, MSG0(id, (id)(void *)cx_class_named("NSMutableDictionary"), "alloc"), "init");
    MSG(void, info, "setObject:forKey:", cx_str(m ? m : "erro no PAC"), cx_str("NSLocalizedDescription"));
    id e = CFErrorCreate(0, cx_str("kCFErrorDomainCFNetwork"), 2, info);    /* kCFProxyAutoConfigurationJavaScriptError... */
    objc_release(info);
    return e;
}
id CFNetworkCopyProxiesForAutoConfigurationScript(id script, id url, id *error) {
    if (error) *error = 0;
    const char *u = url ? cx_cstr(MSG0(id, url, "absoluteString")) : "";
    char *err = 0;
    char *r = cx_pac_result(script ? cx_cstr(script) : "", u ? u : "", &err);
    if (!r) { if (error) *error = pac_error(err); free(err); return 0; }
    id a = cx_proxies_from_pac_result(r);
    free(r); free(err);
    return a;
}
/* execução assíncrona: devolve um CFRunLoopSource; ao terminar, o callback roda no run loop onde a fonte foi posta */
void *CFRunLoopSourceCreate(void *alloc, long order, void *ctx);
void CFRunLoopSourceSignal(void *s);
void CFRunLoopWakeUp(void *rl);
void *CFRunLoopGetMain(void);
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
int pthread_detach(unsigned long t);
typedef void (*pac_cb_t)(void *client, id proxies, id error);
typedef struct { long version; void *info; void *(*retain)(void *); void (*release)(void *); void *desc; } client_ctx_t;
typedef struct {
    char *script, *pac_url, *url;
    pac_cb_t cb; client_ctx_t client;
    void *source, *rl;
    id proxies, error;
    volatile int done, delivered;
} pacjob_t;
static void pj_schedule(void *info, void *rl, void *mode) { (void)mode; ((pacjob_t *)info)->rl = rl; }
static void pj_perform(void *info) {
    pacjob_t *j = info;
    if (!j->done || j->delivered) return;
    j->delivered = 1;
    j->cb(j->client.info, j->proxies, j->error);
    if (j->client.release && j->client.info) j->client.release(j->client.info);
}
static void *pj_thread(void *arg) {
    pacjob_t *j = arg;
    char *err = 0;
    char *r = j->pac_url ? cx_pac_result_url(j->pac_url, j->url, &err) : cx_pac_result(j->script, j->url, &err);
    if (r) j->proxies = cx_proxies_from_pac_result(r); else j->error = pac_error(err);
    free(r); free(err);
    __sync_synchronize();
    j->done = 1;
    CFRunLoopSourceSignal(j->source);
    if (j->rl) CFRunLoopWakeUp(j->rl);
    CFRunLoopWakeUp(CFRunLoopGetMain());
    return 0;
}
static void *pj_start(const char *script, const char *pac_url, id url, pac_cb_t cb, client_ctx_t *ctx) {
    pacjob_t *j = calloc(1, sizeof *j);
    if (!j || !cb) { free(j); return 0; }
    j->script = script ? mnet_strndup(script, strlen(script)) : 0;
    j->pac_url = pac_url ? mnet_strndup(pac_url, strlen(pac_url)) : 0;
    const char *u = url ? cx_cstr(MSG0(id, url, "absoluteString")) : "";
    j->url = mnet_strndup(u ? u : "", strlen(u ? u : ""));
    j->cb = cb;
    if (ctx) { j->client = *ctx; if (ctx->retain && ctx->info) j->client.info = ctx->retain(ctx->info); }
    void *sctx[10] = {0};
    sctx[1] = j; sctx[7] = (void *)pj_schedule; sctx[9] = (void *)pj_perform;   /* CFRunLoopSourceContext versão 0 */
    j->source = CFRunLoopSourceCreate(0, 0, sctx);
    unsigned long t;
    if (!pthread_create(&t, 0, pj_thread, j)) pthread_detach(t);
    return j->source;
}
void *CFNetworkExecuteProxyAutoConfigurationScript(id script, id url, pac_cb_t cb, client_ctx_t *ctx) {
    return pj_start(script ? cx_cstr(script) : "", 0, url, cb, ctx);
}
void *CFNetworkExecuteProxyAutoConfigurationURL(id pac, id url, pac_cb_t cb, client_ctx_t *ctx) {
    const char *p = pac ? cx_cstr(MSG0(id, pac, "absoluteString")) : 0;
    if (!p) return 0;
    return pj_start(0, p, url, cb, ctx);
}
#endif
