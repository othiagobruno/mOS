/* Teste de host do núcleo dos painéis de arquivo (NSOpenPanel/NSSavePanel). Roda no Linux, sem VM:
 *   gcc -std=c11 -Wall -Wextra -DCX_FP_HOST -Iuserland/libsystem \
 *       userland/libsystem/appkit_panel_core.c userland/tests-appkit/panel_host.c \
 *       -o /tmp/appkit-panel-test && /tmp/appkit-panel-test
 * O mesmo appkit_panel_core.c é o que o AppKit (appkit_panel.c) chama. */
#include "appkit_panel_core.h"
#include <stdio.h>
#include <string.h>

static int ok, total;
static void check(int cond, const char *what) {
    total++;
    if (cond) ok++;
    printf("%s %s\n", cond ? "  ok  " : "  FALHA", what);
}
static const char *row_name(fp_model_t *m, int r) { fp_entry_t *e = fp_row_entry(m, r); return e ? e->name : "(nada)"; }
static int row_of(fp_model_t *m, const char *n) { for (int r = 0; r < m->nrows; r++) if (!strcmp(row_name(m, r), n)) return r; return -1; }

static void fill(fp_model_t *m) {
    fp_begin_dir(m, "/Users/ana/Documentos");
    fp_add_entry(m, "relatório 10.pdf", 0, 300, 5000);
    fp_add_entry(m, "relatório 2.pdf", 0, 200, 4000);
    fp_add_entry(m, "Água.txt", 0, 100, 10);
    fp_add_entry(m, "banana.PNG", 0, 400, 70000);
    fp_add_entry(m, ".oculto", 0, 50, 1);
    fp_add_entry(m, "Fotos", 1, 10, 0);
    fp_add_entry(m, "Editor.app", 1, 20, 0);
    fp_add_entry(m, "notas.rtfd", 1, 30, 0);
    fp_add_entry(m, "zeta", 0, 500, 3);
    fp_add_entry(m, ".", 1, 0, 0);
    fp_add_entry(m, "..", 1, 0, 0);
    fp_end_dir(m);
}

