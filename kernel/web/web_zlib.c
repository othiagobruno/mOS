/* mOS — CompressionStream e DecompressionStream do motor web: deflate (RFC 1951) com envelope gzip (RFC 1952),
 * zlib (RFC 1950) ou cru, em streaming. A descompressão decodifica bloco a bloco: um bloco que ainda não chegou
 * inteiro é refeito quando chegam mais bytes; a saída sai a cada bloco completo e só os últimos 32 KiB ficam
 * guardados como janela. A compressão manda cada pedaço como um bloco de Huffman fixo com LZ77 (não final) e
 * fecha com o bloco final vazio e o rodapé. O estado vive num ArrayBuffer externo (o coletor libera). */
#include "js_int.h"
#include "web.h"

#define NATIVE(name) static jsv name(jsv thisv, int argc, jsv *argv, jsobj *self, int flags)
#define UNUSED_ARGS (void)thisv; (void)argc; (void)argv; (void)self; (void)flags

enum { ZF_GZIP, ZF_ZLIB, ZF_RAW };
enum { ZS_HEAD, ZS_BLOCKS, ZS_TRAIL, ZS_DONE };
#define ZWIN 32768u

typedef struct {
    int fmt;
    bool comp;
    int stage;
    u32 crc, adler;
    u64 total;
    /* descompressão: entrada desde o começo do bloco atual (bit0 = bit dentro do 1º byte) e a janela */
    u8 *in;
    usize inlen, incap;
    int bit0;
    u8 *win;
    usize wlen;
    /* compressão: bits ainda não completaram um byte */
    u32 bb;
    int bn;
} zst_t;

/* ------------------------------------------------------------ somas */
static u32 crc_tab[256];
static u32 crc32_up(u32 c, const u8 *d, usize n) {
    if (!crc_tab[1])
        for (u32 i = 0; i < 256; i++) { u32 v = i; for (int k = 0; k < 8; k++) v = v & 1 ? 0xEDB88320u ^ (v >> 1) : v >> 1; crc_tab[i] = v; }
    c = ~c;
    for (usize i = 0; i < n; i++) c = crc_tab[(c ^ d[i]) & 255] ^ (c >> 8);
    return ~c;
}
static u32 adler_up(u32 a, const u8 *d, usize n) {
    u32 s1 = a & 0xFFFF, s2 = a >> 16;
    while (n) {
        usize k = n < 5552 ? n : 5552;
        n -= k;
        while (k--) { s1 += *d++; s2 += s1; }
        s1 %= 65521;
        s2 %= 65521;
    }
    return s2 << 16 | s1;
}

/* ------------------------------------------------------------ buffer de saída */
typedef struct { u8 *p; usize n, cap; bool err; } zbuf_t;
static bool zb_put(zbuf_t *b, const u8 *d, usize n) {
    if (b->err) return false;
    if (b->n + n > b->cap) {
        usize nc = b->cap ? b->cap : 4096;
        while (nc < b->n + n) nc *= 2;
        u8 *np = krealloc(b->p, nc);
        if (!np) { b->err = true; return false; }
        b->p = np;
        b->cap = nc;
    }
    if (n) memcpy(b->p + b->n, d, n);
    b->n += n;
    return true;
}
static bool zb_byte(zbuf_t *b, u8 v) { return zb_put(b, &v, 1); }

/* ------------------------------------------------------------ inflate de um bloco */
typedef struct { u16 count[16], sym[320]; } zhuff_t;
typedef struct {
    const u8 *in;
    usize n, pos;
    u32 bb;
    int bc;
    bool eof, err;
    zbuf_t *o;          /* janela + saída do bloco (as distâncias apontam para dentro dela) */
} zin_t;

static u32 zbits(zin_t *s, int need) {
    while (s->bc < need) {
        if (s->pos >= s->n) { s->eof = true; return 0; }
        s->bb |= (u32)s->in[s->pos++] << s->bc;
        s->bc += 8;
    }
    u32 v = s->bb & ((1u << need) - 1);
    s->bb >>= need;
    s->bc -= need;
    return v;
}

