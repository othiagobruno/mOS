/* mOS — TCP (RFC 793/1122/5681/6298, apenas abertura ativa)
 * Janela deslizante com retransmissão (go-back-N + retransmissão rápida), controle de congestionamento
 * simples (slow start / congestion avoidance), opção MSS, buffer de recepção de 64 KiB, FIN nos dois
 * sentidos e RST. Segmentos fora de ordem são descartados (o remetente os retransmite). */
#include "netpriv.h"

#define TCP_SOCKS     64        /* o Chromium abre dezenas de conexões (antes 16, só o navegador do kernel) */
#define TX_BUF        (64 * 1024)
#define RX_BUF        (64 * 1024)
#define OUR_MSS       1460
#define RTO_INIT      1000
#define RTO_MIN       200
#define RTO_MAX       60000
#define SYN_RETRIES   6
#define DATA_RETRIES  10
#define TIME_WAIT_MS  2000
#define FIN_WAIT2_MS  30000

#define F_FIN 0x01
#define F_SYN 0x02
#define F_RST 0x04
#define F_PSH 0x08
#define F_ACK 0x10

enum { S_FREE, S_SYN_SENT, S_ESTAB, S_FIN_WAIT1, S_FIN_WAIT2, S_CLOSING, S_TIME_WAIT, S_CLOSE_WAIT, S_LAST_ACK, S_ERR };

#define SEQ_LT(a, b) ((i32)((a) - (b)) < 0)
#define SEQ_LE(a, b) ((i32)((a) - (b)) <= 0)
#define SEQ_GT(a, b) ((i32)((a) - (b)) > 0)
#define SEQ_GE(a, b) ((i32)((a) - (b)) >= 0)

typedef struct {
    int  st;
    bool user_closed;   /* aplicativo chamou tcp_close: o slot é liberado ao fim do fechamento */
    bool peer_fin;
    bool fin_ever, fin_sent, fin_acked;
    bool ack_now;
    const char *err;

    u32 rip;
    u16 lport, rport;

    u32 iss, snd_una, snd_nxt, snd_max, snd_wnd, fin_seq;
    u32 cwnd, ssthresh;
    u16 mss;
    u8  dupacks;

    u32 rcv_nxt, rcv_adv;   /* rcv_adv: borda direita da janela já anunciada */

    u8 *tx; u32 tx_head, tx_len;   /* bytes a partir de snd_una (não confirmados + não enviados) */
    u8 *rx; u32 rx_head, rx_len;

    u64 rto_at;
    u32 rto, srtt, rttvar;
    u8  retries;
    bool rtt_on;
    u32 rtt_seq;
    u64 rtt_t0;

    u64 close_at;
} tcb_t;

static tcb_t socks[TCP_SOCKS];

static tcb_t *get(int s) {
    if (s < 0 || s >= TCP_SOCKS || socks[s].st == S_FREE) return NULL;
    return &socks[s];
}

static void tcb_free(tcb_t *t) {
    kfree(t->tx);
    kfree(t->rx);
    memset(t, 0, sizeof *t);
}

static u16 rx_window(tcb_t *t) {
    u32 free_ = RX_BUF - t->rx_len;
    return (u16)MIN(free_, 65535u);
}

/* monta e envia um segmento; dados copiados do anel de TX a partir de off (relativo a snd_una) */
static void send_raw(u32 rip, u16 lport, u16 rport, u32 seq, u32 ack, u8 flags, u16 win,
                     const u8 *opts, int optlen, tcb_t *t, u32 off, u16 dlen) {
    static u8 seg[ETH_MTU];
    int hl = 20 + optlen;
    wr16(seg, lport);
    wr16(seg + 2, rport);
    wr32(seg + 4, seq);
    wr32(seg + 8, ack);
    seg[12] = (u8)((hl / 4) << 4);
    seg[13] = flags;
    wr16(seg + 14, win);
    wr16(seg + 16, 0);
    wr16(seg + 18, 0);
    if (optlen) memcpy(seg + 20, opts, optlen);
    if (dlen && t) {
        u32 start = (t->tx_head + off) % TX_BUF;
        u32 first = MIN((u32)dlen, TX_BUF - start);
        memcpy(seg + hl, t->tx + start, first);
        if (first < dlen) memcpy(seg + hl + first, t->tx, dlen - first);
    }
    u16 total = (u16)(hl + dlen);
    u32 s = inet_pseudo_sum(g_net.ip, rip, IPPROTO_TCP, total);
    wr16(seg + 16, inet_csum_fold(inet_csum_add(s, seg, total)));
    ipv4_send(rip, IPPROTO_TCP, seg, total);
}

