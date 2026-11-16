/* mOS — núcleo da classe de vídeo USB (UVC 1.0/1.1/1.5), sem dependência do kernel: descritores de VideoStreaming,
 * controle PROBE/COMMIT, escolha da alternate setting e remontagem dos quadros a partir dos payloads isócronos.
 * Usado por kernel/dev/uvc.c; teste de host: tools/uvc_core_test.c. Código original do mOS (MIT). */
#pragma once
#include <stdint.h>

#define UVC_MAX_FORMATS 8
#define UVC_MAX_FRAMES 16
#define UVC_MAX_INTERVALS 8
enum { UVC_FMT_YUY2 = 1, UVC_FMT_NV12 = 2, UVC_FMT_MJPEG = 3, UVC_FMT_OTHER = 4 };
typedef struct {
    uint8_t index;                  /* bFrameIndex */
    uint16_t width, height;
    uint32_t max_size;              /* dwMaxVideoFrameBufferSize */
    uint32_t def_interval;          /* 100 ns */
    uint8_t ninterval;
    uint32_t interval[UVC_MAX_INTERVALS];   /* lista; contínuo vira {mín, máx} */
} uvc_frame;
typedef struct {
    uint8_t index;                  /* bFormatIndex */
    int kind;                       /* UVC_FMT_* */
    uint32_t fourcc;                /* os 4 primeiros bytes do GUID (descompactado) */
    uint8_t bpp, def_frame;
    uint8_t nframe;
    uvc_frame frame[UVC_MAX_FRAMES];
} uvc_format;
typedef struct { uint8_t nformat; uvc_format format[UVC_MAX_FORMATS]; uint8_t terminal_link; } uvc_stream_info;
/* descritores de classe da interface VideoStreaming (it->extra de alt 0); 0 ok */
int uvc_parse_vs(const uint8_t *extra, uint32_t len, uvc_stream_info *out);
/* bcdUVC do cabeçalho da VideoControl (0 se não achou) */
uint16_t uvc_parse_vc_version(const uint8_t *extra, uint32_t len);
/* tamanho do bloco PROBE/COMMIT pela versão: 26 (1.0), 34 (1.1), 48 (1.5) */
int uvc_probe_len(uint16_t bcd);
typedef struct {
    uint16_t hint;
    uint8_t format, frame;
    uint32_t interval;
    uint32_t max_frame_size, max_payload;
    uint32_t clock_hz;
} uvc_probe;
void uvc_probe_pack(const uvc_probe *p, uint8_t *buf, int len);
void uvc_probe_unpack(const uint8_t *buf, int len, uvc_probe *p);
/* escolhe a menor banda >= need entre as alternates (bandas por intervalo de serviço); devolve o índice ou -1 */
int uvc_pick_alt(const uint32_t *bandwidth, int nalt, uint32_t need);

/* remontagem: cada payload tem cabeçalho (bHeaderLength, bmHeaderInfo: FID bit0, EOF bit1, ERR bit6) */
typedef void (*uvc_frame_cb)(void *ctx, const uint8_t *data, uint32_t len, int fid);
typedef struct {
    uint8_t *buf;
    uint32_t cap, len;
    int fid, have_fid, bad;
    uint32_t expect;                /* tamanho exato do quadro descompactado (0 = qualquer, MJPEG) */
    uvc_frame_cb cb;
    void *ctx;
    uint32_t frames, dropped;
} uvc_asm;
void uvc_asm_init(uvc_asm *a, uint8_t *buf, uint32_t cap, uint32_t expect, uvc_frame_cb cb, void *ctx);
void uvc_asm_payload(uvc_asm *a, const uint8_t *p, uint32_t n);
