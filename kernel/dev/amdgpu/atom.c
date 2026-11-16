/* mOS — interpretador do bytecode ATOM do VBIOS da AMD. As tabelas de comando do VBIOS (SetPixelClock,
 * DIGxEncoderControl, UNIPHYTransmitterControl...) programam relógios, encoders e PHYs; as tabelas de dados
 * descrevem a placa (cristal, conectores, linhas DDC/HPD). Escrito a partir da especificação do formato que o
 * atom.c do amdgpu implementa (MIT): mesma decodificação de operandos e mesma tabela de opcodes.
 * Lógica pura: o acesso a registradores vem por callback. Testado em tools/amdgpu_atom_host.c.
 *
 * Diferenças de propósito: toda leitura do VBIOS é conferida contra o tamanho da imagem; o laço preso é
 * detectado por um orçamento de instruções (não por relógio); a recursão de CALL_TABLE tem limite; PCI e
 * SYSIO (que as APUs não usam) e o banco de registradores indiretos IIO são tratados, e um método IIO mal
 * formado só deixa o método de fora. */
#include "atom.h"

#define ARG_REG 0
#define ARG_PS  1
#define ARG_WS  2
#define ARG_FB  3
#define ARG_ID  4
#define ARG_IMM 5
#define ARG_PLL 6
#define ARG_MC  7

#define SRC_DWORD 0
#define SRC_WORD0 1
#define SRC_WORD8 2
#define SRC_WORD16 3
#define SRC_BYTE0 4
#define SRC_BYTE8 5
#define SRC_BYTE16 6
#define SRC_BYTE24 7

#define WS_QUOTIENT   0x40
#define WS_REMAINDER  0x41
#define WS_DATAPTR    0x42
#define WS_SHIFT      0x43
#define WS_OR_MASK    0x44
#define WS_AND_MASK   0x45
#define WS_FB_WINDOW  0x46
#define WS_ATTRIBUTES 0x47
#define WS_REGPTR     0x48

#define IO_MM    0
#define IO_PCI   1
#define IO_SYSIO 2
#define IO_IIO   0x80

enum { C_ABOVE, C_ABOVEOREQUAL, C_ALWAYS, C_BELOW, C_BELOWOREQUAL, C_EQUAL, C_NOTEQUAL };
#define OP_CNT      127
#define OP_EOT      91
#define CASE_MAGIC  0x63
#define CASE_END    0x5A5A
#define MAX_DEPTH   32
#define STEP_BUDGET 20000000u        /* instruções por execução: acima disso a tabela está presa num laço */

static const u32 arg_mask[8] = {0xFFFFFFFFu, 0xFFFF, 0xFFFF00, 0xFFFF0000u, 0xFF, 0xFF00, 0xFF0000, 0xFF000000u};
static const int arg_shift[8] = {0, 0, 8, 16, 0, 8, 16, 24};
static const int dst_to_src[8][4] = {
    {0, 0, 0, 0}, {1, 2, 3, 0}, {1, 2, 3, 0}, {1, 2, 3, 0}, {4, 5, 6, 7}, {4, 5, 6, 7}, {4, 5, 6, 7}, {4, 5, 6, 7},
};
static const int def_dst[8] = {0, 0, 1, 2, 0, 1, 2, 3};

/* cada ponto de erro tem um código (o último fica em a->last_error, para o log) */
#define ERR(a, code) ((a)->errors++, (a)->last_error = (code))

typedef struct {
    amd_atom_t *a;
    u32 *ps, *ws;
    int ps_size, ws_size, ps_shift;
    u32 start;
    bool abort;
} ex_t;

/* ------------------------------------------------------------ leitura segura da imagem */
static u8 B8(amd_atom_t *a, u32 o) { if (o >= a->size) { a->bad_reads++; return 0; } return a->bios[o]; }
static u16 B16(amd_atom_t *a, u32 o) { return (u16)(B8(a, o) | B8(a, o + 1) << 8); }
static u32 B32(amd_atom_t *a, u32 o) { return (u32)B16(a, o) | (u32)B16(a, o + 2) << 16; }

