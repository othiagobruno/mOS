/*
 * mOS — AVMetadataItem / AVMutableMetadataItem e as constantes de metadado, tipo de arquivo, vídeo e erro da
 * AVFoundation que o corpus importa (docs/lacunas/isabela.md).
 * Só entram aqui as constantes cujo valor é certo: as chaves comuns ("title"...), os identificadores "common/..." e
 * "mdta/com.apple.quicktime..." (o mesmo formato das que já existem em wa_media_consts.c), os espaços de chave, os
 * UTIs de arquivo, as chaves de vídeo que repetem as do CoreVideo e as que valem o próprio nome. As chaves iTunes,
 * QuickTime user data e os identificadores ID3 ficam de fora até o valor ser medido num Mac (docs da Apple).
 * Código original do mOS (MIT).
 */
#include "foundation.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
size_t strlen(const char *s);
IMP class_getMethodImplementation(cx_class_t *c, SEL s);

extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } amc_t;
#define K(var, text) static amc_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; const void *var = &var##_s;
#define SAME(var) K(var, #var)
/* chaves e identificadores comuns */
K(AVMetadataCommonKeyTitle, "title") K(AVMetadataCommonKeyArtist, "artist") K(AVMetadataCommonKeyAlbumName, "albumName")
K(AVMetadataCommonKeyAuthor, "author") K(AVMetadataCommonKeyCopyrights, "copyrights") K(AVMetadataCommonKeyCreationDate, "creationDate")
K(AVMetadataCommonKeyDescription, "description") K(AVMetadataCommonKeyLanguage, "language") K(AVMetadataCommonKeyPublisher, "publisher")
K(AVMetadataCommonKeyType, "type") K(AVMetadataCommonKeyArtwork, "artwork")
K(AVMetadataCommonIdentifierTitle, "common/title") K(AVMetadataCommonIdentifierArtist, "common/artist")
K(AVMetadataCommonIdentifierAlbumName, "common/albumName") K(AVMetadataCommonIdentifierAuthor, "common/author")
K(AVMetadataCommonIdentifierCopyrights, "common/copyrights") K(AVMetadataCommonIdentifierCreationDate, "common/creationDate")
K(AVMetadataCommonIdentifierDescription, "common/description") K(AVMetadataCommonIdentifierLanguage, "common/language")
K(AVMetadataCommonIdentifierPublisher, "common/publisher") K(AVMetadataCommonIdentifierType, "common/type")
K(AVMetadataCommonIdentifierArtwork, "common/artwork")
/* espaços de chave */
K(AVMetadataKeySpaceiTunes, "itsk") K(AVMetadataKeySpaceQuickTimeUserData, "udta") K(AVMetadataKeySpaceID3, "org.id3")
/* metadado QuickTime (mdta) */
#define QT(n, k) K(AVMetadataIdentifierQuickTimeMetadata##n, "mdta/com.apple.quicktime." k)
QT(Album, "album") QT(Artist, "artist") QT(Artwork, "artwork") QT(Author, "author") QT(Comment, "comment") QT(Composer, "composer")
QT(Copyright, "copyright") QT(CreationDate, "creationdate") QT(Description, "description") QT(Genre, "genre") QT(Performer, "performer")
QT(Publisher, "publisher") QT(Title, "title") QT(ContentIdentifier, "content.identifier")
/* 3GPP (TS 26.244, caixas de udta) */
K(AVMetadata3GPUserDataKeyAlbumAndTrack, "albm") K(AVMetadata3GPUserDataKeyAuthor, "auth") K(AVMetadata3GPUserDataKeyDescription, "dscp")
K(AVMetadata3GPUserDataKeyGenre, "gnre") K(AVMetadata3GPUserDataKeyPerformer, "perf") K(AVMetadata3GPUserDataKeyTitle, "titl")
/* tipos de arquivo (UTIs) e mídia */
K(AVFileTypeMPEGLayer3, "public.mp3") K(AVFileType3GPP2, "public.3gpp2") K(AVFileTypeAC3, "public.ac3-audio")
K(AVFileTypeAIFC, "public.aifc-audio") K(AVFileTypeAIFF, "public.aiff-audio") K(AVFileTypeAMR, "org.3gpp.adaptive-multi-rate-audio")
K(AVFileTypeEnhancedAC3, "public.enhanced-ac3-audio") K(AVFileTypeSunAU, "public.au-audio")
K(AVMediaTypeSubtitle, "sbtl") K(AVMediaCharacteristicLegible, "public.legible") K(AVMediaCharacteristicAudible, "public.audible")
/* configurações de vídeo (os mesmos nomes do VideoToolbox / CoreVideo) */
K(AVVideoCodecH264, "avc1") K(AVVideoAllowFrameReorderingKey, "AllowFrameReordering") K(AVVideoH264EntropyModeKey, "H264EntropyMode")
K(AVVideoH264EntropyModeCABAC, "CABAC") K(AVVideoProfileLevelH264High40, "H264_High_4_0")
K(AVVideoCleanApertureKey, "CleanAperture") K(AVVideoCleanApertureWidthKey, "Width") K(AVVideoCleanApertureHeightKey, "Height")
K(AVVideoCleanApertureHorizontalOffsetKey, "HorizontalOffset") K(AVVideoCleanApertureVerticalOffsetKey, "VerticalOffset")
K(AVVideoPixelAspectRatioKey, "PixelAspectRatio") K(AVVideoPixelAspectRatioHorizontalSpacingKey, "HorizontalSpacing")
K(AVVideoPixelAspectRatioVerticalSpacingKey, "VerticalSpacing")
/* as que valem o próprio nome */
SAME(AVErrorDeviceKey) SAME(AVErrorFileSizeKey) SAME(AVErrorMediaSubTypeKey) SAME(AVErrorMediaTypeKey) SAME(AVErrorPIDKey)
SAME(AVErrorRecordingSuccessfullyFinishedKey) SAME(AVErrorTimeKey) SAME(AVEncoderBitDepthHintKey) SAME(AVCaptureDeviceTypeDeskViewCamera)
SAME(AVPlayerRateDidChangeNotification) SAME(AVSampleBufferAudioRendererWasFlushedAutomaticallyNotification)
SAME(AVSampleBufferAudioRendererFlushTimeKey)

