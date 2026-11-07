/* mOS — relógio de tempo real CMOS */
#include "kernel.h"

int g_tz_offset_min = -180; /* fuso: Brasília (UTC-3); RTC é lido como UTC em VMs */
bool g_rtc_is_utc = true;

static u8 cmos(u8 r) { outb(0x70, r); return inb(0x71); }

static int days_in_month(int y, int m) {
    static const int d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2 && ((y % 4 == 0 && y % 100) || y % 400 == 0)) return 29;
    return d[m - 1];
}

void rtc_now(datetime_t *dt) {
    u8 s, mi, h, d, mo, y, b;
    do {
        while (cmos(0x0A) & 0x80) {}
        s = cmos(0); mi = cmos(2); h = cmos(4); d = cmos(7); mo = cmos(8); y = cmos(9);
    } while (s != cmos(0));
    b = cmos(0x0B);
    bool pm = h & 0x80;
    h &= 0x7F;
    if (!(b & 4)) {
#define BCD(x) (((x) & 15) + ((x) >> 4) * 10)
        s = BCD(s); mi = BCD(mi); h = BCD(h); d = BCD(d); mo = BCD(mo); y = BCD(y);
    }
    if (!(b & 2) && pm) h = (h + 12) % 24;
    int Y = 2000 + y, M = mo, D = d, H = h, MI = mi;
    if (g_rtc_is_utc) {
        int total = H * 60 + MI + g_tz_offset_min;
        while (total < 0) {
            total += 1440;
            if (--D < 1) { if (--M < 1) { M = 12; Y--; } D = days_in_month(Y, M); }
        }
        while (total >= 1440) {
            total -= 1440;
            if (++D > days_in_month(Y, M)) { D = 1; if (++M > 12) { M = 1; Y++; } }
        }
        H = total / 60; MI = total % 60;
    }
    dt->year = Y; dt->month = M; dt->day = D; dt->hour = H; dt->minute = MI; dt->second = s;
    /* Zeller: 0 = domingo */
    int yy = Y, mm = M;
    if (mm < 3) { mm += 12; yy--; }
    int k = yy % 100, j = yy / 100;
    int wd = (D + 13 * (mm + 1) / 5 + k + k / 4 + j / 4 + 5 * j) % 7; /* 0 = sábado */
    dt->weekday = (wd + 6) % 7;
}

/* hora em UTC (validação de certificados e OCSP): a hora local menos o fuso */
void rtc_now_utc(datetime_t *dt) {
    rtc_now(dt);
    int total = dt->hour * 60 + dt->minute - g_tz_offset_min;
    int Y = dt->year, M = dt->month, D = dt->day;
    while (total < 0) {
        total += 1440;
        if (--D < 1) { if (--M < 1) { M = 12; Y--; } D = days_in_month(Y, M); }
    }
    while (total >= 1440) {
        total -= 1440;
        if (++D > days_in_month(Y, M)) { D = 1; if (++M > 12) { M = 1; Y++; } }
    }
    dt->year = Y; dt->month = M; dt->day = D; dt->hour = total / 60; dt->minute = total % 60;
}
