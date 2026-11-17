/*
 * mOS libSystem — constantes NSString/CFString da Foundation que eram ponteiro nulo em sys_zerados.c,
 * e o __CFStringMakeConstantString (o CFSTR() de quem compila sem constantes de compilador).
 *
 * Os valores são os que o macOS publica (são as chaves que o app compara ou grava em plist):
 * as chaves de NSLocale são as de CFLocale ("locale:id", "currency", "kCFLocale...Key"),
 * as chaves de NSURL valem o próprio nome, as transformações ICU usam o identificador do ICU.
 * kCFNetwork* e kCFStream* são da frente de rede (Patricia) e ficam fora daqui.
 *
 * __CFStringMakeConstantString: o mesmo texto devolve sempre o mesmo objeto, imortal (isa de
 * constante, como os CFSTR do compilador), então retain/release não o liberam.
 */
#ifdef FDN_CONSTS_HOST
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
typedef uint64_t u64;
char cx_cfconst_isa[16];
#else
#include "sys.h"
#include "libc.h"
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
#endif

typedef struct { void *isa; u64 flags; const char *s; u64 n; } fc_const_t;
#define FC(var, text) static fc_const_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; const void *var = &var##_s;

/* NSLocale: chaves de -objectForKey: (iguais às kCFLocale*Key). */
FC(NSLocaleIdentifier, "locale:id")
FC(NSLocaleCountryCode, "kCFLocaleCountryCodeKey")
FC(NSLocaleScriptCode, "kCFLocaleScriptCodeKey")
FC(NSLocaleVariantCode, "kCFLocaleVariantCodeKey")
FC(NSLocaleCalendar, "kCFLocaleCalendarKey")
FC(NSLocaleCurrencyCode, "currency")
FC(NSLocaleCurrencySymbol, "kCFLocaleCurrencySymbolKey")

/* Exceções e atributos. */
FC(NSMallocException, "NSMallocException")
FC(NSPresentationIntentAttributeName, "NSPresentationIntent")
FC(NSStreamFileCurrentOffsetKey, "kCFStreamPropertyFileCurrentOffset")

/* Transformações de -stringByApplyingTransform:reverse: (identificadores do ICU). */
FC(NSStringTransformLatinToArabic, "Latin-Arabic")
FC(NSStringTransformToLatin, "Any-Latin")
FC(kCFStringTransformToLatin, "Any-Latin")

/* Chaves de recurso de NSURL / CFURL. */
FC(NSURLContentAccessDateKey, "NSURLContentAccessDateKey")
FC(NSURLFileAllocatedSizeKey, "NSURLFileAllocatedSizeKey")
FC(NSURLFileResourceIdentifierKey, "NSURLFileResourceIdentifierKey")
FC(NSURLIsHiddenKey, "NSURLIsHiddenKey")
FC(NSURLIsUbiquitousItemKey, "NSURLIsUbiquitousItemKey")
FC(NSURLLinkCountKey, "NSURLLinkCountKey")
FC(NSURLTotalFileAllocatedSizeKey, "NSURLTotalFileAllocatedSizeKey")
FC(NSURLTotalFileSizeKey, "NSURLTotalFileSizeKey")
FC(NSURLUbiquitousItemDownloadingStatusKey, "NSURLUbiquitousItemDownloadingStatusKey")
FC(NSURLUbiquitousItemDownloadingStatusCurrent, "NSURLUbiquitousItemDownloadingStatusCurrent")
FC(NSURLUbiquitousItemDownloadingStatusDownloaded, "NSURLUbiquitousItemDownloadingStatusDownloaded")
FC(NSURLUbiquitousItemIsDownloadingKey, "NSURLUbiquitousItemIsDownloadingKey")
FC(NSURLVolumeAvailableCapacityKey, "NSURLVolumeAvailableCapacityKey")
FC(NSURLVolumeAvailableCapacityForImportantUsageKey, "NSURLVolumeAvailableCapacityForImportantUsageKey")
FC(NSURLVolumeTotalCapacityKey, "NSURLVolumeTotalCapacityKey")
FC(kCFURLContentAccessDateKey, "NSURLContentAccessDateKey")
FC(kCFURLFileSizeKey, "NSURLFileSizeKey")

