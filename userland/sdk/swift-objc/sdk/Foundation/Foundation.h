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
#include <dispatch/dispatch.h>

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

@class NSString, NSCharacterSet, NSArray<ObjectType>, NSDictionary<KeyType, ObjectType>, NSData, NSError, NSURL, NSEnumerator<ObjectType>;

typedef struct _NSRange {
    NSUInteger location;
    NSUInteger length;
} NSRange;
typedef NSRange *NSRangePointer;

/* NSZone.h: no Swift, NSZone * vira o NSZone do módulo ObjectiveC */
typedef struct _NSZone NSZone;

@protocol NSCopying
- (id)copyWithZone:(nullable NSZone *)zone;
@end
@protocol NSMutableCopying
- (id)mutableCopyWithZone:(nullable NSZone *)zone;
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
/* as do libSystem (coreanimation.c): faixa e geometria, para as pontes de NSRange e CGPoint/CGSize/CGRect */
+ (NSValue *)valueWithRange:(NSRange)range;
@property (readonly) NSRange rangeValue;
+ (NSValue *)valueWithPoint:(CGPoint)point;
+ (NSValue *)valueWithSize:(CGSize)size;
+ (NSValue *)valueWithRect:(CGRect)rect;
@property (readonly) CGPoint pointValue;
@property (readonly) CGSize sizeValue;
@property (readonly) CGRect rectValue;
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

/* NSDateComponents (bridging de DateComponents) e NSFileHandle (FileHandle.write(contentsOf:)): o helper "Computer Use"
 * do Grok Bot; implementados em userland/libsystem/foundation_extra.c e foundation_more.c */
@interface NSDateComponents : NSObject <NSCopying>
@property NSInteger era;
@property NSInteger year;
@property NSInteger month;
@property NSInteger day;
@property NSInteger hour;
@property NSInteger minute;
@property NSInteger second;
@property NSInteger nanosecond;
@property NSInteger weekday;
@property NSInteger weekOfYear;
@property NSInteger yearForWeekOfYear;
@property (nullable, copy) NSTimeZone *timeZone;
@end

__attribute__((swift_name("FileHandle")))
@interface NSFileHandle : NSObject <NSSecureCoding>
- (void)writeData:(NSData *)data;
@property (readonly) int fileDescriptor;
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
NS_SWIFT_NAME(URLSessionTask)
@interface NSURLSessionTask : NSObject
- (void)resume;
- (void)cancel;
- (void)suspend;
@end
NS_SWIFT_NAME(URLSessionDataTask)
@interface NSURLSessionDataTask : NSURLSessionTask
@end
NS_SWIFT_NAME(URLSession)
@interface NSURLSession : NSObject
@property (class, readonly, strong) NSURLSession *sharedSession;
- (NSURLSessionDataTask *)dataTaskWithRequest:(NSURLRequest *)request completionHandler:(void (^)(NSData * _Nullable data, NSURLResponse * _Nullable response, NSError * _Nullable error))completionHandler;
- (NSURLSessionDataTask *)dataTaskWithURL:(NSURL *)url completionHandler:(void (^)(NSData * _Nullable data, NSURLResponse * _Nullable response, NSError * _Nullable error))completionHandler;
@end

/* ---- run loop, timer, centro de notificações e filas de operação (classes do libSystem: foundation.c,
 * fdn_operation.c), com os nomes Swift da Apple */
typedef double NSTimeInterval;
typedef NSString *NSRunLoopMode NS_TYPED_EXTENSIBLE_ENUM NS_SWIFT_NAME(RunLoop.Mode);
FOUNDATION_EXPORT NSRunLoopMode const NSDefaultRunLoopMode NS_SWIFT_NAME(RunLoop.Mode.default);
FOUNDATION_EXPORT NSRunLoopMode const NSRunLoopCommonModes NS_SWIFT_NAME(RunLoop.Mode.common);

