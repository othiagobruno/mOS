/* mOS libSystem — núcleo do layout de linhas do TextKit (ver appkit_tk_core.h). Sem libc. */
#include "appkit_tk_core.h"

void tk_para_default(tk_para *p) {
    p->alignment = 4; p->first_indent = p->head_indent = p->tail_indent = 0;
    p->line_spacing = p->para_spacing = p->para_spacing_before = 0;
    p->min_h = p->max_h = 0; p->multiple = 0;
}
static double right_edge(const tk_para *p, double container_w, double padding) {
    double r = container_w - padding;
    if (p->tail_indent > 0) r = padding + p->tail_indent;
    else if (p->tail_indent < 0) r = container_w - padding + p->tail_indent;
    return r;
}
double tk_avail_width(const tk_para *p, double container_w, double padding, int first_line) {
    double left = padding + (first_line ? p->first_indent : p->head_indent);
    double w = right_edge(p, container_w, padding) - left;
    return w > 1 ? w : 1;
}
tk_line tk_place(const tk_para *p, double container_w, double padding, int first_line, int para_start, int para_end,
                 double y, double ascent, double descent, double leading, double text_w, double trailing_ws) {
    tk_line L;
    double natural = ascent + descent + (leading > 0 ? leading : 0);
    double h = p->multiple > 0 ? natural * p->multiple : natural;
    if (p->min_h > 0 && h < p->min_h) h = p->min_h;
    if (p->max_h > 0 && h > p->max_h) h = p->max_h;
    double before = para_start ? p->para_spacing_before : 0;
    double after = (para_end ? p->para_spacing : 0) + p->line_spacing;
    double left = padding + (first_line ? p->first_indent : p->head_indent);
    double avail = right_edge(p, container_w, padding) - left;
    if (avail < 1) avail = 1;
    double vis = text_w - trailing_ws;                 /* o espaço no fim da linha não conta para alinhar */
    double off = 0;
    int a = p->alignment;
    if (a == 1) off = avail - vis;
    else if (a == 2) off = (avail - vis) / 2;
    if (off < 0) off = 0;
    L.x = 0;
    L.y = y;
    L.w = container_w;
    L.h = before + h + after;
    L.used_x = left + off;
    L.used_w = text_w;
    /* a linha extra da altura (múltiplo/mínimo) vai para cima do texto, como no TextKit */
    L.baseline = before + (h - natural) + ascent;
    return L;
}
