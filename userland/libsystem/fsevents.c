/*
 * FSEvents (CoreServices). No mOS a entrega vem do journal de eventos do kernel (kernel/lib/vfsevents.c, lido pelo
 * SYS_FSMETA/MOS_FSM_EVENTS em fsevents_core.inc); a varredura de stat/readdir fica para o teste de host
 * (tools/fseventstest.c, FSE_HOST). Ver docs/fsevents.md.
 */
#include "sys.h"
#include "fsevents_core.inc"

extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } fse_cfconst_t;
static fse_cfconst_t ext_path_s = {cx_cfconst_isa, 0x7c8, "path", 4};
static fse_cfconst_t ext_fileid_s = {cx_cfconst_isa, 0x7c8, "fileID", 6};
const void *kFSEventStreamEventExtendedDataPathKey = &ext_path_s;
const void *kFSEventStreamEventExtendedFileIDKey = &ext_fileid_s;