NS_SWIFT_NAME(Timer)
@interface NSTimer : NSObject
+ (NSTimer *)timerWithTimeInterval:(NSTimeInterval)interval repeats:(BOOL)repeats block:(void (^)(NSTimer *timer))block;
+ (NSTimer *)scheduledTimerWithTimeInterval:(NSTimeInterval)interval repeats:(BOOL)repeats block:(void (^)(NSTimer *timer))block;
+ (NSTimer *)timerWithTimeInterval:(NSTimeInterval)ti target:(id)aTarget selector:(SEL)aSelector userInfo:(nullable id)userInfo repeats:(BOOL)yesOrNo;
+ (NSTimer *)scheduledTimerWithTimeInterval:(NSTimeInterval)ti target:(id)aTarget selector:(SEL)aSelector userInfo:(nullable id)userInfo repeats:(BOOL)yesOrNo;
- (instancetype)initWithFireDate:(NSDate *)date interval:(NSTimeInterval)interval repeats:(BOOL)repeats block:(void (^)(NSTimer *timer))block;
- (void)fire;
- (void)invalidate;
@property (copy) NSDate *fireDate;
@property (readonly) NSTimeInterval timeInterval;
@property NSTimeInterval tolerance;
@property (readonly, getter=isValid) BOOL valid;
@property (nullable, readonly, retain) id userInfo;
@end

NS_SWIFT_NAME(RunLoop)
@interface NSRunLoop : NSObject
@property (class, readonly, strong) NSRunLoop *currentRunLoop;
@property (class, readonly, strong) NSRunLoop *mainRunLoop;
@property (nullable, readonly, copy) NSRunLoopMode currentMode;
- (void)addTimer:(NSTimer *)timer forMode:(NSRunLoopMode)mode;
- (void)run;
- (void)runUntilDate:(NSDate *)limitDate NS_SWIFT_NAME(run(until:));
- (BOOL)runMode:(NSRunLoopMode)mode beforeDate:(NSDate *)limitDate NS_SWIFT_NAME(run(mode:before:));
- (void)performBlock:(void (^)(void))block;
- (void)performInModes:(NSArray<NSRunLoopMode> *)modes block:(void (^)(void))block;
@end

@class NSOperationQueue;
NS_SWIFT_NAME(NotificationCenter)
@interface NSNotificationCenter : NSObject
@property (class, readonly, strong) NSNotificationCenter *defaultCenter;
- (void)addObserver:(id)observer selector:(SEL)aSelector name:(nullable NSNotificationName)aName object:(nullable id)anObject;
- (id<NSObject>)addObserverForName:(nullable NSNotificationName)name object:(nullable id)obj queue:(nullable NSOperationQueue *)queue usingBlock:(void (^)(NSNotification *notification))block;
- (void)postNotification:(NSNotification *)notification;
- (void)postNotificationName:(NSNotificationName)aName object:(nullable id)anObject;
- (void)postNotificationName:(NSNotificationName)aName object:(nullable id)anObject userInfo:(nullable NSDictionary *)aUserInfo;
- (void)removeObserver:(id)observer;
- (void)removeObserver:(id)observer name:(nullable NSNotificationName)aName object:(nullable id)anObject;
@end

typedef NS_ENUM(NSInteger, NSOperationQueuePriority) {
    NSOperationQueuePriorityVeryLow = -8L, NSOperationQueuePriorityLow = -4L, NSOperationQueuePriorityNormal = 0,
    NSOperationQueuePriorityHigh = 4, NSOperationQueuePriorityVeryHigh = 8
};
typedef NS_ENUM(NSInteger, NSQualityOfService) {
    NSQualityOfServiceUserInteractive = 0x21, NSQualityOfServiceUserInitiated = 0x19, NSQualityOfServiceUtility = 0x11,
    NSQualityOfServiceBackground = 0x09, NSQualityOfServiceDefault = -1
};

