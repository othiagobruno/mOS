/*
 * mOS — WebKit.framework (Mach-O, em /System/Library/Frameworks): WKWebView e companhia sobre o motor web do sistema,
 * o mesmo do Horizonte (kernel/web: HTML, CSS, layout e o JavaScript "Brisa"), pela syscall SYS_WEB
 * (kernel/web/webview.c). O kernel carrega a página, roda os scripts e pinta num buffer BGRA; aqui ficam a API da
 * Apple, os delegates (WKNavigationDelegate, WKUIDelegate, WKScriptMessageHandler), o KVO, a entrada de mouse e
 * teclado e o desenho na NSView.
 *
 * Como o WebKit da Apple:
 *  - evaluateJavaScript: devolve o valor convertido (NSString, NSNumber, NSNull, NSDictionary, NSArray, NSDate) ou
 *    um NSError do WKErrorDomain (exceção, tipo sem conversão), sempre de forma assíncrona;
 *  - window.webkit.messageHandlers.<nome>.postMessage(x) chega a userContentController:didReceiveScriptMessage:
 *    (e, com WKScriptMessageHandlerWithReply, a promessa do JavaScript resolve com a resposta do app);
 *  - toda navegação passa por webView:decidePolicyForNavigationAction:decisionHandler:;
 *  - WKURLSchemeHandler serve a navegação principal de esquemas próprios (app://...);
 *  - alert/confirm/prompt e window.close() chegam ao WKUIDelegate. O motor não para o script esperando a resposta:
 *    confirm devolve false e prompt devolve null para a página (o completionHandler do app é aceito e ignorado).
 * Limites: um mundo de conteúdo só (WKContentWorld é aceito e ignorado), sem quadros (iframes não carregam), sem
 * mídia, sem impressão/PDF, e os recursos de esquemas próprios só valem para o documento principal.
 * WebArchive/WebResource (WebKit1) guardam o plist da Apple (WebMainResource / WebSubresources /
 * WebSubframeArchives). `data` grava XML; `initWithData:` lê XML e bplist. Não há desafio de autenticação,
 * download (WKDownload) nem painel de arquivo: o motor não emite esses eventos, então o delegate não é chamado.
 *   tools/build-frameworks-objc.ps1 -Only WebKit
 */
#import <AppKit/AppKit.h>
#include "../../../kernel/include/syscall.h"

/* ---------------------------------------------------------------- o que o SDK mínimo do mOS não declara */
typedef struct _NSZone NSZone;
@interface NSURL : NSObject <NSCopying>
+ (instancetype)URLWithString:(NSString *)s;
+ (instancetype)URLWithString:(NSString *)s relativeToURL:(NSURL *)base;
+ (instancetype)fileURLWithPath:(NSString *)p;
- (NSString *)absoluteString;
- (NSString *)scheme;
- (NSString *)host;
- (NSNumber *)port;
- (NSString *)path;
- (BOOL)isFileURL;
@end
@interface NSURLRequest : NSObject <NSCopying>
+ (instancetype)requestWithURL:(NSURL *)u;
- (instancetype)initWithURL:(NSURL *)u;
- (NSURL *)URL;
- (NSString *)HTTPMethod;
- (NSData *)HTTPBody;
- (NSDictionary *)allHTTPHeaderFields;
- (NSString *)valueForHTTPHeaderField:(NSString *)f;
@end
@interface NSMutableURLRequest : NSURLRequest
- (void)setHTTPMethod:(NSString *)m;
- (void)setHTTPBody:(NSData *)b;
- (void)setValue:(NSString *)v forHTTPHeaderField:(NSString *)f;
@end
@interface NSURLResponse : NSObject
- (instancetype)initWithURL:(NSURL *)u MIMEType:(NSString *)m expectedContentLength:(NSInteger)n textEncodingName:(NSString *)e;
- (NSString *)MIMEType;
- (NSString *)textEncodingName;
- (NSURL *)URL;
@end
@interface NSHTTPURLResponse : NSURLResponse
- (instancetype)initWithURL:(NSURL *)u statusCode:(NSInteger)s HTTPVersion:(NSString *)v headerFields:(NSDictionary *)h;
- (NSInteger)statusCode;
@end
@interface NSJSONSerialization : NSObject
+ (id)JSONObjectWithData:(NSData *)d options:(NSUInteger)o error:(NSError **)e;
+ (NSData *)dataWithJSONObject:(id)o options:(NSUInteger)opt error:(NSError **)e;
+ (BOOL)isValidJSONObject:(id)o;
@end
@interface NSNull : NSObject
+ (NSNull *)null;
@end
@interface NSSet : NSObject
+ (instancetype)setWithArray:(NSArray *)a;
+ (instancetype)set;
- (BOOL)containsObject:(id)o;
- (NSArray *)allObjects;
@end
@interface NSHTTPCookie : NSObject
- (NSString *)name;
- (NSString *)value;
- (NSString *)domain;
@end
@interface NSHTTPCookieStorage : NSObject
+ (NSHTTPCookieStorage *)sharedHTTPCookieStorage;
- (NSArray *)cookies;
- (void)setCookie:(NSHTTPCookie *)c;
- (void)deleteCookie:(NSHTTPCookie *)c;
@end
@interface NSTrackingArea : NSObject
- (instancetype)initWithRect:(NSRect)r options:(NSUInteger)o owner:(id)owner userInfo:(NSDictionary *)i;
@end
@interface NSCursor : NSObject
+ (NSCursor *)arrowCursor;
+ (NSCursor *)IBeamCursor;
+ (NSCursor *)pointingHandCursor;
- (void)set;
@end
@interface NSView (MOSWebKit)
- (void)addTrackingArea:(NSTrackingArea *)a;
- (void)removeTrackingArea:(NSTrackingArea *)a;
- (NSRect)bounds;
@end
@interface NSObject (MOSWebKitKVO)
- (void)willChangeValueForKey:(NSString *)k;
- (void)didChangeValueForKey:(NSString *)k;
- (BOOL)respondsToSelector:(SEL)s;
@end
@interface NSString (MOSWebKit)
- (NSString *)lowercaseString;
- (BOOL)hasPrefix:(NSString *)p;
- (NSString *)stringByReplacingOccurrencesOfString:(NSString *)a withString:(NSString *)b;
@end
@interface NSEvent (MOSWebKit)
- (NSInteger)clickCount;
- (CGFloat)deltaY;
- (BOOL)hasPreciseScrollingDeltas;
@end
@interface NSImage (MOSWebKit)
- (instancetype)initWithCGImage:(void *)cg size:(NSSize)s;
@end
@interface NSMutableData : NSData
- (void)appendData:(NSData *)d;
@end
@interface NSException : NSObject
+ (void)raise:(NSString *)n format:(NSString *)f, ...;
@end
@interface NSPropertyListSerialization (MOSWebArchive)
+ (NSData *)dataWithPropertyList:(id)plist format:(NSUInteger)format options:(NSUInteger)opt error:(NSError **)error;
@end

void *_Block_copy(const void *b);
void _Block_release(const void *b);
/* libSystem (appkit.c): a conversão da view em drawRect: e a composição de uma imagem ARGB em pixels da janela */
void cx_gfx_view_xform(double *ox, double *oy, double *sy);
void cx_gfx_image_px(int x, int y, int w, int h, const unsigned int *px);
void *CGDataProviderCreateWithData(void *info, const void *data, size_t size, void (*release)(void *, const void *, size_t));
void CGDataProviderRelease(void *p);
void *CGImageCreate(size_t w, size_t h, size_t bpc, size_t bpp, size_t bpr, const void *space, uint32_t info, void *prov,
                    const CGFloat *decode, bool interp, int intent);
void *CGColorSpaceCreateDeviceRGB(void);
void CGColorSpaceRelease(void *s);

#if defined(__aarch64__)
/* slice arm64 (tradutor arm64, docs/tradutor-arm64.md): svc #0x4d4f, número em x8, argumentos x0..x3 */
static long wk_sys(long op, long id, long a, long b) {
    register long x8 __asm__("x8") = SYS_WEB;
    register long x0 __asm__("x0") = op;
    register long x1 __asm__("x1") = id;
    register long x2 __asm__("x2") = a;
    register long x3 __asm__("x3") = b;
    __asm__ volatile("svc #0x4d4f" : "+r"(x0) : "r"(x8), "r"(x1), "r"(x2), "r"(x3) : "memory");
    return x0;
}
#else
static long wk_sys(long op, long id, long a, long b) {
    long r;
    register long r10 __asm__("r10") = b;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"((long)SYS_WEB), "D"(op), "S"(id), "d"(a), "r"(r10) : "rcx", "r11", "memory");
    return r;
}
#endif

/* ---------------------------------------------------------------- constantes públicas */
NSString *const WKErrorDomain = @"WKErrorDomain";
NSString *const WKJavaScriptExceptionMessageErrorKey = @"WKJavaScriptExceptionMessage";
NSString *const WKJavaScriptExceptionLineNumberErrorKey = @"WKJavaScriptExceptionLineNumber";
NSString *const WKJavaScriptExceptionColumnNumberErrorKey = @"WKJavaScriptExceptionColumnNumber";
NSString *const WKJavaScriptExceptionSourceURLErrorKey = @"WKJavaScriptExceptionSourceURL";
NSString *const WKWebsiteDataTypeCookies = @"WKWebsiteDataTypeCookies";
NSString *const WKWebsiteDataTypeDiskCache = @"WKWebsiteDataTypeDiskCache";
NSString *const WKWebsiteDataTypeMemoryCache = @"WKWebsiteDataTypeMemoryCache";
NSString *const WKWebsiteDataTypeOfflineWebApplicationCache = @"WKWebsiteDataTypeOfflineWebApplicationCache";
NSString *const WKWebsiteDataTypeSessionStorage = @"WKWebsiteDataTypeSessionStorage";
NSString *const WKWebsiteDataTypeLocalStorage = @"WKWebsiteDataTypeLocalStorage";
NSString *const WKWebsiteDataTypeWebSQLDatabases = @"WKWebsiteDataTypeWebSQLDatabases";
NSString *const WKWebsiteDataTypeIndexedDBDatabases = @"WKWebsiteDataTypeIndexedDBDatabases";
NSString *const WKWebsiteDataTypeServiceWorkerRegistrations = @"WKWebsiteDataTypeServiceWorkerRegistrations";
NSString *const WKWebsiteDataTypeFetchCache = @"WKWebsiteDataTypeFetchCache";
NSString *const WKWebsiteDataTypeFileSystem = @"WKWebsiteDataTypeFileSystem";
NSString *const WKWebsiteDataTypeSearchFieldRecentSearches = @"WKWebsiteDataTypeSearchFieldRecentSearches";
NSString *const WKWebsiteDataTypeMediaKeys = @"WKWebsiteDataTypeMediaKeys";
NSString *const WKWebsiteDataTypeHashSalt = @"WKWebsiteDataTypeHashSalt";
NSString *const WKWebsiteDataTypeScreenTime = @"WKWebsiteDataTypeScreenTime";
NSString *const NSURLErrorDomainWK = @"NSURLErrorDomain";

typedef NS_ENUM(NSInteger, WKNavigationType) {
    WKNavigationTypeLinkActivated, WKNavigationTypeFormSubmitted, WKNavigationTypeBackForward, WKNavigationTypeReload,
    WKNavigationTypeFormResubmitted, WKNavigationTypeOther = -1
};
typedef NS_ENUM(NSInteger, WKNavigationActionPolicy) { WKNavigationActionPolicyCancel, WKNavigationActionPolicyAllow, WKNavigationActionPolicyDownload };
typedef NS_ENUM(NSInteger, WKNavigationResponsePolicy) { WKNavigationResponsePolicyCancel, WKNavigationResponsePolicyAllow, WKNavigationResponsePolicyDownload };
typedef NS_ENUM(NSInteger, WKUserScriptInjectionTime) { WKUserScriptInjectionTimeAtDocumentStart, WKUserScriptInjectionTimeAtDocumentEnd };
typedef NS_ENUM(NSInteger, WKErrorCode) {
    WKErrorUnknown = 1, WKErrorWebContentProcessTerminated, WKErrorWebViewInvalidated, WKErrorJavaScriptExceptionOccurred,
    WKErrorJavaScriptResultTypeIsUnsupported, WKErrorContentRuleListStoreCompileFailed, WKErrorContentRuleListStoreLookUpFailed,
    WKErrorContentRuleListStoreRemoveFailed, WKErrorContentRuleListStoreVersionMismatch, WKErrorAttributedStringContentFailedToLoad,
    WKErrorAttributedStringContentLoadTimedOut, WKErrorJavaScriptInvalidFrameTarget, WKErrorNavigationAppBoundDomain,
    WKErrorJavaScriptAppBoundDomain
};

static NSString *wk_str(const char *s, size_t n) {
    return [[[NSString alloc] initWithBytes:s length:n encoding:NSUTF8StringEncoding] autorelease];
}
static NSString *wk_cstr(const char *s) { return s ? [NSString stringWithUTF8String:s] : @""; }
static NSError *wk_error(NSString *domain, NSInteger code, NSString *msg, NSDictionary *extra) {
    NSMutableDictionary *ui = [NSMutableDictionary dictionary];
    if (msg) [ui setObject:msg forKey:@"NSLocalizedDescription"];
    if (extra) [ui addEntriesFromDictionary:extra];
    return [NSError errorWithDomain:domain code:code userInfo:ui];
}

/* valor do motor (tipo MOS_WEB_T_* + texto) -> objeto Foundation */
static id wk_value(int type, const char *s, size_t n, BOOL *unsupported) {
    if (unsupported) *unsupported = NO;
    switch (type) {
    case MOS_WEB_T_UNDEFINED: return nil;
    case MOS_WEB_T_NULL: return [NSNull null];
    case MOS_WEB_T_BOOL: return [NSNumber numberWithBool:n >= 4 && !memcmp(s, "true", 4)];
    case MOS_WEB_T_NUMBER: {
        char b[64];
        size_t k = n < 63 ? n : 63;
        memcpy(b, s, k);
        b[k] = 0;
        return [NSNumber numberWithDouble:strtod(b, NULL)];
    }
    case MOS_WEB_T_DATE: {
        char b[64];
        size_t k = n < 63 ? n : 63;
        memcpy(b, s, k);
        b[k] = 0;
        return [NSDate dateWithTimeIntervalSince1970:strtod(b, NULL) / 1000.0];
    }
    case MOS_WEB_T_STRING: return wk_str(s, n);
    case MOS_WEB_T_JSON: {
        NSData *d = [NSData dataWithBytes:s length:n];
        id o = [NSJSONSerialization JSONObjectWithData:d options:4 /* fragmentos */ error:NULL];
        if (!o && unsupported) *unsupported = YES;
        return o;
    }
    default:
        if (unsupported) *unsupported = YES;
        return nil;
    }
}

/* objeto Foundation -> literal JavaScript (argumentos de callAsyncJavaScript, respostas de messageHandlers) */
static NSString *wk_js_literal(id v) {
    if (!v || v == (id)[NSNull null]) return v ? @"null" : @"undefined";
    if ([v isKindOfClass:[NSString class]] || [v isKindOfClass:[NSArray class]] || [v isKindOfClass:[NSDictionary class]] ||
        [v isKindOfClass:[NSNumber class]]) {
        NSData *d = [NSJSONSerialization dataWithJSONObject:[NSArray arrayWithObject:v] options:0 error:NULL];
        if (d) {
            NSString *s = wk_str([d bytes], [d length]);
            if ([s length] >= 2) return [s substringWithRange:NSMakeRange(1, [s length] - 2)];
        }
        if ([v isKindOfClass:[NSNumber class]]) return [v stringValue];
    }
    if ([v isKindOfClass:[NSDate class]]) return [NSString stringWithFormat:@"new Date(%.0f)", [v timeIntervalSince1970] * 1000.0];
    return @"undefined";
}

