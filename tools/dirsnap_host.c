/* Teste de host do retrato de pasta do readdir (userland/libsystem/dirsnap.inc):
 *   gcc -std=c11 -Wall -Wextra -O1 tools/dirsnap_host.c -o /tmp/dirsnap_host && /tmp/dirsnap_host
 * A pasta falsa lista por índice ao vivo, como o SYS_READDIR do kernel. O caso é o do Codex CLI: o limpador de
 * ~/.codex/tmp/arg0 apaga apply_patch, .lock e applypatch enquanto varre e depois faz rmdir. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../userland/libsystem/dirsnap.inc"

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static char ents[4000][32];
static int nents;
static int lists;
static int fake_list(void *ctx, const char *dir, long idx, char *name, int cap) {
    (void)ctx; (void)dir;
    lists++;
    if (idx < 0 || idx >= nents) return 0;
    snprintf(name, (size_t)cap, "%s", ents[idx]);
    return 1;
}
static void fake_unlink(const char *n) {
    for (int i = 0; i < nents; i++)
        if (!strcmp(ents[i], n)) { memmove(ents[i], ents[i + 1], (size_t)(nents - i - 1) * sizeof ents[0]); nents--; return; }
}
static void set(const char *const *names, int n) { nents = n; for (int i = 0; i < n; i++) snprintf(ents[i], sizeof ents[i], "%s", names[i]); }

int main(void) {
    static const char *const arg0[] = { "apply_patch", ".lock", "applypatch" };
    char name[64];

    /* antes: índice ao vivo, apagando a cada passo -> .lock fica e o rmdir daria ENOTEMPTY */
    set(arg0, 3);
    for (long i = 0; fake_list(0, "/x", i, name, sizeof name) == 1; i++) fake_unlink(name);
    CHECK(nents == 1 && !strcmp(ents[0], ".lock"));

    /* agora: retrato no primeiro readdir */
    set(arg0, 3);
    ds_snap_t s = {0};
    CHECK(ds_fill(&s, fake_list, 0, "/x") == 0 && s.n == 3);
    for (long i = 0; ds_get(&s, i); i++) fake_unlink(ds_get(&s, i));
    CHECK(nents == 0);
    CHECK(ds_get(&s, 3) == 0 && ds_get(&s, -1) == 0);

    /* entrada criada depois do retrato não aparece; rewinddir (ds_free + ds_fill) a vê */
    set(arg0, 3);
    CHECK(ds_fill(&s, fake_list, 0, "/x") == 0);
    snprintf(ents[nents++], sizeof ents[0], "nova");
    long seen = 0;
    for (long i = 0; ds_get(&s, i); i++) seen++;
    CHECK(seen == 3);
    ds_free(&s);
    CHECK(!s.ready && s.n == 0);
    CHECK(ds_fill(&s, fake_list, 0, "/x") == 0 && s.n == 4 && !strcmp(ds_get(&s, 3), "nova"));

    /* pasta vazia e pasta grande (o buffer cresce; 4000 nomes) */
    nents = 0;
    CHECK(ds_fill(&s, fake_list, 0, "/x") == 0 && s.n == 0 && s.ready);
    nents = 4000;
    for (int i = 0; i < nents; i++) snprintf(ents[i], sizeof ents[i], "arquivo-%04d.txt", i);
    CHECK(ds_fill(&s, fake_list, 0, "/x") == 0 && s.n == 4000);
    CHECK(!strcmp(ds_get(&s, 0), "arquivo-0000.txt") && !strcmp(ds_get(&s, 3999), "arquivo-3999.txt"));
    for (long i = 0; ds_get(&s, i); i++) fake_unlink(ds_get(&s, i));
    CHECK(nents == 0);
    ds_free(&s);

    printf("dirsnap_host: %d checagens, %d falhas\n", checks, fails);
    return fails ? 1 : 0;
}
