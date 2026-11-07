/*
 * mOS SDK (variante com interop ObjC, docs/swift-objc-runtime.md, fase 5) — Foundation em ObjC para o importador Swift
 * compilar o overlay Foundation (userland/sdk/swift-objc/Foundation). As declarações seguem as da Apple (nomes de
 * classe, tipos C e nomes Swift), porque os nomes mangled das extensões do overlay saem delas; as classes são as do
 * libSystem do mOS (foundation.c e companhia). Só entra o que o overlay usa; cresce por marco.
 * (Os apps em ObjC do mOS usam outro cabeçalho, userland/sdk/include/Foundation.)
 */
#pragma once
#import <objc/NSObject.h>
#include <stdarg.h>
#include <stdint.h>
#include <CoreFoundation/CoreFoundation.h>

#define NS_ASSUME_NONNULL_BEGIN _Pragma("clang assume_nonnull begin")
#define NS_ASSUME_NONNULL_END _Pragma("clang assume_nonnull end")
#define NS_SWIFT_NAME(n) __attribute__((swift_name(#n)))
#define NS_REFINED_FOR_SWIFT __attribute__((swift_private))
#define NS_SWIFT_UNAVAILABLE(m) __attribute__((availability(swift, unavailable, message=m)))
#define NS_ENUM(T, N) enum __attribute__((enum_extensibility(open))) N : T N; enum N : T
#define NS_OPTIONS(T, N) enum __attribute__((flag_enum, enum_extensibility(open))) N : T N; enum N : T
#define NS_TYPED_EXTENSIBLE_ENUM __attribute__((swift_wrapper(struct)))
#define NS_STRING_ENUM __attribute__((swift_wrapper(enum)))
#define FOUNDATION_EXPORT extern
#define NS_FORMAT_FUNCTION(F, A) __attribute__((format(__NSString__, F, A)))
#define NS_RETURNS_INNER_POINTER __attribute__((objc_returns_inner_pointer))

typedef unsigned short unichar;

NS_ASSUME_NONNULL_BEGIN

@class NSString, NSArray<ObjectType>, NSDictionary<KeyType, ObjectType>, NSData, NSError, NSURL, NSEnumerator<ObjectType>;

typedef struct _NSRange {
    NSUInteger location;
    NSUInteger length;
} NSRange;
typedef NSRange *NSRangePointer;

@protocol NSCopying
- (id)copyWithZone:(nullable struct _NSZone *)zone;
@end
@protocol NSMutableCopying
- (id)mutableCopyWithZone:(nullable struct _NSZone *)zone;
@end
@protocol NSSecureCoding
@end

typedef struct {
    unsigned long state;
    id __unsafe_unretained _Nullable * _Nullable itemsPtr;
    unsigned long * _Nullable mutationsPtr;
    unsigned long extra[5];
} NSFastEnumerationState;

@protocol NSFastEnumeration
- (NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState *)state
                                  objects:(id __unsafe_unretained _Nullable [_Nonnull])buffer count:(NSUInteger)len;
@end

@interface NSObject (NSFoundationMOS)
- (id)copy;
- (id)mutableCopy;
@property (readonly, copy) NSString *description;
@end

/* ---- NSString */
typedef NSUInteger NSStringEncoding;
typedef NS_OPTIONS(NSUInteger, NSStringCompareOptions) {
    NSCaseInsensitiveSearch = 1, NSLiteralSearch = 2, NSBackwardsSearch = 4, NSAnchoredSearch = 8,
    NSNumericSearch = 64, NSDiacriticInsensitiveSearch = 128, NSWidthInsensitiveSearch = 256,
    NSForcedOrderingSearch = 512, NSRegularExpressionSearch = 1024
};
typedef NS_ENUM(NSInteger, NSComparisonResult) { NSOrderedAscending = -1L, NSOrderedSame, NSOrderedDescending };

