/*
 * Photos.framework. Mesmo índice de userland/libsystem/photos_fw.c:
 * pasta Imagens (ou MOS_PHOTOS_LIBRARY) e TCC kTCCServicePhotos.
 * Sem concessão ou sem pasta, fetchAssetsWithOptions: devolve resultado com count 0.
 * tools/build-frameworks-objc.ps1
 */
#import <Foundation/Foundation.h>
#include "photos_index.h"
#include <string.h>

extern int cx_tcc_state(const char *service);
extern int cx_tcc_request(const char *service);
extern id NSHomeDirectory(void);

static const char *kSvc = "kTCCServicePhotos";

static void library_path(char *out, size_t cap) {
    const char *env = getenv("MOS_PHOTOS_LIBRARY");
    if (env && env[0]) { snprintf(out, cap, "%s", env); return; }
    id home = NSHomeDirectory();
    photos_root_from_home(home ? [home UTF8String] : 0, out, cap);
}

@interface PHAsset : NSObject {
   @public
    photos_asset a;
}
@end
@interface PHFetchOptions : NSObject {
   @public
    id pred, sorts;
    NSUInteger fetchLimit;
    BOOL includeHiddenAssets;
}
@end
@interface PHFetchResult : NSObject {
   @public
    photos_asset *items;
    NSUInteger n;
}
@end
@interface NSObject (MOSPhotos)
- (BOOL)evaluateWithObject:(id)obj;
- (NSString *)key;
- (BOOL)ascending;
@end

@implementation PHAsset
- (NSString *)localIdentifier { return [NSString stringWithUTF8String:a.ident]; }
- (NSInteger)mediaType { return a.media; }
- (NSInteger)pixelWidth { return a.pixel_w; }
- (NSInteger)pixelHeight { return a.pixel_h; }
- (NSTimeInterval)duration { return a.duration; }
- (BOOL)isHidden { return a.hidden ? YES : NO; }
- (NSDate *)creationDate { return [NSDate dateWithTimeIntervalSince1970:(NSTimeInterval)a.creation_unix]; }
- (id)valueForKey:(NSString *)key {
    const char *k = key ? [key UTF8String] : "";
    if (!strcmp(k, "localIdentifier")) return [self localIdentifier];
    if (!strcmp(k, "mediaType")) return [NSNumber numberWithInteger:a.media];
    if (!strcmp(k, "pixelWidth")) return [NSNumber numberWithInteger:a.pixel_w];
    if (!strcmp(k, "pixelHeight")) return [NSNumber numberWithInteger:a.pixel_h];
    if (!strcmp(k, "duration")) return [NSNumber numberWithDouble:a.duration];
    if (!strcmp(k, "creationDate")) return [self creationDate];
    if (!strcmp(k, "hidden") || !strcmp(k, "isHidden")) return [NSNumber numberWithBool:a.hidden ? YES : NO];
    return nil;
}
+ (PHFetchResult *)fetchAssetsWithOptions:(PHFetchOptions *)opts;
@end

@implementation PHFetchResult
- (void)dealloc { free(items); [super dealloc]; }
- (NSUInteger)count { return n; }
- (id)objectAtIndex:(NSUInteger)i {
    PHAsset *o;
    if (i >= n) return nil;
    o = [PHAsset new];
    o->a = items[i];
    return o;
}
- (id)objectAtIndexedSubscript:(NSUInteger)i { return [self objectAtIndex:i]; }
- (id)firstObject { return n ? [self objectAtIndex:0] : nil; }
- (id)lastObject { return n ? [self objectAtIndex:n - 1] : nil; }
- (void)enumerateObjectsUsingBlock:(id)block {
    NSUInteger i;
    BOOL stop = NO;
    void (*inv)(void *, id, NSUInteger, BOOL *);
    if (!block) return;
    inv = *(void (**)(void *, id, NSUInteger, BOOL *))((char *)block + 16);
    if (!inv) return;
    for (i = 0; i < n && !stop; i++) inv(block, [self objectAtIndex:i], i, &stop);
}
@end

