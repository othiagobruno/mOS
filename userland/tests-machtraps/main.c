/* mOS — traps Mach de tempo e de portas: mk_timer_*, mach_wait_until, clock_sleep, mach_reply_port,
 * mach_port_destroy, mach_port_get_attributes, swtch_pri, mach_generate_activity_id.
 * Valores esperados: os do XNU (mk_timer.c, clock_oldops.c, mach_port.c), que o Darling roda no darlingserver.
 * O mesmo binário roda no Darling, no macOS e no mOS (tools/test-machtraps.sh). */
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/clock.h>
#include <mach/mach_traps.h>

extern mach_port_name_t mk_timer_create(void);
extern kern_return_t mk_timer_destroy(mach_port_name_t);
extern kern_return_t mk_timer_arm(mach_port_name_t, uint64_t);
extern kern_return_t mk_timer_cancel(mach_port_name_t, uint64_t *);
extern mach_port_t thread_get_special_reply_port(void);
extern mach_port_name_t mach_reply_port(void);
extern kern_return_t mach_generate_activity_id(mach_port_t, int, uint64_t *);

static int fails, total;
#define EXPECT(got, want, what) do { long long g_ = (long long)(got), w_ = (long long)(want); total++; \
    if (g_ == w_) printf("  ok  %s = %lld\n", what, g_); \
    else { fails++; printf("  FALHOU  %s = %lld (esperado %lld)\n", what, g_, w_); } } while (0)

static long now_ms(void) { struct timeval tv; gettimeofday(&tv, 0); return tv.tv_sec * 1000L + tv.tv_usec / 1000; }
static uint64_t abs_in_ms(long ms) {
    mach_timebase_info_data_t tb; mach_timebase_info(&tb);
    return mach_absolute_time() + (uint64_t)ms * 1000000ull * tb.denom / tb.numer;
}
typedef struct { mach_msg_header_t h; uint64_t unused[3]; mach_msg_trailer_t t; } expire_rcv_t;
static kern_return_t rcv(mach_port_t p, expire_rcv_t *m, unsigned timeout) {
    memset(m, 0x5A, sizeof *m);
    return mach_msg(&m->h, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, sizeof *m, p, timeout, MACH_PORT_NULL);
}
static int has_receive(mach_port_t p) {
    mach_port_type_t t = 0;
    return mach_port_type(mach_task_self(), p, &t) == KERN_SUCCESS && (t & MACH_PORT_TYPE_RECEIVE) != 0;
}
static unsigned msgcount(mach_port_t p) {
    mach_port_status_t st; mach_msg_type_number_t n = MACH_PORT_RECEIVE_STATUS_COUNT;
    return mach_port_get_attributes(mach_task_self(), p, MACH_PORT_RECEIVE_STATUS, (mach_port_info_t)&st, &n) ? 999 : st.mps_msgcount;
}