/* CFError: a chave da razão da falha (a mesma do NSLocalizedFailureReasonErrorKey). */
FC(kCFErrorLocalizedFailureReasonKey, "NSLocalizedFailureReason")

/* chaves de CFLocaleGetValue/-objectForKey: que faltavam (mesmos valores que fdn_i18n.c reconhece) */
FC(kCFLocaleLanguageCode, "kCFLocaleLanguageCodeKey")
FC(kCFLocaleScriptCode, "kCFLocaleScriptCodeKey")
FC(kCFLocaleVariantCode, "kCFLocaleVariantCodeKey")
FC(kCFLocaleExemplarCharacterSet, "kCFLocaleExemplarCharacterSetKey")
FC(NSLocaleExemplarCharacterSet, "kCFLocaleExemplarCharacterSetKey")
FC(kCFLocaleCalendarIdentifier, "calendar")
FC(kCFLocaleCalendar, "kCFLocaleCalendarKey")
FC(kCFLocaleCollationIdentifier, "collation")
FC(NSLocaleCollationIdentifier, "collation")
FC(kCFLocaleUsesMetricSystem, "kCFLocaleUsesMetricSystemKey")
FC(NSLocaleUsesMetricSystem, "kCFLocaleUsesMetricSystemKey")
FC(kCFLocaleMeasurementSystem, "kCFLocaleMeasurementSystemKey")
FC(NSLocaleMeasurementSystem, "kCFLocaleMeasurementSystemKey")
FC(kCFLocaleDecimalSeparator, "kCFLocaleDecimalSeparatorKey")
FC(kCFLocaleGroupingSeparator, "kCFLocaleGroupingSeparatorKey")
FC(kCFLocaleCurrencySymbol, "kCFLocaleCurrencySymbolKey")
FC(kCFLocaleCurrencyCode, "currency")
FC(kCFLocaleCollatorIdentifier, "kCFLocaleCollatorIdentifierKey")
FC(NSLocaleCollatorIdentifier, "kCFLocaleCollatorIdentifierKey")
FC(kCFLocaleQuotationBeginDelimiterKey, "kCFLocaleQuotationBeginDelimiterKey")
FC(NSLocaleQuotationBeginDelimiterKey, "kCFLocaleQuotationBeginDelimiterKey")
FC(kCFLocaleQuotationEndDelimiterKey, "kCFLocaleQuotationEndDelimiterKey")
FC(NSLocaleQuotationEndDelimiterKey, "kCFLocaleQuotationEndDelimiterKey")
FC(kCFLocaleAlternateQuotationBeginDelimiterKey, "kCFLocaleAlternateQuotationBeginDelimiterKey")
FC(NSLocaleAlternateQuotationBeginDelimiterKey, "kCFLocaleAlternateQuotationBeginDelimiterKey")
FC(kCFLocaleAlternateQuotationEndDelimiterKey, "kCFLocaleAlternateQuotationEndDelimiterKey")
FC(NSLocaleAlternateQuotationEndDelimiterKey, "kCFLocaleAlternateQuotationEndDelimiterKey")
FC(kCFLocaleCurrentLocaleDidChangeNotification, "kCFLocaleCurrentLocaleDidChangeNotification")
FC(kCFTimeZoneSystemTimeZoneDidChangeNotification, "kCFTimeZoneSystemTimeZoneDidChangeNotification")
FC(NSCalendarDayChangedNotification, "NSCalendarDayChangedNotification")
/* identificadores de calendário (os da ICU) */
FC(kCFGregorianCalendar, "gregorian")
FC(kCFBuddhistCalendar, "buddhist")
FC(NSCalendarIdentifierBuddhist, "buddhist")
FC(kCFChineseCalendar, "chinese")
FC(NSCalendarIdentifierChinese, "chinese")
FC(kCFHebrewCalendar, "hebrew")
FC(NSCalendarIdentifierHebrew, "hebrew")
FC(kCFIslamicCalendar, "islamic")
FC(NSCalendarIdentifierIslamic, "islamic")
FC(kCFIslamicCivilCalendar, "islamic-civil")
FC(NSCalendarIdentifierIslamicCivil, "islamic-civil")
FC(kCFJapaneseCalendar, "japanese")
FC(NSCalendarIdentifierJapanese, "japanese")
FC(kCFRepublicOfChinaCalendar, "roc")
FC(NSCalendarIdentifierRepublicOfChina, "roc")
FC(kCFPersianCalendar, "persian")
FC(NSCalendarIdentifierPersian, "persian")
FC(kCFIndianCalendar, "indian")
FC(NSCalendarIdentifierIndian, "indian")
FC(kCFISO8601Calendar, "iso8601")
FC(NSCalendarIdentifierISO8601, "iso8601")
FC(kCFIslamicTabularCalendar, "islamic-tbla")
FC(NSCalendarIdentifierIslamicTabular, "islamic-tbla")
FC(kCFIslamicUmmAlQuraCalendar, "islamic-umalqura")
FC(NSCalendarIdentifierIslamicUmmAlQura, "islamic-umalqura")
FC(kCFCopticCalendar, "coptic")
FC(NSCalendarIdentifierCoptic, "coptic")
FC(kCFEthiopicAmeteMihretCalendar, "ethiopic")
FC(NSCalendarIdentifierEthiopicAmeteMihret, "ethiopic")
FC(kCFEthiopicAmeteAlemCalendar, "ethiopic-amete-alem")
FC(NSCalendarIdentifierEthiopicAmeteAlem, "ethiopic-amete-alem")
FC(NSGregorianCalendar, "gregorian")
/* chaves de recurso de CFURL (valem o nome NSURL...Key) */
FC(kCFURLIsPackageKey, "NSURLIsPackageKey")
FC(kCFURLNameKey, "NSURLNameKey")
FC(kCFURLVolumeURLKey, "NSURLVolumeURLKey")
FC(kCFURLIsAliasFileKey, "NSURLIsAliasFileKey")
FC(kCFURLIsDirectoryKey, "NSURLIsDirectoryKey")
FC(kCFURLIsSymbolicLinkKey, "NSURLIsSymbolicLinkKey")
FC(kCFURLIsHiddenKey, "NSURLIsHiddenKey")
FC(kCFURLContentModificationDateKey, "NSURLContentModificationDateKey")
FC(kCFURLCreationDateKey, "NSURLCreationDateKey")
FC(kCFURLIsRegularFileKey, "NSURLIsRegularFileKey")
FC(kCFURLTypeIdentifierKey, "NSURLTypeIdentifierKey")
FC(kCFURLParentDirectoryURLKey, "NSURLParentDirectoryURLKey")
FC(kCFURLFileResourceTypeKey, "NSURLFileResourceTypeKey")
FC(kCFURLIsVolumeKey, "NSURLIsVolumeKey")
FC(kCFURLPathKey, "NSURLPathKey")
FC(kCFURLIsReadableKey, "NSURLIsReadableKey")
FC(kCFURLIsWritableKey, "NSURLIsWritableKey")
FC(kCFURLIsExecutableKey, "NSURLIsExecutableKey")
FC(kCFURLFileAllocatedSizeKey, "NSURLFileAllocatedSizeKey")
FC(kCFURLTotalFileSizeKey, "NSURLTotalFileSizeKey")
FC(kCFURLAttributeModificationDateKey, "NSURLAttributeModificationDateKey")
FC(kCFURLLinkCountKey, "NSURLLinkCountKey")
/* chaves NSURL*, tipos de recurso, erros, transformers e notificações que o corpus pedia */
FC(kCFURLVolumeSupportsCaseSensitiveNamesKey, "NSURLVolumeSupportsCaseSensitiveNamesKey")
FC(kCFURLIsApplicationKey, "NSURLIsApplicationKey")
FC(kCFURLVolumeLocalizedNameKey, "NSURLVolumeLocalizedNameKey")
FC(NSURLVolumeSupportsCaseSensitiveNamesKey, "NSURLVolumeSupportsCaseSensitiveNamesKey")
FC(NSURLErrorFailingURLPeerTrustErrorKey, "NSURLErrorFailingURLPeerTrustErrorKey")
FC(NSURLCustomIconKey, "NSURLCustomIconKey")
FC(NSURLIsMountTriggerKey, "NSURLIsMountTriggerKey")
FC(NSURLLabelNumberKey, "NSURLLabelNumberKey")
FC(NSURLLocalizedLabelKey, "NSURLLocalizedLabelKey")
FC(NSURLLocalizedTypeDescriptionKey, "NSURLLocalizedTypeDescriptionKey")
FC(NSURLPreferredIOBlockSizeKey, "NSURLPreferredIOBlockSizeKey")
FC(NSURLUbiquitousItemDownloadingErrorKey, "NSURLUbiquitousItemDownloadingErrorKey")
FC(NSURLUbiquitousItemHasUnresolvedConflictsKey, "NSURLUbiquitousItemHasUnresolvedConflictsKey")
FC(NSURLUbiquitousItemIsUploadedKey, "NSURLUbiquitousItemIsUploadedKey")
FC(NSURLUbiquitousItemIsUploadingKey, "NSURLUbiquitousItemIsUploadingKey")
FC(NSURLUbiquitousItemUploadingErrorKey, "NSURLUbiquitousItemUploadingErrorKey")
FC(NSURLVolumeCreationDateKey, "NSURLVolumeCreationDateKey")
FC(NSURLVolumeIsAutomountedKey, "NSURLVolumeIsAutomountedKey")
FC(NSURLVolumeIsJournalingKey, "NSURLVolumeIsJournalingKey")
FC(NSURLVolumeLocalizedFormatDescriptionKey, "NSURLVolumeLocalizedFormatDescriptionKey")
FC(NSURLVolumeLocalizedNameKey, "NSURLVolumeLocalizedNameKey")
FC(NSURLVolumeMaximumFileSizeKey, "NSURLVolumeMaximumFileSizeKey")
FC(NSURLVolumeResourceCountKey, "NSURLVolumeResourceCountKey")
FC(NSURLVolumeSupportsAdvisoryFileLockingKey, "NSURLVolumeSupportsAdvisoryFileLockingKey")
FC(NSURLVolumeSupportsCasePreservedNamesKey, "NSURLVolumeSupportsCasePreservedNamesKey")
FC(NSURLVolumeSupportsExtendedSecurityKey, "NSURLVolumeSupportsExtendedSecurityKey")
FC(NSURLVolumeSupportsHardLinksKey, "NSURLVolumeSupportsHardLinksKey")
FC(NSURLVolumeSupportsJournalingKey, "NSURLVolumeSupportsJournalingKey")
FC(NSURLVolumeSupportsPersistentIDsKey, "NSURLVolumeSupportsPersistentIDsKey")
FC(NSURLVolumeSupportsRenamingKey, "NSURLVolumeSupportsRenamingKey")
FC(NSURLVolumeSupportsRootDirectoryDatesKey, "NSURLVolumeSupportsRootDirectoryDatesKey")
FC(NSURLVolumeSupportsSparseFilesKey, "NSURLVolumeSupportsSparseFilesKey")
FC(NSURLVolumeSupportsSymbolicLinksKey, "NSURLVolumeSupportsSymbolicLinksKey")
FC(NSURLVolumeSupportsVolumeSizesKey, "NSURLVolumeSupportsVolumeSizesKey")
FC(NSURLVolumeSupportsZeroRunsKey, "NSURLVolumeSupportsZeroRunsKey")
FC(NSURLVolumeURLForRemountingKey, "NSURLVolumeURLForRemountingKey")
FC(NSURLVolumeUUIDStringKey, "NSURLVolumeUUIDStringKey")
FC(NSURLVolumeAvailableCapacityForOpportunisticUsageKey, "NSURLVolumeAvailableCapacityForOpportunisticUsageKey")
FC(NSURLFileResourceTypeBlockSpecial, "NSURLFileResourceTypeBlockSpecial")
FC(NSURLFileResourceTypeCharacterSpecial, "NSURLFileResourceTypeCharacterSpecial")
FC(NSURLFileResourceTypeNamedPipe, "NSURLFileResourceTypeNamedPipe")
FC(NSURLFileResourceTypeSocket, "NSURLFileResourceTypeSocket")
FC(NSURLUbiquitousItemDownloadingStatusNotDownloaded, "NSURLUbiquitousItemDownloadingStatusNotDownloaded")
FC(NSProgressFileOperationKindKey, "NSProgressFileOperationKindKey")
FC(NSProgressFileURLKey, "NSProgressFileURLKey")
FC(NSRegistrationDomain, "NSRegistrationDomain")
FC(NSConnectionDidDieNotification, "NSConnectionDidDieNotification")
FC(NSFileHandleReadToEndOfFileCompletionNotification, "NSFileHandleReadToEndOfFileCompletionNotification")
FC(NSStringEncodingDetectionAllowLossyKey, "NSStringEncodingDetectionAllowLossyKey")
FC(NSStringEncodingDetectionLikelyLanguageKey, "NSStringEncodingDetectionLikelyLanguageKey")
FC(NSUndefinedKeyException, "NSUnknownKeyException")
FC(NSStringEncodingErrorKey, "NSStringEncoding")
FC(NSHelpAnchorErrorKey, "NSHelpAnchor")
FC(NSLocalizedFailureErrorKey, "NSLocalizedFailure")
FC(NSIsNilTransformerName, "NSIsNil")
FC(NSKeyedUnarchiveFromDataTransformerName, "NSKeyedUnarchiveFromData")
FC(NSSecureUnarchiveFromDataTransformerName, "NSSecureUnarchiveFromData")
FC(NSKeyValueChangeIndexesKey, "indexes")
FC(NSStringTransformFullwidthToHalfwidth, "Fullwidth-Halfwidth")

