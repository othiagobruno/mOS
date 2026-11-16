/* mOS — teste de host do interpretador ATOM (kernel/dev/amdgpu/atom.c) com um VBIOS montado à mão.
 * Rodar da raiz:
 *   gcc -O2 -Wall -Wextra -DMOS_HOST_TEST -I kernel/dev/amdgpu tools/amdgpu_atom_host.c kernel/dev/amdgpu/atom.c -o /tmp/atom && /tmp/atom */
#include "atom.h"

static int pass, total;
#define CHECK(c, ...) do { total++; if (c) pass++; else { printf("FALHOU %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static u32 regs[0x10000];
static u64 delays_us;
static u32 rr(void *c, u32 r) { (void)c; return regs[r & 0xFFFF]; }
static void rw(void *c, u32 r, u32 v) { (void)c; regs[r & 0xFFFF] = v; }
static void dl(void *c, u32 us) { (void)c; delays_us += us; }

/* imagem: 0x0000 cabeçalho PCI/ATOM, 0x0100 cabeçalho ROM, 0x0200 lista de comandos, 0x0300 lista de dados,
 * 0x0400 dados, 0x0800+ tabelas de comando */
static u8 bios[0x4000];
static u32 cpos;
static void w8(u32 o, u32 v) { bios[o] = (u8)v; }
static void w16(u32 o, u32 v) { w8(o, v); w8(o + 1, v >> 8); }
static void w32(u32 o, u32 v) { w16(o, v); w16(o + 2, v >> 16); }
static u32 E(u32 v) { w8(cpos, v); return cpos++; }
static void E16(u32 v) { E(v); E(v >> 8); }
static void E32(u32 v) { E16(v); E16(v >> 16); }

static u32 tstart;
/* começa a tabela de comando idx com ws dwords de rascunho */
static void table(int idx, u32 ws) {
    tstart = cpos;
    w16(0x200 + 4 + 2 * idx, cpos);
    E16(0); E(1); E(1); E(ws); E(0);               /* tamanho (preenchido no fim), frev, crev, ws, ps */
}
static void endtable(void) { E(91); w16(tstart, cpos - tstart); }   /* EOT */
static u32 rel(void) { return cpos - tstart; }

/* attr: src arg | src align << 3 | dst align << 6 */
#define A(arg, sal, dal) ((arg) | (sal) << 3 | (dal) << 6)
enum { REG = 0, PS = 1, WS = 2, FB = 3, ID = 4, IMM = 5 };

