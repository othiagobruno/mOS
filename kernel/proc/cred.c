#include "cred.h"

#define SIG_KILL 9
#define SIG_STOP 20
#define MODE_SET 0x10000u

void cred_spawn_apply(unsigned long long handlers[32], unsigned *mask,
                      int *uid, int *euid, int *suid, int *gid, int *egid, int *sgid,
                      unsigned sigdef, unsigned sigmask, unsigned extra) {
    if (extra & 1)
        for (int s = 1; s < 32; s++)
            if ((sigdef & (1u << s)) && handlers[s] == 1) handlers[s] = 0;
    if ((extra & 2) && mask)
        *mask = sigmask & ~((1u << SIG_KILL) | (1u << SIG_STOP));
    if (extra & 4) {
        if (euid && uid) *euid = *uid;
        if (suid && uid) *suid = *uid;
        if (egid && gid) *egid = *gid;
        if (sgid && gid) *sgid = *gid;
    }
}

unsigned mode_for_creat(unsigned req) {
    unsigned m = req & 07777u;
    if (!m) m = 0644;
    return m | MODE_SET;
}

void mode_apply(unsigned *slot, unsigned mode) {
    if (slot) *slot = (mode & 07777u) | MODE_SET;
}

void fchmod_apply(unsigned *vnode_mode, unsigned *fd_mode, unsigned mode) {
    mode_apply(vnode_mode, mode);
    if (fd_mode) *fd_mode = mode & 07777u;
}

unsigned mode_stat(unsigned slot, int is_dir) {
    if (slot & MODE_SET) return slot & 07777u;
    return is_dir ? 0755u : 0644u;
}

int cred_seteid(int real, int saved, int *eff, int neu) {
    if (!eff || neu < 0) return -1;
    if (neu == real || neu == saved || neu == *eff) { *eff = neu; return 0; }
    return -1;
}
