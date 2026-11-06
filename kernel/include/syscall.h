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
    SYS_MUNMAP,        /* (addr, len) -> 0 */
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
    SYS_PIPE,          /* (int fds[2]) -> 0                        fds[0] leitura, fds[1] escrita */
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
};

/* SYS_MACH_BOOTSTRAP */
enum { MACH_BS_CHECK_IN = 1, MACH_BS_LOOK_UP, MACH_BS_REGISTER };

/* SYS_MACH_VM */
enum {
    MACH_VM_MAKE_ENTRY = 1, /* a0 = tamanho, a1 = deslocamento/endereço, a2 = perm, a3 = pai -> a4 = nome, a5 = tamanho */
    MACH_VM_MAP,            /* a0 = endereço (entrada se fixo), a1 = tamanho, a2 = flags, a3 = objeto, a4 = deslocamento, a5 = prot -> a0 */
    MACH_VM_ENTRY_INFO,     /* a0 = objeto -> a1 = tamanho */
};

#define MOS_O_RDONLY 0x0000
#define MOS_O_WRONLY 0x0001
#define MOS_O_RDWR   0x0002
#define MOS_O_CREAT  0x0040
#define MOS_O_TRUNC  0x0200
#define MOS_O_APPEND 0x0400

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
#define MOS_SIGCHLD 17
#define MOS_NSIG    32
#define MOS_SIG_DFL 0
#define MOS_SIG_IGN 1

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
#define MOS_EPIPE  32

typedef struct {
    int pid, ppid, state;     /* state: 0 pronto, 1 rodando, 2 dormindo, 3 esperando */
    int pad;
    unsigned long long mem_kb, cpu_ms;
    char name[32];
} mos_procinfo_t;

typedef struct {
    unsigned long long size;
    unsigned long long is_dir;
    long long mtime;          /* segundos desde 1970 (0 = desconhecido) */
} mos_stat_t;
