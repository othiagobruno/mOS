/* Campos de NSURLRequest que o NSURLSession aplica no transporte.
 * O prazo vira SO_RCVTIMEO/SO_SNDTIMEO, o mesmo mecanismo do resolvedor. */
#ifndef URLREQUEST_FIELDS_H
#define URLREQUEST_FIELDS_H

typedef struct {
    double timeout;
    unsigned long policy;
} urlreq_opts_t;

static inline void urlreq_init_opts(urlreq_opts_t *o) {
    o->timeout = 60;
    o->policy = 0; /* NSURLRequestUseProtocolCachePolicy */
}

static inline void urlreq_set_timeout(urlreq_opts_t *o, double t) {
    o->timeout = t > 0 ? t : 60;
}

static inline double urlreq_timeout(const urlreq_opts_t *o) {
    return o && o->timeout > 0 ? o->timeout : 60;
}

static inline void urlreq_set_policy(urlreq_opts_t *o, unsigned long p) {
    if (o) o->policy = p;
}

static inline unsigned long urlreq_policy(const urlreq_opts_t *o) {
    return o ? o->policy : 0;
}

/* segundos inteiros + micros, para setsockopt do pedido que a sessão envia */
static inline void urlreq_timeval(double timeout, long *sec, long *usec) {
    double t = timeout > 0 ? timeout : 60;
    long s = (long)t;
    long u = (long)((t - (double)s) * 1000000.0);
    if (s < 1 && u <= 0) { s = 1; u = 0; }
    if (u < 0) u = 0;
    if (u > 999999) u = 999999;
    *sec = s;
    *usec = u;
}

#endif