/* ---------------------------------------------------------------- AVMetadataItem */
typedef struct { i64 value; i32 timescale; u32 flags; i64 epoch; } cmt_t;
typedef struct {
    cx_class_t *isa;
    id identifier, key, keySpace, value, dataType, lang, locale, extra, startDate;
    cmt_t time, duration;
} mi_t;
#define MI(o) ((mi_t *)(o))
cx_class_t AVMetadataItem_c __asm__("OBJC_CLASS_$_AVMetadataItem"), AVMetadataItem_m __asm__("OBJC_METACLASS_$_AVMetadataItem");
cx_class_t AVMutableMetadataItem_c __asm__("OBJC_CLASS_$_AVMutableMetadataItem"), AVMutableMetadataItem_m __asm__("OBJC_METACLASS_$_AVMutableMetadataItem");

static id cls(const char *n) { return (id)cx_class_named(n); }
static int is(id o, const char *c) { return o && MSG(BOOL, o, "isKindOfClass:", cls(c)); }
static void setobj(id *slot, id v) { if (*slot == v) return; if (v) objc_retain(v); if (*slot) objc_release(*slot); *slot = v; }
static cmt_t invalid(void) { cmt_t t = {0, 0, 0, 0}; return t; }

/* chave como texto: NSString, ou NSNumber com um código de 4 caracteres */
static const char *key_text(id key, char buf[8]) {
    if (!key) return 0;
    if (cx_is_str(key)) return cx_cstr(key);
    if (is(key, "NSNumber")) {
        u32 v = (u32)MSG0(i64, key, "longLongValue");
        for (int i = 0; i < 4; i++) buf[i] = (char)(v >> (24 - 8 * i));
        buf[4] = 0;
        return buf;
    }
    return 0;
}
/* identificador = prefixo do espaço / chave, com bytes fora do ASCII imprimível e '/' em %XX */
static id identifier_for(id key, id ks) {
    char kb[8];
    const char *k = key_text(key, kb), *s = ks ? cx_cstr(ks) : 0;
    if (!k || !s) return 0;
    const char *pre = !strcmp(s, "comn") ? "common" : s;
    char out[512];
    size_t n = 0;
    for (const char *p = pre; *p && n < 200; p++) out[n++] = *p;
    out[n++] = '/';
    static const char hx[] = "0123456789ABCDEF";
    for (const unsigned char *p = (const unsigned char *)k; *p && n < 500; p++) {
        if (*p < 0x20 || *p >= 0x7f || *p == '/' || *p == '%') { out[n++] = '%'; out[n++] = hx[*p >> 4]; out[n++] = hx[*p & 15]; }
        else out[n++] = (char)*p;
    }
    return cx_str_n(out, n);
}
static int hexv(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'A' && c <= 'F' ? c - 'A' + 10 : c >= 'a' && c <= 'f' ? c - 'a' + 10 : -1; }
static id split_identifier(id ident, int want_key) {
    const char *s = ident ? cx_cstr(ident) : 0, *sl = 0;
    if (!s) return 0;
    for (const char *p = s; *p; p++) if (*p == '/') { sl = p; break; }
    if (!sl) return 0;
    if (!want_key) {
        size_t n = (size_t)(sl - s);
        if (n == 6 && !strncmp(s, "common", 6)) return cx_str("comn");
        return cx_str_n(s, n);
    }
    char out[512];
    size_t n = 0;
    for (const char *p = sl + 1; *p && n < 510; p++) {
        if (*p == '%' && hexv(p[1]) >= 0 && hexv(p[2]) >= 0) { out[n++] = (char)(hexv(p[1]) * 16 + hexv(p[2])); p += 2; }
        else out[n++] = *p;
    }
    return cx_str_n(out, n);
}
static id mi_k_identifier(id k, SEL c, id key, id ks) { (void)k; (void)c; return identifier_for(key, ks); }
static id mi_k_key(id k, SEL c, id ident) { (void)k; (void)c; return split_identifier(ident, 1); }
static id mi_k_keyspace(id k, SEL c, id ident) { (void)k; (void)c; return split_identifier(ident, 0); }

