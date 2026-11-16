/* mOS — decodificação progressiva de MP3 (audiostream.c): estéreo 48 kHz i16, como o kit_load_wav */
#pragma once
#ifndef AS_HOST
#include "types.h"
#endif
typedef struct kit_stream kit_stream_t;
bool          kit_mp3_probe(const u8 *data, u64 len);              /* parece MP3 (camada III)? */
u32           kit_mp3_estimate_frames(const u8 *data, u64 len);    /* duração em quadros de 48 kHz (Xing/Info ou taxa) */
kit_stream_t *kit_stream_open_mem(const u8 *data, u64 len);        /* copia os bytes; NULL se não for MP3 */
bool          kit_stream_step(kit_stream_t *s, u32 budget_us);     /* decodifica por até budget_us; true = terminou */
i16          *kit_stream_pcm(kit_stream_t *s);                     /* pode mudar de endereço a cada passo */
u32           kit_stream_frames(kit_stream_t *s);                  /* quadros prontos até agora */
bool          kit_stream_done(kit_stream_t *s);
void          kit_stream_close(kit_stream_t *s);
