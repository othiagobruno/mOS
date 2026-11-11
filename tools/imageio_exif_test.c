/* EXIF do ImageIO (userland/frameworks/ImageIO/imageio_exif.c): JPEG com APP1 little-endian e TIFF big-endian.
 *   zig cc -std=c11 -Wall -Wextra -Werror -Iuserland/frameworks/ImageIO tools/imageio_exif_test.c \
 *     userland/frameworks/ImageIO/imageio_exif.c -o /tmp/imageio_exif_test && /tmp/imageio_exif_test
 */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "imageio_exif.h"

static int fails;
#define CHECK(c, m) do { if (!(c)) { fprintf(stderr, "FAIL %s\n", m); fails++; } else printf("ok %s\n", m); } while (0)

static uint8_t buf[2048];
static size_t len;
static int le = 1;
static void p8(size_t o, uint8_t v) { buf[o] = v; if (o + 1 > len) len = o + 1; }
static void p16(size_t o, uint16_t v) { if (le) { p8(o, v & 255); p8(o + 1, v >> 8); } else { p8(o, v >> 8); p8(o + 1, v & 255); } }
static void p32(size_t o, uint32_t v) { if (le) { p16(o, v & 0xFFFF); p16(o + 2, v >> 16); } else { p16(o, v >> 16); p16(o + 2, v & 0xFFFF); } }
static void pstr(size_t o, const char *s) { size_t n = strlen(s) + 1; for (size_t i = 0; i < n; i++) p8(o + i, (uint8_t)s[i]); }
static void entry(size_t at, uint16_t tag, uint16_t type, uint32_t count, uint32_t value) { p16(at, tag); p16(at + 2, type); p32(at + 4, count); p32(at + 8, value); }

/* TIFF com IFD0 (Make, Orientation, DateTime, ponteiro Exif) e IFD Exif; base = início do TIFF em buf */
static size_t build_tiff(size_t base) {
    size_t T = base;
    p8(T, le ? 'I' : 'M'); p8(T + 1, le ? 'I' : 'M'); p16(T + 2, 42); p32(T + 4, 8);
    size_t ifd0 = T + 8;
    p16(ifd0, 4);
    size_t data = ifd0 + 2 + 4 * 12 + 4;                 /* dados depois do IFD0 */
    entry(ifd0 + 2, 0x010F, 2, 6, (uint32_t)(data - T)); pstr(data, "Apple");
    size_t dt = data + 6;
    if (le) entry(ifd0 + 14, 0x0112, 3, 1, 6);           /* SHORT inline: no começo do campo */
    else { p16(ifd0 + 14, 0x0112); p16(ifd0 + 16, 3); p32(ifd0 + 18, 1); p16(ifd0 + 22, 6); p16(ifd0 + 24, 0); }
    entry(ifd0 + 26, 0x0132, 2, 20, (uint32_t)(dt - T)); pstr(dt, "2026:11:10 20:30:00");
    size_t exif = dt + 20;
    entry(ifd0 + 38, 0x8769, 4, 1, (uint32_t)(exif - T));
    p32(ifd0 + 50, 0);
    /* IFD Exif */
    p16(exif, 8);
    size_t ed = exif + 2 + 8 * 12 + 4;
    entry(exif + 2, 0x829A, 5, 1, (uint32_t)(ed - T)); p32(ed, 1); p32(ed + 4, 125);
    entry(exif + 14, 0x829D, 5, 1, (uint32_t)(ed + 8 - T)); p32(ed + 8, 18); p32(ed + 12, 10);
    if (le) entry(exif + 26, 0x8827, 3, 1, 200);
    else { p16(exif + 26, 0x8827); p16(exif + 28, 3); p32(exif + 30, 1); p16(exif + 34, 200); p16(exif + 36, 0); }
    entry(exif + 38, 0x9003, 2, 20, (uint32_t)(ed + 16 - T)); pstr(ed + 16, "2026:11:10 20:29:58");
    entry(exif + 50, 0x9204, 10, 1, (uint32_t)(ed + 36 - T)); p32(ed + 36, (uint32_t)-1); p32(ed + 40, 3);
    entry(exif + 62, 0xA432, 5, 4, (uint32_t)(ed + 44 - T));
    p32(ed + 44, 26); p32(ed + 48, 10); p32(ed + 52, 26); p32(ed + 56, 10); p32(ed + 60, 16); p32(ed + 64, 10); p32(ed + 68, 16); p32(ed + 72, 10);
    entry(exif + 74, 0x927C, 7, 10, (uint32_t)(ed + 76 - T)); for (int i = 0; i < 10; i++) p8(ed + 76 + i, (uint8_t)(0xA0 + i));
    entry(exif + 86, 0xA002, 4, 1, 4032);
    p32(exif + 98, 0);
    return ed + 86 - T;
}

