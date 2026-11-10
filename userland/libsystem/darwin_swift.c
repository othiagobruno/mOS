/*
 * Overlay Swift Darwin (libswiftDarwin): open, openat, fcntl, ioctl, sem_open,
 * errno e as constantes de modo que os apps importam com o nome mangled.
 * A convenção é swiftcall. Nos inteiros e ponteiros deste arquivo ela cabe
 * nos mesmos registradores da ABI C do arm64 e do x86_64, e o atributo deixa
 * o compilador emitir essa convenção nos dois alvos.
 * Cada função chama a libc do mOS (syscalls de verdade). O teste de host
 * (MOS_HOST_TEST) traduz os O_* do Darwin para os do Linux e chama a libc
 * do host.
 */
#ifdef MOS_HOST_TEST
#include <errno.h>
#include <fcntl.h>
#include <semaphore.h>
#include <stdarg.h>
#include <sys/ioctl.h>
#else
int open(const char *path, int flags, ...);
int openat(int fd, const char *path, int flags, ...);
int fcntl(int fd, int cmd, ...);
int ioctl(int fd, unsigned long req, ...);
void *sem_open(const char *name, int oflag, ...);
int *__error(void);
#endif

#define MOS_SWIFT __attribute__((swiftcall))

#ifdef MOS_HOST_TEST
static int *mos_err(void) { return &errno; }
static int mos_flags(int f) {
    int o = f & 3;                              /* O_RDONLY/O_WRONLY/O_RDWR coincidem */
    if (f & 0x0004) o |= O_NONBLOCK;
    if (f & 0x0008) o |= O_APPEND;
    if (f & 0x0200) o |= O_CREAT;
    if (f & 0x0400) o |= O_TRUNC;
    if (f & 0x0800) o |= O_EXCL;
    return o;
}
static int mos_open(const char *path, int flags, unsigned mode) {
    if (flags & 0x0200) return open(path, mos_flags(flags), mode);
    return open(path, mos_flags(flags));
}
static int mos_openat(int fd, const char *path, int flags, unsigned mode) {
    if (flags & 0x0200) return openat(fd, path, mos_flags(flags), mode);
    return openat(fd, path, mos_flags(flags));
}
static void *mos_sem_open(const char *name, int oflag, unsigned mode, unsigned value) {
    int o = 0;
    if (oflag & 0x0200) o |= O_CREAT;
    if (oflag & 0x0800) o |= O_EXCL;
    if (o & O_CREAT) return sem_open(name, o, (mode_t)mode, value);
    return sem_open(name, o);
}
#else
static int *mos_err(void) { return __error(); }
static int mos_open(const char *path, int flags, unsigned mode) { return open(path, flags, mode); }
static int mos_openat(int fd, const char *path, int flags, unsigned mode) { return openat(fd, path, flags, mode); }
static void *mos_sem_open(const char *name, int oflag, unsigned mode, unsigned value) {
    if (oflag & 0x0200) return sem_open(name, oflag, mode, value);
    return sem_open(name, oflag);
}
#endif

static int mos_bad_path(const char *path) {
    if (path) return 0;
    *mos_err() = 14;                            /* EFAULT */
    return 1;
}

MOS_SWIFT int $s6Darwin4openys5Int32VSPys4Int8VG_ADtF(const char *path, int oflag) {
    if (mos_bad_path(path)) return -1;
    return mos_open(path, oflag, 0666);
}
MOS_SWIFT int $s6Darwin4openys5Int32VSPys4Int8VG_ADs6UInt16VtF(const char *path, int oflag, unsigned short mode) {
    if (mos_bad_path(path)) return -1;
    return mos_open(path, oflag, mode);
}
MOS_SWIFT int $s6Darwin6openatys5Int32VAD_SPys4Int8VGADtF(int fd, const char *path, int oflag) {
    if (mos_bad_path(path)) return -1;
    return mos_openat(fd, path, oflag, 0666);
}
MOS_SWIFT int $s6Darwin6openatys5Int32VAD_SPys4Int8VGADs6UInt16VtF(int fd, const char *path, int oflag, unsigned short mode) {
    if (mos_bad_path(path)) return -1;
    return mos_openat(fd, path, oflag, mode);
}

MOS_SWIFT int $s6Darwin5fcntlys5Int32VAD_ADtF(int fd, int cmd) { return fcntl(fd, cmd); }
MOS_SWIFT int $s6Darwin5fcntlys5Int32VAD_A2DtF(int fd, int cmd, int value) { return fcntl(fd, cmd, value); }
MOS_SWIFT int $s6Darwin5fcntlys5Int32VAD_ADSvtF(int fd, int cmd, void *ptr) { return fcntl(fd, cmd, ptr); }

MOS_SWIFT int $s6Darwin5ioctlys5Int32VAD_SutF(int fd, unsigned long request) { return ioctl(fd, request); }
MOS_SWIFT int $s6Darwin5ioctlys5Int32VAD_SuADtF(int fd, unsigned long request, int value) { return ioctl(fd, request, value); }
MOS_SWIFT int $s6Darwin5ioctlys5Int32VAD_SuSvtF(int fd, unsigned long request, void *ptr) { return ioctl(fd, request, ptr); }

/* sem_t do Darwin (zig any-darwin) é int. O importador vê sem_t * como
 * UnsafeMutablePointer<Int32>. O ! do overlay é o opcional no mangling. */
MOS_SWIFT void *$s6Darwin8sem_openySpys5Int32VGSgSPys4Int8VG_s5Int32VtF(const char *name, int oflag) {
    if (mos_bad_path(name)) return 0;
    return mos_sem_open(name, oflag, 0, 0);
}
MOS_SWIFT void *$s6Darwin8sem_openySpys5Int32VGSgSPys4Int8VG_s5Int32Vs6UInt16Vs6UInt32VtF(const char *name, int oflag, unsigned short mode, unsigned value) {
    if (mos_bad_path(name)) return 0;
    return mos_sem_open(name, oflag, mode, value);
}

MOS_SWIFT int $s6Darwin5errnos5Int32Vvg(void) { return *mos_err(); }
MOS_SWIFT int $s6Darwin5noErrs5Int32Vvg(void) { return 0; }

MOS_SWIFT unsigned short $s6Darwin6S_IFMTs6UInt16Vvg(void) { return 0170000; }
MOS_SWIFT unsigned short $s6Darwin7S_IFDIRs6UInt16Vvg(void) { return 0040000; }
MOS_SWIFT unsigned short $s6Darwin7S_IFLNKs6UInt16Vvg(void) { return 0120000; }
MOS_SWIFT unsigned short $s6Darwin7S_IFREGs6UInt16Vvg(void) { return 0100000; }
MOS_SWIFT unsigned short $s6Darwin8S_IFSOCKs6UInt16Vvg(void) { return 0140000; }
MOS_SWIFT unsigned short $s6Darwin7S_IRUSRs6UInt16Vvg(void) { return 0000400; }
MOS_SWIFT unsigned short $s6Darwin7S_IWUSRs6UInt16Vvg(void) { return 0000200; }
MOS_SWIFT unsigned short $s6Darwin7S_IWGRPs6UInt16Vvg(void) { return 0000020; }
MOS_SWIFT unsigned short $s6Darwin7S_IWOTHs6UInt16Vvg(void) { return 0000002; }
