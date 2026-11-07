/* mOS — threads, escalonador e processos de usuário (ring 3, ELF64, syscalls via int 0x80)
 *
 * Modelo de execução:
 *   - Thread 0 é o laço do desktop (cooperativo). Ela só perde o processador quando uma interrupção
 *     a encontra parada num `hlt` (ociosa), então a UI, a rede, o USB e o áudio nunca são
 *     interrompidos no meio de uma operação.
 *   - Threads do kernel e processos de usuário são preemptivos entre si (fatia de SCHED_SLICE_MS)
 *     e devolvem o processador ao desktop ao fim de cada fatia.
 *   - Syscalls executam com interrupções desligadas e o desktop parado, por isso podem usar o
 *     VFS. Elas nunca podem esperar: quem precisa esperar (sleep, wait) bloqueia a thread.
 */
#pragma once
#include "kernel.h"

#define SCHED_SLICE_MS 4
/* região de usuário: PML4[128..135], 4 TiB. O V8 do Electron reserva a gaiola do sandbox (~1,34 TiB) de uma vez;
 * com menos, cai no sandbox parcial e o alocador de ArrayBuffer devolve nulo (BackingStore vazio, aborto) */
#define USER_PML4_SLOTS 8
#define USER_BASE   0x0000400000000000ULL
#define USER_END    (USER_BASE + USER_PML4_SLOTS * 0x8000000000ULL)
#define USER_PML4_FIRST ((int)((USER_BASE >> 39) & 511))
#define USER_STACK_TOP  (USER_BASE + 0x40000000ULL)   /* 1 GiB acima da base */
#define USER_STACK_SIZE (2048u << 10)   /* 2 MiB: argv/env do Electron passam de 1 MiB */
/* descritores por processo (o padrão do macOS é 256). Com 32 (29 livres), o processo principal do VS Code dava EMFILE
 * no state.vscdb, no watcher da pasta User, no /dev/null e no workbench.html, e a janela não carregava.
 * Cada entrada tem ~288 bytes (o caminho vai junto): ~72 KiB por processo. */
#define PROC_MAX_FDS 256

enum { TH_READY, TH_RUNNING, TH_SLEEPING, TH_WAITING, TH_DEAD };

typedef struct process process_t;

/* pipe entre processos (SYS_PIPE): 4 KiB; leitura bloqueia se vazio, escrita se cheio */
enum { FD_FILE, FD_PIPE_R, FD_PIPE_W };
typedef struct pipe {
    char buf[4096];
    u32 head, tail, count;
    int readers, writers;
    struct thread *rwait, *wwait;   /* threads bloqueadas */
} pipe_t;

typedef struct thread {
    int id;
    char name[32];
    int state;
    u64 frame;              /* int_frame_t* salvo (rsp na troca de contexto) */
    u8 *kstack;             /* pilha do kernel (NULL para a thread 0) */
    u64 kstack_top;
    u64 wake_at;            /* TH_SLEEPING */
    int wait_pid;           /* TH_WAITING */
    process_t *proc;        /* NULL = thread do kernel */
    u64 cpu_ms;             /* tempo de CPU */
    struct thread *next;
    u64 wait_status;        /* SYS_WAITANY: onde gravar o código de saída (endereço de usuário) */
    u64 read_buf, read_len; /* leitura de stdin bloqueada */
    int exit_code;          /* SYS_THREAD_EXIT */
    int join_tid;           /* TH_WAITING em SYS_THREAD_JOIN */
    u64 gs_base;            /* base do GS em ring 3 (TLS das threads do macOS: gs:[chave*8]) */
    bool wait_posix;        /* SYS_WAITPID: rax = pid e status POSIX gravado em wait_status */
    /* espera numa porta Mach (kernel/proc/machipc.c), com wait_pid = -7 e TH_WAITING */
    void *mach_wait;        /* porta ou conjunto esperado */
    u64 mach_deadline;      /* 0 = sem prazo; o escalonador acorda no prazo (rax já tem o erro de tempo) */
    u64 mach_buf;           /* buffer de recepção (endereço de usuário) */
    u32 mach_rsize, mach_opt;
    bool mach_peek;         /* MACH_OP_WAIT: só espera haver mensagem */
    u64 futex_va;           /* TH_WAITING com wait_pid = -8: palavra do SYS_FUTEX_WAIT (prazo em wake_at; 0 = sem) */
} thread_t;

/* sessão de terminal: entrada (stdin) e saída (stdout/stderr) de um grupo de processos */
typedef struct tty {
    char in[2048];  u32 ih, it;  bool eof;
    char out[16384]; u32 oh, ot;
    thread_t *reader;       /* thread bloqueada em read(0) */
    int refs;
    bool closed;            /* o dono (ex.: o Terminal) não vai mais ler */
} tty_t;

