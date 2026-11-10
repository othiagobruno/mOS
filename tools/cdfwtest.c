#define _POSIX_C_SOURCE 200809L
/* gcc -std=c11 -Wall -Wextra -O1 -DCD_HOST tools/cdfwtest.c tools/cdfw_host.c userland/libsystem/sqlite3_mos.c -o /tmp/cdfwtest -lm
 * Mesma loja que a dylib CoreData.framework abre. Não é a VM. */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include "../userland/frameworks/CoreData/cd_api.h"

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

int main(void) {
    char path[] = "/tmp/moscd-fw-XXXXXX";
    int fd = mkstemp(path);
    cd_store *a, *b, *m;
    unsigned long p, q;
    unsigned n = 0;
    unsigned long *ids;
    char key[64], val[64];
    CHECK(fd >= 0);
    if (fd >= 0) close(fd);
    unlink(path);

    CHECK(moscd_split_eq("name == \"Ana\"", key, sizeof key, val, sizeof val) == 1);
    CHECK(strcmp(key, "name") == 0 && strcmp(val, "Ana") == 0);
    CHECK(moscd_split_eq("age == 30", key, sizeof key, val, sizeof val) == 1);
    CHECK(strcmp(val, "30") == 0);

    m = moscd_open_memory();
    p = moscd_insert(m, "Person");
    CHECK(moscd_set_str(m, p, "name", "Ana") == 0);
    CHECK(moscd_save(m) == 0);
    moscd_close(m);
    CHECK(access(path, 0) != 0);

    a = moscd_open_file(path);
    CHECK(a != 0);
    p = moscd_insert(a, "Person");
    q = moscd_insert(a, "Person");
    CHECK(moscd_set_str(a, p, "name", "Ana") == 0);
    CHECK(moscd_set_i64(a, p, "age", 30) == 0);
    CHECK(moscd_set_str(a, q, "name", "Bob") == 0);
    CHECK(moscd_save(a) == 0);
    moscd_close(a);

    b = moscd_open_file(path);
    CHECK(b != 0);
    ids = moscd_fetch(b, "Person", "name", "Ana", 0, 0, &n);
    CHECK(n == 1 && ids && moscd_get_i64(b, ids[0], "age") == 30);
    free(ids);
    ids = moscd_fetch(b, "Person", "name", "Bob", 0, 0, &n);
    CHECK(n == 1 && ids);
    if (ids) moscd_delete(b, ids[0]);
    free(ids);
    CHECK(moscd_save(b) == 0);
    moscd_close(b);

    b = moscd_open_file(path);
    ids = moscd_fetch(b, "Person", "name", "Ana", 0, 0, &n);
    CHECK(n == 1);
    free(ids);
    ids = moscd_fetch(b, "Person", "name", "Bob", 0, 0, &n);
    CHECK(n == 0);
    free(ids);
    moscd_close(b);

    {
        char cmd[640], buf[128];
        FILE *sp;
        snprintf(cmd, sizeof cmd, "sqlite3 '%s' \"SELECT vtext FROM mos_attr WHERE key='name';\"", path);
        sp = popen(cmd, "r");
        CHECK(sp != 0);
        buf[0] = 0;
        if (sp) {
            CHECK(fgets(buf, sizeof buf, sp) != 0);
            CHECK(strncmp(buf, "Ana", 3) == 0);
            pclose(sp);
        }
    }
    unlink(path);
    {
        FILE *old = fopen(path, "wb");
        cd_store *mig;
        CHECK(old != 0);
        if (old) {
            fputs("MOSCD1\nO\t7\tPerson\nA\t7\ts\tname\tAna\nA\t7\ti\tage\t30\n", old);
            fclose(old);
        }
        mig = moscd_open_file(path);
        CHECK(mig != 0);
        ids = moscd_fetch(mig, "Person", "name", "Ana", 0, 0, &n);
        CHECK(n == 1 && ids && moscd_get_i64(mig, ids[0], "age") == 30);
        free(ids);
        if (mig) moscd_close(mig);
        {
            char cmd[640], buf[64];
            FILE *sp;
            snprintf(cmd, sizeof cmd, "sqlite3 '%s' \"SELECT vint FROM mos_attr WHERE key='age';\"", path);
            sp = popen(cmd, "r");
            CHECK(sp != 0);
            buf[0] = 0;
            if (sp) {
                CHECK(fgets(buf, sizeof buf, sp) != 0);
                CHECK(strncmp(buf, "30", 2) == 0);
                pclose(sp);
            }
        }
    }
    unlink(path);
    printf("%d checks, %d fails\n", checks, fails);
    return fails ? 1 : 0;
}
