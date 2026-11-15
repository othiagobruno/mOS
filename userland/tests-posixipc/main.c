/* mOS — IPC POSIX nomeado e chamadas BSD: shm_open/shm_unlink, sem_open e família, getattrlist/setattrlist,
 * abort_with_reason e as chamadas que o macOS/Darling devolvem com ENOTSUP nestes volumes.
 * Valores esperados: o contrato do macOS (o mesmo que o Darling entrega). O mesmo binário roda no Darling,
 * no macOS e no mOS (tools/test-posixipc.sh e o "teste" do boot). */
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <semaphore.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <sys/attr.h>
#include <sys/time.h>
#include <sys/stat.h>

extern void abort_with_reason(uint32_t, uint64_t, const char *, uint64_t) __attribute__((noreturn));
extern int getattrlistbulk(int, void *, void *, size_t, uint64_t);
extern int exchangedata(const char *, const char *, unsigned int);

static int fails, total;
#define EXPECT(got, want, what) do { long long g_ = (long long)(got), w_ = (long long)(want); total++; \
    if (g_ == w_) printf("  ok  %s = %lld\n", what, g_); \
    else { fails++; printf("  FALHOU  %s = %lld (esperado %lld)\n", what, g_, w_); } } while (0)
/* -1 e o errno numa linha só: "r/errno" vira r * 1000 + errno */
#define RE(expr) ({ errno = 0; long r_ = (long)(expr); r_ < 0 ? -(1000 + errno) : r_; })
static long now_ms(void) { struct timeval tv; gettimeofday(&tv, 0); return tv.tv_sec * 1000L + tv.tv_usec / 1000; }

