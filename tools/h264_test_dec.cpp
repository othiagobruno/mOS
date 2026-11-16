// Harness do tools/h264_test.sh: Annex B -> I420 cru com o OpenH264 vendorizado (userland/third_party/openh264).
// uso: h264_test_dec entrada.264 saida.yuv [decode]   ("decode": saída na ordem de decodificação, m_bOutputDecodeOrder)
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "codec_api.h"
#include "welsDecoderExt.h"
int main(int c, char **v) {
    FILE *f = fopen(v[1], "rb");
    if (!f) return 2;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *b = (unsigned char *)malloc(n + 4);
    if (fread(b, 1, n, f) != (size_t)n) return 2;
    fclose(f);
    FILE *o = fopen(v[2], "wb");
    ISVCDecoder *d = 0;
    WelsCreateDecoder(&d);
    if (c > 3 && !strcmp(v[3], "decode")) static_cast<WelsDec::CWelsDecoder *>(d)->m_bOutputDecodeOrder = true;
    SDecodingParam p; memset(&p, 0, sizeof p);
    p.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_AVC;
    p.eEcActiveIdc = ERROR_CON_DISABLE;
    d->Initialize(&p);
    int frames = 0, errs = 0; long at = 0;
    auto out = [&](unsigned char **y, SBufferInfo &bi) {
        if (bi.iBufferStatus != 1) return;
        int w = bi.UsrData.sSystemBuffer.iWidth, h = bi.UsrData.sSystemBuffer.iHeight;
        for (int r = 0; r < h; r++) fwrite(y[0] + r * bi.UsrData.sSystemBuffer.iStride[0], 1, w, o);
        for (int k = 1; k < 3; k++) for (int r = 0; r < h / 2; r++) fwrite(y[k] + r * bi.UsrData.sSystemBuffer.iStride[1], 1, w / 2, o);
        frames++;
    };
    while (at < n) {   // uma NAL por chamada
        long e = at + 3;
        while (e + 3 < n && !(b[e] == 0 && b[e + 1] == 0 && (b[e + 2] == 1 || (b[e + 2] == 0 && b[e + 3] == 1)))) e++;
        if (e + 3 >= n) e = n;
        unsigned char *y[3]; SBufferInfo bi; memset(&bi, 0, sizeof bi);
        if (d->DecodeFrameNoDelay(b + at, (int)(e - at), y, &bi)) errs++;
        out(y, bi);
        at = e;
    }
    int end = 1;
    d->SetOption(DECODER_OPTION_END_OF_STREAM, &end);
    for (int k = 0; k < 32; k++) {
        unsigned char *y[3]; SBufferInfo bi; memset(&bi, 0, sizeof bi);
        d->FlushFrame(y, &bi);
        if (bi.iBufferStatus != 1) break;
        out(y, bi);
    }
    fprintf(stderr, "%d quadros, %d erros\n", frames, errs);
    fclose(o);
    d->Uninitialize();
    WelsDestroyDecoder(d);
    return errs != 0;
}
