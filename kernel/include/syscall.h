/* mOS — ABI de syscalls dos processos de usuário (compartilhado entre kernel e userland).
 *
 *   int 0x80, número em rax, argumentos em rdi, rsi, rdx, r10, r8; resultado em rax.
 *   Erros: valor negativo = -errno (valores do Linux/BSD abaixo). Registradores preservados,
 *   exceto rax (rcx e r11 também podem mudar, como no `syscall` do Linux).
 */
#pragma once

enum {
    SYS_EXIT = 0,      /* (code)                                   não volta */
    SYS_WRITE,         /* (fd, buf, len) -> bytes                  fd 1/2 = console; senão arquivo */
    SYS_GETPID,        /* () -> pid */
    SYS_SLEEP,         /* (ms) -> 0 */
    SYS_YIELD,         /* () -> 0 */
    SYS_TIME,          /* () -> ms desde o boot */
    SYS_BRK,           /* (addr) -> novo brk; addr 0 = consulta */
    SYS_READFILE,      /* (path, buf, max) -> bytes                arquivo inteiro de uma vez */
    SYS_WRITEFILE,     /* (path, buf, len) -> 0                    cria ou substitui */
    SYS_SPAWN,         /* (path, arg) -> pid                       nome curto procura em /Sistema/Programas */
    SYS_WAIT,          /* (pid) -> código de saída                 bloqueia até o processo terminar */
    SYS_FILESIZE,      /* (path) -> tamanho */
    SYS_CPUS,          /* () -> núcleos ativos */
    SYS_KILL,          /* (pid) -> 0                               o processo sai com -9 */
    SYS_OPEN,          /* (path, flags) -> fd                      flags O_* abaixo */
    SYS_CLOSE,         /* (fd) -> 0 */
    SYS_READ,          /* (fd, buf, len) -> bytes                  fd 0 = sem entrada (0 = EOF) */
    SYS_LSEEK,         /* (fd, off, whence) -> nova posição        SEEK_SET/CUR/END */
    SYS_FSTAT,         /* (fd, mos_stat_t*) -> 0 */
    SYS_MMAP,          /* (len) -> endereço                        memória anônima zerada, RW */
    SYS_MUNMAP,        /* (addr, len[, MOS_MUNMAP_KEEP]) -> 0       KEEP: só descarta as páginas, a faixa segue reservada */
    SYS_UNIXTIME,      /* () -> segundos desde 1970 (relógio do sistema) */
    SYS_STAT,          /* (path, mos_stat_t*) -> 0 */
    SYS_READDIR,       /* (path, index, namebuf, cap) -> 1 = nome copiado, 0 = fim */
    SYS_MKDIR,         /* (path) -> 0 */
    SYS_UNLINK,        /* (path) -> 0                              arquivo ou pasta (recursivo) */
    SYS_RENAME,        /* (from, to) -> 0 */
    SYS_WAITANY,       /* (int *status) -> pid de um filho que terminou; bloqueia; -ESRCH se não há filhos */
    SYS_CHDIR,         /* (path) -> 0 */
    SYS_GETCWD,        /* (buf, cap) -> tamanho */
    SYS_PROCLIST,      /* (index, mos_procinfo_t*) -> 1 = preenchido, 0 = fim */
    SYS_PIPE,          /* (int fds[2], modo) -> 0                  fds[0] leitura, fds[1] escrita; modo MOS_PIPE_SOCKPAIR: par bidirecional */
    SYS_SPAWNIO,       /* (path, arg, int io[3]) -> pid            io[i] = fd do pai para o fd i do filho, ou -1 */
    SYS_DUP2,          /* (fd, novo) -> novo */
    SYS_THREAD_CREATE, /* (entry, user_rsp, arg) -> tid             nova thread no mesmo processo (rdi = arg) */
    SYS_THREAD_EXIT,   /* (code)                                    encerra só a thread atual (a principal encerra o processo) */
    SYS_THREAD_JOIN,   /* (tid) -> code                             bloqueia até a thread terminar */
    SYS_GETTID,        /* () -> tid */
    SYS_FORK,          /* () -> pid do filho no pai, 0 no filho     memória copiada sob demanda (COW) */
    SYS_SIGACTION,     /* (sig, handler, restorer) -> handler antigo   0 = padrão, 1 = ignorar */
    SYS_SIGRETURN,     /* ()                                        volta do tratador (chamado pelo restorer) */
    SYS_SIGKILL,       /* (pid, sig) -> 0                           envia um sinal */
    SYS_DYLD,          /* 42: (op, a, b, c) -> ...                  dlopen em tempo de execução (MOS_DYLD_* abaixo) */
    SYS_NET,           /* 43: (op, a, b, c) -> ...                  TCP e DNS para os apps (MOS_NET_* abaixo, kernel/net/usersock.c) */
    SYS_SPAWNVEC,      /* 44: (path, mos_spawn_vec_t*) -> pid       argv e envp completos (posix_spawn do Electron) */
    SYS_EXECVE,        /* 45: (path, mos_spawn_vec_t*) -> só volta com erro   troca a imagem (mesmo pid, fds e sessão) */
    SYS_WAITPID,       /* 46: (pid, int *status, opções) -> pid      status POSIX; pid -1 = qualquer filho; MOS_WNOHANG */
    SYS_FDPATH,        /* 47: (fd, buf, cap) -> tamanho             caminho do arquivo aberto (F_GETPATH, fchdir) */
    /* futex: espera numa palavra de 32 bits do processo (pthread_mutex/cond e semáforos do libSystem) */
    SYS_FUTEX_WAIT,    /* 48: (u32 *end, valor, prazo ms; ~0 = sem) -> 0 acordada, -MOS_EAGAIN valor já mudou, -MOS_ETIMEDOUT */
    SYS_FUTEX_WAKE,    /* 49: (u32 *end, n) -> quantas threads acordou */
    SYS_FDFLAGS,       /* 50: (fd, mudar, valor) -> flags        FD_CLOEXEC por fd: mudar=1 grava valor (0/1); devolve 1 se close-on-exec */
    SYS_PREAD,         /* 51: (fd, buf, n, deslocamento em r10) -> lidos      sem mexer na posição do fd (atômico entre threads) */
    SYS_PWRITE,        /* 52: (fd, buf, n, deslocamento em r10) -> escritos */
    SYS_FTRUNCATE,     /* 53: (fd, tamanho) -> 0                    muda o tamanho de um arquivo aberto (zeros ao crescer) */
    SYS_IMGATTACH,     /* 54: (caminho .dmg, buf, cap) -> nº de volumes     monta em /Volumes; buf recebe os caminhos, um por linha (hdiutil) */
    SYS_IMGDETACH,     /* 55: (ponto de montagem) -> 0              desmonta um volume de /Volumes */
    SYS_TTY,           /* 56: (op, a, b, c) -> ...                  pseudoterminais, sessões e grupos de processos (MOS_TTY_* abaixo, kernel/proc/pty.c) */
    SYS_LS,            /* 57: (op, mos_ls_req_t*) -> resultado      Launch Services: tipos, apps padrão, abrir (MOS_LS_* abaixo, kernel/compat/launchsvc.c) */
    SYS_SIG,           /* 58: (op, a, b, c) -> ...                  sinais POSIX completos: ações, máscaras, sigsuspend, sigaltstack (MOS_SIGOP_* abaixo) */
    SYS_COUNT,
    SYS_GUI_FIRST = 64,/* 64..79: interface gráfica dos apps do macOS (kernel/compat/guisys.c) */
    SYS_GUI_LAST = 79,
    /* 80..95: portas Mach entre processos (kernel/proc/machipc.c; libSystem: userland/libsystem/mach.c) */
    SYS_MACH_FIRST = 80,
    SYS_MACH_MSG = 80,      /* (mos_mach_msg_args_t*) -> kern_return_t      mach_msg completo (envio e/ou recepção) */
    SYS_MACH_PORT = 81,     /* (op, u64 args[8]) -> kern_return_t           direitos e atributos (MACH_OP_* abaixo) */
    SYS_MACH_BOOTSTRAP = 82,/* (op, nome, u32 *porta) -> código do bootstrap  registro global de serviços */
    SYS_MACH_VM = 83,       /* (op, u64 args[8]) -> kern_return_t           entradas de memória compartilhada */
    SYS_MACH_LAST = 95
};