static int sort_code(const char *key, int asc) {
    if (!key) return PHOTOS_SORT_NONE;
    if (!strcmp(key, "creationDate")) return asc ? PHOTOS_SORT_CREATION_ASC : PHOTOS_SORT_CREATION_DESC;
    if (!strcmp(key, "localIdentifier")) return asc ? PHOTOS_SORT_IDENT_ASC : PHOTOS_SORT_IDENT_DESC;
    if (!strcmp(key, "pixelWidth")) return asc ? PHOTOS_SORT_WIDTH_ASC : PHOTOS_SORT_WIDTH_DESC;
    if (!strcmp(key, "pixelHeight")) return asc ? PHOTOS_SORT_HEIGHT_ASC : PHOTOS_SORT_HEIGHT_DESC;
    if (!strcmp(key, "duration")) return asc ? PHOTOS_SORT_DURATION_ASC : PHOTOS_SORT_DURATION_DESC;
    return PHOTOS_SORT_NONE;
}

@implementation PHFetchOptions
- (instancetype)init {
    self = [super init];
    fetchLimit = 0;
    includeHiddenAssets = NO;
    return self;
}
@end

@interface PHPhotoLibrary : NSObject
+ (NSInteger)authorizationStatus;
+ (void)requestAuthorization:(id)handler;
@end

@implementation PHPhotoLibrary
static PHPhotoLibrary *g_lib;
+ (instancetype)sharedPhotoLibrary {
    if (!g_lib) g_lib = [PHPhotoLibrary new];
    return g_lib;
}
+ (NSInteger)authorizationStatus { return photos_auth_from_tcc(cx_tcc_state(kSvc)); }
+ (NSInteger)authorizationStatusForAccessLevel:(NSInteger)level {
    (void)level;
    return [self authorizationStatus];
}
+ (void)requestAuthorization:(id)handler {
    void (*inv)(void *, NSInteger);
    cx_tcc_request(kSvc);
    if (!handler) return;
    inv = *(void (**)(void *, NSInteger))((char *)handler + 16);
    if (inv) inv(handler, [self authorizationStatus]);
}
+ (void)requestAuthorizationForAccessLevel:(NSInteger)level handler:(id)handler {
    (void)level;
    [self requestAuthorization:handler];
}
@end

@implementation PHAsset (Fetch)
+ (PHFetchResult *)fetchAssetsWithOptions:(PHFetchOptions *)opts {
    PHFetchResult *empty = [PHFetchResult new];
    char path[512];
    photos_lib *lib;
    photos_query q, q2;
    int idx[400], idx2[400];
    photos_asset kept[400], ordered[400];
    int n, nk = 0, n2, i;
    if ([PHPhotoLibrary authorizationStatus] != PHOTOS_AUTH_AUTHORIZED) return empty;
    library_path(path, sizeof path);
    lib = photos_open(path);
    if (!lib) return empty;
    memset(&q, 0, sizeof q);
    q.media = -1;
    q.include_hidden = (opts && opts->includeHiddenAssets) ? 1 : 0;
    {
        int all = photos_count_all(lib);
        const photos_asset *base = all ? photos_at(lib, 0) : 0;
        /* photos_at(0) é o primeiro; o vetor é contíguo. */
        n = base ? photos_apply(base, all, &q, idx, 400) : 0;
    }
    for (i = 0; i < n; i++) {
        const photos_asset *src = photos_at(lib, idx[i]);
        PHAsset *tmp;
        if (!src) continue;
        if (opts && opts->pred) {
            tmp = [PHAsset new];
            tmp->a = *src;
            if (![opts->pred evaluateWithObject:tmp]) { [tmp release]; continue; }
            [tmp release];
        }
        kept[nk++] = *src;
    }
    photos_close(lib);
    memset(&q2, 0, sizeof q2);
    q2.media = -1;
    q2.include_hidden = 1;
    if (opts && opts->sorts) {
        NSUInteger ns = [opts->sorts count];
        NSUInteger s;
        if (ns > 4) ns = 4;
        for (s = 0; s < ns; s++) {
            id d = [opts->sorts objectAtIndex:s];
            NSString *key = [d key];
            int code = sort_code(key ? [key UTF8String] : 0, [d ascending] ? 1 : 0);
            if (code) q2.sort[q2.nsort++] = code;
        }
        q2.limit = (unsigned)opts->fetchLimit;
    }
    n2 = photos_apply(kept, nk, &q2, idx2, 400);
    for (i = 0; i < n2; i++) ordered[i] = kept[idx2[i]];
    if (n2 > 0) {
        empty->items = calloc((size_t)n2, sizeof(photos_asset));
        if (empty->items) {
            memcpy(empty->items, ordered, (size_t)n2 * sizeof(photos_asset));
            empty->n = (NSUInteger)n2;
        }
    }
    return empty;
}
@end
