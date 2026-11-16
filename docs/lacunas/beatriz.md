# Lacunas de Beatriz (AppKit)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Beatriz (AppKit) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**755** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| ApplicationServices.framework | `_PasteboardCreate` | 28 | audacity, calibre, db-browser-for-sqlite, dolphin |
| ApplicationServices.framework | `_PasteboardSynchronize` | 28 | audacity, calibre, db-browser-for-sqlite, dolphin |
| Carbon.framework | `_GetEventDispatcherTarget` | 28 | adobe-acrobat-reader, alt-tab, betterdisplay, clipy |
| AppKit.framework | `_OBJC_CLASS_$_NSFontPanel` | 27 | alfred, audacity, calibre, coteditor |
| AppKit.framework | `_OBJC_CLASS_$_NSPageLayout` | 25 | adobe-acrobat-reader, android-studio, calibre, db-browser-for-sqlite |
| Carbon.framework | `_TISCopyInputMethodKeyboardLayoutOverride` | 25 | audacity, calibre, db-browser-for-sqlite, dolphin |
| Carbon.framework | `_TISCreateInputSourceList` | 25 | adobe-acrobat-reader, alfred, alt-tab, android-studio |
| AppKit.framework | `_NSAccessibilityUnignoredChildrenForOnlyChild` | 23 | android-studio, audacity, calibre, db-browser-for-sqlite |
| AppKit.framework | `_NSFileContentsPboardType` | 22 | android-studio, audacity, calibre, db-browser-for-sqlite |
| Carbon.framework | `_kTISPropertyInputSourceIsASCIICapable` | 21 | android-studio, blender, clipy, firefox |
| AppKit.framework | `_OBJC_CLASS_$_NSPrintOperation` | 20 | adobe-acrobat-reader, android-studio, coteditor, dbeaver-community |
| ApplicationServices.framework | `_GetCurrentProcess` | 19 | adobe-acrobat-reader, android-studio, cyberduck, dbeaver-community |
| ApplicationServices.framework | `_PMRetain` | 19 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, firefox |
| Carbon.framework | `_TISSelectInputSource` | 19 | adobe-acrobat-reader, alfred, android-studio, firefox |
| AppKit.framework | `_NSApplicationLoad` | 18 | adobe-acrobat-reader, android-studio, bitwarden, blender |
| ApplicationServices.framework | `_PasteboardCopyItemFlavorData` | 18 | audacity, calibre, db-browser-for-sqlite, dolphin |
| ApplicationServices.framework | `_PasteboardCopyItemFlavors` | 18 | audacity, calibre, db-browser-for-sqlite, dolphin |
| ApplicationServices.framework | `_PasteboardGetItemCount` | 18 | audacity, calibre, db-browser-for-sqlite, dolphin |
| ApplicationServices.framework | `_PasteboardGetItemIdentifier` | 18 | audacity, calibre, db-browser-for-sqlite, dolphin |
| ApplicationServices.framework | `_PlotIconRefInContext` | 18 | adobe-acrobat-reader, audacity, calibre, db-browser-for-sqlite |
| Carbon.framework | `_kTISPropertyInputSourceType` | 18 | firefox, floorp, git-credential-manager, godot |
| AppKit.framework | `_OBJC_CLASS_$_NSStepperCell` | 17 | calibre, db-browser-for-sqlite, dolphin, keepassxc |
| ApplicationServices.framework | `_PMGetDuplex` | 17 | adobe-acrobat-reader, android-studio, calibre, db-browser-for-sqlite |
| ApplicationServices.framework | `_PasteboardClear` | 17 | audacity, calibre, db-browser-for-sqlite, dolphin |
| ApplicationServices.framework | `_PasteboardCopyName` | 17 | audacity, calibre, db-browser-for-sqlite, dolphin |
| ApplicationServices.framework | `_PasteboardPutItemFlavor` | 17 | audacity, calibre, db-browser-for-sqlite, dolphin |
| ApplicationServices.framework | `_PasteboardResolvePromises` | 17 | audacity, calibre, db-browser-for-sqlite, dolphin |
| ApplicationServices.framework | `_PasteboardSetPromiseKeeper` | 17 | audacity, calibre, db-browser-for-sqlite, dolphin |
| AppKit.framework | `_NSControlTintDidChangeNotification` | 16 | adobe-acrobat-reader, alt-tab, firefox, floorp |
| ApplicationServices.framework | `_PMSessionError` | 16 | calibre, db-browser-for-sqlite, floorp, krita |
| AppKit.framework | `_OBJC_CLASS_$_NSPrinter` | 15 | adobe-acrobat-reader, android-studio, dbeaver-community, firefox |
| AppKit.framework | `_OBJC_CLASS_$_NSViewAnimation` | 15 | firefox, floorp, iterm2, kicad |
| ApplicationServices.framework | `_PasteboardGetItemFlavorFlags` | 15 | audacity, db-browser-for-sqlite, dolphin, keepassxc |
| Carbon.framework | `_GetCurrentEventKeyModifiers` | 15 | alfred, dbeaver-community, firefox, floorp |
| Carbon.framework | `_kTISTypeKeyboardInputMode` | 15 | firefox, floorp, godot, hammerspoon |
| Carbon.framework | `_kTISTypeKeyboardLayout` | 15 | firefox, floorp, git-credential-manager, godot |
| AppKit.framework | `_NSRectClip` | 14 | alfred, bitwarden, emacs-app, gemini |
| Carbon.framework | `_GetThemeMetric` | 14 | adobe-acrobat-reader, firefox, floorp, kicad |
| Carbon.framework | `_HIThemeDrawButton` | 14 | adobe-acrobat-reader, firefox, floorp, kicad |
| ApplicationServices.framework | `_PMGetFirstPage` | 13 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, kicad |
| ApplicationServices.framework | `_PMGetLastPage` | 13 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, kicad |
| ApplicationServices.framework | `_PMServerCreatePrinterList` | 13 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, kicad |
| AppKit.framework | `_NSFrameRectWithWidthUsingOperation` | 12 | bitwarden, gemini, hex-fiend, iterm2 |
| AppKit.framework | `_NSWorkspaceRecycleOperation` | 12 | brave-browser, gemini, hex-fiend, iterm2 |
| AppKit.framework | `_OBJC_CLASS_$_NSComboBoxCell` | 12 | firefox, floorp, iterm2, kicad |
| AppKit.framework | `_OBJC_CLASS_$_NSMatrix` | 12 | adobe-acrobat-reader, bruno, emacs-app, headlamp |
| ApplicationServices.framework | `_PMGetCollate` | 12 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, kicad |
| ApplicationServices.framework | `_PMGetCopies` | 12 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, kicad |
| ApplicationServices.framework | `_PMGetUnadjustedPaperRect` | 12 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, kicad |
| ApplicationServices.framework | `_PMPrinterIsDefault` | 12 | calibre, db-browser-for-sqlite, kicad, krita |
| ApplicationServices.framework | `_PMSetPageRange` | 12 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, kicad |
| ApplicationServices.framework | `_SetFrontProcess` | 12 | adobe-acrobat-reader, deluge, firefox, floorp |
| AppKit.framework | `_NSBestDepth` | 11 | bitwarden, db-browser-for-sqlite, emacs-app, gemini |
| ApplicationServices.framework | `_PMCreatePageFormat` | 11 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, krita |
| ApplicationServices.framework | `_PMCreatePrintSettings` | 11 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, krita |
| ApplicationServices.framework | `_PMCreateSession` | 11 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, krita |
| ApplicationServices.framework | `_PMPrinterGetMimeTypes` | 11 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, krita |
| ApplicationServices.framework | `_PMPrinterGetName` | 11 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, krita |
| ApplicationServices.framework | `_PMPrinterGetOutputResolution` | 11 | calibre, db-browser-for-sqlite, kicad, krita |
| ApplicationServices.framework | `_PMSessionDefaultPageFormat` | 11 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, krita |
| ApplicationServices.framework | `_PMSessionDefaultPrintSettings` | 11 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, krita |
| Carbon.framework | `_GetEventClass` | 11 | adobe-acrobat-reader, alt-tab, flycut, openmtp |
| Carbon.framework | `_TISCopyCurrentASCIICapableKeyboardInputSource` | 11 | clipy, cmux, firefox, floorp |
| AppKit.framework | `_NSPostScriptPboardType` | 10 | adobe-acrobat-reader, android-studio, db-browser-for-sqlite, dbeaver-community |
| AppKit.framework | `_NSSpeechPitchBaseProperty` | 10 | adobe-acrobat-reader, godot, hammerspoon, mullvad-browser |
| AppKit.framework | `_OBJC_CLASS_$_NSSliderCell` | 10 | betterdisplay, iina, libreoffice, microsoft-edge |
| ApplicationServices.framework | `_PMPrintSettingsGetJobName` | 10 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, krita |
| ApplicationServices.framework | `_PMPrinterCopyDescriptionURL` | 10 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, krita |
| ApplicationServices.framework | `_PMPrinterIsRemote` | 10 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, krita |
| Carbon.framework | `_kTISPropertyInputSourceIsEnabled` | 10 | clipy, firefox, floorp, librewolf |
| AppKit.framework | `_NSAboutPanelOptionApplicationName` | 9 | cc-switch, clash-verge-rev, handy, iina |
| AppKit.framework | `_NSAboutPanelOptionApplicationVersion` | 9 | cc-switch, clash-verge-rev, handy, iina |
| AppKit.framework | `_NSPICTPboardType` | 9 | adobe-acrobat-reader, android-studio, dbeaver-community, intellij-idea |
| AppKit.framework | `_NSVCardPboardType` | 9 | android-studio, db-browser-for-sqlite, dbeaver-community, intellij-idea |
| AppKit.framework | `_NSVoiceLocaleIdentifier` | 9 | adobe-acrobat-reader, godot, mullvad-browser, openmtp |
| AppKit.framework | `_NSVoiceName` | 9 | adobe-acrobat-reader, godot, mullvad-browser, openmtp |
| AppKit.framework | `_OBJC_CLASS_$_NSSharingServicePickerTouchBarItem` | 9 | coteditor, firefox, floorp, librewolf |
| AppKit.framework | `_OBJC_METACLASS_$_NSSliderCell` | 9 | betterdisplay, iina, microsoft-edge, microsoft-teams |
| ApplicationServices.framework | `_PMPaperCreateLocalizedName` | 9 | calibre, db-browser-for-sqlite, krita, lmms |
| ApplicationServices.framework | `_PMPrinterGetLocation` | 9 | calibre, db-browser-for-sqlite, krita, lmms |
| ApplicationServices.framework | `_PMPrinterGetMakeAndModelName` | 9 | calibre, db-browser-for-sqlite, krita, lmms |
| ApplicationServices.framework | `_PMPrinterGetState` | 9 | calibre, db-browser-for-sqlite, krita, lmms |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyVMa` | 8 | cmux, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyVMn` | 8 | cmux, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10specialKeyAbCE07SpecialE0VSgvg` | 8 | cmux, codexbar, coteditor, finetune |
| AppKit.framework | `_NSAboutPanelOptionApplicationIcon` | 8 | cc-switch, clash-verge-rev, handy, iina |
| AppKit.framework | `_NSAboutPanelOptionVersion` | 8 | cc-switch, clash-verge-rev, handy, microsoft-edge |
| AppKit.framework | `_NSDisableScreenUpdates` | 8 | deluge, flycut, hammerspoon, lmms |
| AppKit.framework | `_NSEnableScreenUpdates` | 8 | deluge, flycut, hammerspoon, lmms |
| AppKit.framework | `_NSRectClipList` | 8 | bitwarden, emacs-app, gemini, monitorcontrol |
| AppKit.framework | `_NSRectFillListUsingOperation` | 8 | bitwarden, gemini, monitorcontrol, mos |
| AppKit.framework | `_NSRectFillListWithColorsUsingOperation` | 8 | bitwarden, gemini, hex-fiend, monitorcontrol |
| AppKit.framework | `_NSShowAnimationEffect` | 8 | bitwarden, gemini, iterm2, monitorcontrol |
| AppKit.framework | `_OBJC_CLASS_$_NSATSTypesetter` | 8 | adobe-acrobat-reader, bruno, headlamp, openmtp |
| AppKit.framework | `_OBJC_CLASS_$_NSSwitch` | 8 | alt-tab, iina, lulu, orbstack |
| AppKit.framework | `_OBJC_METACLASS_$_NSATSTypesetter` | 8 | adobe-acrobat-reader, bruno, headlamp, openmtp |
| ApplicationServices.framework | `_PasteboardCopyPasteLocation` | 8 | firefox, floorp, librewolf, mullvad-browser |
| Carbon.framework | `_AddEventTypesToHandler` | 8 | adobe-acrobat-reader, codexbar, finetune, maccy |
| Carbon.framework | `_RemoveEventTypesFromHandler` | 8 | adobe-acrobat-reader, codexbar, finetune, maccy |
| Carbon.framework | `_TISCopyInputSourceForLanguage` | 8 | firefox, floorp, librewolf, mullvad-browser |
| Carbon.framework | `_TSMGetActiveDocument` | 8 | firefox, floorp, librewolf, mullvad-browser |
| Carbon.framework | `_kTISPropertyBundleID` | 8 | firefox, floorp, librewolf, mullvad-browser |
| AppKit.framework | `_$sSo10NSMenuItemC6AppKitE13sectionHeader5titleABSS_tFZ` | 7 | 1password, cmux, coteditor, markedit |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyVSQACMc` | 7 | cmux, codexbar, coteditor, finetune |
| AppKit.framework | `_NSAboutPanelOptionCredits` | 7 | cc-switch, clash-verge-rev, handy, maccy |
| AppKit.framework | `_NSAvailableWindowDepths` | 7 | bitwarden, gemini, monitorcontrol, mos |
| AppKit.framework | `_NSImageNameFolderSmart` | 7 | cc-switch, clash-verge-rev, hammerspoon, handy |
| AppKit.framework | `_NSSpeechRateProperty` | 7 | adobe-acrobat-reader, mullvad-browser, openmtp, pencil |
| AppKit.framework | `_NSSpeechVolumeProperty` | 7 | adobe-acrobat-reader, mullvad-browser, openmtp, pencil |
| AppKit.framework | `_OBJC_CLASS_$_NSGestureRecognizer` | 7 | gemini, markedit, mos, obsidian |
| AppKit.framework | `_OBJC_CLASS_$_NSInputManager` | 7 | adobe-acrobat-reader, android-studio, dbeaver-community, deluge |
| AppKit.framework | `_OBJC_CLASS_$_NSMagnificationGestureRecognizer` | 7 | iina, iterm2, kicad, mos |
| AppKit.framework | `_OBJC_CLASS_$_NSPanGestureRecognizer` | 7 | kicad, markedit, mos, prusaslicer |
| ApplicationServices.framework | `_PMPrinterPrintWithFile` | 7 | adobe-acrobat-reader, firefox, floorp, librewolf |
| Carbon.framework | `_SetSystemUIMode` | 7 | adobe-acrobat-reader, gimp, inkscape, kicad |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f1AEvgZ` | 6 | cmux, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f2AEvgZ` | 6 | cmux, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f3AEvgZ` | 6 | cmux, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f4AEvgZ` | 6 | cmux, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f5AEvgZ` | 6 | cmux, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f6AEvgZ` | 6 | cmux, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f7AEvgZ` | 6 | cmux, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f8AEvgZ` | 6 | cmux, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f9AEvgZ` | 6 | cmux, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f10AEvgZ` | 6 | cmux, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f11AEvgZ` | 6 | cmux, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f12AEvgZ` | 6 | cmux, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f13AEvgZ` | 6 | cmux, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f14AEvgZ` | 6 | cmux, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f15AEvgZ` | 6 | cmux, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f16AEvgZ` | 6 | cmux, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f17AEvgZ` | 6 | cmux, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f18AEvgZ` | 6 | cmux, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f19AEvgZ` | 6 | cmux, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f20AEvgZ` | 6 | cmux, codexbar, coteditor, finetune |
| AppKit.framework | `_NSAccessibilityTextAlignmentAttribute` | 6 | godot, hammerspoon, openlogi, pixelorama |
| AppKit.framework | `_NSAccessibilityUnderlineColorTextAttribute` | 6 | godot, libreoffice, openlogi, pixelorama |
| AppKit.framework | `_NSImageNameBluetoothTemplate` | 6 | cc-switch, clash-verge-rev, hammerspoon, handy |
| AppKit.framework | `_NSImageNameFolderBurnable` | 6 | cc-switch, clash-verge-rev, hammerspoon, handy |
| AppKit.framework | `_NSImageNameIChatTheaterTemplate` | 6 | cc-switch, clash-verge-rev, hammerspoon, handy |
| AppKit.framework | `_NSImageNameRefreshFreestandingTemplate` | 6 | cc-switch, clash-verge-rev, hammerspoon, handy |
| AppKit.framework | `_NSImageNameTouchBarPauseTemplate` | 6 | hammerspoon, handbrake-app, iina, microsoft-edge |
| AppKit.framework | `_NSImageNameTouchBarPlayTemplate` | 6 | hammerspoon, handbrake-app, iina, microsoft-edge |
| AppKit.framework | `_OBJC_CLASS_$_NSPDFImageRep` | 6 | aldente, alfred, iterm2, protonvpn |
| AppKit.framework | `_OBJC_CLASS_$_NSScrubberFlowLayout` | 6 | adobe-acrobat-reader, android-studio, intellij-idea, iterm2 |
| AppKit.framework | `_OBJC_CLASS_$_NSTouch` | 6 | alt-tab, dockdoor, iterm2, mos |
| Carbon.framework | `_GetCurrentEventQueue` | 6 | adobe-acrobat-reader, openmtp, pencil, virtualbox |
| Carbon.framework | `_GetEventTime` | 6 | adobe-acrobat-reader, kicad, openmtp, pencil |
| Carbon.framework | `_HIThemeBeginFocus` | 6 | android-studio, cyberduck, dbeaver-community, intellij-idea |
| Carbon.framework | `_HIThemeDrawTrack` | 6 | kicad, openscad, pgadmin4, prusaslicer |
| Carbon.framework | `_HIThemeEndFocus` | 6 | android-studio, cyberduck, dbeaver-community, intellij-idea |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV13deleteForwardAEvgZ` | 5 | codexbar, coteditor, finetune, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f21AEvgZ` | 5 | codexbar, coteditor, finetune, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f22AEvgZ` | 5 | codexbar, coteditor, finetune, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f23AEvgZ` | 5 | codexbar, coteditor, finetune, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f24AEvgZ` | 5 | codexbar, coteditor, finetune, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f25AEvgZ` | 5 | codexbar, coteditor, finetune, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f26AEvgZ` | 5 | codexbar, coteditor, finetune, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f27AEvgZ` | 5 | codexbar, coteditor, finetune, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f28AEvgZ` | 5 | codexbar, coteditor, finetune, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f29AEvgZ` | 5 | codexbar, coteditor, finetune, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f30AEvgZ` | 5 | codexbar, coteditor, finetune, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f31AEvgZ` | 5 | codexbar, coteditor, finetune, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f32AEvgZ` | 5 | codexbar, coteditor, finetune, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f33AEvgZ` | 5 | codexbar, coteditor, finetune, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f34AEvgZ` | 5 | codexbar, coteditor, finetune, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f35AEvgZ` | 5 | codexbar, coteditor, finetune, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3tabAEvgZ` | 5 | codexbar, coteditor, finetune, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV6deleteAEvgZ` | 5 | codexbar, coteditor, finetune, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV9backspaceAEvgZ` | 5 | codexbar, coteditor, finetune, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyVSHACMc` | 5 | codexbar, coteditor, finetune, maccy |
| AppKit.framework | `_NSAccessibilityLanguageTextAttribute` | 5 | godot, openlogi, pixelorama, unity-hub |
| AppKit.framework | `_NSAccessibilityStrikethroughColorTextAttribute` | 5 | godot, openlogi, pixelorama, unity-hub |
| AppKit.framework | `_OBJC_CLASS_$_NSLevelIndicatorCell` | 5 | kicad, prusaslicer, transmission, zoom |
| AppKit.framework | `_OBJC_CLASS_$_NSTextBlock` | 5 | alfred, iterm2, raycast, telegram |
| AppKit.framework | `_OBJC_CLASS_$_NSTreeNode` | 5 | gemini, orbstack, sequel-ace, sourcetree |
| Carbon.framework | `_GetCurrentEvent` | 5 | adobe-acrobat-reader, kicad, openmtp, pencil |
| Carbon.framework | `_GetEventMonitorTarget` | 5 | adobe-acrobat-reader, alfred, openmtp, pencil |
| Carbon.framework | `_GetMainEventQueue` | 5 | adobe-acrobat-reader, openmtp, openscad, pencil |
| Carbon.framework | `_HIThemeDrawFrame` | 5 | kicad, libreoffice, openscad, pgadmin4 |
| Carbon.framework | `_HIThemeDrawGroupBox` | 5 | adobe-acrobat-reader, kicad, openscad, pgadmin4 |
| QuickLookThumbnailing.framework | `_OBJC_CLASS_$_QLThumbnailGenerationRequest` | 5 | iina, raycast, shottr, tailscale-app |
| QuickLookThumbnailing.framework | `_OBJC_CLASS_$_QLThumbnailGenerator` | 5 | iina, raycast, shottr, tailscale-app |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyVMa` | 5 | betterdisplay, iterm2, raycast, shottr |
| AppKit.framework | `_NSBitsPerSampleFromDepth` | 4 | adobe-acrobat-reader, db-browser-for-sqlite, krita, lmms |
| AppKit.framework | `_NSColorSpaceFromDepth` | 4 | db-browser-for-sqlite, emacs-app, krita, lmms |
| AppKit.framework | `_NSInkTextPboardType` | 4 | db-browser-for-sqlite, krita, lmms, openscad |
| AppKit.framework | `_NSMultipleTextSelectionPboardType` | 4 | db-browser-for-sqlite, krita, lmms, openscad |
| AppKit.framework | `_NSRunAlertPanel` | 4 | cyberduck, flycut, libreoffice, xquartz |
| AppKit.framework | `_OBJC_CLASS_$_NSController` | 4 | alfred, alt-tab, keka, sourcetree |
| AppKit.framework | `_OBJC_CLASS_$_NSRotationGestureRecognizer` | 4 | kicad, mos, prusaslicer, tableplus |
| ApplicationServices.framework | `_DisposeSpeechChannel` | 4 | adobe-acrobat-reader, openmtp, pencil, webtorrent |
| ApplicationServices.framework | `_HIShapeCreateMutable` | 4 | kicad, openscad, pgadmin4, prusaslicer |
| ApplicationServices.framework | `_HIShapeEnumerate` | 4 | kicad, openscad, pgadmin4, prusaslicer |
| ApplicationServices.framework | `_HIShapeUnionWithRect` | 4 | kicad, openscad, pgadmin4, prusaslicer |
| ApplicationServices.framework | `_NewSpeechChannel` | 4 | adobe-acrobat-reader, openmtp, pencil, webtorrent |
| ApplicationServices.framework | `_SpeakCFString` | 4 | adobe-acrobat-reader, openmtp, pencil, webtorrent |
| ApplicationServices.framework | `_SpeechBusy` | 4 | adobe-acrobat-reader, openmtp, pencil, webtorrent |
| Carbon.framework | `_CopyEvent` | 4 | adobe-acrobat-reader, openmtp, pencil, webtorrent |
| Carbon.framework | `_CreateEvent` | 4 | adobe-acrobat-reader, openmtp, pencil, webtorrent |
| Carbon.framework | `_HIThemeBrushCreateCGColor` | 4 | kicad, openscad, pgadmin4, prusaslicer |
| Carbon.framework | `_HIThemeDrawSeparator` | 4 | kicad, openscad, pgadmin4, zotero |
| Carbon.framework | `_HIThemeGetButtonBackgroundBounds` | 4 | kicad, openscad, pgadmin4, webtorrent |
| Carbon.framework | `_PopSymbolicHotKeyMode` | 4 | anydesk, flycut, sequel-ace, sourcetree |
| Carbon.framework | `_PostEventToQueue` | 4 | adobe-acrobat-reader, openmtp, pencil, webtorrent |
| Carbon.framework | `_PushSymbolicHotKeyMode` | 4 | anydesk, flycut, sequel-ace, sourcetree |
| Carbon.framework | `_RetainEvent` | 4 | adobe-acrobat-reader, openmtp, pencil, webtorrent |
| Carbon.framework | `_SendEventToEventTargetWithOptions` | 4 | adobe-acrobat-reader, openmtp, pencil, webtorrent |
| Carbon.framework | `_SetEventParameter` | 4 | adobe-acrobat-reader, openmtp, pencil, webtorrent |
| Carbon.framework | `_SetEventTime` | 4 | adobe-acrobat-reader, openmtp, pencil, webtorrent |
| Carbon.framework | `_kTISNotifyEnabledKeyboardInputSourcesChanged` | 4 | clipy, iterm2, linearmouse, maccy |
| Carbon.framework | `_kTISPropertyInputSourceIsSelectCapable` | 4 | clipy, hammerspoon, maccy, vorssaint |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyVMn` | 4 | betterdisplay, iterm2, raycast, shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyVSQACMc` | 4 | betterdisplay, iterm2, raycast, shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10specialKeyAbCE07SpecialE0VSgvg` | 4 | betterdisplay, iterm2, raycast, shottr |
| AppKit.framework | `_$sSo7NSImageC6AppKitE8resourceAB21DeveloperToolsSupport13ImageResourceV_tcfC` | 3 | coteditor, jordanbaird-ice, thaw |
| AppKit.framework | `_NSAccessibilityRoleDescriptionForUIElement` | 3 | iterm2, warp, zoom |
| AppKit.framework | `_NSAccessibilityUnignoredDescendant` | 3 | kicad, prusaslicer, zoom |
| AppKit.framework | `_NSAllRomanInputSourcesLocaleIdentifier` | 3 | coteditor, tableplus, zoom |
| AppKit.framework | `_NSImageNameTouchBarExitFullScreenTemplate` | 3 | hammerspoon, handbrake-app, iina |
| AppKit.framework | `_NSImageNameTouchBarGoDownTemplate` | 3 | hammerspoon, iterm2, sublime-text |
| AppKit.framework | `_NSImageNameTouchBarGoUpTemplate` | 3 | hammerspoon, iterm2, sublime-text |
| AppKit.framework | `_NSImageNameTouchBarPlayPauseTemplate` | 3 | hammerspoon, microsoft-edge, microsoft-teams |
| AppKit.framework | `_NSImageNameTouchBarRecordStopTemplate` | 3 | hammerspoon, handbrake-app, zoom |
| AppKit.framework | `_NSOutlineViewDisclosureButtonKey` | 3 | iterm2, sourcetree, tableplus |
| AppKit.framework | `_NSSharingServiceNameSendViaAirDrop` | 3 | hammerspoon, microsoft-edge, microsoft-teams |
| AppKit.framework | `_NSSpeechResetProperty` | 3 | godot, hammerspoon, pixelorama |
| AppKit.framework | `_NSTextContentTypeOneTimeCode` | 3 | cmux, multipass, xcodes-app |
| AppKit.framework | `_NSTextContentTypePassword` | 3 | cmux, multipass, xcodes-app |
| AppKit.framework | `_NSTextContentTypeUsername` | 3 | cmux, multipass, xcodes-app |
| AppKit.framework | `_NSVoiceIdentifier` | 3 | mullvad-browser, tor-browser, zotero |
| AppKit.framework | `_NSWorkspaceLaunchConfigurationEnvironment` | 3 | 1password, godot, pixelorama |
| AppKit.framework | `_OBJC_CLASS_$_NSCollectionViewGridLayout` | 3 | protonvpn, windows-app, zoom |
| AppKit.framework | `_OBJC_CLASS_$_NSCustomImageRep` | 3 | flycut, sequel-ace, sourcetree |
| AppKit.framework | `_OBJC_CLASS_$_NSEPSImageRep` | 3 | iterm2, libreoffice, protonvpn |
| AppKit.framework | `_OBJC_CLASS_$_NSFontCollection` | 3 | alfred, iterm2, markedit |
| AppKit.framework | `_OBJC_CLASS_$_NSHelpManager` | 3 | coteditor, sequel-ace, transmission |
| AppKit.framework | `_OBJC_CLASS_$_NSPressGestureRecognizer` | 3 | iterm2, kicad, prusaslicer |
| AppKit.framework | `_OBJC_CLASS_$_NSSliderAccessory` | 3 | adobe-acrobat-reader, coteditor, spotify |
| AppKit.framework | `_OBJC_METACLASS_$_NSController` | 3 | alfred, keka, sourcetree |
| AppKit.framework | `_OBJC_METACLASS_$_NSMatrix` | 3 | adobe-acrobat-reader, windows-app, zoom |
| AppKit.framework | `_OBJC_METACLASS_$_NSTreeNode` | 3 | sequel-ace, sourcetree, zoom |
| ApplicationServices.framework | `_CoreDockSendNotification` | 3 | hammerspoon, linearmouse, raycast |
| ApplicationServices.framework | `_GetNextProcess` | 3 | adobe-acrobat-reader, flycut, vlc |
| ApplicationServices.framework | `_HIShapeCreateIntersection` | 3 | kicad, pgadmin4, prusaslicer |
| ApplicationServices.framework | `_HIShapeCreateMutableCopy` | 3 | kicad, pgadmin4, prusaslicer |
| ApplicationServices.framework | `_HIShapeCreateWithRect` | 3 | kicad, pgadmin4, prusaslicer |
| ApplicationServices.framework | `_HIShapeDifference` | 3 | kicad, pgadmin4, prusaslicer |
| ApplicationServices.framework | `_HIShapeGetBounds` | 3 | kicad, pgadmin4, prusaslicer |
| ApplicationServices.framework | `_HIShapeIntersect` | 3 | kicad, pgadmin4, prusaslicer |
| ApplicationServices.framework | `_HIShapeIsEmpty` | 3 | kicad, pgadmin4, prusaslicer |
| ApplicationServices.framework | `_HIShapeIsRectangular` | 3 | kicad, pgadmin4, prusaslicer |
| ApplicationServices.framework | `_HIShapeOffset` | 3 | kicad, pgadmin4, prusaslicer |
| ApplicationServices.framework | `_HIShapeReplacePathInCGContext` | 3 | kicad, pgadmin4, prusaslicer |
| ApplicationServices.framework | `_HIShapeUnion` | 3 | kicad, pgadmin4, prusaslicer |
| ApplicationServices.framework | `_PMGetPageRange` | 3 | adobe-acrobat-reader, kicad, prusaslicer |
| ApplicationServices.framework | `_PMGetUnadjustedPageRect` | 3 | adobe-acrobat-reader, kicad, prusaslicer |
| ApplicationServices.framework | `_ProcessInformationCopyDictionary` | 3 | adobe-acrobat-reader, lulu, vlc |
| ApplicationServices.framework | `_StopSpeechAt` | 3 | openmtp, pencil, webtorrent |
| Carbon.framework | `_AESend` | 3 | adobe-acrobat-reader, deluge, qbittorrent |
| Carbon.framework | `_GetThemeMenuSeparatorHeight` | 3 | kicad, openscad, pgadmin4 |
| Carbon.framework | `_HIThemeDrawGrowBox` | 3 | kicad, openscad, pgadmin4 |
| Carbon.framework | `_HIThemeDrawPaneSplitter` | 3 | kicad, openscad, prusaslicer |
| Carbon.framework | `_HIThemeDrawTab` | 3 | kicad, openscad, pgadmin4 |
| Carbon.framework | `_HIThemeDrawTabPane` | 3 | kicad, openscad, pgadmin4 |
| Carbon.framework | `_HIThemeGetButtonContentBounds` | 3 | kicad, openscad, pgadmin4 |
| Carbon.framework | `_HIThemeGetGrowBoxBounds` | 3 | kicad, openscad, pgadmin4 |
| Carbon.framework | `_HIThemeSetFill` | 3 | kicad, openscad, pgadmin4 |
| Carbon.framework | `_TISDisableInputSource` | 3 | android-studio, intellij-idea, jetbrains-toolbox |
| Carbon.framework | `_TISEnableInputSource` | 3 | android-studio, intellij-idea, jetbrains-toolbox |
| Carbon.framework | `_kTISCategoryKeyboardInputSource` | 3 | alfred, iterm2, vorssaint |
| Carbon.framework | `_kTISPropertyInputModeID` | 3 | clipy, git-credential-manager, maccy |
| Carbon.framework | `_kTISPropertyInputSourceCategory` | 3 | alfred, iterm2, vorssaint |
| Carbon.framework | `_kTISPropertyInputSourceIsSelected` | 3 | clipy, hammerspoon, maccy |
| QuickLook.framework | `_QLThumbnailImageCreate` | 3 | alfred, gemini, sourcetree |
| QuickLookThumbnailing.framework | `_OBJC_CLASS_$_QLThumbnailProvider` | 3 | blender, krita, libreoffice |
| QuickLookThumbnailing.framework | `_OBJC_CLASS_$_QLThumbnailReply` | 3 | blender, krita, libreoffice |
| QuickLookThumbnailing.framework | `_OBJC_METACLASS_$_QLThumbnailProvider` | 3 | blender, krita, libreoffice |
| QuickLookUI.framework | `_OBJC_CLASS_$_QLPreviewProvider` | 3 | krita, qlmarkdown, transmission |
| QuickLookUI.framework | `_OBJC_CLASS_$_QLPreviewReply` | 3 | krita, qlmarkdown, transmission |
| QuickLookUI.framework | `_OBJC_METACLASS_$_QLPreviewProvider` | 3 | krita, qlmarkdown, transmission |
| libswiftAppKit.dylib | `_$sSo13NSWindowLevela6AppKit01_cdD23NumericRawRepresentableACMc` | 3 | betterdisplay, raycast, zoom |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV015BackgroundColorB0OAA19AttributedStringKeyADMc` | 2 | coteditor, maccy |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV015BackgroundColorB0ON` | 2 | coteditor, maccy |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesVMn` | 2 | coteditor, protonvpn |
| AppKit.framework | `_$sSo13NSWindowLevela6AppKit01_cdD23NumericRawRepresentableACMc` | 2 | jordanbaird-ice, thaw |
| AppKit.framework | `_$sSo6CGRectV6AppKitE4clipyyF` | 2 | jordanbaird-ice, thaw |
| AppKit.framework | `_$sSo7NSColorC6AppKitE8resourceAB21DeveloperToolsSupport13ColorResourceV_tcfC` | 2 | coteditor, thaw |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV14carriageReturnAEvgZ` | 2 | coteditor, orbstack |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV5enterAEvgZ` | 2 | coteditor, orbstack |
| AppKit.framework | `_NSDrawThreePartImage` | 2 | alt-tab, vlc |
| AppKit.framework | `_NSFontDescriptorSystemDesignRounded` | 2 | cmux, markedit |
| AppKit.framework | `_NSFontTextStyleCaption1` | 2 | 1password, orbstack |
| AppKit.framework | `_NSGetAlertPanel` | 2 | steam, vlc |
| AppKit.framework | `_NSImageNameTouchBarAddDetailTemplate` | 2 | gemini, hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarBookmarksTemplate` | 2 | hammerspoon, sublime-text |
| AppKit.framework | `_NSImageNameTouchBarColorPickerFill` | 2 | hammerspoon, iterm2 |
| AppKit.framework | `_NSImageNameTouchBarCommunicationAudioTemplate` | 2 | hammerspoon, zoom |
| AppKit.framework | `_NSImageNameTouchBarCommunicationVideoTemplate` | 2 | hammerspoon, zoom |
| AppKit.framework | `_NSImageNameTouchBarEnterFullScreenTemplate` | 2 | hammerspoon, handbrake-app |
| AppKit.framework | `_NSImageNameTouchBarFastForwardTemplate` | 2 | hammerspoon, iina |
| AppKit.framework | `_NSImageNameTouchBarIconViewTemplate` | 2 | hammerspoon, monitorcontrol |
| AppKit.framework | `_NSImageNameTouchBarQuickLookTemplate` | 2 | hammerspoon, handbrake-app |
| AppKit.framework | `_NSImageNameTouchBarRewindTemplate` | 2 | hammerspoon, iina |
| AppKit.framework | `_NSImageNameTouchBarSearchTemplate` | 2 | hammerspoon, zoom |
| AppKit.framework | `_NSImageNameTouchBarSkipAhead15SecondsTemplate` | 2 | hammerspoon, iina |
| AppKit.framework | `_NSImageNameTouchBarSkipAhead30SecondsTemplate` | 2 | hammerspoon, iina |
| AppKit.framework | `_NSImageNameTouchBarSkipAheadTemplate` | 2 | hammerspoon, iina |
| AppKit.framework | `_NSImageNameTouchBarSkipBack15SecondsTemplate` | 2 | hammerspoon, iina |
| AppKit.framework | `_NSImageNameTouchBarSkipBack30SecondsTemplate` | 2 | hammerspoon, iina |
| AppKit.framework | `_NSImageNameTouchBarSkipBackTemplate` | 2 | hammerspoon, iina |
| AppKit.framework | `_NSImageNameTouchBarVolumeDownTemplate` | 2 | hammerspoon, iina |
| AppKit.framework | `_NSImageNameTouchBarVolumeUpTemplate` | 2 | hammerspoon, iina |
| AppKit.framework | `_NSRectFillListWithColors` | 2 | hex-fiend, transmission |
| AppKit.framework | `_NSReleaseAlertPanel` | 2 | steam, vlc |
| AppKit.framework | `_NSSelectedTagBinding` | 2 | iina, scroll-reverser |
| AppKit.framework | `_NSSelectionIndexPathsBinding` | 2 | gemini, orbstack |
| AppKit.framework | `_NSSharingServiceNamePostOnTwitter` | 2 | adobe-acrobat-reader, zotero |
| AppKit.framework | `_NSSplitViewItemUnspecifiedDimension` | 2 | coteditor, zoom |
| AppKit.framework | `_NSTextListMarkerDisc` | 2 | raycast, zoom |
| AppKit.framework | `_NSViewNoInstrinsicMetric` | 2 | gemini, zoom |
| AppKit.framework | `_NSWindowDidOrderOnScreenNotification` | 2 | kicad, pgadmin4 |
| AppKit.framework | `_NSWindowWillOrderOnScreenNotification` | 2 | kicad, pgadmin4 |
| AppKit.framework | `_OBJC_CLASS_$_NSGlassEffectView` | 2 | markedit, thaw |
| AppKit.framework | `_OBJC_CLASS_$_NSMenuItemBadge` | 2 | cmux, coteditor |
| AppKit.framework | `_OBJC_CLASS_$_NSPICTImageRep` | 2 | adobe-acrobat-reader, sequel-ace |
| AppKit.framework | `_OBJC_CLASS_$_NSPressureConfiguration` | 2 | telegram, utm |
| AppKit.framework | `_OBJC_CLASS_$_NSRuleEditor` | 2 | coteditor, sequel-ace |
| AppKit.framework | `_OBJC_CLASS_$_NSTextLineFragment` | 2 | coteditor, raycast |
| AppKit.framework | `_OBJC_CLASS_$_NSTreeController` | 2 | gemini, orbstack |
| AppKit.framework | `_OBJC_METACLASS_$_NSComboBoxCell` | 2 | iterm2, sequel-ace |
| AppKit.framework | `_OBJC_METACLASS_$_NSLevelIndicatorCell` | 2 | transmission, zoom |
| AppKit.framework | `_OBJC_METACLASS_$_NSSwitch` | 2 | iina, tailscale-app |
| AppKit.framework | `_OBJC_METACLASS_$_NSTextBlock` | 2 | alfred, raycast |
| AppKit.framework | `__NSPrependToKillRing` | 2 | calibre, vnote |
| ApplicationServices.framework | `_CoreDockGetAutoHideEnabled` | 2 | dockdoor, ghostty |
| ApplicationServices.framework | `_CoreDockGetOrientationAndPinning` | 2 | dockdoor, ghostty |
| ApplicationServices.framework | `_GetFrontProcess` | 2 | adobe-acrobat-reader, zoom |
| ApplicationServices.framework | `_HIShapeContainsPoint` | 2 | kicad, prusaslicer |
| ApplicationServices.framework | `_HIShapeCreateCopy` | 2 | kicad, pgadmin4 |
| ApplicationServices.framework | `_HIShapeCreateDifference` | 2 | kicad, pgadmin4 |
| ApplicationServices.framework | `_HIShapeCreateEmpty` | 2 | kicad, pgadmin4 |
| ApplicationServices.framework | `_HIShapeCreateMutableWithRect` | 2 | kicad, pgadmin4 |
| ApplicationServices.framework | `_HIShapeCreateUnion` | 2 | kicad, prusaslicer |
| ApplicationServices.framework | `_HIShapeIntersectsRect` | 2 | kicad, pgadmin4 |
| ApplicationServices.framework | `_HIShapeSetEmpty` | 2 | kicad, pgadmin4 |
| ApplicationServices.framework | `_HIShapeSetWithShape` | 2 | kicad, pgadmin4 |
| ApplicationServices.framework | `_PMWorkflowSubmitPDFWithSettings` | 2 | firefox, librewolf |
| ApplicationServices.framework | `_PasteboardSetPasteLocation` | 2 | kicad, prusaslicer |
| ApplicationServices.framework | `_SetFrontProcessWithOptions` | 2 | hammerspoon, zoom |
| Carbon.framework | `_AEInteractWithUser` | 2 | adobe-acrobat-reader, deluge |
| Carbon.framework | `_AEResumeTheCurrentEvent` | 2 | adobe-acrobat-reader, deluge |
| Carbon.framework | `_AESuspendTheCurrentEvent` | 2 | adobe-acrobat-reader, deluge |
| Carbon.framework | `_AcquireFirstMatchingEventInQueue` | 2 | virtualbox, zotero |
| Carbon.framework | `_GetCurrentButtonState` | 2 | adobe-acrobat-reader, kicad |
| Carbon.framework | `_GetCurrentEventButtonState` | 2 | blender, kicad |
| Carbon.framework | `_GetThemeMenuItemExtra` | 2 | kicad, pgadmin4 |
| Carbon.framework | `_HIThemeApplyBackground` | 2 | kicad, pgadmin4 |
| Carbon.framework | `_HIThemeDrawFocusRect` | 2 | kicad, prusaslicer |
| Carbon.framework | `_HIThemeDrawMenuBackground` | 2 | libreoffice, webtorrent |
| Carbon.framework | `_HIThemeDrawMenuItem` | 2 | libreoffice, openscad |
| Carbon.framework | `_HIThemeDrawTextBox` | 2 | libreoffice, openscad |
| Carbon.framework | `_HIThemeGetUIFontType` | 2 | kicad, pgadmin4 |
| Carbon.framework | `_HIThemeSetStroke` | 2 | kicad, pgadmin4 |
| Carbon.framework | `_HIThemeSetTextFill` | 2 | kicad, pgadmin4 |
| Carbon.framework | `_kTISPropertyInputSourceIsEnableCapable` | 2 | clipy, maccy |
| QuickLook.framework | `_QLThumbnailCopyImage` | 2 | openmtp, pencil |
| QuickLook.framework | `_QLThumbnailCreate` | 2 | openmtp, pencil |
| QuickLook.framework | `_QLThumbnailDispatchAsync` | 2 | openmtp, pencil |
| libswiftAppKit.dylib | `_$sSo17NSAnimationEffectV6AppKitE4show10centeredAt4size17completionHandlerySo7CGPointV_So6CGSizeVyyctF` | 2 | iina, iterm2 |
| libswiftAppKit.dylib | `_$sSo6CGRectV6AppKitE4clipyyF` | 2 | iterm2, sequel-ace |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV13deleteForwardAEvgZ` | 2 | betterdisplay, shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f1AEvgZ` | 2 | betterdisplay, shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f2AEvgZ` | 2 | betterdisplay, shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f3AEvgZ` | 2 | betterdisplay, shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f4AEvgZ` | 2 | betterdisplay, shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f5AEvgZ` | 2 | betterdisplay, shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f6AEvgZ` | 2 | betterdisplay, shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f7AEvgZ` | 2 | betterdisplay, shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f8AEvgZ` | 2 | betterdisplay, shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f9AEvgZ` | 2 | betterdisplay, shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f10AEvgZ` | 2 | betterdisplay, shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f11AEvgZ` | 2 | betterdisplay, shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f12AEvgZ` | 2 | betterdisplay, shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f13AEvgZ` | 2 | betterdisplay, shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f14AEvgZ` | 2 | betterdisplay, shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f15AEvgZ` | 2 | betterdisplay, shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f16AEvgZ` | 2 | betterdisplay, shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f17AEvgZ` | 2 | betterdisplay, shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f18AEvgZ` | 2 | betterdisplay, shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f19AEvgZ` | 2 | betterdisplay, shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f20AEvgZ` | 2 | betterdisplay, shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3tabAEvgZ` | 2 | betterdisplay, shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV6deleteAEvgZ` | 2 | betterdisplay, shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV7upArrowAEvgZ` | 2 | iterm2, raycast |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV9backspaceAEvgZ` | 2 | betterdisplay, shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV9downArrowAEvgZ` | 2 | iterm2, raycast |
| libswiftQuickLookUI.dylib | `_$sSo14QLPreviewReplyC11QuickLookUIE17dataOfContentType11contentSize15createDataUsingAB07UniformI11Identifiers6UTTypeV_So6CGSizeV10Foundation0M0VABKctcfC` | 2 | krita, qlmarkdown |
| Accessibility.framework | `_$s13Accessibility01_A13NotificationsPAAE4postyyF` | 1 | coteditor |
| Accessibility.framework | `_$s13Accessibility0A12NotificationO12AnnouncementVAA01_A13NotificationsAAWP` | 1 | coteditor |
| Accessibility.framework | `_$s13Accessibility0A12NotificationO12AnnouncementVMa` | 1 | coteditor |
| Accessibility.framework | `_$s13Accessibility0A12NotificationO12AnnouncementVyAESScfC` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE03appE0AcDE0dE10AttributesVmvg` | 1 | protonvpn |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE03appE0AcDE0dE10AttributesVmvpMV` | 1 | protonvpn |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV015BackgroundColorB0OMn` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV015ForegroundColorB0OAA19AttributedStringKeyADMc` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV015ForegroundColorB0OMn` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV015ForegroundColorB0ON` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV15backgroundColorAF010BackgroundhB0Ovg` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV15backgroundColorAF010BackgroundhB0OvpMV` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV15foregroundColorAF010ForegroundhB0Ovg` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV15foregroundColorAF010ForegroundhB0OvpMV` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesVAA0B5ScopeADMc` | 1 | protonvpn |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesVMa` | 1 | protonvpn |
| AppKit.framework | `_$s10Foundation22AttributeDynamicLookupO6AppKitE13dynamicMemberxs7KeyPathCyAA0B6ScopesOADE0eF10AttributesVxG_tcAA016AttributedStringI0Rzluig` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation22AttributeDynamicLookupO6AppKitE13dynamicMemberxs7KeyPathCyAA0B6ScopesOADE0eF10AttributesVxG_tcAA016AttributedStringI0RzluipMV` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation9IndexPathV6AppKitE4itemSivM` | 1 | windows-app |
| AppKit.framework | `_$s10Foundation9IndexPathV6AppKitE4itemSivs` | 1 | windows-app |
| AppKit.framework | `_$s6AppKit29NSTableViewDiffableDataSourceC05tableD012cellProviderACyxq_GSo0cD0C_So6NSViewCAH_So0C6ColumnCSiq_tctcfc` | 1 | windows-app |
| AppKit.framework | `_$s6AppKit29NSTableViewDiffableDataSourceC5apply_20animatingDifferences10completionyAA010NSDiffablefG8SnapshotVyxq_G_SbyycSgtFTj` | 1 | windows-app |
| AppKit.framework | `_$s6AppKit29NSTableViewDiffableDataSourceCMn` | 1 | windows-app |
| AppKit.framework | `_$s6AppKit34NSCollectionViewDiffableDataSourceC013supplementaryD8ProviderSo0cD7Element_So6NSViewCXcSgSo0cD0C_SS10Foundation9IndexPathVtcSgvsTj` | 1 | windows-app |
| AppKit.framework | `_$sSo10NSGradientC6AppKitE18colorsAndLocationsABSgSo7NSColorC_12CoreGraphics7CGFloatVtd_tcfC` | 1 | cmux |
| AppKit.framework | `_$sSo12NSPasteboardC6AppKitE14DetectedValuesV5linksSaySo11DDMatchLinkCGvg` | 1 | markedit |
| AppKit.framework | `_$sSo12NSPasteboardC6AppKitE14DetectedValuesV5linksSaySo11DDMatchLinkCGvpMV` | 1 | markedit |
| AppKit.framework | `_$sSo12NSPasteboardC6AppKitE14DetectedValuesVMa` | 1 | markedit |
| AppKit.framework | `_$sSo12NSPasteboardC6AppKitE14DetectedValuesVMn` | 1 | markedit |
| AppKit.framework | `_$sSo12NSPasteboardC6AppKitE14detectedValues3forAbCE08DetectedE0VShys14PartialKeyPathCyAGGG_tYaKF` | 1 | markedit |
| AppKit.framework | `_$sSo12NSPasteboardC6AppKitE14detectedValues3forAbCE08DetectedE0VShys14PartialKeyPathCyAGGG_tYaKFTu` | 1 | markedit |
| AppKit.framework | `_$sSo12NSStoryboardC6AppKitE28instantiateInitialController7creatorxSgAFSo7NSCoderCcSg_tSo06NSViewF0CRbzlF` | 1 | coteditor |
| AppKit.framework | `_$sSo16NSViewControllerC6AppKitE11ViewLoadingV18_enclosingInstance7wrapped7storagexqd___s24ReferenceWritableKeyPathCyqd__xGAJyqd__AEy_xGGtcABRbd__luigZ` | 1 | coteditor |
| AppKit.framework | `_$sSo16NSViewControllerC6AppKitE11ViewLoadingV18_enclosingInstance7wrapped7storagexqd___s24ReferenceWritableKeyPathCyqd__xGAJyqd__AEy_xGGtcABRbd__luisZ` | 1 | coteditor |
| AppKit.framework | `_$sSo16NSViewControllerC6AppKitE11ViewLoadingVAEy_xGycfC` | 1 | coteditor |
| AppKit.framework | `_$sSo16NSViewControllerC6AppKitE11ViewLoadingVMa` | 1 | coteditor |
| AppKit.framework | `_$sSo16NSViewControllerC6AppKitE11ViewLoadingVMn` | 1 | coteditor |
| AppKit.framework | `_$sSo6NSViewC6AppKitE10edgeInsets3forSo06NSEdgeE0VAbCE12LayoutRegionV_tF` | 1 | coteditor |
| AppKit.framework | `_$sSo6NSViewC6AppKitE12InvalidatingV12wrappedValue_AEy_xq_Gx_q_tcfC` | 1 | coteditor |
| AppKit.framework | `_$sSo6NSViewC6AppKitE12InvalidatingV18_enclosingInstance7wrapped7storagexqd___s24ReferenceWritableKeyPathCyqd__xGAJyqd__AEy_xq_GGtcABRbd__luigZ` | 1 | coteditor |
| AppKit.framework | `_$sSo6NSViewC6AppKitE12InvalidatingV18_enclosingInstance7wrapped7storagexqd___s24ReferenceWritableKeyPathCyqd__xGAJyqd__AEy_xq_GGtcABRbd__luisZ` | 1 | coteditor |
| AppKit.framework | `_$sSo6NSViewC6AppKitE12InvalidatingVMa` | 1 | coteditor |
| AppKit.framework | `_$sSo6NSViewC6AppKitE12InvalidatingVMn` | 1 | coteditor |
| AppKit.framework | `_$sSo6NSViewC6AppKitE12LayoutRegionV14AdaptivityAxisOMa` | 1 | coteditor |
| AppKit.framework | `_$sSo6NSViewC6AppKitE12LayoutRegionV14AdaptivityAxisOMn` | 1 | coteditor |
| AppKit.framework | `_$sSo6NSViewC6AppKitE12LayoutRegionV7margins16cornerAdaptationA2E14AdaptivityAxisOSg_tFZ` | 1 | coteditor |
| AppKit.framework | `_$sSo6NSViewC6AppKitE12LayoutRegionVMa` | 1 | coteditor |
| AppKit.framework | `_$sSo6NSViewC6AppKitE13InvalidationsO7DisplayVAC0A12InvalidatingACWP` | 1 | coteditor |
| AppKit.framework | `_$sSo6NSViewC6AppKitE13InvalidationsO7DisplayVAGycfC` | 1 | coteditor |
| AppKit.framework | `_$sSo6NSViewC6AppKitE13InvalidationsO7DisplayVMa` | 1 | coteditor |
| AppKit.framework | `_$sSo6NSViewC6AppKitE13InvalidationsO7DisplayVMn` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV10rightArrowAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV12clearDisplayAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV13unicodeScalars7UnicodeO0G0Vvg` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3endAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV4helpAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV4homeAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV6pageUpAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV7backTabAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV7newlineAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV7upArrowAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV8formFeedAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV8pageDownAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV8rawValueAESi_tcfC` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV9clearLineAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV9downArrowAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV9leftArrowAEvgZ` | 1 | coteditor |
| AppKit.framework | `_NSAccessibilityAnnotationTextAttribute` | 1 | hammerspoon |
| AppKit.framework | `_NSAccessibilityCenterTabStopMarkerTypeValue` | 1 | hammerspoon |
| AppKit.framework | `_NSAccessibilityCentimetersUnitValue` | 1 | hammerspoon |
| AppKit.framework | `_NSAccessibilityCustomTextAttribute` | 1 | hammerspoon |
| AppKit.framework | `_NSAccessibilityDecimalTabStopMarkerTypeValue` | 1 | hammerspoon |
| AppKit.framework | `_NSAccessibilityFirstLineIndentMarkerTypeValue` | 1 | hammerspoon |
| AppKit.framework | `_NSAccessibilityHeadIndentMarkerTypeValue` | 1 | hammerspoon |
| AppKit.framework | `_NSAccessibilityInchesUnitValue` | 1 | hammerspoon |
| AppKit.framework | `_NSAccessibilityLeftTabStopMarkerTypeValue` | 1 | hammerspoon |
| AppKit.framework | `_NSAccessibilityMisspelledTextAttribute` | 1 | libreoffice |
| AppKit.framework | `_NSAccessibilityPicasUnitValue` | 1 | hammerspoon |
| AppKit.framework | `_NSAccessibilityPointsUnitValue` | 1 | hammerspoon |
| AppKit.framework | `_NSAccessibilityRightTabStopMarkerTypeValue` | 1 | hammerspoon |
| AppKit.framework | `_NSAccessibilityShadowTextAttribute` | 1 | libreoffice |
| AppKit.framework | `_NSAccessibilityTailIndentMarkerTypeValue` | 1 | hammerspoon |
| AppKit.framework | `_NSAccessibilityUnknownMarkerTypeValue` | 1 | hammerspoon |
| AppKit.framework | `_NSAccessibilityUnknownSubrole` | 1 | vorssaint |
| AppKit.framework | `_NSAccessibilityUnknownUnitValue` | 1 | hammerspoon |
| AppKit.framework | `_NSAccessibilityVisibleNameKey` | 1 | libreoffice |
| AppKit.framework | `_NSAllowsNullArgumentBindingOption` | 1 | gemini |
| AppKit.framework | `_NSApplicationDidUpdateNotification` | 1 | vorssaint |
| AppKit.framework | `_NSBeginCriticalAlertSheet` | 1 | vlc |
| AppKit.framework | `_NSBeginInformationalAlertSheet` | 1 | vlc |
| AppKit.framework | `_NSCollectionElementKindInterItemGapIndicator` | 1 | windows-app |
| AppKit.framework | `_NSDarkGray` | 1 | vlc |
| AppKit.framework | `_NSDrawWindowBackground` | 1 | openscad |
| AppKit.framework | `_NSFontCollectionRecentlyUsed` | 1 | markedit |
| AppKit.framework | `_NSFontTextStyleFootnote` | 1 | 1password |
| AppKit.framework | `_NSImageNameTouchBarAlarmTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarAudioInputMuteTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarAudioInputTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarAudioOutputMuteTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarAudioOutputVolumeHighTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarAudioOutputVolumeLowTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarAudioOutputVolumeMediumTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarAudioOutputVolumeOffTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarColorPickerFont` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarColorPickerStroke` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarComposeTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarDeleteTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarDownloadTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarFolderCopyToTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarFolderMoveToTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarFolderTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarGetInfoTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarGoBackTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarGoForwardTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarHistoryTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarListViewTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarMailTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarNewFolderTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarNewMessageTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarOpenInBrowserTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarPlayheadTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarRecordStartTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarRefreshTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarRemoveTemplate` | 1 | keepingyouawake |
| AppKit.framework | `_NSImageNameTouchBarRotateLeftTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarRotateRightTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarShareTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarSidebarTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarSkipToEndTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarSkipToStartTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarSlideshowTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarTagIconTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarTextBoldTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarTextBoxTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarTextCenterAlignTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarTextItalicTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarTextJustifiedAlignTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarTextLeftAlignTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarTextListTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarTextRightAlignTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarTextStrikethroughTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarTextUnderlineTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarUserAddTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarUserGroupTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarUserTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSIsControllerMarker` | 1 | alt-tab |
| AppKit.framework | `_NSModalPanelRunLoopMode` | 1 | vorssaint |
| AppKit.framework | `_NSNoSelectionMarker` | 1 | alt-tab |
| AppKit.framework | `_NSPasteboardURLReadingContentsConformToTypesKey` | 1 | vorssaint |
| AppKit.framework | `_NSRectFillListWithGrays` | 1 | hex-fiend |
| AppKit.framework | `_NSRunInformationalAlertPanel` | 1 | vlc |
| AppKit.framework | `_NSSharingServiceNameAddToAperture` | 1 | hammerspoon |
| AppKit.framework | `_NSSharingServiceNameAddToIPhoto` | 1 | hammerspoon |
| AppKit.framework | `_NSSharingServiceNameUseAsDesktopPicture` | 1 | hammerspoon |
| AppKit.framework | `_NSSpeechPhonemeSymbolsProperty` | 1 | hammerspoon |
| AppKit.framework | `_NSSpeechPitchModProperty` | 1 | hammerspoon |
| AppKit.framework | `_NSSpeechRecentSyncProperty` | 1 | hammerspoon |
| AppKit.framework | `_NSSpeechStatusOutputBusy` | 1 | hammerspoon |
| AppKit.framework | `_NSSpeechStatusOutputPaused` | 1 | hammerspoon |
| AppKit.framework | `_NSSpeechStatusProperty` | 1 | hammerspoon |
| AppKit.framework | `_NSTextContentTypeURL` | 1 | coteditor |
| AppKit.framework | `_NSTextListMarkerCircle` | 1 | raycast |
| AppKit.framework | `_NSTextListMarkerLowercaseAlpha` | 1 | raycast |
| AppKit.framework | `_NSTextListMarkerLowercaseRoman` | 1 | raycast |
| AppKit.framework | `_NSTextListMarkerSquare` | 1 | raycast |
| AppKit.framework | `_NSWorkspaceDesktopImageAllowClippingKey` | 1 | vorssaint |
| AppKit.framework | `_NSWorkspaceDesktopImageScalingKey` | 1 | vorssaint |
| AppKit.framework | `_NSWorkspaceDidLaunchApplicationNotification` | 1 | vorssaint |
| AppKit.framework | `_NSWorkspaceDidMountNotification` | 1 | vorssaint |
| AppKit.framework | `_NSWorkspaceDidUnmountNotification` | 1 | vorssaint |
| AppKit.framework | `_NSWorkspaceLaunchConfigurationAppleEvent` | 1 | windows-app |
| AppKit.framework | `_NSWorkspaceVolumeURLKey` | 1 | vorssaint |
| AppKit.framework | `_NSWorkspaceWillLaunchApplicationNotification` | 1 | vorssaint |
| AppKit.framework | `_OBJC_CLASS_$_NSFrameView` | 1 | zoom |
| AppKit.framework | `_OBJC_CLASS_$_NSGlassEffectContainerView` | 1 | coteditor |
| AppKit.framework | `_OBJC_CLASS_$_NSGlyphInfo` | 1 | emacs-app |
| AppKit.framework | `_OBJC_CLASS_$_NSNextStepFrame` | 1 | zoom |
| AppKit.framework | `_OBJC_CLASS_$_NSObjectController` | 1 | alt-tab |
| AppKit.framework | `_OBJC_CLASS_$_NSPathCell` | 1 | gemini |
| AppKit.framework | `_OBJC_CLASS_$_NSPredicateEditorRowTemplate` | 1 | transmission |
| AppKit.framework | `_OBJC_CLASS_$_NSSpeechRecognizer` | 1 | hammerspoon |
| AppKit.framework | `_OBJC_CLASS_$_NSSplitViewItemAccessoryViewController` | 1 | coteditor |
| AppKit.framework | `_OBJC_CLASS_$_NSStoryboardSegue` | 1 | gemini |
| AppKit.framework | `_OBJC_CLASS_$_NSTableViewRowAction` | 1 | sourcetree |
| AppKit.framework | `_OBJC_CLASS_$_NSTextInsertionIndicator` | 1 | coteditor |
| AppKit.framework | `_OBJC_CLASS_$_NSTextLayoutFragment` | 1 | coteditor |
| AppKit.framework | `_OBJC_CLASS_$_NSTextRange` | 1 | coteditor |
| AppKit.framework | `_OBJC_CLASS_$_NSToolTipManager` | 1 | zoom |
| AppKit.framework | `_OBJC_CLASS_$_NSWindowTabGroup` | 1 | ghostty |
| AppKit.framework | `_OBJC_CLASS_$_NSWritingToolsCoordinator` | 1 | coteditor |
| AppKit.framework | `_OBJC_IVAR_$_NSApplication._appFlags` | 1 | xquartz |
| AppKit.framework | `_OBJC_METACLASS_$_NSFontPanel` | 1 | iterm2 |
| AppKit.framework | `_OBJC_METACLASS_$_NSObjectController` | 1 | alt-tab |
| AppKit.framework | `_OBJC_METACLASS_$_NSPredicateEditorRowTemplate` | 1 | transmission |
| AppKit.framework | `_OBJC_METACLASS_$_NSRuleEditor` | 1 | sequel-ace |
| AppKit.framework | `_OBJC_METACLASS_$_NSSpeechRecognizer` | 1 | hammerspoon |
| AppKit.framework | `_OBJC_METACLASS_$_NSStepperCell` | 1 | vlc |
| AppKit.framework | `_OBJC_METACLASS_$_NSStoryboardSegue` | 1 | gemini |
| AppKit.framework | `_OBJC_METACLASS_$_NSTextLayoutFragment` | 1 | coteditor |
| AppKit.framework | `__NSDrawCarbonThemeBezel` | 1 | webtorrent |
| AppKit.framework | `__NSDrawCarbonThemeListBox` | 1 | webtorrent |
| ApplicationServices.framework | `_AXAPIEnabled` | 1 | hammerspoon |
| ApplicationServices.framework | `_AXObserverGetTypeID` | 1 | hammerspoon |
| ApplicationServices.framework | `_ContinueSpeech` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_CoreCursorSet` | 1 | raycast |
| ApplicationServices.framework | `_CoreCursorUnregisterAll` | 1 | raycast |
| ApplicationServices.framework | `_CoreDockIsMagnificationEnabled` | 1 | dockdoor |
| ApplicationServices.framework | `_CoreDockSetAutoHideEnabled` | 1 | ghostty |
| ApplicationServices.framework | `_CountVoices` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_GetIndVoice` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_GetVoiceDescription` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_ICGetPref` | 1 | virtualbox |
| ApplicationServices.framework | `_ICMapFilename` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_ICStart` | 1 | virtualbox |
| ApplicationServices.framework | `_ICStop` | 1 | virtualbox |
| ApplicationServices.framework | `_KillProcess` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_LaunchApplication` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_PMGetScale` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_PMPrinterCopyHostName` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_PMPrinterGetDriverCreator` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_PMPrinterGetDriverReleaseInfo` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_PMPrinterGetLanguageInfo` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_PMPrinterIsFavorite` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_PMSessionCreatePageFormatList` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_PMSessionCreatePrinterList` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_PMSetScale` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_PauseSpeechAt` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_SameProcess` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_SetIconFamilyData` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_SetSpeechPitch` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_SetSpeechProperty` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_SetSpeechRate` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_StopSpeech` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_WakeUpProcess` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_kAXAttachmentTextAttribute` | 1 | hammerspoon |
| ApplicationServices.framework | `_kAXAutocorrectedTextAttribute` | 1 | hammerspoon |
| ApplicationServices.framework | `_kAXBackgroundColorTextAttribute` | 1 | hammerspoon |
| ApplicationServices.framework | `_kAXFontTextAttribute` | 1 | hammerspoon |
| ApplicationServices.framework | `_kAXForegroundColorTextAttribute` | 1 | hammerspoon |
| ApplicationServices.framework | `_kAXLinkTextAttribute` | 1 | hammerspoon |
| ApplicationServices.framework | `_kAXListItemIndexTextAttribute` | 1 | hammerspoon |
| ApplicationServices.framework | `_kAXListItemLevelTextAttribute` | 1 | hammerspoon |
| ApplicationServices.framework | `_kAXListItemPrefixTextAttribute` | 1 | hammerspoon |
| ApplicationServices.framework | `_kAXMarkedMisspelledTextAttribute` | 1 | hammerspoon |
| ApplicationServices.framework | `_kAXMisspelledTextAttribute` | 1 | hammerspoon |
| ApplicationServices.framework | `_kAXNaturalLanguageTextAttribute` | 1 | hammerspoon |
| ApplicationServices.framework | `_kAXReplacementStringTextAttribute` | 1 | hammerspoon |
| ApplicationServices.framework | `_kAXShadowTextAttribute` | 1 | hammerspoon |
| ApplicationServices.framework | `_kAXStrikethroughColorTextAttribute` | 1 | hammerspoon |
| ApplicationServices.framework | `_kAXStrikethroughTextAttribute` | 1 | hammerspoon |
| ApplicationServices.framework | `_kAXSuperscriptTextAttribute` | 1 | hammerspoon |
| ApplicationServices.framework | `_kAXUnderlineColorTextAttribute` | 1 | hammerspoon |
| ApplicationServices.framework | `_kAXUnderlineTextAttribute` | 1 | hammerspoon |
| ApplicationServices.framework | `_kSpeechCurrentVoiceProperty` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_kSpeechSpeechDoneCallBack` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_kSpeechVoiceCreator` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_kSpeechVoiceID` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_kSpeechVolumeProperty` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_AEGetInteractionAllowed` | 1 | deluge |
| Carbon.framework | `_AEGetTheCurrentEvent` | 1 | deluge |
| Carbon.framework | `_AEProcessAppleEvent` | 1 | deluge |
| Carbon.framework | `_AEResetTimer` | 1 | deluge |
| Carbon.framework | `_AESetInteractionAllowed` | 1 | deluge |
| Carbon.framework | `_AESetTheCurrentEvent` | 1 | deluge |
| Carbon.framework | `_AHLookupAnchor` | 1 | xquartz |
| Carbon.framework | `_Button` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_CheckEventQueueForUserCancel` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_CopyThemeIdentifier` | 1 | openscad |
| Carbon.framework | `_FindSpecificEventInQueue` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_GetApplicationTextEncoding` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_GetCurrentEventTime` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_GetKeys` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_GetMainEventLoop` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_GetMenuTrackingData` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_GetNumEventsInQueue` | 1 | openscad |
| Carbon.framework | `_GetSystemUIMode` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_HIGetMousePosition` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_HIPointConvert` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_HIRectConvert` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_HISearchWindowShow` | 1 | alfred |
| Carbon.framework | `_HIThemeDrawBackground` | 1 | prusaslicer |
| Carbon.framework | `_HIThemeDrawMenuBarBackground` | 1 | openscad |
| Carbon.framework | `_HIThemeDrawMenuSeparator` | 1 | openscad |
| Carbon.framework | `_HIThemeDrawPlacard` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_HIThemeDrawPopupArrow` | 1 | openscad |
| Carbon.framework | `_HIThemeDrawTitleBarWidget` | 1 | openscad |
| Carbon.framework | `_HIThemeDrawTrackTickMarks` | 1 | openscad |
| Carbon.framework | `_HIThemeDrawWindowFrame` | 1 | openscad |
| Carbon.framework | `_HIThemeGetButtonShape` | 1 | openscad |
| Carbon.framework | `_HIThemeGetMenuBackgroundShape` | 1 | openscad |
| Carbon.framework | `_HIThemeGetTextDimensions` | 1 | openscad |
| Carbon.framework | `_HIThemeGetTrackBounds` | 1 | openscad |
| Carbon.framework | `_HIThemeGetTrackDragRect` | 1 | openscad |
| Carbon.framework | `_HIThemeGetTrackPartBounds` | 1 | openscad |
| Carbon.framework | `_HIThemeGetTrackThumbShape` | 1 | openscad |
| Carbon.framework | `_HIThemeGetWindowShape` | 1 | openscad |
| Carbon.framework | `_HIThemeHitTestScrollBarArrows` | 1 | openscad |
| Carbon.framework | `_HIThemeHitTestTrack` | 1 | openscad |
| Carbon.framework | `_InstallEventLoopTimer` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_OSACompile` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_OSADispose` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_OSAExecuteEvent` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_OSAScriptError` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_RemoveEventLoopTimer` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_RunCurrentEventLoop` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_kTISPropertyIconRef` | 1 | hammerspoon |
| Carbon.framework | `_kTISTypeKeyboardInputMethodModeEnabled` | 1 | raycast |
| QuickLook.framework | `_OBJC_CLASS_$_QLPreviewController` | 1 | whatsapp |
| QuickLook.framework | `_OBJC_METACLASS_$_QLPreviewController` | 1 | whatsapp |
| QuickLook.framework | `_QLPreviewRequestIsCancelled` | 1 | transmission |
| QuickLook.framework | `_QLPreviewRequestSetDataRepresentation` | 1 | transmission |
| QuickLook.framework | `_kQLPreviewPropertyAttachmentDataKey` | 1 | transmission |
| QuickLook.framework | `_kQLPreviewPropertyAttachmentsKey` | 1 | transmission |
| QuickLook.framework | `_kQLPreviewPropertyMIMETypeKey` | 1 | transmission |
| QuickLook.framework | `_kQLPreviewPropertyTextEncodingNameKey` | 1 | transmission |
| QuickLookThumbnailing.framework | `_OBJC_CLASS_$_QLThumbnailRepresentation` | 1 | whatsapp |
| QuickLookUI.framework | `_OBJC_CLASS_$_QLPreviewReplyAttachment` | 1 | transmission |
| libswiftAppKit.dylib | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV015ForegroundColorB0OAA19AttributedStringKeyADMc` | 1 | raycast |
| libswiftAppKit.dylib | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV015ForegroundColorB0OMn` | 1 | raycast |
| libswiftAppKit.dylib | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV015ForegroundColorB0ON` | 1 | raycast |
| libswiftAppKit.dylib | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV15foregroundColorAF010ForegroundhB0Ovg` | 1 | raycast |
| libswiftAppKit.dylib | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV15foregroundColorAF010ForegroundhB0OvpMV` | 1 | raycast |
| libswiftAppKit.dylib | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesVMn` | 1 | raycast |
| libswiftAppKit.dylib | `_$s10Foundation22AttributeDynamicLookupO6AppKitE13dynamicMemberxs7KeyPathCyAA0B6ScopesOADE0eF10AttributesVxG_tcAA016AttributedStringI0Rzluig` | 1 | raycast |
| libswiftAppKit.dylib | `_$s10Foundation22AttributeDynamicLookupO6AppKitE13dynamicMemberxs7KeyPathCyAA0B6ScopesOADE0eF10AttributesVxG_tcAA016AttributedStringI0RzluipMV` | 1 | raycast |
| libswiftAppKit.dylib | `_$s10Foundation9IndexPathV6AppKitE4itemSivs` | 1 | shottr |
| libswiftAppKit.dylib | `_$s6AppKit28NSDiffableDataSourceSnapshotV11deleteItemsyySayq_GF` | 1 | raycast |
| libswiftAppKit.dylib | `_$s6AppKit28NSDiffableDataSourceSnapshotV11reloadItemsyySayq_GF` | 1 | raycast |
| libswiftAppKit.dylib | `_$s6AppKit28NSDiffableDataSourceSnapshotV13numberOfItemsSivg` | 1 | raycast |
| libswiftAppKit.dylib | `_$s6AppKit28NSDiffableDataSourceSnapshotV14deleteAllItemsyyF` | 1 | raycast |
| libswiftAppKit.dylib | `_$s6AppKit28NSDiffableDataSourceSnapshotV15itemIdentifiersSayq_Gvg` | 1 | raycast |
| libswiftAppKit.dylib | `_$s6AppKit28NSDiffableDataSourceSnapshotV16numberOfSectionsSivg` | 1 | raycast |
| libswiftAppKit.dylib | `_$s6AppKit28NSDiffableDataSourceSnapshotV18sectionIdentifiersSayxGvg` | 1 | raycast |
| libswiftAppKit.dylib | `_$s6AppKit34NSCollectionViewDiffableDataSourceC8snapshotAA010NSDiffablefG8SnapshotVyxq_GyFTj` | 1 | raycast |
| libswiftAppKit.dylib | `_$s6AppKit34NSCollectionViewDiffableDataSourceC9indexPath3for10Foundation05IndexI0VSgq__tFTj` | 1 | raycast |
| libswiftAppKit.dylib | `_$sSo12NSStoryboardC6AppKitE28instantiateInitialController7creatorxSgAFSo7NSCoderCcSg_tSo08NSWindowF0CRbzlF` | 1 | sequel-ace |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV10rightArrowAEvgZ` | 1 | raycast |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV14carriageReturnAEvgZ` | 1 | sourcetree |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f21AEvgZ` | 1 | shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f22AEvgZ` | 1 | shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f23AEvgZ` | 1 | shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f24AEvgZ` | 1 | shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f25AEvgZ` | 1 | shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f26AEvgZ` | 1 | shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f27AEvgZ` | 1 | shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f28AEvgZ` | 1 | shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f29AEvgZ` | 1 | shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f30AEvgZ` | 1 | shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f31AEvgZ` | 1 | shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f32AEvgZ` | 1 | shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f33AEvgZ` | 1 | shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f34AEvgZ` | 1 | shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f35AEvgZ` | 1 | shottr |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV8rawValueSivg` | 1 | sourcetree |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV9leftArrowAEvgZ` | 1 | raycast |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyVSHACMc` | 1 | shottr |
| libswiftQuickLookUI.dylib | `_$sSo14QLPreviewReplyC11QuickLookUIE14stringEncodingSS10FoundationE0G0Vvs` | 1 | qlmarkdown |
