/*
 * mOS — mnet: WebSocket (RFC 6455) e permessage-deflate (RFC 7692), sem E/S.
 * Cliente mascara todo quadro; servidor exige máscara. Mensagens fragmentadas são juntadas; controle (ping/pong/close)
 * pode vir no meio. Texto é conferido como UTF-8 (1007). Erro de protocolo enfileira o close certo (1002/1007/1009).
 */
#include "mnet.h"
#define Z_SOLO 1
#include "../third_party/zlib/zlib.h"

/* ---------------------------------------------------------------- SHA-1 (FIPS 180-4) e base64 */
static uint32_t rol(uint32_t x, int n) { return x << n | x >> (32 - n); }
static void sha1_block(uint32_t h[5], const uint8_t *p) {
    uint32_t w[80];
    for (int i = 0; i < 16; i++) w[i] = (uint32_t)p[4 * i] << 24 | (uint32_t)p[4 * i + 1] << 16 | (uint32_t)p[4 * i + 2] << 8 | p[4 * i + 3];
    for (int i = 16; i < 80; i++) w[i] = rol(w[i - 3] ^ w[i - 8] ^ w[i - 14] ^ w[i - 16], 1);
    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4];
    for (int i = 0; i < 80; i++) {
        uint32_t f, k;
        if (i < 20) { f = (b & c) | (~b & d); k = 0x5A827999; }
        else if (i < 40) { f = b ^ c ^ d; k = 0x6ED9EBA1; }
        else if (i < 60) { f = (b & c) | (b & d) | (c & d); k = 0x8F1BBCDC; }
        else { f = b ^ c ^ d; k = 0xCA62C1D6; }
        uint32_t t = rol(a, 5) + f + e + k + w[i];
        e = d; d = c; c = rol(b, 30); b = a; a = t;
    }
    h[0] += a; h[1] += b; h[2] += c; h[3] += d; h[4] += e;
}
void mnet_sha1(const void *data, size_t n, uint8_t out[20]) {
    uint32_t h[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    const uint8_t *p = data;
    size_t i = 0;
    for (; i + 64 <= n; i += 64) sha1_block(h, p + i);
    uint8_t last[128] = {0};
    size_t r = n - i;
    memcpy(last, p + i, r);
    last[r] = 0x80;
    size_t tot = r + 1 + 8 <= 64 ? 64 : 128;
    uint64_t bits = (uint64_t)n * 8;
    for (int k = 0; k < 8; k++) last[tot - 1 - k] = (uint8_t)(bits >> (8 * k));
    sha1_block(h, last);
    if (tot == 128) sha1_block(h, last + 64);
    for (int k = 0; k < 5; k++) { out[4 * k] = (uint8_t)(h[k] >> 24); out[4 * k + 1] = (uint8_t)(h[k] >> 16); out[4 * k + 2] = (uint8_t)(h[k] >> 8); out[4 * k + 3] = (uint8_t)h[k]; }
}
size_t mnet_b64(const uint8_t *in, size_t n, char *o) {
    static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t k = 0;
    for (size_t i = 0; i < n; i += 3) {
        uint32_t v = (uint32_t)in[i] << 16 | (i + 1 < n ? (uint32_t)in[i + 1] << 8 : 0) | (i + 2 < n ? in[i + 2] : 0);
        o[k++] = T[v >> 18 & 63]; o[k++] = T[v >> 12 & 63];
        o[k++] = i + 1 < n ? T[v >> 6 & 63] : '='; o[k++] = i + 2 < n ? T[v & 63] : '=';
    }
    o[k] = 0;
    return k;
}
void mnet_ws_key(const uint8_t rnd[16], char out[25]) { mnet_b64(rnd, 16, out); }
void mnet_ws_accept(const char *key, char out[29]) {
    static const char G[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    char buf[128];
    size_t kl = strlen(key);
    if (kl > 80) kl = 80;
    memcpy(buf, key, kl); memcpy(buf + kl, G, sizeof G - 1);
    uint8_t h[20];
    mnet_sha1(buf, kl + sizeof G - 1, h);
    mnet_b64(h, 20, out);
}

int mnet_utf8_valid(const uint8_t *s, size_t n) {
    size_t i = 0;
    while (i < n) {
        uint8_t c = s[i];
        if (c < 0x80) { i++; continue; }
        int len; uint32_t cp;
        if ((c & 0xE0) == 0xC0) { len = 2; cp = c & 0x1F; }
        else if ((c & 0xF0) == 0xE0) { len = 3; cp = c & 0x0F; }
        else if ((c & 0xF8) == 0xF0) { len = 4; cp = c & 0x07; }
        else return 0;
        if (i + (size_t)len > n) return 0;
        for (int k = 1; k < len; k++) { if ((s[i + k] & 0xC0) != 0x80) return 0; cp = cp << 6 | (s[i + k] & 0x3F); }
        if ((len == 2 && cp < 0x80) || (len == 3 && cp < 0x800) || (len == 4 && cp < 0x10000) || cp > 0x10FFFF ||
            (cp >= 0xD800 && cp <= 0xDFFF)) return 0;
        i += (size_t)len;
    }
    return 1;
}

/* ---------------------------------------------------------------- handshake */
int mnet_ws_request(mnet_buf_t *o, const char *target, const char *host, const char *origin, const char *key,
                    const char *protocols, const mnet_hdrs_t *extra, int offer_deflate) {
    int r = 0;
    r |= mnet_buf_puts(o, "GET "); r |= mnet_buf_puts(o, target && *target ? target : "/");
    r |= mnet_buf_puts(o, " HTTP/1.1\r\nHost: "); r |= mnet_buf_puts(o, host);
    r |= mnet_buf_puts(o, "\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Key: "); r |= mnet_buf_puts(o, key);
    r |= mnet_buf_puts(o, "\r\nSec-WebSocket-Version: 13\r\n");
    if (origin) { r |= mnet_buf_puts(o, "Origin: "); r |= mnet_buf_puts(o, origin); r |= mnet_buf_puts(o, "\r\n"); }
    if (protocols && *protocols) { r |= mnet_buf_puts(o, "Sec-WebSocket-Protocol: "); r |= mnet_buf_puts(o, protocols); r |= mnet_buf_puts(o, "\r\n"); }
    if (offer_deflate) r |= mnet_buf_puts(o, "Sec-WebSocket-Extensions: permessage-deflate; client_max_window_bits\r\n");
    for (int i = 0; extra && i < extra->n; i++) {
        const char *n = extra->v[i].name;
        if (mnet_ieq(n, "host") || mnet_ieq(n, "upgrade") || mnet_ieq(n, "connection") || mnet_ieqn(n, "sec-websocket-", 14)) continue;
        int bad = 0;
        for (const char *p = extra->v[i].value; *p; p++) if (*p == '\r' || *p == '\n') bad = 1;
        if (bad) continue;
        r |= mnet_buf_puts(o, n); r |= mnet_buf_puts(o, ": "); r |= mnet_buf_puts(o, extra->v[i].value); r |= mnet_buf_puts(o, "\r\n");
    }
    r |= mnet_buf_puts(o, "\r\n");
    return r ? -1 : 0;
}
/* lê "permessage-deflate; a; b=10" e aplica; client = quem fala é o cliente (servidor lendo a oferta) */
static int parse_pmd(mnet_ws_t *ws, const char *ext, int from_server, int *ok) {
    *ok = 0;
    const char *p = ext;
    while (*p) {
        while (*p == ' ' || *p == ',') p++;
        const char *s = p;
        while (*p && *p != ',') p++;
        /* uma extensão: nome; parâmetros */
        const char *e = p, *q = s;
        while (q < e && *q != ';' && *q != ' ') q++;
        if ((size_t)(q - s) != 18 || !mnet_ieqn(s, "permessage-deflate", 18)) { if (from_server) return -1; continue; }
        int my_no = 0, peer_no = 0, my_bits = 15, peer_bits = 15, bad = 0;
        while (q < e) {
            while (q < e && (*q == ';' || *q == ' ')) q++;
            const char *ns = q;
            while (q < e && *q != ';' && *q != '=' && *q != ' ') q++;
            size_t nl = (size_t)(q - ns);
            int val = -1;
            while (q < e && *q == ' ') q++;
            if (q < e && *q == '=') {
                q++;
                while (q < e && (*q == ' ' || *q == '"')) q++;
                val = 0;
                while (q < e && *q >= '0' && *q <= '9') val = val * 10 + (*q++ - '0');
                while (q < e && *q == '"') q++;
            }
            if (!nl) continue;
            /* do ponto de vista de quem recebe: "client_*" é o cliente */
            int about_client = nl >= 7 && mnet_ieqn(ns, "client_", 7);
            int is_mine = ws->is_server ? !about_client : about_client;
            if ((nl == 26 && mnet_ieqn(ns, "server_no_context_takeover", 26)) || (nl == 26 && mnet_ieqn(ns, "client_no_context_takeover", 26))) {
                if (is_mine) my_no = 1; else peer_no = 1;
            } else if (nl == 22 && (mnet_ieqn(ns, "server_max_window_bits", 22) || mnet_ieqn(ns, "client_max_window_bits", 22))) {
                if (val == -1) { if (from_server || !about_client) bad = 1; continue; }     /* client_max_window_bits sem valor só na oferta */
                if (val < 8 || val > 15) bad = 1;
                if (is_mine) my_bits = val; else peer_bits = val;
            } else bad = 1;
        }
        if (bad) { if (from_server) return -1; continue; }
        if (my_bits < 9) { if (from_server) return -1; continue; }   /* a zlib não comprime com janela de 256 bytes */
        ws->deflate = 1;
        ws->my_no_ctx = my_no; ws->peer_no_ctx = peer_no;
        ws->my_bits = my_bits; ws->peer_bits = peer_bits;
        *ok = 1;
        return 0;
    }
    return 0;
}
int mnet_ws_client_accept(mnet_ws_t *ws, const mnet_h1_t *resp, const char *key, const char *protocols, char **proto, const char **why) {
    *proto = 0;
    if (resp->status != 101) { *why = "o servidor não aceitou o WebSocket"; return -1; }
    char *up = mnet_hdrs_join(&resp->hdrs, "upgrade"), *cn = mnet_hdrs_join(&resp->hdrs, "connection");
    int ok = up && mnet_tok_has(up, "websocket") && cn && mnet_tok_has(cn, "upgrade");
    free(up); free(cn);
    if (!ok) { *why = "resposta 101 sem Upgrade: websocket"; return -1; }
    char acc[29];
    mnet_ws_accept(key, acc);
    const char *a = mnet_hdrs_get(&resp->hdrs, "sec-websocket-accept");
    if (!a || strcmp(a, acc)) { *why = "Sec-WebSocket-Accept errado"; return -1; }
    const char *pr = mnet_hdrs_get(&resp->hdrs, "sec-websocket-protocol");
    if (pr) {
        if (!protocols || !mnet_tok_has(protocols, pr)) { *why = "subprotocolo que não pedimos"; return -1; }
        *proto = mnet_strndup(pr, strlen(pr));
    }
    char *ex = mnet_hdrs_join(&resp->hdrs, "sec-websocket-extensions");
    if (ex) {
        int okx;
        int r = parse_pmd(ws, ex, 1, &okx);
        free(ex);
        if (r) { *why = "extensão que não oferecemos"; free(*proto); *proto = 0; return -1; }
    }
    return 0;
}
int mnet_ws_server_accept(mnet_ws_t *ws, const mnet_hdrs_t *req, const char *const *sup, mnet_buf_t *o, char **proto) {
    *proto = 0;
    const char *key = mnet_hdrs_get(req, "sec-websocket-key"), *ver = mnet_hdrs_get(req, "sec-websocket-version");
    char *up = mnet_hdrs_join(req, "upgrade"), *cn = mnet_hdrs_join(req, "connection");
    int ok = key && strlen(key) == 24 && ver && !strcmp(ver, "13") && up && mnet_tok_has(up, "websocket") && cn && mnet_tok_has(cn, "upgrade");
    free(up); free(cn);
    if (!ok) { mnet_buf_puts(o, "HTTP/1.1 400 Bad Request\r\nSec-WebSocket-Version: 13\r\nContent-Length: 0\r\n\r\n"); return -1; }
    char acc[29];
    mnet_ws_accept(key, acc);
    mnet_buf_puts(o, "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ");
    mnet_buf_puts(o, acc); mnet_buf_puts(o, "\r\n");
    char *pl = mnet_hdrs_join(req, "sec-websocket-protocol");
    for (int i = 0; pl && sup && sup[i]; i++)
        if (mnet_tok_has(pl, sup[i])) { *proto = mnet_strndup(sup[i], strlen(sup[i])); break; }
    free(pl);
    if (*proto) { mnet_buf_puts(o, "Sec-WebSocket-Protocol: "); mnet_buf_puts(o, *proto); mnet_buf_puts(o, "\r\n"); }
    char *ex = mnet_hdrs_join(req, "sec-websocket-extensions");
    if (ex) {
        int okx;
        parse_pmd(ws, ex, 0, &okx);
        free(ex);
        if (okx) {
            mnet_buf_puts(o, "Sec-WebSocket-Extensions: permessage-deflate");
            if (ws->peer_no_ctx) mnet_buf_puts(o, "; client_no_context_takeover");
            if (ws->my_no_ctx) mnet_buf_puts(o, "; server_no_context_takeover");
            if (ws->my_bits < 15) { mnet_buf_puts(o, "; server_max_window_bits="); mnet_buf_putu(o, (unsigned)ws->my_bits); }
            mnet_buf_puts(o, "\r\n");
        }
    }
    mnet_buf_puts(o, "\r\n");
    return 0;
}

/* ---------------------------------------------------------------- quadros */
static voidpf za(voidpf o, uInt n, uInt m) { (void)o; return calloc(n, m); }
static void zf(voidpf o, voidpf p) { (void)o; free(p); }
int mnet_ws_init(mnet_ws_t *ws, int is_server) {
    mnet_ws_t keep = *ws;
    memset(ws, 0, sizeof *ws);
    ws->ctx = keep.ctx; ws->on_message = keep.on_message; ws->on_ping = keep.on_ping; ws->on_pong = keep.on_pong;
    ws->on_close = keep.on_close; ws->random = keep.random;
    ws->deflate = keep.deflate; ws->my_no_ctx = keep.my_no_ctx; ws->peer_no_ctx = keep.peer_no_ctx;
    ws->my_bits = keep.my_bits ? keep.my_bits : 15; ws->peer_bits = keep.peer_bits ? keep.peer_bits : 15;
    ws->max_msg = keep.max_msg;
    ws->is_server = is_server;
    return 0;
}
void mnet_ws_free(mnet_ws_t *ws) {
    if (ws->zin) { inflateEnd(ws->zin); free(ws->zin); }
    if (ws->zout) { deflateEnd(ws->zout); free(ws->zout); }
    ws->zin = ws->zout = 0;
    mnet_buf_free(&ws->in); mnet_buf_free(&ws->msg); mnet_buf_free(&ws->out);
}
static int put_frame(mnet_ws_t *ws, int fin, int rsv1, int op, const uint8_t *d, size_t n) {
    uint8_t h[14];
    size_t hl = 2;
    h[0] = (uint8_t)((fin ? 0x80 : 0) | (rsv1 ? 0x40 : 0) | op);
    uint8_t m = ws->is_server ? 0 : 0x80;
    if (n < 126) h[1] = (uint8_t)(m | n);
    else if (n < 65536) { h[1] = m | 126; h[2] = (uint8_t)(n >> 8); h[3] = (uint8_t)n; hl = 4; }
    else { h[1] = m | 127; for (int i = 0; i < 8; i++) h[2 + i] = (uint8_t)((uint64_t)n >> (56 - 8 * i)); hl = 10; }
    uint8_t key[4] = {0, 0, 0, 0};
    if (!ws->is_server) {
        if (ws->random) ws->random(ws->ctx, key, 4);
        memcpy(h + hl, key, 4); hl += 4;
    }
    if (mnet_buf_put(&ws->out, h, hl)) return -1;
    if (!n) return 0;
    size_t at = ws->out.n;
    if (mnet_buf_put(&ws->out, d, n)) return -1;
    if (!ws->is_server) for (size_t i = 0; i < n; i++) ws->out.p[at + i] ^= key[i & 3];
    return 0;
}
static int deflate_msg(mnet_ws_t *ws, const uint8_t *d, size_t n, mnet_buf_t *o) {
    if (!ws->zout) {
        z_stream *z = calloc(1, sizeof *z);
        if (!z) return -1;
        z->zalloc = za; z->zfree = zf;
        if (deflateInit2(z, 6, Z_DEFLATED, -ws->my_bits, 8, Z_DEFAULT_STRATEGY) != Z_OK) { free(z); return -1; }
        ws->zout = z;
    }
    z_stream *z = ws->zout;
    uint8_t buf[16384];
    z->next_in = (Bytef *)d; z->avail_in = (uInt)n;
    do {
        z->next_out = buf; z->avail_out = sizeof buf;
        if (deflate(z, Z_SYNC_FLUSH) == Z_STREAM_ERROR) return -1;
        if (mnet_buf_put(o, buf, sizeof buf - z->avail_out)) return -1;
    } while (z->avail_out == 0);
    if (o->n >= 4 && !memcmp(o->p + o->n - 4, "\x00\x00\xff\xff", 4)) o->n -= 4;     /* §7.2.1 */
    if (!o->n) mnet_buf_putc(o, 0);                                                    /* bloco vazio: 0x00 */
    if (ws->my_no_ctx) deflateReset(z);
    return 0;
}
int mnet_ws_send(mnet_ws_t *ws, int op, const void *d, size_t n) {
    if (ws->close_sent) return -1;
    if (ws->deflate && (op == MNET_WS_TEXT || op == MNET_WS_BINARY)) {
        mnet_buf_t c = {0};
        int r = deflate_msg(ws, d, n, &c);
        if (!r) r = put_frame(ws, 1, 1, op, c.p, c.n);
        mnet_buf_free(&c);
        return r;
    }
    return put_frame(ws, 1, 0, op, d, n);
}
int mnet_ws_ping(mnet_ws_t *ws, const void *d, size_t n) { return n > 125 || ws->close_sent ? -1 : put_frame(ws, 1, 0, MNET_WS_PING, d, n); }
int mnet_ws_close(mnet_ws_t *ws, int code, const char *reason) {
    if (ws->close_sent) return 0;
    uint8_t b[125];
    size_t n = 0;
    if (code) {
        b[0] = (uint8_t)(code >> 8); b[1] = (uint8_t)code; n = 2;
        size_t rl = reason ? strlen(reason) : 0;
        if (rl > 123) rl = 123;
        while (rl && !mnet_utf8_valid((const uint8_t *)reason, rl)) rl--;              /* não corta no meio de um caractere */
        if (rl) memcpy(b + 2, reason, rl);
        n += rl;
    }
    ws->close_sent = 1;
    return put_frame(ws, 1, 0, MNET_WS_CLOSE, b, n);
}
static long proto_error(mnet_ws_t *ws, int code, const char *why) {
    ws->err = why;
    mnet_ws_close(ws, code, why);
    ws->close_code = code;
    return -1;
}
static int valid_close_code(int c) {
    if (c >= 3000 && c <= 4999) return 1;
    return (c >= 1000 && c <= 1003) || (c >= 1007 && c <= 1014);
}
static int finish_msg(mnet_ws_t *ws) {
    mnet_buf_t plain = {0};
    const uint8_t *p = ws->msg.p ? ws->msg.p : (const uint8_t *)"";
    size_t n = ws->msg.n;
    if (ws->msg_rsv1) {
        if (!ws->zin) {
            z_stream *z = calloc(1, sizeof *z);
            if (!z) return -1;
            z->zalloc = za; z->zfree = zf;
            if (inflateInit2(z, -15) != Z_OK) { free(z); return -1; }
            ws->zin = z;
        }
        z_stream *z = ws->zin;
        static const uint8_t tail[4] = {0, 0, 0xff, 0xff};
        uint8_t buf[16384];
        size_t limit = ws->max_msg ? ws->max_msg : (64u << 20);
        for (int pass = 0; pass < 2; pass++) {
            z->next_in = (Bytef *)(pass ? tail : p);
            z->avail_in = (uInt)(pass ? 4 : n);
            do {
                z->next_out = buf; z->avail_out = sizeof buf;
                int r = inflate(z, Z_SYNC_FLUSH);
                if (r != Z_OK && r != Z_BUF_ERROR && r != Z_STREAM_END) { mnet_buf_free(&plain); return -2; }
                if (mnet_buf_put(&plain, buf, sizeof buf - z->avail_out)) { mnet_buf_free(&plain); return -1; }
                if (plain.n > limit) { mnet_buf_free(&plain); return -3; }
                if (r == Z_BUF_ERROR) break;
            } while (z->avail_in || !z->avail_out);
        }
        if (ws->peer_no_ctx) inflateReset(z);
        p = plain.p ? plain.p : (const uint8_t *)""; n = plain.n;
    }
    int r = 0;
    if (ws->msg_op == MNET_WS_TEXT && !mnet_utf8_valid(p, n)) r = -4;
    else if (ws->on_message && ws->on_message(ws->ctx, ws->msg_op, p, n)) r = -5;
    mnet_buf_free(&plain);
    ws->msg.n = 0; ws->in_msg = 0; ws->msg_rsv1 = 0;
    return r;
}
long mnet_ws_feed(mnet_ws_t *ws, const uint8_t *d, size_t n) {
    if (ws->close_recv) return (long)n;                            /* depois do close nada mais vale */
    if (mnet_buf_put(&ws->in, d, n)) return proto_error(ws, 1011, "sem memória");
    size_t off = 0;
    size_t limit = ws->max_msg ? ws->max_msg : (64u << 20);
    for (;;) {
        size_t av = ws->in.n - off;
        const uint8_t *h = ws->in.p + off;
        if (av < 2) break;
        int fin = h[0] & 0x80, rsv1 = h[0] & 0x40, rsv23 = h[0] & 0x30, op = h[0] & 0x0F;
        int masked = h[1] & 0x80;
        uint64_t len = h[1] & 0x7F;
        size_t hl = 2;
        if (len == 126) { if (av < 4) break; len = (uint64_t)h[2] << 8 | h[3]; hl = 4; }
        else if (len == 127) {
            if (av < 10) break;
            len = 0; for (int i = 0; i < 8; i++) len = len << 8 | h[2 + i];
            if (len >> 63) { mnet_buf_consume(&ws->in, off); return proto_error(ws, 1002, "tamanho inválido"); }
            hl = 10;
        }
        if (masked) hl += 4;
        if (rsv23 || (rsv1 && (!ws->deflate || op >= 8 || op == MNET_WS_CONT))) { mnet_buf_consume(&ws->in, off); return proto_error(ws, 1002, "bits RSV inválidos"); }
        if (ws->is_server && !masked) { mnet_buf_consume(&ws->in, off); return proto_error(ws, 1002, "quadro do cliente sem máscara"); }
        if (!ws->is_server && masked) { mnet_buf_consume(&ws->in, off); return proto_error(ws, 1002, "quadro do servidor com máscara"); }
        if (op >= 8) {
            if (op > 10) { mnet_buf_consume(&ws->in, off); return proto_error(ws, 1002, "opcode reservado"); }
            if (!fin || len > 125) { mnet_buf_consume(&ws->in, off); return proto_error(ws, 1002, "quadro de controle inválido"); }
        } else if (op > 2) { mnet_buf_consume(&ws->in, off); return proto_error(ws, 1002, "opcode reservado"); }
        else if (op == MNET_WS_CONT && !ws->in_msg) { mnet_buf_consume(&ws->in, off); return proto_error(ws, 1002, "continuação sem mensagem"); }
        else if (op != MNET_WS_CONT && ws->in_msg) { mnet_buf_consume(&ws->in, off); return proto_error(ws, 1002, "mensagem nova no meio de outra"); }
        if (len > limit || ws->msg.n + len > limit) { mnet_buf_consume(&ws->in, off); return proto_error(ws, 1009, "mensagem grande demais"); }
        if (av < hl + len) break;
        uint8_t *pl = ws->in.p + off + hl;
        if (masked) { const uint8_t *k = pl - 4; for (uint64_t i = 0; i < len; i++) pl[i] ^= k[i & 3]; }
        off += hl + (size_t)len;
        if (op == MNET_WS_PING) {
            put_frame(ws, 1, 0, MNET_WS_PONG, pl, (size_t)len);
            if (ws->on_ping) ws->on_ping(ws->ctx, pl, (size_t)len);
            continue;
        }
        if (op == MNET_WS_PONG) { if (ws->on_pong) ws->on_pong(ws->ctx, pl, (size_t)len); continue; }
        if (op == MNET_WS_CLOSE) {
            int code = 1005;
            if (len == 1) { mnet_buf_consume(&ws->in, off); return proto_error(ws, 1002, "close com 1 byte"); }
            if (len >= 2) {
                code = pl[0] << 8 | pl[1];
                if (!valid_close_code(code)) { mnet_buf_consume(&ws->in, off); return proto_error(ws, 1002, "código de close inválido"); }
                if (!mnet_utf8_valid(pl + 2, (size_t)len - 2)) { mnet_buf_consume(&ws->in, off); return proto_error(ws, 1007, "motivo do close não é UTF-8"); }
                size_t rl = (size_t)len - 2;
                memcpy(ws->close_reason, pl + 2, rl); ws->close_reason[rl] = 0;
            }
            ws->close_recv = 1;
            ws->close_code = code;
            if (!ws->close_sent) mnet_ws_close(ws, code == 1005 ? 1000 : code, 0);   /* responde o close */
            if (ws->on_close) ws->on_close(ws->ctx, code, ws->close_reason);
            break;
        }
        /* dados */
        if (op != MNET_WS_CONT) { ws->msg_op = op; ws->msg_rsv1 = rsv1 != 0; ws->in_msg = 1; ws->msg.n = 0; }
        if (len && mnet_buf_put(&ws->msg, pl, (size_t)len)) { mnet_buf_consume(&ws->in, off); return proto_error(ws, 1011, "sem memória"); }
        if (fin) {
            int r = finish_msg(ws);
            if (r) {
                mnet_buf_consume(&ws->in, off);
                return proto_error(ws, r == -4 ? 1007 : r == -3 ? 1009 : r == -2 ? 1007 : 1011,
                                   r == -4 ? "texto não é UTF-8" : r == -3 ? "mensagem grande demais" : r == -2 ? "dados comprimidos inválidos" : "recusado");
            }
        }
    }
    mnet_buf_consume(&ws->in, off);
    return (long)n;
}
