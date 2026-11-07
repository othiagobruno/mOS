/* mOS — declarações para os testes dos serviços (o zig não traz os headers de xpc, notify, os_log, dispatch, CF e
 * SystemConfiguration do SDK). Mesmos nomes e assinaturas do macOS; o os_log usa os builtins do clang, então o buffer
 * que chega ao _os_log_impl é o mesmo de um app compilado pelo Xcode. */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/* ---- dispatch */
typedef void *dispatch_queue_t, *dispatch_semaphore_t;
typedef uint64_t dispatch_time_t;
#define DISPATCH_TIME_NOW 0ull
#define NSEC_PER_SEC 1000000000ull
dispatch_queue_t dispatch_get_global_queue(long prio, unsigned long flags);
dispatch_semaphore_t dispatch_semaphore_create(long v);
long dispatch_semaphore_wait(dispatch_semaphore_t s, dispatch_time_t t);
long dispatch_semaphore_signal(dispatch_semaphore_t s);
dispatch_time_t dispatch_time(dispatch_time_t when, int64_t delta);

/* ---- xpc */
typedef void *xpc_object_t, *xpc_connection_t;
typedef const struct _xpc_type_s *xpc_type_t;
extern const struct _xpc_type_s _xpc_type_dictionary;
extern struct _xpc_dictionary_s _xpc_error_connection_invalid, _xpc_error_connection_interrupted;
#define XPC_TYPE_DICTIONARY (&_xpc_type_dictionary)
#define XPC_ERROR_CONNECTION_INVALID ((xpc_object_t)&_xpc_error_connection_invalid)
#define XPC_ERROR_CONNECTION_INTERRUPTED ((xpc_object_t)&_xpc_error_connection_interrupted)
#define XPC_CONNECTION_MACH_SERVICE_LISTENER 1ull
xpc_object_t xpc_dictionary_create(const char *const *k, const xpc_object_t *v, size_t n);
xpc_object_t xpc_dictionary_create_reply(xpc_object_t original);
void xpc_dictionary_set_string(xpc_object_t, const char *, const char *);
void xpc_dictionary_set_int64(xpc_object_t, const char *, int64_t);
void xpc_dictionary_set_bool(xpc_object_t, const char *, bool);
const char *xpc_dictionary_get_string(xpc_object_t, const char *);
int64_t xpc_dictionary_get_int64(xpc_object_t, const char *);
xpc_type_t xpc_get_type(xpc_object_t);
void xpc_release(xpc_object_t);
xpc_connection_t xpc_connection_create(const char *name, dispatch_queue_t q);
xpc_connection_t xpc_connection_create_mach_service(const char *name, dispatch_queue_t q, uint64_t flags);
void xpc_connection_set_event_handler(xpc_connection_t, void (^)(xpc_object_t));
void xpc_connection_resume(xpc_connection_t);
void xpc_connection_send_message(xpc_connection_t, xpc_object_t);
void xpc_connection_send_message_with_reply(xpc_connection_t, xpc_object_t, dispatch_queue_t, void (^)(xpc_object_t));
xpc_object_t xpc_connection_send_message_with_reply_sync(xpc_connection_t, xpc_object_t);
int xpc_connection_get_pid(xpc_connection_t);
void xpc_main(void (*handler)(xpc_connection_t));

/* ---- notify */
uint32_t notify_post(const char *name);
uint32_t notify_register_check(const char *name, int *token);
uint32_t notify_register_dispatch(const char *name, int *token, dispatch_queue_t q, void (^h)(int));
uint32_t notify_check(int token, int *changed);
uint32_t notify_set_state(int token, uint64_t state);
uint32_t notify_get_state(int token, uint64_t *state);

/* ---- os_log */
typedef void *os_log_t;
void *os_log_create(const char *subsystem, const char *category);
void _os_log_impl(void *dso, os_log_t log, uint8_t type, const char *fmt, uint8_t *buf, uint32_t size);
#define OS_LOG_TYPE_DEFAULT 0x00
#define OS_LOG_TYPE_ERROR 0x10
#define os_log_with_type(log, type, fmt, ...) do { \
        __attribute__((section("__TEXT,__oslogstring,cstring_literals"))) static const char _f[] = fmt; \
        uint8_t _b[__builtin_os_log_format_buffer_size(fmt, ##__VA_ARGS__)] __attribute__((aligned(8))); \
        _os_log_impl(0, (log), (type), _f, (uint8_t *)__builtin_os_log_format(_b, fmt, ##__VA_ARGS__), (uint32_t)sizeof(_b)); \
    } while (0)
#define os_log(log, fmt, ...) os_log_with_type(log, OS_LOG_TYPE_DEFAULT, fmt, ##__VA_ARGS__)
#define os_log_error(log, fmt, ...) os_log_with_type(log, OS_LOG_TYPE_ERROR, fmt, ##__VA_ARGS__)

/* ---- CoreFoundation / SystemConfiguration */
typedef const void *CFTypeRef;
typedef const struct __CFString *CFStringRef;
typedef const struct __CFDictionary *CFDictionaryRef;
typedef const struct __SCDynamicStore *SCDynamicStoreRef;
#define CFSTR(s) ((CFStringRef)__builtin___CFStringMakeConstantString(s))
#define kCFStringEncodingUTF8 0x08000100u
const void *CFDictionaryGetValue(CFDictionaryRef d, const void *k);
bool CFStringGetCString(CFStringRef s, char *buf, long cap, uint32_t enc);
SCDynamicStoreRef SCDynamicStoreCreate(const void *alloc, CFStringRef name, void *callout, void *ctx);
CFTypeRef SCDynamicStoreCopyValue(SCDynamicStoreRef s, CFStringRef key);
CFDictionaryRef SCDynamicStoreCopyProxies(SCDynamicStoreRef s);
CFStringRef SCDynamicStoreCopyComputerName(SCDynamicStoreRef s, uint32_t *enc);
