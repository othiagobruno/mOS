/* Teste de host do overlay Darwin: chama os símbolos Swift com swiftcall. */
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <semaphore.h>

typedef struct { int value; unsigned char tag; } mos_posix_opt;

#define MOS_SWIFT __attribute__((swiftcall))

MOS_SWIFT int $s6Darwin6openatys5Int32VAD_SPys4Int8VGADtF(int fd, const char *path, int oflag);
MOS_SWIFT int $s6Darwin6openatys5Int32VAD_SPys4Int8VGADs6UInt16VtF(int fd, const char *path, int oflag, unsigned short mode);
MOS_SWIFT int $s6Darwin4openys5Int32VSPys4Int8VG_ADtF(const char *path, int oflag);
MOS_SWIFT int $s6Darwin5fcntlys5Int32VAD_ADtF(int fd, int cmd);
MOS_SWIFT int $s6Darwin5ioctlys5Int32VAD_SuSvtF(int fd, unsigned long request, void *ptr);
MOS_SWIFT void *$s6Darwin8sem_openySpys5Int32VGSgSPys4Int8VG_s5Int32Vs6UInt16Vs6UInt32VtF(const char *name, int oflag, unsigned short mode, unsigned value);
MOS_SWIFT int $s6Darwin5errnos5Int32Vvg(void);
MOS_SWIFT unsigned short $s6Darwin6S_IFMTs6UInt16Vvg(void);
MOS_SWIFT unsigned short $s6Darwin7S_IFDIRs6UInt16Vvg(void);
MOS_SWIFT mos_posix_opt $s6Darwin14POSIXErrorCodeO8rawValueACSgs5Int32V_tcfC(int raw);
MOS_SWIFT void *$s6Darwin6stdoutSpySo7__sFILEVGvg(void);
MOS_SWIFT void *$s6Darwin6stderrSpySo7__sFILEVGvg(void);

#define O_CREAT_D 0x200
#define O_EXCL_D  0x800
#define O_RDWR_D  0x2

static int fail(const char *m) { fprintf(stderr, "falha: %s (errno %d)\n", m, errno); return 1; }

