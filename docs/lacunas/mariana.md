# Lacunas de Mariana (CG, Core Text, ImageIO, PDFKit)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Mariana (CG, Core Text, ImageIO, PDFKit) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**166** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| CoreGraphics.framework | `_kCGColorWhite` | 24 | audacity, bitwarden, calibre, db-browser-for-sqlite |
| CoreText.framework | `_kCTFontLanguagesAttribute` | 21 | adobe-acrobat-reader, audacity, calibre, db-browser-for-sqlite |
| CoreText.framework | `_kCTFontUniqueNameKey` | 20 | audacity, calibre, db-browser-for-sqlite, dolphin |
| CoreText.framework | `_kCTFontCopyrightNameKey` | 19 | audacity, calibre, db-browser-for-sqlite, dolphin |
| CoreText.framework | `_kCTFontVariationAxisNameKey` | 14 | audacity, calibre, cmux, figma |
| CoreText.framework | `_CTFontCopyNameForGlyph` | 9 | audacity, calibre, keepassxc, moonlight |
| CoreText.framework | `_CTFontManagerRegisterFontURLs` | 9 | 1password, firefox, floorp, librewolf |
| CoreText.framework | `_kCTWritingDirectionAttributeName` | 9 | firefox, floorp, librewolf, mullvad-browser |
| CoreGraphics.framework | `_CGContextSetBaseCTM` | 8 | firefox, floorp, librewolf, mullvad-browser |
| CoreGraphics.framework | `_CGContextSetCTM` | 8 | firefox, floorp, librewolf, mullvad-browser |
| CoreGraphics.framework | `_CGPDFContextAddDestinationAtPoint` | 8 | firefox, floorp, librewolf, mullvad-browser |
| CoreGraphics.framework | `_CGPDFContextSetDestinationForRect` | 8 | firefox, floorp, librewolf, mullvad-browser |
| CoreGraphics.framework | `_CGPDFContextSetURLForRect` | 8 | firefox, floorp, librewolf, mullvad-browser |
| PDFKit.framework | `_OBJC_CLASS_$_PDFDocument` | 8 | adobe-acrobat-reader, cmux, kicad, markedit |
| CoreGraphics.framework | `_kCGPDFContextCreator` | 6 | floorp, mullvad-browser, thunderbird, tor-browser |
| CoreText.framework | `_kCTFontVersionNameKey` | 6 | android-studio, intellij-idea, jetbrains-toolbox, openlogi |
| libswiftCoreGraphics.dylib | `_$sSo10CGColorRefa12CoreGraphicsE5whiteABvgZ` | 6 | iterm2, jordanbaird-ice, protonvpn, raycast |
| libswiftCoreGraphics.dylib | `_$sSo16CGMutablePathRefa12CoreGraphicsE10addEllipse2in9transformySo6CGRectV_So17CGAffineTransformVtF` | 6 | clipy, protonvpn, shottr, utm |
| CoreGraphics.framework | `_CGEnableEventStateCombining` | 5 | android-studio, dbeaver-community, intellij-idea, jetbrains-toolbox |
| CoreText.framework | `_CTFontCopyDisplayName` | 5 | cmux, ghostty, kitty, markedit |
| CoreText.framework | `_CTFontDescriptorCreateCopyWithVariation` | 5 | cmux, ghostty, orbstack, rustdesk |
| CoreText.framework | `_CTRunGetAdvancesPtr` | 5 | cmux, ghostty, orbstack, sublime-text |
| CoreText.framework | `_CTRunGetTypeID` | 5 | adobe-acrobat-reader, audacity, calibre, musescore |
| ColorSync.framework | `_kColorSyncDeviceProfileURL` | 4 | adobe-acrobat-reader, betterdisplay, blender, krita |
| CoreText.framework | `_CTFontGetPlatformFont` | 4 | adobe-acrobat-reader, openmtp, pencil, webtorrent |
| ColorSync.framework | `_ColorSyncDeviceCopyDeviceInfo` | 3 | betterdisplay, blender, krita |
| ColorSync.framework | `_ColorSyncIterateDeviceProfiles` | 3 | adobe-acrobat-reader, betterdisplay, iina |
| ColorSync.framework | `_kColorSyncCustomProfiles` | 3 | betterdisplay, blender, krita |
| ColorSync.framework | `_kColorSyncFactoryProfiles` | 3 | betterdisplay, blender, krita |
| CoreGraphics.framework | `_CGColorSpaceCopyICCProfile` | 3 | openmtp, pencil, webtorrent |
| CoreText.framework | `_CTFontDescriptorCreateCopyWithSymbolicTraits` | 3 | kicad, kitty, prusaslicer |
| CoreText.framework | `_CTFontGetStringEncoding` | 3 | adobe-acrobat-reader, kicad, prusaslicer |
| CoreText.framework | `_CTParagraphStyleCreate` | 3 | rustdesk, utm, warp |
| CoreText.framework | `_CTRunDelegateCreate` | 3 | openttd, telegram, zoom |
| CoreText.framework | `_kCTFontSubFamilyNameKey` | 3 | openlogi, warp, zed |
| CoreText.framework | `_kCTLigatureAttributeName` | 3 | iterm2, microsoft-edge, utm |
| CoreText.framework | `_kCTParagraphStyleAttributeName` | 3 | rustdesk, utm, warp |
| CoreText.framework | `_kCTRunDelegateAttributeName` | 3 | openttd, telegram, zoom |
| libswiftCoreGraphics.dylib | `_$sSo16CGMutablePathRefa12CoreGraphicsE14addRoundedRect2in11cornerWidth0J6Height9transformySo6CGRectV_AC7CGFloatVALSo17CGAffineTransformVtF` | 3 | raycast, shottr, vorssaint |
| libswiftCoreGraphics.dylib | `_$sSo16CGMutablePathRefa12CoreGraphicsE8addLines7between9transformySaySo7CGPointVG_So17CGAffineTransformVtF` | 3 | cmux, iterm2, vorssaint |
| libswiftCoreGraphics.dylib | `_$sSo7CGPointV12CoreGraphicsE16debugDescriptionSSvg` | 3 | alt-tab, raycast, rectangle |
| ColorSync.framework | `_kColorSyncDeviceClass` | 2 | adobe-acrobat-reader, betterdisplay |
| ColorSync.framework | `_kColorSyncDeviceDescription` | 2 | adobe-acrobat-reader, betterdisplay |
| ColorSync.framework | `_kColorSyncDisplayDeviceProfilesNotification` | 2 | betterdisplay, monitorcontrol |
| ColorSync.framework | `_kColorSyncProfileURL` | 2 | adobe-acrobat-reader, betterdisplay |
| CoreGraphics.framework | `_CGPDFContextBeginPage` | 2 | kicad, vorssaint |
| CoreGraphics.framework | `_CGPDFContextEndPage` | 2 | kicad, vorssaint |
| CoreGraphics.framework | `_CGPreflightPostEventAccess` | 2 | openclaw, vorssaint |
| CoreGraphics.framework | `_kCGWindowWorkspace` | 2 | hammerspoon, zoom |
| CoreText.framework | `_CTFontGetOpticalBoundsForGlyphs` | 2 | openttd, zoom |
| CoreText.framework | `_CTFontManagerCopyAvailableFontURLs` | 2 | adobe-acrobat-reader, figma |
| CoreText.framework | `_CTFontManagerUnregisterFontsForURL` | 2 | iterm2, libreoffice |
| CoreText.framework | `_CTFontManagerUnregisterGraphicsFont` | 2 | openclaw, telegram |
| CoreText.framework | `_CTLineGetStringIndexForPosition` | 2 | telegram, zoom |
| CoreText.framework | `_kCTFontDescriptionNameKey` | 2 | warp, zed |
| CoreText.framework | `_kCTFontDesignerNameKey` | 2 | warp, zed |
| CoreText.framework | `_kCTFontDesignerURLNameKey` | 2 | warp, zed |
| CoreText.framework | `_kCTFontEnabledAttribute` | 2 | alacritty, libreoffice |
| CoreText.framework | `_kCTFontLicenseNameKey` | 2 | warp, zed |
| CoreText.framework | `_kCTFontLicenseURLNameKey` | 2 | warp, zed |
| CoreText.framework | `_kCTFontManufacturerNameKey` | 2 | warp, zed |
| CoreText.framework | `_kCTFontPostScriptCIDNameKey` | 2 | warp, zed |
| CoreText.framework | `_kCTFontSampleTextNameKey` | 2 | warp, zed |
| CoreText.framework | `_kCTFontTrademarkNameKey` | 2 | warp, zed |
| CoreText.framework | `_kCTFontVendorURLNameKey` | 2 | warp, zed |
| CoreText.framework | `_kCTStrokeColorAttributeName` | 2 | kicad, prusaslicer |
| CoreText.framework | `_kCTStrokeWidthAttributeName` | 2 | kicad, prusaslicer |
| CoreText.framework | `_kCTUnderlineStyleAttributeName` | 2 | rustdesk, zoom |
| ImageIO.framework | `_CGImageSourceCopyMetadataAtIndex` | 2 | adobe-acrobat-reader, whatsapp |
| PDFKit.framework | `_OBJC_CLASS_$_PDFPage` | 2 | adobe-acrobat-reader, whatsapp |
| PDFKit.framework | `_OBJC_CLASS_$_PDFView` | 2 | cmux, protonvpn |
| libswiftCoreGraphics.dylib | `_$sSo12CGContextRefa12CoreGraphicsE4clip2toySaySo6CGRectVG_tF` | 2 | utm, vorssaint |
| libswiftCoreGraphics.dylib | `_$sSo6CGRectV12CoreGraphicsE16debugDescriptionSSvg` | 2 | raycast, rectangle |
| libswiftCoreGraphics.dylib | `_$sSo6CGSizeV12CoreGraphicsE16debugDescriptionSSvg` | 2 | raycast, rectangle |
| libswiftCoreGraphics.dylib | `_$sSo9CGPathRefa12CoreGraphicsE5union_5usingA2B_AC0A8FillRuleOtF` | 2 | jordanbaird-ice, vorssaint |
| ColorSync.framework | `_ColorSyncCMMCopyLocalizedName` | 1 | adobe-acrobat-reader |
| ColorSync.framework | `_ColorSyncCMMGetBundle` | 1 | adobe-acrobat-reader |
| ColorSync.framework | `_ColorSyncDeviceSetCustomProfiles` | 1 | betterdisplay |
| ColorSync.framework | `_ColorSyncIterateInstalledCMMs` | 1 | adobe-acrobat-reader |
| ColorSync.framework | `_ColorSyncIterateInstalledProfiles` | 1 | adobe-acrobat-reader |
| ColorSync.framework | `_ColorSyncIterateInstalledProfilesWithOptions` | 1 | betterdisplay |
| ColorSync.framework | `_kColorSyncDeviceID` | 1 | betterdisplay |
| ColorSync.framework | `_kColorSyncDeviceProfileIsCurrent` | 1 | adobe-acrobat-reader |
| ColorSync.framework | `_kColorSyncDeviceProfileIsDefault` | 1 | adobe-acrobat-reader |
| ColorSync.framework | `_kColorSyncDeviceProfileIsFactory` | 1 | betterdisplay |
| ColorSync.framework | `_kColorSyncProfileClass` | 1 | betterdisplay |
| ColorSync.framework | `_kColorSyncProfileColorSpace` | 1 | betterdisplay |
| ColorSync.framework | `_kColorSyncProfileDescription` | 1 | betterdisplay |
| ColorSync.framework | `_kColorSyncProfileRepositoryChangeNotification` | 1 | betterdisplay |
| ColorSync.framework | `_kColorSyncWaitForCacheReply` | 1 | betterdisplay |
| CoreGraphics.framework | `_$sSo12CGContextRefa12CoreGraphicsE10showGlyphs_2atySays6UInt16VG_SaySo7CGPointVGtF` | 1 | coteditor |
| CoreGraphics.framework | `_$sSo16CGMutablePathRefa12CoreGraphicsE8addLines7between9transformySaySo7CGPointVG_So17CGAffineTransformVtF` | 1 | coteditor |
| CoreGraphics.framework | `_$sSo6CGRectV12CoreGraphicsE16debugDescriptionSSvg` | 1 | thaw |
| CoreGraphics.framework | `_$sSo9CGPathRefa12CoreGraphicsE4copy17strokingWithWidth7lineCap0I4Join10miterLimit9transformAbC7CGFloatV_So06CGLineJ0VSo0pK0VAKSo17CGAffineTransformVtF` | 1 | coteditor |
| CoreGraphics.framework | `_$sSo9CGPathRefa12CoreGraphicsE5union_5usingA2B_AC0A8FillRuleOtF` | 1 | thaw |
| CoreGraphics.framework | `_CGColorCreateWithPattern` | 1 | vorssaint |
| CoreGraphics.framework | `_CGContextGetFontSmoothingStyle` | 1 | iterm2 |
| CoreGraphics.framework | `_CGContextSetFontSmoothingStyle` | 1 | iterm2 |
| CoreGraphics.framework | `_CGCursorIsVisible` | 1 | zoom |
| CoreGraphics.framework | `_CGGetEventTapList` | 1 | openlogi |
| CoreGraphics.framework | `_CGImageGetTypeID` | 1 | vorssaint |
| CoreGraphics.framework | `_CGOpenGLDisplayMaskToDisplayID` | 1 | kitty |
| CoreGraphics.framework | `_CGPDFContextCreateWithURL` | 1 | vorssaint |
| CoreGraphics.framework | `_CGRequestPostEventAccess` | 1 | openclaw |
| CoreGraphics.framework | `_CGUnregisterScreenRefreshCallback` | 1 | zoom |
| CoreGraphics.framework | `_CPSEnableForegroundOperation` | 1 | deluge |
| CoreGraphics.framework | `_CPSGetCurrentProcess` | 1 | deluge |
| CoreGraphics.framework | `_CPSSetFrontProcess` | 1 | deluge |
| CoreGraphics.framework | `_CPSSetProcessName` | 1 | deluge |
| CoreGraphics.framework | `_OBJC_CLASS_$_CGVirtualDisplay` | 1 | betterdisplay |
| CoreGraphics.framework | `_OBJC_CLASS_$_CGVirtualDisplayDescriptor` | 1 | betterdisplay |
| CoreGraphics.framework | `_OBJC_CLASS_$_CGVirtualDisplayMode` | 1 | betterdisplay |
| CoreGraphics.framework | `_OBJC_CLASS_$_CGVirtualDisplaySettings` | 1 | betterdisplay |
| CoreGraphics.framework | `_kCGSPackagesMainDisplayIdentifier` | 1 | openclaw |
| CoreGraphics.framework | `_kCGWindowBackingLocationVideoMemory` | 1 | jordanbaird-ice |
| CoreGraphics.framework | `_kCGWindowMemoryUsage` | 1 | jordanbaird-ice |
| CoreGraphics.framework | `_kCGWindowStoreType` | 1 | jordanbaird-ice |
| CoreText.framework | `_CTFontManagerCompareFontFamilyNames` | 1 | emacs-app |
| CoreText.framework | `_CTFontManagerRegisterFontDescriptors` | 1 | iterm2 |
| CoreText.framework | `_CTFramesetterGetTypesetter` | 1 | raycast |
| CoreText.framework | `_CTLineEnumerateCaretOffsets` | 1 | warp |
| CoreText.framework | `_CTLineGetPenOffsetForFlush` | 1 | telegram |
| CoreText.framework | `_CTRunGetImageBounds` | 1 | alt-tab |
| CoreText.framework | `_CTTypesetterCreateLineWithOffset` | 1 | telegram |
| CoreText.framework | `_kCTFontFixedAdvanceAttribute` | 1 | microsoft-edge |
| CoreText.framework | `_kCTFontMatrixAttribute` | 1 | iterm2 |
| CoreText.framework | `_kCTFontOpticalSizeAttribute` | 1 | unity-hub |
| ImageIO.framework | `_CGImageDestinationCopyTypeIdentifiers` | 1 | vorssaint |
| ImageIO.framework | `_CGImageDestinationSetProperties` | 1 | vorssaint |
| ImageIO.framework | `_CGImageMetadataCopyStringValueWithPath` | 1 | whatsapp |
| ImageIO.framework | `_CGImageMetadataCopyTagWithPath` | 1 | whatsapp |
| ImageIO.framework | `_CGImageMetadataCreateXMPData` | 1 | adobe-acrobat-reader |
| ImageIO.framework | `_CGImageMetadataGetTypeID` | 1 | whatsapp |
| ImageIO.framework | `_CGImageMetadataTagCopyValue` | 1 | whatsapp |
| ImageIO.framework | `_kCGImagePropertyDPIHeight` | 1 | vorssaint |
| ImageIO.framework | `_kCGImagePropertyDPIWidth` | 1 | vorssaint |
| ImageIO.framework | `_kCGImagePropertyExifLensSerialNumber` | 1 | gemini |
| ImageIO.framework | `_kCGImagePropertyExifUserComment` | 1 | shottr |
| ImageIO.framework | `_kCGImagePropertyGIFDelayTime` | 1 | vorssaint |
| ImageIO.framework | `_kCGImagePropertyGIFDictionary` | 1 | vorssaint |
| ImageIO.framework | `_kCGImagePropertyGIFLoopCount` | 1 | vorssaint |
| ImageIO.framework | `_kCGImagePropertyGPSAltitude` | 1 | gemini |
| ImageIO.framework | `_kCGImagePropertyGPSDateStamp` | 1 | gemini |
| ImageIO.framework | `_kCGImagePropertyGPSImgDirection` | 1 | gemini |
| ImageIO.framework | `_kCGImagePropertyGPSLatitude` | 1 | gemini |
| ImageIO.framework | `_kCGImagePropertyGPSLongitude` | 1 | gemini |
| ImageIO.framework | `_kCGImagePropertyGPSSpeed` | 1 | gemini |
| ImageIO.framework | `_kCGImagePropertyGPSTimeStamp` | 1 | gemini |
| ImageIO.framework | `_kCGImagePropertyOrientation` | 1 | vorssaint |
| ImageIO.framework | `_kCGImagePropertyTIFFTileLength` | 1 | adobe-acrobat-reader |
| ImageIO.framework | `_kCGImagePropertyTIFFTileWidth` | 1 | adobe-acrobat-reader |
| PDFKit.framework | `_OBJC_CLASS_$_PDFOutline` | 1 | cmux |
| PDFKit.framework | `_OBJC_METACLASS_$_PDFView` | 1 | cmux |
| PDFKit.framework | `_PDFViewPageChangedNotification` | 1 | cmux |
| Quartz.framework | `_IKPictureTakerOutputImageMaxSizeKey` | 1 | telegram |
| Quartz.framework | `_IKPictureTakerShowEffectsKey` | 1 | telegram |
| Quartz.framework | `_OBJC_CLASS_$_IKPictureTaker` | 1 | telegram |
| Quartz.framework | `_OBJC_CLASS_$_PDFDocument` | 1 | alfred |
| Quartz.framework | `_OBJC_CLASS_$_PDFView` | 1 | alfred |
| Quartz.framework | `_QLThumbnailCopyImage` | 1 | webtorrent |
| Quartz.framework | `_QLThumbnailCreate` | 1 | webtorrent |
| Quartz.framework | `_QLThumbnailDispatchAsync` | 1 | webtorrent |
| libswiftCoreGraphics.dylib | `_$sSo12CGContextRefa12CoreGraphicsE12addQuadCurve2to7controlySo7CGPointV_AHtF` | 1 | vorssaint |
| libswiftCoreGraphics.dylib | `_$sSo12CGContextRefa12CoreGraphicsE18strokeLineSegments7betweenySaySo7CGPointVG_tF` | 1 | tableplus |
| libswiftCoreGraphics.dylib | `_$sSo12CGContextRefa12CoreGraphicsE4draw_2inySo07CGLayerB0a_So6CGRectVtF` | 1 | tableplus |
| libswiftCoreGraphics.dylib | `_$sSo16CGMutablePathRefa12CoreGraphicsE14addRelativeArc6center6radius10startAngle5delta9transformySo7CGPointV_AC7CGFloatVA2MSo17CGAffineTransformVtF` | 1 | tableplus |
