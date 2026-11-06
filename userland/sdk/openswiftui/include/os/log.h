/*
 * mOS SDK — os/log.h mínimo para compilar as dependências do OpenSwiftUI (os headers macOS do zig não
 * trazem o log unificado). O libSystem do mOS exporta os_log_create e OS_LOG_DEFAULT; info/debug são
 * descartados e erro/falha vão para o stderr do processo (Console do mOS).
 */
#pragma once
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct os_log_s *os_log_t;
extern struct os_log_s _os_log_default;
extern struct os_log_s _os_log_disabled;
#define OS_LOG_DEFAULT (&_os_log_default)
#define OS_LOG_DISABLED (&_os_log_disabled)

os_log_t os_log_create(const char *subsystem, const char *category);

#define os_log(log, format, ...) ((void)(log))
#define os_log_info(log, format, ...) ((void)(log))
#define os_log_debug(log, format, ...) ((void)(log))
#define os_log_error(log, format, ...) ((void)(log), fprintf(stderr, format "\n", ##__VA_ARGS__))
#define os_log_fault(log, format, ...) ((void)(log), fprintf(stderr, format "\n", ##__VA_ARGS__))

#ifdef __cplusplus
}
#endif
