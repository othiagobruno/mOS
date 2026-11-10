/* CMTime, CMTimeRange, relógio e fila de buffers. Sem Objective-C.
 * O arredondamento padrão é metade para longe de zero, como o CMTime da Apple.
 * Infinito, indefinido e inválido não viram um número. Overflow devolve inválido. */
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#define I64_MAX 9223372036854775807LL
#define I64_MIN (-I64_MAX - 1)
static int is_nan(double x) { return x != x; }
static int is_inf(double x) { return x != 0 && x == 2 * x; }
static double dnan(void) { return 0.0 / 0.0; }
static double dpos(void) { return 1.0 / 0.0; }
typedef long long i64;
typedef int i32;
typedef unsigned u32;
typedef struct { i64 value; i32 timescale; u32 flags; i64 epoch; } cm_time_t;
typedef struct { cm_time_t start, duration; } cm_range_t;
typedef struct { cm_time_t duration, presentationTimeStamp, decodeTimeStamp; } cm_timing_t;

enum {
    CMT_VALID = 1u,
    CMT_ROUNDED = 2u,
    CMT_POS_INF = 4u,
    CMT_NEG_INF = 8u,
    CMT_INDEF = 16u
};
static const cm_time_t T_BAD = {0, 0, 0, 0};
const cm_time_t kCMTimePositiveInfinity = {0, 0, CMT_VALID | CMT_POS_INF, 0};
const cm_time_t kCMTimeNegativeInfinity = {0, 0, CMT_VALID | CMT_NEG_INF, 0};
const cm_range_t kCMTimeRangeInvalid = {{0, 0, 0, 0}, {0, 0, 0, 0}};
const cm_timing_t kCMTimingInfoInvalid = {{0, 0, 0, 0}, {0, 0, 0, 0}, {0, 0, 0, 0}};

static int is_valid(cm_time_t t) { return (t.flags & CMT_VALID) != 0; }
static int is_num(cm_time_t t) { return is_valid(t) && !(t.flags & (CMT_POS_INF | CMT_NEG_INF | CMT_INDEF)) && t.timescale > 0; }
static int is_pos(cm_time_t t) { return is_valid(t) && (t.flags & CMT_POS_INF); }
static int is_neg(cm_time_t t) { return is_valid(t) && (t.flags & CMT_NEG_INF); }
static int is_indef(cm_time_t t) { return is_valid(t) && (t.flags & CMT_INDEF); }
static cm_time_t indef(void) { cm_time_t t = {0, 0, CMT_VALID | CMT_INDEF, 0}; return t; }

static int mul_ok(i64 a, i64 b, i64 *o) {
    __int128 p = (__int128)a * b;
    if (p > (__int128)I64_MAX || p < (__int128)I64_MIN) return 0;
    *o = (i64)p;
    return 1;
}
cm_time_t CMTimeAdd(cm_time_t a, cm_time_t b);
cm_time_t CMTimeSubtract(cm_time_t a, cm_time_t b);
static int add_ok(i64 a, i64 b, i64 *o) {
    __int128 p = (__int128)a + b;
    if (p > (__int128)I64_MAX || p < (__int128)I64_MIN) return 0;
    *o = (i64)p;
    return 1;
}

cm_time_t CMTimeMake(i64 value, i32 timescale) {
    if (timescale <= 0) return T_BAD;
    cm_time_t t = {value, timescale, CMT_VALID, 0};
    return t;
}
cm_time_t CMTimeMakeWithSeconds(double seconds, i32 timescale) {
    if (timescale <= 0 || is_nan(seconds)) return T_BAD;
    if (is_inf(seconds)) return seconds > 0 ? kCMTimePositiveInfinity : kCMTimeNegativeInfinity;
    long double x = (long double)seconds * (long double)timescale;
    if (x > (long double)I64_MAX || x < (long double)I64_MIN) return T_BAD;
    long double ax = x < 0 ? -x : x;
    i64 whole = (i64)x;
    long double frac = ax - (long double)(whole < 0 ? -whole : whole);
    int rounded = frac > 0.0000000000001L;
    if (frac >= 0.5L) whole += x < 0 ? -1 : 1;
    cm_time_t t = {whole, timescale, CMT_VALID | (rounded ? CMT_ROUNDED : 0), 0};
    return t;
}
double CMTimeGetSeconds(cm_time_t t) {
    if (!is_valid(t) || is_indef(t)) return dnan();
    if (is_pos(t)) return dpos();
    if (is_neg(t)) return -dpos();
    if (t.timescale <= 0) return dnan();
    return (double)t.value / (double)t.timescale;
}

