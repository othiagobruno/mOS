/* h264_sps_wh (userland/libsystem/h264sw.c) no primeiro SPS de um Annex B: imprime "LxA". Rodado por tools/h264_test.sh. */
#include <stdio.h>
#include <stdlib.h>
#include "h264sw.h"
int main(int c, char **v) {
    (void)c;
    FILE *f = fopen(v[1], "rb"); fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *b = malloc(n); if (fread(b, 1, n, f) != (size_t)n) return 2; fclose(f);
    for (long i = 0; i + 4 < n; i++) {
        if (b[i] || b[i + 1] || b[i + 2] != 1 || (b[i + 3] & 31) != 7) continue;
        long e = i + 3;
        while (e + 3 < n && !(b[e] == 0 && b[e + 1] == 0 && (b[e + 2] == 1 || b[e + 2] == 0))) e++;
        int w, h;
        if (h264_sps_wh(b + i + 3, (int)(e - i - 3), &w, &h)) { puts("erro"); return 1; }
        printf("%dx%d\n", w, h);
        return 0;
    }
    puts("sem SPS");
    return 1;
}
