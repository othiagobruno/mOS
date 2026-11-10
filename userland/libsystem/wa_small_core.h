/*
 * Estado local do que o WhatsApp 2.26.45.13 importa de CallKit, CoreTelephony,
 * CoreLocation e BackgroundTasks. Não há rede celular, GPS nem execução em background.
 */
#ifndef MOS_WA_SMALL_CORE_H
#define MOS_WA_SMALL_CORE_H

enum {
    CX_START = 1,
    CX_ANSWER,
    CX_END,
    CX_HOLD,
    CX_MUTE
};
enum { CX_PENDING = 0, CX_FULFILLED = 1, CX_FAILED = 2 };
enum { CX_OK = 0, CX_ERR_NO_PROVIDER = 1, CX_ERR_BAD = 2, CX_ERR_LIMIT = 3, CX_ERR_HANDLE = 4, CX_ERR_VIDEO = 5 };

enum { CX_CALL_RINGING = 1, CX_CALL_ACTIVE = 2, CX_CALL_ENDED = 3 };
enum { CX_HANDLE_PHONE = 1, CX_HANDLE_EMAIL = 2, CX_HANDLE_GENERIC = 3 };
enum { CX_EV_ACTION = 1, CX_EV_CHANGED = 2 };

typedef struct {
    int on;
    int max_groups; /* padrão da Apple: 2 */
    int max_per;    /* padrão da Apple: 5 */
    int video;
    unsigned handle_mask; /* bit (1u << CX_HANDLE_*) */
} cx_cfg;

typedef struct {
    int has_handle;
    int handle_type;
    char handle[80];
    int has_name;
    char name[80];
    int has_video; /* -1 ausente, 0 ou 1 */
    int hold, group, ungroup, dtmf; /* -1 ausente */
} cx_update;

typedef struct {
    int kind;
    int state;
    int muted;
    int held;
    int video;
    int handle_type;
    int group;
    char uuid[48];
    char handle[80];
} cx_act;

typedef struct {
    int alive;
    int held;
    int muted;
    int outgoing;
    int on_hold;
    int has_connected;
    int has_ended;
    int has_video;
    int group;
    int cap_hold, cap_group, cap_ungroup, cap_dtmf;
    int handle_type;
    char uuid[48];
    char handle[80];
    char name[80];
} cx_call;

typedef struct {
    int qid;
    int what;
    int action;
    int outgoing, on_hold, connected, ended;
    char uuid[48];
} cx_ev;

typedef struct {
    int provider;
    int delegate;
    int delegate_q;
    int observer;
    int observer_q;
    int next_group;
    cx_cfg cfg;
    cx_act acts[16];
    int nacts;
    cx_call calls[16];
    int ncalls;
    cx_ev q[32];
    int nq;
} cx_box;

void wa_cx_box_init(cx_box *b);
int wa_cx_set_provider(cx_box *b, int on, int delegate_on);
void wa_cx_set_delegate_queue(cx_box *b, int qid);
void wa_cx_set_observer(cx_box *b, int on, int qid);
void wa_cx_set_config(cx_box *b, const cx_cfg *cfg);
int wa_cx_handle_equal(int t1, const char *v1, int t2, const char *v2);
unsigned wa_cx_handle_hash(int type, const char *value);
int wa_cx_submit(cx_box *b, int kind, const char *uuid, const char *handle, int muted, int held, int *idx);
int wa_cx_submit_full(cx_box *b, int kind, const char *uuid, const char *handle, int handle_type, int muted, int held, int video, int *idx);
int wa_cx_fulfill(cx_box *b, int idx);
int wa_cx_fail(cx_box *b, int idx);
int wa_cx_report_incoming(cx_box *b, const char *uuid, const char *handle);
int wa_cx_apply_update(cx_box *b, const char *uuid, const cx_update *u);
const cx_call *wa_cx_find_call(const cx_box *b, const char *uuid);
int wa_cx_call_count(const cx_box *b);
const cx_call *wa_cx_call_at(const cx_box *b, int i);
int wa_cx_drain(cx_box *b, int qid, cx_ev *out, int cap);

/* CoreLocation. Concedido no TCC vira WhenInUse. Sem GPS o resultado nunca é coordenada. */
enum { CL_AUTH_NOT = 0, CL_AUTH_DENIED = 2, CL_AUTH_WHEN_IN_USE = 4 };
enum { CL_ERR_UNKNOWN = 0, CL_ERR_DENIED = 1 };
int wa_cl_auth(int tcc_state);
int wa_cl_error(int auth);
int wa_cl_coord_valid(double lat, double lon);

/* BackgroundTasks. 2 = indisponível, 4 = não registrado. */
enum { BG_REFRESH = 1, BG_PROCESSING = 2, BG_ERR_UNAVAILABLE = 2, BG_ERR_NOT_PERMITTED = 4 };
typedef struct {
    char id[80];
    int kind;
} bg_item;
typedef struct {
    bg_item reg[8];
    int nreg;
    bg_item sub[8];
    int nsub;
} bg_box;
void wa_bg_init(bg_box *b);
int wa_bg_register(bg_box *b, const char *ident);
/* 0 e *err preenchido: a tarefa fica gravada só se o identificador foi registrado. */
int wa_bg_submit(bg_box *b, const char *ident, int kind, int *err);

/* CoreTelephony: sem operadora. */
int wa_ct_providers(void);
const char *wa_ct_radio(void);

#endif
