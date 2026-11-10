/* Conta de CMTime do libSystem. Sem QEMU.
 *   clang -O2 -o /tmp/cmvt tools/cmvt_test.c userland/libsystem/cmtime.c
 */
#include <stdio.h>
typedef long long i64;
typedef int i32;
typedef unsigned u32;
typedef struct { i64 value; i32 timescale; u32 flags; i64 epoch; } cm_time_t;
cm_time_t CMTimeMake(i64 value, i32 timescale);
cm_time_t CMTimeMakeWithSeconds(double seconds, i32 timescale);
cm_time_t CMTimeSubtract(cm_time_t a, cm_time_t b);
int main(void) {
    cm_time_t a = CMTimeMakeWithSeconds(1.5, 600);
    cm_time_t b = CMTimeMake(300, 600);
    cm_time_t d = CMTimeSubtract(a, b);
    if (a.value != 900 || a.timescale != 600) { printf("make %lld\n", (long long)a.value); return 1; }
    if (d.value != 600 || d.timescale != 600) { printf("sub %lld/%d\n", (long long)d.value, d.timescale); return 2; }
    cm_time_t c = CMTimeSubtract(CMTimeMake(1, 2), CMTimeMake(1, 3));
    if (c.value != 1 || c.timescale != 6) { printf("cross %lld/%d\n", (long long)c.value, c.timescale); return 3; }
    printf("cmtime ok\n");
    return 0;
}
