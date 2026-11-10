/* gcc -std=c11 -Wall -Wextra -DCX_CN_HOST -Iuserland/libsystem \
      userland/libsystem/cn_store.c userland/tests-appkit/cn_host.c -o /tmp/cn-host
*/
#include "cn_store.h"
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int ok, total;

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
    printf("cn-host: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
