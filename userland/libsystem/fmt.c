/*
 * API C dos formatadores de fmt_core.inc. O overlay Swift (FormatStyles / IntegerFormatStyle)
 * repete as mesmas saídas para en, pt, de e fr; o teste de host compara este núcleo.
 * Os símbolos mangled que o WhatsApp importa estão em fi_swift.c e chamam este núcleo.
 */
#include "foundation.h"
#include "libc.h"
#include "fmt_core.inc"

int mos_fmt_integer(const char *locale, long long v, int grouping, char *out, size_t cap) {
    return mf_integer(locale, v, grouping, out, cap);
}
int mos_fmt_decimal(const char *locale, double v, int frac, int grouping, char *out, size_t cap) {
    return mf_decimal(locale, v, frac, grouping, out, cap);
}
int mos_fmt_date(const char *locale, int y, int m, int d, int style, char *out, size_t cap) {
    return mf_date(locale, y, m, d, style, out, cap);
}
int mos_fmt_time(const char *locale, int hour, int min, int sec, int style, char *out, size_t cap) {
    return mf_time(locale, hour, min, sec, style, out, cap);
}
int mos_fmt_list(const char *locale, const char **items, int n, char *out, size_t cap) {
    return mf_list(locale, items, n, out, cap);
}
int mos_fmt_convert(const char *from, const char *to, double v, double *out) {
    return mf_convert(from, to, v, out);
}
int mos_fmt_dec_add(const char *a, const char *b, char *out, size_t cap) {
    return mf_dec_add(a, b, out, cap);
}
int mos_fmt_markdown(const char *src, char *plain, size_t pcap, mf_span *sp, int scap, char *hrefs, size_t hcap) {
    return mf_markdown(src, plain, pcap, sp, scap, hrefs, hcap);
}
