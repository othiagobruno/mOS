/*
 * VideoToolbox que o OpenBot importa além das sessões que já devolvem erro.
 * Codificar e decodificar H.264/HEVC continua sem codec (o Create em vscode_media.c
 * devolve -12908). Copiar um CVPixelBuffer para outro, no mesmo formato e tamanho, é real.
 * Não há plugin profissional: o registro não instala encoder nenhum.
 */
#include "foundation.h"

void *malloc(size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);

extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } vt_cf_t;
#define CFCONST(var, text) \
    static vt_cf_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; \
    const void *var = &var##_s;

CFCONST(kVTCompressionPropertyKey_Quality, "Quality")
CFCONST(kVTCompressionPropertyKey_DataRateLimits, "DataRateLimits")
CFCONST(kVTCompressionPropertyKey_MaxH264SliceBytes, "MaxH264SliceBytes")
CFCONST(kVTCompressionPropertyKey_MoreFramesBeforeStart, "MoreFramesBeforeStart")
CFCONST(kVTCompressionPropertyKey_MoreFramesAfterEnd, "MoreFramesAfterEnd")
CFCONST(kVTCompressionPropertyKey_PixelAspectRatio, "PixelAspectRatio")
CFCONST(kVTCompressionPropertyKey_AllowOpenGOP, "AllowOpenGOP")
CFCONST(kVTCompressionPropertyKey_H264EntropyMode, "H264EntropyMode")
CFCONST(kVTH264EntropyMode_CABAC, "CABAC")
CFCONST(kVTPixelTransferPropertyKey_DestinationColorPrimaries, "DestinationColorPrimaries")
CFCONST(kVTPixelTransferPropertyKey_DestinationTransferFunction, "DestinationTransferFunction")
CFCONST(kVTPixelTransferPropertyKey_DestinationYCbCrMatrix, "DestinationYCbCrMatrix")
CFCONST(kVTPixelTransferPropertyKey_DestinationCleanAperture, "DestinationCleanAperture")
CFCONST(kVTPixelTransferPropertyKey_DownsamplingMode, "DownsamplingMode")
CFCONST(kVTPixelTransferPropertyKey_RealTime, "RealTime")
CFCONST(kVTProfileLevel_H264_Baseline_1_3, "H264_Baseline_1_3")
CFCONST(kVTProfileLevel_H264_Baseline_3_0, "H264_Baseline_3_0")
CFCONST(kVTProfileLevel_H264_Baseline_3_1, "H264_Baseline_3_1")
CFCONST(kVTProfileLevel_H264_Baseline_3_2, "H264_Baseline_3_2")
CFCONST(kVTProfileLevel_H264_Baseline_4_1, "H264_Baseline_4_1")
CFCONST(kVTProfileLevel_H264_High_5_0, "H264_High_5_0")
CFCONST(kVTProfileLevel_H264_Main_3_0, "H264_Main_3_0")
CFCONST(kVTProfileLevel_H264_Main_3_1, "H264_Main_3_1")
CFCONST(kVTProfileLevel_H264_Main_3_2, "H264_Main_3_2")
CFCONST(kVTProfileLevel_H264_Main_4_0, "H264_Main_4_0")
CFCONST(kVTProfileLevel_H264_Main_4_1, "H264_Main_4_1")
CFCONST(kVTProfileLevel_H264_Main_5_0, "H264_Main_5_0")

#define kVTCouldNotFindVideoEncoderErr (-12908)
#define kVTPropertyNotSupportedErr (-12900)

void *VTCompressionSessionGetPixelBufferPool(void *s) { (void)s; return 0; }
int VTCompressionSessionPrepareToEncodeFrames(void *s) { (void)s; return 0; }
int VTCopySupportedPropertyDictionaryForEncoder(int w, int h, u32 codec, id spec, id *props, id *ident) {
    (void)w; (void)h; (void)codec; (void)spec;
    if (props) *props = 0;
    if (ident) *ident = 0;
    return kVTCouldNotFindVideoEncoderErr;
}
void VTRegisterProfessionalVideoWorkflowVideoEncoders(void) {}
/* Os plug-ins profissionais (ProRes RAW e afins) vêm de pacotes da Apple que o mOS não tem: não há o que registrar. */
void VTRegisterProfessionalVideoWorkflowVideoDecoders(void) {}

