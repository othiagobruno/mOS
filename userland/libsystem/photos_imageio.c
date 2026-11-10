/*
 * O Photos chama mos_img_uti, mos_img_decode e mos_img_free.
 * A definição única está em grok_imageio.c, que inclui imageio_decode.c.
 * Incluir o .c aqui de novo duplicava esses três símbolos no libSystem.
 */
#include "../frameworks/ImageIO/imageio_codec.h"
