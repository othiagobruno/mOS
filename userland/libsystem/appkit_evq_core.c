/* mOS libSystem — núcleo da fila de eventos (ver appkit_evq_core.h). Sem libc. */
#include "appkit_evq_core.h"

unsigned evq_ns_type(const gui_event_t *g) {
    switch (g->type) {
    case GEV_MOUSE_DOWN: return 1;                       /* NSEventTypeLeftMouseDown */
    case GEV_MOUSE_UP: return 2;                         /* LeftMouseUp */
    case GEV_RIGHT_DOWN: return 3;                       /* RightMouseDown */
    case GEV_MOUSE_MOVE: return g->buttons & 1 ? 6 : 5;  /* LeftMouseDragged / MouseMoved */
    case GEV_KEY_DOWN: return 10;
    case GEV_KEY_UP: return 11;
    case GEV_FLAGS: return 12;
    case GEV_SCROLL: return 22;                          /* ScrollWheel */
    default: return 0;
    }
}
int evq_matches(const gui_event_t *g, unsigned long long mask) {
    unsigned t = evq_ns_type(g);
    return t && t < 64 && (mask & (1ULL << t));
}
int evq_push(evq_t *q, const gui_event_t *g) {
    if (q->n >= EVQ_MAX) return 0;
    q->ev[q->n++] = *g;
    return 1;
}
int evq_find(const evq_t *q, unsigned long long mask) {
    for (int i = 0; i < q->n; i++) if (evq_matches(&q->ev[i], mask)) return i;
    return -1;
}
void evq_remove(evq_t *q, int i) {
    if (i < 0 || i >= q->n) return;
    for (int k = i; k + 1 < q->n; k++) q->ev[k] = q->ev[k + 1];
    q->n--;
}
int evq_pop_front(evq_t *q, gui_event_t *out) {
    if (!q->n) return 0;
    *out = q->ev[0];
    evq_remove(q, 0);
    return 1;
}
int evq_discard(evq_t *q, unsigned long long mask) {
    int w = 0, gone = 0;
    for (int r = 0; r < q->n; r++) {
        if (evq_matches(&q->ev[r], mask)) { gone++; continue; }
        q->ev[w++] = q->ev[r];
    }
    q->n = w;
    return gone;
}
unsigned evq_mods_from_mac(unsigned long long f) {
    return ((f & (1ULL << 17)) ? 1 : 0) | ((f & (1ULL << 18)) ? 2 : 0) | ((f & (1ULL << 19)) ? 4 : 0) | ((f & (1ULL << 20)) ? 8 : 0);
}
unsigned evq_gev_from_ns(unsigned t) {
    switch (t) {
    case 1: return GEV_MOUSE_DOWN;
    case 2: return GEV_MOUSE_UP;
    case 3: return GEV_RIGHT_DOWN;
    case 5: case 6: return GEV_MOUSE_MOVE;
    case 10: return GEV_KEY_DOWN;
    case 11: return GEV_KEY_UP;
    case 12: return GEV_FLAGS;
    case 22: return GEV_SCROLL;
    default: return 0;
    }
}
