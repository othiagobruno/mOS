/* Lado C dos símbolos Swift de formatação. Incluído só por fi_swift.c.
 * Strings de até 15 bytes saem no small-string do Swift (último byte 0xE0|n).
 * Maiores pedem String._fromUTF8Repairing no libswiftCore, se ele já estiver carregado.
 */
#include "fmt_core.inc"

typedef struct { unsigned long long w0, w1; } fi_swstr;

static fi_swstr fi_small(const char *s, int n) {
    unsigned char b[16];
    fi_swstr r;
    memset(b, 0, sizeof b);
    if (n < 0) n = 0;
    if (n > 15) n = 15;
    if (s && n) memcpy(b, s, (size_t)n);
    b[15] = (unsigned char)(0xE0 | n);
    memcpy(&r.w0, b, 8);
    memcpy(&r.w1, b + 8, 8);
    return r;
}

typedef fi_swstr (*fi_repair_fn)(const void *bytes, long n);
static fi_repair_fn fi_repair_cached;
static int fi_repair_tried;

static fi_repair_fn fi_repair(void) {
    if (fi_repair_tried) return fi_repair_cached;
    fi_repair_tried = 1;
#ifdef FI_HOST
    fi_repair_cached = (fi_repair_fn)dlsym((void *)-2,
        "$sSS18_fromUTF8RepairingySS6result_Sb11repairsMadetSRys5UInt8VGFZ");
#else
    fi_repair_cached = (fi_repair_fn)dlsym((void *)-2,
        "$sSS18_fromUTF8RepairingySS6result_Sb11repairsMadetSRys5UInt8VGFZ");
#endif
    return fi_repair_cached;
}

static fi_swstr fi_pack(const char *s, int n) {
    fi_repair_fn fn;
    if (!s) n = 0;
    if (n < 0) n = (int)strlen(s);
    if (n <= 15) return fi_small(s, n);
    fn = fi_repair();
    if (fn) {
        fi_swstr r = fn(s, n);
        if (r.w0 || r.w1) return r;
    }
    /* libswiftCore ainda não está na imagem: os 15 primeiros bytes, e o resto se perde. */
    return fi_small(s, 15);
}

static int fi_read_small(unsigned long long w0, unsigned long long w1, char *buf, int cap) {
    unsigned char b[16];
    int n;
    memcpy(b, &w0, 8);
    memcpy(b + 8, &w1, 8);
    if ((b[15] & 0xF0) != 0xE0) return -1;
    n = b[15] & 0x0F;
    if (n >= cap) n = cap - 1;
    memcpy(buf, b, (size_t)n);
    buf[n] = 0;
    return n;
}

static const long long FI_UNIX_2001 = 978307200LL;

static void fi_parts(unsigned long long bits, int *y, int *m, int *d, int *hh, int *mm, int *ss) {
    double ref;
    long long unix_s;
    memcpy(&ref, &bits, sizeof ref);
    if (ref >= 0) unix_s = (long long)(ref + (double)FI_UNIX_2001);
    else unix_s = (long long)(ref + (double)FI_UNIX_2001 - 1.0);
    mf_civil_from_unix(unix_s, y, m, d, hh, mm, ss);
}

static int fi_apple_date(unsigned long long raw) {
    if (raw < 1 || raw > 4) return -1;
    return (int)raw - 1;
}

static fi_swstr fi_pack_parts(int y, int m, int d, int hh, int mm, int ss, int date_style, int time_style, int gmt) {
    char buf[200], extra[220];
    if (mf_datetime("en", y, m, d, hh, mm, ss, date_style, time_style, buf, sizeof buf) < 0)
        return fi_small("", 0);
    if (gmt && time_style > 0) {
        snprintf(extra, sizeof extra, "%s GMT", buf);
        return fi_pack(extra, (int)strlen(extra));
    }
    return fi_pack(buf, (int)strlen(buf));
}

