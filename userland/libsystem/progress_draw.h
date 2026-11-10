/* Barra de progresso parada: o macOS esconde o indicador quando displayedWhenStopped é falso. */
#pragma once

static inline int cx_progress_draws(int displayed_when_stopped, double value, double minv) {
    if (displayed_when_stopped) return 1;
    return value > minv;
}
