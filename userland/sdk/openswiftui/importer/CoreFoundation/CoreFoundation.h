/*
 * mOS SDK — CoreFoundation em C só para o importador clang do swiftc (módulos C das deps do OpenSwiftUI:
 * OpenAttributeGraphCxx, OpenRenderBoxCxx, OpenSwiftUI_SPI). Os cabeçalhos de libc do macOS que vêm com o
 * zig não funcionam no modo de módulos do importador sem o SDK da Apple; os cabeçalhos públicos dessas
 * bibliotecas só precisam dos tipos e macros do CF, que ficam aqui, iguais aos do swift-corelibs-foundation.
 * A compilação em C/C++ (zig) continua usando o CoreFoundation completo de SwiftCorelibs.
 */
#ifndef MOS_IMPORTER_COREFOUNDATION_H
#define MOS_IMPORTER_COREFOUNDATION_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#if defined(__cplusplus)
#define CF_EXTERN_C_BEGIN extern "C" {
#define CF_EXTERN_C_END }
#define CF_EXPORT extern "C" __attribute__((visibility("default")))
#else
#define CF_EXTERN_C_BEGIN
#define CF_EXTERN_C_END
#define CF_EXPORT extern __attribute__((visibility("default")))
#endif
#define CF_EXTERN CF_EXPORT
#define CF_INLINE static __inline__ __attribute__((always_inline))
#define CF_ASSUME_NONNULL_BEGIN _Pragma("clang assume_nonnull begin")
#define CF_ASSUME_NONNULL_END _Pragma("clang assume_nonnull end")
#define CF_IMPLICIT_BRIDGING_ENABLED _Pragma("clang arc_cf_code_audited begin")
#define CF_IMPLICIT_BRIDGING_DISABLED _Pragma("clang arc_cf_code_audited end")
#define CF_BRIDGED_TYPE(T) __attribute__((objc_bridge(T)))
#define CF_BRIDGED_MUTABLE_TYPE(T) __attribute__((objc_bridge_mutable(T)))
#define CF_RELATED_TYPE(T, C, I) __attribute__((objc_bridge_related(T, C, I)))
#define CF_SWIFT_NAME(_name) __attribute__((swift_name(#_name)))
#define CF_REFINED_FOR_SWIFT __attribute__((swift_private))
#define CF_SWIFT_UNAVAILABLE(_msg) __attribute__((availability(swift, unavailable, message = _msg)))
#define CF_RETURNS_RETAINED __attribute__((cf_returns_retained))
#define CF_RETURNS_NOT_RETAINED __attribute__((cf_returns_not_retained))
#define CF_RELEASES_ARGUMENT __attribute__((cf_consumed))
#define CF_CONSUMED __attribute__((cf_consumed))
#define CF_NOESCAPE __attribute__((noescape))
#define CF_WARN_UNUSED_RESULT __attribute__((warn_unused_result))
#define CF_NO_TAIL_CALL
#define CF_AUTOMATED_REFCOUNT_UNAVAILABLE
#define CF_FORMAT_FUNCTION(F, A) __attribute__((format(CFString, F, A)))
#define CF_FORMAT_ARGUMENT(A) __attribute__((format_arg(A)))
#define CF_ENUM_AVAILABLE(...)
#define CF_AVAILABLE(...)
#define CF_AVAILABLE_MAC(...)
#define API_AVAILABLE(...)
#define API_UNAVAILABLE(...)
#define API_DEPRECATED(...)
#define CF_ENUM_ATTRIBUTES __attribute__((enum_extensibility(open)))
#define CF_CLOSED_ENUM_ATTRIBUTES __attribute__((enum_extensibility(closed)))
#define CF_OPTIONS_ATTRIBUTES __attribute__((flag_enum, enum_extensibility(open)))
#define __CF_ENUM_GET_MACRO(_1, _2, NAME, ...) NAME
/* mesma forma do CFAvailability.h do swift-corelibs: em C, `typedef` precisa de um declarador antes */
#define __CF_NAMED_ENUM(_type, _name) int __CF_ENUM_##_name; enum CF_ENUM_ATTRIBUTES _name : _type; typedef enum _name _name; enum _name : _type
#define __CF_ANON_ENUM(_type) enum CF_ENUM_ATTRIBUTES : _type
#define CF_ENUM(...) __CF_ENUM_GET_MACRO(__VA_ARGS__, __CF_NAMED_ENUM, __CF_ANON_ENUM, )(__VA_ARGS__)
#define CF_CLOSED_ENUM(_type, _name) int __CF_ENUM_##_name; enum CF_CLOSED_ENUM_ATTRIBUTES _name : _type; typedef enum _name _name; enum _name : _type
#if defined(__cplusplus)
#define CF_OPTIONS(_type, _name) _type _name; enum CF_OPTIONS_ATTRIBUTES : _type
#else
#define CF_OPTIONS(_type, _name) int __CF_OPTIONS_##_name; enum CF_OPTIONS_ATTRIBUTES _name : _type; typedef enum _name _name; enum _name : _type
#endif
#ifndef CF_SWIFT_UNAVAILABLE_FROM_ASYNC
#define CF_SWIFT_UNAVAILABLE_FROM_ASYNC(_msg)
#endif