typedef struct { int live; } vtpx_t;
int VTPixelTransferSessionCreate(void *a, void **out) {
    (void)a;
    if (!out) return kVTPropertyNotSupportedErr;
    vtpx_t *s = malloc(sizeof *s);
    if (!s) return -108;
    s->live = 1;
    *out = s;
    return 0;
}
void VTPixelTransferSessionInvalidate(void *s) { if (s) ((vtpx_t *)s)->live = 0; }

int CVPixelBufferLockBaseAddress(void *p, u64 flags);
int CVPixelBufferUnlockBaseAddress(void *p, u64 flags);
void *CVPixelBufferGetBaseAddress(void *p);
u64 CVPixelBufferGetBytesPerRow(void *p);
u64 CVPixelBufferGetWidth(void *p);
u64 CVPixelBufferGetHeight(void *p);
u32 CVPixelBufferGetPixelFormatType(void *p);

int VTPixelTransferSessionTransferImage(void *s, void *src, void *dst) {
    vtpx_t *x = s;
    if (!x || !x->live || !src || !dst) return kVTPropertyNotSupportedErr;
    if (CVPixelBufferGetPixelFormatType(src) != CVPixelBufferGetPixelFormatType(dst)) return kVTPropertyNotSupportedErr;
    u64 w = CVPixelBufferGetWidth(src), h = CVPixelBufferGetHeight(src);
    if (!w || w != CVPixelBufferGetWidth(dst) || h != CVPixelBufferGetHeight(dst)) return kVTPropertyNotSupportedErr;
    if (CVPixelBufferLockBaseAddress(src, 1) || CVPixelBufferLockBaseAddress(dst, 0)) return kVTPropertyNotSupportedErr;
    u8 *from = CVPixelBufferGetBaseAddress(src);
    u8 *to = CVPixelBufferGetBaseAddress(dst);
    u64 sb = CVPixelBufferGetBytesPerRow(src), db = CVPixelBufferGetBytesPerRow(dst);
    int rc = 0;
    if (!from || !to || sb < w * 4 || db < w * 4) rc = kVTPropertyNotSupportedErr;
    else {
        u64 row = w * 4;
        for (u64 r = 0; r < h; r++) memcpy(to + r * db, from + r * sb, (size_t)row);
    }
    CVPixelBufferUnlockBaseAddress(src, 1);
    CVPixelBufferUnlockBaseAddress(dst, 0);
    return rc;
}

