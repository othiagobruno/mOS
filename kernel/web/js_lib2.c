/* mOS — motor JavaScript "Brisa": biblioteca padrão, parte 2 (JSON, Date, RegExp, Promise, console) */
#include "js_int.h"

#define NATIVE(name) static jsv name(jsv thisv, int argc, jsv *argv, jsobj *self, int flags)
#define TRYV(x) do { if ((x) == JS_EXC) return JS_EXC; } while (0)
#define UNUSED_ARGS (void)thisv; (void)argc; (void)argv; (void)self; (void)flags

/* ============================================================ JSON */
typedef struct { const char *s; int len, pos; int depth; } jparse;

static void jws(jparse *P) { while (P->pos < P->len && (P->s[P->pos] == ' ' || P->s[P->pos] == '\t' || P->s[P->pos] == '\n' || P->s[P->pos] == '\r')) P->pos++; }

static jsv jerr(jparse *P) {
    if (P->pos >= P->len) return js_throw_err(E_SYNTAX, "Fim inesperado do JSON");
    char c = P->s[P->pos];
    return js_throw_err(E_SYNTAX, "Token inesperado '%c' no JSON na posição %d", c >= 32 && c < 127 ? c : '?', P->pos);
}

static int jhex(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if ((c | 32) >= 'a' && (c | 32) <= 'f') return (c | 32) - 'a' + 10;
    return -1;
}

static jsstr *jstring(jparse *P) {
    P->pos++;
    sbuf_t sb = {0};
    while (P->pos < P->len) {
        char c = P->s[P->pos];
        if (c == '"') { P->pos++; jsstr *r = js_strn(sb.p ? sb.p : "", sb.len); sb_free(&sb); return r; }
        if ((u8)c < 0x20) break;
        if (c == '\\') {
            P->pos++;
            if (P->pos >= P->len) break;
            char e = P->s[P->pos++];
            switch (e) {
            case '"': sb_putc(&sb, '"'); break;
            case '\\': sb_putc(&sb, '\\'); break;
            case '/': sb_putc(&sb, '/'); break;
            case 'b': sb_putc(&sb, '\b'); break;
            case 'f': sb_putc(&sb, '\f'); break;
            case 'n': sb_putc(&sb, '\n'); break;
            case 'r': sb_putc(&sb, '\r'); break;
            case 't': sb_putc(&sb, '\t'); break;
            case 'u': {
                u32 cp = 0;
                for (int i = 0; i < 4; i++) {
                    int h = P->pos < P->len ? jhex(P->s[P->pos]) : -1;
                    if (h < 0) { sb_free(&sb); return NULL; }
                    cp = cp * 16 + (u32)h;
                    P->pos++;
                }
                if (cp >= 0xD800 && cp < 0xDC00 && P->pos + 5 < P->len && P->s[P->pos] == '\\' && P->s[P->pos + 1] == 'u') {
                    u32 lo = 0;
                    bool ok = true;
                    for (int i = 0; i < 4; i++) { int h = jhex(P->s[P->pos + 2 + i]); if (h < 0) { ok = false; break; } lo = lo * 16 + (u32)h; }
                    if (ok && lo >= 0xDC00 && lo < 0xE000) { cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00); P->pos += 6; }
                }
                char t[4];
                sb_putn(&sb, t, js_utf8_put(t, cp));
                break;
            }
            default: sb_free(&sb); P->pos--; return NULL;
            }
            continue;
        }
        int st = P->pos;
        while (P->pos < P->len && P->s[P->pos] != '"' && P->s[P->pos] != '\\' && (u8)P->s[P->pos] >= 0x20) P->pos++;
        sb_putn(&sb, P->s + st, P->pos - st);
    }
    sb_free(&sb);
    return NULL;
}

static jsv jvalue(jparse *P) {
    jws(P);
    if (P->pos >= P->len) return jerr(P);
    if (++P->depth > 500) return js_throw_err(E_RANGE, "JSON aninhado demais");
    char c = P->s[P->pos];
    jsv r;
    if (c == '{') {
        P->pos++;
        jsobj *o = js_newobj(J.realm->p_object);
        jws(P);
        if (P->pos < P->len && P->s[P->pos] == '}') { P->pos++; P->depth--; return jv_obj(o); }
        for (;;) {
            jws(P);
            if (P->pos >= P->len || P->s[P->pos] != '"') return jerr(P);
            jsstr *k = jstring(P);
            if (!k) return jerr(P);
            jws(P);
            if (P->pos >= P->len || P->s[P->pos] != ':') return jerr(P);
            P->pos++;
            jsv v = jvalue(P);
            TRYV(v);
            u32 idx;
            if (js_str_isindex(k, &idx)) { if (js_set(jv_obj(o), jv_str(k), v) < 0) return JS_EXC; }
            else js_defprop(o, k, v, 0);
            jws(P);
            if (P->pos < P->len && P->s[P->pos] == ',') { P->pos++; continue; }
            if (P->pos < P->len && P->s[P->pos] == '}') { P->pos++; break; }
            return jerr(P);
        }
        r = jv_obj(o);
    } else if (c == '[') {
        P->pos++;
        jsobj *a = js_newarr(0);
        jws(P);
        if (P->pos < P->len && P->s[P->pos] == ']') { P->pos++; P->depth--; return jv_obj(a); }
        for (;;) {
            jsv v = jvalue(P);
            TRYV(v);
            js_arr_push(a, v);
            jws(P);
            if (P->pos < P->len && P->s[P->pos] == ',') { P->pos++; continue; }
            if (P->pos < P->len && P->s[P->pos] == ']') { P->pos++; break; }
            return jerr(P);
        }
        r = jv_obj(a);
    } else if (c == '"') {
        jsstr *s = jstring(P);
        if (!s) return jerr(P);
        r = jv_str(s);
    } else if (c == '-' || isdigit((u8)c)) {
        int st = P->pos;
        if (c == '-') P->pos++;
        if (P->pos >= P->len || !isdigit((u8)P->s[P->pos])) return jerr(P);
        if (P->s[P->pos] == '0' && P->pos + 1 < P->len && isdigit((u8)P->s[P->pos + 1])) return jerr(P);
        while (P->pos < P->len && isdigit((u8)P->s[P->pos])) P->pos++;
        if (P->pos < P->len && P->s[P->pos] == '.') {
            P->pos++;
            if (P->pos >= P->len || !isdigit((u8)P->s[P->pos])) return jerr(P);
            while (P->pos < P->len && isdigit((u8)P->s[P->pos])) P->pos++;
        }
        if (P->pos < P->len && (P->s[P->pos] == 'e' || P->s[P->pos] == 'E')) {
            P->pos++;
            if (P->pos < P->len && (P->s[P->pos] == '+' || P->s[P->pos] == '-')) P->pos++;
            if (P->pos >= P->len || !isdigit((u8)P->s[P->pos])) return jerr(P);
            while (P->pos < P->len && isdigit((u8)P->s[P->pos])) P->pos++;
        }
        bool neg = P->s[st] == '-';
        int used;
        double d = js_strtod(P->s + st + neg, P->pos - st - neg, &used);
        r = jv_num(neg ? -d : d);
    } else if (P->len - P->pos >= 4 && !strncmp(P->s + P->pos, "true", 4)) { P->pos += 4; r = JS_TRUE; }
    else if (P->len - P->pos >= 5 && !strncmp(P->s + P->pos, "false", 5)) { P->pos += 5; r = JS_FALSE; }
    else if (P->len - P->pos >= 4 && !strncmp(P->s + P->pos, "null", 4)) { P->pos += 4; r = JS_NULL; }
    else return jerr(P);
    P->depth--;
    return r;
}

static jsv revive(jsv holder, jsstr *key, jsv reviver) {
    jsv val = js_getk(holder, key);
    TRYV(val);
    if (jv_isobj(val)) {
        jsobj *keys = js_keylist(jv_o(val), true, false);
        for (u32 i = 0; i < keys->alen; i++) {
            jsstr *k = jv_s(keys->arr[i]);
            jsv nv = revive(val, k, reviver);
            TRYV(nv);
            if (nv == JS_UNDEF) js_delete(val, jv_str(k));
            else if (js_set(val, jv_str(k), nv) < 0) return JS_EXC;
        }
    }
    jsv args[2] = {jv_str(key), val};
    return js_call(reviver, holder, 2, args);
}

jsv js_json_parse(jsstr *s, jsv reviver) {
    jparse P = {s->d, (int)s->len, 0, 0};
    jsv v = jvalue(&P);
    TRYV(v);
    jws(&P);
    if (P.pos < P.len) return jerr(&P);
    if (js_iscallable(reviver)) {
        jsobj *root = js_newobj(J.realm->p_object);
        js_defprop(root, A[A_empty], v, 0);
        return revive(jv_obj(root), A[A_empty], reviver);
    }
    return v;
}

typedef struct { sbuf_t sb; jsv replacer; jsobj *allow; char indent[12]; jsobj *stack; } jstr_t;

static void jquote(sbuf_t *sb, jsstr *s) {
    sb_putc(sb, '"');
    for (int i = 0; i < (int)s->len;) {
        u8 c = (u8)s->d[i];
        if (c == '"') { sb_puts(sb, "\\\""); i++; }
        else if (c == '\\') { sb_puts(sb, "\\\\"); i++; }
        else if (c == '\n') { sb_puts(sb, "\\n"); i++; }
        else if (c == '\r') { sb_puts(sb, "\\r"); i++; }
        else if (c == '\t') { sb_puts(sb, "\\t"); i++; }
        else if (c == '\b') { sb_puts(sb, "\\b"); i++; }
        else if (c == '\f') { sb_puts(sb, "\\f"); i++; }
        else if (c < 0x20) { sb_printf(sb, "\\u%04x", c); i++; }
        else if (c == 0xED && i + 2 < (int)s->len && ((u8)s->d[i + 1] & 0xE0) == 0xA0) {
            int k = i;
            u32 cp = js_utf8_get(s->d, (int)s->len, &k);
            sb_printf(sb, "\\u%04x", cp);
            i = k;
        } else { sb_putc(sb, (char)c); i++; }
    }
    sb_putc(sb, '"');
}

