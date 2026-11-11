/*
 * mOS — EXIF/TIFF de um JPEG (segmento APP1 "Exif\0\0") ou de um TIFF solto, sem runtime.
 * O ImageIO.m monta {TIFF}, {Exif} e Orientation com isto; tools/imageio_exif_test.c testa.
 */
#ifndef MOS_IMAGEIO_EXIF_H
#define MOS_IMAGEIO_EXIF_H
#include <stddef.h>
#include <stdint.h>

enum { EXIF_IFD_TIFF = 0, EXIF_IFD_EXIF = 1 };
enum { EXIF_NUM = 0, EXIF_STR = 1, EXIF_BYTES = 2 };

typedef struct {
    int ifd;                     /* EXIF_IFD_TIFF (IFD0) ou EXIF_IFD_EXIF */
    uint16_t tag;
    int kind;                    /* EXIF_NUM: num[0..nnum); EXIF_STR: str; EXIF_BYTES: bytes/nbytes */
    int nnum;
    double num[4];
    char str[96];
    const uint8_t *bytes;        /* aponta para dentro do arquivo */
    size_t nbytes;
} mos_exif_entry;

typedef struct {
    mos_exif_entry e[64];
    int n;
} mos_exif;

/* 0 = achou EXIF; -1 = não há. data pode ser um JPEG ou um TIFF. */
int mos_exif_parse(const uint8_t *data, size_t n, mos_exif *out);
const mos_exif_entry *mos_exif_find(const mos_exif *x, int ifd, uint16_t tag);
/* nome do campo no dicionário do ImageIO ("Make", "DateTimeOriginal"...) ou NULL se o tag não é exposto */
const char *mos_exif_name(int ifd, uint16_t tag);

#endif
