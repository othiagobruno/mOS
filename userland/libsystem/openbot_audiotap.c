/*
 * AudioHardwareCreateProcessTap deste processo: o PCM que o HAL escreve em DEV_AUDIO_WRITE
 * (coreaudio.c) entra num anel, e o IOProc do dispositivo do tap lê esse anel.
 * Áudio de outro processo não passa por aqui: não há syscall de mix. Esse caso fica sem amostras.
 */
#include "foundation.h"
#include "sys.h"

void *calloc(size_t n, size_t s);
void free(void *p);
void *memset(void *d, int c, size_t n);
void *memcpy(void *d, const void *s, size_t n);
int usleep(unsigned us);
int pthread_create(unsigned long *t, const void *attr, void *(*fn)(void *), void *arg);
void *_Block_copy(const void *b);
void _Block_release(const void *b);

#define TAP_ID 0x74500001u
#define TAP_N 48000u
static short tap_ring[TAP_N * 2];
static u32 tap_w, tap_r;
static int tap_live;
static volatile int tap_lock;
static void tlock(void) { while (__sync_lock_test_and_set(&tap_lock, 1)) SYS0(SYS_YIELD); }
static void tunlock(void) { __sync_lock_release(&tap_lock); }

void cx_tap_push(const void *pcm, u32 frames) {
    if (!tap_live || !pcm || !frames) return;
    const short *s = pcm;
    tlock();
    for (u32 i = 0; i < frames; i++) {
        tap_ring[(tap_w % TAP_N) * 2] = s[i * 2];
        tap_ring[(tap_w % TAP_N) * 2 + 1] = s[i * 2 + 1];
        tap_w++;
        if (tap_w - tap_r > TAP_N) tap_r = tap_w - TAP_N;
    }
    tunlock();
}

int cx_tap_owns(u32 d) { return d == TAP_ID && tap_live; }

typedef int (*tap_proc_t)(u32 dev, const void *now, const void *in, const void *inTime, void *out, const void *outTime, void *ctx);
typedef struct { tap_proc_t proc; void *block; void *ctx; int run; unsigned long th; } tap_io_t;
static tap_io_t tap_io;

static void *tap_thread(void *arg) {
    tap_io_t *io = arg;
    float buf[960 * 2];
    unsigned char ts[128];
    struct { u32 n; u32 ch; u32 bytes; void *data; } abl;
    while (io->run) {
        u32 got = 0;
        tlock();
        u32 avail = tap_w - tap_r;
        if (avail > 960) avail = 960;
        for (u32 i = 0; i < avail; i++) {
            short l = tap_ring[(tap_r % TAP_N) * 2];
            short r = tap_ring[(tap_r % TAP_N) * 2 + 1];
            tap_r++;
            buf[i * 2] = (float)l / 32768.0f;
            buf[i * 2 + 1] = (float)r / 32768.0f;
        }
        got = avail;
        tunlock();
        if (!got) { usleep(5000); continue; }
        memset(ts, 0, sizeof ts);
        memset(&abl, 0, sizeof abl);
        abl.n = 1;
        abl.ch = 2;
        abl.bytes = got * 8;
        abl.data = buf;
        if (io->proc) io->proc(TAP_ID, ts, &abl, ts, 0, ts, io->ctx);
        else if (io->block) {
            void *inv = *(void **)((unsigned char *)io->block + 16);
            if (inv) ((void (*)(void *, const void *, const void *, const void *, void *, const void *))inv)(io->block, ts, &abl, ts, 0, ts);
        }
    }
    return 0;
}

int cx_tap_add_ioproc(void *proc, void *block, void *ctx, void **out) {
    if (!out || (!proc && !block) || !tap_live) return -50;
    if (tap_io.run) return 0x6E6F7065; /* 'nope' */
    tap_io.proc = proc;
    tap_io.block = block ? _Block_copy(block) : 0;
    tap_io.ctx = ctx;
    tap_io.run = 1;
    if (pthread_create(&tap_io.th, 0, tap_thread, &tap_io) != 0) { tap_io.run = 0; return -1; }
    *out = (void *)1;
    return 0;
}
int cx_tap_remove_ioproc(void *pid) {
    (void)pid;
    if (!tap_io.run && !tap_io.proc && !tap_io.block) return 0x6E6F7065;
    tap_io.run = 0;
    usleep(8000);
    if (tap_io.block) _Block_release(tap_io.block);
    tap_io.proc = 0;
    tap_io.block = 0;
    return 0;
}

int AudioHardwareCreateProcessTap(id desc, u32 *out) {
    (void)desc;
    if (!out) return -50;
    tap_live = 1;
    *out = TAP_ID;
    return 0;
}
int AudioHardwareDestroyProcessTap(u32 d) {
    if (d != TAP_ID || !tap_live) return 0x216F626A; /* '!obj' */
    cx_tap_remove_ioproc(0);
    tap_live = 0;
    tap_w = tap_r = 0;
    return 0;
}
