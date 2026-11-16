# Lacunas de Eduardo (Foundation/CoreFoundation)

Símbolos que os apps do corpus importam e o mOS não exporta, na frente de Eduardo (Foundation/CoreFoundation) (`TIME.md`).
Ordenados pelo número de apps que pedem o símbolo. Gerado por `tools/corpus/harness.py donos`.

**615** símbolos distintos.

| biblioteca | símbolo | apps | exemplos |
|---|---|---:|---|
| CoreFoundation.framework | `_CFBundleGetBundleWithIdentifier` | 7 | firefox, iina, krita, musescore |
| CoreFoundation.framework | `_CFDateFormatterGetFormat` | 7 | firefox, krita, musescore, obs |
| CoreFoundation.framework | `_CFLocaleCopyDisplayNameForPropertyValue` | 7 | krita, monitorcontrol, mos, musescore |
| CoreFoundation.framework | `_CFURLEnumeratorGetNextURL` | 7 | blender, firefox, krita, musescore |
| CoreFoundation.framework | `_kCFURLIsPackageKey` | 7 | firefox, krita, musescore, obs |
| Foundation.framework | `_NSRectFromString` | 7 | iina, iterm2, krita, macdown |
| CoreFoundation.framework | `_CFURLCreateBookmarkDataFromFile` | 6 | krita, libreoffice, musescore, obs |
| CoreFoundation.framework | `_CFURLCreateByResolvingBookmarkData` | 6 | krita, libreoffice, musescore, obs |
| CoreFoundation.framework | `_kCFLocaleLanguageCode` | 6 | firefox, musescore, obs, qbittorrent |
| Foundation.framework | `_NSFileTypeForHFSTypeCode` | 6 | alt-tab, iterm2, krita, musescore |
| CoreFoundation.framework | `_CFBundleCopyBuiltInPlugInsURL` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_CFBundleGetPackageInfoInDirectory` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_CFCalendarGetFirstWeekday` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_CFDateFormatterCopyProperty` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_CFNumberFormatterCreateStringWithValue` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_CFNumberFormatterSetProperty` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_CFPreferencesSetMultiple` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_CFSocketCreateWithNative` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_CFSocketDisableCallBacks` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_CFSocketEnableCallBacks` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_CFSocketGetSocketFlags` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_CFStringGetSystemEncoding` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_CFURLEnumeratorCreateForMountedVolumes` | 5 | blender, krita, musescore, obs |
| CoreFoundation.framework | `_kCFDateFormatterAMSymbol` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_kCFDateFormatterMonthSymbols` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_kCFDateFormatterPMSymbol` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_kCFDateFormatterShortMonthSymbols` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_kCFDateFormatterShortStandaloneMonthSymbols` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_kCFDateFormatterShortWeekdaySymbols` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_kCFDateFormatterStandaloneMonthSymbols` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_kCFDateFormatterWeekdaySymbols` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_kCFLocaleAlternateQuotationBeginDelimiterKey` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_kCFLocaleAlternateQuotationEndDelimiterKey` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_kCFLocaleCurrencyCode` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_kCFLocaleCurrencySymbol` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_kCFLocaleDecimalSeparator` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_kCFLocaleGroupingSeparator` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_kCFLocaleMeasurementSystem` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_kCFLocaleQuotationBeginDelimiterKey` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_kCFLocaleQuotationEndDelimiterKey` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_kCFNumberFormatterCurrencySymbol` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_kCFURLIsAliasFileKey` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_kCFURLNameKey` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_kCFURLVolumeURLKey` | 5 | krita, musescore, obs, qbittorrent |
| CoreFoundation.framework | `_CFBundleGetAllBundles` | 4 | krita, musescore, obs, shotcut |
| CoreFoundation.framework | `_kCFDateFormatterShortStandaloneWeekdaySymbols` | 4 | musescore, obs, qbittorrent, shotcut |
| CoreFoundation.framework | `_kCFDateFormatterStandaloneWeekdaySymbols` | 4 | musescore, obs, qbittorrent, shotcut |
| CoreFoundation.framework | `_kCFDateFormatterVeryShortMonthSymbols` | 4 | musescore, obs, qbittorrent, shotcut |
| CoreFoundation.framework | `_kCFDateFormatterVeryShortStandaloneMonthSymbols` | 4 | musescore, obs, qbittorrent, shotcut |
| CoreFoundation.framework | `_kCFDateFormatterVeryShortStandaloneWeekdaySymbols` | 4 | musescore, obs, qbittorrent, shotcut |
| CoreFoundation.framework | `_kCFDateFormatterVeryShortWeekdaySymbols` | 4 | musescore, obs, qbittorrent, shotcut |
| CoreFoundation.framework | `_kCFLocaleScriptCode` | 4 | musescore, obs, qbittorrent, shotcut |
| CoreFoundation.framework | `_kCFURLVolumeSupportsCaseSensitiveNamesKey` | 4 | musescore, obs, qbittorrent, shotcut |
| Foundation.framework | `_NSUndefinedKeyException` | 4 | iina, krita, obs, shotcut |
| CoreFoundation.framework | `_CFBundleCopyExecutableArchitecturesForURL` | 3 | firefox, musescore, thunderbird |
| CoreFoundation.framework | `_CFNumberFormatterCopyProperty` | 3 | musescore, obs, qbittorrent |
| CoreFoundation.framework | `_CFPlugInAddInstanceForFactory` | 3 | krita, libreoffice, transmission |
| CoreFoundation.framework | `_CFPlugInRemoveInstanceForFactory` | 3 | krita, libreoffice, transmission |
| CoreFoundation.framework | `_CFShow` | 3 | musescore, qbittorrent, shotcut |
| CoreFoundation.framework | `_CFURLGetFSRef` | 3 | firefox, thunderbird, vlc |
| CoreFoundation.framework | `_NSLocaleQuotationBeginDelimiterKey` | 3 | iterm2, monitorcontrol, mos |
| CoreFoundation.framework | `_NSLocaleQuotationEndDelimiterKey` | 3 | iterm2, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLFileResourceTypeKey` | 3 | iterm2, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLIsExecutableKey` | 3 | coteditor, monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeSupportsCaseSensitiveNamesKey` | 3 | iterm2, monitorcontrol, mos |
| CoreFoundation.framework | `_kCFNumberFormatterGroupingSize` | 3 | musescore, obs, qbittorrent |
| Foundation.framework | `_NSDecimalCompare` | 3 | iterm2, monitorcontrol, mos |
| Foundation.framework | `_NSStringEncodingErrorKey` | 3 | coteditor, monitorcontrol, mos |
| Foundation.framework | `_NSURLErrorFailingURLPeerTrustErrorKey` | 3 | iterm2, monitorcontrol, mos |
| Foundation.framework | `_OBJC_CLASS_$_NSAppleScript` | 3 | iterm2, obs, vlc |
| Foundation.framework | `_OBJC_CLASS_$_NSConnection` | 3 | firefox, thunderbird, vlc |
| Foundation.framework | `_OBJC_CLASS_$_NSURLDownload` | 3 | iterm2, macdown, vlc |
| CoreData.framework | `_NSAffectedObjectsErrorKey` | 2 | monitorcontrol, mos |
| CoreData.framework | `_NSAffectedStoresErrorKey` | 2 | monitorcontrol, mos |
| CoreData.framework | `_NSPersistentStoreSaveConflictsErrorKey` | 2 | monitorcontrol, mos |
| CoreData.framework | `_NSValidationPredicateErrorKey` | 2 | monitorcontrol, mos |
| CoreData.framework | `_NSValidationValueErrorKey` | 2 | monitorcontrol, mos |
| CoreData.framework | `_OBJC_CLASS_$_NSManagedObjectID` | 2 | monitorcontrol, mos |
| CoreData.framework | `_OBJC_CLASS_$_NSMergeConflict` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_CFArrayExchangeValuesAtIndices` | 2 | krita, shotcut |
| CoreFoundation.framework | `_CFBundleGetDataPointerForName` | 2 | firefox, thunderbird |
| CoreFoundation.framework | `_CFCharacterSetCreateMutableCopy` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_CFCharacterSetCreateWithBitmapRepresentation` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_CFCharacterSetCreateWithCharactersInRange` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_CFCharacterSetHasMemberInPlane` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_CFCharacterSetIntersect` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_CFCharacterSetInvert` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_CFCharacterSetIsSupersetOfSet` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_CFCharacterSetRemoveCharactersInRange` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_CFCharacterSetRemoveCharactersInString` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_CFHashBytes` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_CFStringCompareWithOptionsAndLocale` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_CFStringGetDoubleValue` | 2 | firefox, thunderbird |
| CoreFoundation.framework | `_CFStringGetIntValue` | 2 | firefox, thunderbird |
| CoreFoundation.framework | `_CFStringHashCString` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_CFStringHashNSString` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_CFURLCreateFromFSRef` | 2 | firefox, thunderbird |
| CoreFoundation.framework | `_CFURLCreateFromFileSystemRepresentationRelativeToBase` | 2 | firefox, thunderbird |
| CoreFoundation.framework | `_CFURLEnumeratorCreateForDirectoryURL` | 2 | firefox, thunderbird |
| CoreFoundation.framework | `_NSCalendarIdentifierBuddhist` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSCalendarIdentifierChinese` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSCalendarIdentifierCoptic` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSCalendarIdentifierEthiopicAmeteAlem` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSCalendarIdentifierEthiopicAmeteMihret` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSCalendarIdentifierHebrew` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSCalendarIdentifierISO8601` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSCalendarIdentifierIndian` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSCalendarIdentifierIslamic` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSCalendarIdentifierIslamicCivil` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSCalendarIdentifierJapanese` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSCalendarIdentifierPersian` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSCalendarIdentifierRepublicOfChina` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSLocaleAlternateQuotationBeginDelimiterKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSLocaleAlternateQuotationEndDelimiterKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSLocaleCollationIdentifier` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSLocaleCollatorIdentifier` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSLocaleExemplarCharacterSet` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSLocaleUsesMetricSystem` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLAttributeModificationDateKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLCustomIconKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLIsMountTriggerKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLIsPackageKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLIsSystemImmutableKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLIsUserImmutableKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLLabelColorKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLLabelNumberKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLLocalizedLabelKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLLocalizedTypeDescriptionKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLParentDirectoryURLKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLPreferredIOBlockSizeKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLUbiquitousItemDownloadingErrorKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLUbiquitousItemHasUnresolvedConflictsKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLUbiquitousItemIsUploadedKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLUbiquitousItemIsUploadingKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLUbiquitousItemUploadingErrorKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeCreationDateKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeIsAutomountedKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeIsBrowsableKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeIsEjectableKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeIsJournalingKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeIsReadOnlyKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeIsRemovableKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeLocalizedFormatDescriptionKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeLocalizedNameKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeMaximumFileSizeKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeResourceCountKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeSupportsAdvisoryFileLockingKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeSupportsCasePreservedNamesKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeSupportsExtendedSecurityKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeSupportsHardLinksKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeSupportsJournalingKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeSupportsPersistentIDsKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeSupportsRenamingKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeSupportsRootDirectoryDatesKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeSupportsSparseFilesKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeSupportsSymbolicLinksKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeSupportsVolumeSizesKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeSupportsZeroRunsKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeURLForRemountingKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_NSURLVolumeUUIDStringKey` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_OBJC_CLASS_$_NSFileSecurity` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_kCFLocaleCalendarIdentifier` | 2 | monitorcontrol, mos |
| CoreFoundation.framework | `_kCFLocaleCurrentLocaleDidChangeNotification` | 2 | firefox, thunderbird |
| CoreFoundation.framework | `_kCFURLIsApplicationKey` | 2 | firefox, thunderbird |
| CoreFoundation.framework | `_kCFURLIsHiddenKey` | 2 | krita, shotcut |
| Foundation.framework | `_$s10Foundation12CharacterSetV16lowercaseLettersACvgZ` | 2 | coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation12CharacterSetV16uppercaseLettersACvgZ` | 2 | coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation24FloatingPointFormatStyleVyxGAA09ParseabledE0AAMc` | 2 | coteditor, rectangle |
| Foundation.framework | `_$s10Foundation6LocaleV8LanguageVMa` | 2 | coteditor, rectangle |
| Foundation.framework | `_$s10Foundation6LocaleV8languageAC8LanguageVvg` | 2 | coteditor, rectangle |
| Foundation.framework | `_$s10Foundation8IndexSetV6insert10integersInyx_tSXRzSi5BoundRtzlF` | 2 | coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation8IndexSetV9RangeViewVMn` | 2 | coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation8IndexSetV9formUnionyyACF` | 2 | coteditor, iterm2 |
| Foundation.framework | `_$s10Foundation8IndexSetVSKAAMc` | 2 | coteditor, iterm2 |
| Foundation.framework | `_$sSS10FoundationE8EncodingV17utf16LittleEndianACvgZ` | 2 | coteditor, iterm2 |
| Foundation.framework | `_$sSS10FoundationE8EncodingV5utf16ACvgZ` | 2 | coteditor, iterm2 |
| Foundation.framework | `_$sSS10FoundationE9localized12defaultValue5table6bundle6locale7commentSSs12StaticStringV_SSAAE012LocalizationD0VSSSgSo8NSBundleCSgAA6LocaleVAISgtcfC` | 2 | coteditor, iterm2 |
| Foundation.framework | `_$sSS10FoundationE9localized5table6bundle6locale7commentS2SAAE17LocalizationValueV_SSSgSo8NSBundleCSgAA6LocaleVs12StaticStringVSgtcfC` | 2 | coteditor, rectangle |
| Foundation.framework | `_$sSo8_NSRangeV10FoundationE11descriptionSSvg` | 2 | coteditor, iterm2 |
| Foundation.framework | `_$sSo9NSDecimala10FoundationE11descriptionSSvg` | 2 | coteditor, iterm2 |
| Foundation.framework | `_$sSy10FoundationE12getLineStart_3end11contentsEnd3forySpySS5IndexVG_A2Hqd__tSXRd__AG5BoundRtd__lF` | 2 | coteditor, iterm2 |
| Foundation.framework | `_$sSy10FoundationE14enumerateLines8invokingyySS_Sbztc_tF` | 2 | coteditor, iterm2 |
| Foundation.framework | `_NSCopyMemoryPages` | 2 | monitorcontrol, mos |
| Foundation.framework | `_NSDataDeallocatorFree` | 2 | monitorcontrol, mos |
| Foundation.framework | `_NSDataDeallocatorUnmap` | 2 | monitorcontrol, mos |
| Foundation.framework | `_NSDataDeallocatorVM` | 2 | monitorcontrol, mos |
| Foundation.framework | `_NSDecimalAdd` | 2 | monitorcontrol, mos |
| Foundation.framework | `_NSDecimalCompact` | 2 | monitorcontrol, mos |
| Foundation.framework | `_NSDecimalDivide` | 2 | monitorcontrol, mos |
| Foundation.framework | `_NSDecimalPower` | 2 | monitorcontrol, mos |
| Foundation.framework | `_NSDecimalString` | 2 | monitorcontrol, mos |
| Foundation.framework | `_NSDecimalSubtract` | 2 | monitorcontrol, mos |
| Foundation.framework | `_NSDivideRect` | 2 | iterm2, transmission |
| Foundation.framework | `_NSFileHFSCreatorCode` | 2 | firefox, thunderbird |
| Foundation.framework | `_NSFileHFSTypeCode` | 2 | firefox, thunderbird |
| Foundation.framework | `_NSFileTypeUnknown` | 2 | firefox, thunderbird |
| Foundation.framework | `_NSFoundationVersionNumber` | 2 | monitorcontrol, mos |
| Foundation.framework | `_NSHelpAnchorErrorKey` | 2 | monitorcontrol, mos |
| Foundation.framework | `_NSIsNilTransformerName` | 2 | coteditor, rectangle |
| Foundation.framework | `_NSKeyValueChangeIndexesKey` | 2 | monitorcontrol, mos |
| Foundation.framework | `_NSPageSize` | 2 | monitorcontrol, mos |
| Foundation.framework | `_NSPointFromString` | 2 | iterm2, macdown |
| Foundation.framework | `_NSProgressFileOperationKindKey` | 2 | firefox, thunderbird |
| Foundation.framework | `_NSProgressFileURLKey` | 2 | firefox, thunderbird |
| Foundation.framework | `_NSRoundDownToMultipleOfPageSize` | 2 | monitorcontrol, mos |
| Foundation.framework | `_NSRoundUpToMultipleOfPageSize` | 2 | monitorcontrol, mos |
| Foundation.framework | `_OBJC_CLASS_$_NSFileWrapper` | 2 | coteditor, iterm2 |
| Foundation.framework | `_OBJC_CLASS_$_NSSocketPort` | 2 | firefox, thunderbird |
| Foundation.framework | `_OBJC_CLASS_$_NSSocketPortNameServer` | 2 | firefox, thunderbird |
| Foundation.framework | `_OBJC_CLASS_$_NSURLFileTypeMappings` | 2 | firefox, thunderbird |
| Foundation.framework | `_OBJC_CLASS_$_NSUnit` | 2 | alt-tab, iterm2 |
| Foundation.framework | `_OBJC_CLASS_$_NSUserAppleScriptTask` | 2 | coteditor, iterm2 |
| Foundation.framework | `_OBJC_CLASS_$_NSUserNotificationAction` | 2 | firefox, thunderbird |
| libswiftFoundation.dylib | `_$s10Foundation15AffineTransformV19_bridgeToObjectiveCSo08NSAffineC0CyF` | 2 | alt-tab, iina |
| libswiftFoundation.dylib | `_$s10Foundation15AffineTransformVMa` | 2 | alt-tab, iina |
| libswiftFoundation.dylib | `_$sSy10FoundationE14enumerateLines8invokingyySS_Sbztc_tF` | 2 | iina, stats |
| CoreData.framework | `_NSSQLiteManualVacuumOption` | 1 | iterm2 |
| CoreData.framework | `_OBJC_CLASS_$_NSAttributeDescription` | 1 | alt-tab |
| CoreFoundation.framework | `_CFArrayReplaceValues` | 1 | iterm2 |
| CoreFoundation.framework | `_CFBundleCopyLocalizationsForPreferences` | 1 | vlc |
| CoreFoundation.framework | `_CFBundleCopyPrivateFrameworksURL` | 1 | shotcut |
| CoreFoundation.framework | `_CFBundleLoadExecutableAndReturnError` | 1 | musescore |
| CoreFoundation.framework | `_CFDictionaryContainsValue` | 1 | obs |
| CoreFoundation.framework | `_CFLocaleCopyAvailableLocaleIdentifiers` | 1 | vlc |
| CoreFoundation.framework | `_CFLocaleCreateCanonicalLanguageIdentifierFromString` | 1 | shotcut |
| CoreFoundation.framework | `_CFLocaleCreateCanonicalLocaleIdentifierFromString` | 1 | libreoffice |
| CoreFoundation.framework | `_CFPreferencesCopyMultiple` | 1 | firefox |
| CoreFoundation.framework | `_CFRunLoopTimerDoesRepeat` | 1 | iterm2 |
| CoreFoundation.framework | `_CFStringCreateMutableWithExternalCharactersNoCopy` | 1 | iterm2 |
| CoreFoundation.framework | `_CFStringGetRangeOfComposedCharactersAtIndex` | 1 | iterm2 |
| CoreFoundation.framework | `_CFStringSetExternalCharactersNoCopy` | 1 | iterm2 |
| CoreFoundation.framework | `_NSGregorianCalendar` | 1 | vlc |
| CoreFoundation.framework | `_NSStringTransformFullwidthToHalfwidth` | 1 | coteditor |
| CoreFoundation.framework | `_NSURLFileResourceTypeBlockSpecial` | 1 | iterm2 |
| CoreFoundation.framework | `_NSURLFileResourceTypeCharacterSpecial` | 1 | iterm2 |
| CoreFoundation.framework | `_NSURLFileResourceTypeNamedPipe` | 1 | iterm2 |
| CoreFoundation.framework | `_NSURLFileResourceTypeSocket` | 1 | iterm2 |
| CoreFoundation.framework | `_NSURLUbiquitousItemDownloadingStatusNotDownloaded` | 1 | iterm2 |
| CoreFoundation.framework | `_NSURLVolumeAvailableCapacityForOpportunisticUsageKey` | 1 | obs |
| CoreFoundation.framework | `_kCFStreamSocketSecurityLevelNegotiatedSSL` | 1 | vlc |
| CoreFoundation.framework | `_kCFStreamSocketSecurityLevelSSLv2` | 1 | vlc |
| CoreFoundation.framework | `_kCFStreamSocketSecurityLevelSSLv3` | 1 | vlc |
| CoreFoundation.framework | `_kCFStreamSocketSecurityLevelTLSv1` | 1 | vlc |
| CoreFoundation.framework | `_kCFURLVolumeLocalizedNameKey` | 1 | blender |
| CoreTransferable.framework | `_$s10Foundation3URLV16CoreTransferable0D0ADMc` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation10CocoaErrorV16fileWriteUnknownAC4CodeVvgZ` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation10CocoaErrorV19fileWriteFileExistsAC4CodeVvgZ` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation10CocoaErrorV21fileWriteNoPermissionAC4CodeVvgZ` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation10CocoaErrorV4CodeV23propertyListReadCorruptAEvgZ` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation10CocoaErrorV4CodeV25fileReadUnsupportedSchemeAEvgZ` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation10CocoaErrorV4CodeV29fileReadUnknownStringEncodingAEvgZ` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation10CocoaErrorV4CodeV34fileReadInapplicableStringEncodingAEvgZ` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation10CocoaErrorV5error_8userInfo3urls0C0_pAC4CodeV_SDys11AnyHashableVypGSgAA3URLVSgtFZ` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation10CocoaErrorVs0C0AAMc` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation10URLRequestV14httpBodyStreamSo07NSInputE0CSgvs` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation11FormatStyleMp` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation11FormatStyleP6formaty0B6OutputQz0B5InputQzFTq` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation11FormatStyleP6localeyxAA6LocaleVFTq` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation11FormatStylePA2A09ByteCountbC0VRszrlE04byteE05style12allowedUnits13spellsOutZero014includesActualdE0A2E0C0O_AE0I0VS2btFZ` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation11FormatStylePA2A3URLVABVRszrlE3urlAFvgZ` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation11FormatStylePAAE6localeyxAA6LocaleVF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation11FormatStylePAAs8DurationVAAE05UnitsbC0VRszrlE5units7allowed5width16maximumUnitCount09zeroValueE011valueLength14fractionalPartAGShyAG0J0VG_AG0J5WidthVSiSgAG04ZeromE15DisplayStrategyVAtG010FractionalqtU0VtFZ` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation11FormatStylePSETb` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation11FormatStylePSHTb` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation11FormatStylePSeTb` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation11JSONDecoderC11allowsJSON5SbvsTj` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation11JSONEncoderC16OutputFormattingVSYAAMc` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation11JSONEncoderC16OutputFormattingVs9OptionSetAAMc` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation11JSONEncoderC20DateEncodingStrategyO21millisecondsSince1970yA2EmFWC` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation11JSONEncoderC7Combine15TopLevelEncoderAAMc` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation11MeasurementV19_bridgeToObjectiveCSo13NSMeasurementCyF` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation12CharacterSetVSQAAMc` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation12DataProtocolPA2A15ContiguousBytesRzrlE04copyE02to4fromySryqd__G_qd_0_tSXRd_0_5BoundQyd_0_5IndexSlRtzr0_lF` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation13ParseStrategyP5parsey0B6OutputQz0B5InputQzKFTq` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation13ParseStrategyPSETb` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation13ParseStrategyPSHTb` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation13ParseStrategyPSeTb` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation13__DataStorageC27ensureUniqueBufferReference9growingTo5clearySi_SbtF` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation14DateComponentsVSHAAMc` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation14DateComponentsVs23CustomStringConvertibleAAMc` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation14SortComparatorMp` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation14SortComparatorP5orderAA0B5OrderOvMTq` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation14SortComparatorP5orderAA0B5OrderOvgTq` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation14SortComparatorP5orderAA0B5OrderOvsTq` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation14SortComparatorP7compareySo18NSComparisonResultV8ComparedQz_AHtFTq` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation14SortComparatorPAASSAAE0C0VRszrlE17localizedStandardAEvgZ` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation14SortComparatorPAASSAAE0C0VRszrlE9localizedAEvgZ` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation14SortComparatorPSHTb` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation15AffineTransformV14translationByX3byYAC12CoreGraphics7CGFloatV_AHtcfC` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation15AffineTransformV19_bridgeToObjectiveCSo08NSAffineC0CyF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation15AffineTransformVMa` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation15AttributeScopesO0A10AttributesV024InlinePresentationIntentB0OAA19AttributedStringKeyAAMc` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation15AttributeScopesO0A10AttributesV024InlinePresentationIntentB0OMn` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation15AttributeScopesO0A10AttributesV024InlinePresentationIntentB0ON` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation15AttributeScopesO0A10AttributesV09ByteCountB0O9ComponentO5valueyA2ImFWC` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation15AttributeScopesO0A10AttributesV09ByteCountB0O9ComponentOMa` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation15AttributeScopesO0A10AttributesV09ByteCountB0OAA19AttributedStringKeyAAMc` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation15AttributeScopesO0A10AttributesV09ByteCountB0OMn` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation15AttributeScopesO0A10AttributesV09ByteCountB0ON` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation15AttributeScopesO0A10AttributesV24inlinePresentationIntentAE06InlinefgB0Ovg` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation15AttributeScopesO0A10AttributesV24inlinePresentationIntentAE06InlinefgB0OvpMV` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation15AttributeScopesO0A10AttributesV9byteCountAE04BytefB0Ovg` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation15AttributeScopesO0A10AttributesV9byteCountAE04BytefB0OvpMV` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16AttributedStringV10startIndexAC0E0Vvg` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16AttributedStringV13CharacterViewV10startIndexAC0G0Vvg` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16AttributedStringV13CharacterViewV6_index_8offsetByAC5IndexVAI_SitF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16AttributedStringV13CharacterViewV8endIndexAC0G0Vvg` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16AttributedStringV13CharacterViewV9_distance4from2toSiAC5IndexV_AJtF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16AttributedStringV13CharacterViewVSKAAMc` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16AttributedStringV14removeSubrangeyyxSXRzAC5IndexV5BoundRtzlF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16AttributedStringV22MarkdownParsingOptionsV17InterpretedSyntaxO4fullyA2GmFWC` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16AttributedStringV4RunsV3RunV10attributesAA18AttributeContainerVvg` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16AttributedStringV4RunsVSTAAMc` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16AttributedStringV6insert_2atyx_AC5IndexVtAA0bC8ProtocolRzlF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16AttributedStringV8markdown7options7baseURLACSS_AC22MarkdownParsingOptionsVAA0G0VSgtKcfC` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16AttributedStringVyAA0B9SubstringVxcSXRzAC5IndexV5BoundRtzluiM` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16AttributedStringVyACSo012NSAttributedC0CcfC` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16RecoverableErrorMp` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16RecoverableErrorP15attemptRecovery11optionIndex13resultHandlerySi_ySbctFTq` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16RecoverableErrorP15attemptRecovery11optionIndexSbSi_tFTq` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16RecoverableErrorP15recoveryOptionsSaySSGvgTq` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16RecoverableErrorPAAE15attemptRecovery11optionIndex13resultHandlerySi_ySbctF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation16RecoverableErrorPs0C0Tb` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation17KeyPathComparatorV_10comparatorACyxGs0bC0Cyxqd__G_qd_0_tc8ComparedQyd_0_Rsd__AA04SortD0Rd_0_r0_lufC` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation17KeyPathComparatorV_5orderACyxGs0bC0Cyxqd__SgG_AA9SortOrderOtcSLRd__lufC` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation17KeyPathComparatorVyxGAA04SortD0AAMc` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation17KeyPathComparatorVyxGSQAAMc` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV10isWritableSbSgvg` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV12isExecutableSbSgvg` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV16fileResourceTypeSo09NSURLFileeF0aSgvg` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation17URLResourceValuesV32volumeSupportsCaseSensitiveNamesSbSgvg` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation18AttributeContainerV13dynamicMemberAC7BuilderVy_xGs7KeyPathCyAA0B13DynamicLookupOxG_tcAA016AttributedStringG0Rzluig` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation18AttributeContainerV13dynamicMemberAC7BuilderVy_xGs7KeyPathCyAA0B13DynamicLookupOxG_tcAA016AttributedStringG0RzluigZ` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation18AttributeContainerV7BuilderV14callAsFunctionyAC5ValueQzF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation18AttributeContainerV7BuilderVMn` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation18IntegerFormatStyleV7PercentV6localeAEyx_GAA6LocaleV_tcfC` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation18IntegerFormatStyleV7PercentVMn` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation18IntegerFormatStyleV7PercentVyx_GAA0cD0AAMc` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation19AttributedSubstringVy5ValueQzSgxmcAA0B9StringKeyRzluis` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation20ByteCountFormatStyleV0E0O4fileyA2EmFWC` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation20ByteCountFormatStyleV0E0O6binaryyA2EmFWC` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation20ByteCountFormatStyleV0E0OMa` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation20ByteCountFormatStyleV10AttributedVAA0dE0AAMc` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation20ByteCountFormatStyleV10AttributedVMa` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation20ByteCountFormatStyleV10attributedAC10AttributedVvg` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation20ByteCountFormatStyleV5UnitsV3allAEvgZ` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation20ByteCountFormatStyleV5UnitsV5bytesAEvgZ` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation20ByteCountFormatStyleV5UnitsVMa` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation20ByteCountFormatStyleVAA0dE0AAMc` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation20ByteCountFormatStyleVMa` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation20ParseableFormatStyleMp` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation20ParseableFormatStyleP13parseStrategy0F0QzvgTq` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation20ParseableFormatStyleP8StrategyAC_AA05ParseE0Tn` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation20ParseableFormatStylePAA0cD0Tb` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation23LocalizedStringResourceV17BundleDescriptionO4mainyA2EmFWC` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation23LocalizedStringResourceV17BundleDescriptionO5atURLyAeA0H0VcAEmFWC` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation23LocalizedStringResourceV17BundleDescriptionO8forClassyAEyXlXpcAEmFWC` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation23LocalizedStringResourceV17BundleDescriptionOMa` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation23LocalizedStringResourceV19stringInterpolationACSSAAE17LocalizationValueV0cF0V_tcfC` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation23LocalizedStringResourceV6localeAA6LocaleVvs` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation23LocalizedStringResourceVAA06CustombcD11ConvertibleAAWP` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation23LocalizedStringResourceVMa` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation23LocalizedStringResourceVMn` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation23LocalizedStringResourceVSQAAMc` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation23LocalizedStringResourceV_12defaultValue5table6locale6bundle7commentACs06StaticC0V_SSAAE012LocalizationF0VSSSgAA6LocaleVAC17BundleDescriptionOAJSgtcfC` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation23LocalizedStringResourceV_5table6locale6bundle7commentACSSAAE17LocalizationValueV_SSSgAA6LocaleVAC17BundleDescriptionOs06StaticC0VSgtcfC` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation24AttributedStringProtocolPAAE17settingAttributesyAA0bC0VAA18AttributeContainerVF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation24AttributedStringProtocolPAAE19replacingAttributes_4withAA0bC0VAA18AttributeContainerV_AItF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation24FloatingPointFormatStyleV13parseStrategyAA0bc5ParseG0VyACyxGGvg` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation24FloatingPointFormatStyleV6localeyACyxGAA6LocaleVF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation24FloatingPointFormatStyleV7PercentVyx_GAA09ParseabledE0AAMc` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation26DecodableWithConfigurationP4from13configurationxs7Decoder_p_08DecodingD0QztKcfCTj` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation26DecodableWithConfigurationTL` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation26FloatingPointParseStrategyV5parsey11FormatInputQzSSKF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation26FloatingPointParseStrategyVMn` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation30DecodingConfigurationProvidingP08decodingC00bC0QzvgZTj` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation30NumberFormatStyleConfigurationO9PrecisionV14fractionLengthyAExSXRzSi5BoundRtzlFZ` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation3URLV11FormatStyleV22ComponentDisplayOptionV5neverAGvgZ` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation3URLV11FormatStyleV22ComponentDisplayOptionVMa` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation3URLV11FormatStyleV6schemeyA2E22ComponentDisplayOptionVF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation3URLV11FormatStyleVAadAMc` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation3URLV11FormatStyleVMa` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation3URLV12bookmarkData7options30includingResourceValuesForKeys10relativeToAA0D0VSo28NSURLBookmarkCreationOptionsV_ShySo16NSURLResourceKeyaGSgACSgtKF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation3URLV15fileURLWithPath10relativeToACSSh_ACSghtcfC` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation3URLV17lastPathComponentSSvpMV` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation3URLV20applicationDirectoryACvgZ` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation3URLV3for2in14appropriateFor6createACSo21NSSearchPathDirectoryV_So0hI10DomainMaskVACSgSbtKcfC` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation3URLV9appending10components13directoryHintACxd_AC09DirectoryF0OtSyRzlF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation3URLV9formattedy12FormatOutputQzxAA0D5StyleRzAC0D5InputRtzlF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation4DataV10LargeSliceV6append10contentsOfySW_tF` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation4DataV11InlineSliceV6append10contentsOfySW_tF` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation4DataV16_copyBytesHelper2to4fromySv_SnySiGtF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation4DataV34_conditionallyBridgeFromObjectiveC_6resultSbSo6NSDataC_ACSgztFZ` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation4DataV8advanced2byACSi_tF` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation4DateV18ISO8601FormatStyleV0B13TimeSeparatorO5spaceyA2GmFWC` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation4DateV18ISO8601FormatStyleV17dateTimeSeparatoryA2E0bgH0OF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation6LocaleV12LanguageCodeV13stringLiteralAESS_tcfC` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation6LocaleV12LanguageCodeVMa` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation6LocaleV12LanguageCodeVMn` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation6LocaleV12LanguageCodeVSQAAMc` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation6LocaleV12languageCode6script0C6RegionA2C08LanguageD0VSg_AC6ScriptVSgAC0F0VSgtcfC` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation6LocaleV6ScriptV13stringLiteralAESS_tcfC` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation6LocaleV6ScriptVMa` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation6LocaleV6ScriptVMn` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation6LocaleV8LanguageV12languageCodeAC0cE0VSgvg` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation6LocaleV8LanguageV17maximalIdentifierSSvg` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation6LocaleV8LanguageV18characterDirectionSo08NSLocalecE0Vvg` | 1 | rectangle |
| Foundation.framework | `_$s10Foundation8IndexSetV10integersInACx_tcSXRzSi5BoundRtzlufC` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8IndexSetV10intersects10integersInSbx_tSXRzSi5BoundRtzlF` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8IndexSetV18integerGreaterThanySiSgSiF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation8IndexSetV4lastSiSgvg` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation8IndexSetV5count2inSiSnySiG_tF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation8IndexSetV5count2inSix_tSXRzSi5BoundRtzlF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation8IndexSetV5unionyA2CF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation8IndexSetV6remove10integersInySNySiG_tF` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8IndexSetV9RangeViewV12makeIterators08IndexingG0VyAEGyF` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8IndexSetV9RangeViewVSKAAMc` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation8IndexSetV9rangeView2ofAC05RangeE0VSnySiG_tF` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8IndexSetVs23CustomStringConvertibleAAMc` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8URLErrorV25clientCertificateRejectedAC4CodeVvgZ` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8URLErrorV25clientCertificateRequiredAC4CodeVvgZ` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8URLErrorV26serverCertificateUntrustedAC4CodeVvgZ` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8URLErrorV27serverCertificateHasBadDateAC4CodeVvgZ` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8URLErrorV28serverCertificateNotYetValidAC4CodeVvgZ` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8URLErrorV31serverCertificateHasUnknownRootAC4CodeVvgZ` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV14dataNotAllowedAEvgZ` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV14unsupportedURLAEvgZ` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV19resourceUnavailableAEvgZ` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV23internationalRoamingOffAEvgZ` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV25clientCertificateRejectedAEvgZ` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV25clientCertificateRequiredAEvgZ` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV26serverCertificateUntrustedAEvgZ` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV27serverCertificateHasBadDateAEvgZ` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV28serverCertificateNotYetValidAEvgZ` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation8URLErrorV4CodeV31serverCertificateHasUnknownRootAEvgZ` | 1 | iterm2 |
| Foundation.framework | `_$s10Foundation9SortOrderO4hash4intoys6HasherVz_tF` | 1 | coteditor |
| Foundation.framework | `_$s10Foundation9SortOrderOMn` | 1 | coteditor |
| Foundation.framework | `_$s10ParseInput10Foundation0A8StrategyPTl` | 1 | coteditor |
| Foundation.framework | `_$s11FormatInput10Foundation0A5StylePTl` | 1 | coteditor |
| Foundation.framework | `_$s11ParseOutput10Foundation0A8StrategyPTl` | 1 | coteditor |
| Foundation.framework | `_$s12FormatOutput10Foundation0A5StylePTl` | 1 | coteditor |
| Foundation.framework | `_$s21DecodingConfiguration10Foundation013DecodableWithB0PTl` | 1 | coteditor |
| Foundation.framework | `_$s7SubjectSo20NSNotificationCenterC10FoundationE16MainActorMessagePTl` | 1 | coteditor |
| Foundation.framework | `_$s8Compared10Foundation14SortComparatorPTl` | 1 | coteditor |
| Foundation.framework | `_$s8Dispatch0A4DataV10FoundationE6RegionVAD0B8ProtocolADMc` | 1 | iterm2 |
| Foundation.framework | `_$s8Dispatch0A4DataV10FoundationE6RegionVMa` | 1 | iterm2 |
| Foundation.framework | `_$s8Dispatch0A4DataV10FoundationE6RegionVSlADMc` | 1 | iterm2 |
| Foundation.framework | `_$s8Dispatch0A4DataV10FoundationE7regionsSayAcDE6RegionVGvg` | 1 | iterm2 |
| Foundation.framework | `_$s8Strategy10Foundation20ParseableFormatStylePTl` | 1 | coteditor |
| Foundation.framework | `_$sSB10FoundationE9formattedSSyF` | 1 | coteditor |
| Foundation.framework | `_$sSM10FoundationSkRzrlE4sort5usingyqd___tAA14SortComparatorRd__8ComparedQyd__7ElementSTRtzlF` | 1 | coteditor |
| Foundation.framework | `_$sSM10FoundationSkRzrlE4sort5usingyqd___tSTRd__AA14SortComparatorRd_0_7ElementQyd__Rsd_0_8ComparedQyd_0_AESTRtzr0_lF` | 1 | coteditor |
| Foundation.framework | `_$sSS10FoundationE10ComparatorVAA04SortB0AAMc` | 1 | coteditor |
| Foundation.framework | `_$sSS10FoundationE10ComparatorVMa` | 1 | coteditor |
| Foundation.framework | `_$sSS10FoundationE11_charactersSSAA16AttributedStringV13CharacterViewV_tcfC` | 1 | coteditor |
| Foundation.framework | `_$sSS10FoundationE13localizedName2ofS2SAAE8EncodingV_tFZ` | 1 | coteditor |
| Foundation.framework | `_$sSS10FoundationE17LocalizationValueV19StringInterpolationV06appendE0_6formatyx_q_t11FormatInputQy_RszAA0H5StyleR_Sy0H6OutputRp_r0_lF` | 1 | coteditor |
| Foundation.framework | `_$sSS10FoundationE17LocalizationValueV19StringInterpolationV06appendE0yyxAA015CustomLocalizedD19ResourceConvertibleRzlF` | 1 | coteditor |
| Foundation.framework | `_$sSS10FoundationE18StandardComparatorV09localizedB0ACvgZ` | 1 | coteditor |
| Foundation.framework | `_$sSS10FoundationE18StandardComparatorVMa` | 1 | coteditor |
| Foundation.framework | `_$sSS10FoundationE24availableStringEncodingsSaySSAAE8EncodingVGvgZ` | 1 | coteditor |
| Foundation.framework | `_$sSS10FoundationE8EncodingV14utf16BigEndianACvgZ` | 1 | coteditor |
| Foundation.framework | `_$sSS10FoundationE8EncodingV14utf32BigEndianACvgZ` | 1 | coteditor |
| Foundation.framework | `_$sSS10FoundationE8EncodingV17utf32LittleEndianACvgZ` | 1 | coteditor |
| Foundation.framework | `_$sSS10FoundationE8EncodingV5utf32ACvgZ` | 1 | coteditor |
| Foundation.framework | `_$sSS10FoundationE8EncodingV8rawValueSuvpMV` | 1 | coteditor |
| Foundation.framework | `_$sSS10FoundationE8EncodingV8rawValueSuvs` | 1 | coteditor |
| Foundation.framework | `_$sSS10FoundationE8EncodingVSHAAMc` | 1 | coteditor |
| Foundation.framework | `_$sSS10FoundationE9localizedSSAA23LocalizedStringResourceV_tcfC` | 1 | coteditor |
| Foundation.framework | `_$sST10FoundationE6sorted5usingSay7ElementQzGqd___tAA14SortComparatorRd__8ComparedQyd__AERSlF` | 1 | coteditor |
| Foundation.framework | `_$sST10FoundationE6sorted5usingSay7ElementQzGqd___tSTRd__AA14SortComparatorRd_0_ADQyd__Rsd_0_8ComparedQyd_0_AERSr0_lF` | 1 | coteditor |
| Foundation.framework | `_$sSd10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo8NSNumberC_SdSgztFZ` | 1 | iterm2 |
| Foundation.framework | `_$sSf10FoundationE26_forceBridgeFromObjectiveC_6resultySo8NSNumberC_SfSgztFZ` | 1 | coteditor |
| Foundation.framework | `_$sSf10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo8NSNumberC_SfSgztFZ` | 1 | coteditor |
| Foundation.framework | `_$sSf10FoundationE36_unconditionallyBridgeFromObjectiveCySfSo8NSNumberCSgFZ` | 1 | coteditor |
| Foundation.framework | `_$sSh10FoundationE26_forceBridgeFromObjectiveC_6resultySo5NSSetC_ShyxGSgztFZ` | 1 | iterm2 |
| Foundation.framework | `_$sSi10FoundationE7exactlySiSgSo8NSNumberCh_tcfC` | 1 | iterm2 |
| Foundation.framework | `_$sSn10FoundationAA16AttributedStringV5IndexVRszrlE_2inSnyAEGSgSo8_NSRangeV_qd__tcAA0bC8ProtocolRd__lufC` | 1 | coteditor |
| Foundation.framework | `_$sSn10FoundationAA16AttributedStringV5IndexVRszrlE_2inSnyAEGSgqd___qd_0_tcSXRd__AA0bC8ProtocolRd_0_SSADV5BoundRtd__r0_lufC` | 1 | coteditor |
| Foundation.framework | `_$sSo12NSDictionaryC10FoundationE8IteratorCMa` | 1 | coteditor |
| Foundation.framework | `_$sSo12NSDictionaryC10FoundationE8IteratorCStACMc` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE11addObserver2of3for5usingAbCE16ObservationTokenV7SubjectQy__xyq_ScMYcctAbCE17MessageIdentifierRzAbCE09MainActorL0R_0L4TypeQzRs_AKRLCr0_lF` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE11addObserver2of3for5usingAbCE16ObservationTokenV7SubjectQzSg_xmyxScMYcctAbCE16MainActorMessageRzAKRLClF` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE14removeObserveryyAbCE16ObservationTokenVF` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE16MainActorMessageMp` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE16MainActorMessageP04makeF0yxSgAC12NotificationVFZTq` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE16MainActorMessageP16makeNotificationyAC0H0VxFZTq` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE16MainActorMessageP4nameSo0A4NameavgZTq` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE16MainActorMessagePACE04makeF0yxSgAC12NotificationVFZ` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE16MainActorMessagePACE16makeNotificationyAC0H0VxFZ` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE16MainActorMessagePACE4nameSo0A4NameavgZ` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE16ObservationTokenVMa` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE16ObservationTokenVMn` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE16ObservationTokenVSHACMc` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE16ObservationTokenVSQACMc` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE21BaseMessageIdentifierVAEy_xGycAbCE09MainActorE0RzlufC` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE21BaseMessageIdentifierVMn` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE21BaseMessageIdentifierVy_xGAbCE0eF0ACMc` | 1 | coteditor |
| Foundation.framework | `_$sSo20NSNotificationCenterC10FoundationE4post_7subjectyx_7SubjectQztAbCE16MainActorMessageRzAGRLClF` | 1 | coteditor |
| Foundation.framework | `_$sSo6NSDataC10FoundationE10startIndexSivg` | 1 | iterm2 |
| Foundation.framework | `_$sSo6NSDataC10FoundationE8endIndexSivg` | 1 | iterm2 |
| Foundation.framework | `_$sSo8NSObjectCs28CustomDebugStringConvertible10FoundationMc` | 1 | iterm2 |
| Foundation.framework | `_$sSo8_NSRangeV10FoundationE19_bridgeToObjectiveCSo7NSValueCyF` | 1 | coteditor |
| Foundation.framework | `_$sSo8_NSRangeV10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo7NSValueC_ABSgztFZ` | 1 | coteditor |
| Foundation.framework | `_$sSo9NSDecimala10FoundationE11FormatStyleV8CurrencyVAcdCMc` | 1 | coteditor |
| Foundation.framework | `_$sSo9NSDecimalaSQ10FoundationMc` | 1 | coteditor |
| Foundation.framework | `_$sSo9NSRunLoopC10FoundationE17SchedulerTimeTypeV6StrideV14integerLiteralAGSd_tcfC` | 1 | coteditor |
| Foundation.framework | `_$sSs10FoundationE19_bridgeToObjectiveCSo8NSStringCyF` | 1 | coteditor |
| Foundation.framework | `_$sSy10FoundationE14canBeConverted2toSbSSAAE8EncodingV_tF` | 1 | coteditor |
| Foundation.framework | `_$sSy10FoundationE18localizedLowercaseSSvpMV` | 1 | coteditor |
| Foundation.framework | `_$sSy10FoundationE18localizedUppercaseSSvpMV` | 1 | coteditor |
| Foundation.framework | `_$sSy10FoundationE37precomposedStringWithCanonicalMappingSSvpMV` | 1 | coteditor |
| Foundation.framework | `_$sSy10FoundationE40decomposedStringWithCompatibilityMappingSSvg` | 1 | coteditor |
| Foundation.framework | `_$sSy10FoundationE5write6toFile10atomically8encodingyqd___SbSSAAE8EncodingVtKSyRd__lF` | 1 | iterm2 |
| Foundation.framework | `_$sSy10FoundationE9lineRange3forSnySS5IndexVGqd___tSXRd__AE5BoundRtd__lF` | 1 | coteditor |
| Foundation.framework | `_$sSz10FoundationE9formattedSSyF` | 1 | coteditor |
| Foundation.framework | `_$sSz10FoundationE9formattedy12FormatOutputQyd__qd__AA0C5StyleRd__Sz0C5InputRpd__lF` | 1 | coteditor |
| Foundation.framework | `_$ss11AnyHashableV10FoundationE36_unconditionallyBridgeFromObjectiveCyABSo8NSObjectCSgFZ` | 1 | iterm2 |
| Foundation.framework | `_$ss5Int32V10Foundation18_FormatSpecifiableACWP` | 1 | iterm2 |
| Foundation.framework | `_$ss5Int32V10FoundationE10truncatingABSo8NSNumberCh_tcfC` | 1 | rectangle |
| Foundation.framework | `_$ss5UInt8V10FoundationE10truncatingABSo8NSNumberCh_tcfC` | 1 | iterm2 |
| Foundation.framework | `_$ss8DurationV10FoundationE15TimeFormatStyleV7PatternV12minuteSecondAGvgZ` | 1 | coteditor |
| Foundation.framework | `_$ss8DurationV10FoundationE15TimeFormatStyleV7PatternV16hourMinuteSecondAGvgZ` | 1 | coteditor |
| Foundation.framework | `_$ss8DurationV10FoundationE16UnitsFormatStyleV09ZeroValueC15DisplayStrategyV4hideAGvgZ` | 1 | coteditor |
| Foundation.framework | `_$ss8DurationV10FoundationE16UnitsFormatStyleV09ZeroValueC15DisplayStrategyVMa` | 1 | coteditor |
| Foundation.framework | `_$ss8DurationV10FoundationE16UnitsFormatStyleV29FractionalPartDisplayStrategyV4show6length7rounded9incrementAGSi_s25FloatingPointRoundingRuleOSdSgtFZ` | 1 | coteditor |
| Foundation.framework | `_$ss8DurationV10FoundationE16UnitsFormatStyleV29FractionalPartDisplayStrategyVMa` | 1 | coteditor |
| Foundation.framework | `_$ss8DurationV10FoundationE16UnitsFormatStyleV4UnitV7secondsAGvgZ` | 1 | coteditor |
| Foundation.framework | `_$ss8DurationV10FoundationE16UnitsFormatStyleV4UnitVMa` | 1 | coteditor |
| Foundation.framework | `_$ss8DurationV10FoundationE16UnitsFormatStyleV4UnitVMn` | 1 | coteditor |
| Foundation.framework | `_$ss8DurationV10FoundationE16UnitsFormatStyleV4UnitVSHACMc` | 1 | coteditor |
| Foundation.framework | `_$ss8DurationV10FoundationE16UnitsFormatStyleV4UnitVSQACMc` | 1 | coteditor |
| Foundation.framework | `_$ss8DurationV10FoundationE16UnitsFormatStyleV9UnitWidthV4wideAGvgZ` | 1 | coteditor |
| Foundation.framework | `_$ss8DurationV10FoundationE16UnitsFormatStyleV9UnitWidthVMa` | 1 | coteditor |
| Foundation.framework | `_$ss8DurationV10FoundationE16UnitsFormatStyleVAC0dE0ACMc` | 1 | coteditor |
| Foundation.framework | `_$ss8DurationV10FoundationE16UnitsFormatStyleVMa` | 1 | coteditor |
| Foundation.framework | `_NSAppleScriptErrorBriefMessage` | 1 | iterm2 |
| Foundation.framework | `_NSAppleScriptErrorMessage` | 1 | obs |
| Foundation.framework | `_NSAppleScriptErrorNumber` | 1 | iterm2 |
| Foundation.framework | `_NSConnectionDidDieNotification` | 1 | vlc |
| Foundation.framework | `_NSDefaultMallocZone` | 1 | macdown |
| Foundation.framework | `_NSFileHandleReadToEndOfFileCompletionNotification` | 1 | macdown |
| Foundation.framework | `_NSFileOwnerAccountName` | 1 | coteditor |
| Foundation.framework | `_NSGlobalDomain` | 1 | vlc |
| Foundation.framework | `_NSHFSTypeCodeFromFileType` | 1 | iterm2 |
| Foundation.framework | `_NSHTTPCookieOriginURL` | 1 | iina |
| Foundation.framework | `_NSKeyedUnarchiveFromDataTransformerName` | 1 | rectangle |
| Foundation.framework | `_NSLocalizedFailureErrorKey` | 1 | coteditor |
| Foundation.framework | `_NSRegistrationDomain` | 1 | coteditor |
| Foundation.framework | `_NSSecureUnarchiveFromDataTransformerName` | 1 | rectangle |
| Foundation.framework | `_NSSizeFromString` | 1 | iterm2 |
| Foundation.framework | `_NSStringEncodingDetectionAllowLossyKey` | 1 | transmission |
| Foundation.framework | `_NSStringEncodingDetectionLikelyLanguageKey` | 1 | transmission |
| Foundation.framework | `_OBJC_CLASS_$_NSBackgroundActivityScheduler` | 1 | stats |
| Foundation.framework | `_OBJC_CLASS_$_NSClassDescription` | 1 | iterm2 |
| Foundation.framework | `_OBJC_CLASS_$_NSIndexSpecifier` | 1 | iterm2 |
| Foundation.framework | `_OBJC_CLASS_$_NSPortMessage` | 1 | obs |
| Foundation.framework | `_OBJC_CLASS_$_NSPropertySpecifier` | 1 | coteditor |
| Foundation.framework | `_OBJC_CLASS_$_NSScriptSuiteRegistry` | 1 | iterm2 |
| Foundation.framework | `_OBJC_CLASS_$_NSUnarchiver` | 1 | iina |
| Foundation.framework | `_OBJC_CLASS_$_NSUnitTemperature` | 1 | stats |
| Foundation.framework | `_OBJC_CLASS_$_NSUserUnixTask` | 1 | coteditor |
| libswiftFoundation.dylib | `_$s10Foundation10CocoaErrorV13userCancelledAC4CodeVvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation10CocoaErrorV5error_8userInfo3urls0C0_pAC4CodeV_SDys11AnyHashableVypGSgAA3URLVSgtFZ` | 1 | krita |
| libswiftFoundation.dylib | `_$s10Foundation11JSONEncoderC34NonConformingFloatEncodingStrategyO15convertToStringyAESS_S2StcAEmFWC` | 1 | alt-tab |
| libswiftFoundation.dylib | `_$s10Foundation11MeasurementV19_bridgeToObjectiveCSo13NSMeasurementCyF` | 1 | alt-tab |
| libswiftFoundation.dylib | `_$s10Foundation11MeasurementVAASo11NSDimensionCRbzrlE7convert2toyx_tF` | 1 | stats |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV14urlUserAllowedACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV16lowercaseLettersACvgZ` | 1 | alt-tab |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV16uppercaseLettersACvgZ` | 1 | alt-tab |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV18urlFragmentAllowedACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation12CharacterSetV18urlPasswordAllowedACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation15AffineTransformV14translationByX3byYAC12CoreGraphics7CGFloatV_AHtcfC` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation15AffineTransformV3m113m123m213m222tX2tYAC12CoreGraphics7CGFloatV_A5LtcfC` | 1 | alt-tab |
| libswiftFoundation.dylib | `_$s10Foundation17URLResourceValuesV13volumeIsLocalSbSgvg` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation20ReferenceConvertibleTL` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation22kCFStringEncodingASCIIs6UInt32Vvg` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation25NSFastEnumerationIteratorVMn` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation25NSFastEnumerationIteratorVyACSo0bC0_pcfC` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation3URLV12bookmarkData7options30includingResourceValuesForKeys10relativeToAA0D0VSo28NSURLBookmarkCreationOptionsV_ShySo16NSURLResourceKeyaGSgACSgtKF` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation3URLV21resolveSymlinksInPathyyF` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation4DataV8advanced2byACSi_tF` | 1 | stats |
| libswiftFoundation.dylib | `_$s10Foundation6LocaleV16debugDescriptionSSvg` | 1 | alt-tab |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV10ordinality2of2in3forSiSgAC9ComponentO_AjA4DateVtF` | 1 | stats |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV22standaloneMonthSymbolsSaySSGvg` | 1 | stats |
| libswiftFoundation.dylib | `_$s10Foundation8CalendarV9ComponentO3erayA2EmFWC` | 1 | stats |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV05startB0AC0B0Vvg` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetV10integersInACx_tcSXRzSi5BoundRtzlufC` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetVs21_ObjectiveCBridgeableAAMc` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation8IndexSetVySiAC0B0Vcig` | 1 | iina |
| libswiftFoundation.dylib | `_$s10Foundation8TimeZoneV05knownbC11IdentifiersSaySSGvgZ` | 1 | stats |
| libswiftFoundation.dylib | `_$s13ReferenceType10Foundation0A11ConvertiblePTl` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV11japaneseEUCACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV13windowsCP1250ACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV13windowsCP1251ACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV13windowsCP1252ACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV13windowsCP1253ACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV13windowsCP1254ACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV14utf16BigEndianACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV14utf32BigEndianACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV17utf16LittleEndianACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV17utf32LittleEndianACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV5utf16ACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV5utf32ACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV6symbolACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV8nextstepACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV8shiftJISACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV9iso2022JPACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSS10FoundationE8EncodingV9isoLatin2ACvgZ` | 1 | iina |
| libswiftFoundation.dylib | `_$sSf10FoundationE26_forceBridgeFromObjectiveC_6resultySo8NSNumberC_SfSgztFZ` | 1 | alt-tab |
| libswiftFoundation.dylib | `_$sSf10FoundationE34_conditionallyBridgeFromObjectiveC_6resultSbSo8NSNumberC_SfSgztFZ` | 1 | alt-tab |
| libswiftFoundation.dylib | `_$sSf10FoundationE36_unconditionallyBridgeFromObjectiveCySfSo8NSNumberCSgFZ` | 1 | alt-tab |
| libswiftFoundation.dylib | `_$sSy10FoundationE25localizedStandardContainsySbqd__SyRd__lF` | 1 | iina |
| libswiftFoundation.dylib | `_$sSy10FoundationE5write6toFile10atomically8encodingyqd___SbSSAAE8EncodingVtKSyRd__lF` | 1 | iina |
| self) | `_CFBundleGetBundleWithIdentifier` | 1 | libreoffice |
