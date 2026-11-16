/*
 * mOS libSystem — IPC System V (semget/semop/semctl, shmget/shmat/shmdt/shmctl, msgget/msgsnd/msgrcv/msgctl) entre
 * processos, com o contrato e os layouts do macOS (sys/sem.h, sys/shm.h, sys/msg.h, #pragma pack(4)).
 * docs/darwin-bsd-mach.md, fase F7.
 *
 * Cada objeto é um arquivo em /tmp/.sysv-ipc ("sem.<id>", "shm.<id>", "msg.<id>") mapeado MAP_SHARED; a chave
 * (key_t) aponta para o id por um arquivo "<tipo>k.<chave>". Os processos veem a mesma página e quem espera dorme no
 * futex de uma palavra de geração dentro dela (o kernel acorda entre processos numa página MAP_SHARED, PR #236).
 * Uma trava curta (spin + futex) no cabeçalho protege as operações. Limites: os padrões do macOS
 * (kern.sysv.*: semmsl 87, semopm 5, shmmax 4 MiB, shmseg 8, msgmnb 2048).
 */
#include "sys.h"

void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);

#define E_PERM 1
#define E_NOENT 2
#define E_INTR 4
#define E_2BIG 7
#define E_NOMEM 12
#define E_ACCES 13
#define E_EXIST 17
#define E_INVAL 22
#define E_NOSPC 28
#define E_FBIG 27
#define E_RANGE 34
#define E_AGAIN 35
#define E_IDRM 82
#define E_NOMSG 91
#define E_MFILE 24

#define IPC_CREAT  0x0200
#define IPC_EXCL   0x0400
#define IPC_NOWAIT 0x0800
#define IPC_PRIVATE 0
#define IPC_RMID 0
#define IPC_SET  1
#define IPC_STAT 2
#define SEM_UNDO 0x1000
#define GETNCNT 3
#define GETPID  4
#define GETVAL  5
#define GETALL  6
#define GETZCNT 7
#define SETVAL  8
#define SETALL  9
#define SHM_RDONLY 0x1000
#define SHM_RND    0x2000
#define MSG_NOERROR 0x1000

#define SEMMSL 87
#define SEMOPM 5
#define SEMVMX 32767
#define SHMMAX (4u << 20)
#define SHMSEG 8
#define MSGMNB 2048
#define MSGMAX 2048

#define O_RDWR_ 2
#define O_CREAT_ 0x200
#define O_EXCL_ 0x800
#define DIR "/tmp/.sysv-ipc"

int open(const char *path, int flags, ...);
int close(int fd);
ssize_t read(int fd, void *b, size_t n);
ssize_t write(int fd, const void *b, size_t n);
int unlink(const char *path);
int mkdir(const char *path, int mode);
int ftruncate(int fd, long len);
void *mmap(void *addr, size_t len, int prot, int flags, int fd, off_t off);
int munmap(void *a, size_t len);
int cx_futex_wait(volatile int *w, int expected, long ms);
int cx_futex_wake(volatile int *w, int n);
int snprintf(char *b, size_t n, const char *fmt, ...);
long time(long *t);
int getpid(void);

static int fail(int e) { cx_errno = e; return -1; }

/* struct ipc_perm do macOS (24 bytes) */
typedef struct { u32 uid, gid, cuid, cgid; u16 mode, seq; i32 key; } ipc_perm_t;

/* cabeçalho comum no começo da página 0 de todo objeto */
#define IPC_MAGIC 0x56535949u   /* "IYSV" */
typedef struct {
    u32 magic, kind;               /* 1 sem, 2 shm, 3 msg */
    volatile int lock;             /* 0 livre, 1 travado, 2 travado com espera */
    volatile int gen;              /* sobe a cada mudança: quem espera dorme aqui */
    volatile int removed;
    int id;
    ipc_perm_t perm;
    i64 otime, ctime, atime, dtime;
    int lpid, cpid, lrpid;
    u32 n;                         /* sem: nsems; shm: segsz; msg: qbytes */
    u32 nattch, used, qnum, waiters;
} ipc_hdr_t;
typedef struct { volatile int val; int pid; u16 ncnt, zcnt; } sem_ent_t;   /* depois do cabeçalho, nsems vezes */
typedef struct { i64 type; u32 len, pad; } msg_ent_t;                    /* fila: entradas seguidas de len bytes */
#define HDR 4096u
#define MSGAREA 8192u

