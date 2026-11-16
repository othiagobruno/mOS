/*
 * mOS — mnet: Content-Encoding em fluxo (RFC 9110 §8.4): gzip/x-gzip, deflate (zlib, ou cru como alguns
 * servidores mandam) e br. Codificações em cadeia são desfeitas da última para a primeira.
 * zlib 1.3.2 e Brotli (MIT) já estão no libSystem (compression_z.c e o link de libcompression).
 */
#include "mnet.h"
#define Z_SOLO 1
#include "../third_party/zlib/zlib.h"
/* API pública do decodificador Brotli (c/include/brotli/decode.h), declarada aqui: o link do libSystem
   não tem o -I do brotli */
typedef struct BrotliDecoderStateStruct BrotliDecoderState;
typedef enum { BROTLI_DECODER_RESULT_ERROR = 0, BROTLI_DECODER_RESULT_SUCCESS = 1,
               BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT = 2, BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT = 3 } BrotliDecoderResult;
BrotliDecoderState *BrotliDecoderCreateInstance(void *(*alloc)(void *, size_t), void (*fr)(void *, void *), void *opaque);
void BrotliDecoderDestroyInstance(BrotliDecoderState *s);
BrotliDecoderResult BrotliDecoderDecompressStream(BrotliDecoderState *s, size_t *avail_in, const uint8_t **next_in,
                                                  size_t *avail_out, uint8_t **next_out, size_t *total_out);

enum { D_GZIP = 1, D_DEFLATE, D_BR };
typedef struct {
    int kind, started, done, raw_tried;
    z_stream z;
    BrotliDecoderState *br;
    uint8_t first[2]; int nfirst;          /* deflate: os dois primeiros bytes decidem zlib x cru */
} stage_t;
struct mnet_dec {
    stage_t st[4];
    int n;
    int (*sink)(void *, const uint8_t *, size_t);
    void *ctx;
    const char *err;
};

static voidpf zalloc_(voidpf o, uInt n, uInt m) { (void)o; return calloc(n, m); }
static void zfree_(voidpf o, voidpf p) { (void)o; free(p); }

static int stage_init(stage_t *s, int kind) {
    memset(s, 0, sizeof *s);
    s->kind = kind;
    if (kind == D_BR) return (s->br = BrotliDecoderCreateInstance(0, 0, 0)) ? 0 : -1;
    s->z.zalloc = zalloc_; s->z.zfree = zfree_;
    return inflateInit2(&s->z, kind == D_GZIP ? 16 + 15 : 15) == Z_OK ? 0 : -1;
}
static void stage_free(stage_t *s) {
    if (s->kind == D_BR) { if (s->br) BrotliDecoderDestroyInstance(s->br); }
    else if (s->kind) inflateEnd(&s->z);
    s->kind = 0;
}

mnet_dec_t *mnet_dec_new(const char *ce, int (*sink)(void *, const uint8_t *, size_t), void *ctx) {
    if (!ce) return 0;
    int kinds[4], n = 0;
    const char *p = ce;
    while (*p) {
        while (*p == ' ' || *p == '\t' || *p == ',') p++;
        const char *s = p;
        while (*p && *p != ',' && *p != ' ' && *p != '\t' && *p != ';') p++;
        size_t l = (size_t)(p - s);
        while (*p && *p != ',') p++;
        if (!l) continue;
        int k;
        if ((l == 4 && mnet_ieqn(s, "gzip", 4)) || (l == 6 && mnet_ieqn(s, "x-gzip", 6))) k = D_GZIP;
        else if (l == 7 && mnet_ieqn(s, "deflate", 7)) k = D_DEFLATE;
        else if (l == 2 && mnet_ieqn(s, "br", 2)) k = D_BR;
        else if (l == 8 && mnet_ieqn(s, "identity", 8)) continue;
        else return 0;                                   /* desconhecida: o chamador entrega o corpo como veio */
        if (n == 4) return 0;
        kinds[n++] = k;
    }
    if (!n) return 0;
    mnet_dec_t *d = calloc(1, sizeof *d);
    if (!d) return 0;
    d->sink = sink; d->ctx = ctx;
    /* a última aplicada é a primeira a desfazer */
    for (int i = 0; i < n; i++)
        if (stage_init(&d->st[i], kinds[n - 1 - i])) { d->n = i; mnet_dec_free(d); return 0; }
    d->n = n;
    return d;
}

