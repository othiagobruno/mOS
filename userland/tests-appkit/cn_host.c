/* gcc -std=c11 -Wall -Wextra -DCX_CN_HOST -Iuserland/libsystem \
      userland/libsystem/cn_store.c userland/tests-appkit/cn_host.c -o /tmp/cn-host
*/
#include "cn_store.h"
#include "cn_labels.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int ok, total;
static char g_pick_id[40];

static void on_pick(const char *id, void *ctx) {
    int *n = ctx;
    if (n) (*n)++;
    if (id) {
        int i = 0;
        for (; i < 39 && id[i]; i++) g_pick_id[i] = id[i];
        g_pick_id[i] = 0;
    }
}
static void on_cancel(void *ctx) {
    int *n = ctx;
    if (n) (*n)++;
}

static void check(int cond, const char *what) {
    total++;
    if (cond) ok++;
    printf("%s %s\n", cond ? "  ok  " : "  FALHA", what);
}

int main(void) {
    cx_cn_contact in, out;
    char id[40];
    char formatted[400];
    char path[] = "/tmp/mos-cn-host.moscn";
    int n, i, found;
    unlink(path);
    cx_cn_reset();
    cx_cn_set_path(path);

    memset(&in, 0, sizeof in);
    strcpy(in.given, "Ada");
    strcpy(in.family, "Lovelace");
    strcpy(in.phone, "119999");
    strcpy(in.street, "Rua 1");
    strcpy(in.city, "Sao Paulo");
    strcpy(in.state, "SP");
    strcpy(in.zip, "01000-000");
    strcpy(in.country, "Brasil");
    strcpy(in.iso, "BR");

    check(cx_cn_commit() == CX_CN_ERR_DENIED, "sem autorização o commit falha");
    check(cx_cn_count() < 0, "sem autorização a leitura falha");

    cx_cn_set_auth(CX_CN_AUTHORIZED);
    cx_cn_begin();
    check(cx_cn_stage_add(&in, id) == CX_CN_OK && id[0], "o add entra no pedido");
    check(cx_cn_commit() == CX_CN_OK, "commit grava o contato");
    n = cx_cn_count();
    check(n == 1, "a leitura vê um contato");
    check(cx_cn_get(0, &out) == CX_CN_OK && strcmp(out.given, "Ada") == 0 && strcmp(out.id, id) == 0, "o nome e o id voltaram do arquivo");

    strcpy(in.id, id);
    strcpy(in.family, "Byron");
    cx_cn_begin();
    check(cx_cn_stage_update(&in) == CX_CN_OK && cx_cn_commit() == CX_CN_OK, "update grava o sobrenome novo");
    check(cx_cn_get(0, &out) == CX_CN_OK && strcmp(out.family, "Byron") == 0 && strcmp(out.phone, "119999") == 0, "o update preserva o telefone");

    cx_cn_format_postal(&out, formatted, (int)sizeof formatted);
    check(strstr(formatted, "Rua 1") && strstr(formatted, "Sao Paulo - SP") && strstr(formatted, "01000-000"), "o formatador postal em BR junta cidade e UF");

    cx_cn_begin();
    check(cx_cn_stage_delete(id) == CX_CN_OK && cx_cn_commit() == CX_CN_OK, "delete apaga o contato");
    check(cx_cn_count() == 0, "depois de apagar a agenda fica vazia");

    cx_cn_reset();
    cx_cn_set_path(path);
    cx_cn_set_auth(CX_CN_AUTHORIZED);
    check(cx_cn_count() == 0, "reabrir o arquivo vazio continua vazio");

    found = 0;
    for (i = 0; i < n; i++) found++;
    (void)found;
    unlink(path);
    {
        cx_cn_contact extra;
        memset(&extra, 0, sizeof extra);
        strcpy(extra.given, "Lia");
        strcpy(extra.family, "Souza");
        strcpy(extra.prefix, "Dra.");
        strcpy(extra.phonetic_given, "LI");
        strcpy(extra.phonetic_family, "SO");
        strcpy(extra.url, "https://lia.example");
        strcpy(extra.relation, CN_LBL_SISTER);
        strcpy(extra.im_user, "lia");
        strcpy(extra.im_service, "Skype");
        strcpy(extra.social_user, "liasouza");
        strcpy(extra.social_service, "Twitter");
        strcpy(extra.image, "6869");
        cx_cn_begin();
        check(cx_cn_stage_add(&extra, id) == CX_CN_OK && cx_cn_commit() == CX_CN_OK, "grava prefixo, fonética, url, relação, IM e imagem");
        cx_cn_reset();
        cx_cn_set_path(path);
        cx_cn_set_auth(CX_CN_AUTHORIZED);
        check(cx_cn_count() == 1 && cx_cn_get(0, &out) == CX_CN_OK
              && strcmp(out.prefix, "Dra.") == 0 && strcmp(out.phonetic_family, "SO") == 0
              && strcmp(out.url, "https://lia.example") == 0 && strcmp(out.relation, CN_LBL_SISTER) == 0
              && strcmp(out.im_service, "Skype") == 0 && strcmp(out.social_service, "Twitter") == 0
              && strcmp(out.image, "6869") == 0,
              "a leitura devolve os campos das chaves");
        check(strcmp(CN_LBL_HOME, "_$!<Home>!$_") == 0 && strcmp(CN_LBL_IPHONE, "iPhone") == 0
              && strcmp(CN_LBL_ICLOUD, "iCloud") == 0 && strcmp(CN_LBL_MANAGER, "_$!<Manager>!$_") == 0,
              "os rótulos usam o texto da Apple");
    }

    {
        unsigned char raw[8];
        char shown[80];
        int picked = 0;
        int cancelled = 0;
        cx_cn_contact a, b;
        memset(&a, 0, sizeof a);
        memset(&b, 0, sizeof b);
        strcpy(a.given, "Lia"); strcpy(a.family, "Souza"); strcpy(a.prefix, "Dra.");
        strcpy(b.given, "Otto"); strcpy(b.family, "Lima");
        cx_cn_begin();
        check(cx_cn_stage_add(&a, id) == CX_CN_OK && cx_cn_stage_add(&b, 0) == CX_CN_OK && cx_cn_commit() == CX_CN_OK, "dois contatos para o picker");
        check(cx_cn_image_bytes("6869", raw, 8) == 2 && raw[0] == 0x68 && raw[1] == 0x69, "a foto hex vira os bytes");
        check(cx_cn_image_bytes("abc", raw, 8) < 0, "hex ímpar não vira foto");
        check(strcmp(cx_cn_container_id(), "local") == 0 && strcmp(cx_cn_container_name(), "Contatos") == 0, "há um contêiner local");
        cx_cn_set_name_order(1);
        check(cx_cn_get(0, &out) == CX_CN_OK && cx_cn_display_name(&out, shown, 80) > 0 && strcmp(shown, "Dra. Lia Souza") == 0, "a ordem padrão começa pelo nome");
        cx_cn_set_name_order(2);
        cx_cn_set_sort_order(3);
        check(cx_cn_name_order() == 2 && cx_cn_sort_order() == 3 && cx_cn_display_name(&out, shown, 80) && strcmp(shown, "Souza Lia") == 0, "a ordem de família troca a exibição");
        check(cx_cn_picker_select(1, on_pick, &picked) == CX_CN_OK && picked == 1 && g_pick_id[0], "o picker entrega o contato escolhido");
        cx_cn_picker_cancel(on_cancel, &cancelled);
        check(cancelled == 1, "cancelar avisa o delegate");
    }

    printf("cn-host: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