static id mi_identifier(id s, SEL c) {
    (void)c;
    if (MI(s)->identifier) return MI(s)->identifier;
    return MI(s)->key && MI(s)->keySpace ? identifier_for(MI(s)->key, MI(s)->keySpace) : 0;
}
static id mi_key(id s, SEL c) { (void)c; return MI(s)->key ? MI(s)->key : MI(s)->identifier ? split_identifier(MI(s)->identifier, 1) : 0; }
static id mi_keyspace(id s, SEL c) { (void)c; return MI(s)->keySpace ? MI(s)->keySpace : MI(s)->identifier ? split_identifier(MI(s)->identifier, 0) : 0; }
static id mi_value(id s, SEL c) { (void)c; return MI(s)->value; }
static id mi_datatype(id s, SEL c) { (void)c; return MI(s)->dataType; }
static id mi_lang(id s, SEL c) { (void)c; return MI(s)->lang; }
static id mi_locale(id s, SEL c) { (void)c; return MI(s)->locale; }
static id mi_extra(id s, SEL c) { (void)c; return MI(s)->extra; }
static id mi_startdate(id s, SEL c) { (void)c; return MI(s)->startDate; }
static cmt_t mi_time(id s, SEL c) { (void)c; return MI(s)->time; }
static cmt_t mi_duration(id s, SEL c) { (void)c; return MI(s)->duration; }
/* commonKey: a própria chave no espaço comum; nos outros, o equivalente conhecido do QuickTime (mdta) */
static id mi_commonkey(id s, SEL c) {
    id ks = mi_keyspace(s, c), key = mi_key(s, c);
    const char *sp = ks ? cx_cstr(ks) : 0;
    char kb[8];
    const char *k = key_text(key, kb);
    if (!sp || !k) return 0;
    if (!strcmp(sp, "comn")) return key;
    if (!strcmp(sp, "mdta") && !strncmp(k, "com.apple.quicktime.", 20)) {
        static const char *map[][2] = {{"title", "title"}, {"artist", "artist"}, {"album", "albumName"}, {"author", "author"},
                                       {"copyright", "copyrights"}, {"creationdate", "creationDate"}, {"description", "description"},
                                       {"publisher", "publisher"}, {"artwork", "artwork"}};
        for (unsigned i = 0; i < sizeof map / sizeof map[0]; i++) if (!strcmp(k + 20, map[i][0])) return cx_str(map[i][1]);
    }
    return 0;
}
static id mi_string(id s, SEL c) {
    (void)c;
    id v = MI(s)->value;
    if (!v) return 0;
    if (cx_is_str(v)) return v;
    if (is(v, "NSNumber") || is(v, "NSDate")) return MSG0(id, v, "description");
    return 0;
}
static id mi_number(id s, SEL c) {
    (void)c;
    id v = MI(s)->value;
    if (is(v, "NSNumber")) return v;
    if (v && cx_is_str(v)) {
        const char *t = cx_cstr(v);
        int ok = t && *t;
        for (const char *p = t; ok && *p; p++) if (!((*p >= '0' && *p <= '9') || *p == '-' || *p == '.' || *p == '+')) ok = 0;
        if (ok) return MSG(id, cls("NSNumber"), "numberWithDouble:", MSG0(double, v, "doubleValue"));
    }
    return 0;
}
static id mi_date(id s, SEL c) { (void)c; return is(MI(s)->value, "NSDate") ? MI(s)->value : 0; }
static id mi_data(id s, SEL c) { (void)c; return is(MI(s)->value, "NSData") ? MI(s)->value : 0; }
static i64 mi_status(id s, SEL c, id key, id *err) { (void)s; (void)c; (void)key; if (err) *err = 0; return 2; /* AVKeyValueStatusLoaded */ }
static void mi_load(id s, SEL c, id keys, void *blk) { (void)s; (void)c; (void)keys; if (blk) ((void (*)(void *))((void **)blk)[2])(blk); }