NS_SWIFT_NAME(Operation)
@interface NSOperation : NSObject
- (void)start;
- (void)main;
- (void)cancel;
- (void)addDependency:(NSOperation *)op;
- (void)removeDependency:(NSOperation *)op;
- (void)waitUntilFinished;
@property (readonly, getter=isCancelled) BOOL cancelled;
@property (readonly, getter=isExecuting) BOOL executing;
@property (readonly, getter=isFinished) BOOL finished;
@property (readonly, getter=isAsynchronous) BOOL asynchronous;
@property (readonly, getter=isReady) BOOL ready;
@property (readonly, copy) NSArray<NSOperation *> *dependencies;
@property NSOperationQueuePriority queuePriority;
@property NSQualityOfService qualityOfService;
@property (nullable, copy) void (^completionBlock)(void);
@property (nullable, copy) NSString *name;
@end

NS_SWIFT_NAME(BlockOperation)
@interface NSBlockOperation : NSOperation
+ (instancetype)blockOperationWithBlock:(void (^)(void))block;
- (void)addExecutionBlock:(void (^)(void))block;
@property (readonly, copy) NSArray<void (^)(void)> *executionBlocks;
@end

NS_SWIFT_NAME(OperationQueue)
@interface NSOperationQueue : NSObject
@property (class, readonly, strong) NSOperationQueue *mainQueue;
@property (class, readonly, strong, nullable) NSOperationQueue *currentQueue;
- (void)addOperation:(NSOperation *)op;
- (void)addOperations:(NSArray<NSOperation *> *)ops waitUntilFinished:(BOOL)wait;
- (void)addOperationWithBlock:(void (^)(void))block;
- (void)addBarrierBlock:(void (^)(void))barrier;
- (void)cancelAllOperations;
- (void)waitUntilAllOperationsAreFinished;
@property (readonly, copy) NSArray<NSOperation *> *operations;
@property (readonly) NSUInteger operationCount;
@property NSInteger maxConcurrentOperationCount;
@property (getter=isSuspended) BOOL suspended;
@property (nullable, copy) NSString *name;
@property NSQualityOfService qualityOfService;
@property (nullable, assign) dispatch_queue_t underlyingQueue;
@end
FOUNDATION_EXPORT const NSInteger NSOperationQueueDefaultMaxConcurrentOperationCount;

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

/* ---- NSScanner do libSystem (fdn_scanner.c): scanLocation em UTF-16, como no macOS */
__attribute__((swift_name("Scanner")))
@interface NSScanner : NSObject <NSCopying>
+ (instancetype)scannerWithString:(NSString *)string;
+ (id)localizedScannerWithString:(NSString *)string;
- (instancetype)initWithString:(NSString *)string;
@property (readonly, copy) NSString *string;
@property NSUInteger scanLocation;
@property (nullable, copy) NSCharacterSet *charactersToBeSkipped NS_REFINED_FOR_SWIFT;
@property BOOL caseSensitive;
@property (nullable, retain) id locale;
@property (readonly, getter=isAtEnd) BOOL atEnd;
- (BOOL)scanInt:(nullable int *)result NS_SWIFT_NAME(scanInt32(_:));
- (BOOL)scanInteger:(nullable NSInteger *)result NS_SWIFT_NAME(scanInt(_:));
- (BOOL)scanLongLong:(nullable long long *)result NS_SWIFT_NAME(scanInt64(_:));
- (BOOL)scanUnsignedLongLong:(nullable unsigned long long *)result NS_SWIFT_NAME(scanUnsignedLongLong(_:));
- (BOOL)scanFloat:(nullable float *)result;
- (BOOL)scanDouble:(nullable double *)result;
- (BOOL)scanHexInt:(nullable unsigned *)result NS_SWIFT_NAME(scanHexInt32(_:));
- (BOOL)scanHexLongLong:(nullable unsigned long long *)result NS_SWIFT_NAME(scanHexInt64(_:));
- (BOOL)scanHexFloat:(nullable float *)result;
- (BOOL)scanHexDouble:(nullable double *)result;
/* NSDecimal *: o Decimal do Swift tem o mesmo layout (Scanner.swift expõe scanDecimal(_:)) */
- (BOOL)scanDecimal:(nullable void *)dcm NS_REFINED_FOR_SWIFT;
- (BOOL)scanString:(NSString *)string intoString:(NSString * _Nullable * _Nullable)result NS_SWIFT_NAME(scanString(_:into:));
- (BOOL)scanCharactersFromSet:(NSCharacterSet *)set intoString:(NSString * _Nullable * _Nullable)result NS_SWIFT_NAME(scanCharacters(from:into:));
- (BOOL)scanUpToString:(NSString *)string intoString:(NSString * _Nullable * _Nullable)result NS_SWIFT_NAME(scanUpTo(_:into:));
- (BOOL)scanUpToCharactersFromSet:(NSCharacterSet *)set intoString:(NSString * _Nullable * _Nullable)result NS_SWIFT_NAME(scanUpToCharacters(from:into:));
/* do mOS (o Scanner.swift usa): posição depois de pular charactersToBeSkipped, sem mover */
- (NSUInteger)_mos_skippedLocation;
@end

