/*
 * mOS — teste do WebKit.framework (userland/frameworks/WebKit) e do motor web do sistema (SYS_WEB, kernel/web/webview.c).
 * Um app AppKit com um WKWebView numa janela, usando a API como um app do macOS usa: delegates de navegação e de UI,
 * KVO, evaluateJavaScript, callAsyncJavaScript, messageHandlers (com e sem resposta), WKUserScript, política de
 * navegação, histórico, esquema próprio, busca, clique de verdade num link e a WebView legada. A saída vai ao log serial:
 * "webkit: ok <nome>" / "webkit: FALHA <nome> (...)" e no fim "webkit: N/M ok".
 *   .\build.ps1 -OutDir build-wk
 *   .\tools\build-webkittest.ps1 -OutDir build-wk
 *   .\run.ps1 -Headless -Image build-wk\mOS.img -Serial build-wk\serial.log
 */
#import <AppKit/AppKit.h>
#include "../../kernel/include/syscall.h"

/* ---- a API do WebKit que o teste usa (o SDK do mOS não traz os cabeçalhos; o binário liga ao framework) */
@interface NSURL : NSObject
+ (instancetype)URLWithString:(NSString *)s;
- (NSString *)absoluteString;
@end
@interface NSURLRequest : NSObject
+ (instancetype)requestWithURL:(NSURL *)u;
- (NSURL *)URL;
@end
@interface NSURLResponse : NSObject
- (instancetype)initWithURL:(NSURL *)u MIMEType:(NSString *)m expectedContentLength:(NSInteger)n textEncodingName:(NSString *)e;
@end
@interface NSNull : NSObject
+ (NSNull *)null;
@end
@interface NSString (T)
- (NSData *)dataUsingEncoding:(NSUInteger)e;
- (BOOL)containsString:(NSString *)s;
- (BOOL)hasPrefix:(NSString *)s;
@end
@interface NSError (T)
- (NSInteger)code;
- (NSDictionary *)userInfo;
@end
@interface NSObject (TKVO)
- (void)performSelector:(SEL)s withObject:(id)o afterDelay:(double)d;
- (void)addObserver:(id)o forKeyPath:(NSString *)k options:(NSUInteger)opt context:(void *)c;
@end