/* SYS_SPAWNVEC: vetores no espaço do pai. argv e envp são char *const * terminados em NULL.
 * envp 0 = o ambiente curto padrão (HOME, PATH, …). O Electron passa os dois completos:
 * a linha de comando leva a chave do rendezvous Mach e o ambiente leva a política de validação.
 * acts são as posix_spawn_file_actions (dup2/close/open/chdir), na ordem. */
/* SYS_MUNMAP com c = MOS_MUNMAP_KEEP: o mmap(MAP_FIXED|MAP_ANON) do libSystem troca um trecho que ainda é do chamador
 * por páginas zeradas. As páginas saem, mas a faixa não volta ao alocador de endereços (mmap_next): sem isto, um trecho
 * no topo virava espaço livre, o próximo mmap de outro componente caía nele, e o dono, ao ligar o trecho de novo
 * (mprotect), herdava as páginas do outro (o Claude Code faz MAP_FIXED no topo logo na partida).
 * c = MOS_MUNMAP_DISCARD: o madvise(MADV_DONTNEED/FREE/FREE_REUSABLE) do libSystem. Só saem as páginas privadas de
 * faixas anônimas sob demanda, que voltam zeradas no próximo toque; nada é desmapeado de fato (proc/process.c) */
#define MOS_MUNMAP_KEEP    0x4B454550u   /* 'KEEP' */
#define MOS_MUNMAP_DISCARD 0x44495343u   /* 'DISC' */
#define MOS_SPAWN_FA_MAX 48
#define MOS_SPAWN_FA_DUP2  1
#define MOS_SPAWN_FA_CLOSE 2
#define MOS_SPAWN_FA_OPEN  3
#define MOS_SPAWN_FA_CHDIR 4
#define MOS_SPAWN_CLOEXEC_DEFAULT 0x4000u   /* o filho começa sem os fds do pai; só as ações abrem */
#define MOS_SPAWN_SETPGROUP 0x0002u          /* (valores do macOS) o filho vira líder de um grupo de processos novo */
#define MOS_SPAWN_SETSID    0x0400u          /* o filho começa uma sessão nova, sem terminal de controle (node-pty) */
typedef struct {
    int kind, a, b, flags;
    unsigned mode;
    char path[256];
} mos_spawn_fa_t;
typedef struct {
    unsigned long long argv;
    unsigned long long envp;
    unsigned long long acts;     /* mos_spawn_fa_t[nacts] no pai */
    unsigned int nacts;
    unsigned int flags;
} mos_spawn_vec_t;