static int zbuild(zhuff_t *h, const u8 *len, int n) {
    for (int i = 0; i < 16; i++) h->count[i] = 0;
    for (int i = 0; i < n; i++) h->count[len[i]]++;
    if (h->count[0] == n) return 0;
    int left = 1;
    for (int i = 1; i < 16; i++) { left <<= 1; left -= h->count[i]; if (left < 0) return left; }
    u16 offs[16];
    offs[1] = 0;
    for (int i = 1; i < 15; i++) offs[i + 1] = (u16)(offs[i] + h->count[i]);
    for (int i = 0; i < n; i++) if (len[i]) h->sym[offs[len[i]]++] = (u16)i;
    return left;
}

static int zdecode(zin_t *s, const zhuff_t *h) {
    int code = 0, first = 0, index = 0;
    for (int len = 1; len < 16; len++) {
        code |= (int)zbits(s, 1);
        if (s->eof) return -1;
        int count = h->count[len];
        if (code - count < first) return h->sym[index + (code - first)];
        index += count;
        first += count;
        first <<= 1;
        code <<= 1;
    }
    s->err = true;
    return -1;
}

static const u16 zlbase[29] = { 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258 };
static const u8 zlext[29] = { 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0 };
static const u16 zdbase[30] = { 1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577 };
static const u8 zdext[30] = { 0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13 };

static bool zcodes(zin_t *s, const zhuff_t *lc, const zhuff_t *dc) {
    for (;;) {
        int sym = zdecode(s, lc);
        if (sym < 0) return false;
        if (sym < 256) { if (!zb_byte(s->o, (u8)sym)) { s->err = true; return false; } continue; }
        if (sym == 256) return true;
        sym -= 257;
        if (sym >= 29) { s->err = true; return false; }
        int len = zlbase[sym] + (int)zbits(s, zlext[sym]);
        int ds = zdecode(s, dc);
        if (ds < 0) return false;
        if (ds >= 30) { s->err = true; return false; }
        usize dist = zdbase[ds] + zbits(s, zdext[ds]);
        if (s->eof) return false;
        if (dist > s->o->n) { s->err = true; return false; }
        for (int i = 0; i < len; i++) if (!zb_byte(s->o, s->o->p[s->o->n - dist])) { s->err = true; return false; }
    }
}

/* um bloco; *last = BFINAL. false com eof = falta entrada; false com err = dados inválidos */
static bool zblock(zin_t *s, bool *last) {
    static zhuff_t flc, fdc;
    static bool fixed_ready;
    *last = zbits(s, 1);
    int type = (int)zbits(s, 2);
    if (s->eof) return false;
    if (type == 0) {
        s->bb = 0;
        s->bc = 0;
        if (s->pos + 4 > s->n) { s->eof = true; return false; }
        u32 len = s->in[s->pos] | (u32)s->in[s->pos + 1] << 8, nlen = s->in[s->pos + 2] | (u32)s->in[s->pos + 3] << 8;
        if ((len ^ 0xFFFF) != nlen) { s->err = true; return false; }
        if (s->pos + 4 + len > s->n) { s->eof = true; return false; }
        s->pos += 4;
        if (!zb_put(s->o, s->in + s->pos, len)) { s->err = true; return false; }
        s->pos += len;
        return true;
    }
    if (type == 1) {
        if (!fixed_ready) {
            u8 l[288];
            int i = 0;
            for (; i < 144; i++) l[i] = 8;
            for (; i < 256; i++) l[i] = 9;
            for (; i < 280; i++) l[i] = 7;
            for (; i < 288; i++) l[i] = 8;
            zbuild(&flc, l, 288);
            for (i = 0; i < 30; i++) l[i] = 5;
            zbuild(&fdc, l, 30);
            fixed_ready = true;
        }
        return zcodes(s, &flc, &fdc);
    }
    if (type == 2) {
        static const u8 ord[19] = { 16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15 };
        int nlen = (int)zbits(s, 5) + 257, ndist = (int)zbits(s, 5) + 1, ncode = (int)zbits(s, 4) + 4;
        if (s->eof) return false;
        if (nlen > 286 || ndist > 30) { s->err = true; return false; }
        u8 lens[320];
        memset(lens, 0, sizeof lens);
        for (int i = 0; i < ncode; i++) lens[ord[i]] = (u8)zbits(s, 3);
        if (s->eof) return false;
        zhuff_t lc, dc;
        if (zbuild(&lc, lens, 19) != 0) { s->err = true; return false; }
        int idx = 0;
        while (idx < nlen + ndist) {
            int sym = zdecode(s, &lc);
            if (sym < 0) return false;
            if (sym < 16) { lens[idx++] = (u8)sym; continue; }
            int len = 0, rep;
            if (sym == 16) { if (!idx) { s->err = true; return false; } len = lens[idx - 1]; rep = 3 + (int)zbits(s, 2); }
            else if (sym == 17) rep = 3 + (int)zbits(s, 3);
            else rep = 11 + (int)zbits(s, 7);
            if (s->eof) return false;
            if (idx + rep > nlen + ndist) { s->err = true; return false; }
            while (rep--) lens[idx++] = (u8)len;
        }
        if (!lens[256]) { s->err = true; return false; }
        int e1 = zbuild(&lc, lens, nlen);
        if (e1 < 0 || (e1 > 0 && nlen - lc.count[0] != 1)) { s->err = true; return false; }
        int e2 = zbuild(&dc, lens + nlen, ndist);
        if (e2 < 0 || (e2 > 0 && ndist - dc.count[0] != 1)) { s->err = true; return false; }
        return zcodes(s, &lc, &dc);
    }
    s->err = true;
    return false;
}

