/* Ponte dos apps para as notificações e para o selo do Dock.
 * SYS_DESKTOP: MOS_DESK_BANNER/BANNER_EX publicam a notificação (desktop.c); o que o usuário faz com ela (clique no
 * corpo, botão de ação, texto de resposta, fechar) entra na fila deste arquivo e o app busca com MOS_DESK_TAKE (só
 * cliques, a ABI antiga) ou MOS_DESK_TAKE_EX (tudo). MOS_DESK_REMOVE tira as do app do banner e da Central.
 * MOS_DESK_BADGE grava o rótulo no app_t que o Dock desenha. */
#include "proc.h"
#include "ui.h"
#include "syscall.h"
#include "compat.h"

enum { DESK_Q = 16 };

static struct { int used, pid, token, action; char text[256]; } desk_q[DESK_Q];
static int desk_seq;

void desk_event_enqueue(int pid, int token, int action, const char *text) {
    int i, slot = -1;
    if (token <= 0) return;
    for (i = 0; i < DESK_Q; i++) if (!desk_q[i].used) { slot = i; break; }
    if (slot < 0) slot = desk_seq++ % DESK_Q;       /* cheia: o mais antigo cede o lugar */
    desk_q[slot].used = 1;
    desk_q[slot].pid = pid;
    desk_q[slot].token = token;
    desk_q[slot].action = action;
    snprintf(desk_q[slot].text, sizeof desk_q[slot].text, "%s", text ? text : "");
}

void desk_banner_enqueue(int pid, int token) { desk_event_enqueue(pid, token, MOS_DESK_EV_CLICK, NULL); }

/* clicks_only: a ABI antiga (MOS_DESK_TAKE) só conhece o clique no corpo */
static int desk_take(int pid, bool clicks_only, mos_desk_event_t *ev) {
    for (int i = 0; i < DESK_Q; i++) {
        if (!desk_q[i].used || desk_q[i].pid != pid) continue;
        if (clicks_only && desk_q[i].action != MOS_DESK_EV_CLICK) continue;
        ev->token = desk_q[i].token;
        ev->action = desk_q[i].action;
        snprintf(ev->text, sizeof ev->text, "%s", desk_q[i].text);
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
    if (op == MOS_DESK_BANNER_EX) {
        mos_desk_banner_t bn;
        static char title[64], subtitle[96], body[256], actions[4 * 64 + 8];
        if (!b || !uvm_copy_in(p, &bn, b, sizeof bn)) return -MOS_EFAULT;
        if (bn.token <= 0) return -MOS_EINVAL;
        if (!copy_user_str(p, bn.title, title, (int)sizeof title) || !copy_user_str(p, bn.subtitle, subtitle, (int)sizeof subtitle) ||
            !copy_user_str(p, bn.body, body, (int)sizeof body) || !copy_user_str(p, bn.actions, actions, (int)sizeof actions))
            return -MOS_EFAULT;
        notify_tagged_ex(title, subtitle, body, actions, (int)p->pid, bn.token, bn.flags);
        return 0;
    }
    if (op == MOS_DESK_TAKE || op == MOS_DESK_TAKE_EX) {
        mos_desk_event_t ev;
        memset(&ev, 0, sizeof ev);
        if (!desk_take((int)p->pid, op == MOS_DESK_TAKE, &ev)) return 0;
        if (op == MOS_DESK_TAKE) { if (b && !uvm_copy_out(p, b, &ev.token, sizeof ev.token)) return -MOS_EFAULT; }
        else if (!b || !uvm_copy_out(p, b, &ev, sizeof ev)) return -MOS_EFAULT;
        return 1;
    }
    if (op == MOS_DESK_REMOVE) {
        int token = (int)(i64)b;
        if (token < 0) return -MOS_EINVAL;
        return notify_remove((int)p->pid, token);      /* eventos ainda não lidos ficam: o clique do usuário vale */
    }
    if (op == MOS_DESK_SEARCH) {
        char q[96];
        if (!copy_user_str(p, b, q, (int)sizeof q)) return -MOS_EFAULT;
        desktop_spotlight_query(q);
        return 0;
    }
    if (op == MOS_DESK_BADGE) {
        char label[64];
        if (!copy_user_str(p, b, label, (int)sizeof label)) return -MOS_EFAULT;
        return compat_gui_set_badge_label(p, label) ? 1 : 0;
    }
    return -MOS_EINVAL;
}
