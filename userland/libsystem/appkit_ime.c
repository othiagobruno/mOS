/*
 * mOS libSystem — método de entrada (IME) para japonês, chinês e coreano, no papel do servidor de métodos
 * de entrada do macOS: a composição vai para o cliente NSTextInputClient como marked text
 * (setMarkedText:selectedRange:replacementRange:) e o texto escolhido chega por insertText:replacementRange:,
 * exatamente como o Chromium e o NSTextView esperam. A fonte de entrada é global (o window server guarda;
 * ⌃Espaço troca) e a lista de candidatos é desenhada pelo window server perto do cursor de texto
 * (firstRectForCharacterRange:actualRange:).
 *
 *  - Japonês (Kotoeri, Romaji): romaji → hiragana enquanto digita (っ por consoante dobrada, ん, ゃゅょ, ー,
 *    pontuação 、。「」); Espaço converte (kana-kanji por dicionário, com segmentação gulosa e partículas
 *    preservadas) e percorre os candidatos; a lista sempre traz hiragana e katakana.
 *  - Chinês simplificado (Pinyin): segmenta o pinyin em sílabas, mostra os candidatos já durante a
 *    digitação (palavras do dicionário primeiro, depois caracteres da primeira sílaba, também para sílaba
 *    incompleta como "zh"); Espaço/1–9 escolhem, a escolha consome as sílabas e a composição continua com o
 *    resto. Enter confirma as letras. Pontuação vira a de largura total (，。？！：；、).
 *  - Coreano (2-Set): autômato de hangul com vogais e finais compostas e a passagem da final para a
 *    próxima sílaba quando vem uma vogal; Backspace desfaz jamo por jamo.
 *
 * Dicionários: uma tabela embutida (palavras comuns) e, se existirem, arquivos maiores em
 * /System/Library/Input Methods/mOS/{ja,zh}.dict ("leitura<TAB>cand1 cand2 ...") ou no formato SKK
 * ("leitura /cand1/cand2/", ex.: SKK-JISYO.L em ja.dict) — consultados antes da tabela embutida.
 * Campos nativos do mOS (NSTextField/NSTextView do AppKit daqui) não desenham marked text: neles a
 * composição aparece na própria janelinha de candidatos.
 */
#include "foundation.h"
#include "sys.h"
#include "../../kernel/include/compat_gui.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
const char *cx_cstr(id s);
id cx_str(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
void *malloc(size_t n);
void *realloc(void *p, size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
size_t strlen(const char *s);
char *strcpy(char *d, const char *s);
extern u8 *cx_read_file(const char *path, u64 *len);
typedef struct { double x, y, w, h; } NSRect_;
int cx_window_gid(id w);
double cx_window_content_h(id w);
NSRect_ cx_window_screen_frame(id w);
BOOL cx_view_rect_in_window(id v, double *x, double *y, double *w, double *h);
void cx_native_insert_text(id v, const char *utf8);
extern void objc_msgSend_stret(void);

typedef struct { u64 loc, len; } rng_t;
#define NOT_FOUND 0x7fffffffffffffffull
#define MOD_SHIFT (1ull << 17)
#define MOD_CTRL  (1ull << 18)
#define MOD_OPT   (1ull << 19)
#define MOD_CMD   (1ull << 20)

static BOOL resp(id o, const char *sel) { return o && cx_responds(object_getClass(o), sel_registerName(sel)); }

/* ================================================================ UTF-8 */
static int u8enc(u32 cp, char *o) {
    if (cp < 0x80) { o[0] = (char)cp; return 1; }
    if (cp < 0x800) { o[0] = (char)(0xC0 | cp >> 6); o[1] = (char)(0x80 | (cp & 63)); return 2; }
    o[0] = (char)(0xE0 | cp >> 12); o[1] = (char)(0x80 | ((cp >> 6) & 63)); o[2] = (char)(0x80 | (cp & 63));
    return 3;
}
static u32 u8dec(const char **pp) {
    const u8 *p = (const u8 *)*pp;
    u32 c = *p++;
    if (c >= 0xF0 && p[0] && p[1] && p[2]) { c = ((c & 7) << 18) | ((p[0] & 63) << 12) | ((p[1] & 63) << 6) | (p[2] & 63); p += 3; }
    else if (c >= 0xE0 && p[0] && p[1]) { c = ((c & 15) << 12) | ((p[0] & 63) << 6) | (p[1] & 63); p += 2; }
    else if (c >= 0xC0 && p[0]) { c = ((c & 31) << 6) | (p[0] & 63); p += 1; }
    *pp = (const char *)p;
    return c;
}
static int u8last(const char *s, int n) { int i = n - 1; while (i > 0 && ((u8)s[i] & 0xC0) == 0x80) i--; return i < 0 ? 0 : i; }
static void cat(char *d, size_t cap, const char *s) { size_t a = strlen(d), b = strlen(s); if (a + b + 1 > cap) b = cap - a - 1; memcpy(d + a, s, b); d[a + b] = 0; }

/* ================================================================ dicionários
 * Uma entrada: leitura → candidatos. Embutido + arquivo opcional, os dois no mesmo formato de linhas. */
#include "appkit_ime_data.inc"     /* dicionários embutidos */
#include "appkit_ime_engine.inc"   /* composição, candidatos e ligação com o NSTextInputClient */
