/*
 * mOS — ImageIO.framework, implementação única (o dylib separado saiu; o dyld cai no libSystem).
 * Codecs em iio_*.inc (sem CF; tools/imageio_host.c os testa com -DIIO_HOST). Abaixo deles,
 * CGImageSource / CGImageDestination sobre CGImage de verdade (coregraphics.c).
 * Estado e limites: docs/graficos-texto-imagens.md.
 */
#if defined(IIO_HOST) || defined(IIO_CF_HOST)
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#else
#include "sys.h"
#include "libc.h"
#endif
#include <stdint.h>
typedef int8_t i8;
typedef int16_t i16;
#define Z_SOLO 1
#include "../third_party/zlib/zlib.h"

#include "imageio_codec.h"
#include "iio_exif.h"
#include "iio_exif.inc"
#include "iio_pngjpeg.inc"
#include "iio_jpegenc.inc"
#include "iio_gif.inc"
#include "iio_bmp.inc"
#include "iio_tiff.inc"
#include "iio_core.inc"

#ifndef IIO_HOST
#include "imageio_cf.inc"      /* IIO_CF_HOST: tools/imageio_cf_host.c dá uma CF mínima */
#endif
