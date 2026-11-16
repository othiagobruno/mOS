/* Casca C do codificador H.264 do OpenH264 (userland/third_party/openh264, BSD-2): ver h264_glue.h.
 * C++ freestanding, uma thread. */
#include "codec_api.h"
#include "h264_glue.h"

extern "C" {
void *malloc(unsigned long n);
void free(void *p);
void *memset(void *d, int c, unsigned long n);

struct cx_h264enc {
    ISVCEncoder *enc;
    cx_h264enc_cfg cfg;
};

cx_h264enc *cx_h264enc_open(const cx_h264enc_cfg *c) {
    if (!c || c->width < 16 || c->height < 16 || (c->width & 1) || (c->height & 1)) return 0;
    ISVCEncoder *e = 0;
    if (WelsCreateSVCEncoder(&e) || !e) return 0;
    SEncParamExt p;
    e->GetDefaultParams(&p);
    float fps = c->fps > 0 ? (float)c->fps : 30.0f;
    p.iUsageType = CAMERA_VIDEO_REAL_TIME;   /* o único modo de vídeo de câmera que o 2.6.0 valida; arquivo: sem descarte */
    p.fMaxFrameRate = fps;
    p.iPicWidth = c->width;
    p.iPicHeight = c->height;
    p.iTargetBitrate = c->bitrate > 0 ? c->bitrate : 0;
    p.iMaxBitrate = c->bitrate > 0 ? c->bitrate * 2 : UNSPECIFIED_BIT_RATE;
    p.iRCMode = c->bitrate > 0 ? RC_BITRATE_MODE : RC_OFF_MODE;
    p.bEnableFrameSkip = c->bitrate > 0 && c->realtime;   /* arquivo: nunca pula quadro */
    p.iSpatialLayerNum = 1;
    p.iTemporalLayerNum = 1;
    p.uiIntraPeriod = c->keyint > 0 ? (unsigned)c->keyint : 0;
    p.iMultipleThreadIdc = 1;
    p.iEntropyCodingModeFlag = c->profile >= 77 ? (c->entropy == 1 ? 0 : 1) : 0;
    p.bPrefixNalAddingCtrl = false;
    p.eSpsPpsIdStrategy = CONSTANT_ID;
    SSpatialLayerConfig &l = p.sSpatialLayers[0];
    l.iVideoWidth = c->width;
    l.iVideoHeight = c->height;
    l.fFrameRate = fps;
    l.iSpatialBitrate = p.iTargetBitrate;
    l.iMaxSpatialBitrate = p.iMaxBitrate;
    l.uiProfileIdc = c->profile >= 100 ? PRO_HIGH : c->profile >= 77 ? PRO_MAIN : PRO_BASELINE;
    l.uiLevelIdc = LEVEL_UNKNOWN;
    l.sSliceArgument.uiSliceMode = SM_SINGLE_SLICE;
    /* VUI: faixa de vídeo e a matriz da conversão do VTCompressionSession (HD BT.709, SD BT.601), como o macOS declara */
    bool hd = c->height >= 720;
    l.bVideoSignalTypePresent = true;
    l.uiVideoFormat = VF_UNDEF;
    l.bFullRange = false;
    l.bColorDescriptionPresent = true;
    l.uiColorPrimaries = hd ? CP_BT709 : CP_SMPTE170M;
    l.uiTransferCharacteristics = hd ? TRC_BT709 : TRC_SMPTE170M;
    l.uiColorMatrix = hd ? CM_BT709 : CM_SMPTE170M;
    if (c->bitrate <= 0) { l.iDLayerQp = c->qp > 0 && c->qp <= 51 ? c->qp : 24; }
    if (e->InitializeExt(&p) != 0) { WelsDestroySVCEncoder(e); return 0; }
    int fmt = videoFormatI420;
    e->SetOption(ENCODER_OPTION_DATAFORMAT, &fmt);
    cx_h264enc *x = (cx_h264enc *)malloc(sizeof(cx_h264enc));
    if (!x) { e->Uninitialize(); WelsDestroySVCEncoder(e); return 0; }
    x->enc = e;
    x->cfg = *c;
    return x;
}

int cx_h264enc_encode(cx_h264enc *x, const uint8_t *y, const uint8_t *u, const uint8_t *v, int sy, int suv, uint64_t ts_ms,
                      int force_idr, cx_h264enc_nal nal, void *ctx, int *idr) {
    if (idr) *idr = 0;
    if (!x || !y || !u || !v) return -1;
    if (force_idr) x->enc->ForceIntraFrame(true);
    SSourcePicture pic;
    memset(&pic, 0, sizeof pic);
    pic.iPicWidth = x->cfg.width;
    pic.iPicHeight = x->cfg.height;
    pic.iColorFormat = videoFormatI420;
    pic.iStride[0] = sy;
    pic.iStride[1] = pic.iStride[2] = suv;
    pic.pData[0] = (unsigned char *)y;
    pic.pData[1] = (unsigned char *)u;
    pic.pData[2] = (unsigned char *)v;
    pic.uiTimeStamp = (long long)ts_ms;
    SFrameBSInfo info;
    memset(&info, 0, sizeof info);
    if (x->enc->EncodeFrame(&pic, &info) != cmResultSuccess) return -2;
    if (info.eFrameType == videoFrameTypeSkip || info.eFrameType == videoFrameTypeInvalid) return 0;
    if (idr) *idr = info.eFrameType == videoFrameTypeIDR;
    for (int li = 0; li < info.iLayerNum; li++) {
        const SLayerBSInfo &L = info.sLayerInfo[li];
        const uint8_t *p = L.pBsBuf;
        for (int k = 0; k < L.iNalCount; k++) {
            int len = L.pNalLengthInByte[k];
            int sc = len >= 4 && !p[0] && !p[1] && !p[2] && p[3] == 1 ? 4 : len >= 3 && !p[0] && !p[1] && p[2] == 1 ? 3 : 0;
            if (nal && len > sc) nal(ctx, p + sc, len - sc, p[sc] & 31);
            p += len;
        }
    }
    return 0;
}

int cx_h264enc_set_bitrate(cx_h264enc *x, int bitrate) {
    if (!x || bitrate <= 0) return -1;
    SBitrateInfo b;
    b.iLayer = SPATIAL_LAYER_ALL;
    b.iBitrate = bitrate;
    x->cfg.bitrate = bitrate;
    return x->enc->SetOption(ENCODER_OPTION_BITRATE, &b) == cmResultSuccess ? 0 : -1;
}
int cx_h264enc_set_fps(cx_h264enc *x, double fps) {
    if (!x || fps <= 0) return -1;
    float f = (float)fps;
    x->cfg.fps = fps;
    return x->enc->SetOption(ENCODER_OPTION_FRAME_RATE, &f) == cmResultSuccess ? 0 : -1;
}
void cx_h264enc_close(cx_h264enc *x) {
    if (!x) return;
    x->enc->Uninitialize();
    WelsDestroySVCEncoder(x->enc);
    free(x);
}
}
