/* Teste de host do centro de notificações (userland/libsystem/un_center.c).
 *   gcc -std=gnu11 -Wall -Wextra -fsanitize=address,undefined -DCX_UN_HOST -Iuserland/libsystem \
 *       userland/libsystem/un_center.c userland/tests-appkit/un_host.c -o /tmp/un-host && /tmp/un-host
 */
#include "un_center.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int ok, total;
static void check(int c, const char *what) { total++; if (c) ok++; else printf("FALHA: %s\n", what); }

static int n_banner, last_token, last_sound, n_removed, last_removed, last_badge = -1, n_receive, present_opts = CX_UN_PRESENT_ALL;
static char last_title[128], last_actions[256], recv_ident[64], recv_action[128], recv_text[128];
static int recv_text_null;
static void s_banner(const cx_un_req_t *r, const cx_un_category_t *cat, int sound, void *ctx) {
    (void)ctx;
    n_banner++;
    last_token = r->token;
    last_sound = sound;
    snprintf(last_title, sizeof last_title, "%s", r->title ? r->title : "");
    last_actions[0] = 0;
    for (int i = 0; cat && i < cat->nactions; i++) { strcat(last_actions, cat->actions[i].title); strcat(last_actions, ";"); }
}
static void s_remove(int token, void *ctx) { (void)ctx; n_removed++; last_removed = token; }
static void s_badge(int n, void *ctx) { (void)ctx; last_badge = n; }
static int d_present(const cx_un_req_t *r, void *ctx) { (void)r; (void)ctx; return present_opts; }
static void d_receive(const cx_un_req_t *r, const char *action, const char *text, void *ctx) {
    (void)ctx;
    n_receive++;
    snprintf(recv_ident, sizeof recv_ident, "%s", r->ident);
    snprintf(recv_action, sizeof recv_action, "%s", action);
    recv_text_null = text == 0;
    snprintf(recv_text, sizeof recv_text, "%s", text ? text : "");
}
static cx_un_sink_t SINK = {s_banner, s_remove, s_badge, 0};

static cx_un_req_t req(const char *ident, const char *title) {
    cx_un_req_t r;
    memset(&r, 0, sizeof r);
    r.ident = (char *)ident;
    r.title = (char *)title;
    r.badge = -1;
    r.cal = (cx_un_cal_t){-1, -1, -1, -1, -1, -1, -1};
    return r;
}
static void fresh(void) {
    cx_un_reset();
    cx_un_set_sink(&SINK);
    cx_un_set_delegate(d_present, d_receive, 0);
    cx_un_set_auth(CX_UN_AUTHORIZED);
    present_opts = CX_UN_PRESENT_ALL;
    n_banner = n_removed = n_receive = 0;
    last_badge = -1;
}