/* ------------------------------------------------------------ IIO: métodos de acesso indireto */
static u32 iio_run(amd_atom_t *a, u32 base, u32 index, u32 data) {
    u32 temp = 0xCDCDCDCDu;
    while (base + 3 < a->size) {
        u8 op = B8(a, base);
        u32 bits = B8(a, base + 1), m = bits >= 32 ? 0xFFFFFFFFu : (bits ? 0xFFFFFFFFu >> (32 - bits) : 0);
        switch (op) {
        case 0: base++; break;                                                  /* NOP */
        case 2: temp = a->reg_read(a->card, B16(a, base + 1)); base += 3; break;   /* READ */
        case 3: a->reg_write(a->card, B16(a, base + 1), temp); base += 3; break;   /* WRITE */
        case 4: temp &= ~(m << B8(a, base + 2)); base += 3; break;                 /* CLEAR */
        case 5: temp |= m << B8(a, base + 2); base += 3; break;                    /* SET */
        case 6: case 7: case 8: {                                                  /* MOVE_INDEX/ATTR/DATA */
            u32 v = op == 6 ? index : op == 7 ? a->io_attr : data;
            u32 sh = B8(a, base + 3);
            temp &= ~(m << sh);
            temp |= ((v >> B8(a, base + 2)) & m) << sh;
            base += 4;
            break;
        }
        case 9: return temp;                                                      /* END */
        default: ERR(a, 1); return 0;
        }
    }
    ERR(a, 2);
    return 0;
}

static void index_iio(amd_atom_t *a, u32 base) {
    static const u8 len[] = {1, 2, 3, 3, 3, 3, 4, 4, 4, 3};
    while (base + 1 < a->size && B8(a, base) == 1) {                             /* START */
        u8 idx = B8(a, base + 1);
        u32 start = base + 2;
        base += 2;
        while (base < a->size && B8(a, base) != 9) {
            u8 op = B8(a, base);
            if (op >= sizeof len) return;                                         /* não dá para continuar */
            base += len[op];
        }
        if (base >= a->size) return;
        a->iio[idx] = (u16)start;
        base += 3;
    }
}

/* ------------------------------------------------------------ operandos */
static u32 reg_rd(ex_t *x, u32 idx) {
    amd_atom_t *a = x->a;
    idx += a->reg_block;
    if (a->io_mode == IO_MM) return a->reg_read(a->card, idx);
    if (a->io_mode & IO_IIO) {
        u16 m = a->iio[a->io_mode & 0x7F];
        if (m) return iio_run(a, m, idx, 0);
    }
    ERR(a, 3);                                                                  /* PCI/SYSIO não existem na APU */
    return 0;
}
static void reg_wr(ex_t *x, u32 idx, u32 v) {
    amd_atom_t *a = x->a;
    idx += a->reg_block;
    if (a->io_mode == IO_MM) { a->reg_write(a->card, idx, idx == 0 ? v << 2 : v); return; }
    if (a->io_mode & IO_IIO) {
        u16 m = a->iio[a->io_mode & 0x7F];
        if (m) { iio_run(a, m, idx, v); return; }
    }
    ERR(a, 4);
}

