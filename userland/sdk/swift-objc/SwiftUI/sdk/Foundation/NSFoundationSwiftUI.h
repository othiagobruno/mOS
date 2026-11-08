/* mOS SDK (variante com interop ObjC) — classes do Foundation do libSystem que o SwiftUI.framework usa e que o
 * Foundation.h da variante (userland/sdk/swift-objc/sdk/Foundation) ainda não declara, com os nomes Swift da Apple
 * (Bundle, Formatter, NumberFormatter, RunLoop, UserDefaults...). O tools\build-swiftui-objc.ps1 inclui este arquivo
 * no fim da cópia do Foundation.h usada pelo SwiftUI. Implementações: userland/libsystem (foundation*.c, appkit.c). */
#pragma once
#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@class NSTimer;

@interface NSCoder (NSCoderSwiftUIMOS)
- (void)encodeObject:(nullable id)object forKey:(NSString *)key;
- (void)encodeInteger:(NSInteger)value forKey:(NSString *)key;
- (void)encodeBool:(BOOL)value forKey:(NSString *)key;
- (void)encodeDouble:(double)value forKey:(NSString *)key;
- (NSInteger)decodeIntegerForKey:(NSString *)key;
- (BOOL)decodeBoolForKey:(NSString *)key;
- (double)decodeDoubleForKey:(NSString *)key;
- (BOOL)containsValueForKey:(NSString *)key;
@end

NS_SWIFT_NAME(Bundle) @interface NSBundle : NSObject
@property (class, readonly, strong) NSBundle *mainBundle;
+ (nullable instancetype)bundleWithPath:(NSString *)path;
+ (nullable instancetype)bundleWithURL:(NSURL *)url;
+ (NSBundle *)bundleForClass:(Class)aClass;
+ (nullable NSBundle *)bundleWithIdentifier:(NSString *)identifier;
- (nullable instancetype)initWithPath:(NSString *)path NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)initWithURL:(NSURL *)url;
@property (readonly, copy) NSString *bundlePath;
@property (readonly, copy) NSURL *bundleURL;
@property (nullable, readonly, copy) NSString *resourcePath;
@property (nullable, readonly, copy) NSURL *resourceURL;
@property (nullable, readonly, copy) NSString *bundleIdentifier;
@property (nullable, readonly, copy) NSDictionary<NSString *, id> *infoDictionary;
@property (readonly, copy) NSArray<NSString *> *preferredLocalizations;
- (nullable id)objectForInfoDictionaryKey:(NSString *)key;
- (nullable NSString *)pathForResource:(nullable NSString *)name ofType:(nullable NSString *)ext;
- (nullable NSURL *)URLForResource:(nullable NSString *)name withExtension:(nullable NSString *)ext;
- (nullable NSURL *)URLForResource:(nullable NSString *)name withExtension:(nullable NSString *)ext subdirectory:(nullable NSString *)subpath;
- (NSString *)localizedStringForKey:(NSString *)key value:(nullable NSString *)value table:(nullable NSString *)tableName;
@end

NS_SWIFT_NAME(Formatter) @interface NSFormatter : NSObject <NSCopying, NSCoding>
- (nullable NSString *)stringForObjectValue:(nullable id)obj;
- (nullable NSString *)editingStringForObjectValue:(id)obj;
- (BOOL)getObjectValue:(out id _Nullable * _Nullable)obj forString:(NSString *)string errorDescription:(out NSString * _Nullable * _Nullable)error;
@end

typedef NS_ENUM(NSUInteger, NSNumberFormatterStyle) {
    NSNumberFormatterNoStyle NS_SWIFT_NAME(none) = 0, NSNumberFormatterDecimalStyle NS_SWIFT_NAME(decimal) = 1,
    NSNumberFormatterCurrencyStyle NS_SWIFT_NAME(currency) = 2, NSNumberFormatterPercentStyle NS_SWIFT_NAME(percent) = 3,
    NSNumberFormatterScientificStyle NS_SWIFT_NAME(scientific) = 4, NSNumberFormatterSpellOutStyle NS_SWIFT_NAME(spellOut) = 5,
    NSNumberFormatterOrdinalStyle NS_SWIFT_NAME(ordinal) = 6, NSNumberFormatterCurrencyISOCodeStyle NS_SWIFT_NAME(currencyISOCode) = 8,
    NSNumberFormatterCurrencyPluralStyle NS_SWIFT_NAME(currencyPlural) = 9, NSNumberFormatterCurrencyAccountingStyle NS_SWIFT_NAME(currencyAccounting) = 10,
} NS_SWIFT_NAME(NumberFormatter.Style);
NS_SWIFT_NAME(NumberFormatter) @interface NSNumberFormatter : NSFormatter
+ (NSString *)localizedStringFromNumber:(NSNumber *)num numberStyle:(NSNumberFormatterStyle)nstyle;
@property (null_resettable, copy) NSLocale *locale;
- (nullable NSString *)stringFromNumber:(NSNumber *)number;
- (nullable NSNumber *)numberFromString:(NSString *)string;
@property NSNumberFormatterStyle numberStyle;
@property NSUInteger maximumFractionDigits;
@property NSUInteger minimumFractionDigits;
@end

