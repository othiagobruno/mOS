# Lacunas de compatibilidade no corpus macOS

Gerado por `python3 tools/corpus/harness.py gaps` com as medidas atuais de `tools/corpus/varrer-tudo.sh`.
libSystem medido: build final 4c39ade2 2026-11-17.
O disco é o da varredura: libunwind, libc++, libswift*, libz, libcurl e, com o artefato do CI, os frameworks do
mOS. Os baldes libc++ e Swift só contam o que nenhuma dessas exporta.
Binários não estão no git. URLs e sha256 ficam em `tools/corpus/manifest.json`.

Apps com pelo menos uma imagem Mach-O x86_64: **294**.

## Símbolos que faltam em mais apps

| símbolo | apps |
|---|---:|
| `_OBJC_CLASS_$_WKUserScript` | 106 |
| `_OBJC_CLASS_$_WebView` | 102 |
| `_OBJC_CLASS_$_WKUserContentController` | 96 |
| `_vswprintf` | 88 |
| `_OBJC_CLASS_$_WebPreferences` | 87 |
| `_swprintf` | 86 |
| `_$s4Body7SwiftUI4ViewPTl` | 69 |
| `_$s7SwiftUI15ModifiedContentVMn` | 69 |
| `_$s7SwiftUI15ModifiedContentVyxq_GAA4ViewA2aERzAA0E8ModifierR_rlMc` | 69 |
| `_$s7SwiftUI4TextVyACxcSyRzlufC` | 69 |
| `_$s7SwiftUI4ViewMp` | 69 |
| `_$s7SwiftUI4ViewP05_makeC04view6inputsAA01_C7OutputsVAA11_GraphValueVyxG_AA01_C6InputsVtFZTq` | 69 |
| `_$s7SwiftUI4ViewP05_makeC4List4view6inputsAA01_cE7OutputsVAA11_GraphValueVyxG_AA01_cE6InputsVtFZTq` | 69 |
| `_$s7SwiftUI4ViewP4BodyAC_AaBTn` | 69 |
| `_$s7SwiftUI4ViewP4body4BodyQzvgTq` | 69 |
| `_$s7SwiftUI4ViewPAAE05_makeC04view6inputsAA01_C7OutputsVAA11_GraphValueVyxG_AA01_C6InputsVtFZ` | 69 |
| `_$s7SwiftUI4ViewPAAE05_makeC4List4view6inputsAA01_cE7OutputsVAA11_GraphValueVyxG_AA01_cE6InputsVtFZ` | 69 |
| `_$s7SwiftUI12_FrameLayoutVMn` | 68 |
| `_$s7SwiftUI19HorizontalAlignmentV7leadingACvgZ` | 68 |
| `_$s7SwiftUI4TextVMn` | 68 |
| `_$s7SwiftUI4ViewP14_viewListCount6inputsSiSgAA01_ceF6InputsV_tFZTq` | 68 |
| `_$s7SwiftUI4ViewPAAE14_viewListCount6inputsSiSgAA01_ceF6InputsV_tFZ` | 68 |
| `_$s7SwiftUI6HStackVMn` | 68 |
| `_$s7SwiftUI9AlignmentV6centerACvgZ` | 68 |
| `_$s7SwiftUI9TupleViewVMn` | 68 |
| `_$s7SwiftUI12_FrameLayoutV5width6height9alignmentAC12CoreGraphics7CGFloatVSg_AjA9AlignmentVtcfC` | 67 |
| `_$s7SwiftUI19HorizontalAlignmentV6centerACvgZ` | 67 |
| `_$s7SwiftUI19_ConditionalContentVMn` | 67 |
| `_$s7SwiftUI30_EnvironmentKeyWritingModifierVMn` | 67 |
| `_$s7SwiftUI5ColorVMn` | 67 |
| `_$s7SwiftUI6SpacerVMn` | 67 |
| `_$s7SwiftUI6VStackVMn` | 67 |
| `_$s7SwiftUI6VStackVyxGAA4ViewAAMc` | 67 |
| `_$s7SwiftUI7ForEachVA2A4ViewR0_rlE_2id7contentACyxq_q0_Gx_s7KeyPathCy7ElementQzq_Gq0_AKctcfC` | 67 |
| `_$s7SwiftUI7ForEachVMn` | 67 |
| `_$s7SwiftUI9EmptyViewVMn` | 67 |
| `_$s7SwiftUI13_VStackLayoutVMn` | 66 |
| `_$s7SwiftUI13_VariadicViewO4TreeVMn` | 66 |
| `_$s7SwiftUI14_PaddingLayoutVMn` | 66 |
| `_$s7SwiftUI15ModifiedContentVMa` | 66 |

## Por origem

### sistema

| símbolo | apps |
|---|---:|
| `_OBJC_CLASS_$_WKUserScript` | 106 |
| `_OBJC_CLASS_$_WebView` | 102 |
| `_OBJC_CLASS_$_WKUserContentController` | 96 |
| `_vswprintf` | 88 |
| `_OBJC_CLASS_$_WebPreferences` | 87 |
| `_swprintf` | 86 |
| `_$s4Body7SwiftUI4ViewPTl` | 69 |
| `_$s7SwiftUI15ModifiedContentVMn` | 69 |
| `_$s7SwiftUI15ModifiedContentVyxq_GAA4ViewA2aERzAA0E8ModifierR_rlMc` | 69 |
| `_$s7SwiftUI4TextVyACxcSyRzlufC` | 69 |
| `_$s7SwiftUI4ViewMp` | 69 |
| `_$s7SwiftUI4ViewP05_makeC04view6inputsAA01_C7OutputsVAA11_GraphValueVyxG_AA01_C6InputsVtFZTq` | 69 |
| `_$s7SwiftUI4ViewP05_makeC4List4view6inputsAA01_cE7OutputsVAA11_GraphValueVyxG_AA01_cE6InputsVtFZTq` | 69 |
| `_$s7SwiftUI4ViewP4BodyAC_AaBTn` | 69 |
| `_$s7SwiftUI4ViewP4body4BodyQzvgTq` | 69 |