/* SYS_MACH_MSG: os argumentos do mach_msg não cabem nos 5 registradores */
typedef struct {
    unsigned long long msg;
    unsigned int option, send_size, rcv_size, rcv_name, timeout, notify;
} mos_mach_msg_args_t;

/* SYS_MACH_PORT: operação em rdi, vetor u64 a[8] (entrada e saída) em rsi */
enum {
    MACH_OP_ALLOCATE = 1,   /* a0 = right, a1 = nome desejado (0 = qualquer) -> a2 = nome */
    MACH_OP_DEALLOCATE,     /* a0 = nome */
    MACH_OP_MOD_REFS,       /* a0 = nome, a1 = right, a2 = delta */
    MACH_OP_INSERT_RIGHT,   /* a0 = nome, a1 = poly, a2 = disposição */
    MACH_OP_TYPE,           /* a0 = nome -> a1 = MACH_PORT_TYPE_* */
    MACH_OP_DESTROY,        /* a0 = nome */
    MACH_OP_GET_REFS,       /* a0 = nome, a1 = right -> a2 = urefs */
    MACH_OP_INSERT_MEMBER,  /* a0 = porta, a1 = conjunto */
    MACH_OP_EXTRACT_MEMBER, /* a0 = porta, a1 = conjunto */
    MACH_OP_MOVE_MEMBER,    /* a0 = porta, a1 = conjunto (0 = sai do conjunto atual) */
    MACH_OP_NOTIFY,         /* a0 = nome, a1 = variante, a2 = sync, a3 = notify, a4 = disposição -> a5 = anterior */
    MACH_OP_SET_ATTR,       /* a0 = nome, a1 = flavor, a2 = valor (qlimit) */
    MACH_OP_STATUS,         /* a0 = nome -> a1 = msgcount, a2 = seqno, a3 = qlimit, a4 = srights, a5 = sorights, a6 = mscount, a7 = conjunto */
    MACH_OP_SET_CONTEXT,    /* a0 = nome, a1 = contexto */
    MACH_OP_GET_CONTEXT,    /* a0 = nome -> a1 = contexto */
    MACH_OP_PENDING,        /* a0 = nome -> a1 = mensagens na fila, a2 = nome do membro com mensagem (conjuntos) */
    MACH_OP_WAIT,           /* a0 = nome, a1 = prazo em ms (~0 = sem prazo): bloqueia até haver mensagem, sem receber */
    MACH_OP_PEEK,           /* a0 = nome -> a1 = tamanho, a2 = seqno, a3 = id */
    MACH_OP_DESTRUCT,       /* a0 = nome, a1 = delta de envio (<= 0) */
    MACH_OP_EXTRACT_RIGHT,  /* a0 = nome, a1 = disposição -> a2 = nome novo, a3 = tipo */
    MACH_OP_GET_SPECIAL,    /* a0 = qual (TASK_*_PORT) -> a1 = nome */
    MACH_OP_SET_SPECIAL,    /* a0 = qual, a1 = nome */
    MACH_OP_SPAWN_SPECIAL,  /* a0 = qual (4 = bootstrap), a1 = nome: porta especial do PRÓXIMO filho (posix_spawnattr_setspecialport_np) */
    MACH_OP_CONSTRUCT,      /* a0 = flags MPO_*, a1 = qlimit, a2 = contexto -> a3 = nome */
    /* fileports: um descritor que viaja como direito de envio (o Mojo do Chromium passa arquivos assim) */
    MACH_OP_FILEPORT_MAKE,  /* a0 = fd -> a1 = nome (envio de uma porta que guarda uma cópia do descritor) */
    MACH_OP_FILEPORT_FD,    /* a0 = nome -> a1 = fd novo neste processo (mesmo arquivo/pipe, mesmo modo) */
};

/* SYS_MACH_BOOTSTRAP */
enum { MACH_BS_CHECK_IN = 1, MACH_BS_LOOK_UP, MACH_BS_REGISTER,
       MACH_BS_RESERVE };   /* launchd: guarda o direito de receber até o check_in do daemon; volta a ele quando o daemon morre */

