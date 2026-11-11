/* AX entre processos sem ObjC: transporte com reserva, páginas de filhos e de texto, dono da janela num ponto.
 * Ver axremote.h. Sem alocação e sem headers da libc (compila no libSystem e no host). */
#include "axremote.h"

void *memset(void *d, int c, unsigned long n);
void *memcpy(void *d, const void *s, unsigned long n);

enum { AX_CANT = -25204, AX_WIRE = 640 };

static const cx_ax_transport *g_fallback;

void cx_ax_transport_set_fallback(const cx_ax_transport *t) { g_fallback = t; }
const cx_ax_transport *cx_ax_transport_fallback(void) { return g_fallback; }

static int via(const cx_ax_transport *t, int pid, const unsigned char *buf, int n, ax_msg *rep) {
    unsigned char rbuf[AX_WIRE];
    int rn = 0;
    if (!t || !t->call) return AX_CANT;
    if (t->ready && !t->ready()) return AX_CANT;
    if (t->call(pid, buf, n, rbuf, (int)sizeof rbuf, &rn) != 0) return AX_CANT;
    if (ax_msg_decode(rbuf, rn, rep) != 0) return AX_CANT;
    return 0;
}

int cx_ax_call(int pid, const ax_msg *req, ax_msg *rep) {
    unsigned char buf[AX_WIRE];
    const cx_ax_transport *bound = cx_ax_transport_get();
    int n;
    if (!req || !rep || pid <= 0) return AX_CANT;
    memset(rep, 0, sizeof *rep);
    n = ax_msg_encode(req, buf, (int)sizeof buf);
    if (n < 0) return AX_CANT;
    if (via(bound, pid, buf, n, rep) == 0) return 0;
    /* o outro processo pode ter ficado só com o AF_UNIX (check-in Mach falhou lá): tenta a reserva */
    if (g_fallback && g_fallback != bound && via(g_fallback, pid, buf, n, rep) == 0) return 0;
    return AX_CANT;
}

int cx_ax_call_kids(int pid, const ax_msg *req, int *kinds, int *ids, int cap, int *n, ax_msg *first) {
    ax_msg q, rep;
    int got = 0;
    if (n) *n = 0;
    if (!req) return AX_CANT;
    if (cap > AX_PAGE_KIDS_MAX) cap = AX_PAGE_KIDS_MAX;
    q = *req;
    for (int page = 0; page < AX_PAGE_KIDS_MAX; page++) {
        int st;
        q.loc = got;
        if (cx_ax_call(pid, &q, &rep) != 0) return AX_CANT;
        if (page == 0 && first) *first = rep;
        st = rep.status;
        if (st) return st;
        if (rep.pay != AX_PAY_KIDS) return 0;          /* não é lista: quem chamou olha *first */
        for (int i = 0; i < rep.nkids && i < AX_KIDS_MAX && got < cap; i++) {
            if (kinds) kinds[got] = rep.kid_kind[i];
            if (ids) ids[got] = rep.kid_id[i];
            got++;
        }
        if (n) *n = got;
        /* um elemento só, servidor sem páginas (len 0), página vazia, lista completa ou teto: acabou */
        if (rep.loc == AX_SINGLE || rep.len <= got || rep.nkids <= 0 || got >= cap) return 0;
    }
    return 0;
}

int cx_ax_call_str(int pid, const ax_msg *req, char *out, int cap, ax_msg *first) {
    ax_msg q, rep;
    int got = 0;
    if (!req || !out || cap <= 0) return AX_CANT;
    out[0] = 0;
    if (cap > AX_PAGE_STR_MAX) cap = AX_PAGE_STR_MAX;
    q = *req;
    for (int page = 0; page < AX_PAGE_STR_MAX / (AX_STR_MAX - 1) + 1; page++) {
        int k = 0;
        q.loc = got;
        if (cx_ax_call(pid, &q, &rep) != 0) return AX_CANT;
        if (page == 0 && first) *first = rep;
        if (rep.status) return rep.status;
        if (rep.pay != AX_PAY_STR) return 0;
        while (rep.str[k] && got + 1 < cap) out[got++] = rep.str[k++];
        out[got] = 0;
        if (rep.len <= got || !k || got + 1 >= cap) return 0;
    }
    return 0;
}

void ax_page_kids(ax_msg *rep, const int *kinds, const int *ids, int n, int offset) {
    if (n < 0) n = 0;
    if (offset < 0 || offset > n) offset = n;
    rep->pay = AX_PAY_KIDS;
    rep->status = 0;
    rep->loc = offset;
    rep->len = n;
    rep->nkids = 0;
    for (int i = offset; i < n && rep->nkids < AX_KIDS_MAX; i++) {
        rep->kid_kind[rep->nkids] = kinds[i];
        rep->kid_id[rep->nkids] = ids[i];
        rep->nkids++;
    }
}

void ax_page_str(ax_msg *rep, const char *s, int offset) {
    int total = 0, k = 0;
    if (!s) s = "";
    while (s[total]) total++;
    if (offset < 0 || offset > total) offset = total;
    rep->pay = AX_PAY_STR;
    rep->status = 0;
    rep->loc = offset;
    rep->len = total;
    for (; s[offset + k] && k < AX_STR_MAX - 1; k++) rep->str[k] = s[offset + k];
    rep->str[k] = 0;
}

void ax_page_single(ax_msg *rep, int kind, int id) {
    rep->pay = AX_PAY_KIDS;
    rep->status = 0;
    rep->loc = AX_SINGLE;
    rep->len = 1;
    rep->nkids = 1;
    rep->kid_kind[0] = kind;
    rep->kid_id[0] = id;
}

int ax_window_owner_at(int (*at)(int index, ax_win_t *out), int x, int y, int *pid, unsigned *wid) {
    ax_win_t w;
    if (!at) return 0;
    for (int i = 0; i < 256; i++) {                     /* a lista vem da frente para o fundo */
        memset(&w, 0, sizeof w);
        if (!at(i, &w)) return 0;
        if (!w.onscreen || w.pid <= 0 || w.w <= 0 || w.h <= 0) continue;
        if (x < w.x || y < w.y || x >= w.x + w.w || y >= w.y + w.h) continue;
        if (pid) *pid = w.pid;
        if (wid) *wid = w.number;
        return 1;
    }
    return 0;
}
