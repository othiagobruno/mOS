// mOS — executor global do _Concurrency na variante com interop ObjC (docs/swift-objc-runtime.md, fase 3).
//
// O executor do Darwin (DispatchGlobalExecutor.cpp) manda os jobs às filas globais do libdispatch. No mOS elas ainda
// não têm threads (dispatch.c roda o trabalho na hora, na thread de quem chama, e dispatch_after_f fora da fila
// principal ignora o atraso). Mudar isso no dispatch.c afetaria o porte do VS Code, que conta com esse comportamento.
// Em vez disso, este arquivo usa os ganchos que o próprio Concurrency oferece (ConcurrencyHooks.def) e instala, num
// construtor estático, um executor global de verdade: threads de trabalho (pthreads do libSystem) com fila FIFO, mais
// uma lista de jobs com prazo (Task.sleep, timeouts) servida por uma thread de temporização. O executor principal
// (MainActor) continua sendo a fila principal do dispatch, drenada pelo dispatch_main/CFRunLoop do mOS.
#include "swift/Runtime/Concurrency.h"
#include "swift/ABI/Task.h"
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

using namespace swift;

namespace {

struct Node {
  Job *job;
  long long sec, nsec;   // prazo (relógio contínuo); só nos jobs adiados
  Node *next;
};

pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t g_ready = PTHREAD_COND_INITIALIZER;
Node *g_head = nullptr, *g_tail = nullptr;   // prontos (FIFO)
Node *g_timed = nullptr;                      // adiados, em ordem de prazo
bool g_started = false;
constexpr int kWorkers = 4;

void push_ready_locked(Node *n) {
  n->next = nullptr;
  if (g_tail) g_tail->next = n; else g_head = n;
  g_tail = n;
  pthread_cond_signal(&g_ready);
}

bool before(const Node *a, long long sec, long long nsec) {
  return a->sec < sec || (a->sec == sec && a->nsec <= nsec);
}

void *worker(void *) {
  for (;;) {
    pthread_mutex_lock(&g_lock);
    while (!g_head) pthread_cond_wait(&g_ready, &g_lock);
    Node *n = g_head;
    g_head = n->next;
    if (!g_head) g_tail = nullptr;
    pthread_mutex_unlock(&g_lock);
    Job *job = n->job;
    free(n);
    swift_job_run(job, SerialExecutorRef::generic());
  }
  return nullptr;
}

void *timer(void *) {
  for (;;) {
    long long sec, nsec;
    swift_get_time(&sec, &nsec, swift_clock_id_continuous);
    pthread_mutex_lock(&g_lock);
    while (g_timed && before(g_timed, sec, nsec)) {
      Node *n = g_timed;
      g_timed = n->next;
      push_ready_locked(n);
    }
    long long wait_us = 10000;   // no máximo 10 ms entre verificações
    if (g_timed) {
      long long d = (g_timed->sec - sec) * 1000000 + (g_timed->nsec - nsec) / 1000;
      if (d < wait_us) wait_us = d < 200 ? 200 : d;
    }
    pthread_mutex_unlock(&g_lock);
    usleep((unsigned)wait_us);
  }
  return nullptr;
}

void start_locked() {
  if (g_started) return;
  g_started = true;
  pthread_t t;
  for (int i = 0; i < kWorkers; i++) { pthread_create(&t, nullptr, worker, nullptr); pthread_detach(t); }
  pthread_create(&t, nullptr, timer, nullptr);
  pthread_detach(t);
}

void enqueue_at(Job *job, long long sec, long long nsec, bool timed) {
  Node *n = (Node *)malloc(sizeof(Node));
  n->job = job;
  n->sec = sec;
  n->nsec = nsec;
  pthread_mutex_lock(&g_lock);
  start_locked();
  if (!timed) {
    push_ready_locked(n);
  } else {
    Node **p = &g_timed;
    while (*p && before(*p, sec, nsec)) p = &(*p)->next;
    n->next = *p;
    *p = n;
  }
  pthread_mutex_unlock(&g_lock);
}

SWIFT_CC(swift) void mos_enqueueGlobal(Job *job, swift_task_enqueueGlobal_original) {
  enqueue_at(job, 0, 0, false);
}

SWIFT_CC(swift) void mos_enqueueGlobalWithDelay(unsigned long long delay, Job *job,
                                                swift_task_enqueueGlobalWithDelay_original) {
  long long sec, nsec;
  swift_get_time(&sec, &nsec, swift_clock_id_continuous);
  nsec += (long long)(delay % 1000000000ull);
  sec += (long long)(delay / 1000000000ull) + nsec / 1000000000;
  nsec %= 1000000000;
  enqueue_at(job, sec, nsec, true);
}

// o prazo vem no relógio pedido (contínuo, suspenso ou de parede); traduzido para o contínuo pela diferença
SWIFT_CC(swift) void mos_enqueueGlobalWithDeadline(long long sec, long long nsec, long long tsec, long long tnsec,
                                                   int clock, Job *job,
                                                   swift_task_enqueueGlobalWithDeadline_original) {
  (void)tsec; (void)tnsec;
  long long csec, cnsec, nowsec, nownsec;
  swift_get_time(&csec, &cnsec, swift_clock_id_continuous);
  swift_get_time(&nowsec, &nownsec, (swift_clock_id)clock);
  long long dsec = sec - nowsec, dnsec = nsec - nownsec;
  long long s = csec + dsec, ns = cnsec + dnsec;
  while (ns < 0) { ns += 1000000000; s--; }
  while (ns >= 1000000000) { ns -= 1000000000; s++; }
  enqueue_at(job, s, ns, true);
}

__attribute__((constructor)) void mos_install_executor() {
  swift_task_enqueueGlobal_hook = mos_enqueueGlobal;
  swift_task_enqueueGlobalWithDelay_hook = mos_enqueueGlobalWithDelay;
  swift_task_enqueueGlobalWithDeadline_hook = mos_enqueueGlobalWithDeadline;
}

} // namespace
