/*
 * mOS — mnet: HTTP/2 (RFC 9113) sem E/S. Quadros, fluxos, controle de fluxo, SETTINGS/PING/GOAWAY/RST,
 * cabeçalhos em HEADERS + CONTINUATION com HPACK (mnet_hpack.c). Push desligado (SETTINGS_ENABLE_PUSH = 0).
 *
 * Fluxo: state bit 1 = fechado do nosso lado, bit 2 = fechado do lado do par; os dois = fechado, o fluxo
 * sai da lista depois de on_close (o ponteiro deixa de valer ali). A janela de recepção é reposta quando
 * os dados são entregues (o consumidor guarda o corpo; NSURLSession e libcurl fazem isso).
 */
#include "mnet.h"

enum { F_DATA, F_HEADERS, F_PRIORITY, F_RST, F_SETTINGS, F_PUSH, F_PING, F_GOAWAY, F_WINUP, F_CONT };
enum { FL_END_STREAM = 1, FL_ACK = 1, FL_END_HEADERS = 4, FL_PADDED = 8, FL_PRIORITY = 0x20 };
enum { ST_LOCAL = 1, ST_REMOTE = 2 };

#define PREFACE "PRI * HTTP/2.0\r\n\r\nSM\r\n\r\n"
#define PREFACE_LEN 24
#define WIN_MAX 0x7fffffffLL
#define LOCAL_STREAM_WIN (4LL << 20)     /* 4 MiB por fluxo */
#define LOCAL_CONN_WIN (16LL << 20)      /* 16 MiB na conexão */
#define HEADER_BLOCK_MAX (256 * 1024)

static uint32_t be32(const uint8_t *p) { return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3]; }
static void put32(uint8_t *p, uint32_t v) { p[0] = (uint8_t)(v >> 24); p[1] = (uint8_t)(v >> 16); p[2] = (uint8_t)(v >> 8); p[3] = (uint8_t)v; }

static int frame(mnet_h2_t *c, int type, int flags, uint32_t sid, const void *p, size_t n) {
    uint8_t h[9];
    h[0] = (uint8_t)(n >> 16); h[1] = (uint8_t)(n >> 8); h[2] = (uint8_t)n;
    h[3] = (uint8_t)type; h[4] = (uint8_t)flags;
    put32(h + 5, sid & 0x7fffffffu);
    return mnet_buf_put(&c->out, h, 9) | (n ? mnet_buf_put(&c->out, p, n) : 0);
}
static int winup(mnet_h2_t *c, uint32_t sid, uint32_t inc) {
    uint8_t b[4]; put32(b, inc & 0x7fffffffu);
    return frame(c, F_WINUP, 0, sid, b, 4);
}

int mnet_h2_init(mnet_h2_t *c, int is_server) {
    void *ctx = c->ctx;
    void (*oh)(void *, mnet_h2_stream_t *, const mnet_hdrs_t *, int) = c->on_headers;
    int (*od)(void *, mnet_h2_stream_t *, const uint8_t *, size_t, int) = c->on_data;
    void (*oc)(void *, mnet_h2_stream_t *, int) = c->on_close;
    void (*og)(void *, uint32_t, int) = c->on_goaway;
    void (*op)(void *, const uint8_t *) = c->on_ping_ack;
    memset(c, 0, sizeof *c);
    c->ctx = ctx; c->on_headers = oh; c->on_data = od; c->on_close = oc; c->on_goaway = og; c->on_ping_ack = op;
    c->is_server = is_server;
    c->next_id = is_server ? 2 : 1;
    c->peer_max_frame = 16384;
    c->peer_max_concurrent = 100;          /* até o SETTINGS dele chegar (sem limite pelo RFC; 100 é o recomendado) */
    c->peer_header_table = 4096;
    c->peer_max_header_list = 0xffffffffu;
    c->peer_initial_win = 65535;
    c->peer_enable_push = 1;
    c->local_max_frame = 16384;
    c->local_initial_win = LOCAL_STREAM_WIN;
    c->conn_recv_win = LOCAL_CONN_WIN;
    c->conn_send_win = 65535;
    mnet_hpack_init(&c->enc, 4096);
    mnet_hpack_init(&c->dec, 4096);
    int r = 0;
    if (!is_server) r |= mnet_buf_put(&c->out, PREFACE, PREFACE_LEN);
    uint8_t s[6 * 4], *p = s;
    struct { uint16_t id; uint32_t v; } set[4] = {
        {2, 0}, {3, 256}, {4, (uint32_t)LOCAL_STREAM_WIN}, {6, HEADER_BLOCK_MAX},
    };
    for (int i = 0; i < 4; i++) {
        if (is_server && set[i].id == 2) continue;          /* ENABLE_PUSH só faz sentido do cliente */
        p[0] = (uint8_t)(set[i].id >> 8); p[1] = (uint8_t)set[i].id; put32(p + 2, set[i].v); p += 6;
    }
    r |= frame(c, F_SETTINGS, 0, 0, s, (size_t)(p - s));
    r |= winup(c, 0, (uint32_t)(LOCAL_CONN_WIN - 65535));    /* a janela da conexão só muda por WINDOW_UPDATE */
    return r ? -1 : 0;
}

