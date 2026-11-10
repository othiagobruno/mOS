#define _POSIX_C_SOURCE 200809L
/* gcc -std=c11 -Wall -Wextra -O1 -DCD_HOST tools/cdtest.c -o /tmp/cdtest && /tmp/cdtest
 * Teste de host da loja MOSCD1. Não é o libSystem ligado, não é a VM e não é SQLite. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#define CD_HOST 1
#include "../userland/libsystem/coredata_store.inc"

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static unsigned nfetch(cd_store *s, const char *ent, const char *k, const char *v) {
    unsigned n = 0;
    unsigned long *ids = cd_fetch(s, ent, k, v, 0, 0, &n);
    free(ids);
    return n;
}

int main(void) {
    char path[] = "/tmp/moscd-test-XXXXXX";
    int fd = mkstemp(path);
    cd_store *m, *a, *b;
    unsigned long p, q, t;
    unsigned n = 0;
    unsigned long *ids;
    CHECK(fd >= 0);
    if (fd >= 0) close(fd);
    unlink(path);

    m = cd_open_memory();
    p = cd_insert(m, "Person");
    CHECK(p != 0);
    CHECK(cd_set_str(m, p, "name", "Ana") == 0);
    CHECK(cd_set_i64(m, p, "age", 30) == 0);
    CHECK(cd_set_f64(m, p, "score", 1.5) == 0);
    CHECK(cd_get_kind(m, p, "score") == 3);
    CHECK(cd_get_f64(m, p, "score") == 1.5);
    CHECK(strcmp(cd_entity_of(m, p), "Person") == 0);
    CHECK(nfetch(m, "Person", "name", "Ana") == 1);
    CHECK(nfetch(m, "Person", "name", "Bob") == 0);
    CHECK(cd_save(m) == 0);
    cd_close(m);

    a = cd_open_file(path);
    CHECK(a != 0);
    p = cd_insert(a, "Person");
    q = cd_insert(a, "Person");
    t = cd_insert(a, "Team");
    CHECK(cd_set_str(a, p, "name", "Ana") == 0);
    CHECK(cd_set_i64(a, p, "age", 30) == 0);
    CHECK(cd_set_f64(a, p, "score", 1.5) == 0);
    CHECK(cd_set_str(a, q, "name", "Bob\t\n") == 0);
    CHECK(cd_set_str(a, t, "name", "mOS") == 0);
    CHECK(cd_save(a) == 0);
    cd_close(a);

    b = cd_open_file(path);
    CHECK(b != 0);
    CHECK(nfetch(b, "Person", 0, 0) == 2);
    CHECK(nfetch(b, "Team", 0, 0) == 1);
    CHECK(nfetch(b, "Person", "name", "Ana") == 1);
    ids = cd_fetch(b, "Person", "name", "Bob\t\n", 0, 0, &n);
    CHECK(n == 1 && ids && strcmp(cd_get_str(b, ids[0], "name"), "Bob\t\n") == 0);
    free(ids);
    ids = cd_fetch(b, "Person", "age", "30", 0, 0, &n);
    CHECK(n == 1 && ids && cd_get_i64(b, ids[0], "age") == 30 && cd_get_f64(b, ids[0], "score") == 1.5);
    free(ids);
    ids = cd_fetch(b, "Person", 0, 0, 1, 0, &n);
    CHECK(n == 1);
    free(ids);
    cd_close(b);

    b = cd_open_file(path);
    ids = cd_fetch(b, "Person", "name", "Ana", 0, 0, &n);
    CHECK(n == 1 && ids);
    if (ids) cd_delete(b, ids[0]);
    free(ids);
    CHECK(cd_save(b) == 0);
    cd_close(b);
    b = cd_open_file(path);
    CHECK(nfetch(b, "Person", "name", "Ana") == 0);
    CHECK(nfetch(b, "Person", 0, 0) == 1);
    CHECK(nfetch(b, "Team", "name", "mOS") == 1);
    cd_close(b);

    {
        FILE *bad = fopen(path, "wb");
        CHECK(bad != 0);
        if (bad) { fputs("nope\n", bad); fclose(bad); }
        CHECK(cd_open_file(path) == 0);
    }
    m = cd_open_memory();
    CHECK(nfetch(m, "Person", 0, 0) == 0);
    cd_close(m);
    unlink(path);

    printf("%d checks, %d fails\n", checks, fails);
    return fails ? 1 : 0;
}
