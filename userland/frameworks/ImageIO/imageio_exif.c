#include "imageio_exif.h"
#include <string.h>

typedef struct { const uint8_t *b; size_t n; int le; } tiff_t;

static uint16_t rd16(const tiff_t *t, size_t o) {
    if (o + 2 > t->n) return 0;
    return t->le ? (uint16_t)(t->b[o] | t->b[o + 1] << 8) : (uint16_t)(t->b[o] << 8 | t->b[o + 1]);
}
static uint32_t rd32(const tiff_t *t, size_t o) {
    if (o + 4 > t->n) return 0;
    return t->le ? (uint32_t)t->b[o] | (uint32_t)t->b[o + 1] << 8 | (uint32_t)t->b[o + 2] << 16 | (uint32_t)t->b[o + 3] << 24
                 : (uint32_t)t->b[o] << 24 | (uint32_t)t->b[o + 1] << 16 | (uint32_t)t->b[o + 2] << 8 | (uint32_t)t->b[o + 3];
}

/* os tags que o ImageIO da Apple põe em {TIFF} e {Exif} e que os apps leem */
static const struct { int ifd; uint16_t tag; const char *name; } names[] = {
    {EXIF_IFD_TIFF, 0x010F, "Make"}, {EXIF_IFD_TIFF, 0x0110, "Model"}, {EXIF_IFD_TIFF, 0x0112, "Orientation"},
    {EXIF_IFD_TIFF, 0x011A, "XResolution"}, {EXIF_IFD_TIFF, 0x011B, "YResolution"}, {EXIF_IFD_TIFF, 0x0128, "ResolutionUnit"},
    {EXIF_IFD_TIFF, 0x0131, "Software"}, {EXIF_IFD_TIFF, 0x0132, "DateTime"},
    {EXIF_IFD_EXIF, 0x829A, "ExposureTime"}, {EXIF_IFD_EXIF, 0x829D, "FNumber"}, {EXIF_IFD_EXIF, 0x8822, "ExposureProgram"},
    {EXIF_IFD_EXIF, 0x8827, "ISOSpeedRatings"}, {EXIF_IFD_EXIF, 0x9003, "DateTimeOriginal"}, {EXIF_IFD_EXIF, 0x9004, "DateTimeDigitized"},
    {EXIF_IFD_EXIF, 0x9010, "OffsetTime"}, {EXIF_IFD_EXIF, 0x9011, "OffsetTimeOriginal"}, {EXIF_IFD_EXIF, 0x9012, "OffsetTimeDigitized"},
    {EXIF_IFD_EXIF, 0x9201, "ShutterSpeedValue"}, {EXIF_IFD_EXIF, 0x9202, "ApertureValue"}, {EXIF_IFD_EXIF, 0x9203, "BrightnessValue"},
    {EXIF_IFD_EXIF, 0x9204, "ExposureBiasValue"}, {EXIF_IFD_EXIF, 0x9207, "MeteringMode"}, {EXIF_IFD_EXIF, 0x9209, "Flash"},
    {EXIF_IFD_EXIF, 0x920A, "FocalLength"}, {EXIF_IFD_EXIF, 0x927C, "MakerNote"}, {EXIF_IFD_EXIF, 0x9290, "SubsecTime"},
    {EXIF_IFD_EXIF, 0x9291, "SubsecTimeOriginal"}, {EXIF_IFD_EXIF, 0xA001, "ColorSpace"}, {EXIF_IFD_EXIF, 0xA002, "PixelXDimension"},
    {EXIF_IFD_EXIF, 0xA003, "PixelYDimension"}, {EXIF_IFD_EXIF, 0xA217, "SensingMethod"}, {EXIF_IFD_EXIF, 0xA402, "ExposureMode"},
    {EXIF_IFD_EXIF, 0xA403, "WhiteBalance"}, {EXIF_IFD_EXIF, 0xA405, "FocalLenIn35mmFilm"}, {EXIF_IFD_EXIF, 0xA406, "SceneCaptureType"},
    {EXIF_IFD_EXIF, 0xA432, "LensSpecification"}, {EXIF_IFD_EXIF, 0xA433, "LensMake"}, {EXIF_IFD_EXIF, 0xA434, "LensModel"},
};
const char *mos_exif_name(int ifd, uint16_t tag) {
    for (size_t i = 0; i < sizeof names / sizeof names[0]; i++) if (names[i].ifd == ifd && names[i].tag == tag) return names[i].name;
    return 0;
}

/* tamanho em bytes de cada tipo TIFF (1 BYTE, 2 ASCII, 3 SHORT, 4 LONG, 5 RATIONAL, 7 UNDEFINED, 9 SLONG, 10 SRATIONAL) */
static size_t type_size(uint16_t t) {
    switch (t) {
    case 1: case 2: case 6: case 7: return 1;
    case 3: case 8: return 2;
    case 4: case 9: case 11: return 4;
    case 5: case 10: case 12: return 8;
    default: return 0;
    }
}

