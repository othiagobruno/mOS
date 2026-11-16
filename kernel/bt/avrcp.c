/*
 * mOS — Bluetooth clássico: servidor SDP e AVRCP (alvo e controle) para fones A2DP.
 *
 * SDP (PSM 0x0001): três registros — fonte A2DP (0x110A), alvo AVRCP (0x110C) e controle AVRCP
 * (0x110E/0x110F) — para o fone achar o AVRCP do mOS e abrir o canal AVCTP. Pedidos 0x02 (busca),
 * 0x04 (atributos) e 0x06 (busca + atributos), com filtro de UUID e de faixas de atributos.
 *
 * AVRCP 1.6 sobre AVCTP (PSM 0x0017), papel de alvo: os botões do fone chegam como PASS THROUGH
 * (play, pausa, parar, próxima, anterior, volume, mudo) e viram teclas de mídia do sistema
 * (input_media_usage, página Consumer). UNIT INFO, SUBUNIT INFO, GetCapabilities (empresa e eventos),
 * GetPlayStatus e RegisterNotification (estado de reprodução) respondidos; o resto, NOT IMPLEMENTED.
 */
#ifdef MOS_HOST_TEST
#include "host_shim.h"
void input_media_usage(u32 usage, bool down);
#else
#include "kernel.h"
#endif

static void be16(u8 *p, u16 v) { p[0] = (u8)(v >> 8); p[1] = (u8)v; }
static u16 rd16(const u8 *p) { return (u16)(p[0] << 8 | p[1]); }

/* ============================================================ SDP */
/* registros já codificados: lista de (ID do atributo, valor) — cada valor é um elemento de dados */
#define U16(v) 0x09, (u8)((v) >> 8), (u8)(v)
#define U32(v) 0x0A, (u8)((v) >> 24), (u8)((v) >> 16), (u8)((v) >> 8), (u8)(v)
#define UUID(v) 0x19, (u8)((v) >> 8), (u8)(v)
#define DES(n) 0x35, (n)
#define ATTR(id) (u8)((id) >> 8), (u8)(id)

static const u8 rec_a2dp[] = {
    ATTR(0x0000), U32(0x00010001),
    ATTR(0x0001), DES(3), UUID(0x110A),
    ATTR(0x0004), DES(16), DES(6), UUID(0x0100), U16(0x0019), DES(6), UUID(0x0019), U16(0x0103),
    ATTR(0x0005), DES(3), UUID(0x1002),
    ATTR(0x0009), DES(8), DES(6), UUID(0x110D), U16(0x0103),
    ATTR(0x0311), U16(0x0001),
};
static const u8 rec_avrcp_tg[] = {
    ATTR(0x0000), U32(0x00010002),
    ATTR(0x0001), DES(3), UUID(0x110C),
    ATTR(0x0004), DES(16), DES(6), UUID(0x0100), U16(0x0017), DES(6), UUID(0x0017), U16(0x0104),
    ATTR(0x0005), DES(3), UUID(0x1002),
    ATTR(0x0009), DES(8), DES(6), UUID(0x110E), U16(0x0106),
    ATTR(0x0311), U16(0x0001),
};
static const u8 rec_avrcp_ct[] = {
    ATTR(0x0000), U32(0x00010003),
    ATTR(0x0001), DES(6), UUID(0x110E), UUID(0x110F),
    ATTR(0x0004), DES(16), DES(6), UUID(0x0100), U16(0x0017), DES(6), UUID(0x0017), U16(0x0104),
    ATTR(0x0005), DES(3), UUID(0x1002),
    ATTR(0x0009), DES(8), DES(6), UUID(0x110E), U16(0x0106),
    ATTR(0x0311), U16(0x0001),
};
static const struct { const u8 *d; u16 n; u32 handle; } recs[] = {
    {rec_a2dp, sizeof rec_a2dp, 0x00010001}, {rec_avrcp_tg, sizeof rec_avrcp_tg, 0x00010002}, {rec_avrcp_ct, sizeof rec_avrcp_ct, 0x00010003},
};

/* tamanho de um elemento de dados (cabeçalho + conteúdo); -1 se não couber */
static int de_len(const u8 *p, int max) {
    if (max < 1) return -1;
    u8 t = p[0] >> 3, s = p[0] & 7;
    int hl = 1, cl;
    if (t == 0) cl = 0;
    else if (s <= 4 && t != 4 && t != 6 && t != 7 && t != 8) cl = 1 << s;
    else if (s == 5) { if (max < 2) return -1; hl = 2; cl = p[1]; }
    else if (s == 6) { if (max < 3) return -1; hl = 3; cl = rd16(p + 1); }
    else if (s == 7) { if (max < 5) return -1; hl = 5; cl = (int)((u32)p[1] << 24 | (u32)p[2] << 16 | (u32)p[3] << 8 | p[4]); }
    else cl = 1 << s;
    return hl + cl <= max ? hl + cl : -1;
}
static int de_hdr(const u8 *p) { u8 s = p[0] & 7; return s == 5 ? 2 : s == 6 ? 3 : s == 7 ? 5 : 1; }

