# Bibliotecas de mídia

O repositório é MIT. Só entra decodificador com licença compatível, e só o que de fato produz PCM.

| Formato | Biblioteca | Licença | Onde | Estado |
|---|---|---|---|---|
| MP3 | [minimp3](https://github.com/lieff/minimp3) (`minimp3.h`) | CC0 | `userland/third_party/minimp3/` | Funciona. Compilado com `MINIMP3_NO_SIMD` para o libSystem freestanding. |
| ALAC 16 bits | decodificador de referência da Apple (`ag_dec.c`, `dp_dec.c`, `matrix_dec.c`, `ALACBitUtilities.c`, `EndianPortable.c`) | Apache-2.0 (`userland/third_party/alac/APPLE_LICENSE.txt`) | `userland/third_party/alac/` + casca `alac_dec.c` | Funciona em CAF e M4A. 20/24/32 bits devolvem erro (`alac_open` = -2). O `.cpp` da Apple não entra: o libSystem só compila C. |
| AAC | fdk-aac seria a escolha BSD | — | não vendida | Reconhecido (ADTS e `mp4a`), sem PCM. FAAD2 é GPL e foi rejeitado. |
| Opus | libopus seria a escolha BSD-3 | — | não vendida | Reconhecido (Ogg `OpusHead` e CAF `opus`), sem PCM. |

H.264 em software é I_PCM (Constrained Baseline, 4:2:0, 8 bits) em `userland/libsystem/h264sw.c`. As tabelas CAVLC copiadas do OpenH264 (BSD-2, Cisco) ficam no arquivo; o quadro que sai é I_PCM, porque o residual CAVLC ainda não passa num decodificador de referência. Não há x264, x265, libde265 nem FFmpeg no fonte. HEVC não entra: o encoder permissivo (Kvazaar) é C++ e só codifica, e não há decodificador HEVC permissivo pequeno o bastante para o libSystem. VideoToolbox devolve -12906/-12908 para HEVC. Não se contorna FairPlay nem outro DRM.

A permissão de microfone e câmera não está nesta frente. O contrato é o símbolo fraco

```c
int cx_tcc_request(const char *service);
```

da frente de TCC. `1` concedido, `0` negado, ponteiro nulo (símbolo ausente) tratado como `-1` por `cx_media_tcc`. Serviços pedidos: `kTCCServiceMicrophone`, `kTCCServiceCamera`, `kTCCServiceScreenCapture`.