@class WKWebView, WKNavigation, WKFrameInfo;
@interface WKPreferences : NSObject
@property BOOL javaScriptEnabled;
@end
@interface WKUserScript : NSObject
- (instancetype)initWithSource:(NSString *)s injectionTime:(NSInteger)t forMainFrameOnly:(BOOL)m;
@end
@interface WKUserContentController : NSObject
- (void)addScriptMessageHandler:(id)h name:(NSString *)n;
- (void)addScriptMessageHandlerWithReply:(id)h contentWorld:(id)w name:(NSString *)n;
- (void)addUserScript:(WKUserScript *)s;
@end
@interface WKContentWorld : NSObject
+ (WKContentWorld *)pageWorld;
@end
@interface WKWebViewConfiguration : NSObject
@property (strong) WKPreferences *preferences;
@property (strong) WKUserContentController *userContentController;
- (void)setURLSchemeHandler:(id)h forURLScheme:(NSString *)s;
@end
@interface WKScriptMessage : NSObject
@property (readonly) id body;
@property (readonly, copy) NSString *name;
@end
@interface WKNavigationAction : NSObject
@property (readonly, copy) NSURLRequest *request;
@property (readonly) NSInteger navigationType;
@end
@interface WKFindConfiguration : NSObject
@end
@interface WKFindResult : NSObject
@property (readonly) BOOL matchFound;
@end
@interface WKBackForwardListItem : NSObject
@property (readonly, copy) NSString *title;
@end
@interface WKBackForwardList : NSObject
@property (readonly) WKBackForwardListItem *backItem;
@end
@protocol WKURLSchemeTask <NSObject>
@property (readonly, copy) NSURLRequest *request;
- (void)didReceiveResponse:(NSURLResponse *)r;
- (void)didReceiveData:(NSData *)d;
- (void)didFinish;
@end
@interface WKWebView : NSView
@property (readonly, copy) WKWebViewConfiguration *configuration;
@property (weak) id navigationDelegate;
@property (weak) id UIDelegate;
@property (readonly, copy) NSString *title;
@property (readonly, copy) NSURL *URL;
@property (readonly, getter=isLoading) BOOL loading;
@property (readonly) double estimatedProgress;
@property (readonly) BOOL canGoBack;
@property (readonly) BOOL canGoForward;
@property (readonly) WKBackForwardList *backForwardList;
- (instancetype)initWithFrame:(NSRect)f configuration:(WKWebViewConfiguration *)c;
- (WKNavigation *)loadHTMLString:(NSString *)s baseURL:(NSURL *)u;
- (WKNavigation *)loadRequest:(NSURLRequest *)r;
- (WKNavigation *)loadData:(NSData *)d MIMEType:(NSString *)m characterEncodingName:(NSString *)e baseURL:(NSURL *)u;
- (WKNavigation *)goBack;
- (WKNavigation *)goForward;
- (WKNavigation *)reload;
- (void)evaluateJavaScript:(NSString *)js completionHandler:(void (^)(id, NSError *))h;
- (void)callAsyncJavaScript:(NSString *)b arguments:(NSDictionary *)a inFrame:(id)f inContentWorld:(id)w completionHandler:(void (^)(id, NSError *))h;
- (void)findString:(NSString *)s withConfiguration:(WKFindConfiguration *)c completionHandler:(void (^)(WKFindResult *))h;
- (int)_mosEngineID;
@end
@interface WebView : WKWebView
@property (weak) id frameLoadDelegate;
- (instancetype)initWithFrame:(NSRect)f frameName:(NSString *)fn groupName:(NSString *)gn;
- (id)mainFrame;
- (NSString *)stringByEvaluatingJavaScriptFromString:(NSString *)js;
@end
@interface NSObject (WebFrameT)
- (id)loadHTMLString:(NSString *)s baseURL:(NSURL *)u;
@end

static long wk_sys(long op, long id, long a, long b) {
    long r;
    register long r10 __asm__("r10") = b;
    __asm__ volatile("int $0x80" : "=a"(r) : "a"((long)SYS_WEB), "D"(op), "S"(id), "d"(a), "r"(r10) : "rcx", "r11", "memory");
    return r;
}

static int g_ok, g_total;
static void check(BOOL c, const char *name, NSString *detail) {
    g_total++;
    if (c) { g_ok++; fprintf(stderr, "webkit: ok %s\n", name); }
    else fprintf(stderr, "webkit: FALHA %s (%s)\n", name, detail ? [[detail description] UTF8String] : "");
}

/* ================================================================ o app */
@interface Teste : NSObject
@property (strong) NSWindow *win;
@property (strong) WKWebView *wv;
@property (strong) WebView *legacy;
@property (strong) NSMutableArray *log;        /* eventos de navegação */
@property (strong) NSMutableArray *msgs;       /* mensagens de messageHandlers.teste */
@property (strong) NSMutableArray *alerts;
@property (strong) NSMutableArray *policies;   /* URLs pedidas à política */
@property int titleKVO;
@property int step;
@property double t0;
@property BOOL waiting;
@property BOOL legacyDone;
@property (strong) NSString *legacyTitle;
@property int failProvisional;
@end

@implementation Teste
- (double)now { return [[NSDate date] timeIntervalSince1970]; }

