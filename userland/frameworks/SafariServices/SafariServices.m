/* mOS — SafariServices.framework: a API das extensões do Safari (SFSafariApplication, SFSafariExtensionHandler,
 * SFSafariExtensionManager, SFSafariExtensionState...). No mOS não há Safari; o navegador do sistema é o Horizonte.
 * Então: abrir uma janela com URL abre no navegador padrão (NSWorkspace), e o que depende do Safari (mensagem para a
 * extensão, preferências, janela ativa) responde como um Mac sem o Safari: erro do SFErrorDomain ou nil/desativado. */
#import <Foundation/Foundation.h>

@interface NSURL : NSObject
+ (instancetype)URLWithString:(NSString *)s;
@end
@interface NSWorkspace : NSObject
+ (NSWorkspace *)sharedWorkspace;
- (BOOL)openURL:(NSURL *)u;
@end

NSString *const SFErrorDomain = @"SFErrorDomain";
NSString *const SFExtensionMessageKey = @"message";
enum { SFErrorNoExtensionFound = 1, SFErrorNoAttachmentFound = 2, SFErrorLoadingInterrupted = 3 };

static NSError *no_safari(NSInteger code) {
    return [NSError errorWithDomain:SFErrorDomain code:code
                           userInfo:[NSDictionary dictionaryWithObject:@"O Safari não está instalado neste sistema (mOS)" forKey:@"NSLocalizedDescription"]];
}

@interface SFSafariExtensionState : NSObject { BOOL _enabled; }
@property (readonly, getter=isEnabled) BOOL enabled;
@end
@implementation SFSafariExtensionState
- (BOOL)isEnabled { return _enabled; }
@end

@interface SFSafariApplication : NSObject
@end
@implementation SFSafariApplication
+ (void)getActiveWindowWithCompletionHandler:(void (^)(id window))h { if (h) h(nil); }
+ (void)getAllWindowsWithCompletionHandler:(void (^)(NSArray *windows))h { if (h) h([NSArray array]); }
+ (void)openWindowWithURL:(NSURL *)url completionHandler:(void (^)(id window))h {
    if (url) [[NSWorkspace sharedWorkspace] openURL:url];   /* no navegador padrão do mOS */
    if (h) h(nil);
}
+ (void)setToolbarItemsNeedUpdate {}
+ (void)showPreferencesForExtensionWithIdentifier:(NSString *)ident completionHandler:(void (^)(NSError *error))h { if (h) h(no_safari(SFErrorNoExtensionFound)); }
+ (void)dispatchMessageWithName:(NSString *)name toExtensionWithIdentifier:(NSString *)ident userInfo:(NSDictionary *)info
              completionHandler:(void (^)(NSError *error))h { if (h) h(no_safari(SFErrorNoExtensionFound)); }
+ (void)getHostApplicationWithCompletionHandler:(void (^)(id app))h { if (h) h(nil); }
@end

@interface SFSafariExtensionManager : NSObject
@end
@implementation SFSafariExtensionManager
+ (void)getStateOfSafariExtensionWithIdentifier:(NSString *)ident completionHandler:(void (^)(SFSafariExtensionState *state, NSError *error))h {
    if (h) h(nil, no_safari(SFErrorNoExtensionFound));
}
@end

/* a extensão é carregada pelo Safari, que não existe aqui: a classe só precisa existir para a subclasse do app ligar */
@interface SFSafariExtensionHandler : NSObject
@end
@implementation SFSafariExtensionHandler
- (void)messageReceivedWithName:(NSString *)name fromPage:(id)page userInfo:(NSDictionary *)info {}
- (void)messageReceivedFromContainingAppWithName:(NSString *)name userInfo:(NSDictionary *)info {}
- (void)toolbarItemClickedInWindow:(id)window {}
- (void)validateToolbarItemInWindow:(id)window validationHandler:(void (^)(BOOL enabled, NSString *badge))h { if (h) h(YES, @""); }
- (id)popoverViewController { return nil; }
- (void)popoverWillShowInWindow:(id)window {}
- (void)popoverDidCloseInWindow:(id)window {}
- (void)contextMenuItemSelectedWithCommand:(NSString *)cmd inPage:(id)page userInfo:(NSDictionary *)info {}
- (void)validateContextMenuItemWithCommand:(NSString *)cmd inPage:(id)page userInfo:(NSDictionary *)info
                         validationHandler:(void (^)(BOOL hidden, NSString *text))h { if (h) h(NO, nil); }
- (void)page:(id)page willNavigateToURL:(NSURL *)url {}
- (void)additionalRequestHeadersForURL:(NSURL *)url completionHandler:(void (^)(NSDictionary *))h { if (h) h(nil); }
@end