/* 1 metade para longe de zero, 2 para zero, 3 para longe de zero, 4 para −∞, 5 para +∞ */
cm_time_t CMTimeConvertScale(cm_time_t t, i32 scale, u32 method) {
    if (!is_num(t) || scale <= 0) {
        if (is_pos(t) || is_neg(t) || is_indef(t) || !is_valid(t)) return t;
        return T_BAD;
    }
    if (t.timescale == scale) return t;
    i64 den = t.timescale;
    i64 qa = t.value / den, ra = t.value % den;
    i64 q = qa * (i64)scale;
    i64 prod;
    if (!mul_ok(ra, scale, &prod)) return T_BAD;
    i64 q2 = prod / den, r = prod % den;
    if (!add_ok(q, q2, &q)) return T_BAD;
    int rounded = r != 0;
    if (r) {
        int neg = t.value < 0;
        i64 ar = r < 0 ? -r : r;
        switch (method) {
        case 2: break;
        case 3: q += neg ? -1 : 1; break;
        case 4: if (neg) q -= 1; break;
        case 5: if (!neg) q += 1; break;
        default:
            if (2 * ar >= den) q += neg ? -1 : 1;
        }
    }
    t.value = q;
    t.timescale = scale;
    if (rounded) t.flags |= CMT_ROUNDED;
    else t.flags &= ~CMT_ROUNDED;
    return t;
}

i32 CMTimeCompare(cm_time_t a, cm_time_t b) {
    if (!is_valid(a) || !is_valid(b) || is_indef(a) || is_indef(b)) return 0;
    if (is_pos(a) && is_pos(b)) return 0;
    if (is_neg(a) && is_neg(b)) return 0;
    if (is_pos(a) || is_neg(b)) return 1;
    if (is_neg(a) || is_pos(b)) return -1;
    if (a.epoch != b.epoch) return 0;
    cm_time_t d = CMTimeSubtract(a, b);
    if (!is_num(d)) return 0;
    return d.value < 0 ? -1 : d.value > 0 ? 1 : 0;
}

