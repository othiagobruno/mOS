/*
 * Fatia do runtime de concorrência do Swift 5.9 (Darwin, arm64 e x86_64).
 * Convenção swiftcall: o primeiro ponteiro vai no mesmo registrador da ABI C,
 * e ExecutorRef são duas palavras (identidade e witness). O nome C não leva
 * o "_" do Mach-O: o alvo macOS acrescenta um, o ELF do libSystem não.
 *
 * O bit de cancelamento é o 0x100 de ActiveTaskStatus.Flags. No AsyncTask de
 * 64 bits esse uint32 fica no offset 96 (Job de 64, ResumeContext, Reserved,
 * ExclusivityAccessSet[2], depois o status).
 *
 * O executor principal é a fila dispatch principal, com uma witness table
 * estável. Os jobs enfileirados nela rodam por swift_job_run.
 */
#ifdef MOS_HOST_TEST
#include <stddef.h>
#include <stdint.h>
#include <sys/syscall.h>
#include <unistd.h>
static int mos_tid(void) { return (int)syscall(SYS_gettid); }
#else
#include "sys.h"
static int mos_tid(void) { return (int)SYS0(SYS_GETTID); }
#endif

#define MOS_SWIFT __attribute__((swiftcall))
#define MOS_ASYNC __attribute__((swiftasynccall))

void *dispatch_get_main_queue(void);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));

typedef struct {
    void *identity;
    unsigned long impl;
} mos_exec_t;

/* Job (8 ponteiros) + contexto de retomada. Flags no offset 32, função no 56. */
#define JOB_FLAGS 32
#define JOB_RUN 56
#define TASK_RESUME_CTX 64
#define TASK_CANCEL_OFF 96
#define TASK_CANCEL_BIT 0x100u

static char mos_main_wtable[16];

typedef struct mos_slot {
    int tid;
    void *task;
    void *identity;
    unsigned long impl;
    int used;
} mos_slot_t;

static mos_slot_t mos_slot[64];
static volatile int mos_slot_lock;

static mos_slot_t *mos_find(int create) {
    int tid = mos_tid();
    mos_slot_t *found = 0;
    while (__sync_lock_test_and_set(&mos_slot_lock, 1)) {}
    int free_i = -1;
    int i;
    for (i = 0; i < 64; i++) {
        if (mos_slot[i].used && mos_slot[i].tid == tid) { found = &mos_slot[i]; break; }
        if (!mos_slot[i].used && free_i < 0) free_i = i;
    }
    if (!found && create && free_i >= 0) {
        mos_slot[free_i].used = 1;
        mos_slot[free_i].tid = tid;
        mos_slot[free_i].task = 0;
        mos_slot[free_i].identity = 0;
        mos_slot[free_i].impl = 0;
        found = &mos_slot[free_i];
    }
    __sync_lock_release(&mos_slot_lock);
    return found;
}

MOS_SWIFT void *swift_task_getCurrent(void) {
    mos_slot_t *s = mos_find(0);
    return s ? s->task : 0;
}

MOS_SWIFT _Bool swift_task_isCancelled(void *task) {
    if (!task) return 0;
    unsigned flags = __atomic_load_n((unsigned *)((char *)task + TASK_CANCEL_OFF), __ATOMIC_ACQUIRE);
    return (flags & TASK_CANCEL_BIT) != 0;
}

MOS_SWIFT void swift_task_cancel(void *task) {
    if (!task) return;
    __atomic_fetch_or((unsigned *)((char *)task + TASK_CANCEL_OFF), TASK_CANCEL_BIT, __ATOMIC_RELEASE);
}

MOS_SWIFT mos_exec_t swift_task_getMainExecutor(void) {
    mos_exec_t e;
    e.identity = dispatch_get_main_queue();
    e.impl = (unsigned long)mos_main_wtable;
    return e;
}

MOS_SWIFT mos_exec_t swift_task_getCurrentExecutor(void) {
    mos_exec_t e;
    mos_slot_t *s = mos_find(0);
    e.identity = s ? s->identity : 0;
    e.impl = s ? s->impl : 0;
    return e;
}

MOS_SWIFT _Bool swift_task_isCurrentExecutor(mos_exec_t executor) {
    mos_exec_t cur = swift_task_getCurrentExecutor();
    return cur.identity == executor.identity && cur.impl == executor.impl;
}

typedef void (*mos_async_fn)(void *) MOS_ASYNC;

MOS_SWIFT void swift_job_run(void *job, mos_exec_t executor) {
    if (!job) return;
    mos_slot_t *s = mos_find(1);
    void *old_task = 0;
    void *old_id = 0;
    unsigned long old_impl = 0;
    if (s) {
        old_task = s->task;
        old_id = s->identity;
        old_impl = s->impl;
        s->identity = executor.identity;
        s->impl = executor.impl;
    }
    unsigned kind = *(unsigned *)((char *)job + JOB_FLAGS) & 0xffu;
    void *ctx = job;
    if (kind == 0) {
        ctx = *(void **)((char *)job + TASK_RESUME_CTX);
        if (s) s->task = job;
    }
    void *fn = *(void **)((char *)job + JOB_RUN);
    if (fn) ((mos_async_fn)fn)(ctx);
    if (s) {
        s->task = old_task;
        s->identity = old_id;
        s->impl = old_impl;
    }
}

static void mos_run_on_main(void *job) {
    swift_job_run(job, swift_task_getMainExecutor());
}

MOS_SWIFT void swift_task_enqueueMainExecutor(void *job) {
    if (!job) return;
    dispatch_async_f(dispatch_get_main_queue(), job, mos_run_on_main);
}
