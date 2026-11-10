/* Ponte dos apps para o banner e para o selo do Dock.
 * SYS_DESKTOP: MOS_DESK_BANNER publica notify_tagged; o clique no banner
 * (desktop.c) entra na fila e o app busca com MOS_DESK_TAKE. MOS_DESK_BADGE
 * grava o rótulo no app_t que o Dock desenha. */
#include "proc.h"
#include "ui.h"
#include "syscall.h"
#include "compat.h"

enum { DESK_Q = 16 };

static struct { int used, pid, token; } desk_q[DESK_Q];

void desk_banner_enqueue(int pid, int token) {
    int i;
    if (token <= 0) return;
    for (i = 0; i < DESK_Q; i++) if (!desk_q[i].used) {
        desk_q[i].used = 1;
        desk_q[i].pid = pid;
        desk_q[i].token = token;
        return;
    }
    desk_q[0].used = 1;
    desk_q[0].pid = pid;
    desk_q[0].token = token;
}

static int desk_take(int pid, int *token) {
    int i;
    for (i = 0; i < DESK_Q; i++) if (desk_q[i].used && desk_q[i].pid == pid) {
        if (token) *token = desk_q[i].token;
        desk_q[i].used = 0;
        return 1;
    }
    return 0;
}

static int copy_user_str(process_t *p, u64 va, char *out, int cap) {
    int i;
    if (!out || cap < 2) return 0;
    out[0] = 0;
    if (!va) return 1;
    for (i = 0; i + 1 < cap; i++) {
        char ch = 0;
        if (!uvm_copy_in(p, &ch, va + (u64)i, 1)) return 0;
        out[i] = ch;
        if (!ch) return 1;
    }
    out[cap - 1] = 0;
    return 1;
}

i64 deskbridge_syscall(process_t *p, u64 op, u64 b, u64 c, u64 d) {
    if (!p) return -MOS_EINVAL;
    if (op == MOS_DESK_BANNER) {
        char title[64], body[160];
        if (!copy_user_str(p, b, title, (int)sizeof title)) return -MOS_EFAULT;
        if (!copy_user_str(p, c, body, (int)sizeof body)) return -MOS_EFAULT;
        notify_tagged(title, body, (int)p->pid, (int)d);
        return 0;
    }
    if (op == MOS_DESK_TAKE) {
        int token = 0;
        if (!desk_take((int)p->pid, &token)) return 0;
        if (b && !uvm_copy_out(p, b, &token, sizeof token)) return -MOS_EFAULT;
        return 1;
    }
    if (op == MOS_DESK_BADGE) {
        char label[64];
        if (!copy_user_str(p, b, label, (int)sizeof label)) return -MOS_EFAULT;
        return compat_gui_set_badge_label(p, label) ? 1 : 0;
    }
    return -MOS_EINVAL;
}
