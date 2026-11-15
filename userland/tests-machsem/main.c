/* mOS — semáforos Mach: o contrato do XNU, conferido com o Darling (que roda o sync_sema.c do XNU).
 * Cada linha imprime o kern_return_t cru e o valor esperado; o mesmo binário roda no Darling, no macOS
 * e no mOS (tools/test-machsem.sh), e as linhas "ok" têm de sair iguais nos três.
 * Esperas entre threads usam usleep para a outra thread chegar na fila; 150 ms bastam no QEMU. */
#include <stdio.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <mach/mach.h>

static int fails, total;
#define EXPECT(got, want, what) do { int g_ = (int)(got), w_ = (int)(want); total++; \
    if (g_ == w_) printf("  ok  %s = %d\n", what, g_); \
    else { fails++; printf("  FALHOU  %s = %d (esperado %d)\n", what, g_, w_); } } while (0)

static long now_ms(void) { struct timeval tv; gettimeofday(&tv, 0); return tv.tv_sec * 1000L + tv.tv_usec / 1000; }
static mach_timespec_t ts(unsigned s, int ns) { mach_timespec_t t = {s, ns}; return t; }

typedef struct { semaphore_t wait, sig; int mode; volatile int kr; volatile int done; } job_t;
enum { J_WAIT, J_WAIT_SIGNAL, J_TIMEDWAIT_LONG };
static void *worker(void *p) {
    job_t *j = p;
    if (j->mode == J_WAIT) j->kr = semaphore_wait(j->wait);
    else if (j->mode == J_WAIT_SIGNAL) j->kr = semaphore_wait_signal(j->wait, j->sig);
    else j->kr = semaphore_timedwait(j->wait, ts(5, 0));
    __atomic_store_n(&j->done, 1, __ATOMIC_RELEASE);
    return 0;
}
static pthread_t spawn(job_t *j) { pthread_t t; pthread_create(&t, 0, worker, j); usleep(150000); return t; }

