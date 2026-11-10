#pragma once
#ifdef NDEBUG
#define assert(e) ((void)0)
#else
void abort(void);
#define assert(e) ((e) ? (void)0 : abort())
#endif
