/* mOS — kernel.h de mentira para compilar kernel/lib/vfs.c e vfsmeta.c no host (tools/vfsmeta_test.c) */
#pragma once
#include "../host_shim.h"
#include <ctype.h>
#define MOS_NAME    "mOS"
#define MOS_VERSION "teste"
typedef struct { char path[256]; const u8 *data; u64 size; } boot_file_t;
typedef struct { u32 file_count; boot_file_t files[4]; } boot_info_t;
extern boot_info_t *g_boot;
u64  timer_ticks(void);
static inline bool kmalloc_is_pages(const void *p) { (void)p; return false; }