static int push(mnet_dec_t *d, int i, const uint8_t *in, size_t n, int fin);

static int emit(mnet_dec_t *d, int i, const uint8_t *o, size_t n) {
    if (!n) return 0;
    if (i + 1 < d->n) return push(d, i + 1, o, n, 0);
    return d->sink && d->sink(d->ctx, o, n) ? (d->err = "recusado", -1) : 0;
}
static int push(mnet_dec_t *d, int i, const uint8_t *in, size_t n, int fin) {
    stage_t *s = &d->st[i];
    uint8_t out[16384];
    if (s->done) return 0;                               /* lixo depois do fim: ignora, como os navegadores */
    if (s->kind == D_BR) {
        size_t ai = n, ao;
        const uint8_t *ni = in;
        for (;;) {
            uint8_t *no = out; ao = sizeof out;
            BrotliDecoderResult r = BrotliDecoderDecompressStream(s->br, &ai, &ni, &ao, &no, 0);
            if (emit(d, i, out, sizeof out - ao)) return -1;
            s->started = 1;
            if (r == BROTLI_DECODER_RESULT_ERROR) { d->err = "br inválido"; return -1; }
            if (r == BROTLI_DECODER_RESULT_SUCCESS) { s->done = 1; return 0; }
            if (r == BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT) {
                if (fin) { d->err = "br truncado"; return -1; }
                return 0;
            }
        }
    }
    if (s->kind == D_DEFLATE && !s->started) {
        /* "deflate" deveria ser zlib (RFC 1950), mas há servidores que mandam deflate cru */
        while (n && s->nfirst < 2) { s->first[s->nfirst++] = *in++; n--; }
        if (s->nfirst < 2 && !fin) return 0;
        s->started = 1;
        int zlib_hdr = s->nfirst == 2 && (s->first[0] & 0x0f) == 8 && ((s->first[0] << 8) | s->first[1]) % 31 == 0;
        if (!zlib_hdr) { inflateEnd(&s->z); s->z.zalloc = zalloc_; s->z.zfree = zfree_; if (inflateInit2(&s->z, -15) != Z_OK) return -1; }
        uint8_t f[2]; memcpy(f, s->first, 2);
        int k = s->nfirst;
        s->nfirst = 0;
        if (push(d, i, f, (size_t)k, 0)) return -1;
    }
    s->started = 1;
    s->z.next_in = (Bytef *)in;
    s->z.avail_in = (uInt)n;
    do {
        s->z.next_out = out;
        s->z.avail_out = sizeof out;
        int r = inflate(&s->z, Z_NO_FLUSH);
        if (emit(d, i, out, sizeof out - s->z.avail_out)) return -1;
        if (r == Z_STREAM_END) {
            if (s->kind == D_GZIP && s->z.avail_in) {
                /* gzip com vários membros: continua no próximo */
                const uint8_t *rest = s->z.next_in; size_t rn = s->z.avail_in;
                inflateReset(&s->z);
                s->z.next_in = (Bytef *)rest; s->z.avail_in = (uInt)rn;
                continue;
            }
            s->done = 1;
            return 0;
        }
        if (r == Z_BUF_ERROR && !s->z.avail_in) break;
        if (r != Z_OK) { d->err = "dados comprimidos inválidos"; return -1; }
    } while (s->z.avail_in || !s->z.avail_out);
    return 0;
}
int mnet_dec_feed(mnet_dec_t *d, const uint8_t *in, size_t n) { return d->err ? -1 : push(d, 0, in, n, 0); }
int mnet_dec_finish(mnet_dec_t *d) {
    if (d->err) return -1;
    if (push(d, 0, 0, 0, 1)) return -1;
    for (int i = 0; i < d->n; i++)
        if (!d->st[i].done && d->st[i].started) { d->err = "dados comprimidos truncados"; return -1; }
    return 0;
}
const char *mnet_dec_error(const mnet_dec_t *d) { return d->err; }
void mnet_dec_free(mnet_dec_t *d) {
    if (!d) return;
    for (int i = 0; i < d->n; i++) stage_free(&d->st[i]);
    free(d);
}