static void xmit(tcb_t *t, u32 seq, u8 flags, u32 off, u16 dlen) {
    u8 opt[4];
    int optlen = 0;
    if (flags & F_SYN) { opt[0] = 2; opt[1] = 4; wr16(opt + 2, OUR_MSS); optlen = 4; }
    u16 win = rx_window(t);
    send_raw(t->rip, t->lport, t->rport, seq, (flags & F_ACK) ? t->rcv_nxt : 0, flags, win, opt, optlen, t, off, dlen);
    if (flags & F_ACK) {
        t->rcv_adv = t->rcv_nxt + win;
        t->ack_now = false;
    }
}

static void arm_rto(tcb_t *t) { t->rto_at = timer_ticks() + t->rto; }

static void set_error(tcb_t *t, const char *why) {
    if (t->user_closed) { tcb_free(t); return; }
    t->st = S_ERR;
    t->err = why;
    t->rto_at = 0;
}

/* envia o que a janela permitir, e o FIN quando o aplicativo fechou e não há mais dados */
static void output(tcb_t *t) {
    if (t->st != S_ESTAB && t->st != S_CLOSE_WAIT && t->st != S_FIN_WAIT1 && t->st != S_CLOSING && t->st != S_LAST_ACK) return;
    for (;;) {
        if (t->fin_sent) break;
        u32 off = t->snd_nxt - t->snd_una;
        if (off > t->tx_len) break;
        u32 unsent = t->tx_len - off;
        u32 wnd = MIN(t->snd_wnd, t->cwnd);
        u32 avail = wnd > off ? wnd - off : 0;
        if (unsent) {
            u32 n = MIN(unsent, MIN(avail, (u32)t->mss));
            /* evita segmentos minúsculos (SWS do remetente) quando há dados em trânsito */
            if (n == 0 || (n < t->mss && n < unsent && off > 0)) break;
            xmit(t, t->snd_nxt, F_ACK | (n == unsent ? F_PSH : 0), off, (u16)n);
            if (!t->rtt_on) { t->rtt_on = true; t->rtt_seq = t->snd_nxt; t->rtt_t0 = timer_ticks(); }
            t->snd_nxt += n;
            if (SEQ_GT(t->snd_nxt, t->snd_max)) t->snd_max = t->snd_nxt;
            if (!t->rto_at) arm_rto(t);
            continue;
        }
        if (t->user_closed && !t->fin_acked) {
            if (!t->fin_ever) { t->fin_ever = true; t->fin_seq = t->snd_nxt; }
            xmit(t, t->fin_seq, F_FIN | F_ACK, 0, 0);
            t->fin_sent = true;
            t->snd_nxt = t->fin_seq + 1;
            if (SEQ_GT(t->snd_nxt, t->snd_max)) t->snd_max = t->snd_nxt;
            if (t->st == S_ESTAB) t->st = S_FIN_WAIT1;
            else if (t->st == S_CLOSE_WAIT) t->st = S_LAST_ACK;
            if (!t->rto_at) arm_rto(t);
        }
        break;
    }
    /* janela zero com dados pendentes: o temporizador funciona como "persist" */
    if (!t->rto_at && t->tx_len > t->snd_nxt - t->snd_una) arm_rto(t);
}

/* retransmite um segmento a partir de snd_una (retransmissão rápida) */
static void retransmit_one(tcb_t *t) {
    if (t->tx_len) {
        u16 n = (u16)MIN(t->tx_len, (u32)t->mss);
        xmit(t, t->snd_una, F_ACK, 0, n);
    } else if (t->fin_ever && !t->fin_acked) {
        xmit(t, t->fin_seq, F_FIN | F_ACK, 0, 0);
    }
}

