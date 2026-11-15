/* mOS — contenção de falhas dos apps do sistema (kernel/apps e as janelas do compat).
 *
 * Os apps do próprio desktop rodam no ring 0, no laço do BSP. Sem isto, uma exceção de CPU dentro de um
 * paint, tick, tecla ou menu de um app cai em exc_panic e derruba o sistema inteiro.
 *
 * app_guard_run marca um ponto de retorno (kjmp_set, arch/kjmp.S) e chama o código do app. Se a CPU levantar
 * uma exceção recuperável no BSP enquanto essa guarda está ativa, isr_dispatch chama app_guard_fault, que
 * troca o RIP do frame por um trampolim; o iret cai no trampolim, que faz kjmp_go até a guarda. Um callback
 * que passa de 10 s é tirado do mesmo jeito pelo tique do PIT; com as interrupções desligadas o PIT não chega,
 * e um AP que vigia pelo HPET manda um NMI ao núcleo 0 (app_guard_ap_tick / app_guard_nmi).
 * O app é marcado e encerrado depois, no topo do laço do desktop (app_guard_reap): as janelas fecham (o
 * on_close de cada uma roda, protegido, para o app soltar os ponteiros) e o on_quit não roda.
 *
 * Só se recupera quando é seguro desfazer a pilha:
 *  - núcleo 0 (o laço do desktop) e código do kernel (CPL 0);
 *  - nenhuma seção irq_save aberta neste núcleo (g_irqoff[0] == 0): todo spinlock do kernel passa por irq_save,
 *    então nenhum está preso e nenhuma estrutura protegida está no meio de uma alteração. Exceção e PIT exigem
 *    também IF=1; o NMI aceita IF=0 (desligado pelo próprio app) e religa as interrupções na volta;
 *  - fora de par_for: o trabalho fica na pilha do BSP e os outros núcleos ainda podem lê-lo;
 *  - exceções: só #DE, #BR, #UD, #NM, #GP, #PF, #MF, #AC e #XM. #DF, #MC e NMI de hardware continuam em pânico.
 * App travado com IF=0 dentro de uma seção crítica do kernel não pode ser desfeito: vira tela de pânico com o nome
 * dele (antes congelava sem aviso).
 * Vazamento de memória do app que caiu é aceito: o estado dele é descartado, não liberado. */
#include "ui.h"
#include "apps.h"
extern window_t *wm_nth(int i);

/* arch/kjmp.S: setjmp/longjmp com todos os registradores preservados do ABI Win64 (xmm6..15 inclusive) */
typedef struct { _Alignas(16) u8 b[256]; } kjmp_buf_t;
__attribute__((returns_twice)) int kjmp_set(kjmp_buf_t *b);
__attribute__((noreturn)) void kjmp_go(kjmp_buf_t *b, int v);

typedef struct guard {
    kjmp_buf_t jb;
    app_t *app;
    struct guard *prev;
    u64 t0;                              /* timer_ticks() na entrada: o watchdog mede o tempo sem voltar */
    u64 t0_ns;                           /* hpet_ns() na entrada: com IF=0 o PIT para e só o HPET anda */
    bool if_on;                          /* interrupções ligadas na entrada (o NMI as religa ao voltar) */
} guard_t;

static guard_t *volatile cur;
static guard_t *volatile landing;        /* a guarda para onde o trampolim saltou */
static volatile int fault_vec;          /* -1 = não respondeu (watchdog); -2 = idem, com interrupções desligadas */
/* o que o vigia num AP lê sem mexer na pilha do núcleo 0 (a guarda vive nela e pode sumir a qualquer momento) */
static volatile u64 watch_t0_ns, watch_serial, nmi_for_serial;
static volatile bool nmi_pending;
static u64 guard_serial;
static u64 hang_ms = 10000;
static volatile u64 fault_rip, fault_cr2;