static id mi_init(id s, SEL c) {
    (void)c;
    MI(s)->time = invalid();
    MI(s)->duration = invalid();
    return s;
}
static id copy_into(id src, cx_class_t *k) {
    id o = mi_init(cx_alloc_instance(k), 0);
    id *from[] = {&MI(src)->identifier, &MI(src)->key, &MI(src)->keySpace, &MI(src)->value, &MI(src)->dataType, &MI(src)->lang, &MI(src)->locale, &MI(src)->extra, &MI(src)->startDate};
    id *to[] = {&MI(o)->identifier, &MI(o)->key, &MI(o)->keySpace, &MI(o)->value, &MI(o)->dataType, &MI(o)->lang, &MI(o)->locale, &MI(o)->extra, &MI(o)->startDate};
    for (unsigned i = 0; i < sizeof from / sizeof from[0]; i++) setobj(to[i], *from[i]);
    MI(o)->time = MI(src)->time;
    MI(o)->duration = MI(src)->duration;
    return o;
}
static id mi_copy(id s, SEL c, void *z) { (void)c; (void)z; return copy_into(s, &AVMetadataItem_c); }
static id mi_mcopy(id s, SEL c, void *z) { (void)c; (void)z; return copy_into(s, &AVMutableMetadataItem_c); }
static void mi_dealloc(id s, SEL c) {
    id *f[] = {&MI(s)->identifier, &MI(s)->key, &MI(s)->keySpace, &MI(s)->value, &MI(s)->dataType, &MI(s)->lang, &MI(s)->locale, &MI(s)->extra, &MI(s)->startDate};
    for (unsigned i = 0; i < sizeof f / sizeof f[0]; i++) setobj(f[i], 0);
    IMP d = class_getMethodImplementation(cx_class_named("NSObject"), c);
    if (d) ((void (*)(id, SEL))d)(s, c);
}
/* AVMutableMetadataItem: setters (o identificador e o par chave/espaço andam juntos, como na AVFoundation) */
static void mm_set_identifier(id s, SEL c, id v) {
    (void)c;
    setobj(&MI(s)->identifier, v);
    setobj(&MI(s)->key, v ? split_identifier(v, 1) : 0);
    setobj(&MI(s)->keySpace, v ? split_identifier(v, 0) : 0);
}
static void mm_set_key(id s, SEL c, id v) { (void)c; setobj(&MI(s)->key, v); setobj(&MI(s)->identifier, 0); }
static void mm_set_keyspace(id s, SEL c, id v) { (void)c; setobj(&MI(s)->keySpace, v); setobj(&MI(s)->identifier, 0); }
static void mm_set_value(id s, SEL c, id v) { (void)c; setobj(&MI(s)->value, v ? MSG0(id, v, "copy") : 0); }
static void mm_set_datatype(id s, SEL c, id v) { (void)c; setobj(&MI(s)->dataType, v); }
static void mm_set_lang(id s, SEL c, id v) { (void)c; setobj(&MI(s)->lang, v); }
static void mm_set_locale(id s, SEL c, id v) { (void)c; setobj(&MI(s)->locale, v); }
static void mm_set_extra(id s, SEL c, id v) { (void)c; setobj(&MI(s)->extra, v); }
static void mm_set_startdate(id s, SEL c, id v) { (void)c; setobj(&MI(s)->startDate, v); }
static void mm_set_time(id s, SEL c, cmt_t t) { (void)c; MI(s)->time = t; }
static void mm_set_duration(id s, SEL c, cmt_t t) { (void)c; MI(s)->duration = t; }
static id mm_item(id k, SEL c) { (void)k; (void)c; return objc_autorelease(mi_init(cx_alloc_instance(&AVMutableMetadataItem_c), 0)); }