static void lk(ipc_hdr_t *h) {
    int c = __sync_val_compare_and_swap(&h->lock, 0, 1);
    if (!c) return;
    if (c != 2) c = __sync_lock_test_and_set(&h->lock, 2);
    while (c) { cx_futex_wait(&h->lock, 2, -1); c = __sync_lock_test_and_set(&h->lock, 2); }
}
static void ulk(ipc_hdr_t *h) { if (__sync_fetch_and_sub(&h->lock, 1) != 1) { h->lock = 0; cx_futex_wake(&h->lock, 1); } }
static void bump(ipc_hdr_t *h) { __sync_fetch_and_add(&h->gen, 1); cx_futex_wake(&h->gen, 0x7fffffff); }

static const char *kname(int kind) { return kind == 1 ? "sem" : kind == 2 ? "shm" : "msg"; }
static void obj_path(int kind, int id, char *out, size_t cap) { snprintf(out, cap, DIR "/%s.%d", kname(kind), id); }
static void key_path(int kind, int key, char *out, size_t cap) { snprintf(out, cap, DIR "/%sk.%08x", kname(kind), (u32)key); }

/* mapeamentos deste processo: id -> cabeçalho (o mapeamento fica até o processo sair) */
#define MAXMAP 128
static struct { int kind, id; ipc_hdr_t *h; size_t len; } maps[MAXMAP];
static volatile int maps_lk;
static void ml(void) { while (__sync_lock_test_and_set(&maps_lk, 1)) SYS0(SYS_YIELD); }
static void mu(void) { __sync_lock_release(&maps_lk); }

static ipc_hdr_t *map_obj(int kind, int id) {
    ml();
    for (int i = 0; i < MAXMAP; i++) if (maps[i].h && maps[i].kind == kind && maps[i].id == id) { ipc_hdr_t *h = maps[i].h; mu(); return h; }
    mu();
    char p[96];
    obj_path(kind, id, p, sizeof p);
    int fd = open(p, O_RDWR_, 0);
    if (fd < 0) return 0;
    size_t len = HDR + (kind == 3 ? MSGAREA : 0);
    void *m = mmap(0, len, 3, 1, fd, 0);
    close(fd);
    if (m == (void *)-1 || !m) return 0;
    ipc_hdr_t *h = m;
    if (h->magic != IPC_MAGIC || (int)h->kind != kind) { munmap(m, len); return 0; }
    ml();
    for (int i = 0; i < MAXMAP; i++) if (!maps[i].h) { maps[i].kind = kind; maps[i].id = id; maps[i].h = h; maps[i].len = len; mu(); return h; }
    mu();
    return h;                                   /* tabela cheia: fica mapeado sem cache */
}
static ipc_hdr_t *get_live(int kind, int id) {
    if (id <= 0) { cx_errno = E_INVAL; return 0; }
    ipc_hdr_t *h = map_obj(kind, id);
    if (!h) { cx_errno = E_INVAL; return 0; }
    if (h->removed) { cx_errno = kind == 3 || kind == 1 ? E_IDRM : E_INVAL; return 0; }
    return h;
}

/* permissão: bits rwx do dono, grupo ou outros (root passa) */
static int may(const ipc_hdr_t *h, int want) {   /* want: 4 ler, 2 escrever (alterar) */
    u32 eu = (u32)SYS0(SYS_GETEUID), eg = (u32)SYS0(SYS_GETEGID);
    if (eu == 0) return 1;
    u16 m = h->perm.mode;
    int bits = (eu == h->perm.uid || eu == h->perm.cuid) ? (m >> 6) & 7 : (eg == h->perm.gid || eg == h->perm.cgid) ? (m >> 3) & 7 : m & 7;
    return (bits & want) == want;
}