/* SYS_MACH_VM */
enum {
    MACH_VM_MAKE_ENTRY = 1, /* a0 = tamanho, a1 = deslocamento/endereço, a2 = perm, a3 = pai -> a4 = nome, a5 = tamanho */
    MACH_VM_MAP,            /* a0 = endereço (entrada se fixo), a1 = tamanho, a2 = flags, a3 = objeto, a4 = deslocamento, a5 = prot -> a0 */
    MACH_VM_ENTRY_INFO,     /* a0 = objeto -> a1 = tamanho */
    MACH_VM_PROTECT = 16,   /* (16+: longe das MACH_VM_CA_* da sessão CoreAnimation, 4..) */
                            /* a0 = endereço, a1 = tamanho, a2 = prot (VM_PROT_*): registra a proteção (mprotect) */
    MACH_VM_REGION = 17,    /* a0 = endereço -> a1 = início, a2 = tamanho, a3 = prot, a4 = prot máxima */
    MACH_VM_CA_ALLOC = 4,   /* () -> a0 = id de CAContext, único no sistema */
    MACH_VM_CA_PUBLISH,     /* a0 = id, a1 = nome da entrada de memória -> a2 = geração */
    MACH_VM_CA_GEN,         /* a0 = id -> a1 = geração (0 = ninguém publicou) */
    MACH_VM_CA_LOOKUP,      /* a0 = id -> a1 = nome (envio), a2 = tamanho, a3 = geração */
    MACH_VM_CA_DROP,        /* a0 = id: o dono tira o contexto do registro */
};

#define MOS_O_RDONLY 0x0000
#define MOS_O_WRONLY 0x0001
#define MOS_O_RDWR   0x0002
#define MOS_O_CREAT  0x0040
#define MOS_O_TRUNC  0x0200
#define MOS_O_APPEND 0x0400
#define MOS_O_NOCTTY 0x0100      /* abrir um terminal não o torna o terminal de controle */
#define MOS_O_NONBLOCK 0x0800    /* leitura/escrita que esperaria devolve -EAGAIN (o kernel só consulta em terminais;
                                    nos pipes o libSystem decide pelo SYS_FSTAT) */
#define MOS_O_CLOEXEC 0x1000000  /* valor do macOS: o fd fecha no execve */
#define MOS_WNOHANG  1           /* SYS_WAITPID: não bloqueia (devolve 0) */

/* sinais (números do Linux/BSD para os comuns) */
#define MOS_SIGHUP   1
#define MOS_SIGINT   2
#define MOS_SIGQUIT  3
#define MOS_SIGILL   4
#define MOS_SIGFPE   8
#define MOS_SIGKILL  9
#define MOS_SIGUSR1 10
#define MOS_SIGSEGV 11
#define MOS_SIGUSR2 12
#define MOS_SIGPIPE 13
#define MOS_SIGALRM 14
#define MOS_SIGTERM 15
#define MOS_SIGURG  16
#define MOS_SIGCHLD 17
#define MOS_SIGTSTP 18           /* os de terminal têm os números do macOS (o libSystem só troca USR1/USR2/CHLD/STOP/BUS/SYS) */
#define MOS_SIGCONT 19
#define MOS_SIGSTOP 20
#define MOS_SIGTTIN 21
#define MOS_SIGTTOU 22
#define MOS_SIGIO   23
#define MOS_SIGWINCH 28
#define MOS_SIGINFO 29
#define MOS_NSIG    32
#define MOS_SIG_DFL 0
#define MOS_SIG_IGN 1
#define MOS_SIGTRAP  5           /* int3 / passo a passo */
#define MOS_SIGABRT  6
#define MOS_SIGBUS  30           /* números do kernel: o libSystem troca BUS 10 <-> 30 e SYS 12 <-> 31 (bijeção em libc.c) */
#define MOS_SIGSYS  31

/* SYS_SIG: sinais POSIX (kernel/proc/process.c, bloco "sinais"; libSystem: libc.c). Conjuntos são u32 com o bit
 * (1 << sinal) na numeração do kernel. Cada thread tem máscara e pendentes próprios; o processo tem as ações e
 * os pendentes de kill() (vão para qualquer thread que não os bloqueie). A entrega acontece na volta ao ring 3:
 * o kernel monta na pilha (ou na sigaltstack, com SA_ONSTACK) um quadro com siginfo, ucontext e mcontext no
 * layout do Darwin x86-64 (mos_ucontext_t abaixo) e:
 *   - com trampolim (ação instalada por MOS_SIGOP_ACTION com tramp != 0, o caso do libSystem): desvia para
 *     tramp(handler, estilo, sinal, siginfo*, ucontext*) como o _sigtramp do macOS; estilo 1 = UC_TRAD, 30 =
 *     UC_FLAVOR (SA_SIGINFO). O trampolim chama o tratador e volta por MOS_SIGOP_RETURN(ucontext*).
 *   - sem trampolim (SYS_SIGACTION antigo, userland/mos.h): chama handler(sinal, siginfo*, ucontext*) com o
 *     restorer como endereço de retorno; o restorer chama SYS_SIGRETURN.
 * A volta restaura registradores, FPU/SSE (o __fs é a imagem FXSAVE) e a máscara de uc_sigmask; quem mexer no
 * mcontext (ex.: __ss.__rip) retoma noutro ponto. Uma syscall bloqueante interrompida devolve -MOS_EINTR, ou é
 * refeita do zero se a ação tem SA_RESTART (read/write de pipe e terminal, wait/waitpid, futex). Faltas (#PF,
 * #GP, #DE, #UD...) viram SIGSEGV/SIGBUS/SIGFPE/SIGILL/SIGTRAP com si_addr; bloqueadas ou sem tratador, matam. */