/* UUIDs de 16 bits do padrão de busca (UUID de 128 bits na base do Bluetooth também vale) */
static int parse_uuids(const u8 *p, int len, u16 *out, int max) {
    int l = de_len(p, len);
    if (l < 0 || (p[0] >> 3) != 6) return -1;
    int n = 0;
    for (int i = de_hdr(p); i < l && n < max;) {
        int el = de_len(p + i, l - i);
        if (el < 0) return -1;
        if (p[i] == 0x19) out[n++] = rd16(p + i + 1);
        else if (p[i] == 0x1A) out[n++] = rd16(p + i + 3);
        else if (p[i] == 0x1C) out[n++] = rd16(p + i + 3);
        i += el;
    }
    return n;
}

/* procura um UUID de 16 bits dentro de um elemento (descendo nas sequências) */
static bool de_has_uuid(const u8 *p, int len, u16 u, int depth) {
    int l = de_len(p, len);
    if (l < 0 || depth > 6) return false;
    if (p[0] == 0x19) return rd16(p + 1) == u;
    if ((p[0] >> 3) == 6 || (p[0] >> 3) == 7)
        for (int i = de_hdr(p); i < l;) {
            int el = de_len(p + i, l - i);
            if (el < 0) return false;
            if (de_has_uuid(p + i, el, u, depth + 1)) return true;
            i += el;
        }
    return false;
}
static bool rec_has_uuid(int r, u16 u) {
    const u8 *d = recs[r].d;
    for (int i = 0; i + 2 < recs[r].n;) {
        int vl = de_len(d + i + 2, recs[r].n - i - 2);
        if (vl < 0) return false;
        if (de_has_uuid(d + i + 2, vl, u, 0)) return true;
        i += 2 + vl;
    }
    return false;
}
static bool rec_match(int r, const u16 *u, int n) {
    for (int k = 0; k < n; k++) if (!rec_has_uuid(r, u[k])) return false;
    return n > 0;
}

/* lista de faixas de atributos: uint16 (um ID) ou uint32 (início << 16 | fim) */
static int parse_ranges(const u8 *p, int len, u32 *lo, u32 *hi, int max) {
    int l = de_len(p, len);
    if (l < 0 || (p[0] >> 3) != 6) return -1;
    int n = 0;
    for (int i = de_hdr(p); i < l && n < max;) {
        if (p[i] == 0x09) { lo[n] = hi[n] = rd16(p + i + 1); n++; i += 3; }
        else if (p[i] == 0x0A) { lo[n] = rd16(p + i + 1); hi[n] = rd16(p + i + 3); n++; i += 5; }
        else return -1;
    }
    return n;
}

/* escreve DES{ id valor ... } de um registro, só com os atributos pedidos */
static int rec_attrs(int r, const u32 *lo, const u32 *hi, int nr, u8 *out, int max) {
    const u8 *d = recs[r].d;
    int body = 0;
    u8 tmp[160];
    for (int i = 0; i < recs[r].n;) {
        u16 id = rd16(d + i);
        int vl = de_len(d + i + 2, recs[r].n - i - 2);
        if (vl < 0) break;
        bool want = false;
        for (int k = 0; k < nr; k++) if (id >= lo[k] && id <= hi[k]) want = true;
        if (want && body + 3 + vl <= (int)sizeof tmp) { tmp[body++] = 0x09; tmp[body++] = (u8)(id >> 8); tmp[body++] = (u8)id; memcpy(tmp + body, d + i + 2, (usize)vl); body += vl; }
        i += 2 + vl;
    }
    if (body + 2 > max) return -1;
    out[0] = 0x35; out[1] = (u8)body;
    memcpy(out + 2, tmp, (usize)body);
    return 2 + body;
}

static int sdp_error(u16 tid, u16 code, u8 *o) { o[0] = 0x01; be16(o + 1, tid); be16(o + 3, 2); be16(o + 5, code); return 7; }

