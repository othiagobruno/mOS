/*
 * mOS SDK (importador) — CFRunLoop. Os objetos de run loop vêm do libSystem (objetos ObjC do mOS), que o
 * ARC do Swift sem ObjC não sabe reter; por isso os tipos aqui NÃO seguem o padrão `struct __CFxxx *` que o
 * Swift reconhece como classe CF. O Swift reconhece também pelo NOME do typedef (CFRunLoopRef etc.), então os
 * typedefs aqui se chamam MOSCFRunLoopRef...: chegam ao Swift como OpaquePointer, e o Foundation do mOS define
 * `typealias CFRunLoop = MOSCFRunLoopRef` e afins. Implementação em userland/libsystem/cfrunloop.c.
 */
#pragma once
#include <CoreFoundation/CoreFoundation.h>

CF_EXTERN_C_BEGIN
CF_ASSUME_NONNULL_BEGIN

typedef struct MOSCFRunLoop *MOSCFRunLoopRef;
typedef struct MOSCFRunLoopObserver *MOSCFRunLoopObserverRef;
typedef struct MOSCFRunLoopTimer *MOSCFRunLoopTimerRef;
typedef CFStringRef CFRunLoopMode;

typedef CF_OPTIONS(CFOptionFlags, CFRunLoopActivity) {
    kCFRunLoopEntry = (1UL << 0),
    kCFRunLoopBeforeTimers = (1UL << 1),
    kCFRunLoopBeforeSources = (1UL << 2),
    kCFRunLoopBeforeWaiting = (1UL << 5),
    kCFRunLoopAfterWaiting = (1UL << 6),
    kCFRunLoopExit = (1UL << 7),
    kCFRunLoopAllActivities = 0x0FFFFFFFU
};
typedef CF_ENUM(SInt32, CFRunLoopRunResult) {
    kCFRunLoopRunFinished = 1, kCFRunLoopRunStopped = 2, kCFRunLoopRunTimedOut = 3, kCFRunLoopRunHandledSource = 4
};

typedef struct {
    CFIndex version;
    void *_Nullable info;
    const void *_Nullable (*_Nullable retain)(const void *_Nullable info);
    void (*_Nullable release)(const void *_Nullable info);
    CFStringRef _Nullable (*_Nullable copyDescription)(const void *_Nullable info);
} CFRunLoopObserverContext;
typedef void (*CFRunLoopObserverCallBack)(MOSCFRunLoopObserverRef observer, CFRunLoopActivity activity, void *_Nullable info);

CF_EXPORT const CFRunLoopMode kCFRunLoopDefaultMode;
CF_EXPORT const CFRunLoopMode kCFRunLoopCommonModes;

CF_EXPORT MOSCFRunLoopRef CFRunLoopGetCurrent(void);
CF_EXPORT MOSCFRunLoopRef CFRunLoopGetMain(void);
CF_EXPORT void CFRunLoopRun(void);
CF_EXPORT CFRunLoopRunResult CFRunLoopRunInMode(CFRunLoopMode mode, CFTimeInterval seconds, Boolean returnAfterSourceHandled);
CF_EXPORT void CFRunLoopStop(MOSCFRunLoopRef rl);
CF_EXPORT void CFRunLoopWakeUp(MOSCFRunLoopRef rl);
CF_EXPORT MOSCFRunLoopObserverRef _Nullable CFRunLoopObserverCreate(CFAllocatorRef _Nullable allocator, CFOptionFlags activities, Boolean repeats, CFIndex order,
                                                                 CFRunLoopObserverCallBack _Nullable callout, CFRunLoopObserverContext *_Nullable context);
CF_EXPORT void CFRunLoopAddObserver(MOSCFRunLoopRef rl, MOSCFRunLoopObserverRef observer, CFRunLoopMode mode);
CF_EXPORT void CFRunLoopRemoveObserver(MOSCFRunLoopRef rl, MOSCFRunLoopObserverRef observer, CFRunLoopMode mode);
CF_EXPORT Boolean CFRunLoopContainsObserver(MOSCFRunLoopRef rl, MOSCFRunLoopObserverRef observer, CFRunLoopMode mode);
CF_EXPORT void CFRunLoopObserverInvalidate(MOSCFRunLoopObserverRef observer);

CF_ASSUME_NONNULL_END
CF_EXTERN_C_END
