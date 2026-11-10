/*
 * Page cache unificado do VFS, por disco físico (o pai da cadeia de partições).
 * Leitura: páginas de 4 KiB, readahead adaptativo que não atravessa o fim do
 * dispositivo que pediu a leitura (a partição vizinha não entra no writeback).
 * Escrita: writeback. O disco só muda em blk_flush, pcache_tick ou aio_fsync.
 */
#include "fs.h"
#include "syscall.h"

#define PG 4096u
#define HT 1024
#define MAX_PAGES 256
#define RA_MAX 32u

typedef struct page {
    blkdev_t *root;
    u64 off;
    u8 *data;
    u32 valid;
    bool dirty;
    u64 stamp;
    struct page *hnext, *dnext, *dprev;
} page_t;

typedef struct ra_st {
    blkdev_t *root;
    u64 last_end;
    u32 seq, win;
    struct ra_st *next;
} ra_st;

typedef struct job {
    int kind; /* 1 bloco, 2 POSIX dispositivo, 3 memória (syscall) */
    blk_aiocb_t *bcb;
    struct aiocb *pcb;
    u64 key;
    bool write, fsync, done;
    int err;
    i64 ret;
    u8 *mem, *bounce;
    u64 memlen, off, n;
    void *ubuf;
    blkdev_t *dev;
    struct job *next;
} job_t;

static page_t *ht[HT];
static page_t *dirty_head, *dirty_tail;
static int npages;
static u64 clockn;
static ra_st *ras;
static pcache_stats_t stt;
static job_t *jobs, *jobs_tail;

struct { int fd; blkdev_t *dev; } abind[32];

static u64 irq_lock(void) {
#ifndef MOS_HOST_TEST
    return irq_save();
#else
    return 0;
#endif
}
static void irq_unlock(u64 f) {
#ifndef MOS_HOST_TEST
    irq_restore(f);
#else
    (void)f;
#endif
}

static blkdev_t *root_of(blkdev_t *d, u64 off, u64 *abs) {
    u64 a = off;
    while (d->parent) { a += d->base; d = d->parent; }
    *abs = a;
    return d;
}

static u32 hash(blkdev_t *r, u64 off) {
    u64 x = (u64)(usize)r ^ (off / PG);
    x ^= x >> 17;
    return (u32)x & (HT - 1);
}

static page_t *find_page(blkdev_t *r, u64 off) {
    for (page_t *p = ht[hash(r, off)]; p; p = p->hnext)
        if (p->root == r && p->off == off) return p;
    return NULL;
}

static void dirty_add(page_t *p) {
    if (p->dirty) return;
    p->dirty = true;
    p->dnext = NULL;
    p->dprev = dirty_tail;
    if (dirty_tail) dirty_tail->dnext = p;
    else dirty_head = p;
    dirty_tail = p;
}

static void dirty_del(page_t *p) {
    if (!p->dirty) return;
    if (p->dprev) p->dprev->dnext = p->dnext;
    else dirty_head = p->dnext;
    if (p->dnext) p->dnext->dprev = p->dprev;
    else dirty_tail = p->dprev;
    p->dirty = false;
    p->dnext = p->dprev = NULL;
}

static bool write_page(page_t *p) {
    if (!p->dirty) return true;
    if (!p->root->write || !p->root->write(p->root, p->off, p->data, p->valid)) return false;
    stt.dev_writes++;
    stt.writebacks++;
    dirty_del(p);
    return true;
}

static void free_page(page_t *p) {
    u32 h = hash(p->root, p->off);
    page_t **pp = &ht[h];
    while (*pp && *pp != p) pp = &(*pp)->hnext;
    if (*pp) *pp = p->hnext;
    dirty_del(p);
    kfree(p->data);
    kfree(p);
    npages--;
}

static bool evict_one(void) {
    page_t *best = NULL;
    for (int i = 0; i < HT; i++)
        for (page_t *p = ht[i]; p; p = p->hnext)
            if (!p->dirty && (!best || p->stamp < best->stamp)) best = p;
    if (best) { free_page(best); return true; }
    if (!dirty_head) return false;
    if (!write_page(dirty_head)) return false;
    free_page(dirty_head);
    return true;
}