static u32 get_src_int(ex_t *x, u8 attr, u32 *ptr, u32 *saved) {
    amd_atom_t *a = x->a;
    u32 arg = attr & 7, align = (attr >> 3) & 7, idx, val = 0xCDCDCDCDu;
    switch (arg) {
    case ARG_REG: idx = B16(a, *ptr); *ptr += 2; val = reg_rd(x, idx); break;
    case ARG_PS:
        idx = B8(a, (*ptr)++);
        if ((int)idx < x->ps_size) val = x->ps[idx]; else ERR(a, 5);
        break;
    case ARG_WS:
        idx = B8(a, (*ptr)++);
        switch (idx) {
        case WS_QUOTIENT: val = a->divmul[0]; break;
        case WS_REMAINDER: val = a->divmul[1]; break;
        case WS_DATAPTR: val = a->data_block; break;
        case WS_SHIFT: val = a->shift; break;
        case WS_OR_MASK: val = 1u << (a->shift & 31); break;
        case WS_AND_MASK: val = ~(1u << (a->shift & 31)); break;
        case WS_FB_WINDOW: val = a->fb_base; break;
        case WS_ATTRIBUTES: val = a->io_attr; break;
        case WS_REGPTR: val = a->reg_block; break;
        default: if ((int)idx < x->ws_size) val = x->ws[idx]; else ERR(a, 6);
        }
        break;
    case ARG_ID: idx = B16(a, *ptr); *ptr += 2; val = B32(a, idx + a->data_block); break;
    case ARG_FB:
        idx = B8(a, (*ptr)++);
        if (a->fb_base + idx * 4 < sizeof a->scratch) val = a->scratch[a->fb_base / 4 + idx];
        else { ERR(a, 7); val = 0; }
        break;
    case ARG_IMM:
        if (align == SRC_DWORD) { val = B32(a, *ptr); *ptr += 4; }
        else if (align <= SRC_WORD16) { val = B16(a, *ptr); *ptr += 2; }
        else { val = B8(a, *ptr); *ptr += 1; }
        return val;                                                              /* imediato: sem máscara */
    case ARG_PLL: idx = B8(a, (*ptr)++); val = a->pll_read ? a->pll_read(a->card, idx) : 0; break;
    case ARG_MC: idx = B8(a, (*ptr)++); val = a->mc_read ? a->mc_read(a->card, idx) : 0; break;
    }
    if (saved) *saved = val;
    return (val & arg_mask[align]) >> arg_shift[align];
}
static void skip_src_int(ex_t *x, u8 attr, u32 *ptr) {
    (void)x;
    u32 align = (attr >> 3) & 7, arg = attr & 7;
    if (arg == ARG_REG || arg == ARG_ID) *ptr += 2;
    else if (arg == ARG_IMM) *ptr += align == SRC_DWORD ? 4 : align <= SRC_WORD16 ? 2 : 1;
    else *ptr += 1;
}
static u32 get_src(ex_t *x, u8 attr, u32 *ptr) { return get_src_int(x, attr, ptr, NULL); }
static u32 get_src_direct(ex_t *x, u8 align, u32 *ptr) {
    amd_atom_t *a = x->a;
    u32 v;
    if (align == SRC_DWORD) { v = B32(a, *ptr); *ptr += 4; }
    else if (align <= SRC_WORD16) { v = B16(a, *ptr); *ptr += 2; }
    else { v = B8(a, *ptr); *ptr += 1; }
    return v;
}
static u8 dst_attr(int arg, u8 attr) { return (u8)(arg | dst_to_src[(attr >> 3) & 7][(attr >> 6) & 3] << 3); }
static u32 get_dst(ex_t *x, int arg, u8 attr, u32 *ptr, u32 *saved) { return get_src_int(x, dst_attr(arg, attr), ptr, saved); }
static void skip_dst(ex_t *x, int arg, u8 attr, u32 *ptr) { skip_src_int(x, dst_attr(arg, attr), ptr); }

static void put_dst(ex_t *x, int arg, u8 attr, u32 *ptr, u32 val, u32 saved) {
    amd_atom_t *a = x->a;
    u32 align = (u32)dst_to_src[(attr >> 3) & 7][(attr >> 6) & 3], idx;
    val = (val << arg_shift[align]) & arg_mask[align];
    val |= saved & ~arg_mask[align];
    switch (arg) {
    case ARG_REG: idx = B16(a, *ptr); *ptr += 2; reg_wr(x, idx, val); break;
    case ARG_PS:
        idx = B8(a, (*ptr)++);
        if ((int)idx < x->ps_size) x->ps[idx] = val; else ERR(a, 8);
        break;
    case ARG_WS:
        idx = B8(a, (*ptr)++);
        switch (idx) {
        case WS_QUOTIENT: a->divmul[0] = val; break;
        case WS_REMAINDER: a->divmul[1] = val; break;
        case WS_DATAPTR: a->data_block = val; break;
        case WS_SHIFT: a->shift = val; break;
        case WS_OR_MASK: case WS_AND_MASK: break;
        case WS_FB_WINDOW: a->fb_base = val; break;
        case WS_ATTRIBUTES: a->io_attr = val; break;
        case WS_REGPTR: a->reg_block = val; break;
        default: if ((int)idx < x->ws_size) x->ws[idx] = val; else ERR(a, 9);
        }
        break;
    case ARG_FB:
        idx = B8(a, (*ptr)++);
        if (a->fb_base + idx * 4 < sizeof a->scratch) a->scratch[a->fb_base / 4 + idx] = val; else ERR(a, 10);
        break;
    case ARG_PLL: idx = B8(a, (*ptr)++); if (a->pll_write) a->pll_write(a->card, idx, val); break;
    case ARG_MC: idx = B8(a, (*ptr)++); if (a->mc_write) a->mc_write(a->card, idx, val); break;
    default: ERR(a, 11);
    }
}

