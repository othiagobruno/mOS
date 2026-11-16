/* Centro local de notificações (un_center.h). Sem classes: o teste de host e o ObjC usam estas funções. */
#ifdef CX_UN_HOST
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#else
#include "sys.h"
#include "foundation.h"
#include "libc.h"
typedef struct cx_file cx_file;
void *fopen(const char *p, const char *m);
unsigned long fread(void *b, unsigned long s, unsigned long n, void *f);
unsigned long fwrite(const void *b, unsigned long s, unsigned long n, void *f);
int fclose(void *f);
int rename(const char *a, const char *b);
double strtod(const char *s, char **end);
#define atof(s) strtod((s), 0)
#define FILE void
#endif
#include "un_center.h"

enum { UN_MAX_DELIVERED = 64, UN_MAX_PENDING = 512 };

static cx_un_req_t *g_pend, *g_deliv;
static int g_npend, g_ndeliv, g_cappend, g_capdeliv;
static cx_un_category_t *g_cats;
static int g_ncats;
static int g_auth, g_tok = 1, g_badge, g_tz;
static double g_clock;
static char *g_store;
static cx_un_present_fn g_present;
static cx_un_receive_fn g_receive;
static void *g_ctx;
static cx_un_sink_t g_sink;
static int g_loading, g_noauto;
void cx_un_set_auto_flush(int on) { g_noauto = !on; }

/* ---------------------------------------------------------------- texto */
static char *sdup(const char *s) {
    size_t n;
    char *r;
    if (!s) return 0;
    n = strlen(s);
    r = malloc(n + 1);
    if (r) memcpy(r, s, n + 1);
    return r;
}
static int same(const char *a, const char *b) { return strcmp(a ? a : "", b ? b : "") == 0; }

static void req_free(cx_un_req_t *r) {
    free(r->ident); free(r->title); free(r->subtitle); free(r->body); free(r->category); free(r->thread);
    free(r->sound); free(r->info); free(r->attach); free(r->target); free(r->launch_image);
    memset(r, 0, sizeof *r);
}
static void req_copy(cx_un_req_t *d, const cx_un_req_t *s) {
    *d = *s;
    d->ident = sdup(s->ident ? s->ident : "");
    d->title = sdup(s->title); d->subtitle = sdup(s->subtitle); d->body = sdup(s->body);
    d->category = sdup(s->category); d->thread = sdup(s->thread); d->sound = sdup(s->sound);
    d->info = sdup(s->info); d->attach = sdup(s->attach); d->target = sdup(s->target);
    d->launch_image = sdup(s->launch_image);
}
static cx_un_req_t *push(cx_un_req_t **arr, int *n, int *cap) {
    if (*n >= *cap) {
        int nc = *cap ? *cap * 2 : 16;
        cx_un_req_t *na = realloc(*arr, sizeof *na * (size_t)nc);
        if (!na) return 0;
        *arr = na;
        *cap = nc;
    }
    cx_un_req_t *r = &(*arr)[(*n)++];
    memset(r, 0, sizeof *r);
    return r;
}
static void drop(cx_un_req_t *arr, int *n, int i) {
    req_free(&arr[i]);
    memmove(&arr[i], &arr[i + 1], sizeof *arr * (size_t)(*n - i - 1));
    (*n)--;
}

/* ---------------------------------------------------------------- relógio e calendário */
double cx_un_now(void) {
    if (g_clock > 0) return g_clock;
#ifdef CX_UN_HOST
    return (double)time(0);
#else
    return cx_now();
#endif
}
void cx_un_set_clock(double t) { g_clock = t; cx_un_flush(); }
void cx_un_set_tz_offset(int seconds) { g_tz = seconds; }