static void build(void) {
    memset(bios, 0, sizeof bios);
    w16(0, 0xAA55);
    memcpy(bios + 0x30, " 761295520", 10);
    w16(0x48, 0x100);
    memcpy(bios + 0x104, "ATOM", 4);
    w16(0x100 + 0x1E, 0x200);
    w16(0x100 + 0x20, 0x300);
    /* dados: tabela 4 (FirmwareInfo) com cabeçalho e um dword em +4; IIO (0x32) com um método */
    w16(0x300 + 4 + 2 * 4, 0x400);
    w16(0x400, 16); w8(0x402, 3); w8(0x403, 1); w32(0x404, 0x12345678);
    w16(0x300 + 0x32, 0x480);
    /* IIO em 0x484: START 5; READ reg 0x20; MOVE_DATA 8 bits de data>>0 para bit 8; WRITE reg 0x21; END */
    u32 i = 0x484;
    w8(i++, 1); w8(i++, 5);
    w8(i++, 2); w16(i, 0x20); i += 2;
    w8(i++, 8); w8(i++, 8); w8(i++, 0); w8(i++, 8);
    w8(i++, 3); w16(i, 0x21); i += 2;
    w8(i++, 9); w8(i++, 0); w8(i++, 0);

    cpos = 0x800;
    /* tabela 1: soma 1..10 num laço (ws0), grava em REG 0x10; PS0 = REG 0x11 + 5; switch em PS1 */
    table(1, 2);
    E(1 + 2); E(A(IMM, 0, 0)); E(0); E32(0);                 /* MOVE ws0 = 0 */
    E(1 + 2); E(A(IMM, 0, 0)); E(1); E32(1);                 /* MOVE ws1 = 1 */
    u32 loop = rel();
    E(43 + 2); E(A(WS, 0, 0)); E(0); E(1);                   /* ADD ws0 += ws1 */
    E(43 + 2); E(A(IMM, 0, 0)); E(1); E32(1);                /* ADD ws1 += 1 */
    E(60 + 2); E(A(IMM, 0, 0)); E(1); E32(11);               /* COMPARE ws1, 11 */
    E(69); E16(loop);                                        /* JUMP_BELOW loop */
    E(1); E(A(WS, 0, 0)); E16(0x10); E(0);                   /* MOVE reg 0x10 = ws0 */
    E(1 + 1); E(A(REG, 0, 0)); E(0); E16(0x11);              /* MOVE ps0 = reg 0x11 */
    E(43 + 1); E(A(IMM, 0, 0)); E(0); E32(5);                /* ADD ps0 += 5 */
    E(66); E(A(PS, 4 << 0, 0) | 0); E(1);                    /* SWITCH ps1 (byte0) */
    E(0x63); E(7); u32 t7 = cpos; E16(0);
    E(0x63); E(9); u32 t9 = cpos; E16(0);
    E16(0x5A5A);
    E(1); E(A(IMM, 0, 0)); E16(0x12); E32(0xDEFA);           /* default: reg 0x12 = 0xDEFA */
    E(67); u32 tend = cpos; E16(0);                          /* JUMP fim */
    w16(t7, rel());
    E(1); E(A(IMM, 0, 0)); E16(0x12); E32(0x7777);           /* caso 7 */
    E(67); u32 tend2 = cpos; E16(0);
    w16(t9, rel());
    E(1); E(A(IMM, 0, 0)); E16(0x12); E32(0x9999);           /* caso 9 */
    w16(tend, rel()); w16(tend2, rel());
    E(82); E(2);                                             /* CALL_TABLE 2 */
    endtable();

    /* tabela 2: bits e aritmética em REG 0x30.. e PS; SETDATABLOCK + ID; DELAY; IIO */
    table(2, 1);
    E(1); E(A(IMM, 0, 0)); E16(0x30); E32(0xFF00FF00);       /* reg 0x30 = 0xFF00FF00 */
    E(7); E(A(IMM, 0, 0)); E16(0x30); E32(0x0FF00FF0);       /* AND reg 0x30 &= 0x0FF00FF0 */
    E(13); E(A(IMM, 4, 1)); E16(0x30); E(0x5A);              /* OR byte 1 (bits 15:8) |= 0x5A */
    E(19); E(A(0, 0, 0)); E16(0x30); E(4);                   /* SHIFT_LEFT reg 0x30 <<= 4 */
    E(92); E(A(IMM, 1, 1)); E16(0x31); E16(0x00FF); E16(0x1200);  /* MASK reg 0x31 word8: (x & 0xFF) | 0x1200 */
    E(84); E(A(0, 4, 0)); E16(0x32);                         /* CLEAR reg 0x32 byte0 */
    E(31 + 1); E(A(IMM, 0, 0)); E(0); E32(7);                /* MUL ps0 * 7 -> quociente */
    E(1); E(A(WS, 0, 0)); E16(0x33); E(0x40);                /* reg 0x33 = quociente */
    E(37 + 1); E(A(IMM, 0, 0)); E(0); E32(4);                /* DIV ps0 / 4 */
    E(1); E(A(WS, 0, 0)); E16(0x34); E(0x41);                /* reg 0x34 = resto */
    E(102); E(4);                                            /* SET_DATA_BLOCK 4 */
    E(1); E(A(ID, 0, 0)); E16(0x35); E16(4);                 /* reg 0x35 = dado[4] */
    E(81); E(25);                                            /* DELAY 25 µs */
    E(80); E(3);                                             /* DELAY 3 ms */
    E(55); E16(5);                                           /* SET_ATI_PORT 5 (IIO) */
    E(1); E(A(IMM, 0, 0)); E16(0x0); E32(0xAB);              /* escrita indireta: data = 0xAB */
    E(55); E16(0);                                           /* volta ao MMIO */
    E(74); E(A(IMM, 0, 0)); E16(0x30); E32(0x1);             /* TEST reg 0x30 & 1 */
    E(68); u32 teq = cpos; E16(0);                           /* JUMP_EQUAL (bit 0 é zero) */
    E(1); E(A(IMM, 0, 0)); E16(0x36); E32(0xBAD);
    w16(teq, rel());
    E(103); E(A(IMM, 0, 0)); E16(0x37); E32(0xFFFF);         /* XOR reg 0x37 ^= 0xFFFF */
    E(109); E(A(IMM, 0, 0)); E16(0x38); E32(8);              /* SHL reg 0x38 <<= 8 (dword) */
    E(109); E(A(IMM, 4, 0)); E16(0x3B); E(4);                /* SHL no byte 0: o que sai do byte se perde */
    E(123); E(A(IMM, 0, 0)); E(0); E32(0x10000);             /* MUL32 ps0 * 0x10000 */
    E(1); E(A(WS, 0, 0)); E16(0x39); E(0x41);                /* reg 0x39 = parte alta */
    endtable();

    /* tabela 3: laço infinito; tabela 4: chama a si mesma (recursão) */
    table(3, 0);
    u32 l3 = rel(); E(67); E16(l3);
    endtable();
    table(4, 0);
    E(82); E(4);
    endtable();
    /* tabela 5: lê fora da imagem (ID com data_block perto do fim) */
    table(5, 0);
    E(102); E(255);                                          /* data_block = início da tabela */
    E(1); E(A(ID, 0, 0)); E16(0x3A); E16(0xF000);
    endtable();
    /* tabela 6: opcode 0 encerra; tabela 7: REPEAT não tem formato definido e aborta */
    table(6, 0); E(0); endtable();
    table(7, 0); E(83); endtable();                          /* REPEAT e depois o EOT */
}