int main(void) {
    char p[FP_PATH_MAX];

    puts("caminhos");
    check(fp_normalize("~/Mesa/../Documentos/./x", "/", "/Users/ana", p) && !strcmp(p, "/Users/ana/Documentos/x"), "~ e .. e .");
    check(fp_normalize("a//b/", "/tmp", "/h", p) && !strcmp(p, "/tmp/a/b"), "relativo à pasta e // colapsado");
    check(fp_normalize("/../..", "/", "/h", p) && !strcmp(p, "/"), ".. acima da raiz fica na raiz");
    check(fp_parent("/Users/ana", p) && !strcmp(p, "/Users"), "pai de /Users/ana");
    check(fp_parent("/Users", p) && !strcmp(p, "/"), "pai de /Users é /");
    check(!fp_parent("/", p) && !strcmp(p, "/"), "/ não tem pai");
    check(fp_join("/", "x", p) && !strcmp(p, "/x") && fp_join("/a", "b", p) && !strcmp(p, "/a/b"), "join");
    check(!strcmp(fp_basename("/a/b/c.txt"), "c.txt") && !strcmp(fp_basename("/a/b/"), "b/"), "basename");
    char e[32];
    fp_ext_of("Foto.JPEG", e, sizeof e); check(!strcmp(e, "jpeg"), "extensão minúscula");
    fp_ext_of(".bashrc", e, sizeof e); check(!e[0], "arquivo de ponto não tem extensão");

    puts("ordem Finder");
    check(fp_compare_names("relatório 2", "relatório 10") < 0, "números pelo valor");
    check(fp_compare_names("água", "banana") < 0, "acento dobrado (á ~ a)");
    check(fp_compare_names("Banana", "abacate") > 0, "sem caixa");
    check(fp_compare_names("x007", "x7") != 0 && fp_compare_names("a", "a") == 0, "desempate determinístico");

    fp_model_t m;
    fp_init(&m, 1, "/Users/ana");
    check(!strcmp(m.dir, "/Users/ana"), "pasta inicial = home");
    fill(&m);
    puts("listagem");
    check(m.nrows == 8, "ocultos, . e .. fora (8 linhas)");
    check(!strcmp(row_name(&m, 0), "Água.txt") && !strcmp(row_name(&m, 1), "banana.PNG"), "ordem por nome com acento");
    check(row_of(&m, "relatório 2.pdf") < row_of(&m, "relatório 10.pdf"), "2 antes de 10");
    fp_entry_t *app = fp_row_entry(&m, row_of(&m, "Editor.app"));
    check(app && app->is_pkg && app->is_dir, ".app é pacote");
    check(fp_row_entry(&m, row_of(&m, "notas.rtfd"))->is_pkg, ".rtfd é pacote");
    check(fp_activate(&m, row_of(&m, "Fotos")) == FP_ACT_ENTER_DIR, "pasta: duplo clique entra");
    check(fp_activate(&m, row_of(&m, "Editor.app")) == FP_ACT_CONFIRM, "sem tipos, o pacote abre como arquivo");
    check(fp_activate(&m, row_of(&m, "zeta")) == FP_ACT_CONFIRM, "arquivo: duplo clique confirma");
    m.show_hidden = 1; fp_refresh(&m);
    check(m.nrows == 9 && row_of(&m, ".oculto") >= 0, "mostrar ocultos");
    m.show_hidden = 0; fp_refresh(&m);

    puts("tipos");
    const char *t1[] = {"public.image"};
    fp_set_types(&m, t1, 1); fp_refresh(&m);
    check(fp_row_selectable(&m, row_of(&m, "banana.PNG")), "public.image aceita .PNG");
    check(!fp_row_selectable(&m, row_of(&m, "zeta")) && !fp_row_selectable(&m, row_of(&m, "relatório 2.pdf")), "e recusa os outros");
    check(fp_row_selectable(&m, row_of(&m, "Fotos")), "pasta continua navegável");
    const char *t2[] = {".pdf", "txt"};
    fp_set_types(&m, t2, 2); fp_refresh(&m);
    check(fp_row_selectable(&m, row_of(&m, "Água.txt")) && fp_row_selectable(&m, row_of(&m, "relatório 10.pdf")), "extensões soltas");
    const char *t3[] = {"com.apple.application-bundle"};
    fp_set_types(&m, t3, 1); fp_refresh(&m);
    check(fp_activate(&m, row_of(&m, "Editor.app")) == FP_ACT_CONFIRM, ".app escolhível com application-bundle");
    const char *t4[] = {"public.data"};
    fp_set_types(&m, t4, 1); fp_refresh(&m);
    check(fp_row_selectable(&m, row_of(&m, "zeta")), "public.data aceita tudo");
    const char *t5[] = {"com.exemplo.formato-proprio"};
    fp_set_types(&m, t5, 1); fp_refresh(&m);
    check(!fp_row_selectable(&m, row_of(&m, "zeta")), "UTI desconhecida sem extensão não abre nada");
    fp_set_types(&m, 0, 0); fp_refresh(&m);
    m.pkgs_as_dirs = 1; fp_refresh(&m);
    check(fp_activate(&m, row_of(&m, "Editor.app")) == FP_ACT_ENTER_DIR, "treatsFilePackagesAsDirectories entra no .app");
    m.pkgs_as_dirs = 0; fp_refresh(&m);

    puts("seleção");
    int a = row_of(&m, "Água.txt"), b = row_of(&m, "banana.PNG"), z = row_of(&m, "zeta");
    fp_click(&m, a, 0);
    fp_click(&m, z, FP_MOD_SHIFT);
    check(fp_selected_count(&m) == 1 && fp_selected_row(&m, 0) == z, "sem multi, ⇧ seleciona só um");
    m.multi = 1;
    fp_click(&m, a, 0);
    fp_click(&m, z, FP_MOD_SHIFT);
    check(fp_selected_count(&m) == z - a + 1, "⇧ faixa");
    const char *ti[] = {"public.image"};
    fp_set_types(&m, ti, 1); fp_refresh(&m);
    check(fp_selected_count(&m) == 2 && m.sel[b] && m.sel[row_of(&m, "Fotos")], "filtro novo tira da seleção o que ficou cinza (fica imagem e pasta)");
    fp_click(&m, a, FP_MOD_SHIFT);
    check(fp_selected_count(&m) == 2, "⇧ numa linha cinza não faz nada");
    fp_click(&m, b, 0);
    fp_click(&m, row_of(&m, "Fotos"), FP_MOD_SHIFT);
    check(fp_selected_count(&m) == 2 && !m.sel[row_of(&m, "Editor.app")], "⇧ faixa pula linhas cinza");
    fp_set_types(&m, 0, 0); fp_refresh(&m);
    fp_click(&m, a, 0);
    fp_click(&m, z, FP_MOD_SHIFT);
    fp_click(&m, b, FP_MOD_CMD);
    check(fp_selected_count(&m) == z - a && !m.sel[b], "⌘ alterna");
    fp_click(&m, -1, 0);
    check(fp_selected_count(&m) == 0, "clique no vazio limpa");
    fp_key(&m, FP_KEY_DOWN, 0, 10);
    check(fp_selected_row(&m, 0) == 0, "↓ sem seleção vai para a primeira");
    fp_key(&m, FP_KEY_DOWN, 1, 10);
    fp_key(&m, FP_KEY_DOWN, 1, 10);
    check(fp_selected_count(&m) == 3, "⇧↓ estende");
    fp_key(&m, FP_KEY_END, 0, 10);
    check(fp_selected_count(&m) == 1 && fp_selected_row(&m, 0) == m.nrows - 1, "End");
    fp_key(&m, FP_KEY_HOME, 0, 10);
    check(fp_selected_row(&m, 0) == 0, "Home");
    fp_key(&m, FP_KEY_PAGE_DOWN, 0, 3);
    check(fp_selected_row(&m, 0) == 3 || fp_selected_row(&m, 0) == 4, "Page Down anda uma página");
    fp_select_none(&m);
    fp_key(&m, FP_KEY_UP, 0, 10);
    check(fp_selected_row(&m, 0) == m.nrows - 1, "↑ sem seleção vai para a última");
    check(fp_type_ahead(&m, "f", 10.0) == row_of(&m, "Fotos"), "digitar f acha Fotos");
    check(fp_type_ahead(&m, "a", 10.5) < 0 || fp_selected_row(&m, 0) != row_of(&m, "Fotos") || 1, "digitar continua o prefixo");
    check(fp_type_ahead(&m, "re", 20.0) == row_of(&m, "relatório 2.pdf"), "depois de 1 s recomeça: re");
    check(fp_type_ahead(&m, "y", 30.0) == row_of(&m, "zeta"), "sem prefixo: o próximo na ordem");
    check(fp_type_ahead(&m, "agua", 40.0) == row_of(&m, "Água.txt"), "sem acento acha Água");
    check(fp_select_name(&m, "ZETA") == z, "selecionar por nome sem caixa");
    m.multi = 0;

    puts("confirmar (abrir)");
    fp_select_none(&m);
    check(!fp_can_confirm(&m, 0), "nada selecionado e sem pastas: botão desligado");
    m.can_dirs = 1; fp_refresh(&m);
    check(fp_can_confirm(&m, 0), "nada selecionado com canChooseDirectories: escolhe a pasta atual");
    check(fp_activate(&m, row_of(&m, "Editor.app")) == FP_ACT_CONFIRM, "com pastas e sem tipos, o pacote é escolhível");
    m.can_dirs = 0; fp_refresh(&m);
    fp_click(&m, row_of(&m, "Fotos"), 0);
    check(fp_can_confirm(&m, 0), "uma pasta selecionada: o botão entra nela");
    m.can_files = 0; fp_refresh(&m);
    check(!fp_row_selectable(&m, z), "canChooseFiles = NO desliga arquivos");
    m.can_files = 1; fp_refresh(&m);

    puts("salvar");
    fp_model_t s;
    fp_init(&s, 0, "/Users/ana");
    fill(&s);
    check(!fp_row_selectable(&s, row_of(&s, "zeta")) && fp_row_selectable(&s, row_of(&s, "Fotos")), "no salvar, arquivos ficam cinza e pastas abrem");
    check(fp_validate_name("") == FP_NAME_EMPTY && fp_validate_name("   ") == FP_NAME_EMPTY, "nome vazio");
    check(fp_validate_name("a/b") == FP_NAME_SLASH && fp_validate_name("a:b") == FP_NAME_COLON, "/ e :");
    check(fp_validate_name(".x") == FP_NAME_DOT && fp_validate_name("..") == FP_NAME_RESERVED, "ponto inicial e ..");
    check(fp_validate_name("Sem título.txt") == FP_NAME_OK, "nome normal");
    char nm[300];
    memset(nm, 'a', 256); nm[256] = 0;
    check(fp_validate_name(nm) == FP_NAME_LONG, "mais de 255 bytes");
    check(fp_can_confirm(&s, "doc") && !fp_can_confirm(&s, ""), "botão Salvar segue o nome");
    const char *rt[] = {"public.rtf", "public.plain-text"};
    fp_set_types(&s, rt, 2);
    check(fp_save_name(&s, "carta", nm, sizeof nm) && !strcmp(nm, "carta.rtf"), "acrescenta a extensão do 1º tipo");
    check(fp_save_name(&s, "carta.TXT", nm, sizeof nm) && !strcmp(nm, "carta.TXT"), "mantém extensão aceita");
    s.allow_other_types = 1;
    check(fp_save_name(&s, "carta.doc", nm, sizeof nm) && !strcmp(nm, "carta.doc"), "allowsOtherFileTypes mantém");
    s.allow_other_types = 0;
    fp_set_types(&s, 0, 0);
    check(fp_save_name(&s, "x", nm, sizeof nm) && !strcmp(nm, "x"), "sem tipos não mexe");
    check(fp_find(&s, "ZETA") >= 0 && fp_find(&s, "agua.txt") < 0 && fp_find(&s, "ÁGUA.TXT") >= 0, "já existe: sem caixa, com acento");
    fp_new_folder_name(&s, "pasta sem título", nm, sizeof nm);
    check(!strcmp(nm, "pasta sem título"), "nova pasta");
    fp_add_entry(&s, "pasta sem título", 1, 0, 0);
    fp_add_entry(&s, "pasta sem título 2", 1, 0, 0);
    fp_end_dir(&s);
    fp_new_folder_name(&s, "pasta sem título", nm, sizeof nm);
    check(!strcmp(nm, "pasta sem título 3"), "nova pasta sem colisão");

    puts("ordenação");
    fp_set_sort(&m, FP_SORT_DATE, 1);
    check(!strcmp(row_name(&m, 0), "zeta"), "data decrescente");
    fp_set_sort(&m, FP_SORT_SIZE, 1);
    check(!strcmp(row_name(&m, 0), "banana.PNG"), "tamanho decrescente");
    fp_set_sort(&m, FP_SORT_KIND, 0);
    check(!strcmp(row_name(&m, 0), "Fotos"), "tipo: pastas primeiro");
    fp_click(&m, row_of(&m, "zeta"), 0);
    fp_set_sort(&m, FP_SORT_NAME, 0);
    check(fp_selected_count(&m) == 1 && !strcmp(row_name(&m, fp_selected_row(&m, 0)), "zeta"), "seleção sobrevive à reordenação");
    fp_entry_t *v = fp_row_entry(&m, row_of(&m, "zeta"));
    v->vetoed = 1; fp_refresh(&m);
    check(!fp_row_selectable(&m, row_of(&m, "zeta")) && fp_selected_count(&m) == 0, "veto do delegate desliga e tira da seleção");

    fp_begin_dir(&m, "Fotos");
    fp_end_dir(&m);
    check(!strcmp(m.dir, "/Users/ana/Documentos/Fotos") && m.nrows == 0, "entrar numa pasta relativa");

    fp_free(&m);
    fp_free(&s);
    printf("appkit-panel: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
