#pragma once
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define CTL_HW 6       /* valores do macOS */
#define HW_NCPU 3
int sysctl(int *name, unsigned namelen, void *old, size_t *oldlen, void *newp, size_t newlen);
int sysctlbyname(const char *name, void *old, size_t *oldlen, void *newp, size_t newlen);
#ifdef __cplusplus
}
#endif
