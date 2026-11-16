# Lacunas de Fernanda (Swift runtime, Dispatch)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Fernanda (Swift runtime, Dispatch) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**838** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| libswiftos.dylib | `_$sSo13os_log_type_ta0A0E5errorABvgZ` | 59 | 1password, aldente, applite, bartender |
| libswiftos.dylib | `_$s2os0A4_log_3dso0B0__ySo0a1_B7_type_ta_SVSo03OS_a1_B0Cs12StaticStringVs7CVarArg_pdtF` | 57 | 1password, aldente, applite, bartender |
| libswiftos.dylib | `_$sSo13os_log_type_ta0A0E5faultABvgZ` | 55 | 1password, aldente, applite, bartender |
| libswiftos.dylib | `_$s2os6LoggerV9logObjectSo03OS_a1_C0Cvg` | 50 | 1password, aldente, applite, bartender |
| libswiftos.dylib | `_$s2os6LoggerV9subsystem8categoryACSS_SStcfC` | 50 | 1password, aldente, applite, bartender |
| libswiftos.dylib | `_$s2os6LoggerVMa` | 50 | 1password, aldente, applite, bartender |
| libswiftos.dylib | `_$sSo13os_log_type_ta0A0E4infoABvgZ` | 48 | 1password, applite, bartender, betterdisplay |
| libswiftos.dylib | `_$sSo13os_log_type_ta0A0E5debugABvgZ` | 42 | bartender, betterdisplay, bettertouchtool, caskhub |
| libswiftos.dylib | `_$sSo13os_log_type_ta0A0E7defaultABvgZ` | 38 | 1password, aldente, applite, bartender |
| libswiftos.dylib | `_$s2os6LoggerVMn` | 32 | applite, bartender, bettertouchtool, bitwarden |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexVMn` | 27 | applite, bettertouchtool, caskhub, chatgpt |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexVyxGAA0C9ComponentAAMc` | 27 | applite, bettertouchtool, caskhub, chatgpt |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV06_regexA07versionACyxGSS_SitcfC` | 26 | applite, bettertouchtool, caskhub, chatgpt |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV5MatchVMn` | 26 | applite, bettertouchtool, caskhub, chatgpt |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing14RegexComponentP5regexAA0C0Vy0C6OutputQzGvgTj` | 24 | applite, bettertouchtool, caskhub, chatgpt |
| libswiftos.dylib | `_$sSo9OS_os_logC0B0E7defaultABvgZ` | 22 | 1password, aldente, bartender, betterdisplay |
| libobjc.A.dylib | `_class_getIvarLayout` | 20 | alt-tab, android-file-transfer, bbedit, bettertouchtool |
| libobjc.A.dylib | `_objc_initializeClassPair` | 20 | alt-tab, android-file-transfer, bbedit, bettertouchtool |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV5MatchV13dynamicMemberqd__s7KeyPathCyxqd__G_tcluig` | 20 | applite, bettertouchtool, caskhub, claude |
| libswiftos.dylib | `_$s2os0A4_log_3dso0B04type_ys12StaticStringV_SVSgSo03OS_a1_B0CSo0a1_b1_D2_tas7CVarArg_pdtF` | 20 | 1password, aldente, bartender, betterdisplay |
| libswiftos.dylib | `_$s2os21OSAllocatedUnfairLockVMn` | 20 | applite, bartender, chatgpt, claude |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV10firstMatch2inAC0E0Vyx_GSgSs_tKF` | 19 | applite, bettertouchtool, caskhub, chatgpt |
| libswiftObservation.dylib | `_$s11Observation0A9RegistrarV12withMutation2of7keyPath_q0_x_s03KeyG0Cyxq_Gq0_yKXEtKAA10ObservableRzr1_lF` | 18 | applite, bartender, caskhub, chatgpt |
| libswiftObservation.dylib | `_$s11Observation0A9RegistrarV6access_7keyPathyx_s03KeyE0Cyxq_GtAA10ObservableRzr0_lF` | 18 | applite, bartender, caskhub, chatgpt |
| libswiftObservation.dylib | `_$s11Observation0A9RegistrarVACycfC` | 18 | applite, bartender, caskhub, chatgpt |
| libswiftObservation.dylib | `_$s11Observation0A9RegistrarVMa` | 18 | applite, bartender, caskhub, chatgpt |
| libswiftObservation.dylib | `_$s11Observation0A9RegistrarVMn` | 18 | applite, bartender, caskhub, chatgpt |
| libswiftObservation.dylib | `_$s11Observation10ObservableMp` | 18 | applite, bartender, caskhub, chatgpt |
| libswiftObservation.dylib | `_$s11Observation0A9RegistrarV6didSet_7keyPathyx_s03KeyF0Cyxq_GtAA10ObservableRzr0_lF` | 16 | applite, bartender, caskhub, chatgpt |
| libswiftObservation.dylib | `_$s11Observation0A9RegistrarV7willSet_7keyPathyx_s03KeyF0Cyxq_GtAA10ObservableRzr0_lF` | 16 | applite, bartender, caskhub, chatgpt |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV5MatchV6outputxvg` | 15 | bettertouchtool, chatgpt, codexbar, coteditor |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV10wholeMatch2inAC0E0Vyx_GSgSs_tKF` | 13 | chatgpt, cmux, codexbar, coteditor |
| libswiftos.dylib | `_$s2os12OSSignpostIDV3logACSo03OS_a1_D0C_tcfC` | 13 | bartender, bettertouchtool, chatgpt, cmux |
| libswiftos.dylib | `_$s2os12OSSignpostIDVMa` | 13 | bartender, bettertouchtool, chatgpt, cmux |
| libswiftos.dylib | `_$sSo18os_signpost_type_ta0A0E3endABvgZ` | 13 | bartender, bettertouchtool, chatgpt, cmux |
| libswiftos.dylib | `_$sSo18os_signpost_type_ta0A0E5beginABvgZ` | 13 | bartender, bettertouchtool, chatgpt, cmux |
| libswift_StringProcessing.dylib | `_$sSK17_StringProcessingSs11SubSequenceRtzrlE8containsySbqd__AA14RegexComponentRd__lF` | 12 | coteditor, easydict, fluidvoice, ghostty |
| libSystem.B.dylib | `_dispatch_queue_set_width` | 11 | duckduckgo, iina, karabiner-elements, keyclu |
| libswift_StringProcessing.dylib | `_$sSm17_StringProcessingSs11SubSequenceRtzrlE9replacing_15maxReplacements4withxqd_1__Siqd_0_AA5RegexV5MatchVyqd___GKXEtK0I6OutputQyd_1_Rsd__SlRd_0_AA0I9ComponentRd_1_SJ7ElementRtd_0_r1_lF` | 11 | bettertouchtool, coteditor, fluidvoice, markedit |
| libswift_StringProcessing.dylib | `_$sSm17_StringProcessingSs11SubSequenceRtzrlE9replacing_4with15maxReplacementsxqd_0__qd__SitSlRd__AA14RegexComponentRd_0_SJ7ElementRtd__r0_lF` | 11 | bettertouchtool, chatgpt, coteditor, easydict |
| libswiftObservation.dylib | `_$s11Observation04withA8Tracking_8onChangexxyXE_yyYbcyXKtlF` | 10 | bartender, chatgpt, cmux, codexbar |
| libswiftRegexBuilder.dylib | `_$sSy12RegexBuilderSs11SubSequenceRtzrlE5split9separator9maxSplits25omittingEmptySubsequencesSaySsGSS_SiSbtF` | 10 | bettertouchtool, chatgpt, easydict, fluidvoice |
| libswift_StringProcessing.dylib | `_$sSK17_StringProcessingSs11SubSequenceRtzrlE7matches2ofSayAA5RegexV5MatchVyqd___GGqd_0__t0G6OutputQyd_0_Rsd__AA0G9ComponentRd_0_r0_lF` | 10 | applite, bettertouchtool, caskhub, cmux |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder0a9ComponentB0O15buildExpressionyxx17_StringProcessing0aC0RzlFZ` | 9 | bettertouchtool, chatgpt, easydict, fluidvoice |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder0a9ComponentB0O17buildPartialBlock5first17_StringProcessing0A0Vy0A6OutputQzGx_tAF0aC0RzlFZ` | 9 | bettertouchtool, chatgpt, easydict, fluidvoice |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder10ZeroOrMoreVMn` | 9 | bettertouchtool, chatgpt, easydict, fluidvoice |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder10ZeroOrMoreVyACyxG17_StringProcessing0A0VyxGcfC` | 9 | bettertouchtool, chatgpt, easydict, fluidvoice |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder10ZeroOrMoreVyxG17_StringProcessing0A9ComponentAAMc` | 9 | bettertouchtool, chatgpt, easydict, fluidvoice |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder11makeFactory17_StringProcessing01_aD0VyF` | 9 | bettertouchtool, chatgpt, easydict, fluidvoice |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder14CharacterClassV17_StringProcessing0A9ComponentAAMc` | 9 | bettertouchtool, chatgpt, easydict, fluidvoice |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder14CharacterClassVMa` | 9 | bettertouchtool, chatgpt, easydict, fluidvoice |
| libswiftRegexBuilder.dylib | `_$s17_StringProcessing5RegexV0C7BuilderEyACyxGqd__yXEc0C6OutputQyd__RszAA0C9ComponentRd__lufC` | 9 | bettertouchtool, chatgpt, easydict, fluidvoice |
| libswiftRegexBuilder.dylib | `_$sSS17_StringProcessing14RegexComponent0C7BuilderMc` | 9 | bettertouchtool, chatgpt, easydict, fluidvoice |
| libswiftSystem.dylib | `_$s6System8FilePathVMa` | 9 | chatgpt, easydict, ghostty, homebrew-app |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing13_RegexFactoryV10accumulateyAA0C0VyxGq__q0_tAA0C9ComponentR_AaHR0_r1_lF` | 9 | bettertouchtool, chatgpt, easydict, fluidvoice |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing13_RegexFactoryV10zeroOrMoreyAA0C0VyxGq__AA0C18RepetitionBehaviorVSgtAA0C9ComponentR_r0_lF` | 9 | bettertouchtool, chatgpt, easydict, fluidvoice |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing13_RegexFactoryVMa` | 9 | bettertouchtool, chatgpt, easydict, fluidvoice |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing23RegexRepetitionBehaviorVMa` | 9 | bettertouchtool, chatgpt, easydict, fluidvoice |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing23RegexRepetitionBehaviorVMn` | 9 | bettertouchtool, chatgpt, easydict, fluidvoice |
| libswift_StringProcessing.dylib | `_$sSK17_StringProcessingSs11SubSequenceRtzrlE6starts4withSbqd___tAA14RegexComponentRd__lF` | 9 | chatgpt, coteditor, easydict, fluidvoice |
| libswift_StringProcessing.dylib | `_$sSm17_StringProcessingSQ7ElementRpzrlE9replacing_4with15maxReplacementsxqd___qd_0_SitSlRd__SlRd_0_ABQyd__ACRSABQyd_0_AGRSr0_lF` | 9 | chatgpt, clipy, coteditor, easydict |
| libswiftos.dylib | `_$s2os12OSSignpostIDV8rawValues6UInt64Vvg` | 9 | bettertouchtool, chatgpt, cmux, fluidvoice |
| libswiftos.dylib | `_$s2os12OSSignposterV9logHandleSo03OS_a1_C0Cvg` | 9 | bettertouchtool, chatgpt, cmux, fluidvoice |
| libswiftos.dylib | `_$s2os12OSSignposterVMa` | 9 | bettertouchtool, chatgpt, cmux, fluidvoice |
| libswiftos.dylib | `_$s2os15OSSignpostErrorO9doubleEndyA2CmFWC` | 9 | bettertouchtool, chatgpt, cmux, fluidvoice |
| libswiftos.dylib | `_$s2os15OSSignpostErrorOMa` | 9 | bettertouchtool, chatgpt, cmux, fluidvoice |
| libswiftos.dylib | `_$s2os23OSSignpostIntervalStateC10signpostIDAA0bF0Vvg` | 9 | bettertouchtool, chatgpt, cmux, fluidvoice |
| libswiftos.dylib | `_$s2os23OSSignpostIntervalStateC2id6isOpenAcA0B2IDV_Sbtcfc` | 9 | bettertouchtool, chatgpt, cmux, fluidvoice |
| libswiftos.dylib | `_$s2os23OSSignpostIntervalStateCMa` | 9 | bettertouchtool, chatgpt, cmux, fluidvoice |
| libswiftos.dylib | `_$s2os28checkForErrorAndConsumeState5stateAA010OSSignpostD0OAA0i8IntervalG0C_tF` | 9 | bettertouchtool, chatgpt, cmux, fluidvoice |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder10OptionallyVMn` | 8 | chatgpt, easydict, fluidvoice, openclaw |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder10OptionallyVyACyxG17_StringProcessing0A0VyxGcfC` | 8 | chatgpt, easydict, fluidvoice, openclaw |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder10OptionallyVyxG17_StringProcessing0A9ComponentAAMc` | 8 | chatgpt, easydict, fluidvoice, openclaw |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder7CaptureVMn` | 8 | bettertouchtool, chatgpt, easydict, fluidvoice |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder7CaptureVyACyxG17_StringProcessing0A0VyxGcfC` | 8 | bettertouchtool, chatgpt, easydict, fluidvoice |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder7CaptureVyxG17_StringProcessing0A9ComponentAAMc` | 8 | bettertouchtool, chatgpt, easydict, fluidvoice |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder9OneOrMoreVMn` | 8 | bettertouchtool, chatgpt, easydict, fluidvoice |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder9OneOrMoreVyACyxG17_StringProcessing0A0VyxGcfC` | 8 | bettertouchtool, chatgpt, easydict, fluidvoice |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder9OneOrMoreVyxG17_StringProcessing0A9ComponentAAMc` | 8 | bettertouchtool, chatgpt, easydict, fluidvoice |
| libswiftRegexBuilder.dylib | `_$s17_StringProcessing14RegexComponentP0C7BuilderAD14CharacterClassVRszrlE3anyAFvgZ` | 8 | bettertouchtool, chatgpt, easydict, fluidvoice |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing13_RegexFactoryV7captureyAA0C0VyxGq_AA0C9ComponentR_r0_lF` | 8 | bettertouchtool, chatgpt, easydict, fluidvoice |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing13_RegexFactoryV9oneOrMoreyAA0C0VyxGq__AA0C18RepetitionBehaviorVSgtAA0C9ComponentR_r0_lF` | 8 | bettertouchtool, chatgpt, easydict, fluidvoice |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing13_RegexFactoryV9zeroOrOneyAA0C0VyxGq__AA0C18RepetitionBehaviorVSgtAA0C9ComponentR_r0_lF` | 8 | chatgpt, easydict, fluidvoice, openclaw |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing14AnyRegexOutputVMn` | 8 | bettertouchtool, coteditor, easydict, markedit |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV11ignoresCaseyACyxGSbF` | 8 | coteditor, fluidvoice, loop, openclaw |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexVA2A03AnyC6OutputVRszlEyACyAEGSSKcfC` | 8 | bettertouchtool, coteditor, easydict, markedit |
| libswift_StringProcessing.dylib | `_$sSl17_StringProcessingSQ7ElementRpzrlE14trimmingPrefixy11SubSequenceQzqd__STRd__ABQyd__ACRSlF` | 8 | codexbar, fluidvoice, macwhisper, openclaw |
| libswiftos.dylib | `_$s2os12OSSignpostIDV9exclusiveACvgZ` | 8 | chatgpt, codexbar, fluidvoice, macwhisper |
| libswiftos.dylib | `_$s2os6LoggerVACycfC` | 8 | applite, fluidvoice, macwhisper, markedit |
| libswiftos.dylib | `_$s2os6LoggerVyACSo03OS_A4_logCcfC` | 8 | chatgpt, clipy, dockdoor, loop |
| libswiftos.dylib | `_$sSo18os_signpost_type_ta0A0E5eventABvgZ` | 8 | bartender, cmux, codexbar, macwhisper |
| libobjc.A.dylib | `_object_getInstanceVariable` | 7 | adobe-acrobat-reader, cmux, daisydisk, ghostty |
| libswiftSystem.dylib | `_$s6System14FileDescriptorVMn` | 7 | chatgpt, homebrew-app, macpacker, openclaw |
| libswiftSystem.dylib | `_$s6System5ErrnoVN` | 7 | chatgpt, homebrew-app, macpacker, openclaw |
| libswiftSystem.dylib | `_$s6System5ErrnoVs5ErrorAAMc` | 7 | chatgpt, homebrew-app, macpacker, openclaw |
| libswiftSystem.dylib | `_$s6System8FilePathVyACSScfC` | 7 | chatgpt, ghostty, homebrew-app, macpacker |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing23RegexRepetitionBehaviorV9reluctantACvgZ` | 7 | bettertouchtool, chatgpt, fluidvoice, openclaw |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV5MatchV5rangeSnySS5IndexVGvg` | 7 | bettertouchtool, caskhub, coteditor, easydict |
| libswiftos.dylib | `_$s2os0A9_signpost_3dso3log4name0B2ID__ySo0a1_B7_type_ta_SVSo03OS_a1_D0Cs12StaticStringVAA010OSSignpostF0VALs7CVarArg_pdtF` | 7 | bartender, codexbar, protonvpn, raycast |
| libswiftos.dylib | `_$s2os0A9_signpost_3dso3log4name0B2IDySo0a1_B7_type_ta_SVSo03OS_a1_D0Cs12StaticStringVAA010OSSignpostF0VtF` | 7 | bartender, bettertouchtool, codexbar, protonvpn |
| libswiftos.dylib | `_$s2os12OSSignposterVMn` | 7 | chatgpt, cmux, fluidvoice, protonvpn |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder14CharacterClassV8invertedACvg` | 6 | chatgpt, easydict, fluidvoice, openclaw |
| libswiftRegexBuilder.dylib | `_$s17_StringProcessing14RegexComponentP0C7BuilderAD14CharacterClassVRszrlE5anyOfyAFqd__STRd__SJ7ElementRtd__lFZ` | 6 | chatgpt, easydict, fluidvoice, openclaw |
| libswiftSystem.dylib | `_$s6System14FileDescriptorV5_read4into16retryOnInterrupts6ResultOySiAA5ErrnoVGSw_SbtKF` | 6 | chatgpt, homebrew-app, macpacker, openclaw |
| libswiftSystem.dylib | `_$s6System14FileDescriptorV6_closes6ResultOyytAA5ErrnoVGyF` | 6 | chatgpt, homebrew-app, macpacker, openclaw |
| libswiftSystem.dylib | `_$s6System14FileDescriptorV6_write_16retryOnInterrupts6ResultOySiAA5ErrnoVGSW_SbtF` | 6 | chatgpt, homebrew-app, macpacker, openclaw |
| libswiftSystem.dylib | `_$s6System8FilePathV11withCStringyxxSPys4Int8VGKXEKlF` | 6 | easydict, homebrew-app, macpacker, openclaw |
| libswiftSystem.dylib | `_$s6System8FilePathV6stringSSvg` | 6 | chatgpt, ghostty, homebrew-app, macpacker |
| libswiftDarwin.dylib | `_$s6Darwin7environSpySpys4Int8VGSgGvg` | 5 | betterdisplay, cmux, codexbar, openclaw |
| libswiftDispatch.dylib | `_$s8Dispatch0A12DataIteratorVMa` | 5 | chatgpt, claude, cmux, easydict |
| libswiftDispatch.dylib | `_$s8Dispatch0A12DataIteratorVMn` | 5 | chatgpt, claude, cmux, easydict |
| libswiftDispatch.dylib | `_$s8Dispatch0A12DataIteratorVStAAMc` | 5 | chatgpt, claude, cmux, easydict |
| libswiftSystem.dylib | `_$s6System8FilePathV2eeoiySbAC_ACtFZ` | 5 | chatgpt, homebrew-app, macpacker, openclaw |
| libswiftSystem.dylib | `_$s6System8FilePathV9appendingyACSSnF` | 5 | chatgpt, homebrew-app, macpacker, openclaw |
| libswiftSystem.dylib | `_$s6System8FilePathVMn` | 5 | chatgpt, homebrew-app, macpacker, openclaw |
| libswiftSystem.dylib | `_$s6System8FilePathVSHAAMc` | 5 | chatgpt, homebrew-app, macpacker, openclaw |
| libswiftSystem.dylib | `_$sSS6SystemE18withPlatformStringyxxSPys4Int8VGKXEKlF` | 5 | homebrew-app, macpacker, openclaw, teamviewer |
| libswift_StringProcessing.dylib | `_$sSl17_StringProcessingE14trimmingPrefix5while11SubSequenceQzSb7ElementQzKXE_tKF` | 5 | chatgpt, easydict, openclaw, superwhisper |
| libswiftos.dylib | `_$s2os12OSSignpostIDVMn` | 5 | bettertouchtool, protonvpn, supacode, utm |
| libswiftos.dylib | `_$s2os12OSSignposterV9subsystem8categoryACSS_SStcfC` | 5 | bettertouchtool, chatgpt, cmux, fluidvoice |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA0Mp` | 4 | cmux, fuse-t, qlmarkdown, syntax-highlight |
| libswiftNaturalLanguage.dylib | `_$sSo11NLTokenizerC15NaturalLanguageE6tokens3forSaySnySS5IndexVGGAH_tF` | 4 | markedit, raycast, superwhisper, voiceink |
| libswiftNaturalLanguage.dylib | `_$sSo8NLTaggerC15NaturalLanguageE13enumerateTags2in4unit6scheme7options5usingySnySS5IndexVG_So11NLTokenUnitVSo11NLTagSchemeaSo0A7OptionsVSbSo0N0aSg_ALtXEtF` | 4 | fluidvoice, openclaw, superwhisper, voiceink |
| libswiftSynchronization.dylib | `_$s15Synchronization5MutexVMn` | 4 | coteditor, homebrew-app, openclaw, thaw |
| libswiftSystem.dylib | `_$s6System14FileDescriptorV5_open__7options11permissions16retryOnInterrupts6ResultOyAcA5ErrnoVGSPys4Int8VG_AC10AccessModeVAC11OpenOptionsVAA0B11PermissionsVSgSbtFZ` | 4 | homebrew-app, macpacker, openclaw, thaw |
| libswiftSystem.dylib | `_$s6System14FileDescriptorVN` | 4 | homebrew-app, macpacker, openclaw, thaw |
| libswiftSystem.dylib | `_$s6System14FileDescriptorVSHAAMc` | 4 | homebrew-app, macpacker, openclaw, thaw |
| libswiftSystem.dylib | `_$s6System5ErrnoV11descriptionSSvg` | 4 | homebrew-app, macpacker, openclaw, thaw |
| libswiftSystem.dylib | `_$s6System5ErrnoVMn` | 4 | homebrew-app, macpacker, openclaw, thaw |
| libswiftSystem.dylib | `_$s6System8FilePathV13stringLiteralACSS_tcfC` | 4 | homebrew-app, macpacker, openclaw, thaw |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV11prefixMatch2inAC0E0Vyx_GSgSs_tKF` | 4 | coteditor, openclaw, sfm, thaw |
| libswiftos.dylib | `_$s2os12OSSignposterV6loggerAcA6LoggerV_tcfC` | 4 | chatgpt, macwhisper, raycast, superwhisper |
| libswiftos.dylib | `_$s2os12OSSignposterV9subsystem8categoryACSS_So03OS_A4_logCAAE8CategoryVtcfC` | 4 | cmux, fluidvoice, opensuperwhisper, superwhisper |
| libswiftos.dylib | `_$s2os23OSSignpostIntervalStateCMn` | 4 | cmux, protonvpn, raycast, whatsapp |
| libswiftos.dylib | `_$sSo9OS_os_logC0B0E8CategoryVMa` | 4 | cmux, fluidvoice, opensuperwhisper, superwhisper |
| CoreML.framework | `_$sSf6CoreML19MLShapedArrayScalarAAWP` | 3 | fluidvoice, openclaw, voiceink |
| ExtensionFoundation.framework | `_$s13Configuration19ExtensionFoundation03AppB0PTl` | 3 | fuse-t, qlmarkdown, syntax-highlight |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA0P13ConfigurationAC_AA0caD0Tn` | 3 | fuse-t, qlmarkdown, syntax-highlight |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA0P13configuration13ConfigurationQzvgTq` | 3 | fuse-t, qlmarkdown, syntax-highlight |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA0PAAE4mainyyKFZ` | 3 | fuse-t, qlmarkdown, syntax-highlight |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA0PxycfCTq` | 3 | fuse-t, qlmarkdown, syntax-highlight |
| SwiftData.framework | `_$s9SwiftData07BackingB0Mp` | 3 | bartender, maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData07BackingB0P8setValue6forKey2toys0G4PathCy5ModelQzqd__G_qd__tSERd__lFTj` | 3 | bartender, maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData12ModelContextC4saveyyKFTj` | 3 | bartender, maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData12ModelContextC5fetchySayxGAA15FetchDescriptorVyxGKAA010PersistentC0RzlFTj` | 3 | bartender, maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData12ModelContextC6deleteyyxAA010PersistentC0RzlFTj` | 3 | bartender, maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData12ModelContextC6insertyyxAA010PersistentC0RzlFTj` | 3 | bartender, maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData14ModelContainerCMa` | 3 | bartender, maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData14ModelContainerCMn` | 3 | bartender, maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData15FetchDescriptorV9predicate6sortByACyxG10Foundation9PredicateVyx_QPGSg_SayAG04SortD0VyxGGtcfC` | 3 | bartender, maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData15FetchDescriptorVMn` | 3 | bartender, maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData15PersistentModelMp` | 3 | bartender, maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData15PersistentModelP017persistentBackingB0AA0fB0_px0D0Rts_XPvMTq` | 3 | bartender, maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData15PersistentModelP017persistentBackingB0AA0fB0_px0D0Rts_XPvgTq` | 3 | bartender, maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData15PersistentModelP017persistentBackingB0AA0fB0_px0D0Rts_XPvsTq` | 3 | bartender, maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData15PersistentModelP07backingB0xAA07BackingB0_px0D0Rts_XP_tcfCTq` | 3 | bartender, maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData15PersistentModelP11Observation10ObservableTb` | 3 | bartender, maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData15PersistentModelP14schemaMetadataSayAA6SchemaC08PropertyF0VGvgZTq` | 3 | bartender, maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData15PersistentModelPAAE013createBackingB0QryAaBRd__lFZ` | 3 | bartender, maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData15PersistentModelPAAE013createBackingB0QryAaBRd__lFZQOMQ` | 3 | bartender, maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData15PersistentModelPAAE8getValue6forKeyqd__s0H4PathCyxqd__G_tSeRd__lF` | 3 | bartender, maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData15PersistentModelPAAE8setValue6forKey2toys0H4PathCyxqd__G_qd__tSERd__lF` | 3 | bartender, maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData15PersistentModelPSHTb` | 3 | bartender, maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData15PersistentModelPs12IdentifiableTb` | 3 | bartender, maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData18ModelConfigurationVMa` | 3 | bartender, maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData18ModelConfigurationVMn` | 3 | bartender, maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData6SchemaC16PropertyMetadataV4name7keypath12defaultValue8metadataAESS_s10AnyKeyPathCypSgAA0cD0_pSgtcfC` | 3 | bartender, maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData6SchemaC16PropertyMetadataVMa` | 3 | bartender, maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData6SchemaC16PropertyMetadataVMn` | 3 | bartender, maccy, voiceink |
| libobjc.A.dylib | `_protocol_addProtocol` | 3 | bitwarden, emacs-app, orbstack |
| libswiftCoreML.dylib | `_$s6CoreML13MLShapedArrayVMn` | 3 | macwhisper, opensuperwhisper, superwhisper |
| libswiftCoreML.dylib | `_$s6CoreML13MLShapedArrayVyxGAA0cD8ProtocolAAMc` | 3 | macwhisper, opensuperwhisper, superwhisper |
| libswiftCoreML.dylib | `_$s6CoreML21MLShapedArrayProtocolPAAE7scalarsSay6ScalarQzGvg` | 3 | macwhisper, opensuperwhisper, superwhisper |
| libswiftCoreML.dylib | `_$sSf6CoreML19MLShapedArrayScalarAAWP` | 3 | macwhisper, opensuperwhisper, superwhisper |
| libswiftCoreML.dylib | `_$sSo12MLMultiArrayC6CoreMLE30withUnsafeMutableBufferPointer6ofType_q_xm_q_SryxG_SaySiGtKXEtKAC08MLShapedB6ScalarRzr0_lF` | 3 | macwhisper, opensuperwhisper, superwhisper |
| libswiftCoreML.dylib | `_$sSo7MLModelC6CoreMLE4load10contentsOf13configurationAB10Foundation3URLV_So0A13ConfigurationCtYaKFZ` | 3 | macwhisper, opensuperwhisper, superwhisper |
| libswiftCoreML.dylib | `_$sSo7MLModelC6CoreMLE4load10contentsOf13configurationAB10Foundation3URLV_So0A13ConfigurationCtYaKFZTu` | 3 | macwhisper, opensuperwhisper, superwhisper |
| libswiftCoreML.dylib | `_$ss5Int32V6CoreML19MLShapedArrayScalarACWP` | 3 | macwhisper, opensuperwhisper, superwhisper |
| libswiftDispatch.dylib | `_$sSo24OS_dispatch_queue_serialC8DispatchE10AttributesVMa` | 3 | macwhisper, supacode, voiceink |
| libswiftDispatch.dylib | `_$sSo24OS_dispatch_queue_serialC8DispatchE10AttributesVMn` | 3 | macwhisper, supacode, voiceink |
| libswiftDispatch.dylib | `_$sSo24OS_dispatch_queue_serialC8DispatchE10AttributesVs10SetAlgebraACMc` | 3 | macwhisper, supacode, voiceink |
| libswiftDispatch.dylib | `_$sSo24OS_dispatch_queue_serialC8DispatchE5label3qos10attributes20autoreleaseFrequency6targetABSS_AC0E3QoSVAbCE10AttributesVSo0a1_b1_C0CACE011AutoreleaseJ0OANSgtcfC` | 3 | macwhisper, supacode, voiceink |
| libswiftDispatch.dylib | `_$sSo33OS_dispatch_queue_serial_executorC8DispatchE23asUnownedSerialExecutorSceyF` | 3 | macwhisper, supacode, voiceink |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder011AlternationB0V15buildExpressionyxx17_StringProcessing0A9ComponentRzlFZ` | 3 | bettertouchtool, easydict, sfm |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder011AlternationB0V17buildPartialBlock5firstAA8ChoiceOfVy0A6OutputQzGx_t17_StringProcessing0A9ComponentRzlFZ` | 3 | bettertouchtool, easydict, sfm |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder8ChoiceOfVMn` | 3 | bettertouchtool, easydict, sfm |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder8ChoiceOfVyACyxG17_StringProcessing0A0VyxGcfC` | 3 | bettertouchtool, easydict, sfm |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder8ChoiceOfVyACyxGADyXEcfC` | 3 | bettertouchtool, easydict, sfm |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder8ChoiceOfVyxG17_StringProcessing0A9ComponentAAMc` | 3 | bettertouchtool, easydict, sfm |
| libswiftSynchronization.dylib | `_$s15Synchronization5MutexVMa` | 3 | coteditor, openclaw, thaw |
| libswiftSystem.dylib | `_$s6System14FileDescriptorV10_duplicate2as16retryOnInterrupts6ResultOyAcA5ErrnoVGACSg_SbtKF` | 3 | homebrew-app, macpacker, thaw |
| libswiftSystem.dylib | `_$s6System14FileDescriptorV5_pipes6ResultOyAC7readEnd_AC05writeG0tAA5ErrnoVGyFZ` | 3 | homebrew-app, macpacker, thaw |
| libswiftSystem.dylib | `_$s6System8FilePathV10isAbsoluteSbvg` | 3 | homebrew-app, openclaw, thaw |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing13_RegexFactoryV21accumulateAlternationyAA0C0VyxGq__q0_tAA0C9ComponentR_AaHR0_r1_lF` | 3 | bettertouchtool, easydict, sfm |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing14AnyRegexOutputVMa` | 3 | bettertouchtool, markedit, openclaw |
| libswift_StringProcessing.dylib | `_$sSl17_StringProcessingSQ7ElementRpzrlE5split9separator9maxSplits25omittingEmptySubsequencesSay11SubSequenceQzGqd___SiSbtSlRd__ABQyd__ACRSlF` | 3 | bartender, codexbar, coteditor |
| libswift_StringProcessing.dylib | `_$sSl17_StringProcessingSQ7ElementRpzrlE8containsySbqd__SlRd__ABQyd__ACRSlF` | 3 | macwhisper, superwhisper, xcodes-app |
| libswift_StringProcessing.dylib | `_$sSm17_StringProcessingSQ7ElementRpzrlE7replace_4with15maxReplacementsyqd___qd_0_SitSlRd__SlRd_0_ABQyd__ACRSABQyd_0_AGRSr0_lF` | 3 | chatgpt, coteditor, superwhisper |
| libswift_StringProcessing.dylib | `_$sSm17_StringProcessingSs11SubSequenceRtzrlE7replace_4with15maxReplacementsyqd_0__qd__SitSlRd__AA14RegexComponentRd_0_SJ7ElementRtd__r0_lF` | 3 | coteditor, easydict, squirrel-app |
| libswiftos.dylib | `_$sSo9OS_os_logC0B0E8CategoryV16pointsOfInterestAEvgZ` | 3 | fluidvoice, opensuperwhisper, superwhisper |
| CoreML.framework | `_$s6CoreML13MLShapedArrayVMn` | 2 | fluidvoice, openclaw |
| CoreML.framework | `_$s6CoreML13MLShapedArrayVyxGAA0cD8ProtocolAAMc` | 2 | fluidvoice, openclaw |
| CoreML.framework | `_$s6CoreML21MLShapedArrayProtocolPAAE7scalarsSay6ScalarQzGvg` | 2 | fluidvoice, openclaw |
| CoreML.framework | `_$sSo12MLMultiArrayC6CoreMLE30withUnsafeMutableBufferPointer6ofType_q_xm_q_SryxG_SaySiGtKXEtKAC08MLShapedB6ScalarRzr0_lF` | 2 | fluidvoice, voiceink |
| CoreML.framework | `_$sSo7MLModelC6CoreMLE10prediction4from7optionsSo17MLFeatureProvider_pSoAG_p_So19MLPredictionOptionsCtYaKF` | 2 | fluidvoice, voiceink |
| CoreML.framework | `_$sSo7MLModelC6CoreMLE10prediction4from7optionsSo17MLFeatureProvider_pSoAG_p_So19MLPredictionOptionsCtYaKFTu` | 2 | fluidvoice, voiceink |
| CoreML.framework | `_$sSo7MLModelC6CoreMLE12compileModel2at10Foundation3URLVAH_tYaKFZ` | 2 | fluidvoice, voiceink |
| CoreML.framework | `_$sSo7MLModelC6CoreMLE12compileModel2at10Foundation3URLVAH_tYaKFZTu` | 2 | fluidvoice, voiceink |
| CoreML.framework | `_$sSo7MLModelC6CoreMLE4load10contentsOf13configurationAB10Foundation3URLV_So0A13ConfigurationCtYaKFZ` | 2 | fluidvoice, voiceink |
| CoreML.framework | `_$sSo7MLModelC6CoreMLE4load10contentsOf13configurationAB10Foundation3URLV_So0A13ConfigurationCtYaKFZTu` | 2 | fluidvoice, voiceink |
| CoreML.framework | `_$ss5Int32V6CoreML19MLShapedArrayScalarACWP` | 2 | fluidvoice, openclaw |
| SwiftData.framework | `_$s9SwiftData10ModelActorMp` | 2 | bartender, voiceink |
| SwiftData.framework | `_$s9SwiftData10ModelActorP13modelExecutorAA0cF0_pvgTq` | 2 | bartender, voiceink |
| SwiftData.framework | `_$s9SwiftData10ModelActorP14modelContainerAA0cF0CvgTq` | 2 | bartender, voiceink |
| SwiftData.framework | `_$s9SwiftData10ModelActorPAAE15unownedExecutorScevg` | 2 | bartender, voiceink |
| SwiftData.framework | `_$s9SwiftData10ModelActorPScATb` | 2 | bartender, voiceink |
| SwiftData.framework | `_$s9SwiftData12ModelContextC10fetchCountySiAA15FetchDescriptorVyxGKAA010PersistentC0RzlFTj` | 2 | maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData12ModelContextC11transaction5blockyyyKXE_tKFTj` | 2 | maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData12ModelContextCMa` | 2 | bartender, voiceink |
| SwiftData.framework | `_$s9SwiftData12ModelContextCMn` | 2 | bartender, voiceink |
| SwiftData.framework | `_$s9SwiftData12ModelContextCyAcA0C9ContainerCcfc` | 2 | bartender, voiceink |
| SwiftData.framework | `_$s9SwiftData13ModelExecutorMp` | 2 | bartender, voiceink |
| SwiftData.framework | `_$s9SwiftData14ModelContainerC11mainContextAA0cF0CvgTj` | 2 | maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData14ModelContainerC3for13migrationPlan14configurationsAcA6SchemaC_AA0i9MigrationG0_pXpSgAA0C13ConfigurationVdtKcfC` | 2 | bartender, voiceink |
| SwiftData.framework | `_$s9SwiftData15PersistentModelPAAE2eeoiySbx_xtFZ` | 2 | maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData15PersistentModelPAAE2idAA0C10IdentifierVvg` | 2 | maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData15PersistentModelPAAE4hash4intoys6HasherVz_tF` | 2 | maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData18ModelConfigurationV16CloudKitDatabaseV9automaticAEvgZ` | 2 | maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData18ModelConfigurationV16CloudKitDatabaseVMa` | 2 | maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData18ModelConfigurationV_6schema3url10allowsSave16cloudKitDatabaseACSSSg_AA6SchemaCSg10Foundation3URLVSbAC05CloudjK0VtcfC` | 2 | maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData20PersistentIdentifierVMa` | 2 | maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData20PersistentIdentifierVMn` | 2 | maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData20PersistentIdentifierVSHAAMc` | 2 | maccy, voiceink |
| SwiftData.framework | `_$s9SwiftData26DefaultSerialModelExecutorC12modelContextAcA0eH0C_tcfc` | 2 | bartender, voiceink |
| SwiftData.framework | `_$s9SwiftData26DefaultSerialModelExecutorCAA0eF0AAMc` | 2 | bartender, voiceink |
| SwiftData.framework | `_$s9SwiftData26DefaultSerialModelExecutorCMa` | 2 | bartender, voiceink |
| SwiftData.framework | `_$s9SwiftData6SchemaC7VersionVMa` | 2 | bartender, voiceink |
| SwiftData.framework | `_$s9SwiftData6SchemaC7VersionVyAESi_S2itcfC` | 2 | bartender, voiceink |
| SwiftData.framework | `_$s9SwiftData6SchemaCMa` | 2 | bartender, voiceink |
| SwiftData.framework | `_$s9SwiftData6SchemaC_7versionACSayAA15PersistentModel_pXpG_AC7VersionVtcfc` | 2 | bartender, voiceink |
| Translation.framework | `_$s11Translation0A7SessionC07prepareA0yyYaKFTjTu` | 2 | macwhisper, markedit |
| Translation.framework | `_$s11Translation0A7SessionC8ResponseV10targetTextSSvg` | 2 | macwhisper, markedit |
| Translation.framework | `_$s11Translation0A7SessionC8ResponseVMa` | 2 | macwhisper, markedit |
| Translation.framework | `_$s11Translation0A7SessionC9translateyAC8ResponseVSSYaKFTjTu` | 2 | macwhisper, markedit |
| Translation.framework | `_$s11Translation0A7SessionCMa` | 2 | macwhisper, markedit |
| WeatherKit.framework | `_$s10WeatherKit03DayA0V14lowTemperature10Foundation11MeasurementVySo06NSUnitE0CGvg` | 2 | bartender, raycast |
| WeatherKit.framework | `_$s10WeatherKit03DayA0V15highTemperature10Foundation11MeasurementVySo06NSUnitE0CGvg` | 2 | bartender, raycast |
| WeatherKit.framework | `_$s10WeatherKit03DayA0V3sunAA9SunEventsVvg` | 2 | bartender, raycast |
| WeatherKit.framework | `_$s10WeatherKit03DayA0VMa` | 2 | bartender, raycast |
| WeatherKit.framework | `_$s10WeatherKit03DayA0VMn` | 2 | bartender, raycast |
| WeatherKit.framework | `_$s10WeatherKit03DayA0VSQAAMc` | 2 | bartender, raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0V19precipitationChanceSdvg` | 2 | bartender, raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0V4date10Foundation4DateVvg` | 2 | bartender, raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0VMa` | 2 | bartender, raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0VMn` | 2 | bartender, raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0VSQAAMc` | 2 | bartender, raycast |
| WeatherKit.framework | `_$s10WeatherKit07CurrentA0V10symbolNameSSvg` | 2 | bartender, raycast |
| WeatherKit.framework | `_$s10WeatherKit07CurrentA0V11temperature10Foundation11MeasurementVySo17NSUnitTemperatureCGvg` | 2 | bartender, raycast |
| WeatherKit.framework | `_$s10WeatherKit07CurrentA0V9conditionAA0A9ConditionOvg` | 2 | bartender, raycast |
| WeatherKit.framework | `_$s10WeatherKit07CurrentA0VMa` | 2 | bartender, raycast |
| WeatherKit.framework | `_$s10WeatherKit07CurrentA0VMn` | 2 | bartender, raycast |
| WeatherKit.framework | `_$s10WeatherKit0A5QueryV5dailyACyAA8ForecastVyAA03DayA0VGGvgZ` | 2 | bartender, raycast |
| WeatherKit.framework | `_$s10WeatherKit0A5QueryV6hourlyACyAA8ForecastVyAA04HourA0VGGvgZ` | 2 | bartender, raycast |
| WeatherKit.framework | `_$s10WeatherKit0A5QueryV7currentACyAA07CurrentA0VGvgZ` | 2 | bartender, raycast |
| WeatherKit.framework | `_$s10WeatherKit0A5QueryVMn` | 2 | bartender, raycast |
| WeatherKit.framework | `_$s10WeatherKit0A7ServiceCMa` | 2 | bartender, raycast |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO11descriptionSSvg` | 2 | bartender, raycast |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionOMa` | 2 | bartender, raycast |
| WeatherKit.framework | `_$s10WeatherKit7UVIndexVMa` | 2 | bartender, raycast |
| WeatherKit.framework | `_$s10WeatherKit8ForecastVMn` | 2 | bartender, raycast |
| WeatherKit.framework | `_$s10WeatherKit8ForecastVyxGSTAAMc` | 2 | bartender, raycast |
| WeatherKit.framework | `_$s10WeatherKit8ForecastVyxGSlAAMc` | 2 | bartender, raycast |
| WeatherKit.framework | `_$s10WeatherKit9SunEventsV6sunset10Foundation4DateVSgvg` | 2 | bartender, raycast |
| WeatherKit.framework | `_$s10WeatherKit9SunEventsVMa` | 2 | bartender, raycast |
| _AVKit_SwiftUI.framework | `_$s14_AVKit_SwiftUI11VideoPlayerVAA0bC09EmptyViewVRszrlE6playerACyAFGSo8AVPlayerCSg_tcfC` | 2 | openclaw, proxyman |
| _AVKit_SwiftUI.framework | `_$s14_AVKit_SwiftUI11VideoPlayerVMn` | 2 | openclaw, proxyman |
| _AppIntents_AppKit.framework | `_$s10AppIntents21DisplayRepresentationV5ImageV01_ab1_A3KitE10systemName9tintColor19symbolConfigurationAESgSS_So7NSColorCSgSo013NSImageSymbolL0CSgtcfC` | 2 | bartender, tailscale-app |
| libSystem.B.dylib | `__dispatch_main_queue_callback_4CF` | 2 | cloudflare-warp, telegram-desktop |
| libobjc.A.dylib | `_protocol_addMethodDescription` | 2 | bitwarden, orbstack |
| libobjc.A.dylib | `_protocol_copyPropertyList` | 2 | easydict, teamviewer |
| libswiftCore.dylib | `_$sSs17UnicodeScalarViewVMn` | 2 | opensuperwhisper, vorssaint |
| libswiftCoreML.dylib | `_$sSo12MLMultiArrayC6CoreMLE15withUnsafeBytesyxxSWKXEKlF` | 2 | macwhisper, superwhisper |
| libswiftCoreML.dylib | `_$sSo12MLMultiArrayC6CoreMLE22withUnsafeMutableBytesyxxSw_SaySiGtKXEKlF` | 2 | macwhisper, superwhisper |
| libswiftCoreML.dylib | `_$sSo12MLMultiArrayC6CoreMLEyABxcAC08MLShapedB8ProtocolRzlufC` | 2 | macwhisper, superwhisper |
| libswiftDispatch.dylib | `_$s8Dispatch0A12TimeIntervalOMa` | 2 | opensuperwhisper, vorssaint |
| libswiftDispatch.dylib | `_$s8Dispatch0A13WorkItemFlagsVMa` | 2 | opensuperwhisper, vorssaint |
| libswiftDispatch.dylib | `_$s8Dispatch0A13WorkItemFlagsVMn` | 2 | opensuperwhisper, vorssaint |
| libswiftDispatch.dylib | `_$s8Dispatch0A13WorkItemFlagsVs10SetAlgebraAAMc` | 2 | opensuperwhisper, vorssaint |
| libswiftDispatch.dylib | `_$s8Dispatch0A3QoSVMa` | 2 | opensuperwhisper, vorssaint |
| libswiftDispatch.dylib | `_$s8Dispatch0A4TimeVMa` | 2 | opensuperwhisper, vorssaint |
| libswiftDispatch.dylib | `_$s8Dispatch1poiyAA0A4TimeVAD_SdtF` | 2 | opensuperwhisper, vorssaint |
| libswiftDispatch.dylib | `_$sSo17OS_dispatch_groupC8DispatchE4waityyF` | 2 | opensuperwhisper, vorssaint |
| libswiftDispatch.dylib | `_$sSo17OS_dispatch_queueC8DispatchE10AttributesVMa` | 2 | opensuperwhisper, vorssaint |
| libswiftDispatch.dylib | `_$sSo17OS_dispatch_queueC8DispatchE10AttributesVMn` | 2 | opensuperwhisper, vorssaint |
| libswiftDispatch.dylib | `_$sSo17OS_dispatch_queueC8DispatchE10AttributesVs10SetAlgebraACMc` | 2 | opensuperwhisper, vorssaint |
| libswiftDispatch.dylib | `_$sSo17OS_dispatch_queueC8DispatchE20AutoreleaseFrequencyOMa` | 2 | opensuperwhisper, vorssaint |
| libswiftDispatch.dylib | `_$sSo17OS_dispatch_queueC8DispatchE4mainABvgZ` | 2 | opensuperwhisper, vorssaint |
| libswiftDispatch.dylib | `_$sSo17OS_dispatch_queueC8DispatchE5async5group3qos5flags7executeySo0a1_b1_F0CSg_AC0D3QoSVAC0D13WorkItemFlagsVyyXBtF` | 2 | opensuperwhisper, vorssaint |
| libswiftDispatch.dylib | `_$sSo18OS_dispatch_sourceC8DispatchE10TimerFlagsVMa` | 2 | opensuperwhisper, vorssaint |
| libswiftDispatch.dylib | `_$sSo18OS_dispatch_sourceC8DispatchE10TimerFlagsVMn` | 2 | opensuperwhisper, vorssaint |
| libswiftDispatch.dylib | `_$sSo18OS_dispatch_sourceC8DispatchE10TimerFlagsVs10SetAlgebraACMc` | 2 | opensuperwhisper, vorssaint |
| libswiftDispatch.dylib | `_$sSo18OS_dispatch_sourceP8DispatchE15setEventHandler3qos5flags7handleryAC0D3QoSV_AC0D13WorkItemFlagsVyyXBSgtF` | 2 | opensuperwhisper, vorssaint |
| libswiftDispatch.dylib | `_$sSo18OS_dispatch_sourceP8DispatchE6cancelyyF` | 2 | opensuperwhisper, vorssaint |
| libswiftDispatch.dylib | `_$sSo18OS_dispatch_sourceP8DispatchE6resumeyyF` | 2 | opensuperwhisper, vorssaint |
| libswiftDispatch.dylib | `_$sSo21OS_dispatch_semaphoreC8DispatchE4wait7timeoutAC0D13TimeoutResultOAC0D4TimeV_tF` | 2 | opensuperwhisper, vorssaint |
| libswiftDispatch.dylib | `_$sSo21OS_dispatch_semaphoreC8DispatchE6signalSiyF` | 2 | opensuperwhisper, vorssaint |
| libswiftNaturalLanguage.dylib | `_$sSo11NLTokenizerC15NaturalLanguageE10tokenRange2atSnySS5IndexVGAG_tF` | 2 | easydict, markedit |
| libswiftNaturalLanguage.dylib | `_$sSo8NLTaggerC15NaturalLanguageE03setC0_5rangeySo10NLLanguagea_SnySS5IndexVGtF` | 2 | openclaw, voiceink |
| libswiftNaturalLanguage.dylib | `_$sSo8NLTaggerC15NaturalLanguageE3tag2at4unit6schemeSo5NLTagaSg_SnySS5IndexVGtAL_So11NLTokenUnitVSo0H6SchemeatF` | 2 | fluidvoice, superwhisper |
| libswiftObservation.dylib | `_$s11Observation12ObservationsV17makeAsyncIteratorAC0E0Vyxq__GyF` | 2 | coteditor, thaw |
| libswiftObservation.dylib | `_$s11Observation12ObservationsV8IteratorV4next9isolationxSgScA_pSgYi_tYaq_YKF` | 2 | coteditor, thaw |
| libswiftObservation.dylib | `_$s11Observation12ObservationsV8IteratorV4next9isolationxSgScA_pSgYi_tYaq_YKFTu` | 2 | coteditor, thaw |
| libswiftObservation.dylib | `_$s11Observation12ObservationsV8IteratorVMn` | 2 | coteditor, thaw |
| libswiftObservation.dylib | `_$s11Observation12ObservationsVMn` | 2 | coteditor, thaw |
| libswiftObservation.dylib | `_$s11Observation12ObservationsVyACyxq_GxyYbq_YKYAccfC` | 2 | coteditor, thaw |
| libswiftObservation.dylib | `_$s11Observation12ObservationsVyxq_GSciAAMc` | 2 | coteditor, thaw |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder3OneVMn` | 2 | easydict, sfm |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder3OneVyACyxGqd__c0A6OutputQyd__Rsz17_StringProcessing0A9ComponentRd__lufC` | 2 | easydict, sfm |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder3OneVyxG17_StringProcessing0A9ComponentAAMc` | 2 | easydict, sfm |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder6AnchorV11startOfLineACvgZ` | 2 | bettertouchtool, easydict |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder6AnchorV17_StringProcessing0A9ComponentAAMc` | 2 | bettertouchtool, easydict |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder6AnchorVMa` | 2 | bettertouchtool, easydict |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder6RepeatVMn` | 2 | easydict, raycast |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder6RepeatVyACyxG17_StringProcessing0A0VyxGcfC` | 2 | easydict, raycast |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder6RepeatVyxG17_StringProcessing0A9ComponentAAMc` | 2 | easydict, raycast |
| libswiftRegexBuilder.dylib | `_$s17_StringProcessing14RegexComponentP0C7BuilderAD14CharacterClassVRszrlE10whitespaceAFvgZ` | 2 | easydict, sfm |
| libswiftRegexBuilder.dylib | `_$s17_StringProcessing14RegexComponentP0C7BuilderAD14CharacterClassVRszrlE4wordAFvgZ` | 2 | easydict, sfm |
| libswiftRegexBuilder.dylib | `_$s17_StringProcessing14RegexComponentP0C7BuilderAD14CharacterClassVRszrlE5digitAFvgZ` | 2 | easydict, raycast |
| libswiftSynchronization.dylib | `_$s15Synchronization5_CellVMa` | 2 | openclaw, thaw |
| libswiftSystem.dylib | `_$s6System8FilePathV14platformStringACSPys4Int8VG_tcfC` | 2 | easydict, orbstack |
| libswiftXPC.dylib | `_$s3XPC0A16_TYPE_CONNECTIONs13OpaquePointerVvg` | 2 | cloudflare-warp, orbstack |
| libswiftXPC.dylib | `_$s3XPC10XPCSessionC10xpcService11targetQueue7options19cancellationHandlerACSS_So17OS_dispatch_queueCSgAC21InitializationOptionsVyAA12XPCRichErrorVcSgtKcfC` | 2 | bartender, thaw |
| libswiftXPC.dylib | `_$s3XPC10XPCSessionC21InitializationOptionsV8inactiveAEvgZ` | 2 | bartender, thaw |
| libswiftXPC.dylib | `_$s3XPC10XPCSessionC21InitializationOptionsVMa` | 2 | bartender, thaw |
| libswiftXPC.dylib | `_$s3XPC10XPCSessionC4send_12replyHandleryx_ys6ResultOyAA18XPCReceivedMessageVAA12XPCRichErrorVGctKSERzlFTj` | 2 | bartender, thaw |
| libswiftXPC.dylib | `_$s3XPC10XPCSessionC6cancel6reasonySS_tFTj` | 2 | bartender, thaw |
| libswiftXPC.dylib | `_$s3XPC10XPCSessionC8activateyyKFTj` | 2 | bartender, thaw |
| libswiftXPC.dylib | `_$s3XPC10XPCSessionCMa` | 2 | bartender, thaw |
| libswiftXPC.dylib | `_$s3XPC10XPCSessionCMn` | 2 | bartender, thaw |
| libswiftXPC.dylib | `_$s3XPC11XPCListenerC21InitializationOptionsVMa` | 2 | bartender, thaw |
| libswiftXPC.dylib | `_$s3XPC11XPCListenerC22IncomingSessionRequestC6accept22incomingMessageHandler012cancellationI0AE8DecisionVSE_pSgAA011XPCReceivedH0Vc_yAA12XPCRichErrorVcSgtFTj` | 2 | bartender, thaw |
| libswiftXPC.dylib | `_$s3XPC11XPCListenerC6cancelyyFTj` | 2 | bartender, thaw |
| libswiftXPC.dylib | `_$s3XPC11XPCListenerC7service11targetQueue7options11requirement22incomingSessionHandlerACSS_So17OS_dispatch_queueCSgAC21InitializationOptionsVAA18XPCPeerRequirementVAC08IncomingI7RequestC8DecisionVAQctKcfC` | 2 | bartender, thaw |
| libswiftXPC.dylib | `_$s3XPC11XPCListenerCMa` | 2 | bartender, thaw |
| libswiftXPC.dylib | `_$s3XPC11XPCListenerCMn` | 2 | bartender, thaw |
| libswiftXPC.dylib | `_$s3XPC12XPCRichErrorVMa` | 2 | bartender, thaw |
| libswiftXPC.dylib | `_$s3XPC12XPCRichErrorVMn` | 2 | bartender, thaw |
| libswiftXPC.dylib | `_$s3XPC12XPCRichErrorVs0C0AAMc` | 2 | bartender, thaw |
| libswiftXPC.dylib | `_$s3XPC18XPCPeerRequirementV14isFromSameTeam27andMatchesSigningIdentifierACSSSg_tFZ` | 2 | bartender, thaw |
| libswiftXPC.dylib | `_$s3XPC18XPCPeerRequirementVMa` | 2 | bartender, thaw |
| libswiftXPC.dylib | `_$s3XPC18XPCReceivedMessageV6decode2asxxm_tKSeRzlF` | 2 | bartender, thaw |
| libswiftXPC.dylib | `_$s3XPC18XPCReceivedMessageVMn` | 2 | bartender, thaw |
| libswift_Concurrency.dylib | `_$sScA15unownedExecutorScevgTj` | 2 | opensuperwhisper, vorssaint |
| libswift_Concurrency.dylib | `_$sScP8rawValues5UInt8Vvg` | 2 | opensuperwhisper, vorssaint |
| libswift_Concurrency.dylib | `_$sScPMa` | 2 | opensuperwhisper, vorssaint |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing14AnyRegexOutputV7ElementVMa` | 2 | bettertouchtool, openclaw |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV10wholeMatch2inAC0E0Vyx_GSgSS_tKF` | 2 | coteditor, squirrel-app |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV5MatchV13dynamicMemberxs7KeyPathCyx_yt9_doNotUsetxG_tcig` | 2 | easydict, sfm |
| libswift_StringProcessing.dylib | `_$sSK17_StringProcessingSs11SubSequenceRtzrlE10firstRange2ofSnySS5IndexVGSgqd___tAA14RegexComponentRd__lF` | 2 | coteditor, supacode |
| libswift_StringProcessing.dylib | `_$sSK17_StringProcessingSs11SubSequenceRtzrlE6ranges2ofSaySnySS5IndexVGGqd___tAA14RegexComponentRd__lF` | 2 | bettertouchtool, squirrel-app |
| libswift_StringProcessing.dylib | `_$sSm17_StringProcessingSs11SubSequenceRtzrlE7replace_15maxReplacements4withyqd_1__Siqd_0_AA5RegexV5MatchVyqd___GKXEtK0I6OutputQyd_1_Rsd__SlRd_0_AA0I9ComponentRd_1_SJ7ElementRtd_0_r1_lF` | 2 | bettertouchtool, easydict |
| libswiftos.dylib | `_$s2os12OSSignpostIDV3log6objectACSo03OS_a1_D0C_yXltcfC` | 2 | bartender, raycast |
| libswiftos.dylib | `_$s2os12OSSignpostIDV7invalidACvgZ` | 2 | bettertouchtool, utm |
| libswiftos.dylib | `_$sSo9OS_os_logC0B0E8disabledABvgZ` | 2 | chatgpt, clipy |
| CoreML.framework | `_$s6CoreML13MLShapedArrayV5shapeSaySiGvg` | 1 | openclaw |
| CoreML.framework | `_$s6CoreML21MLShapedArrayProtocolPAAE7scalarsSay6ScalarQzGvM` | 1 | openclaw |
| CoreML.framework | `_$sSo12MLMultiArrayC6CoreMLE15withUnsafeBytesyxxSWKXEKlF` | 1 | voiceink |
| CoreML.framework | `_$sSo12MLMultiArrayC6CoreMLE22withUnsafeMutableBytesyxxSw_SaySiGtKXEKlF` | 1 | voiceink |
| CoreML.framework | `_$sSo12MLMultiArrayC6CoreMLE23withUnsafeBufferPointer6ofType_q_xm_q_SRyxGKXEtKAC08MLShapedB6ScalarRzr0_lF` | 1 | voiceink |
| CoreML.framework | `_$sSo14MLFeatureValueC6CoreMLE011shapedArrayB02ofAC08MLShapedF0VyxGSgxm_tAC0hF6ScalarRzlF` | 1 | fluidvoice |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA5PointV10IdentifierVMa` | 1 | fuse-t |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA5PointV10IdentifierVyAEs12StaticStringVcfC` | 1 | fuse-t |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA5PointV10identifierACs12StaticStringV_tKcfC` | 1 | fuse-t |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA5PointV4BindV10buildBlockyA2C10IdentifierVFZ` | 1 | fuse-t |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA5PointV7MonitorC03appaD0AeC_tYaKcfC` | 1 | fuse-t |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA5PointV7MonitorC03appaD0AeC_tYaKcfCTu` | 1 | fuse-t |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA5PointV7MonitorCMa` | 1 | fuse-t |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA5PointV7MonitorCMn` | 1 | fuse-t |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA5PointVMa` | 1 | fuse-t |
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
| FSKit.framework | `_$s10FileSystem5FSKit05UnaryaB9ExtensionPTl` | 1 | fuse-t |
| FSKit.framework | `_$s5FSKit24UnaryFileSystemExtensionMp` | 1 | fuse-t |
| FSKit.framework | `_$s5FSKit24UnaryFileSystemExtensionP04fileD00cD0QzvgTq` | 1 | fuse-t |
| FSKit.framework | `_$s5FSKit24UnaryFileSystemExtensionP0E10Foundation03AppE0Tb` | 1 | fuse-t |
| FSKit.framework | `_$s5FSKit24UnaryFileSystemExtensionPAAE13configurationQrvg` | 1 | fuse-t |
| FSKit.framework | `_$s5FSKit24UnaryFileSystemExtensionPAAE13configurationQrvpQOMQ` | 1 | fuse-t |
| FoundationModels.framework | `_$s16FoundationModels17GenerationOptionsV12SamplingModeV6greedyAEvgZ` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels17GenerationOptionsV12SamplingModeV6random20probabilityThreshold4seedAESd_s6UInt64VSgtFZ` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels17GenerationOptionsV12SamplingModeV6random3top4seedAESi_s6UInt64VSgtFZ` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels17GenerationOptionsV12SamplingModeVMa` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels17GenerationOptionsV12SamplingModeVMn` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels17GenerationOptionsVMa` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels19SystemLanguageModelC11isAvailableSbvg` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels19SystemLanguageModelC12AvailabilityO17UnavailableReasonO13modelNotReadyyA2GmFWC` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels19SystemLanguageModelC12AvailabilityO17UnavailableReasonO17deviceNotEligibleyA2GmFWC` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels19SystemLanguageModelC12AvailabilityO17UnavailableReasonO27appleIntelligenceNotEnabledyA2GmFWC` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels19SystemLanguageModelC12AvailabilityO17UnavailableReasonOMa` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels19SystemLanguageModelC12AvailabilityOMa` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels19SystemLanguageModelC12availabilityAC12AvailabilityOvg` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels19SystemLanguageModelC7defaultACvgZ` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels19SystemLanguageModelCMa` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels20LanguageModelSessionC12isRespondingSbvg` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels20LanguageModelSessionC14ResponseStreamV13AsyncIteratorV4next9isolationAE8SnapshotVy_x_GSgScA_pSgYi_tYaKF` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels20LanguageModelSessionC14ResponseStreamV13AsyncIteratorV4next9isolationAE8SnapshotVy_x_GSgScA_pSgYi_tYaKFTu` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels20LanguageModelSessionC14ResponseStreamV13AsyncIteratorVMn` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels20LanguageModelSessionC14ResponseStreamV17makeAsyncIteratorAE0iJ0Vy_x_GyF` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels20LanguageModelSessionC14ResponseStreamV7collectAC0F0Vy_xGyYaKF` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels20LanguageModelSessionC14ResponseStreamV7collectAC0F0Vy_xGyYaKFTu` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels20LanguageModelSessionC14ResponseStreamV8SnapshotV7content18PartiallyGeneratedQzvg` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels20LanguageModelSessionC14ResponseStreamV8SnapshotVMn` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels20LanguageModelSessionC14ResponseStreamVMn` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels20LanguageModelSessionC14streamResponse7options6promptAC0G6StreamVy_SSGAA17GenerationOptionsV_AA6PromptVyKXEtKF` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels20LanguageModelSessionC5model5tools12instructionsAcA06SystemcD0C_SayAA4Tool_pGSSSgtcfC` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels20LanguageModelSessionC7respond7options6promptAC8ResponseVy_SSGAA17GenerationOptionsV_AA6PromptVyKXEtYaKF` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels20LanguageModelSessionC7respond7options6promptAC8ResponseVy_SSGAA17GenerationOptionsV_AA6PromptVyKXEtYaKFTu` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels20LanguageModelSessionC8ResponseV7contentxvg` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels20LanguageModelSessionC8ResponseVMn` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels20LanguageModelSessionCMa` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels20LanguageModelSessionCMn` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels6PromptVAA0C13RepresentableAAWP` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels6PromptVMa` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels6PromptVMn` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels6PromptVyACxcAA0C13RepresentableRzlufC` | 1 | markedit |
| FoundationModels.framework | `_$s16FoundationModels6PromptVyACxxQpcRvzAA0C13RepresentableRzlufC` | 1 | markedit |
| FoundationModels.framework | `_$sSS16FoundationModels19PromptRepresentableAAWP` | 1 | markedit |
| FoundationModels.framework | `_$sSayxG16FoundationModels19PromptRepresentableA2bCRzlMc` | 1 | markedit |
| LightweightCodeRequirements.framework | `_$s27LightweightCodeRequirements07ProcessB11RequirementV5anyOf11requirementACSayAA0D10Constraint_pGyXE_tKFZ` | 1 | macwhisper |
| LightweightCodeRequirements.framework | `_$s27LightweightCodeRequirements07ProcessB11RequirementVMa` | 1 | macwhisper |
| LightweightCodeRequirements.framework | `_$s27LightweightCodeRequirements14TeamIdentifierVAA17ProcessConstraintAAMc` | 1 | macwhisper |
| LightweightCodeRequirements.framework | `_$s27LightweightCodeRequirements14TeamIdentifierVMa` | 1 | macwhisper |
| LightweightCodeRequirements.framework | `_$s27LightweightCodeRequirements14TeamIdentifierVyACSScfC` | 1 | macwhisper |
| LightweightCodeRequirements.framework | `_$s27LightweightCodeRequirements17ProcessConstraintMp` | 1 | macwhisper |
| LightweightCodeRequirements.framework | `_$s27LightweightCodeRequirements24ProcessConstraintBuilderV10buildBlockySayAA0dE0_pGAFd_tFZ` | 1 | macwhisper |
| LightweightCodeRequirements.framework | `_$s27LightweightCodeRequirements24ProcessConstraintBuilderV15buildExpressionySayAA0dE0_pGAaE_pFZ` | 1 | macwhisper |
| LightweightCodeRequirements.framework | `_$s27LightweightCodeRequirements29SecTaskValidateForRequirement4task11requirementSbSo0dE3Refa_AA07ProcessbH0VtKF` | 1 | macwhisper |
| SwiftData.framework | `_$s9SwiftData07BackingB0P8setValue6forKey2toys0G4PathCy5ModelQzqd__G_qd__tAA22RelationshipCollectionRd__17PersistentElementQyd__Rsd_0_r0_lFTj` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData07BackingB0P8setValue6forKey2toys0G4PathCy5ModelQzqd__SgG_AKtAA010PersistentJ0Rd__lFTj` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData10ModelActorPAAE12modelContextAA0cF0Cvg` | 1 | voiceink |
| SwiftData.framework | `_$s9SwiftData12ModelContextC21processPendingChangesyyFTj` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData12ModelContextC6delete5model5where17includeSubclassesyxm_10Foundation9PredicateVyx_QPGSgSbtKAA010PersistentC0RzlFTj` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData12ModelContextC8rollbackyyFTj` | 1 | voiceink |
| SwiftData.framework | `_$s9SwiftData12ModelContextC9containerAA0C9ContainerCvgTj` | 1 | voiceink |
| SwiftData.framework | `_$s9SwiftData14ModelContainerC3for13migrationPlan14configurationsAcA010PersistentC0_pXpd_AA015SchemaMigrationG0_pXpSgAA0C13ConfigurationVdtKcfC` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData15FetchDescriptorV012propertiesToC0Says14PartialKeyPathCyxGGvs` | 1 | voiceink |
| SwiftData.framework | `_$s9SwiftData15FetchDescriptorV10fetchLimitSiSgvs` | 1 | voiceink |
| SwiftData.framework | `_$s9SwiftData15FetchDescriptorV11fetchOffsetSiSgvs` | 1 | voiceink |
| SwiftData.framework | `_$s9SwiftData15FetchDescriptorV6sortBySay10Foundation04SortD0VyxGGvs` | 1 | bartender |
| SwiftData.framework | `_$s9SwiftData15FetchDescriptorV9predicate10Foundation9PredicateVyx_QPGSgvs` | 1 | voiceink |
| SwiftData.framework | `_$s9SwiftData15PersistentModelPAAE010persistentD2IDAA0C10IdentifierVvg` | 1 | voiceink |
| SwiftData.framework | `_$s9SwiftData15PersistentModelPAAE010persistentD2IDAA0C10IdentifierVvpMV` | 1 | voiceink |
| SwiftData.framework | `_$s9SwiftData15PersistentModelPAAE8getValue6forKeyqd__Sgs0H4PathCyxAFG_tAaBRd__lF` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData15PersistentModelPAAE8getValue6forKeyqd__s0H4PathCyxqd__G_tAA22RelationshipCollectionRd__0C7ElementQyd__Rsd_0_r0_lF` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData15PersistentModelPAAE8setValue6forKey2toys0H4PathCyxqd__G_qd__tAA22RelationshipCollectionRd__0C7ElementQyd__Rsd_0_r0_lF` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData15PersistentModelPAAE8setValue6forKey2toys0H4PathCyxqd__SgG_AItAaBRd__lF` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData18ModelConfigurationV14GroupContainerV9automaticAEvgZ` | 1 | voiceink |
| SwiftData.framework | `_$s9SwiftData18ModelConfigurationV14GroupContainerVMa` | 1 | voiceink |
| SwiftData.framework | `_$s9SwiftData18ModelConfigurationV16CloudKitDatabaseV4noneAEvgZ` | 1 | voiceink |
| SwiftData.framework | `_$s9SwiftData18ModelConfigurationV16CloudKitDatabaseV7privateyAESSFZ` | 1 | voiceink |
| SwiftData.framework | `_$s9SwiftData18ModelConfigurationV20isStoredInMemoryOnlyACSb_tcfC` | 1 | bartender |
| SwiftData.framework | `_$s9SwiftData18ModelConfigurationV_6schema20isStoredInMemoryOnly10allowsSave14groupContainer16cloudKitDatabaseACSSSg_AA6SchemaCSgS2bAC05GroupN0VAC05CloudpQ0VtcfC` | 1 | voiceink |
| SwiftData.framework | `_$s9SwiftData20PersistentIdentifierV2eeoiySbAC_ACtFZ` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData20PersistentIdentifierVSQAAMc` | 1 | voiceink |
| SwiftData.framework | `_$s9SwiftData6SchemaC12RelationshipC10DeleteRuleO7cascadeyA2GmFWC` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData6SchemaC12RelationshipC10DeleteRuleO7nullifyyA2GmFWC` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData6SchemaC12RelationshipC10DeleteRuleOMa` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData6SchemaC12RelationshipCAA0C8PropertyAAMc` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData6SchemaC12RelationshipCMa` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData6SchemaC12RelationshipC_10deleteRule17minimumModelCount07maximumhI012originalName7inverse12hashModifierA2E6OptionVd_AE06DeleteF0OSiSgAPSSSgs10AnyKeyPathCSgAQtcfc` | 1 | maccy |
| SwiftData.framework | `_$s9SwiftData6SchemaC9AttributeCAA0C8PropertyAAMc` | 1 | voiceink |
| SwiftData.framework | `_$s9SwiftData6SchemaC9AttributeCMa` | 1 | voiceink |
| SwiftData.framework | `_$s9SwiftData6SchemaC9AttributeC_12originalName12hashModifierA2E6OptionVd_SSSgAJtcfc` | 1 | voiceink |
| SwiftData.framework | `_$sSayxG9SwiftData22RelationshipCollectionA2B15PersistentModelRzlMc` | 1 | maccy |
| TabularData.framework | `_$s11TabularData6ColumnV010eraseToAnyC0AA0fC0VyF` | 1 | tableplus |
| TabularData.framework | `_$s11TabularData6ColumnV4name8contentsACyxGSS_qd__tcSTRd__xSg7ElementRtd__lufC` | 1 | tableplus |
| TabularData.framework | `_$s11TabularData6ColumnVMa` | 1 | tableplus |
| TipKit.framework | `_$s6TipKit03AnyA0VMn` | 1 | macwhisper |
| TipKit.framework | `_$s6TipKit0A0Mp` | 1 | macwhisper |
| TipKit.framework | `_$s6TipKit0A0P2idSSvgTq` | 1 | macwhisper |
| TipKit.framework | `_$s6TipKit0A0P5image7SwiftUI5ImageVSgvgTq` | 1 | macwhisper |
| TipKit.framework | `_$s6TipKit0A0P5rulesSayAA4TipsO4RuleVGvgTq` | 1 | macwhisper |
| TipKit.framework | `_$s6TipKit0A0P5title7SwiftUI4TextVvgTq` | 1 | macwhisper |
| TipKit.framework | `_$s6TipKit0A0P7actionsSayAA4TipsO6ActionVGvgTq` | 1 | macwhisper |
| TipKit.framework | `_$s6TipKit0A0P7message7SwiftUI4TextVSgvgTq` | 1 | macwhisper |
| TipKit.framework | `_$s6TipKit0A0P7optionsSayAA0A6Option_pGvgTq` | 1 | macwhisper |
| TipKit.framework | `_$s6TipKit0A0PAAE10invalidate6reasonyAA4TipsO18InvalidationReasonO_tF` | 1 | macwhisper |
| TipKit.framework | `_$s6TipKit0A0PAAE2idSSvg` | 1 | macwhisper |
| TipKit.framework | `_$s6TipKit0A0PAAE5image7SwiftUI5ImageVSgvg` | 1 | macwhisper |
| TipKit.framework | `_$s6TipKit0A0PAAE5rulesSayAA4TipsO4RuleVGvg` | 1 | macwhisper |
| TipKit.framework | `_$s6TipKit0A0PAAE7actionsSayAA4TipsO6ActionVGvg` | 1 | macwhisper |
| TipKit.framework | `_$s6TipKit0A0PAAE7optionsSayAA0A6Option_pGvg` | 1 | macwhisper |
| TipKit.framework | `_$s6TipKit0A0Ps12IdentifiableTb` | 1 | macwhisper |
| TipKit.framework | `_$s6TipKit0A4ViewVMn` | 1 | macwhisper |
| TipKit.framework | `_$s6TipKit0A4ViewV_9arrowEdge6actionACyAA03AnyA0VGAA0A0_pSg_7SwiftUI0E0OSgyAA4TipsO6ActionVctcAGRszrlufC` | 1 | macwhisper |
| TipKit.framework | `_$s6TipKit4TipsO18InvalidationReasonO15actionPerformedyA2EmFWC` | 1 | macwhisper |
| TipKit.framework | `_$s6TipKit4TipsO18InvalidationReasonOMa` | 1 | macwhisper |
| TipKit.framework | `_$s6TipKit4TipsO9configureyySayAC19ConfigurationOptionVGKFZ` | 1 | macwhisper |
| Translation.framework | `_$s11Translation0A5ErrorV08internalB0ACvgZ` | 1 | macwhisper |
| Translation.framework | `_$s11Translation0A5ErrorV18nothingToTranslateACvgZ` | 1 | macwhisper |
| Translation.framework | `_$s11Translation0A5ErrorV25unsupportedSourceLanguageACvgZ` | 1 | macwhisper |
| Translation.framework | `_$s11Translation0A5ErrorV25unsupportedTargetLanguageACvgZ` | 1 | macwhisper |
| Translation.framework | `_$s11Translation0A5ErrorV26unsupportedLanguagePairingACvgZ` | 1 | macwhisper |
| Translation.framework | `_$s11Translation0A5ErrorV2teoiySbAC_s0B0_ptFZ` | 1 | macwhisper |
| Translation.framework | `_$s11Translation0A5ErrorVMa` | 1 | macwhisper |
| Translation.framework | `_$s11Translation0A5ErrorVs0B0AAMc` | 1 | macwhisper |
| Translation.framework | `_$s11Translation0A7SessionC12translations4fromSayAC8ResponseVGSayAC7RequestVG_tYaKFTjTu` | 1 | macwhisper |
| Translation.framework | `_$s11Translation0A7SessionC13ConfigurationV10invalidateyyF` | 1 | macwhisper |
| Translation.framework | `_$s11Translation0A7SessionC13ConfigurationV6source10Foundation6LocaleV8LanguageVSgvg` | 1 | macwhisper |
| Translation.framework | `_$s11Translation0A7SessionC13ConfigurationV6source6targetAE10Foundation6LocaleV8LanguageVSg_AMtcfC` | 1 | macwhisper |
| Translation.framework | `_$s11Translation0A7SessionC13ConfigurationV6target10Foundation6LocaleV8LanguageVSgvg` | 1 | macwhisper |
| Translation.framework | `_$s11Translation0A7SessionC13ConfigurationVMa` | 1 | macwhisper |
| Translation.framework | `_$s11Translation0A7SessionC13ConfigurationVMn` | 1 | macwhisper |
| Translation.framework | `_$s11Translation0A7SessionC14targetLanguage10Foundation6LocaleV0D0VSgvg` | 1 | macwhisper |
| Translation.framework | `_$s11Translation0A7SessionC15installedSource6targetAC10Foundation6LocaleV8LanguageV_AJSgtcfC` | 1 | markedit |
| Translation.framework | `_$s11Translation0A7SessionC7RequestV10sourceText16clientIdentifierAESS_SSSgtcfC` | 1 | macwhisper |
| Translation.framework | `_$s11Translation0A7SessionC7RequestVMa` | 1 | macwhisper |
| Translation.framework | `_$s11Translation0A7SessionC7RequestVMn` | 1 | macwhisper |
| Translation.framework | `_$s11Translation20LanguageAvailabilityC18supportedLanguagesSay10Foundation6LocaleV0B0VGvgTjTu` | 1 | macwhisper |
| Translation.framework | `_$s11Translation20LanguageAvailabilityC6StatusO11unsupportedyA2EmFWC` | 1 | macwhisper |
| Translation.framework | `_$s11Translation20LanguageAvailabilityC6StatusO9installedyA2EmFWC` | 1 | macwhisper |
| Translation.framework | `_$s11Translation20LanguageAvailabilityC6StatusOMa` | 1 | macwhisper |
| Translation.framework | `_$s11Translation20LanguageAvailabilityC6StatusOSQAAMc` | 1 | macwhisper |
| Translation.framework | `_$s11Translation20LanguageAvailabilityC6status4from2toAC6StatusO10Foundation6LocaleV0B0V_AMSgtYaFTjTu` | 1 | macwhisper |
| Translation.framework | `_$s11Translation20LanguageAvailabilityCACycfc` | 1 | macwhisper |
| Translation.framework | `_$s11Translation20LanguageAvailabilityCMa` | 1 | macwhisper |
| Virtualization.framework | `_$sSo16VZVirtualMachineC14VirtualizationE5pause17completionHandleryys6ResultOyyts5Error_pGc_tF` | 1 | orbstack |
| Virtualization.framework | `_$sSo16VZVirtualMachineC14VirtualizationE5start17completionHandleryys6ResultOyyts5Error_pGc_tF` | 1 | orbstack |
| Virtualization.framework | `_$sSo16VZVirtualMachineC14VirtualizationE6resume17completionHandleryys6ResultOyyts5Error_pGc_tF` | 1 | orbstack |
| Virtualization.framework | `_$sSo20VZVirtioSocketDeviceC14VirtualizationE7connect6toPort17completionHandlerys6UInt32V_ys6ResultOySo0aB10ConnectionCs5Error_pGctF` | 1 | orbstack |
| WeatherKit.framework | `_$s10WeatherKit03DayA0V10symbolNameSSvg` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit03DayA0V14rainfallAmount10Foundation11MeasurementVySo12NSUnitLengthCGvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit03DayA0V14snowfallAmount10Foundation11MeasurementVySo12NSUnitLengthCGvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit03DayA0V19precipitationChanceSdvg` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit03DayA0V4date10Foundation4DateVvg` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit03DayA0V4moonAA10MoonEventsVvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit03DayA0V9conditionAA0A9ConditionOvg` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit03DayA0VSEAAMc` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit03DayA0VSeAAMc` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0V10cloudCoverSdvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0V10symbolNameSSvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0V10visibility10Foundation11MeasurementVySo12NSUnitLengthCGvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0V11temperature10Foundation11MeasurementVySo17NSUnitTemperatureCGvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0V13pressureTrendAA08PressureE0Ovg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0V19apparentTemperature10Foundation11MeasurementVySo06NSUnitE0CGvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0V4windAA4WindVvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0V7uvIndexAA7UVIndexVvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0V8dewPoint10Foundation11MeasurementVySo17NSUnitTemperatureCGvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0V8humiditySdvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0V8pressure10Foundation11MeasurementVySo14NSUnitPressureCGvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0V9conditionAA0A9ConditionOvg` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit04HourA0VSEAAMc` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit04HourA0VSeAAMc` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit07CurrentA0V10cloudCoverSdvg` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit07CurrentA0V7uvIndexAA7UVIndexVvg` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit07CurrentA0V8humiditySdvg` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit07CurrentA0V8metadataAA0A8MetadataVvg` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit07CurrentA0V8pressure10Foundation11MeasurementVySo14NSUnitPressureCGvg` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit07CurrentA0VSEAAMc` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit07CurrentA0VSQAAMc` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A0V07currentA0AA07CurrentA0Vvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit0A0V13dailyForecastAA0D0VyAA03DayA0VGvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit0A0V14hourlyForecastAA0D0VyAA04HourA0VGvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit0A0VMa` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit0A7ServiceC6sharedACvgZ` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A7ServiceC7weather3for9including__x_q_q0_tSo10CLLocationC_AA0A5QueryVyxGAJyq_GAJyq0_GtYaKr1_lF` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A7ServiceC7weather3for9including__x_q_q0_tSo10CLLocationC_AA0A5QueryVyxGAJyq_GAJyq0_GtYaKr1_lFTu` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A7ServiceC7weather3for9includingxSo10CLLocationC_AA0A5QueryVyxGtYaKlF` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit0A7ServiceC7weather3for9includingxSo10CLLocationC_AA0A5QueryVyxGtYaKlFTu` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit0A7ServiceC7weather3forAA0A0VSo10CLLocationC_tYaKF` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit0A7ServiceC7weather3forAA0A0VSo10CLLocationC_tYaKFTu` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit0A7ServiceCACycfC` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit0A7ServiceCMn` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A8MetadataV8locationSo10CLLocationCvg` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A8MetadataVMa` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO10sunShowersyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO11blowingDustyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO11blowingSnowyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO11mostlyClearyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO11sunFlurriesyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO12freezingRainyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO12mostlyCloudyyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO12partlyCloudyyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO12strongStormsyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO13thunderstormsyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO13tropicalStormyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO15freezingDrizzleyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO21isolatedThunderstormsyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO22scatteredThunderstormsyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO3hotyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO4hailyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO4hazeyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO4rainyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO4snowyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO5clearyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO5foggyyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO5sleetyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO5smokyyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO5windyyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO6breezyyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO6cloudyyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO6frigidyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO7drizzleyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO8blizzardyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO8flurriesyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO9heavyRainyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO9heavySnowyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO9hurricaneyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionO9wintryMixyA2CmFWC` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionOMn` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit0A9ConditionOSQAAMc` | 1 | bartender |
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
| WeatherKit.framework | `_$s10WeatherKit7UVIndexV5valueSivg` | 1 | bartender |
| WeatherKit.framework | `_$s10WeatherKit7UVIndexV8categoryAC16ExposureCategoryOvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit8ForecastV8forecastSayxGvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit8ForecastVMa` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit8ForecastVyxGSEAAMc` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit9MoonPhaseO11descriptionSSvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit9SunEventsV7sunrise10Foundation4DateVSgvg` | 1 | raycast |
| WeatherKit.framework | `_$s10WeatherKit9SunEventsVMn` | 1 | bartender |
| _AVKit_SwiftUI.framework | `_$s14_AVKit_SwiftUI11VideoPlayerVyxG0bC04ViewAAMc` | 1 | proxyman |
| _SwiftData_SwiftUI.framework | `_$s011_SwiftData_A2UI5QueryV12wrappedValueq_vg` | 1 | voiceink |
| _SwiftData_SwiftUI.framework | `_$s011_SwiftData_A2UI5QueryV6filter4sort11transactionACyxSayxGG10Foundation9PredicateVyx_QPGSg_SayAI14SortDescriptorVyxGG0aC011TransactionVSgtcAGRs_rlufC` | 1 | voiceink |
| _SwiftData_SwiftUI.framework | `_$s011_SwiftData_A2UI5QueryVMa` | 1 | voiceink |
| _SwiftData_SwiftUI.framework | `_$s011_SwiftData_A2UI5QueryVMn` | 1 | voiceink |
| _SwiftData_SwiftUI.framework | `_$s011_SwiftData_A2UI5QueryV_11transactionACyxSayxGG0aB015FetchDescriptorVyxG_0aC011TransactionVSgtcAERs_rlufC` | 1 | voiceink |
| libSystem.B.dylib | `__dispatch_get_main_queue_port_4CF` | 1 | cloudflare-warp |
| libSystem.B.dylib | `_dispatch_source_get_extended_data` | 1 | microsoft-teams |
| libobjc.A.dylib | `_objc_collectingEnabled` | 1 | android-file-transfer |
| libobjc.A.dylib | `_objc_registerThreadWithCollector` | 1 | android-file-transfer |
| libswiftCompression.dylib | `_$s11Compression11InputFilterC8readData8ofLength10Foundation0E0VSgSi_tKFTj` | 1 | bartender |
| libswiftCompression.dylib | `_$s11Compression11InputFilterCMn` | 1 | bartender |
| libswiftCompression.dylib | `_$s11Compression11InputFilterC_5using14bufferCapacity11readingFromACyxGAA0C9OperationO_AA9AlgorithmOSixSgSiKctKcfc` | 1 | bartender |
| libswiftCompression.dylib | `_$s11Compression9AlgorithmO8rawValueACSgSo21compression_algorithma_tcfC` | 1 | bartender |
| libswiftCompression.dylib | `_$s11Compression9AlgorithmOMn` | 1 | bartender |
| libswiftCompression.dylib | `_$s11Compression9AlgorithmOSQAAMc` | 1 | bartender |
| libswiftCore.dylib | `_$sSNyxGSksSxRzSZ6StrideRpzrlMc` | 1 | vorssaint |
| libswiftCore.dylib | `_$sSR7indicesSnySiGvg` | 1 | opensuperwhisper |
| libswiftCore.dylib | `_$sSR8endIndexSivg` | 1 | opensuperwhisper |
| libswiftCore.dylib | `_$sSS8IteratorVMn` | 1 | opensuperwhisper |
| libswiftCore.dylib | `_$sSSs26ExpressibleByStringLiteralsWP` | 1 | opensuperwhisper |
| libswiftCore.dylib | `_$sSdSxsWP` | 1 | vorssaint |
| libswiftCore.dylib | `_$sSf5write2toyxz_ts16TextOutputStreamRzlF` | 1 | opensuperwhisper |
| libswiftCore.dylib | `_$sSh7isEmptySbvpMV` | 1 | vorssaint |
| libswiftCore.dylib | `_$sSlsE5countSivpMV` | 1 | vorssaint |
| libswiftCore.dylib | `_$sSm17_customRemoveLast7ElementQzSgyFTj` | 1 | vorssaint |
| libswiftCore.dylib | `_$ss10SetAlgebraP16formIntersectionyyxFTj` | 1 | vorssaint |
| libswiftCore.dylib | `_$ss10SetAlgebraP19symmetricDifferenceyxxnFTj` | 1 | vorssaint |
| libswiftCore.dylib | `_$ss10SetAlgebraP7isEmptySbvgTj` | 1 | vorssaint |
| libswiftCore.dylib | `_$ss10SetAlgebraP8isSubset2ofSbx_tFTj` | 1 | vorssaint |
| libswiftCore.dylib | `_$ss10SetAlgebraP8subtractyyxFTj` | 1 | vorssaint |
| libswiftCore.dylib | `_$ss12_IteratorBoxCMn` | 1 | opensuperwhisper |
| libswiftCore.dylib | `_$ss14PrefixSequenceV12makeIteratorAB0D0Vyx_GyF` | 1 | opensuperwhisper |
| libswiftCore.dylib | `_$ss14PrefixSequenceV8IteratorV4next7ElementQzSgyF` | 1 | opensuperwhisper |
| libswiftCore.dylib | `_$ss14PrefixSequenceV8IteratorVMa` | 1 | opensuperwhisper |
| libswiftCore.dylib | `_$ss14PrefixSequenceVMa` | 1 | opensuperwhisper |
| libswiftCore.dylib | `_$ss18ReversedCollectionVyxGSksSkRzrlMc` | 1 | vorssaint |
| libswiftCore.dylib | `_$ss21_ClosureBasedIteratorVMn` | 1 | opensuperwhisper |
| libswiftCore.dylib | `_$ss24UnkeyedDecodingContainerP6decodeyS2umKFTj` | 1 | opensuperwhisper |
| libswiftCore.dylib | `_$ss24UnkeyedEncodingContainerP6encodeyySuKFTj` | 1 | opensuperwhisper |
| libswiftCore.dylib | `_$ss28SingleValueDecodingContainerP6decodeyS2fmKFTj` | 1 | opensuperwhisper |
| libswiftCore.dylib | `_$ss28SingleValueEncodingContainerP6encodeyySfKFTj` | 1 | opensuperwhisper |
| libswiftCore.dylib | `_$ss7Float16VMn` | 1 | opensuperwhisper |
| libswiftCore.dylib | `_$ss7Float16VN` | 1 | opensuperwhisper |
| libswiftCore.dylib | `_$ss7UnicodeO15GeneralCategoryO10privateUseyA2DmFWC` | 1 | opensuperwhisper |
| libswiftCore.dylib | `_$ss7UnicodeO15GeneralCategoryO10unassignedyA2DmFWC` | 1 | opensuperwhisper |
| libswiftCore.dylib | `_$ss7UnicodeO15GeneralCategoryO9surrogateyA2DmFWC` | 1 | opensuperwhisper |
| libswiftCore.dylib | `_$ss7UnicodeO6ScalarV10PropertiesV12isAlphabeticSbvg` | 1 | vorssaint |
| libswiftCore.dylib | `_$ss7UnicodeO6ScalarV10PropertiesV15isEmojiModifierSbvg` | 1 | vorssaint |
| libswiftCore.dylib | `_$ss7UnicodeO6ScalarV10PropertiesV19isEmojiModifierBaseSbvg` | 1 | vorssaint |
| libswiftCore.dylib | `_$ss8DurationV2peoiyyABz_ABtFZ` | 1 | opensuperwhisper |
| libswiftCore.dylib | `_$ss8RepeatedVyxGSKsMc` | 1 | opensuperwhisper |
| libswiftCoreML.dylib | `_$s6CoreML21MLShapedArrayProtocolPAAE10convertingxSo07MLMultiD0C_tcfC` | 1 | superwhisper |
| libswiftCoreML.dylib | `_$sSo14MLFeatureValueC6CoreMLE011shapedArrayB02ofAC08MLShapedF0VyxGSgxm_tAC0hF6ScalarRzlF` | 1 | opensuperwhisper |
| libswiftCoreML.dylib | `_$sSo7MLModelC6CoreMLE10prediction4from7optionsSo17MLFeatureProvider_pSoAG_p_So19MLPredictionOptionsCtYaKF` | 1 | opensuperwhisper |
| libswiftCoreML.dylib | `_$sSo7MLModelC6CoreMLE10prediction4from7optionsSo17MLFeatureProvider_pSoAG_p_So19MLPredictionOptionsCtYaKFTu` | 1 | opensuperwhisper |
| libswiftCoreML.dylib | `_$sSo7MLModelC6CoreMLE12compileModel2at10Foundation3URLVAH_tYaKFZ` | 1 | opensuperwhisper |
| libswiftCoreML.dylib | `_$sSo7MLModelC6CoreMLE12compileModel2at10Foundation3URLVAH_tYaKFZTu` | 1 | opensuperwhisper |
| libswiftDarwin.dylib | `_$s6Darwin10SEM_FAILEDSpys5Int32VGSgvg` | 1 | orbstack |
| libswiftDarwin.dylib | `_$s6Darwin14POSIXErrorCodeO5ELASTACvgZ` | 1 | protonvpn |
| libswiftDarwin.dylib | `_$s6Darwin8sem_openySpys5Int32VGSgSPys4Int8VG_ADs6UInt16Vs6UInt32VtF` | 1 | orbstack |
| libswiftDarwin.dylib | `_$s6Darwin8sem_openySpys5Int32VGSgSPys4Int8VG_ADtF` | 1 | orbstack |
| libswiftDispatch.dylib | `_$s8Dispatch0A12TimeIntervalO12millisecondsyACSicACmFWC` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$s8Dispatch0A4DataV9copyBytes2to4fromSiSryxG_SnySiGSgtlF` | 1 | xcodes-app |
| libswiftDispatch.dylib | `_$s8Dispatch1poiyAA0A4TimeVAD_AA0aB8IntervalOtF` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$sSo14OS_dispatch_ioC8DispatchE10StreamTypeO6randomyA2EmFWC` | 1 | cmux |
| libswiftDispatch.dylib | `_$sSo17OS_dispatch_groupC8DispatchE4wait7timeoutAC0D13TimeoutResultOAC0D4TimeV_tF` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$sSo17OS_dispatch_queueC8DispatchE3qosAC0D3QoSVvg` | 1 | opensuperwhisper |
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
| libswiftDispatch.dylib | `_$sSo24OS_dispatch_source_timerP8DispatchE8schedule8deadline9repeating6leewayyAC0E4TimeV_AC0eJ8IntervalOAKtF` | 1 | vorssaint |
| libswiftDispatch.dylib | `_$sSo24OS_dispatch_source_vnodeP8DispatchE4dataSo0a1_b1_C0CACE15FileSystemEventVvg` | 1 | vorssaint |
| libswiftNaturalLanguage.dylib | `_$sSo20NLLanguageRecognizerC15NaturalLanguageE13languageHintsSDySo0A0aSdGvs` | 1 | easydict |
| libswiftObservation.dylib | `_$s11Observation12ObservationsV8IteratorVyxq__GScIAAMc` | 1 | coteditor |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder10TryCaptureVMn` | 1 | raycast |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder10TryCaptureVyACyxG17_StringProcessing0A0VyxGcfC` | 1 | raycast |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder10TryCaptureVyxG17_StringProcessing0A9ComponentAAMc` | 1 | raycast |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder14CharacterClassVMn` | 1 | easydict |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder17NegativeLookaheadVMn` | 1 | easydict |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder17NegativeLookaheadVyACyxGqd__c0A6OutputQyd__Rsz17_StringProcessing0A9ComponentRd__lufC` | 1 | easydict |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder17NegativeLookaheadVyACyxGqd__yXEc0A6OutputQyd__Rsz17_StringProcessing0A9ComponentRd__lufC` | 1 | easydict |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder17NegativeLookaheadVyxG17_StringProcessing0A9ComponentAAMc` | 1 | easydict |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder3zzzoiyAA14CharacterClassVSJ_SJtF` | 1 | easydict |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder6AnchorV12endOfSubjectACvgZ` | 1 | easydict |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder6AnchorV12wordBoundaryACvgZ` | 1 | easydict |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder6AnchorV14startOfSubjectACvgZ` | 1 | easydict |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder6AnchorV9endOfLineACvgZ` | 1 | bettertouchtool |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder9LookaheadVMn` | 1 | easydict |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder9LookaheadVyACyxGqd__yXEc0A6OutputQyd__Rsz17_StringProcessing0A9ComponentRd__lufC` | 1 | easydict |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder9LookaheadVyxG17_StringProcessing0A9ComponentAAMc` | 1 | easydict |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder9ReferenceV4_rawSivg` | 1 | bettertouchtool |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder9ReferenceVMn` | 1 | bettertouchtool |
| libswiftRegexBuilder.dylib | `_$s12RegexBuilder9ReferenceVyACyxGxmcfC` | 1 | bettertouchtool |
| libswiftRegexBuilder.dylib | `_$s17_StringProcessing14RegexComponentP0C7BuilderAD14CharacterClassVRszrlE15newlineSequenceAFvgZ` | 1 | sfm |
| libswiftRegexBuilder.dylib | `_$s17_StringProcessing14RegexComponentP0C7BuilderAD14CharacterClassVRszrlE20horizontalWhitespaceAFvgZ` | 1 | bettertouchtool |
| libswiftRegexBuilder.dylib | `_$s17_StringProcessing14RegexComponentP0C7BuilderAD14CharacterClassVRszrlEyA2F_AFdtcfC` | 1 | easydict |
| libswiftRegexBuilder.dylib | `_$s17_StringProcessing5RegexV5MatchV0C7BuilderEyqd__AF9ReferenceVyqd__Gcluig` | 1 | bettertouchtool |
| libswiftRegexBuilder.dylib | `_$sSs17_StringProcessing14RegexComponent0C7BuilderMc` | 1 | easydict |
| libswiftSynchronization.dylib | `_$s15Synchronization5_CellVMn` | 1 | coteditor |
| libswiftSystem.dylib | `_$s6System14FileDescriptorV11OpenOptionsVMn` | 1 | whatsapp |
| libswiftSystem.dylib | `_$s6System14FileDescriptorV11OpenOptionsVN` | 1 | whatsapp |
| libswiftSystem.dylib | `_$s6System14FileDescriptorV11OpenOptionsVs10SetAlgebraAAMc` | 1 | whatsapp |
| libswiftSystem.dylib | `_$s6System15FilePermissionsVMn` | 1 | whatsapp |
| libswiftSystem.dylib | `_$s6System15FilePermissionsVN` | 1 | whatsapp |
| libswiftSystem.dylib | `_$s6System15FilePermissionsVs10SetAlgebraAAMc` | 1 | whatsapp |
| libswiftSystem.dylib | `_$s6System8FilePathV13lastComponentAC0E0VSgvg` | 1 | chatgpt |
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
| libswiftSystem_Foundation.dylib | `_$s6System8FilePathV01_A11_FoundationEyACSg0D03URLVcfC` | 1 | chatgpt |
| libswiftXPC.dylib | `_$s3XPC0A10_TYPE_DATEs13OpaquePointerVvg` | 1 | orbstack |
| libswiftXPC.dylib | `_$s3XPC0A11_TYPE_SHMEMs13OpaquePointerVvg` | 1 | orbstack |
| libswiftXPC.dylib | `_$s3XPC0A14_TYPE_ACTIVITYs13OpaquePointerVvg` | 1 | orbstack |
| libswiftXPC.dylib | `_$s3XPC0A14_TYPE_ENDPOINTs13OpaquePointerVvg` | 1 | orbstack |
| libswiftXPC.dylib | `_$s3XPC0A27_ERROR_TERMINATION_IMMINENTSo13OS_xpc_object_pvg` | 1 | orbstack |
| libswiftXPC.dylib | `_$s3XPC10XPCSessionC14setTargetQueueyySo17OS_dispatch_queueCFTj` | 1 | thaw |
| libswiftXPC.dylib | `_$s3XPC10XPCSessionC18setPeerRequirementyyAA07XPCPeerE0VF` | 1 | thaw |
| libswiftXPC.dylib | `_$s3XPC11XPCListenerC21InitializationOptionsV4noneAEvgZ` | 1 | thaw |
| libswiftXPC.dylib | `_$s3XPC11XPCListenerC21InitializationOptionsV8inactiveAEvgZ` | 1 | bartender |
| libswiftXPC.dylib | `_$s3XPC11XPCListenerC22IncomingSessionRequestC6reject6reasonAE8DecisionVSS_tFTj` | 1 | bartender |
| libswiftXPC.dylib | `_$s3XPC11XPCListenerC22IncomingSessionRequestC8DecisionVMa` | 1 | bartender |
| libswiftXPC.dylib | `_$s3XPC11XPCListenerC22IncomingSessionRequestC8DecisionVMn` | 1 | bartender |
| libswiftXPC.dylib | `_$s3XPC11XPCListenerC7service11targetQueue7options22incomingSessionHandlerACSS_So17OS_dispatch_queueCSgAC21InitializationOptionsVAC08IncomingH7RequestC8DecisionVANctKcfc` | 1 | thaw |
| libswiftXPC.dylib | `_$s3XPC11XPCListenerC8activateyyKFTj` | 1 | bartender |
| libswift_Concurrency.dylib | `_$sScs12ContinuationV11YieldResultO10terminatedyADyxq___GAFms5ErrorR_r0_lFWC` | 1 | opensuperwhisper |
| libswift_Concurrency.dylib | `_$sScs12ContinuationV11YieldResultO7droppedyADyxq___GxcAFms5ErrorR_r0_lFWC` | 1 | opensuperwhisper |
| libswift_Concurrency.dylib | `_$sScs12ContinuationV11YieldResultO8enqueuedyADyxq___GSi_tcAFms5ErrorR_r0_lFWC` | 1 | opensuperwhisper |
| libswift_Concurrency.dylib | `_$sScs8IteratorVyxq__GScIsMc` | 1 | opensuperwhisper |
| libswift_StringProcessing.dylib | `_$s11RegexOutput17_StringProcessing0A9ComponentPTl` | 1 | bettertouchtool |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing13_RegexFactoryV15captureOptionalyAA0C0VyxGq1__SiSgq0_Sgq_KctAA0C9ComponentR1_r2_lF` | 1 | raycast |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing13_RegexFactoryV27ignoreCapturesInTypedOutputyAA0C0VySsGxAA0C9ComponentRzlF` | 1 | bettertouchtool |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing13_RegexFactoryV7captureyAA0C0VyxGq__SitAA0C9ComponentR_r0_lF` | 1 | bettertouchtool |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing13_RegexFactoryV7exactlyyAA0C0VyxGSi_q_tAA0C9ComponentR_r0_lF` | 1 | raycast |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing13_RegexFactoryV9repeatingyAA0C0VyxGSnySiG_AA0C18RepetitionBehaviorVSgq_tAA0C9ComponentR_r0_lF` | 1 | easydict |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing14AnyRegexOutputV7ElementV5rangeSnySS5IndexVGSgvg` | 1 | bettertouchtool |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing14AnyRegexOutputV7ElementV9substringSsSgvg` | 1 | openclaw |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing14AnyRegexOutputVSlAAMc` | 1 | bettertouchtool |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing14AnyRegexOutputVyAC7ElementVSicig` | 1 | openclaw |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing14RegexComponentMp` | 1 | bettertouchtool |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing14RegexComponentP5regexAA0C0Vy0C6OutputQzGvgTq` | 1 | bettertouchtool |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing18RegexSemanticLevelV13unicodeScalarACvgZ` | 1 | coteditor |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing18RegexSemanticLevelVMa` | 1 | coteditor |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing29CustomConsumingRegexComponentMp` | 1 | bettertouchtool |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing29CustomConsumingRegexComponentP9consuming_10startingAt2inSS5IndexV10upperBound_0E6OutputQz6outputtSgSS_AHSnyAHGtKFTq` | 1 | bettertouchtool |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing29CustomConsumingRegexComponentPAA0eF0Tb` | 1 | bettertouchtool |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing29CustomConsumingRegexComponentPAAE5regexAA0E0Vy0E6OutputQzGvg` | 1 | bettertouchtool |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV10firstMatch2inAC0E0Vyx_GSgSS_tKF` | 1 | squirrel-app |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV17matchingSemanticsyACyxGAA0C13SemanticLevelVF` | 1 | coteditor |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV18dotMatchesNewlinesyACyxGSbF` | 1 | easydict |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV23anchorsMatchLineEndingsyACyxGSbF` | 1 | coteditor |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV5MatchV13dynamicMemberqd__s7KeyPathCyxqd__G_tcluipMV` | 1 | coteditor |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV5MatchV6outputxvpMV` | 1 | coteditor |
| libswift_StringProcessing.dylib | `_$s17_StringProcessing5RegexV_2asACyxGSS_xmtKcfC` | 1 | coteditor |
| libswift_StringProcessing.dylib | `_$sSK17_StringProcessingSL7ElementRpzrlE10firstRange2ofSny5IndexQzGSgqd___tSlRd__ABQyd__ACRSlF` | 1 | microsoft-teams |
| libswift_StringProcessing.dylib | `_$sSK17_StringProcessingSs11SubSequenceRtzrlE5split9separator9maxSplits25omittingEmptySubsequencesSaySsGqd___SiSbtAA14RegexComponentRd__lF` | 1 | orbstack |
| libswift_StringProcessing.dylib | `_$sSl17_StringProcessingSQ7ElementRpzrlE6ranges2ofSaySny5IndexQzGGqd___tSlRd__ABQyd__ACRSlF` | 1 | orbstack |
| libswift_StringProcessing.dylib | `_$sSm17_StringProcessingE10trimPrefix5whileySb7ElementQzKXE_tKF` | 1 | jordanbaird-ice |
| libswift_StringProcessing.dylib | `_$sSm17_StringProcessingSKRzSs11SubSequenceSlRtzrlE10trimPrefixyyqd__AA14RegexComponentRd__lF` | 1 | sfm |
| libswift_StringProcessing.dylib | `_$sSy17_StringProcessingE8containsySbSSF` | 1 | supacode |
| libswiftos.dylib | `_$s2os12OSSignposterV9isEnabledSbvg` | 1 | cmux |
| libswiftos.dylib | `_$s2os12OSSignposterVACycfC` | 1 | protonvpn |
| libswiftos.dylib | `_$sSo9OS_os_logC0B0E8CategoryV14dynamicTracingAEvgZ` | 1 | cmux |
| libswiftsimd.dylib | `_$sSo13simd_float3x3a0A0EyABSo0A6_quatfacfC` | 1 | raycast |
