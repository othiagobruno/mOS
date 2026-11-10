#define _POSIX_C_SOURCE 200809L
/* gcc -std=c11 -Wall -Wextra -O1 -DFI_HOST userland/libsystem/fi_swift.c tools/fitest.c -ldl -o /tmp/fitest */
#include <stdio.h>
#include <string.h>
#include <stdint.h>

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

static int small_eq(fi_swstr s, const char *want) {
    unsigned char b[16];
    int n = (int)strlen(want);
    memcpy(b, &s.w0, 8);
    memcpy(b + 8, &s.w1, 8);
    return n <= 15 && (b[15] == (unsigned char)(0xE0 | n)) && memcmp(b, want, (size_t)n) == 0;
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
    if (fails) { fprintf(stderr, "fitest: %d falhas em %d\n", fails, checks); return 1; }
    printf("fitest: %d/%d PASS\n", checks, checks);
    return 0;
}