/* ---- lacunas do corpus: perfis H.264, chaves de decodificação e a lista de codificadores */
CFCONST(kVTProfileLevel_H264_Baseline_4_0, "H264_Baseline_4_0") CFCONST(kVTProfileLevel_H264_Baseline_4_2, "H264_Baseline_4_2")
CFCONST(kVTProfileLevel_H264_Baseline_5_0, "H264_Baseline_5_0") CFCONST(kVTProfileLevel_H264_Baseline_5_1, "H264_Baseline_5_1")
CFCONST(kVTProfileLevel_H264_Baseline_5_2, "H264_Baseline_5_2") CFCONST(kVTProfileLevel_H264_High_3_0, "H264_High_3_0")
CFCONST(kVTProfileLevel_H264_High_3_1, "H264_High_3_1") CFCONST(kVTProfileLevel_H264_High_3_2, "H264_High_3_2")
CFCONST(kVTProfileLevel_H264_High_4_0, "H264_High_4_0") CFCONST(kVTProfileLevel_H264_High_4_1, "H264_High_4_1")
CFCONST(kVTProfileLevel_H264_High_4_2, "H264_High_4_2") CFCONST(kVTProfileLevel_H264_High_5_1, "H264_High_5_1")
CFCONST(kVTProfileLevel_H264_High_5_2, "H264_High_5_2") CFCONST(kVTProfileLevel_H264_Main_4_2, "H264_Main_4_2")
CFCONST(kVTProfileLevel_H264_Main_5_1, "H264_Main_5_1") CFCONST(kVTProfileLevel_H264_Main_5_2, "H264_Main_5_2")
CFCONST(kVTDecompressionPropertyKey_UsingHardwareAcceleratedVideoDecoder, "UsingHardwareAcceleratedVideoDecoder")
CFCONST(kVTDecompressionPropertyKey_RealTime, "RealTime")
CFCONST(kVTCompressionPropertyKey_ConstantBitRate, "ConstantBitRate")
CFCONST(kVTH264EntropyMode_CAVLC, "CAVLC")
CFCONST(kVTVideoEncoderList_CodecType, "CodecType") CFCONST(kVTVideoEncoderList_DisplayName, "DisplayName")
CFCONST(kVTVideoEncoderList_EncoderID, "EncoderID") CFCONST(kVTVideoEncoderList_IsHardwareAccelerated, "IsHardwareAccelerated")
CFCONST(kVTVideoEncoderSpecification_EncoderID, "EncoderID")
/* um codificador: H.264 em software (OpenH264) */
id objc_retain(id o);
int VTCopyVideoEncoderList(id options, id *out) {
    (void)options;
    if (!out) return -12902;
    id d = MSG0(id, (id)cx_class_named("NSMutableDictionary"), "dictionary");
    MSG(void, d, "setObject:forKey:", cx_num_i(0x61766331), cx_str("CodecType"));
    MSG(void, d, "setObject:forKey:", cx_str("H.264"), cx_str("CodecName"));
    MSG(void, d, "setObject:forKey:", cx_str("mOS H.264 (OpenH264)"), cx_str("DisplayName"));
    MSG(void, d, "setObject:forKey:", cx_str("com.mos.openh264"), cx_str("EncoderID"));
    MSG(void, d, "setObject:forKey:", cx_str("mOS"), cx_str("EncoderName"));
    id no = MSG(id, (id)cx_class_named("NSNumber"), "numberWithBool:", (BOOL)0);
    MSG(void, d, "setObject:forKey:", no, cx_str("IsHardwareAccelerated"));
    *out = objc_retain(MSG(id, (id)cx_class_named("NSArray"), "arrayWithObject:", d));
    return 0;
}

/* o codificador do mOS faz uma passada só (canPerformMultiplePasses = NO): não há trechos para outra passada */
typedef struct { long long value; int timescale; unsigned flags; long long epoch; } vt_time_t;
typedef struct { vt_time_t start, duration; } vt_range_t;
int VTCompressionSessionGetTimeRangesForNextPass(void *session, long *count, const vt_range_t **ranges) {
    if (!session || !count || !ranges) return -12902;   /* kVTParameterErr */
    *count = 0;
    *ranges = 0;
    return 0;
}

/* ---- VTPixelRotationSession: girar 90/180/270 e espelhar CVPixelBuffer de 32 bits por pixel (BGRA/ARGB/RGBA),
 * de verdade. As propriedades chegam pelo VTSessionSetProperty (vscode_media.c chama cx_vt_rot_property primeiro). */
