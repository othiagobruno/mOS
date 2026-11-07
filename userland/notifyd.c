/* mOS — notifyd: notificações do notify(3) entre processos (notify_post / notify_register_*).
 *
 * Serviço Mach NOTIFYD_SERVICE (sob demanda pelo launchd: /Sistema/Biblioteca/LaunchDaemons/com.apple.notifyd.plist).
 * Guarda, por nome, o estado de 64 bits (notify_set_state) e a geração (quantos posts houve: notify_check do
 * cliente compara a geração). Registros por porta recebem um aviso SVC_NOTE_ID com o token; registros por sinal
 * recebem o sinal. O libSystem monta register_dispatch/file_descriptor/check sobre isso (userland/libsystem/notify.c).
 * Protocolo: svcproto.h (NT_*). */
#include "mos.h"
#include "svcnative.h"

#define MAX_NAMES 512
#define MAX_REGS 1024

typedef struct { char name[128]; u64 state; unsigned gen; } nname_t;
typedef struct { int used, name, pid, token, sig; unsigned port; } nreg_t;
static nname_t names[MAX_NAMES];
static int nnames;
static nreg_t regs[MAX_REGS];

static int name_idx(const char *n, int create) {
    for (int i = 0; i < nnames; i++) if (!strcmp(names[i].name, n)) return i;
    if (!create || nnames == MAX_NAMES) return -1;
    usize k = 0;
    while (n[k] && k + 1 < sizeof names[0].name) { names[nnames].name[k] = n[k]; k++; }
    names[nnames].name[k] = 0;
    return nnames++;
}
static void reg_drop(nreg_t *r) {
    if (r->port) port_drop(r->port);
    memset(r, 0, sizeof *r);
}
static nreg_t *reg_new(void) {
    for (int i = 0; i < MAX_REGS; i++) if (!regs[i].used) { regs[i].used = 1; return &regs[i]; }
    return 0;
}

static void post(int ni) {
    names[ni].gen++;
    for (int i = 0; i < MAX_REGS; i++) {
        nreg_t *r = &regs[i];
        if (!r->used || r->name != ni) continue;
        if (r->port) {
            i64 kr = svc_note(r->port, (unsigned)r->token, (unsigned)r->token, names[ni].name, (unsigned)strlen(names[ni].name) + 1);
            if (kr == SEND_INVALID_DEST || kr == 0x10000002 /* SEND_INVALID_DATA */) reg_drop(r);   /* o cliente morreu */
        } else if (r->sig) {
            if (sigkill(r->pid, r->sig) < 0) reg_drop(r);
        }
    }
}

void _start(const char *arg) {
    (void)arg;
    unsigned port = 0;
    if (bs_op(MACH_BS_CHECK_IN, NOTIFYD_SERVICE, &port) != 0 || !port) { print("notifyd: check_in falhou\n"); exit(1); }
    print("notifyd: pronto\n");
    static unsigned char msg[SVC_MAX_MSG + 128];
    for (;;) {
        memset(msg, 0, sizeof(mhdr_t));
        i64 r = mach_msg_n(msg, M_RCV | M_TRAILER, 0, sizeof msg, port, 0);
        if (r) { if (r == 0x10004004) continue; sleep_ms(10); continue; }
        mhdr_t *h = (mhdr_t *)msg;
        unsigned rport = 0;
        svc_body_t *b = svc_body(msg, sizeof msg, &rport);
        unsigned reply = (h->bits & 0x1f) == D_MOVE_SO ? h->remote : 0   /* na recepção a porta de resposta vem no remote */;
        if (!b || h->id != SVC_MSG_ID) { if (rport) port_drop(rport); if (reply) svc_reply(reply, 22, 0, 0, 0, 0); continue; }
        int pid = msg_sender_pid(msg);
        char nm[128];
        usize n1 = b->n1 < sizeof nm ? b->n1 : sizeof nm - 1;
        memcpy(nm, svc_s1(b), n1);
        nm[n1] = 0;
        unsigned code = 0, ra = 0;
        u64 rq = 0;
        switch (b->op) {
        case NT_POST: {
            int ni = name_idx(nm, 1);
            if (ni >= 0) post(ni);
            break;
        }
        case NT_REGISTER_PORT:
        case NT_REGISTER_SIGNAL: {
            int ni = name_idx(nm, 1);
            nreg_t *g = ni >= 0 ? reg_new() : 0;
            if (!g) { code = 1000000; if (rport) port_drop(rport); break; }   /* NOTIFY_STATUS_FAILED */
            g->name = ni; g->pid = pid; g->token = (int)b->a;
            if (b->op == NT_REGISTER_PORT) { g->port = rport; rport = 0; }
            else g->sig = (int)b->b;
            ra = names[ni].gen;
            break;
        }
        case NT_CANCEL:
            for (int i = 0; i < MAX_REGS; i++) if (regs[i].used && regs[i].pid == pid && regs[i].token == (int)b->a) reg_drop(&regs[i]);
            break;
        case NT_GET_STATE: case NT_GENERATION: {
            int ni = name_idx(nm, 0);
            if (ni >= 0) { rq = names[ni].state; ra = names[ni].gen; }
            break;
        }
        case NT_SET_STATE: {
            int ni = name_idx(nm, 1);
            if (ni >= 0) names[ni].state = b->q;
            break;
        }
        default:
            code = 1000000;
        }
        if (rport) port_drop(rport);
        if (reply) svc_reply(reply, code, ra, rq, 0, 0);
    }
}
