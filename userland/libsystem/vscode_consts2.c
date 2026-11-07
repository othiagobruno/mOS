/*
 * mOS libSystem — constantes CFString/NSString que o Electron/VS Code importa de AppKit, CoreFoundation,
 * SystemConfiguration, CoreGraphics, HIToolbox, Security e LaunchServices (segunda leva, gerada por um script a partir do
 * relatório do tools/machocheck.c). Valores iguais aos do macOS: acessibilidade "AX...", tipos de pasteboard
 * em UTI, chaves do SystemConfiguration sem prefixo; notificações e chaves NSURL valem o próprio nome.
 */
#include "sys.h"
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } vk_const_t;
#define VK(var, text) static vk_const_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; const void *var = &var##_s;
VK(ASAuthorizationErrorDomain, "ASAuthorizationErrorDomain")
VK(ASAuthorizationOperationLogin, "ASAuthorizationOperationLogin")
VK(ASAuthorizationOperationRefresh, "ASAuthorizationOperationRefresh")
VK(ASAuthorizationPublicKeyCredentialAttestationKindNone, "ASAuthorizationPublicKeyCredentialAttestationKindNone")
VK(ASAuthorizationPublicKeyCredentialUserVerificationPreferenceDiscouraged, "ASAuthorizationPublicKeyCredentialUserVerificationPreferenceDiscouraged")
VK(ASAuthorizationPublicKeyCredentialUserVerificationPreferencePreferred, "ASAuthorizationPublicKeyCredentialUserVerificationPreferencePreferred")
VK(ASAuthorizationPublicKeyCredentialUserVerificationPreferenceRequired, "ASAuthorizationPublicKeyCredentialUserVerificationPreferenceRequired")
VK(AVCaptureDeviceTypeBuiltInWideAngleCamera, "AVCaptureDeviceTypeBuiltInWideAngleCamera")
VK(AVCaptureDeviceTypeExternalUnknown, "AVCaptureDeviceTypeExternalUnknown")
VK(AVCaptureDeviceWasConnectedNotification, "AVCaptureDeviceWasConnectedNotification")
VK(AVCaptureDeviceWasDisconnectedNotification, "AVCaptureDeviceWasDisconnectedNotification")
VK(AVCaptureSessionErrorKey, "AVCaptureSessionErrorKey")
VK(AVCaptureSessionRuntimeErrorNotification, "AVCaptureSessionRuntimeErrorNotification")
VK(AVLayerVideoGravityResize, "AVLayerVideoGravityResize")
VK(AVMediaTypeAudio, "soun")
VK(AVMediaTypeMuxed, "muxx")
VK(AVMediaTypeVideo, "vide")
VK(AVVideoCodecKey, "AVVideoCodecKey")
VK(AVVideoCodecTypeJPEG, "jpeg")
VK(AVVideoScalingModeKey, "AVVideoScalingModeKey")
VK(AVVideoScalingModeResizeAspectFill, "AVVideoScalingModeResizeAspectFill")
VK(CBAdvertisementDataLocalNameKey, "CBAdvertisementDataLocalNameKey")
VK(CBAdvertisementDataManufacturerDataKey, "CBAdvertisementDataManufacturerDataKey")
VK(CBAdvertisementDataOverflowServiceUUIDsKey, "CBAdvertisementDataOverflowServiceUUIDsKey")
VK(CBAdvertisementDataServiceDataKey, "CBAdvertisementDataServiceDataKey")
VK(CBAdvertisementDataServiceUUIDsKey, "CBAdvertisementDataServiceUUIDsKey")
VK(CBAdvertisementDataTxPowerLevelKey, "CBAdvertisementDataTxPowerLevelKey")
VK(CBCentralManagerScanOptionAllowDuplicatesKey, "CBCentralManagerScanOptionAllowDuplicatesKey")
VK(CHHapticDynamicParameterIDHapticIntensityControl, "CHHapticDynamicParameterIDHapticIntensityControl")
VK(CHHapticDynamicParameterIDHapticSharpnessControl, "CHHapticDynamicParameterIDHapticSharpnessControl")
VK(CHHapticEventParameterIDHapticIntensity, "CHHapticEventParameterIDHapticIntensity")
VK(CHHapticEventParameterIDHapticSharpness, "CHHapticEventParameterIDHapticSharpness")
VK(CHHapticEventTypeHapticContinuous, "CHHapticEventTypeHapticContinuous")
VK(CIDetectorAccuracy, "CIDetectorAccuracy")
VK(CIDetectorAccuracyHigh, "CIDetectorAccuracyHigh")
VK(CIDetectorAccuracyLow, "CIDetectorAccuracyLow")
VK(CIDetectorTypeFace, "CIDetectorTypeFace")
VK(CIDetectorTypeText, "CIDetectorTypeText")
VK(GCControllerDidConnectNotification, "GCControllerDidConnectNotification")
VK(GCControllerDidDisconnectNotification, "GCControllerDidDisconnectNotification")
VK(GCHapticsLocalityDefault, "GCHapticsLocalityDefault")
VK(GCHapticsLocalityLeftHandle, "GCHapticsLocalityLeftHandle")
VK(GCHapticsLocalityLeftTrigger, "GCHapticsLocalityLeftTrigger")
VK(GCHapticsLocalityRightHandle, "GCHapticsLocalityRightHandle")
VK(GCHapticsLocalityRightTrigger, "GCHapticsLocalityRightTrigger")
VK(MPMediaItemPropertyAlbumTitle, "MPMediaItemPropertyAlbumTitle")
VK(MPMediaItemPropertyArtist, "MPMediaItemPropertyArtist")
VK(MPMediaItemPropertyArtwork, "MPMediaItemPropertyArtwork")
VK(MPMediaItemPropertyPlaybackDuration, "MPMediaItemPropertyPlaybackDuration")
VK(MPMediaItemPropertyTitle, "MPMediaItemPropertyTitle")
VK(MPNowPlayingInfoPropertyCurrentPlaybackDate, "MPNowPlayingInfoPropertyCurrentPlaybackDate")
VK(MPNowPlayingInfoPropertyElapsedPlaybackTime, "MPNowPlayingInfoPropertyElapsedPlaybackTime")
VK(MPNowPlayingInfoPropertyPlaybackRate, "MPNowPlayingInfoPropertyPlaybackRate")
VK(UNNotificationAttachmentOptionsTypeHintKey, "UNNotificationAttachmentOptionsTypeHintKey")
VK(UNNotificationDefaultActionIdentifier, "com.apple.UNNotificationDefaultActionIdentifier")
VK(UNNotificationDismissActionIdentifier, "com.apple.UNNotificationDismissActionIdentifier")
VK(VNBarcodeSymbologyAztec, "VNBarcodeSymbologyAztec")
VK(VNBarcodeSymbologyCode128, "VNBarcodeSymbologyCode128")
VK(VNBarcodeSymbologyCode39, "VNBarcodeSymbologyCode39")
VK(VNBarcodeSymbologyCode39Checksum, "VNBarcodeSymbologyCode39Checksum")
VK(VNBarcodeSymbologyCode39FullASCII, "VNBarcodeSymbologyCode39FullASCII")
VK(VNBarcodeSymbologyCode39FullASCIIChecksum, "VNBarcodeSymbologyCode39FullASCIIChecksum")
VK(VNBarcodeSymbologyCode93, "VNBarcodeSymbologyCode93")
VK(VNBarcodeSymbologyCode93i, "VNBarcodeSymbologyCode93i")
VK(VNBarcodeSymbologyDataMatrix, "VNBarcodeSymbologyDataMatrix")
VK(VNBarcodeSymbologyEAN13, "VNBarcodeSymbologyEAN13")
VK(VNBarcodeSymbologyEAN8, "VNBarcodeSymbologyEAN8")
VK(VNBarcodeSymbologyI2of5, "VNBarcodeSymbologyI2of5")
VK(VNBarcodeSymbologyI2of5Checksum, "VNBarcodeSymbologyI2of5Checksum")
VK(VNBarcodeSymbologyITF14, "VNBarcodeSymbologyITF14")
VK(VNBarcodeSymbologyPDF417, "VNBarcodeSymbologyPDF417")
VK(VNBarcodeSymbologyQR, "VNBarcodeSymbologyQR")
VK(VNBarcodeSymbologyUPCE, "VNBarcodeSymbologyUPCE")
VK(WKWebsiteDataTypeCookies, "WKWebsiteDataTypeCookies")

