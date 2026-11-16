/* mOS — núcleo do NSSegmentedControl (sem Objective-C): larguras, distribuição, acerto e seleção.
 * A cola fica em appkit_controls.c; o teste de host em userland/tests-appkit/controls_host.c. */
#pragma once
#define SG_MAX 64
enum { SG_SELECT_ONE, SG_SELECT_ANY, SG_MOMENTARY, SG_MOMENTARY_ACCEL };            /* NSSegmentSwitchTracking */
enum { SG_FIT, SG_FILL, SG_FILL_EQUALLY, SG_FILL_PROPORTIONALLY };                  /* NSSegmentDistribution */
typedef struct {
    int n;
    double fixed[SG_MAX];     /* setWidth:forSegment: (0 = automático) */
    double natural[SG_MAX];   /* largura do conteúdo (título/imagem) medida pela cola */
    unsigned char selected[SG_MAX], enabled[SG_MAX];
    int tracking, distribution;
    int selected_one;         /* selectedSegment (-1 = nenhum) */
    int pressed;              /* segmento sob o mouse durante o clique (momentâneo) */
    double x[SG_MAX], w[SG_MAX];
} sg_t;
void sg_init(sg_t *s);
void sg_set_count(sg_t *s, int n);
/* largura mínima de um segmento automático: conteúdo + 2 × 8 de margem, no mínimo 20 */
double sg_auto_width(double natural);
/* posições para a largura total do controle; devolve a largura usada */
double sg_layout(sg_t *s, double total_w);
double sg_intrinsic_width(const sg_t *s);
int sg_hit(const sg_t *s, double x);
/* clique num segmento: aplica o modo de seleção. Devolve 1 se a seleção mudou ou o modo é momentâneo. */
int sg_click(sg_t *s, int i);
void sg_set_selected(sg_t *s, int i, int on);
int sg_selected_segment(const sg_t *s);
