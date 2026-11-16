/*
 * mOS libSystem — funções de libc da BSD/Darwin que ainda eram stub vazio em sys_zerados.c (frente runtime C):
 * o gerador random() da BSD, ffs/fls, lldiv, timingsafe_bcmp, NXArchInfo, o desenrolar de pilha por ponteiro de
 * frame (backtrace_from_fp), e os poucos *_np de pthread/posix_spawn que o runtime importa. Tudo com comportamento
 * de verdade; nada de corpo vazio.
 */
#include "sys.h"
#include "libc.h"

int open(const char *path, int flags, ...);
int getpid(void);
void *malloc(size_t);
char *strdup(const char *);
unsigned qos_class_self(void);
int vasprintf(char **out, const char *fmt, __builtin_va_list ap);
long long strtoll(const char *s, char **end, int base);
unsigned long long strtoull(const char *s, char **end, int base);

/* ================================================================ random() da BSD
 * Gerador de Fibonacci atrasado, o mesmo da BSD/Darwin (port do musl 1.2.5, MIT), com a semeadura corrigida.
 * Substitui o random()/srandom() que eram só um atalho para rand() (sequência fraca e com período curto). */
static u32 rnd_init[] = {
    0x00000000, 0x5851f42d, 0xc0b18ccf, 0xcbb5f646, 0xc7033129, 0x30705b04, 0x20fd5db4, 0x9a8b7f78,
    0x502959d8, 0xab894868, 0x6c0356a7, 0x88cdb7ff, 0xb477d43f, 0x70a3a52b, 0xa8e4baf1, 0xfd8341fc,
    0x8ae16fd9, 0x742d2f7a, 0x0d1f0796, 0x76035e09, 0x40f7702c, 0x6fa72ca5, 0xaaa84157, 0x58a0df74,
    0xc74a0364, 0xae533cc4, 0x04185faf, 0x6de3b115, 0x0cab8628, 0xf043bfa4, 0x398150e9, 0x37521657};
static int rnd_n = 31, rnd_i = 3, rnd_j = 0;
static u32 *rnd_x = rnd_init + 1;
static volatile int rnd_lock;

static void rnd_lk(void) { while (__sync_lock_test_and_set(&rnd_lock, 1)) while (rnd_lock) __asm__ __volatile__("" ::: "memory"); }
static void rnd_ul(void) { __sync_lock_release(&rnd_lock); }
static u32 lcg31(u32 x) { return (1103515245u * x + 12345u) & 0x7fffffff; }
static u64 lcg64(u64 x) { return 6364136223846793005ull * x + 1; }
static void *rnd_save(void) { rnd_x[-1] = ((u32)rnd_n << 16) | ((u32)rnd_i << 8) | (u32)rnd_j; return rnd_x - 1; }
static void rnd_load(u32 *s) { rnd_x = s + 1; rnd_n = rnd_x[-1] >> 16; rnd_i = (rnd_x[-1] >> 8) & 0xff; rnd_j = rnd_x[-1] & 0xff; }

static void rnd_seed(unsigned seed) {
    u64 s = seed;
    if (rnd_n == 0) { rnd_x[0] = (u32)s; return; }
    rnd_i = rnd_n == 31 || rnd_n == 7 ? 3 : 1;
    rnd_j = 0;
    for (int k = 0; k < rnd_n; k++) { s = lcg64(s); rnd_x[k] = (u32)(s >> 32); }
    rnd_x[0] |= 1;                                  /* garante ao menos um ímpar */
}

void srandom(unsigned seed) { rnd_lk(); rnd_seed(seed); rnd_ul(); }

char *initstate(unsigned seed, char *state, size_t size) {
    void *old;
    if (size < 8) return 0;
    rnd_lk();
    old = rnd_save();
    rnd_n = size < 32 ? 0 : size < 64 ? 7 : size < 128 ? 15 : size < 256 ? 31 : 63;
    rnd_x = (u32 *)state + 1;
    rnd_seed(seed);
    rnd_save();
    rnd_ul();
    return old;
}

char *setstate(char *state) {
    void *old;
    rnd_lk();
    old = rnd_save();
    rnd_load((u32 *)state);
    rnd_ul();
    return old;
}

