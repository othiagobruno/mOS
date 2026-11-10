/*
 * O decodificador do ImageIO (time Sistemas, userland/frameworks/ImageIO) entra no
 * libSystem só para o Photos medir PNG e JPEG. O fonte de lá não é editado.
 * Tradução separada: imageio_decode.c define typedef u8, que já existe em sys.h.
 */
/* grok_imageio.c já inclui imageio_decode.c neste mesmo libSystem.
 * Incluir de novo duplica mos_img_uti, mos_img_free e mos_img_decode. */

