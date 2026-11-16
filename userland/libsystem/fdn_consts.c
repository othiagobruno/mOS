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
