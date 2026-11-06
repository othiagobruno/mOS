/*
 * mOS SDK — CFRunLoop para as deps do OpenSwiftUI (o CoreFoundation do swift-corelibs que vem com elas não
 * tem). Implementação em userland/libsystem/cfrunloop.c; mesma ABI do macOS.
 */
#pragma once
#include <CoreFoundation/CFBase.h>

CF_EXTERN_C_BEGIN

typedef struct __CFRunLoop *CFRunLoopRef;
typedef struct __CFRunLoopObserver *CFRunLoopObserverRef;
typedef struct __CFRunLoopTimer *CFRunLoopTimerRef;
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

typedef struct {
    CFIndex version;
    void *info;
    const void *(*retain)(const void *info);
    void (*release)(const void *info);
    CFStringRef (*copyDescription)(const void *info);
} CFRunLoopObserverContext;
typedef void (*CFRunLoopObserverCallBack)(CFRunLoopObserverRef observer, CFRunLoopActivity activity, void *info);

CF_EXPORT const CFRunLoopMode kCFRunLoopDefaultMode;
CF_EXPORT const CFRunLoopMode kCFRunLoopCommonModes;

CF_EXPORT CFRunLoopRef CFRunLoopGetCurrent(void);
CF_EXPORT CFRunLoopRef CFRunLoopGetMain(void);
CF_EXPORT void CFRunLoopRun(void);
CF_EXPORT void CFRunLoopStop(CFRunLoopRef rl);
CF_EXPORT void CFRunLoopWakeUp(CFRunLoopRef rl);
CF_EXPORT CFRunLoopObserverRef CFRunLoopObserverCreate(CFAllocatorRef allocator, CFOptionFlags activities, Boolean repeats,
                                                       CFIndex order, CFRunLoopObserverCallBack callout, CFRunLoopObserverContext *context);
CF_EXPORT void CFRunLoopAddObserver(CFRunLoopRef rl, CFRunLoopObserverRef observer, CFRunLoopMode mode);
CF_EXPORT void CFRunLoopRemoveObserver(CFRunLoopRef rl, CFRunLoopObserverRef observer, CFRunLoopMode mode);
CF_EXPORT void CFRunLoopObserverInvalidate(CFRunLoopObserverRef observer);

CF_EXTERN_C_END