### libc++

| símbolo | apps |
|---|---:|
| `__ZN2v811HandleScope12CreateHandleEPNS_8internal7IsolateEm` | 2 |
| `__ZN2v811HandleScopeC1EPNS_7IsolateE` | 2 |
| `__ZN2v811HandleScopeD1Ev` | 2 |
| `__ZN2v811HandleScopeD2Ev` | 2 |
| `__ZN2v86Object32SetAlignedPointerInInternalFieldEiPv` | 2 |
| `__ZN2v86Object38SlowGetAlignedPointerFromInternalFieldEi` | 2 |
| `__ZN2v87Context10GetIsolateEv` | 2 |
| `__ZN2v88External3NewEPNS_7IsolateEPv` | 2 |
| `__ZNK2v86String11WriteUtf8V2EPNS_7IsolateEPcmiPm` | 2 |
| `__ZNK2v88External5ValueEv` | 2 |
| `__ZdlPvSt19__type_descriptor_t` | 2 |
| `__Z10add_friendP9tree_nodeS0_b` | 1 |
| `__Z10build_declj9tree_codeP9tree_nodeS1_` | 1 |
| `__Z10get_typeidP9tree_nodei` | 1 |
| `__Z10start_enumP9tree_nodeS0_S0_S0_bPb` | 1 |

### Swift

| símbolo | apps |
|---|---:|
| `_$sSo10CGColorRefa12CoreGraphicsE5whiteABvgZ` | 16 |
| `_$s6System8FilePathVMa` | 11 |
| `_$sSo16CGMutablePathRefa12CoreGraphicsE10addEllipse2in9transformySo6CGRectV_So17CGAffineTransformVtF` | 11 |
| `_$s10Foundation4DataV11referencingACSo6NSDataCh_tcfC` | 9 |
| `_$s6System5ErrnoVN` | 8 |
| `_$s6System5ErrnoVs5ErrorAAMc` | 8 |
| `_$s6System8FilePathVyACSScfC` | 8 |
| `_$s7Network10NWListenerCMn` | 8 |
| `_$s6System14FileDescriptorV6_closes6ResultOyytAA5ErrnoVGyF` | 7 |
| `_$s6System14FileDescriptorVMn` | 7 |
| `_$s6System8FilePathV11withCStringyxxSPys4Int8VGKXEKlF` | 7 |
| `_$s6System8FilePathV6stringSSvg` | 7 |
| `_$s6System8FilePathVMn` | 7 |
| `_$sSo16CGMutablePathRefa12CoreGraphicsE14addRoundedRect2in11cornerWidth0J6Height9transformySo6CGRectV_AC7CGFloatVALSo17CGAffineTransformVtF` | 7 |
| `_$s6System14FileDescriptorV5_read4into16retryOnInterrupts6ResultOySiAA5ErrnoVGSw_SbtKF` | 6 |

### unwind

| símbolo | apps |
|---|---:|
| `__Unwind_Resume_or_Rethrow` | 2 |
| `__Unwind_Find_FDE` | 1 |

## Por framework

### SwiftUI.framework

| símbolo | apps |
|---|---:|
| `_$s4Body7SwiftUI4ViewPTl` | 69 |
| `_$s7SwiftUI15ModifiedContentVMn` | 69 |
| `_$s7SwiftUI15ModifiedContentVyxq_GAA4ViewA2aERzAA0E8ModifierR_rlMc` | 69 |
| `_$s7SwiftUI4TextVyACxcSyRzlufC` | 69 |
| `_$s7SwiftUI4ViewMp` | 69 |
| `_$s7SwiftUI4ViewP05_makeC04view6inputsAA01_C7OutputsVAA11_GraphValueVyxG_AA01_C6InputsVtFZTq` | 69 |
| `_$s7SwiftUI4ViewP05_makeC4List4view6inputsAA01_cE7OutputsVAA11_GraphValueVyxG_AA01_cE6InputsVtFZTq` | 69 |
| `_$s7SwiftUI4ViewP4BodyAC_AaBTn` | 69 |

### OfficeArt.framework

| símbolo | apps |
|---|---:|
| `__Z17FlushMetroPackageR8IUnknown` | 1 |
| `__Z20GetRemoteCmdXmlNamesv` | 1 |
| `__Z24UnpackUtf8StreamToStringRN3Ofc8CIStreamERNS_7CVarStrE` | 1 |
| `__Z29GetRemoteCmdXmlNamesShapeMovev` | 1 |
| `__Z31GetRemoteCmdXmlNamesShapeFormatv` | 1 |
| `__ZN10MediaUtils24GetTemporaryMediaFileURLEPK10__CFStringS2_` | 1 |
| `__ZN11AutoCaption12ImageAdapter26FGetCompressedIBSFromImageERKN3GEL6IImageERN3Mso7TCntPtrI11IByteStreamEE` | 1 |
| `__ZN11AutoCaption12ImageAdapter26FGetResizedImageByteStreamERKN3GEL6IImageERN3Mso7TCntPtrI11IByteStreamEERN15ImageCaptioning12ImageAdapter26CompressImageTelemetryDataE` | 1 |

### Combine.framework

