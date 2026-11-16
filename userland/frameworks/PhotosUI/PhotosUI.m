/*
 * mOS — PhotosUI.framework do Mac Catalyst (docs/photosui.md). Reimplementação própria, com os nomes do SDK da Apple:
 *   PHPickerFilter (imagens, vídeos, Live Photos, panoramas, capturas de tela... e as combinações any/all/not),
 *   PHPickerConfiguration, PHPickerResult, PHPickerViewController (UIViewController do UIKit do mOS) e PHLivePhotoView.
 * O seletor mostra a biblioteca do Photos.framework do mOS (a pasta ~/Imagens, docs/photos.md) numa grade própria:
 * toque seleciona (com número na seleção ordenada), "Adicionar" entrega os resultados ao delegate, "Cancelar" entrega
 * uma lista vazia, como o PHPicker da Apple. Com limite 1 e seleção padrão, o toque já conclui.
 * Cada resultado leva um NSItemProvider de arquivo (_PHPickerItemProvider): tipos registrados pelo UTI da extensão,
 * dados, cópia temporária do arquivo (loadFileRepresentation), arquivo no lugar e UIImage.
 * O Photos.framework é chamado pelo runtime (objc_getClass), sem ligação direta: o PhotosUI carrega sem ele.
 */
#import "../UIKit/UIKitMOS.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <strings.h>
#include <math.h>

extern id objc_getClass(const char *);
extern void objc_setAssociatedObject(id, const void *, id, unsigned long);
extern id objc_getAssociatedObject(id, const void *);
typedef struct CGContext *CGContextRef;
extern CGContextRef UIGraphicsGetCurrentContext(void);
extern void CGContextSetFillColorWithColor(CGContextRef, void *);
extern void CGContextFillRect(CGContextRef, CGRect);
extern void CGContextFillEllipseInRect(CGContextRef, CGRect);
extern void CGContextStrokeEllipseInRect(CGContextRef, CGRect);
extern void CGContextSetStrokeColorWithColor(CGContextRef, void *);
extern void CGContextSetLineWidth(CGContextRef, CGFloat);
extern void CGContextSaveGState(CGContextRef);
extern void CGContextRestoreGState(CGContextRef);
extern void CGContextClipToRect(CGContextRef, CGRect);
extern void *CGImageSourceCreateWithURL(id url, id opts);
extern void *CGImageSourceCreateImageAtIndex(void *src, size_t i, id opts);
extern void *CGImageSourceCreateThumbnailAtIndex(void *src, size_t i, id opts);
extern void CFRelease(const void *);
extern void CGImageRelease(void *);

/* ---- o que os cabeçalhos mínimos do SDK do mOS não declaram (classes do libSystem) */
@interface NSObject (MOSPhotosUIRuntime)
+ (id)fileURLWithPath:(NSString *)p;
- (NSString *)path;
- (NSString *)lastPathComponent;
- (NSString *)pathExtension;
- (NSString *)stringByDeletingPathExtension;
- (NSString *)lowercaseString;
- (BOOL)hasPrefix:(NSString *)s;
+ (id)defaultManager;
- (BOOL)copyItemAtPath:(NSString *)a toPath:(NSString *)b error:(id *)e;
- (BOOL)removeItemAtPath:(NSString *)p error:(id *)e;
- (BOOL)fileExistsAtPath:(NSString *)p;
+ (id)dataWithContentsOfFile:(NSString *)p;
+ (id)errorWithDomain:(NSString *)d code:(NSInteger)c userInfo:(NSDictionary *)u;
- (id)fetchAssetsWithOptions:(id)o;
- (NSUInteger)count;
- (id)objectAtIndex:(NSUInteger)i;
- (NSString *)localIdentifier;
- (NSInteger)mediaType;
- (NSUInteger)mediaSubtypes;
- (double)duration;
+ (id)fetchAssetsWithLocalIdentifiers:(NSArray *)ids options:(id)o;
+ (id)sharedPhotoLibrary;
@end

/* ---- métodos do UIKit do mOS que o UIKitMOS.h não declara */
@interface UIView (MOSPhotosUIDecl)
- (void)setContentSize:(CGSize)s;
- (void)setContentOffset:(CGPoint)p;
- (void)setTitle:(NSString *)t forState:(NSUInteger)s;
- (void)setEnabled:(BOOL)e;
- (void)setText:(NSString *)t;
- (void)setTextAlignment:(NSInteger)a;
- (void)setFont:(UIFont *)f;
- (void)touchesEnded:(NSSet *)t withEvent:(id)e;
@end
@interface UIImage (MOSPhotosUIDecl)
- (void)drawInRect:(CGRect)r;
@end