void mnet_h2_free(mnet_h2_t *c) {
    mnet_h2_stream_t *s = c->streams;
    while (s) { mnet_h2_stream_t *n = s->next; free(s); s = n; }
    c->streams = 0;
    mnet_buf_free(&c->in); mnet_buf_free(&c->out); mnet_buf_free(&c->hb);
    mnet_hpack_free(&c->enc); mnet_hpack_free(&c->dec);
}

mnet_h2_stream_t *mnet_h2_find(mnet_h2_t *c, uint32_t id) {
    for (mnet_h2_stream_t *s = c->streams; s; s = s->next) if (s->id == id) return s;
    return 0;
}
static mnet_h2_stream_t *new_stream(mnet_h2_t *c, uint32_t id) {
    mnet_h2_stream_t *s = calloc(1, sizeof *s);
    if (!s) return 0;
    s->id = id;
    s->send_win = c->peer_initial_win;
    s->recv_win = c->local_initial_win;
    s->next = c->streams;
    c->streams = s;
    c->open_streams++;
    return s;
}
static void drop_stream(mnet_h2_t *c, mnet_h2_stream_t *s, int code) {
    mnet_h2_stream_t **pp = &c->streams;
    while (*pp && *pp != s) pp = &(*pp)->next;
    if (!*pp) return;
    *pp = s->next;
    c->open_streams--;
    if (c->on_close) c->on_close(c->ctx, s, code);
    free(s);
}
static void close_side(mnet_h2_t *c, mnet_h2_stream_t *s, int side) {
    s->state |= side;
    if (s->state == (ST_LOCAL | ST_REMOTE)) drop_stream(c, s, MNET_H2_NO_ERROR);
}

static long conn_error(mnet_h2_t *c, int code, const char *why) {
    if (!c->goaway_sent) {
        uint8_t b[8];
        put32(b, c->last_peer_id);
        put32(b + 4, (uint32_t)code);
        frame(c, F_GOAWAY, 0, 0, b, 8);
        c->goaway_sent = 1;
    }
    c->dead = 1;
    c->err_code = code;
    c->err = why;
    return -1;
}
static int stream_error(mnet_h2_t *c, uint32_t sid, int code) {
    uint8_t b[4]; put32(b, (uint32_t)code);
    int r = frame(c, F_RST, 0, sid, b, 4);
    mnet_h2_stream_t *s = mnet_h2_find(c, sid);
    if (s) drop_stream(c, s, code);
    return r;
}

int mnet_h2_can_open(const mnet_h2_t *c) {
    return !c->dead && !c->goaway_recv && !c->goaway_sent && (uint32_t)c->open_streams < c->peer_max_concurrent &&
           c->next_id < 0x7fffffffu;
}

