#define _POSIX_C_SOURCE 200809L
/* Teste de host do núcleo de CF (cf_public_core.inc): calendário, fuso, UUID, saco,
 * NFC, URL, XML, número, anel de stream e versão de bundle. Não sobe a VM.
 *   gcc -std=c11 -Wall -Wextra -O1 -DCF_HOST tools/cftest.c -o /tmp/cftest && /tmp/cftest
 */
#define CF_HOST 1
#include "../userland/libsystem/cf_public_core.inc"
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
static int checks, fails;
static unsigned char sock_got[8];
static int sock_gn;
static void sock_cb(void *info, int kind, const void *data, int n) {
    (void)info; (void)kind;
    if (data && n > 0 && n < 8) { memcpy(sock_got, data, (size_t)n); sock_gn = n; }
}
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)
int main(void) {
    int y, mo, d, h, mi, s, wd;
    double at = cf_abs_from_ymdhms(2024, 3, 5, 0, 0, 0);
    unsigned char ub[16], ub2[16];
    char uu[37], buf[80], buf2[80];
    cf_bag bag; cf_url_parts url; cf_ring ring;
    const char *p = "ptr";
    CHECK(at == 731289600.0);
    cf_ymdhms_from_abs(at, &y, &mo, &d, &h, &mi, &s, &wd);
    CHECK(y == 2024 && mo == 3 && d == 5 && h == 0 && wd == 3);
    CHECK(cf_tz_offset("UTC", at) == 0);
    CHECK(cf_tz_offset("America/Sao_Paulo", at) == -3 * 3600);
    CHECK(cf_tz_offset("America/New_York", cf_abs_from_ymdhms(2024, 1, 15, 18, 0, 0)) == -5 * 3600);
    CHECK(cf_tz_offset("America/New_York", cf_abs_from_ymdhms(2024, 7, 15, 18, 0, 0)) == -4 * 3600);
    CHECK(cf_tz_offset("Europe/Paris", cf_abs_from_ymdhms(2024, 1, 15, 12, 0, 0)) == 3600);
    CHECK(cf_tz_offset("Europe/Paris", cf_abs_from_ymdhms(2024, 7, 15, 12, 0, 0)) == 7200);
    CHECK(cf_tz_offset("GMT-03:30", at) == -(3 * 3600 + 30 * 60));
    memset(ub, 0, 16); ub[0] = 0xab; ub[15] = 0xcd;
    cf_uuid_format(ub, uu);
    CHECK(cf_uuid_parse(uu, ub2) && ub2[0] == 0xab && ub2[15] == 0xcd);
    CHECK(!cf_uuid_parse("nope", ub2));
    memset(&bag, 0, sizeof bag);
    cf_bag_add(&bag, p); cf_bag_add(&bag, p); cf_bag_add(&bag, "q");
    CHECK(cf_bag_count(&bag, 0) == 3 && cf_bag_count(&bag, p) == 2);
    cf_bag_remove(&bag, p);
    CHECK(cf_bag_count(&bag, p) == 1);
    free(bag.s);
    cf_norm_utf8("\xC3\xA1", 0, buf, 80);
    CHECK(!strcmp(buf, "a\xCC\x81"));
    cf_norm_utf8(buf, 2, buf2, 80);
    CHECK(!strcmp(buf2, "\xC3\xA1"));
    cf_url_parse("https://mos.example:8443/a/b?x=1", &url);
    CHECK(!strcmp(url.scheme, "https") && !strcmp(url.host, "mos.example") && url.port == 8443);
    CHECK(!strcmp(url.path, "/a/b") && !strcmp(url.query, "x=1"));
    cf_xml_escape("a<b>&\"", buf, 80);
    CHECK(!strcmp(buf, "a&lt;b&gt;&amp;&quot;"));
    cf_xml_unescape(buf, buf2, 80);
    CHECK(!strcmp(buf2, "a<b>&\""));
    cf_fmt_grouped(1234567, -1, 1234567, '.', ',', buf, 80);
    CHECK(!strcmp(buf, "1,234,567"));
    cf_fmt_grouped(1234, 2, 1234.5, ',', '.', buf, 80);
    CHECK(!strcmp(buf, "1.234,50"));
    memset(&ring, 0, sizeof ring);
    CHECK(cf_ring_write(&ring, (const cf_u8 *)"abc", 3) == 3);
    CHECK(cf_ring_read(&ring, (cf_u8 *)buf, 2) == 2 && buf[0] == 'a' && buf[1] == 'b');
    ring.closed_w = 1;
    CHECK(cf_ring_read(&ring, (cf_u8 *)buf, 8) == 1 && buf[0] == 'c');
    CHECK(cf_version_bcd("2.3.4") == ((2u << 24) | (3u << 20) | (4u << 16)));
    char dec, grp; cf_locale_seps("en_US", &dec, &grp); CHECK(dec == '.' && grp == ',');
    cf_locale_seps("pt_BR", &dec, &grp); CHECK(dec == ',' && grp == '.');
    cf_locale_seps("fr_FR", &dec, &grp); CHECK(dec == ',' && grp == ' ');
    CHECK(!strcmp(cf_tz_abbr("America/Sao_Paulo", at), "BRT"));
    CHECK(cf_b64((const cf_u8 *)"Man", 3, buf, 80) == 4 && !strcmp(buf, "TWFu"));
    CHECK(cf_tz_next_dst("America/Sao_Paulo", at) == 0);
    CHECK(cf_tz_next_dst("America/New_York", cf_abs_from_ymdhms(2024, 1, 15, 12, 0, 0)) > 0);
    {
        cf_bpw *w = cf_bpw_new();
        unsigned char *raw = 0; int rn = 0;
        int ks, vs, top;
        cf_bpr rd;
        int kind = 0, len = 0; unsigned long long iv = 0; const unsigned char *bytes = 0;
        int arr[2];
        ks = cf_bpw_str(w, "nome");
        vs = cf_bpw_str(w, "mOS");
        arr[0] = cf_bpw_bool(w, 1);
        arr[1] = cf_bpw_int(w, 42);
        CHECK(cf_bpw_data(w, "xy", 2) >= 0);
        CHECK(cf_bpw_arr(w, arr, 2) >= 0);
        top = cf_bpw_dict(w, &ks, &vs, 1);
        CHECK(cf_bpw_finish(w, top, &raw, &rn) == 0);
        CHECK(cf_bpr_open(&rd, raw, rn) == 0 && rd.top == top);
        CHECK(cf_bpr_obj(&rd, vs, &kind, &iv, &bytes, &len) && kind == 5 && len == 3 && !memcmp(bytes, "mOS", 3));
        free(raw); cf_bpw_free(w);
    }
    {
        int sv[2];
        CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, sv) == 0);
        CHECK(write(sv[0], "hi", 2) == 2);
        CHECK(cf_sock_service(sv[1], 3, sock_cb, 0) == 1 && sock_gn == 2 && sock_got[0] == 'h' && sock_got[1] == 'i');
        close(sv[0]); close(sv[1]);
    }
    if (fails) { fprintf(stderr, "%d falhas em %d\n", fails, checks); return 1; }
    printf("%d ok\n", checks);
    return 0;
}
