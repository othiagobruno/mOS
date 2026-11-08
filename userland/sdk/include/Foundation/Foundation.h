/*
 * mOS SDK — Foundation (subconjunto da API da Apple implementado pelo libSystem do mOS).
 * Apps compilados com estes headers rodam no mOS e usam os mesmos símbolos do macOS.
 */
#pragma once
#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#if __has_include(<stdio.h>)   /* como no macOS, Foundation já traz a libc básica */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#endif

#ifndef IBOutlet
#define IBOutlet
#define IBOutletCollection(c)
#define IBAction void
#define IBInspectable
#define IB_DESIGNABLE
#endif
typedef double CGFloat;
typedef long NSInteger;
typedef unsigned long NSUInteger;
typedef double NSTimeInterval;
#if defined(__OBJC_BOOL_IS_BOOL) && __OBJC_BOOL_IS_BOOL   /* ObjC no arm64 da Apple: BOOL é bool (como o objc/objc.h) */
#ifdef __cplusplus
typedef bool BOOL;
#else
typedef _Bool BOOL;
#endif
#else
typedef signed char BOOL;
#endif
#ifndef YES
#define YES ((BOOL)1)
#define NO ((BOOL)0)
#endif
#ifndef nil
#define nil ((void *)0)
#define Nil ((Class)0)
#endif
#define NSNotFound ((NSInteger)0x7fffffffffffffffL)
#define NS_ASSUME_NONNULL_BEGIN
#define NS_ASSUME_NONNULL_END
#define NS_ENUM(T, N) enum N : T N; enum N : T
#define NS_OPTIONS(T, N) enum N : T N; enum N : T
#define FOUNDATION_EXPORT extern
#define NSUTF8StringEncoding 4

typedef struct CGPoint { CGFloat x, y; } CGPoint;
typedef struct CGSize { CGFloat width, height; } CGSize;
typedef struct CGRect { CGPoint origin; CGSize size; } CGRect;
typedef CGPoint NSPoint;
typedef CGSize NSSize;
typedef CGRect NSRect;
typedef struct _NSRange { NSUInteger location, length; } NSRange;
static inline NSRect NSMakeRect(CGFloat x, CGFloat y, CGFloat w, CGFloat h) { NSRect r = {{x, y}, {w, h}}; return r; }
static inline NSPoint NSMakePoint(CGFloat x, CGFloat y) { NSPoint p = {x, y}; return p; }
static inline NSSize NSMakeSize(CGFloat w, CGFloat h) { NSSize s = {w, h}; return s; }
static inline NSRange NSMakeRange(NSUInteger l, NSUInteger n) { NSRange r = {l, n}; return r; }
static inline CGRect CGRectMake(CGFloat x, CGFloat y, CGFloat w, CGFloat h) { CGRect r = {{x, y}, {w, h}}; return r; }
static inline CGPoint CGPointMake(CGFloat x, CGFloat y) { CGPoint p = {x, y}; return p; }
static inline CGSize CGSizeMake(CGFloat w, CGFloat h) { CGSize s = {w, h}; return s; }
static inline CGFloat NSWidth(NSRect r) { return r.size.width; }
static inline CGFloat NSHeight(NSRect r) { return r.size.height; }
static inline CGFloat NSMinX(NSRect r) { return r.origin.x; }
static inline CGFloat NSMinY(NSRect r) { return r.origin.y; }
static inline CGFloat NSMaxX(NSRect r) { return r.origin.x + r.size.width; }
static inline CGFloat NSMaxY(NSRect r) { return r.origin.y + r.size.height; }
static inline CGFloat NSMidX(NSRect r) { return r.origin.x + r.size.width / 2; }
static inline CGFloat NSMidY(NSRect r) { return r.origin.y + r.size.height / 2; }
static inline NSRect NSInsetRect(NSRect r, CGFloat dx, CGFloat dy) { return NSMakeRect(r.origin.x + dx, r.origin.y + dy, r.size.width - 2 * dx, r.size.height - 2 * dy); }
static inline BOOL NSPointInRect(NSPoint p, NSRect r) { return p.x >= r.origin.x && p.y >= r.origin.y && p.x < r.origin.x + r.size.width && p.y < r.origin.y + r.size.height; }
#define CGRectGetWidth NSWidth
#define CGRectGetHeight NSHeight
#define CGRectGetMidX NSMidX
#define CGRectGetMidY NSMidY
#define CGRectInset NSInsetRect
#define NSZeroRect NSMakeRect(0, 0, 0, 0)
#define CGRectZero NSMakeRect(0, 0, 0, 0)