fi_swstr fi_date_default(unsigned long long bits) {
    int y, m, d, hh, mm, ss;
    fi_parts(bits, &y, &m, &d, &hh, &mm, &ss);
    return fi_pack_parts(y, m, d, hh, mm, ss, MF_DATE_NUMERIC, 1, 0);
}

fi_swstr fi_date_styles(unsigned long long bits, unsigned long long time_style, unsigned long long date_style) {
    int y, m, d, hh, mm, ss;
    fi_parts(bits, &y, &m, &d, &hh, &mm, &ss);
    return fi_pack_parts(y, m, d, hh, mm, ss, fi_apple_date(date_style), time_style > 3 ? 0 : (int)time_style, time_style == 3);
}

fi_swstr fi_iso_format(unsigned long long bits) {
    int y, m, d, hh, mm, ss;
    char buf[40];
    fi_parts(bits, &y, &m, &d, &hh, &mm, &ss);
    snprintf(buf, sizeof buf, "%04d-%02d-%02dT%02d:%02d:%02dZ", y, m, d, hh, mm, ss);
    return fi_pack(buf, (int)strlen(buf));
}

/* Devolve os bits do Double (segundos desde 2001-01-01). Texto que não é o ISO que gravamos vira 0. */
unsigned long long fi_iso_parse_buf(const char *s) {
    int y, m, d, hh, mm, ss;
    double ref;
    unsigned long long bits = 0;
    long long unix_s;
    if (!s || sscanf(s, "%d-%d-%dT%d:%d:%dZ", &y, &m, &d, &hh, &mm, &ss) != 6) return 0;
    /* dia civil -> unix, inverso aproximado via mf só no sentido já testado: reconstitui pelo mesmo calendário. */
    {
        int i;
        long long lo = 0, hi = 20000000000LL;
        /* busca o dia; o horário entra depois. Suficiente para os instantes que o format produz. */
        unix_s = -1;
        for (i = 0; i < 48; i++) {
            long long mid = lo + (hi - lo) / 2;
            int Y, M, D, H, N, S;
            mf_civil_from_unix(mid, &Y, &M, &D, &H, &N, &S);
            if (Y < y || (Y == y && (M < m || (M == m && D < d)))) lo = mid + 1;
            else hi = mid;
        }
        {
            int Y, M, D, H, N, S;
            mf_civil_from_unix(lo, &Y, &M, &D, &H, &N, &S);
            if (Y == y && M == m && D == d) unix_s = lo - (long long)H * 3600 - (long long)N * 60 - S + (long long)hh * 3600 + (long long)mm * 60 + ss;
        }
    }
    if (unix_s < 0) return 0;
    ref = (double)(unix_s - FI_UNIX_2001);
    memcpy(&bits, &ref, sizeof bits);
    return bits;
}

unsigned long long fi_iso_parse(unsigned long long w0, unsigned long long w1) {
    char buf[64];
    if (fi_read_small(w0, w1, buf, sizeof buf) >= 0) return fi_iso_parse_buf(buf);
    return 0;
}

/* ---- NSDecimal: inteiro em base 65536 + expoente decimal, layout do clang em x86_64. */
typedef struct {
    signed int _exponent:8;
    unsigned int _length:4;
    unsigned int _isNegative:1;
    unsigned int _isCompact:1;
    unsigned int _reserved:18;
    unsigned short _mantissa[8];
} fi_nsdec;

static void fi_dec_zero(fi_nsdec *o) { memset(o, 0, sizeof *o); }

static void fi_dec_mul10(unsigned short *m) {
    unsigned carry = 0;
    int i;
    for (i = 0; i < 8; i++) {
        unsigned v = (unsigned)m[i] * 10u + carry;
        m[i] = (unsigned short)v;
        carry = v >> 16;
    }
}

static void fi_dec_add_digit(unsigned short *m, unsigned digit) {
    unsigned carry = digit;
    int i;
    for (i = 0; i < 8 && carry; i++) {
        unsigned v = (unsigned)m[i] + carry;
        m[i] = (unsigned short)v;
        carry = v >> 16;
    }
}