enum {
    MOS_SIGOP_ACTION = 1,  /* (sinal, const mos_sigaction_t *nova, mos_sigaction_t *antiga) -> 0 */
    MOS_SIGOP_MASK,        /* (como, const u32 *conjunto, u32 *antigo) -> 0       máscara da thread (MOS_SIG_BLOCK...) */
    MOS_SIGOP_PENDING,     /* (u32 *saída) -> 0                                  pendentes da thread e do processo */
    MOS_SIGOP_SUSPEND,     /* (máscara) -> -MOS_EINTR                            troca a máscara e espera um tratador rodar */
    MOS_SIGOP_ALTSTACK,    /* (const mos_sigstack_t *nova, mos_sigstack_t *antiga) -> 0 */
    MOS_SIGOP_RETURN,      /* (mos_ucontext_t *) -> não volta                    fim do tratador (trampolim) */
    MOS_SIGOP_TKILL,       /* (tid, sinal) -> 0                                  pthread_kill: uma thread do mesmo processo */
    MOS_SIGOP_WAIT,        /* (conjunto, mos_siginfo_t *saída, prazo ms; ~0 = sem) -> sinal | -MOS_EAGAIN (prazo) | -MOS_EINTR */
};
#define MOS_SIG_BLOCK   1        /* valores do macOS */
#define MOS_SIG_UNBLOCK 2
#define MOS_SIG_SETMASK 3
/* SYS_PIPE: 2º argumento que pede um par bidirecional (socketpair) em vez de um pipe; mágico porque o SYS1 dos programas
 * que chamam SYS_PIPE com um argumento só deixa lixo no rsi */
#define MOS_PIPE_SOCKPAIR 0x534F434B50414952ULL   /* "SOCKPAIR" */
/* SYS_DUP2(fd, MOS_DUP_ANY, mínimo): dup()/F_DUPFD. O kernel escolhe o menor fd livre >= mínimo (>= 3) e copia na mesma
 * syscall; escolher no libSystem e chamar dup2 depois deixava outra thread abrir aquele número no meio, e o dup2 o
 * substituía em silêncio */
#define MOS_DUP_ANY 0x7FFFFFFF
#define MOS_SA_ONSTACK   0x0001  /* flags de sa_flags: valores do macOS */
#define MOS_SA_RESTART   0x0002
#define MOS_SA_RESETHAND 0x0004
#define MOS_SA_NOCLDSTOP 0x0008
#define MOS_SA_NODEFER   0x0010
#define MOS_SA_NOCLDWAIT 0x0020
#define MOS_SA_SIGINFO   0x0040
#define MOS_SS_ONSTACK   1
#define MOS_SS_DISABLE   4
typedef struct { unsigned long long handler, tramp; unsigned int mask, flags; } mos_sigaction_t;
typedef struct { unsigned long long sp, size; int flags, pad; } mos_sigstack_t;   /* stack_t do Darwin (ss_sp, ss_size, ss_flags) */
/* layout do Darwin x86-64: __siginfo (104), __darwin_ucontext (56), __darwin_mcontext64 (712) */
typedef struct {
    int si_signo, si_errno, si_code, si_pid;
    unsigned int si_uid; int si_status;
    unsigned long long si_addr, si_value;
    long long si_band; unsigned long long pad[7];
} mos_siginfo_t;
typedef struct {
    unsigned long long rax, rbx, rcx, rdx, rdi, rsi, rbp, rsp, r8, r9, r10, r11, r12, r13, r14, r15, rip, rflags, cs, fs, gs;
} mos_thread_state_t;            /* x86_thread_state64_t */
typedef struct {
    unsigned short trapno, cpu; unsigned int err; unsigned long long faultvaddr;   /* __es: x86_exception_state64_t */
    mos_thread_state_t ss;
    int fpu_reserved[2]; unsigned char fx[512]; int fpu_reserved1;   /* __fs: x86_float_state64_t (a imagem FXSAVE) */
    int pad;
} mos_mcontext_t;
typedef struct {
    int uc_onstack; unsigned int uc_sigmask;
    mos_sigstack_t uc_stack;
    unsigned long long uc_link, uc_mcsize, uc_mcontext;   /* uc_mcontext aponta para o mos_mcontext_t */
} mos_ucontext_t;
#define MOS_SI_USER     0x10001  /* si_code (valores do macOS) */
#define MOS_SI_QUEUE    0x10002
#define MOS_SI_TIMER    0x10003
#define MOS_SEGV_MAPERR 1
#define MOS_SEGV_ACCERR 2
#define MOS_BUS_ADRALN  1
#define MOS_BUS_ADRERR  2
#define MOS_FPE_INTDIV  7
#define MOS_FPE_FLTINV  5
#define MOS_ILL_ILLOPC  1
#define MOS_TRAP_BRKPT  1
#define MOS_CLD_EXITED  1
#define MOS_CLD_KILLED  2