NS_SWIFT_NAME(DateFormatter) @interface NSDateFormatter : NSFormatter
- (NSString *)stringFromDate:(NSDate *)date;
- (nullable NSDate *)dateFromString:(NSString *)string;
@property (null_resettable, copy) NSString *dateFormat;
@property (null_resettable, copy) NSLocale *locale;
@property (null_resettable, copy) NSTimeZone *timeZone;
@property (null_resettable, copy) id calendar;
@property NSUInteger dateStyle;
@property NSUInteger timeStyle;
@end

NS_SWIFT_NAME(DateIntervalFormatter) @interface NSDateIntervalFormatter : NSFormatter
@property (null_resettable, copy) NSLocale *locale;
@property (null_resettable, copy) NSTimeZone *timeZone;
@property (null_resettable, copy) id calendar;
@end
NS_SWIFT_NAME(MassFormatter) @interface NSMassFormatter : NSFormatter
@property (copy) NSNumberFormatter *numberFormatter;
@end
NS_SWIFT_NAME(LengthFormatter) @interface NSLengthFormatter : NSFormatter
@property (copy) NSNumberFormatter *numberFormatter;
@end
NS_SWIFT_NAME(EnergyFormatter) @interface NSEnergyFormatter : NSFormatter
@property (copy) NSNumberFormatter *numberFormatter;
@end
NS_SWIFT_NAME(ISO8601DateFormatter) @interface NSISO8601DateFormatter : NSFormatter
- (NSString *)stringFromDate:(NSDate *)date;
- (nullable NSDate *)dateFromString:(NSString *)string;
@property (null_resettable, copy) NSTimeZone *timeZone;
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
    NSCalendarUnitCalendar = (1UL << 20),
    NSCalendarUnitTimeZone = (1UL << 21),
} NS_SWIFT_NAME(NSCalendar.Unit);

@interface NSCalendar : NSObject <NSCopying, NSSecureCoding>
@property (class, readonly, copy) NSCalendar *currentCalendar;
@end

typedef NSString *NSRunLoopMode NS_SWIFT_NAME(RunLoop.Mode) __attribute__((swift_wrapper(struct)));
FOUNDATION_EXPORT NSRunLoopMode const NSDefaultRunLoopMode NS_SWIFT_NAME(RunLoop.Mode.default);
FOUNDATION_EXPORT NSRunLoopMode const NSRunLoopCommonModes NS_SWIFT_NAME(RunLoop.Mode.common);
NS_SWIFT_NAME(RunLoop) @interface NSRunLoop : NSObject
- (void)addTimer:(NSTimer *)timer forMode:(NSRunLoopMode)mode;
- (void)performInModes:(NSArray<NSRunLoopMode> *)modes block:(void (^)(void))block;
- (void)performBlock:(void (^)(void))block;
@property (nullable, readonly, copy) NSRunLoopMode currentMode;
- (BOOL)runMode:(NSRunLoopMode)mode beforeDate:(NSDate *)limitDate NS_SWIFT_NAME(run(mode:before:));
@property (class, readonly, strong) NSRunLoop *currentRunLoop;
@property (class, readonly, strong) NSRunLoop *mainRunLoop;
- (void)run;
- (void)runUntilDate:(NSDate *)limitDate;
@end

NS_SWIFT_NAME(UserDefaults) @interface NSUserDefaults : NSObject
@property (class, readonly, strong) NSUserDefaults *standardUserDefaults;
- (nullable instancetype)initWithSuiteName:(nullable NSString *)suitename;
- (nullable id)objectForKey:(NSString *)defaultName;
- (void)setObject:(nullable id)value forKey:(NSString *)defaultName;
- (BOOL)boolForKey:(NSString *)defaultName;
- (NSInteger)integerForKey:(NSString *)defaultName;
- (double)doubleForKey:(NSString *)defaultName;
- (nullable NSString *)stringForKey:(NSString *)defaultName;
- (void)setBool:(BOOL)value forKey:(NSString *)defaultName;
- (void)setInteger:(NSInteger)value forKey:(NSString *)defaultName;
- (void)removeObjectForKey:(NSString *)defaultName;
- (void)registerDefaults:(NSDictionary<NSString *, id> *)registrationDictionary;
@end

