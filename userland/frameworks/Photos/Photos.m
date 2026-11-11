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
    NSArray *objs;               /* coleções: quando não é nil, vale no lugar de items */
    id opts;                     /* opções da busca de assets, para refazer depois de uma mudança */
}
@end
@interface PHAsset (Fetch)
+ (PHFetchResult *)fetchAssetsWithOptions:(PHFetchOptions *)opts;
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
- (NSDate *)modificationDate { return [self creationDate]; }
- (BOOL)isEqual:(id)o { return o && [o isKindOfClass:[PHAsset class]] && !strcmp(a.ident, ((PHAsset *)o)->a.ident); }
- (NSUInteger)hash { NSUInteger h = 5381; for (const char *s = a.ident; *s; s++) h = h * 33 + (unsigned char)*s; return h; }
- (NSInteger)mediaSubtypes { return 0; }
- (NSInteger)sourceType { return 1; }        /* PHAssetSourceTypeUserLibrary */
- (BOOL)isFavorite { return NO; }
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
@end

@implementation PHFetchResult
- (void)dealloc { free(items); [objs release]; [opts release]; [super dealloc]; }
- (NSUInteger)count { return objs ? [objs count] : n; }
- (id)objectAtIndex:(NSUInteger)i {
    PHAsset *o;
    if (objs) return i < [objs count] ? [objs objectAtIndex:i] : nil;
    if (i >= n) return nil;
    o = [PHAsset new];
    o->a = items[i];
    return [o autorelease];
}
- (BOOL)containsObject:(id)obj {
    for (NSUInteger i = 0; i < [self count]; i++) if ([[self objectAtIndex:i] isEqual:obj]) return YES;
    return NO;
}
- (NSUInteger)indexOfObject:(id)obj {
    for (NSUInteger i = 0; i < [self count]; i++) if ([[self objectAtIndex:i] isEqual:obj]) return i;
    return (NSUInteger)-1;      /* NSNotFound */
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
    if (opts) empty->opts = [opts retain];
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

/* ======================================================================== resto da API sobre a mesma biblioteca local
 * Imagem: o arquivo do asset passa pelo ImageIO do mOS (PNG e JPEG) e sai como UIImage (Catalyst) ou NSImage.
 * Criação: performChanges copia o arquivo para a pasta da biblioteca e avisa os observadores. Sem nuvem:
 * PHImageResultIsInCloudKey é sempre NO e não há imagem degradada antes da final. */
#include <stdio.h>
#include <time.h>
#include <sys/stat.h>

typedef struct { double width, height; } PHSize;
const PHSize PHImageManagerMaximumSize = {-1, -1};
NSString *const PHImageErrorKey = @"PHImageErrorKey";
NSString *const PHImageCancelledKey = @"PHImageCancelledKey";
NSString *const PHImageResultIsInCloudKey = @"PHImageResultIsInCloudKey";
NSString *const PHImageResultIsDegradedKey = @"PHImageResultIsDegradedKey";
NSString *const PHImageResultRequestIDKey = @"PHImageResultRequestIDKey";
NSString *const PHLivePhotoInfoErrorKey = @"PHLivePhotoInfoErrorKey";
NSString *const PHPhotosErrorDomain = @"PHPhotosErrorDomain";

extern void *CGImageSourceCreateWithURL(void *url, void *opts);
extern void *CGImageSourceCreateImageAtIndex(void *src, size_t i, void *opts);
extern void *CGImageSourceCreateThumbnailAtIndex(void *src, size_t i, void *opts);
extern void CFRelease(void *o);
extern void CGImageRelease(void *img);
extern id objc_getClass(const char *name);

static id ph_error(long code, const char *what) {
    id info = [NSDictionary dictionaryWithObject:[NSString stringWithUTF8String:what] forKey:@"NSLocalizedDescription"];
    return [NSError errorWithDomain:PHPhotosErrorDomain code:code userInfo:info];
}
static id file_url(const char *path) { return [(id)objc_getClass("NSURL") fileURLWithPath:[NSString stringWithUTF8String:path]]; }
/* invoca um bloco ObjC: o ponteiro da função fica em +16 */
#define BLK(b, T) (*(T *)((char *)(b) + 16))
static id blk_copy(id b) { extern void *_Block_copy(const void *); return b ? (id)_Block_copy(b) : nil; }
static void blk_release(id b) { extern void _Block_release(const void *); if (b) _Block_release(b); }

static int copy_file(const char *from, const char *to) {
    FILE *in = fopen(from, "rb"), *out;
    char buf[65536];
    size_t k;
    if (!in) return 0;
    out = fopen(to, "wb");
    if (!out) { fclose(in); return 0; }
    while ((k = fread(buf, 1, sizeof buf, in)) > 0) if (fwrite(buf, 1, k, out) != k) { fclose(in); fclose(out); return 0; }
    fclose(in);
    return fclose(out) == 0;
}
static const char *base_name(const char *p) { const char *s = strrchr(p, '/'); return s ? s + 1 : p; }
static const char *uti_of_name(const char *name) {
    const char *dot = strrchr(name, '.');
    if (!dot) return "public.data";
    if (!strcasecmp(dot, ".jpg") || !strcasecmp(dot, ".jpeg")) return "public.jpeg";
    if (!strcasecmp(dot, ".png")) return "public.png";
    if (!strcasecmp(dot, ".heic")) return "public.heic";
    if (!strcasecmp(dot, ".gif")) return "com.compuserve.gif";
    if (!strcasecmp(dot, ".mov")) return "com.apple.quicktime-movie";
    if (!strcasecmp(dot, ".mp4") || !strcasecmp(dot, ".m4v")) return "public.mpeg-4";
    return "public.data";
}

/* ---------------------------------------------------------------- opções (sacos de propriedades) */
@interface PHImageRequestOptions : NSObject {
   @public
    NSInteger version, deliveryMode, resizeMode;
    BOOL networkAccessAllowed, synchronous, allowSecondaryDegradedImage;
    id progressHandler;
}
@end
@implementation PHImageRequestOptions
- (void)dealloc { blk_release(progressHandler); [super dealloc]; }
- (NSInteger)version { return version; } - (void)setVersion:(NSInteger)v { version = v; }
- (NSInteger)deliveryMode { return deliveryMode; } - (void)setDeliveryMode:(NSInteger)v { deliveryMode = v; }
- (NSInteger)resizeMode { return resizeMode; } - (void)setResizeMode:(NSInteger)v { resizeMode = v; }
- (BOOL)isNetworkAccessAllowed { return networkAccessAllowed; } - (void)setNetworkAccessAllowed:(BOOL)v { networkAccessAllowed = v; }
- (BOOL)isSynchronous { return synchronous; } - (void)setSynchronous:(BOOL)v { synchronous = v; }
- (BOOL)allowSecondaryDegradedImage { return allowSecondaryDegradedImage; } - (void)setAllowSecondaryDegradedImage:(BOOL)v { allowSecondaryDegradedImage = v; }
- (id)progressHandler { return progressHandler; } - (void)setProgressHandler:(id)h { blk_release(progressHandler); progressHandler = blk_copy(h); }
@end
@interface PHVideoRequestOptions : PHImageRequestOptions
@end
@implementation PHVideoRequestOptions
@end
@interface PHLivePhotoRequestOptions : PHImageRequestOptions
@end
@implementation PHLivePhotoRequestOptions
@end
@interface PHContentEditingInputRequestOptions : PHImageRequestOptions
@end
@implementation PHContentEditingInputRequestOptions
@end
@interface PHAssetResourceRequestOptions : PHImageRequestOptions
@end
@implementation PHAssetResourceRequestOptions
@end
@interface PHAssetResourceCreationOptions : NSObject {
   @public
    NSString *originalFilename, *uniformTypeIdentifier;
    BOOL shouldMoveFile;
}
@end
@implementation PHAssetResourceCreationOptions
- (void)dealloc { [originalFilename release]; [uniformTypeIdentifier release]; [super dealloc]; }
- (NSString *)originalFilename { return originalFilename; }
- (void)setOriginalFilename:(NSString *)v { [originalFilename release]; originalFilename = [v copy]; }
- (NSString *)uniformTypeIdentifier { return uniformTypeIdentifier; }
- (void)setUniformTypeIdentifier:(NSString *)v { [uniformTypeIdentifier release]; uniformTypeIdentifier = [v copy]; }
- (BOOL)shouldMoveFile { return shouldMoveFile; } - (void)setShouldMoveFile:(BOOL)v { shouldMoveFile = v; }
@end

/* ---------------------------------------------------------------- PHImageManager */
@interface PHImageManager : NSObject {
   @public
    int next_id;
    NSMutableArray *cancelled;
}
@end
static PHImageManager *g_mgr;
static id image_object(void *cg) {
    id uiimage = objc_getClass("UIImage");
    if (uiimage) return [uiimage imageWithCGImage:(id)cg];
    id nsimage = objc_getClass("NSImage");
    if (!nsimage) return nil;
    PHSize s = {0, 0};
    return [[[nsimage alloc] initWithCGImage:(id)cg size:*(NSSize *)&s] autorelease];
}
static void *decode_cg(const photos_asset *a, PHSize target) {
    void *src = CGImageSourceCreateWithURL(file_url(a->path), 0);
    if (!src) return 0;
    void *img = 0;
    double maxpx = target.width > target.height ? target.width : target.height;
    if (target.width > 0 && target.height > 0 && maxpx < (a->pixel_w > a->pixel_h ? a->pixel_w : a->pixel_h)) {
        NSDictionary *o = [NSDictionary dictionaryWithObjectsAndKeys:
            [NSNumber numberWithBool:YES], @"kCGImageSourceCreateThumbnailFromImageAlways",
            [NSNumber numberWithLongLong:(long long)(maxpx + 0.5)], @"kCGImageSourceThumbnailMaxPixelSize", nil];
        img = CGImageSourceCreateThumbnailAtIndex(src, 0, o);
    }
    if (!img) img = CGImageSourceCreateImageAtIndex(src, 0, 0);
    CFRelease(src);
    return img;
}
@implementation PHImageManager
+ (instancetype)defaultManager { if (!g_mgr) g_mgr = [PHImageManager new]; return g_mgr; }
- (void)dealloc { [cancelled release]; [super dealloc]; }
- (int)requestImageForAsset:(PHAsset *)asset targetSize:(PHSize)size contentMode:(NSInteger)mode options:(PHImageRequestOptions *)o resultHandler:(id)handler {
    (void)mode; (void)o;
    int rid = ++next_id;
    void *cg = asset ? decode_cg(&asset->a, size) : 0;
    id img = cg ? image_object(cg) : nil;
    if (cg) CGImageRelease(cg);
    NSMutableDictionary *info = [NSMutableDictionary dictionary];
    [info setObject:[NSNumber numberWithBool:NO] forKey:PHImageResultIsDegradedKey];
    [info setObject:[NSNumber numberWithBool:NO] forKey:PHImageResultIsInCloudKey];
    [info setObject:[NSNumber numberWithInt:rid] forKey:PHImageResultRequestIDKey];
    if (!img) [info setObject:ph_error(-1, "Photos[mOS]: o arquivo do asset não decodifica (o ImageIO lê PNG e JPEG)") forKey:PHImageErrorKey];
    if (handler) BLK(handler, void (*)(id, id, id))(handler, img, info);
    return rid;
}
- (void)cancelImageRequest:(int)rid { (void)rid; }   /* a entrega é síncrona: nada fica pendente */
static NSData *asset_bytes(PHAsset *asset) {
    return asset ? [(id)objc_getClass("NSData") dataWithContentsOfFile:[NSString stringWithUTF8String:asset->a.path]] : nil;
}
/* handler(imageData, dataUTI, orientation, info) */
- (int)requestImageDataAndOrientationForAsset:(PHAsset *)asset options:(PHImageRequestOptions *)o resultHandler:(id)handler {
    (void)o;
    int rid = ++next_id;
    NSData *d = asset_bytes(asset);
    NSString *uti = asset ? [NSString stringWithUTF8String:uti_of_name(asset->a.path)] : nil;
    NSDictionary *info = d ? [NSDictionary dictionary] : [NSDictionary dictionaryWithObject:ph_error(-1, "Photos[mOS]: arquivo do asset ilegível") forKey:PHImageErrorKey];
    if (handler) BLK(handler, void (*)(id, id, id, unsigned, id))(handler, d, uti, 1, info);
    return rid;
}
/* handler(imageData, dataUTI, UIImageOrientation, info): 0 = up */
- (int)requestImageDataForAsset:(PHAsset *)asset options:(PHImageRequestOptions *)o resultHandler:(id)handler {
    (void)o;
    int rid = ++next_id;
    NSData *d = asset_bytes(asset);
    NSString *uti = asset ? [NSString stringWithUTF8String:uti_of_name(asset->a.path)] : nil;
    NSDictionary *info = d ? [NSDictionary dictionary] : [NSDictionary dictionaryWithObject:ph_error(-1, "Photos[mOS]: arquivo do asset ilegível") forKey:PHImageErrorKey];
    if (handler) BLK(handler, void (*)(id, id, id, long, id))(handler, d, uti, 0, info);
    return rid;
}
/* handler(AVAsset, AVAudioMix, info): AVURLAsset do arquivo */
- (int)requestAVAssetForVideo:(PHAsset *)asset options:(PHVideoRequestOptions *)o resultHandler:(id)handler {
    (void)o;
    int rid = ++next_id;
    id cls = objc_getClass("AVURLAsset");
    id av = (asset && asset->a.media == PHOTOS_MEDIA_VIDEO && cls) ? [cls URLAssetWithURL:file_url(asset->a.path) options:nil] : nil;
    NSDictionary *info = av ? [NSDictionary dictionary] : [NSDictionary dictionaryWithObject:ph_error(-1, "Photos[mOS]: o asset não é vídeo") forKey:PHImageErrorKey];
    if (handler) BLK(handler, void (*)(id, id, id, id))(handler, av, nil, info);
    return rid;
}
/* Live Photo precisa de HEIC + MOV pareados; a biblioteca do mOS não guarda o par */
- (int)requestLivePhotoForAsset:(PHAsset *)asset targetSize:(PHSize)size contentMode:(NSInteger)mode options:(id)o resultHandler:(id)handler {
    (void)asset; (void)size; (void)mode; (void)o;
    int rid = ++next_id;
    NSDictionary *info = [NSDictionary dictionaryWithObject:ph_error(3300, "Photos[mOS]: sem Live Photo nesta biblioteca") forKey:PHLivePhotoInfoErrorKey];
    if (handler) BLK(handler, void (*)(id, id, id))(handler, nil, info);
    return rid;
}
@end

/* O cache não muda o resultado: cada pedido decodifica o arquivo de novo. */
@interface PHCachingImageManager : PHImageManager {
    BOOL allowsHQ;
}
@end
@implementation PHCachingImageManager
- (void)startCachingImagesForAssets:(NSArray *)a targetSize:(PHSize)s contentMode:(NSInteger)m options:(id)o { (void)a; (void)s; (void)m; (void)o; }
- (void)stopCachingImagesForAssets:(NSArray *)a targetSize:(PHSize)s contentMode:(NSInteger)m options:(id)o { (void)a; (void)s; (void)m; (void)o; }
- (void)stopCachingImagesForAllAssets {}
- (BOOL)allowsCachingHighQualityImages { return allowsHQ; }
- (void)setAllowsCachingHighQualityImages:(BOOL)v { allowsHQ = v; }
@end

/* ---------------------------------------------------------------- PHAssetResource: o arquivo do asset */
@interface PHAssetResource : NSObject {
   @public
    photos_asset a;
}
@end
@implementation PHAssetResource
+ (NSArray *)assetResourcesForAsset:(PHAsset *)asset {
    if (!asset) return [NSArray array];
    PHAssetResource *r = [[PHAssetResource new] autorelease];
    r->a = asset->a;
    return [NSArray arrayWithObject:r];
}
- (NSInteger)type { return a.media == PHOTOS_MEDIA_VIDEO ? 2 : 1; }   /* PHAssetResourceTypeVideo / Photo */
- (NSString *)originalFilename { return [NSString stringWithUTF8String:base_name(a.path)]; }
- (NSString *)uniformTypeIdentifier { return [NSString stringWithUTF8String:uti_of_name(a.path)]; }
- (NSString *)assetLocalIdentifier { return [NSString stringWithUTF8String:a.ident]; }
@end
@interface PHAssetResourceManager : NSObject
@end
static PHAssetResourceManager *g_rmgr;
@implementation PHAssetResourceManager
+ (instancetype)defaultManager { if (!g_rmgr) g_rmgr = [PHAssetResourceManager new]; return g_rmgr; }
/* dataReceived(NSData) em pedaços de 1 MiB, depois completion(NSError) */
- (int)requestDataForAssetResource:(PHAssetResource *)r options:(id)o dataReceivedHandler:(id)got completionHandler:(id)done {
    (void)o;
    FILE *f = r ? fopen(r->a.path, "rb") : 0;
    id err = f ? nil : ph_error(-1, "Photos[mOS]: arquivo do recurso ilegível");
    if (f) {
        static char buf[1 << 20];
        size_t k;
        while ((k = fread(buf, 1, sizeof buf, f)) > 0)
            if (got) BLK(got, void (*)(id, id))(got, [(id)objc_getClass("NSData") dataWithBytes:buf length:k]);
        fclose(f);
    }
    if (done) BLK(done, void (*)(id, id))(done, err);
    return 1;
}
- (void)cancelDataRequest:(int)rid { (void)rid; }
- (void)writeDataForAssetResource:(PHAssetResource *)r toFile:(id)url options:(id)o completionHandler:(id)done {
    (void)o;
    const char *to = url ? [[url path] UTF8String] : 0;
    id err = (r && to && copy_file(r->a.path, to)) ? nil : ph_error(-1, "Photos[mOS]: não consegui copiar o recurso");
    if (done) BLK(done, void (*)(id, id))(done, err);
}
@end

/* ---------------------------------------------------------------- coleções: a biblioteca é o álbum "Todas" */
@interface PHAssetCollection : NSObject {
   @public
    NSInteger type, subtype;
}
@end
@implementation PHAssetCollection
+ (PHFetchResult *)fetchAssetCollectionsWithType:(NSInteger)type subtype:(NSInteger)subtype options:(id)o {
    (void)o;
    PHFetchResult *r = [[PHFetchResult new] autorelease];
    /* PHAssetCollectionTypeSmartAlbum = 2, subtipo smartAlbumUserLibrary = 209 ou "qualquer" (0x7FFFFFFFFFFFFFFF) */
    if (type == 2 && (subtype == 209 || subtype == 0x7FFFFFFFFFFFFFFFL)) {
        PHAssetCollection *c = [[PHAssetCollection new] autorelease];
        c->type = 2;
        c->subtype = 209;
        r->objs = [[NSArray arrayWithObject:c] retain];
    } else {
        r->objs = [[NSArray array] retain];
    }
    return r;
}
+ (PHFetchResult *)fetchAssetCollectionsWithLocalIdentifiers:(NSArray *)ids options:(id)o {
    (void)ids;
    return [self fetchAssetCollectionsWithType:2 subtype:209 options:o];
}
- (NSString *)localizedTitle { return @"Todas as Fotos"; }
- (NSString *)localIdentifier { return @"mOS-library/all"; }
- (NSInteger)assetCollectionType { return type; }
- (NSInteger)assetCollectionSubtype { return subtype; }
- (NSUInteger)estimatedAssetCount { return [[PHAsset fetchAssetsWithOptions:nil] count]; }
- (BOOL)canPerformEditOperation:(NSInteger)op { (void)op; return NO; }
@end
@implementation PHAsset (Collections)
+ (PHFetchResult *)fetchAssetsInAssetCollection:(PHAssetCollection *)c options:(PHFetchOptions *)o { (void)c; return [self fetchAssetsWithOptions:o]; }
+ (PHFetchResult *)fetchAssetsWithMediaType:(NSInteger)media options:(PHFetchOptions *)o {
    PHFetchResult *all = [self fetchAssetsWithOptions:o];
    NSUInteger k = 0;
    for (NSUInteger i = 0; i < all->n; i++) if (all->items[i].media == media) all->items[k++] = all->items[i];
    all->n = k;
    return all;
}
+ (PHFetchResult *)fetchAssetsWithLocalIdentifiers:(NSArray *)ids options:(PHFetchOptions *)o {
    PHFetchResult *all = [self fetchAssetsWithOptions:o];
    NSUInteger k = 0;
    for (NSUInteger i = 0; i < all->n; i++) {
        NSString *s = [NSString stringWithUTF8String:all->items[i].ident];
        if ([ids containsObject:s]) all->items[k++] = all->items[i];
    }
    all->n = k;
    return all;
}
@end
@interface PHCollectionList : NSObject
@end
@implementation PHCollectionList
+ (PHFetchResult *)fetchTopLevelUserCollectionsWithOptions:(id)o { (void)o; PHFetchResult *r = [[PHFetchResult new] autorelease]; r->objs = [[NSArray array] retain]; return r; }
+ (PHFetchResult *)fetchCollectionListsWithType:(NSInteger)t subtype:(NSInteger)s options:(id)o { (void)t; (void)s; return [self fetchTopLevelUserCollectionsWithOptions:o]; }
@end

/* ---------------------------------------------------------------- mudanças */
@interface PHObjectChangeDetails : NSObject {
   @public
    id before, after;
}
@end
@implementation PHObjectChangeDetails
- (void)dealloc { [before release]; [after release]; [super dealloc]; }
- (id)objectBeforeChanges { return before; }
- (id)objectAfterChanges { return after; }
- (BOOL)assetContentChanged { return NO; }
- (BOOL)objectWasDeleted { return after == nil; }
@end
@interface PHFetchResultChangeDetails : NSObject {
   @public
    PHFetchResult *before, *after;
    id removed, inserted;        /* NSMutableIndexSet */
}
@end
static NSString *ident_at(PHFetchResult *r, NSUInteger i) {
    id o = [r objectAtIndex:i];
    return [o respondsToSelector:@selector(localIdentifier)] ? [o localIdentifier] : nil;
}
static BOOL result_has(PHFetchResult *r, NSString *ident) {
    for (NSUInteger i = 0; ident && i < [r count]; i++) if ([ident_at(r, i) isEqual:ident]) return YES;
    return NO;
}
@implementation PHFetchResultChangeDetails
+ (instancetype)changeDetailsFromFetchResult:(PHFetchResult *)from toFetchResult:(PHFetchResult *)to changedObjects:(NSArray *)changed {
    (void)changed;
    PHFetchResultChangeDetails *d = [[PHFetchResultChangeDetails new] autorelease];
    id set = objc_getClass("NSMutableIndexSet");
    d->before = [from retain];
    d->after = [to retain];
    d->removed = [[set indexSet] retain];
    d->inserted = [[set indexSet] retain];
    for (NSUInteger i = 0; i < [from count]; i++) if (!result_has(to, ident_at(from, i))) [d->removed addIndex:i];
    for (NSUInteger i = 0; i < [to count]; i++) if (!result_has(from, ident_at(to, i))) [d->inserted addIndex:i];
    return d;
}
- (void)dealloc { [before release]; [after release]; [removed release]; [inserted release]; [super dealloc]; }
- (PHFetchResult *)fetchResultBeforeChanges { return before; }
- (PHFetchResult *)fetchResultAfterChanges { return after; }
- (BOOL)hasIncrementalChanges { return YES; }
- (id)removedIndexes { return [removed count] ? removed : nil; }
- (id)insertedIndexes { return [inserted count] ? inserted : nil; }
- (id)changedIndexes { return nil; }
- (BOOL)hasMoves { return NO; }
- (void)enumerateMovesWithBlock:(id)b { (void)b; }
- (NSArray *)removedObjects { NSMutableArray *a = [NSMutableArray array]; for (NSUInteger i = 0; i < [before count]; i++) if ([removed containsIndex:i]) [a addObject:[before objectAtIndex:i]]; return a; }
- (NSArray *)insertedObjects { NSMutableArray *a = [NSMutableArray array]; for (NSUInteger i = 0; i < [after count]; i++) if ([inserted containsIndex:i]) [a addObject:[after objectAtIndex:i]]; return a; }
- (NSArray *)changedObjects { return [NSArray array]; }
@end
/* PHChange: o que photoLibraryDidChange: recebe; refaz a busca com as mesmas opções */
@interface PHChange : NSObject
@end
@implementation PHChange
- (PHFetchResultChangeDetails *)changeDetailsForFetchResult:(PHFetchResult *)old {
    if (!old || old->objs) return nil;
    PHFetchResult *now = [PHAsset fetchAssetsWithOptions:old->opts];
    PHFetchResultChangeDetails *d = [PHFetchResultChangeDetails changeDetailsFromFetchResult:old toFetchResult:now changedObjects:nil];
    return ([d removedIndexes] || [d insertedIndexes]) ? d : nil;
}
- (PHObjectChangeDetails *)changeDetailsForObject:(id)o { (void)o; return nil; }
@end

/* ---------------------------------------------------------------- pedidos de criação dentro de performChanges */
@interface PHObjectPlaceholder : NSObject {
   @public
    NSString *ident;
}
@end
@implementation PHObjectPlaceholder
- (void)dealloc { [ident release]; [super dealloc]; }
- (NSString *)localIdentifier { return ident; }
@end
@interface PHAssetChangeRequest : NSObject {
   @public
    NSString *src;               /* arquivo de origem */
    BOOL move;
    NSString *name;
    PHObjectPlaceholder *ph;
    BOOL favorite, hidden;
    id creationDate;
}
@end
static NSMutableArray *g_pending;        /* pedidos abertos pelo bloco de performChanges */
static int g_in_changes;
static PHAssetChangeRequest *pending_new(Class k) {
    if (!g_in_changes) return nil;          /* fora de performChanges a Apple dispara exceção; aqui só recusa */
    PHAssetChangeRequest *r = [[k new] autorelease];
    if (!g_pending) g_pending = [NSMutableArray new];
    [g_pending addObject:r];
    return r;
}
@implementation PHAssetChangeRequest
- (void)dealloc { [src release]; [name release]; [ph release]; [creationDate release]; [super dealloc]; }
+ (instancetype)creationRequestForAssetFromImageAtFileURL:(id)url {
    PHAssetChangeRequest *r = pending_new(self);
    if (r) r->src = [[url path] copy];
    return r;
}
+ (instancetype)creationRequestForAssetFromVideoAtFileURL:(id)url { return [self creationRequestForAssetFromImageAtFileURL:url]; }
+ (instancetype)changeRequestForAsset:(PHAsset *)a { (void)a; return pending_new(self); }
- (PHObjectPlaceholder *)placeholderForCreatedAsset {
    if (!ph) {
        ph = [PHObjectPlaceholder new];
        ph->ident = [[NSString stringWithFormat:@"mOS-pendente/%p", (void *)self] retain];
    }
    return ph;
}
- (void)setFavorite:(BOOL)v { favorite = v; }
- (void)setHidden:(BOOL)v { hidden = v; }
- (void)setCreationDate:(id)d { [creationDate release]; creationDate = [d retain]; }
@end
@interface PHAssetCreationRequest : PHAssetChangeRequest
@end
@implementation PHAssetCreationRequest
+ (instancetype)creationRequestForAsset { return (PHAssetCreationRequest *)pending_new(self); }
+ (BOOL)supportsAssetResourceTypes:(NSArray *)types { (void)types; return YES; }
- (void)addResourceWithType:(NSInteger)type fileURL:(id)url options:(PHAssetResourceCreationOptions *)o {
    (void)type;
    [src release];
    src = [[url path] copy];
    if (o) { move = o->shouldMoveFile; [name release]; name = [o->originalFilename copy]; }
}
- (void)addResourceWithType:(NSInteger)type data:(NSData *)data options:(PHAssetResourceCreationOptions *)o {
    char tmp[256];
    snprintf(tmp, sizeof tmp, "/tmp/mos-photos-%ld-%p.bin", (long)time(0), (void *)self);
    FILE *f = fopen(tmp, "wb");
    if (!f) return;
    fwrite([data bytes], 1, [data length], f);
    fclose(f);
    [self addResourceWithType:type fileURL:file_url(tmp) options:o];
    move = YES;
}
@end
@interface PHAssetCollectionChangeRequest : NSObject
@end
@implementation PHAssetCollectionChangeRequest
/* álbuns do usuário não existem na biblioteca de pasta do mOS: pedir um devolve nil */
+ (instancetype)creationRequestForAssetCollectionWithTitle:(NSString *)t { (void)t; return nil; }
+ (instancetype)changeRequestForAssetCollection:(PHAssetCollection *)c { (void)c; return nil; }
- (void)addAssets:(id)a { (void)a; }
@end

/* grava cada pedido como arquivo na pasta da biblioteca: <nome original> ou mOS-<hora>-<n>.<ext> */
static int commit_pending(id *err) {
    char root[512], dst[1024];
    int ok = 1, k = 0;
    library_path(root, sizeof root);
    mkdir(root, 0755);
    for (PHAssetChangeRequest *r in g_pending) {
        if (!r->src) continue;                    /* pedido de mudança: favorito/oculto não têm onde ficar */
        const char *from = [r->src UTF8String];
        const char *ext = strrchr(base_name(from), '.');
        if (r->name) snprintf(dst, sizeof dst, "%s/%s", root, [r->name UTF8String]);
        else snprintf(dst, sizeof dst, "%s/mOS-%ld-%d%s", root, (long)time(0), k, ext ? ext : "");
        struct stat st;
        if (stat(dst, &st) == 0) snprintf(dst + strlen(dst), sizeof dst - strlen(dst), "-%d", ++k);
        if (!copy_file(from, dst)) { ok = 0; continue; }
        if (r->move) remove(from);
        if (r->ph) { [r->ph->ident release]; r->ph->ident = [[NSString stringWithUTF8String:dst] retain]; }
        k++;
    }
    [g_pending removeAllObjects];
    if (!ok && err) *err = ph_error(-1, "Photos[mOS]: não consegui gravar o arquivo na biblioteca");
    return ok;
}

static NSMutableArray *g_observers;
@implementation PHPhotoLibrary (Changes)
- (void)registerChangeObserver:(id)o { if (!g_observers) g_observers = [NSMutableArray new]; if (o) [g_observers addObject:o]; }
- (void)unregisterChangeObserver:(id)o { [g_observers removeObject:o]; }
- (BOOL)performChangesAndWait:(id)block error:(id *)err {
    if (err) *err = nil;
    if ([PHPhotoLibrary authorizationStatus] != PHOTOS_AUTH_AUTHORIZED) { if (err) *err = ph_error(3311, "Photos[mOS]: sem acesso à biblioteca"); return NO; }
    g_in_changes = 1;
    if (block) BLK(block, void (*)(id))(block);
    g_in_changes = 0;
    BOOL ok = commit_pending(err) ? YES : NO;
    PHChange *ch = [[PHChange new] autorelease];
    for (id o in [[g_observers copy] autorelease]) if ([o respondsToSelector:@selector(photoLibraryDidChange:)]) [o photoLibraryDidChange:ch];
    return ok;
}
- (void)performChanges:(id)block completionHandler:(id)done {
    id err = nil;
    BOOL ok = [self performChangesAndWait:block error:&err];
    if (done) BLK(done, void (*)(id, BOOL, id))(done, ok, err);
}
@end

/* ---------------------------------------------------------------- Live Photo e edição */
@interface PHLivePhoto : NSObject {
    PHSize size;
}
@end
@implementation PHLivePhoto
- (PHSize)size { return size; }
+ (int)requestLivePhotoWithResourceFileURLs:(NSArray *)urls placeholderImage:(id)ph targetSize:(PHSize)s contentMode:(NSInteger)m resultHandler:(id)h {
    (void)urls; (void)ph; (void)s; (void)m;
    NSDictionary *info = [NSDictionary dictionaryWithObject:ph_error(3300, "Photos[mOS]: o mOS não monta Live Photo (HEIC + MOV)") forKey:PHLivePhotoInfoErrorKey];
    if (h) BLK(h, void (*)(id, id, id))(h, nil, info);
    return 0;
}
+ (void)cancelLivePhotoRequestWithRequestID:(int)rid { (void)rid; }
@end
@interface PHAdjustmentData : NSObject {
    NSString *fmt, *ver;
    NSData *data;
}
@end
@implementation PHAdjustmentData
- (instancetype)initWithFormatIdentifier:(NSString *)f formatVersion:(NSString *)v data:(NSData *)d {
    self = [super init];
    if (self) { fmt = [f copy]; ver = [v copy]; data = [d retain]; }
    return self;
}
- (void)dealloc { [fmt release]; [ver release]; [data release]; [super dealloc]; }
- (NSString *)formatIdentifier { return fmt; }
- (NSString *)formatVersion { return ver; }
- (NSData *)data { return data; }
@end
@interface PHContentEditingOutput : NSObject {
    PHAdjustmentData *adj;
    id url;
}
@end
@implementation PHContentEditingOutput
- (instancetype)initWithContentEditingInput:(id)input {
    (void)input;
    self = [super init];
    if (self) {
        char p[256];
        snprintf(p, sizeof p, "/tmp/mos-photos-edit-%ld-%p.jpg", (long)time(0), (void *)self);
        url = [file_url(p) retain];
    }
    return self;
}
- (void)dealloc { [adj release]; [url release]; [super dealloc]; }
- (PHAdjustmentData *)adjustmentData { return adj; }
- (void)setAdjustmentData:(PHAdjustmentData *)a { [adj release]; adj = [a retain]; }
- (id)renderedContentURL { return url; }
@end
