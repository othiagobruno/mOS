/* UserNotifications local do mOS (I23).
 * Pedido, gatilho imediato ou de intervalo, banner e delegate.
 * O teste de host compila un_center.c com -DCX_UN_HOST.
 * usernotifications.c liga os métodos ObjC e o banner do kernel.
 * Não há APNs. */
#pragma once

enum {
    CX_UN_NOT_DETERMINED = 0,
    CX_UN_DENIED = 1,
    CX_UN_AUTHORIZED = 2
};

enum {
    CX_UN_OK = 0,
    CX_UN_ERR_DENIED = 1,
    CX_UN_ERR_BAD = 2,
    CX_UN_ERR_FULL = 3
};

/* UNNotificationPresentationOptions */
enum {
    CX_UN_PRESENT_BADGE = 1 << 0,
    CX_UN_PRESENT_SOUND = 1 << 1,
    CX_UN_PRESENT_ALERT = 1 << 2,
    CX_UN_PRESENT_LIST = 1 << 3,
    CX_UN_PRESENT_BANNER = 1 << 4
};

#define CX_UN_ACTION_DEFAULT "com.apple.UNNotificationDefaultActionIdentifier"

/* Devolve as opções de apresentação. NULL no registro: banner e lista. */
typedef int (*cx_un_present_fn)(const char *ident, const char *title, const char *body, void *ctx);
typedef void (*cx_un_receive_fn)(const char *ident, const char *action, void *ctx);
typedef void (*cx_un_banner_fn)(const char *title, const char *body, int token, void *ctx);

void cx_un_reset(void);
void cx_un_set_auth(int status);
int cx_un_auth(void);
void cx_un_set_delegate(cx_un_present_fn present, cx_un_receive_fn receive, void *ctx);
void cx_un_set_banner_sink(cx_un_banner_fn fn, void *ctx);

/* interval <= 0 entrega agora. repeats só reprograma quando interval > 0. */
/* Texto do userInfo para o próximo cx_un_add. Vazio limpa. */
void cx_un_stage_info(const char *info);
const char *cx_un_info(const char *ident);

int cx_un_add(const char *ident, const char *title, const char *subtitle, const char *body,
              double interval, int repeats);
int cx_un_remove_pending(const char *ident);   /* ident NULL: todos os pendentes */
int cx_un_remove_delivered(const char *ident); /* ident NULL: todas as entregues */
int cx_un_pending_count(void);
int cx_un_delivered_count(void);
int cx_un_pending_has(const char *ident);
int cx_un_delivered_has(const char *ident);

void cx_un_set_clock(double t);
void cx_un_advance(double dt);
void cx_un_flush(void);

int cx_un_banner_visible(void);
const char *cx_un_banner_title(void);
const char *cx_un_banner_body(void);
int cx_un_banner_token(void);
int cx_un_banner_click(void);
int cx_un_click_token(int token);

int cx_un_present_count(void);
int cx_un_receive_count(void);
const char *cx_un_last_action(void);
const char *cx_un_last_ident(void);
