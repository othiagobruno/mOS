/* mOS PDFKit — texto puro (sem ObjC): dobra de caixa e acentos, busca, palavra e linha. Incluído por pdfkit.c e
 * pelo teste de host (tools/pdfkit_host.c). */

/* minúscula e, se pedido, sem acento (Latim-1, Latim Estendido-A, grego e cirílico básicos) */
static u16 pk_fold(u16 c, int diacritics) {
    if (c >= 'A' && c <= 'Z') c = (u16)(c + 32);
    else if (c >= 0xC0 && c <= 0xDE && c != 0xD7) c = (u16)(c + 32);
    else if (c >= 0x100 && c <= 0x17F && c != 0x130 && c != 0x131 && c != 0x138 && c != 0x149 && c != 0x178) {
        /* pares maiúscula/minúscula: par/ímpar, com a faixa 0x139–0x148 e 0x179–0x17E deslocadas */
        if ((c >= 0x139 && c <= 0x148) || (c >= 0x179 && c <= 0x17E)) { if (c & 1) c++; }
        else if (!(c & 1)) c++;
    } else if (c == 0x178) c = 0xFF;
    else if (c >= 0x391 && c <= 0x3AB && c != 0x3A2) c = (u16)(c + 32);
    else if (c >= 0x410 && c <= 0x42F) c = (u16)(c + 32);
    else if (c >= 0x400 && c <= 0x40F) c = (u16)(c + 80);
    if (!diacritics) return c;
    static const char lat1[] = "aaaaaaaceeeeiiiidnooooo/ouuuuyty";       /* 0xE0–0xFF */
    if (c >= 0xE0 && c <= 0xFF && c != 0xF7) { char m = lat1[c - 0xE0]; if (m != '/') return (u16)m; }
    if (c >= 0x100 && c <= 0x17F) {
        static const char exta[] = "aaaaaaccccccccddddeeeeeeeeeegggggggghhhhiiiiiiiiiiiijjkkkllllllllllnnnnnnnnnoooooooorrrrrrssssssssttttttuuuuuuuuuuuuwwyyyzzzzzzs";
        return (u16)exta[c - 0x100];
    }
    if (c >= 0x300 && c <= 0x36F) return 0;            /* acento combinante some */
    return c;
}
/* busca needle (já dobrado) em hay a partir de start; devolve o índice ou −1. Ignora acentos combinantes no texto
 * quando diacritics. Tamanho do trecho em *mlen. */
static long pk_find(const u16 *hay, long n, const u16 *needle, long m, long start, int fold_case, int diacritics, int backwards, long *mlen) {
    if (m <= 0 || n <= 0) return -1;
    long first = backwards ? n - 1 : start, step = backwards ? -1 : 1;
    if (backwards && start >= 0 && start < n) first = start;
    for (long i = first; i >= 0 && i < n; i += step) {
        long j = i, k = 0;
        while (j < n && k < m) {
            u16 a = hay[j];
            if (fold_case || diacritics) a = pk_fold(a, diacritics);
            if (!a && diacritics) { j++; continue; }       /* combinante dentro do trecho */
            u16 b = needle[k];
            /* espaço do padrão casa com qualquer espaço em branco (o texto pode ter quebra de linha) */
            if (b == ' ' && (a == ' ' || a == '\n' || a == '\t' || a == 0xA0)) { j++; k++; continue; }
            if (a != b) break;
            j++; k++;
        }
        if (k == m) { *mlen = j - i; return i; }
    }
    return -1;
}
static int pk_is_word(u16 c) {
    return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_' || c == '\'' || c == 0x2019 ||
           (c >= 0xC0 && c != 0xD7 && c != 0xF7 && c < 0x2000) || (c >= 0x3040 && c < 0xA000) || (c >= 0xAC00 && c < 0xD7B0);
}
static void pk_word_at(const u16 *t, long n, long i, long *a, long *b) {
    if (i < 0 || i >= n) { *a = *b = i; return; }
    long s = i, e = i + 1;
    if (pk_is_word(t[i])) {
        while (s > 0 && pk_is_word(t[s - 1])) s--;
        while (e < n && pk_is_word(t[e])) e++;
    }
    *a = s; *b = e;
}
static void pk_line_at(const u16 *t, long n, long i, long *a, long *b) {
    if (i < 0 || i >= n) { *a = *b = i; return; }
    long s = i, e = i;
    while (s > 0 && t[s - 1] != '\n') s--;
    while (e < n && t[e] != '\n') e++;
    *a = s; *b = e;
}
