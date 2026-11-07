/*
 * mOS — SecurityInterface.framework (Mach-O, em /System/Library/Frameworks): o painel de confiança de certificados.
 * Binários da Apple (Postico) referenciam a classe SFCertificateTrustPanel; o mOS ainda não mostra o painel: quem
 * pede a confirmação de um certificado recebe "cancelado" (NSModalResponseCancel), o mesmo que o usuário recusar.
 * tools/build-frameworks-objc.ps1
 */
#import <AppKit/AppKit.h>
id objc_msgSend(id, SEL, ...);

@interface SFCertificatePanel : NSObject
+ (instancetype)sharedCertificatePanel;
- (NSInteger)runModalForCertificates:(NSArray *)certificates showGroup:(BOOL)showGroup;
- (void)setShowsHelp:(BOOL)v;
- (void)setDefaultButtonTitle:(NSString *)t;
- (void)setAlternateButtonTitle:(NSString *)t;
@end

@interface SFCertificateTrustPanel : SFCertificatePanel
+ (instancetype)sharedCertificateTrustPanel;
- (NSInteger)runModalForTrust:(void *)trust message:(NSString *)message;
- (void)beginSheetForWindow:(NSWindow *)w modalDelegate:(id)d didEndSelector:(SEL)s contextInfo:(void *)c
                      trust:(void *)trust message:(NSString *)message;
- (void)setInformativeText:(NSString *)t;
@end

@implementation SFCertificatePanel
+ (instancetype)sharedCertificatePanel { static id p; if (!p) p = [[self alloc] init]; return p; }
- (NSInteger)runModalForCertificates:(NSArray *)certificates showGroup:(BOOL)showGroup { return 0; }
- (void)setShowsHelp:(BOOL)v {}
- (void)setDefaultButtonTitle:(NSString *)t {}
- (void)setAlternateButtonTitle:(NSString *)t {}
@end

@implementation SFCertificateTrustPanel
+ (instancetype)sharedCertificateTrustPanel { static id p; if (!p) p = [[self alloc] init]; return p; }
- (NSInteger)runModalForTrust:(void *)trust message:(NSString *)message {
    fprintf(stderr, "SecurityInterface[mOS]: painel de confiança de certificado não implementado; respondendo \"cancelar\" (%s)\n",
            message ? [message UTF8String] : "");
    return 0;   /* NSModalResponseCancel */
}
- (void)beginSheetForWindow:(NSWindow *)w modalDelegate:(id)d didEndSelector:(SEL)s contextInfo:(void *)c
                      trust:(void *)trust message:(NSString *)message {
    NSInteger r = [self runModalForTrust:trust message:message];
    if (d && s) ((void (*)(id, SEL, id, NSInteger, void *))objc_msgSend)(d, s, self, r, c);
}
- (void)setInformativeText:(NSString *)t {}
@end
