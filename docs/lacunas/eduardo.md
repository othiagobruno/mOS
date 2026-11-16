# Lacunas de Eduardo (Foundation/CoreFoundation)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Eduardo (Foundation/CoreFoundation) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**3321** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| CoreFoundation.framework | `_CFBundleGetBundleWithIdentifier` | 29 | alacritty, audacity, bitwarden, blackhole-2ch |
| CoreFoundation.framework | `_CFStringGetSystemEncoding` | 24 | android-studio, audacity, cyberduck, db-browser-for-sqlite |
| CoreFoundation.framework | `_kCFLocaleLanguageCode` | 20 | android-studio, audacity, cyberduck, dbeaver-community |
| CoreFoundation.framework | `_kCFURLIsPackageKey` | 20 | audacity, db-browser-for-sqlite, dolphin, firefox |
| CoreFoundation.framework | `_CFURLEnumeratorGetNextURL` | 19 | audacity, blender, db-browser-for-sqlite, dolphin |
| Foundation.framework | `_$s10Foundation11JSONDecoderC6decode_4fromxxm_AA4DataVtKSeRzlFTj` | 18 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation11JSONDecoderCACycfc` | 18 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation11JSONDecoderCMa` | 18 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation22_convertNSErrorToErrorys0E0_pSo0C0CSgF` | 18 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation3URLV19_bridgeToObjectiveCSo5NSURLCyF` | 18 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation3URLV36_unconditionallyBridgeFromObjectiveCyACSo5NSURLCSgFZ` | 18 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation3URLVMa` | 18 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation3URLVMn` | 18 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$sSS10FoundationE19_bridgeToObjectiveCSo8NSStringCyF` | 18 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$sSS10FoundationE36_unconditionallyBridgeFromObjectiveCySSSo8NSStringCSgFZ` | 18 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$sSa10FoundationE19_bridgeToObjectiveCSo7NSArrayCyF` | 18 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$ss5ErrorP10FoundationE20localizedDescriptionSSvg` | 18 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_OBJC_CLASS_$_NSAppleScript` | 18 | 1password, android-studio, betterdisplay, bruno |
| CoreFoundation.framework | `_CFSocketCreateWithNative` | 17 | audacity, db-browser-for-sqlite, dolphin, hammerspoon |
| Foundation.framework | `_$s10Foundation4DataV19_bridgeToObjectiveCSo6NSDataCyF` | 17 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation4DateVMa` | 17 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation4UUIDV2eeoiySbAC_ACtFZ` | 17 | betterdisplay, chatgpt, claude, clipy |
| Foundation.framework | `_$sSD10FoundationE19_bridgeToObjectiveCSo12NSDictionaryCyF` | 17 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$sSa10FoundationE36_unconditionallyBridgeFromObjectiveCySayxGSo7NSArrayCSgFZ` | 17 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$sSy10FoundationE20replacingOccurrences2of4with7options5rangeSSqd___qd_0_So22NSStringCompareOptionsVSnySS5IndexVGSgtSyRd__SyRd_0_r0_lF` | 17 | 1password, betterdisplay, chatgpt, clipy |
| libswiftFoundation.dylib | `_$sSS10FoundationE19_bridgeToObjectiveCSo8NSStringCyF` | 17 | 1password, alt-tab, antigravity, claude |
| CoreFoundation.framework | `_CFSocketGetSocketFlags` | 16 | audacity, db-browser-for-sqlite, dolphin, hammerspoon |
| CoreFoundation.framework | `_kCFLocaleCurrencyCode` | 16 | android-studio, audacity, cyberduck, db-browser-for-sqlite |
| CoreFoundation.framework | `_kCFLocaleCurrencySymbol` | 16 | android-studio, audacity, cyberduck, db-browser-for-sqlite |
| Foundation.framework | `_$s10Foundation11JSONEncoderC6encodeyAA4DataVxKSERzlFTj` | 16 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation11JSONEncoderCACycfc` | 16 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation11JSONEncoderCMa` | 16 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation12NotificationV36_unconditionallyBridgeFromObjectiveCyACSo14NSNotificationCSgFZ` | 16 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation12NotificationVMa` | 16 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation4DataV36_unconditionallyBridgeFromObjectiveCyACSo6NSDataCSgFZ` | 16 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation4DateV19_bridgeToObjectiveCSo6NSDateCyF` | 16 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation4UUIDVACycfC` | 16 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation4UUIDVMa` | 16 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation4UUIDVMn` | 16 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$sSD10FoundationE36_unconditionallyBridgeFromObjectiveCySDyxq_GSo12NSDictionaryCSgFZ` | 16 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$sSS10FoundationE26_forceBridgeFromObjectiveC_6resultySo8NSStringC_SSSgztFZ` | 16 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$sSS10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo8NSStringC_SSSgztFZ` | 16 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$sSS10FoundationE8EncodingV4utf8ACvgZ` | 16 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$sSS10FoundationE8EncodingVMa` | 16 | betterdisplay, chatgpt, clipy, cmux |
| libswiftFoundation.dylib | `_$s10Foundation4DataV36_unconditionallyBridgeFromObjectiveCyACSo6NSDataCSgFZ` | 16 | 1password, alt-tab, claude, cursor |
| libswiftFoundation.dylib | `_$sSS10FoundationE36_unconditionallyBridgeFromObjectiveCySSSo8NSStringCSgFZ` | 16 | 1password, alt-tab, antigravity, claude |
| CoreFoundation.framework | `_CFPreferencesSetMultiple` | 15 | audacity, clipy, db-browser-for-sqlite, dolphin |
| CoreFoundation.framework | `_kCFLocaleDecimalSeparator` | 15 | audacity, db-browser-for-sqlite, dolphin, keepassxc |
| CoreFoundation.framework | `_kCFLocaleGroupingSeparator` | 15 | audacity, db-browser-for-sqlite, dolphin, keepassxc |
| Foundation.framework | `_$s10Foundation12CharacterSetV22whitespacesAndNewlinesACvgZ` | 15 | 1password, betterdisplay, chatgpt, clipy |
| Foundation.framework | `_$s10Foundation12CharacterSetVMa` | 15 | 1password, betterdisplay, chatgpt, clipy |
| Foundation.framework | `_$s10Foundation12NotificationV6objectypSgvg` | 15 | betterdisplay, chatgpt, claude, clipy |
| Foundation.framework | `_$s10Foundation13__DataStorageC5bytes6length4copy11deallocator6offsetACSvSg_SiSbySv_SitcSgSitcfc` | 15 | 1password, betterdisplay, chatgpt, clipy |
| Foundation.framework | `_$s10Foundation13__DataStorageC6_bytesSvSgvg` | 15 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation13__DataStorageC7_lengthSivg` | 15 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation13__DataStorageC7_offsetSivg` | 15 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation22_convertErrorToNSErrorySo0E0Cs0C0_pF` | 15 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation3URLV22appendingPathComponentyACSSF` | 15 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation3URLV4pathSSvg` | 15 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation3URLV6stringACSgSSh_tcfC` | 15 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation4DataV10contentsOf7optionsAcA3URLVh_So20NSDataReadingOptionsVtKcfC` | 15 | 1password, betterdisplay, chatgpt, clipy |
| Foundation.framework | `_$s10Foundation4DataVMn` | 15 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation4DataVN` | 15 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation4DateVACycfC` | 15 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation4DateVMn` | 15 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation4UUIDV10uuidStringACSgSSh_tcfC` | 15 | 1password, betterdisplay, chatgpt, claude |
| Foundation.framework | `_$s10Foundation4UUIDVSHAAMc` | 15 | betterdisplay, chatgpt, claude, clipy |
| Foundation.framework | `_$s10Foundation6LocaleVMa` | 15 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation6LocaleVMn` | 15 | 1password, betterdisplay, chatgpt, clipy |
| Foundation.framework | `_$sSy10FoundationE8containsySbqd__SyRd__lF` | 15 | betterdisplay, chatgpt, clipy, cmux |
| libswiftFoundation.dylib | `_$s10Foundation3URLV36_unconditionallyBridgeFromObjectiveCyACSo5NSURLCSgFZ` | 15 | 1password, alt-tab, claude, cursor |
| libswiftFoundation.dylib | `_$s10Foundation3URLV4pathSSvg` | 15 | 1password, alt-tab, claude, cursor |
| libswiftFoundation.dylib | `_$s10Foundation3URLVMa` | 15 | 1password, alt-tab, claude, cursor |
| libswiftFoundation.dylib | `_$s10Foundation3URLVMn` | 15 | 1password, alt-tab, claude, cursor |
| libswiftFoundation.dylib | `_$s10Foundation4DataV19_bridgeToObjectiveCSo6NSDataCyF` | 15 | 1password, alt-tab, claude, cursor |
| CoreFoundation.framework | `_CFSocketDisableCallBacks` | 14 | audacity, db-browser-for-sqlite, dolphin, keepassxc |
| CoreFoundation.framework | `_CFSocketEnableCallBacks` | 14 | audacity, db-browser-for-sqlite, dolphin, keepassxc |
| Foundation.framework | `_$s10Foundation13__DataStorageC6lengthACSi_tcfc` | 14 | 1password, betterdisplay, chatgpt, clipy |
| Foundation.framework | `_$s10Foundation15ContiguousBytesMp` | 14 | 1password, betterdisplay, chatgpt, clipy |
| Foundation.framework | `_$s10Foundation15ContiguousBytesP010withUnsafeC0yqd__qd__SWKXEKlFTj` | 14 | 1password, betterdisplay, chatgpt, clipy |
| Foundation.framework | `_$s10Foundation4DataV10LargeSliceV21ensureUniqueReferenceyyF` | 14 | 1password, betterdisplay, chatgpt, clipy |
| Foundation.framework | `_$s10Foundation4DataV15_RepresentationO15replaceSubrange_4with5countySnySiG_SVSgSitF` | 14 | 1password, betterdisplay, chatgpt, clipy |
| Foundation.framework | `_$s10Foundation4DataV15_RepresentationO6append10contentsOfySW_tF` | 14 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation4DataV15_RepresentationON` | 14 | 1password, betterdisplay, chatgpt, clipy |
| Foundation.framework | `_$s10Foundation4DateV36_unconditionallyBridgeFromObjectiveCyACSo6NSDateCSgFZ` | 14 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$sSS10FoundationE4data8encodingSSSgAA4DataVh_SSAAE8EncodingVtcfC` | 14 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$sSy10FoundationE18trimmingCharacters2inSSAA12CharacterSetV_tF` | 14 | 1password, betterdisplay, chatgpt, clipy |
| Foundation.framework | `_$sSy10FoundationE4data5using20allowLossyConversionAA4DataVSgSSAAE8EncodingV_SbtF` | 14 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$sSy10FoundationE5range2of7optionsAB6localeSnySS5IndexVGSgqd___So22NSStringCompareOptionsVAiA6LocaleVSgtSyRd__lF` | 14 | betterdisplay, chatgpt, clipy, cmux |
| libswiftFoundation.dylib | `_$s10Foundation13__DataStorageC6_bytesSvSgvg` | 14 | 1password, alt-tab, antigravity, claude |
| libswiftFoundation.dylib | `_$s10Foundation13__DataStorageC7_offsetSivg` | 14 | 1password, alt-tab, antigravity, claude |
| libswiftFoundation.dylib | `_$s10Foundation22_convertNSErrorToErrorys0E0_pSo0C0CSgF` | 14 | 1password, alt-tab, claude, cursor |
| libswiftFoundation.dylib | `_$s10Foundation3URLV19_bridgeToObjectiveCSo5NSURLCyF` | 14 | 1password, alt-tab, claude, cursor |
| libswiftFoundation.dylib | `_$s10Foundation4DataVN` | 14 | 1password, alt-tab, claude, cursor |
| libswiftFoundation.dylib | `_$sSD10FoundationE19_bridgeToObjectiveCSo12NSDictionaryCyF` | 14 | 1password, alt-tab, antigravity, claude |
| libswiftFoundation.dylib | `_$sSa10FoundationE19_bridgeToObjectiveCSo7NSArrayCyF` | 14 | 1password, alt-tab, claude, cursor |
| libswiftFoundation.dylib | `_$sSa10FoundationE36_unconditionallyBridgeFromObjectiveCySayxGSo7NSArrayCSgFZ` | 14 | 1password, alt-tab, claude, cursor |
| libswiftFoundation.dylib | `_$ss5ErrorP10FoundationE20localizedDescriptionSSvg` | 14 | 1password, alt-tab, claude, cursor |
| CoreFoundation.framework | `_CFBundleCopyBuiltInPlugInsURL` | 13 | audacity, db-browser-for-sqlite, dolphin, keepassxc |
| CoreFoundation.framework | `_CFBundleGetPackageInfoInDirectory` | 13 | audacity, db-browser-for-sqlite, dolphin, keepassxc |
| CoreFoundation.framework | `_CFURLEnumeratorCreateForMountedVolumes` | 13 | audacity, blender, db-browser-for-sqlite, dolphin |
| CoreFoundation.framework | `_kCFLocaleAlternateQuotationBeginDelimiterKey` | 13 | audacity, db-browser-for-sqlite, dolphin, keepassxc |
| CoreFoundation.framework | `_kCFLocaleAlternateQuotationEndDelimiterKey` | 13 | audacity, db-browser-for-sqlite, dolphin, keepassxc |
| CoreFoundation.framework | `_kCFLocaleMeasurementSystem` | 13 | audacity, db-browser-for-sqlite, dolphin, keepassxc |
| CoreFoundation.framework | `_kCFLocaleQuotationBeginDelimiterKey` | 13 | audacity, db-browser-for-sqlite, dolphin, keepassxc |
| CoreFoundation.framework | `_kCFLocaleQuotationEndDelimiterKey` | 13 | audacity, db-browser-for-sqlite, dolphin, keepassxc |
| CoreFoundation.framework | `_kCFURLIsAliasFileKey` | 13 | audacity, db-browser-for-sqlite, dolphin, keepassxc |
| CoreFoundation.framework | `_kCFURLNameKey` | 13 | audacity, db-browser-for-sqlite, dolphin, keepassxc |
| CoreFoundation.framework | `_kCFURLVolumeURLKey` | 13 | audacity, db-browser-for-sqlite, dolphin, keepassxc |
| Foundation.framework | `_$s10Foundation12NotificationV8userInfoSDys11AnyHashableVypGSgvg` | 13 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation13URLComponentsVMa` | 13 | 1password, betterdisplay, chatgpt, clipy |
| Foundation.framework | `_$s10Foundation13URLComponentsVMn` | 13 | 1password, betterdisplay, chatgpt, clipy |
| Foundation.framework | `_$s10Foundation13__DataStorageC5bytes6lengthACSVSg_Sitcfc` | 13 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation13__DataStorageCMa` | 13 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation3URLV14absoluteStringSSvg` | 13 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation4DataV14RangeReferenceCMa` | 13 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation4DateVSEAAMc` | 13 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation4UUIDV10uuidStringSSvg` | 13 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation4UUIDVSQAAMc` | 13 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$sSS10FoundationE6format_S2Sh_s7CVarArg_pdtcfC` | 13 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$sSb10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF` | 13 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$sSi10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF` | 13 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$sSy10FoundationE10components11separatedBySaySSGAA12CharacterSetV_tF` | 13 | 1password, betterdisplay, chatgpt, claude |
| Foundation.framework | `_NSFileTypeForHFSTypeCode` | 13 | alt-tab, db-browser-for-sqlite, dolphin, iterm2 |
| libswiftFoundation.dylib | `_$sSD10FoundationE36_unconditionallyBridgeFromObjectiveCySDyxq_GSo12NSDictionaryCSgFZ` | 13 | 1password, alt-tab, antigravity, claude |
| libswiftFoundation.dylib | `_$sSS10FoundationE4data8encodingSSSgAA4DataVh_SSAAE8EncodingVtcfC` | 13 | 1password, alt-tab, claude, cursor |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV4utf8ACvgZ` | 13 | 1password, alt-tab, claude, cursor |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingVMa` | 13 | 1password, alt-tab, claude, cursor |
| CoreFoundation.framework | `_CFShow` | 12 | android-studio, audacity, bitwarden, dbeaver-community |
| CoreFoundation.framework | `_kCFLocaleScriptCode` | 12 | android-studio, audacity, cyberduck, dbeaver-community |
| Foundation.framework | `_$s10Foundation13URLComponentsV3url23resolvingAgainstBaseURLACSgAA0G0Vh_SbtcfC` | 12 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation14LocalizedErrorMp` | 12 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation14LocalizedErrorP10helpAnchorSSSgvgTq` | 12 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation14LocalizedErrorP13failureReasonSSSgvgTq` | 12 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation14LocalizedErrorP16errorDescriptionSSSgvgTq` | 12 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation14LocalizedErrorP18recoverySuggestionSSSgvgTq` | 12 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation14LocalizedErrorPAAE10helpAnchorSSSgvg` | 12 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation14LocalizedErrorPAAE13failureReasonSSSgvg` | 12 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation14LocalizedErrorPAAE18recoverySuggestionSSSgvg` | 12 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation14LocalizedErrorPs0C0Tb` | 12 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation3URLV13pathExtensionSSvg` | 12 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation3URLV15fileURLWithPathACSSh_tcfC` | 12 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation3URLV17lastPathComponentSSvg` | 12 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation3URLV21deletingPathExtensionACyF` | 12 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation3URLV25deletingLastPathComponentACyF` | 12 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation4DataV5write2to7optionsyAA3URLV_So20NSDataWritingOptionsVtKF` | 12 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation4DataVSEAAMc` | 12 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation4DateV17timeIntervalSinceySdACF` | 12 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation4DateV21timeIntervalSince1970ACSd_tcfC` | 12 | betterdisplay, chatgpt, claude, clipy |
| Foundation.framework | `_$s10Foundation4DateVSeAAMc` | 12 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation6LocaleV19_bridgeToObjectiveCSo8NSLocaleCyF` | 12 | betterdisplay, chatgpt, claude, clipy |
| Foundation.framework | `_$sSa10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo7NSArrayC_SayxGSgztFZ` | 12 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$sSh10FoundationE19_bridgeToObjectiveCSo5NSSetCyF` | 12 | betterdisplay, clipy, cmux, coteditor |
| Foundation.framework | `_$sSy10FoundationE10components11separatedBySaySSGqd___tSyRd__lF` | 12 | 1password, betterdisplay, chatgpt, cmux |
| libswiftFoundation.dylib | `_$s10Foundation12NotificationV36_unconditionallyBridgeFromObjectiveCyACSo14NSNotificationCSgFZ` | 12 | 1password, alt-tab, claude, cursor |
| libswiftFoundation.dylib | `_$s10Foundation12NotificationVMa` | 12 | 1password, alt-tab, claude, cursor |
| libswiftFoundation.dylib | `_$s10Foundation13__DataStorageC7_lengthSivg` | 12 | 1password, alt-tab, claude, github |
| libswiftFoundation.dylib | `_$s10Foundation13__DataStorageCMa` | 12 | 1password, alt-tab, claude, cursor |
| libswiftFoundation.dylib | `_$s10Foundation22_convertErrorToNSErrorySo0E0Cs0C0_pF` | 12 | 1password, claude, drawio, github |
| libswiftFoundation.dylib | `_$s10Foundation3URLV15fileURLWithPathACSSh_tcfC` | 12 | 1password, alt-tab, claude, cursor |
| libswiftFoundation.dylib | `_$s10Foundation3URLV6stringACSgSSh_tcfC` | 12 | 1password, alt-tab, claude, cursor |
| libswiftFoundation.dylib | `_$s10Foundation4DataVMn` | 12 | 1password, alt-tab, antigravity, claude |
| libswiftFoundation.dylib | `_$sSS10FoundationE26_forceBridgeFromObjectiveC_6resultySo8NSStringC_SSSgztFZ` | 12 | 1password, alt-tab, claude, cursor |
| libswiftFoundation.dylib | `_$sSS10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo8NSStringC_SSSgztFZ` | 12 | 1password, alt-tab, claude, cursor |
| Foundation.framework | `_$s10Foundation12CharacterSetV11whitespacesACvgZ` | 11 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation12URLQueryItemVMa` | 11 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation21NSKeyValueObservationCMn` | 11 | betterdisplay, chatgpt, cmux, coteditor |
| Foundation.framework | `_$s10Foundation25NSFastEnumerationIteratorVMa` | 11 | betterdisplay, chatgpt, claude, clipy |
| Foundation.framework | `_$s10Foundation27_KeyValueCodingAndObservingPAAE7observe_7options13changeHandlerAA05NSKeyC11ObservationCs0B4PathCyxqd__G_So0kcF7OptionsVyx_AA0kC14ObservedChangeVyqd__GtctlF` | 11 | betterdisplay, chatgpt, cmux, coteditor |
| Foundation.framework | `_$s10Foundation3URLV22appendingPathComponent_11isDirectoryACSS_SbtF` | 11 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation3URLVSHAAMc` | 11 | chatgpt, claude, clipy, cmux |
| Foundation.framework | `_$s10Foundation3URLVSQAAMc` | 11 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation3URLVs23CustomStringConvertibleAAMc` | 11 | betterdisplay, chatgpt, claude, cmux |
| Foundation.framework | `_$s10Foundation4DataV8IteratorVMa` | 11 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation4DateV026timeIntervalSinceReferenceB0Sdvg` | 11 | betterdisplay, chatgpt, claude, clipy |
| Foundation.framework | `_$s10Foundation4DateV18addingTimeIntervalyACSdF` | 11 | betterdisplay, chatgpt, cmux, coteditor |
| Foundation.framework | `_$s10Foundation4DateV1loiySbAC_ACtFZ` | 11 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation4DateV21timeIntervalSince1970Sdvg` | 11 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation4UUIDV19_bridgeToObjectiveCSo6NSUUIDCyF` | 11 | betterdisplay, chatgpt, claude, cmux |
| Foundation.framework | `_$s10Foundation6LocaleV10identifierACSS_tcfC` | 11 | betterdisplay, chatgpt, claude, clipy |
| Foundation.framework | `_$s10Foundation6LocaleV7currentACvgZ` | 11 | 1password, betterdisplay, chatgpt, clipy |
| Foundation.framework | `_$s10Foundation8TimeZoneVMa` | 11 | 1password, betterdisplay, chatgpt, clipy |
| Foundation.framework | `_$sSD10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo12NSDictionaryC_SDyxq_GSgztFZ` | 11 | betterdisplay, chatgpt, cmux, coteditor |
| Foundation.framework | `_$sSS10FoundationE5bytes8encodingSSSgxh_SSAAE8EncodingVtcSTRzs5UInt8V7ElementRtzlufC` | 11 | betterdisplay, chatgpt, claude, cmux |
| Foundation.framework | `_$sSo8NSObjectC10Foundation27_KeyValueCodingAndObservingACWP` | 11 | betterdisplay, chatgpt, cmux, coteditor |
| Foundation.framework | `_$sSo8_NSRangeV10FoundationE_2inABx_q_tcSXRzSyR_SS5IndexV5BoundRtzr0_lufC` | 11 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$sSy10FoundationE11capitalizedSSvg` | 11 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_OBJC_CLASS_$_NSUserNotificationAction` | 11 | bruno, firefox, floorp, hammerspoon |
| libswiftFoundation.dylib | `_$s10Foundation11JSONDecoderC6decode_4fromxxm_AA4DataVtKSeRzlFTj` | 11 | 1password, alt-tab, claude, cursor |
| libswiftFoundation.dylib | `_$s10Foundation11JSONDecoderCACycfc` | 11 | 1password, alt-tab, claude, cursor |
| libswiftFoundation.dylib | `_$s10Foundation11JSONDecoderCMa` | 11 | 1password, alt-tab, claude, cursor |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC6encodeyAA4DataVxKSERzlFTj` | 11 | 1password, alt-tab, claude, hex-fiend |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderCACycfc` | 11 | 1password, alt-tab, claude, hex-fiend |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderCMa` | 11 | 1password, alt-tab, claude, hex-fiend |
| libswiftFoundation.dylib | `_$s10Foundation13__DataStorageC5bytes6lengthACSVSg_Sitcfc` | 11 | 1password, claude, cursor, github |
| libswiftFoundation.dylib | `_$s10Foundation3URLV17lastPathComponentSSvg` | 11 | 1password, alt-tab, claude, cursor |
| libswiftFoundation.dylib | `_$s10Foundation3URLV22appendingPathComponentyACSSF` | 11 | 1password, alt-tab, claude, cursor |
| libswiftFoundation.dylib | `_$s10Foundation4DataV14RangeReferenceCMa` | 11 | 1password, claude, cursor, github |
| libswiftFoundation.dylib | `_$s10Foundation4DateV19_bridgeToObjectiveCSo6NSDateCyF` | 11 | 1password, alt-tab, claude, cursor |
| libswiftFoundation.dylib | `_$s10Foundation4DateVMa` | 11 | 1password, alt-tab, claude, cursor |
| libswiftFoundation.dylib | `_$s10Foundation4UUIDVMa` | 11 | 1password, alt-tab, claude, iina |
| libswiftFoundation.dylib | `_$s10Foundation6LocaleVMa` | 11 | 1password, alt-tab, claude, hex-fiend |
| CoreFoundation.framework | `_CFBundleGetAllBundles` | 10 | audacity, db-browser-for-sqlite, dolphin, krita |
| CoreFoundation.framework | `_CFBundleGetDataPointerForName` | 10 | deluge, firefox, floorp, git-credential-manager |
| CoreFoundation.framework | `_CFURLCreateFromFSRef` | 10 | deluge, firefox, floorp, flycut |
| Foundation.framework | `_$s10Foundation10CocoaErrorV4CodeVMa` | 10 | betterdisplay, chatgpt, claude, clipy |
| Foundation.framework | `_$s10Foundation10URLRequestV19_bridgeToObjectiveCSo12NSURLRequestCyF` | 10 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation10URLRequestV36_unconditionallyBridgeFromObjectiveCyACSo12NSURLRequestCSgFZ` | 10 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation10URLRequestV3url11cachePolicy15timeoutIntervalAcA3URLV_So017NSURLRequestCacheE0VSdtcfC` | 10 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation10URLRequestV8setValue_18forHTTPHeaderFieldySSSg_SStF` | 10 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation10URLRequestVMa` | 10 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation11JSONEncoderC16OutputFormattingV10sortedKeysAEvgZ` | 10 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation11JSONEncoderC16OutputFormattingVMa` | 10 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation11JSONEncoderC16outputFormattingAC06OutputD0VvsTj` | 10 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation11JSONEncoderCMn` | 10 | betterdisplay, chatgpt, claude, clipy |
| Foundation.framework | `_$s10Foundation12CharacterSetV8newlinesACvgZ` | 10 | chatgpt, claude, cmux, coteditor |
| Foundation.framework | `_$s10Foundation13URLComponentsV10queryItemsSayAA12URLQueryItemVGSgvg` | 10 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation16AttributedStringVMa` | 10 | 1password, chatgpt, cmux, coteditor |
| Foundation.framework | `_$s10Foundation16AttributedStringVMn` | 10 | 1password, chatgpt, cmux, coteditor |
| Foundation.framework | `_$s10Foundation17URLResourceValuesVMa` | 10 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation24NSKeyValueObservedChangeV03newC0xSgvg` | 10 | betterdisplay, chatgpt, cmux, coteditor |
| Foundation.framework | `_$s10Foundation24NSKeyValueObservedChangeVMn` | 10 | betterdisplay, chatgpt, cmux, coteditor |
| Foundation.framework | `_$s10Foundation3URLV14resourceValues7forKeysAA011URLResourceD0VShySo16NSURLResourceKeyaG_tKF` | 10 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation3URLV2eeoiySbAC_ACtFZ` | 10 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation3URLV6schemeSSSgvg` | 10 | betterdisplay, chatgpt, cmux, ghostty |
| Foundation.framework | `_$s10Foundation4DataV13_copyContents12initializingAC8IteratorV_SitSrys5UInt8VG_tF` | 10 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation4DataV13base64Encoded7optionsACSgSSh_So27NSDataBase64DecodingOptionsVtcfC` | 10 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation4DataV19base64EncodedString7optionsSSSo27NSDataBase64EncodingOptionsV_tF` | 10 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation4DataV4hash4intoys6HasherVz_tF` | 10 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation4DataV6appendyyACF` | 10 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation4DataV8IteratorV_2atAeC_SitcfC` | 10 | 1password, betterdisplay, chatgpt, clipy |
| Foundation.framework | `_$s10Foundation4DataVSeAAMc` | 10 | betterdisplay, chatgpt, cmux, ghostty |
| Foundation.framework | `_$s10Foundation4DateV2eeoiySbAC_ACtFZ` | 10 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation8CalendarVMa` | 10 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation8IndexSetVMa` | 10 | clipy, cmux, coteditor, iterm2 |
| Foundation.framework | `_$sSS10FoundationE17LocalizationValueVMa` | 10 | 1password, betterdisplay, chatgpt, clipy |
| Foundation.framework | `_$sSS10FoundationE8EncodingV5asciiACvgZ` | 10 | betterdisplay, chatgpt, claude, cmux |
| Foundation.framework | `_$sSSs7CVarArg10FoundationMc` | 10 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$sSh10FoundationE36_unconditionallyBridgeFromObjectiveCyShyxGSo5NSSetCSgFZ` | 10 | betterdisplay, clipy, cmux, ghostty |
| Foundation.framework | `_$sSn10FoundationSS5IndexVRszrlE_2inSnyACGSgSo8_NSRangeV_SShtcfC` | 10 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$sSo7NSArrayC10FoundationE12makeIteratorAC017NSFastEnumerationD0VyF` | 10 | betterdisplay, chatgpt, claude, cmux |
| Foundation.framework | `_$sSy10FoundationE21addingPercentEncoding21withAllowedCharactersSSSgAA12CharacterSetV_tF` | 10 | betterdisplay, chatgpt, cmux, coteditor |
| Foundation.framework | `_$sSy10FoundationE22caseInsensitiveCompareySo18NSComparisonResultVqd__SyRd__lF` | 10 | chatgpt, claude, clipy, cmux |
| Foundation.framework | `_$sSy10FoundationE23removingPercentEncodingSSSgvg` | 10 | betterdisplay, chatgpt, cmux, coteditor |
| Foundation.framework | `_$sSy10FoundationE31localizedCaseInsensitiveCompareySo18NSComparisonResultVqd__SyRd__lF` | 10 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_NSDivideRect` | 10 | clipy, cyberduck, hammerspoon, hex-fiend |
| Foundation.framework | `_NSRectFromString` | 10 | iina, iterm2, krita, macdown |
| libswiftFoundation.dylib | `_$s10Foundation3URLV13pathExtensionSSvg` | 10 | 1password, alt-tab, claude, iina |
| libswiftFoundation.dylib | `_$s10Foundation3URLV14absoluteStringSSvg` | 10 | 1password, alt-tab, claude, iina |
| libswiftFoundation.dylib | `_$s10Foundation3URLV25deletingLastPathComponentACyF` | 10 | 1password, alt-tab, claude, cursor |
| libswiftFoundation.dylib | `_$s10Foundation4DataV15_RepresentationO6append10contentsOfySW_tF` | 10 | 1password, alt-tab, claude, github |
| libswiftFoundation.dylib | `_$s10Foundation4DateV21timeIntervalSince1970Sdvg` | 10 | 1password, alt-tab, claude, cursor |
| libswiftFoundation.dylib | `_$s10Foundation4DateVACycfC` | 10 | alt-tab, claude, cursor, iina |
| libswiftFoundation.dylib | `_$s10Foundation4DateVMn` | 10 | alt-tab, claude, cursor, iina |
| libswiftFoundation.dylib | `_$s10Foundation4UUIDV10uuidStringSSvg` | 10 | 1password, alt-tab, claude, iina |
| libswiftFoundation.dylib | `_$s10Foundation4UUIDVACycfC` | 10 | 1password, alt-tab, claude, iina |
| libswiftFoundation.dylib | `_$s10Foundation4UUIDVMn` | 10 | 1password, claude, iina, linearmouse |
| libswiftFoundation.dylib | `_$s10Foundation6LocaleVMn` | 10 | alt-tab, claude, hex-fiend, iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE6format_S2Sh_s7CVarArg_pdtcfC` | 10 | 1password, alt-tab, antigravity, claude |
| libswiftFoundation.dylib | `_$sSSs7CVarArg10FoundationMc` | 10 | 1password, alt-tab, claude, cursor |
| libswiftFoundation.dylib | `_$sSa10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo7NSArrayC_SayxGSgztFZ` | 10 | 1password, alt-tab, claude, cursor |
| libswiftFoundation.dylib | `_$sSy10FoundationE20replacingOccurrences2of4with7options5rangeSSqd___qd_0_So22NSStringCompareOptionsVSnySS5IndexVGSgtSyRd__SyRd_0_r0_lF` | 10 | alt-tab, claude, hex-fiend, iina |
| libswiftFoundation.dylib | `_$sSy10FoundationE4data5using20allowLossyConversionAA4DataVSgSSAAE8EncodingV_SbtF` | 10 | 1password, alt-tab, claude, iina |
| libswiftFoundation.dylib | `_$sSy10FoundationE8containsySbqd__SyRd__lF` | 10 | 1password, alt-tab, claude, cursor |
| CoreFoundation.framework | `_CFURLGetFSRef` | 9 | cyberduck, firefox, floorp, flycut |
| Foundation.framework | `_$s10Foundation10URLRequestV10httpMethodSSSgvs` | 9 | betterdisplay, chatgpt, cmux, hammerspoon |
| Foundation.framework | `_$s10Foundation10URLRequestV8httpBodyAA4DataVSgvs` | 9 | betterdisplay, chatgpt, cmux, hammerspoon |
| Foundation.framework | `_$s10Foundation10URLRequestVMn` | 9 | betterdisplay, chatgpt, cmux, hammerspoon |
| Foundation.framework | `_$s10Foundation11JSONDecoderC20DateDecodingStrategyOMa` | 9 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation11JSONDecoderC20dateDecodingStrategyAC04DatedE0OvsTj` | 9 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation11JSONDecoderCMn` | 9 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation11JSONEncoderC16OutputFormattingV13prettyPrintedAEvgZ` | 9 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation11JSONEncoderC20DateEncodingStrategyOMa` | 9 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation11JSONEncoderC20dateEncodingStrategyAC04DatedE0OvsTj` | 9 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation12CharacterSetV12charactersInACSSh_tcfC` | 9 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation12CharacterSetV13alphanumericsACvgZ` | 9 | betterdisplay, chatgpt, cmux, coteditor |
| Foundation.framework | `_$s10Foundation12CharacterSetV8containsySbs7UnicodeO6ScalarVF` | 9 | betterdisplay, chatgpt, cmux, coteditor |
| Foundation.framework | `_$s10Foundation12URLQueryItemV4name5valueACSSh_SSSghtcfC` | 9 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation12URLQueryItemVMn` | 9 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation13URLComponentsV10queryItemsSayAA12URLQueryItemVGSgvs` | 9 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation13URLComponentsV3urlAA3URLVSgvg` | 9 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation16AttributedStringV_10attributesACSS_AA18AttributeContainerVtcfC` | 9 | 1password, chatgpt, cmux, coteditor |
| Foundation.framework | `_$s10Foundation18AttributeContainerVACycfC` | 9 | 1password, chatgpt, cmux, coteditor |
| Foundation.framework | `_$s10Foundation18AttributeContainerVMa` | 9 | 1password, chatgpt, cmux, coteditor |
| Foundation.framework | `_$s10Foundation21_BridgedStoredNSErrorPAAE_8userInfox4CodeQz_SDySSypGtcfC` | 9 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation23LocalizedStringResourceVMa` | 9 | 1password, betterdisplay, clipy, cmux |
| Foundation.framework | `_$s10Foundation25NSFastEnumerationIteratorVStAAMc` | 9 | betterdisplay, chatgpt, claude, cmux |
| Foundation.framework | `_$s10Foundation3URLV06isFileB0Sbvg` | 9 | chatgpt, cmux, coteditor, ghostty |
| Foundation.framework | `_$s10Foundation3URLV13DirectoryHintO13inferFromPathyA2EmFWC` | 9 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation3URLV13DirectoryHintOMa` | 9 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation3URLV22appendingPathExtensionyACSSF` | 9 | betterdisplay, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation3URLV4hostSSSgvg` | 9 | betterdisplay, chatgpt, cmux, ghostty |
| Foundation.framework | `_$s10Foundation4DataV8IteratorVStAAMc` | 9 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation4DataVAA0B8ProtocolAAMc` | 9 | betterdisplay, chatgpt, cmux, coteditor |
| Foundation.framework | `_$s10Foundation4DateV11distantPastACvgZ` | 9 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation4DateV1goiySbAC_ACtFZ` | 9 | betterdisplay, chatgpt, cmux, ghostty |
| Foundation.framework | `_$s10Foundation4DateVSLAAMc` | 9 | chatgpt, clipy, cmux, iterm2 |
| Foundation.framework | `_$s10Foundation4DateVSQAAMc` | 9 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation4DateVs23CustomStringConvertibleAAMc` | 9 | betterdisplay, chatgpt, claude, cmux |
| Foundation.framework | `_$s10Foundation4UUIDV36_unconditionallyBridgeFromObjectiveCyACSo6NSUUIDCSgFZ` | 9 | betterdisplay, chatgpt, claude, clipy |
| Foundation.framework | `_$s10Foundation4UUIDVSEAAMc` | 9 | chatgpt, claude, cmux, coteditor |
| Foundation.framework | `_$s10Foundation4UUIDVSeAAMc` | 9 | betterdisplay, chatgpt, cmux, coteditor |
| Foundation.framework | `_$s10Foundation5NSLogyySS_s7CVarArg_pdtF` | 9 | betterdisplay, cmux, hiddenbar, iterm2 |
| Foundation.framework | `_$s10Foundation6LocaleV10identifierSSvg` | 9 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation6LocaleV19autoupdatingCurrentACvgZ` | 9 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation8IndexSetVMn` | 9 | clipy, cmux, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation8TimeZoneV19_bridgeToObjectiveCSo06NSTimeC0CyF` | 9 | 1password, betterdisplay, chatgpt, clipy |
| Foundation.framework | `_$s10Foundation8TimeZoneV7currentACvgZ` | 9 | betterdisplay, chatgpt, cmux, coteditor |
| Foundation.framework | `_$s10Foundation8TimeZoneVMn` | 9 | 1password, betterdisplay, chatgpt, clipy |
| Foundation.framework | `_$sSS10FoundationE10contentsOf8encodingSSAA3URLVh_SSAAE8EncodingVtKcfC` | 9 | chatgpt, cmux, coteditor, iterm2 |
| Foundation.framework | `_$sSS10FoundationE17LocalizationValueV13stringLiteralACSS_tcfC` | 9 | 1password, betterdisplay, clipy, cmux |
| Foundation.framework | `_$sSS10FoundationEySSSo8NSStringCcfC` | 9 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$sSi10FoundationE26_forceBridgeFromObjectiveC_6resultySo8NSNumberC_SiSgztFZ` | 9 | betterdisplay, cmux, coteditor, ghostty |
| Foundation.framework | `_$sSi10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo8NSNumberC_SiSgztFZ` | 9 | betterdisplay, cmux, coteditor, ghostty |
| Foundation.framework | `_$sSi10FoundationE36_unconditionallyBridgeFromObjectiveCySiSo8NSNumberCSgFZ` | 9 | betterdisplay, cmux, coteditor, ghostty |
| Foundation.framework | `_$sSo8NSNumberC10FoundationE14integerLiteralABSi_tcfC` | 9 | betterdisplay, cmux, coteditor, iterm2 |
| Foundation.framework | `_$sSy10FoundationE24localizedStandardCompareySo18NSComparisonResultVqd__SyRd__lF` | 9 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_OBJC_CLASS_$_NSConnection` | 9 | firefox, floorp, mullvad-browser, sequel-ace |
| libswiftFoundation.dylib | `_$s10Foundation10CocoaErrorV4CodeVMa` | 9 | alt-tab, drawio, iina, krita |
| libswiftFoundation.dylib | `_$s10Foundation12NotificationV8userInfoSDys11AnyHashableVypGSgvg` | 9 | 1password, alt-tab, claude, iina |
| libswiftFoundation.dylib | `_$s10Foundation14LocalizedErrorMp` | 9 | alt-tab, claude, drawio, hex-fiend |
| libswiftFoundation.dylib | `_$s10Foundation15ContiguousBytesP010withUnsafeC0yqd__qd__SWKXEKlFTj` | 9 | alt-tab, claude, github, iina |
| libswiftFoundation.dylib | `_$s10Foundation3URLV15fileURLWithPath11isDirectoryACSSh_SbtcfC` | 9 | alt-tab, claude, iina, linearmouse |
| libswiftFoundation.dylib | `_$s10Foundation3URLVSQAAMc` | 9 | 1password, alt-tab, claude, iina |
| libswiftFoundation.dylib | `_$s10Foundation4DataV19base64EncodedString7optionsSSSo27NSDataBase64EncodingOptionsV_tF` | 9 | 1password, alt-tab, claude, hex-fiend |
| libswiftFoundation.dylib | `_$s10Foundation4DataV5write2to7optionsyAA3URLV_So20NSDataWritingOptionsVtKF` | 9 | alt-tab, claude, cursor, iina |
| libswiftFoundation.dylib | `_$s10Foundation4DateVSLAAMc` | 9 | alt-tab, claude, cursor, iina |
| libswiftFoundation.dylib | `_$sSb10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF` | 9 | 1password, alt-tab, drawio, iina |
| libswiftFoundation.dylib | `_$sSi10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF` | 9 | alt-tab, claude, hex-fiend, iina |
| CoreFoundation.framework | `_CFBundleCopyExecutableArchitecturesForURL` | 8 | audacity, firefox, floorp, mullvad-browser |
| CoreFoundation.framework | `_NSURLVolumeIsReadOnlyKey` | 8 | audacity, bitwarden, hammerspoon, kicad |
| Foundation.framework | `_$s10Foundation10NSNotFoundSivg` | 8 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation11JSONEncoderC16OutputFormattingVs10SetAlgebraAAMc` | 8 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation12CharacterSetV5unionyA2CF` | 8 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation12CharacterSetV8invertedACvg` | 8 | betterdisplay, chatgpt, cmux, coteditor |
| Foundation.framework | `_$s10Foundation12NotificationV4name6object8userInfoACSo18NSNotificationNamea_ypSgSDys11AnyHashableVypGSgtcfC` | 8 | cmux, coteditor, ghostty, hiddenbar |
| Foundation.framework | `_$s10Foundation12URLQueryItemV4nameSSvg` | 8 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation12URLQueryItemV5valueSSSgvg` | 8 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation13CustomNSErrorMp` | 8 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation13CustomNSErrorP11errorDomainSSvgZTq` | 8 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation13CustomNSErrorP13errorUserInfoSDySSypGvgTq` | 8 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation13CustomNSErrorP9errorCodeSivgTq` | 8 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation13CustomNSErrorPs5ErrorTb` | 8 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation13URLComponentsV4hostSSSgvs` | 8 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation13URLComponentsV4pathSSvs` | 8 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation13URLComponentsV4userSSSgvs` | 8 | betterdisplay, chatgpt, cmux, hammerspoon |
| Foundation.framework | `_$s10Foundation13URLComponentsV6schemeSSSgvs` | 8 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation13URLComponentsV6stringACSgSSh_tcfC` | 8 | 1password, betterdisplay, chatgpt, cmux |
| Foundation.framework | `_$s10Foundation13URLComponentsV8passwordSSSgvs` | 8 | betterdisplay, chatgpt, cmux, hammerspoon |
| Foundation.framework | `_$s10Foundation13URLComponentsVACycfC` | 8 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation13__DataStorageC9_capacitySivM` | 8 | chatgpt, claude, clipy, cmux |
| Foundation.framework | `_$s10Foundation16AttributedStringVAA0bC8ProtocolAAMc` | 8 | chatgpt, cmux, coteditor, ghostty |
| Foundation.framework | `_$s10Foundation23LocalizedStringResourceVMn` | 8 | betterdisplay, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation25NSFastEnumerationIteratorV4nextypSgyF` | 8 | chatgpt, claude, clipy, cmux |
| Foundation.framework | `_$s10Foundation3URLV016standardizedFileB0ACvg` | 8 | chatgpt, cmux, coteditor, ghostty |
| Foundation.framework | `_$s10Foundation3URLV14pathComponentsSaySSGvg` | 8 | betterdisplay, cmux, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation3URLV16hasDirectoryPathSbvg` | 8 | chatgpt, cmux, coteditor, ghostty |
| Foundation.framework | `_$s10Foundation3URLV23resolvingSymlinksInPathACyF` | 8 | chatgpt, cmux, coteditor, ghostty |
| Foundation.framework | `_$s10Foundation3URLVSEAAMc` | 8 | chatgpt, cmux, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation4DataV11DeallocatorO12_deallocatoryySv_Sitcvg` | 8 | chatgpt, claude, clipy, cmux |
| Foundation.framework | `_$s10Foundation4DataV11DeallocatorO4noneyA2EmFWC` | 8 | chatgpt, claude, clipy, cmux |
| Foundation.framework | `_$s10Foundation4DataV11DeallocatorOMa` | 8 | chatgpt, claude, clipy, cmux |
| Foundation.framework | `_$s10Foundation4DataV15_RepresentationOyACSnySiGcig` | 8 | chatgpt, cmux, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation4DataV15_RepresentationOys5UInt8VSicig` | 8 | betterdisplay, chatgpt, claude, cmux |
| Foundation.framework | `_$s10Foundation4DataV7subdata2inACSnySiG_tF` | 8 | betterdisplay, claude, cmux, hammerspoon |
| Foundation.framework | `_$s10Foundation4DateV026timeIntervalSinceReferenceB0ACSd_tcfC` | 8 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation4DateV20timeIntervalSinceNowACSd_tcfC` | 8 | chatgpt, cmux, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation4DateV20timeIntervalSinceNowSdvg` | 8 | chatgpt, cmux, iterm2, maccy |
| Foundation.framework | `_$s10Foundation4DateV3nowACvgZ` | 8 | chatgpt, cmux, coteditor, jordanbaird-ice |
| Foundation.framework | `_$s10Foundation4UUIDVs23CustomStringConvertibleAAMc` | 8 | cmux, ghostty, iterm2, maccy |
| Foundation.framework | `_$s10Foundation8IndexSetV19_bridgeToObjectiveCSo07NSIndexC0CyF` | 8 | clipy, cmux, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation8IndexSetV7integerACSi_tcfC` | 8 | clipy, cmux, coteditor, iterm2 |
| Foundation.framework | `_$s12CoreGraphics7CGFloatV10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF` | 8 | 1password, betterdisplay, chatgpt, claude |
| Foundation.framework | `_$sSS10FoundationE8EncodingV8rawValueSuvg` | 8 | betterdisplay, chatgpt, cmux, coteditor |
| Foundation.framework | `_$sSo12NSDictionaryC10FoundationE17dictionaryLiteralAByp_yptd_tcfC` | 8 | betterdisplay, cmux, hammerspoon, iterm2 |
| Foundation.framework | `_$sSo12NSFileHandleC10FoundationE4read9upToCountAC4DataVSgSi_tKF` | 8 | chatgpt, claude, clipy, cmux |
| Foundation.framework | `_$sSo12NSURLSessionC10FoundationE4data3for8delegateAC4DataV_So13NSURLResponseCtAC10URLRequestV_So0A12TaskDelegate_pSgtYaKF` | 8 | betterdisplay, chatgpt, cmux, iterm2 |
| Foundation.framework | `_$sSo12NSURLSessionC10FoundationE4data3for8delegateAC4DataV_So13NSURLResponseCtAC10URLRequestV_So0A12TaskDelegate_pSgtYaKFTu` | 8 | betterdisplay, chatgpt, cmux, iterm2 |
| Foundation.framework | `_$sSo8NSStringC10FoundationE13stringLiteralABs12StaticStringV_tcfC` | 8 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$sSy10FoundationE32localizedCaseInsensitiveContainsySbqd__SyRd__lF` | 8 | 1password, betterdisplay, cmux, ghostty |
| Foundation.framework | `_$sSy10FoundationE5write2to10atomically8encodingyAA3URLV_SbSSAAE8EncodingVtKF` | 8 | chatgpt, cmux, iterm2, orbstack |
| Foundation.framework | `_$ss5ErrorP10FoundationAC13CustomNSErrorRzrlE7_domainSSvg` | 8 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_NSKeyValueChangeIndexesKey` | 8 | bitwarden, linearmouse, localsend, maccy |
| Foundation.framework | `_NSPointFromString` | 8 | android-studio, dbeaver-community, hex-fiend, iterm2 |
| libswiftFoundation.dylib | `_$s10Foundation10URLRequestV3url11cachePolicy15timeoutIntervalAcA3URLV_So017NSURLRequestCacheE0VSdtcfC` | 8 | alt-tab, claude, iina, obsidian |
| libswiftFoundation.dylib | `_$s10Foundation10URLRequestVMa` | 8 | alt-tab, claude, iina, obsidian |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV11whitespacesACvgZ` | 8 | alt-tab, claude, iina, linearmouse |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV12charactersInACSSh_tcfC` | 8 | 1password, alt-tab, claude, iina |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV22whitespacesAndNewlinesACvgZ` | 8 | alt-tab, claude, iina, linearmouse |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetVMa` | 8 | alt-tab, claude, iina, linearmouse |
| libswiftFoundation.dylib | `_$s10Foundation14LocalizedErrorP10helpAnchorSSSgvgTq` | 8 | alt-tab, claude, drawio, iina |
| libswiftFoundation.dylib | `_$s10Foundation14LocalizedErrorP13failureReasonSSSgvgTq` | 8 | alt-tab, claude, drawio, iina |
| libswiftFoundation.dylib | `_$s10Foundation14LocalizedErrorP16errorDescriptionSSSgvgTq` | 8 | alt-tab, claude, drawio, iina |
| libswiftFoundation.dylib | `_$s10Foundation14LocalizedErrorP18recoverySuggestionSSSgvgTq` | 8 | alt-tab, claude, drawio, iina |
| libswiftFoundation.dylib | `_$s10Foundation14LocalizedErrorPAAE10helpAnchorSSSgvg` | 8 | alt-tab, claude, drawio, iina |
| libswiftFoundation.dylib | `_$s10Foundation14LocalizedErrorPAAE13failureReasonSSSgvg` | 8 | alt-tab, claude, drawio, iina |
| libswiftFoundation.dylib | `_$s10Foundation14LocalizedErrorPAAE18recoverySuggestionSSSgvg` | 8 | alt-tab, claude, drawio, iina |
| libswiftFoundation.dylib | `_$s10Foundation14LocalizedErrorPs0C0Tb` | 8 | alt-tab, claude, drawio, iina |
| libswiftFoundation.dylib | `_$s10Foundation17URLResourceValuesVMa` | 8 | claude, iina, localsend, obsidian |
| libswiftFoundation.dylib | `_$s10Foundation21_BridgedStoredNSErrorPAAE_8userInfox4CodeQz_SDySSypGtcfC` | 8 | alt-tab, claude, drawio, iina |
| libswiftFoundation.dylib | `_$s10Foundation3URLV14resourceValues7forKeysAA011URLResourceD0VShySo16NSURLResourceKeyaG_tKF` | 8 | claude, iina, localsend, obsidian |
| libswiftFoundation.dylib | `_$s10Foundation3URLV21deletingPathExtensionACyF` | 8 | 1password, claude, iina, linearmouse |
| libswiftFoundation.dylib | `_$s10Foundation3URLV22appendingPathComponent_11isDirectoryACSS_SbtF` | 8 | 1password, claude, iina, linearmouse |
| libswiftFoundation.dylib | `_$s10Foundation3URLV6schemeSSSgvg` | 8 | 1password, alt-tab, claude, iina |
| libswiftFoundation.dylib | `_$s10Foundation3URLVs23CustomStringConvertibleAAMc` | 8 | 1password, drawio, hex-fiend, iina |
| libswiftFoundation.dylib | `_$s10Foundation4DataV10contentsOf7optionsAcA3URLVh_So20NSDataReadingOptionsVtKcfC` | 8 | alt-tab, claude, hex-fiend, iina |
| libswiftFoundation.dylib | `_$s10Foundation4DataV15_RepresentationO15replaceSubrange_4with5countySnySiG_SVSgSitF` | 8 | alt-tab, claude, cursor, iina |
| libswiftFoundation.dylib | `_$s10Foundation4DataV6appendyyACF` | 8 | 1password, claude, cursor, iina |
| libswiftFoundation.dylib | `_$s10Foundation4DataVAA0B8ProtocolAAMc` | 8 | claude, cursor, github, stats |
| libswiftFoundation.dylib | `_$s10Foundation4DateV17timeIntervalSinceySdACF` | 8 | alt-tab, claude, cursor, iina |
| libswiftFoundation.dylib | `_$s10Foundation4DateV1loiySbAC_ACtFZ` | 8 | claude, cursor, iina, linearmouse |
| libswiftFoundation.dylib | `_$s10Foundation4DateVSQAAMc` | 8 | alt-tab, claude, cursor, iina |
| libswiftFoundation.dylib | `_$sSh10FoundationE19_bridgeToObjectiveCSo5NSSetCyF` | 8 | 1password, claude, iina, linearmouse |
| libswiftFoundation.dylib | `_$sSo8NSObjectC10Foundation27_KeyValueCodingAndObservingACWP` | 8 | alt-tab, linearmouse, localsend, obsidian |
| libswiftFoundation.dylib | `_$sSy10FoundationE18trimmingCharacters2inSSAA12CharacterSetV_tF` | 8 | alt-tab, claude, iina, linearmouse |
| libswiftFoundation.dylib | `_$sSy10FoundationE5range2of7optionsAB6localeSnySS5IndexVGSgqd___So22NSStringCompareOptionsVAiA6LocaleVSgtSyRd__lF` | 8 | alt-tab, claude, hex-fiend, iina |
| CoreFoundation.framework | `_NSURLIsExecutableKey` | 7 | bitwarden, cmux, coteditor, ghostty |
| CoreFoundation.framework | `_NSURLVolumeIsRemovableKey` | 7 | audacity, bitwarden, hammerspoon, kicad |
| CoreFoundation.framework | `_NSURLVolumeLocalizedNameKey` | 7 | audacity, bitwarden, hammerspoon, kicad |
| CoreFoundation.framework | `_NSURLVolumeSupportsCaseSensitiveNamesKey` | 7 | bitwarden, godot, iterm2, monitorcontrol |
| CoreFoundation.framework | `_kCFURLIsHiddenKey` | 7 | db-browser-for-sqlite, dolphin, krita, lmms |
| CoreFoundation.framework | `_kCFURLVolumeSupportsCaseSensitiveNamesKey` | 7 | audacity, keepassxc, musescore, obs |
| Foundation.framework | `_$s10Foundation10POSIXErrorVMa` | 7 | chatgpt, cmux, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation11JSONDecoderC20DateDecodingStrategyO6customyAeA0C0Vs7Decoder_pKccAEmFWC` | 7 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation11JSONEncoderC16OutputFormattingVMn` | 7 | betterdisplay, chatgpt, cmux, coteditor |
| Foundation.framework | `_$s10Foundation12CharacterSetV6insert12charactersInySS_tF` | 7 | betterdisplay, cmux, hammerspoon, iterm2 |
| Foundation.framework | `_$s10Foundation12CharacterSetVMn` | 7 | chatgpt, cmux, iterm2, maccy |
| Foundation.framework | `_$s10Foundation12NotificationV19_bridgeToObjectiveCSo14NSNotificationCyF` | 7 | cmux, coteditor, hiddenbar, iterm2 |
| Foundation.framework | `_$s10Foundation12NotificationVMn` | 7 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation13URLComponentsV4hostSSSgvg` | 7 | 1password, chatgpt, cmux, iterm2 |
| Foundation.framework | `_$s10Foundation13URLComponentsV4pathSSvg` | 7 | betterdisplay, chatgpt, cmux, ghostty |
| Foundation.framework | `_$s10Foundation13URLComponentsV5querySSSgvg` | 7 | betterdisplay, cmux, hammerspoon, iterm2 |
| Foundation.framework | `_$s10Foundation13URLComponentsV6stringSSSgvg` | 7 | betterdisplay, chatgpt, cmux, hammerspoon |
| Foundation.framework | `_$s10Foundation13URLComponentsV8fragmentSSSgvg` | 7 | betterdisplay, chatgpt, cmux, hammerspoon |
| Foundation.framework | `_$s10Foundation14DateComponentsVMa` | 7 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation14LocalizedErrorP16errorDescriptionSSSgvgTj` | 7 | chatgpt, cmux, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation16AttributedStringV5IndexVMa` | 7 | cmux, coteditor, ghostty, maccy |
| Foundation.framework | `_$s10Foundation16AttributedStringV5IndexVMn` | 7 | cmux, coteditor, ghostty, maccy |
| Foundation.framework | `_$s10Foundation16AttributedStringV8markdown7options7baseURLACSS_AC22MarkdownParsingOptionsVAA0G0VSgtKcfC` | 7 | 1password, cmux, coteditor, ghostty |
| Foundation.framework | `_$s10Foundation21NSKeyValueObservationC10invalidateyyFTj` | 7 | betterdisplay, chatgpt, cmux, ghostty |
| Foundation.framework | `_$s10Foundation21_BridgedStoredNSErrorPAAE4code4CodeQzvg` | 7 | clipy, cmux, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation3URLV24checkResourceIsReachableSbyKF` | 7 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation3URLV35stopAccessingSecurityScopedResourceyyF` | 7 | clipy, cmux, coteditor, jordanbaird-ice |
| Foundation.framework | `_$s10Foundation3URLV36startAccessingSecurityScopedResourceSbyF` | 7 | clipy, cmux, coteditor, jordanbaird-ice |
| Foundation.framework | `_$s10Foundation3URLV4portSiSgvg` | 7 | betterdisplay, chatgpt, cmux, iterm2 |
| Foundation.framework | `_$s10Foundation3URLV8filePath13directoryHint10relativeToACSS_AC09DirectoryF0OACSgtcfC` | 7 | chatgpt, clipy, coteditor, ghostty |
| Foundation.framework | `_$s10Foundation4DataV8IteratorV4nexts5UInt8VSgyF` | 7 | 1password, betterdisplay, chatgpt, claude |
| Foundation.framework | `_$s10Foundation4DataVAA15ContiguousBytesAAWP` | 7 | 1password, betterdisplay, chatgpt, cmux |
| Foundation.framework | `_$s10Foundation4UUIDV4uuidACs5UInt8V_A15Ft_tcfC` | 7 | chatgpt, claude, clipy, cmux |
| Foundation.framework | `_$s10Foundation8CalendarV10IdentifierO9gregorianyA2EmFWC` | 7 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation8CalendarV10IdentifierOMa` | 7 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation8CalendarV10identifierA2C10IdentifierOh_tcfC` | 7 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation8CalendarV7currentACvgZ` | 7 | betterdisplay, chatgpt, cmux, iterm2 |
| Foundation.framework | `_$s10Foundation8CalendarV9ComponentOMa` | 7 | betterdisplay, chatgpt, cmux, iterm2 |
| Foundation.framework | `_$s10Foundation8IndexSetV0B0VMa` | 7 | clipy, cmux, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation8IndexSetV0B0VSQAAMc` | 7 | clipy, cmux, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation8IndexSetV36_unconditionallyBridgeFromObjectiveCyACSo07NSIndexC0CSgFZ` | 7 | clipy, cmux, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation8IndexSetV8containsySbSiF` | 7 | clipy, cmux, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation8IndexSetVSlAAMc` | 7 | clipy, cmux, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation8TimeZoneV14secondsFromGMT3forSiAA4DateV_tF` | 7 | betterdisplay, clipy, cmux, hammerspoon |
| Foundation.framework | `_$s10Foundation8TimeZoneV14secondsFromGMTACSgSi_tcfC` | 7 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$sSS10FoundationE14utf16CodeUnits5countSSSPys6UInt16VG_SitcfC` | 7 | betterdisplay, cmux, coteditor, iterm2 |
| Foundation.framework | `_$sSS10FoundationE8EncodingV2eeoiySbAC_ACtFZ` | 7 | betterdisplay, cmux, coteditor, hammerspoon |
| Foundation.framework | `_$sSS10FoundationE8EncodingV9isoLatin1ACvgZ` | 7 | betterdisplay, cmux, coteditor, hammerspoon |
| Foundation.framework | `_$sSayxG10Foundation15ContiguousBytesABs5UInt8VRszlMc` | 7 | betterdisplay, chatgpt, claude, cmux |
| Foundation.framework | `_$sSd10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF` | 7 | betterdisplay, chatgpt, clipy, cmux |
| Foundation.framework | `_$sSo12NSDictionaryC10FoundationE12makeIteratorAbCE0D0CyF` | 7 | betterdisplay, chatgpt, coteditor, iterm2 |
| Foundation.framework | `_$sSo12NSFileHandleC10FoundationE5write10contentsOfyx_tKAC12DataProtocolRzlF` | 7 | chatgpt, cmux, coteditor, iterm2 |
| Foundation.framework | `_$sSy10FoundationE16rangeOfCharacter4from7options0B0SnySS5IndexVGSgAA0D3SetV_So22NSStringCompareOptionsVAItF` | 7 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$ss5ErrorP10FoundationAC13CustomNSErrorRzrlE5_codeSivg` | 7 | chatgpt, clipy, coteditor, iterm2 |
| libswiftFoundation.dylib | `_$s10Foundation10URLRequestV19_bridgeToObjectiveCSo12NSURLRequestCyF` | 7 | alt-tab, claude, iina, obsidian |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC16OutputFormattingVMa` | 7 | 1password, claude, hex-fiend, linearmouse |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC16outputFormattingAC06OutputD0VvsTj` | 7 | 1password, claude, hex-fiend, linearmouse |
| libswiftFoundation.dylib | `_$s10Foundation12NotificationV6objectypSgvg` | 7 | alt-tab, iina, linearmouse, stats |
| libswiftFoundation.dylib | `_$s10Foundation12URLQueryItemVMa` | 7 | alt-tab, claude, iina, linearmouse |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsVMa` | 7 | alt-tab, claude, iina, linearmouse |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsVMn` | 7 | alt-tab, claude, iina, linearmouse |
| libswiftFoundation.dylib | `_$s10Foundation13__DataStorageC5bytes6length4copy11deallocator6offsetACSvSg_SiSbySv_SitcSgSitcfc` | 7 | alt-tab, claude, iina, stats |
| libswiftFoundation.dylib | `_$s10Foundation27_KeyValueCodingAndObservingPAAE7observe_7options13changeHandlerAA05NSKeyC11ObservationCs0B4PathCyxqd__G_So0kcF7OptionsVyx_AA0kC14ObservedChangeVyqd__GtctlF` | 7 | alt-tab, localsend, obsidian, stats |
| libswiftFoundation.dylib | `_$s10Foundation4DataV10LargeSliceV21ensureUniqueReferenceyyF` | 7 | alt-tab, claude, iina, stats |
| libswiftFoundation.dylib | `_$s10Foundation4DataV13base64Encoded7optionsACSgSSh_So27NSDataBase64DecodingOptionsVtcfC` | 7 | 1password, iina, localsend, stats |
| libswiftFoundation.dylib | `_$s10Foundation4DataV15_RepresentationON` | 7 | alt-tab, claude, iina, stats |
| libswiftFoundation.dylib | `_$s10Foundation4DataV15_RepresentationOyACSnySiGcig` | 7 | 1password, alt-tab, claude, cursor |
| libswiftFoundation.dylib | `_$s10Foundation4DataV8IteratorVMa` | 7 | 1password, claude, linearmouse, localsend |
| libswiftFoundation.dylib | `_$s10Foundation4DateV18addingTimeIntervalyACSdF` | 7 | alt-tab, claude, cursor, linearmouse |
| libswiftFoundation.dylib | `_$s10Foundation4DateV1goiySbAC_ACtFZ` | 7 | alt-tab, claude, iina, stats |
| libswiftFoundation.dylib | `_$s10Foundation4DateV21timeIntervalSince1970ACSd_tcfC` | 7 | 1password, alt-tab, cursor, stats |
| libswiftFoundation.dylib | `_$s10Foundation4UUIDVSQAAMc` | 7 | 1password, claude, linearmouse, stats |
| libswiftFoundation.dylib | `_$s10Foundation5NSLogyySS_s7CVarArg_pdtF` | 7 | 1password, claude, cursor, iina |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetVMa` | 7 | alt-tab, iina, linearmouse, localsend |
| libswiftFoundation.dylib | `_$sSD10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo12NSDictionaryC_SDyxq_GSgztFZ` | 7 | 1password, alt-tab, claude, iina |
| libswiftFoundation.dylib | `_$sSn10FoundationSS5IndexVRszrlE_2inSnyACGSgSo8_NSRangeV_SShtcfC` | 7 | cursor, iina, linearmouse, stats |
| libswiftFoundation.dylib | `_$sSo8_NSRangeV10FoundationE_2inABx_q_tcSXRzSyR_SS5IndexV5BoundRtzr0_lufC` | 7 | alt-tab, cursor, linearmouse, stats |
| libswiftFoundation.dylib | `_$sSy10FoundationE10components11separatedBySaySSGqd___tSyRd__lF` | 7 | alt-tab, claude, iina, linearmouse |
| CoreFoundation.framework | `_CFBundleCopyPrivateFrameworksURL` | 6 | audacity, bitwarden, localsend, shotcut |
| CoreFoundation.framework | `_CFCharacterSetCreateWithCharactersInRange` | 6 | bitwarden, cmux, ghostty, monitorcontrol |
| CoreFoundation.framework | `_CFStringGetDoubleValue` | 6 | firefox, floorp, mullvad-browser, thunderbird |
| CoreFoundation.framework | `_CFStringGetIntValue` | 6 | firefox, floorp, mullvad-browser, thunderbird |
| CoreFoundation.framework | `_CFURLCreateFromFileSystemRepresentationRelativeToBase` | 6 | firefox, floorp, mullvad-browser, thunderbird |
| CoreFoundation.framework | `_CFURLEnumeratorCreateForDirectoryURL` | 6 | firefox, floorp, mullvad-browser, thunderbird |
| CoreFoundation.framework | `_kCFLocaleCalendarIdentifier` | 6 | android-studio, bitwarden, cyberduck, dbeaver-community |
| CoreFoundation.framework | `_kCFLocaleCurrentLocaleDidChangeNotification` | 6 | firefox, floorp, mullvad-browser, thunderbird |
| CoreFoundation.framework | `_kCFURLIsApplicationKey` | 6 | firefox, floorp, mullvad-browser, thunderbird |
| Foundation.framework | `_$s10Foundation10CocoaErrorV03_nsC0So7NSErrorCvg` | 6 | betterdisplay, chatgpt, claude, cmux |
| Foundation.framework | `_$s10Foundation10CocoaErrorVAA21_BridgedStoredNSErrorAAMc` | 6 | betterdisplay, chatgpt, claude, cmux |
| Foundation.framework | `_$s10Foundation10CocoaErrorVMa` | 6 | betterdisplay, chatgpt, claude, cmux |
| Foundation.framework | `_$s10Foundation10POSIXErrorV8_nsErrorSo7NSErrorCvg` | 6 | chatgpt, cmux, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation10POSIXErrorVAA21_BridgedStoredNSErrorAAMc` | 6 | chatgpt, cmux, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation10URLRequestV15timeoutIntervalSdvs` | 6 | betterdisplay, cmux, iterm2, raycast |
| Foundation.framework | `_$s10Foundation10URLRequestV5value18forHTTPHeaderFieldSSSgSS_tF` | 6 | betterdisplay, cmux, hammerspoon, orbstack |
| Foundation.framework | `_$s10Foundation10URLRequestV8httpBodyAA4DataVSgvg` | 6 | betterdisplay, cmux, hammerspoon, orbstack |
| Foundation.framework | `_$s10Foundation11JSONEncoderC16OutputFormattingV22withoutEscapingSlashesAEvgZ` | 6 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation11JSONEncoderC20DateEncodingStrategyO16secondsSince1970yA2EmFWC` | 6 | betterdisplay, cmux, hammerspoon, iterm2 |
| Foundation.framework | `_$s10Foundation12CharacterSetV13decimalDigitsACvgZ` | 6 | cmux, coteditor, iterm2, raycast |
| Foundation.framework | `_$s10Foundation12CharacterSetV16lowercaseLettersACvgZ` | 6 | clipy, cmux, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation12CharacterSetV21punctuationCharactersACvgZ` | 6 | betterdisplay, chatgpt, claude, cmux |
| Foundation.framework | `_$s10Foundation12NotificationV4nameSo18NSNotificationNameavg` | 6 | chatgpt, cmux, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation13URLComponentsV4portSiSgvs` | 6 | betterdisplay, chatgpt, cmux, raycast |
| Foundation.framework | `_$s10Foundation13URLComponentsV4userSSSgvg` | 6 | betterdisplay, cmux, hammerspoon, iterm2 |
| Foundation.framework | `_$s10Foundation13URLComponentsV8passwordSSSgvg` | 6 | betterdisplay, cmux, hammerspoon, iterm2 |
| Foundation.framework | `_$s10Foundation13__DataStorageC8capacityACSi_tcfc` | 6 | chatgpt, cmux, iterm2, orbstack |
| Foundation.framework | `_$s10Foundation14DateComponentsV3daySiSgvs` | 6 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation14DateComponentsV4hourSiSgvs` | 6 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation14DateComponentsV4yearSiSgvs` | 6 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation14DateComponentsV5monthSiSgvs` | 6 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation14DateComponentsV6minuteSiSgvs` | 6 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation14DateComponentsV6secondSiSgvs` | 6 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation14DateComponentsV8calendar8timeZone3era4year5month3day4hour6minute6second10nanosecond7weekday0O7Ordinal7quarter11weekOfMonth0rS4Year0h7ForWeeksU0AcA8CalendarVSg_AA04TimeF0VSgSiSgA13ZtcfC` | 6 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation14DateComponentsV8timeZoneAA04TimeE0VSgvs` | 6 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation14LocalizedErrorPAAE16errorDescriptionSSSgvg` | 6 | chatgpt, coteditor, iterm2, orbstack |
| Foundation.framework | `_$s10Foundation16AttributedStringV10charactersAC13CharacterViewVvg` | 6 | chatgpt, cmux, coteditor, ghostty |
| Foundation.framework | `_$s10Foundation16AttributedStringV13CharacterViewVMa` | 6 | chatgpt, cmux, coteditor, ghostty |
| Foundation.framework | `_$s10Foundation16AttributedStringV22MarkdownParsingOptionsV17InterpretedSyntaxO4fullyA2GmFWC` | 6 | 1password, cmux, coteditor, ghostty |
| Foundation.framework | `_$s10Foundation16AttributedStringV5IndexVSLAAMc` | 6 | cmux, coteditor, ghostty, maccy |
| Foundation.framework | `_$s10Foundation16AttributedStringVyAA0B9SubstringVxcSXRzAC5IndexV5BoundRtzluiM` | 6 | cmux, coteditor, ghostty, maccy |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV11isDirectorySbSgvg` | 6 | chatgpt, cmux, coteditor, ghostty |
| Foundation.framework | `_$s10Foundation18_ErrorCodeProtocolPAAE2teoiySbx_s0B0_ptFZ` | 6 | chatgpt, clipy, cmux, iterm2 |
| Foundation.framework | `_$s10Foundation26_ObjectiveCBridgeableErrorMp` | 6 | clipy, coteditor, iterm2, jordanbaird-ice |
| Foundation.framework | `_$s10Foundation26_ObjectiveCBridgeableErrorP15_bridgedNSErrorxSgSo0F0Ch_tcfCTq` | 6 | clipy, coteditor, iterm2, jordanbaird-ice |
| Foundation.framework | `_$s10Foundation26_ObjectiveCBridgeableErrorPs0D0Tb` | 6 | clipy, coteditor, iterm2, jordanbaird-ice |
| Foundation.framework | `_$s10Foundation3URLV15fileURLWithPath11isDirectoryACSSh_SbtcfC` | 6 | chatgpt, clipy, cmux, iterm2 |
| Foundation.framework | `_$s10Foundation3URLV4path14percentEncodedSSSb_tF` | 6 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation3URLV4userSSSgvg` | 6 | betterdisplay, cmux, hammerspoon, iterm2 |
| Foundation.framework | `_$s10Foundation3URLV9appending4path13directoryHintACx_AC09DirectoryF0OtSyRzlF` | 6 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation3URLVSeAAMc` | 6 | chatgpt, cmux, iterm2, maccy |
| Foundation.framework | `_$s10Foundation4DataV15_RepresentationO10resetBytes2inySnySiG_tF` | 6 | betterdisplay, cmux, iterm2, orbstack |
| Foundation.framework | `_$s10Foundation4DateV13distantFutureACvgZ` | 6 | clipy, cmux, iterm2, raycast |
| Foundation.framework | `_$s10Foundation4DateVSHAAMc` | 6 | chatgpt, clipy, cmux, maccy |
| Foundation.framework | `_$s10Foundation4UUIDV4uuids5UInt8V_A15Ftvg` | 6 | claude, clipy, cmux, ghostty |
| Foundation.framework | `_$s10Foundation6LocaleV18preferredLanguagesSaySSGvgZ` | 6 | chatgpt, clipy, cmux, iterm2 |
| Foundation.framework | `_$s10Foundation8CalendarV10startOfDay3forAA4DateVAG_tF` | 6 | chatgpt, clipy, cmux, iterm2 |
| Foundation.framework | `_$s10Foundation8CalendarV9ComponentO4yearyA2EmFWC` | 6 | betterdisplay, chatgpt, cmux, iterm2 |
| Foundation.framework | `_$s10Foundation8CalendarVMn` | 6 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation8IndexSetV5countSivg` | 6 | cmux, coteditor, iterm2, orbstack |
| Foundation.framework | `_$s10Foundation8IndexSetV6insertySb8inserted_Si17memberAfterInserttSiF` | 6 | cmux, coteditor, iterm2, maccy |
| Foundation.framework | `_$s10Foundation8IndexSetV7isEmptySbvg` | 6 | cmux, coteditor, iterm2, maccy |
| Foundation.framework | `_$s10Foundation8IndexSetVACycfC` | 6 | cmux, coteditor, iterm2, maccy |
| Foundation.framework | `_$s10Foundation8IndexSetVSTAAMc` | 6 | cmux, coteditor, iterm2, maccy |
| Foundation.framework | `_$s10Foundation8IndexSetVs0C7AlgebraAAMc` | 6 | cmux, coteditor, iterm2, jordanbaird-ice |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeVMa` | 6 | betterdisplay, chatgpt, cmux, iterm2 |
| Foundation.framework | `_$s10Foundation8URLErrorVAA21_BridgedStoredNSErrorAAMc` | 6 | betterdisplay, chatgpt, cmux, iterm2 |
| Foundation.framework | `_$s10Foundation8URLErrorVMa` | 6 | betterdisplay, chatgpt, cmux, iterm2 |
| Foundation.framework | `_$s10Foundation9IndexPathVMa` | 6 | chatgpt, clipy, cmux, iterm2 |
| Foundation.framework | `_$sSS10FoundationE8EncodingV17utf16LittleEndianACvgZ` | 6 | betterdisplay, cmux, coteditor, iterm2 |
| Foundation.framework | `_$sSS10FoundationE9localized5table6bundle6locale7commentS2SAAE17LocalizationValueV_SSSgSo8NSBundleCSgAA6LocaleVs12StaticStringVSgtcfC` | 6 | betterdisplay, clipy, coteditor, maccy |
| Foundation.framework | `_$sSf10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF` | 6 | cmux, coteditor, iterm2, raycast |
| Foundation.framework | `_$sSo12NSDictionaryC10FoundationE8IteratorC4nextyp3key_yp5valuetSgyF` | 6 | betterdisplay, chatgpt, iterm2, orbstack |
| Foundation.framework | `_$sSo12NSEnumeratorC10FoundationE12makeIteratorAC017NSFastEnumerationD0VyF` | 6 | chatgpt, claude, cmux, iterm2 |
| Foundation.framework | `_$sSo13NSFileManagerC10FoundationE10enumerator2at26includingPropertiesForKeys7options12errorHandlerSo21NSDirectoryEnumeratorCSgAC3URLV_SaySo16NSURLResourceKeyaGSgSo0M18EnumerationOptionsVSbAM_s5Error_ptcSgtF` | 6 | chatgpt, claude, cmux, iterm2 |
| Foundation.framework | `_$sSy10FoundationE13lengthOfBytes5usingSiSSAAE8EncodingV_tF` | 6 | cmux, coteditor, docker-desktop, ghostty |
| Foundation.framework | `_$sSy10FoundationE7padding8toLength7withPad10startingAtSSSi_qd__SitSyRd__lF` | 6 | 1password, claude, cmux, coteditor |
| Foundation.framework | `_NSAppleScriptErrorMessage` | 6 | docker-desktop, hex-fiend, obs, orbstack |
| Foundation.framework | `_NSAppleScriptErrorNumber` | 6 | docker-desktop, hex-fiend, iterm2, linearmouse |
| Foundation.framework | `_NSProgressFileOperationKindKey` | 6 | firefox, floorp, mullvad-browser, thunderbird |
| Foundation.framework | `_NSProgressFileURLKey` | 6 | firefox, floorp, mullvad-browser, thunderbird |
| Foundation.framework | `_OBJC_CLASS_$_NSSocketPort` | 6 | firefox, floorp, mullvad-browser, thunderbird |
| Foundation.framework | `_OBJC_CLASS_$_NSSocketPortNameServer` | 6 | firefox, floorp, mullvad-browser, thunderbird |
| Foundation.framework | `_OBJC_CLASS_$_NSURLFileTypeMappings` | 6 | firefox, floorp, mullvad-browser, thunderbird |
| libswiftFoundation.dylib | `_$s10Foundation10CocoaErrorVAA21_BridgedStoredNSErrorAAMc` | 6 | alt-tab, drawio, obsidian, stats |
| libswiftFoundation.dylib | `_$s10Foundation10CocoaErrorVMa` | 6 | alt-tab, drawio, obsidian, stats |
| libswiftFoundation.dylib | `_$s10Foundation10NSNotFoundSivg` | 6 | alt-tab, linearmouse, stats, utm |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC16OutputFormattingV10sortedKeysAEvgZ` | 6 | claude, hex-fiend, linearmouse, stats |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC16OutputFormattingVs10SetAlgebraAAMc` | 6 | 1password, claude, hex-fiend, linearmouse |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV8containsySbs7UnicodeO6ScalarVF` | 6 | alt-tab, claude, iina, utm |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV10queryItemsSayAA12URLQueryItemVGSgvg` | 6 | alt-tab, iina, linearmouse, utm |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV3url23resolvingAgainstBaseURLACSgAA0G0Vh_SbtcfC` | 6 | alt-tab, iina, linearmouse, utm |
| libswiftFoundation.dylib | `_$s10Foundation13__DataStorageC6lengthACSi_tcfc` | 6 | claude, iina, stats, utm |
| libswiftFoundation.dylib | `_$s10Foundation15ContiguousBytesMp` | 6 | alt-tab, claude, iina, stats |
| libswiftFoundation.dylib | `_$s10Foundation17URLResourceValuesV11isDirectorySbSgvg` | 6 | claude, iina, localsend, obsidian |
| libswiftFoundation.dylib | `_$s10Foundation17URLResourceValuesVMn` | 6 | claude, iina, localsend, utm |
| libswiftFoundation.dylib | `_$s10Foundation21NSKeyValueObservationCMn` | 6 | alt-tab, obsidian, stats, utm |
| libswiftFoundation.dylib | `_$s10Foundation3URLV016standardizedFileB0ACvg` | 6 | claude, iina, linearmouse, utm |
| libswiftFoundation.dylib | `_$s10Foundation3URLV23resolvingSymlinksInPathACyF` | 6 | claude, iina, linearmouse, obsidian |
| libswiftFoundation.dylib | `_$s10Foundation3URLV4hostSSSgvg` | 6 | 1password, alt-tab, claude, iina |
| libswiftFoundation.dylib | `_$s10Foundation3URLVSHAAMc` | 6 | iina, localsend, stats, utm |
| libswiftFoundation.dylib | `_$s10Foundation4DataV13_copyContents12initializingAC8IteratorV_SitSrys5UInt8VG_tF` | 6 | 1password, claude, linearmouse, localsend |
| libswiftFoundation.dylib | `_$s10Foundation4DataVAA15ContiguousBytesAAWP` | 6 | claude, github, utm, visual-studio-code |
| libswiftFoundation.dylib | `_$s10Foundation4DateV36_unconditionallyBridgeFromObjectiveCyACSo6NSDateCSgFZ` | 6 | claude, iina, linearmouse, utm |
| libswiftFoundation.dylib | `_$s10Foundation4UUIDV2eeoiySbAC_ACtFZ` | 6 | 1password, linearmouse, obsidian, stats |
| libswiftFoundation.dylib | `_$s10Foundation4UUIDVSHAAMc` | 6 | 1password, linearmouse, stats, utm |
| libswiftFoundation.dylib | `_$s10Foundation6LocaleV10identifierACSS_tcfC` | 6 | 1password, iina, localsend, stats |
| libswiftFoundation.dylib | `_$s10Foundation6LocaleV19_bridgeToObjectiveCSo8NSLocaleCyF` | 6 | 1password, iina, localsend, stats |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV7integerACSi_tcfC` | 6 | alt-tab, iina, linearmouse, utm |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetVMn` | 6 | iina, linearmouse, localsend, utm |
| libswiftFoundation.dylib | `_$sSS10FoundationE5bytes8encodingSSSgxh_SSAAE8EncodingVtcSTRzs5UInt8V7ElementRtzlufC` | 6 | 1password, iina, linearmouse, stats |
| libswiftFoundation.dylib | `_$sSo12NSDictionaryC10FoundationE17dictionaryLiteralAByp_yptd_tcfC` | 6 | 1password, alt-tab, claude, localsend |
| libswiftFoundation.dylib | `_$sSo8NSNumberC10FoundationE14integerLiteralABSi_tcfC` | 6 | alt-tab, iina, linearmouse, utm |
| libswiftFoundation.dylib | `_$sSy10FoundationE10components11separatedBySaySSGAA12CharacterSetV_tF` | 6 | alt-tab, iina, stats, utm |
| CoreFoundation.framework | `_CFArrayExchangeValuesAtIndices` | 5 | krita, love, openrct2, shotcut |
| CoreFoundation.framework | `_CFArrayReplaceValues` | 5 | hex-fiend, iterm2, pencil, sequel-ace |
| CoreFoundation.framework | `_CFStringCreateMutableWithExternalCharactersNoCopy` | 5 | audacity, iterm2, lmms, sequel-ace |
| CoreFoundation.framework | `_NSLocaleQuotationBeginDelimiterKey` | 5 | bitwarden, hammerspoon, iterm2, monitorcontrol |
| CoreFoundation.framework | `_NSLocaleQuotationEndDelimiterKey` | 5 | bitwarden, hammerspoon, iterm2, monitorcontrol |
| CoreFoundation.framework | `_NSLocaleUsesMetricSystem` | 5 | bitwarden, hammerspoon, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLIsPackageKey` | 5 | bitwarden, cmux, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLIsSystemImmutableKey` | 5 | bitwarden, godot, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLLocalizedTypeDescriptionKey` | 5 | bitwarden, monitorcontrol, mos, raycast |
| Foundation.framework | `_$s10Foundation10CocoaErrorV4CodeVAA01_cD8ProtocolAAMc` | 5 | chatgpt, clipy, cmux, iterm2 |
| Foundation.framework | `_$s10Foundation10URLRequestV3urlAA3URLVSgvg` | 5 | chatgpt, cmux, iterm2, raycast |
| Foundation.framework | `_$s10Foundation10URLRequestVSQAAMc` | 5 | cmux, orbstack, raycast, sequel-ace |
| Foundation.framework | `_$s10Foundation11JSONDecoderC20DateDecodingStrategyO21millisecondsSince1970yA2EmFWC` | 5 | chatgpt, clipy, cmux, iterm2 |
| Foundation.framework | `_$s10Foundation11JSONDecoderC20DateDecodingStrategyO7iso8601yA2EmFWC` | 5 | chatgpt, cmux, orbstack, raycast |
| Foundation.framework | `_$s10Foundation11JSONEncoderC19KeyEncodingStrategyO18convertToSnakeCaseyA2EmFWC` | 5 | chatgpt, clipy, cmux, orbstack |
| Foundation.framework | `_$s10Foundation11JSONEncoderC19KeyEncodingStrategyOMa` | 5 | chatgpt, clipy, cmux, orbstack |
| Foundation.framework | `_$s10Foundation11JSONEncoderC19keyEncodingStrategyAC03KeydE0OvsTj` | 5 | chatgpt, clipy, cmux, orbstack |
| Foundation.framework | `_$s10Foundation12CharacterSetV15urlQueryAllowedACvgZ` | 5 | betterdisplay, cmux, iterm2, raycast |
| Foundation.framework | `_$s10Foundation12CharacterSetV16uppercaseLettersACvgZ` | 5 | clipy, coteditor, iterm2, raycast |
| Foundation.framework | `_$s10Foundation14DateComponentsV3daySiSgvg` | 5 | chatgpt, clipy, cmux, iterm2 |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV13isRegularFileSbSgvg` | 5 | chatgpt, cmux, ghostty, iterm2 |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV8fileSizeSiSgvg` | 5 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation17URLResourceValuesVMn` | 5 | chatgpt, cmux, iterm2, raycast |
| Foundation.framework | `_$s10Foundation19AttributedSubstringVy5ValueQzSgxmcAA0B9StringKeyRzluis` | 5 | cmux, coteditor, ghostty, maccy |
| Foundation.framework | `_$s10Foundation19PropertyListDecoderC6decode_4fromxxm_AA4DataVtKSeRzlFTj` | 5 | chatgpt, coteditor, iterm2, orbstack |
| Foundation.framework | `_$s10Foundation21NSKeyValueObservationCMa` | 5 | cmux, iterm2, maccy, orbstack |
| Foundation.framework | `_$s10Foundation23LocalizedStringResourceV13stringLiteralACSS_tcfC` | 5 | betterdisplay, ghostty, maccy, raycast |
| Foundation.framework | `_$s10Foundation23LocalizedStringResourceV19stringInterpolationACSSAAE17LocalizationValueV0cF0V_tcfC` | 5 | betterdisplay, coteditor, ghostty, raycast |
| Foundation.framework | `_$s10Foundation3URLV08absoluteB0ACvg` | 5 | cmux, ghostty, iterm2, orbstack |
| Foundation.framework | `_$s10Foundation3URLV12standardizedACvg` | 5 | cmux, iterm2, orbstack, sequel-ace |
| Foundation.framework | `_$s10Foundation3URLV13DirectoryHintO02isC0yA2EmFWC` | 5 | chatgpt, clipy, coteditor, ghostty |
| Foundation.framework | `_$s10Foundation3URLV21resolvingBookmarkData7options10relativeTo08bookmarkE7IsStaleAcA0E0Vh_So30NSURLBookmarkResolutionOptionsVACSghSbztKcfC` | 5 | coteditor, iterm2, orbstack, sequel-ace |
| Foundation.framework | `_$s10Foundation3URLV8passwordSSSgvg` | 5 | betterdisplay, cmux, hammerspoon, orbstack |
| Foundation.framework | `_$s10Foundation40CustomLocalizedStringResourceConvertibleMp` | 5 | betterdisplay, ghostty, maccy, raycast |
| Foundation.framework | `_$s10Foundation40CustomLocalizedStringResourceConvertibleP09localizeddE0AA0cdE0VvgTq` | 5 | betterdisplay, ghostty, maccy, raycast |
| Foundation.framework | `_$s10Foundation4DataV11descriptionSSvg` | 5 | clipy, iterm2, orbstack, raycast |
| Foundation.framework | `_$s10Foundation4DataV15_RepresentationO15reserveCapacityyySiF` | 5 | betterdisplay, chatgpt, cmux, iterm2 |
| Foundation.framework | `_$s10Foundation4DataVSHAAMc` | 5 | betterdisplay, chatgpt, cmux, orbstack |
| Foundation.framework | `_$s10Foundation4DataVSTAAMc` | 5 | chatgpt, cmux, iterm2, raycast |
| Foundation.framework | `_$s10Foundation4DateV18ISO8601FormatStyleV0B13TimeSeparatorOMa` | 5 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation4DateV18ISO8601FormatStyleVAA0dE0AAMc` | 5 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation4DateV18ISO8601FormatStyleVMa` | 5 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation4DateV9formattedy12FormatOutputQzxAA0D5StyleRzAC0D5InputRtzlF` | 5 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation6LocaleV8LanguageVMa` | 5 | cmux, coteditor, raycast, rectangle |
| Foundation.framework | `_$s10Foundation6LocaleV8languageAC8LanguageVvg` | 5 | cmux, coteditor, raycast, rectangle |
| Foundation.framework | `_$s10Foundation8CalendarV14dateComponents_4fromAA04DateD0VShyAC9ComponentOG_AA0F0VtF` | 5 | chatgpt, cmux, iterm2, raycast |
| Foundation.framework | `_$s10Foundation8CalendarV4date4fromAA4DateVSgAA0E10ComponentsV_tF` | 5 | chatgpt, clipy, cmux, iterm2 |
| Foundation.framework | `_$s10Foundation8CalendarV8timeZoneAA04TimeD0Vvs` | 5 | chatgpt, clipy, cmux, raycast |
| Foundation.framework | `_$s10Foundation8CalendarV9ComponentO3dayyA2EmFWC` | 5 | chatgpt, cmux, iterm2, raycast |
| Foundation.framework | `_$s10Foundation8CalendarV9ComponentO5monthyA2EmFWC` | 5 | chatgpt, cmux, iterm2, raycast |
| Foundation.framework | `_$s10Foundation8CalendarV9ComponentO6secondyA2EmFWC` | 5 | chatgpt, cmux, raycast, stats |
| Foundation.framework | `_$s10Foundation8CalendarV9ComponentOMn` | 5 | chatgpt, cmux, iterm2, raycast |
| Foundation.framework | `_$s10Foundation8CalendarV9ComponentOSHAAMc` | 5 | chatgpt, cmux, iterm2, raycast |
| Foundation.framework | `_$s10Foundation8CalendarV9ComponentOSQAAMc` | 5 | chatgpt, cmux, iterm2, raycast |
| Foundation.framework | `_$s10Foundation8IndexSetV10integersInACSnySiG_tcfC` | 5 | cmux, coteditor, iterm2, orbstack |
| Foundation.framework | `_$s10Foundation8IndexSetV12makeIterators08IndexingE0VyACGyF` | 5 | clipy, cmux, iterm2, orbstack |
| Foundation.framework | `_$s10Foundation8IndexSetV5firstSiSgvg` | 5 | cmux, coteditor, iterm2, orbstack |
| Foundation.framework | `_$s10Foundation8TimeZoneV10identifierACSgSSh_tcfC` | 5 | chatgpt, cmux, iterm2, raycast |
| Foundation.framework | `_$s10Foundation8TimeZoneV10identifierSSvg` | 5 | chatgpt, clipy, cmux, raycast |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV9cancelledAEvgZ` | 5 | betterdisplay, cmux, iterm2, raycast |
| Foundation.framework | `_$s10Foundation8URLErrorV8_nsErrorSo7NSErrorCvg` | 5 | betterdisplay, chatgpt, cmux, iterm2 |
| Foundation.framework | `_$s10Foundation8URLErrorVMn` | 5 | chatgpt, cmux, iterm2, raycast |
| Foundation.framework | `_$s10Foundation8URLErrorVs5ErrorAAMc` | 5 | chatgpt, cmux, iterm2, raycast |
| Foundation.framework | `_$sSS10FoundationE10contentsOfSSAA3URLVh_tKcfC` | 5 | chatgpt, iterm2, orbstack, raycast |
| Foundation.framework | `_$sSS10FoundationE14contentsOfFile8encodingS2Sh_SSAAE8EncodingVtKcfC` | 5 | claude, cmux, iterm2, raycast |
| Foundation.framework | `_$sSS10FoundationE7cString8encodingSSSgSPys4Int8VG_SSAAE8EncodingVtcfC` | 5 | betterdisplay, cmux, ghostty, hammerspoon |
| Foundation.framework | `_$sSS10FoundationE8EncodingV14utf16BigEndianACvgZ` | 5 | betterdisplay, cmux, coteditor, raycast |
| Foundation.framework | `_$sSS10FoundationE8EncodingV5utf16ACvgZ` | 5 | cmux, coteditor, iterm2, raycast |
| Foundation.framework | `_$sSS10FoundationE8EncodingV8rawValueACSu_tcfC` | 5 | cmux, coteditor, iterm2, raycast |
| Foundation.framework | `_$sSS10FoundationE8EncodingVMn` | 5 | cmux, coteditor, iterm2, raycast |
| Foundation.framework | `_$sSW10Foundation15ContiguousBytesAAWP` | 5 | betterdisplay, chatgpt, cmux, raycast |
| Foundation.framework | `_$sSo10CFErrorRefas5Error10FoundationMc` | 5 | betterdisplay, cmux, orbstack, raycast |
| Foundation.framework | `_$sSo7NSCoderC10FoundationE12decodeObject2of6forKeyxSgxm_SStSo8NSObjectCRbzSo8NSCodingRzlF` | 5 | clipy, cmux, coteditor, ghostty |
| Foundation.framework | `_$sSo8NSNumberC10FoundationE12floatLiteralABSd_tcfC` | 5 | chatgpt, cmux, iterm2, orbstack |
| Foundation.framework | `_$sSo9NSDecimala10FoundationE11descriptionSSvg` | 5 | clipy, coteditor, iterm2, raycast |
| Foundation.framework | `_$sSy10FoundationE19enumerateSubstrings2in7options_yqd___So26NSStringEnumerationOptionsVySSSg_SnySS5IndexVGAJSbztctSXRd__AI5BoundRtd__lF` | 5 | claude, coteditor, iterm2, raycast |
| Foundation.framework | `_$sSy10FoundationE5write6toFile10atomically8encodingyqd___SbSSAAE8EncodingVtKSyRd__lF` | 5 | cmux, iterm2, orbstack, raycast |
| Foundation.framework | `_$sSy10FoundationE7folding7options6localeSSSo22NSStringCompareOptionsV_AA6LocaleVSgtF` | 5 | cmux, coteditor, iterm2, raycast |
| Foundation.framework | `_$ss10ArraySliceVyxG10Foundation15ContiguousBytesADs5UInt8VRszlMc` | 5 | betterdisplay, chatgpt, cmux, orbstack |
| Foundation.framework | `_$ss15CollectionOfOneVyxG10Foundation15ContiguousBytesADs5UInt8VRszlMc` | 5 | betterdisplay, chatgpt, claude, cmux |
| Foundation.framework | `_NSDecimalCompare` | 5 | bitwarden, iterm2, monitorcontrol, mos |
| Foundation.framework | `_NSGlobalDomain` | 5 | cmux, jordanbaird-ice, raycast, vlc |
| Foundation.framework | `_NSStringEncodingErrorKey` | 5 | bitwarden, coteditor, monitorcontrol, mos |
| Foundation.framework | `_NSURLFileScheme` | 5 | betterdisplay, cmux, hammerspoon, orbstack |
| Foundation.framework | `_NSUndefinedKeyException` | 5 | clipy, iina, krita, obs |
| Foundation.framework | `_OBJC_CLASS_$_NSURLDownload` | 5 | iterm2, macdown, openscad, vlc |
| libswiftFoundation.dylib | `_$s10Foundation10CocoaErrorV03_nsC0So7NSErrorCvg` | 5 | alt-tab, drawio, obsidian, utm |
| libswiftFoundation.dylib | `_$s10Foundation10URLRequestV10httpMethodSSSgvs` | 5 | alt-tab, iina, stats, utm |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC16OutputFormattingV13prettyPrintedAEvgZ` | 5 | 1password, hex-fiend, linearmouse, utm |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC16OutputFormattingVMn` | 5 | 1password, claude, hex-fiend, linearmouse |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV17controlCharactersACvgZ` | 5 | 1password, alt-tab, claude, stats |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV5unionyA2CF` | 5 | 1password, claude, iina, utm |
| libswiftFoundation.dylib | `_$s10Foundation12URLQueryItemV4name5valueACSSh_SSSghtcfC` | 5 | alt-tab, claude, linearmouse, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12URLQueryItemV4nameSSvg` | 5 | alt-tab, iina, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12URLQueryItemV5valueSSSgvg` | 5 | alt-tab, iina, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12URLQueryItemVMn` | 5 | alt-tab, claude, linearmouse, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV10queryItemsSayAA12URLQueryItemVGSgvs` | 5 | alt-tab, claude, linearmouse, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV3urlAA3URLVSgvg` | 5 | alt-tab, claude, iina, linearmouse |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV4hostSSSgvg` | 5 | claude, iina, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV6stringACSgSSh_tcfC` | 5 | alt-tab, claude, iina, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation25NSFastEnumerationIteratorVMa` | 5 | iina, stats, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation26_ObjectiveCBridgeableErrorMp` | 5 | 1password, claude, drawio, utm |
| libswiftFoundation.dylib | `_$s10Foundation26_ObjectiveCBridgeableErrorP15_bridgedNSErrorxSgSo0F0Ch_tcfCTq` | 5 | 1password, claude, drawio, utm |
| libswiftFoundation.dylib | `_$s10Foundation26_ObjectiveCBridgeableErrorPs0D0Tb` | 5 | 1password, claude, drawio, utm |
| libswiftFoundation.dylib | `_$s10Foundation3URLV2eeoiySbAC_ACtFZ` | 5 | claude, iina, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation3URLVSEAAMc` | 5 | claude, iina, linearmouse, stats |
| libswiftFoundation.dylib | `_$s10Foundation3URLVSeAAMc` | 5 | claude, iina, linearmouse, stats |
| libswiftFoundation.dylib | `_$s10Foundation4DataVSEAAMc` | 5 | claude, linearmouse, stats, utm |
| libswiftFoundation.dylib | `_$s10Foundation4DataVSTAAMc` | 5 | github, iina, utm, visual-studio-code |
| libswiftFoundation.dylib | `_$s10Foundation4DataVSeAAMc` | 5 | claude, linearmouse, stats, utm |
| libswiftFoundation.dylib | `_$s10Foundation4DateV11distantPastACvgZ` | 5 | claude, iina, stats, utm |
| libswiftFoundation.dylib | `_$s10Foundation4DateV2eeoiySbAC_ACtFZ` | 5 | linearmouse, stats, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateVSEAAMc` | 5 | iina, linearmouse, stats, utm |
| libswiftFoundation.dylib | `_$s10Foundation4DateVSeAAMc` | 5 | iina, linearmouse, stats, utm |
| libswiftFoundation.dylib | `_$s10Foundation4UUIDV10uuidStringACSgSSh_tcfC` | 5 | linearmouse, localsend, stats, utm |
| libswiftFoundation.dylib | `_$s10Foundation4UUIDV36_unconditionallyBridgeFromObjectiveCyACSo6NSUUIDCSgFZ` | 5 | linearmouse, stats, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation6LocaleV7currentACvgZ` | 5 | alt-tab, iina, stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV7currentACvgZ` | 5 | alt-tab, iina, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarVMa` | 5 | alt-tab, iina, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV12makeIterators08IndexingE0VyACGyF` | 5 | iina, raycast, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV19_bridgeToObjectiveCSo07NSIndexC0CyF` | 5 | alt-tab, iina, linearmouse, whatsapp |
| libswiftFoundation.dylib | `_$s12CoreGraphics7CGFloatV10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF` | 5 | alt-tab, stats, utm, whatsapp |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV5asciiACvgZ` | 5 | iina, stats, utm, whatsapp |
| libswiftFoundation.dylib | `_$sSo8NSNumberC10FoundationE12floatLiteralABSd_tcfC` | 5 | alt-tab, iina, utm, whatsapp |
| libswiftFoundation.dylib | `_$sSo8NSStringC10FoundationE13stringLiteralABs12StaticStringV_tcfC` | 5 | 1password, claude, iina, whatsapp |
| libswiftFoundation.dylib | `_$sSy10FoundationE22caseInsensitiveCompareySo18NSComparisonResultVqd__SyRd__lF` | 5 | cursor, linearmouse, utm, whatsapp |
| CoreData.framework | `_OBJC_CLASS_$_NSManagedObjectID` | 4 | bitwarden, monitorcontrol, mos, whatsapp |
| CoreData.framework | `_OBJC_CLASS_$_NSMergeConflict` | 4 | bitwarden, monitorcontrol, mos, whatsapp |
| CoreFoundation.framework | `_CFPlugInAddInstanceForFactory` | 4 | cyberduck, krita, libreoffice, transmission |
| CoreFoundation.framework | `_CFPlugInRemoveInstanceForFactory` | 4 | cyberduck, krita, libreoffice, transmission |
| CoreFoundation.framework | `_CFStringCompareWithOptionsAndLocale` | 4 | bitwarden, monitorcontrol, mos, openttd |
| CoreFoundation.framework | `_CFStringGetNameOfEncoding` | 4 | audacity, hex-fiend, kicad, prusaslicer |
| CoreFoundation.framework | `_CFURLCreateStringByAddingPercentEscapes` | 4 | hammerspoon, lmms, openscad, zoom |
| CoreFoundation.framework | `_NSLocaleAlternateQuotationBeginDelimiterKey` | 4 | bitwarden, hammerspoon, monitorcontrol, mos |
| CoreFoundation.framework | `_NSLocaleAlternateQuotationEndDelimiterKey` | 4 | bitwarden, hammerspoon, monitorcontrol, mos |
| CoreFoundation.framework | `_NSLocaleCollationIdentifier` | 4 | bitwarden, hammerspoon, monitorcontrol, mos |
| CoreFoundation.framework | `_NSLocaleCollatorIdentifier` | 4 | bitwarden, hammerspoon, monitorcontrol, mos |
| CoreFoundation.framework | `_NSLocaleExemplarCharacterSet` | 4 | bitwarden, hammerspoon, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLAttributeModificationDateKey` | 4 | bitwarden, monitorcontrol, mos, raycast |
| CoreFoundation.framework | `_NSURLFileResourceTypeKey` | 4 | bitwarden, iterm2, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLParentDirectoryURLKey` | 4 | appcleaner, bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeIsAutomountedKey` | 4 | bitwarden, hammerspoon, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeIsBrowsableKey` | 4 | bitwarden, hammerspoon, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeIsEjectableKey` | 4 | bitwarden, hammerspoon, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeLocalizedFormatDescriptionKey` | 4 | bitwarden, hammerspoon, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeMaximumFileSizeKey` | 4 | bitwarden, hammerspoon, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeURLForRemountingKey` | 4 | bitwarden, hammerspoon, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeUUIDStringKey` | 4 | bitwarden, hammerspoon, monitorcontrol, mos |
| Foundation.framework | `_$s10Foundation10CocoaErrorV19fileWriteFileExistsAC4CodeVvgZ` | 4 | clipy, cmux, coteditor, sequel-ace |
| Foundation.framework | `_$s10Foundation10CocoaErrorV4CodeV16fileWriteUnknownAEvgZ` | 4 | claude, cmux, coteditor, orbstack |
| Foundation.framework | `_$s10Foundation10URLRequestV10httpMethodSSSgvg` | 4 | cmux, iterm2, raycast, sequel-ace |
| Foundation.framework | `_$s10Foundation10URLRequestV3urlAA3URLVSgvs` | 4 | chatgpt, cmux, raycast, sequel-ace |
| Foundation.framework | `_$s10Foundation11JSONEncoderC20DateEncodingStrategyO21millisecondsSince1970yA2EmFWC` | 4 | clipy, cmux, iterm2, raycast |
| Foundation.framework | `_$s10Foundation11MeasurementV36_unconditionallyBridgeFromObjectiveCyACyxGSo13NSMeasurementCSgFZ` | 4 | betterdisplay, clipy, cmux, tailscale-app |
| Foundation.framework | `_$s10Foundation12CharacterSetV17controlCharactersACvgZ` | 4 | chatgpt, cmux, iterm2, tailscale-app |
| Foundation.framework | `_$s10Foundation13URLComponentsV18percentEncodedPathSSvg` | 4 | cmux, iterm2, orbstack, raycast |
| Foundation.framework | `_$s10Foundation13URLComponentsV5querySSSgvs` | 4 | chatgpt, cmux, iterm2, raycast |
| Foundation.framework | `_$s10Foundation13URLComponentsV6schemeSSSgvg` | 4 | chatgpt, cmux, iterm2, raycast |
| Foundation.framework | `_$s10Foundation13URLComponentsV8fragmentSSSgvs` | 4 | chatgpt, cmux, iterm2, tailscale-app |
| Foundation.framework | `_$s10Foundation14DateComponentsV10nanosecondSiSgvs` | 4 | chatgpt, clipy, cmux, raycast |
| Foundation.framework | `_$s10Foundation14DateComponentsV4hourSiSgvg` | 4 | chatgpt, clipy, cmux, raycast |
| Foundation.framework | `_$s10Foundation14DateComponentsV4yearSiSgvg` | 4 | chatgpt, clipy, cmux, raycast |
| Foundation.framework | `_$s10Foundation14DateComponentsV5monthSiSgvg` | 4 | chatgpt, clipy, cmux, raycast |
| Foundation.framework | `_$s10Foundation14DateComponentsV6minuteSiSgvg` | 4 | chatgpt, clipy, cmux, raycast |
| Foundation.framework | `_$s10Foundation14DateComponentsV6secondSiSgvg` | 4 | chatgpt, clipy, cmux, raycast |
| Foundation.framework | `_$s10Foundation14DateComponentsV8calendarAA8CalendarVSgvs` | 4 | cmux, coteditor, iterm2, raycast |
| Foundation.framework | `_$s10Foundation14LocalizedErrorP13failureReasonSSSgvgTj` | 4 | chatgpt, clipy, jordanbaird-ice, raycast |
| Foundation.framework | `_$s10Foundation14LocalizedErrorP18recoverySuggestionSSSgvgTj` | 4 | clipy, coteditor, jordanbaird-ice, tailscale-app |
| Foundation.framework | `_$s10Foundation16AttributedStringV13stringLiteralACSS_tcfC` | 4 | coteditor, raycast, sequel-ace, tailscale-app |
| Foundation.framework | `_$s10Foundation16AttributedStringV5IndexVSQAAMc` | 4 | coteditor, ghostty, orbstack, tailscale-app |
| Foundation.framework | `_$s10Foundation16AttributedStringV5IndexV_6withinAESgSSADV_xtcAA0bC8ProtocolRzlufC` | 4 | cmux, maccy, orbstack, raycast |
| Foundation.framework | `_$s10Foundation16AttributedStringV6appendyyxAA0bC8ProtocolRzlF` | 4 | chatgpt, coteditor, raycast, tailscale-app |
| Foundation.framework | `_$s10Foundation16AttributedStringVACycfC` | 4 | cmux, coteditor, raycast, tailscale-app |
| Foundation.framework | `_$s10Foundation16AttributedStringVy5ValueQzSgxmcAA0bC3KeyRzluis` | 4 | chatgpt, coteditor, raycast, tailscale-app |
| Foundation.framework | `_$s10Foundation18IntegerFormatStyleV6localeACyxGAA6LocaleV_tcfC` | 4 | chatgpt, cmux, coteditor, orbstack |
| Foundation.framework | `_$s10Foundation19PropertyListDecoderCACycfc` | 4 | chatgpt, coteditor, iterm2, orbstack |
| Foundation.framework | `_$s10Foundation19PropertyListDecoderCMa` | 4 | chatgpt, coteditor, iterm2, orbstack |
| Foundation.framework | `_$s10Foundation23LocalizedStringResourceV17BundleDescriptionOMa` | 4 | 1password, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation24NSKeyValueObservedChangeV03oldC0xSgvg` | 4 | betterdisplay, maccy, raycast, rectangle |
| Foundation.framework | `_$s10Foundation3URLV12bookmarkData7options30includingResourceValuesForKeys10relativeToAA0D0VSo28NSURLBookmarkCreationOptionsV_ShySo16NSURLResourceKeyaGSgACSgtKF` | 4 | coteditor, orbstack, sequel-ace, tailscale-app |
| Foundation.framework | `_$s10Foundation3URLV13DirectoryHintO03notC0yA2EmFWC` | 4 | chatgpt, coteditor, ghostty, raycast |
| Foundation.framework | `_$s10Foundation3URLV18dataRepresentation10relativeTo10isAbsoluteACSgAA4DataVh_AGhSbtcfC` | 4 | clipy, maccy, raycast, tailscale-app |
| Foundation.framework | `_$s10Foundation3URLV34withUnsafeFileSystemRepresentationyxxSPys4Int8VGSgKXEKlF` | 4 | cmux, coteditor, orbstack, raycast |
| Foundation.framework | `_$s10Foundation3URLV5querySSSgvg` | 4 | cmux, ghostty, orbstack, raycast |
| Foundation.framework | `_$s10Foundation3URLV6string10relativeToACSgSSh_AFhtcfC` | 4 | cmux, iterm2, orbstack, raycast |
| Foundation.framework | `_$s10Foundation3URLVs21_ObjectiveCBridgeableAAMc` | 4 | clipy, ghostty, iterm2, raycast |
| Foundation.framework | `_$s10Foundation4DataV15_RepresentationO5countSivs` | 4 | chatgpt, cmux, iterm2, raycast |
| Foundation.framework | `_$s10Foundation4DataV5range2of7options2inSnySiGSgAC_So19NSDataSearchOptionsVAItF` | 4 | chatgpt, claude, cmux, sequel-ace |
| Foundation.framework | `_$s10Foundation4DataVSQAAMc` | 4 | clipy, cmux, coteditor, raycast |
| Foundation.framework | `_$s10Foundation4DateV11FormatStyleV04TimeD0V8standardAGvgZ` | 4 | cmux, jordanbaird-ice, orbstack, raycast |
| Foundation.framework | `_$s10Foundation4DateV18ISO8601FormatStyleV0B13TimeSeparatorO8standardyA2GmFWC` | 4 | chatgpt, cmux, coteditor, raycast |
| Foundation.framework | `_$s10Foundation4DateV18ISO8601FormatStyleV0B9SeparatorO4dashyA2GmFWC` | 4 | chatgpt, cmux, coteditor, raycast |
| Foundation.framework | `_$s10Foundation4DateV18ISO8601FormatStyleV0B9SeparatorOMa` | 4 | chatgpt, cmux, coteditor, raycast |
| Foundation.framework | `_$s10Foundation4DateV18ISO8601FormatStyleV13TimeSeparatorO5colonyA2GmFWC` | 4 | chatgpt, cmux, coteditor, raycast |
| Foundation.framework | `_$s10Foundation4DateV18ISO8601FormatStyleV13TimeSeparatorOMa` | 4 | chatgpt, cmux, coteditor, raycast |
| Foundation.framework | `_$s10Foundation4DateV18ISO8601FormatStyleV13dateSeparator0f4TimeG004timeG00i4ZoneG026includingFractionalSeconds0iJ0A2E0bG0O_AE0bhG0OAE0hG0OAE0hjG0OSbAA0hJ0VtcfC` | 4 | chatgpt, cmux, coteditor, raycast |
| Foundation.framework | `_$s10Foundation4DateV18ISO8601FormatStyleV17TimeZoneSeparatorO7omittedyA2GmFWC` | 4 | chatgpt, cmux, coteditor, raycast |
| Foundation.framework | `_$s10Foundation4DateV18ISO8601FormatStyleV17TimeZoneSeparatorOMa` | 4 | chatgpt, cmux, coteditor, raycast |
| Foundation.framework | `_$s10Foundation4DateV18ISO8601FormatStyleV3dayAEyF` | 4 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation4DateV18ISO8601FormatStyleV4yearAEyF` | 4 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation4DateV18ISO8601FormatStyleV5monthAEyF` | 4 | chatgpt, clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation4DateV_8strategyAC10ParseInputQz_xtKcAA0D8StrategyRzAC0D6OutputRtzlufC` | 4 | chatgpt, clipy, cmux, raycast |
| Foundation.framework | `_$s10Foundation6LocaleV12LanguageCodeVMa` | 4 | cmux, coteditor, raycast, tailscale-app |
| Foundation.framework | `_$s10Foundation6LocaleV12LanguageCodeVMn` | 4 | cmux, coteditor, raycast, tailscale-app |
| Foundation.framework | `_$s10Foundation6LocaleV8LanguageV12languageCodeAC0cE0VSgvg` | 4 | cmux, coteditor, raycast, tailscale-app |
| Foundation.framework | `_$s10Foundation8CalendarV10IdentifierO7iso8601yA2EmFWC` | 4 | clipy, cmux, raycast, sequel-ace |
| Foundation.framework | `_$s10Foundation8CalendarV4date8byAdding5value2to18wrappingComponentsAA4DateVSgAC9ComponentO_SiAJSbtF` | 4 | chatgpt, cmux, raycast, stats |
| Foundation.framework | `_$s10Foundation8CalendarV6localeAA6LocaleVSgvs` | 4 | clipy, cmux, raycast, sequel-ace |
| Foundation.framework | `_$s10Foundation8CalendarV9ComponentO4houryA2EmFWC` | 4 | chatgpt, cmux, raycast, tailscale-app |
| Foundation.framework | `_$s10Foundation8CalendarV9ComponentO6minuteyA2EmFWC` | 4 | chatgpt, cmux, raycast, tailscale-app |
| Foundation.framework | `_$s10Foundation8IndexSetVSQAAMc` | 4 | iterm2, maccy, orbstack, sequel-ace |
| Foundation.framework | `_$s10Foundation8TimeZoneV12abbreviationACSgSSh_tcfC` | 4 | 1password, betterdisplay, cmux, raycast |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV17badServerResponseAEvgZ` | 4 | chatgpt, cmux, raycast, sequel-ace |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV6badURLAEvgZ` | 4 | chatgpt, cmux, iterm2, raycast |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeVSQAAMc` | 4 | cmux, iterm2, raycast, sequel-ace |
| Foundation.framework | `_$s10Foundation9IndexPathV36_unconditionallyBridgeFromObjectiveCyACSo07NSIndexC0CSgFZ` | 4 | clipy, cmux, iterm2, raycast |
| Foundation.framework | `_$s10Foundation9IndexPathVMn` | 4 | chatgpt, cmux, iterm2, raycast |
| Foundation.framework | `_$sSS10FoundationE14contentsOfFileS2Sh_tKcfC` | 4 | cmux, iterm2, orbstack, raycast |
| Foundation.framework | `_$sSS10FoundationE25localizedStringWithFormatyS2S_s7CVarArg_pdtFZ` | 4 | cmux, maccy, obs, sequel-ace |
| Foundation.framework | `_$sSS10FoundationE6format9argumentsS2Sh_Says7CVarArg_pGhtcfC` | 4 | clipy, iterm2, sequel-ace, tailscale-app |
| Foundation.framework | `_$sSS10FoundationE8EncodingV14utf32BigEndianACvgZ` | 4 | cmux, coteditor, raycast, sequel-ace |
| Foundation.framework | `_$sSS10FoundationE8EncodingV17utf32LittleEndianACvgZ` | 4 | cmux, coteditor, raycast, sequel-ace |
| Foundation.framework | `_$sSS10FoundationE8EncodingVs23CustomStringConvertibleAAMc` | 4 | coteditor, iterm2, raycast, sequel-ace |
| Foundation.framework | `_$sSS10FoundationE9localizedSSAA23LocalizedStringResourceV_tcfC` | 4 | 1password, clipy, cmux, coteditor |
| Foundation.framework | `_$sSa10FoundationE26_forceBridgeFromObjectiveC_6resultySo7NSArrayC_SayxGSgztFZ` | 4 | chatgpt, iterm2, orbstack, raycast |
| Foundation.framework | `_$sSo12NSFileHandleC10FoundationE9readToEndAC4DataVSgyKF` | 4 | chatgpt, cmux, raycast, tailscale-app |
| Foundation.framework | `_$sSo12NSFileHandleC10FoundationE9seekToEnds6UInt64VyKF` | 4 | chatgpt, cmux, iterm2, tailscale-app |
| Foundation.framework | `_$sSo13NSUndoManagerC10FoundationE12registerUndo10withTarget7handleryx_yxctRlzClF` | 4 | coteditor, ghostty, iterm2, raycast |
| Foundation.framework | `_$sSo8NSNumberC10FoundationE14booleanLiteralABSb_tcfC` | 4 | betterdisplay, cmux, coteditor, iterm2 |
| Foundation.framework | `_$sSy10FoundationE18localizedLowercaseSSvg` | 4 | clipy, coteditor, iterm2, raycast |
| Foundation.framework | `_$sSy10FoundationE18localizedUppercaseSSvg` | 4 | 1password, clipy, coteditor, raycast |
| Foundation.framework | `_$sSy10FoundationE7cString5usingSays4Int8VGSgSSAAE8EncodingV_tF` | 4 | cmux, iterm2, obs, raycast |
| Foundation.framework | `_$sSy10FoundationE7compare_7options5range6localeSo18NSComparisonResultVqd___So22NSStringCompareOptionsVSnySS5IndexVGSgAA6LocaleVSgtSyRd__lF` | 4 | claude, cmux, coteditor, sequel-ace |
| Foundation.framework | `_NSDecimalDivide` | 4 | bitwarden, monitorcontrol, mos, raycast |
| Foundation.framework | `_NSFileHandleDataAvailableNotification` | 4 | raycast, sequel-ace, sublime-text, zoom |
| Foundation.framework | `_NSHFSTypeCodeFromFileType` | 4 | iterm2, pencil, sequel-ace, webtorrent |
| Foundation.framework | `_NSHelpAnchorErrorKey` | 4 | bitwarden, monitorcontrol, mos, raycast |
| Foundation.framework | `_NSIsNilTransformerName` | 4 | appcleaner, coteditor, maccy, rectangle |
| Foundation.framework | `_NSURLErrorFailingURLPeerTrustErrorKey` | 4 | bitwarden, iterm2, monitorcontrol, mos |
| Foundation.framework | `_OBJC_CLASS_$_NSPort` | 4 | android-studio, cyberduck, dbeaver-community, linearmouse |
| Foundation.framework | `_OBJC_CLASS_$_NSUnarchiver` | 4 | hex-fiend, iina, sequel-ace, zoom |
| Foundation.framework | `_OBJC_CLASS_$_NSUnit` | 4 | alt-tab, betterdisplay, clipy, iterm2 |
| libswiftFoundation.dylib | `_$s10Foundation10CocoaErrorV4CodeV15fileReadUnknownAEvgZ` | 4 | drawio, obsidian, stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10POSIXErrorVAA21_BridgedStoredNSErrorAAMc` | 4 | claude, stats, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10POSIXErrorVMa` | 4 | claude, stats, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10URLRequestV36_unconditionallyBridgeFromObjectiveCyACSo12NSURLRequestCSgFZ` | 4 | iina, obsidian, stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10URLRequestV8httpBodyAA4DataVSgvs` | 4 | alt-tab, iina, stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10URLRequestV8setValue_18forHTTPHeaderFieldySSSg_SStF` | 4 | claude, iina, stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONDecoderCMn` | 4 | claude, iina, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderCMn` | 4 | 1password, stats, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV13alphanumericsACvgZ` | 4 | alt-tab, iina, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV8invertedACvg` | 4 | stats, utm, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation12NotificationV19_bridgeToObjectiveCSo14NSNotificationCyF` | 4 | iina, utm, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation12NotificationVMn` | 4 | alt-tab, iina, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation13CustomNSErrorMp` | 4 | 1password, drawio, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13CustomNSErrorP11errorDomainSSvgZTq` | 4 | 1password, drawio, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13CustomNSErrorP13errorUserInfoSDySSypGvgTq` | 4 | 1password, drawio, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13CustomNSErrorP9errorCodeSivgTq` | 4 | 1password, drawio, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13CustomNSErrorPs5ErrorTb` | 4 | 1password, drawio, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV6schemeSSSgvg` | 4 | iina, utm, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsVMa` | 4 | iina, stats, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation14LocalizedErrorP16errorDescriptionSSSgvgTj` | 4 | claude, hex-fiend, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation15AffineTransformV19_bridgeToObjectiveCSo08NSAffineC0CyF` | 4 | alt-tab, iina, localsend, zoom |
| libswiftFoundation.dylib | `_$s10Foundation15AffineTransformVMa` | 4 | alt-tab, iina, localsend, zoom |
| libswiftFoundation.dylib | `_$s10Foundation17URLResourceValuesV8fileSizeSiSgvg` | 4 | iina, utm, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation18_ErrorCodeProtocolMp` | 4 | 1password, drawio, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation18_ErrorCodeProtocolP01_B4TypeAC_AA21_BridgedStoredNSErrorTn` | 4 | 1password, drawio, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation18_ErrorCodeProtocolPAAE2teoiySbx_s0B0_ptFZ` | 4 | 1password, iina, linearmouse, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation18_ErrorCodeProtocolPSQTb` | 4 | 1password, drawio, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation21NSKeyValueObservationC10invalidateyyFTj` | 4 | alt-tab, stats, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation21_BridgedStoredNSErrorMp` | 4 | 1password, drawio, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation21_BridgedStoredNSErrorP4CodeAC_8RawValueSYs17FixedWidthIntegerTn` | 4 | 1password, drawio, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation21_BridgedStoredNSErrorP4CodeAC_AA06_ErrorE8ProtocolTn` | 4 | 1password, drawio, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation21_BridgedStoredNSErrorP4CodeAC_SYTn` | 4 | 1password, drawio, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation21_BridgedStoredNSErrorP8_nsErrorSo0D0CvgTq` | 4 | 1password, drawio, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation21_BridgedStoredNSErrorP8_nsErrorxSo0D0C_tcfCTq` | 4 | 1password, drawio, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation21_BridgedStoredNSErrorPAA06CustomD0Tb` | 4 | 1password, drawio, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation21_BridgedStoredNSErrorPAA26_ObjectiveCBridgeableErrorTb` | 4 | 1password, drawio, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation21_BridgedStoredNSErrorPAAE012_getEmbeddedD0yXlSgyF` | 4 | 1password, drawio, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation21_BridgedStoredNSErrorPAAE08_bridgedD0xSgSo0D0C_tcfC` | 4 | 1password, drawio, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation21_BridgedStoredNSErrorPAAE13errorUserInfoSDySSypGvg` | 4 | 1password, drawio, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation21_BridgedStoredNSErrorPAAE2eeoiySbx_xtFZ` | 4 | 1password, drawio, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation21_BridgedStoredNSErrorPAAE4code4CodeQzvg` | 4 | 1password, drawio, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation21_BridgedStoredNSErrorPAAE4hash4intoys6HasherVz_tF` | 4 | 1password, drawio, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation21_BridgedStoredNSErrorPAAE9errorCodeSivg` | 4 | 1password, drawio, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation21_BridgedStoredNSErrorPSHTb` | 4 | 1password, drawio, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation25NSFastEnumerationIteratorV4nextypSgyF` | 4 | stats, utm, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation3URLV06isFileB0Sbvg` | 4 | iina, localsend, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation3URLV12standardizedACvg` | 4 | iina, linearmouse, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation3URLV14pathComponentsSaySSGvg` | 4 | 1password, claude, stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation3URLV22appendingPathExtensionyACSSF` | 4 | 1password, claude, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation3URLV34withUnsafeFileSystemRepresentationyxxSPys4Int8VGSgKXEKlF` | 4 | 1password, iina, linearmouse, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation3URLV6string10relativeToACSgSSh_AFhtcfC` | 4 | drawio, iina, localsend, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataV15_RepresentationOys5UInt8VSicig` | 4 | linearmouse, stats, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataV5countSivg` | 4 | alt-tab, claude, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataV7subdata2inACSnySiG_tF` | 4 | iina, stats, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateV026timeIntervalSinceReferenceB0SdvgZ` | 4 | 1password, iina, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation4DateV20timeIntervalSinceNowSdvg` | 4 | linearmouse, stats, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateVSHAAMc` | 4 | claude, linearmouse, stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation6LocaleV36_unconditionallyBridgeFromObjectiveCyACSo8NSLocaleCSgFZ` | 4 | 1password, raycast, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV9ComponentO3dayyA2EmFWC` | 4 | iina, stats, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV9ComponentO4yearyA2EmFWC` | 4 | stats, utm, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV9ComponentOMa` | 4 | iina, utm, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV9ComponentOMn` | 4 | iina, stats, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV9ComponentOSHAAMc` | 4 | iina, stats, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV0B0VMa` | 4 | iina, utm, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV0B0VSQAAMc` | 4 | iina, utm, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV10integersInACSnySiG_tcfC` | 4 | iina, utm, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV6insertySb8inserted_Si17memberAfterInserttSiF` | 4 | iina, utm, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetVACycfC` | 4 | iina, utm, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetVSQAAMc` | 4 | linearmouse, localsend, raycast, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetVSlAAMc` | 4 | iina, utm, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8URLErrorV4CodeVMa` | 4 | iina, obsidian, stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8URLErrorV8_nsErrorSo7NSErrorCvg` | 4 | iina, obsidian, stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8URLErrorVAA21_BridgedStoredNSErrorAAMc` | 4 | iina, obsidian, stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8URLErrorVMa` | 4 | iina, obsidian, stats, whatsapp |
| libswiftFoundation.dylib | `_$s10_ErrorType10Foundation01_A12CodeProtocolPTl` | 4 | 1password, drawio, utm, whatsapp |
| libswiftFoundation.dylib | `_$s4Code10Foundation21_BridgedStoredNSErrorPTl` | 4 | 1password, drawio, utm, whatsapp |
| libswiftFoundation.dylib | `_$sSS10FoundationE14utf16CodeUnits5countSSSPys6UInt16VG_SitcfC` | 4 | iina, linearmouse, stats, whatsapp |
| libswiftFoundation.dylib | `_$sSS10FoundationE6format9argumentsS2Sh_Says7CVarArg_pGhtcfC` | 4 | 1password, iina, whatsapp, zoom |
| libswiftFoundation.dylib | `_$sSS10FoundationE7cString8encodingSSSgSPys4Int8VG_SSAAE8EncodingVtcfC` | 4 | alt-tab, iina, stats, whatsapp |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingVMn` | 4 | iina, stats, utm, whatsapp |
| libswiftFoundation.dylib | `_$sSS10FoundationEySSSo8NSStringCcfC` | 4 | iina, linearmouse, whatsapp, zoom |
| libswiftFoundation.dylib | `_$sSayxG10Foundation15ContiguousBytesABs5UInt8VRszlMc` | 4 | cursor, iina, linearmouse, whatsapp |
| libswiftFoundation.dylib | `_$sSd10FoundationE10truncatingSdSo8NSNumberCh_tcfC` | 4 | 1password, iina, linearmouse, whatsapp |
| libswiftFoundation.dylib | `_$sSd10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF` | 4 | claude, iina, whatsapp, zoom |
| libswiftFoundation.dylib | `_$sSo12NSFileHandleC10FoundationE4read9upToCountAC4DataVSgSi_tKF` | 4 | alt-tab, obsidian, utm, whatsapp |
| libswiftFoundation.dylib | `_$sSo12NSFileHandleC10FoundationE5write10contentsOfyx_tKAC12DataProtocolRzlF` | 4 | claude, cursor, utm, whatsapp |
| libswiftFoundation.dylib | `_$sSo12NSFileHandleC10FoundationE9readToEndAC4DataVSgyKF` | 4 | cursor, obsidian, utm, whatsapp |
| libswiftFoundation.dylib | `_$sSo7NSArrayC10FoundationE12arrayLiteralABypd_tcfC` | 4 | claude, localsend, whatsapp, zoom |
| libswiftFoundation.dylib | `_$sSo7NSArrayC10FoundationE12makeIteratorAC017NSFastEnumerationD0VyF` | 4 | iina, stats, whatsapp, zoom |
| libswiftFoundation.dylib | `_$sSy10FoundationE16rangeOfCharacter4from7options0B0SnySS5IndexVGSgAA0D3SetV_So22NSStringCompareOptionsVAItF` | 4 | 1password, iina, whatsapp, zoom |
| libswiftFoundation.dylib | `_$sSy10FoundationE31localizedCaseInsensitiveCompareySo18NSComparisonResultVqd__SyRd__lF` | 4 | iina, linearmouse, stats, whatsapp |
| libswiftFoundation.dylib | `_$sSy10FoundationE5write2to10atomically8encodingyAA3URLV_SbSSAAE8EncodingVtKF` | 4 | claude, iina, linearmouse, whatsapp |
| libswiftFoundation.dylib | `_$ss5ErrorP10FoundationAC13CustomNSErrorRzrlE5_codeSivg` | 4 | 1password, drawio, utm, whatsapp |
| libswiftFoundation.dylib | `_$ss5ErrorP10FoundationAC13CustomNSErrorRzrlE7_domainSSvg` | 4 | 1password, drawio, utm, whatsapp |
| CoreData.framework | `_NSAffectedObjectsErrorKey` | 3 | bitwarden, monitorcontrol, mos |
| CoreData.framework | `_NSAffectedStoresErrorKey` | 3 | bitwarden, monitorcontrol, mos |
| CoreData.framework | `_NSPersistentStoreSaveConflictsErrorKey` | 3 | bitwarden, monitorcontrol, mos |
| CoreData.framework | `_NSValidationPredicateErrorKey` | 3 | bitwarden, monitorcontrol, mos |
| CoreData.framework | `_NSValidationValueErrorKey` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_CFBundleLoadExecutableAndReturnError` | 3 | android-studio, audacity, musescore |
| CoreFoundation.framework | `_CFCharacterSetCreateMutableCopy` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_CFCharacterSetCreateWithBitmapRepresentation` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_CFCharacterSetHasMemberInPlane` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_CFCharacterSetIntersect` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_CFCharacterSetInvert` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_CFCharacterSetIsSupersetOfSet` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_CFCharacterSetRemoveCharactersInRange` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_CFCharacterSetRemoveCharactersInString` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_CFFileDescriptorDisableCallBacks` | 3 | bitwarden, localsend, zoom |
| CoreFoundation.framework | `_CFHashBytes` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_CFMakeCollectable` | 3 | cyberduck, flycut, openrct2 |
| CoreFoundation.framework | `_CFStringDelete` | 3 | android-studio, cyberduck, dbeaver-community |
| CoreFoundation.framework | `_CFStringHashCString` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_CFStringHashNSString` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_CFStringIsEncodingAvailable` | 3 | audacity, kicad, prusaslicer |
| CoreFoundation.framework | `_NSCalendarIdentifierBuddhist` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSCalendarIdentifierChinese` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSCalendarIdentifierCoptic` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSCalendarIdentifierEthiopicAmeteAlem` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSCalendarIdentifierEthiopicAmeteMihret` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSCalendarIdentifierHebrew` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSCalendarIdentifierISO8601` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSCalendarIdentifierIndian` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSCalendarIdentifierIslamic` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSCalendarIdentifierIslamicCivil` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSCalendarIdentifierJapanese` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSCalendarIdentifierPersian` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSCalendarIdentifierRepublicOfChina` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLCustomIconKey` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLIsMountTriggerKey` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLIsUserImmutableKey` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLLabelColorKey` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLLabelNumberKey` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLLocalizedLabelKey` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLPreferredIOBlockSizeKey` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLUbiquitousItemDownloadingErrorKey` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLUbiquitousItemHasUnresolvedConflictsKey` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLUbiquitousItemIsUploadedKey` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLUbiquitousItemIsUploadingKey` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLUbiquitousItemUploadingErrorKey` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeCreationDateKey` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeIsJournalingKey` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeResourceCountKey` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeSupportsAdvisoryFileLockingKey` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeSupportsCasePreservedNamesKey` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeSupportsExtendedSecurityKey` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeSupportsHardLinksKey` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeSupportsJournalingKey` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeSupportsPersistentIDsKey` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeSupportsRenamingKey` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeSupportsRootDirectoryDatesKey` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeSupportsSparseFilesKey` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeSupportsSymbolicLinksKey` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeSupportsVolumeSizesKey` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeSupportsZeroRunsKey` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_OBJC_CLASS_$_NSFileSecurity` | 3 | bitwarden, monitorcontrol, mos |
| CoreFoundation.framework | `_kCFJapaneseCalendar` | 3 | android-studio, cyberduck, dbeaver-community |
| CoreFoundation.framework | `_kCFLocaleVariantCode` | 3 | android-studio, cyberduck, dbeaver-community |
| CoreFoundation.framework | `_kCFStreamSocketSecurityLevelNegotiatedSSL` | 3 | hammerspoon, raycast, vlc |
| Foundation.framework | `_$s10Foundation10CocoaErrorV4CodeV19fileReadCorruptFileAEvgZ` | 3 | betterdisplay, cmux, coteditor |
| Foundation.framework | `_$s10Foundation10CocoaErrorV4CodeV8rawValueSivg` | 3 | coteditor, iterm2, raycast |
| Foundation.framework | `_$s10Foundation10URLRequestV19allHTTPHeaderFieldsSDyS2SGSgvg` | 3 | cmux, raycast, sequel-ace |
| Foundation.framework | `_$s10Foundation10URLRequestV8addValue_18forHTTPHeaderFieldySS_SStF` | 3 | chatgpt, cmux, raycast |
| Foundation.framework | `_$s10Foundation11FormatStylePA2A4DateV07ISO8601bC0VRszrlE7iso8601AGvgZ` | 3 | chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation11FormatStylePAAs8DurationVAAE05UnitsbC0VRszrlE5units7allowed5width16maximumUnitCount09zeroValueE011valueLength14fractionalPartAGShyAG0J0VG_AG0J5WidthVSiSgAG04ZeromE15DisplayStrategyVAtG010FractionalqtU0VtFZ` | 3 | cmux, coteditor, ghostty |
| Foundation.framework | `_$s10Foundation11JSONDecoderC19KeyDecodingStrategyO20convertFromSnakeCaseyA2EmFWC` | 3 | chatgpt, orbstack, raycast |
| Foundation.framework | `_$s10Foundation11JSONDecoderC19KeyDecodingStrategyOMa` | 3 | chatgpt, orbstack, raycast |
| Foundation.framework | `_$s10Foundation11JSONDecoderC20DateDecodingStrategyO16secondsSince1970yA2EmFWC` | 3 | chatgpt, iterm2, raycast |
| Foundation.framework | `_$s10Foundation11JSONEncoderC16outputFormattingAC06OutputD0VvMTj` | 3 | chatgpt, iterm2, raycast |
| Foundation.framework | `_$s10Foundation11JSONEncoderC20DateEncodingStrategyO6customyAEyAA0C0V_s7Encoder_ptKccAEmFWC` | 3 | chatgpt, clipy, raycast |
| Foundation.framework | `_$s10Foundation11JSONEncoderC20DateEncodingStrategyO7iso8601yA2EmFWC` | 3 | chatgpt, cmux, raycast |
| Foundation.framework | `_$s10Foundation11JSONEncoderC7Combine15TopLevelEncoderAAMc` | 3 | cmux, coteditor, jordanbaird-ice |
| Foundation.framework | `_$s10Foundation12CharacterSetV10isSuperset2ofSbAC_tF` | 3 | coteditor, iterm2, raycast |
| Foundation.framework | `_$s10Foundation12CharacterSetV14urlHostAllowedACvgZ` | 3 | cmux, orbstack, tailscale-app |
| Foundation.framework | `_$s10Foundation12CharacterSetV19_bridgeToObjectiveCSo011NSCharacterC0CyF` | 3 | coteditor, iterm2, sequel-ace |
| Foundation.framework | `_$s10Foundation12CharacterSetVs0C7AlgebraAAMc` | 3 | cmux, coteditor, raycast |
| Foundation.framework | `_$s10Foundation12DataProtocolPA2A15ContiguousBytesRzrlE04copyE02to4fromySryqd__G_qd_0_tSXRd_0_5BoundQyd_0_5IndexSlRtzr0_lF` | 3 | betterdisplay, cmux, iterm2 |
| Foundation.framework | `_$s10Foundation12DataProtocolPAAE9copyBytes2to4fromSiSw_qd__tSXRd__5BoundQyd__5IndexRtzlF` | 3 | claude, cmux, orbstack |
| Foundation.framework | `_$s10Foundation12DateIntervalVMa` | 3 | chatgpt, raycast, sequel-ace |
| Foundation.framework | `_$s10Foundation13CustomNSErrorPAAE13errorUserInfoSDySSypGvg` | 3 | chatgpt, cmux, iterm2 |
| Foundation.framework | `_$s10Foundation13ParseStrategyPA2A4DateV18ISO8601FormatStyleVRszrlE7iso8601AGvgZ` | 3 | chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation13URLComponentsV10queryItemsSayAA12URLQueryItemVGSgvM` | 3 | cmux, iterm2, raycast |
| Foundation.framework | `_$s10Foundation13URLComponentsV19percentEncodedQuerySSSgvg` | 3 | cmux, raycast, sequel-ace |
| Foundation.framework | `_$s10Foundation13URLComponentsV19percentEncodedQuerySSSgvs` | 3 | cmux, iterm2, sequel-ace |
| Foundation.framework | `_$s10Foundation13URLComponentsV4portSiSgvg` | 3 | chatgpt, cmux, iterm2 |
| Foundation.framework | `_$s10Foundation13__DataStorageC12_deallocatorySv_SitcSgvg` | 3 | iterm2, orbstack, sequel-ace |
| Foundation.framework | `_$s10Foundation14DateComponentsV10nanosecondSiSgvg` | 3 | chatgpt, clipy, raycast |
| Foundation.framework | `_$s10Foundation14DateComponentsV4dateAA0B0VSgvg` | 3 | coteditor, iterm2, raycast |
| Foundation.framework | `_$s10Foundation14DateComponentsVMn` | 3 | clipy, iterm2, raycast |
| Foundation.framework | `_$s10Foundation15AttributeScopesO0A10AttributesV04LinkB0OAA19AttributedStringKeyAAMc` | 3 | cmux, coteditor, raycast |
| Foundation.framework | `_$s10Foundation15AttributeScopesO0A10AttributesV04LinkB0ON` | 3 | cmux, coteditor, raycast |
| Foundation.framework | `_$s10Foundation15_BridgedNSErrorMp` | 3 | jordanbaird-ice, raycast, sequel-ace |
| Foundation.framework | `_$s10Foundation15_BridgedNSErrorP14_nsErrorDomainSSvgZTq` | 3 | jordanbaird-ice, raycast, sequel-ace |
| Foundation.framework | `_$s10Foundation15_BridgedNSErrorP8RawValueSY_s17FixedWidthIntegerTn` | 3 | jordanbaird-ice, raycast, sequel-ace |
| Foundation.framework | `_$s10Foundation15_BridgedNSErrorPAA26_ObjectiveCBridgeableErrorTb` | 3 | jordanbaird-ice, raycast, sequel-ace |
| Foundation.framework | `_$s10Foundation15_BridgedNSErrorPAAE08_bridgedC0xSgSo0C0Ch_tcfC` | 3 | jordanbaird-ice, raycast, sequel-ace |
| Foundation.framework | `_$s10Foundation15_BridgedNSErrorPAAE4hash4intoys6HasherVz_tF` | 3 | jordanbaird-ice, raycast, sequel-ace |
| Foundation.framework | `_$s10Foundation15_BridgedNSErrorPAAE5_codeSivg` | 3 | jordanbaird-ice, raycast, sequel-ace |
| Foundation.framework | `_$s10Foundation15_BridgedNSErrorPAAE7_domainSSvg` | 3 | jordanbaird-ice, raycast, sequel-ace |
| Foundation.framework | `_$s10Foundation15_BridgedNSErrorPSHTb` | 3 | jordanbaird-ice, raycast, sequel-ace |
| Foundation.framework | `_$s10Foundation15_BridgedNSErrorPSYTb` | 3 | jordanbaird-ice, raycast, sequel-ace |
| Foundation.framework | `_$s10Foundation16AttributedStringV13CharacterViewVMn` | 3 | coteditor, ghostty, tailscale-app |
| Foundation.framework | `_$s10Foundation16AttributedStringV13CharacterViewVSTAAMc` | 3 | chatgpt, cmux, raycast |
| Foundation.framework | `_$s10Foundation16AttributedStringV13CharacterViewVSlAAMc` | 3 | coteditor, ghostty, tailscale-app |
| Foundation.framework | `_$s10Foundation16AttributedStringV4RunsV3RunVMa` | 3 | cmux, coteditor, raycast |
| Foundation.framework | `_$s10Foundation16AttributedStringV4RunsV5IndexVMa` | 3 | cmux, coteditor, raycast |
| Foundation.framework | `_$s10Foundation16AttributedStringV4RunsV5IndexVSQAAMc` | 3 | cmux, coteditor, raycast |
| Foundation.framework | `_$s10Foundation16AttributedStringV4RunsVMa` | 3 | cmux, coteditor, raycast |
| Foundation.framework | `_$s10Foundation16AttributedStringV4RunsVSlAAMc` | 3 | cmux, coteditor, raycast |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV12isExecutableSbSgvg` | 3 | cmux, coteditor, ghostty |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV14isSymbolicLinkSbSgvg` | 3 | cmux, iterm2, raycast |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV20isExcludedFromBackupSbSgvs` | 3 | cmux, iterm2, raycast |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV23contentModificationDateAA0F0VSgvg` | 3 | cmux, coteditor, raycast |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV8isHiddenSbSgvg` | 3 | chatgpt, cmux, coteditor |
| Foundation.framework | `_$s10Foundation17URLResourceValuesVACycfC` | 3 | cmux, iterm2, raycast |
| Foundation.framework | `_$s10Foundation18AttributeContainerVy5ValueQzSgxmcAA19AttributedStringKeyRzluis` | 3 | cmux, raycast, tailscale-app |
| Foundation.framework | `_$s10Foundation18IntegerFormatStyleVMn` | 3 | cmux, coteditor, orbstack |
| Foundation.framework | `_$s10Foundation18_ErrorCodeProtocolMp` | 3 | clipy, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation18_ErrorCodeProtocolP01_B4TypeAC_AA21_BridgedStoredNSErrorTn` | 3 | clipy, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation18_ErrorCodeProtocolPSQTb` | 3 | clipy, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation19PropertyListEncoderC6encodeyAA4DataVxKSERzlFTj` | 3 | chatgpt, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation19PropertyListEncoderCACycfc` | 3 | chatgpt, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation19PropertyListEncoderCMa` | 3 | chatgpt, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation21_BridgedStoredNSErrorMp` | 3 | clipy, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation21_BridgedStoredNSErrorP4CodeAC_8RawValueSYs17FixedWidthIntegerTn` | 3 | clipy, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation21_BridgedStoredNSErrorP4CodeAC_AA06_ErrorE8ProtocolTn` | 3 | clipy, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation21_BridgedStoredNSErrorP4CodeAC_SYTn` | 3 | clipy, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation21_BridgedStoredNSErrorP8_nsErrorSo0D0CvgTq` | 3 | clipy, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation21_BridgedStoredNSErrorP8_nsErrorxSo0D0C_tcfCTq` | 3 | clipy, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation21_BridgedStoredNSErrorPAA06CustomD0Tb` | 3 | clipy, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation21_BridgedStoredNSErrorPAA26_ObjectiveCBridgeableErrorTb` | 3 | clipy, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation21_BridgedStoredNSErrorPAAE012_getEmbeddedD0yXlSgyF` | 3 | clipy, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation21_BridgedStoredNSErrorPAAE08_bridgedD0xSgSo0D0C_tcfC` | 3 | clipy, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation21_BridgedStoredNSErrorPAAE13errorUserInfoSDySSypGvg` | 3 | clipy, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation21_BridgedStoredNSErrorPAAE2eeoiySbx_xtFZ` | 3 | clipy, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation21_BridgedStoredNSErrorPAAE4hash4intoys6HasherVz_tF` | 3 | clipy, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation21_BridgedStoredNSErrorPAAE9errorCodeSivg` | 3 | clipy, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation21_BridgedStoredNSErrorPSHTb` | 3 | clipy, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation22AttributeDynamicLookupOMn` | 3 | cmux, coteditor, raycast |
| Foundation.framework | `_$s10Foundation23LocalizedStringResourceV_12defaultValue5table6locale6bundle7commentACs06StaticC0V_SSAAE012LocalizationF0VSSSgAA6LocaleVAC17BundleDescriptionOAJSgtcfC` | 3 | 1password, cmux, coteditor |
| Foundation.framework | `_$s10Foundation24FloatingPointFormatStyleVyxGAA09ParseabledE0AAMc` | 3 | cmux, coteditor, rectangle |
| Foundation.framework | `_$s10Foundation30NumberFormatStyleConfigurationO9PrecisionV14fractionLengthyAExSXRzSi5BoundRtzlFZ` | 3 | betterdisplay, cmux, coteditor |
| Foundation.framework | `_$s10Foundation3URLV15fileURLWithPath10relativeToACSSh_ACSghtcfC` | 3 | cmux, iterm2, tailscale-app |
| Foundation.framework | `_$s10Foundation3URLV17setResourceValuesyyAA011URLResourceE0VKF` | 3 | cmux, iterm2, raycast |
| Foundation.framework | `_$s10Foundation3URLV18dataRepresentationAA4DataVvg` | 3 | chatgpt, clipy, iterm2 |
| Foundation.framework | `_$s10Foundation3URLV19appendPathComponentyySSF` | 3 | iterm2, orbstack, raycast |
| Foundation.framework | `_$s10Foundation3URLV23deleteLastPathComponentyyF` | 3 | cmux, coteditor, orbstack |
| Foundation.framework | `_$s10Foundation3URLV6append10queryItemsySayAA12URLQueryItemVG_tF` | 3 | betterdisplay, orbstack, raycast |
| Foundation.framework | `_$s10Foundation3URLV6append4path13directoryHintyx_AC09DirectoryF0OtSyRzlF` | 3 | clipy, orbstack, raycast |
| Foundation.framework | `_$s10Foundation3URLV8fragmentSSSgvg` | 3 | cmux, ghostty, iterm2 |
| Foundation.framework | `_$s10Foundation3URLV9appending9component13directoryHintACx_AC09DirectoryF0OtSyRzlF` | 3 | chatgpt, clipy, coteditor |
| Foundation.framework | `_$s10Foundation4DataV11DeallocatorO6customyAEySv_SitccAEmFWC` | 3 | chatgpt, cmux, orbstack |
| Foundation.framework | `_$s10Foundation4DataV11InlineSliceV21ensureUniqueReferenceyyF` | 3 | chatgpt, cmux, raycast |
| Foundation.framework | `_$s10Foundation4DataV15_RepresentationOyAESWcfC` | 3 | betterdisplay, cmux, orbstack |
| Foundation.framework | `_$s10Foundation4DataV5countSivg` | 3 | betterdisplay, cmux, orbstack |
| Foundation.framework | `_$s10Foundation4DataV9copyBytes2to5countySpys5UInt8VG_SitF` | 3 | raycast, sequel-ace, visual-studio-code |
| Foundation.framework | `_$s10Foundation4DateV026timeIntervalSinceReferenceB0SdvgZ` | 3 | betterdisplay, iterm2, raycast |
| Foundation.framework | `_$s10Foundation4DateV035timeIntervalBetween1970AndReferenceB0SdvgZ` | 3 | chatgpt, orbstack, raycast |
| Foundation.framework | `_$s10Foundation4DateV11FormatStyleV6SymbolV6SecondVMa` | 3 | clipy, cmux, raycast |
| Foundation.framework | `_$s10Foundation4DateV13ISO8601FormatySSAC0cD5StyleVF` | 3 | chatgpt, cmux, raycast |
| Foundation.framework | `_$s10Foundation4DateV18ISO8601FormatStyleV17dateTimeSeparatoryA2E0bgH0OF` | 3 | clipy, cmux, coteditor |
| Foundation.framework | `_$s10Foundation4DateV18ISO8601FormatStyleV4time26includingFractionalSecondsAESb_tF` | 3 | chatgpt, clipy, coteditor |
| Foundation.framework | `_$s10Foundation4DateV18ISO8601FormatStyleVAA13ParseStrategyAAMc` | 3 | chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation4DateV7compareySo18NSComparisonResultVACF` | 3 | betterdisplay, cmux, raycast |
| Foundation.framework | `_$s10Foundation4UUIDV9hashValueSivg` | 3 | hammerspoon, orbstack, raycast |
| Foundation.framework | `_$s10Foundation6LocaleV12LanguageCodeV10identifierSSvg` | 3 | cmux, raycast, tailscale-app |
| Foundation.framework | `_$s10Foundation6LocaleV12languageCodeSSSgvg` | 3 | betterdisplay, cmux, raycast |
| Foundation.framework | `_$s10Foundation6LocaleV15localizedString15forLanguageCodeSSSgSS_tF` | 3 | coteditor, iterm2, raycast |
| Foundation.framework | `_$s10Foundation6LocaleV36_unconditionallyBridgeFromObjectiveCyACSo8NSLocaleCSgFZ` | 3 | clipy, iterm2, sequel-ace |
| Foundation.framework | `_$s10Foundation6LocaleV6RegionVMa` | 3 | coteditor, raycast, tailscale-app |
| Foundation.framework | `_$s10Foundation8CalendarV19_bridgeToObjectiveCSo10NSCalendarCyF` | 3 | chatgpt, cmux, sequel-ace |
| Foundation.framework | `_$s10Foundation8CalendarV19autoupdatingCurrentACvgZ` | 3 | chatgpt, clipy, raycast |
| Foundation.framework | `_$s10Foundation8CalendarV8timeZoneAA04TimeD0Vvg` | 3 | chatgpt, clipy, cmux |
| Foundation.framework | `_$s10Foundation8CalendarV9component_4fromSiAC9ComponentO_AA4DateVtF` | 3 | betterdisplay, iterm2, raycast |
| Foundation.framework | `_$s10Foundation8IndexSetV6removeySiSgSiF` | 3 | coteditor, iterm2, raycast |
| Foundation.framework | `_$s10Foundation8IndexSetV9RangeViewVMa` | 3 | clipy, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation8IndexSetV9RangeViewVMn` | 3 | clipy, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation8IndexSetV9RangeViewVSlAAMc` | 3 | clipy, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation8IndexSetV9formUnionyyACF` | 3 | cmux, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation8IndexSetV9rangeViewAC05RangeE0Vvg` | 3 | clipy, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation8IndexSetVSKAAMc` | 3 | clipy, coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation8TimeZoneV19autoupdatingCurrentACvgZ` | 3 | chatgpt, clipy, raycast |
| Foundation.framework | `_$s10Foundation8TimeZoneV36_unconditionallyBridgeFromObjectiveCyACSo06NSTimeC0CSgFZ` | 3 | clipy, raycast, sequel-ace |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV14cannotFindHostAEvgZ` | 3 | cmux, iterm2, sequel-ace |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV14dataNotAllowedAEvgZ` | 3 | cmux, iterm2, sequel-ace |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV15dnsLookupFailedAEvgZ` | 3 | cmux, iterm2, sequel-ace |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV19cannotConnectToHostAEvgZ` | 3 | cmux, iterm2, sequel-ace |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV21networkConnectionLostAEvgZ` | 3 | cmux, iterm2, sequel-ace |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV22notConnectedToInternetAEvgZ` | 3 | cmux, iterm2, sequel-ace |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV22secureConnectionFailedAEvgZ` | 3 | cmux, iterm2, sequel-ace |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV23internationalRoamingOffAEvgZ` | 3 | cmux, iterm2, sequel-ace |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV27serverCertificateHasBadDateAEvgZ` | 3 | cmux, iterm2, sequel-ace |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV28serverCertificateNotYetValidAEvgZ` | 3 | cmux, iterm2, sequel-ace |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV8rawValueAESi_tcfC` | 3 | cmux, iterm2, sequel-ace |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV8timedOutAEvgZ` | 3 | cmux, iterm2, sequel-ace |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeVMn` | 3 | cmux, iterm2, sequel-ace |
| Foundation.framework | `_$s10Foundation9IndexPathV19_bridgeToObjectiveCSo07NSIndexC0CyF` | 3 | cmux, iterm2, raycast |
| Foundation.framework | `_$s10Foundation9IndexPathVSHAAMc` | 3 | chatgpt, cmux, raycast |
| Foundation.framework | `_$s10Foundation9IndexPathVSQAAMc` | 3 | chatgpt, cmux, raycast |
| Foundation.framework | `_$s10Foundation9SortOrderO2eeoiySbAC_ACtFZ` | 3 | chatgpt, coteditor, raycast |
| Foundation.framework | `_$s10_ErrorType10Foundation01_A12CodeProtocolPTl` | 3 | clipy, coteditor, iterm2 |
| Foundation.framework | `_$s12CoreGraphics7CGFloatV10FoundationE10truncatingACSo8NSNumberCh_tcfC` | 3 | chatgpt, cmux, rectangle |
| Foundation.framework | `_$s12CoreGraphics7CGFloatV10FoundationE26_forceBridgeFromObjectiveC_6resultySo8NSNumberC_ACSgztFZ` | 3 | 1password, chatgpt, cmux |
| Foundation.framework | `_$s12CoreGraphics7CGFloatV10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo8NSNumberC_ACSgztFZ` | 3 | 1password, chatgpt, cmux |
| Foundation.framework | `_$s12CoreGraphics7CGFloatV10FoundationE36_unconditionallyBridgeFromObjectiveCyACSo8NSNumberCSgFZ` | 3 | 1password, chatgpt, cmux |
| Foundation.framework | `_$s4Code10Foundation21_BridgedStoredNSErrorPTl` | 3 | clipy, coteditor, iterm2 |
| Foundation.framework | `_$sSB10FoundationE9formattedSSyF` | 3 | coteditor, jordanbaird-ice, tailscale-app |
| Foundation.framework | `_$sSS10FoundationE8EncodingV13nonLossyASCIIACvgZ` | 3 | betterdisplay, cmux, hammerspoon |
| Foundation.framework | `_$sSS10FoundationE8EncodingVSQAAMc` | 3 | coteditor, raycast, sequel-ace |
| Foundation.framework | `_$sSS10FoundationE9localized12defaultValue5table6bundle6locale7commentSSs12StaticStringV_SSAAE012LocalizationD0VSSSgSo8NSBundleCSgAA6LocaleVAISgtcfC` | 3 | cmux, coteditor, iterm2 |
| Foundation.framework | `_$sSS10FoundationEySSs5SliceVyAA16AttributedStringV13CharacterViewVGcfC` | 3 | coteditor, ghostty, tailscale-app |
| Foundation.framework | `_$sSb10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo8NSNumberC_SbSgztFZ` | 3 | clipy, iterm2, sequel-ace |
| Foundation.framework | `_$sSd10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo8NSNumberC_SdSgztFZ` | 3 | clipy, iterm2, sequel-ace |
| Foundation.framework | `_$sSf10FoundationE26_forceBridgeFromObjectiveC_6resultySo8NSNumberC_SfSgztFZ` | 3 | coteditor, raycast, sequel-ace |
| Foundation.framework | `_$sSf10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo8NSNumberC_SfSgztFZ` | 3 | coteditor, raycast, sequel-ace |
| Foundation.framework | `_$sSf10FoundationE36_unconditionallyBridgeFromObjectiveCySfSo8NSNumberCSgFZ` | 3 | coteditor, raycast, sequel-ace |
| Foundation.framework | `_$sSo12NSDictionaryC10FoundationE8IteratorCMa` | 3 | coteditor, orbstack, raycast |
| Foundation.framework | `_$sSo12NSDictionaryC10FoundationE8IteratorCStACMc` | 3 | coteditor, orbstack, raycast |
| Foundation.framework | `_$sSo13NSFileManagerC10FoundationE13replaceItemAt_04witheF006backupE4Name7optionsAC3URLVSgAI_AISSSgSo0abE18ReplacementOptionsVtKF` | 3 | claude, cmux, iterm2 |
| Foundation.framework | `_$sSo8_NSRangeV10FoundationE11descriptionSSvg` | 3 | coteditor, iterm2, raycast |
| Foundation.framework | `_$sSo9NSScannerC10FoundationE10scanStringySSSgSSF` | 3 | chatgpt, claude, raycast |
| Foundation.framework | `_$sSo9NSScannerC10FoundationE14scanUpToStringySSSgSSF` | 3 | chatgpt, claude, raycast |
| Foundation.framework | `_$sSy10FoundationE12getLineStart_3end11contentsEnd3forySpySS5IndexVG_A2Hqd__tSXRd__AG5BoundRtd__lF` | 3 | coteditor, iterm2, raycast |
| Foundation.framework | `_$sSy10FoundationE16localizedCompareySo18NSComparisonResultVqd__SyRd__lF` | 3 | clipy, iterm2, raycast |
| Foundation.framework | `_$sSy10FoundationE20localizedCapitalizedSSvg` | 3 | clipy, coteditor, raycast |
| Foundation.framework | `_$sSy10FoundationE37precomposedStringWithCanonicalMappingSSvg` | 3 | cmux, coteditor, iterm2 |
| Foundation.framework | `_$sSz10FoundationE9formattedSSyF` | 3 | cmux, coteditor, raycast |
| Foundation.framework | `_$sSz10FoundationE9formattedy12FormatOutputQyd__qd__0C5InputQyd__RszAA0C5StyleRd__lF` | 3 | cmux, coteditor, orbstack |
| Foundation.framework | `_$ss8DurationV10FoundationE16UnitsFormatStyleV09ZeroValueC15DisplayStrategyV4hideAGvgZ` | 3 | cmux, coteditor, ghostty |
| Foundation.framework | `_$ss8DurationV10FoundationE16UnitsFormatStyleV09ZeroValueC15DisplayStrategyVMa` | 3 | cmux, coteditor, ghostty |
| Foundation.framework | `_$ss8DurationV10FoundationE16UnitsFormatStyleV29FractionalPartDisplayStrategyVMa` | 3 | cmux, coteditor, ghostty |
| Foundation.framework | `_$ss8DurationV10FoundationE16UnitsFormatStyleV4UnitV7secondsAGvgZ` | 3 | cmux, coteditor, ghostty |
| Foundation.framework | `_$ss8DurationV10FoundationE16UnitsFormatStyleV4UnitVMa` | 3 | cmux, coteditor, ghostty |
| Foundation.framework | `_$ss8DurationV10FoundationE16UnitsFormatStyleV4UnitVMn` | 3 | cmux, coteditor, ghostty |
| Foundation.framework | `_$ss8DurationV10FoundationE16UnitsFormatStyleV4UnitVSHACMc` | 3 | cmux, coteditor, ghostty |
| Foundation.framework | `_$ss8DurationV10FoundationE16UnitsFormatStyleV4UnitVSQACMc` | 3 | cmux, coteditor, ghostty |
| Foundation.framework | `_$ss8DurationV10FoundationE16UnitsFormatStyleV9UnitWidthVMa` | 3 | cmux, coteditor, ghostty |
| Foundation.framework | `_$ss8DurationV10FoundationE16UnitsFormatStyleVAC0dE0ACMc` | 3 | cmux, coteditor, ghostty |
| Foundation.framework | `_$ss8DurationV10FoundationE16UnitsFormatStyleVMa` | 3 | cmux, coteditor, ghostty |
| Foundation.framework | `_NSCopyMemoryPages` | 3 | bitwarden, monitorcontrol, mos |
| Foundation.framework | `_NSDataDeallocatorFree` | 3 | bitwarden, monitorcontrol, mos |
| Foundation.framework | `_NSDataDeallocatorUnmap` | 3 | bitwarden, monitorcontrol, mos |
| Foundation.framework | `_NSDataDeallocatorVM` | 3 | bitwarden, monitorcontrol, mos |
| Foundation.framework | `_NSDecimalAdd` | 3 | bitwarden, monitorcontrol, mos |
| Foundation.framework | `_NSDecimalCompact` | 3 | bitwarden, monitorcontrol, mos |
| Foundation.framework | `_NSDecimalPower` | 3 | bitwarden, monitorcontrol, mos |
| Foundation.framework | `_NSDecimalString` | 3 | bitwarden, monitorcontrol, mos |
| Foundation.framework | `_NSDecimalSubtract` | 3 | bitwarden, monitorcontrol, mos |
| Foundation.framework | `_NSFoundationVersionNumber` | 3 | bitwarden, monitorcontrol, mos |
| Foundation.framework | `_NSPageSize` | 3 | bitwarden, monitorcontrol, mos |
| Foundation.framework | `_NSRoundDownToMultipleOfPageSize` | 3 | bitwarden, monitorcontrol, mos |
| Foundation.framework | `_NSRoundUpToMultipleOfPageSize` | 3 | bitwarden, monitorcontrol, mos |
| Foundation.framework | `_NSSizeFromString` | 3 | drawio, iterm2, zoom |
| Foundation.framework | `_OBJC_CLASS_$_NSExtensionItem` | 3 | bitwarden, localsend, tailscale-app |
| Foundation.framework | `_OBJC_CLASS_$_NSFileWrapper` | 3 | betterdisplay, coteditor, iterm2 |
| Foundation.framework | `_OBJC_CLASS_$_NSUserAppleScriptTask` | 3 | coteditor, iterm2, raycast |
| libswiftFoundation.dylib | `_$s10Foundation10CocoaErrorV18fileReadNoSuchFileAC4CodeVvgZ` | 3 | krita, linearmouse, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10CocoaErrorV4CodeV16fileReadTooLargeAEvgZ` | 3 | obsidian, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10CocoaErrorV4CodeV18fileReadNoSuchFileAEvgZ` | 3 | obsidian, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10CocoaErrorV4CodeVAA01_cD8ProtocolAAMc` | 3 | iina, linearmouse, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10POSIXErrorV8_nsErrorSo7NSErrorCvg` | 3 | claude, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10URLRequestV3urlAA3URLVSgvg` | 3 | iina, obsidian, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10URLRequestV8addValue_18forHTTPHeaderFieldySS_SStF` | 3 | alt-tab, iina, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10URLRequestVMn` | 3 | iina, stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONDecoderC19KeyDecodingStrategyO20convertFromSnakeCaseyA2EmFWC` | 3 | iina, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONDecoderC19KeyDecodingStrategyOMa` | 3 | iina, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONDecoderC19keyDecodingStrategyAC03KeydE0OvsTj` | 3 | iina, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV13decimalDigitsACvgZ` | 3 | iina, stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV14urlPathAllowedACvgZ` | 3 | iina, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV15urlQueryAllowedACvgZ` | 3 | iina, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV19_bridgeToObjectiveCSo011NSCharacterC0CyF` | 3 | iina, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV9formUnionyyACF` | 3 | 1password, claude, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12NotificationV4name6object8userInfoACSo18NSNotificationNamea_ypSgSDys11AnyHashableVypGSgtcfC` | 3 | iina, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation12NotificationV4nameSo18NSNotificationNameavg` | 3 | alt-tab, iina, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV4hostSSSgvs` | 3 | claude, iina, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV6stringSSSgvg` | 3 | claude, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsV19_bridgeToObjectiveCSo06NSDateC0CyF` | 3 | raycast, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsV3daySiSgvg` | 3 | iina, stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsV3daySiSgvs` | 3 | stats, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsV4yearSiSgvs` | 3 | stats, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsV5monthSiSgvs` | 3 | stats, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsV7weekdaySiSgvg` | 3 | raycast, stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsV8calendar8timeZone3era4year5month3day4hour6minute6second10nanosecond7weekday0O7Ordinal7quarter11weekOfMonth0rS4Year0h7ForWeeksU0AcA8CalendarVSg_AA04TimeF0VSgSiSgA13ZtcfC` | 3 | stats, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsVMn` | 3 | stats, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation17URLResourceValuesV13isRegularFileSbSgvg` | 3 | claude, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation17URLResourceValuesV14isSymbolicLinkSbSgvg` | 3 | iina, obsidian, zoom |
| libswiftFoundation.dylib | `_$s10Foundation19PropertyListDecoderC6decode_4fromxxm_AA4DataVtKSeRzlFTj` | 3 | alt-tab, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation19PropertyListDecoderCACycfc` | 3 | alt-tab, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation19PropertyListDecoderCMa` | 3 | alt-tab, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation24NSKeyValueObservedChangeVMn` | 3 | alt-tab, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation3URLV12bookmarkData7options30includingResourceValuesForKeys10relativeToAA0D0VSo28NSURLBookmarkCreationOptionsV_ShySo16NSURLResourceKeyaGSgACSgtKF` | 3 | iina, localsend, utm |
| libswiftFoundation.dylib | `_$s10Foundation3URLV19appendPathComponentyySSF` | 3 | 1password, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation3URLV21resolvingBookmarkData7options10relativeTo08bookmarkE7IsStaleAcA0E0Vh_So30NSURLBookmarkResolutionOptionsVACSghSbztKcfC` | 3 | iina, localsend, utm |
| libswiftFoundation.dylib | `_$s10Foundation3URLV23deleteLastPathComponentyyF` | 3 | 1password, linearmouse, utm |
| libswiftFoundation.dylib | `_$s10Foundation3URLV24checkResourceIsReachableSbyKF` | 3 | drawio, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation3URLV35stopAccessingSecurityScopedResourceyyF` | 3 | localsend, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation3URLV36startAccessingSecurityScopedResourceSbyF` | 3 | localsend, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation3URLV4portSiSgvg` | 3 | claude, iina, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation3URLVs21_ObjectiveCBridgeableAAMc` | 3 | alt-tab, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataV15_RepresentationO15reserveCapacityyySiF` | 3 | claude, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataV15_RepresentationO5countAESi_tcfC` | 3 | alt-tab, claude, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataV15_RepresentationO5countSivs` | 3 | iina, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataV15_RepresentationOyAESWcfC` | 3 | alt-tab, claude, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataV2eeoiySbAC_ACtFZ` | 3 | alt-tab, claude, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataV4hash4intoys6HasherVz_tF` | 3 | linearmouse, stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataV5range2of7options2inSnySiGSgAC_So19NSDataSearchOptionsVAItF` | 3 | iina, stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataVyACxcSTRzs5UInt8V7ElementRtzlufC` | 3 | github, visual-studio-code, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateV026timeIntervalSinceReferenceB0ACSd_tcfC` | 3 | claude, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateV026timeIntervalSinceReferenceB0Sdvg` | 3 | stats, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateV12timeInterval5sinceACSd_ACtcfC` | 3 | raycast, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation4DateV13distantFutureACvgZ` | 3 | alt-tab, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation4DateV20timeIntervalSinceNowACSd_tcfC` | 3 | stats, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateV7compareySo18NSComparisonResultVACF` | 3 | iina, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation4UUIDV19_bridgeToObjectiveCSo6NSUUIDCyF` | 3 | iina, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4UUIDVSEAAMc` | 3 | stats, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4UUIDVSeAAMc` | 3 | stats, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4UUIDVs23CustomStringConvertibleAAMc` | 3 | linearmouse, utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation6LocaleV12currencyCodeSSSgvg` | 3 | localsend, raycast, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV10IdentifierO9gregorianyA2EmFWC` | 3 | stats, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV10IdentifierOMa` | 3 | stats, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV10identifierA2C10IdentifierOh_tcfC` | 3 | stats, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV10startOfDay3forAA4DateVAG_tF` | 3 | stats, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV14dateComponents_4fromAA04DateD0VShyAC9ComponentOG_AA0F0VtF` | 3 | stats, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV4date4fromAA4DateVSgAA0E10ComponentsV_tF` | 3 | stats, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV5range2of2in3forSnySiGSgAC9ComponentO_AkA4DateVtF` | 3 | stats, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV6isDate_11inSameDayAsSbAA0D0V_AGtF` | 3 | alt-tab, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV8timeZoneAA04TimeD0Vvs` | 3 | stats, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV9ComponentO4houryA2EmFWC` | 3 | stats, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV9ComponentO5monthyA2EmFWC` | 3 | stats, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV9ComponentO6minuteyA2EmFWC` | 3 | stats, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV9ComponentO7weekdayyA2EmFWC` | 3 | stats, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV9ComponentOSQAAMc` | 3 | iina, stats, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV9component_4fromSiAC9ComponentO_AA4DateVtF` | 3 | stats, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarVMn` | 3 | stats, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV5countSivg` | 3 | iina, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetVs0C7AlgebraAAMc` | 3 | iina, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8TimeZoneV10identifierSSvg` | 3 | stats, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8TimeZoneV19_bridgeToObjectiveCSo06NSTimeC0CyF` | 3 | stats, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8TimeZoneV7currentACvgZ` | 3 | stats, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8TimeZoneVMa` | 3 | stats, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8TimeZoneVMn` | 3 | stats, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8URLErrorV4CodeV17badServerResponseAEvgZ` | 3 | iina, stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8URLErrorV4CodeV6badURLAEvgZ` | 3 | obsidian, stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation9IndexPathV19_bridgeToObjectiveCSo07NSIndexC0CyF` | 3 | iina, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation9IndexPathV36_unconditionallyBridgeFromObjectiveCyACSo07NSIndexC0CSgFZ` | 3 | iina, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation9IndexPathVMa` | 3 | iina, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation9IndexPathVMn` | 3 | iina, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation9IndexPathVSHAAMc` | 3 | iina, whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation9IndexPathVSQAAMc` | 3 | iina, whatsapp, zoom |
| libswiftFoundation.dylib | `_$sSD10FoundationE26_forceBridgeFromObjectiveC_6resultySo12NSDictionaryC_SDyxq_GSgztFZ` | 3 | iina, localsend, utm |
| libswiftFoundation.dylib | `_$sSS10FoundationE10contentsOf8encodingSSAA3URLVh_SSAAE8EncodingVtKcfC` | 3 | alt-tab, claude, whatsapp |
| libswiftFoundation.dylib | `_$sSS10FoundationE10contentsOfSSAA3URLVh_tKcfC` | 3 | iina, utm, whatsapp |
| libswiftFoundation.dylib | `_$sSS10FoundationE14contentsOfFile8encodingS2Sh_SSAAE8EncodingVtKcfC` | 3 | iina, whatsapp, zoom |
| libswiftFoundation.dylib | `_$sSS10FoundationE25localizedStringWithFormatyS2S_s7CVarArg_pdtFZ` | 3 | raycast, utm, whatsapp |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV13nonLossyASCIIACvgZ` | 3 | iina, utm, whatsapp |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV8rawValueACSu_tcfC` | 3 | iina, utm, whatsapp |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV8rawValueSuvg` | 3 | iina, whatsapp, zoom |
| libswiftFoundation.dylib | `_$sSW10Foundation15ContiguousBytesAAWP` | 3 | claude, whatsapp, zoom |
| libswiftFoundation.dylib | `_$sSf10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF` | 3 | alt-tab, whatsapp, zoom |
| libswiftFoundation.dylib | `_$sSh10FoundationE36_unconditionallyBridgeFromObjectiveCyShyxGSo5NSSetCSgFZ` | 3 | alt-tab, whatsapp, zoom |
| libswiftFoundation.dylib | `_$sSi10FoundationE26_forceBridgeFromObjectiveC_6resultySo8NSNumberC_SiSgztFZ` | 3 | alt-tab, iina, zoom |
| libswiftFoundation.dylib | `_$sSi10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo8NSNumberC_SiSgztFZ` | 3 | alt-tab, iina, zoom |
| libswiftFoundation.dylib | `_$sSi10FoundationE36_unconditionallyBridgeFromObjectiveCySiSo8NSNumberCSgFZ` | 3 | alt-tab, iina, zoom |
| libswiftFoundation.dylib | `_$sSo10CFErrorRefas5Error10FoundationMc` | 3 | 1password, stats, whatsapp |
| libswiftFoundation.dylib | `_$sSo12NSDictionaryC10FoundationE12makeIteratorAbCE0D0CyF` | 3 | claude, whatsapp, zoom |
| libswiftFoundation.dylib | `_$sSo12NSFileHandleC10FoundationE9seekToEnds6UInt64VyKF` | 3 | claude, cursor, whatsapp |
| libswiftFoundation.dylib | `_$sSo12NSURLSessionC10FoundationE4data3for8delegateAC4DataV_So13NSURLResponseCtAC10URLRequestV_So0A12TaskDelegate_pSgtYaKF` | 3 | stats, utm, whatsapp |
| libswiftFoundation.dylib | `_$sSo12NSURLSessionC10FoundationE4data3for8delegateAC4DataV_So13NSURLResponseCtAC10URLRequestV_So0A12TaskDelegate_pSgtYaKFTu` | 3 | stats, utm, whatsapp |
| libswiftFoundation.dylib | `_$sSo13NSFileManagerC10FoundationE10enumerator2at26includingPropertiesForKeys7options12errorHandlerSo21NSDirectoryEnumeratorCSgAC3URLV_SaySo16NSURLResourceKeyaGSgSo0M18EnumerationOptionsVSbAM_s5Error_ptcSgtF` | 3 | alt-tab, utm, whatsapp |
| libswiftFoundation.dylib | `_$sSo8NSStringC10FoundationE6format_A2Bh_s7CVarArg_pdtcfC` | 3 | stats, whatsapp, zoom |
| libswiftFoundation.dylib | `_$sSu10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF` | 3 | hex-fiend, whatsapp, zoom |
| libswiftFoundation.dylib | `_$sSy10FoundationE11capitalizedSSvg` | 3 | linearmouse, stats, whatsapp |
| libswiftFoundation.dylib | `_$sSy10FoundationE14enumerateLines8invokingyySS_Sbztc_tF` | 3 | iina, stats, zoom |
| libswiftFoundation.dylib | `_$sSy10FoundationE21addingPercentEncoding21withAllowedCharactersSSSgAA12CharacterSetV_tF` | 3 | iina, stats, whatsapp |
| libswiftFoundation.dylib | `_$sSy10FoundationE23removingPercentEncodingSSSgvg` | 3 | iina, whatsapp, zoom |
| libswiftFoundation.dylib | `_$sSy10FoundationE24localizedStandardCompareySo18NSComparisonResultVqd__SyRd__lF` | 3 | alt-tab, iina, whatsapp |
| libswiftFoundation.dylib | `_$sSy10FoundationE32localizedCaseInsensitiveContainsySbqd__SyRd__lF` | 3 | hex-fiend, utm, whatsapp |
| libswiftFoundation.dylib | `_$sSy10FoundationE7cString5usingSays4Int8VGSgSSAAE8EncodingV_tF` | 3 | iina, stats, whatsapp |
| libswiftFoundation.dylib | `_$ss6UInt64V10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF` | 3 | iina, utm, whatsapp |
| CoreData.framework | `_OBJC_CLASS_$_NSAttributeDescription` | 2 | alt-tab, whatsapp |
| CoreFoundation.framework | `_CFBundleCloseBundleResourceMap` | 2 | pencil, webtorrent |
| CoreFoundation.framework | `_CFBundleCopySharedSupportURL` | 2 | bitwarden, localsend |
| CoreFoundation.framework | `_CFBundleOpenBundleResourceMap` | 2 | pencil, webtorrent |
| CoreFoundation.framework | `_CFFileDescriptorGetContext` | 2 | bitwarden, localsend |
| CoreFoundation.framework | `_CFFileDescriptorIsValid` | 2 | bitwarden, localsend |
| CoreFoundation.framework | `_CFMachPortIsValid` | 2 | linearmouse, zoom |
| CoreFoundation.framework | `_CFMessagePortGetName` | 2 | hammerspoon, zoom |
| CoreFoundation.framework | `_CFPreferencesCopyMultiple` | 2 | cmux, firefox |
| CoreFoundation.framework | `_CFPreferencesRemoveSuitePreferencesFromApp` | 2 | clipy, sequel-ace |
| CoreFoundation.framework | `_CFPropertyListCreateFromXMLData` | 2 | deluge, lmms |
| CoreFoundation.framework | `_CFReadStreamCreate` | 2 | pencil, webtorrent |
| CoreFoundation.framework | `_CFRunLoopTimerDoesRepeat` | 2 | hammerspoon, iterm2 |
| CoreFoundation.framework | `_CFStringCapitalize` | 2 | flycut, sequel-ace |
| CoreFoundation.framework | `_CFStringGetRangeOfComposedCharactersAtIndex` | 2 | iterm2, openttd |
| CoreFoundation.framework | `_CFStringSetExternalCharactersNoCopy` | 2 | iterm2, sequel-ace |
| CoreFoundation.framework | `_CFURLCreateData` | 2 | kicad, prusaslicer |
| CoreFoundation.framework | `_CFUUIDCreateWithBytes` | 2 | android-studio, zoom |
| CoreFoundation.framework | `_CFWriteStreamCopyProperty` | 2 | hammerspoon, raycast |
| CoreFoundation.framework | `_NSURLUbiquitousItemDownloadingStatusNotDownloaded` | 2 | iterm2, raycast |
| CoreFoundation.framework | `_kCFStreamPropertySocketSecurityLevel` | 2 | hammerspoon, raycast |
| Foundation.framework | `_$s10Foundation10CocoaErrorV18fileReadNoSuchFileAC4CodeVvgZ` | 2 | chatgpt, iterm2 |
| Foundation.framework | `_$s10Foundation10CocoaErrorV4CodeV13userCancelledAEvgZ` | 2 | chatgpt, coteditor |
| Foundation.framework | `_$s10Foundation10CocoaErrorV4CodeV14fileNoSuchFileAEvgZ` | 2 | cmux, coteditor |
| Foundation.framework | `_$s10Foundation10CocoaErrorV4CodeV15fileReadUnknownAEvgZ` | 2 | chatgpt, cmux |
| Foundation.framework | `_$s10Foundation10CocoaErrorV4CodeV16coderReadCorruptAEvgZ` | 2 | chatgpt, coteditor |
| Foundation.framework | `_$s10Foundation10CocoaErrorV4CodeV18fileReadNoSuchFileAEvgZ` | 2 | cmux, coteditor |
| Foundation.framework | `_$s10Foundation10CocoaErrorV4CodeVSQAAMc` | 2 | cmux, coteditor |
| Foundation.framework | `_$s10Foundation10CocoaErrorV5error_8userInfo3urls0C0_pAC4CodeV_SDys11AnyHashableVypGSgAA3URLVSgtFZ` | 2 | coteditor, sequel-ace |
| Foundation.framework | `_$s10Foundation10CocoaErrorVs0C0AAMc` | 2 | cmux, coteditor |
| Foundation.framework | `_$s10Foundation10POSIXErrorVMn` | 2 | cmux, raycast |
| Foundation.framework | `_$s10Foundation10URLRequestV11cachePolicySo017NSURLRequestCacheD0Vvg` | 2 | cmux, raycast |
| Foundation.framework | `_$s10Foundation10URLRequestV11cachePolicySo017NSURLRequestCacheD0Vvs` | 2 | cmux, raycast |
| Foundation.framework | `_$s10Foundation10URLRequestV19allHTTPHeaderFieldsSDyS2SGSgvs` | 2 | raycast, sequel-ace |
| Foundation.framework | `_$s10Foundation10URLRequestV23httpShouldHandleCookiesSbvs` | 2 | chatgpt, cmux |
| Foundation.framework | `_$s10Foundation10URLRequestVs23CustomStringConvertibleAAMc` | 2 | raycast, sequel-ace |
| Foundation.framework | `_$s10Foundation11FormatStyleP6formaty0B6OutputQz0B5InputQzFTj` | 2 | chatgpt, coteditor |
| Foundation.framework | `_$s10Foundation11FormatStylePA2A09ByteCountbC0VRszrlE04byteE05style12allowedUnits13spellsOutZero014includesActualdE0A2E0C0O_AE0I0VS2btFZ` | 2 | coteditor, orbstack |
| Foundation.framework | `_$s10Foundation11JSONDecoderC19keyDecodingStrategyAC03KeydE0OvsTj` | 2 | orbstack, raycast |
| Foundation.framework | `_$s10Foundation11JSONDecoderC20DataDecodingStrategyO6base64yA2EmFWC` | 2 | clipy, raycast |
| Foundation.framework | `_$s10Foundation11JSONDecoderC20DataDecodingStrategyOMa` | 2 | clipy, raycast |
| Foundation.framework | `_$s10Foundation11JSONDecoderC20dataDecodingStrategyAC04DatadE0OvsTj` | 2 | clipy, raycast |
| Foundation.framework | `_$s10Foundation11JSONDecoderC34NonConformingFloatDecodingStrategyO5throwyA2EmFWC` | 2 | clipy, raycast |
| Foundation.framework | `_$s10Foundation11JSONDecoderC34NonConformingFloatDecodingStrategyOMa` | 2 | clipy, raycast |
| Foundation.framework | `_$s10Foundation11JSONDecoderC34nonConformingFloatDecodingStrategyAC03NondefG0OvsTj` | 2 | clipy, raycast |
| Foundation.framework | `_$s10Foundation11JSONDecoderC7Combine15TopLevelDecoderAAMc` | 2 | cmux, coteditor |
| Foundation.framework | `_$s10Foundation11JSONDecoderC8userInfoSDys010CodingUserD3KeyVypGvMTj` | 2 | cmux, raycast |
| Foundation.framework | `_$s10Foundation11JSONDecoderC8userInfoSDys010CodingUserD3KeyVypGvsTj` | 2 | clipy, raycast |
| Foundation.framework | `_$s10Foundation11JSONEncoderC16OutputFormattingVSYAAMc` | 2 | iterm2, raycast |
| Foundation.framework | `_$s10Foundation11JSONEncoderC16OutputFormattingVs9OptionSetAAMc` | 2 | iterm2, raycast |
| Foundation.framework | `_$s10Foundation11JSONEncoderC16outputFormattingAC06OutputD0VvgTj` | 2 | clipy, raycast |
| Foundation.framework | `_$s10Foundation11JSONEncoderC20DataEncodingStrategyO6base64yA2EmFWC` | 2 | clipy, raycast |
| Foundation.framework | `_$s10Foundation11JSONEncoderC20DataEncodingStrategyOMa` | 2 | clipy, raycast |
| Foundation.framework | `_$s10Foundation11JSONEncoderC20dataEncodingStrategyAC04DatadE0OvsTj` | 2 | clipy, raycast |
| Foundation.framework | `_$s10Foundation11JSONEncoderC34NonConformingFloatEncodingStrategyO15convertToStringyAESS_S2StcAEmFWC` | 2 | clipy, raycast |
| Foundation.framework | `_$s10Foundation11JSONEncoderC34NonConformingFloatEncodingStrategyO5throwyA2EmFWC` | 2 | clipy, raycast |
| Foundation.framework | `_$s10Foundation11JSONEncoderC34NonConformingFloatEncodingStrategyOMa` | 2 | clipy, raycast |
| Foundation.framework | `_$s10Foundation11JSONEncoderC34nonConformingFloatEncodingStrategyAC03NondefG0OvsTj` | 2 | clipy, raycast |
| Foundation.framework | `_$s10Foundation11JSONEncoderC8userInfoSDys010CodingUserD3KeyVypGvsTj` | 2 | clipy, raycast |
| Foundation.framework | `_$s10Foundation12CharacterSetV11subtractingyA2CF` | 2 | iterm2, sequel-ace |
| Foundation.framework | `_$s10Foundation12CharacterSetV12charactersInACSNys7UnicodeO6ScalarVG_tcfC` | 2 | iterm2, raycast |
| Foundation.framework | `_$s10Foundation12CharacterSetV14urlPathAllowedACvgZ` | 2 | cmux, raycast |
| Foundation.framework | `_$s10Foundation12CharacterSetV6insert12charactersInySNys7UnicodeO6ScalarVG_tF` | 2 | iterm2, raycast |
| Foundation.framework | `_$s10Foundation12CharacterSetV6remove12charactersInySS_tF` | 2 | cmux, sequel-ace |
| Foundation.framework | `_$s10Foundation12CharacterSetV7lettersACvgZ` | 2 | cmux, raycast |
| Foundation.framework | `_$s10Foundation12CharacterSetVACycfC` | 2 | iterm2, sequel-ace |
| Foundation.framework | `_$s10Foundation12DataProtocolPAAE9copyBytes2toSiSw_tF` | 2 | chatgpt, iterm2 |
| Foundation.framework | `_$s10Foundation12DateIntervalV3endAA0B0Vvg` | 2 | chatgpt, raycast |
| Foundation.framework | `_$s10Foundation12DateIntervalV5start8durationAcA0B0V_SdtcfC` | 2 | chatgpt, raycast |
| Foundation.framework | `_$s10Foundation12DateIntervalV5startAA0B0Vvg` | 2 | chatgpt, raycast |
| Foundation.framework | `_$s10Foundation12DateIntervalVMn` | 2 | chatgpt, raycast |
| Foundation.framework | `_$s10Foundation12NotificationVs23CustomStringConvertibleAAMc` | 2 | betterdisplay, orbstack |
| Foundation.framework | `_$s10Foundation13CustomNSErrorPAAE9errorCodeSivg` | 2 | coteditor, orbstack |
| Foundation.framework | `_$s10Foundation13ParseStrategyMp` | 2 | chatgpt, coteditor |
| Foundation.framework | `_$s10Foundation13URLComponentsV18percentEncodedPathSSvs` | 2 | cmux, iterm2 |
| Foundation.framework | `_$s10Foundation13URLComponentsV22percentEncodedFragmentSSSgvs` | 2 | cmux, iterm2 |
| Foundation.framework | `_$s10Foundation13URLComponentsV3url10relativeToAA3URLVSgAH_tF` | 2 | chatgpt, raycast |
| Foundation.framework | `_$s10Foundation13URLComponentsV4pathSSvM` | 2 | iterm2, raycast |
| Foundation.framework | `_$s10Foundation13__DataStorageC27ensureUniqueBufferReference9growingTo5clearySi_SbtF` | 2 | iterm2, orbstack |
| Foundation.framework | `_$s10Foundation14LocalizedErrorP10helpAnchorSSSgvgTj` | 2 | clipy, jordanbaird-ice |
| Foundation.framework | `_$s10Foundation14SortComparatorMp` | 2 | coteditor, raycast |
| Foundation.framework | `_$s10Foundation14SortComparatorP5orderAA0B5OrderOvMTq` | 2 | coteditor, raycast |
| Foundation.framework | `_$s10Foundation14SortComparatorP5orderAA0B5OrderOvgTq` | 2 | coteditor, raycast |
| Foundation.framework | `_$s10Foundation14SortComparatorP5orderAA0B5OrderOvsTq` | 2 | coteditor, raycast |
| Foundation.framework | `_$s10Foundation14SortComparatorP7compareySo18NSComparisonResultV8ComparedQz_AHtFTq` | 2 | coteditor, raycast |
| Foundation.framework | `_$s10Foundation14SortComparatorPSHTb` | 2 | coteditor, raycast |
| Foundation.framework | `_$s10Foundation15AffineTransformV14translationByX3byYAC12CoreGraphics7CGFloatV_AHtcfC` | 2 | coteditor, raycast |
| Foundation.framework | `_$s10Foundation15AffineTransformV19_bridgeToObjectiveCSo08NSAffineC0CyF` | 2 | coteditor, raycast |
| Foundation.framework | `_$s10Foundation15AffineTransformVMa` | 2 | coteditor, raycast |
| Foundation.framework | `_$s10Foundation15AttributeScopesO0A10AttributesV024InlinePresentationIntentB0OAA19AttributedStringKeyAAMc` | 2 | cmux, coteditor |
| Foundation.framework | `_$s10Foundation15AttributeScopesO0A10AttributesV024InlinePresentationIntentB0ON` | 2 | cmux, coteditor |
| Foundation.framework | `_$s10Foundation15AttributeScopesO0A10AttributesV04LinkB0OMn` | 2 | cmux, coteditor |
| Foundation.framework | `_$s10Foundation15AttributeScopesO0A10AttributesV4linkAE04LinkB0Ovg` | 2 | cmux, coteditor |
| Foundation.framework | `_$s10Foundation15AttributeScopesO0A10AttributesV4linkAE04LinkB0OvpMV` | 2 | cmux, coteditor |
| Foundation.framework | `_$s10Foundation15AttributeScopesO0A10AttributesVMn` | 2 | cmux, coteditor |
| Foundation.framework | `_$s10Foundation16AttributedStringV10startIndexAC0E0Vvg` | 2 | coteditor, ghostty |
| Foundation.framework | `_$s10Foundation16AttributedStringV20AttributeMergePolicyO7keepNewyA2EmFWC` | 2 | raycast, tailscale-app |
| Foundation.framework | `_$s10Foundation16AttributedStringV20AttributeMergePolicyOMa` | 2 | raycast, tailscale-app |
| Foundation.framework | `_$s10Foundation16AttributedStringV2peoiyyACz_ACtFZ` | 2 | cmux, coteditor |
| Foundation.framework | `_$s10Foundation16AttributedStringV4RunsV3RunV10attributesAA18AttributeContainerVvg` | 2 | coteditor, raycast |
| Foundation.framework | `_$s10Foundation16AttributedStringV4RunsV3RunV5rangeSnyAC5IndexVGvg` | 2 | cmux, coteditor |
| Foundation.framework | `_$s10Foundation16AttributedStringV4RunsVMn` | 2 | cmux, coteditor |
| Foundation.framework | `_$s10Foundation16AttributedStringV4runsAC4RunsVvg` | 2 | cmux, coteditor |
| Foundation.framework | `_$s10Foundation16AttributedStringVyAA0B9SubstringVxcSXRzAC5IndexV5BoundRtzluig` | 2 | coteditor, raycast |
| Foundation.framework | `_$s10Foundation16AttributedStringVyACSo012NSAttributedC0CcfC` | 2 | coteditor, tailscale-app |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV03allC0SDySo16NSURLResourceKeyaypGvg` | 2 | maccy, raycast |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV11isAliasFileSbSgvg` | 2 | coteditor, raycast |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV12creationDateAA0E0VSgvg` | 2 | cmux, raycast |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV13localizedNameSSSgvg` | 2 | cmux, raycast |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV14typeIdentifierSSSgvg` | 2 | iterm2, raycast |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV22totalFileAllocatedSizeSiSgvg` | 2 | orbstack, raycast |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV24localizedTypeDescriptionSSSgvg` | 2 | raycast, tailscale-app |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV32volumeSupportsCaseSensitiveNamesSbSgvg` | 2 | iterm2, raycast |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV4nameSSSgvg` | 2 | cmux, raycast |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV9isPackageSbSgvg` | 2 | cmux, raycast |
| Foundation.framework | `_$s10Foundation18AttributeContainerV13dynamicMemberAC7BuilderVy_xGs7KeyPathCyAA0B13DynamicLookupOxG_tcAA016AttributedStringG0Rzluig` | 2 | coteditor, raycast |
| Foundation.framework | `_$s10Foundation18AttributeContainerV7BuilderV14callAsFunctionyAC5ValueQzF` | 2 | coteditor, raycast |
| Foundation.framework | `_$s10Foundation18AttributeContainerV7BuilderVMn` | 2 | coteditor, raycast |
| Foundation.framework | `_$s10Foundation18AttributeContainerVMn` | 2 | cmux, raycast |
| Foundation.framework | `_$s10Foundation18IntegerFormatStyleVyxGAA09ParseablecD0AAMc` | 2 | cmux, orbstack |
| Foundation.framework | `_$s10Foundation18IntegerFormatStyleVyxGAA0cD0AAMc` | 2 | cmux, coteditor |
| Foundation.framework | `_$s10Foundation19AttributedSubstringVMa` | 2 | coteditor, raycast |
| Foundation.framework | `_$s10Foundation20ByteCountFormatStyleV0E0O4fileyA2EmFWC` | 2 | coteditor, orbstack |
| Foundation.framework | `_$s10Foundation20ByteCountFormatStyleV0E0OMa` | 2 | coteditor, orbstack |
| Foundation.framework | `_$s10Foundation20ByteCountFormatStyleV5UnitsV3allAEvgZ` | 2 | coteditor, orbstack |
| Foundation.framework | `_$s10Foundation20ByteCountFormatStyleV5UnitsVMa` | 2 | coteditor, orbstack |
| Foundation.framework | `_$s10Foundation20ByteCountFormatStyleVAA0dE0AAMc` | 2 | coteditor, orbstack |
| Foundation.framework | `_$s10Foundation20ByteCountFormatStyleVMa` | 2 | coteditor, orbstack |
| Foundation.framework | `_$s10Foundation20ParseableFormatStylePA2A4DateV07ISO8601cD0VRszrlE7iso8601AGvgZ` | 2 | cmux, raycast |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO10NilLiteralVMn` | 2 | chatgpt, maccy |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO11build_Equal3lhs3rhsAC0E0Vy_xq_Gx_q_tAA0B10ExpressionRzAaJR_SQ6OutputRpzAKQy_ALRSr0_lFZ` | 2 | chatgpt, maccy |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO13build_KeyPath4root03keyF0AC0eF0Vy_xq_Gx_sAGCy6OutputQzq_GtAA0B10ExpressionRzr0_lFZ` | 2 | chatgpt, maccy |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO5EqualVMn` | 2 | chatgpt, maccy |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO5EqualVy_xq_GAA08StandardB10ExpressionA2aGRzAaGR_rlMc` | 2 | chatgpt, maccy |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO7KeyPathVMn` | 2 | chatgpt, maccy |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO7KeyPathVy_xq_GAA08StandardB10ExpressionA2aGRzrlMc` | 2 | chatgpt, maccy |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO7KeyPathVy_xq_GAA0B10ExpressionAAMc` | 2 | chatgpt, maccy |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO8VariableVMn` | 2 | chatgpt, maccy |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO8VariableVy_xGAA08StandardB10ExpressionAAMc` | 2 | chatgpt, maccy |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO8VariableVy_xGAA0B10ExpressionAAMc` | 2 | chatgpt, maccy |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO9build_ArgyxxAA0B10ExpressionRzlFZ` | 2 | chatgpt, maccy |
| Foundation.framework | `_$s10Foundation20ReferenceConvertibleTL` | 2 | clipy, raycast |
| Foundation.framework | `_$s10Foundation21_BridgedStoredNSErrorPAAE8userInfoSDySSypGvg` | 2 | clipy, sequel-ace |
| Foundation.framework | `_$s10Foundation22AttributeDynamicLookupO13dynamicMemberxs7KeyPathCyAA0B6ScopesO0A10AttributesVxG_tcAA016AttributedStringG0Rzluig` | 2 | cmux, coteditor |
| Foundation.framework | `_$s10Foundation22AttributeDynamicLookupO13dynamicMemberxs7KeyPathCyAA0B6ScopesO0A10AttributesVxG_tcAA016AttributedStringG0RzluipMV` | 2 | cmux, coteditor |
| Foundation.framework | `_$s10Foundation23LocalizedStringResourceV17BundleDescriptionO4mainyA2EmFWC` | 2 | cmux, coteditor |
| Foundation.framework | `_$s10Foundation23LocalizedStringResourceV17BundleDescriptionO5atURLyAeA0H0VcAEmFWC` | 2 | 1password, coteditor |
| Foundation.framework | `_$s10Foundation23LocalizedStringResourceV17BundleDescriptionO8forClassyAEyXlXpcAEmFWC` | 2 | clipy, coteditor |
| Foundation.framework | `_$s10Foundation23LocalizedStringResourceV6localeAA6LocaleVvs` | 2 | 1password, coteditor |
| Foundation.framework | `_$s10Foundation23LocalizedStringResourceV_5table6locale6bundle7commentACSSAAE17LocalizationValueV_SSSgAA6LocaleVAC17BundleDescriptionOs06StaticC0VSgtcfC` | 2 | clipy, coteditor |
| Foundation.framework | `_$s10Foundation24FloatingPointFormatStyleV6localeyACyxGAA6LocaleVF` | 2 | betterdisplay, coteditor |
| Foundation.framework | `_$s10Foundation24FloatingPointFormatStyleV8CurrencyV4code6localeAEyx_GSS_AA6LocaleVtcfC` | 2 | cmux, raycast |
| Foundation.framework | `_$s10Foundation24FloatingPointFormatStyleV8CurrencyVMn` | 2 | cmux, raycast |
| Foundation.framework | `_$s10Foundation24FloatingPointFormatStyleV8CurrencyVyx_GAA0dE0AAMc` | 2 | cmux, raycast |
| Foundation.framework | `_$s10Foundation3URLV17lastPathComponentSSvpMV` | 2 | coteditor, raycast |
| Foundation.framework | `_$s10Foundation3URLV20resolvingAliasFileAt7optionsA2Ch_So30NSURLBookmarkResolutionOptionsVtKcfC` | 2 | coteditor, raycast |
| Foundation.framework | `_$s10Foundation3URLV21resolveSymlinksInPathyyF` | 2 | orbstack, raycast |
| Foundation.framework | `_$s10Foundation3URLV28checkPromisedItemIsReachableSbyKF` | 2 | raycast, sequel-ace |
| Foundation.framework | `_$s10Foundation3URLV35fileURLWithFileSystemRepresentation11isDirectory10relativeToACSPys4Int8VG_SbACSghtcfC` | 2 | chatgpt, orbstack |
| Foundation.framework | `_$s10Foundation3URLV4host14percentEncodedSSSgSb_tF` | 2 | chatgpt, cmux |
| Foundation.framework | `_$s10Foundation4DataV11DeallocatorO4freeyA2EmFWC` | 2 | claude, orbstack |
| Foundation.framework | `_$s10Foundation4DataV11referencingACSo6NSDataCh_tcfC` | 2 | cmux, orbstack |
| Foundation.framework | `_$s10Foundation4DataV15_RepresentationO22withUnsafeMutableBytesyxxSwKXEKlF` | 2 | chatgpt, raycast |
| Foundation.framework | `_$s10Foundation4DataV15_RepresentationOys5UInt8VSicis` | 2 | betterdisplay, cmux |
| Foundation.framework | `_$s10Foundation4DataV15withUnsafeBytesyxxSWKXEKlF` | 2 | chatgpt, raycast |
| Foundation.framework | `_$s10Foundation4DataV16debugDescriptionSSvg` | 2 | chatgpt, iterm2 |
| Foundation.framework | `_$s10Foundation4DataV34_conditionallyBridgeFromObjectiveC_6resultSbSo6NSDataC_ACSgztFZ` | 2 | coteditor, ghostty |
| Foundation.framework | `_$s10Foundation4DataV8IteratorVMn` | 2 | betterdisplay, cmux |
| Foundation.framework | `_$s10Foundation4DataV8advanced2byACSi_tF` | 2 | iterm2, raycast |
| Foundation.framework | `_$s10Foundation4DateV11FormatStyleV6SymbolV6SecondV13defaultDigitsAIvgZ` | 2 | clipy, cmux |
| Foundation.framework | `_$s10Foundation4DateV11FormatStyleV6secondyA2E6SymbolV6SecondVF` | 2 | clipy, cmux |
| Foundation.framework | `_$s10Foundation4DateV11descriptionSSvg` | 2 | raycast, tailscale-app |
| Foundation.framework | `_$s10Foundation4DateV18ISO8601FormatStyleV0B13TimeSeparatorO5spaceyA2GmFWC` | 2 | clipy, coteditor |
| Foundation.framework | `_$s10Foundation4DateV18ISO8601FormatStyleVMn` | 2 | chatgpt, cmux |
| Foundation.framework | `_$s10Foundation4DateV8advanced2byACSd_tF` | 2 | orbstack, raycast |
| Foundation.framework | `_$s10Foundation4UUIDV11descriptionSSvg` | 2 | clipy, raycast |
| Foundation.framework | `_$s10Foundation6LocaleV15localizedString13forIdentifierSSSgSS_tF` | 2 | betterdisplay, coteditor |
| Foundation.framework | `_$s10Foundation6LocaleV6RegionV10identifierSSvg` | 2 | raycast, tailscale-app |
| Foundation.framework | `_$s10Foundation6LocaleV6RegionVMn` | 2 | coteditor, raycast |
| Foundation.framework | `_$s10Foundation6LocaleV6ScriptV13stringLiteralAESS_tcfC` | 2 | coteditor, raycast |
| Foundation.framework | `_$s10Foundation6LocaleV6ScriptVMa` | 2 | coteditor, raycast |
| Foundation.framework | `_$s10Foundation6LocaleV6ScriptVMn` | 2 | coteditor, raycast |
| Foundation.framework | `_$s10Foundation6LocaleV6regionAC6RegionVSgvg` | 2 | coteditor, raycast |
| Foundation.framework | `_$s10Foundation8CalendarV10identifierAC10IdentifierOvg` | 2 | clipy, raycast |
| Foundation.framework | `_$s10Foundation8CalendarV12dateInterval2of3forAA04DateD0VSgAC9ComponentO_AA0G0VtF` | 2 | chatgpt, raycast |
| Foundation.framework | `_$s10Foundation8CalendarV12firstWeekdaySivg` | 2 | clipy, raycast |
| Foundation.framework | `_$s10Foundation8CalendarV14MatchingPolicyO8nextTimeyA2EmFWC` | 2 | chatgpt, raycast |
| Foundation.framework | `_$s10Foundation8CalendarV14MatchingPolicyOMa` | 2 | chatgpt, raycast |
| Foundation.framework | `_$s10Foundation8CalendarV14dateComponents_4from2toAA04DateD0VShyAC9ComponentOG_AA0G0VAMtF` | 2 | iterm2, raycast |
| Foundation.framework | `_$s10Foundation8CalendarV15SearchDirectionO7forwardyA2EmFWC` | 2 | chatgpt, raycast |
| Foundation.framework | `_$s10Foundation8CalendarV15SearchDirectionOMa` | 2 | chatgpt, raycast |
| Foundation.framework | `_$s10Foundation8CalendarV18RepeatedTimePolicyO5firstyA2EmFWC` | 2 | chatgpt, raycast |
| Foundation.framework | `_$s10Foundation8CalendarV18RepeatedTimePolicyOMa` | 2 | chatgpt, raycast |
| Foundation.framework | `_$s10Foundation8CalendarV4date13bySettingHour6minute6second2of14matchingPolicy012repeatedTimeK09directionAA4DateVSgSi_S2iAmC08MatchingK0OAC08RepeatedmK0OAC15SearchDirectionOtF` | 2 | chatgpt, raycast |
| Foundation.framework | `_$s10Foundation8CalendarV4date8byAdding2to18wrappingComponentsAA4DateVSgAA0iH0V_AISbtF` | 2 | chatgpt, raycast |
| Foundation.framework | `_$s10Foundation8CalendarV6localeAA6LocaleVSgvg` | 2 | clipy, cmux |
| Foundation.framework | `_$s10Foundation8CalendarV9ComponentO10nanosecondyA2EmFWC` | 2 | chatgpt, raycast |
| Foundation.framework | `_$s10Foundation8CalendarV9ComponentO10weekOfYearyA2EmFWC` | 2 | chatgpt, raycast |
| Foundation.framework | `_$s10Foundation8CalendarV9ComponentO7weekdayyA2EmFWC` | 2 | cmux, raycast |
| Foundation.framework | `_$s10Foundation8IndexSetV08filteredbC014includeIntegerACSbSiKXE_tKF` | 2 | iterm2, raycast |
| Foundation.framework | `_$s10Foundation8IndexSetV2eeoiySbAC_ACtFZ` | 2 | cmux, iterm2 |
| Foundation.framework | `_$s10Foundation8IndexSetV4lastSiSgvg` | 2 | coteditor, raycast |
| Foundation.framework | `_$s10Foundation8IndexSetV6insert10integersInySnySiG_tF` | 2 | coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation8IndexSetV6insert10integersInyx_tSXRzSi5BoundRtzlF` | 2 | coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation8IndexSetV9RangeViewVSTAAMc` | 2 | coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation8TimeZoneV12abbreviation3forSSSgAA4DateV_tF` | 2 | clipy, raycast |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV12callIsActiveAEvgZ` | 2 | cmux, sequel-ace |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV14unsupportedURLAEvgZ` | 2 | cmux, iterm2 |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV19resourceUnavailableAEvgZ` | 2 | iterm2, sequel-ace |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV21cannotLoadFromNetworkAEvgZ` | 2 | cmux, sequel-ace |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV25clientCertificateRejectedAEvgZ` | 2 | cmux, iterm2 |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV25clientCertificateRequiredAEvgZ` | 2 | cmux, iterm2 |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV26serverCertificateUntrustedAEvgZ` | 2 | cmux, iterm2 |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV26userAuthenticationRequiredAEvgZ` | 2 | chatgpt, cmux |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV31serverCertificateHasUnknownRootAEvgZ` | 2 | cmux, iterm2 |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV8rawValueSivg` | 2 | cmux, iterm2 |
| Foundation.framework | `_$s10Foundation9IndexPathVSlAAMc` | 2 | chatgpt, raycast |
| Foundation.framework | `_$s10Foundation9PredicateVMn` | 2 | chatgpt, maccy |
| Foundation.framework | `_$s10Foundation9PredicateVyACyxxQp_QPGAA08StandardB10Expression_pSb6OutputAA0bD0PRts_XPAA0B11ExpressionsO8VariableVy_xGxQpXEcfC` | 2 | chatgpt, maccy |
| Foundation.framework | `_$s10Foundation9SortOrderO4hash4intoys6HasherVz_tF` | 2 | coteditor, raycast |
| Foundation.framework | `_$s10Foundation9SortOrderOMn` | 2 | coteditor, raycast |
| Foundation.framework | `_$s13ReferenceType10Foundation0A11ConvertiblePTl` | 2 | clipy, raycast |
| Foundation.framework | `_$s8Compared10Foundation14SortComparatorPTl` | 2 | coteditor, raycast |
| Foundation.framework | `_$sSD10FoundationE26_forceBridgeFromObjectiveC_6resultySo12NSDictionaryC_SDyxq_GSgztFZ` | 2 | iterm2, jordanbaird-ice |
| Foundation.framework | `_$sSS10FoundationE4data5using20allowLossyConversionAA4DataVSgSSAAE8EncodingV_SbtF` | 2 | claude, coteditor |
| Foundation.framework | `_$sSS10FoundationE6format6locale9argumentsS2Sh_AA6LocaleVSghSays7CVarArg_pGhtcfC` | 2 | clipy, cmux |
| Foundation.framework | `_$sSS10FoundationE8EncodingV10macOSRomanACvgZ` | 2 | chatgpt, raycast |
| Foundation.framework | `_$sSS10FoundationE8EncodingV5utf32ACvgZ` | 2 | coteditor, sequel-ace |
| Foundation.framework | `_$sSS10FoundationE8EncodingVSHAAMc` | 2 | coteditor, raycast |
| Foundation.framework | `_$sST10FoundationE6sorted5usingSay7ElementQzGqd___tAA14SortComparatorRd__8ComparedQyd__AERSlF` | 2 | coteditor, raycast |
| Foundation.framework | `_$sSa10Foundations5UInt8VRszlE7regionss15CollectionOfOneVySayACGGvg` | 2 | clipy, orbstack |
| Foundation.framework | `_$sSayxG10Foundation12DataProtocolABs5UInt8VRszlMc` | 2 | claude, orbstack |
| Foundation.framework | `_$sSd10Foundation18_FormatSpecifiableAAWP` | 2 | betterdisplay, tailscale-app |
| Foundation.framework | `_$sSn10FoundationAA16AttributedStringV5IndexVRszrlE_2inSnyAEGSgSo8_NSRangeV_qd__tcAA0bC8ProtocolRd__lufC` | 2 | coteditor, tailscale-app |
| Foundation.framework | `_$sSo10NSURLCacheC10FoundationE14memoryCapacity04diskD09directoryABSi_SiAC3URLVSgtcfC` | 2 | cmux, raycast |
| Foundation.framework | `_$sSo12NSDictionaryC10FoundationE10dictionaryA2Bh_tcfC` | 2 | iterm2, sequel-ace |
| Foundation.framework | `_$sSo12NSURLSessionC10FoundationE10AsyncBytesVSciACMc` | 2 | cmux, raycast |
| Foundation.framework | `_$sSo8_NSRangeV10FoundationE12intersectionyABSgABF` | 2 | coteditor, iterm2 |
| Foundation.framework | `_$sSo8_NSRangeVSH10FoundationMc` | 2 | coteditor, raycast |
| Foundation.framework | `_$sSo9NSScannerC10FoundationE10scanDouble14representationSdSgAbCE20NumberRepresentationO_tF` | 2 | betterdisplay, raycast |
| Foundation.framework | `_$sSo9NSScannerC10FoundationE12currentIndexSS0D0Vvg` | 2 | claude, cmux |
| Foundation.framework | `_$sSo9NSScannerC10FoundationE18scanUpToCharacters4fromSSSgAC12CharacterSetV_tF` | 2 | betterdisplay, raycast |
| Foundation.framework | `_$sSo9NSScannerC10FoundationE20NumberRepresentationO7decimalyA2EmFWC` | 2 | betterdisplay, raycast |
| Foundation.framework | `_$sSo9NSScannerC10FoundationE20NumberRepresentationOMa` | 2 | betterdisplay, raycast |
| Foundation.framework | `_$sSy10FoundationE11capitalized4withSSAA6LocaleVSg_tF` | 2 | chatgpt, raycast |
| Foundation.framework | `_$sSy10FoundationE14enumerateLines8invokingyySS_Sbztc_tF` | 2 | coteditor, iterm2 |
| Foundation.framework | `_$sSy10FoundationE25localizedStandardContainsySbqd__SyRd__lF` | 2 | cmux, raycast |
| Foundation.framework | `_$sSy10FoundationE36decomposedStringWithCanonicalMappingSSvg` | 2 | coteditor, iterm2 |
| Foundation.framework | `_$sSz10FoundationE9formattedy12FormatOutputQyd__qd__AA0C5StyleRd__Sz0C5InputRpd__lF` | 2 | coteditor, orbstack |
| Foundation.framework | `_$ss11AnyHashableV10FoundationE19_bridgeToObjectiveCSo8NSObjectCyF` | 2 | coteditor, iterm2 |
| Foundation.framework | `_$ss5ErrorP10FoundationAC13CustomNSErrorRzSYRzs17FixedWidthInteger8RawValueSYRpzrlE5_codeSivg` | 2 | chatgpt, cmux |
| Foundation.framework | `_$ss5Int32V10Foundation18_FormatSpecifiableACWP` | 2 | cmux, iterm2 |
| Foundation.framework | `_$ss5Int32V10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF` | 2 | clipy, iterm2 |
| Foundation.framework | `_$ss5Int64V10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF` | 2 | clipy, iterm2 |
| Foundation.framework | `_$ss6UInt32V10FoundationE10truncatingABSo8NSNumberCh_tcfC` | 2 | chatgpt, rectangle |
| Foundation.framework | `_$ss8DurationV10FoundationE16UnitsFormatStyleV29FractionalPartDisplayStrategyV4hideAGvgZ` | 2 | cmux, ghostty |
| Foundation.framework | `_$ss8DurationV10FoundationE16UnitsFormatStyleV4UnitV12millisecondsAGvgZ` | 2 | cmux, ghostty |
| Foundation.framework | `_$ss8DurationV10FoundationE16UnitsFormatStyleV9UnitWidthV11abbreviatedAGvgZ` | 2 | cmux, ghostty |
| Foundation.framework | `_NSArgumentDomain` | 2 | pencil, webtorrent |
| Foundation.framework | `_NSHFSTypeOfFile` | 2 | pencil, webtorrent |
| Foundation.framework | `_NSHTTPCookieMaximumAge` | 2 | cc-switch, zoom |
| Foundation.framework | `_NSHTTPCookieOriginURL` | 2 | cmux, iina |
| Foundation.framework | `_NSLocalizedFailureErrorKey` | 2 | coteditor, raycast |
| Foundation.framework | `_NSMetadataItemContentTypeTreeKey` | 2 | cmux, hammerspoon |
| Foundation.framework | `_NSMetadataQueryNetworkScope` | 2 | cmux, hammerspoon |
| Foundation.framework | `_NSRegistrationDomain` | 2 | coteditor, sequel-ace |
| Foundation.framework | `_NSStringEncodingDetectionAllowLossyKey` | 2 | cmux, transmission |
| Foundation.framework | `_NSURLAuthenticationMethodDefault` | 2 | cmux, hammerspoon |
| Foundation.framework | `_NSURLErrorBackgroundTaskCancelledReasonKey` | 2 | clipy, sequel-ace |
| Foundation.framework | `_OBJC_CLASS_$_NSArchiver` | 2 | hex-fiend, zoom |
| Foundation.framework | `_OBJC_CLASS_$_NSBackgroundActivityScheduler` | 2 | raycast, stats |
| Foundation.framework | `_OBJC_CLASS_$_NSPropertySpecifier` | 2 | coteditor, spotify |
| Foundation.framework | `_OBJC_CLASS_$_NSUbiquitousKeyValueStore` | 2 | maccy, whatsapp |
| Foundation.framework | `_OBJC_CLASS_$_NSUnitTemperature` | 2 | raycast, stats |
| Foundation.framework | `_OBJC_CLASS_$_NSUserUnixTask` | 2 | 1password, coteditor |
| libswiftFoundation.dylib | `_$s10Foundation10CocoaErrorV14fileNoSuchFileAC4CodeVvgZ` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10CocoaErrorV4CodeV19fileReadCorruptFileAEvgZ` | 2 | alt-tab, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10CocoaErrorV4CodeV19fileWriteFileExistsAEvgZ` | 2 | utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10CocoaErrorV4CodeV20fileReadNoPermissionAEvgZ` | 2 | obsidian, utm |
| libswiftFoundation.dylib | `_$s10Foundation10CocoaErrorV4CodeV23fileReadInvalidFileNameAEvgZ` | 2 | utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10URLRequestV10httpMethodSSSgvg` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10URLRequestV11cachePolicySo017NSURLRequestCacheD0Vvs` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10URLRequestV15timeoutIntervalSdvs` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10URLRequestV19allHTTPHeaderFieldsSDyS2SGSgvg` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10URLRequestV5value18forHTTPHeaderFieldSSSgSS_tF` | 2 | stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10URLRequestV8httpBodyAA4DataVSgvg` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONDecoderC20DateDecodingStrategyOMa` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONDecoderC20dateDecodingStrategyAC04DatedE0OvsTj` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC16OutputFormattingV22withoutEscapingSlashesAEvgZ` | 2 | 1password, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC16outputFormattingAC06OutputD0VvMTj` | 2 | hex-fiend, utm |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC20DateEncodingStrategyOMa` | 2 | 1password, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC20dateEncodingStrategyAC04DatedE0OvsTj` | 2 | 1password, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC34NonConformingFloatEncodingStrategyO15convertToStringyAESS_S2StcAEmFWC` | 2 | alt-tab, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC34NonConformingFloatEncodingStrategyOMa` | 2 | alt-tab, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC34nonConformingFloatEncodingStrategyAC03NondefG0OvsTj` | 2 | alt-tab, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV14urlHostAllowedACvgZ` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV17illegalCharactersACvgZ` | 2 | 1password, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV21punctuationCharactersACvgZ` | 2 | alt-tab, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV36_unconditionallyBridgeFromObjectiveCyACSo011NSCharacterC0CSgFZ` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV6insert12charactersInySS_tF` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV6remove12charactersInySS_tF` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV7symbolsACvgZ` | 2 | alt-tab, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV8newlinesACvgZ` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetVACycfC` | 2 | 1password, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetVMn` | 2 | claude, zoom |
| libswiftFoundation.dylib | `_$s10Foundation12DataProtocolPAAE9copyBytes2toSiSw_tF` | 2 | stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12DateIntervalV5start3endAcA0B0V_AGtcfC` | 2 | raycast, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12DateIntervalV5startAA0B0Vvg` | 2 | stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12DateIntervalVMa` | 2 | stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12DateIntervalVMn` | 2 | stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV19percentEncodedQuerySSSgvs` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV4pathSSvg` | 2 | whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV4pathSSvs` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV4portSiSgvg` | 2 | claude, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV6schemeSSSgvs` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsVACycfC` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13__DataStorageC8capacityACSi_tcfc` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13__DataStorageC9_capacitySivM` | 2 | utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsV07isValidB02inSbAA8CalendarV_tF` | 2 | raycast, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsV2eeoiySbAC_ACtFZ` | 2 | raycast, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsV36_unconditionallyBridgeFromObjectiveCyACSo06NSDateC0CSgFZ` | 2 | whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsV4hourSiSgvg` | 2 | whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsV4hourSiSgvs` | 2 | whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsV4yearSiSgvg` | 2 | stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsV5monthSiSgvg` | 2 | stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsV5value3forSiSgAA8CalendarV9ComponentO_tF` | 2 | raycast, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsV6minuteSiSgvg` | 2 | whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsV6minuteSiSgvs` | 2 | whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsV6secondSiSgvs` | 2 | whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsV8setValue_3forySiSg_AA8CalendarV9ComponentOtF` | 2 | raycast, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation15AffineTransformV14translationByX3byYAC12CoreGraphics7CGFloatV_AHtcfC` | 2 | iina, zoom |
| libswiftFoundation.dylib | `_$s10Foundation15_BridgedNSErrorMp` | 2 | claude, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation15_BridgedNSErrorP14_nsErrorDomainSSvgZTq` | 2 | claude, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation15_BridgedNSErrorP8RawValueSY_s17FixedWidthIntegerTn` | 2 | claude, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation15_BridgedNSErrorPAA26_ObjectiveCBridgeableErrorTb` | 2 | claude, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation15_BridgedNSErrorPAAE08_bridgedC0xSgSo0C0Ch_tcfC` | 2 | claude, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation15_BridgedNSErrorPAAE4hash4intoys6HasherVz_tF` | 2 | claude, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation15_BridgedNSErrorPAAE5_codeSivg` | 2 | claude, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation15_BridgedNSErrorPAAE7_domainSSvg` | 2 | claude, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation15_BridgedNSErrorPSHTb` | 2 | claude, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation15_BridgedNSErrorPSYTb` | 2 | claude, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation16AttributedStringVMa` | 2 | utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation17URLResourceValuesV17contentAccessDateAA0F0VSgvg` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation17URLResourceValuesV22fileResourceIdentifierSo9NSCopying_So14NSSecureCodingSo8NSObjectpSgvg` | 2 | utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation17URLResourceValuesV22totalFileAllocatedSizeSiSgvg` | 2 | utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation17URLResourceValuesV23contentModificationDateAA0F0VSgvg` | 2 | utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation17URLResourceValuesV40volumeAvailableCapacityForImportantUsages5Int64VSgvg` | 2 | stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation19PropertyListEncoderC6encodeyAA4DataVxKSERzlFTj` | 2 | utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation19PropertyListEncoderCACycfc` | 2 | utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation19PropertyListEncoderCMa` | 2 | utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation21NSKeyValueObservationCMa` | 2 | alt-tab, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation21_BridgedStoredNSErrorPAAE8userInfoSDySSypGvg` | 2 | drawio, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation24NSKeyValueObservedChangeV03newC0xSgvg` | 2 | alt-tab, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation24NSKeyValueObservedChangeV03oldC0xSgvg` | 2 | alt-tab, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation25NSFastEnumerationIteratorVStAAMc` | 2 | iina, zoom |
| libswiftFoundation.dylib | `_$s10Foundation3URLV08absoluteB0ACvg` | 2 | linearmouse, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation3URLV12relativePathSSvg` | 2 | utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation3URLV15fileURLWithPath10relativeToACSSh_ACSghtcfC` | 2 | linearmouse, utm |
| libswiftFoundation.dylib | `_$s10Foundation3URLV16hasDirectoryPathSbvg` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation3URLV19appendPathComponent_11isDirectoryySS_SbtF` | 2 | 1password, alt-tab |
| libswiftFoundation.dylib | `_$s10Foundation3URLV19appendPathExtensionyySSF` | 2 | utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation3URLV29removeAllCachedResourceValuesyyF` | 2 | stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation3URLV9hashValueSivg` | 2 | utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataV013base64EncodedB07optionsACSo27NSDataBase64EncodingOptionsV_tF` | 2 | utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataV11DeallocatorO12_deallocatoryySv_Sitcvg` | 2 | utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataV11DeallocatorO4noneyA2EmFWC` | 2 | utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataV11DeallocatorO6customyAEySv_SitccAEmFWC` | 2 | utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataV11DeallocatorOMa` | 2 | utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataV13base64Encoded7optionsACSgACh_So27NSDataBase64DecodingOptionsVtcfC` | 2 | utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataV15_RepresentationOys5UInt8VSicis` | 2 | stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataV8IteratorVStAAMc` | 2 | claude, utm |
| libswiftFoundation.dylib | `_$s10Foundation4DataV8IteratorV_2atAeC_SitcfC` | 2 | claude, utm |
| libswiftFoundation.dylib | `_$s10Foundation4DataV9copyBytes2to4fromSiSryxG_SnySiGSgtlF` | 2 | github, visual-studio-code |
| libswiftFoundation.dylib | `_$s10Foundation4DateV035timeIntervalBetween1970AndReferenceB0SdvgZ` | 2 | whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation4DateV1poiyA2C_SdtFZ` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateV3nowACvgZ` | 2 | 1password, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateV8distance2toSdAC_tF` | 2 | utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateV9formattedy12FormatOutputQzxAA0D5StyleRzAC0D5InputRtzlF` | 2 | utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateVs23CustomStringConvertibleAAMc` | 2 | linearmouse, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4UUIDV4uuidACs5UInt8V_A15Ft_tcfC` | 2 | stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4UUIDV4uuids5UInt8V_A15Ftvg` | 2 | stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation6LocaleV10regionCodeSSSgvg` | 2 | 1password, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation6LocaleV12languageCodeSSSgvg` | 2 | 1password, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation6LocaleV15localizedString13forIdentifierSSSgSS_tF` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation6LocaleV18preferredLanguagesSaySSGvgZ` | 2 | alt-tab, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation6LocaleV2eeoiySbAC_ACtFZ` | 2 | raycast, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation6LocaleVSQAAMc` | 2 | raycast, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation6LocaleVs23CustomStringConvertibleAAMc` | 2 | raycast, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV10ordinality2of2in3forSiSgAC9ComponentO_AjA4DateVtF` | 2 | raycast, stats |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV12dateInterval2of3forAA04DateD0VSgAC9ComponentO_AA0G0VtF` | 2 | stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV12firstWeekdaySivg` | 2 | stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV14MatchingPolicyO8nextTimeyA2EmFWC` | 2 | whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV14MatchingPolicyOMa` | 2 | whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV14dateComponents_4from2toAA04DateD0VShyAC9ComponentOG_AA0G0VAMtF` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV15SearchDirectionO7forwardyA2EmFWC` | 2 | whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV15SearchDirectionOMa` | 2 | whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV18RepeatedTimePolicyO5firstyA2EmFWC` | 2 | whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV18RepeatedTimePolicyOMa` | 2 | whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV19_bridgeToObjectiveCSo10NSCalendarCyF` | 2 | stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV36_unconditionallyBridgeFromObjectiveCyACSo10NSCalendarCSgFZ` | 2 | stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV4date13bySettingHour6minute6second2of14matchingPolicy012repeatedTimeK09directionAA4DateVSgSi_S2iAmC08MatchingK0OAC08RepeatedmK0OAC15SearchDirectionOtF` | 2 | whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV4date8byAdding2to18wrappingComponentsAA4DateVSgAA0iH0V_AISbtF` | 2 | whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV4date8byAdding5value2to18wrappingComponentsAA4DateVSgAC9ComponentO_SiAJSbtF` | 2 | whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV6isDate_7equalTo13toGranularitySbAA0D0V_AhC9ComponentOtF` | 2 | utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV6localeAA6LocaleVSgvg` | 2 | raycast, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV8nextDate5after8matching0F6Policy012repeatedTimeG09directionAA0D0VSgAK_AA0D10ComponentsVAC08MatchingG0OAC08RepeatediG0OAC15SearchDirectionOtF` | 2 | raycast, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV8timeZoneAA04TimeD0Vvg` | 2 | raycast, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV9ComponentO10weekOfYearyA2EmFWC` | 2 | stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV9ComponentO6secondyA2EmFWC` | 2 | whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV10integersInACx_tcSXRzSi5BoundRtzlufC` | 2 | iina, raycast |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV12intersectionyA2CF` | 2 | raycast, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV2eeoiySbAC_ACtFZ` | 2 | raycast, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV36_unconditionallyBridgeFromObjectiveCyACSo07NSIndexC0CSgFZ` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV6insert10integersInySnySiG_tF` | 2 | raycast, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV6removeySiSgSiF` | 2 | utm, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV7isEmptySbvg` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV8containsySbSiF` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV9RangeViewVMa` | 2 | raycast, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV9RangeViewVSlAAMc` | 2 | raycast, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV9formUnionyyACF` | 2 | raycast, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV9rangeViewAC05RangeE0Vvg` | 2 | raycast, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetVSHAAMc` | 2 | raycast, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetVSKAAMc` | 2 | raycast, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetVSTAAMc` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8TimeZoneV10identifierACSgSSh_tcfC` | 2 | stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8TimeZoneV14secondsFromGMT3forSiAA4DateV_tF` | 2 | stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8TimeZoneV14secondsFromGMTACSgSi_tcfC` | 2 | stats, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8TimeZoneV2eeoiySbAC_ACtFZ` | 2 | raycast, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8TimeZoneV36_unconditionallyBridgeFromObjectiveCyACSo06NSTimeC0CSgFZ` | 2 | whatsapp, zoom |
| libswiftFoundation.dylib | `_$s10Foundation8TimeZoneVSHAAMc` | 2 | raycast, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8TimeZoneVSQAAMc` | 2 | raycast, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8URLErrorV4CodeV21networkConnectionLostAEvgZ` | 2 | obsidian, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8URLErrorV4CodeV8timedOutAEvgZ` | 2 | obsidian, whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8URLErrorV4CodeVAA06_ErrorC8ProtocolAAMc` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$s12CoreGraphics7CGFloatV10FoundationE10truncatingACSo8NSNumberCh_tcfC` | 2 | localsend, whatsapp |
| libswiftFoundation.dylib | `_$sSRyxG10Foundation12DataProtocolABs5UInt8VRszlMc` | 2 | github, visual-studio-code |
| libswiftFoundation.dylib | `_$sSRyxG10Foundation15ContiguousBytesABs5UInt8VRszlMc` | 2 | github, visual-studio-code |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV14utf32BigEndianACvgZ` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV7unicodeACvgZ` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV9isoLatin1ACvgZ` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingVSQAAMc` | 2 | utm, whatsapp |
| libswiftFoundation.dylib | `_$sSa10FoundationE26_forceBridgeFromObjectiveC_6resultySo7NSArrayC_SayxGSgztFZ` | 2 | alt-tab, utm |
| libswiftFoundation.dylib | `_$sSi10FoundationE10truncatingSiSo8NSNumberCh_tcfC` | 2 | utm, whatsapp |
| libswiftFoundation.dylib | `_$sSo12NSDictionaryC10FoundationE10dictionaryA2Bh_tcfC` | 2 | whatsapp, zoom |
| libswiftFoundation.dylib | `_$sSo12NSDictionaryC10FoundationE8IteratorC4nextyp3key_yp5valuetSgyF` | 2 | claude, whatsapp |
| libswiftFoundation.dylib | `_$sSo12NSEnumeratorC10FoundationE12makeIteratorAC017NSFastEnumerationD0VyF` | 2 | utm, whatsapp |
| libswiftFoundation.dylib | `_$sSo13NSFileManagerC10FoundationE13replaceItemAt_04witheF006backupE4Name7optionsAC3URLVSgAI_AISSSgSo0abE18ReplacementOptionsVtKF` | 2 | utm, whatsapp |
| libswiftFoundation.dylib | `_$sSo7NSCoderC10FoundationE12decodeObject2of6forKeyxSgxm_SStSo8NSObjectCRbzSo8NSCodingRzlF` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$sSo8NSNumberC10FoundationE14booleanLiteralABSb_tcfC` | 2 | localsend, whatsapp |
| libswiftFoundation.dylib | `_$sSo9NSDecimala10FoundationE4hash4intoys6HasherVz_tF` | 2 | linearmouse, raycast |
| libswiftFoundation.dylib | `_$sSo9NSScannerC10FoundationE10scanStringySSSgSSF` | 2 | utm, whatsapp |
| libswiftFoundation.dylib | `_$sSo9NSScannerC10FoundationE14scanUpToStringySSSgSSF` | 2 | utm, whatsapp |
| libswiftFoundation.dylib | `_$sSo9NSScannerC10FoundationE20NumberRepresentationO7decimalyA2EmFWC` | 2 | claude, whatsapp |
| libswiftFoundation.dylib | `_$sSo9NSScannerC10FoundationE20NumberRepresentationOMa` | 2 | claude, whatsapp |
| libswiftFoundation.dylib | `_$sSy10FoundationE16localizedCompareySo18NSComparisonResultVqd__SyRd__lF` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$sSy10FoundationE20localizedCapitalizedSSvg` | 2 | whatsapp, zoom |
| libswiftFoundation.dylib | `_$sSy10FoundationE25localizedStandardContainsySbqd__SyRd__lF` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$sSy10FoundationE37precomposedStringWithCanonicalMappingSSvg` | 2 | iina, whatsapp |
| libswiftFoundation.dylib | `_$sSy10FoundationE5write6toFile10atomically8encodingyqd___SbSSAAE8EncodingVtKSyRd__lF` | 2 | iina, zoom |
| libswiftFoundation.dylib | `_$sSy10FoundationE7compare_7options5range6localeSo18NSComparisonResultVqd___So22NSStringCompareOptionsVSnySS5IndexVGSgAA6LocaleVSgtSyRd__lF` | 2 | alt-tab, whatsapp |
| libswiftFoundation.dylib | `_$sSy10FoundationE7folding7options6localeSSSo22NSStringCompareOptionsV_AA6LocaleVSgtF` | 2 | alt-tab, whatsapp |
| libswiftFoundation.dylib | `_$sSy10FoundationE7padding8toLength7withPad10startingAtSSSi_qd__SitSyRd__lF` | 2 | utm, whatsapp |
| libswiftFoundation.dylib | `_$ss10ArraySliceVyxG10Foundation15ContiguousBytesADs5UInt8VRszlMc` | 2 | claude, linearmouse |
| libswiftFoundation.dylib | `_$ss5Int32V10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF` | 2 | claude, whatsapp |
| libswiftFoundation.dylib | `_$ss5Int32V10FoundationE7exactlyABSgSo8NSNumberCh_tcfC` | 2 | whatsapp, zoom |
| libswiftFoundation.dylib | `_$ss5Int64V10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF` | 2 | raycast, whatsapp |
| libswiftFoundation.dylib | `_$ss6UInt16V10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF` | 2 | whatsapp, zoom |
| CoreData.framework | `_NSInvalidatedAllObjectsKey` | 1 | whatsapp |
| CoreData.framework | `_NSInvalidatedObjectsKey` | 1 | whatsapp |
| CoreData.framework | `_NSSQLiteManualVacuumOption` | 1 | iterm2 |
| CoreData.framework | `_OBJC_CLASS_$_NSBatchDeleteRequest` | 1 | whatsapp |
| CoreData.framework | `_OBJC_CLASS_$_NSBatchDeleteResult` | 1 | whatsapp |
| CoreData.framework | `_OBJC_CLASS_$_NSBatchInsertRequest` | 1 | whatsapp |
| CoreData.framework | `_OBJC_CLASS_$_NSBatchUpdateRequest` | 1 | whatsapp |
| CoreData.framework | `_OBJC_CLASS_$_NSBatchUpdateResult` | 1 | whatsapp |
| CoreData.framework | `_OBJC_CLASS_$_NSExpressionDescription` | 1 | whatsapp |
| CoreData.framework | `_OBJC_CLASS_$_NSFetchIndexDescription` | 1 | whatsapp |
| CoreData.framework | `_OBJC_CLASS_$_NSFetchIndexElementDescription` | 1 | whatsapp |
| CoreData.framework | `_OBJC_CLASS_$_NSFetchedResultsController` | 1 | whatsapp |
| CoreData.framework | `_OBJC_CLASS_$_NSQueryGenerationToken` | 1 | whatsapp |
| CoreData.framework | `_OBJC_CLASS_$_NSRelationshipDescription` | 1 | whatsapp |
| CoreData.framework | `_OBJC_METACLASS_$_NSFetchedResultsController` | 1 | whatsapp |
| CoreFoundation.framework | `_CFArrayBSearchValues` | 1 | hex-fiend |
| CoreFoundation.framework | `_CFAttributedStringBeginEditing` | 1 | openttd |
| CoreFoundation.framework | `_CFAttributedStringEndEditing` | 1 | openttd |
| CoreFoundation.framework | `_CFAttributedStringReplaceAttributedString` | 1 | zoom |
| CoreFoundation.framework | `_CFAttributedStringSetAttributes` | 1 | utm |
| CoreFoundation.framework | `_CFBundleCopyAuxiliaryExecutableURL` | 1 | kitty |
| CoreFoundation.framework | `_CFBundleCopyExecutableArchitectures` | 1 | android-studio |
| CoreFoundation.framework | `_CFBundleCopyLocalizationsForPreferences` | 1 | vlc |
| CoreFoundation.framework | `_CFBundleCopyResourceURLInDirectory` | 1 | zoom |
| CoreFoundation.framework | `_CFBundleIsExecutableLoaded` | 1 | android-studio |
| CoreFoundation.framework | `_CFBundleLoadExecutable` | 1 | zoom |
| CoreFoundation.framework | `_CFDictionaryContainsValue` | 1 | obs |
| CoreFoundation.framework | `_CFMachPortCreateWithPort` | 1 | zoom |
| CoreFoundation.framework | `_CFMachPortGetPort` | 1 | zoom |
| CoreFoundation.framework | `_CFMessagePortGetContext` | 1 | zoom |
| CoreFoundation.framework | `_CFMessagePortIsRemote` | 1 | hammerspoon |
| CoreFoundation.framework | `_CFMessagePortSetName` | 1 | zoom |
| CoreFoundation.framework | `_CFNumberGetByteSize` | 1 | localsend |
| CoreFoundation.framework | `_CFStringCompareWithOptions` | 1 | audacity |
| CoreFoundation.framework | `_CFStringCreateByCombiningStrings` | 1 | lmms |
| CoreFoundation.framework | `_CFStringCreateWithFormatAndArguments` | 1 | audacity |
| CoreFoundation.framework | `_CFStringFindWithOptionsAndLocale` | 1 | openttd |
| CoreFoundation.framework | `_CFStringGetListOfAvailableEncodings` | 1 | sequel-ace |
| CoreFoundation.framework | `_CFStringGetMostCompatibleMacStringEncoding` | 1 | sequel-ace |
| CoreFoundation.framework | `_CFURLCopyPathExtension` | 1 | betterdisplay |
| CoreFoundation.framework | `_CFURLCreateDataAndPropertiesFromResource` | 1 | prusaslicer |
| CoreFoundation.framework | `_CFURLCreateStringByReplacingPercentEscapesUsingEncoding` | 1 | webtorrent |
| CoreFoundation.framework | `_CFWriteStreamCreateWithFile` | 1 | utm |
| CoreFoundation.framework | `_NSGregorianCalendar` | 1 | vlc |
| CoreFoundation.framework | `_NSLocaleMeasurementSystem` | 1 | hammerspoon |
| CoreFoundation.framework | `_NSStreamSOCKSProxyConfigurationKey` | 1 | hammerspoon |
| CoreFoundation.framework | `_NSStreamSOCKSProxyHostKey` | 1 | hammerspoon |
| CoreFoundation.framework | `_NSStreamSOCKSProxyPasswordKey` | 1 | hammerspoon |
| CoreFoundation.framework | `_NSStreamSOCKSProxyPortKey` | 1 | hammerspoon |
| CoreFoundation.framework | `_NSStreamSOCKSProxyUserKey` | 1 | hammerspoon |
| CoreFoundation.framework | `_NSStringTransformFullwidthToHalfwidth` | 1 | coteditor |
| CoreFoundation.framework | `_NSStringTransformStripDiacritics` | 1 | raycast |
| CoreFoundation.framework | `_NSURLFileResourceTypeBlockSpecial` | 1 | iterm2 |
| CoreFoundation.framework | `_NSURLFileResourceTypeCharacterSpecial` | 1 | iterm2 |
| CoreFoundation.framework | `_NSURLFileResourceTypeNamedPipe` | 1 | iterm2 |
| CoreFoundation.framework | `_NSURLFileResourceTypeSocket` | 1 | iterm2 |
| CoreFoundation.framework | `_NSURLUbiquitousItemContainerDisplayNameKey` | 1 | raycast |
| CoreFoundation.framework | `_NSURLVolumeAvailableCapacityForOpportunisticUsageKey` | 1 | obs |
| CoreFoundation.framework | `_NSURLVolumeIsRootFileSystemKey` | 1 | raycast |
| CoreFoundation.framework | `_kCFGregorianCalendar` | 1 | android-studio |
| CoreFoundation.framework | `_kCFStreamErrorDomainSOCKS` | 1 | hammerspoon |
| CoreFoundation.framework | `_kCFStreamErrorDomainSSL` | 1 | hammerspoon |
| CoreFoundation.framework | `_kCFStreamSocketSecurityLevelSSLv2` | 1 | vlc |
| CoreFoundation.framework | `_kCFStreamSocketSecurityLevelSSLv3` | 1 | vlc |
| CoreFoundation.framework | `_kCFStreamSocketSecurityLevelTLSv1` | 1 | vlc |
| CoreFoundation.framework | `_kCFURLVolumeLocalizedNameKey` | 1 | blender |
| Foundation.framework | `_$s10Foundation10CocoaErrorV11errorDomainSSvgZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation10CocoaErrorV14fileNoSuchFileAC4CodeVvgZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation10CocoaErrorV15fileReadUnknownAC4CodeVvgZ` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation10CocoaErrorV16fileWriteUnknownAC4CodeVvgZ` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation10CocoaErrorV21fileWriteNoPermissionAC4CodeVvgZ` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation10CocoaErrorV4CodeV16fileReadTooLargeAEvgZ` | 1 | cmux |
| Foundation.framework | `_$s10Foundation10CocoaErrorV4CodeV18coderValueNotFoundAEvgZ` | 1 | sequel-ace |
| Foundation.framework | `_$s10Foundation10CocoaErrorV4CodeV19fileWriteFileExistsAEvgZ` | 1 | cmux |
| Foundation.framework | `_$s10Foundation10CocoaErrorV4CodeV20fileReadNoPermissionAEvgZ` | 1 | cmux |
| Foundation.framework | `_$s10Foundation10CocoaErrorV4CodeV21fileWriteNoPermissionAEvgZ` | 1 | cmux |
| Foundation.framework | `_$s10Foundation10CocoaErrorV4CodeV23fileReadInvalidFileNameAEvgZ` | 1 | cmux |
| Foundation.framework | `_$s10Foundation10CocoaErrorV4CodeV23propertyListReadCorruptAEvgZ` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation10CocoaErrorV4CodeV24fileWriteInvalidFileNameAEvgZ` | 1 | cmux |
| Foundation.framework | `_$s10Foundation10CocoaErrorV4CodeV25fileReadUnsupportedSchemeAEvgZ` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation10CocoaErrorV4CodeV29fileReadUnknownStringEncodingAEvgZ` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation10CocoaErrorV4CodeV34fileReadInapplicableStringEncodingAEvgZ` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation10CocoaErrorV4CodeV35fileWriteInapplicableStringEncodingAEvgZ` | 1 | cmux |
| Foundation.framework | `_$s10Foundation10CocoaErrorV4CodeVMn` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation10CocoaErrorVMn` | 1 | cmux |
| Foundation.framework | `_$s10Foundation10POSIXErrorV9ECANCELED6Darwin0B4CodeOvgZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation10POSIXErrorVs5ErrorAAMc` | 1 | raycast |
| Foundation.framework | `_$s10Foundation10URLRequestV14httpBodyStreamSo07NSInputE0CSgvg` | 1 | cmux |
| Foundation.framework | `_$s10Foundation10URLRequestV14httpBodyStreamSo07NSInputE0CSgvs` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation10URLRequestV15mainDocumentURLAA0E0VSgvg` | 1 | sequel-ace |
| Foundation.framework | `_$s10Foundation10URLRequestV15timeoutIntervalSdvg` | 1 | raycast |
| Foundation.framework | `_$s10Foundation10URLRequestV20allowsCellularAccessSbvg` | 1 | raycast |
| Foundation.framework | `_$s10Foundation10URLRequestV2eeoiySbAC_ACtFZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation10URLRequestVSHAAMc` | 1 | raycast |
| Foundation.framework | `_$s10Foundation11FormatStyleMp` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation11FormatStyleP6formaty0B6OutputQz0B5InputQzFTq` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation11FormatStyleP6localeyxAA6LocaleVFTq` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation11FormatStylePA2A3URLVABVRszrlE3urlAFvgZ` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation11FormatStylePA2A4DateV08RelativebC0VRszrlE8relative12presentation05unitsC0A2G12PresentationV_AG05UnitsC0VtFZ` | 1 | cmux |
| Foundation.framework | `_$s10Foundation11FormatStylePAAE11measurement5width5usage14hidesScaleName06numberbC0AA11MeasurementVAASo11NSDimensionCRbzrlEABVySo17NSUnitTemperatureC_GAM9UnitWidthVyAO__G_AA0kbO5UsageVyAOGSbAA013FloatingPointbC0VySdGSgtAPRszrlFZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation11FormatStylePAAE6localeyxAA6LocaleVF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation11FormatStylePSETb` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation11FormatStylePSHTb` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation11FormatStylePSeTb` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation11JSONDecoderC11allowsJSON5SbvsTj` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation11JSONDecoderC19KeyDecodingStrategyO14useDefaultKeysyA2EmFWC` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation11JSONDecoderC19KeyDecodingStrategyO6customyAEs06CodingC0_pSaysAG_pGccAEmFWC` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation11JSONDecoderC19KeyDecodingStrategyOMn` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation11JSONDecoderC20DateDecodingStrategyO010deferredToC0yA2EmFWC` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation11JSONDecoderC20DateDecodingStrategyOMn` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation11JSONDecoderCACycfC` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation11JSONDecoderCACycfCTj` | 1 | raycast |
| Foundation.framework | `_$s10Foundation11JSONEncoderC16OutputFormattingVSQAAMc` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation11JSONEncoderC6encodeyAA4DataVxKSERzlFTq` | 1 | raycast |
| Foundation.framework | `_$s10Foundation11JSONEncoderC8userInfoSDys010CodingUserD3KeyVypGvMTj` | 1 | raycast |
| Foundation.framework | `_$s10Foundation11JSONEncoderCACycfC` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation11JSONEncoderCACycfCTj` | 1 | raycast |
| Foundation.framework | `_$s10Foundation11JSONEncoderCACycfCTq` | 1 | raycast |
| Foundation.framework | `_$s10Foundation11JSONEncoderCMm` | 1 | raycast |
| Foundation.framework | `_$s10Foundation11JSONEncoderCMu` | 1 | raycast |
| Foundation.framework | `_$s10Foundation11JSONEncoderCfd` | 1 | raycast |
| Foundation.framework | `_$s10Foundation11MeasurementV19_bridgeToObjectiveCSo13NSMeasurementCyF` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation11MeasurementV2eeoiySbACyqd__G_ACyqd_0_GtSo6NSUnitCRbd__AHRbd_0_r0_lFZ` | 1 | jordanbaird-ice |
| Foundation.framework | `_$s10Foundation11MeasurementV4unitxvg` | 1 | raycast |
| Foundation.framework | `_$s10Foundation11MeasurementVAASo11NSDimensionCRbzrlE11FormatStyleV9UnitWidthV6narrowAIyx__GvgZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation11MeasurementVAASo11NSDimensionCRbzrlE11FormatStyleV9UnitWidthVMn` | 1 | raycast |
| Foundation.framework | `_$s10Foundation11MeasurementVAASo11NSDimensionCRbzrlE11FormatStyleVMn` | 1 | raycast |
| Foundation.framework | `_$s10Foundation11MeasurementVAASo11NSDimensionCRbzrlE11FormatStyleVyx_GAafAMc` | 1 | raycast |
| Foundation.framework | `_$s10Foundation11MeasurementVAASo11NSDimensionCRbzrlE9formattedy12FormatOutputQyd__qd__AA0E5StyleRd__ACyxG0E5InputRtd__lF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation11MeasurementVyxGs23CustomStringConvertibleAAMc` | 1 | tailscale-app |
| Foundation.framework | `_$s10Foundation12CharacterSetV14urlUserAllowedACvgZ` | 1 | tailscale-app |
| Foundation.framework | `_$s10Foundation12CharacterSetV17illegalCharactersACvgZ` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation12CharacterSetV36_unconditionallyBridgeFromObjectiveCyACSo011NSCharacterC0CSgFZ` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation12CharacterSetV6insertySb8inserted_s7UnicodeO6ScalarV17memberAfterInserttAIF` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation12CharacterSetV7symbolsACvgZ` | 1 | cmux |
| Foundation.framework | `_$s10Foundation12CharacterSetVSQAAMc` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation12DataProtocolMp` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation12DataProtocolP10firstRange2of2inSny5IndexQzGSgqd___qd_0_tAaBRd__SXRd_0_5BoundQyd_0_AHRSr0_lFTq` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation12DataProtocolP11SubSequenceSl_AaBTn` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation12DataProtocolP7RegionsAC_7ElementST11SubSequenceSlAA15ContiguousBytesTn` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation12DataProtocolP7RegionsAC_7ElementSTAA15ContiguousBytesTn` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation12DataProtocolP7RegionsAC_7ElementSTAaBTn` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation12DataProtocolP7RegionsAC_SKTn` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation12DataProtocolP7regions7RegionsQzvgTj` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation12DataProtocolP7regions7RegionsQzvgTq` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation12DataProtocolP9copyBytes2to4fromSiSryqd__G_qd_0_tSXRd_0_5BoundQyd_0_5IndexRtzr0_lFTq` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation12DataProtocolP9copyBytes2to4fromSiSw_qd__tSXRd__5BoundQyd__5IndexRtzlFTq` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation12DataProtocolP9copyBytes2to5countSiSryqd__G_SitlFTq` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation12DataProtocolP9copyBytes2to5countSiSw_SitFTq` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation12DataProtocolP9lastRange2of2inSny5IndexQzGSgqd___qd_0_tAaBRd__SXRd_0_5BoundQyd_0_AHRSr0_lFTq` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation12DataProtocolPAAE10firstRange2of2inSny5IndexQzGSgqd___qd_0_tAaBRd__SXRd_0_5BoundQyd_0_AHRSr0_lF` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation12DataProtocolPAAE9copyBytes2to4fromSiSryqd__G_qd_0_tSXRd_0_5BoundQyd_0_5IndexRtzr0_lF` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation12DataProtocolPAAE9copyBytes2to5countSiSryqd__G_SitlF` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation12DataProtocolPAAE9copyBytes2to5countSiSw_SitF` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation12DataProtocolPAAE9lastRange2of2inSny5IndexQzGSgqd___qd_0_tAaBRd__SXRd_0_5BoundQyd_0_AHRSr0_lF` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation12DataProtocolPSkTb` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation12DataProtocolTL` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation12DateIntervalV2eeoiySbAC_ACtFZ` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation12DateIntervalV36_unconditionallyBridgeFromObjectiveCyACSo06NSDateC0CSgFZ` | 1 | sequel-ace |
| Foundation.framework | `_$s10Foundation12DateIntervalV5start3endAcA0B0V_AGtcfC` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation12DateIntervalV8containsySbAA0B0VF` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation12DateIntervalV8durationSdvg` | 1 | sequel-ace |
| Foundation.framework | `_$s10Foundation12DateIntervalVSHAAMc` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation12DateIntervalVSQAAMc` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation12URLQueryItemV36_unconditionallyBridgeFromObjectiveCyACSo010NSURLQueryC0CSgFZ` | 1 | clipy |
| Foundation.framework | `_$s10Foundation12URLQueryItemV4nameSSvpMV` | 1 | raycast |
| Foundation.framework | `_$s10Foundation12URLQueryItemV4nameSSvs` | 1 | raycast |
| Foundation.framework | `_$s10Foundation12URLQueryItemVs23CustomStringConvertibleAAMc` | 1 | raycast |
| Foundation.framework | `_$s10Foundation13CustomNSErrorPAAE11errorDomainSSvgZ` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation13ParseStrategyP5parsey0B6OutputQz0B5InputQzKFTj` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation13ParseStrategyP5parsey0B6OutputQz0B5InputQzKFTq` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation13ParseStrategyPSETb` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation13ParseStrategyPSHTb` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation13ParseStrategyPSeTb` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation13URLComponentsV11encodedHostSSSgvg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation13URLComponentsV18percentEncodedHostSSSgvg` | 1 | cmux |
| Foundation.framework | `_$s10Foundation13URLComponentsV18percentEncodedUserSSSgvg` | 1 | cmux |
| Foundation.framework | `_$s10Foundation13URLComponentsV22percentEncodedFragmentSSSgvg` | 1 | cmux |
| Foundation.framework | `_$s10Foundation13URLComponentsV22percentEncodedPasswordSSSgvg` | 1 | cmux |
| Foundation.framework | `_$s10Foundation13URLComponentsV24percentEncodedQueryItemsSayAA12URLQueryItemVGSgvg` | 1 | raycast |
| Foundation.framework | `_$s10Foundation13URLComponentsV24percentEncodedQueryItemsSayAA12URLQueryItemVGSgvs` | 1 | raycast |
| Foundation.framework | `_$s10Foundation13URLComponentsV36_unconditionallyBridgeFromObjectiveCyACSo15NSURLComponentsCSgFZ` | 1 | clipy |
| Foundation.framework | `_$s10Foundation13URLComponentsVs23CustomStringConvertibleAAMc` | 1 | tailscale-app |
| Foundation.framework | `_$s10Foundation14AttributeScopeMp` | 1 | cmux |
| Foundation.framework | `_$s10Foundation14AttributeScopeP21decodingConfigurationAA0bc7CodableE0VvgZTq` | 1 | cmux |
| Foundation.framework | `_$s10Foundation14AttributeScopeP21encodingConfigurationAA0bc7CodableE0VvgZTq` | 1 | cmux |
| Foundation.framework | `_$s10Foundation14AttributeScopePAA30DecodingConfigurationProvidingTb` | 1 | cmux |
| Foundation.framework | `_$s10Foundation14AttributeScopePAA30EncodingConfigurationProvidingTb` | 1 | cmux |
| Foundation.framework | `_$s10Foundation14AttributeScopePAAE21decodingConfigurationAA0bc7CodableE0VvgZ` | 1 | cmux |
| Foundation.framework | `_$s10Foundation14AttributeScopePAAE21encodingConfigurationAA0bc7CodableE0VvgZ` | 1 | cmux |
| Foundation.framework | `_$s10Foundation14DateComponentsV19_bridgeToObjectiveCSo06NSDateC0CyF` | 1 | tailscale-app |
| Foundation.framework | `_$s10Foundation14DateComponentsV2eeoiySbAC_ACtFZ` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation14DateComponentsV5value3forSiSgAA8CalendarV9ComponentO_tF` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation14DateComponentsV7weekdaySiSgvg` | 1 | cmux |
| Foundation.framework | `_$s10Foundation14DateComponentsV8setValue_3forySiSg_AA8CalendarV9ComponentOtF` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation14DateComponentsVSHAAMc` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation14DateComponentsVSQAAMc` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation14DateComponentsVs23CustomStringConvertibleAAMc` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation14PredicateErrorV12invalidInputACvgZ` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation14PredicateErrorVMa` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation14PredicateErrorVs0C0AAMc` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation14SortComparatorPAASSAAE0C0VRszrlE17localizedStandardAEvgZ` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation14SortComparatorPAASSAAE0C0VRszrlE9localizedAEvgZ` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation15AffineTransformV3m113m123m213m222tX2tYAC12CoreGraphics7CGFloatV_A5LtcfC` | 1 | raycast |
| Foundation.framework | `_$s10Foundation15AffineTransformV6rotate9byDegreesy12CoreGraphics7CGFloatV_tF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation15AffineTransformV9translate1x1yy12CoreGraphics7CGFloatV_AItF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation15AffineTransformVACycfC` | 1 | raycast |
| Foundation.framework | `_$s10Foundation15AttributeScopesO0A10AttributesV024InlinePresentationIntentB0OMn` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation15AttributeScopesO0A10AttributesV09ByteCountB0O9ComponentO5valueyA2ImFWC` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation15AttributeScopesO0A10AttributesV09ByteCountB0O9ComponentOMa` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation15AttributeScopesO0A10AttributesV09ByteCountB0OAA19AttributedStringKeyAAMc` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation15AttributeScopesO0A10AttributesV09ByteCountB0OMn` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation15AttributeScopesO0A10AttributesV09ByteCountB0ON` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation15AttributeScopesO0A10AttributesV24inlinePresentationIntentAE06InlinefgB0Ovg` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation15AttributeScopesO0A10AttributesV24inlinePresentationIntentAE06InlinefgB0OvpMV` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation15AttributeScopesO0A10AttributesV9byteCountAE04BytefB0Ovg` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation15AttributeScopesO0A10AttributesV9byteCountAE04BytefB0OvpMV` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation15AttributeScopesOMn` | 1 | cmux |
| Foundation.framework | `_$s10Foundation15ContiguousBytesP010withUnsafeC0yqd__qd__SWKXEKlFTq` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation16AttributedStringV13CharacterViewV10startIndexAC0G0Vvg` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16AttributedStringV13CharacterViewV6_index_8offsetByAC5IndexVAI_SitF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16AttributedStringV13CharacterViewV8endIndexAC0G0Vvg` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16AttributedStringV13CharacterViewV9_distance4from2toSiAC5IndexV_AJtF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16AttributedStringV13CharacterViewVSKAAMc` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16AttributedStringV14removeSubrangeyyxSXRzAC5IndexV5BoundRtzlF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16AttributedStringV15replaceSubrange_4withyx_q_tSXRzAA0bC8ProtocolR_AC5IndexV5BoundRtzr0_lF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation16AttributedStringV1poiyA2C_ACtFZ` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16AttributedStringV22transformingAttributesyACs7KeyPathCyAA22AttributeDynamicLookupOxG_AFyAHq_GyAC06SingleH11TransformerVy_xGz_ALy_q_GztXEtAA0bcF0RzAaOR_r0_lF` | 1 | cmux |
| Foundation.framework | `_$s10Foundation16AttributedStringV26SingleAttributeTransformerV5value5ValueQzSgvg` | 1 | cmux |
| Foundation.framework | `_$s10Foundation16AttributedStringV26SingleAttributeTransformerV5value5ValueQzSgvs` | 1 | cmux |
| Foundation.framework | `_$s10Foundation16AttributedStringV26SingleAttributeTransformerVMn` | 1 | cmux |
| Foundation.framework | `_$s10Foundation16AttributedStringV4RunsV3RunVMn` | 1 | raycast |
| Foundation.framework | `_$s10Foundation16AttributedStringV4RunsV3RunVy5ValueQzSgxmcAA0bC3KeyRzluig` | 1 | cmux |
| Foundation.framework | `_$s10Foundation16AttributedStringV4RunsVSTAAMc` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16AttributedStringV6insert_2atyx_AC5IndexVtAA0bC8ProtocolRzlF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16AttributedStringV9localizedAcA09LocalizedC8ResourceV_tcfC` | 1 | ghostty |
| Foundation.framework | `_$s10Foundation16AttributedStringVSHAAMc` | 1 | maccy |
| Foundation.framework | `_$s10Foundation16AttributedStringVSQAAMc` | 1 | maccy |
| Foundation.framework | `_$s10Foundation16AttributedStringV_10attributesACx_AA18AttributeContainerVtcSTRzSJ7ElementRtzlufC` | 1 | raycast |
| Foundation.framework | `_$s10Foundation16RecoverableErrorMp` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16RecoverableErrorP15attemptRecovery11optionIndex13resultHandlerySi_ySbctFTq` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16RecoverableErrorP15attemptRecovery11optionIndexSbSi_tFTq` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16RecoverableErrorP15recoveryOptionsSaySSGvgTq` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16RecoverableErrorPAAE15attemptRecovery11optionIndex13resultHandlerySi_ySbctF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16RecoverableErrorPs0C0Tb` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation17AsyncLineSequenceV0B8IteratorVyx_GScIAAMc` | 1 | raycast |
| Foundation.framework | `_$s10Foundation17AsyncLineSequenceVyxGSciAAMc` | 1 | raycast |
| Foundation.framework | `_$s10Foundation17KeyPathComparatorV_10comparatorACyxGs0bC0Cyxqd__G_qd_0_tc8ComparedQyd_0_Rsd__AA04SortD0Rd_0_r0_lufC` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation17KeyPathComparatorV_5orderACyxGs0bC0Cyxqd__SgG_AA9SortOrderOtcSLRd__lufC` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation17KeyPathComparatorVyxGAA04SortD0AAMc` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation17KeyPathComparatorVyxGSQAAMc` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation17PredicateBindingsVMa` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation17PredicateBindingsVyAcA0B11ExpressionsO8VariableVy_xG_xtxQpcRvzlufC` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV10isReadableSbSgvg` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV10isWritableSbSgvg` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV10volumeNameSSSgvg` | 1 | cmux |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV13canonicalPathSSSgvg` | 1 | cmux |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV13totalFileSizeSiSgvg` | 1 | raycast |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV13volumeIsLocalSbSgvg` | 1 | raycast |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV16fileResourceTypeSo09NSURLFileeF0aSgvg` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV16volumeIsInternalSbSgvg` | 1 | raycast |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV16volumeIsReadOnlySbSgvg` | 1 | raycast |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV17contentAccessDateAA0F0VSgvg` | 1 | raycast |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV19volumeTotalCapacitySiSgvg` | 1 | raycast |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV20quarantinePropertiesSDySSypGSgvs` | 1 | cmux |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV22volumeIsRootFileSystemSbSgvg` | 1 | raycast |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV25attributeModificationDateAA0F0VSgvg` | 1 | raycast |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV34ubiquitousItemContainerDisplayNameSSSgvg` | 1 | raycast |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV40volumeAvailableCapacityForImportantUsages5Int64VSgvg` | 1 | raycast |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV8tagNamesSaySSGSgvg` | 1 | raycast |
| Foundation.framework | `_$s10Foundation18AttributeContainerV13dynamicMemberAC7BuilderVy_xGs7KeyPathCyAA0B13DynamicLookupOxG_tcAA016AttributedStringG0RzluigZ` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation18AttributeContainerV5merge_0D6PolicyyAC_AA16AttributedStringV0b5MergeE0OtF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation18AttributeContainerVy5ValueQzSgxmcAA19AttributedStringKeyRzluig` | 1 | cmux |
| Foundation.framework | `_$s10Foundation18IntegerFormatStyleV6formatySSxF` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation18IntegerFormatStyleV7PercentV6localeAEyx_GAA6LocaleV_tcfC` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation18IntegerFormatStyleV7PercentVMn` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation18IntegerFormatStyleV7PercentVyx_GAA0cD0AAMc` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation18IntegerFormatStyleV8notationyACyxGAA06NumbercD13ConfigurationO8NotationVF` | 1 | cmux |
| Foundation.framework | `_$s10Foundation18IntegerFormatStyleV9precisionyACyxGAA06NumbercD13ConfigurationO9PrecisionVF` | 1 | cmux |
| Foundation.framework | `_$s10Foundation18IntegerFormatStyleVMa` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation19AttributedStringKeyMp` | 1 | cmux |
| Foundation.framework | `_$s10Foundation19AttributedStringKeyP4nameSSvgZTq` | 1 | cmux |
| Foundation.framework | `_$s10Foundation19AttributedStringKeyP5ValueAC_SHTn` | 1 | cmux |
| Foundation.framework | `_$s10Foundation19AttributedSubstringV10charactersAA0B6StringV13CharacterViewVvg` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation19AttributedSubstringV15mergeAttributes_0D6PolicyyAA18AttributeContainerV_AA0B6StringV0g5MergeF0OtF` | 1 | tailscale-app |
| Foundation.framework | `_$s10Foundation19AttributedSubstringV4runsAA0B6StringV4RunsVvg` | 1 | raycast |
| Foundation.framework | `_$s10Foundation19MutableDataProtocolMp` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation19MutableDataProtocolP10resetBytes2inyqd___tSXRd__5BoundQyd__5IndexRtzlFTq` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation19MutableDataProtocolPAA0cD0Tb` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation19MutableDataProtocolPAAE10resetBytes2inyqd___tSXRd__5BoundQyd__5IndexRtzlF` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation19MutableDataProtocolPSMTb` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation19MutableDataProtocolPSmTb` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation19PredicateExpressionMp` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation19PredicateExpressionP8evaluatey6OutputQzAA0B8BindingsVKFTj` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation19PredicateExpressionTL` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation19PropertyListDecoderCMn` | 1 | sequel-ace |
| Foundation.framework | `_$s10Foundation20ByteCountFormatStyleV0E0O6binaryyA2EmFWC` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation20ByteCountFormatStyleV10AttributedVAA0dE0AAMc` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation20ByteCountFormatStyleV10AttributedVMa` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation20ByteCountFormatStyleV10attributedAC10AttributedVvg` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation20ByteCountFormatStyleV5UnitsV5bytesAEvgZ` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation20ParseableFormatStyleMp` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation20ParseableFormatStyleP13parseStrategy0F0QzvgTq` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation20ParseableFormatStyleP8StrategyAC_AA05ParseE0Tn` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation20ParseableFormatStylePAA0cD0Tb` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO018CollectionContainsD0V4basexvg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO018CollectionContainsD0V5otherq_vg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO018CollectionContainsD0VMn` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO10ArithmeticV2opAC0D8OperatorOvg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO10ArithmeticV3lhsxvg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO10ArithmeticV3rhsq_vg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO10ArithmeticVMn` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO10ComparisonV2opAC0D8OperatorOvg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO10ComparisonV3lhsxvg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO10ComparisonV3rhsq_vg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO10ComparisonVMn` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO10NilLiteralVy_xGAA08StandardB10ExpressionAAMc` | 1 | maccy |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO10NilLiteralVy_xGAA0B10ExpressionAAMc` | 1 | maccy |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO10UnaryMinusV7wrappedxvg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO10UnaryMinusVMn` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO11ConditionalV10trueBranchq_vg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO11ConditionalV11falseBranchq0_vg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO11ConditionalV4testxvg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO11ConditionalVMn` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO11ConjunctionV3lhsxvg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO11ConjunctionV3rhsq_vg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO11ConjunctionVMn` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO11ConjunctionVy_xq_GAA08StandardB10ExpressionA2aGRzAaGR_rlMc` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO11DisjunctionV3lhsxvg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO11DisjunctionV3rhsq_vg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO11DisjunctionVMn` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO11IntDivisionV3lhsxvg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO11IntDivisionV3rhsq_vg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO11IntDivisionVMn` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO11NilCoalesceV3lhsxvg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO11NilCoalesceV3rhsq_vg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO11NilCoalesceVMn` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO12ForcedUnwrapV5innerxvg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO12ForcedUnwrapVMn` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO12IntRemainderV3lhsxvg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO12IntRemainderV3rhsq_vg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO12IntRemainderVMn` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO13FloatDivisionV3lhsxvg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO13FloatDivisionV3rhsq_vg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO13FloatDivisionVMn` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO13build_flatMapyAC012OptionalFlatF0Vy_xq0_q_q1_Gx_q_AC8VariableVy_q0_GXEtAA0B10ExpressionRzAaKR_q0_Sg6OutputRtzq1_SgAMRt_r2_lFZ` | 1 | maccy |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO14build_containsyAC16SequenceContainsVy_xq_Gx_q_tAA0B10ExpressionRzAaHR_ST6OutputRpzSQAIRp_AI_7ElementQZAKRSr0_lFZ` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO15ConditionalCastV5inputxvg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO15ConditionalCastVMn` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO15OptionalFlatMapVMn` | 1 | maccy |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO15OptionalFlatMapVy_xq_q0_q1_GAA08StandardB10ExpressionA2aGRzAaGR0_rlMc` | 1 | maccy |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO15OptionalFlatMapVy_xq_q0_q1_GAA0B10ExpressionAAMc` | 1 | maccy |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO16SequenceContainsV7elementq_vg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO16SequenceContainsV8sequencexvg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO16SequenceContainsVMn` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO16SequenceContainsVy_xq_GAA08StandardB10ExpressionA2aGRzAaGR_rlMc` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO16build_NilLiteralAC0eF0Vy_xGylFZ` | 1 | maccy |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO17build_Conjunction3lhs3rhsAC0E0Vy_xq_Gx_q_tAA0B10ExpressionRzAaJR_Sb6OutputRtzSbAKRt_r0_lFZ` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO18ArithmeticOperatorO3addyA2EmFWC` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO18ArithmeticOperatorO8multiplyyA2EmFWC` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO18ArithmeticOperatorO8subtractyA2EmFWC` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO18ArithmeticOperatorOMa` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO18ComparisonOperatorO11greaterThanyA2EmFWC` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO18ComparisonOperatorO15lessThanOrEqualyA2EmFWC` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO18ComparisonOperatorO18greaterThanOrEqualyA2EmFWC` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO18ComparisonOperatorO8lessThanyA2EmFWC` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO18ComparisonOperatorOMa` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO18SequenceStartsWithV4basexvg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO18SequenceStartsWithV6prefixq_vg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO18SequenceStartsWithVMn` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO23RangeExpressionContainsV5rangexvg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO23RangeExpressionContainsV7elementq_vg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO23RangeExpressionContainsVMn` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO5EqualV3lhsxvg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO5EqualV3rhsq_vg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO5EqualVy_xq_GAA0B10ExpressionAAMc` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO5ValueVMn` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO5ValueVy_xGAA08StandardB10ExpressionAASeRzSERzlMc` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO5ValueVy_xGAA0B10ExpressionAAMc` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO7KeyPathV03keyE0s8Sendable_sADCy6OutputQzq_GXcvg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO7KeyPathV8evaluateyq_AA0B8BindingsVKF` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO8NegationV7wrappedxvg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO8NegationVMn` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO8NotEqualV3lhsxvg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO8NotEqualV3rhsq_vg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO8NotEqualVMn` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO9ForceCastV5inputxvg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO9ForceCastVMn` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation20PredicateExpressionsO9build_ArgyAC5ValueVy_xGxlFZ` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation22AttributeDynamicLookupOyxxmcAA19AttributedStringKeyRzluig` | 1 | cmux |
| Foundation.framework | `_$s10Foundation23LocalizedStringResourceVAA06CustombcD11ConvertibleAAWP` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation23LocalizedStringResourceVSQAAMc` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation24AttributedStringProtocolPAAE17settingAttributesyAA0bC0VAA18AttributeContainerVF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation24AttributedStringProtocolPAAE19replacingAttributes_4withAA0bC0VAA18AttributeContainerV_AItF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation24AttributedStringProtocolPAAE5index_18offsetByCharactersAA0bC0V5IndexVAI_SitF` | 1 | ghostty |
| Foundation.framework | `_$s10Foundation24FloatingPointFormatStyleV13parseStrategyAA0bc5ParseG0VyACyxGGvg` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation24FloatingPointFormatStyleV7PercentV6localeyAEyx_GAA6LocaleVF` | 1 | cmux |
| Foundation.framework | `_$s10Foundation24FloatingPointFormatStyleV7PercentVyx_GAA09ParseabledE0AAMc` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation24FloatingPointFormatStyleV8CurrencyV16decimalSeparator8strategyAEyx_GAA06NumberdE13ConfigurationO07DecimalH15DisplayStrategyV_tF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation24FloatingPointFormatStyleV8CurrencyV6localeyAEyx_GAA6LocaleVF` | 1 | cmux |
| Foundation.framework | `_$s10Foundation24FloatingPointFormatStyleV8CurrencyV9precisionyAEyx_GAA06NumberdE13ConfigurationO9PrecisionVF` | 1 | cmux |
| Foundation.framework | `_$s10Foundation24NSKeyValueObservedChangeVMa` | 1 | raycast |
| Foundation.framework | `_$s10Foundation25NSFastEnumerationIteratorVMn` | 1 | clipy |
| Foundation.framework | `_$s10Foundation25NSFastEnumerationIteratorVyACSo0bC0_pcfC` | 1 | clipy |
| Foundation.framework | `_$s10Foundation26AsyncUnicodeScalarSequenceV04makeB8IteratorAC0bG0Vyx_GyF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation26AsyncUnicodeScalarSequenceVMn` | 1 | raycast |
| Foundation.framework | `_$s10Foundation26DecodableWithConfigurationP4from13configurationxs7Decoder_p_08DecodingD0QztKcfCTj` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation26DecodableWithConfigurationTL` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation26FloatingPointParseStrategyV5parsey11FormatInputQzSSKF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation26FloatingPointParseStrategyVMn` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation26MeasurementFormatUnitUsageV7generalACyxGvgZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation26MeasurementFormatUnitUsageVMn` | 1 | raycast |
| Foundation.framework | `_$s10Foundation27StandardPredicateExpressionMp` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation30DecodingConfigurationProvidingMp` | 1 | cmux |
| Foundation.framework | `_$s10Foundation30DecodingConfigurationProvidingP08decodingC00bC0QzvgZTj` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation30DecodingConfigurationProvidingP08decodingC00bC0QzvgZTq` | 1 | cmux |
| Foundation.framework | `_$s10Foundation30EncodingConfigurationProvidingMp` | 1 | cmux |
| Foundation.framework | `_$s10Foundation30EncodingConfigurationProvidingP08encodingC00bC0QzvgZTq` | 1 | cmux |
| Foundation.framework | `_$s10Foundation30NumberFormatStyleConfigurationO31DecimalSeparatorDisplayStrategyV9automaticAEvgZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation34AttributeScopeCodableConfigurationVMn` | 1 | cmux |
| Foundation.framework | `_$s10Foundation3URLV04baseB0ACSgvg` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation3URLV11FormatStyleV22ComponentDisplayOptionV5neverAGvgZ` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation3URLV11FormatStyleV22ComponentDisplayOptionVMa` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation3URLV11FormatStyleV6schemeyA2E22ComponentDisplayOptionVF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation3URLV11FormatStyleVAadAMc` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation3URLV11FormatStyleVMa` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation3URLV11descriptionSSvg` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation3URLV12relativePathSSvg` | 1 | raycast |
| Foundation.framework | `_$s10Foundation3URLV13DirectoryHintO15checkFileSystemyA2EmFWC` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation3URLV14absoluteStringSSvpMV` | 1 | raycast |
| Foundation.framework | `_$s10Foundation3URLV14relativeStringSSvg` | 1 | raycast |
| Foundation.framework | `_$s10Foundation3URLV18documentsDirectoryACvgZ` | 1 | maccy |
| Foundation.framework | `_$s10Foundation3URLV18temporaryDirectoryACvgZ` | 1 | clipy |
| Foundation.framework | `_$s10Foundation3URLV19appendPathComponent_11isDirectoryySS_SbtF` | 1 | cmux |
| Foundation.framework | `_$s10Foundation3URLV19appendPathExtensionyySSF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation3URLV20applicationDirectoryACvgZ` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation3URLV25removeCachedResourceValue6forKeyySo013NSURLResourceH0a_tF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation3URLV3for2in14appropriateFor6createACSo21NSSearchPathDirectoryV_So0hI10DomainMaskVACSgSbtKcfC` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation3URLV4pathSSvpMV` | 1 | raycast |
| Foundation.framework | `_$s10Foundation3URLV6append9component13directoryHintyx_AC09DirectoryF0OtSyRzlF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation3URLV9appending10components13directoryHintACxd_AC09DirectoryF0OtSyRzlF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation3URLV9appending10queryItemsACSayAA12URLQueryItemVG_tF` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation3URLV9formattedy12FormatOutputQzxAA0D5StyleRzAC0D5InputRtzlF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation4DataV06InlineB0VyAESWcfC` | 1 | claude |
| Foundation.framework | `_$s10Foundation4DataV10LargeSliceV6append10contentsOfySW_tF` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation4DataV11InlineSliceV6append10contentsOfySW_tF` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation4DataV11bytesNoCopy5count11deallocatorACSv_SiAC11DeallocatorOtcfC` | 1 | claude |
| Foundation.framework | `_$s10Foundation4DataV15_RepresentationO5countAESi_tcfC` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation4DataV15_RepresentationO8capacityAESi_tcfC` | 1 | claude |
| Foundation.framework | `_$s10Foundation4DataV15_RepresentationO_5countAeA02__B7StorageC_SitcfC` | 1 | claude |
| Foundation.framework | `_$s10Foundation4DataV16_copyBytesHelper2to4fromySv_SnySiGtF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation4DataV22withUnsafeMutableBytesyxxSwKXEKlF` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation4DataV4fromACs7Decoder_p_tKcfC` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation4DataV6append10contentsOfyx_tSTRzs5UInt8V7ElementRtzlF` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation4DataV6encode2toys7Encoder_p_tKF` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation4DataVSmAAMc` | 1 | orbstack |
| Foundation.framework | `_$s10Foundation4DataVyACxcSTRzs5UInt8V7ElementRtzlufC` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation4DateV11FormatStyleV0bD0V8completeAGvgZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV11FormatStyleV4weekyA2E6SymbolV4WeekVF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV11FormatStyleV6SymbolV12VerbatimHourV0G5CycleV8oneBasedAKvgZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV11FormatStyleV6SymbolV12VerbatimHourV0G5CycleV9zeroBasedAKvgZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV11FormatStyleV6SymbolV12VerbatimHourV0G5CycleVMa` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV11FormatStyleV6SymbolV12VerbatimHourV13defaultDigits5clock9hourCycleA2I5ClockV_AI0gL0VtFZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV11FormatStyleV6SymbolV12VerbatimHourV5ClockV010twentyFourG0AKvgZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV11FormatStyleV6SymbolV12VerbatimHourV5ClockV06twelveG0AKvgZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV11FormatStyleV6SymbolV12VerbatimHourV5ClockVMa` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV11FormatStyleV6SymbolV12VerbatimHourV9twoDigits5clock9hourCycleA2I5ClockV_AI0gL0VtFZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV11FormatStyleV6SymbolV12VerbatimHourVMa` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV11FormatStyleV6SymbolV14SecondFractionV10fractionalyAISiFZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV11FormatStyleV6SymbolV14SecondFractionVMa` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV11FormatStyleV6SymbolV3DayV9twoDigitsAIvgZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV11FormatStyleV6SymbolV4WeekV13defaultDigitsAIvgZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV11FormatStyleV6SymbolV4WeekVMa` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV11FormatStyleV6SymbolV4YearV6paddedyAISiFZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV11FormatStyleV6SymbolV5MonthV4wideAIvgZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV11FormatStyleV6SymbolV5MonthV9twoDigitsAIvgZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV11FormatStyleV6SymbolV6MinuteV9twoDigitsAIvgZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV11FormatStyleV6SymbolV6SecondV9twoDigitsAIvgZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV11FormatStyleV6SymbolV7WeekdayV5shortAIvgZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV11FormatStyleV6SymbolV9DayPeriodV5WidthO4wideyA2KmFWC` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV11FormatStyleV6SymbolV9DayPeriodV5WidthOMa` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV11FormatStyleV6SymbolV9DayPeriodV8standardyA2I5WidthOFZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV11FormatStyleV6SymbolV9DayPeriodVMa` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV11FormatStyleV6formatySSACF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV11FormatStyleVMn` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV12FormatStringV0D13InterpolationV06appendE014secondFractionyAC0C5StyleV6SymbolV06SecondH0V_tF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV12FormatStringV0D13InterpolationV06appendE03dayyAC0C5StyleV6SymbolV3DayV_tF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV12FormatStringV0D13InterpolationV06appendE04houryAC0C5StyleV6SymbolV12VerbatimHourV_tF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV12FormatStringV0D13InterpolationV06appendE04yearyAC0C5StyleV6SymbolV4YearV_tF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV12FormatStringV0D13InterpolationV06appendE05monthyAC0C5StyleV6SymbolV5MonthV_tF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV12FormatStringV0D13InterpolationV06appendE06minuteyAC0C5StyleV6SymbolV6MinuteV_tF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV12FormatStringV0D13InterpolationV06appendE06secondyAC0C5StyleV6SymbolV6SecondV_tF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV12FormatStringV0D13InterpolationV06appendE07weekdayyAC0C5StyleV6SymbolV7WeekdayV_tF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV12FormatStringV0D13InterpolationV06appendE09dayPeriodyAC0C5StyleV6SymbolV03DayH0V_tF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV12FormatStringV0D13InterpolationV13appendLiteralyySSF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV12FormatStringV0D13InterpolationV15literalCapacity18interpolationCountAGSi_SitcfC` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV12FormatStringV0D13InterpolationVMa` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV12FormatStringV19stringInterpolationA2E0dF0V_tcfC` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV12FormatStringVMa` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV12timeInterval5sinceACSd_ACtcfC` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation4DateV13ParseStrategyVAadAMc` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV13ParseStrategyVMa` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV15addTimeIntervalyySdF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV18ISO8601FormatStyleV0B9SeparatorO7omittedyA2GmFWC` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV18ISO8601FormatStyleV13TimeSeparatorO7omittedyA2GmFWC` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV18ISO8601FormatStyleV13dateSeparatoryA2E0bG0OF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV18ISO8601FormatStyleV13timeSeparatoryA2E04TimeG0OF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV18ISO8601FormatStyleV17TimeZoneSeparatorO5colonyA2GmFWC` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation4DateV18ISO8601FormatStyleV17timeZoneSeparatoryA2E04TimegH0OF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV18ISO8601FormatStyleV5parseyACSSKF` | 1 | cmux |
| Foundation.framework | `_$s10Foundation4DateV18ISO8601FormatStyleV8timeZone9separatorA2E04TimeG9SeparatorO_tF` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation4DateV18ISO8601FormatStyleV8timeZoneAA04TimeG0Vvs` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV19RelativeFormatStyleV05UnitsE0V4wideAGvgZ` | 1 | cmux |
| Foundation.framework | `_$s10Foundation4DateV19RelativeFormatStyleV05UnitsE0VMa` | 1 | cmux |
| Foundation.framework | `_$s10Foundation4DateV19RelativeFormatStyleV12PresentationV5namedAGvgZ` | 1 | cmux |
| Foundation.framework | `_$s10Foundation4DateV19RelativeFormatStyleV12PresentationVMa` | 1 | cmux |
| Foundation.framework | `_$s10Foundation4DateV19RelativeFormatStyleVAA0dE0AAMc` | 1 | cmux |
| Foundation.framework | `_$s10Foundation4DateV19RelativeFormatStyleVMa` | 1 | cmux |
| Foundation.framework | `_$s10Foundation4DateV19VerbatimFormatStyleV13parseStrategyAC05ParseG0Vvg` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV19VerbatimFormatStyleV6format6locale8timeZone8calendarAeC0D6StringV_AA6LocaleVSgAA04TimeI0VAA8CalendarVtcfC` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV19VerbatimFormatStyleV8timeZoneAA04TimeG0Vvs` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV19VerbatimFormatStyleVAA0dE0AAMc` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV19VerbatimFormatStyleVMa` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV19VerbatimFormatStyleVMn` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV1poiyA2C_SdtFZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV1soiyA2C_SdtFZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateV4fromACs7Decoder_p_tKcfC` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation4DateV8distance2toSdAC_tF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4DateVs21_ObjectiveCBridgeableAAMc` | 1 | ghostty |
| Foundation.framework | `_$s10Foundation4UUIDV10uuidStringSSvpMV` | 1 | raycast |
| Foundation.framework | `_$s10Foundation4UUIDVSLAAMc` | 1 | cmux |
| Foundation.framework | `_$s10Foundation6LocaleV12LanguageCodeV13stringLiteralAESS_tcfC` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation6LocaleV12LanguageCodeVSQAAMc` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation6LocaleV12languageCode6script0C6RegionA2C08LanguageD0VSg_AC6ScriptVSgAC0F0VSgtcfC` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation6LocaleV14currencySymbolSSSgvg` | 1 | raycast |
| Foundation.framework | `_$s10Foundation6LocaleV15NumberingSystemV10identifierSSvg` | 1 | raycast |
| Foundation.framework | `_$s10Foundation6LocaleV15NumberingSystemVMa` | 1 | raycast |
| Foundation.framework | `_$s10Foundation6LocaleV15localizedString13forRegionCodeSSSgSS_tF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation6LocaleV15localizedString13forScriptCodeSSSgSS_tF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation6LocaleV15numberingSystemAC09NumberingD0Vvg` | 1 | raycast |
| Foundation.framework | `_$s10Foundation6LocaleV16decimalSeparatorSSSgvg` | 1 | raycast |
| Foundation.framework | `_$s10Foundation6LocaleV17MeasurementSystemV2eeoiySbAE_AEtFZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation6LocaleV17MeasurementSystemV6metricAEvgZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation6LocaleV17MeasurementSystemVMa` | 1 | raycast |
| Foundation.framework | `_$s10Foundation6LocaleV17groupingSeparatorSSSgvg` | 1 | raycast |
| Foundation.framework | `_$s10Foundation6LocaleV17measurementSystemAC011MeasurementD0Vvg` | 1 | raycast |
| Foundation.framework | `_$s10Foundation6LocaleV2eeoiySbAC_ACtFZ` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation6LocaleV6RegionV10isoRegionsSayAEGvgZ` | 1 | tailscale-app |
| Foundation.framework | `_$s10Foundation6LocaleV6RegionV7unknownAEvgZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation6LocaleV6ScriptV10identifierSSvg` | 1 | raycast |
| Foundation.framework | `_$s10Foundation6LocaleV6ScriptVSQAAMc` | 1 | raycast |
| Foundation.framework | `_$s10Foundation6LocaleV8CurrencyV10identifierSSvg` | 1 | raycast |
| Foundation.framework | `_$s10Foundation6LocaleV8CurrencyVMa` | 1 | raycast |
| Foundation.framework | `_$s10Foundation6LocaleV8CurrencyVMn` | 1 | raycast |
| Foundation.framework | `_$s10Foundation6LocaleV8LanguageV17maximalIdentifierSSvg` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation6LocaleV8LanguageV18characterDirectionSo08NSLocalecE0Vvg` | 1 | rectangle |
| Foundation.framework | `_$s10Foundation6LocaleV8LanguageV6scriptAC6ScriptVSgvg` | 1 | raycast |
| Foundation.framework | `_$s10Foundation6LocaleV8calendarAA8CalendarVvg` | 1 | raycast |
| Foundation.framework | `_$s10Foundation6LocaleV8currencyAC8CurrencyVSgvg` | 1 | raycast |
| Foundation.framework | `_$s10Foundation6LocaleV9HourCycleO11oneToTwelveyA2EmFWC` | 1 | raycast |
| Foundation.framework | `_$s10Foundation6LocaleV9HourCycleO12zeroToElevenyA2EmFWC` | 1 | raycast |
| Foundation.framework | `_$s10Foundation6LocaleV9HourCycleO15oneToTwentyFouryA2EmFWC` | 1 | raycast |
| Foundation.framework | `_$s10Foundation6LocaleV9HourCycleO17zeroToTwentyThreeyA2EmFWC` | 1 | raycast |
| Foundation.framework | `_$s10Foundation6LocaleV9HourCycleOMa` | 1 | raycast |
| Foundation.framework | `_$s10Foundation6LocaleV9hourCycleAC04HourD0Ovg` | 1 | raycast |
| Foundation.framework | `_$s10Foundation8CalendarV10IdentifierO12islamicCivilyA2EmFWC` | 1 | raycast |
| Foundation.framework | `_$s10Foundation8CalendarV10IdentifierO14islamicTabularyA2EmFWC` | 1 | raycast |
| Foundation.framework | `_$s10Foundation8CalendarV10IdentifierO15republicOfChinayA2EmFWC` | 1 | raycast |
| Foundation.framework | `_$s10Foundation8CalendarV10IdentifierO16islamicUmmAlQurayA2EmFWC` | 1 | raycast |
| Foundation.framework | `_$s10Foundation8CalendarV10IdentifierO17ethiopicAmeteAlemyA2EmFWC` | 1 | raycast |
| Foundation.framework | `_$s10Foundation8CalendarV10IdentifierO19ethiopicAmeteMihretyA2EmFWC` | 1 | raycast |
| Foundation.framework | `_$s10Foundation8CalendarV10IdentifierO6copticyA2EmFWC` | 1 | raycast |
| Foundation.framework | `_$s10Foundation8CalendarV10IdentifierO6hebrewyA2EmFWC` | 1 | raycast |
| Foundation.framework | `_$s10Foundation8CalendarV10IdentifierO6indianyA2EmFWC` | 1 | raycast |
| Foundation.framework | `_$s10Foundation8CalendarV10IdentifierO7chineseyA2EmFWC` | 1 | raycast |
| Foundation.framework | `_$s10Foundation8CalendarV10IdentifierO7islamicyA2EmFWC` | 1 | raycast |
| Foundation.framework | `_$s10Foundation8CalendarV10IdentifierO7persianyA2EmFWC` | 1 | raycast |
| Foundation.framework | `_$s10Foundation8CalendarV10IdentifierO8buddhistyA2EmFWC` | 1 | raycast |
| Foundation.framework | `_$s10Foundation8CalendarV10IdentifierO8japaneseyA2EmFWC` | 1 | raycast |
| Foundation.framework | `_$s10Foundation8CalendarV12firstWeekdaySivs` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation8CalendarV12maximumRange2ofSnySiGSgAC9ComponentO_tF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation8CalendarV13isDateInTodayySbAA0D0VF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation8CalendarV14enumerateDates13startingAfter8matching0G6Policy012repeatedTimeH09direction5usingyAA4DateV_AA0M10ComponentsVAC08MatchingH0OAC08RepeatedjH0OAC15SearchDirectionOyALSg_S2bztXEtF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation8CalendarV15SearchDirectionOMn` | 1 | raycast |
| Foundation.framework | `_$s10Foundation8CalendarV16isDateInTomorrowySbAA0D0VF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation8CalendarV17isDateInYesterdayySbAA0D0VF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation8CalendarV22minimumDaysInFirstWeekSivg` | 1 | clipy |
| Foundation.framework | `_$s10Foundation8CalendarV36_unconditionallyBridgeFromObjectiveCyACSo10NSCalendarCSgFZ` | 1 | clipy |
| Foundation.framework | `_$s10Foundation8CalendarV5range2of2in3forSnySiGSgAC9ComponentO_AkA4DateVtF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation8CalendarV6isDate_11inSameDayAsSbAA0D0V_AGtF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation8CalendarV6isDate_7equalTo13toGranularitySbAA0D0V_AhC9ComponentOtF` | 1 | raycast |
| Foundation.framework | `_$s10Foundation8CalendarV9ComponentO11weekOfMonthyA2EmFWC` | 1 | raycast |
| Foundation.framework | `_$s10Foundation8CalendarV9ComponentO17yearForWeekOfYearyA2EmFWC` | 1 | raycast |
| Foundation.framework | `_$s10Foundation8CalendarV9ComponentO7quarteryA2EmFWC` | 1 | raycast |
| Foundation.framework | `_$s10Foundation8CalendarV9ComponentO8timeZoneyA2EmFWC` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation8IndexSetV10integersInACx_tcSXRzSi5BoundRtzlufC` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8IndexSetV10intersects10integersInSbx_tSXRzSi5BoundRtzlF` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8IndexSetV12intersectionyA2CF` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8IndexSetV18integerGreaterThanySiSgSiF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation8IndexSetV5count2inSiSnySiG_tF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation8IndexSetV5count2inSix_tSXRzSi5BoundRtzlF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation8IndexSetV5shift10startingAt2byySi_SitF` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8IndexSetV5unionyA2CF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation8IndexSetV6remove10integersInySNySiG_tF` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8IndexSetV6remove10integersInySnySiG_tF` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8IndexSetV8contains10integersInSbSnySiG_tF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation8IndexSetV9RangeViewV12makeIterators08IndexingG0VyAEGyF` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8IndexSetV9RangeViewVSKAAMc` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation8IndexSetV9rangeView2ofAC05RangeE0VSnySiG_tF` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8IndexSetV9removeAllyyF` | 1 | cmux |
| Foundation.framework | `_$s10Foundation8IndexSetVs23CustomStringConvertibleAAMc` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8TimeZoneV016isDaylightSavingB03forSbAA4DateV_tF` | 1 | clipy |
| Foundation.framework | `_$s10Foundation8TimeZoneV05knownbC11IdentifiersSaySSGvgZ` | 1 | sequel-ace |
| Foundation.framework | `_$s10Foundation8URLErrorV22notConnectedToInternetAC4CodeVvgZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation8URLErrorV25clientCertificateRejectedAC4CodeVvgZ` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8URLErrorV25clientCertificateRequiredAC4CodeVvgZ` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8URLErrorV26serverCertificateUntrustedAC4CodeVvgZ` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8URLErrorV27serverCertificateHasBadDateAC4CodeVvgZ` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8URLErrorV28serverCertificateNotYetValidAC4CodeVvgZ` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8URLErrorV31serverCertificateHasUnknownRootAC4CodeVvgZ` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV23cannotDecodeContentDataAEvgZ` | 1 | cmux |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV31downloadDecodingFailedMidStreamAEvgZ` | 1 | sequel-ace |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV32backgroundSessionWasDisconnectedAEvgZ` | 1 | sequel-ace |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV32downloadDecodingFailedToCompleteAEvgZ` | 1 | sequel-ace |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV38backgroundSessionInUseByAnotherProcessAEvgZ` | 1 | sequel-ace |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV44appTransportSecurityRequiresSecureConnectionAEvgZ` | 1 | cmux |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV7unknownAEvgZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeVAA06_ErrorC8ProtocolAAMc` | 1 | raycast |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeVSHAAMc` | 1 | sequel-ace |
| Foundation.framework | `_$s10Foundation8URLErrorV9cancelledAC4CodeVvgZ` | 1 | raycast |
| Foundation.framework | `_$s10Foundation8URLErrorVSQAAMc` | 1 | cmux |
| Foundation.framework | `_$s10Foundation9IndexPathV03endB0Sivg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation9IndexPathV05startB0Sivg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation9IndexPathV11descriptionSSvg` | 1 | raycast |
| Foundation.framework | `_$s10Foundation9IndexPathV12arrayLiteralACSid_tcfC` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation9IndexPathV2eeoiySbAC_ACtFZ` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation9IndexPathV5countSivg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation9IndexPathV5indexACSi_tcfC` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation9IndexPathV7compareySo18NSComparisonResultVACF` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation9IndexPathV8dropLastACyF` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation9IndexPathV9appendingyACSiF` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation9IndexPathVSTAAMc` | 1 | raycast |
| Foundation.framework | `_$s10Foundation9IndexPathVs23CustomStringConvertibleAAMc` | 1 | raycast |
| Foundation.framework | `_$s10Foundation9IndexPathVyS2icig` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation9PredicateV10expressionAA08StandardB10Expression_pSb6OutputAA0bE0PRts_XPvg` | 1 | chatgpt |
| Foundation.framework | `_$s10Foundation9PredicateVMa` | 1 | chatgpt |
| Foundation.framework | `_$s10ParseInput10Foundation0A8StrategyPTl` | 1 | coteditor |
| Foundation.framework | `_$s11FormatInput10Foundation0A5StylePTl` | 1 | coteditor |
| Foundation.framework | `_$s11ParseOutput10Foundation0A8StrategyPTl` | 1 | coteditor |
| Foundation.framework | `_$s12FormatOutput10Foundation0A5StylePTl` | 1 | coteditor |
| Foundation.framework | `_$s21DecodingConfiguration10Foundation013DecodableWithB0PTl` | 1 | coteditor |
| Foundation.framework | `_$s21DecodingConfiguration10Foundation0aB9ProvidingPTl` | 1 | cmux |
| Foundation.framework | `_$s21EncodingConfiguration10Foundation0aB9ProvidingPTl` | 1 | cmux |
| Foundation.framework | `_$s5Value10Foundation19AttributedStringKeyPTl` | 1 | cmux |
| Foundation.framework | `_$s6Output10Foundation19PredicateExpressionPTl` | 1 | chatgpt |
| Foundation.framework | `_$s7Regions10Foundation12DataProtocolPTl` | 1 | orbstack |
| Foundation.framework | `_$s7SubjectSo20NSNotificationCenterC10FoundationE16MainActorMessagePTl` | 1 | coteditor |
| Foundation.framework | `_$s8Dispatch0A4DataV10FoundationE6RegionVAD0B8ProtocolADMc` | 1 | iterm2 |
| Foundation.framework | `_$s8Dispatch0A4DataV10FoundationE6RegionVMa` | 1 | iterm2 |
| Foundation.framework | `_$s8Dispatch0A4DataV10FoundationE6RegionVSlADMc` | 1 | iterm2 |
| Foundation.framework | `_$s8Dispatch0A4DataV10FoundationE7regionsSayAcDE6RegionVGvg` | 1 | iterm2 |
| Foundation.framework | `_$s8Strategy10Foundation20ParseableFormatStylePTl` | 1 | coteditor |
| Foundation.framework | `_$sSM10FoundationSkRzrlE4sort5usingyqd___tAA14SortComparatorRd__8ComparedQyd__7ElementSTRtzlF` | 1 | coteditor |
| Foundation.framework | `_$sSM10FoundationSkRzrlE4sort5usingyqd___tSTRd__AA14SortComparatorRd_0_7ElementQyd__Rsd_0_8ComparedQyd_0_AESTRtzr0_lF` | 1 | coteditor |
| Foundation.framework | `_$sSRyxG10Foundation15ContiguousBytesABs5UInt8VRszlMc` | 1 | chatgpt |
| Foundation.framework | `_$sSS10FoundationE10ComparatorVAA04SortB0AAMc` | 1 | coteditor |
| Foundation.framework | `_$sSS10FoundationE10ComparatorVMa` | 1 | coteditor |
| Foundation.framework | `_$sSS10FoundationE11_charactersSSAA16AttributedStringV13CharacterViewV_tcfC` | 1 | coteditor |
| Foundation.framework | `_$sSS10FoundationE11bytesNoCopy6length8encoding12freeWhenDoneSSSgSv_SiSSAAE8EncodingVSbtcfC` | 1 | raycast |
| Foundation.framework | `_$sSS10FoundationE13localizedName2ofS2SAAE8EncodingV_tFZ` | 1 | coteditor |
| Foundation.framework | `_$sSS10FoundationE14contentsOfFile12usedEncodingS2Sh_SSAAE0F0VztKcfC` | 1 | chatgpt |
| Foundation.framework | `_$sSS10FoundationE17LocalizationValueV11PlaceholderO3intyA2EmFWC` | 1 | tailscale-app |
| Foundation.framework | `_$sSS10FoundationE17LocalizationValueV11PlaceholderOMa` | 1 | tailscale-app |
| Foundation.framework | `_$sSS10FoundationE17LocalizationValueV19StringInterpolationV06appendE011placeholder9specifieryAC11PlaceholderO_SStF` | 1 | tailscale-app |
| Foundation.framework | `_$sSS10FoundationE17LocalizationValueV19StringInterpolationV06appendE0_6formatyx_q_t11FormatInputQy_RszAA0H5StyleR_Sy0H6OutputRp_r0_lF` | 1 | coteditor |
| Foundation.framework | `_$sSS10FoundationE17LocalizationValueV19StringInterpolationV06appendE0yyxAA015CustomLocalizedD19ResourceConvertibleRzlF` | 1 | coteditor |
| Foundation.framework | `_$sSS10FoundationE17LocalizationValueV19StringInterpolationV06appendE0yyxSo8NSObjectCRbzlF` | 1 | betterdisplay |
| Foundation.framework | `_$sSS10FoundationE17LocalizationValueVMn` | 1 | cmux |
| Foundation.framework | `_$sSS10FoundationE18StandardComparatorV09localizedB0ACvgZ` | 1 | coteditor |
| Foundation.framework | `_$sSS10FoundationE18StandardComparatorVMa` | 1 | coteditor |
| Foundation.framework | `_$sSS10FoundationE24availableStringEncodingsSaySSAAE8EncodingVGvgZ` | 1 | coteditor |
| Foundation.framework | `_$sSS10FoundationE6format6locale_S2Sh_AA6LocaleVSghs7CVarArg_pdtcfC` | 1 | cmux |
| Foundation.framework | `_$sSS10FoundationE8EncodingV11descriptionSSvg` | 1 | raycast |
| Foundation.framework | `_$sSS10FoundationE8EncodingV8rawValueSuvpMV` | 1 | coteditor |
| Foundation.framework | `_$sSS10FoundationE8EncodingV8rawValueSuvs` | 1 | coteditor |
| Foundation.framework | `_$sSSs21_ObjectiveCBridgeable10FoundationMc` | 1 | chatgpt |
| Foundation.framework | `_$sST10FoundationE6sorted5usingSay7ElementQzGqd___tSTRd__AA14SortComparatorRd_0_ADQyd__Rsd_0_8ComparedQyd_0_AERSr0_lF` | 1 | coteditor |
| Foundation.framework | `_$sSayxGs21_ObjectiveCBridgeable10FoundationMc` | 1 | chatgpt |
| Foundation.framework | `_$sSb10FoundationE7exactlySbSgSo8NSNumberCh_tcfC` | 1 | raycast |
| Foundation.framework | `_$sSci10Foundations5UInt8V7ElementRtzrlE14unicodeScalarsAA26AsyncUnicodeScalarSequenceVyxGvg` | 1 | raycast |
| Foundation.framework | `_$sSd10FoundationE10truncatingSdSo8NSNumberCh_tcfC` | 1 | sequel-ace |
| Foundation.framework | `_$sSh10FoundationE26_forceBridgeFromObjectiveC_6resultySo5NSSetC_ShyxGSgztFZ` | 1 | iterm2 |
| Foundation.framework | `_$sSi10FoundationE10truncatingSiSo8NSNumberCh_tcfC` | 1 | betterdisplay |
| Foundation.framework | `_$sSi10FoundationE7exactlySiSgSo8NSNumberCh_tcfC` | 1 | iterm2 |
| Foundation.framework | `_$sSis21_ObjectiveCBridgeable10FoundationMc` | 1 | chatgpt |
| Foundation.framework | `_$sSn10FoundationAA16AttributedStringV5IndexVRszrlE_2inSnyAEGSgqd___qd_0_tcSXRd__AA0bC8ProtocolRd_0_SSADV5BoundRtd__r0_lufC` | 1 | coteditor |
| Foundation.framework | `_$sSn10FoundationSS5IndexVRszrlE_2inSnyACGSgSo8_NSRangeV_qd__htcSyRd__lufC` | 1 | raycast |
| Foundation.framework | `_$sSo11NSDimensionC10FoundationE9forLocaleAbC0D0V_tcfC` | 1 | raycast |
| Foundation.framework | `_$sSo12NSFileHandleC10FoundationE10AsyncBytesV04makeD8IteratorAE0G0VyF` | 1 | cmux |
| Foundation.framework | `_$sSo16NSOperationQueueC10FoundationE16SchedulerOptionsVMn` | 1 | clipy |
| Foundation.framework | `_$sSo16NSOperationQueueC10FoundationE16minimumToleranceAbCE17SchedulerTimeTypeV6StrideVvg` | 1 | clipy |
| Foundation.framework | `_$sSo16NSOperationQueueC10FoundationE17SchedulerTimeTypeV6StrideVMa` | 1 | clipy |
| Foundation.framework | `_$sSo16NSOperationQueueC10FoundationE17SchedulerTimeTypeV6StrideVs18AdditiveArithmeticACMc` | 1 | clipy |
| Foundation.framework | `_$sSo16NSOperationQueueC10FoundationE17SchedulerTimeTypeVMa` | 1 | clipy |
| Foundation.framework | `_$sSo16NSOperationQueueC10FoundationE17SchedulerTimeTypeVMn` | 1 | clipy |
| Foundation.framework | `_$sSo16NSOperationQueueC10FoundationE17SchedulerTimeTypeVyAeC4DateVcfC` | 1 | clipy |
| Foundation.framework | `_$sSo16NSOperationQueueC10FoundationE3nowAbCE17SchedulerTimeTypeVvg` | 1 | clipy |
| Foundation.framework | `_$sSo16NSOperationQueueC10FoundationE8schedule5after8interval9tolerance7options_7Combine11Cancellable_pAbCE17SchedulerTimeTypeV_AL6StrideVAnbCE0K7OptionsVSgyyctF` | 1 | clipy |
| Foundation.framework | `_$sSo16NSOperationQueueC10FoundationE8schedule5after9tolerance7options_yAbCE17SchedulerTimeTypeV_AI6StrideVAbCE0H7OptionsVSgyyctF` | 1 | clipy |
| Foundation.framework | `_$sSo16NSOperationQueueC10FoundationE8schedule7options_yAbCE16SchedulerOptionsVSg_yyctF` | 1 | clipy |
| Foundation.framework | `_$sSo17NSKeyedUnarchiverC10FoundationE15decodeDecodable_6forKeyxSgxm_SStSeRzlF` | 1 | ghostty |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE11addObserver2of3for5usingAbCE16ObservationTokenV7SubjectQy__xyq_ScMYcctAbCE17MessageIdentifierRzAbCE09MainActorL0R_0L4TypeQzRs_AKRLCr0_lF` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE11addObserver2of3for5usingAbCE16ObservationTokenV7SubjectQzSg_xmyxScMYcctAbCE16MainActorMessageRzAKRLClF` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE14removeObserveryyAbCE16ObservationTokenVF` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE16MainActorMessageMp` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE16MainActorMessageP04makeF0yxSgAC12NotificationVFZTq` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE16MainActorMessageP16makeNotificationyAC0H0VxFZTq` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE16MainActorMessageP4nameSo0A4NameavgZTq` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE16MainActorMessagePACE04makeF0yxSgAC12NotificationVFZ` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE16MainActorMessagePACE16makeNotificationyAC0H0VxFZ` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE16MainActorMessagePACE4nameSo0A4NameavgZ` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE16ObservationTokenVMa` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE16ObservationTokenVMn` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE16ObservationTokenVSHACMc` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE16ObservationTokenVSQACMc` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE21BaseMessageIdentifierVAEy_xGycAbCE09MainActorE0RzlufC` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE21BaseMessageIdentifierVMn` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE21BaseMessageIdentifierVy_xGAbCE0eF0ACMc` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE4post_7subjectyx_7SubjectQztAbCE16MainActorMessageRzAGRLClF` | 1 | coteditor |
| Foundation.framework | `_$sSo5NSSetC10FoundationE12arrayLiteralABypd_tcfC` | 1 | iterm2 |
| Foundation.framework | `_$sSo5NSSetC10FoundationE12makeIteratorAC017NSFastEnumerationD0VyF` | 1 | orbstack |
| Foundation.framework | `_$sSo6CGRectV10FoundationE19_bridgeToObjectiveCSo7NSValueCyF` | 1 | iterm2 |
| Foundation.framework | `_$sSo6NSDataC10FoundationE10startIndexSivg` | 1 | iterm2 |
| Foundation.framework | `_$sSo6NSDataC10FoundationE8endIndexSivg` | 1 | iterm2 |
| Foundation.framework | `_$sSo7CGPointV10FoundationE19_bridgeToObjectiveCSo7NSValueCyF` | 1 | raycast |
| Foundation.framework | `_$sSo7NSArrayC10FoundationE12arrayLiteralABypd_tcfC` | 1 | iterm2 |
| Foundation.framework | `_$sSo7NSArrayCST10FoundationMc` | 1 | raycast |
| Foundation.framework | `_$sSo7NSErrorCs5Error10FoundationMc` | 1 | raycast |
| Foundation.framework | `_$sSo8NSObjectCs28CustomDebugStringConvertible10FoundationMc` | 1 | iterm2 |
| Foundation.framework | `_$sSo8_NSRangeV10FoundationE16debugDescriptionSSvg` | 1 | raycast |
| Foundation.framework | `_$sSo8_NSRangeV10FoundationE19_bridgeToObjectiveCSo7NSValueCyF` | 1 | coteditor |
| Foundation.framework | `_$sSo8_NSRangeV10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo7NSValueC_ABSgztFZ` | 1 | coteditor |
| Foundation.framework | `_$sSo9NSDecimala10FoundationE11FormatStyleV8CurrencyVAcdCMc` | 1 | coteditor |
| Foundation.framework | `_$sSo9NSDecimala10FoundationEyABs6UInt64VcfC` | 1 | raycast |
| Foundation.framework | `_$sSo9NSDecimalaSQ10FoundationMc` | 1 | coteditor |
| Foundation.framework | `_$sSo9NSRunLoopC10FoundationE16minimumToleranceAbCE17SchedulerTimeTypeV6StrideVvg` | 1 | clipy |
| Foundation.framework | `_$sSo9NSRunLoopC10FoundationE17SchedulerTimeTypeV6StrideV14integerLiteralAGSd_tcfC` | 1 | coteditor |
| Foundation.framework | `_$sSo9NSRunLoopC10FoundationE17SchedulerTimeTypeV6StrideVs18AdditiveArithmeticACMc` | 1 | clipy |
| Foundation.framework | `_$sSo9NSRunLoopC10FoundationE17SchedulerTimeTypeVMa` | 1 | clipy |
| Foundation.framework | `_$sSo9NSRunLoopC10FoundationE17SchedulerTimeTypeVMn` | 1 | clipy |
| Foundation.framework | `_$sSo9NSRunLoopC10FoundationE17SchedulerTimeTypeVyAeC4DateVcfC` | 1 | clipy |
| Foundation.framework | `_$sSo9NSRunLoopC10FoundationE3nowAbCE17SchedulerTimeTypeVvg` | 1 | clipy |
| Foundation.framework | `_$sSo9NSRunLoopC10FoundationE8schedule5after8interval9tolerance7options_7Combine11Cancellable_pAbCE17SchedulerTimeTypeV_AL6StrideVAnbCE0K7OptionsVSgyyctF` | 1 | clipy |
| Foundation.framework | `_$sSo9NSRunLoopC10FoundationE8schedule5after9tolerance7options_yAbCE17SchedulerTimeTypeV_AI6StrideVAbCE0H7OptionsVSgyyctF` | 1 | clipy |
| Foundation.framework | `_$sSo9NSRunLoopC10FoundationE8schedule7options_yAbCE16SchedulerOptionsVSg_yyctF` | 1 | clipy |
| Foundation.framework | `_$sSo9NSScannerC10FoundationE10scanUInt6414representations0D0VSgAbCE20NumberRepresentationO_tF` | 1 | raycast |
| Foundation.framework | `_$sSo9NSScannerC10FoundationE14scanCharacters4fromSSSgAC12CharacterSetV_tF` | 1 | raycast |
| Foundation.framework | `_$sSo9NSScannerC10FoundationE20NumberRepresentationO11hexadecimalyA2EmFWC` | 1 | raycast |
| Foundation.framework | `_$sSo9NSScannerC10FoundationE7scanInt14representationSiSgAbCE20NumberRepresentationO_tF` | 1 | raycast |
| Foundation.framework | `_$sSo9NSScannerC10FoundationE9scanFloat14representationSfSgAbCE20NumberRepresentationO_tF` | 1 | raycast |
| Foundation.framework | `_$sSryxG10Foundation15ContiguousBytesABs5UInt8VRszlMc` | 1 | orbstack |
| Foundation.framework | `_$sSs10FoundationE19_bridgeToObjectiveCSo8NSStringCyF` | 1 | coteditor |
| Foundation.framework | `_$sSy10FoundationE14canBeConverted2toSbSSAAE8EncodingV_tF` | 1 | coteditor |
| Foundation.framework | `_$sSy10FoundationE18localizedLowercaseSSvpMV` | 1 | coteditor |
| Foundation.framework | `_$sSy10FoundationE18localizedUppercaseSSvpMV` | 1 | coteditor |
| Foundation.framework | `_$sSy10FoundationE20localizedCapitalizedSSvpMV` | 1 | coteditor |
| Foundation.framework | `_$sSy10FoundationE22localizedStandardRange2ofSnySS5IndexVGSgqd___tSyRd__lF` | 1 | raycast |
| Foundation.framework | `_$sSy10FoundationE37precomposedStringWithCanonicalMappingSSvpMV` | 1 | coteditor |
| Foundation.framework | `_$sSy10FoundationE40decomposedStringWithCompatibilityMappingSSvg` | 1 | coteditor |
| Foundation.framework | `_$sSy10FoundationE41precomposedStringWithCompatibilityMappingSSvg` | 1 | coteditor |
| Foundation.framework | `_$sSy10FoundationE4hashSivg` | 1 | betterdisplay |
| Foundation.framework | `_$sSy10FoundationE4hashSivpMV` | 1 | betterdisplay |
| Foundation.framework | `_$sSy10FoundationE9appendingySSqd__SyRd__lF` | 1 | raycast |
| Foundation.framework | `_$sSy10FoundationE9lineRange3forSnySS5IndexVGqd___tSXRd__AE5BoundRtd__lF` | 1 | coteditor |
| Foundation.framework | `_$ss10ArraySliceV10Foundations5UInt8VRszlE7regionss15CollectionOfOneVyAByAEGGvg` | 1 | orbstack |
| Foundation.framework | `_$ss10ArraySliceVyxG10Foundation12DataProtocolADs5UInt8VRszlMc` | 1 | orbstack |
| Foundation.framework | `_$ss11AnyHashableV10FoundationE36_unconditionallyBridgeFromObjectiveCyABSo8NSObjectCSgFZ` | 1 | iterm2 |
| Foundation.framework | `_$ss5ErrorP10FoundationE20localizedDescriptionSSvpMV` | 1 | raycast |
| Foundation.framework | `_$ss5Int32V10FoundationE10truncatingABSo8NSNumberCh_tcfC` | 1 | rectangle |
| Foundation.framework | `_$ss5Int64V10FoundationE10truncatingABSo8NSNumberCh_tcfC` | 1 | raycast |
| Foundation.framework | `_$ss5NeverO10Foundation14SortComparatorACWP` | 1 | orbstack |
| Foundation.framework | `_$ss5SliceV10FoundationAC15ContiguousBytesRzrlE010withUnsafeD0yqd__qd__SWKXEKlF` | 1 | chatgpt |
| Foundation.framework | `_$ss5SliceVyxG10Foundation15ContiguousBytesA2dERzrlMc` | 1 | chatgpt |
| Foundation.framework | `_$ss5UInt8V10FoundationE10truncatingABSo8NSNumberCh_tcfC` | 1 | iterm2 |
| Foundation.framework | `_$ss6UInt16V10Foundation18_FormatSpecifiableACWP` | 1 | betterdisplay |
| Foundation.framework | `_$ss6UInt32V10Foundation18_FormatSpecifiableACWP` | 1 | betterdisplay |
| Foundation.framework | `_$ss6UInt32V10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF` | 1 | raycast |
| Foundation.framework | `_$ss6UInt32V10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo8NSNumberC_ABSgztFZ` | 1 | betterdisplay |
| Foundation.framework | `_$ss8DurationV10FoundationE15TimeFormatStyleV7PatternV12minuteSecondAGvgZ` | 1 | coteditor |
| Foundation.framework | `_$ss8DurationV10FoundationE15TimeFormatStyleV7PatternV16hourMinuteSecondAGvgZ` | 1 | coteditor |
| Foundation.framework | `_$ss8DurationV10FoundationE16UnitsFormatStyleV29FractionalPartDisplayStrategyV4show6length7rounded9incrementAGSi_s25FloatingPointRoundingRuleOSdSgtFZ` | 1 | coteditor |
| Foundation.framework | `_$ss8DurationV10FoundationE16UnitsFormatStyleV4UnitV5hoursAGvgZ` | 1 | ghostty |
| Foundation.framework | `_$ss8DurationV10FoundationE16UnitsFormatStyleV4UnitV7minutesAGvgZ` | 1 | ghostty |
| Foundation.framework | `_$ss8DurationV10FoundationE16UnitsFormatStyleV9UnitWidthV4wideAGvgZ` | 1 | coteditor |
| Foundation.framework | `_NSAppleScriptErrorBriefMessage` | 1 | iterm2 |
| Foundation.framework | `_NSConnectionDidDieNotification` | 1 | vlc |
| Foundation.framework | `_NSDefaultMallocZone` | 1 | macdown |
| Foundation.framework | `_NSEdgeInsetsEqual` | 1 | zoom |
| Foundation.framework | `_NSFileHandleConnectionAcceptedNotification` | 1 | sublime-text |
| Foundation.framework | `_NSFileHandleNotificationFileHandleItem` | 1 | sublime-text |
| Foundation.framework | `_NSFileHandleReadToEndOfFileCompletionNotification` | 1 | macdown |
| Foundation.framework | `_NSHTTPCookieSameSiteLax` | 1 | cmux |
| Foundation.framework | `_NSHTTPCookieSameSitePolicy` | 1 | cmux |
| Foundation.framework | `_NSHTTPCookieSameSiteStrict` | 1 | cmux |
| Foundation.framework | `_NSIntegralRectWithOptions` | 1 | appcleaner |
| Foundation.framework | `_NSKeyedUnarchiveFromDataTransformerName` | 1 | rectangle |
| Foundation.framework | `_NSMetadataItemAcquisitionMakeKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemAcquisitionModelKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemAlbumKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemAltitudeKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemApertureKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemAppleLoopDescriptorsKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemAppleLoopsKeyFilterTypeKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemAppleLoopsLoopModeKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemAppleLoopsRootKeyKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemApplicationCategoriesKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemAttributeChangeDateKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemAudiencesKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemAudioBitRateKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemAudioChannelCountKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemAudioEncodingApplicationKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemAudioSampleRateKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemAudioTrackNumberKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemAuthorAddressesKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemAuthorEmailAddressesKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemAuthorsKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemBitsPerSampleKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemCFBundleIdentifierKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemCameraOwnerKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemCityKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemCodecsKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemColorSpaceKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemCommentKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemComposerKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemContactKeywordsKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemContentCreationDateKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemContentModificationDateKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemContentTypeKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemContributorsKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemCopyrightKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemCountryKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemCoverageKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemCreatorKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemDateAddedKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemDeliveryTypeKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemDescriptionKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemDirectorKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemDownloadedDateKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemDueDateKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemDurationSecondsKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemEXIFGPSVersionKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemEXIFVersionKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemEditorsKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemEmailAddressesKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemEncodingApplicationsKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemExecutableArchitecturesKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemExecutablePlatformKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemExposureModeKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemExposureProgramKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemExposureTimeSecondsKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemExposureTimeStringKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemFNumberKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemFSCreationDateKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemFinderCommentKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemFlashOnOffKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemFocalLength35mmKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemFocalLengthKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemFontsKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemGPSAreaInformationKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemGPSDOPKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemGPSDateStampKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemGPSDestBearingKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemGPSDestDistanceKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemGPSDestLatitudeKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemGPSDestLongitudeKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemGPSDifferentalKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemGPSMapDatumKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemGPSMeasureModeKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemGPSProcessingMethodKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemGPSStatusKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemGPSTrackKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemGenreKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemHasAlphaChannelKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemHeadlineKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemISOSpeedKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemIdentifierKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemImageDirectionKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemInformationKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemInstantMessageAddressesKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemInstructionsKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemIsApplicationManagedKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemIsGeneralMIDISequenceKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemIsLikelyJunkKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemIsUbiquitousKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemKeySignatureKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemKeywordsKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemKindKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemLanguagesKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemLastUsedDateKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemLatitudeKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemLayerNamesKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemLensModelKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemLongitudeKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemLyricistKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemMaxApertureKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemMediaTypesKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemMeteringModeKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemMusicalGenreKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemMusicalInstrumentCategoryKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemMusicalInstrumentNameKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemNamedLocationKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemNumberOfPagesKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemOrganizationsKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemOrientationKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemOriginalFormatKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemOriginalSourceKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemPageHeightKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemPageWidthKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemParticipantsKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemPerformersKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemPhoneNumbersKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemPixelCountKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemPixelHeightKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemPixelWidthKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemProducerKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemProfileNameKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemProjectsKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemPublishersKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemRecipientAddressesKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemRecipientEmailAddressesKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemRecipientsKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemRecordingDateKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemRecordingYearKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemRedEyeOnOffKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemResolutionHeightDPIKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemResolutionWidthDPIKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemRightsKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemSecurityMethodKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemSpeedKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemStarRatingKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemStateOrProvinceKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemStreamableKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemSubjectKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemTempoKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemTextContentKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemThemeKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemTimeSignatureKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemTimestampKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemTitleKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemTotalBitRateKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemVersionKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemVideoBitRateKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemWhereFromsKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataItemWhiteBalanceKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataQueryAccessibleUbiquitousExternalDocumentsScope` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataQueryDidStartGatheringNotification` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataQueryGatheringProgressNotification` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataQueryIndexedLocalComputerScope` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataQueryIndexedNetworkScope` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataQueryUbiquitousDocumentsScope` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataQueryUserHomeScope` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataUbiquitousItemContainerDisplayNameKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataUbiquitousItemDownloadRequestedKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataUbiquitousItemDownloadingStatusDownloaded` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataUbiquitousItemDownloadingStatusNotDownloaded` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataUbiquitousItemIsExternalDocumentKey` | 1 | hammerspoon |
| Foundation.framework | `_NSMetadataUbiquitousItemURLInLocalContainerKey` | 1 | hammerspoon |
| Foundation.framework | `_NSNetServicesErrorCode` | 1 | hammerspoon |
| Foundation.framework | `_NSPortDidBecomeInvalidNotification` | 1 | zoom |
| Foundation.framework | `_NSSecureUnarchiveFromDataTransformerName` | 1 | rectangle |
| Foundation.framework | `_NSStreamNetworkServiceType` | 1 | hammerspoon |
| Foundation.framework | `_NSStreamNetworkServiceTypeBackground` | 1 | hammerspoon |
| Foundation.framework | `_NSStreamNetworkServiceTypeCallSignaling` | 1 | hammerspoon |
| Foundation.framework | `_NSStreamNetworkServiceTypeVideo` | 1 | hammerspoon |
| Foundation.framework | `_NSStreamNetworkServiceTypeVoIP` | 1 | hammerspoon |
| Foundation.framework | `_NSStreamNetworkServiceTypeVoice` | 1 | hammerspoon |
| Foundation.framework | `_NSStringEncodingDetectionLikelyLanguageKey` | 1 | transmission |
| Foundation.framework | `_NSStringEncodingDetectionSuggestedEncodingsKey` | 1 | cmux |
| Foundation.framework | `_NSStringEncodingDetectionUseOnlySuggestedEncodingsKey` | 1 | cmux |
| Foundation.framework | `_NSTaskDidTerminateNotification` | 1 | zoom |
| Foundation.framework | `_NSThreadWillExitNotification` | 1 | sequel-ace |
| Foundation.framework | `_NSURLAuthenticationMethodHTMLForm` | 1 | hammerspoon |
| Foundation.framework | `_NSURLProtectionSpaceFTPProxy` | 1 | hammerspoon |
| Foundation.framework | `_NSURLProtectionSpaceHTTPProxy` | 1 | hammerspoon |
| Foundation.framework | `_NSURLProtectionSpaceHTTPSProxy` | 1 | hammerspoon |
| Foundation.framework | `_NSURLProtectionSpaceSOCKSProxy` | 1 | hammerspoon |
| Foundation.framework | `_NSURLSessionTaskPriorityDefault` | 1 | spotify |
| Foundation.framework | `_NSURLSessionTaskPriorityLow` | 1 | spotify |
| Foundation.framework | `_OBJC_CLASS_$_NSClassDescription` | 1 | iterm2 |
| Foundation.framework | `_OBJC_CLASS_$_NSCloneCommand` | 1 | utm |
| Foundation.framework | `_OBJC_CLASS_$_NSCreateCommand` | 1 | utm |
| Foundation.framework | `_OBJC_CLASS_$_NSDeleteCommand` | 1 | utm |
| Foundation.framework | `_OBJC_CLASS_$_NSFileAccessIntent` | 1 | raycast |
| Foundation.framework | `_OBJC_CLASS_$_NSIndexSpecifier` | 1 | iterm2 |
| Foundation.framework | `_OBJC_CLASS_$_NSLengthFormatter` | 1 | raycast |
| Foundation.framework | `_OBJC_CLASS_$_NSMassFormatter` | 1 | raycast |
| Foundation.framework | `_OBJC_CLASS_$_NSPortMessage` | 1 | obs |
| Foundation.framework | `_OBJC_CLASS_$_NSScriptExecutionContext` | 1 | sequel-ace |
| Foundation.framework | `_OBJC_CLASS_$_NSScriptSuiteRegistry` | 1 | iterm2 |
| Foundation.framework | `_OBJC_CLASS_$_NSUnitMass` | 1 | raycast |
| Foundation.framework | `_OBJC_CLASS_$_NSUnitPressure` | 1 | raycast |
| Foundation.framework | `_OBJC_CLASS_$_NSUnitSpeed` | 1 | raycast |
| Foundation.framework | `_OBJC_IVAR_$_NSRegularExpression._options` | 1 | clipy |
| Foundation.framework | `_OBJC_IVAR_$_NSRegularExpression._pattern` | 1 | clipy |
| Foundation.framework | `_OBJC_METACLASS_$_NSCloneCommand` | 1 | utm |
| Foundation.framework | `_OBJC_METACLASS_$_NSCreateCommand` | 1 | utm |
| Foundation.framework | `_OBJC_METACLASS_$_NSDeleteCommand` | 1 | utm |
| libswiftFoundation.dylib | `_$s10Foundation10CocoaErrorV13userCancelledAC4CodeVvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation10CocoaErrorV19fileWriteOutOfSpaceAC4CodeVvgZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10CocoaErrorV4CodeV13userCancelledAEvgZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10CocoaErrorV4CodeV14fileNoSuchFileAEvgZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10CocoaErrorV4CodeV16fileWriteUnknownAEvgZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10CocoaErrorV4CodeV25ubiquitousFileUnavailableAEvgZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10CocoaErrorV4CodeV34fileReadInapplicableStringEncodingAEvgZ` | 1 | obsidian |
| libswiftFoundation.dylib | `_$s10Foundation10CocoaErrorV4CodeV35ubiquitousFileNotUploadedDueToQuotaAEvgZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10CocoaErrorV4CodeV40ubiquitousFileUbiquityServerNotAvailableAEvgZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10CocoaErrorV4CodeV8rawValueSivg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10CocoaErrorV4CodeVSQAAMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10CocoaErrorV5error_8userInfo3urls0C0_pAC4CodeV_SDys11AnyHashableVypGSgAA3URLVSgtFZ` | 1 | krita |
| libswiftFoundation.dylib | `_$s10Foundation10POSIXErrorV9ECANCELED6Darwin0B4CodeOvgZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10URLRequestV15mainDocumentURLAA0E0VSgvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10URLRequestV15timeoutIntervalSdvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10URLRequestV19allHTTPHeaderFieldsSDyS2SGSgvM` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10URLRequestV19allHTTPHeaderFieldsSDyS2SGSgvs` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10URLRequestV19assumesHTTP3CapableSbvs` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10URLRequestV23httpShouldHandleCookiesSbvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10URLRequestV23httpShouldHandleCookiesSbvs` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10URLRequestV2eeoiySbAC_ACtFZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation10URLRequestV3urlAA3URLVSgvs` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11FormatStyleMp` | 1 | utm |
| libswiftFoundation.dylib | `_$s10Foundation11FormatStyleP6formaty0B6OutputQz0B5InputQzFTq` | 1 | utm |
| libswiftFoundation.dylib | `_$s10Foundation11FormatStyleP6localeyxAA6LocaleVFTq` | 1 | utm |
| libswiftFoundation.dylib | `_$s10Foundation11FormatStylePA2A4DateV07ISO8601bC0VRszrlE7iso8601AGvgZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11FormatStylePAAE6localeyxAA6LocaleVF` | 1 | utm |
| libswiftFoundation.dylib | `_$s10Foundation11FormatStylePSETb` | 1 | utm |
| libswiftFoundation.dylib | `_$s10Foundation11FormatStylePSHTb` | 1 | utm |
| libswiftFoundation.dylib | `_$s10Foundation11FormatStylePSeTb` | 1 | utm |
| libswiftFoundation.dylib | `_$s10Foundation11JSONDecoderC20DataDecodingStrategyO6customyAeA0C0Vs7Decoder_pKccAEmFWC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONDecoderC20DataDecodingStrategyOMa` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONDecoderC20DateDecodingStrategyO010deferredToC0yA2EmFWC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONDecoderC20DateDecodingStrategyO16secondsSince1970yA2EmFWC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONDecoderC20DateDecodingStrategyO21millisecondsSince1970yA2EmFWC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONDecoderC20DateDecodingStrategyO6customyAeA0C0Vs7Decoder_pKccAEmFWC` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation11JSONDecoderC20DateDecodingStrategyO7iso8601yA2EmFWC` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation11JSONDecoderC20dataDecodingStrategyAC04DatadE0OvsTj` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONDecoderC34NonConformingFloatDecodingStrategyO17convertFromStringyAESS_S2StcAEmFWC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONDecoderC34NonConformingFloatDecodingStrategyOMa` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONDecoderC34nonConformingFloatDecodingStrategyAC03NondefG0OvsTj` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONDecoderC7Combine15TopLevelDecoderAAMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONDecoderCACycfCTq` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONDecoderCMm` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONDecoderCfd` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC16OutputFormattingVSQAAMc` | 1 | hex-fiend |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC16OutputFormattingVSYAAMc` | 1 | utm |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC16OutputFormattingVs9OptionSetAAMc` | 1 | utm |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC19KeyEncodingStrategyO14useDefaultKeysyA2EmFWC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC19KeyEncodingStrategyO18convertToSnakeCaseyA2EmFWC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC19KeyEncodingStrategyOMa` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC19KeyEncodingStrategyOMn` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC19keyEncodingStrategyAC03KeydE0OvsTj` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC20DataEncodingStrategyOMa` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC20DataEncodingStrategyOMn` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC20DateEncodingStrategyO010deferredToC0yA2EmFWC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC20DateEncodingStrategyO16secondsSince1970yA2EmFWC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC20DateEncodingStrategyO21millisecondsSince1970yA2EmFWC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC20DateEncodingStrategyO6customyAEyAA0C0V_s7Encoder_ptKccAEmFWC` | 1 | 1password |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC20DateEncodingStrategyO7iso8601yA2EmFWC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC20DateEncodingStrategyOMn` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC20dataEncodingStrategyAC04DatadE0OvsTj` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC34NonConformingFloatEncodingStrategyOMn` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC8userInfoSDys010CodingUserD3KeyVypGvMTj` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC8userInfoSDys010CodingUserD3KeyVypGvsTj` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderCACycfCTj` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderCACycfCTq` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderCMm` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderCfd` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation11MeasurementV19_bridgeToObjectiveCSo13NSMeasurementCyF` | 1 | alt-tab |
| libswiftFoundation.dylib | `_$s10Foundation11MeasurementVAASo11NSDimensionCRbzrlE7convert2toyx_tF` | 1 | stats |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV10isSuperset2ofSbAC_tF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV11subtractingyA2CF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV12charactersInACSNys7UnicodeO6ScalarVG_tcfC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV14urlUserAllowedACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV16lowercaseLettersACvgZ` | 1 | alt-tab |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV16uppercaseLettersACvgZ` | 1 | alt-tab |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV18urlFragmentAllowedACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV18urlPasswordAllowedACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV6insert12charactersInySNys7UnicodeO6ScalarVG_tF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV6insertySb8inserted_s7UnicodeO6ScalarV17memberAfterInserttAIF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV6invertyyF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV7lettersACvgZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetVs0C7AlgebraAAMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12DataProtocolPA2A15ContiguousBytesRzrlE04copyE02to4fromySryqd__G_qd_0_tSXRd_0_5BoundQyd_0_5IndexSlRtzr0_lF` | 1 | utm |
| libswiftFoundation.dylib | `_$s10Foundation12DataProtocolPAAE9copyBytes2to4fromSiSw_qd__tSXRd__5BoundQyd__5IndexRtzlF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12DateIntervalV11descriptionSSvg` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation12DateIntervalV3endAA0B0Vvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12DateIntervalV5start8durationAcA0B0V_SdtcfC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12DateIntervalV8containsySbAA0B0VF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12DateIntervalV8durationSdvg` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation12DateIntervalVACycfC` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation12DateIntervalVSQAAMc` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation12URLQueryItemV19_bridgeToObjectiveCSo010NSURLQueryC0CyF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12URLQueryItemV2eeoiySbAC_ACtFZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12URLQueryItemV36_unconditionallyBridgeFromObjectiveCyACSo010NSURLQueryC0CSgFZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation12URLQueryItemVSQAAMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13CustomNSErrorP13errorUserInfoSDySSypGvgTj` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13CustomNSErrorP9errorCodeSivgTj` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13CustomNSErrorPAAE11errorDomainSSvgZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13CustomNSErrorPAAE13errorUserInfoSDySSypGvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13CustomNSErrorPAAE9errorCodeSivg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13CustomNSErrorPAASYRzs17FixedWidthInteger8RawValueSYRpzrlE9errorCodeSivg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13ParseStrategyMp` | 1 | utm |
| libswiftFoundation.dylib | `_$s10Foundation13ParseStrategyP5parsey0B6OutputQz0B5InputQzKFTq` | 1 | utm |
| libswiftFoundation.dylib | `_$s10Foundation13ParseStrategyPSETb` | 1 | utm |
| libswiftFoundation.dylib | `_$s10Foundation13ParseStrategyPSHTb` | 1 | utm |
| libswiftFoundation.dylib | `_$s10Foundation13ParseStrategyPSeTb` | 1 | utm |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV10queryItemsSayAA12URLQueryItemVGSgvM` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV12rangeOfQuerySnySS5IndexVGSgvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV18percentEncodedHostSSSgvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV18percentEncodedPathSSvM` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV18percentEncodedPathSSvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV18percentEncodedPathSSvs` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV18percentEncodedUserSSSgvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV19_bridgeToObjectiveCSo15NSURLComponentsCyF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV19percentEncodedQuerySSSgvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV22percentEncodedFragmentSSSgvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV22percentEncodedFragmentSSSgvs` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV22percentEncodedPasswordSSSgvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV24percentEncodedQueryItemsSayAA12URLQueryItemVGSgvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV24percentEncodedQueryItemsSayAA12URLQueryItemVGSgvs` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV36_unconditionallyBridgeFromObjectiveCyACSo15NSURLComponentsCSgFZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV4pathSSvM` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV4portSiSgvs` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV4userSSSgvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV4userSSSgvs` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV5querySSSgvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV5querySSSgvs` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV8fragmentSSSgvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV8fragmentSSSgvs` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV8passwordSSSgvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsV8passwordSSSgvs` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13URLComponentsVSQAAMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation13__DataStorageC12_deallocatorySv_SitcSgvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsV10weekOfYearSiSgvg` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsV11weekOfMonthSiSgvg` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsV14weekdayOrdinalSiSgvs` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsV4dateAA0B0VSgvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsV4yearSiSgvM` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsV5monthSiSgvM` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsV6secondSiSgvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsV7quarterSiSgvg` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsV7quarterSiSgvs` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsV7weekdaySiSgvs` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsV8calendarAA8CalendarVSgvs` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsV8timeZoneAA04TimeE0VSgvs` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation14DateComponentsVSQAAMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation14LocalizedErrorP13failureReasonSSSgvgTj` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation14LocalizedErrorP18recoverySuggestionSSSgvgTj` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation14LocalizedErrorPAAE16errorDescriptionSSSgvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation15AffineTransformV3m113m123m213m222tX2tYAC12CoreGraphics7CGFloatV_A5LtcfC` | 1 | alt-tab |
| libswiftFoundation.dylib | `_$s10Foundation15AffineTransformV6rotate9byRadiansy12CoreGraphics7CGFloatV_tF` | 1 | localsend |
| libswiftFoundation.dylib | `_$s10Foundation15AffineTransformV8scaleByX3byYAC12CoreGraphics7CGFloatV_AHtcfC` | 1 | zoom |
| libswiftFoundation.dylib | `_$s10Foundation15AffineTransformV9translate1x1yy12CoreGraphics7CGFloatV_AItF` | 1 | localsend |
| libswiftFoundation.dylib | `_$s10Foundation15AffineTransformVACycfC` | 1 | localsend |
| libswiftFoundation.dylib | `_$s10Foundation15AttributeScopesO0A10AttributesV04LinkB0OAA19AttributedStringKeyAAMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation15AttributeScopesO0A10AttributesV04LinkB0OMn` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation15AttributeScopesO0A10AttributesV04LinkB0ON` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation15AttributeScopesO0A10AttributesV4linkAE04LinkB0Ovg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation15AttributeScopesO0A10AttributesV4linkAE04LinkB0OvpMV` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation15AttributeScopesO0A10AttributesVMn` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation16AttributedStringV10charactersAC13CharacterViewVvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation16AttributedStringV13CharacterViewVMa` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation16AttributedStringV13CharacterViewVMn` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation16AttributedStringV13CharacterViewVSlAAMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation16AttributedStringV13dynamicMember5ValueQzSgs7KeyPathCyAA22AttributeDynamicLookupOxG_tcAA0bcG0Rzluis` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation16AttributedStringV13stringLiteralACSS_tcfC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation16AttributedStringV1poiyA2C_ACtFZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation16AttributedStringV22MarkdownParsingOptionsV17InterpretedSyntaxO4fullyA2GmFWC` | 1 | utm |
| libswiftFoundation.dylib | `_$s10Foundation16AttributedStringV2peoiyyACz_ACtFZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation16AttributedStringV4RunsV3RunV13dynamicMember5ValueQzSgs7KeyPathCyAA22AttributeDynamicLookupOxG_tcAA0bcI0Rzluig` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation16AttributedStringV4RunsV3RunV5rangeSnyAC5IndexVGvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation16AttributedStringV4RunsV3RunVMa` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation16AttributedStringV4RunsV5IndexVMa` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation16AttributedStringV4RunsV5IndexVSQAAMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation16AttributedStringV4RunsVMa` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation16AttributedStringV4RunsVMn` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation16AttributedStringV4RunsVSlAAMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation16AttributedStringV4runsAC4RunsVvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation16AttributedStringV5IndexVMa` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation16AttributedStringV5IndexVMn` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation16AttributedStringV5IndexVSLAAMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation16AttributedStringV5IndexVSQAAMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation16AttributedStringV6appendyyxAA0bC8ProtocolRzlF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation16AttributedStringV8markdown7options7baseURLACSS_AC22MarkdownParsingOptionsVAA0G0VSgtKcfC` | 1 | utm |
| libswiftFoundation.dylib | `_$s10Foundation16AttributedStringVAA0bC8ProtocolAAMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation16AttributedStringVACycfC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation16AttributedStringVMn` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation16AttributedStringV_10attributesACSS_AA18AttributeContainerVtcfC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation16AttributedStringV_10attributesACSs_AA18AttributeContainerVtcfC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation16AttributedStringVyAA0B9SubstringVxcSXRzAC5IndexV5BoundRtzluig` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation16AttributedStringVyACSo012NSAttributedC0CcfC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation17KeyPathComparatorVyxGAA04SortD0AAMc` | 1 | utm |
| libswiftFoundation.dylib | `_$s10Foundation17URLResourceValuesV03allC0SDySo16NSURLResourceKeyaypGvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation17URLResourceValuesV12creationDateAA0E0VSgvg` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation17URLResourceValuesV13totalFileSizeSiSgvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation17URLResourceValuesV13volumeIsLocalSbSgvg` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation17URLResourceValuesV14typeIdentifierSSSgvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation17URLResourceValuesV17fileAllocatedSizeSiSgvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation17URLResourceValuesV19volumeTotalCapacitySiSgvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation17URLResourceValuesV20isExcludedFromBackupSbSgvs` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation17URLResourceValuesV23volumeAvailableCapacitySiSgvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation17URLResourceValuesV4pathSSSgvg` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation17URLResourceValuesVACycfC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation18AttributeContainerV13dynamicMember5ValueQzSgs7KeyPathCyAA0B13DynamicLookupOxG_tcAA016AttributedStringG0Rzluis` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation18AttributeContainerVACycfC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation18AttributeContainerVMa` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation18IntegerFormatStyleV6localeACyxGAA6LocaleV_tcfC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation18IntegerFormatStyleVMn` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation18IntegerFormatStyleVyxGAA0cD0AAMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation19AttributedSubstringV10charactersAA0B6StringV13CharacterViewVvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation19AttributedSubstringVMa` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation19PropertyListDecoderC8userInfoSDys010CodingUserF3KeyVypGvsTj` | 1 | utm |
| libswiftFoundation.dylib | `_$s10Foundation20ParseableFormatStyleMp` | 1 | utm |
| libswiftFoundation.dylib | `_$s10Foundation20ParseableFormatStyleP13parseStrategy0F0QzvgTq` | 1 | utm |
| libswiftFoundation.dylib | `_$s10Foundation20ParseableFormatStyleP8StrategyAC_AA05ParseE0Tn` | 1 | utm |
| libswiftFoundation.dylib | `_$s10Foundation20ParseableFormatStylePAA0cD0Tb` | 1 | utm |
| libswiftFoundation.dylib | `_$s10Foundation20ReferenceConvertibleTL` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation21_BridgedStoredNSErrorP8_nsErrorSo0D0CvgTj` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation22AttributeDynamicLookupO13dynamicMemberxs7KeyPathCyAA0B6ScopesO0A10AttributesVxG_tcAA016AttributedStringG0Rzluig` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation22AttributeDynamicLookupO13dynamicMemberxs7KeyPathCyAA0B6ScopesO0A10AttributesVxG_tcAA016AttributedStringG0RzluipMV` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation22AttributeDynamicLookupOMn` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation22kCFStringEncodingASCIIs6UInt32Vvg` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation25NSFastEnumerationIteratorVMn` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation25NSFastEnumerationIteratorVyACSo0bC0_pcfC` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation3URLV14relativeStringSSvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation3URLV14resourceValues7forKeys16fromBookmarkDataAA011URLResourceD0VSgShySo16NSURLResourceKeyaG_AA0I0VtFZ` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation3URLV17setResourceValuesyyAA011URLResourceE0VKF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation3URLV18dataRepresentationAA4DataVvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation3URLV20resolvingAliasFileAt7optionsA2Ch_So30NSURLBookmarkResolutionOptionsVtKcfC` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation3URLV21resolveSymlinksInPathyyF` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation3URLV25removeCachedResourceValue6forKeyySo013NSURLResourceH0a_tF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation3URLV4userSSSgvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation3URLV5querySSSgvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation3URLV8fragmentSSSgvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation3URLV8passwordSSSgvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation3URLVAA20ReferenceConvertibleAAMc` | 1 | utm |
| libswiftFoundation.dylib | `_$s10Foundation4DataV10LargeSliceV6append10contentsOfySW_tF` | 1 | utm |
| libswiftFoundation.dylib | `_$s10Foundation4DataV11InlineSliceV21ensureUniqueReferenceyyF` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation4DataV11InlineSliceV6append10contentsOfySW_tF` | 1 | utm |
| libswiftFoundation.dylib | `_$s10Foundation4DataV11bytesNoCopy5count11deallocatorACSv_SiAC11DeallocatorOtcfC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataV11descriptionSSvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataV12makeIteratorAC0D0VyF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataV15_RepresentationO10resetBytes2inySnySiG_tF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataV15_RepresentationO8capacityAESi_tcfC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataV15_RepresentationO_5countAeA02__B7StorageC_SitcfC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataV5countACSi_tcfC` | 1 | claude |
| libswiftFoundation.dylib | `_$s10Foundation4DataV5countSivpMV` | 1 | utm |
| libswiftFoundation.dylib | `_$s10Foundation4DataV6append10contentsOfyx_tSTRzs5UInt8V7ElementRtzlF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataV6append_5countySPys5UInt8VG_SitF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataV6appendyySRyxGlF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataV6bufferACSRyxG_tclufC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataV8IteratorV4nexts5UInt8VSgyF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataV8advanced2byACSi_tF` | 1 | stats |
| libswiftFoundation.dylib | `_$s10Foundation4DataV8endIndexSivg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataV9copyBytes2to5countySpys5UInt8VG_SitF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataV9hashValueSivg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataV9repeating5countACs5UInt8V_SitcfC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataVSHAAMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataVSKAAMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataVSQAAMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataVSlAAMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataVSmAAMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataVs21_ObjectiveCBridgeableAAMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataVs23CustomStringConvertibleAAMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataVyACxcSXRzs17FixedWidthInteger5BoundRpzluig` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DataVyACxcSXRzs17FixedWidthInteger5BoundRpzluis` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateV11FormatStyleV0bD0V2eeoiySbAG_AGtFZ` | 1 | utm |
| libswiftFoundation.dylib | `_$s10Foundation4DateV11descriptionSSvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateV15addTimeIntervalyySdF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateV18ISO8601FormatStyleV0B13TimeSeparatorO8standardyA2GmFWC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateV18ISO8601FormatStyleV0B13TimeSeparatorOMa` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateV18ISO8601FormatStyleV0B9SeparatorO4dashyA2GmFWC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateV18ISO8601FormatStyleV0B9SeparatorOMa` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateV18ISO8601FormatStyleV13TimeSeparatorO5colonyA2GmFWC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateV18ISO8601FormatStyleV13TimeSeparatorOMa` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateV18ISO8601FormatStyleV13dateSeparator0f4TimeG004timeG00i4ZoneG026includingFractionalSeconds0iJ0A2E0bG0O_AE0bhG0OAE0hG0OAE0hjG0OSbAA0hJ0VtcfC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateV18ISO8601FormatStyleV13dateSeparatoryA2E0bG0OF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateV18ISO8601FormatStyleV17TimeZoneSeparatorO7omittedyA2GmFWC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateV18ISO8601FormatStyleV17TimeZoneSeparatorOMa` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateV18ISO8601FormatStyleV3dayAEyF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateV18ISO8601FormatStyleV4yearAEyF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateV18ISO8601FormatStyleV5monthAEyF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateV18ISO8601FormatStyleV5parseyACSSKF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateV18ISO8601FormatStyleV6formatySSACF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateV18ISO8601FormatStyleVAA0dE0AAMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateV18ISO8601FormatStyleVMa` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateV1soiyA2C_SdtFZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateV8advanced2byACSd_tF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4DateVs21_ObjectiveCBridgeableAAMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation4UUIDV4hash4intoys6HasherVz_tF` | 1 | utm |
| libswiftFoundation.dylib | `_$s10Foundation6LocaleV10components14fromIdentifierSDyS2SGSS_tFZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation6LocaleV10identifier14fromComponentsSSSDyS2SG_tFZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation6LocaleV10identifierSSvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation6LocaleV10identifierSSvpMV` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation6LocaleV14currencySymbolSSSgvg` | 1 | localsend |
| libswiftFoundation.dylib | `_$s10Foundation6LocaleV15localizedString13forRegionCodeSSSgSS_tF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation6LocaleV15localizedString15forLanguageCodeSSSgSS_tF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation6LocaleV16debugDescriptionSSvg` | 1 | alt-tab |
| libswiftFoundation.dylib | `_$s10Foundation6LocaleV16decimalSeparatorSSSgvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation6LocaleV16isoCurrencyCodesSaySSGvgZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation6LocaleV16usesMetricSystemSbvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation6LocaleV17groupingSeparatorSSSgvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation6LocaleV19autoupdatingCurrentACvgZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation6LocaleVSHAAMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV10IdentifierO2eeoiySbAE_AEtFZ` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV10IdentifierO7iso8601yA2EmFWC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV10IdentifierOSHAAMc` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV12dateInterval2of5start8interval3forSbAC9ComponentO_AA4DateVzSdzALtF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV12firstWeekdaySivs` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV12monthSymbolsSaySSGvg` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV13isDateInTodayySbAA0D0VF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV14MatchingPolicyO35nextTimePreservingSmallerComponentsyA2EmFWC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV14dateComponents2in4fromAA04DateD0VAA8TimeZoneV_AA0G0VtF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV14dateComponents_4from2toAA04DateD0VShyAC9ComponentOG_A2HtF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV14weekdaySymbolsSaySSGvg` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV15SearchDirectionO2eeoiySbAE_AEtFZ` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV15SearchDirectionO8backwardyA2EmFWC` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV15isDateInWeekendySbAA0D0VF` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV16isDateInTomorrowySbAA0D0VF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV17isDateInYesterdayySbAA0D0VF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV17shortMonthSymbolsSaySSGvg` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV19autoupdatingCurrentACvgZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV19shortQuarterSymbolsSaySSGvg` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV19shortWeekdaySymbolsSaySSGvg` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV22standaloneMonthSymbolsSaySSGvg` | 1 | stats |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV24standaloneWeekdaySymbolsSaySSGvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV29shortStandaloneWeekdaySymbolsSaySSGvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV2eeoiySbAC_ACtFZ` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV33veryShortStandaloneWeekdaySymbolsSaySSGvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV6localeAA6LocaleVSgvs` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV7compare_2to0D11GranularitySo18NSComparisonResultVAA4DateV_AjC9ComponentOtF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV8amSymbolSSvg` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV8pmSymbolSSvg` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV9ComponentO10nanosecondyA2EmFWC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV9ComponentO2eeoiySbAE_AEtFZ` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV9ComponentO3erayA2EmFWC` | 1 | stats |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV9ComponentO8calendaryA2EmFWC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV9ComponentO8timeZoneyA2EmFWC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV05startB0AC0B0Vvg` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV08filteredbC014includeIntegerACSbSiKXE_tKF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV10intersects10integersInSbx_tSXRzSi5BoundRtzlF` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV5firstSiSgvg` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV5shift10startingAt2byySi_SitF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV5unionyA2CF` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV6remove10integersInySnySiG_tF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV8contains10integersInSbSnySiG_tF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV9RangeViewV12makeIterators08IndexingG0VyAEGyF` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV9RangeViewVMn` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV9RangeViewVSTAAMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV9hashValueSivg` | 1 | utm |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetVs21_ObjectiveCBridgeableAAMc` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetVs23CustomStringConvertibleAAMc` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetVySiAC0B0Vcig` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation8TimeZoneV05knownbC11IdentifiersSaySSGvgZ` | 1 | stats |
| libswiftFoundation.dylib | `_$s10Foundation8TimeZoneV12abbreviation3forSSSgAA4DateV_tF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8TimeZoneV12abbreviationACSgSSh_tcfC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8TimeZoneV19autoupdatingCurrentACvgZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8TimeZoneVSEAAMc` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation8TimeZoneVSeAAMc` | 1 | raycast |
| libswiftFoundation.dylib | `_$s10Foundation8URLErrorV14cannotFindHostAC4CodeVvgZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8URLErrorV15dnsLookupFailedAC4CodeVvgZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8URLErrorV19cannotConnectToHostAC4CodeVvgZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8URLErrorV21networkConnectionLostAC4CodeVvgZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8URLErrorV22notConnectedToInternetAC4CodeVvgZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8URLErrorV4CodeV14cannotFindHostAEvgZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8URLErrorV4CodeV15dnsLookupFailedAEvgZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8URLErrorV4CodeV16fileDoesNotExistAEvgZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8URLErrorV4CodeV19cannotConnectToHostAEvgZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8URLErrorV4CodeV22notConnectedToInternetAEvgZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8URLErrorV4CodeV22secureConnectionFailedAEvgZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8URLErrorV4CodeV24dataLengthExceedsMaximumAEvgZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8URLErrorV4CodeV26userAuthenticationRequiredAEvgZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8URLErrorV4CodeV8rawValueSivg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8URLErrorV4CodeV9cancelledAEvgZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8URLErrorV4CodeVMn` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8URLErrorV4CodeVSQAAMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8URLErrorV8timedOutAC4CodeVvgZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation8URLErrorV9cancelledAC4CodeVvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation9IndexPathV05startB0Sivg` | 1 | zoom |
| libswiftFoundation.dylib | `_$s10Foundation9IndexPathV1loiySbAC_ACtFZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation9IndexPathV2eeoiySbAC_ACtFZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation9IndexPathV2leoiySbAC_ACtFZ` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation9IndexPathV5countSivg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation9IndexPathV7indexesACSaySiG_tcfC` | 1 | zoom |
| libswiftFoundation.dylib | `_$s10Foundation9IndexPathVACycfC` | 1 | zoom |
| libswiftFoundation.dylib | `_$s10Foundation9IndexPathVSKAAMc` | 1 | zoom |
| libswiftFoundation.dylib | `_$s10Foundation9IndexPathVSLAAMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10Foundation9IndexPathVSlAAMc` | 1 | zoom |
| libswiftFoundation.dylib | `_$s10Foundation9IndexPathVs23CustomStringConvertibleAAMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s10ParseInput10Foundation0A8StrategyPTl` | 1 | utm |
| libswiftFoundation.dylib | `_$s11FormatInput10Foundation0A5StylePTl` | 1 | utm |
| libswiftFoundation.dylib | `_$s11ParseOutput10Foundation0A8StrategyPTl` | 1 | utm |
| libswiftFoundation.dylib | `_$s12CoreGraphics7CGFloatVs21_ObjectiveCBridgeable10FoundationMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$s12FormatOutput10Foundation0A5StylePTl` | 1 | utm |
| libswiftFoundation.dylib | `_$s13ReferenceType10Foundation0A11ConvertiblePTl` | 1 | iina |
| libswiftFoundation.dylib | `_$s8Strategy10Foundation20ParseableFormatStylePTl` | 1 | utm |
| libswiftFoundation.dylib | `_$sSDyxq_Gs21_ObjectiveCBridgeable10FoundationMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSM10FoundationSkRzrlE4sort5usingyqd___tAA14SortComparatorRd__8ComparedQyd__7ElementSTRtzlF` | 1 | utm |
| libswiftFoundation.dylib | `_$sSM10FoundationSkRzrlE4sort5usingyqd___tSTRd__AA14SortComparatorRd_0_7ElementQyd__Rsd_0_8ComparedQyd_0_AESTRtzr0_lF` | 1 | utm |
| libswiftFoundation.dylib | `_$sSS10FoundationE10contentsOf12usedEncodingSSAA3URLVh_SSAAE0E0VztKcfC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSS10FoundationE17LocalizationValueV13stringLiteralACSS_tcfC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSS10FoundationE17LocalizationValueVMa` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSS10FoundationE18StandardComparatorV09localizedB0ACvgZ` | 1 | utm |
| libswiftFoundation.dylib | `_$sSS10FoundationE18StandardComparatorVMa` | 1 | utm |
| libswiftFoundation.dylib | `_$sSS10FoundationE6format6locale9argumentsS2Sh_AA6LocaleVSghSays7CVarArg_pGhtcfC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSS10FoundationE6format6locale_S2Sh_AA6LocaleVSghs7CVarArg_pdtcfC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV10macOSRomanACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV11japaneseEUCACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV13windowsCP1250ACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV13windowsCP1251ACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV13windowsCP1252ACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV13windowsCP1253ACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV13windowsCP1254ACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV14utf16BigEndianACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV17utf16LittleEndianACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV17utf32LittleEndianACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV5utf16ACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV5utf32ACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV6symbolACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV8nextstepACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV8shiftJISACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV9iso2022JPACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV9isoLatin2ACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationEySSs5SliceVyAA16AttributedStringV13CharacterViewVGcfC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSSs21_ObjectiveCBridgeable10FoundationMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSayxG10Foundation12DataProtocolABs5UInt8VRszlMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSayxGs21_ObjectiveCBridgeable10FoundationMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSb10FoundationE10truncatingSbSo8NSNumberCh_tcfC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSds21_ObjectiveCBridgeable10FoundationMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSf10FoundationE26_forceBridgeFromObjectiveC_6resultySo8NSNumberC_SfSgztFZ` | 1 | alt-tab |
| libswiftFoundation.dylib | `_$sSf10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo8NSNumberC_SfSgztFZ` | 1 | alt-tab |
| libswiftFoundation.dylib | `_$sSf10FoundationE36_unconditionallyBridgeFromObjectiveCySfSo8NSNumberCSgFZ` | 1 | alt-tab |
| libswiftFoundation.dylib | `_$sSfs21_ObjectiveCBridgeable10FoundationMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSh10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo5NSSetC_ShyxGSgztFZ` | 1 | linearmouse |
| libswiftFoundation.dylib | `_$sShyxGs21_ObjectiveCBridgeable10FoundationMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSi10FoundationE7exactlySiSgSo8NSNumberCh_tcfC` | 1 | zoom |
| libswiftFoundation.dylib | `_$sSis21_ObjectiveCBridgeable10FoundationMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSo10NSIndexSetCST10FoundationMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSo12NSDictionaryC10FoundationE8IteratorCMa` | 1 | zoom |
| libswiftFoundation.dylib | `_$sSo12NSDictionaryC10FoundationE8IteratorCStACMc` | 1 | zoom |
| libswiftFoundation.dylib | `_$sSo12NSDictionaryCST10FoundationMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSo12NSFileHandleC10FoundationE6offsets6UInt64VyKF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSo13NSUndoManagerC10FoundationE12registerUndo10withTarget7handleryx_yxctRlzClF` | 1 | iina |
| libswiftFoundation.dylib | `_$sSo14NSItemProviderC10FoundationE10loadObject7ofClass17completionHandlerSo10NSProgressCxm_yxSg_s5Error_pSgtcts21_ObjectiveCBridgeableRzSo0aB7Reading01_L5CTypeRpzlF` | 1 | utm |
| libswiftFoundation.dylib | `_$sSo5NSSetC10FoundationE12arrayLiteralABypd_tcfC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSo5NSSetC10FoundationE12makeIteratorAC017NSFastEnumerationD0VyF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSo5NSSetC10FoundationE3setA2Bh_tcfC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSo5NSSetCST10FoundationMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSo6CGRectV10FoundationE19_bridgeToObjectiveCSo7NSValueCyF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSo6CGSizeV10FoundationE19_bridgeToObjectiveCSo7NSValueCyF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSo7CGPointV10FoundationE19_bridgeToObjectiveCSo7NSValueCyF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSo7CGPointVs21_ObjectiveCBridgeable10FoundationMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSo7NSArrayC10FoundationE5arrayA2Bh_tcfC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSo7NSArrayCST10FoundationMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSo7NSErrorC10FoundationE5_codeSivg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSo7NSErrorC10FoundationE7_domainSSvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSo7NSErrorCs5Error10FoundationMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSo8NSObjectC10FoundationE26KeyValueObservingPublisherVMa` | 1 | linearmouse |
| libswiftFoundation.dylib | `_$sSo8NSStringC10FoundationE6stringA2Bh_tcfC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSo8_NSRangeV10FoundationE12intersectionyABSgABF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSo8_NSRangeV10FoundationEyABxcSXRzs17FixedWidthInteger5BoundRpzlufC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSo8_NSRangeVSH10FoundationMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSo8_NSRangeVSQ10FoundationMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSo8_NSRangeVs21_ObjectiveCBridgeable10FoundationMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSo8_NSRangeVs23CustomStringConvertible10FoundationMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSo9NSDecimala10FoundationE10isInfiniteSbvg` | 1 | raycast |
| libswiftFoundation.dylib | `_$sSo9NSDecimala10FoundationE11descriptionSSvg` | 1 | linearmouse |
| libswiftFoundation.dylib | `_$sSo9NSDecimala10FoundationE11isSignMinusSbvg` | 1 | raycast |
| libswiftFoundation.dylib | `_$sSo9NSDecimala10FoundationE21leastNonzeroMagnitudeABvgZ` | 1 | raycast |
| libswiftFoundation.dylib | `_$sSo9NSDecimala10FoundationE2deoiyyABz_ABtFZ` | 1 | raycast |
| libswiftFoundation.dylib | `_$sSo9NSDecimala10FoundationE2meoiyyABz_ABtFZ` | 1 | raycast |
| libswiftFoundation.dylib | `_$sSo9NSDecimala10FoundationE2peoiyyABz_ABtFZ` | 1 | raycast |
| libswiftFoundation.dylib | `_$sSo9NSDecimala10FoundationE3nanABvgZ` | 1 | raycast |
| libswiftFoundation.dylib | `_$sSo9NSDecimala10FoundationE4sign8exponent11significandABs17FloatingPointSignO_SiABtcfC` | 1 | raycast |
| libswiftFoundation.dylib | `_$sSo9NSDecimala10FoundationE8isNormalSbvg` | 1 | raycast |
| libswiftFoundation.dylib | `_$sSo9NSDecimala10FoundationE9magnitudeABvg` | 1 | raycast |
| libswiftFoundation.dylib | `_$sSo9NSDecimalaSL10FoundationMc` | 1 | linearmouse |
| libswiftFoundation.dylib | `_$sSo9NSDecimalaSQ10FoundationMc` | 1 | linearmouse |
| libswiftFoundation.dylib | `_$sSo9NSRunLoopC10FoundationE17SchedulerTimeTypeV6StrideV7secondsyAGSiFZ` | 1 | linearmouse |
| libswiftFoundation.dylib | `_$sSo9NSScannerC10FoundationE10scanDouble14representationSdSgAbCE20NumberRepresentationO_tF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSo9NSScannerC10FoundationE12currentIndexSS0D0Vvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSo9NSScannerC10FoundationE12currentIndexSS0D0Vvs` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSo9NSScannerC10FoundationE14scanCharacters4fromSSSgAC12CharacterSetV_tF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSo9NSScannerC10FoundationE18scanUpToCharacters4fromSSSgAC12CharacterSetV_tF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSu10FoundationE10truncatingSuSo8NSNumberCh_tcfC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSus21_ObjectiveCBridgeable10FoundationMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSy10FoundationE10lowercased4withSSAA6LocaleVSg_tF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSy10FoundationE10uppercased4withSSAA6LocaleVSg_tF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSy10FoundationE11capitalized4withSSAA6LocaleVSg_tF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSy10FoundationE13lengthOfBytes5usingSiSSAAE8EncodingV_tF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSy10FoundationE15appendingFormatySSqd___s7CVarArg_pdtSyRd__lF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSy10FoundationE18localizedLowercaseSSvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSy10FoundationE18localizedUppercaseSSvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSy10FoundationE19enumerateSubstrings2in7options_yqd___So26NSStringEnumerationOptionsVySSSg_SnySS5IndexVGAJSbztctSXRd__AI5BoundRtd__lF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSy10FoundationE19replacingCharacters2in4withSSqd_0__qd__tSyRd__SXRd_0_SS5IndexV5BoundRtd_0_r0_lF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSy10FoundationE20localizedCapitalizedSSvpMV` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSy10FoundationE32rangeOfComposedCharacterSequence2atSnySS5IndexVGAE_tF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSy10FoundationE33rangeOfComposedCharacterSequences3forSnySS5IndexVGqd___tSXRd__AE5BoundRtd__lF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSy10FoundationE36decomposedStringWithCanonicalMappingSSvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSy10FoundationE40decomposedStringWithCompatibilityMappingSSvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSy10FoundationE41precomposedStringWithCompatibilityMappingSSvg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSy10FoundationE4hashSivg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSy10FoundationE9appendingySSqd__SyRd__lF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$sSz10FoundationE9formattedy12FormatOutputQyd__qd__0C5InputQyd__RszAA0C5StyleRd__lF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$ss11AnyHashableV10FoundationE19_bridgeToObjectiveCSo8NSObjectCyF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$ss11AnyHashableV10FoundationE36_unconditionallyBridgeFromObjectiveCyABSo8NSObjectCSgFZ` | 1 | utm |
| libswiftFoundation.dylib | `_$ss5ErrorP10FoundationAC13CustomNSErrorRzSYRzs17FixedWidthInteger8RawValueSYRpzrlE5_codeSivg` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$ss5Int16V10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$ss5Int32Vs21_ObjectiveCBridgeable10FoundationMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$ss5Int64V10FoundationE10truncatingABSo8NSNumberCh_tcfC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$ss6UInt32V10FoundationE10truncatingABSo8NSNumberCh_tcfC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$ss6UInt32V10FoundationE19_bridgeToObjectiveCSo8NSNumberCyF` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$ss6UInt32Vs21_ObjectiveCBridgeable10FoundationMc` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$ss6UInt64V10FoundationE10truncatingABSo8NSNumberCh_tcfC` | 1 | whatsapp |
| libswiftFoundation.dylib | `_$ss6UInt64Vs21_ObjectiveCBridgeable10FoundationMc` | 1 | whatsapp |
| self) | `_CFBundleGetBundleWithIdentifier` | 1 | libreoffice |
