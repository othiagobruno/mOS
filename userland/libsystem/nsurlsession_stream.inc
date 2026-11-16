/*
 * mOS — NSURLSessionStreamTask (incluído por nsurlsession.c): TCP/TLS sobre o socket do mnet_stream.c.
 * Um fio lê (fila de readDataOfMinLength:maxLength:timeout:completionHandler: e closeRead) e outro escreve (fila de
 * writeData:timeout:completionHandler:, closeWrite, startSecureConnection), cada um na ordem em que o app pediu, com os
 * blocos chamados na fila de delegate da sessão. captureStreams espera as duas filas e entrega NSInputStream e
 * NSOutputStream ao delegate (URLSession:streamTask:didBecomeInputStream:outputStream:). A tarefa termina quando os
 * dois lados fecham (fim da leitura e closeWrite), na captura, no cancelamento ou em erro.
 * Limite: stopSecureConnection não volta para texto claro (o motor TLS não desfaz a sessão); closeWrite num socket
 * com TLS só para de escrever (sem close_notify parcial).
 */
id cx_netstream_new_input(mnet_stream_t *m, void *ext, const void *ops);
id cx_netstream_new_output(mnet_stream_t *m);
int shutdown(int fd, int how);
unsigned long long dispatch_time(unsigned long long when, long long delta);

enum { SQ_READ, SQ_WRITE, SQ_CLOSE_READ, SQ_CLOSE_WRITE, SQ_TLS, SQ_CAPTURE };
typedef struct sq { int op; long minl, maxl; int tmo_ms; id data; void *blk; struct sq *next; } sq_t;
struct stx {
    char host[256]; int port;
    mnet_stream_t *r, *w;
    sq_t *rh, *rt, *wh, *wt;
    void *rsem, *wsem;
    int read_closed, write_closed, captured, finished, both_at_capture;
    id cap_in, cap_out;
};
static stx_t *stx_of(task_t *t) { if (!t->stx) t->stx = calloc(1, sizeof(stx_t)); return t->stx; }
static void sq_push(stx_t *x, int to_write, sq_t *e) {
    LK();
    if (to_write) { if (x->wt) x->wt->next = e; else x->wh = e; x->wt = e; }
    else { if (x->rt) x->rt->next = e; else x->rh = e; x->rt = e; }
    ULK();
    dispatch_semaphore_signal(to_write ? x->wsem : x->rsem);
}
static sq_t *sq_pop(stx_t *x, int from_write) {
    LK();
    sq_t *e = from_write ? x->wh : x->rh;
    if (e) {
        if (from_write) { x->wh = e->next; if (!x->wh) x->wt = 0; }
        else { x->rh = e->next; if (!x->rh) x->rt = 0; }
    }
    ULK();
    return e;
}
static void sq_free(sq_t *e) { if (!e) return; rel(e->data); if (e->blk) _Block_release(e->blk); free(e); }

/* erro do socket -> NSURLErrorDomain */
static int stx_code(mnet_stream_t *s, const char **msg) {
    long code = 0;
    int dom = mnet_stream_error(s, &code, msg);
    if (dom == MNET_SD_NETDB) return MNET_E_NO_HOST;
    if (dom == MNET_SD_SSL) return code == -9807 ? MNET_E_CERT_UNTRUSTED : MNET_E_TLS;
    if (dom == MNET_SD_SOCKS) return MNET_E_PROXY;
    if (dom == MNET_SD_POSIX) return code == 60 ? MNET_E_TIMEOUT : code == 61 ? MNET_E_CONNECT : MNET_E_LOST;
    return MNET_E_UNKNOWN;
}

