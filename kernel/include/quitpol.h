/* Política de encerrar um app, sem estado e sem o kernel.
 * GEV_QUIT, espera QUIT_GRACE_MS, SIGTERM, espera QUIT_TERM_MS, SIGKILL.
 * Um segundo pedido sobe um estágio. Forçar vai direto ao SIGKILL.
 * Quem manda o sinal é o guisys (proc_signal); este arquivo só decide o estágio. */
#pragma once

enum { QUIT_GRACE_MS = 3000, QUIT_TERM_MS = 2000 };

typedef enum {
    QUIT_IDLE = 0,
    QUIT_GEV = 1,
    QUIT_TERM = 2,
    QUIT_KILL = 3
} quit_stage_t;

static inline quit_stage_t quitpol_step(quit_stage_t stage, int force, int again, int elapsed_ms) {
    if (force) return QUIT_KILL;
    if (stage <= QUIT_IDLE) return QUIT_GEV;
    if (again) return stage == QUIT_GEV ? QUIT_TERM : QUIT_KILL;
    if (stage == QUIT_GEV && elapsed_ms >= QUIT_GRACE_MS) return QUIT_TERM;
    if (stage == QUIT_TERM && elapsed_ms >= QUIT_TERM_MS) return QUIT_KILL;
    return stage;
}