int main(void) {
    setbuf(stdout, 0);
    task_t me = mach_task_self();
    expire_rcv_t m;
    long t0;

    printf("mk_timer\n");
    mach_port_t t = mk_timer_create();
    EXPECT(t != 0, 1, "create devolve um nome");
    EXPECT(has_receive(t), 1, "o timer é um RECEIVE");
    t0 = now_ms();
    EXPECT(mk_timer_arm(t, abs_in_ms(60)), KERN_SUCCESS, "arm em 60 ms");
    EXPECT(rcv(t, &m, 2000), MACH_MSG_SUCCESS, "chegou a expiração");
    EXPECT(now_ms() - t0 >= 50, 1, "não chegou antes do prazo");
    EXPECT(m.h.msgh_size, 48, "msgh_size");
    EXPECT(m.h.msgh_id, 0, "msgh_id");
    EXPECT(m.h.msgh_local_port, t, "msgh_local_port é o timer");
    EXPECT(m.h.msgh_remote_port, 0, "msgh_remote_port nulo");
    EXPECT(MACH_MSGH_BITS_REMOTE(m.h.msgh_bits), 0, "bits remotos");
    EXPECT(m.unused[0] | m.unused[1] | m.unused[2], 0, "unused zerado");
    EXPECT(mk_timer_arm(t, 0), KERN_SUCCESS, "arm no passado");
    EXPECT(rcv(t, &m, 1000), MACH_MSG_SUCCESS, "prazo no passado dispara na hora");

    uint64_t far = abs_in_ms(10000), got = 1;
    EXPECT(mk_timer_arm(t, far), KERN_SUCCESS, "arm em 10 s");
    EXPECT(mk_timer_cancel(t, &got), KERN_SUCCESS, "cancel armado");
    EXPECT(got == far, 1, "cancel devolve o prazo armado");
    EXPECT(rcv(t, &m, 200), MACH_RCV_TIMED_OUT, "cancelado não dispara");
    got = 1;
    EXPECT(mk_timer_cancel(t, &got), KERN_SUCCESS, "cancel sem estar armado");
    EXPECT(got, 0, "sem prazo armado devolve 0");

    EXPECT(mk_timer_arm(t, abs_in_ms(10000)), KERN_SUCCESS, "arm em 10 s de novo");
    EXPECT(mk_timer_arm(t, abs_in_ms(50)), KERN_SUCCESS, "rearm em 50 ms troca o prazo");
    EXPECT(rcv(t, &m, 2000), MACH_MSG_SUCCESS, "o prazo novo vale");
    EXPECT(rcv(t, &m, 200), MACH_RCV_TIMED_OUT, "o prazo antigo sumiu");

    mk_timer_arm(t, 0);
    t0 = now_ms(); while (now_ms() - t0 < 150) swtch_pri(0);
    mk_timer_arm(t, 0);
    t0 = now_ms(); while (now_ms() - t0 < 150) swtch_pri(0);
    EXPECT(msgcount(t), 1, "uma expiração na fila de cada vez");
    rcv(t, &m, 100);

    mach_port_t plain = MACH_PORT_NULL;
    mach_port_allocate(me, MACH_PORT_RIGHT_RECEIVE, &plain);
    EXPECT(mk_timer_arm(0x7ff0ff03, 0), KERN_INVALID_NAME, "arm num nome que não existe");
    EXPECT(mk_timer_arm(plain, 0), KERN_INVALID_ARGUMENT, "arm num RECEIVE que não é timer");
    EXPECT(mk_timer_arm(me, 0), KERN_INVALID_RIGHT, "arm na porta da task (só SEND)");
    EXPECT(mk_timer_destroy(plain), KERN_INVALID_ARGUMENT, "destroy num RECEIVE que não é timer");
    EXPECT(mk_timer_destroy(t), KERN_SUCCESS, "destroy");
    EXPECT(has_receive(t), 0, "destroy some com a porta");
    EXPECT(mk_timer_arm(t, 0), KERN_INVALID_NAME, "arm depois do destroy");

    printf("mach_wait_until e clock_sleep\n");
    t0 = now_ms();
    EXPECT(mach_wait_until(abs_in_ms(60)), KERN_SUCCESS, "mach_wait_until 60 ms");
    EXPECT(now_ms() - t0 >= 50, 1, "mach_wait_until esperou");
    t0 = now_ms();
    EXPECT(mach_wait_until(0), KERN_SUCCESS, "mach_wait_until no passado");
    EXPECT(now_ms() - t0 < 50, 1, "no passado volta na hora");
    clock_serv_t sys = 0, cal = 0;
    host_get_clock_service(mach_host_self(), SYSTEM_CLOCK, &sys);
    host_get_clock_service(mach_host_self(), CALENDAR_CLOCK, &cal);
    mach_timespec_t w = {0, 0}, rel = {0, 60000000};
    t0 = now_ms();
    EXPECT(clock_sleep(sys, TIME_RELATIVE, rel, &w), KERN_SUCCESS, "clock_sleep relativo 60 ms");
    EXPECT(now_ms() - t0 >= 50, 1, "clock_sleep esperou");
    EXPECT(w.tv_sec != 0 || w.tv_nsec != 0, 1, "wakeup_time preenchido");
    EXPECT(clock_sleep(MACH_PORT_NULL, TIME_RELATIVE, rel, &w), KERN_SUCCESS, "clock_sleep com relógio nulo");
    EXPECT(clock_sleep(cal, TIME_RELATIVE, rel, &w), KERN_FAILURE, "clock_sleep no CALENDAR_CLOCK");
    mach_timespec_t bad = {0, 1000000000};
    EXPECT(clock_sleep(sys, TIME_RELATIVE, bad, &w), KERN_INVALID_VALUE, "clock_sleep nsec 1e9");
    EXPECT(clock_sleep(sys, 7, rel, &w), KERN_INVALID_VALUE, "clock_sleep tipo inválido");
    mach_timespec_t past = {0, 0};
    t0 = now_ms();
    EXPECT(clock_sleep(sys, TIME_ABSOLUTE, past, &w), KERN_SUCCESS, "clock_sleep absoluto no passado");
    EXPECT(now_ms() - t0 < 50, 1, "no passado volta na hora");

    printf("portas\n");
    mach_port_t r1 = mach_reply_port(), r2 = mach_reply_port();
    EXPECT(r1 && r2 && r1 != r2, 1, "mach_reply_port dá um nome novo a cada chamada");
    EXPECT(has_receive(r1), 1, "mach_reply_port é RECEIVE");
    mach_port_t sp = thread_get_special_reply_port();
    EXPECT(sp != 0 && has_receive(sp), 1, "thread_get_special_reply_port é RECEIVE");
    EXPECT(mach_port_destroy(me, r1), KERN_SUCCESS, "mach_port_destroy");
    EXPECT(has_receive(r1), 0, "mach_port_destroy some com o RECEIVE");
    EXPECT(mach_port_destroy(me, r1), KERN_INVALID_NAME, "mach_port_destroy de novo");

    printf("mach_port_get_attributes\n");
    mach_port_limits_t lim = {0};
    mach_msg_type_number_t n = MACH_PORT_LIMITS_INFO_COUNT;
    EXPECT(mach_port_get_attributes(me, r2, MACH_PORT_LIMITS_INFO, (mach_port_info_t)&lim, &n), KERN_SUCCESS, "LIMITS_INFO");
    EXPECT(lim.mpl_qlimit, MACH_PORT_QLIMIT_DEFAULT, "qlimit padrão");
    EXPECT(n, MACH_PORT_LIMITS_INFO_COUNT, "count do LIMITS_INFO");
    n = MACH_PORT_LIMITS_INFO_COUNT;
    EXPECT(mach_port_get_attributes(me, MACH_PORT_NULL, MACH_PORT_LIMITS_INFO, (mach_port_info_t)&lim, &n), KERN_SUCCESS, "LIMITS_INFO nome nulo");
    EXPECT(n, 0, "nome nulo zera o count");
    mach_port_status_t st;
    n = MACH_PORT_RECEIVE_STATUS_COUNT - 1;
    EXPECT(mach_port_get_attributes(me, r2, MACH_PORT_RECEIVE_STATUS, (mach_port_info_t)&st, &n), KERN_FAILURE, "RECEIVE_STATUS com count curto");
    mach_port_insert_right(me, r2, r2, MACH_MSG_TYPE_MAKE_SEND);
    for (int i = 0; i < 2; i++) {
        mach_msg_header_t h = {MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0), sizeof h, r2, 0, 0, 7};
        mach_msg(&h, MACH_SEND_MSG, sizeof h, 0, 0, 0, 0);
    }
    n = MACH_PORT_RECEIVE_STATUS_COUNT;
    memset(&st, 0x5A, sizeof st);
    EXPECT(mach_port_get_attributes(me, r2, MACH_PORT_RECEIVE_STATUS, (mach_port_info_t)&st, &n), KERN_SUCCESS, "RECEIVE_STATUS");
    EXPECT(st.mps_msgcount, 2, "mps_msgcount");
    EXPECT(st.mps_srights, 1, "mps_srights");
    EXPECT(st.mps_pset, 0, "mps_pset");
    EXPECT(st.mps_qlimit, MACH_PORT_QLIMIT_DEFAULT, "mps_qlimit");
    EXPECT(n, MACH_PORT_RECEIVE_STATUS_COUNT, "count do RECEIVE_STATUS");
    n = MACH_PORT_RECEIVE_STATUS_COUNT;
    EXPECT(mach_port_get_attributes(me, MACH_PORT_NULL, MACH_PORT_RECEIVE_STATUS, (mach_port_info_t)&st, &n), KERN_INVALID_RIGHT, "RECEIVE_STATUS nome nulo");
    EXPECT(mach_port_get_attributes(me, 0x7ff0ff03, MACH_PORT_RECEIVE_STATUS, (mach_port_info_t)&st, &n), KERN_INVALID_NAME, "RECEIVE_STATUS nome que não existe");
    EXPECT(mach_port_get_attributes(me, r2, 99, (mach_port_info_t)&st, &n), KERN_INVALID_ARGUMENT, "flavor desconhecido");

    printf("outros\n");
    uint64_t a = 0, b = 0;
    EXPECT(mach_generate_activity_id(me, 1, &a), KERN_SUCCESS, "activity id");
    EXPECT(mach_generate_activity_id(me, 1, &b), KERN_SUCCESS, "activity id de novo");
    EXPECT(a != 0 && b != 0 && a != b, 1, "activity ids diferentes");
    EXPECT(mach_generate_activity_id(me, 0, &a), KERN_INVALID_ARGUMENT, "activity id count 0");
    swtch_pri(0);
    EXPECT(1, 1, "swtch_pri volta");

    printf("machtraps: %d/%d ok\n", total - fails, total);
    return fails ? 1 : 0;
}
