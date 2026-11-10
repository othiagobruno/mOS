/* Freestanding: o sqlite3 amalgamado faz #include <assert.h>.
 * O libSystem não tem libc do host. NDEBUG zera o assert. */
#ifndef MOS_KSHIM_ASSERT_H
#define MOS_KSHIM_ASSERT_H
#ifdef NDEBUG
#define assert(x) ((void)0)
#else
#define assert(x) ((void)(x))
#endif
#endif