/* ---- classes do libSystem para as pontes de URLComponents, URLQueryItem, Calendar e CharacterSet */
@interface NSURLQueryItem : NSObject <NSCopying, NSSecureCoding>
+ (instancetype)queryItemWithName:(NSString *)name value:(nullable NSString *)value;
@property (readonly) NSString *name;
@property (nullable, readonly) NSString *value;
@end

@interface NSURLComponents : NSObject <NSCopying>
+ (nullable instancetype)componentsWithString:(NSString *)URLString;
@property (nullable, copy) NSString *string;
@end

typedef NS_OPTIONS(NSUInteger, NSCalendarUnit) {
    NSCalendarUnitEra = (1UL << 1),
    NSCalendarUnitYear = (1UL << 2),
    NSCalendarUnitMonth = (1UL << 3),
    NSCalendarUnitDay = (1UL << 4),
    NSCalendarUnitHour = (1UL << 5),
    NSCalendarUnitMinute = (1UL << 6),
    NSCalendarUnitSecond = (1UL << 7),
    NSCalendarUnitWeekday = (1UL << 9),
    NSCalendarUnitWeekdayOrdinal = (1UL << 10),
    NSCalendarUnitQuarter = (1UL << 11),
    NSCalendarUnitWeekOfMonth = (1UL << 12),
    NSCalendarUnitWeekOfYear = (1UL << 13),
    NSCalendarUnitYearForWeekOfYear = (1UL << 14),
    NSCalendarUnitNanosecond = (1UL << 15),
    NSCalendarUnitDayOfYear = (1UL << 16),
    NSCalendarUnitCalendar = (1UL << 20),
    NSCalendarUnitTimeZone = (1UL << 21),
} NS_SWIFT_NAME(NSCalendar.Unit);

@interface NSCalendar : NSObject <NSCopying, NSSecureCoding>
@property (class, readonly, copy) NSCalendar *currentCalendar;
@property (class, readonly, strong) NSCalendar *autoupdatingCurrentCalendar;
+ (nullable NSCalendar *)calendarWithIdentifier:(NSString *)calendarIdentifierConstant;
- (nullable instancetype)initWithCalendarIdentifier:(NSString *)ident;
@property (readonly, copy) NSString *calendarIdentifier;
@property (copy) NSTimeZone *timeZone;
@end

typedef unsigned int UTF32Char;
@interface NSCharacterSet : NSObject <NSCopying, NSMutableCopying, NSSecureCoding>
+ (instancetype)characterSetWithBitmapRepresentation:(NSData *)data;
@property (class, readonly, copy) NSCharacterSet *whitespaceAndNewlineCharacterSet;
- (BOOL)characterIsMember:(unichar)aCharacter;
- (BOOL)longCharacterIsMember:(UTF32Char)theLongChar;
@property (readonly, copy) NSData *bitmapRepresentation;
@end

/* ---- NSPathUtilities: funções C do libSystem (foundation.c, foundation_more.c) */
FOUNDATION_EXPORT NSString *NSUserName(void);
FOUNDATION_EXPORT NSString *NSFullUserName(void);
FOUNDATION_EXPORT NSString *NSHomeDirectory(void);
FOUNDATION_EXPORT NSString * _Nullable NSHomeDirectoryForUser(NSString * _Nullable userName);
FOUNDATION_EXPORT NSString *NSTemporaryDirectory(void);

NS_ASSUME_NONNULL_END