int sdp_handle(const u8 *q, int len, u8 *o, int max) {
    if (len < 5 || max < 64) return 0;
    u8 pdu = q[0];
    u16 tid = rd16(q + 1), plen = rd16(q + 3);
    if (5 + plen > len) return sdp_error(tid, 0x0004, o);
    const u8 *p = q + 5;
    int pl = plen;
    u16 uu[12];
    u32 lo[16], hi[16];
    if (pdu == 0x02) {                                         /* ServiceSearchRequest */
        int nu = parse_uuids(p, pl, uu, 12);
        int sl = de_len(p, pl);
        if (nu < 0 || sl < 0 || sl + 2 > pl) return sdp_error(tid, 0x0003, o);
        u16 maxrec = rd16(p + sl);
        int n = 0;
        u8 *h = o + 9;
        for (int r = 0; r < (int)ARRAY_LEN(recs) && n < maxrec; r++)
            if (rec_match(r, uu, nu)) { u32 v = recs[r].handle; h[4 * n] = (u8)(v >> 24); h[4 * n + 1] = (u8)(v >> 16); h[4 * n + 2] = (u8)(v >> 8); h[4 * n + 3] = (u8)v; n++; }
        o[0] = 0x03; be16(o + 1, tid); be16(o + 3, (u16)(4 + 4 * n + 1));
        be16(o + 5, (u16)n); be16(o + 7, (u16)n);
        o[9 + 4 * n] = 0;                                      /* sem continuação */
        return 10 + 4 * n;
    }
    if (pdu == 0x04) {                                         /* ServiceAttributeRequest */
        if (pl < 7) return sdp_error(tid, 0x0003, o);
        u32 handle = (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3];
        int nr = parse_ranges(p + 6, pl - 6, lo, hi, 16);
        if (nr < 0) return sdp_error(tid, 0x0003, o);
        int r = -1;
        for (int k = 0; k < (int)ARRAY_LEN(recs); k++) if (recs[k].handle == handle) r = k;
        if (r < 0) return sdp_error(tid, 0x0002, o);
        int al = rec_attrs(r, lo, hi, nr, o + 7, max - 8);
        if (al < 0) return sdp_error(tid, 0x0005, o);
        o[0] = 0x05; be16(o + 1, tid); be16(o + 3, (u16)(2 + al + 1)); be16(o + 5, (u16)al);
        o[7 + al] = 0;
        return 8 + al;
    }
    if (pdu == 0x06) {                                         /* ServiceSearchAttributeRequest */
        int nu = parse_uuids(p, pl, uu, 12);
        int sl = de_len(p, pl);
        if (nu < 0 || sl < 0 || sl + 2 >= pl) return sdp_error(tid, 0x0003, o);
        int nr = parse_ranges(p + sl + 2, pl - sl - 2, lo, hi, 16);
        if (nr < 0) return sdp_error(tid, 0x0003, o);
        int body = 0;
        u8 *lists = o + 9;
        for (int r = 0; r < (int)ARRAY_LEN(recs); r++) {
            if (!rec_match(r, uu, nu)) continue;
            int al = rec_attrs(r, lo, hi, nr, lists + body, max - 12 - body);
            if (al < 0) return sdp_error(tid, 0x0005, o);
            body += al;
        }
        o[7] = 0x35; o[8] = (u8)body;                          /* DES de listas de atributos */
        if (body > 255) return sdp_error(tid, 0x0005, o);
        int total = 2 + body;
        o[0] = 0x07; be16(o + 1, tid); be16(o + 3, (u16)(2 + total + 1)); be16(o + 5, (u16)total);
        o[7 + total] = 0;
        return 8 + total;
    }
    return sdp_error(tid, 0x0003, o);
}

/* ============================================================ AVRCP (alvo) */
enum { AVC_CONTROL = 0, AVC_STATUS = 1, AVC_NOTIFY = 3, AVC_NOT_IMPL = 8, AVC_ACCEPTED = 9, AVC_REJECTED = 10,
       AVC_STABLE = 12, AVC_CHANGED = 13, AVC_INTERIM = 15 };
static bool playing;
void avrcp_set_playing(bool on) { playing = on; }

static u32 op_to_usage(u8 op) {
    switch (op) {
    case 0x41: return 0x0C00E9;                                /* volume + */
    case 0x42: return 0x0C00EA;                                /* volume - */
    case 0x43: return 0x0C00E2;                                /* mudo */
    case 0x44: return 0x0C00CD;                                /* play (alterna) */
    case 0x46: return 0x0C00CD;                                /* pausa (alterna) */
    case 0x45: return 0x0C00B7;                                /* parar */
    case 0x4B: return 0x0C00B5;                                /* próxima */
    case 0x4C: return 0x0C00B6;                                /* anterior */
    case 0x48: return 0x0C00B4;                                /* voltar */
    case 0x49: return 0x0C00B3;                                /* avançar */
    default: return 0;
    }
}