static int fi_dec_from_str(fi_nsdec *o, const char *s) {
    mf_dec p;
    int i;
    if (mf_dec_parse(&p, s)) return -1;
    fi_dec_zero(o);
    if (p.n == 1 && p.d[0] == '0') return 0;
    for (i = 0; i < p.n; i++) fi_dec_mul10(o->_mantissa), fi_dec_add_digit(o->_mantissa, (unsigned)(p.d[i] - '0'));
    o->_exponent = (signed int)(-p.scale);
    o->_isNegative = p.neg ? 1u : 0u;
    o->_isCompact = 1u;
    o->_length = 0;
    for (i = 7; i >= 0; i--) if (o->_mantissa[i]) { o->_length = (unsigned)(i + 1); break; }
    return 0;
}

static int fi_dec_to_str(const fi_nsdec *a, char *out, size_t cap) {
    unsigned short m[8];
    char digs[80];
    int n = 0, i, exp;
    if ((a->_exponent == -128 && a->_length == 0) || (a->_reserved & 1)) {
        snprintf(out, cap, "NaN");
        return 0;
    }
    memcpy(m, a->_mantissa, sizeof m);
    if (a->_length == 0) { snprintf(out, cap, "0"); return 0; }
    while (1) {
        unsigned rem = 0;
        int any = 0;
        for (i = 7; i >= 0; i--) {
            unsigned v = (rem << 16) | m[i];
            m[i] = (unsigned short)(v / 10);
            rem = v % 10;
            if (m[i]) any = 1;
        }
        digs[n++] = (char)('0' + rem);
        if (!any || n > 70) break;
    }
    while (n > 1 && digs[n - 1] == '0') n--;
    {
        char body[80];
        int k = 0;
        if (a->_isNegative) body[k++] = '-';
        for (i = n - 1; i >= 0; i--) body[k++] = digs[i];
        body[k] = 0;
        exp = (int)a->_exponent;
        if (exp == 0) return mf_dec_add(body, "0", out, cap);
        if (exp > 0) {
            char z[80];
            snprintf(z, sizeof z, "%s", body);
            for (i = 0; i < exp && strlen(z) + 1 < sizeof z; i++) strcat(z, "0");
            return mf_dec_add(z, "0", out, cap);
        }
        {
            char z[80];
            int ip = n + exp;
            int t = 0;
            if (a->_isNegative) z[t++] = '-';
            if (ip <= 0) {
                z[t++] = '0';
                z[t++] = '.';
                for (i = 0; i < -ip; i++) z[t++] = '0';
                for (i = n - 1; i >= 0; i--) z[t++] = digs[i];
            } else {
                for (i = n - 1; i >= n - ip; i--) z[t++] = digs[i];
                z[t++] = '.';
                for (; i >= 0; i--) z[t++] = digs[i];
            }
            z[t] = 0;
            return mf_dec_add(z, "0", out, cap);
        }
    }
}

static void fi_dec_nan(fi_nsdec *o) {
    fi_dec_zero(o);
    o->_exponent = -128;
}

void fi_dec_from_int(long long v, void *out) {
    char b[40];
    fi_nsdec d;
    snprintf(b, sizeof b, "%lld", v);
    if (fi_dec_from_str(&d, b)) fi_dec_zero(&d);
    memcpy(out, &d, sizeof d);
}

void fi_dec_from_double(unsigned long long bits, void *out) {
    double v;
    char b[64];
    fi_nsdec d;
    memcpy(&v, &bits, sizeof v);
    snprintf(b, sizeof b, "%.15g", v);
    if (fi_dec_from_str(&d, b)) fi_dec_zero(&d);
    memcpy(out, &d, sizeof d);
}

