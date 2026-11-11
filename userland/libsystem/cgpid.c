/* CGEventPostToPid: pacotes de DEV_INPUT_TO_PID e a regra dos taps. Ver cgpid.h. */
#include "cgpid.h"

int cx_cgpid_taps(int target_pid, int self_pid) { return target_pid > 0 && target_pid == self_pid; }

static int put(cgpid_pkt *out, int cap, int n, unsigned type, unsigned buttons, unsigned mods, int wheel, int key, int pid,
               int x, int y) {
    if (n >= cap) return n;
    out[n].b = (unsigned long long)type | (unsigned long long)buttons << 8 | (unsigned long long)(mods & 0xFFFF) << 16 |
               (unsigned long long)(unsigned)wheel << 32;
    out[n].c = (unsigned long long)(unsigned)key | (unsigned long long)(unsigned)pid << 32;
    out[n].d = (unsigned long long)(unsigned)x | (unsigned long long)(unsigned)y << 32;
    return n + 1;
}

int cx_cgpid_packets(const cgpid_ev *e, int pid, cgpid_pkt *out, int cap) {
    int n = 0;
    unsigned t;
    if (!e || !out || cap <= 0 || pid <= 0) return 0;
    t = e->type;
    if (t == 10 || t == 11) {
        if (e->nuni && e->uni) {                         /* texto: cada caractere como tecla (Latin-1), descida e subida */
            if (t != 10) return 0;
            for (unsigned i = 0; i < e->nuni; i++) {
                unsigned ch = e->uni[i];
                int k = ch == '\r' || ch == '\n' ? 10 : ch == '\t' ? 9 : ch < 0x100 ? (int)ch : -1;
                if (k < 0) continue;
                n = put(out, cap, n, 10, 0, e->mods, 0, k, pid, 0, 0);
                n = put(out, cap, n, 11, 0, e->mods, 0, k, pid, 0, 0);
            }
            return n;
        }
        int k = e->key;
        if (k < 0) return 0;
        if ((e->mods & 1) && k >= 'a' && k <= 'z') k -= 32;
        return put(out, cap, n, t, 0, e->mods, 0, k, pid, 0, 0);
    }
    unsigned bit = 0, buttons = 0;
    switch (t) {
    case 1: case 2: case 6: bit = 1; break;
    case 3: case 4: case 7: bit = 2; break;
    case 25: case 26: case 27: bit = 4; break;
    case 5: case 22: break;
    default: return 0;
    }
    if (t == 1 || t == 3 || t == 25) buttons = bit;
    if (t == 22 && !e->wheel) return 0;
    return put(out, cap, n, t, buttons, e->mods, t == 22 ? e->wheel : 0, 0, pid, e->x, e->y);
}
