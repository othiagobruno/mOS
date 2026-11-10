/* aio POSIX sobre SYS_AIO. Submeter e consultar não bloqueiam. */
#include "sys.h"
#include "libc.h"

struct aiocb {
    int aio_fildes;
    int aio_pad;
    long long aio_offset;
    volatile void *aio_buf;
    unsigned long long aio_nbytes;
    int aio_reqprio;
    int aio_lio_opcode;
};

#define SYS_AIO 96
#define AIO_EINPROGRESS 36

static long re(long r) { if (r < 0) { cx_errno = (int)-r; return -1; } return r; }

int aio_read(struct aiocb *cb) { long r = SYS2(SYS_AIO, 1, cb); return r < 0 ? (int)re(r) : 0; }
int aio_write(struct aiocb *cb) { long r = SYS2(SYS_AIO, 2, cb); return r < 0 ? (int)re(r) : 0; }
int aio_error(const struct aiocb *cb) {
    long r = SYS2(SYS_AIO, 3, cb);
    if (r == AIO_EINPROGRESS) return AIO_EINPROGRESS;
    if (r < 0) { re(r); return cx_errno; }
    return (int)r;
}
long aio_return(struct aiocb *cb) { return re(SYS2(SYS_AIO, 4, cb)); }
int aio_fsync(int op, struct aiocb *cb) { (void)op; long r = SYS2(SYS_AIO, 5, cb); return r < 0 ? (int)re(r) : 0; }
int aio_suspend(const struct aiocb *const list[], int nent, const void *timeout) {
    long r = SYS4(SYS_AIO, 6, list, nent, timeout);
    return r < 0 ? (int)re(r) : 0;
}