static cm_time_t num_bin(i64 value, i32 scale, i64 epoch, u32 extra) {
    if (scale <= 0) return T_BAD;
    cm_time_t t = {value, scale, CMT_VALID | extra, epoch};
    return t;
}
static i32 gcd_i(i32 a, i32 b) {
    if (a < 0) a = -a;
    if (b < 0) b = -b;
    while (b) { i32 t = a % b; a = b; b = t; }
    return a ? a : 1;
}
cm_time_t CMTimeAdd(cm_time_t a, cm_time_t b) {
    if (!is_valid(a) || !is_valid(b)) return T_BAD;
    if (is_indef(a) || is_indef(b)) return indef();
    if ((is_pos(a) && is_neg(b)) || (is_neg(a) && is_pos(b))) return T_BAD;
    if (is_pos(a) || is_pos(b)) return kCMTimePositiveInfinity;
    if (is_neg(a) || is_neg(b)) return kCMTimeNegativeInfinity;
    if (a.epoch != b.epoch) return T_BAD;
    if (a.timescale == b.timescale) {
        i64 v;
        if (!add_ok(a.value, b.value, &v)) return T_BAD;
        return num_bin(v, a.timescale, a.epoch, 0);
    }
    i64 av, bv, sum;
    i32 sc;
    __int128 scale = (__int128)a.timescale * b.timescale;
    if (scale > 2147483647) return T_BAD;
    if (!mul_ok(a.value, b.timescale, &av) || !mul_ok(b.value, a.timescale, &bv) || !add_ok(av, bv, &sum)) return T_BAD;
    sc = (i32)scale;
    i32 g = gcd_i((i32)(sum < 0 ? -sum : sum) > 0 ? 1 : 1, sc); /* reduzido abaixo */
    (void)g;
    /* reduz valor/escala por um divisor comum da escala e do valor, se couber */
    i64 avs = sum < 0 ? -sum : sum;
    i32 gs = gcd_i(sc, (i32)(avs > 2147483647 ? 1 : (i32)avs));
    if (gs > 1 && avs <= 2147483647) { sum /= gs; sc /= gs; }
    return num_bin(sum, sc, a.epoch, 0);
}
cm_time_t CMTimeSubtract(cm_time_t a, cm_time_t b) {
    if (is_num(b) && b.value == I64_MIN) return T_BAD;
    if (is_num(b)) b.value = -b.value;
    else if (is_pos(b)) b = kCMTimeNegativeInfinity;
    else if (is_neg(b)) b = kCMTimePositiveInfinity;
    return CMTimeAdd(a, b);
}
cm_time_t CMTimeMultiply(cm_time_t t, i32 m) {
    if (!is_valid(t)) return T_BAD;
    if (is_indef(t)) return t;
    if (is_pos(t) || is_neg(t)) {
        if (m == 0) return T_BAD;
        if (m > 0) return t;
        return is_pos(t) ? kCMTimeNegativeInfinity : kCMTimePositiveInfinity;
    }
    i64 v;
    if (!mul_ok(t.value, m, &v)) return T_BAD;
    return num_bin(v, t.timescale, t.epoch, 0);
}
cm_time_t CMTimeMultiplyByFloat64(cm_time_t t, double m) {
    if (!is_valid(t) || is_nan(m)) return T_BAD;
    if (is_indef(t)) return t;
    if (is_pos(t) || is_neg(t)) {
        if (m == 0 || is_nan(m)) return T_BAD;
        int pos = is_pos(t);
        if (m < 0) pos = !pos;
        return pos ? kCMTimePositiveInfinity : kCMTimeNegativeInfinity;
    }
    if (is_inf(m)) return m > 0 ? (t.value < 0 ? kCMTimeNegativeInfinity : t.value > 0 ? kCMTimePositiveInfinity : T_BAD)
                               : (t.value < 0 ? kCMTimePositiveInfinity : t.value > 0 ? kCMTimeNegativeInfinity : T_BAD);
    return CMTimeMakeWithSeconds(CMTimeGetSeconds(t) * m, t.timescale);
}
cm_time_t CMTimeMultiplyByRatio(cm_time_t t, i32 mul, i32 div) {
    if (!is_valid(t) || div == 0) return T_BAD;
    if (is_indef(t)) return t;
    if (is_pos(t) || is_neg(t)) {
        if (mul == 0) return T_BAD;
        int pos = is_pos(t);
        if ((mul < 0) != (div < 0)) pos = !pos;
        return pos ? kCMTimePositiveInfinity : kCMTimeNegativeInfinity;
    }
    i64 v;
    if (!mul_ok(t.value, mul, &v)) return T_BAD;
    cm_time_t u = num_bin(v, t.timescale, t.epoch, 0);
    /* value * mul / div na escala atual: converte via escala * div no numerador já aplicado */
    i64 q = v / div, r = v % div;
    int neg = (v < 0) != (div < 0);
    i64 ar = r < 0 ? -r : r;
    i64 ad = div < 0 ? -(i64)div : div;
    if (ar && 2 * ar >= ad) q += neg ? -1 : 1;
    u.value = q;
    if (r) u.flags |= CMT_ROUNDED;
    return u;
}
cm_time_t CMTimeAbsoluteValue(cm_time_t t) {
    if (!is_valid(t)) return T_BAD;
    if (is_indef(t)) return t;
    if (is_neg(t)) return kCMTimePositiveInfinity;
    if (is_pos(t)) return t;
    if (t.value < 0) {
        if (t.value == I64_MIN) return T_BAD;
        t.value = -t.value;
    }
    return t;
}
cm_time_t CMTimeMinimum(cm_time_t a, cm_time_t b) {
    if (!is_valid(a) || !is_valid(b) || is_indef(a) || is_indef(b)) return T_BAD;
    if (is_num(a) && is_num(b) && a.epoch != b.epoch) return T_BAD;
    return CMTimeCompare(a, b) <= 0 ? a : b;
}
cm_time_t CMTimeMaximum(cm_time_t a, cm_time_t b) {
    if (!is_valid(a) || !is_valid(b) || is_indef(a) || is_indef(b)) return T_BAD;
    if (is_num(a) && is_num(b) && a.epoch != b.epoch) return T_BAD;
    return CMTimeCompare(a, b) >= 0 ? a : b;
}

