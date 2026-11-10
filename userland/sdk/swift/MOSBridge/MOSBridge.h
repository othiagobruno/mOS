/* mOS SDK — ponte C usada pelo módulo SwiftUI do mOS (implementada em userland/libsystem/swiftbridge.c) */
#pragma once

void *mos_class(const char *name);
void *mos_str(const char *s);
const char *mos_cstr(void *s);
void mos_retain(void *o);
void mos_release(void *o);
void *mos_send(void *o, const char *sel);
void *mos_send_p(void *o, const char *sel, void *a);
void *mos_send_pp(void *o, const char *sel, void *a, void *b);
void mos_send_d(void *o, const char *sel, double d);
void mos_send_i(void *o, const char *sel, long v);
long mos_send_ret_i(void *o, const char *sel);
double mos_send_ret_d(void *o, const char *sel);
void mos_set_string(void *o, const char *sel, const char *s);
const char *mos_get_string(void *o, const char *sel);
void *mos_new_view(const char *klass, double x, double y, double w, double h);
void mos_set_frame(void *v, double x, double y, double w, double h);
void mos_add_subview(void *parent, void *child);
void mos_remove_from_superview(void *v);
void *mos_font(double size, int weight);
void *mos_color(double r, double g, double b, double a);
void mos_measure(const char *s, double size, int weight, double *w, double *h);
void *mos_on_action(void *control, void (*fn)(void *), void *ctx);
void *mos_on_text(void *field, void (*fn)(void *, const char *), void *ctx);
void mos_request_render(void);
void *mos_main_window(void);
void mos_run_app(const char *title, double w, double h, void (*render)(void *, void *, double, double), void *ctx);
void mos_log(const char *s);
/* Geometria do hosting SwiftUI (userland/libsystem/swiftui_geom.c). O vidro é fallback: não amostra o fundo. */
double mos_sui_clamp_offset(double offset, double delta, double viewport, double content);
int mos_sui_intersect(double ax, double ay, double aw, double ah, double bx, double by, double bw, double bh,
                      double *x, double *y, double *w, double *h);
void mos_sui_popover_frame(double ax, double ay, double aw, double ah, double pw, double ph, double bx, double by,
                           double bw, double bh, double *x, double *y, double *w, double *h);
void mos_sui_glass_fallback(double *r, double *g, double *b, double *a, double *radius, int *fallback,
                            int *samples_backdrop);
int mos_sui_selftest(char *err, int errlen);
/* Polegar da barra de rolagem. Devolve 0 se o conteúdo cabe ou se o indicador está oculto. */
int mos_sui_indicator(double offset, double viewport, double content, int shows,
                      double vx, double vy, double vw, double vh, int vertical,
                      double *x, double *y, double *w, double *h);
/* TextEditor: offsets em bytes UTF-8. O clique usa floor(x / char_w). */
int mos_sui_ed_index(double x, double char_w, int len);
int mos_sui_ed_selection(int caret, int anchor, int *a, int *b);
void mos_sui_ed_place(int len, int *caret, int *anchor, int index, int extend);
void mos_sui_ed_replace(char *text, int cap, int *len, int *caret, int *anchor, const char *ins);
void mos_sui_ed_backspace(char *text, int cap, int *len, int *caret, int *anchor);
void mos_sui_ed_arrow(const char *text, int len, int *caret, int *anchor, int dir, int shift);
void mos_sui_ed_vertical(const char *text, int len, int *caret, int *anchor, int dir, int shift);
/* Vista do runtime embutido que recebe scrollWheel: (x, y origem no topo, dx, dy = scrollingDeltaY). */
typedef void (*mos_scroll_fn)(void *ctx, double x, double y, double dx, double dy);
void *mos_host_view(double x, double y, double w, double h);
void mos_view_on_scroll(void *view, mos_scroll_fn fn, void *ctx);
void mos_canvas_on_scroll(void *view, mos_scroll_fn fn);
/* NSAlert modal: devolve o índice do botão (0 = primeiro) */
int mos_alert(const char *title, const char *message, const char *b1, const char *b2);
/* Canvas e desenho (dentro do callback do canvas; coordenadas com origem no topo) */
void *mos_canvas(void (*draw)(void *ctx, double w, double h), void *ctx);
void mos_canvas_set_ctx(void *view, void *ctx);
void mos_set_needs_display(void *view);
void mos_draw_rect(double x, double y, double w, double h, double radius, double r, double g, double b, double a, int fill, double lw);
void mos_draw_ellipse(double x, double y, double w, double h, double r, double g, double b, double a, int fill, double lw);
void mos_draw_path(const double *xy, int n, int closed, double r, double g, double b, double a, int fill, double lw);
void mos_draw_text(const char *s, double x, double y, double size, int weight, double r, double g, double b, double a);
/* timer repetitivo */
void *mos_timer(double interval, void (*fn)(void *), void *ctx);
void mos_timer_stop(void *timer);void mos_canvas_input(void *view, void (*mouse)(void *ctx, double x, double y), void (*key)(void *ctx, int ch));
void mos_image_size(const char *name, double *w, double *h);
void mos_draw_image(const char *name, double x, double y, double w, double h);
#include "MOSBridge_path.h"
#include "MOSBridge_input.h"
#include "MOSBridge_image.h"
