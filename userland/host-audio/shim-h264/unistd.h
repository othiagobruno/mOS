#pragma once
#include "sys/types.h"
#ifdef __cplusplus
extern "C" {
#endif
#define _SC_NPROCESSORS_CONF 57   /* valores do macOS (o sysconf do libSystem segue o Darwin) */
#define _SC_NPROCESSORS_ONLN 58
long sysconf(int name);
int usleep(useconds_t us);
#ifdef __cplusplus
}
#endif
