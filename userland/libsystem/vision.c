/*
 * mOS libSystem — Vision.framework.
 *
 * VNImageRequestHandler lê CGImage, CVPixelBuffer (BGRA/ARGB/RGBA e os biplanares 420v/420f pelo plano Y), NSData
 * e arquivos (PNG/JPEG/BMP/GIF pelo decodificador do sistema, GUI_DECODE_IMAGE), converte para luminância e roda:
 *
 *  - VNDetectBarcodesRequest: QR Code de verdade, versões 1–40 — binarização com média local, padrões
 *    localizadores 1:1:3:1:1 nas duas direções, orientação, grade afim, formato com BCH (corrige até 3 bits),
 *    máscaras, desintercalação dos blocos e Reed-Solomon em GF(256) (Berlekamp-Massey, Chien, Forney), segmentos
 *    numéricos, alfanuméricos, bytes (UTF-8/Latin-1), kanji (Shift-JIS) e ECI; e EAN-13, UPC-A (relatado como EAN-13, como na
 *    Apple), EAN-8 e Code 128 (conjuntos A/B/C) em linhas e colunas, com dígito verificador. Resultados em VNBarcodeObservation com caixa,
 *    cantos (coordenadas normalizadas, origem embaixo à esquerda), payload e simbologia.
 *  - VNGenerateImageFeaturePrintRequest: descritor de 96 floats (8×8 de luminância sem o brilho médio e
 *    histograma de 32 faixas); VNFeaturePrintObservation calcula a distância euclidiana.
 *  - VNDetectRectanglesRequest: Sobel, componentes de borda e quadriláteros com ângulos perto de 90°.
 *  - Faces, texto e classificação: dependem dos modelos de aprendizado de máquina da Apple, que o mOS
 *    não tem; os pedidos executam e terminam sem observações (results vazio), sem erro — o mesmo que um Mac
 *    devolve para uma imagem sem rostos/texto.
 */
#include "foundation.h"
#include "sys.h"
#include "../../kernel/include/compat_gui.h"
#include "vision_sjis.inc"         /* modo kanji do QR */

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
const char *cx_cstr(id s);
id cx_str(const char *s);
id cx_str_n(const char *s, u64 n);
int strcmp(const char *a, const char *b);
void *malloc(size_t n);
void *calloc(size_t n, size_t m);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
void *memmove(void *d, const void *s, size_t n);
size_t strlen(const char *s);
double sqrt(double), fabs(double), floor(double);
extern u8 *cx_read_file(const char *path, u64 *len);
size_t CGImageGetWidth(void *img);
size_t CGImageGetHeight(void *img);
size_t CGImageGetBytesPerRow(void *img);
u32 CGImageGetBitmapInfo(void *img);
void *CGImageGetDataProvider(void *img);
void *CGDataProviderCopyData(void *p);
const u8 *CFDataGetBytePtr(void *d);
long CFDataGetLength(void *d);
void CFRelease(void *o);
void *CVPixelBufferGetBaseAddress(void *p);
void *CVPixelBufferGetBaseAddressOfPlane(void *p, u64 i);
u64 CVPixelBufferGetBytesPerRow(void *p);
u64 CVPixelBufferGetBytesPerRowOfPlane(void *p, u64 i);
u64 CVPixelBufferGetWidth(void *p);
u64 CVPixelBufferGetHeight(void *p);
u32 CVPixelBufferGetPixelFormatType(void *p);
int CVPixelBufferLockBaseAddress(void *p, u64 f);
int CVPixelBufferUnlockBaseAddress(void *p, u64 f);

static id cls(const char *n) { return (id)cx_class_named(n); }
static BOOL resp(id o, const char *s) { return o && cx_responds(object_getClass(o), sel_registerName(s)); }
static id ret(id o) { return o ? objc_retain(o) : 0; }
static void setobj(id *slot, id v) { if (*slot == v) return; if (v) objc_retain(v); if (*slot) objc_release(*slot); *slot = v; }
static u64 cnt(id a) { return a && resp(a, "count") ? MSG0(u64, a, "count") : 0; }
static id at(id a, u64 i) { return MSG(id, a, "objectAtIndex:", i); }
static id marr(void) { return MSG0(id, cls("NSMutableArray"), "array"); }
typedef struct { double x, y; } pt_t;
typedef struct { double x, y, w, h; } rc_t;

/* ================================================================ imagem → luminância */
typedef struct { int w, h; u8 *y; } luma_t;
static void luma_free(luma_t *l) { free(l->y); l->y = 0; }
/* 32 bits por pixel: o verde fica no byte 1 (RGBA, BGRA) ou 2 (ARGB, XRGB big-endian) */
static BOOL luma_from_32(luma_t *l, const u8 *p, int w, int h, size_t bpr, int alpha_first_big) {
    l->w = w; l->h = h;
    l->y = malloc((size_t)w * h);
    if (!l->y) return NO;
    int ro = alpha_first_big ? 1 : 0, go = ro + 1, bo = ro + 2;
    for (int yy = 0; yy < h; yy++) {
        const u8 *row = p + (size_t)yy * bpr;
        for (int x = 0; x < w; x++) {
            const u8 *q = row + 4 * x;
            l->y[yy * w + x] = (u8)((q[ro] * 77 + q[go] * 150 + q[bo] * 29) >> 8);
        }
    }
    return YES;
}
static BOOL luma_from_cgimage(luma_t *l, void *img) {
    int w = (int)CGImageGetWidth(img), h = (int)CGImageGetHeight(img);
    size_t bpr = CGImageGetBytesPerRow(img);
    if (w <= 0 || h <= 0) return NO;
    void *prov = CGImageGetDataProvider(img);
    void *data = prov ? CGDataProviderCopyData(prov) : 0;
    if (!data) return NO;
    u32 info = CGImageGetBitmapInfo(img);
    int alpha = (int)(info & 31), little = (info & 0x7000) == 0x2000;
    int first = alpha == 2 || alpha == 4 || alpha == 6;
    BOOL ok = (size_t)CFDataGetLength(data) >= bpr * (size_t)h && luma_from_32(l, CFDataGetBytePtr(data), w, h, bpr, first && !little);
    CFRelease(data);
    return ok;
}
static BOOL luma_from_pixelbuffer(luma_t *l, void *pb) {
    u32 fmt = CVPixelBufferGetPixelFormatType(pb);
    int w = (int)CVPixelBufferGetWidth(pb), h = (int)CVPixelBufferGetHeight(pb);
    if (w <= 0 || h <= 0) return NO;
    CVPixelBufferLockBaseAddress(pb, 1);
    BOOL ok = NO;
    if (fmt == 0x34323076 || fmt == 0x34323066 || fmt == 0x79343230) {   /* '420v' '420f' 'y420': plano Y já é luminância */
        const u8 *yp = CVPixelBufferGetBaseAddressOfPlane(pb, 0);
        u64 bpr = CVPixelBufferGetBytesPerRowOfPlane(pb, 0);
        if (yp) {
            l->w = w; l->h = h; l->y = malloc((size_t)w * h);
            if (l->y) { for (int yy = 0; yy < h; yy++) memcpy(l->y + yy * w, yp + yy * bpr, (size_t)w); ok = YES; }
        }
    } else {
        const u8 *base = CVPixelBufferGetBaseAddress(pb);
        if (base) ok = luma_from_32(l, base, w, h, CVPixelBufferGetBytesPerRow(pb), fmt == 0x00000020 /* k32ARGB */);
    }
    CVPixelBufferUnlockBaseAddress(pb, 1);
    return ok;
}
/* bytes de um PNG/JPEG/BMP/GIF → ARGB pelo decodificador do sistema */
static BOOL luma_from_encoded(luma_t *l, const u8 *b, u64 n) {
    long need = SYS4(GUI_DECODE_IMAGE, b, n, 0, 0);
    if (need <= 8) return NO;
    u8 *out = malloc((size_t)need);
    if (!out) return NO;
    BOOL ok = NO;
    if (SYS4(GUI_DECODE_IMAGE, b, n, out, need) == need) {
        u32 w = *(u32 *)out, h = *(u32 *)(out + 4);
        if (w && h && 8 + (u64)w * h * 4 <= (u64)need) ok = luma_from_32(l, out + 8, (int)w, (int)h, (size_t)w * 4, 0) ;
        /* ARGB em u32 little-endian = bytes B G R A: o verde no byte 1, como o caminho RGBA/BGRA */
    }
    free(out);
    return ok;
}

/* ================================================================ binarização (média local por imagem integral) */
static u8 *binarize(const luma_t *l) {
    int w = l->w, h = l->h, r = (w < h ? w : h) / 16;
    if (r < 4) r = 4;
    u32 *ii = malloc(sizeof(u32) * (size_t)(w + 1) * (h + 1));
    u8 *bin = malloc((size_t)w * h);
    if (!ii || !bin) { free(ii); free(bin); return 0; }
    memset(ii, 0, sizeof(u32) * (size_t)(w + 1));
    for (int y = 0; y < h; y++) {
        u32 run = 0;
        ii[(size_t)(y + 1) * (w + 1)] = 0;
        for (int x = 0; x < w; x++) { run += l->y[y * w + x]; ii[(size_t)(y + 1) * (w + 1) + x + 1] = ii[(size_t)y * (w + 1) + x + 1] + run; }
    }
    for (int y = 0; y < h; y++) {
        int y0 = y - r < 0 ? 0 : y - r, y1 = y + r >= h ? h - 1 : y + r;
        for (int x = 0; x < w; x++) {
            int x0 = x - r < 0 ? 0 : x - r, x1 = x + r >= w ? w - 1 : x + r;
            u32 s = ii[(size_t)(y1 + 1) * (w + 1) + x1 + 1] - ii[(size_t)y0 * (w + 1) + x1 + 1] - ii[(size_t)(y1 + 1) * (w + 1) + x0] + ii[(size_t)y0 * (w + 1) + x0];
            u32 area = (u32)((x1 - x0 + 1) * (y1 - y0 + 1));
            u32 v = l->y[y * w + x];
            bin[y * w + x] = v * area * 100 < s * 92 ? 1 : 0;    /* 1 = escuro */
        }
    }
    free(ii);
    return bin;
}

/* ================================================================ Reed-Solomon (GF(256), polinômio 0x11D, raízes α^0..) */
static u8 gexp[512], glog[256];
static void gf_init(void) {
    if (gexp[0]) return;
    int x = 1;
    for (int i = 0; i < 255; i++) { gexp[i] = (u8)x; glog[x] = (u8)i; x <<= 1; if (x & 0x100) x ^= 0x11D; }
    for (int i = 255; i < 512; i++) gexp[i] = gexp[i - 255];
}
static u8 gmul(u8 a, u8 b) { return a && b ? gexp[glog[a] + glog[b]] : 0; }
static u8 gdiv(u8 a, u8 b) { return a ? gexp[(glog[a] + 255 - glog[b]) % 255] : 0; }
/* corrige msg[0..n) (coeficiente de maior grau primeiro) com nsym símbolos de paridade; -1 se não der */
static int rs_correct(u8 *msg, int n, int nsym) {
    gf_init();
    u8 S[80];
    int any = 0;
    for (int i = 0; i < nsym; i++) {
        u8 v = 0;
        for (int k = 0; k < n; k++) v = gmul(v, gexp[i]) ^ msg[k];
        S[i] = v;
        any |= v;
    }
    if (!any) return 0;
    u8 C[81] = {1}, B[81] = {1}, T[81];
    int L = 0, m = 1;
    u8 b = 1;
    for (int k = 0; k < nsym; k++) {
        u8 d = S[k];
        for (int i = 1; i <= L; i++) d ^= gmul(C[i], S[k - i]);
        if (!d) { m++; continue; }
        memcpy(T, C, sizeof C);
        u8 coef = gdiv(d, b);
        for (int i = 0; i + m <= nsym; i++) C[i + m] ^= gmul(coef, B[i]);
        if (2 * L <= k) { L = k + 1 - L; memcpy(B, T, sizeof B); b = d; m = 1; }
        else m++;
    }
    if (2 * L > nsym) return -1;
    int pos[40], np = 0;
    for (int p = 0; p < n; p++) {                                  /* Chien: Λ(α^-e) = 0 com e = grau da posição */
        int e = n - 1 - p;
        u8 xi = gexp[(255 - e % 255) % 255], v = 0, xp = 1;
        for (int i = 0; i <= L; i++) { v ^= gmul(C[i], xp); xp = gmul(xp, xi); }
        if (!v && np < 40) pos[np++] = p;
    }
    if (np != L) return -1;
    u8 O[80];                                                       /* Ω = S·Λ mod x^nsym */
    for (int i = 0; i < nsym; i++) { u8 v = 0; for (int j = 0; j <= i && j <= L; j++) v ^= gmul(C[j], S[i - j]); O[i] = v; }
    for (int k = 0; k < np; k++) {
        int e = n - 1 - pos[k];
        u8 X = gexp[e % 255], xi = gexp[(255 - e % 255) % 255];
        u8 om = 0, xp = 1;
        for (int i = 0; i < nsym; i++) { om ^= gmul(O[i], xp); xp = gmul(xp, xi); }
        u8 dl = 0;                                                  /* Λ'(x): só os termos ímpares */
        xp = 1;
        for (int i = 1; i <= L; i += 2) { dl ^= gmul(C[i], xp); xp = gmul(xp, gmul(xi, xi)); }
        if (!dl) return -1;
        msg[pos[k]] ^= gmul(X, gdiv(om, dl));
    }
    for (int i = 0; i < nsym; i++) {                                /* confere */
        u8 v = 0;
        for (int k = 0; k < n; k++) v = gmul(v, gexp[i]) ^ msg[k];
        if (v) return -1;
    }
    return np;
}

