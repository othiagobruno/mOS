#pragma once
#include "../shim-midia/stdio.h"
#ifdef __cplusplus
extern "C" {
#endif
int vsnprintf(char *s, size_t n, const char *fmt, __mos_va_list ap);
#ifdef __cplusplus
}
#endif
