/* Teste de host do kernel/dev/uvc_core.c: descritores sintéticos (YUY2 + MJPEG), PROBE/COMMIT, escolha da alt
 * e remontagem de quadros com FID/EOF/ERR. gcc -std=gnu11 -O1 tools/uvc_core_test.c kernel/dev/uvc_core.c */
#include <stdio.h>
#include <string.h>
#include "../kernel/dev/uvc_core.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FALHOU %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static int put_frame(uint8_t *d, uint8_t sub, uint8_t idx, uint16_t w, uint16_t h, uint32_t maxsz, uint32_t def, int nint, const uint32_t *iv) {
    int bl = 26 + 4 * (nint ? nint : 3);
    memset(d, 0, bl);
    d[0] = bl; d[1] = 0x24; d[2] = sub; d[3] = idx;
    d[5] = w; d[6] = w >> 8; d[7] = h; d[8] = h >> 8;
    memcpy(d + 17, &maxsz, 4); memcpy(d + 21, &def, 4);
    d[25] = nint;
    for (int i = 0; i < (nint ? nint : 3); i++) memcpy(d + 26 + 4 * i, &iv[i], 4);
    return bl;
}
static uint8_t got[1 << 16]; static uint32_t gotlen; static int gotn;
static void cb(void *ctx, const uint8_t *d, uint32_t n, int fid) { (void)ctx; (void)fid; memcpy(got, d, n); gotlen = n; gotn++; }
static void pay(uvc_asm *a, uint8_t info, const uint8_t *data, int n, int hl) {
    uint8_t p[2048]; memset(p, 0, hl); p[0] = hl; p[1] = info | 0x80; memcpy(p + hl, data, n); uvc_asm_payload(a, p, hl + n);
}