/* ---- entregas na fila de delegate */
typedef struct { task_t *t; void *blk; id data, err; int eof, op; } sdone_t;
static void ev_sdone(void *p) {
    sdone_t *d = p;
    if (d->op == SQ_READ && d->blk) ((void (*)(void *, id, BOOL, id))BLK(d->blk)->invoke)(d->blk, d->data, (BOOL)d->eof, d->err);
    else if (d->blk) call_1(d->blk, d->err);
    if (d->blk) _Block_release(d->blk);
    rel(d->data); rel(d->err);
    rel((id)d->t);
    free(d);
}
static void post_done(task_t *t, sq_t *e, id data, int eof, id err) {
    sdone_t *d = calloc(1, sizeof *d);
    d->t = (task_t *)retain((id)t); d->op = e->op == SQ_READ ? SQ_READ : SQ_WRITE;
    d->blk = e->blk; e->blk = 0;
    d->data = retain(data); d->err = retain(err); d->eof = eof;
    on_queue(t->session, ev_sdone, d, 0);
}
static void ev_side_closed(void *p) {
    sdone_t *d = p;
    const char *sel = d->op == SQ_CLOSE_READ ? "URLSession:readClosedForStreamTask:" : "URLSession:writeClosedForStreamTask:";
    id dl = task_delegate_for(d->t, sel);
    if (dl) MSG(void, dl, sel, d->t->session, (id)d->t);
    rel((id)d->t);
    free(d);
}
static void post_side_closed(task_t *t, int op) {
    sdone_t *d = calloc(1, sizeof *d);
    d->t = (task_t *)retain((id)t); d->op = op;
    on_queue(t->session, ev_side_closed, d, 0);
}
static void ev_captured(void *p) {
    task_t *t = p;
    stx_t *x = t->stx;
    id dl = task_delegate_for(t, "URLSession:streamTask:didBecomeInputStream:outputStream:");
    if (dl) MSG(void, dl, "URLSession:streamTask:didBecomeInputStream:outputStream:", t->session, (id)t, x->cap_in, x->cap_out);
    rel(x->cap_in); rel(x->cap_out); x->cap_in = x->cap_out = 0;
    rel((id)t);
}
/* fim da tarefa: uma vez, quando os dois lados acabaram (o fio leitor segura a referência do task_start) */
static int stx_should_finish(task_t *t) {
    stx_t *x = t->stx;
    LK();
    int go = !x->finished && (t->cancel || t->error || x->captured || (x->read_closed && x->write_closed));
    if (go) x->finished = 1;
    ULK();
    return go;
}
static void stx_fail_queue(task_t *t, int from_write, id err) {
    for (sq_t *e; (e = sq_pop(t->stx, from_write));) {
        if (e->op == SQ_READ || e->op == SQ_WRITE) post_done(t, e, 0, 0, err);
        sq_free(e);
    }
}

/* ---- escrita */
static void *stx_writer(void *arg) {
    task_t *t = arg;
    stx_t *x = t->stx;
    void *pool = objc_autoreleasePoolPush();
    for (;;) {
        if (dispatch_semaphore_wait(x->wsem, dispatch_time(0, 200000000LL))) { if (t->cancel || x->finished) break; continue; }
        sq_t *e = sq_pop(x, 1);
        if (!e) continue;
        if (t->cancel || t->error) {
            if (e->op == SQ_WRITE) post_done(t, e, 0, 0, t->error ? t->error : ar(url_error(MNET_E_CANCELLED, 0, 0)));
            sq_free(e);
            continue;
        }
        if (e->op == SQ_WRITE) {
            const u8 *p = MSG0(const u8 *, e->data, "bytes");
            size_t n = (size_t)MSG0(u64, e->data, "length"), off = 0;
            id err = 0;
            if (x->write_closed) err = url_error(MNET_E_LOST, "o lado de escrita foi fechado", 0);
            while (!err && off < n) {
                long k = mnet_stream_write(x->w, p + off, n - off);
                if (k <= 0) { const char *m = 0; int c = stx_code(x->w, &m); err = url_error(t->cancel ? MNET_E_CANCELLED : c, m, 0); break; }
                off += (size_t)k;
                LK(); t->sent += k; ULK();
            }
            post_done(t, e, 0, 0, err ? ar(err) : 0);
        } else if (e->op == SQ_CLOSE_WRITE) {
            if (!x->write_closed) {
                x->write_closed = 1;
                if (!mnet_stream_tls_active(x->w)) shutdown(cx_mnet_conn_fd(mnet_stream_conn(x->w)), 1);   /* SHUT_WR */
                post_side_closed(t, SQ_CLOSE_WRITE);
            }
        } else if (e->op == SQ_TLS) mnet_stream_starttls(x->w);
        else if (e->op == SQ_CAPTURE) {
            LK(); x->both_at_capture++; ULK();                      /* a leitura espera este ponto para capturar */
            sq_free(e);
            break;
        }
        sq_free(e);
        if (x->write_closed && x->read_closed) break;
    }
    objc_autoreleasePoolPop(pool);
    rel((id)t);
    return 0;
}

