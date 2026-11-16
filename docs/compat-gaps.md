# Lacunas de compatibilidade no corpus macOS

Gerado por `python3 tools/corpus/harness.py gaps` com as medidas atuais de `tools/corpus/varrer-tudo.sh`.
libSystem medido: main ce6dc05c (merge #392), copiado de build do Ricardo 2026-11-16_01:41.
O disco é o da varredura: libunwind, libc++, libswift*, libz, libcurl e, com o artefato do CI, os frameworks do
mOS. Os baldes libc++ e Swift só contam o que nenhuma dessas exporta.
Binários não estão no git. URLs e sha256 ficam em `tools/corpus/manifest.json`.

Apps com pelo menos uma imagem Mach-O x86_64: **10**.

## Símbolos que faltam em mais apps

| símbolo | apps |
|---|---:|
| `_OBJC_CLASS_$_WKUserContentController` | 5 |
| `_OBJC_CLASS_$_WKUserScript` | 5 |
| `_OBJC_CLASS_$_WebView` | 5 |
| `_$s10Foundation11JSONDecoderC6decode_4fromxxm_AA4DataVtKSeRzlFTj` | 4 |
| `_$s10Foundation11JSONDecoderCACycfc` | 4 |
| `_$s10Foundation11JSONDecoderCMa` | 4 |
| `_$s10Foundation11JSONEncoderC6encodeyAA4DataVxKSERzlFTj` | 4 |
| `_$s10Foundation11JSONEncoderCACycfc` | 4 |
| `_$s10Foundation11JSONEncoderCMa` | 4 |
| `_$s10Foundation12NotificationV36_unconditionallyBridgeFromObjectiveCyACSo14NSNotificationCSgFZ` | 4 |
| `_$s10Foundation12NotificationVMa` | 4 |
| `_$s10Foundation22_convertNSErrorToErrorys0E0_pSo0C0CSgF` | 4 |
| `_$s10Foundation3URLV19_bridgeToObjectiveCSo5NSURLCyF` | 4 |
| `_$s10Foundation3URLV36_unconditionallyBridgeFromObjectiveCyACSo5NSURLCSgFZ` | 4 |
| `_$s10Foundation3URLV6stringACSgSSh_tcfC` | 4 |
| `_$s10Foundation3URLVMa` | 4 |
| `_$s10Foundation3URLVMn` | 4 |
| `_$s10Foundation4DataV19_bridgeToObjectiveCSo6NSDataCyF` | 4 |
| `_$s10Foundation4DateV19_bridgeToObjectiveCSo6NSDateCyF` | 4 |
| `_$s10Foundation4DateVACycfC` | 4 |
| `_$s10Foundation4DateVMa` | 4 |
| `_$s10Foundation4UUIDVACycfC` | 4 |
| `_$s10Foundation4UUIDVMa` | 4 |
| `_$sSD10FoundationE19_bridgeToObjectiveCSo12NSDictionaryCyF` | 4 |
| `_$sSD10FoundationE36_unconditionallyBridgeFromObjectiveCySDyxq_GSo12NSDictionaryCSgFZ` | 4 |
| `_$sSS10FoundationE19_bridgeToObjectiveCSo8NSStringCyF` | 4 |
| `_$sSS10FoundationE36_unconditionallyBridgeFromObjectiveCySSSo8NSStringCSgFZ` | 4 |
| `_$sSa10FoundationE19_bridgeToObjectiveCSo7NSArrayCyF` | 4 |
| `_$sSa10FoundationE36_unconditionallyBridgeFromObjectiveCySayxGSo7NSArrayCSgFZ` | 4 |
| `_$ss5ErrorP10FoundationE20localizedDescriptionSSvg` | 4 |
| `_GetEventDispatcherTarget` | 4 |
| `_getprotobyname` | 4 |
| `_$s10Foundation12NotificationV6objectypSgvg` | 3 |
| `_$s10Foundation12NotificationV8userInfoSDys11AnyHashableVypGSgvg` | 3 |
| `_$s10Foundation12URLQueryItemV4nameSSvg` | 3 |
| `_$s10Foundation12URLQueryItemV5valueSSSgvg` | 3 |
| `_$s10Foundation12URLQueryItemVMa` | 3 |
| `_$s10Foundation13URLComponentsV10queryItemsSayAA12URLQueryItemVGSgvg` | 3 |
| `_$s10Foundation13URLComponentsV3url23resolvingAgainstBaseURLACSgAA0G0Vh_SbtcfC` | 3 |
| `_$s10Foundation13URLComponentsVMa` | 3 |

## Por origem

### sistema

| símbolo | apps |
|---|---:|
| `_OBJC_CLASS_$_WKUserContentController` | 5 |
| `_OBJC_CLASS_$_WKUserScript` | 5 |
| `_OBJC_CLASS_$_WebView` | 5 |
| `_GetEventDispatcherTarget` | 4 |
| `_getprotobyname` | 4 |
| `_$s10Foundation11JSONDecoderC6decode_4fromxxm_AA4DataVtKSeRzlFTj` | 3 |
| `_$s10Foundation11JSONDecoderCACycfc` | 3 |
| `_$s10Foundation11JSONDecoderCMa` | 3 |
| `_$s10Foundation11JSONEncoderC6encodeyAA4DataVxKSERzlFTj` | 3 |
| `_$s10Foundation11JSONEncoderCACycfc` | 3 |
| `_$s10Foundation11JSONEncoderCMa` | 3 |
| `_$s10Foundation12NotificationV36_unconditionallyBridgeFromObjectiveCyACSo14NSNotificationCSgFZ` | 3 |
| `_$s10Foundation12NotificationVMa` | 3 |
| `_$s10Foundation22_convertNSErrorToErrorys0E0_pSo0C0CSgF` | 3 |
| `_$s10Foundation3URLV19_bridgeToObjectiveCSo5NSURLCyF` | 3 |

### libc++

| símbolo | apps |
|---|---:|
| — | 0 |

### Swift

| símbolo | apps |
|---|---:|
| `_$s22UniformTypeIdentifiers6UTTypeVMa` | 3 |
| `_$s22UniformTypeIdentifiers6UTTypeVMn` | 3 |
| `_$s22UniformTypeIdentifiers6UTTypeV10identifierSSvg` | 2 |
| `_$s22UniformTypeIdentifiers6UTTypeV11applicationACvgZ` | 2 |
| `_$s22UniformTypeIdentifiers6UTTypeV19_bridgeToObjectiveCSoABCyF` | 2 |
| `_$sSo7CGPointV12CoreGraphicsE16debugDescriptionSSvg` | 2 |
| `_$sSo9OS_os_logC0B0E7defaultABvgZ` | 2 |
| `_$s10Foundation10CocoaErrorV03_nsC0So7NSErrorCvg` | 1 |
| `_$s10Foundation10CocoaErrorV4CodeV19fileReadCorruptFileAEvgZ` | 1 |
| `_$s10Foundation10CocoaErrorV4CodeVMa` | 1 |
| `_$s10Foundation10CocoaErrorVAA21_BridgedStoredNSErrorAAMc` | 1 |
| `_$s10Foundation10CocoaErrorVMa` | 1 |
| `_$s10Foundation10NSNotFoundSivg` | 1 |
| `_$s10Foundation10URLRequestV10httpMethodSSSgvs` | 1 |
| `_$s10Foundation10URLRequestV19_bridgeToObjectiveCSo12NSURLRequestCyF` | 1 |

### unwind

| símbolo | apps |
|---|---:|
| — | 0 |

## Por framework

### Foundation.framework

| símbolo | apps |
|---|---:|
| `_$s10Foundation11JSONDecoderC6decode_4fromxxm_AA4DataVtKSeRzlFTj` | 3 |
| `_$s10Foundation11JSONDecoderCACycfc` | 3 |
| `_$s10Foundation11JSONDecoderCMa` | 3 |
| `_$s10Foundation11JSONEncoderC6encodeyAA4DataVxKSERzlFTj` | 3 |
| `_$s10Foundation11JSONEncoderCACycfc` | 3 |
| `_$s10Foundation11JSONEncoderCMa` | 3 |
| `_$s10Foundation12NotificationV36_unconditionallyBridgeFromObjectiveCyACSo14NSNotificationCSgFZ` | 3 |
| `_$s10Foundation12NotificationVMa` | 3 |

### libswiftFoundation.dylib

| símbolo | apps |
|---|---:|
| `_$s10Foundation10CocoaErrorV03_nsC0So7NSErrorCvg` | 1 |
| `_$s10Foundation10CocoaErrorV4CodeV19fileReadCorruptFileAEvgZ` | 1 |
| `_$s10Foundation10CocoaErrorV4CodeVMa` | 1 |
| `_$s10Foundation10CocoaErrorVAA21_BridgedStoredNSErrorAAMc` | 1 |
| `_$s10Foundation10CocoaErrorVMa` | 1 |
| `_$s10Foundation10NSNotFoundSivg` | 1 |
| `_$s10Foundation10URLRequestV10httpMethodSSSgvs` | 1 |
| `_$s10Foundation10URLRequestV19_bridgeToObjectiveCSo12NSURLRequestCyF` | 1 |

### libSystem.B.dylib

| símbolo | apps |
|---|---:|
| `_getprotobyname` | 4 |
| `_getsectdatafromheader_64` | 3 |
| `_if_freenameindex` | 3 |
| `_if_nameindex` | 3 |
| `_putenv` | 3 |
| `_OSAtomicAdd32Barrier` | 2 |
| `_csqrt` | 2 |
| `_csqrtf` | 2 |

### libncurses.5.4.dylib

| símbolo | apps |
|---|---:|
| `_setupterm` | 2 |
| `_tigetflag` | 2 |
| `_tigetnum` | 2 |
| `_tigetstr` | 2 |
| `_BC` | 1 |
| `_COLORS` | 1 |
| `_COLOR_PAIRS` | 1 |
| `_COLS` | 1 |

### libcups.2.dylib

| símbolo | apps |
|---|---:|
| `_httpGetHostname` | 3 |
| `_cupsArrayFirst` | 2 |
| `_cupsDoAuthentication` | 2 |
| `_cupsEncryption` | 2 |
| `_cupsFreeOptions` | 2 |
| `_cupsLangDefault` | 2 |
| `_cupsLangFree` | 2 |
| `_cupsParseOptions` | 2 |

### CoreFoundation.framework

| símbolo | apps |
|---|---:|
| `_CFURLGetFSRef` | 2 |
| `_NSLocaleQuotationBeginDelimiterKey` | 2 |
| `_NSLocaleQuotationEndDelimiterKey` | 2 |
| `_NSURLFileResourceTypeKey` | 2 |
| `_NSURLVolumeSupportsCaseSensitiveNamesKey` | 2 |
| `_CFArrayReplaceValues` | 1 |
| `_CFBundleCopyExecutableArchitecturesForURL` | 1 |
| `_CFBundleCopyLocalizationsForPreferences` | 1 |

### AppKit.framework

| símbolo | apps |
|---|---:|
| `_NSControlTintDidChangeNotification` | 3 |
| `_NSFrameRectWithWidthUsingOperation` | 3 |
| `_OBJC_CLASS_$_NSTouch` | 3 |
| `_OBJC_CLASS_$_NSViewAnimation` | 3 |
| `_NSApplicationLoad` | 2 |
| `_NSDrawThreePartImage` | 2 |
| `_NSRectClip` | 2 |
| `_NSShowAnimationEffect` | 2 |

### SwiftUI.framework

| símbolo | apps |
|---|---:|
| `_$s7SwiftUI10GridLayoutVMn` | 1 |
| `_$s7SwiftUI10TransitionPAAE8combined4withQrqd___tAaBRd__lF` | 1 |
| `_$s7SwiftUI10TransitionPAAE8combined4withQrqd___tAaBRd__lFQOMQ` | 1 |
| `_$s7SwiftUI11ControlSizeO5largeyA2CmFWC` | 1 |
| `_$s7SwiftUI11StrokeStyleVN` | 1 |
| `_$s7SwiftUI14MoveTransitionV4edgeAcA4EdgeO_tcfC` | 1 |
| `_$s7SwiftUI14MoveTransitionVAA0D0AAMc` | 1 |
| `_$s7SwiftUI14MoveTransitionVMa` | 1 |

### Tcl.framework

| símbolo | apps |
|---|---:|
| `_TclBN_mp_clear` | 1 |
| `_TclBN_mp_init` | 1 |
| `_TclBN_mp_read_radix` | 1 |
| `_TclBN_mp_to_unsigned_bin_n` | 1 |
| `_TclBN_mp_unsigned_bin_size` | 1 |
| `_TclFreeObj` | 1 |
| `_Tcl_AddErrorInfo` | 1 |
| `_Tcl_AttemptAlloc` | 1 |

### Charts.framework

| símbolo | apps |
|---|---:|
| `_$s6Charts10ChartProxyV8position4forX12CoreGraphics7CGFloatVSgx_tAA9PlottableRzlF` | 1 |
| `_$s6Charts10ChartProxyVMa` | 1 |
| `_$s6Charts10ChartProxyVMn` | 1 |
| `_$s6Charts11BuilderPairVMn` | 1 |
| `_$s6Charts11BuilderPairVyxq_GAA12ChartContentA2aERzAaER_rlMc` | 1 |
| `_$s6Charts11BuilderPairVyxq_GAA8AxisMarkA2aERzAaER_rlMc` | 1 |
| `_$s6Charts12AxisGridLineV8centered6strokeACSbSg_7SwiftUI11StrokeStyleVSgtcfC` | 1 |
| `_$s6Charts12AxisGridLineVAA0B4MarkAAWP` | 1 |

### CryptoKit.framework

| símbolo | apps |
|---|---:|
| `_$s9CryptoKit03ChaC4PolyO4open_5using10Foundation4DataVAC9SealedBoxV_AA12SymmetricKeyVtKFZ` | 1 |
| `_$s9CryptoKit03ChaC4PolyO4seal_5using5nonceAC9SealedBoxVx_AA12SymmetricKeyVAC5NonceVSgtK10Foundation12DataProtocolRzlFZ` | 1 |
| `_$s9CryptoKit03ChaC4PolyO5NonceVMa` | 1 |
| `_$s9CryptoKit03ChaC4PolyO5NonceVMn` | 1 |
| `_$s9CryptoKit0aB5ErrorO22incorrectParameterSizeyA2CmFWC` | 1 |
| `_$s9CryptoKit0aB5ErrorOMa` | 1 |
| `_$s9CryptoKit0aB5ErrorOs0C0AAMc` | 1 |
| `_$s9CryptoKit10Curve25519O7SigningO10PrivateKeyV06publicF0AE06PublicF0Vvg` | 1 |

### CoreServices.framework

| símbolo | apps |
|---|---:|
| `_FSCopyObjectSync` | 2 |
| `_FSGetCatalogInfo` | 2 |
| `_FSPathMakeRefWithOptions` | 2 |
| `_AEStreamClose` | 1 |
| `_AEStreamCreateEvent` | 1 |
| `_AEStreamWriteKeyDesc` | 1 |
| `_CreateTextEncoding` | 1 |
| `_DisposeHandle` | 1 |

### libxslt.1.dylib

| símbolo | apps |
|---|---:|
| `_xsltApplyOneTemplate` | 1 |
| `_xsltApplyStylesheet` | 1 |
| `_xsltApplyStylesheetUser` | 1 |
| `_xsltCleanupGlobals` | 1 |
| `_xsltDocDefaultLoader` | 1 |
| `_xsltFreeSecurityPrefs` | 1 |
| `_xsltFreeStylesheet` | 1 |
| `_xsltFreeTransformContext` | 1 |

### libicucore.A.dylib

| símbolo | apps |
|---|---:|
| `_u_charAge` | 1 |
| `_u_charName` | 1 |
| `_u_errorName` | 1 |
| `_u_getNumericValue` | 1 |
| `_u_strToLower` | 1 |
| `_u_strToTitle` | 1 |
| `_u_strToUpper` | 1 |
| `_u_strlen` | 1 |

### libswiftUniformTypeIdentifiers.dylib

| símbolo | apps |
|---|---:|
| `_$s22UniformTypeIdentifiers6UTTypeVMa` | 3 |
| `_$s22UniformTypeIdentifiers6UTTypeVMn` | 3 |
| `_$s22UniformTypeIdentifiers6UTTypeV10identifierSSvg` | 2 |
| `_$s22UniformTypeIdentifiers6UTTypeV11applicationACvgZ` | 2 |
| `_$s22UniformTypeIdentifiers6UTTypeV19_bridgeToObjectiveCSoABCyF` | 2 |
| `_$s10Foundation17URLResourceValuesV22UniformTypeIdentifiersE07contentE0AD6UTTypeVSgvg` | 1 |
| `_$s22UniformTypeIdentifiers6UTTypeV04mimeB012conformingToACSgSS_ACtcfC` | 1 |
| `_$s22UniformTypeIdentifiers6UTTypeV12propertyListACvgZ` | 1 |

### Carbon.framework

| símbolo | apps |
|---|---:|
| `_GetEventDispatcherTarget` | 4 |
| `_TISCreateInputSourceList` | 3 |
| `_GetEventClass` | 2 |
| `_SetSystemUIMode` | 2 |
| `_TISSelectInputSource` | 2 |
| `_GetCurrentEventKeyModifiers` | 1 |
| `_GetThemeMetric` | 1 |
| `_HIThemeDrawButton` | 1 |

### CoreGraphics.framework

| símbolo | apps |
|---|---:|
| `_CGDisplayMoveCursorToPoint` | 2 |
| `_CGEventCreateCopy` | 2 |
| `_CGContextGetFontSmoothingStyle` | 1 |
| `_CGContextSetBaseCTM` | 1 |
| `_CGContextSetCTM` | 1 |
| `_CGContextSetFontSmoothingStyle` | 1 |
| `_CGDisplayStreamCreateWithDispatchQueue` | 1 |
| `_CGEventGetTypeID` | 1 |

### WebKit.framework

| símbolo | apps |
|---|---:|
| `_OBJC_CLASS_$_WKUserContentController` | 5 |
| `_OBJC_CLASS_$_WKUserScript` | 5 |
| `_OBJC_CLASS_$_WebView` | 5 |
| `_OBJC_CLASS_$_WKContentRuleListStore` | 3 |
| `_OBJC_CLASS_$_WebPreferences` | 3 |
| `_OBJC_CLASS_$_WKBackForwardListItem` | 1 |
| `_OBJC_CLASS_$_WKContentWorld` | 1 |
| `_OBJC_CLASS_$_WKFrameInfo` | 1 |

### SkyLight.framework

| símbolo | apps |
|---|---:|
| `_SLPSPostEventRecordTo` | 1 |
| `_SLSRegisterConnectionNotifyProc` | 1 |
| `_SLSRequestNotificationsForWindows` | 1 |
| `_SLSSpaceSetFrontPSN` | 1 |
| `_SLSWindowIteratorAdvance` | 1 |
| `_SLSWindowIteratorCopyTitle` | 1 |
| `_SLSWindowIteratorGetAlpha` | 1 |
| `_SLSWindowIteratorGetAttributes` | 1 |

### Security.framework

| símbolo | apps |
|---|---:|
| `_SSLSetAllowsAnyRoot` | 1 |
| `_SSLSetAllowsExpiredCerts` | 1 |
| `_SSLSetAllowsExpiredRoots` | 1 |
| `_SSLSetDiffieHellmanParams` | 1 |
| `_SSLSetEnableCertVerify` | 1 |
| `_SSLSetProtocolVersionEnabled` | 1 |
| `_SecKeychainItemCopyAttributesAndData` | 1 |
| `_SecKeychainItemFreeAttributesAndData` | 1 |

### ApplicationServices.framework

| símbolo | apps |
|---|---:|
| `_CGAcquireDisplayFadeReservation` | 1 |
| `_CGDisplayFade` | 1 |
| `_CGReleaseDisplayFadeReservation` | 1 |
| `_CGSCreateRegisteredCursorImage` | 1 |
| `_GetCurrentProcess` | 1 |
| `_GetNextProcess` | 1 |
| `_PMPrinterPrintWithFile` | 1 |
| `_PMRetain` | 1 |

### AudioToolbox.framework

| símbolo | apps |
|---|---:|
| `_AUGraphAddNode` | 1 |
| `_AUGraphConnectNodeInput` | 1 |
| `_AUGraphInitialize` | 1 |
| `_AUGraphNodeInfo` | 1 |
| `_AUGraphOpen` | 1 |
| `_AUGraphStart` | 1 |
| `_AUGraphStop` | 1 |
| `_AudioFileStreamClose` | 1 |

### libpanel.5.4.dylib

| símbolo | apps |
|---|---:|
| `_bottom_panel` | 1 |
| `_del_panel` | 1 |
| `_hide_panel` | 1 |
| `_move_panel` | 1 |
| `_new_panel` | 1 |
| `_panel_above` | 1 |
| `_panel_below` | 1 |
| `_panel_hidden` | 1 |

### Accelerate.framework

| símbolo | apps |
|---|---:|
| `_dgemm_` | 1 |
| `_dgemv_` | 1 |
| `_dger_` | 1 |
| `_dtrsm_` | 1 |
| `_dtrsv_` | 1 |
| `_vDSP_maxv` | 1 |
| `_vDSP_measqv` | 1 |
| `_vDSP_vsmsa` | 1 |

### libswiftos.dylib

| símbolo | apps |
|---|---:|
| `_$sSo9OS_os_logC0B0E7defaultABvgZ` | 2 |
| `_$s2os0A4_log_3dso0B04type_ys12StaticStringV_SVSgSo03OS_a1_B0CSo0a1_b1_D2_tas7CVarArg_pdtF` | 1 |
| `_$s2os21OSAllocatedUnfairLockVMn` | 1 |
| `_$s2os6LoggerV9logObjectSo03OS_a1_C0Cvg` | 1 |
| `_$s2os6LoggerV9subsystem8categoryACSS_SStcfC` | 1 |
| `_$s2os6LoggerVMa` | 1 |
| `_$sSo13os_log_type_ta0A0E4infoABvgZ` | 1 |
| `_$sSo13os_log_type_ta0A0E5debugABvgZ` | 1 |

## Contagem distinta por app

| app | distintos | sistema | libc++ | Swift | unwind |
|---|---:|---:|---:|---:|---:|
| iterm2 | 848 | 804 | 0 | 44 | 0 |
| inkscape | 470 | 470 | 0 | 0 | 0 |
| alt-tab | 255 | 58 | 0 | 197 | 0 |
| rectangle | 199 | 183 | 0 | 16 | 0 |
| mos | 156 | 156 | 0 | 0 | 0 |
| vlc | 120 | 120 | 0 | 0 | 0 |
| firefox | 119 | 119 | 0 | 0 | 0 |
| gimp | 109 | 109 | 0 | 0 | 0 |
| hiddenbar | 35 | 35 | 0 | 0 | 0 |
| keepingyouawake | 5 | 5 | 0 | 0 | 0 |
