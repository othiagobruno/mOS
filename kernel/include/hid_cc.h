/* mOS — teclas de mídia HID: Consumer Control (página 0x0C) e System Control (0x01/0x80)
 * Parser sem dependências do kernel (testado no host por tools/hidcc_host.c). */
#pragma once
#ifdef MOS_HOST_TEST
#include <stdint.h>
#include <stdbool.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef int8_t i8; typedef int16_t i16; typedef int32_t i32;
#else
#include "types.h"
#endif

#define HIDCC_FIELDS 8
typedef struct {
    u8  rid;                 /* 0 = sem Report ID */
    u16 page;                /* 0x0C consumer, 0x01 system control */
    u16 off, size, count;    /* bits */
    bool array;              /* array de usos (valor = uso) ou variável (um bit por uso) */
    i32 lmin;
    u16 umin;                /* primeiro uso (array: uso = umin + valor - lmin; variável: uso = umin + i) */
    u16 usages[16];          /* lista explícita de usos (variável), se houver */
    u8  nusages;
} hidcc_field_t;

typedef struct {
    hidcc_field_t f[HIDCC_FIELDS];
    int n;
    bool ids;
} hidcc_t;

/* acha os campos de Consumer/System Control no report descriptor; devolve o número de campos */
int hidcc_parse(hidcc_t *c, const u8 *rd, int len);
/* lista os usos ativos (página << 16 | uso) de um relatório; devolve quantos (até max) */
int hidcc_active(const hidcc_t *c, const u8 *rep, u32 len, u32 *out, int max);
