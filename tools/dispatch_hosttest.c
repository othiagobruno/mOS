/*
 * Teste de host da libdispatch do mOS (userland/libsystem/dispatch*.c) no Linux.
 *   sh tools/dispatch_hosttest.sh
 * Os blocos são montados à mão na ABI de Blocks (sem -fblocks): {isa, flags, reserved, invoke, ...capturas}.
 * Sai com o número de falhas.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <pthread.h>
#include <sys/wait.h>
#include <time.h>

/* classes ObjC que os objetos usam no isa */
#define C(n) char n[16] __asm__("OBJC_CLASS_$_" #n)
C(OS_dispatch_queue_main); C(OS_dispatch_queue_global); C(OS_dispatch_queue_serial); C(OS_dispatch_queue_concurrent);
C(OS_dispatch_group); C(OS_dispatch_semaphore); C(OS_dispatch_queue_attr); C(OS_dispatch_workloop);
C(OS_dispatch_source); C(OS_dispatch_data); C(OS_dispatch_io);
void *_Block_copy(const void *b) { return (void *)b; }
void _Block_release(const void *b) { (void)b; }

typedef unsigned long long u64;
typedef void (*fn_t)(void *);
void *dispatch_queue_create(const char *, void *);
void *dispatch_queue_create_with_target(const char *, void *, void *);
void *dispatch_get_global_queue(long, unsigned long);
void *dispatch_get_main_queue(void);
void dispatch_async_f(void *, void *, fn_t);
void dispatch_sync_f(void *, void *, fn_t);
void dispatch_barrier_async_f(void *, void *, fn_t);
void dispatch_barrier_sync_f(void *, void *, fn_t);
void dispatch_async(void *, void *);
void dispatch_after_f(u64, void *, void *, fn_t);
u64 dispatch_time(u64, long long);
void dispatch_suspend(void *); void dispatch_resume(void *); void dispatch_activate(void *);
void *dispatch_queue_attr_make_initially_inactive(void *);
extern char _dispatch_queue_attr_concurrent[];
void *dispatch_group_create(void); void dispatch_group_enter(void *); void dispatch_group_leave(void *);
long dispatch_group_wait(void *, u64); void dispatch_group_async_f(void *, void *, void *, fn_t); void dispatch_group_notify_f(void *, void *, void *, fn_t);
void *dispatch_semaphore_create(long); long dispatch_semaphore_signal(void *); long dispatch_semaphore_wait(void *, u64);
void dispatch_once_f(long *, void *, fn_t);
void dispatch_apply_f(size_t, void *, void *, void (*)(void *, size_t));
void dispatch_queue_set_specific(void *, const void *, void *, fn_t); void *dispatch_get_specific(const void *);
void dispatch_release(void *); void dispatch_retain(void *);
void dispatch_set_context(void *, void *); void dispatch_set_finalizer_f(void *, fn_t);
int cx_dispatch_drain_main(void);
void *dispatch_source_create(const void *, unsigned long, unsigned long, void *);
void dispatch_source_set_event_handler_f(void *, fn_t); void dispatch_source_set_cancel_handler_f(void *, fn_t);
void dispatch_source_set_timer(void *, u64, u64, u64); void dispatch_source_cancel(void *);
unsigned long dispatch_source_get_data(void *); void dispatch_source_merge_data(void *, unsigned long);
extern char _dispatch_source_type_timer[], _dispatch_source_type_data_add[], _dispatch_source_type_read[],
    _dispatch_source_type_signal[], _dispatch_source_type_proc[], _dispatch_source_type_vnode[], _dispatch_source_type_data_or[];
