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
