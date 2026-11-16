# Bibliotecas de mídia

O repositório é MIT. Só entra decodificador com licença compatível, e só o que de fato produz PCM.

| Formato | Biblioteca | Licença | Onde | Estado |
|---|---|---|---|---|
| MP3 | [minimp3](https://github.com/lieff/minimp3) (`minimp3.h`) | CC0 | `userland/third_party/minimp3/` | Funciona. Compilado com `MINIMP3_NO_SIMD` para o libSystem freestanding. |
| ALAC 16 bits | decodificador de referência da Apple (`ag_dec.c`, `dp_dec.c`, `matrix_dec.c`, `ALACBitUtilities.c`, `EndianPortable.c`) | Apache-2.0 (`userland/third_party/alac/APPLE_LICENSE.txt`) | `userland/third_party/alac/` + casca `alac_dec.c` | Funciona em CAF e M4A. 20/24/32 bits devolvem erro (`alac_open` = -2). O `.cpp` da Apple não entra: o libSystem só compila C. |
| AAC | [FDK AAC](https://github.com/mstorsjo/fdk-aac) 2.0.3, só o decodificador (libAACdec, libSBRdec, libSACdec, libDRCdec, libMpegTPDec, libPCMutils, libArithCoding, libFDK, libSYS) | Licença do Fraunhofer FDK AAC (`userland/third_party/fdk-aac/NOTICE`): redistribuição em fonte e binário permitida; **não dá licença de patente** | `userland/third_party/fdk-aac/` + `userland/host-audio/aac_glue.cpp` | AAC-LC, HE-AAC v1/v2, LD/ELD e xHE-AAC em ADTS, M4A/MP4/MOV e CAF. C++ freestanding (sem exceções, RTTI nem libc++). FAAD2 é GPL e foi rejeitado. |
| Opus | [libopus](https://opus-codec.org) 1.5.2, ponto fixo, sem DNN/DRED | BSD-3 (`userland/third_party/opus/COPYING`) | `userland/third_party/opus/` + `userland/host-audio/opus_glue.c` | Ogg Opus, MP4 (`Opus`/`dOps`) e CAF; multistream (até 255 canais, downmix para estéreo). |

Os codecs entram no libSystem por `tools/build-midia.sh` com `-fvisibility=hidden`: os nomes internos ficam locais e não aparecem como exportações do libSystem. As patentes de AAC (Via LA) não são resolvidas por nenhuma licença de código; isso fica registrado aqui como limitação legal do produto, não técnica.

O decodificador H.264 Baseline (CAVLC, intra 4x4 e 16x16, P-slice, deblocking) é o [h264bsd](https://github.com/oneam/h264bsd) (Apache-2.0, Android Open Source) em `userland/third_party/h264bsd/`, com a casca `h264bsd_glue.c`. O codificador em `h264sw.c` continua I_PCM: o residual CAVLC próprio não passa no ffmpeg. Não há x264, x265, libde265 nem libavcodec no fonte. HEVC é stub: VideoToolbox devolve -12906/-12908. Não se contorna FairPlay nem outro DRM.

A permissão de microfone e câmera não está nesta frente. O contrato é o símbolo fraco

```c
int cx_tcc_request(const char *service);
```

da frente de TCC. `1` concedido, `0` negado, ponteiro nulo (símbolo ausente) tratado como `-1` por `cx_media_tcc`. Serviços pedidos: `kTCCServiceMicrophone`, `kTCCServiceCamera`, `kTCCServiceScreenCapture`.