@interface NSString (MOSWebKitMore)
- (instancetype)initWithBytes:(const void *)b length:(NSUInteger)n encoding:(NSUInteger)e;
- (NSData *)dataUsingEncoding:(NSUInteger)e;
- (NSString *)uppercaseString;
- (NSString *)substringWithRange:(NSRange)r;
@end
@interface NSMutableDictionary (MOSWebKit)
- (void)addEntriesFromDictionary:(NSDictionary *)d;
@end
@interface NSError (MOSWebKit)
+ (instancetype)errorWithDomain:(NSString *)d code:(NSInteger)c userInfo:(NSDictionary *)u;
@end
@interface NSView (MOSWebKitTracking)
- (void)updateTrackingAreas;
@end

/* ================================================================ objetos de configuração */
@class WKWebView, WKUserContentController, WKWebsiteDataStore, WKProcessPool, WKContentWorld, WKFrameInfo, WKWebViewConfiguration;

@interface WKWebView : NSView
@property (readonly, copy) WKWebViewConfiguration *configuration;
@property (assign) id navigationDelegate;
@property (assign) id UIDelegate;
@property BOOL allowsBackForwardNavigationGestures;
@property BOOL allowsMagnification;
@property BOOL allowsLinkPreview;
@property (copy) NSString *customUserAgent;
@property (getter=isInspectable) BOOL inspectable;
@property (retain) NSColor *underPageBackgroundColor;
@property (copy) NSString *mediaType;
@property (copy) NSString *interactionState;
- (instancetype)initWithFrame:(NSRect)frame configuration:(WKWebViewConfiguration *)configuration;
- (void)_mosDecide:(BOOL)allow seq:(int)seq url:(NSString *)url;
@end

@interface WKPreferences : NSObject <NSCopying>
@property BOOL javaScriptEnabled;
@property BOOL javaScriptCanOpenWindowsAutomatically;
@property CGFloat minimumFontSize;
@property BOOL tabFocusesLinks;
@property BOOL fraudulentWebsiteWarningEnabled;
@property BOOL textInteractionEnabled;
@property BOOL elementFullscreenEnabled;
@property BOOL siteSpecificQuirksModeEnabled;
@property BOOL shouldPrintBackgrounds;
@property BOOL javaEnabled;
@property BOOL plugInsEnabled;
@property NSInteger inactiveSchedulingPolicy;
@end
@implementation WKPreferences
- (instancetype)init {
    if ((self = [super init])) { _javaScriptEnabled = YES; _fraudulentWebsiteWarningEnabled = YES; _textInteractionEnabled = YES; }
    return self;
}
- (id)copyWithZone:(NSZone *)z {
    WKPreferences *p = [[WKPreferences alloc] init];
    p.javaScriptEnabled = _javaScriptEnabled;
    p.javaScriptCanOpenWindowsAutomatically = _javaScriptCanOpenWindowsAutomatically;
    p.minimumFontSize = _minimumFontSize;
    p.tabFocusesLinks = _tabFocusesLinks;
    return p;
}
- (BOOL)isFraudulentWebsiteWarningEnabled { return _fraudulentWebsiteWarningEnabled; }
- (BOOL)isTextInteractionEnabled { return _textInteractionEnabled; }
- (BOOL)isElementFullscreenEnabled { return _elementFullscreenEnabled; }
- (BOOL)isSiteSpecificQuirksModeEnabled { return _siteSpecificQuirksModeEnabled; }
/* chaves privadas que os apps ligam por KVC (setValue:@YES forKey:@"developerExtrasEnabled") */
- (void)setValue:(id)v forUndefinedKey:(NSString *)k {}
- (id)valueForUndefinedKey:(NSString *)k { return nil; }
@end

@interface WKWebpagePreferences : NSObject <NSCopying>
@property BOOL allowsContentJavaScript;
@property NSInteger preferredContentMode;
@property BOOL lockdownModeEnabled;
@end
@implementation WKWebpagePreferences
- (instancetype)init { if ((self = [super init])) _allowsContentJavaScript = YES; return self; }
- (id)copyWithZone:(NSZone *)z {
    WKWebpagePreferences *p = [[WKWebpagePreferences alloc] init];
    p.allowsContentJavaScript = _allowsContentJavaScript;
    p.preferredContentMode = _preferredContentMode;
    return p;
}
- (BOOL)isLockdownModeEnabled { return _lockdownModeEnabled; }
@end

@interface WKContentWorld : NSObject
+ (WKContentWorld *)pageWorld;
+ (WKContentWorld *)defaultClientWorld;
+ (WKContentWorld *)worldWithName:(NSString *)n;
@property (readonly, copy) NSString *name;
@end
@implementation WKContentWorld {
    NSString *_name;
}
static WKContentWorld *wk_world(NSString *n) {
    WKContentWorld *w = [[WKContentWorld alloc] init];
    w->_name = [n copy];
    return w;
}
+ (WKContentWorld *)pageWorld { static WKContentWorld *w; if (!w) w = wk_world(nil); return w; }
+ (WKContentWorld *)defaultClientWorld { static WKContentWorld *w; if (!w) w = wk_world(@"defaultClient"); return w; }
+ (WKContentWorld *)worldWithName:(NSString *)n { return [wk_world(n) autorelease]; }
- (NSString *)name { return _name; }
- (void)dealloc { [_name release]; [super dealloc]; }
@end

@interface WKUserScript : NSObject <NSCopying>
- (instancetype)initWithSource:(NSString *)source injectionTime:(WKUserScriptInjectionTime)t forMainFrameOnly:(BOOL)main;
- (instancetype)initWithSource:(NSString *)source injectionTime:(WKUserScriptInjectionTime)t forMainFrameOnly:(BOOL)main inContentWorld:(WKContentWorld *)w;
@property (readonly, copy) NSString *source;
@property (readonly) WKUserScriptInjectionTime injectionTime;
@property (readonly, getter=isForMainFrameOnly) BOOL forMainFrameOnly;
@end
@implementation WKUserScript
- (instancetype)initWithSource:(NSString *)source injectionTime:(WKUserScriptInjectionTime)t forMainFrameOnly:(BOOL)main {
    if ((self = [super init])) { _source = [source copy]; _injectionTime = t; _forMainFrameOnly = main; }
    return self;
}
- (instancetype)initWithSource:(NSString *)source injectionTime:(WKUserScriptInjectionTime)t forMainFrameOnly:(BOOL)main inContentWorld:(WKContentWorld *)w {
    return [self initWithSource:source injectionTime:t forMainFrameOnly:main];
}
- (id)copyWithZone:(NSZone *)z { return [self retain]; }
- (void)dealloc { [_source release]; [super dealloc]; }
@end

@interface WKContentRuleList : NSObject {
@public
    NSString *_ident;
}
@property (readonly, copy) NSString *identifier;
@end
@implementation WKContentRuleList
- (NSString *)identifier { return _ident; }
- (void)dealloc { [_ident release]; [super dealloc]; }
@end

@protocol WKScriptMessageHandler;
@interface WKScriptMessage : NSObject
@property (readonly, retain) id body;
@property (readonly, assign) WKWebView *webView;
@property (readonly, retain) WKFrameInfo *frameInfo;
@property (readonly, copy) NSString *name;
@property (readonly, retain) WKContentWorld *world;
@end

/* um manipulador por nome: o objeto do app e se ele responde com reply */
@interface WKHandlerEntry : NSObject {
@public
    NSString *name;
    id handler;
    BOOL reply;
}
@end
@implementation WKHandlerEntry
- (void)dealloc { [name release]; [handler release]; [super dealloc]; }
@end

@interface WKUserContentController : NSObject
- (void)addScriptMessageHandler:(id)h name:(NSString *)n;
- (void)addScriptMessageHandler:(id)h contentWorld:(WKContentWorld *)w name:(NSString *)n;
- (void)addScriptMessageHandlerWithReply:(id)h contentWorld:(WKContentWorld *)w name:(NSString *)n;
- (void)removeScriptMessageHandlerForName:(NSString *)n;
- (void)removeScriptMessageHandlerForName:(NSString *)n contentWorld:(WKContentWorld *)w;
- (void)removeAllScriptMessageHandlersFromContentWorld:(WKContentWorld *)w;
- (void)removeAllScriptMessageHandlers;
- (void)addUserScript:(WKUserScript *)s;
- (void)removeAllUserScripts;
- (NSArray *)userScripts;
- (void)addContentRuleList:(WKContentRuleList *)l;
- (void)removeContentRuleList:(WKContentRuleList *)l;
- (void)removeAllContentRuleLists;
@end

@interface WKWebView (MOSInternal)
- (void)_mosHandlersChanged;
- (void)_mosScriptsChanged;
@end

#define WK_MAXVIEWS 64
@implementation WKUserContentController {
@public
    NSMutableArray *_handlers;   /* WKHandlerEntry */
    NSMutableArray *_scripts;
    NSMutableArray *_rules;
    WKWebView *_views[WK_MAXVIEWS];   /* vistas que usam este controlador (sem retenção) */
    int _nviews;
}
- (instancetype)init {
    if ((self = [super init])) { _handlers = [[NSMutableArray alloc] init]; _scripts = [[NSMutableArray alloc] init]; _rules = [[NSMutableArray alloc] init]; }
    return self;
}
- (void)dealloc { [_handlers release]; [_scripts release]; [_rules release]; [super dealloc]; }
- (void)_attach:(WKWebView *)v { if (_nviews < WK_MAXVIEWS) _views[_nviews++] = v; }
- (void)_detach:(WKWebView *)v {
    for (int i = 0; i < _nviews; i++) if (_views[i] == v) { _views[i] = _views[--_nviews]; break; }
}
- (void)_notifyHandlers { for (int i = 0; i < _nviews; i++) [_views[i] _mosHandlersChanged]; }
- (void)_notifyScripts { for (int i = 0; i < _nviews; i++) [_views[i] _mosScriptsChanged]; }
- (WKHandlerEntry *)_entryNamed:(NSString *)n {
    for (WKHandlerEntry *e in _handlers) if ([e->name isEqualToString:n]) return e;
    return nil;
}
- (void)_add:(id)h name:(NSString *)n reply:(BOOL)reply {
    if (!h || !n) return;
    if ([self _entryNamed:n]) {
        [NSException raise:@"NSInvalidArgumentException" format:@"Attempt to add script message handler with name '%@' when one already exists.", n];
        return;
    }
    WKHandlerEntry *e = [[WKHandlerEntry alloc] init];
    e->name = [n copy];
    e->handler = [h retain];
    e->reply = reply;
    [_handlers addObject:e];
    [e release];
    [self _notifyHandlers];
}
- (void)addScriptMessageHandler:(id)h name:(NSString *)n { [self _add:h name:n reply:NO]; }
- (void)addScriptMessageHandler:(id)h contentWorld:(WKContentWorld *)w name:(NSString *)n { [self _add:h name:n reply:NO]; }
- (void)addScriptMessageHandlerWithReply:(id)h contentWorld:(WKContentWorld *)w name:(NSString *)n { [self _add:h name:n reply:YES]; }
- (void)removeScriptMessageHandlerForName:(NSString *)n {
    WKHandlerEntry *e = [self _entryNamed:n];
    if (!e) return;
    [_handlers removeObject:e];
    [self _notifyHandlers];
}
- (void)removeScriptMessageHandlerForName:(NSString *)n contentWorld:(WKContentWorld *)w { [self removeScriptMessageHandlerForName:n]; }
- (void)removeAllScriptMessageHandlersFromContentWorld:(WKContentWorld *)w { [self removeAllScriptMessageHandlers]; }
- (void)removeAllScriptMessageHandlers { [_handlers removeAllObjects]; [self _notifyHandlers]; }
- (void)addUserScript:(WKUserScript *)s { if (s) { [_scripts addObject:s]; [self _notifyScripts]; } }
- (void)removeAllUserScripts { [_scripts removeAllObjects]; [self _notifyScripts]; }
- (void)_removeAllUserScriptsAssociatedWithContentWorld:(WKContentWorld *)w { [self removeAllUserScripts]; }
- (NSArray *)userScripts { return [[_scripts copy] autorelease]; }
- (void)addContentRuleList:(WKContentRuleList *)l { if (l) [_rules addObject:l]; }
- (void)removeContentRuleList:(WKContentRuleList *)l { [_rules removeObject:l]; }
- (void)removeAllContentRuleLists { [_rules removeAllObjects]; }
@end

@interface WKProcessPool : NSObject
@end
@implementation WKProcessPool
@end

@interface WKHTTPCookieStore : NSObject
- (void)getAllCookies:(void (^)(NSArray *))h;
- (void)setCookie:(NSHTTPCookie *)c completionHandler:(void (^)(void))h;
- (void)deleteCookie:(NSHTTPCookie *)c completionHandler:(void (^)(void))h;
- (void)addObserver:(id)o;
- (void)removeObserver:(id)o;
- (void)setCookiePolicy:(NSInteger)p completionHandler:(void (^)(void))h;
- (void)getCookiePolicy:(void (^)(NSInteger))h;
@end
@implementation WKHTTPCookieStore
- (void)getAllCookies:(void (^)(NSArray *))h { if (h) h([[NSHTTPCookieStorage sharedHTTPCookieStorage] cookies] ?: [NSArray array]); }
- (void)setCookie:(NSHTTPCookie *)c completionHandler:(void (^)(void))h { if (c) [[NSHTTPCookieStorage sharedHTTPCookieStorage] setCookie:c]; if (h) h(); }
- (void)deleteCookie:(NSHTTPCookie *)c completionHandler:(void (^)(void))h { if (c) [[NSHTTPCookieStorage sharedHTTPCookieStorage] deleteCookie:c]; if (h) h(); }
- (void)addObserver:(id)o {}
- (void)removeObserver:(id)o {}
- (void)setCookiePolicy:(NSInteger)p completionHandler:(void (^)(void))h { if (h) h(); }
- (void)getCookiePolicy:(void (^)(NSInteger))h { if (h) h(0); }
@end

@interface WKWebsiteDataRecord : NSObject
@property (readonly, copy) NSString *displayName;
@property (readonly, copy) NSSet *dataTypes;
@end
@implementation WKWebsiteDataRecord
@end

@interface WKWebsiteDataStore : NSObject
+ (WKWebsiteDataStore *)defaultDataStore;
+ (WKWebsiteDataStore *)nonPersistentDataStore;
+ (NSSet *)allWebsiteDataTypes;
@property (readonly, getter=isPersistent) BOOL persistent;
@property (readonly) WKHTTPCookieStore *httpCookieStore;
@end
@implementation WKWebsiteDataStore {
    WKHTTPCookieStore *_cookies;
    BOOL _persistent;
}
static WKWebsiteDataStore *wk_store(BOOL persistent) {
    WKWebsiteDataStore *s = [[WKWebsiteDataStore alloc] init];
    s->_cookies = [[WKHTTPCookieStore alloc] init];
    s->_persistent = persistent;
    return s;
}
+ (WKWebsiteDataStore *)defaultDataStore { static WKWebsiteDataStore *s; if (!s) s = wk_store(YES); return s; }
+ (WKWebsiteDataStore *)nonPersistentDataStore { return [wk_store(NO) autorelease]; }
+ (WKWebsiteDataStore *)dataStoreForIdentifier:(id)uuid { return [wk_store(YES) autorelease]; }
+ (void)fetchAllDataStoreIdentifiers:(void (^)(NSArray *))h { if (h) h([NSArray array]); }
+ (void)removeDataStoreForIdentifier:(id)uuid completionHandler:(void (^)(NSError *))h { if (h) h(nil); }
+ (NSSet *)allWebsiteDataTypes {
    return [NSSet setWithArray:[NSArray arrayWithObjects:WKWebsiteDataTypeCookies, WKWebsiteDataTypeDiskCache, WKWebsiteDataTypeMemoryCache,
        WKWebsiteDataTypeOfflineWebApplicationCache, WKWebsiteDataTypeSessionStorage, WKWebsiteDataTypeLocalStorage,
        WKWebsiteDataTypeWebSQLDatabases, WKWebsiteDataTypeIndexedDBDatabases, WKWebsiteDataTypeServiceWorkerRegistrations,
        WKWebsiteDataTypeFetchCache, WKWebsiteDataTypeFileSystem, WKWebsiteDataTypeSearchFieldRecentSearches,
        WKWebsiteDataTypeMediaKeys, WKWebsiteDataTypeHashSalt, nil]];
}
- (BOOL)isPersistent { return _persistent; }
- (WKHTTPCookieStore *)httpCookieStore { return _cookies; }
- (void)fetchDataRecordsOfTypes:(NSSet *)t completionHandler:(void (^)(NSArray *))h { if (h) h([NSArray array]); }
- (void)removeDataOfTypes:(NSSet *)t forDataRecords:(NSArray *)r completionHandler:(void (^)(void))h { if (h) h(); }
- (void)removeDataOfTypes:(NSSet *)t modifiedSince:(NSDate *)d completionHandler:(void (^)(void))h { if (h) h(); }
- (id)identifier { return nil; }
- (void)dealloc { [_cookies release]; [super dealloc]; }
@end

