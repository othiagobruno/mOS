/* Teste de host da tabela do CFNotificationCenter (cfnotify_core.inc).
 *   gcc -std=c11 -Wall -Wextra -O1 -DCFN_HOST tools/cfnotifytest.c -o /tmp/cfnotifytest
 *   /tmp/cfnotifytest
 */
#define _POSIX_C_SOURCE 200809L
#define CFN_HOST 1
#include "../userland/libsystem/cfnotify_core.inc"
#include <stdio.h>

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static int hits;
static char last[64];
static void *last_obj;
static void *last_info;
static cfn_tab *g;
static void *center_a = (void *)1;
static void *center_b = (void *)2;

static void on_note(void *center, void *observer, void *name_token, void *object, void *info) {
    (void)center;
    (void)observer;
    hits++;
    last[0] = 0;
    if (name_token) snprintf(last, sizeof last, "%s", (const char *)name_token);
    last_obj = object;
    last_info = info;
}

static void on_remove_self(void *center, void *observer, void *name_token, void *object, void *info) {
    (void)name_token; (void)object; (void)info;
    hits++;
    cfn_remove(g, center, observer, "a", 0, 0, 1);
}

static void on_repost(void *center, void *observer, void *name_token, void *object, void *info) {
    (void)observer; (void)object; (void)info;
    hits++;
    if (hits < 4) cfn_post(g, center, "b", name_token, 0, 0);
}

int main(void) {
    g = cfn_new();
    void *obs = (void *)10;
    void *other = (void *)11;
    void *obj = (void *)20;
    cfn_add(g, center_a, obs, on_note, "tema", 0);
    cfn_add(g, center_a, other, on_note, "outro", 0);
    cfn_add(g, center_a, obs, on_note, 0, 0); /* qualquer nome */
    cfn_add(g, center_b, obs, on_note, "tema", 0);
    cfn_add(g, center_a, obs, on_note, "click", obj);

    hits = 0;
    CHECK(cfn_post(g, center_a, "tema", "tema", 0, (void *)1) == 2); /* específico + qualquer */
    CHECK(hits == 2);
    CHECK(strcmp(last, "tema") == 0);
    CHECK(last_info == (void *)1);
    hits = 0;
    CHECK(cfn_post(g, center_a, "click", "click", (void *)99, 0) == 1); /* objeto não bate; só o "qualquer" */
    CHECK(cfn_post(g, center_a, "click", "click", obj, 0) == 2);
    hits = 0;
    CHECK(cfn_post(g, center_b, "tema", "tema", 0, 0) == 1);
    CHECK(cfn_post(g, center_a, "nao-tem", "nao-tem", 0, 0) == 1); /* só o observador de nome NULL */

    cfn_remove(g, center_a, obs, "tema", 0, 0, 1);
    CHECK(cfn_count(g, center_a, "tema") == 1); /* sobra o observador de nome NULL, que também casa */
    hits = 0;
    int ran = cfn_post(g, center_a, "tema", "tema", 0, 0);
    CHECK(ran == 1);

    cfn_remove(g, center_a, obs, 0, 0, 1, 1);
    CHECK(cfn_post(g, center_a, "tema", "tema", 0, 0) == 0);
    CHECK(cfn_post(g, center_b, "tema", "tema", 0, 0) == 1); /* outro centro intacto */

    void *killer = (void *)30;
    cfn_add(g, center_a, killer, on_remove_self, "a", 0);
    cfn_add(g, center_a, killer, on_note, "a", 0);
    hits = 0;
    /* a ordem de inserção é a inversa: on_note foi por último, então é o primeiro da lista.
     * Remover "a" no callback do outro não pode derrubar quem já foi tirado; o que ainda está
     * vivo e foi marcado no snapshot deixa de rodar se alive cair antes da chamada. */
    cfn_post(g, center_a, "a", "a", 0, 0);
    CHECK(hits >= 1);
    CHECK(cfn_count(g, center_a, "a") == 0);

    void *rep = (void *)40;
    cfn_add(g, center_a, rep, on_repost, "b", 0);
    hits = 0;
    cfn_post(g, center_a, "b", "b", 0, 0);
    CHECK(hits == 4); /* 1 + reentrâncias até o teto do próprio callback */

    cfn_free(g);
    if (fails) {
        fprintf(stderr, "cfnotifytest: %d falhas em %d checagens\n", fails, checks);
        return 1;
    }
    printf("cfnotifytest: %d/%d PASS\n", checks, checks);
    return 0;
}
