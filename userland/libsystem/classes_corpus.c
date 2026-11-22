/*
 * mOS libSystem — classes ObjC que os apps do corpus referenciam e que nenhum pedaço do mOS define
 * (docs/lacunas/, OBJC_CLASS_$_ de frameworks de sistema sem implementação no mOS). Sem a classe, o símbolo
 * caía no stub de ausente e o app quebrava no carregamento, ao herdar dela ou ao mandar a primeira mensagem.
 *
 * Aqui cada uma existe com a superclasse da Apple (um app pode herdar: NEPacketTunnelProvider, SKScene, SCNView,
 * NSBrowserCell...). O runtime do mOS devolve nil/0 para seletor que a classe não tem (objc.c: missing_imp), então
 * um objeto destes se comporta como o serviço ausente: um navegador de câmeras que não acha câmera, um gerenciador
 * de VPN sem configuração, uma cena do SpriteKit que não desenha. O que tem implementação de verdade continua no
 * arquivo do framework; esta tabela só cobre o que não tinha nada.
 *
 * Registrado por cx_corpus_classes_init, no fim do boot do runtime (objc.c), depois de todas as outras classes:
 * a superclasse é procurada pelo nome nessa hora (NSView, CALayer, NSCell, AVAssetReaderOutput...).
 */
#include "foundation.h"

void cx_add_cmethod(cx_class_t *cls, const char *sel, void *imp);
void cx_mark_stub_class(cx_class_t *c);
id objc_retain(id o);
id objc_autorelease(id o);
extern cx_class_t NSConstantString_c __asm__("__CFConstantStringClassReference");

