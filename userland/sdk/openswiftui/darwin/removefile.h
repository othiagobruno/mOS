/*
 * mOS SDK — <removefile.h> do macOS (o zig não traz): remoção recursiva com callbacks. Declarações com o ABI do macOS;
 * a implementação fica no libSystem do mOS.
 */
#ifndef __REMOVEFILE_H__
#define __REMOVEFILE_H__

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t removefile_flags_t;
typedef struct _removefile_state *removefile_state_t;

enum {
    REMOVEFILE_RECURSIVE = (1 << 0),
    REMOVEFILE_KEEP_PARENT = (1 << 1),
    REMOVEFILE_SECURE_7_PASS = (1 << 2),
    REMOVEFILE_SECURE_35_PASS = (1 << 3),
    REMOVEFILE_SECURE_1_PASS = (1 << 4),
    REMOVEFILE_SECURE_3_PASS = (1 << 5),
    REMOVEFILE_SECURE_1_PASS_ZERO = (1 << 6),
    REMOVEFILE_CROSS_MOUNT = (1 << 7),
    REMOVEFILE_ALLOW_LONG_PATHS = (1 << 8),
    REMOVEFILE_CLEAR_PURGEABLE = (1 << 9),
    REMOVEFILE_SYSTEM_DISCARDED = (1 << 10),
};

enum {
    REMOVEFILE_STATE_CONFIRM_CALLBACK = 1,
    REMOVEFILE_STATE_CONFIRM_CONTEXT = 2,
    REMOVEFILE_STATE_ERROR_CALLBACK = 3,
    REMOVEFILE_STATE_ERROR_CONTEXT = 4,
    REMOVEFILE_STATE_ERRNO = 5,
    REMOVEFILE_STATE_STATUS_CALLBACK = 6,
    REMOVEFILE_STATE_STATUS_CONTEXT = 7,
    REMOVEFILE_STATE_FTSENT = 8,
};

enum {
    REMOVEFILE_PROCEED = 0,
    REMOVEFILE_SKIP = 1,
    REMOVEFILE_STOP = 2,
};

typedef int (*removefile_callback_t)(removefile_state_t state, const char *path, void *context);

removefile_state_t removefile_state_alloc(void);
int removefile_state_free(removefile_state_t state);
int removefile_state_get(removefile_state_t state, uint32_t key, void *dst);
int removefile_state_set(removefile_state_t state, uint32_t key, const void *value);
int removefile(const char *path, removefile_state_t state, removefile_flags_t flags);
int removefileat(int fd, const char *path, removefile_state_t state, removefile_flags_t flags);
int removefile_cancel(removefile_state_t state);

#ifdef __cplusplus
}
#endif

#endif