cm_range_t CMTimeRangeMake(cm_time_t start, cm_time_t duration) {
    cm_range_t r = {T_BAD, T_BAD};
    if (!is_valid(start) || !is_num(duration) || duration.value < 0) return kCMTimeRangeInvalid;
    r.start = start;
    r.duration = duration;
    return r;
}
cm_time_t CMTimeRangeGetEnd(cm_range_t r) { return CMTimeAdd(r.start, r.duration); }
int CMTimeRangeContainsTime(cm_range_t r, cm_time_t t) {
    if (!is_valid(r.start) || !is_num(r.duration) || !is_valid(t)) return 0;
    if (CMTimeCompare(t, r.start) < 0) return 0;
    cm_time_t end = CMTimeRangeGetEnd(r);
    if (!is_valid(end)) return 0;
    return CMTimeCompare(t, end) < 0;
}
int CMTimeRangeContainsTimeRange(cm_range_t r, cm_range_t o) {
    if (!CMTimeRangeContainsTime(r, o.start)) return 0;
    cm_time_t end = CMTimeRangeGetEnd(o);
    if (!is_valid(end)) return 0;
    cm_time_t rend = CMTimeRangeGetEnd(r);
    return CMTimeCompare(end, rend) <= 0;
}
cm_range_t CMTimeRangeGetIntersection(cm_range_t a, cm_range_t b) {
    if (!is_valid(a.start) || !is_valid(b.start) || !is_num(a.duration) || !is_num(b.duration)) return kCMTimeRangeInvalid;
    cm_time_t as = a.start, ae = CMTimeRangeGetEnd(a), bs = b.start, be = CMTimeRangeGetEnd(b);
    if (!is_valid(ae) || !is_valid(be)) return kCMTimeRangeInvalid;
    cm_time_t s = CMTimeCompare(as, bs) >= 0 ? as : bs;
    cm_time_t e = CMTimeCompare(ae, be) <= 0 ? ae : be;
    if (CMTimeCompare(s, e) > 0) return kCMTimeRangeInvalid;
    cm_time_t d = CMTimeSubtract(e, s);
    if (!is_num(d) || d.value < 0) return kCMTimeRangeInvalid;
    return CMTimeRangeMake(s, d);
}
cm_time_t CMTimeClampToRange(cm_time_t t, cm_range_t r) {
    if (!is_valid(t) || !is_valid(r.start) || !is_num(r.duration)) return T_BAD;
    if (CMTimeCompare(t, r.start) < 0) return r.start;
    cm_time_t end = CMTimeRangeGetEnd(r);
    if (!is_valid(end)) return T_BAD;
    if (CMTimeCompare(t, end) > 0) return end;
    return t;
}

/* ---- relógio e timebase. O agora é injetável no teste; no sistema é mach_absolute_time. */
extern unsigned long long mach_absolute_time(void) __attribute__((weak));
static double host_now(void) {
    if (mach_absolute_time) return (double)mach_absolute_time() / 1e9;
    return 0;
}
static double (*g_now)(void) = host_now;
void cm_set_now(double (*fn)(void)) { g_now = fn ? fn : host_now; }

typedef struct cm_sync {
    int kind; /* 1 relógio, 2 timebase */
    struct cm_sync *source;
    double rate, anchor_src, anchor_tb;
    void **timers;
    double *timer_at;
    int nt, ct;
} cm_sync_t;