CF_EXTERN_C_BEGIN
CF_ASSUME_NONNULL_BEGIN

typedef unsigned long u_long;
typedef unsigned int u_int;
typedef unsigned short u_short;
typedef unsigned char u_char;
typedef long ssize_t;
typedef bool Boolean;
typedef uint8_t UInt8;
typedef int8_t SInt8;
typedef uint16_t UInt16;
typedef int16_t SInt16;
typedef uint32_t UInt32;
typedef int32_t SInt32;
typedef uint64_t UInt64;
typedef int64_t SInt64;
typedef uint16_t UniChar;
typedef uint32_t UTF32Char;
typedef uint16_t UTF16Char;
typedef uint8_t UTF8Char;
typedef unsigned long CFTypeID;
typedef unsigned long CFOptionFlags;
typedef unsigned long CFHashCode;
typedef signed long CFIndex;
typedef UInt32 CFStringEncoding;

typedef const CF_BRIDGED_TYPE(id) void *CFTypeRef;
typedef const struct CF_BRIDGED_TYPE(NSString) __CFString *CFStringRef;
typedef struct CF_BRIDGED_MUTABLE_TYPE(NSMutableString) __CFString *CFMutableStringRef;
typedef const struct CF_BRIDGED_TYPE(id) __CFAllocator *CFAllocatorRef;
typedef const struct CF_BRIDGED_TYPE(NSArray) __CFArray *CFArrayRef;
typedef struct CF_BRIDGED_MUTABLE_TYPE(NSMutableArray) __CFArray *CFMutableArrayRef;
typedef const struct CF_BRIDGED_TYPE(NSDictionary) __CFDictionary *CFDictionaryRef;
typedef struct CF_BRIDGED_MUTABLE_TYPE(NSMutableDictionary) __CFDictionary *CFMutableDictionaryRef;
typedef const struct CF_BRIDGED_TYPE(NSData) __CFData *CFDataRef;
typedef struct CF_BRIDGED_MUTABLE_TYPE(NSMutableData) __CFData *CFMutableDataRef;
typedef const struct CF_BRIDGED_TYPE(NSURL) __CFURL *CFURLRef;
typedef const struct CF_BRIDGED_TYPE(NSNumber) __CFNumber *CFNumberRef;
typedef const struct CF_BRIDGED_TYPE(NSNull) __CFNull *CFNullRef;
typedef const struct CF_BRIDGED_TYPE(NSCharacterSet) __CFCharacterSet *CFCharacterSetRef;
typedef CF_BRIDGED_TYPE(id) CFTypeRef CFPropertyListRef;

typedef struct { CFIndex location; CFIndex length; } CFRange;
CF_INLINE CFRange CFRangeMake(CFIndex loc, CFIndex len) { CFRange r; r.location = loc; r.length = len; return r; }
typedef CF_ENUM(CFIndex, CFComparisonResult) { kCFCompareLessThan = -1L, kCFCompareEqualTo = 0, kCFCompareGreaterThan = 1 };

/* ---- objetos CF vistos pelo Swift: todos são objetos Swift nativos criados por libMOSCFRuntime.a
 * (userland/sdk/openswiftui/src/cf_mos.cpp), porque o Swift sem ObjC os retém com swift_retain. */
typedef const struct CF_BRIDGED_TYPE(NSNumber) __CFBoolean *CFBooleanRef;
typedef double CFAbsoluteTime;
typedef double CFTimeInterval;

