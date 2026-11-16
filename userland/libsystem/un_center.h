/* Centro local de notificações do mOS (UserNotifications e NSUserNotification), sem classes.
 * Pedidos pendentes (gatilho de intervalo ou de calendário) e notificações entregues ficam em listas separadas,
 * como no macOS, e vão para um arquivo por app (sobrevivem ao app fechar; o que venceu com o app fechado é entregue
 * quando ele abre). Categorias com ações, badge e o retorno do clique (ação, texto digitado, "fechar").
 * usernotifications.c e nsusernotification.c ligam as classes; o teste de host compila este arquivo com
 * -DCX_UN_HOST (userland/tests-appkit/un_host.c). Não há APNs. */
#pragma once

enum { CX_UN_NOT_DETERMINED = 0, CX_UN_DENIED = 1, CX_UN_AUTHORIZED = 2, CX_UN_PROVISIONAL = 3 };
enum { CX_UN_OK = 0, CX_UN_ERR_DENIED = 1, CX_UN_ERR_BAD = 2, CX_UN_ERR_FULL = 3 };

/* UNNotificationPresentationOptions */
enum {
    CX_UN_PRESENT_BADGE = 1 << 0,
    CX_UN_PRESENT_SOUND = 1 << 1,
    CX_UN_PRESENT_ALERT = 1 << 2,
    CX_UN_PRESENT_LIST = 1 << 3,
    CX_UN_PRESENT_BANNER = 1 << 4
};
#define CX_UN_PRESENT_ALL (CX_UN_PRESENT_BADGE | CX_UN_PRESENT_SOUND | CX_UN_PRESENT_LIST | CX_UN_PRESENT_BANNER)

#define CX_UN_ACTION_DEFAULT "com.apple.UNNotificationDefaultActionIdentifier"
#define CX_UN_ACTION_DISMISS "com.apple.UNNotificationDismissActionIdentifier"

enum { CX_UN_TRIG_NONE = 0, CX_UN_TRIG_INTERVAL = 1, CX_UN_TRIG_CALENDAR = 2, CX_UN_TRIG_PUSH = 3 };
enum { CX_UN_SRC_UN = 0, CX_UN_SRC_LEGACY = 1 };   /* UNUserNotificationCenter ou NSUserNotificationCenter */

/* -1 num campo = qualquer. Hora local (cx_un_set_tz_offset). weekday: 1 = domingo ... 7 = sábado */
typedef struct { int year, month, day, hour, minute, second, weekday; } cx_un_cal_t;

typedef struct {
    char *ident, *title, *subtitle, *body, *category, *thread, *sound, *info, *attach, *target, *launch_image;
    int badge;                         /* -1 = não mexe no badge */
    int interruption;                  /* UNNotificationInterruptionLevel */
    double relevance;
    int source;                        /* CX_UN_SRC_* */
    int trig, repeats;                 /* CX_UN_TRIG_* */
    double interval;                   /* segundos (intervalo) */
    cx_un_cal_t cal;
    /* estado (preenchido pelo centro) */
    double fire_at;                    /* pendente: próxima entrega (Unix); no add, a primeira (intervalo) */
    double date;                       /* entregue: quando */
    int token;                         /* entregue: o do banner (0 = sem banner nesta sessão) */
} cx_un_req_t;

/* UNNotificationActionOptions: 1 authenticationRequired, 2 destructive, 4 foreground */
typedef struct { char *ident, *title; int options, text_input; char *button, *placeholder; } cx_un_action_t;
/* UNNotificationCategoryOptions: 1 customDismissAction */
typedef struct { char *ident; int options; cx_un_action_t *actions; int nactions; } cx_un_category_t;

typedef int (*cx_un_present_fn)(const cx_un_req_t *r, void *ctx);              /* devolve as opções */
typedef void (*cx_un_receive_fn)(const cx_un_req_t *r, const char *action, const char *text, void *ctx);
typedef struct {
    /* mostra o banner; cat pode ser NULL; sound = 0 pede silêncio */
    void (*banner)(const cx_un_req_t *r, const cx_un_category_t *cat, int sound, void *ctx);
    void (*remove)(int token, void *ctx);                                       /* tirar da tela e da Central */
    void (*badge)(int count, void *ctx);                                        /* 0 = sem badge */
    void *ctx;
} cx_un_sink_t;

void cx_un_reset(void);
/* arquivo do app: carrega agora e grava a cada mudança (NULL = só memória) */
int cx_un_set_store(const char *path);
void cx_un_set_auth(int status);
int cx_un_auth(void);
void cx_un_set_delegate(cx_un_present_fn present, cx_un_receive_fn receive, void *ctx);
void cx_un_set_sink(const cx_un_sink_t *sink);
void cx_un_set_tz_offset(int seconds);           /* hora local = UTC + offset */

/* copia o pedido. Mesmo identificador substitui o pendente. Sem gatilho: entrega agora. */
int cx_un_add(const cx_un_req_t *spec);
int cx_un_count(int delivered);
const cx_un_req_t *cx_un_at(int delivered, int i);
const cx_un_req_t *cx_un_find(int delivered, const char *ident);
int cx_un_remove_pending(const char *ident, int source);     /* ident NULL: todos da origem */
int cx_un_remove_delivered(const char *ident, int source);

void cx_un_set_categories(const cx_un_category_t *cats, int n);   /* as do app; as internas ficam */
#define CX_UN_INTERNAL_CAT "__mos.nsun:"                             /* prefixo das categorias internas */
void cx_un_put_category(const cx_un_category_t *c);                /* uma (nactions < 0 apaga) */
int cx_un_category_count(void);
const cx_un_category_t *cx_un_category_at(int i);
const cx_un_category_t *cx_un_category(const char *ident);

void cx_un_set_badge(int count);
int cx_un_badge(void);

/* clique/ação no banner: action = -1 corpo, -2 fechado, 0.. botão da categoria; 1 = era deste app */
int cx_un_event(int token, int action, const char *text);

double cx_un_calendar_next(const cx_un_cal_t *c, double from, int repeats);
double cx_un_now(void);
void cx_un_set_clock(double t);                  /* teste: relógio fixo (0 = o de verdade) */
void cx_un_flush(void);                          /* entrega o que venceu */
void cx_un_set_auto_flush(int on);               /* 0: cx_un_add não entrega na hora (quem chama faz o flush) */
double cx_un_next_fire(void);                    /* próxima entrega pendente (0 = nenhuma) */

/* UNNotificationSetting: 0 notSupported, 1 disabled, 2 enabled; alert_style 0 none, 1 banner, 2 alert */
typedef struct { int authorization, sound, badge, alert, notification_center, lock_screen, alert_style; } cx_un_settings_t;
void cx_un_settings(cx_un_settings_t *out);
