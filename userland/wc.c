/* mOS — wc: conta linhas, palavras e bytes da entrada padrão (ou de um arquivo, se dado) */
#include "mos.h"

void _start(const char *arg) {
    int fd = 0;
    if (arg && *arg) {
        fd = open(arg, MOS_O_RDONLY);
        if (fd < 0) { print("wc: %s: erro %d\n", arg, (i64)fd); exit(1); }
    }
    u64 lines = 0, words = 0, bytes = 0;
    int in_word = 0;
    char buf[1024];
    i64 n;
    while ((n = read(fd, buf, sizeof buf)) > 0) {
        bytes += (u64)n;
        for (i64 i = 0; i < n; i++) {
            char c = buf[i];
            if (c == '\n') lines++;
            int sp = c == ' ' || c == '\n' || c == '\t' || c == '\r';
            if (!sp && !in_word) words++;
            in_word = !sp;
        }
    }
    print("%u linhas, %u palavras, %u bytes\n", lines, words, bytes);
    exit(0);
}