@protocol WKURLSchemeHandler;
@interface WKWebViewConfiguration : NSObject <NSCopying>
@property (retain) WKPreferences *preferences;
@property (retain) WKUserContentController *userContentController;
@property (retain) WKWebsiteDataStore *websiteDataStore;
@property (retain) WKProcessPool *processPool;
@property (retain) WKWebpagePreferences *defaultWebpagePreferences;
@property (copy) NSString *applicationNameForUserAgent;
@property BOOL suppressesIncrementalRendering;
@property BOOL allowsAirPlayForMediaPlayback;
@property BOOL allowsInlinePredictions;
@property BOOL limitsNavigationsToAppBoundDomains;
@property BOOL upgradeKnownHostsToHTTPS;
@property NSUInteger mediaTypesRequiringUserActionForPlayback;
@property NSInteger userInterfaceDirectionPolicy;
@property NSUInteger dataDetectorTypes;
- (void)setURLSchemeHandler:(id)h forURLScheme:(NSString *)s;
- (id)urlSchemeHandlerForURLScheme:(NSString *)s;
@end
@implementation WKWebViewConfiguration {
@public
    NSMutableDictionary *_schemes;
}
- (instancetype)init {
    if ((self = [super init])) {
        _preferences = [[WKPreferences alloc] init];
        _userContentController = [[WKUserContentController alloc] init];
        _websiteDataStore = [[WKWebsiteDataStore defaultDataStore] retain];
        _processPool = [[WKProcessPool alloc] init];
        _defaultWebpagePreferences = [[WKWebpagePreferences alloc] init];
        _schemes = [[NSMutableDictionary alloc] init];
        _upgradeKnownHostsToHTTPS = YES;
    }
    return self;
}
- (id)copyWithZone:(NSZone *)z {
    WKWebViewConfiguration *c = [[WKWebViewConfiguration alloc] init];
    c.preferences = [[_preferences copy] autorelease];
    c.userContentController = _userContentController;   /* compartilhado, como no WebKit */
    c.websiteDataStore = _websiteDataStore;
    c.processPool = _processPool;
    c.defaultWebpagePreferences = [[_defaultWebpagePreferences copy] autorelease];
    c.applicationNameForUserAgent = _applicationNameForUserAgent;
    c.suppressesIncrementalRendering = _suppressesIncrementalRendering;
    c.mediaTypesRequiringUserActionForPlayback = _mediaTypesRequiringUserActionForPlayback;
    [c->_schemes addEntriesFromDictionary:_schemes];
    return c;
}
- (void)setURLSchemeHandler:(id)h forURLScheme:(NSString *)s {
    if (!s) return;
    NSString *k = [s lowercaseString];
    if ([k isEqualToString:@"http"] || [k isEqualToString:@"https"] || [k isEqualToString:@"file"] || [k isEqualToString:@"about"] ||
        [k isEqualToString:@"data"] || [k isEqualToString:@"blob"] || [k isEqualToString:@"javascript"] || [k isEqualToString:@"ws"] || [k isEqualToString:@"wss"]) {
        [NSException raise:@"NSInvalidArgumentException" format:@"'%@' is a URL scheme that WKWebView handles natively", s];
        return;
    }
    if (h) [_schemes setObject:h forKey:k];
    else [_schemes removeObjectForKey:k];
}
- (id)urlSchemeHandlerForURLScheme:(NSString *)s { return s ? [_schemes objectForKey:[s lowercaseString]] : nil; }
- (void)setValue:(id)v forUndefinedKey:(NSString *)k {}
- (id)valueForUndefinedKey:(NSString *)k { return nil; }
- (void)dealloc {
    [_preferences release]; [_userContentController release]; [_websiteDataStore release]; [_processPool release];
    [_defaultWebpagePreferences release]; [_applicationNameForUserAgent release]; [_schemes release];
    [super dealloc];
}
@end

/* ================================================================ navegação */
@interface WKSecurityOrigin : NSObject
@property (readonly, copy) NSString *protocol;
@property (readonly, copy) NSString *host;
@property (readonly) NSInteger port;
@end
@implementation WKSecurityOrigin {
@public
    NSString *_p, *_h;
    NSInteger _port;
}
- (NSString *)protocol { return _p ?: @""; }
- (NSString *)host { return _h ?: @""; }
- (NSInteger)port { return _port; }
- (void)dealloc { [_p release]; [_h release]; [super dealloc]; }
@end

@interface WKFrameInfo : NSObject <NSCopying>
@property (readonly, getter=isMainFrame) BOOL mainFrame;
@property (readonly, copy) NSURLRequest *request;
@property (readonly) WKSecurityOrigin *securityOrigin;
@property (readonly, assign) WKWebView *webView;
@end
@implementation WKFrameInfo {
@public
    NSURLRequest *_req;
    WKSecurityOrigin *_origin;
    WKWebView *_wv;
}
static WKFrameInfo *wk_frame(WKWebView *wv, NSString *url) {
    WKFrameInfo *f = [[[WKFrameInfo alloc] init] autorelease];
    NSURL *u = url && [url length] ? [NSURL URLWithString:url] : nil;
    f->_req = [(u ? [NSURLRequest requestWithURL:u] : [[[NSURLRequest alloc] init] autorelease]) retain];
    f->_origin = [[WKSecurityOrigin alloc] init];
    f->_origin->_p = [([u scheme] ?: @"") copy];
    f->_origin->_h = [([u host] ?: @"") copy];
    f->_origin->_port = [[u port] integerValue];
    f->_wv = wv;
    return f;
}
- (BOOL)isMainFrame { return YES; }
- (NSURLRequest *)request { return _req; }
- (WKSecurityOrigin *)securityOrigin { return _origin; }
- (WKWebView *)webView { return _wv; }
- (id)copyWithZone:(NSZone *)z { return [self retain]; }
- (void)dealloc { [_req release]; [_origin release]; [super dealloc]; }
@end

@implementation WKScriptMessage {
@public
    id _b;
    WKWebView *_w;
    WKFrameInfo *_f;
    NSString *_n;
}
- (id)body { return _b; }
- (WKWebView *)webView { return _w; }
- (WKFrameInfo *)frameInfo { return _f; }
- (NSString *)name { return _n; }
- (WKContentWorld *)world { return [WKContentWorld pageWorld]; }
- (void)dealloc { [_b release]; [_f release]; [_n release]; [super dealloc]; }
@end

@interface WKNavigation : NSObject
@property (readonly, retain) WKWebpagePreferences *effectiveContentMode;
@end
@implementation WKNavigation
- (WKWebpagePreferences *)effectiveContentMode { return nil; }
@end

@interface WKNavigationAction : NSObject
@property (readonly, copy) NSURLRequest *request;
@property (readonly) WKNavigationType navigationType;
@property (readonly, retain) WKFrameInfo *sourceFrame;
@property (readonly, retain) WKFrameInfo *targetFrame;
@property (readonly) NSUInteger modifierFlags;
@property (readonly) NSInteger buttonNumber;
@property (readonly) BOOL shouldPerformDownload;
@end
@implementation WKNavigationAction {
@public
    NSURLRequest *_r;
    WKNavigationType _t;
    WKFrameInfo *_src, *_dst;
    NSUInteger _mods;
}
- (NSURLRequest *)request { return _r; }
- (WKNavigationType)navigationType { return _t; }
- (WKFrameInfo *)sourceFrame { return _src; }
- (WKFrameInfo *)targetFrame { return _dst; }
- (NSUInteger)modifierFlags { return _mods; }
- (NSInteger)buttonNumber { return _t == WKNavigationTypeLinkActivated ? 1 : 0; }
- (BOOL)shouldPerformDownload { return NO; }
- (void)dealloc { [_r release]; [_src release]; [_dst release]; [super dealloc]; }
@end

@interface WKNavigationResponse : NSObject
@property (readonly, getter=isForMainFrame) BOOL forMainFrame;
@property (readonly, copy) NSURLResponse *response;
@property (readonly) BOOL canShowMIMEType;
@end
@implementation WKNavigationResponse {
@public
    NSURLResponse *_resp;
}
- (BOOL)isForMainFrame { return YES; }
- (NSURLResponse *)response { return _resp; }
- (BOOL)canShowMIMEType { return YES; }
- (void)dealloc { [_resp release]; [super dealloc]; }
@end

@interface WKWindowFeatures : NSObject
@property (readonly) NSNumber *menuBarVisibility, *statusBarVisibility, *toolbarsVisibility, *allowsResizing, *x, *y, *width, *height;
@end
@implementation WKWindowFeatures
@end

@interface WKBackForwardListItem : NSObject
@property (readonly, copy) NSURL *URL;
@property (readonly, copy) NSString *title;
@property (readonly, copy) NSURL *initialURL;
@end
@implementation WKBackForwardListItem {
@public
    NSURL *_u;
    NSString *_t;
    int _rel;
}
- (NSURL *)URL { return _u; }
- (NSURL *)initialURL { return _u; }
- (NSString *)title { return _t; }
- (void)dealloc { [_u release]; [_t release]; [super dealloc]; }
@end

@interface WKBackForwardList : NSObject
@property (readonly) WKBackForwardListItem *currentItem, *backItem, *forwardItem;
@property (readonly, copy) NSArray *backList, *forwardList;
- (WKBackForwardListItem *)itemAtIndex:(NSInteger)i;
@end
@implementation WKBackForwardList {
@public
    int _wid;
}
- (WKBackForwardListItem *)itemAtIndex:(NSInteger)i {
    mos_web_hist_t h;
    if (_wid <= 0 || wk_sys(MOS_WEB_HISTORY, _wid, (long)i, (long)&h) != 1) return nil;
    WKBackForwardListItem *it = [[[WKBackForwardListItem alloc] init] autorelease];
    it->_u = [[NSURL URLWithString:wk_cstr(h.url)] retain];
    it->_t = [wk_cstr(h.title) copy];
    it->_rel = (int)i;
    return it;
}
- (WKBackForwardListItem *)currentItem { return [self itemAtIndex:0]; }
- (WKBackForwardListItem *)backItem { return [self itemAtIndex:-1]; }
- (WKBackForwardListItem *)forwardItem { return [self itemAtIndex:1]; }
- (NSArray *)backList {
    NSMutableArray *a = [NSMutableArray array];
    for (int i = -1;; i--) { WKBackForwardListItem *it = [self itemAtIndex:i]; if (!it) break; [a insertObject:it atIndex:0]; }
    return a;
}
- (NSArray *)forwardList {
    NSMutableArray *a = [NSMutableArray array];
    for (int i = 1;; i++) { WKBackForwardListItem *it = [self itemAtIndex:i]; if (!it) break; [a addObject:it]; }
    return a;
}
@end

@interface WKFindConfiguration : NSObject <NSCopying>
@property BOOL backwards, caseSensitive, wraps;
@end
@implementation WKFindConfiguration
- (instancetype)init { if ((self = [super init])) _wraps = YES; return self; }
- (id)copyWithZone:(NSZone *)z { return [self retain]; }
@end
@interface WKFindResult : NSObject
@property (readonly) BOOL matchFound;
@end
@implementation WKFindResult {
@public
    BOOL _found;
}
- (BOOL)matchFound { return _found; }
@end

@interface WKSnapshotConfiguration : NSObject <NSCopying>
@property NSRect rect;
@property (retain) NSNumber *snapshotWidth;
@property BOOL afterScreenUpdates;
@end
@implementation WKSnapshotConfiguration
- (instancetype)init { if ((self = [super init])) { _rect = NSMakeRect(0, 0, 0, 0); _afterScreenUpdates = YES; } return self; }
- (id)copyWithZone:(NSZone *)z { return [self retain]; }
- (void)dealloc { [_snapshotWidth release]; [super dealloc]; }
@end
@interface WKPDFConfiguration : NSObject <NSCopying>
@property NSRect rect;
@end
@implementation WKPDFConfiguration
- (id)copyWithZone:(NSZone *)z { return [self retain]; }
@end

@interface WKOpenPanelParameters : NSObject
@property (readonly) BOOL allowsMultipleSelection, allowsDirectories;
@end
@implementation WKOpenPanelParameters
@end

/* ---------------------------------------------------------------- esquemas próprios (WKURLSchemeHandler) */
@protocol WKURLSchemeTask <NSObject>
@property (readonly, copy) NSURLRequest *request;
- (void)didReceiveResponse:(NSURLResponse *)r;
- (void)didReceiveData:(NSData *)d;
- (void)didFinish;
- (void)didFailWithError:(NSError *)e;
@end
@interface WKMOSSchemeTask : NSObject <WKURLSchemeTask> {
@public
    NSURLRequest *_req;
    NSURLResponse *_resp;
    NSMutableData *_data;
    WKWebView *_wv;
    id _handler;
    BOOL _done, _stopped;
}
@end

/* ================================================================ WKWebView */
@class WKNavigation, WKNavigationAction, WKNavigationResponse, WKWindowFeatures, WKScriptMessage, WKWebpagePreferences;
@protocol WKNavigationDelegate <NSObject>
@optional
- (void)webView:(WKWebView *)w decidePolicyForNavigationAction:(WKNavigationAction *)a decisionHandler:(void (^)(WKNavigationActionPolicy))h;
- (void)webView:(WKWebView *)w decidePolicyForNavigationAction:(WKNavigationAction *)a preferences:(WKWebpagePreferences *)p
    decisionHandler:(void (^)(WKNavigationActionPolicy, WKWebpagePreferences *))h;
- (void)webView:(WKWebView *)w decidePolicyForNavigationResponse:(WKNavigationResponse *)r decisionHandler:(void (^)(WKNavigationResponsePolicy))h;
- (void)webView:(WKWebView *)w didStartProvisionalNavigation:(WKNavigation *)n;
- (void)webView:(WKWebView *)w didReceiveServerRedirectForProvisionalNavigation:(WKNavigation *)n;
- (void)webView:(WKWebView *)w didFailProvisionalNavigation:(WKNavigation *)n withError:(NSError *)e;
- (void)webView:(WKWebView *)w didCommitNavigation:(WKNavigation *)n;
- (void)webView:(WKWebView *)w didFinishNavigation:(WKNavigation *)n;
- (void)webView:(WKWebView *)w didFailNavigation:(WKNavigation *)n withError:(NSError *)e;
- (void)webViewWebContentProcessDidTerminate:(WKWebView *)w;
@end
@protocol WKUIDelegate <NSObject>
@optional
- (WKWebView *)webView:(WKWebView *)w createWebViewWithConfiguration:(id)c forNavigationAction:(WKNavigationAction *)a windowFeatures:(WKWindowFeatures *)f;
- (void)webViewDidClose:(WKWebView *)w;
- (void)webView:(WKWebView *)w runJavaScriptAlertPanelWithMessage:(NSString *)m initiatedByFrame:(id)f completionHandler:(void (^)(void))h;
- (void)webView:(WKWebView *)w runJavaScriptConfirmPanelWithMessage:(NSString *)m initiatedByFrame:(id)f completionHandler:(void (^)(BOOL))h;
- (void)webView:(WKWebView *)w runJavaScriptTextInputPanelWithPrompt:(NSString *)p defaultText:(NSString *)d initiatedByFrame:(id)f
    completionHandler:(void (^)(NSString *))h;
