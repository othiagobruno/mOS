/*
 * mOS libSystem — os_activity (libsystem_trace): atividades com id único, escopo por thread (enter/leave), apply e
 * a atividade atual. Não há o daemon de log de atividades da Apple: o id e o pai são guardados e consultáveis
 * (os_activity_get_identifier). Também os bootstrap_* antigos e _dyld_bind_fully_image_containing_address.
 * docs/darwin-bsd-mach.md (lacunas do corpus, docs/lacunas/renata.md).
 */
#include "sys.h"

void *calloc(size_t n, size_t s);
typedef u32 mach_port_t;
int mach_generate_activity_id(mach_port_t task, int count, u64 *id);

typedef struct os_act { u64 id, parent; const char *desc; u32 flags; } os_act_t;
/* OS_ACTIVITY_NONE = &_os_activity_none, OS_ACTIVITY_CURRENT = &_os_activity_current (os/activity.h) */
os_act_t _os_activity_none = {0, 0, "none", 0};
os_act_t _os_activity_current = {0, 0, "current", 0};
os_act_t _os_activity_default = {0, 0, "default", 0};
/* atividade atual por thread numa chave de pthread (sem __thread: o %fs das threads é 0 no mOS, sysconfig.c) */
typedef unsigned long pthread_key_t;
int pthread_key_create(pthread_key_t *k, void (*dtor)(void *));
void *pthread_getspecific(pthread_key_t k);
int pthread_setspecific(pthread_key_t k, const void *v);
static pthread_key_t act_key;
static volatile int act_key_state;   /* 0 nada, 1 criando, 2 pronta */
static pthread_key_t key(void) {
    if (act_key_state != 2) {
        if (__sync_bool_compare_and_swap(&act_key_state, 0, 1)) { pthread_key_create(&act_key, 0); __sync_synchronize(); act_key_state = 2; }
        else while (act_key_state != 2) SYS0(SYS_YIELD);
    }
    return act_key;
}
#define cur_get() ((os_act_t *)pthread_getspecific(key()))
#define cur_set(v) pthread_setspecific(key(), (v))

static u64 new_id(void) {
    u64 id = 0;
    if (mach_generate_activity_id(0x103, 1, &id) != 0 || !id) { static volatile u64 n = 1; id = __sync_fetch_and_add(&n, 1); }
    return id;
}
static os_act_t *resolve(os_act_t *a) { return a == &_os_activity_current ? cur_get() : a; }

/* os_activity_create(dso, descrição, pai, flags): OS_ACTIVITY_FLAG_DETACHED (1) não herda o pai */
void *_os_activity_create(void *dso, const char *desc, void *parent, u32 flags) {
    (void)dso;
    os_act_t *a = calloc(1, sizeof *a);
    if (!a) return &_os_activity_none;
    a->id = new_id();
    a->desc = desc;
    a->flags = flags;
    os_act_t *p = (flags & 1) ? 0 : resolve(parent);
    a->parent = p && p != &_os_activity_none ? p->id : 0;
    return a;
}
void *os_activity_create(void *dso, const char *desc, void *parent, u32 flags) { return _os_activity_create(dso, desc, parent, flags); }

/* os_activity_scope_state_s {u64 opaque[2]}: guarda a atividade de antes */
void os_activity_scope_enter(void *activity, u64 *state) {
    if (state) { state[0] = (u64)(usize)cur_get(); state[1] = 0x6d4f53u; }
    os_act_t *a = resolve(activity);
    cur_set(a == &_os_activity_none ? 0 : a);
}
void os_activity_scope_leave(u64 *state) {
    if (!state || state[1] != 0x6d4f53u) return;           /* estado que não veio do enter: ignora, como o macOS */
    cur_set((os_act_t *)(usize)state[0]);
    state[1] = 0;
}
/* os_activity_apply / apply_f: roda o bloco/função com a atividade como atual */
typedef struct { void *isa; int flags, res; void (*invoke)(void *); } os_block_t;
void os_activity_apply_f(void *activity, void *ctx, void (*fn)(void *)) {
    u64 st[2];
    os_activity_scope_enter(activity, st);
    if (fn) fn(ctx);
    os_activity_scope_leave(st);
}
void os_activity_apply(void *activity, void *block) {
    u64 st[2];
    os_activity_scope_enter(activity, st);
    if (block) ((os_block_t *)block)->invoke(block);
    os_activity_scope_leave(st);
}
/* _os_activity_initiate(dso, desc, flags, block) e _f: cria, aplica e solta */
void _os_activity_initiate(void *dso, const char *desc, u32 flags, void *block) { os_activity_apply(_os_activity_create(dso, desc, &_os_activity_current, flags), block); }
void _os_activity_initiate_f(void *dso, const char *desc, u32 flags, void *ctx, void (*fn)(void *)) { os_activity_apply_f(_os_activity_create(dso, desc, &_os_activity_current, flags), ctx, fn); }
void *_os_activity_start(void *dso, const char *desc, u32 flags) { os_act_t *a = _os_activity_create(dso, desc, &_os_activity_current, flags); cur_set(a); return (void *)(usize)a->id; }
void os_activity_end(void *id) { (void)id; cur_set(0); }
u64 os_activity_get_identifier(void *activity, u64 *parent) {
    os_act_t *a = resolve(activity);
    if (!a || a == &_os_activity_none) { if (parent) *parent = 0; return 0; }
    if (parent) *parent = a->parent;
    return a->id;
}
u64 os_activity_get_active(u64 *ids, unsigned *count) {   /* a atividade atual da thread */
    os_act_t *c = cur_get();
    u64 id = c ? c->id : 0;
    if (ids && count && *count) { ids[0] = id; *count = id ? 1 : 0; }
    return id;
}
void os_activity_label_useraction(const char *label) { (void)label; }   /* rótulo de ação do usuário: sem coletor */
void _os_activity_label_useraction(void *dso, const char *label) { (void)dso; (void)label; }
void os_activity_set_breadcrumb(const char *name) { (void)name; }

/* ---- bootstrap_* antigos: o launchd atual os aposentou e responde BOOTSTRAP_NOT_PRIVILEGED (1100) a quem não é
 * root; bootstrap_parent devolve o próprio bootstrap (não há subconjuntos). */
extern u32 bootstrap_port;
int bootstrap_parent(u32 bp, u32 *parent) { if (!parent) return 1103; *parent = bp ? bp : bootstrap_port; return 0; }
int bootstrap_subset(u32 bp, u32 requestor, u32 *subset) { (void)bp; (void)requestor; if (subset) *subset = 0; return 1100; }
int bootstrap_create_server(u32 bp, const char *cmd, unsigned uid, int ondemand, u32 *server) { (void)bp; (void)cmd; (void)uid; (void)ondemand; if (server) *server = 0; return 1100; }
int bootstrap_create_service(u32 bp, const char *name, u32 *port) { (void)bp; (void)name; if (port) *port = 0; return 1100; }

/* o carregador do mOS liga todos os símbolos de uma imagem ao carregá-la (sem lazy binding): já está tudo ligado */
int _dyld_bind_fully_image_containing_address(const void *addr) { return addr != 0; }
