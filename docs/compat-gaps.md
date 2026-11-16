# Lacunas de compatibilidade no corpus macOS

Gerado por `python3 tools/corpus/harness.py gaps` com as medidas atuais de `tools/corpus/varrer-tudo.sh`.
libSystem medido: main ce6dc05c (merge #392), copiado de build do Ricardo 2026-11-16_01:41.
O disco é o da varredura: libunwind, libc++, libswift*, libz, libcurl e, com o artefato do CI, os frameworks do
mOS. Os baldes libc++ e Swift só contam o que nenhuma dessas exporta.
Binários não estão no git. URLs e sha256 ficam em `tools/corpus/manifest.json`.

Apps com pelo menos uma imagem Mach-O x86_64: **106**.

## Símbolos que faltam em mais apps

| símbolo | apps |
|---|---:|
| `_$sSS10FoundationE19_bridgeToObjectiveCSo8NSStringCyF` | 35 |
| `_$sSS10FoundationE36_unconditionallyBridgeFromObjectiveCySSSo8NSStringCSgFZ` | 34 |
| `_$s10Foundation3URLV36_unconditionallyBridgeFromObjectiveCyACSo5NSURLCSgFZ` | 33 |
| `_$s10Foundation3URLVMa` | 33 |
| `_$s10Foundation3URLVMn` | 33 |
| `_$s10Foundation22_convertNSErrorToErrorys0E0_pSo0C0CSgF` | 32 |
| `_$s10Foundation3URLV19_bridgeToObjectiveCSo5NSURLCyF` | 32 |
| `_$s10Foundation4DataV19_bridgeToObjectiveCSo6NSDataCyF` | 32 |
| `_$s10Foundation4DataV36_unconditionallyBridgeFromObjectiveCyACSo6NSDataCSgFZ` | 32 |
| `_$sSa10FoundationE19_bridgeToObjectiveCSo7NSArrayCyF` | 32 |
| `_$ss5ErrorP10FoundationE20localizedDescriptionSSvg` | 32 |
| `_$sSD10FoundationE19_bridgeToObjectiveCSo12NSDictionaryCyF` | 31 |
| `_$sSa10FoundationE36_unconditionallyBridgeFromObjectiveCySayxGSo7NSArrayCSgFZ` | 31 |
| `_$s10Foundation3URLV4pathSSvg` | 30 |
| `_CFBundleGetBundleWithIdentifier` | 30 |
| `_OBJC_CLASS_$_WKUserScript` | 30 |
| `_OBJC_CLASS_$_WebView` | 30 |
| `_$s10Foundation11JSONDecoderC6decode_4fromxxm_AA4DataVtKSeRzlFTj` | 29 |
| `_$s10Foundation11JSONDecoderCACycfc` | 29 |
| `_$s10Foundation11JSONDecoderCMa` | 29 |
| `_$s10Foundation13__DataStorageC6_bytesSvSgvg` | 29 |
| `_$s10Foundation13__DataStorageC7_offsetSivg` | 29 |
| `_$s10Foundation4DataVN` | 29 |
| `_$sSD10FoundationE36_unconditionallyBridgeFromObjectiveCySDyxq_GSo12NSDictionaryCSgFZ` | 29 |
| `_$sSS10FoundationE8EncodingV4utf8ACvgZ` | 29 |
| `_$sSS10FoundationE8EncodingVMa` | 29 |
| `_$s10Foundation12NotificationV36_unconditionallyBridgeFromObjectiveCyACSo14NSNotificationCSgFZ` | 28 |
| `_$s10Foundation12NotificationVMa` | 28 |
| `_$s10Foundation4DateVMa` | 28 |
| `_$sSS10FoundationE26_forceBridgeFromObjectiveC_6resultySo8NSStringC_SSSgztFZ` | 28 |
| `_$sSS10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo8NSStringC_SSSgztFZ` | 28 |
| `_$s10Foundation11JSONEncoderC6encodeyAA4DataVxKSERzlFTj` | 27 |
| `_$s10Foundation11JSONEncoderCACycfc` | 27 |
| `_$s10Foundation11JSONEncoderCMa` | 27 |
| `_$s10Foundation13__DataStorageC7_lengthSivg` | 27 |
| `_$s10Foundation22_convertErrorToNSErrorySo0E0Cs0C0_pF` | 27 |
| `_$s10Foundation3URLV6stringACSgSSh_tcfC` | 27 |
| `_$s10Foundation4DataVMn` | 27 |
| `_$s10Foundation4DateV19_bridgeToObjectiveCSo6NSDateCyF` | 27 |
| `_$s10Foundation4UUIDVMa` | 27 |

## Por origem

### sistema

| símbolo | apps |
|---|---:|
| `_CFBundleGetBundleWithIdentifier` | 30 |
| `_OBJC_CLASS_$_WKUserScript` | 30 |
| `_OBJC_CLASS_$_WebView` | 30 |
| `_OBJC_CLASS_$_WKUserContentController` | 25 |
| `_CFStringGetSystemEncoding` | 24 |
| `_OBJC_CLASS_$_WebPreferences` | 24 |
| `_PasteboardCreate` | 22 |
| `_PasteboardSynchronize` | 22 |
| `_putenv` | 21 |
| `_OBJC_CLASS_$_NSFontPanel` | 20 |
| `_kCFLocaleLanguageCode` | 20 |
| `_kCFURLIsPackageKey` | 20 |
| `_CFURLEnumeratorGetNextURL` | 19 |
| `_TISCopyInputMethodKeyboardLayoutOverride` | 19 |
| `_kUTTypeFileURL` | 19 |

### libc++

| símbolo | apps |
|---|---:|
| `__ZdlPvSt19__type_descriptor_t` | 1 |

### Swift

| símbolo | apps |
|---|---:|
| `_$s22UniformTypeIdentifiers6UTTypeVMa` | 22 |
| `_$s22UniformTypeIdentifiers6UTTypeVMn` | 21 |
| `_$sSo13os_log_type_ta0A0E5errorABvgZ` | 20 |
| `_$s2os0A4_log_3dso0B0__ySo0a1_B7_type_ta_SVSo03OS_a1_B0Cs12StaticStringVs7CVarArg_pdtF` | 18 |
| `_$sSo13os_log_type_ta0A0E5faultABvgZ` | 18 |
| `_$s2os6LoggerV9logObjectSo03OS_a1_C0Cvg` | 17 |
| `_$s2os6LoggerV9subsystem8categoryACSS_SStcfC` | 17 |
| `_$s2os6LoggerVMa` | 17 |
| `_$sSS10FoundationE19_bridgeToObjectiveCSo8NSStringCyF` | 17 |
| `_$sSo13os_log_type_ta0A0E4infoABvgZ` | 17 |
| `_$s10Foundation4DataV36_unconditionallyBridgeFromObjectiveCyACSo6NSDataCSgFZ` | 16 |
| `_$sSS10FoundationE36_unconditionallyBridgeFromObjectiveCySSSo8NSStringCSgFZ` | 16 |
| `_$s10Foundation3URLV36_unconditionallyBridgeFromObjectiveCyACSo5NSURLCSgFZ` | 15 |
| `_$s10Foundation3URLV4pathSSvg` | 15 |
| `_$s10Foundation3URLVMa` | 15 |

### unwind

| símbolo | apps |
|---|---:|
| — | 0 |

## Por framework

### Foundation.framework

| símbolo | apps |
|---|---:|
| `_$s10Foundation11JSONDecoderC6decode_4fromxxm_AA4DataVtKSeRzlFTj` | 18 |
| `_$s10Foundation11JSONDecoderCACycfc` | 18 |
| `_$s10Foundation11JSONDecoderCMa` | 18 |
| `_$s10Foundation22_convertNSErrorToErrorys0E0_pSo0C0CSgF` | 18 |
| `_$s10Foundation3URLV19_bridgeToObjectiveCSo5NSURLCyF` | 18 |
| `_$s10Foundation3URLV36_unconditionallyBridgeFromObjectiveCyACSo5NSURLCSgFZ` | 18 |
| `_$s10Foundation3URLVMa` | 18 |
| `_$s10Foundation3URLVMn` | 18 |

### SwiftUI.framework

| símbolo | apps |
|---|---:|
| `_$s7SwiftUI11ControlSizeO5largeyA2CmFWC` | 13 |
| `_$s7SwiftUI17VerticalAlignmentV3topACvgZ` | 13 |
| `_$s7SwiftUI26NSViewRepresentableContextV11coordinator11CoordinatorQzvg` | 13 |
| `_$s7SwiftUI6IDViewVMn` | 13 |
| `_$s7SwiftUI6VStackVMa` | 13 |
| `_$s7SwiftUI26NSViewRepresentableContextVMn` | 12 |
| `_$s5Value7SwiftUI13PreferenceKeyPTl` | 11 |
| `_$s7SwiftUI13PreferenceKeyMp` | 11 |

### libswiftFoundation.dylib

| símbolo | apps |
|---|---:|
| `_$sSS10FoundationE19_bridgeToObjectiveCSo8NSStringCyF` | 17 |
| `_$s10Foundation4DataV36_unconditionallyBridgeFromObjectiveCyACSo6NSDataCSgFZ` | 16 |
| `_$sSS10FoundationE36_unconditionallyBridgeFromObjectiveCySSSo8NSStringCSgFZ` | 16 |
| `_$s10Foundation3URLV36_unconditionallyBridgeFromObjectiveCyACSo5NSURLCSgFZ` | 15 |
| `_$s10Foundation3URLV4pathSSvg` | 15 |
| `_$s10Foundation3URLVMa` | 15 |
| `_$s10Foundation3URLVMn` | 15 |
| `_$s10Foundation4DataV19_bridgeToObjectiveCSo6NSDataCyF` | 15 |

### Combine.framework

| símbolo | apps |
|---|---:|
| `_$s7Combine10PublishersO3MapVMn` | 15 |
| `_$s7Combine10PublishersO3MapVy_xq_GAA9PublisherAAMc` | 15 |
| `_$s7Combine9PublisherPAAE3mapyAA10PublishersO3MapVy_xqd__Gqd__6OutputQzclF` | 15 |
| `_$s7Combine10PublishersO9MergeManyVy_xGAA9PublisherAAMc` | 12 |
| `_$s7Combine11CancellableMp` | 12 |
| `_$s6Output7Combine9PublisherPTl` | 11 |
| `_$s7Combine06CustomA21IdentifierConvertibleMp` | 11 |
| `_$s7Combine06CustomA21IdentifierConvertibleP07combineC0AA0aC0VvgTq` | 11 |

### CoreFoundation.framework

| símbolo | apps |
|---|---:|
| `_CFBundleGetBundleWithIdentifier` | 29 |
| `_CFStringGetSystemEncoding` | 24 |
| `_kCFLocaleLanguageCode` | 20 |
| `_kCFURLIsPackageKey` | 20 |
| `_CFURLEnumeratorGetNextURL` | 19 |
| `_CFSocketCreateWithNative` | 17 |
| `_CFSocketGetSocketFlags` | 16 |
| `_kCFLocaleCurrencyCode` | 16 |

### libSystem.B.dylib

| símbolo | apps |
|---|---:|
| `_putenv` | 21 |
| `_getprotobyname` | 18 |
| `_getloadavg` | 14 |
| `_pthread_attr_getschedpolicy` | 14 |
| `_OSAtomicAdd32Barrier` | 13 |
| `__os_activity_create` | 13 |
| `_os_activity_scope_enter` | 13 |
| `_os_activity_scope_leave` | 13 |

### AppIntents.framework

| símbolo | apps |
|---|---:|
| `_$s10AppIntents09_SupportsA12DependenciesMp` | 6 |
| `_$s10AppIntents0A6IntentMp` | 6 |
| `_$s10AppIntents0A6IntentP04openA7WhenRunSbvgZTq` | 6 |
| `_$s10AppIntents0A6IntentP11descriptionAA0C11DescriptionVSgvgZTq` | 6 |
| `_$s10AppIntents0A6IntentP13PerformResultAC_AA0cE0Tn` | 6 |
| `_$s10AppIntents0A6IntentP14SummaryContentAC_AA09ParameterD0Tn` | 6 |
| `_$s10AppIntents0A6IntentP16parameterSummary0E7ContentQzvgZTq` | 6 |
| `_$s10AppIntents0A6IntentP20authenticationPolicyAA0c14AuthenticationE0OvgZTq` | 6 |

### AppKit.framework

| símbolo | apps |
|---|---:|
| `_OBJC_CLASS_$_NSFontPanel` | 20 |
| `_OBJC_CLASS_$_NSPageLayout` | 17 |
| `_NSAccessibilityUnignoredChildrenForOnlyChild` | 16 |
| `_NSFileContentsPboardType` | 15 |
| `_OBJC_CLASS_$_NSStepperCell` | 13 |
| `_OBJC_CLASS_$_NSPrintOperation` | 12 |
| `_OBJC_CLASS_$_NSViewAnimation` | 12 |
| `_NSControlTintDidChangeNotification` | 10 |

### libncurses.5.4.dylib

| símbolo | apps |
|---|---:|
| `_setupterm` | 7 |
| `_tigetnum` | 7 |
| `_tigetflag` | 6 |
| `_tigetstr` | 6 |
| `_COLORS` | 5 |
| `_COLOR_PAIRS` | 5 |
| `_COLS` | 5 |
| `_LINES` | 5 |

### Security.framework

| símbolo | apps |
|---|---:|
| `_kSecAttrServer` | 16 |
| `_kSecAttrProtocol` | 13 |
| `_SSLSetDiffieHellmanParams` | 12 |
| `_kSecImportExportKeychain` | 11 |
| `_SecIdentitySearchCopyNext` | 8 |
| `_SecIdentitySearchCreate` | 8 |
| `_kSecAttrProtocolFTPProxy` | 8 |
| `_kSecAttrProtocolHTTPProxy` | 8 |

### CoreServices.framework

| símbolo | apps |
|---|---:|
| `_kUTTypeFileURL` | 19 |
| `_GetIconRef` | 17 |
| `_kUTTagClassNSPboardType` | 16 |
| `_ReleaseIconRef` | 15 |
| `_UCCreateCollator` | 15 |
| `_UCGetCollationKey` | 14 |
| `_FSFindFolder` | 12 |
| `_LocaleRefFromLocaleString` | 11 |

### CryptoKit.framework

| símbolo | apps |
|---|---:|
| `_$s9CryptoKit12HashFunctionP6update13bufferPointerySW_tFTj` | 14 |
| `_$s9CryptoKit12HashFunctionP8finalize6DigestQzyFTj` | 14 |
| `_$s9CryptoKit12HashFunctionPxycfCTj` | 14 |
| `_$s9CryptoKit12SHA256DigestVMa` | 13 |
| `_$s9CryptoKit6SHA256VAA12HashFunctionAAMc` | 13 |
| `_$s9CryptoKit6SHA256VMa` | 13 |
| `_$s9CryptoKit12SHA256DigestVSTAAMc` | 11 |
| `_$s9CryptoKit12SymmetricKeyV4dataACx_tc10Foundation15ContiguousBytesRzlufC` | 10 |

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

### ApplicationServices.framework

| símbolo | apps |
|---|---:|
| `_PasteboardCreate` | 22 |
| `_PasteboardSynchronize` | 22 |
| `_PasteboardCopyItemFlavorData` | 14 |
| `_PasteboardCopyItemFlavors` | 14 |
| `_PasteboardGetItemCount` | 14 |
| `_PasteboardGetItemIdentifier` | 14 |
| `_PMRetain` | 13 |
| `_PasteboardClear` | 13 |

### CoreGraphics.framework

| símbolo | apps |
|---|---:|
| `_kCGColorWhite` | 16 |
| `_CGDisplayCopyAllDisplayModes` | 15 |
| `_CGDisplayMoveCursorToPoint` | 14 |
| `_CGDisplaySetDisplayMode` | 13 |
| `_CGEventTapCreateForPid` | 13 |
| `_CGDisplayModeCopyPixelEncoding` | 12 |
| `_CGAcquireDisplayFadeReservation` | 11 |
| `_CGDisplayCapture` | 11 |

### libswiftUniformTypeIdentifiers.dylib

| símbolo | apps |
|---|---:|
| `_$s22UniformTypeIdentifiers6UTTypeVMa` | 22 |
| `_$s22UniformTypeIdentifiers6UTTypeVMn` | 21 |
| `_$s22UniformTypeIdentifiers6UTTypeV10identifierSSvg` | 15 |
| `_$s22UniformTypeIdentifiers6UTTypeV4dataACvgZ` | 14 |
| `_$s22UniformTypeIdentifiers6UTTypeV17filenameExtension12conformingToACSgSS_ACtcfC` | 13 |
| `_$s22UniformTypeIdentifiers6UTTypeV19_bridgeToObjectiveCSoABCyF` | 13 |
| `_$s22UniformTypeIdentifiers6UTTypeVyACSgSScfC` | 12 |
| `_$s22UniformTypeIdentifiers6UTTypeV8conforms2toSbAC_tF` | 11 |

### Carbon.framework

| símbolo | apps |
|---|---:|
| `_TISCopyInputMethodKeyboardLayoutOverride` | 19 |
| `_TISCreateInputSourceList` | 17 |
| `_GetEventDispatcherTarget` | 15 |
| `_kTISPropertyInputSourceIsASCIICapable` | 14 |
| `_kTISPropertyInputSourceType` | 13 |
| `_TISSelectInputSource` | 12 |
| `_kTISTypeKeyboardLayout` | 12 |
| `_kTISTypeKeyboardInputMode` | 11 |

### AVFoundation.framework

| símbolo | apps |
|---|---:|
| `_OBJC_CLASS_$_AVPlayerLayer` | 8 |
| `_AVCaptureSessionPresetLow` | 6 |
| `_OBJC_CLASS_$_AVMutableMetadataItem` | 5 |
| `_OBJC_CLASS_$_AVPlayerItemVideoOutput` | 5 |
| `_AVFileTypeWAVE` | 4 |
| `_OBJC_CLASS_$_AVAssetWriterInputPixelBufferAdaptor` | 4 |
| `_OBJC_CLASS_$_AVCaptureStillImageOutput` | 4 |
| `_AVFileTypeMPEGLayer3` | 3 |

### libswiftUIKit.dylib

| símbolo | apps |
|---|---:|
| `_$s10Foundation15AttributeScopesO5UIKitE0D10AttributesV015ForegroundColorB0OAA19AttributedStringKeyADMc` | 1 |
| `_$s10Foundation15AttributeScopesO5UIKitE0D10AttributesV015ForegroundColorB0OMn` | 1 |
| `_$s10Foundation15AttributeScopesO5UIKitE0D10AttributesV015ForegroundColorB0ON` | 1 |
| `_$s10Foundation15AttributeScopesO5UIKitE0D10AttributesV04FontB0OAA19AttributedStringKeyADMc` | 1 |
| `_$s10Foundation15AttributeScopesO5UIKitE0D10AttributesV04FontB0OMn` | 1 |
| `_$s10Foundation15AttributeScopesO5UIKitE0D10AttributesV04FontB0ON` | 1 |
| `_$s10Foundation15AttributeScopesO5UIKitE0D10AttributesV15foregroundColorAF010ForegroundgB0Ovg` | 1 |
| `_$s10Foundation15AttributeScopesO5UIKitE0D10AttributesV15foregroundColorAF010ForegroundgB0OvpMV` | 1 |

### libswiftos.dylib

| símbolo | apps |
|---|---:|
| `_$sSo13os_log_type_ta0A0E5errorABvgZ` | 20 |
| `_$s2os0A4_log_3dso0B0__ySo0a1_B7_type_ta_SVSo03OS_a1_B0Cs12StaticStringVs7CVarArg_pdtF` | 18 |
| `_$sSo13os_log_type_ta0A0E5faultABvgZ` | 18 |
| `_$s2os6LoggerV9logObjectSo03OS_a1_C0Cvg` | 17 |
| `_$s2os6LoggerV9subsystem8categoryACSS_SStcfC` | 17 |
| `_$s2os6LoggerVMa` | 17 |
| `_$sSo13os_log_type_ta0A0E4infoABvgZ` | 17 |
| `_$sSo13os_log_type_ta0A0E5debugABvgZ` | 15 |

### libcups.2.dylib

| símbolo | apps |
|---|---:|
| `_ppdClose` | 12 |
| `_ppdFindOption` | 12 |
| `_ppdFindMarkedChoice` | 10 |
| `_cupsGetDests2` | 9 |
| `_ppdOpenFile` | 9 |
| `_httpGetHostname` | 8 |
| `_cupsConnectDest` | 6 |
| `_cupsGetDestMediaByIndex` | 6 |

### libicucore.A.dylib

| símbolo | apps |
|---|---:|
| `_u_strToLower` | 4 |
| `_u_strToUpper` | 4 |
| `_ubrk_close` | 4 |
| `_ubrk_following` | 4 |
| `_ubrk_open` | 4 |
| `_ubrk_setText` | 4 |
| `_unorm2_getNFCInstance` | 4 |
| `_unorm2_normalize` | 4 |

### libedit.3.dylib

| símbolo | apps |
|---|---:|
| `_add_history` | 7 |
| `_history_truncate_file` | 6 |
| `_read_history` | 6 |
| `_rl_attempted_completion_function` | 6 |
| `_rl_attempted_completion_over` | 6 |
| `_rl_completion_append_character` | 6 |
| `_rl_completion_matches` | 6 |
| `_write_history` | 6 |

### IOKit.framework

| símbolo | apps |
|---|---:|
| `_IODisplayCreateInfoDictionary` | 15 |
| `_IOIteratorIsValid` | 11 |
| `_IOHIDDeviceGetValue` | 10 |
| `_IOHIDElementGetTypeID` | 9 |
| `_IOObjectIsEqualTo` | 9 |
| `_IODispatchCalloutFromMessage` | 8 |
| `_IOHIDElementGetChildren` | 8 |
| `_IOHIDElementGetCookie` | 8 |

### StoreKit.framework

| símbolo | apps |
|---|---:|
| `_$s8StoreKit0aB5ErrorO07networkC0yAC10Foundation8URLErrorVcACmFWC` | 2 |
| `_$s8StoreKit0aB5ErrorO13userCancelledyA2CmFWC` | 2 |
| `_$s8StoreKit0aB5ErrorOMa` | 2 |
| `_$s8StoreKit0aB5ErrorOs0C0AAMc` | 2 |
| `_$s8StoreKit11TransactionV6finishyyYaF` | 2 |
| `_$s8StoreKit11TransactionV6finishyyYaFTu` | 2 |
| `_$s8StoreKit11TransactionVMa` | 2 |
| `_$s8StoreKit11TransactionVMn` | 2 |

## Contagem distinta por app

| app | distintos | sistema | libc++ | Swift | unwind |
|---|---:|---:|---:|---:|---:|
| whatsapp | 2611 | 1262 | 0 | 1349 | 0 |
| cmux | 2134 | 2031 | 0 | 103 | 0 |
| coteditor | 1878 | 1769 | 0 | 109 | 0 |
| raycast | 1863 | 1579 | 0 | 284 | 0 |
| orbstack | 1424 | 1382 | 0 | 42 | 0 |
| tailscale-app | 1191 | 1154 | 0 | 37 | 0 |
| utm | 964 | 537 | 1 | 426 | 0 |
| betterdisplay | 916 | 869 | 0 | 47 | 0 |
| iterm2 | 848 | 804 | 0 | 44 | 0 |
| hammerspoon | 836 | 836 | 0 | 0 | 0 |
| clipy | 823 | 804 | 0 | 19 | 0 |
| chatgpt | 768 | 640 | 0 | 128 | 0 |
| maccy | 765 | 742 | 0 | 23 | 0 |
| ghostty | 663 | 630 | 0 | 33 | 0 |
| sequel-ace | 620 | 594 | 0 | 26 | 0 |
| zoom | 615 | 335 | 0 | 280 | 0 |
| jordanbaird-ice | 547 | 532 | 0 | 15 | 0 |
| iina | 535 | 154 | 0 | 381 | 0 |
| 1password | 500 | 343 | 0 | 157 | 0 |
| inkscape | 470 | 470 | 0 | 0 | 0 |
| deluge | 452 | 452 | 0 | 0 | 0 |
| krita | 449 | 432 | 0 | 17 | 0 |
| blender | 433 | 433 | 0 | 0 | 0 |
| linearmouse | 375 | 205 | 0 | 170 | 0 |
| shotcut | 363 | 363 | 0 | 0 | 0 |
| stats | 361 | 115 | 0 | 246 | 0 |
| kitty | 339 | 339 | 0 | 0 | 0 |
| claude | 314 | 80 | 0 | 234 | 0 |
| obs | 301 | 290 | 0 | 11 | 0 |
| kicad | 297 | 297 | 0 | 0 | 0 |
| wireshark-app | 272 | 272 | 0 | 0 | 0 |
| localsend | 265 | 161 | 0 | 104 | 0 |
| alt-tab | 255 | 58 | 0 | 197 | 0 |
| xquartz | 227 | 227 | 0 | 0 | 0 |
| bitwarden | 223 | 219 | 0 | 4 | 0 |
| lmms | 206 | 206 | 0 | 0 | 0 |
| rectangle | 199 | 183 | 0 | 16 | 0 |
| android-studio | 192 | 192 | 0 | 0 | 0 |
| monitorcontrol | 184 | 184 | 0 | 0 | 0 |
| libreoffice | 180 | 180 | 0 | 0 | 0 |
| opencode-desktop | 176 | 176 | 0 | 0 | 0 |
| openscad | 175 | 175 | 0 | 0 | 0 |
| mos | 156 | 156 | 0 | 0 | 0 |
| thunderbird | 152 | 152 | 0 | 0 | 0 |
| webtorrent | 152 | 152 | 0 | 0 | 0 |
| pencil | 141 | 141 | 0 | 0 | 0 |
| vnote | 139 | 139 | 0 | 0 | 0 |
| musescore | 136 | 136 | 0 | 0 | 0 |
| hex-fiend | 126 | 79 | 0 | 47 | 0 |
| audacity | 124 | 124 | 0 | 0 | 0 |
| cyberduck | 123 | 122 | 0 | 1 | 0 |
| vlc | 120 | 120 | 0 | 0 | 0 |
| firefox | 119 | 119 | 0 | 0 | 0 |
| floorp | 118 | 118 | 0 | 0 | 0 |
| zen | 118 | 118 | 0 | 0 | 0 |
| mullvad-browser | 117 | 117 | 0 | 0 | 0 |
| tor-browser | 115 | 115 | 0 | 0 | 0 |
| visual-studio-code | 110 | 91 | 0 | 19 | 0 |
| db-browser-for-sqlite | 110 | 110 | 0 | 0 | 0 |
| gimp | 109 | 109 | 0 | 0 | 0 |
| prusaslicer | 108 | 108 | 0 | 0 | 0 |
| github | 106 | 87 | 0 | 19 | 0 |
| dbeaver-community | 105 | 105 | 0 | 0 | 0 |
| keepassxc | 103 | 103 | 0 | 0 | 0 |
| dolphin | 97 | 97 | 0 | 0 | 0 |
| drawio | 95 | 28 | 0 | 67 | 0 |
| docker-desktop | 87 | 87 | 0 | 0 | 0 |
| qbittorrent | 80 | 80 | 0 | 0 | 0 |
| obsidian | 77 | 3 | 0 | 74 | 0 |
| spotify | 77 | 77 | 0 | 0 | 0 |
| cursor | 64 | 0 | 0 | 64 | 0 |
| tic80 | 58 | 58 | 0 | 0 | 0 |
| git-credential-manager | 46 | 46 | 0 | 0 | 0 |
| zed | 45 | 45 | 0 | 0 | 0 |
| microsoft-edge | 44 | 44 | 0 | 0 | 0 |
| openrct2 | 42 | 42 | 0 | 0 | 0 |
| love | 40 | 40 | 0 | 0 | 0 |
| flycut | 38 | 38 | 0 | 0 | 0 |
| transmission | 36 | 36 | 0 | 0 | 0 |
| godot | 36 | 36 | 0 | 0 | 0 |
| pixelorama | 36 | 36 | 0 | 0 | 0 |
| hiddenbar | 35 | 35 | 0 | 0 | 0 |
| bruno | 33 | 33 | 0 | 0 | 0 |
| ollama-app | 32 | 32 | 0 | 0 | 0 |
| openttd | 30 | 30 | 0 | 0 | 0 |
| sublime-text | 28 | 28 | 0 | 0 | 0 |
| macdown | 27 | 27 | 0 | 0 | 0 |
| cc-switch | 21 | 21 | 0 | 0 | 0 |
| postman | 15 | 15 | 0 | 0 | 0 |
| brave-browser | 14 | 14 | 0 | 0 | 0 |
| google-chrome | 13 | 13 | 0 | 0 | 0 |
| appcleaner | 12 | 12 | 0 | 0 | 0 |
| alacritty | 11 | 11 | 0 | 0 | 0 |
| antigravity | 8 | 0 | 0 | 8 | 0 |
| insomnia | 7 | 7 | 0 | 0 | 0 |
| keepingyouawake | 5 | 5 | 0 | 0 | 0 |
| macfuse | 2 | 2 | 0 | 0 | 0 |
| blackhole-2ch | 1 | 1 | 0 | 0 | 0 |
| element | 0 | 0 | 0 | 0 | 0 |
| finicky | 0 | 0 | 0 | 0 | 0 |
| vscodium | 0 | 0 | 0 | 0 | 0 |
| dbgate | 0 | 0 | 0 | 0 | 0 |
| session-manager-plugin | 0 | 0 | 0 | 0 | 0 |
| discord | 0 | 0 | 0 | 0 | 0 |
| slack | 0 | 0 | 0 | 0 | 0 |
| notion | 0 | 0 | 0 | 0 | 0 |