@interface NSString : NSObject <NSCopying, NSMutableCopying, NSSecureCoding>
@property (readonly) NSUInteger length;
- (unichar)characterAtIndex:(NSUInteger)index;
- (instancetype)init;
- (nullable instancetype)initWithBytes:(const void *)bytes length:(NSUInteger)len encoding:(NSStringEncoding)encoding;
- (instancetype)initWithCharacters:(const unichar *)characters length:(NSUInteger)length;
- (nullable instancetype)initWithUTF8String:(const char *)s;
- (instancetype)initWithString:(NSString *)s;
- (instancetype)initWithFormat:(NSString *)format arguments:(va_list)argList NS_FORMAT_FUNCTION(1,0);
+ (instancetype)stringWithString:(NSString *)s;
@property (nullable, readonly) const char *UTF8String NS_RETURNS_INNER_POINTER;
- (nullable const char *)cStringUsingEncoding:(NSStringEncoding)encoding NS_RETURNS_INNER_POINTER;
- (NSUInteger)lengthOfBytesUsingEncoding:(NSStringEncoding)enc;
- (BOOL)isEqualToString:(NSString *)aString;
- (NSComparisonResult)compare:(NSString *)string;
- (BOOL)hasPrefix:(NSString *)str;
- (BOOL)hasSuffix:(NSString *)str;
- (NSRange)rangeOfString:(NSString *)searchString;
@end

@interface NSMutableString : NSString
- (void)appendString:(NSString *)aString;
@end

/* ---- NSValue / NSNumber */
@interface NSValue : NSObject <NSCopying, NSSecureCoding>
@end

@interface NSNumber : NSValue
- (NSNumber *)initWithChar:(char)value;
- (NSNumber *)initWithUnsignedChar:(unsigned char)value;
- (NSNumber *)initWithShort:(short)value;
- (NSNumber *)initWithUnsignedShort:(unsigned short)value;
- (NSNumber *)initWithInt:(int)value;
- (NSNumber *)initWithUnsignedInt:(unsigned int)value;
- (NSNumber *)initWithLong:(long)value;
- (NSNumber *)initWithUnsignedLong:(unsigned long)value;
- (NSNumber *)initWithLongLong:(long long)value;
- (NSNumber *)initWithUnsignedLongLong:(unsigned long long)value;
- (NSNumber *)initWithFloat:(float)value;
- (NSNumber *)initWithDouble:(double)value;
- (NSNumber *)initWithBool:(BOOL)value;
- (NSNumber *)initWithInteger:(NSInteger)value;
- (NSNumber *)initWithUnsignedInteger:(NSUInteger)value;
@property (readonly) char charValue;
@property (readonly) unsigned char unsignedCharValue;
@property (readonly) short shortValue;
@property (readonly) unsigned short unsignedShortValue;
@property (readonly) int intValue;
@property (readonly) unsigned int unsignedIntValue;
@property (readonly) long longValue;
@property (readonly) unsigned long unsignedLongValue;
@property (readonly) long long longLongValue;
@property (readonly) unsigned long long unsignedLongLongValue;
@property (readonly) float floatValue;
@property (readonly) double doubleValue;
@property (readonly) BOOL boolValue;
@property (readonly) NSInteger integerValue;
@property (readonly) NSUInteger unsignedIntegerValue;
@property (readonly, copy) NSString *stringValue;
@property (readonly) const char *objCType NS_RETURNS_INNER_POINTER;
- (BOOL)isEqualToNumber:(NSNumber *)number;
@end

/* ---- coleções */
@interface NSEnumerator<ObjectType> : NSObject <NSFastEnumeration>
- (nullable ObjectType)nextObject;
@end

@interface NSArray<__covariant ObjectType> : NSObject <NSCopying, NSMutableCopying, NSSecureCoding, NSFastEnumeration>
@property (readonly) NSUInteger count;
- (ObjectType)objectAtIndex:(NSUInteger)index;
- (instancetype)init;
- (instancetype)initWithObjects:(const ObjectType _Nonnull [_Nullable])objects count:(NSUInteger)cnt;
+ (instancetype)arrayWithObjects:(const ObjectType _Nonnull [_Nonnull])objects count:(NSUInteger)cnt;
- (NSEnumerator<ObjectType> *)objectEnumerator;
@end

