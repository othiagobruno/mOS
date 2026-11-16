/* mOS libSystem — fila de eventos do window server para -[NSApplication nextEventMatchingMask:...] (appkit.c):
 * tipo NSEvent de cada gui_event_t, máscara e a fila FIFO com retirada no meio. Teste: tests-appkit/evq_host.c. */
#ifndef APPKIT_EVQ_CORE_H
#define APPKIT_EVQ_CORE_H
#ifdef CX_EVQ_HOST
#include "../../kernel/include/types.h"
#else
#include "sys.h"
#endif
#include "../../kernel/include/compat_gui.h"

#define EVQ_MAX 512
typedef struct { gui_event_t ev[EVQ_MAX]; int n; } evq_t;

/* NSEventType do evento (0 = evento interno do window server: fechar, redimensionar, menu, arrasto...) */
unsigned evq_ns_type(const gui_event_t *g);
/* o evento entra na máscara (NSEventMask: bit 1 << tipo; ~0 = todos)? */
int evq_matches(const gui_event_t *g, unsigned long long mask);
int evq_push(evq_t *q, const gui_event_t *g);              /* 0 se a fila está cheia */
int evq_find(const evq_t *q, unsigned long long mask);     /* índice do primeiro que casa, ou -1 */
void evq_remove(evq_t *q, int i);
int evq_pop_front(evq_t *q, gui_event_t *out);
/* retira todos os que casam com a máscara; devolve quantos */
int evq_discard(evq_t *q, unsigned long long mask);
/* NSEventModifierFlags (shift 1<<17, control 1<<18, option 1<<19, command 1<<20) ↔ MOD_* do window server */
unsigned evq_mods_from_mac(unsigned long long flags);
/* NSEventType de mouse/teclado → gui_event_t (o inverso de evq_ns_type); 0 se não há equivalente */
unsigned evq_gev_from_ns(unsigned ns_type);
#endif
