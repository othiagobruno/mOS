/*
 * mOS libSystem — a ICU (libicucore) vista pela Foundation.
 *
 * A Foundation do macOS faz locale, calendário, fuso, formatters, collation e transliteração em cima da ICU.
 * Aqui é igual: a libicucore.A.dylib do disco (tools/build-icu.sh, ICU4C 74.2) é aberta na primeira vez que
 * alguém precisa dela, e cada função é resolvida por dlsym. No teste de host (FDN_ICU_HOST) a mesma tabela
 * resolve a ICU do Linux, cujos nomes levam o sufixo da versão (uloc_getLanguage_78).
 *
 * Só a API C estável da ICU (os cabeçalhos unicode/). Os tipos opacos ficam como void.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>

typedef uint16_t UChar;
typedef int32_t UChar32;
typedef int UErrorCode;
typedef int8_t UBool;
typedef double UDate;
typedef struct { int32_t line, offset; UChar preContext[16], postContext[16]; } UParseError;
typedef struct { int32_t field, beginIndex, endIndex; } UFieldPosition;

#define U_ZERO_ERROR 0
#define U_BUFFER_OVERFLOW_ERROR 15
#define U_FAILURE(e) ((e) > 0)

/* UCalendarDateFields */
enum { UCAL_ERA, UCAL_YEAR, UCAL_MONTH, UCAL_WEEK_OF_YEAR, UCAL_WEEK_OF_MONTH, UCAL_DATE, UCAL_DAY_OF_YEAR,
       UCAL_DAY_OF_WEEK, UCAL_DAY_OF_WEEK_IN_MONTH, UCAL_AM_PM, UCAL_HOUR, UCAL_HOUR_OF_DAY, UCAL_MINUTE,
       UCAL_SECOND, UCAL_MILLISECOND, UCAL_ZONE_OFFSET, UCAL_DST_OFFSET, UCAL_YEAR_WOY, UCAL_DOW_LOCAL,
       UCAL_EXTENDED_YEAR, UCAL_JULIAN_DAY, UCAL_MILLISECONDS_IN_DAY, UCAL_IS_LEAP_MONTH, UCAL_ORDINAL_MONTH };
enum { UCAL_LENIENT, UCAL_FIRST_DAY_OF_WEEK, UCAL_MINIMAL_DAYS_IN_FIRST_WEEK };
enum { UCAL_MINIMUM, UCAL_MAXIMUM, UCAL_GREATEST_MINIMUM, UCAL_LEAST_MAXIMUM, UCAL_ACTUAL_MINIMUM, UCAL_ACTUAL_MAXIMUM };
enum { UCAL_STANDARD, UCAL_SHORT_STANDARD, UCAL_DST, UCAL_SHORT_DST };
enum { UCAL_TZ_TRANSITION_NEXT, UCAL_TZ_TRANSITION_NEXT_INCLUSIVE, UCAL_TZ_TRANSITION_PREVIOUS,
       UCAL_TZ_TRANSITION_PREVIOUS_INCLUSIVE };
/* UDateFormatStyle */
enum { UDAT_FULL = 0, UDAT_LONG = 1, UDAT_MEDIUM = 2, UDAT_SHORT = 3, UDAT_RELATIVE = 128, UDAT_NONE = -1, UDAT_PATTERN = -2 };