/* lacunas do corpus (docs/lacunas/eduardo.md): NSURL, NSUserDefaults, notificações, cookies, CoreData,
 * transformações (regras da ICU), autenticação, CFUserNotification; valores do macOS */
FC(NSURLIsPackageKey, "NSURLIsPackageKey")
FC(NSURLVolumeIsReadOnlyKey, "NSURLVolumeIsReadOnlyKey")
FC(NSURLVolumeIsRemovableKey, "NSURLVolumeIsRemovableKey")
FC(NSURLVolumeIsEjectableKey, "NSURLVolumeIsEjectableKey")
FC(NSURLIsExecutableKey, "NSURLIsExecutableKey")
FC(NSURLFileScheme, "file")
FC(NSURLAttributeModificationDateKey, "NSURLAttributeModificationDateKey")
FC(NSURLParentDirectoryURLKey, "NSURLParentDirectoryURLKey")
FC(NSURLFileResourceTypeKey, "NSURLFileResourceTypeKey")
FC(NSURLIsSystemImmutableKey, "NSURLIsSystemImmutableKey")
FC(NSURLIsUserImmutableKey, "NSURLIsUserImmutableKey")
FC(NSURLVolumeIsBrowsableKey, "NSURLVolumeIsBrowsableKey")
FC(NSURLLabelColorKey, "NSURLLabelColorKey")
FC(NSURLIsApplicationKey, "NSURLIsApplicationKey")
FC(NSURLAddedToDirectoryDateKey, "NSURLAddedToDirectoryDateKey")
FC(NSURLVolumeIsRootFileSystemKey, "NSURLVolumeIsRootFileSystemKey")
FC(NSURLMayHaveExtendedAttributesKey, "NSURLMayHaveExtendedAttributesKey")
FC(NSURLUbiquitousItemContainerDisplayNameKey, "NSURLUbiquitousItemContainerDisplayNameKey")
FC(NSURLFileResourceTypeDirectory, "NSURLFileResourceTypeDirectory")
FC(NSURLFileResourceTypeSymbolicLink, "NSURLFileResourceTypeSymbolicLink")
FC(NSURLFileResourceTypeRegular, "NSURLFileResourceTypeRegular")
FC(kCFURLIsSystemImmutableKey, "NSURLIsSystemImmutableKey")
FC(kCFURLIsUserImmutableKey, "NSURLIsUserImmutableKey")
FC(kCFURLFileResourceIdentifierKey, "NSURLFileResourceIdentifierKey")
FC(kCFURLVolumeUUIDStringKey, "NSURLVolumeUUIDStringKey")
FC(NSArgumentDomain, "NSArgumentDomain")
FC(NSGlobalDomain, "NSGlobalDomain")
FC(NSFileHandleDataAvailableNotification, "NSFileHandleDataAvailableNotification")
FC(NSFileHandleConnectionAcceptedNotification, "NSFileHandleConnectionAcceptedNotification")
FC(NSFileHandleNotificationFileHandleItem, "NSFileHandleNotificationFileHandleItem")
FC(NSTaskDidTerminateNotification, "NSTaskDidTerminateNotification")
FC(NSThreadWillExitNotification, "NSThreadWillExitNotification")
FC(NSPortDidBecomeInvalidNotification, "NSPortDidBecomeInvalidNotification")
FC(NSAppleEventManagerWillProcessFirstEventNotification, "NSAppleEventManagerWillProcessFirstEventNotification")
FC(NSHTTPCookieMaximumAge, "Max-Age")
FC(NSHTTPCookieOriginURL, "OriginURL")
FC(NSHTTPCookieComment, "Comment")
FC(NSHTTPCookieCommentURL, "CommentURL")
FC(NSHTTPCookieDiscard, "Discard")
FC(NSHTTPCookiePort, "Port")
FC(NSHTTPCookieSameSitePolicy, "SameSite")
FC(NSHTTPCookieSameSiteLax, "lax")
FC(NSHTTPCookieSameSiteStrict, "strict")
FC(NSPersistentStoreSaveConflictsErrorKey, "conflictList")
FC(NSAffectedObjectsErrorKey, "NSAffectedObjectsErrorKey")
FC(NSAffectedStoresErrorKey, "NSAffectedStoresErrorKey")
FC(NSValidationPredicateErrorKey, "NSValidationErrorPredicate")
FC(NSValidationValueErrorKey, "NSValidationErrorValue")
FC(NSInvalidatedObjectsKey, "invalidatedObjects")
FC(NSInvalidatedAllObjectsKey, "invalidatedAll")
FC(NSStoreTypeKey, "NSStoreType")
FC(NSSQLiteManualVacuumOption, "NSSQLiteManualVacuumOption")
FC(NSProgressThroughputKey, "NSProgressThroughputKey")
FC(NSProgressFileOperationKindCopying, "NSProgressFileOperationKindCopying")
FC(NSProgressFileOperationKindReceiving, "NSProgressFileOperationKindReceiving")
FC(NSStringTransformStripDiacritics, "NFD; [:M:] Remove; NFC")
FC(NSStringTransformStripCombiningMarks, "NFD; [:Mn:] Remove; NFC")
FC(kCFStringTransformStripDiacritics, "NFD; [:M:] Remove; NFC")
FC(kCFStringTransformStripCombiningMarks, "NFD; [:Mn:] Remove; NFC")
FC(kCFStringTransformMandarinLatin, "Han-Latin")
FC(kCFStringTransformHiraganaKatakana, "Hiragana-Katakana")
FC(kCFStringTransformLatinHiragana, "Latin-Hiragana")
FC(kCFStringTransformToUnicodeName, "Any-Name")
FC(NSInlinePresentationIntentAttributeName, "NSInlinePresentationIntent")
FC(NSURLAuthenticationMethodDefault, "NSURLAuthenticationMethodDefault")
FC(NSURLAuthenticationMethodHTMLForm, "NSURLAuthenticationMethodHTMLForm")
FC(NSURLProtectionSpaceHTTPProxy, "http")
FC(NSURLProtectionSpaceHTTPSProxy, "https")
FC(NSURLProtectionSpaceFTPProxy, "ftp")
FC(NSURLProtectionSpaceSOCKSProxy, "SOCKS")
FC(NSURLErrorBackgroundTaskCancelledReasonKey, "NSURLErrorBackgroundTaskCancelledReasonKey")
FC(NSNetServicesErrorCode, "NSNetServicesErrorCode")
FC(NSNetServicesErrorDomain, "NSNetServicesErrorDomain")
FC(NSItemProviderErrorDomain, "NSItemProviderErrorDomain")
FC(NSInvalidArchiveOperationException, "NSInvalidArchiveOperationException")
FC(NSStackTraceKey, "NSStackTraceKey")
FC(NSStringEncodingDetectionSuggestedEncodingsKey, "NSStringEncodingDetectionSuggestedEncodingsKey")
FC(NSStringEncodingDetectionUseOnlySuggestedEncodingsKey, "NSStringEncodingDetectionUseOnlySuggestedEncodingsKey")
FC(kCFErrorDomainMach, "NSMachErrorDomain")
FC(kCFUserNotificationAlertHeaderKey, "AlertHeader")
FC(kCFUserNotificationAlertMessageKey, "AlertMessage")
FC(kCFUserNotificationDefaultButtonTitleKey, "DefaultButtonTitle")
FC(kCFUserNotificationAlternateButtonTitleKey, "AlternateButtonTitle")
FC(kCFUserNotificationOtherButtonTitleKey, "OtherButtonTitle")
FC(kCFUserNotificationIconURLKey, "IconURL")
FC(kCFUserNotificationCheckBoxTitlesKey, "CheckBoxTitles")
FC(kCFUserNotificationTextFieldTitlesKey, "TextFieldTitles")
FC(kCFUserNotificationTextFieldValuesKey, "TextFieldValues")
#ifndef FDN_CONSTS_HOST
const float NSURLSessionTaskPriorityDefault = 0.5f, NSURLSessionTaskPriorityLow = 0.0f;
const double kCFAbsoluteTimeIntervalSince1904 = 3061152000.0;
#endif

