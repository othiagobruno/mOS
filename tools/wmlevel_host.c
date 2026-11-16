/* Teste no Linux da ordem por nível e do foco do window server (kernel/ui/wmlevel.inc).
 * gcc -O2 -Wall -Wextra -fno-builtin -DMOS_HOST_TEST -I kernel/include -I kernel/ui tools/wmlevel_host.c -o /tmp/wmlevel_host */
/* sem <stdio.h>/<string.h>: o kernel.h declara memcpy e afins com outro tipo */
#include "ui.h"
int printf(const char *f, ...);
int strcmp(const char *a, const char *b);
#include "wmlevel.inc"

static int fails;
#define CHECK(c, m) do { if (!(c)) { printf("FALHOU: %s\n", m); fails++; } } while (0)

static bool usable(window_t *w) { return w->visible && !w->minimized && !w->closing; }

static window_t W[8];
static window_t *v[8];
static int n;

/* o que o wm_focus faz: põe no topo, marca a key, reordena */
static window_t *focus(window_t *w, window_t **key) {
    int i = 0;
    while (v[i] != w) i++;
    for (; i < n - 1; i++) v[i] = v[i + 1];
    v[n - 1] = w;
    *key = w;
    wl_restack(v, n);
    return wl_focused(v, n, *key, usable);
}

static const char *order(void) {
    static char s[16];
    for (int i = 0; i < n; i++) s[i] = (char)('A' + (v[i] - W));
    s[n] = 0;
    return s;
}

int main(void) {
    for (int i = 0; i < 8; i++) { W[i] = (window_t){0}; W[i].visible = true; }
    window_t *A = &W[0], *B = &W[1], *C = &W[2], *D = &W[3], *E = &W[4], *key = NULL;
    v[0] = A; v[1] = B; v[2] = C; n = 3;

    /* sem níveis: nada muda, foco = topo */
    CHECK(!wl_restack(v, n), "sem nível não reordena");
    CHECK(wl_focused(v, n, NULL, usable) == C, "sem nível o foco é o topo");

    /* A vira painel flutuante (3): sobe acima de B e C */
    A->level = 3;
    CHECK(wl_restack(v, n) && !strcmp(order(), "BCA"), "painel sobe");
    /* foco em B: B sobe entre as normais, o painel continua no topo, B em foco */
    CHECK(focus(B, &key) == B && !strcmp(order(), "CBA"), "foco numa normal não passa do painel");
    /* clique no painel: ele fica em foco */
    CHECK(focus(A, &key) == A && !strcmp(order(), "CBA"), "painel focado");
    /* janela nova normal (D) entra abaixo do painel e, sem foco explícito, vira a em foco (é a normal do topo) */
    A->level = 3; key = B;
    v[n++] = D;
    wl_restack(v, n);
    CHECK(!strcmp(order(), "CBDA"), "janela nova fica abaixo do painel");
    /* como sem níveis: a normal do topo (a nova) fica em foco, mesmo com outra key mais abaixo */
    CHECK(wl_focused(v, n, key, usable) == D, "janela nova normal fica em foco");
    /* key num painel abaixo de outro painel: vale a key */
    CHECK(wl_focused(v, n, A, usable) == A, "painel focado continua em foco");

    /* filha (E) de um painel herda o nível e fica acima dele quando adicionada depois */
    E->owner = A;
    v[n++] = E;
    CHECK(!wl_restack(v, n) && !strcmp(order(), "CBDAE"), "filha do painel continua acima");
    /* popup (101) acima de status (25) acima de modal (8) */
    B->level = 101; C->level = 25; D->level = 8;
    wl_restack(v, n);
    CHECK(!strcmp(order(), "AEDCB"), "ordem 3 < 8 < 25 < 101");
    /* volta ao normal: B desce para o fundo do grupo normal mantendo a ordem relativa */
    B->level = 0; C->level = 0; D->level = 0;
    wl_restack(v, n);
    CHECK(!strcmp(order(), "DCBAE"), "nível 0 de novo fica abaixo do painel");

    /* painel escondido: o foco cai na normal do topo */
    key = A; A->visible = false;
    CHECK(wl_focused(v, n, key, usable) == B, "painel escondido perde o foco");
    /* só janelas com nível: a mais alta */
    for (int i = 0; i < n; i++) if (v[i] != E) v[i]->visible = false;
    CHECK(wl_focused(v, n, NULL, usable) == E, "só flutuantes: a do topo");

    if (fails) { printf("%d falha(s)\n", fails); return 1; }
    printf("wmlevel: todos os testes passaram\n");
    return 0;
}
