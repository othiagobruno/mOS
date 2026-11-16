/* mOS libSystem — núcleo sem ObjC das classes de appkit_more.c (testado em userland/tests-appkit/more_host.c):
 * curvas de NSAnimation, máquinas de estado dos reconhecedores de gesto, geometria e seleção do NSMatrix,
 * valor do NSSliderCell/NSStepperCell e cabeçalhos de EPS (NSEPSImageRep). */
#ifndef APPKIT_MORE_CORE_H
#define APPKIT_MORE_CORE_H

/* NSAnimationCurve: EaseInOut 0, EaseIn 1, EaseOut 2, Linear 3 */
double amc_curve(int curve, double p);

/* NSGestureRecognizerState */
enum { GR_POSSIBLE = 0, GR_BEGAN = 1, GR_CHANGED = 2, GR_ENDED = 3, GR_CANCELLED = 4, GR_FAILED = 5 };
enum { GR_K_PAN = 0, GR_K_PRESS = 1, GR_K_CLICK = 2, GR_K_MAGNIFY = 3, GR_K_ROTATE = 4 };
/* eventos que alimentam a máquina: mouse e gestos de trackpad (NSEventTypeMagnify/Rotate, fases do NSEvent) */
enum { GE_DOWN = 0, GE_DRAG = 1, GE_UP = 2, GE_TICK = 3, GE_MAG = 4, GE_ROT = 5 };
typedef struct {
    int kind, state;
    int button_mask;            /* bits dos botões que valem (1 = esquerdo, 2 = direito) */
    int clicks_required;        /* NSClickGestureRecognizer.numberOfClicksRequired */
    double min_duration;        /* NSPressGestureRecognizer.minimumPressDuration (s) */
    double allowable;           /* movimento tolerado antes de falhar (press/click) */
    double x0, y0, t0;          /* onde e quando começou */
    double x, y;                /* posição atual (coordenadas da janela) */
    double magnification, rotation;   /* acumulados (magnify: fração; rotate: graus, como no NSEvent) */
    int down;
} gr_core;
void gr_init(gr_core *g, int kind);
/* alimenta um evento; devolve 1 se o estado mudou de um jeito que dispara a ação (Began/Changed/Ended/recognized) */
int gr_feed(gr_core *g, int ev, double x, double y, double t, int button, int clicks, double amount, int phase_end);
/* depois da ação: Ended/Cancelled/Failed voltam a Possible (o AppKit faz o reset no fim do ciclo) */
void gr_reset_if_done(gr_core *g);

/* NSMatrix */
typedef struct { int rows, cols; double cw, ch, sx, sy; } mx_geom;
/* célula sob o ponto (coordenadas da matriz, y para baixo); devolve 0 se caiu no espaçamento ou fora */
int mx_hit(const mx_geom *g, double x, double y, int *row, int *col);
void mx_cell_frame(const mx_geom *g, int row, int col, double *x, double *y, double *w, double *h);
/* NSMatrixMode: Radio 0, Highlight 1, List 2, Track 3. sel[] tem rows*cols estados (0/1).
 * mods: 1 = shift (estende), 2 = ⌘ (alterna). anchor guarda o índice de partida para shift. */
void mx_click(int mode, int allows_empty, unsigned char *sel, int n, int idx, int mods, int *anchor);

/* NSSliderCell: valor sob a posição ao longo do trilho; snap às marcas quando only_ticks */
double sl_value_at(double pos, double track_start, double track_len, double minv, double maxv);
double sl_closest_tick(double v, double minv, double maxv, int ticks);
double sl_knob_pos(double v, double track_start, double track_len, double minv, double maxv);
/* NSStepperCell: passo com valueWraps */
double st_step(double v, double inc, double minv, double maxv, int up, int wraps);

/* EPS: cabeçalho binário DOS (C5 D0 D3 C6) e %%BoundingBox. Devolve 1 se achou. */
int eps_dos_header(const unsigned char *b, unsigned long n, unsigned long *ps_off, unsigned long *ps_len,
                   unsigned long *tiff_off, unsigned long *tiff_len);
int eps_bbox(const unsigned char *ps, unsigned long n, double *x, double *y, double *w, double *h);

#endif