/* ------------------------------------------------------------ __CFStringMakeConstantString */

#define FC_BUCKETS 512
typedef struct fc_node { struct fc_node *next; u64 hash; fc_const_t str; } fc_node;
static fc_node *fc_table[FC_BUCKETS];
static volatile int fc_lock;

static u64 fc_hash(const char *s, u64 n) {
    u64 h = 1469598103934665603ULL;
    for (u64 i = 0; i < n; i++) { h ^= (unsigned char)s[i]; h *= 1099511628211ULL; }
    return h;
}

const void *__CFStringMakeConstantString(const char *cstr) {
    u64 n, h;
    fc_node *e;
    if (!cstr) return 0;
    n = strlen(cstr);
    h = fc_hash(cstr, n);
    while (__atomic_exchange_n(&fc_lock, 1, __ATOMIC_ACQUIRE)) { }
    for (e = fc_table[h % FC_BUCKETS]; e; e = e->next)
        if (e->hash == h && e->str.n == n && !memcmp(e->str.s, cstr, n)) break;
    if (!e) {
        char *copy = malloc(n + 1);
        e = malloc(sizeof *e);
        if (!copy || !e) { free(copy); free(e); __atomic_store_n(&fc_lock, 0, __ATOMIC_RELEASE); return 0; }
        memcpy(copy, cstr, n + 1);
        e->hash = h;
        e->str.isa = cx_cfconst_isa;
        e->str.flags = 0x7c8;
        e->str.s = copy;
        e->str.n = n;
        e->next = fc_table[h % FC_BUCKETS];
        fc_table[h % FC_BUCKETS] = e;
    }
    __atomic_store_n(&fc_lock, 0, __ATOMIC_RELEASE);
    return &e->str;
}