@interface NSKeyedArchiver : NSCoder
- (instancetype)initRequiringSecureCoding:(BOOL)requiresSecureCoding;
@property (readonly, strong) NSData *encodedData;
- (void)finishEncoding;
+ (nullable NSData *)archivedDataWithRootObject:(id)object requiringSecureCoding:(BOOL)requiresSecureCoding error:(NSError **)error;
@end
@interface NSKeyedUnarchiver : NSCoder
- (nullable instancetype)initForReadingFromData:(NSData *)data error:(NSError **)error;
@property BOOL requiresSecureCoding;
- (void)finishDecoding;
+ (nullable id)unarchivedObjectOfClass:(Class)cls fromData:(NSData *)data error:(NSError **)error NS_REFINED_FOR_SWIFT;
+ (nullable id)unarchivedObjectOfClasses:(NSSet<Class> *)classes fromData:(NSData *)data error:(NSError **)error NS_REFINED_FOR_SWIFT;
+ (nullable id)unarchiveTopLevelObjectWithData:(NSData *)data error:(NSError **)error NS_SWIFT_NAME(unarchiveTopLevelObjectWithData(_:));
@end

typedef NS_OPTIONS(NSUInteger, NSJSONReadingOptions) {
    NSJSONReadingMutableContainers = (1UL << 0),
    NSJSONReadingMutableLeaves = (1UL << 1),
    NSJSONReadingFragmentsAllowed = (1UL << 2),
};
typedef NS_OPTIONS(NSUInteger, NSJSONWritingOptions) {
    NSJSONWritingPrettyPrinted = (1UL << 0),
    NSJSONWritingSortedKeys = (1UL << 1),
    NSJSONWritingFragmentsAllowed = (1UL << 2),
    NSJSONWritingWithoutEscapingSlashes = (1UL << 3),
};
NS_SWIFT_NAME(JSONSerialization) @interface NSJSONSerialization : NSObject
+ (BOOL)isValidJSONObject:(id)obj;
+ (nullable NSData *)dataWithJSONObject:(id)obj options:(NSJSONWritingOptions)opt error:(NSError **)error;
+ (nullable id)JSONObjectWithData:(NSData *)data options:(NSJSONReadingOptions)opt error:(NSError **)error;
@end

NS_SWIFT_NAME(Timer) @interface NSTimer : NSObject
+ (NSTimer *)scheduledTimerWithTimeInterval:(double)interval repeats:(BOOL)repeats block:(void (^)(NSTimer *timer))block;
+ (NSTimer *)timerWithTimeInterval:(double)interval repeats:(BOOL)repeats block:(void (^)(NSTimer *timer))block;
- (void)invalidate;
- (void)fire;
@property (readonly, getter=isValid) BOOL valid;
@end

@interface NSLock : NSObject
- (void)lock;
- (void)unlock;
- (BOOL)tryLock;
@end

NS_SWIFT_NAME(Thread) @interface NSThread : NSObject
@property (class, readonly, strong) NSThread *currentThread;
@property (class, readonly, strong) NSThread *mainThread;
@property (class, readonly) BOOL isMainThread;
@property (readonly) BOOL isMainThread;
@property (readonly, retain) NSMutableDictionary *threadDictionary;
+ (void)sleepForTimeInterval:(double)ti;
+ (void)sleepUntilDate:(NSDate *)date;
@end

NS_SWIFT_NAME(Progress) @interface NSProgress : NSObject
+ (nullable NSProgress *)currentProgress;
+ (NSProgress *)progressWithTotalUnitCount:(int64_t)unitCount;
- (instancetype)initWithParent:(nullable NSProgress *)parentProgressOrNil userInfo:(nullable NSDictionary *)userInfoOrNil NS_DESIGNATED_INITIALIZER;
@property int64_t totalUnitCount;
@property int64_t completedUnitCount;
@property (readonly) double fractionCompleted;
@property (readonly, getter=isIndeterminate) BOOL indeterminate;
@property (readonly, getter=isFinished) BOOL finished;
@property (getter=isCancellable) BOOL cancellable;
@property (getter=isPausable) BOOL pausable;
@property (readonly, getter=isCancelled) BOOL cancelled;
@property (readonly, getter=isPaused) BOOL paused;
@property (null_resettable, copy) NSString *localizedDescription;
@property (null_resettable, copy) NSString *localizedAdditionalDescription;
- (void)cancel;
- (void)pause;
- (void)resume;
@end

NS_ASSUME_NONNULL_END
