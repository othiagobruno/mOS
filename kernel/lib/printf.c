/* mOS — vsnprintf, log do kernel (porta serial + buffer circular "dmesg") e panic */
#include "kernel.h"

static void out(char *buf, usize n, usize *pos, char c) {
    if (*pos + 1 < n) buf[*pos] = c;
    (*pos)++;
}

int vsnprintf(char *buf, usize n, const char *fmt, va_list ap) {
    usize pos = 0;
    for (; *fmt; fmt++) {
        if (*fmt != '%') { out(buf, n, &pos, *fmt); continue; }
        fmt++;
        int left = 0, zero = 0, width = 0, prec = -1, lng = 0, plus = 0;
        for (;; fmt++) {
            if (*fmt == '-') left = 1;
            else if (*fmt == '0') zero = 1;
            else if (*fmt == '+') plus = 1;
            else break;
        }
        if (*fmt == '*') { width = va_arg(ap, int); fmt++; }
        while (isdigit(*fmt)) width = width * 10 + (*fmt++ - '0');
        if (*fmt == '.') {
            fmt++; prec = 0;
            if (*fmt == '*') { prec = va_arg(ap, int); fmt++; }
            while (isdigit(*fmt)) prec = prec * 10 + (*fmt++ - '0');
        }
        while (*fmt == 'l' || *fmt == 'z' || *fmt == 'h') {
            if (*fmt == 'z') lng = 2;
            else if (*fmt == 'l') lng++;
            fmt++;
        }
        char tmp[72];
        int len = 0;
        const char *s = tmp;
        switch (*fmt) {
        case 'c': tmp[0] = (char)va_arg(ap, int); len = 1; break;
        case 's':
            s = va_arg(ap, const char *);
            if (!s) s = "(null)";
            len = (int)strlen(s);
            if (prec >= 0 && len > prec) len = prec;
            break;
        case 'd': case 'i': case 'u': case 'x': case 'X': case 'p': case 'o': {
            u64 v;
            int neg = 0, base = 10;
            if (*fmt == 'p') { v = (u64)va_arg(ap, void *); base = 16; }
            else if (*fmt == 'd' || *fmt == 'i') {
                i64 sv = lng >= 2 ? va_arg(ap, i64) : (lng == 1 && sizeof(long) == 8) ? va_arg(ap, long) : va_arg(ap, int);
                if (sv < 0) { neg = 1; v = (u64)-sv; } else v = (u64)sv;
            } else {
                v = lng >= 2 ? va_arg(ap, u64) : (lng == 1 && sizeof(long) == 8) ? va_arg(ap, unsigned long) : va_arg(ap, unsigned);
                if (*fmt == 'x' || *fmt == 'X') base = 16;
                if (*fmt == 'o') base = 8;
            }
            const char *dig = *fmt == 'X' ? "0123456789ABCDEF" : "0123456789abcdef";
            char r[66];
            int k = 0;
            do { r[k++] = dig[v % base]; v /= base; } while (v);
            while (k < prec) r[k++] = '0';
            if (neg) tmp[len++] = '-'; else if (plus) tmp[len++] = '+';
            if (zero && !left) while (k + len < width) r[k++] = '0';
            while (k) tmp[len++] = r[--k];
            break;
        }
        case 'f': {
            double v = va_arg(ap, double);
            if (prec < 0) prec = 2;
            if (v < 0) { tmp[len++] = '-'; v = -v; } else if (plus) tmp[len++] = '+';
            double m = 1;
            for (int i = 0; i < prec; i++) m *= 10;
            u64 all = (u64)(v * m + 0.5);
            u64 ip = all / (u64)m, fp = all % (u64)m;
            char r[32];
            int k = 0;
            do { r[k++] = '0' + ip % 10; ip /= 10; } while (ip);
            while (k) tmp[len++] = r[--k];
            if (prec) {
                tmp[len++] = '.';
                for (int i = prec - 1; i >= 0; i--) { r[i] = '0' + fp % 10; fp /= 10; }
                for (int i = 0; i < prec; i++) tmp[len++] = r[i];
            }
            break;
        }
        case '%': tmp[0] = '%'; len = 1; break;
        default: tmp[0] = '%'; tmp[1] = *fmt; len = 2; break;
        }
        if (!left) while (len < width--) out(buf, n, &pos, ' ');
        for (int i = 0; i < len; i++) out(buf, n, &pos, s[i]);
        if (left) while (len < width--) out(buf, n, &pos, ' ');
    }
    if (n) buf[pos < n ? pos : n - 1] = 0;
    return (int)pos;
}

int snprintf(char *buf, usize n, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    int r = vsnprintf(buf, n, fmt, ap);
    va_end(ap);
    return r;
}

/* buffer circular do log, exibido pelo app Console */
#define KLOG_SIZE 32768
char klog_buf[KLOG_SIZE];
usize klog_len;

static void klog_put(char c) {
    if (klog_len >= KLOG_SIZE - 1) {
        memmove(klog_buf, klog_buf + KLOG_SIZE / 2, KLOG_SIZE / 2);
        klog_len -= KLOG_SIZE / 2;
    }
    klog_buf[klog_len++] = c;
    klog_buf[klog_len] = 0;
}

void kprintf(const char *fmt, ...) {
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    u64 f = irq_save();
    for (char *p = buf; *p; p++) {
        if (*p == '\n') serial_putc('\r');
        serial_putc(*p);
        klog_put(*p);
    }
    irq_restore(f);
}

extern void panic_screen(const char *msg, int_frame_t *f);

void panic(const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    cli();
    kprintf("\n*** PANIC: %s\n", buf);
    panic_screen(buf, NULL);
    for (;;) hlt();
}