@end
@protocol WKScriptMessageHandler <NSObject>
- (void)userContentController:(id)c didReceiveScriptMessage:(WKScriptMessage *)m;
@end
@protocol WKScriptMessageHandlerWithReply <NSObject>
- (void)userContentController:(id)c didReceiveScriptMessage:(WKScriptMessage *)m replyHandler:(void (^)(id, NSString *))h;
@end
@protocol WKURLSchemeHandler <NSObject>
- (void)webView:(WKWebView *)w startURLSchemeTask:(id)t;
- (void)webView:(WKWebView *)w stopURLSchemeTask:(id)t;
@end
@protocol WebPolicyDelegate <NSObject>
@optional
- (void)webView:(id)w decidePolicyForNavigationAction:(NSDictionary *)info request:(NSURLRequest *)r frame:(id)f decisionListener:(id)l;
@end
@protocol WebFrameLoadDelegate <NSObject>
@optional
- (void)webView:(id)w didStartProvisionalLoadForFrame:(id)f;
- (void)webView:(id)w didCommitLoadForFrame:(id)f;
- (void)webView:(id)w didFinishLoadForFrame:(id)f;
- (void)webView:(id)w didFailProvisionalLoadWithError:(NSError *)e forFrame:(id)f;
- (void)webView:(id)w didFailLoadWithError:(NSError *)e forFrame:(id)f;
- (void)webView:(id)w didReceiveTitle:(NSString *)t forFrame:(id)f;
@end
@interface WKWebView (MOSLegacyPolicy)
- (void)_mosLegacyDecide:(WKNavigationAction *)act url:(NSString *)url seq:(int)seq;
@end

@interface WKTimerTarget : NSObject {
@public
    WKWebView *wv;
}
@end


#define WK_MAXPENDING 64
@implementation WKWebView {
@public
    int _wid;
    WKWebViewConfiguration *_config;
    NSTimer *_timer;
    WKTimerTarget *_tt;
    unsigned int *_buf;
    size_t _bufcap;
    int _sw, _sh;               /* tamanho mandado ao kernel */
    NSString *_title;
    NSURL *_URL;
    BOOL _loading, _canBack, _canForward, _secure, _committed;
    double _progress;
    WKNavigation *_nav;          /* navegação em andamento */
    WKNavigation *_pendingNav;   /* criada por load*, à espera do START */
    int _reqSeq;                 /* identifica a decisão pendente */
    WKBackForwardList *_bfl;
    double _pageZoom, _magnification;
    BOOL _drawsBackground;
    NSTrackingArea *_ta;
    void *_pending[WK_MAXPENDING];   /* blocos para o próximo tique (completions assíncronos) */
    int _npending;
    int _asyncSeq;
    NSMutableDictionary *_asyncHandlers;   /* id -> bloco de callAsyncJavaScript */
    NSMutableArray *_schemeTasks;
    int _cursor;
    BOOL _closed;
    NSMutableArray *_kernelHandlers;       /* nomes que o kernel conhece (MOS_WEB_HANDLER) */
}

static NSString *const kReplyShimFmt =
    @"(function(){var hs=window.webkit&&window.webkit.messageHandlers;if(!hs)return;var n=%@;var h=hs[n];"
     "if(!h||h.__mosReply)return;var post=h.postMessage;var seq=0;window.__mosReplies=window.__mosReplies||{};"
     "h.__mosReply=true;h.postMessage=function(b){var id=n+':'+(++seq);return new Promise(function(res,rej){"
     "window.__mosReplies[id]={res:res,rej:rej};post.call(h,{__mosReplyId:id,body:b});});};})();";

- (void)_mosSetup:(WKWebViewConfiguration *)configuration {
    _config = configuration ? [configuration copy] : [[WKWebViewConfiguration alloc] init];
    _wid = (int)wk_sys(MOS_WEB_NEW, 0, 0, 0);
    if (_wid <= 0) fprintf(stderr, "WebKit[mOS]: o motor web recusou a vista (%d)\n", _wid);
    _pageZoom = 1;
    _magnification = 1;
    _drawsBackground = YES;
    _progress = 0;
    _bfl = [[WKBackForwardList alloc] init];
    _bfl->_wid = _wid;
    _asyncHandlers = [[NSMutableDictionary alloc] init];
    _schemeTasks = [[NSMutableArray alloc] init];
    _kernelHandlers = [[NSMutableArray alloc] init];
    [_config.userContentController _attach:self];
    wk_sys(MOS_WEB_POLICY, _wid, 1, 0);   /* toda navegação passa pelo framework (delegate e esquemas próprios) */
    [self _mosApplyOptions];
    [self _mosHandlersChanged];
    [self _mosScriptsChanged];
    NSRect b = [self bounds];
    [self _mosSyncSize:b.size];
    _tt = [[WKTimerTarget alloc] init];
    _tt->wv = self;
    _timer = [[NSTimer scheduledTimerWithTimeInterval:1.0 / 60 target:_tt selector:@selector(fire:) userInfo:nil repeats:YES] retain];
}

- (instancetype)initWithFrame:(NSRect)frame configuration:(WKWebViewConfiguration *)configuration {
    if ((self = [super initWithFrame:frame])) [self _mosSetup:configuration];
    return self;
}
- (instancetype)initWithFrame:(NSRect)frame { return [self initWithFrame:frame configuration:nil]; }
- (instancetype)initWithCoder:(NSCoder *)c {
    if ((self = [super initWithFrame:NSMakeRect(0, 0, 400, 300)])) [self _mosSetup:nil];
    return self;
}

- (void)dealloc {
    [_timer invalidate];
    [_timer release];
    if (_tt) _tt->wv = nil;
    [_tt release];
    [_config.userContentController _detach:self];
    if (_wid > 0) wk_sys(MOS_WEB_FREE, _wid, 0, 0);
    for (int i = 0; i < _npending; i++) _Block_release(_pending[i]);
    free(_buf);
    [_config release]; [_title release]; [_URL release]; [_nav release]; [_pendingNav release]; [_bfl release];
    [_asyncHandlers release]; [_schemeTasks release]; [_ta release]; [_kernelHandlers release];
    [_customUserAgent release]; [_underPageBackgroundColor release]; [_mediaType release]; [_interactionState release];
    [super dealloc];
}

+ (BOOL)handlesURLScheme:(NSString *)s {
    NSString *k = [s lowercaseString];
    return [k isEqualToString:@"http"] || [k isEqualToString:@"https"] || [k isEqualToString:@"file"] || [k isEqualToString:@"about"] ||
           [k isEqualToString:@"data"] || [k isEqualToString:@"blob"] || [k isEqualToString:@"javascript"] || [k isEqualToString:@"ws"] ||
           [k isEqualToString:@"wss"];
}

- (WKWebViewConfiguration *)configuration { return [[_config copy] autorelease]; }
- (BOOL)isFlipped { return YES; }
- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)acceptsFirstMouse:(NSEvent *)e { return YES; }
- (BOOL)isOpaque { return _drawsBackground; }

/* ---------------------------------------------------------------- opções, handlers e scripts */
- (void)_mosApplyOptions {
    long o = 0;
    if (!_config.preferences.javaScriptEnabled || !_config.defaultWebpagePreferences.allowsContentJavaScript) o |= MOS_WEB_OPT_NOJS;
    if (!_drawsBackground) o |= MOS_WEB_OPT_TRANSPARENT;
    wk_sys(MOS_WEB_OPTS, _wid, o, 0);
}
- (void)_mosHandlersChanged {
    /* o kernel guarda a lista e a repõe em cada página nova; ele recusa nome repetido, então tira os que conhece e
     * põe a lista atual do controlador */
    for (NSString *n in _kernelHandlers) wk_sys(MOS_WEB_HANDLER, _wid, (long)[n UTF8String], 0);
    [_kernelHandlers removeAllObjects];
    for (WKHandlerEntry *e in _config.userContentController->_handlers)
        if (wk_sys(MOS_WEB_HANDLER, _wid, (long)[e->name UTF8String], 1) == 0) [_kernelHandlers addObject:e->name];
    [self _mosScriptsChanged];   /* o calço de reply depende da lista */
}
- (void)_mosAddScript:(NSString *)src atEnd:(BOOL)end {
    const char *s = [src UTF8String];
    mos_web_script_t S = {(unsigned long long)(uintptr_t)s, (unsigned)strlen(s), end ? 1 : 0};
    wk_sys(MOS_WEB_SCRIPT, _wid, (long)&S, 0);
}
- (void)_mosScriptsChanged {
    mos_web_script_t clear = {0, 0, 0};
    wk_sys(MOS_WEB_SCRIPT, _wid, (long)&clear, 0);
    WKUserContentController *ucc = _config.userContentController;
    for (WKHandlerEntry *e in ucc->_handlers)
        if (e->reply) [self _mosAddScript:[NSString stringWithFormat:kReplyShimFmt, wk_js_literal(e->name)] atEnd:NO];
    for (WKUserScript *s in ucc->_scripts) [self _mosAddScript:s.source atEnd:s.injectionTime == WKUserScriptInjectionTimeAtDocumentEnd];
}

/* ---------------------------------------------------------------- tamanho e desenho */
- (void)_mosSyncSize:(NSSize)s {
    int w = (int)(s.width + 0.5), h = (int)(s.height + 0.5);
    if (w < 0) w = 0;
    if (h < 0) h = 0;
    if (w == _sw && h == _sh) return;
    _sw = w; _sh = h;
    wk_sys(MOS_WEB_SIZE, _wid, w, h);
    [self setNeedsDisplay:YES];
}
- (void)setFrameSize:(NSSize)s { [super setFrameSize:s]; [self _mosSyncSize:s]; }
- (void)setFrame:(NSRect)f { [super setFrame:f]; [self _mosSyncSize:f.size]; }

- (BOOL)_mosPaint {
    if (_sw <= 0 || _sh <= 0 || _wid <= 0) return NO;
    size_t need = (size_t)_sw * _sh * 4;
    if (need > _bufcap) {
        free(_buf);
        _buf = malloc(need);
        _bufcap = _buf ? need : 0;
        if (!_buf) return NO;
    }
    return wk_sys(MOS_WEB_PAINT, _wid, (long)_buf, (long)_bufcap) == (long)need;
}

- (void)drawRect:(NSRect)dirty {
    [self _mosSyncSize:[self bounds].size];
    if (![self _mosPaint]) return;
    double ox = 0, oy = 0, sy = 1;
    cx_gfx_view_xform(&ox, &oy, &sy);
    cx_gfx_image_px((int)(ox + 0.5), (int)(oy + 0.5), _sw, _sh, _buf);
}

- (void)viewDidMoveToWindow {
    if ([self window]) [self updateTrackingAreas];
}
- (void)updateTrackingAreas {
    if (_ta) { [self removeTrackingArea:_ta]; [_ta release]; _ta = nil; }
    /* NSTrackingMouseEnteredAndExited | MouseMoved | ActiveInKeyWindow | InVisibleRect */
    _ta = [[NSTrackingArea alloc] initWithRect:[self bounds] options:0x01 | 0x02 | 0x20 | 0x200 owner:self userInfo:nil];
    [self addTrackingArea:_ta];
    [super updateTrackingAreas];
}

/* ---------------------------------------------------------------- entrada */
static int wk_mods(NSEvent *e) {
    NSUInteger f = [e modifierFlags];
    int m = 0;
    if (f & (1 << 17)) m |= MOS_WEB_M_SHIFT;
    if (f & (1 << 18)) m |= MOS_WEB_M_CTRL;
    if (f & (1 << 19)) m |= MOS_WEB_M_ALT;
    if (f & (1 << 20)) m |= MOS_WEB_M_CMD;
    return m;
}
- (void)_mosInput:(mos_web_input_t *)in {
    long cur = wk_sys(MOS_WEB_INPUT, _wid, (long)in, 0);
    if (cur >= 0 && cur != _cursor) {
        _cursor = (int)cur;
        [(cur == MOS_WEB_CURSOR_HAND ? [NSCursor pointingHandCursor] : cur == MOS_WEB_CURSOR_IBEAM ? [NSCursor IBeamCursor] : [NSCursor arrowCursor]) set];
    }
    [self _mosTick];
}
- (void)_mosMouse:(NSEvent *)e kind:(int)kind {
    NSPoint p = [self convertPoint:[e locationInWindow] fromView:nil];
    mos_web_input_t in;
    memset(&in, 0, sizeof in);
    in.kind = kind;
    in.x = (int)p.x;
    in.y = (int)p.y;
    in.mods = wk_mods(e);
    in.clicks = kind == MOS_WEB_IN_DOWN ? (int)[e clickCount] : 0;
    [self _mosInput:&in];
}
- (void)mouseDown:(NSEvent *)e { [[self window] makeFirstResponder:self]; [self _mosMouse:e kind:MOS_WEB_IN_DOWN]; }
- (void)mouseUp:(NSEvent *)e { [self _mosMouse:e kind:MOS_WEB_IN_UP]; }
- (void)mouseDragged:(NSEvent *)e { [self _mosMouse:e kind:MOS_WEB_IN_MOVE]; }
- (void)mouseMoved:(NSEvent *)e { [self _mosMouse:e kind:MOS_WEB_IN_MOVE]; }
- (void)mouseEntered:(NSEvent *)e { [self _mosMouse:e kind:MOS_WEB_IN_MOVE]; }
- (void)mouseExited:(NSEvent *)e { [self _mosMouse:e kind:MOS_WEB_IN_EXIT]; }
- (void)rightMouseDown:(NSEvent *)e { [self _mosMouse:e kind:MOS_WEB_IN_MOVE]; }
- (void)scrollWheel:(NSEvent *)e {
    NSPoint p = [self convertPoint:[e locationInWindow] fromView:nil];
    mos_web_input_t in;
    memset(&in, 0, sizeof in);
    in.kind = MOS_WEB_IN_WHEEL;
    in.x = (int)p.x;
    in.y = (int)p.y;
    double dy = [e scrollingDeltaY];
    in.wheel = (int)(-dy * ([e hasPreciseScrollingDeltas] ? 1 : 40));
    if (!in.wheel) return;
    [self _mosInput:&in];
}
- (void)keyDown:(NSEvent *)e {
    mos_web_input_t in;
    memset(&in, 0, sizeof in);
    in.kind = MOS_WEB_IN_KEY;
    in.mods = wk_mods(e);
    switch ([e keyCode]) {
    case 123: in.key = MOS_WEB_K_LEFT; break;
    case 124: in.key = MOS_WEB_K_RIGHT; break;
    case 125: in.key = MOS_WEB_K_DOWN; break;
    case 126: in.key = MOS_WEB_K_UP; break;
    case 51: in.key = MOS_WEB_K_BACKSPACE; break;
    case 117: in.key = MOS_WEB_K_DELETE; break;
    case 36: case 76: in.key = MOS_WEB_K_ENTER; break;
    case 48: in.key = MOS_WEB_K_TAB; break;
    case 53: in.key = MOS_WEB_K_ESC; break;
    case 115: in.key = MOS_WEB_K_HOME; break;
    case 119: in.key = MOS_WEB_K_END; break;
    case 116: in.key = MOS_WEB_K_PGUP; break;
    case 121: in.key = MOS_WEB_K_PGDN; break;
    default: break;
    }
    if (!in.key) {
        NSString *ch = [e characters];
        const char *u = ch ? [ch UTF8String] : NULL;
        if (!u || !*u) return;
        strncpy(in.text, u, sizeof in.text - 1);
        unsigned char c0 = (unsigned char)u[0];
        if (c0 == 0x7f) in.key = MOS_WEB_K_BACKSPACE, in.text[0] = 0;
        else if (c0 == '\r' || c0 == '\n') in.key = MOS_WEB_K_ENTER, in.text[0] = 0;
        else if (c0 == '\t') in.key = MOS_WEB_K_TAB, in.text[0] = 0;
        else if (c0 == 0x1b) in.key = MOS_WEB_K_ESC, in.text[0] = 0;
        else if (c0 == 0xEF && (unsigned char)u[1] >= 0x9C && (unsigned char)u[1] <= 0xA3) return;   /* U+F700–U+F8FF: teclas de função do AppKit */
        else in.key = c0 < 0x80 ? c0 : 0;
    }
    [self _mosInput:&in];
}
- (BOOL)resignFirstResponder {
    mos_web_input_t in;
    memset(&in, 0, sizeof in);
    in.kind = MOS_WEB_IN_BLUR;
    wk_sys(MOS_WEB_INPUT, _wid, (long)&in, 0);
    [self setNeedsDisplay:YES];
    return YES;
}