static void read_ifd(const tiff_t *t, size_t off, int ifd, mos_exif *out, uint32_t *exif_ptr, int depth) {
    if (depth > 2 || off + 2 > t->n) return;
    uint16_t cnt = rd16(t, off);
    for (uint16_t i = 0; i < cnt; i++) {
        size_t e = off + 2 + (size_t)i * 12;
        if (e + 12 > t->n) break;
        uint16_t tag = rd16(t, e), type = rd16(t, e + 2);
        uint32_t count = rd32(t, e + 4);
        size_t sz = type_size(type);
        if (!sz || count > 0x100000) continue;
        size_t total = sz * count, voff = total <= 4 ? e + 8 : rd32(t, e + 8);
        if (voff + total > t->n) continue;
        if (ifd == EXIF_IFD_TIFF && tag == 0x8769) { if (exif_ptr) *exif_ptr = rd32(t, e + 8); continue; }
        if (!mos_exif_name(ifd, tag) || out->n >= 64) continue;
        mos_exif_entry *x = &out->e[out->n];
        memset(x, 0, sizeof *x);
        x->ifd = ifd;
        x->tag = tag;
        if (type == 2) {
            x->kind = EXIF_STR;
            size_t k = total < sizeof x->str ? total : sizeof x->str - 1;
            memcpy(x->str, t->b + voff, k);
            x->str[k] = 0;
            while (k && (x->str[k - 1] == 0 || x->str[k - 1] == ' ')) x->str[--k] = 0;   /* ASCII termina em NUL; tira o resto */
        } else if (type == 7 || type == 1 || type == 6) {
            if (tag == 0x927C || count > 4) { x->kind = EXIF_BYTES; x->bytes = t->b + voff; x->nbytes = total; }
            else { x->kind = EXIF_NUM; for (uint32_t k = 0; k < count && k < 4; k++) x->num[x->nnum++] = t->b[voff + k]; }
        } else {
            x->kind = EXIF_NUM;
            for (uint32_t k = 0; k < count && k < 4; k++) {
                size_t p = voff + k * sz;
                double v = 0;
                switch (type) {
                case 3: v = rd16(t, p); break;
                case 8: v = (int16_t)rd16(t, p); break;
                case 4: v = rd32(t, p); break;
                case 9: v = (int32_t)rd32(t, p); break;
                case 5: { uint32_t a = rd32(t, p), b = rd32(t, p + 4); v = b ? (double)a / b : 0; break; }
                case 10: { int32_t a = (int32_t)rd32(t, p), b = (int32_t)rd32(t, p + 4); v = b ? (double)a / b : 0; break; }
                default: break;
                }
                x->num[x->nnum++] = v;
            }
        }
        out->n++;
    }
}

static int parse_tiff(const uint8_t *b, size_t n, mos_exif *out) {
    tiff_t t = {b, n, 0};
    if (n < 8) return -1;
    if (b[0] == 'I' && b[1] == 'I') t.le = 1;
    else if (!(b[0] == 'M' && b[1] == 'M')) return -1;
    if (rd16(&t, 2) != 42) return -1;
    uint32_t ifd0 = rd32(&t, 4), exif = 0;
    read_ifd(&t, ifd0, EXIF_IFD_TIFF, out, &exif, 0);
    if (exif) read_ifd(&t, exif, EXIF_IFD_EXIF, out, 0, 1);
    return out->n ? 0 : -1;
}

int mos_exif_parse(const uint8_t *d, size_t n, mos_exif *out) {
    memset(out, 0, sizeof *out);
    if (!d || n < 8) return -1;
    if ((d[0] == 'I' && d[1] == 'I') || (d[0] == 'M' && d[1] == 'M')) return parse_tiff(d, n, out);
    if (d[0] != 0xFF || d[1] != 0xD8) return -1;
    size_t p = 2;
    while (p + 4 <= n) {
        if (d[p] != 0xFF) return -1;
        uint8_t mk = d[p + 1];
        if (mk == 0xD9 || mk == 0xDA) break;                  /* fim ou início dos dados: não há mais APPn */
        size_t len = (size_t)d[p + 2] << 8 | d[p + 3];
        if (len < 2 || p + 2 + len > n) return -1;
        if (mk == 0xE1 && len >= 8 && !memcmp(d + p + 4, "Exif\0\0", 6)) return parse_tiff(d + p + 10, len - 8, out);
        p += 2 + len;
    }
    return -1;
}

const mos_exif_entry *mos_exif_find(const mos_exif *x, int ifd, uint16_t tag) {
    for (int i = 0; x && i < x->n; i++) if (x->e[i].ifd == ifd && x->e[i].tag == tag) return &x->e[i];
    return 0;
}