static void rtt_sample(tcb_t *t, u32 r) {
    if (!t->srtt) { t->srtt = r ? r : 1; t->rttvar = r / 2; }
    else {
        u32 d = t->srtt > r ? t->srtt - r : r - t->srtt;
        t->rttvar = (3 * t->rttvar + d) / 4;
        t->srtt = (7 * t->srtt + r) / 8;
    }
    u32 rto = t->srtt + MAX(10u, 4 * t->rttvar);
    t->rto = CLAMP(rto, (u32)RTO_MIN, (u32)RTO_MAX);
}

/* =================================================================== API */
int tcp_connect(u32 ip, u16 port) {
    if (!g_netdev || !g_net.configured || !ip || !port) return -1;
    int s = -1;
    for (int i = 0; i < TCP_SOCKS; i++) if (socks[i].st == S_FREE) { s = i; break; }
    if (s < 0) return -1;
    tcb_t *t = &socks[s];
    memset(t, 0, sizeof *t);
    t->tx = kmalloc(TX_BUF);
    t->rx = kmalloc(RX_BUF);
    if (!t->tx || !t->rx) { kfree(t->tx); kfree(t->rx); t->tx = t->rx = NULL; return -1; }
    t->rip = ip;
    t->rport = port;
    /* porta efêmera única */
    for (int tries = 0; tries < 64; tries++) {
        u16 p = net_ephemeral_port();
        bool dup = false;
        for (int i = 0; i < TCP_SOCKS; i++) if (i != s && socks[i].st != S_FREE && socks[i].lport == p) dup = true;
        t->lport = p;
        if (!dup) break;
    }
    t->iss = rand32() ^ (u32)(timer_ticks() * 250);
    t->snd_una = t->iss;
    t->snd_nxt = t->iss + 1;
    t->snd_max = t->snd_nxt;
    t->mss = 536;
    t->cwnd = 10 * OUR_MSS;
    t->ssthresh = 0xFFFF;
    t->rto = RTO_INIT;
    t->st = S_SYN_SENT;
    xmit(t, t->iss, F_SYN, 0, 0);
    t->rtt_on = true; t->rtt_seq = t->iss; t->rtt_t0 = timer_ticks();
    arm_rto(t);
    return s;
}

int tcp_state(int s) {
    if (s >= TCP_MEM_BASE) return memsock_state(s);
    tcb_t *t = get(s);
    if (!t) return TCP_CLOSED;
    switch (t->st) {
    case S_SYN_SENT: return TCP_CONNECTING;
    case S_ESTAB: case S_FIN_WAIT1: case S_FIN_WAIT2: return t->peer_fin ? TCP_PEER_CLOSED : TCP_OPEN;
    case S_ERR: return TCP_ERROR;
    default: return TCP_PEER_CLOSED;
    }
}

const char *tcp_error_reason(int s) {
    tcb_t *t = get(s);
    if (!t) return "conexão fechada";
    return t->err ? t->err : "erro de conexão";
}

int tcp_send(int s, const void *data, int len) {
    if (s >= TCP_MEM_BASE) return memsock_send(s, data, len);
    tcb_t *t = get(s);
    if (!t || t->user_closed || len < 0) return -1;
    if (t->st != S_SYN_SENT && t->st != S_ESTAB && t->st != S_CLOSE_WAIT) return -1;
    u32 room = TX_BUF - t->tx_len;
    u32 n = MIN((u32)len, room);
    const u8 *p = data;
    u32 tail = (t->tx_head + t->tx_len) % TX_BUF;
    u32 first = MIN(n, TX_BUF - tail);
    memcpy(t->tx + tail, p, first);
    if (first < n) memcpy(t->tx, p + first, n - first);
    t->tx_len += n;
    if (t->st != S_SYN_SENT) output(t);
    return (int)n;
}

