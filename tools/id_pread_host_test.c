#include "host_id.h"
#include "../kernel/include/syscall.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void die(const char *w) { fprintf(stderr, "falhou %s\n", w); exit(1); }

int main(void) {
    if (host_getid(SYS_GETUID) != 501 || host_getid(SYS_GETEUID) != 501) die("uid");
    if (host_getid(SYS_GETGID) != 20 || host_getid(SYS_GETEGID) != 20) die("gid");
    char name[] = "/tmp/mos-pread-XXXXXX";
    int fd = mkstemp(name);
    if (fd < 0) die("tmp");
    if (write(fd, "abcd", 4) != 4) die("write");
    if (lseek(fd, 3, SEEK_SET) != 3) die("seek");
    char buf[4];
    memset(buf, 0, sizeof buf);
    long n = host_pread_fd(fd, buf, 2, 1);
    if (n != 2 || memcmp(buf, "bc", 2) != 0) die("bytes");
    if (lseek(fd, 0, SEEK_CUR) != 3) die("pos");
    n = host_pread_fd(fd, buf, 4, 100);
    if (n != 0) die("eof");
    if (lseek(fd, 0, SEEK_CUR) != 3) die("pos2");
    close(fd);
    unlink(name);
    printf("id+pread ok uid=501 gid=20\n");
    return 0;
}
