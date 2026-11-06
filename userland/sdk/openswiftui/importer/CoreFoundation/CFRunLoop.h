/*
 * mOS SDK (importador) — CFRunLoop. Os objetos de run loop vêm do libSystem (objetos ObjC do mOS), que o
 * ARC do Swift sem ObjC não sabe reter; por isso os tipos aqui NÃO seguem o padrão `struct __CFxxx *` que o
 * Swift reconhece como classe CF. O Swift reconhece também pelo NOME do typedef (CFRunLoopRef etc.), então os
 * typedefs aqui se chamam MOSCFRunLoopRef...: chegam ao Swift como OpaquePointer, e o Foundation do mOS define
 * `typealias CFRunLoop = MOSCFRunLoopRef` e afins. Os modos também são opacos: no libSystem são NSString (ObjC),
 * que o Swift trataria como classe CF e reteria com swift_retain. Implementação em userland/libsystem/cfrunloop.c
 * (um laço por thread; os modos são aceitos, mas tudo roda em todos).
 */
#pragma once
#include <CoreFoundation/CoreFoundation.h>

CF_EXTERN_C_BEGIN
CF_ASSUME_NONNULL_BEGIN

typedef struct MOSCFRunLoop *MOSCFRunLoopRef;
typedef struct MOSCFRunLoopObserver *MOSCFRunLoopObserverRef;
typedef struct MOSCFRunLoopTimer *MOSCFRunLoopTimerRef;
typedef struct MOSCFRunLoopMode *CFRunLoopMode;

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

typedef struct {
    CFIndex version;
    void *_Nullable info;
    const void *_Nullable (*_Nullable retain)(const void *_Nullable info);
    void (*_Nullable release)(const void *_Nullable info);
    CFStringRef _Nullable (*_Nullable copyDescription)(const void *_Nullable info);
} CFRunLoopTimerContext;
typedef void (*CFRunLoopTimerCallBack)(MOSCFRunLoopTimerRef timer, void *_Nullable info);

CF_EXPORT const CFRunLoopMode kCFRunLoopDefaultMode;
CF_EXPORT const CFRunLoopMode kCFRunLoopCommonModes;

CF_EXPORT MOSCFRunLoopRef CFRunLoopGetCurrent(void);
CF_EXPORT MOSCFRunLoopRef CFRunLoopGetMain(void);
CF_EXPORT void CFRunLoopRun(void);
CF_EXPORT CFRunLoopRunResult CFRunLoopRunInMode(CFRunLoopMode _Nullable mode, CFTimeInterval seconds, Boolean returnAfterSourceHandled);
CF_EXPORT void CFRunLoopStop(MOSCFRunLoopRef rl);
CF_EXPORT void CFRunLoopWakeUp(MOSCFRunLoopRef rl);
CF_EXPORT Boolean CFRunLoopIsWaiting(MOSCFRunLoopRef rl);
CF_EXPORT CFRunLoopMode _Nullable CFRunLoopCopyCurrentMode(MOSCFRunLoopRef rl);

CF_EXPORT MOSCFRunLoopObserverRef _Nullable CFRunLoopObserverCreate(CFAllocatorRef _Nullable allocator, CFOptionFlags activities, Boolean repeats, CFIndex order,
                                                                 CFRunLoopObserverCallBack _Nullable callout, CFRunLoopObserverContext *_Nullable context);
/* como no SDK da Apple (cabeçalho sem auditoria de nulabilidade): parâmetros implicitamente desembrulhados */
CF_EXPORT void CFRunLoopAddObserver(MOSCFRunLoopRef _Null_unspecified rl, MOSCFRunLoopObserverRef _Null_unspecified observer, CFRunLoopMode _Null_unspecified mode);
CF_EXPORT void CFRunLoopRemoveObserver(MOSCFRunLoopRef _Null_unspecified rl, MOSCFRunLoopObserverRef _Null_unspecified observer, CFRunLoopMode _Null_unspecified mode);
CF_EXPORT Boolean CFRunLoopContainsObserver(MOSCFRunLoopRef _Null_unspecified rl, MOSCFRunLoopObserverRef _Null_unspecified observer, CFRunLoopMode _Null_unspecified mode);
CF_EXPORT void CFRunLoopObserverInvalidate(MOSCFRunLoopObserverRef observer);

CF_EXPORT MOSCFRunLoopTimerRef CFRunLoopTimerCreate(CFAllocatorRef _Nullable allocator, CFAbsoluteTime fireDate, CFTimeInterval interval,
                                                   CFOptionFlags flags, CFIndex order, CFRunLoopTimerCallBack _Nullable callout,
                                                   CFRunLoopTimerContext *_Nullable context);
CF_EXPORT void CFRunLoopAddTimer(MOSCFRunLoopRef rl, MOSCFRunLoopTimerRef timer, CFRunLoopMode mode);
CF_EXPORT void CFRunLoopRemoveTimer(MOSCFRunLoopRef rl, MOSCFRunLoopTimerRef timer, CFRunLoopMode mode);
CF_EXPORT Boolean CFRunLoopContainsTimer(MOSCFRunLoopRef rl, MOSCFRunLoopTimerRef timer, CFRunLoopMode mode);
CF_EXPORT void CFRunLoopTimerInvalidate(MOSCFRunLoopTimerRef timer);
CF_EXPORT Boolean CFRunLoopTimerIsValid(MOSCFRunLoopTimerRef timer);
CF_EXPORT CFAbsoluteTime CFRunLoopTimerGetNextFireDate(MOSCFRunLoopTimerRef timer);
CF_EXPORT void CFRunLoopTimerSetNextFireDate(MOSCFRunLoopTimerRef timer, CFAbsoluteTime fireDate);
CF_EXPORT CFTimeInterval CFRunLoopTimerGetInterval(MOSCFRunLoopTimerRef timer);
CF_EXPORT CFTimeInterval CFRunLoopTimerGetTolerance(MOSCFRunLoopTimerRef timer);
CF_EXPORT void CFRunLoopTimerSetTolerance(MOSCFRunLoopTimerRef timer, CFTimeInterval tolerance);

CF_ASSUME_NONNULL_END
CF_EXTERN_C_END