/* cria ou acha o objeto da chave. *created = 1 quando acabou de criar (o chamador inicializa o resto). */
static int obtain(int kind, int key, int flg, size_t len, int *created, ipc_hdr_t **out) {
    mkdir("/tmp", 0777);
    mkdir(DIR, 0777);
    *created = 0;
    char kp[96], op[96];
    for (int tries = 0; tries < 64; tries++) {
        if (key != IPC_PRIVATE) {
            key_path(kind, key, kp, sizeof kp);
            int kfd = open(kp, O_RDWR_, 0);
            if (kfd >= 0) {
                int id = 0;
                ssize_t r = read(kfd, &id, 4);
                close(kfd);
                ipc_hdr_t *h = r == 4 ? map_obj(kind, id) : 0;
                if (h && !h->removed) {
                    if ((flg & IPC_CREAT) && (flg & IPC_EXCL)) return fail(E_EXIST);
                    *out = h;
                    return id;
                }
                if (r != 4) { SYS0(SYS_YIELD); continue; }   /* outro processo ainda gravando a chave */
                unlink(kp);                                  /* chave de objeto removido */
                continue;
            }
            if (!(flg & IPC_CREAT)) return fail(E_NOENT);
        }
        /* id novo: o primeiro nome livre (O_EXCL), a partir de um ponto que varia com o pid */
        int fd = -1, id = 0;
        u32 base = ((u32)getpid() * 131u) % 30000u;
        for (u32 i = 0; i < 32768 && fd < 0; i++) {
            id = (int)((base + i) % 32767u) + 1;
            obj_path(kind, id, op, sizeof op);
            fd = open(op, O_RDWR_ | O_CREAT_ | O_EXCL_, 0600);
        }
        if (fd < 0) return fail(E_NOSPC);
        if (ftruncate(fd, (long)len) != 0) { close(fd); unlink(op); return fail(E_NOMEM); }
        void *m = mmap(0, len, 3, 1, fd, 0);
        close(fd);
        if (m == (void *)-1 || !m) { unlink(op); return fail(E_NOMEM); }
        ipc_hdr_t *h = m;
        memset(h, 0, sizeof *h);
        h->kind = (u32)kind; h->id = id;
        u32 eu = (u32)SYS0(SYS_GETEUID), eg = (u32)SYS0(SYS_GETEGID);
        h->perm.uid = h->perm.cuid = eu; h->perm.gid = h->perm.cgid = eg;
        h->perm.mode = (u16)(flg & 0777); h->perm.key = key;
        h->ctime = time(0); h->cpid = getpid();
        if (key != IPC_PRIVATE) {
            int kfd = open(kp, O_RDWR_ | O_CREAT_ | O_EXCL_, 0666);
            if (kfd < 0) { munmap(m, len); unlink(op); continue; }   /* outro processo criou a chave junto: refaz */
            write(kfd, &id, 4);
            close(kfd);
        }
        ml();
        for (int i = 0; i < MAXMAP; i++) if (!maps[i].h) { maps[i].kind = kind; maps[i].id = id; maps[i].h = h; maps[i].len = len; break; }
        mu();
        *created = 1;
        *out = h;
        return id;
    }
    return fail(E_AGAIN);
}
static void publish(ipc_hdr_t *h) { __sync_synchronize(); h->magic = IPC_MAGIC; }
static void remove_obj(ipc_hdr_t *h) {
    h->removed = 1;
    char p[96];
    if (h->perm.key != IPC_PRIVATE) {
        key_path((int)h->kind, h->perm.key, p, sizeof p);
        int fd = open(p, O_RDWR_, 0), id = 0;
        if (fd >= 0) { if (read(fd, &id, 4) == 4 && id == h->id) unlink(p); close(fd); }
    }
    obj_path((int)h->kind, h->id, p, sizeof p);
    if (h->kind != 2 || !h->nattch) unlink(p);   /* shm com anexos: some quando o último sair (shmdt) */
    bump(h);
}
static void perm_out(const ipc_hdr_t *h, u8 *d) { memcpy(d, &h->perm, 24); }
static int perm_set(ipc_hdr_t *h, const u8 *s) {
    u32 eu = (u32)SYS0(SYS_GETEUID);
    if (eu != 0 && eu != h->perm.uid && eu != h->perm.cuid) return fail(E_PERM);
    ipc_perm_t p;
    memcpy(&p, s, 24);
    h->perm.uid = p.uid; h->perm.gid = p.gid; h->perm.mode = (u16)((h->perm.mode & ~0777) | (p.mode & 0777));
    h->ctime = time(0);
    return 0;
}
static void put64(u8 *b, u32 o, i64 v) { memcpy(b + o, &v, 8); }
static void put32(u8 *b, u32 o, u32 v) { memcpy(b + o, &v, 4); }