int main(void) {
    mos_exif x;
    /* JPEG: SOI, APP1 "Exif\0\0" + TIFF, EOI */
    memset(buf, 0, sizeof buf); len = 0; le = 1;
    p8(0, 0xFF); p8(1, 0xD8); p8(2, 0xFF); p8(3, 0xE1);
    memcpy(buf + 6, "Exif\0\0", 6);
    size_t tl = build_tiff(12);
    size_t seg = tl + 8;
    buf[4] = (uint8_t)(seg >> 8); buf[5] = (uint8_t)seg;
    p8(12 + tl, 0xFF); p8(13 + tl, 0xD9);
    CHECK(mos_exif_parse(buf, len, &x) == 0, "JPEG com APP1 Exif");
    const mos_exif_entry *e = mos_exif_find(&x, EXIF_IFD_TIFF, 0x010F);
    CHECK(e && e->kind == EXIF_STR && !strcmp(e->str, "Apple"), "TIFF Make = Apple");
    e = mos_exif_find(&x, EXIF_IFD_TIFF, 0x0112);
    CHECK(e && e->num[0] == 6, "Orientation = 6");
    e = mos_exif_find(&x, EXIF_IFD_TIFF, 0x0132);
    CHECK(e && !strcmp(e->str, "2026:11:10 20:30:00"), "TIFF DateTime");
    e = mos_exif_find(&x, EXIF_IFD_EXIF, 0x829A);
    CHECK(e && fabs(e->num[0] - 1.0 / 125) < 1e-12, "ExposureTime 1/125");
    e = mos_exif_find(&x, EXIF_IFD_EXIF, 0x829D);
    CHECK(e && fabs(e->num[0] - 1.8) < 1e-12, "FNumber 1,8");
    e = mos_exif_find(&x, EXIF_IFD_EXIF, 0x8827);
    CHECK(e && e->num[0] == 200, "ISOSpeedRatings 200");
    e = mos_exif_find(&x, EXIF_IFD_EXIF, 0x9003);
    CHECK(e && !strcmp(e->str, "2026:11:10 20:29:58"), "DateTimeOriginal");
    e = mos_exif_find(&x, EXIF_IFD_EXIF, 0x9204);
    CHECK(e && fabs(e->num[0] + 1.0 / 3) < 1e-12, "ExposureBiasValue -1/3 (SRATIONAL)");
    e = mos_exif_find(&x, EXIF_IFD_EXIF, 0xA432);
    CHECK(e && e->nnum == 4 && fabs(e->num[0] - 2.6) < 1e-12 && fabs(e->num[3] - 1.6) < 1e-12, "LensSpecification 4 racionais");
    e = mos_exif_find(&x, EXIF_IFD_EXIF, 0x927C);
    CHECK(e && e->kind == EXIF_BYTES && e->nbytes == 10 && e->bytes[9] == 0xA9, "MakerNote em bytes");
    e = mos_exif_find(&x, EXIF_IFD_EXIF, 0xA002);
    CHECK(e && e->num[0] == 4032, "PixelXDimension 4032");
    CHECK(!strcmp(mos_exif_name(EXIF_IFD_EXIF, 0x9003), "DateTimeOriginal") && !mos_exif_name(EXIF_IFD_EXIF, 0x1234), "nomes dos campos");

    /* TIFF solto big-endian */
    memset(buf, 0, sizeof buf); len = 0; le = 0;
    build_tiff(0);
    CHECK(mos_exif_parse(buf, len, &x) == 0, "TIFF big-endian");
    e = mos_exif_find(&x, EXIF_IFD_TIFF, 0x0112);
    CHECK(e && e->num[0] == 6, "big-endian: Orientation = 6");
    e = mos_exif_find(&x, EXIF_IFD_EXIF, 0x8827);
    CHECK(e && e->num[0] == 200, "big-endian: ISO 200");

    /* sem EXIF e truncado */
    uint8_t plain[] = {0xFF, 0xD8, 0xFF, 0xDB, 0x00, 0x04, 0, 0, 0xFF, 0xD9};
    CHECK(mos_exif_parse(plain, sizeof plain, &x) == -1, "JPEG sem APP1: -1");
    uint8_t trunc[] = {0xFF, 0xD8, 0xFF, 0xE1, 0x40, 0x00, 'E', 'x'};
    CHECK(mos_exif_parse(trunc, sizeof trunc, &x) == -1, "APP1 truncado: -1 sem ler fora");

    if (fails) { fprintf(stderr, "imageio_exif: %d falhas\n", fails); return 1; }
    printf("imageio_exif: ok (0 falhas)\n");
    return 0;
}
