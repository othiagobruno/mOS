/* mOS — teste de host das contas do TD isócrono do xHCI (kernel/include/usb_iso.h)
 *   gcc -O1 -fsanitize=address,undefined -DMOS_HOST_TEST -Ikernel/include tools/usb_iso_host.c -o /tmp/usb_iso && /tmp/usb_iso */
#include "usb_iso.h"
#include <stdio.h>
static int fails;
static void ok(int c, const char *m) { if (!c) { fails++; printf("FALHOU: %s\n", m); } }
int main(void) {
    u32 tbc, tlbpc;
    /* HS alta banda (UVC 1080p): mps 1024, 3 transações por microframe */
    ok(usb_iso_esit(1024, 2, 0, 0, false) == 3072, "HS: ESIT 3072");
    usb_iso_td_fields(3072, 1024, 2, false, true, &tbc, &tlbpc);
    ok(tbc == 0 && tlbpc == 2, "HS: 3 pacotes = TBC 0, TLBPC 2");
    usb_iso_td_fields(1500, 1024, 2, false, true, &tbc, &tlbpc);
    ok(tbc == 0 && tlbpc == 1, "HS: 1500 bytes = 2 pacotes");
    /* FS áudio (UAC1 48 kHz estéreo 16 bits = 192 bytes por ms) */
    ok(usb_iso_esit(196, 0, 0, 0, false) == 196, "FS: ESIT = mps");
    usb_iso_td_fields(192, 196, 0, false, true, &tbc, &tlbpc);
    ok(tbc == 0 && tlbpc == 0, "FS: um pacote");
    /* SS: burst 16, Mult 3 → 48 pacotes de 1024 */
    ok(usb_iso_esit(1024, 15, 2, 0, true) == 49152 && usb_iso_esit(1024, 15, 2, 40000, true) == 40000, "SS: ESIT pelo wBytesPerInterval");
    usb_iso_td_fields(49152, 1024, 15, true, true, &tbc, &tlbpc);
    ok(tbc == 2 && tlbpc == 15, "SS: 48 pacotes = 3 bursts cheios");
    usb_iso_td_fields(1500, 1024, 3, true, true, &tbc, &tlbpc);
    ok(tbc == 0 && tlbpc == 1, "SS: 2 pacotes num burst de 4");
    usb_iso_td_fields(5 * 1024, 1024, 3, true, true, &tbc, &tlbpc);
    ok(tbc == 1 && tlbpc == 0, "SS: 5 pacotes = burst de 4 + 1");
    usb_iso_td_fields(5 * 1024, 1024, 3, true, false, &tbc, &tlbpc);
    ok(tbc == 0 && tlbpc == 4, "xHCI 0.96: sem TBC");
    usb_iso_td_fields(0, 1024, 3, true, true, &tbc, &tlbpc);
    ok(tbc == 0 && tlbpc == 0, "TD vazio conta 1 pacote");
    u32 d3 = usb_iso_trb_d3(2, 15, true, 5);
    ok((d3 >> 31) == 1 && ((d3 >> 16) & 15) == 15 && ((d3 >> 10) & 63) == 5 && ((d3 >> 7) & 3) == 2 && (d3 & (1u << 5)) && (d3 & (1u << 2)) && !(d3 & 1), "bits do Isoch TRB");
    printf(fails ? "usb-iso: %d falhas\n" : "usb-iso: ok\n", fails);
    return fails != 0;
}