@class NSString, NSArray, NSDictionary, NSError, NSDate, NSTimer, NSNotification, NSMutableArray;
@protocol NSObject
@end
@protocol NSCopying
@end
@protocol NSFastEnumeration
@end

__attribute__((objc_root_class))
@interface NSObject <NSObject> { Class isa; }
+ (instancetype)alloc;
+ (instancetype)new;
+ (Class)class;
+ (NSString *)description;
+ (BOOL)instancesRespondToSelector:(SEL)s;
+ (BOOL)isSubclassOfClass:(Class)c;
+ (void)initialize;
+ (void)load;
- (instancetype)init;
- (void)dealloc;
- (instancetype)retain;
- (oneway void)release;
- (instancetype)autorelease;
- (Class)class;
- (Class)superclass;
- (instancetype)self;
- (BOOL)isKindOfClass:(Class)c;
- (BOOL)isMemberOfClass:(Class)c;
- (BOOL)respondsToSelector:(SEL)s;
- (id)performSelector:(SEL)s;
- (id)performSelector:(SEL)s withObject:(id)o;
- (id)performSelector:(SEL)s withObject:(id)a withObject:(id)b;
- (void)performSelectorOnMainThread:(SEL)s withObject:(id)o waitUntilDone:(BOOL)w;
- (NSString *)description;
- (NSUInteger)hash;
- (BOOL)isEqual:(id)o;
- (id)copy;
- (id)mutableCopy;
- (id)valueForKey:(NSString *)k;
- (void)setValue:(id)v forKey:(NSString *)k;
@end

@interface NSString : NSObject <NSCopying>
+ (instancetype)string;
+ (instancetype)stringWithUTF8String:(const char *)s;
+ (instancetype)stringWithString:(NSString *)s;
+ (instancetype)stringWithFormat:(NSString *)fmt, ... __attribute__((format(__NSString__, 1, 2)));
+ (instancetype)stringWithContentsOfFile:(NSString *)p encoding:(NSUInteger)e error:(NSError **)err;
- (instancetype)initWithUTF8String:(const char *)s;
- (instancetype)initWithFormat:(NSString *)fmt, ... __attribute__((format(__NSString__, 1, 2)));
- (instancetype)initWithString:(NSString *)s;
@property (readonly) NSUInteger length;
@property (readonly) const char *UTF8String;
@property (readonly) int intValue;
@property (readonly) NSInteger integerValue;
@property (readonly) double doubleValue;
@property (readonly) float floatValue;
@property (readonly) BOOL boolValue;
@property (readonly, copy) NSString *uppercaseString;
@property (readonly, copy) NSString *lowercaseString;
@property (readonly, copy) NSString *capitalizedString;
@property (readonly, copy) NSString *lastPathComponent;
@property (readonly, copy) NSString *pathExtension;
@property (readonly, copy) NSString *stringByDeletingLastPathComponent;
- (unsigned short)characterAtIndex:(NSUInteger)i;
- (BOOL)isEqualToString:(NSString *)s;
- (NSInteger)compare:(NSString *)s;
- (NSInteger)caseInsensitiveCompare:(NSString *)s;
- (BOOL)hasPrefix:(NSString *)s;
- (BOOL)hasSuffix:(NSString *)s;
- (BOOL)containsString:(NSString *)s;
- (NSRange)rangeOfString:(NSString *)s;
- (NSString *)stringByAppendingString:(NSString *)s;
- (NSString *)stringByAppendingFormat:(NSString *)fmt, ... __attribute__((format(__NSString__, 1, 2)));
- (NSString *)stringByAppendingPathComponent:(NSString *)s;
- (NSString *)substringFromIndex:(NSUInteger)i;
- (NSString *)substringToIndex:(NSUInteger)i;
- (NSString *)substringWithRange:(NSRange)r;
- (NSString *)stringByReplacingOccurrencesOfString:(NSString *)a withString:(NSString *)b;
- (NSArray *)componentsSeparatedByString:(NSString *)s;
- (BOOL)writeToFile:(NSString *)p atomically:(BOOL)a encoding:(NSUInteger)e error:(NSError **)err;
@end

