# Lacunas de Fernanda (Swift runtime, Dispatch)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Fernanda (Swift runtime, Dispatch) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**451** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| libswiftos.dylib | `_$sSo13os_log_type_ta0A0E5errorABvgZ` | 24 | 1password, chatgpt, claude, cmux |
| libswiftos.dylib | `_$s2os0A4_log_3dso0B0__ySo0a1_B7_type_ta_SVSo03OS_a1_B0Cs12StaticStringVs7CVarArg_pdtF` | 22 | 1password, betterdisplay, chatgpt, claude |
| libswiftos.dylib | `_$sSo13os_log_type_ta0A0E5faultABvgZ` | 22 | 1password, betterdisplay, chatgpt, claude |
| libswiftos.dylib | `_$s2os6LoggerV9logObjectSo03OS_a1_C0Cvg` | 20 | 1password, bitwarden, chatgpt, clipy |
| libswiftos.dylib | `_$s2os6LoggerV9subsystem8categoryACSS_SStcfC` | 20 | 1password, bitwarden, chatgpt, clipy |
| libswiftos.dylib | `_$s2os6LoggerVMa` | 20 | 1password, bitwarden, chatgpt, clipy |
| libswiftos.dylib | `_$sSo13os_log_type_ta0A0E4infoABvgZ` | 20 | 1password, betterdisplay, chatgpt, claude |
| libswiftos.dylib | `_$sSo13os_log_type_ta0A0E5debugABvgZ` | 18 | betterdisplay, claude, clipy, cmux |
| libswiftos.dylib | `_$sSo13os_log_type_ta0A0E7defaultABvgZ` | 13 | 1password, betterdisplay, chatgpt, claude |
| libobjc.A.dylib | `_class_getIvarLayout` | 11 | alt-tab, cyberduck, flycut, hex-fiend |
| libobjc.A.dylib | `_objc_initializeClassPair` | 11 | alt-tab, cyberduck, flycut, hex-fiend |
| libswiftos.dylib | `_$s2os6LoggerVMn` | 11 | bitwarden, chatgpt, clipy, cmux |
| libswiftos.dylib | `_$sSo9OS_os_logC0B0E7defaultABvgZ` | 10 | 1password, betterdisplay, iterm2, linearmouse |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV06_regexA07versionACyxGSS_SitcfC` | 9 | chatgpt, claude, cmux, coteditor |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexVMn` | 9 | chatgpt, claude, cmux, coteditor |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexVyxGAA0C9ComponentAAMc` | 9 | chatgpt, claude, cmux, coteditor |
| libswiftos.dylib | `_$s2os0A4_log_3dso0B04type_ys12StaticStringV_SVSgSo03OS_a1_B0CSo0a1_b1_D2_tas7CVarArg_pdtF` | 9 | 1password, betterdisplay, claude, iterm2 |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV5MatchVMn` | 8 | chatgpt, claude, cmux, coteditor |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing14RegexComponentP5regexAA0C0Vy0C6OutputQzGvgTj` | 7 | chatgpt, claude, cmux, coteditor |
| libswiftos.dylib | `_$s2os21OSAllocatedUnfairLockVMn` | 7 | chatgpt, claude, cmux, iterm2 |
| libSystem.B.dylib | `_dispatch_queue_set_width` | 6 | iina, karabiner-elements, linearmouse, localsend |
| libswiftObservation.dylib | `_$s11Observation0A9RegistrarV12withMutation2of7keyPath_q0_x_s03KeyG0Cyxq_Gq0_yKXEtKAA10ObservableRzr1_lF` | 6 | chatgpt, cmux, coteditor, maccy |
| libswiftObservation.dylib | `_$s11Observation0A9RegistrarV6access_7keyPathyx_s03KeyE0Cyxq_GtAA10ObservableRzr0_lF` | 6 | chatgpt, cmux, coteditor, maccy |
| libswiftObservation.dylib | `_$s11Observation0A9RegistrarV6didSet_7keyPathyx_s03KeyF0Cyxq_GtAA10ObservableRzr0_lF` | 6 | chatgpt, cmux, coteditor, maccy |
| libswiftObservation.dylib | `_$s11Observation0A9RegistrarV7willSet_7keyPathyx_s03KeyF0Cyxq_GtAA10ObservableRzr0_lF` | 6 | chatgpt, cmux, coteditor, maccy |
| libswiftObservation.dylib | `_$s11Observation0A9RegistrarVACycfC` | 6 | chatgpt, cmux, coteditor, maccy |
| libswiftObservation.dylib | `_$s11Observation0A9RegistrarVMa` | 6 | chatgpt, cmux, coteditor, maccy |
| libswiftObservation.dylib | `_$s11Observation0A9RegistrarVMn` | 6 | chatgpt, cmux, coteditor, maccy |
| libswiftObservation.dylib | `_$s11Observation10ObservableMp` | 6 | chatgpt, cmux, coteditor, maccy |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV10firstMatch2inAC0E0Vyx_GSgSs_tKF` | 6 | chatgpt, claude, cmux, coteditor |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV5MatchV13dynamicMemberqd__s7KeyPathCyxqd__G_tcluig` | 6 | claude, cmux, coteditor, openclaw |
| libswiftSystem.dylib | `_$s6System8FilePathVMa` | 5 | chatgpt, ghostty, openclaw, orbstack |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV5MatchV6outputxvg` | 5 | chatgpt, coteditor, openclaw, orbstack |
| libswiftos.dylib | `_$s2os12OSSignpostIDV3logACSo03OS_a1_D0C_tcfC` | 5 | chatgpt, cmux, raycast, utm |
| libswiftos.dylib | `_$s2os12OSSignpostIDVMa` | 5 | chatgpt, cmux, raycast, utm |
| libswiftos.dylib | `_$sSo18os_signpost_type_ta0A0E3endABvgZ` | 5 | chatgpt, cmux, raycast, utm |
| libswiftos.dylib | `_$sSo18os_signpost_type_ta0A0E5beginABvgZ` | 5 | chatgpt, cmux, raycast, utm |
| libswiftDarwin.dylib | `_$s6Darwin7environSpySpys4Int8VGSgGvg` | 4 | betterdisplay, cmux, openclaw, vorssaint |
| libswiftDispatch.dylib | `_$s8Dispatch0A12DataIteratorVMa` | 4 | chatgpt, claude, cmux, raycast |
| libswiftDispatch.dylib | `_$s8Dispatch0A12DataIteratorVMn` | 4 | chatgpt, claude, cmux, raycast |
| libswiftDispatch.dylib | `_$s8Dispatch0A12DataIteratorVStAAMc` | 4 | chatgpt, claude, cmux, raycast |
| libswiftObservation.dylib | `_$s11Observation04withA8Tracking_8onChangexxyXE_yyYbcyXKtlF` | 4 | chatgpt, cmux, maccy, openclaw |
| libswiftRegexBuilder.dylib | `_$sSy12RegexBuilderSs11SubSequenceRtzrlE5split9separator9maxSplits25omittingEmptySubsequencesSaySsGSS_SiSbtF` | 4 | chatgpt, openclaw, raycast, tailscale-app |
| libswiftSystem.dylib | `_$s6System8FilePathVyACSScfC` | 4 | chatgpt, ghostty, openclaw, whatsapp |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV10wholeMatch2inAC0E0Vyx_GSgSs_tKF` | 4 | chatgpt, cmux, coteditor, openclaw |
| libswift_StringProcessing.dylib | `_$sSK17_StringProcessingSs11SubSequenceRtzrlE6starts4withSbqd___tAA14RegexComponentRd__lF` | 4 | chatgpt, coteditor, openclaw, raycast |
| libswift_StringProcessing.dylib | `_$sSK17_StringProcessingSs11SubSequenceRtzrlE7matches2ofSayAA5RegexV5MatchVyqd___GGqd_0__t0G6OutputQyd_0_Rsd__AA0G9ComponentRd_0_r0_lF` | 4 | cmux, coteditor, openclaw, raycast |
| libswift_StringProcessing.dylib | `_$sSK17_StringProcessingSs11SubSequenceRtzrlE8containsySbqd__AA14RegexComponentRd__lF` | 4 | coteditor, ghostty, openclaw, raycast |
| libswiftos.dylib | `_$s2os12OSSignpostIDV8rawValues6UInt64Vvg` | 4 | chatgpt, cmux, raycast, whatsapp |
| libswiftos.dylib | `_$s2os12OSSignposterV9logHandleSo03OS_a1_C0Cvg` | 4 | chatgpt, cmux, raycast, whatsapp |
| libswiftos.dylib | `_$s2os12OSSignposterVMa` | 4 | chatgpt, cmux, raycast, whatsapp |
| libswiftos.dylib | `_$s2os12OSSignposterVMn` | 4 | chatgpt, cmux, raycast, whatsapp |
| libswiftos.dylib | `_$s2os15OSSignpostErrorO9doubleEndyA2CmFWC` | 4 | chatgpt, cmux, raycast, whatsapp |
| libswiftos.dylib | `_$s2os15OSSignpostErrorOMa` | 4 | chatgpt, cmux, raycast, whatsapp |
| libswiftos.dylib | `_$s2os23OSSignpostIntervalStateC10signpostIDAA0bF0Vvg` | 4 | chatgpt, cmux, raycast, whatsapp |
| libswiftos.dylib | `_$s2os23OSSignpostIntervalStateC2id6isOpenAcA0B2IDV_Sbtcfc` | 4 | chatgpt, cmux, raycast, whatsapp |
| libswiftos.dylib | `_$s2os23OSSignpostIntervalStateCMa` | 4 | chatgpt, cmux, raycast, whatsapp |
| libswiftos.dylib | `_$s2os28checkForErrorAndConsumeState5stateAA010OSSignpostD0OAA0i8IntervalG0C_tF` | 4 | chatgpt, cmux, raycast, whatsapp |
| libobjc.A.dylib | `_object_getInstanceVariable` | 3 | cmux, ghostty, orbstack |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder0a9ComponentB0O15buildExpressionyxx17_StringProcessing0aC0RzlFZ` | 3 | chatgpt, openclaw, raycast |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder0a9ComponentB0O17buildPartialBlock5first17_StringProcessing0A0Vy0A6OutputQzGx_tAF0aC0RzlFZ` | 3 | chatgpt, openclaw, raycast |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder10OptionallyVMn` | 3 | chatgpt, openclaw, raycast |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder10OptionallyVyACyxG17_StringProcessing0A0VyxGcfC` | 3 | chatgpt, openclaw, raycast |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder10OptionallyVyxG17_StringProcessing0A9ComponentAAMc` | 3 | chatgpt, openclaw, raycast |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder10ZeroOrMoreVMn` | 3 | chatgpt, openclaw, raycast |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder10ZeroOrMoreVyACyxG17_StringProcessing0A0VyxGcfC` | 3 | chatgpt, openclaw, raycast |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder10ZeroOrMoreVyxG17_StringProcessing0A9ComponentAAMc` | 3 | chatgpt, openclaw, raycast |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder11makeFactory17_StringProcessing01_aD0VyF` | 3 | chatgpt, openclaw, raycast |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder14CharacterClassV17_StringProcessing0A9ComponentAAMc` | 3 | chatgpt, openclaw, raycast |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder14CharacterClassVMa` | 3 | chatgpt, openclaw, raycast |
| libswiftRegexBuilder.dylib | `_$s17_StringProcessing5RegexV0C7BuilderEyACyxGqd__yXEc0C6OutputQyd__RszAA0C9ComponentRd__lufC` | 3 | chatgpt, openclaw, raycast |
| libswiftRegexBuilder.dylib | `_$sSS17_StringProcessing14RegexComponent0C7BuilderMc` | 3 | chatgpt, openclaw, raycast |
| libswiftSystem.dylib | `_$s6System14FileDescriptorV6_closes6ResultOyytAA5ErrnoVGyF` | 3 | chatgpt, openclaw, orbstack |
| libswiftSystem.dylib | `_$s6System14FileDescriptorVMn` | 3 | chatgpt, openclaw, orbstack |
| libswiftSystem.dylib | `_$s6System5ErrnoVN` | 3 | chatgpt, openclaw, orbstack |
| libswiftSystem.dylib | `_$s6System5ErrnoVs5ErrorAAMc` | 3 | chatgpt, openclaw, orbstack |
| libswiftSystem.dylib | `_$s6System8FilePathV6stringSSvg` | 3 | chatgpt, ghostty, openclaw |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing13_RegexFactoryV10accumulateyAA0C0VyxGq__q0_tAA0C9ComponentR_AaHR0_r1_lF` | 3 | chatgpt, openclaw, raycast |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing13_RegexFactoryV10zeroOrMoreyAA0C0VyxGq__AA0C18RepetitionBehaviorVSgtAA0C9ComponentR_r0_lF` | 3 | chatgpt, openclaw, raycast |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing13_RegexFactoryV9zeroOrOneyAA0C0VyxGq__AA0C18RepetitionBehaviorVSgtAA0C9ComponentR_r0_lF` | 3 | chatgpt, openclaw, raycast |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing13_RegexFactoryVMa` | 3 | chatgpt, openclaw, raycast |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing14AnyRegexOutputVMn` | 3 | coteditor, openclaw, orbstack |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing23RegexRepetitionBehaviorVMa` | 3 | chatgpt, openclaw, raycast |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing23RegexRepetitionBehaviorVMn` | 3 | chatgpt, openclaw, raycast |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV11ignoresCaseyACyxGSbF` | 3 | coteditor, openclaw, raycast |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV5MatchV5rangeSnySS5IndexVGvg` | 3 | coteditor, openclaw, raycast |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexVA2A03AnyC6OutputVRszlEyACyAEGSSKcfC` | 3 | coteditor, openclaw, orbstack |
| libswift_StringProcessing.dylib | `_$sSm17_StringProcessingSQ7ElementRpzrlE9replacing_4with15maxReplacementsxqd___qd_0_SitSlRd__SlRd_0_ABQyd__ACRSABQyd_0_AGRSr0_lF` | 3 | chatgpt, clipy, coteditor |
| libswift_StringProcessing.dylib | `_$sSm17_StringProcessingSs11SubSequenceRtzrlE9replacing_15maxReplacements4withxqd_1__Siqd_0_AA5RegexV5MatchVyqd___GKXEtK0I6OutputQyd_1_Rsd__SlRd_0_AA0I9ComponentRd_1_SJ7ElementRtd_0_r1_lF` | 3 | coteditor, openclaw, orbstack |
| libswift_StringProcessing.dylib | `_$sSm17_StringProcessingSs11SubSequenceRtzrlE9replacing_4with15maxReplacementsxqd_0__qd__SitSlRd__AA14RegexComponentRd_0_SJ7ElementRtd__r0_lF` | 3 | chatgpt, coteditor, openclaw |
| libswiftos.dylib | `_$s2os0A9_signpost_3dso3log4name0B2ID__ySo0a1_B7_type_ta_SVSo03OS_a1_D0Cs12StaticStringVAA010OSSignpostF0VALs7CVarArg_pdtF` | 3 | raycast, utm, whatsapp |
| libswiftos.dylib | `_$s2os12OSSignpostIDV9exclusiveACvgZ` | 3 | chatgpt, raycast, whatsapp |
| libswiftos.dylib | `_$s2os23OSSignpostIntervalStateCMn` | 3 | cmux, raycast, whatsapp |
| libswiftos.dylib | `_$s2os6LoggerVyACSo03OS_A4_logCcfC` | 3 | chatgpt, clipy, maccy |
| libswiftos.dylib | `_$sSo18os_signpost_type_ta0A0E5eventABvgZ` | 3 | cmux, raycast, whatsapp |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA0Mp` | 2 | cmux, qlmarkdown |
| libobjc.A.dylib | `_protocol_addMethodDescription` | 2 | bitwarden, orbstack |
| libobjc.A.dylib | `_protocol_addProtocol` | 2 | bitwarden, orbstack |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder14CharacterClassV8invertedACvg` | 2 | chatgpt, openclaw |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder7CaptureVMn` | 2 | chatgpt, openclaw |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder7CaptureVyACyxG17_StringProcessing0A0VyxGcfC` | 2 | chatgpt, openclaw |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder7CaptureVyxG17_StringProcessing0A9ComponentAAMc` | 2 | chatgpt, openclaw |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder9OneOrMoreVMn` | 2 | chatgpt, openclaw |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder9OneOrMoreVyACyxG17_StringProcessing0A0VyxGcfC` | 2 | chatgpt, openclaw |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder9OneOrMoreVyxG17_StringProcessing0A9ComponentAAMc` | 2 | chatgpt, openclaw |
| libswiftRegexBuilder.dylib | `_$s17_StringProcessing14RegexComponentP0C7BuilderAD14CharacterClassVRszrlE3anyAFvgZ` | 2 | chatgpt, openclaw |
| libswiftRegexBuilder.dylib | `_$s17_StringProcessing14RegexComponentP0C7BuilderAD14CharacterClassVRszrlE5anyOfyAFqd__STRd__SJ7ElementRtd__lFZ` | 2 | chatgpt, openclaw |
| libswiftSynchronization.dylib | `_$s15Synchronization5MutexVMa` | 2 | coteditor, openclaw |
| libswiftSynchronization.dylib | `_$s15Synchronization5MutexVMn` | 2 | coteditor, openclaw |
| libswiftSystem.dylib | `_$s6System14FileDescriptorV5_read4into16retryOnInterrupts6ResultOySiAA5ErrnoVGSw_SbtKF` | 2 | chatgpt, openclaw |
| libswiftSystem.dylib | `_$s6System14FileDescriptorV6_write_16retryOnInterrupts6ResultOySiAA5ErrnoVGSW_SbtF` | 2 | chatgpt, openclaw |
| libswiftSystem.dylib | `_$s6System8FilePathV11withCStringyxxSPys4Int8VGKXEKlF` | 2 | openclaw, orbstack |
| libswiftSystem.dylib | `_$s6System8FilePathV2eeoiySbAC_ACtFZ` | 2 | chatgpt, openclaw |
| libswiftSystem.dylib | `_$s6System8FilePathV9appendingyACSSnF` | 2 | chatgpt, openclaw |
| libswiftSystem.dylib | `_$s6System8FilePathVMn` | 2 | chatgpt, openclaw |
| libswiftSystem.dylib | `_$s6System8FilePathVSHAAMc` | 2 | chatgpt, openclaw |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing13_RegexFactoryV7captureyAA0C0VyxGq_AA0C9ComponentR_r0_lF` | 2 | chatgpt, openclaw |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing13_RegexFactoryV9oneOrMoreyAA0C0VyxGq__AA0C18RepetitionBehaviorVSgtAA0C9ComponentR_r0_lF` | 2 | chatgpt, openclaw |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing23RegexRepetitionBehaviorV9reluctantACvgZ` | 2 | chatgpt, openclaw |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV11prefixMatch2inAC0E0Vyx_GSgSs_tKF` | 2 | coteditor, openclaw |
| libswift_StringProcessing.dylib | `_$sSl17_StringProcessingE14trimmingPrefix5while11SubSequenceQzSb7ElementQzKXE_tKF` | 2 | chatgpt, openclaw |
| libswift_StringProcessing.dylib | `_$sSl17_StringProcessingSQ7ElementRpzrlE14trimmingPrefixy11SubSequenceQzqd__STRd__ABQyd__ACRSlF` | 2 | openclaw, orbstack |
| libswift_StringProcessing.dylib | `_$sSm17_StringProcessingSQ7ElementRpzrlE7replace_4with15maxReplacementsyqd___qd_0_SitSlRd__SlRd_0_ABQyd__ACRSABQyd_0_AGRSr0_lF` | 2 | chatgpt, coteditor |
| libswiftos.dylib | `_$s2os0A9_signpost_3dso3log4name0B2IDySo0a1_B7_type_ta_SVSo03OS_a1_D0Cs12StaticStringVAA010OSSignpostF0VtF` | 2 | raycast, utm |
| libswiftos.dylib | `_$s2os12OSSignpostIDVMn` | 2 | utm, whatsapp |
| libswiftos.dylib | `_$s2os12OSSignposterV6loggerAcA6LoggerV_tcfC` | 2 | chatgpt, raycast |
| libswiftos.dylib | `_$s2os12OSSignposterV9subsystem8categoryACSS_SStcfC` | 2 | chatgpt, cmux |
| libswiftos.dylib | `_$s2os6LoggerVACycfC` | 2 | openclaw, whatsapp |
| libswiftos.dylib | `_$sSo9OS_os_logC0B0E8disabledABvgZ` | 2 | chatgpt, clipy |
| CoreML.framework | `_$s6CoreML13MLShapedArrayV5shapeSaySiGvg` | 1 | openclaw |
| CoreML.framework | `_$s6CoreML13MLShapedArrayVMn` | 1 | openclaw |
| CoreML.framework | `_$s6CoreML13MLShapedArrayVyxGAA0cD8ProtocolAAMc` | 1 | openclaw |
| CoreML.framework | `_$s6CoreML21MLShapedArrayProtocolPAAE7scalarsSay6ScalarQzGvM` | 1 | openclaw |
| CoreML.framework | `_$s6CoreML21MLShapedArrayProtocolPAAE7scalarsSay6ScalarQzGvg` | 1 | openclaw |
| CoreML.framework | `_$sSf6CoreML19MLShapedArrayScalarAAWP` | 1 | openclaw |
| CoreML.framework | `_$ss5Int32V6CoreML19MLShapedArrayScalarACWP` | 1 | openclaw |
| ExtensionFoundation.framework | `_$s13Configuration19ExtensionFoundation03AppB0PTl` | 1 | qlmarkdown |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA0P13ConfigurationAC_AA0caD0Tn` | 1 | qlmarkdown |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA0P13configuration13ConfigurationQzvgTq` | 1 | qlmarkdown |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA0PAAE4mainyyKFZ` | 1 | qlmarkdown |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA0PxycfCTq` | 1 | qlmarkdown |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA8IdentityV10IdentitiesV13AsyncIteratorV4nextSayACGSgyYaF` | 1 | cmux |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA8IdentityV10IdentitiesV13AsyncIteratorV4nextSayACGSgyYaFTu` | 1 | cmux |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA8IdentityV10IdentitiesV13AsyncIteratorVMa` | 1 | cmux |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA8IdentityV10IdentitiesV17makeAsyncIteratorAE0gH0VyF` | 1 | cmux |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA8IdentityV10IdentitiesVMa` | 1 | cmux |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA8IdentityV12AvailabilityV13disabledCountSivg` | 1 | cmux |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA8IdentityV12AvailabilityV15unapprovedCountSivg` | 1 | cmux |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA8IdentityV12AvailabilityVMa` | 1 | cmux |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA8IdentityV12AvailabilityVMn` | 1 | cmux |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA8IdentityV13localizedNameSSvg` | 1 | cmux |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA8IdentityV16bundleIdentifierSSvg` | 1 | cmux |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA8IdentityV16bundleIdentifierSSvpMV` | 1 | cmux |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA8IdentityV19availabilityUpdatesScSyAC12AvailabilityVGvgZ` | 1 | cmux |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA8IdentityV8matching03appA8PointIDsAC10IdentitiesVSSd_tKFZ` | 1 | cmux |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA8IdentityVMa` | 1 | cmux |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA8IdentityVMn` | 1 | cmux |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA8IdentityVSQAAMc` | 1 | cmux |
| ExtensionKit.framework | `_$s12ExtensionKit012PrimitiveAppA5SceneV2id7content12onConnectionACSS_xycSbSo15NSXPCConnectionCctc7SwiftUI4ViewRzlufC` | 1 | cmux |
| ExtensionKit.framework | `_$s12ExtensionKit012PrimitiveAppA5SceneVAA0daE0AAWP` | 1 | cmux |
| ExtensionKit.framework | `_$s12ExtensionKit012PrimitiveAppA5SceneVMn` | 1 | cmux |
| ExtensionKit.framework | `_$s12ExtensionKit03AppA18SceneConfigurationVMn` | 1 | cmux |
| ExtensionKit.framework | `_$s12ExtensionKit03AppA18SceneConfigurationVyACxyXAcAA0caD0RzlufC` | 1 | cmux |
| ExtensionKit.framework | `_$s12ExtensionKit03AppA5SceneMp` | 1 | cmux |
| ExtensionKit.framework | `_$s12ExtensionKit03AppA5SceneP05_makeD04withAA09PrimitivecaD0VSgSS_tFTq` | 1 | cmux |
| ExtensionKit.framework | `_$s12ExtensionKit03AppA5SceneP4BodyAC_AaBTn` | 1 | cmux |
| ExtensionKit.framework | `_$s12ExtensionKit03AppA5SceneP4body4BodyQzvgTq` | 1 | cmux |
| ExtensionKit.framework | `_$s12ExtensionKit03AppA5ScenePAAE05_makeD04withAA09PrimitivecaD0VSgSS_tF` | 1 | cmux |
| ExtensionKit.framework | `_$s4Body12ExtensionKit03AppB5ScenePTl` | 1 | cmux |
| ExtensionKit.framework | `_$sSo20EXHostViewControllerC12ExtensionKitE13ConfigurationV03appD07sceneIDAE0D10Foundation03AppD8IdentityV_SStcfC` | 1 | cmux |
| ExtensionKit.framework | `_$sSo20EXHostViewControllerC12ExtensionKitE13ConfigurationVMa` | 1 | cmux |
| ExtensionKit.framework | `_$sSo20EXHostViewControllerC12ExtensionKitE13ConfigurationVMn` | 1 | cmux |
| ExtensionKit.framework | `_$sSo20EXHostViewControllerC12ExtensionKitE13configurationAbCE13ConfigurationVSgvs` | 1 | cmux |
| SwiftData.framework | `_$s9SwiftData07BackingB0Mp` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData07BackingB0P8setValue6forKey2toys0G4PathCy5ModelQzqd__G_qd__tAA22RelationshipCollectionRd__17PersistentElementQyd__Rsd_0_r0_lFTj` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData07BackingB0P8setValue6forKey2toys0G4PathCy5ModelQzqd__G_qd__tSERd__lFTj` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData07BackingB0P8setValue6forKey2toys0G4PathCy5ModelQzqd__SgG_AKtAA010PersistentJ0Rd__lFTj` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData12ModelContextC10fetchCountySiAA15FetchDescriptorVyxGKAA010PersistentC0RzlFTj` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData12ModelContextC11transaction5blockyyyKXE_tKFTj` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData12ModelContextC21processPendingChangesyyFTj` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData12ModelContextC4saveyyKFTj` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData12ModelContextC5fetchySayxGAA15FetchDescriptorVyxGKAA010PersistentC0RzlFTj` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData12ModelContextC6delete5model5where17includeSubclassesyxm_10Foundation9PredicateVyx_QPGSgSbtKAA010PersistentC0RzlFTj` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData12ModelContextC6deleteyyxAA010PersistentC0RzlFTj` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData12ModelContextC6insertyyxAA010PersistentC0RzlFTj` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData14ModelContainerC11mainContextAA0cF0CvgTj` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData14ModelContainerC3for13migrationPlan14configurationsAcA010PersistentC0_pXpd_AA015SchemaMigrationG0_pXpSgAA0C13ConfigurationVdtKcfC` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData14ModelContainerCMa` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData14ModelContainerCMn` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData15FetchDescriptorV9predicate6sortByACyxG10Foundation9PredicateVyx_QPGSg_SayAG04SortD0VyxGGtcfC` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData15FetchDescriptorVMn` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData15PersistentModelMp` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData15PersistentModelP017persistentBackingB0AA0fB0_px0D0Rts_XPvMTq` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData15PersistentModelP017persistentBackingB0AA0fB0_px0D0Rts_XPvgTq` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData15PersistentModelP017persistentBackingB0AA0fB0_px0D0Rts_XPvsTq` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData15PersistentModelP07backingB0xAA07BackingB0_px0D0Rts_XP_tcfCTq` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData15PersistentModelP11Observation10ObservableTb` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData15PersistentModelP14schemaMetadataSayAA6SchemaC08PropertyF0VGvgZTq` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData15PersistentModelPAAE013createBackingB0QryAaBRd__lFZ` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData15PersistentModelPAAE013createBackingB0QryAaBRd__lFZQOMQ` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData15PersistentModelPAAE2eeoiySbx_xtFZ` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData15PersistentModelPAAE2idAA0C10IdentifierVvg` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData15PersistentModelPAAE4hash4intoys6HasherVz_tF` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData15PersistentModelPAAE8getValue6forKeyqd__Sgs0H4PathCyxAFG_tAaBRd__lF` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData15PersistentModelPAAE8getValue6forKeyqd__s0H4PathCyxqd__G_tAA22RelationshipCollectionRd__0C7ElementQyd__Rsd_0_r0_lF` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData15PersistentModelPAAE8getValue6forKeyqd__s0H4PathCyxqd__G_tSeRd__lF` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData15PersistentModelPAAE8setValue6forKey2toys0H4PathCyxqd__G_qd__tAA22RelationshipCollectionRd__0C7ElementQyd__Rsd_0_r0_lF` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData15PersistentModelPAAE8setValue6forKey2toys0H4PathCyxqd__G_qd__tSERd__lF` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData15PersistentModelPAAE8setValue6forKey2toys0H4PathCyxqd__SgG_AItAaBRd__lF` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData15PersistentModelPSHTb` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData15PersistentModelPs12IdentifiableTb` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData18ModelConfigurationV16CloudKitDatabaseV9automaticAEvgZ` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData18ModelConfigurationV16CloudKitDatabaseVMa` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData18ModelConfigurationVMa` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData18ModelConfigurationVMn` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData18ModelConfigurationV_6schema3url10allowsSave16cloudKitDatabaseACSSSg_AA6SchemaCSg10Foundation3URLVSbAC05CloudjK0VtcfC` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData20PersistentIdentifierV2eeoiySbAC_ACtFZ` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData20PersistentIdentifierVMa` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData20PersistentIdentifierVMn` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData20PersistentIdentifierVSHAAMc` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData6SchemaC12RelationshipC10DeleteRuleO7cascadeyA2GmFWC` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData6SchemaC12RelationshipC10DeleteRuleO7nullifyyA2GmFWC` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData6SchemaC12RelationshipC10DeleteRuleOMa` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData6SchemaC12RelationshipCAA0C8PropertyAAMc` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData6SchemaC12RelationshipCMa` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData6SchemaC12RelationshipC_10deleteRule17minimumModelCount07maximumhI012originalName7inverse12hashModifierA2E6OptionVd_AE06DeleteF0OSiSgAPSSSgs10AnyKeyPathCSgAQtcfc` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData6SchemaC16PropertyMetadataV4name7keypath12defaultValue8metadataAESS_s10AnyKeyPathCypSgAA0cD0_pSgtcfC` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData6SchemaC16PropertyMetadataVMa` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData6SchemaC16PropertyMetadataVMn` | 1 | maccy |
| SwiftData.framework | `_$sSayxG9SwiftData22RelationshipCollectionA2B15PersistentModelRzlMc` | 1 | maccy |
| Virtualization.framework | `_$sSo16VZVirtualMachineC14VirtualizationE5pause17completionHandleryys6ResultOyyts5Error_pGc_tF` | 1 | orbstack |
| Virtualization.framework | `_$sSo16VZVirtualMachineC14VirtualizationE5start17completionHandleryys6ResultOyyts5Error_pGc_tF` | 1 | orbstack |
| Virtualization.framework | `_$sSo16VZVirtualMachineC14VirtualizationE6resume17completionHandleryys6ResultOyyts5Error_pGc_tF` | 1 | orbstack |
| Virtualization.framework | `_$sSo20VZVirtioSocketDeviceC14VirtualizationE7connect6toPort17completionHandlerys6UInt32V_ys6ResultOySo0aB10ConnectionCs5Error_pGctF` | 1 | orbstack |
| WeatherKit.framework | `_$s10WeatherKit03DayA0V14lowTemperature10Foundation11MeasurementVySo06NSUnitE0CGvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit03DayA0V14rainfallAmount10Foundation11MeasurementVySo12NSUnitLengthCGvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit03DayA0V14snowfallAmount10Foundation11MeasurementVySo12NSUnitLengthCGvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit03DayA0V15highTemperature10Foundation11MeasurementVySo06NSUnitE0CGvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit03DayA0V3sunAA9SunEventsVvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit03DayA0V4moonAA10MoonEventsVvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit03DayA0VMa` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit03DayA0VMn` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit03DayA0VSEAAMc` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit03DayA0VSQAAMc` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit03DayA0VSeAAMc` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0V10cloudCoverSdvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0V10symbolNameSSvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0V10visibility10Foundation11MeasurementVySo12NSUnitLengthCGvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0V11temperature10Foundation11MeasurementVySo17NSUnitTemperatureCGvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0V13pressureTrendAA08PressureE0Ovg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0V19apparentTemperature10Foundation11MeasurementVySo06NSUnitE0CGvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0V19precipitationChanceSdvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0V4date10Foundation4DateVvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0V4windAA4WindVvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0V7uvIndexAA7UVIndexVvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0V8dewPoint10Foundation11MeasurementVySo17NSUnitTemperatureCGvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0V8humiditySdvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0V8pressure10Foundation11MeasurementVySo14NSUnitPressureCGvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0VMa` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0VMn` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0VSEAAMc` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0VSQAAMc` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0VSeAAMc` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit07CurrentA0V10symbolNameSSvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit07CurrentA0V11temperature10Foundation11MeasurementVySo17NSUnitTemperatureCGvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit07CurrentA0V9conditionAA0A9ConditionOvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit07CurrentA0VMa` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit07CurrentA0VMn` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit07CurrentA0VSEAAMc` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit0A0V07currentA0AA07CurrentA0Vvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit0A0V13dailyForecastAA0D0VyAA03DayA0VGvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit0A0V14hourlyForecastAA0D0VyAA04HourA0VGvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit0A0VMa` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit0A5QueryV5dailyACyAA8ForecastVyAA03DayA0VGGvgZ` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit0A5QueryV6hourlyACyAA8ForecastVyAA04HourA0VGGvgZ` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit0A5QueryV7currentACyAA07CurrentA0VGvgZ` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit0A5QueryVMn` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit0A7ServiceC7weather3for9includingxSo10CLLocationC_AA0A5QueryVyxGtYaKlF` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit0A7ServiceC7weather3for9includingxSo10CLLocationC_AA0A5QueryVyxGtYaKlFTu` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit0A7ServiceC7weather3forAA0A0VSo10CLLocationC_tYaKF` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit0A7ServiceC7weather3forAA0A0VSo10CLLocationC_tYaKFTu` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit0A7ServiceCACycfC` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit0A7ServiceCMa` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO11descriptionSSvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionOMa` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit10MoonEventsV5phaseAA0C5PhaseOvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit10MoonEventsV7moonset10Foundation4DateVSgvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit10MoonEventsV8moonrise10Foundation4DateVSgvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit10MoonEventsVMa` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit13PressureTrendO11descriptionSSvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit13PressureTrendOMa` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit4WindV16CompassDirectionO11descriptionSSvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit4WindV16compassDirectionAC07CompassE0Ovg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit4WindV5speed10Foundation11MeasurementVySo11NSUnitSpeedCGvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit4WindVMa` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit7UVIndexV16ExposureCategoryO11descriptionSSvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit7UVIndexV8categoryAC16ExposureCategoryOvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit7UVIndexVMa` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit8ForecastV8forecastSayxGvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit8ForecastVMa` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit8ForecastVMn` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit8ForecastVyxGSEAAMc` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit8ForecastVyxGSTAAMc` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit8ForecastVyxGSlAAMc` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit9MoonPhaseO11descriptionSSvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit9SunEventsV6sunset10Foundation4DateVSgvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit9SunEventsV7sunrise10Foundation4DateVSgvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit9SunEventsVMa` | 1 | raycast |
| _AVKit_SwiftUI.framework | `_$s14_AVKit_SwiftUI11VideoPlayerVAA0bC09EmptyViewVRszrlE6playerACyAFGSo8AVPlayerCSg_tcfC` | 1 | openclaw |
| _AVKit_SwiftUI.framework | `_$s14_AVKit_SwiftUI11VideoPlayerVMn` | 1 | openclaw |
| _AppIntents_AppKit.framework | `_$s10AppIntents21DisplayRepresentationV5ImageV01_ab1_A3KitE10systemName9tintColor19symbolConfigurationAESgSS_So7NSColorCSgSo013NSImageSymbolL0CSgtcfC` | 1 | tailscale-app |
| libswiftCore.dylib | `_$sSNyxGSksSxRzSZ6StrideRpzrlMc` | 1 | vorssaint |
| libswiftCore.dylib | `_$sSdSxsWP` | 1 | vorssaint |
| libswiftCore.dylib | `_$sSh7isEmptySbvpMV` | 1 | vorssaint |
| libswiftCore.dylib | `_$sSlsE5countSivpMV` | 1 | vorssaint |
| libswiftCore.dylib | `_$sSm17_customRemoveLast7ElementQzSgyFTj` | 1 | vorssaint |
| libswiftCore.dylib | `_$sSs17UnicodeScalarViewVMn` | 1 | vorssaint |
| libswiftCore.dylib | `_$ss10SetAlgebraP16formIntersectionyyxFTj` | 1 | vorssaint |
| libswiftCore.dylib | `_$ss10SetAlgebraP19symmetricDifferenceyxxnFTj` | 1 | vorssaint |
| libswiftCore.dylib | `_$ss10SetAlgebraP7isEmptySbvgTj` | 1 | vorssaint |
| libswiftCore.dylib | `_$ss10SetAlgebraP8isSubset2ofSbx_tFTj` | 1 | vorssaint |
| libswiftCore.dylib | `_$ss10SetAlgebraP8subtractyyxFTj` | 1 | vorssaint |
| libswiftCore.dylib | `_$ss18ReversedCollectionVyxGSksSkRzrlMc` | 1 | vorssaint |
| libswiftCore.dylib | `_$ss7UnicodeO6ScalarV10PropertiesV12isAlphabeticSbvg` | 1 | vorssaint |
| libswiftCore.dylib | `_$ss7UnicodeO6ScalarV10PropertiesV15isEmojiModifierSbvg` | 1 | vorssaint |
| libswiftCore.dylib | `_$ss7UnicodeO6ScalarV10PropertiesV19isEmojiModifierBaseSbvg` | 1 | vorssaint |
| libswiftDarwin.dylib | `_$s6Darwin10SEM_FAILEDSpys5Int32VGSgvg` | 1 | orbstack |
| libswiftDarwin.dylib | `_$s6Darwin8sem_openySpys5Int32VGSgSPys4Int8VG_ADs6UInt16Vs6UInt32VtF` | 1 | orbstack |
| libswiftDarwin.dylib | `_$s6Darwin8sem_openySpys5Int32VGSgSPys4Int8VG_ADtF` | 1 | orbstack |
| libswiftDispatch.dylib | `_$s8Dispatch0A12TimeIntervalO12millisecondsyACSicACmFWC` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$s8Dispatch0A12TimeIntervalOMa` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$s8Dispatch0A13WorkItemFlagsVMa` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$s8Dispatch0A13WorkItemFlagsVMn` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$s8Dispatch0A13WorkItemFlagsVs10SetAlgebraAAMc` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$s8Dispatch0A3QoSVMa` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$s8Dispatch0A4TimeVMa` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$s8Dispatch1poiyAA0A4TimeVAD_AA0aB8IntervalOtF` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$s8Dispatch1poiyAA0A4TimeVAD_SdtF` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$sSo14OS_dispatch_ioC8DispatchE10StreamTypeO6randomyA2EmFWC` | 1 | cmux |
| libswiftDispatch.dylib | `_$sSo17OS_dispatch_groupC8DispatchE4wait7timeoutAC0D13TimeoutResultOAC0D4TimeV_tF` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$sSo17OS_dispatch_groupC8DispatchE4waityyF` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$sSo17OS_dispatch_queueC8DispatchE10AttributesVMa` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$sSo17OS_dispatch_queueC8DispatchE10AttributesVMn` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$sSo17OS_dispatch_queueC8DispatchE10AttributesVs10SetAlgebraACMc` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$sSo17OS_dispatch_queueC8DispatchE20AutoreleaseFrequencyOMa` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$sSo17OS_dispatch_queueC8DispatchE4mainABvgZ` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$sSo17OS_dispatch_queueC8DispatchE5async5group3qos5flags7executeySo0a1_b1_F0CSg_AC0D3QoSVAC0D13WorkItemFlagsVyyXBtF` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$sSo18OS_dispatch_sourceC8DispatchE10TimerFlagsVMa` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$sSo18OS_dispatch_sourceC8DispatchE10TimerFlagsVMn` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$sSo18OS_dispatch_sourceC8DispatchE10TimerFlagsVs10SetAlgebraACMc` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$sSo18OS_dispatch_sourceC8DispatchE15FileSystemEventV6attribAEvgZ` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$sSo18OS_dispatch_sourceC8DispatchE15FileSystemEventV6deleteAEvgZ` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$sSo18OS_dispatch_sourceC8DispatchE15FileSystemEventV6extendAEvgZ` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$sSo18OS_dispatch_sourceC8DispatchE15FileSystemEventV6renameAEvgZ` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$sSo18OS_dispatch_sourceC8DispatchE15FileSystemEventV6revokeAEvgZ` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$sSo18OS_dispatch_sourceC8DispatchE15FileSystemEventVMn` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$sSo18OS_dispatch_sourceC8DispatchE15FileSystemEventVSQACMc` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$sSo18OS_dispatch_sourceC8DispatchE15FileSystemEventVSYACMc` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$sSo18OS_dispatch_sourceC8DispatchE15FileSystemEventVs10SetAlgebraACMc` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$sSo18OS_dispatch_sourceC8DispatchE15FileSystemEventVs9OptionSetACMc` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$sSo18OS_dispatch_sourceP8DispatchE15setEventHandler3qos5flags7handleryAC0D3QoSV_AC0D13WorkItemFlagsVyyXBSgtF` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$sSo18OS_dispatch_sourceP8DispatchE6cancelyyF` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$sSo18OS_dispatch_sourceP8DispatchE6resumeyyF` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$sSo21OS_dispatch_semaphoreC8DispatchE4wait7timeoutAC0D13TimeoutResultOAC0D4TimeV_tF` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$sSo21OS_dispatch_semaphoreC8DispatchE6signalSiyF` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$sSo24OS_dispatch_source_timerP8DispatchE8schedule8deadline9repeating6leewayyAC0E4TimeV_AC0eJ8IntervalOAKtF` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$sSo24OS_dispatch_source_vnodeP8DispatchE4dataSo0a1_b1_C0CACE15FileSystemEventVvg` | 1 | vorssaint |
| libswiftNaturalLanguage.dylib | `_$sSo11NLTokenizerC15NaturalLanguageE6tokens3forSaySnySS5IndexVGGAH_tF` | 1 | raycast |
| libswiftNaturalLanguage.dylib | `_$sSo8NLTaggerC15NaturalLanguageE03setC0_5rangeySo10NLLanguagea_SnySS5IndexVGtF` | 1 | openclaw |
| libswiftNaturalLanguage.dylib | `_$sSo8NLTaggerC15NaturalLanguageE13enumerateTags2in4unit6scheme7options5usingySnySS5IndexVG_So11NLTokenUnitVSo11NLTagSchemeaSo0A7OptionsVSbSo0N0aSg_ALtXEtF` | 1 | openclaw |
| libswiftObservation.dylib | `_$s11Observation12ObservationsV17makeAsyncIteratorAC0E0Vyxq__GyF` | 1 | coteditor |
| libswiftObservation.dylib | `_$s11Observation12ObservationsV8IteratorV4next9isolationxSgScA_pSgYi_tYaq_YKF` | 1 | coteditor |
| libswiftObservation.dylib | `_$s11Observation12ObservationsV8IteratorV4next9isolationxSgScA_pSgYi_tYaq_YKFTu` | 1 | coteditor |
| libswiftObservation.dylib | `_$s11Observation12ObservationsV8IteratorVMn` | 1 | coteditor |
| libswiftObservation.dylib | `_$s11Observation12ObservationsV8IteratorVyxq__GScIAAMc` | 1 | coteditor |
| libswiftObservation.dylib | `_$s11Observation12ObservationsVMn` | 1 | coteditor |
| libswiftObservation.dylib | `_$s11Observation12ObservationsVyACyxq_GxyYbq_YKYAccfC` | 1 | coteditor |
| libswiftObservation.dylib | `_$s11Observation12ObservationsVyxq_GSciAAMc` | 1 | coteditor |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder10TryCaptureVMn` | 1 | raycast |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder10TryCaptureVyACyxG17_StringProcessing0A0VyxGcfC` | 1 | raycast |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder10TryCaptureVyxG17_StringProcessing0A9ComponentAAMc` | 1 | raycast |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder6RepeatVMn` | 1 | raycast |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder6RepeatVyACyxG17_StringProcessing0A0VyxGcfC` | 1 | raycast |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder6RepeatVyxG17_StringProcessing0A9ComponentAAMc` | 1 | raycast |
| libswiftRegexBuilder.dylib | `_$s17_StringProcessing14RegexComponentP0C7BuilderAD14CharacterClassVRszrlE5digitAFvgZ` | 1 | raycast |
| libswiftSynchronization.dylib | `_$s15Synchronization5_CellVMa` | 1 | openclaw |
| libswiftSynchronization.dylib | `_$s15Synchronization5_CellVMn` | 1 | coteditor |
| libswiftSystem.dylib | `_$s6System14FileDescriptorV11OpenOptionsVMn` | 1 | whatsapp |
| libswiftSystem.dylib | `_$s6System14FileDescriptorV11OpenOptionsVN` | 1 | whatsapp |
| libswiftSystem.dylib | `_$s6System14FileDescriptorV11OpenOptionsVs10SetAlgebraAAMc` | 1 | whatsapp |
| libswiftSystem.dylib | `_$s6System14FileDescriptorV5_open__7options11permissions16retryOnInterrupts6ResultOyAcA5ErrnoVGSPys4Int8VG_AC10AccessModeVAC11OpenOptionsVAA0B11PermissionsVSgSbtFZ` | 1 | openclaw |
| libswiftSystem.dylib | `_$s6System14FileDescriptorVN` | 1 | openclaw |
| libswiftSystem.dylib | `_$s6System14FileDescriptorVSHAAMc` | 1 | openclaw |
| libswiftSystem.dylib | `_$s6System15FilePermissionsVMn` | 1 | whatsapp |
| libswiftSystem.dylib | `_$s6System15FilePermissionsVN` | 1 | whatsapp |
| libswiftSystem.dylib | `_$s6System15FilePermissionsVs10SetAlgebraAAMc` | 1 | whatsapp |
| libswiftSystem.dylib | `_$s6System5ErrnoV11descriptionSSvg` | 1 | openclaw |
| libswiftSystem.dylib | `_$s6System5ErrnoVMn` | 1 | openclaw |
| libswiftSystem.dylib | `_$s6System8FilePathV10isAbsoluteSbvg` | 1 | openclaw |
| libswiftSystem.dylib | `_$s6System8FilePathV13lastComponentAC0E0VSgvg` | 1 | chatgpt |
| libswiftSystem.dylib | `_$s6System8FilePathV13stringLiteralACSS_tcfC` | 1 | openclaw |
| libswiftSystem.dylib | `_$s6System8FilePathV14platformStringACSPys4Int8VG_tcfC` | 1 | orbstack |
| libswiftSystem.dylib | `_$s6System8FilePathV9ComponentV4KindO2eeoiySbAG_AGtFZ` | 1 | chatgpt |
| libswiftSystem.dylib | `_$s6System8FilePathV9ComponentV4kindAE4KindOvg` | 1 | chatgpt |
| libswiftSystem.dylib | `_$s6System8FilePathV9ComponentV6stringSSvg` | 1 | chatgpt |
| libswiftSystem.dylib | `_$s6System8FilePathV9ComponentV9extensionSSSgvg` | 1 | chatgpt |
| libswiftSystem.dylib | `_$s6System8FilePathV9ComponentVMa` | 1 | chatgpt |
| libswiftSystem.dylib | `_$s6System8FilePathV9ComponentVMn` | 1 | chatgpt |
| libswiftSystem.dylib | `_$s6System8FilePathV9ComponentVyAESgSScfC` | 1 | chatgpt |
| libswiftSystem.dylib | `_$s6System8FilePathVSEAAMc` | 1 | chatgpt |
| libswiftSystem.dylib | `_$s6System8FilePathVSQAAMc` | 1 | openclaw |
| libswiftSystem.dylib | `_$s6System8FilePathVSeAAMc` | 1 | chatgpt |
| libswiftSystem.dylib | `_$s6System8FilePathVs23CustomStringConvertibleAAMc` | 1 | chatgpt |
| libswiftSystem.dylib | `_$sSS6SystemE18withPlatformStringyxxSPys4Int8VGKXEKlF` | 1 | openclaw |
| libswiftSystem_Foundation.dylib | `_$s6System8FilePathV01_A11_FoundationEyACSg0D03URLVcfC` | 1 | chatgpt |
| libswiftXPC.dylib | `_$s3XPC0A10_TYPE_DATEs13OpaquePointerVvg` | 1 | orbstack |
| libswiftXPC.dylib | `_$s3XPC0A11_TYPE_SHMEMs13OpaquePointerVvg` | 1 | orbstack |
| libswiftXPC.dylib | `_$s3XPC0A14_TYPE_ACTIVITYs13OpaquePointerVvg` | 1 | orbstack |
| libswiftXPC.dylib | `_$s3XPC0A14_TYPE_ENDPOINTs13OpaquePointerVvg` | 1 | orbstack |
| libswiftXPC.dylib | `_$s3XPC0A16_TYPE_CONNECTIONs13OpaquePointerVvg` | 1 | orbstack |
| libswiftXPC.dylib | `_$s3XPC0A27_ERROR_TERMINATION_IMMINENTSo13OS_xpc_object_pvg` | 1 | orbstack |
| libswift_Concurrency.dylib | `_$sScA15unownedExecutorScevgTj` | 1 | vorssaint |
| libswift_Concurrency.dylib | `_$sScP8rawValues5UInt8Vvg` | 1 | vorssaint |
| libswift_Concurrency.dylib | `_$sScPMa` | 1 | vorssaint |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing13_RegexFactoryV15captureOptionalyAA0C0VyxGq1__SiSgq0_Sgq_KctAA0C9ComponentR1_r2_lF` | 1 | raycast |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing13_RegexFactoryV7exactlyyAA0C0VyxGSi_q_tAA0C9ComponentR_r0_lF` | 1 | raycast |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing14AnyRegexOutputV7ElementV9substringSsSgvg` | 1 | openclaw |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing14AnyRegexOutputV7ElementVMa` | 1 | openclaw |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing14AnyRegexOutputVMa` | 1 | openclaw |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing14AnyRegexOutputVyAC7ElementVSicig` | 1 | openclaw |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing18RegexSemanticLevelV13unicodeScalarACvgZ` | 1 | coteditor |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing18RegexSemanticLevelVMa` | 1 | coteditor |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV10wholeMatch2inAC0E0Vyx_GSgSS_tKF` | 1 | coteditor |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV17matchingSemanticsyACyxGAA0C13SemanticLevelVF` | 1 | coteditor |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV23anchorsMatchLineEndingsyACyxGSbF` | 1 | coteditor |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV5MatchV13dynamicMemberqd__s7KeyPathCyxqd__G_tcluipMV` | 1 | coteditor |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV5MatchV6outputxvpMV` | 1 | coteditor |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV_2asACyxGSS_xmtKcfC` | 1 | coteditor |
| libswift_StringProcessing.dylib | `_$sSK17_StringProcessingSs11SubSequenceRtzrlE10firstRange2ofSnySS5IndexVGSgqd___tAA14RegexComponentRd__lF` | 1 | coteditor |
| libswift_StringProcessing.dylib | `_$sSK17_StringProcessingSs11SubSequenceRtzrlE5split9separator9maxSplits25omittingEmptySubsequencesSaySsGqd___SiSbtAA14RegexComponentRd__lF` | 1 | orbstack |
| libswift_StringProcessing.dylib | `_$sSl17_StringProcessingSQ7ElementRpzrlE5split9separator9maxSplits25omittingEmptySubsequencesSay11SubSequenceQzGqd___SiSbtSlRd__ABQyd__ACRSlF` | 1 | coteditor |
| libswift_StringProcessing.dylib | `_$sSl17_StringProcessingSQ7ElementRpzrlE6ranges2ofSaySny5IndexQzGGqd___tSlRd__ABQyd__ACRSlF` | 1 | orbstack |
| libswift_StringProcessing.dylib | `_$sSm17_StringProcessingE10trimPrefix5whileySb7ElementQzKXE_tKF` | 1 | jordanbaird-ice |
| libswift_StringProcessing.dylib | `_$sSm17_StringProcessingSs11SubSequenceRtzrlE7replace_4with15maxReplacementsyqd_0__qd__SitSlRd__AA14RegexComponentRd_0_SJ7ElementRtd__r0_lF` | 1 | coteditor |
| libswiftos.dylib | `_$s2os12OSSignpostIDV3log6objectACSo03OS_a1_D0C_yXltcfC` | 1 | raycast |
| libswiftos.dylib | `_$s2os12OSSignpostIDV7invalidACvgZ` | 1 | utm |
| libswiftos.dylib | `_$s2os12OSSignposterV9isEnabledSbvg` | 1 | cmux |
| libswiftos.dylib | `_$s2os12OSSignposterV9subsystem8categoryACSS_So03OS_A4_logCAAE8CategoryVtcfC` | 1 | cmux |
| libswiftos.dylib | `_$sSo9OS_os_logC0B0E8CategoryV14dynamicTracingAEvgZ` | 1 | cmux |
| libswiftos.dylib | `_$sSo9OS_os_logC0B0E8CategoryVMa` | 1 | cmux |
| libswiftsimd.dylib | `_$sSo13simd_float3x3a0A0EyABSo0A6_quatfacfC` | 1 | raycast |