/* nome, superclasse. A ordem importa: uma classe vem depois da superclasse que está na mesma tabela. */
#define CORPUS_CLASSES(X) \
    /* ImageCaptureCore */ \
    X(ICDevice, "NSObject") X(ICCameraDevice, "ICDevice") X(ICScannerDevice, "ICDevice") X(ICDeviceBrowser, "NSObject") \
    X(ICCameraItem, "NSObject") X(ICCameraFile, "ICCameraItem") X(ICCameraFolder, "ICCameraItem") \
    /* NetworkExtension */ \
    X(NEProvider, "NSObject") X(NETunnelProvider, "NEProvider") X(NEPacketTunnelProvider, "NETunnelProvider") \
    X(NEAppProxyProvider, "NETunnelProvider") X(NETransparentProxyProvider, "NEAppProxyProvider") \
    X(NEFilterProvider, "NEProvider") X(NEFilterDataProvider, "NEFilterProvider") X(NEDNSProxyProvider, "NEProvider") \
    X(NEVPNManager, "NSObject") X(NETunnelProviderManager, "NEVPNManager") X(NEAppProxyProviderManager, "NETunnelProviderManager") \
    X(NETransparentProxyManager, "NEVPNManager") X(NEFilterManager, "NSObject") X(NEDNSProxyManager, "NSObject") \
    X(NEDNSSettingsManager, "NSObject") \
    X(NEVPNProtocol, "NSObject") X(NETunnelProviderProtocol, "NEVPNProtocol") X(NEVPNProtocolIPSec, "NEVPNProtocol") \
    X(NEVPNProtocolIKEv2, "NEVPNProtocolIPSec") \
    X(NEVPNConnection, "NSObject") X(NETunnelProviderSession, "NEVPNConnection") \
    X(NETunnelNetworkSettings, "NSObject") X(NEPacketTunnelNetworkSettings, "NETunnelNetworkSettings") \
    X(NETransparentProxyNetworkSettings, "NETunnelNetworkSettings") \
    X(NEIPv4Settings, "NSObject") X(NEIPv6Settings, "NSObject") X(NEIPv4Route, "NSObject") X(NEIPv6Route, "NSObject") \
    X(NEDNSSettings, "NSObject") X(NEDNSOverHTTPSSettings, "NEDNSSettings") X(NEDNSOverTLSSettings, "NEDNSSettings") \
    X(NEProxySettings, "NSObject") X(NEProxyServer, "NSObject") \
    X(NEOnDemandRule, "NSObject") X(NEOnDemandRuleConnect, "NEOnDemandRule") X(NEOnDemandRuleDisconnect, "NEOnDemandRule") \
    X(NEOnDemandRuleIgnore, "NEOnDemandRule") X(NEOnDemandRuleEvaluateConnection, "NEOnDemandRule") \
    X(NEEvaluateConnectionRule, "NSObject") X(NENetworkRule, "NSObject") \
    X(NEAppProxyFlow, "NSObject") X(NEAppProxyTCPFlow, "NEAppProxyFlow") X(NEAppProxyUDPFlow, "NEAppProxyFlow") \
    X(NEPacketTunnelFlow, "NSObject") X(NEPacket, "NSObject") \
    X(NEFilterSettings, "NSObject") X(NEFilterRule, "NSObject") X(NEFilterProviderConfiguration, "NSObject") \
    X(NEFilterVerdict, "NSObject") X(NEFilterNewFlowVerdict, "NEFilterVerdict") X(NEFilterDataVerdict, "NEFilterVerdict") \
    X(NEFilterFlow, "NSObject") X(NEFilterSocketFlow, "NEFilterFlow") \
    X(NEHotspotNetwork, "NSObject") X(NEHotspotConfiguration, "NSObject") X(NEHotspotConfigurationManager, "NSObject") \
    /* Network (API ObjC antiga) */ \
    X(NWEndpoint, "NSObject") X(NWHostEndpoint, "NWEndpoint") X(NWTCPConnection, "NSObject") X(NWUDPSession, "NSObject") \
    X(NWPath, "NSObject") \
    /* AVFoundation e AVFAudio */ \
    X(AVAudioMix, "NSObject") X(AVMutableAudioMix, "AVAudioMix") X(AVAudioMixInputParameters, "NSObject") \
    X(AVMutableAudioMixInputParameters, "AVAudioMixInputParameters") \
    X(AVAssetReaderAudioMixOutput, "AVAssetReaderOutput") X(AVAssetReaderVideoCompositionOutput, "AVAssetReaderOutput") \
    X(AVPlayerItemLegibleOutput, "AVPlayerItemOutput") X(AVPlayerItemMetadataOutput, "AVPlayerItemOutput") \
    X(AVMetadataGroup, "NSObject") X(AVTimedMetadataGroup, "AVMetadataGroup") X(AVMutableTimedMetadataGroup, "AVTimedMetadataGroup") \
    X(AVPlayerLooper, "NSObject") X(AVAssetWriterInputMetadataAdaptor, "NSObject") \
    X(AVMediaSelectionGroup, "NSObject") X(AVMediaSelectionOption, "NSObject") X(AVMediaSelection, "NSObject") \
    X(AVCaptureAudioChannel, "NSObject") X(AVVideoCompositionCoreAnimationTool, "NSObject") X(AVPlayerItemTrack, "NSObject") \
    X(AVOutputSettingsAssistant, "NSObject") X(AVMetadataObject, "NSObject") X(AVMetadataFaceObject, "AVMetadataObject") \
    X(AVMetadataMachineReadableCodeObject, "AVMetadataObject") X(AVCaptureMultiCamSession, "AVCaptureSession") \
    X(AVCaptureMetadataOutput, "AVCaptureOutput") X(AVCaptureInputPort, "NSObject") \
    X(AVAudioRecorder, "NSObject") X(AVAudioChannelLayout, "NSObject") X(AVAudioConnectionPoint, "NSObject") \
    X(AVAudioCompressedBuffer, "AVAudioBuffer") X(AVAssetTrackSegment, "NSObject") X(AVAssetSegmentTrackReport, "NSObject") \
    X(AVAssetResourceLoadingDataRequest, "NSObject") X(AVAggregateAssetDownloadTask, "NSURLSessionTask") \
    X(AVPictureInPictureControllerContentSource, "NSObject") X(AVPictureInPictureVideoCallViewController, "NSViewController") \
    /* SpriteKit, SceneKit, GameplayKit, ModelIO */ \
    X(SKNode, "NSResponder") X(SKEffectNode, "SKNode") X(SKScene, "SKEffectNode") X(SKSpriteNode, "SKNode") \
    X(SKLabelNode, "SKNode") X(SKShapeNode, "SKNode") X(SKEmitterNode, "SKNode") X(SKView, "NSView") \
    X(SKTexture, "NSObject") X(SKAction, "NSObject") X(SKKeyframeSequence, "NSObject") X(SKPhysicsBody, "NSObject") \
    X(SCNView, "NSView") X(SCNScene, "NSObject") X(SCNNode, "NSObject") X(SCNGeometry, "NSObject") X(SCNPlane, "SCNGeometry") \
    X(SCNFloor, "SCNGeometry") X(SCNMaterial, "NSObject") X(SCNLight, "NSObject") X(SCNCamera, "NSObject") \
    X(SCNAction, "NSObject") X(SCNTransaction, "NSObject") X(SCNParticleSystem, "NSObject") \
    X(SCNParticlePropertyController, "NSObject") \
    X(GKNoiseSource, "NSObject") X(GKCoherentNoiseSource, "GKNoiseSource") X(GKPerlinNoiseSource, "GKCoherentNoiseSource") \
    X(GKNoise, "NSObject") X(GKNoiseMap, "NSObject") \
    X(MDLObject, "NSObject") X(MDLMesh, "MDLObject") X(MDLAsset, "NSObject") \
    /* Metal Performance Shaders, MetalFX, Metal */ \
    X(MPSImageConvolution, "MPSUnaryImageKernel") \
    X(MPSImageBox, "MPSUnaryImageKernel") X(MPSImageAreaMax, "MPSUnaryImageKernel") X(MPSImageAreaMin, "MPSImageAreaMax") \
    X(MPSImageGaussianBlur, "MPSUnaryImageKernel") X(MPSImage, "NSObject") X(MPSTemporaryImage, "MPSImage") \
    X(MPSImageDescriptor, "NSObject") \
    X(MTLFXSpatialScalerDescriptor, "NSObject") X(MTLFXTemporalScalerDescriptor, "NSObject") \
    X(MTLMeshRenderPipelineDescriptor, "NSObject") X(MTLAccelerationStructureAllocationDescriptor, "NSObject") \
    /* CoreML, Vision, NaturalLanguage */ \
    X(MLFeatureDescription, "NSObject") X(MLArrayBatchProvider, "NSObject") X(MLModelAsset, "NSObject") \
    \
    X(VNTextObservation, "VNRectangleObservation") \
    X(VNDetectTextRectanglesRequest, "VNImageBasedRequest") X(VNGeneratePersonSegmentationRequest, "VNImageBasedRequest") \
    X(VNDocumentCameraViewController, "NSViewController") \
    /* AppKit */ \
    X(NSBrowserCell, "NSCell") X(NSAccessibilityCustomRotor, "NSObject") X(NSAccessibilityCustomRotorItemResult, "NSObject") \
    X(NSAccessibilityCustomRotorSearchParameters, "NSObject") X(NSMediaLibraryBrowserController, "NSObject") \
    X(NSScrubberSelectionView, "NSView") X(NSOpenGLLayer, "CAOpenGLLayer") X(NSDictionaryController, "NSArrayController") \
    X(NSComboButton, "NSControl") X(NSTextAlternatives, "NSObject") X(NSTextPlaceholder, "NSObject") \
    X(NSKeyboardShortcut, "NSObject") \
    /* Foundation (o resto do script/AppleScript e extensões) */ \
    X(NSScriptObjectSpecifier, "NSObject") X(NSIndexSpecifier, "NSScriptObjectSpecifier") \
    X(NSNameSpecifier, "NSScriptObjectSpecifier") X(NSPropertySpecifier, "NSScriptObjectSpecifier") \
    X(NSRangeSpecifier, "NSScriptObjectSpecifier") X(NSPositionalSpecifier, "NSObject") \
    X(NSCreateCommand, "NSScriptCommand") X(NSDeleteCommand, "NSScriptCommand") X(NSCloneCommand, "NSScriptCommand") \
    X(NSMoveCommand, "NSScriptCommand") X(NSScriptExecutionContext, "NSObject") X(NSScriptCoercionHandler, "NSObject") \
    X(NSClassDescription, "NSObject") \
    X(NSUserScriptTask, "NSObject") X(NSUserUnixTask, "NSUserScriptTask") X(NSFileAccessIntent, "NSObject") \
    X(NSFileVersion, "NSObject") X(NSExtensionContext, "NSObject") X(_NSExtensionContextVendor, "NSObject") \
    X(NSExtension, "NSObject") X(NSMassFormatter, "NSFormatter") X(NSLengthFormatter, "NSFormatter") \
    X(NSCalendarDate, "NSDate") X(NSOrthography, "NSObject") X(NSPresentationIntent, "NSObject") X(NSURLError, "NSError") \
    X(NSBlockInvocation, "NSInvocation") X(NSExceptionHandler, "NSObject") \
    /* QuartzCore, CoreGraphics, Quartz */ \
    X(CABackdropLayer, "CALayer") X(CAFilter, "NSObject") X(CAPresentationModifier, "NSObject") \
    X(CAPresentationModifierGroup, "CAPresentationModifier") \
    X(CGVirtualDisplayDescriptor, "NSObject") X(CGVirtualDisplaySettings, "NSObject") X(CGVirtualDisplayMode, "NSObject") \
    X(CGVirtualDisplay, "NSObject") \
    X(PDFAnnotationTextWidget, "PDFAnnotation") X(PDFAnnotationChoiceWidget, "PDFAnnotation") \
    X(PDFAnnotationButtonWidget, "PDFAnnotation") X(IKSlideshow, "NSObject") X(IKPictureTaker, "NSPanel") \
    /* MapKit */ \
    X(MKShape, "NSObject") X(MKMultiPoint, "MKShape") X(MKPolyline, "MKMultiPoint") X(MKGeodesicPolyline, "MKPolyline") \
    X(MKPolygon, "MKMultiPoint") X(MKOverlayPathRenderer, "MKOverlayRenderer") \
    X(MKPolylineRenderer, "MKOverlayPathRenderer") X(MKPolygonRenderer, "MKOverlayPathRenderer") \
    X(MKMapConfiguration, "NSObject") X(MKStandardMapConfiguration, "MKMapConfiguration") X(MKMapCameraZoomRange, "NSObject") \
    /* FileProvider, FSKit */ \
    X(NSFileProviderManager, "NSObject") X(NSFileProviderDomain, "NSObject") X(NSFileProviderItemVersion, "NSObject") \
    X(NSFileProviderDomainVersion, "NSObject") \
    X(FSClient, "NSObject") X(FSFileSystemBase, "NSObject") X(FSUnaryFileSystem, "FSFileSystemBase") X(FSVolume, "NSObject") \
    X(FSItem, "NSObject") X(FSItemAttributes, "NSObject") X(FSFileName, "NSObject") X(FSStatFSResult, "NSObject") \
    X(FSProbeResult, "NSObject") X(FSContainerStatus, "NSObject") X(FSContainerIdentifier, "NSObject") \
    X(FSVolumeIdentifier, "NSObject") X(FSVolumeSupportedCapabilities, "NSObject") \
    /* hardware e serviços sem equivalente no mOS */ \
    X(IOUSBHostObject, "NSObject") X(IOUSBHostDevice, "IOUSBHostObject") X(IOUSBHostInterface, "IOUSBHostObject") \
    X(IOUSBHostControllerInterface, "NSObject") X(IOUSBHostCIEndpointStateMachine, "NSObject") \
    X(IOUSBHostCIDeviceStateMachine, "NSObject") X(IOBluetoothDevicePair, "NSObject") \
    X(PGDeviceDescriptor, "NSObject") X(PGDisplayDescriptor, "NSObject") X(PGDisplayMode, "NSObject") \
    X(TKSmartCardSlotManager, "NSObject") X(TKSmartCard, "NSObject") X(CWNetwork, "NSObject") X(CWNetworkProfile, "NSObject") \
    X(CMMotionManager, "NSObject") X(CMAltimeter, "NSObject") X(CBBlueLightClient, "NSObject") X(CBTrueToneClient, "NSObject") \
    X(AUGenericView, "NSView") X(AUPannerView, "NSView") X(APSConnection, "NSObject") X(APSURLTokenInfo, "NSObject") \
    X(INStartCallIntent, "INIntent") X(INSearchForMessagesIntent, "INIntent") \
    X(SecKeyProxy, "NSObject") X(SFAuthorization, "NSObject") X(RPSystemBroadcastPickerView, "NSView") \
    X(QTMovieModernizer, "NSObject") X(PIPViewController, "NSViewController") X(OSLaunchdJob, "NSObject") \
    X(OSDManager, "NSObject") X(ITLibrary, "NSObject") X(HIDEvent, "NSObject") X(ASIdentifierManager, "NSObject") \
    X(ASAuthorizationPlatformPublicKeyCredentialAssertionRequest, "ASAuthorizationRequest") \
    X(AKAuthorizationController, "NSObject") \
    X(LSBundleProxy, "NSObject") X(LSDatabaseContext, "NSObject") X(LSAppLink, "NSObject") X(_LSOpenConfiguration, "NSObject") \
    X(_NSHSTSStorage, "NSObject") X(_NSHTTPAlternativeServicesStorage, "NSObject") X(_NSHTTPAlternativeServicesFilter, "NSObject")

