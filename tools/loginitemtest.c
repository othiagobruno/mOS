/* gcc -std=c11 -Wall -Wextra -O1 -D_GNU_SOURCE -Ikernel/include tools/loginitemtest.c -o /tmp/loginitemtest && /tmp/loginitemtest */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#define LI_HOST 1
#define LI_SYNC 1
#include "loginitem.inc"

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static int has_line(const char *file, const char *path) {
    char paths[LI_MAX][LI_PATH];
    int n = li_file_list(file, paths, LI_MAX), i;
    for (i = 0; i < n; i++) if (!strcmp(paths[i], path)) return 1;
    return 0;
}

int main(void) {
    char dir[] = "/tmp/mos-li-XXXXXX";
    char file[256];
    char paths[LI_MAX][LI_PATH];
    char text[LI_TEXT], out[LI_TEXT];
    li_slot_t slots[LI_MAX];
    int n, up;
    const char *a = "/Aplicativos/Ficar.app";
    const char *b = "/Aplicativos/Sair.app";
    if (!mkdtemp(dir)) { perror("mkdtemp"); return 1; }
    snprintf(file, sizeof file, "%s/com.mos.loginitems", dir);

    CHECK(li_file_add(file, a) == 0);
    {
        FILE *f = fopen(file, "rb");
        char got[256] = {0};
        if (f) { size_t nr = fread(got, 1, sizeof got - 1, f); got[nr] = 0; fclose(f); }
        printf("login semente inicial: %u\n", li_seed(got));
    }
    CHECK(li_file_add(file, b) == 0);
    CHECK(li_file_add(file, a) == 0);
    CHECK(li_file_add(file, "relativo") != 0);
    CHECK(li_file_add(file, "/tmp/../etc/passwd") != 0);
    n = li_file_list(file, paths, LI_MAX);
    CHECK(n == 2);
    CHECK(has_line(file, a));
    CHECK(has_line(file, b));
    printf("login lista: %d\n  %s\n  %s\n", n, paths[0], paths[1]);

    CHECK(li_file_remove(file, b) == 0);
    n = li_file_list(file, paths, LI_MAX);
    CHECK(n == 1);
    CHECK(has_line(file, a));
    CHECK(!has_line(file, b));
    {
        FILE *f = fopen(file, "rb");
        char got[512] = {0};
        size_t nr;
        CHECK(f != 0);
        if (f) { nr = fread(got, 1, sizeof got - 1, f); got[nr] = 0; fclose(f); }
        CHECK(strstr(got, "Sair.app") == 0);
        CHECK(strstr(got, "Ficar.app") != 0);
        printf("login arquivo depois de remover:\n%s", got);
    }

    memset(slots, 0, sizeof slots);
    n = li_file_list(file, paths, LI_MAX);
    up = li_sync(slots, LI_MAX, paths, n);
    CHECK(up == 1);
    CHECK(slots[0].used && slots[0].runatload && !slots[0].last_start);
    CHECK(strcmp(slots[0].label, "mos.login.Ficar.app") == 0);
    printf("login init subiria: %d (%s)\n", up, slots[0].label);

    n = 0;
    up = li_sync(slots, LI_MAX, paths, n);
    CHECK(up == 0);
    CHECK(!slots[0].used);
    printf("login init depois de remover: %d\n", up);

    memset(slots, 0, sizeof slots);
    li_copy(slots[0].path, LI_PATH, b);
    li_copy(slots[0].label, (int)sizeof slots[0].label, "mos.login.Sair.app");
    slots[0].used = 1;
    slots[0].pid = 7;
    slots[0].last_start = 1;
    slots[0].runatload = 1;
    up = li_sync(slots, LI_MAX, paths, 0);
    CHECK(up == 0);
    CHECK(slots[0].used && slots[0].pid == 7 && !slots[0].runatload && !slots[0].keep);
    printf("login em execução não sobe de novo: pid=%d runatload=%d\n", slots[0].pid, slots[0].runatload);

    CHECK(li_text_add("", "/bin/ok", out, LI_TEXT) == 0);
    CHECK(li_text_remove(out, "/bin/ok", text, LI_TEXT) == 0);
    CHECK(text[0] == 0);

    printf("loginitemtest: %d/%d PASS\n", checks - fails, checks);
    snprintf(out, sizeof out, "rm -rf '%s'", dir);
    if (system(out)) return 1;
    return fails ? 1 : 0;
}