static int send_headers(mnet_h2_t *c, uint32_t sid, const mnet_hdrs_t *h, int end_stream) {
    mnet_buf_t blk = {0};
    if (mnet_hpack_encode(&c->enc, h, &blk, 1)) { mnet_buf_free(&blk); return -1; }
    size_t off = 0, n = blk.n, max = c->peer_max_frame;
    int r = 0, first = 1;
    do {
        size_t k = n - off < max ? n - off : max;
        int last = off + k == n;
        int fl = (last ? FL_END_HEADERS : 0) | (first && end_stream ? FL_END_STREAM : 0);
        r |= frame(c, first ? F_HEADERS : F_CONT, fl, sid, blk.p ? blk.p + off : 0, k);
        off += k;
        first = 0;
    } while (off < n);
    mnet_buf_free(&blk);
    return r ? -1 : 0;
}
mnet_h2_stream_t *mnet_h2_request(mnet_h2_t *c, const mnet_hdrs_t *h, int end_stream) {
    if (c->is_server || !mnet_h2_can_open(c)) return 0;
    mnet_h2_stream_t *s = new_stream(c, c->next_id);
    if (!s) return 0;
    c->next_id += 2;
    if (send_headers(c, s->id, h, end_stream)) { drop_stream(c, s, MNET_H2_INTERNAL_ERROR); return 0; }
    if (end_stream) s->state |= ST_LOCAL;
    return s;
}
int mnet_h2_respond(mnet_h2_t *c, mnet_h2_stream_t *s, const mnet_hdrs_t *h, int end_stream) {
    if (!s || (s->state & ST_LOCAL)) return -1;
    if (send_headers(c, s->id, h, end_stream)) return -1;
    if (end_stream) close_side(c, s, ST_LOCAL);
    return 0;
}
long long mnet_h2_send_window(const mnet_h2_t *c, const mnet_h2_stream_t *s) {
    long long w = c->conn_send_win;
    if (s && s->send_win < w) w = s->send_win;
    return w < 0 ? 0 : w;
}
long mnet_h2_send_data(mnet_h2_t *c, mnet_h2_stream_t *s, const uint8_t *d, size_t n, int end_stream) {
    if (!s || c->dead || (s->state & ST_LOCAL)) return -1;
    size_t sent = 0;
    while (sent < n) {
        long long w = mnet_h2_send_window(c, s);
        if (w <= 0) break;
        size_t k = n - sent;
        if ((long long)k > w) k = (size_t)w;
        if (k > c->peer_max_frame) k = c->peer_max_frame;
        int fin = end_stream && sent + k == n;
        if (frame(c, F_DATA, fin ? FL_END_STREAM : 0, s->id, d + sent, k)) return -1;
        c->conn_send_win -= (long long)k;
        s->send_win -= (long long)k;
        sent += k;
        if (fin) { close_side(c, s, ST_LOCAL); return (long)sent; }
    }
    if (n == 0 && end_stream) {
        if (frame(c, F_DATA, FL_END_STREAM, s->id, 0, 0)) return -1;
        close_side(c, s, ST_LOCAL);
    }
    return (long)sent;
}
int mnet_h2_reset(mnet_h2_t *c, mnet_h2_stream_t *s, int code) { return s ? stream_error(c, s->id, code) : -1; }
int mnet_h2_ping(mnet_h2_t *c, const uint8_t o[8]) { return frame(c, F_PING, 0, 0, o, 8); }
int mnet_h2_goaway(mnet_h2_t *c, int code) {
    if (c->goaway_sent) return 0;
    uint8_t b[8];
    put32(b, c->last_peer_id);
    put32(b + 4, (uint32_t)code);
    c->goaway_sent = 1;
    return frame(c, F_GOAWAY, 0, 0, b, 8);
}