static double sync_seconds(cm_sync_t *s) {
    if (!s) return dnan();
    if (s->kind == 1) return g_now ? g_now() : 0;
    double src = sync_seconds(s->source);
    return s->anchor_tb + (src - s->anchor_src) * s->rate;
}
void *CMClockGetHostTimeClock(void) {
    static cm_sync_t host;
    host.kind = 1;
    host.rate = 1;
    return &host;
}
int CMAudioClockCreate(void *a, void **out) {
    (void)a;
    if (!out) return -12752;
    cm_sync_t *c = calloc(1, sizeof *c);
    if (!c) return -12752;
    c->kind = 1;
    c->rate = 1;
    *out = c;
    return 0;
}
cm_time_t CMClockGetTime(void *clock) {
    cm_sync_t *c = clock;
    if (!c) return T_BAD;
    return CMTimeMakeWithSeconds(sync_seconds(c), 1000000000);
}
int CMTimebaseCreateWithSourceClock(void *a, void *clock, void **out) {
    (void)a;
    if (!out || !clock) return -12752;
    cm_sync_t *t = calloc(1, sizeof *t);
    if (!t) return -12752;
    t->kind = 2;
    t->source = clock;
    t->rate = 1;
    t->anchor_src = sync_seconds(clock);
    t->anchor_tb = 0;
    *out = t;
    return 0;
}
double CMTimebaseGetRate(void *tb) { cm_sync_t *t = tb; return t && t->kind == 2 ? t->rate : 0; }
cm_time_t CMTimebaseGetTime(void *tb) {
    cm_sync_t *t = tb;
    if (!t || t->kind != 2) return T_BAD;
    return CMTimeMakeWithSeconds(sync_seconds(t), 1000000000);
}
int CMTimebaseSetRate(void *tb, double rate) {
    cm_sync_t *t = tb;
    if (!t || t->kind != 2 || is_nan(rate)) return -12752;
    double now = sync_seconds(t);
    double src = sync_seconds(t->source);
    t->anchor_tb = now;
    t->anchor_src = src;
    t->rate = rate;
    return 0;
}
int CMTimebaseSetTime(void *tb, cm_time_t time) {
    cm_sync_t *t = tb;
    if (!t || t->kind != 2 || !is_num(time)) return -12752;
    t->anchor_src = sync_seconds(t->source);
    t->anchor_tb = CMTimeGetSeconds(time);
    return 0;
}
static int timer_slot(cm_sync_t *t, void *src, int make) {
    for (int i = 0; i < t->nt; i++) if (t->timers[i] == src) return i;
    if (!make) return -1;
    if (t->nt == t->ct) {
        int n = t->ct ? t->ct * 2 : 4;
        void **p = realloc(t->timers, (size_t)n * sizeof(void *));
        if (!p) return -1;
        t->timers = p;
        double *d = realloc(t->timer_at, (size_t)n * sizeof(double));
        if (!d) return -1;
        t->timer_at = d;
        t->ct = n;
    }
    t->timers[t->nt] = src;
    t->timer_at[t->nt] = dnan();
    return t->nt++;
}
int CMTimebaseAddTimerDispatchSource(void *tb, void *src) {
    cm_sync_t *t = tb;
    if (!t || t->kind != 2 || !src) return -12752;
    return timer_slot(t, src, 1) < 0 ? -12752 : 0;
}
int CMTimebaseRemoveTimerDispatchSource(void *tb, void *src) {
    cm_sync_t *t = tb;
    if (!t || t->kind != 2) return -12752;
    int i = timer_slot(t, src, 0);
    if (i < 0) return 0;
    t->timers[i] = t->timers[t->nt - 1];
    t->timer_at[i] = t->timer_at[t->nt - 1];
    t->nt--;
    return 0;
}
int CMTimebaseSetTimerDispatchSourceNextFireTime(void *tb, void *src, cm_time_t fire, u32 flags) {
    (void)flags;
    cm_sync_t *t = tb;
    if (!t || t->kind != 2 || !is_num(fire)) return -12752;
    int i = timer_slot(t, src, 0);
    if (i < 0) return -12752;
    t->timer_at[i] = CMTimeGetSeconds(fire);
    return 0;
}
double cm_timebase_timer_seconds(void *tb, void *src) {
    cm_sync_t *t = tb;
    if (!t) return dnan();
    int i = timer_slot(t, src, 0);
    return i < 0 ? dnan() : t->timer_at[i];
}
cm_time_t CMSyncConvertTime(cm_time_t time, void *from, void *to) {
    if (!is_num(time) || !from || !to) return !is_valid(time) ? T_BAD : time;
    double sec = CMTimeGetSeconds(time);
    /* o tempo numérico já está na linha do relógio de origem; expressa no destino pela diferença de agora */
    double delta = sync_seconds(to) - sync_seconds(from);
    return CMTimeMakeWithSeconds(sec + delta, time.timescale > 0 ? time.timescale : 1000000000);
}
double CMSyncGetRelativeRate(void *of, void *relative) {
    cm_sync_t *a = of, *b = relative;
    if (!a || !b) return dnan();
    double ra = a->kind == 2 ? a->rate : 1;
    double rb = b->kind == 2 ? b->rate : 1;
    if (rb == 0) return dnan();
    return ra / rb;
}