static int fi_dec_bin(const fi_nsdec *a, const fi_nsdec *b, fi_nsdec *o, int op) {
    char as[80], bs[80], rs[80];
    if (fi_dec_to_str(a, as, sizeof as) || fi_dec_to_str(b, bs, sizeof bs)) return -1;
    if (!strcmp(as, "NaN") || !strcmp(bs, "NaN")) { fi_dec_nan(o); return 0; }
    if (op == 0) { if (mf_dec_add(as, bs, rs, sizeof rs)) return -1; }
    else if (op == 1) {
        char neg[80];
        if (bs[0] == '-') snprintf(neg, sizeof neg, "%s", bs + 1);
        else snprintf(neg, sizeof neg, "-%s", bs);
        if (mf_dec_add(as, neg, rs, sizeof rs)) return -1;
    } else if (op == 2 || op == 3) {
        mf_dec x, y;
        int acc[96];
        int i, j, n, carry;
        if (mf_dec_parse(&x, as) || mf_dec_parse(&y, bs)) return -1;
        if (op == 3 && y.n == 1 && y.d[0] == '0') { fi_dec_nan(o); return 0; }
        if (op == 2) {
            memset(acc, 0, sizeof acc);
            for (i = 0; i < x.n; i++) for (j = 0; j < y.n; j++) acc[i + j] += (x.d[x.n - 1 - i] - '0') * (y.d[y.n - 1 - j] - '0');
            carry = 0;
            n = x.n + y.n;
            for (i = 0; i < n; i++) { int s = acc[i] + carry; acc[i] = s % 10; carry = s / 10; }
            while (n > 1 && acc[n - 1] == 0) n--;
            {
                char body[96];
                int k = 0, sc = x.scale + y.scale;
                if (x.neg ^ y.neg) body[k++] = '-';
                for (i = n - 1; i >= 0; i--) body[k++] = (char)('0' + acc[i]);
                body[k] = 0;
                if (sc > 0) {
                    char tmp[120];
                    int ip = n - sc;
                    int t = 0;
                    if (body[0] == '-') tmp[t++] = '-';
                    if (ip <= 0) {
                        tmp[t++] = '0'; tmp[t++] = '.';
                        for (i = 0; i < -ip; i++) tmp[t++] = '0';
                        for (i = (body[0] == '-' ? 1 : 0); body[i]; i++) tmp[t++] = body[i];
                    } else {
                        int start = body[0] == '-' ? 1 : 0;
                        for (i = 0; i < ip; i++) tmp[t++] = body[start + i];
                        tmp[t++] = '.';
                        for (; body[start + i]; i++) tmp[t++] = body[start + i];
                    }
                    tmp[t] = 0;
                    snprintf(rs, sizeof rs, "%s", tmp);
                } else snprintf(rs, sizeof rs, "%s", body);
                if (mf_dec_add(rs, "0", rs, sizeof rs)) return -1;
            }
        } else {
            /* divisão: quociente com até 18 casas, no núcleo decimal (subtrações do divisor). */
            char num[80], den[80], q[80];
            int qn = 0, sc, step;
            snprintf(num, sizeof num, "%s", x.neg ? as + 1 : as);
            snprintf(den, sizeof den, "%s", y.neg ? bs + 1 : bs);
            {
                char scaled[80];
                int shift = 18 + y.scale - x.scale;
                if (shift < 0) shift = 0;
                snprintf(scaled, sizeof scaled, "%s", num);
                for (i = 0; i < shift && strlen(scaled) + 1 < sizeof scaled; i++) strcat(scaled, "0");
                /* tira o ponto */
                {
                    char flat[80];
                    int t = 0;
                    for (i = 0; scaled[i]; i++) if (scaled[i] != '.') flat[t++] = scaled[i];
                    flat[t] = 0;
                    snprintf(scaled, sizeof scaled, "%s", flat);
                }
                {
                    char denflat[80];
                    int t = 0;
                    for (i = 0; den[i]; i++) if (den[i] != '.') denflat[t++] = den[i];
                    denflat[t] = 0;
                    snprintf(den, sizeof den, "%s", denflat);
                    snprintf(num, sizeof num, "%s", scaled);
                }
            }
            sc = 18;
            {
                char rem[80];
                snprintf(rem, sizeof rem, "0");
                for (step = 0; num[step]; step++) {
                    char cur[80];
                    int k;
                    snprintf(cur, sizeof cur, "%s%c", strcmp(rem, "0") ? rem : "", num[step]);
                    if (cur[0] == 0) snprintf(cur, sizeof cur, "0");
                    k = 0;
                    while (k < 9) {
                        char trial[8], sum[80];
                        snprintf(trial, sizeof trial, "%d", k + 1);
                        if (mf_dec_add(den, "0", sum, sizeof sum)) break;
                        /* compara (k+1)*den com cur via subtração repetida abaixo */
                        {
                            char prod[80];
                            int t;
                            snprintf(prod, sizeof prod, "0");
                            for (t = 0; t < k + 1; t++) if (mf_dec_add(prod, den, prod, sizeof prod)) break;
                            {
                                char neg[80], diff[80];
                                if (prod[0] == '-') snprintf(neg, sizeof neg, "%s", prod + 1);
                                else snprintf(neg, sizeof neg, "-%s", prod);
                                /* cur + (-prod) >= 0 então prod <= cur */
                                if (mf_dec_add(cur, neg, diff, sizeof diff)) break;
                                if (diff[0] == '-') break;
                                k++;
                            }
                        }
                    }
                    q[qn++] = (char)('0' + k);
                    {
                        char prod[80], neg[80];
                        int t;
                        snprintf(prod, sizeof prod, "0");
                        for (t = 0; t < k; t++) mf_dec_add(prod, den, prod, sizeof prod);
                        if (prod[0] == '-') snprintf(neg, sizeof neg, "%s", prod + 1);
                        else snprintf(neg, sizeof neg, "-%s", prod);
                        mf_dec_add(cur, neg, rem, sizeof rem);
                    }
                }
                q[qn] = 0;
                {
                    char outq[96];
                    int ip = qn - sc;
                    int t = 0;
                    if (x.neg ^ y.neg) outq[t++] = '-';
                    if (ip <= 0) {
                        outq[t++] = '0'; outq[t++] = '.';
                        for (i = 0; i < -ip; i++) outq[t++] = '0';
                        for (i = 0; q[i]; i++) outq[t++] = q[i];
                    } else {
                        for (i = 0; i < ip; i++) outq[t++] = q[i];
                        outq[t++] = '.';
                        for (; q[i]; i++) outq[t++] = q[i];
                    }
                    outq[t] = 0;
                    if (mf_dec_add(outq, "0", rs, sizeof rs)) return -1;
                }
            }
        }
    } else return -1;
    return fi_dec_from_str(o, rs);
}