#define DECL(N, S) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
CORPUS_CLASSES(DECL)
typedef struct { cx_class_t *c, *m; const char *name, *super; } corpus_class_t;
#define ROW(N, S) {&N##_c, &N##_m, #N, S},
static const corpus_class_t tab[] = {CORPUS_CLASSES(ROW)};

/* campos para os poucos estados guardados e espaço para quem herda sem ivars próprias */
#define EXTRA 64
typedef struct { cx_class_t *isa; id slot[7]; } ccobj_t;

/* ---- comportamento mínimo onde nil quebraria o app */
static id shared_of(id cls, SEL c) {
    (void)c;
    static struct { cx_class_t *k; id o; } s[16];
    for (int i = 0; i < 16; i++) {
        if (s[i].k == (cx_class_t *)cls) return s[i].o;
        if (!s[i].k) { s[i].k = (cx_class_t *)cls; s[i].o = objc_retain(cx_alloc_instance((cx_class_t *)cls)); return s[i].o; }
    }
    return 0;
}
/* +loadAllFromPreferencesWithCompletionHandler: e +sharedManager.load...: o bloco recebe (lista vazia, nil) */
typedef struct { void *isa; int flags, res; void (*invoke)(void *, ...); } blk_t;
static void load_all(id cls, SEL c, blk_t *b) {
    (void)cls; (void)c;
    if (b && b->invoke) b->invoke(b, MSG0(id, (id)cx_class_named("NSArray"), "array"), (id)0);
}
static void load_one(id s, SEL c, blk_t *b) { (void)s; (void)c; if (b && b->invoke) b->invoke(b, (id)0); }
static i64 vpn_status(id s, SEL c) { (void)s; (void)c; return 0; }                  /* NEVPNStatusInvalid */
static id ns_slot0(id s, SEL c) { (void)c; return ((ccobj_t *)s)->slot[0]; }
static void ns_set_slot0(id s, SEL c, id v) { (void)c; ((ccobj_t *)s)->slot[0] = v; } /* delegate: não retém */
static id empty_arr(id s, SEL c) { (void)s; (void)c; return MSG0(id, (id)cx_class_named("NSArray"), "array"); }
static BOOL no_m(id s, SEL c) { (void)s; (void)c; return NO; }

