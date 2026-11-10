/* OpenBot 0.66.0: o que o machocheck ainda marcava em sistema, fora de DNS (#73)
 * e das funções em openbot_pure.c. Constantes são CFString. gap_stub devolve 0. */
#include "foundation.h"
#include "libc.h"
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; unsigned long long flags; const char *s; unsigned long long n; } gap_cf_t;
#define GAPCF(var, text) \
    static gap_cf_t var##_box = {(void *)cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; \
    const void *var = &var##_box;
GAPCF(kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly, "kSecAttrAccessibleWhenPasscodeSetThisDeviceOnly")
GAPCF(kCFCopyStringSetCallBacks, "kCFCopyStringSetCallBacks")
GAPCF(kCFTypeBagCallBacks, "kCFTypeBagCallBacks")
GAPCF(kCMFormatDescriptionExtensionKey_MetadataKeyTable, "kCMFormatDescriptionExtensionKey_MetadataKeyTable")
GAPCF(kCMMetadataFormatDescriptionKey_ConformingDataTypes, "kCMMetadataFormatDescriptionKey_ConformingDataTypes")
GAPCF(kCMMetadataFormatDescriptionKey_DataType, "kCMMetadataFormatDescriptionKey_DataType")
GAPCF(kCMMetadataFormatDescriptionKey_DataTypeNamespace, "kCMMetadataFormatDescriptionKey_DataTypeNamespace")
GAPCF(kCMMetadataFormatDescriptionKey_LanguageTag, "kCMMetadataFormatDescriptionKey_LanguageTag")
GAPCF(kCMMetadataFormatDescriptionKey_LocalID, "kCMMetadataFormatDescriptionKey_LocalID")
GAPCF(kCMMetadataFormatDescriptionKey_Namespace, "kCMMetadataFormatDescriptionKey_Namespace")
GAPCF(kCMMetadataFormatDescriptionKey_SetupData, "kCMMetadataFormatDescriptionKey_SetupData")
GAPCF(kCMMetadataFormatDescriptionKey_StructuralDependency, "kCMMetadataFormatDescriptionKey_StructuralDependency")
GAPCF(kCMMetadataFormatDescriptionKey_Value, "kCMMetadataFormatDescriptionKey_Value")
GAPCF(kCMMetadataFormatDescriptionMetadataSpecificationKey_DataType, "kCMMetadataFormatDescriptionMetadataSpecificationKey_DataType")
GAPCF(kCMMetadataFormatDescriptionMetadataSpecificationKey_ExtendedLanguageTag, "kCMMetadataFormatDescriptionMetadataSpecificationKey_ExtendedLanguageTag")
GAPCF(kCMMetadataFormatDescriptionMetadataSpecificationKey_Identifier, "kCMMetadataFormatDescriptionMetadataSpecificationKey_Identifier")
GAPCF(kCMMetadataFormatDescriptionMetadataSpecificationKey_SetupData, "kCMMetadataFormatDescriptionMetadataSpecificationKey_SetupData")
GAPCF(kCMMetadataFormatDescriptionMetadataSpecificationKey_StructuralDependency, "kCMMetadataFormatDescriptionMetadataSpecificationKey_StructuralDependency")
GAPCF(kCMMetadataFormatDescription_StructuralDependencyKey_DependencyIsInvalidFlag, "kCMMetadataFormatDescription_StructuralDependencyKey_DependencyIsInvalidFlag")
GAPCF(kCGImageDestinationLossyCompressionQuality, "kCGImageDestinationLossyCompressionQuality")
GAPCF(kCMFormatDescriptionExtension_VerbatimSampleDescription, "kCMFormatDescriptionExtension_VerbatimSampleDescription")
typedef struct { long long v; int scale; int flags; } gap_cmtime_t;
const gap_cmtime_t kCMTimeIndefinite = {0, 0, 3};
GAPCF(kCVImageBufferChromaLocation_Bottom, "kCVImageBufferChromaLocation_Bottom")
GAPCF(kCVImageBufferChromaLocation_BottomLeft, "kCVImageBufferChromaLocation_BottomLeft")
GAPCF(kCVImageBufferChromaLocation_Center, "kCVImageBufferChromaLocation_Center")
GAPCF(kCVImageBufferChromaLocation_Left, "kCVImageBufferChromaLocation_Left")
GAPCF(kCVImageBufferChromaLocation_Top, "kCVImageBufferChromaLocation_Top")
GAPCF(kCVImageBufferChromaLocation_TopLeft, "kCVImageBufferChromaLocation_TopLeft")
GAPCF(kCVImageBufferPixelAspectRatioHorizontalSpacingKey, "kCVImageBufferPixelAspectRatioHorizontalSpacingKey")
GAPCF(kCVImageBufferPixelAspectRatioKey, "kCVImageBufferPixelAspectRatioKey")
GAPCF(kCVImageBufferPixelAspectRatioVerticalSpacingKey, "kCVImageBufferPixelAspectRatioVerticalSpacingKey")
GAPCF(kVTCompressionPropertyKey_DataRateLimits, "kVTCompressionPropertyKey_DataRateLimits")
GAPCF(kVTCompressionPropertyKey_MaxH264SliceBytes, "kVTCompressionPropertyKey_MaxH264SliceBytes")
GAPCF(kVTCompressionPropertyKey_MoreFramesAfterEnd, "kVTCompressionPropertyKey_MoreFramesAfterEnd")
GAPCF(kVTCompressionPropertyKey_MoreFramesBeforeStart, "kVTCompressionPropertyKey_MoreFramesBeforeStart")
GAPCF(kVTCompressionPropertyKey_PixelAspectRatio, "kVTCompressionPropertyKey_PixelAspectRatio")
GAPCF(kVTCompressionPropertyKey_Quality, "kVTCompressionPropertyKey_Quality")
GAPCF(kVTProfileLevel_H264_Baseline_1_3, "kVTProfileLevel_H264_Baseline_1_3")
GAPCF(kVTProfileLevel_H264_Baseline_3_0, "kVTProfileLevel_H264_Baseline_3_0")
GAPCF(kVTProfileLevel_H264_Baseline_3_1, "kVTProfileLevel_H264_Baseline_3_1")
GAPCF(kVTProfileLevel_H264_Baseline_3_2, "kVTProfileLevel_H264_Baseline_3_2")
GAPCF(kVTProfileLevel_H264_Baseline_4_1, "kVTProfileLevel_H264_Baseline_4_1")
GAPCF(kVTProfileLevel_H264_High_5_0, "kVTProfileLevel_H264_High_5_0")
GAPCF(kVTProfileLevel_H264_Main_3_0, "kVTProfileLevel_H264_Main_3_0")
GAPCF(kVTProfileLevel_H264_Main_3_1, "kVTProfileLevel_H264_Main_3_1")
GAPCF(kVTProfileLevel_H264_Main_3_2, "kVTProfileLevel_H264_Main_3_2")
GAPCF(kVTProfileLevel_H264_Main_4_0, "kVTProfileLevel_H264_Main_4_0")
GAPCF(kVTProfileLevel_H264_Main_4_1, "kVTProfileLevel_H264_Main_4_1")
GAPCF(kVTProfileLevel_H264_Main_5_0, "kVTProfileLevel_H264_Main_5_0")

