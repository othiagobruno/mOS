/* mOS — conversão dos quadros da câmera (YUY2, NV12 ou BGRA do JPEG decodificado) para o formato de pixel que o app
 * pediu no AVCaptureVideoDataOutput (kCVPixelBufferPixelFormatTypeKey). BT.601: as webcams UVC mandam faixa de vídeo
 * (16..235); '420f' sai em faixa cheia. Sem Objective-C; teste de host: tools/avcap_conv_test.c. Código original (MIT). */
#pragma once
#include <stddef.h>
#include <stdint.h>

enum { AVC_SRC_YUY2 = 1, AVC_SRC_NV12 = 2, AVC_SRC_BGRA = 3 };
#define AVC_FMT_BGRA 0x42475241u   /* 'BGRA' */
#define AVC_FMT_2VUY 0x32767579u   /* '2vuy' Cb Y0 Cr Y1 */
#define AVC_FMT_YUVS 0x79757673u   /* 'yuvs' Y0 Cb Y1 Cr (o YUY2 do UVC) */
#define AVC_FMT_420V 0x34323076u   /* '420v' biplanar, faixa de vídeo */
#define AVC_FMT_420F 0x34323066u   /* '420f' biplanar, faixa cheia */
#define AVC_FMT_Y420 0x79343230u   /* 'y420' três planos */

int avc_supported(uint32_t fmt);
/* planos do formato: devolve quantos (0 = não suportado) e preenche a largura em bytes (sem folga) e a altura de cada */
int avc_planes(uint32_t fmt, int w, int h, size_t *bpr, size_t *rows);
/* 0 ok; -1 formato não suportado ou origem curta. pl/bpr: planos de destino já alocados */
int avc_convert(int src, const uint8_t *s, size_t slen, int w, int h, uint32_t fmt, uint8_t *const *pl, const size_t *bpr);
/* espelha horizontalmente os planos já convertidos (isVideoMirrored da AVCaptureConnection) */
void avc_mirror(uint32_t fmt, int w, int h, uint8_t *const *pl, const size_t *bpr);

/* modo da câmera (do DEV_CAMERA_INFO) e escolha pelo sessionPreset */
typedef struct { uint32_t fourcc; int w, h; uint32_t min_interval; } avc_mode;
enum { AVC_PICK_SIZE = 0, AVC_PICK_HIGH = 1, AVC_PICK_PHOTO = 2 };
/* SIZE: o mais perto de w x h; HIGH: o maior até 1920x1080 com pelo menos 24 qps (senão o maior);
 * PHOTO: o maior. Empate: mais quadros por segundo, depois o descompactado. -1 se n == 0 */
int avc_pick_mode(const avc_mode *m, int n, int how, int w, int h);