/* ---- tipo do arquivo pela extensão (os UTIs públicos da Apple) */
static const char *pu_uti_for_ext(const char *e) {
    static const struct { const char *ext, *uti; } map[] = {
        {"jpg", "public.jpeg"}, {"jpeg", "public.jpeg"}, {"png", "public.png"}, {"gif", "com.compuserve.gif"},
        {"heic", "public.heic"}, {"heif", "public.heif"}, {"tif", "public.tiff"}, {"tiff", "public.tiff"},
        {"bmp", "com.microsoft.bmp"}, {"webp", "org.webmproject.webp"}, {"mov", "com.apple.quicktime-movie"},
        {"mp4", "public.mpeg-4"}, {"m4v", "com.apple.m4v-video"}, {"avi", "public.avi"}, {"dng", "com.adobe.raw-image"},
    };
    for (size_t i = 0; i < sizeof map / sizeof map[0]; i++) if (!strcasecmp(e, map[i].ext)) return map[i].uti;
    return "public.data";
}
static BOOL pu_is_video_uti(const char *u) { return !strcmp(u, "com.apple.quicktime-movie") || !strcmp(u, "public.mpeg-4") || !strcmp(u, "com.apple.m4v-video") || !strcmp(u, "public.avi"); }
/* conformidade simples da árvore de UTIs públicos: o tipo, public.image/public.movie, public.data, public.item */
static BOOL pu_conforms(const char *have, const char *want) {
    if (!strcmp(have, want)) return YES;
    if (!strcmp(want, "public.item") || !strcmp(want, "public.data") || !strcmp(want, "public.content")) return YES;
    BOOL vid = pu_is_video_uti(have);
    if (!strcmp(want, "public.image")) return !vid && strcmp(have, "public.data") != 0;
    if (!strcmp(want, "public.movie") || !strcmp(want, "public.audiovisual-content") || !strcmp(want, "public.video")) return vid;
    return NO;
}
static NSString *pu_str(const char *s) { return [NSString stringWithUTF8String:s]; }

/* ================================================================ PHPickerFilter */
enum { PUF_IMAGES = 1, PUF_VIDEOS, PUF_LIVE, PUF_PANORAMA, PUF_SCREENSHOT, PUF_SLOMO, PUF_TIMELAPSE, PUF_DEPTH,
       PUF_BURSTS, PUF_CINEMATIC, PUF_SPATIAL, PUF_SCREENREC, PUF_ANY = 100, PUF_ALL, PUF_NOT };
@interface PHPickerFilter : NSObject <NSCopying> {
@public
    int _kind;
    NSArray *_subs;
}
@end
/* subtipos do PHAsset (PHAssetMediaSubtype) */
enum { SUB_PANORAMA = 1 << 0, SUB_HDR = 1 << 1, SUB_SCREENSHOT = 1 << 2, SUB_LIVE = 1 << 3, SUB_DEPTH = 1 << 4,
       SUB_STREAMED = 1 << 16, SUB_HFR = 1 << 17, SUB_TIMELAPSE = 1 << 18, SUB_CINEMATIC = 1 << 21, SUB_SPATIAL = 1 << 22 };
static BOOL pu_filter_match(PHPickerFilter *f, NSInteger media, NSUInteger sub) {
    if (!f) return YES;
    switch (f->_kind) {
    case PUF_IMAGES: return media == 1;
    case PUF_VIDEOS: return media == 2;
    case PUF_LIVE: return media == 1 && (sub & SUB_LIVE);
    case PUF_PANORAMA: return media == 1 && (sub & SUB_PANORAMA);
    case PUF_SCREENSHOT: return media == 1 && (sub & SUB_SCREENSHOT);
    case PUF_DEPTH: return media == 1 && (sub & SUB_DEPTH);
    case PUF_SLOMO: return media == 2 && (sub & SUB_HFR);
    case PUF_TIMELAPSE: return media == 2 && (sub & SUB_TIMELAPSE);
    case PUF_CINEMATIC: return media == 2 && (sub & SUB_CINEMATIC);
    case PUF_SPATIAL: return (sub & SUB_SPATIAL) != 0;
    case PUF_BURSTS: return NO;        /* a biblioteca do mOS não agrupa rajadas */
    case PUF_SCREENREC: return NO;     /* nem marca gravações de tela */
    case PUF_ANY: for (PHPickerFilter *s in f->_subs) if (pu_filter_match(s, media, sub)) return YES; return NO;
    case PUF_ALL: for (PHPickerFilter *s in f->_subs) if (!pu_filter_match(s, media, sub)) return NO; return YES;
    case PUF_NOT: return f->_subs.count ? !pu_filter_match([f->_subs objectAtIndex:0], media, sub) : YES;
    }
    return NO;
}
@implementation PHPickerFilter
+ (PHPickerFilter *)_k:(int)k subs:(NSArray *)s { PHPickerFilter *f = [[[self alloc] init] autorelease]; f->_kind = k; f->_subs = [s copy]; return f; }
- (void)dealloc { [_subs release]; [super dealloc]; }
- (id)copyWithZone:(NSZone *)z { (void)z; return [self retain]; }
+ (PHPickerFilter *)imagesFilter { return [self _k:PUF_IMAGES subs:nil]; }
+ (PHPickerFilter *)videosFilter { return [self _k:PUF_VIDEOS subs:nil]; }
+ (PHPickerFilter *)livePhotosFilter { return [self _k:PUF_LIVE subs:nil]; }
+ (PHPickerFilter *)panoramasFilter { return [self _k:PUF_PANORAMA subs:nil]; }
+ (PHPickerFilter *)screenshotsFilter { return [self _k:PUF_SCREENSHOT subs:nil]; }
+ (PHPickerFilter *)slomoVideosFilter { return [self _k:PUF_SLOMO subs:nil]; }
+ (PHPickerFilter *)timelapseVideosFilter { return [self _k:PUF_TIMELAPSE subs:nil]; }
+ (PHPickerFilter *)depthEffectPhotosFilter { return [self _k:PUF_DEPTH subs:nil]; }
+ (PHPickerFilter *)burstsFilter { return [self _k:PUF_BURSTS subs:nil]; }
+ (PHPickerFilter *)cinematicVideosFilter { return [self _k:PUF_CINEMATIC subs:nil]; }
+ (PHPickerFilter *)spatialMediaFilter { return [self _k:PUF_SPATIAL subs:nil]; }
+ (PHPickerFilter *)screenRecordingsFilter { return [self _k:PUF_SCREENREC subs:nil]; }
+ (PHPickerFilter *)playbackStyleFilter:(NSInteger)s { return s == 3 || s == 4 ? [self videosFilter] : [self imagesFilter]; }
+ (PHPickerFilter *)anyFilterMatchingSubfilters:(NSArray *)s { return [self _k:PUF_ANY subs:s]; }
+ (PHPickerFilter *)allFilterMatchingSubfilters:(NSArray *)s { return [self _k:PUF_ALL subs:s]; }
+ (PHPickerFilter *)notFilterOfSubfilter:(PHPickerFilter *)s { return [self _k:PUF_NOT subs:s ? [NSArray arrayWithObject:s] : nil]; }
- (BOOL)_mosMatchesMediaType:(NSInteger)m subtypes:(NSUInteger)s { return pu_filter_match(self, m, s); }
- (BOOL)isEqual:(id)o {
    if (o == self) return YES;
    if (![o isKindOfClass:[PHPickerFilter class]]) return NO;
    PHPickerFilter *f = o;
    if (f->_kind != _kind || f->_subs.count != _subs.count) return NO;
    for (NSUInteger i = 0; i < _subs.count; i++) if (![[_subs objectAtIndex:i] isEqual:[f->_subs objectAtIndex:i]]) return NO;
    return YES;
}
- (NSUInteger)hash { NSUInteger h = (NSUInteger)_kind * 31; for (id s in _subs) h = h * 17 + [s hash]; return h; }
@end