/* ================================================================ semáforos */
static sem_ent_t *sems_of(ipc_hdr_t *h) { return (sem_ent_t *)((u8 *)h + sizeof(ipc_hdr_t)); }

int semget(int key, int nsems, int flg) {
    if (nsems < 0 || nsems > SEMMSL) return fail(E_INVAL);
    ipc_hdr_t *h;
    int created;
    int id = obtain(1, key, flg, HDR, &created, &h);
    if (id < 0) return -1;
    if (created) {
        if (nsems == 0) { remove_obj(h); return fail(E_INVAL); }
        h->n = (u32)nsems;
        publish(h);
        return id;
    }
    if (nsems > (int)h->n) return fail(E_INVAL);
    if (!may(h, 4)) return fail(E_ACCES);
    return id;
}

/* SEM_UNDO: ajustes deste processo, desfeitos na saída (_exit chama cx_sysv_exit, libc.c; vale para exit e abort).
 * Morte por sinal não passa por aqui: o ajuste fica (o kernel do mOS não conhece o semáforo). */
#define MAXUNDO 64
static struct { int id; u16 num; int adj; } undo[MAXUNDO];
static int undo_any;
void cx_sysv_exit(void) {
    if (!undo_any) return;
    for (int i = 0; i < MAXUNDO; i++) {
        if (!undo[i].adj) continue;
        ipc_hdr_t *h = map_obj(1, undo[i].id);
        if (!h || h->removed || undo[i].num >= h->n) continue;
        lk(h);
        sem_ent_t *s = &sems_of(h)[undo[i].num];
        int v = s->val + undo[i].adj;
        s->val = v < 0 ? 0 : v > SEMVMX ? SEMVMX : v;
        ulk(h);
        bump(h);
        undo[i].adj = 0;
    }
}
static void undo_add(int id, u16 num, int delta) {
    undo_any = 1;
    int free_i = -1;
    for (int i = 0; i < MAXUNDO; i++) {
        if (undo[i].adj && undo[i].id == id && undo[i].num == num) { undo[i].adj -= delta; return; }
        if (!undo[i].adj && free_i < 0) free_i = i;
    }
    if (free_i >= 0) { undo[free_i].id = id; undo[free_i].num = num; undo[free_i].adj = -delta; }
}

typedef struct { u16 num; i16 op; i16 flg; } sembuf_t;
int semop(int id, void *ops_, size_t nops) {
    sembuf_t *ops = ops_;
    if (nops > SEMOPM) return fail(E_2BIG);
    if (!ops && nops) return fail(E_INVAL);
    ipc_hdr_t *h = get_live(1, id);
    if (!h) return -1;
    int alter = 0;
    for (size_t i = 0; i < nops; i++) { if (ops[i].num >= h->n) return fail(E_FBIG); if (ops[i].op) alter = 1; }
    if (!may(h, alter ? 2 : 4)) return fail(E_ACCES);
    for (;;) {
        lk(h);
        if (h->removed) { ulk(h); return fail(E_IDRM); }
        sem_ent_t *s = sems_of(h);
        size_t blk = nops;
        for (size_t i = 0; i < nops && blk == nops; i++) {   /* tudo ou nada: confere antes de aplicar */
            int v = s[ops[i].num].val;
            for (size_t j = 0; j < i; j++) if (ops[j].num == ops[i].num) v += ops[j].op;
            if (ops[i].op < 0 && v + ops[i].op < 0) blk = i;
            else if (ops[i].op == 0 && v != 0) blk = i;
            else if (ops[i].op > 0 && v + ops[i].op > SEMVMX) { ulk(h); return fail(E_RANGE); }
        }
        if (blk == nops) {
            for (size_t i = 0; i < nops; i++) {
                s[ops[i].num].val += ops[i].op;
                s[ops[i].num].pid = getpid();
                if ((ops[i].flg & SEM_UNDO) && ops[i].op) undo_add(id, ops[i].num, ops[i].op);
            }
            h->otime = time(0);
            ulk(h);
            if (alter) bump(h);
            return 0;
        }
        if (ops[blk].flg & IPC_NOWAIT) { ulk(h); return fail(E_AGAIN); }
        u16 *cnt = ops[blk].op ? &s[ops[blk].num].ncnt : &s[ops[blk].num].zcnt;
        (*cnt)++;
        int g = h->gen;
        ulk(h);
        int r = cx_futex_wait(&h->gen, g, -1);
        lk(h);
        if (*cnt) (*cnt)--;
        ulk(h);
        if (r == -4) return fail(E_INTR);
    }
}
int semop_nocancel(int id, void *ops, size_t n) { return semop(id, ops, n); }