/* ---- recepção */
static int valid_block(const mnet_hdrs_t *h, int is_trailer) {
    int pseudo_done = 0;
    for (int i = 0; i < h->n; i++) {
        const char *n = h->v[i].name;
        if (!*n) return 0;
        for (const char *p = n; *p; p++) if (*p >= 'A' && *p <= 'Z') return 0;   /* nomes em minúsculas (§8.2) */
        if (n[0] == ':') {
            if (pseudo_done || is_trailer) return 0;
        } else {
            pseudo_done = 1;
            /* campos de conexão são proibidos (§8.2.2) */
            if (!strcmp(n, "connection") || !strcmp(n, "keep-alive") || !strcmp(n, "proxy-connection") ||
                !strcmp(n, "transfer-encoding") || !strcmp(n, "upgrade")) return 0;
            if (!strcmp(n, "te") && strcmp(h->v[i].value, "trailers")) return 0;
        }
    }
    return 1;
}
static long header_block_done(mnet_h2_t *c) {
    uint32_t sid = c->hb_stream;
    int end_stream = c->hb_end_stream;
    c->hb_stream = 0;
    mnet_hdrs_t h = {0};
    int bad = mnet_hpack_decode(&c->dec, c->hb.p, c->hb.n, &h, 4096);
    c->hb.n = 0;
    if (bad) { mnet_hdrs_free(&h); return conn_error(c, MNET_H2_COMPRESSION_ERROR, "HPACK inválido"); }
    mnet_h2_stream_t *s = mnet_h2_find(c, sid);
    if (!s) { mnet_hdrs_free(&h); return 0; }                      /* fluxo já resetado: o bloco só alimenta o HPACK */
    /* cliente: depois de 1xx vem outra cabeça; depois da final, só trailer. Servidor: a segunda é trailer. */
    int trailer = c->is_server ? s->got_headers != 0 : s->got_headers == 2;
    if (!valid_block(&h, trailer) || (trailer && !end_stream)) {
        mnet_hdrs_free(&h);
        stream_error(c, sid, MNET_H2_PROTOCOL_ERROR);
        return 0;
    }
    if (!trailer) {
        const char *k = mnet_hdrs_get(&h, c->is_server ? ":method" : ":status");
        if (!k || (!c->is_server && strlen(k) != 3)) { mnet_hdrs_free(&h); stream_error(c, sid, MNET_H2_PROTOCOL_ERROR); return 0; }
        s->got_headers = !c->is_server && k[0] == '1' ? 1 : 2;
        if (s->got_headers == 1 && end_stream) { mnet_hdrs_free(&h); stream_error(c, sid, MNET_H2_PROTOCOL_ERROR); return 0; }
    }
    if (c->on_headers) c->on_headers(c->ctx, s, &h, end_stream);
    mnet_hdrs_free(&h);
    s = mnet_h2_find(c, sid);                                        /* o callback pode ter resetado */
    if (s && end_stream) close_side(c, s, ST_REMOTE);
    return 0;
}
/* tira o enchimento (PADDED) e, em HEADERS, a prioridade */
static int strip(int flags, int prio_ok, const uint8_t **p, size_t *n) {
    size_t pad = 0;
    if (flags & FL_PADDED) {
        if (*n < 1) return -1;
        pad = (*p)[0];
        (*p)++; (*n)--;
    }
    if (prio_ok && (flags & FL_PRIORITY)) {
        if (*n < 5) return -1;
        *p += 5; *n -= 5;
    }
    if (pad > *n) return -1;
    *n -= pad;
    return 0;
}
static long apply_settings(mnet_h2_t *c, const uint8_t *p, size_t n) {
    for (size_t i = 0; i + 6 <= n; i += 6) {
        uint16_t id = (uint16_t)(p[i] << 8 | p[i + 1]);
        uint32_t v = be32(p + i + 2);
        switch (id) {
        case 1: c->peer_header_table = v; mnet_hpack_set_max(&c->enc, v < 4096 ? v : 4096); break;
        case 2: if (v > 1) return conn_error(c, MNET_H2_PROTOCOL_ERROR, "ENABLE_PUSH inválido");
                if (c->is_server == 0 && v) return conn_error(c, MNET_H2_PROTOCOL_ERROR, "servidor mandou ENABLE_PUSH");
                c->peer_enable_push = (int)v; break;
        case 3: c->peer_max_concurrent = v; break;
        case 4: {
            if (v > WIN_MAX) return conn_error(c, MNET_H2_FLOW_CONTROL_ERROR, "INITIAL_WINDOW_SIZE grande demais");
            long long delta = (long long)v - c->peer_initial_win;
            for (mnet_h2_stream_t *s = c->streams; s; s = s->next) {
                s->send_win += delta;
                if (s->send_win > WIN_MAX) return conn_error(c, MNET_H2_FLOW_CONTROL_ERROR, "janela estourou");
            }
            c->peer_initial_win = v;
            break;
        }
        case 5: if (v < 16384 || v > 16777215) return conn_error(c, MNET_H2_PROTOCOL_ERROR, "MAX_FRAME_SIZE inválido");
                c->peer_max_frame = v; break;
        case 6: c->peer_max_header_list = v; break;
        default: break;                                               /* desconhecido: ignora (§6.5.2) */
        }
    }
    return frame(c, F_SETTINGS, FL_ACK, 0, 0, 0) ? -1 : 0;
}