/* ---------------------------------------------------------------- KVO */
- (void)_mosSet:(NSString *)key old:(BOOL)ov new:(BOOL)nv store:(BOOL *)slot {
    if (ov == nv) return;
    [self willChangeValueForKey:key];
    *slot = nv;
    [self didChangeValueForKey:key];
}

- (void)_mosRefreshInfo:(mos_web_info_t *)I events:(long)ev {
    NSString *t = wk_cstr(I->title);
    if (![t isEqualToString:_title ?: @""]) {
        [self willChangeValueForKey:@"title"];
        [_title release];
        _title = [t copy];
        [self didChangeValueForKey:@"title"];
        [self _mosTitleChanged];
    }
    NSString *us = wk_cstr(I->url);
    if (![us isEqualToString:[_URL absoluteString] ?: @""]) {
        [self willChangeValueForKey:@"URL"];
        [_URL release];
        _URL = [us length] ? [[NSURL URLWithString:us] retain] : nil;
        [self didChangeValueForKey:@"URL"];
    }
    [self _mosSet:@"canGoBack" old:_canBack new:I->can_back != 0 store:&_canBack];
    [self _mosSet:@"canGoForward" old:_canForward new:I->can_forward != 0 store:&_canForward];
    [self _mosSet:@"hasOnlySecureContent" old:_secure new:I->secure != 0 store:&_secure];
    double pr = I->progress / 1000.0;
    if (pr != _progress) {
        [self willChangeValueForKey:@"estimatedProgress"];
        _progress = pr;
        [self didChangeValueForKey:@"estimatedProgress"];
    }
}

/* ---------------------------------------------------------------- tique */
- (void)_mosDefer:(void (^)(void))b {
    if (!b) return;
    if (_npending >= WK_MAXPENDING) { b(); return; }
    _pending[_npending++] = _Block_copy(b);
}

- (WKNavigationAction *)_mosAction:(NSString *)url kind:(int)kind {
    WKNavigationAction *a = [[[WKNavigationAction alloc] init] autorelease];
    NSURL *u = [NSURL URLWithString:url];
    a->_r = [(u ? [NSURLRequest requestWithURL:u] : [[[NSURLRequest alloc] init] autorelease]) retain];
    a->_t = (WKNavigationType)kind;
    a->_src = [wk_frame(self, [_URL absoluteString]) retain];
    a->_dst = [wk_frame(self, url) retain];
    return a;
}

- (void)_mosDecide:(BOOL)allow seq:(int)seq url:(NSString *)url {
    if (seq != _reqSeq || _closed) return;
    _reqSeq++;
    NSURL *u = [NSURL URLWithString:url];
    id sh = u ? [_config urlSchemeHandlerForURLScheme:[u scheme]] : nil;
    if (allow && sh) {
        wk_sys(MOS_WEB_DECIDE, _wid, 0, 0);
        [self _mosStartSchemeTask:sh request:[NSURLRequest requestWithURL:u]];
        return;
    }
    wk_sys(MOS_WEB_DECIDE, _wid, allow ? 1 : 0, 0);
    if (!allow) { [_pendingNav release]; _pendingNav = nil; }
}

- (void)_mosNavRequest:(mos_web_info_t *)I {
    int seq = ++_reqSeq;
    NSString *url = wk_cstr(I->req_url);
    WKNavigationAction *act = [self _mosAction:url kind:I->req_kind];
    id d = _navigationDelegate;
    __block BOOL called = NO;
    if (d && [d respondsToSelector:@selector(webView:decidePolicyForNavigationAction:preferences:decisionHandler:)]) {
        [d webView:self decidePolicyForNavigationAction:act preferences:[[_config.defaultWebpagePreferences copy] autorelease]
            decisionHandler:^(WKNavigationActionPolicy p, WKWebpagePreferences *pref) {
                if (called) return;
                called = YES;
                [self _mosDecide:p == WKNavigationActionPolicyAllow seq:seq url:url];
            }];
    } else if (d && [d respondsToSelector:@selector(webView:decidePolicyForNavigationAction:decisionHandler:)]) {
        [d webView:self decidePolicyForNavigationAction:act decisionHandler:^(WKNavigationActionPolicy p) {
            if (called) return;
            called = YES;
            [self _mosDecide:p == WKNavigationActionPolicyAllow seq:seq url:url];
        }];
    } else [self _mosLegacyDecide:act url:url seq:seq];   /* WebView: o policyDelegate do WebKit1; senão permite */
}

- (void)_mosNewWindow:(mos_web_info_t *)I {
    NSString *url = wk_cstr(I->newwin);
    if (![url length]) return;
    id u = _UIDelegate;
    if (u && [u respondsToSelector:@selector(webView:createWebViewWithConfiguration:forNavigationAction:windowFeatures:)]) {
        WKWebViewConfiguration *cfg = [[_config copy] autorelease];
        WKWebView *nw = [u webView:self createWebViewWithConfiguration:cfg forNavigationAction:[self _mosAction:url kind:0]
                    windowFeatures:[[[WKWindowFeatures alloc] init] autorelease]];
        NSURL *nu = [NSURL URLWithString:url];
        if (nw && nu) [nw loadRequest:[NSURLRequest requestWithURL:nu]];
    }
}

- (void)_mosHandleMessages {
    size_t cap = 4096;
    char *buf = malloc(cap);
    mos_web_msg_t *M = calloc(1, sizeof *M);
    if (!buf || !M) { free(buf); free(M); return; }
    for (;;) {
        M->body = (unsigned long long)(uintptr_t)buf;
        M->cap = (unsigned)cap;
        long r = wk_sys(MOS_WEB_TAKEMSG, _wid, (long)M, 0);
        if (r == -MOS_EINVAL && M->len > cap) {
            char *nb = realloc(buf, (size_t)M->len + 1);
            if (!nb) break;
            buf = nb;
            cap = (size_t)M->len + 1;
            continue;
        }
        if (r != 1) break;
        BOOL bad = NO;
        id body = wk_value(M->type, buf, M->len, &bad);
        NSString *name = wk_cstr(M->name);
        NSString *src = wk_cstr(M->url);
        if ([name isEqualToString:@"__mosAlert"]) { [self _mosAlert:body frame:src]; continue; }
        if ([name isEqualToString:@"__mosAsync"]) { [self _mosAsyncDone:body]; continue; }
        WKHandlerEntry *e = [_config.userContentController _entryNamed:name];
        if (!e) continue;
        NSString *replyId = nil;
        if (e->reply && [body isKindOfClass:[NSDictionary class]] && [body objectForKey:@"__mosReplyId"]) {
            replyId = [body objectForKey:@"__mosReplyId"];
            body = [body objectForKey:@"body"];
        }
        WKScriptMessage *m = [[WKScriptMessage alloc] init];
        m->_b = [body retain];
        m->_w = self;
        m->_f = [wk_frame(self, src) retain];
        m->_n = [name copy];
        if (e->reply && [e->handler respondsToSelector:@selector(userContentController:didReceiveScriptMessage:replyHandler:)]) {
            NSString *rid = [[replyId copy] autorelease];
            [e->handler userContentController:_config.userContentController didReceiveScriptMessage:m replyHandler:^(id reply, NSString *err) {
                if (!rid) return;
                NSString *js = err ? [NSString stringWithFormat:@"(function(r){if(r){delete window.__mosReplies[%@];r.rej(new Error(%@));}})(window.__mosReplies&&window.__mosReplies[%@])",
                                         wk_js_literal(rid), wk_js_literal(err), wk_js_literal(rid)]
                                   : [NSString stringWithFormat:@"(function(r){if(r){delete window.__mosReplies[%@];r.res(%@);}})(window.__mosReplies&&window.__mosReplies[%@])",
                                         wk_js_literal(rid), wk_js_literal(reply), wk_js_literal(rid)];
                [self _mosEvalRaw:js];
            }];
        } else if ([e->handler respondsToSelector:@selector(userContentController:didReceiveScriptMessage:)])
            [e->handler userContentController:_config.userContentController didReceiveScriptMessage:m];
        [m release];
    }
    free(buf);
    free(M);
}

- (void)_mosAlert:(id)body frame:(NSString *)src {
    if (![body isKindOfClass:[NSDictionary class]]) return;
    NSString *kind = [body objectForKey:@"kind"], *msg = [body objectForKey:@"message"];
    if (![msg isKindOfClass:[NSString class]]) msg = @"";
    id u = _UIDelegate;
    WKFrameInfo *fi = wk_frame(self, src);
    if ([kind isEqualToString:@"alert"] && [u respondsToSelector:@selector(webView:runJavaScriptAlertPanelWithMessage:initiatedByFrame:completionHandler:)])
        [u webView:self runJavaScriptAlertPanelWithMessage:msg initiatedByFrame:fi completionHandler:^{}];
    else if ([kind isEqualToString:@"confirm"] && [u respondsToSelector:@selector(webView:runJavaScriptConfirmPanelWithMessage:initiatedByFrame:completionHandler:)])
        [u webView:self runJavaScriptConfirmPanelWithMessage:msg initiatedByFrame:fi completionHandler:^(BOOL r) {}];
    else if ([kind isEqualToString:@"prompt"] && [u respondsToSelector:@selector(webView:runJavaScriptTextInputPanelWithPrompt:defaultText:initiatedByFrame:completionHandler:)]) {
        id dt = [body objectForKey:@"defaultText"];
        [u webView:self runJavaScriptTextInputPanelWithPrompt:msg defaultText:[dt isKindOfClass:[NSString class]] ? dt : nil
            initiatedByFrame:fi completionHandler:^(NSString *r) {}];
    } else if ([kind isEqualToString:@"close"] && [u respondsToSelector:@selector(webViewDidClose:)]) [u webViewDidClose:self];
}

- (NSError *)_mosNavError:(mos_web_info_t *)I {
    NSString *url = wk_cstr(I->req_url[0] ? I->req_url : I->url);
    NSInteger code = I->error_code ? I->error_code : -1;
    NSString *msg = wk_cstr(I->error);
    NSDictionary *extra = [NSDictionary dictionaryWithObjectsAndKeys:url, @"NSErrorFailingURLStringKey", url, @"NSURLErrorFailingURLStringErrorKey",
                           [NSURL URLWithString:url] ?: (id)url, @"NSErrorFailingURLKey", nil];
    return wk_error(code == -1 ? WKErrorDomain : NSURLErrorDomainWK, code == -1 ? WKErrorUnknown : code, msg, extra);
}

/* ganchos que a WebView legada sobrescreve */
- (void)_mosDidStart {}
- (void)_mosDidCommit {}
- (void)_mosDidFinish {}
- (void)_mosDidFail:(NSError *)e provisional:(BOOL)prov {}
- (void)_mosTitleChanged {}

- (void)_mosTick {
    if (_wid <= 0 || _closed) return;
    /* completions adiados primeiro (como o WebKit, nunca dentro da chamada que os pediu) */
    if (_npending) {
        int n = _npending;
        void *run[WK_MAXPENDING];
        memcpy(run, _pending, sizeof(void *) * (size_t)n);
        _npending = 0;
        [self retain];
        for (int i = 0; i < n; i++) { ((void (^)(void))run[i])(); _Block_release(run[i]); }
        [self release];
    }
    long ev = wk_sys(MOS_WEB_PUMP, _wid, 0, 0);
    if (ev <= 0) return;
    [self retain];
    mos_web_info_t *I = calloc(1, sizeof *I);
    if (!I) { [self release]; return; }
    wk_sys(MOS_WEB_INFO, _wid, (long)I, 0);
    id d = _navigationDelegate;
    if (ev & MOS_WEB_EV_START) {
        [_nav release];
        _nav = _pendingNav ? _pendingNav : [[WKNavigation alloc] init];
        _pendingNav = nil;
        _committed = NO;
        if (!_loading) { [self willChangeValueForKey:@"loading"]; _loading = YES; [self didChangeValueForKey:@"loading"]; }
        if ([d respondsToSelector:@selector(webView:didStartProvisionalNavigation:)]) [d webView:self didStartProvisionalNavigation:_nav];
        [self _mosDidStart];
    }
    if ((ev & MOS_WEB_EV_REDIRECT) && [d respondsToSelector:@selector(webView:didReceiveServerRedirectForProvisionalNavigation:)])
        [d webView:self didReceiveServerRedirectForProvisionalNavigation:_nav];
    if (ev & MOS_WEB_EV_COMMIT) {
        _committed = YES;
        if ([d respondsToSelector:@selector(webView:decidePolicyForNavigationResponse:decisionHandler:)]) {
            WKNavigationResponse *nr = [[[WKNavigationResponse alloc] init] autorelease];
            NSURL *u = [NSURL URLWithString:wk_cstr(I->url)];
            nr->_resp = [[NSHTTPURLResponse alloc] initWithURL:u statusCode:I->http_status ?: 200 HTTPVersion:@"HTTP/1.1"
                                                  headerFields:[NSDictionary dictionaryWithObject:wk_cstr(I->mime) forKey:@"Content-Type"]];
            __block int cancel = -1;
            [d webView:self decidePolicyForNavigationResponse:nr decisionHandler:^(WKNavigationResponsePolicy p) { if (cancel < 0) cancel = p != WKNavigationResponsePolicyAllow; }];
            /* o documento já chegou: "cancelar" volta ao anterior, o mais perto do WebKit que dá depois do commit */
            if (cancel == 1) { if (I->can_back) [self goBack]; else [self loadHTMLString:@"" baseURL:nil]; }
        }
        if ([d respondsToSelector:@selector(webView:didCommitNavigation:)]) [d webView:self didCommitNavigation:_nav];
        [self _mosDidCommit];
    }
    [self _mosRefreshInfo:I events:ev];
    if (ev & MOS_WEB_EV_MSG) [self _mosHandleMessages];
    if (ev & MOS_WEB_EV_NAVREQ) [self _mosNavRequest:I];
    if (ev & MOS_WEB_EV_NEWWIN) [self _mosNewWindow:I];
    if (ev & MOS_WEB_EV_FAIL) {
        NSError *e = [self _mosNavError:I];
        BOOL prov = !_committed;
        if (_loading) { [self willChangeValueForKey:@"loading"]; _loading = NO; [self didChangeValueForKey:@"loading"]; }
        if (prov && [d respondsToSelector:@selector(webView:didFailProvisionalNavigation:withError:)]) [d webView:self didFailProvisionalNavigation:_nav withError:e];
        else if (!prov && [d respondsToSelector:@selector(webView:didFailNavigation:withError:)]) [d webView:self didFailNavigation:_nav withError:e];
        [self _mosDidFail:e provisional:prov];
    }
    if (ev & MOS_WEB_EV_FINISH) {
        if (_loading) { [self willChangeValueForKey:@"loading"]; _loading = NO; [self didChangeValueForKey:@"loading"]; }
        if ([d respondsToSelector:@selector(webView:didFinishNavigation:)]) [d webView:self didFinishNavigation:_nav];
        [self _mosDidFinish];
    }
    if (ev & MOS_WEB_EV_DIRTY) [self setNeedsDisplay:YES];
    free(I);
    [self release];
}

/* ---------------------------------------------------------------- carregar */
- (WKNavigation *)_mosBeginNav {
    [_pendingNav release];
    _pendingNav = [[WKNavigation alloc] init];
    return _pendingNav;
}
/* pergunta ao navigationDelegate (ou ao policyDelegate da WebView legada) sobre uma navegação que o framework faz
 * sozinho, sem o kernel: loadHTMLString/loadData e esquemas próprios; then roda com a decisão */