int semctl(int id, int num, int cmd, ...) {
    __builtin_va_list ap;
    __builtin_va_start(ap, cmd);
    void *arg = __builtin_va_arg(ap, void *);   /* union semun: int val ou ponteiro, passado como um registro */
    __builtin_va_end(ap);
    ipc_hdr_t *h = get_live(1, id);
    if (!h) return -1;
    sem_ent_t *s = sems_of(h);
    int r = 0;
    switch (cmd) {
    case IPC_RMID: {
        u32 eu = (u32)SYS0(SYS_GETEUID);
        if (eu && eu != h->perm.uid && eu != h->perm.cuid) return fail(E_PERM);
        remove_obj(h);
        return 0;
    }
    case IPC_STAT: {                            /* struct semid_ds (72 bytes, pack 4) */
        if (!may(h, 4)) return fail(E_ACCES);
        u8 *d = arg;
        if (!d) return fail(E_INVAL);
        memset(d, 0, 72);
        perm_out(h, d);
        u16 n = (u16)h->n;
        memcpy(d + 28, &n, 2);
        put64(d, 32, h->otime); put64(d, 44, h->ctime);
        return 0;
    }
    case IPC_SET: if (!arg) return fail(E_INVAL); lk(h); r = perm_set(h, arg); ulk(h); return r;
    }
    if (cmd == GETALL || cmd == SETALL) {
        u16 *v = arg;
        if (!v) return fail(E_INVAL);
        if (!may(h, cmd == GETALL ? 4 : 2)) return fail(E_ACCES);
        lk(h);
        for (u32 i = 0; i < h->n; i++) {
            if (cmd == GETALL) v[i] = (u16)s[i].val;
            else if (v[i] > SEMVMX) { ulk(h); return fail(E_RANGE); }
            else { s[i].val = v[i]; s[i].pid = getpid(); }
        }
        if (cmd == SETALL) h->ctime = time(0);
        ulk(h);
        if (cmd == SETALL) bump(h);
        return 0;
    }
    if (num < 0 || (u32)num >= h->n) return fail(E_INVAL);
    switch (cmd) {
    case GETVAL: if (!may(h, 4)) return fail(E_ACCES); return s[num].val;
    case GETPID: if (!may(h, 4)) return fail(E_ACCES); return s[num].pid;
    case GETNCNT: if (!may(h, 4)) return fail(E_ACCES); return s[num].ncnt;
    case GETZCNT: if (!may(h, 4)) return fail(E_ACCES); return s[num].zcnt;
    case SETVAL: {
        int v = (int)(long)arg;
        if (!may(h, 2)) return fail(E_ACCES);
        if (v < 0 || v > SEMVMX) return fail(E_RANGE);
        lk(h);
        s[num].val = v; s[num].pid = getpid(); h->ctime = time(0);
        for (int i = 0; i < MAXUNDO; i++) if (undo[i].id == id && undo[i].num == num) undo[i].adj = 0;
        ulk(h);
        bump(h);
        return 0;
    }
    }
    return fail(E_INVAL);
}

