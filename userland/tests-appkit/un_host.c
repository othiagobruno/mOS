/* Teste de host do centro de notificações.
 *   gcc -std=c11 -Wall -Wextra -DCX_UN_HOST -Iuserland/libsystem \
 *       userland/libsystem/un_center.c userland/tests-appkit/un_host.c \
 *       -o /tmp/un-host && /tmp/un-host
 */
#include "un_center.h"
#include <stdio.h>
#include <string.h>

static int ok, total;
static int present_opts = CX_UN_PRESENT_BANNER | CX_UN_PRESENT_LIST;
static int got_click;
static const char *g_fail_dom;

static void remote_fail(int code, const char *domain, void *ctx) {
    int *out = ctx;
    if (out) *out = code;
    g_fail_dom = domain;
}
static char click_id[64];
static char click_action[80];
static char sink_title[96];
static int sink_token;

static void check(int cond, const char *what) {
    total++;
    if (cond) ok++;
    printf("%s %s\n", cond ? "  ok  " : "  FALHA", what);
}

static int present(const char *ident, const char *title, const char *body, void *ctx) {
    (void)ident; (void)title; (void)body; (void)ctx;
    return present_opts;
}

static void receive(const char *ident, const char *action, void *ctx) {
    (void)ctx;
    got_click++;
    snprintf(click_id, sizeof click_id, "%s", ident ? ident : "");
    snprintf(click_action, sizeof click_action, "%s", action ? action : "");
}

static void sink(const char *title, const char *body, int token, void *ctx) {
    (void)body; (void)ctx;
    snprintf(sink_title, sizeof sink_title, "%s", title ? title : "");
    sink_token = token;
}