/* ------------------------------------------------------------ descompressão em streaming */
static void z_consume(zst_t *z, usize k) {
    memmove(z->in, z->in + k, z->inlen - k);
    z->inlen -= k;
}

/* cabeçalho gzip/zlib; 1 pronto, 0 falta entrada, -1 inválido */
static int z_header(zst_t *z) {
    if (z->fmt == ZF_RAW) return 1;
    const u8 *d = z->in;
    usize n = z->inlen;
    if (z->fmt == ZF_ZLIB) {
        if (n < 2) return 0;
        if ((d[0] & 15) != 8 || (d[0] >> 4) > 7 || ((d[0] << 8) | d[1]) % 31 || (d[1] & 0x20)) return -1;   /* sem dicionário */
        z_consume(z, 2);
        return 1;
    }
    if (n < 10) return 0;
    if (d[0] != 0x1F || d[1] != 0x8B || d[2] != 8 || (d[3] & 0xE0)) return -1;
    u8 fl = d[3];
    usize p = 10;
    if (fl & 4) { if (n < p + 2) return 0; p += 2 + (d[p] | (usize)d[p + 1] << 8); if (n < p) return 0; }
    if (fl & 8) { while (p < n && d[p]) p++; if (p >= n) return 0; p++; }
    if (fl & 16) { while (p < n && d[p]) p++; if (p >= n) return 0; p++; }
    if (fl & 2) { if (n < p + 2) return 0; p += 2; }
    z_consume(z, p);
    return 1;
}

static jsv z_fail(const char *m) { return js_throw_err(E_TYPE, "%s", m); }

