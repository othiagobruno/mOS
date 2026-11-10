#include "host_id.h"
#include "../kernel/include/syscall.h"
#include <errno.h>
#ifdef _WIN32
#include <stdio.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <unistd.h>
#include <sys/stat.h>
#endif

long host_getid(unsigned sysno) {
    if (sysno == SYS_GETUID || sysno == SYS_GETEUID) return 501;
    if (sysno == SYS_GETGID || sysno == SYS_GETEGID) return 20;
    return -MOS_EINVAL;
}

long host_pread_fd(int fd, void *buf, unsigned long n, unsigned long off) {
#ifdef _WIN32
    struct _stat64 st;
    if (_fstat64(fd, &st) != 0) return -MOS_ENOENT;
    if (st.st_mode & _S_IFDIR) return -MOS_EISDIR;
    long long cur = _lseeki64(fd, 0, SEEK_CUR);
    if (_lseeki64(fd, (long long)off, SEEK_SET) < 0) {
        if (cur >= 0) _lseeki64(fd, cur, SEEK_SET);
        return -MOS_EINVAL;
    }
    int got = _read(fd, buf, (unsigned)n);
    if (cur >= 0) _lseeki64(fd, cur, SEEK_SET);
    return got < 0 ? -MOS_EIO : got;
#else
    struct stat st;
    if (fstat(fd, &st) != 0) return -MOS_ENOENT;
    if (S_ISDIR(st.st_mode)) return -MOS_EISDIR;
    ssize_t got = pread(fd, buf, (size_t)n, (off_t)off);
    if (got < 0) return -MOS_EIO;
    return (long long)got;
#endif
}