typedef struct { unsigned version; void *refcon; void *gd, *gp, *gdur, *ready; int (*compare)(void *, void *, void *); void *note; } cm_qcb_t;
typedef struct { void **item; int n, cap; cm_qcb_t cb; } cm_q_t;
int CMBufferQueueCreate(void *a, int capacity, const void *callbacks, void **out) {
    (void)a; (void)capacity;
    if (!out) return -12757;
    cm_q_t *q = calloc(1, sizeof *q);
    if (!q) return -12757;
    if (callbacks) memcpy(&q->cb, callbacks, sizeof q->cb);
    *out = q;
    return 0;
}
static void q_retain(void *p) { (void)p; }
int CMBufferQueueEnqueue(void *q_, void *buf) {
    cm_q_t *q = q_;
    if (!q || !buf) return -12757;
    if (q->n == q->cap) {
        int n = q->cap ? q->cap * 2 : 8;
        void **p = realloc(q->item, (size_t)n * sizeof(void *));
        if (!p) return -12757;
        q->item = p; q->cap = n;
    }
    int at = q->n;
    if (q->cb.compare) {
        at = 0;
        while (at < q->n && q->cb.compare(q->item[at], buf, q->cb.refcon) <= 0) at++;
        memmove(q->item + at + 1, q->item + at, (size_t)(q->n - at) * sizeof(void *));
    }
    q->item[at] = buf;
    q->n++;
    q_retain(buf);
    return 0;
}
void *CMBufferQueueDequeueAndRetain(void *q_) {
    cm_q_t *q = q_;
    if (!q || !q->n) return 0;
    void *b = q->item[0];
    memmove(q->item, q->item + 1, (size_t)(q->n - 1) * sizeof(void *));
    q->n--;
    return b;
}
int CMBufferQueueReset(void *q_) {
    cm_q_t *q = q_;
    if (!q) return -12757;
    q->n = 0;
    return 0;
}
static int cmp_unspec(void *a, void *b, void *ref) { (void)a; (void)b; (void)ref; return 0; }
const void *CMBufferQueueGetCallbacksForUnsortedSampleBuffers(void) {
    static cm_qcb_t cb;
    cb.version = 0;
    cb.compare = cmp_unspec;
    return &cb;
}

typedef struct { i32 width, height; } cm_dim_t;
cm_dim_t cm_presentation_dimensions(i32 w, i32 h, int par_n, int par_d, int clean_w, int clean_h, int use_par, int use_clean) {
    cm_dim_t d = {w, h};
    if (use_clean && clean_w > 0 && clean_h > 0) { d.width = clean_w; d.height = clean_h; }
    if (use_par && par_n > 0 && par_d > 0) d.width = (i32)((i64)d.width * par_n / par_d);
    return d;
}
int cm_copy_asbd(const void *src, int has, void *dst, unsigned n) {
    if (!has || !src || !dst || n < 40) return -12710;
    memcpy(dst, src, 40);
    return 0;
}
int cm_timing_copy(const cm_timing_t *src, long nsrc, cm_timing_t *dst, long ndst, long *need) {
    if (need) *need = nsrc;
    if (!src || nsrc < 0) return -12743;
    if (!dst) return 0;
    if (ndst < nsrc) return -12743;
    memcpy(dst, src, (size_t)nsrc * sizeof *src);
    return 0;
}