/* ---- constantes numéricas e funções desses frameworks */
const float GCHapticDurationInfinite = 3.40282347e+38f;
const float AVSpeechUtteranceDefaultSpeechRate = 0.5f;
const float AVSpeechUtteranceMaximumSpeechRate = 1.0f;
/* HIToolbox: tipo do teclado físico (kKeyboardISO = 1, ABNT2 é ISO) */
u32 KBGetLayoutType(i16 kbdType) { (void)kbdType; return 1; }
/* Bluetooth: sem controlador */
void IOBluetoothPreferenceSetControllerPowerState(int state) { (void)state; }
/* ForceFeedback: nenhum dispositivo com retorno de força (FFERR_NOINTERFACE) */
#define FFERR_NOINTERFACE ((int)0x80004002)
int FFIsForceFeedback(u32 service) { (void)service; return FFERR_NOINTERFACE; }
int FFCreateDevice(u32 service, void **dev) { (void)service; if (dev) *dev = 0; return FFERR_NOINTERFACE; }
int FFReleaseDevice(void *dev) { (void)dev; return 0; }
int FFDeviceGetForceFeedbackCapabilities(void *dev, void *caps) { (void)dev; (void)caps; return FFERR_NOINTERFACE; }
int FFDeviceCreateEffect(void *dev, void *uuid, void *eff, void **out) { (void)dev; (void)uuid; (void)eff; if (out) *out = 0; return FFERR_NOINTERFACE; }
int FFDeviceReleaseEffect(void *dev, void *eff) { (void)dev; (void)eff; return 0; }
int FFEffectStart(void *eff, u32 iter, u32 flags) { (void)eff; (void)iter; (void)flags; return FFERR_NOINTERFACE; }
int FFEffectStop(void *eff) { (void)eff; return FFERR_NOINTERFACE; }
int FFEffectSetParameters(void *eff, void *p, u32 flags) { (void)eff; (void)p; (void)flags; return FFERR_NOINTERFACE; }