/* SYS_DYLD: carregador Mach-O em tempo de execução (kernel/compat/macho.c, userland/libsystem/dlfcn.c).
 * Imagens são índices (0 = executável, na ordem de carga). Só processos Mach-O; senão -EINVAL. */
enum {
    MOS_DYLD_OPEN = 1, /* (path, mos_dyld_open_t*, endereço de quem chamou) -> índice | 0x10000 (libSystem) | -errno
                          carrega MH_DYLIB/MH_BUNDLE e dependências; @rpath/@loader_path relativos a quem chamou */
    MOS_DYLD_SYM,      /* (handle, nome C sem "_", endereço de quem chamou) -> endereço ou 0 */
    MOS_DYLD_CLOSE,    /* (índice) -> 0                              nada é descarregado */
    MOS_DYLD_COUNT,    /* () -> número de imagens */
    MOS_DYLD_IMAGE,    /* (índice, buf, cap) -> endereço do cabeçalho Mach-O (0 = não existe); buf recebe o caminho */
    MOS_DYLD_FIND,     /* (endereço) -> índice da imagem que o contém ou -ESRCH */
    MOS_DYLD_ERROR,    /* (buf, cap) -> tamanho da mensagem do último erro */
};
#define MOS_RTLD_NOLOAD    0x10      /* flags de mos_dyld_open_t (valores do macOS) */
#define MOS_RTLD_NEXT      (-1)      /* handles especiais do dlsym */
#define MOS_RTLD_DEFAULT   (-2)
#define MOS_RTLD_SELF      (-3)
#define MOS_RTLD_MAIN_ONLY (-5)
typedef struct {
    unsigned long long flags;            /* entrada: RTLD_* */
    unsigned long long inits, ninits;    /* saída: inicializadores das imagens novas (ponteiros absolutos, em ordem) */
    unsigned long long objc, nobjc;      /* saída: seções ObjC {addr, size, kind}[nobjc] para cx_objc_init_image */
    unsigned long long first, nnew;      /* saída: índices das imagens novas: [first, first + nnew) */
} mos_dyld_open_t;

/* SYS_NET: a pilha TCP/DNS do kernel (kernel/net) para os sockets AF_INET do libSystem (userland/libsystem/sockets.c).
 * Tudo é não-bloqueante: quem chama espera (SYS_SLEEP) e pergunta de novo. Conexões são do processo que as abriu
 * e fecham quando ele sai. IPs em ordem de host (10.0.2.15 = 0x0A00020F). */
