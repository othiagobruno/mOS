#define _POSIX_C_SOURCE 200809L
/* Teste de host do núcleo de preferências (userland/libsystem/prefs_core.inc).
 * Não sobe a VM: confere o plist XML, a ordem de busca e a gravação atômica
 * num diretório temporário.
 *
 *   gcc -std=c11 -Wall -Wextra -O1 -DPF_HOST tools/prefstest.c -o /tmp/prefstest
 *   /tmp/prefstest
 */
#define PF_HOST 1
#include "../userland/libsystem/prefs_core.inc"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static int g_fail_write;

static int host_read(void *ctx, const char *path, char **buf, size_t *len, unsigned long long *stamp) {
    (void)ctx;
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return -1; }
    rewind(f);
    char *b = (char *)malloc((size_t)sz + 1);
    if (!b) { fclose(f); return -1; }
    size_t n = fread(b, 1, (size_t)sz, f);
    fclose(f);
    b[n] = 0;
    *buf = b;
    *len = n;
    if (stamp) *stamp = pf_fnv(b, n);
    return 0;
}
static int host_write(void *ctx, const char *path, const char *buf, size_t len) {
    (void)ctx;
    if (g_fail_write) return -1;
    char tmp[1200];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) return -1;
    if (fwrite(buf, 1, len, f) != len) { fclose(f); unlink(tmp); return -1; }
    if (fclose(f) != 0) { unlink(tmp); return -1; }
    if (rename(tmp, path) != 0) { unlink(tmp); return -1; }
    return 0;
}
static int host_stamp(void *ctx, const char *path, unsigned long long *stamp) {
    char *b = 0;
    size_t n = 0;
    if (host_read(ctx, path, &b, &n, stamp) != 0) return -1;
    free(b);
    return 0;
}

static const pf_fs fs = {0, host_read, host_write, host_stamp};

static void test_roundtrip(void) {
    pf_val *d = pf_dict();
    pf_dict_put(d, "nome", pf_str("a&b<c>"));
    pf_dict_put(d, "n", pf_int(-42));
    pf_dict_put(d, "ok", pf_bool(1));
    pf_dict_put(d, "no", pf_bool(0));
    pf_dict_put(d, "r", pf_real(1.5));
    unsigned char raw[4] = {0, 1, 255, 10};
    pf_dict_put(d, "bin", pf_data(raw, 4));
    pf_dict_put(d, "quando", pf_date(1700000000.5));
    pf_val *arr = pf_array();
    pf_array_add(arr, pf_str("um"));
    pf_array_add(arr, pf_int(2));
    pf_dict_put(d, "lista", arr);
    pf_val *sub = pf_dict();
    pf_dict_put(sub, "k", pf_str("v"));
    pf_dict_put(d, "sub", sub);
    size_t n = 0;
    char *xml = pf_xml_write(d, &n);
    CHECK(xml && n > 0);
    CHECK(strstr(xml, "&amp;") != 0);
    CHECK(strstr(xml, "&lt;") != 0);
    int kind = 0;
    pf_val *back = pf_xml_parse(xml, n, &kind);
    CHECK(kind == 0);
    pf_val *copied = pf_copy(d);
    CHECK(pf_equal(d, copied));
    pf_free(copied);
    CHECK(pf_equal(d, back));
    CHECK(!pf_equal(pf_dict_get(back, "ok"), pf_dict_get(back, "n"))); /* bool não é int */
    pf_val *one = pf_int(1);
    CHECK(!pf_equal(pf_dict_get(back, "ok"), one));
    pf_free(one);
    /* entidade numérica */
    const char *ent = "<dict><key>c</key><string>&#65;&#x42;</string></dict>";
    pf_val *e = pf_xml_parse(ent, strlen(ent), &kind);
    CHECK(e && pf_dict_get(e, "c") && strcmp(pf_dict_get(e, "c")->str, "AB") == 0);
    pf_free(e);
    pf_free(back);
    pf_free(d);
    free(xml);
    CHECK(pf_date_unix("2024-03-05T00:00:00Z") == 1709596800.0);
    CHECK(pf_date_unix("1709596800") == 1709596800.0);
    const char *iso = "<?xml version=\"1.0\"?><plist version=\"1.0\"><dict><key>d</key><date>2024-03-05T00:00:00Z</date></dict></plist>";
    int iso_kind = 0;
    pf_val *iso_d = pf_xml_parse(iso, strlen(iso), &iso_kind);
    CHECK(iso_d && pf_dict_get(iso_d, "d") && pf_dict_get(iso_d, "d")->d == 1709596800.0);
    pf_free(iso_d);
}