struct process {
    int pid, ppid;
    char name[32];
    char path[384];         /* caminhos de helpers dentro de bundles passam de 128 (Chrome: ~200 caracteres) */
    u64 *pml4;             /* espaço de endereçamento (físico = virtual no kernel) */
    u64 brk, brk_start;     /* heap */
    int exit_code;
    bool exited;
    u64 mem_pages;
    u64 start_ms;
    thread_t *main;
    struct process *next;
    struct { bool used; int flags; u64 off; char path[256]; u8 kind; struct pipe *pipe; } fds[PROC_MAX_FDS];   /* arquivos (VFS) e pipes */
    u64 mmap_next;          /* próxima região anônima (SYS_MMAP) */
    int nthreads;           /* threads vivas ou ainda não recolhidas (a memória sai com a última) */
    u64 sig_handler[32];    /* 0 = padrão, 1 = ignorar, senão endereço do tratador */
    u64 sig_restorer;       /* rotina de usuário que chama SYS_SIGRETURN */
    u32 sig_pending;
    struct { int tid, code; } texit[16];   /* códigos de threads já encerradas (para join) */
    tty_t *tty;             /* NULL = console global do sistema */
    char cwd[128];          /* pasta atual (caminhos relativos nas syscalls) */
    void *dyld;             /* Mach-O: grafo de imagens do carregador, para o dlopen (kernel/compat/macho.c) */
    void *mach;             /* espaço de nomes de portas Mach (kernel/proc/machipc.c); NULL até o primeiro uso */
    char **spawn_argv;      /* só durante macho_load: argv já copiado para o kernel (posix_spawn) */
    int spawn_argc;
    char **spawn_env;
    int spawn_envc;
};

/* ---- portas Mach entre processos (proc/machipc.c) */
void      machipc_spawn(process_t *parent, process_t *child);   /* filho novo: porta de bootstrap herdada */
void      machipc_exit(process_t *p);                           /* destrói os direitos do processo */
u64       machipc_syscall(int_frame_t *f);                      /* SYS_MACH_FIRST..SYS_MACH_LAST */
void      machipc_set_prot(process_t *p, u64 lo, u64 hi, u32 prot);   /* proteção registrada de [lo, hi) (mach_vm_region) */

/* páginas físicas compartilhadas entre espaços (memória Mach, proc/process.c) */
bool      uvm_map_phys(process_t *p, u64 va, u64 phys, bool rw);  /* mais uma referência à página */
void      uvm_page_get(u64 phys);
void      uvm_page_put(u64 phys);

#define USER_MMAP_BASE (USER_BASE + 0x100000000ULL)   /* 4 GiB acima da base, depois da pilha */

/* ---- escalonador (proc/sched.c) */
void      sched_init(void);
thread_t *thread_current(void);
thread_t *kthread_create(const char *name, void (*fn)(void *), void *arg);
void      thread_exit(void) NORETURN;           /* threads do kernel */
u64       sched_irq_exit(int_frame_t *f, bool timer);   /* devolve o frame a restaurar */
u64       sched_switch(int_frame_t *f);         /* a thread atual bloqueou/saiu: escolhe outra */
void      sched_make_ready(thread_t *t);
thread_t *sched_threads(void);

/* ---- processos (proc/process.c) */
void      proc_init(void);                      /* cria a infra e roda o autoteste, se houver */
int       proc_spawn(const char *path, const char *arg);   /* pid ou -1 */
bool      proc_kill(int pid);
int       proc_count(void);
bool      proc_info(int i, int *pid, char *name, usize ncap, int *state, u64 *mem_kb, u64 *cpu_ms);
usize     proc_console_read(char *buf, usize max);         /* saída (stdout/stderr) dos programas */

/* sessões de terminal (para o app Terminal hospedar programas interativos, ex.: "sh").
 * Filhos herdam a sessão; read(0) bloqueia até haver entrada; chame do laço do desktop. */
int       proc_spawn_tty(const char *path, const char *arg);     /* pid do líder ou -1 */
usize     proc_tty_read(int pid, char *buf, usize max);          /* saída da sessão (consome) */
bool      proc_tty_write(int pid, const char *data, usize len);  /* entrada (stdin) */
void      proc_tty_eof(int pid);                                 /* fim da entrada (Ctrl+D) */
int       proc_tty_interrupt(int pid);                           /* Ctrl+C: encerra os processos da sessão
                                                                    exceto o líder; devolve quantos */
int       proc_tty_interrupt_fg(int pid);                        /* idem, só se houver processo em primeiro plano */
bool      proc_alive(int pid);
int       proc_exit_code(int pid);                               /* -1000 se ainda roda ou não existe */
u64       proc_syscall(int_frame_t *f);         /* int 0x80 */
u64       proc_fault(int_frame_t *f, u64 cr2);  /* exceção em ring 3: COW, sinal ou encerra o processo */
u64       proc_signal_check(int_frame_t *f);    /* antes de voltar ao ring 3: entrega sinais pendentes */
bool      proc_signal(int pid, int sig);        /* envia um sinal (SIGKILL encerra na hora) */
thread_t *uthread_clone(process_t *p, const int_frame_t *src);   /* fork: mesma posição de retorno */
void      proc_reap(process_t *p);
void      proc_thread_exited(thread_t *t);

/* ---- Mach-O (kernel/compat/, opcional): monta segmentos, pilha e argumentos; devolve rip/rsp/rdi */
bool      macho_load(process_t *p, const u8 *img, usize len, const char *arg, u64 *entry,
                     u64 *user_rsp, u64 *rdi, char *err, usize errlen);

/* ---- ELF (proc/elf.c) */
bool      elf_load(process_t *p, const u8 *img, usize len, u64 *entry, char *err, usize errlen);

/* ---- memória de usuário (proc/process.c) */
bool      uvm_map(process_t *p, u64 va, u64 len);       /* páginas de 4 KiB, zeradas, U|RW */
u64      *uvm_pte(process_t *p, u64 va);               /* NULL se não mapeado */
bool      uvm_copy_out(process_t *p, u64 va, const void *src, usize len);
bool      uvm_copy_in(process_t *p, void *dst, u64 va, usize len);

/* ---- arquitetura (arch/cpu.c, arch/smp.c) */
void      tss_set_rsp0(u64 rsp0);
void      smp_init(void);
int       smp_cpu_count(void);                  /* núcleos ativos (BSP + APs) */