@interface NSMutableString : NSString
+ (instancetype)stringWithCapacity:(NSUInteger)n;
- (void)appendString:(NSString *)s;
- (void)appendFormat:(NSString *)fmt, ... __attribute__((format(__NSString__, 1, 2)));
- (void)setString:(NSString *)s;
- (void)insertString:(NSString *)s atIndex:(NSUInteger)i;
- (void)deleteCharactersInRange:(NSRange)r;
@end

@interface NSNumber : NSObject <NSCopying>
+ (NSNumber *)numberWithInt:(int)v;
+ (NSNumber *)numberWithInteger:(NSInteger)v;
+ (NSNumber *)numberWithLongLong:(long long)v;
+ (NSNumber *)numberWithUnsignedInteger:(NSUInteger)v;
+ (NSNumber *)numberWithDouble:(double)v;
+ (NSNumber *)numberWithFloat:(float)v;
+ (NSNumber *)numberWithBool:(BOOL)v;
@property (readonly) int intValue;
@property (readonly) NSInteger integerValue;
@property (readonly) long long longLongValue;
@property (readonly) double doubleValue;
@property (readonly) float floatValue;
@property (readonly) BOOL boolValue;
@property (readonly, copy) NSString *stringValue;
- (BOOL)isEqualToNumber:(NSNumber *)n;
- (NSInteger)compare:(NSNumber *)n;
@end

@interface NSArray<ObjectType> : NSObject <NSCopying, NSFastEnumeration>
+ (instancetype)array;
+ (instancetype)arrayWithObject:(ObjectType)o;
+ (instancetype)arrayWithObjects:(ObjectType)first, ... __attribute__((sentinel));
+ (instancetype)arrayWithObjects:(const ObjectType *)objs count:(NSUInteger)n;
+ (instancetype)arrayWithArray:(NSArray *)a;
@property (readonly) NSUInteger count;
@property (readonly) ObjectType firstObject;
@property (readonly) ObjectType lastObject;
- (ObjectType)objectAtIndex:(NSUInteger)i;
- (ObjectType)objectAtIndexedSubscript:(NSUInteger)i;
- (BOOL)containsObject:(ObjectType)o;
- (NSUInteger)indexOfObject:(ObjectType)o;
- (NSString *)componentsJoinedByString:(NSString *)s;
- (NSArray *)arrayByAddingObject:(ObjectType)o;
- (NSArray *)sortedArrayUsingSelector:(SEL)s;
- (NSArray *)sortedArrayUsingComparator:(NSInteger (^)(id a, id b))cmp;
- (void)enumerateObjectsUsingBlock:(void (^)(ObjectType obj, NSUInteger idx, BOOL *stop))b;
- (NSUInteger)countByEnumeratingWithState:(void *)st objects:(id *)buf count:(NSUInteger)n;
@end

@interface NSMutableArray<ObjectType> : NSArray<ObjectType>
+ (instancetype)arrayWithCapacity:(NSUInteger)n;
- (void)addObject:(ObjectType)o;
- (void)addObjectsFromArray:(NSArray *)a;
- (void)insertObject:(ObjectType)o atIndex:(NSUInteger)i;
- (void)removeObjectAtIndex:(NSUInteger)i;
- (void)removeLastObject;
- (void)removeAllObjects;
- (void)removeObject:(ObjectType)o;
- (void)replaceObjectAtIndex:(NSUInteger)i withObject:(ObjectType)o;
- (void)setObject:(ObjectType)o atIndexedSubscript:(NSUInteger)i;
- (void)sortUsingSelector:(SEL)s;
@end

