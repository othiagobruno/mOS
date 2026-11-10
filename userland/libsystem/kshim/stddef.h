#pragma once
#include "stdlib.h"
typedef long ptrdiff_t;
#define offsetof(type, member) __builtin_offsetof(type, member)
