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
enum { CX_OK = 0, CX_ERR_NO_PROVIDER = 1, CX_ERR_BAD = 2 };

enum { CX_CALL_RINGING = 1, CX_CALL_ACTIVE = 2, CX_CALL_ENDED = 3 };

typedef struct {
    int kind;
    int state;
    int muted; /* 0 ou 1; -1 se a ação não mexe no mudo */
    int held;
    char uuid[48];
    char handle[80];
} cx_act;

typedef struct {
    int alive; /* 0 livre, senão CX_CALL_* */
    int held;
    int muted;
    char uuid[48];
    char handle[80];
} cx_call;

typedef struct {
    int provider;
    int delegate;
    cx_act acts[16];
    int nacts;
    cx_call calls[16];
    int ncalls;
} cx_box;

void cx_box_init(cx_box *b);
int cx_set_provider(cx_box *b, int on, int delegate_on);
int cx_submit(cx_box *b, int kind, const char *uuid, const char *handle, int muted, int held, int *idx);
int cx_fulfill(cx_box *b, int idx);
int cx_fail(cx_box *b, int idx);
int cx_report_incoming(cx_box *b, const char *uuid, const char *handle);
const cx_call *cx_find_call(const cx_box *b, const char *uuid);

/* CoreLocation. Concedido no TCC vira WhenInUse. Sem GPS o resultado nunca é coordenada. */
enum { CL_AUTH_NOT = 0, CL_AUTH_DENIED = 2, CL_AUTH_WHEN_IN_USE = 4 };
enum { CL_ERR_UNKNOWN = 0, CL_ERR_DENIED = 1 };
int cl_auth_from_tcc(int tcc_state);
int cl_location_error(int auth);
int cl_coord_valid(double lat, double lon);

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
void bg_box_init(bg_box *b);
int bg_register(bg_box *b, const char *ident);
/* 0 e *err preenchido: a tarefa fica gravada só se o identificador foi registrado. */
int bg_submit(bg_box *b, const char *ident, int kind, int *err);

/* CoreTelephony: sem operadora. */
int ct_provider_count(void);
const char *ct_radio(void);

#endif