int main(void) {
    char dir[] = "/tmp/mos-darwin-XXXXXX";
    if (!mkdtemp(dir)) return fail("mkdtemp");
    int dirfd = open(dir, O_RDONLY | O_DIRECTORY);
    if (dirfd < 0) return fail("open dir");
    umask(0);
    int fd = $s6Darwin6openatys5Int32VAD_SPys4Int8VGADs6UInt16VtF(dirfd, "criado", O_CREAT_D | O_RDWR_D, 0640);
    if (fd < 0) return fail("openat com mode");
    if (write(fd, "ok", 2) != 2) return fail("write");
    struct stat st;
    if (fstat(fd, &st) != 0) return fail("fstat");
    if ((st.st_mode & 0777) != 0640) {
        fprintf(stderr, "falha: mode %o\n", st.st_mode & 0777);
        return 1;
    }
    close(fd);
    fd = $s6Darwin6openatys5Int32VAD_SPys4Int8VGADtF(dirfd, "criado", O_RDWR_D);
    if (fd < 0) return fail("openat sem mode");
    char buf[4] = {0};
    if (read(fd, buf, 2) != 2 || memcmp(buf, "ok", 2) != 0) return fail("read");
    close(fd);
    errno = 0;
    int bad = $s6Darwin6openatys5Int32VAD_SPys4Int8VGADtF(dirfd, "nao-existe", O_RDWR_D);
    int e1 = $s6Darwin5errnos5Int32Vvg();
    if (bad != -1 || e1 != ENOENT) {
        fprintf(stderr, "falha: ausente retornou %d errno %d\n", bad, e1);
        return 1;
    }
    errno = 0;
    bad = $s6Darwin6openatys5Int32VAD_SPys4Int8VGADs6UInt16VtF(dirfd, "criado", O_CREAT_D | O_EXCL_D | O_RDWR_D, 0600);
    if (bad != -1 || $s6Darwin5errnos5Int32Vvg() != EEXIST) return fail("O_EXCL");
    errno = 0;
    bad = $s6Darwin6openatys5Int32VAD_SPys4Int8VGADtF(-1, "criado", O_RDWR_D);
    if (bad != -1 || $s6Darwin5errnos5Int32Vvg() != EBADF) return fail("dirfd");
    char path[256];
    snprintf(path, sizeof path, "%s/criado", dir);
    int fo = $s6Darwin4openys5Int32VSPys4Int8VG_ADtF(path, O_RDWR_D);
    if (fo < 0) return fail("open");
    int fl = $s6Darwin5fcntlys5Int32VAD_ADtF(fo, F_GETFL);
    if (fl < 0) return fail("fcntl");
    if ((fl & O_ACCMODE) != O_RDWR) {
        fprintf(stderr, "falha: F_GETFL %d\n", fl);
        return 1;
    }
    close(fo);
    int pfd[2];
    if (pipe(pfd) != 0) return fail("pipe");
    errno = 0;
    int io = $s6Darwin5ioctlys5Int32VAD_SuSvtF(pfd[0], 0x5413 /* TIOCGWINSZ linux */, &st);
    if (io != -1 || errno != ENOTTY) {
        fprintf(stderr, "falha: ioctl %d errno %d\n", io, errno);
        return 1;
    }
    close(pfd[0]);
    close(pfd[1]);
    if ($s6Darwin6S_IFMTs6UInt16Vvg() != 0170000) return fail("S_IFMT");
    if ($s6Darwin7S_IFDIRs6UInt16Vvg() != 0040000) return fail("S_IFDIR");
    void *sem = $s6Darwin8sem_openySpys5Int32VGSgSPys4Int8VG_s5Int32Vs6UInt16Vs6UInt32VtF(
        "/mos-darwin-swift", O_CREAT_D, 0600, 1);
    if (sem == (void *)-1 || sem == 0) return fail("sem_open");
    void *again = $s6Darwin8sem_openySpys5Int32VGSgSPys4Int8VG_s5Int32Vs6UInt16Vs6UInt32VtF(
        "/mos-darwin-swift", O_CREAT_D | O_EXCL_D, 0600, 1);
    if ((again != (void *)-1 && again != 0) || $s6Darwin5errnos5Int32Vvg() != EEXIST) return fail("sem_open excl");
    sem_close(sem);
    sem_unlink("/mos-darwin-swift");
    mos_posix_opt some = $s6Darwin14POSIXErrorCodeO8rawValueACSgs5Int32V_tcfC(2);
    mos_posix_opt last = $s6Darwin14POSIXErrorCodeO8rawValueACSgs5Int32V_tcfC(107);
    mos_posix_opt zero = $s6Darwin14POSIXErrorCodeO8rawValueACSgs5Int32V_tcfC(0);
    mos_posix_opt over = $s6Darwin14POSIXErrorCodeO8rawValueACSgs5Int32V_tcfC(108);
    mos_posix_opt neg = $s6Darwin14POSIXErrorCodeO8rawValueACSgs5Int32V_tcfC(-1);
    if (some.tag != 0 || some.value != 2) return fail("POSIX some ENOENT");
    if (last.tag != 0 || last.value != 107) return fail("POSIX some ELAST");
    if (zero.tag != 1 || over.tag != 1 || neg.tag != 1) return fail("POSIX nil");
    FILE *out = $s6Darwin6stdoutSpySo7__sFILEVGvg();
    FILE *err = $s6Darwin6stderrSpySo7__sFILEVGvg();
    if (out != stdout || err != stderr || out == err) return fail("stdout/stderr");
    if (fputc('x', err) == EOF) return fail("fputc stderr");
    fflush(err);
    unlink(path);
    close(dirfd);
    rmdir(dir);
    printf("darwin overlay ok\n");
    return 0;
}