static void civil_from_unix(long long z, int *Y, int *M, int *D, int *h, int *mi, int *s, int *wd) {
    long long days, rem, era, doe, yoe, yy, doy, mp;
    days = z >= 0 ? z / 86400 : -((-z + 86399) / 86400);
    rem = z - days * 86400;
    *wd = (int)(((days % 7) + 7 + 4) % 7) + 1;           /* 1970-01-01 foi quinta (5) */
    days += 719468;
    era = (days >= 0 ? days : days - 146096) / 146097;
    doe = days - era * 146097;
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    yy = yoe + era * 400;
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    mp = (5 * doy + 2) / 153;
    *D = (int)(doy - (153 * mp + 2) / 5 + 1);
    *M = (int)(mp < 10 ? mp + 3 : mp - 9);
    *Y = (int)(yy + (*M <= 2));
    *h = (int)(rem / 3600);
    *mi = (int)((rem % 3600) / 60);
    *s = (int)(rem % 60);
}
static int set_(int v) { return v >= 0; }
/* próxima data (Unix, UTC) depois de from que casa com os componentes em hora local; -1 = nunca.
 * Regra do NSCalendar: as unidades menores que a menor informada valem 0; as maiores não informadas são livres. */
double cx_un_calendar_next(const cx_un_cal_t *c, double from, int repeats) {
    (void)repeats;
    if (!c) return -1;
    cx_un_cal_t k = *c;
    int small = set_(k.second) ? 0 : set_(k.minute) ? 1 : set_(k.hour) ? 2 : (set_(k.day) || set_(k.weekday) || set_(k.month) || set_(k.year)) ? 3 : -1;
    if (small < 0) return -1;
    if (small >= 1) k.second = 0;
    if (small >= 2) k.minute = 0;
    if (small >= 3) k.hour = 0;
    if ((set_(k.month) && (k.month < 1 || k.month > 12)) || (set_(k.day) && (k.day < 1 || k.day > 31)) || k.hour > 23 || k.minute > 59 ||
        k.second > 59 || (set_(k.weekday) && (k.weekday < 1 || k.weekday > 7)))
        return -1;
    long long lf = (long long)from + g_tz;
    long long day0 = lf >= 0 ? lf / 86400 : -((-lf + 86399) / 86400);
    for (long long d = day0; d < day0 + 366LL * 29; d++) {
        int Y, M, D, h, mi, s, wd;
        civil_from_unix(d * 86400, &Y, &M, &D, &h, &mi, &s, &wd);
        if (set_(k.year) && Y > k.year) return -1;
        if ((set_(k.year) && Y != k.year) || (set_(k.month) && M != k.month) || (set_(k.day) && D != k.day) || (set_(k.weekday) && wd != k.weekday)) continue;
        int h0 = set_(k.hour) ? k.hour : 0, h1 = set_(k.hour) ? k.hour : 23;
        for (int hh = h0; hh <= h1; hh++) {
            long long base = d * 86400 + hh * 3600LL;
            if (base + 3599 <= lf) continue;
            int m0 = set_(k.minute) ? k.minute : 0, m1 = set_(k.minute) ? k.minute : 59;
            for (int mm = m0; mm <= m1; mm++) {
                int s0 = set_(k.second) ? k.second : 0, s1 = set_(k.second) ? k.second : 59;
                for (int ss = s0; ss <= s1; ss++) {
                    long long t = base + mm * 60LL + ss;
                    if (t > lf) return (double)(t - g_tz);
                }
            }
        }
    }
    return -1;
}

