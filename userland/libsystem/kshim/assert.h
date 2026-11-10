#pragma once
#ifdef NDEBUG
#define assert(cond) ((void)0)
#else
#define assert(cond) ((void)(cond))
#endif
#ifndef static_assert
#define static_assert(cond, msg) _Static_assert(cond, msg)
#endif