/* ================================================================ memória compartilhada */
static struct { void *addr; int id; size_t len; } att[64];

int shmget(int key, size_t size, int flg) {
    ipc_hdr_t *h;
    int created;
    if (size > SHMMAX) return fail(E_INVAL);
    size_t pg = 4096, segl = (size + pg - 1) & ~(pg - 1);
    int id = obtain(2, key, flg, HDR + segl, &created, &h);
    if (id < 0) return -1;
    if (created) {
        if (!size) { remove_obj(h); return fail(E_INVAL); }
        h->n = (u32)size;
        publish(h);
        return id;
    }
    if (size > h->n) return fail(E_INVAL);
    if (!may(h, 4)) return fail(E_ACCES);
    return id;
}
void *shmat(int id, const void *addr, int flg) {
    ipc_hdr_t *h = get_live(2, id);
    if (!h) return (void *)-1;
    if (!may(h, (flg & SHM_RDONLY) ? 4 : 6)) { cx_errno = E_ACCES; return (void *)-1; }
    int nat = 0;
    for (int i = 0; i < 64; i++) if (att[i].addr) nat++;
    if (nat >= SHMSEG) { cx_errno = E_MFILE; return (void *)-1; }
    char p[96];
    obj_path(2, id, p, sizeof p);
    int fd = open(p, O_RDWR_, 0);
    if (fd < 0) { cx_errno = E_INVAL; return (void *)-1; }
    size_t len = ((size_t)h->n + 4095) & ~(size_t)4095;
    void *want = (void *)addr;
    if (want && (flg & SHM_RND)) want = (void *)((u64)want & ~(u64)4095);
    else if (want && ((u64)want & 4095)) { close(fd); cx_errno = E_INVAL; return (void *)-1; }
    void *m = mmap(want, len, (flg & SHM_RDONLY) ? 1 : 3, 1 | (want ? 0x10 : 0), fd, HDR);   /* MAP_SHARED [| MAP_FIXED] */
    close(fd);
    if (m == (void *)-1 || !m) { cx_errno = E_NOMEM; return (void *)-1; }
    for (int i = 0; i < 64; i++) if (!att[i].addr) { att[i].addr = m; att[i].id = id; att[i].len = len; break; }
    lk(h);
    h->nattch++; h->atime = time(0); h->lpid = getpid();
    ulk(h);
    return m;
}
int shmdt(const void *addr) {
    for (int i = 0; i < 64; i++) {
        if (att[i].addr != addr) continue;
        munmap(att[i].addr, att[i].len);
        ipc_hdr_t *h = map_obj(2, att[i].id);
        att[i].addr = 0;
        if (h) {
            lk(h);
            if (h->nattch) h->nattch--;
            h->dtime = time(0); h->lpid = getpid();
            int gone = h->removed && !h->nattch;
            ulk(h);
            if (gone) { char p[96]; obj_path(2, h->id, p, sizeof p); unlink(p); }
        }
        return 0;
    }
    return fail(E_INVAL);
}
int shmctl(int id, int cmd, void *buf) {
    ipc_hdr_t *h = get_live(2, id);
    if (!h) return -1;
    switch (cmd) {
    case IPC_RMID: {
        u32 eu = (u32)SYS0(SYS_GETEUID);
        if (eu && eu != h->perm.uid && eu != h->perm.cuid) return fail(E_PERM);
        remove_obj(h);
        return 0;
    }
    case IPC_STAT: {                            /* struct shmid_ds (76 bytes, pack 4) */
        if (!may(h, 4)) return fail(E_ACCES);
        u8 *d = buf;
        if (!d) return fail(E_INVAL);
        memset(d, 0, 76);
        perm_out(h, d);
        put64(d, 24, h->n); put32(d, 32, (u32)h->lpid); put32(d, 36, (u32)h->cpid);
        u16 na = (u16)h->nattch;
        memcpy(d + 40, &na, 2);
        put64(d, 44, h->atime); put64(d, 52, h->dtime); put64(d, 60, h->ctime);
        return 0;
    }
    case IPC_SET: { if (!buf) return fail(E_INVAL); lk(h); int r = perm_set(h, buf); ulk(h); return r; }
    }
    return fail(E_INVAL);
}