@interface NSDictionary<KeyType, ObjectType> : NSObject <NSCopying, NSFastEnumeration>
+ (instancetype)dictionary;
+ (instancetype)dictionaryWithObjects:(const ObjectType *)v forKeys:(const KeyType *)k count:(NSUInteger)n;
+ (instancetype)dictionaryWithObject:(ObjectType)o forKey:(KeyType)k;
+ (instancetype)dictionaryWithObjectsAndKeys:(id)first, ... __attribute__((sentinel));
@property (readonly) NSUInteger count;
@property (readonly, copy) NSArray<KeyType> *allKeys;
@property (readonly, copy) NSArray<ObjectType> *allValues;
- (ObjectType)objectForKey:(KeyType)k;
- (ObjectType)objectForKeyedSubscript:(KeyType)k;
- (void)enumerateKeysAndObjectsUsingBlock:(void (^)(KeyType key, ObjectType obj, BOOL *stop))b;
- (NSUInteger)countByEnumeratingWithState:(void *)st objects:(id *)buf count:(NSUInteger)n;
@end

@interface NSMutableDictionary<KeyType, ObjectType> : NSDictionary<KeyType, ObjectType>
+ (instancetype)dictionaryWithCapacity:(NSUInteger)n;
- (void)setObject:(ObjectType)o forKey:(KeyType)k;
- (void)setObject:(ObjectType)o forKeyedSubscript:(KeyType)k;
- (void)removeObjectForKey:(KeyType)k;
- (void)removeAllObjects;
@end

@interface NSDate : NSObject
+ (instancetype)date;
+ (instancetype)dateWithTimeIntervalSinceNow:(NSTimeInterval)s;
+ (instancetype)dateWithTimeIntervalSince1970:(NSTimeInterval)s;
@property (readonly) NSTimeInterval timeIntervalSince1970;
@property (readonly) NSTimeInterval timeIntervalSinceNow;
- (NSTimeInterval)timeIntervalSinceDate:(NSDate *)d;
@end

@interface NSTimer : NSObject
+ (NSTimer *)scheduledTimerWithTimeInterval:(NSTimeInterval)t target:(id)target selector:(SEL)s userInfo:(id)info repeats:(BOOL)r;
+ (NSTimer *)scheduledTimerWithTimeInterval:(NSTimeInterval)t repeats:(BOOL)r block:(void (^)(NSTimer *timer))b;
- (void)invalidate;
@property (readonly, getter=isValid) BOOL valid;
@property (readonly) id userInfo;
@end

@interface NSRunLoop : NSObject
+ (NSRunLoop *)currentRunLoop;
+ (NSRunLoop *)mainRunLoop;
- (void)addTimer:(NSTimer *)t forMode:(NSString *)m;
- (void)run;
- (void)runUntilDate:(NSDate *)d;
@end

@interface NSNotification : NSObject
@property (readonly, copy) NSString *name;
@property (readonly) id object;
@property (readonly, copy) NSDictionary *userInfo;
@end

@interface NSNotificationCenter : NSObject
+ (NSNotificationCenter *)defaultCenter;
- (void)addObserver:(id)o selector:(SEL)s name:(NSString *)n object:(id)obj;
- (void)postNotificationName:(NSString *)n object:(id)obj;
- (void)postNotificationName:(NSString *)n object:(id)obj userInfo:(NSDictionary *)info;
- (void)removeObserver:(id)o;
@end

@interface NSProcessInfo : NSObject
+ (NSProcessInfo *)processInfo;
@property (readonly, copy) NSString *processName;
@property (readonly, copy) NSArray<NSString *> *arguments;
@property (readonly, copy) NSDictionary<NSString *, NSString *> *environment;
@property (readonly) int processIdentifier;
@property (readonly, copy) NSString *operatingSystemVersionString;
@end