enum {
    MOS_NET_RESOLVE = 1, /* (nome, u32 *ip) -> 1 pronto, 0 pendente, -1 falha     registros A, com cache */
    MOS_NET_CONNECT,     /* (ip, porta) -> conexão | -errno                     inicia o SYN */
    MOS_NET_STATE,       /* (conexão) -> MOS_NET_ST_*                            */
    MOS_NET_SEND,        /* (conexão, buf, n) -> bytes aceitos (0 = buffer cheio) | -errno */
    MOS_NET_RECV,        /* (conexão, buf, max) -> bytes; 0 = nada ainda; -1 = fim (fechada e vazia) */
    MOS_NET_CLOSE,       /* (conexão) -> 0 */
    MOS_NET_INFO,        /* (u32 out[4]) -> 1 se a rede está configurada: ip, máscara, gateway, dns */
    MOS_NET_AVAIL,       /* (conexão) -> bytes recebidos esperando leitura (>= 0) | -errno */
    /* TLS do kernel (net/tls.c) sobre um canal de memória, para o SecureTransport do libSystem (security.c): o app
     * move os bytes cifrados pelas próprias callbacks; o kernel faz o handshake, valida a cadeia e cifra. */
    MOS_NET_TLS_NEW,       /* (sni) -> sessão | -errno */
    MOS_NET_TLS_FEED,      /* (sessão, buf, n) -> bytes aceitos; n = 0 marca o fim do fluxo de entrada */
    MOS_NET_TLS_DRAIN,     /* (sessão, buf, max) -> bytes cifrados a enviar */
    MOS_NET_TLS_HANDSHAKE, /* (sessão) -> 1 pronto, 0 precisa de mais entrada, -1 erro */
    MOS_NET_TLS_SEND,      /* (sessão, buf, n) -> bytes aceitos | -1 */
    MOS_NET_TLS_RECV,      /* (sessão, buf, max) -> bytes; 0 = precisa de mais entrada; -1 = fim ou erro */
    MOS_NET_TLS_ERROR,     /* (sessão, buf, max) -> tamanho da mensagem de erro */
    MOS_NET_TLS_FREE,      /* (sessão, buf, max) -> bytes finais (close_notify) copiados para buf */
    MOS_NET_TLS_ROOTS,     /* (buf, max) -> tamanho do PEM das raízes da Mozilla (buf = 0: só o tamanho) */
    /* IPv6, servidor TCP e laço local (kernel/net/ipv6.c, tcp.c). Endereços: 16 bytes em ordem de rede; IPv4 vai
     * mapeado (::ffff:a.b.c.d). */
    MOS_NET_RESOLVE6,      /* (nome, u8 out[16]) -> 1 pronto, 0 pendente, -1 falha     registros AAAA */
    MOS_NET_CONNECT6,      /* (u8 *end16, porta) -> conexão | -errno (-MOS_ECONNREFUSED: local sem escuta) */
    MOS_NET_LISTEN,        /* (u8 *end16 ou 0, porta (0 = efêmera), backlog | famílias MOS_NET_L_* << 16) -> ouvinte | -errno */
    MOS_NET_ACCEPT,        /* (ouvinte) -> conexão | -MOS_EAGAIN (nenhuma pronta); MOS_NET_AVAIL no ouvinte = prontas */
    MOS_NET_ADDRS,         /* (conexão ou ouvinte, mos_net_addrs_t *) -> 0 */
    MOS_NET_INFO6,         /* (mos_net6_info_t *) -> 1 se há IPv6 global com roteador padrão */
    MOS_NET_ICMP_SEND,     /* (u8 *end16, msg, n) -> n | -errno   eco (tipo 8/128) montado pelo app; o kernel soma */
    MOS_NET_ICMP_RECV,     /* (id, buf, max) -> bytes: 16 de origem + a resposta de eco; 0 = nada */
};
enum { MOS_NET_ST_CLOSED, MOS_NET_ST_CONNECTING, MOS_NET_ST_OPEN, MOS_NET_ST_PEER_CLOSED, MOS_NET_ST_ERROR };
#define MOS_NET_L_V4 1     /* MOS_NET_LISTEN: aceita IPv4 */
#define MOS_NET_L_V6 2     /* aceita IPv6 */
typedef struct { unsigned char lip[16], rip[16]; unsigned short lport, rport; } mos_net_addrs_t;
#define MOS_NET6_LL     1  /* mos_net6_info_t.flags */
#define MOS_NET6_GLOBAL 2
#define MOS_NET6_ROUTER 4
#define MOS_NET6_DNS    8
typedef struct { unsigned char ll[16], global[16], router[16], dns[16]; unsigned flags, prefix_len; } mos_net6_info_t;
#define MOS_ENETUNREACH 51
#define MOS_ECONNREFUSED 61
#define MOS_EADDRINUSE 48
#define MOS_EADDRNOTAVAIL 49

/* SYS_TTY: pseudoterminais (kernel/proc/pty.c; libSystem: userland/libsystem/pty.c).
 * SYS_OPEN de "/dev/ptmx" cria um par e devolve o mestre; "/dev/ttysNNN" abre o escravo (depois do unlockpt);
 * "/dev/tty" abre o terminal de controle. Um líder de sessão sem terminal que abre um escravo sem
 * MOS_O_NOCTTY o adota (como no macOS: é o que o spawn-helper do node-pty faz). Escrever no mestre é digitar
 * (disciplina de linha: ICANON, ECHO, ISIG, ICRNL...); o que o escravo escreve sai lido no mestre (OPOST/ONLCR).
 * SYS_FSTAT num terminal: is_dir = 5, size = bytes que um read devolve agora, mtime = 1 se o read devolveria
 * fim de arquivo (a outra ponta fechou, ou ^D numa linha vazia). Sessões e grupos seguem o POSIX; filhos do
 * kernel e do init começam uma sessão própria (como os jobs do launchd). */
