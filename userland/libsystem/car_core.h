/* mOS — leitor de catálogos de recursos compilados (Assets.car) sem libc: o contêiner BOM, as árvores FACETKEYS e
 * RENDITIONS, o KEYFORMAT, as renditions CSI (bitmap MLEC em blocos KCBC, dados brutos RAWD, cor RLOC, ligação interna
 * KNLI para os atlas "ZZZZPackedAsset"). A descompressão vem de quem usa (LZFSE/LZVN no libSystem; zlib).
 * Formato conferido em catálogos reais (userland/tests-appkit/car_host.c). Código original do mOS. */
#ifndef CAR_CORE_H
#define CAR_CORE_H
#include <stdint.h>

/* descompressores: devolvem o número de bytes escritos (0 se falhou). kind: 2 zlib, 3 lzvn, 4 lzfse */
typedef long (*car_decompress_fn)(int kind, const uint8_t *in, long n, uint8_t *out, long cap);

typedef struct {
    const uint8_t *d; long n;
    const uint8_t *idx; uint32_t nblocks;
    uint32_t facets_tree, renditions_tree;
    uint16_t keyformat[32]; int nkeys;
} car_file;

int car_open(car_file *c, const uint8_t *data, long n);

/* uma rendition achada */
enum { CAR_NONE = 0, CAR_BITMAP = 1, CAR_DATA = 2, CAR_COLOR = 3, CAR_UNSUPPORTED = 4 };
typedef struct {
    int kind;
    int width, height, scale, layout;     /* scale em 100 = 1x */
    uint32_t pixel_format;                /* 'ARGB', 'GA8 ', 'RGB5', 'DATA'... */
    double color[4]; int ncolor;          /* CAR_COLOR: componentes (sRGB ou cinza) */
    const uint8_t *data; long data_len;   /* CAR_DATA: PNG/JPEG/PDF/SVG */
    char name[129];
} car_rendition;

/* atributos de busca (identificadores do KEYFORMAT: 7 aparência, 12 escala, 15 idiom...) */
typedef struct { uint16_t attr, value; } car_attr;

/* acha a rendition do nome com a preferência pedida: escala (100/200/300; 0 = a maior), aparência (0 clara, 1 escura) */
int car_find(const car_file *c, const char *name, int want_scale, int dark, car_rendition *out, uint16_t *key_out, int *nkey_out);
/* pixels ARGB não pré-multiplicados (w×h) da rendition de bitmap; out precisa de w*h*4 bytes; scratch de 2× o bruto */
int car_bitmap_argb(const car_file *c, const uint16_t *key, int nkey, car_decompress_fn dec, uint32_t *out, int w, int h);
/* rendition pelo nome interno (ex.: "icon_16x16.png") */
int car_find_rendition_named(const car_file *c, const char *rname, car_rendition *out, uint16_t *key_out, int *nkey_out);
/* espaço de trabalho para descomprimir (o atlas inteiro precisa caber nele) */
void car_set_scratch(uint8_t *p, long cap);
/* nomes do catálogo (FACETKEYS), um a um; devolve o tamanho do nome ou -1 no fim */
int car_name_at(const car_file *c, int i, char *out, int cap);
#endif