int tcp_recv(int s, void *buf, int max) {
    if (s >= TCP_MEM_BASE) return memsock_recv(s, buf, max);
    tcb_t *t = get(s);
    if (!t || t->user_closed) return -1;
    if (t->rx_len == 0 || max <= 0) {
        if (t->st == S_ERR || t->peer_fin) return -1;
        return 0;
    }
    u32 n = MIN((u32)max, t->rx_len);
    u32 first = MIN(n, RX_BUF - t->rx_head);
    u8 *o = buf;
    memcpy(o, t->rx + t->rx_head, first);
    if (first < n) memcpy(o + first, t->rx, n - first);
    t->rx_head = (t->rx_head + n) % RX_BUF;
    t->rx_len -= n;
    /* anuncia a janela reaberta se o ganho for significativo */
    if (t->st != S_ERR && !t->peer_fin) {
        u32 right = t->rcv_nxt + rx_window(t);
        u32 gain = right - t->rcv_adv;
        if (SEQ_GT(right, t->rcv_adv) && gain >= MIN((u32)RX_BUF / 4, 2u * t->mss)) t->ack_now = true;
    }
    return (int)n;
}

/* bytes recebidos esperando leitura (para poll/kqueue dos apps, net/usersock.c) */
int tcp_avail(int s) {
    tcb_t *t = get(s);
    return t && !t->user_closed ? (int)t->rx_len : 0;
}

void tcp_close(int s) {
    tcb_t *t = get(s);
    if (!t || t->user_closed) return;
    t->user_closed = true;
    t->rx_len = 0;
    switch (t->st) {
    case S_SYN_SENT:
        send_raw(t->rip, t->lport, t->rport, t->snd_nxt, 0, F_RST, 0, NULL, 0, NULL, 0, 0);
        tcb_free(t);
        break;
    case S_ERR:
        tcb_free(t);
        break;
    case S_ESTAB:
    case S_CLOSE_WAIT:
        output(t);
        break;
    default:
        break;
    }
}

/* =================================================================== entrada */
static void reply_rst(u32 src, const u8 *p, u16 dlen) {
    u8 flags = p[13];
    if (flags & F_RST) return;
    u16 sport = rd16(p), dport = rd16(p + 2);
    u32 seq = rd32(p + 4), ack = rd32(p + 8);
    if (flags & F_ACK) send_raw(src, dport, sport, ack, 0, F_RST, 0, NULL, 0, NULL, 0, 0);
    else {
        u32 sl = dlen + ((flags & F_SYN) ? 1 : 0) + ((flags & F_FIN) ? 1 : 0);
        send_raw(src, dport, sport, 0, seq + sl, F_RST | F_ACK, 0, NULL, 0, NULL, 0, 0);
    }
}

static void parse_mss(tcb_t *t, const u8 *p, int hl) {
    for (int i = 20; i < hl;) {
        u8 k = p[i];
        if (k == 0) break;
        if (k == 1) { i++; continue; }
        if (i + 1 >= hl) break;
        u8 l = p[i + 1];
        if (l < 2 || i + l > hl) break;
        if (k == 2 && l == 4) {
            u16 m = rd16(p + i + 2);
            if (m >= 64) t->mss = (u16)MIN(m, (u16)OUR_MSS);
        }
        i += l;
    }
}

static void on_fin_acked(tcb_t *t, u64 now) {
    t->fin_acked = true;
    switch (t->st) {
    case S_FIN_WAIT1: t->st = S_FIN_WAIT2; t->close_at = now + FIN_WAIT2_MS; break;
    case S_CLOSING:   t->st = S_TIME_WAIT; t->close_at = now + TIME_WAIT_MS; break;
    case S_LAST_ACK:  tcb_free(t); break;
    }
}

