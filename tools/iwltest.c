/* mOS — confere no host a leitura do firmware Intel (kernel/wifi/iwl_fw.c) com o .ucode real */
#include "iwl.h"
long long g_live_allocs, g_live_bytes;

int main(int argc, char **argv) {
    const char *path = argc > 1 ? argv[1] : "firmware/intel/iwlwifi-so-a0-hr-b0-89.ucode";
    FILE *f = fopen(path, "rb");
    if (!f) { perror(path); return 2; }
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    u8 *d = malloc(n);
    fread(d, 1, n, f);
    fclose(f);
    iwl_fw_t fw;
    char err[96];
    if (!iwl_fw_parse(d, (u64)n, &fw, err, sizeof err)) { printf("FALHOU: %s\n", err); return 1; }
    printf("\"%s\"\nversão %u.%u, %d TLVs, %d seções RT (%llu KiB), %d INIT, IML %u bytes, %u CPU(s), paginação %u, canais de varredura %u\n",
           fw.human, fw.major, fw.minor, fw.ntlv, fw.n_rt, (unsigned long long)(fw.rt_bytes >> 10), fw.n_init, fw.iml_len, fw.num_cpu,
           fw.paging_size, fw.n_scan_channels);
    int ok = fw.n_rt > 4 && fw.iml_len > 0 && fw.major >= 70;
    printf(ok ? "TUDO OK\n" : "FALHOU (esperava várias seções, IML e versão >= 70)\n");
    return !ok;
}
