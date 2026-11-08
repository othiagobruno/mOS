/* suíte diferencial C: strings e memória (implementações próprias), ordenação, parsing */
#include "rt.h"

static size_t s_len(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
static int s_cmp(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return (int)(unsigned char)*a - (int)(unsigned char)*b; }
static char *s_chr(const char *s, int c) { for (; *s; s++) if (*s == (char)c) return (char *)s; return c ? 0 : (char *)s; }
static void m_cpy(void *d, const void *s, size_t n) { unsigned char *p = d; const unsigned char *q = s; for (size_t i = 0; i < n; i++) p[i] = q[i]; }
static void m_move(void *d, const void *s, size_t n) {
    unsigned char *p = d; const unsigned char *q = s;
    if (p < q) for (size_t i = 0; i < n; i++) p[i] = q[i]; else for (size_t i = n; i-- > 0;) p[i] = q[i];
}
static int64_t s_tol(const char *s, int base, const char **end) {
    int neg = 0; while (*s == ' ') s++;
    if (*s == '-' || *s == '+') neg = *s++ == '-';
    uint64_t v = 0;
    for (;; s++) {
        int d = (*s >= '0' && *s <= '9') ? *s - '0' : (*s >= 'a' && *s <= 'z') ? *s - 'a' + 10 : (*s >= 'A' && *s <= 'Z') ? *s - 'A' + 10 : 99;
        if (d >= base) break;
        v = v * (uint64_t)base + (uint64_t)d;
    }
    *end = s;
    return neg ? (int64_t)(0 - v) : (int64_t)v;
}
static int u_toa(uint64_t v, int base, char *out) {
    char t[70]; int n = 0;
    do { t[n++] = "0123456789abcdefghijklmnopqrstuvwxyz"[v % (uint64_t)base]; v /= (uint64_t)base; } while (v);
    for (int i = 0; i < n; i++) out[i] = t[n - 1 - i];
    out[n] = 0; return n;
}

static int32_t arr[3000], tmp[3000];
static void qs(int32_t *a, int lo, int hi) {
    while (lo < hi) {
        int32_t p = a[(lo + hi) / 2]; int i = lo, j = hi;
        while (i <= j) { while (a[i] < p) i++; while (a[j] > p) j--; if (i <= j) { int32_t t = a[i]; a[i] = a[j]; a[j] = t; i++; j--; } }
        if (j - lo < hi - i) { qs(a, lo, j); lo = i; } else { qs(a, i, hi); hi = j; }
    }
}
static void msort(int32_t *a, int n) {
    for (int w = 1; w < n; w *= 2) {
        for (int i = 0; i < n; i += 2 * w) {
            int m = i + w < n ? i + w : n, e = i + 2 * w < n ? i + 2 * w : n, p = i, q = m, k = i;
            while (p < m && q < e) tmp[k++] = a[p] <= a[q] ? a[p++] : a[q++];
            while (p < m) tmp[k++] = a[p++];
            while (q < e) tmp[k++] = a[q++];
        }
        m_cpy(a, tmp, (size_t)n * 4);
    }
}
static void sift(int32_t *a, int r, int e) {
    for (;;) {
        int c = 2 * r + 1; if (c >= e) break;
        if (c + 1 < e && a[c + 1] > a[c]) c++;
        if (a[r] >= a[c]) break;
        int32_t t = a[r]; a[r] = a[c]; a[c] = t; r = c;
    }
}
static void heapsort(int32_t *a, int n) {
    for (int s = n / 2 - 1; s >= 0; s--) sift(a, s, n);
    for (int e = n - 1; e > 0; e--) { int32_t t = a[0]; a[0] = a[e]; a[e] = t; sift(a, 0, e); }
}

static char buf[1 << 14], buf2[1 << 14];
static void run(void) {
    put_s("c_str\n");
    hsh h; h_init(&h);
    for (int i = 0; i < (int)sizeof buf - 1; i++) buf[i] = (char)(' ' + (rnd64() % 95));
    buf[sizeof buf - 1] = 0;
    for (int k = 0; k < 400; k++) {
        size_t off = rnd64() % 4000, n = rnd64() % 3000, to = rnd64() % 4000;
        m_move(buf + to, buf + off, n); h_u(&h, (uint64_t)(unsigned char)buf[to + n / 2]);
        m_cpy(buf2 + (k & 15), buf + off, n); buf2[(k & 15) + n] = 0;
        h_u(&h, s_len(buf2 + (k & 15)));
        char *c = s_chr(buf + off, 'Q'); h_u(&h, c ? (uint64_t)(c - buf) : 0);
        h_u(&h, (uint64_t)(int64_t)s_cmp(buf + off, buf + to));
    }
    for (int i = 0; i < (int)sizeof buf - 1; i++) { char c = buf[i]; if (c >= 'a' && c <= 'z') buf[i] = (char)(c - 32); }
    for (size_t i = 0; i < sizeof buf; i++) h_u(&h, (unsigned char)buf[i]);
    kv_x("mem/str", h.h);

    h_init(&h);
    char num[80];
    for (int k = 0; k < 3000; k++) {
        uint64_t v = rnd64() >> (k % 64);
        int base = 2 + k % 35;
        int n = u_toa(v, base, num); h_u(&h, (uint64_t)n);
        const char *e; int64_t back = s_tol(num, base, &e);
        h_u(&h, (uint64_t)back == v); h_u(&h, (uint64_t)(e - num));
        for (int i = 0; i < n; i++) h_u(&h, (unsigned char)num[i]);
    }
    const char *e; kv_i("strtol(-12345)", s_tol("  -12345xyz", 10, &e)); kv_i("strtol(ff)", s_tol("fF", 16, &e));
    kv_x("conversao numerica", h.h);

    for (int r = 0; r < 3; r++) {
        for (int i = 0; i < 3000; i++) arr[i] = (int32_t)(rnd64() >> 33) - (1 << 30);
        if (r == 0) qs(arr, 0, 2999); else if (r == 1) msort(arr, 3000); else heapsort(arr, 3000);
        int ok = 1; for (int i = 1; i < 3000; i++) ok &= arr[i - 1] <= arr[i];
        h_init(&h); for (int i = 0; i < 3000; i++) h_u(&h, (uint64_t)(uint32_t)arr[i]);
        put_s(r == 0 ? "quicksort" : r == 1 ? "mergesort" : "heapsort"); put_s(ok ? " ordenado " : " ERRO "); put_x(h.h, 16); put_c('\n');
        int32_t key = arr[1234]; int lo = 0, hi = 2999;
        while (lo < hi) { int mid = (lo + hi) / 2; if (arr[mid] < key) lo = mid + 1; else hi = mid; }
        kv_i("busca binaria", lo);
    }
}
