/* mOS — relógio de tempo real CMOS (MC146818 do FCH AMD)
 *
 * Leitura coerente (espera o fim do ciclo de atualização e repete até duas leituras iguais), BCD/binário e
 * 12/24 h pelo registro B, século pelo índice que a FADT informa (CENTURY; 0x32 no PC do Thiago) e escrita
 * com SET (reg. B bit 7) para o ajuste da hora. O alarme (regs. 1/3/5, AIE) serve de despertador do S3/S5:
 * o FADT liga RTC_EN no PM1 e o firmware acorda a máquina.
 */
#include "kernel.h"

int g_tz_offset_min = -180; /* fuso: Brasília (UTC-3); RTC é lido como UTC em VMs */
bool g_rtc_is_utc = true;

static u8 cmos(u8 r) { outb(0x70, (u8)(r | 0x80)); u8 v = inb(0x71); outb(0x70, r & 0x7F); return v; }   /* NMI fica mascarada só durante o acesso */
static void cmos_w(u8 r, u8 v) { outb(0x70, (u8)(r | 0x80)); outb(0x71, v); outb(0x70, r & 0x7F); }

static int days_in_month(int y, int m) {
    static const int d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m == 2 && ((y % 4 == 0 && y % 100) || y % 400 == 0)) return 29;
    return d[m - 1];
}

#define BCD(x) (((x) & 15) + ((x) >> 4) * 10)
#define TOBCD(x) ((u8)((((x) / 10) << 4) | ((x) % 10)))

typedef struct { int Y, M, D, h, m, s; } raw_t;

static void read_raw(raw_t *r) {
    u8 s, mi, h, d, mo, y, c = 0, b;
    u8 cidx = acpi_rtc_century();
    for (int tries = 0; tries < 8; tries++) {
        for (int i = 0; i < 100000 && (cmos(0x0A) & 0x80); i++) {}
        s = cmos(0); mi = cmos(2); h = cmos(4); d = cmos(7); mo = cmos(8); y = cmos(9);
        if (cidx) c = cmos(cidx);
        if (s == cmos(0) && mi == cmos(2)) break;
    }
    b = cmos(0x0B);
    bool pm = h & 0x80;
    h &= 0x7F;
    if (!(b & 4)) { s = BCD(s); mi = BCD(mi); h = BCD(h); d = BCD(d); mo = BCD(mo); y = BCD(y); if (cidx) c = BCD(c); }
    if (!(b & 2)) { if (h == 12) h = 0; if (pm) h += 12; }
    int century = (cidx && c >= 19 && c <= 30) ? c : 20;
    r->Y = century * 100 + y; r->M = mo ? mo : 1; r->D = d ? d : 1; r->h = h; r->m = mi; r->s = s;
}

static void add_minutes(raw_t *r, int delta) {
    int total = r->h * 60 + r->m + delta;
    while (total < 0) {
        total += 1440;
        if (--r->D < 1) { if (--r->M < 1) { r->M = 12; r->Y--; } r->D = days_in_month(r->Y, r->M); }
    }
    while (total >= 1440) {
        total -= 1440;
        if (++r->D > days_in_month(r->Y, r->M)) { r->D = 1; if (++r->M > 12) { r->M = 1; r->Y++; } }
    }
    r->h = total / 60; r->m = total % 60;
}

static int weekday(int Y, int M, int D) {   /* Zeller: 0 = domingo */
    int yy = Y, mm = M;
    if (mm < 3) { mm += 12; yy--; }
    int k = yy % 100, j = yy / 100;
    int wd = (D + 13 * (mm + 1) / 5 + k + k / 4 + j / 4 + 5 * j) % 7; /* 0 = sábado */
    return (wd + 6) % 7;
}

static void fill(datetime_t *dt, const raw_t *r) {
    dt->year = r->Y; dt->month = r->M; dt->day = r->D; dt->hour = r->h; dt->minute = r->m; dt->second = r->s;
    dt->weekday = weekday(r->Y, r->M, r->D);
}

void rtc_now(datetime_t *dt) {
    raw_t r; read_raw(&r);
    if (g_rtc_is_utc) add_minutes(&r, g_tz_offset_min);
    fill(dt, &r);
}

/* hora em UTC (validação de certificados e OCSP) */
void rtc_now_utc(datetime_t *dt) {
    raw_t r; read_raw(&r);
    if (!g_rtc_is_utc) add_minutes(&r, -g_tz_offset_min);
    fill(dt, &r);
}

/* grava a hora (UTC) no CMOS — ajuste por NTP ou pelos Ajustes */
void rtc_set_utc(const datetime_t *dt) {
    raw_t r = { dt->year, dt->month, dt->day, dt->hour, dt->minute, dt->second };
    if (!g_rtc_is_utc) add_minutes(&r, g_tz_offset_min);
    u8 b = cmos(0x0B);
    bool bin = b & 4, h24 = b & 2;
    int h = r.h; bool pm = false;
    if (!h24) { pm = h >= 12; h %= 12; if (!h) h = 12; }
    u8 cidx = acpi_rtc_century();
    u64 f = irq_save();
    cmos_w(0x0B, b | 0x80);                                   /* SET: congela a atualização */
    cmos_w(0, bin ? (u8)r.s : TOBCD(r.s));
    cmos_w(2, bin ? (u8)r.m : TOBCD(r.m));
    cmos_w(4, (u8)((bin ? (u8)h : TOBCD(h)) | (pm ? 0x80 : 0)));
    cmos_w(6, (u8)(weekday(r.Y, r.M, r.D) + 1));
    cmos_w(7, bin ? (u8)r.D : TOBCD(r.D));
    cmos_w(8, bin ? (u8)r.M : TOBCD(r.M));
    cmos_w(9, bin ? (u8)(r.Y % 100) : TOBCD(r.Y % 100));
    if (cidx) cmos_w(cidx, bin ? (u8)(r.Y / 100) : TOBCD(r.Y / 100));
    cmos_w(0x0B, b & 0x7F);
    irq_restore(f);
}

/* alarme diário em UTC (h:m:s); o reg. D tem dia do mês no FCH, mas o despertar de S3 só precisa do horário.
 * ligar = false desliga o AIE. Devolve false se o RTC não aceitar. */
bool rtc_alarm_set_utc(int hour, int minute, int second, bool on) {
    u8 b = cmos(0x0B);
    u64 f = irq_save();
    if (!on) { cmos_w(0x0B, b & ~0x20); irq_restore(f); return true; }
    raw_t r = { 2000, 1, 1, hour, minute, second };
    if (!g_rtc_is_utc) add_minutes(&r, g_tz_offset_min);
    bool bin = b & 4, h24 = b & 2;
    int h = r.h; bool pm = false;
    if (!h24) { pm = h >= 12; h %= 12; if (!h) h = 12; }
    cmos_w(1, bin ? (u8)r.s : TOBCD(r.s));
    cmos_w(3, bin ? (u8)r.m : TOBCD(r.m));
    cmos_w(5, (u8)((bin ? (u8)h : TOBCD(h)) | (pm ? 0x80 : 0)));
    (void)cmos(0x0C);                                         /* limpa flags pendentes */
    cmos_w(0x0B, b | 0x20);                                   /* AIE */
    irq_restore(f);
    return (cmos(0x0B) & 0x20) != 0;
}
