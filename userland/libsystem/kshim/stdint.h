/* Cabeçalhos mínimos para o alvo freestanding (audiodec.c e o minimp3/ALAC). */
#pragma once
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef signed char int8_t;
typedef short int16_t;
typedef int int32_t;
typedef long long int64_t;
typedef unsigned long uintptr_t;
typedef long intptr_t;
#define UINT32_MAX 0xffffffffu
#define INT32_MAX 2147483647
