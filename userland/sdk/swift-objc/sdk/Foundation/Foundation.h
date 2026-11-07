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
#include <Foundation/NSFoundationOptions.h>

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
/* NSStringCompareOptions e as outras opções: NSFoundationOptions.h (módulo _MOSNSOptions) */
/* NSComparisonResult (ComparisonResult no Swift): NSFoundationOptions.h */

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
typedef NSString *NSErrorUserInfoKey;   /* como no SDK da Apple: sem tipo próprio no Swift (userInfo é [String: Any]) */

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
FOUNDATION_EXPORT NSErrorDomain const NSURLErrorDomain;
/* CFError (o CoreFoundation do importador não tem o CFError.h): no mOS é um NSError; o Swift o vê como a classe CFError */
typedef struct __attribute__((objc_bridge(NSError))) __CFError *CFErrorRef;
FOUNDATION_EXPORT NSErrorUserInfoKey const NSURLErrorFailingURLErrorKey;
FOUNDATION_EXPORT NSErrorUserInfoKey const NSURLErrorFailingURLStringErrorKey;

/* ---- tipos-valor (pontes do marco B) */
@interface NSData : NSObject <NSCopying, NSMutableCopying, NSSecureCoding>
@property (readonly) NSUInteger length;
@property (readonly) const void *bytes NS_RETURNS_INNER_POINTER;
+ (instancetype)dataWithBytes:(nullable const void *)bytes length:(NSUInteger)length;
@end

@interface NSURL : NSObject <NSCopying, NSSecureCoding>
+ (nullable instancetype)URLWithString:(NSString *)URLString;
+ (NSURL *)fileURLWithPath:(NSString *)path isDirectory:(BOOL)isDir;
@property (nullable, readonly, copy) NSString *absoluteString;
@property (nullable, readonly, copy) NSString *path;
@property (readonly, getter=isFileURL) BOOL fileURL;
@end

@interface NSDate : NSObject <NSCopying, NSSecureCoding>
+ (instancetype)dateWithTimeIntervalSince1970:(double)secs;
@property (readonly) double timeIntervalSince1970;
@end

@interface NSUUID : NSObject <NSCopying, NSSecureCoding>
- (nullable instancetype)initWithUUIDString:(NSString *)string;
@property (readonly, copy) NSString *UUIDString;
@end

@interface NSLocale : NSObject <NSCopying, NSSecureCoding>
@property (class, readonly, copy) NSLocale *currentLocale;
@property (readonly, copy) NSString *localeIdentifier;
@end

@interface NSTimeZone : NSObject <NSCopying, NSSecureCoding>
+ (nullable instancetype)timeZoneWithName:(NSString *)tzName;
+ (instancetype)timeZoneForSecondsFromGMT:(NSInteger)seconds;
@property (readonly, copy) NSString *name;
@property (readonly) NSInteger secondsFromGMT;
@end

/* ---- tipos do marco B: IndexSet, Notification, URLRequest, URLResourceValues, KVO, URLSession */
typedef NSString *NSURLResourceKey NS_TYPED_EXTENSIBLE_ENUM NS_SWIFT_NAME(URLResourceKey);
typedef NSString *NSNotificationName NS_TYPED_EXTENSIBLE_ENUM;

typedef NS_ENUM(NSUInteger, NSURLRequestCachePolicy) {
    NSURLRequestUseProtocolCachePolicy = 0,
    NSURLRequestReloadIgnoringLocalCacheData = 1,
    NSURLRequestReloadIgnoringLocalAndRemoteCacheData = 4,
    NSURLRequestReturnCacheDataElseLoad = 2,
    NSURLRequestReturnCacheDataDontLoad = 3,
    NSURLRequestReloadRevalidatingCacheData = 5,
};

typedef NS_OPTIONS(NSUInteger, NSKeyValueObservingOptions) {
    NSKeyValueObservingOptionNew __attribute__((swift_name("new"))) = 0x01,
    NSKeyValueObservingOptionOld __attribute__((swift_name("old"))) = 0x02,
    NSKeyValueObservingOptionInitial __attribute__((swift_name("initial"))) = 0x04,
    NSKeyValueObservingOptionPrior __attribute__((swift_name("prior"))) = 0x08,
};

@interface NSIndexSet : NSObject <NSCopying, NSMutableCopying, NSSecureCoding>
+ (instancetype)indexSetWithIndex:(NSUInteger)value;
@property (readonly) NSUInteger firstIndex;
@end
@interface NSMutableIndexSet : NSIndexSet
+ (instancetype)indexSet;
- (void)addIndex:(NSUInteger)value;
- (BOOL)containsIndex:(NSUInteger)value;
@property (readonly) NSUInteger count;
@property (readonly) NSUInteger lastIndex;
@end

@interface NSNotification : NSObject <NSCopying, NSSecureCoding>
+ (instancetype)notificationWithName:(NSNotificationName)aName object:(nullable id)anObject userInfo:(nullable NSDictionary *)aUserInfo;
@property (readonly, copy) NSNotificationName name;
@property (nullable, readonly, retain) id object;
@property (nullable, readonly, copy) NSDictionary *userInfo;
@end