@interface NSUserDefaults : NSObject
+ (NSUserDefaults *)standardUserDefaults;
- (id)objectForKey:(NSString *)k;
- (void)setObject:(id)o forKey:(NSString *)k;
- (BOOL)boolForKey:(NSString *)k;
- (NSInteger)integerForKey:(NSString *)k;
- (void)setBool:(BOOL)b forKey:(NSString *)k;
- (void)setInteger:(NSInteger)i forKey:(NSString *)k;
@end

@interface NSBundle : NSObject
+ (NSBundle *)mainBundle;
@property (readonly, copy) NSString *bundlePath;
@property (readonly, copy) NSString *resourcePath;
- (NSString *)pathForResource:(NSString *)name ofType:(NSString *)ext;
@end

@interface NSFileManager : NSObject
+ (NSFileManager *)defaultManager;
- (BOOL)fileExistsAtPath:(NSString *)p;
- (NSArray<NSString *> *)contentsOfDirectoryAtPath:(NSString *)p error:(NSError **)e;
@end

@interface NSThread : NSObject
+ (BOOL)isMainThread;
+ (void)sleepForTimeInterval:(NSTimeInterval)t;
@end

@interface NSAutoreleasePool : NSObject
- (void)drain;
@end

@interface NSError : NSObject
@property (readonly, copy) NSString *localizedDescription;
@end

FOUNDATION_EXPORT void NSLog(NSString *fmt, ...) __attribute__((format(__NSString__, 1, 2)));
FOUNDATION_EXPORT NSString *NSStringFromClass(Class c);
FOUNDATION_EXPORT Class NSClassFromString(NSString *s);
FOUNDATION_EXPORT NSString *NSStringFromSelector(SEL s);
FOUNDATION_EXPORT NSString *NSStringFromRect(NSRect r);
FOUNDATION_EXPORT NSString *NSStringFromSize(NSSize s);
FOUNDATION_EXPORT NSString *NSStringFromPoint(NSPoint p);
FOUNDATION_EXPORT SEL NSSelectorFromString(NSString *s);
FOUNDATION_EXPORT NSString *NSHomeDirectory(void);

@interface NSData : NSObject
+ (instancetype)dataWithContentsOfFile:(NSString *)p;
+ (instancetype)dataWithBytes:(const void *)b length:(NSUInteger)n;
- (instancetype)initWithContentsOfFile:(NSString *)p;
- (instancetype)initWithBytes:(const void *)b length:(NSUInteger)n;
@property (readonly) const void *bytes;
@property (readonly) NSUInteger length;
- (BOOL)writeToFile:(NSString *)p atomically:(BOOL)a;
@end

@interface NSIndexSet : NSObject
+ (instancetype)indexSetWithIndex:(NSUInteger)i;
@property (readonly) NSUInteger firstIndex;
@end
FOUNDATION_EXPORT NSString *const NSDefaultRunLoopMode;

@interface NSCoder : NSObject
@property (readonly) BOOL allowsKeyedCoding;
- (id)decodeObjectForKey:(NSString *)k;
- (id)decodeObjectOfClass:(Class)c forKey:(NSString *)k;
- (BOOL)decodeBoolForKey:(NSString *)k;
- (NSInteger)decodeIntegerForKey:(NSString *)k;
- (int)decodeIntForKey:(NSString *)k;
- (double)decodeDoubleForKey:(NSString *)k;
- (float)decodeFloatForKey:(NSString *)k;
- (BOOL)containsValueForKey:(NSString *)k;
@end
@protocol NSCoding
- (instancetype)initWithCoder:(NSCoder *)c;
@end
@interface NSBundle (mOSInfo)
@property (readonly, copy) NSDictionary<NSString *, id> *infoDictionary;
- (id)objectForInfoDictionaryKey:(NSString *)k;
- (NSString *)pathForResource:(NSString *)n ofType:(NSString *)t inDirectory:(NSString *)d;
@end
@interface NSDictionary (mOSPlist)
+ (instancetype)dictionaryWithContentsOfFile:(NSString *)p;
@end
@interface NSPropertyListSerialization : NSObject
+ (id)propertyListWithData:(NSData *)d options:(NSUInteger)o format:(NSUInteger *)f error:(NSError **)e;
@end