static long on_frame(mnet_h2_t *c, int type, int flags, uint32_t sid, const uint8_t *p, size_t n) {
    if (c->hb_stream && (type != F_CONT || sid != c->hb_stream))
        return conn_error(c, MNET_H2_PROTOCOL_ERROR, "esperava CONTINUATION");
    if (!c->preface_done) {
        if (type != F_SETTINGS || (flags & FL_ACK)) return conn_error(c, MNET_H2_PROTOCOL_ERROR, "o primeiro quadro não é SETTINGS");
        c->preface_done = 1;
    }
    mnet_h2_stream_t *s = sid ? mnet_h2_find(c, sid) : 0;
    switch (type) {
    case F_DATA: {
        if (!sid) return conn_error(c, MNET_H2_PROTOCOL_ERROR, "DATA no fluxo 0");
        if ((long long)n > c->conn_recv_win) return conn_error(c, MNET_H2_FLOW_CONTROL_ERROR, "janela da conexão excedida");
        c->conn_recv_win -= (long long)n;
        c->conn_recv_unacked += (long long)n;
        if (c->conn_recv_unacked >= LOCAL_CONN_WIN / 2) {
            winup(c, 0, (uint32_t)c->conn_recv_unacked);
            c->conn_recv_win += c->conn_recv_unacked;
            c->conn_recv_unacked = 0;
        }
        const uint8_t *d = p; size_t dn = n;
        if (strip(flags, 0, &d, &dn)) return conn_error(c, MNET_H2_PROTOCOL_ERROR, "enchimento inválido");
        if (!s) {
            if (sid >= c->next_id && !c->is_server && (sid & 1)) return conn_error(c, MNET_H2_PROTOCOL_ERROR, "DATA em fluxo ocioso");
            return stream_error(c, sid, MNET_H2_STREAM_CLOSED) ? -1 : 0;
        }
        if (s->state & ST_REMOTE) return stream_error(c, sid, MNET_H2_STREAM_CLOSED) ? -1 : 0;
        if (!s->got_headers) return stream_error(c, sid, MNET_H2_PROTOCOL_ERROR) ? -1 : 0;
        if ((long long)n > s->recv_win) return stream_error(c, sid, MNET_H2_FLOW_CONTROL_ERROR) ? -1 : 0;
        s->recv_win -= (long long)n;
        s->recv_unacked += (long long)n;
        int end = flags & FL_END_STREAM;
        if (c->on_data && c->on_data(c->ctx, s, d, dn, end)) return stream_error(c, sid, MNET_H2_CANCEL) ? -1 : 0;
        s = mnet_h2_find(c, sid);
        if (!s) return 0;
        if (!end && !c->manual_win && s->recv_unacked >= c->local_initial_win / 2) {
            winup(c, sid, (uint32_t)s->recv_unacked);
            s->recv_win += s->recv_unacked;
            s->recv_unacked = 0;
        }
        if (end) close_side(c, s, ST_REMOTE);
        return 0;
    }
    case F_HEADERS: {
        if (!sid) return conn_error(c, MNET_H2_PROTOCOL_ERROR, "HEADERS no fluxo 0");
        const uint8_t *d = p; size_t dn = n;
        if (strip(flags, 1, &d, &dn)) return conn_error(c, MNET_H2_PROTOCOL_ERROR, "enchimento inválido");
        if (!s) {
            if (!c->is_server) {
                if ((sid & 1) && sid < c->next_id) {
                    /* fluxo nosso já fechado: decodifica o bloco para manter o HPACK em dia */
                } else return conn_error(c, MNET_H2_PROTOCOL_ERROR, "HEADERS em fluxo que não abrimos");
            } else {
                if (!(sid & 1) || sid <= c->last_peer_id) return conn_error(c, MNET_H2_PROTOCOL_ERROR, "id de fluxo inválido");
                c->last_peer_id = sid;
                if (c->goaway_sent || (uint32_t)c->open_streams >= 256) {
                    stream_error(c, sid, MNET_H2_REFUSED_STREAM);
                } else if (!new_stream(c, sid)) return conn_error(c, MNET_H2_INTERNAL_ERROR, "sem memória");
            }
        } else if (s->state & ST_REMOTE) {
            stream_error(c, sid, MNET_H2_STREAM_CLOSED);
        }
        c->hb.n = 0;
        if (mnet_buf_put(&c->hb, d, dn)) return conn_error(c, MNET_H2_INTERNAL_ERROR, "sem memória");
        c->hb_stream = sid;
        c->hb_end_stream = flags & FL_END_STREAM;
        if (flags & FL_END_HEADERS) return header_block_done(c);
        return 0;
    }
    case F_CONT:
        if (!c->hb_stream) return conn_error(c, MNET_H2_PROTOCOL_ERROR, "CONTINUATION fora de lugar");
        if (c->hb.n + n > HEADER_BLOCK_MAX) return conn_error(c, MNET_H2_ENHANCE_YOUR_CALM, "bloco de cabeçalho grande demais");
        if (mnet_buf_put(&c->hb, p, n)) return conn_error(c, MNET_H2_INTERNAL_ERROR, "sem memória");
        if (flags & FL_END_HEADERS) return header_block_done(c);
        return 0;
    case F_PRIORITY:
        if (!sid) return conn_error(c, MNET_H2_PROTOCOL_ERROR, "PRIORITY no fluxo 0");
        if (n != 5) return stream_error(c, sid, MNET_H2_FRAME_SIZE_ERROR) ? -1 : 0;
        return 0;                                                     /* prioridade é só dica (RFC 9113 a deprecou) */
    case F_RST:
        if (!sid) return conn_error(c, MNET_H2_PROTOCOL_ERROR, "RST_STREAM no fluxo 0");
        if (n != 4) return conn_error(c, MNET_H2_FRAME_SIZE_ERROR, "RST_STREAM com tamanho errado");
        if (!s && sid >= c->next_id && (sid & 1) == (c->is_server ? 0u : 1u))
            return conn_error(c, MNET_H2_PROTOCOL_ERROR, "RST_STREAM em fluxo ocioso");
        if (s) drop_stream(c, s, (int)be32(p));
        return 0;
    case F_SETTINGS:
        if (sid) return conn_error(c, MNET_H2_PROTOCOL_ERROR, "SETTINGS fora do fluxo 0");
        if (flags & FL_ACK) {
            if (n) return conn_error(c, MNET_H2_FRAME_SIZE_ERROR, "SETTINGS ACK com corpo");
            c->settings_acked = 1;
            return 0;
        }
        if (n % 6) return conn_error(c, MNET_H2_FRAME_SIZE_ERROR, "SETTINGS com tamanho errado");
        return apply_settings(c, p, n);
    case F_PUSH:
        return conn_error(c, MNET_H2_PROTOCOL_ERROR, "PUSH_PROMISE com push desligado");
    case F_PING:
        if (sid) return conn_error(c, MNET_H2_PROTOCOL_ERROR, "PING fora do fluxo 0");
        if (n != 8) return conn_error(c, MNET_H2_FRAME_SIZE_ERROR, "PING com tamanho errado");
        if (flags & FL_ACK) { if (c->on_ping_ack) c->on_ping_ack(c->ctx, p); return 0; }
        return frame(c, F_PING, FL_ACK, 0, p, 8) ? -1 : 0;
    case F_GOAWAY: {
        if (sid) return conn_error(c, MNET_H2_PROTOCOL_ERROR, "GOAWAY fora do fluxo 0");
        if (n < 8) return conn_error(c, MNET_H2_FRAME_SIZE_ERROR, "GOAWAY curto");
        uint32_t last = be32(p) & 0x7fffffffu;
        int code = (int)be32(p + 4);
        c->goaway_recv = 1;
        c->goaway_last = last;
        /* fluxos nossos acima do último processado não foram vistos: podem ser refeitos noutra conexão */
        for (mnet_h2_stream_t *x = c->streams, *nx; x; x = nx) {
            nx = x->next;
            if (x->id > last && (x->id & 1) == (c->is_server ? 0u : 1u)) drop_stream(c, x, MNET_H2_REFUSED_STREAM);
        }
        if (c->on_goaway) c->on_goaway(c->ctx, last, code);
        return 0;
    }
    case F_WINUP: {
        if (n != 4) return conn_error(c, MNET_H2_FRAME_SIZE_ERROR, "WINDOW_UPDATE com tamanho errado");
        uint32_t inc = be32(p) & 0x7fffffffu;
        if (!sid) {
            if (!inc) return conn_error(c, MNET_H2_PROTOCOL_ERROR, "WINDOW_UPDATE zero");
            c->conn_send_win += inc;
            if (c->conn_send_win > WIN_MAX) return conn_error(c, MNET_H2_FLOW_CONTROL_ERROR, "janela da conexão estourou");
            return 0;
        }
        if (!s) return 0;
        if (!inc) return stream_error(c, sid, MNET_H2_PROTOCOL_ERROR) ? -1 : 0;
        s->send_win += inc;
        if (s->send_win > WIN_MAX) return stream_error(c, sid, MNET_H2_FLOW_CONTROL_ERROR) ? -1 : 0;
        return 0;
    }
    default:
        return 0;                                                     /* tipo desconhecido: ignora (§5.5) */
    }
}