long random(void) {
    long k;
    rnd_lk();
    if (rnd_n == 0) { k = rnd_x[0] = lcg31(rnd_x[0]); }
    else {
        rnd_x[rnd_i] += rnd_x[rnd_j];
        k = rnd_x[rnd_i] >> 1;
        if (++rnd_i == rnd_n) rnd_i = 0;
        if (++rnd_j == rnd_n) rnd_j = 0;
    }
    rnd_ul();
    return k;
}

/* ================================================================ busca de bit (BSD) */
int ffs(int v) { return __builtin_ffs(v); }
int ffsl(long v) { return __builtin_ffsl(v); }
int ffsll(long long v) { return __builtin_ffsll(v); }
int fls(int v) { return v ? 32 - __builtin_clz((unsigned)v) : 0; }
int flsl(long v) { return v ? 64 - __builtin_clzl((unsigned long)v) : 0; }
int flsll(long long v) { return v ? 64 - __builtin_clzll((unsigned long long)v) : 0; }

/* ================================================================ divisão 64 bits */
typedef struct { long long quot, rem; } lldiv_t;
lldiv_t lldiv(long long a, long long b) { lldiv_t r = {a / b, a % b}; return r; }

/* ================================================================ comparação em tempo constante */
int timingsafe_bcmp(const void *a, const void *b, size_t n) {
    const volatile u8 *p = a, *q = b;
    u8 acc = 0;
    for (size_t k = 0; k < n; k++) acc |= (u8)(p[k] ^ q[k]);
    return acc != 0;
}

/* ================================================================ NXArchInfo (libmacho/libSystem) */
typedef struct { const char *name; int cputype, cpusubtype; int byteorder; const char *description; } NXArchInfo;
#define CPU_ARCH_ABI64 0x01000000
#define CPU_TYPE_X86_64 (0x7 | CPU_ARCH_ABI64)
#define CPU_TYPE_ARM64 (0xC | CPU_ARCH_ABI64)
#define NX_LittleEndian 1
static const NXArchInfo nx_table[] = {
    {"x86_64", CPU_TYPE_X86_64, 3 /* _ALL */, NX_LittleEndian, "Intel x86-64"},
    {"x86_64h", CPU_TYPE_X86_64, 8 /* _H (Haswell) */, NX_LittleEndian, "Intel x86-64h Haswell"},
    {"arm64", CPU_TYPE_ARM64, 0 /* _ALL */, NX_LittleEndian, "ARM64"},
    {"arm64e", CPU_TYPE_ARM64, 2, NX_LittleEndian, "ARM64E"},
    {0, 0, 0, 0, 0},
};
/* -1 em cpusubtype (CPU_SUBTYPE_MULTIPLE) casa com o primeiro da arquitetura, como no cctools */
const NXArchInfo *NXGetArchInfoFromCpuType(int cputype, int cpusubtype) {
    const NXArchInfo *best = 0;
    for (const NXArchInfo *a = nx_table; a->name; a++) {
        if (a->cputype != cputype) continue;
        if (cpusubtype == -1 || (cpusubtype & ~0x80000000) == a->cpusubtype) return a;
        if (!best) best = a;                        /* a arquitetura bate, o subtipo não: guarda o primeiro */
    }
    return best;
}
const NXArchInfo *NXGetLocalArchInfo(void) {
#if defined(__aarch64__)
    return NXGetArchInfoFromCpuType(CPU_TYPE_ARM64, -1);
#else
    return NXGetArchInfoFromCpuType(CPU_TYPE_X86_64, -1);
#endif
}

/* ================================================================ backtrace por ponteiro de frame
 * O quadro de chamada guarda {frame anterior; endereço de retorno} em [fp] e [fp+8] nas duas arquiteturas
 * (x86_64 com rbp encadeado; arm64 com x29/x30). Para no primeiro fp fora de faixa ou não crescente. */
int backtrace_from_fp(void *startfp, void **array, int size) {
    int n = 0;
    u64 fp = (u64)startfp, prev = 0;
    while (fp && n < size && fp > prev && (fp & 7) == 0) {
        u64 next = ((u64 *)fp)[0];
        void *ret = (void *)((u64 *)fp)[1];
        if (!ret) break;
        array[n++] = ret;
        prev = fp;
        fp = next;
    }
    return n;
}
/* backtrace_async: a pilha do contexto atual (o mOS não tem continuações async do Swift aqui); task_id = 0 */
unsigned long backtrace_async(void **array, unsigned long length, u32 *task_id) {
    if (task_id) *task_id = 0;
    int n = backtrace_from_fp(__builtin_frame_address(0), array, (int)length);
    return (unsigned long)n;
}

