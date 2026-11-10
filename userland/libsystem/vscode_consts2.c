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
/* Tipos novos (macOS 14+), importados como weak pelo Chromium atrás de @available: o mOS diz ser macOS novo, então
 * o media/device_monitors/device_monitor_mac.mm os lê direto; sem definição o GOT fica 0 e o utilitário
 * video_capture caía em ponteiro nulo assim que subia. */
VK(AVCaptureDeviceTypeContinuityCamera, "AVCaptureDeviceTypeContinuityCamera")
VK(AVCaptureDeviceTypeExternal, "AVCaptureDeviceTypeExternal")
VK(AVCaptureDeviceTypeMicrophone, "AVCaptureDeviceTypeMicrophone")
VK(AVCaptureDeviceTypeBuiltInMicrophone, "AVCaptureDeviceTypeBuiltInMicrophone")
VK(AVFileTypeMPEG4, "public.mpeg-4")
VK(AVFileTypeQuickTimeMovie, "com.apple.quicktime-movie")
VK(AVVideoCodecTypeH264, "avc1")
VK(AVVideoCodecTypeHEVC, "hvc1")
VK(AVVideoScalingModeResizeAspect, "AVVideoScalingModeResizeAspect")
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
VK(CIDetectorAccuracy, "CIDetectorAccuracy")
VK(CIDetectorAccuracyHigh, "CIDetectorAccuracyHigh")
VK(CIDetectorAccuracyLow, "CIDetectorAccuracyLow")
VK(CIDetectorTypeFace, "CIDetectorTypeFace")
VK(CIDetectorTypeText, "CIDetectorTypeText")
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
VK(WKWebsiteDataTypeCookies, "WKWebsiteDataTypeCookies")

/* ---- constantes numéricas e funções desses frameworks */
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
