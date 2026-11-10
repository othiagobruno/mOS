#pragma once
#include "screencap_core.inc"

typedef struct cx_cap_run cx_cap_run;
struct cx_cap_run {
    int run, done;
    int x, y, w, h;
    cx_cap_t cap;
    void (*on_frame)(void *ctx, id surf, id pb, id sb);
    void *ctx;
    unsigned long th;
};
int cx_cap_start(cx_cap_run *r);
void cx_cap_stop(cx_cap_run *r);
int cx_cap_wrap(const unsigned *px, int w, int h, long long pts_us, id *surf_out, id *pb_out, id *sb_out);