/* ---------------------------------------------------------------- persistência */
typedef struct { char *b; size_t n, cap; } sbuf;
static void sb_put(sbuf *s, const char *t, size_t n) {
    if (s->n + n + 1 > s->cap) {
        size_t nc = s->cap ? s->cap * 2 : 1024;
        while (nc < s->n + n + 1) nc *= 2;
        char *nb = realloc(s->b, nc);
        if (!nb) return;
        s->b = nb;
        s->cap = nc;
    }
    memcpy(s->b + s->n, t, n);
    s->n += n;
    s->b[s->n] = 0;
}
static void sb_esc(sbuf *s, const char *t) {
    if (!t) { sb_put(s, "\\0", 2); return; }
    for (; *t; t++) {
        if (*t == '\\') sb_put(s, "\\\\", 2);
        else if (*t == '\t') sb_put(s, "\\t", 2);
        else if (*t == '\n') sb_put(s, "\\n", 2);
        else if (*t == '\r') sb_put(s, "\\r", 2);
        else sb_put(s, t, 1);
    }
}
static void sb_num(sbuf *s, double v) {
    char b[48];
    long long iv = (long long)v;
    if ((double)iv == v) snprintf(b, sizeof b, "%lld", iv);
    else snprintf(b, sizeof b, "%.6f", v);
    sb_put(s, b, strlen(b));
}
static void write_req(sbuf *s, const cx_un_req_t *r, char kind) {
    const char *str[11] = {r->ident, r->title, r->subtitle, r->body, r->category, r->thread, r->sound, r->info, r->attach, r->target, r->launch_image};
    double num[16] = {r->badge, r->interruption, r->relevance, r->source, r->trig, r->repeats, r->interval, r->cal.year, r->cal.month,
                      r->cal.day, r->cal.hour, r->cal.minute, r->cal.second, r->cal.weekday, r->fire_at, r->date};
    char k[3] = {kind, '\t', 0};
    sb_put(s, k, 2);
    for (int i = 0; i < 11; i++) { sb_esc(s, str[i]); sb_put(s, "\t", 1); }
    for (int i = 0; i < 16; i++) { sb_num(s, num[i]); sb_put(s, i < 15 ? "\t" : "\n", 1); }
}
static void save(void) {
    if (!g_store || g_loading) return;
    sbuf s = {0, 0, 0};
    char b[32];
    sb_put(&s, "mOS-UN 1\n", 9);
    snprintf(b, sizeof b, "B\t%d\n", g_badge);
    sb_put(&s, b, strlen(b));
    for (int i = 0; i < g_npend; i++) write_req(&s, &g_pend[i], 'P');
    for (int i = 0; i < g_ndeliv; i++) write_req(&s, &g_deliv[i], 'D');
    size_t pl = strlen(g_store);
    char *tmp = malloc(pl + 8);
    if (tmp) {
        memcpy(tmp, g_store, pl);
        memcpy(tmp + pl, ".novo", 6);
        FILE *f = fopen(tmp, "w");
        if (f) {
            int ok = fwrite(s.b, 1, s.n, f) == s.n;
            ok &= fclose(f) == 0;
            if (ok) rename(tmp, g_store);
        }
        free(tmp);
    }
    free(s.b);
}
/* campo escapado até o próximo \t ou \n; NULL para "\0" */
static char *field(char **p) {
    char *s = *p, *o, *w;
    size_t n = 0;
    while (s[n] && s[n] != '\t' && s[n] != '\n') n++;
    if (n == 2 && s[0] == '\\' && s[1] == '0') { *p = s + n + (s[n] ? 1 : 0); return 0; }
    o = malloc(n + 1);
    if (!o) { *p = s + n; return 0; }
    w = o;
    for (size_t i = 0; i < n; i++) {
        if (s[i] == '\\' && i + 1 < n) {
            char c = s[++i];
            *w++ = c == 't' ? '\t' : c == 'n' ? '\n' : c == 'r' ? '\r' : c;
        } else *w++ = s[i];
    }
    *w = 0;
    *p = s + n + (s[n] ? 1 : 0);
    return o;
}
static double fnum(char **p) { char *f = field(p); double v = f ? atof(f) : 0; free(f); return v; }
static void load(void) {
    FILE *f = fopen(g_store, "r");
    if (!f) return;
    sbuf s = {0, 0, 0};
    char buf[4096];
    unsigned long n;
    while ((n = fread(buf, 1, sizeof buf, f)) > 0) sb_put(&s, buf, n);
    fclose(f);
    if (!s.b || strncmp(s.b, "mOS-UN 1\n", 9)) { free(s.b); return; }
    char *p = s.b + 9;
    while (*p) {
        char kind = *p;
        if (kind == 'B' && p[1] == '\t') { p += 2; g_badge = (int)fnum(&p); continue; }
        if ((kind != 'P' && kind != 'D') || p[1] != '\t') { while (*p && *p != '\n') p++; if (*p) p++; continue; }
        p += 2;
        cx_un_req_t r;
        memset(&r, 0, sizeof r);
        char **str[11] = {&r.ident, &r.title, &r.subtitle, &r.body, &r.category, &r.thread, &r.sound, &r.info, &r.attach, &r.target, &r.launch_image};
        for (int i = 0; i < 11; i++) *str[i] = field(&p);
        r.badge = (int)fnum(&p); r.interruption = (int)fnum(&p); r.relevance = fnum(&p); r.source = (int)fnum(&p);
        r.trig = (int)fnum(&p); r.repeats = (int)fnum(&p); r.interval = fnum(&p);
        r.cal.year = (int)fnum(&p); r.cal.month = (int)fnum(&p); r.cal.day = (int)fnum(&p); r.cal.hour = (int)fnum(&p);
        r.cal.minute = (int)fnum(&p); r.cal.second = (int)fnum(&p); r.cal.weekday = (int)fnum(&p);
        r.fire_at = fnum(&p); r.date = fnum(&p);
        if (!r.ident) { req_free(&r); continue; }
        cx_un_req_t *d = kind == 'P' ? push(&g_pend, &g_npend, &g_cappend) : push(&g_deliv, &g_ndeliv, &g_capdeliv);
        if (d) *d = r; else req_free(&r);
    }
    free(s.b);
}
int cx_un_set_store(const char *path) {
    free(g_store);
    g_store = path ? sdup(path) : 0;
    if (!g_store) return 0;
    g_loading = 1;
    load();
    g_loading = 0;
    return g_npend + g_ndeliv;
}