/* ================================================================ filas de mensagens */
static u8 *msg_area(ipc_hdr_t *h) { return (u8 *)h + HDR; }

int msgget(int key, int flg) {
    ipc_hdr_t *h;
    int created;
    int id = obtain(3, key, flg, HDR + MSGAREA, &created, &h);
    if (id < 0) return -1;
    if (created) { h->n = MSGMNB; publish(h); return id; }
    if (!may(h, 4)) return fail(E_ACCES);
    return id;
}
int msgsnd(int id, const void *msgp, size_t sz, int flg) {
    if (sz > MSGMAX) return fail(E_INVAL);
    if (!msgp) return fail(E_INVAL);
    i64 type;
    memcpy(&type, msgp, 8);
    if (type < 1) return fail(E_INVAL);
    ipc_hdr_t *h = get_live(3, id);
    if (!h) return -1;
    if (!may(h, 2)) return fail(E_ACCES);
    size_t need = sizeof(msg_ent_t) + ((sz + 7) & ~(size_t)7);
    for (;;) {
        lk(h);
        if (h->removed) { ulk(h); return fail(E_IDRM); }
        u32 qbytes_used = 0;                    /* bytes de texto na fila */
        for (u32 o = 0; o < h->used; ) { msg_ent_t *e = (msg_ent_t *)(msg_area(h) + o); qbytes_used += e->len; o += sizeof *e + ((e->len + 7) & ~7u); }
        if (qbytes_used + sz <= h->n && h->used + need <= MSGAREA) {
            msg_ent_t *e = (msg_ent_t *)(msg_area(h) + h->used);
            e->type = type; e->len = (u32)sz;
            memcpy(e + 1, (const u8 *)msgp + 8, sz);
            h->used += (u32)need; h->qnum++;
            h->lpid = getpid(); h->otime = time(0);
            ulk(h);
            bump(h);
            return 0;
        }
        if (flg & IPC_NOWAIT) { ulk(h); return fail(E_AGAIN); }
        int g = h->gen;
        ulk(h);
        if (cx_futex_wait(&h->gen, g, -1) == -4) return fail(E_INTR);
    }
}
ssize_t msgrcv(int id, void *msgp, size_t sz, long type, int flg) {
    if (!msgp) return fail(E_INVAL);
    ipc_hdr_t *h = get_live(3, id);
    if (!h) return -1;
    if (!may(h, 4)) return fail(E_ACCES);
    for (;;) {
        lk(h);
        if (h->removed) { ulk(h); return fail(E_IDRM); }
        u32 best = 0xffffffffu, o = 0;
        i64 bt = 0;
        while (o < h->used) {
            msg_ent_t *e = (msg_ent_t *)(msg_area(h) + o);
            int ok = type == 0 || (type > 0 && e->type == type) || (type < 0 && e->type <= -type);
            if (ok && (best == 0xffffffffu || (type < 0 && e->type < bt))) { best = o; bt = e->type; if (type >= 0) break; }
            o += sizeof *e + ((e->len + 7) & ~7u);
        }
        if (best != 0xffffffffu) {
            msg_ent_t *e = (msg_ent_t *)(msg_area(h) + best);
            if (e->len > sz && !(flg & MSG_NOERROR)) { ulk(h); return fail(E_2BIG); }
            size_t n = e->len > sz ? sz : e->len;
            memcpy(msgp, &e->type, 8);
            memcpy((u8 *)msgp + 8, e + 1, n);
            u32 el = (u32)(sizeof *e + ((e->len + 7) & ~7u));
            u8 *a = msg_area(h);
            for (u32 i = best; i + el < h->used; i++) a[i] = a[i + el];
            h->used -= el; h->qnum--;
            h->lrpid = getpid(); h->atime = time(0);
            ulk(h);
            bump(h);
            return (ssize_t)n;
        }
        if (flg & IPC_NOWAIT) { ulk(h); return fail(E_NOMSG); }
        int g = h->gen;
        ulk(h);
        if (cx_futex_wait(&h->gen, g, -1) == -4) return fail(E_INTR);
    }
}
int msgsnd_nocancel(int id, const void *m, size_t sz, int flg) { return msgsnd(id, m, sz, flg); }
ssize_t msgrcv_nocancel(int id, void *m, size_t sz, long t, int flg) { return msgrcv(id, m, sz, t, flg); }
int msgctl(int id, int cmd, void *buf) {
    ipc_hdr_t *h = get_live(3, id);
    if (!h) return -1;
    switch (cmd) {
    case IPC_RMID: {
        u32 eu = (u32)SYS0(SYS_GETEUID);
        if (eu && eu != h->perm.uid && eu != h->perm.cuid) return fail(E_PERM);
        remove_obj(h);
        return 0;
    }
    case IPC_STAT: {                            /* struct msqid_ds (116 bytes, pack 4) */
        if (!may(h, 4)) return fail(E_ACCES);
        u8 *d = buf;
        if (!d) return fail(E_INVAL);
        memset(d, 0, 116);
        perm_out(h, d);
        lk(h);
        u64 cb = 0;
        for (u32 o = 0; o < h->used; ) { msg_ent_t *e = (msg_ent_t *)(msg_area(h) + o); cb += e->len; o += sizeof *e + ((e->len + 7) & ~7u); }
        put64(d, 32, (i64)cb); put64(d, 40, h->qnum); put64(d, 48, h->n);
        put32(d, 56, (u32)h->lpid); put32(d, 60, (u32)h->lrpid);
        put64(d, 64, h->otime); put64(d, 76, h->atime); put64(d, 88, h->ctime);
        ulk(h);
        return 0;
    }
    case IPC_SET: {
        if (!buf) return fail(E_INVAL);
        lk(h);
        int r = perm_set(h, buf);
        u64 qb;
        memcpy(&qb, (u8 *)buf + 48, 8);
        if (!r && qb) {
            if (qb > MSGMNB && SYS0(SYS_GETEUID) != 0) r = fail(E_PERM);
            else h->n = (u32)(qb > MSGAREA ? MSGAREA : qb);
        }
        ulk(h);
        return r;
    }
    }
    return fail(E_INVAL);
}