void *dispatch_data_create(const void *, size_t, void *, void *); size_t dispatch_data_get_size(void *);
void *dispatch_data_create_concat(void *, void *); void *dispatch_data_create_subrange(void *, size_t, size_t);
void *dispatch_data_create_map(void *, const void **, size_t *);
extern char _dispatch_data_empty[];
void *dispatch_io_create(unsigned long, int, void *, void *);
void dispatch_io_read(void *, long long, size_t, void *, void *); void dispatch_io_close(void *, unsigned long);
void dispatch_io_write(void *, long long, void *, void *, void *);
void *dispatch_block_create(unsigned long, void *); long dispatch_block_wait(void *, u64); void dispatch_block_cancel(void *);
const char *dispatch_queue_get_label(void *);
void dispatch_async_swift_job(void *, void *, unsigned);
void *dispatch_workloop_create(const char *);

static int fails, total;
#define CHECK(c, ...) do { total++; if (c) printf("  ok    "); else { fails++; printf("  FALHA "); } printf(__VA_ARGS__); printf("\n"); fflush(stdout); } while (0)
#define NSEC 1000000000ull
static u64 now_ns(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (u64)t.tv_sec * NSEC + (u64)t.tv_nsec; }
static void msleep(int ms) { usleep((unsigned)ms * 1000); }

/* ---- bloco à mão */
typedef struct blk { void *isa; int flags, reserved; void (*invoke)(void *); volatile int *p; int v; } blk_t;
static void blk_inc(void *b) { __sync_fetch_and_add(((blk_t *)b)->p, ((blk_t *)b)->v); }

/* ---- serial: ordem */
static int order_buf[2000]; static volatile int order_n;
static void rec(void *c) { int i = (int)(long)c; order_buf[__sync_fetch_and_add(&order_n, 1)] = i; }
/* ---- concorrência */
static volatile int conc_now, conc_max;
static void conc_job(void *c) { (void)c; int n = __sync_add_and_fetch(&conc_now, 1); int m; while ((m = conc_max) < n && !__sync_bool_compare_and_swap(&conc_max, m, n)); msleep(30); __sync_fetch_and_sub(&conc_now, 1); }
static volatile int bar_seen_other, bar_running;
static void bar_job(void *c) { (void)c; bar_running = 1; msleep(20); if (conc_now) bar_seen_other = 1; bar_running = 0; }
static volatile int bar_overlap;
static void conc_job2(void *c) { (void)c; __sync_fetch_and_add(&conc_now, 1); if (bar_running) bar_overlap = 1; msleep(5); __sync_fetch_and_sub(&conc_now, 1); }
static volatile int cnt;
static void inc(void *c) { (void)c; __sync_fetch_and_add(&cnt, 1); }
static void slow_inc(void *c) { (void)c; msleep(5); __sync_fetch_and_add(&cnt, 1); }
static volatile long once_runs; static long once_pred;
static void once_fn(void *c) { (void)c; msleep(20); __sync_fetch_and_add(&once_runs, 1); }
static void *once_thr(void *a) { (void)a; dispatch_once_f(&once_pred, 0, once_fn); return 0; }
static volatile long apply_sum; static volatile int apply_threads[64]; static volatile int apply_nt;
static void apply_fn(void *c, size_t i) { (void)c; __sync_fetch_and_add(&apply_sum, (long)i); }
static char key1;
static void *spec_got;
static void spec_job(void *c) { (void)c; spec_got = dispatch_get_specific(&key1); }
static volatile int fin_called;
static void fin(void *c) { (void)c; fin_called = 1; }
static volatile int main_ran_on_main;
static int main_tid;
#include <sys/syscall.h>
static void main_job(void *c) { (void)c; main_ran_on_main = (int)syscall(SYS_gettid) == main_tid; }
static void *bg_sync_main(void *a) { dispatch_sync_f(dispatch_get_main_queue(), 0, main_job); *(volatile int *)a = 1; return 0; }
static volatile int tfires; static void *tsrc; static volatile unsigned long tdata_sum;
static void tick(void *c) { (void)c; tdata_sum += dispatch_source_get_data(tsrc); tfires++; }
static volatile int cancel_called; static void on_cancel(void *c) { (void)c; cancel_called = 1; }
static void *dsrc; static volatile unsigned long dsum; static volatile int dcalls;
size_t dispatch_source_get_extended_data(void *, void *, size_t);
static volatile int dext_ok = 1;
static void dhandler(void *c) {
    (void)c;
    msleep(20);
    unsigned long d = dispatch_source_get_data(dsrc), ext[3] = {7, 7, 7};
    /* {data, status}: copia no máximo 16 bytes e devolve quantos; com size 8 só o data */
    if (dispatch_source_get_extended_data(dsrc, ext, sizeof ext) != 16 || ext[0] != d || ext[1] != 0 || ext[2] != 7) dext_ok = 0;
    unsigned long one = 0;
    if (dispatch_source_get_extended_data(dsrc, &one, sizeof one) != 8 || one != d) dext_ok = 0;
    dsum += d;
    dcalls++;
}
static void *rsrc; static volatile unsigned long rgot; static int rfd;
static void rhandler(void *c) { (void)c; char b[64]; long n = read(rfd, b, sizeof b); if (n > 0) rgot += (unsigned long)n; }
static void *ssrc; static volatile unsigned long sgot; static void shandler(void *c) { (void)c; sgot += dispatch_source_get_data(ssrc); }
static void *psrc; static volatile unsigned long pgot; static void phandler(void *c) { (void)c; pgot = dispatch_source_get_data(psrc); }
static void *vsrc; static volatile unsigned long vgot; static void vhandler(void *c) { (void)c; vgot |= dispatch_source_get_data(vsrc); }
static volatile int swift_ran;
static void swift_invoke(void *job, void *ctx, unsigned flags) { (void)job; (void)ctx; (void)flags; swift_ran = 1; }
static void *swift_md[8] = {0, 0, 0, 0, 0, 0, (void *)swift_invoke, 0};
static void *swift_job[4] = {&swift_md[0], 0, 0, 0};
/* io: handler (bool done, data, int err) */
typedef struct { void *isa; int flags, reserved; void (*invoke)(void *, _Bool, void *, int); } ioblk_t;
static volatile size_t io_bytes; static volatile int io_done, io_err, io_parts;
static void io_h(void *b, _Bool done, void *data, int err) { (void)b; io_bytes += dispatch_data_get_size(data); io_parts++; if (done) { io_err = err; io_done = 1; } }
static volatile int wdone;
static void io_w(void *b, _Bool done, void *data, int err) { (void)b; (void)data; if (done) { io_err = err; wdone = 1; } }
static volatile int wl_order[8]; static volatile int wl_n;
static void wl_rec(void *c) { wl_order[__sync_fetch_and_add(&wl_n, 1)] = (int)(long)c; }

