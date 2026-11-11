/* Protocolo serializado da árvore de acessibilidade entre processos.
 * O transporte (quem leva estes bytes) não é este arquivo: é a interface em axtransport.h,
 * que fica sem vínculo até SYS-XPC-MACH (bc-1a126ada) ou KER-UNIX-IPC (bc-4bbc4f8a).
 * Formato little-endian, sem ponteiros. */
#pragma once

enum {
    AX_MAGIC = 0x31584D41u, /* 'AXM1' */
    AX_OP_COPY = 1,
    AX_OP_PARAM = 2,
    AX_OP_SET = 3,
    AX_OP_PRESS = 4,
    AX_OP_NOTE = 5,
    AX_OP_SUB = 6,
    AX_OP_REPLY = 7,
    AX_OP_SETTABLE = 8,     /* AXUIElementIsAttributeSettable: resposta AX_PAY_I32 */
    AX_OP_AT = 9            /* AXUIElementCopyElementAtPosition no app: px, py; resposta é um elemento (axremote.h) */
};

enum {
    AX_PAY_NONE = 0,
    AX_PAY_STR = 1,
    AX_PAY_I32 = 2,
    AX_PAY_BOX = 3,
    AX_PAY_KIDS = 4,
    AX_PAY_RANGE = 5
};

#define AX_NAME_MAX 48
#define AX_STR_MAX 96
#define AX_KIDS_MAX 12

typedef struct {
    int op, pid, kind, id, status, pay;
    char name[AX_NAME_MAX];
    char str[AX_STR_MAX];
    int i, x, y, w, h, loc, len, px, py;
    int nkids;
    int kid_kind[AX_KIDS_MAX];
    int kid_id[AX_KIDS_MAX];
} ax_msg;

/* bytes escritos, ou -1 se não couber. */
int ax_msg_encode(const ax_msg *m, void *dst, int cap);
/* 0 = ok, -1 = truncado ou mágico errado. */
int ax_msg_decode(const void *src, int n, ax_msg *m);