static int jser(jstr_t *S, jsv holder, jsstr *key, jsv v, int level);

static void jnl(jstr_t *S, int level) {
    if (!S->indent[0]) return;
    sb_putc(&S->sb, '\n');
    for (int i = 0; i < level; i++) sb_puts(&S->sb, S->indent);
}

/* 1 escreveu, 0 omitido (undefined), -1 exceção */
static int jser(jstr_t *S, jsv holder, jsstr *key, jsv v, int level) {
    if (jv_isobj(v)) {
        jsv tj = js_getk(v, A[A_toJSON]);
        if (tj == JS_EXC) return -1;
        if (js_iscallable(tj)) { jsv a[1] = {jv_str(key)}; v = js_call(tj, v, 1, a); if (v == JS_EXC) return -1; }
    }
    if (js_iscallable(S->replacer)) {
        jsv a[2] = {jv_str(key), v};
        v = js_call(S->replacer, holder, 2, a);
        if (v == JS_EXC) return -1;
    }
    if (jv_isobj(v)) {
        jsobj *o = jv_o(v);
        if (o->cls == CL_NUMBER) { v = js_tonumv(v); if (v == JS_EXC) return -1; }
        else if (o->cls == CL_STRING) v = o->u.prim;
        else if (o->cls == CL_BOOLEAN) v = o->u.prim;
    }
    if (jv_iscls(v, CL_BIGINT)) v = jv_o(v)->u.prim;
    if (jv_isbig(v)) { js_throw_err(E_TYPE, "Não é possível serializar um BigInt em JSON"); return -1; }
    if (v == JS_NULL) { sb_puts(&S->sb, "null"); return 1; }
    if (v == JS_TRUE) { sb_puts(&S->sb, "true"); return 1; }
    if (v == JS_FALSE) { sb_puts(&S->sb, "false"); return 1; }
    if (jv_isnum(v)) {
        double d = jv_n(v);
        if (!m_isfinite(d)) sb_puts(&S->sb, "null");
        else { char b[40]; js_numfmt(d, b, sizeof b); sb_puts(&S->sb, b); }
        return 1;
    }
    if (jv_isstr(v)) {
        if (jv_s(v)->sym) return 0;
        jquote(&S->sb, jv_s(v));
        return 1;
    }
    if (!jv_isobj(v) || js_iscallable(v)) return 0;
    jsobj *o = jv_o(v);
    for (u32 i = 0; i < S->stack->alen; i++) if (S->stack->arr[i] == v) { js_throw_err(E_TYPE, "Estrutura circular convertida para JSON"); return -1; }
    if (S->stack->alen > 300) { js_throw_err(E_RANGE, "Estrutura aninhada demais para JSON"); return -1; }
    js_arr_push(S->stack, v);
    if (o->cls == CL_ARRAY) {
        sb_putc(&S->sb, '[');
        u32 n = o->alen;
        for (u32 i = 0; i < n; i++) {
            if (i) sb_putc(&S->sb, ',');
            jnl(S, level + 1);
            jsv e = js_getidx(v, i);
            if (e == JS_EXC) return -1;
            char kb[16];
            snprintf(kb, sizeof kb, "%u", i);
            int r = jser(S, v, js_strz(kb), e, level + 1);
            if (r < 0) return -1;
            if (r == 0) sb_puts(&S->sb, "null");
        }
        if (n) jnl(S, level);
        sb_putc(&S->sb, ']');
    } else {
        sb_putc(&S->sb, '{');
        jsobj *keys = S->allow ? S->allow : js_keylist(o, true, false);
        bool any = false;
        for (u32 i = 0; i < keys->alen; i++) {
            jsstr *k = jv_s(keys->arr[i]);
            jsv e = js_getk(v, k);
            if (e == JS_EXC) return -1;
            int mark = S->sb.len;
            if (any) sb_putc(&S->sb, ',');
            jnl(S, level + 1);
            jquote(&S->sb, k);
            sb_putc(&S->sb, ':');
            if (S->indent[0]) sb_putc(&S->sb, ' ');
            int r = jser(S, v, k, e, level + 1);
            if (r < 0) return -1;
            if (r == 0) { S->sb.len = mark; if (S->sb.p) S->sb.p[mark] = 0; continue; }
            any = true;
        }
        if (any) jnl(S, level);
        sb_putc(&S->sb, '}');
    }
    S->stack->alen--;
    return 1;
}

jsv js_json_stringify(jsv v, jsv replacer, jsv space) {
    jstr_t S;
    memset(&S, 0, sizeof S);
    S.replacer = JS_UNDEF;
    S.stack = js_newarr(0);
    if (js_iscallable(replacer)) S.replacer = replacer;
    else if (js_isarray(replacer)) {
        S.allow = js_newarr(0);
        jsobj *ra = jv_o(replacer);
        for (u32 i = 0; i < ra->alen; i++) {
            jsv e = ra->arr[i];
            if (jv_isstr(e) || jv_isnum(e)) { jsstr *k = js_tostr(e); if (k) js_arr_push(S.allow, jv_str(k)); }
        }
    }
    if (jv_iscls(space, CL_NUMBER)) space = jv_o(space)->u.prim;
    if (jv_isnum(space)) {
        int n = (int)CLAMP(jv_n(space), 0, 10);
        for (int i = 0; i < n; i++) S.indent[i] = ' ';
    } else if (jv_isstr(space)) {
        int n = (int)MIN(jv_s(space)->len, 10u);
        memcpy(S.indent, jv_s(space)->d, (usize)n);
    }
    jsobj *holder = js_newobj(J.realm->p_object);
    js_defprop(holder, A[A_empty], v, 0);
    int r = jser(&S, jv_obj(holder), A[A_empty], v, 0);
    if (r < 0) { sb_free(&S.sb); return JS_EXC; }
    if (r == 0) { sb_free(&S.sb); return JS_UNDEF; }
    jsstr *out = js_strn(S.sb.p ? S.sb.p : "", S.sb.len);
    sb_free(&S.sb);
    return jv_str(out);
}

NATIVE(json_parse) {
    UNUSED_ARGS;
    jsstr *s = js_tostr(ARG(0));
    if (!s) return JS_EXC;
    return js_json_parse(s, ARG(1));
}

NATIVE(json_stringify) { UNUSED_ARGS; return js_json_stringify(ARG(0), ARG(1), ARG(2)); }

/* ============================================================ Date */
#define MS_DAY 86400000.0

static double days_from_civil(double y, int m, int d) {
    y -= m <= 2;
    double era = m_floor(y / 400);
    double yoe = y - era * 400;
    double doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    doy = m_floor(doy);
    double doe = yoe * 365 + m_floor(yoe / 4) - m_floor(yoe / 100) + doy;
    return era * 146097 + doe - 719468;
}

static void civil_from_days(double z, int *y, int *m, int *d) {
    z += 719468;
    double era = m_floor(z / 146097);
    double doe = z - era * 146097;
    double yoe = m_floor((doe - m_floor(doe / 1460) + m_floor(doe / 36524) - m_floor(doe / 146096)) / 365);
    double yy = yoe + era * 400;
    double doy = doe - (365 * yoe + m_floor(yoe / 4) - m_floor(yoe / 100));
    double mp = m_floor((5 * doy + 2) / 153);
    *d = (int)(doy - m_floor((153 * mp + 2) / 5) + 1);
    *m = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y = (int)(yy + (*m <= 2));
}

static double make_date(double y, double mon, double d, double h, double mi, double s, double ms) {
    if (!m_isfinite(y) || !m_isfinite(mon) || !m_isfinite(d) || !m_isfinite(h) || !m_isfinite(mi) || !m_isfinite(s) || !m_isfinite(ms))
        return jv_n(JS_NAN_BITS);
    double ym = y + m_floor(mon / 12);
    int mn = (int)(mon - m_floor(mon / 12) * 12);
    double days = days_from_civil(m_trunc(ym), mn + 1, 1) + m_trunc(d) - 1;
    double t = days * MS_DAY + m_trunc(h) * 3600000.0 + m_trunc(mi) * 60000.0 + m_trunc(s) * 1000.0 + m_trunc(ms);
    if (m_abs(t) > 8.64e15) return jv_n(JS_NAN_BITS);
    return t;
}

typedef struct { int y, mon, d, h, mi, s, ms, wd; } dparts;

static void split_date(double t, dparts *p) {
    double days = m_floor(t / MS_DAY);
    double r = t - days * MS_DAY;
    civil_from_days(days, &p->y, &p->mon, &p->d);
    p->mon--;
    int ir = (int)r;
    p->h = ir / 3600000;
    p->mi = ir / 60000 % 60;
    p->s = ir / 1000 % 60;
    p->ms = ir % 1000;
    int wd = (int)m_fmod(days + 4, 7);
    if (wd < 0) wd += 7;
    p->wd = wd;
}

static double date_base;
static u64 date_tick;

double js_date_now(void) {
    if (!date_tick) {
        datetime_t dt;
        rtc_now(&dt);
        date_base = make_date(dt.year, dt.month - 1, dt.day, dt.hour, dt.minute, dt.second, 0);
        date_tick = timer_ticks();
        if (!date_tick) date_tick = 1;
    }
    return date_base + (double)(timer_ticks() - date_tick);
}

static int pnum(const char *s, int len, int *i, int n) {
    int v = 0;
    for (int k = 0; k < n; k++) {
        if (*i >= len || !isdigit((u8)s[*i])) return -1;
        v = v * 10 + (s[(*i)++] - '0');
    }
    return v;
}