/* ---------------------------------------------------------------- estado */
static void free_cats(void) {
    for (int i = 0; i < g_ncats; i++) {
        cx_un_category_t *c = &g_cats[i];
        for (int k = 0; k < c->nactions; k++) {
            free(c->actions[k].ident); free(c->actions[k].title); free(c->actions[k].button); free(c->actions[k].placeholder);
        }
        free(c->actions);
        free(c->ident);
    }
    free(g_cats);
    g_cats = 0;
    g_ncats = 0;
}
void cx_un_reset(void) {
    for (int i = 0; i < g_npend; i++) req_free(&g_pend[i]);
    for (int i = 0; i < g_ndeliv; i++) req_free(&g_deliv[i]);
    free(g_pend); free(g_deliv);
    g_pend = g_deliv = 0;
    g_npend = g_ndeliv = g_cappend = g_capdeliv = 0;
    free_cats();
    free(g_store);
    g_store = 0;
    g_auth = 0; g_tok = 1; g_badge = 0; g_tz = 0; g_clock = 0;
    g_present = 0; g_receive = 0; g_ctx = 0; g_noauto = 0;
    memset(&g_sink, 0, sizeof g_sink);
}
void cx_un_set_auth(int status) { g_auth = status; }
int cx_un_auth(void) { return g_auth; }
void cx_un_set_delegate(cx_un_present_fn present, cx_un_receive_fn receive, void *ctx) { g_present = present; g_receive = receive; g_ctx = ctx; }
void cx_un_set_sink(const cx_un_sink_t *sink) { if (sink) g_sink = *sink; else memset(&g_sink, 0, sizeof g_sink); }

