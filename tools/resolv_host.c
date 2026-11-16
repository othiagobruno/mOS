/* Teste leve no host: search, ndots, um nome real e um NXDOMAIN. Sem QEMU. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int res_init(void);
int res_query(const char *name, int cls, int type, unsigned char *ans, int anslen);
int mos_res_ndots(void);
int mos_res_nsearch(void);
const char *mos_res_search(int i);
extern int h_errno;
int res_9_nmkquery(void *st, int op, const char *d, int cls, int type, const unsigned char *data, int dl, const unsigned char *nr, unsigned char *b, int bl);
int res_9_nsend(void *st, const unsigned char *m, int ml, unsigned char *a, int al);
int res_9_setservers(void *st, const void *set, int cnt);
int res_9_getservers(void *st, void *set, int cnt);
int main(void) {
    if (system("mkdir -p /tmp && printf '%s\\n' 'nameserver 1.1.1.1' 'search exemplo.mos teste.mos' 'options ndots:2' > /tmp/mos-resolv.conf") != 0)
        return 1;
    setenv("MOS_RESOLV_CONF", "/tmp/mos-resolv.conf", 1);
    int fails = 0;
    if (res_init() != 0) { printf("FALHA res_init\n"); return 1; }
    {   /* res_9_setservers / getservers / nmkquery / nsend (OBS, MuseScore, Shotcut) */
        unsigned char set[2][128] = {{0}}, got[3][128];
        set[0][0] = 28; set[0][1] = 30;                                  /* IPv6: ignorado */
        set[1][0] = 16; set[1][1] = 2; set[1][3] = 53; set[1][4] = 9; set[1][5] = 9; set[1][6] = 9; set[1][7] = 9;
        if (res_9_setservers(0, set, 2) != 1 || res_9_getservers(0, got, 3) != 1 || got[0][4] != 9 || got[0][7] != 9) { printf("FALHA setservers\n"); fails++; }
        unsigned char q[512], a[2048];
        int qn = res_9_nmkquery(0, 0, "example.com", 1, 1, 0, 0, 0, q, sizeof q);
        int an = qn > 0 ? res_9_nsend(0, q, qn, a, sizeof a) : -1;
        if (qn != 29 || an < 12 || a[0] != q[0] || a[1] != q[1] || !(a[2] & 0x80) || ((a[6] << 8) | a[7]) < 1) { printf("FALHA nsend (%d %d)\n", qn, an); fails++; }
        else printf("ok    nmkquery + nsend via 9.9.9.9: %d bytes, %d respostas\n", an, (a[6] << 8) | a[7]);
        set[1][4] = 1; set[1][5] = 1; set[1][6] = 1; set[1][7] = 1;
        res_9_setservers(0, set, 2);
    }
    int nd = mos_res_ndots();
    const char *s0 = mos_res_search(0);
    printf("ndots=%d nsearch=%d search0=%s\n", nd, mos_res_nsearch(), s0);
    if (nd != 2 || mos_res_nsearch() != 2 || strcmp(s0, "exemplo.mos")) { printf("FALHA search/ndots\n"); fails++; }
    else printf("ok search e ndots\n");
    unsigned char ans[1024];
    int n = res_query("example.com", 1, 1, ans, sizeof ans);
    printf("example.com A -> %d rcode=%d\n", n, n > 3 ? ans[3] & 15 : -1);
    if (n < 12) { printf("FALHA nome real\n"); fails++; }
    else printf("ok nome real\n");
    n = res_query("nxdomain.invalid", 1, 1, ans, sizeof ans);
    printf("nxdomain -> %d h_errno=%d\n", n, h_errno);
    if (n >= 0 || h_errno != 1) { printf("FALHA NXDOMAIN\n"); fails++; }
    else printf("ok NXDOMAIN\n");
    printf("resolv_host: %s\n", fails ? "falhou" : "ok");
    return fails ? 1 : 0;
}
