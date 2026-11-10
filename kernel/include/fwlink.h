/* Link de framework que o Windows gravou como arquivo de texto (o alvo numa linha)
 * ou link do mOS ("#mOS-link:<alvo>"). Não é um Mach-O, plist, JSON nem URL.
 * Cabe no kernel (sem libc) e no mkimg/host. */
#pragma once

static inline int fwlink_magic4(const unsigned char *b, unsigned long long n, unsigned a, unsigned c, unsigned d, unsigned e) {
    return n >= 4 && b[0] == a && b[1] == c && b[2] == d && b[3] == e;
}

static inline int fwlink_macho(const unsigned char *b, unsigned long long n) {
    /* MH_MAGIC_64 / MH_CIGAM_64, MH_MAGIC / MH_CIGAM, FAT_MAGIC / FAT_CIGAM */
    return fwlink_magic4(b, n, 0xCF, 0xFA, 0xED, 0xFE) || fwlink_magic4(b, n, 0xFE, 0xED, 0xFA, 0xCF)
        || fwlink_magic4(b, n, 0xCE, 0xFA, 0xED, 0xFE) || fwlink_magic4(b, n, 0xFE, 0xED, 0xFA, 0xCE)
        || fwlink_magic4(b, n, 0xCA, 0xFE, 0xBA, 0xBE) || fwlink_magic4(b, n, 0xBE, 0xBA, 0xFE, 0xCA);
}

static inline int fwlink_ok(unsigned char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')
        || c == '.' || c == '_' || c == '+' || c == '-' || c == '@' || c == '(' || c == ')'
        || c == ' ' || c == '/';
}

/* 1 se reconheceu. out recebe o alvo, sem o prefixo e sem o newline. */
static inline int fwlink_target(const void *data, unsigned long long n, char *out, unsigned long long cap) {
    const unsigned char *b = (const unsigned char *)data;
    unsigned long long i = 0, o = 0;
    int prefixed = 0, slash = 0;
    if (!b || !out || cap < 2 || !n || n > 400) return 0;
    if (n >= 10 && b[0] == '#' && b[1] == 'm' && b[2] == 'O' && b[3] == 'S' && b[4] == '-'
        && b[5] == 'l' && b[6] == 'i' && b[7] == 'n' && b[8] == 'k' && b[9] == ':') {
        prefixed = 1;
        i = 10;
    } else {
        if (fwlink_macho(b, n)) return 0;
        if (b[0] == '<' || b[0] == '{' || b[0] == '[') return 0;
        if (n >= 6 && b[0] == 'b' && b[1] == 'p' && b[2] == 'l' && b[3] == 'i' && b[4] == 's' && b[5] == 't') return 0;
        if (n >= 8 && b[0] == 'h' && b[1] == 't' && b[2] == 't' && b[3] == 'p' && b[4] == 's' && b[5] == ':' && b[6] == '/' && b[7] == '/') return 0;
        if (n >= 7 && b[0] == 'h' && b[1] == 't' && b[2] == 't' && b[3] == 'p' && b[4] == ':' && b[5] == '/' && b[6] == '/') return 0;
    }
    for (; i < n; i++) {
        unsigned char c = b[i];
        if (c == '\n' || c == '\r') {
            unsigned long long j;
            for (j = i; j < n; j++) if (b[j] != '\n' && b[j] != '\r' && b[j] != ' ') return 0;
            break;
        }
        if (!fwlink_ok(c)) return 0;
        if (c == '/') slash = 1;
        if (o + 1 >= cap) return 0;
        out[o++] = (char)c;
    }
    while (o && (out[o - 1] == ' ' || out[o - 1] == '/')) o--;
    out[o] = 0;
    if (!o || o > 300) return 0;
    if (out[0] == ' ' || (out[0] == '.' && out[1] == 0)) return 0;
    if (!prefixed && !slash && o > 80) return 0;
    return 1;
}

/* Reescreve o buffer como "#mOS-link:<alvo>". 0 se não era link ou já estava no formato. */
static inline unsigned long long fwlink_canonicalize(const void *data, unsigned long long n, char *out, unsigned long long cap) {
    char tgt[512];
    unsigned long long i, tl;
    const char *pre = "#mOS-link:";
    if (!fwlink_target(data, n, tgt, sizeof tgt)) return 0;
    if (n >= 10) {
        const unsigned char *b = (const unsigned char *)data;
        if (b[0] == '#' && b[1] == 'm' && b[2] == 'O' && b[3] == 'S' && b[4] == '-') return 0;
    }
    tl = 0;
    while (tgt[tl]) tl++;
    if (10 + tl + 1 > cap) return 0;
    for (i = 0; i < 10; i++) out[i] = pre[i];
    for (i = 0; i < tl; i++) out[10 + i] = tgt[i];
    out[10 + tl] = 0;
    return 10 + tl;
}
