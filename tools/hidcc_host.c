/* mOS — teste de host do parser de teclas de mídia (kernel/dev/hid_cc.c)
 *   gcc -O1 -fsanitize=address,undefined -DMOS_HOST_TEST -Ikernel/include tools/hidcc_host.c kernel/dev/hid_cc.c -o /tmp/hidcc
 *   /tmp/hidcc tools/hidcc    -> "hidcc: ok"
 * Os .rd são os report descriptors lidos do sysfs do PC do Thiago (Satechi 0c45:7016 e mouse Sunplus 1bcf:0005):
 * descrição de hardware, não código. */
#include "hid_cc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails;
static void ok(int c, const char *m) { if (!c) { fails++; printf("FALHOU: %s\n", m); } }
static int load(const char *dir, const char *name, u8 *buf) {
    char p[512]; snprintf(p, sizeof p, "%s/%s", dir, name);
    FILE *f = fopen(p, "rb"); if (!f) { perror(p); exit(2); }
    int n = (int)fread(buf, 1, 4096, f); fclose(f); return n;
}
int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "tools/hidcc";
    u8 rd[4096]; hidcc_t c; u32 a[16];
    int n = load(dir, "satechi-consumer.rd", rd);
    ok(hidcc_parse(&c, rd, n) == 1, "satechi: um campo consumer");
    ok(c.ids && c.f[0].rid == 3 && c.f[0].array && c.f[0].size == 16 && c.f[0].count == 4 && c.f[0].page == 0x0C, "satechi: array 4x16 no id 3");
    u8 up[] = {3, 0xE9, 0, 0, 0, 0, 0, 0, 0};                     /* volume + */
    ok(hidcc_active(&c, up, sizeof up, a, 16) == 1 && a[0] == 0x000C00E9, "satechi: volume +");
    u8 two[] = {3, 0xCD, 0, 0xEA, 0, 0, 0, 0, 0};                  /* play/pausa + volume - */
    ok(hidcc_active(&c, two, sizeof two, a, 16) == 2 && a[0] == 0x000C00CD && a[1] == 0x000C00EA, "satechi: duas teclas");
    u8 rel[] = {3, 0, 0, 0, 0, 0, 0, 0, 0};
    ok(hidcc_active(&c, rel, sizeof rel, a, 16) == 0, "satechi: soltou");
    u8 other[] = {1, 0xE9, 0};
    ok(hidcc_active(&c, other, sizeof other, a, 16) == 0, "satechi: outro report id ignorado");
    n = load(dir, "satechi-teclado.rd", rd);
    ok(hidcc_parse(&c, rd, n) == 0, "teclado boot: sem consumer");
    n = load(dir, "mouse-sunplus.rd", rd);
    ok(hidcc_parse(&c, rd, n) == 0, "mouse: AC Pan fora de coleção Consumer não conta");
    /* teclado com bits variáveis (estilo Logitech): Usage Page 0C, Usage 01, Collection, Report ID 2,
     * Usages E9 EA E2 CD, 4 x 1 bit, Input(Data,Var,Abs), 4 bits de enchimento */
    static const u8 var[] = { 0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01, 0x85, 0x02, 0x09, 0xE9, 0x09, 0xEA, 0x09, 0xE2, 0x09, 0xCD,
                              0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x04, 0x81, 0x02, 0x95, 0x04, 0x81, 0x03,
                              0x05, 0x01, 0x09, 0x80, 0xA1, 0x01, 0x85, 0x04, 0x19, 0x81, 0x29, 0x83, 0x75, 0x01, 0x95, 0x03,
                              0x81, 0x02, 0x95, 0x05, 0x81, 0x03, 0xC0, 0xC0 };
    ok(hidcc_parse(&c, var, sizeof var) >= 1, "variável: campos");
    u8 m[] = {2, 0x04};
    ok(hidcc_active(&c, m, sizeof m, a, 16) == 1 && a[0] == 0x000C00E2, "variável: mudo");
    u8 pp[] = {2, 0x09};
    ok(hidcc_active(&c, pp, sizeof pp, a, 16) == 2 && a[0] == 0x000C00E9 && a[1] == 0x000C00CD, "variável: dois bits");
    printf(fails ? "hidcc: %d falhas\n" : "hidcc: ok\n", fails);
    return fails != 0;
}
