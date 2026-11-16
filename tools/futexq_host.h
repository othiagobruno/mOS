/* Host: o mínimo de proc.h que kernel/proc/futexq.c usa (tools/futexq_host.c) */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef uint64_t u64;
enum { TH_READY = 0, TH_RUNNING = 1, TH_SLEEPING = 2, TH_WAITING = 3, TH_DEAD = 4 };
typedef struct process { int pid; } process_t;
typedef struct thread {
    int id, state, wait_pid;
    process_t *proc;
    u64 futex_va, futex_phys, wake_at;
    struct thread *fq_next[2], *fq_prev[2];
    u16 fq_b[2];
    u8 fq_on;
} thread_t;
