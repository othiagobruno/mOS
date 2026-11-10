/* CMTime sem o runtime ObjC, para o host testar a mesma conta do libSystem. */
typedef long long i64;
typedef int i32;
typedef unsigned u32;
typedef struct { i64 value; i32 timescale; u32 flags; i64 epoch; } cm_time_t;
#define kCMTimeFlags_Valid 1u

cm_time_t CMTimeMake(i64 value, i32 timescale) {
    cm_time_t t = {value, timescale, kCMTimeFlags_Valid, 0};
    return t;
}
cm_time_t CMTimeMakeWithSeconds(double seconds, i32 timescale) {
    cm_time_t t = {0, 0, 0, 0};
    if (timescale <= 0) return t;
    double x = seconds * (double)timescale;
    if (x >= 0) x += 0.5; else x -= 0.5;
    t.value = (i64)x;
    t.timescale = timescale;
    t.flags = kCMTimeFlags_Valid;
    return t;
}
cm_time_t CMTimeSubtract(cm_time_t a, cm_time_t b) {
    cm_time_t z = {0, 0, 0, 0};
    if (!(a.flags & kCMTimeFlags_Valid) || !(b.flags & kCMTimeFlags_Valid) || a.timescale <= 0 || b.timescale <= 0) return z;
    if (a.timescale == b.timescale) {
        z.value = a.value - b.value;
        z.timescale = a.timescale;
        z.flags = kCMTimeFlags_Valid;
        return z;
    }
    z.value = a.value * (i64)b.timescale - b.value * (i64)a.timescale;
    z.timescale = a.timescale * b.timescale;
    z.flags = kCMTimeFlags_Valid;
    return z;
}