/* processa o que der; saída em out */
static jsv z_inflate_run(zst_t *z, zbuf_t *out) {
    for (;;) {
        if (z->stage == ZS_HEAD) {
            int h = z_header(z);
            if (h < 0) return z_fail("DecompressionStream: cabeçalho inválido");
            if (h == 0) return JS_UNDEF;
            z->stage = ZS_BLOCKS;
            z->bit0 = 0;
        }
        if (z->stage == ZS_BLOCKS) {
            zbuf_t ob = { 0 };
            if (!zb_put(&ob, z->win, z->wlen)) return js_throw_err(E_RANGE, "memória insuficiente");
            zin_t s = { 0 };
            s.in = z->in;
            s.n = z->inlen;
            s.o = &ob;
            if (z->bit0) { zbits(&s, z->bit0); if (s.eof) { kfree(ob.p); return JS_UNDEF; } }
            bool last = false;
            bool ok = zblock(&s, &last);
            if (!ok) {
                kfree(ob.p);
                if (s.err || ob.err) return z_fail("DecompressionStream: dados comprimidos inválidos");
                return JS_UNDEF;   /* o bloco ainda não chegou inteiro */
            }
            /* bloco completo: saída, somas, janela e posição */
            const u8 *nd = ob.p + z->wlen;
            usize nn = ob.n - z->wlen;
            if (!zb_put(out, nd, nn)) { kfree(ob.p); return js_throw_err(E_RANGE, "memória insuficiente"); }
            if (z->fmt == ZF_GZIP) z->crc = crc32_up(z->crc, nd, nn);
            else if (z->fmt == ZF_ZLIB) z->adler = adler_up(z->adler, nd, nn);
            z->total += nn;
            usize keep = ob.n < ZWIN ? ob.n : ZWIN;
            if (!z->win) z->win = kmalloc(ZWIN);
            if (!z->win) { kfree(ob.p); return js_throw_err(E_RANGE, "memória insuficiente"); }
            memcpy(z->win, ob.p + ob.n - keep, keep);
            z->wlen = keep;
            kfree(ob.p);
            /* posição em bits: s.pos bytes lidos menos os bits que sobraram no acumulador */
            usize bitpos = s.pos * 8 - (usize)s.bc;
            z_consume(z, bitpos / 8);
            z->bit0 = (int)(bitpos % 8);
            if (last) {
                if (z->bit0) { z_consume(z, 1); z->bit0 = 0; }   /* o rodapé começa no próximo byte */
                z->stage = ZS_TRAIL;
            }
            continue;
        }
        if (z->stage == ZS_TRAIL) {
            usize need = z->fmt == ZF_GZIP ? 8 : z->fmt == ZF_ZLIB ? 4 : 0;
            if (z->inlen < need) return JS_UNDEF;
            const u8 *d = z->in;
            if (z->fmt == ZF_GZIP) {
                u32 c = d[0] | (u32)d[1] << 8 | (u32)d[2] << 16 | (u32)d[3] << 24, sz = d[4] | (u32)d[5] << 8 | (u32)d[6] << 16 | (u32)d[7] << 24;
                if (c != z->crc || sz != (u32)z->total) return z_fail("DecompressionStream: CRC ou tamanho do gzip não confere");
            } else if (z->fmt == ZF_ZLIB) {
                u32 a = (u32)d[0] << 24 | (u32)d[1] << 16 | (u32)d[2] << 8 | d[3];
                if (a != z->adler) return z_fail("DecompressionStream: Adler-32 não confere");
            }
            z_consume(z, need);
            z->stage = ZS_DONE;
        }
        if (z->stage == ZS_DONE) {
            if (z->inlen) return z_fail("DecompressionStream: dados depois do fim do stream comprimido");
            return JS_UNDEF;
        }
    }
}

/* ------------------------------------------------------------ compressão */
static void zw_bits(zst_t *z, zbuf_t *o, u32 v, int n) {
    z->bb |= v << z->bn;
    z->bn += n;
    while (z->bn >= 8) { zb_byte(o, (u8)z->bb); z->bb >>= 8; z->bn -= 8; }
}
static void zw_huff(zst_t *z, zbuf_t *o, u32 code, int len) {   /* códigos de Huffman vão do bit mais alto */
    u32 r = 0;
    for (int i = 0; i < len; i++) r |= ((code >> i) & 1) << (len - 1 - i);
    zw_bits(z, o, r, len);
}
static void zw_lit(zst_t *z, zbuf_t *o, int sym) {
    if (sym < 144) zw_huff(z, o, 0x30 + (u32)sym, 8);
    else if (sym < 256) zw_huff(z, o, 0x190 + (u32)sym - 144, 9);
    else if (sym < 280) zw_huff(z, o, (u32)sym - 256, 7);
    else zw_huff(z, o, 0xC0 + (u32)sym - 280, 8);
}

