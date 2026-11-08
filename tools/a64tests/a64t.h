/* utilidades dos testes C do a64test (sem libc) */
#pragma once
long a64t_write(int fd, const void *b, unsigned long n);
void a64t_dump(void);
static inline unsigned long a64t_strlen(const char *s) { unsigned long n = 0; while (s[n]) n++; return n; }
static inline void a64t_puts(const char *s) { a64t_write(1, s, a64t_strlen(s)); }
static inline void a64t_hex(unsigned long long v) {
    char b[19]; b[0] = '0'; b[1] = 'x';
    for (int i = 0; i < 16; i++) b[2 + i] = "0123456789abcdef"[(v >> (60 - 4 * i)) & 15];
    b[18] = 0; a64t_puts(b);
}
