/* Teste de host de SYS_UNIX e SYS_GPU, sem o processo Mach-O. */
#include "host_unix.h"
#include "../kernel/include/mos_gpu.h"
#include "../kernel/include/syscall.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>

long host_gpu_op(unsigned long long op, unsigned long long user, unsigned long long len, int *blocked);

#define NFD 64
static int kind[NFD], slot[NFD], nb[NFD];

static int alloc_fd(int s, int n, int clo) {
    (void)clo;
    for (int i = 3; i < NFD; i++) if (!kind[i]) {
        kind[i] = 1; slot[i] = s; nb[i] = n;
        return i;
    }
    return -MOS_EMFILE;
}
static int fd_slot(int fd) { return (fd > 0 && fd < NFD && kind[fd]) ? slot[fd] : -1; }
static void set_slot(int fd, int s) { if (fd > 0 && fd < NFD) slot[fd] = s; }
static int dup_fd(int fd) {
    if (!kind[fd]) return -MOS_EBADF;
    host_unix_hold(fd);
    return alloc_fd(slot[fd], nb[fd], 0);
}
static int parent_ok(const char *path) {
    char tmp[128];
    snprintf(tmp, sizeof tmp, "%s", path);
    char *sl = strrchr(tmp, '/');
    if (!sl || sl == tmp) return 1;
    *sl = 0;
    return access(tmp, F_OK) == 0;
}
static int create_node(const char *path) {
    if (access(path, F_OK) == 0) return -MOS_EADDRINUSE;
    FILE *f = fopen(path, "wb");
    if (!f) return -MOS_ENOENT;
    fclose(f);
    return 0;
}
static int unlink_node(const char *path) { unlink(path); return 0; }

static void die(const char *w, long r) { fprintf(stderr, "falhou %s (%ld)\n", w, r); exit(1); }

int main(void) {
    host_unix_hooks hk = { alloc_fd, fd_slot, set_slot, dup_fd, parent_ok, create_node, unlink_node };
    host_unix_set_hooks(&hk);

    int fds[2];
    long r = host_unix_op(MOS_UNIX_PAIR, 1, 0, (unsigned long long)fds);
    if (r) die("pair", r);
    const char *msg = "abc";
    r = host_unix_write(fds[0], msg, 3);
    if (r != 3) die("write", r);
    char buf[8];
    memset(buf, 0, sizeof buf);
    r = host_unix_read(fds[1], buf, 8);
    if (r != 3 || memcmp(buf, "abc", 3)) die("read", r);
    host_unix_write(fds[0], "XY", 2);
    r = host_unix_read(fds[1], buf, 1);
    if (r != 1 || buf[0] != 'X') die("split", r);
    r = host_unix_read(fds[1], buf, 8);
    if (r != 1 || buf[0] != 'Y') die("resto", r);

    int lfd = (int)host_unix_op(MOS_UNIX_SOCKET, 1, 1 | 0x800, 0);
    if (lfd < 0) die("socket", lfd);
    unsigned char sun[16];
    memset(sun, 0, sizeof sun);
    sun[0] = 8; sun[1] = 1;
    memcpy(sun + 2, "/tmp/ux", 6);
    r = host_unix_op(MOS_UNIX_BIND, (unsigned)lfd, (unsigned long long)sun, 8);
    if (r) die("bind", r);
    r = host_unix_op(MOS_UNIX_LISTEN, (unsigned)lfd, 4, 0);
    if (r) die("listen", r);
    int cfd = (int)host_unix_op(MOS_UNIX_SOCKET, 1, 1, 0);
    if (cfd < 0) die("csock", cfd);
    r = host_unix_op(MOS_UNIX_CONNECT, (unsigned)cfd, (unsigned long long)sun, 8);
    if (r) die("connect", r);
    int afd = (int)host_unix_op(MOS_UNIX_ACCEPT, (unsigned)lfd, 0, 0);
    if (afd < 0) die("accept", afd);
    host_unix_write(cfd, "Z", 1);
    r = host_unix_read(afd, buf, 4);
    if (r != 1 || buf[0] != 'Z') die("aceito", r);
    unsigned long long sz = 0; int dir = 0; long long mt = 0;
    host_unix_fstat(afd, &sz, &dir, &mt);
    if (dir != 6) die("fstat", dir);

    int val = 0;
    mos_unix_sockopt_t so = { .level = 0xffff, .name = 0x1008, .len = 4, .val = &val };
    r = host_unix_op(MOS_UNIX_GETSOCKOPT, (unsigned)lfd, (unsigned long long)&so, 0);
    if (r || val != 1) die("getsockopt", r ? r : val);

    mos_gpu_info_t info;
    memset(&info, 0, sizeof info);
    int blocked = 0;
    r = host_gpu_op(MOS_GPU_INFO, (unsigned long long)&info, sizeof info, &blocked);
    if (r) die("gpu info", r);
    if (info.abi != MOS_GPU_ABI || !info.name[0]) die("gpu abi", info.abi);
    mos_gpu_ctx_t ctx;
    memset(&ctx, 0, sizeof ctx);
    ctx.kind = MOS_GPU_CTX_VIRGL;
    r = host_gpu_op(MOS_GPU_CTX_CREATE, (unsigned long long)&ctx, sizeof ctx, &blocked);
    if (r || !ctx.ctx) die("gpu ctx", r);
    printf("unix+gpu ok abi=%u ctx=%u\n", info.abi, ctx.ctx);
    unlink("/tmp/ux");
    return 0;
}