/* ================================================================ PHPickerConfiguration */
@interface PHPickerConfiguration : NSObject <NSCopying> {
@public
    NSInteger _rep, _selection, _limit, _mode;
    NSUInteger _disabled, _edges;
    PHPickerFilter *_filter;
    NSArray *_preselected;
    id _library;
}
@end
@implementation PHPickerConfiguration
- (instancetype)init { if ((self = [super init])) { _limit = 1; _preselected = [[NSArray alloc] init]; } return self; }
- (instancetype)initWithPhotoLibrary:(id)lib { if ((self = [self init])) _library = [lib retain]; return self; }
- (void)dealloc { [_filter release]; [_preselected release]; [_library release]; [super dealloc]; }
- (id)copyWithZone:(NSZone *)z {
    (void)z;
    PHPickerConfiguration *c = [[PHPickerConfiguration alloc] init];
    c->_rep = _rep; c->_selection = _selection; c->_limit = _limit; c->_mode = _mode; c->_disabled = _disabled; c->_edges = _edges;
    c->_filter = [_filter retain]; [c->_preselected release]; c->_preselected = [_preselected copy]; c->_library = [_library retain];
    return c;
}
- (NSInteger)preferredAssetRepresentationMode { return _rep; } - (void)setPreferredAssetRepresentationMode:(NSInteger)v { _rep = v; }
- (NSInteger)selection { return _selection; } - (void)setSelection:(NSInteger)v { _selection = v; }
- (NSInteger)selectionLimit { return _limit; } - (void)setSelectionLimit:(NSInteger)v { _limit = v < 0 ? 0 : v; }
- (NSInteger)mode { return _mode; } - (void)setMode:(NSInteger)v { _mode = v; }
- (NSUInteger)disabledCapabilities { return _disabled; } - (void)setDisabledCapabilities:(NSUInteger)v { _disabled = v; }
- (NSUInteger)edgesWithoutContentMargins { return _edges; } - (void)setEdgesWithoutContentMargins:(NSUInteger)v { _edges = v; }
- (PHPickerFilter *)filter { return _filter; }
- (void)setFilter:(PHPickerFilter *)f { [f retain]; [_filter release]; _filter = f; }
- (NSArray *)preselectedAssetIdentifiers { return _preselected; }
- (void)setPreselectedAssetIdentifiers:(NSArray *)a { a = a ? [a copy] : [[NSArray alloc] init]; [_preselected release]; _preselected = a; }
- (id)_mosLibrary { return _library; }
- (BOOL)isEqual:(id)o {
    if (o == self) return YES;
    if (![o isKindOfClass:[PHPickerConfiguration class]]) return NO;
    PHPickerConfiguration *c = o;
    return c->_rep == _rep && c->_selection == _selection && c->_limit == _limit && c->_mode == _mode && c->_disabled == _disabled &&
        c->_edges == _edges && (c->_filter == _filter || [c->_filter isEqual:_filter]) && [c->_preselected isEqual:_preselected] &&
        c->_library == _library;
}
- (NSUInteger)hash { return (NSUInteger)(_limit * 131 + _selection * 17 + _rep * 7 + _mode) ^ [_filter hash]; }
@end