/* ================================================================ ganchos de rastreio (nulos por padrão) */
void (*malloc_logger)(u32 type, uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t result, u32 num_hot_frames_to_skip);
void (*__syscall_logger)(u32 type, uintptr_t a1, uintptr_t a2, uintptr_t a3, uintptr_t a4, uintptr_t a5);

/* ================================================================ syscall() genérica
 * O mOS não usa a numeração do XNU; só os poucos números que apps de fato chamam direto por syscall() têm sentido.
 * O resto devolve -1/ENOSYS, como uma syscall ausente, em vez de silenciosamente não fazer nada. */
long syscall(long number, ...) {
    switch (number) {
    case 20: return getpid();                        /* SYS_getpid */
    case 372: return SYS0(SYS_GETTID);               /* SYS_thread_selfid */
    }
    cx_errno = CX_ENOSYS;
    return -1;
}
int sys_nerr = 256;                                  /* variável, não função: tamanho de sys_errlist no Darwin */

/* ================================================================ variantes com locale (o mOS tem só o "C") */
int vasprintf_l(char **out, void *loc, const char *fmt, __builtin_va_list ap) { (void)loc; return vasprintf(out, fmt, ap); }
long long strtoll_l(const char *s, char **end, int base, void *loc) { (void)loc; return strtoll(s, end, base); }
unsigned long long strtoull_l(const char *s, char **end, int base, void *loc) { (void)loc; return strtoull(s, end, base); }

/* ================================================================ atributos de arquivo / xattr */
int open_dprotected_np(const char *path, int flags, int class, int dpflags, int mode) {
    (void)class; (void)dpflags;                      /* o mOS não tem Data Protection: abre normal */
    return open(path, flags, mode);
}
/* xattr_name_with_flags: devolve o nome do atributo com o sufixo que codifica os flags (XATTR_FLAG_*).
 * Sem flags (o caso comum), é o próprio nome. A string é alocada; o chamador libera. */
char *xattr_name_with_flags(const char *name, int flags) {
    if (!name) return 0;
    if (!flags) return strdup(name);
    size_t n = strlen(name);
    char *r = malloc(n + 8);
    if (!r) return 0;
    memcpy(r, name, n);
    char *p = r + n;
    if (flags & 0x0001) *p++ = '#';                  /* XATTR_FLAG_NO_EXPORT  -> '#B' (preservado em cópias) */
    if (flags & 0x0002) *p++ = 'P';                  /* XATTR_FLAG_CONTENT_DEPENDENT */
    if (flags & 0x0004) *p++ = 'S';                  /* XATTR_FLAG_NEVER_PRESERVE */
    if (flags & 0x0008) *p++ = 'C';                  /* XATTR_FLAG_SYNCABLE */
    *p = 0;
    return r;
}

/* ================================================================ pthread / posix_spawn (_np) */
int pthread_fchdir_np(int fd) {                      /* muda a pasta atual pelo descritor (por thread no Darwin; aqui, global) */
    int fchdir(int fd);
    return fchdir(fd);
}
/* a pthread_t de uma porta de thread Mach: só dá para a thread atual (o mOS não indexa o bloco das outras) */
void *pthread_from_mach_thread_np(u32 port) {
    void *pthread_self(void);
    u32 mach_thread_self(void);
    return port == mach_thread_self() ? pthread_self() : 0;
}
/* pthread_get_qos_class_np e pthread_introspection_hook_install: darwin.c e extra.c (QoS por thread e gancho chamado
 * no pthread_create e no início/fim da thread); __pthread_workqueue_setkill: dispatch.c */
/* portas de exceção do próximo filho de posix_spawn: o mOS não propaga portas de exceção no spawn */
int posix_spawnattr_setexceptionports_np(void *attr, unsigned mask, u32 port, int behavior, int flavor) {
    (void)attr; (void)mask; (void)port; (void)behavior; (void)flavor;
    return 0;
}