/* ---- leitura (fio da tarefa) */
static void *stream_main(void *arg) {
    task_t *t = arg;
    stx_t *x = t->stx;
    void *pool = objc_autoreleasePoolPush();
    int err = 0;
    const char *msg = 0;
    if (mnet_stream_socket(cx_mnet_io(), x->host, x->port, &x->r, &x->w)) err = MNET_E_BAD_URL;
    else {
        mnet_stream_open(x->r);
        mnet_stream_open(x->w);
        while (!t->cancel && mnet_stream_status(x->r) == MNET_SS_OPENING) usleep(5000);
        if (mnet_stream_status(x->r) == MNET_SS_ERROR) err = stx_code(x->r, &msg);
    }
    if (t->cancel) err = MNET_E_CANCELLED;
    if (err) t->error = url_error(err, msg, 0);
    unsigned long th;
    retain((id)t);
    if (pthread_create(&th, 0, stx_writer, t)) rel((id)t); else pthread_detach(th);
    while (!t->error && !t->cancel) {
        if (dispatch_semaphore_wait(x->rsem, dispatch_time(0, 200000000LL))) {
            if (x->captured || (x->read_closed && x->write_closed)) break;
            continue;
        }
        sq_t *e = sq_pop(x, 0);
        if (!e) continue;
        if (e->op == SQ_READ) {
            id data = mutdata_new();
            u8 buf[16384];
            int eof = 0;
            id rerr = 0;
            long long deadline = e->tmo_ms > 0 ? (long long)(cx_now() * 1000) + e->tmo_ms : 0;
            while (!x->read_closed && (long)MSG0(u64, data, "length") < (e->minl > 0 ? e->minl : 1)) {
                long room = e->maxl - (long)MSG0(u64, data, "length");
                if (room <= 0) break;
                int left = deadline ? (int)(deadline - (long long)(cx_now() * 1000)) : 0;
                if (deadline && left <= 0) { rerr = url_error(MNET_E_TIMEOUT, 0, 0); break; }
                long k = mnet_stream_read_wait(x->r, buf, (size_t)(room < (long)sizeof buf ? room : (long)sizeof buf), deadline ? left : 0);
                if (k == -2) { rerr = url_error(MNET_E_TIMEOUT, 0, 0); break; }
                if (k == 0) { eof = 1; break; }
                if (k < 0) { const char *m = 0; int c = stx_code(x->r, &m); rerr = url_error(t->cancel ? MNET_E_CANCELLED : c, m, 0); break; }
                MSG(void, data, "appendBytes:length:", buf, (u64)k);
                LK(); t->recv += k; ULK();
            }
            if (!eof && !rerr && mnet_stream_status(x->r) == MNET_SS_AT_END) eof = 1;
            u64 got = MSG0(u64, data, "length");
            post_done(t, e, got || !rerr ? data : 0, eof, rerr ? ar(rerr) : 0);
            rel(data);
            if (eof && !x->read_closed) { x->read_closed = 1; post_side_closed(t, SQ_CLOSE_READ); }
        } else if (e->op == SQ_CLOSE_READ) {
            if (!x->read_closed) { x->read_closed = 1; post_side_closed(t, SQ_CLOSE_READ); }
        } else if (e->op == SQ_CAPTURE) {
            sq_free(e);
            while (!t->cancel) { LK(); int both = x->both_at_capture >= 1; ULK(); if (both) break; usleep(2000); }
            if (!t->cancel) {
                x->cap_in = cx_netstream_new_input(x->r, 0, 0);
                x->cap_out = cx_netstream_new_output(x->w);
                x->r = x->w = 0;                                       /* agora são dos streams do app */
                x->captured = 1;
                on_queue(t->session, ev_captured, retain((id)t), 0);
            }
            break;
        }
        sq_free(e);
        if (x->read_closed && x->write_closed) break;
    }
    /* cancelamento ou erro: tudo o que está na fila falha */
    id ferr = t->error ? t->error : t->cancel ? ar(url_error(MNET_E_CANCELLED, 0, 0)) : 0;
    if (ferr) {
        if (!t->error && t->cancel) t->error = retain(ferr);
        if (x->r) mnet_stream_close(x->r);
        if (x->w) mnet_stream_close(x->w);
        stx_fail_queue(t, 0, ferr);
        stx_fail_queue(t, 1, ferr);
    }
    stx_should_finish(t);
    LK(); x->finished = 1; ULK();
    t->state = ST_COMPLETED;
    on_queue(t->session, ev_complete, t, 0);
    objc_autoreleasePoolPop(pool);
    return 0;
}