CF_EXPORT const CFAllocatorRef _Nullable kCFAllocatorDefault;
CF_EXPORT const CFAllocatorRef _Nullable kCFAllocatorSystemDefault;
CF_EXPORT CFTypeID CFGetTypeID(CFTypeRef cf);
CF_EXPORT CFTypeRef CFRetain(CFTypeRef cf);
CF_EXPORT void CFRelease(CFTypeRef cf);
CF_EXPORT Boolean CFEqual(CFTypeRef cf1, CFTypeRef cf2);
CF_EXPORT CFHashCode CFHash(CFTypeRef cf);
CF_EXPORT CFStringRef _Null_unspecified CFCopyDescription(CFTypeRef _Nullable cf) CF_RETURNS_RETAINED;
CF_EXPORT CFAllocatorRef _Nullable CFAllocatorGetDefault(void);
CF_EXPORT CFAbsoluteTime CFAbsoluteTimeGetCurrent(void);

/* CFString */
typedef CF_ENUM(CFStringEncoding, CFStringBuiltInEncodings) {
    kCFStringEncodingMacRoman = 0,
    kCFStringEncodingWindowsLatin1 = 0x0500,
    kCFStringEncodingISOLatin1 = 0x0201,
    kCFStringEncodingNextStepLatin = 0x0B01,
    kCFStringEncodingASCII = 0x0600,
    kCFStringEncodingUnicode = 0x0100,
    kCFStringEncodingUTF8 = 0x08000100,
    kCFStringEncodingNonLossyASCII = 0x0BFF,
    kCFStringEncodingUTF16 = 0x0100,
    kCFStringEncodingUTF16BE = 0x10000100,
    kCFStringEncodingUTF16LE = 0x14000100,
    kCFStringEncodingUTF32 = 0x0c000100,
};
CF_EXPORT CFTypeID CFStringGetTypeID(void);
CF_EXPORT CFStringRef _Null_unspecified CFStringCreateWithCString(CFAllocatorRef _Nullable alloc, const char *_Nullable cStr, CFStringEncoding encoding) CF_RETURNS_RETAINED;
CF_EXPORT CFStringRef _Null_unspecified CFStringCreateWithBytes(CFAllocatorRef _Nullable alloc, const UInt8 *_Nullable bytes, CFIndex numBytes, CFStringEncoding encoding, Boolean isExternalRepresentation) CF_RETURNS_RETAINED;
CF_EXPORT CFStringRef _Null_unspecified CFStringCreateWithCharacters(CFAllocatorRef _Nullable alloc, const UniChar *_Nullable chars, CFIndex numChars) CF_RETURNS_RETAINED;
CF_EXPORT CFStringRef _Null_unspecified CFStringCreateCopy(CFAllocatorRef _Nullable alloc, CFStringRef theString) CF_RETURNS_RETAINED;
CF_EXPORT CFMutableStringRef _Null_unspecified CFStringCreateMutable(CFAllocatorRef _Nullable alloc, CFIndex maxLength) CF_RETURNS_RETAINED;
CF_EXPORT CFMutableStringRef _Null_unspecified CFStringCreateMutableCopy(CFAllocatorRef _Nullable alloc, CFIndex maxLength, CFStringRef theString) CF_RETURNS_RETAINED;
CF_EXPORT void CFStringAppend(CFMutableStringRef theString, CFStringRef appendedString);
CF_EXPORT void CFStringAppendCString(CFMutableStringRef theString, const char *cStr, CFStringEncoding encoding);
CF_EXPORT CFIndex CFStringGetLength(CFStringRef theString);
CF_EXPORT UniChar CFStringGetCharacterAtIndex(CFStringRef theString, CFIndex idx);
CF_EXPORT void CFStringGetCharacters(CFStringRef theString, CFRange range, UniChar *buffer);
CF_EXPORT const char *_Nullable CFStringGetCStringPtr(CFStringRef theString, CFStringEncoding encoding);
CF_EXPORT Boolean CFStringGetCString(CFStringRef theString, char *buffer, CFIndex bufferSize, CFStringEncoding encoding);
CF_EXPORT CFIndex CFStringGetMaximumSizeForEncoding(CFIndex length, CFStringEncoding encoding);
CF_EXPORT CFComparisonResult CFStringCompare(CFStringRef theString1, CFStringRef theString2, CFOptionFlags compareOptions);
CF_EXPORT Boolean CFStringHasPrefix(CFStringRef theString, CFStringRef prefix);
CF_EXPORT Boolean CFStringHasSuffix(CFStringRef theString, CFStringRef suffix);