int cx_un_count(int delivered) { return delivered ? g_ndeliv : g_npend; }
const cx_un_req_t *cx_un_at(int delivered, int i) {
    if (i < 0 || i >= (delivered ? g_ndeliv : g_npend)) return 0;
    return delivered ? &g_deliv[i] : &g_pend[i];
}
static int find_i(int delivered, const char *ident, int source) {
    int n = delivered ? g_ndeliv : g_npend;
    cx_un_req_t *a = delivered ? g_deliv : g_pend;
    for (int i = 0; i < n; i++) if (same(a[i].ident, ident) && (source < 0 || a[i].source == source)) return i;
    return -1;
}
const cx_un_req_t *cx_un_find(int delivered, const char *ident) { int i = find_i(delivered, ident, -1); return i < 0 ? 0 : cx_un_at(delivered, i); }

const cx_un_category_t *cx_un_category(const char *ident) {
    if (!ident || !ident[0]) return 0;
    for (int i = 0; i < g_ncats; i++) if (same(g_cats[i].ident, ident)) return &g_cats[i];
    return 0;
}
int cx_un_category_count(void) { return g_ncats; }
const cx_un_category_t *cx_un_category_at(int i) { return i >= 0 && i < g_ncats ? &g_cats[i] : 0; }
static void cat_free_one(cx_un_category_t *c) {
    for (int k = 0; k < c->nactions; k++) { free(c->actions[k].ident); free(c->actions[k].title); free(c->actions[k].button); free(c->actions[k].placeholder); }
    free(c->actions);
    free(c->ident);
    memset(c, 0, sizeof *c);
}
static void cat_copy_one(cx_un_category_t *c, const cx_un_category_t *src) {
    c->ident = sdup(src->ident ? src->ident : "");
    c->options = src->options;
    c->nactions = 0;
    c->actions = src->nactions > 0 ? calloc((size_t)src->nactions, sizeof *c->actions) : 0;
    for (int k = 0; c->actions && k < src->nactions; k++) {
        const cx_un_action_t *a = &src->actions[k];
        cx_un_action_t *d = &c->actions[c->nactions++];
        d->ident = sdup(a->ident ? a->ident : "");
        d->title = sdup(a->title ? a->title : "");
        d->options = a->options;
        d->text_input = a->text_input;
        d->button = sdup(a->button);
        d->placeholder = sdup(a->placeholder);
    }
}
static int internal_cat(const char *id) { return id && !strncmp(id, CX_UN_INTERNAL_CAT, sizeof CX_UN_INTERNAL_CAT - 1); }
/* uma categoria interna (NSUserNotification): troca a de mesmo nome; nactions < 0 apaga */
void cx_un_put_category(const cx_un_category_t *c) {
    if (!c || !c->ident) return;
    for (int i = 0; i < g_ncats; i++) {
        if (!same(g_cats[i].ident, c->ident)) continue;
        cat_free_one(&g_cats[i]);
        if (c->nactions < 0) { memmove(&g_cats[i], &g_cats[i + 1], sizeof *g_cats * (size_t)(g_ncats - i - 1)); g_ncats--; }
        else cat_copy_one(&g_cats[i], c);
        return;
    }
    if (c->nactions < 0) return;
    cx_un_category_t *na = realloc(g_cats, sizeof *na * (size_t)(g_ncats + 1));
    if (!na) return;
    g_cats = na;
    cat_copy_one(&g_cats[g_ncats++], c);
}
void cx_un_set_categories(const cx_un_category_t *cats, int n) {
    /* as categorias internas (CX_UN_INTERNAL_CAT) ficam */
    int keep = 0;
    for (int i = 0; i < g_ncats; i++) {
        if (internal_cat(g_cats[i].ident)) g_cats[keep++] = g_cats[i];
        else cat_free_one(&g_cats[i]);
    }
    g_ncats = keep;
    if (n <= 0 || !cats) return;
    for (int i = 0; i < n; i++) if (!internal_cat(cats[i].ident)) cx_un_put_category(&cats[i]);
}
void cx_un_set_badge(int count) {
    if (count < 0) count = 0;
    g_badge = count;
    if (g_sink.badge) g_sink.badge(count, g_sink.ctx);
    save();
}
int cx_un_badge(void) { return g_badge; }

