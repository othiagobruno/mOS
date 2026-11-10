/* Sonda do machotest: mach_port_construct, socketpair e futex, pelos símbolos do libSystem.
 *   zig cc -target x86_64-macos -O1 -fno-stack-protector tools/testdata/ipc-harness.c \
 *       -o build/ipc-harness -Wl,-undefined,dynamic_lookup
 *   ./build/machotest build/libSystem.mos build build/ipc-harness
 * A saída esperada termina em "ipc: ok". */
int printf(const char *fmt, ...);
int mach_port_construct(unsigned task, void *options, unsigned long long context, unsigned *name);
int mach_port_type(unsigned task, unsigned name, unsigned *ptype);
int socketpair(int domain, int type, int proto, int sv[2]);
long write(int fd, const void *buf, unsigned long n);
long read(int fd, void *buf, unsigned long n);
int cx_futex_wait(volatile int *w, int expected, long ms);
int cx_futex_wake(volatile int *w, int n);
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
int pthread_join(unsigned long t, void **ret);
void exit(int code);

static volatile int word;
static int woke;

static void *waiter(void *arg) {
    (void)arg;
    int r = cx_futex_wait(&word, 0, 3000);
    woke = r;
    return (void *)(long)r;
}

int main(void) {
    unsigned opt[2] = {0x02 | 0x10, 8}; /* MPO_QLIMIT | MPO_INSERT_SEND_RIGHT */
    unsigned name = 0, ty = 0;
    int kr = mach_port_construct(0x103, opt, 0xC0FFEEULL, &name);
    int tr = name ? mach_port_type(0x103, name, &ty) : -1;
    printf("ipc: construct kr=%d nome=%u type_kr=%d type=0x%x\n", kr, name, tr, ty);

    int sv[2] = {-1, -1};
    int sr = socketpair(1, 1, 0, sv);
    char out[] = "ping";
    char in[8] = {0};
    char back[8] = {0};
    long wr = write(sv[0], out, 4);
    long rd = read(sv[1], in, 4);
    long wr2 = write(sv[1], "pong", 4);
    long rd2 = read(sv[0], back, 4);
    printf("ipc: socketpair r=%d fds=%d,%d foi=%ld voltou=%ld texto=%s/%s\n", sr, sv[0], sv[1], wr, rd, in, back);
    printf("ipc: volta wr=%ld rd=%ld\n", wr2, rd2);

    word = 0;
    unsigned long th = 0;
    int pr = pthread_create(&th, 0, waiter, 0);
    for (volatile int i = 0; i < 20000000; i++) {}
    word = 1;
    int wk = cx_futex_wake(&word, 1);
    void *ret = 0;
    int jr = pthread_join(th, &ret);
    printf("ipc: futex create=%d wake=%d join=%d wait_r=%d thread_ret=%ld\n", pr, wk, jr, woke, (long)ret);

    int bad = kr || !name || !(ty & 0x30000) || sr || wr != 4 || rd != 4 || wr2 != 4 || rd2 != 4 || pr || jr || woke;
    printf("ipc: %s\n", bad ? "FALHA" : "ok");
    exit(bad ? 1 : 0);
}