/* CFNumber / CFBoolean */
typedef CF_ENUM(CFIndex, CFNumberType) {
    kCFNumberSInt8Type = 1, kCFNumberSInt16Type = 2, kCFNumberSInt32Type = 3, kCFNumberSInt64Type = 4,
    kCFNumberFloat32Type = 5, kCFNumberFloat64Type = 6, kCFNumberCharType = 7, kCFNumberShortType = 8,
    kCFNumberIntType = 9, kCFNumberLongType = 10, kCFNumberLongLongType = 11, kCFNumberFloatType = 12,
    kCFNumberDoubleType = 13, kCFNumberCFIndexType = 14, kCFNumberNSIntegerType = 15, kCFNumberCGFloatType = 16,
    kCFNumberMaxType = 16
};
CF_EXPORT CFTypeID CFNumberGetTypeID(void);
CF_EXPORT CFNumberRef _Null_unspecified CFNumberCreate(CFAllocatorRef _Nullable alloc, CFNumberType theType, const void *valuePtr) CF_RETURNS_RETAINED;
CF_EXPORT CFNumberType CFNumberGetType(CFNumberRef number);
CF_EXPORT Boolean CFNumberGetValue(CFNumberRef number, CFNumberType theType, void *valuePtr);
CF_EXPORT Boolean CFNumberIsFloatType(CFNumberRef number);
CF_EXPORT CFComparisonResult CFNumberCompare(CFNumberRef number, CFNumberRef otherNumber, void *_Nullable context);
CF_EXPORT const CFBooleanRef kCFBooleanTrue;
CF_EXPORT const CFBooleanRef kCFBooleanFalse;
CF_EXPORT CFTypeID CFBooleanGetTypeID(void);
CF_EXPORT Boolean CFBooleanGetValue(CFBooleanRef boolean);

/* CFArray */
typedef const void *_Nullable (*CFArrayRetainCallBack)(CFAllocatorRef _Nullable allocator, const void *_Nullable value);
typedef void (*CFArrayReleaseCallBack)(CFAllocatorRef _Nullable allocator, const void *_Nullable value);
typedef CFStringRef _Nullable (*CFArrayCopyDescriptionCallBack)(const void *_Nullable value);
typedef Boolean (*CFArrayEqualCallBack)(const void *_Nullable value1, const void *_Nullable value2);
typedef struct {
    CFIndex version;
    CFArrayRetainCallBack _Nullable retain;
    CFArrayReleaseCallBack _Nullable release;
    CFArrayCopyDescriptionCallBack _Nullable copyDescription;
    CFArrayEqualCallBack _Nullable equal;
} CFArrayCallBacks;
CF_EXPORT const CFArrayCallBacks kCFTypeArrayCallBacks;
typedef void (*CFArrayApplierFunction)(const void *_Nullable value, void *_Nullable context);
CF_EXPORT CFTypeID CFArrayGetTypeID(void);
CF_EXPORT CFArrayRef _Null_unspecified CFArrayCreate(CFAllocatorRef _Nullable allocator, const void *_Nullable *_Nullable values, CFIndex numValues, const CFArrayCallBacks *_Nullable callBacks) CF_RETURNS_RETAINED;
CF_EXPORT CFMutableArrayRef _Null_unspecified CFArrayCreateMutable(CFAllocatorRef _Nullable allocator, CFIndex capacity, const CFArrayCallBacks *_Nullable callBacks) CF_RETURNS_RETAINED;
CF_EXPORT CFIndex CFArrayGetCount(CFArrayRef theArray);
CF_EXPORT const void *_Nullable CFArrayGetValueAtIndex(CFArrayRef theArray, CFIndex idx);
CF_EXPORT void CFArrayAppendValue(CFMutableArrayRef theArray, const void *_Nullable value);
CF_EXPORT void CFArrayApplyFunction(CFArrayRef theArray, CFRange range, CFArrayApplierFunction applier, void *_Nullable context);