/* ------------------------------------------------------------ opcodes */
typedef enum { K_NONE, K_MOVE, K_AND, K_OR, K_SHL1, K_SHR1, K_MUL, K_DIV, K_ADD, K_SUB, K_SETPORT, K_SETREGBLOCK,
               K_SETFBBASE, K_COMPARE, K_SWITCH, K_JUMP, K_TEST, K_DELAY, K_CALL, K_REPEAT, K_CLEAR, K_NOP, K_EOT,
               K_MASK, K_POSTCARD, K_BEEP, K_SAVEREG, K_RESTOREREG, K_SETDATABLOCK, K_XOR, K_SHL, K_SHR, K_DEBUG,
               K_PROCESSDS, K_MUL32, K_DIV32 } kind_t;
typedef struct { u8 kind, arg; } opdef_t;

static opdef_t optab[OP_CNT];
static void build_optab(void) {
    static bool done;
    if (done) return;
    int o = 1;
    const kind_t six[] = {K_MOVE, K_AND, K_OR, K_SHL1, K_SHR1, K_MUL, K_DIV, K_ADD, K_SUB};
    for (unsigned k = 0; k < 9; k++) for (int g = 0; g < 6; g++) optab[o++] = (opdef_t){(u8)six[k], (u8)(g == 3 ? ARG_FB : g == 4 ? ARG_PLL : g == 5 ? ARG_MC : g)};
    for (int g = 0; g < 3; g++) optab[o++] = (opdef_t){K_SETPORT, (u8)g};
    optab[o++] = (opdef_t){K_SETREGBLOCK, 0};
    optab[o++] = (opdef_t){K_SETFBBASE, 0};
    for (int g = 0; g < 6; g++) optab[o++] = (opdef_t){K_COMPARE, (u8)(g == 3 ? ARG_FB : g == 4 ? ARG_PLL : g == 5 ? ARG_MC : g)};
    optab[o++] = (opdef_t){K_SWITCH, 0};
    const u8 conds[] = {C_ALWAYS, C_EQUAL, C_BELOW, C_ABOVE, C_BELOWOREQUAL, C_ABOVEOREQUAL, C_NOTEQUAL};
    for (int c = 0; c < 7; c++) optab[o++] = (opdef_t){K_JUMP, conds[c]};
    for (int g = 0; g < 6; g++) optab[o++] = (opdef_t){K_TEST, (u8)(g == 3 ? ARG_FB : g == 4 ? ARG_PLL : g == 5 ? ARG_MC : g)};
    optab[o++] = (opdef_t){K_DELAY, 1};                 /* milissegundos */
    optab[o++] = (opdef_t){K_DELAY, 0};                 /* microssegundos */
    optab[o++] = (opdef_t){K_CALL, 0};
    optab[o++] = (opdef_t){K_REPEAT, 0};
    for (int g = 0; g < 6; g++) optab[o++] = (opdef_t){K_CLEAR, (u8)(g == 3 ? ARG_FB : g == 4 ? ARG_PLL : g == 5 ? ARG_MC : g)};
    optab[o++] = (opdef_t){K_NOP, 0};
    optab[o++] = (opdef_t){K_EOT, 0};                   /* 91 */
    for (int g = 0; g < 6; g++) optab[o++] = (opdef_t){K_MASK, (u8)(g == 3 ? ARG_FB : g == 4 ? ARG_PLL : g == 5 ? ARG_MC : g)};
    optab[o++] = (opdef_t){K_POSTCARD, 0};
    optab[o++] = (opdef_t){K_BEEP, 0};
    optab[o++] = (opdef_t){K_SAVEREG, 0};
    optab[o++] = (opdef_t){K_RESTOREREG, 0};
    optab[o++] = (opdef_t){K_SETDATABLOCK, 0};
    for (int g = 0; g < 6; g++) optab[o++] = (opdef_t){K_XOR, (u8)(g == 3 ? ARG_FB : g == 4 ? ARG_PLL : g == 5 ? ARG_MC : g)};
    for (int g = 0; g < 6; g++) optab[o++] = (opdef_t){K_SHL, (u8)(g == 3 ? ARG_FB : g == 4 ? ARG_PLL : g == 5 ? ARG_MC : g)};
    for (int g = 0; g < 6; g++) optab[o++] = (opdef_t){K_SHR, (u8)(g == 3 ? ARG_FB : g == 4 ? ARG_PLL : g == 5 ? ARG_MC : g)};
    optab[o++] = (opdef_t){K_DEBUG, 0};
    optab[o++] = (opdef_t){K_PROCESSDS, 0};
    optab[o++] = (opdef_t){K_MUL32, ARG_PS};
    optab[o++] = (opdef_t){K_MUL32, ARG_WS};
    optab[o++] = (opdef_t){K_DIV32, ARG_PS};
    optab[o++] = (opdef_t){K_DIV32, ARG_WS};
    done = true;
}

