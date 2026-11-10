/* mOS — testes de ls_open e de um ls_rescan que vê plist do mesmo tamanho.
 * Só entra no kernel com -DMOS_UNIT_TESTS. Não abre janela: g_screen ainda é nulo neste ponto do boot,
 * e um ls_open que acha app chama o GUI ou o alerta. Os caminhos abaixo voltam antes disso.
 * O carimbo do mesmo tamanho também é exercitado em unit_tests.c (test_launch_runtime). */
#ifdef MOS_UNIT_TESTS
#include "kernel.h"
#include "vfs.h"
#include "launchsvc.h"

void ls_open_tests_run(int *ok, int *bad) {
#define CHECK(c, what) do { if (c) (*ok)++; else { (*bad)++; kprintf("unit: FALHOU %s (linha %d)\n", what, __LINE__); } } while (0)
    ls_app_t *d;
    vnode_t *n;
    char *p;
    int before;
    static const char plist[] =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<plist version=\"1.0\"><dict>\n"
        "<key>CFBundleIdentifier</key><string>com.mos.lsopen1</string>\n"
        "<key>CFBundleExecutable</key><string>LsOpen</string>\n"
        "<key>CFBundleURLTypes</key><array><dict>\n"
        "<key>CFBundleURLSchemes</key><array><string>moslsopen1</string></array>\n"
        "</dict></array></dict></plist>\n";

    ls_init();
    CHECK(!ls_open(NULL, NULL), "ls_open sem alvo");
    CHECK(!ls_open("", NULL), "ls_open com alvo vazio");
    CHECK(!ls_open("/tmp/ls-open-ausente.xyzzy", NULL), "ls_open de caminho que não existe");
    CHECK(!ls_open("file:///tmp/ls-open-ausente.xyzzy", NULL), "ls_open de file:// inexistente");
    CHECK(!ls_open("moslsopenausente://item", NULL), "ls_open de esquema sem app");

    vfs_write_file("/tmp/ls-open-sem-dono.xyzzy", "abc", 3);
    CHECK(vfs_lookup("/tmp/ls-open-sem-dono.xyzzy") != NULL, "arquivo de extensão desconhecida está no VFS");
    CHECK(ls_default_for_path("/tmp/ls-open-sem-dono.xyzzy", LS_ROLE_ALL) == NULL, "extensão desconhecida não tem app padrão");
    CHECK(!ls_open("/tmp/ls-open-sem-dono.xyzzy", NULL), "ls_open de arquivo sem app não abre");
    CHECK(!ls_open("file:///tmp/ls-open-sem-dono.xyzzy", NULL), "ls_open de file:// sem app não abre");

    vfs_write_file("/Aplicativos/LsOpen.app/Contents/Info.plist", plist, strlen(plist));
    vfs_write_file("/Aplicativos/LsOpen.app/Contents/MacOS/LsOpen", "bin", 3);
    ls_rescan();
    CHECK(ls_app_by_id("com.mos.lsopen1") != NULL, "ls_rescan registra o bundle novo");
    d = ls_default_for_scheme("moslsopen1");
    CHECK(d && !strcmp(ls_app_id(d), "com.mos.lsopen1"), "ls_rescan publica o esquema do plist");

    n = vfs_lookup("/Aplicativos/LsOpen.app/Contents/Info.plist");
    p = n && n->data ? strstr((char *)n->data, "moslsopen1") : NULL;
    CHECK(p != NULL, "plist do LsOpen está no VFS");
    if (p) p[strlen("moslsopen1") - 1] = '2';   /* mesmo tamanho, mesmo mtime: moslsopen1 -> moslsopen2 */
    before = ls_test_reparses();
    ls_test_poke();
    ls_tick();
    CHECK(ls_test_reparses() > before, "plist do mesmo tamanho com outro conteúdo dispara a releitura");
    d = ls_default_for_scheme("moslsopen2");
    CHECK(d && !strcmp(ls_app_id(d), "com.mos.lsopen1"), "o esquema novo passa a ser o do app");
    CHECK(ls_default_for_scheme("moslsopen1") == NULL, "o esquema antigo sai do banco");

    vfs_delete(vfs_lookup("/Aplicativos/LsOpen.app"));
    vfs_delete(vfs_lookup("/tmp/ls-open-sem-dono.xyzzy"));
    ls_rescan();
    CHECK(ls_app_by_id("com.mos.lsopen1") == NULL, "o app de teste sai do banco");
#undef CHECK
}
#endif
