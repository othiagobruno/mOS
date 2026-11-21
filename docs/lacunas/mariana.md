# Lacunas de Mariana (CG, Core Text, ImageIO, PDFKit)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Mariana (CG, Core Text, ImageIO, PDFKit) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**242** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| libswiftCoreGraphics.dylib | `_$sSo10CGColorRefa12CoreGraphicsE5whiteABvgZ` | 16 | cleanshot, duckduckgo, easydict, iterm2 |
| libswiftCoreGraphics.dylib | `_$sSo16CGMutablePathRefa12CoreGraphicsE10addEllipse2in9transformySo6CGRectV_So17CGAffineTransformVtF` | 11 | bettertouchtool, cleanshot, clipy, easydict |
| CoreGraphics.framework | `_CGEnableEventStateCombining` | 9 | android-studio, bettertouchtool, cryptomator, datagrip |
| CoreGraphics.framework | `_CGPreflightPostEventAccess` | 7 | feishu, macwhisper, openclaw, superwhisper |
| libswiftCoreGraphics.dylib | `_$sSo16CGMutablePathRefa12CoreGraphicsE14addRoundedRect2in11cornerWidth0J6Height9transformySo6CGRectV_AC7CGFloatVALSo17CGAffineTransformVtF` | 7 | cleanshot, microsoft-outlook, microsoft-powerpoint, raycast |
| libswiftCoreGraphics.dylib | `_$sSo16CGMutablePathRefa12CoreGraphicsE8addLines7between9transformySaySo7CGPointVG_So17CGAffineTransformVtF` | 6 | bettertouchtool, cleanshot, cmux, iterm2 |
| libswiftCoreGraphics.dylib | `_$sSo6CGRectV12CoreGraphicsE16debugDescriptionSSvg` | 6 | bartender, loop, macwhisper, microsoft-outlook |
| libswiftCoreGraphics.dylib | `_$sSo7CGPointV12CoreGraphicsE16debugDescriptionSSvg` | 6 | alt-tab, bartender, loop, raycast |
| CoreGraphics.framework | `_CGColorSpaceCopyICCProfile` | 5 | another-redis-desktop-manager, feishu, openmtp, pencil |
| CoreGraphics.framework | `_CGGetEventTapList` | 5 | bartender, bettertouchtool, macwhisper, openlogi |
| ImageIO.framework | `_kCGImagePropertyExifUserComment` | 5 | bbedit, capcut, cleanshot, feishu |
| libswiftCoreGraphics.dylib | `_$sSo9CGPathRefa12CoreGraphicsE4copy17strokingWithWidth7lineCap0I4Join10miterLimit9transformAbC7CGFloatV_So06CGLineJ0VSo0pK0VAKSo17CGAffineTransformVtF` | 5 | bartender, bettertouchtool, cleanshot, proxyman |
| CoreGraphics.framework | `_CGOpenGLDisplayMaskToDisplayID` | 4 | bambu-studio, crossover, kitty, orcaslicer |
| CoreGraphics.framework | `_kCGWindowMemoryUsage` | 4 | anythingllm, bartender, jordanbaird-ice, macwhisper |
| CoreText.framework | `_kCTFontMacintoshEncodingsAttribute` | 4 | microsoft-excel, microsoft-outlook, microsoft-powerpoint, microsoft-word |
| ImageIO.framework | `_kCGImagePropertyGPSLatitude` | 4 | capcut, feishu, gemini, proxyman |
| ImageIO.framework | `_kCGImagePropertyGPSLongitude` | 4 | capcut, feishu, gemini, proxyman |
| CoreGraphics.framework | `_CGPostMouseEvent` | 3 | bartender, feishu, teamviewer |
| ImageIO.framework | `_kCGImagePropertyGPSLatitudeRef` | 3 | capcut, feishu, proxyman |
| ImageIO.framework | `_kCGImagePropertyGPSLongitudeRef` | 3 | capcut, feishu, proxyman |
| PDFKit.framework | `_OBJC_METACLASS_$_PDFView` | 3 | bettertouchtool, cmux, microsoft-word |
| libswiftCoreGraphics.dylib | `_$sSo6CGSizeV12CoreGraphicsE16debugDescriptionSSvg` | 3 | macwhisper, raycast, rectangle |
| libswiftCoreGraphics.dylib | `_$sSo9CGPathRefa12CoreGraphicsE5union_5usingA2B_AC0A8FillRuleOtF` | 3 | jordanbaird-ice, sf-symbols, vorssaint |
| ColorSync.framework | `_kColorSyncProfileURL` | 2 | adobe-acrobat-reader, betterdisplay |
| CoreGraphics.framework | `_$sSo16CGMutablePathRefa12CoreGraphicsE14addRoundedRect2in11cornerWidth0J6Height9transformySo6CGRectV_AC7CGFloatVALSo17CGAffineTransformVtF` | 2 | microsoft-office, orion |
| CoreGraphics.framework | `_CGContextGetType` | 2 | orion, sf-symbols |
| CoreGraphics.framework | `_CGCursorIsVisible` | 2 | epic-games, zoom |
| CoreGraphics.framework | `_CGRequestPostEventAccess` | 2 | macwhisper, openclaw |
| CoreGraphics.framework | `_OBJC_CLASS_$_CGVirtualDisplay` | 2 | betterdisplay, displaylink |
| CoreGraphics.framework | `_OBJC_CLASS_$_CGVirtualDisplayDescriptor` | 2 | betterdisplay, displaylink |
| CoreGraphics.framework | `_OBJC_CLASS_$_CGVirtualDisplayMode` | 2 | betterdisplay, displaylink |
| CoreGraphics.framework | `_OBJC_CLASS_$_CGVirtualDisplaySettings` | 2 | betterdisplay, displaylink |
| CoreGraphics.framework | `_kCGWindowBackingLocationVideoMemory` | 2 | bartender, jordanbaird-ice |
| CoreGraphics.framework | `_kCGWindowStoreType` | 2 | bartender, jordanbaird-ice |
| CoreGraphics.framework | `_kCGWindowWorkspace` | 2 | hammerspoon, zoom |
| CoreText.framework | `_kCTFontFixedAdvanceAttribute` | 2 | microsoft-edge, orion |
| CoreText.framework | `_kCTFontOpticalSizeAttribute` | 2 | orion, unity-hub |
| CoreText.framework | `_kCTFontPriorityAttribute` | 2 | capcut, wechat |
| ImageIO.framework | `_kCGImagePropertyTIFFTileLength` | 2 | adobe-acrobat-reader, bettertouchtool |
| ImageIO.framework | `_kCGImagePropertyTIFFTileWidth` | 2 | adobe-acrobat-reader, bettertouchtool |
| libswiftCoreGraphics.dylib | `_$sSo12CGContextRefa12CoreGraphicsE18strokeLineSegments7betweenySaySo7CGPointVG_tF` | 2 | cleanshot, tableplus |
| libswiftCoreGraphics.dylib | `_$sSo12CGContextRefa12CoreGraphicsE4clip2toySaySo6CGRectVG_tF` | 2 | utm, vorssaint |
| libswiftCoreGraphics.dylib | `_$sSo9CGPathRefa12CoreGraphicsE10intersects_5usingSbAB_AC0A8FillRuleOtF` | 2 | sf-symbols, sfm |
| libswiftCoreGraphics.dylib | `_$sSo9CGPathRefa12CoreGraphicsE10normalized5usingAbC0A8FillRuleO_tF` | 2 | cleanshot, sf-symbols |
| libswiftCoreGraphics.dylib | `_$sSo9CGPathRefa12CoreGraphicsE11subtracting_5usingA2B_AC0A8FillRuleOtF` | 2 | cleanshot, sf-symbols |
| ColorSync.framework | `_ColorSyncCMMCopyLocalizedName` | 1 | adobe-acrobat-reader |
| ColorSync.framework | `_ColorSyncCMMGetBundle` | 1 | adobe-acrobat-reader |
| ColorSync.framework | `_ColorSyncDeviceSetCustomProfiles` | 1 | betterdisplay |
| ColorSync.framework | `_ColorSyncIterateInstalledCMMs` | 1 | adobe-acrobat-reader |
| ColorSync.framework | `_ColorSyncIterateInstalledProfiles` | 1 | adobe-acrobat-reader |
| ColorSync.framework | `_ColorSyncIterateInstalledProfilesWithOptions` | 1 | betterdisplay |
| ColorSync.framework | `_ColorSyncProfileCreateWithDisplayID` | 1 | affinity |
| ColorSync.framework | `_ColorSyncTransformCopyProperty` | 1 | affinity |
| ColorSync.framework | `_kColorSyncConversion3DLut` | 1 | affinity |
| ColorSync.framework | `_kColorSyncConversionGridPoints` | 1 | affinity |
| ColorSync.framework | `_kColorSyncProfileClass` | 1 | betterdisplay |
| ColorSync.framework | `_kColorSyncProfileColorSpace` | 1 | betterdisplay |
| ColorSync.framework | `_kColorSyncProfileDescription` | 1 | betterdisplay |
| ColorSync.framework | `_kColorSyncProfileRepositoryChangeNotification` | 1 | betterdisplay |
| ColorSync.framework | `_kColorSyncTransformSimplifiedConversionData` | 1 | affinity |
| ColorSync.framework | `_kColorSyncWaitForCacheReply` | 1 | betterdisplay |
| CoreGraphics.framework | `_$sSo10CGColorRefa12CoreGraphicsE5whiteABvgZ` | 1 | orion |
| CoreGraphics.framework | `_$sSo12CGContextRefa12CoreGraphicsE10showGlyphs_2atySays6UInt16VG_SaySo7CGPointVGtF` | 1 | coteditor |
| CoreGraphics.framework | `_$sSo16CGMutablePathRefa12CoreGraphicsE10addEllipse2in9transformySo6CGRectV_So17CGAffineTransformVtF` | 1 | microsoft-office |
| CoreGraphics.framework | `_$sSo16CGMutablePathRefa12CoreGraphicsE8addLines7between9transformySaySo7CGPointVG_So17CGAffineTransformVtF` | 1 | coteditor |
| CoreGraphics.framework | `_$sSo6CGRectV12CoreGraphicsE16debugDescriptionSSvg` | 1 | thaw |
| CoreGraphics.framework | `_$sSo9CGPathRefa12CoreGraphicsE4copy17strokingWithWidth7lineCap0I4Join10miterLimit9transformAbC7CGFloatV_So06CGLineJ0VSo0pK0VAKSo17CGAffineTransformVtF` | 1 | coteditor |
| CoreGraphics.framework | `_$sSo9CGPathRefa12CoreGraphicsE5union_5usingA2B_AC0A8FillRuleOtF` | 1 | thaw |
| CoreGraphics.framework | `_CGColorSpaceCopyICCProfileDescription` | 1 | orion |
| CoreGraphics.framework | `_CGColorSpaceEqualToColorSpace` | 1 | orion |
| CoreGraphics.framework | `_CGColorTransformConvertColorComponents` | 1 | orion |
| CoreGraphics.framework | `_CGColorTransformCreate` | 1 | orion |
| CoreGraphics.framework | `_CGContextCopyTopGState` | 1 | sf-symbols |
| CoreGraphics.framework | `_CGContextCreateWithDelegate` | 1 | orion |
| CoreGraphics.framework | `_CGContextDelegateCreate` | 1 | orion |
| CoreGraphics.framework | `_CGContextDelegateGetInfo` | 1 | orion |
| CoreGraphics.framework | `_CGContextDelegateSetCallback` | 1 | orion |
| CoreGraphics.framework | `_CGContextGetAllowsFontSmoothing` | 1 | orion |
| CoreGraphics.framework | `_CGContextGetAllowsFontSubpixelPositioning` | 1 | orion |
| CoreGraphics.framework | `_CGContextGetAllowsFontSubpixelQuantization` | 1 | orion |
| CoreGraphics.framework | `_CGContextGetBaseCTM` | 1 | orion |
| CoreGraphics.framework | `_CGContextGetColorSpace` | 1 | orion |
| CoreGraphics.framework | `_CGContextGetFillColorAsColor` | 1 | orion |
| CoreGraphics.framework | `_CGContextGetShouldAntialias` | 1 | orion |
| CoreGraphics.framework | `_CGContextGetShouldSmoothFonts` | 1 | orion |
| CoreGraphics.framework | `_CGContextGetStrokeColorAsColor` | 1 | orion |
| CoreGraphics.framework | `_CGContextReplaceTopGState` | 1 | sf-symbols |
| CoreGraphics.framework | `_CGContextSetCompositeOperation` | 1 | orion |
| CoreGraphics.framework | `_CGContextSetShouldAntialiasFonts` | 1 | orion |
| CoreGraphics.framework | `_CGContextSetStyle` | 1 | orion |
| CoreGraphics.framework | `_CGDataProviderCreateMultiRangeDirectAccess` | 1 | orion |
| CoreGraphics.framework | `_CGDataProviderCreateWithCopyOfData` | 1 | orion |
| CoreGraphics.framework | `_CGDataProviderSetProperty` | 1 | orion |
| CoreGraphics.framework | `_CGEnterLockdownModeForPDF` | 1 | orion |
| CoreGraphics.framework | `_CGGStateGetAlpha` | 1 | orion |
| CoreGraphics.framework | `_CGGStateGetCTM` | 1 | orion |
| CoreGraphics.framework | `_CGGStateGetFillColor` | 1 | orion |
| CoreGraphics.framework | `_CGGStateGetFont` | 1 | orion |
| CoreGraphics.framework | `_CGGStateGetFontSize` | 1 | orion |
| CoreGraphics.framework | `_CGGStateGetStrokeColor` | 1 | orion |
| CoreGraphics.framework | `_CGGStateGetStyle` | 1 | orion |
| CoreGraphics.framework | `_CGGStateRelease` | 1 | sf-symbols |
| CoreGraphics.framework | `_CGGradientCreateWithColorComponentsAndOptions` | 1 | orion |
| CoreGraphics.framework | `_CGIOSurfaceContextCreate` | 1 | orion |
| CoreGraphics.framework | `_CGIOSurfaceContextCreateImage` | 1 | orion |
| CoreGraphics.framework | `_CGIOSurfaceContextCreateImageReference` | 1 | orion |
| CoreGraphics.framework | `_CGIOSurfaceContextGetBitmapInfo` | 1 | orion |
| CoreGraphics.framework | `_CGIOSurfaceContextGetColorSpace` | 1 | orion |
| CoreGraphics.framework | `_CGIOSurfaceContextSetDisplayMask` | 1 | orion |
| CoreGraphics.framework | `_CGImageGetCachingFlags` | 1 | orion |
| CoreGraphics.framework | `_CGImageGetContentHeadroom` | 1 | orion |
| CoreGraphics.framework | `_CGImageSetCachingFlags` | 1 | orion |
| CoreGraphics.framework | `_CGImageSetProperty` | 1 | orion |
| CoreGraphics.framework | `_CGImageWriteToFile` | 1 | sf-symbols |
| CoreGraphics.framework | `_CGPDFDocumentIsTaggedPDF` | 1 | orion |
| CoreGraphics.framework | `_CGPDFPageLayoutGetAreaOfInterestAtPoint` | 1 | orion |
| CoreGraphics.framework | `_CGPSConverterAbort` | 1 | skim |
| CoreGraphics.framework | `_CGPSConverterConvert` | 1 | skim |
| CoreGraphics.framework | `_CGPSConverterCreate` | 1 | skim |
| CoreGraphics.framework | `_CGPathAddContinuousRoundedRect` | 1 | orion |
| CoreGraphics.framework | `_CGPathAddUnevenCornersRoundedRect` | 1 | orion |
| CoreGraphics.framework | `_CGPathIntersectsRect` | 1 | sf-symbols |
| CoreGraphics.framework | `_CGPatternCreateWithImage2` | 1 | orion |
| CoreGraphics.framework | `_CGPostKeyboardEvent` | 1 | bettertouchtool |
| CoreGraphics.framework | `_CGUnregisterScreenRefreshCallback` | 1 | zoom |
| CoreGraphics.framework | `_CPSEnableForegroundOperation` | 1 | deluge |
| CoreGraphics.framework | `_CPSEqualProcess` | 1 | dropbox |
| CoreGraphics.framework | `_CPSGetCurrentProcess` | 1 | deluge |
| CoreGraphics.framework | `_CPSGetWindowOwner` | 1 | dropbox |
| CoreGraphics.framework | `_CPSSetFrontProcess` | 1 | deluge |
| CoreGraphics.framework | `_CPSSetProcessName` | 1 | deluge |
| CoreGraphics.framework | `_kCGColorSpaceExtendedRange` | 1 | orion |
| CoreGraphics.framework | `_kCGDataProviderHasHighLatency` | 1 | orion |
| CoreGraphics.framework | `_kCGDataProviderIndeterminateSize` | 1 | orion |
| CoreGraphics.framework | `_kCGGradientInterpolatesPremultiplied` | 1 | orion |
| CoreGraphics.framework | `_kCGSPackagesMainDisplayIdentifier` | 1 | openclaw |
| CoreText.framework | `_CTFontCopyColorGlyphCoverage` | 1 | orion |
| CoreText.framework | `_CTFontCopyGlyphCoverageForFeature` | 1 | orion |
| CoreText.framework | `_CTFontCreateForCharactersWithLanguage` | 1 | orion |
| CoreText.framework | `_CTFontCreateForCharactersWithLanguageAndOption` | 1 | orion |
| CoreText.framework | `_CTFontCreateWithQuickdrawInstance` | 1 | affinity |
| CoreText.framework | `_CTFontDescriptorCreateForCSSFamily` | 1 | orion |
| CoreText.framework | `_CTFontDescriptorCreateLastResort` | 1 | orion |
| CoreText.framework | `_CTFontDescriptorCreateWithAttributesAndOptions` | 1 | orion |
| CoreText.framework | `_CTFontDescriptorCreateWithTextStyle` | 1 | orion |
| CoreText.framework | `_CTFontDescriptorGetOptions` | 1 | orion |
| CoreText.framework | `_CTFontGetGlyphsForCharacterRange` | 1 | orion |
| CoreText.framework | `_CTFontGetPhysicalSymbolicTraits` | 1 | orion |
| CoreText.framework | `_CTFontGetSbixImageSizeForGlyphAndContentsScale` | 1 | orion |
| CoreText.framework | `_CTFontGetUIFontType` | 1 | orion |
| CoreText.framework | `_CTFontGetVerticalGlyphsForCharacters` | 1 | orion |
| CoreText.framework | `_CTFontIsAppleColorEmoji` | 1 | orion |
| CoreText.framework | `_CTFontManagerCompareFontFamilyNames` | 1 | emacs-app |
| CoreText.framework | `_CTFontManagerCreateMemorySafeFontDescriptorFromData` | 1 | orion |
| CoreText.framework | `_CTFontManagerEnableAllUserFonts` | 1 | orion |
| CoreText.framework | `_CTFontShapeGlyphs` | 1 | orion |
| CoreText.framework | `_CTFontTransformGlyphsWithLanguage` | 1 | sf-symbols |
| CoreText.framework | `_CTLineCreateWithUniCharProvider` | 1 | orion |
| CoreText.framework | `_CTParagraphStyleSetCompositionLanguage` | 1 | orion |
| CoreText.framework | `_CTRunGetInitialAdvance` | 1 | orion |
| CoreText.framework | `_CTTypesetterCreateWithUniCharProviderAndOptions` | 1 | orion |
| CoreText.framework | `_kCTFontBaselineAdjustAttribute` | 1 | orion |
| CoreText.framework | `_kCTFontCSSFamilyCursive` | 1 | orion |
| CoreText.framework | `_kCTFontCSSFamilyFantasy` | 1 | orion |
| CoreText.framework | `_kCTFontCSSFamilyMonospace` | 1 | orion |
| CoreText.framework | `_kCTFontCSSFamilySansSerif` | 1 | orion |
| CoreText.framework | `_kCTFontCSSFamilySerif` | 1 | orion |
| CoreText.framework | `_kCTFontCSSWeightAttribute` | 1 | orion |
| CoreText.framework | `_kCTFontCSSWidthAttribute` | 1 | orion |
| CoreText.framework | `_kCTFontDescriptorTextStyleAttribute` | 1 | orion |
| CoreText.framework | `_kCTFontDownloadedAttribute` | 1 | orion |
| CoreText.framework | `_kCTFontFallbackOptionAttribute` | 1 | orion |
| CoreText.framework | `_kCTFontIgnoreLegibilityWeightAttribute` | 1 | orion |
| CoreText.framework | `_kCTFontPaletteAttribute` | 1 | orion |
| CoreText.framework | `_kCTFontPaletteColorsAttribute` | 1 | orion |
| CoreText.framework | `_kCTFontPostScriptNameAttribute` | 1 | orion |
| CoreText.framework | `_kCTFontReferenceURLAttribute` | 1 | orion |
| CoreText.framework | `_kCTFontSizeCategoryAttribute` | 1 | orion |
| CoreText.framework | `_kCTFontTrackAttribute` | 1 | orion |
| CoreText.framework | `_kCTFontUnscaledTrackingAttribute` | 1 | orion |
| CoreText.framework | `_kCTFontUserInstalledAttribute` | 1 | orion |
| CoreText.framework | `_kCTFontVariationAxisHiddenKey` | 1 | affinity |
| CoreText.framework | `_kCTFontWidthExtraCompressed` | 1 | orion |
| CoreText.framework | `_kCTFontWidthExtraCondensed` | 1 | orion |
| CoreText.framework | `_kCTFontWidthExtraExpanded` | 1 | orion |
| CoreText.framework | `_kCTFontWidthSemiCondensed` | 1 | orion |
| CoreText.framework | `_kCTFontWidthSemiExpanded` | 1 | orion |
| CoreText.framework | `_kCTFontWidthUltraCompressed` | 1 | orion |
| CoreText.framework | `_kCTFrameMaximumNumberOfLinesAttributeName` | 1 | orion |
| CoreText.framework | `_kCTUIFontTextStyleShortBody` | 1 | orion |
| CoreText.framework | `_kCTUIFontTextStyleShortCaption1` | 1 | orion |
| CoreText.framework | `_kCTUIFontTextStyleShortFootnote` | 1 | orion |
| CoreText.framework | `_kCTUIFontTextStyleShortHeadline` | 1 | orion |
| CoreText.framework | `_kCTUIFontTextStyleShortSubhead` | 1 | orion |
| CoreText.framework | `_kCTUIFontTextStyleTallBody` | 1 | orion |
| CoreText.framework | `_kCTUIFontTextStyleTitle4` | 1 | orion |
| ImageIO.framework | `_CGImageSourceDisableHardwareDecoding` | 1 | orion |
| ImageIO.framework | `_CGImageSourceEnableRestrictedDecoding` | 1 | orion |
| ImageIO.framework | `_CGImageSourceGetTypeWithData` | 1 | orion |
| ImageIO.framework | `_CGImageSourceSetAllowableTypes` | 1 | orion |
| ImageIO.framework | `_kCFErrorDomainCGImageMetadata` | 1 | bbedit |
| ImageIO.framework | `_kCGImagePropertyAuxiliaryData` | 1 | orion |
| ImageIO.framework | `_kCGImagePropertyAuxiliaryDataType` | 1 | orion |
| ImageIO.framework | `_kCGImagePropertyExifLensSerialNumber` | 1 | gemini |
| ImageIO.framework | `_kCGImagePropertyFileContentsDictionary` | 1 | orion |
| ImageIO.framework | `_kCGImagePropertyGPSAltitude` | 1 | gemini |
| ImageIO.framework | `_kCGImagePropertyGPSDateStamp` | 1 | gemini |
| ImageIO.framework | `_kCGImagePropertyGPSImgDirection` | 1 | gemini |
| ImageIO.framework | `_kCGImagePropertyGPSSpeed` | 1 | gemini |
| ImageIO.framework | `_kCGImagePropertyGPSTimeStamp` | 1 | gemini |
| ImageIO.framework | `_kCGImagePropertyIPTCObjectName` | 1 | bbedit |
| ImageIO.framework | `_kCGImageSourceShouldPreferRGB32` | 1 | orion |
| ImageIO.framework | `_kCGImageSourceSkipMetadata` | 1 | orion |
| ImageIO.framework | `_kCGImageSourceUseHardwareAcceleration` | 1 | orion |
| Quartz.framework | `_IKPictureTakerOutputImageMaxSizeKey` | 1 | telegram |
| Quartz.framework | `_IKPictureTakerShowEffectsKey` | 1 | telegram |
| Quartz.framework | `_OBJC_CLASS_$_IKPictureTaker` | 1 | telegram |
| Quartz.framework | `_OBJC_CLASS_$_IKSlideshow` | 1 | orion |
| Quartz.framework | `_OBJC_CLASS_$_PDFAnnotationButtonWidget` | 1 | skim |
| Quartz.framework | `_OBJC_CLASS_$_PDFAnnotationChoiceWidget` | 1 | skim |
| Quartz.framework | `_OBJC_CLASS_$_PDFAnnotationTextWidget` | 1 | skim |
| Quartz.framework | `_OBJC_METACLASS_$_PDFAnnotation` | 1 | skim |
| Quartz.framework | `_OBJC_METACLASS_$_PDFDocument` | 1 | skim |
| Quartz.framework | `_OBJC_METACLASS_$_PDFPage` | 1 | skim |
| Quartz.framework | `_OBJC_METACLASS_$_PDFView` | 1 | skim |
| Quartz.framework | `_PDFAnnotationWidgetSubtypeButton` | 1 | skim |
| Quartz.framework | `_PDFAnnotationWidgetSubtypeChoice` | 1 | skim |
| Quartz.framework | `_PDFAnnotationWidgetSubtypeText` | 1 | skim |
| Quartz.framework | `_kPDFDestinationUnspecifiedValue` | 1 | skim |
| libswiftCoreGraphics.dylib | `_$s12CoreGraphics22_CGColorInitTrampolinePAAE16_colorLiteralRed5green4blue5alphaxSf_S3ftcfC` | 1 | loop |
| libswiftCoreGraphics.dylib | `_$sSo10CGColorRefa12CoreGraphics01_A14InitTrampolineACWP` | 1 | loop |
| libswiftCoreGraphics.dylib | `_$sSo12CGContextRefa12CoreGraphicsE10showGlyphs_2atySays6UInt16VG_SaySo7CGPointVGtF` | 1 | fork |
| libswiftCoreGraphics.dylib | `_$sSo12CGContextRefa12CoreGraphicsE12addQuadCurve2to7controlySo7CGPointV_AHtF` | 1 | vorssaint |
| libswiftCoreGraphics.dylib | `_$sSo12CGContextRefa12CoreGraphicsE4draw_2inySo07CGLayerB0a_So6CGRectVtF` | 1 | tableplus |
| libswiftCoreGraphics.dylib | `_$sSo16CGMutablePathRefa12CoreGraphicsE14addRelativeArc6center6radius10startAngle5delta9transformySo7CGPointV_AC7CGFloatVA2MSo17CGAffineTransformVtF` | 1 | tableplus |
| libswiftCoreGraphics.dylib | `_$sSo16CGMutablePathRefa12CoreGraphicsE8addRects_9transformySaySo6CGRectVG_So17CGAffineTransformVtF` | 1 | cleanshot |
| libswiftCoreGraphics.dylib | `_$sSo9CGPathRefa12CoreGraphicsE12intersection_5usingA2B_AC0A8FillRuleOtF` | 1 | sf-symbols |
| self) | `_CGPSConverterCreate` | 1 | dropbox |
| self) | `_CGReleaseScreenRefreshRects` | 1 | dropbox |
| self) | `_CGUnregisterScreenRefreshCallback` | 1 | dropbox |
| self) | `_CGWaitForScreenRefreshRects` | 1 | dropbox |
| self) | `_CGWaitForScreenUpdateRects` | 1 | dropbox |