static int exec_locked(amd_atom_t *a, int index, u32 *params, int params_size);

static void step(ex_t *x, u8 op, u32 *ptr) {
    amd_atom_t *a = x->a;
    opdef_t d = optab[op];
    int arg = d.arg;
    u8 attr;
    u32 dst, src, saved, dptr;
    switch ((kind_t)d.kind) {
    case K_MOVE:
        attr = B8(a, (*ptr)++); dptr = *ptr;
        if (((attr >> 3) & 7) != SRC_DWORD) get_dst(x, arg, attr, ptr, &saved);
        else { skip_dst(x, arg, attr, ptr); saved = 0xCDCDCDCDu; }
        src = get_src(x, attr, ptr);
        put_dst(x, arg, attr, &dptr, src, saved);
        break;
    case K_AND: case K_OR: case K_XOR: case K_ADD: case K_SUB:
        attr = B8(a, (*ptr)++); dptr = *ptr;
        dst = get_dst(x, arg, attr, ptr, &saved);
        src = get_src(x, attr, ptr);
        dst = d.kind == K_AND ? dst & src : d.kind == K_OR ? dst | src : d.kind == K_XOR ? dst ^ src : d.kind == K_ADD ? dst + src : dst - src;
        put_dst(x, arg, attr, &dptr, dst, saved);
        break;
    case K_SHL1: case K_SHR1: {
        attr = B8(a, (*ptr)++); dptr = *ptr;
        attr &= 0x38;
        attr |= (u8)(def_dst[attr >> 3] << 6);
        dst = get_dst(x, arg, attr, ptr, &saved);
        u32 sh = get_src_direct(x, SRC_BYTE0, ptr) & 0xFF;
        dst = sh >= 32 ? 0 : d.kind == K_SHL1 ? dst << sh : dst >> sh;
        put_dst(x, arg, attr, &dptr, dst, saved);
        break;
    }
    case K_SHL: case K_SHR: {
        attr = B8(a, (*ptr)++); dptr = *ptr;
        u32 al = (u32)dst_to_src[(attr >> 3) & 7][(attr >> 6) & 3];
        get_dst(x, arg, attr, ptr, &saved);
        dst = saved;
        u32 sh = get_src(x, attr, ptr) & 0xFF;
        dst = sh >= 32 ? 0 : d.kind == K_SHL ? dst << sh : dst >> sh;
        dst = (dst & arg_mask[al]) >> arg_shift[al];
        put_dst(x, arg, attr, &dptr, dst, saved);
        break;
    }
    case K_MUL: case K_MUL32:
        attr = B8(a, (*ptr)++);
        dst = get_dst(x, arg, attr, ptr, NULL);
        src = get_src(x, attr, ptr);
        if (d.kind == K_MUL) a->divmul[0] = dst * src;
        else { u64 v = (u64)dst * src; a->divmul[0] = (u32)v; a->divmul[1] = (u32)(v >> 32); }
        break;
    case K_DIV: case K_DIV32:
        attr = B8(a, (*ptr)++);
        dst = get_dst(x, arg, attr, ptr, NULL);
        src = get_src(x, attr, ptr);
        if (!src) { a->divmul[0] = a->divmul[1] = 0; break; }
        if (d.kind == K_DIV) { a->divmul[0] = dst / src; a->divmul[1] = dst % src; }
        else { u64 v = ((u64)a->divmul[1] << 32 | dst) / src; a->divmul[0] = (u32)v; a->divmul[1] = (u32)(v >> 32); }
        break;
    case K_SETPORT:
        if (arg == 0) { u16 port = B16(a, *ptr); a->io_mode = port ? (u32)(IO_IIO | (port & 0x7F)) : IO_MM; *ptr += 2; }
        else { a->io_mode = arg == 1 ? IO_PCI : IO_SYSIO; *ptr += 1; }
        break;
    case K_SETREGBLOCK: a->reg_block = B16(a, *ptr); *ptr += 2; break;
    case K_SETFBBASE: attr = B8(a, (*ptr)++); a->fb_base = get_src(x, attr, ptr); break;
    case K_COMPARE:
        attr = B8(a, (*ptr)++);
        dst = get_dst(x, arg, attr, ptr, NULL);
        src = get_src(x, attr, ptr);
        a->cs_equal = dst == src; a->cs_above = dst > src;
        break;
    case K_TEST:
        attr = B8(a, (*ptr)++);
        dst = get_dst(x, arg, attr, ptr, NULL);
        src = get_src(x, attr, ptr);
        a->cs_equal = (dst & src) == 0;
        break;
    case K_SWITCH: {
        attr = B8(a, (*ptr)++);
        src = get_src(x, attr, ptr);
        while (B16(a, *ptr) != CASE_END) {
            if (*ptr >= a->size || B8(a, *ptr) != CASE_MAGIC) { ERR(a, 12); if (*ptr >= a->size) x->abort = true; return; }   /* como o amdgpu: registra e segue */
            (*ptr)++;
            u32 val = get_src(x, (u8)((attr & 0x38) | ARG_IMM), ptr);
            u16 target = B16(a, *ptr);
            if (val == src) { *ptr = x->start + target; return; }
            *ptr += 2;
        }
        *ptr += 2;
        break;
    }
    case K_JUMP: {
        u16 target = B16(a, *ptr);
        *ptr += 2;
        bool go = arg == C_ALWAYS || (arg == C_EQUAL && a->cs_equal) || (arg == C_NOTEQUAL && !a->cs_equal) ||
                  (arg == C_ABOVE && a->cs_above) || (arg == C_ABOVEOREQUAL && (a->cs_above || a->cs_equal)) ||
                  (arg == C_BELOW && !(a->cs_above || a->cs_equal)) || (arg == C_BELOWOREQUAL && !a->cs_above);
        if (go) *ptr = x->start + target;
        break;
    }
    case K_DELAY: {
        u32 n = B8(a, (*ptr)++);
        if (a->delay_us) a->delay_us(a->card, arg ? n * 1000 : n);
        break;
    }
    case K_CALL: {
        u32 idx = B8(a, (*ptr)++);
        if (B16(a, a->cmd_table + 4 + 2 * idx) && exec_locked(a, (int)idx, x->ps + x->ps_shift, x->ps_size - x->ps_shift)) x->abort = true;
        break;
    }
    case K_CLEAR:
        attr = B8(a, (*ptr)++); dptr = *ptr;
        attr &= 0x38;
        attr |= (u8)(def_dst[attr >> 3] << 6);
        get_dst(x, arg, attr, ptr, &saved);
        put_dst(x, arg, attr, &dptr, 0, saved);
        break;
    case K_MASK: {
        attr = B8(a, (*ptr)++); dptr = *ptr;
        dst = get_dst(x, arg, attr, ptr, &saved);
        u32 mask = get_src_direct(x, (attr >> 3) & 7, ptr);
        src = get_src(x, attr, ptr);
        put_dst(x, arg, attr, &dptr, (dst & mask) | src, saved);
        break;
    }
    case K_SETDATABLOCK: {
        u32 idx = B8(a, (*ptr)++);
        a->data_block = !idx ? 0 : idx == 255 ? x->start : B16(a, a->data_table + 4 + 2 * idx);
        break;
    }
    case K_POSTCARD: case K_DEBUG: (*ptr)++; break;
    case K_PROCESSDS: { u16 n = B16(a, *ptr); *ptr += 2u + n; break; }
    case K_REPEAT: case K_SAVEREG: case K_RESTOREREG:
        ERR(a, 13);                                 /* sem formato definido: o amdgpu também só registra e segue */
        break;
    case K_NOP: case K_BEEP: case K_EOT: case K_NONE: break;
    }
}