- (void)_mosAskPolicy:(NSString *)url kind:(int)kind then:(void (^)(BOOL))then {
    int seq = ++_reqSeq;
    void (^go)(BOOL) = [[then copy] autorelease];
    __block BOOL called = NO;
    void (^decide)(BOOL) = ^(BOOL allow) {
        if (called || seq != _reqSeq || _closed) return;
        called = YES;
        _reqSeq++;
        if (!allow) { [_pendingNav release]; _pendingNav = nil; }
        go(allow);
    };
    WKNavigationAction *act = [self _mosAction:url kind:kind];
    id d = _navigationDelegate;
    if ([d respondsToSelector:@selector(webView:decidePolicyForNavigationAction:preferences:decisionHandler:)])
        [d webView:self decidePolicyForNavigationAction:act preferences:[[_config.defaultWebpagePreferences copy] autorelease]
            decisionHandler:^(WKNavigationActionPolicy p, WKWebpagePreferences *pref) { decide(p == WKNavigationActionPolicyAllow); }];
    else if ([d respondsToSelector:@selector(webView:decidePolicyForNavigationAction:decisionHandler:)])
        [d webView:self decidePolicyForNavigationAction:act decisionHandler:^(WKNavigationActionPolicy p) { decide(p == WKNavigationActionPolicyAllow); }];
    else decide(YES);
}

- (WKNavigation *)loadRequest:(NSURLRequest *)request {
    NSURL *u = [request URL];
    NSString *s = [u absoluteString];
    if (!s) return nil;
    WKNavigation *n = [self _mosBeginNav];
    id sh = [_config urlSchemeHandlerForURLScheme:[u scheme]];
    if (sh) {
        /* esquema próprio: o pedido também passa pela política (WKNavigationTypeOther) */
        NSURLRequest *rq = [[request copy] autorelease];
        [self _mosAskPolicy:s kind:WKNavigationTypeOther then:^(BOOL allow) { if (allow) [self _mosStartSchemeTask:sh request:rq]; }];
        return n;
    }
    mos_web_load_t L;
    memset(&L, 0, sizeof L);
    const char *us = [s UTF8String];
    L.url = (unsigned long long)(uintptr_t)us;
    NSData *body = [request HTTPBody];
    NSString *method = [request HTTPMethod];
    if (body && [[method uppercaseString] isEqualToString:@"POST"]) {
        L.post = (unsigned long long)(uintptr_t)[body bytes];
        L.post_len = [body length];
        NSString *ct = [request valueForHTTPHeaderField:@"Content-Type"];
        if (ct) strncpy(L.content_type, [ct UTF8String], sizeof L.content_type - 1);
    }
    wk_sys(MOS_WEB_LOAD, _wid, (long)&L, 0);
    return n;
}
/* conteúdo direto (loadHTMLString/loadData): pergunta à política como o WebKit e depois entrega ao motor */
- (WKNavigation *)_mosLoadContent:(NSData *)data mime:(NSString *)mime base:(NSURL *)base {
    WKNavigation *n = [self _mosBeginNav];
    NSString *url = [base absoluteString] ?: @"about:blank";
    NSData *keep = [[data copy] autorelease];
    NSURL *b = [[base copy] autorelease];
    NSString *m = [[mime copy] autorelease];
    [self _mosAskPolicy:url kind:WKNavigationTypeOther then:^(BOOL allow) {
        if (!allow) return;
        WKNavigation *cur = [_pendingNav retain];
        [self _mosLoadBytes:[keep bytes] length:[keep length] mime:m base:b];
        if (cur) { [_pendingNav release]; _pendingNav = cur; }
    }];
    return n;
}
- (WKNavigation *)_mosLoadBytes:(const void *)bytes length:(size_t)len mime:(NSString *)mime base:(NSURL *)base {
    if (!_pendingNav) [self _mosBeginNav];
    WKNavigation *n = _pendingNav;
    mos_web_load_t L;
    memset(&L, 0, sizeof L);
    static const char empty[1] = {0};
    L.html = (unsigned long long)(uintptr_t)(bytes ? bytes : empty);
    L.html_len = len;
    NSString *bs = [base absoluteString];
    L.base = bs ? (unsigned long long)(uintptr_t)[bs UTF8String] : 0;
    if (mime) strncpy(L.mime, [mime UTF8String], sizeof L.mime - 1);
    wk_sys(MOS_WEB_LOAD, _wid, (long)&L, 0);
    return n;
}
- (WKNavigation *)loadHTMLString:(NSString *)html baseURL:(NSURL *)baseURL {
    return [self _mosLoadContent:[html ?: @"" dataUsingEncoding:NSUTF8StringEncoding] mime:@"text/html;charset=utf-8" base:baseURL];
}
- (WKNavigation *)loadData:(NSData *)data MIMEType:(NSString *)mime characterEncodingName:(NSString *)enc baseURL:(NSURL *)baseURL {
    NSString *m = mime ?: @"text/html";
    if (enc && [enc length]) m = [NSString stringWithFormat:@"%@;charset=%@", m, enc];
    return [self _mosLoadContent:data ?: [NSData data] mime:m base:baseURL];
}
- (WKNavigation *)loadFileURL:(NSURL *)url allowingReadAccessToURL:(NSURL *)dir { return [self loadRequest:[NSURLRequest requestWithURL:url]]; }
- (WKNavigation *)loadFileRequest:(NSURLRequest *)r allowingReadAccessToURL:(NSURL *)dir { return [self loadRequest:r]; }
- (WKNavigation *)loadSimulatedRequest:(NSURLRequest *)r responseHTMLString:(NSString *)html { return [self loadHTMLString:html baseURL:[r URL]]; }
- (WKNavigation *)loadSimulatedRequest:(NSURLRequest *)r response:(NSURLResponse *)resp responseData:(NSData *)data {
    return [self loadData:data MIMEType:[resp MIMEType] characterEncodingName:[resp textEncodingName] baseURL:[r URL]];
}
- (WKNavigation *)reload { WKNavigation *n = [self _mosBeginNav]; wk_sys(MOS_WEB_GO, _wid, MOS_WEB_GO_RELOAD, 0); return n; }
- (WKNavigation *)reloadFromOrigin { return [self reload]; }
- (void)stopLoading { wk_sys(MOS_WEB_GO, _wid, MOS_WEB_GO_STOP, 0); for (WKMOSSchemeTask *t in _schemeTasks) [self _mosStopTask:t]; }
- (WKNavigation *)goBack { if (!_canBack) return nil; WKNavigation *n = [self _mosBeginNav]; wk_sys(MOS_WEB_GO, _wid, MOS_WEB_GO_BACK, 0); return n; }
- (WKNavigation *)goForward { if (!_canForward) return nil; WKNavigation *n = [self _mosBeginNav]; wk_sys(MOS_WEB_GO, _wid, MOS_WEB_GO_FORWARD, 0); return n; }
- (void)_mosGoRelative:(int)rel {
    for (; rel < 0; rel++) wk_sys(MOS_WEB_GO, _wid, MOS_WEB_GO_BACK, 0);
    for (; rel > 0; rel--) wk_sys(MOS_WEB_GO, _wid, MOS_WEB_GO_FORWARD, 0);
}
- (WKNavigation *)goToBackForwardListItem:(WKBackForwardListItem *)item {
    if (!item || !item->_rel) return nil;
    int rel = item->_rel;
    WKNavigation *n = [self _mosBeginNav];
    id d = _navigationDelegate;
    if ([d respondsToSelector:@selector(webView:shouldGoToBackForwardListItem:willUseInstantBack:completionHandler:)]) {
        __block BOOL called = NO;
        /* instant back não existe neste motor: o terceiro argumento é NO */
        [d webView:self shouldGoToBackForwardListItem:item willUseInstantBack:NO completionHandler:^(BOOL allow) {
            if (called || _closed) return;
            called = YES;
            if (!allow) { [_pendingNav autorelease]; _pendingNav = nil; return; }
            [self _mosGoRelative:rel];
        }];
        return n;
    }
    [self _mosGoRelative:rel];
    return n;
}
- (IBAction)goBack:(id)s { [self goBack]; }
- (IBAction)goForward:(id)s { [self goForward]; }
- (IBAction)reload:(id)s { [self reload]; }
- (IBAction)reloadFromOrigin:(id)s { [self reload]; }
- (IBAction)stopLoading:(id)s { [self stopLoading]; }

/* ---------------------------------------------------------------- estado */
- (NSString *)title { return _title; }
- (NSURL *)URL { return _URL; }
- (BOOL)isLoading { return _loading; }
- (double)estimatedProgress { return _progress; }
- (BOOL)canGoBack { return _canBack; }
- (BOOL)canGoForward { return _canForward; }
- (BOOL)hasOnlySecureContent { return _secure; }
- (void *)serverTrust { return NULL; }
- (WKBackForwardList *)backForwardList { return _bfl; }
- (NSColor *)themeColor { return nil; }
- (NSInteger)cameraCaptureState { return 0; }
- (NSInteger)microphoneCaptureState { return 0; }
- (BOOL)isWritingToolsActive { return NO; }
- (double)pageZoom { return _pageZoom; }
- (void)setPageZoom:(double)z { if (z <= 0) return; _pageZoom = z; wk_sys(MOS_WEB_ZOOM, _wid, (long)(z * _magnification * 1000), 0); [self setNeedsDisplay:YES]; }
- (double)magnification { return _magnification; }
- (void)setMagnification:(double)m { if (m <= 0) return; _magnification = m; wk_sys(MOS_WEB_ZOOM, _wid, (long)(m * _pageZoom * 1000), 0); [self setNeedsDisplay:YES]; }
- (void)setMagnification:(double)m centeredAtPoint:(NSPoint)p { [self setMagnification:m]; }
- (BOOL)drawsBackground { return _drawsBackground; }
- (void)setDrawsBackground:(BOOL)b { _drawsBackground = b; [self _mosApplyOptions]; [self setNeedsDisplay:YES]; }
- (void)_setDrawsBackground:(BOOL)b { [self setDrawsBackground:b]; }
- (void)setValue:(id)v forUndefinedKey:(NSString *)k {}
- (id)valueForUndefinedKey:(NSString *)k { return nil; }
- (NSString *)_mosUserAgent { return _customUserAgent ?: @"Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko)"; }

/* ---------------------------------------------------------------- JavaScript */
- (void)_mosEvalRaw:(NSString *)js {
    const char *s = [js UTF8String];
    char tmp[256];
    mos_web_eval_t E = {(unsigned long long)(uintptr_t)s, (unsigned long long)(uintptr_t)tmp, (unsigned)strlen(s), sizeof tmp, 0, 0};
    wk_sys(MOS_WEB_EVAL, _wid, (long)&E, 0);
    if (E.len > sizeof tmp) { char *big = malloc(E.len); if (big) { wk_sys(MOS_WEB_RESULT, _wid, (long)big, E.len); free(big); } }
    [self setNeedsDisplay:YES];
}

/* avalia agora e devolve (resultado, erro) já convertidos */
- (id)_mosEval:(NSString *)js error:(NSError **)err {
    if (err) *err = nil;
    const char *s = [js ?: @"" UTF8String];
    size_t cap = 64 * 1024;
    char *buf = malloc(cap);
    if (!buf) { if (err) *err = wk_error(WKErrorDomain, WKErrorUnknown, @"sem memória", nil); return nil; }
    mos_web_eval_t E = {(unsigned long long)(uintptr_t)s, (unsigned long long)(uintptr_t)buf, (unsigned)strlen(s), (unsigned)cap, 0, 0};
    long r = wk_sys(MOS_WEB_EVAL, _wid, (long)&E, 0);
    if (r < 0) { free(buf); if (err) *err = wk_error(WKErrorDomain, WKErrorWebViewInvalidated, @"a vista não existe mais", nil); return nil; }
    if (E.len > cap) {
        char *nb = realloc(buf, E.len);
        if (!nb) { free(buf); return nil; }
        buf = nb;
        wk_sys(MOS_WEB_RESULT, _wid, (long)buf, E.len);
    }
    id v = nil;
    if (E.type == MOS_WEB_T_EXCEPTION) {
        NSString *m = wk_str(buf, E.len);
        if (err) *err = wk_error(WKErrorDomain, WKErrorJavaScriptExceptionOccurred, @"A JavaScript exception occurred",
                                 [NSDictionary dictionaryWithObjectsAndKeys:m, WKJavaScriptExceptionMessageErrorKey,
                                  [NSNumber numberWithInt:0], WKJavaScriptExceptionLineNumberErrorKey,
                                  [_URL absoluteString] ?: @"", WKJavaScriptExceptionSourceURLErrorKey, nil]);
    } else {
        BOOL bad = NO;
        v = wk_value(E.type, buf, E.len, &bad);
        if (bad && err) *err = wk_error(WKErrorDomain, WKErrorJavaScriptResultTypeIsUnsupported, @"JavaScript execution returned a result of an unsupported type", nil);
    }
    free(buf);
    [self setNeedsDisplay:YES];
    return v;
}

- (void)evaluateJavaScript:(NSString *)js completionHandler:(void (^)(id, NSError *))handler {
    NSError *e = nil;
    id v = [self _mosEval:js error:&e];
    if (!handler) return;
    [v retain];
    [e retain];
    [self _mosDefer:^{ handler(v, e); [v release]; [e release]; }];
}
- (void)evaluateJavaScript:(NSString *)js inFrame:(WKFrameInfo *)f inContentWorld:(WKContentWorld *)w completionHandler:(void (^)(id, NSError *))h {
    [self evaluateJavaScript:js completionHandler:h];
}
- (void)callAsyncJavaScript:(NSString *)body arguments:(NSDictionary *)args inFrame:(WKFrameInfo *)f inContentWorld:(WKContentWorld *)w
          completionHandler:(void (^)(id, NSError *))handler {
    NSMutableString *names = [NSMutableString string], *vals = [NSMutableString string];
    for (NSString *k in args) {
        if ([names length]) { [names appendString:@","]; [vals appendString:@","]; }
        [names appendString:k];
        [vals appendString:wk_js_literal([args objectForKey:k])];
    }
    int aid = ++_asyncSeq;
    if (handler) [_asyncHandlers setObject:[[handler copy] autorelease] forKey:[NSNumber numberWithInt:aid]];
    NSString *js = [NSString stringWithFormat:
        @"(function(){var h=window.webkit.messageHandlers.__mosAsync;try{Promise.resolve((async function(%@){%@\n})(%@)).then("
         "function(v){var t=typeof v;h.postMessage({id:%d,ok:true,undef:t==='undefined',v:(t==='function'||t==='symbol')?null:v,bad:(t==='function'||t==='symbol')});},"
         "function(e){h.postMessage({id:%d,ok:false,e:String(e&&e.message||e)});});}catch(e){h.postMessage({id:%d,ok:false,e:String(e&&e.message||e)});}})()",
        names, body ?: @"", vals, aid, aid, aid];
    NSError *e = nil;
    [self _mosEval:js error:&e];
    if (e) {
        void (^hb)(id, NSError *) = [_asyncHandlers objectForKey:[NSNumber numberWithInt:aid]];
        if (hb) { [[hb retain] autorelease]; [_asyncHandlers removeObjectForKey:[NSNumber numberWithInt:aid]]; [e retain]; [self _mosDefer:^{ hb(nil, e); [e release]; }]; }
    }
}
- (void)_mosAsyncDone:(id)body {
    if (![body isKindOfClass:[NSDictionary class]]) return;
    NSNumber *aid = [body objectForKey:@"id"];
    void (^h)(id, NSError *) = aid ? [_asyncHandlers objectForKey:aid] : nil;
    if (!h) return;
    [[h retain] autorelease];
    [_asyncHandlers removeObjectForKey:aid];
    if ([[body objectForKey:@"ok"] boolValue]) {
        if ([[body objectForKey:@"bad"] boolValue]) h(nil, wk_error(WKErrorDomain, WKErrorJavaScriptResultTypeIsUnsupported, @"JavaScript execution returned a result of an unsupported type", nil));
        else if ([[body objectForKey:@"undef"] boolValue]) h(nil, nil);
        else h([body objectForKey:@"v"], nil);
    } else {
        NSString *m = [body objectForKey:@"e"];
        h(nil, wk_error(WKErrorDomain, WKErrorJavaScriptExceptionOccurred, @"A JavaScript exception occurred",
                        [NSDictionary dictionaryWithObject:m ?: @"" forKey:WKJavaScriptExceptionMessageErrorKey]));
    }
}