| símbolo | apps |
|---|---:|
| `_$s7Combine25ObservableObjectPublisherC4sendyyF` | 43 |
| `_$s7Combine10PublishersO3MapVMn` | 40 |
| `_$s7Combine10PublishersO3MapVy_xq_GAA9PublisherAAMc` | 40 |
| `_$s7Combine9PublisherPAAE3mapyAA10PublishersO3MapVy_xqd__Gqd__6OutputQzclF` | 40 |
| `_$s7Combine11CancellableMp` | 38 |
| `_$s6Output7Combine9PublisherPTl` | 35 |
| `_$s7Combine9PublisherMp` | 35 |
| `_$s7Combine06CustomA21IdentifierConvertibleMp` | 34 |

### self)

| símbolo | apps |
|---|---:|
| `_PyDict_New` | 5 |
| `_PyDict_SetItem` | 5 |
| `_PyDict_SetItemString` | 5 |
| `_PyErr_Clear` | 5 |
| `_PyErr_Format` | 5 |
| `_PyErr_Occurred` | 5 |
| `_PyErr_SetString` | 5 |
| `_PyExc_AttributeError` | 5 |

### AppIntents.framework

| símbolo | apps |
|---|---:|
| `_$s10AppIntents09_SupportsA12DependenciesMp` | 22 |
| `_$s10AppIntents0A6IntentMp` | 22 |
| `_$s10AppIntents0A6IntentP04openA7WhenRunSbvgZTq` | 22 |
| `_$s10AppIntents0A6IntentP11descriptionAA0C11DescriptionVSgvgZTq` | 22 |
| `_$s10AppIntents0A6IntentP13PerformResultAC_AA0cE0Tn` | 22 |
| `_$s10AppIntents0A6IntentP14SummaryContentAC_AA09ParameterD0Tn` | 22 |
| `_$s10AppIntents0A6IntentP16parameterSummary0E7ContentQzvgZTq` | 22 |
| `_$s10AppIntents0A6IntentP20authenticationPolicyAA0c14AuthenticationE0OvgZTq` | 22 |

### Foundation.framework

| símbolo | apps |
|---|---:|
| `_$s10Foundation23LocalizedStringResourceVMa` | 37 |
| `_$s10Foundation23LocalizedStringResourceVMn` | 33 |
| `_$s10Foundation23LocalizedStringResourceV13stringLiteralACSS_tcfC` | 28 |
| `_$sSS10FoundationE9localized5table6bundle6locale7commentS2SAAE17LocalizationValueV_SSSgSo8NSBundleCSgAA6LocaleVs12StaticStringVSgtcfC` | 28 |
| `_$s10Foundation6LocaleV8LanguageV12languageCodeAC0cE0VSgvg` | 23 |
| `_$s10Foundation16AttributedStringV8markdown7options7baseURLACSS_AC22MarkdownParsingOptionsVAA0G0VSgtKcfC` | 21 |
| `_$s10Foundation40CustomLocalizedStringResourceConvertibleMp` | 21 |
| `_$s10Foundation40CustomLocalizedStringResourceConvertibleP09localizeddE0AA0cdE0VvgTq` | 21 |

### mso99.framework

| símbolo | apps |
|---|---:|
| `_$s17BrandKitTemplates0aB10ClientInfoC7hostApp9sessionIDACSS_SStcfc` | 1 |
| `_$s17BrandKitTemplates0aB10ClientInfoCMa` | 1 |
| `_$s17BrandKitTemplates0aB12TemplateItemC5titleSSvpWvd` | 1 |
| `_$s17BrandKitTemplates0aB12TemplateItemC7assetIDSSvpWvd` | 1 |
| `_$s17BrandKitTemplates0aB12TemplateItemC7setKindAA0ab3SetG0OvpWvd` | 1 |
| `_$s17BrandKitTemplates0aB12TemplateItemC8mimeTypeSSvpWvd` | 1 |
| `_$s17BrandKitTemplates0aB12TemplateItemC9assetTypeSSvpWvd` | 1 |
| `_$s17BrandKitTemplates0aB12TemplateItemC9referenceSSvpWvd` | 1 |

### Accelerate.framework

| símbolo | apps |
|---|---:|
| `_vImagePremultiplyData_RGBA8888` | 8 |
| `_vImageVerticalReflect_ARGB8888` | 6 |
| `_vDSP_mmul` | 5 |
| `_vDSP_vsmulD` | 5 |
| `_vImageConvert_420Yp8_CbCr8ToARGB8888` | 5 |
| `_vImageConvert_PlanarFtoPlanar16F` | 5 |
| `_vDSP_vmulD` | 4 |
| `_vDSP_vsq` | 4 |

### libncurses.5.4.dylib

| símbolo | apps |
|---|---:|
| `_setupterm` | 14 |
| `_tigetnum` | 14 |
| `_tigetstr` | 13 |
| `_COLORS` | 12 |
| `_COLOR_PAIRS` | 12 |
| `_COLS` | 12 |
| `_LINES` | 12 |
| `_acs_map` | 12 |

### libQt63DRender.6.dylib

| símbolo | apps |
|---|---:|
| `__ZN10Qt3DRender10QAlphaTest11qt_metacallEN11QMetaObject4CallEiPPv` | 1 |
| `__ZN10Qt3DRender10QAlphaTest11qt_metacastEPKc` | 1 |
| `__ZN10Qt3DRender10QAlphaTest16setAlphaFunctionENS0_13AlphaFunctionE` | 1 |
| `__ZN10Qt3DRender10QAlphaTest16staticMetaObjectE` | 1 |
| `__ZN10Qt3DRender10QAlphaTest17setReferenceValueEf` | 1 |
| `__ZN10Qt3DRender10QAlphaTestC2EPN8Qt3DCore5QNodeE` | 1 |
| `__ZN10Qt3DRender10QAlphaTestD2Ev` | 1 |
| `__ZN10Qt3DRender10QClipPlane11qt_metacallEN11QMetaObject4CallEiPPv` | 1 |

### libSystem.B.dylib

