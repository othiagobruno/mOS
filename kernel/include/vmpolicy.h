/* Escolhas puras da memória virtual (proc/process.c, SYS_VMCTL): nível de pressão, limites de recurso (rlimit) e
 * vítima do jetsam. Sem estado: tools/vmpolicy_host.c testa no host. */
#pragma once

/* kern.memorystatus_vm_pressure_level do Darwin: 1 normal, 2 aviso, 4 crítico.
 * Aviso abaixo do piso do vm_pageout; crítico abaixo da metade dele. */
static inline int vmp_pressure_level(unsigned long long free_pages, unsigned long long floor_pages) {
    if (!floor_pages) return 1;
    if (free_pages < floor_pages / 2) return 4;
    if (free_pages < floor_pages) return 2;
    return 1;
}

/* kern.memorystatus_level: porcentagem de memória disponível (livre + o que o compressor ainda pode tirar) */
static inline int vmp_memorystatus_level(unsigned long long free_pages, unsigned long long total_pages) {
    if (!total_pages) return 100;
    unsigned long long p = free_pages * 100 / total_pages;
    return p > 100 ? 100 : (int)p;
}

/* RLIMIT_* do Darwin (sys/resource.h) */
enum { VMP_RLIMIT_CPU, VMP_RLIMIT_FSIZE, VMP_RLIMIT_DATA, VMP_RLIMIT_STACK, VMP_RLIMIT_CORE, VMP_RLIMIT_AS,
       VMP_RLIMIT_MEMLOCK, VMP_RLIMIT_NPROC, VMP_RLIMIT_NOFILE, VMP_RLIMIT_N };
#define VMP_RLIM_INF 0x7FFFFFFFFFFFFFFFULL

/* valor inicial de cada limite (cur e max). nofile e stack são os do kernel (PROC_MAX_FDS, USER_STACK_SIZE). */
static inline void vmp_rlim_default(int res, unsigned long long nofile, unsigned long long stack,
                                    unsigned long long *cur, unsigned long long *max) {
    *cur = *max = VMP_RLIM_INF;
    if (res == VMP_RLIMIT_CORE) *cur = 0;
    else if (res == VMP_RLIMIT_STACK) { *cur = stack; *max = 65532ULL << 10; }   /* max do macOS; o cur é a pilha real */
    else if (res == VMP_RLIMIT_NOFILE) *cur = nofile;          /* max infinito, como no macOS; o set corta no teto */
    else if (res == VMP_RLIMIT_NPROC) *cur = *max = 1064;      /* kern.maxprocperuid do macOS */
}

/* setrlimit: 0 ou -errno (22 EINVAL, 1 EPERM). cur > max é EINVAL; subir o max só root. Limites com teto do
 * kernel (stack, nofile) têm o cur cortado no teto, como o XNU faz com o NOFILE (maxfilesperproc). */
static inline int vmp_rlim_set(int res, unsigned long long old_max, unsigned long long new_cur,
                               unsigned long long new_max, int is_root, unsigned long long hard_cap,
                               unsigned long long *out_cur, unsigned long long *out_max) {
    if (res < 0 || res >= VMP_RLIMIT_N || new_cur > new_max) return -22;
    if (new_max > old_max && !is_root) return -1;
    if (hard_cap && new_cur > hard_cap) new_cur = hard_cap;    /* só o cur: o max pedido fica, como no XNU */
    *out_cur = new_cur;
    *out_max = new_max;
    return 0;
}

/* JETSAM_PRIORITY_* do Darwin: 0 ocioso, 3 fundo, 10 primeiro plano, 180 crítico (não sai). */
#define VMP_JPRIO_DEFAULT 10
#define VMP_JPRIO_CRITICAL 180

/* vítima sob pressão crítica: a de menor prioridade de jetsam; no empate, a de maior footprint. pid <= 1 e
 * prioridade >= 180 nunca. -1 = ninguém elegível. */
static inline int vmp_jetsam_pick(int n, const int *pid, const int *jprio, const unsigned long long *footprint) {
    int best = -1;
    for (int i = 0; i < n; i++) {
        if (pid[i] <= 1 || jprio[i] >= VMP_JPRIO_CRITICAL || !footprint[i]) continue;
        if (best < 0 || jprio[i] < jprio[best] || (jprio[i] == jprio[best] && footprint[i] > footprint[best])) best = i;
    }
    return best;
}

/* o processo passou do limite de footprint (memorystatus / task_limit)? limite 0 = sem limite */
static inline int vmp_over_limit(unsigned long long resident, unsigned long long swapped, unsigned long long limit) {
    return limit && resident + swapped > limit;
}