/* ================================================================ QR Code: tabelas (ISO/IEC 18004) */
static const signed char ECC_PER_BLOCK[4][41] = {   /* ordem L, M, Q, H */
    {-1, 7, 10, 15, 20, 26, 18, 20, 24, 30, 18, 20, 24, 26, 30, 22, 24, 28, 30, 28, 28, 28, 28, 30, 30, 26, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
    {-1, 10, 16, 26, 18, 24, 16, 18, 22, 22, 26, 30, 22, 22, 24, 24, 28, 28, 26, 26, 26, 26, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28, 28},
    {-1, 13, 22, 18, 26, 18, 24, 18, 22, 20, 24, 28, 26, 24, 20, 30, 24, 28, 28, 26, 30, 28, 30, 30, 30, 30, 28, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
    {-1, 17, 28, 22, 16, 22, 28, 26, 26, 24, 28, 24, 28, 22, 24, 24, 30, 28, 28, 26, 28, 30, 24, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30, 30},
};
static const signed char NUM_BLOCKS[4][41] = {
    {-1, 1, 1, 1, 1, 1, 2, 2, 2, 2, 4, 4, 4, 4, 4, 6, 6, 6, 6, 7, 8, 8, 9, 9, 10, 12, 12, 12, 13, 14, 15, 16, 17, 18, 19, 19, 20, 21, 22, 24, 25},
    {-1, 1, 1, 1, 2, 2, 4, 4, 4, 5, 5, 5, 8, 9, 9, 10, 10, 11, 13, 14, 16, 17, 17, 18, 20, 21, 23, 25, 26, 28, 29, 31, 33, 35, 37, 38, 40, 43, 45, 47, 49},
    {-1, 1, 1, 2, 2, 4, 4, 6, 6, 8, 8, 8, 10, 12, 16, 12, 17, 16, 18, 21, 20, 23, 23, 25, 27, 29, 34, 34, 35, 38, 40, 43, 45, 48, 51, 53, 56, 59, 62, 65, 68},
    {-1, 1, 1, 2, 4, 4, 4, 5, 6, 8, 8, 11, 11, 16, 16, 18, 16, 19, 21, 25, 25, 25, 34, 30, 32, 35, 37, 40, 42, 45, 48, 51, 54, 57, 60, 63, 66, 70, 74, 77, 81},
};
static int raw_modules(int v) {
    int r = (16 * v + 128) * v + 64;
    if (v >= 2) { int na = v / 7 + 2; r -= (25 * na - 10) * na - 55; if (v >= 7) r -= 36; }
    return r;
}
static int align_pos(int v, int *out) {
    if (v == 1) return 0;
    int na = v / 7 + 2, size = 4 * v + 17;
    int step = v == 32 ? 26 : (v * 4 + na * 2 + 1) / (na * 2 - 2) * 2;
    out[0] = 6;
    for (int i = na - 1, p = size - 7; i >= 1; i--, p -= step) out[i] = p;
    return na;
}
/* módulos de função (não carregam dados) */
static void mark_function(u8 *fn, int v) {
    int n = 4 * v + 17;
    memset(fn, 0, (size_t)n * n);
#define FN(x, y) fn[(y) * n + (x)] = 1
    for (int i = 0; i < n; i++) { FN(6, i); FN(i, 6); }
    for (int y = 0; y < 9; y++) for (int x = 0; x < 9; x++) { FN(x, y); if (n - 8 + x - 1 >= 0 && x < 8) FN(n - 8 + x, y); if (y < 8) FN(x, n - 8 + y); }
    for (int y = 0; y < 9; y++) for (int x = n - 8; x < n; x++) FN(x, y);
    for (int y = n - 8; y < n; y++) for (int x = 0; x < 9; x++) FN(x, y);
    int ap[8], na = align_pos(v, ap);
    for (int i = 0; i < na; i++)
        for (int j = 0; j < na; j++) {
            if ((i == 0 && j == 0) || (i == 0 && j == na - 1) || (i == na - 1 && j == 0)) continue;
            for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) FN(ap[i] + dx, ap[j] + dy);
        }
    if (v >= 7) for (int i = 0; i < 18; i++) { int a = n - 11 + i % 3, b = i / 3; FN(a, b); FN(b, a); }
#undef FN
}
static int mask_bit(int m, int x, int y) {
    switch (m) {
    case 0: return (x + y) % 2 == 0;
    case 1: return y % 2 == 0;
    case 2: return x % 3 == 0;
    case 3: return (x + y) % 3 == 0;
    case 4: return (x / 3 + y / 2) % 2 == 0;
    case 5: return x * y % 2 + x * y % 3 == 0;
    case 6: return (x * y % 2 + x * y % 3) % 2 == 0;
    default: return ((x + y) % 2 + x * y % 3) % 2 == 0;
    }
}
static int format_word(int ecl_bits, int mask) {
    int data = ecl_bits << 3 | mask, rem = data;
    for (int i = 0; i < 10; i++) rem = (rem << 1) ^ ((rem >> 9) * 0x537);
    return (data << 10 | rem) ^ 0x5412;
}
static int popcount(int x) { int c = 0; while (x) { c += x & 1; x >>= 1; } return c; }

/* ---- grade de módulos lida da imagem → texto */
typedef struct { char *s; int n, cap; } sbuf_t;
static void sb_put(sbuf_t *b, const char *p, int n) {
    if (b->n + n + 1 > b->cap) { int c = b->cap ? b->cap * 2 : 256; while (c < b->n + n + 1) c *= 2; char *q = malloc((size_t)c); if (!q) return; if (b->s) { memcpy(q, b->s, (size_t)b->n); free(b->s); } b->s = q; b->cap = c; }
    memcpy(b->s + b->n, p, (size_t)n); b->n += n; b->s[b->n] = 0;
}
static void sb_cp(sbuf_t *b, u32 cp) {
    char t[4]; int n;
    if (cp < 0x80) { t[0] = (char)cp; n = 1; }
    else if (cp < 0x800) { t[0] = (char)(0xC0 | cp >> 6); t[1] = (char)(0x80 | (cp & 63)); n = 2; }
    else { t[0] = (char)(0xE0 | cp >> 12); t[1] = (char)(0x80 | ((cp >> 6) & 63)); t[2] = (char)(0x80 | (cp & 63)); n = 3; }
    sb_put(b, t, n);
}
static int valid_utf8(const u8 *p, int n) {
    for (int i = 0; i < n;) {
        int k = p[i] < 0x80 ? 0 : (p[i] & 0xE0) == 0xC0 ? 1 : (p[i] & 0xF0) == 0xE0 ? 2 : (p[i] & 0xF8) == 0xF0 ? 3 : -1;
        if (k < 0 || i + k >= n + (k ? 0 : 1)) return 0;
        for (int j = 1; j <= k; j++) if (i + j >= n || (p[i + j] & 0xC0) != 0x80) return 0;
        i += k + 1;
    }
    return 1;
}
typedef struct { const u8 *d; int n, pos; } bits_t;
static int getb(bits_t *b, int k) {
    int v = 0;
    for (int i = 0; i < k; i++) { if (b->pos >= b->n * 8) return -1; v = v << 1 | ((b->d[b->pos >> 3] >> (7 - (b->pos & 7))) & 1); b->pos++; }
    return v;
}
static char *qr_parse(const u8 *data, int n, int v) {
    static const char ALNUM[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";
    bits_t b = {data, n, 0};
    sbuf_t out = {0};
    int grp = v <= 9 ? 0 : v <= 26 ? 1 : 2;
    for (;;) {
        int mode = getb(&b, 4);
        if (mode <= 0) break;
        if (mode == 7) { int e = getb(&b, 8); if (e & 0x80) getb(&b, (e & 0x40) ? 16 : 8); continue; }   /* ECI: o texto continua em UTF-8 */
        if (mode == 1) {
            static const int cb[3] = {10, 12, 14};
            int c = getb(&b, cb[grp]);
            if (c < 0) break;
            for (; c >= 3; c -= 3) { int x = getb(&b, 10); if (x < 0) break; char t[3] = {(char)('0' + x / 100), (char)('0' + x / 10 % 10), (char)('0' + x % 10)}; sb_put(&out, t, 3); }
            if (c == 2) { int x = getb(&b, 7); char t[2] = {(char)('0' + x / 10), (char)('0' + x % 10)}; sb_put(&out, t, 2); }
            else if (c == 1) { int x = getb(&b, 4); char t = (char)('0' + x); sb_put(&out, &t, 1); }
        } else if (mode == 2) {
            static const int cb[3] = {9, 11, 13};
            int c = getb(&b, cb[grp]);
            if (c < 0) break;
            for (; c >= 2; c -= 2) { int x = getb(&b, 11); if (x < 0 || x >= 45 * 45) break; char t[2] = {ALNUM[x / 45], ALNUM[x % 45]}; sb_put(&out, t, 2); }
            if (c == 1) { int x = getb(&b, 6); if (x >= 0 && x < 45) sb_put(&out, &ALNUM[x], 1); }
        } else if (mode == 4) {
            static const int cb[3] = {8, 16, 16};
            int c = getb(&b, cb[grp]);
            if (c < 0) break;
            u8 *tmp = malloc((size_t)c + 1);
            int k = 0;
            for (; k < c; k++) { int x = getb(&b, 8); if (x < 0) break; tmp[k] = (u8)x; }
            if (valid_utf8(tmp, k)) sb_put(&out, (const char *)tmp, k);
            else for (int i = 0; i < k; i++) sb_cp(&out, tmp[i]);          /* Latin-1 */
            free(tmp);
        } else if (mode == 8) {
            static const int cb[3] = {8, 10, 12};
            int c = getb(&b, cb[grp]);
            for (; c > 0; c--) { int k = getb(&b, 13); if (k < 0) break; u32 cp = SJIS_QR[k]; sb_cp(&out, cp ? cp : 0xFFFD); }   /* Kanji: Shift-JIS → Unicode */
        } else break;
    }
    if (!out.s) sb_put(&out, "", 0);
    return out.s;
}
/* lê a grade n×n (1 = escuro) já amostrada e decodifica */
static char *qr_decode_grid(const u8 *g, int v) {
    int n = 4 * v + 17;
    /* formato: as duas cópias, a melhor (menor distância de Hamming entre as 32 palavras válidas) */
    int f1 = 0, f2 = 0;
#define G(x, y) (g[(y) * n + (x)])
    for (int i = 0; i <= 5; i++) f1 |= G(8, i) << i;
    f1 |= G(8, 7) << 6; f1 |= G(8, 8) << 7; f1 |= G(7, 8) << 8;
    for (int i = 9; i < 15; i++) f1 |= G(14 - i, 8) << i;
    for (int i = 0; i < 8; i++) f2 |= G(n - 1 - i, 8) << i;
    for (int i = 8; i < 15; i++) f2 |= G(8, n - 15 + i) << i;
    int best = -1, bd = 99;
    for (int e = 0; e < 4; e++)
        for (int m = 0; m < 8; m++) {
            int w = format_word(e, m), d = popcount(w ^ f1), d2 = popcount(w ^ f2);
            if (d2 < d) d = d2;
            if (d < bd) { bd = d; best = e << 3 | m; }
        }
    if (bd > 3) return 0;
    int ecl_bits = best >> 3, mask = best & 7;
    int ecl = ecl_bits == 1 ? 0 : ecl_bits == 0 ? 1 : ecl_bits == 3 ? 2 : 3;   /* bits do formato → L M Q H */
    u8 *fn = malloc((size_t)n * n);
    int total = raw_modules(v) / 8;
    u8 *cw = calloc((size_t)total + 1, 1);
    if (!fn || !cw) { free(fn); free(cw); return 0; }
    mark_function(fn, v);
    int bit = 0;
    for (int right = n - 1; right >= 1; right -= 2) {
        if (right == 6) right = 5;
        for (int vert = 0; vert < n; vert++)
            for (int j = 0; j < 2; j++) {
                int x = right - j, up = ((right + 1) & 2) == 0, y = up ? n - 1 - vert : vert;
                if (fn[y * n + x] || bit >= total * 8) continue;
                int val = G(x, y) ^ mask_bit(mask, x, y);
                if (val) cw[bit >> 3] |= (u8)(0x80 >> (bit & 7));
                bit++;
            }
    }
#undef G
    free(fn);
    int nb = NUM_BLOCKS[ecl][v], ecc = ECC_PER_BLOCK[ecl][v];
    int short_n = nb - total % nb, short_len = total / nb, short_data = short_len - ecc;
    int data_total = total - ecc * nb;
    u8 *blocks = calloc((size_t)nb * (short_len + 1), 1), *data = malloc((size_t)data_total + 1);
    if (!blocks || !data) { free(blocks); free(data); free(cw); return 0; }
    int k = 0;
    for (int i = 0; i <= short_len; i++)
        for (int j = 0; j < nb; j++) {
            if (i == short_data && j < short_n) continue;
            if (k < total) blocks[j * (short_len + 1) + i] = cw[k++];
        }
    int dk = 0, ok = 1;
    for (int j = 0; j < nb; j++) {
        u8 *bl = blocks + j * (short_len + 1);
        int len = j < short_n ? short_len : short_len + 1, dl = len - ecc;
        u8 tmp[200];
        if (j < short_n) { memcpy(tmp, bl, (size_t)short_data); memcpy(tmp + short_data, bl + short_data + 1, (size_t)ecc); }
        else memcpy(tmp, bl, (size_t)len);
        if (rs_correct(tmp, len, ecc) < 0) { ok = 0; break; }
        memcpy(data + dk, tmp, (size_t)dl);
        dk += dl;
    }
    char *s = ok ? qr_parse(data, dk, v) : 0;
    free(blocks); free(data); free(cw);
    return s;
}

/* ---- localizadores e geometria */
typedef struct { double x, y, m; int hits; } finder_t;
static int ratio_ok(const int *r, double *unit) {
    int tot = r[0] + r[1] + r[2] + r[3] + r[4];
    if (tot < 7) return 0;
    double u = tot / 7.0, var = u * 0.6;
    if (fabs(r[0] - u) > var || fabs(r[1] - u) > var || fabs(r[2] - 3 * u) > 3 * var || fabs(r[3] - u) > var || fabs(r[4] - u) > var) return 0;
    *unit = u;
    return 1;
}
/* corrida vertical/horizontal em torno de (cx, cy): devolve o centro refinado ou -1 */
static double cross(const u8 *bin, int w, int h, int cx, int cy, int dx, int dy, double *unit) {
    int r[5] = {0};
    int x = cx, y = cy;
#define B(xx, yy) (bin[(yy) * w + (xx)])
    if (x < 0 || y < 0 || x >= w || y >= h || !B(x, y)) return -1;
    while (x >= 0 && y >= 0 && x < w && y < h && B(x, y)) { r[2]++; x -= dx; y -= dy; }
    while (x >= 0 && y >= 0 && x < w && y < h && !B(x, y)) { r[1]++; x -= dx; y -= dy; }
    while (x >= 0 && y >= 0 && x < w && y < h && B(x, y)) { r[0]++; x -= dx; y -= dy; }
    int start = dx ? x + 1 : y + 1;
    x = cx + dx; y = cy + dy;
    while (x >= 0 && y >= 0 && x < w && y < h && B(x, y)) { r[2]++; x += dx; y += dy; }
    while (x >= 0 && y >= 0 && x < w && y < h && !B(x, y)) { r[3]++; x += dx; y += dy; }
    while (x >= 0 && y >= 0 && x < w && y < h && B(x, y)) { r[4]++; x += dx; y += dy; }
#undef B
    if (!ratio_ok(r, unit)) return -1;
    return start + r[0] + r[1] + r[2] / 2.0;
}

/* em cada linha: cinco corridas escuro-claro-escuro-claro-escuro em 1:1:3:1:1, confirmadas na vertical e de novo na
 * horizontal; candidatos próximos viram um só (média), e "hits" conta em quantas linhas apareceram */
static int find_finders(const u8 *bin, int w, int h, finder_t *f, int maxf) {
    int nf = 0;
    int *len = malloc(sizeof(int) * (size_t)(w + 1)), *st = malloc(sizeof(int) * (size_t)(w + 1));
    u8 *col = malloc((size_t)w + 1);
    if (!len || !st || !col) { free(len); free(st); free(col); return 0; }
    for (int y = 0; y < h; y++) {
        const u8 *row = bin + y * w;
        int nr = 0;
        for (int x = 0; x < w;) {
            int c = row[x], s = x;
            while (x < w && row[x] == c) x++;
            st[nr] = s; len[nr] = x - s; col[nr] = (u8)c; nr++;
        }
        for (int i = 4; i < nr; i++) {
            if (!col[i] || col[i - 1] || !col[i - 2] || col[i - 3] || !col[i - 4]) continue;
            int r[5] = {len[i - 4], len[i - 3], len[i - 2], len[i - 1], len[i]};
            double u;
            if (!ratio_ok(r, &u)) continue;
            int cx = st[i - 2] + len[i - 2] / 2;
            double vy = cross(bin, w, h, cx, y, 0, 1, &u);
            if (vy < 0) continue;
            double vx = cross(bin, w, h, cx, (int)vy, 1, 0, &u);
            if (vx < 0) continue;
            int merged = 0;
            for (int k = 0; k < nf; k++)
                if (fabs(f[k].x - vx) < f[k].m * 3 && fabs(f[k].y - vy) < f[k].m * 3) {
                    f[k].x = (f[k].x * f[k].hits + vx) / (f[k].hits + 1);
                    f[k].y = (f[k].y * f[k].hits + vy) / (f[k].hits + 1);
                    f[k].m = (f[k].m * f[k].hits + u) / (f[k].hits + 1);
                    f[k].hits++;
                    merged = 1;
                    break;
                }
            if (!merged && nf < maxf) { f[nf].x = vx; f[nf].y = vy; f[nf].m = u; f[nf].hits = 1; nf++; }
        }
    }
    free(len); free(st); free(col);
    return nf;
}

typedef struct { char *text; pt_t corner[4]; } qr_hit_t;   /* cantos: TL, TR, BR, BL em pixels */
static int qr_try(const u8 *bin, int w, int h, finder_t *a, finder_t *b, finder_t *c, qr_hit_t *out) {
    /* a = canto superior esquerdo (o ângulo reto); b e c em ordem horária */
    double abx = b->x - a->x, aby = b->y - a->y, acx = c->x - a->x, acy = c->y - a->y;
    if (abx * acy - aby * acx < 0) { finder_t *t = b; b = c; c = t; abx = b->x - a->x; aby = b->y - a->y; acx = c->x - a->x; acy = c->y - a->y; }
    double m = (a->m + b->m + c->m) / 3, d = (sqrt(abx * abx + aby * aby) + sqrt(acx * acx + acy * acy)) / 2 / m + 7;
    int dim0 = (int)((d - 17) / 4 + 0.5) * 4 + 17;
    for (int tryd = 0; tryd < 3; tryd++) {
        int dim = dim0 + (tryd == 1 ? 4 : tryd == 2 ? -4 : 0), v = (dim - 17) / 4;
        if (v < 1 || v > 40) continue;
        u8 *g = malloc((size_t)dim * dim);
        if (!g) return 0;
        double span = dim - 7;
        for (int yy = 0; yy < dim; yy++)
            for (int xx = 0; xx < dim; xx++) {
                double u = (xx + 0.5 - 3.5) / span, t = (yy + 0.5 - 3.5) / span;
                int px = (int)(a->x + u * abx + t * acx), py = (int)(a->y + u * aby + t * acy);
                g[yy * dim + xx] = px >= 0 && py >= 0 && px < w && py < h ? bin[py * w + px] : 0;
            }
        char *s = qr_decode_grid(g, v);
        free(g);
        if (s) {
            out->text = s;
            double k0 = -3.5 / span, k1 = (dim - 3.5) / span;
            pt_t P[4] = {{a->x + k0 * abx + k0 * acx, a->y + k0 * aby + k0 * acy}, {a->x + k1 * abx + k0 * acx, a->y + k1 * aby + k0 * acy},
                         {a->x + k1 * abx + k1 * acx, a->y + k1 * aby + k1 * acy}, {a->x + k0 * abx + k1 * acx, a->y + k0 * aby + k1 * acy}};
            memcpy(out->corner, P, sizeof P);
            return 1;
        }
    }
    return 0;
}
static int qr_find(const luma_t *l, qr_hit_t *hits, int maxh) {
    u8 *bin = binarize(l);
    if (!bin) return 0;
    finder_t f[64];
    int nf = find_finders(bin, l->w, l->h, f, 64), nh = 0;
    /* combinações de três com tamanhos de módulo parecidos e um ângulo quase reto */
    for (int i = 0; i < nf && nh < maxh; i++)
        for (int j = 0; j < nf && nh < maxh; j++)
            for (int k = j + 1; k < nf && nh < maxh; k++) {
                if (i == j || i == k || f[i].hits < 2 || f[j].hits < 2 || f[k].hits < 2) continue;
                double mm = f[i].m, r1 = f[j].m / mm, r2 = f[k].m / mm;
                if (r1 < 0.6 || r1 > 1.6 || r2 < 0.6 || r2 > 1.6) continue;
                double ax = f[j].x - f[i].x, ay = f[j].y - f[i].y, bx = f[k].x - f[i].x, by = f[k].y - f[i].y;
                double la = sqrt(ax * ax + ay * ay), lb = sqrt(bx * bx + by * by);
                if (la < 7 * mm || lb < 7 * mm || la / lb > 1.3 || lb / la > 1.3) continue;
                if (fabs((ax * bx + ay * by) / (la * lb)) > 0.25) continue;
                if (qr_try(bin, l->w, l->h, &f[i], &f[j], &f[k], &hits[nh])) {
                    int dup = 0;
                    for (int q = 0; q < nh; q++) if (!strcmp(hits[q].text, hits[nh].text)) dup = 1;
                    if (dup) free(hits[nh].text); else nh++;
                }
            }
    free(bin);
    return nh;
}

/* ================================================================ EAN-13 / UPC-A / EAN-8 */
static const u8 EAN_L[10][4] = {{3, 2, 1, 1}, {2, 2, 2, 1}, {2, 1, 2, 2}, {1, 4, 1, 1}, {1, 1, 3, 2}, {1, 2, 3, 1}, {1, 1, 1, 4}, {1, 3, 1, 2}, {1, 2, 1, 3}, {3, 1, 1, 2}};
static const char *EAN_PARITY[10] = {"LLLLLL", "LLGLGG", "LLGGLG", "LLGGGL", "LGLLGG", "LGGLLG", "LGGGLL", "LGLGLG", "LGLGGL", "LGGLGL"};
/* dígito de 4 larguras (7 módulos): melhor padrão L, G (L invertido) ou R (= L) */
static int ean_digit(const int *w, int *is_g) {
    double tot = w[0] + w[1] + w[2] + w[3], best = 1e9;
    int bd = -1;
    for (int g = 0; g < 2; g++)
        for (int d = 0; d < 10; d++) {
            double e = 0;
            for (int i = 0; i < 4; i++) { double want = g ? EAN_L[d][3 - i] : EAN_L[d][i]; e += fabs(w[i] * 7.0 / tot - want); }
            if (e < best) { best = e; bd = d; *is_g = g; }
        }
    return best < 1.6 ? bd : -1;
}
typedef struct { char text[16]; int sym; int x0, x1; } ean_hit_t;   /* sym: 13 ou 8 */
static int ean_line(const u8 *line, int n, ean_hit_t *out) {
    /* corridas, começando por espaço */
    int runs[1024], rn = 0, x = 0;
    int start_px[1024];
    while (x < n && rn < 1024) {
        int c = line[x], len = 0;
        start_px[rn] = x;
        while (x < n && line[x] == c) { x++; len++; }
        runs[rn++] = c ? len : -len;                                /* > 0 barra, < 0 espaço */
    }
    for (int i = 1; i + 3 < rn; i++) {
        if (runs[i] <= 0 || runs[i + 1] >= 0 || runs[i + 2] <= 0 || runs[i - 1] >= 0) continue;
        double u = (runs[i] - runs[i + 1] + runs[i + 2]) / 3.0;
        if (-runs[i - 1] < 3 * u) continue;                          /* zona de silêncio */
        for (int kind = 13; kind >= 8; kind -= 5) {
            int half = kind == 13 ? 6 : 4, need = 3 + 4 * half + 5 + 4 * half + 3;
            if (i + need > rn) continue;
            int p = i + 3, digits[13], nd = 0, parity = 0, ok = 1;
            for (int d = 0; d < half && ok; d++, p += 4) {
                int w[4] = {-runs[p], runs[p + 1], -runs[p + 2], runs[p + 3]}, g = 0;
                if (w[0] <= 0 || w[1] <= 0 || w[2] <= 0 || w[3] <= 0) { ok = 0; break; }
                int v = ean_digit(w, &g);
                if (v < 0 || (kind == 8 && g)) { ok = 0; break; }
                digits[nd++] = v;
                parity = parity << 1 | g;
            }
            if (!ok) continue;
            if (runs[p] >= 0 || runs[p + 1] <= 0 || runs[p + 2] >= 0 || runs[p + 3] <= 0 || runs[p + 4] >= 0) continue;   /* guarda do meio */
            p += 5;
            for (int d = 0; d < half && ok; d++, p += 4) {
                int w[4] = {runs[p], -runs[p + 1], runs[p + 2], -runs[p + 3]}, g = 0;
                if (w[0] <= 0 || w[1] <= 0 || w[2] <= 0 || w[3] <= 0) { ok = 0; break; }
                int v = ean_digit(w, &g);
                if (v < 0 || g) { ok = 0; break; }
                digits[nd++] = v;
            }
            if (!ok || runs[p] <= 0 || runs[p + 1] >= 0 || runs[p + 2] <= 0) continue;   /* guarda final */
            char t[16];
            int len = 0, all[13], na = 0;
            if (kind == 13) {
                int first = -1;
                for (int f = 0; f < 10; f++) {
                    int pv = 0;
                    for (int q = 0; q < 6; q++) pv = pv << 1 | (EAN_PARITY[f][q] == 'G');
                    if (pv == parity) first = f;
                }
                if (first < 0) continue;
                all[na++] = first;
            }
            for (int q = 0; q < nd; q++) all[na++] = digits[q];
            int sum = 0;
            for (int q = 0; q < na - 1; q++) sum += all[q] * ((na - 1 - q) % 2 ? 3 : 1);
            if ((10 - sum % 10) % 10 != all[na - 1]) continue;       /* dígito verificador */
            for (int q = 0; q < na; q++) t[len++] = (char)('0' + all[q]);
            t[len] = 0;
            memcpy(out->text, t, (size_t)len + 1);
            out->sym = kind;
            out->x0 = start_px[i];
            out->x1 = start_px[p + 2] + runs[p + 2];
            return 1;
        }
    }
    return 0;
}

/* ================================================================ Code 128 (conjuntos A, B e C, FNC1, shift) */
static const char *C128[107] = {
    "212222", "222122", "222221", "121223", "121322", "131222", "122213", "122312", "132212", "221213", "221312", "231212",
    "112232", "122132", "122231", "113222", "123122", "123221", "223211", "221132", "221231", "213212", "223112", "312131",
    "311222", "321122", "321221", "312212", "322112", "322211", "212123", "212321", "232121", "111323", "131123", "131321",
    "112313", "132113", "132311", "211313", "231113", "231311", "112133", "112331", "132131", "113123", "113321", "133121",
    "313121", "211331", "231131", "213113", "213311", "213131", "311123", "311321", "331121", "312113", "312311", "332111",
    "314111", "221411", "431111", "111224", "111422", "121124", "121421", "141122", "141221", "112214", "112412", "122114",
    "122411", "142112", "142211", "241211", "221114", "413111", "241112", "134111", "111242", "121142", "121241", "114212",
    "124112", "124211", "411212", "421112", "421211", "212141", "214121", "412121", "111143", "111341", "131141", "114113",
    "114311", "411113", "411311", "113141", "114131", "311141", "411131", "211412", "211214", "211232", "2331112",
};
/* 6 larguras (barra, espaço, ...) → valor 0..105, ou -1 */
static double c128_err;
static int c128_symbol(const int *w) {
    double tot = 0, best = 1e9;
    for (int i = 0; i < 6; i++) tot += w[i];
    int bv = -1;
    for (int v = 0; v < 106; v++) {
        double e = 0;
        for (int i = 0; i < 6; i++) e += fabs(w[i] * 11.0 / tot - (C128[v][i] - '0'));
        if (e < best) { best = e; bv = v; }
    }
    c128_err = best;
    return best < 2.2 ? bv : -1;
}
static int c128_line(const u8 *line, int n, ean_hit_t *out, char *text, int cap) {
    int runs[1024], start_px[1024], rn = 0, x = 0;
    while (x < n && rn < 1024) {
        int c = line[x], len = 0;
        start_px[rn] = x;
        while (x < n && line[x] == c) { x++; len++; }
        runs[rn++] = len * (c ? 1 : -1);
    }
    for (int i = 1; i + 13 < rn; i++) {
        if (runs[i] <= 0 || runs[i - 1] >= 0) continue;
        int w[6];
        for (int k = 0; k < 6; k++) w[k] = runs[i + k] < 0 ? -runs[i + k] : runs[i + k];
        int st = c128_symbol(w);
        if (st < 103 || st > 105) continue;
        double unit = (w[0] + w[1] + w[2] + w[3] + w[4] + w[5]) / 11.0;
        if (-runs[i - 1] < 5 * unit) continue;                       /* zona de silêncio de 10 módulos (folga) */
        int vals[256], nv = 0, p = i + 6, ok = 0;
        while (p + 6 < rn && nv < 256) {
            for (int k = 0; k < 6; k++) w[k] = runs[p + k] < 0 ? -runs[p + k] : runs[p + k];
            /* parada: 2331112 (7 elementos) */
            if (p + 7 <= rn) {
                int s7[7], t7 = 0;
                for (int k = 0; k < 7; k++) { s7[k] = runs[p + k] < 0 ? -runs[p + k] : runs[p + k]; t7 += s7[k]; }
                double e = 0;
                for (int k = 0; k < 7; k++) e += fabs(s7[k] * 13.0 / t7 - (C128[106][k] - '0'));
                c128_symbol(w);
                if (e < 1.5 && e < c128_err && runs[p] > 0) { ok = 1; break; }
            }
            int v = c128_symbol(w);
            if (v < 0 || v > 102) break;
            vals[nv++] = v;
            p += 6;
        }
        if (!ok || nv < 2) continue;
        int sum = st;
        for (int k = 0; k < nv - 1; k++) sum += (k + 1) * vals[k];
        if (sum % 103 != vals[nv - 1]) continue;                    /* dígito verificador */
        int set = st - 103, shift = -1, len = 0;                    /* 0 = A, 1 = B, 2 = C */
        for (int k = 0; k < nv - 1 && len + 3 < cap; k++) {
            int v = vals[k], cur = shift >= 0 ? shift : set;
            shift = -1;
            if (cur == 2) {
                if (v < 100) { text[len++] = (char)('0' + v / 10); text[len++] = (char)('0' + v % 10); }
                else if (v == 100) set = 1; else if (v == 101) set = 0;   /* 102 = FNC1: separador GS1, omitido no início */
                else if (v == 102 && k) text[len++] = 0x1D;
                continue;
            }
            if (v < 96) {
                int ch = cur == 0 ? (v < 64 ? v + 32 : v - 64) : v + 32;
                text[len++] = (char)(ch == 127 ? '?' : ch);
            } else if (v == 98) shift = cur == 0 ? 1 : 0;
            else if (v == 99) set = 2;
            else if (v == 100 && cur == 0) set = 1;
            else if (v == 101 && cur == 1) set = 0;
            else if (v == 102 && k) text[len++] = 0x1D;
        }
        text[len] = 0;
        out->sym = 128;
        out->x0 = start_px[i];
        out->x1 = start_px[p + 6] + (runs[p + 6] > 0 ? runs[p + 6] : 0);
        return 1;
    }
    return 0;
}

/* ================================================================ retângulos (VNDetectRectanglesRequest)
 * Bordas por Sobel, componentes conexos de borda, e para cada um o quadrilátero dos pontos extremos (min/max de
 * x+y e x−y); vale se quase todo ponto da borda estiver perto dos quatro lados e se o tamanho/proporção baterem
 * com o pedido (minimumSize, minimumAspectRatio, maximumAspectRatio, quadratureTolerance). */
typedef struct { pt_t c[4]; double score, area; } quad_t;
static double seg_dist(pt_t p, pt_t a, pt_t b) {
    double dx = b.x - a.x, dy = b.y - a.y, l = dx * dx + dy * dy, t = l ? ((p.x - a.x) * dx + (p.y - a.y) * dy) / l : 0;
    if (t < 0) t = 0; if (t > 1) t = 1;
    double ex = a.x + t * dx - p.x, ey = a.y + t * dy - p.y;
    return sqrt(ex * ex + ey * ey);
}
static int find_quads(const luma_t *l, quad_t *out, int maxq, double min_size, double min_ar, double max_ar, double tol_deg) {
    int w = l->w, h = l->h, nq = 0;
    u8 *edge = calloc((size_t)w * h, 1);
    int *lab = calloc((size_t)w * h, sizeof(int)), *stack = malloc(sizeof(int) * (size_t)w * h);
    if (!edge || !lab || !stack) { free(edge); free(lab); free(stack); return 0; }
    for (int y = 1; y < h - 1; y++)
        for (int x = 1; x < w - 1; x++) {
            const u8 *p = l->y + y * w + x;
            int gx = -p[-w - 1] - 2 * p[-1] - p[w - 1] + p[-w + 1] + 2 * p[1] + p[w + 1];
            int gy = -p[-w - 1] - 2 * p[-w] - p[-w + 1] + p[w - 1] + 2 * p[w] + p[w + 1];
            edge[y * w + x] = gx * gx + gy * gy > 120 * 120;
        }
    int next = 0;
    for (int s = 0; s < w * h && nq < maxq; s++) {
        if (!edge[s] || lab[s]) continue;
        int sp = 0, np = 0;
        stack[sp++] = s; lab[s] = ++next;
        int *pts = malloc(sizeof(int) * 65536);
        if (!pts) break;
        while (sp) {
            int q = stack[--sp];
            if (np < 65536) pts[np++] = q;
            int qx = q % w, qy = q / w;
            for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
                int nx = qx + dx, ny = qy + dy;
                if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                int r = ny * w + nx;
                if (edge[r] && !lab[r]) { lab[r] = next; stack[sp++] = r; }
            }
        }
        if (np < 40) { free(pts); continue; }
        pt_t c[4];
        double ext[4] = {1e18, -1e18, 1e18, 1e18};                  /* min x+y (TL), max x−y (TR), max x+y (BR), min x−y (BL) */
        for (int k = 0; k < np; k++) {
            double x = pts[k] % w, y = pts[k] / w;
            if (x + y < ext[0]) { ext[0] = x + y; c[0] = (pt_t){x, y}; }
            if (x - y > ext[1]) { ext[1] = x - y; c[1] = (pt_t){x, y}; }
            if (-(x + y) < ext[2]) { ext[2] = -(x + y); c[2] = (pt_t){x, y}; }
            if (x - y < ext[3]) { ext[3] = x - y; c[3] = (pt_t){x, y}; }
        }
        double sw = (sqrt((c[1].x - c[0].x) * (c[1].x - c[0].x) + (c[1].y - c[0].y) * (c[1].y - c[0].y)) + sqrt((c[2].x - c[3].x) * (c[2].x - c[3].x) + (c[2].y - c[3].y) * (c[2].y - c[3].y))) / 2;
        double shh = (sqrt((c[3].x - c[0].x) * (c[3].x - c[0].x) + (c[3].y - c[0].y) * (c[3].y - c[0].y)) + sqrt((c[2].x - c[1].x) * (c[2].x - c[1].x) + (c[2].y - c[1].y) * (c[2].y - c[1].y))) / 2;
        double small = sw < shh ? sw : shh, big = sw < shh ? shh : sw, dim = w < h ? w : h;
        int near = 0;
        for (int k = 0; k < np; k++) {
            pt_t p = {pts[k] % w, pts[k] / w};
            double d = seg_dist(p, c[0], c[1]), d2 = seg_dist(p, c[1], c[2]), d3 = seg_dist(p, c[2], c[3]), d4 = seg_dist(p, c[3], c[0]);
            if (d2 < d) d = d2; if (d3 < d) d = d3; if (d4 < d) d = d4;
            if (d < 3) near++;
        }
        /* ângulos internos perto de 90° (quadratureTolerance) */
        int square = 1;
        for (int k = 0; k < 4; k++) {
            pt_t a = c[(k + 3) % 4], b = c[k], d = c[(k + 1) % 4];
            double ux = a.x - b.x, uy = a.y - b.y, vx = d.x - b.x, vy = d.y - b.y;
            double cs = (ux * vx + uy * vy) / (sqrt(ux * ux + uy * uy) * sqrt(vx * vx + vy * vy) + 1e-9);
            if (fabs(cs) > __builtin_sin(tol_deg * 3.14159265 / 180)) square = 0;
        }
        double ar = big > 0 ? small / big : 0;
        if (square && near > np * 9 / 10 && big >= min_size * dim && ar >= min_ar && ar <= max_ar && small > 8) {
            quad_t *q = &out[nq++];
            memcpy(q->c, c, sizeof c);
            q->score = (double)near / np;
            q->area = sw * shh;
        }
        free(pts);
    }
    free(edge); free(lab); free(stack);
    /* maiores primeiro */
    for (int i = 1; i < nq; i++) for (int j = i; j > 0 && out[j].area > out[j - 1].area; j--) { quad_t t = out[j]; out[j] = out[j - 1]; out[j - 1] = t; }
    return nq;
}

/* ================================================================ classes do Vision */
id VNBarcodeSymbologyQR, VNBarcodeSymbologyEAN13, VNBarcodeSymbologyEAN8, VNBarcodeSymbologyUPCE, VNBarcodeSymbologyAztec,
   VNBarcodeSymbologyCode128, VNBarcodeSymbologyCode39, VNBarcodeSymbologyCode39Checksum, VNBarcodeSymbologyCode39FullASCII,
   VNBarcodeSymbologyCode39FullASCIIChecksum, VNBarcodeSymbologyCode93, VNBarcodeSymbologyCode93i, VNBarcodeSymbologyDataMatrix,
   VNBarcodeSymbologyI2of5, VNBarcodeSymbologyI2of5Checksum, VNBarcodeSymbologyITF14, VNBarcodeSymbologyPDF417,
   VNBarcodeSymbologyCodabar, VNBarcodeSymbologyGS1DataBar, VNBarcodeSymbologyMicroQR, VNBarcodeSymbologyMicroPDF417;
id VNErrorDomain, VNImageOptionProperties, VNImageOptionCameraIntrinsics, VNImageOptionCIContext;

cx_class_t VNRequest_c __asm__("OBJC_CLASS_$_VNRequest"), VNRequest_m __asm__("OBJC_METACLASS_$_VNRequest");
cx_class_t VNImageBasedRequest_c __asm__("OBJC_CLASS_$_VNImageBasedRequest"), VNImageBasedRequest_m __asm__("OBJC_METACLASS_$_VNImageBasedRequest");
cx_class_t VNDetectBarcodesRequest_c __asm__("OBJC_CLASS_$_VNDetectBarcodesRequest"), VNDetectBarcodesRequest_m __asm__("OBJC_METACLASS_$_VNDetectBarcodesRequest");
cx_class_t VNDetectFaceRectanglesRequest_c __asm__("OBJC_CLASS_$_VNDetectFaceRectanglesRequest"), VNDetectFaceRectanglesRequest_m __asm__("OBJC_METACLASS_$_VNDetectFaceRectanglesRequest");
cx_class_t VNDetectFaceLandmarksRequest_c __asm__("OBJC_CLASS_$_VNDetectFaceLandmarksRequest"), VNDetectFaceLandmarksRequest_m __asm__("OBJC_METACLASS_$_VNDetectFaceLandmarksRequest");
cx_class_t VNRecognizeTextRequest_c __asm__("OBJC_CLASS_$_VNRecognizeTextRequest"), VNRecognizeTextRequest_m __asm__("OBJC_METACLASS_$_VNRecognizeTextRequest");
cx_class_t VNDetectRectanglesRequest_c __asm__("OBJC_CLASS_$_VNDetectRectanglesRequest"), VNDetectRectanglesRequest_m __asm__("OBJC_METACLASS_$_VNDetectRectanglesRequest");
cx_class_t VNClassifyImageRequest_c __asm__("OBJC_CLASS_$_VNClassifyImageRequest"), VNClassifyImageRequest_m __asm__("OBJC_METACLASS_$_VNClassifyImageRequest");
cx_class_t VNGenerateImageFeaturePrintRequest_c __asm__("OBJC_CLASS_$_VNGenerateImageFeaturePrintRequest"), VNGenerateImageFeaturePrintRequest_m __asm__("OBJC_METACLASS_$_VNGenerateImageFeaturePrintRequest");
cx_class_t VNImageRequestHandler_c __asm__("OBJC_CLASS_$_VNImageRequestHandler"), VNImageRequestHandler_m __asm__("OBJC_METACLASS_$_VNImageRequestHandler");
cx_class_t VNSequenceRequestHandler_c __asm__("OBJC_CLASS_$_VNSequenceRequestHandler"), VNSequenceRequestHandler_m __asm__("OBJC_METACLASS_$_VNSequenceRequestHandler");
cx_class_t VNObservation_c __asm__("OBJC_CLASS_$_VNObservation"), VNObservation_m __asm__("OBJC_METACLASS_$_VNObservation");
cx_class_t VNDetectedObjectObservation_c __asm__("OBJC_CLASS_$_VNDetectedObjectObservation"), VNDetectedObjectObservation_m __asm__("OBJC_METACLASS_$_VNDetectedObjectObservation");
cx_class_t VNRectangleObservation_c __asm__("OBJC_CLASS_$_VNRectangleObservation"), VNRectangleObservation_m __asm__("OBJC_METACLASS_$_VNRectangleObservation");
cx_class_t VNBarcodeObservation_c __asm__("OBJC_CLASS_$_VNBarcodeObservation"), VNBarcodeObservation_m __asm__("OBJC_METACLASS_$_VNBarcodeObservation");
cx_class_t VNFaceObservation_c __asm__("OBJC_CLASS_$_VNFaceObservation"), VNFaceObservation_m __asm__("OBJC_METACLASS_$_VNFaceObservation");
cx_class_t VNFeaturePrintObservation_c __asm__("OBJC_CLASS_$_VNFeaturePrintObservation"), VNFeaturePrintObservation_m __asm__("OBJC_METACLASS_$_VNFeaturePrintObservation");

enum { RQ_BARCODE = 1, RQ_FACE, RQ_LANDMARKS, RQ_TEXT, RQ_RECT, RQ_CLASSIFY, RQ_FEATURE };
typedef struct {
    void *isa;
    int kind;
    id results, symbologies;
    void *completion;
    i64 revision;
    BOOL cpu_only, background, cancelled;
    rc_t roi;
    /* texto: só guardados (sem modelo) */
    id languages, custom_words;
    i64 level;
    BOOL lang_correction;
    float min_height;
    /* retângulos */
    float rmin_ar, rmax_ar, rtol, rmin_size, rmin_conf;
    i64 rmax_obs;
} vnreq_t;
typedef struct {
    void *isa;
    rc_t box;
    float confidence;
    id uuid, payload, symbology;
    pt_t tl, tr, br, bl;
    float *fp;
    int fpn;
} vnobs_t;
#define RQ(o) ((vnreq_t *)(o))
#define OB(o) ((vnobs_t *)(o))
void *_Block_copy(const void *b);
void _Block_release(const void *b);
typedef struct { void *isa; int flags, reserved; void *invoke; } blk_t;

static id req_init_kind(id s, int kind, void *completion) {
    vnreq_t *r = RQ(s);
    r->kind = kind;
    r->roi = (rc_t){0, 0, 1, 1};
    r->revision = 1;
    if (completion) r->completion = _Block_copy(completion);
    if (kind == RQ_BARCODE) {
        r->symbologies = ret(MSG(id, cls("NSArray"), "arrayWithObjects:count:", ((id[]){VNBarcodeSymbologyQR, VNBarcodeSymbologyEAN13, VNBarcodeSymbologyEAN8, VNBarcodeSymbologyCode128}), 4ull));
    }
    r->level = 0;
    r->rmin_ar = 0.5f; r->rmax_ar = 1.0f; r->rtol = 30; r->rmin_size = 0.2f; r->rmin_conf = 0; r->rmax_obs = 1;   /* padrões da Apple */
    return s;
}
static int kind_of(id s) {
    cx_class_t *k = object_getClass(s);
    if (cx_is_subclass(k, &VNDetectBarcodesRequest_c)) return RQ_BARCODE;
    if (cx_is_subclass(k, &VNDetectFaceLandmarksRequest_c)) return RQ_LANDMARKS;
    if (cx_is_subclass(k, &VNDetectFaceRectanglesRequest_c)) return RQ_FACE;
    if (cx_is_subclass(k, &VNRecognizeTextRequest_c)) return RQ_TEXT;
    if (cx_is_subclass(k, &VNDetectRectanglesRequest_c)) return RQ_RECT;
    if (cx_is_subclass(k, &VNClassifyImageRequest_c)) return RQ_CLASSIFY;
    if (cx_is_subclass(k, &VNGenerateImageFeaturePrintRequest_c)) return RQ_FEATURE;
    return 0;
}
static id req_init(id s, SEL c) { (void)c; return req_init_kind(s, kind_of(s), 0); }
static id req_init_completion(id s, SEL c, void *h) { (void)c; return req_init_kind(s, kind_of(s), h); }
static id req_results(id s, SEL c) { (void)c; return RQ(s)->results; }
static void *req_completion(id s, SEL c) { (void)c; return RQ(s)->completion; }
static i64 req_revision(id s, SEL c) { (void)c; return RQ(s)->revision; }
static void req_set_revision(id s, SEL c, i64 v) { (void)c; RQ(s)->revision = v; }
static BOOL req_cpu(id s, SEL c) { (void)c; return RQ(s)->cpu_only; }
static void req_set_cpu(id s, SEL c, BOOL b) { (void)c; RQ(s)->cpu_only = b; }
static BOOL req_bg(id s, SEL c) { (void)c; return RQ(s)->background; }
static void req_set_bg(id s, SEL c, BOOL b) { (void)c; RQ(s)->background = b; }
static void req_cancel(id s, SEL c) { (void)c; RQ(s)->cancelled = YES; }
static rc_t req_roi(id s, SEL c) { (void)c; return RQ(s)->roi; }
static void req_set_roi(id s, SEL c, rc_t r) { (void)c; RQ(s)->roi = r; }
static id req_symbologies(id s, SEL c) { (void)c; return RQ(s)->symbologies; }
static void req_set_symbologies(id s, SEL c, id a) { (void)c; setobj(&RQ(s)->symbologies, a); }
static id supported_symbologies(void) { return MSG(id, cls("NSArray"), "arrayWithObjects:count:", ((id[]){VNBarcodeSymbologyQR, VNBarcodeSymbologyEAN13, VNBarcodeSymbologyEAN8, VNBarcodeSymbologyCode128}), 4ull); }
static id req_supported_sym_c(id k, SEL c) { (void)k; (void)c; return supported_symbologies(); }
static id req_supported_sym_err(id s, SEL c, id *err) { (void)s; (void)c; if (err) *err = 0; return supported_symbologies(); }
static id req_langs(id s, SEL c) { (void)c; return RQ(s)->languages ? RQ(s)->languages : MSG(id, cls("NSArray"), "arrayWithObject:", cx_str("en-US")); }
static void req_set_langs(id s, SEL c, id a) { (void)c; setobj(&RQ(s)->languages, a); }
static id req_supported_langs(id s, SEL c, id *err) { (void)s; (void)c; if (err) *err = 0; return MSG(id, cls("NSArray"), "arrayWithObject:", cx_str("en-US")); }
static id req_words(id s, SEL c) { (void)c; return RQ(s)->custom_words ? RQ(s)->custom_words : MSG0(id, cls("NSArray"), "array"); }
static void req_set_words(id s, SEL c, id a) { (void)c; setobj(&RQ(s)->custom_words, a); }
static i64 req_level(id s, SEL c) { (void)c; return RQ(s)->level; }
static void req_set_level(id s, SEL c, i64 v) { (void)c; RQ(s)->level = v; }
static BOOL req_lc(id s, SEL c) { (void)c; return RQ(s)->lang_correction; }
static void req_set_lc(id s, SEL c, BOOL b) { (void)c; RQ(s)->lang_correction = b; }
static float req_minh(id s, SEL c) { (void)c; return RQ(s)->min_height; }
#define RF(get, set, field) static float get(id s, SEL c) { (void)c; return RQ(s)->field; } static void set(id s, SEL c, float v) { (void)c; RQ(s)->field = v; }
RF(rq_min_ar, rq_set_min_ar, rmin_ar) RF(rq_max_ar, rq_set_max_ar, rmax_ar) RF(rq_tol, rq_set_tol, rtol)
RF(rq_min_size, rq_set_min_size, rmin_size) RF(rq_min_conf, rq_set_min_conf, rmin_conf)
static u64 rq_max_obs(id s, SEL c) { (void)c; return (u64)RQ(s)->rmax_obs; }
static void rq_set_max_obs(id s, SEL c, u64 v) { (void)c; RQ(s)->rmax_obs = (i64)v; }
static void req_set_minh(id s, SEL c, float v) { (void)c; RQ(s)->min_height = v; }

/* observações */
static id obs_new(cx_class_t *k, rc_t box) {
    vnobs_t *o = (vnobs_t *)cx_alloc_instance(k);
    o->box = box;
    o->confidence = 1.0f;
    o->uuid = ret(objc_autorelease(MSG0(id, MSG0(id, cls("NSUUID"), "alloc"), "init")));
    return objc_autorelease((id)o);
}
static rc_t ob_box(id s, SEL c) { (void)c; return OB(s)->box; }
static float ob_conf(id s, SEL c) { (void)c; return OB(s)->confidence; }
static id ob_uuid(id s, SEL c) { (void)c; return OB(s)->uuid; }
static double ob_time(id s, SEL c) { (void)s; (void)c; return 0; }
static pt_t ob_tl(id s, SEL c) { (void)c; return OB(s)->tl; }
static pt_t ob_tr(id s, SEL c) { (void)c; return OB(s)->tr; }
static pt_t ob_br(id s, SEL c) { (void)c; return OB(s)->br; }
static pt_t ob_bl(id s, SEL c) { (void)c; return OB(s)->bl; }
static id ob_payload(id s, SEL c) { (void)c; return OB(s)->payload; }
static id ob_symbology(id s, SEL c) { (void)c; return OB(s)->symbology; }
static id ob_nil(id s, SEL c) { (void)s; (void)c; return 0; }
static id ob_zero_num(id s, SEL c) { (void)s; (void)c; return MSG(id, cls("NSNumber"), "numberWithDouble:", 0.0); }
static i64 fp_count(id s, SEL c) { (void)c; return OB(s)->fpn; }
static u64 fp_type(id s, SEL c) { (void)s; (void)c; return 1; }        /* VNElementTypeFloat */
static id fp_data(id s, SEL c) { (void)c; return MSG(id, cls("NSData"), "dataWithBytes:length:", OB(s)->fp, (u64)OB(s)->fpn * 4); }
static BOOL fp_distance(id s, SEL c, float *out, id other, id *err) {
    (void)c;
    if (err) *err = 0;
    if (!other || OB(other)->fpn != OB(s)->fpn || !OB(s)->fp) return NO;
    double d = 0;
    for (int i = 0; i < OB(s)->fpn; i++) { double t = OB(s)->fp[i] - OB(other)->fp[i]; d += t * t; }
    if (out) *out = (float)sqrt(d);
    return YES;
}

/* ---- execução */
static rc_t norm_box(const pt_t *P, int n, int w, int h, pt_t *outP) {
    double x0 = 1e9, y0 = 1e9, x1 = -1e9, y1 = -1e9;
    for (int i = 0; i < n; i++) {
        double x = P[i].x / w, y = 1 - P[i].y / h;                 /* origem embaixo à esquerda, como no Vision */
        if (outP) outP[i] = (pt_t){x, y};
        if (x < x0) x0 = x; if (y < y0) y0 = y; if (x > x1) x1 = x; if (y > y1) y1 = y;
    }
    return (rc_t){x0 < 0 ? 0 : x0, y0 < 0 ? 0 : y0, (x1 > 1 ? 1 : x1) - (x0 < 0 ? 0 : x0), (y1 > 1 ? 1 : y1) - (y0 < 0 ? 0 : y0)};
}
static BOOL wants(vnreq_t *r, id sym) {
    if (!r->symbologies || !cnt(r->symbologies)) return YES;
    for (u64 i = 0; i < cnt(r->symbologies); i++) if (MSG(BOOL, at(r->symbologies, i), "isEqualToString:", sym)) return YES;
    return NO;
}
static void run_barcodes(vnreq_t *r, const luma_t *l, id results) {
    if (wants(r, VNBarcodeSymbologyQR)) {
        qr_hit_t hits[8];
        int n = qr_find(l, hits, 8);
        for (int i = 0; i < n; i++) {
            pt_t P[4];
            rc_t box = norm_box(hits[i].corner, 4, l->w, l->h, P);
            id o = obs_new(&VNBarcodeObservation_c, box);
            OB(o)->tl = P[0]; OB(o)->tr = P[1]; OB(o)->br = P[2]; OB(o)->bl = P[3];
            OB(o)->payload = ret(cx_str(hits[i].text));
            OB(o)->symbology = ret(VNBarcodeSymbologyQR);
            MSG(void, results, "addObject:", o);
            free(hits[i].text);
        }
    }
    BOOL e13 = wants(r, VNBarcodeSymbologyEAN13), e8 = wants(r, VNBarcodeSymbologyEAN8), c128 = wants(r, VNBarcodeSymbologyCode128);
    if (!e13 && !e8 && !c128) return;
    u8 *bin = binarize(l);
    if (!bin) return;
    char seen[8][260];
    int ns = 0;
    for (int pass = 0; pass < 2; pass++) {                          /* linhas e depois colunas (código em pé) */
        int lines = pass ? l->w : l->h, len = pass ? l->h : l->w;
        u8 *buf = malloc((size_t)len);
        for (int k = 1; k < 24 && buf; k++) {
            int idx = lines * k / 24;
            for (int i = 0; i < len; i++) buf[i] = pass ? bin[i * l->w + idx] : bin[idx * l->w + i];
            /* também de trás para frente (código de cabeça para baixo) */
            for (int rev = 0; rev < 2; rev++) {
                if (rev) for (int i = 0; i < len / 2; i++) { u8 t = buf[i]; buf[i] = buf[len - 1 - i]; buf[len - 1 - i] = t; }
                for (int kind = 0; kind < 2; kind++) {
                    ean_hit_t h;
                    char text[260];
                    if (kind == 0) {
                        if ((!e13 && !e8) || !ean_line(buf, len, &h)) continue;
                        if ((h.sym == 13 && !e13) || (h.sym == 8 && !e8)) continue;
                        memcpy(text, h.text, sizeof h.text);
                    } else if (!c128 || !c128_line(buf, len, &h, text, sizeof text)) continue;
                    int dup = 0;
                    for (int q = 0; q < ns; q++) if (!strcmp(seen[q], text)) dup = 1;
                    if (dup || ns >= 8) continue;
                    memcpy(seen[ns++], text, strlen(text) + 1);
                    int a = rev ? len - 1 - h.x1 : h.x0, b = rev ? len - 1 - h.x0 : h.x1;
                    pt_t P[4];
                    double t0 = idx - lines / 48.0, t1 = idx + lines / 48.0;
                    if (!pass) { P[0] = (pt_t){a, t0}; P[1] = (pt_t){b, t0}; P[2] = (pt_t){b, t1}; P[3] = (pt_t){a, t1}; }
                    else { P[0] = (pt_t){t0, a}; P[1] = (pt_t){t1, a}; P[2] = (pt_t){t1, b}; P[3] = (pt_t){t0, b}; }
                    pt_t N[4];
                    rc_t box = norm_box(P, 4, l->w, l->h, N);
                    id o = obs_new(&VNBarcodeObservation_c, box);
                    OB(o)->tl = N[0]; OB(o)->tr = N[1]; OB(o)->br = N[2]; OB(o)->bl = N[3];
                    OB(o)->payload = ret(cx_str(text));
                    OB(o)->symbology = ret(h.sym == 13 ? VNBarcodeSymbologyEAN13 : h.sym == 8 ? VNBarcodeSymbologyEAN8 : VNBarcodeSymbologyCode128);
                    MSG(void, results, "addObject:", o);
                }
            }
        }
        free(buf);
    }
    free(bin);
}
/* VNDetectRectanglesRequest */
static void run_rects(vnreq_t *r, const luma_t *l, id results) {
    quad_t q[16];
    int n = find_quads(l, q, 16, r->rmin_size, r->rmin_ar, r->rmax_ar, r->rtol);
    int max = r->rmax_obs > 0 ? (int)r->rmax_obs : n;
    for (int i = 0; i < n && i < max; i++) {
        if (q[i].score < r->rmin_conf) continue;
        pt_t N[4];
        rc_t box = norm_box(q[i].c, 4, l->w, l->h, N);
        id o = obs_new(&VNRectangleObservation_c, box);
        OB(o)->tl = N[0]; OB(o)->tr = N[1]; OB(o)->br = N[2]; OB(o)->bl = N[3];
        OB(o)->confidence = (float)q[i].score;
        MSG(void, results, "addObject:", o);
    }
}
static void run_feature(const luma_t *l, id results) {
    id o = obs_new(&VNFeaturePrintObservation_c, (rc_t){0, 0, 1, 1});
    vnobs_t *ob = OB(o);
    ob->fpn = 96;
    ob->fp = calloc(96, sizeof(float));
    double mean = 0;
    for (int by = 0; by < 8; by++)
        for (int bx = 0; bx < 8; bx++) {
            double s = 0; int c = 0;
            for (int y = by * l->h / 8; y < (by + 1) * l->h / 8; y++)
                for (int x = bx * l->w / 8; x < (bx + 1) * l->w / 8; x++) { s += l->y[y * l->w + x]; c++; }
            ob->fp[by * 8 + bx] = (float)(c ? s / c / 255.0 : 0);
            mean += ob->fp[by * 8 + bx];
        }
    mean /= 64;
    for (int i = 0; i < 64; i++) ob->fp[i] -= (float)mean;      /* invariante a brilho */
    for (int i = 0; i < l->w * l->h; i++) ob->fp[64 + l->y[i] / 8] += 1.0f;
    for (int i = 64; i < 96; i++) ob->fp[i] /= (float)(l->w * l->h);
    MSG(void, results, "addObject:", o);
}
static BOOL run_requests(const luma_t *l, id requests, id *err) {
    if (err) *err = 0;
    for (u64 i = 0; i < cnt(requests); i++) {
        id q = at(requests, i);
        if (!object_getClass(q) || !cx_is_subclass(object_getClass(q), &VNRequest_c)) continue;
        vnreq_t *r = RQ(q);
        if (!r->kind) r->kind = kind_of(q);
        id results = marr();
        /* região de interesse: recorta a luminância */
        luma_t sub = *l, owned = {0};
        if (r->roi.w > 0 && r->roi.h > 0 && (r->roi.w < 1 || r->roi.h < 1 || r->roi.x > 0 || r->roi.y > 0)) {
            int x0 = (int)(r->roi.x * l->w), y0 = (int)((1 - r->roi.y - r->roi.h) * l->h), w = (int)(r->roi.w * l->w), h = (int)(r->roi.h * l->h);
            if (w > 8 && h > 8 && x0 >= 0 && y0 >= 0 && x0 + w <= l->w && y0 + h <= l->h) {
                owned.w = w; owned.h = h; owned.y = malloc((size_t)w * h);
                for (int y = 0; owned.y && y < h; y++) memcpy(owned.y + y * w, l->y + (y0 + y) * l->w + x0, (size_t)w);
                if (owned.y) sub = owned;
            }
        }
        switch (r->kind) {
        case RQ_BARCODE: run_barcodes(r, &sub, results); break;
        case RQ_FEATURE: run_feature(&sub, results); break;
        case RQ_RECT: run_rects(r, &sub, results); break;
        default: break;                                             /* faces, texto, retângulos, classificação: sem modelo */
        }
        luma_free(&owned);
        setobj(&r->results, results);
        if (r->completion) ((void (*)(void *, id, id))((blk_t *)r->completion)->invoke)(r->completion, q, (id)0);
    }
    return YES;
}

/* ---- VNImageRequestHandler */
typedef struct { void *isa; luma_t l; id error; } vnhandler_t;
#define HD(o) ((vnhandler_t *)(o))
static id err_image(void) {
    id info = MSG0(id, cls("NSMutableDictionary"), "dictionary");
    MSG(void, info, "setObject:forKey:", cx_str("Não foi possível ler a imagem"), cx_str("NSLocalizedDescription"));
    return MSG(id, cls("NSError"), "errorWithDomain:code:userInfo:", VNErrorDomain, 3 /* VNErrorInvalidImage? */, info);
}
static id h_init_cg(id s, SEL c, void *img, id opts) { (void)c; (void)opts; if (!img || !luma_from_cgimage(&HD(s)->l, img)) HD(s)->error = ret(err_image()); return s; }
static id h_init_cg_o(id s, SEL c, void *img, u32 orient, id opts) { (void)orient; return h_init_cg(s, c, img, opts); }
static id h_init_pb(id s, SEL c, void *pb, id opts) { (void)c; (void)opts; if (!pb || !luma_from_pixelbuffer(&HD(s)->l, pb)) HD(s)->error = ret(err_image()); return s; }
static id h_init_pb_o(id s, SEL c, void *pb, u32 orient, id opts) { (void)orient; return h_init_pb(s, c, pb, opts); }
static id h_init_data(id s, SEL c, id data, id opts) {
    (void)c; (void)opts;
    u64 n = data ? MSG0(u64, data, "length") : 0;
    if (!n || !luma_from_encoded(&HD(s)->l, MSG0(const u8 *, data, "bytes"), n)) HD(s)->error = ret(err_image());
    return s;
}
static id h_init_data_o(id s, SEL c, id data, u32 orient, id opts) { (void)orient; return h_init_data(s, c, data, opts); }
static id h_init_url(id s, SEL c, id url, id opts) {
    (void)c; (void)opts;
    id path = url && resp(url, "path") ? MSG0(id, url, "path") : url;
    u64 n = 0;
    u8 *b = path ? cx_read_file(cx_cstr(path), &n) : 0;
    if (!b || !luma_from_encoded(&HD(s)->l, b, n)) HD(s)->error = ret(err_image());
    free(b);
    return s;
}
static id h_init_url_o(id s, SEL c, id url, u32 orient, id opts) { (void)orient; return h_init_url(s, c, url, opts); }
static id h_init_ci(id s, SEL c, id ci, id opts) { (void)ci; (void)opts; (void)c; HD(s)->error = ret(err_image()); return s; }
static BOOL h_perform(id s, SEL c, id requests, id *err) {
    (void)c;
    if (HD(s)->error || !HD(s)->l.y) { if (err) *err = HD(s)->error ? HD(s)->error : err_image(); return NO; }
    return run_requests(&HD(s)->l, requests, err);
}
static void h_dealloc(id s, SEL c) { (void)c; luma_free(&HD(s)->l); }
/* VNSequenceRequestHandler: performRequests:onCGImage:error: e afins */
static BOOL sq_cg(id s, SEL c, id requests, void *img, id *err) {
    (void)s; (void)c;
    luma_t l = {0};
    if (!img || !luma_from_cgimage(&l, img)) { if (err) *err = err_image(); return NO; }
    BOOL ok = run_requests(&l, requests, err);
    luma_free(&l);
    return ok;
}
static BOOL sq_pb(id s, SEL c, id requests, void *pb, id *err) {
    (void)s; (void)c;
    luma_t l = {0};
    if (!pb || !luma_from_pixelbuffer(&l, pb)) { if (err) *err = err_image(); return NO; }
    BOOL ok = run_requests(&l, requests, err);
    luma_free(&l);
    return ok;
}
static BOOL sq_cg_o(id s, SEL c, id rq, void *img, u32 o, id *err) { (void)o; return sq_cg(s, c, rq, img, err); }
static BOOL sq_pb_o(id s, SEL c, id rq, void *pb, u32 o, id *err) { (void)o; return sq_pb(s, c, rq, pb, err); }

/* autoteste (harness): monta um QR "mOS" conhecido e um EAN-13 numa imagem e lê de volta */
int cx_vision_selftest(char *out, size_t cap);

static void K(cx_class_t *k, cx_class_t *m, const char *name, cx_class_t *super, u32 size) { if (!cx_class_named(name)) cx_define_class(k, m, name, super, size); }
#define A(k, s, f) cx_add_method(k, s, (void *)(f))
#define AC(k, s, f) cx_add_cmethod(k, s, (void *)(f))
static id S(const char *s) { return ret(cx_str(s)); }
void cx_vision_init(void) {
    cx_class_t *obj = cx_class_named("NSObject");
    VNBarcodeSymbologyQR = S("VNBarcodeSymbologyQR"); VNBarcodeSymbologyEAN13 = S("VNBarcodeSymbologyEAN13");
    VNBarcodeSymbologyEAN8 = S("VNBarcodeSymbologyEAN8"); VNBarcodeSymbologyUPCE = S("VNBarcodeSymbologyUPCE");
    VNBarcodeSymbologyAztec = S("VNBarcodeSymbologyAztec"); VNBarcodeSymbologyCode128 = S("VNBarcodeSymbologyCode128");
    VNBarcodeSymbologyCode39 = S("VNBarcodeSymbologyCode39"); VNBarcodeSymbologyCode39Checksum = S("VNBarcodeSymbologyCode39Checksum");
    VNBarcodeSymbologyCode39FullASCII = S("VNBarcodeSymbologyCode39FullASCII"); VNBarcodeSymbologyCode39FullASCIIChecksum = S("VNBarcodeSymbologyCode39FullASCIIChecksum");
    VNBarcodeSymbologyCode93 = S("VNBarcodeSymbologyCode93"); VNBarcodeSymbologyCode93i = S("VNBarcodeSymbologyCode93i");
    VNBarcodeSymbologyDataMatrix = S("VNBarcodeSymbologyDataMatrix"); VNBarcodeSymbologyI2of5 = S("VNBarcodeSymbologyI2of5");
    VNBarcodeSymbologyI2of5Checksum = S("VNBarcodeSymbologyI2of5Checksum"); VNBarcodeSymbologyITF14 = S("VNBarcodeSymbologyITF14");
    VNBarcodeSymbologyPDF417 = S("VNBarcodeSymbologyPDF417"); VNBarcodeSymbologyCodabar = S("VNBarcodeSymbologyCodabar");
    VNBarcodeSymbologyGS1DataBar = S("VNBarcodeSymbologyGS1DataBar"); VNBarcodeSymbologyMicroQR = S("VNBarcodeSymbologyMicroQR");
    VNBarcodeSymbologyMicroPDF417 = S("VNBarcodeSymbologyMicroPDF417");
    VNErrorDomain = S("com.apple.vis"); VNImageOptionProperties = S("VNImageOptionProperties");
    VNImageOptionCameraIntrinsics = S("VNImageOptionCameraIntrinsics"); VNImageOptionCIContext = S("VNImageOptionCIContext");

    K(&VNRequest_c, &VNRequest_m, "VNRequest", obj, (u32)(sizeof(vnreq_t) - 8));
    cx_class_t *r = &VNRequest_c;
    A(r, "init", req_init); A(r, "initWithCompletionHandler:", req_init_completion); A(r, "results", req_results);
    A(r, "completionHandler", req_completion); A(r, "revision", req_revision); A(r, "setRevision:", req_set_revision);
    A(r, "usesCPUOnly", req_cpu); A(r, "setUsesCPUOnly:", req_set_cpu); A(r, "preferBackgroundProcessing", req_bg);
    A(r, "setPreferBackgroundProcessing:", req_set_bg); A(r, "cancel", req_cancel);
    K(&VNImageBasedRequest_c, &VNImageBasedRequest_m, "VNImageBasedRequest", r, 0);
    A(&VNImageBasedRequest_c, "regionOfInterest", req_roi); A(&VNImageBasedRequest_c, "setRegionOfInterest:", req_set_roi);
    cx_class_t *ib = &VNImageBasedRequest_c;
    K(&VNDetectBarcodesRequest_c, &VNDetectBarcodesRequest_m, "VNDetectBarcodesRequest", ib, 0);
    A(&VNDetectBarcodesRequest_c, "symbologies", req_symbologies); A(&VNDetectBarcodesRequest_c, "setSymbologies:", req_set_symbologies);
    A(&VNDetectBarcodesRequest_c, "supportedSymbologiesAndReturnError:", req_supported_sym_err);
    AC(&VNDetectBarcodesRequest_c, "supportedSymbologies", req_supported_sym_c);
    K(&VNDetectFaceRectanglesRequest_c, &VNDetectFaceRectanglesRequest_m, "VNDetectFaceRectanglesRequest", ib, 0);
    K(&VNDetectFaceLandmarksRequest_c, &VNDetectFaceLandmarksRequest_m, "VNDetectFaceLandmarksRequest", ib, 0);
    K(&VNRecognizeTextRequest_c, &VNRecognizeTextRequest_m, "VNRecognizeTextRequest", ib, 0);
    cx_class_t *tx = &VNRecognizeTextRequest_c;
    A(tx, "recognitionLanguages", req_langs); A(tx, "setRecognitionLanguages:", req_set_langs);
    A(tx, "supportedRecognitionLanguagesAndReturnError:", req_supported_langs); A(tx, "customWords", req_words);
    A(tx, "setCustomWords:", req_set_words); A(tx, "recognitionLevel", req_level); A(tx, "setRecognitionLevel:", req_set_level);
    A(tx, "usesLanguageCorrection", req_lc); A(tx, "setUsesLanguageCorrection:", req_set_lc);
    A(tx, "minimumTextHeight", req_minh); A(tx, "setMinimumTextHeight:", req_set_minh);
    K(&VNDetectRectanglesRequest_c, &VNDetectRectanglesRequest_m, "VNDetectRectanglesRequest", ib, 0);
    cx_class_t *rr = &VNDetectRectanglesRequest_c;
    A(rr, "minimumAspectRatio", rq_min_ar); A(rr, "setMinimumAspectRatio:", rq_set_min_ar); A(rr, "maximumAspectRatio", rq_max_ar);
    A(rr, "setMaximumAspectRatio:", rq_set_max_ar); A(rr, "quadratureTolerance", rq_tol); A(rr, "setQuadratureTolerance:", rq_set_tol);
    A(rr, "minimumSize", rq_min_size); A(rr, "setMinimumSize:", rq_set_min_size); A(rr, "minimumConfidence", rq_min_conf);
    A(rr, "setMinimumConfidence:", rq_set_min_conf); A(rr, "maximumObservations", rq_max_obs); A(rr, "setMaximumObservations:", rq_set_max_obs);
    K(&VNClassifyImageRequest_c, &VNClassifyImageRequest_m, "VNClassifyImageRequest", ib, 0);
    K(&VNGenerateImageFeaturePrintRequest_c, &VNGenerateImageFeaturePrintRequest_m, "VNGenerateImageFeaturePrintRequest", ib, 0);

    K(&VNObservation_c, &VNObservation_m, "VNObservation", obj, (u32)(sizeof(vnobs_t) - 8));
    A(&VNObservation_c, "confidence", ob_conf); A(&VNObservation_c, "uuid", ob_uuid); A(&VNObservation_c, "timeRange", ob_time);
    K(&VNDetectedObjectObservation_c, &VNDetectedObjectObservation_m, "VNDetectedObjectObservation", &VNObservation_c, 0);
    A(&VNDetectedObjectObservation_c, "boundingBox", ob_box);
    K(&VNRectangleObservation_c, &VNRectangleObservation_m, "VNRectangleObservation", &VNDetectedObjectObservation_c, 0);
    cx_class_t *ro = &VNRectangleObservation_c;
    A(ro, "topLeft", ob_tl); A(ro, "topRight", ob_tr); A(ro, "bottomRight", ob_br); A(ro, "bottomLeft", ob_bl);
    K(&VNBarcodeObservation_c, &VNBarcodeObservation_m, "VNBarcodeObservation", ro, 0);
    A(&VNBarcodeObservation_c, "payloadStringValue", ob_payload); A(&VNBarcodeObservation_c, "symbology", ob_symbology);
    A(&VNBarcodeObservation_c, "barcodeDescriptor", ob_nil); A(&VNBarcodeObservation_c, "payloadData", ob_nil);
    K(&VNFaceObservation_c, &VNFaceObservation_m, "VNFaceObservation", &VNDetectedObjectObservation_c, 0);
    A(&VNFaceObservation_c, "landmarks", ob_nil); A(&VNFaceObservation_c, "roll", ob_zero_num); A(&VNFaceObservation_c, "yaw", ob_zero_num);
    A(&VNFaceObservation_c, "pitch", ob_zero_num); A(&VNFaceObservation_c, "faceCaptureQuality", ob_nil);
    K(&VNFeaturePrintObservation_c, &VNFeaturePrintObservation_m, "VNFeaturePrintObservation", &VNObservation_c, 0);
    A(&VNFeaturePrintObservation_c, "elementCount", fp_count); A(&VNFeaturePrintObservation_c, "elementType", fp_type);
    A(&VNFeaturePrintObservation_c, "data", fp_data); A(&VNFeaturePrintObservation_c, "computeDistance:toFeaturePrintObservation:error:", fp_distance);

    K(&VNImageRequestHandler_c, &VNImageRequestHandler_m, "VNImageRequestHandler", obj, (u32)(sizeof(vnhandler_t) - 8));
    cx_class_t *h = &VNImageRequestHandler_c;
    A(h, "initWithCGImage:options:", h_init_cg); A(h, "initWithCGImage:orientation:options:", h_init_cg_o);
    A(h, "initWithCVPixelBuffer:options:", h_init_pb); A(h, "initWithCVPixelBuffer:orientation:options:", h_init_pb_o);
    A(h, "initWithData:options:", h_init_data); A(h, "initWithData:orientation:options:", h_init_data_o);
    A(h, "initWithURL:options:", h_init_url); A(h, "initWithURL:orientation:options:", h_init_url_o);
    A(h, "initWithCIImage:options:", h_init_ci);
    A(h, "performRequests:error:", h_perform); A(h, ".cxx_destruct", h_dealloc);
    K(&VNSequenceRequestHandler_c, &VNSequenceRequestHandler_m, "VNSequenceRequestHandler", obj, 0);
    cx_class_t *sq = &VNSequenceRequestHandler_c;
    A(sq, "performRequests:onCGImage:error:", sq_cg); A(sq, "performRequests:onCVPixelBuffer:error:", sq_pb);
    A(sq, "performRequests:onCGImage:orientation:error:", sq_cg_o); A(sq, "performRequests:onCVPixelBuffer:orientation:error:", sq_pb_o);
}

/* ================================================================ autoteste: gera um QR e um EAN-13 e lê de volta
 * O codificador é mínimo (modo byte, nível L, um bloco: versões 1–5) e só existe para conferir o leitor inteiro:
 * imagem → binarização → localizadores → grade → formato → máscara → Reed-Solomon → texto. */
static void rs_encode(const u8 *data, int n, int ecc, u8 *out) {
    gf_init();
    u8 gen[40] = {1};
    int gl = 1;
    for (int i = 0; i < ecc; i++) {                                 /* g(x) = Π (x - α^i) */
        u8 ng[40] = {0};
        for (int j = 0; j < gl; j++) { ng[j] ^= gen[j]; ng[j + 1] ^= gmul(gen[j], gexp[i]); }
        gl++;
        memcpy(gen, ng, sizeof gen);
    }
    u8 rem[40] = {0};
    for (int k = 0; k < n; k++) {
        u8 f = data[k] ^ rem[0];
        memmove(rem, rem + 1, (size_t)ecc - 1);
        rem[ecc - 1] = 0;
        for (int j = 0; j < ecc; j++) rem[j] ^= gmul(gen[j + 1], f);
    }
    memcpy(out, rem, (size_t)ecc);
}
static u8 *qr_encode(const char *text, int v, int mask, int *dim_out) {
    int n = 4 * v + 17, total = raw_modules(v) / 8, ecc = ECC_PER_BLOCK[0][v], nd = total - ecc;
    if (NUM_BLOCKS[0][v] != 1) return 0;
    u8 cw[200] = {0};
    int len = (int)strlen(text), bit = 0;
#define PUT(val, k) do { for (int _i = (k) - 1; _i >= 0; _i--) { if (((val) >> _i) & 1) cw[bit >> 3] |= (u8)(0x80 >> (bit & 7)); bit++; } } while (0)
    PUT(4, 4); PUT(len, 8);
    for (int i = 0; i < len; i++) PUT((u8)text[i], 8);
    PUT(0, 4);
    bit = (bit + 7) / 8 * 8;
    for (int k = bit / 8, p = 0; k < nd; k++, p ^= 1) cw[k] = p ? 0x11 : 0xEC;
#undef PUT
    rs_encode(cw, nd, ecc, cw + nd);
    u8 *g = calloc((size_t)n * n, 1), *fn = malloc((size_t)n * n);
    mark_function(fn, v);
#define SETM(x, y, b) g[(y) * n + (x)] = (u8)(b)
    for (int i = 0; i < n; i++) { SETM(6, i, i % 2 == 0); SETM(i, 6, i % 2 == 0); }
    int corners[3][2] = {{0, 0}, {n - 7, 0}, {0, n - 7}};
    for (int c = 0; c < 3; c++)
        for (int dy = -1; dy <= 7; dy++)
            for (int dx = -1; dx <= 7; dx++) {
                int x = corners[c][0] + dx, y = corners[c][1] + dy;
                if (x < 0 || y < 0 || x >= n || y >= n) continue;
                int ring = dx < 0 || dy < 0 || dx > 6 || dy > 6 ? 0 : (dx == 0 || dy == 0 || dx == 6 || dy == 6 || (dx >= 2 && dx <= 4 && dy >= 2 && dy <= 4));
                SETM(x, y, ring);
            }
    int ap[8], na = align_pos(v, ap);
    for (int i = 0; i < na; i++)
        for (int j = 0; j < na; j++) {
            if ((i == 0 && j == 0) || (i == 0 && j == na - 1) || (i == na - 1 && j == 0)) continue;
            for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) SETM(ap[i] + dx, ap[j] + dy, dx == -2 || dx == 2 || dy == -2 || dy == 2 || (!dx && !dy));
        }
    int fw = format_word(1 /* L */, mask);
    for (int i = 0; i <= 5; i++) SETM(8, i, (fw >> i) & 1);
    SETM(8, 7, (fw >> 6) & 1); SETM(8, 8, (fw >> 7) & 1); SETM(7, 8, (fw >> 8) & 1);
    for (int i = 9; i < 15; i++) SETM(14 - i, 8, (fw >> i) & 1);
    for (int i = 0; i < 8; i++) SETM(n - 1 - i, 8, (fw >> i) & 1);
    for (int i = 8; i < 15; i++) SETM(8, n - 15 + i, (fw >> i) & 1);
    SETM(8, n - 8, 1);
    bit = 0;
    for (int right = n - 1; right >= 1; right -= 2) {
        if (right == 6) right = 5;
        for (int vert = 0; vert < n; vert++)
            for (int j = 0; j < 2; j++) {
                int x = right - j, up = ((right + 1) & 2) == 0, y = up ? n - 1 - vert : vert;
                if (fn[y * n + x]) continue;
                int b = bit < total * 8 ? (cw[bit >> 3] >> (7 - (bit & 7))) & 1 : 0;
                bit++;
                SETM(x, y, b ^ mask_bit(mask, x, y));
            }
    }
#undef SETM
    free(fn);
    *dim_out = n;
    return g;
}
static void draw_ean13(u8 *img, int W, int H, int x0, const char *digits) {
    int bits[95], nb = 0;
#define B3(a, b, c) do { bits[nb++] = a; bits[nb++] = b; bits[nb++] = c; } while (0)
    B3(1, 0, 1);
    const char *par = EAN_PARITY[digits[0] - '0'];
    for (int i = 1; i <= 6; i++) {
        const u8 *w = EAN_L[digits[i] - '0'];
        int ww[4] = {w[0], w[1], w[2], w[3]};
        if (par[i - 1] == 'G') { ww[0] = w[3]; ww[1] = w[2]; ww[2] = w[1]; ww[3] = w[0]; }
        for (int k = 0; k < 4; k++) for (int m = 0; m < ww[k]; m++) bits[nb++] = k & 1;   /* espaço, barra, espaço, barra */
    }
    bits[nb++] = 0; B3(1, 0, 1); bits[nb++] = 0;
    for (int i = 7; i <= 12; i++) { const u8 *w = EAN_L[digits[i] - '0']; for (int k = 0; k < 4; k++) for (int m = 0; m < w[k]; m++) bits[nb++] = !(k & 1); }
    B3(1, 0, 1);
#undef B3
    for (int y = H / 4; y < H * 3 / 4; y++)
        for (int i = 0; i < nb; i++) for (int s = 0; s < 3; s++) img[y * W + x0 + i * 3 + s] = bits[i] ? 0 : 255;
}
int cx_vision_selftest(char *out, size_t cap) {
    out[0] = 0;
    int dim = 0;
    u8 *g = qr_encode("https://mos.example/teste", 2, 3, &dim);
    if (!g) { snprintf(out, cap, "codificador falhou"); return 1; }
    int sc = 5, W = (dim + 8) * sc + 400, H = (dim + 8) * sc + 40;
    luma_t l = {W, H, malloc((size_t)W * H)};
    memset(l.y, 255, (size_t)W * H);
    for (int y = 0; y < dim; y++) for (int x = 0; x < dim; x++)
        if (g[y * dim + x]) for (int a = 0; a < sc; a++) for (int b = 0; b < sc; b++) l.y[(20 + (y + 4) * sc + a) * W + (x + 4) * sc + b] = 0;
    free(g);
    draw_ean13(l.y, W, H, (dim + 8) * sc + 40, "4006381333931");
    vnreq_t req;
    memset(&req, 0, sizeof req);
    id res = marr();
    run_barcodes(&req, &l, res);
    int ok = 0;
    for (u64 i = 0; i < cnt(res); i++) {
        id o = at(res, i);
        size_t n = strlen(out);
        snprintf(out + n, cap - n, "%s%s=%s", n ? " " : "", cx_cstr(OB(o)->symbology) + 18, cx_cstr(OB(o)->payload));
        if (!strcmp(cx_cstr(OB(o)->payload), "https://mos.example/teste")) ok |= 1;
        if (!strcmp(cx_cstr(OB(o)->payload), "4006381333931")) ok |= 2;
    }
    /* Reed-Solomon sozinho: 4 bytes errados num bloco com 10 de paridade */
    u8 m[30], e[10];
    for (int i = 0; i < 20; i++) m[i] = (u8)(i * 7 + 3);
    rs_encode(m, 20, 10, e);
    memcpy(m + 20, e, 10);
    m[2] ^= 0x55; m[9] ^= 1; m[17] ^= 0xFF; m[25] ^= 0x10;
    int fixed = rs_correct(m, 30, 10);
    int rs_ok = fixed == 4 && m[2] == 17 && m[9] == 66 && m[17] == (u8)(17 * 7 + 3);
    size_t n = strlen(out);
    snprintf(out + n, cap - n, " RS=%s", rs_ok ? "ok" : "falhou");
    luma_free(&l);
    return (ok == 3 && rs_ok) ? 0 : 1;
}

