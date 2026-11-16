# Lacunas de Mariana (CG, Core Text, ImageIO, PDFKit)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Mariana (CG, Core Text, ImageIO, PDFKit) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**142** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| CoreGraphics.framework | `_CGBitmapContextGetColorSpace` | 10 | firefox, gimp, inkscape, krita |
| CoreGraphics.framework | `_CGContextClipToRects` | 9 | firefox, krita, monitorcontrol, mos |
| CoreGraphics.framework | `_CGContextDrawShading` | 8 | firefox, gimp, inkscape, krita |
| CoreGraphics.framework | `_CGImageCreateCopyWithColorSpace` | 8 | firefox, krita, libreoffice, musescore |
| CoreGraphics.framework | `_CGShadingRelease` | 8 | firefox, gimp, inkscape, krita |
| CoreGraphics.framework | `_CGContextSetTextPosition` | 7 | krita, monitorcontrol, mos, musescore |
| CoreGraphics.framework | `_kCGColorWhite` | 7 | krita, monitorcontrol, mos, musescore |
| CoreGraphics.framework | `_CGContextSetStrokeColorSpace` | 6 | firefox, gimp, inkscape, libreoffice |
| CoreGraphics.framework | `_CGContextSetStrokePattern` | 6 | firefox, gimp, inkscape, libreoffice |
| CoreGraphics.framework | `_CGDisplayMoveCursorToPoint` | 6 | blender, gimp, inkscape, krita |
| CoreGraphics.framework | `_CGFunctionCreate` | 6 | firefox, gimp, inkscape, libreoffice |
| CoreGraphics.framework | `_CGFunctionRelease` | 6 | firefox, gimp, inkscape, libreoffice |
| CoreGraphics.framework | `_CGShadingCreateAxial` | 6 | firefox, gimp, inkscape, libreoffice |
| CoreGraphics.framework | `_CGShadingCreateRadial` | 6 | firefox, gimp, inkscape, libreoffice |
| CoreGraphics.framework | `_CGColorSpaceCopyPropertyList` | 5 | krita, musescore, obs, qbittorrent |
| CoreGraphics.framework | `_CGDataConsumerCreate` | 5 | krita, musescore, obs, shotcut |
| CoreGraphics.framework | `_CGEventTapCreateForPid` | 5 | krita, musescore, obs, qbittorrent |
| CoreText.framework | `_kCTFontCopyrightNameKey` | 5 | krita, musescore, obs, qbittorrent |
| CoreText.framework | `_kCTFontLanguagesAttribute` | 5 | krita, musescore, obs, qbittorrent |
| CoreText.framework | `_kCTFontUniqueNameKey` | 5 | krita, musescore, obs, qbittorrent |
| ImageIO.framework | `_CGImageSourceCreateWithDataProvider` | 5 | krita, libreoffice, musescore, obs |
| ImageIO.framework | `_kCGImagePropertyDPIHeight` | 5 | iterm2, krita, musescore, obs |
| ImageIO.framework | `_kCGImagePropertyDPIWidth` | 5 | iterm2, krita, musescore, obs |
| ImageIO.framework | `_kCGImagePropertyOrientation` | 5 | krita, libreoffice, musescore, obs |
| CoreGraphics.framework | `_CGDataProviderCreateSequential` | 4 | krita, musescore, obs, shotcut |
| CoreGraphics.framework | `_CGFontGetUnitsPerEm` | 4 | firefox, iterm2, libreoffice, thunderbird |
| ImageIO.framework | `_CGImageDestinationCopyTypeIdentifiers` | 4 | krita, musescore, obs, shotcut |
| ImageIO.framework | `_CGImageDestinationCreateWithDataConsumer` | 4 | krita, musescore, obs, shotcut |
| CoreGraphics.framework | `_CGColorSpaceCreateWithICCData` | 3 | musescore, obs, qbittorrent |
| CoreGraphics.framework | `_CGContextAddLines` | 3 | libreoffice, monitorcontrol, mos |
| CoreGraphics.framework | `_CGContextAddRects` | 3 | libreoffice, monitorcontrol, mos |
| CoreGraphics.framework | `_CGContextShowGlyphsAtPoint` | 3 | firefox, shotcut, thunderbird |
| CoreGraphics.framework | `_CGContextShowGlyphsWithAdvances` | 3 | firefox, shotcut, thunderbird |
| CoreGraphics.framework | `_CGDisplayCopyAllDisplayModes` | 3 | iina, krita, shotcut |
| CoreGraphics.framework | `_CGDisplayIOServicePort` | 3 | blender, krita, shotcut |
| CoreGraphics.framework | `_CGDisplayModeGetIOFlags` | 3 | iina, krita, shotcut |
| CoreGraphics.framework | `_CGEventCreateCopy` | 3 | iina, iterm2, mos |
| CoreGraphics.framework | `_CGFontGetGlyphAdvances` | 3 | firefox, iterm2, thunderbird |
| CoreGraphics.framework | `_CGGetDisplayTransferByTable` | 3 | krita, monitorcontrol, shotcut |
| CoreGraphics.framework | `_CGImageIsMask` | 3 | krita, libreoffice, musescore |
| CoreGraphics.framework | `_CGSetDisplayTransferByTable` | 3 | krita, monitorcontrol, shotcut |
| CoreText.framework | `_CTFontCopyNameForGlyph` | 3 | musescore, obs, qbittorrent |
| CoreText.framework | `_kCTFontVariationAxisNameKey` | 3 | musescore, obs, qbittorrent |
| ColorSync.framework | `_ColorSyncDeviceCopyDeviceInfo` | 2 | blender, krita |
| ColorSync.framework | `_kColorSyncCustomProfiles` | 2 | blender, krita |
| ColorSync.framework | `_kColorSyncDeviceDefaultProfileID` | 2 | blender, krita |
| ColorSync.framework | `_kColorSyncDeviceProfileURL` | 2 | blender, krita |
| ColorSync.framework | `_kColorSyncDisplayDeviceClass` | 2 | blender, krita |
| ColorSync.framework | `_kColorSyncFactoryProfiles` | 2 | blender, krita |
| CoreGraphics.framework | `_CGAcquireDisplayFadeReservation` | 2 | krita, shotcut |
| CoreGraphics.framework | `_CGCaptureAllDisplays` | 2 | krita, shotcut |
| CoreGraphics.framework | `_CGColorSpaceGetColorTable` | 2 | monitorcontrol, mos |
| CoreGraphics.framework | `_CGColorSpaceGetColorTableCount` | 2 | monitorcontrol, mos |
| CoreGraphics.framework | `_CGContextAddArcToPoint` | 2 | monitorcontrol, mos |
| CoreGraphics.framework | `_CGContextGetTextPosition` | 2 | monitorcontrol, mos |
| CoreGraphics.framework | `_CGContextSetBaseCTM` | 2 | firefox, thunderbird |
| CoreGraphics.framework | `_CGContextSetCTM` | 2 | firefox, thunderbird |
| CoreGraphics.framework | `_CGContextSetFillColor` | 2 | iterm2, libreoffice |
| CoreGraphics.framework | `_CGDisplayCapture` | 2 | krita, shotcut |
| CoreGraphics.framework | `_CGDisplayFade` | 2 | krita, shotcut |
| CoreGraphics.framework | `_CGDisplayModeCopyPixelEncoding` | 2 | krita, shotcut |
| CoreGraphics.framework | `_CGDisplayModeIsUsableForDesktopGUI` | 2 | krita, shotcut |
| CoreGraphics.framework | `_CGDisplayRelease` | 2 | krita, shotcut |
| CoreGraphics.framework | `_CGDisplaySetDisplayMode` | 2 | krita, shotcut |
| CoreGraphics.framework | `_CGDisplayStreamCreateWithDispatchQueue` | 2 | alt-tab, obs |
| CoreGraphics.framework | `_CGEventSourceButtonState` | 2 | firefox, thunderbird |
| CoreGraphics.framework | `_CGEventSourceCounterForEventType` | 2 | firefox, thunderbird |
| CoreGraphics.framework | `_CGFontCopyTableTags` | 2 | firefox, thunderbird |
| CoreGraphics.framework | `_CGFontCopyVariationAxes` | 2 | firefox, thunderbird |
| CoreGraphics.framework | `_CGFontCopyVariations` | 2 | firefox, thunderbird |
| CoreGraphics.framework | `_CGFontCreateCopyWithVariations` | 2 | firefox, thunderbird |
| CoreGraphics.framework | `_CGFontGetCapHeight` | 2 | firefox, thunderbird |
| CoreGraphics.framework | `_CGFontGetGlyphBBoxes` | 2 | firefox, thunderbird |
| CoreGraphics.framework | `_CGFontGetXHeight` | 2 | firefox, thunderbird |
| CoreGraphics.framework | `_CGGetLastMouseDelta` | 2 | monitorcontrol, mos |
| CoreGraphics.framework | `_CGImageCreateWithMaskingColors` | 2 | monitorcontrol, mos |
| CoreGraphics.framework | `_CGPDFContextAddDestinationAtPoint` | 2 | firefox, thunderbird |
| CoreGraphics.framework | `_CGPDFContextSetDestinationForRect` | 2 | firefox, thunderbird |
| CoreGraphics.framework | `_CGPDFContextSetURLForRect` | 2 | firefox, thunderbird |
| CoreGraphics.framework | `_CGReleaseAllDisplays` | 2 | krita, shotcut |
| CoreGraphics.framework | `_CGReleaseDisplayFadeReservation` | 2 | krita, shotcut |
| CoreGraphics.framework | `_CGSShutdownServerConnections` | 2 | firefox, thunderbird |
| CoreGraphics.framework | `_kCGDisplayShowDuplicateLowResolutionModes` | 2 | krita, shotcut |
| CoreGraphics.framework | `_kCGFontVariationAxisName` | 2 | firefox, thunderbird |
| CoreText.framework | `_CTFontManagerRegisterFontURLs` | 2 | firefox, thunderbird |
| CoreText.framework | `_CTFontManagerUnregisterFontsForURL` | 2 | iterm2, libreoffice |
| CoreText.framework | `_kCTWritingDirectionAttributeName` | 2 | firefox, thunderbird |
| ImageIO.framework | `_kCGImageSourceShouldAllowFloat` | 2 | firefox, thunderbird |
| ImageIO.framework | `_kCGImageSourceTypeIdentifierHint` | 2 | firefox, thunderbird |
| libswiftCoreGraphics.dylib | `_$sSo7CGPointV12CoreGraphicsE16debugDescriptionSSvg` | 2 | alt-tab, rectangle |
| ColorSync.framework | `_ColorSyncIterateDeviceProfiles` | 1 | iina |
| ColorSync.framework | `_kColorSyncDisplayDeviceProfilesNotification` | 1 | monitorcontrol |
| CoreGraphics.framework | `_$sSo12CGContextRefa12CoreGraphicsE10showGlyphs_2atySays6UInt16VG_SaySo7CGPointVGtF` | 1 | coteditor |
| CoreGraphics.framework | `_$sSo16CGMutablePathRefa12CoreGraphicsE8addLines7between9transformySaySo7CGPointVG_So17CGAffineTransformVtF` | 1 | coteditor |
| CoreGraphics.framework | `_$sSo9CGPathRefa12CoreGraphicsE4copy17strokingWithWidth7lineCap0I4Join10miterLimit9transformAbC7CGFloatV_So06CGLineJ0VSo0pK0VAKSo17CGAffineTransformVtF` | 1 | coteditor |
| CoreGraphics.framework | `_CGBeginDisplayConfiguration` | 1 | monitorcontrol |
| CoreGraphics.framework | `_CGColorSpaceCreateLab` | 1 | iterm2 |
| CoreGraphics.framework | `_CGCompleteDisplayConfiguration` | 1 | monitorcontrol |
| CoreGraphics.framework | `_CGConfigureDisplayMirrorOfDisplay` | 1 | monitorcontrol |
| CoreGraphics.framework | `_CGContextGetFontSmoothingStyle` | 1 | iterm2 |
| CoreGraphics.framework | `_CGContextIsPathEmpty` | 1 | coteditor |
| CoreGraphics.framework | `_CGContextSelectFont` | 1 | iterm2 |
| CoreGraphics.framework | `_CGContextSetFontSmoothingStyle` | 1 | iterm2 |
| CoreGraphics.framework | `_CGContextSetStrokeColor` | 1 | libreoffice |
| CoreGraphics.framework | `_CGDataConsumerRelease` | 1 | thunderbird |
| CoreGraphics.framework | `_CGDisplayIsInHWMirrorSet` | 1 | monitorcontrol |
| CoreGraphics.framework | `_CGDisplayRestoreColorSyncSettings` | 1 | monitorcontrol |
| CoreGraphics.framework | `_CGDisplayStreamUpdateGetDropCount` | 1 | obs |
| CoreGraphics.framework | `_CGEventGetTypeID` | 1 | mos |
| CoreGraphics.framework | `_CGImageCreateWithJPEGDataProvider` | 1 | alt-tab |
| CoreGraphics.framework | `_CGImageCreateWithMask` | 1 | libreoffice |
| CoreGraphics.framework | `_CGImageGetByteOrderInfo` | 1 | shotcut |
| CoreGraphics.framework | `_CGImageGetPixelFormatInfo` | 1 | shotcut |
| CoreGraphics.framework | `_CGSCopyActiveMenuBarDisplayIdentifier` | 1 | alt-tab |
| CoreGraphics.framework | `_CGSCopyManagedDisplaySpaces` | 1 | alt-tab |
| CoreGraphics.framework | `_CGSCopySpacesForWindows` | 1 | alt-tab |
| CoreGraphics.framework | `_CGSCopyWindowProperty` | 1 | alt-tab |
| CoreGraphics.framework | `_CGSCopyWindowsWithOptionsAndTags` | 1 | alt-tab |
| CoreGraphics.framework | `_CGSDefaultConnectionForThread` | 1 | iterm2 |
| CoreGraphics.framework | `_CGSGetWindowLevel` | 1 | alt-tab |
| CoreGraphics.framework | `_CGSHWCaptureWindowList` | 1 | alt-tab |
| CoreGraphics.framework | `_CGSManagedDisplayGetCurrentSpace` | 1 | alt-tab |
| CoreGraphics.framework | `_CGSServiceForDisplayNumber` | 1 | monitorcontrol |
| CoreGraphics.framework | `_CGSSetSymbolicHotKeyEnabled` | 1 | alt-tab |
| CoreGraphics.framework | `_kCGColorSpaceDisplayP3_HLG` | 1 | iina |
| CoreGraphics.framework | `_kCGColorSpaceDisplayP3_PQ` | 1 | iina |
| CoreGraphics.framework | `_kCGDisplayStreamQueueDepth` | 1 | obs |
| CoreGraphics.framework | `_kCGDisplayStreamSourceRect` | 1 | obs |
| CoreGraphics.framework | `_kCGPDFContextCreator` | 1 | thunderbird |
| CoreText.framework | `_CTFontManagerRegisterFontDescriptors` | 1 | iterm2 |
| CoreText.framework | `_CTRunGetImageBounds` | 1 | alt-tab |
| CoreText.framework | `_CTRunGetTypeID` | 1 | musescore |
| CoreText.framework | `_kCTFontEnabledAttribute` | 1 | libreoffice |
| CoreText.framework | `_kCTFontMatrixAttribute` | 1 | iterm2 |
| CoreText.framework | `_kCTLigatureAttributeName` | 1 | iterm2 |
| ImageIO.framework | `_kCGImagePropertyHasAlpha` | 1 | libreoffice |
| ImageIO.framework | `_kCGImagePropertyPNGDictionary` | 1 | iterm2 |
| ImageIO.framework | `_kCGImagePropertyPNGInterlaceType` | 1 | iterm2 |
| libswiftCoreGraphics.dylib | `_$sSo10CGColorRefa12CoreGraphicsE5whiteABvgZ` | 1 | iterm2 |
| libswiftCoreGraphics.dylib | `_$sSo16CGMutablePathRefa12CoreGraphicsE8addLines7between9transformySaySo7CGPointVG_So17CGAffineTransformVtF` | 1 | iterm2 |
| libswiftCoreGraphics.dylib | `_$sSo6CGRectV12CoreGraphicsE16debugDescriptionSSvg` | 1 | rectangle |
| libswiftCoreGraphics.dylib | `_$sSo6CGSizeV12CoreGraphicsE16debugDescriptionSSvg` | 1 | rectangle |
