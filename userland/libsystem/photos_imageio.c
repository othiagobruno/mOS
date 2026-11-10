/*
 * O decodificador do ImageIO (time Sistemas, userland/frameworks/ImageIO) entra no
 * libSystem só para o Photos medir PNG e JPEG. O fonte de lá não é editado.
 * Tradução separada: imageio_decode.c define typedef u8, que já existe em sys.h.
 */
/* O zig freestanding não declara cosf. O corpo está em libc.c. Não alteramos o ImageIO. */
float cosf(float x);
#include "../frameworks/ImageIO/imageio_decode.c"