static int exec_locked(amd_atom_t *a, int index, u32 *params, int params_size) {
    if (index < 0 || index > 255) return -1;
    u32 base = B16(a, a->cmd_table + 4 + 2 * (u32)index);
    if (!base || base >= a->size) return -1;
    if (a->depth >= MAX_DEPTH) { ERR(a, 14); return -2; }
    a->depth++;
    ex_t x;
    u32 ws_buf[256];
    x.a = a;
    x.ws_size = B8(a, base + 4);
    x.ps_shift = (B8(a, base + 5) & 0x7F) / 4;
    x.start = base;
    x.ps = params;
    x.ps_size = params_size;
    x.abort = false;
    memset(ws_buf, 0, sizeof(u32) * (usize)x.ws_size);
    x.ws = ws_buf;
    u32 ptr = base + 6;
    int ret = 0;
    for (;;) {
        if (++a->steps > STEP_BUDGET) { ERR(a, 15); ret = -3; break; }   /* laço preso */
        if (x.abort) { ret = -4; break; }
        if (ptr >= a->size) { ERR(a, 16); ret = -5; break; }
        u8 op = B8(a, ptr++);
        if (op == 0 || op >= OP_CNT) break;
        step(&x, op, &ptr);
        if (op == OP_EOT) break;
    }
    if (!ret && x.abort) ret = -4;
    a->depth--;
    return ret;
}

