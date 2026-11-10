/* QoS de thread do harness, o mesmo mapa de kernel/include/smp_perf.h e sched_set_qos. */
#pragma once

int host_qos_from_darwin(unsigned q);
void host_thr_boot(void);
void host_thr_add(int tid);
void host_thr_dead(int tid);
/* tid 0 é a thread que chama (self). Devolve 0 ou -MOS_ESRCH. */
long host_thr_setqos(int tid, int self, unsigned darwin);
int host_thr_qos(int tid);