/* X(nome, retorno, (parâmetros)) — a ordem não importa; o nome é o da ICU sem sufixo */
#define FDN_ICU_FUNCS(X) \
    X(u_errorName, const char *, (UErrorCode)) \
    X(uloc_getDefault, const char *, (void)) \
    X(uloc_setDefault, void, (const char *, UErrorCode *)) \
    X(uloc_getName, int32_t, (const char *, char *, int32_t, UErrorCode *)) \
    X(uloc_getBaseName, int32_t, (const char *, char *, int32_t, UErrorCode *)) \
    X(uloc_canonicalize, int32_t, (const char *, char *, int32_t, UErrorCode *)) \
    X(uloc_getLanguage, int32_t, (const char *, char *, int32_t, UErrorCode *)) \
    X(uloc_getScript, int32_t, (const char *, char *, int32_t, UErrorCode *)) \
    X(uloc_getCountry, int32_t, (const char *, char *, int32_t, UErrorCode *)) \
    X(uloc_getVariant, int32_t, (const char *, char *, int32_t, UErrorCode *)) \
    X(uloc_getKeywordValue, int32_t, (const char *, const char *, char *, int32_t, UErrorCode *)) \
    X(uloc_setKeywordValue, int32_t, (const char *, const char *, char *, int32_t, UErrorCode *)) \
    X(uloc_getDisplayName, int32_t, (const char *, const char *, UChar *, int32_t, UErrorCode *)) \
    X(uloc_getDisplayLanguage, int32_t, (const char *, const char *, UChar *, int32_t, UErrorCode *)) \
    X(uloc_getDisplayCountry, int32_t, (const char *, const char *, UChar *, int32_t, UErrorCode *)) \
    X(uloc_getDisplayScript, int32_t, (const char *, const char *, UChar *, int32_t, UErrorCode *)) \
    X(uloc_getDisplayVariant, int32_t, (const char *, const char *, UChar *, int32_t, UErrorCode *)) \
    X(uloc_getDisplayKeywordValue, int32_t, (const char *, const char *, const char *, UChar *, int32_t, UErrorCode *)) \
    X(uloc_countAvailable, int32_t, (void)) \
    X(uloc_getAvailable, const char *, (int32_t)) \
    X(uloc_getISOLanguages, const char *const *, (void)) \
    X(uloc_getISOCountries, const char *const *, (void)) \
    X(uloc_toLanguageTag, int32_t, (const char *, char *, int32_t, UBool, UErrorCode *)) \
    X(uloc_forLanguageTag, int32_t, (const char *, char *, int32_t, int32_t *, UErrorCode *)) \
    X(uloc_addLikelySubtags, int32_t, (const char *, char *, int32_t, UErrorCode *)) \
    X(uloc_minimizeSubtags, int32_t, (const char *, char *, int32_t, UErrorCode *)) \
    X(uloc_getCharacterOrientation, int32_t, (const char *, UErrorCode *)) \
    X(uloc_getLineOrientation, int32_t, (const char *, UErrorCode *)) \
    X(ulocdata_open, void *, (const char *, UErrorCode *)) \
    X(ulocdata_close, void, (void *)) \
    X(ulocdata_getDelimiter, int32_t, (void *, int32_t, UChar *, int32_t, UErrorCode *)) \
    X(ulocdata_getMeasurementSystem, int32_t, (const char *, UErrorCode *)) \
    X(ulocdata_getExemplarSet, void *, (void *, void *, uint32_t, int32_t, UErrorCode *)) \
    X(uset_openPattern, void *, (const UChar *, int32_t, UErrorCode *)) \
    X(uset_close, void, (void *)) \
    X(uset_getItemCount, int32_t, (const void *)) \
    X(uset_getItem, int32_t, (const void *, int32_t, UChar32 *, UChar32 *, UChar *, int32_t, UErrorCode *)) \
    X(ucurr_forLocale, int32_t, (const char *, UChar *, int32_t, UErrorCode *)) \
    X(ucurr_getName, const UChar *, (const UChar *, const char *, int32_t, UBool *, int32_t *, UErrorCode *)) \
    X(ucurr_openISOCurrencies, void *, (uint32_t, UErrorCode *)) \
    X(ucurr_getDefaultFractionDigits, int32_t, (const UChar *, UErrorCode *)) \
    X(uenum_next, const char *, (void *, int32_t *, UErrorCode *)) \
    X(uenum_count, int32_t, (void *, UErrorCode *)) \
    X(uenum_close, void, (void *)) \
    X(ucal_open, void *, (const UChar *, int32_t, const char *, int32_t, UErrorCode *)) \
    X(ucal_clone, void *, (const void *, UErrorCode *)) \
    X(ucal_close, void, (void *)) \
    X(ucal_getType, const char *, (const void *, UErrorCode *)) \
    X(ucal_setMillis, void, (void *, UDate, UErrorCode *)) \
    X(ucal_getMillis, UDate, (const void *, UErrorCode *)) \
    X(ucal_get, int32_t, (const void *, int32_t, UErrorCode *)) \
    X(ucal_set, void, (void *, int32_t, int32_t)) \
    X(ucal_clear, void, (void *)) \
    X(ucal_add, void, (void *, int32_t, int32_t, UErrorCode *)) \
    X(ucal_roll, void, (void *, int32_t, int32_t, UErrorCode *)) \
    X(ucal_getAttribute, int32_t, (const void *, int32_t)) \
    X(ucal_setAttribute, void, (void *, int32_t, int32_t)) \
    X(ucal_getLimit, int32_t, (const void *, int32_t, int32_t, UErrorCode *)) \
    X(ucal_getFieldDifference, int32_t, (void *, UDate, int32_t, UErrorCode *)) \
    X(ucal_setTimeZone, void, (void *, const UChar *, int32_t, UErrorCode *)) \
    X(ucal_getTimeZoneDisplayName, int32_t, (const void *, int32_t, const char *, UChar *, int32_t, UErrorCode *)) \
    X(ucal_inDaylightTime, UBool, (const void *, UErrorCode *)) \
    X(ucal_openTimeZones, void *, (UErrorCode *)) \
    X(ucal_getDefaultTimeZone, int32_t, (UChar *, int32_t, UErrorCode *)) \
    X(ucal_setDefaultTimeZone, void, (const UChar *, UErrorCode *)) \
    X(ucal_getCanonicalTimeZoneID, int32_t, (const UChar *, int32_t, UChar *, int32_t, UBool *, UErrorCode *)) \
    X(ucal_getTimeZoneTransitionDate, UBool, (const void *, int32_t, UDate *, UErrorCode *)) \
    X(ucal_getDSTSavings, int32_t, (const UChar *, UErrorCode *)) \
    X(ucal_getTZDataVersion, const char *, (UErrorCode *)) \
    X(ucal_getNow, UDate, (void)) \
    X(ucal_getDayOfWeekType, int32_t, (const void *, int32_t, UErrorCode *)) \
    X(udat_open, void *, (int32_t, int32_t, const char *, const UChar *, int32_t, const UChar *, int32_t, UErrorCode *)) \
    X(udat_close, void, (void *)) \
    X(udat_format, int32_t, (const void *, UDate, UChar *, int32_t, UFieldPosition *, UErrorCode *)) \
    X(udat_parse, UDate, (const void *, const UChar *, int32_t, int32_t *, UErrorCode *)) \
    X(udat_toPattern, int32_t, (const void *, UBool, UChar *, int32_t, UErrorCode *)) \
    X(udat_applyPattern, void, (void *, UBool, const UChar *, int32_t)) \
    X(udat_setLenient, void, (void *, UBool)) \
    X(udat_setCalendar, void, (void *, const void *)) \
    X(udat_set2DigitYearStart, void, (void *, UDate, UErrorCode *)) \
    X(udat_getSymbols, int32_t, (const void *, int32_t, int32_t, UChar *, int32_t, UErrorCode *)) \
    X(udat_countSymbols, int32_t, (const void *, int32_t)) \
    X(udat_setSymbols, void, (void *, int32_t, int32_t, UChar *, int32_t, UErrorCode *)) \
    X(udat_setContext, void, (void *, int32_t, UErrorCode *)) \
    X(udatpg_open, void *, (const char *, UErrorCode *)) \
    X(udatpg_close, void, (void *)) \
    X(udatpg_getBestPattern, int32_t, (void *, const UChar *, int32_t, UChar *, int32_t, UErrorCode *)) \
    X(unum_open, void *, (int32_t, const UChar *, int32_t, const char *, UParseError *, UErrorCode *)) \
    X(unum_close, void, (void *)) \
    X(unum_formatDouble, int32_t, (const void *, double, UChar *, int32_t, UFieldPosition *, UErrorCode *)) \
    X(unum_formatInt64, int32_t, (const void *, int64_t, UChar *, int32_t, UFieldPosition *, UErrorCode *)) \
    X(unum_formatDecimal, int32_t, (const void *, const char *, int32_t, UChar *, int32_t, UFieldPosition *, UErrorCode *)) \
    X(unum_parseDouble, double, (const void *, const UChar *, int32_t, int32_t *, UErrorCode *)) \
    X(unum_parseInt64, int64_t, (const void *, const UChar *, int32_t, int32_t *, UErrorCode *)) \
    X(unum_getAttribute, int32_t, (const void *, int32_t)) \
    X(unum_setAttribute, void, (void *, int32_t, int32_t)) \
    X(unum_getDoubleAttribute, double, (const void *, int32_t)) \
    X(unum_setDoubleAttribute, void, (void *, int32_t, double)) \
    X(unum_getTextAttribute, int32_t, (const void *, int32_t, UChar *, int32_t, UErrorCode *)) \
    X(unum_setTextAttribute, void, (void *, int32_t, const UChar *, int32_t, UErrorCode *)) \
    X(unum_getSymbol, int32_t, (const void *, int32_t, UChar *, int32_t, UErrorCode *)) \
    X(unum_setSymbol, void, (void *, int32_t, const UChar *, int32_t, UErrorCode *)) \
    X(unum_applyPattern, void, (void *, UBool, const UChar *, int32_t, UParseError *, UErrorCode *)) \
    X(unum_toPattern, int32_t, (const void *, UBool, UChar *, int32_t, UErrorCode *)) \
    X(unumf_openForSkeletonAndLocale, void *, (const UChar *, int32_t, const char *, UErrorCode *)) \
    X(unumf_openResult, void *, (UErrorCode *)) \
    X(unumf_formatDouble, void, (const void *, double, void *, UErrorCode *)) \
    X(unumf_resultToString, int32_t, (const void *, UChar *, int32_t, UErrorCode *)) \
    X(unumf_close, void, (void *)) \
    X(unumf_closeResult, void, (void *)) \
    X(ucol_open, void *, (const char *, UErrorCode *)) \
    X(ucol_close, void, (void *)) \
    X(ucol_strcoll, int32_t, (const void *, const UChar *, int32_t, const UChar *, int32_t)) \
    X(ucol_setStrength, void, (void *, int32_t)) \
    X(ucol_setAttribute, void, (void *, int32_t, int32_t, UErrorCode *)) \
    X(ucol_getSortKey, int32_t, (const void *, const UChar *, int32_t, uint8_t *, int32_t)) \
    X(utrans_openU, void *, (const UChar *, int32_t, int32_t, const UChar *, int32_t, UParseError *, UErrorCode *)) \
    X(utrans_close, void, (void *)) \
    X(utrans_transUChars, void, (const void *, UChar *, int32_t *, int32_t, int32_t, int32_t *, UErrorCode *)) \
    X(unorm2_getNFCInstance, const void *, (UErrorCode *)) \
    X(unorm2_getNFDInstance, const void *, (UErrorCode *)) \
    X(unorm2_getNFKCInstance, const void *, (UErrorCode *)) \
    X(unorm2_getNFKDInstance, const void *, (UErrorCode *)) \
    X(unorm2_normalize, int32_t, (const void *, const UChar *, int32_t, UChar *, int32_t, UErrorCode *)) \
    X(u_strToUpper, int32_t, (UChar *, int32_t, const UChar *, int32_t, const char *, UErrorCode *)) \
    X(u_strToLower, int32_t, (UChar *, int32_t, const UChar *, int32_t, const char *, UErrorCode *)) \
    X(u_strToTitle, int32_t, (UChar *, int32_t, const UChar *, int32_t, void *, const char *, UErrorCode *)) \
    X(u_strFoldCase, int32_t, (UChar *, int32_t, const UChar *, int32_t, uint32_t, UErrorCode *)) \
    X(ulistfmt_openForType, void *, (const char *, int32_t, int32_t, UErrorCode *)) \
    X(ulistfmt_close, void, (void *)) \
    X(ulistfmt_format, int32_t, (const void *, const UChar *const *, const int32_t *, int32_t, UChar *, int32_t, UErrorCode *)) \
    X(ureldatefmt_open, void *, (const char *, void *, int32_t, int32_t, UErrorCode *)) \
    X(ureldatefmt_close, void, (void *)) \
    X(ureldatefmt_format, int32_t, (const void *, double, int32_t, UChar *, int32_t, UErrorCode *)) \
    X(ureldatefmt_formatNumeric, int32_t, (const void *, double, int32_t, UChar *, int32_t, UErrorCode *)) \
    X(udtitvfmt_open, void *, (const char *, const UChar *, int32_t, const UChar *, int32_t, UErrorCode *)) \
    X(udtitvfmt_close, void, (void *)) \
    X(udtitvfmt_format, int32_t, (const void *, UDate, UDate, UChar *, int32_t, UFieldPosition *, UErrorCode *)) \
    X(uplrules_open, void *, (const char *, UErrorCode *)) \
    X(uplrules_close, void, (void *)) \
    X(uplrules_select, int32_t, (const void *, double, UChar *, int32_t, UErrorCode *)) \
    X(ubrk_open, void *, (int32_t, const char *, const UChar *, int32_t, UErrorCode *)) \
    X(ubrk_close, void, (void *)) \
    X(ubrk_first, int32_t, (void *)) \
    X(ubrk_next, int32_t, (void *)) \
    X(ubrk_following, int32_t, (void *, int32_t)) \
    X(ubrk_preceding, int32_t, (void *, int32_t)) \
    X(ubrk_getRuleStatus, int32_t, (void *)) \
    X(uregex_open, void *, (const UChar *, int32_t, uint32_t, UParseError *, UErrorCode *)) \
    X(uregex_close, void, (void *)) \
    X(uregex_setText, void, (void *, const UChar *, int32_t, UErrorCode *)) \
    X(uregex_find, UBool, (void *, int32_t, UErrorCode *)) \
    X(uregex_findNext, UBool, (void *, UErrorCode *)) \
    X(uregex_start, int32_t, (void *, int32_t, UErrorCode *)) \
    X(uregex_end, int32_t, (void *, int32_t, UErrorCode *)) \
    X(uregex_groupCount, int32_t, (void *, UErrorCode *)) \
    X(uregex_setRegion, void, (void *, int32_t, int32_t, UErrorCode *)) \
    X(uregex_replaceAll, int32_t, (void *, const UChar *, int32_t, UChar *, int32_t, UErrorCode *)) \
    X(uregex_reset, void, (void *, int32_t, UErrorCode *)) \
    X(uregex_lookingAt, UBool, (void *, int32_t, UErrorCode *)) \
    X(uregex_matches, UBool, (void *, int32_t, UErrorCode *)) \
    X(uregex_useAnchoringBounds, void, (void *, UBool, UErrorCode *)) \
    X(uregex_useTransparentBounds, void, (void *, UBool, UErrorCode *)) \
    X(uregex_hitEnd, UBool, (const void *, UErrorCode *)) \
    X(uregex_requireEnd, UBool, (const void *, UErrorCode *)) \
    X(u_charType, int8_t, (UChar32)) \
    X(u_isalpha, UBool, (UChar32)) \
    X(u_isUWhiteSpace, UBool, (UChar32)) \
    X(u_hasBinaryProperty, UBool, (UChar32, int32_t))

typedef struct fdn_icu_t {
    int ok;   /* 1 quando a biblioteca abriu e as funções essenciais existem */
#define FDN_ICU_FIELD(name, ret, args) ret (*name) args;
    FDN_ICU_FUNCS(FDN_ICU_FIELD)
#undef FDN_ICU_FIELD
} fdn_icu_t;

/* A tabela, carregada na primeira chamada. Campos de funções que a biblioteca não tem ficam nulos. */
const fdn_icu_t *fdn_icu(void);

/* UTF-8 <-> UTF-16. Devolvem o comprimento (sem o 0 final); com cap curto, truncam e continuam contando. */
int32_t fdn_u16_from_utf8(const char *s, int32_t n, UChar *out, int32_t cap);
int32_t fdn_utf8_from_u16(const UChar *s, int32_t n, char *out, int32_t cap);
/* Cópias em memória nova (malloc), com 0 no fim. *len pode ser NULL. */
UChar *fdn_u16_dup(const char *utf8, int32_t *len);
char *fdn_utf8_dup(const UChar *s, int32_t n);