void fi_dec_binop(const void *a, const void *b, void *out, int op) {
    fi_nsdec da, db, dr;
    memcpy(&da, a, sizeof da);
    memcpy(&db, b, sizeof db);
    if (fi_dec_bin(&da, &db, &dr, op)) fi_dec_nan(&dr);
    memcpy(out, &dr, sizeof dr);
}

int fi_dec_cmp(const void *a, const void *b) {
    char as[80], bs[80], diff[80];
    fi_nsdec da, db;
    memcpy(&da, a, sizeof da);
    memcpy(&db, b, sizeof db);
    if (fi_dec_to_str(&da, as, sizeof as) || fi_dec_to_str(&db, bs, sizeof bs)) return 0;
    if (!strcmp(as, bs)) return 0;
    {
        char neg[80];
        if (bs[0] == '-') snprintf(neg, sizeof neg, "%s", bs + 1);
        else snprintf(neg, sizeof neg, "-%s", bs);
        if (mf_dec_add(as, neg, diff, sizeof diff)) return 0;
        if (diff[0] == '-' ) return -1;
        if (!strcmp(diff, "0")) return 0;
        return 1;
    }
}

/* texto do decimal, para o teste de host */
int fi_dec_format(const void *a, char *out, size_t cap) {
    fi_nsdec d;
    memcpy(&d, a, sizeof d);
    return fi_dec_to_str(&d, out, cap);
}

int fi_convert_units(const char *from, const char *to, double v, double *out) {
    return mf_convert(from, to, v, out);
}
