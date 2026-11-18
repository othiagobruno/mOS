// mOS — ExtensionFoundation: descoberta das extensões de app (.appex) instaladas.
// No macOS quem faz isso é o pluginkit/extensionkitservice; no mOS não há esse serviço, então o framework lê o disco:
// o pacote do próprio app e os apps de /Applications, /System/Applications e ~/Applications. Cada .appex declara o ponto
// de extensão no Info.plist: EXAppExtensionAttributes.EXExtensionPointIdentifier (ExtensionKit) ou
// NSExtension.NSExtensionPointIdentifier (extensões NSExtension antigas).
#import <Foundation/Foundation.h>

/* o mesmo protótipo de include/ExtensionFoundation_mos.h (lá ele fica ao lado de um NSXPCConnection mínimo) */
typedef void (*mos_ef_visit)(void *ctx, const char *bid, const char *name, const char *point, const char *path);
void mos_ef_enumerate(void *ctx, mos_ef_visit cb);

static NSString *str_de(id v) {
    return [v isKindOfClass:[NSString class]] ? (NSString *)v : nil;
}

static NSDictionary *dict_de(id v) {
    return [v isKindOfClass:[NSDictionary class]] ? (NSDictionary *)v : nil;
}

static void visita_appex(NSString *appex, void *ctx, mos_ef_visit cb) {
    NSDictionary *info = [NSDictionary dictionaryWithContentsOfFile:[appex stringByAppendingPathComponent:@"Contents/Info.plist"]];
    if (!info) return;
    NSString *bid = str_de([info objectForKey:@"CFBundleIdentifier"]);
    if ([bid length] == 0) return;
    NSString *ponto = str_de([dict_de([info objectForKey:@"EXAppExtensionAttributes"]) objectForKey:@"EXExtensionPointIdentifier"]);
    if ([ponto length] == 0)
        ponto = str_de([dict_de([info objectForKey:@"NSExtension"]) objectForKey:@"NSExtensionPointIdentifier"]);
    if ([ponto length] == 0) return;
    NSString *nome = str_de([info objectForKey:@"CFBundleDisplayName"]);
    if ([nome length] == 0) nome = str_de([info objectForKey:@"CFBundleName"]);
    if ([nome length] == 0) nome = [appex lastPathComponent];   /* o Swift tira o .appex */
    cb(ctx, [bid UTF8String], [nome UTF8String], [ponto UTF8String], [appex UTF8String]);
}

static void visita_pacote(NSString *app, void *ctx, mos_ef_visit cb) {
    NSFileManager *fm = [NSFileManager defaultManager];
    for (NSString *sub in @[ @"Contents/Extensions", @"Contents/PlugIns" ]) {
        NSString *dir = [app stringByAppendingPathComponent:sub];
        for (NSString *n in [fm contentsOfDirectoryAtPath:dir error:NULL])
            if ([[n pathExtension] isEqualToString:@"appex"])
                visita_appex([dir stringByAppendingPathComponent:n], ctx, cb);
    }
}

void mos_ef_enumerate(void *ctx, mos_ef_visit cb) {
    @autoreleasepool {
        NSString *proprio = [[NSBundle mainBundle] bundlePath];
        if ([proprio length]) visita_pacote(proprio, ctx, cb);
        NSFileManager *fm = [NSFileManager defaultManager];
        NSString *home = NSHomeDirectory();
        NSArray *raizes = @[ @"/Applications", @"/System/Applications",
                             home ? [home stringByAppendingPathComponent:@"Applications"] : @"/Applications" ];
        for (NSString *raiz in raizes)
            for (NSString *n in [fm contentsOfDirectoryAtPath:raiz error:NULL]) {
                if (![[n pathExtension] isEqualToString:@"app"]) continue;
                NSString *app = [raiz stringByAppendingPathComponent:n];
                if ([proprio length] && [app isEqualToString:proprio]) continue;
                visita_pacote(app, ctx, cb);
            }
    }
}