static const char *mon_names[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
static const char *day_names[] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

static double parse_date(jsstr *str) {
    const char *s = str->d;
    int len = (int)str->len, i = 0;
    while (i < len && s[i] == ' ') i++;
    double nan = jv_n(JS_NAN_BITS);
    if (i < len && isdigit((u8)s[i])) {
        int st = i;
        int y = pnum(s, len, &i, 4);
        if (y < 0) return nan;
        if (i < len && (s[i] == '-' || s[i] == '/')) {
            char sep = s[i];
            i++;
            int mo = pnum(s, len, &i, 2), d = 1;
            if (mo < 0) { i = st; goto other; }
            if (i < len && s[i] == sep) { i++; d = pnum(s, len, &i, 2); if (d < 0) return nan; }
            int h = 0, mi = 0, se = 0, ms = 0;
            bool has_time = false;
            double off = 0;
            if (i < len && (s[i] == 'T' || s[i] == ' ')) {
                i++;
                has_time = true;
                h = pnum(s, len, &i, 2);
                if (h < 0 || i >= len || s[i] != ':') return nan;
                i++;
                mi = pnum(s, len, &i, 2);
                if (mi < 0) return nan;
                if (i < len && s[i] == ':') {
                    i++;
                    se = pnum(s, len, &i, 2);
                    if (se < 0) return nan;
                    if (i < len && s[i] == '.') {
                        i++;
                        int digs = 0;
                        ms = 0;
                        while (i < len && isdigit((u8)s[i])) { if (digs < 3) { ms = ms * 10 + (s[i] - '0'); digs++; } i++; }
                        while (digs < 3) { ms *= 10; digs++; }
                    }
                }
                if (i < len && s[i] == 'Z') i++;
                else if (i < len && (s[i] == '+' || s[i] == '-')) {
                    int sg = s[i] == '-' ? -1 : 1;
                    i++;
                    int oh = pnum(s, len, &i, 2);
                    if (i < len && s[i] == ':') i++;
                    int om = pnum(s, len, &i, 2);
                    if (oh < 0 || om < 0) return nan;
                    off = sg * (oh * 60 + om) * 60000.0;
                }
            }
            (void)has_time;
            if (mo < 1 || mo > 12 || d < 1 || d > 31 || h > 24 || mi > 59 || se > 59) return nan;
            return make_date(y, mo - 1, d, h, mi, se, ms) - off;
        }
        i = st;
    }
other:
    /* "Nov 6 2026 10:00:00", "Fri Nov 06 2026", "6 Nov 2026" */
    {
        int mon = -1, d = -1, y = -1, h = 0, mi = 0, se = 0;
        while (i < len) {
            if (isalpha((u8)s[i])) {
                int st = i;
                while (i < len && isalpha((u8)s[i])) i++;
                if (i - st >= 3) for (int k = 0; k < 12; k++) if (!strncasecmp(s + st, mon_names[k], 3)) mon = k;
            } else if (isdigit((u8)s[i])) {
                int st = i, v = 0;
                while (i < len && isdigit((u8)s[i])) v = v * 10 + (s[i++] - '0');
                if (i < len && s[i] == ':') {
                    h = v; i++;
                    mi = pnum(s, len, &i, 2);
                    if (i < len && s[i] == ':') { i++; se = pnum(s, len, &i, 2); }
                    if (mi < 0 || se < 0) return nan;
                } else if (i - st >= 3) y = v;
                else if (d < 0) d = v;
                else y = v;
            } else i++;
        }
        if (mon < 0 || d < 0 || y < 0) return nan;
        return make_date(y, mon, d, h, mi, se, 0);
    }
}

static double this_date(jsv thisv, bool *ok) {
    if (jv_iscls(thisv, CL_DATE)) { *ok = true; return jv_o(thisv)->u.date; }
    *ok = false;
    js_throw_err(E_TYPE, "Este objeto não é uma Date");
    return 0;
}

static jsstr *date_string(double t, int kind) {
    if (t != t) return js_strz("Invalid Date");
    dparts p;
    split_date(t, &p);
    char b[96];
    switch (kind) {
    case 0: snprintf(b, sizeof b, "%s %s %02d %04d %02d:%02d:%02d GMT+0000 (Horário Universal)", day_names[p.wd], mon_names[p.mon], p.d, p.y, p.h, p.mi, p.s); break;
    case 1: snprintf(b, sizeof b, "%04d-%02d-%02dT%02d:%02d:%02d.%03dZ", p.y, p.mon + 1, p.d, p.h, p.mi, p.s, p.ms); break;
    case 2: snprintf(b, sizeof b, "%s %s %02d %04d", day_names[p.wd], mon_names[p.mon], p.d, p.y); break;
    case 3: snprintf(b, sizeof b, "%02d:%02d:%02d GMT+0000", p.h, p.mi, p.s); break;
    case 4: snprintf(b, sizeof b, "%02d/%02d/%04d", p.d, p.mon + 1, p.y); break;
    case 5: snprintf(b, sizeof b, "%02d:%02d:%02d", p.h, p.mi, p.s); break;
    case 6: snprintf(b, sizeof b, "%02d/%02d/%04d, %02d:%02d:%02d", p.d, p.mon + 1, p.y, p.h, p.mi, p.s); break;
    default: snprintf(b, sizeof b, "%s, %02d %s %04d %02d:%02d:%02d GMT", day_names[p.wd], p.d, mon_names[p.mon], p.y, p.h, p.mi, p.s); break;
    }
    return js_strz(b);
}

NATIVE(date_ctor) {
    UNUSED_ARGS;
    if (!(flags & NF_NEW)) return jv_str(date_string(js_date_now(), 0));
    double t;
    if (argc == 0) t = js_date_now();
    else if (argc == 1) {
        jsv v = argv[0];
        if (jv_iscls(v, CL_DATE)) t = jv_o(v)->u.date;
        else {
            jsv p = js_toprim(v, 0);
            TRYV(p);
            if (jv_isstr(p)) t = parse_date(jv_s(p));
            else { t = js_tonum(p); if (js_pending()) return JS_EXC; if (m_abs(t) > 8.64e15) t = jv_n(JS_NAN_BITS); else t = m_trunc(t); }
        }
    } else {
        double f[7] = {0, 0, 1, 0, 0, 0, 0};
        for (int i = 0; i < 7 && i < argc; i++) { f[i] = js_tonum(argv[i]); if (js_pending()) return JS_EXC; }
        if (f[0] >= 0 && f[0] <= 99 && f[0] == m_trunc(f[0])) f[0] += 1900;
        t = make_date(f[0], f[1], f[2], f[3], f[4], f[5], f[6]);
    }
    jsobj *o = jv_o(thisv);
    o->cls = CL_DATE;
    o->u.date = t;
    return thisv;
}

NATIVE(date_now) { UNUSED_ARGS; return jv_num(m_floor(js_date_now())); }

NATIVE(date_parse) {
    UNUSED_ARGS;
    jsstr *s = js_tostr(ARG(0));
    if (!s) return JS_EXC;
    return jv_num(parse_date(s));
}

NATIVE(date_utc) {
    UNUSED_ARGS;
    double f[7] = {0, 0, 1, 0, 0, 0, 0};
    for (int i = 0; i < 7 && i < argc; i++) { f[i] = js_tonum(argv[i]); if (js_pending()) return JS_EXC; }
    if (f[0] >= 0 && f[0] <= 99 && f[0] == m_trunc(f[0])) f[0] += 1900;
    return jv_num(make_date(f[0], f[1], f[2], f[3], f[4], f[5], f[6]));
}

NATIVE(datep_get) {
    UNUSED_ARGS;
    bool ok;
    double t = this_date(thisv, &ok);
    if (!ok) return JS_EXC;
    int k = self->u.nat.magic;
    if (k == 0) return jv_num(t);
    if (t != t) return JS_NAN_BITS;
    if (k == 9) return jv_int(0);
    dparts p;
    split_date(t, &p);
    switch (k) {
    case 1: return jv_int(p.y);
    case 2: return jv_int(p.mon);
    case 3: return jv_int(p.d);
    case 4: return jv_int(p.wd);
    case 5: return jv_int(p.h);
    case 6: return jv_int(p.mi);
    case 7: return jv_int(p.s);
    case 8: return jv_int(p.ms);
    case 10: return jv_int(p.y - 1900);
    }
    return JS_UNDEF;
}

NATIVE(datep_set) {
    UNUSED_ARGS;
    bool ok;
    double t = this_date(thisv, &ok);
    if (!ok) return JS_EXC;
    int k = self->u.nat.magic;
    double a[4];
    for (int i = 0; i < 4; i++) { a[i] = i < argc ? js_tonum(argv[i]) : jv_n(JS_NAN_BITS); if (js_pending()) return JS_EXC; }
    if (k == 0) { t = m_abs(a[0]) <= 8.64e15 ? m_trunc(a[0]) : jv_n(JS_NAN_BITS); jv_o(thisv)->u.date = t; return jv_num(t); }
    if (t != t) { if (k != 1) return JS_NAN_BITS; t = 0; }
    dparts p;
    split_date(t, &p);
    double y = p.y, mo = p.mon, d = p.d, h = p.h, mi = p.mi, s = p.s, ms = p.ms;
    switch (k) {
    case 1: y = a[0]; if (argc > 1) mo = a[1]; if (argc > 2) d = a[2]; break;
    case 2: mo = a[0]; if (argc > 1) d = a[1]; break;
    case 3: d = a[0]; break;
    case 5: h = a[0]; if (argc > 1) mi = a[1]; if (argc > 2) s = a[2]; if (argc > 3) ms = a[3]; break;
    case 6: mi = a[0]; if (argc > 1) s = a[1]; if (argc > 2) ms = a[2]; break;
    case 7: s = a[0]; if (argc > 1) ms = a[1]; break;
    case 8: ms = a[0]; break;
    }
    t = make_date(y, mo, d, h, mi, s, ms);
    jv_o(thisv)->u.date = t;
    return jv_num(t);
}

NATIVE(datep_str) {
    UNUSED_ARGS;
    bool ok;
    double t = this_date(thisv, &ok);
    if (!ok) return JS_EXC;
    int k = self->u.nat.magic;
    if (k == 1 && t != t) return js_throw_err(E_RANGE, "Data inválida");
    if (k == 8) { if (t != t) return JS_NULL; k = 1; }
    return jv_str(date_string(t, k));
}

NATIVE(datep_toprim) {
    UNUSED_ARGS;
    jsv h = ARG(0);
    int hint = jv_isstr(h) && !strcmp(jv_s(h)->d, "number") ? 1 : 2;
    jsstr *order[2] = {hint == 2 ? A[A_toString] : A[A_valueOf], hint == 2 ? A[A_valueOf] : A[A_toString]};
    for (int i = 0; i < 2; i++) {
        jsv f = js_getk(thisv, order[i]);
        TRYV(f);
        if (js_iscallable(f)) { jsv r = js_call(f, thisv, 0, NULL); TRYV(r); if (!jv_isobj(r)) return r; }
    }
    return js_throw_err(E_TYPE, "Não é possível converter a data em primitivo");
}

/* ============================================================ RegExp */
static jsobj *make_regexp(rxprog *prog, jsstr *src, jsstr *flags) {
    jsobj *o = js_newobj_cls(CL_REGEXP, J.realm->p_regexp);
    o->u.re.prog = prog;
    o->u.re.src = src;
    o->u.re.flags = flags;
    js_defprop(o, A[A_lastIndex], jv_int(0), PF_NOENUM);
    return o;
}

jsv js_regexp_from_prog(rxprog *p, jsstr *src, jsstr *flags) {
    rx_ref(p);
    return jv_obj(make_regexp(p, src, flags));
}

static int parse_flags(jsstr *f, int *out) {
    int fl = 0;
    for (u32 i = 0; i < f->len; i++) {
        int b;
        switch (f->d[i]) {
        case 'g': b = RXF_G; break;
        case 'i': b = RXF_I; break;
        case 'm': b = RXF_M; break;
        case 's': b = RXF_S; break;
        case 'y': b = RXF_Y; break;
        case 'u': b = RXF_U; break;
        case 'd': b = 0; break;
        default: return -1;
        }
        if (fl & b) return -1;
        fl |= b;
    }
    *out = fl;
    return 0;
}

jsv js_regexp_new(jsstr *src, jsstr *flags) {
    int fl;
    if (parse_flags(flags, &fl) < 0) return js_throw_err(E_SYNTAX, "Flags inválidas na expressão regular: '%s'", flags->d);
    char err[120];
    rxprog *p = rx_compile(src->d, (int)src->len, fl, err, sizeof err);
    if (!p) return js_throw_err(E_SYNTAX, "Expressão regular inválida: /%s/: %s", src->d, err);
    return jv_obj(make_regexp(p, src, flags));
}

NATIVE(re_ctor) {
    UNUSED_ARGS;
    jsv pat = ARG(0), fl = ARG(1);
    jsstr *src, *fls;
    if (jv_iscls(pat, CL_REGEXP)) {
        if (fl == JS_UNDEF && !(flags & NF_NEW)) return pat;
        src = jv_o(pat)->u.re.src;
        fls = fl == JS_UNDEF ? jv_o(pat)->u.re.flags : js_tostr(fl);
        if (!fls) return JS_EXC;
    } else {
        src = pat == JS_UNDEF ? A[A_empty] : js_tostr(pat);
        if (!src) return JS_EXC;
        fls = fl == JS_UNDEF ? A[A_empty] : js_tostr(fl);
        if (!fls) return JS_EXC;
    }
    jsv r = js_regexp_new(src, fls);
    TRYV(r);
    if (flags & NF_NEW) jv_o(r)->proto = jv_o(thisv)->proto;
    return r;
}
static jsobj *this_re(jsv thisv) {
    if (jv_iscls(thisv, CL_REGEXP)) return jv_o(thisv);
    js_throw_err(E_TYPE, "Este objeto não é uma expressão regular");
    return NULL;
}

static int get_lastindex(jsobj *re, jsstr *s, int *byteoff) {
    jsv li = js_getk(jv_obj(re), A[A_lastIndex]);
    if (li == JS_EXC) return -1;
    double d = js_tointeger(li);
    if (js_pending()) return -1;
    if (d < 0) d = 0;
    if (d > s->ulen) { *byteoff = -1; return 0; }
    *byteoff = js_str_boff(s, (int)d);
    return 0;
}

static int set_lastindex(jsobj *re, jsstr *s, int byteoff) {
    return js_setk(jv_obj(re), A[A_lastIndex], jv_int(byteoff <= 0 ? 0 : js_str_uidx(s, byteoff)));
}

/* executa a partir de start (bytes); caps deve ter 2*ngroups; 1/0, -1 exceção */
static int run_re(jsobj *re, jsstr *s, int start, int *caps, bool sticky) {
    int r = rx_exec(re->u.re.prog, s->d, (int)s->len, start, caps, sticky);
    if (r < 0) { js_throw_err(E_RANGE, "Expressão regular complexa demais (limite de passos excedido)"); return -1; }
    return r;
}

static jsv match_result(jsobj *re, jsstr *s, int *caps) {
    rxprog *p = re->u.re.prog;
    int ng = rx_ngroups(p);
    jsobj *a = js_newarr(0);
    for (int g = 0; g < ng; g++) {
        if (caps[g * 2] < 0 || caps[g * 2 + 1] < 0) js_arr_push(a, JS_UNDEF);
        else js_arr_push(a, jv_str(js_strn(s->d + caps[g * 2], caps[g * 2 + 1] - caps[g * 2])));
    }
    js_defprop(a, A[A_index], jv_int(js_str_uidx(s, caps[0])), 0);
    js_defprop(a, A[A_input], jv_str(s), 0);
    jsv groups = JS_UNDEF;
    for (int g = 1; g < ng; g++) {
        const char *nm = rx_group_name(p, g);
        if (!nm) continue;
        if (groups == JS_UNDEF) groups = jv_obj(js_newobj(NULL));
        js_defprop(jv_o(groups), js_strz(nm), js_arr_get(a, (u32)g), 0);
    }
    js_defprop(a, A[A_groups], groups, 0);
    return jv_obj(a);
}

static jsv re_exec_impl(jsobj *re, jsstr *s, bool want_result) {
    int fl = rx_flags(re->u.re.prog);
    bool gy = (fl & (RXF_G | RXF_Y)) != 0;
    int start = 0;
    if (gy) {
        if (get_lastindex(re, s, &start) < 0) return JS_EXC;
        if (start < 0) { if (set_lastindex(re, s, 0) < 0) return JS_EXC; return JS_NULL; }
    }
    int ng = rx_ngroups(re->u.re.prog);
    int cbuf[40];
    int *caps = ng * 2 <= 40 ? cbuf : kmalloc(sizeof(int) * (usize)ng * 2);
    if (!caps) return JS_EXC;
    int r = run_re(re, s, start, caps, (fl & RXF_Y) != 0);
    jsv res;
    if (r < 0) res = JS_EXC;
    else if (r == 0) {
        res = JS_NULL;
        if (gy && set_lastindex(re, s, 0) < 0) res = JS_EXC;
    } else {
        if (gy && set_lastindex(re, s, caps[1]) < 0) res = JS_EXC;
        else res = want_result ? match_result(re, s, caps) : JS_TRUE;
    }
    if (caps != cbuf) kfree(caps);
    return res;
}

NATIVE(rep_exec) {
    UNUSED_ARGS;
    jsobj *re = this_re(thisv);
    if (!re) return JS_EXC;
    jsstr *s = js_tostr(ARG(0));
    if (!s) return JS_EXC;
    return re_exec_impl(re, s, self->u.nat.magic == 0);
}

NATIVE(rep_test) {
    UNUSED_ARGS;
    jsobj *re = this_re(thisv);
    if (!re) return JS_EXC;
    jsstr *s = js_tostr(ARG(0));
    if (!s) return JS_EXC;
    jsv r = re_exec_impl(re, s, false);
    TRYV(r);
    return jv_bool(r == JS_TRUE);
}

NATIVE(rep_tostring) {
    UNUSED_ARGS;
    jsobj *re = this_re(thisv);
    if (!re) return JS_EXC;
    return jv_str(js_str_fmt("/%s/%s", re->u.re.src->len ? re->u.re.src->d : "(?:)", re->u.re.flags->d));
}

NATIVE(rep_getter) {
    UNUSED_ARGS;
    if (thisv == jv_obj(J.realm->p_regexp)) return self->u.nat.magic == 0 ? jv_str(js_strz("(?:)")) : JS_UNDEF;
    jsobj *re = this_re(thisv);
    if (!re) return JS_EXC;
    int fl = rx_flags(re->u.re.prog);
    switch (self->u.nat.magic) {
    case 0: return jv_str(re->u.re.src->len ? re->u.re.src : js_strz("(?:)"));
    case 1: {
        char b[8];
        int k = 0;
        if (fl & RXF_G) b[k++] = 'g';
        if (fl & RXF_I) b[k++] = 'i';
        if (fl & RXF_M) b[k++] = 'm';
        if (fl & RXF_S) b[k++] = 's';
        if (fl & RXF_U) b[k++] = 'u';
        if (fl & RXF_Y) b[k++] = 'y';
        return jv_str(js_strn(b, k));
    }
    case 2: return jv_bool(fl & RXF_G);
    case 3: return jv_bool(fl & RXF_I);
    case 4: return jv_bool(fl & RXF_M);
    case 5: return jv_bool(fl & RXF_S);
    case 6: return jv_bool(fl & RXF_Y);
    case 7: return jv_bool(fl & RXF_U);
    }
    return JS_UNDEF;
}

static int next_cp(jsstr *s, int i) {
    if (i >= (int)s->len) return i + 1;
    js_utf8_get(s->d, (int)s->len, &i);
    return i;
}

jsv js_regexp_split(jsobj *re, jsstr *s, u32 limit) {
    jsobj *out = js_newarr(0);
    if (!limit) return jv_obj(out);
    int ng = rx_ngroups(re->u.re.prog);
    int cbuf[40];
    int *caps = ng * 2 <= 40 ? cbuf : kmalloc(sizeof(int) * (usize)ng * 2);
    if (!caps) return JS_EXC;
    jsv res = jv_obj(out);
    int size = (int)s->len;
    if (size == 0) {
        int r = run_re(re, s, 0, caps, true);
        if (r < 0) res = JS_EXC;
        else if (!r) js_arr_push(out, jv_str(s));
        goto done;
    }
    int p = 0, q = 0;
    while (q < size) {
        int r = run_re(re, s, q, caps, true);
        if (r < 0) { res = JS_EXC; goto done; }
        if (!r) { q = next_cp(s, q); continue; }
        int e = caps[1];
        if (e == p || e > size) { q = next_cp(s, q); continue; }
        js_arr_push(out, jv_str(js_strn(s->d + p, q - p)));
        if (out->alen >= limit) goto done;
        for (int g = 1; g < ng; g++) {
            js_arr_push(out, caps[g * 2] < 0 ? JS_UNDEF : jv_str(js_strn(s->d + caps[g * 2], caps[g * 2 + 1] - caps[g * 2])));
            if (out->alen >= limit) goto done;
        }
        p = e;
        q = p;
    }
    js_arr_push(out, jv_str(js_strn(s->d + p, size - p)));
done:
    if (caps != cbuf) kfree(caps);
    return res;
}

jsv js_regexp_replace(jsobj *re, jsstr *s, jsv rep) {
    int fl = rx_flags(re->u.re.prog);
    bool global = (fl & RXF_G) != 0;
    jsstr *reps = NULL;
    if (!js_iscallable(rep)) { reps = js_tostr(rep); if (!reps) return JS_EXC; }
    int ng = rx_ngroups(re->u.re.prog);
    int cbuf[40];
    int *caps = ng * 2 <= 40 ? cbuf : kmalloc(sizeof(int) * (usize)ng * 2);
    if (!caps) return JS_EXC;
    sbuf_t sb = {0};
    int last = 0, pos = 0;
    jsv res = JS_UNDEF;
    if (!global && (fl & RXF_Y)) {
        if (get_lastindex(re, s, &pos) < 0) { res = JS_EXC; goto out; }
        if (pos < 0) { set_lastindex(re, s, 0); res = jv_str(s); goto out; }
    }
    while (pos <= (int)s->len) {
        int r = run_re(re, s, pos, caps, (fl & RXF_Y) != 0);
        if (r < 0) { res = JS_EXC; goto out; }
        if (!r) break;
        int ms = caps[0], me = caps[1];
        sb_putn(&sb, s->d + last, ms - last);
        if (reps) {
            jsobj *groups = NULL;
            if (strstr(reps->d, "$<")) {
                jsv mr = match_result(re, s, caps);
                jsv gv = js_getk(mr, A[A_groups]);
                if (jv_isobj(gv)) groups = jv_o(gv);
            }
            jsstr *x = js_expand_replacement(reps, s, ms, me, caps, ng, groups);
            sb_putn(&sb, x->d, (int)x->len);
        } else {
            jsobj *args = js_newarr(0);
            for (int g = 0; g < ng; g++)
                js_arr_push(args, caps[g * 2] < 0 ? JS_UNDEF : jv_str(js_strn(s->d + caps[g * 2], caps[g * 2 + 1] - caps[g * 2])));
            js_arr_push(args, jv_int(js_str_uidx(s, ms)));
            js_arr_push(args, jv_str(s));
            bool named = false;
            for (int g = 1; g < ng; g++) if (rx_group_name(re->u.re.prog, g)) named = true;
            if (named) { jsv mr = match_result(re, s, caps); js_arr_push(args, js_getk(mr, A[A_groups])); }
            jsv rv = js_call(rep, JS_UNDEF, (int)args->alen, args->arr);
            if (rv == JS_EXC) { res = JS_EXC; goto out; }
            jsstr *rs = js_tostr(rv);
            if (!rs) { res = JS_EXC; goto out; }
            sb_putn(&sb, rs->d, (int)rs->len);
        }
        last = me;
        if (!global) { if (fl & RXF_Y) set_lastindex(re, s, me); break; }
        pos = me == ms ? next_cp(s, me) : me;
    }
    if (global) set_lastindex(re, s, 0);
    if (last < (int)s->len) sb_putn(&sb, s->d + last, (int)s->len - last);
    res = jv_str(js_strn(sb.p ? sb.p : "", sb.len));
out:
    sb_free(&sb);
    if (caps != cbuf) kfree(caps);
    return res;
}

jsv js_regexp_match(jsobj *re, jsstr *s) {
    int fl = rx_flags(re->u.re.prog);
    if (!(fl & RXF_G)) return re_exec_impl(re, s, true);
    int ng = rx_ngroups(re->u.re.prog);
    int cbuf[40];
    int *caps = ng * 2 <= 40 ? cbuf : kmalloc(sizeof(int) * (usize)ng * 2);
    if (!caps) return JS_EXC;
    jsobj *out = js_newarr(0);
    jsv res = jv_obj(out);
    int pos = 0;
    while (pos <= (int)s->len) {
        int r = run_re(re, s, pos, caps, (fl & RXF_Y) != 0);
        if (r < 0) { res = JS_EXC; break; }
        if (!r) break;
        js_arr_push(out, jv_str(js_strn(s->d + caps[0], caps[1] - caps[0])));
        pos = caps[1] == caps[0] ? next_cp(s, caps[1]) : caps[1];
    }
    if (caps != cbuf) kfree(caps);
    if (res != JS_EXC) { if (set_lastindex(re, s, 0) < 0) return JS_EXC; }
    if (res != JS_EXC && !out->alen) return JS_NULL;
    return res;
}

jsv js_regexp_matchall(jsobj *re, jsstr *s) {
    int ng = rx_ngroups(re->u.re.prog);
    int cbuf[40];
    int *caps = ng * 2 <= 40 ? cbuf : kmalloc(sizeof(int) * (usize)ng * 2);
    if (!caps) return JS_EXC;
    jsobj *out = js_newarr(0);
    int pos = 0;
    jsv res = jv_obj(out);
    while (pos <= (int)s->len) {
        int r = run_re(re, s, pos, caps, false);
        if (r < 0) { res = JS_EXC; break; }
        if (!r) break;
        js_arr_push(out, match_result(re, s, caps));
        pos = caps[1] == caps[0] ? next_cp(s, caps[1]) : caps[1];
    }
    if (caps != cbuf) kfree(caps);
    if (res == JS_EXC) return res;
    return js_invoke(res, js_intern("values", -1), 0, NULL);
}

jsv js_regexp_search(jsobj *re, jsstr *s) {
    int ng = rx_ngroups(re->u.re.prog);
    int cbuf[40];
    int *caps = ng * 2 <= 40 ? cbuf : kmalloc(sizeof(int) * (usize)ng * 2);
    if (!caps) return JS_EXC;
    int r = run_re(re, s, 0, caps, false);
    jsv res = r < 0 ? JS_EXC : r == 0 ? jv_int(-1) : jv_int(js_str_uidx(s, caps[0]));
    if (caps != cbuf) kfree(caps);
    return res;
}

/* ============================================================ Promise */
enum { JOB_CALL, JOB_REACTION, JOB_THENABLE };
enum { PS_PENDING, PS_FULFILLED, PS_REJECTED };

static jsobj *unhandled;   /* promessas rejeitadas sem tratador (verificadas após as microtarefas) */

void js_enqueue_job(jsv fn, jsv arg0, jsv arg1, int kind) {
    jsobj *q = J.realm->jobs;
    js_arr_push(q, fn);
    js_arr_push(q, arg0);
    js_arr_push(q, arg1);
    js_arr_push(q, jv_int(kind));
}

jsobj *js_promise_new(void) {
    jsobj *p = js_newobj_cls(CL_PROMISE, J.realm->p_promise);
    p->u.prom.state = PS_PENDING;
    p->u.prom.value = JS_UNDEF;
    p->u.prom.reactions = NULL;
    return p;
}

static void settle(jsobj *p, int state, jsv v) {
    if (p->u.prom.state != PS_PENDING) return;
    p->u.prom.state = state;
    p->u.prom.value = v;
    jsobj *rs = p->u.prom.reactions;
    p->u.prom.reactions = NULL;
    if (rs) for (u32 i = 0; i < rs->alen; i++) js_enqueue_job(rs->arr[i], jv_obj(p), JS_UNDEF, JOB_REACTION);
    if (state == PS_REJECTED && !p->u.prom.handled) {
        if (!unhandled) unhandled = js_newarr(0);
        js_arr_push(unhandled, jv_obj(p));
    }
}

void js_promise_reject(jsobj *p, jsv v) { settle(p, PS_REJECTED, v); }

static jsv make_resolver(jsobj *p, jsobj *flag, bool reject);

void js_promise_resolve(jsobj *p, jsv v) {
    if (p->u.prom.state != PS_PENDING) return;
    if (v == jv_obj(p)) { js_promise_reject(p, jv_obj(js_make_err(E_TYPE, js_strz("Uma promessa não pode ser resolvida com ela mesma")))); return; }
    if (jv_isobj(v)) {
        jsv then = js_getk(v, A[A_then]);
        if (then == JS_EXC) { js_promise_reject(p, js_catch()); return; }
        if (js_iscallable(then)) { js_enqueue_job(then, v, jv_obj(p), JOB_THENABLE); return; }
    }
    settle(p, PS_FULFILLED, v);
}

NATIVE(prom_resolver) {
    UNUSED_ARGS;
    jsobj *p = jv_o(self->u.nat.data);
    jsobj *flag = jv_o(self->u.nat.data2);
    if (flag->noext) return JS_UNDEF;
    flag->noext = 1;
    if (self->u.nat.magic) js_promise_reject(p, ARG(0));
    else js_promise_resolve(p, ARG(0));
    return JS_UNDEF;
}

static jsv make_resolver(jsobj *p, jsobj *flag, bool reject) {
    jsobj *f = js_newnative(prom_resolver, reject ? "reject" : "resolve", 1, reject);
    f->u.nat.data = jv_obj(p);
    f->u.nat.data2 = jv_obj(flag);
    return jv_obj(f);
}

NATIVE(prom_ctor) {
    UNUSED_ARGS;
    if (!(flags & NF_NEW)) return js_throw_err(E_TYPE, "O construtor Promise exige 'new'");
    jsv ex = ARG(0);
    if (!js_iscallable(ex)) return js_throw_err(E_TYPE, "O executor da Promise não é uma função");
    jsobj *p = jv_o(thisv);
    p->cls = CL_PROMISE;
    p->u.prom.state = PS_PENDING;
    p->u.prom.value = JS_UNDEF;
    p->u.prom.reactions = NULL;
    jsobj *flag = js_newobj(NULL);
    jsv args[2] = {make_resolver(p, flag, false), make_resolver(p, flag, true)};
    jsv r = js_call(ex, JS_UNDEF, 2, args);
    if (r == JS_EXC) {
        if (J.interrupted) return JS_EXC;
        jsv e = js_catch();
        if (!flag->noext) { flag->noext = 1; js_promise_reject(p, e); }
    }
    return thisv;
}

jsobj *js_promise_then(jsobj *p, jsv onf, jsv onr) {
    jsobj *derived = js_promise_new();
    jsv rec[3] = {js_iscallable(onf) ? onf : JS_UNDEF, js_iscallable(onr) ? onr : JS_UNDEF, jv_obj(derived)};
    jsobj *reaction = js_arr_from(rec, 3);
    p->u.prom.handled = true;
    if (p->u.prom.state == PS_PENDING) {
        if (!p->u.prom.reactions) p->u.prom.reactions = js_newarr(0);
        js_arr_push(p->u.prom.reactions, jv_obj(reaction));
    } else js_enqueue_job(jv_obj(reaction), jv_obj(p), JS_UNDEF, JOB_REACTION);
    return derived;
}

NATIVE(promp_then) {
    UNUSED_ARGS;
    if (!jv_iscls(thisv, CL_PROMISE)) return js_throw_err(E_TYPE, "Promise.prototype.then chamado em algo que não é Promise");
    jsv onf = self->u.nat.magic == 1 ? JS_UNDEF : ARG(0);
    jsv onr = self->u.nat.magic == 1 ? ARG(0) : ARG(1);
    return jv_obj(js_promise_then(jv_o(thisv), onf, onr));
}

NATIVE(prom_finally_cb) {
    UNUSED_ARGS;
    jsv fn = self->u.nat.data;
    if (js_iscallable(fn)) { jsv r = js_call(fn, JS_UNDEF, 0, NULL); TRYV(r); }
    if (self->u.nat.magic) return js_throw(ARG(0));
    return ARG(0);
}

NATIVE(promp_finally) {
    UNUSED_ARGS;
    if (!jv_iscls(thisv, CL_PROMISE)) return js_throw_err(E_TYPE, "Promise.prototype.finally chamado em algo que não é Promise");
    jsobj *a = js_newnative(prom_finally_cb, "", 1, 0), *b = js_newnative(prom_finally_cb, "", 1, 1);
    a->u.nat.data = ARG(0);
    b->u.nat.data = ARG(0);
    return jv_obj(js_promise_then(jv_o(thisv), jv_obj(a), jv_obj(b)));
}

jsobj *js_promise_from(jsv v) {
    if (jv_iscls(v, CL_PROMISE)) return jv_o(v);
    jsobj *p = js_promise_new();
    js_promise_resolve(p, v);
    return p;
}

NATIVE(prom_resolve_s) {
    UNUSED_ARGS;
    if (self->u.nat.magic) { jsobj *p = js_promise_new(); js_promise_reject(p, ARG(0)); return jv_obj(p); }
    return jv_obj(js_promise_from(ARG(0)));
}

/* Promise.all / allSettled / race / any: elemento resolvido */
NATIVE(prom_all_elem) {
    UNUSED_ARGS;
    jsobj *st = jv_o(self->u.nat.data);   /* [resultados, restantes, promessa, modo] */
    int idx = self->u.nat.magic;
    jsobj *results = jv_o(st->arr[0]);
    jsobj *p = jv_o(st->arr[2]);
    int mode = (int)jv_n(st->arr[3]);
    bool rejected_cb = jv_n(self->u.nat.data2) != 0;
    if (p->u.prom.state != PS_PENDING) return JS_UNDEF;
    jsv v = ARG(0);
    if (mode == 1) {
        jsobj *o = js_newobj(J.realm->p_object);
        js_defprop(o, A[A_status], jv_str(rejected_cb ? A[A_rejected] : A[A_fulfilled]), 0);
        js_defprop(o, rejected_cb ? A[A_reason] : A[A_value], v, 0);
        v = jv_obj(o);
    }
    js_arr_set(results, (u32)idx, v);
    double rem = jv_n(st->arr[1]) - 1;
    st->arr[1] = jv_num(rem);
    if (rem == 0) {
        if (mode == 3) {
            jsobj *e = js_make_err(E_ERROR, js_strz("Todas as promessas foram rejeitadas"));
            js_defprop(e, A[A_errors], jv_obj(results), 0);
            js_promise_reject(p, jv_obj(e));
        } else js_promise_resolve(p, jv_obj(results));
    }
    return JS_UNDEF;
}

NATIVE(prom_settle_cb) {
    UNUSED_ARGS;
    jsobj *p = jv_o(self->u.nat.data);
    if (self->u.nat.magic) js_promise_reject(p, ARG(0));
    else js_promise_resolve(p, ARG(0));
    return JS_UNDEF;
}

NATIVE(prom_combinator) {
    UNUSED_ARGS;
    int mode = self->u.nat.magic;   /* 0 all, 1 allSettled, 2 race, 3 any */
    jsobj *p = js_promise_new();
    jsv it = js_iter_open(ARG(0));
    if (it == JS_EXC) { js_promise_reject(p, js_catch()); return jv_obj(p); }
    jsobj *results = js_newarr(0);
    jsv stv[4] = {jv_obj(results), jv_num(1), jv_obj(p), jv_num(mode)};
    jsobj *st = js_arr_from(stv, 4);
    int idx = 0;
    for (;;) {
        jsv v;
        int r = js_iter_step(it, &v);
        if (r < 0) { js_promise_reject(p, js_catch()); return jv_obj(p); }
        if (!r) break;
        jsobj *ep = js_promise_from(v);
        if (mode == 2) {
            jsobj *a = js_newnative(prom_settle_cb, "", 1, 0), *b = js_newnative(prom_settle_cb, "", 1, 1);
            a->u.nat.data = b->u.nat.data = jv_obj(p);
            js_promise_then(ep, jv_obj(a), jv_obj(b));
        } else {
            js_arr_set(results, (u32)idx, JS_UNDEF);
            st->arr[1] = jv_num(jv_n(st->arr[1]) + 1);
            jsobj *ok = js_newnative(prom_all_elem, "", 1, idx), *bad;
            ok->u.nat.data = jv_obj(st);
            ok->u.nat.data2 = jv_num(mode == 3 ? 1 : 0);
            if (mode == 0) { bad = js_newnative(prom_settle_cb, "", 1, 1); bad->u.nat.data = jv_obj(p); }
            else if (mode == 3) {
                /* any: sucesso resolve direto; falha conta */
                jsobj *win = js_newnative(prom_settle_cb, "", 1, 0);
                win->u.nat.data = jv_obj(p);
                bad = ok;
                ok = win;
            } else { bad = js_newnative(prom_all_elem, "", 1, idx); bad->u.nat.data = jv_obj(st); bad->u.nat.data2 = jv_num(1); }
            js_promise_then(ep, jv_obj(ok), jv_obj(bad));
        }
        idx++;
    }
    if (mode != 2) {
        double rem = jv_n(st->arr[1]) - 1;
        st->arr[1] = jv_num(rem);
        if (rem == 0) {
            if (mode == 3) {
                jsobj *e = js_make_err(E_ERROR, js_strz("Todas as promessas foram rejeitadas"));
                js_defprop(e, A[A_errors], jv_obj(results), 0);
                js_promise_reject(p, jv_obj(e));
            } else js_promise_resolve(p, jv_obj(results));
        }
    }
    return jv_obj(p);
}

static void run_job(jsv fn, jsv a0, jsv a1, int kind) {
    switch (kind) {
    case JOB_CALL: {
        jsv r = js_call(fn, JS_UNDEF, a0 == JS_HOLE ? 0 : 1, &a0);
        if (r == JS_EXC) js_report_exception();
        break;
    }
    case JOB_THENABLE: {
        jsobj *p = jv_o(a1);
        jsobj *flag = js_newobj(NULL);
        jsv args[2] = {make_resolver(p, flag, false), make_resolver(p, flag, true)};
        jsv r = js_call(fn, a0, 2, args);
        if (r == JS_EXC) {
            if (J.interrupted) { js_report_exception(); break; }
            jsv e = js_catch();
            if (!flag->noext) { flag->noext = 1; js_promise_reject(p, e); }
        }
        break;
    }
    case JOB_REACTION: {
        jsobj *rec = jv_o(fn);
        jsobj *src = jv_o(a0);
        jsobj *derived = jv_o(rec->arr[2]);
        bool ok = src->u.prom.state == PS_FULFILLED;
        jsv h = ok ? rec->arr[0] : rec->arr[1];
        jsv v = src->u.prom.value;
        if (h == JS_UNDEF) {
            if (ok) js_promise_resolve(derived, v);
            else js_promise_reject(derived, v);
            break;
        }
        jsv r = js_call(h, JS_UNDEF, 1, &v);
        if (r == JS_EXC) {
            if (J.interrupted) { js_report_exception(); break; }
            js_promise_reject(derived, js_catch());
        } else js_promise_resolve(derived, r);
        break;
    }
    }
}

bool js_run_jobs(void) {
    jsrealm *r = J.realm;
    bool any = false;
    for (int guard = 0; guard < 100000; guard++) {
        jsobj *q = r->jobs;
        if (r->jobpos >= q->alen) break;
        u32 i = r->jobpos;
        jsv fn = q->arr[i], a0 = q->arr[i + 1], a1 = q->arr[i + 2];
        int kind = (int)jv_n(q->arr[i + 3]);
        r->jobpos += 4;
        J.interrupted = false;
        js_set_budget(2000);
        run_job(fn, a0, a1, kind);
        if (js_pending()) js_report_exception();
        any = true;
    }
    r->jobs->alen = 0;
    r->jobpos = 0;
    js_report_unhandled();
    return any;
}

void js_report_unhandled(void) {
    if (!unhandled || !unhandled->alen) return;
    jsobj *list = unhandled;
    unhandled = NULL;
    for (u32 i = 0; i < list->alen; i++) {
        jsobj *p = jv_o(list->arr[i]);
        if (p->u.prom.handled) continue;
        jsstr *s = js_exc_string(p->u.prom.value);
        char b[400];
        snprintf(b, sizeof b, "Uncaught (in promise) %s", s ? s->d : "?");
        js_console_log(3, b);
    }
}

NATIVE(g_queuemicrotask) {
    UNUSED_ARGS;
    if (!js_iscallable(ARG(0))) return js_throw_err(E_TYPE, "queueMicrotask: o argumento não é uma função");
    js_enqueue_job(ARG(0), JS_HOLE, JS_UNDEF, JOB_CALL);
    return JS_UNDEF;
}

/* ============================================================ console */
static void inspect_str(sbuf_t *sb, jsstr *s) {
    sb_putc(sb, '\'');
    for (u32 i = 0; i < s->len; i++) {
        char c = s->d[i];
        if (c == '\'') sb_puts(sb, "\\'");
        else if (c == '\n') sb_puts(sb, "\\n");
        else if (c == '\\') sb_puts(sb, "\\\\");
        else sb_putc(sb, c);
    }
    sb_putc(sb, '\'');
}

static jsobj *inspect_seen;

void js_inspect(jsv v, sbuf_t *sb, int depth, bool top) {
    char b[64];
    if (jv_isstr(v)) {
        jsstr *s = jv_s(v);
        if (s->sym) { sb_printf(sb, "Symbol(%s)", s->d); return; }
        if (top) sb_putn(sb, s->d, (int)s->len);
        else inspect_str(sb, s);
        return;
    }
    if (!jv_isobj(v)) {
        if (jv_isnum(v) && jv_n(v) == 0 && 1 / jv_n(v) < 0) { sb_puts(sb, "-0"); return; }
        sb_puts(sb, js_cstr(v, b, sizeof b));
        return;
    }
    jsobj *o = jv_o(v);
    if (js_iscallable(v)) {
        jsprop *p = js_ownprop(o, A[A_name]);
        const char *n = p && jv_isstr(p->v) && jv_s(p->v)->len ? jv_s(p->v)->d : NULL;
        if (o->cls == CL_FUNCTION && o->is_class) sb_printf(sb, "[class %s]", n ? n : "(anônima)");
        else if (n) sb_printf(sb, "[Function: %s]", n);
        else sb_puts(sb, "[Function (anônima)]");
        return;
    }
    switch (o->cls) {
    case CL_ERROR: {
        jsstr *s = js_exc_string(v);
        sb_puts(sb, s ? s->d : "Error");
        return;
    }
    case CL_DATE: { jsstr *s = date_string(o->u.date, 1); sb_puts(sb, s->d); return; }
    case CL_REGEXP: sb_printf(sb, "/%s/%s", o->u.re.src->d, o->u.re.flags->d); return;
    case CL_STRING: sb_puts(sb, "[String: "); inspect_str(sb, jv_s(o->u.prim)); sb_putc(sb, ']'); return;
    case CL_NUMBER: sb_printf(sb, "[Number: %s]", js_cstr(o->u.prim, b, sizeof b)); return;
    case CL_BOOLEAN: sb_printf(sb, "[Boolean: %s]", o->u.prim == JS_TRUE ? "true" : "false"); return;
    case CL_PROMISE:
        sb_printf(sb, "Promise { %s }", o->u.prom.state == PS_PENDING ? "<pendente>" : o->u.prom.state == PS_FULFILLED ? "<resolvida>" : "<rejeitada>");
        return;
    case CL_NODE: {
        node_t *n = o->u.host.node;
        if (n && n->type == N_ELEM) {
            const char *id = node_attr(n, "id"), *cl = node_attr(n, "class");
            sb_printf(sb, "<%s", n->name);
            if (id) sb_printf(sb, " id=\"%s\"", id);
            if (cl) sb_printf(sb, " class=\"%s\"", cl);
            sb_putc(sb, '>');
        } else if (n && n->type == N_TEXT) sb_puts(sb, "#text");
        else sb_puts(sb, "#document");
        return;
    }
    default: break;
    }
    if (!inspect_seen) inspect_seen = js_newarr(0);
    for (u32 i = 0; i < inspect_seen->alen; i++) if (inspect_seen->arr[i] == v) { sb_puts(sb, "[Circular]"); return; }
    if (depth > 2) { sb_puts(sb, o->cls == CL_ARRAY ? "[Array]" : "[Object]"); return; }
    js_arr_push(inspect_seen, v);
    if (o->cls == CL_MAP || o->cls == CL_SET) {
        bool isset = o->cls == CL_SET;
        sb_printf(sb, "%s(%u) {", isset ? "Set" : "Map", o->u.map.live);
        int shown = 0;
        for (u32 i = 0; i < o->u.map.keys->alen && shown < 30; i++) {
            jsv k = o->u.map.keys->arr[i];
            if (k == JS_HOLE) continue;
            sb_puts(sb, shown ? ", " : " ");
            js_inspect(k, sb, depth + 1, false);
            if (!isset) { sb_puts(sb, " => "); js_inspect(o->u.map.vals->arr[i], sb, depth + 1, false); }
            shown++;
        }
        sb_puts(sb, shown ? " }" : "}");
    } else if (o->cls == CL_ARRAY || o->cls == CL_ARGUMENTS) {
        sb_putc(sb, '[');
        u32 n = MIN(o->alen, 100u);
        for (u32 i = 0; i < n; i++) {
            sb_puts(sb, i ? ", " : " ");
            if (o->arr[i] == JS_HOLE) sb_puts(sb, "<vazio>");
            else js_inspect(o->arr[i], sb, depth + 1, false);
        }
        if (o->alen > n) sb_printf(sb, ", ... mais %u itens", o->alen - n);
        sb_puts(sb, o->alen ? " ]" : "]");
    } else {
        jsobj *keys = js_keylist(o, true, false);
        if (o->proto && o->proto != J.realm->p_object) {
            jsprop *cp = js_findprop(o->proto, A[A_constructor], NULL);
            if (cp && js_iscallable(cp->v)) {
                jsprop *np = js_ownprop(jv_o(cp->v), A[A_name]);
                if (np && jv_isstr(np->v) && jv_s(np->v)->len) sb_printf(sb, "%s ", jv_s(np->v)->d);
            }
        }
        sb_putc(sb, '{');
        u32 n = MIN(keys->alen, 50u);
        for (u32 i = 0; i < n; i++) {
            jsstr *k = jv_s(keys->arr[i]);
            sb_puts(sb, i ? ", " : " ");
            sb_putn(sb, k->d, (int)k->len);
            sb_puts(sb, ": ");
            jsprop *p = js_ownprop(o, k);
            if (p && (p->flags & PF_ACCESSOR)) sb_puts(sb, "[Getter/Setter]");
            else if (p) js_inspect(p->v, sb, depth + 1, false);
            else js_inspect(js_get(v, keys->arr[i]), sb, depth + 1, false);
        }
        if (keys->alen > n) sb_printf(sb, ", ... mais %u", keys->alen - n);
        sb_puts(sb, keys->alen ? " }" : "}");
    }
    inspect_seen->alen--;
}

static int console_indent;

static void console_out(int level, int argc, jsv *argv) {
    sbuf_t sb = {0};
    for (int i = 0; i < console_indent && i < 10; i++) sb_puts(&sb, "  ");
    int start = 0;
    if (argc > 0 && jv_isstr(argv[0]) && !jv_s(argv[0])->sym && strchr(jv_s(argv[0])->d, '%')) {
        jsstr *f = jv_s(argv[0]);
        int ai = 1;
        for (u32 i = 0; i < f->len; i++) {
            char c = f->d[i];
            if (c == '%' && i + 1 < f->len) {
                char t = f->d[i + 1];
                if (t == '%') { sb_putc(&sb, '%'); i++; continue; }
                if (strchr("sdifoOc", t) && t) {
                    i++;
                    if (t == 'c') { ai++; continue; }
                    if (ai >= argc) { sb_putc(&sb, '%'); sb_putc(&sb, t); continue; }
                    jsv a = argv[ai++];
                    if (t == 'd' || t == 'i') { double d = js_tonum(a); if (js_pending()) js_catch(); char b[40]; js_numfmt(m_trunc(d), b, sizeof b); sb_puts(&sb, b); }
                    else if (t == 'f') { double d = js_tonum(a); if (js_pending()) js_catch(); char b[40]; js_numfmt(d, b, sizeof b); sb_puts(&sb, b); }
                    else js_inspect(a, &sb, 0, t == 's');
                    continue;
                }
            }
            sb_putc(&sb, c);
        }
        start = ai;
        for (int i = start; i < argc; i++) { sb_putc(&sb, ' '); js_inspect(argv[i], &sb, 0, true); }
    } else {
        for (int i = 0; i < argc; i++) {
            if (i) sb_putc(&sb, ' ');
            js_inspect(argv[i], &sb, 0, true);
        }
    }
    if (js_pending()) js_catch();
    js_console_log(level, sb.p ? sb.p : "");
    sb_free(&sb);
}

NATIVE(con_log) { UNUSED_ARGS; console_out(self->u.nat.magic, argc, argv); return JS_UNDEF; }

NATIVE(con_assert) {
    UNUSED_ARGS;
    if (js_truthy(ARG(0))) return JS_UNDEF;
    if (argc <= 1) { js_console_log(3, "Falha de asserção"); return JS_UNDEF; }
    console_out(3, argc - 1, argv + 1);
    return JS_UNDEF;
}

NATIVE(con_group) {
    UNUSED_ARGS;
    if (self->u.nat.magic == 0) { if (argc) console_out(0, argc, argv); console_indent++; }
    else if (console_indent > 0) console_indent--;
    return JS_UNDEF;
}

static jsobj *con_counts;

NATIVE(con_count) {
    UNUSED_ARGS;
    jsstr *label = ARG(0) == JS_UNDEF ? js_intern("default", -1) : js_tostr(ARG(0));
    if (!label) return JS_EXC;
    if (!con_counts) con_counts = js_newobj(NULL);
    int mode = self->u.nat.magic;   /* 0 count, 1 countReset, 2 time, 3 timeEnd/timeLog */
    jsprop *p = js_ownprop(con_counts, label);
    char b[200];
    if (mode == 0) {
        double n = p ? jv_n(p->v) + 1 : 1;
        js_defprop(con_counts, label, jv_num(n), 0);
        snprintf(b, sizeof b, "%s: %d", label->d, (int)n);
        js_console_log(0, b);
    } else if (mode == 1) js_defprop(con_counts, label, jv_num(0), 0);
    else if (mode == 2) js_defprop(con_counts, js_str_fmt("\x01%s", label->d), jv_num((double)timer_ticks()), 0);
    else {
        jsprop *t = js_ownprop(con_counts, js_str_fmt("\x01%s", label->d));
        if (t) { snprintf(b, sizeof b, "%s: %u ms", label->d, (u32)(timer_ticks() - (u64)jv_n(t->v))); js_console_log(0, b); }
    }
    return JS_UNDEF;
}

NATIVE(con_clear) { UNUSED_ARGS; js_console_log(-1, ""); return JS_UNDEF; }

/* ============================================================ montagem */
void js_lib2_init(jsrealm *r) {
    jsobj *g = r->global, *op = r->p_object;
    jsobj *j = js_newobj(op);
    js_defn(g, "JSON", jv_obj(j), PF_NOENUM);
    js_defnative(j, "parse", json_parse, 2, 0);
    js_defnative(j, "stringify", json_stringify, 3, 0);
    js_defprop(j, A[A_sym_toStringTag], jv_str(js_intern("JSON", -1)), PF_NOENUM | PF_RO);

    /* Date */
    jsobj *dp = js_newobj(op);
    r->p_date = dp;
    jsobj *c = js_newctor(date_ctor, "Date", 7, dp);
    js_defn(g, "Date", jv_obj(c), PF_NOENUM);
    js_defnative(c, "now", date_now, 0, 0);
    js_defnative(c, "parse", date_parse, 1, 0);
    js_defnative(c, "UTC", date_utc, 7, 0);
    static const char *gets[] = {"getTime", "getFullYear", "getMonth", "getDate", "getDay", "getHours", "getMinutes",
                                 "getSeconds", "getMilliseconds", "getTimezoneOffset", "getYear"};
    for (u32 i = 0; i < ARRAY_LEN(gets); i++) js_defnative(dp, gets[i], datep_get, 0, (int)i);
    static const char *ugets[] = {NULL, "getUTCFullYear", "getUTCMonth", "getUTCDate", "getUTCDay", "getUTCHours",
                                  "getUTCMinutes", "getUTCSeconds", "getUTCMilliseconds"};
    for (u32 i = 1; i < ARRAY_LEN(ugets); i++) js_defnative(dp, ugets[i], datep_get, 0, (int)i);
    js_defnative(dp, "valueOf", datep_get, 0, 0);
    static const char *sets[] = {"setTime", "setFullYear", "setMonth", "setDate", NULL, "setHours", "setMinutes", "setSeconds", "setMilliseconds"};
    static const char *usets[] = {NULL, "setUTCFullYear", "setUTCMonth", "setUTCDate", NULL, "setUTCHours", "setUTCMinutes", "setUTCSeconds", "setUTCMilliseconds"};
    for (u32 i = 0; i < ARRAY_LEN(sets); i++) {
        if (sets[i]) js_defnative(dp, sets[i], datep_set, 1, (int)i);
        if (usets[i]) js_defnative(dp, usets[i], datep_set, 1, (int)i);
    }
    js_defnative(dp, "toString", datep_str, 0, 0);
    js_defnative(dp, "toISOString", datep_str, 0, 1);
    js_defnative(dp, "toDateString", datep_str, 0, 2);
    js_defnative(dp, "toTimeString", datep_str, 0, 3);
    js_defnative(dp, "toLocaleDateString", datep_str, 0, 4);
    js_defnative(dp, "toLocaleTimeString", datep_str, 0, 5);
    js_defnative(dp, "toLocaleString", datep_str, 0, 6);
    js_defnative(dp, "toUTCString", datep_str, 0, 7);
    js_defnative(dp, "toGMTString", datep_str, 0, 7);
    js_defnative(dp, "toJSON", datep_str, 0, 8);
    js_defprop(dp, A[A_sym_toPrimitive], jv_obj(js_newnative(datep_toprim, "[Symbol.toPrimitive]", 1, 0)), PF_NOENUM);

    /* RegExp */
    jsobj *rp = js_newobj(op);
    r->p_regexp = rp;
    c = js_newctor(re_ctor, "RegExp", 2, rp);
    r->c_regexp = c;
    js_defn(g, "RegExp", jv_obj(c), PF_NOENUM);
    js_defnative(rp, "exec", rep_exec, 1, 0);
    js_defnative(rp, "test", rep_test, 1, 0);
    js_defnative(rp, "toString", rep_tostring, 0, 0);
    static const char *rg[] = {"source", "flags", "global", "ignoreCase", "multiline", "dotAll", "sticky", "unicode"};
    for (u32 i = 0; i < ARRAY_LEN(rg); i++) js_defgetter(rp, rg[i], rep_getter, (int)i);

    /* Promise */
    jsobj *pp = js_newobj(op);
    r->p_promise = pp;
    c = js_newctor(prom_ctor, "Promise", 1, pp);
    r->c_promise = c;
    js_defn(g, "Promise", jv_obj(c), PF_NOENUM);
    js_defnative(pp, "then", promp_then, 2, 0);
    js_defnative(pp, "catch", promp_then, 1, 1);
    js_defnative(pp, "finally", promp_finally, 1, 0);
    js_defnative(c, "resolve", prom_resolve_s, 1, 0);
    js_defnative(c, "reject", prom_resolve_s, 1, 1);
    js_defnative(c, "all", prom_combinator, 1, 0);
    js_defnative(c, "allSettled", prom_combinator, 1, 1);
    js_defnative(c, "race", prom_combinator, 1, 2);
    js_defnative(c, "any", prom_combinator, 1, 3);
    js_defnative(g, "queueMicrotask", g_queuemicrotask, 1, 0);

    /* console */
    jsobj *con = js_newobj(op);
    js_defn(g, "console", jv_obj(con), PF_NOENUM);
    js_defnative(con, "log", con_log, 0, 0);
    js_defnative(con, "info", con_log, 0, 1);
    js_defnative(con, "debug", con_log, 0, 1);
    js_defnative(con, "warn", con_log, 0, 2);
    js_defnative(con, "error", con_log, 0, 3);
    js_defnative(con, "trace", con_log, 0, 1);
    js_defnative(con, "dir", con_log, 0, 0);
    js_defnative(con, "table", con_log, 0, 0);
    js_defnative(con, "assert", con_assert, 0, 0);
    js_defnative(con, "group", con_group, 0, 0);
    js_defnative(con, "groupCollapsed", con_group, 0, 0);
    js_defnative(con, "groupEnd", con_group, 0, 1);
    js_defnative(con, "count", con_count, 0, 0);
    js_defnative(con, "countReset", con_count, 0, 1);
    js_defnative(con, "time", con_count, 0, 2);
    js_defnative(con, "timeEnd", con_count, 0, 3);
    js_defnative(con, "timeLog", con_count, 0, 3);
    js_defnative(con, "clear", con_clear, 0, 0);
}

/* raízes globais da biblioteca */
void js_lib1_mark(void);
void js_lib_mark(void) {
    js_lib1_mark();
    gc_mark_ptr(unhandled);
    gc_mark_ptr(inspect_seen);
    gc_mark_ptr(con_counts);
}