/* CFDictionary */
typedef const void *_Nullable (*CFDictionaryRetainCallBack)(CFAllocatorRef _Nullable allocator, const void *_Nullable value);
typedef void (*CFDictionaryReleaseCallBack)(CFAllocatorRef _Nullable allocator, const void *_Nullable value);
typedef CFStringRef _Nullable (*CFDictionaryCopyDescriptionCallBack)(const void *_Nullable value);
typedef Boolean (*CFDictionaryEqualCallBack)(const void *_Nullable value1, const void *_Nullable value2);
typedef CFHashCode (*CFDictionaryHashCallBack)(const void *_Nullable value);
typedef struct {
    CFIndex version;
    CFDictionaryRetainCallBack _Nullable retain;
    CFDictionaryReleaseCallBack _Nullable release;
    CFDictionaryCopyDescriptionCallBack _Nullable copyDescription;
    CFDictionaryEqualCallBack _Nullable equal;
    CFDictionaryHashCallBack _Nullable hash;
} CFDictionaryKeyCallBacks;
typedef struct {
    CFIndex version;
    CFDictionaryRetainCallBack _Nullable retain;
    CFDictionaryReleaseCallBack _Nullable release;
    CFDictionaryCopyDescriptionCallBack _Nullable copyDescription;
    CFDictionaryEqualCallBack _Nullable equal;
} CFDictionaryValueCallBacks;
CF_EXPORT const CFDictionaryKeyCallBacks kCFTypeDictionaryKeyCallBacks;
CF_EXPORT const CFDictionaryKeyCallBacks kCFCopyStringDictionaryKeyCallBacks;
CF_EXPORT const CFDictionaryValueCallBacks kCFTypeDictionaryValueCallBacks;
typedef void (*CFDictionaryApplierFunction)(const void *_Nullable key, const void *_Nullable value, void *_Nullable context);
CF_EXPORT CFTypeID CFDictionaryGetTypeID(void);
CF_EXPORT CFDictionaryRef _Null_unspecified CFDictionaryCreate(CFAllocatorRef _Nullable allocator, const void *_Nullable *_Nullable keys, const void *_Nullable *_Nullable values, CFIndex numValues,
                                             const CFDictionaryKeyCallBacks *_Nullable keyCallBacks, const CFDictionaryValueCallBacks *_Nullable valueCallBacks) CF_RETURNS_RETAINED;
CF_EXPORT CFMutableDictionaryRef _Null_unspecified CFDictionaryCreateMutable(CFAllocatorRef _Nullable allocator, CFIndex capacity, const CFDictionaryKeyCallBacks *_Nullable keyCallBacks,
                                                           const CFDictionaryValueCallBacks *_Nullable valueCallBacks) CF_RETURNS_RETAINED;
CF_EXPORT CFMutableDictionaryRef _Null_unspecified CFDictionaryCreateMutableCopy(CFAllocatorRef _Nullable allocator, CFIndex capacity, CFDictionaryRef theDict) CF_RETURNS_RETAINED;
CF_EXPORT CFIndex CFDictionaryGetCount(CFDictionaryRef theDict);
CF_EXPORT const void *_Nullable CFDictionaryGetValue(CFDictionaryRef theDict, const void *_Nullable key);
CF_EXPORT Boolean CFDictionaryGetValueIfPresent(CFDictionaryRef theDict, const void *_Nullable key, const void *_Nullable *_Nullable value);
CF_EXPORT Boolean CFDictionaryContainsKey(CFDictionaryRef theDict, const void *_Nullable key);
CF_EXPORT void CFDictionaryGetKeysAndValues(CFDictionaryRef theDict, const void *_Nullable *_Nullable keys, const void *_Nullable *_Nullable values);
CF_EXPORT void CFDictionarySetValue(CFMutableDictionaryRef theDict, const void *_Nullable key, const void *_Nullable value);
CF_EXPORT void CFDictionaryRemoveValue(CFMutableDictionaryRef theDict, const void *_Nullable key);
CF_EXPORT void CFDictionaryApplyFunction(CFDictionaryRef theDict, CFDictionaryApplierFunction applier, void *_Nullable context);

/* CFData */
CF_EXPORT CFTypeID CFDataGetTypeID(void);
CF_EXPORT CFDataRef _Null_unspecified CFDataCreate(CFAllocatorRef _Nullable allocator, const UInt8 *_Nullable bytes, CFIndex length) CF_RETURNS_RETAINED;
CF_EXPORT CFMutableDataRef _Null_unspecified CFDataCreateMutable(CFAllocatorRef _Nullable allocator, CFIndex capacity) CF_RETURNS_RETAINED;
CF_EXPORT CFIndex CFDataGetLength(CFDataRef theData);
CF_EXPORT const UInt8 *_Nullable CFDataGetBytePtr(CFDataRef theData);
CF_EXPORT UInt8 *_Nullable CFDataGetMutableBytePtr(CFMutableDataRef theData);
CF_EXPORT void CFDataSetLength(CFMutableDataRef theData, CFIndex length);
CF_EXPORT void CFDataAppendBytes(CFMutableDataRef theData, const UInt8 *bytes, CFIndex length);

CF_ASSUME_NONNULL_END
CF_EXTERN_C_END

#include <CoreFoundation/CFRunLoop.h>
#include <CoreFoundation/CFCGTypes.h>

#endif