static page_t *new_page(blkdev_t *r, u64 off) {
    while (npages >= MAX_PAGES) { if (!evict_one()) return NULL; }
    page_t *p = kcalloc(1, sizeof *p);
    u8 *d = kmalloc(PG);
    if (!p || !d) { kfree(p); kfree(d); return NULL; }
    p->root = r;
    p->off = off;
    p->data = d;
    u32 h = hash(r, off);
    p->hnext = ht[h];
    ht[h] = p;
    npages++;
    return p;
}

static bool load_page(page_t *p) {
    u64 sz = p->root->size;
    if (p->off >= sz) return false;
    u32 n = (u32)MIN((u64)PG, sz - p->off);
    if (!p->root->read(p->root, p->off, p->data, n)) return false;
    if (n < PG) memset(p->data + n, 0, PG - n);
    p->valid = n;
    p->stamp = ++clockn;
    stt.dev_reads++;
    stt.dev_read_bytes += n;
    stt.misses++;
    return true;
}

/* carrega páginas ausentes contíguas com uma leitura só */
static bool load_run(blkdev_t *r, u64 off, int n) {
    page_t *ps[RA_MAX + 8];
    if (n > (int)(RA_MAX + 8)) return false;
    u64 sz = r->size;
    u32 bytes = 0;
    for (int i = 0; i < n; i++) {
        u64 o = off + (u64)i * PG;
        if (o >= sz) { n = i; break; }
        ps[i] = find_page(r, o);
        if (ps[i] && ps[i]->valid) { /* buraco no meio: para */
            n = i;
            break;
        }
        bytes += (u32)MIN((u64)PG, sz - o);
    }
    if (n <= 0) return true;
    if (n == 1) {
        if (!ps[0]) ps[0] = new_page(r, off);
        if (!ps[0]) return false;
        if (ps[0]->valid) return true;
        return load_page(ps[0]);
    }
    u8 *tmp = kmalloc(bytes);
    if (!tmp) {
        for (int i = 0; i < n; i++) {
            if (!ps[i]) ps[i] = new_page(r, off + (u64)i * PG);
            if (!ps[i] || (ps[i]->valid == 0 && !load_page(ps[i]))) return false;
        }
        return true;
    }
    bool ok = r->read(r, off, tmp, bytes);
    if (ok) { stt.dev_reads++; stt.dev_read_bytes += bytes; }
    u32 pos = 0;
    for (int i = 0; ok && i < n; i++) {
        u64 o = off + (u64)i * PG;
        u32 vn = (u32)MIN((u64)PG, sz - o);
        if (!ps[i]) ps[i] = new_page(r, o);
        if (!ps[i]) { ok = false; break; }
        memcpy(ps[i]->data, tmp + pos, vn);
        if (vn < PG) memset(ps[i]->data + vn, 0, PG - vn);
        ps[i]->valid = vn;
        ps[i]->stamp = ++clockn;
        stt.misses++;
        pos += vn;
    }
    kfree(tmp);
    return ok;
}

static ra_st *ra_of(blkdev_t *r) {
    for (ra_st *s = ras; s; s = s->next) if (s->root == r) return s;
    ra_st *s = kcalloc(1, sizeof *s);
    if (!s) return NULL;
    s->root = r;
    s->next = ras;
    ras = s;
    return s;
}

static bool range_ok(blkdev_t *d, u64 off, u64 len) {
    return d && d->read && off <= d->size && len <= d->size - off;
}

