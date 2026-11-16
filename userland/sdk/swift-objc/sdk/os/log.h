/*
 * mOS SDK (variante com interop ObjC) — os/log.h no formato do SDK da Apple para o importador Swift: com ObjC,
 * os_log_t é um objeto OS_OBJECT (classe OS_os_log no Swift) e os_log_type_t é um "swift_wrapper" (OSLogType).
 * É o que dá aos símbolos do overlay os (userland/sdk/swift-objc/os) os nomes mangled que os binários da Apple
 * importam (__C.OS_os_log, __C.os_log_type_t). No libSystem do mOS (darwin.c), OS_LOG_DEFAULT/_os_log_impl:
 * info/debug são descartados, erro/falha vão para o stderr.
 */
#pragma once
#include <stdint.h>
#if defined(__OBJC__)
#import <objc/NSObject.h>
/* o que OS_OBJECT_DECL_CLASS(os_log) dá no SDK da Apple para o Swift (OS_OBJECT_SWIFT3): uma classe ObjC
 * visível só pelo runtime, filha de NSObject (lá, de OS_object) */
__attribute__((objc_runtime_visible))
@interface OS_os_log : NSObject
- (instancetype)init __attribute__((unavailable));
@end
typedef OS_os_log *os_log_t;
#else
typedef struct os_log_s *os_log_t;
#endif


typedef uint8_t os_log_type_t __attribute__((swift_wrapper(enum))) __attribute__((swift_name("OSLogType")));
#define OS_LOG_TYPE_DEFAULT ((os_log_type_t)0x00)
#define OS_LOG_TYPE_INFO    ((os_log_type_t)0x01)
#define OS_LOG_TYPE_DEBUG   ((os_log_type_t)0x02)
#define OS_LOG_TYPE_ERROR   ((os_log_type_t)0x10)
#define OS_LOG_TYPE_FAULT   ((os_log_type_t)0x11)

#ifdef __cplusplus
extern "C" {
#endif
extern char _os_log_default[64];    /* libSystem do mOS (darwin.c) */
extern char _os_log_disabled[64];
#define OS_LOG_DEFAULT ((os_log_t)(void *)_os_log_default)
static inline void *_mos_os_log_default(void) { return (void *)_os_log_default; }
#define OS_LOG_DISABLED ((os_log_t)(void *)_os_log_disabled)
static inline void *_mos_os_log_disabled(void) { return (void *)_os_log_disabled; }

os_log_t os_log_create(const char *subsystem, const char *category);
int os_log_type_enabled(os_log_t log, os_log_type_t type);

/* os/signpost.h: tipo do sinal (swift_wrapper, como no SDK: o overlay estende os_signpost_type_t) e o id de 64 bits */
typedef uint8_t os_signpost_type_t __attribute__((swift_wrapper(enum)));
#define OS_SIGNPOST_EVENT          ((os_signpost_type_t)0x00)
#define OS_SIGNPOST_INTERVAL_BEGIN ((os_signpost_type_t)0x01)
#define OS_SIGNPOST_INTERVAL_END   ((os_signpost_type_t)0x02)
typedef uint64_t os_signpost_id_t;
#define OS_SIGNPOST_ID_NULL      ((os_signpost_id_t)0)
#define OS_SIGNPOST_ID_INVALID   ((os_signpost_id_t)~0)
#define OS_SIGNPOST_ID_EXCLUSIVE ((os_signpost_id_t)0xeeeeb0b5b2b2eeee)
_Bool os_signpost_enabled(os_log_t log);
os_signpost_id_t os_signpost_id_generate(os_log_t log);
os_signpost_id_t os_signpost_id_make_with_pointer(os_log_t log, const void *ptr);
void _os_signpost_emit_with_name_impl(void *dso, os_log_t log, os_signpost_type_t type, os_signpost_id_t spid,
                                      const char *name, const char *format, uint8_t *buf, uint32_t size);
void _os_log_impl(void *dso, os_log_t log, os_log_type_t type, const char *format, uint8_t *buf, uint32_t size);
#ifdef __cplusplus
}
#endif