| símbolo | apps |
|---|---:|
| `_vswprintf` | 88 |
| `_swprintf` | 86 |
| `_hash_create` | 24 |
| `_hash_search` | 24 |
| `__res` | 20 |
| `_dbm_clearerr` | 18 |
| `_dbm_close` | 18 |
| `_dbm_delete` | 18 |

### StoreKit.framework

| símbolo | apps |
|---|---:|
| `_$s8StoreKit11TransactionVMa` | 12 |
| `_$s8StoreKit11TransactionVMn` | 12 |
| `_$s8StoreKit18VerificationResultOMn` | 12 |
| `_$s8StoreKit10StorefrontV11countryCodeSSvg` | 11 |
| `_$s8StoreKit10StorefrontVMa` | 11 |
| `_$s8StoreKit11TransactionV12TransactionsV13AsyncIteratorV4nextAA18VerificationResultOyACGSgyYaF` | 11 |
| `_$s8StoreKit11TransactionV12TransactionsV13AsyncIteratorV4nextAA18VerificationResultOyACGSgyYaFTu` | 11 |
| `_$s8StoreKit11TransactionV12TransactionsV13AsyncIteratorVMa` | 11 |

### libQt6Charts.6.dylib

| símbolo | apps |
|---|---:|
| `__ZN10QBarSeries11qt_metacallEN11QMetaObject4CallEiPPv` | 1 |
| `__ZN10QBarSeries11qt_metacastEPKc` | 1 |
| `__ZN10QBarSeries16staticMetaObjectE` | 1 |
| `__ZN10QBarSeriesC2EP7QObject` | 1 |
| `__ZN10QBarSeriesD2Ev` | 1 |
| `__ZN10QChartView10wheelEventEP11QWheelEvent` | 1 |
| `__ZN10QChartView11qt_metacallEN11QMetaObject4CallEiPPv` | 1 |
| `__ZN10QChartView11qt_metacastEPKc` | 1 |

### CoreGraphics.framework

| símbolo | apps |
|---|---:|
| `_CGDisplayCopyAllDisplayModes` | 36 |
| `_CGEventSourceButtonState` | 33 |
| `_CGDisplayModeCopyPixelEncoding` | 31 |
| `_CGEventTapCreateForPid` | 31 |
| `_CGDisplaySetDisplayMode` | 29 |
| `_CGEventSourceCounterForEventType` | 29 |
| `_CGDisplayCapture` | 24 |
| `_CGDisplayModeGetIOFlags` | 23 |

### libicucore.A.dylib

| símbolo | apps |
|---|---:|
| `_u_strToLower` | 15 |
| `_u_strToUpper` | 15 |
| `_ubrk_close` | 15 |
| `_ubrk_following` | 15 |
| `_ubrk_open` | 15 |
| `_ubrk_setText` | 15 |
| `_unorm2_getNFCInstance` | 15 |
| `_unorm2_normalize` | 15 |

### libQt6Graphs.6.dylib

| símbolo | apps |
|---|---:|
| `__Z13comparesEqualRK11QGraphsLineS1_` | 1 |
| `__ZN10QBarSeries11deselectAllEv` | 1 |
| `__ZN10QBarSeries11setBarWidthEd` | 1 |
| `__ZN10QBarSeries11setBarsTypeENS_8BarsTypeE` | 1 |
| `__ZN10QBarSeries14removeMultipleExx` | 1 |
| `__ZN10QBarSeries14setBarDelegateEP13QQmlComponent` | 1 |
| `__ZN10QBarSeries14setLabelsAngleEd` | 1 |
| `__ZN10QBarSeries15setBorderColorsERK5QListI6QColorE` | 1 |

### mso20.framework

| símbolo | apps |
|---|---:|
| `_MSOCopilotLicenseAvailable` | 1 |
| `_MSOExperimentViewController` | 1 |
| `_MSOProcessGetSharedSessionID` | 1 |
| `_MSOShouldShowExperimentUI` | 1 |
| `_MsoCpRgchToRgwchCore` | 1 |
| `_MsoCreateFileW` | 1 |
| `_MsoFAllocPx` | 1 |
| `_MsoFAlphaWch` | 1 |

### libQt63DExtras.6.dylib

| símbolo | apps |
|---|---:|
| `__ZN10Qt3DExtras10QPlaneMesh11qt_metacallEN11QMetaObject4CallEiPPv` | 1 |
| `__ZN10Qt3DExtras10QPlaneMesh11qt_metacastEPKc` | 1 |
| `__ZN10Qt3DExtras10QPlaneMesh11setMirroredEb` | 1 |
| `__ZN10Qt3DExtras10QPlaneMesh16staticMetaObjectE` | 1 |
| `__ZN10Qt3DExtras10QPlaneMesh17setMeshResolutionERK5QSize` | 1 |
| `__ZN10Qt3DExtras10QPlaneMesh8setWidthEf` | 1 |
| `__ZN10Qt3DExtras10QPlaneMesh9setHeightEf` | 1 |
| `__ZN10Qt3DExtras10QPlaneMeshC2EPN8Qt3DCore5QNodeE` | 1 |

### WebKit.framework

| símbolo | apps |
|---|---:|
| `_OBJC_CLASS_$_WKUserScript` | 106 |
| `_OBJC_CLASS_$_WebView` | 102 |
| `_OBJC_CLASS_$_WKUserContentController` | 96 |
| `_OBJC_CLASS_$_WebPreferences` | 87 |
| `_OBJC_CLASS_$_WKContentRuleListStore` | 54 |
| `_OBJC_CLASS_$_WKContentWorld` | 18 |
| `_OBJC_CLASS_$_WKPreferences` | 18 |
| `_OBJC_CLASS_$_WKProcessPool` | 12 |

### libQt6Multimedia.6.dylib