/* ---------------------------------------------------------------- entrega */
static void deliver(int pi) {
    cx_un_req_t snap;
    req_copy(&snap, &g_pend[pi]);
    /* reprograma ou tira da lista de pendentes */
    cx_un_req_t *p = &g_pend[pi];
    double now = cx_un_now();
    if (p->repeats && p->trig == CX_UN_TRIG_CALENDAR) {
        double nx = cx_un_calendar_next(&p->cal, now > p->fire_at ? now : p->fire_at, 1);
        if (nx < 0) drop(g_pend, &g_npend, pi); else p->fire_at = nx;
    } else if (p->repeats && p->trig == CX_UN_TRIG_INTERVAL && p->interval > 0) {
        double nx = p->fire_at + p->interval;
        while (nx <= now) nx += p->interval;             /* venceu várias vezes com o app fechado: entrega uma */
        p->fire_at = nx;
    } else drop(g_pend, &g_npend, pi);
    int opts = g_present ? g_present(&snap, g_ctx) : CX_UN_PRESENT_ALL;
    snap.date = now;
    snap.fire_at = 0;
    if (opts & (CX_UN_PRESENT_LIST | CX_UN_PRESENT_BANNER | CX_UN_PRESENT_ALERT)) {
        int old = find_i(1, snap.ident, snap.source);
        if (old >= 0) {                                  /* mesmo identificador: substitui o entregue */
            if (g_deliv[old].token && g_sink.remove) g_sink.remove(g_deliv[old].token, g_sink.ctx);
            drop(g_deliv, &g_ndeliv, old);
        }
        while (g_ndeliv >= UN_MAX_DELIVERED) {
            if (g_deliv[0].token && g_sink.remove) g_sink.remove(g_deliv[0].token, g_sink.ctx);
            drop(g_deliv, &g_ndeliv, 0);
        }
        cx_un_req_t *d = push(&g_deliv, &g_ndeliv, &g_capdeliv);
        if (d) {
            *d = snap;
            memset(&snap, 0, sizeof snap);
            if (opts & (CX_UN_PRESENT_BANNER | CX_UN_PRESENT_ALERT)) {
                d->token = g_tok++;
                if (g_tok < 1) g_tok = 1;
                int sound = (opts & CX_UN_PRESENT_SOUND) && d->sound && d->sound[0];
                if (g_sink.banner) g_sink.banner(d, cx_un_category(d->category), sound, g_sink.ctx);
            }
            if ((opts & CX_UN_PRESENT_BADGE) && d->badge >= 0) {
                g_badge = d->badge;
                if (g_sink.badge) g_sink.badge(g_badge, g_sink.ctx);
            }
        }
    } else if ((opts & CX_UN_PRESENT_BADGE) && snap.badge >= 0) {
        g_badge = snap.badge;
        if (g_sink.badge) g_sink.badge(g_badge, g_sink.ctx);
    }
    req_free(&snap);
}
void cx_un_flush(void) {
    static int busy;
    if (busy) return;
    busy = 1;
    int changed = 0;
    for (int guard = 0; guard < 1000; guard++) {
        double now = cx_un_now();
        int due = -1;
        for (int i = 0; i < g_npend; i++) if (g_pend[i].fire_at <= now && (due < 0 || g_pend[i].fire_at < g_pend[due].fire_at)) due = i;
        if (due < 0) break;
        deliver(due);
        changed = 1;
    }
    busy = 0;
    if (changed) save();
}
double cx_un_next_fire(void) {
    double t = 0;
    for (int i = 0; i < g_npend; i++) if (t == 0 || g_pend[i].fire_at < t) t = g_pend[i].fire_at;
    return t;
}