int main(void) {
    build();
    amd_atom_t a;
    memset(&a, 0, sizeof a);
    a.reg_read = rr; a.reg_write = rw; a.delay_us = dl;
    CHECK(amd_atom_init(&a, bios, sizeof bios) == 0 && a.cmd_table == 0x200 && a.data_table == 0x300 && a.iio[5] == 0x486, "cabeçalhos e IIO (iio[5]=0x%x)", a.iio[5]);
    u16 sz; u8 fr, cr; u16 st;
    CHECK(amd_atom_data_header(&a, ATOM_DATA_FIRMWARE_INFO, &sz, &fr, &cr, &st) && sz == 16 && fr == 3 && cr == 1 && st == 0x400, "cabeçalho de dados");
    CHECK(!amd_atom_data_header(&a, 9, NULL, NULL, NULL, NULL) && amd_atom_cmd_header(&a, 1, &fr, &cr) && fr == 1, "tabelas ausentes e presentes");
    regs[0x11] = 30; regs[0x31] = 0xABCD; regs[0x32] = 0x12345678; regs[0x37] = 0x0F0F; regs[0x38] = 0x00AB; regs[0x3B] = 0x12345683; regs[0x20] = 0xFFFF0000;
    u32 ps[4] = {0, 9, 0, 0};
    int e = amd_atom_execute(&a, 1, ps, 4);
    CHECK(!e, "tabela 1 rodou (%d)", e);
    CHECK(regs[0x10] == 55, "laço com COMPARE/JUMP_BELOW: soma 1..10 = %u", regs[0x10]);
    CHECK(ps[0] == 35 * 0 + 35, "PS = REG + 5 (%u)", ps[0]);
    CHECK(regs[0x12] == 0x9999, "SWITCH no caso 9 (0x%x)", regs[0x12]);
    CHECK(regs[0x30] == 0xF005F000u, "AND, OR no byte 1 e SHIFT_LEFT (0x%x)", regs[0x30]);   /* (0x0F000F00 | 0x5A00) << 4 */
    CHECK(regs[0x31] == 0x12ABCD, "MASK na palavra dos bits 23:8 (0x%x)", regs[0x31]);
    CHECK(regs[0x32] == 0x12345600, "CLEAR do byte 0 (0x%x)", regs[0x32]);
    CHECK(regs[0x33] == 35 * 7 && regs[0x34] == 35 % 4, "MUL e DIV (%u %u)", regs[0x33], regs[0x34]);
    CHECK(regs[0x35] == 0x12345678, "SET_DATA_BLOCK + ID (0x%x)", regs[0x35]);
    CHECK(delays_us == 25 + 3000, "DELAY µs e ms (%llu)", delays_us);
    CHECK(regs[0x21] == 0xFFFFAB00, "registrador indireto pelo IIO (0x%x)", regs[0x21]);
    CHECK(regs[0x36] == 0, "TEST + JUMP_EQUAL pulou");
    CHECK(regs[0x37] == 0xF0F0 && regs[0x38] == 0xAB00 && regs[0x3B] == 0x12345630, "XOR e SHL (0x%x 0x%x 0x%x)", regs[0x37], regs[0x38], regs[0x3B]);
    CHECK(regs[0x39] == 0, "MUL32 parte alta (%u)", regs[0x39]);
    CHECK(a.errors == 0 && a.bad_reads == 0, "sem erros (%llu %llu)", a.errors, a.bad_reads);

    CHECK(amd_atom_execute(&a, 3, ps, 4) == -3, "laço preso abortado pelo orçamento");
    CHECK(amd_atom_execute(&a, 4, ps, 4) != 0 && a.depth == 0, "recursão limitada e profundidade de volta a 0");
    u64 br = a.bad_reads;
    amd_atom_execute(&a, 5, ps, 4);
    CHECK(a.bad_reads > br && regs[0x3A] == 0, "leitura fora da imagem vira 0 e é contada");
    CHECK(amd_atom_execute(&a, 6, ps, 4) == 0, "opcode 0 encerra");
    u64 e7 = a.errors;
    CHECK(amd_atom_execute(&a, 7, ps, 4) == 0 && a.errors == e7 + 1, "REPEAT só é registrado (como no amdgpu)");
    CHECK(amd_atom_execute(&a, 99, ps, 4) < 0, "tabela inexistente");

    /* imagens ruins */
    amd_atom_t b;
    memset(&b, 0, sizeof b);
    u8 bad[0x200];
    memset(bad, 0, sizeof bad);
    CHECK(amd_atom_init(&b, bad, sizeof bad) < 0, "sem 0xAA55");
    bad[0] = 0x55; bad[1] = 0xAA;
    CHECK(amd_atom_init(&b, bad, sizeof bad) == -2, "sem a assinatura da ATI");
    memcpy(bad + 0x30, " 761295520", 10); bad[0x48] = 0xF0; bad[0x49] = 0x01;
    CHECK(amd_atom_init(&b, bad, sizeof bad) == -3, "cabeçalho ROM fora da imagem");

    printf("amdgpu-atom-host: %d/%d ok\n", pass, total);
    return pass == total ? 0 : 1;
}