bool blk_read(blkdev_t *d, u64 off, void *buf, u64 len) {
    if (!range_ok(d, off, len)) return false;
    if (!len) return true;
    u64 abs;
    blkdev_t *root = root_of(d, off, &abs);
    u64 limit = abs - off + d->size; /* não passa do fim deste dispositivo */
    u64 fl = irq_lock();
    ra_st *rs = ra_of(root);
    u32 win = 0;
    if (rs) {
        if (abs == rs->last_end) {
            rs->seq++;
            if (rs->seq >= 2) rs->win = rs->win ? MIN(RA_MAX, rs->win * 2) : 1;
        } else {
            rs->seq = 1;
            rs->win = 0;
        }
        win = rs->win;
        rs->last_end = abs + len;
    }
    u64 end = abs + len;
    u64 ra_end = end + (u64)win * PG;
    if (ra_end > limit) ra_end = limit;
    if (win && ra_end > end) stt.ra_pages += (u32)((ra_end - end + PG - 1) / PG);
    u8 *out = buf;
    u64 cur = abs;
    bool ok = true;
    while (ok && cur < end) {
        u64 pg = cur & ~(u64)(PG - 1);
        int run = 0;
        u64 o = pg;
        u64 span_end = cur < end ? end : ra_end;
        if (span_end < ra_end && (cur & (PG - 1)) == 0 && end - cur >= PG) span_end = ra_end;
        while (o < ra_end && run < (int)RA_MAX) {
            page_t *p = find_page(root, o);
            if (p && p->valid) break;
            run++;
            o += PG;
        }
        if (run > 1 && pg + (u64)run * PG > span_end && cur != abs) {
            /* prefetch largo só no começo do pedido */
        }
        if (run > 0) ok = load_run(root, pg, run);
        page_t *p = ok ? find_page(root, pg) : NULL;
        if (!p || !p->valid) { ok = false; break; }
        p->stamp = ++clockn;
        u32 skip = (u32)(cur - pg);
        u32 n = (u32)MIN((u64)p->valid - skip, end - cur);
        memcpy(out, p->data + skip, n);
        stt.hits++;
        out += n;
        cur += n;
        if (run == 0) {
            /* página seguinte já estava quente: segue */
        }
    }
    irq_unlock(fl);
    return ok;
}

bool blk_write(blkdev_t *d, u64 off, const void *buf, u64 len) {
    if (!d || !blk_writable(d) || off > d->size || len > d->size - off) return false;
    if (!len) return true;
    u64 abs;
    blkdev_t *root = root_of(d, off, &abs);
    if (!root->write) return false;
    const u8 *in = buf;
    u64 fl = irq_lock();
    bool ok = true;
    while (ok && len) {
        u64 pg = abs & ~(u64)(PG - 1);
        page_t *p = find_page(root, pg);
        if (!p) p = new_page(root, pg);
        if (!p) { ok = false; break; }
        if (!p->valid && !load_page(p)) { ok = false; break; }
        u32 skip = (u32)(abs - pg);
        if (skip >= p->valid) { ok = false; break; }
        u32 n = (u32)MIN((u64)p->valid - skip, len);
        memcpy(p->data + skip, in, n);
        p->stamp = ++clockn;
        dirty_add(p);
        in += n; abs += n; len -= n;
    }
    irq_unlock(fl);
    return ok;
}

static bool writeback_range(blkdev_t *d) {
    u64 abs0, abs1;
    blkdev_t *root = root_of(d, 0, &abs0);
    abs1 = abs0 + d->size;
    bool ok = true;
    page_t *p = dirty_head;
    while (p) {
        page_t *n = p->dnext;
        /* a página que cruza a fronteira entra: o conteúdo do vizinho nela é o que o cache já tinha */
        if (p->root == root && p->off < abs1 && p->off + p->valid > abs0) ok = write_page(p) && ok;
        p = n;
    }
    return ok;
}

bool blk_flush(blkdev_t *d) {
    if (!d) return false;
    u64 fl = irq_lock();
    bool ok = writeback_range(d);
    blkdev_t *root = d;
    while (root->parent) root = root->parent;
    irq_unlock(fl);
    if (root->flush) ok = root->flush(root) && ok;
    return ok;
}

void pcache_tick(void) {
    u64 fl = irq_lock();
    page_t *p = dirty_head;
    int n = 0;
    while (p && n < 32) {
        page_t *nx = p->dnext;
        write_page(p);
        p = nx;
        n++;
    }
    irq_unlock(fl);
    aio_pump();
}

void pcache_drop(blkdev_t *d) {
    if (!d || d->parent) return;
    u64 fl = irq_lock();
    for (int i = 0; i < HT; i++) {
        page_t *p = ht[i];
        while (p) {
            page_t *n = p->hnext;
            if (p->root == d) { write_page(p); free_page(p); }
            p = n;
        }
    }
    ra_st **rs = &ras;
    while (*rs) {
        if ((*rs)->root == d) { ra_st *n = (*rs)->next; kfree(*rs); *rs = n; }
        else rs = &(*rs)->next;
    }
    irq_unlock(fl);
}