int amd_atom_execute(amd_atom_t *a, int index, u32 *params, int params_size) {
    if (!a || !a->bios) return -1;
    a->data_block = 0; a->reg_block = 0; a->fb_base = 0; a->io_mode = IO_MM;
    a->divmul[0] = a->divmul[1] = 0;
    a->steps = 0;
    return exec_locked(a, index, params, params_size);
}

/* ------------------------------------------------------------ cabeçalhos */
int amd_atom_init(amd_atom_t *a, const u8 *bios, u32 size) {
    build_optab();
    a->bios = NULL;
    if (!bios || size < 0x100 || (bios[0] | bios[1] << 8) != 0xAA55) return -1;
    a->bios = bios; a->size = size;
    if (memcmp(bios + 0x30, " 761295520", 10)) { a->bios = NULL; return -2; }
    u32 rom = B16(a, 0x48);
    if (rom + 8 > size || memcmp(bios + rom + 4, "ATOM", 4)) { a->bios = NULL; return -3; }
    a->rom_header = rom;
    a->cmd_table = B16(a, rom + 0x1E);
    a->data_table = B16(a, rom + 0x20);
    if (!a->cmd_table || !a->data_table || a->cmd_table >= size || a->data_table >= size) { a->bios = NULL; return -4; }
    memset(a->iio, 0, sizeof a->iio);
    u32 iio = B16(a, a->data_table + 0x32);
    if (iio) index_iio(a, iio + 4);
    return 0;
}

bool amd_atom_data_header(amd_atom_t *a, int index, u16 *size, u8 *frev, u8 *crev, u16 *start) {
    u32 off = B16(a, a->data_table + 4 + 2 * (u32)index);
    if (!off || off + 4 > a->size) return false;
    if (size) *size = B16(a, off);
    if (frev) *frev = B8(a, off + 2);
    if (crev) *crev = B8(a, off + 3);
    if (start) *start = (u16)off;
    return true;
}

bool amd_atom_cmd_header(amd_atom_t *a, int index, u8 *frev, u8 *crev) {
    u32 off = B16(a, a->cmd_table + 4 + 2 * (u32)index);
    if (!off || off + 4 > a->size) return false;
    if (frev) *frev = B8(a, off + 2);
    if (crev) *crev = B8(a, off + 3);
    return true;
}

u8 amd_atom_u8(amd_atom_t *a, u32 o) { return B8(a, o); }
u16 amd_atom_u16(amd_atom_t *a, u32 o) { return B16(a, o); }
u32 amd_atom_u32(amd_atom_t *a, u32 o) { return B32(a, o); }