/* ---- WKNavigationDelegate */
- (void)webView:(WKWebView *)w decidePolicyForNavigationAction:(WKNavigationAction *)a decisionHandler:(void (^)(NSInteger))h {
    NSString *u = [[a.request URL] absoluteString] ?: @"";
    [self.policies addObject:[NSString stringWithFormat:@"%ld %@", (long)a.navigationType, u]];
    h([u containsString:@"bloqueado"] ? 0 : 1);
}
- (void)webView:(WKWebView *)w didStartProvisionalNavigation:(WKNavigation *)n { [self.log addObject:@"start"]; }
- (void)webView:(WKWebView *)w didCommitNavigation:(WKNavigation *)n { [self.log addObject:@"commit"]; }
- (void)webView:(WKWebView *)w didFinishNavigation:(WKNavigation *)n { [self.log addObject:@"finish"]; }
- (void)webView:(WKWebView *)w didFailProvisionalNavigation:(WKNavigation *)n withError:(NSError *)e {
    [self.log addObject:[NSString stringWithFormat:@"failprov %ld", (long)[e code]]];
    self.failProvisional = (int)[e code];
}
/* ---- WKUIDelegate */
- (void)webView:(WKWebView *)w runJavaScriptAlertPanelWithMessage:(NSString *)m initiatedByFrame:(id)f completionHandler:(void (^)(void))h {
    [self.alerts addObject:m];
    h();
}
/* ---- WKScriptMessageHandler e WithReply */
- (void)userContentController:(id)c didReceiveScriptMessage:(WKScriptMessage *)m {
    [self.msgs addObject:m.body ?: [NSNull null]];
}
- (void)userContentController:(id)c didReceiveScriptMessage:(WKScriptMessage *)m replyHandler:(void (^)(id, NSString *))reply {
    reply([NSString stringWithFormat:@"pong:%@", m.body], nil);
}
/* ---- WKURLSchemeHandler */
- (void)webView:(WKWebView *)w startURLSchemeTask:(id<WKURLSchemeTask>)t {
    NSString *html = @"<html><head><title>Esquema</title></head><body><p id=e>servido pelo app</p></body></html>";
    [t didReceiveResponse:[[NSURLResponse alloc] initWithURL:[t.request URL] MIMEType:@"text/html" expectedContentLength:-1 textEncodingName:@"utf-8"]];
    [t didReceiveData:[html dataUsingEncoding:NSUTF8StringEncoding]];
    [t didFinish];
}
- (void)webView:(WKWebView *)w stopURLSchemeTask:(id)t {}
/* ---- KVO */
- (void)observeValueForKeyPath:(NSString *)k ofObject:(id)o change:(NSDictionary *)c context:(void *)x {
    if ([k isEqualToString:@"title"]) self.titleKVO++;
}
/* ---- WebFrameLoadDelegate (legada) */
- (void)webView:(id)w didFinishLoadForFrame:(id)f { self.legacyDone = YES; }
- (void)webView:(id)w didReceiveTitle:(NSString *)t forFrame:(id)f { self.legacyTitle = t; }

/* espera até a condição valer (ou o prazo); o tique roda o run loop entre as verificações */
- (BOOL)waitFor:(BOOL (^)(void))cond seconds:(double)s {
    double t = [self now];
    while (!cond()) {
        if ([self now] - t > s) return NO;
        [[NSRunLoop currentRunLoop] runUntilDate:[NSDate dateWithTimeIntervalSinceNow:0.02]];
    }
    return YES;
}
- (id)eval:(NSString *)js error:(NSError **)err {
    __block BOOL done = NO;
    __block id val = nil;
    __block NSError *e = nil;
    [self.wv evaluateJavaScript:js completionHandler:^(id v, NSError *er) { val = v; e = er; done = YES; }];
    BOOL sync = done;
    [self waitFor:^{ return done; } seconds:5];
    check(!sync, [[NSString stringWithFormat:@"evaluateJavaScript assíncrono (%@)", js] UTF8String], nil);
    if (err) *err = e;
    return val;
}

