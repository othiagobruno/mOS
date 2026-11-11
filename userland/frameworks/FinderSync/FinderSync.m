/*
 * mOS — FinderSync.framework (Mach-O, em /System/Library/Frameworks): a extensão de sincronização do Finder.
 * O Setapp traz um FinderSyncExt.appex cuja classe principal herda de FIFinderSync e usa o FIFinderSyncController.
 *
 * O mOS não tem o Finder da Apple nem o pluginkit que carrega essas extensões. O que existe aqui é o estado que a API
 * guarda: as pastas observadas (directoryURLs), as imagens e rótulos de badge e o badge de cada URL, que a extensão
 * consulta de volta. Ninguém chama menuForMenuKind:, beginObservingDirectoryAtURL: etc. por conta própria, e
 * +isExtensionEnabled responde NO (não há extensão do Finder ligada no mOS); +showExtensionManagementInterface
 * avisa no stderr que não há painel de extensões.
 * tools/build-frameworks-objc.sh
 */
#import <AppKit/AppKit.h>

@interface FIFinderSyncController : NSObject {
    id _dirs;            /* NSSet de NSURL */
    id _badges;          /* identificador -> @{ image, label } */
    id _urlBadges;       /* caminho absoluto -> identificador */
    id _lastUsed;        /* NSDate */
}
+ (instancetype)defaultController;
+ (BOOL)isExtensionEnabled;
+ (void)showExtensionManagementInterface;
@property (copy) id directoryURLs;
@property (copy) id lastUsedDate;
- (void)setBadgeImage:(id)image label:(id)label forBadgeIdentifier:(id)badgeID;
- (void)setBadgeIdentifier:(id)badgeID forURL:(id)url;
- (id)targetedURL;
- (id)selectedItemURLs;
@end

@interface FIFinderSync : NSObject
- (id)menuForMenuKind:(NSInteger)menuKind;
- (void)beginObservingDirectoryAtURL:(id)url;
- (void)endObservingDirectoryAtURL:(id)url;
- (void)requestBadgeIdentifierForURL:(id)url;
- (id)toolbarItemName;
- (id)toolbarItemImage;
- (id)toolbarItemToolTip;
@end

static id url_key(id url) {
    if (!url) return nil;
    if ([url respondsToSelector:@selector(path)]) return [url performSelector:@selector(path)];
    return [url description];
}

@implementation FIFinderSyncController
+ (instancetype)defaultController {
    static FIFinderSyncController *c;
    if (!c) c = [[self alloc] init];
    return c;
}
+ (BOOL)isExtensionEnabled { return NO; }
+ (void)showExtensionManagementInterface {
    fprintf(stderr, "FinderSync[mOS]: o mOS não tem o painel de extensões do Finder\n");
}
- (instancetype)init {
    if ((self = [super init])) {
        _dirs = [[NSClassFromString(@"NSSet") alloc] init];
        _badges = [[NSMutableDictionary alloc] init];
        _urlBadges = [[NSMutableDictionary alloc] init];
    }
    return self;
}
- (id)directoryURLs { return _dirs; }
- (void)setDirectoryURLs:(id)set {
    id n = set ? [set copy] : [[NSClassFromString(@"NSSet") alloc] init];
    [_dirs release];
    _dirs = n;
}
- (id)lastUsedDate { return _lastUsed; }
- (void)setLastUsedDate:(id)d { id n = [d copy]; [_lastUsed release]; _lastUsed = n; }
- (void)setBadgeImage:(id)image label:(id)label forBadgeIdentifier:(id)badgeID {
    if (!badgeID) return;
    NSMutableDictionary *b = [NSMutableDictionary dictionary];
    if (image) [b setObject:image forKey:@"image"];
    if (label) [b setObject:label forKey:@"label"];
    [_badges setObject:b forKey:badgeID];
}
- (void)setBadgeIdentifier:(id)badgeID forURL:(id)url {
    id k = url_key(url);
    if (!k) return;
    if (badgeID && [badgeID length]) [_urlBadges setObject:badgeID forKey:k];
    else [_urlBadges removeObjectForKey:k];
}
/* sem Finder não há janela nem seleção: nada mirado, nada selecionado */
- (id)targetedURL { return nil; }
- (id)selectedItemURLs { return nil; }
@end

@implementation FIFinderSync
- (id)menuForMenuKind:(NSInteger)menuKind { return nil; }
- (void)beginObservingDirectoryAtURL:(id)url {}
- (void)endObservingDirectoryAtURL:(id)url {}
- (void)requestBadgeIdentifierForURL:(id)url {}
- (id)toolbarItemName { return nil; }
- (id)toolbarItemImage { return nil; }
- (id)toolbarItemToolTip { return nil; }
@end