static void wait_until(volatile int *v, int want, int ms) { u64 end = now_ns() + (u64)ms * 1000000ull; while (*v < want && now_ns() < end) msleep(1); }

int main(void) {
    main_tid = (int)syscall(SYS_gettid);
    printf("dispatch_hosttest: libdispatch do mOS no Linux\n");

    /* serial: 1000 itens em ordem, vindos de async e no fim um sync */
    void *s = dispatch_queue_create("t.serial", 0);
    CHECK(!strcmp(dispatch_queue_get_label(s), "t.serial"), "label");
    for (long i = 0; i < 1000; i++) dispatch_async_f(s, (void *)i, rec);
    dispatch_sync_f(s, (void *)1000L, rec);
    int ok = order_n == 1001;
    for (int i = 0; ok && i <= 1000; i++) if (order_buf[i] != i) ok = 0;
    CHECK(ok, "serial: 1001 itens em ordem (async x1000 + sync)");

    /* concorrente: roda em paralelo */
    void *c = dispatch_queue_create("t.conc", _dispatch_queue_attr_concurrent);
    void *g = dispatch_group_create();
    for (int i = 0; i < 8; i++) dispatch_group_async_f(g, c, 0, conc_job);
    CHECK(dispatch_group_wait(g, ~0ull) == 0, "group_wait volta quando o grupo esvazia");
    CHECK(conc_max >= 2, "concorrente: %d ao mesmo tempo", conc_max);

    /* barreira: sozinha */
    conc_now = 0;
    for (int i = 0; i < 6; i++) dispatch_group_async_f(g, c, 0, conc_job2);
    dispatch_barrier_async_f(c, 0, bar_job);
    for (int i = 0; i < 6; i++) dispatch_group_async_f(g, c, 0, conc_job2);
    dispatch_barrier_sync_f(c, 0, inc);
    dispatch_group_wait(g, ~0ull);
    CHECK(!bar_seen_other && !bar_overlap, "barreira roda sozinha na fila concorrente");

    /* sync dentro de async na mesma cadeia de outra fila */
    cnt = 0;
    void *t = dispatch_queue_create("t.target", 0);
    void *a1 = dispatch_queue_create_with_target("t.a1", _dispatch_queue_attr_concurrent, t);
    void *a2 = dispatch_queue_create_with_target("t.a2", _dispatch_queue_attr_concurrent, t);
    conc_now = conc_max = 0;
    for (int i = 0; i < 4; i++) { dispatch_group_async_f(g, a1, 0, conc_job); dispatch_group_async_f(g, a2, 0, conc_job); }
    dispatch_group_wait(g, ~0ull);
    CHECK(conc_max == 1, "destino serial serializa duas filas concorrentes (máx %d)", conc_max);

    /* suspensão */
    cnt = 0;
    void *sq = dispatch_queue_create("t.susp", 0);
    dispatch_suspend(sq);
    dispatch_async_f(sq, 0, inc);
    msleep(30);
    int before = cnt;
    dispatch_resume(sq);
    dispatch_sync_f(sq, 0, inc);
    CHECK(before == 0 && cnt == 2, "suspend segura, resume libera (%d, %d)", before, cnt);
    cnt = 0;
    void *iq = dispatch_queue_create("t.inactive", dispatch_queue_attr_make_initially_inactive(0));
    dispatch_async_f(iq, 0, inc);
    msleep(20);
    before = cnt;
    dispatch_activate(iq);
    dispatch_sync_f(iq, 0, inc);
    CHECK(before == 0 && cnt == 2, "fila inativa só roda depois de activate");

    /* after */
    cnt = 0;
    u64 t0 = now_ns();
    dispatch_after_f(dispatch_time(0, 50 * 1000000LL), s, 0, inc);
    wait_until(&cnt, 1, 1000);
    u64 el = (now_ns() - t0) / 1000000ull;
    CHECK(cnt == 1 && el >= 49 && el < 300, "dispatch_after em ~50 ms (%llu ms)", el);

    /* relógio contínuo (bit 63), como o _Concurrency manda os prazos do Task.sleep */
    cnt = 0;
    t0 = now_ns();
    u64 mono = (t0 + 50 * 1000000ull) | (1ull << 63);
    dispatch_after_f(mono, s, 0, inc);
    wait_until(&cnt, 1, 1000);
    el = (now_ns() - t0) / 1000000ull;
    CHECK(cnt == 1 && el >= 49 && el < 300, "dispatch_after no relógio contínuo em ~50 ms (%llu ms)", el);
    u64 m2 = dispatch_time(mono, 10 * 1000000LL);
    CHECK((m2 >> 63) && !((m2 >> 62) & 1) && (m2 & ~(1ull << 63)) == (mono & ~(1ull << 63)) + 10000000ull,
          "dispatch_time soma no relógio contínuo e mantém o bit 63");
    u64 dispatch_walltime(const void *, long long);
    u64 w = dispatch_walltime(0, 0), w2 = dispatch_time(w, 1000000000LL);
    CHECK(((w >> 62) & 3) == 3 && -(long long)w2 == -(long long)w + 1000000000LL, "parede: bits 63 e 62, dispatch_time soma 1 s");

    /* grupo: notify e enter/leave e prazo */
    cnt = 0;
    dispatch_group_enter(g);
    dispatch_group_notify_f(g, s, 0, inc);
    CHECK(dispatch_group_wait(g, dispatch_time(0, 20 * 1000000LL)) != 0, "group_wait com prazo vence");
    dispatch_group_leave(g);
    dispatch_sync_f(s, 0, (fn_t)msleep == 0 ? inc : inc);
    wait_until(&cnt, 2, 500);
    CHECK(cnt >= 1, "group_notify depois do leave");

    /* semáforo */
    void *sem = dispatch_semaphore_create(0);
    t0 = now_ns();
    CHECK(dispatch_semaphore_wait(sem, dispatch_time(0, 30 * 1000000LL)) != 0, "semáforo: prazo");
    CHECK((now_ns() - t0) >= 29000000ull, "semáforo esperou o prazo");
    dispatch_semaphore_signal(sem);
    CHECK(dispatch_semaphore_wait(sem, ~0ull) == 0, "semáforo: signal antes do wait");

    /* once com várias threads */
    pthread_t th[8];
    for (int i = 0; i < 8; i++) pthread_create(&th[i], 0, once_thr, 0);
    for (int i = 0; i < 8; i++) pthread_join(th[i], 0);
    CHECK(once_runs == 1 && once_pred == ~0l, "dispatch_once uma vez com 8 threads");

    /* apply */
    dispatch_apply_f(10000, dispatch_get_global_queue(0, 0), 0, apply_fn);
    CHECK(apply_sum == 10000L * 9999 / 2, "dispatch_apply soma %ld", apply_sum);
    apply_sum = 0;
    dispatch_apply_f(100, s, 0, apply_fn);
    CHECK(apply_sum == 4950, "dispatch_apply em fila serial");

    /* specific pela cadeia de destino */
    dispatch_queue_set_specific(t, &key1, (void *)0x1234, 0);
    dispatch_sync_f(a1, 0, spec_job);
    CHECK(spec_got == (void *)0x1234, "get_specific acha o valor do destino");
    spec_got = 0;
    dispatch_async_f(a2, 0, spec_job);
    dispatch_barrier_sync_f(a2, 0, inc);
    CHECK(spec_got == (void *)0x1234, "get_specific num item assíncrono");

    /* finalizador */
    void *fq = dispatch_queue_create("t.fin", 0);
    dispatch_set_context(fq, (void *)1);
    dispatch_set_finalizer_f(fq, fin);
    dispatch_retain(fq);
    dispatch_release(fq);
    CHECK(!fin_called, "retain/release não libera");
    dispatch_release(fq);
    msleep(30);
    CHECK(fin_called, "finalizador no último release");

    /* fila principal: sync de outra thread, executada pela thread principal */
    volatile int bg_done = 0;
    pthread_t bt;
    pthread_create(&bt, 0, bg_sync_main, (void *)&bg_done);
    for (int i = 0; i < 500 && !bg_done; i++) { cx_dispatch_drain_main(); msleep(1); }
    pthread_join(bt, 0);
    CHECK(bg_done && main_ran_on_main, "dispatch_sync(main) de outra thread roda na thread principal");
    cnt = 0;
    dispatch_async_f(dispatch_get_main_queue(), 0, inc);
    CHECK(cnt == 0, "async(main) não roda na hora");
    cx_dispatch_drain_main();
    CHECK(cnt == 1, "async(main) roda no escoamento da fila principal");

    /* bloco */
    blk_t b1 = {0, 0, 0, blk_inc, &cnt, 10};
    cnt = 0;
    void *db = dispatch_block_create(0, &b1);
    dispatch_async(s, db);
    CHECK(dispatch_block_wait(db, ~0ull) == 0 && cnt == 10, "dispatch_block_wait espera o bloco");
    void *db2 = dispatch_block_create(0, &b1);
    dispatch_block_cancel(db2);
    dispatch_async(s, db2);
    dispatch_block_wait(db2, ~0ull);
    CHECK(cnt == 10, "bloco cancelado não roda");
    void *db3 = dispatch_block_create(1 /* BARRIER */, &b1);
    conc_now = 0; bar_overlap = 0;
    for (int i = 0; i < 4; i++) dispatch_async_f(c, 0, conc_job2);
    dispatch_async(c, db3);
    dispatch_block_wait(db3, ~0ull);
    dispatch_barrier_sync_f(c, 0, inc);
    CHECK(cnt == 21, "bloco com DISPATCH_BLOCK_BARRIER");

    /* job do Swift */
    dispatch_async_swift_job(dispatch_get_global_queue(0x19, 0), swift_job, 0x19);
    wait_until(&swift_ran, 1, 500);
    CHECK(swift_ran, "dispatch_async_swift_job chama o invoke da metadata");

    /* workloop: QoS mais alta primeiro */
    void *wl = dispatch_workloop_create("t.wl");
    CHECK(wl != 0, "workloop criado");
    dispatch_suspend(wl);
    dispatch_async_f(dispatch_queue_create_with_target("t.lo", dispatch_queue_attr_make_initially_inactive(0), wl), 0, wl_rec);
    dispatch_async_f(wl, (void *)1, wl_rec);
    dispatch_async_f(wl, (void *)2, wl_rec);
    dispatch_resume(wl);
    dispatch_sync_f(wl, (void *)3, wl_rec);
    CHECK(wl_n == 3 && wl_order[0] == 1 && wl_order[1] == 2, "workloop em ordem dentro da QoS");

    /* fonte de timer: repete; cancel chama o handler de cancelamento */
    tsrc = dispatch_source_create(_dispatch_source_type_timer, 0, 0, s);
    dispatch_source_set_event_handler_f(tsrc, tick);
    dispatch_source_set_cancel_handler_f(tsrc, on_cancel);
    dispatch_source_set_timer(tsrc, dispatch_time(0, 10 * 1000000LL), 10 * 1000000ull, 0);
    dispatch_resume(tsrc);
    wait_until(&tfires, 5, 1000);
    CHECK(tfires >= 5, "timer repetido (%d disparos)", tfires);
    dispatch_source_cancel(tsrc);
    wait_until(&cancel_called, 1, 500);
    int tf = tfires;
    msleep(40);
    CHECK(cancel_called && tfires == tf, "cancel para o timer e chama o cancel handler");

    /* data_add: junta enquanto o handler está ocupado */
    dsrc = dispatch_source_create(_dispatch_source_type_data_add, 0, 0, s);
    dispatch_source_set_event_handler_f(dsrc, dhandler);
    dispatch_activate(dsrc);
    for (int i = 0; i < 100; i++) dispatch_source_merge_data(dsrc, 1);
    u64 end = now_ns() + NSEC;
    while (dsum < 100 && now_ns() < end) msleep(2);
    CHECK(dsum == 100 && dcalls < 100, "data_add soma 100 em %d chamadas", dcalls);
    CHECK(dext_ok, "get_extended_data: {data, status 0}, tamanho limitado a 16");

    /* _dispatch_main_queue_callback_4CF: quem recebe a mensagem da porta roda a fila principal */
    void _dispatch_main_queue_callback_4CF(void *);
    unsigned _dispatch_get_main_queue_port_4CF(void);
    main_ran_on_main = 0;
    dispatch_async_f(dispatch_get_main_queue(), 0, main_job);
    _dispatch_main_queue_callback_4CF(0);
    CHECK(main_ran_on_main, "_dispatch_main_queue_callback_4CF roda a fila principal na thread principal");
    CHECK(_dispatch_get_main_queue_port_4CF() == 0, "porta da fila principal: 0 no host (sem Mach)");

    /* read: pipe */
    int pp[2];
    pipe(pp);
    rfd = pp[0];
    rsrc = dispatch_source_create(_dispatch_source_type_read, (unsigned long)pp[0], 0, s);
    dispatch_source_set_event_handler_f(rsrc, rhandler);
    dispatch_resume(rsrc);
    write(pp[1], "abcdef", 6);
    end = now_ns() + NSEC;
    while (rgot < 6 && now_ns() < end) msleep(2);
    CHECK(rgot == 6, "fonte de leitura no pipe (%lu bytes)", rgot);
    dispatch_source_cancel(rsrc);

    /* sinal */
    signal(SIGUSR1, SIG_IGN);
    ssrc = dispatch_source_create(_dispatch_source_type_signal, SIGUSR1, 0, s);
    dispatch_source_set_event_handler_f(ssrc, shandler);
    dispatch_resume(ssrc);
    msleep(10);
    kill(getpid(), SIGUSR1);
    end = now_ns() + NSEC;
    while (!sgot && now_ns() < end) msleep(2);
    CHECK(sgot >= 1, "fonte de sinal (%lu)", sgot);

    /* processo */
    pid_t kid = fork();
    if (!kid) { usleep(30000); _exit(0); }
    psrc = dispatch_source_create(_dispatch_source_type_proc, (unsigned long)kid, 0x80000000ul, s);
    dispatch_source_set_event_handler_f(psrc, phandler);
    dispatch_resume(psrc);
    waitpid(kid, 0, 0);
    end = now_ns() + NSEC;
    while (!pgot && now_ns() < end) msleep(2);
    CHECK(pgot == 0x80000000ul, "fonte de processo: saída");

    /* vnode */
    char path[] = "/tmp/dispatch_hosttest_XXXXXX";
    int vf = mkstemp(path);
    vsrc = dispatch_source_create(_dispatch_source_type_vnode, (unsigned long)vf, 0x2 | 0x4 | 0x1, s);
    dispatch_source_set_event_handler_f(vsrc, vhandler);
    dispatch_resume(vsrc);
    msleep(20);
    int wf = open(path, O_WRONLY | O_APPEND);
    write(wf, "x", 1);
    close(wf);
    end = now_ns() + NSEC;
    while (!(vgot & 0x4) && now_ns() < end) msleep(2);
    unlink(path);
    end = now_ns() + NSEC;
    while (!(vgot & 0x1) && now_ns() < end) msleep(2);
    CHECK((vgot & 0x6) == 0x6 && (vgot & 0x1), "fonte vnode: write, extend e delete (0x%lx)", vgot);

    /* dados */
    void *d1 = dispatch_data_create("abc", 3, 0, 0), *d2 = dispatch_data_create("defg", 4, 0, 0);
    void *d3 = dispatch_data_create_concat(d1, d2);
    void *d4 = dispatch_data_create_subrange(d3, 2, 3);
    const void *mb; size_t mn;
    void *mp = dispatch_data_create_map(d4, &mb, &mn);
    CHECK(dispatch_data_get_size(d3) == 7 && mn == 3 && !memcmp(mb, "cde", 3), "data: concat, subrange e map");
    CHECK(dispatch_data_create(0, 0, 0, 0) == (void *)_dispatch_data_empty, "data vazio é dispatch_data_empty");
    dispatch_release(mp); dispatch_release(d4); dispatch_release(d3); dispatch_release(d2); dispatch_release(d1);

    /* io: grava e lê de volta com marca alta */
    char p2[] = "/tmp/dispatch_hosttest_io_XXXXXX";
    int iof = mkstemp(p2);
    char *big = malloc(300000);
    memset(big, 'z', 300000);
    void *bd = dispatch_data_create(big, 300000, 0, 0);
    void *ch = dispatch_io_create(1 /* RANDOM */, iof, s, 0);
    ioblk_t wb = {0, 0, 0, io_w};
    dispatch_io_write(ch, 0, bd, s, &wb);
    wait_until(&wdone, 1, 2000);
    CHECK(wdone && io_err == 0, "dispatch_io_write gravou");
    void dispatch_io_set_high_water(void *, size_t);
    dispatch_io_set_high_water(ch, 100000);
    ioblk_t rb = {0, 0, 0, io_h};
    dispatch_io_read(ch, 0, (size_t)-1, s, &rb);
    wait_until(&io_done, 1, 2000);
    CHECK(io_done && io_bytes == 300000 && io_parts >= 3, "dispatch_io_read leu %zu bytes em %d partes", (size_t)io_bytes, io_parts);
    dispatch_io_close(ch, 0);
    unlink(p2);

    printf("dispatch_hosttest: %d de %d ok\n", total - fails, total);
    return fails;
}
