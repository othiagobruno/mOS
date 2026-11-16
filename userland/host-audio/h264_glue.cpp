/* Casca C do OpenH264 (userland/third_party/openh264, BSD-2): ver h264_glue.h.
 * Compila como C++ freestanding (sem exceções, RTTI nem libc++), uma thread de decodificação. */
#include "codec_api.h"
#include "welsDecoderExt.h"
#include "h264_glue.h"

extern "C" {
void *malloc(unsigned long n);
void free(void *p);
void *memset(void *d, int c, unsigned long n);

struct cx_h264 {
    ISVCDecoder *dec;
    int decode_order;
};

cx_h264 *cx_h264_open(int decode_order) {
    ISVCDecoder *d = 0;
    if (WelsCreateDecoder(&d) || !d) return 0;
    SDecodingParam p;
    memset(&p, 0, sizeof p);
    p.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_AVC;
    p.eEcActiveIdc = ERROR_CON_DISABLE;   /* quadro com erro não sai inventado */
    if (d->Initialize(&p) != 0) { WelsDestroyDecoder(d); return 0; }
    static_cast<WelsDec::CWelsDecoder *>(d)->m_bOutputDecodeOrder = decode_order != 0;
    cx_h264 *c = (cx_h264 *)malloc(sizeof(cx_h264));
    if (!c) { d->Uninitialize(); WelsDestroyDecoder(d); return 0; }
    c->dec = d;
    c->decode_order = decode_order != 0;
    return c;
}

static void out(ISVCDecoder *d, const SBufferInfo &bi, unsigned char **y, uint64_t tag, cx_h264_emit emit, void *ctx) {
    if (bi.iBufferStatus != 1 || !y[0] || !emit) return;
    const SSysMEMBuffer &b = bi.UsrData.sSystemBuffer;
    cx_h264_color col = {0, 2, 2, 2};
    const WelsDec::SVui *vui = static_cast<WelsDec::CWelsDecoder *>(d)->MosActiveVui();
    if (vui && vui->bVideoSignalTypePresentFlag) {
        col.full_range = vui->bVideoFullRangeFlag;
        if (vui->bColourDescripPresentFlag) {
            col.primaries = vui->uiColourPrimaries;
            col.transfer = vui->uiTransferCharacteristics;
            col.matrix = vui->uiMatrixCoeffs;
        }
    }
    emit(ctx, y[0], y[1], y[2], b.iStride[0], b.iStride[1], b.iWidth, b.iHeight, tag, &col);
}

int cx_h264_decode(cx_h264 *c, const uint8_t *annexb, int n, uint64_t tag, cx_h264_emit emit, void *ctx) {
    if (!c || !annexb || n <= 0) return -1;
    unsigned char *y[3] = {0, 0, 0};
    SBufferInfo bi;
    memset(&bi, 0, sizeof bi);
    bi.uiInBsTimeStamp = tag;
    DECODING_STATE st = c->dec->DecodeFrameNoDelay(annexb, n, y, &bi);
    out(c->dec, bi, y, c->decode_order ? tag : bi.uiOutYuvTimeStamp, emit, ctx);
    return st == dsErrorFree ? 0 : -(int)st;
}

int cx_h264_flush(cx_h264 *c, cx_h264_emit emit, void *ctx) {
    if (!c) return -1;
    int end = 1;
    c->dec->SetOption(DECODER_OPTION_END_OF_STREAM, &end);
    for (int k = 0; k < 64; k++) {
        unsigned char *y[3] = {0, 0, 0};
        SBufferInfo bi;
        memset(&bi, 0, sizeof bi);
        c->dec->FlushFrame(y, &bi);
        if (bi.iBufferStatus != 1) break;
        out(c->dec, bi, y, bi.uiOutYuvTimeStamp, emit, ctx);
    }
    end = 0;
    c->dec->SetOption(DECODER_OPTION_END_OF_STREAM, &end);
    return 0;
}

void cx_h264_close(cx_h264 *c) {
    if (!c) return;
    c->dec->Uninitialize();
    WelsDestroyDecoder(c->dec);
    free(c);
}
}