/* ================================================================ NSItemProvider de arquivo */
@interface NSItemProvider : NSObject
@end
@interface _PHPickerItemProvider : NSItemProvider {
@public
    NSString *_path;
    NSString *_uti;
}
- (instancetype)_mosInitWithPath:(NSString *)p;
@end
typedef struct { void *isa; int flags, reserved; void *invoke; } pu_block;
#define PU_CALL(b, T) ((T)((pu_block *)(b))->invoke)
static id pu_error(NSInteger code, const char *msg) {
    NSDictionary *u = [NSDictionary dictionaryWithObject:pu_str(msg) forKey:@"NSLocalizedDescription"];
    return [(id)objc_getClass("NSError") errorWithDomain:@"NSItemProviderErrorDomain" code:code userInfo:u];
}
static int pu_tmp_seq;
/* cópia do arquivo com POSIX (sem depender do NSFileManager) */
static BOOL pu_copy_file(const char *from, const char *to) {
    int in = open(from, O_RDONLY), out;
    if (in < 0) return NO;
    out = open(to, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (out < 0) { close(in); return NO; }
    char buf[65536];
    ssize_t n;
    BOOL ok = YES;
    while ((n = read(in, buf, sizeof buf)) > 0)
        for (ssize_t w = 0; w < n;) { ssize_t k = write(out, buf + w, (size_t)(n - w)); if (k <= 0) { ok = NO; break; } w += k; }
    if (n < 0) ok = NO;
    close(in);
    if (close(out) != 0) ok = NO;
    if (!ok) unlink(to);
    return ok;
}
@implementation _PHPickerItemProvider
- (instancetype)_mosInitWithPath:(NSString *)p {
    if ((self = [super init])) {
        _path = [p copy];
        _uti = [pu_str(pu_uti_for_ext([[[p pathExtension] lowercaseString] UTF8String])) retain];
    }
    return self;
}
- (void)dealloc { [_path release]; [_uti release]; [super dealloc]; }
- (NSArray *)registeredTypeIdentifiers { return [NSArray arrayWithObject:_uti]; }
- (NSArray *)registeredTypeIdentifiersWithFileOptions:(NSInteger)o { (void)o; return [self registeredTypeIdentifiers]; }
- (NSArray *)registeredContentTypes { return [self registeredTypeIdentifiers]; }
- (BOOL)hasItemConformingToTypeIdentifier:(NSString *)t { return t && pu_conforms([_uti UTF8String], [t UTF8String]); }
- (BOOL)hasRepresentationConformingToTypeIdentifier:(NSString *)t fileOptions:(NSInteger)o { (void)o; return [self hasItemConformingToTypeIdentifier:t]; }
- (NSString *)suggestedName { return [[_path lastPathComponent] stringByDeletingPathExtension]; }
- (void)setSuggestedName:(NSString *)n { (void)n; }
- (BOOL)canLoadObjectOfClass:(Class)c {
    if (!c) return NO;
    if (c == (Class)objc_getClass("UIImage") || c == (Class)objc_getClass("NSImage")) return !pu_is_video_uti([_uti UTF8String]) && strcmp([_uti UTF8String], "public.data") != 0;
    if (c == (Class)objc_getClass("NSURL")) return YES;
    return NO;
}
- (UIImage *)_mosImage {
    void *src = CGImageSourceCreateWithURL([(id)objc_getClass("NSURL") fileURLWithPath:_path], nil);
    if (!src) return nil;
    void *cg = CGImageSourceCreateImageAtIndex(src, 0, nil);
    CFRelease(src);
    if (!cg) return nil;
    UIImage *i = [UIImage imageWithCGImage:cg];
    CGImageRelease(cg);
    return i;
}
/* handler(object, error) */
- (id)loadObjectOfClass:(Class)c completionHandler:(id)h {
    id obj = nil, err = nil;
    if (c == (Class)objc_getClass("NSURL")) obj = [(id)objc_getClass("NSURL") fileURLWithPath:_path];
    else if ([self canLoadObjectOfClass:c]) {
        obj = [self _mosImage];
        if (!obj) err = pu_error(-1000, "PhotosUI[mOS]: o ImageIO não decodificou o arquivo");
    } else err = pu_error(-1200, "PhotosUI[mOS]: o item não se converte nessa classe");
    if (h) PU_CALL(h, void (*)(id, id, id))(h, obj, err);
    return nil;
}
/* handler(data, error) */
- (id)loadDataRepresentationForTypeIdentifier:(NSString *)t completionHandler:(id)h {
    id d = nil, err = nil;
    if (![self hasItemConformingToTypeIdentifier:t]) err = pu_error(-1000, "PhotosUI[mOS]: o item não tem esse tipo");
    else if (!(d = [(id)objc_getClass("NSData") dataWithContentsOfFile:_path])) err = pu_error(-1000, "PhotosUI[mOS]: arquivo ilegível");
    if (h) PU_CALL(h, void (*)(id, id, id))(h, d, err);
    return nil;
}
- (id)loadDataRepresentationForContentType:(id)t completionHandler:(id)h { return [self loadDataRepresentationForTypeIdentifier:[t description] completionHandler:h]; }
/* handler(url, error): cópia temporária, apagada quando o handler volta (como na Apple) */
- (id)loadFileRepresentationForTypeIdentifier:(NSString *)t completionHandler:(id)h {
    if (![self hasItemConformingToTypeIdentifier:t]) {
        if (h) PU_CALL(h, void (*)(id, id, id))(h, nil, pu_error(-1000, "PhotosUI[mOS]: o item não tem esse tipo"));
        return nil;
    }
    char tmp[512];
    snprintf(tmp, sizeof tmp, "/tmp/.mos-phpicker-%d-%d-%s", (int)getpid(), ++pu_tmp_seq, [[_path lastPathComponent] UTF8String]);
    if (!pu_copy_file([_path UTF8String], tmp)) {
        if (h) PU_CALL(h, void (*)(id, id, id))(h, nil, pu_error(-1000, "PhotosUI[mOS]: não deu para copiar o arquivo"));
        return nil;
    }
    if (h) PU_CALL(h, void (*)(id, id, id))(h, [(id)objc_getClass("NSURL") fileURLWithPath:pu_str(tmp)], nil);
    unlink(tmp);
    return nil;
}
- (id)loadFileRepresentationForContentType:(id)t openInPlace:(BOOL)inPlace completionHandler:(id)h {
    if (!inPlace) return [self loadFileRepresentationForTypeIdentifier:[t description] completionHandler:h];
    if (h) PU_CALL(h, void (*)(id, id, BOOL, id))(h, [(id)objc_getClass("NSURL") fileURLWithPath:_path], YES, nil);
    return nil;
}
/* handler(url, isInPlace, error) */
- (id)loadInPlaceFileRepresentationForTypeIdentifier:(NSString *)t completionHandler:(id)h {
    BOOL ok = [self hasItemConformingToTypeIdentifier:t];
    if (h) PU_CALL(h, void (*)(id, id, BOOL, id))(h, ok ? [(id)objc_getClass("NSURL") fileURLWithPath:_path] : nil, YES,
                                                  ok ? nil : pu_error(-1000, "PhotosUI[mOS]: o item não tem esse tipo"));
    return nil;
}
/* handler(item, error): a URL do arquivo, como o PHPicker entrega */
- (void)loadItemForTypeIdentifier:(NSString *)t options:(NSDictionary *)o completionHandler:(id)h {
    (void)o;
    BOOL ok = [self hasItemConformingToTypeIdentifier:t];
    if (h) PU_CALL(h, void (*)(id, id, id))(h, ok ? [(id)objc_getClass("NSURL") fileURLWithPath:_path] : nil,
                                            ok ? nil : pu_error(-1000, "PhotosUI[mOS]: o item não tem esse tipo"));
}
- (NSString *)_mosPath { return _path; }
@end

/* ================================================================ PHPickerResult */
@interface PHPickerResult : NSObject {
@public
    NSItemProvider *_provider;
    NSString *_ident;
}
@end
@implementation PHPickerResult
- (void)dealloc { [_provider release]; [_ident release]; [super dealloc]; }
- (NSItemProvider *)itemProvider { return _provider; }
- (NSString *)assetIdentifier { return _ident; }
- (BOOL)isEqual:(id)o {
    if (o == self) return YES;
    if (![o isKindOfClass:[PHPickerResult class]]) return NO;
    PHPickerResult *r = o;
    return r->_provider == _provider && (r->_ident == _ident || [r->_ident isEqual:_ident]);
}
- (NSUInteger)hash { return [_ident hash] ^ (NSUInteger)_provider; }
@end
static PHPickerResult *pu_result(NSString *path, NSString *ident) {
    PHPickerResult *r = [[[PHPickerResult alloc] init] autorelease];
    r->_provider = [[_PHPickerItemProvider alloc] _mosInitWithPath:path];
    r->_ident = [ident copy];
    return r;
}

/* ================================================================ a grade do seletor */
@interface _PHPickerItem : NSObject {
@public
    NSString *_path, *_ident;
    NSInteger _media;
    double _duration;
    UIImage *_thumb;
    BOOL _thumbTried;
}
@end
@implementation _PHPickerItem
- (void)dealloc { [_path release]; [_ident release]; [_thumb release]; [super dealloc]; }
@end

@class PHPickerViewController;
@interface _PHPickerGridView : UIScrollView {
@public
    NSArray *_items;                    /* _PHPickerItem */
    NSMutableArray *_selected;          /* identificadores, na ordem do toque */
    PHPickerViewController *_picker;    /* não retido */
}
- (NSInteger)_mosColumns;
- (CGFloat)_mosCell;
- (NSInteger)_mosIndexAt:(CGPoint)p;
@end

@interface PHPickerViewController : UIViewController {
@public
    PHPickerConfiguration *_config;
    id _delegate;                        /* fraco, como na Apple */
    _PHPickerGridView *_grid;
    UILabel *_titleLabel, *_countLabel;
    UIButton *_cancel, *_add;
    BOOL _finished;
}
- (void)_mosToggle:(NSInteger)idx;
- (void)_mosFinish:(BOOL)cancelled;
- (void)_mosUpdateBar;
@end

static const CGFloat PU_GAP = 2, PU_BAR = 44, PU_MIN_CELL = 110;

@implementation _PHPickerGridView
- (void)dealloc { [_items release]; [_selected release]; [super dealloc]; }
- (NSInteger)_mosColumns { NSInteger n = (NSInteger)((_frame.size.width + PU_GAP) / (PU_MIN_CELL + PU_GAP)); return n < 1 ? 1 : n; }
- (CGFloat)_mosCell { NSInteger n = [self _mosColumns]; return (_frame.size.width - PU_GAP * (n - 1)) / n; }
- (void)_mosRelayout {
    NSInteger n = [self _mosColumns], rows = ((NSInteger)_items.count + n - 1) / n;
    CGFloat c = [self _mosCell];
    [self setContentSize:CGSizeMake(_frame.size.width, rows * c + (rows > 0 ? (rows - 1) * PU_GAP : 0))];
}
- (void)setFrame:(CGRect)f { [super setFrame:f]; [self _mosRelayout]; }
- (NSInteger)_mosIndexAt:(CGPoint)p {
    CGFloat c = [self _mosCell];
    if (p.x < 0 || p.y < 0) return -1;
    NSInteger col = (NSInteger)(p.x / (c + PU_GAP)), row = (NSInteger)(p.y / (c + PU_GAP));
    if (col >= [self _mosColumns] || p.x - col * (c + PU_GAP) > c || p.y - row * (c + PU_GAP) > c) return -1;
    NSInteger i = row * [self _mosColumns] + col;
    return i < (NSInteger)_items.count ? i : -1;
}
static UIImage *pu_thumb(_PHPickerItem *it, CGFloat side) {
    if (it->_thumbTried) return it->_thumb;
    it->_thumbTried = YES;
    if (it->_media != 1) return nil;     /* vídeo: o quadro fica para o AVFoundation; a célula mostra a duração */
    void *src = CGImageSourceCreateWithURL([(id)objc_getClass("NSURL") fileURLWithPath:it->_path], nil);
    if (!src) return nil;
    NSDictionary *o = [NSDictionary dictionaryWithObjectsAndKeys:
        [NSNumber numberWithBool:YES], @"kCGImageSourceCreateThumbnailFromImageAlways",
        [NSNumber numberWithLongLong:(long long)(side * 2)], @"kCGImageSourceThumbnailMaxPixelSize", nil];
    void *cg = CGImageSourceCreateThumbnailAtIndex(src, 0, o);
    if (!cg) cg = CGImageSourceCreateImageAtIndex(src, 0, nil);
    CFRelease(src);
    if (cg) { it->_thumb = [[UIImage imageWithCGImage:cg] retain]; CGImageRelease(cg); }
    return it->_thumb;
}
static void pu_text(NSString *s, CGFloat x, CGFloat y, CGFloat size, UIColor *col) {
    NSDictionary *a = [NSDictionary dictionaryWithObjectsAndKeys:[NSFont systemFontOfSize:size], NSFontAttributeName,
        [NSColor colorWithRed:col->_r green:col->_g blue:col->_b alpha:col->_a], NSForegroundColorAttributeName, nil];
    [s drawAtPoint:NSMakePoint(x, y) withAttributes:a];
}
- (void)drawRect:(CGRect)r {
    CGContextRef c = UIGraphicsGetCurrentContext();
    if (!c) return;
    CGFloat side = [self _mosCell], top = r.origin.y, h = r.size.height;
    NSInteger n = [self _mosColumns];
    BOOL ordered = _picker && (_picker->_config->_selection == 1 || _picker->_config->_selection == 3);   /* ordered, continuousAndOrdered */
    for (NSInteger i = 0; i < (NSInteger)_items.count; i++) {
        NSInteger row = i / n, col = i % n;
        CGFloat x = col * (side + PU_GAP), y = row * (side + PU_GAP);
        if (y + side < top || y > top + h) continue;          /* só o que está na área visível */
        CGRect cell = CGRectMake(x, y - top, side, side);
        _PHPickerItem *it = [_items objectAtIndex:i];
        CGContextSaveGState(c);
        CGContextClipToRect(c, cell);
        CGContextSetFillColorWithColor(c, [[UIColor systemGray4Color] CGColor]);
        CGContextFillRect(c, cell);
        UIImage *img = pu_thumb(it, side);
        if (img) {
            CGSize s = [img size];
            CGFloat k = s.width > 0 && s.height > 0 ? (side / s.width > side / s.height ? side / s.width : side / s.height) : 1;
            CGFloat w = s.width * k, hh = s.height * k;   /* preenche a célula (aspect fill), cortado pelo clip */
            [img drawInRect:CGRectMake(x + (side - w) / 2, y - top + (side - hh) / 2, w, hh)];
        }
        if (it->_media == 2) {
            int sec = (int)(it->_duration + 0.5);
            pu_text([NSString stringWithFormat:@"%d:%02d", sec / 60, sec % 60], x + side - 40, y - top + side - 20, 12, [UIColor whiteColor]);
        }
        NSUInteger sel = [_selected indexOfObject:it->_ident];
        CGRect badge = CGRectMake(x + side - 28, y - top + 6, 22, 22);
        if (sel != (NSUInteger)NSNotFound) {
            CGContextSetFillColorWithColor(c, [[UIColor colorWithWhite:1 alpha:0.25] CGColor]);
            CGContextFillRect(c, cell);
            CGContextSetFillColorWithColor(c, [[UIColor systemBlueColor] CGColor]);
            CGContextFillEllipseInRect(c, badge);
            pu_text(ordered ? [NSString stringWithFormat:@"%lu", (unsigned long)sel + 1] : @"✓", badge.origin.x + 6, badge.origin.y + 3, 13, [UIColor whiteColor]);
        } else if (_picker && _picker->_config->_limit != 1) {
            CGContextSetStrokeColorWithColor(c, [[UIColor whiteColor] CGColor]);
            CGContextSetLineWidth(c, 1.5);
            CGContextStrokeEllipseInRect(c, badge);
        }
        CGContextRestoreGState(c);
    }
    if (!_items.count) pu_text(@"Nenhuma foto ou vídeo em Imagens", 16, 16, 14, [UIColor labelColor]);
}
- (void)touchesEnded:(NSSet *)t withEvent:(UIEvent *)e {
    UITouch *u = [t anyObject];
    [super touchesEnded:t withEvent:e];
    if (!u) return;
    CGPoint p = [u locationInView:self];
    if (fabs(p.y - _dragLast.y) > 6) return;      /* foi rolagem, não toque */
    NSInteger i = [self _mosIndexAt:p];
    if (i >= 0) [_picker _mosToggle:i];
}
@end

/* ---- o delegate da Apple (ObjC): - picker:didFinishPicking: */
@protocol PHPickerViewControllerDelegate <NSObject>
- (void)picker:(PHPickerViewController *)picker didFinishPicking:(NSArray *)results;
@end

@implementation PHPickerViewController
- (instancetype)initWithConfiguration:(PHPickerConfiguration *)c {
    if ((self = [super init])) _config = c ? [c copy] : [[PHPickerConfiguration alloc] init];
    return self;
}
- (instancetype)init { return [self initWithConfiguration:nil]; }
- (void)dealloc { [_config release]; [_grid release]; [_titleLabel release]; [_countLabel release]; [_cancel release]; [_add release]; [super dealloc]; }
- (PHPickerConfiguration *)configuration { return [[_config copy] autorelease]; }
- (id)delegate { return _delegate; }
- (void)setDelegate:(id)d { _delegate = d; }
- (NSInteger)modalPresentationStyle { return 2; }  /* folha (UIModalPresentationFormSheet), como no Catalyst */
- (NSArray *)_mosLoadItems {
    NSMutableArray *out = [NSMutableArray array];
    id assetCls = objc_getClass("PHAsset");
    if (!assetCls) return out;
    id res = [assetCls fetchAssetsWithOptions:nil];
    for (NSUInteger i = 0, n = [res count]; i < n; i++) {
        id a = [res objectAtIndex:i];
        NSInteger m = [a mediaType];
        NSUInteger sub = [a respondsToSelector:@selector(mediaSubtypes)] ? [a mediaSubtypes] : 0;
        if (!pu_filter_match(_config->_filter, m, sub)) continue;
        _PHPickerItem *it = [[[_PHPickerItem alloc] init] autorelease];
        it->_ident = [[a localIdentifier] copy];
        it->_path = [it->_ident copy];                   /* no Photos do mOS o identificador é o caminho do arquivo */
        it->_media = m;
        it->_duration = [a respondsToSelector:@selector(duration)] ? [a duration] : 0;
        [out addObject:it];
    }
    return out;
}
- (void)loadView {
    UIView *v = [[[UIView alloc] initWithFrame:CGRectMake(0, 0, 540, 620)] autorelease];
    [v setBackgroundColor:[UIColor systemBackgroundColor]];
    _cancel = [[UIButton buttonWithType:1] retain];
    [_cancel setTitle:@"Cancelar" forState:0];
    [_cancel addTarget:self action:@selector(_mosCancelTapped:) forControlEvents:1 << 6];
    _add = [[UIButton buttonWithType:1] retain];
    [_add setTitle:@"Adicionar" forState:0];
    [_add addTarget:self action:@selector(_mosAddTapped:) forControlEvents:1 << 6];
    _titleLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    [_titleLabel setTextAlignment:1];
    [_titleLabel setFont:[UIFont boldSystemFontOfSize:15]];
    [_titleLabel setText:@"Fotos"];
    _countLabel = [[UILabel alloc] initWithFrame:CGRectZero];
    [_countLabel setTextAlignment:1];
    [_countLabel setFont:[UIFont systemFontOfSize:11]];
    _grid = [[_PHPickerGridView alloc] initWithFrame:CGRectZero];
    _grid->_picker = self;
    _grid->_items = [[self _mosLoadItems] retain];
    _grid->_selected = [[NSMutableArray alloc] init];
    for (NSString *ident in _config->_preselected)
        for (_PHPickerItem *it in _grid->_items)
            if ([it->_ident isEqual:ident] && (_config->_limit == 0 || (NSInteger)_grid->_selected.count < _config->_limit)) { [_grid->_selected addObject:ident]; break; }
    [v addSubview:_titleLabel]; [v addSubview:_countLabel]; [v addSubview:_cancel]; [v addSubview:_add]; [v addSubview:_grid];
    [self setView:v];
    [self viewDidLayoutSubviews];
    [self _mosUpdateBar];
}
- (void)viewDidLayoutSubviews {
    CGRect b = [[self view] bounds];
    [_cancel setFrame:CGRectMake(8, 6, 90, 32)];
    [_add setFrame:CGRectMake(b.size.width - 98, 6, 90, 32)];
    [_titleLabel setFrame:CGRectMake(100, 4, b.size.width - 200, 22)];
    [_countLabel setFrame:CGRectMake(100, 24, b.size.width - 200, 16)];
    [_grid setFrame:CGRectMake(0, PU_BAR, b.size.width, b.size.height - PU_BAR)];
}
/* PHPickerConfigurationSelection: 0 default, 1 ordered, 2 continuous, 3 continuousAndOrdered */
- (BOOL)_mosContinuous { return _config->_selection == 2 || _config->_selection == 3; }
- (BOOL)_mosImmediate { return _config->_limit == 1 && ![self _mosContinuous]; }
- (void)_mosUpdateBar {
    NSUInteger n = _grid->_selected.count;
    [_countLabel setText:n ? [NSString stringWithFormat:@"%lu selecionado%s", (unsigned long)n, n == 1 ? "" : "s"] : @""];
    [_add setHidden:[self _mosImmediate]];
    [_add setEnabled:n > 0 || [self _mosContinuous]];
    [_grid setNeedsDisplay];
}
- (void)_mosToggle:(NSInteger)idx {
    if (_finished || idx < 0 || idx >= (NSInteger)_grid->_items.count) return;
    NSString *ident = ((_PHPickerItem *)[_grid->_items objectAtIndex:idx])->_ident;
    NSUInteger at = [_grid->_selected indexOfObject:ident];
    if (at != (NSUInteger)NSNotFound) [_grid->_selected removeObjectAtIndex:at];
    else if (_config->_limit == 1) { [_grid->_selected removeAllObjects]; [_grid->_selected addObject:ident]; }
    else if (_config->_limit == 0 || (NSInteger)_grid->_selected.count < _config->_limit) [_grid->_selected addObject:ident];
    [self _mosUpdateBar];
    if ([self _mosImmediate] && _grid->_selected.count == 1) [self _mosFinish:NO];
    else if ([self _mosContinuous]) [self _mosDeliver];   /* contínua: entrega a cada mudança */
}
- (NSArray *)_mosResults {
    NSMutableArray *out = [NSMutableArray array];
    for (NSString *ident in _grid->_selected)
        for (_PHPickerItem *it in _grid->_items)
            if ([it->_ident isEqual:ident]) {
                /* sem biblioteca no init, a Apple não devolve o identificador do asset */
                [out addObject:pu_result(it->_path, _config->_library ? it->_ident : nil)];
                break;
            }
    return out;
}
- (void)_mosDeliver {
    id d = _delegate;
    if ([d respondsToSelector:@selector(picker:didFinishPicking:)]) [d picker:self didFinishPicking:[self _mosResults]];
}
- (void)_mosFinish:(BOOL)cancelled {
    if (_finished) return;
    _finished = YES;
    if (cancelled) [_grid->_selected removeAllObjects];
    [self retain];
    [self _mosDeliver];       /* o app fecha o seletor no delegate (dismiss), como na Apple */
    [self release];
}
- (void)_mosCancelTapped:(id)s { (void)s; [self _mosFinish:YES]; }
- (void)_mosAddTapped:(id)s { (void)s; [self _mosFinish:NO]; }
- (void)deselectAssetsWithIdentifiers:(NSArray *)ids { [self view]; for (NSString *i in ids) { NSUInteger k = [_grid->_selected indexOfObject:i]; if (k != (NSUInteger)NSNotFound) [_grid->_selected removeObjectAtIndex:k]; } [self _mosUpdateBar]; }
- (void)moveAssetWithIdentifier:(NSString *)a afterAssetWithIdentifier:(NSString *)b {
    [self view];
    NSUInteger i = [_grid->_selected indexOfObject:a];
    if (i == (NSUInteger)NSNotFound) return;
    [a retain];
    [_grid->_selected removeObjectAtIndex:i];
    NSUInteger j = b ? [_grid->_selected indexOfObject:b] : (NSUInteger)NSNotFound;
    [_grid->_selected insertObject:a atIndex:j == (NSUInteger)NSNotFound ? 0 : j + 1];
    [a release];
    [self _mosUpdateBar];
}
- (void)scrollToInitialPosition { [self view]; [_grid setContentOffset:CGPointMake(0, 0)]; }
- (void)scrollToAutomaticInitialPosition { [self scrollToInitialPosition]; }
- (void)zoomInOnAssetWithIdentifier:(NSString *)i { (void)i; }
- (void)zoomOutOnAssetWithIdentifier:(NSString *)i { (void)i; }
- (void)updatePickerUsingConfiguration:(id)u {
    /* PHPickerUpdateConfiguration: selectionLimit e edgesWithoutContentMargins */
    if ([u respondsToSelector:@selector(selectionLimit)]) _config->_limit = [u selectionLimit];
    [self _mosUpdateBar];
}
/* teste de host: toca o item i como o mouse faria */
- (void)_mosTapItemAtIndex:(NSInteger)i { [self view]; [self _mosToggle:i]; }
- (void)_mosTapAdd { [self view]; [self _mosAddTapped:nil]; }
- (void)_mosTapCancel { [self view]; [self _mosCancelTapped:nil]; }
- (NSUInteger)_mosItemCount { [self view]; return _grid->_items.count; }
@end

@interface NSObject (MOSLivePhotoDelegate)
- (void)livePhotoView:(id)v willBeginPlaybackWithStyle:(NSInteger)s;
- (void)livePhotoView:(id)v didEndPlaybackWithStyle:(NSInteger)s;
@end

/* ================================================================ PHLivePhotoView */
@interface PHLivePhotoView : UIView {
@public
    id _livePhoto;
    id _delegate;
    BOOL _muted;
    NSInteger _contentMode2;
    id _gesture;
}
@end
@implementation PHLivePhotoView
+ (UIImage *)livePhotoBadgeImageWithOptions:(NSUInteger)o { (void)o; return [UIImage systemImageNamed:@"livephoto"]; }
- (void)dealloc { [_livePhoto release]; [_gesture release]; [super dealloc]; }
- (id)livePhoto { return _livePhoto; }
- (void)setLivePhoto:(id)p { [p retain]; [_livePhoto release]; _livePhoto = p; [self setNeedsDisplay]; }
- (id)delegate { return _delegate; } - (void)setDelegate:(id)d { _delegate = d; }
- (BOOL)isMuted { return _muted; } - (void)setMuted:(BOOL)m { _muted = m; }
- (NSInteger)contentMode { return _contentMode2; } - (void)setContentMode:(NSInteger)m { _contentMode2 = m; }
- (id)playbackGestureRecognizer { if (!_gesture) _gesture = [[(id)objc_getClass("UILongPressGestureRecognizer") alloc] init]; return _gesture; }
- (UIView *)contentView { return self; }
/* a biblioteca do mOS não guarda o par HEIC + MOV de uma Live Photo, então não há reprodução a fazer:
 * o delegate recebe começo e fim, como quando a Apple termina um vídeo curto */
- (void)startPlaybackWithStyle:(NSInteger)style {
    if ([_delegate respondsToSelector:@selector(livePhotoView:willBeginPlaybackWithStyle:)]) [_delegate livePhotoView:self willBeginPlaybackWithStyle:style];
    if ([_delegate respondsToSelector:@selector(livePhotoView:didEndPlaybackWithStyle:)]) [_delegate livePhotoView:self didEndPlaybackWithStyle:style];
}
- (void)stopPlayback {}
- (void)stopPlaybackAnimated:(BOOL)a { (void)a; }
@end