- (void)run {
    self.log = [NSMutableArray array];
    self.msgs = [NSMutableArray array];
    self.alerts = [NSMutableArray array];
    self.policies = [NSMutableArray array];
    self.win = [[NSWindow alloc] initWithContentRect:NSMakeRect(100, 100, 640, 480) styleMask:15 backing:2 defer:NO];
    [self.win setTitle:@"Teste WebKit"];
    WKWebViewConfiguration *cfg = [[WKWebViewConfiguration alloc] init];
    [cfg.userContentController addScriptMessageHandler:self name:@"teste"];
    [cfg.userContentController addScriptMessageHandlerWithReply:self contentWorld:[WKContentWorld pageWorld] name:@"eco"];
    [cfg.userContentController addUserScript:[[WKUserScript alloc] initWithSource:@"window.__inicio = (window.__inicio||0) + 1;" injectionTime:0 forMainFrameOnly:YES]];
    [cfg.userContentController addUserScript:[[WKUserScript alloc] initWithSource:@"window.__fim = document.title;" injectionTime:1 forMainFrameOnly:YES]];
    [cfg setURLSchemeHandler:self forURLScheme:@"mostest"];
    self.wv = [[WKWebView alloc] initWithFrame:NSMakeRect(0, 0, 640, 480) configuration:cfg];
    self.wv.navigationDelegate = self;
    self.wv.UIDelegate = self;
    [self.wv addObserver:self forKeyPath:@"title" options:1 context:NULL];
    [self.win setContentView:self.wv];
    [self.win makeKeyAndOrderFront:nil];
    check(self.wv && [self.wv _mosEngineID] > 0, "WKWebView criado com vista no motor", nil);

    /* 1. loadHTMLString: navegação, mensagens, alert, título e KVO */
    NSString *html1 =
        @"<html><head><title>Pagina 1</title><style>body{background:#ff0000;margin:0}a{font-size:40px}</style></head><body>"
         "<a id=lk href='data:text/html,%3Ctitle%3ELinkado%3C/title%3Eok'>LINK GRANDE AQUI</a><div style='height:3000px'></div><p>fim agulha</p>"
         "<script>window.webkit.messageHandlers.teste.postMessage({a:1,b:[2,3],c:'x'});alert('oi');</script></body></html>";
    [self.wv loadHTMLString:html1 baseURL:nil];
    BOOL fin = [self waitFor:^{ return [self.log containsObject:@"finish"]; } seconds:10];
    check(fin, "didFinishNavigation do loadHTMLString", [self.log componentsJoinedByString:@","]);
    check([self.log count] >= 3 && [self.log[0] isEqualToString:@"start"] && [self.log[1] isEqualToString:@"commit"], "ordem start/commit/finish",
          [self.log componentsJoinedByString:@","]);
    check([self.wv.title isEqualToString:@"Pagina 1"], "title", self.wv.title);
    check(self.titleKVO >= 1, "KVO de title", [NSString stringWithFormat:@"%d", self.titleKVO]);
    check(!self.wv.loading && self.wv.estimatedProgress == 1.0, "loading/estimatedProgress no fim", nil);
    [self waitFor:^{ return (BOOL)([self.msgs count] > 0 && [self.alerts count] > 0); } seconds:3];
    id m0 = [self.msgs firstObject];
    check([m0 isKindOfClass:[NSDictionary class]] && [m0[@"a"] intValue] == 1 && [m0[@"b"] count] == 2 && [m0[@"c"] isEqualToString:@"x"],
          "messageHandlers.postMessage com objeto", [m0 description]);
    check([[self.alerts firstObject] isEqualToString:@"oi"], "alert chega ao WKUIDelegate", [self.alerts description]);
    check([self.policies count] >= 1, "decidePolicyForNavigationAction no loadHTMLString", [self.policies description]);

    /* 2. evaluateJavaScript e os tipos */
    NSError *e = nil;
    id v = [self eval:@"1+2" error:&e];
    check([v isKindOfClass:[NSNumber class]] && [v intValue] == 3 && !e, "evaluate 1+2", [v description]);
    v = [self eval:@"document.title" error:&e];
    check([v isEqualToString:@"Pagina 1"], "evaluate document.title", [v description]);
    v = [self eval:@"({x:[1,2],s:'t',n:null})" error:&e];
    check([v isKindOfClass:[NSDictionary class]] && [v[@"x"] count] == 2 && v[@"n"] == [NSNull null], "evaluate objeto -> NSDictionary", [v description]);
    v = [self eval:@"null" error:&e];
    check(v == [NSNull null] && !e, "evaluate null -> NSNull", [v description]);
    v = [self eval:@"true" error:&e];
    check([v isKindOfClass:[NSNumber class]] && [v boolValue], "evaluate true", [v description]);
    v = [self eval:@"throw new Error('boom')" error:&e];
    check(!v && [e code] == 4 && [[e.userInfo[@"WKJavaScriptExceptionMessage"] description] containsString:@"boom"], "exceção -> WKErrorJavaScriptExceptionOccurred",
          [e description]);
    v = [self eval:@"(function(){})" error:&e];
    check(!v && [e code] == 5, "função -> WKErrorJavaScriptResultTypeIsUnsupported", [e description]);
    v = [self eval:@"window.__inicio" error:&e];
    check([v intValue] == 1, "WKUserScript AtDocumentStart", [v description]);
    v = [self eval:@"window.__fim" error:&e];
    check([v isEqualToString:@"Pagina 1"], "WKUserScript AtDocumentEnd", [v description]);
    v = [self eval:@"document.body.style.background='#00ff00'; 'cor'" error:&e];

    /* 3. pixels: a página pintou o fundo verde que o script pôs */
    int wid = [self.wv _mosEngineID];
    [self waitFor:^{ return NO; } seconds:0.2];
    uint32_t *px = malloc(640 * 480 * 4);
    long pr = wk_sys(MOS_WEB_PAINT, wid, (long)px, 640 * 480 * 4);
    uint32_t c = pr > 0 ? px[480 / 2 * 640 + 600] : 0;
    check(pr == 640 * 480 * 4 && (c & 0xFFFFFF) == 0x00FF00, "pintura no buffer (fundo do body)", [NSString stringWithFormat:@"r=%ld px=%08x", pr, c]);

    /* 4. callAsyncJavaScript */
    __block id av = nil;
    __block BOOL ad = NO;
    [self.wv callAsyncJavaScript:@"return await Promise.resolve(a * 2);" arguments:@{@"a": @21} inFrame:nil inContentWorld:[WKContentWorld pageWorld]
               completionHandler:^(id r, NSError *er) { av = r; ad = YES; }];
    [self waitFor:^{ return ad; } seconds:5];
    check(ad && [av intValue] == 42, "callAsyncJavaScript com promessa", [av description]);

    /* 5. messageHandler com resposta (a promessa do JavaScript resolve com o que o app devolveu) */
    [self eval:@"window.webkit.messageHandlers.eco.postMessage('ping').then(function(r){window.__eco=r;})" error:NULL];
    __block id eco = nil;
    [self waitFor:^{ eco = [self eval:@"window.__eco||''" error:NULL]; return (BOOL)[eco length]; } seconds:5];
    check([eco isEqualToString:@"pong:ping"], "addScriptMessageHandlerWithReply", [eco description]);

    /* 6. busca */
    __block int found = -1;
    [self.wv findString:@"agulha" withConfiguration:[[WKFindConfiguration alloc] init] completionHandler:^(WKFindResult *r) { found = r.matchFound; }];
    [self waitFor:^{ return (BOOL)(found >= 0); } seconds:3];
    check(found == 1, "findString", nil);
    v = [self eval:@"window.scrollY" error:NULL];

    /* 7. clique de verdade no link (MOS_WEB_INPUT): navegação com WKNavigationTypeLinkActivated */
    [self eval:@"window.scrollTo(0,0)" error:NULL];
    [self waitFor:^{ return NO; } seconds:0.2];
    [self.log removeAllObjects];
    [self.policies removeAllObjects];
    mos_web_input_t in;
    memset(&in, 0, sizeof in);
    in.x = 60; in.y = 20;
    in.kind = MOS_WEB_IN_MOVE; wk_sys(MOS_WEB_INPUT, wid, (long)&in, 0);
    in.kind = MOS_WEB_IN_DOWN; in.clicks = 1; long cur = wk_sys(MOS_WEB_INPUT, wid, (long)&in, 0);
    in.kind = MOS_WEB_IN_UP; wk_sys(MOS_WEB_INPUT, wid, (long)&in, 0);
    check(cur == MOS_WEB_CURSOR_HAND, "cursor de mão sobre o link", [NSString stringWithFormat:@"%ld", cur]);
    fin = [self waitFor:^{ return [self.wv.title isEqualToString:@"Linkado"]; } seconds:5];
    check(fin, "clique no link navega", self.wv.title);
    check([self.policies count] && [self.policies[0] hasPrefix:@"0 data:"], "política com WKNavigationTypeLinkActivated", [self.policies description]);
    check(self.wv.canGoBack, "canGoBack depois do link", nil);

    /* 8. voltar: a página do loadHTMLString volta com o conteúdo (o histórico guarda o HTML) */
    [self.wv goBack];
    fin = [self waitFor:^{ return [self.wv.title isEqualToString:@"Pagina 1"]; } seconds:5];
    check(fin, "goBack volta ao HTML carregado", self.wv.title);
    check(self.wv.canGoForward, "canGoForward depois de voltar", nil);
    v = [self eval:@"document.getElementById('lk') ? 'sim' : 'nao'" error:NULL];
    check([v isEqualToString:@"sim"], "o DOM da página voltou", [v description]);

    /* 9. política: o app cancela a navegação */
    [self eval:@"location.href='data:text/html,<title>bloqueado</title>x'" error:NULL];
    [self waitFor:^{ return NO; } seconds:1];
    check([self.wv.title isEqualToString:@"Pagina 1"], "decisionHandler(Cancel) impede a navegação", self.wv.title);

    /* 10. esquema próprio (WKURLSchemeHandler) */
    [self.wv loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"mostest://app/index.html"]]];
    fin = [self waitFor:^{ return [self.wv.title isEqualToString:@"Esquema"]; } seconds:5];
    check(fin, "WKURLSchemeHandler serve a página", self.wv.title);
    check([[self.wv.URL absoluteString] isEqualToString:@"mostest://app/index.html"], "URL do esquema próprio", [self.wv.URL absoluteString]);

    /* 11. loadData com text/plain */
    [self.wv loadData:[@"texto puro <b>sem</b> html" dataUsingEncoding:NSUTF8StringEncoding] MIMEType:@"text/plain" characterEncodingName:@"utf-8"
              baseURL:[NSURL URLWithString:@"about:blank"]];
    [self waitFor:^{ return NO; } seconds:0.5];
    v = [self eval:@"document.body.textContent" error:NULL];
    check([v containsString:@"<b>sem</b>"], "loadData text/plain mostra o texto cru", [v description]);

    /* 12. falha provisória (esquema sem suporte) */
    self.failProvisional = 0;
    [self.wv loadRequest:[NSURLRequest requestWithURL:[NSURL URLWithString:@"gopher://x/"]]];
    [self waitFor:^{ return (BOOL)(self.failProvisional != 0); } seconds:5];
    check(self.failProvisional == -1002, "didFailProvisionalNavigation (NSURLErrorUnsupportedURL)", [NSString stringWithFormat:@"%d", self.failProvisional]);

    /* 13. WebView legada (WebKit1) */
    self.legacy = [[WebView alloc] initWithFrame:NSMakeRect(0, 0, 300, 200) frameName:nil groupName:nil];
    self.legacy.frameLoadDelegate = self;
    [[self.legacy mainFrame] loadHTMLString:@"<title>Antiga</title><p>x</p>" baseURL:nil];
    fin = [self waitFor:^{ return self.legacyDone; } seconds:5];
    check(fin, "WebView: didFinishLoadForFrame", nil);
    check([self.legacyTitle isEqualToString:@"Antiga"], "WebView: didReceiveTitle", self.legacyTitle);
    check([[self.legacy stringByEvaluatingJavaScriptFromString:@"2*3"] isEqualToString:@"6"], "WebView: stringByEvaluatingJavaScriptFromString", nil);

    /* 14. NSClassFromString acha o WKWebView do framework, não a fachada do libSystem */
    check(NSClassFromString(@"WKWebView") == [WKWebView class], "NSClassFromString(WKWebView) é o do framework", nil);

    fprintf(stderr, "webkit: %d/%d ok\n", g_ok, g_total);
    fflush(stderr);
    exit(g_ok == g_total ? 0 : 1);
}
@end

int main(int argc, const char **argv) {
    @autoreleasepool {
        [NSApplication sharedApplication];
        Teste *t = [[Teste alloc] init];
        [t performSelector:@selector(run) withObject:nil afterDelay:0.1];
        [NSApp run];
    }
    return 0;
}
