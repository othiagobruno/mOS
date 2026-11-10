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
int main(void) {
    if (system("mkdir -p /tmp && printf '%s\\n' 'nameserver 1.1.1.1' 'search exemplo.mos teste.mos' 'options ndots:2' > /tmp/mos-resolv.conf") != 0)
        return 1;
    setenv("MOS_RESOLV_CONF", "/tmp/mos-resolv.conf", 1);
    int fails = 0;
    if (res_init() != 0) { printf("FALHA res_init\n"); return 1; }
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