CFCONST(kVTPixelRotationPropertyKey_Rotation, "Rotation")
CFCONST(kVTPixelRotationPropertyKey_FlipHorizontalOrientation, "FlipHorizontalOrientation")
CFCONST(kVTPixelRotationPropertyKey_FlipVerticalOrientation, "FlipVerticalOrientation")
CFCONST(kVTRotation_0, "Rotation_0") CFCONST(kVTRotation_CW90, "Rotation_CW90")
CFCONST(kVTRotation_180, "Rotation_180") CFCONST(kVTRotation_CCW90, "Rotation_CCW90")
CFCONST(kVTScalingMode_Normal, "Normal") CFCONST(kVTScalingMode_Letterbox, "Letterbox")
CFCONST(kVTCompressionPropertyKey_AllowTemporalCompression, "AllowTemporalCompression")
int strcmp(const char *a, const char *b);
#define ROT_MAGIC 0x524f5431u
typedef struct { u32 magic; int quarter; int fliph, flipv; int live; } vtrot_t;
int VTPixelRotationSessionCreate(void *alloc, void **out) {
    (void)alloc;
    if (!out) return kVTPropertyNotSupportedErr;
    vtrot_t *r = malloc(sizeof *r);
    if (!r) return -108;
    r->magic = ROT_MAGIC; r->quarter = 0; r->fliph = r->flipv = 0; r->live = 1;
    *out = r;
    return 0;
}
void VTPixelRotationSessionInvalidate(void *s) { vtrot_t *r = s; if (r && r->magic == ROT_MAGIC) r->live = 0; }
u64 VTPixelRotationSessionGetTypeID(void) { return 0x2E0; }
/* 1 = era sessão de rotação (o resultado em *rc) */
int cx_vt_rot_property(void *s, id key, id val, int *rc) {
    vtrot_t *r = s;
    if (!r || r->magic != ROT_MAGIC) return 0;
    const char *k = key ? cx_cstr(key) : "";
    *rc = 0;
    if (!strcmp(k, "Rotation")) {
        const char *v = val ? cx_cstr(val) : "";
        r->quarter = !strcmp(v, "Rotation_CW90") ? 1 : !strcmp(v, "Rotation_180") ? 2 : !strcmp(v, "Rotation_CCW90") ? 3 : 0;
    } else if (!strcmp(k, "FlipHorizontalOrientation")) r->fliph = val && MSG0(BOOL, val, "boolValue");
    else if (!strcmp(k, "FlipVerticalOrientation")) r->flipv = val && MSG0(BOOL, val, "boolValue");
    else *rc = kVTPropertyNotSupportedErr;
    return 1;
}
int VTPixelRotationSessionRotateImage(void *s, void *src, void *dst) {
    vtrot_t *r = s;
    if (!r || r->magic != ROT_MAGIC || !r->live || !src || !dst) return kVTPropertyNotSupportedErr;
    if (CVPixelBufferGetPixelFormatType(src) != CVPixelBufferGetPixelFormatType(dst)) return kVTPropertyNotSupportedErr;
    u64 w = CVPixelBufferGetWidth(src), h = CVPixelBufferGetHeight(src);
    u64 ow = r->quarter & 1 ? h : w, oh = r->quarter & 1 ? w : h;
    if (!w || CVPixelBufferGetWidth(dst) != ow || CVPixelBufferGetHeight(dst) != oh) return kVTPropertyNotSupportedErr;
    if (CVPixelBufferLockBaseAddress(src, 1) || CVPixelBufferLockBaseAddress(dst, 0)) return kVTPropertyNotSupportedErr;
    const u8 *from = CVPixelBufferGetBaseAddress(src);
    u8 *to = CVPixelBufferGetBaseAddress(dst);
    u64 sb = CVPixelBufferGetBytesPerRow(src), db = CVPixelBufferGetBytesPerRow(dst);
    int rc = 0;
    if (!from || !to || sb < w * 4 || db < ow * 4) rc = kVTPropertyNotSupportedErr;
    else
        for (u64 y = 0; y < oh; y++)
            for (u64 x = 0; x < ow; x++) {
                u64 dx = r->fliph ? ow - 1 - x : x, dy = r->flipv ? oh - 1 - y : y;   /* espelho depois da rotação */
                u64 sx, sy;
                switch (r->quarter) {
                case 1: sx = dy; sy = h - 1 - dx; break;             /* 90° horário */
                case 2: sx = w - 1 - dx; sy = h - 1 - dy; break;
                case 3: sx = w - 1 - dy; sy = dx; break;             /* 90° anti-horário */
                default: sx = dx; sy = dy;
                }
                memcpy(to + y * db + x * 4, from + sy * sb + sx * 4, 4);
            }
    CVPixelBufferUnlockBaseAddress(src, 1);
    CVPixelBufferUnlockBaseAddress(dst, 0);
    return rc;
}