/* filtros de classe */
static id filter(id arr, int (*keep)(id item, void *ctx), void *ctx) {
    id out = MSG0(id, cls("NSMutableArray"), "array");
    u64 n = arr ? MSG0(u64, arr, "count") : 0;
    for (u64 i = 0; i < n; i++) { id it = MSG(id, arr, "objectAtIndex:", i); if (keep(it, ctx)) MSG(void, out, "addObject:", it); }
    return out;
}
typedef struct { id key, ks, ident, locale; } fctx;
static int eq(id a, id b) { return a == b || (a && b && MSG(BOOL, a, "isEqual:", b)); }
static int keep_key(id it, void *p) {
    fctx *f = p;
    return (!f->key || eq(mi_key(it, 0), f->key)) && (!f->ks || eq(mi_keyspace(it, 0), f->ks));
}
static int keep_ident(id it, void *p) { return eq(mi_identifier(it, 0), ((fctx *)p)->ident); }
static int keep_locale(id it, void *p) {
    id l = MI(it)->locale, want = ((fctx *)p)->locale;
    return !want || !l || eq(MSG0(id, l, "localeIdentifier"), MSG0(id, want, "localeIdentifier"));
}
static id mi_k_with_key(id k, SEL c, id arr, id key, id ks) { (void)k; (void)c; fctx f = {key, ks, 0, 0}; return filter(arr, keep_key, &f); }
static id mi_k_with_ident(id k, SEL c, id arr, id ident) { (void)k; (void)c; fctx f = {0, 0, ident, 0}; return filter(arr, keep_ident, &f); }
static id mi_k_with_locale(id k, SEL c, id arr, id loc) { (void)k; (void)c; fctx f = {0, 0, 0, loc}; return filter(arr, keep_locale, &f); }

void cx_avmetadata_init(void) {
    cx_class_t *ob = cx_class_named("NSObject");
    if (!cx_class_named("AVMetadataItem")) cx_define_class(&AVMetadataItem_c, &AVMetadataItem_m, "AVMetadataItem", ob, (u32)(sizeof(mi_t) - 8));
    if (!cx_class_named("AVMutableMetadataItem")) cx_define_class(&AVMutableMetadataItem_c, &AVMutableMetadataItem_m, "AVMutableMetadataItem", &AVMetadataItem_c, 0);
#define M(s, f) cx_add_method(&AVMetadataItem_c, s, (void *)(f))
#define CM(s, f) cx_add_cmethod(&AVMetadataItem_c, s, (void *)(f))
#define MM(s, f) cx_add_method(&AVMutableMetadataItem_c, s, (void *)(f))
    M("init", mi_init); M("dealloc", mi_dealloc); M("copyWithZone:", mi_copy); M("mutableCopyWithZone:", mi_mcopy);
    M("copy", mi_copy); M("mutableCopy", mi_mcopy);
    M("identifier", mi_identifier); M("key", mi_key); M("keySpace", mi_keyspace); M("commonKey", mi_commonkey);
    M("value", mi_value); M("dataType", mi_datatype); M("extendedLanguageTag", mi_lang); M("locale", mi_locale);
    M("extraAttributes", mi_extra); M("startDate", mi_startdate); M("time", mi_time); M("duration", mi_duration);
    M("stringValue", mi_string); M("numberValue", mi_number); M("dateValue", mi_date); M("dataValue", mi_data);
    M("statusOfValueForKey:error:", mi_status); M("loadValuesAsynchronouslyForKeys:completionHandler:", mi_load);
    CM("identifierForKey:keySpace:", mi_k_identifier); CM("keyForIdentifier:", mi_k_key); CM("keySpaceForIdentifier:", mi_k_keyspace);
    CM("metadataItemsFromArray:withKey:keySpace:", mi_k_with_key);
    CM("metadataItemsFromArray:filteredByIdentifier:", mi_k_with_ident);
    CM("metadataItemsFromArray:withLocale:", mi_k_with_locale);
    cx_add_cmethod(&AVMutableMetadataItem_c, "metadataItem", (void *)mm_item);
    MM("setIdentifier:", mm_set_identifier); MM("setKey:", mm_set_key); MM("setKeySpace:", mm_set_keyspace);
    MM("setValue:", mm_set_value); MM("setDataType:", mm_set_datatype); MM("setExtendedLanguageTag:", mm_set_lang);
    MM("setLocale:", mm_set_locale); MM("setExtraAttributes:", mm_set_extra); MM("setStartDate:", mm_set_startdate);
    MM("setTime:", mm_set_time); MM("setDuration:", mm_set_duration);
#undef M
#undef CM
#undef MM
}
