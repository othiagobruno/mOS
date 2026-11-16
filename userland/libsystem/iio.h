/*
 * mOS ImageIO — camada de codecs, sem CoreFoundation (imageio.c põe a casca CF/CG por cima e
 * tools/imageio_host.c testa esta camada no host).
 *
 * Pixels: 0xAARRGGBB, alfa reto (não pré-multiplicado), linhas contíguas sem preenchimento.
 * Um arquivo animado (GIF, APNG) ou multipágina (TIFF) vira uma lista de quadros; quadros de
 * animação já saem compostos sobre a tela (disposição e mistura aplicadas), do tamanho da tela.
 */
#ifndef MOS_IIO_H
#define MOS_IIO_H
#include <stddef.h>
#include <stdint.h>

enum { IIO_RGB = 0, IIO_GRAY = 1, IIO_CMYK = 2 };

typedef struct {
    uint32_t *px;
    int w, h;
    int delay_ms;          /* GIF/APNG: atraso deste quadro em ms (como no arquivo, sem o piso de 100 ms) */
    int depth;             /* bits por componente no arquivo */
    int color;             /* IIO_RGB, IIO_GRAY, IIO_CMYK */
    int has_alpha;         /* o arquivo declara alfa (canal, tRNS ou cor transparente) */
    int indexed;           /* paleta */
    int orientation;       /* 1..8 (EXIF/TIFF); 1 sem o dado */
    double dpi_x, dpi_y;   /* 0 = o arquivo não diz */
    int compression;       /* TIFF: tag 259 */
    int photometric;       /* TIFF: tag 262 */
} iio_frame;

typedef struct {
    const char *uti;       /* "public.png", "public.jpeg", "com.compuserve.gif", "com.microsoft.bmp", "public.tiff"... */
    int w, h;              /* tela (o primeiro quadro em formatos sem tela) */
    int nframes;
    iio_frame *f;
    int loop_count;        /* GIF/APNG: 0 = para sempre; -1 = o arquivo não diz */
    int interlaced;        /* PNG Adam7, GIF entrelaçado, JPEG progressivo */
    double gamma;          /* PNG gAMA; 0 sem o chunk */
    int srgb_intent;       /* PNG sRGB: 0..3; -1 sem o chunk */
    int has_global_map;    /* GIF */
    uint8_t *icc;          /* perfil ICC embutido (PNG iCCP, JPEG APP2, TIFF 34675) */
    size_t icc_n;
} iio_image;

/* O tipo pelo cabeçalho, ou NULL. */
const char *iio_sniff(const uint8_t *d, size_t n);
/* 0 = ok. Decodifica todos os quadros. */
int iio_decode(const uint8_t *d, size_t n, iio_image *out);
void iio_image_free(iio_image *img);

typedef struct {
    const uint32_t *px;
    int w, h;
    int delay_ms;
    int orientation;       /* 0 = a de iio_enc_opts (TIFF grava uma por página) */
} iio_enc_frame;

typedef struct {
    double quality;        /* 0..1; < 0 = padrão do formato */
    int orientation;       /* 1..8 */
    double dpi_x, dpi_y;   /* 0 = não grava */
    int loop_count;        /* GIF/APNG */
    uint32_t background;   /* 0xAARRGGBB para achatar o alfa em formato sem alfa (JPEG, BMP de 24 bits) */
    int tiff_compression;  /* 1 nenhuma, 5 LZW (padrão), 8 Deflate, 32773 PackBits */
} iio_enc_opts;

/* 0 = ok. *out vem de malloc. Formatos de uma imagem só gravam o primeiro quadro. */
int iio_encode(const char *uti, const iio_enc_frame *fr, int n, const iio_enc_opts *o, uint8_t **out, size_t *on);
/* 1 se o tipo é gravável; *multi = aceita vários quadros. */
int iio_can_encode(const char *uti, int *multi);
/* Tipos que iio_decode lê e que iio_encode grava (listas terminadas em NULL). */
extern const char *const iio_read_types[];
extern const char *const iio_write_types[];

/* 1 se o arquivo termina com a marca de fim do formato (PNG IEND, JPEG EOI, GIF trailer). */
int iio_has_end(const uint8_t *d, size_t n);
/* Redução com filtro de caixa (média das amostras cobertas). */
uint32_t *iio_resize(const uint32_t *px, int w, int h, int nw, int nh);
/* Aplica a orientação EXIF (1..8): devolve um buffer novo e as dimensões resultantes. */
uint32_t *iio_orient(const uint32_t *px, int w, int h, int orientation, int *ow, int *oh);

#endif