/* um bloco não final de Huffman fixo com LZ77 dentro do pedaço */
static bool z_deflate_chunk(zst_t *z, const u8 *in, usize n, zbuf_t *o) {
    if (!n) return true;
    zw_bits(z, o, 0, 1);   /* BFINAL = 0 */
    zw_bits(z, o, 1, 2);   /* BTYPE = 01 */
    enum { HB = 15 };
    i32 *head = kmalloc(sizeof(i32) << HB), *prev = kmalloc(sizeof(i32) * ZWIN);
    if (!head || !prev) { kfree(head); kfree(prev); return false; }
    for (int i = 0; i < (1 << HB); i++) head[i] = -1;
#define ZHASH(p) ((((u32)(p)[0] << 10) ^ ((u32)(p)[1] << 5) ^ (p)[2]) & ((1u << HB) - 1))
    usize i = 0;
    while (i < n) {
        int best = 0, bdist = 0;
        if (i + 3 <= n) {
            u32 h = ZHASH(in + i);
            i32 cand = head[h];
            int chain = 64;
            while (cand >= 0 && (usize)cand + ZWIN > i && chain--) {
                int maxl = (int)MIN((usize)258, n - i), l = 0;
                const u8 *a = in + cand, *b = in + i;
                while (l < maxl && a[l] == b[l]) l++;
                if (l > best) { best = l; bdist = (int)(i - (usize)cand); if (l == maxl) break; }
                cand = prev[cand & (ZWIN - 1)];
            }
            prev[i & (ZWIN - 1)] = head[h];
            head[h] = (i32)i;
        }
        if (best >= 3) {
            int li = 28;
            while (li > 0 && zlbase[li] > best) li--;
            zw_lit(z, o, 257 + li);
            if (zlext[li]) zw_bits(z, o, (u32)(best - zlbase[li]), zlext[li]);
            int di = 29;
            while (di > 0 && zdbase[di] > bdist) di--;
            zw_huff(z, o, (u32)di, 5);
            if (zdext[di]) zw_bits(z, o, (u32)(bdist - zdbase[di]), zdext[di]);
            for (int k = 1; k < best; k++) {
                usize j = i + (usize)k;
                if (j + 3 <= n) { u32 h = ZHASH(in + j); prev[j & (ZWIN - 1)] = head[h]; head[h] = (i32)j; }
            }
            i += (usize)best;
        } else zw_lit(z, o, in[i++]);
    }
#undef ZHASH
    zw_lit(z, o, 256);
    kfree(head);
    kfree(prev);
    return !o->err;
}

static void z_head_out(zst_t *z, zbuf_t *o) {
    if (z->stage != ZS_HEAD) return;
    z->stage = ZS_BLOCKS;
    if (z->fmt == ZF_GZIP) { static const u8 h[10] = { 0x1F, 0x8B, 8, 0, 0, 0, 0, 0, 0, 0xFF }; zb_put(o, h, 10); }
    else if (z->fmt == ZF_ZLIB) { zb_byte(o, 0x78); zb_byte(o, 0x9C); }
}

/* ------------------------------------------------------------ natives: init(formato, comprimir), push(h, bytes), end(h) */
static void z_free(void *bytes, void *ctx) {
    (void)ctx;
    zst_t *z = bytes;
    kfree(z->in);
    kfree(z->win);
    kfree(z);
}

/* BufferSource: bytes sem cópia (ArrayBuffer, typed array ou DataView) */
static bool buf_src(jsv v, const u8 **d, u32 *n) {
    *d = NULL;
    *n = 0;
    if (jv_iscls(v, CL_ARRAYBUFFER)) { if (!jv_o(v)->u.buf.detached) { *d = jv_o(v)->u.buf.data; *n = jv_o(v)->u.buf.len; } return true; }
    if (jv_iscls(v, CL_TYPED) || jv_iscls(v, CL_DATAVIEW)) {
        jsobj *o = jv_o(v);
        if (o->u.ta.buffer && !o->u.ta.buffer->u.buf.detached) {
            *d = o->u.ta.buffer->u.buf.data + o->u.ta.off;
            *n = o->cls == CL_DATAVIEW ? o->u.ta.len : js_typed_length(o) * (u32)js_typed_elsize(o->u.ta.kind);
        }
        return true;
    }
    return false;
}

static zst_t *z_of(jsv h) {
    if (!jv_isobj(h) || jv_o(h)->cls != CL_ARRAYBUFFER || jv_o(h)->u.buf.dealloc != z_free) return NULL;
    return (zst_t *)jv_o(h)->u.buf.data;
}

static jsv z_out(zbuf_t *o) {
    jsobj *b = js_abuf_new((u32)o->n);
    if (!b) { kfree(o->p); return JS_EXC; }
    if (o->n) memcpy(b->u.buf.data, o->p, o->n);
    kfree(o->p);
    jsobj *t = js_typed_new(TA_UINT8, b, 0, (u32)o->n);
    return t ? jv_obj(t) : JS_EXC;
}

