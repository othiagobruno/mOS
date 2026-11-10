#include "host_bootstrap.h"
#include "../kernel/include/syscall.h"
#include <stdio.h>
#include <string.h>

#define BS_N 64
#define BS_UNKNOWN 1102
#define BS_ACTIVE  1103
#define BS_NOMEM   1105

static struct { char name[96]; unsigned port; int recv; } bs[BS_N];

static int find(const char *name) {
    if (!name || !name[0]) return -1;
    for (int i = 0; i < BS_N; i++) if (bs[i].port && !strcmp(bs[i].name, name)) return i;
    return -1;
}

long host_bootstrap(unsigned op, const char *name, unsigned *port, unsigned (*new_recv)(void)) {
    int i = find(name);
    if (op == MACH_BS_REGISTER) {
        if (!name || !port || !*port) return BS_NOMEM;
        if (i < 0) {
            for (i = 0; i < BS_N && bs[i].port; i++) {}
            if (i == BS_N) return BS_NOMEM;
        }
        snprintf(bs[i].name, sizeof bs[i].name, "%s", name);
        bs[i].port = *port;
        bs[i].recv = 1;
        return 0;
    }
    if (op == MACH_BS_RESERVE || op == MACH_BS_CHECK_IN) {
        if (!name || !name[0] || !port) return BS_UNKNOWN;
        if (i >= 0 && bs[i].recv) return BS_ACTIVE;
        if (!new_recv) return BS_NOMEM;
        unsigned n = new_recv();
        if (!n) return BS_NOMEM;
        if (i < 0) {
            for (i = 0; i < BS_N && bs[i].port; i++) {}
            if (i == BS_N) return BS_NOMEM;
        }
        snprintf(bs[i].name, sizeof bs[i].name, "%s", name);
        bs[i].port = n;
        bs[i].recv = 1;
        *port = n;
        return 0;
    }
    if (op == MACH_BS_LOOK_UP) {
        if (port) *port = 0;
        if (i < 0 || !bs[i].port) return BS_UNKNOWN;
        if (port) *port = bs[i].port;
        return 0;
    }
    return BS_UNKNOWN;
}
