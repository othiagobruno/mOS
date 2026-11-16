/* Teste de host do núcleo do NSDocument. Roda no Linux, sem VM:
 *   gcc -std=c11 -Wall -Wextra -DCX_DC_HOST -Iuserland/libsystem \
 *       userland/libsystem/appkit_doc_core.c userland/tests-appkit/document_host.c \
 *       -o /tmp/appkit-document-test && /tmp/appkit-document-test */
#include "appkit_doc_core.h"
#include <stdio.h>
#include <string.h>

static int ok, total;
static void check(int c, const char *w) { total++; if (c) ok++; printf("%s %s\n", c ? "  ok  " : "  FALHA", w); }

int main(void) {
    puts("mudanças");
    dc_count_t c = {0, 0, 0, 0};
    check(!dc_edited(&c), "novo: sem mudanças");
    dc_update(&c, DC_DONE); dc_update(&c, DC_DONE);
    check(dc_edited(&c) && c.change == 2, "duas mudanças");
    dc_update(&c, DC_UNDONE);
    dc_update(&c, DC_UNDONE);
    check(!dc_edited(&c), "desfazer tudo volta ao salvo");
    dc_update(&c, DC_UNDONE);
    check(dc_edited(&c), "desfazer além do salvo também é mudança");
    dc_update(&c, DC_REDONE);
    check(!dc_edited(&c), "refazer volta");
    dc_update(&c, DC_DONE); dc_update(&c, DC_AUTOSAVED);
    check(dc_edited(&c) && !dc_has_unautosaved(&c), "autosalvo: editado mas sem pendência de autosave");
    dc_update(&c, DC_DONE);
    check(dc_has_unautosaved(&c), "nova mudança depois do autosave");
    dc_update(&c, DC_CLEARED);
    check(!dc_edited(&c) && !dc_has_unautosaved(&c), "salvar limpa");
    dc_update(&c, DC_READ_OTHER);
    check(dc_edited(&c), "conteúdo de outro arquivo conta como editado");
    dc_update(&c, DC_CLEARED);
    dc_update(&c, DC_DONE | DC_DISCARDABLE);
    check(c.discardable && dc_edited(&c), "mudança descartável");
    puts("sem título");
    char b[64];
    dc_untitled_name("Sem título", 0, 0, b, sizeof b); check(!strcmp(b, "Sem título"), "primeiro");
    const char *u1[] = {"Sem título"};
    dc_untitled_name("Sem título", u1, 1, b, sizeof b); check(!strcmp(b, "Sem título 2"), "segundo");
    const char *u2[] = {"Sem título 2", "Sem título 3"};
    dc_untitled_name("Sem título", u2, 2, b, sizeof b); check(!strcmp(b, "Sem título"), "reaproveita o primeiro livre");
    const char *u3[] = {"Sem título", "Sem título 2", "Sem título 3"};
    dc_untitled_name("Sem título", u3, 3, b, sizeof b); check(!strcmp(b, "Sem título 4"), "quarto");
    puts("recentes");
    dc_mru_t m;
    dc_mru_init(&m, 3);
    dc_mru_add(&m, "/a"); dc_mru_add(&m, "/b"); dc_mru_add(&m, "/c");
    check(m.n == 3 && !strcmp(m.item[0], "/c") && !strcmp(m.item[2], "/a"), "o mais novo primeiro");
    dc_mru_add(&m, "/a");
    check(m.n == 3 && !strcmp(m.item[0], "/a") && !strcmp(m.item[1], "/c"), "repetido sobe, sem duplicar");
    dc_mru_add(&m, "/d");
    check(m.n == 3 && !strcmp(m.item[2], "/c"), "passa do máximo: cai o mais velho");
    dc_mru_set_max(&m, 1);
    check(m.n == 1 && !strcmp(m.item[0], "/d"), "diminuir o máximo corta");
    dc_mru_set_max(&m, 0); dc_mru_add(&m, "/x");
    check(m.n == 0, "máximo 0 desliga");
    dc_mru_clear(&m);
    puts("tipos");
    dc_type_t t[3];
    dc_type_init(&t[0], "Imagem", "Viewer", "ImgDoc");
    dc_type_add_ext(&t[0], "png"); dc_type_add_ext(&t[0], ".JPG"); dc_type_add_uti(&t[0], "public.image");
    dc_type_init(&t[1], "Texto", "Editor", "TxtDoc");
    dc_type_add_ext(&t[1], "txt"); dc_type_add_ext(&t[1], "png"); dc_type_add_uti(&t[1], "public.plain-text");
    dc_type_init(&t[2], "Qualquer", "Viewer", "");
    dc_type_add_ext(&t[2], "*");
    check(dc_type_for_ext(t, 3, "jpg") == 0, "extensão sem caixa e sem ponto");
    check(dc_type_for_ext(t, 3, "PNG") == 1, "duas aceitam: vence o Editor");
    check(dc_type_for_ext(t, 3, "zip") == 2, "* aceita o resto");
    check(dc_type_for_ext(t, 2, "zip") == -1, "sem *: nenhum");
    check(dc_type_named(t, 3, "Texto") == 1 && dc_type_named(t, 3, "public.image") == 0 && dc_type_named(t, 3, "x") == -1, "por nome e por UTI");
    check(dc_default_type(t, 3) == 1, "padrão: 1º Editor com classe");
    check(dc_type_writable(&t[1]) && !dc_type_writable(&t[0]), "só Editor grava");
    puts("títulos");
    dc_window_title("Carta.txt", 0, b, sizeof b); check(!strcmp(b, "Carta.txt"), "limpo");
    dc_window_title("Carta.txt", 1, b, sizeof b); check(!strcmp(b, "Carta.txt \xE2\x80\x94 Editado"), "editado");
    dc_display_name("/Users/ana/Carta.final.txt", 1, b, sizeof b); check(!strcmp(b, "Carta.final"), "esconde a extensão");
    dc_display_name("/Users/ana/Carta.txt", 0, b, sizeof b); check(!strcmp(b, "Carta.txt"), "mostra a extensão");
    dc_display_name("/Users/ana/Pacote.rtfd/", 0, b, sizeof b); check(!strcmp(b, "Pacote.rtfd"), "pasta com barra no fim");
    dc_display_name("/Users/ana/.bashrc", 1, b, sizeof b); check(!strcmp(b, ".bashrc"), "arquivo de ponto");
    printf("appkit-document: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
