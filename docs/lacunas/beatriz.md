# Lacunas de Beatriz (AppKit)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Beatriz (AppKit) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**96** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| Carbon.framework | `_GetEventDispatcherTarget` | 4 | alt-tab, hiddenbar, iterm2, rectangle |
| AppKit.framework | `_NSControlTintDidChangeNotification` | 3 | alt-tab, firefox, vlc |
| AppKit.framework | `_NSFrameRectWithWidthUsingOperation` | 3 | iterm2, mos, vlc |
| AppKit.framework | `_OBJC_CLASS_$_NSTouch` | 3 | alt-tab, iterm2, mos |
| AppKit.framework | `_OBJC_CLASS_$_NSViewAnimation` | 3 | firefox, iterm2, vlc |
| Carbon.framework | `_TISCreateInputSourceList` | 3 | alt-tab, firefox, iterm2 |
| AppKit.framework | `_NSApplicationLoad` | 2 | mos, vlc |
| AppKit.framework | `_NSDrawThreePartImage` | 2 | alt-tab, vlc |
| AppKit.framework | `_NSRectClip` | 2 | iterm2, mos |
| AppKit.framework | `_NSShowAnimationEffect` | 2 | iterm2, mos |
| AppKit.framework | `_NSWorkspaceRecycleOperation` | 2 | iterm2, vlc |
| AppKit.framework | `_OBJC_CLASS_$_NSComboBoxCell` | 2 | firefox, iterm2 |
| AppKit.framework | `_OBJC_CLASS_$_NSFontPanel` | 2 | iterm2, vlc |
| AppKit.framework | `_OBJC_CLASS_$_NSMagnificationGestureRecognizer` | 2 | iterm2, mos |
| AppKit.framework | `_OBJC_CLASS_$_NSPrintOperation` | 2 | firefox, iterm2 |
| Carbon.framework | `_GetEventClass` | 2 | alt-tab, rectangle |
| Carbon.framework | `_SetSystemUIMode` | 2 | gimp, inkscape |
| Carbon.framework | `_TISSelectInputSource` | 2 | firefox, iterm2 |
| AppKit.framework | `_NSAccessibilityRoleDescriptionForUIElement` | 1 | iterm2 |
| AppKit.framework | `_NSAvailableWindowDepths` | 1 | mos |
| AppKit.framework | `_NSBeginCriticalAlertSheet` | 1 | vlc |
| AppKit.framework | `_NSBeginInformationalAlertSheet` | 1 | vlc |
| AppKit.framework | `_NSBestDepth` | 1 | mos |
| AppKit.framework | `_NSDarkGray` | 1 | vlc |
| AppKit.framework | `_NSDisableScreenUpdates` | 1 | vlc |
| AppKit.framework | `_NSEnableScreenUpdates` | 1 | vlc |
| AppKit.framework | `_NSGetAlertPanel` | 1 | vlc |
| AppKit.framework | `_NSImageNameTouchBarColorPickerFill` | 1 | iterm2 |
| AppKit.framework | `_NSImageNameTouchBarGoDownTemplate` | 1 | iterm2 |
| AppKit.framework | `_NSImageNameTouchBarGoUpTemplate` | 1 | iterm2 |
| AppKit.framework | `_NSImageNameTouchBarRemoveTemplate` | 1 | keepingyouawake |
| AppKit.framework | `_NSIsControllerMarker` | 1 | alt-tab |
| AppKit.framework | `_NSNoSelectionMarker` | 1 | alt-tab |
| AppKit.framework | `_NSOutlineViewDisclosureButtonKey` | 1 | iterm2 |
| AppKit.framework | `_NSRectClipList` | 1 | mos |
| AppKit.framework | `_NSRectFillListUsingOperation` | 1 | mos |
| AppKit.framework | `_NSRectFillListWithColorsUsingOperation` | 1 | mos |
| AppKit.framework | `_NSReleaseAlertPanel` | 1 | vlc |
| AppKit.framework | `_NSRunInformationalAlertPanel` | 1 | vlc |
| AppKit.framework | `_OBJC_CLASS_$_NSController` | 1 | alt-tab |
| AppKit.framework | `_OBJC_CLASS_$_NSEPSImageRep` | 1 | iterm2 |
| AppKit.framework | `_OBJC_CLASS_$_NSFontCollection` | 1 | iterm2 |
| AppKit.framework | `_OBJC_CLASS_$_NSGestureRecognizer` | 1 | mos |
| AppKit.framework | `_OBJC_CLASS_$_NSMatrix` | 1 | iterm2 |
| AppKit.framework | `_OBJC_CLASS_$_NSObjectController` | 1 | alt-tab |
| AppKit.framework | `_OBJC_CLASS_$_NSPDFImageRep` | 1 | iterm2 |
| AppKit.framework | `_OBJC_CLASS_$_NSPageLayout` | 1 | firefox |
| AppKit.framework | `_OBJC_CLASS_$_NSPanGestureRecognizer` | 1 | mos |
| AppKit.framework | `_OBJC_CLASS_$_NSPressGestureRecognizer` | 1 | iterm2 |
| AppKit.framework | `_OBJC_CLASS_$_NSPrinter` | 1 | firefox |
| AppKit.framework | `_OBJC_CLASS_$_NSRotationGestureRecognizer` | 1 | mos |
| AppKit.framework | `_OBJC_CLASS_$_NSScrubberFlowLayout` | 1 | iterm2 |
| AppKit.framework | `_OBJC_CLASS_$_NSSharingServicePickerTouchBarItem` | 1 | firefox |
| AppKit.framework | `_OBJC_CLASS_$_NSSliderCell` | 1 | vlc |
| AppKit.framework | `_OBJC_CLASS_$_NSStepperCell` | 1 | vlc |
| AppKit.framework | `_OBJC_CLASS_$_NSSwitch` | 1 | alt-tab |
| AppKit.framework | `_OBJC_CLASS_$_NSTextBlock` | 1 | iterm2 |
| AppKit.framework | `_OBJC_METACLASS_$_NSComboBoxCell` | 1 | iterm2 |
| AppKit.framework | `_OBJC_METACLASS_$_NSFontPanel` | 1 | iterm2 |
| AppKit.framework | `_OBJC_METACLASS_$_NSObjectController` | 1 | alt-tab |
| AppKit.framework | `_OBJC_METACLASS_$_NSSliderCell` | 1 | vlc |
| AppKit.framework | `_OBJC_METACLASS_$_NSStepperCell` | 1 | vlc |
| ApplicationServices.framework | `_GetCurrentProcess` | 1 | firefox |
| ApplicationServices.framework | `_GetNextProcess` | 1 | vlc |
| ApplicationServices.framework | `_PMPrinterPrintWithFile` | 1 | firefox |
| ApplicationServices.framework | `_PMRetain` | 1 | firefox |
| ApplicationServices.framework | `_PMWorkflowSubmitPDFWithSettings` | 1 | firefox |
| ApplicationServices.framework | `_PasteboardCopyPasteLocation` | 1 | firefox |
| ApplicationServices.framework | `_PasteboardCreate` | 1 | firefox |
| ApplicationServices.framework | `_PasteboardSynchronize` | 1 | firefox |
| ApplicationServices.framework | `_ProcessInformationCopyDictionary` | 1 | vlc |
| ApplicationServices.framework | `_SetFrontProcess` | 1 | firefox |
| Carbon.framework | `_GetCurrentEventKeyModifiers` | 1 | firefox |
| Carbon.framework | `_GetThemeMetric` | 1 | firefox |
| Carbon.framework | `_HIThemeDrawButton` | 1 | firefox |
| Carbon.framework | `_TISCopyCurrentASCIICapableKeyboardInputSource` | 1 | firefox |
| Carbon.framework | `_TISCopyInputMethodKeyboardLayoutOverride` | 1 | firefox |
| Carbon.framework | `_TISCopyInputSourceForLanguage` | 1 | firefox |
| Carbon.framework | `_TSMGetActiveDocument` | 1 | firefox |
| Carbon.framework | `_kTISCategoryKeyboardInputSource` | 1 | iterm2 |
| Carbon.framework | `_kTISNotifyEnabledKeyboardInputSourcesChanged` | 1 | iterm2 |
| Carbon.framework | `_kTISPropertyBundleID` | 1 | firefox |
| Carbon.framework | `_kTISPropertyInputSourceCategory` | 1 | iterm2 |
| Carbon.framework | `_kTISPropertyInputSourceIsASCIICapable` | 1 | firefox |
| Carbon.framework | `_kTISPropertyInputSourceIsEnabled` | 1 | firefox |
| Carbon.framework | `_kTISPropertyInputSourceType` | 1 | firefox |
| Carbon.framework | `_kTISTypeKeyboardInputMode` | 1 | firefox |
| Carbon.framework | `_kTISTypeKeyboardLayout` | 1 | firefox |
| libswiftAppKit.dylib | `_$sSo17NSAnimationEffectV6AppKitE4show10centeredAt4size17completionHandlerySo7CGPointV_So6CGSizeVyyctF` | 1 | iterm2 |
| libswiftAppKit.dylib | `_$sSo6CGRectV6AppKitE4clipyyF` | 1 | iterm2 |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV7upArrowAEvgZ` | 1 | iterm2 |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyV9downArrowAEvgZ` | 1 | iterm2 |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyVMa` | 1 | iterm2 |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyVMn` | 1 | iterm2 |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10SpecialKeyVSQACMc` | 1 | iterm2 |
| libswiftAppKit.dylib | `_$sSo7NSEventC6AppKitE10specialKeyAbCE07SpecialE0VSgvg` | 1 | iterm2 |