int main(void) {
    setbuf(stdout, 0);
    task_t me = mach_task_self();
    semaphore_t s = 0, b = 0;

    printf("criação\n");
    EXPECT(semaphore_create(me, &s, SYNC_POLICY_FIFO, -1), KERN_INVALID_ARGUMENT, "create valor -1");
    EXPECT(s, 0, "create valor -1 zera a saída");
    EXPECT(semaphore_create(me, &s, 99, 0), KERN_INVALID_ARGUMENT, "create policy 99");
    EXPECT(semaphore_create(me, &s, SYNC_POLICY_FIFO, 0), KERN_SUCCESS, "create ok");
    EXPECT(semaphore_create(me, &b, SYNC_POLICY_FIFO, 0), KERN_SUCCESS, "create segundo");

    printf("sem ninguém esperando\n");
    EXPECT(semaphore_timedwait(s, ts(0, 0)), KERN_OPERATION_TIMED_OUT, "timedwait {0,0} vazio");
    EXPECT(semaphore_timedwait(s, ts(0, 1000000000)), KERN_INVALID_VALUE, "timedwait nsec 1e9");
    EXPECT(semaphore_timedwait(s, ts(0, -1)), KERN_INVALID_VALUE, "timedwait nsec -1");
    EXPECT(semaphore_signal(s), KERN_SUCCESS, "signal guarda crédito");
    EXPECT(semaphore_timedwait(s, ts(0, 0)), KERN_SUCCESS, "timedwait {0,0} pega o crédito");
    EXPECT(semaphore_signal_all(s), KERN_SUCCESS, "signal_all vazio");
    EXPECT(semaphore_timedwait(s, ts(0, 0)), KERN_OPERATION_TIMED_OUT, "signal_all não guarda crédito");
    EXPECT(semaphore_signal_thread(s, MACH_PORT_NULL), KERN_NOT_WAITING, "signal_thread nulo vazio");
    EXPECT(semaphore_timedwait(s, ts(0, 0)), KERN_OPERATION_TIMED_OUT, "signal_thread não guarda crédito");
    EXPECT(semaphore_signal_thread(s, mach_thread_self()), KERN_NOT_WAITING, "signal_thread em quem não espera");
    long t0 = now_ms();
    EXPECT(semaphore_timedwait(s, ts(0, 100000000)), KERN_OPERATION_TIMED_OUT, "timedwait 100 ms vazio");
    long dt = now_ms() - t0;
    EXPECT(dt >= 80 && dt < 2000, 1, "timedwait 100 ms esperou o prazo");

    printf("nomes errados\n");
    EXPECT(semaphore_signal(MACH_PORT_NULL), KERN_INVALID_NAME, "signal nome nulo");
    EXPECT(semaphore_wait(MACH_PORT_NULL), KERN_INVALID_NAME, "wait nome nulo");
    EXPECT(semaphore_signal(me), KERN_INVALID_CAPABILITY, "signal na porta da task");

    printf("com uma thread esperando\n");
    job_t j1 = {s, 0, J_WAIT, -1, 0};
    pthread_t t1 = spawn(&j1);
    EXPECT(j1.done, 0, "wait bloqueou");
    EXPECT(semaphore_signal_thread(s, MACH_PORT_NULL), KERN_SUCCESS, "signal_thread nulo acorda");
    pthread_join(t1, 0);
    EXPECT(j1.kr, KERN_SUCCESS, "wait voltou com sucesso");

    job_t j2 = {s, 0, J_WAIT, -1, 0};
    pthread_t t2 = spawn(&j2);
    EXPECT(semaphore_signal_thread(s, pthread_mach_thread_np(t2)), KERN_SUCCESS, "signal_thread na thread certa");
    pthread_join(t2, 0);
    EXPECT(j2.kr, KERN_SUCCESS, "a thread certa acordou");

    printf("FIFO e signal_all\n");
    job_t j3 = {s, 0, J_WAIT, -1, 0}, j4 = {s, 0, J_WAIT, -1, 0};
    pthread_t t3 = spawn(&j3), t4 = spawn(&j4);
    EXPECT(semaphore_signal(s), KERN_SUCCESS, "signal com dois na fila");
    usleep(150000);
    EXPECT(j3.done + j4.done * 2, 1, "acordou só o primeiro da fila");
    EXPECT(semaphore_signal_all(s), KERN_SUCCESS, "signal_all com um na fila");
    pthread_join(t3, 0); pthread_join(t4, 0);
    EXPECT(j4.kr, KERN_SUCCESS, "signal_all acordou o segundo");
    EXPECT(semaphore_timedwait(s, ts(0, 0)), KERN_OPERATION_TIMED_OUT, "nenhum crédito sobrou");

    printf("wait_signal\n");
    job_t j5 = {s, b, J_WAIT_SIGNAL, -1, 0};
    pthread_t t5 = spawn(&j5);
    EXPECT(semaphore_timedwait(b, ts(0, 0)), KERN_SUCCESS, "wait_signal sinalizou o outro");
    EXPECT(j5.done, 0, "wait_signal continua esperando");
    EXPECT(semaphore_signal(s), KERN_SUCCESS, "signal libera wait_signal");
    pthread_join(t5, 0);
    EXPECT(j5.kr, KERN_SUCCESS, "wait_signal voltou com sucesso");
    EXPECT(semaphore_timedwait_signal(s, b, ts(0, 0)), KERN_OPERATION_TIMED_OUT, "timedwait_signal {0,0} vazio");
    EXPECT(semaphore_timedwait(b, ts(0, 0)), KERN_SUCCESS, "timedwait_signal sinalizou mesmo sem esperar");

    printf("destroy\n");
    job_t j6 = {s, 0, J_WAIT, -1, 0}, j7 = {s, 0, J_TIMEDWAIT_LONG, -1, 0};
    pthread_t t6 = spawn(&j6), t7 = spawn(&j7);
    EXPECT(semaphore_destroy(me, s), KERN_SUCCESS, "destroy com dois na fila");
    pthread_join(t6, 0); pthread_join(t7, 0);
    EXPECT(j6.kr, KERN_TERMINATED, "wait acordou com KERN_TERMINATED");
    EXPECT(j7.kr, KERN_TERMINATED, "timedwait acordou com KERN_TERMINATED");
    EXPECT(semaphore_destroy(me, MACH_PORT_NULL), KERN_INVALID_ARGUMENT, "destroy nome nulo");
    EXPECT(semaphore_destroy(me, b), KERN_SUCCESS, "destroy segundo");

    printf("machsem: %d/%d ok\n", total - fails, total);
    return fails ? 1 : 0;
}
