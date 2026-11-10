#include "jit_share.h"

static jit_share_tab local;
static jit_share_tab *T;

static void lk(jit_share_tab *t) {
    while (__atomic_exchange_n(&t->lock, 1, __ATOMIC_ACQUIRE))
        while (t->lock) __builtin_ia32_pause();
}
static void ulk(jit_share_tab *t) { __atomic_store_n(&t->lock, 0, __ATOMIC_RELEASE); }

static jit_share_tab *tab(void) {
    if (!T) {
        local.magic = JIT_SHARE_MAGIC;
        T = &local;
    }
    return T;
}

void jit_share_bind(void *mem, uint32_t bytes) {
    jit_share_tab *t;
    if (!mem || bytes < sizeof(jit_share_tab)) return;
    t = (jit_share_tab *)mem;
    if (t->magic != JIT_SHARE_MAGIC) {
        __builtin_memset(t, 0, sizeof *t);
        t->magic = JIT_SHARE_MAGIC;
    }
    T = t;
}

uint64_t jit_share_hash(const void *p, uint32_t n) {
    const uint8_t *s = (const uint8_t *)p;
    uint64_t h = 1469598103934665603ull;
    uint32_t i;
    for (i = 0; i < n; i++) {
        h ^= s[i];
        h *= 1099511628211ull;
    }
    return h;
}

int jit_share_executable(void) { return 0; }

static int same(const jit_share_ent *e, uint64_t id, uint64_t off, uint64_t pc) {
    return e->file_id == id && e->file_off == off && e->pc == pc && e->host_len && e->guest_len;
}

int jit_share_peek(uint64_t file_id, uint64_t file_off, uint64_t pc,
                   uint32_t *guest_len, uint64_t *hash, void *host, uint32_t cap, uint32_t *host_len) {
    jit_share_tab *t = tab();
    uint32_t i;
    int hit = 0;
    if (!file_id || !host || !guest_len || !hash || !host_len) return 0;
    lk(t);
    for (i = 0; i < t->n && i < JIT_SHARE_N; i++) {
        jit_share_ent *e = &t->ent[i];
        if (!same(e, file_id, file_off, pc) || e->host_len > cap || e->host_len > JIT_SHARE_HOST) continue;
        __builtin_memcpy(host, e->host, e->host_len);
        *guest_len = e->guest_len;
        *hash = e->hash;
        *host_len = e->host_len;
        hit = 1;
        break;
    }
    ulk(t);
    return hit;
}

void jit_share_publish(uint64_t file_id, uint64_t file_off, uint64_t pc, uint64_t hash,
                       const void *host, uint32_t host_len, uint32_t guest_len) {
    jit_share_tab *t = tab();
    uint32_t i, slot;
    if (!file_id || !host || !host_len || host_len > JIT_SHARE_HOST || !guest_len || guest_len > 4096) return;
    lk(t);
    slot = t->n;
    for (i = 0; i < t->n && i < JIT_SHARE_N; i++)
        if (same(&t->ent[i], file_id, file_off, pc)) { slot = i; break; }
    if (slot >= JIT_SHARE_N) {
        slot = t->clock % JIT_SHARE_N;
        t->clock++;
    } else if (slot == t->n) {
        t->n++;
    }
    t->ent[slot].file_id = file_id;
    t->ent[slot].file_off = file_off;
    t->ent[slot].pc = pc;
    t->ent[slot].hash = hash;
    t->ent[slot].host_len = host_len;
    t->ent[slot].guest_len = guest_len;
    __builtin_memcpy(t->ent[slot].host, host, host_len);
    ulk(t);
}

void jit_share_invalidate(uint64_t lo, uint64_t hi) {
    jit_share_tab *t = tab();
    uint32_t i;
    if (hi < lo) hi = ~(uint64_t)0;
    lk(t);
    for (i = 0; i < t->n && i < JIT_SHARE_N; i++) {
        jit_share_ent *e = &t->ent[i];
        uint64_t eh;
        if (!e->guest_len) continue;
        eh = e->pc + e->guest_len;
        if (e->pc < hi && eh > lo) {
            e->host_len = 0;
            e->guest_len = 0;
            e->file_id = 0;
        }
    }
    ulk(t);
}

__attribute__((weak)) int a64_host_ro_file(uint64_t page, uint64_t *file_id, uint64_t *file_off) {
    (void)page; (void)file_id; (void)file_off;
    return 0;
}
