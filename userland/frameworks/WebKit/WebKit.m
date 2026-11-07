/*
 * mOS — WebKit.framework (Mach-O, em /System/Library/Frameworks): por enquanto só as classes WKWebView e
 * WKWebViewConfiguration que binários da Apple referenciam (o Postico mostra ajuda/novidades num WKWebView). Sem
 * motor HTML: a view é uma NSView vazia e os carregamentos são ignorados, com aviso no stderr.
 * tools/build-frameworks-objc.ps1
 */
#import <AppKit/AppKit.h>

@interface WKPreferences : NSObject
@property BOOL javaScriptEnabled;
@property BOOL javaScriptCanOpenWindowsAutomatically;
@end
@implementation WKPreferences
@end

@interface WKUserContentController : NSObject
- (void)addScriptMessageHandler:(id)h name:(NSString *)n;
- (void)removeScriptMessageHandlerForName:(NSString *)n;
- (void)addUserScript:(id)s;
- (void)removeAllUserScripts;
@end
@implementation WKUserContentController
- (void)addScriptMessageHandler:(id)h name:(NSString *)n {}
- (void)removeScriptMessageHandlerForName:(NSString *)n {}
- (void)addUserScript:(id)s {}
- (void)removeAllUserScripts {}
@end

@interface WKWebViewConfiguration : NSObject <NSCopying>
@property (strong) WKPreferences *preferences;
@property (strong) WKUserContentController *userContentController;
@property (strong) id websiteDataStore;
@property (strong) id processPool;
@property BOOL suppressesIncrementalRendering;
@end
@implementation WKWebViewConfiguration
- (instancetype)init {
    if ((self = [super init])) {
        _preferences = [[WKPreferences alloc] init];
        _userContentController = [[WKUserContentController alloc] init];
    }
    return self;
}
- (id)copyWithZone:(void *)zone { return [self retain]; }
@end

@interface WKWebView : NSView
@property (readonly, copy) WKWebViewConfiguration *configuration;
@property (assign) id navigationDelegate;
@property (assign) id UIDelegate;
@property BOOL allowsBackForwardNavigationGestures;
@property BOOL allowsMagnification;
- (instancetype)initWithFrame:(NSRect)frame configuration:(WKWebViewConfiguration *)configuration;
- (id)loadRequest:(id)request;
- (id)loadHTMLString:(NSString *)html baseURL:(id)baseURL;
- (id)loadFileURL:(id)url allowingReadAccessToURL:(id)dir;
- (void)evaluateJavaScript:(NSString *)js completionHandler:(void (^)(id, id))handler;
- (id)URL;
- (NSString *)title;
- (BOOL)isLoading;
- (double)estimatedProgress;
- (void)stopLoading;
- (id)reload;
- (id)goBack;
- (id)goForward;
- (BOOL)canGoBack;
- (BOOL)canGoForward;
@end

static void wk_warn(const char *what) {
    static int warned;
    if (!warned++) fprintf(stderr, "WebKit[mOS]: sem motor HTML ainda; %s ignorado (a WKWebView fica vazia)\n", what);
}

@implementation WKWebView {
    WKWebViewConfiguration *_config;
}
- (instancetype)initWithFrame:(NSRect)frame configuration:(WKWebViewConfiguration *)configuration {
    if ((self = [super initWithFrame:frame])) _config = configuration ? [configuration retain] : [[WKWebViewConfiguration alloc] init];
    return self;
}
- (instancetype)initWithFrame:(NSRect)frame { return [self initWithFrame:frame configuration:nil]; }
- (WKWebViewConfiguration *)configuration { return _config; }
- (id)loadRequest:(id)request { wk_warn("loadRequest:"); return nil; }
- (id)loadHTMLString:(NSString *)html baseURL:(id)baseURL { wk_warn("loadHTMLString:baseURL:"); return nil; }
- (id)loadFileURL:(id)url allowingReadAccessToURL:(id)dir { wk_warn("loadFileURL:"); return nil; }
- (void)evaluateJavaScript:(NSString *)js completionHandler:(void (^)(id, id))handler { if (handler) handler(nil, nil); }
- (id)URL { return nil; }
- (NSString *)title { return nil; }
- (BOOL)isLoading { return NO; }
- (double)estimatedProgress { return 1.0; }
- (void)stopLoading {}
- (id)reload { return nil; }
- (id)goBack { return nil; }
- (id)goForward { return nil; }
- (BOOL)canGoBack { return NO; }
- (BOOL)canGoForward { return NO; }
@end

/* o que o Sparkle referencia (notas de versão numa WebView): classes reais, sem motor HTML como o WKWebView acima */
@interface WKUserScript : NSObject
- (instancetype)initWithSource:(NSString *)source injectionTime:(long)t forMainFrameOnly:(BOOL)main;
@end
@implementation WKUserScript
- (instancetype)initWithSource:(NSString *)source injectionTime:(long)t forMainFrameOnly:(BOOL)main { return [super init]; }
@end

/* listas de regras de conteúdo: o mOS não compila nenhuma; o completion responde "sem lista" */
@interface WKContentRuleListStore : NSObject
+ (instancetype)defaultStore;
- (void)compileContentRuleListForIdentifier:(NSString *)i encodedContentRuleList:(NSString *)l completionHandler:(void (^)(id list, NSError *err))h;
- (void)lookUpContentRuleListForIdentifier:(NSString *)i completionHandler:(void (^)(id list, NSError *err))h;
@end
@implementation WKContentRuleListStore
+ (instancetype)defaultStore { static WKContentRuleListStore *s; if (!s) s = [[self alloc] init]; return s; }
- (void)compileContentRuleListForIdentifier:(NSString *)i encodedContentRuleList:(NSString *)l completionHandler:(void (^)(id list, NSError *err))h { if (h) h(nil, nil); }
- (void)lookUpContentRuleListForIdentifier:(NSString *)i completionHandler:(void (^)(id list, NSError *err))h { if (h) h(nil, nil); }
@end

/* WebView legada (WebKit1) e as preferências dela */
@interface WebPreferences : NSObject
+ (instancetype)standardPreferences;
@property BOOL javaScriptEnabled;
@property BOOL plugInsEnabled;
@end
@implementation WebPreferences
+ (instancetype)standardPreferences { static WebPreferences *s; if (!s) s = [[self alloc] init]; return s; }
@end

@interface WebView : NSView
@property (strong) WebPreferences *preferences;
@property (assign) id frameLoadDelegate;   /* MRC: assign, como era o weak antes do ARC */
@property (assign) id policyDelegate;
@property (assign) id UIDelegate;
- (id)mainFrame;
@end
@implementation WebView
- (id)mainFrame { return nil; }
@end