size_t CGImageGetWidth(void *img);
size_t CGImageGetHeight(void *img);
size_t CGImageGetBytesPerRow(void *img);
void *CGImageGetDataProvider(void *img);
void *CGDataProviderCopyData(void *p);
const unsigned char *CFDataGetBytePtr(void *d);
long CFDataGetLength(void *d);
void CFRelease(void *o);
int open(const char *path, int flags, ...);
long write(int fd, const void *b, unsigned long n);
int close(int fd);
static unsigned gap_crc(const unsigned char *s, unsigned long n) {
    unsigned c = 0xffffffffu; unsigned long i; int k;
    for (i = 0; i < n; i++) { c ^= s[i]; for (k = 0; k < 8; k++) c = (c >> 1) ^ (0xEDB88320u & (0u - (c & 1))); }
    return c ^ 0xffffffffu;
}
static void gap_chunk(unsigned char *png, unsigned long *o, const char *typ, const unsigned char *data, unsigned long n) {
    unsigned c; unsigned char *p = png + *o;
    p[0] = (unsigned char)(n >> 24); p[1] = (unsigned char)(n >> 16); p[2] = (unsigned char)(n >> 8); p[3] = (unsigned char)n;
    memcpy(p + 4, typ, 4); if (n && data) memcpy(p + 8, data, n);
    c = gap_crc(p + 4, 4 + n);
    p[8 + n] = (unsigned char)(c >> 24); p[9 + n] = (unsigned char)(c >> 16);
    p[10 + n] = (unsigned char)(c >> 8); p[11 + n] = (unsigned char)c;
    *o += 12 + n;
}
static int gap_png(const unsigned char *px, int w, int h, int bpr, unsigned char **out, unsigned long *n) {
    unsigned long row, rawn, zcap, zo, off, y, x;
    unsigned char *raw, *z, *png, ih[13];
    unsigned ad_a = 1, ad_b = 0;
    if (!px || w < 1 || h < 1 || bpr < w * 4) return -1;
    row = (unsigned long)w * 4ul + 1ul; rawn = row * (unsigned long)h;
    raw = malloc(rawn); if (!raw) return -1;
    for (y = 0; y < (unsigned long)h; y++) {
        unsigned char *r = raw + y * row; r[0] = 0;
        for (x = 0; x < (unsigned long)w; x++) {
            const unsigned char *q = px + y * (unsigned long)bpr + x * 4ul;
            unsigned char *d = r + 1 + x * 4ul;
            d[0] = q[2]; d[1] = q[1]; d[2] = q[0]; d[3] = q[3];
        }
    }
    zcap = rawn + (rawn / 65535ul + 1ul) * 5ul + 6ul;
    z = malloc(zcap); if (!z) { free(raw); return -1; }
    z[0] = 0x78; z[1] = 0x01; zo = 2; off = 0;
    while (off < rawn) {
        unsigned long chunk = rawn - off; int last; if (chunk > 65535ul) chunk = 65535ul;
        last = off + chunk == rawn;
        z[zo++] = (unsigned char)last; z[zo++] = (unsigned char)chunk; z[zo++] = (unsigned char)(chunk >> 8);
        z[zo++] = (unsigned char)(chunk ^ 0xffu); z[zo++] = (unsigned char)((chunk >> 8) ^ 0xffu);
        memcpy(z + zo, raw + off, chunk); zo += chunk; off += chunk;
    }
    for (off = 0; off < rawn; off++) { ad_a = (ad_a + raw[off]) % 65521u; ad_b = (ad_b + ad_a) % 65521u; }
    z[zo++] = (unsigned char)((ad_b << 16 | ad_a) >> 24); z[zo++] = (unsigned char)((ad_b << 16 | ad_a) >> 16);
    z[zo++] = (unsigned char)((ad_b << 16 | ad_a) >> 8); z[zo++] = (unsigned char)(ad_b << 16 | ad_a);
    free(raw);
    png = malloc(8 + 25 + 12 + zo + 12); if (!png) { free(z); return -1; }
    memcpy(png, "\x89PNG\r\n\x1a\n", 8); off = 8;
    ih[0] = (unsigned char)(w >> 24); ih[1] = (unsigned char)(w >> 16); ih[2] = (unsigned char)(w >> 8); ih[3] = (unsigned char)w;
    ih[4] = (unsigned char)(h >> 24); ih[5] = (unsigned char)(h >> 16); ih[6] = (unsigned char)(h >> 8); ih[7] = (unsigned char)h;
    ih[8] = 8; ih[9] = 6; ih[10] = ih[11] = ih[12] = 0;
    gap_chunk(png, &off, "IHDR", ih, 13);
    gap_chunk(png, &off, "IDAT", z, zo);
    gap_chunk(png, &off, "IEND", 0, 0);
    free(z); *out = png; *n = off; return 0;
}
typedef struct { id url; id img; } gap_dest;
void *CGImageDestinationCreateWithURL(id url, id type, unsigned long count, id opts) {
    gap_dest *d; (void)type; (void)count; (void)opts;
    d = calloc(1, sizeof *d); if (!d) return 0; d->url = url; return d;
}
void CGImageDestinationAddImage(void *dst, id img, id props) {
    gap_dest *d = dst; (void)props; if (d) d->img = img;
}
int CGImageDestinationFinalize(void *dst) {
    gap_dest *d = dst; int w, h, bpr, fd; void *prov, *data; const unsigned char *px;
    unsigned char *png; unsigned long pn; id path; const char *c;
    if (!d || !d->img || !d->url) return 0;
    w = (int)CGImageGetWidth(d->img); h = (int)CGImageGetHeight(d->img); bpr = (int)CGImageGetBytesPerRow(d->img);
    prov = CGImageGetDataProvider(d->img);
    data = prov ? CGDataProviderCopyData(prov) : 0;
    if (!data) return 0;
    px = CFDataGetBytePtr(data);
    if (gap_png(px, w, h, bpr, &png, &pn)) { CFRelease(data); return 0; }
    CFRelease(data);
    path = MSG0(id, d->url, "path");
    c = path ? cx_cstr(path) : 0;
    if (!c) { free(png); return 0; }
    fd = open(c, 577, 0644);
    if (fd < 0) { free(png); return 0; }
    if (write(fd, png, pn) != (long)pn) { close(fd); free(png); return 0; }
    close(fd); free(png); return 1;
}
int AudioHardwareCreateProcessTap(void *desc, unsigned *out) { (void)desc; if (out) *out = 0; return 1852797029; }
int AudioHardwareDestroyProcessTap(unsigned tap) { (void)tap; return 1852797029; }

