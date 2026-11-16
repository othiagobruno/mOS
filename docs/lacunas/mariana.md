# Lacunas de Mariana (CG, Core Text, ImageIO, PDFKit)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Mariana (CG, Core Text, ImageIO, PDFKit) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**273** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| CoreGraphics.framework | `_kCGColorWhite` | 32 | audacity, bitwarden, calibre, daisydisk |
| CoreText.framework | `_kCTFontLanguagesAttribute` | 25 | adobe-acrobat-reader, audacity, calibre, db-browser-for-sqlite |
| CoreText.framework | `_kCTFontUniqueNameKey` | 24 | audacity, calibre, db-browser-for-sqlite, dolphin |
| CoreText.framework | `_kCTFontCopyrightNameKey` | 23 | audacity, calibre, db-browser-for-sqlite, dolphin |
| CoreText.framework | `_kCTFontVariationAxisNameKey` | 17 | audacity, calibre, cmux, figma |
| PDFKit.framework | `_OBJC_CLASS_$_PDFDocument` | 15 | adobe-acrobat-reader, bbedit, bettertouchtool, cmux |
| libswiftCoreGraphics.dylib | `_$sSo10CGColorRefa12CoreGraphicsE5whiteABvgZ` | 12 | cleanshot, duckduckgo, easydict, iterm2 |
| CoreText.framework | `_CTFontCopyNameForGlyph` | 10 | audacity, calibre, keepassxc, moonlight |
| CoreText.framework | `_CTFontManagerRegisterFontURLs` | 10 | 1password, firefox, floorp, librewolf |
| CoreGraphics.framework | `_CGPDFContextAddDestinationAtPoint` | 9 | firefox, floorp, librewolf, mullvad-browser |
| CoreGraphics.framework | `_CGPDFContextSetDestinationForRect` | 9 | firefox, floorp, librewolf, mullvad-browser |
| CoreGraphics.framework | `_CGPDFContextSetURLForRect` | 9 | firefox, floorp, librewolf, mullvad-browser |
| CoreText.framework | `_kCTWritingDirectionAttributeName` | 9 | firefox, floorp, librewolf, mullvad-browser |
| libswiftCoreGraphics.dylib | `_$sSo16CGMutablePathRefa12CoreGraphicsE10addEllipse2in9transformySo6CGRectV_So17CGAffineTransformVtF` | 9 | bettertouchtool, cleanshot, clipy, easydict |
| CoreGraphics.framework | `_CGContextSetBaseCTM` | 8 | firefox, floorp, librewolf, mullvad-browser |
| CoreGraphics.framework | `_CGContextSetCTM` | 8 | firefox, floorp, librewolf, mullvad-browser |
| CoreText.framework | `_CTRunGetAdvancesPtr` | 8 | bbedit, cmux, ghostty, orbstack |
| CoreGraphics.framework | `_CGEnableEventStateCombining` | 7 | android-studio, bettertouchtool, cryptomator, dbeaver-community |
| CoreText.framework | `_CTFontCopyDisplayName` | 7 | cmux, ghostty, kitty, markedit |
| CoreText.framework | `_CTFontDescriptorCreateCopyWithVariation` | 7 | cmux, ghostty, orbstack, rustdesk |
| CoreGraphics.framework | `_CGPDFContextEndPage` | 6 | duckduckgo, kicad, sfm, skim |
| CoreGraphics.framework | `_kCGPDFContextCreator` | 6 | floorp, mullvad-browser, thunderbird, tor-browser |
| CoreText.framework | `_CTRunGetTypeID` | 6 | adobe-acrobat-reader, audacity, calibre, musescore |
| CoreText.framework | `_kCTFontVersionNameKey` | 6 | android-studio, intellij-idea, jetbrains-toolbox, openlogi |
| libswiftCoreGraphics.dylib | `_$sSo16CGMutablePathRefa12CoreGraphicsE8addLines7between9transformySaySo7CGPointVG_So17CGAffineTransformVtF` | 6 | bettertouchtool, cleanshot, cmux, iterm2 |
| CoreGraphics.framework | `_CGPDFContextBeginPage` | 5 | duckduckgo, kicad, skim, typora |
| CoreGraphics.framework | `_CGPreflightPostEventAccess` | 5 | macwhisper, openclaw, superwhisper, voiceink |
| CoreText.framework | `_CTFontGetPlatformFont` | 5 | adobe-acrobat-reader, another-redis-desktop-manager, openmtp, pencil |
| CoreText.framework | `_kCTParagraphStyleAttributeName` | 5 | bettertouchtool, cryptomator, rustdesk, utm |
| libswiftCoreGraphics.dylib | `_$sSo16CGMutablePathRefa12CoreGraphicsE14addRoundedRect2in11cornerWidth0J6Height9transformySo6CGRectV_AC7CGFloatVALSo17CGAffineTransformVtF` | 5 | cleanshot, raycast, sfm, shottr |
| libswiftCoreGraphics.dylib | `_$sSo6CGRectV12CoreGraphicsE16debugDescriptionSSvg` | 5 | bartender, loop, macwhisper, raycast |
| libswiftCoreGraphics.dylib | `_$sSo7CGPointV12CoreGraphicsE16debugDescriptionSSvg` | 5 | alt-tab, bartender, loop, raycast |
| ColorSync.framework | `_kColorSyncDeviceProfileURL` | 4 | adobe-acrobat-reader, betterdisplay, blender, krita |
| CoreGraphics.framework | `_CGColorSpaceCopyICCProfile` | 4 | another-redis-desktop-manager, openmtp, pencil, webtorrent |
| CoreGraphics.framework | `_CGGetEventTapList` | 4 | bartender, bettertouchtool, macwhisper, openlogi |
| CoreText.framework | `_CTFontDescriptorCreateCopyWithSymbolicTraits` | 4 | espanso, kicad, kitty, prusaslicer |
| CoreText.framework | `_CTFontGetStringEncoding` | 4 | adobe-acrobat-reader, espanso, kicad, prusaslicer |
| CoreText.framework | `_CTParagraphStyleCreate` | 4 | cryptomator, rustdesk, utm, warp |
| CoreText.framework | `_kCTLigatureAttributeName` | 4 | bettertouchtool, iterm2, microsoft-edge, utm |
| CoreText.framework | `_kCTStrokeColorAttributeName` | 4 | bettertouchtool, espanso, kicad, prusaslicer |
| CoreText.framework | `_kCTStrokeWidthAttributeName` | 4 | bettertouchtool, espanso, kicad, prusaslicer |
| libswiftCoreGraphics.dylib | `_$sSo9CGPathRefa12CoreGraphicsE4copy17strokingWithWidth7lineCap0I4Join10miterLimit9transformAbC7CGFloatV_So06CGLineJ0VSo0pK0VAKSo17CGAffineTransformVtF` | 4 | bartender, bettertouchtool, cleanshot, proxyman |
| ColorSync.framework | `_ColorSyncDeviceCopyDeviceInfo` | 3 | betterdisplay, blender, krita |
| ColorSync.framework | `_ColorSyncIterateDeviceProfiles` | 3 | adobe-acrobat-reader, betterdisplay, iina |
| ColorSync.framework | `_kColorSyncCustomProfiles` | 3 | betterdisplay, blender, krita |
| ColorSync.framework | `_kColorSyncFactoryProfiles` | 3 | betterdisplay, blender, krita |
| CoreGraphics.framework | `_kCGWindowMemoryUsage` | 3 | bartender, jordanbaird-ice, macwhisper |
| CoreText.framework | `_CTLineGetStringIndexForPosition` | 3 | sfm, telegram, zoom |
| CoreText.framework | `_CTRunDelegateCreate` | 3 | openttd, telegram, zoom |
| CoreText.framework | `_kCTFontSubFamilyNameKey` | 3 | openlogi, warp, zed |
| CoreText.framework | `_kCTRunDelegateAttributeName` | 3 | openttd, telegram, zoom |
| CoreText.framework | `_kCTUnderlineStyleAttributeName` | 3 | bettertouchtool, rustdesk, zoom |
| ImageIO.framework | `_CGImageSourceCopyMetadataAtIndex` | 3 | adobe-acrobat-reader, cleanshot, whatsapp |
| ImageIO.framework | `_kCGImagePropertyExifUserComment` | 3 | bbedit, cleanshot, shottr |
| PDFKit.framework | `_OBJC_CLASS_$_PDFPage` | 3 | adobe-acrobat-reader, realvnc-connect-viewer, whatsapp |
| PDFKit.framework | `_OBJC_CLASS_$_PDFView` | 3 | bettertouchtool, cmux, protonvpn |
| Quartz.framework | `_OBJC_CLASS_$_PDFDocument` | 3 | alfred, r-app, skim |
| libswiftCoreGraphics.dylib | `_$sSo6CGSizeV12CoreGraphicsE16debugDescriptionSSvg` | 3 | macwhisper, raycast, rectangle |
| ColorSync.framework | `_kColorSyncDeviceClass` | 2 | adobe-acrobat-reader, betterdisplay |
| ColorSync.framework | `_kColorSyncDeviceDescription` | 2 | adobe-acrobat-reader, betterdisplay |
| ColorSync.framework | `_kColorSyncDisplayDeviceProfilesNotification` | 2 | betterdisplay, monitorcontrol |
| ColorSync.framework | `_kColorSyncProfileURL` | 2 | adobe-acrobat-reader, betterdisplay |
| CoreGraphics.framework | `_CGContextSetFontSmoothingStyle` | 2 | iterm2, sfm |
| CoreGraphics.framework | `_CGCursorIsVisible` | 2 | epic-games, zoom |
| CoreGraphics.framework | `_CGPDFArrayGetCount` | 2 | skim, typora |
| CoreGraphics.framework | `_CGPDFDictionaryGetArray` | 2 | skim, typora |
| CoreGraphics.framework | `_CGPDFDictionaryGetDictionary` | 2 | skim, typora |
| CoreGraphics.framework | `_CGPDFDictionaryGetString` | 2 | skim, typora |
| CoreGraphics.framework | `_CGPDFStringGetBytePtr` | 2 | skim, typora |
| CoreGraphics.framework | `_CGPostMouseEvent` | 2 | bartender, teamviewer |
| CoreGraphics.framework | `_CGRequestPostEventAccess` | 2 | macwhisper, openclaw |
| CoreGraphics.framework | `_OBJC_CLASS_$_CGVirtualDisplay` | 2 | betterdisplay, displaylink |
| CoreGraphics.framework | `_OBJC_CLASS_$_CGVirtualDisplayDescriptor` | 2 | betterdisplay, displaylink |
| CoreGraphics.framework | `_OBJC_CLASS_$_CGVirtualDisplayMode` | 2 | betterdisplay, displaylink |
| CoreGraphics.framework | `_OBJC_CLASS_$_CGVirtualDisplaySettings` | 2 | betterdisplay, displaylink |
| CoreGraphics.framework | `_kCGWindowBackingLocationVideoMemory` | 2 | bartender, jordanbaird-ice |
| CoreGraphics.framework | `_kCGWindowStoreType` | 2 | bartender, jordanbaird-ice |
| CoreGraphics.framework | `_kCGWindowWorkspace` | 2 | hammerspoon, zoom |
| CoreText.framework | `_CTFontGetOpticalBoundsForGlyphs` | 2 | openttd, zoom |
| CoreText.framework | `_CTFontManagerCopyAvailableFontURLs` | 2 | adobe-acrobat-reader, figma |
| CoreText.framework | `_CTFontManagerUnregisterFontsForURL` | 2 | iterm2, libreoffice |
| CoreText.framework | `_CTFontManagerUnregisterGraphicsFont` | 2 | openclaw, telegram |
| CoreText.framework | `_CTLineGetPenOffsetForFlush` | 2 | bbedit, telegram |
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
| CoreText.framework | `_kCTGlyphInfoAttributeName` | 2 | bbedit, bettertouchtool |
| ImageIO.framework | `_CGImageDestinationCopyImageSource` | 2 | bbedit, cleanshot |
| ImageIO.framework | `_CGImageMetadataCopyStringValueWithPath` | 2 | cleanshot, whatsapp |
| ImageIO.framework | `_CGImageMetadataCreateMutable` | 2 | bbedit, cleanshot |
| ImageIO.framework | `_CGImageMetadataTagCopyValue` | 2 | cleanshot, whatsapp |
| ImageIO.framework | `_kCGImagePropertyGPSLatitude` | 2 | gemini, proxyman |
| ImageIO.framework | `_kCGImagePropertyGPSLongitude` | 2 | gemini, proxyman |
| ImageIO.framework | `_kCGImagePropertyTIFFTileLength` | 2 | adobe-acrobat-reader, bettertouchtool |
| ImageIO.framework | `_kCGImagePropertyTIFFTileWidth` | 2 | adobe-acrobat-reader, bettertouchtool |
| PDFKit.framework | `_OBJC_CLASS_$_PDFDestination` | 2 | macdown-3000, typora |
| PDFKit.framework | `_OBJC_CLASS_$_PDFOutline` | 2 | cmux, typora |
| PDFKit.framework | `_OBJC_METACLASS_$_PDFView` | 2 | bettertouchtool, cmux |
| PDFKit.framework | `_PDFDocumentTitleAttribute` | 2 | bettertouchtool, typora |
| Quartz.framework | `_OBJC_CLASS_$_PDFView` | 2 | alfred, skim |
| Quartz.framework | `_QLThumbnailCopyImage` | 2 | another-redis-desktop-manager, webtorrent |
| Quartz.framework | `_QLThumbnailCreate` | 2 | another-redis-desktop-manager, webtorrent |
| Quartz.framework | `_QLThumbnailDispatchAsync` | 2 | another-redis-desktop-manager, webtorrent |
| libswiftCoreGraphics.dylib | `_$sSo12CGContextRefa12CoreGraphicsE18strokeLineSegments7betweenySaySo7CGPointVG_tF` | 2 | cleanshot, tableplus |
| libswiftCoreGraphics.dylib | `_$sSo12CGContextRefa12CoreGraphicsE4clip2toySaySo6CGRectVG_tF` | 2 | utm, vorssaint |
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
| CoreGraphics.framework | `_CGFontGetUnitsPerEm` | 1 | battery |
| CoreGraphics.framework | `_CGImageGetTypeID` | 1 | vorssaint |
| CoreGraphics.framework | `_CGOpenGLDisplayMaskToDisplayID` | 1 | kitty |
| CoreGraphics.framework | `_CGPDFArrayGetArray` | 1 | skim |
| CoreGraphics.framework | `_CGPDFArrayGetDictionary` | 1 | skim |
| CoreGraphics.framework | `_CGPDFArrayGetInteger` | 1 | skim |
| CoreGraphics.framework | `_CGPDFArrayGetName` | 1 | skim |
| CoreGraphics.framework | `_CGPDFArrayGetNumber` | 1 | skim |
| CoreGraphics.framework | `_CGPDFArrayGetObject` | 1 | typora |
| CoreGraphics.framework | `_CGPDFArrayGetString` | 1 | skim |
| CoreGraphics.framework | `_CGPDFContextCreateWithURL` | 1 | vorssaint |
| CoreGraphics.framework | `_CGPDFDictionaryGetBoolean` | 1 | skim |
| CoreGraphics.framework | `_CGPDFDictionaryGetInteger` | 1 | skim |
| CoreGraphics.framework | `_CGPDFDictionaryGetName` | 1 | skim |
| CoreGraphics.framework | `_CGPDFDictionaryGetNumber` | 1 | skim |
| CoreGraphics.framework | `_CGPDFDocumentGetCatalog` | 1 | skim |
| CoreGraphics.framework | `_CGPDFDocumentGetID` | 1 | skim |
| CoreGraphics.framework | `_CGPDFObjectGetValue` | 1 | typora |
| CoreGraphics.framework | `_CGPDFPageGetDictionary` | 1 | typora |
| CoreGraphics.framework | `_CGPDFStringCopyDate` | 1 | skim |
| CoreGraphics.framework | `_CGPDFStringCopyTextString` | 1 | skim |
| CoreGraphics.framework | `_CGPDFStringGetLength` | 1 | skim |
| CoreGraphics.framework | `_CGPSConverterAbort` | 1 | skim |
| CoreGraphics.framework | `_CGPSConverterConvert` | 1 | skim |
| CoreGraphics.framework | `_CGPSConverterCreate` | 1 | skim |
| CoreGraphics.framework | `_CGPostKeyboardEvent` | 1 | bettertouchtool |
| CoreGraphics.framework | `_CGUnregisterScreenRefreshCallback` | 1 | zoom |
| CoreGraphics.framework | `_CPSEnableForegroundOperation` | 1 | deluge |
| CoreGraphics.framework | `_CPSGetCurrentProcess` | 1 | deluge |
| CoreGraphics.framework | `_CPSSetFrontProcess` | 1 | deluge |
| CoreGraphics.framework | `_CPSSetProcessName` | 1 | deluge |
| CoreGraphics.framework | `_kCGSPackagesMainDisplayIdentifier` | 1 | openclaw |
| CoreText.framework | `_CTFontManagerCompareFontFamilyNames` | 1 | emacs-app |
| CoreText.framework | `_CTFontManagerGetScopeForURL` | 1 | teamviewer |
| CoreText.framework | `_CTFontManagerRegisterFontDescriptors` | 1 | iterm2 |
| CoreText.framework | `_CTFontManagerUnregisterFontURLs` | 1 | mac-mouse-fix |
| CoreText.framework | `_CTFramesetterGetTypesetter` | 1 | raycast |
| CoreText.framework | `_CTGlyphInfoCreateWithGlyph` | 1 | bbedit |
| CoreText.framework | `_CTGlyphInfoCreateWithGlyphName` | 1 | bbedit |
| CoreText.framework | `_CTLineEnumerateCaretOffsets` | 1 | warp |
| CoreText.framework | `_CTRunGetImageBounds` | 1 | alt-tab |
| CoreText.framework | `_CTTypesetterCreateLineWithOffset` | 1 | telegram |
| CoreText.framework | `_kCTCharacterShapeAttributeName` | 1 | bettertouchtool |
| CoreText.framework | `_kCTFontFixedAdvanceAttribute` | 1 | microsoft-edge |
| CoreText.framework | `_kCTFontManagerErrorDomain` | 1 | bartender |
| CoreText.framework | `_kCTFontMatrixAttribute` | 1 | iterm2 |
| CoreText.framework | `_kCTFontOpticalSizeAttribute` | 1 | unity-hub |
| CoreText.framework | `_kCTSuperscriptAttributeName` | 1 | bettertouchtool |
| CoreText.framework | `_kCTTypesetterOptionAllowUnboundedLayout` | 1 | bbedit |
| CoreText.framework | `_kCTUnderlineColorAttributeName` | 1 | bettertouchtool |
| ImageIO.framework | `_CGImageDestinationCopyTypeIdentifiers` | 1 | vorssaint |
| ImageIO.framework | `_CGImageDestinationSetProperties` | 1 | vorssaint |
| ImageIO.framework | `_CGImageMetadataCopyTagMatchingImageProperty` | 1 | cleanshot |
| ImageIO.framework | `_CGImageMetadataCopyTagWithPath` | 1 | whatsapp |
| ImageIO.framework | `_CGImageMetadataCreateMutableCopy` | 1 | cleanshot |
| ImageIO.framework | `_CGImageMetadataCreateXMPData` | 1 | adobe-acrobat-reader |
| ImageIO.framework | `_CGImageMetadataGetTypeID` | 1 | whatsapp |
| ImageIO.framework | `_CGImageMetadataRemoveTagWithPath` | 1 | cleanshot |
| ImageIO.framework | `_CGImageMetadataSetValueMatchingImageProperty` | 1 | cleanshot |
| ImageIO.framework | `_kCFErrorDomainCGImageMetadata` | 1 | bbedit |
| ImageIO.framework | `_kCGImageMetadataShouldExcludeGPS` | 1 | sfm |
| ImageIO.framework | `_kCGImagePropertyDPIHeight` | 1 | vorssaint |
| ImageIO.framework | `_kCGImagePropertyDPIWidth` | 1 | vorssaint |
| ImageIO.framework | `_kCGImagePropertyExifLensSerialNumber` | 1 | gemini |
| ImageIO.framework | `_kCGImagePropertyGIFDelayTime` | 1 | vorssaint |
| ImageIO.framework | `_kCGImagePropertyGIFDictionary` | 1 | vorssaint |
| ImageIO.framework | `_kCGImagePropertyGIFLoopCount` | 1 | vorssaint |
| ImageIO.framework | `_kCGImagePropertyGPSAltitude` | 1 | gemini |
| ImageIO.framework | `_kCGImagePropertyGPSDateStamp` | 1 | gemini |
| ImageIO.framework | `_kCGImagePropertyGPSImgDirection` | 1 | gemini |
| ImageIO.framework | `_kCGImagePropertyGPSLatitudeRef` | 1 | proxyman |
| ImageIO.framework | `_kCGImagePropertyGPSLongitudeRef` | 1 | proxyman |
| ImageIO.framework | `_kCGImagePropertyGPSSpeed` | 1 | gemini |
| ImageIO.framework | `_kCGImagePropertyGPSTimeStamp` | 1 | gemini |
| ImageIO.framework | `_kCGImagePropertyIPTCObjectName` | 1 | bbedit |
| ImageIO.framework | `_kCGImagePropertyOrientation` | 1 | vorssaint |
| PDFKit.framework | `_OBJC_CLASS_$_PDFActionGoTo` | 1 | macdown-3000 |
| PDFKit.framework | `_OBJC_CLASS_$_PDFAnnotation` | 1 | macdown-3000 |
| PDFKit.framework | `_PDFAnnotationSubtypeLink` | 1 | macdown-3000 |
| PDFKit.framework | `_PDFDocumentAuthorAttribute` | 1 | typora |
| PDFKit.framework | `_PDFDocumentCreatorAttribute` | 1 | typora |
| PDFKit.framework | `_PDFDocumentKeywordsAttribute` | 1 | typora |
| PDFKit.framework | `_PDFDocumentProducerAttribute` | 1 | typora |
| PDFKit.framework | `_PDFDocumentSubjectAttribute` | 1 | typora |
| PDFKit.framework | `_PDFViewPageChangedNotification` | 1 | cmux |
| Quartz.framework | `_IKPictureTakerOutputImageMaxSizeKey` | 1 | telegram |
| Quartz.framework | `_IKPictureTakerShowEffectsKey` | 1 | telegram |
| Quartz.framework | `_OBJC_CLASS_$_IKPictureTaker` | 1 | telegram |
| Quartz.framework | `_OBJC_CLASS_$_PDFActionGoTo` | 1 | skim |
| Quartz.framework | `_OBJC_CLASS_$_PDFAnnotation` | 1 | skim |
| Quartz.framework | `_OBJC_CLASS_$_PDFAnnotationButtonWidget` | 1 | skim |
| Quartz.framework | `_OBJC_CLASS_$_PDFAnnotationChoiceWidget` | 1 | skim |
| Quartz.framework | `_OBJC_CLASS_$_PDFAnnotationTextWidget` | 1 | skim |
| Quartz.framework | `_OBJC_CLASS_$_PDFBorder` | 1 | skim |
| Quartz.framework | `_OBJC_CLASS_$_PDFDestination` | 1 | skim |
| Quartz.framework | `_OBJC_CLASS_$_PDFOutline` | 1 | skim |
| Quartz.framework | `_OBJC_CLASS_$_PDFPage` | 1 | skim |
| Quartz.framework | `_OBJC_CLASS_$_PDFSelection` | 1 | skim |
| Quartz.framework | `_OBJC_METACLASS_$_PDFAnnotation` | 1 | skim |
| Quartz.framework | `_OBJC_METACLASS_$_PDFDocument` | 1 | skim |
| Quartz.framework | `_OBJC_METACLASS_$_PDFPage` | 1 | skim |
| Quartz.framework | `_OBJC_METACLASS_$_PDFView` | 1 | skim |
| Quartz.framework | `_PDFAnnotationKeyDefaultAppearance` | 1 | skim |
| Quartz.framework | `_PDFAnnotationKeyIconName` | 1 | skim |
| Quartz.framework | `_PDFAnnotationKeyLinePoints` | 1 | skim |
| Quartz.framework | `_PDFAnnotationKeySubtype` | 1 | skim |
| Quartz.framework | `_PDFAnnotationWidgetSubtypeButton` | 1 | skim |
| Quartz.framework | `_PDFAnnotationWidgetSubtypeChoice` | 1 | skim |
| Quartz.framework | `_PDFAnnotationWidgetSubtypeText` | 1 | skim |
| Quartz.framework | `_PDFDocumentAuthorAttribute` | 1 | skim |
| Quartz.framework | `_PDFDocumentCreationDateAttribute` | 1 | skim |
| Quartz.framework | `_PDFDocumentCreatorAttribute` | 1 | skim |
| Quartz.framework | `_PDFDocumentDidBeginWriteNotification` | 1 | skim |
| Quartz.framework | `_PDFDocumentDidEndPageWriteNotification` | 1 | skim |
| Quartz.framework | `_PDFDocumentDidEndWriteNotification` | 1 | skim |
| Quartz.framework | `_PDFDocumentDidUnlockNotification` | 1 | skim |
| Quartz.framework | `_PDFDocumentKeywordsAttribute` | 1 | skim |
| Quartz.framework | `_PDFDocumentModificationDateAttribute` | 1 | skim |
| Quartz.framework | `_PDFDocumentProducerAttribute` | 1 | skim |
| Quartz.framework | `_PDFDocumentSubjectAttribute` | 1 | skim |
| Quartz.framework | `_PDFDocumentTitleAttribute` | 1 | skim |
| Quartz.framework | `_PDFViewChangedHistoryNotification` | 1 | skim |
| Quartz.framework | `_PDFViewDisplayBoxChangedNotification` | 1 | skim |
| Quartz.framework | `_PDFViewDisplayModeChangedNotification` | 1 | skim |
| Quartz.framework | `_PDFViewPageChangedNotification` | 1 | skim |
| Quartz.framework | `_PDFViewScaleChangedNotification` | 1 | skim |
| Quartz.framework | `_kPDFDestinationUnspecifiedValue` | 1 | skim |
| libswiftCoreGraphics.dylib | `_$s12CoreGraphics22_CGColorInitTrampolinePAAE16_colorLiteralRed5green4blue5alphaxSf_S3ftcfC` | 1 | loop |
| libswiftCoreGraphics.dylib | `_$sSo10CGColorRefa12CoreGraphics01_A14InitTrampolineACWP` | 1 | loop |
| libswiftCoreGraphics.dylib | `_$sSo12CGContextRefa12CoreGraphicsE10showGlyphs_2atySays6UInt16VG_SaySo7CGPointVGtF` | 1 | fork |
| libswiftCoreGraphics.dylib | `_$sSo12CGContextRefa12CoreGraphicsE12addQuadCurve2to7controlySo7CGPointV_AHtF` | 1 | vorssaint |
| libswiftCoreGraphics.dylib | `_$sSo12CGContextRefa12CoreGraphicsE4draw_2inySo07CGLayerB0a_So6CGRectVtF` | 1 | tableplus |
| libswiftCoreGraphics.dylib | `_$sSo16CGMutablePathRefa12CoreGraphicsE14addRelativeArc6center6radius10startAngle5delta9transformySo7CGPointV_AC7CGFloatVA2MSo17CGAffineTransformVtF` | 1 | tableplus |
| libswiftCoreGraphics.dylib | `_$sSo16CGMutablePathRefa12CoreGraphicsE8addRects_9transformySaySo6CGRectVG_So17CGAffineTransformVtF` | 1 | cleanshot |
| libswiftCoreGraphics.dylib | `_$sSo9CGPathRefa12CoreGraphicsE10intersects_5usingSbAB_AC0A8FillRuleOtF` | 1 | sfm |
| libswiftCoreGraphics.dylib | `_$sSo9CGPathRefa12CoreGraphicsE10normalized5usingAbC0A8FillRuleO_tF` | 1 | cleanshot |
| libswiftCoreGraphics.dylib | `_$sSo9CGPathRefa12CoreGraphicsE11subtracting_5usingA2B_AC0A8FillRuleOtF` | 1 | cleanshot |
