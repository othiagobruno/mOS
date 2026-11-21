# Lacunas de Fernanda (Swift runtime, Dispatch)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Fernanda (Swift runtime, Dispatch) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**637** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| libswiftSystem.dylib | `_$s6System8FilePathVMa` | 11 | chatgpt, easydict, ghostty, github-copilot-for-xcode |
| libswiftSystem.dylib | `_$s6System5ErrnoVN` | 8 | chatgpt, github-copilot-for-xcode, homebrew-app, macpacker |
| libswiftSystem.dylib | `_$s6System5ErrnoVs5ErrorAAMc` | 8 | chatgpt, github-copilot-for-xcode, homebrew-app, macpacker |
| libswiftSystem.dylib | `_$s6System8FilePathVyACSScfC` | 8 | chatgpt, ghostty, homebrew-app, macpacker |
| libswiftSystem.dylib | `_$s6System14FileDescriptorV6_closes6ResultOyytAA5ErrnoVGyF` | 7 | chatgpt, github-copilot-for-xcode, homebrew-app, macpacker |
| libswiftSystem.dylib | `_$s6System14FileDescriptorVMn` | 7 | chatgpt, homebrew-app, macpacker, openclaw |
| libswiftSystem.dylib | `_$s6System8FilePathV11withCStringyxxSPys4Int8VGKXEKlF` | 7 | easydict, github-copilot-for-xcode, homebrew-app, macpacker |
| libswiftSystem.dylib | `_$s6System8FilePathV6stringSSvg` | 7 | chatgpt, ghostty, github-copilot-for-xcode, homebrew-app |
| libswiftSystem.dylib | `_$s6System8FilePathVMn` | 7 | chatgpt, github-copilot-for-xcode, homebrew-app, macpacker |
| libswiftSystem.dylib | `_$s6System14FileDescriptorV5_read4into16retryOnInterrupts6ResultOySiAA5ErrnoVGSw_SbtKF` | 6 | chatgpt, homebrew-app, macpacker, openclaw |
| libswiftSystem.dylib | `_$s6System14FileDescriptorV6_write_16retryOnInterrupts6ResultOySiAA5ErrnoVGSW_SbtF` | 6 | chatgpt, homebrew-app, macpacker, openclaw |
| libswiftSystem.dylib | `_$s6System8FilePathV9appendingyACSSnF` | 6 | chatgpt, github-copilot-for-xcode, homebrew-app, macpacker |
| libswiftDarwin.dylib | `_$s6Darwin7environSpySpys4Int8VGSgGvg` | 5 | betterdisplay, cmux, codexbar, openclaw |
| libswiftSystem.dylib | `_$s6System14FileDescriptorV5_open__7options11permissions16retryOnInterrupts6ResultOyAcA5ErrnoVGSPys4Int8VG_AC10AccessModeVAC11OpenOptionsVAA0B11PermissionsVSgSbtFZ` | 5 | github-copilot-for-xcode, homebrew-app, macpacker, openclaw |
| libswiftSystem.dylib | `_$s6System14FileDescriptorVN` | 5 | github-copilot-for-xcode, homebrew-app, macpacker, openclaw |
| libswiftSystem.dylib | `_$s6System8FilePathV13stringLiteralACSS_tcfC` | 5 | github-copilot-for-xcode, homebrew-app, macpacker, openclaw |
| libswiftSystem.dylib | `_$s6System8FilePathV2eeoiySbAC_ACtFZ` | 5 | chatgpt, homebrew-app, macpacker, openclaw |
| libswiftSystem.dylib | `_$s6System8FilePathVSHAAMc` | 5 | chatgpt, homebrew-app, macpacker, openclaw |
| libswiftSystem.dylib | `_$sSS6SystemE18withPlatformStringyxxSPys4Int8VGKXEKlF` | 5 | homebrew-app, macpacker, openclaw, teamviewer |
| ExtensionFoundation.framework | `_$s19ExtensionFoundation03AppA0Mp` | 4 | cmux, fuse-t, qlmarkdown, syntax-highlight |
| libswiftSystem.dylib | `_$s6System14FileDescriptorVSHAAMc` | 4 | homebrew-app, macpacker, openclaw, thaw |
| libswiftSystem.dylib | `_$s6System5ErrnoV11descriptionSSvg` | 4 | homebrew-app, macpacker, openclaw, thaw |
| libswiftSystem.dylib | `_$s6System5ErrnoVMn` | 4 | homebrew-app, macpacker, openclaw, thaw |
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
| Translation.framework | `_$s11Translation0A7SessionC07prepareA0yyYaKFTjTu` | 3 | macwhisper, markedit, orion |
| Translation.framework | `_$s11Translation0A7SessionC8ResponseV10targetTextSSvg` | 3 | macwhisper, markedit, orion |
| Translation.framework | `_$s11Translation0A7SessionC8ResponseVMa` | 3 | macwhisper, markedit, orion |
| Translation.framework | `_$s11Translation0A7SessionCMa` | 3 | macwhisper, markedit, orion |
| libswiftSystem.dylib | `_$s6System14FileDescriptorV10_duplicate2as16retryOnInterrupts6ResultOyAcA5ErrnoVGACSg_SbtKF` | 3 | homebrew-app, macpacker, thaw |
| libswiftSystem.dylib | `_$s6System14FileDescriptorV5_pipes6ResultOyAC7readEnd_AC05writeG0tAA5ErrnoVGyFZ` | 3 | homebrew-app, macpacker, thaw |
| libswiftSystem.dylib | `_$s6System8FilePathV10isAbsoluteSbvg` | 3 | homebrew-app, openclaw, thaw |
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
| Translation.framework | `_$s11Translation0A5ErrorV18nothingToTranslateACvgZ` | 2 | macwhisper, orion |
| Translation.framework | `_$s11Translation0A5ErrorV2teoiySbAC_s0B0_ptFZ` | 2 | macwhisper, orion |
| Translation.framework | `_$s11Translation0A5ErrorVMa` | 2 | macwhisper, orion |
| Translation.framework | `_$s11Translation0A5ErrorVs0B0AAMc` | 2 | macwhisper, orion |
| Translation.framework | `_$s11Translation0A7SessionC7RequestV10sourceText16clientIdentifierAESS_SSSgtcfC` | 2 | macwhisper, orion |
| Translation.framework | `_$s11Translation0A7SessionC7RequestVMa` | 2 | macwhisper, orion |
| Translation.framework | `_$s11Translation0A7SessionC7RequestVMn` | 2 | macwhisper, orion |
| Translation.framework | `_$s11Translation0A7SessionC9translateyAC8ResponseVSSYaKFTjTu` | 2 | macwhisper, markedit |
| Translation.framework | `_$s11Translation20LanguageAvailabilityC18supportedLanguagesSay10Foundation6LocaleV0B0VGvgTjTu` | 2 | macwhisper, orion |
| Translation.framework | `_$s11Translation20LanguageAvailabilityC6StatusO9installedyA2EmFWC` | 2 | macwhisper, orion |
| Translation.framework | `_$s11Translation20LanguageAvailabilityC6StatusOMa` | 2 | macwhisper, orion |
| Translation.framework | `_$s11Translation20LanguageAvailabilityC6StatusOSQAAMc` | 2 | macwhisper, orion |
| Translation.framework | `_$s11Translation20LanguageAvailabilityC6status4from2toAC6StatusO10Foundation6LocaleV0B0V_AMSgtYaFTjTu` | 2 | macwhisper, orion |
| Translation.framework | `_$s11Translation20LanguageAvailabilityCACycfc` | 2 | macwhisper, orion |
| Translation.framework | `_$s11Translation20LanguageAvailabilityCMa` | 2 | macwhisper, orion |
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
| libobjc.A.dylib | `_OBJC_CLASS_$_Protocol` | 2 | crossover, dropbox |
| libobjc.A.dylib | `_class_getVersion` | 2 | crossover, dropbox |
| libobjc.A.dylib | `_class_setVersion` | 2 | crossover, dropbox |
| libswiftSystem.dylib | `_$s6System8FilePathV14platformStringACSPys4Int8VG_tcfC` | 2 | easydict, orbstack |
| libswiftSystem.dylib | `_$s6System8FilePathV9ComponentV6stringSSvg` | 2 | chatgpt, nordvpn |
| libswiftSystem.dylib | `_$s6System8FilePathV9ComponentVMa` | 2 | chatgpt, nordvpn |
| libswiftSystem.dylib | `_$s6System8FilePathV9ComponentVMn` | 2 | chatgpt, nordvpn |
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
| CocoaUI.framework | `_$s7CocoaUI15CUIPresenceViewC10dataSourceAA0cd4DataF0_pSgvs` | 1 | microsoft-office |
| CocoaUI.framework | `_$s7CocoaUI15CUIPresenceViewC28userInterfaceLayoutDirectionSo06NSUserfgH0Vvs` | 1 | microsoft-office |
| CocoaUI.framework | `_$s7CocoaUI15CUIPresenceViewCMa` | 1 | microsoft-office |
| CocoaUI.framework | `_$s7CocoaUI15CUIPresenceViewCMn` | 1 | microsoft-office |
| CocoaUI.framework | `_$s7CocoaUI18AnyViewObjCWrapperC4viewAC05SwiftB00cD0V_tcfc` | 1 | microsoft-office |
| CocoaUI.framework | `_$s7CocoaUI18AnyViewObjCWrapperCMa` | 1 | microsoft-office |
| CocoaUI.framework | `_$s7CocoaUI19OverlayHostGeometryC12contentFrame2inSo6CGRectVSgSo8NSWindowCSg_tF` | 1 | microsoft-office |
| CocoaUI.framework | `_$s7CocoaUI19OverlayHostGeometryC18hasMeasuredContentSbvg` | 1 | microsoft-office |
| CocoaUI.framework | `_$s7CocoaUI19OverlayHostGeometryC20contentFrameInGlobalSo6CGRectVSgvg` | 1 | microsoft-office |
| CocoaUI.framework | `_$s7CocoaUI19OverlayHostGeometryC23setMeasurementRequestedyySbF` | 1 | microsoft-office |
| CocoaUI.framework | `_$s7CocoaUI19OverlayHostGeometryCMa` | 1 | microsoft-office |
| CocoaUI.framework | `_$s7CocoaUI19OverlayHostGeometryCMn` | 1 | microsoft-office |
| CocoaUI.framework | `_$s7CocoaUI25DocumentShellFocusElementC4viewACSo6NSViewC_tcfc` | 1 | microsoft-office |
| CocoaUI.framework | `_$s7CocoaUI25DocumentShellFocusElementCMa` | 1 | microsoft-office |
| CocoaUI.framework | `_$s7CocoaUI28TitleBarBackgroundPreferenceOMn` | 1 | microsoft-office |
| CocoaUI.framework | `_$s7CocoaUI28TitleBarBackgroundPreferenceON` | 1 | microsoft-office |
| CocoaUI.framework | `_$s7CocoaUI28TitleBarBackgroundPreferenceOSHAAMc` | 1 | microsoft-office |
| CocoaUI.framework | `_$s7CocoaUI28TitleBarBackgroundPreferenceOSQAAMc` | 1 | microsoft-office |
| CocoaUI.framework | `_$s7CocoaUI29KeyboardNavigationFocusBridgeC11hostingView15excludedSubtreeACSo6NSViewC_AGSgtcfc` | 1 | microsoft-office |
| CocoaUI.framework | `_$s7CocoaUI29KeyboardNavigationFocusBridgeC5startyyF` | 1 | microsoft-office |
| CocoaUI.framework | `_$s7CocoaUI29KeyboardNavigationFocusBridgeCMa` | 1 | microsoft-office |
| CocoaUI.framework | `_$s7CocoaUI29KeyboardNavigationFocusBridgeCMn` | 1 | microsoft-office |
| CocoaUI.framework | `_$s7CocoaUI35TitleBarBackgroundPreferenceManagerC18aluminumThemeLabelSSvau` | 1 | microsoft-office |
| CocoaUI.framework | `_$s7CocoaUI35TitleBarBackgroundPreferenceManagerC18colorfulThemeLabelSSvau` | 1 | microsoft-office |
| CocoaUI.framework | `_$s7CocoaUI35TitleBarBackgroundPreferenceManagerC21themeChangeAlertLabelSSvau` | 1 | microsoft-office |
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
| FluentUI.framework | `_$s8FluentUI0A5ThemeC10ColorTokenOMn` | 1 | microsoft-office |
| FluentUI.framework | `_$s8FluentUI0A5ThemeC12swiftUIColory05SwiftB05ColorVAC0G5TokenOF` | 1 | microsoft-office |
| FluentUI.framework | `_$s8FluentUI0A5ThemeC15TypographyTokenOMn` | 1 | microsoft-office |
| FluentUI.framework | `_$s8FluentUI0A5ThemeC6sharedACvgZ` | 1 | microsoft-office |
| FluentUI.framework | `_$s8FluentUI0A5ThemeCACycfC` | 1 | microsoft-office |
| FluentUI.framework | `_$s8FluentUI0A5ThemeCMa` | 1 | microsoft-office |
| FluentUI.framework | `_$s8FluentUI0A5ThemeCMn` | 1 | microsoft-office |
| FluentUI.framework | `_$s8FluentUI10ShadowInfoC11ambientBlur12CoreGraphics7CGFloatVvg` | 1 | microsoft-office |
| FluentUI.framework | `_$s8FluentUI10ShadowInfoC12ambientColor05SwiftB00F0Vvg` | 1 | microsoft-office |
| FluentUI.framework | `_$s8FluentUI10ShadowInfoC4xKey12CoreGraphics7CGFloatVvg` | 1 | microsoft-office |
| FluentUI.framework | `_$s8FluentUI10ShadowInfoC4yKey12CoreGraphics7CGFloatVvg` | 1 | microsoft-office |
| FluentUI.framework | `_$s8FluentUI10ShadowInfoC7keyBlur12CoreGraphics7CGFloatVvg` | 1 | microsoft-office |
| FluentUI.framework | `_$s8FluentUI10ShadowInfoC8keyColor05SwiftB00F0Vvg` | 1 | microsoft-office |
| FluentUI.framework | `_$s8FluentUI10ShadowInfoC8xAmbient12CoreGraphics7CGFloatVvg` | 1 | microsoft-office |
| FluentUI.framework | `_$s8FluentUI10ShadowInfoC8yAmbient12CoreGraphics7CGFloatVvg` | 1 | microsoft-office |
| FluentUI.framework | `_$s8FluentUI10ShadowInfoCMn` | 1 | microsoft-office |
| FluentUI.framework | `_$s8FluentUI12GlobalTokensC12SpacingTokenOMn` | 1 | microsoft-office |
| FluentUI.framework | `_$s8FluentUI12GlobalTokensC16StrokeWidthTokenOMn` | 1 | microsoft-office |
| FluentUI.framework | `_$s8FluentUI12GlobalTokensC17CornerRadiusTokenOMn` | 1 | microsoft-office |
| FluentUI.framework | `_$s8FluentUI12GlobalTokensC18sharedSwiftUIColory0fB05ColorVAC06SharedH3SetO_AC0iH5TokenOtFZ` | 1 | microsoft-office |
| FluentUI.framework | `_$s8FluentUI12GlobalTokensC19neutralSwiftUIColory0fB05ColorVAC07NeutralH5TokenOFZ` | 1 | microsoft-office |
| FluentUI.framework | `_$s8FluentUI12GlobalTokensC4icony12CoreGraphics7CGFloatVAC13IconSizeTokenOFZ` | 1 | microsoft-office |
| FluentUI.framework | `_$s8FluentUI12GlobalTokensC6cornery12CoreGraphics7CGFloatVAC17CornerRadiusTokenOFZ` | 1 | microsoft-office |
| FluentUI.framework | `_$s8FluentUI12GlobalTokensC6strokey12CoreGraphics7CGFloatVAC16StrokeWidthTokenOFZ` | 1 | microsoft-office |
| FluentUI.framework | `_$s8FluentUI12GlobalTokensC7spacingy12CoreGraphics7CGFloatVAC12SpacingTokenOFZ` | 1 | microsoft-office |
| FluentUI.framework | `_$s8FluentUI12GlobalTokensCMa` | 1 | microsoft-office |
| FluentUI.framework | `_$s8FluentUI8FontInfoC4nameSSSgvg` | 1 | microsoft-office |
| FluentUI.framework | `_$s8FluentUI8FontInfoC4size12CoreGraphics7CGFloatVvg` | 1 | microsoft-office |
| FluentUI.framework | `_$s8FluentUI8FontInfoC6weight05SwiftB00C0V6WeightVvg` | 1 | microsoft-office |
| FluentUI.framework | `_$sSo7NSColorC8FluentUIE5light4darkA2B_ABSgtcfC` | 1 | microsoft-office |
| FluentUI.framework | `_$sSo7NSColorC8FluentUIE8hexValueABs6UInt32V_tcfC` | 1 | microsoft-office |
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
| TabularData.framework | `_$s11TabularData0B5FrameV03csvB07columns4rows5types7optionsAC10Foundation0B0V_SaySSGSgSnySiGSgSDySSAA7CSVTypeOGAA17CSVReadingOptionsVtKcfC` | 1 | sf-symbols |
| TabularData.framework | `_$s11TabularData0B5FrameV13indexOfColumnySiSgSSF` | 1 | sf-symbols |
| TabularData.framework | `_$s11TabularData0B5FrameV17contentsOfCSVFile7columns4rows5types7optionsAC10Foundation3URLV_SaySSGSgSnySiGSgSDySSAA7CSVTypeOGAA17CSVReadingOptionsVtKcfC` | 1 | sf-symbols |
| TabularData.framework | `_$s11TabularData0B5FrameV3RowVMa` | 1 | sf-symbols |
| TabularData.framework | `_$s11TabularData0B5FrameV3RowVyxSgAA8ColumnIDVyxGcluig` | 1 | sf-symbols |
| TabularData.framework | `_$s11TabularData0B5FrameV3RowVyxSgSS_xmtcluig` | 1 | sf-symbols |
| TabularData.framework | `_$s11TabularData0B5FrameV4RowsVMa` | 1 | sf-symbols |
| TabularData.framework | `_$s11TabularData0B5FrameV4RowsVMn` | 1 | sf-symbols |
| TabularData.framework | `_$s11TabularData0B5FrameV4RowsVSTAAMc` | 1 | sf-symbols |
| TabularData.framework | `_$s11TabularData0B5FrameV4RowsVSlAAMc` | 1 | sf-symbols |
| TabularData.framework | `_$s11TabularData0B5FrameV4rowsAC4RowsVvg` | 1 | sf-symbols |
| TabularData.framework | `_$s11TabularData0B5FrameV7columnsSayAA9AnyColumnVGvg` | 1 | sf-symbols |
| TabularData.framework | `_$s11TabularData0B5FrameVMa` | 1 | sf-symbols |
| TabularData.framework | `_$s11TabularData0B5FrameVMn` | 1 | sf-symbols |
| TabularData.framework | `_$s11TabularData17CSVReadingOptionsV12hasHeaderRow12nilEncodings04trueI005falseI017floatingPointType17ignoresEmptyLines11usesQuoting0R8Escaping9delimiter15escapeCharacterACSb_ShySSGA2nA7CSVTypeOS3bS2JtcfC` | 1 | sf-symbols |
| TabularData.framework | `_$s11TabularData17CSVReadingOptionsVMa` | 1 | sf-symbols |
| TabularData.framework | `_$s11TabularData6ColumnV010eraseToAnyC0AA0fC0VyF` | 1 | tableplus |
| TabularData.framework | `_$s11TabularData6ColumnV4name8contentsACyxGSS_qd__tcSTRd__xSg7ElementRtd__lufC` | 1 | tableplus |
| TabularData.framework | `_$s11TabularData6ColumnVMa` | 1 | tableplus |
| TabularData.framework | `_$s11TabularData7CSVTypeO6doubleyA2CmFWC` | 1 | sf-symbols |
| TabularData.framework | `_$s11TabularData7CSVTypeO6stringyA2CmFWC` | 1 | sf-symbols |
| TabularData.framework | `_$s11TabularData7CSVTypeO7booleanyA2CmFWC` | 1 | sf-symbols |
| TabularData.framework | `_$s11TabularData7CSVTypeO7integeryA2CmFWC` | 1 | sf-symbols |
| TabularData.framework | `_$s11TabularData7CSVTypeOMa` | 1 | sf-symbols |
| TabularData.framework | `_$s11TabularData7CSVTypeOMn` | 1 | sf-symbols |
| TabularData.framework | `_$s11TabularData8ColumnIDV4nameSSvg` | 1 | sf-symbols |
| TabularData.framework | `_$s11TabularData8ColumnIDVMn` | 1 | sf-symbols |
| TabularData.framework | `_$s11TabularData8ColumnIDVyACyxGSS_xmtcfC` | 1 | sf-symbols |
| TabularData.framework | `_$s11TabularData9AnyColumnV18wrappedElementTypeypXpvg` | 1 | sf-symbols |
| TabularData.framework | `_$s11TabularData9AnyColumnVMa` | 1 | sf-symbols |
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
| Translation.framework | `_$s11Translation0A5ErrorV25unsupportedSourceLanguageACvgZ` | 1 | macwhisper |
| Translation.framework | `_$s11Translation0A5ErrorV25unsupportedTargetLanguageACvgZ` | 1 | macwhisper |
| Translation.framework | `_$s11Translation0A5ErrorV26unsupportedLanguagePairingACvgZ` | 1 | macwhisper |
| Translation.framework | `_$s11Translation0A5ErrorVMn` | 1 | orion |
| Translation.framework | `_$s11Translation0A7SessionC12translations4fromSayAC8ResponseVGSayAC7RequestVG_tYaKFTjTu` | 1 | macwhisper |
| Translation.framework | `_$s11Translation0A7SessionC13BatchResponseV13AsyncIteratorVMa` | 1 | orion |
| Translation.framework | `_$s11Translation0A7SessionC13BatchResponseV13AsyncIteratorVScIAAMc` | 1 | orion |
| Translation.framework | `_$s11Translation0A7SessionC13BatchResponseV17makeAsyncIteratorAE0fG0VyF` | 1 | orion |
| Translation.framework | `_$s11Translation0A7SessionC13BatchResponseVMa` | 1 | orion |
| Translation.framework | `_$s11Translation0A7SessionC13ConfigurationV10invalidateyyF` | 1 | macwhisper |
| Translation.framework | `_$s11Translation0A7SessionC13ConfigurationV6source10Foundation6LocaleV8LanguageVSgvg` | 1 | macwhisper |
| Translation.framework | `_$s11Translation0A7SessionC13ConfigurationV6source6targetAE10Foundation6LocaleV8LanguageVSg_AMtcfC` | 1 | macwhisper |
| Translation.framework | `_$s11Translation0A7SessionC13ConfigurationV6target10Foundation6LocaleV8LanguageVSgvg` | 1 | macwhisper |
| Translation.framework | `_$s11Translation0A7SessionC13ConfigurationVMa` | 1 | macwhisper |
| Translation.framework | `_$s11Translation0A7SessionC13ConfigurationVMn` | 1 | macwhisper |
| Translation.framework | `_$s11Translation0A7SessionC14targetLanguage10Foundation6LocaleV0D0VSgvg` | 1 | macwhisper |
| Translation.framework | `_$s11Translation0A7SessionC15installedSource6targetAC10Foundation6LocaleV8LanguageV_AJSgtcfC` | 1 | markedit |
| Translation.framework | `_$s11Translation0A7SessionC8ResponseV16clientIdentifierSSSgvg` | 1 | orion |
| Translation.framework | `_$s11Translation0A7SessionC8ResponseVMn` | 1 | orion |
| Translation.framework | `_$s11Translation0A7SessionC9translate5batchAC13BatchResponseVSayAC7RequestVG_tFTj` | 1 | orion |
| Translation.framework | `_$s11Translation0A7SessionCMn` | 1 | orion |
| Translation.framework | `_$s11Translation20LanguageAvailabilityC6StatusO11unsupportedyA2EmFWC` | 1 | macwhisper |
| Translation.framework | `_$s11Translation20LanguageAvailabilityC6StatusO2eeoiySbAE_AEtFZ` | 1 | orion |
| Translation.framework | `_$s11Translation20LanguageAvailabilityC6StatusO9supportedyA2EmFWC` | 1 | orion |
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
| libobjc.A.dylib | `__class_setCustomDeallocInitiation` | 1 | orion |
| libobjc.A.dylib | `__objc_deallocOnMainThreadHelper` | 1 | orion |
| libobjc.A.dylib | `__protocol_getMethodTypeEncoding` | 1 | orion |
| libswiftCompression.dylib | `_$s11Compression11InputFilterC8readData8ofLength10Foundation0E0VSgSi_tKFTj` | 1 | bartender |
| libswiftCompression.dylib | `_$s11Compression11InputFilterCMn` | 1 | bartender |
| libswiftCompression.dylib | `_$s11Compression11InputFilterC_5using14bufferCapacity11readingFromACyxGAA0C9OperationO_AA9AlgorithmOSixSgSiKctKcfc` | 1 | bartender |
| libswiftCompression.dylib | `_$s11Compression9AlgorithmO8rawValueACSgSo21compression_algorithma_tcfC` | 1 | bartender |
| libswiftCompression.dylib | `_$s11Compression9AlgorithmOMn` | 1 | bartender |
| libswiftCompression.dylib | `_$s11Compression9AlgorithmOSQAAMc` | 1 | bartender |
| libswiftDarwin.dylib | `_$s6Darwin10SEM_FAILEDSpys5Int32VGSgvg` | 1 | orbstack |
| libswiftDarwin.dylib | `_$s6Darwin14POSIXErrorCodeO5ELASTACvgZ` | 1 | protonvpn |
| libswiftDarwin.dylib | `_$s6Darwin8sem_openySpys5Int32VGSgSPys4Int8VG_ADs6UInt16Vs6UInt32VtF` | 1 | orbstack |
| libswiftDarwin.dylib | `_$s6Darwin8sem_openySpys5Int32VGSgSPys4Int8VG_ADtF` | 1 | orbstack |
| libswiftDispatch.dylib | `_$s8Dispatch0A4DataVys5SliceVyACGSnySiGcig` | 1 | nordvpn |
| libswiftDispatch.dylib | `_$sSo17OS_dispatch_queueC8DispatchE12asyncAndWait7executexxyKXE_tKlF` | 1 | microsoft-outlook |
| libswiftDispatch.dylib | `_$sSo18OS_dispatch_sourceP8DispatchE16setCancelHandler7handleryAC0D8WorkItemC_tF` | 1 | nordvpn |
| libswiftSystem.dylib | `_$s6System14FileDescriptorV11OpenOptionsVMn` | 1 | whatsapp |
| libswiftSystem.dylib | `_$s6System14FileDescriptorV11OpenOptionsVN` | 1 | whatsapp |
| libswiftSystem.dylib | `_$s6System14FileDescriptorV11OpenOptionsVs10SetAlgebraAAMc` | 1 | whatsapp |
| libswiftSystem.dylib | `_$s6System15FilePermissionsVMn` | 1 | whatsapp |
| libswiftSystem.dylib | `_$s6System15FilePermissionsVN` | 1 | whatsapp |
| libswiftSystem.dylib | `_$s6System15FilePermissionsVs10SetAlgebraAAMc` | 1 | whatsapp |
| libswiftSystem.dylib | `_$s6System8FilePathV10componentsAC13ComponentViewVvg` | 1 | nordvpn |
| libswiftSystem.dylib | `_$s6System8FilePathV11descriptionSSvg` | 1 | github-copilot-for-xcode |
| libswiftSystem.dylib | `_$s6System8FilePathV13ComponentViewV5IndexVMa` | 1 | nordvpn |
| libswiftSystem.dylib | `_$s6System8FilePathV13ComponentViewVMa` | 1 | nordvpn |
| libswiftSystem.dylib | `_$s6System8FilePathV13ComponentViewVSKAAMc` | 1 | nordvpn |
| libswiftSystem.dylib | `_$s6System8FilePathV13ComponentViewVSlAAMc` | 1 | nordvpn |
| libswiftSystem.dylib | `_$s6System8FilePathV13lastComponentAC0E0VSgvg` | 1 | chatgpt |
| libswiftSystem.dylib | `_$s6System8FilePathV9ComponentV4KindO2eeoiySbAG_AGtFZ` | 1 | chatgpt |
| libswiftSystem.dylib | `_$s6System8FilePathV9ComponentV4kindAE4KindOvg` | 1 | chatgpt |
| libswiftSystem.dylib | `_$s6System8FilePathV9ComponentV9extensionSSSgvg` | 1 | chatgpt |
| libswiftSystem.dylib | `_$s6System8FilePathV9ComponentVyAESgSScfC` | 1 | chatgpt |
| libswiftSystem.dylib | `_$s6System8FilePathVACycfC` | 1 | nordvpn |
| libswiftSystem.dylib | `_$s6System8FilePathVSEAAMc` | 1 | chatgpt |
| libswiftSystem.dylib | `_$s6System8FilePathVSQAAMc` | 1 | openclaw |
| libswiftSystem.dylib | `_$s6System8FilePathVSeAAMc` | 1 | chatgpt |
| libswiftSystem.dylib | `_$s6System8FilePathVs23CustomStringConvertibleAAMc` | 1 | chatgpt |
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
| libswiftos.dylib | `_$s2os12OSSignposterVACycfC` | 1 | protonvpn |
| libswiftsimd.dylib | `_$sSo13simd_float3x3a0A0EyABSo0A6_quatfacfC` | 1 | raycast |
| mbuinstrument.framework | `_$s13mbuinstrument15LoggingCategoryV11copilotChatACvau` | 1 | microsoft-office |
| mbuinstrument.framework | `_$s13mbuinstrument15LoggingCategoryV6docsuiACvau` | 1 | microsoft-office |
| mbuinstrument.framework | `_$s13mbuinstrument15LoggingSeverityV4infoACvau` | 1 | microsoft-office |
| mbuinstrument.framework | `_$s13mbuinstrument15LoggingSeverityV5errorACvau` | 1 | microsoft-office |
| mbuinstrument.framework | `_$s13mbuinstrument15LoggingSeverityV7verboseACvau` | 1 | microsoft-office |
| mbuinstrument.framework | `_$s13mbuinstrument15LoggingSeverityV7warningACvau` | 1 | microsoft-office |
| mbuinstrument.framework | `_$s13mbuinstrument19sendDiagnosticTrace3tag8category8severity19validDataCategories5label9argumentsys6UInt32V_AA15LoggingCategoryVAA0N8SeverityVAA0n5ValidiJ0VSSSDySSAA26ClassifiedStructuredObject_p_AA0nI14ClassificationVtGtF` | 1 | microsoft-office |
| mbuinstrument.framework | `_$s13mbuinstrument25LoggingDataClassificationV014publicPersonalC0ACvau` | 1 | microsoft-office |
| mbuinstrument.framework | `_$s13mbuinstrument25LoggingDataClassificationV017publicNonPersonalC0ACvau` | 1 | microsoft-office |
| mbuinstrument.framework | `_$s13mbuinstrument25LoggingDataClassificationV07accountC0ACvau` | 1 | microsoft-office |
| mbuinstrument.framework | `_$s13mbuinstrument25LoggingDataClassificationV07supportC0ACvau` | 1 | microsoft-office |
| mbuinstrument.framework | `_$s13mbuinstrument25LoggingDataClassificationV13accessControlACvau` | 1 | microsoft-office |
| mbuinstrument.framework | `_$s13mbuinstrument25LoggingDataClassificationV14systemMetadataACvau` | 1 | microsoft-office |
| mbuinstrument.framework | `_$s13mbuinstrument25LoggingDataClassificationV15customerContentACvau` | 1 | microsoft-office |
| mbuinstrument.framework | `_$s13mbuinstrument25LoggingDataClassificationV24essentialServiceMetadataACvau` | 1 | microsoft-office |
| mbuinstrument.framework | `_$s13mbuinstrument25LoggingDataClassificationV30endUserIdentifiableInformationACvau` | 1 | microsoft-office |
| mbuinstrument.framework | `_$s13mbuinstrument25LoggingDataClassificationV30endUserPseudonymousInformationACvau` | 1 | microsoft-office |
| mbuinstrument.framework | `_$s13mbuinstrument25LoggingDataClassificationV35organizationIdentifiableInformationACvau` | 1 | microsoft-office |
| mbuinstrument.framework | `_$s13mbuinstrument25LoggingDataClassificationVMn` | 1 | microsoft-office |
| mbuinstrument.framework | `_$s13mbuinstrument26ClassifiedStructuredObjectMp` | 1 | microsoft-office |
| mbuinstrument.framework | `_$s13mbuinstrument26LoggingValidDataCategoriesV19productServiceUsageACvau` | 1 | microsoft-office |
| mbuinstrument.framework | `_$sSS13mbuinstrument26ClassifiedStructuredObjectAAWP` | 1 | microsoft-office |
| mbuinstrument.framework | `_$sSi13mbuinstrument26ClassifiedStructuredObjectAAWP` | 1 | microsoft-office |
| mbukernel.framework | `_$s16MBUIdleWorkAsync0ab3AddC0yySi_So0aB10CoalescingVSbyYaYbctYaF` | 1 | microsoft-office |
| mbukernel.framework | `_$s16MBUIdleWorkAsync0ab3AddC0yySi_So0aB10CoalescingVSbyYaYbctYaFTu` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB10ClientInfoC7hostApp9sessionIDACSS_SStcfc` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB10ClientInfoCMa` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB12TemplateItemC5titleSSvpWvd` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB12TemplateItemC7assetIDSSvpWvd` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB12TemplateItemC7setKindAA0ab3SetG0OvpWvd` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB12TemplateItemC8mimeTypeSSvpWvd` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB12TemplateItemC9assetTypeSSvpWvd` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB12TemplateItemC9referenceSSvpWvd` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB12TemplateItemCMa` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB12TemplateItemCN` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB13ServiceConfigV08isInlineB13FieldsEnabled0f23StreamingInResolveAssetI00fab5CacheI00f17ThumbnailPrefetchI009thumbnailP11Concurrency0qP5Limit0f4BootpI00f19Thumbnail404RemovalI019evictionStatusCodes15cacheTtlMinutes25searchAssetsFirstPageSize20searchAssetsPageSize0f26IdentityScopedLicenseCheckI0ACSb_S3bS2iS2bSaySiGS3iSbtcfC` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB13ServiceConfigVN` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB18TemplateOperationsC06cancelD8DownloadyyF` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB18TemplateOperationsC08downloadaD07assetID9reference0G4Type04mimeJ05title10completionySS_S4Sy10Foundation3URLVSgctF` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB18TemplateOperationsC08downloadaD13WithCacheInfo7assetID9reference0J4Type04mimeM05title010completionghI0ySS_S4Sy10Foundation3URLVSg_SbtctF` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB18TemplateOperationsC13updateHandleryycSgvpWvd` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB18TemplateOperationsC14fetchThumbnail12forReference7setKind10completionySS_AA0ab3SetK0Oy10Foundation4DataVSgctF` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB18TemplateOperationsC14retrievalStateAA0abd9RetrievalG0OvpWvd` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB18TemplateOperationsC16hadCacheDeliverySbvpWvd` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB18TemplateOperationsC20clearCacheForSignOutyyF` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB18TemplateOperationsC4stopyyF` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB18TemplateOperationsC5startyyF` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB18TemplateOperationsC6create10clientInfo12capabilities6configAcA0ab6ClientH0C_AA0abD12CapabilitiesCAA0aB13ServiceConfigVtKFZ` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB18TemplateOperationsC6flowIDSSvpWvd` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB18TemplateOperationsC6reloadyyF` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB18TemplateOperationsC8activate4modeyAA0abD9FetchModeO_tF` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB18TemplateOperationsC8allItemsSayAA0abD4ItemCGvpWvd` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB18TemplateOperationsC9isOfflineSbvpWvd` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB18TemplateOperationsCMa` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB18TemplateOperationsCMn` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB20TemplateCapabilitiesC9assetType23supportedFileExtensions18defaultAspectRatioACSS_SaySSGSdtcfc` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB20TemplateCapabilitiesCMa` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB22TemplateRetrievalStateON` | 1 | microsoft-office |
| mso99.framework | `_$s17BrandKitTemplates0aB7SetKindON` | 1 | microsoft-office |
| mso99.framework | `_$s19AggregatedMRUFacade0A9MRUUpdateVMa` | 1 | microsoft-office |
| mso99.framework | `_$s19AggregatedMRUFacade0A9MRUUpdateVMn` | 1 | microsoft-office |
| mso99.framework | `_$s19AggregatedMRUFacadeAAC5appID7optionsABSgSo019MSOAggregatedMRUAppD0V_AA0A10MRUOptionsVtYacfc` | 1 | microsoft-office |
| mso99.framework | `_$s19AggregatedMRUFacadeAAC5appID7optionsABSgSo019MSOAggregatedMRUAppD0V_AA0A10MRUOptionsVtYacfcTu` | 1 | microsoft-office |
| mso99.framework | `_$s19AggregatedMRUFacadeAACMa` | 1 | microsoft-office |
| mso99.framework | `_$s19AggregatedMRUFacadeAACMn` | 1 | microsoft-office |