void pcache_stats(pcache_stats_t *o) { if (o) *o = stt; }
void pcache_stats_reset(void) { memset(&stt, 0, sizeof stt); }

static job_t *job_new(void) {
    job_t *j = kcalloc(1, sizeof *j);
    if (!j) return NULL;
    j->err = MOS_EINPROGRESS;
    if (jobs_tail) jobs_tail->next = j;
    else jobs = j;
    jobs_tail = j;
    return j;
}

static void job_unlink(job_t *j) {
    job_t **pp = &jobs;
    job_t *prev = NULL;
    while (*pp && *pp != j) { prev = *pp; pp = &(*pp)->next; }
    if (!*pp) return;
    *pp = j->next;
    if (jobs_tail == j) jobs_tail = prev;
    kfree(j);
}

int blk_aio_submit(blk_aiocb_t *cb) {
    if (!cb || !cb->dev || cb->link) return -1;
    u64 fl = irq_lock();
    job_t *j = job_new();
    if (!j) { irq_unlock(fl); return -1; }
    j->kind = 1;
    j->bcb = cb;
    j->write = cb->write;
    j->dev = cb->dev;
    j->off = cb->off;
    j->n = cb->len;
    cb->err = MOS_EINPROGRESS;
    cb->ret = 0;
    cb->link = j;
    irq_unlock(fl);
    return 0;
}

static job_t *job_by_blk(const blk_aiocb_t *cb) {
    for (job_t *j = jobs; j; j = j->next) if (j->bcb == cb) return j;
    return NULL;
}
static job_t *job_by_pos(const struct aiocb *cb) {
    for (job_t *j = jobs; j; j = j->next) if (j->pcb == cb) return j;
    return NULL;
}
static job_t *job_by_key(u64 key) {
    for (job_t *j = jobs; j; j = j->next) if (j->kind == 3 && j->key == key) return j;
    return NULL;
}

int blk_aio_error(const blk_aiocb_t *cb) {
    if (!cb) return -1;
    job_t *j = job_by_blk(cb);
    if (!j) return cb->err;
    return j->done ? j->err : MOS_EINPROGRESS;
}

i64 blk_aio_return(blk_aiocb_t *cb) {
    job_t *j = job_by_blk(cb);
    if (!j || !j->done) return -1;
    i64 r = j->ret;
    cb->err = j->err;
    cb->ret = r;
    cb->link = NULL;
    job_unlink(j);
    return r;
}

int aio_bind(int fd, blkdev_t *dev) {
    for (int i = 0; i < 32; i++) if (abind[i].fd == fd) { abind[i].dev = dev; return 0; }
    for (int i = 0; i < 32; i++) if (!abind[i].dev) { abind[i].fd = fd; abind[i].dev = dev; return 0; }
    return -1;
}
static blkdev_t *bound(int fd) {
    for (int i = 0; i < 32; i++) if (abind[i].dev && abind[i].fd == fd) return abind[i].dev;
    return NULL;
}

static int posix_submit(struct aiocb *cb, bool write, bool fsync) {
    if (!cb) return -1;
    if (job_by_pos(cb)) return -1;
    blkdev_t *d = bound(cb->aio_fildes);
    if (!d) return -1;
    u64 fl = irq_lock();
    job_t *j = job_new();
    if (!j) { irq_unlock(fl); return -1; }
    j->kind = 2;
    j->pcb = cb;
    j->write = write;
    j->fsync = fsync;
    j->dev = d;
    j->off = (u64)cb->aio_offset;
    j->n = cb->aio_nbytes;
    irq_unlock(fl);
    return 0;
}

int aio_read(struct aiocb *cb) { return posix_submit(cb, false, false); }
int aio_write(struct aiocb *cb) { return posix_submit(cb, true, false); }
int aio_fsync(int op, struct aiocb *cb) { (void)op; return posix_submit(cb, false, true); }