enum {
    MOS_TTY_IOCTL = 1,   /* (fd, pedido, arg) -> 0 | -errno     pedidos ioctl do macOS (TIOCGETA, TIOCSWINSZ, TIOCSCTTY,
                            TIOCPTYGNAME, FIONREAD...); -MOS_ENOTTY se o fd não é terminal, -MOS_EBADF se não é do kernel */
    MOS_TTY_NAME,        /* (fd, buf, cap) -> tamanho            "/dev/ttysNNN" (escravo ou mestre de um par) */
    MOS_TTY_SETFL,       /* (fd, nonblock) -> 0                  liga/desliga MOS_O_NONBLOCK num fd do kernel (fcntl F_SETFL) */
    MOS_TTY_SETSID,      /* () -> sid                            -MOS_EPERM se já é líder de grupo */
    MOS_TTY_SETPGID,     /* (pid, pgid) -> 0                     0 = o próprio / o pid */
    MOS_TTY_GETPGID,     /* (pid) -> pgid                        pid 0 = o próprio */
    MOS_TTY_GETSID,      /* (pid) -> sid */
    MOS_TTY_KILLPG,      /* (pgrp, sinal) -> 0 | -MOS_ESRCH      pgrp 0 = o grupo de quem chama; sinal 0 só pergunta */
    MOS_TTY_PROCINFO,    /* (pid, i32 out[4]) -> 0               pgid, sid, terminal de controle (índice ou -1), grupo
                            em primeiro plano dele (ou -1): o sysctl KERN_PROC (e_pgid, e_tdev, e_tpgid) */
};
#define MOS_PTY_MAX 64           /* pares simultâneos: /dev/ttys000..063 */
#define MOS_EPERM   1
#define MOS_EINTR   4
#define MOS_EIO     5
#define MOS_ENXIO   6
#define MOS_EACCES 13
#define MOS_ENOTTY 25

#define MOS_SEEK_SET 0
#define MOS_SEEK_CUR 1
#define MOS_SEEK_END 2

#define MOS_ENOENT  2
#define MOS_EBADF   9
#define MOS_ENOMEM 12
#define MOS_EFAULT 14
#define MOS_EEXIST 17
#define MOS_EISDIR 21
#define MOS_EINVAL 22
#define MOS_EMFILE 24
#define MOS_ENOSPC 28
#define MOS_ESRCH   3
#define MOS_ECHILD 10
#define MOS_ENOTDIR 20
#define MOS_ENOTEMPTY 66   /* valores do macOS */
#define MOS_EPIPE  32
#define MOS_ESPIPE 29            /* pread/pwrite num pipe */
#define MOS_EAGAIN 35            /* valores do macOS */
#define MOS_ETIMEDOUT 60

typedef struct {
    int pid, ppid, state;     /* state: 0 pronto, 1 rodando, 2 dormindo, 3 esperando */
    int pad;
    unsigned long long mem_kb, cpu_ms;
    char name[32];
} mos_procinfo_t;

typedef struct {
    unsigned long long size;
    unsigned long long is_dir; /* 0 arquivo, 1 pasta, 2/3 pipe (leitura/escrita), 4 /dev/null, 5 terminal (SYS_TTY) */
    long long mtime;          /* segundos desde 1970 (0 = desconhecido) */
} mos_stat_t;

/* SYS_LS: o banco do Launch Services fica no kernel (kernel/compat/launchsvc.c); o libSystem (openurl.c) faz as
 * APIs LS*, UTType e NSWorkspace sobre ele. Pedido e resposta no mesmo bloco. Listas em out: uma linha por app,
 * "identificador\tcaminho do .app (vazio = app do sistema)\tnome". Resultado < 0 = não achou / erro. */
enum {
    MOS_LS_DEFAULT_SCHEME = 1, /* key = esquema -> out = uma linha */
    MOS_LS_ALL_SCHEME,         /* key = esquema -> out = linhas, o padrão primeiro */
    MOS_LS_DEFAULT_TYPE,       /* key = UTI, roles -> out = uma linha */
    MOS_LS_ALL_TYPE,           /* key = UTI, roles */
    MOS_LS_DEFAULT_FILE,       /* key = caminho, roles */
    MOS_LS_ALL_FILE,           /* key = caminho, roles */
    MOS_LS_SET_SCHEME,         /* key = esquema, key2 = identificador; http/https: o sistema pergunta ao usuário */
    MOS_LS_SET_TYPE,           /* key = UTI, key2 = identificador, roles */
    MOS_LS_TYPE_FOR_TAG,       /* key = etiqueta, key2 = classe ("public.filename-extension"...), key3 = conformidade -> out = UTI */
    MOS_LS_TAG_FOR_TYPE,       /* key = UTI, key2 = classe -> out = etiquetas, uma por linha (a preferida primeiro) */
    MOS_LS_CONFORMS,           /* key = UTI, key2 = UTI -> 1/0 */
    MOS_LS_TYPE_INFO,          /* key = UTI -> out = "descrição\npais separados por espaço"; < 0 se o tipo não existe */
    MOS_LS_TYPE_OF_FILE,       /* key = caminho -> out = UTI */
    MOS_LS_APP_BY_ID,          /* key = identificador -> out = uma linha */
    MOS_LS_APP_BY_NAME,        /* key = nome -> out = uma linha */
    MOS_LS_APP_BY_PATH,        /* key = caminho do .app (registra se preciso) -> out = uma linha */
    MOS_LS_OPEN,               /* key = caminho ou URL, key2 = app (identificador, ou caminho se flags & 1) ou vazio -> 0 */
    MOS_LS_KIND_OF_FILE,       /* key = caminho -> out = "Imagem PNG" */
};
typedef struct {
    unsigned int roles, flags;
    char key[1024], key2[512], key3[256];
    char out[8192];
} mos_ls_req_t;