/* entradas antigas do XNU (semsys/shmsys/msgsys): o primeiro argumento escolhe a chamada */
int semsys(int which, long a, long b, long c, long d) {
    switch (which) {
    case 0: return semctl((int)a, (int)b, (int)c, (void *)d);
    case 1: return semget((int)a, (int)b, (int)c);
    case 2: return semop((int)a, (void *)b, (size_t)c);
    }
    return fail(E_INVAL);
}
int shmsys(int which, long a, long b, long c) {
    switch (which) {
    case 0: { void *r = shmat((int)a, (void *)b, (int)c); return r == (void *)-1 ? -1 : (int)(long)r; }
    case 1: return shmctl((int)a, (int)b, (void *)c);
    case 2: return shmdt((void *)a);
    case 3: return shmget((int)a, (size_t)b, (int)c);
    }
    return fail(E_INVAL);
}
int msgsys(int which, long a, long b, long c, long d, long e) {
    switch (which) {
    case 0: return msgctl((int)a, (int)b, (void *)c);
    case 1: return msgget((int)a, (int)b);
    case 2: return msgsnd((int)a, (void *)b, (size_t)c, (int)d);
    case 3: return (int)msgrcv((int)a, (void *)b, (size_t)c, d, (int)e);
    }
    return fail(E_INVAL);
}

/* ftok: chave a partir do inode e do dispositivo do arquivo (mesma fórmula do libc do macOS) */
int stat(const char *p, void *st);
int ftok(const char *path, int id) {
    struct { u32 dev; u16 mode, nlink; u64 ino; u8 rest[128]; } s;
    if (stat(path, &s) != 0) return -1;
    return (int)((s.ino & 0xffff) | ((s.dev & 0xff) << 16) | ((u32)(id & 0xff) << 24));
}