#define KNV(sym, lit) static nsstr_t sym##_o = { &NSConstantString_c, 0, (char *)lit, sizeof(lit) - 1 }; id sym = (id)&sym##_o;
/* ImageCaptureCore e NetworkExtension */
KNV(ICTransportTypeMassStorage, "ICTransportTypeMassStorage") KNV(ICTransportTypeUSB, "ICTransportTypeUSB")
KNV(ICTransportTypeExFAT, "ICTransportTypeExFAT") KNV(ICTransportTypeTCPIP, "ICTransportTypeTCPIP")
KNV(ICSavedFilename, "savedFilename") KNV(ICSaveAsFilename, "saveAsFilename") KNV(ICOverwrite, "overwrite")
KNV(ICDownloadsDirectoryURL, "downloadsDirectoryURL") KNV(ICDeleteAfterSuccessfulDownload, "deleteAfterDownload")
KNV(ICDownloadSidecarFiles, "downloadSidecarFiles")
KNV(NEVPNStatusDidChangeNotification, "NEVPNStatusDidChangeNotification")
KNV(NEVPNConfigurationChangeNotification, "NEVPNConfigurationChangeNotification")
KNV(NEVPNErrorDomain, "NEVPNErrorDomain") KNV(NETunnelProviderErrorDomain, "NETunnelProviderErrorDomain")
KNV(NEFilterErrorDomain, "NEFilterErrorDomain") KNV(NEAppProxyErrorDomain, "NEAppProxyErrorDomain")
KNV(NEVPNConnectionStartOptionUsername, "NEVPNConnectionStartOptionUsername")
KNV(NEVPNConnectionStartOptionPassword, "NEVPNConnectionStartOptionPassword")
/* AppKit */
KNV(NSFontDescriptorSystemDesignDefault, "NSCTFontUIFontDesignDefault")
KNV(NSFontDescriptorSystemDesignMonospaced, "NSCTFontUIFontDesignMonospaced")
KNV(NSSpeechModeNormal, "NORM") KNV(NSSpeechModeLiteral, "LTRL") KNV(NSSpeechModePhoneme, "PHON") KNV(NSSpeechModeText, "TEXT")
KNV(NSAccessibilityAutocorrectedTextAttribute, "AXAutocorrectedText")
KNV(NSAccessibilityAnnotationLocation, "AXAnnotationLocation")
KNV(NSAccessibilityAnnotationLabel, "AXAnnotationLabel")
KNV(NSAccessibilityAnnotationElement, "AXAnnotationElement")