int main(void) {
    uint8_t d[512]; int o = 0;
    /* VS_INPUT_HEADER (13 bytes, terminal 3) */
    uint8_t ih[13] = {13, 0x24, 0x01, 2, 0, 0, 0x81, 0, 3, 0, 0, 0, 0}; memcpy(d + o, ih, 13); o += 13;
    /* FORMAT_UNCOMPRESSED YUY2, 2 quadros */
    uint8_t fu[27] = {27, 0x24, 0x04, 1, 2, 'Y', 'U', 'Y', '2', 0, 0, 0x10, 0, 0x80, 0, 0, 0xAA, 0, 0x38, 0x9B, 0x71, 16, 1, 0, 0, 0, 0};
    memcpy(d + o, fu, 27); o += 27;
    uint32_t iv1[2] = {333333, 666666};
    o += put_frame(d + o, 0x05, 1, 640, 480, 614400, 333333, 2, iv1);
    uint32_t ivc[3] = {333333, 1000000, 333333};
    o += put_frame(d + o, 0x05, 2, 320, 240, 0, 333333, 0, ivc);   /* contínuo, tamanho calculado */
    /* um descritor que não é de classe no meio (endpoint) */
    uint8_t ep[7] = {7, 0x05, 0x81, 0x05, 0x00, 0x0C, 1}; memcpy(d + o, ep, 7); o += 7;
    /* FORMAT_MJPEG, 1 quadro */
    uint8_t fm[11] = {11, 0x24, 0x06, 2, 1, 0, 1, 0, 0, 0, 0}; memcpy(d + o, fm, 11); o += 11;
    uint32_t iv2[1] = {333333};
    o += put_frame(d + o, 0x07, 1, 1280, 720, 1843200, 333333, 1, iv2);
    /* quadro MJPEG fora de lugar não pode entrar no YUY2: já fechou; frame truncado no fim é ignorado */
    d[o] = 40; d[o + 1] = 0x24; d[o + 2] = 0x05; o += 3;

    uvc_stream_info si;
    CHECK(uvc_parse_vs(d, o, &si) == 0);
    CHECK(si.nformat == 2);
    CHECK(si.terminal_link == 3);
    CHECK(si.format[0].kind == UVC_FMT_YUY2 && si.format[0].fourcc == 0x32595559 && si.format[0].bpp == 16 && si.format[0].nframe == 2);
    CHECK(si.format[0].frame[0].width == 640 && si.format[0].frame[0].height == 480 && si.format[0].frame[0].max_size == 614400);
    CHECK(si.format[0].frame[0].ninterval == 2 && si.format[0].frame[0].interval[1] == 666666);
    CHECK(si.format[0].frame[1].ninterval == 2 && si.format[0].frame[1].interval[0] == 333333 && si.format[0].frame[1].interval[1] == 1000000);
    CHECK(si.format[0].frame[1].max_size == 320 * 240 * 2);
    CHECK(si.format[1].kind == UVC_FMT_MJPEG && si.format[1].index == 2 && si.format[1].nframe == 1 && si.format[1].frame[0].width == 1280);
    CHECK(uvc_parse_vs(ep, 7, &si) == -1);

    uint8_t vc[13] = {13, 0x24, 0x01, 0x10, 0x01, 0, 0, 0, 0, 0, 0, 1, 1};
    CHECK(uvc_parse_vc_version(vc, 13) == 0x0110);
    CHECK(uvc_probe_len(0x0100) == 26 && uvc_probe_len(0x0110) == 34 && uvc_probe_len(0x0150) == 48);

    uvc_probe p = {1, 2, 1, 333333, 1843200, 3072, 48000000}, q;
    uint8_t b[48];
    uvc_probe_pack(&p, b, 34);
    CHECK(b[0] == 1 && b[2] == 2 && b[3] == 1 && b[4] == 0x15 && b[5] == 0x16 && b[6] == 0x05);   /* 333333 = 0x051615 */
    CHECK(b[22] == 0x00 && b[23] == 0x0C && b[30] == 3);
    uvc_probe_unpack(b, 34, &q);
    CHECK(q.format == 2 && q.frame == 1 && q.interval == 333333 && q.max_frame_size == 1843200 && q.max_payload == 3072 && q.clock_hz == 48000000);
    uvc_probe_pack(&p, b, 26);
    uvc_probe_unpack(b, 26, &q);
    CHECK(q.clock_hz == 0 && q.max_payload == 3072);

    uint32_t bw[4] = {0, 1024, 3072, 2048};
    CHECK(uvc_pick_alt(bw, 4, 2000) == 3);
    CHECK(uvc_pick_alt(bw, 4, 3072) == 2);
    CHECK(uvc_pick_alt(bw, 4, 9000) == 2);
    CHECK(uvc_pick_alt(bw, 0, 1) == -1);

    /* remontagem descompactada: quadro de 1000 bytes em 3 payloads, EOF no último */
    static uint8_t buf[1 << 16], src[4000];
    for (int i = 0; i < 4000; i++) src[i] = (uint8_t)(i * 7 + 1);
    uvc_asm a;
    uvc_asm_init(&a, buf, sizeof buf, 1000, cb, 0);
    pay(&a, 0, src, 400, 12); pay(&a, 0, src + 400, 400, 2); pay(&a, 0x02, src + 800, 200, 12);
    CHECK(gotn == 1 && gotlen == 1000 && !memcmp(got, src, 1000));
    /* próximo quadro sem EOF: FID troca fecha o anterior */
    pay(&a, 1, src, 500, 12); pay(&a, 1, src + 500, 500, 12); pay(&a, 0, src, 0, 12);
    CHECK(gotn == 2 && gotlen == 1000);
    /* quadro com ERR é descartado */
    pay(&a, 0, src, 500, 12); pay(&a, 0x40, src + 500, 500, 12); pay(&a, 0x02, src, 0, 12);
    CHECK(gotn == 2 && a.dropped == 1);
    /* quadro curto (pacote perdido) é descartado */
    pay(&a, 1, src, 500, 12); pay(&a, 1 | 0x02, src, 300, 12);
    CHECK(gotn == 2 && a.dropped == 2);
    /* MJPEG: tamanho livre, exige SOI */
    uvc_asm_init(&a, buf, sizeof buf, 0, cb, 0);
    uint8_t jpg[300]; memset(jpg, 0x55, sizeof jpg); jpg[0] = 0xFF; jpg[1] = 0xD8; jpg[298] = 0xFF; jpg[299] = 0xD9;
    pay(&a, 0, jpg, 150, 12); pay(&a, 0x02, jpg + 150, 150, 12);
    CHECK(gotn == 3 && gotlen == 300 && got[299] == 0xD9);
    pay(&a, 1, jpg + 2, 100, 12); pay(&a, 1 | 0x02, jpg, 10, 12);
    CHECK(gotn == 3 && a.dropped == 1);
    /* estouro do buffer */
    uvc_asm_init(&a, buf, 100, 0, cb, 0);
    pay(&a, 0, jpg, 150, 2); pay(&a, 0x02, jpg, 0, 2);
    CHECK(gotn == 3 && a.dropped == 1);
    /* cabeçalho inválido */
    uint8_t bad[4] = {20, 0x80, 0, 0}; uvc_asm_payload(&a, bad, 4); CHECK(a.bad == 1);

    /* unidades: Camera Terminal (id 1, AE mode + exposição + foco auto) e Processing Unit (id 2, brilho + WB auto) */
    uint8_t vcd[18 + 11] = {18, 0x24, 0x02, 1, 0x01, 0x02, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0x0A, 0x00, 0x02,
                            11, 0x24, 0x05, 2, 1, 0, 0, 3, 0x01, 0x10, 0x00};
    uvc_units u;
    uvc_parse_units(vcd, sizeof vcd, &u);
    CHECK(u.ct_id == 1 && u.pu_id == 2 && u.ct_bm == 0x2000A && u.pu_bm == 0x1001);
    CHECK(uvc_ctl_supported(&u, UVC_CTL_AE_MODE) && uvc_ctl_supported(&u, UVC_CTL_EXPOSURE) && uvc_ctl_supported(&u, UVC_CTL_FOCUS_AUTO));
    CHECK(!uvc_ctl_supported(&u, UVC_CTL_FOCUS) && uvc_ctl_supported(&u, UVC_CTL_BRIGHTNESS) && uvc_ctl_supported(&u, UVC_CTL_WB_AUTO) && !uvc_ctl_supported(&u, UVC_CTL_CONTRAST));
    uint8_t cb[4];
    uvc_ctl_encode(uvc_ctl(UVC_CTL_BRIGHTNESS), -5, cb);
    CHECK(cb[0] == 0xFB && cb[1] == 0xFF && uvc_ctl_decode(uvc_ctl(UVC_CTL_BRIGHTNESS), cb) == -5);
    uvc_ctl_encode(uvc_ctl(UVC_CTL_EXPOSURE), 156, cb);
    CHECK(uvc_ctl_decode(uvc_ctl(UVC_CTL_EXPOSURE), cb) == 156);
    printf(fails ? "uvc_core: %d falhas\n" : "uvc_core: ok\n", fails);
    return fails != 0;
}
