/* Geometria da grade (NSCollectionView) e da folha (NSWindow beginSheet).
 * Sem ObjC, para o teste de host. Origem Cocoa: embaixo à esquerda.
 */
#pragma once

typedef struct { double x, y, w, h; } ui_rect;

/* Coloca `n` itens. Largura 0 vira uma coluna (barra lateral). Devolve quantos couberam em cap. */
static int cx_flow_place(int n, double iw, double ih, double gap_x, double gap_y,
                         double inset, double width, ui_rect *out, int cap) {
    if (!out || cap < 1 || n < 1) return 0;
    if (iw < 1) iw = 1;
    if (ih < 1) ih = 1;
    if (gap_x < 0) gap_x = 0;
    if (gap_y < 0) gap_y = 0;
    if (inset < 0) inset = 0;
    int cols = 1;
    if (width > inset * 2 + iw) {
        cols = (int)((width - inset * 2 + gap_x) / (iw + gap_x));
        if (cols < 1) cols = 1;
    }
    int k = n < cap ? n : cap;
    for (int i = 0; i < k; i++) {
        int col = i % cols;
        int row = i / cols;
        out[i].x = inset + col * (iw + gap_x);
        out[i].y = inset + row * (ih + gap_y);
        out[i].w = iw;
        out[i].h = ih;
    }
    return k;
}

/* Folha encostada no topo da janela, centrada, sem passar da largura. */
static void cx_sheet_frame(double win_w, double win_h, double sheet_w, double sheet_h,
                           double *x, double *y, double *w, double *h) {
    if (win_w < 1) win_w = 1;
    if (win_h < 1) win_h = 1;
    if (sheet_w < 1) sheet_w = win_w;
    if (sheet_h < 1) sheet_h = 1;
    if (sheet_w > win_w) sheet_w = win_w;
    if (sheet_h > win_h) sheet_h = win_h;
    if (x) *x = (win_w - sheet_w) / 2;
    if (y) *y = win_h - sheet_h;
    if (w) *w = sheet_w;
    if (h) *h = sheet_h;
}

/* Nove retângulos: cantos com o tamanho do canto, faixas esticadas. */
static void cx_nine_rects(ui_rect frame, double cw, double ch, ui_rect out[9]) {
    if (cw < 0) cw = 0;
    if (ch < 0) ch = 0;
    if (cw * 2 > frame.w) cw = frame.w / 2;
    if (ch * 2 > frame.h) ch = frame.h / 2;
    double mw = frame.w - cw * 2;
    double mh = frame.h - ch * 2;
    double x0 = frame.x, x1 = frame.x + cw, x2 = frame.x + cw + mw;
    double y0 = frame.y, y1 = frame.y + ch, y2 = frame.y + ch + mh;
    out[0] = (ui_rect){x0, y2, cw, ch};
    out[1] = (ui_rect){x1, y2, mw, ch};
    out[2] = (ui_rect){x2, y2, cw, ch};
    out[3] = (ui_rect){x0, y1, cw, mh};
    out[4] = (ui_rect){x1, y1, mw, mh};
    out[5] = (ui_rect){x2, y1, cw, mh};
    out[6] = (ui_rect){x0, y0, cw, ch};
    out[7] = (ui_rect){x1, y0, mw, ch};
    out[8] = (ui_rect){x2, y0, cw, ch};
}

static int cx_depth_bits(int depth) {
    if (depth <= 0) return 0;
    if (depth <= 8) return depth;
    if (depth == 24 || depth == 32) return depth;
    return 32;
}
