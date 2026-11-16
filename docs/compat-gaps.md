# Lacunas de compatibilidade no corpus macOS

Gerado por `python3 tools/corpus/harness.py gaps` com as medidas atuais de `tools/corpus/varrer-tudo.sh`.
libSystem medido: main ce6dc05c (merge #392), copiado de build do Ricardo 2026-11-16_01:41.
O disco é o da varredura: libunwind, libc++, libswift*, libz, libcurl e, com o artefato do CI, os frameworks do
mOS. Os baldes libc++ e Swift só contam o que nenhuma dessas exporta.
Binários não estão no git. URLs e sha256 ficam em `tools/corpus/manifest.json`.

Apps com pelo menos uma imagem Mach-O x86_64: **250**.

## Símbolos que faltam em mais apps

| símbolo | apps |
|---|---:|
| `_$sSS10FoundationE19_bridgeToObjectiveCSo8NSStringCyF` | 94 |
| `_$sSS10FoundationE36_unconditionallyBridgeFromObjectiveCySSSo8NSStringCSgFZ` | 93 |
| `_$s10Foundation3URLVMa` | 90 |
| `_$s10Foundation3URLVMn` | 90 |
| `_$sSa10FoundationE19_bridgeToObjectiveCSo7NSArrayCyF` | 90 |
| `_$sSa10FoundationE36_unconditionallyBridgeFromObjectiveCySayxGSo7NSArrayCSgFZ` | 89 |
| `_$s10Foundation22_convertNSErrorToErrorys0E0_pSo0C0CSgF` | 87 |
| `_$s10Foundation3URLV36_unconditionallyBridgeFromObjectiveCyACSo5NSURLCSgFZ` | 87 |
| `_$sSD10FoundationE19_bridgeToObjectiveCSo12NSDictionaryCyF` | 87 |
| `_$s10Foundation3URLV19_bridgeToObjectiveCSo5NSURLCyF` | 86 |
| `_$s10Foundation4DataV19_bridgeToObjectiveCSo6NSDataCyF` | 86 |
| `_$sSD10FoundationE36_unconditionallyBridgeFromObjectiveCySDyxq_GSo12NSDictionaryCSgFZ` | 86 |
| `_OBJC_CLASS_$_WebView` | 86 |
| `_$s10Foundation4DataV36_unconditionallyBridgeFromObjectiveCyACSo6NSDataCSgFZ` | 85 |
| `_OBJC_CLASS_$_WKUserScript` | 85 |
| `_$s10Foundation3URLV6stringACSgSSh_tcfC` | 84 |
| `_$s10Foundation12NotificationVMa` | 83 |
| `_$ss5ErrorP10FoundationE20localizedDescriptionSSvg` | 83 |
| `_$s10Foundation12NotificationV36_unconditionallyBridgeFromObjectiveCyACSo14NSNotificationCSgFZ` | 82 |
| `_$s10Foundation4DateVMa` | 81 |
| `_$sSS10FoundationE26_forceBridgeFromObjectiveC_6resultySo8NSStringC_SSSgztFZ` | 81 |
| `_$sSS10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo8NSStringC_SSSgztFZ` | 81 |
| `_$s10Foundation3URLV4pathSSvg` | 80 |
| `_$sSS10FoundationE8EncodingV4utf8ACvgZ` | 80 |
| `_$sSS10FoundationE8EncodingVMa` | 80 |
| `_$s10Foundation4DateVMn` | 78 |
| `_OBJC_CLASS_$_WKUserContentController` | 78 |
| `_$sSy10FoundationE20replacingOccurrences2of4with7options5rangeSSqd___qd_0_So22NSStringCompareOptionsVSnySS5IndexVGSgtSyRd__SyRd_0_r0_lF` | 77 |
| `_$s10Foundation4UUIDVMa` | 76 |
| `_$s10Foundation6LocaleVMa` | 76 |
| `_$sSS10FoundationE4data8encodingSSSgAA4DataVh_SSAAE8EncodingVtcfC` | 76 |
| `_$s10Foundation13__DataStorageC6_bytesSvSgvg` | 75 |
| `_$s10Foundation13__DataStorageC7_offsetSivg` | 75 |
| `_$s10Foundation3URLV22appendingPathComponentyACSSF` | 75 |
| `_$s10Foundation4DataVN` | 75 |
| `_OBJC_CLASS_$_WebPreferences` | 75 |
| `_$s10Foundation12CharacterSetVMa` | 74 |
| `_$s10Foundation3URLV15fileURLWithPathACSSh_tcfC` | 74 |
| `_$s10Foundation4DateV19_bridgeToObjectiveCSo6NSDateCyF` | 74 |
| `_$s10Foundation13__DataStorageC7_lengthSivg` | 73 |

## Por origem

### sistema

| símbolo | apps |
|---|---:|
| `_OBJC_CLASS_$_WebView` | 86 |
| `_OBJC_CLASS_$_WKUserScript` | 85 |
| `_OBJC_CLASS_$_WKUserContentController` | 78 |
| `_OBJC_CLASS_$_WebPreferences` | 75 |
| `_OBJC_CLASS_$_NSAppleScript` | 61 |
| `_$s10Foundation3URLV36_unconditionallyBridgeFromObjectiveCyACSo5NSURLCSgFZ` | 53 |
| `_$s10Foundation3URLVMn` | 53 |
| `_$sSS10FoundationE19_bridgeToObjectiveCSo8NSStringCyF` | 53 |
| `_$sSS10FoundationE36_unconditionallyBridgeFromObjectiveCySSSo8NSStringCSgFZ` | 53 |
| `_CFBundleGetBundleWithIdentifier` | 53 |
| `_$s10Foundation3URLVMa` | 52 |
| `_$sSa10FoundationE19_bridgeToObjectiveCSo7NSArrayCyF` | 52 |
| `_$ss5ErrorP10FoundationE20localizedDescriptionSSvg` | 52 |
| `_$s10Foundation22_convertNSErrorToErrorys0E0_pSo0C0CSgF` | 51 |
| `_$s10Foundation4DateVMa` | 51 |

### libc++

| símbolo | apps |
|---|---:|
| `__Z10add_friendP9tree_nodeS0_b` | 1 |
| `__Z10build_declj9tree_codeP9tree_nodeS1_` | 1 |
| `__Z10get_typeidP9tree_nodei` | 1 |
| `__Z10start_enumP9tree_nodeS0_S0_S0_bPb` | 1 |
| `__Z11begin_scope10scope_kindP9tree_node` | 1 |
| `__Z11build_throwjP9tree_nodei` | 1 |
| `__Z11clone_cdtorP9tree_nodeb` | 1 |
| `__Z11fancy_abortPKciS0_` | 1 |
| `__Z11fatal_errorjPKcz` | 1 |
| `__Z11finish_enumP9tree_node` | 1 |
| `__Z11ggc_reallocPvm` | 1 |
| `__Z11layout_typeP9tree_node` | 1 |
| `__Z11leave_scopev` | 1 |
| `__Z11linemap_addP9line_maps9lc_reasonjPKcj` | 1 |
| `__Z11lookup_nameP9tree_node` | 1 |

### Swift

| símbolo | apps |
|---|---:|
| `_$sSo13os_log_type_ta0A0E5errorABvgZ` | 59 |
| `_$s2os0A4_log_3dso0B0__ySo0a1_B7_type_ta_SVSo03OS_a1_B0Cs12StaticStringVs7CVarArg_pdtF` | 57 |
| `_$s22UniformTypeIdentifiers6UTTypeVMa` | 55 |
| `_$sSo13os_log_type_ta0A0E5faultABvgZ` | 55 |
| `_$s22UniformTypeIdentifiers6UTTypeVMn` | 51 |
| `_$s2os6LoggerV9logObjectSo03OS_a1_C0Cvg` | 50 |
| `_$s2os6LoggerV9subsystem8categoryACSS_SStcfC` | 50 |
| `_$s2os6LoggerVMa` | 50 |
| `_$sSo13os_log_type_ta0A0E4infoABvgZ` | 48 |
| `_$sSo13os_log_type_ta0A0E5debugABvgZ` | 42 |
| `_$sSS10FoundationE19_bridgeToObjectiveCSo8NSStringCyF` | 41 |
| `_$sSS10FoundationE36_unconditionallyBridgeFromObjectiveCySSSo8NSStringCSgFZ` | 40 |
| `_$s10Foundation3URLVMa` | 38 |
| `_$sSa10FoundationE19_bridgeToObjectiveCSo7NSArrayCyF` | 38 |
| `_$sSa10FoundationE36_unconditionallyBridgeFromObjectiveCySayxGSo7NSArrayCSgFZ` | 38 |

### unwind

| símbolo | apps |
|---|---:|
| — | 0 |

## Por framework

### SwiftUI.framework

| símbolo | apps |
|---|---:|
| `_$s7SwiftUI17VerticalAlignmentV3topACvgZ` | 49 |
| `_$s7SwiftUI6IDViewVMn` | 48 |
| `_$s7SwiftUI4ViewPAAE11toggleStyleyQrqd__AA06ToggleE0Rd__lF` | 45 |
| `_$s7SwiftUI4ViewPAAE11toggleStyleyQrqd__AA06ToggleE0Rd__lFQOMQ` | 45 |
| `_$s7SwiftUI26NSViewRepresentableContextV11coordinator11CoordinatorQzvg` | 43 |
| `_$s7SwiftUI26NSViewRepresentableContextVMn` | 42 |
| `_$s7SwiftUI21PinnedScrollableViewsVMa` | 40 |
| `_$s7SwiftUI21PinnedScrollableViewsVs9OptionSetAAMc` | 40 |

### Foundation.framework

| símbolo | apps |
|---|---:|
| `_OBJC_CLASS_$_NSAppleScript` | 61 |
| `_$s10Foundation3URLV36_unconditionallyBridgeFromObjectiveCyACSo5NSURLCSgFZ` | 53 |
| `_$s10Foundation3URLVMn` | 53 |
| `_$sSS10FoundationE19_bridgeToObjectiveCSo8NSStringCyF` | 53 |
| `_$sSS10FoundationE36_unconditionallyBridgeFromObjectiveCySSSo8NSStringCSgFZ` | 53 |
| `_$s10Foundation3URLVMa` | 52 |
| `_$sSa10FoundationE19_bridgeToObjectiveCSo7NSArrayCyF` | 52 |
| `_$ss5ErrorP10FoundationE20localizedDescriptionSSvg` | 52 |

### libswiftFoundation.dylib

| símbolo | apps |
|---|---:|
| `_$sSS10FoundationE19_bridgeToObjectiveCSo8NSStringCyF` | 41 |
| `_$sSS10FoundationE36_unconditionallyBridgeFromObjectiveCySSSo8NSStringCSgFZ` | 40 |
| `_$s10Foundation3URLVMa` | 38 |
| `_$sSa10FoundationE19_bridgeToObjectiveCSo7NSArrayCyF` | 38 |
| `_$sSa10FoundationE36_unconditionallyBridgeFromObjectiveCySayxGSo7NSArrayCSgFZ` | 38 |
| `_$s10Foundation3URLV19_bridgeToObjectiveCSo5NSURLCyF` | 37 |
| `_$s10Foundation3URLVMn` | 37 |
| `_$s10Foundation22_convertNSErrorToErrorys0E0_pSo0C0CSgF` | 36 |

### Combine.framework

| símbolo | apps |
|---|---:|
| `_$s7Combine25ObservableObjectPublisherC4sendyyF` | 35 |
| `_$s7Combine10PublishersO3MapVMn` | 32 |
| `_$s7Combine10PublishersO3MapVy_xq_GAA9PublisherAAMc` | 32 |
| `_$s7Combine9PublisherPAAE3mapyAA10PublishersO3MapVy_xqd__Gqd__6OutputQzclF` | 32 |
| `_$s7Combine11CancellableMp` | 30 |
| `_$s6Output7Combine9PublisherPTl` | 29 |
| `_$s7Combine9PublisherMp` | 29 |
| `_$s7Combine06CustomA21IdentifierConvertibleMp` | 28 |

### CoreFoundation.framework

| símbolo | apps |
|---|---:|
| `_CFBundleGetBundleWithIdentifier` | 52 |
| `_CFStringGetSystemEncoding` | 40 |
| `_kCFLocaleLanguageCode` | 37 |
| `_CFSocketCreateWithNative` | 34 |
| `_CFURLEnumeratorGetNextURL` | 30 |
| `_kCFURLIsPackageKey` | 30 |
| `_CFShow` | 28 |
| `_CFSocketGetSocketFlags` | 28 |

### AppIntents.framework

| símbolo | apps |
|---|---:|
| `_$s10AppIntents09_SupportsA12DependenciesMp` | 17 |
| `_$s10AppIntents0A6IntentMp` | 17 |
| `_$s10AppIntents0A6IntentP04openA7WhenRunSbvgZTq` | 17 |
| `_$s10AppIntents0A6IntentP11descriptionAA0C11DescriptionVSgvgZTq` | 17 |
| `_$s10AppIntents0A6IntentP13PerformResultAC_AA0cE0Tn` | 17 |
| `_$s10AppIntents0A6IntentP14SummaryContentAC_AA09ParameterD0Tn` | 17 |
| `_$s10AppIntents0A6IntentP16parameterSummary0E7ContentQzvgZTq` | 17 |
| `_$s10AppIntents0A6IntentP20authenticationPolicyAA0c14AuthenticationE0OvgZTq` | 17 |

### AppKit.framework

| símbolo | apps |
|---|---:|
| `_OBJC_CLASS_$_NSFontPanel` | 39 |
| `_NSFileContentsPboardType` | 30 |
| `_OBJC_CLASS_$_NSPageLayout` | 30 |
| `_NSAccessibilityUnignoredChildrenForOnlyChild` | 29 |
| `_OBJC_CLASS_$_NSPrintOperation` | 27 |
| `_NSApplicationLoad` | 22 |
| `_OBJC_CLASS_$_NSStepperCell` | 20 |
| `_OBJC_CLASS_$_NSViewAnimation` | 20 |

### CryptoKit.framework

| símbolo | apps |
|---|---:|
| `_$s9CryptoKit6SHA256VAA12HashFunctionAAMc` | 42 |
| `_$s9CryptoKit6SHA256VMa` | 42 |
| `_$s9CryptoKit12HashFunctionP6update13bufferPointerySW_tFTj` | 41 |
| `_$s9CryptoKit12SHA256DigestVMa` | 40 |
| `_$s9CryptoKit12HashFunctionP8finalize6DigestQzyFTj` | 39 |
| `_$s9CryptoKit12HashFunctionPxycfCTj` | 38 |
| `_$s9CryptoKit12SHA256DigestVSTAAMc` | 35 |
| `_$s9CryptoKit12SymmetricKeyVMa` | 25 |

### libSystem.B.dylib

| símbolo | apps |
|---|---:|
| `_putenv` | 37 |
| `_getloadavg` | 29 |
| `_getprotobyname` | 25 |
| `_pthread_attr_getschedpolicy` | 23 |
| `__os_activity_create` | 22 |
| `_os_activity_scope_enter` | 22 |
| `_os_activity_scope_leave` | 22 |
| `_OSAtomicAdd32Barrier` | 21 |

### Security.framework

| símbolo | apps |
|---|---:|
| `_kSecAttrServer` | 35 |
| `_kSecAttrProtocol` | 28 |
| `_SSLSetDiffieHellmanParams` | 22 |
| `_kSecAttrProtocolHTTPProxy` | 21 |
| `_kSecAttrProtocolHTTPSProxy` | 21 |
| `_kSecAttrProtocolFTPProxy` | 20 |
| `_kSecImportExportKeychain` | 20 |
| `_SecIdentitySearchCopyNext` | 19 |

### CoreServices.framework

| símbolo | apps |
|---|---:|
| `_kUTTypeFileURL` | 33 |
| `_GetIconRef` | 31 |
| `_FSPathMakeRefWithOptions` | 28 |
| `_ReleaseIconRef` | 27 |
| `_kUTTagClassNSPboardType` | 27 |
| `_UCCreateCollator` | 26 |
| `_FSGetCatalogInfo` | 23 |
| `_UCGetCollationKey` | 23 |

### libncurses.5.4.dylib

| símbolo | apps |
|---|---:|
| `_setupterm` | 9 |
| `_tigetnum` | 9 |
| `_tigetflag` | 8 |
| `_tigetstr` | 8 |
| `_COLORS` | 7 |
| `_COLOR_PAIRS` | 7 |
| `_COLS` | 7 |
| `_LINES` | 7 |

### CoreGraphics.framework

| símbolo | apps |
|---|---:|
| `_kCGColorWhite` | 32 |
| `_CGDisplayCopyAllDisplayModes` | 28 |
| `_CGEventTapCreateForPid` | 26 |
| `_CGEventSourceButtonState` | 24 |
| `_CGDisplaySetDisplayMode` | 23 |
| `_CGDisplayCapture` | 21 |
| `_CGDisplayModeCopyPixelEncoding` | 20 |
| `_CGEventSourceCounterForEventType` | 20 |

### ApplicationServices.framework

| símbolo | apps |
|---|---:|
| `_PasteboardCreate` | 34 |
| `_PasteboardSynchronize` | 34 |
| `_PasteboardCopyItemFlavorData` | 23 |
| `_PasteboardCopyItemFlavors` | 23 |
| `_PasteboardGetItemCount` | 23 |
| `_PasteboardGetItemIdentifier` | 23 |
| `_PMGetDuplex` | 22 |
| `_PasteboardClear` | 22 |

### self)

| símbolo | apps |
|---|---:|
| `_PyDict_New` | 4 |
| `_PyDict_SetItem` | 4 |
| `_PyDict_SetItemString` | 4 |
| `_PyErr_Clear` | 4 |
| `_PyErr_Format` | 4 |
| `_PyErr_Occurred` | 4 |
| `_PyErr_SetString` | 4 |
| `_PyExc_AttributeError` | 4 |

### libswiftUniformTypeIdentifiers.dylib

| símbolo | apps |
|---|---:|
| `_$s22UniformTypeIdentifiers6UTTypeVMa` | 55 |
| `_$s22UniformTypeIdentifiers6UTTypeVMn` | 51 |
| `_$s22UniformTypeIdentifiers6UTTypeV4dataACvgZ` | 36 |
| `_$s22UniformTypeIdentifiers6UTTypeV10identifierSSvg` | 34 |
| `_$s22UniformTypeIdentifiers6UTTypeV17filenameExtension12conformingToACSgSS_ACtcfC` | 34 |
| `_$s22UniformTypeIdentifiers6UTTypeV19_bridgeToObjectiveCSoABCyF` | 28 |
| `_$s22UniformTypeIdentifiers6UTTypeVyACSgSScfC` | 27 |
| `_$s22UniformTypeIdentifiers6UTTypeV8conforms2toSbAC_tF` | 26 |

### libswiftos.dylib

| símbolo | apps |
|---|---:|
| `_$sSo13os_log_type_ta0A0E5errorABvgZ` | 59 |
| `_$s2os0A4_log_3dso0B0__ySo0a1_B7_type_ta_SVSo03OS_a1_B0Cs12StaticStringVs7CVarArg_pdtF` | 57 |
| `_$sSo13os_log_type_ta0A0E5faultABvgZ` | 55 |
| `_$s2os6LoggerV9logObjectSo03OS_a1_C0Cvg` | 50 |
| `_$s2os6LoggerV9subsystem8categoryACSS_SStcfC` | 50 |
| `_$s2os6LoggerVMa` | 50 |
| `_$sSo13os_log_type_ta0A0E4infoABvgZ` | 48 |
| `_$sSo13os_log_type_ta0A0E5debugABvgZ` | 42 |

### StoreKit.framework

| símbolo | apps |
|---|---:|
| `_$s8StoreKit11TransactionVMa` | 7 |
| `_$s8StoreKit11TransactionVMn` | 7 |
| `_$s8StoreKit18VerificationResultOMn` | 7 |
| `_$s8StoreKit10StorefrontV11countryCodeSSvg` | 6 |
| `_$s8StoreKit10StorefrontVMa` | 6 |
| `_$s8StoreKit11TransactionV12TransactionsV13AsyncIteratorV4nextAA18VerificationResultOyACGSgyYaF` | 6 |
| `_$s8StoreKit11TransactionV12TransactionsV13AsyncIteratorV4nextAA18VerificationResultOyACGSgyYaFTu` | 6 |
| `_$s8StoreKit11TransactionV12TransactionsV13AsyncIteratorVMa` | 6 |

### Carbon.framework

| símbolo | apps |
|---|---:|
| `_GetEventDispatcherTarget` | 41 |
| `_TISCreateInputSourceList` | 31 |
| `_TISCopyInputMethodKeyboardLayoutOverride` | 29 |
| `_kTISPropertyInputSourceIsASCIICapable` | 24 |
| `_TISSelectInputSource` | 21 |
| `_kTISPropertyInputSourceType` | 18 |
| `_GetCurrentEventKeyModifiers` | 16 |
| `_GetEventClass` | 16 |

### libicucore.A.dylib

| símbolo | apps |
|---|---:|
| `_u_strToLower` | 12 |
| `_u_strToUpper` | 12 |
| `_ubrk_close` | 12 |
| `_ubrk_following` | 12 |
| `_ubrk_open` | 12 |
| `_ubrk_setText` | 12 |
| `_unorm2_getNFCInstance` | 12 |
| `_unorm2_normalize` | 12 |

### AVFoundation.framework

| símbolo | apps |
|---|---:|
| `_OBJC_CLASS_$_AVPlayerLayer` | 18 |
| `_OBJC_CLASS_$_AVCaptureStillImageOutput` | 10 |
| `_AVCaptureSessionPresetLow` | 8 |
| `_OBJC_CLASS_$_AVCaptureVideoPreviewLayer` | 8 |
| `_OBJC_CLASS_$_AVMutableMetadataItem` | 8 |
| `_OBJC_CLASS_$_AVAssetExportSession` | 7 |
| `_OBJC_CLASS_$_AVAssetWriterInputPixelBufferAdaptor` | 7 |
| `_OBJC_CLASS_$_AVMetadataItem` | 7 |

### Charts.framework

| símbolo | apps |
|---|---:|
| `_$s6Charts12ChartContentPAAE15foregroundStyleyQrqd__7SwiftUI05ShapeE0Rd__lF` | 6 |
| `_$s6Charts12ChartContentPAAE15foregroundStyleyQrqd__7SwiftUI05ShapeE0Rd__lFQOMQ` | 6 |
| `_$s6Charts14AxisMarkPresetVMa` | 6 |
| `_$s6Charts14AxisMarkValuesVMa` | 6 |
| `_$s6Charts14AxisValueLabelVMn` | 6 |
| `_$s6Charts14AxisValueLabelVyxGAA0B4MarkAAMc` | 6 |
| `_$s6Charts14PlottableValueVMn` | 6 |
| `_$s6Charts16AxisMarkPositionV9automaticACvgZ` | 6 |

### UIKit.framework

| símbolo | apps |
|---|---:|
| `_CGRectFromString` | 1 |
| `_NSDirectionalEdgeInsetsZero` | 1 |
| `_NSStringFromCGAffineTransform` | 1 |
| `_OBJC_CLASS_$_NSCollectionLayoutDecorationItem` | 1 |
| `_OBJC_CLASS_$_NSCollectionLayoutEdgeSpacing` | 1 |
| `_OBJC_CLASS_$_NSDiffableDataSourceSnapshot` | 1 |
| `_OBJC_CLASS_$_NSStringDrawingContext` | 1 |
| `_OBJC_CLASS_$_UIAccessibilityCustomAction` | 1 |

### WebKit.framework

| símbolo | apps |
|---|---:|
| `_OBJC_CLASS_$_WebView` | 86 |
| `_OBJC_CLASS_$_WKUserScript` | 85 |
| `_OBJC_CLASS_$_WKUserContentController` | 78 |
| `_OBJC_CLASS_$_WebPreferences` | 75 |
| `_OBJC_CLASS_$_WKContentRuleListStore` | 46 |
| `_OBJC_CLASS_$_WKContentWorld` | 13 |
| `_OBJC_CLASS_$_WKPreferences` | 13 |
| `_OBJC_CLASS_$_WKProcessPool` | 8 |

### libswift_StringProcessing.dylib

| símbolo | apps |
|---|---:|
| `_$s17_StringProcessing5RegexVMn` | 27 |
| `_$s17_StringProcessing5RegexVyxGAA0C9ComponentAAMc` | 27 |
| `_$s17_StringProcessing5RegexV06_regexA07versionACyxGSS_SitcfC` | 26 |
| `_$s17_StringProcessing5RegexV5MatchVMn` | 26 |
| `_$s17_StringProcessing14RegexComponentP5regexAA0C0Vy0C6OutputQzGvgTj` | 24 |
| `_$s17_StringProcessing5RegexV5MatchV13dynamicMemberqd__s7KeyPathCyxqd__G_tcluig` | 20 |
| `_$s17_StringProcessing5RegexV10firstMatch2inAC0E0Vyx_GSgSs_tKF` | 19 |
| `_$s17_StringProcessing5RegexV5MatchV6outputxvg` | 15 |

## Contagem distinta por app

| app | distintos | sistema | libc++ | Swift | unwind |
|---|---:|---:|---:|---:|---:|
| macwhisper | 3063 | 2903 | 0 | 160 | 0 |
| whatsapp | 2611 | 1262 | 0 | 1349 | 0 |
| bettertouchtool | 2480 | 1716 | 0 | 764 | 0 |
| cmux | 2134 | 2031 | 0 | 103 | 0 |
| vorssaint | 2072 | 1990 | 0 | 82 | 0 |
| openclaw | 2037 | 1895 | 0 | 142 | 0 |
| bartender | 2025 | 1930 | 0 | 95 | 0 |
| codexbar | 1899 | 1851 | 0 | 48 | 0 |
| coteditor | 1878 | 1769 | 0 | 109 | 0 |
| raycast | 1863 | 1579 | 0 | 284 | 0 |
| duckduckgo | 1670 | 830 | 0 | 840 | 0 |
| superwhisper | 1636 | 1459 | 0 | 177 | 0 |
| fluidvoice | 1602 | 1499 | 0 | 103 | 0 |
| supacode | 1511 | 1448 | 0 | 63 | 0 |
| proxyman | 1508 | 1431 | 0 | 77 | 0 |
| voiceink | 1500 | 1403 | 0 | 97 | 0 |
| protonvpn | 1486 | 1416 | 0 | 70 | 0 |
| orbstack | 1424 | 1382 | 0 | 42 | 0 |
| easydict | 1406 | 1226 | 0 | 180 | 0 |
| sfm | 1266 | 1163 | 0 | 103 | 0 |
| tailscale-app | 1191 | 1154 | 0 | 37 | 0 |
| thaw | 1188 | 1086 | 0 | 102 | 0 |
| utm | 964 | 537 | 1 | 426 | 0 |
| opensuperwhisper | 950 | 874 | 1 | 75 | 0 |
| betterdisplay | 916 | 869 | 0 | 47 | 0 |
| tableplus | 907 | 238 | 0 | 669 | 0 |
| windows-app | 884 | 871 | 0 | 13 | 0 |
| iterm2 | 848 | 804 | 0 | 44 | 0 |
| hammerspoon | 836 | 836 | 0 | 0 | 0 |
| caskhub | 826 | 801 | 0 | 25 | 0 |
| clipy | 823 | 804 | 0 | 19 | 0 |
| xcodes-app | 822 | 790 | 0 | 32 | 0 |
| applite | 821 | 788 | 0 | 33 | 0 |
| teamviewer | 806 | 281 | 0 | 525 | 0 |
| cleanshot | 792 | 745 | 0 | 47 | 0 |
| chatgpt | 768 | 640 | 0 | 128 | 0 |
| maccy | 765 | 742 | 0 | 23 | 0 |
| markedit | 762 | 716 | 0 | 46 | 0 |
| pearcleaner | 754 | 732 | 0 | 22 | 0 |
| calibre | 740 | 740 | 0 | 0 | 0 |
| loop | 712 | 681 | 0 | 31 | 0 |
| ghostty | 663 | 630 | 0 | 33 | 0 |
| bbedit | 645 | 645 | 0 | 0 | 0 |
| sequel-ace | 620 | 594 | 0 | 26 | 0 |
| zoom | 615 | 335 | 0 | 280 | 0 |
| dockdoor | 612 | 591 | 0 | 21 | 0 |
| macpacker | 609 | 565 | 0 | 44 | 0 |
| aldente | 549 | 277 | 0 | 272 | 0 |
| puremac | 548 | 535 | 0 | 13 | 0 |
| jordanbaird-ice | 547 | 532 | 0 | 15 | 0 |
| adobe-acrobat-reader | 540 | 540 | 0 | 0 | 0 |
| iina | 535 | 154 | 0 | 381 | 0 |
| homebrew-app | 506 | 458 | 0 | 48 | 0 |
| 1password | 500 | 343 | 0 | 157 | 0 |
| finetune | 492 | 462 | 0 | 30 | 0 |
| inkscape | 470 | 470 | 0 | 0 | 0 |
| microsoft-teams | 461 | 453 | 0 | 8 | 0 |
| deluge | 452 | 452 | 0 | 0 | 0 |
| krita | 449 | 432 | 0 | 17 | 0 |
| blender | 433 | 433 | 0 | 0 | 0 |
| cloudflare-warp | 408 | 396 | 0 | 12 | 0 |
| swiftbar | 377 | 160 | 0 | 217 | 0 |
| linearmouse | 375 | 205 | 0 | 170 | 0 |
| telegram | 372 | 372 | 0 | 0 | 0 |
| qlmarkdown | 365 | 314 | 0 | 51 | 0 |
| shotcut | 363 | 363 | 0 | 0 | 0 |
| stats | 361 | 115 | 0 | 246 | 0 |
| meld | 354 | 354 | 0 | 0 | 0 |
| sourcetree | 350 | 235 | 0 | 115 | 0 |
| syntax-highlight | 350 | 267 | 0 | 83 | 0 |
| fork | 344 | 42 | 0 | 302 | 0 |
| kitty | 339 | 339 | 0 | 0 | 0 |
| shottr | 325 | 60 | 0 | 265 | 0 |
| amethyst | 319 | 62 | 0 | 257 | 0 |
| claude | 314 | 80 | 0 | 234 | 0 |
| virtualbox | 304 | 304 | 0 | 0 | 0 |
| obs | 301 | 290 | 0 | 11 | 0 |
| kicad | 297 | 297 | 0 | 0 | 0 |
| displaylink | 286 | 281 | 0 | 5 | 0 |
| gemini | 285 | 285 | 0 | 0 | 0 |
| wireshark-app | 272 | 272 | 0 | 0 | 0 |
| marta | 269 | 50 | 0 | 219 | 0 |
| localsend | 265 | 161 | 0 | 104 | 0 |
| alt-tab | 255 | 58 | 0 | 197 | 0 |
| keyclu | 252 | 108 | 0 | 144 | 0 |
| lm-studio | 246 | 246 | 0 | 0 | 0 |
| telegram-desktop | 240 | 240 | 0 | 0 | 0 |
| emacs-app | 236 | 57 | 179 | 0 | 0 |
| basictex | 230 | 230 | 0 | 0 | 0 |
| xquartz | 227 | 227 | 0 | 0 | 0 |
| bitwarden | 223 | 219 | 0 | 4 | 0 |
| zotero | 211 | 148 | 0 | 63 | 0 |
| lmms | 206 | 206 | 0 | 0 | 0 |
| rustdesk | 202 | 202 | 0 | 0 | 0 |
| skim | 202 | 202 | 0 | 0 | 0 |
| mac-mouse-fix | 202 | 51 | 0 | 151 | 0 |
| rectangle | 199 | 183 | 0 | 16 | 0 |
| warp | 197 | 26 | 0 | 171 | 0 |
| android-studio | 192 | 192 | 0 | 0 | 0 |
| the-unarchiver | 188 | 188 | 0 | 0 | 0 |
| monitorcontrol | 184 | 184 | 0 | 0 | 0 |
| pgadmin4 | 181 | 181 | 0 | 0 | 0 |
| libreoffice | 180 | 180 | 0 | 0 | 0 |
| opencode-desktop | 176 | 176 | 0 | 0 | 0 |
| stremio | 176 | 127 | 0 | 49 | 0 |
| openscad | 175 | 175 | 0 | 0 | 0 |
| moonlight | 174 | 174 | 0 | 0 | 0 |
| dotnet-sdk | 171 | 171 | 0 | 0 | 0 |
| mos | 156 | 156 | 0 | 0 | 0 |
| jellyfin | 156 | 128 | 0 | 28 | 0 |
| thunderbird | 152 | 152 | 0 | 0 | 0 |
| webtorrent | 152 | 152 | 0 | 0 | 0 |
| karabiner-elements | 152 | 66 | 0 | 86 | 0 |
| openmtp | 148 | 148 | 0 | 0 | 0 |
| quicklook-video | 144 | 36 | 0 | 108 | 0 |
| nextcloud | 143 | 143 | 0 | 0 | 0 |
| pencil | 141 | 141 | 0 | 0 | 0 |
| vnote | 139 | 139 | 0 | 0 | 0 |
| musescore | 136 | 136 | 0 | 0 | 0 |
| syncthing-app | 134 | 134 | 0 | 0 | 0 |
| another-redis-desktop-manager | 134 | 134 | 0 | 0 | 0 |
| r-app | 132 | 132 | 0 | 0 | 0 |
| macs-fan-control | 130 | 130 | 0 | 0 | 0 |
| hex-fiend | 126 | 79 | 0 | 47 | 0 |
| intellij-idea | 126 | 126 | 0 | 0 | 0 |
| audacity | 124 | 124 | 0 | 0 | 0 |
| cyberduck | 123 | 122 | 0 | 1 | 0 |
| jetbrains-toolbox | 122 | 122 | 0 | 0 | 0 |
| unnaturalscrollwheels | 122 | 122 | 0 | 0 | 0 |
| vlc | 120 | 120 | 0 | 0 | 0 |
| firefox | 119 | 119 | 0 | 0 | 0 |
| floorp | 118 | 118 | 0 | 0 | 0 |
| zen | 118 | 118 | 0 | 0 | 0 |
| mullvad-browser | 117 | 117 | 0 | 0 | 0 |
| librewolf | 117 | 117 | 0 | 0 | 0 |
| tor-browser | 115 | 115 | 0 | 0 | 0 |
| cryptomator | 111 | 111 | 0 | 0 | 0 |
| visual-studio-code | 110 | 91 | 0 | 19 | 0 |
| db-browser-for-sqlite | 110 | 110 | 0 | 0 | 0 |
| gimp | 109 | 109 | 0 | 0 | 0 |
| prusaslicer | 108 | 108 | 0 | 0 | 0 |
| raspberry-pi-imager | 108 | 108 | 0 | 0 | 0 |
| temurin | 106 | 106 | 0 | 0 | 0 |
| github | 106 | 87 | 0 | 19 | 0 |
| dbeaver-community | 105 | 105 | 0 | 0 | 0 |
| keepassxc | 103 | 103 | 0 | 0 | 0 |
| dolphin | 97 | 97 | 0 | 0 | 0 |
| drawio | 95 | 28 | 0 | 67 | 0 |
| prismlauncher | 95 | 95 | 0 | 0 | 0 |
| squirrel-app | 89 | 73 | 0 | 16 | 0 |
| alfred | 88 | 88 | 0 | 0 | 0 |
| docker-desktop | 87 | 87 | 0 | 0 | 0 |
| daisydisk | 82 | 82 | 0 | 0 | 0 |
| qbittorrent | 80 | 80 | 0 | 0 | 0 |
| espanso | 79 | 79 | 0 | 0 | 0 |
| obsidian | 77 | 3 | 0 | 74 | 0 |
| spotify | 77 | 77 | 0 | 0 | 0 |
| realvnc-connect-viewer | 75 | 24 | 0 | 51 | 0 |
| figma | 67 | 67 | 0 | 0 | 0 |
| battery | 67 | 67 | 0 | 0 | 0 |
| cursor | 64 | 0 | 0 | 64 | 0 |
| android-file-transfer | 60 | 60 | 0 | 0 | 0 |
| tic80 | 58 | 58 | 0 | 0 | 0 |
| unity-hub | 57 | 57 | 0 | 0 | 0 |
| keka | 49 | 49 | 0 | 0 | 0 |
| handbrake-app | 49 | 49 | 0 | 0 | 0 |
| typora | 49 | 49 | 0 | 0 | 0 |
| mongodb-compass | 48 | 38 | 10 | 0 | 0 |
| mounty | 47 | 47 | 0 | 0 | 0 |
| git-credential-manager | 46 | 46 | 0 | 0 | 0 |
| zed | 45 | 45 | 0 | 0 | 0 |
| microsoft-edge | 44 | 44 | 0 | 0 | 0 |
| comfy | 43 | 43 | 0 | 0 | 0 |
| openrct2 | 42 | 42 | 0 | 0 | 0 |
| love | 40 | 40 | 0 | 0 | 0 |
| openlogi | 40 | 40 | 0 | 0 | 0 |
| ubersicht | 39 | 39 | 0 | 0 | 0 |
| flycut | 38 | 38 | 0 | 0 | 0 |
| clash-verge-rev | 38 | 38 | 0 | 0 | 0 |
| fuse-t | 37 | 37 | 0 | 0 | 0 |
| transmission | 36 | 36 | 0 | 0 | 0 |
| godot | 36 | 36 | 0 | 0 | 0 |
| pixelorama | 36 | 36 | 0 | 0 | 0 |
| lulu | 36 | 36 | 0 | 0 | 0 |
| hiddenbar | 35 | 35 | 0 | 0 | 0 |
| multipass | 35 | 35 | 0 | 0 | 0 |
| epic-games | 34 | 34 | 0 | 0 | 0 |
| bruno | 33 | 33 | 0 | 0 | 0 |
| anydesk | 33 | 33 | 0 | 0 | 0 |
| tunnelblick | 33 | 33 | 0 | 0 | 0 |
| ollama-app | 32 | 32 | 0 | 0 | 0 |
| middleclick | 31 | 8 | 0 | 23 | 0 |
| openttd | 30 | 30 | 0 | 0 | 0 |
| handy | 29 | 29 | 0 | 0 | 0 |
| sublime-text | 28 | 28 | 0 | 0 | 0 |
| macdown-3000 | 28 | 28 | 0 | 0 | 0 |
| macdown | 27 | 27 | 0 | 0 | 0 |
| microsoft-auto-update | 24 | 24 | 0 | 0 | 0 |
| steam | 23 | 23 | 0 | 0 | 0 |
| cc-switch | 21 | 21 | 0 | 0 | 0 |
| dbx | 21 | 21 | 0 | 0 | 0 |
| libndi | 21 | 21 | 0 | 0 | 0 |
| keycastr | 20 | 20 | 0 | 0 | 0 |
| vagrant | 18 | 18 | 0 | 0 | 0 |
| postman | 15 | 15 | 0 | 0 | 0 |
| brave-browser | 14 | 14 | 0 | 0 | 0 |
| tigervnc | 14 | 14 | 0 | 0 | 0 |
| headlamp | 14 | 14 | 0 | 0 | 0 |
| itsycal | 14 | 14 | 0 | 0 | 0 |
| google-chrome | 13 | 13 | 0 | 0 | 0 |
| antigravity-ide | 13 | 13 | 0 | 0 | 0 |
| lunar | 13 | 13 | 0 | 0 | 0 |
| appcleaner | 12 | 12 | 0 | 0 | 0 |
| scroll-reverser | 12 | 12 | 0 | 0 | 0 |
| alacritty | 11 | 11 | 0 | 0 | 0 |
| imageoptim | 10 | 10 | 0 | 0 | 0 |
| antigravity | 8 | 0 | 0 | 8 | 0 |
| insomnia | 7 | 7 | 0 | 0 | 0 |
| t3-code | 7 | 7 | 0 | 0 | 0 |
| notunes | 7 | 0 | 0 | 7 | 0 |
| background-music | 6 | 6 | 0 | 0 | 0 |
| keepingyouawake | 5 | 5 | 0 | 0 | 0 |
| redis-insight | 4 | 4 | 0 | 0 | 0 |
| grandperspective | 4 | 4 | 0 | 0 | 0 |
| wave | 4 | 4 | 0 | 0 | 0 |
| wailbrew | 4 | 4 | 0 | 0 | 0 |
| block-goose | 3 | 3 | 0 | 0 | 0 |
| macfuse | 2 | 2 | 0 | 0 | 0 |
| wezterm | 2 | 2 | 0 | 0 | 0 |
| signal | 2 | 2 | 0 | 0 | 0 |
| openvpn-connect | 2 | 2 | 0 | 0 | 0 |
| blackhole-2ch | 1 | 1 | 0 | 0 | 0 |
| caffeine | 1 | 1 | 0 | 0 | 0 |
| blackhole-16ch | 1 | 1 | 0 | 0 | 0 |
| helium-browser | 1 | 1 | 0 | 0 | 0 |
| termius | 1 | 1 | 0 | 0 | 0 |
| element | 0 | 0 | 0 | 0 | 0 |
| finicky | 0 | 0 | 0 | 0 | 0 |
| vscodium | 0 | 0 | 0 | 0 | 0 |
| dbgate | 0 | 0 | 0 | 0 | 0 |
| session-manager-plugin | 0 | 0 | 0 | 0 | 0 |
| discord | 0 | 0 | 0 | 0 | 0 |
| slack | 0 | 0 | 0 | 0 | 0 |
| notion | 0 | 0 | 0 | 0 | 0 |
| google-drive | 0 | 0 | 0 | 0 | 0 |
| podman-desktop | 0 | 0 | 0 | 0 | 0 |
| balenaetcher | 0 | 0 | 0 | 0 | 0 |
| ungoogled-chromium | 0 | 0 | 0 | 0 | 0 |
| onyx | 0 | 0 | 0 | 0 | 0 |
| tabby | 0 | 0 | 0 | 0 | 0 |