int main(void) {
    const double T0 = 1794830400;                      /* 2026-11-16 12:00 UTC (segunda, 09:00 em Brasília) */
    cx_un_req_t r;

    /* autorização */
    fresh();
    cx_un_set_auth(CX_UN_DENIED);
    r = req("a", "x");
    check(cx_un_add(&r) == CX_UN_ERR_DENIED && cx_un_count(1) == 0, "sem autorização o pedido é recusado");

    /* entrega imediata e banner */
    fresh();
    cx_un_set_clock(T0);
    r = req("agora", "Olá");
    r.sound = "default";
    check(cx_un_add(&r) == CX_UN_OK && cx_un_count(1) == 1 && cx_un_count(0) == 0, "sem gatilho: entregue na hora");
    check(n_banner == 1 && last_token > 0 && !strcmp(last_title, "Olá") && last_sound == 1, "banner com título e som");
    check(cx_un_at(1, 0)->date == T0, "data da entrega");
    r = req("mudo", "x");
    cx_un_add(&r);
    check(last_sound == 0, "sem som definido: banner mudo");

    /* intervalo */
    fresh();
    cx_un_set_clock(T0);
    r = req("int", "depois");
    r.trig = CX_UN_TRIG_INTERVAL;
    r.interval = 30;
    cx_un_add(&r);
    check(cx_un_count(0) == 1 && cx_un_at(0, 0)->fire_at == T0 + 30 && n_banner == 0, "intervalo fica pendente");
    check(cx_un_next_fire() == T0 + 30, "próxima entrega");
    cx_un_set_clock(T0 + 29);
    check(n_banner == 0, "ainda não venceu");
    cx_un_set_clock(T0 + 30);
    check(n_banner == 1 && cx_un_count(0) == 0 && cx_un_count(1) == 1, "venceu: entregue e sai dos pendentes");
    r.repeats = 1;
    r.interval = 59;
    check(cx_un_add(&r) == CX_UN_ERR_BAD, "repetir com menos de 60 s é inválido");
    r.interval = 0;
    r.repeats = 0;
    check(cx_un_add(&r) == CX_UN_ERR_BAD, "intervalo 0 é inválido");
    r.interval = 60;
    r.repeats = 1;
    cx_un_add(&r);
    cx_un_set_clock(T0 + 30 + 185);                     /* venceu 3 vezes com o app parado */
    check(n_banner == 2 && cx_un_count(0) == 1 && cx_un_at(0, 0)->fire_at == T0 + 30 + 240, "repetição atrasada entrega uma vez e reprograma");
    check(cx_un_count(1) == 1, "mesmo identificador substitui o entregue");
    check(n_removed == 1, "o banner antigo sai da Central");

    /* mesmo identificador substitui o pendente */
    fresh();
    cx_un_set_clock(T0);
    r = req("p", "um");
    r.trig = CX_UN_TRIG_INTERVAL;
    r.interval = 100;
    cx_un_add(&r);
    r.title = "dois";
    r.interval = 200;
    cx_un_add(&r);
    check(cx_un_count(0) == 1 && !strcmp(cx_un_at(0, 0)->title, "dois") && cx_un_at(0, 0)->fire_at == T0 + 200, "pendente substituído");
    r = req("q", "q");
    r.trig = CX_UN_TRIG_INTERVAL;
    r.interval = 100;
    r.source = CX_UN_SRC_LEGACY;
    cx_un_add(&r);
    check(cx_un_remove_pending(0, CX_UN_SRC_UN) == 1 && cx_un_count(0) == 1 && cx_un_find(0, "q"), "remover todos de uma origem");
    check(cx_un_remove_pending("q", -1) == 1 && cx_un_count(0) == 0, "remover pelo identificador");

    /* calendário (hora local: Brasília, UTC-3) */
    fresh();
    cx_un_set_tz_offset(-3 * 3600);
    cx_un_cal_t c9 = {-1, -1, -1, 9, 0, -1, -1};
    check(cx_un_calendar_next(&c9, T0, 1) == 1794916800, "todo dia às 9h (já passou hoje: amanhã)");
    cx_un_cal_t mon = {-1, -1, -1, 8, -1, -1, 2};
    check(cx_un_calendar_next(&mon, T0, 1) == 1795431600, "segunda às 8h: a próxima semana");
    cx_un_cal_t feb = {-1, 2, 29, -1, -1, -1, -1};
    check(cx_un_calendar_next(&feb, T0, 1) == 1835406000, "29 de fevereiro: 2028 à meia-noite");
    cx_un_cal_t m30 = {-1, -1, -1, -1, 30, -1, -1};
    check(cx_un_calendar_next(&m30, T0, 1) == 1794832200, "minuto 30 de qualquer hora");
    cx_un_cal_t past = {2025, 1, 1, 0, 0, 0, -1};
    check(cx_un_calendar_next(&past, T0, 0) < 0, "data no passado: nunca");
    cx_un_cal_t bad = {-1, 13, -1, -1, -1, -1, -1};
    check(cx_un_calendar_next(&bad, T0, 0) < 0, "mês 13 é inválido");
    cx_un_set_clock(T0);
    r = req("cal", "bom dia");
    r.trig = CX_UN_TRIG_CALENDAR;
    r.cal = c9;
    r.repeats = 1;
    check(cx_un_add(&r) == CX_UN_OK && cx_un_at(0, 0)->fire_at == 1794916800, "pedido de calendário");
    cx_un_set_clock(1794916800);
    check(n_banner == 1 && cx_un_count(0) == 1 && cx_un_at(0, 0)->fire_at == 1794916800 + 86400, "entregue e reprogramado para o dia seguinte");
    r.cal = past;
    r.repeats = 0;
    check(cx_un_add(&r) == CX_UN_ERR_BAD, "calendário que nunca dispara é recusado");

    /* categorias, ações, texto, fechar */
    fresh();
    cx_un_set_clock(T0);
    cx_un_action_t acts[2] = {{"responder", "Responder", 0, 1, "Enviar", "Mensagem"}, {"apagar", "Apagar", 2, 0, 0, 0}};
    cx_un_category_t cats[2] = {{"msg", 1, acts, 2}, {"simples", 0, 0, 0}};
    cx_un_set_categories(cats, 2);
    check(cx_un_category_count() == 2 && cx_un_category("msg")->nactions == 2 && cx_un_category("msg")->actions[0].text_input, "categorias copiadas");
    r = req("m1", "Nova mensagem");
    r.category = "msg";
    cx_un_add(&r);
    check(!strcmp(last_actions, "Responder;Apagar;"), "banner leva as ações da categoria");
    int tk = last_token;
    check(cx_un_event(tk, 0, "oi!") == 1 && n_receive == 1 && !strcmp(recv_action, "responder") && !strcmp(recv_text, "oi!"), "resposta em texto");
    check(cx_un_count(1) == 0, "ação tira da Central");
    check(cx_un_event(tk, 0, "de novo") == 0, "token usado não vale mais");
    cx_un_add(&r);
    tk = last_token;
    cx_un_event(tk, 1, 0);
    check(!strcmp(recv_action, "apagar") && recv_text_null, "botão sem texto: userText ausente");
    cx_un_add(&r);
    tk = last_token;
    cx_un_event(tk, -2, 0);
    check(!strcmp(recv_action, CX_UN_ACTION_DISMISS) && cx_un_count(1) == 0, "fechar com customDismissAction");
    r.category = "simples";
    cx_un_add(&r);
    tk = last_token;
    n_receive = 0;
    cx_un_event(tk, -2, 0);
    check(n_receive == 0, "fechar sem customDismissAction não avisa o app");
    cx_un_add(&r);
    tk = last_token;
    cx_un_event(tk, -1, 0);
    check(n_receive == 1 && !strcmp(recv_action, CX_UN_ACTION_DEFAULT) && cx_un_count(1) == 1, "clique no corpo: ação padrão, fica na Central");
    check(cx_un_event(tk, -1, 0) == 1, "clique de novo pela Central");
    check(cx_un_remove_delivered("m1", -1) == 1 && last_removed == tk, "remover entregue tira do banner/Central");

    /* apresentação e badge */
    fresh();
    cx_un_set_clock(T0);
    r = req("b", "badge");
    r.badge = 5;
    cx_un_add(&r);
    check(last_badge == 5 && cx_un_badge() == 5, "badge do conteúdo");
    present_opts = CX_UN_PRESENT_LIST;
    r.ident = "lista";
    r.badge = -1;
    n_banner = 0;
    cx_un_add(&r);
    check(n_banner == 0 && cx_un_find(1, "lista"), "só na lista: sem banner, mas na Central");
    present_opts = 0;
    r.ident = "nada";
    cx_un_add(&r);
    check(!cx_un_find(1, "nada"), "apresentação vazia: não entra na Central");
    present_opts = CX_UN_PRESENT_BADGE;
    r.ident = "so-badge";
    r.badge = 2;
    cx_un_add(&r);
    check(last_badge == 2 && !cx_un_find(1, "so-badge"), "só badge");
    cx_un_set_badge(0);
    check(last_badge == 0, "setBadgeCount 0");
    present_opts = CX_UN_PRESENT_ALL;
    for (int i = 0; i < 70; i++) { char id[16]; snprintf(id, sizeof id, "n%d", i); r = req(id, "t"); cx_un_add(&r); }
    check(cx_un_count(1) == 64 && !cx_un_find(1, "n0") && cx_un_find(1, "n69"), "Central guarda as 64 mais novas");

    /* persistência */
    char path[] = "/tmp/un-host-XXXXXX";
    int fd = mkstemp(path);
    close(fd);
    unlink(path);
    fresh();
    cx_un_set_clock(T0);
    cx_un_set_store(path);
    r = req("salvo", "linha 1\nlinha\t2 \\ fim");
    r.subtitle = "";
    r.info = "<plist>x</plist>";
    r.trig = CX_UN_TRIG_CALENDAR;
    r.cal = (cx_un_cal_t){-1, -1, -1, 9, 0, -1, -1};
    r.repeats = 1;
    r.badge = 3;
    cx_un_add(&r);
    r = req("vence", "venceu fechado");
    r.trig = CX_UN_TRIG_INTERVAL;
    r.interval = 60;
    cx_un_add(&r);
    r = req("entregue", "já foi");
    cx_un_add(&r);
    cx_un_set_badge(7);
    fresh();                                            /* o app fecha */
    cx_un_set_tz_offset(-3 * 3600);
    check(cx_un_set_store(path) == 3, "carrega 2 pendentes e 1 entregue");
    const cx_un_req_t *s = cx_un_find(0, "salvo");
    check(s && !strcmp(s->title, "linha 1\nlinha\t2 \\ fim") && s->subtitle && !s->subtitle[0] && !s->body, "texto com escapes, vazio e ausente");
    check(s && !strcmp(s->info, "<plist>x</plist>") && s->trig == CX_UN_TRIG_CALENDAR && s->cal.hour == 9 && s->repeats && s->badge == 3, "gatilho e campos");
    check(cx_un_find(1, "entregue") && cx_un_find(1, "entregue")->token == 0 && cx_un_badge() == 7, "entregue sem token da sessão anterior; badge");
    n_banner = 0;
    cx_un_set_clock(T0 + 3600);                        /* o app abre uma hora depois */
    check(n_banner == 1 && cx_un_find(1, "vence") && !cx_un_find(0, "vence"), "o que venceu com o app fechado sai ao abrir");
    fresh();
    cx_un_set_store(path);
    check(cx_un_find(1, "vence") && cx_un_count(0) == 1, "a entrega também foi gravada");
    cx_un_reset();
    unlink(path);

    printf("un_host: %d/%d %s\n", ok, total, ok == total ? "ok" : "FALHOU");
    return ok == total ? 0 : 1;
}
