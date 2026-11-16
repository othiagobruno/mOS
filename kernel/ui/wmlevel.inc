/* mOS — ordem das janelas por nível (NSWindowLevel) e escolha da janela em foco. Sem desenho nem estado global: o wm.c
 * passa a lista; tools/wmlevel_host.c testa no Linux. */

/* nível efetivo: o da janela ou o maior entre as donas (filhas de um painel flutuante ficam com ele) */
static int wl_eff_level(const window_t *w) {
    int l = w->level;
    for (int d = 0; w->owner && d < 8; d++) { w = w->owner; if (w->level > l) l = w->level; }
    return l;
}

/* ordenação estável por nível efetivo (v[0] = fundo): dentro do mesmo nível a ordem não muda. true = mexeu. */
static bool wl_restack(window_t **v, int n) {
    bool moved = false;
    for (int i = 1; i < n; i++) {
        window_t *w = v[i];
        int l = wl_eff_level(w), j = i;
        while (j > 0 && wl_eff_level(v[j - 1]) > l) { v[j] = v[j - 1]; j--; }
        if (j != i) { v[j] = w; moved = true; }
    }
    return moved;
}

/* janela em foco: de cima para baixo, a primeira utilizável que é a key (último wm_focus) ou de nível normal; janelas
 * com nível só ficam em foco quando foram focadas. Só há janelas com nível: a mais alta. */
static window_t *wl_focused(window_t **v, int n, const window_t *key, bool (*usable)(window_t *)) {
    window_t *top = NULL;
    for (int i = n - 1; i >= 0; i--) {
        window_t *w = v[i];
        if (!usable(w)) continue;
        if (!top) top = w;
        if (w == key || wl_eff_level(w) <= 0) return w;
    }
    return top;
}