| símbolo | apps |
|---|---:|
| `__ZN10QAudioSink11qt_metacallEN11QMetaObject4CallEiPPv` | 1 |
| `__ZN10QAudioSink11qt_metacastEPKc` | 1 |
| `__ZN10QAudioSink13setBufferSizeEx` | 1 |
| `__ZN10QAudioSink16staticMetaObjectE` | 1 |
| `__ZN10QAudioSink4stopEv` | 1 |
| `__ZN10QAudioSink5resetEv` | 1 |
| `__ZN10QAudioSink5startEP9QIODevice` | 1 |
| `__ZN10QAudioSink5startEv` | 1 |

### libedit.3.dylib

| símbolo | apps |
|---|---:|
| `_add_history` | 18 |
| `_read_history` | 16 |
| `_rl_attempted_completion_function` | 16 |
| `_write_history` | 16 |
| `_history_truncate_file` | 15 |
| `_rl_attempted_completion_over` | 15 |
| `_rl_completion_append_character` | 15 |
| `_rl_completion_matches` | 15 |

### libcups.2.dylib

| símbolo | apps |
|---|---:|
| `_ppdClose` | 31 |
| `_ppdFindOption` | 31 |
| `_ppdFindMarkedChoice` | 28 |
| `_cupsGetDests2` | 20 |
| `_ppdOpenFile` | 18 |
| `_httpBlocking` | 16 |
| `_httpError` | 16 |
| `_ppdFindAttr` | 16 |

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

### IOKit.framework

| símbolo | apps |
|---|---:|
| `_IODisplayCreateInfoDictionary` | 33 |
| `_IOHIDDeviceGetValue` | 22 |
| `_IORegistryEntryGetChildIterator` | 22 |
| `_IOHIDElementGetTypeID` | 17 |
| `_IOIteratorIsValid` | 16 |
| `_IOHIDElementGetCookie` | 14 |
| `_IOObjectIsEqualTo` | 14 |
| `_IOHIDElementGetChildren` | 13 |

### JavaScriptCore.framework

| símbolo | apps |
|---|---:|
| `_OBJC_CLASS_$_JSContext` | 19 |
| `_OBJC_CLASS_$_JSValue` | 13 |
| `_JSGlobalContextRelease` | 11 |
| `_JSStringGetUTF8CString` | 11 |
| `_JSStringRelease` | 11 |
| `_JSValueToObject` | 11 |
| `_JSStringCreateWithUTF8CString` | 10 |
| `_JSEvaluateScript` | 9 |

## Contagem distinta por app

