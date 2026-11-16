# Lacunas de Mariana (CG, Core Text, ImageIO, PDFKit)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Mariana (CG, Core Text, ImageIO, PDFKit) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**115** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| CoreGraphics.framework | `_kCGColorWhite` | 16 | audacity, bitwarden, db-browser-for-sqlite, dolphin |
| CoreText.framework | `_kCTFontCopyrightNameKey` | 14 | audacity, db-browser-for-sqlite, dolphin, keepassxc |
| CoreText.framework | `_kCTFontLanguagesAttribute` | 14 | audacity, db-browser-for-sqlite, dolphin, keepassxc |
| CoreText.framework | `_kCTFontUniqueNameKey` | 14 | audacity, db-browser-for-sqlite, dolphin, keepassxc |
| CoreText.framework | `_kCTFontVariationAxisNameKey` | 10 | audacity, cmux, ghostty, keepassxc |
| CoreText.framework | `_CTFontManagerRegisterFontURLs` | 7 | 1password, firefox, floorp, mullvad-browser |
| CoreText.framework | `_kCTWritingDirectionAttributeName` | 7 | firefox, floorp, mullvad-browser, thunderbird |
| CoreGraphics.framework | `_CGContextSetBaseCTM` | 6 | firefox, floorp, mullvad-browser, thunderbird |
| CoreGraphics.framework | `_CGContextSetCTM` | 6 | firefox, floorp, mullvad-browser, thunderbird |
| CoreGraphics.framework | `_CGPDFContextAddDestinationAtPoint` | 6 | firefox, floorp, mullvad-browser, thunderbird |
| CoreGraphics.framework | `_CGPDFContextSetDestinationForRect` | 6 | firefox, floorp, mullvad-browser, thunderbird |
| CoreGraphics.framework | `_CGPDFContextSetURLForRect` | 6 | firefox, floorp, mullvad-browser, thunderbird |
| CoreText.framework | `_CTFontCopyNameForGlyph` | 6 | audacity, keepassxc, musescore, obs |
| CoreGraphics.framework | `_kCGPDFContextCreator` | 5 | floorp, mullvad-browser, thunderbird, tor-browser |
| CoreText.framework | `_CTFontCopyDisplayName` | 4 | cmux, ghostty, kitty, orbstack |
| CoreText.framework | `_CTRunGetAdvancesPtr` | 4 | cmux, ghostty, orbstack, sublime-text |
| PDFKit.framework | `_OBJC_CLASS_$_PDFDocument` | 4 | cmux, kicad, raycast, whatsapp |
| libswiftCoreGraphics.dylib | `_$sSo10CGColorRefa12CoreGraphicsE5whiteABvgZ` | 4 | iterm2, jordanbaird-ice, raycast, zoom |
| ColorSync.framework | `_ColorSyncDeviceCopyDeviceInfo` | 3 | betterdisplay, blender, krita |
| ColorSync.framework | `_kColorSyncCustomProfiles` | 3 | betterdisplay, blender, krita |
| ColorSync.framework | `_kColorSyncDeviceProfileURL` | 3 | betterdisplay, blender, krita |
| ColorSync.framework | `_kColorSyncFactoryProfiles` | 3 | betterdisplay, blender, krita |
| CoreText.framework | `_CTFontDescriptorCreateCopyWithSymbolicTraits` | 3 | kicad, kitty, prusaslicer |
| CoreText.framework | `_CTFontDescriptorCreateCopyWithVariation` | 3 | cmux, ghostty, orbstack |
| CoreText.framework | `_CTRunGetTypeID` | 3 | audacity, musescore, vnote |
| CoreText.framework | `_kCTLigatureAttributeName` | 3 | iterm2, microsoft-edge, utm |
| libswiftCoreGraphics.dylib | `_$sSo16CGMutablePathRefa12CoreGraphicsE10addEllipse2in9transformySo6CGRectV_So17CGAffineTransformVtF` | 3 | clipy, utm, zoom |
| libswiftCoreGraphics.dylib | `_$sSo7CGPointV12CoreGraphicsE16debugDescriptionSSvg` | 3 | alt-tab, raycast, rectangle |
| ColorSync.framework | `_ColorSyncIterateDeviceProfiles` | 2 | betterdisplay, iina |
| ColorSync.framework | `_kColorSyncDisplayDeviceProfilesNotification` | 2 | betterdisplay, monitorcontrol |
| CoreGraphics.framework | `_CGColorSpaceCopyICCProfile` | 2 | pencil, webtorrent |
| CoreGraphics.framework | `_CGEnableEventStateCombining` | 2 | android-studio, dbeaver-community |
| CoreGraphics.framework | `_kCGWindowWorkspace` | 2 | hammerspoon, zoom |
| CoreText.framework | `_CTFontGetOpticalBoundsForGlyphs` | 2 | openttd, zoom |
| CoreText.framework | `_CTFontGetPlatformFont` | 2 | pencil, webtorrent |
| CoreText.framework | `_CTFontGetStringEncoding` | 2 | kicad, prusaslicer |
| CoreText.framework | `_CTFontManagerUnregisterFontsForURL` | 2 | iterm2, libreoffice |
| CoreText.framework | `_CTRunDelegateCreate` | 2 | openttd, zoom |
| CoreText.framework | `_kCTFontEnabledAttribute` | 2 | alacritty, libreoffice |
| CoreText.framework | `_kCTFontVersionNameKey` | 2 | android-studio, zed |
| CoreText.framework | `_kCTRunDelegateAttributeName` | 2 | openttd, zoom |
| CoreText.framework | `_kCTStrokeColorAttributeName` | 2 | kicad, prusaslicer |
| CoreText.framework | `_kCTStrokeWidthAttributeName` | 2 | kicad, prusaslicer |
| libswiftCoreGraphics.dylib | `_$sSo16CGMutablePathRefa12CoreGraphicsE8addLines7between9transformySaySo7CGPointVG_So17CGAffineTransformVtF` | 2 | cmux, iterm2 |
| libswiftCoreGraphics.dylib | `_$sSo6CGRectV12CoreGraphicsE16debugDescriptionSSvg` | 2 | raycast, rectangle |
| libswiftCoreGraphics.dylib | `_$sSo6CGSizeV12CoreGraphicsE16debugDescriptionSSvg` | 2 | raycast, rectangle |
| ColorSync.framework | `_ColorSyncDeviceSetCustomProfiles` | 1 | betterdisplay |
| ColorSync.framework | `_ColorSyncIterateInstalledProfilesWithOptions` | 1 | betterdisplay |
| ColorSync.framework | `_kColorSyncDeviceClass` | 1 | betterdisplay |
| ColorSync.framework | `_kColorSyncDeviceDescription` | 1 | betterdisplay |
| ColorSync.framework | `_kColorSyncDeviceID` | 1 | betterdisplay |
| ColorSync.framework | `_kColorSyncDeviceProfileIsFactory` | 1 | betterdisplay |
| ColorSync.framework | `_kColorSyncProfileClass` | 1 | betterdisplay |
| ColorSync.framework | `_kColorSyncProfileColorSpace` | 1 | betterdisplay |
| ColorSync.framework | `_kColorSyncProfileDescription` | 1 | betterdisplay |
| ColorSync.framework | `_kColorSyncProfileRepositoryChangeNotification` | 1 | betterdisplay |
| ColorSync.framework | `_kColorSyncProfileURL` | 1 | betterdisplay |
| ColorSync.framework | `_kColorSyncWaitForCacheReply` | 1 | betterdisplay |
| CoreGraphics.framework | `_$sSo12CGContextRefa12CoreGraphicsE10showGlyphs_2atySays6UInt16VG_SaySo7CGPointVGtF` | 1 | coteditor |
| CoreGraphics.framework | `_$sSo16CGMutablePathRefa12CoreGraphicsE8addLines7between9transformySaySo7CGPointVG_So17CGAffineTransformVtF` | 1 | coteditor |
| CoreGraphics.framework | `_$sSo9CGPathRefa12CoreGraphicsE4copy17strokingWithWidth7lineCap0I4Join10miterLimit9transformAbC7CGFloatV_So06CGLineJ0VSo0pK0VAKSo17CGAffineTransformVtF` | 1 | coteditor |
| CoreGraphics.framework | `_CGContextGetFontSmoothingStyle` | 1 | iterm2 |
| CoreGraphics.framework | `_CGContextSetFontSmoothingStyle` | 1 | iterm2 |
| CoreGraphics.framework | `_CGCursorIsVisible` | 1 | zoom |
| CoreGraphics.framework | `_CGOpenGLDisplayMaskToDisplayID` | 1 | kitty |
| CoreGraphics.framework | `_CGPDFContextBeginPage` | 1 | kicad |
| CoreGraphics.framework | `_CGPDFContextEndPage` | 1 | kicad |
| CoreGraphics.framework | `_CGUnregisterScreenRefreshCallback` | 1 | zoom |
| CoreGraphics.framework | `_CPSEnableForegroundOperation` | 1 | deluge |
| CoreGraphics.framework | `_CPSGetCurrentProcess` | 1 | deluge |
| CoreGraphics.framework | `_CPSSetFrontProcess` | 1 | deluge |
| CoreGraphics.framework | `_CPSSetProcessName` | 1 | deluge |
| CoreGraphics.framework | `_OBJC_CLASS_$_CGVirtualDisplay` | 1 | betterdisplay |
| CoreGraphics.framework | `_OBJC_CLASS_$_CGVirtualDisplayDescriptor` | 1 | betterdisplay |
| CoreGraphics.framework | `_OBJC_CLASS_$_CGVirtualDisplayMode` | 1 | betterdisplay |
| CoreGraphics.framework | `_OBJC_CLASS_$_CGVirtualDisplaySettings` | 1 | betterdisplay |
| CoreGraphics.framework | `_kCGWindowBackingLocationVideoMemory` | 1 | jordanbaird-ice |
| CoreGraphics.framework | `_kCGWindowMemoryUsage` | 1 | jordanbaird-ice |
| CoreGraphics.framework | `_kCGWindowStoreType` | 1 | jordanbaird-ice |
| CoreText.framework | `_CTFontManagerRegisterFontDescriptors` | 1 | iterm2 |
| CoreText.framework | `_CTFramesetterGetTypesetter` | 1 | raycast |
| CoreText.framework | `_CTLineGetStringIndexForPosition` | 1 | zoom |
| CoreText.framework | `_CTParagraphStyleCreate` | 1 | utm |
| CoreText.framework | `_CTRunGetImageBounds` | 1 | alt-tab |
| CoreText.framework | `_kCTFontDescriptionNameKey` | 1 | zed |
| CoreText.framework | `_kCTFontDesignerNameKey` | 1 | zed |
| CoreText.framework | `_kCTFontDesignerURLNameKey` | 1 | zed |
| CoreText.framework | `_kCTFontFixedAdvanceAttribute` | 1 | microsoft-edge |
| CoreText.framework | `_kCTFontLicenseNameKey` | 1 | zed |
| CoreText.framework | `_kCTFontLicenseURLNameKey` | 1 | zed |
| CoreText.framework | `_kCTFontManufacturerNameKey` | 1 | zed |
| CoreText.framework | `_kCTFontMatrixAttribute` | 1 | iterm2 |
| CoreText.framework | `_kCTFontPostScriptCIDNameKey` | 1 | zed |
| CoreText.framework | `_kCTFontSampleTextNameKey` | 1 | zed |
| CoreText.framework | `_kCTFontSubFamilyNameKey` | 1 | zed |
| CoreText.framework | `_kCTFontTrademarkNameKey` | 1 | zed |
| CoreText.framework | `_kCTFontVendorURLNameKey` | 1 | zed |
| CoreText.framework | `_kCTParagraphStyleAttributeName` | 1 | utm |
| CoreText.framework | `_kCTUnderlineStyleAttributeName` | 1 | zoom |
| ImageIO.framework | `_CGImageMetadataCopyStringValueWithPath` | 1 | whatsapp |
| ImageIO.framework | `_CGImageMetadataCopyTagWithPath` | 1 | whatsapp |
| ImageIO.framework | `_CGImageMetadataGetTypeID` | 1 | whatsapp |
| ImageIO.framework | `_CGImageMetadataTagCopyValue` | 1 | whatsapp |
| ImageIO.framework | `_CGImageSourceCopyMetadataAtIndex` | 1 | whatsapp |
| PDFKit.framework | `_OBJC_CLASS_$_PDFOutline` | 1 | cmux |
| PDFKit.framework | `_OBJC_CLASS_$_PDFPage` | 1 | whatsapp |
| PDFKit.framework | `_OBJC_CLASS_$_PDFView` | 1 | cmux |
| PDFKit.framework | `_OBJC_METACLASS_$_PDFView` | 1 | cmux |
| PDFKit.framework | `_PDFViewPageChangedNotification` | 1 | cmux |
| Quartz.framework | `_QLThumbnailCopyImage` | 1 | webtorrent |
| Quartz.framework | `_QLThumbnailCreate` | 1 | webtorrent |
| Quartz.framework | `_QLThumbnailDispatchAsync` | 1 | webtorrent |
| libswiftCoreGraphics.dylib | `_$sSo12CGContextRefa12CoreGraphicsE4clip2toySaySo6CGRectVG_tF` | 1 | utm |
| libswiftCoreGraphics.dylib | `_$sSo16CGMutablePathRefa12CoreGraphicsE14addRoundedRect2in11cornerWidth0J6Height9transformySo6CGRectV_AC7CGFloatVALSo17CGAffineTransformVtF` | 1 | raycast |
| libswiftCoreGraphics.dylib | `_$sSo9CGPathRefa12CoreGraphicsE5union_5usingA2B_AC0A8FillRuleOtF` | 1 | jordanbaird-ice |
