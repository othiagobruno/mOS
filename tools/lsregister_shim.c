/* Shim de host do lsregister(1): um banco do Launch Services em memória no lugar do kernel. */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
static char reg[16][512];
static int nreg;
long cx_ls_raw(int op, const char *key, unsigned roles, char *out, size_t cap) {
    out[0] = 0;
    if (op == 16) {                                      /* APP_BY_PATH */
        char ip[600];
        struct stat st;
        snprintf(ip, sizeof ip, "%s/Contents/Info.plist", key);
        if (stat(ip, &st)) return -1;
        int found = 0;
        for (int i = 0; i < nreg; i++) if (!strcmp(reg[i], key)) found = 1;
        if (!found && nreg < 16) snprintf(reg[nreg++], 512, "%s", key);
        const char *b = strrchr(key, '/');
        snprintf(out, cap, "com.teste.%.*s\t%s\t%s\n", (int)(strlen(b + 1) - 4), b + 1, key, b + 1);
        FILE *f = fopen(getenv("LSREG_LOG"), "a");
        if (f) { fprintf(f, "reg %s\n", key); fclose(f); }
        return 0;
    }
    if (op == 19) {                                      /* DUMP */
        if ((int)roles < nreg) snprintf(out, cap, "bundle id:      x%u\npath:           %s\n", roles, reg[roles]);
        return nreg;
    }
    if (op == 20) {                                      /* UNREGISTER */
        for (int i = 0; i < nreg; i++) if (!strcmp(reg[i], key)) { memmove(reg[i], reg[i + 1], (size_t)(nreg - i - 1) * 512); nreg--; return 0; }
        return -2;
    }
    if (op == 21) return 7;                              /* RESCAN */
    return -22;
}