int main(void) {
    setbuf(stdout, 0);
    char nome[64];
    snprintf(nome, sizeof nome, "/mos-t%d", (int)getpid());

    printf("shm_open\n");
    shm_unlink(nome);
    EXPECT(RE(shm_open(nome, O_RDWR)), -(1000 + ENOENT), "shm_open sem O_CREAT num nome novo");
    int fd = shm_open(nome, O_RDWR | O_CREAT | O_EXCL, 0600);
    EXPECT(fd >= 0, 1, "shm_open O_CREAT|O_EXCL");
    EXPECT(RE(shm_open(nome, O_RDWR | O_CREAT | O_EXCL, 0600)), -(1000 + EEXIST), "shm_open O_EXCL de novo");
    EXPECT(RE(shm_open("/nome-com-mais-de-trinta-e-um-bytes-xx", O_RDWR | O_CREAT, 0600)), -(1000 + ENAMETOOLONG), "shm_open nome longo");
    EXPECT(ftruncate(fd, 4096), 0, "ftruncate do objeto");
    volatile int *shm = mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    EXPECT(shm != MAP_FAILED, 1, "mmap MAP_SHARED");
    shm[0] = 0;
    pid_t c = fork();
    if (c == 0) { shm[0] = 4242; _exit(0); }
    int st = 0;
    waitpid(c, &st, 0);
    EXPECT(shm[0], 4242, "o filho escreveu na mesma página");
    int fd2 = shm_open(nome, O_RDWR);
    volatile int *shm2 = fd2 >= 0 ? mmap(0, 4096, PROT_READ | PROT_WRITE, MAP_SHARED, fd2, 0) : MAP_FAILED;
    EXPECT(shm2 != MAP_FAILED && shm2[0] == 4242, 1, "outro shm_open do mesmo nome vê o mesmo conteúdo");
    EXPECT(shm_unlink(nome), 0, "shm_unlink");
    EXPECT(RE(shm_unlink(nome)), -(1000 + ENOENT), "shm_unlink de novo");
    close(fd); if (fd2 >= 0) close(fd2);

    printf("sem_open\n");
    sem_unlink(nome);
    EXPECT(RE((long)sem_open(nome, 0)), -(1000 + ENOENT), "sem_open sem O_CREAT num nome novo");
    sem_t *s = sem_open(nome, O_CREAT | O_EXCL, 0600, 1);
    EXPECT(s != SEM_FAILED, 1, "sem_open O_CREAT|O_EXCL valor 1");
    EXPECT(RE((long)sem_open(nome, O_CREAT | O_EXCL, 0600, 0)), -(1000 + EEXIST), "sem_open O_EXCL de novo");
    EXPECT(RE((long)sem_open("/x", O_CREAT, 0600, 40000)), -(1000 + EINVAL), "sem_open valor acima de SEM_VALUE_MAX");
    EXPECT(RE((long)sem_open("/nome-com-mais-de-trinta-e-um-bytes-xx", O_CREAT, 0600, 0)), -(1000 + ENAMETOOLONG), "sem_open nome longo");
    EXPECT(sem_trywait(s), 0, "sem_trywait pega o 1");
    EXPECT(RE(sem_trywait(s)), -(1000 + EAGAIN), "sem_trywait vazio");
    EXPECT(sem_post(s), 0, "sem_post");
    EXPECT(sem_wait(s), 0, "sem_wait pega o post");

    c = fork();
    if (c == 0) {
        sem_t *cs = sem_open(nome, 0);
        if (cs == SEM_FAILED) _exit(2);
        usleep(200000);
        sem_post(cs);
        _exit(0);
    }
    long t0 = now_ms();
    EXPECT(sem_wait(s), 0, "sem_wait acordado pelo post de outro processo");
    long dt = now_ms() - t0;
    EXPECT(dt >= 150 && dt < 3000, 1, "esperou o filho postar");
    waitpid(c, &st, 0);
    EXPECT(WIFEXITED(st) && WEXITSTATUS(st) == 0, 1, "o filho abriu pelo nome");

    EXPECT(sem_unlink(nome), 0, "sem_unlink");
    EXPECT(RE((long)sem_open(nome, 0)), -(1000 + ENOENT), "nome some depois do sem_unlink");
    EXPECT(sem_post(s), 0, "o semáforo aberto continua valendo");
    EXPECT(sem_trywait(s), 0, "e o valor também");
    EXPECT(RE(sem_unlink(nome)), -(1000 + ENOENT), "sem_unlink de novo");
    EXPECT(sem_close(s), 0, "sem_close");
    sem_t anon;
    EXPECT(RE(sem_init(&anon, 0, 1)), -(1000 + ENOSYS), "sem_init (sem semáforo anônimo no macOS)");

    printf("getattrlist e setattrlist\n");
    const char *dir = "/tmp/mos-attr-teste";
    mkdir(dir, 0755);
    char f1[128]; snprintf(f1, sizeof f1, "%s/a", dir);
    int ff = open(f1, O_WRONLY | O_CREAT | O_TRUNC, 0644); if (ff >= 0) { write(ff, "x", 1); close(ff); }
    struct attrlist al = {ATTR_BIT_MAP_COUNT, 0, 0, 0, 0, 0, 0};
    struct { uint32_t len; uint32_t count; } __attribute__((packed)) dbuf = {0, 99};
    al.dirattr = ATTR_DIR_ENTRYCOUNT;
    EXPECT(getattrlist(dir, &al, &dbuf, sizeof dbuf, 0), 0, "getattrlist ATTR_DIR_ENTRYCOUNT");
    EXPECT(dbuf.len, 8, "tamanho devolvido");
    EXPECT(dbuf.count, 1, "uma entrada, sem . e ..");
    struct { uint32_t len; uint8_t finfo[32]; } __attribute__((packed)) fbuf;
    memset(&fbuf, 0x5A, sizeof fbuf);
    al.dirattr = 0; al.commonattr = ATTR_CMN_FNDRINFO;
    EXPECT(getattrlist(f1, &al, &fbuf, sizeof fbuf, 0), 0, "getattrlist ATTR_CMN_FNDRINFO");
    int zero = 1; for (int i = 0; i < 32; i++) if (fbuf.finfo[i]) zero = 0;
    EXPECT(zero && fbuf.len == 36, 1, "Finder Info zerado (36 bytes)");
    al.commonattr = ATTR_CMN_NAME;
    EXPECT(RE(getattrlist(f1, &al, &fbuf, sizeof fbuf, 0)), -(1000 + EINVAL), "getattrlist atributo sem suporte");
    al.commonattr = ATTR_CMN_FNDRINFO; al.bitmapcount = 4;
    EXPECT(RE(getattrlist(f1, &al, &fbuf, sizeof fbuf, 0)), -(1000 + EINVAL), "getattrlist bitmapcount 4");
    al.bitmapcount = ATTR_BIT_MAP_COUNT;
    struct { uint32_t len; } shortb = {0};
    EXPECT(getattrlist(f1, &al, &shortb, sizeof shortb, 0), 0, "buffer curto trunca sem erro");
    EXPECT(shortb.len, 4, "o tamanho diz o que coube");
    EXPECT(getattrlist(f1, &al, &shortb, sizeof shortb, FSOPT_REPORT_FULLSIZE), 0, "FSOPT_REPORT_FULLSIZE");
    EXPECT(shortb.len, 36, "o tamanho diz o que seria preciso");

    struct attrlist sl = {ATTR_BIT_MAP_COUNT, 0, ATTR_CMN_ACCESSMASK, 0, 0, 0, 0};
    uint32_t mode = 0600;
    EXPECT(setattrlist(f1, &sl, &mode, sizeof mode, 0), 0, "setattrlist ATTR_CMN_ACCESSMASK");
    struct stat sb; stat(f1, &sb);
    EXPECT(sb.st_mode & 0777, 0600, "o modo mudou");
    sl.commonattr = ATTR_CMN_NAME;
    EXPECT(RE(setattrlist(f1, &sl, &mode, sizeof mode, 0)), -(1000 + EINVAL), "setattrlist atributo sem suporte");
    sl.commonattr = ATTR_CMN_ACCESSMASK; sl.bitmapcount = 3;
    EXPECT(RE(setattrlist(f1, &sl, &mode, sizeof mode, 0)), -(1000 + EINVAL), "setattrlist bitmapcount 3");
    sl.bitmapcount = ATTR_BIT_MAP_COUNT;
    EXPECT(RE(setattrlist("/tmp/mos-attr-teste/nao-existe", &sl, &mode, sizeof mode, 0)), -(1000 + ENOENT), "setattrlist num arquivo que não existe");

    printf("sem suporte nestes volumes\n");
    char big[256];
    al.commonattr = ATTR_CMN_RETURNED_ATTRS | ATTR_CMN_NAME;
    int dfd = open(dir, O_RDONLY);
    EXPECT(RE(getattrlistbulk(dfd, &al, big, sizeof big, 0)), -(1000 + ENOTSUP), "getattrlistbulk");
    EXPECT(RE(exchangedata(f1, f1, 0)), -(1000 + ENOTSUP), "exchangedata");
    close(dfd);
    unlink(f1); rmdir(dir);

    printf("abort_with_reason\n");
    c = fork();
    if (c == 0) { close(2); abort_with_reason(1, 0x1234, "teste do mOS", 0); }
    waitpid(c, &st, 0);
    EXPECT(WIFSIGNALED(st) && WTERMSIG(st) == SIGABRT, 1, "o filho morreu com SIGABRT");

    printf("posixipc: %d/%d ok\n", total - fails, total);
    return fails ? 1 : 0;
}
