# Lacunas de Beatriz (AppKit)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Beatriz (AppKit) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**640** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| ApplicationServices.framework | `_PasteboardCreate` | 22 | audacity, db-browser-for-sqlite, dolphin, firefox |
| ApplicationServices.framework | `_PasteboardSynchronize` | 22 | audacity, db-browser-for-sqlite, dolphin, firefox |
| AppKit.framework | `_OBJC_CLASS_$_NSFontPanel` | 20 | audacity, coteditor, db-browser-for-sqlite, dolphin |
| Carbon.framework | `_TISCopyInputMethodKeyboardLayoutOverride` | 19 | audacity, db-browser-for-sqlite, dolphin, firefox |
| AppKit.framework | `_OBJC_CLASS_$_NSPageLayout` | 17 | android-studio, db-browser-for-sqlite, dbeaver-community, firefox |
| Carbon.framework | `_TISCreateInputSourceList` | 17 | alt-tab, android-studio, clipy, firefox |
| AppKit.framework | `_NSAccessibilityUnignoredChildrenForOnlyChild` | 16 | android-studio, audacity, db-browser-for-sqlite, dbeaver-community |
| AppKit.framework | `_NSFileContentsPboardType` | 15 | android-studio, audacity, db-browser-for-sqlite, dbeaver-community |
| Carbon.framework | `_GetEventDispatcherTarget` | 15 | alt-tab, betterdisplay, clipy, flycut |
| ApplicationServices.framework | `_PasteboardCopyItemFlavorData` | 14 | audacity, db-browser-for-sqlite, dolphin, keepassxc |
| ApplicationServices.framework | `_PasteboardCopyItemFlavors` | 14 | audacity, db-browser-for-sqlite, dolphin, keepassxc |
| ApplicationServices.framework | `_PasteboardGetItemCount` | 14 | audacity, db-browser-for-sqlite, dolphin, keepassxc |
| ApplicationServices.framework | `_PasteboardGetItemIdentifier` | 14 | audacity, db-browser-for-sqlite, dolphin, keepassxc |
| Carbon.framework | `_kTISPropertyInputSourceIsASCIICapable` | 14 | android-studio, blender, clipy, firefox |
| AppKit.framework | `_OBJC_CLASS_$_NSStepperCell` | 13 | db-browser-for-sqlite, dolphin, keepassxc, krita |
| ApplicationServices.framework | `_PMRetain` | 13 | db-browser-for-sqlite, firefox, floorp, krita |
| ApplicationServices.framework | `_PasteboardClear` | 13 | audacity, db-browser-for-sqlite, dolphin, keepassxc |
| ApplicationServices.framework | `_PasteboardCopyName` | 13 | audacity, db-browser-for-sqlite, dolphin, keepassxc |
| ApplicationServices.framework | `_PasteboardPutItemFlavor` | 13 | audacity, db-browser-for-sqlite, dolphin, keepassxc |
| ApplicationServices.framework | `_PasteboardResolvePromises` | 13 | audacity, db-browser-for-sqlite, dolphin, keepassxc |
| ApplicationServices.framework | `_PasteboardSetPromiseKeeper` | 13 | audacity, db-browser-for-sqlite, dolphin, keepassxc |
| ApplicationServices.framework | `_PlotIconRefInContext` | 13 | audacity, db-browser-for-sqlite, dolphin, keepassxc |
| Carbon.framework | `_kTISPropertyInputSourceType` | 13 | firefox, floorp, git-credential-manager, godot |
| AppKit.framework | `_OBJC_CLASS_$_NSPrintOperation` | 12 | android-studio, coteditor, dbeaver-community, firefox |
| AppKit.framework | `_OBJC_CLASS_$_NSViewAnimation` | 12 | firefox, floorp, iterm2, kicad |
| ApplicationServices.framework | `_PMSessionError` | 12 | db-browser-for-sqlite, floorp, krita, lmms |
| ApplicationServices.framework | `_PasteboardGetItemFlavorFlags` | 12 | audacity, db-browser-for-sqlite, dolphin, keepassxc |
| Carbon.framework | `_TISSelectInputSource` | 12 | android-studio, firefox, floorp, godot |
| Carbon.framework | `_kTISTypeKeyboardLayout` | 12 | firefox, floorp, git-credential-manager, godot |
| ApplicationServices.framework | `_GetCurrentProcess` | 11 | android-studio, cyberduck, dbeaver-community, deluge |
| ApplicationServices.framework | `_PMGetDuplex` | 11 | android-studio, db-browser-for-sqlite, dbeaver-community, kicad |
| Carbon.framework | `_kTISTypeKeyboardInputMode` | 11 | firefox, floorp, godot, hammerspoon |
| AppKit.framework | `_NSControlTintDidChangeNotification` | 10 | alt-tab, firefox, floorp, mullvad-browser |
| AppKit.framework | `_OBJC_CLASS_$_NSComboBoxCell` | 10 | firefox, floorp, iterm2, kicad |
| Carbon.framework | `_GetThemeMetric` | 10 | firefox, floorp, kicad, libreoffice |
| Carbon.framework | `_HIThemeDrawButton` | 10 | firefox, floorp, kicad, mullvad-browser |
| AppKit.framework | `_OBJC_CLASS_$_NSPrinter` | 9 | android-studio, dbeaver-community, firefox, floorp |
| ApplicationServices.framework | `_PMGetCollate` | 9 | db-browser-for-sqlite, kicad, krita, lmms |
| ApplicationServices.framework | `_PMGetCopies` | 9 | db-browser-for-sqlite, kicad, krita, lmms |
| ApplicationServices.framework | `_PMGetFirstPage` | 9 | db-browser-for-sqlite, kicad, krita, lmms |
| ApplicationServices.framework | `_PMGetLastPage` | 9 | db-browser-for-sqlite, kicad, krita, lmms |
| ApplicationServices.framework | `_PMGetUnadjustedPaperRect` | 9 | db-browser-for-sqlite, kicad, krita, lmms |
| ApplicationServices.framework | `_PMPrinterGetOutputResolution` | 9 | db-browser-for-sqlite, kicad, krita, lmms |
| ApplicationServices.framework | `_PMPrinterIsDefault` | 9 | db-browser-for-sqlite, kicad, krita, lmms |
| ApplicationServices.framework | `_PMServerCreatePrinterList` | 9 | db-browser-for-sqlite, kicad, krita, lmms |
| ApplicationServices.framework | `_PMSetPageRange` | 9 | db-browser-for-sqlite, kicad, krita, lmms |
| Carbon.framework | `_GetCurrentEventKeyModifiers` | 9 | dbeaver-community, firefox, floorp, kicad |
| Carbon.framework | `_TISCopyCurrentASCIICapableKeyboardInputSource` | 9 | clipy, cmux, firefox, floorp |
| AppKit.framework | `_NSApplicationLoad` | 8 | android-studio, bitwarden, blender, dbeaver-community |
| AppKit.framework | `_NSFrameRectWithWidthUsingOperation` | 8 | bitwarden, hex-fiend, iterm2, kicad |
| AppKit.framework | `_NSWorkspaceRecycleOperation` | 8 | brave-browser, hex-fiend, iterm2, macdown |
| ApplicationServices.framework | `_SetFrontProcess` | 8 | deluge, firefox, floorp, mullvad-browser |
| Carbon.framework | `_GetEventClass` | 8 | alt-tab, flycut, pencil, raycast |
| Carbon.framework | `_kTISPropertyInputSourceIsEnabled` | 8 | clipy, firefox, floorp, maccy |
| AppKit.framework | `_NSSpeechPitchBaseProperty` | 7 | godot, hammerspoon, mullvad-browser, pencil |
| AppKit.framework | `_OBJC_CLASS_$_NSMatrix` | 7 | bruno, iterm2, libreoffice, pencil |
| AppKit.framework | `_OBJC_CLASS_$_NSSharingServicePickerTouchBarItem` | 7 | coteditor, firefox, floorp, mullvad-browser |
| AppKit.framework | `_OBJC_CLASS_$_NSSliderCell` | 7 | betterdisplay, iina, libreoffice, microsoft-edge |
| ApplicationServices.framework | `_PMCreatePageFormat` | 7 | db-browser-for-sqlite, krita, lmms, musescore |
| ApplicationServices.framework | `_PMCreatePrintSettings` | 7 | db-browser-for-sqlite, krita, lmms, musescore |
| ApplicationServices.framework | `_PMCreateSession` | 7 | db-browser-for-sqlite, krita, lmms, musescore |
| ApplicationServices.framework | `_PMPaperCreateLocalizedName` | 7 | db-browser-for-sqlite, krita, lmms, musescore |
| ApplicationServices.framework | `_PMPrintSettingsGetJobName` | 7 | db-browser-for-sqlite, krita, lmms, musescore |
| ApplicationServices.framework | `_PMPrinterCopyDescriptionURL` | 7 | db-browser-for-sqlite, krita, lmms, musescore |
| ApplicationServices.framework | `_PMPrinterGetLocation` | 7 | db-browser-for-sqlite, krita, lmms, musescore |
| ApplicationServices.framework | `_PMPrinterGetMakeAndModelName` | 7 | db-browser-for-sqlite, krita, lmms, musescore |
| ApplicationServices.framework | `_PMPrinterGetMimeTypes` | 7 | db-browser-for-sqlite, krita, lmms, musescore |
| ApplicationServices.framework | `_PMPrinterGetName` | 7 | db-browser-for-sqlite, krita, lmms, musescore |
| ApplicationServices.framework | `_PMPrinterGetState` | 7 | db-browser-for-sqlite, krita, lmms, musescore |
| ApplicationServices.framework | `_PMPrinterIsRemote` | 7 | db-browser-for-sqlite, krita, lmms, musescore |
| ApplicationServices.framework | `_PMSessionDefaultPageFormat` | 7 | db-browser-for-sqlite, krita, lmms, musescore |
| ApplicationServices.framework | `_PMSessionDefaultPrintSettings` | 7 | db-browser-for-sqlite, krita, lmms, musescore |
| AppKit.framework | `_NSBestDepth` | 6 | bitwarden, db-browser-for-sqlite, krita, lmms |
| AppKit.framework | `_NSDisableScreenUpdates` | 6 | deluge, flycut, hammerspoon, lmms |
| AppKit.framework | `_NSEnableScreenUpdates` | 6 | deluge, flycut, hammerspoon, lmms |
| AppKit.framework | `_NSPostScriptPboardType` | 6 | android-studio, db-browser-for-sqlite, dbeaver-community, krita |
| AppKit.framework | `_NSVCardPboardType` | 6 | android-studio, db-browser-for-sqlite, dbeaver-community, krita |
| AppKit.framework | `_NSVoiceLocaleIdentifier` | 6 | godot, mullvad-browser, pencil, pixelorama |
| AppKit.framework | `_NSVoiceName` | 6 | godot, mullvad-browser, pencil, pixelorama |
| AppKit.framework | `_OBJC_METACLASS_$_NSSliderCell` | 6 | betterdisplay, iina, microsoft-edge, monitorcontrol |
| ApplicationServices.framework | `_PasteboardCopyPasteLocation` | 6 | firefox, floorp, mullvad-browser, thunderbird |
| Carbon.framework | `_TISCopyInputSourceForLanguage` | 6 | firefox, floorp, mullvad-browser, thunderbird |
| Carbon.framework | `_TSMGetActiveDocument` | 6 | firefox, floorp, mullvad-browser, thunderbird |
| Carbon.framework | `_kTISPropertyBundleID` | 6 | firefox, floorp, mullvad-browser, thunderbird |
| AppKit.framework | `_NSRectClip` | 5 | bitwarden, iterm2, monitorcontrol, mos |
| AppKit.framework | `_OBJC_CLASS_$_NSMagnificationGestureRecognizer` | 5 | iina, iterm2, kicad, mos |
| AppKit.framework | `_OBJC_CLASS_$_NSSwitch` | 5 | alt-tab, iina, orbstack, stats |
| Carbon.framework | `_SetSystemUIMode` | 5 | gimp, inkscape, kicad, prusaslicer |
| AppKit.framework | `_$sSo10NSMenuItemC6AppKitE13sectionHeader5titleABSS_tFZ` | 4 | 1password, cmux, coteditor, orbstack |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyVMa` | 4 | cmux, coteditor, maccy, orbstack |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyVMn` | 4 | cmux, coteditor, maccy, orbstack |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyVSQACMc` | 4 | cmux, coteditor, maccy, orbstack |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10specialKeyAbCE07SpecialE0VSgvg` | 4 | cmux, coteditor, maccy, orbstack |
| AppKit.framework | `_NSAboutPanelOptionApplicationName` | 4 | cc-switch, iina, microsoft-edge, zoom |
| AppKit.framework | `_NSAboutPanelOptionApplicationVersion` | 4 | cc-switch, iina, microsoft-edge, zoom |
| AppKit.framework | `_NSAccessibilityTextAlignmentAttribute` | 4 | godot, hammerspoon, pixelorama, zed |
| AppKit.framework | `_NSAccessibilityUnderlineColorTextAttribute` | 4 | godot, libreoffice, pixelorama, zed |
| AppKit.framework | `_NSImageNameTouchBarPauseTemplate` | 4 | hammerspoon, iina, microsoft-edge, zoom |
| AppKit.framework | `_NSImageNameTouchBarPlayTemplate` | 4 | hammerspoon, iina, microsoft-edge, zoom |
| AppKit.framework | `_NSInkTextPboardType` | 4 | db-browser-for-sqlite, krita, lmms, openscad |
| AppKit.framework | `_NSMultipleTextSelectionPboardType` | 4 | db-browser-for-sqlite, krita, lmms, openscad |
| AppKit.framework | `_NSPICTPboardType` | 4 | android-studio, dbeaver-community, lmms, xquartz |
| AppKit.framework | `_NSRectFillListUsingOperation` | 4 | bitwarden, monitorcontrol, mos, sequel-ace |
| AppKit.framework | `_NSRectFillListWithColorsUsingOperation` | 4 | bitwarden, hex-fiend, monitorcontrol, mos |
| AppKit.framework | `_NSRunAlertPanel` | 4 | cyberduck, flycut, libreoffice, xquartz |
| AppKit.framework | `_NSShowAnimationEffect` | 4 | bitwarden, iterm2, monitorcontrol, mos |
| AppKit.framework | `_NSSpeechRateProperty` | 4 | mullvad-browser, pencil, tor-browser, webtorrent |
| AppKit.framework | `_NSSpeechVolumeProperty` | 4 | mullvad-browser, pencil, tor-browser, webtorrent |
| AppKit.framework | `_OBJC_CLASS_$_NSATSTypesetter` | 4 | bruno, pencil, postman, webtorrent |
| AppKit.framework | `_OBJC_CLASS_$_NSLevelIndicatorCell` | 4 | kicad, prusaslicer, transmission, zoom |
| AppKit.framework | `_OBJC_CLASS_$_NSPanGestureRecognizer` | 4 | kicad, mos, prusaslicer, zoom |
| AppKit.framework | `_OBJC_METACLASS_$_NSATSTypesetter` | 4 | bruno, pencil, postman, webtorrent |
| ApplicationServices.framework | `_PMPrinterPrintWithFile` | 4 | firefox, floorp, thunderbird, zen |
| Carbon.framework | `_GetEventTime` | 4 | kicad, pencil, prusaslicer, webtorrent |
| Carbon.framework | `_HIThemeDrawFrame` | 4 | kicad, libreoffice, openscad, prusaslicer |
| Carbon.framework | `_HIThemeDrawTrack` | 4 | kicad, openscad, prusaslicer, webtorrent |
| Carbon.framework | `_kTISNotifyEnabledKeyboardInputSourcesChanged` | 4 | clipy, iterm2, linearmouse, maccy |
| QuickLookThumbnailing.framework | `_OBJC_CLASS_$_QLThumbnailGenerationRequest` | 4 | iina, raycast, tailscale-app, whatsapp |
| QuickLookThumbnailing.framework | `_OBJC_CLASS_$_QLThumbnailGenerator` | 4 | iina, raycast, tailscale-app, whatsapp |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f1AEvgZ` | 3 | cmux, coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f2AEvgZ` | 3 | cmux, coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f3AEvgZ` | 3 | cmux, coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f4AEvgZ` | 3 | cmux, coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f5AEvgZ` | 3 | cmux, coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f6AEvgZ` | 3 | cmux, coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f7AEvgZ` | 3 | cmux, coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f8AEvgZ` | 3 | cmux, coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f9AEvgZ` | 3 | cmux, coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f10AEvgZ` | 3 | cmux, coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f11AEvgZ` | 3 | cmux, coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f12AEvgZ` | 3 | cmux, coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f13AEvgZ` | 3 | cmux, coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f14AEvgZ` | 3 | cmux, coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f15AEvgZ` | 3 | cmux, coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f16AEvgZ` | 3 | cmux, coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f17AEvgZ` | 3 | cmux, coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f18AEvgZ` | 3 | cmux, coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f19AEvgZ` | 3 | cmux, coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f20AEvgZ` | 3 | cmux, coteditor, maccy |
| AppKit.framework | `_NSAboutPanelOptionApplicationIcon` | 3 | cc-switch, iina, microsoft-edge |
| AppKit.framework | `_NSAboutPanelOptionVersion` | 3 | cc-switch, microsoft-edge, zoom |
| AppKit.framework | `_NSAccessibilityLanguageTextAttribute` | 3 | godot, pixelorama, zed |
| AppKit.framework | `_NSAccessibilityStrikethroughColorTextAttribute` | 3 | godot, pixelorama, zed |
| AppKit.framework | `_NSAccessibilityUnignoredDescendant` | 3 | kicad, prusaslicer, zoom |
| AppKit.framework | `_NSAvailableWindowDepths` | 3 | bitwarden, monitorcontrol, mos |
| AppKit.framework | `_NSBitsPerSampleFromDepth` | 3 | db-browser-for-sqlite, krita, lmms |
| AppKit.framework | `_NSColorSpaceFromDepth` | 3 | db-browser-for-sqlite, krita, lmms |
| AppKit.framework | `_NSImageNameTouchBarGoDownTemplate` | 3 | hammerspoon, iterm2, sublime-text |
| AppKit.framework | `_NSImageNameTouchBarGoUpTemplate` | 3 | hammerspoon, iterm2, sublime-text |
| AppKit.framework | `_NSRectClipList` | 3 | bitwarden, monitorcontrol, mos |
| AppKit.framework | `_NSSpeechResetProperty` | 3 | godot, hammerspoon, pixelorama |
| AppKit.framework | `_NSWorkspaceLaunchConfigurationEnvironment` | 3 | 1password, godot, pixelorama |
| AppKit.framework | `_OBJC_CLASS_$_NSGestureRecognizer` | 3 | mos, obsidian, zoom |
| AppKit.framework | `_OBJC_CLASS_$_NSHelpManager` | 3 | coteditor, sequel-ace, transmission |
| AppKit.framework | `_OBJC_CLASS_$_NSInputManager` | 3 | android-studio, dbeaver-community, deluge |
| AppKit.framework | `_OBJC_CLASS_$_NSPressGestureRecognizer` | 3 | iterm2, kicad, prusaslicer |
| AppKit.framework | `_OBJC_CLASS_$_NSRotationGestureRecognizer` | 3 | kicad, mos, prusaslicer |
| AppKit.framework | `_OBJC_CLASS_$_NSScrubberFlowLayout` | 3 | android-studio, iterm2, microsoft-edge |
| AppKit.framework | `_OBJC_CLASS_$_NSTextBlock` | 3 | iterm2, raycast, zoom |
| AppKit.framework | `_OBJC_CLASS_$_NSTouch` | 3 | alt-tab, iterm2, mos |
| AppKit.framework | `_OBJC_CLASS_$_NSTreeNode` | 3 | orbstack, sequel-ace, zoom |
| ApplicationServices.framework | `_CoreDockSendNotification` | 3 | hammerspoon, linearmouse, raycast |
| ApplicationServices.framework | `_HIShapeCreateMutable` | 3 | kicad, openscad, prusaslicer |
| ApplicationServices.framework | `_HIShapeEnumerate` | 3 | kicad, openscad, prusaslicer |
| ApplicationServices.framework | `_HIShapeUnionWithRect` | 3 | kicad, openscad, prusaslicer |
| Carbon.framework | `_AddEventTypesToHandler` | 3 | maccy, pencil, webtorrent |
| Carbon.framework | `_GetCurrentEvent` | 3 | kicad, pencil, webtorrent |
| Carbon.framework | `_GetMainEventQueue` | 3 | openscad, pencil, webtorrent |
| Carbon.framework | `_HIThemeBeginFocus` | 3 | android-studio, cyberduck, dbeaver-community |
| Carbon.framework | `_HIThemeBrushCreateCGColor` | 3 | kicad, openscad, prusaslicer |
| Carbon.framework | `_HIThemeDrawPaneSplitter` | 3 | kicad, openscad, prusaslicer |
| Carbon.framework | `_HIThemeEndFocus` | 3 | android-studio, cyberduck, dbeaver-community |
| Carbon.framework | `_HIThemeGetButtonBackgroundBounds` | 3 | kicad, openscad, webtorrent |
| Carbon.framework | `_RemoveEventTypesFromHandler` | 3 | maccy, pencil, webtorrent |
| Carbon.framework | `_kTISPropertyInputModeID` | 3 | clipy, git-credential-manager, maccy |
| Carbon.framework | `_kTISPropertyInputSourceIsSelectCapable` | 3 | clipy, hammerspoon, maccy |
| Carbon.framework | `_kTISPropertyInputSourceIsSelected` | 3 | clipy, hammerspoon, maccy |
| QuickLookThumbnailing.framework | `_OBJC_CLASS_$_QLThumbnailProvider` | 3 | blender, krita, libreoffice |
| QuickLookThumbnailing.framework | `_OBJC_CLASS_$_QLThumbnailReply` | 3 | blender, krita, libreoffice |
| QuickLookThumbnailing.framework | `_OBJC_METACLASS_$_QLThumbnailProvider` | 3 | blender, krita, libreoffice |
| libswiftAppKit.dylib | `_$sSo13NSWindowLevela6AppKit01_cdD23NumericRawRepresentableACMc` | 3 | betterdisplay, raycast, zoom |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyVMa` | 3 | betterdisplay, iterm2, raycast |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyVMn` | 3 | betterdisplay, iterm2, raycast |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyVSQACMc` | 3 | betterdisplay, iterm2, raycast |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10specialKeyAbCE07SpecialE0VSgvg` | 3 | betterdisplay, iterm2, raycast |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV015BackgroundColorB0OAA19AttributedStringKeyADMc` | 2 | coteditor, maccy |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV015BackgroundColorB0ON` | 2 | coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV13deleteForwardAEvgZ` | 2 | coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV14carriageReturnAEvgZ` | 2 | coteditor, orbstack |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f21AEvgZ` | 2 | coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f22AEvgZ` | 2 | coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f23AEvgZ` | 2 | coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f24AEvgZ` | 2 | coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f25AEvgZ` | 2 | coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f26AEvgZ` | 2 | coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f27AEvgZ` | 2 | coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f28AEvgZ` | 2 | coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f29AEvgZ` | 2 | coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f30AEvgZ` | 2 | coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f31AEvgZ` | 2 | coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f32AEvgZ` | 2 | coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f33AEvgZ` | 2 | coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f34AEvgZ` | 2 | coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f35AEvgZ` | 2 | coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3tabAEvgZ` | 2 | coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV5enterAEvgZ` | 2 | coteditor, orbstack |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV6deleteAEvgZ` | 2 | coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV9backspaceAEvgZ` | 2 | coteditor, maccy |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyVSHACMc` | 2 | coteditor, maccy |
| AppKit.framework | `_$sSo7NSImageC6AppKitE8resourceAB21DeveloperToolsSupport13ImageResourceV_tcfC` | 2 | coteditor, jordanbaird-ice |
| AppKit.framework | `_NSAboutPanelOptionCredits` | 2 | cc-switch, maccy |
| AppKit.framework | `_NSAccessibilityRoleDescriptionForUIElement` | 2 | iterm2, zoom |
| AppKit.framework | `_NSAllRomanInputSourcesLocaleIdentifier` | 2 | coteditor, zoom |
| AppKit.framework | `_NSDrawThreePartImage` | 2 | alt-tab, vlc |
| AppKit.framework | `_NSFontTextStyleCaption1` | 2 | 1password, orbstack |
| AppKit.framework | `_NSImageNameBluetoothTemplate` | 2 | cc-switch, hammerspoon |
| AppKit.framework | `_NSImageNameFolderBurnable` | 2 | cc-switch, hammerspoon |
| AppKit.framework | `_NSImageNameFolderSmart` | 2 | cc-switch, hammerspoon |
| AppKit.framework | `_NSImageNameIChatTheaterTemplate` | 2 | cc-switch, hammerspoon |
| AppKit.framework | `_NSImageNameRefreshFreestandingTemplate` | 2 | cc-switch, hammerspoon |
| AppKit.framework | `_NSImageNameTouchBarBookmarksTemplate` | 2 | hammerspoon, sublime-text |
| AppKit.framework | `_NSImageNameTouchBarColorPickerFill` | 2 | hammerspoon, iterm2 |
| AppKit.framework | `_NSImageNameTouchBarCommunicationAudioTemplate` | 2 | hammerspoon, zoom |
| AppKit.framework | `_NSImageNameTouchBarCommunicationVideoTemplate` | 2 | hammerspoon, zoom |
| AppKit.framework | `_NSImageNameTouchBarExitFullScreenTemplate` | 2 | hammerspoon, iina |
| AppKit.framework | `_NSImageNameTouchBarFastForwardTemplate` | 2 | hammerspoon, iina |
| AppKit.framework | `_NSImageNameTouchBarIconViewTemplate` | 2 | hammerspoon, monitorcontrol |
| AppKit.framework | `_NSImageNameTouchBarPlayPauseTemplate` | 2 | hammerspoon, microsoft-edge |
| AppKit.framework | `_NSImageNameTouchBarRecordStopTemplate` | 2 | hammerspoon, zoom |
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
| AppKit.framework | `_NSSharingServiceNameSendViaAirDrop` | 2 | hammerspoon, microsoft-edge |
| AppKit.framework | `_NSSplitViewItemUnspecifiedDimension` | 2 | coteditor, zoom |
| AppKit.framework | `_NSTextListMarkerDisc` | 2 | raycast, zoom |
| AppKit.framework | `_NSVoiceIdentifier` | 2 | mullvad-browser, tor-browser |
| AppKit.framework | `_OBJC_CLASS_$_NSCustomImageRep` | 2 | flycut, sequel-ace |
| AppKit.framework | `_OBJC_CLASS_$_NSEPSImageRep` | 2 | iterm2, libreoffice |
| AppKit.framework | `_OBJC_CLASS_$_NSMenuItemBadge` | 2 | cmux, coteditor |
| AppKit.framework | `_OBJC_CLASS_$_NSPDFImageRep` | 2 | iterm2, zoom |
| AppKit.framework | `_OBJC_CLASS_$_NSRuleEditor` | 2 | coteditor, sequel-ace |
| AppKit.framework | `_OBJC_CLASS_$_NSSliderAccessory` | 2 | coteditor, spotify |
| AppKit.framework | `_OBJC_CLASS_$_NSTextLineFragment` | 2 | coteditor, raycast |
| AppKit.framework | `_OBJC_METACLASS_$_NSComboBoxCell` | 2 | iterm2, sequel-ace |
| AppKit.framework | `_OBJC_METACLASS_$_NSLevelIndicatorCell` | 2 | transmission, zoom |
| AppKit.framework | `_OBJC_METACLASS_$_NSSwitch` | 2 | iina, tailscale-app |
| AppKit.framework | `_OBJC_METACLASS_$_NSTreeNode` | 2 | sequel-ace, zoom |
| ApplicationServices.framework | `_DisposeSpeechChannel` | 2 | pencil, webtorrent |
| ApplicationServices.framework | `_GetNextProcess` | 2 | flycut, vlc |
| ApplicationServices.framework | `_HIShapeContainsPoint` | 2 | kicad, prusaslicer |
| ApplicationServices.framework | `_HIShapeCreateIntersection` | 2 | kicad, prusaslicer |
| ApplicationServices.framework | `_HIShapeCreateMutableCopy` | 2 | kicad, prusaslicer |
| ApplicationServices.framework | `_HIShapeCreateUnion` | 2 | kicad, prusaslicer |
| ApplicationServices.framework | `_HIShapeCreateWithRect` | 2 | kicad, prusaslicer |
| ApplicationServices.framework | `_HIShapeDifference` | 2 | kicad, prusaslicer |
| ApplicationServices.framework | `_HIShapeGetBounds` | 2 | kicad, prusaslicer |
| ApplicationServices.framework | `_HIShapeIntersect` | 2 | kicad, prusaslicer |
| ApplicationServices.framework | `_HIShapeIsEmpty` | 2 | kicad, prusaslicer |
| ApplicationServices.framework | `_HIShapeIsRectangular` | 2 | kicad, prusaslicer |
| ApplicationServices.framework | `_HIShapeOffset` | 2 | kicad, prusaslicer |
| ApplicationServices.framework | `_HIShapeReplacePathInCGContext` | 2 | kicad, prusaslicer |
| ApplicationServices.framework | `_HIShapeUnion` | 2 | kicad, prusaslicer |
| ApplicationServices.framework | `_NewSpeechChannel` | 2 | pencil, webtorrent |
| ApplicationServices.framework | `_PMGetPageRange` | 2 | kicad, prusaslicer |
| ApplicationServices.framework | `_PMGetUnadjustedPageRect` | 2 | kicad, prusaslicer |
| ApplicationServices.framework | `_PasteboardSetPasteLocation` | 2 | kicad, prusaslicer |
| ApplicationServices.framework | `_SetFrontProcessWithOptions` | 2 | hammerspoon, zoom |
| ApplicationServices.framework | `_SpeakCFString` | 2 | pencil, webtorrent |
| ApplicationServices.framework | `_SpeechBusy` | 2 | pencil, webtorrent |
| ApplicationServices.framework | `_StopSpeechAt` | 2 | pencil, webtorrent |
| Carbon.framework | `_AESend` | 2 | deluge, qbittorrent |
| Carbon.framework | `_CopyEvent` | 2 | pencil, webtorrent |
| Carbon.framework | `_CreateEvent` | 2 | pencil, webtorrent |
| Carbon.framework | `_GetCurrentEventButtonState` | 2 | blender, kicad |
| Carbon.framework | `_GetCurrentEventQueue` | 2 | pencil, webtorrent |
| Carbon.framework | `_GetEventMonitorTarget` | 2 | pencil, webtorrent |
| Carbon.framework | `_GetThemeMenuSeparatorHeight` | 2 | kicad, openscad |
| Carbon.framework | `_HIThemeDrawFocusRect` | 2 | kicad, prusaslicer |
| Carbon.framework | `_HIThemeDrawGroupBox` | 2 | kicad, openscad |
| Carbon.framework | `_HIThemeDrawGrowBox` | 2 | kicad, openscad |
| Carbon.framework | `_HIThemeDrawMenuBackground` | 2 | libreoffice, webtorrent |
| Carbon.framework | `_HIThemeDrawMenuItem` | 2 | libreoffice, openscad |
| Carbon.framework | `_HIThemeDrawSeparator` | 2 | kicad, openscad |
| Carbon.framework | `_HIThemeDrawTab` | 2 | kicad, openscad |
| Carbon.framework | `_HIThemeDrawTabPane` | 2 | kicad, openscad |
| Carbon.framework | `_HIThemeDrawTextBox` | 2 | libreoffice, openscad |
| Carbon.framework | `_HIThemeGetButtonContentBounds` | 2 | kicad, openscad |
| Carbon.framework | `_HIThemeGetGrowBoxBounds` | 2 | kicad, openscad |
| Carbon.framework | `_HIThemeSetFill` | 2 | kicad, openscad |
| Carbon.framework | `_PopSymbolicHotKeyMode` | 2 | flycut, sequel-ace |
| Carbon.framework | `_PostEventToQueue` | 2 | pencil, webtorrent |
| Carbon.framework | `_PushSymbolicHotKeyMode` | 2 | flycut, sequel-ace |
| Carbon.framework | `_RetainEvent` | 2 | pencil, webtorrent |
| Carbon.framework | `_SendEventToEventTargetWithOptions` | 2 | pencil, webtorrent |
| Carbon.framework | `_SetEventParameter` | 2 | pencil, webtorrent |
| Carbon.framework | `_SetEventTime` | 2 | pencil, webtorrent |
| Carbon.framework | `_kTISPropertyInputSourceIsEnableCapable` | 2 | clipy, maccy |
| QuickLookUI.framework | `_OBJC_CLASS_$_QLPreviewProvider` | 2 | krita, transmission |
| QuickLookUI.framework | `_OBJC_CLASS_$_QLPreviewReply` | 2 | krita, transmission |
| QuickLookUI.framework | `_OBJC_METACLASS_$_QLPreviewProvider` | 2 | krita, transmission |
| libswiftAppKit.dylib | `_$sSo17NSAnimationEffectV6AppKitE4show10centeredAt4size17completionHandlerySo7CGPointV_So6CGSizeVyyctF` | 2 | iina, iterm2 |
| libswiftAppKit.dylib | `_$sSo6CGRectV6AppKitE4clipyyF` | 2 | iterm2, sequel-ace |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV7upArrowAEvgZ` | 2 | iterm2, raycast |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV9downArrowAEvgZ` | 2 | iterm2, raycast |
| Accessibility.framework | `_$s13Accessibility01_A13NotificationsPAAE4postyyF` | 1 | coteditor |
| Accessibility.framework | `_$s13Accessibility0A12NotificationO12AnnouncementVAA01_A13NotificationsAAWP` | 1 | coteditor |
| Accessibility.framework | `_$s13Accessibility0A12NotificationO12AnnouncementVMa` | 1 | coteditor |
| Accessibility.framework | `_$s13Accessibility0A12NotificationO12AnnouncementVyAESScfC` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV015BackgroundColorB0OMn` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV015ForegroundColorB0OAA19AttributedStringKeyADMc` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV015ForegroundColorB0OMn` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV015ForegroundColorB0ON` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV15backgroundColorAF010BackgroundhB0Ovg` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV15backgroundColorAF010BackgroundhB0OvpMV` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV15foregroundColorAF010ForegroundhB0Ovg` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV15foregroundColorAF010ForegroundhB0OvpMV` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesVMn` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation22AttributeDynamicLookupO6AppKitE13dynamicMemberxs7KeyPathCyAA0B6ScopesOADE0eF10AttributesVxG_tcAA016AttributedStringI0Rzluig` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation22AttributeDynamicLookupO6AppKitE13dynamicMemberxs7KeyPathCyAA0B6ScopesOADE0eF10AttributesVxG_tcAA016AttributedStringI0RzluipMV` | 1 | coteditor |
| AppKit.framework | `_$sSo10NSGradientC6AppKitE18colorsAndLocationsABSgSo7NSColorC_12CoreGraphics7CGFloatVtd_tcfC` | 1 | cmux |
| AppKit.framework | `_$sSo12NSStoryboardC6AppKitE28instantiateInitialController7creatorxSgAFSo7NSCoderCcSg_tSo06NSViewF0CRbzlF` | 1 | coteditor |
| AppKit.framework | `_$sSo13NSWindowLevela6AppKit01_cdD23NumericRawRepresentableACMc` | 1 | jordanbaird-ice |
| AppKit.framework | `_$sSo16NSViewControllerC6AppKitE11ViewLoadingV18_enclosingInstance7wrapped7storagexqd___s24ReferenceWritableKeyPathCyqd__xGAJyqd__AEy_xGGtcABRbd__luigZ` | 1 | coteditor |
| AppKit.framework | `_$sSo16NSViewControllerC6AppKitE11ViewLoadingV18_enclosingInstance7wrapped7storagexqd___s24ReferenceWritableKeyPathCyqd__xGAJyqd__AEy_xGGtcABRbd__luisZ` | 1 | coteditor |
| AppKit.framework | `_$sSo16NSViewControllerC6AppKitE11ViewLoadingVAEy_xGycfC` | 1 | coteditor |
| AppKit.framework | `_$sSo16NSViewControllerC6AppKitE11ViewLoadingVMa` | 1 | coteditor |
| AppKit.framework | `_$sSo16NSViewControllerC6AppKitE11ViewLoadingVMn` | 1 | coteditor |
| AppKit.framework | `_$sSo6CGRectV6AppKitE4clipyyF` | 1 | jordanbaird-ice |
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
| AppKit.framework | `_$sSo7NSColorC6AppKitE8resourceAB21DeveloperToolsSupport13ColorResourceV_tcfC` | 1 | coteditor |
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
| AppKit.framework | `_NSAccessibilityUnknownUnitValue` | 1 | hammerspoon |
| AppKit.framework | `_NSAccessibilityVisibleNameKey` | 1 | libreoffice |
| AppKit.framework | `_NSBeginCriticalAlertSheet` | 1 | vlc |
| AppKit.framework | `_NSBeginInformationalAlertSheet` | 1 | vlc |
| AppKit.framework | `_NSDarkGray` | 1 | vlc |
| AppKit.framework | `_NSDrawWindowBackground` | 1 | openscad |
| AppKit.framework | `_NSFontDescriptorSystemDesignRounded` | 1 | cmux |
| AppKit.framework | `_NSFontTextStyleFootnote` | 1 | 1password |
| AppKit.framework | `_NSGetAlertPanel` | 1 | vlc |
| AppKit.framework | `_NSImageNameTouchBarAddDetailTemplate` | 1 | hammerspoon |
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
| AppKit.framework | `_NSImageNameTouchBarEnterFullScreenTemplate` | 1 | hammerspoon |
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
| AppKit.framework | `_NSImageNameTouchBarQuickLookTemplate` | 1 | hammerspoon |
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
| AppKit.framework | `_NSNoSelectionMarker` | 1 | alt-tab |
| AppKit.framework | `_NSOutlineViewDisclosureButtonKey` | 1 | iterm2 |
| AppKit.framework | `_NSRectFillListWithGrays` | 1 | hex-fiend |
| AppKit.framework | `_NSReleaseAlertPanel` | 1 | vlc |
| AppKit.framework | `_NSRunInformationalAlertPanel` | 1 | vlc |
| AppKit.framework | `_NSSelectedTagBinding` | 1 | iina |
| AppKit.framework | `_NSSelectionIndexPathsBinding` | 1 | orbstack |
| AppKit.framework | `_NSSharingServiceNameAddToAperture` | 1 | hammerspoon |
| AppKit.framework | `_NSSharingServiceNameAddToIPhoto` | 1 | hammerspoon |
| AppKit.framework | `_NSSharingServiceNameUseAsDesktopPicture` | 1 | hammerspoon |
| AppKit.framework | `_NSSpeechPhonemeSymbolsProperty` | 1 | hammerspoon |
| AppKit.framework | `_NSSpeechPitchModProperty` | 1 | hammerspoon |
| AppKit.framework | `_NSSpeechRecentSyncProperty` | 1 | hammerspoon |
| AppKit.framework | `_NSSpeechStatusOutputBusy` | 1 | hammerspoon |
| AppKit.framework | `_NSSpeechStatusOutputPaused` | 1 | hammerspoon |
| AppKit.framework | `_NSSpeechStatusProperty` | 1 | hammerspoon |
| AppKit.framework | `_NSTextContentTypeOneTimeCode` | 1 | cmux |
| AppKit.framework | `_NSTextContentTypePassword` | 1 | cmux |
| AppKit.framework | `_NSTextContentTypeURL` | 1 | coteditor |
| AppKit.framework | `_NSTextContentTypeUsername` | 1 | cmux |
| AppKit.framework | `_NSTextListMarkerCircle` | 1 | raycast |
| AppKit.framework | `_NSTextListMarkerLowercaseAlpha` | 1 | raycast |
| AppKit.framework | `_NSTextListMarkerLowercaseRoman` | 1 | raycast |
| AppKit.framework | `_NSTextListMarkerSquare` | 1 | raycast |
| AppKit.framework | `_NSViewNoInstrinsicMetric` | 1 | zoom |
| AppKit.framework | `_NSWindowDidOrderOnScreenNotification` | 1 | kicad |
| AppKit.framework | `_NSWindowWillOrderOnScreenNotification` | 1 | kicad |
| AppKit.framework | `_OBJC_CLASS_$_NSCollectionViewGridLayout` | 1 | zoom |
| AppKit.framework | `_OBJC_CLASS_$_NSController` | 1 | alt-tab |
| AppKit.framework | `_OBJC_CLASS_$_NSFontCollection` | 1 | iterm2 |
| AppKit.framework | `_OBJC_CLASS_$_NSFrameView` | 1 | zoom |
| AppKit.framework | `_OBJC_CLASS_$_NSGlassEffectContainerView` | 1 | coteditor |
| AppKit.framework | `_OBJC_CLASS_$_NSNextStepFrame` | 1 | zoom |
| AppKit.framework | `_OBJC_CLASS_$_NSObjectController` | 1 | alt-tab |
| AppKit.framework | `_OBJC_CLASS_$_NSPICTImageRep` | 1 | sequel-ace |
| AppKit.framework | `_OBJC_CLASS_$_NSPredicateEditorRowTemplate` | 1 | transmission |
| AppKit.framework | `_OBJC_CLASS_$_NSPressureConfiguration` | 1 | utm |
| AppKit.framework | `_OBJC_CLASS_$_NSSpeechRecognizer` | 1 | hammerspoon |
| AppKit.framework | `_OBJC_CLASS_$_NSSplitViewItemAccessoryViewController` | 1 | coteditor |
| AppKit.framework | `_OBJC_CLASS_$_NSTextInsertionIndicator` | 1 | coteditor |
| AppKit.framework | `_OBJC_CLASS_$_NSTextLayoutFragment` | 1 | coteditor |
| AppKit.framework | `_OBJC_CLASS_$_NSTextRange` | 1 | coteditor |
| AppKit.framework | `_OBJC_CLASS_$_NSToolTipManager` | 1 | zoom |
| AppKit.framework | `_OBJC_CLASS_$_NSTreeController` | 1 | orbstack |
| AppKit.framework | `_OBJC_CLASS_$_NSWindowTabGroup` | 1 | ghostty |
| AppKit.framework | `_OBJC_CLASS_$_NSWritingToolsCoordinator` | 1 | coteditor |
| AppKit.framework | `_OBJC_IVAR_$_NSApplication._appFlags` | 1 | xquartz |
| AppKit.framework | `_OBJC_METACLASS_$_NSFontPanel` | 1 | iterm2 |
| AppKit.framework | `_OBJC_METACLASS_$_NSMatrix` | 1 | zoom |
| AppKit.framework | `_OBJC_METACLASS_$_NSObjectController` | 1 | alt-tab |
| AppKit.framework | `_OBJC_METACLASS_$_NSPredicateEditorRowTemplate` | 1 | transmission |
| AppKit.framework | `_OBJC_METACLASS_$_NSRuleEditor` | 1 | sequel-ace |
| AppKit.framework | `_OBJC_METACLASS_$_NSSpeechRecognizer` | 1 | hammerspoon |
| AppKit.framework | `_OBJC_METACLASS_$_NSStepperCell` | 1 | vlc |
| AppKit.framework | `_OBJC_METACLASS_$_NSTextBlock` | 1 | raycast |
| AppKit.framework | `_OBJC_METACLASS_$_NSTextLayoutFragment` | 1 | coteditor |
| AppKit.framework | `__NSDrawCarbonThemeBezel` | 1 | webtorrent |
| AppKit.framework | `__NSDrawCarbonThemeListBox` | 1 | webtorrent |
| AppKit.framework | `__NSPrependToKillRing` | 1 | vnote |
| ApplicationServices.framework | `_AXAPIEnabled` | 1 | hammerspoon |
| ApplicationServices.framework | `_AXObserverGetTypeID` | 1 | hammerspoon |
| ApplicationServices.framework | `_CoreCursorSet` | 1 | raycast |
| ApplicationServices.framework | `_CoreCursorUnregisterAll` | 1 | raycast |
| ApplicationServices.framework | `_CoreDockGetAutoHideEnabled` | 1 | ghostty |
| ApplicationServices.framework | `_CoreDockGetOrientationAndPinning` | 1 | ghostty |
| ApplicationServices.framework | `_CoreDockSetAutoHideEnabled` | 1 | ghostty |
| ApplicationServices.framework | `_GetFrontProcess` | 1 | zoom |
| ApplicationServices.framework | `_HIShapeCreateCopy` | 1 | kicad |
| ApplicationServices.framework | `_HIShapeCreateDifference` | 1 | kicad |
| ApplicationServices.framework | `_HIShapeCreateEmpty` | 1 | kicad |
| ApplicationServices.framework | `_HIShapeCreateMutableWithRect` | 1 | kicad |
| ApplicationServices.framework | `_HIShapeIntersectsRect` | 1 | kicad |
| ApplicationServices.framework | `_HIShapeSetEmpty` | 1 | kicad |
| ApplicationServices.framework | `_HIShapeSetWithShape` | 1 | kicad |
| ApplicationServices.framework | `_PMWorkflowSubmitPDFWithSettings` | 1 | firefox |
| ApplicationServices.framework | `_ProcessInformationCopyDictionary` | 1 | vlc |
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
| Carbon.framework | `_AEGetInteractionAllowed` | 1 | deluge |
| Carbon.framework | `_AEGetTheCurrentEvent` | 1 | deluge |
| Carbon.framework | `_AEInteractWithUser` | 1 | deluge |
| Carbon.framework | `_AEProcessAppleEvent` | 1 | deluge |
| Carbon.framework | `_AEResetTimer` | 1 | deluge |
| Carbon.framework | `_AEResumeTheCurrentEvent` | 1 | deluge |
| Carbon.framework | `_AESetInteractionAllowed` | 1 | deluge |
| Carbon.framework | `_AESetTheCurrentEvent` | 1 | deluge |
| Carbon.framework | `_AESuspendTheCurrentEvent` | 1 | deluge |
| Carbon.framework | `_AHLookupAnchor` | 1 | xquartz |
| Carbon.framework | `_CopyThemeIdentifier` | 1 | openscad |
| Carbon.framework | `_GetCurrentButtonState` | 1 | kicad |
| Carbon.framework | `_GetNumEventsInQueue` | 1 | openscad |
| Carbon.framework | `_GetThemeMenuItemExtra` | 1 | kicad |
| Carbon.framework | `_HIThemeApplyBackground` | 1 | kicad |
| Carbon.framework | `_HIThemeDrawBackground` | 1 | prusaslicer |
| Carbon.framework | `_HIThemeDrawMenuBarBackground` | 1 | openscad |
| Carbon.framework | `_HIThemeDrawMenuSeparator` | 1 | openscad |
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
| Carbon.framework | `_HIThemeGetUIFontType` | 1 | kicad |
| Carbon.framework | `_HIThemeGetWindowShape` | 1 | openscad |
| Carbon.framework | `_HIThemeHitTestScrollBarArrows` | 1 | openscad |
| Carbon.framework | `_HIThemeHitTestTrack` | 1 | openscad |
| Carbon.framework | `_HIThemeSetStroke` | 1 | kicad |
| Carbon.framework | `_HIThemeSetTextFill` | 1 | kicad |
| Carbon.framework | `_TISDisableInputSource` | 1 | android-studio |
| Carbon.framework | `_TISEnableInputSource` | 1 | android-studio |
| Carbon.framework | `_kTISCategoryKeyboardInputSource` | 1 | iterm2 |
| Carbon.framework | `_kTISPropertyIconRef` | 1 | hammerspoon |
| Carbon.framework | `_kTISPropertyInputSourceCategory` | 1 | iterm2 |
| Carbon.framework | `_kTISTypeKeyboardInputMethodModeEnabled` | 1 | raycast |
| QuickLook.framework | `_OBJC_CLASS_$_QLPreviewController` | 1 | whatsapp |
| QuickLook.framework | `_OBJC_METACLASS_$_QLPreviewController` | 1 | whatsapp |
| QuickLook.framework | `_QLPreviewRequestIsCancelled` | 1 | transmission |
| QuickLook.framework | `_QLPreviewRequestSetDataRepresentation` | 1 | transmission |
| QuickLook.framework | `_QLThumbnailCopyImage` | 1 | pencil |
| QuickLook.framework | `_QLThumbnailCreate` | 1 | pencil |
| QuickLook.framework | `_QLThumbnailDispatchAsync` | 1 | pencil |
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
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV13deleteForwardAEvgZ` | 1 | betterdisplay |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f1AEvgZ` | 1 | betterdisplay |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f2AEvgZ` | 1 | betterdisplay |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f3AEvgZ` | 1 | betterdisplay |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f4AEvgZ` | 1 | betterdisplay |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f5AEvgZ` | 1 | betterdisplay |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f6AEvgZ` | 1 | betterdisplay |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f7AEvgZ` | 1 | betterdisplay |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f8AEvgZ` | 1 | betterdisplay |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f9AEvgZ` | 1 | betterdisplay |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f10AEvgZ` | 1 | betterdisplay |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f11AEvgZ` | 1 | betterdisplay |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f12AEvgZ` | 1 | betterdisplay |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f13AEvgZ` | 1 | betterdisplay |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f14AEvgZ` | 1 | betterdisplay |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f15AEvgZ` | 1 | betterdisplay |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f16AEvgZ` | 1 | betterdisplay |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f17AEvgZ` | 1 | betterdisplay |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f18AEvgZ` | 1 | betterdisplay |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f19AEvgZ` | 1 | betterdisplay |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f20AEvgZ` | 1 | betterdisplay |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV3tabAEvgZ` | 1 | betterdisplay |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV6deleteAEvgZ` | 1 | betterdisplay |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV9backspaceAEvgZ` | 1 | betterdisplay |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV9leftArrowAEvgZ` | 1 | raycast |
| libswiftQuickLookUI.dylib | `_$sSo14QLPreviewReplyC11QuickLookUIE17dataOfContentType11contentSize15createDataUsingAB07UniformI11Identifiers6UTTypeV_So6CGSizeV10Foundation0M0VABKctcfC` | 1 | krita |
