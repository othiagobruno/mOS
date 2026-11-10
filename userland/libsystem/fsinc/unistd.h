#pragma once
#include <stddef.h>
int close(int fd);
long read(int fd, void *b, size_t n);
long write(int fd, const void *b, size_t n);
long lseek(int fd, long off, int whence);
int unlink(const char *p);
int fsync(int fd);
int access(const char *p, int mode);
int getpid(void);
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
