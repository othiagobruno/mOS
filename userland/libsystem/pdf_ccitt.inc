/* mOS PDF — CCITTFaxDecode (ITU-T T.4 grupo 3, 1D e 2D, e T.6 grupo 4). Incluído por cgpdf_draw.c.
 * Saída: 1 bit por pixel, linhas alinhadas a byte, 0 = preto (como o filtro do PDF com BlackIs1 false). */

static const char *const cc_white_term[64] = {
    "00110101", "000111", "0111", "1000", "1011", "1100", "1110", "1111", "10011", "10100", "00111", "01000", "001000", "000011", "110100",
    "110101", "101010", "101011", "0100111", "0001100", "0001000", "0010111", "0000011", "0000100", "0101000", "0101011", "0010011",
    "0100100", "0011000", "00000010", "00000011", "00011010", "00011011", "00010010", "00010011", "00010100", "00010101", "00010110",
    "00010111", "00101000", "00101001", "00101010", "00101011", "00101100", "00101101", "00000100", "00000101", "00001010", "00001011",
    "01010010", "01010011", "01010100", "01010101", "00100100", "00100101", "01011000", "01011001", "01011010", "01011011", "01001010",
    "01001011", "00110010", "00110011", "00110100"};
static const char *const cc_white_make[27] = {
    "11011", "10010", "010111", "0110111", "00110110", "00110111", "01100100", "01100101", "01101000", "01100111", "011001100",
    "011001101", "011010010", "011010011", "011010100", "011010101", "011010110", "011010111", "011011000", "011011001", "011011010",
    "011011011", "010011000", "010011001", "010011010", "011000", "010011011"};
static const char *const cc_black_term[64] = {
    "0000110111", "010", "11", "10", "011", "0011", "0010", "00011", "000101", "000100", "0000100", "0000101", "0000111", "00000100",
    "00000111", "000011000", "0000010111", "0000011000", "0000001000", "00001100111", "00001101000", "00001101100", "00000110111",
    "00000101000", "00000010111", "00000011000", "000011001010", "000011001011", "000011001100", "000011001101", "000001101000",
    "000001101001", "000001101010", "000001101011", "000011010010", "000011010011", "000011010100", "000011010101", "000011010110",
    "000011010111", "000001101100", "000001101101", "000011011010", "000011011011", "000001010100", "000001010101", "000001010110",
    "000001010111", "000001100100", "000001100101", "000001010010", "000001010011", "000000100100", "000000110111", "000000111000",
    "000000100111", "000000101000", "000001011000", "000001011001", "000000101011", "000000101100", "000001011010", "000001100110",
    "000001100111"};
static const char *const cc_black_make[27] = {
    "0000001111", "000011001000", "000011001001", "000001011011", "000000110011", "000000110100", "000000110101", "0000001101100",
    "0000001101101", "0000001001010", "0000001001011", "0000001001100", "0000001001101", "0000001110010", "0000001110011",
    "0000001110100", "0000001110101", "0000001110110", "0000001110111", "0000001010010", "0000001010011", "0000001010100",
    "0000001010101", "0000001011010", "0000001011011", "0000001100100", "0000001100101"};
static const char *const cc_ext_make[13] = {"00000001000", "00000001100", "00000001101", "000000010010", "000000010011", "000000010100",
                                            "000000010101", "000000010110", "000000010111", "000000011100", "000000011101",
                                            "000000011110", "000000011111"};