void cx_corpus_classes_init(void) {
    cx_class_t *ob = cx_class_named("NSObject");
    static int done;
    if (done++) return;
    for (size_t i = 0; i < sizeof tab / sizeof *tab; i++) {
        cx_class_t *s = cx_class_named(tab[i].super);
        cx_define_class(tab[i].c, tab[i].m, tab[i].name, s ? s : ob, EXTRA);
        cx_mark_stub_class(tab[i].c);      /* uma classe de verdade com o mesmo nome (framework carregado depois) ganha */
    }
    /* singletons e carregamento de preferências que o app espera receber */
    cx_add_cmethod(&NEVPNManager_c, "sharedManager", (void *)shared_of);
    cx_add_cmethod(&NEFilterManager_c, "sharedManager", (void *)shared_of);
    cx_add_cmethod(&NEDNSProxyManager_c, "sharedManager", (void *)shared_of);
    cx_add_cmethod(&NEDNSSettingsManager_c, "sharedManager", (void *)shared_of);
    cx_add_cmethod(&NEHotspotConfigurationManager_c, "sharedManager", (void *)shared_of);
    cx_add_cmethod(&NETunnelProviderManager_c, "loadAllFromPreferencesWithCompletionHandler:", (void *)load_all);
    cx_add_cmethod(&NETransparentProxyManager_c, "loadAllFromPreferencesWithCompletionHandler:", (void *)load_all);
    cx_add_method(&NEVPNManager_c, "loadFromPreferencesWithCompletionHandler:", (void *)load_one);
    cx_add_method(&NEVPNManager_c, "saveToPreferencesWithCompletionHandler:", (void *)load_one);
    cx_add_method(&NEVPNManager_c, "removeFromPreferencesWithCompletionHandler:", (void *)load_one);
    cx_add_method(&NEFilterManager_c, "loadFromPreferencesWithCompletionHandler:", (void *)load_one);
    cx_add_method(&NEFilterManager_c, "saveToPreferencesWithCompletionHandler:", (void *)load_one);
    cx_add_method(&NEVPNConnection_c, "status", (void *)vpn_status);
    cx_add_method(&ICDeviceBrowser_c, "delegate", (void *)ns_slot0);
    cx_add_method(&ICDeviceBrowser_c, "setDelegate:", (void *)ns_set_slot0);
    cx_add_method(&ICDeviceBrowser_c, "devices", (void *)empty_arr);
    cx_add_method(&ICDeviceBrowser_c, "isBrowsing", (void *)no_m);
    cx_add_method(&ICDeviceBrowser_c, "isSuspended", (void *)no_m);
    cx_add_method(&AVMutableAudioMix_c, "inputParameters", (void *)empty_arr);
    cx_add_method(&AVAudioMix_c, "inputParameters", (void *)empty_arr);
    cx_add_method(&MLArrayBatchProvider_c, "count", (void *)vpn_status);
}