int main(void) {
    cx_un_reset();
    cx_un_set_delegate(present, receive, 0);
    cx_un_set_banner_sink(sink, 0);
    cx_un_set_clock(0);

    check(cx_un_add("a", "Oi", 0, "corpo", 0, 0) == CX_UN_ERR_DENIED, "sem autorização o pedido é recusado");
    check(cx_un_banner_visible() == 0 && cx_un_present_count() == 0, "recusa não chama o delegate nem abre banner");

    cx_un_set_auth(CX_UN_DENIED);
    check(cx_un_add("a", "Oi", 0, "corpo", 0, 0) == CX_UN_ERR_DENIED, "autorização negada recusa o pedido");

    cx_un_set_auth(CX_UN_AUTHORIZED);
    check(cx_un_add(0, "Oi", 0, "corpo", 0, 0) == CX_UN_ERR_BAD, "pedido sem identificador é inválido");

    present_opts = CX_UN_PRESENT_BANNER | CX_UN_PRESENT_LIST;
    check(cx_un_add("agora", "Chegou", "sub", "", 0, 0) == CX_UN_OK, "gatilho imediato é aceito");
    check(cx_un_banner_visible() == 1, "notificação autorizada abre banner");
    check(strcmp(cx_un_banner_title(), "Chegou") == 0, "o banner mostra o título");
    check(strcmp(cx_un_banner_body(), "sub") == 0, "sem corpo, o banner usa o subtítulo");
    check(cx_un_present_count() == 1, "willPresent foi chamado");
    check(cx_un_delivered_has("agora") == 1 && cx_un_pending_count() == 0, "entrega imediata sai da fila pendente");
    check(strcmp(sink_title, "Chegou") == 0 && sink_token == cx_un_banner_token(), "o banner foi publicado com o token");

    check(cx_un_click_token(sink_token) == 1, "o clique no banner chama o delegate");
    check(got_click == 1 && strcmp(click_id, "agora") == 0, "o clique entrega o identificador");
    check(strcmp(click_action, CX_UN_ACTION_DEFAULT) == 0, "a ação do clique é a padrão");
    check(cx_un_banner_visible() == 0, "o clique recolhe o banner");
    check(cx_un_click_token(sink_token) == 0, "segundo clique no mesmo token não repete");

    cx_un_stage_info("conversa=42");
    check(cx_un_add("com-info", "Aviso", 0, "texto", 0, 0) == CX_UN_OK, "pedido com userInfo é aceito");
    check(strcmp(cx_un_info("com-info"), "conversa=42") == 0, "o userInfo permanece depois da entrega");
    cx_un_stage_info("nao-deve-vazar");
    check(cx_un_add("sem-usar", "X", 0, "y", 5, 0) == CX_UN_OK, "o userInfo staged vai para o pedido seguinte");
    check(strcmp(cx_un_info("sem-usar"), "nao-deve-vazar") == 0, "pedido pendente guarda o userInfo");
    check(cx_un_info("ausente")[0] == 0, "identificador desconhecido não tem userInfo");
    cx_un_remove_delivered("com-info");
    cx_un_remove_pending("sem-usar");
    check(cx_un_banner_visible() == 0, "remover a entregue recolhe o banner do userInfo");

    present_opts = 0;
    check(cx_un_add("mudo", "Silêncio", 0, "x", 0, 0) == CX_UN_OK, "willPresent pode recusar o banner");
    check(cx_un_banner_visible() == 0 && cx_un_delivered_has("mudo") == 1, "sem opção de banner não há cartão");
    check(cx_un_banner_click() == 0, "sem banner o clique não chama o delegate");

    present_opts = CX_UN_PRESENT_ALERT;
    check(cx_un_add("alerta", "Alerta", 0, "texto", 0, 0) == CX_UN_OK, "a opção antiga de alerta também abre banner");
    check(cx_un_banner_visible() == 1 && strcmp(cx_un_banner_body(), "texto") == 0, "o corpo do alerta aparece no banner");
    cx_un_remove_delivered("alerta");
    check(cx_un_banner_visible() == 0 && cx_un_delivered_has("alerta") == 0, "removeDelivered tira o banner");

    cx_un_set_clock(10);
    present_opts = CX_UN_PRESENT_BANNER;
    check(cx_un_add("depois", "Mais tarde", 0, "intervalo", 5, 0) == CX_UN_OK, "gatilho de intervalo fica pendente");
    check(cx_un_pending_has("depois") == 1 && cx_un_banner_visible() == 0, "antes do prazo não há banner");
    cx_un_advance(4);
    check(cx_un_banner_visible() == 0 && cx_un_pending_has("depois") == 1, "4 s de 5 ainda não disparam");
    cx_un_advance(1);
    check(cx_un_banner_visible() == 1 && strcmp(cx_un_banner_title(), "Mais tarde") == 0, "no prazo o banner aparece");
    check(cx_un_pending_has("depois") == 0 && cx_un_delivered_has("depois") == 1, "intervalo sem repetir sai da fila");
    got_click = 0;
    check(cx_un_banner_click() == 1 && got_click == 1 && strcmp(click_id, "depois") == 0, "clique no banner de intervalo chama o delegate");

    cx_un_set_clock(100);
    check(cx_un_add("repete", "De novo", 0, "b", 10, 1) == CX_UN_OK, "intervalo com repetição é aceito");
    cx_un_advance(10);
    check(cx_un_present_count() >= 2 && cx_un_pending_has("repete") == 1, "a repetição continua pendente e apresenta de novo");
    cx_un_remove_pending("repete");
    {
        int before = cx_un_present_count();
        cx_un_advance(10);
        check(cx_un_pending_has("repete") == 0 && cx_un_present_count() == before, "removePending cancela a repetição");
    }

    cx_un_set_clock(0);
    cx_un_add("um", "Um", 0, "a", 3, 0);
    cx_un_add("dois", "Dois", 0, "b", 3, 0);
    check(cx_un_remove_pending("um") == 1 && cx_un_pending_has("um") == 0 && cx_un_pending_has("dois") == 1, "removePending tira só o identificador pedido");
    cx_un_remove_pending(0);
    check(cx_un_pending_count() == 0, "removeAllPending esvazia a fila");

    cx_un_banner_click();
    present_opts = CX_UN_PRESENT_LIST;
    check(cx_un_add("lista", "Lista", 0, "só lista", 0, 0) == CX_UN_OK, "só a lista não abre banner");
    check(cx_un_banner_visible() == 0 && cx_un_delivered_has("lista") == 1, "a entrega na lista não é um banner");

    cx_un_set_auth(CX_UN_AUTHORIZED);
    {
        cx_un_settings_t st;
        void *token = (void *)1;
        int fail_code = 0;
        char kept[32];
        cx_un_settings(&st);
        check(st.authorization == CX_UN_AUTHORIZED && st.alert == 2 && st.sound == 2 && st.badge == 2
              && st.notification_center == 2 && st.lock_screen == 2 && st.alert_style == 1,
              "autorizado liga alerta, som, badge, central e tela bloqueada");
        cx_un_set_auth(CX_UN_DENIED);
        cx_un_settings(&st);
        check(st.authorization == CX_UN_DENIED && st.alert == 1 && st.alert_style == 0,
              "negado desliga o alerta e não escolhe estilo");
        check(cx_un_register_remote(&token, remote_fail, &fail_code) == CX_UN_ERR_DENIED && token == 0
              && fail_code == 1 && g_fail_dom && strcmp(g_fail_dom, "UNErrorDomain") == 0,
              "registro remoto falha sem token");
        strcpy(kept, "original");
        check(cx_un_service_receive("original", kept, (int)sizeof kept) == CX_UN_ERR_DENIED
              && strcmp(kept, "original") == 0,
              "a extensão não inventa payload remoto");
    }

    printf("un-host: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