/* autoteste 2: Code 128 (conjunto B) e um retângulo inclinado */
static void draw_c128(u8 *img, int W, int y0, int y1, int x0, const char *s) {
    int vals[64], n = 0, sum = 104;
    vals[n++] = 104;
    for (int i = 0; s[i]; i++) { vals[n] = s[i] - 32; sum += vals[n] * i + vals[n]; n++; }
    sum = 104;
    for (int i = 1; i < n; i++) sum += vals[i] * i;
    vals[n++] = sum % 103;
    vals[n++] = 106;
    int x = x0;
    for (int k = 0; k < n; k++) {
        const char *p = C128[vals[k]];
        for (int e = 0; p[e]; e++) {
            int wpx = (p[e] - '0') * 2;
            if (!(e & 1)) for (int y = y0; y < y1; y++) for (int q = 0; q < wpx; q++) img[y * W + x + q] = 0;
            x += wpx;
        }
    }
}
int cx_vision_selftest2(char *out, size_t cap) {
    int W = 640, H = 360;
    luma_t l = {W, H, malloc((size_t)W * H)};
    memset(l.y, 255, (size_t)W * H);
    draw_c128(l.y, W, 20, 90, 60, "mOS-128 Visao!");
    /* quadrado de 160 px girado ~10°, cinza-escuro */
    double cx = 400, cy = 240, a = 0.17, cs = __builtin_cos(a), sn = __builtin_sin(a);
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        double u = (x - cx) * cs + (y - cy) * sn, v = -(x - cx) * sn + (y - cy) * cs;
        if (u > -80 && u < 80 && v > -60 && v < 60) l.y[y * W + x] = 40;
    }
    vnreq_t bq, rq;
    memset(&bq, 0, sizeof bq);
    memset(&rq, 0, sizeof rq);
    rq.rmin_ar = 0.5f; rq.rmax_ar = 1; rq.rtol = 30; rq.rmin_size = 0.2f; rq.rmax_obs = 4;
    id br = marr(), rr = marr();
    run_barcodes(&bq, &l, br);
    run_rects(&rq, &l, rr);
    int ok = 0;
    out[0] = 0;
    for (u64 i = 0; i < cnt(br); i++) {
        const char *p = cx_cstr(OB(at(br, i))->payload);
        size_t n = strlen(out);
        snprintf(out + n, cap - n, "%s%s=%s", n ? " " : "", cx_cstr(OB(at(br, i))->symbology) + 18, p);
        if (!strcmp(p, "mOS-128 Visao!")) ok |= 1;
    }
    for (u64 i = 0; i < cnt(rr); i++) {
        vnobs_t *o = OB(at(rr, i));
        size_t n = strlen(out);
        snprintf(out + n, cap - n, " retângulo %.2fx%.2f@(%.2f,%.2f)", o->box.w, o->box.h, o->box.x, o->box.y);
        if (o->box.w > 0.25 && o->box.w < 0.4 && o->box.x > 0.4) ok |= 2;
    }
    luma_free(&l);
    return ok == 3 ? 0 : 1;
}
/* autoteste 3: segmento kanji (漢 = SJIS 0x8ABF → 1855, 字 = 0x8E9A → 2586) */
int cx_vision_selftest3(char *out, size_t cap) {
    u8 d[8] = {0};
    int bit = 0;
#define PUT(val, k) do { for (int _i = (k) - 1; _i >= 0; _i--) { if (((val) >> _i) & 1) d[bit >> 3] |= (u8)(0x80 >> (bit & 7)); bit++; } } while (0)
    PUT(8, 4); PUT(2, 8); PUT(1855, 13); PUT(2586, 13); PUT(0, 4);
#undef PUT
    char *s = qr_parse(d, 8, 1);
    snprintf(out, cap, "kanji=%s", s ? s : "?");
    int ok = s && !strcmp(s, "\xE6\xBC\xA2\xE5\xAD\x97");
    free(s);
    return ok ? 0 : 1;
}
