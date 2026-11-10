#include "fileport.h"
#include "syscall.h"

int fileport_dup2(unsigned char *used, int *flags, unsigned *mode, char *path, unsigned pathcap,
                  int snap_used, int snap_flags, unsigned snap_mode, const char *snap_path) {
    if (!used || !snap_used) return -1;
    *used = 1;
    if (flags) *flags = snap_flags & ~MOS_O_CLOEXEC;
    if (mode) *mode = snap_mode;
    if (path && pathcap) {
        unsigned i = 0;
        if (!snap_path) snap_path = "";
        while (i + 1 < pathcap && snap_path[i]) { path[i] = snap_path[i]; i++; }
        path[i] = 0;
    }
    return 0;
}
