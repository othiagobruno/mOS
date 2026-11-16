# Lacunas de Beatriz (AppKit)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Beatriz (AppKit) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**884** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| Carbon.framework | `_GetEventDispatcherTarget` | 41 | adobe-acrobat-reader, alt-tab, amethyst, another-redis-desktop-manager |
| AppKit.framework | `_OBJC_CLASS_$_NSFontPanel` | 39 | alfred, audacity, bbedit, bettertouchtool |
| ApplicationServices.framework | `_PasteboardCreate` | 34 | audacity, bbedit, calibre, db-browser-for-sqlite |
| ApplicationServices.framework | `_PasteboardSynchronize` | 34 | audacity, bbedit, calibre, db-browser-for-sqlite |
| Carbon.framework | `_TISCreateInputSourceList` | 31 | adobe-acrobat-reader, alfred, alt-tab, android-studio |
| AppKit.framework | `_NSFileContentsPboardType` | 30 | android-studio, audacity, bettertouchtool, calibre |
| AppKit.framework | `_OBJC_CLASS_$_NSPageLayout` | 30 | adobe-acrobat-reader, android-studio, calibre, cryptomator |
| AppKit.framework | `_NSAccessibilityUnignoredChildrenForOnlyChild` | 29 | android-studio, audacity, calibre, cryptomator |
| Carbon.framework | `_TISCopyInputMethodKeyboardLayoutOverride` | 29 | audacity, calibre, db-browser-for-sqlite, dolphin |
| AppKit.framework | `_OBJC_CLASS_$_NSPrintOperation` | 27 | adobe-acrobat-reader, android-studio, bbedit, cleanshot |
| Carbon.framework | `_kTISPropertyInputSourceIsASCIICapable` | 24 | android-studio, blender, clipy, easydict |
| ApplicationServices.framework | `_PasteboardCopyItemFlavorData` | 23 | audacity, bbedit, calibre, db-browser-for-sqlite |
| ApplicationServices.framework | `_PasteboardCopyItemFlavors` | 23 | audacity, bbedit, calibre, db-browser-for-sqlite |
| ApplicationServices.framework | `_PasteboardGetItemCount` | 23 | audacity, bbedit, calibre, db-browser-for-sqlite |
| ApplicationServices.framework | `_PasteboardGetItemIdentifier` | 23 | audacity, bbedit, calibre, db-browser-for-sqlite |
| AppKit.framework | `_NSApplicationLoad` | 22 | adobe-acrobat-reader, android-file-transfer, android-studio, bitwarden |
| ApplicationServices.framework | `_PMGetDuplex` | 22 | adobe-acrobat-reader, android-studio, bbedit, calibre |
| ApplicationServices.framework | `_PasteboardClear` | 22 | audacity, bbedit, calibre, db-browser-for-sqlite |
| ApplicationServices.framework | `_PasteboardPutItemFlavor` | 22 | audacity, bbedit, calibre, db-browser-for-sqlite |
| ApplicationServices.framework | `_PasteboardResolvePromises` | 22 | audacity, bbedit, calibre, db-browser-for-sqlite |
| ApplicationServices.framework | `_PasteboardSetPromiseKeeper` | 22 | audacity, bbedit, calibre, db-browser-for-sqlite |
| ApplicationServices.framework | `_PlotIconRefInContext` | 22 | adobe-acrobat-reader, audacity, calibre, db-browser-for-sqlite |
| ApplicationServices.framework | `_PMRetain` | 21 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, firefox |
| ApplicationServices.framework | `_PasteboardCopyName` | 21 | audacity, calibre, db-browser-for-sqlite, dolphin |
| Carbon.framework | `_TISSelectInputSource` | 21 | adobe-acrobat-reader, alfred, android-studio, bettertouchtool |
| AppKit.framework | `_OBJC_CLASS_$_NSStepperCell` | 20 | calibre, db-browser-for-sqlite, dolphin, keepassxc |
| AppKit.framework | `_OBJC_CLASS_$_NSViewAnimation` | 20 | bettertouchtool, daisydisk, espanso, firefox |
| ApplicationServices.framework | `_GetCurrentProcess` | 20 | adobe-acrobat-reader, android-studio, cryptomator, cyberduck |
| AppKit.framework | `_NSWorkspaceRecycleOperation` | 19 | brave-browser, daisydisk, gemini, hex-fiend |
| AppKit.framework | `_OBJC_CLASS_$_NSMatrix` | 19 | adobe-acrobat-reader, another-redis-desktop-manager, antigravity-ide, battery |
| ApplicationServices.framework | `_PMSessionError` | 19 | bbedit, calibre, db-browser-for-sqlite, floorp |
| AppKit.framework | `_NSControlTintDidChangeNotification` | 18 | adobe-acrobat-reader, alt-tab, another-redis-desktop-manager, firefox |
| AppKit.framework | `_OBJC_CLASS_$_NSSwitch` | 18 | alt-tab, bartender, bettertouchtool, cleanshot |
| ApplicationServices.framework | `_PasteboardGetItemFlavorFlags` | 18 | audacity, db-browser-for-sqlite, dolphin, keepassxc |
| Carbon.framework | `_kTISPropertyInputSourceType` | 18 | firefox, floorp, git-credential-manager, godot |
| AppKit.framework | `_OBJC_CLASS_$_NSPrinter` | 17 | adobe-acrobat-reader, android-studio, bbedit, cryptomator |
| ApplicationServices.framework | `_PMGetFirstPage` | 17 | adobe-acrobat-reader, bbedit, calibre, db-browser-for-sqlite |
| ApplicationServices.framework | `_PMGetLastPage` | 17 | adobe-acrobat-reader, bbedit, calibre, db-browser-for-sqlite |
| ApplicationServices.framework | `_PMServerCreatePrinterList` | 17 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, espanso |
| ApplicationServices.framework | `_PMPrinterIsDefault` | 16 | calibre, db-browser-for-sqlite, espanso, kicad |
| Carbon.framework | `_GetCurrentEventKeyModifiers` | 16 | alfred, cryptomator, dbeaver-community, firefox |
| Carbon.framework | `_GetEventClass` | 16 | adobe-acrobat-reader, alt-tab, another-redis-desktop-manager, battery |
| AppKit.framework | `_NSRectClip` | 15 | alfred, bitwarden, emacs-app, gemini |
| ApplicationServices.framework | `_PMGetCollate` | 15 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, espanso |
| ApplicationServices.framework | `_PMGetCopies` | 15 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, espanso |
| ApplicationServices.framework | `_PMGetUnadjustedPaperRect` | 15 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, espanso |
| ApplicationServices.framework | `_PMSetPageRange` | 15 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, espanso |
| Carbon.framework | `_AddEventTypesToHandler` | 15 | adobe-acrobat-reader, amethyst, another-redis-desktop-manager, bartender |
| Carbon.framework | `_GetThemeMetric` | 15 | adobe-acrobat-reader, espanso, firefox, floorp |
| Carbon.framework | `_HIThemeDrawButton` | 15 | adobe-acrobat-reader, espanso, firefox, floorp |
| Carbon.framework | `_kTISTypeKeyboardInputMode` | 15 | firefox, floorp, godot, hammerspoon |
| Carbon.framework | `_kTISTypeKeyboardLayout` | 15 | firefox, floorp, git-credential-manager, godot |
| AppKit.framework | `_OBJC_CLASS_$_NSATSTypesetter` | 14 | adobe-acrobat-reader, another-redis-desktop-manager, antigravity-ide, battery |
| AppKit.framework | `_OBJC_METACLASS_$_NSATSTypesetter` | 14 | adobe-acrobat-reader, another-redis-desktop-manager, antigravity-ide, battery |
| ApplicationServices.framework | `_PMPrinterGetMimeTypes` | 14 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, krita |
| ApplicationServices.framework | `_PMPrinterGetName` | 14 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, krita |
| ApplicationServices.framework | `_PMPrinterGetOutputResolution` | 14 | calibre, db-browser-for-sqlite, espanso, kicad |
| Carbon.framework | `_RemoveEventTypesFromHandler` | 14 | adobe-acrobat-reader, amethyst, another-redis-desktop-manager, bartender |
| Carbon.framework | `_TISCopyCurrentASCIICapableKeyboardInputSource` | 14 | bettertouchtool, clipy, cmux, easydict |
| AppKit.framework | `_NSBestDepth` | 13 | bitwarden, db-browser-for-sqlite, emacs-app, gemini |
| AppKit.framework | `_NSFrameRectWithWidthUsingOperation` | 13 | bbedit, bitwarden, gemini, hex-fiend |
| AppKit.framework | `_NSPostScriptPboardType` | 13 | adobe-acrobat-reader, android-studio, cryptomator, db-browser-for-sqlite |
| AppKit.framework | `_OBJC_CLASS_$_NSComboBoxCell` | 13 | daisydisk, firefox, floorp, iterm2 |
| AppKit.framework | `_OBJC_CLASS_$_NSSliderCell` | 13 | bartender, betterdisplay, bettertouchtool, cleanshot |
| ApplicationServices.framework | `_PMCreatePageFormat` | 13 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, krita |
| ApplicationServices.framework | `_PMCreatePrintSettings` | 13 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, krita |
| ApplicationServices.framework | `_PMCreateSession` | 13 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, krita |
| ApplicationServices.framework | `_PMPrinterCopyDescriptionURL` | 13 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, krita |
| ApplicationServices.framework | `_PMPrinterIsRemote` | 13 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, krita |
| ApplicationServices.framework | `_PMSessionDefaultPageFormat` | 13 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, krita |
| ApplicationServices.framework | `_PMSessionDefaultPrintSettings` | 13 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, krita |
| ApplicationServices.framework | `_SetFrontProcess` | 13 | adobe-acrobat-reader, bettertouchtool, deluge, firefox |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyVMa` | 12 | bartender, cmux, codexbar, coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyVMn` | 12 | bartender, cmux, codexbar, coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10specialKeyAbCE07SpecialE0VSgvg` | 12 | bartender, cmux, codexbar, coteditor |
| AppKit.framework | `_NSDisableScreenUpdates` | 12 | bartender, bettertouchtool, daisydisk, deluge |
| AppKit.framework | `_NSEnableScreenUpdates` | 12 | bartender, bettertouchtool, daisydisk, deluge |
| AppKit.framework | `_NSVCardPboardType` | 12 | android-studio, cryptomator, db-browser-for-sqlite, dbeaver-community |
| AppKit.framework | `_NSVoiceLocaleIdentifier` | 12 | adobe-acrobat-reader, another-redis-desktop-manager, easydict, godot |
| ApplicationServices.framework | `_PMPrintSettingsGetJobName` | 12 | adobe-acrobat-reader, calibre, db-browser-for-sqlite, krita |
| ApplicationServices.framework | `_PMPrinterGetLocation` | 12 | calibre, db-browser-for-sqlite, krita, lmms |
| ApplicationServices.framework | `_PMPrinterGetMakeAndModelName` | 12 | calibre, db-browser-for-sqlite, krita, lmms |
| Carbon.framework | `_kTISPropertyInputSourceIsEnabled` | 12 | clipy, easydict, firefox, floorp |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyVSQACMc` | 11 | bartender, cmux, codexbar, coteditor |
| AppKit.framework | `_NSAboutPanelOptionApplicationName` | 11 | cc-switch, clash-verge-rev, dbx, handy |
| AppKit.framework | `_NSAboutPanelOptionApplicationVersion` | 11 | cc-switch, clash-verge-rev, dbx, handy |
| AppKit.framework | `_NSSpeechPitchBaseProperty` | 11 | adobe-acrobat-reader, another-redis-desktop-manager, godot, hammerspoon |
| AppKit.framework | `_OBJC_CLASS_$_NSPDFImageRep` | 11 | aldente, alfred, bettertouchtool, cleanshot |
| AppKit.framework | `_OBJC_CLASS_$_NSPanGestureRecognizer` | 11 | bettertouchtool, espanso, homebrew-app, kicad |
| AppKit.framework | `_OBJC_METACLASS_$_NSSliderCell` | 11 | betterdisplay, bettertouchtool, cleanshot, iina |
| ApplicationServices.framework | `_PMPaperCreateLocalizedName` | 11 | calibre, db-browser-for-sqlite, krita, lmms |
| ApplicationServices.framework | `_PMPrinterGetState` | 11 | calibre, db-browser-for-sqlite, krita, lmms |
| AppKit.framework | `_NSAboutPanelOptionApplicationIcon` | 10 | cc-switch, clash-verge-rev, dbx, handy |
| AppKit.framework | `_NSAboutPanelOptionVersion` | 10 | cc-switch, clash-verge-rev, dbx, handy |
| AppKit.framework | `_NSImageNameRefreshFreestandingTemplate` | 10 | cc-switch, clash-verge-rev, dbx, hammerspoon |
| AppKit.framework | `_NSPICTPboardType` | 10 | adobe-acrobat-reader, android-studio, cryptomator, dbeaver-community |
| AppKit.framework | `_NSRectFillListWithColorsUsingOperation` | 10 | bettertouchtool, bitwarden, gemini, hex-fiend |
| AppKit.framework | `_NSVoiceName` | 10 | adobe-acrobat-reader, another-redis-desktop-manager, godot, mullvad-browser |
| QuickLookThumbnailing.framework | `_OBJC_CLASS_$_QLThumbnailGenerationRequest` | 10 | bartender, bbedit, bettertouchtool, cleanshot |
| QuickLookThumbnailing.framework | `_OBJC_CLASS_$_QLThumbnailGenerator` | 10 | bartender, bbedit, bettertouchtool, cleanshot |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f1AEvgZ` | 9 | bartender, cmux, codexbar, coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f2AEvgZ` | 9 | bartender, cmux, codexbar, coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f3AEvgZ` | 9 | bartender, cmux, codexbar, coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f4AEvgZ` | 9 | bartender, cmux, codexbar, coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f5AEvgZ` | 9 | bartender, cmux, codexbar, coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f6AEvgZ` | 9 | bartender, cmux, codexbar, coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f7AEvgZ` | 9 | bartender, cmux, codexbar, coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f8AEvgZ` | 9 | bartender, cmux, codexbar, coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f9AEvgZ` | 9 | bartender, cmux, codexbar, coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f10AEvgZ` | 9 | bartender, cmux, codexbar, coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f11AEvgZ` | 9 | bartender, cmux, codexbar, coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f12AEvgZ` | 9 | bartender, cmux, codexbar, coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f13AEvgZ` | 9 | bartender, cmux, codexbar, coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f14AEvgZ` | 9 | bartender, cmux, codexbar, coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f15AEvgZ` | 9 | bartender, cmux, codexbar, coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f16AEvgZ` | 9 | bartender, cmux, codexbar, coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f17AEvgZ` | 9 | bartender, cmux, codexbar, coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f18AEvgZ` | 9 | bartender, cmux, codexbar, coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f19AEvgZ` | 9 | bartender, cmux, codexbar, coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f20AEvgZ` | 9 | bartender, cmux, codexbar, coteditor |
| AppKit.framework | `_NSAboutPanelOptionCredits` | 9 | cc-switch, clash-verge-rev, dbx, handy |
| AppKit.framework | `_NSImageNameFolderSmart` | 9 | cc-switch, clash-verge-rev, dbx, hammerspoon |
| AppKit.framework | `_NSRectClipList` | 9 | bbedit, bitwarden, emacs-app, gemini |
| AppKit.framework | `_NSRectFillListUsingOperation` | 9 | bitwarden, gemini, monitorcontrol, mos |
| AppKit.framework | `_OBJC_CLASS_$_NSGestureRecognizer` | 9 | bettertouchtool, gemini, markedit, mos |
| AppKit.framework | `_OBJC_CLASS_$_NSSharingServicePickerTouchBarItem` | 9 | coteditor, firefox, floorp, librewolf |
| Carbon.framework | `_GetEventTime` | 9 | adobe-acrobat-reader, another-redis-desktop-manager, battery, espanso |
| Carbon.framework | `_SetSystemUIMode` | 9 | adobe-acrobat-reader, bettertouchtool, espanso, gimp |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyVMa` | 9 | amethyst, betterdisplay, cleanshot, duckduckgo |
| AppKit.framework | `_$sSo10NSMenuItemC6AppKitE13sectionHeader5titleABSS_tFZ` | 8 | 1password, cmux, coteditor, macwhisper |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV13deleteForwardAEvgZ` | 8 | bartender, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f21AEvgZ` | 8 | bartender, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f22AEvgZ` | 8 | bartender, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f23AEvgZ` | 8 | bartender, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f24AEvgZ` | 8 | bartender, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f25AEvgZ` | 8 | bartender, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f26AEvgZ` | 8 | bartender, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f27AEvgZ` | 8 | bartender, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f28AEvgZ` | 8 | bartender, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f29AEvgZ` | 8 | bartender, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f30AEvgZ` | 8 | bartender, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f31AEvgZ` | 8 | bartender, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f32AEvgZ` | 8 | bartender, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f33AEvgZ` | 8 | bartender, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f34AEvgZ` | 8 | bartender, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f35AEvgZ` | 8 | bartender, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3tabAEvgZ` | 8 | bartender, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV6deleteAEvgZ` | 8 | bartender, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV9backspaceAEvgZ` | 8 | bartender, codexbar, coteditor, finetune |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyVSHACMc` | 8 | bartender, codexbar, coteditor, finetune |
| AppKit.framework | `_NSImageNameBluetoothTemplate` | 8 | cc-switch, clash-verge-rev, dbx, hammerspoon |
| AppKit.framework | `_NSImageNameFolderBurnable` | 8 | cc-switch, clash-verge-rev, dbx, hammerspoon |
| AppKit.framework | `_NSImageNameIChatTheaterTemplate` | 8 | cc-switch, clash-verge-rev, dbx, hammerspoon |
| AppKit.framework | `_NSShowAnimationEffect` | 8 | bitwarden, gemini, iterm2, monitorcontrol |
| AppKit.framework | `_NSSpeechRateProperty` | 8 | adobe-acrobat-reader, another-redis-desktop-manager, mullvad-browser, openmtp |
| AppKit.framework | `_NSSpeechVolumeProperty` | 8 | adobe-acrobat-reader, another-redis-desktop-manager, mullvad-browser, openmtp |
| AppKit.framework | `_OBJC_CLASS_$_NSInputManager` | 8 | adobe-acrobat-reader, android-studio, cryptomator, dbeaver-community |
| AppKit.framework | `_OBJC_CLASS_$_NSMagnificationGestureRecognizer` | 8 | espanso, iina, iterm2, kicad |
| AppKit.framework | `_OBJC_CLASS_$_NSPressGestureRecognizer` | 8 | bettertouchtool, espanso, iterm2, kicad |
| AppKit.framework | `_OBJC_CLASS_$_NSScrubberFlowLayout` | 8 | adobe-acrobat-reader, android-studio, bettertouchtool, intellij-idea |
| AppKit.framework | `_OBJC_CLASS_$_NSTreeNode` | 8 | bartender, bettertouchtool, gemini, mac-mouse-fix |
| ApplicationServices.framework | `_PMPrinterPrintWithFile` | 8 | adobe-acrobat-reader, firefox, floorp, librewolf |
| ApplicationServices.framework | `_PasteboardCopyPasteLocation` | 8 | firefox, floorp, librewolf, mullvad-browser |
| Carbon.framework | `_GetCurrentEventQueue` | 8 | adobe-acrobat-reader, another-redis-desktop-manager, battery, openmtp |
| Carbon.framework | `_TISCopyInputSourceForLanguage` | 8 | firefox, floorp, librewolf, mullvad-browser |
| Carbon.framework | `_TSMGetActiveDocument` | 8 | firefox, floorp, librewolf, mullvad-browser |
| Carbon.framework | `_kTISPropertyBundleID` | 8 | firefox, floorp, librewolf, mullvad-browser |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyVMn` | 8 | amethyst, betterdisplay, cleanshot, duckduckgo |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyVSQACMc` | 8 | amethyst, betterdisplay, cleanshot, duckduckgo |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10specialKeyAbCE07SpecialE0VSgvg` | 8 | amethyst, betterdisplay, cleanshot, duckduckgo |
| AppKit.framework | `_NSAvailableWindowDepths` | 7 | bitwarden, gemini, monitorcontrol, mos |
| AppKit.framework | `_NSImageNameTouchBarPauseTemplate` | 7 | hammerspoon, handbrake-app, iina, microsoft-edge |
| AppKit.framework | `_NSImageNameTouchBarPlayTemplate` | 7 | hammerspoon, handbrake-app, iina, microsoft-edge |
| AppKit.framework | `_NSRunAlertPanel` | 7 | amethyst, bettertouchtool, cyberduck, flycut |
| AppKit.framework | `_OBJC_CLASS_$_NSLevelIndicatorCell` | 7 | kicad, prusaslicer, skim, teamviewer |
| Carbon.framework | `_GetCurrentEvent` | 7 | adobe-acrobat-reader, another-redis-desktop-manager, battery, kicad |
| Carbon.framework | `_GetEventMonitorTarget` | 7 | adobe-acrobat-reader, alfred, another-redis-desktop-manager, battery |
| Carbon.framework | `_GetMainEventQueue` | 7 | adobe-acrobat-reader, another-redis-desktop-manager, battery, openmtp |
| Carbon.framework | `_HIThemeBeginFocus` | 7 | android-studio, cryptomator, cyberduck, dbeaver-community |
| Carbon.framework | `_HIThemeDrawTrack` | 7 | espanso, kicad, openscad, pgadmin4 |
| Carbon.framework | `_HIThemeEndFocus` | 7 | android-studio, cryptomator, cyberduck, dbeaver-community |
| Carbon.framework | `_PopSymbolicHotKeyMode` | 7 | anydesk, bartender, bettertouchtool, flycut |
| Carbon.framework | `_PushSymbolicHotKeyMode` | 7 | anydesk, bartender, bettertouchtool, flycut |
| Carbon.framework | `_kTISPropertyInputSourceIsSelectCapable` | 7 | bettertouchtool, clipy, easydict, hammerspoon |
| libswiftAppKit.dylib | `_$sSo13NSWindowLevela6AppKit01_cdD23NumericRawRepresentableACMc` | 7 | betterdisplay, bettertouchtool, cleanshot, raycast |
| AppKit.framework | `_NSAccessibilityTextAlignmentAttribute` | 6 | godot, hammerspoon, openlogi, pixelorama |
| AppKit.framework | `_NSAccessibilityUnderlineColorTextAttribute` | 6 | godot, libreoffice, openlogi, pixelorama |
| AppKit.framework | `_NSBitsPerSampleFromDepth` | 6 | adobe-acrobat-reader, db-browser-for-sqlite, krita, lmms |
| AppKit.framework | `_NSColorSpaceFromDepth` | 6 | db-browser-for-sqlite, emacs-app, krita, lmms |
| AppKit.framework | `_NSInkTextPboardType` | 6 | db-browser-for-sqlite, krita, lmms, macs-fan-control |
| AppKit.framework | `_NSMultipleTextSelectionPboardType` | 6 | db-browser-for-sqlite, krita, lmms, macs-fan-control |
| AppKit.framework | `_OBJC_CLASS_$_NSController` | 6 | alfred, alt-tab, keka, keycastr |
| AppKit.framework | `_OBJC_CLASS_$_NSHelpManager` | 6 | bbedit, coteditor, imageoptim, sequel-ace |
| AppKit.framework | `_OBJC_CLASS_$_NSTouch` | 6 | alt-tab, dockdoor, iterm2, mos |
| ApplicationServices.framework | `_HIShapeCreateMutable` | 6 | bbedit, espanso, kicad, openscad |
| ApplicationServices.framework | `_HIShapeUnionWithRect` | 6 | bbedit, espanso, kicad, openscad |
| Carbon.framework | `_CopyEvent` | 6 | adobe-acrobat-reader, another-redis-desktop-manager, battery, openmtp |
| Carbon.framework | `_CreateEvent` | 6 | adobe-acrobat-reader, another-redis-desktop-manager, battery, openmtp |
| Carbon.framework | `_HIThemeDrawFrame` | 6 | espanso, kicad, libreoffice, openscad |
| Carbon.framework | `_PostEventToQueue` | 6 | adobe-acrobat-reader, another-redis-desktop-manager, battery, openmtp |
| Carbon.framework | `_RetainEvent` | 6 | adobe-acrobat-reader, another-redis-desktop-manager, battery, openmtp |
| Carbon.framework | `_SendEventToEventTargetWithOptions` | 6 | adobe-acrobat-reader, another-redis-desktop-manager, battery, openmtp |
| Carbon.framework | `_SetEventParameter` | 6 | adobe-acrobat-reader, another-redis-desktop-manager, battery, openmtp |
| Carbon.framework | `_SetEventTime` | 6 | adobe-acrobat-reader, another-redis-desktop-manager, battery, openmtp |
| QuickLookThumbnailing.framework | `_OBJC_CLASS_$_QLThumbnailProvider` | 6 | blender, cleanshot, krita, libreoffice |
| QuickLookThumbnailing.framework | `_OBJC_CLASS_$_QLThumbnailReply` | 6 | blender, cleanshot, krita, libreoffice |
| QuickLookThumbnailing.framework | `_OBJC_METACLASS_$_QLThumbnailProvider` | 6 | blender, cleanshot, krita, libreoffice |
| AppKit.framework | `_$sSo13NSWindowLevela6AppKit01_cdD23NumericRawRepresentableACMc` | 5 | bartender, jordanbaird-ice, superwhisper, thaw |
| AppKit.framework | `_NSAccessibilityLanguageTextAttribute` | 5 | godot, openlogi, pixelorama, unity-hub |
| AppKit.framework | `_NSAccessibilityStrikethroughColorTextAttribute` | 5 | godot, openlogi, pixelorama, unity-hub |
| AppKit.framework | `_NSFontDescriptorSystemDesignRounded` | 5 | bettertouchtool, cleanshot, cmux, markedit |
| AppKit.framework | `_NSOutlineViewDisclosureButtonKey` | 5 | duckduckgo, iterm2, proxyman, sourcetree |
| AppKit.framework | `_NSRectFillListWithColors` | 5 | bbedit, bettertouchtool, hex-fiend, proxyman |
| AppKit.framework | `_NSSharingServiceNameSendViaAirDrop` | 5 | bartender, hammerspoon, microsoft-edge, microsoft-teams |
| AppKit.framework | `_NSTextContentTypeOneTimeCode` | 5 | cmux, duckduckgo, multipass, tunnelblick |
| AppKit.framework | `_OBJC_CLASS_$_NSRotationGestureRecognizer` | 5 | espanso, kicad, mos, prusaslicer |
| AppKit.framework | `_OBJC_CLASS_$_NSTextBlock` | 5 | alfred, iterm2, raycast, telegram |
| ApplicationServices.framework | `_DisposeSpeechChannel` | 5 | adobe-acrobat-reader, another-redis-desktop-manager, openmtp, pencil |
| ApplicationServices.framework | `_HIShapeCreateIntersection` | 5 | bbedit, espanso, kicad, pgadmin4 |
| ApplicationServices.framework | `_HIShapeCreateMutableCopy` | 5 | bbedit, espanso, kicad, pgadmin4 |
| ApplicationServices.framework | `_HIShapeCreateWithRect` | 5 | bbedit, espanso, kicad, pgadmin4 |
| ApplicationServices.framework | `_HIShapeDifference` | 5 | bbedit, espanso, kicad, pgadmin4 |
| ApplicationServices.framework | `_HIShapeEnumerate` | 5 | bbedit, kicad, openscad, pgadmin4 |
| ApplicationServices.framework | `_HIShapeGetBounds` | 5 | bbedit, espanso, kicad, pgadmin4 |
| ApplicationServices.framework | `_HIShapeIsEmpty` | 5 | bbedit, espanso, kicad, pgadmin4 |
| ApplicationServices.framework | `_HIShapeIsRectangular` | 5 | bbedit, espanso, kicad, pgadmin4 |
| ApplicationServices.framework | `_HIShapeOffset` | 5 | bbedit, espanso, kicad, pgadmin4 |
| ApplicationServices.framework | `_HIShapeReplacePathInCGContext` | 5 | bbedit, espanso, kicad, pgadmin4 |
| ApplicationServices.framework | `_HIShapeUnion` | 5 | bbedit, espanso, kicad, pgadmin4 |
| ApplicationServices.framework | `_NewSpeechChannel` | 5 | adobe-acrobat-reader, another-redis-desktop-manager, openmtp, pencil |
| ApplicationServices.framework | `_SpeakCFString` | 5 | adobe-acrobat-reader, another-redis-desktop-manager, openmtp, pencil |
| ApplicationServices.framework | `_SpeechBusy` | 5 | adobe-acrobat-reader, another-redis-desktop-manager, openmtp, pencil |
| Carbon.framework | `_AESend` | 5 | adobe-acrobat-reader, bbedit, bettertouchtool, deluge |
| Carbon.framework | `_HIThemeBrushCreateCGColor` | 5 | espanso, kicad, openscad, pgadmin4 |
| Carbon.framework | `_HIThemeDrawGroupBox` | 5 | adobe-acrobat-reader, kicad, openscad, pgadmin4 |
| Carbon.framework | `_kTISNotifyEnabledKeyboardInputSourcesChanged` | 5 | clipy, easydict, iterm2, linearmouse |
| Carbon.framework | `_kTISPropertyInputSourceIsSelected` | 5 | clipy, easydict, hammerspoon, maccy |
| QuickLookUI.framework | `_OBJC_CLASS_$_QLPreviewReply` | 5 | krita, qlmarkdown, skim, syntax-highlight |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV7upArrowAEvgZ` | 5 | cleanshot, duckduckgo, iterm2, raycast |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV9downArrowAEvgZ` | 5 | cleanshot, duckduckgo, iterm2, raycast |
| AppKit.framework | `_$sSo7NSImageC6AppKitE8resourceAB21DeveloperToolsSupport13ImageResourceV_tcfC` | 4 | coteditor, fluidvoice, jordanbaird-ice, thaw |
| AppKit.framework | `_NSAccessibilityUnignoredDescendant` | 4 | kicad, prusaslicer, skim, zoom |
| AppKit.framework | `_NSFontTextStyleCaption1` | 4 | 1password, fluidvoice, orbstack, supacode |
| AppKit.framework | `_NSImageNameTouchBarEnterFullScreenTemplate` | 4 | hammerspoon, handbrake-app, skim, telegram-desktop |
| AppKit.framework | `_NSImageNameTouchBarExitFullScreenTemplate` | 4 | hammerspoon, handbrake-app, iina, telegram-desktop |
| AppKit.framework | `_NSImageNameTouchBarGoDownTemplate` | 4 | hammerspoon, iterm2, proxyman, sublime-text |
| AppKit.framework | `_NSImageNameTouchBarSearchTemplate` | 4 | bettertouchtool, hammerspoon, proxyman, zoom |
| AppKit.framework | `_NSRunInformationalAlertPanel` | 4 | bettertouchtool, epic-games, r-app, vlc |
| AppKit.framework | `_NSSplitViewItemUnspecifiedDimension` | 4 | bettertouchtool, coteditor, fork, zoom |
| AppKit.framework | `_NSVoiceIdentifier` | 4 | easydict, mullvad-browser, tor-browser, zotero |
| AppKit.framework | `_OBJC_CLASS_$_NSCollectionViewGridLayout` | 4 | protonvpn, teamviewer, windows-app, zoom |
| AppKit.framework | `_OBJC_CLASS_$_NSCustomImageRep` | 4 | bettertouchtool, flycut, sequel-ace, sourcetree |
| AppKit.framework | `_OBJC_CLASS_$_NSEPSImageRep` | 4 | cleanshot, iterm2, libreoffice, protonvpn |
| AppKit.framework | `_OBJC_CLASS_$_NSTextLineFragment` | 4 | coteditor, raycast, squirrel-app, superwhisper |
| AppKit.framework | `_OBJC_CLASS_$_NSTextRange` | 4 | coteditor, duckduckgo, squirrel-app, superwhisper |
| AppKit.framework | `_OBJC_CLASS_$_NSTreeController` | 4 | bbedit, bettertouchtool, gemini, orbstack |
| AppKit.framework | `_OBJC_METACLASS_$_NSController` | 4 | alfred, keka, sourcetree, ubersicht |
| AppKit.framework | `_OBJC_METACLASS_$_NSLevelIndicatorCell` | 4 | skim, teamviewer, transmission, zoom |
| AppKit.framework | `_OBJC_METACLASS_$_NSTreeNode` | 4 | mac-mouse-fix, sequel-ace, sourcetree, zoom |
| ApplicationServices.framework | `_CoreDockSendNotification` | 4 | bettertouchtool, hammerspoon, linearmouse, raycast |
| ApplicationServices.framework | `_GetNextProcess` | 4 | adobe-acrobat-reader, android-file-transfer, flycut, vlc |
| ApplicationServices.framework | `_HIShapeContainsPoint` | 4 | bbedit, espanso, kicad, prusaslicer |
| ApplicationServices.framework | `_HIShapeCreateUnion` | 4 | bbedit, espanso, kicad, prusaslicer |
| ApplicationServices.framework | `_HIShapeIntersect` | 4 | espanso, kicad, pgadmin4, prusaslicer |
| ApplicationServices.framework | `_PMGetPageRange` | 4 | adobe-acrobat-reader, espanso, kicad, prusaslicer |
| ApplicationServices.framework | `_PMGetUnadjustedPageRect` | 4 | adobe-acrobat-reader, espanso, kicad, prusaslicer |
| ApplicationServices.framework | `_ProcessInformationCopyDictionary` | 4 | adobe-acrobat-reader, bettertouchtool, lulu, vlc |
| ApplicationServices.framework | `_StopSpeechAt` | 4 | another-redis-desktop-manager, openmtp, pencil, webtorrent |
| Carbon.framework | `_HIThemeDrawPaneSplitter` | 4 | espanso, kicad, openscad, prusaslicer |
| Carbon.framework | `_HIThemeDrawSeparator` | 4 | kicad, openscad, pgadmin4, zotero |
| Carbon.framework | `_HIThemeGetButtonBackgroundBounds` | 4 | kicad, openscad, pgadmin4, webtorrent |
| Carbon.framework | `_TISDisableInputSource` | 4 | android-studio, intellij-idea, jetbrains-toolbox, squirrel-app |
| Carbon.framework | `_TISEnableInputSource` | 4 | android-studio, intellij-idea, jetbrains-toolbox, squirrel-app |
| Carbon.framework | `_kTISCategoryKeyboardInputSource` | 4 | alfred, iterm2, superwhisper, vorssaint |
| Carbon.framework | `_kTISPropertyInputModeID` | 4 | clipy, easydict, git-credential-manager, maccy |
| Carbon.framework | `_kTISPropertyInputSourceCategory` | 4 | alfred, iterm2, superwhisper, vorssaint |
| QuickLook.framework | `_QLThumbnailImageCreate` | 4 | alfred, bettertouchtool, gemini, sourcetree |
| QuickLookUI.framework | `_OBJC_CLASS_$_QLPreviewProvider` | 4 | krita, qlmarkdown, skim, transmission |
| QuickLookUI.framework | `_OBJC_METACLASS_$_QLPreviewProvider` | 4 | krita, qlmarkdown, skim, transmission |
| libswiftAppKit.dylib | `_$sSo6CGRectV6AppKitE4clipyyF` | 4 | fork, iterm2, marta, sequel-ace |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV13deleteForwardAEvgZ` | 4 | amethyst, betterdisplay, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f1AEvgZ` | 4 | amethyst, betterdisplay, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f2AEvgZ` | 4 | amethyst, betterdisplay, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f3AEvgZ` | 4 | amethyst, betterdisplay, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f4AEvgZ` | 4 | amethyst, betterdisplay, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f5AEvgZ` | 4 | amethyst, betterdisplay, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f6AEvgZ` | 4 | amethyst, betterdisplay, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f7AEvgZ` | 4 | amethyst, betterdisplay, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f8AEvgZ` | 4 | amethyst, betterdisplay, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f9AEvgZ` | 4 | amethyst, betterdisplay, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f10AEvgZ` | 4 | amethyst, betterdisplay, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f11AEvgZ` | 4 | amethyst, betterdisplay, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f12AEvgZ` | 4 | amethyst, betterdisplay, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f13AEvgZ` | 4 | amethyst, betterdisplay, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f14AEvgZ` | 4 | amethyst, betterdisplay, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f15AEvgZ` | 4 | amethyst, betterdisplay, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f16AEvgZ` | 4 | amethyst, betterdisplay, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f17AEvgZ` | 4 | amethyst, betterdisplay, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f18AEvgZ` | 4 | amethyst, betterdisplay, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f19AEvgZ` | 4 | amethyst, betterdisplay, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f20AEvgZ` | 4 | amethyst, betterdisplay, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3tabAEvgZ` | 4 | amethyst, betterdisplay, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV6deleteAEvgZ` | 4 | amethyst, betterdisplay, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV9backspaceAEvgZ` | 4 | amethyst, betterdisplay, shottr, teamviewer |
| Accessibility.framework | `_$s13Accessibility01_A13NotificationsPAAE4postyyF` | 3 | coteditor, fluidvoice, macwhisper |
| Accessibility.framework | `_$s13Accessibility0A12NotificationO12AnnouncementVAA01_A13NotificationsAAWP` | 3 | coteditor, fluidvoice, macwhisper |
| Accessibility.framework | `_$s13Accessibility0A12NotificationO12AnnouncementVMa` | 3 | coteditor, fluidvoice, macwhisper |
| Accessibility.framework | `_$s13Accessibility0A12NotificationO12AnnouncementVyAESScfC` | 3 | coteditor, fluidvoice, macwhisper |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesVMn` | 3 | bettertouchtool, coteditor, protonvpn |
| AppKit.framework | `_$sSo6CGRectV6AppKitE4clipyyF` | 3 | bartender, jordanbaird-ice, thaw |
| AppKit.framework | `_NSAccessibilityRoleDescriptionForUIElement` | 3 | iterm2, warp, zoom |
| AppKit.framework | `_NSAllRomanInputSourcesLocaleIdentifier` | 3 | coteditor, tableplus, zoom |
| AppKit.framework | `_NSDrawThreePartImage` | 3 | alt-tab, keycastr, vlc |
| AppKit.framework | `_NSFontTextStyleFootnote` | 3 | 1password, fluidvoice, supacode |
| AppKit.framework | `_NSGetAlertPanel` | 3 | android-file-transfer, steam, vlc |
| AppKit.framework | `_NSImageNameTouchBarDeleteTemplate` | 3 | hammerspoon, proxyman, skim |
| AppKit.framework | `_NSImageNameTouchBarGoUpTemplate` | 3 | hammerspoon, iterm2, sublime-text |
| AppKit.framework | `_NSImageNameTouchBarPlayPauseTemplate` | 3 | hammerspoon, microsoft-edge, microsoft-teams |
| AppKit.framework | `_NSImageNameTouchBarQuickLookTemplate` | 3 | hammerspoon, handbrake-app, skim |
| AppKit.framework | `_NSImageNameTouchBarRecordStopTemplate` | 3 | hammerspoon, handbrake-app, zoom |
| AppKit.framework | `_NSIsControllerMarker` | 3 | alt-tab, keycastr, skim |
| AppKit.framework | `_NSRectFillListWithGrays` | 3 | bettertouchtool, hex-fiend, proxyman |
| AppKit.framework | `_NSReleaseAlertPanel` | 3 | android-file-transfer, steam, vlc |
| AppKit.framework | `_NSSelectedTagBinding` | 3 | bettertouchtool, iina, scroll-reverser |
| AppKit.framework | `_NSSpeechResetProperty` | 3 | godot, hammerspoon, pixelorama |
| AppKit.framework | `_NSTextContentTypePassword` | 3 | cmux, multipass, xcodes-app |
| AppKit.framework | `_NSTextContentTypeUsername` | 3 | cmux, multipass, xcodes-app |
| AppKit.framework | `_NSWorkspaceLaunchConfigurationEnvironment` | 3 | 1password, godot, pixelorama |
| AppKit.framework | `_OBJC_CLASS_$_NSFontCollection` | 3 | alfred, iterm2, markedit |
| AppKit.framework | `_OBJC_CLASS_$_NSGlassEffectView` | 3 | markedit, supacode, thaw |
| AppKit.framework | `_OBJC_CLASS_$_NSPressureConfiguration` | 3 | loop, telegram, utm |
| AppKit.framework | `_OBJC_CLASS_$_NSSliderAccessory` | 3 | adobe-acrobat-reader, coteditor, spotify |
| AppKit.framework | `_OBJC_METACLASS_$_NSComboBoxCell` | 3 | daisydisk, iterm2, sequel-ace |
| AppKit.framework | `_OBJC_METACLASS_$_NSMatrix` | 3 | adobe-acrobat-reader, windows-app, zoom |
| AppKit.framework | `_OBJC_METACLASS_$_NSSwitch` | 3 | iina, mac-mouse-fix, tailscale-app |
| ApplicationServices.framework | `_AXAPIEnabled` | 3 | amethyst, bettertouchtool, hammerspoon |
| ApplicationServices.framework | `_GetFrontProcess` | 3 | adobe-acrobat-reader, bettertouchtool, zoom |
| ApplicationServices.framework | `_HIShapeCreateCopy` | 3 | bbedit, kicad, pgadmin4 |
| ApplicationServices.framework | `_HIShapeCreateDifference` | 3 | bbedit, kicad, pgadmin4 |
| ApplicationServices.framework | `_HIShapeCreateEmpty` | 3 | bbedit, kicad, pgadmin4 |
| ApplicationServices.framework | `_HIShapeCreateMutableWithRect` | 3 | bbedit, kicad, pgadmin4 |
| ApplicationServices.framework | `_HIShapeIntersectsRect` | 3 | bbedit, kicad, pgadmin4 |
| ApplicationServices.framework | `_HIShapeSetEmpty` | 3 | bbedit, kicad, pgadmin4 |
| ApplicationServices.framework | `_PasteboardSetPasteLocation` | 3 | espanso, kicad, prusaslicer |
| ApplicationServices.framework | `_SetFrontProcessWithOptions` | 3 | bettertouchtool, hammerspoon, zoom |
| Carbon.framework | `_AEResumeTheCurrentEvent` | 3 | adobe-acrobat-reader, bbedit, deluge |
| Carbon.framework | `_AESuspendTheCurrentEvent` | 3 | adobe-acrobat-reader, bbedit, deluge |
| Carbon.framework | `_GetCurrentButtonState` | 3 | adobe-acrobat-reader, bettertouchtool, kicad |
| Carbon.framework | `_GetCurrentEventButtonState` | 3 | bettertouchtool, blender, kicad |
| Carbon.framework | `_GetThemeMenuSeparatorHeight` | 3 | kicad, openscad, pgadmin4 |
| Carbon.framework | `_HIThemeDrawFocusRect` | 3 | espanso, kicad, prusaslicer |
| Carbon.framework | `_HIThemeDrawGrowBox` | 3 | kicad, openscad, pgadmin4 |
| Carbon.framework | `_HIThemeDrawTab` | 3 | kicad, openscad, pgadmin4 |
| Carbon.framework | `_HIThemeDrawTabPane` | 3 | kicad, openscad, pgadmin4 |
| Carbon.framework | `_HIThemeGetButtonContentBounds` | 3 | kicad, openscad, pgadmin4 |
| Carbon.framework | `_HIThemeGetGrowBoxBounds` | 3 | kicad, openscad, pgadmin4 |
| Carbon.framework | `_HIThemeSetFill` | 3 | kicad, openscad, pgadmin4 |
| Carbon.framework | `_kTISPropertyInputSourceIsEnableCapable` | 3 | clipy, easydict, maccy |
| QuickLook.framework | `_QLThumbnailCopyImage` | 3 | openmtp, pencil, teamviewer |
| QuickLook.framework | `_QLThumbnailCreate` | 3 | openmtp, pencil, teamviewer |
| QuickLook.framework | `_QLThumbnailDispatchAsync` | 3 | openmtp, pencil, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV10rightArrowAEvgZ` | 3 | duckduckgo, raycast, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f21AEvgZ` | 3 | amethyst, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f22AEvgZ` | 3 | amethyst, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f23AEvgZ` | 3 | amethyst, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f24AEvgZ` | 3 | amethyst, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f25AEvgZ` | 3 | amethyst, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f26AEvgZ` | 3 | amethyst, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f27AEvgZ` | 3 | amethyst, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f28AEvgZ` | 3 | amethyst, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f29AEvgZ` | 3 | amethyst, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f30AEvgZ` | 3 | amethyst, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f31AEvgZ` | 3 | amethyst, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f32AEvgZ` | 3 | amethyst, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f33AEvgZ` | 3 | amethyst, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f34AEvgZ` | 3 | amethyst, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f35AEvgZ` | 3 | amethyst, shottr, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV9leftArrowAEvgZ` | 3 | duckduckgo, raycast, teamviewer |
| libswiftQuickLookUI.dylib | `_$sSo14QLPreviewReplyC11QuickLookUIE17dataOfContentType11contentSize15createDataUsingAB07UniformI11Identifiers6UTTypeV_So6CGSizeV10Foundation0M0VABKctcfC` | 3 | krita, qlmarkdown, syntax-highlight |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE03appE0AcDE0dE10AttributesVmvg` | 2 | bettertouchtool, protonvpn |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE03appE0AcDE0dE10AttributesVmvpMV` | 2 | bettertouchtool, protonvpn |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV015BackgroundColorB0OAA19AttributedStringKeyADMc` | 2 | coteditor, maccy |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV015BackgroundColorB0ON` | 2 | coteditor, maccy |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesVAA0B5ScopeADMc` | 2 | bettertouchtool, protonvpn |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesVMa` | 2 | bettertouchtool, protonvpn |
| AppKit.framework | `_$s10Foundation9IndexPathV6AppKitE4itemSivs` | 2 | bartender, windows-app |
| AppKit.framework | `_$sSo7NSColorC6AppKitE8resourceAB21DeveloperToolsSupport13ColorResourceV_tcfC` | 2 | coteditor, thaw |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV10rightArrowAEvgZ` | 2 | coteditor, supacode |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV14carriageReturnAEvgZ` | 2 | coteditor, orbstack |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV5enterAEvgZ` | 2 | coteditor, orbstack |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV7upArrowAEvgZ` | 2 | coteditor, supacode |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV9downArrowAEvgZ` | 2 | coteditor, supacode |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV9leftArrowAEvgZ` | 2 | coteditor, supacode |
| AppKit.framework | `_NSBeginCriticalAlertSheet` | 2 | r-app, vlc |
| AppKit.framework | `_NSFontTextStyleCallout` | 2 | fluidvoice, supacode |
| AppKit.framework | `_NSFontTextStyleCaption2` | 2 | fluidvoice, supacode |
| AppKit.framework | `_NSImageNameTouchBarAddDetailTemplate` | 2 | gemini, hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarBookmarksTemplate` | 2 | hammerspoon, sublime-text |
| AppKit.framework | `_NSImageNameTouchBarColorPickerFill` | 2 | hammerspoon, iterm2 |
| AppKit.framework | `_NSImageNameTouchBarCommunicationAudioTemplate` | 2 | hammerspoon, zoom |
| AppKit.framework | `_NSImageNameTouchBarCommunicationVideoTemplate` | 2 | hammerspoon, zoom |
| AppKit.framework | `_NSImageNameTouchBarComposeTemplate` | 2 | hammerspoon, proxyman |
| AppKit.framework | `_NSImageNameTouchBarFastForwardTemplate` | 2 | hammerspoon, iina |
| AppKit.framework | `_NSImageNameTouchBarGoForwardTemplate` | 2 | hammerspoon, proxyman |
| AppKit.framework | `_NSImageNameTouchBarIconViewTemplate` | 2 | hammerspoon, monitorcontrol |
| AppKit.framework | `_NSImageNameTouchBarNewFolderTemplate` | 2 | hammerspoon, skim |
| AppKit.framework | `_NSImageNameTouchBarRewindTemplate` | 2 | hammerspoon, iina |
| AppKit.framework | `_NSImageNameTouchBarRotateLeftTemplate` | 2 | hammerspoon, telegram-desktop |
| AppKit.framework | `_NSImageNameTouchBarShareTemplate` | 2 | hammerspoon, marta |
| AppKit.framework | `_NSImageNameTouchBarSidebarTemplate` | 2 | fork, hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarSkipAhead15SecondsTemplate` | 2 | hammerspoon, iina |
| AppKit.framework | `_NSImageNameTouchBarSkipAhead30SecondsTemplate` | 2 | hammerspoon, iina |
| AppKit.framework | `_NSImageNameTouchBarSkipAheadTemplate` | 2 | hammerspoon, iina |
| AppKit.framework | `_NSImageNameTouchBarSkipBack15SecondsTemplate` | 2 | hammerspoon, iina |
| AppKit.framework | `_NSImageNameTouchBarSkipBack30SecondsTemplate` | 2 | hammerspoon, iina |
| AppKit.framework | `_NSImageNameTouchBarSkipBackTemplate` | 2 | hammerspoon, iina |
| AppKit.framework | `_NSImageNameTouchBarSlideshowTemplate` | 2 | hammerspoon, skim |
| AppKit.framework | `_NSImageNameTouchBarTextItalicTemplate` | 2 | hammerspoon, telegram-desktop |
| AppKit.framework | `_NSImageNameTouchBarTextListTemplate` | 2 | hammerspoon, typora |
| AppKit.framework | `_NSImageNameTouchBarUserTemplate` | 2 | hammerspoon, teamviewer |
| AppKit.framework | `_NSImageNameTouchBarVolumeDownTemplate` | 2 | hammerspoon, iina |
| AppKit.framework | `_NSImageNameTouchBarVolumeUpTemplate` | 2 | hammerspoon, iina |
| AppKit.framework | `_NSNoSelectionMarker` | 2 | alt-tab, keycastr |
| AppKit.framework | `_NSSelectionIndexPathsBinding` | 2 | gemini, orbstack |
| AppKit.framework | `_NSSharingServiceNamePostOnTwitter` | 2 | adobe-acrobat-reader, zotero |
| AppKit.framework | `_NSTextContentTypeURL` | 2 | coteditor, macwhisper |
| AppKit.framework | `_NSTextListMarkerDisc` | 2 | raycast, zoom |
| AppKit.framework | `_NSViewNoInstrinsicMetric` | 2 | gemini, zoom |
| AppKit.framework | `_NSWindowDidOrderOnScreenNotification` | 2 | kicad, pgadmin4 |
| AppKit.framework | `_NSWindowWillOrderOnScreenNotification` | 2 | kicad, pgadmin4 |
| AppKit.framework | `_OBJC_CLASS_$_NSMenuItemBadge` | 2 | cmux, coteditor |
| AppKit.framework | `_OBJC_CLASS_$_NSObjectController` | 2 | alt-tab, keycastr |
| AppKit.framework | `_OBJC_CLASS_$_NSPICTImageRep` | 2 | adobe-acrobat-reader, sequel-ace |
| AppKit.framework | `_OBJC_CLASS_$_NSPredicateEditorRowTemplate` | 2 | bettertouchtool, transmission |
| AppKit.framework | `_OBJC_CLASS_$_NSRuleEditor` | 2 | coteditor, sequel-ace |
| AppKit.framework | `_OBJC_CLASS_$_NSStoryboardSegue` | 2 | bettertouchtool, gemini |
| AppKit.framework | `_OBJC_CLASS_$_NSTableViewRowAction` | 2 | skim, sourcetree |
| AppKit.framework | `_OBJC_CLASS_$_NSTextInsertionIndicator` | 2 | coteditor, superwhisper |
| AppKit.framework | `_OBJC_CLASS_$_NSTextLayoutFragment` | 2 | coteditor, superwhisper |
| AppKit.framework | `_OBJC_CLASS_$_NSTextSelection` | 2 | duckduckgo, superwhisper |
| AppKit.framework | `_OBJC_METACLASS_$_NSGestureRecognizer` | 2 | bettertouchtool, superwhisper |
| AppKit.framework | `_OBJC_METACLASS_$_NSObjectController` | 2 | alt-tab, keycastr |
| AppKit.framework | `_OBJC_METACLASS_$_NSPressGestureRecognizer` | 2 | loop, sfm |
| AppKit.framework | `_OBJC_METACLASS_$_NSStoryboardSegue` | 2 | bettertouchtool, gemini |
| AppKit.framework | `_OBJC_METACLASS_$_NSTextBlock` | 2 | alfred, raycast |
| AppKit.framework | `_OBJC_METACLASS_$_NSTextLayoutFragment` | 2 | coteditor, superwhisper |
| AppKit.framework | `_OBJC_METACLASS_$_NSTreeController` | 2 | bbedit, bettertouchtool |
| AppKit.framework | `__NSPrependToKillRing` | 2 | calibre, vnote |
| ApplicationServices.framework | `_CoreDockGetAutoHideEnabled` | 2 | dockdoor, ghostty |
| ApplicationServices.framework | `_CoreDockGetOrientationAndPinning` | 2 | dockdoor, ghostty |
| ApplicationServices.framework | `_GetProcessBundleLocation` | 2 | android-file-transfer, bettertouchtool |
| ApplicationServices.framework | `_HIShapeSetWithShape` | 2 | kicad, pgadmin4 |
| ApplicationServices.framework | `_PMPrinterCopyHostName` | 2 | adobe-acrobat-reader, teamviewer |
| ApplicationServices.framework | `_PMWorkflowSubmitPDFWithSettings` | 2 | firefox, librewolf |
| Carbon.framework | `_AEInteractWithUser` | 2 | adobe-acrobat-reader, deluge |
| Carbon.framework | `_AHGotoPage` | 2 | bbedit, tunnelblick |
| Carbon.framework | `_AcquireFirstMatchingEventInQueue` | 2 | virtualbox, zotero |
| Carbon.framework | `_CheckEventQueueForUserCancel` | 2 | adobe-acrobat-reader, bbedit |
| Carbon.framework | `_GetKeys` | 2 | adobe-acrobat-reader, daisydisk |
| Carbon.framework | `_GetSystemUIMode` | 2 | adobe-acrobat-reader, bettertouchtool |
| Carbon.framework | `_GetThemeMenuItemExtra` | 2 | kicad, pgadmin4 |
| Carbon.framework | `_HIThemeApplyBackground` | 2 | kicad, pgadmin4 |
| Carbon.framework | `_HIThemeDrawBackground` | 2 | espanso, prusaslicer |
| Carbon.framework | `_HIThemeDrawMenuBackground` | 2 | libreoffice, webtorrent |
| Carbon.framework | `_HIThemeDrawMenuItem` | 2 | libreoffice, openscad |
| Carbon.framework | `_HIThemeDrawTextBox` | 2 | libreoffice, openscad |
| Carbon.framework | `_HIThemeGetUIFontType` | 2 | kicad, pgadmin4 |
| Carbon.framework | `_HIThemeSetStroke` | 2 | kicad, pgadmin4 |
| Carbon.framework | `_HIThemeSetTextFill` | 2 | kicad, pgadmin4 |
| Carbon.framework | `_OSADispose` | 2 | adobe-acrobat-reader, bbedit |
| Carbon.framework | `_OSAScriptError` | 2 | adobe-acrobat-reader, bbedit |
| QuickLook.framework | `_QLPreviewRequestSetDataRepresentation` | 2 | skim, transmission |
| QuickLook.framework | `_kQLPreviewPropertyAttachmentDataKey` | 2 | skim, transmission |
| QuickLook.framework | `_kQLPreviewPropertyAttachmentsKey` | 2 | skim, transmission |
| QuickLook.framework | `_kQLPreviewPropertyMIMETypeKey` | 2 | skim, transmission |
| QuickLook.framework | `_kQLPreviewPropertyTextEncodingNameKey` | 2 | skim, transmission |
| QuickLookUI.framework | `_OBJC_CLASS_$_QLPreviewReplyAttachment` | 2 | skim, transmission |
| libswiftAppKit.dylib | `_$sSo17NSAnimationEffectV6AppKitE4show10centeredAt4size17completionHandlerySo7CGPointV_So6CGSizeVyyctF` | 2 | iina, iterm2 |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV14carriageReturnAEvgZ` | 2 | sourcetree, teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyVSHACMc` | 2 | amethyst, shottr |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV015BackgroundColorB0OMn` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV015ForegroundColorB0OAA19AttributedStringKeyADMc` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV015ForegroundColorB0OMn` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV015ForegroundColorB0ON` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV15backgroundColorAF010BackgroundhB0Ovg` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV15backgroundColorAF010BackgroundhB0OvpMV` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV15foregroundColorAF010ForegroundhB0Ovg` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV15foregroundColorAF010ForegroundhB0OvpMV` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation22AttributeDynamicLookupO6AppKitE13dynamicMemberxs7KeyPathCyAA0B6ScopesOADE0eF10AttributesVxG_tcAA016AttributedStringI0Rzluig` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation22AttributeDynamicLookupO6AppKitE13dynamicMemberxs7KeyPathCyAA0B6ScopesOADE0eF10AttributesVxG_tcAA016AttributedStringI0RzluipMV` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation9IndexPathV6AppKitE4itemSivM` | 1 | windows-app |
| AppKit.framework | `_$s6AppKit28NSDiffableDataSourceSnapshotV11reloadItemsyySayq_GF` | 1 | macwhisper |
| AppKit.framework | `_$s6AppKit28NSDiffableDataSourceSnapshotV14reloadSectionsyySayxGF` | 1 | macwhisper |
| AppKit.framework | `_$s6AppKit29NSTableViewDiffableDataSourceC05tableD012cellProviderACyxq_GSo0cD0C_So6NSViewCAH_So0C6ColumnCSiq_tctcfc` | 1 | windows-app |
| AppKit.framework | `_$s6AppKit29NSTableViewDiffableDataSourceC5apply_20animatingDifferences10completionyAA010NSDiffablefG8SnapshotVyxq_G_SbyycSgtFTj` | 1 | windows-app |
| AppKit.framework | `_$s6AppKit29NSTableViewDiffableDataSourceCMn` | 1 | windows-app |
| AppKit.framework | `_$s6AppKit34NSCollectionViewDiffableDataSourceC013supplementaryD8ProviderSo0cD7Element_So6NSViewCXcSgSo0cD0C_SS10Foundation9IndexPathVtcSgvsTj` | 1 | windows-app |
| AppKit.framework | `_$s6AppKit34NSCollectionViewDiffableDataSourceC14itemIdentifier3forq_Sg10Foundation9IndexPathV_tFTj` | 1 | macwhisper |
| AppKit.framework | `_$s6AppKit34NSCollectionViewDiffableDataSourceC9indexPath3for10Foundation05IndexI0VSgq__tFTj` | 1 | macwhisper |
| AppKit.framework | `_$sSo10NSGradientC6AppKitE18colorsAndLocationsABSgSo7NSColorC_12CoreGraphics7CGFloatVtd_tcfC` | 1 | cmux |
| AppKit.framework | `_$sSo12NSPasteboardC6AppKitE14DetectedValuesV5linksSaySo11DDMatchLinkCGvg` | 1 | markedit |
| AppKit.framework | `_$sSo12NSPasteboardC6AppKitE14DetectedValuesV5linksSaySo11DDMatchLinkCGvpMV` | 1 | markedit |
| AppKit.framework | `_$sSo12NSPasteboardC6AppKitE14DetectedValuesVMa` | 1 | markedit |
| AppKit.framework | `_$sSo12NSPasteboardC6AppKitE14DetectedValuesVMn` | 1 | markedit |
| AppKit.framework | `_$sSo12NSPasteboardC6AppKitE14detectedValues3forAbCE08DetectedE0VShys14PartialKeyPathCyAGGG_tYaKF` | 1 | markedit |
| AppKit.framework | `_$sSo12NSPasteboardC6AppKitE14detectedValues3forAbCE08DetectedE0VShys14PartialKeyPathCyAGGG_tYaKFTu` | 1 | markedit |
| AppKit.framework | `_$sSo12NSStoryboardC6AppKitE28instantiateInitialController7creatorxSgAFSo7NSCoderCcSg_tSo06NSViewF0CRbzlF` | 1 | coteditor |
| AppKit.framework | `_$sSo13NSWindowLevelaSL6AppKitMc` | 1 | bartender |
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
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV12clearDisplayAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV13unicodeScalars7UnicodeO0G0Vvg` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3endAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV4helpAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV4homeAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV6pageUpAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV7backTabAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV7newlineAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV8formFeedAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV8pageDownAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV8rawValueAESi_tcfC` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV9clearLineAEvgZ` | 1 | coteditor |
| AppKit.framework | `_NSAccessibilityAnnotationTextAttribute` | 1 | hammerspoon |
| AppKit.framework | `_NSAccessibilityCenterTabStopMarkerTypeValue` | 1 | hammerspoon |
| AppKit.framework | `_NSAccessibilityCentimetersUnitValue` | 1 | hammerspoon |
| AppKit.framework | `_NSAccessibilityCustomTextAttribute` | 1 | hammerspoon |
| AppKit.framework | `_NSAccessibilityDecimalTabStopMarkerTypeValue` | 1 | hammerspoon |
| AppKit.framework | `_NSAccessibilityException` | 1 | daisydisk |
| AppKit.framework | `_NSAccessibilityExtrasMenuBarAttribute` | 1 | superwhisper |
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
| AppKit.framework | `_NSAccessibilityUnknownRole` | 1 | battery |
| AppKit.framework | `_NSAccessibilityUnknownSubrole` | 1 | vorssaint |
| AppKit.framework | `_NSAccessibilityUnknownUnitValue` | 1 | hammerspoon |
| AppKit.framework | `_NSAccessibilityVisibleNameKey` | 1 | libreoffice |
| AppKit.framework | `_NSAllowsNullArgumentBindingOption` | 1 | gemini |
| AppKit.framework | `_NSApplicationDidUpdateNotification` | 1 | vorssaint |
| AppKit.framework | `_NSAttributedStringBinding` | 1 | skim |
| AppKit.framework | `_NSBeginAlertSheet` | 1 | r-app |
| AppKit.framework | `_NSBeginInformationalAlertSheet` | 1 | vlc |
| AppKit.framework | `_NSCollectionElementKindInterItemGapIndicator` | 1 | windows-app |
| AppKit.framework | `_NSContentObjectsBinding` | 1 | teamviewer |
| AppKit.framework | `_NSContentSetBinding` | 1 | teamviewer |
| AppKit.framework | `_NSDarkGray` | 1 | vlc |
| AppKit.framework | `_NSDisplayNameBindingOption` | 1 | battery |
| AppKit.framework | `_NSDisplayPatternBindingOption` | 1 | battery |
| AppKit.framework | `_NSDrawWindowBackground` | 1 | openscad |
| AppKit.framework | `_NSFontCollectionRecentlyUsed` | 1 | markedit |
| AppKit.framework | `_NSFontNameBinding` | 1 | skim |
| AppKit.framework | `_NSFontSizeBinding` | 1 | skim |
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
| AppKit.framework | `_NSImageNameTouchBarDownloadTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarFolderCopyToTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarFolderMoveToTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarFolderTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarGetInfoTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarGoBackTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarHistoryTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarListViewTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarMailTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarNewMessageTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarOpenInBrowserTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarPlayheadTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarRecordStartTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarRefreshTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarRemoveTemplate` | 1 | keepingyouawake |
| AppKit.framework | `_NSImageNameTouchBarRotateRightTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarSkipToEndTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarSkipToStartTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarTagIconTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarTextBoldTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarTextBoxTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarTextCenterAlignTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarTextJustifiedAlignTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarTextLeftAlignTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarTextRightAlignTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarTextStrikethroughTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarTextUnderlineTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarUserAddTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarUserGroupTemplate` | 1 | hammerspoon |
| AppKit.framework | `_NSMenuItemImportFromDeviceIdentifier` | 1 | typora |
| AppKit.framework | `_NSModalPanelRunLoopMode` | 1 | vorssaint |
| AppKit.framework | `_NSMultipleValuesPlaceholderBindingOption` | 1 | battery |
| AppKit.framework | `_NSNoSelectionPlaceholderBindingOption` | 1 | battery |
| AppKit.framework | `_NSNotApplicablePlaceholderBindingOption` | 1 | battery |
| AppKit.framework | `_NSPasteboardTypeFindPanelSearchOptions` | 1 | cryptomator |
| AppKit.framework | `_NSPasteboardURLReadingContentsConformToTypesKey` | 1 | vorssaint |
| AppKit.framework | `_NSRunCriticalAlertPanel` | 1 | bettertouchtool |
| AppKit.framework | `_NSSharingServiceNameAddToAperture` | 1 | hammerspoon |
| AppKit.framework | `_NSSharingServiceNameAddToIPhoto` | 1 | hammerspoon |
| AppKit.framework | `_NSSharingServiceNameUseAsDesktopPicture` | 1 | hammerspoon |
| AppKit.framework | `_NSSpeechPhonemeSymbolsProperty` | 1 | hammerspoon |
| AppKit.framework | `_NSSpeechPitchModProperty` | 1 | hammerspoon |
| AppKit.framework | `_NSSpeechRecentSyncProperty` | 1 | hammerspoon |
| AppKit.framework | `_NSSpeechStatusOutputBusy` | 1 | hammerspoon |
| AppKit.framework | `_NSSpeechStatusOutputPaused` | 1 | hammerspoon |
| AppKit.framework | `_NSSpeechStatusProperty` | 1 | hammerspoon |
| AppKit.framework | `_NSTableViewRowViewKey` | 1 | skim |
| AppKit.framework | `_NSTextListMarkerCircle` | 1 | raycast |
| AppKit.framework | `_NSTextListMarkerLowercaseAlpha` | 1 | raycast |
| AppKit.framework | `_NSTextListMarkerLowercaseRoman` | 1 | raycast |
| AppKit.framework | `_NSTextListMarkerSquare` | 1 | raycast |
| AppKit.framework | `_NSWorkspaceDesktopImageAllowClippingKey` | 1 | vorssaint |
| AppKit.framework | `_NSWorkspaceDesktopImageScalingKey` | 1 | vorssaint |
| AppKit.framework | `_NSWorkspaceDidLaunchApplicationNotification` | 1 | vorssaint |
| AppKit.framework | `_NSWorkspaceDidMountNotification` | 1 | vorssaint |
| AppKit.framework | `_NSWorkspaceDidPerformFileOperationNotification` | 1 | bettertouchtool |
| AppKit.framework | `_NSWorkspaceDidUnmountNotification` | 1 | vorssaint |
| AppKit.framework | `_NSWorkspaceLaunchConfigurationAppleEvent` | 1 | windows-app |
| AppKit.framework | `_NSWorkspaceVolumeURLKey` | 1 | vorssaint |
| AppKit.framework | `_NSWorkspaceWillLaunchApplicationNotification` | 1 | vorssaint |
| AppKit.framework | `_OBJC_CLASS_$_NSAnimationManager` | 1 | mac-mouse-fix |
| AppKit.framework | `_OBJC_CLASS_$_NSButtonTouchBarItem` | 1 | proxyman |
| AppKit.framework | `_OBJC_CLASS_$_NSCollectionViewFlowLayoutInvalidationContext` | 1 | teamviewer |
| AppKit.framework | `_OBJC_CLASS_$_NSFrameView` | 1 | zoom |
| AppKit.framework | `_OBJC_CLASS_$_NSFunctionRow` | 1 | bettertouchtool |
| AppKit.framework | `_OBJC_CLASS_$_NSGlassEffectContainerView` | 1 | coteditor |
| AppKit.framework | `_OBJC_CLASS_$_NSGlyphGenerator` | 1 | r-app |
| AppKit.framework | `_OBJC_CLASS_$_NSGlyphInfo` | 1 | emacs-app |
| AppKit.framework | `_OBJC_CLASS_$_NSNextStepFrame` | 1 | zoom |
| AppKit.framework | `_OBJC_CLASS_$_NSPathCell` | 1 | gemini |
| AppKit.framework | `_OBJC_CLASS_$_NSScrubberProportionalLayout` | 1 | skim |
| AppKit.framework | `_OBJC_CLASS_$_NSSpeechRecognizer` | 1 | hammerspoon |
| AppKit.framework | `_OBJC_CLASS_$_NSSplitViewItemAccessoryViewController` | 1 | coteditor |
| AppKit.framework | `_OBJC_CLASS_$_NSTextAttachmentViewProvider` | 1 | bettertouchtool |
| AppKit.framework | `_OBJC_CLASS_$_NSTextCheckingController` | 1 | bbedit |
| AppKit.framework | `_OBJC_CLASS_$_NSTextFinder` | 1 | fork |
| AppKit.framework | `_OBJC_CLASS_$_NSTextSelectionNavigation` | 1 | duckduckgo |
| AppKit.framework | `_OBJC_CLASS_$_NSTintConfiguration` | 1 | proxyman |
| AppKit.framework | `_OBJC_CLASS_$_NSToolTipManager` | 1 | zoom |
| AppKit.framework | `_OBJC_CLASS_$_NSWindowTabGroup` | 1 | ghostty |
| AppKit.framework | `_OBJC_CLASS_$_NSWritingToolsCoordinator` | 1 | coteditor |
| AppKit.framework | `_OBJC_IVAR_$_NSApplication._appFlags` | 1 | xquartz |
| AppKit.framework | `_OBJC_METACLASS_$_NSFontPanel` | 1 | iterm2 |
| AppKit.framework | `_OBJC_METACLASS_$_NSGlyphGenerator` | 1 | r-app |
| AppKit.framework | `_OBJC_METACLASS_$_NSPanGestureRecognizer` | 1 | bettertouchtool |
| AppKit.framework | `_OBJC_METACLASS_$_NSPredicateEditorRowTemplate` | 1 | transmission |
| AppKit.framework | `_OBJC_METACLASS_$_NSRuleEditor` | 1 | sequel-ace |
| AppKit.framework | `_OBJC_METACLASS_$_NSSpeechRecognizer` | 1 | hammerspoon |
| AppKit.framework | `_OBJC_METACLASS_$_NSStepperCell` | 1 | vlc |
| AppKit.framework | `_OBJC_METACLASS_$_NSTextAttachmentViewProvider` | 1 | bettertouchtool |
| AppKit.framework | `_OBJC_METACLASS_$_NSTextCheckingController` | 1 | bbedit |
| AppKit.framework | `_OBJC_METACLASS_$_NSTextSelectionNavigation` | 1 | duckduckgo |
| AppKit.framework | `__NSDrawCarbonThemeBezel` | 1 | webtorrent |
| AppKit.framework | `__NSDrawCarbonThemeListBox` | 1 | webtorrent |
| ApplicationServices.framework | `_AXObserverGetTypeID` | 1 | hammerspoon |
| ApplicationServices.framework | `_AXUIElementPostKeyboardEvent` | 1 | cryptomator |
| ApplicationServices.framework | `_ContinueSpeech` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_CopyProcessName` | 1 | bettertouchtool |
| ApplicationServices.framework | `_CoreCursorSet` | 1 | raycast |
| ApplicationServices.framework | `_CoreCursorUnregisterAll` | 1 | raycast |
| ApplicationServices.framework | `_CoreDockIsMagnificationEnabled` | 1 | dockdoor |
| ApplicationServices.framework | `_CoreDockSetAutoHideEnabled` | 1 | ghostty |
| ApplicationServices.framework | `_CountVoices` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_GetIndVoice` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_GetVoiceDescription` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_HIShapeCreateXor` | 1 | bbedit |
| ApplicationServices.framework | `_HIShapeInset` | 1 | bbedit |
| ApplicationServices.framework | `_ICGetPref` | 1 | virtualbox |
| ApplicationServices.framework | `_ICMapFilename` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_ICStart` | 1 | virtualbox |
| ApplicationServices.framework | `_ICStop` | 1 | virtualbox |
| ApplicationServices.framework | `_KillProcess` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_LaunchApplication` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_PMGetScale` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_PMPrinterCopyDeviceURI` | 1 | teamviewer |
| ApplicationServices.framework | `_PMPrinterGetDriverCreator` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_PMPrinterGetDriverReleaseInfo` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_PMPrinterGetLanguageInfo` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_PMPrinterIsFavorite` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_PMSessionCreatePageFormatList` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_PMSessionCreatePrinterList` | 1 | adobe-acrobat-reader |
| ApplicationServices.framework | `_PMSessionSetError` | 1 | bbedit |
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
| Carbon.framework | `_ASCopySourceAttributes` | 1 | bettertouchtool |
| Carbon.framework | `_Button` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_CopyThemeIdentifier` | 1 | openscad |
| Carbon.framework | `_FindSpecificEventInQueue` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_GetApplicationTextEncoding` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_GetCurrentEventTime` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_GetMainEventLoop` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_GetMenuTrackingData` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_GetNumEventsInQueue` | 1 | openscad |
| Carbon.framework | `_HIGetMousePosition` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_HIMouseTrackingGetParameters` | 1 | bbedit |
| Carbon.framework | `_HIPointConvert` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_HIRectConvert` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_HISearchWindowShow` | 1 | alfred |
| Carbon.framework | `_HIThemeDrawChasingArrows` | 1 | bbedit |
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
| Carbon.framework | `_OSACoerceToDesc` | 1 | bbedit |
| Carbon.framework | `_OSACompile` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_OSADoEvent` | 1 | bbedit |
| Carbon.framework | `_OSAExecute` | 1 | bbedit |
| Carbon.framework | `_OSAExecuteEvent` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_OSAGetScriptDataFromURL` | 1 | bbedit |
| Carbon.framework | `_OSAGetScriptInfo` | 1 | bbedit |
| Carbon.framework | `_OSALoadScriptData` | 1 | bbedit |
| Carbon.framework | `_OSASetActiveProc` | 1 | bbedit |
| Carbon.framework | `_OSASetDefaultTarget` | 1 | bbedit |
| Carbon.framework | `_OSASetSendProc` | 1 | bbedit |
| Carbon.framework | `_OSAStoreFile` | 1 | bbedit |
| Carbon.framework | `_RemoveEventLoopTimer` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_RunCurrentEventLoop` | 1 | adobe-acrobat-reader |
| Carbon.framework | `_TISRegisterInputSource` | 1 | squirrel-app |
| Carbon.framework | `_kTISPropertyIconRef` | 1 | hammerspoon |
| Carbon.framework | `_kTISPropertyKeyLayoutName` | 1 | mac-mouse-fix |
| Carbon.framework | `_kTISTypeKeyboardInputMethodModeEnabled` | 1 | raycast |
| QuickLook.framework | `_OBJC_CLASS_$_QLPreviewController` | 1 | whatsapp |
| QuickLook.framework | `_OBJC_METACLASS_$_QLPreviewController` | 1 | whatsapp |
| QuickLook.framework | `_QLPreviewRequestGetGeneratorBundle` | 1 | skim |
| QuickLook.framework | `_QLPreviewRequestIsCancelled` | 1 | transmission |
| QuickLook.framework | `_QLThumbnailRequestCreateContext` | 1 | skim |
| QuickLook.framework | `_QLThumbnailRequestFlushContext` | 1 | skim |
| QuickLook.framework | `_QLThumbnailRequestGetGeneratorBundle` | 1 | skim |
| QuickLookThumbnailing.framework | `_OBJC_CLASS_$_QLThumbnailRepresentation` | 1 | whatsapp |
| libswiftAppKit.dylib | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV014ParagraphStyleB0OAA19AttributedStringKeyADMc` | 1 | bettertouchtool |
| libswiftAppKit.dylib | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV014ParagraphStyleB0ON` | 1 | bettertouchtool |
| libswiftAppKit.dylib | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV015ForegroundColorB0OAA19AttributedStringKeyADMc` | 1 | raycast |
| libswiftAppKit.dylib | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV015ForegroundColorB0OMn` | 1 | raycast |
| libswiftAppKit.dylib | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV015ForegroundColorB0ON` | 1 | raycast |
| libswiftAppKit.dylib | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV15foregroundColorAF010ForegroundhB0Ovg` | 1 | raycast |
| libswiftAppKit.dylib | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV15foregroundColorAF010ForegroundhB0OvpMV` | 1 | raycast |
| libswiftAppKit.dylib | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesVMn` | 1 | raycast |
| libswiftAppKit.dylib | `_$s10Foundation17URLResourceValuesV6AppKitE10labelColorSo7NSColorCSgvg` | 1 | fork |
| libswiftAppKit.dylib | `_$s10Foundation17URLResourceValuesV6AppKitE13effectiveIconyXlSgvg` | 1 | marta |
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
| libswiftAppKit.dylib | `_$sSo10NSGradientC6AppKitE18colorsAndLocationsABSgSo7NSColorC_12CoreGraphics7CGFloatVtd_tcfC` | 1 | bettertouchtool |
| libswiftAppKit.dylib | `_$sSo12NSStoryboardC6AppKitE28instantiateInitialController7creatorxSgAFSo7NSCoderCcSg_tSo08NSWindowF0CRbzlF` | 1 | sequel-ace |
| libswiftAppKit.dylib | `_$sSo13NSWindowLevelaSL6AppKitMc` | 1 | bettertouchtool |
| libswiftAppKit.dylib | `_$sSo16NSLayoutPriorityaSL6AppKitMc` | 1 | mac-mouse-fix |
| libswiftAppKit.dylib | `_$sSo6NSViewC6AppKitE12InvalidatingV12wrappedValue_AEy_xq_Gx_q_tcfC` | 1 | sfm |
| libswiftAppKit.dylib | `_$sSo6NSViewC6AppKitE12InvalidatingV18_enclosingInstance7wrapped7storagexqd___s24ReferenceWritableKeyPathCyqd__xGAJyqd__AEy_xq_GGtcABRbd__luiMZ` | 1 | sfm |
| libswiftAppKit.dylib | `_$sSo6NSViewC6AppKitE12InvalidatingV18_enclosingInstance7wrapped7storagexqd___s24ReferenceWritableKeyPathCyqd__xGAJyqd__AEy_xq_GGtcABRbd__luigZ` | 1 | sfm |
| libswiftAppKit.dylib | `_$sSo6NSViewC6AppKitE12InvalidatingV18_enclosingInstance7wrapped7storagexqd___s24ReferenceWritableKeyPathCyqd__xGAJyqd__AEy_xq_GGtcABRbd__luisZ` | 1 | sfm |
| libswiftAppKit.dylib | `_$sSo6NSViewC6AppKitE12InvalidatingVMa` | 1 | sfm |
| libswiftAppKit.dylib | `_$sSo6NSViewC6AppKitE12InvalidatingVMn` | 1 | sfm |
| libswiftAppKit.dylib | `_$sSo6NSViewC6AppKitE13InvalidationsO7DisplayVAC0A12InvalidatingACWP` | 1 | sfm |
| libswiftAppKit.dylib | `_$sSo6NSViewC6AppKitE13InvalidationsO7DisplayVAGycfC` | 1 | sfm |
| libswiftAppKit.dylib | `_$sSo6NSViewC6AppKitE13InvalidationsO7DisplayVMa` | 1 | sfm |
| libswiftAppKit.dylib | `_$sSo6NSViewC6AppKitE13InvalidationsO7DisplayVMn` | 1 | sfm |
| libswiftAppKit.dylib | `_$sSo7NSColorC6AppKitE16_colorLiteralRed5green4blue5alphaABSf_S3ftcfC` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV10deleteLineAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV10insertLineAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV10modeSwitchAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV10scrollLockAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV11printScreenAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV12clearDisplayAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV13lineSeparatorAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV15deleteCharacterAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV15insertCharacterAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV18paragraphSeparatorAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3endAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV4findAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV4helpAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV4homeAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV4menuAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV4nextAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV4prevAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV4redoAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV4stopAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV4undoAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV4userAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV5beginAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV5breakAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV5enterAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV5pauseAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV5printAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV5resetAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV6insertAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV6pageUpAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV6selectAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV6sysReqAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV6systemAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV7backTabAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV7executeAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV7newlineAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV8formFeedAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV8pageDownAEvgZ` | 1 | teamviewer |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV8rawValueSivg` | 1 | sourcetree |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV9clearLineAEvgZ` | 1 | teamviewer |
| libswiftQuickLookUI.dylib | `_$sSo14QLPreviewReplyC11QuickLookUIE14stringEncodingSS10FoundationE0G0Vvs` | 1 | qlmarkdown |