void tcp_input(u32 src, u32 dst, const u8 *p, u16 len) {
    if (len < 20) return;
    u32 cs = inet_pseudo_sum(src, dst, IPPROTO_TCP, len);
    if (inet_csum_fold(inet_csum_add(cs, p, len)) != 0) return;
    int hl = (p[12] >> 4) * 4;
    if (hl < 20 || hl > len) return;
    u16 sport = rd16(p), dport = rd16(p + 2);
    u32 seq = rd32(p + 4), ack = rd32(p + 8);
    u8 flags = p[13];
    u16 win = rd16(p + 14);
    const u8 *data = p + hl;
    u32 dlen = len - hl;
    u64 now = timer_ticks();

    tcb_t *t = NULL;
    for (int i = 0; i < TCP_SOCKS; i++)
        if (socks[i].st != S_FREE && socks[i].rip == src && socks[i].rport == sport && socks[i].lport == dport) { t = &socks[i]; break; }
    if (!t || t->st == S_ERR) { reply_rst(src, p, (u16)dlen); return; }

    if (t->st == S_SYN_SENT) {
        if ((flags & F_ACK) && ack != t->iss + 1) { reply_rst(src, p, (u16)dlen); return; }
        if (flags & F_RST) { if (flags & F_ACK) set_error(t, "conexão recusada"); return; }
        if ((flags & F_SYN) && (flags & F_ACK)) {
            t->rcv_nxt = seq + 1;
            t->rcv_adv = t->rcv_nxt;
            parse_mss(t, p, hl);
            t->snd_una = ack;
            t->snd_nxt = ack;
            t->snd_max = ack;
            t->snd_wnd = win;
            t->st = S_ESTAB;
            t->rto_at = 0;
            if (t->retries == 0) rtt_sample(t, (u32)(now - t->rtt_t0));
            else t->rto = RTO_INIT;
            t->rtt_on = false;
            t->retries = 0;
            t->cwnd = 10u * t->mss;
            xmit(t, t->snd_nxt, F_ACK, 0, 0);
            output(t);
        }
        return;
    }

    /* RST: aceito se cair dentro da janela de recepção */
    if (flags & F_RST) {
        u32 w = MAX((u32)rx_window(t), 1u);
        if (SEQ_GE(seq, t->rcv_nxt) && SEQ_LT(seq, t->rcv_nxt + w)) {
            if (t->st == S_TIME_WAIT || t->st == S_LAST_ACK || t->user_closed) tcb_free(t);
            else set_error(t, "conexão reiniciada pelo servidor");
        }
        return;
    }
    if (flags & F_SYN) { t->ack_now = true; return; }   /* SYN-ACK duplicado: nosso ACK se perdeu */
    if (!(flags & F_ACK)) return;

    /* ---- processamento do ACK */
    if (SEQ_GT(ack, t->snd_max)) { t->ack_now = true; return; }
    if (SEQ_GT(ack, t->snd_una)) {
        u32 acked = ack - t->snd_una;
        u32 d = MIN(acked, t->tx_len);
        t->tx_head = (t->tx_head + d) % TX_BUF;
        t->tx_len -= d;
        t->snd_una = ack;
        if (SEQ_LT(t->snd_nxt, t->snd_una)) t->snd_nxt = t->snd_una;
        if (t->rtt_on && SEQ_GT(ack, t->rtt_seq)) { rtt_sample(t, (u32)(now - t->rtt_t0)); t->rtt_on = false; }
        t->retries = 0;
        t->dupacks = 0;
        if (t->cwnd < t->ssthresh) t->cwnd += t->mss;
        else t->cwnd += MAX(1u, (u32)t->mss * t->mss / t->cwnd);
        if (t->cwnd > 4 * TX_BUF) t->cwnd = 4 * TX_BUF;
        t->rto_at = (t->snd_una == t->snd_max) ? 0 : now + t->rto;
        t->snd_wnd = win;
        if (t->fin_ever && !t->fin_acked && SEQ_GE(ack, t->fin_seq + 1)) {
            int st0 = t->st;
            on_fin_acked(t, now);
            if (st0 == S_LAST_ACK) return;   /* slot liberado */
        }
    } else if (ack == t->snd_una) {
        if (dlen == 0 && !(flags & F_FIN) && t->snd_max != t->snd_una && win == t->snd_wnd) {
            if (++t->dupacks == 3) {
                u32 inflight = t->snd_max - t->snd_una;
                t->ssthresh = MAX(inflight / 2, 2u * t->mss);
                t->cwnd = t->ssthresh;
                retransmit_one(t);
                t->rtt_on = false;
            }
        }
        t->snd_wnd = win;
    }

    /* ---- dados e FIN */
    bool fin = (flags & F_FIN) != 0;
    if (dlen || fin) {
        if (t->peer_fin) { t->ack_now = true; goto out; }   /* retransmissão após o FIN */
        if (SEQ_LT(seq, t->rcv_nxt)) {
            u32 dup = t->rcv_nxt - seq;
            if (dup > dlen || (dup == dlen && !fin)) { t->ack_now = true; goto out; }
            data += dup; dlen -= dup; seq = t->rcv_nxt;
        }
        if (seq != t->rcv_nxt) { t->ack_now = true; goto out; }   /* fora de ordem: descarta, ACK duplicado */
        u32 n = dlen;
        if (t->user_closed) {
            /* aplicativo já fechou: confirma e descarta */
        } else {
            u32 room = RX_BUF - t->rx_len;
            if (n > room) n = room;
            u32 tail = (t->rx_head + t->rx_len) % RX_BUF;
            u32 first = MIN(n, RX_BUF - tail);
            memcpy(t->rx + tail, data, first);
            if (first < n) memcpy(t->rx, data + first, n - first);
            t->rx_len += n;
        }
        t->rcv_nxt += n;
        t->ack_now = true;
        if (fin && n == dlen) {
            t->rcv_nxt++;
            t->peer_fin = true;
            switch (t->st) {
            case S_ESTAB: t->st = S_CLOSE_WAIT; break;
            case S_FIN_WAIT1:
                if (t->fin_acked) { t->st = S_TIME_WAIT; t->close_at = now + TIME_WAIT_MS; }
                else t->st = S_CLOSING;
                break;
            case S_FIN_WAIT2: t->st = S_TIME_WAIT; t->close_at = now + TIME_WAIT_MS; break;
            }
        }
    }
out:
    output(t);
}