/* ---------------------------------------------------------------- busca, texto, imagem */
- (void)findString:(NSString *)s withConfiguration:(WKFindConfiguration *)c completionHandler:(void (^)(WKFindResult *))h {
    WKFindResult *r = [[[WKFindResult alloc] init] autorelease];
    r->_found = s && wk_sys(MOS_WEB_FIND, _wid, (long)[s UTF8String], 0) == 1;
    [self setNeedsDisplay:YES];
    if (h) { [r retain]; [self _mosDefer:^{ h(r); [r release]; }]; }
}
- (NSString *)_mosPageText {
    long n = wk_sys(MOS_WEB_TEXT, _wid, 0, 0);
    if (n <= 0) return @"";
    char *b = malloc((size_t)n);
    if (!b) return @"";
    long m = wk_sys(MOS_WEB_TEXT, _wid, (long)b, n);
    NSString *s = m > 0 ? wk_str(b, (size_t)(m < n ? m : n)) : @"";
    free(b);
    return s;
}
- (void)takeSnapshotWithConfiguration:(WKSnapshotConfiguration *)cfg completionHandler:(void (^)(NSImage *, NSError *))h {
    if (!h) return;
    NSImage *img = nil;
    if ([self _mosPaint]) {
        size_t n = (size_t)_sw * _sh * 4;
        void *copy = malloc(n);
        if (copy) {
            memcpy(copy, _buf, n);
            void *prov = CGDataProviderCreateWithData(NULL, copy, n, NULL);
            void *cs = CGColorSpaceCreateDeviceRGB();
            /* kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little: BGRA na memória, como o buffer do motor */
            void *cg = CGImageCreate((size_t)_sw, (size_t)_sh, 8, 32, (size_t)_sw * 4, cs, 2 | (2 << 12), prov, NULL, false, 0);
            CGColorSpaceRelease(cs);
            CGDataProviderRelease(prov);
            if (cg) {
                Class ic = NSClassFromString(@"NSImage");
                id im = [ic alloc];
                if ([im respondsToSelector:@selector(initWithCGImage:size:)])
                    img = [[im initWithCGImage:cg size:NSMakeSize(_sw, _sh)] autorelease];
                else [im release];
            }
        }
    }
    NSError *e = img ? nil : wk_error(WKErrorDomain, WKErrorUnknown, @"O instantâneo não pôde ser criado", nil);
    [img retain];
    [e retain];
    [self _mosDefer:^{ h(img, e); [img release]; [e release]; }];
}
- (void)createPDFWithConfiguration:(WKPDFConfiguration *)c completionHandler:(void (^)(NSData *, NSError *))h {
    if (h) [self _mosDefer:^{ h(nil, wk_error(WKErrorDomain, WKErrorUnknown, @"PDF não é suportado pelo motor web do mOS", nil)); }];
}
- (void)createWebArchiveDataWithCompletionHandler:(void (^)(NSData *, NSError *))h {
    if (h) [self _mosDefer:^{ h(nil, wk_error(WKErrorDomain, WKErrorUnknown, @"arquivo web não é suportado pelo motor web do mOS", nil)); }];
}
- (id)printOperationWithPrintInfo:(id)info { return nil; }
- (void)pauseAllMediaPlaybackWithCompletionHandler:(void (^)(void))h { if (h) [self _mosDefer:h]; }
- (void)closeAllMediaPresentationsWithCompletionHandler:(void (^)(void))h { if (h) [self _mosDefer:h]; }
- (void)closeAllMediaPresentations {}
- (void)setAllMediaPlaybackSuspended:(BOOL)s completionHandler:(void (^)(void))h { if (h) [self _mosDefer:h]; }
- (void)requestMediaPlaybackStateWithCompletionHandler:(void (^)(NSInteger))h { if (h) [self _mosDefer:^{ h(0); }]; }
- (void)setCameraCaptureState:(NSInteger)s completionHandler:(void (^)(void))h { if (h) [self _mosDefer:h]; }
- (void)setMicrophoneCaptureState:(NSInteger)s completionHandler:(void (^)(void))h { if (h) [self _mosDefer:h]; }
- (void)startDownloadUsingRequest:(NSURLRequest *)r completionHandler:(void (^)(id))h { if (h) [self _mosDefer:^{ h(nil); }]; }
- (void)resumeDownloadFromResumeData:(NSData *)d completionHandler:(void (^)(id))h { if (h) [self _mosDefer:^{ h(nil); }]; }

/* ---------------------------------------------------------------- esquemas próprios */
- (void)_mosStartSchemeTask:(id)handler request:(NSURLRequest *)rq {
    WKMOSSchemeTask *t = [[WKMOSSchemeTask alloc] init];
    t->_req = [rq copy];
    t->_data = [[NSMutableData alloc] init];
    t->_wv = self;
    t->_handler = [handler retain];
    [_schemeTasks addObject:t];
    [t release];
    if (!_pendingNav) [self _mosBeginNav];
    [handler webView:self startURLSchemeTask:t];
}
- (void)_mosStopTask:(WKMOSSchemeTask *)t {
    if (t->_done || t->_stopped) return;
    t->_stopped = YES;
    if ([t->_handler respondsToSelector:@selector(webView:stopURLSchemeTask:)]) [t->_handler webView:self stopURLSchemeTask:t];
}
- (void)_mosSchemeTaskDone:(WKMOSSchemeTask *)t error:(NSError *)err {
    [[t retain] autorelease];
    [_schemeTasks removeObject:t];
    if (t->_stopped) return;
    if (err) {
        id d = _navigationDelegate;
        WKNavigation *n = _pendingNav ? [[_pendingNav retain] autorelease] : [[[WKNavigation alloc] init] autorelease];
        [_pendingNav release];
        _pendingNav = nil;
        if ([d respondsToSelector:@selector(webView:didFailProvisionalNavigation:withError:)]) [d webView:self didFailProvisionalNavigation:n withError:err];
        [self _mosDidFail:err provisional:YES];
        return;
    }
    NSString *mime = [t->_resp MIMEType] ?: @"text/html";
    WKNavigation *keep = [_pendingNav retain];
    [self _mosLoadBytes:[t->_data bytes] length:[t->_data length] mime:mime base:[t->_req URL]];
    if (keep) { [_pendingNav release]; _pendingNav = keep; }
}

/* ---------------------------------------------------------------- outros */
- (int)_mosEngineID { return _wid; }   /* testes (userland/tests-webkit): a vista do motor, para MOS_WEB_INPUT/PAINT diretos */
- (BOOL)performKeyEquivalent:(NSEvent *)e { return NO; }
- (void)viewWillMoveToWindow:(NSWindow *)w {}
- (void)_mosClose {
    _closed = YES;
    [_timer invalidate];
}
- (void)removeFromSuperview { [super removeFromSuperview]; }
@end

@implementation WKTimerTarget
- (void)fire:(NSTimer *)t { if (wv) [wv _mosTick]; else [t invalidate]; }
@end

@implementation WKMOSSchemeTask
- (NSURLRequest *)request { return _req; }
- (void)didReceiveResponse:(NSURLResponse *)r {
    if (_done) [NSException raise:@"NSInternalInconsistencyException" format:@"This task has already been stopped"];
    [_resp release];
    _resp = [r retain];
}
- (void)didReceiveData:(NSData *)d {
    if (_done) [NSException raise:@"NSInternalInconsistencyException" format:@"This task has already been stopped"];
    if (d) [_data appendData:d];
}
- (void)didFinish {
    if (_done) return;
    _done = YES;
    [_wv _mosSchemeTaskDone:self error:nil];
}
- (void)didFailWithError:(NSError *)e {
    if (_done) return;
    _done = YES;
    [_wv _mosSchemeTaskDone:self error:e ?: wk_error(NSURLErrorDomainWK, -1, @"falha no esquema", nil)];
}
- (void)dealloc { [_req release]; [_resp release]; [_data release]; [_handler release]; [super dealloc]; }
@end

/* ================================================================ WKContentRuleListStore */
@interface WKContentRuleListStore : NSObject
+ (instancetype)defaultStore;
+ (instancetype)storeWithURL:(NSURL *)u;
@end
@implementation WKContentRuleListStore {
    NSMutableDictionary *_lists;
}
+ (instancetype)defaultStore { static WKContentRuleListStore *s; if (!s) { s = [[self alloc] init]; s->_lists = [[NSMutableDictionary alloc] init]; } return s; }
+ (instancetype)storeWithURL:(NSURL *)u { WKContentRuleListStore *s = [[[self alloc] init] autorelease]; s->_lists = [[NSMutableDictionary alloc] init]; return s; }
- (void)compileContentRuleListForIdentifier:(NSString *)i encodedContentRuleList:(NSString *)l completionHandler:(void (^)(WKContentRuleList *, NSError *))h {
    /* as regras são validadas como JSON e guardadas; o motor do mOS não bloqueia conteúdo por elas */
    id parsed = l ? [NSJSONSerialization JSONObjectWithData:[l dataUsingEncoding:NSUTF8StringEncoding] options:0 error:NULL] : nil;
    if (![parsed isKindOfClass:[NSArray class]]) {
        if (h) h(nil, wk_error(WKErrorDomain, WKErrorContentRuleListStoreCompileFailed, @"Rule list compilation failed", nil));
        return;
    }
    WKContentRuleList *r = [[[WKContentRuleList alloc] init] autorelease];
    r->_ident = [i copy];
    if (i) [_lists setObject:r forKey:i];
    if (h) h(r, nil);
}
- (void)lookUpContentRuleListForIdentifier:(NSString *)i completionHandler:(void (^)(WKContentRuleList *, NSError *))h {
    WKContentRuleList *r = i ? [_lists objectForKey:i] : nil;
    if (h) h(r, r ? nil : wk_error(WKErrorDomain, WKErrorContentRuleListStoreLookUpFailed, @"Rule list lookup failed", nil));
}
- (void)removeContentRuleListForIdentifier:(NSString *)i completionHandler:(void (^)(NSError *))h {
    BOOL had = i && [_lists objectForKey:i];
    if (i) [_lists removeObjectForKey:i];
    if (h) h(had ? nil : wk_error(WKErrorDomain, WKErrorContentRuleListStoreRemoveFailed, @"Rule list removal failed", nil));
}
- (void)getAvailableContentRuleListIdentifiers:(void (^)(NSArray *))h { if (h) h([_lists allKeys]); }
- (void)dealloc { [_lists release]; [super dealloc]; }
@end

/* ================================================================ WebArchive / WebResource (WebKit1)
 * Plist da Apple, o que o CUALockScreenGuardian do ChatGPT/Codex liga forte:
 *   WebMainResource, WebSubresources, WebSubframeArchives
 *   WebResourceData, WebResourceURL, WebResourceMIMEType, WebResourceTextEncodingName, WebResourceFrameName
 * `data` grava XML (NSPropertyListXMLFormat_v1_0 = 100). `initWithData:` aceita XML e bplist. */
static NSString *wa_str(id v) { return [v isKindOfClass:[NSString class]] ? v : nil; }
static NSData *wa_bytes(id v) { return [v isKindOfClass:[NSData class]] ? v : nil; }

@interface WebResource : NSObject
- (instancetype)initWithData:(NSData *)data URL:(NSURL *)url MIMEType:(NSString *)mime textEncodingName:(NSString *)enc frameName:(NSString *)frame;
- (NSData *)data;
- (NSURL *)URL;
- (NSString *)MIMEType;
- (NSString *)textEncodingName;
- (NSString *)frameName;
- (NSDictionary *)_mosPlist;
@end
@interface WebArchive : NSObject
- (instancetype)initWithMainResource:(WebResource *)main subresources:(NSArray *)subs subframeArchives:(NSArray *)frames;
- (instancetype)initWithData:(NSData *)data;
- (NSData *)data;
- (WebResource *)mainResource;
- (NSArray *)subresources;
- (NSArray *)subframeArchives;
@end

@implementation WebResource {
    NSData *_data;
    NSURL *_url;
    NSString *_mime, *_enc, *_frame;
}
- (instancetype)initWithData:(NSData *)data URL:(NSURL *)url MIMEType:(NSString *)mime textEncodingName:(NSString *)enc frameName:(NSString *)frame {
    if ((self = [super init])) {
        _data = [data copy];
        _url = [url copy];
        _mime = [mime copy];
        _enc = [enc copy];
        _frame = [frame copy];
    }
    return self;
}
- (NSData *)data { return _data; }
- (NSURL *)URL { return _url; }
- (NSString *)MIMEType { return _mime; }
- (NSString *)textEncodingName { return _enc; }
- (NSString *)frameName { return _frame; }
- (NSDictionary *)_mosPlist {
    NSMutableDictionary *d = [NSMutableDictionary dictionary];
    if (_data) [d setObject:_data forKey:@"WebResourceData"];
    if (_url) [d setObject:[_url absoluteString] ?: @"" forKey:@"WebResourceURL"];
    if (_mime) [d setObject:_mime forKey:@"WebResourceMIMEType"];
    if (_enc) [d setObject:_enc forKey:@"WebResourceTextEncodingName"];
    if (_frame) [d setObject:_frame forKey:@"WebResourceFrameName"];
    return d;
}
+ (WebResource *)_mosFromPlist:(id)pl {
    if (![pl isKindOfClass:[NSDictionary class]]) return nil;
    NSString *us = wa_str([pl objectForKey:@"WebResourceURL"]);
    return [[[self alloc] initWithData:wa_bytes([pl objectForKey:@"WebResourceData"])
                                    URL:us ? [NSURL URLWithString:us] : nil
                               MIMEType:wa_str([pl objectForKey:@"WebResourceMIMEType"])
                       textEncodingName:wa_str([pl objectForKey:@"WebResourceTextEncodingName"])
                              frameName:wa_str([pl objectForKey:@"WebResourceFrameName"])] autorelease];
}
- (void)dealloc { [_data release]; [_url release]; [_mime release]; [_enc release]; [_frame release]; [super dealloc]; }
@end

@implementation WebArchive {
    WebResource *_main;
    NSArray *_subs, *_frames;
}
+ (NSArray *)_mosResources:(id)a {
    NSMutableArray *o = [NSMutableArray array];
    if ([a isKindOfClass:[NSArray class]]) for (id x in a) { WebResource *r = [WebResource _mosFromPlist:x]; if (r) [o addObject:r]; }
    return o;
}
+ (NSArray *)_mosArchives:(id)a {
    NSMutableArray *o = [NSMutableArray array];
    if ([a isKindOfClass:[NSArray class]]) for (id x in a) {
        WebArchive *sub = nil;
        if ([x isKindOfClass:[NSDictionary class]]) {
            sub = [[self alloc] init];
            sub = [sub _mosInitWithPlist:x];
        }
        if (sub) [o addObject:[sub autorelease]];
    }
    return o;
}
/* self já passou por init. Não chama [super init] de novo. */
- (instancetype)_mosInitWithPlist:(NSDictionary *)pl {
    if (![pl isKindOfClass:[NSDictionary class]]) { [self release]; return nil; }
    _main = [[WebResource _mosFromPlist:[pl objectForKey:@"WebMainResource"]] retain];
    _subs = [[[self class] _mosResources:[pl objectForKey:@"WebSubresources"]] retain];
    _frames = [[[self class] _mosArchives:[pl objectForKey:@"WebSubframeArchives"]] retain];
    return self;
}
- (instancetype)initWithMainResource:(WebResource *)main subresources:(NSArray *)subs subframeArchives:(NSArray *)frames {
    if ((self = [super init])) {
        _main = [main retain];
        _subs = [[WebArchive _mosKeep:subs class:[WebResource class]] retain];
        _frames = [[WebArchive _mosKeep:frames class:[WebArchive class]] retain];
    }
    return self;
}
+ (NSArray *)_mosKeep:(NSArray *)a class:(Class)c {
    NSMutableArray *o = [NSMutableArray array];
    if (c && [a isKindOfClass:[NSArray class]]) for (id x in a) if ([x isKindOfClass:c]) [o addObject:x];
    return o;
}
- (instancetype)initWithData:(NSData *)data {
    if (!(self = [super init])) return nil;
    id pl = data ? [NSPropertyListSerialization propertyListWithData:data options:0 format:NULL error:NULL] : nil;
    if (![pl isKindOfClass:[NSDictionary class]]) { [self release]; return nil; }
    return [self _mosInitWithPlist:pl];
}
- (NSDictionary *)_mosPlist {
    NSMutableDictionary *d = [NSMutableDictionary dictionary];
    if (_main) [d setObject:[_main _mosPlist] forKey:@"WebMainResource"];
    NSMutableArray *subs = [NSMutableArray array];
    for (WebResource *r in _subs) [subs addObject:[r _mosPlist]];
    if ([subs count]) [d setObject:subs forKey:@"WebSubresources"];
    NSMutableArray *frames = [NSMutableArray array];
    for (WebArchive *a in _frames) [frames addObject:[a _mosPlist]];
    if ([frames count]) [d setObject:frames forKey:@"WebSubframeArchives"];
    return d;
}
- (NSData *)data { return [NSPropertyListSerialization dataWithPropertyList:[self _mosPlist] format:100 options:0 error:NULL]; }
- (WebResource *)mainResource { return _main; }
- (NSArray *)subresources { return _subs ?: [NSArray array]; }
- (NSArray *)subframeArchives { return _frames ?: [NSArray array]; }
- (void)dealloc { [_main release]; [_subs release]; [_frames release]; [super dealloc]; }
@end