#define MAX_DEAD 8
static app_t *dead[MAX_DEAD];          /* caíram e esperam app_guard_reap */
static bool dead_hung[MAX_DEAD];       /* true = não respondeu (watchdog); false = exceção */
static int ndead;
#define MAX_TRACK 32
static struct { app_t *app; int crashes; } track[MAX_TRACK];

static bool is_dead(app_t *a) {
    for (int i = 0; i < ndead; i++) if (dead[i] == a) return true;
    return false;
}

static int *crash_count(app_t *a) {
    for (int i = 0; i < MAX_TRACK; i++) if (track[i].app == a) return &track[i].crashes;
    for (int i = 0; i < MAX_TRACK; i++) if (!track[i].app) { track[i].app = a; return &track[i].crashes; }
    return NULL;
}

int app_guard_crashes(app_t *a) { int *c = a ? crash_count(a) : NULL; return c ? *c : 0; }

static __attribute__((noinline, noreturn)) void guard_trampoline(void) {
    guard_t *g = cur;
    landing = g;
    kjmp_go(&g->jb, 1);
}

extern bool par_busy(void);

/* chamada por isr_dispatch numa exceção de CPL 0. true = o frame agora volta para a guarda */
bool app_guard_fault(int_frame_t *f) {
    guard_t *g = cur;
    if (!g || this_cpu() != 0 || (f->cs & 3)) return false;
    switch (f->vector) {
    case 0: case 5: case 6: case 7: case 13: case 14: case 16: case 17: case 19: break;
    default: return false;
    }
    if (!(f->rflags & 0x200)) return false;          /* IF=0: pode haver spinlock preso */
    if (g_irqoff[0] || par_busy()) return false;     /* seção irq_save aberta (o AHCI faz sti/hlt dentro de uma) */
    u64 cr2;
    __asm__ volatile("mov %%cr2,%0" : "=r"(cr2));
    fault_vec = (int)f->vector; fault_rip = f->rip; fault_cr2 = cr2;
    /* o trampolim roda na mesma pilha, abaixo do ponto da falha, alinhado como na entrada de uma função */
    f->rsp = ((f->rsp - 512) & ~15ULL) - 8;
    f->rip = (u64)guard_trampoline;
    return true;
}

/* chamada por isr_dispatch no tique do PIT (só chega ao núcleo 0). Um callback de app que passou de hang_ms sem
 * voltar (laço infinito, espera que nunca termina) é tirado da CPU como numa exceção. Mesmas condições:
 * CPL 0, IF=1 no ponto interrompido (nenhum spinlock preso) e fora de par_for. true = o frame volta à guarda */
bool app_guard_watchdog(int_frame_t *f) {
    guard_t *g = cur;
    if (!g || this_cpu() != 0 || (f->cs & 3) || !(f->rflags & 0x200) || g_irqoff[0] || par_busy()) return false;
    if (timer_ticks() - g->t0 < hang_ms) return false;
    fault_vec = -1; fault_rip = f->rip; fault_cr2 = 0;
    f->rsp = ((f->rsp - 512) & ~15ULL) - 8;
    f->rip = (u64)guard_trampoline;
    return true;
}

/* Tique do LAPIC num AP. Se a guarda do núcleo 0 passou do limite + 3 s (o tique do PIT não pegou: interrupções
 * desligadas no núcleo 0), manda um NMI para ele, uma vez por guarda. */
void app_guard_ap_tick(void) {
    u64 serial = watch_serial, t0 = watch_t0_ns;
    if (!serial || !t0 || nmi_for_serial == serial) return;
    u64 now = hpet_ns();
    if (!now || now < t0 || now - t0 < (hang_ms + 3000) * 1000000ULL) return;
    u64 expect = nmi_for_serial;
    if (!__atomic_compare_exchange_n(&nmi_for_serial, &expect, serial, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST)) return;
    if (watch_serial != serial) return;                /* a guarda acabou enquanto isso */
    nmi_pending = true;
    smp_send_nmi(0);
}

