# Lacunas de Beatriz (AppKit)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Beatriz (AppKit) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**466** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| AppKit.framework | `_NSAccessibilityMenuButtonRole` | 9 | coteditor, firefox, krita, libreoffice |
| AppKit.framework | `_NSModalPanelRunLoopMode` | 9 | firefox, iterm2, krita, musescore |
| AppKit.framework | `_NSAccessibilityUnknownRole` | 8 | firefox, krita, libreoffice, musescore |
| AppKit.framework | `_OBJC_CLASS_$_NSFontPanel` | 8 | coteditor, iterm2, krita, musescore |
| AppKit.framework | `_OBJC_CLASS_$_NSStepperCell` | 7 | krita, libreoffice, musescore, obs |
| ApplicationServices.framework | `_PasteboardCreate` | 7 | firefox, krita, musescore, obs |
| ApplicationServices.framework | `_PasteboardSynchronize` | 7 | firefox, krita, musescore, obs |
| Carbon.framework | `_TISCopyInputMethodKeyboardLayoutOverride` | 7 | firefox, krita, musescore, obs |
| AppKit.framework | `_NSAccessibilityApplicationRole` | 6 | krita, musescore, obs, qbittorrent |
| AppKit.framework | `_NSAccessibilitySplitGroupRole` | 6 | krita, libreoffice, musescore, obs |
| AppKit.framework | `_NSPasteboardTypeColor` | 6 | gimp, iterm2, musescore, obs |
| AppKit.framework | `_NSPasteboardTypeMultipleTextSelection` | 6 | coteditor, iterm2, musescore, obs |
| Carbon.framework | `_GetEventDispatcherTarget` | 6 | alt-tab, hiddenbar, iterm2, libreoffice |
| AppKit.framework | `_NSAccessibilityGrowAreaRole` | 5 | krita, musescore, obs, qbittorrent |
| AppKit.framework | `_NSAccessibilityRaiseAction` | 5 | krita, musescore, obs, qbittorrent |
| AppKit.framework | `_NSAccessibilityUnignoredChildrenForOnlyChild` | 5 | krita, musescore, obs, qbittorrent |
| AppKit.framework | `_NSAccessibilityValueIndicatorRole` | 5 | krita, musescore, obs, qbittorrent |
| AppKit.framework | `_NSApplicationLoad` | 5 | blender, libreoffice, monitorcontrol, mos |
| AppKit.framework | `_NSColorPanelColorDidChangeNotification` | 5 | krita, musescore, obs, qbittorrent |
| AppKit.framework | `_NSFileContentsPboardType` | 5 | krita, musescore, obs, qbittorrent |
| AppKit.framework | `_NSFontSetChangedNotification` | 5 | iterm2, musescore, obs, qbittorrent |
| AppKit.framework | `_NSObliquenessAttributeName` | 5 | krita, musescore, obs, qbittorrent |
| AppKit.framework | `_NSPasteboardTypePDF` | 5 | libreoffice, musescore, obs, qbittorrent |
| AppKit.framework | `_NSViewAnimationTargetKey` | 5 | firefox, iterm2, thunderbird, transmission |
| AppKit.framework | `_OBJC_CLASS_$_NSPrintOperation` | 5 | coteditor, firefox, iterm2, libreoffice |
| AppKit.framework | `_OBJC_CLASS_$_NSViewAnimation` | 5 | firefox, iterm2, thunderbird, transmission |
| ApplicationServices.framework | `_PasteboardClear` | 5 | krita, musescore, obs, qbittorrent |
| ApplicationServices.framework | `_PasteboardCopyItemFlavorData` | 5 | krita, musescore, obs, qbittorrent |
| ApplicationServices.framework | `_PasteboardCopyItemFlavors` | 5 | krita, musescore, obs, qbittorrent |
| ApplicationServices.framework | `_PasteboardCopyName` | 5 | krita, musescore, obs, qbittorrent |
| ApplicationServices.framework | `_PasteboardGetItemCount` | 5 | krita, musescore, obs, qbittorrent |
| ApplicationServices.framework | `_PasteboardGetItemIdentifier` | 5 | krita, musescore, obs, qbittorrent |
| ApplicationServices.framework | `_PasteboardPutItemFlavor` | 5 | krita, musescore, obs, qbittorrent |
| ApplicationServices.framework | `_PasteboardResolvePromises` | 5 | krita, musescore, obs, qbittorrent |
| ApplicationServices.framework | `_PasteboardSetPromiseKeeper` | 5 | krita, musescore, obs, qbittorrent |
| ApplicationServices.framework | `_PlotIconRefInContext` | 5 | krita, musescore, obs, qbittorrent |
| AppKit.framework | `_NSAccessibilityUnknownSubrole` | 4 | alt-tab, firefox, libreoffice, thunderbird |
| AppKit.framework | `_NSControlTintDidChangeNotification` | 4 | alt-tab, firefox, thunderbird, vlc |
| AppKit.framework | `_NSDragPboard` | 4 | gimp, inkscape, libreoffice, macdown |
| AppKit.framework | `_NSFrameRectWithWidthUsingOperation` | 4 | iterm2, monitorcontrol, mos, vlc |
| AppKit.framework | `_NSPasteboardTypeFont` | 4 | musescore, obs, qbittorrent, shotcut |
| AppKit.framework | `_NSPasteboardTypeRTFD` | 4 | musescore, obs, qbittorrent, shotcut |
| AppKit.framework | `_NSPasteboardTypeRuler` | 4 | musescore, obs, qbittorrent, shotcut |
| AppKit.framework | `_NSPasteboardTypeTabularText` | 4 | musescore, obs, qbittorrent, shotcut |
| AppKit.framework | `_NSPrintJobSavingURL` | 4 | firefox, libreoffice, macdown, thunderbird |
| AppKit.framework | `_NSPrintPanelAccessorySummaryItemDescriptionKey` | 4 | coteditor, firefox, iterm2, thunderbird |
| AppKit.framework | `_NSPrintPanelAccessorySummaryItemNameKey` | 4 | coteditor, firefox, iterm2, thunderbird |
| AppKit.framework | `_NSPrintSaveJob` | 4 | firefox, libreoffice, macdown, thunderbird |
| AppKit.framework | `_NSViewAnimationEffectKey` | 4 | firefox, thunderbird, transmission, vlc |
| AppKit.framework | `_NSViewAnimationFadeInEffect` | 4 | firefox, thunderbird, transmission, vlc |
| AppKit.framework | `_NSViewAnimationFadeOutEffect` | 4 | firefox, thunderbird, transmission, vlc |
| AppKit.framework | `_NSWorkspaceRecycleOperation` | 4 | iterm2, macdown, transmission, vlc |
| AppKit.framework | `_OBJC_CLASS_$_NSComboBoxCell` | 4 | firefox, iterm2, libreoffice, thunderbird |
| AppKit.framework | `_OBJC_CLASS_$_NSPageLayout` | 4 | firefox, krita, musescore, thunderbird |
| AppKit.framework | `_OBJC_CLASS_$_NSSliderCell` | 4 | iina, libreoffice, monitorcontrol, vlc |
| AppKit.framework | `_OBJC_CLASS_$_NSToolbarItemGroup` | 4 | coteditor, macdown, monitorcontrol, transmission |
| ApplicationServices.framework | `_PMRetain` | 4 | firefox, krita, musescore, thunderbird |
| ApplicationServices.framework | `_PasteboardGetItemFlavorFlags` | 4 | krita, musescore, obs, shotcut |
| Carbon.framework | `_TISCreateInputSourceList` | 4 | alt-tab, firefox, iterm2, thunderbird |
| AppKit.framework | `_NSAccessibilityStandardWindowSubrole` | 3 | libreoffice, rectangle, vlc |
| AppKit.framework | `_NSAccessibilitySwitchSubrole` | 3 | firefox, obs, thunderbird |
| AppKit.framework | `_NSBackingPropertyOldScaleFactorKey` | 3 | iina, krita, shotcut |
| AppKit.framework | `_NSBestDepth` | 3 | krita, monitorcontrol, mos |
| AppKit.framework | `_NSImageNameComputer` | 3 | iterm2, keepingyouawake, vlc |
| AppKit.framework | `_NSImageNameInfo` | 3 | keepingyouawake, monitorcontrol, mos |
| AppKit.framework | `_NSPrintCopies` | 3 | firefox, libreoffice, thunderbird |
| AppKit.framework | `_NSRectClip` | 3 | iterm2, monitorcontrol, mos |
| AppKit.framework | `_NSShowAnimationEffect` | 3 | iterm2, monitorcontrol, mos |
| AppKit.framework | `_NSTitleBinding` | 3 | coteditor, iina, rectangle |
| AppKit.framework | `_OBJC_CLASS_$_NSMagnificationGestureRecognizer` | 3 | iina, iterm2, mos |
| AppKit.framework | `_OBJC_CLASS_$_NSPrinter` | 3 | firefox, libreoffice, thunderbird |
| AppKit.framework | `_OBJC_CLASS_$_NSSharingServicePickerTouchBarItem` | 3 | coteditor, firefox, thunderbird |
| AppKit.framework | `_OBJC_CLASS_$_NSSwitch` | 3 | alt-tab, iina, stats |
| AppKit.framework | `_OBJC_CLASS_$_NSTouch` | 3 | alt-tab, iterm2, mos |
| AppKit.framework | `_OBJC_METACLASS_$_NSSliderCell` | 3 | iina, monitorcontrol, vlc |
| ApplicationServices.framework | `_PMSessionError` | 3 | krita, musescore, thunderbird |
| Carbon.framework | `_GetThemeMetric` | 3 | firefox, libreoffice, thunderbird |
| Carbon.framework | `_TISSelectInputSource` | 3 | firefox, iterm2, thunderbird |
| Carbon.framework | `_kTISPropertyInputSourceIsASCIICapable` | 3 | blender, firefox, thunderbird |
| QuickLookThumbnailing.framework | `_OBJC_CLASS_$_QLThumbnailProvider` | 3 | blender, krita, libreoffice |
| QuickLookThumbnailing.framework | `_OBJC_CLASS_$_QLThumbnailReply` | 3 | blender, krita, libreoffice |
| QuickLookThumbnailing.framework | `_OBJC_METACLASS_$_QLThumbnailProvider` | 3 | blender, krita, libreoffice |
| AppKit.framework | `_NSAccessibilityCloseButtonAttribute` | 2 | rectangle, vlc |
| AppKit.framework | `_NSAccessibilityFullScreenButtonAttribute` | 2 | rectangle, vlc |
| AppKit.framework | `_NSAccessibilityPopoverRole` | 2 | firefox, thunderbird |
| AppKit.framework | `_NSAccessibilitySystemDialogSubrole` | 2 | libreoffice, rectangle |
| AppKit.framework | `_NSAccessibilityToggleSubrole` | 2 | firefox, thunderbird |
| AppKit.framework | `_NSAccessibilityUnknownOrientationValue` | 2 | firefox, thunderbird |
| AppKit.framework | `_NSApplicationDidUpdateNotification` | 2 | alt-tab, coteditor |
| AppKit.framework | `_NSAvailableWindowDepths` | 2 | monitorcontrol, mos |
| AppKit.framework | `_NSColorPboardType` | 2 | inkscape, krita |
| AppKit.framework | `_NSDeviceIsScreen` | 2 | firefox, thunderbird |
| AppKit.framework | `_NSDraggingImageComponentIconKey` | 2 | iina, iterm2 |
| AppKit.framework | `_NSDrawThreePartImage` | 2 | alt-tab, vlc |
| AppKit.framework | `_NSFontFeatureSelectorIdentifierKey` | 2 | coteditor, iterm2 |
| AppKit.framework | `_NSFontFeatureTypeIdentifierKey` | 2 | coteditor, iterm2 |
| AppKit.framework | `_NSGeneralPboard` | 2 | gimp, inkscape |
| AppKit.framework | `_NSHiddenBinding` | 2 | coteditor, rectangle |
| AppKit.framework | `_NSImageNameMultipleDocuments` | 2 | iterm2, transmission |
| AppKit.framework | `_NSImageNameNetwork` | 2 | keepingyouawake, vlc |
| AppKit.framework | `_NSImageNameShareTemplate` | 2 | iterm2, transmission |
| AppKit.framework | `_NSImageNameStatusAvailable` | 2 | mos, transmission |
| AppKit.framework | `_NSMaxValueBinding` | 2 | coteditor, rectangle |
| AppKit.framework | `_NSMenuWillSendActionNotification` | 2 | firefox, thunderbird |
| AppKit.framework | `_NSOpenGLGetOption` | 2 | monitorcontrol, mos |
| AppKit.framework | `_NSOpenGLGetVersion` | 2 | monitorcontrol, mos |
| AppKit.framework | `_NSOpenGLSetOption` | 2 | monitorcontrol, mos |
| AppKit.framework | `_NSPrintSpoolJob` | 2 | firefox, thunderbird |
| AppKit.framework | `_NSRectClipList` | 2 | monitorcontrol, mos |
| AppKit.framework | `_NSRectFillListUsingOperation` | 2 | monitorcontrol, mos |
| AppKit.framework | `_NSRectFillListWithColorsUsingOperation` | 2 | monitorcontrol, mos |
| AppKit.framework | `_NSScrollViewDidLiveScrollNotification` | 2 | iina, iterm2 |
| AppKit.framework | `_NSTIFFPboardType` | 2 | inkscape, krita |
| AppKit.framework | `_NSTextViewDidChangeSelectionNotification` | 2 | coteditor, iterm2 |
| AppKit.framework | `_NSURLPboardType` | 2 | inkscape, krita |
| AppKit.framework | `_NSVerticalGlyphFormAttributeName` | 2 | firefox, thunderbird |
| AppKit.framework | `_NSViewAnimationEndFrameKey` | 2 | iterm2, vlc |
| AppKit.framework | `_NSViewAnimationStartFrameKey` | 2 | iterm2, vlc |
| AppKit.framework | `_NSWindowDidChangeScreenProfileNotification` | 2 | krita, shotcut |
| AppKit.framework | `_NSWindowDidExposeNotification` | 2 | krita, shotcut |
| AppKit.framework | `_NSWorkspaceDidDeactivateApplicationNotification` | 2 | gimp, iterm2 |
| AppKit.framework | `_OBJC_CLASS_$_NSEPSImageRep` | 2 | iterm2, libreoffice |
| AppKit.framework | `_OBJC_CLASS_$_NSHelpManager` | 2 | coteditor, transmission |
| AppKit.framework | `_OBJC_CLASS_$_NSMatrix` | 2 | iterm2, libreoffice |
| AppKit.framework | `_OBJC_CLASS_$_NSMenuToolbarItem` | 2 | coteditor, iina |
| AppKit.framework | `_OBJC_CLASS_$_NSSearchToolbarItem` | 2 | iina, iterm2 |
| ApplicationServices.framework | `_GetCurrentProcess` | 2 | firefox, thunderbird |
| ApplicationServices.framework | `_PMCreatePageFormat` | 2 | krita, musescore |
| ApplicationServices.framework | `_PMCreatePrintSettings` | 2 | krita, musescore |
| ApplicationServices.framework | `_PMCreateSession` | 2 | krita, musescore |
| ApplicationServices.framework | `_PMGetCollate` | 2 | krita, musescore |
| ApplicationServices.framework | `_PMGetCopies` | 2 | krita, musescore |
| ApplicationServices.framework | `_PMGetDuplex` | 2 | krita, musescore |
| ApplicationServices.framework | `_PMGetFirstPage` | 2 | krita, musescore |
| ApplicationServices.framework | `_PMGetLastPage` | 2 | krita, musescore |
| ApplicationServices.framework | `_PMGetUnadjustedPaperRect` | 2 | krita, musescore |
| ApplicationServices.framework | `_PMPaperCreateLocalizedName` | 2 | krita, musescore |
| ApplicationServices.framework | `_PMPrintSettingsGetJobName` | 2 | krita, musescore |
| ApplicationServices.framework | `_PMPrinterCopyDescriptionURL` | 2 | krita, musescore |
| ApplicationServices.framework | `_PMPrinterGetLocation` | 2 | krita, musescore |
| ApplicationServices.framework | `_PMPrinterGetMakeAndModelName` | 2 | krita, musescore |
| ApplicationServices.framework | `_PMPrinterGetMimeTypes` | 2 | krita, musescore |
| ApplicationServices.framework | `_PMPrinterGetName` | 2 | krita, musescore |
| ApplicationServices.framework | `_PMPrinterGetOutputResolution` | 2 | krita, musescore |
| ApplicationServices.framework | `_PMPrinterGetState` | 2 | krita, musescore |
| ApplicationServices.framework | `_PMPrinterIsDefault` | 2 | krita, musescore |
| ApplicationServices.framework | `_PMPrinterIsRemote` | 2 | krita, musescore |
| ApplicationServices.framework | `_PMPrinterPrintWithFile` | 2 | firefox, thunderbird |
| ApplicationServices.framework | `_PMServerCreatePrinterList` | 2 | krita, musescore |
| ApplicationServices.framework | `_PMSessionDefaultPageFormat` | 2 | krita, musescore |
| ApplicationServices.framework | `_PMSessionDefaultPrintSettings` | 2 | krita, musescore |
| ApplicationServices.framework | `_PMSetPageRange` | 2 | krita, musescore |
| ApplicationServices.framework | `_PasteboardCopyPasteLocation` | 2 | firefox, thunderbird |
| ApplicationServices.framework | `_SetFrontProcess` | 2 | firefox, thunderbird |
| Carbon.framework | `_GetCurrentEventKeyModifiers` | 2 | firefox, thunderbird |
| Carbon.framework | `_GetEventClass` | 2 | alt-tab, rectangle |
| Carbon.framework | `_HIThemeDrawButton` | 2 | firefox, thunderbird |
| Carbon.framework | `_SetSystemUIMode` | 2 | gimp, inkscape |
| Carbon.framework | `_TISCopyCurrentASCIICapableKeyboardInputSource` | 2 | firefox, thunderbird |
| Carbon.framework | `_TISCopyInputSourceForLanguage` | 2 | firefox, thunderbird |
| Carbon.framework | `_TSMGetActiveDocument` | 2 | firefox, thunderbird |
| Carbon.framework | `_kTISPropertyBundleID` | 2 | firefox, thunderbird |
| Carbon.framework | `_kTISPropertyInputSourceIsEnabled` | 2 | firefox, thunderbird |
| Carbon.framework | `_kTISPropertyInputSourceType` | 2 | firefox, thunderbird |
| Carbon.framework | `_kTISTypeKeyboardInputMode` | 2 | firefox, thunderbird |
| Carbon.framework | `_kTISTypeKeyboardLayout` | 2 | firefox, thunderbird |
| QuickLookUI.framework | `_OBJC_CLASS_$_QLPreviewProvider` | 2 | krita, transmission |
| QuickLookUI.framework | `_OBJC_CLASS_$_QLPreviewReply` | 2 | krita, transmission |
| QuickLookUI.framework | `_OBJC_METACLASS_$_QLPreviewProvider` | 2 | krita, transmission |
| libswiftAppKit.dylib | `_$sSo17NSAnimationEffectV6AppKitE4show10centeredAt4size17completionHandlerySo7CGPointV_So6CGSizeVyyctF` | 2 | iina, iterm2 |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV015BackgroundColorB0OAA19AttributedStringKeyADMc` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV015BackgroundColorB0OMn` | 1 | coteditor |
| AppKit.framework | `_$s10Foundation15AttributeScopesO6AppKitE0dE10AttributesV015BackgroundColorB0ON` | 1 | coteditor |
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
| AppKit.framework | `_$sSo10NSMenuItemC6AppKitE13sectionHeader5titleABSS_tFZ` | 1 | coteditor |
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
| AppKit.framework | `_$sSo7NSColorC6AppKitE8resourceAB21DeveloperToolsSupport13ColorResourceV_tcfC` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV10rightArrowAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV12clearDisplayAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV13deleteForwardAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV13unicodeScalars7UnicodeO0G0Vvg` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV14carriageReturnAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f1AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f2AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f3AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f4AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f5AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f6AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f7AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f8AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV2f9AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3endAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f10AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f11AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f12AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f13AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f14AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f15AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f16AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f17AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f18AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f19AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f20AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f21AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f22AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f23AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f24AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f25AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f26AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f27AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f28AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f29AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f30AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f31AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f32AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f33AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f34AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3f35AEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV3tabAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV4helpAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV4homeAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV5enterAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV6deleteAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV6pageUpAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV7backTabAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV7newlineAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV7upArrowAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV8formFeedAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV8pageDownAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV8rawValueAESi_tcfC` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV9backspaceAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV9clearLineAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV9downArrowAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyV9leftArrowAEvgZ` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyVMa` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyVMn` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyVSHACMc` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10SpecialKeyVSQACMc` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSEventC6AppKitE10specialKeyAbCE07SpecialE0VSgvg` | 1 | coteditor |
| AppKit.framework | `_$sSo7NSImageC6AppKitE8resourceAB21DeveloperToolsSupport13ImageResourceV_tcfC` | 1 | coteditor |
| AppKit.framework | `_NSAboutPanelOptionApplicationIcon` | 1 | iina |
| AppKit.framework | `_NSAboutPanelOptionApplicationName` | 1 | iina |
| AppKit.framework | `_NSAboutPanelOptionApplicationVersion` | 1 | iina |
| AppKit.framework | `_NSAccessibilityCloseButtonSubrole` | 1 | rectangle |
| AppKit.framework | `_NSAccessibilityFocusedWindowAttribute` | 1 | rectangle |
| AppKit.framework | `_NSAccessibilityFullScreenButtonSubrole` | 1 | rectangle |
| AppKit.framework | `_NSAccessibilityHiddenAttribute` | 1 | rectangle |
| AppKit.framework | `_NSAccessibilityMainAttribute` | 1 | rectangle |
| AppKit.framework | `_NSAccessibilityMinimizeButtonAttribute` | 1 | vlc |
| AppKit.framework | `_NSAccessibilityMinimizedAttribute` | 1 | rectangle |
| AppKit.framework | `_NSAccessibilityMisspelledTextAttribute` | 1 | libreoffice |
| AppKit.framework | `_NSAccessibilityPickAction` | 1 | libreoffice |
| AppKit.framework | `_NSAccessibilityRoleDescriptionForUIElement` | 1 | iterm2 |
| AppKit.framework | `_NSAccessibilityServesAsTitleForUIElementsAttribute` | 1 | libreoffice |
| AppKit.framework | `_NSAccessibilityShadowTextAttribute` | 1 | libreoffice |
| AppKit.framework | `_NSAccessibilitySharedCharacterRangeAttribute` | 1 | libreoffice |
| AppKit.framework | `_NSAccessibilitySharedTextUIElementsAttribute` | 1 | libreoffice |
| AppKit.framework | `_NSAccessibilityTextLinkSubrole` | 1 | libreoffice |
| AppKit.framework | `_NSAccessibilityUnderlineColorTextAttribute` | 1 | libreoffice |
| AppKit.framework | `_NSAccessibilityVisibleNameKey` | 1 | libreoffice |
| AppKit.framework | `_NSAccessibilityWindowsAttribute` | 1 | rectangle |
| AppKit.framework | `_NSAccessibilityZoomButtonSubrole` | 1 | rectangle |
| AppKit.framework | `_NSAllRomanInputSourcesLocaleIdentifier` | 1 | coteditor |
| AppKit.framework | `_NSBackgroundColorDocumentAttribute` | 1 | iterm2 |
| AppKit.framework | `_NSBeginCriticalAlertSheet` | 1 | vlc |
| AppKit.framework | `_NSBeginInformationalAlertSheet` | 1 | vlc |
| AppKit.framework | `_NSBitsPerSampleFromDepth` | 1 | krita |
| AppKit.framework | `_NSCharacterEncodingDocumentAttribute` | 1 | iterm2 |
| AppKit.framework | `_NSColorSpaceFromDepth` | 1 | krita |
| AppKit.framework | `_NSComboBoxSelectionIsChangingNotification` | 1 | iterm2 |
| AppKit.framework | `_NSConditionallySetsEnabledBindingOption` | 1 | keepingyouawake |
| AppKit.framework | `_NSContentArrayBinding` | 1 | iina |
| AppKit.framework | `_NSContentObjectBinding` | 1 | alt-tab |
| AppKit.framework | `_NSDarkGray` | 1 | vlc |
| AppKit.framework | `_NSDeviceCMYKColorSpace` | 1 | krita |
| AppKit.framework | `_NSDeviceWhiteColorSpace` | 1 | blender |
| AppKit.framework | `_NSDisableScreenUpdates` | 1 | vlc |
| AppKit.framework | `_NSEnableScreenUpdates` | 1 | vlc |
| AppKit.framework | `_NSFilesPromisePboardType` | 1 | krita |
| AppKit.framework | `_NSFilterPredicateBinding` | 1 | iina |
| AppKit.framework | `_NSFontFaceAttribute` | 1 | iterm2 |
| AppKit.framework | `_NSFontPboardType` | 1 | krita |
| AppKit.framework | `_NSFontSizeAttribute` | 1 | iterm2 |
| AppKit.framework | `_NSGetAlertPanel` | 1 | vlc |
| AppKit.framework | `_NSHTMLPboardType` | 1 | krita |
| AppKit.framework | `_NSImageCurrentFrame` | 1 | iterm2 |
| AppKit.framework | `_NSImageCurrentFrameDuration` | 1 | iterm2 |
| AppKit.framework | `_NSImageFrameCount` | 1 | iterm2 |
| AppKit.framework | `_NSImageHintUserInterfaceLayoutDirection` | 1 | obs |
| AppKit.framework | `_NSImageNameAdvanced` | 1 | keepingyouawake |
| AppKit.framework | `_NSImageNameColorPanel` | 1 | iterm2 |
| AppKit.framework | `_NSImageNameFollowLinkFreestandingTemplate` | 1 | iterm2 |
| AppKit.framework | `_NSImageNameLockLockedTemplate` | 1 | iterm2 |
| AppKit.framework | `_NSImageNameLockUnlockedTemplate` | 1 | iterm2 |
| AppKit.framework | `_NSImageNameMenuOnStateTemplate` | 1 | iterm2 |
| AppKit.framework | `_NSImageNamePreferencesGeneral` | 1 | keepingyouawake |
| AppKit.framework | `_NSImageNameQuickLookTemplate` | 1 | transmission |
| AppKit.framework | `_NSImageNameStatusPartiallyAvailable` | 1 | transmission |
| AppKit.framework | `_NSImageNameStatusUnavailable` | 1 | transmission |
| AppKit.framework | `_NSImageNameTouchBarColorPickerFill` | 1 | iterm2 |
| AppKit.framework | `_NSImageNameTouchBarExitFullScreenTemplate` | 1 | iina |
| AppKit.framework | `_NSImageNameTouchBarFastForwardTemplate` | 1 | iina |
| AppKit.framework | `_NSImageNameTouchBarGoDownTemplate` | 1 | iterm2 |
| AppKit.framework | `_NSImageNameTouchBarGoUpTemplate` | 1 | iterm2 |
| AppKit.framework | `_NSImageNameTouchBarIconViewTemplate` | 1 | monitorcontrol |
| AppKit.framework | `_NSImageNameTouchBarPauseTemplate` | 1 | iina |
| AppKit.framework | `_NSImageNameTouchBarPlayTemplate` | 1 | iina |
| AppKit.framework | `_NSImageNameTouchBarRemoveTemplate` | 1 | keepingyouawake |
| AppKit.framework | `_NSImageNameTouchBarRewindTemplate` | 1 | iina |
| AppKit.framework | `_NSImageNameTouchBarSkipAhead15SecondsTemplate` | 1 | iina |
| AppKit.framework | `_NSImageNameTouchBarSkipAhead30SecondsTemplate` | 1 | iina |
| AppKit.framework | `_NSImageNameTouchBarSkipAheadTemplate` | 1 | iina |
| AppKit.framework | `_NSImageNameTouchBarSkipBack15SecondsTemplate` | 1 | iina |
| AppKit.framework | `_NSImageNameTouchBarSkipBack30SecondsTemplate` | 1 | iina |
| AppKit.framework | `_NSImageNameTouchBarSkipBackTemplate` | 1 | iina |
| AppKit.framework | `_NSImageNameTouchBarVolumeDownTemplate` | 1 | iina |
| AppKit.framework | `_NSImageNameTouchBarVolumeUpTemplate` | 1 | iina |
| AppKit.framework | `_NSInkTextPboardType` | 1 | krita |
| AppKit.framework | `_NSIsControllerMarker` | 1 | alt-tab |
| AppKit.framework | `_NSMultipleTextSelectionPboardType` | 1 | krita |
| AppKit.framework | `_NSNamedColorSpace` | 1 | krita |
| AppKit.framework | `_NSNoSelectionMarker` | 1 | alt-tab |
| AppKit.framework | `_NSOutlineViewColumnDidResizeNotification` | 1 | iterm2 |
| AppKit.framework | `_NSOutlineViewDisclosureButtonKey` | 1 | iterm2 |
| AppKit.framework | `_NSOutlineViewSelectionDidChangeNotification` | 1 | transmission |
| AppKit.framework | `_NSPDFPboardType` | 1 | krita |
| AppKit.framework | `_NSPasteboardNameFont` | 1 | iterm2 |
| AppKit.framework | `_NSPasteboardURLReadingContentsConformToTypesKey` | 1 | blender |
| AppKit.framework | `_NSPatternColorSpace` | 1 | krita |
| AppKit.framework | `_NSPopUpButtonWillPopUpNotification` | 1 | iterm2 |
| AppKit.framework | `_NSPopoverDidCloseNotification` | 1 | alt-tab |
| AppKit.framework | `_NSPopoverDidShowNotification` | 1 | alt-tab |
| AppKit.framework | `_NSPostScriptPboardType` | 1 | krita |
| AppKit.framework | `_NSPredicateBinding` | 1 | iina |
| AppKit.framework | `_NSPredicateFormatBindingOption` | 1 | iina |
| AppKit.framework | `_NSPrintCancelJob` | 1 | libreoffice |
| AppKit.framework | `_NSPrintDetailedErrorReporting` | 1 | libreoffice |
| AppKit.framework | `_NSPrintHeaderAndFooter` | 1 | coteditor |
| AppKit.framework | `_NSPrintJobDisposition` | 1 | macdown |
| AppKit.framework | `_NSPrintMustCollate` | 1 | libreoffice |
| AppKit.framework | `_NSRTFDPboardType` | 1 | krita |
| AppKit.framework | `_NSRTFPboardType` | 1 | krita |
| AppKit.framework | `_NSRectFillListWithColors` | 1 | transmission |
| AppKit.framework | `_NSReleaseAlertPanel` | 1 | vlc |
| AppKit.framework | `_NSRulerPboardType` | 1 | krita |
| AppKit.framework | `_NSRunAlertPanel` | 1 | libreoffice |
| AppKit.framework | `_NSRunInformationalAlertPanel` | 1 | vlc |
| AppKit.framework | `_NSSelectedObjectBinding` | 1 | iina |
| AppKit.framework | `_NSSelectedTagBinding` | 1 | iina |
| AppKit.framework | `_NSSelectionIndexesBinding` | 1 | iina |
| AppKit.framework | `_NSSplitViewItemUnspecifiedDimension` | 1 | coteditor |
| AppKit.framework | `_NSTabularTextPboardType` | 1 | krita |
| AppKit.framework | `_NSTextContentTypeURL` | 1 | coteditor |
| AppKit.framework | `_NSTextEncodingNameDocumentOption` | 1 | iina |
| AppKit.framework | `_NSTextMovementUserInfoKey` | 1 | iterm2 |
| AppKit.framework | `_NSToolbarInspectorTrackingSeparatorItemIdentifier` | 1 | coteditor |
| AppKit.framework | `_NSToolbarPrintItemIdentifier` | 1 | coteditor |
| AppKit.framework | `_NSToolbarSidebarTrackingSeparatorItemIdentifier` | 1 | coteditor |
| AppKit.framework | `_NSToolbarToggleSidebarItemIdentifier` | 1 | coteditor |
| AppKit.framework | `_NSToolbarWritingToolsItemIdentifier` | 1 | coteditor |
| AppKit.framework | `_NSVCardPboardType` | 1 | krita |
| AppKit.framework | `_NSViewGlobalFrameDidChangeNotification` | 1 | krita |
| AppKit.framework | `_NSWindowDidEndSheetNotification` | 1 | iterm2 |
| AppKit.framework | `_NSWindowDidExitVersionBrowserNotification` | 1 | coteditor |
| AppKit.framework | `_NSWindowWillBeginSheetNotification` | 1 | iterm2 |
| AppKit.framework | `_NSWindowWillEnterVersionBrowserNotification` | 1 | coteditor |
| AppKit.framework | `_NSWindowWillMiniaturizeNotification` | 1 | vlc |
| AppKit.framework | `_NSWorkspaceDidHideApplicationNotification` | 1 | alt-tab |
| AppKit.framework | `_NSWorkspaceDidLaunchApplicationNotification` | 1 | monitorcontrol |
| AppKit.framework | `_NSWorkspaceDidMountNotification` | 1 | vlc |
| AppKit.framework | `_NSWorkspaceDidUnhideApplicationNotification` | 1 | alt-tab |
| AppKit.framework | `_NSWorkspaceDidUnmountNotification` | 1 | vlc |
| AppKit.framework | `_OBJC_CLASS_$_NSController` | 1 | alt-tab |
| AppKit.framework | `_OBJC_CLASS_$_NSFontCollection` | 1 | iterm2 |
| AppKit.framework | `_OBJC_CLASS_$_NSGestureRecognizer` | 1 | mos |
| AppKit.framework | `_OBJC_CLASS_$_NSGlassEffectContainerView` | 1 | coteditor |
| AppKit.framework | `_OBJC_CLASS_$_NSLevelIndicatorCell` | 1 | transmission |
| AppKit.framework | `_OBJC_CLASS_$_NSMenuItemBadge` | 1 | coteditor |
| AppKit.framework | `_OBJC_CLASS_$_NSObjectController` | 1 | alt-tab |
| AppKit.framework | `_OBJC_CLASS_$_NSPDFImageRep` | 1 | iterm2 |
| AppKit.framework | `_OBJC_CLASS_$_NSPanGestureRecognizer` | 1 | mos |
| AppKit.framework | `_OBJC_CLASS_$_NSPredicateEditorRowTemplate` | 1 | transmission |
| AppKit.framework | `_OBJC_CLASS_$_NSPressGestureRecognizer` | 1 | iterm2 |
| AppKit.framework | `_OBJC_CLASS_$_NSRotationGestureRecognizer` | 1 | mos |
| AppKit.framework | `_OBJC_CLASS_$_NSRuleEditor` | 1 | coteditor |
| AppKit.framework | `_OBJC_CLASS_$_NSScrubberFlowLayout` | 1 | iterm2 |
| AppKit.framework | `_OBJC_CLASS_$_NSSharingServicePickerToolbarItem` | 1 | coteditor |
| AppKit.framework | `_OBJC_CLASS_$_NSSliderAccessory` | 1 | coteditor |
| AppKit.framework | `_OBJC_CLASS_$_NSSplitViewItemAccessoryViewController` | 1 | coteditor |
| AppKit.framework | `_OBJC_CLASS_$_NSTextBlock` | 1 | iterm2 |
| AppKit.framework | `_OBJC_CLASS_$_NSTextInsertionIndicator` | 1 | coteditor |
| AppKit.framework | `_OBJC_CLASS_$_NSTextLayoutFragment` | 1 | coteditor |
| AppKit.framework | `_OBJC_CLASS_$_NSTextLineFragment` | 1 | coteditor |
| AppKit.framework | `_OBJC_CLASS_$_NSTextRange` | 1 | coteditor |
| AppKit.framework | `_OBJC_CLASS_$_NSWritingToolsCoordinator` | 1 | coteditor |
| AppKit.framework | `_OBJC_METACLASS_$_NSComboBoxCell` | 1 | iterm2 |
| AppKit.framework | `_OBJC_METACLASS_$_NSFontPanel` | 1 | iterm2 |
| AppKit.framework | `_OBJC_METACLASS_$_NSLevelIndicatorCell` | 1 | transmission |
| AppKit.framework | `_OBJC_METACLASS_$_NSMenuToolbarItem` | 1 | coteditor |
| AppKit.framework | `_OBJC_METACLASS_$_NSObjectController` | 1 | alt-tab |
| AppKit.framework | `_OBJC_METACLASS_$_NSPredicateEditorRowTemplate` | 1 | transmission |
| AppKit.framework | `_OBJC_METACLASS_$_NSStepperCell` | 1 | vlc |
| AppKit.framework | `_OBJC_METACLASS_$_NSSwitch` | 1 | iina |
| AppKit.framework | `_OBJC_METACLASS_$_NSTextLayoutFragment` | 1 | coteditor |
| AppKit.framework | `_OBJC_METACLASS_$_NSToolbarItemGroup` | 1 | transmission |
| ApplicationServices.framework | `_CGAcquireDisplayFadeReservation` | 1 | vlc |
| ApplicationServices.framework | `_CGDisplayFade` | 1 | vlc |
| ApplicationServices.framework | `_CGReleaseDisplayFadeReservation` | 1 | vlc |
| ApplicationServices.framework | `_CGSCreateRegisteredCursorImage` | 1 | vlc |
| ApplicationServices.framework | `_GetNextProcess` | 1 | vlc |
| ApplicationServices.framework | `_PMWorkflowSubmitPDFWithSettings` | 1 | firefox |
| ApplicationServices.framework | `_ProcessInformationCopyDictionary` | 1 | vlc |
| Carbon.framework | `_AESend` | 1 | qbittorrent |
| Carbon.framework | `_GetCurrentEventButtonState` | 1 | blender |
| Carbon.framework | `_HIThemeDrawFrame` | 1 | libreoffice |
| Carbon.framework | `_HIThemeDrawMenuBackground` | 1 | libreoffice |
| Carbon.framework | `_HIThemeDrawMenuItem` | 1 | libreoffice |
| Carbon.framework | `_HIThemeDrawTextBox` | 1 | libreoffice |
| Carbon.framework | `_kTISCategoryKeyboardInputSource` | 1 | iterm2 |
| Carbon.framework | `_kTISNotifyEnabledKeyboardInputSourcesChanged` | 1 | iterm2 |
| Carbon.framework | `_kTISPropertyInputSourceCategory` | 1 | iterm2 |
| QuickLook.framework | `_QLPreviewRequestIsCancelled` | 1 | transmission |
| QuickLook.framework | `_QLPreviewRequestSetDataRepresentation` | 1 | transmission |
| QuickLook.framework | `_kQLPreviewPropertyAttachmentDataKey` | 1 | transmission |
| QuickLook.framework | `_kQLPreviewPropertyAttachmentsKey` | 1 | transmission |
| QuickLook.framework | `_kQLPreviewPropertyMIMETypeKey` | 1 | transmission |
| QuickLook.framework | `_kQLPreviewPropertyTextEncodingNameKey` | 1 | transmission |
| QuickLookThumbnailing.framework | `_OBJC_CLASS_$_QLThumbnailGenerationRequest` | 1 | iina |
| QuickLookThumbnailing.framework | `_OBJC_CLASS_$_QLThumbnailGenerator` | 1 | iina |
| QuickLookUI.framework | `_OBJC_CLASS_$_QLPreviewReplyAttachment` | 1 | transmission |
| libswiftAppKit.dylib | `_$sSo6CGRectV6AppKitE4clipyyF` | 1 | iterm2 |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV7upArrowAEvgZ` | 1 | iterm2 |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV9downArrowAEvgZ` | 1 | iterm2 |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyVMa` | 1 | iterm2 |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyVMn` | 1 | iterm2 |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyVSQACMc` | 1 | iterm2 |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10specialKeyAbCE07SpecialE0VSgvg` | 1 | iterm2 |
| libswiftQuickLookUI.dylib | `_$sSo14QLPreviewReplyC11QuickLookUIE17dataOfContentType11contentSize15createDataUsingAB07UniformI11Identifiers6UTTypeV_So6CGSizeV10Foundation0M0VABKctcfC` | 1 | krita |