/* ================================================================ WebKit1 (WebView legada) sobre o WKWebView */
@interface WebPreferences : NSObject
+ (instancetype)standardPreferences;
@property BOOL javaScriptEnabled;
@property BOOL javaScriptCanOpenWindowsAutomatically;
@property BOOL plugInsEnabled;
@property BOOL javaEnabled;
@property BOOL loadsImagesAutomatically;
@property BOOL autosaves;
@property BOOL privateBrowsingEnabled;
@property int defaultFontSize;
@property (copy) NSString *standardFontFamily;
@property (copy) NSString *identifier;
@end
@implementation WebPreferences
+ (instancetype)standardPreferences {
    static WebPreferences *s;
    if (!s) { s = [[self alloc] init]; s.javaScriptEnabled = YES; s.loadsImagesAutomatically = YES; s.defaultFontSize = 16; }
    return s;
}
- (instancetype)initWithIdentifier:(NSString *)i { if ((self = [self init])) { self.identifier = i; _javaScriptEnabled = YES; _loadsImagesAutomatically = YES; } return self; }
- (BOOL)isJavaScriptEnabled { return _javaScriptEnabled; }
- (BOOL)arePlugInsEnabled { return _plugInsEnabled; }
- (BOOL)isJavaEnabled { return _javaEnabled; }
- (void)setValue:(id)v forUndefinedKey:(NSString *)k {}
- (id)valueForUndefinedKey:(NSString *)k { return nil; }
- (void)dealloc { [_standardFontFamily release]; [_identifier release]; [super dealloc]; }
@end

@class WebView;
@interface WebDataSource : NSObject
- (NSURLRequest *)request;
- (NSURLRequest *)initialRequest;
- (NSURLResponse *)response;
- (NSString *)pageTitle;
- (BOOL)isLoading;
@end
@interface WebFrame : NSObject
- (WebView *)webView;
- (NSString *)name;
- (void)loadRequest:(NSURLRequest *)r;
- (void)loadHTMLString:(NSString *)s baseURL:(NSURL *)u;
- (void)loadData:(NSData *)d MIMEType:(NSString *)m textEncodingName:(NSString *)e baseURL:(NSURL *)u;
- (void)stopLoading;
- (void)reload;
- (WebDataSource *)dataSource;
- (WebDataSource *)provisionalDataSource;
- (WebFrame *)parentFrame;
- (NSArray *)childFrames;
- (id)DOMDocument;
- (id)frameView;
- (id)windowObject;
@end

@interface WebView : WKWebView
@property (retain) WebPreferences *preferences;
@property (assign) id frameLoadDelegate;
@property (assign) id policyDelegate;
@property (assign) id resourceLoadDelegate;
@property (assign) id downloadDelegate;
@property (assign) id editingDelegate;
@property (assign) id UIDelegate_legacy;
@property (copy) NSString *applicationNameForUserAgent;
@property (copy) NSString *groupName;
@property BOOL shouldUpdateWhileOffscreen;
@property BOOL maintainsBackForwardList;
@property float textSizeMultiplier;
@property BOOL drawsBackgroundLegacy;
@property (copy) NSString *customTextEncodingName;
@end

@implementation WebDataSource {
@public
    WebView *_wv;
}
- (NSURLRequest *)request { NSURL *u = [_wv URL]; return u ? [NSURLRequest requestWithURL:u] : nil; }
- (NSURLRequest *)initialRequest { return [self request]; }
- (NSURLResponse *)response {
    NSURL *u = [_wv URL];
    return u ? [[[NSURLResponse alloc] initWithURL:u MIMEType:@"text/html" expectedContentLength:-1 textEncodingName:@"utf-8"] autorelease] : nil;
}
- (NSString *)pageTitle { return [_wv title]; }
- (BOOL)isLoading { return [_wv isLoading]; }
- (WebFrame *)webFrame { return [_wv mainFrame]; }
- (NSData *)data { return [[_wv _mosPageText] dataUsingEncoding:NSUTF8StringEncoding]; }
- (NSString *)textEncodingName { return @"utf-8"; }
@end

@implementation WebFrame {
@public
    WebView *_wv;
    WebDataSource *_ds;
}
- (WebView *)webView { return _wv; }
- (NSString *)name { return @""; }
- (void)loadRequest:(NSURLRequest *)r { [_wv loadRequest:r]; }
- (void)loadHTMLString:(NSString *)s baseURL:(NSURL *)u { [_wv loadHTMLString:s baseURL:u]; }
- (void)loadData:(NSData *)d MIMEType:(NSString *)m textEncodingName:(NSString *)e baseURL:(NSURL *)u { [_wv loadData:d MIMEType:m characterEncodingName:e baseURL:u]; }
- (void)loadData:(NSData *)d MIMEType:(NSString *)m textEncodingName:(NSString *)e baseURL:(NSURL *)u completionHandler:(void (^)(NSError *))h {
    [self loadData:d MIMEType:m textEncodingName:e baseURL:u];
    if (h) h(nil);
}
- (void)loadAlternateHTMLString:(NSString *)s baseURL:(NSURL *)u forUnreachableURL:(NSURL *)x { [_wv loadHTMLString:s baseURL:u]; }
- (void)stopLoading { [_wv stopLoading]; }
- (void)reload { [_wv reload]; }
- (void)reloadFromOrigin { [_wv reload]; }
- (WebDataSource *)dataSource { return [_wv URL] ? _ds : nil; }
- (WebDataSource *)provisionalDataSource { return [_wv isLoading] ? _ds : nil; }
- (WebFrame *)parentFrame { return nil; }
- (NSArray *)childFrames { return [NSArray array]; }
- (WebFrame *)findFrameNamed:(NSString *)n { return self; }
- (id)DOMDocument { return nil; }
- (id)frameView { return _wv; }
- (id)windowObject { return nil; }
- (id)globalContext { return NULL; }
- (void)dealloc { [_ds release]; [super dealloc]; }
@end

@implementation WebView {
    WebFrame *_frame;
}
+ (BOOL)canShowMIMEType:(NSString *)m {
    NSString *l = [m lowercaseString];
    return [l hasPrefix:@"text/"] || [l hasPrefix:@"image/"] || [l isEqualToString:@"application/xhtml+xml"] || [l isEqualToString:@"application/json"];
}
+ (BOOL)canShowMIMETypeAsHTML:(NSString *)m { return [[m lowercaseString] hasPrefix:@"text/html"]; }
+ (NSArray *)MIMETypesShownAsHTML { return [NSArray arrayWithObjects:@"text/html", @"application/xhtml+xml", nil]; }
+ (BOOL)handlesURLScheme:(NSString *)s { return [WKWebView handlesURLScheme:s]; }
- (instancetype)initWithFrame:(NSRect)frame frameName:(NSString *)fn groupName:(NSString *)gn {
    if ((self = [super initWithFrame:frame configuration:nil])) {
        _frame = [[WebFrame alloc] init];
        _frame->_wv = self;
        _frame->_ds = [[WebDataSource alloc] init];
        _frame->_ds->_wv = self;
        self.preferences = [WebPreferences standardPreferences];
        self.groupName = gn;
        self.maintainsBackForwardList = YES;
        self.textSizeMultiplier = 1;
    }
    return self;
}
- (instancetype)initWithFrame:(NSRect)frame { return [self initWithFrame:frame frameName:nil groupName:nil]; }
- (instancetype)initWithCoder:(NSCoder *)c { return [self initWithFrame:NSMakeRect(0, 0, 400, 300) frameName:nil groupName:nil]; }
- (void)dealloc {
    [_frame release];
    [_preferences release]; [_applicationNameForUserAgent release]; [_groupName release]; [_customTextEncodingName release];
    [super dealloc];
}
- (void)setPreferences:(WebPreferences *)p {
    [p retain];
    [_preferences release];
    _preferences = p;
    _config.preferences.javaScriptEnabled = p ? p.javaScriptEnabled : YES;
    [self _mosApplyOptions];
}
- (WebFrame *)mainFrame { return _frame; }
- (WebFrame *)selectedFrame { return _frame; }
- (NSString *)mainFrameURL { return [[self URL] absoluteString]; }
- (void)setMainFrameURL:(NSString *)s { NSURL *u = [NSURL URLWithString:s]; if (u) [self loadRequest:[NSURLRequest requestWithURL:u]]; }
- (NSString *)mainFrameTitle { return [self title] ?: @""; }
- (NSImage *)mainFrameIcon { return nil; }
- (double)estimatedProgress { return [super estimatedProgress]; }
- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)js {
    id v = [self _mosEval:js error:NULL];
    if (!v || v == (id)[NSNull null]) return @"";
    if ([v isKindOfClass:[NSString class]]) return v;
    if ([v isKindOfClass:[NSNumber class]]) return [v stringValue];
    return [v description];
}
- (id)windowScriptObject { return nil; }
- (BOOL)goBack { BOOL c = [self canGoBack]; [super goBack]; return c; }
- (BOOL)goForward { BOOL c = [self canGoForward]; [super goForward]; return c; }
- (IBAction)goBack:(id)s { [self goBack]; }
- (IBAction)goForward:(id)s { [self goForward]; }
- (IBAction)reload:(id)s { [super reload]; }
- (IBAction)stopLoading:(id)s { [super stopLoading]; }
- (IBAction)makeTextLarger:(id)s { self.textSizeMultiplier *= 1.2f; [self setPageZoom:self.textSizeMultiplier]; }
- (IBAction)makeTextSmaller:(id)s { self.textSizeMultiplier /= 1.2f; [self setPageZoom:self.textSizeMultiplier]; }
- (IBAction)makeTextStandardSize:(id)s { self.textSizeMultiplier = 1; [self setPageZoom:1]; }
- (BOOL)canMakeTextLarger { return YES; }
- (BOOL)canMakeTextSmaller { return YES; }
- (BOOL)canMakeTextStandardSize { return self.textSizeMultiplier != 1; }
- (BOOL)searchFor:(NSString *)s direction:(BOOL)fwd caseSensitive:(BOOL)cs wrap:(BOOL)w {
    BOOL ok = s && wk_sys(MOS_WEB_FIND, _wid, (long)[s UTF8String], 0) == 1;
    [self setNeedsDisplay:YES];
    return ok;
}
- (void)setCustomUserAgent:(NSString *)ua { [super setCustomUserAgent:ua]; }
- (NSString *)userAgentForURL:(NSURL *)u { return [self _mosUserAgent]; }
- (void)setUIDelegate:(id)d { [super setUIDelegate:d]; }
- (id)backForwardList { return [super backForwardList]; }
- (void)setMaintainsBackForwardList:(BOOL)b { _maintainsBackForwardList = b; }
- (BOOL)isLoading { return [super isLoading]; }
- (void)setDrawsBackground:(BOOL)b { [super setDrawsBackground:b]; }
- (BOOL)drawsBackground { return [super drawsBackground]; }
- (void)setHostWindow:(NSWindow *)w {}
- (NSWindow *)hostWindow { return [self window]; }
- (void)close { [self _mosClose]; }
- (void)setShouldCloseWithWindow:(BOOL)b {}

/* delegates do WebKit1, chamados pelos ganchos do WKWebView */
- (void)_mosDidStart {
    id d = _frameLoadDelegate;
    if ([d respondsToSelector:@selector(webView:didStartProvisionalLoadForFrame:)]) [d webView:self didStartProvisionalLoadForFrame:_frame];
}
- (void)_mosDidCommit {
    id d = _frameLoadDelegate;
    if ([d respondsToSelector:@selector(webView:didCommitLoadForFrame:)]) [d webView:self didCommitLoadForFrame:_frame];
}
- (void)_mosDidFinish {
    id d = _frameLoadDelegate;
    if ([d respondsToSelector:@selector(webView:didFinishLoadForFrame:)]) [d webView:self didFinishLoadForFrame:_frame];
}
- (void)_mosDidFail:(NSError *)e provisional:(BOOL)prov {
    id d = _frameLoadDelegate;
    if (prov && [d respondsToSelector:@selector(webView:didFailProvisionalLoadWithError:forFrame:)]) [d webView:self didFailProvisionalLoadWithError:e forFrame:_frame];
    else if (!prov && [d respondsToSelector:@selector(webView:didFailLoadWithError:forFrame:)]) [d webView:self didFailLoadWithError:e forFrame:_frame];
}
- (void)_mosTitleChanged {
    id d = _frameLoadDelegate;
    if ([d respondsToSelector:@selector(webView:didReceiveTitle:forFrame:)]) [d webView:self didReceiveTitle:[self title] ?: @"" forFrame:_frame];
}
@end

/* o ouvinte de decisão do WebKit1 */
@interface WebPolicyDecisionListenerMOS : NSObject {
@public
    void (^_decide)(BOOL);
}
- (void)use;
- (void)ignore;
- (void)download;
@end
@implementation WebPolicyDecisionListenerMOS
- (void)use { if (_decide) { _decide(YES); _Block_release(_decide); _decide = nil; } }
- (void)ignore { if (_decide) { _decide(NO); _Block_release(_decide); _decide = nil; } }
- (void)download { [self ignore]; }
- (void)dealloc { if (_decide) _Block_release(_decide); [super dealloc]; }
@end

/* o WebView decide pelo policyDelegate antigo quando não há navigationDelegate novo */
@implementation WKWebView (MOSLegacyPolicy)
- (void)_mosLegacyDecide:(WKNavigationAction *)act url:(NSString *)url seq:(int)seq {
    id pd = [self isKindOfClass:[WebView class]] ? [(WebView *)self policyDelegate] : nil;
    if (pd && [pd respondsToSelector:@selector(webView:decidePolicyForNavigationAction:request:frame:decisionListener:)]) {
        WebPolicyDecisionListenerMOS *l = [[[WebPolicyDecisionListenerMOS alloc] init] autorelease];
        l->_decide = _Block_copy(^(BOOL allow) { [self _mosDecide:allow seq:seq url:url]; });
        NSDictionary *info = [NSDictionary dictionaryWithObjectsAndKeys:[NSNumber numberWithInteger:act.navigationType], @"WebActionNavigationTypeKey",
                              [act.request URL] ?: (id)[NSNull null], @"WebActionOriginalURLKey", nil];
        [pd webView:(WebView *)self decidePolicyForNavigationAction:info request:act.request frame:[(WebView *)self mainFrame] decisionListener:l];
        return;
    }
    [self _mosDecide:YES seq:seq url:url];
}
@end