cx_class_t AVCaptureScreenInput_c __asm__("OBJC_CLASS_$_AVCaptureScreenInput");
cx_class_t AVCaptureScreenInput_m __asm__("OBJC_METACLASS_$_AVCaptureScreenInput");
void cx_openbot_gap_init(void) {
    cx_class_t *obj;
    if (cx_class_named("AVCaptureScreenInput")) return;
    obj = cx_class_named("NSObject");
    cx_define_class(&AVCaptureScreenInput_c, &AVCaptureScreenInput_m, "AVCaptureScreenInput", obj, 0);
}

/* gap_stub: símbolo presente, sem o comportamento do macOS. */
long CVPixelBufferFillExtendedPixels(void) { return 0; }
long CVPixelBufferGetExtendedPixels(void) { return 0; }
long CVPixelBufferPoolGetTypeID(void) { return 0; }
long CFAttributedStringGetLength(void) { return 0; }
long CFAttributedStringGetString(void) { return 0; }
long CFAttributedStringGetTypeID(void) { return 0; }
long CFBagContainsValue(void) { return 0; }
long CFBagCreate(void) { return 0; }
long CFBagGetCount(void) { return 0; }
long CFBagGetCountOfValue(void) { return 0; }
long CFBagGetTypeID(void) { return 0; }
long CFBundleCopyResourceURL(void) { return 0; }
long CFBundleGetTypeID(void) { return 0; }
long CFCalendarCopyCurrent(void) { return 0; }
long CFCalendarCopyTimeZone(void) { return 0; }
long CFCalendarCreateWithIdentifier(void) { return 0; }
long CFCalendarGetIdentifier(void) { return 0; }
long CFCalendarGetTypeID(void) { return 0; }
long CFCalendarSetTimeZone(void) { return 0; }
long CFCharacterSetCreateInvertedSet(void) { return 0; }
long CFCharacterSetCreateWithCharactersInString(void) { return 0; }
long CFCharacterSetGetTypeID(void) { return 0; }
long CFCharacterSetIsCharacterMember(void) { return 0; }
long CFDateFormatterCreate(void) { return 0; }
long CFDateFormatterCreateStringWithDate(void) { return 0; }
long CFDateFormatterGetTypeID(void) { return 0; }
long CFErrorCopyFailureReason(void) { return 0; }
long CFFileDescriptorCreate(void) { return 0; }
long CFFileDescriptorGetNativeDescriptor(void) { return 0; }
long CFFileDescriptorGetTypeID(void) { return 0; }
long CFFileDescriptorInvalidate(void) { return 0; }
long CFFileSecurityCopyOwnerUUID(void) { return 0; }
long CFFileSecurityGetTypeID(void) { return 0; }
long CFFileSecuritySetOwnerUUID(void) { return 0; }
long CFLocaleGetIdentifier(void) { return 0; }
long CFLocaleGetTypeID(void) { return 0; }
long CFMessagePortCreateLocal(void) { return 0; }
long CFMessagePortCreateRemote(void) { return 0; }
long CFMessagePortCreateRunLoopSource(void) { return 0; }
long CFMessagePortGetTypeID(void) { return 0; }
long CFMessagePortInvalidate(void) { return 0; }
long CFMessagePortSendRequest(void) { return 0; }
long CFNotificationCenterGetTypeID(void) { return 0; }
long CFNumberFormatterCreate(void) { return 0; }
long CFNumberFormatterCreateNumberFromString(void) { return 0; }
long CFNumberFormatterCreateStringWithNumber(void) { return 0; }
long CFNumberFormatterGetTypeID(void) { return 0; }
long CFPropertyListCreateData(void) { return 0; }
long CFPropertyListCreateDeepCopy(void) { return 0; }
long CFPropertyListCreateWithStream(void) { return 0; }
long CFPropertyListWrite(void) { return 0; }
long CFReadStreamGetTypeID(void) { return 0; }
long CFRunLoopGetTypeID(void) { return 0; }
long CFRunLoopTimerCreateWithHandler(void) { return 0; }
long CFRunLoopTimerGetTypeID(void) { return 0; }
long CFSetContainsValue(void) { return 0; }
long CFSetCreateCopy(void) { return 0; }
long CFSetCreateMutableCopy(void) { return 0; }
long CFSetGetCountOfValue(void) { return 0; }
long CFSetGetTypeID(void) { return 0; }
long CFSetGetValue(void) { return 0; }
long CFSetGetValueIfPresent(void) { return 0; }
long CFSetRemoveAllValues(void) { return 0; }
long CFSetRemoveValue(void) { return 0; }
long CFSetReplaceValue(void) { return 0; }
long CFSetSetValue(void) { return 0; }
long CFSocketCreate(void) { return 0; }
long CFSocketGetNative(void) { return 0; }
long CFSocketGetTypeID(void) { return 0; }
long CFSocketInvalidate(void) { return 0; }
long CFSocketIsValid(void) { return 0; }
long CFStreamCreateBoundPair(void) { return 0; }
long CFTimeZoneCreateWithName(void) { return 0; }
long CFTimeZoneGetSecondsFromGMT(void) { return 0; }
long CFTimeZoneGetTypeID(void) { return 0; }
long CFUUIDCreateFromString(void) { return 0; }
long CFWriteStreamGetTypeID(void) { return 0; }
long CFXMLCreateStringByEscapingEntities(void) { return 0; }
long CFXMLCreateStringByUnescapingEntities(void) { return 0; }
long CMBlockBufferCopyDataBytes(void) { return 0; }
long CMBlockBufferIsEmpty(void) { return 0; }
long CMFormatDescriptionGetExtensions(void) { return 0; }
long CMFormatDescriptionGetMediaType(void) { return 0; }
long CMMetadataFormatDescriptionCreateByMergingMetadataFormatDescriptions(void) { return 0; }
long CMMetadataFormatDescriptionCreateWithKeys(void) { return 0; }
long CMMetadataFormatDescriptionCreateWithMetadataFormatDescriptionAndMetadataSpecifications(void) { return 0; }
long CMMetadataFormatDescriptionCreateWithMetadataSpecifications(void) { return 0; }
long CMMetadataFormatDescriptionGetIdentifiers(void) { return 0; }
long CMMetadataFormatDescriptionGetKeyWithLocalID(void) { return 0; }
long CMSampleBufferCopyPCMDataIntoAudioBufferList(void) { return 0; }
long CMSampleBufferCreateCopyWithNewTiming(void) { return 0; }
long CMSampleBufferCreateReadyWithImageBuffer(void) { return 0; }
long CMSampleBufferDataIsReady(void) { return 0; }
long CMSampleBufferGetAudioBufferListWithRetainedBlockBuffer(void) { return 0; }
long CMSampleBufferGetDecodeTimeStamp(void) { return 0; }
long CMSampleBufferGetDuration(void) { return 0; }
long CMSampleBufferGetOutputPresentationTimeStamp(void) { return 0; }
long CMSampleBufferGetSampleSize(void) { return 0; }
long CMSampleBufferGetSampleTimingInfo(void) { return 0; }
long CMSampleBufferGetTotalSampleSize(void) { return 0; }
long CMSampleBufferInvalidate(void) { return 0; }
long CMSampleBufferIsValid(void) { return 0; }
long CMSampleBufferMakeDataReady(void) { return 0; }
long CMSampleBufferSetOutputPresentationTimeStamp(void) { return 0; }
long IOSurfacePropertyKeyAllocSize(void) { return 0; }
long IOSurfacePropertyKeyBytesPerElement(void) { return 0; }
long IOSurfacePropertyKeyBytesPerRow(void) { return 0; }
long IOSurfacePropertyKeyHeight(void) { return 0; }
long IOSurfacePropertyKeyPixelFormat(void) { return 0; }
long IOSurfacePropertyKeyPlaneBytesPerElement(void) { return 0; }
long IOSurfacePropertyKeyPlaneBytesPerRow(void) { return 0; }
long IOSurfacePropertyKeyPlaneElementHeight(void) { return 0; }
long IOSurfacePropertyKeyPlaneElementWidth(void) { return 0; }
long IOSurfacePropertyKeyPlaneHeight(void) { return 0; }
long IOSurfacePropertyKeyPlaneInfo(void) { return 0; }
long IOSurfacePropertyKeyPlaneOffset(void) { return 0; }
long IOSurfacePropertyKeyPlaneSize(void) { return 0; }
long IOSurfacePropertyKeyPlaneWidth(void) { return 0; }
long IOSurfacePropertyKeyWidth(void) { return 0; }
long AVCaptureDeviceTypeBuiltInMicrophone(void) { return 0; }
long AVFileTypeMPEG4(void) { return 0; }
long AVFileTypeQuickTimeMovie(void) { return 0; }
long AVVideoCodecTypeH264(void) { return 0; }
long AVVideoCodecTypeHEVC(void) { return 0; }
long AVVideoScalingModeResizeAspect(void) { return 0; }
long CVColorPrimariesGetStringForIntegerCodePoint(void) { return 0; }
long CVTransferFunctionGetStringForIntegerCodePoint(void) { return 0; }
long CVYCbCrMatrixGetStringForIntegerCodePoint(void) { return 0; }
long VTCompressionSessionGetPixelBufferPool(void) { return 0; }
long VTCompressionSessionPrepareToEncodeFrames(void) { return 0; }
long VTCopySupportedPropertyDictionaryForEncoder(void) { return 0; }
long VTRegisterProfessionalVideoWorkflowVideoEncoders(void) { return 0; }
long AVNumberOfChannelsKey(void) { return 0; }
long gap_sw_0(void) __asm__("$s10Foundation22_convertNSErrorToErrorys0E0_pSo0C0CSgF");
long gap_sw_0(void) { return 0; }
long gap_sw_1(void) __asm__("$s10Foundation3URLV15fileURLWithPathACSSh_tcfC");
long gap_sw_1(void) { return 0; }
long gap_sw_2(void) __asm__("$s10Foundation3URLV19_bridgeToObjectiveCSo5NSURLCyF");
long gap_sw_2(void) { return 0; }
long gap_sw_3(void) __asm__("$s10Foundation3URLV36_unconditionallyBridgeFromObjectiveCyACSo5NSURLCSgFZ");
long gap_sw_3(void) { return 0; }
long gap_sw_4(void) __asm__("$s10Foundation3URLV4pathSSvg");
long gap_sw_4(void) { return 0; }
long gap_sw_5(void) __asm__("$s10Foundation3URLVMa");
long gap_sw_5(void) { return 0; }
long gap_sw_6(void) __asm__("$sSD10FoundationE19_bridgeToObjectiveCSo12NSDictionaryCyF");
long gap_sw_6(void) { return 0; }
long gap_sw_7(void) __asm__("$sSS10FoundationE19_bridgeToObjectiveCSo8NSStringCyF");
long gap_sw_7(void) { return 0; }
long gap_sw_8(void) __asm__("$sSS10FoundationE26_forceBridgeFromObjectiveC_6resultySo8NSStringC_SSSgztFZ");
long gap_sw_8(void) { return 0; }
long gap_sw_9(void) __asm__("$sSS10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo8NSStringC_SSSgztFZ");
long gap_sw_9(void) { return 0; }
long gap_sw_10(void) __asm__("$sSS10FoundationE36_unconditionallyBridgeFromObjectiveCySSSo8NSStringCSgFZ");
long gap_sw_10(void) { return 0; }
long gap_sw_11(void) __asm__("$sSS10FoundationE8EncodingV4utf8ACvgZ");
long gap_sw_11(void) { return 0; }
long gap_sw_12(void) __asm__("$sSS10FoundationE8EncodingVMa");
long gap_sw_12(void) { return 0; }
long gap_sw_13(void) __asm__("$sSa10FoundationE19_bridgeToObjectiveCSo7NSArrayCyF");
long gap_sw_13(void) { return 0; }
long gap_sw_14(void) __asm__("$sSa10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo7NSArrayC_SayxGSgztFZ");
long gap_sw_14(void) { return 0; }
long gap_sw_15(void) __asm__("$sSa10FoundationE36_unconditionallyBridgeFromObjectiveCySayxGSo7NSArrayCSgFZ");
long gap_sw_15(void) { return 0; }
long gap_sw_16(void) __asm__("$sSo12NSDictionaryC10FoundationE17dictionaryLiteralAByp_yptd_tcfC");
long gap_sw_16(void) { return 0; }
long gap_sw_17(void) __asm__("$sSy10FoundationE7cString5usingSays4Int8VGSgSSAAE8EncodingV_tF");
long gap_sw_17(void) { return 0; }
long gap_sw_18(void) __asm__("$ss5ErrorP10FoundationE20localizedDescriptionSSvg");
long gap_sw_18(void) { return 0; }
