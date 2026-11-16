/* ganchos do kernel para tools/fsevents_kernel_test.sh (o que fs/persist.c e o boot dariam) */
#include "vfs_host_kernel.h"
#include "vfs.h"
long long g_live_allocs, g_live_bytes;
void rtc_now(datetime_t *dt) { *dt = (datetime_t){2026, 11, 15, 12, 0, 0, 0}; }
uint32_t rand32(void) { return 4; }
static boot_info_t boot0;
boot_info_t *g_boot = &boot0;
bool persist_load(void) { return false; }
void persist_enable(void) {}
void persist_mark(vnode_t *n) { (void)n; }
void persist_delete(vnode_t *n) { (void)n; }
void persist_move(const char *o, vnode_t *n) { (void)o; (void)n; }
bool persist_available(void) { return false; }
void persist_meta_mark(void) {}