/* tabela de 13 bits: (comprimento << 16) | (corrida + 1); 0 = código inválido */
static u32 *cc_tab[2];
static void cc_add(u32 *t, const char *code, int run) {
    int L = (int)strlen(code);
    u32 v = 0;
    for (int i = 0; i < L; i++) v = v << 1 | (u32)(code[i] - '0');
    for (u32 k = 0; k < (1u << (13 - L)); k++) t[v << (13 - L) | k] = (u32)L << 16 | (u32)(run + 1);
}
static int cc_init(void) {
    if (cc_tab[0]) return 1;
    for (int c = 0; c < 2; c++) {
        u32 *t = calloc(8192, 4);
        if (!t) return 0;
        const char *const *term = c ? cc_black_term : cc_white_term, *const *make = c ? cc_black_make : cc_white_make;
        for (int i = 0; i < 64; i++) cc_add(t, term[i], i);
        for (int i = 0; i < 27; i++) cc_add(t, make[i], 64 * (i + 1));
        for (int i = 0; i < 13; i++) cc_add(t, cc_ext_make[i], 1792 + 64 * i);
        cc_tab[c] = t;
    }
    return 1;
}
typedef struct { const u8 *d; size_t n, bit; } cc_rd;
static u32 cc_peek(cc_rd *r, int k) {
    u32 v = 0;
    for (int i = 0; i < k; i++) {
        size_t b = r->bit + (size_t)i;
        v = v << 1 | (b / 8 < r->n ? (u32)(r->d[b / 8] >> (7 - b % 8) & 1) : 0);
    }
    return v;
}
static int cc_eof(cc_rd *r) { return r->bit / 8 >= r->n; }
/* uma corrida da cor dada (soma das makeup até a terminal); -1 erro */
static int cc_run(cc_rd *r, int color) {
    int total = 0;
    for (int guard = 0; guard < 64; guard++) {
        u32 e = cc_tab[color][cc_peek(r, 13)];
        if (!e) return -1;
        r->bit += e >> 16;
        int run = (int)(e & 0xFFFF) - 1;
        total += run;
        if (run < 64) return total;
    }
    return -1;
}
/* modo 2D: 0 pass, 1 horizontal, 2..8 vertical (V0, VR1..3, VL1..3), 9 EOL, -1 erro */
static int cc_mode(cc_rd *r) {
    u32 v = cc_peek(r, 7);
    if (v >> 6 == 1) { r->bit += 1; return 2; }                   /* 1: V0 */
    if (v >> 4 == 3) { r->bit += 3; return 3; }                   /* 011: VR1 */
    if (v >> 4 == 2) { r->bit += 3; return 6; }                   /* 010: VL1 */
    if (v >> 4 == 1) { r->bit += 3; return 1; }                   /* 001: horizontal */
    if (v >> 3 == 1) { r->bit += 4; return 0; }                   /* 0001: pass */
    if (v >> 1 == 3) { r->bit += 6; return 4; }                   /* 000011: VR2 */
    if (v >> 1 == 2) { r->bit += 6; return 7; }                   /* 000010: VL2 */
    if (v == 3) { r->bit += 7; return 5; }                        /* 0000011: VR3 */
    if (v == 2) { r->bit += 7; return 8; }                        /* 0000010: VL3 */
    if (cc_peek(r, 12) == 1) { r->bit += 12; return 9; }          /* EOL */
    return -1;
}
/* posições de mudança de cor de uma linha (começa em branco) */
static int cc_next_change(const int *ch, int nch, int a0, int color, int cols) {
    /* b1: primeira mudança na linha de referência à direita de a0 com cor oposta à de a0 */
    for (int i = 0; i < nch; i++) if (ch[i] > a0 && (i & 1) == color) return ch[i];
    return cols;
}
int pdf_ccitt_decode(const u8 *in, size_t n, int k, int cols, int rows, int black1, int byte_align, int eob, u8 **out, size_t *on) {
    (void)eob;
    if (cols < 1 || cols > 65536 || !cc_init()) return -1;
    size_t bpr = ((size_t)cols + 7) / 8;
    size_t cap = rows > 0 ? (size_t)rows : 64;
    u8 *o = malloc(cap * bpr);
    int *ref = malloc(((size_t)cols + 4) * sizeof(int)), *cur = malloc(((size_t)cols + 4) * sizeof(int));
    if (!o || !ref || !cur) { free(o); free(ref); free(cur); return -1; }
    int nref = 0, ncur;
    cc_rd r = {in, n, 0};
    int y = 0;
    for (; rows <= 0 || y < rows; y++) {
        if (cc_eof(&r)) break;
        if (byte_align && k >= 0) r.bit = (r.bit + 7) & ~(size_t)7;
        /* EOL opcional (grupo 3) */
        int twod = k < 0;
        if (k >= 0) {
            while (cc_peek(&r, 12) == 1) { r.bit += 12; }
            if (k > 0) { twod = cc_peek(&r, 1) == 0; r.bit += 1; }
        } else if (byte_align) r.bit = (r.bit + 7) & ~(size_t)7;
        if (k < 0 && cc_peek(&r, 24) == 0x001001) break;              /* EOFB do grupo 4 */
        ncur = 0;
        int a0 = -1, color = 0, err = 0;
        if (!twod) {
            int x = 0;
            while (x < cols) {
                int run = cc_run(&r, color);
                if (run < 0) { err = 1; break; }
                x += run;
                if (x > cols) x = cols;
                cur[ncur++] = x;
                color ^= 1;
            }
        } else {
            while (a0 < cols) {
                int m = cc_mode(&r);
                if (m < 0 || m == 9) { err = 1; break; }
                int b1 = cc_next_change(ref, nref, a0, color, cols), b2 = b1;
                if (b1 < cols) { for (int i = 0; i < nref; i++) if (ref[i] == b1) { b2 = i + 1 < nref ? ref[i + 1] : cols; break; } }
                if (m == 0) { a0 = b2; continue; }               /* pass: a0 vai para b2, sem mudar de cor */
                if (m == 1) {
                    int r1 = cc_run(&r, color), r2 = cc_run(&r, color ^ 1);
                    if (r1 < 0 || r2 < 0) { err = 1; break; }
                    int start = a0 < 0 ? 0 : a0;
                    int a1 = start + r1, a2 = a1 + r2;
                    if (a1 > cols) a1 = cols;
                    if (a2 > cols) a2 = cols;
                    cur[ncur++] = a1;
                    cur[ncur++] = a2;
                    a0 = a2;
                    continue;
                }
                static const int dv[] = {0, 0, 0, 1, 2, 3, -1, -2, -3};
                int a1 = b1 + dv[m];
                if (a1 < 0) a1 = 0;
                if (a1 > cols) a1 = cols;
                cur[ncur++] = a1;
                a0 = a1;
                color ^= 1;
            }
        }
        if (err && !ncur) break;
        if ((size_t)y >= cap) {
            cap *= 2;
            u8 *no = realloc(o, cap * bpr);
            if (!no) break;
            o = no;
        }
        /* linha: branco até a primeira mudança, depois alterna */
        u8 *row = o + (size_t)y * bpr;
        memset(row, 0xFF, bpr);
        int x = 0, c = 0;
        for (int i = 0; i <= ncur; i++) {
            int e = i < ncur ? cur[i] : cols;
            if (e > cols) e = cols;
            if (c) for (int p = x; p < e; p++) row[p >> 3] &= (u8)~(0x80 >> (p & 7));
            x = e;
            c ^= 1;
        }
        int *t = ref; ref = cur; cur = t;
        nref = ncur;
        if (err) { y++; break; }
    }
    free(ref);
    free(cur);
    if (black1) for (size_t i = 0; i < (size_t)y * bpr; i++) o[i] = (u8)~o[i];
    *out = o;
    *on = (size_t)y * bpr;
    return 0;
}