/* NMI no núcleo 0. false = não foi o vigia (NMI de hardware: segue para o pânico). Se foi: com g_irqoff[0] == 0 o
 * IF=0 é do próprio app e a pilha pode ser desfeita como numa exceção (as interrupções voltam ligadas). Dentro de
 * uma seção irq_save/spinlock não dá: mostra a tela de pânico com o nome do app em vez de congelar mudo. */
bool app_guard_nmi(int_frame_t *f, bool nested) {
    if (!nmi_pending) return false;
    nmi_pending = false;
    guard_t *g = cur;
    if (nested || (f->cs & 3) || !g || this_cpu() != 0) { nmi_for_serial = 0; return true; }   /* agora não: tenta de novo */
    u64 now = hpet_ns();
    if (!now || now - g->t0_ns < hang_ms * 1000000ULL) return true;
    if (g_irqoff[0] || par_busy()) {
        char msg[160];
        snprintf(msg, sizeof msg, "%s travou com as interrupções desligadas dentro de uma seção crítica do kernel", g->app->name);
        kprintf("appguard: %s (RIP=0x%llx); não dá para recuperar\n", msg, f->rip);
        extern void panic_screen(const char *msg, int_frame_t *f);
        panic_screen(msg, f);
        for (;;) { cli(); hlt_raw(); }
    }
    fault_vec = -2; fault_rip = f->rip; fault_cr2 = 0;
    if (g->if_on) f->rflags |= 0x200;
    f->rsp = ((f->rsp - 512) & ~15ULL) - 8;
    f->rip = (u64)guard_trampoline;
    return true;
}

bool app_guard_run(app_t *a, void (*fn)(void *ctx), void *ctx) {
    if (!a) { fn(ctx); return true; }
    if (is_dead(a)) return false;                     /* já caiu: nada mais dele roda até o reap */
    guard_t g;
    g.app = a;
    g.prev = cur;
    g.t0 = timer_ticks();
    g.t0_ns = hpet_ns();
    { u64 fl; __asm__ volatile("pushfq; popq %0" : "=r"(fl)); g.if_on = (fl & 0x200) != 0; }
    u64 prev_t0 = watch_t0_ns, prev_serial = watch_serial;
    if (kjmp_set(&g.jb)) {
        guard_t *me = landing;                        /* lido do global: nada de local depois do salto */
        landing = NULL;
        cur = me->prev;
        watch_t0_ns = me->prev ? me->prev->t0_ns : 0;
        watch_serial = me->prev ? ++guard_serial : 0;
        extern char __ImageBase;
        if (fault_vec < 0)
            kprintf("appguard: \"%s\" não respondeu por %llu ms%s (RIP=base+0x%llx); o app será encerrado\n",
                    me->app->name, hang_ms, fault_vec == -2 ? " com as interrupções desligadas (NMI)" : "", fault_rip - (u64)&__ImageBase);
        else
            kprintf("appguard: \"%s\" falhou (vetor %d em RIP=base+0x%llx, CR2=0x%llx); o app será encerrado\n",
                    me->app->name, fault_vec, fault_rip - (u64)&__ImageBase, fault_cr2);
        int *c = crash_count(me->app);
        if (c) (*c)++;
        if (!is_dead(me->app) && ndead < MAX_DEAD) { dead_hung[ndead] = fault_vec < 0; dead[ndead++] = me->app; }
        return false;
    }
    cur = &g;
    watch_t0_ns = g.t0_ns;
    watch_serial = ++guard_serial;
    fn(ctx);
    cur = g.prev;
    watch_t0_ns = prev_t0;
    watch_serial = prev_t0 ? prev_serial : 0;
    return true;
}

/* no topo do laço do desktop: encerra quem caiu. Do código do app só roda o on_close de cada janela (protegido):
 * é ele que solta os ponteiros que o app guarda para a janela (sem isso o próximo launch usaria memória liberada).
 * O on_quit não roda: o estado de quem caiu pode estar corrompido e não deve ser gravado. */
