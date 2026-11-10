#pragma once
typedef unsigned long size_t;
int access(const char *path, int mode);
int unlink(const char *path);
char *getcwd(char *buf, size_t n);
int ftruncate(int fd, long length);
#define F_OK 0
#define R_OK 4
#define W_OK 2
#define X_OK 1
