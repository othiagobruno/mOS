/* mOS libSystem — núcleo sem ObjC do layout de linhas do TextKit (appkit_textkit.c): onde cada linha cai no contêiner
 * a partir das métricas da fonte e do NSParagraphStyle. Teste: userland/tests-appkit/tk_host.c. */
#ifndef APPKIT_TK_CORE_H
#define APPKIT_TK_CORE_H
typedef struct {
    int alignment;                 /* NSTextAlignment: 0 esquerda, 1 direita, 2 centro, 3 justificado, 4 natural */
    double first_indent, head_indent, tail_indent;   /* tail <= 0 conta da direita */
    double line_spacing, para_spacing, para_spacing_before;
    double min_h, max_h, multiple;
} tk_para;
typedef struct { double x, y, w, h, used_x, used_w, baseline; } tk_line;
/* largura disponível para o texto da linha (sem o padding do contêiner nem os recuos) */
double tk_avail_width(const tk_para *p, double container_w, double padding, int first_line);
/* posiciona a linha em y com as métricas (ascent, descent, leading) e a largura tipográfica do texto */
tk_line tk_place(const tk_para *p, double container_w, double padding, int first_line, int para_start, int para_end,
                 double y, double ascent, double descent, double leading, double text_w, double trailing_ws);
void tk_para_default(tk_para *p);
#endif
