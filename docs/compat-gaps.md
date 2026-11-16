# Lacunas de compatibilidade no corpus macOS

Gerado por `python3 tools/corpus/harness.py gaps` com as medidas atuais de `tools/corpus/varrer-tudo.sh`.
libSystem medido: main ce6dc05c (merge #392), copiado de build do Ricardo 2026-11-16_01:41.
O disco é o da varredura: libunwind, libc++, libswift*, libz, libcurl e, com o artefato do CI, os frameworks do
mOS. Os baldes libc++ e Swift só contam o que nenhuma dessas exporta.
Binários não estão no git. URLs e sha256 ficam em `tools/corpus/manifest.json`.

Apps com pelo menos uma imagem Mach-O x86_64: **182**.

## Símbolos que faltam em mais apps

| símbolo | apps |
|---|---:|
| `_$sSS10FoundationE19_bridgeToObjectiveCSo8NSStringCyF` | 62 |
| `_$sSS10FoundationE36_unconditionallyBridgeFromObjectiveCySSSo8NSStringCSgFZ` | 61 |
| `_$sSa10FoundationE19_bridgeToObjectiveCSo7NSArrayCyF` | 59 |
| `_$s10Foundation3URLVMa` | 58 |
| `_$s10Foundation3URLVMn` | 58 |
| `_$sSa10FoundationE36_unconditionallyBridgeFromObjectiveCySayxGSo7NSArrayCSgFZ` | 58 |
| `_$s10Foundation3URLV36_unconditionallyBridgeFromObjectiveCyACSo5NSURLCSgFZ` | 57 |
| `_$s10Foundation4DataV19_bridgeToObjectiveCSo6NSDataCyF` | 57 |
| `_$s10Foundation3URLV19_bridgeToObjectiveCSo5NSURLCyF` | 56 |
| `_$s10Foundation4DataV36_unconditionallyBridgeFromObjectiveCyACSo6NSDataCSgFZ` | 56 |
| `_$sSD10FoundationE19_bridgeToObjectiveCSo12NSDictionaryCyF` | 56 |
| `_$s10Foundation22_convertNSErrorToErrorys0E0_pSo0C0CSgF` | 55 |
| `_$ss5ErrorP10FoundationE20localizedDescriptionSSvg` | 55 |
| `_$sSD10FoundationE36_unconditionallyBridgeFromObjectiveCySDyxq_GSo12NSDictionaryCSgFZ` | 54 |
| `_OBJC_CLASS_$_WebView` | 54 |
| `_$sSS10FoundationE26_forceBridgeFromObjectiveC_6resultySo8NSStringC_SSSgztFZ` | 53 |
| `_$sSS10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo8NSStringC_SSSgztFZ` | 53 |
| `_OBJC_CLASS_$_WKUserScript` | 53 |
| `_$s10Foundation12NotificationV36_unconditionallyBridgeFromObjectiveCyACSo14NSNotificationCSgFZ` | 52 |
| `_$s10Foundation12NotificationVMa` | 52 |
| `_$s10Foundation3URLV4pathSSvg` | 52 |
| `_$s10Foundation3URLV6stringACSgSSh_tcfC` | 52 |
| `_$sSS10FoundationE8EncodingV4utf8ACvgZ` | 52 |
| `_$sSS10FoundationE8EncodingVMa` | 52 |
| `_$s10Foundation4DateVMa` | 51 |
| `_$s10Foundation4UUIDVMa` | 51 |
| `_$s10Foundation4DataVN` | 50 |
| `_$s10Foundation6LocaleVMa` | 50 |
| `_$sSS10FoundationE4data8encodingSSSgAA4DataVh_SSAAE8EncodingVtcfC` | 50 |
| `_$sSy10FoundationE20replacingOccurrences2of4with7options5rangeSSqd___qd_0_So22NSStringCompareOptionsVSnySS5IndexVGSgtSyRd__SyRd_0_r0_lF` | 50 |
| `_$s10Foundation11JSONDecoderC6decode_4fromxxm_AA4DataVtKSeRzlFTj` | 49 |
| `_$s10Foundation11JSONDecoderCACycfc` | 49 |
| `_$s10Foundation11JSONDecoderCMa` | 49 |
| `_$s10Foundation3URLV22appendingPathComponentyACSSF` | 49 |
| `_$s10Foundation4DataVMn` | 49 |
| `_$s10Foundation4DateV19_bridgeToObjectiveCSo6NSDateCyF` | 49 |
| `_$s10Foundation4UUIDVACycfC` | 49 |
| `_$s10Foundation11JSONEncoderC6encodeyAA4DataVxKSERzlFTj` | 48 |
| `_$s10Foundation11JSONEncoderCACycfc` | 48 |
| `_$s10Foundation11JSONEncoderCMa` | 48 |

## Por origem

### sistema

| símbolo | apps |
|---|---:|
| `_OBJC_CLASS_$_WebView` | 54 |
| `_OBJC_CLASS_$_WKUserScript` | 53 |
| `_OBJC_CLASS_$_WKUserContentController` | 48 |
| `_CFBundleGetBundleWithIdentifier` | 45 |
| `_OBJC_CLASS_$_WebPreferences` | 44 |
| `_OBJC_CLASS_$_NSAppleScript` | 37 |
| `_$s10Foundation3URLVMn` | 35 |
| `_$sSS10FoundationE19_bridgeToObjectiveCSo8NSStringCyF` | 35 |
| `_$sSS10FoundationE36_unconditionallyBridgeFromObjectiveCySSSo8NSStringCSgFZ` | 35 |
| `_$sSa10FoundationE19_bridgeToObjectiveCSo7NSArrayCyF` | 35 |
| `_putenv` | 35 |
| `_$s10Foundation3URLV36_unconditionallyBridgeFromObjectiveCyACSo5NSURLCSgFZ` | 34 |
| `_$s10Foundation3URLVMa` | 34 |
| `_$sSS10FoundationE26_forceBridgeFromObjectiveC_6resultySo8NSStringC_SSSgztFZ` | 34 |
| `_$sSS10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo8NSStringC_SSSgztFZ` | 34 |

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
| `_$s22UniformTypeIdentifiers6UTTypeVMa` | 37 |
| `_$s2os0A4_log_3dso0B0__ySo0a1_B7_type_ta_SVSo03OS_a1_B0Cs12StaticStringVs7CVarArg_pdtF` | 36 |
| `_$sSo13os_log_type_ta0A0E5errorABvgZ` | 36 |
| `_$s22UniformTypeIdentifiers6UTTypeVMn` | 34 |
| `_$sSo13os_log_type_ta0A0E5faultABvgZ` | 34 |
| `_$s2os6LoggerV9logObjectSo03OS_a1_C0Cvg` | 32 |
| `_$s2os6LoggerV9subsystem8categoryACSS_SStcfC` | 32 |
| `_$s2os6LoggerVMa` | 32 |
| `_$sSo13os_log_type_ta0A0E4infoABvgZ` | 29 |
| `_$sSS10FoundationE19_bridgeToObjectiveCSo8NSStringCyF` | 27 |
| `_$sSS10FoundationE36_unconditionallyBridgeFromObjectiveCySSSo8NSStringCSgFZ` | 26 |
| `_$sSo13os_log_type_ta0A0E5debugABvgZ` | 26 |
| `_$s10Foundation3URLVMa` | 24 |
| `_$s10Foundation4DataV19_bridgeToObjectiveCSo6NSDataCyF` | 24 |
| `_$s10Foundation4DataV36_unconditionallyBridgeFromObjectiveCyACSo6NSDataCSgFZ` | 24 |

### unwind

| símbolo | apps |
|---|---:|
| — | 0 |

## Por framework

### Foundation.framework

| símbolo | apps |
|---|---:|
| `_OBJC_CLASS_$_NSAppleScript` | 37 |
| `_$s10Foundation3URLVMn` | 35 |
| `_$sSS10FoundationE19_bridgeToObjectiveCSo8NSStringCyF` | 35 |
| `_$sSS10FoundationE36_unconditionallyBridgeFromObjectiveCySSSo8NSStringCSgFZ` | 35 |
| `_$sSa10FoundationE19_bridgeToObjectiveCSo7NSArrayCyF` | 35 |
| `_$s10Foundation3URLV36_unconditionallyBridgeFromObjectiveCyACSo5NSURLCSgFZ` | 34 |
| `_$s10Foundation3URLVMa` | 34 |
| `_$sSS10FoundationE26_forceBridgeFromObjectiveC_6resultySo8NSStringC_SSSgztFZ` | 34 |

### SwiftUI.framework

| símbolo | apps |
|---|---:|
| `_$s7SwiftUI17VerticalAlignmentV3topACvgZ` | 30 |
| `_$s7SwiftUI6IDViewVMn` | 29 |
| `_$s7SwiftUI26NSViewRepresentableContextV11coordinator11CoordinatorQzvg` | 25 |
| `_$s7SwiftUI4ViewPAAE11toggleStyleyQrqd__AA06ToggleE0Rd__lF` | 25 |
| `_$s7SwiftUI4ViewPAAE11toggleStyleyQrqd__AA06ToggleE0Rd__lFQOMQ` | 25 |
| `_$s7SwiftUI6IDViewVyxq_GAA4ViewAAMc` | 25 |
| `_$s7SwiftUI6VStackVMa` | 25 |
| `_$s7SwiftUI21PinnedScrollableViewsVMa` | 24 |

### libswiftFoundation.dylib

| símbolo | apps |
|---|---:|
| `_$sSS10FoundationE19_bridgeToObjectiveCSo8NSStringCyF` | 27 |
| `_$sSS10FoundationE36_unconditionallyBridgeFromObjectiveCySSSo8NSStringCSgFZ` | 26 |
| `_$s10Foundation3URLVMa` | 24 |
| `_$s10Foundation4DataV19_bridgeToObjectiveCSo6NSDataCyF` | 24 |
| `_$s10Foundation4DataV36_unconditionallyBridgeFromObjectiveCyACSo6NSDataCSgFZ` | 24 |
| `_$sSa10FoundationE19_bridgeToObjectiveCSo7NSArrayCyF` | 24 |
| `_$sSa10FoundationE36_unconditionallyBridgeFromObjectiveCySayxGSo7NSArrayCSgFZ` | 24 |
| `_$s10Foundation3URLV19_bridgeToObjectiveCSo5NSURLCyF` | 23 |

### CoreFoundation.framework

| símbolo | apps |
|---|---:|
| `_CFBundleGetBundleWithIdentifier` | 44 |
| `_CFStringGetSystemEncoding` | 33 |
| `_kCFLocaleLanguageCode` | 31 |
| `_CFSocketCreateWithNative` | 26 |
| `_CFURLEnumeratorGetNextURL` | 26 |
| `_kCFURLIsPackageKey` | 26 |
| `_CFSocketGetSocketFlags` | 23 |
| `_kCFLocaleCurrencyCode` | 23 |

### Combine.framework

| símbolo | apps |
|---|---:|
| `_$s7Combine10PublishersO3MapVMn` | 20 |
| `_$s7Combine10PublishersO3MapVy_xq_GAA9PublisherAAMc` | 20 |
| `_$s7Combine25ObservableObjectPublisherC4sendyyF` | 20 |
| `_$s7Combine9PublisherPAAE3mapyAA10PublishersO3MapVy_xqd__Gqd__6OutputQzclF` | 20 |
| `_$s7Combine11CancellableMp` | 19 |
| `_$s7Combine9PublisherMp` | 18 |
| `_$s6Output7Combine9PublisherPTl` | 17 |
| `_$s7Combine06CustomA21IdentifierConvertibleMp` | 17 |

### AppIntents.framework

| símbolo | apps |
|---|---:|
| `_$s10AppIntents09_SupportsA12DependenciesMp` | 11 |
| `_$s10AppIntents0A6IntentMp` | 11 |
| `_$s10AppIntents0A6IntentP04openA7WhenRunSbvgZTq` | 11 |
| `_$s10AppIntents0A6IntentP11descriptionAA0C11DescriptionVSgvgZTq` | 11 |
| `_$s10AppIntents0A6IntentP13PerformResultAC_AA0cE0Tn` | 11 |
| `_$s10AppIntents0A6IntentP14SummaryContentAC_AA09ParameterD0Tn` | 11 |
| `_$s10AppIntents0A6IntentP16parameterSummary0E7ContentQzvgZTq` | 11 |
| `_$s10AppIntents0A6IntentP20authenticationPolicyAA0c14AuthenticationE0OvgZTq` | 11 |

### libSystem.B.dylib

| símbolo | apps |
|---|---:|
| `_putenv` | 35 |
| `_getloadavg` | 26 |
| `_getprotobyname` | 24 |
| `_OSAtomicAdd32Barrier` | 19 |
| `__os_activity_create` | 18 |
| `_os_activity_scope_enter` | 18 |
| `_os_activity_scope_leave` | 18 |
| `_pthread_attr_getschedpolicy` | 18 |

### AppKit.framework

| símbolo | apps |
|---|---:|
| `_OBJC_CLASS_$_NSFontPanel` | 27 |
| `_OBJC_CLASS_$_NSPageLayout` | 25 |
| `_NSAccessibilityUnignoredChildrenForOnlyChild` | 23 |
| `_NSFileContentsPboardType` | 22 |
| `_OBJC_CLASS_$_NSPrintOperation` | 20 |
| `_NSApplicationLoad` | 18 |
| `_OBJC_CLASS_$_NSStepperCell` | 17 |
| `_NSControlTintDidChangeNotification` | 16 |

### Security.framework

| símbolo | apps |
|---|---:|
| `_kSecAttrServer` | 29 |
| `_kSecAttrProtocol` | 22 |
| `_SSLSetDiffieHellmanParams` | 17 |
| `_kSecAttrProtocolHTTPProxy` | 16 |
| `_kSecAttrProtocolHTTPSProxy` | 16 |
| `_kSecImportExportKeychain` | 16 |
| `_SecIdentitySearchCopyNext` | 15 |
| `_SecIdentitySearchCreate` | 15 |

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

### CryptoKit.framework

| símbolo | apps |
|---|---:|
| `_$s9CryptoKit12HashFunctionP6update13bufferPointerySW_tFTj` | 25 |
| `_$s9CryptoKit12HashFunctionP8finalize6DigestQzyFTj` | 25 |
| `_$s9CryptoKit6SHA256VAA12HashFunctionAAMc` | 25 |
| `_$s9CryptoKit6SHA256VMa` | 25 |
| `_$s9CryptoKit12HashFunctionPxycfCTj` | 24 |
| `_$s9CryptoKit12SHA256DigestVMa` | 24 |
| `_$s9CryptoKit12SHA256DigestVSTAAMc` | 21 |
| `_$s9CryptoKit12SymmetricKeyVMa` | 19 |

### CoreServices.framework

| símbolo | apps |
|---|---:|
| `_GetIconRef` | 25 |
| `_kUTTypeFileURL` | 25 |
| `_kUTTagClassNSPboardType` | 23 |
| `_ReleaseIconRef` | 22 |
| `_UCCreateCollator` | 20 |
| `_UCGetCollationKey` | 19 |
| `_FSFindFolder` | 18 |
| `_FSPathMakeRefWithOptions` | 18 |

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

### ApplicationServices.framework

| símbolo | apps |
|---|---:|
| `_PasteboardCreate` | 28 |
| `_PasteboardSynchronize` | 28 |
| `_GetCurrentProcess` | 19 |
| `_PMRetain` | 19 |
| `_PasteboardCopyItemFlavorData` | 18 |
| `_PasteboardCopyItemFlavors` | 18 |
| `_PasteboardGetItemCount` | 18 |
| `_PasteboardGetItemIdentifier` | 18 |

### CoreGraphics.framework

| símbolo | apps |
|---|---:|
| `_kCGColorWhite` | 24 |
| `_CGDisplayCopyAllDisplayModes` | 22 |
| `_CGEventSourceButtonState` | 20 |
| `_CGEventTapCreateForPid` | 20 |
| `_CGDisplayMoveCursorToPoint` | 18 |
| `_CGDisplaySetDisplayMode` | 18 |
| `_CGDisplayCapture` | 17 |
| `_CGDisplayModeCopyPixelEncoding` | 17 |

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

### Carbon.framework

| símbolo | apps |
|---|---:|
| `_GetEventDispatcherTarget` | 28 |
| `_TISCopyInputMethodKeyboardLayoutOverride` | 25 |
| `_TISCreateInputSourceList` | 25 |
| `_kTISPropertyInputSourceIsASCIICapable` | 21 |
| `_TISSelectInputSource` | 19 |
| `_kTISPropertyInputSourceType` | 18 |
| `_GetCurrentEventKeyModifiers` | 15 |
| `_kTISTypeKeyboardInputMode` | 15 |

### libicucore.A.dylib

| símbolo | apps |
|---|---:|
| `_u_strToLower` | 9 |
| `_u_strToUpper` | 9 |
| `_ubrk_close` | 9 |
| `_ubrk_following` | 9 |
| `_ubrk_open` | 9 |
| `_ubrk_setText` | 9 |
| `_unorm2_getNFCInstance` | 9 |
| `_unorm2_normalize` | 9 |

### libswiftUniformTypeIdentifiers.dylib

| símbolo | apps |
|---|---:|
| `_$s22UniformTypeIdentifiers6UTTypeVMa` | 37 |
| `_$s22UniformTypeIdentifiers6UTTypeVMn` | 34 |
| `_$s22UniformTypeIdentifiers6UTTypeV10identifierSSvg` | 23 |
| `_$s22UniformTypeIdentifiers6UTTypeV4dataACvgZ` | 21 |
| `_$s22UniformTypeIdentifiers6UTTypeV17filenameExtension12conformingToACSgSS_ACtcfC` | 20 |
| `_$s22UniformTypeIdentifiers6UTTypeV19_bridgeToObjectiveCSoABCyF` | 18 |
| `_$s22UniformTypeIdentifiers6UTTypeVyACSgSScfC` | 15 |
| `_$s22UniformTypeIdentifiers6UTTypeV8conforms2toSbAC_tF` | 14 |

### AVFoundation.framework

| símbolo | apps |
|---|---:|
| `_OBJC_CLASS_$_AVPlayerLayer` | 14 |
| `_AVCaptureSessionPresetLow` | 8 |
| `_OBJC_CLASS_$_AVAssetWriterInputPixelBufferAdaptor` | 7 |
| `_OBJC_CLASS_$_AVCaptureStillImageOutput` | 7 |
| `_OBJC_CLASS_$_AVMutableMetadataItem` | 7 |
| `_OBJC_CLASS_$_AVPlayerItemVideoOutput` | 7 |
| `_AVFileTypeWAVE` | 5 |
| `_OBJC_CLASS_$_AVMetadataItem` | 5 |

### libswiftos.dylib

| símbolo | apps |
|---|---:|
| `_$s2os0A4_log_3dso0B0__ySo0a1_B7_type_ta_SVSo03OS_a1_B0Cs12StaticStringVs7CVarArg_pdtF` | 36 |
| `_$sSo13os_log_type_ta0A0E5errorABvgZ` | 36 |
| `_$sSo13os_log_type_ta0A0E5faultABvgZ` | 34 |
| `_$s2os6LoggerV9logObjectSo03OS_a1_C0Cvg` | 32 |
| `_$s2os6LoggerV9subsystem8categoryACSS_SStcfC` | 32 |
| `_$s2os6LoggerVMa` | 32 |
| `_$sSo13os_log_type_ta0A0E4infoABvgZ` | 29 |
| `_$sSo13os_log_type_ta0A0E5debugABvgZ` | 26 |

### libcups.2.dylib

| símbolo | apps |
|---|---:|
| `_ppdClose` | 17 |
| `_ppdFindOption` | 17 |
| `_ppdFindMarkedChoice` | 14 |
| `_cupsGetDests2` | 13 |
| `_httpGetHostname` | 11 |
| `_ppdOpenFile` | 11 |
| `_cupsConnectDest` | 8 |
| `_cupsGetDestMediaByIndex` | 8 |

### IOKit.framework

| símbolo | apps |
|---|---:|
| `_IODisplayCreateInfoDictionary` | 20 |
| `_IOHIDDeviceGetValue` | 13 |
| `_IOIteratorIsValid` | 12 |
| `_IOHIDElementGetTypeID` | 11 |
| `_IOObjectIsEqualTo` | 11 |
| `_IOHIDElementGetCookie` | 10 |
| `_IODispatchCalloutFromMessage` | 9 |
| `_IOHIDElementGetChildren` | 9 |

### Charts.framework

| símbolo | apps |
|---|---:|
| `_$s6Charts12ChartContentPAAE15foregroundStyleyQrqd__7SwiftUI05ShapeE0Rd__lF` | 4 |
| `_$s6Charts12ChartContentPAAE15foregroundStyleyQrqd__7SwiftUI05ShapeE0Rd__lFQOMQ` | 4 |
| `_$s6Charts14AxisMarkPresetVMa` | 4 |
| `_$s6Charts14AxisMarkValuesV9automaticACvgZ` | 4 |
| `_$s6Charts14AxisMarkValuesVMa` | 4 |
| `_$s6Charts14AxisValueLabelVMn` | 4 |
| `_$s6Charts14AxisValueLabelVyxGAA0B4MarkAAMc` | 4 |
| `_$s6Charts14PlottableValueVMn` | 4 |

### libedit.3.dylib

| símbolo | apps |
|---|---:|
| `_add_history` | 10 |
| `_read_history` | 9 |
| `_rl_attempted_completion_function` | 9 |
| `_write_history` | 9 |
| `_history_truncate_file` | 8 |
| `_rl_attempted_completion_over` | 8 |
| `_rl_completion_append_character` | 8 |
| `_rl_completion_matches` | 8 |

## Contagem distinta por app

| app | distintos | sistema | libc++ | Swift | unwind |
|---|---:|---:|---:|---:|---:|
| whatsapp | 2611 | 1262 | 0 | 1349 | 0 |
| cmux | 2134 | 2031 | 0 | 103 | 0 |
| vorssaint | 2072 | 1990 | 0 | 82 | 0 |
| openclaw | 2037 | 1895 | 0 | 142 | 0 |
| codexbar | 1899 | 1851 | 0 | 48 | 0 |
| coteditor | 1878 | 1769 | 0 | 109 | 0 |
| raycast | 1863 | 1579 | 0 | 284 | 0 |
| protonvpn | 1486 | 1416 | 0 | 70 | 0 |
| orbstack | 1424 | 1382 | 0 | 42 | 0 |
| tailscale-app | 1191 | 1154 | 0 | 37 | 0 |
| thaw | 1188 | 1086 | 0 | 102 | 0 |
| utm | 964 | 537 | 1 | 426 | 0 |
| betterdisplay | 916 | 869 | 0 | 47 | 0 |
| tableplus | 907 | 238 | 0 | 669 | 0 |
| windows-app | 884 | 871 | 0 | 13 | 0 |
| iterm2 | 848 | 804 | 0 | 44 | 0 |
| hammerspoon | 836 | 836 | 0 | 0 | 0 |
| clipy | 823 | 804 | 0 | 19 | 0 |
| xcodes-app | 822 | 790 | 0 | 32 | 0 |
| applite | 821 | 788 | 0 | 33 | 0 |
| chatgpt | 768 | 640 | 0 | 128 | 0 |
| maccy | 765 | 742 | 0 | 23 | 0 |
| markedit | 762 | 716 | 0 | 46 | 0 |
| pearcleaner | 754 | 732 | 0 | 22 | 0 |
| calibre | 740 | 740 | 0 | 0 | 0 |
| ghostty | 663 | 630 | 0 | 33 | 0 |
| sequel-ace | 620 | 594 | 0 | 26 | 0 |
| zoom | 615 | 335 | 0 | 280 | 0 |
| dockdoor | 612 | 591 | 0 | 21 | 0 |
| aldente | 549 | 277 | 0 | 272 | 0 |
| puremac | 548 | 535 | 0 | 13 | 0 |
| jordanbaird-ice | 547 | 532 | 0 | 15 | 0 |
| adobe-acrobat-reader | 540 | 540 | 0 | 0 | 0 |
| iina | 535 | 154 | 0 | 381 | 0 |
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
| kitty | 339 | 339 | 0 | 0 | 0 |
| shottr | 325 | 60 | 0 | 265 | 0 |
| claude | 314 | 80 | 0 | 234 | 0 |
| virtualbox | 304 | 304 | 0 | 0 | 0 |
| obs | 301 | 290 | 0 | 11 | 0 |
| kicad | 297 | 297 | 0 | 0 | 0 |
| gemini | 285 | 285 | 0 | 0 | 0 |
| wireshark-app | 272 | 272 | 0 | 0 | 0 |
| localsend | 265 | 161 | 0 | 104 | 0 |
| alt-tab | 255 | 58 | 0 | 197 | 0 |
| lm-studio | 246 | 246 | 0 | 0 | 0 |
| emacs-app | 236 | 57 | 179 | 0 | 0 |
| basictex | 230 | 230 | 0 | 0 | 0 |
| xquartz | 227 | 227 | 0 | 0 | 0 |
| bitwarden | 223 | 219 | 0 | 4 | 0 |
| zotero | 211 | 148 | 0 | 63 | 0 |
| lmms | 206 | 206 | 0 | 0 | 0 |
| rustdesk | 202 | 202 | 0 | 0 | 0 |
| rectangle | 199 | 183 | 0 | 16 | 0 |
| warp | 197 | 26 | 0 | 171 | 0 |
| android-studio | 192 | 192 | 0 | 0 | 0 |
| the-unarchiver | 188 | 188 | 0 | 0 | 0 |
| monitorcontrol | 184 | 184 | 0 | 0 | 0 |
| pgadmin4 | 181 | 181 | 0 | 0 | 0 |
| libreoffice | 180 | 180 | 0 | 0 | 0 |
| opencode-desktop | 176 | 176 | 0 | 0 | 0 |
| openscad | 175 | 175 | 0 | 0 | 0 |
| moonlight | 174 | 174 | 0 | 0 | 0 |
| dotnet-sdk | 171 | 171 | 0 | 0 | 0 |
| mos | 156 | 156 | 0 | 0 | 0 |
| thunderbird | 152 | 152 | 0 | 0 | 0 |
| webtorrent | 152 | 152 | 0 | 0 | 0 |
| karabiner-elements | 152 | 66 | 0 | 86 | 0 |
| openmtp | 148 | 148 | 0 | 0 | 0 |
| pencil | 141 | 141 | 0 | 0 | 0 |
| vnote | 139 | 139 | 0 | 0 | 0 |
| musescore | 136 | 136 | 0 | 0 | 0 |
| hex-fiend | 126 | 79 | 0 | 47 | 0 |
| intellij-idea | 126 | 126 | 0 | 0 | 0 |
| audacity | 124 | 124 | 0 | 0 | 0 |
| cyberduck | 123 | 122 | 0 | 1 | 0 |
| jetbrains-toolbox | 122 | 122 | 0 | 0 | 0 |
| vlc | 120 | 120 | 0 | 0 | 0 |
| firefox | 119 | 119 | 0 | 0 | 0 |
| floorp | 118 | 118 | 0 | 0 | 0 |
| zen | 118 | 118 | 0 | 0 | 0 |
| mullvad-browser | 117 | 117 | 0 | 0 | 0 |
| librewolf | 117 | 117 | 0 | 0 | 0 |
| tor-browser | 115 | 115 | 0 | 0 | 0 |
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
| alfred | 88 | 88 | 0 | 0 | 0 |
| docker-desktop | 87 | 87 | 0 | 0 | 0 |
| qbittorrent | 80 | 80 | 0 | 0 | 0 |
| obsidian | 77 | 3 | 0 | 74 | 0 |
| spotify | 77 | 77 | 0 | 0 | 0 |
| figma | 67 | 67 | 0 | 0 | 0 |
| cursor | 64 | 0 | 0 | 64 | 0 |
| tic80 | 58 | 58 | 0 | 0 | 0 |
| unity-hub | 57 | 57 | 0 | 0 | 0 |
| keka | 49 | 49 | 0 | 0 | 0 |
| handbrake-app | 49 | 49 | 0 | 0 | 0 |
| mongodb-compass | 48 | 38 | 10 | 0 | 0 |
| mounty | 47 | 47 | 0 | 0 | 0 |
| git-credential-manager | 46 | 46 | 0 | 0 | 0 |
| zed | 45 | 45 | 0 | 0 | 0 |
| microsoft-edge | 44 | 44 | 0 | 0 | 0 |
| comfy | 43 | 43 | 0 | 0 | 0 |
| openrct2 | 42 | 42 | 0 | 0 | 0 |
| love | 40 | 40 | 0 | 0 | 0 |
| openlogi | 40 | 40 | 0 | 0 | 0 |
| flycut | 38 | 38 | 0 | 0 | 0 |
| clash-verge-rev | 38 | 38 | 0 | 0 | 0 |
| fuse-t | 37 | 37 | 0 | 0 | 0 |
| transmission | 36 | 36 | 0 | 0 | 0 |
| godot | 36 | 36 | 0 | 0 | 0 |
| pixelorama | 36 | 36 | 0 | 0 | 0 |
| lulu | 36 | 36 | 0 | 0 | 0 |
| hiddenbar | 35 | 35 | 0 | 0 | 0 |
| multipass | 35 | 35 | 0 | 0 | 0 |
| bruno | 33 | 33 | 0 | 0 | 0 |
| anydesk | 33 | 33 | 0 | 0 | 0 |
| ollama-app | 32 | 32 | 0 | 0 | 0 |
| openttd | 30 | 30 | 0 | 0 | 0 |
| handy | 29 | 29 | 0 | 0 | 0 |
| sublime-text | 28 | 28 | 0 | 0 | 0 |
| macdown | 27 | 27 | 0 | 0 | 0 |
| steam | 23 | 23 | 0 | 0 | 0 |
| cc-switch | 21 | 21 | 0 | 0 | 0 |
| vagrant | 18 | 18 | 0 | 0 | 0 |
| postman | 15 | 15 | 0 | 0 | 0 |
| brave-browser | 14 | 14 | 0 | 0 | 0 |
| tigervnc | 14 | 14 | 0 | 0 | 0 |
| headlamp | 14 | 14 | 0 | 0 | 0 |
| google-chrome | 13 | 13 | 0 | 0 | 0 |
| appcleaner | 12 | 12 | 0 | 0 | 0 |
| scroll-reverser | 12 | 12 | 0 | 0 | 0 |
| alacritty | 11 | 11 | 0 | 0 | 0 |
| antigravity | 8 | 0 | 0 | 8 | 0 |
| insomnia | 7 | 7 | 0 | 0 | 0 |
| t3-code | 7 | 7 | 0 | 0 | 0 |
| notunes | 7 | 0 | 0 | 7 | 0 |
| background-music | 6 | 6 | 0 | 0 | 0 |
| keepingyouawake | 5 | 5 | 0 | 0 | 0 |
| macfuse | 2 | 2 | 0 | 0 | 0 |
| wezterm | 2 | 2 | 0 | 0 | 0 |
| signal | 2 | 2 | 0 | 0 | 0 |
| blackhole-2ch | 1 | 1 | 0 | 0 | 0 |
| caffeine | 1 | 1 | 0 | 0 | 0 |
| blackhole-16ch | 1 | 1 | 0 | 0 | 0 |
| helium-browser | 1 | 1 | 0 | 0 | 0 |
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