void app_guard_reap(void) {
    while (ndead > 0) {
        app_t *a = dead[--ndead];
        bool hung = dead_hung[ndead];
        char body[160];
        if (!strncmp(a->exec, "mac", 3)) {
            app_force_quit(a);                        /* processo de usuário: o kernel só pede para ele sair */
        } else {
            for (int i = 0; ; i++) {
                window_t *w = wm_nth(i);
                if (!w) break;
                if (w->app != a) continue;
                w->paint = NULL; w->on_close_request = NULL; w->on_key = NULL;
            }
            wm_close_app(a);                          /* on_close via app_guard_close */
            for (int i = 0; i < ndead; i++)           /* se o próprio on_close caiu, já está encerrado: um aviso só */
                if (dead[i] == a) { --ndead; dead[i] = dead[ndead]; dead_hung[i] = dead_hung[ndead]; i--; }
            a->running = false;
            int *c = crash_count(a);
            /* tick com o app fechado (alarmes etc.) que cai de novo fica desligado até reiniciar */
            if (c && *c >= 2) { a->always_tick = false; if (*c >= 3) a->tick = NULL; }
        }
        wm_damage(desktop_dock_rect());              /* a bolinha do app some do Dock */
        snprintf(body, sizeof body, hung ? "%s não respondeu por %llu s e foi encerrado. O resto do sistema continua."
                                         : "%s parou de funcionar e foi encerrado. O resto do sistema continua.", a->name, hang_ms / 1000);
        notify("App encerrado", body);
    }
}

/* ---------------------------------------------------------------- chamadas protegidas dos callbacks */
typedef struct { app_t *a; int act; } app_ctx_t;
static void do_tick(void *c) { app_ctx_t *x = c; if (x->a->tick) x->a->tick(x->a); }
static void do_launch(void *c) { app_ctx_t *x = c; if (x->a->launch) x->a->launch(x->a); }
static void do_quit(void *c) { app_ctx_t *x = c; if (x->a->on_quit) x->a->on_quit(x->a); }
static void do_menu(void *c) { app_ctx_t *x = c; if (x->a->on_menu) x->a->on_menu(x->a, x->act); }

bool app_guard_tick(app_t *a) { app_ctx_t x = { a, 0 }; return app_guard_run(a, do_tick, &x); }
bool app_guard_launch(app_t *a) { app_ctx_t x = { a, 0 }; return app_guard_run(a, do_launch, &x); }
bool app_guard_quit(app_t *a) { app_ctx_t x = { a, 0 }; return app_guard_run(a, do_quit, &x); }
bool app_guard_menu(app_t *a, int act) { app_ctx_t x = { a, act }; return app_guard_run(a, do_menu, &x); }

typedef struct { window_t *w; ui_t *u; input_event_t *e; bool r; } win_ctx_t;
static void do_paint(void *c) { win_ctx_t *x = c; if (x->w->paint) x->w->paint(x->w, x->u); }
static void do_key(void *c) { win_ctx_t *x = c; if (x->w->on_key) x->w->on_key(x->w, x->e); }
static void do_close(void *c) { win_ctx_t *x = c; if (x->w->on_close) x->w->on_close(x->w); }
static void do_close_req(void *c) { win_ctx_t *x = c; x->r = x->w->on_close_request && x->w->on_close_request(x->w); }

bool app_guard_paint(window_t *w, ui_t *u) { win_ctx_t x = { w, u, NULL, false }; return app_guard_run(w->app, do_paint, &x); }
bool app_guard_key(window_t *w, input_event_t *e) { win_ctx_t x = { w, NULL, e, false }; return app_guard_run(w->app, do_key, &x); }
bool app_guard_close(window_t *w) { win_ctx_t x = { w, NULL, NULL, false }; return app_guard_run(w->app, do_close, &x); }
/* devolve o que on_close_request devolveu; se o app caiu, false (a janela pode fechar) */
bool app_guard_close_request(window_t *w) { win_ctx_t x = { w, NULL, NULL, false }; app_guard_run(w->app, do_close_req, &x); return x.r; }

