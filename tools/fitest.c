#define _POSIX_C_SOURCE 200809L
/* gcc -std=c11 -Wall -Wextra -O1 -DFI_HOST -mno-red-zone -rdynamic \
      userland/libsystem/fi_swift.c userland/libsystem/fi_thunks.c tools/fitest.c -ldl -o /tmp/fitest */
#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <dlfcn.h>

typedef struct { unsigned long long w0, w1; } fi_swstr;
fi_swstr fi_date_default(unsigned long long bits);
fi_swstr fi_date_styles(unsigned long long bits, unsigned long long time_style, unsigned long long date_style);
fi_swstr fi_iso_format(unsigned long long bits);
unsigned long long fi_iso_parse_buf(const char *s);
void fi_dec_from_int(long long v, void *out);
void fi_dec_from_double(unsigned long long bits, void *out);
void fi_dec_binop(const void *a, const void *b, void *out, int op);
int fi_dec_cmp(const void *a, const void *b);
int fi_dec_format(const void *a, char *out, size_t cap);
int fi_convert_units(const char *from, const char *to, double v, double *out);

static int checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fprintf(stderr, "FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

static void eq(const char *got, const char *want) {
    checks++;
    if (strcmp(got, want)) { fprintf(stderr, "FAIL got \"%s\" want \"%s\"\n", got, want); fails++; }
}

static unsigned long long dbits(double v) {
    unsigned long long b;
    memcpy(&b, &v, sizeof b);
    return b;
}

typedef struct { const char *name; int state; const char *why; } fi_cat_row;
extern const fi_cat_row fi_catalog[];
extern const int fi_catalog_n;
extern void fi_witness_zero(void);
size_t fi_md_size(const void *md);
int fi_md_cow(const void *md);
int fi_desc_kind(const void *desc);
void *fi_vw_copy(void *dest, const void *src, const void *md);
void fi_vw_destroy(void *obj, const void *md);

typedef struct fi_obj fi_obj;
typedef struct fi_attr fi_attr;
typedef struct fi_person fi_person;
fi_obj *fi_style_date_default(void);
fi_obj *fi_style_iso_default(void);
fi_obj *fi_style_field(fi_obj *self, int field, unsigned long long arg);
fi_obj *fi_style_iso_part(fi_obj *self, int bit);
fi_obj *fi_style_iso_sep(fi_obj *self, int which, unsigned long long val);
fi_obj *fi_style_int_locale(const void *locale);
fi_obj *fi_style_percent_locale(const void *locale);
fi_obj *fi_style_precision(fi_obj *self, unsigned long long frac);
fi_obj *fi_style_notation(fi_obj *self, unsigned long long notation);
fi_obj *fi_style_sign(fi_obj *self, unsigned long long strategy);
fi_obj *fi_style_currency(unsigned long long w0, unsigned long long w1, const void *locale);
fi_obj *fi_style_list(unsigned long long type, unsigned long long width);
fi_obj *fi_style_locale(fi_obj *self, const void *locale);
fi_swstr fi_date_format_obj(unsigned long long bits, fi_obj *st);
fi_swstr fi_iso_format_styled(unsigned long long bits, fi_obj *st);
fi_swstr fi_int_format_obj(long long v, fi_obj *st);
fi_swstr fi_float_format_obj(unsigned long long bits, fi_obj *st);
fi_swstr fi_list_format(const char *locale, const char **items, int n, int type, int width);
fi_swstr fi_range_format_obj(const void *range, fi_obj *st);
int fi_str_cmp_cstr(const char *a, const char *b, int mode);
typedef struct { int kind; long long i; double d; unsigned long long w0, w1; const char *cstr; } fi_arg;
fi_swstr fi_str_format_args(const char *fmt, const fi_arg *args, int argc);
fi_person *fi_person_new(void);
void fi_person_set(fi_person **slot, int field, unsigned long long w0, unsigned long long w1);
fi_swstr fi_person_get(fi_person *p, int field);
fi_swstr fi_person_formatted(fi_person *p);
fi_attr *fi_attr_from_cstr(const char *s, int n);
fi_attr *fi_attr_concat(fi_attr *a, fi_attr *b);
const char *fi_attr_plain(const fi_attr *a);
int fi_attr_nruns(const fi_attr *a);
int fi_attr_run_kind(const fi_attr *a, int i);
fi_attr *fi_attr_markdown(const char *src, const void *opt);
fi_swstr fi_attr_member(fi_attr *a, const void *key);
extern const char fi_link_key[];
void fi_dec_from_uint(unsigned long long v, void *out);
void fi_dec_pow(const void *base, long long exp, void *out);
void fi_dec_negate(void *d);
long long fi_dec_exponent(const void *d);
int fi_dec_isnan(const void *d);
void fi_dec_significand(const void *d, void *out);
int fi_dec_from_locale(const char *s, const char *locale, void *out, int *is_nil);
fi_swstr fi_meas_format(const void *meas);
void fi_plist_set_format(void *enc, long fmt);
long fi_plist_get_format(void *enc);
void fi_release(void *p);

static int small_eq(fi_swstr s, const char *want);

static int ends(const char *s, const char *suf) {
    size_t n = strlen(s), m = strlen(suf);
    return n >= m && memcmp(s + n - m, suf, m) == 0;
}

static unsigned long long call_rax(void *fn) {
    unsigned char scratch[64];
    unsigned long long rax = (unsigned long long)scratch;
    memset(scratch, 0, sizeof scratch);
    __asm__ volatile(
        "movq %1, %%r11\n\t"
        "xor %%rdi, %%rdi\n\t"
        "xor %%rsi, %%rsi\n\t"
        "xor %%rdx, %%rdx\n\t"
        "xor %%rcx, %%rcx\n\t"
        "xor %%r8, %%r8\n\t"
        "xor %%r9, %%r9\n\t"
        "xor %%r13, %%r13\n\t"
        "xor %%r14, %%r14\n\t"
        "pxor %%xmm0, %%xmm0\n\t"
        "call *%%r11\n\t"
        : "+a"(rax)
        : "r"(fn)
        : "rdi", "rsi", "rdx", "rcx", "r8", "r9", "r10", "r11", "r13", "r14", "xmm0", "memory", "cc");
    return rax;
}

static int cow_name(const char *s) {
    return ends(s, "16AttributedStringVMa") || ends(s, "18AttributeContainerVMa")
        || ends(s, "19AttributedSubstringVMa") || ends(s, "13CharacterViewVMa")
        || ends(s, "4RunsVMa") || ends(s, "3RunVMa")
        || ends(s, "4DateV11FormatStyleVMa") || ends(s, "18ISO8601FormatStyleVMa")
        || ends(s, "19IntervalFormatStyleVMa") || ends(s, "11FormatStyleVMa")
        || ends(s, "8CurrencyVMa") || ends(s, "20PersonNameComponentsVMa")
        || ends(s, "22MarkdownParsingOptionsVMa");
}

static void pack_small(const char *s, unsigned long long *w0, unsigned long long *w1) {
    unsigned char b[16];
    int n = (int)strlen(s);
    memset(b, 0, sizeof b);
    if (n > 15) n = 15;
    memcpy(b, s, (size_t)n);
    b[15] = (unsigned char)(0xE0 | n);
    memcpy(w0, b, 8);
    memcpy(w1, b + 8, 8);
}

static void extra(void) {
    int i, real = 0, stub = 0;
    const char *trio[] = {"Ana", "Bia", "Caio"};
    unsigned long long bits = dbits(731289600.0);
    fi_obj *st, *iso, *num;
    char txt[80];
    CHECK(fi_catalog_n == 187);
    for (i = 0; i < fi_catalog_n; i++) {
        void *p = dlsym(RTLD_DEFAULT, fi_catalog[i].name);
        checks++;
        if (!p) { fprintf(stderr, "FAIL ausente %s\n", fi_catalog[i].name); fails++; continue; }
        if (fi_catalog[i].state == 0) real++;
        else stub++;
        if (ends(fi_catalog[i].name, "Ma")) {
            const void *md = (const void *)call_rax(p);
            size_t sz = fi_md_size(md);
            CHECK(md != 0);
            if (ends(fi_catalog[i].name, "11MeasurementVMa")) CHECK(sz == 16);
            else CHECK(sz == 8);
            CHECK(fi_md_cow(md) == cow_name(fi_catalog[i].name));
        } else if (ends(fi_catalog[i].name, "Mn") || ends(fi_catalog[i].name, "ON")) {
            int k = fi_desc_kind(p);
            int want = (ends(fi_catalog[i].name, "OMn") || ends(fi_catalog[i].name, "ON")) ? 18 : 17;
            CHECK(k == want);
        } else if (ends(fi_catalog[i].name, "Mc")) {
            unsigned char raw[24];
            void *w = 0;
            memcpy(raw, p, sizeof raw);
            memcpy(&w, raw + 16, sizeof w);
            CHECK(w == (void *)fi_witness_zero);
        } else {
            (void)call_rax(p);
        }
    }
    CHECK(real == 163);
    CHECK(stub == 24);

    st = fi_style_date_default();
    CHECK(small_eq(fi_date_format_obj(bits, st), "3/5/2024, 12:00 AM"));
    st = fi_style_field(st, 0, 1);
    st = fi_style_field(st, 1, 2);
    st = fi_style_field(st, 2, 1);
    CHECK(small_eq(fi_date_format_obj(bits, st), "Mar 5, 2024"));
    {
        fi_obj *pt = fi_style_locale(st, "pt_BR");
        CHECK(small_eq(fi_date_format_obj(bits, pt), "5 de mar. de 2024"));
        fi_release(pt);
    }
    st = fi_style_field(st, 1, 3);
    st = fi_style_field(st, 5, 2);
    CHECK(small_eq(fi_date_format_obj(bits, st), "Tuesday, March 5, 2024"));

    iso = fi_style_iso_default();
    CHECK(small_eq(fi_iso_format_styled(bits, iso), "2024-03-05T00:00:00Z"));
    CHECK(small_eq(fi_iso_format_styled(bits, 0), "2024-03-05T00:00:00Z"));
    {
        fi_obj *d = fi_style_iso_part(fi_style_iso_part(fi_style_iso_part(iso, 0), 1), 2);
        CHECK(small_eq(fi_iso_format_styled(bits, d), "2024-03-05"));
        d = fi_style_iso_sep(d, 0, 1);
        CHECK(small_eq(fi_iso_format_styled(bits, d), "20240305"));
        fi_release(d);
    }
    iso = fi_style_iso_sep(iso, 2, 1);
    CHECK(small_eq(fi_iso_format_styled(bits, iso), "2024-03-05 00:00:00Z"));

    num = fi_style_int_locale("en");
    CHECK(small_eq(fi_int_format_obj(1234567, num), "1,234,567"));
    fi_release(num);
    num = fi_style_int_locale("pt_BR");
    CHECK(small_eq(fi_int_format_obj(1234567, num), "1.234.567"));
    num = fi_style_sign(num, 1);
    CHECK(small_eq(fi_int_format_obj(1234, num), "+1.234"));
    CHECK(small_eq(fi_int_format_obj(0, num), "0"));
    num = fi_style_sign(num, 2);
    CHECK(small_eq(fi_int_format_obj(0, num), "+0"));

    num = fi_style_percent_locale("en");
    CHECK(small_eq(fi_float_format_obj(dbits(0.125), num), "12%"));
    CHECK(small_eq(fi_float_format_obj(dbits(0.126), num), "13%"));
    num = fi_style_precision(num, 1);
    CHECK(small_eq(fi_float_format_obj(dbits(0.125), num), "12.5%"));

    {
        unsigned long long c0, c1;
        pack_small("USD", &c0, &c1);
        num = fi_style_currency(c0, c1, "en");
        CHECK(small_eq(fi_float_format_obj(dbits(1234.5), num), "$1,234.50"));
        fi_release(num);
        pack_small("EUR", &c0, &c1);
        num = fi_style_currency(c0, c1, "de_DE");
        CHECK(small_eq(fi_float_format_obj(dbits(1234.5), num), "1.234,50 €"));
        fi_release(num);
        pack_small("BRL", &c0, &c1);
        num = fi_style_currency(c0, c1, "pt_BR");
        CHECK(small_eq(fi_float_format_obj(dbits(1234.5), num), "R$ 1.234,50"));
    }
    num = fi_style_int_locale("en");
    num = fi_style_notation(num, 1);
    CHECK(small_eq(fi_float_format_obj(dbits(1500), num), "1.5K"));

    CHECK(small_eq(fi_list_format("en", trio, 3, 0, 0), "Ana, Bia, and Caio"));
    CHECK(small_eq(fi_list_format("en", trio, 2, 0, 0), "Ana and Bia"));
    CHECK(small_eq(fi_list_format("en", trio, 3, 0, 2), "Ana, Bia, Caio"));
    CHECK(small_eq(fi_list_format("en", trio, 3, 1, 0), "Ana, Bia, or Caio"));
    CHECK(small_eq(fi_list_format("pt_BR", trio, 3, 0, 0), "Ana, Bia e Caio"));
    CHECK(small_eq(fi_list_format("de", trio, 3, 0, 0), "Ana, Bia und Caio"));
    CHECK(small_eq(fi_list_format("fr", trio, 3, 0, 0), "Ana, Bia et Caio"));

    CHECK(fi_str_cmp_cstr("file2", "file10", 2) < 0);
    CHECK(fi_str_cmp_cstr("file2", "file10", 1) > 0);
    CHECK(fi_str_cmp_cstr("A", "a", 0) < 0);
    CHECK(fi_str_cmp_cstr("A", "a", 1) == 0);
    {
        fi_arg args[2];
        memset(args, 0, sizeof args);
        args[0].kind = 0;
        args[0].i = 42;
        CHECK(small_eq(fi_str_format_args("%d", args, 1), "42"));
        args[0].kind = 2;
        args[0].cstr = "a";
        args[1].kind = 0;
        args[1].i = 2;
        CHECK(small_eq(fi_str_format_args("%@-%d", args, 2), "a-2"));
        args[0].kind = 0;
        args[0].i = 3;
        CHECK(small_eq(fi_str_format_args("%d", args, 1), "3"));
    }

    {
        fi_person *p = fi_person_new();
        unsigned long long w0, w1;
        pack_small("Ada", &w0, &w1);
        fi_person_set(&p, 1, w0, w1);
        pack_small("Lovelace", &w0, &w1);
        fi_person_set(&p, 3, w0, w1);
        CHECK(small_eq(fi_person_formatted(p), "Ada Lovelace"));
        CHECK(fi_person_get(p, 0).w0 == 0 && fi_person_get(p, 0).w1 == 0);
        fi_release(p);
    }
    {
        fi_attr *a = fi_attr_from_cstr("Hello", 5);
        fi_attr *b = fi_attr_from_cstr(" World", 6);
        fi_attr *c = fi_attr_concat(a, b);
        eq(fi_attr_plain(c), "Hello World");
        a = fi_attr_markdown("**oi**", 0);
        eq(fi_attr_plain(a), "oi");
        CHECK(fi_attr_nruns(a) == 1);
        CHECK(fi_attr_run_kind(a, 0) == 1);
        a = fi_attr_markdown("[a](https://mos)", 0);
        eq(fi_attr_plain(a), "a");
        CHECK(small_eq(fi_attr_member(a, fi_link_key), "https://mos"));
        CHECK(fi_attr_member(a, "nope").w0 == 0 && fi_attr_member(a, "nope").w1 == 0);
    }
    {
        unsigned char dec[32], out[32];
        int nil = 0;
        fi_dec_from_int(123, dec);
        CHECK(fi_dec_exponent(dec) == 0);
        CHECK(!fi_dec_isnan(dec));
        fi_dec_from_int(2, dec);
        fi_dec_negate(dec);
        CHECK(fi_dec_format(dec, txt, sizeof txt) == 0);
        eq(txt, "-2");
        fi_dec_from_int(2, dec);
        fi_dec_pow(dec, 10, out);
        CHECK(fi_dec_format(out, txt, sizeof txt) == 0);
        eq(txt, "1024");
        fi_dec_from_int(0, dec);
        fi_dec_pow(dec, 0, out);
        CHECK(fi_dec_isnan(out));
        CHECK(fi_dec_from_locale("1,234.50", "en", dec, &nil) == 0 && nil == 0);
        CHECK(fi_dec_format(dec, txt, sizeof txt) == 0);
        eq(txt, "1234.5");
        CHECK(fi_dec_from_locale("1.234,50", "pt_BR", dec, &nil) == 0);
        CHECK(fi_dec_format(dec, txt, sizeof txt) == 0);
        eq(txt, "1234.5");
        fi_dec_significand(dec, out);
        CHECK(fi_dec_exponent(out) == 0);
    }
    {
        struct { double v; const char *u; } m;
        m.v = 1500;
        m.u = "m";
        CHECK(small_eq(fi_meas_format(&m), "1,500 m"));
    }
    {
        int enc = 0;
        fi_plist_set_format(&enc, 100);
        CHECK(fi_plist_get_format(&enc) == 100);
        fi_plist_set_format(&enc, 200);
        CHECK(fi_plist_get_format(&enc) == 200);
    }
    {
        double range[2];
        range[0] = 731289600.0;
        range[1] = 731289600.0 + 86400.0;
        st = fi_style_date_default();
        st = fi_style_field(st, 0, 1);
        st = fi_style_field(st, 1, 2);
        st = fi_style_field(st, 2, 1);
        CHECK(small_eq(fi_range_format_obj(range, st), "Mar 5, 2024 – Mar 6, 2024"));
    }
    {
        fi_obj *a = fi_style_date_default();
        void *src = a, *dst = 0;
        void *ma = dlsym(RTLD_DEFAULT, "$s10Foundation4DateV11FormatStyleVMa");
        const void *md = (const void *)call_rax(ma);
        int ref = 0;
        fi_vw_copy(&dst, &src, md);
        memcpy(&ref, a, sizeof ref);
        CHECK(dst == src && ref == 2);
        fi_vw_destroy(&dst, md);
        memcpy(&ref, a, sizeof ref);
        CHECK(ref == 1);
        fi_release(a);
    }
    {
        void *fn = dlsym(RTLD_DEFAULT, "$s10Foundation4DateV11FormatStyleV0bD0V11abbreviatedAGvgZ");
        CHECK(call_rax(fn) == 2);
        fn = dlsym(RTLD_DEFAULT, "$s10Foundation4DateV11FormatStyleV04TimeD0V9shortenedAGvgZ");
        CHECK(call_rax(fn) == 1);
        {
            fi_obj *style = fi_style_date_default();
            fi_swstr r;
            register unsigned long long datebits __asm__("r13") = bits;
            register void *stp __asm__("rdi") = style;
            __asm__ volatile("call \"$s10Foundation4DateV9formattedy12FormatOutputQzxAA0D5StyleRzAC0D5InputRtzlF\"\n\t"
                             : "=a"(r.w0), "=d"(r.w1)
                             : "r"(datebits), "r"(stp)
                             : "rcx", "rsi", "r8", "r9", "r10", "r11", "memory", "cc");
            CHECK(small_eq(r, "3/5/2024, 12:00 AM"));
            fi_release(style);
        }
        fn = dlsym(RTLD_DEFAULT, "$s10Foundation11JSONEncoderC20DateEncodingStrategyO9formattedyAESo15NSDateFormatterCcAEmFWC");
        {
            unsigned long long rax = 0x1111, rdx = 0;
            __asm__ volatile("call *%2\n\t" : "+a"(rax), "=d"(rdx) : "r"(fn), "D"(0x1111ull) : "rcx", "rsi", "r8", "r9", "r10", "r11", "cc", "memory");
            CHECK(rax == 0x1111 && rdx == 1);
        }
    }
}

extern char fi_last_full[192];

static int small_eq(fi_swstr s, const char *want) {
    unsigned char b[16];
    int n = (int)strlen(want);
    if (n > 15) {
        char prefix[16];
        if (strcmp(fi_last_full, want) != 0) return 0;
        memcpy(prefix, want, 15);
        prefix[15] = 0;
        return small_eq(s, prefix);
    }
    memcpy(b, &s.w0, 8);
    memcpy(b + 8, &s.w1, 8);
    return (b[15] == (unsigned char)(0xE0 | n)) && memcmp(b, want, (size_t)n) == 0;
}

int main(void) {
    char b[80];
    unsigned char da[32], db[32], dr[32];
    double out = 0;
    /* fi_date_styles(bits, time_style, date_style): o thunk swiftcc põe o tempo em rsi e a data em rdi. */
    CHECK(small_eq(fi_date_styles(dbits(0), 1, 0), "12:00 AM"));
    CHECK(small_eq(fi_date_styles(dbits(731289600.0), 0, 1), "3/5/2024"));
    CHECK(small_eq(fi_date_styles(dbits(731289600.0), 0, 2), "Mar 5, 2024"));
    {
        /* O thunk swiftcc lê self em r13, o estilo de data em rdi e o de hora em rsi. */
        fi_swstr r;
        register unsigned long long bits __asm__("r13") = dbits(0);
        register unsigned long long date_style __asm__("rdi") = 0;
        register unsigned long long time_style __asm__("rsi") = 1;
        __asm__ volatile("call \"$s10Foundation4DateV9formatted4date4timeSSAC11FormatStyleV0bG0V_AH04TimeG0VtF\"\n\t"
                         : "=a"(r.w0), "=d"(r.w1)
                         : "r"(bits), "r"(date_style), "r"(time_style)
                         : "rcx", "r8", "r9", "r10", "r11", "memory", "cc");
        CHECK(small_eq(r, "12:00 AM"));
    }
    {
        unsigned char dec[32];
        char txt[32];
        memset(dec, 0xab, sizeof dec);
        register long long val __asm__("rdi") = 42;
        register void *dst __asm__("rax") = dec;
        __asm__ volatile("call \"$sSo9NSDecimala10FoundationEyABSicfC\"\n\t"
                         : "+r"(dst)
                         : "r"(val)
                         : "rsi", "rdx", "rcx", "r8", "r9", "r10", "r11", "memory", "cc");
        CHECK(fi_dec_format(dec, txt, sizeof txt) == 0);
        eq(txt, "42");
    }
    {
        CHECK(fi_iso_parse_buf("2024-03-05T00:00:00Z") == dbits(731289600.0));
        CHECK(fi_iso_parse_buf("2024-03-05") == 0);
    }
    fi_dec_from_int(1, da);
    fi_dec_from_int(2, db);
    fi_dec_binop(da, db, dr, 0);
    CHECK(fi_dec_format(dr, b, sizeof b) == 0); eq(b, "3");
    {
        unsigned long long z = dbits(0.1), w = dbits(0.2);
        fi_dec_from_double(z, da);
        fi_dec_from_double(w, db);
        fi_dec_binop(da, db, dr, 0);
        CHECK(fi_dec_format(dr, b, sizeof b) == 0); eq(b, "0.3");
    }
    fi_dec_from_int(-2, da);
    fi_dec_from_int(5, db);
    CHECK(fi_dec_cmp(da, db) < 0);
    CHECK(fi_dec_cmp(da, da) == 0);
    fi_dec_binop(db, da, dr, 1);
    CHECK(fi_dec_format(dr, b, sizeof b) == 0); eq(b, "7");
    fi_dec_from_int(6, da);
    fi_dec_from_int(3, db);
    fi_dec_binop(da, db, dr, 3);
    CHECK(fi_dec_format(dr, b, sizeof b) == 0); eq(b, "2");
    fi_dec_from_int(1, da);
    fi_dec_from_int(4, db);
    fi_dec_binop(da, db, dr, 3);
    CHECK(fi_dec_format(dr, b, sizeof b) == 0); eq(b, "0.25");
    CHECK(fi_convert_units("m", "km", 1500, &out) == 0 && out == 1.5);
    CHECK(fi_convert_units("g", "kg", 1000, &out) == 0 && out == 1);
    extra();
    if (fails) { fprintf(stderr, "fitest: %d falhas em %d\n", fails, checks); return 1; }
    printf("fitest: %d/%d PASS\n", checks, checks);
    return 0;
}
