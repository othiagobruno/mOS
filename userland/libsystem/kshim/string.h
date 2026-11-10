#pragma once
typedef unsigned long size_t;
void *memcpy(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
void *memmove(void *d, const void *s, size_t n);
int memcmp(const void *a, const void *b, size_t n);
void *memchr(const void *s, int c, size_t n);
size_t strlen(const char *s);
int strncmp(const char *a, const char *b, size_t n);
char *strrchr(const char *s, int c);
size_t strspn(const char *s, const char *accept);
size_t strcspn(const char *s, const char *reject);
int strcmp(const char *a, const char *b);
char *strcpy(char *d, const char *s);
char *strchr(const char *s, int c);
#define NULL ((void *)0)