@interface NSURLRequest : NSObject <NSSecureCoding, NSCopying, NSMutableCopying>
+ (instancetype)requestWithURL:(NSURL *)URL;
@property (nullable, readonly, copy) NSURL *URL;
@property (nullable, readonly, copy) NSString *HTTPMethod;
@property (nullable, readonly, copy) NSDictionary<NSString *, NSString *> *allHTTPHeaderFields;
@property (nullable, readonly, copy) NSData *HTTPBody;
@end
@interface NSMutableURLRequest : NSURLRequest
- (void)setHTTPMethod:(NSString *)m;
- (void)setValue:(nullable NSString *)value forHTTPHeaderField:(NSString *)field;
- (void)setHTTPBody:(nullable NSData *)body;
@end

@interface NSURLResponse : NSObject <NSSecureCoding, NSCopying>
@end
@protocol NSURLSessionTaskDelegate <NSObject>
@end
@interface NSURLSessionTask : NSObject
- (void)resume;
@end
@interface NSURLSessionDataTask : NSURLSessionTask
@end
@interface NSURLSession : NSObject
@property (class, readonly, strong) NSURLSession *sharedSession;
- (NSURLSessionDataTask *)dataTaskWithRequest:(NSURLRequest *)request completionHandler:(void (^)(NSData * _Nullable data, NSURLResponse * _Nullable response, NSError * _Nullable error))completionHandler;
@end

@interface NSObject (NSKeyValueObservingMOS)
- (void)addObserver:(NSObject *)observer forKeyPath:(NSString *)keyPath options:(NSKeyValueObservingOptions)options context:(nullable void *)context;
- (void)removeObserver:(NSObject *)observer forKeyPath:(NSString *)keyPath context:(nullable void *)context;
- (void)observeValueForKeyPath:(nullable NSString *)keyPath ofObject:(nullable id)object change:(nullable NSDictionary<NSString *, id> *)change context:(nullable void *)context;
- (nullable id)valueForKey:(NSString *)key;
- (nullable id)valueForKeyPath:(NSString *)keyPath;
@end

/* ---- NSLog */
FOUNDATION_EXPORT void NSLogv(NSString *format, va_list args) NS_FORMAT_FUNCTION(1,0);

/* ---- NSIndexPath (ponte do IndexPath; tabelas e coleções do UIKit) */
@interface NSIndexPath : NSObject <NSCopying, NSSecureCoding>
+ (instancetype)indexPathWithIndexes:(const NSUInteger *)indexes length:(NSUInteger)length;
@property (readonly) NSUInteger length;
- (NSUInteger)indexAtPosition:(NSUInteger)position;
@end

/* ---- marco C: NSCoder, NSFileManager, NSUndoManager (o que as extensões Swift da Apple usam) */
@class NSCoder;
@protocol NSCoding
- (void)encodeWithCoder:(NSCoder *)coder;
- (nullable instancetype)initWithCoder:(NSCoder *)coder;
@end

@interface NSCoder : NSObject
- (nullable id)decodeObjectForKey:(NSString *)key;
- (nullable id)decodeObjectOfClass:(Class)aClass forKey:(NSString *)key;
@end

typedef NS_OPTIONS(NSUInteger, NSDirectoryEnumerationOptions) {
    NSDirectoryEnumerationSkipsSubdirectoryDescendants = 1UL << 0,
    NSDirectoryEnumerationSkipsPackageDescendants = 1UL << 1,
    NSDirectoryEnumerationSkipsHiddenFiles = 1UL << 2,
    NSDirectoryEnumerationIncludesDirectoriesPostOrder = 1UL << 3,
    NSDirectoryEnumerationProducesRelativePathURLs = 1UL << 4,
};
typedef NS_OPTIONS(NSUInteger, NSFileManagerItemReplacementOptions) {
    NSFileManagerItemReplacementUsingNewMetadataOnly = 1UL << 0,
    NSFileManagerItemReplacementWithoutDeletingBackupItem = 1UL << 1,
};

@interface NSDirectoryEnumerator<ObjectType> : NSEnumerator<ObjectType>
- (void)skipDescendants;
@property (readonly) NSUInteger level;
@end

@interface NSFileManager : NSObject
@property (class, readonly, strong) NSFileManager *defaultManager;
- (BOOL)fileExistsAtPath:(NSString *)path;
- (nullable NSDirectoryEnumerator<NSURL *> *)enumeratorAtURL:(NSURL *)url includingPropertiesForKeys:(nullable NSArray<NSURLResourceKey> *)keys options:(NSDirectoryEnumerationOptions)mask errorHandler:(nullable BOOL (^)(NSURL *url, NSError *error))handler NS_REFINED_FOR_SWIFT;
@end

@interface NSUndoManager : NSObject
- (void)registerUndoWithTarget:(id)target selector:(SEL)selector object:(nullable id)anObject;
- (void)undo;
@property (readonly) BOOL canUndo;
@end

NS_ASSUME_NONNULL_END
