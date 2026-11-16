/* Teste de host da fila de eventos do AppKit (nextEventMatchingMask:). Roda no Linux, sem VM:
 *   gcc -std=c11 -Wall -Wextra -DCX_EVQ_HOST -Iuserland/libsystem userland/libsystem/appkit_evq_core.c \
 *       userland/tests-appkit/evq_host.c -o /tmp/appkit-evq-test && /tmp/appkit-evq-test */
#include "appkit_evq_core.h"
#include <stdio.h>

static int ok, total;
static void check(int c, const char *w) { total++; if (c) ok++; printf("%s %s\n", c ? "  ok  " : "  FALHA", w); }
static gui_event_t ev(unsigned type, int buttons, int x) { gui_event_t g = {0}; g.type = type; g.buttons = buttons; g.x = x; return g; }

int main(void) {
    gui_event_t a = ev(GEV_MOUSE_MOVE, 1, 0), b = ev(GEV_MOUSE_MOVE, 0, 0), r = ev(GEV_RESIZE, 0, 0);
    check(evq_ns_type(&a) == 6 && evq_ns_type(&b) == 5, "mover com botão = arrastar, sem = mover");
    check(evq_ns_type(&r) == 0, "redimensionar não é NSEvent");
    unsigned long long drag_up = (1ULL << 6) | (1ULL << 2);
    check(evq_matches(&a, drag_up) && !evq_matches(&b, drag_up), "máscara de arrastar/soltar");
    check(!evq_matches(&r, ~0ULL), "evento interno não casa nem com NSEventMaskAny");
    static evq_t q;
    evq_push(&q, &b); evq_push(&q, &r);
    gui_event_t d1 = ev(GEV_MOUSE_MOVE, 1, 10), up = ev(GEV_MOUSE_UP, 0, 20), k = ev(GEV_KEY_DOWN, 0, 30);
    evq_push(&q, &d1); evq_push(&q, &k); evq_push(&q, &up);
    int i = evq_find(&q, drag_up);
    check(i == 2 && q.ev[i].x == 10, "acha o primeiro arrasto, pulando os que não casam");
    evq_remove(&q, i);
    i = evq_find(&q, drag_up);
    check(i == 3 && q.ev[i].type == GEV_MOUSE_UP, "depois o soltar; a tecla fica na fila");
    check(q.n == 4 && q.ev[0].type == GEV_MOUSE_MOVE && q.ev[1].type == GEV_RESIZE && q.ev[2].type == GEV_KEY_DOWN, "ordem preservada");
    check(evq_discard(&q, 1ULL << 10) == 1 && q.n == 3, "descarta só as teclas");
    gui_event_t o;
    check(evq_pop_front(&q, &o) && o.type == GEV_MOUSE_MOVE && q.n == 2, "retira da frente");
    static evq_t full;
    gui_event_t z = ev(GEV_KEY_DOWN, 0, 0);
    int pushed = 0;
    for (int n = 0; n < EVQ_MAX + 5; n++) pushed += evq_push(&full, &z);
    check(pushed == EVQ_MAX, "fila cheia recusa");
    check(evq_mods_from_mac((1ULL << 17) | (1ULL << 20)) == (1 | 8), "modificadores do Mac para o window server");
    check(evq_gev_from_ns(6) == GEV_MOUSE_MOVE && evq_gev_from_ns(22) == GEV_SCROLL && evq_gev_from_ns(15) == 0, "tipo do NSEvent de volta");
    printf("%d/%d\n", ok, total);
    return ok == total ? 0 : 1;
}
