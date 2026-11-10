/* Modelo do computer use usado pelo teste de host (dois processos, memória compartilhada).
 * Pixels, geometria, clique e AX passam por estas funções. No mOS o mesmo contrato sai do
 * compositor (DEV_SCREEN_CAPTURE / DEV_WINDOW_LIST / DEV_INPUT_POST) e de appkit_ax.c.
 * A concessão usa os nomes kTCCServiceScreenCapture e kTCCServiceAccessibility, o papel de
 * cx_tcc_decide. Este arquivo não grava o decisions.txt do TCC. */
#pragma once

enum { CU_W = 640, CU_H = 400 };

enum { CU_BUTTON = 1, CU_FIELD = 2 };
enum { CU_KIND_APP = 1, CU_KIND_WIN = 2, CU_KIND_CTL = 3 };

enum {
    CU_ATTR_NONE = 0,
    CU_ATTR_STR = 1,
    CU_ATTR_INT = 2,
    CU_ATTR_BOX = 3,
    CU_ATTR_KIDS = 4
};

typedef struct {
    int id, pid, layer, onscreen, x, y, w, h;
    char owner[32];
    char name[64];
} cu_info;

typedef struct {
    int type;
    char str[80];
    int i;
    int x, y, w, h;
    int nkids;
    int kid_kind[8];
    int kid_id[8];
} cu_attr;

typedef struct cu_world cu_world;

cu_world *cu_world_bytes(void *mem);
unsigned cu_world_bytes_size(void);

void cu_init(cu_world *w);
void cu_grant(cu_world *w, const char *service);

int cu_open_window(cu_world *w, int pid, const char *owner, const char *name, int x, int y, int wd, int ht, unsigned bg);
int cu_add_button(cu_world *w, int win, const char *title, int x, int y, int wd, int ht, unsigned color);
int cu_add_field(cu_world *w, int win, int x, int y, int wd, int ht);

int cu_capture(cu_world *w, int win, unsigned *px, int *out_w, int *out_h);
int cu_list(cu_world *w, cu_info *out, int cap);
int cu_post_mouse(cu_world *w, int tap, int type, int x, int y);
int cu_post_key(cu_world *w, int tap, unsigned unicode, int down);
int cu_post_scroll(cu_world *w, int tap, int dy);
int cu_ax_copy(cu_world *w, int kind, int id, const char *attr, cu_attr *out);
int cu_ax_press(cu_world *w, int kind, int id);
int cu_ax_set(cu_world *w, int kind, int id, const char *attr, const char *value);

int cu_ctl_clicks(cu_world *w, int id);
int cu_ctl_presses(cu_world *w, int id);
int cu_win_scroll(cu_world *w, int id);
int cu_read_value(cu_world *w, int id, char *dst, int cap);

/* Foco de processo, separado do campo de texto em foco. PostToPid não mexe nele. */
void cu_activate(cu_world *w, int pid);
int cu_front_pid(cu_world *w);
int cu_running(cu_world *w, int index, int *pid, char *name, int cap);

/* O filho do pid aplica. O pai só enfileira. Não passa pela fila global. */
int cu_post_begin(cu_world *w, int pid, int type, int x, int y, int extra);
int cu_post_seq(cu_world *w);
void cu_post_poll(cu_world *w, int pid);

int cu_note_seq(cu_world *w);
void cu_note_read(cu_world *w, int *pid, int *kind, int *id, char *name, int cap);

/* Caixa de bytes do protocolo AX. Não é o IPC de produção. */
int cu_ax_begin(cu_world *w, int pid, const void *req, int n);
int cu_ax_seq(cu_world *w);
int cu_ax_poll(cu_world *w, int pid, void *req, int cap);
void cu_ax_finish(cu_world *w, const void *rep, int n);
int cu_ax_reply(cu_world *w, void *rep, int cap);

enum { CU_READY = 0, CU_DONE, CU_APP_CLICKS, CU_APP_PRESSES, CU_APP_SCROLL, CU_APP_TEXT_N };
int cu_load(cu_world *w, int slot);
void cu_store(cu_world *w, int slot, int v);
void cu_publish_text(cu_world *w, const char *s, int n);
void cu_copy_text(cu_world *w, char *dst, int cap);
