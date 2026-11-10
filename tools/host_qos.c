#include "host_qos.h"
#include "../kernel/include/syscall.h"
#include <string.h>

static unsigned char qos[256];
static unsigned char live[256];

int host_qos_from_darwin(unsigned q) {
    switch (q) {
    case 0x21: return 0;
    case 0x19: return 1;
    case 0x15: case 0x00: return 2;
    case 0x11: return 3;
    case 0x09: return 4;
    default: return 2;
    }
}

void host_thr_boot(void) {
    memset(live, 0, sizeof live);
    memset(qos, 0, sizeof qos);
    live[1] = 1;
    qos[1] = 2;
}

void host_thr_add(int tid) {
    if (tid <= 0 || tid >= 256) return;
    live[tid] = 1;
    qos[tid] = 2;
}

void host_thr_dead(int tid) {
    if (tid <= 0 || tid >= 256) return;
    live[tid] = 0;
}

long host_thr_setqos(int tid, int self, unsigned darwin) {
    int id = tid == 0 ? self : tid;
    if (id <= 0 || id >= 256 || !live[id]) return -MOS_ESRCH;
    qos[id] = (unsigned char)host_qos_from_darwin(darwin);
    return 0;
}

int host_thr_qos(int tid) {
    if (tid <= 0 || tid >= 256 || !live[tid]) return -1;
    return qos[tid];
}