@interface NSMutableArray<ObjectType> : NSArray<ObjectType>
- (void)addObject:(ObjectType)anObject;
@end

@interface NSDictionary<__covariant KeyType, __covariant ObjectType> : NSObject <NSCopying, NSMutableCopying, NSSecureCoding, NSFastEnumeration>
@property (readonly) NSUInteger count;
- (nullable ObjectType)objectForKey:(KeyType)aKey;
- (NSEnumerator<KeyType> *)keyEnumerator;
- (instancetype)init;
- (instancetype)initWithObjects:(const ObjectType _Nonnull [_Nullable])objects forKeys:(const KeyType <NSCopying> _Nonnull [_Nullable])keys count:(NSUInteger)cnt;
+ (instancetype)dictionaryWithObjects:(const ObjectType _Nonnull [_Nonnull])objects forKeys:(const KeyType <NSCopying> _Nonnull [_Nonnull])keys count:(NSUInteger)cnt;
@property (readonly, copy) NSArray<KeyType> *allKeys;
@property (readonly, copy) NSArray<ObjectType> *allValues;
@end

@interface NSMutableDictionary<KeyType, ObjectType> : NSDictionary<KeyType, ObjectType>
- (void)setObject:(ObjectType)anObject forKey:(KeyType <NSCopying>)aKey;
@end

@interface NSSet<__covariant ObjectType> : NSObject <NSCopying, NSMutableCopying, NSSecureCoding, NSFastEnumeration>
@property (readonly) NSUInteger count;
- (NSEnumerator<ObjectType> *)objectEnumerator;
- (instancetype)initWithObjects:(const ObjectType _Nonnull [_Nullable])objects count:(NSUInteger)cnt;
+ (instancetype)setWithObjects:(const ObjectType _Nonnull [_Nonnull])objects count:(NSUInteger)cnt;
@property (readonly, copy) NSArray<ObjectType> *allObjects;
@end

@interface NSNull : NSObject <NSCopying, NSSecureCoding>
+ (NSNull *)null;
@end

/* ---- erros */
typedef NSString *NSErrorDomain;
typedef NSString *NSErrorUserInfoKey NS_TYPED_EXTENSIBLE_ENUM;

@interface NSError : NSObject <NSCopying, NSSecureCoding>
- (instancetype)initWithDomain:(NSErrorDomain)domain code:(NSInteger)code userInfo:(nullable NSDictionary<NSErrorUserInfoKey, id> *)dict;
+ (instancetype)errorWithDomain:(NSErrorDomain)domain code:(NSInteger)code userInfo:(nullable NSDictionary<NSErrorUserInfoKey, id> *)dict;
@property (readonly, copy) NSErrorDomain domain;
@property (readonly) NSInteger code;
@property (readonly, copy) NSDictionary<NSErrorUserInfoKey, id> *userInfo;
@property (readonly, copy) NSString *localizedDescription;
@property (nullable, readonly, copy) NSString *localizedFailureReason;
@property (nullable, readonly, copy) NSString *localizedRecoverySuggestion;
@end

FOUNDATION_EXPORT NSErrorUserInfoKey const NSLocalizedDescriptionKey;
FOUNDATION_EXPORT NSErrorUserInfoKey const NSLocalizedFailureReasonErrorKey;
FOUNDATION_EXPORT NSErrorUserInfoKey const NSLocalizedRecoverySuggestionErrorKey;
FOUNDATION_EXPORT NSErrorUserInfoKey const NSHelpAnchorErrorKey;
FOUNDATION_EXPORT NSErrorUserInfoKey const NSUnderlyingErrorKey;
FOUNDATION_EXPORT NSErrorDomain const NSCocoaErrorDomain;
FOUNDATION_EXPORT NSErrorDomain const NSPOSIXErrorDomain;
FOUNDATION_EXPORT NSErrorDomain const NSOSStatusErrorDomain;

/* ---- NSLog */
FOUNDATION_EXPORT void NSLogv(NSString *format, va_list args) NS_FORMAT_FUNCTION(1,0);

NS_ASSUME_NONNULL_END
