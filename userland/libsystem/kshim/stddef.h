#pragma once
#include "../sys.h"
typedef long ptrdiff_t;
#define NULL ((void *)0)
#define offsetof(t, m) __builtin_offsetof(t, m)