/* resposta de um comando dependente de fabricante (Bluetooth SIG, 00-19-58) */
static int vendor_rsp(u8 *o, u8 ctype, u8 pdu, const u8 *params, u16 n) {
    o[3] = ctype; o[4] = 0x48; o[5] = 0x00;
    o[6] = 0x00; o[7] = 0x19; o[8] = 0x58;
    o[9] = pdu; o[10] = 0x00; be16(o + 11, n);
    memcpy(o + 13, params, n);
    return 13 + n;
}

int avrcp_handle(const u8 *f, int len, u8 *o, int max) {
    if (len < 6 || max < 64) return 0;
    if (f[0] & 0x02) return 0;                                 /* é resposta: o mOS não manda comandos ainda */
    if ((f[0] & 0x0C) != 0) return 0;                          /* só pacote único */
    o[0] = (u8)((f[0] & 0xF0) | 0x02);                         /* mesma transação, resposta */
    o[1] = f[1]; o[2] = f[2];
    if (rd16(f + 1) != 0x110E) { o[0] |= 0x01; return 3; }     /* PID inválido: IPID */
    u8 ctype = f[3] & 0x0F, subunit = f[4], opcode = f[5];
    const u8 *a = f + 6;
    int an = len - 6;
    switch (opcode) {
    case 0x30:                                                 /* UNIT INFO */
        o[3] = AVC_STABLE; o[4] = 0xFF; o[5] = 0x30;
        o[6] = 0x07; o[7] = 0x48; o[8] = 0xFF; o[9] = 0xFF; o[10] = 0xFF;
        return 11;
    case 0x31:                                                 /* SUBUNIT INFO */
        o[3] = AVC_STABLE; o[4] = 0xFF; o[5] = 0x31;
        o[6] = 0x07; o[7] = 0x48; o[8] = 0xFF; o[9] = 0xFF; o[10] = 0xFF;
        return 11;
    case 0x7C: {                                               /* PASS THROUGH */
        if (an < 2 || subunit != 0x48) { o[3] = AVC_REJECTED; o[4] = subunit; o[5] = opcode; return 6; }
        u8 op = a[0] & 0x7F;
        bool released = a[0] & 0x80;
        u32 u = op_to_usage(op);
        o[4] = subunit; o[5] = opcode; o[6] = a[0]; o[7] = a[1];
        if (!u) { o[3] = AVC_NOT_IMPL; return 8; }
        input_media_usage(u, !released);
        if (op == 0x44 && released) playing = true;
        if (op == 0x46 && released) playing = false;
        if (op == 0x45 && released) playing = false;
        o[3] = AVC_ACCEPTED;
        return 8;
    }
    case 0x00: {                                               /* dependente de fabricante */
        if (an < 7 || a[0] != 0x00 || a[1] != 0x19 || a[2] != 0x58) { o[3] = AVC_NOT_IMPL; o[4] = subunit; o[5] = opcode; return 6; }
        u8 pdu = a[3];
        const u8 *pp = a + 7;
        u16 pn = rd16(a + 5);
        if (7 + pn > an) pn = (u16)(an - 7);
        if (pdu == 0x10 && pn >= 1) {                          /* GetCapabilities */
            if (pp[0] == 0x02) { static const u8 r[] = {0x02, 1, 0x00, 0x19, 0x58}; return vendor_rsp(o, AVC_STABLE, pdu, r, sizeof r); }
            if (pp[0] == 0x03) { static const u8 r[] = {0x03, 1, 0x01}; return vendor_rsp(o, AVC_STABLE, pdu, r, sizeof r); }   /* só estado de reprodução */
            static const u8 bad[] = {0x01};                    /* parâmetro inválido */
            return vendor_rsp(o, AVC_REJECTED, pdu, bad, 1);
        }
        if (pdu == 0x30) {                                     /* GetPlayStatus: duração e posição desconhecidas */
            u8 r[9] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, (u8)(playing ? 1 : 2)};
            return vendor_rsp(o, AVC_STABLE, pdu, r, sizeof r);
        }
        if (pdu == 0x31 && pn >= 1 && ctype == AVC_NOTIFY) {   /* RegisterNotification */
            if (pp[0] == 0x01) { u8 r[2] = {0x01, (u8)(playing ? 1 : 2)}; return vendor_rsp(o, AVC_INTERIM, pdu, r, 2); }
            static const u8 bad[] = {0x01};
            return vendor_rsp(o, AVC_REJECTED, pdu, bad, 1);
        }
        static const u8 bad[] = {0x00};                        /* comando inválido */
        return vendor_rsp(o, AVC_REJECTED, pdu, bad, 1);
    }
    default:
        o[3] = AVC_NOT_IMPL; o[4] = subunit; o[5] = opcode;
        memcpy(o + 6, a, (usize)(an > 8 ? 8 : an));
        return 6 + (an > 8 ? 8 : an);
    }
    (void)ctype;
}