int aio_error(const struct aiocb *cb) {
    job_t *j = job_by_pos(cb);
    if (!j) return -1;
    return j->done ? j->err : MOS_EINPROGRESS;
}

i64 aio_return(struct aiocb *cb) {
    job_t *j = job_by_pos(cb);
    if (!j || !j->done) return -1;
    i64 r = j->ret;
    job_unlink(j);
    return r;
}

int aio_suspend(const struct aiocb *const list[], int nent, const void *timeout) {
    (void)timeout;
    for (int spin = 0; spin < 64; spin++) {
        int pend = 0;
        for (int i = 0; i < nent; i++) {
            job_t *j = list[i] ? job_by_pos(list[i]) : NULL;
            if (j && !j->done) pend++;
        }
        if (!pend) return 0;
        aio_pump();
    }
    return -1;
}

int aio_mem_submit(u64 key, int write, u8 *mem, u64 memlen, u64 off, u64 n, u8 *bounce, void *ubuf) {
    if (job_by_key(key)) return -1;
    u64 fl = irq_lock();
    job_t *j = job_new();
    if (!j) { irq_unlock(fl); return -1; }
    j->kind = 3;
    j->key = key;
    j->write = write != 0;
    j->mem = mem;
    j->memlen = memlen;
    j->off = off;
    j->n = n;
    j->bounce = bounce;
    j->ubuf = ubuf;
    irq_unlock(fl);
    return 0;
}

int aio_mem_poll(u64 key, i64 *ret) {
    job_t *j = job_by_key(key);
    if (!j) return -1;
    if (!j->done) return MOS_EINPROGRESS;
    if (ret) *ret = j->ret;
    return j->err;
}

void aio_mem_take(u64 key, u8 **bounce, u64 *n, void **ubuf, i64 *ret) {
    job_t *j = job_by_key(key);
    if (!j) return;
    if (bounce) *bounce = j->bounce;
    if (n) *n = j->n;
    if (ubuf) *ubuf = j->ubuf;
    if (ret) *ret = j->ret;
    j->bounce = NULL;
    job_unlink(j);
}

int aio_mem_fsync(u64 key) {
    u64 fl = irq_lock();
    job_t *j = job_new();
    if (!j) { irq_unlock(fl); return 0; }
    j->kind = 3;
    j->key = key;
    j->fsync = true;
    irq_unlock(fl);
    return 1;
}

void aio_pump(void) {
    job_t *j = jobs;
    while (j && j->done) j = j->next;
    if (!j) return;
    if (j->kind == 1) {
        bool ok = j->write ? blk_write(j->dev, j->off, j->bcb->buf, j->n)
                           : blk_read(j->dev, j->off, j->bcb->buf, j->n);
        j->ret = ok ? (i64)j->n : -1;
        j->err = ok ? 0 : MOS_EIO;
        j->bcb->err = j->err;
        j->bcb->ret = j->ret;
        j->done = true;
        return;
    }
    if (j->kind == 2) {
        bool ok = true;
        if (j->fsync) ok = blk_flush(j->dev);
        else if (j->write) ok = blk_write(j->dev, j->off, (void *)j->pcb->aio_buf, j->n);
        else ok = blk_read(j->dev, j->off, (void *)j->pcb->aio_buf, j->n);
        j->ret = ok ? (j->fsync ? 0 : (i64)j->n) : -1;
        j->err = ok ? 0 : MOS_EIO;
        j->done = true;
        return;
    }
    if (j->fsync) {
        bool ok = true;
        u64 fl = irq_lock();
        page_t *p = dirty_head;
        while (p) { page_t *n = p->dnext; ok = write_page(p) && ok; p = n; }
        irq_unlock(fl);
        j->ret = ok ? 0 : -1;
        j->err = ok ? 0 : MOS_EIO;
        j->done = true;
        return;
    }
    if (!j->mem || j->off > j->memlen || j->n > j->memlen - j->off || !j->bounce) {
        j->ret = -1;
        j->err = MOS_EFAULT;
        j->done = true;
        return;
    }
    if (j->write) memcpy(j->mem + j->off, j->bounce, j->n);
    else memcpy(j->bounce, j->mem + j->off, j->n);
    j->ret = (i64)j->n;
    j->err = 0;
    j->done = true;
}