#ifdef MOS_UNIT_TESTS
/* testes: uma falha de página e um #UD dentro da guarda voltam como false e o kernel segue */
static app_t test_app = { .name = "teste-appguard", .exec = "teste-appguard" };
static void boom_gp(void *c) { (void)c; *(volatile u32 *)0xDEAD000000000000ULL = 1; }    /* não canônico: #GP */
static void boom_pf(void *c) { (void)c; *(volatile u32 *)0x00007FFFFFFF0000ULL = 1; }    /* alto e não mapeado: #PF */
static void boom_ud(void *c) { (void)c; __asm__ volatile("ud2"); }
static void boom_de(void *c) { volatile int z = 0; *(volatile int *)c = 7 / z; }
static void fine(void *c) { *(int *)c = 42; }
static void spin(void *c) { (void)c; for (;;) __asm__ volatile("pause"); }
static void x87_mess(void *c) { (void)c; u16 cw = 0x0C7F; __asm__ volatile("fldcw %0" :: "m"(cw)); __asm__ volatile("ud2"); }
static void nested(void *c) { static app_t inner = { .name = "interno", .exec = "interno" }; *(int *)c = app_guard_run(&inner, boom_ud, NULL) ? 1 : 2; }

void appguard_tests_run(int *ok, int *bad) {
#define CHK(c, m) do { if (c) { (*ok)++; kprintf("unit: ok   %s\n", m); } else { (*bad)++; kprintf("unit: FALHA %s\n", m); } } while (0)
    int v = 0;
    CHK(app_guard_run(&test_app, fine, &v) && v == 42, "appguard: chamada normal volta true");
    CHK(!app_guard_run(&test_app, boom_gp, NULL), "appguard: ponteiro não canônico (#GP) no app volta false");
    CHK(ndead == 1 && dead[0] == &test_app, "appguard: app que caiu fica marcado para encerrar");
    CHK(!app_guard_run(&test_app, fine, &v), "appguard: app marcado não roda mais");
    ndead = 0;
    CHK(!app_guard_run(&test_app, boom_pf, NULL), "appguard: falha de página no app volta false");
    ndead = 0;
    CHK(!app_guard_run(&test_app, boom_ud, NULL), "appguard: #UD no app volta false");
    ndead = 0;
    CHK(!app_guard_run(&test_app, boom_de, &v), "appguard: divisão por zero no app volta false");
    ndead = 0;
    v = 0;
    CHK(app_guard_run(&test_app, nested, &v) && v == 2, "appguard: guarda aninhada pega só o app de dentro");
    ndead = 0;
    u64 keep = hang_ms;
    hang_ms = 300;                                    /* no teste, 300 ms em vez de 10 s */
    u64 t0 = timer_ticks();
    CHK(!app_guard_run(&test_app, spin, NULL), "appguard: laço infinito no app é interrompido pelo watchdog");
    CHK(timer_ticks() - t0 >= 300 && timer_ticks() - t0 < 2000, "appguard: o watchdog age depois do limite, não antes");
    CHK(ndead == 1 && dead_hung[0], "appguard: o motivo fica registrado como \"não respondeu\"");
    hang_ms = keep;
    ndead = 0;
    {   /* a palavra de controle do x87 volta ao valor de antes do app (ABI Win64) */
        u16 before, after;
        __asm__ volatile("fnstcw %0" : "=m"(before));
        CHK(!app_guard_run(&test_app, x87_mess, NULL), "appguard: app que mexe no x87 e cai volta false");
        __asm__ volatile("fnstcw %0" : "=m"(after));
        CHK(before == after, "appguard: palavra de controle do x87 restaurada");
        ndead = 0;
    }
    CHK(cur == NULL, "appguard: nenhuma guarda fica pendurada");
    CHK(app_guard_crashes(&test_app) == 6, "appguard: contagem de falhas por app");
    ndead = 0;
    memset(track, 0, sizeof track);
#undef CHK
}
#endif
