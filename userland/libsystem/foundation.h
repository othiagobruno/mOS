/* mOS libSystem — layout dos objetos da Foundation (compartilhado com o AppKit) */
#pragma once
#include "objc.h"
#include <stdarg.h>

typedef struct { cx_class_t *isa; u64 flags; char *s; u64 n; } nsstr_t;       /* = layout de __cfstring */
typedef struct { cx_class_t *isa; id *items; u64 n, cap; } nsarr_t;
typedef struct { cx_class_t *isa; id *keys, *vals; u64 n, cap; } nsdict_t;
typedef struct cx_timer {
    cx_class_t *isa;
    double interval, fire;
    id target, info;
    SEL sel;
    void *block;
    int repeats, valid;
    struct cx_timer *next;
} cx_timer_t;
typedef struct { long sec; int usec; } mac_tv_t;
typedef void FILE_;

id cx_str(const char *s);
id cx_str_n(const char *s, u64 n);
const char *cx_cstr(id o);
BOOL cx_is_str(id o);
id cx_str_vfmt(id fmt, va_list ap);
id cx_num_i(i64 v);
id cx_num_d(double v);
id cx_arr_new(cx_class_t *cls);
void cx_arr_add(id a, id o);
id cx_note(id name, id object, id info);
void cx_post(id note);
double cx_now(void);
double cx_run_timers(void);
extern cx_timer_t *cx_timers;
extern void (*cx_runloop_hook)(double until);
void *_Block_copy(const void *b);

/* libc */
int vasprintf(char **out, const char *fmt, va_list ap);
int snprintf(char *b, size_t n, const char *fmt, ...);
FILE_ *fopen(const char *p, const char *m);
size_t fwrite(const void *p, size_t s, size_t n, FILE_ *f);
size_t fread(void *p, size_t s, size_t n, FILE_ *f);
int fclose(FILE_ *f);
int gettimeofday(mac_tv_t *tv, void *tz);
long time(long *t);
void *gmtime(const long *t);
size_t strftime(char *s, size_t max, const char *fmt, const void *tm);
int getpid(void);
int usleep(unsigned us);
int access(const char *p, int m);
int stat(const char *p, void *st);
void *opendir(const char *p);
void *readdir(void *d);
int closedir(void *d);
int mkdir(const char *p, int m);
int unlink(const char *p);
char *getcwd(char *b, size_t n);
unsigned long clock(void);
long long strtoll(const char *s, char **e, int b);
int atoi(const char *s);
int strcasecmp(const char *a, const char *b);
char *strstr(const char *h, const char *n);
char *strrchr(const char *s, int c);
char *strcat(char *d, const char *s);
void qsort(void *b, size_t n, size_t s, int (*cmp)(const void *, const void *));
int isspace(int c);