long mnet_h2_feed(mnet_h2_t *c, const uint8_t *d, size_t n) {
    if (c->dead) return -1;
    if (mnet_buf_put(&c->in, d, n)) return conn_error(c, MNET_H2_INTERNAL_ERROR, "sem memória");
    size_t off = 0;
    if (c->is_server && !c->preface_in) {                            /* o prefácio do cliente vem antes de tudo */
        size_t k = c->in.n < PREFACE_LEN ? c->in.n : PREFACE_LEN;
        if (memcmp(c->in.p, PREFACE, k)) return conn_error(c, MNET_H2_PROTOCOL_ERROR, "prefácio inválido");
        if (c->in.n < PREFACE_LEN) return (long)n;
        off = PREFACE_LEN;
        c->preface_in = 1;
    }
    while (c->in.n - off >= 9 && !c->dead) {
        const uint8_t *h = c->in.p + off;
        size_t len = (size_t)h[0] << 16 | (size_t)h[1] << 8 | h[2];
        if (len > c->local_max_frame) { mnet_buf_consume(&c->in, off); return conn_error(c, MNET_H2_FRAME_SIZE_ERROR, "quadro grande demais"); }
        if (c->in.n - off < 9 + len) break;
        long r = on_frame(c, h[3], h[4], be32(h + 5) & 0x7fffffffu, h + 9, len);
        off += 9 + len;
        if (r < 0) { mnet_buf_consume(&c->in, off); return -1; }
    }
    mnet_buf_consume(&c->in, off);
    return (long)n;
}
/* modo manual_win: o dono do fluxo avisa o que já consumiu; a janela volta em blocos de meia janela */
int mnet_h2_consumed(mnet_h2_t *c, mnet_h2_stream_t *s, size_t n) {
    if (!c || !s || !n) return 0;
    s->consumed += (long long)n;
    if (s->consumed > s->recv_unacked) s->consumed = s->recv_unacked;
    if ((s->state & ST_REMOTE) || s->consumed < c->local_initial_win / 2) return 0;
    int r = winup(c, s->id, (uint32_t)s->consumed);
    s->recv_win += s->consumed;
    s->recv_unacked -= s->consumed;
    s->consumed = 0;
    return r;
}