static void test_search_and_file(const char *dir) {
    char user[1100], any[1100], glob[1100], binp[1100];
    snprintf(user, sizeof user, "%s/com.visualstudio.code.plist", dir);
    snprintf(any, sizeof any, "%s/any-com.visualstudio.code.plist", dir);
    snprintf(glob, sizeof glob, "%s/.GlobalPreferences.plist", dir);
    snprintf(binp, sizeof binp, "%s/com.binary.plist", dir);
    pf_db *db = pf_db_new();
    CHECK(pf_domain_ok("com.visualstudio.code"));
    CHECK(pf_domain_ok(".GlobalPreferences"));
    CHECK(!pf_domain_ok("../etc"));
    CHECK(!pf_domain_ok("a/b"));
    CHECK(!pf_domain_ok(""));
    CHECK(pf_put(db, "user", "../x", "k", pf_str("no")) == -1);

    /* registro não entra no arquivo: só na busca, por último */
    CHECK(pf_put(db, "reg", "app", "tema", pf_str("claro")) == 1);
    CHECK(pf_put(db, "reg", "app", "tema", pf_str("claro")) == 0); /* igual, não suja de novo além do primeiro */
    CHECK(pf_put(db, "user", ".GlobalPreferences", "AppleLanguages", pf_str("pt-BR")) == 1);
    CHECK(pf_put(db, "user", "com.visualstudio.code", "window", pf_int(1)) == 1);
    CHECK(pf_put(db, "any", "com.visualstudio.code", "sistema", pf_str("mos")) == 1);
    CHECK(pf_put(db, "user", "com.apple.universalaccess", "mouseDriverCursorSize", pf_real(1.0)) == 1);

    const char *sc[] = {"user", "user", "user", "any", "reg"};
    const char *nm[] = {"com.visualstudio.code", "com.apple.suite", ".GlobalPreferences", "com.visualstudio.code", "app"};
    int which = -1;
    CHECK(pf_search(db, sc, nm, 5, "window", &which) && which == 0);
    CHECK(pf_search(db, sc, nm, 5, "AppleLanguages", &which) && which == 2);
    CHECK(pf_search(db, sc, nm, 5, "sistema", &which) && which == 3);
    CHECK(pf_search(db, sc, nm, 5, "tema", &which) && which == 4);
    /* valor persistente esconde o registro; remover faz o registro voltar */
    CHECK(pf_put(db, "user", "com.visualstudio.code", "tema", pf_str("escuro")) == 1);
    CHECK(pf_search(db, sc, nm, 5, "tema", &which) && which == 0);
    CHECK(strcmp(pf_search(db, sc, nm, 5, "tema", &which)->str, "escuro") == 0);
    CHECK(pf_put(db, "user", "com.visualstudio.code", "tema", 0) == 1);
    CHECK(pf_search(db, sc, nm, 5, "tema", &which) && which == 4);
    pf_val *repl = pf_dict();
    pf_dict_put(repl, "so", pf_str("um"));
    CHECK(pf_replace(db, "user", "com.apple.suite", repl) == 1);
    CHECK(pf_get_mem(db, "user", "com.apple.suite", "so") != 0);
    pf_clear(db, "user", "com.apple.suite");
    CHECK(pf_get_mem(db, "user", "com.apple.suite", "so") == 0);
    CHECK(pf_is_dirty(db, "user", "com.apple.suite"));

    /* CopyAppValue não vê o registro: app, global, any-user */
    const char *asc[] = {"user", "user", "any"};
    const char *anm[] = {"com.visualstudio.code", ".GlobalPreferences", "com.visualstudio.code"};
    CHECK(pf_search(db, asc, anm, 3, "tema", &which) == 0);
    CHECK(pf_search(db, asc, anm, 3, "AppleLanguages", &which) && which == 1);
    CHECK(pf_search(db, asc, anm, 3, "sistema", &which) && which == 2);

    CHECK(pf_save(db, "user", "com.visualstudio.code", user, &fs) == 0);
    CHECK(pf_save(db, "user", ".GlobalPreferences", glob, &fs) == 0);
    CHECK(pf_save(db, "any", "com.visualstudio.code", any, &fs) == 0);
    CHECK(!pf_is_dirty(db, "user", "com.visualstudio.code"));
    CHECK(access(user, 0) == 0);
    char tmp_left[1200];
    snprintf(tmp_left, sizeof tmp_left, "%s.tmp", user);
    CHECK(access(tmp_left, 0) != 0); /* rename consumiu o temporário */

    /* falha de escrita mantém a memória e o dirty */
    CHECK(pf_put(db, "user", "com.visualstudio.code", "novo", pf_str("x")) == 1);
    g_fail_write = 1;
    CHECK(pf_save(db, "user", "com.visualstudio.code", user, &fs) == -1);
    CHECK(pf_is_dirty(db, "user", "com.visualstudio.code"));
    CHECK(pf_get_mem(db, "user", "com.visualstudio.code", "novo") != 0);
    g_fail_write = 0;
    CHECK(pf_save(db, "user", "com.visualstudio.code", user, &fs) == 0);

    /* outro banco lê o disco */
    pf_db *db2 = pf_db_new();
    CHECK(pf_load(db2, "user", "com.visualstudio.code", user, &fs, 1) == 0);
    CHECK(pf_get_mem(db2, "user", "com.visualstudio.code", "window") &&
          pf_get_mem(db2, "user", "com.visualstudio.code", "window")->i == 1);
    CHECK(pf_get_mem(db2, "user", "com.visualstudio.code", "novo") &&
          strcmp(pf_get_mem(db2, "user", "com.visualstudio.code", "novo")->str, "x") == 0);
    /* segunda carga com o mesmo stamp não troca o ponteiro da árvore (gen estável) */
    unsigned g0 = pf_gen(db2, "user", "com.visualstudio.code");
    CHECK(pf_load(db2, "user", "com.visualstudio.code", user, &fs, 0) == 0);
    CHECK(pf_gen(db2, "user", "com.visualstudio.code") == g0);

    /* alteração externa: dirty local não é pisado */
    CHECK(pf_put(db2, "user", "com.visualstudio.code", "local", pf_str("fica")) == 1);
    FILE *f = fopen(user, "ab");
    CHECK(f != 0);
    if (f) fclose(f); /* muda o tamanho/mtime, mas o XML pode ficar inválido — dirty protege */
    CHECK(pf_load(db2, "user", "com.visualstudio.code", user, &fs, 1) == 0);
    CHECK(pf_get_mem(db2, "user", "com.visualstudio.code", "local") != 0);

    /* bplist não é importado e não é sobrescrito */
    FILE *bf = fopen(binp, "wb");
    CHECK(bf != 0);
    if (bf) { fwrite("bplist00NAO-MEXER", 1, 16, bf); fclose(bf); }
    CHECK(pf_load(db, "user", "com.binary", binp, &fs, 1) == -2);
    CHECK(pf_is_blocked(db, "user", "com.binary"));
    CHECK(pf_get_mem(db, "user", "com.binary", "k") == 0);
    CHECK(pf_put(db, "user", "com.binary", "k", pf_str("tentou")) == 1);
    CHECK(pf_save(db, "user", "com.binary", binp, &fs) == -2);
    char got[32] = {0};
    FILE *br = fopen(binp, "rb");
    CHECK(br != 0);
    if (br) {
        size_t nr = fread(got, 1, 16, br);
        fclose(br);
        CHECK(nr == 16);
    }
    CHECK(memcmp(got, "bplist00NAO-MEXER", 16) == 0);

    char empty[1100];
    snprintf(empty, sizeof empty, "%s/empty.plist", dir);
    FILE *ef = fopen(empty, "wb");
    CHECK(ef != 0);
    if (ef) fclose(ef);
    CHECK(pf_load(db, "user", "com.empty", empty, &fs, 1) == 0);
    CHECK(!pf_is_blocked(db, "user", "com.empty"));

    pf_db_free(db);
    pf_db_free(db2);
}