/* magic 0 init, 1 push, 2 end */
NATIVE(z_native) {
    UNUSED_ARGS;
    int m = self->u.nat.magic;
    if (m == 0) {
        jsstr *f = js_tostr(ARG(0));
        if (!f) return JS_EXC;
        int fmt = !strcmp(f->d, "gzip") ? ZF_GZIP : !strcmp(f->d, "deflate") ? ZF_ZLIB : !strcmp(f->d, "deflate-raw") ? ZF_RAW : -1;
        if (fmt < 0) return js_throw_err(E_TYPE, "Formato de compressão não suportado: %s", f->d);
        zst_t *z = kcalloc(1, sizeof(zst_t));
        if (!z) return js_throw_err(E_RANGE, "memória insuficiente");
        z->fmt = fmt;
        z->comp = js_truthy(ARG(1));
        z->adler = 1;
        jsobj *b = js_abuf_external(z, sizeof(zst_t), z_free, NULL);
        if (!b) { kfree(z); return JS_EXC; }
        return jv_obj(b);
    }
    zst_t *z = z_of(ARG(0));
    if (!z) return js_throw_err(E_TYPE, "Invocação ilegal");
    zbuf_t out = { 0 };
    if (m == 1) {
        const u8 *d;
        u32 n;
        if (!buf_src(ARG(1), &d, &n)) return js_throw_err(E_TYPE, "O pedaço precisa ser um ArrayBuffer ou uma view (Uint8Array...)");
        if (z->comp) {
            z_head_out(z, &out);
            if (z->fmt == ZF_GZIP) z->crc = crc32_up(z->crc, d, n);
            else if (z->fmt == ZF_ZLIB) z->adler = adler_up(z->adler, d, n);
            z->total += n;
            if (!z_deflate_chunk(z, d, n, &out)) { kfree(out.p); return js_throw_err(E_RANGE, "memória insuficiente"); }
            return z_out(&out);
        }
        if (z->stage == ZS_DONE && n) return z_fail("DecompressionStream: dados depois do fim do stream comprimido");
        if (z->inlen + n > z->incap) {
            usize nc = z->incap ? z->incap : 4096;
            while (nc < z->inlen + n) nc *= 2;
            u8 *ni = krealloc(z->in, nc);
            if (!ni) return js_throw_err(E_RANGE, "memória insuficiente");
            z->in = ni;
            z->incap = nc;
        }
        if (n) memcpy(z->in + z->inlen, d, n);
        z->inlen += n;
        jsv r = z_inflate_run(z, &out);
        if (r == JS_EXC) { kfree(out.p); return JS_EXC; }
        return z_out(&out);
    }
    /* fim */
    if (z->comp) {
        z_head_out(z, &out);
        zw_bits(z, &out, 1, 1);   /* bloco final vazio de Huffman fixo */
        zw_bits(z, &out, 1, 2);
        zw_lit(z, &out, 256);
        if (z->bn) zw_bits(z, &out, 0, 8 - z->bn);
        if (z->fmt == ZF_GZIP) {
            u8 t[8] = { (u8)z->crc, (u8)(z->crc >> 8), (u8)(z->crc >> 16), (u8)(z->crc >> 24),
                        (u8)z->total, (u8)(z->total >> 8), (u8)(z->total >> 16), (u8)(z->total >> 24) };
            zb_put(&out, t, 8);
        } else if (z->fmt == ZF_ZLIB) {
            u8 t[4] = { (u8)(z->adler >> 24), (u8)(z->adler >> 16), (u8)(z->adler >> 8), (u8)z->adler };
            zb_put(&out, t, 4);
        }
        z->stage = ZS_DONE;
        if (out.err) { kfree(out.p); return js_throw_err(E_RANGE, "memória insuficiente"); }
        return z_out(&out);
    }
    jsv r = z_inflate_run(z, &out);
    if (r == JS_EXC) { kfree(out.p); return JS_EXC; }
    if (z->stage != ZS_DONE) { kfree(out.p); return z_fail("DecompressionStream: o stream comprimido terminou no meio"); }
    return z_out(&out);
}

/* objeto com os três natives, passado à fonte dos Streams */
jsv js_zlib_natives(void) {
    jsobj *o = js_newobj(J.realm->p_object);
    js_defnative(o, "init", z_native, 2, 0);
    js_defnative(o, "push", z_native, 2, 1);
    js_defnative(o, "end", z_native, 1, 2);
    return jv_obj(o);
}