/* ---- API */
static id stx_make(id sess, const char *host, long port) {
    char u[300];
    snprintf(u, sizeof u, "tcp://%s:%ld", host, port);
    id url = MSG(id, cls("NSURL"), "URLWithString:", ar(ns(u)));
    id t = mk_task(sess, T_STREAM, url ? req_for_url(url) : 0, 0);
    stx_t *x = stx_of(TK(t));
    snprintf(x->host, sizeof x->host, "%s", host);
    x->port = (int)port;
    x->rsem = dispatch_semaphore_create(0);
    x->wsem = dispatch_semaphore_create(0);
    return t;
}
static id s_stream_host(id s, SEL c, id host, long port) { (void)c; return stx_make(s, cs(host) ? cs(host) : "", port); }
static id s_stream_service(id s, SEL c, id svc) {
    (void)c;
    id h = svc && responds(svc, "hostName") ? MSG0(id, svc, "hostName") : 0;
    if (!h && svc && responds(svc, "resolveWithTimeout:")) {
        MSG(void, svc, "resolveWithTimeout:", 5.0);
        double end = cx_now() + 5;
        while (!(h = MSG0(id, svc, "hostName")) && cx_now() < end) usleep(20000);
    }
    long port = svc && responds(svc, "port") ? MSG0(long, svc, "port") : 0;
    return stx_make(s, cs(h) ? cs(h) : "", port);
}
static void st_read(id self, SEL c, u64 minl, u64 maxl, double tmo, void *blk) {
    (void)c;
    task_t *t = TK(self);
    sq_t *e = calloc(1, sizeof *e);
    e->op = SQ_READ; e->minl = (long)minl; e->maxl = (long)(maxl ? maxl : 1); e->tmo_ms = tmo > 0 ? (int)(tmo * 1000) : 0;
    e->blk = blk ? _Block_copy(blk) : 0;
    sq_push(stx_of(t), 0, e);
}
static void st_write(id self, SEL c, id data, double tmo, void *blk) {
    (void)c; (void)tmo;
    task_t *t = TK(self);
    sq_t *e = calloc(1, sizeof *e);
    e->op = SQ_WRITE; e->data = MSG0(id, data, "copy");
    e->blk = blk ? _Block_copy(blk) : 0;
    sq_push(stx_of(t), 1, e);
}
static void st_op(task_t *t, int op, int to_write) { sq_t *e = calloc(1, sizeof *e); e->op = op; sq_push(stx_of(t), to_write, e); }
static void st_close_read(id self, SEL c) { (void)c; st_op(TK(self), SQ_CLOSE_READ, 0); }
static void st_close_write(id self, SEL c) { (void)c; st_op(TK(self), SQ_CLOSE_WRITE, 1); }
static void st_start_tls(id self, SEL c) { (void)c; st_op(TK(self), SQ_TLS, 1); }
static void st_stop_tls(id self, SEL c) { (void)self; (void)c; }
static void st_capture(id self, SEL c) { (void)c; st_op(TK(self), SQ_CAPTURE, 1); st_op(TK(self), SQ_CAPTURE, 0); }
static void stx_cancel(task_t *t) {
    stx_t *x = t->stx;
    if (!x) return;
    if (x->r) mnet_stream_close(x->r);                                /* acorda leituras e escritas presas */
    if (x->w) mnet_stream_close(x->w);
    dispatch_semaphore_signal(x->rsem);
    dispatch_semaphore_signal(x->wsem);
}
static void stx_free(stx_t *x) {
    if (!x) return;
    for (sq_t *e = x->rh, *n; e; e = n) { n = e->next; sq_free(e); }
    for (sq_t *e = x->wh, *n; e; e = n) { n = e->next; sq_free(e); }
    mnet_stream_free(x->r);
    mnet_stream_free(x->w);
    if (x->rsem) dispatch_release(x->rsem);
    if (x->wsem) dispatch_release(x->wsem);
    rel(x->cap_in); rel(x->cap_out);
    free(x);
}
static void stx_register(void) {
    M(NSURLSession, "streamTaskWithHostName:port:", s_stream_host);
    M(NSURLSession, "streamTaskWithNetService:", s_stream_service);
    M(NSURLSessionStreamTask, "readDataOfMinLength:maxLength:timeout:completionHandler:", st_read);
    M(NSURLSessionStreamTask, "writeData:timeout:completionHandler:", st_write);
    M(NSURLSessionStreamTask, "closeRead", st_close_read);
    M(NSURLSessionStreamTask, "closeWrite", st_close_write);
    M(NSURLSessionStreamTask, "startSecureConnection", st_start_tls);
    M(NSURLSessionStreamTask, "stopSecureConnection", st_stop_tls);
    M(NSURLSessionStreamTask, "captureStreams", st_capture);
}