int cx_un_add(const cx_un_req_t *spec) {
    if (!spec || !spec->ident) return CX_UN_ERR_BAD;
    if (g_auth != CX_UN_AUTHORIZED && g_auth != CX_UN_PROVISIONAL) return CX_UN_ERR_DENIED;
    double now = cx_un_now(), at;
    if (spec->trig == CX_UN_TRIG_PUSH) return CX_UN_ERR_BAD;
    if (spec->trig == CX_UN_TRIG_INTERVAL) {
        if (spec->interval <= 0 || (spec->repeats && spec->interval < 60)) return CX_UN_ERR_BAD;
        at = spec->fire_at > now ? spec->fire_at : now + spec->interval;   /* fire_at: primeira entrega pedida */
    } else if (spec->trig == CX_UN_TRIG_CALENDAR) {
        at = cx_un_calendar_next(&spec->cal, now, spec->repeats);
        if (at < 0) return CX_UN_ERR_BAD;
    } else at = now;
    int old = find_i(0, spec->ident, spec->source);
    if (old >= 0) drop(g_pend, &g_npend, old);
    if (g_npend >= UN_MAX_PENDING) return CX_UN_ERR_FULL;
    cx_un_req_t *r = push(&g_pend, &g_npend, &g_cappend);
    if (!r) return CX_UN_ERR_FULL;
    req_copy(r, spec);
    r->fire_at = at;
    r->date = 0;
    r->token = 0;
    if (spec->trig == CX_UN_TRIG_NONE && !g_noauto) cx_un_flush();
    else save();
    return CX_UN_OK;
}
int cx_un_remove_pending(const char *ident, int source) {
    int n = 0;
    for (int i = g_npend - 1; i >= 0; i--) {
        if (source >= 0 && g_pend[i].source != source) continue;
        if (ident && !same(g_pend[i].ident, ident)) continue;
        drop(g_pend, &g_npend, i);
        n++;
    }
    if (n) save();
    return n;
}
int cx_un_remove_delivered(const char *ident, int source) {
    int n = 0;
    for (int i = g_ndeliv - 1; i >= 0; i--) {
        if (source >= 0 && g_deliv[i].source != source) continue;
        if (ident && !same(g_deliv[i].ident, ident)) continue;
        if (g_deliv[i].token && g_sink.remove) g_sink.remove(g_deliv[i].token, g_sink.ctx);
        drop(g_deliv, &g_ndeliv, i);
        n++;
    }
    if (n) save();
    return n;
}

int cx_un_event(int token, int action, const char *text) {
    int i;
    if (token <= 0) return 0;
    for (i = 0; i < g_ndeliv; i++) if (g_deliv[i].token == token) break;
    if (i == g_ndeliv) return 0;
    cx_un_req_t snap;
    req_copy(&snap, &g_deliv[i]);
    const cx_un_category_t *cat = cx_un_category(snap.category);
    const char *act = 0;
    if (action == -1) act = CX_UN_ACTION_DEFAULT;
    else if (action == -2) act = cat && (cat->options & 1) ? CX_UN_ACTION_DISMISS : 0;   /* só com customDismissAction */
    else if (cat && action >= 0 && action < cat->nactions) act = cat->actions[action].ident;
    /* clique no corpo: o banner sai, a entrada fica na Central (o token continua valendo para ela);
     * botão, resposta ou fechar: sai da Central também */
    if (action == -2 || (cat && action >= 0 && action < cat->nactions)) { drop(g_deliv, &g_ndeliv, i); save(); }
    if (act && g_receive) {
        const char *t = 0;
        if (action >= 0 && cat && cat->actions[action].text_input) t = text ? text : "";
        g_receive(&snap, act, t, g_ctx);
    }
    req_free(&snap);
    return 1;
}

void cx_un_settings(cx_un_settings_t *out) {
    if (!out) return;
    int on = g_auth == CX_UN_AUTHORIZED || g_auth == CX_UN_PROVISIONAL ? 2 : g_auth == CX_UN_DENIED ? 1 : 0;
    out->authorization = g_auth;
    out->sound = on;
    out->badge = on;
    out->alert = on;
    out->notification_center = on;
    out->lock_screen = on;
    out->alert_style = g_auth == CX_UN_AUTHORIZED ? 1 : 0;
}