/* bplist00 gerado pelo plistlib do Python (formato da Apple). */
static const unsigned char k_voz_bplist[] = {
    98,112,108,105,115,116,48,48,215,1,2,3,4,5,6,7,
    8,11,12,13,14,15,16,85,108,105,115,116,97,81,110,82,
    110,111,82,111,107,86,113,117,97,110,100,111,81,114,83,118,
    111,122,162,9,10,81,97,81,98,16,7,8,9,51,65,197,
    203,76,0,0,0,0,35,63,248,0,0,0,0,0,0,87,
    111,112,101,110,98,111,116,8,23,29,31,34,37,44,46,50,
    53,55,57,59,60,61,70,79,0,0,0,0,0,0,1,1,
    0,0,0,0,0,0,0,17,0,0,0,0,0,0,0,0,
    0,0,0,0,0,0,0,87
};

static void test_cross_byhost_bplist(const char *dir) {
    char path[1100], puser[256], phost[256], pany[256], panyh[256];
    CHECK(pf_pref_path("/Users/Visitante", "mos", 0, "com.mos.app", puser, sizeof puser) == 0);
    CHECK(strcmp(puser, "/Users/Visitante/Library/Preferences/com.mos.app.plist") == 0);
    CHECK(pf_pref_path("/Users/Visitante", "mos", "host", "com.mos.app", phost, sizeof phost) == 0);
    CHECK(strcmp(phost, "/Users/Visitante/Library/Preferences/ByHost/com.mos.app.mos.plist") == 0);
    CHECK(pf_pref_path("/Users/Visitante", "mos", "any", "com.mos.app", pany, sizeof pany) == 0);
    CHECK(strcmp(pany, "/Library/Preferences/com.mos.app.plist") == 0);
    CHECK(pf_pref_path("/Users/Visitante", "mos", "any.host", "com.mos.app", panyh, sizeof panyh) == 0);
    CHECK(strcmp(panyh, "/Library/Preferences/ByHost/com.mos.app.mos.plist") == 0);
    CHECK(strcmp(puser, phost) != 0);

    snprintf(path, sizeof path, "%s/cross.plist", dir);
    pf_db *parent = pf_db_new();
    CHECK(pf_put(parent, "user", "com.cross", "k", pf_str("aaaa")) == 1);
    CHECK(pf_save(parent, "user", "com.cross", path, &fs) == 0);
    struct timespec ts[2] = {{1000, 0}, {1000, 0}};
    CHECK(utimensat(AT_FDCWD, path, ts, 0) == 0);

    pid_t pid = fork();
    CHECK(pid >= 0);
    if (pid == 0) {
        pf_db *child = pf_db_new();
        int bad = 0;
        if (pf_load(child, "user", "com.cross", path, &fs, 1) != 0) bad = 1;
        pf_val *seen = pf_get_mem(child, "user", "com.cross", "k");
        if (!seen || seen->type != PF_STR || !seen->str || strcmp(seen->str, "aaaa") != 0) bad = 1;
        if (pf_put(child, "user", "com.cross", "k", pf_str("bbbb")) != 1) bad = 1;
        if (pf_save(child, "user", "com.cross", path, &fs) != 0) bad = 1;
        if (utimensat(AT_FDCWD, path, ts, 0) != 0) bad = 1;
        _exit(bad ? 2 : 0);
    }
    if (pid > 0) {
        int st = 0;
        CHECK(waitpid(pid, &st, 0) == pid);
        CHECK(WIFEXITED(st) && WEXITSTATUS(st) == 0);
    }
    struct stat stt;
    CHECK(stat(path, &stt) == 0 && stt.st_mtime == 1000);
    CHECK(pf_load(parent, "user", "com.cross", path, &fs, 0) == 0);
    pf_val *got = pf_get_mem(parent, "user", "com.cross", "k");
    CHECK(got && got->str && strcmp(got->str, "bbbb") == 0);

    CHECK(pf_put(parent, "host", "com.cross", "k", pf_str("so-neste-host")) == 1);
    CHECK(pf_get_mem(parent, "user", "com.cross", "k") && strcmp(pf_get_mem(parent, "user", "com.cross", "k")->str, "bbbb") == 0);
    CHECK(pf_get_mem(parent, "host", "com.cross", "k") && strcmp(pf_get_mem(parent, "host", "com.cross", "k")->str, "so-neste-host") == 0);

    char bp[1100];
    snprintf(bp, sizeof bp, "%s/voz.bplist", dir);
    FILE *bf = fopen(bp, "wb");
    CHECK(bf != 0);
    if (bf) {
        CHECK(fwrite(k_voz_bplist, 1, sizeof k_voz_bplist, bf) == sizeof k_voz_bplist);
        fclose(bf);
    }
    struct stat before;
    CHECK(stat(bp, &before) == 0);
    CHECK(pf_load(parent, "user", "com.voz", bp, &fs, 1) == 0);
    CHECK(!pf_is_blocked(parent, "user", "com.voz"));
    pf_val *voz = pf_get_mem(parent, "user", "com.voz", "voz");
    CHECK(voz && voz->type == PF_STR && voz->str && strcmp(voz->str, "openbot") == 0);
    pf_val *num = pf_get_mem(parent, "user", "com.voz", "n");
    CHECK(num && num->type == PF_INT && num->i == 7);
    pf_val *okv = pf_get_mem(parent, "user", "com.voz", "ok");
    CHECK(okv && okv->type == PF_BOOL && okv->b == 1);
    pf_val *nov = pf_get_mem(parent, "user", "com.voz", "no");
    CHECK(nov && nov->type == PF_BOOL && nov->b == 0);
    pf_val *real = pf_get_mem(parent, "user", "com.voz", "r");
    CHECK(real && real->type == PF_REAL && real->d > 1.49 && real->d < 1.51);
    pf_val *lista = pf_get_mem(parent, "user", "com.voz", "lista");
    CHECK(lista && lista->type == PF_ARRAY && lista->ent && lista->ent->v && lista->ent->v->type == PF_STR &&
          lista->ent->v->str && strcmp(lista->ent->v->str, "a") == 0 && lista->ent->next && lista->ent->next->v &&
          lista->ent->next->v->str && strcmp(lista->ent->next->v->str, "b") == 0);
    pf_val *quando = pf_get_mem(parent, "user", "com.voz", "quando");
    CHECK(quando && quando->type == PF_DATE && quando->d > 1709596799.0 && quando->d < 1709596801.0);
    CHECK(pf_save(parent, "user", "com.voz", bp, &fs) == 0);
    struct stat after;
    CHECK(stat(bp, &after) == 0 && before.st_size == after.st_size);
    unsigned char magic[8] = {0};
    FILE *br = fopen(bp, "rb");
    CHECK(br != 0);
    if (br) {
        CHECK(fread(magic, 1, 8, br) == 8);
        fclose(br);
    }
    CHECK(memcmp(magic, "bplist00", 8) == 0);
    pf_db_free(parent);
}

int main(void) {
    char dir[] = "/tmp/mos-prefs-test-XXXXXX";
    if (!mkdtemp(dir)) { perror("mkdtemp"); return 1; }
    test_roundtrip();
    test_search_and_file(dir);
    test_cross_byhost_bplist(dir);
    char cmd[1200];
    snprintf(cmd, sizeof cmd, "rm -rf '%s'", dir);
    if (system(cmd) != 0) fprintf(stderr, "aviso: não apaguei %s\n", dir);
    if (fails) {
        fprintf(stderr, "prefstest: %d falhas em %d checagens\n", fails, checks);
        return 1;
    }
    printf("prefstest: %d/%d PASS\n", checks, checks);
    return 0;
}