| app | distintos | sistema | libc++ | Swift | unwind |
|---|---:|---:|---:|---:|---:|
| microsoft-office | 12973 | 12824 | 143 | 6 | 0 |
| freecad | 7301 | 7301 | 0 | 0 | 0 |
| microsoft-outlook | 3291 | 3279 | 0 | 12 | 0 |
| microsoft-powerpoint | 2974 | 2965 | 0 | 9 | 0 |
| macwhisper | 2915 | 2854 | 0 | 61 | 0 |
| qgis | 2893 | 2893 | 0 | 0 | 0 |
| microsoft-excel | 2800 | 2798 | 0 | 2 | 0 |
| microsoft-word | 2799 | 2798 | 0 | 1 | 0 |
| bartender | 2032 | 1997 | 0 | 35 | 0 |
| cmux | 1998 | 1990 | 0 | 8 | 0 |
| bettertouchtool | 1913 | 1865 | 0 | 48 | 0 |
| openclaw | 1824 | 1794 | 0 | 30 | 0 |
| codexbar | 1805 | 1804 | 0 | 1 | 0 |
| coteditor | 1714 | 1710 | 0 | 4 | 0 |
| vorssaint | 1697 | 1685 | 0 | 12 | 0 |
| fluidvoice | 1688 | 1685 | 0 | 3 | 0 |
| whatsapp | 1671 | 1617 | 0 | 54 | 0 |
| superwhisper | 1617 | 1579 | 0 | 38 | 0 |
| supacode | 1599 | 1599 | 0 | 0 | 0 |
| tailscale-app | 1510 | 1498 | 0 | 12 | 0 |
| nordvpn | 1505 | 1491 | 0 | 14 | 0 |
| thaw | 1504 | 1454 | 0 | 50 | 0 |
| voiceink | 1487 | 1478 | 0 | 9 | 0 |
| duckduckgo | 1481 | 1381 | 0 | 100 | 0 |
| sfm | 1319 | 1308 | 0 | 11 | 0 |
| raycast | 1286 | 1218 | 0 | 68 | 0 |
| proxyman | 1278 | 1271 | 0 | 7 | 0 |
| orbstack | 1273 | 1257 | 0 | 16 | 0 |
| github-copilot-for-xcode | 1265 | 1253 | 0 | 12 | 0 |
| anki | 1232 | 1231 | 0 | 1 | 0 |
| protonvpn | 1211 | 1207 | 0 | 4 | 0 |
| betterdisplay | 1105 | 1104 | 0 | 1 | 0 |
| utm | 1070 | 1017 | 1 | 52 | 0 |
| windows-app | 1070 | 1068 | 0 | 2 | 0 |
| dockdoor | 1045 | 1045 | 0 | 0 | 0 |
| loop | 1017 | 1013 | 0 | 4 | 0 |
| pearcleaner | 1015 | 1015 | 0 | 0 | 0 |
| easydict | 1010 | 934 | 0 | 76 | 0 |
| applite | 999 | 999 | 0 | 0 | 0 |
| maccy | 982 | 982 | 0 | 0 | 0 |
| caskhub | 950 | 950 | 0 | 0 | 0 |
| ghostty | 923 | 920 | 0 | 3 | 0 |
| jordanbaird-ice | 918 | 916 | 0 | 2 | 0 |
| orion | 904 | 904 | 0 | 0 | 0 |
| sf-symbols | 885 | 878 | 0 | 7 | 0 |
| xcodes-app | 846 | 846 | 0 | 0 | 0 |
| puremac | 835 | 835 | 0 | 0 | 0 |
| aldente | 805 | 798 | 0 | 7 | 0 |
| finetune | 802 | 796 | 0 | 6 | 0 |
| markedit | 792 | 790 | 0 | 2 | 0 |
| 1password | 773 | 773 | 0 | 0 | 0 |
| macpacker | 737 | 714 | 0 | 23 | 0 |
| opensuperwhisper | 696 | 681 | 0 | 15 | 0 |
| homebrew-app | 688 | 664 | 0 | 24 | 0 |
| clipy | 685 | 684 | 0 | 1 | 0 |
| linearmouse | 597 | 591 | 0 | 6 | 0 |
| microsoft-teams | 572 | 572 | 0 | 0 | 0 |
| claude | 551 | 551 | 0 | 0 | 0 |
| calibre | 533 | 533 | 0 | 0 | 0 |
| cleanshot | 528 | 518 | 0 | 10 | 0 |
| keyclu | 507 | 507 | 0 | 0 | 0 |
| swiftbar | 479 | 477 | 0 | 2 | 0 |
| sequel-ace | 478 | 470 | 0 | 8 | 0 |
| chatgpt | 476 | 452 | 0 | 24 | 0 |
| crossover | 471 | 469 | 1 | 0 | 1 |
| teamviewer | 461 | 445 | 0 | 16 | 0 |
| inkscape | 431 | 431 | 0 | 0 | 0 |
| displaylink | 407 | 407 | 0 | 0 | 0 |
| rectangle | 349 | 346 | 0 | 3 | 0 |
| karabiner-elements | 341 | 339 | 0 | 2 | 0 |
| meld | 334 | 334 | 0 | 0 | 0 |
| iina | 328 | 288 | 0 | 40 | 0 |
| capcut | 317 | 317 | 0 | 0 | 0 |
| kitty | 311 | 311 | 0 | 0 | 0 |
| orcaslicer | 282 | 282 | 0 | 0 | 0 |
| blender | 279 | 279 | 0 | 0 | 0 |
| krita | 260 | 259 | 0 | 1 | 0 |
| hammerspoon | 253 | 253 | 0 | 0 | 0 |
| pycharm | 251 | 251 | 0 | 0 | 0 |
| mysqlworkbench | 250 | 250 | 0 | 0 | 0 |
| deluge | 249 | 249 | 0 | 0 | 0 |
| lm-studio | 246 | 246 | 0 | 0 | 0 |
| iterm2 | 240 | 236 | 0 | 4 | 0 |
| basictex | 225 | 225 | 0 | 0 | 0 |
| xquartz | 217 | 217 | 0 | 0 | 0 |
| emacs-app | 216 | 36 | 179 | 0 | 1 |
| sourcetree | 188 | 187 | 0 | 1 | 0 |
| rstudio | 181 | 181 | 0 | 0 | 0 |
| cherry-studio | 181 | 181 | 0 | 0 | 0 |
| stats | 180 | 174 | 0 | 6 | 0 |
| opencode-desktop | 178 | 178 | 0 | 0 | 0 |
| bbedit | 172 | 172 | 0 | 0 | 0 |
| tableplus | 165 | 66 | 0 | 99 | 0 |
| zoom | 164 | 156 | 0 | 8 | 0 |
| shotcut | 157 | 157 | 0 | 0 | 0 |
| coconutbattery | 157 | 143 | 0 | 14 | 0 |
| feishu | 145 | 145 | 0 | 0 | 0 |
| amethyst | 144 | 141 | 0 | 3 | 0 |
| kicad | 143 | 143 | 0 | 0 | 0 |
| dropbox | 141 | 141 | 0 | 0 | 0 |
| virtualbox | 139 | 139 | 0 | 0 | 0 |
| affinity | 127 | 127 | 0 | 0 | 0 |
| qlmarkdown | 120 | 120 | 0 | 0 | 0 |
| syntax-highlight | 120 | 119 | 0 | 1 | 0 |
| mactex | 114 | 114 | 0 | 0 | 0 |
| mactex-no-gui | 114 | 114 | 0 | 0 | 0 |
| pgadmin4 | 111 | 111 | 0 | 0 | 0 |
| r-app | 109 | 109 | 0 | 0 | 0 |
| obs | 106 | 99 | 0 | 7 | 0 |
| libreoffice | 103 | 103 | 0 | 0 | 0 |
| anythingllm | 102 | 102 | 0 | 0 | 0 |
| wireshark-app | 101 | 101 | 0 | 0 | 0 |
| adobe-acrobat-reader | 88 | 88 | 0 | 0 | 0 |
| gcc-arm-embedded | 82 | 82 | 0 | 0 | 0 |
| localsend | 80 | 75 | 0 | 5 | 0 |
| beekeeper-studio | 78 | 68 | 10 | 0 | 0 |
| telegram | 76 | 76 | 0 | 0 | 0 |
| moonlight | 74 | 74 | 0 | 0 | 0 |
| onedrive | 73 | 73 | 0 | 0 | 0 |
| gimp | 72 | 72 | 0 | 0 | 0 |
| cloudflare-warp | 69 | 65 | 0 | 4 | 0 |
| wechat | 69 | 69 | 0 | 0 | 0 |
| stremio | 66 | 65 | 0 | 0 | 1 |
| android-studio | 62 | 62 | 0 | 0 | 0 |
| onlyoffice | 61 | 61 | 0 | 0 | 0 |
| rancher | 60 | 60 | 0 | 0 | 0 |
| shottr | 59 | 55 | 0 | 4 | 0 |
| gemini | 57 | 57 | 0 | 0 | 0 |
| jellyfin | 55 | 55 | 0 | 0 | 0 |
| thebrowsercompany-dia | 52 | 47 | 0 | 5 | 0 |
| monitorcontrol | 48 | 48 | 0 | 0 | 0 |
| mongodb-compass | 48 | 38 | 10 | 0 | 0 |
| hex-fiend | 47 | 47 | 0 | 0 | 0 |
| arduino-ide | 47 | 35 | 12 | 0 | 0 |
| cyberduck | 46 | 46 | 0 | 0 | 0 |
| istat-menus | 46 | 38 | 0 | 8 | 0 |
| rustdesk | 45 | 45 | 0 | 0 | 0 |
| openmtp | 45 | 45 | 0 | 0 | 0 |
| alt-tab | 41 | 36 | 0 | 5 | 0 |
| skim | 40 | 40 | 0 | 0 | 0 |
| comfy | 39 | 39 | 0 | 0 | 0 |
| lmms | 38 | 38 | 0 | 0 | 0 |
| pencil | 38 | 38 | 0 | 0 | 0 |
| tic80 | 38 | 38 | 0 | 0 | 0 |
| docker-desktop | 38 | 38 | 0 | 0 | 0 |
| fuse-t | 36 | 36 | 0 | 0 | 0 |
| android-file-transfer | 36 | 36 | 0 | 0 | 0 |
| webtorrent | 33 | 33 | 0 | 0 | 0 |
| mos | 32 | 32 | 0 | 0 | 0 |
| the-unarchiver | 32 | 32 | 0 | 0 | 0 |
| google-drive | 31 | 31 | 0 | 0 | 0 |
| upscayl | 31 | 31 | 0 | 0 | 0 |
| another-redis-desktop-manager | 31 | 31 | 0 | 0 | 0 |
| intellij-idea | 30 | 30 | 0 | 0 | 0 |
| zotero | 30 | 30 | 0 | 0 | 0 |
| bambu-studio | 30 | 30 | 0 | 0 | 0 |
| datagrip | 30 | 30 | 0 | 0 | 0 |
| battery | 30 | 30 | 0 | 0 | 0 |
| love | 29 | 29 | 0 | 0 | 0 |
| drawio | 29 | 29 | 0 | 0 | 0 |
| openscad | 28 | 28 | 0 | 0 | 0 |
| bruno | 28 | 28 | 0 | 0 | 0 |
| keka | 28 | 28 | 0 | 0 | 0 |
| lulu | 28 | 28 | 0 | 0 | 0 |
| syncthing-app | 28 | 28 | 0 | 0 | 0 |
| openrct2 | 27 | 27 | 0 | 0 | 0 |
| jetbrains-toolbox | 27 | 27 | 0 | 0 | 0 |
| openlogi | 27 | 27 | 0 | 0 | 0 |
| mac-mouse-fix | 27 | 26 | 0 | 1 | 0 |
| handbrake-app | 26 | 26 | 0 | 0 | 0 |
| bitwarden | 25 | 25 | 0 | 0 | 0 |
| keepassxc | 24 | 24 | 0 | 0 | 0 |
| dolphin | 24 | 24 | 0 | 0 | 0 |
| telegram-desktop | 24 | 24 | 0 | 0 | 0 |
| opera | 24 | 24 | 0 | 0 | 0 |
| wispr-flow | 22 | 20 | 0 | 2 | 0 |
| unnaturalscrollwheels | 22 | 22 | 0 | 0 | 0 |
| temurin | 21 | 21 | 0 | 0 | 0 |
| fork | 21 | 14 | 0 | 7 | 0 |
| vnote | 20 | 20 | 0 | 0 | 0 |
| dbeaver-community | 20 | 20 | 0 | 0 | 0 |
| tor-browser | 19 | 19 | 0 | 0 | 0 |
| github | 19 | 19 | 0 | 0 | 0 |
| firefox | 18 | 18 | 0 | 0 | 0 |
| thunderbird | 18 | 18 | 0 | 0 | 0 |
| floorp | 18 | 18 | 0 | 0 | 0 |
| zen | 18 | 18 | 0 | 0 | 0 |
| librewolf | 18 | 18 | 0 | 0 | 0 |
| epic-games | 18 | 18 | 0 | 0 | 0 |
| marta | 18 | 6 | 0 | 12 | 0 |
| macdown-3000 | 18 | 18 | 0 | 0 | 0 |
| macdown | 17 | 17 | 0 | 0 | 0 |
| vagrant | 17 | 17 | 0 | 0 | 0 |
| cryptomator | 17 | 17 | 0 | 0 | 0 |
| prusaslicer | 16 | 16 | 0 | 0 | 0 |
| microsoft-edge | 16 | 16 | 0 | 0 | 0 |
| figma | 16 | 16 | 0 | 0 | 0 |
| anydesk | 16 | 16 | 0 | 0 | 0 |
| vlc | 15 | 15 | 0 | 0 | 0 |
| godot | 15 | 15 | 0 | 0 | 0 |
| pixelorama | 15 | 15 | 0 | 0 | 0 |
| nextcloud | 15 | 15 | 0 | 0 | 0 |
| unity-hub | 13 | 13 | 0 | 0 | 0 |
| typora | 13 | 13 | 0 | 0 | 0 |
| flycut | 12 | 12 | 0 | 0 | 0 |
| dotnet-sdk | 12 | 12 | 0 | 0 | 0 |
| macs-fan-control | 12 | 12 | 0 | 0 | 0 |
| ubersicht | 12 | 12 | 0 | 0 | 0 |
| clash-verge-rev | 11 | 11 | 0 | 0 | 0 |
| audacity | 10 | 10 | 0 | 0 | 0 |
| postman | 10 | 10 | 0 | 0 | 0 |
| brave-browser | 10 | 10 | 0 | 0 | 0 |
| scroll-reverser | 10 | 10 | 0 | 0 | 0 |
| headlamp | 10 | 10 | 0 | 0 | 0 |
| tunnelblick | 10 | 10 | 0 | 0 | 0 |
| antigravity-ide | 10 | 10 | 0 | 0 | 0 |
| git-credential-manager | 9 | 9 | 0 | 0 | 0 |
| keycastr | 9 | 9 | 0 | 0 | 0 |
| steam | 8 | 8 | 0 | 0 | 0 |
| microsoft-auto-update | 8 | 8 | 0 | 0 | 0 |
| raspberry-pi-imager | 7 | 7 | 0 | 0 | 0 |
| vivaldi | 7 | 7 | 0 | 0 | 0 |
| espanso | 7 | 7 | 0 | 0 | 0 |
| realvnc-connect-viewer | 7 | 7 | 0 | 0 | 0 |
| musescore | 6 | 6 | 0 | 0 | 0 |
| alacritty | 6 | 6 | 0 | 0 | 0 |
| insomnia | 6 | 6 | 0 | 0 | 0 |
| db-browser-for-sqlite | 6 | 6 | 0 | 0 | 0 |
| prismlauncher | 6 | 6 | 0 | 0 | 0 |
| middleclick | 6 | 6 | 0 | 0 | 0 |
| cc-switch | 5 | 5 | 0 | 0 | 0 |
| appcleaner | 5 | 5 | 0 | 0 | 0 |
| alfred | 5 | 5 | 0 | 0 | 0 |
| handy | 5 | 5 | 0 | 0 | 0 |
| background-music | 5 | 5 | 0 | 0 | 0 |
| quarto | 5 | 5 | 0 | 0 | 0 |
| lunar | 5 | 5 | 0 | 0 | 0 |
| dbx | 5 | 5 | 0 | 0 | 0 |
| daisydisk | 5 | 5 | 0 | 0 | 0 |
| keepingyouawake | 4 | 4 | 0 | 0 | 0 |
| transmission | 4 | 4 | 0 | 0 | 0 |
| obsidian | 4 | 4 | 0 | 0 | 0 |
| tigervnc | 4 | 4 | 0 | 0 | 0 |
| wailbrew | 4 | 4 | 0 | 0 | 0 |
| itsycal | 4 | 4 | 0 | 0 | 0 |
| squirrel-app | 4 | 4 | 0 | 0 | 0 |
| imageoptim | 4 | 4 | 0 | 0 | 0 |
| visual-studio-code | 3 | 3 | 0 | 0 | 0 |
| warp | 3 | 2 | 0 | 1 | 0 |
| termius | 3 | 3 | 0 | 0 | 0 |
| element | 2 | 2 | 0 | 0 | 0 |
| openttd | 2 | 2 | 0 | 0 | 0 |
| vscodium | 2 | 2 | 0 | 0 | 0 |
| dbgate | 2 | 2 | 0 | 0 | 0 |
| cursor | 2 | 2 | 0 | 0 | 0 |
| discord | 2 | 2 | 0 | 0 | 0 |
| slack | 2 | 2 | 0 | 0 | 0 |
| antigravity | 2 | 2 | 0 | 0 | 0 |
| notion | 2 | 2 | 0 | 0 | 0 |
| podman-desktop | 2 | 2 | 0 | 0 | 0 |
| balenaetcher | 2 | 2 | 0 | 0 | 0 |
| signal | 2 | 2 | 0 | 0 | 0 |
| ungoogled-chromium | 2 | 2 | 0 | 0 | 0 |
| t3-code | 2 | 2 | 0 | 0 | 0 |
| helium-browser | 2 | 2 | 0 | 0 | 0 |
| freelens | 2 | 2 | 0 | 0 | 0 |
| redis-insight | 2 | 2 | 0 | 0 | 0 |
| maestro | 2 | 2 | 0 | 0 | 0 |
| kiro | 2 | 2 | 0 | 0 | 0 |
| burp-suite | 2 | 2 | 0 | 0 | 0 |
| tabby | 2 | 2 | 0 | 0 | 0 |
| wave | 2 | 2 | 0 | 0 | 0 |
| block-goose | 2 | 2 | 0 | 0 | 0 |
| linear | 2 | 2 | 0 | 0 | 0 |
| lens | 2 | 2 | 0 | 0 | 0 |
| quicklook-video | 2 | 1 | 0 | 1 | 0 |
| qbittorrent | 1 | 1 | 0 | 0 | 0 |
| ollama-app | 1 | 1 | 0 | 0 | 0 |
| sublime-text | 1 | 1 | 0 | 0 | 0 |
| wezterm | 1 | 1 | 0 | 0 | 0 |
| mounty | 1 | 1 | 0 | 0 | 0 |
| openvpn-connect | 1 | 1 | 0 | 0 | 0 |
| hiddenbar | 0 | 0 | 0 | 0 | 0 |
| finicky | 0 | 0 | 0 | 0 | 0 |
| macfuse | 0 | 0 | 0 | 0 | 0 |
| blackhole-2ch | 0 | 0 | 0 | 0 | 0 |
| session-manager-plugin | 0 | 0 | 0 | 0 | 0 |
| zed | 0 | 0 | 0 | 0 | 0 |
| caffeine | 0 | 0 | 0 | 0 | 0 |
| multipass | 0 | 0 | 0 | 0 | 0 |
| blackhole-16ch | 0 | 0 | 0 | 0 | 0 |
| notunes | 0 | 0 | 0 | 0 | 0 |
| grandperspective | 0 | 0 | 0 | 0 | 0 |
| onyx | 0 | 0 | 0 | 0 | 0 |