/* =================================================================== temporizadores */
static void on_rto(tcb_t *t, u64 now) {
    if (t->st == S_SYN_SENT) {
        if (++t->retries > SYN_RETRIES) { set_error(t, "tempo esgotado ao conectar"); return; }
        t->rto = MIN(t->rto * 2, (u32)RTO_MAX);
        t->rtt_on = false;
        xmit(t, t->iss, F_SYN, 0, 0);
        t->rto_at = now + t->rto;
        return;
    }
    u32 off = t->snd_nxt - t->snd_una;
    if (t->snd_wnd == 0 && t->tx_len > 0) {
        /* sonda de janela zero (persist): 1 byte além da janela, sem contar como falha */
        xmit(t, t->snd_una, F_ACK, 0, 1);
        t->snd_nxt = t->snd_una + 1;
        if (SEQ_GT(t->snd_nxt, t->snd_max)) t->snd_max = t->snd_nxt;
        t->rto = MIN(t->rto * 2, (u32)RTO_MAX);
        t->rto_at = now + t->rto;
        return;
    }
    if (t->snd_una == t->snd_max && !(t->tx_len > off)) { t->rto_at = 0; return; }
    if (++t->retries > DATA_RETRIES) { set_error(t, "tempo esgotado (sem resposta do servidor)"); return; }
    u32 inflight = t->snd_max - t->snd_una;
    t->ssthresh = MAX(inflight / 2, 2u * t->mss);
    t->cwnd = t->mss;
    t->rto = MIN(t->rto * 2, (u32)RTO_MAX);
    t->rtt_on = false;
    t->dupacks = 0;
    /* go-back-N */
    t->snd_nxt = t->snd_una;
    if (!t->fin_acked) t->fin_sent = false;
    t->rto_at = 0;
    output(t);
    if (!t->rto_at) t->rto_at = now + t->rto;
}

void tcp_tick(u64 now) {
    for (int i = 0; i < TCP_SOCKS; i++) {
        tcb_t *t = &socks[i];
        if (t->st == S_FREE || t->st == S_ERR) continue;
        if ((t->st == S_TIME_WAIT || t->st == S_FIN_WAIT2) && t->close_at && now >= t->close_at) {
            if (t->user_closed) { tcb_free(t); continue; }
        }
        if (t->rto_at && now >= t->rto_at) {
            on_rto(t, now);
            if (t->st == S_FREE || t->st == S_ERR) continue;
        }
        if (t->ack_now && t->st != S_SYN_SENT) xmit(t, t->snd_nxt, F_ACK, 0, 0);
    }
}
