/* mOS — utilidades comuns dos apps do sistema (datas, armazenamento em texto, ícones) */
#pragma once
#include "apps.h"
#include "web.h"

/* ------------------------------------------------------------ ícone: quadrado arredondado do sistema */
static inline void kit_tile(surface_t *s, int x, int y, int sz, u32 top, u32 bot) {
    float u = sz / 64.0f;
    gfx_shadow(s, (int)(x + 5 * u), (int)(y + 5 * u), (int)(54 * u), (int)(54 * u), 12 * u, 4 * u, 70, (int)(1.5f * u));
    gfx_round_rect_grad(s, x + 5 * u, y + 5 * u, 54 * u, 54 * u, 12 * u, top, bot);
    gfx_specular_rim(s, x + 5 * u, y + 5 * u, 54 * u, 54 * u, 12 * u, 90);
    gfx_round_rect_stroke(s, x + 5 * u, y + 5 * u, 54 * u, 54 * u, 12 * u, 1, RGBA(0, 0, 0, 40));
}

/* ------------------------------------------------------------ datas */
static const char *const kit_month[12] = {"janeiro", "fevereiro", "março", "abril", "maio", "junho", "julho",
                                          "agosto", "setembro", "outubro", "novembro", "dezembro"};
static const char *const kit_month_cap[12] = {"Janeiro", "Fevereiro", "Março", "Abril", "Maio", "Junho", "Julho",
                                              "Agosto", "Setembro", "Outubro", "Novembro", "Dezembro"};
static const char *const kit_wday[7] = {"domingo", "segunda-feira", "terça-feira", "quarta-feira", "quinta-feira", "sexta-feira", "sábado"};
static const char *const kit_wday_short[7] = {"dom", "seg", "ter", "qua", "qui", "sex", "sáb"};

static inline int kit_dim(int y, int m) {
    static const int d[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    return (m == 2 && ((y % 4 == 0 && y % 100) || y % 400 == 0)) ? 29 : d[(m - 1) % 12];
}
static inline int kit_dow(int y, int m, int d) { /* 0 = domingo */
    static const int t[] = {0, 3, 2, 5, 0, 3, 5, 1, 4, 6, 2, 4};
    if (m < 3) y -= 1;
    return (y + y / 4 - y / 100 + y / 400 + t[m - 1] + d) % 7;
}
/* dias desde 1/1/1970 */
static inline i64 kit_days(int y, int m, int d) {
    y -= m <= 2;
    i64 era = (y >= 0 ? y : y - 399) / 400;
    i64 yoe = y - era * 400;
    i64 doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    i64 doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}
static inline void kit_from_days(i64 z, int *y, int *m, int *d) {
    z += 719468;
    i64 era = (z >= 0 ? z : z - 146096) / 146097;
    i64 doe = z - era * 146097;
    i64 yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    i64 yy = yoe + era * 400;
    i64 doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    i64 mp = (5 * doy + 2) / 153;
    *d = (int)(doy - (153 * mp + 2) / 5 + 1);
    *m = (int)(mp < 10 ? mp + 3 : mp - 9);
    *y = (int)(yy + (*m <= 2));
}
/* segundos desde a época (horário local do RTC) */
static inline i64 kit_now_secs(void) {
    datetime_t t;
    rtc_now(&t);
    return kit_days(t.year, t.month, t.day) * 86400 + t.hour * 3600 + t.minute * 60 + t.second;
}
static inline void kit_today(int *y, int *m, int *d) { datetime_t t; rtc_now(&t); *y = t.year; *m = t.month; *d = t.day; }

/* "7 de novembro de 2026" */
static inline void kit_fmt_date(char *out, int n, int y, int m, int d) { snprintf(out, n, "%d de %s de %d", d, kit_month[m - 1], y); }
/* "07/11/2026" */
static inline void kit_fmt_short(char *out, int n, int y, int m, int d) { snprintf(out, n, "%02d/%02d/%04d", d, m, y); }

/* ------------------------------------------------------------ armazenamento simples em texto (UTF-8, linhas com \t) */
/* pasta de dados do app em ~/Biblioteca/<app> (criada se preciso) */
static inline const char *kit_lib(const char *app, const char *file, char *out, int n) {
    char d[200];
    snprintf(d, sizeof d, "%s/Biblioteca/%s", CUR_HOME, app);
    vfs_mkdir(d);
    snprintf(out, n, "%s/%s", d, file);
    return out;
}
/* lê o arquivo inteiro numa cópia terminada em 0 (kfree depois); NULL se não existe */
static inline char *kit_read(const char *path, int *len) {
    vnode_t *v = vfs_lookup(path);
    if (!v || v->dir) return NULL;
    char *b = kmalloc(v->size + 1);
    if (!b) return NULL;
    memcpy(b, v->data, v->size);
    b[v->size] = 0;
    if (len) *len = (int)v->size;
    return b;
}
/* divide uma linha em campos separados por \t (modifica a linha); devolve o número de campos */
static inline int kit_split(char *line, char **f, int max) {
    int n = 0;
    f[n++] = line;
    for (char *p = line; *p && n < max; p++)
        if (*p == '\t') { *p = 0; f[n++] = p + 1; }
    return n;
}
/* próxima linha de um buffer (modifica: troca \n por 0); NULL no fim */
static inline char *kit_line(char **cur) {
    char *p = *cur;
    if (!p || !*p) return NULL;
    char *nl = strchr(p, '\n');
    if (nl) { *nl = 0; *cur = nl + 1; } else *cur = p + strlen(p);
    usize l = strlen(p);
    if (l && p[l - 1] == '\r') p[l - 1] = 0;
    return p;
}
/* copia sem tabs/quebras (para gravar num campo) */
static inline void kit_field(char *dst, int n, const char *src) {
    int k = 0;
    for (; src && *src && k < n - 1; src++) dst[k++] = (*src == '\t' || *src == '\n' || *src == '\r') ? ' ' : *src;
    dst[k] = 0;
}
/* escapa quebras de linha como \n literal (texto longo num campo) e o inverso */
static inline void kit_esc(sbuf_t *b, const char *s) {
    for (; s && *s; s++) {
        if (*s == '\n') sb_puts(b, "\\n");
        else if (*s == '\t') sb_puts(b, "\\t");
        else if (*s == '\\') sb_puts(b, "\\\\");
        else if (*s != '\r') sb_putc(b, *s);
    }
}
static inline void kit_unesc(char *s) {
    char *o = s;
    for (; *s; s++) {
        if (*s == '\\' && s[1]) { s++; *o++ = *s == 'n' ? '\n' : *s == 't' ? '\t' : *s; }
        else *o++ = *s;
    }
    *o = 0;
}
static inline void kit_save(const char *path, sbuf_t *b) { vfs_write_file(path, b->p ? b->p : "", b->len); sb_free(b); }

/* ------------------------------------------------------------ desenho */
/* avatar circular com iniciais (Contatos, Mensagens) */
static inline void kit_avatar(surface_t *s, float cx, float cy, float r, const char *name) {
    char ini[8] = "";
    int k = 0;
    const char *p = name;
    while (*p == ' ') p++;
    if (*p) { const char *q = p; utf8_decode(&q); memcpy(ini + k, p, q - p); k += (int)(q - p); }
    const char *sp = strrchr(name, ' ');
    if (sp && sp[1] && sp > p) { const char *q = sp + 1; utf8_decode(&q); memcpy(ini + k, sp + 1, q - sp - 1); k += (int)(q - sp - 1); }
    ini[k] = 0;
    for (int i = 0; i < k; i++) ini[i] = (char)toupper(ini[i]);
    gfx_circle(s, cx, cy, r, RGB(168, 172, 182));
    gfx_round_rect_grad(s, cx - r, cy - r, 2 * r, 2 * r, r, RGB(178, 182, 192), RGB(130, 134, 146));
    if (ini[0]) text_draw_center(s, font_get(FONT_MEDIUM), r * 0.85f, rect_make((int)(cx - r), (int)(cy - r), (int)(2 * r), (int)(2 * r)), RGB(255, 255, 255), ini);
}

/* ------------------------------------------------------------ matemática em double */
static inline double kit_fabs(double x) { return x < 0 ? -x : x; }
static inline double kit_floor(double x) { double t = (double)(i64)x; return t > x ? t - 1 : t; }
static inline double kit_fmod(double a, double b) { return a - b * kit_floor(a / b); }
static inline double kit_sqrt(double x) {
    if (x <= 0) return 0;
    double r = x > 1 ? x : 1;
    for (int i = 0; i < 60; i++) { double n = 0.5 * (r + x / r); if (n == r) break; r = n; }
    return r;
}
#define KIT_PI 3.14159265358979323846
static inline double kit_sin(double x) {
    x = kit_fmod(x + KIT_PI, 2 * KIT_PI) - KIT_PI;
    double x2 = x * x, t = x, s = x;
    for (int i = 1; i < 14; i++) { t *= -x2 / ((2 * i) * (2 * i + 1)); s += t; }
    return s;
}
static inline double kit_cos(double x) { return kit_sin(x + KIT_PI / 2); }
static inline double kit_tan(double x) { double c = kit_cos(x); return c == 0 ? 1e300 : kit_sin(x) / c; }
static inline double kit_exp(double x) {
    if (x > 700) return 1e300;
    if (x < -700) return 0;
    int k = (int)(x / 0.6931471805599453);
    double r = x - k * 0.6931471805599453, t = 1, s = 1;
    for (int i = 1; i < 25; i++) { t *= r / i; s += t; }
    while (k > 0) { s *= 2; k--; }
    while (k < 0) { s /= 2; k++; }
    return s;
}
static inline double kit_log(double x) {
    if (x <= 0) return -1e300;
    int k = 0;
    while (x > 2) { x /= 2; k++; }
    while (x < 0.5) { x *= 2; k--; }
    double y = (x - 1) / (x + 1), y2 = y * y, t = y, s = 0;
    for (int i = 1; i < 60; i += 2) { s += t / i; t *= y2; }
    return 2 * s + k * 0.6931471805599453;
}
static inline double kit_pow(double b, double e) {
    if (e == (double)(i64)e && kit_fabs(e) < 64) {
        double r = 1, x = b;
        i64 n = (i64)(e < 0 ? -e : e);
        while (n) { if (n & 1) r *= x; x *= x; n >>= 1; }
        return e < 0 ? 1 / r : r;
    }
    if (b <= 0) return 0.0 / 0.0;
    return kit_exp(e * kit_log(b));
}
static inline double kit_atan(double x) {
    bool inv = kit_fabs(x) > 1;
    if (inv) x = 1 / x;
    /* redução: atan(x) = 2 atan(x / (1 + sqrt(1 + x²))) */
    double y = x / (1 + kit_sqrt(1 + x * x));
    double y2 = y * y, t = y, s = 0;
    for (int i = 1; i < 60; i += 2) { s += t / i; t *= -y2; }
    s *= 2;
    if (inv) s = (x > 0 ? KIT_PI / 2 : -KIT_PI / 2) - s;
    return s;
}
static inline double kit_atan2(double y, double x) {
    if (x > 0) return kit_atan(y / x);
    if (x < 0) return y >= 0 ? kit_atan(y / x) + KIT_PI : kit_atan(y / x) - KIT_PI;
    return y > 0 ? KIT_PI / 2 : y < 0 ? -KIT_PI / 2 : 0;
}
static inline double kit_asin(double x) { return kit_atan2(x, kit_sqrt(1 - x * x)); }
static inline double kit_acos(double x) { return kit_atan2(kit_sqrt(1 - x * x), x); }

/* número formatado em pt-BR com até `dec` casas, sem zeros à direita */
static inline void kit_fmt_num(char *out, int n, double v, int dec) {
    if (v != v) { snprintf(out, n, "—"); return; }
    char tmp[48];
    snprintf(tmp, sizeof tmp, "%.*f", dec, v);
    char *dot = strchr(tmp, '.');
    if (dot) { char *e = tmp + strlen(tmp) - 1; while (e > dot && *e == '0') *e-- = 0; if (e == dot) *e = 0; }
    for (char *p = tmp; *p; p++) if (*p == '.') *p = ',';
    snprintf(out, n, "%s", tmp);
}

/* ------------------------------------------------------------ imagens: BMP de 32 bits (gravar) e 24/32 bits (ler) */
static inline void kit_put32(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }
static inline u32 kit_get32(const u8 *p) { return p[0] | p[1] << 8 | p[2] << 16 | (u32)p[3] << 24; }
static inline bool kit_save_bmp(const char *path, const surface_t *s) {
    u32 w = (u32)s->w, h = (u32)s->h, img = w * h * 4, total = 54 + img;
    u8 *b = kcalloc(total, 1);
    if (!b) return false;
    b[0] = 'B'; b[1] = 'M';
    kit_put32(b + 2, total); kit_put32(b + 10, 54); kit_put32(b + 14, 40); kit_put32(b + 18, w); kit_put32(b + 22, (u32)-(i32)h);
    b[26] = 1; b[28] = 32;
    kit_put32(b + 34, img); kit_put32(b + 38, 2835); kit_put32(b + 42, 2835);
    u8 *d = b + 54;
    for (u32 y = 0; y < h; y++)
        for (u32 x = 0; x < w; x++, d += 4) kit_put32(d, s->px[(usize)y * s->stride + x] | 0xFF000000u);
    bool ok = vfs_write_file(path, b, total) != NULL;
    kfree(b);
    return ok;
}
static inline surface_t *kit_load_image(const char *path) {
    vnode_t *v = vfs_lookup(path);
    if (!v || v->dir || v->size < 16) return NULL;
    vfs_ensure(v);
    const u8 *d = v->data;
    if (d[0] == 'B' && d[1] == 'M' && v->size > 54) {
        i32 w = (i32)kit_get32(d + 18), h = (i32)kit_get32(d + 22);
        int bpp = d[28] | d[29] << 8;
        u32 off = kit_get32(d + 10);
        bool top = h < 0;
        if (h < 0) h = -h;
        if (w <= 0 || h <= 0 || w > 8192 || h > 8192 || (bpp != 24 && bpp != 32)) return NULL;
        int stride = ((w * bpp / 8) + 3) & ~3;
        if (off + (u64)stride * h > v->size) return NULL;
        surface_t *s = surface_new(w, h);
        for (int y = 0; y < h; y++) {
            const u8 *row = d + off + (usize)(top ? y : h - 1 - y) * stride;
            for (int x = 0; x < w; x++) {
                const u8 *p = row + x * (bpp / 8);
                s->px[(usize)y * s->stride + x] = 0xFF000000u | (u32)p[2] << 16 | (u32)p[1] << 8 | p[0];
            }
        }
        return s;
    }
    return image_decode(d, v->size);
}
static inline bool kit_is_image(const char *name) {
    const char *e = vfs_ext(name);
    return !strcasecmp(e, "png") || !strcasecmp(e, "jpg") || !strcasecmp(e, "jpeg") || !strcasecmp(e, "bmp") || !strcasecmp(e, "gif");
}

/* ------------------------------------------------------------ áudio: WAV PCM 16 bits */
static inline bool kit_save_wav(const char *path, const i16 *st, u32 frames, int channels, int rate) {
    u32 data = frames * channels * 2, total = 44 + data;
    u8 *b = kmalloc(total);
    if (!b) return false;
    memcpy(b, "RIFF", 4); kit_put32(b + 4, total - 8); memcpy(b + 8, "WAVEfmt ", 8);
    kit_put32(b + 16, 16); b[20] = 1; b[21] = 0; b[22] = (u8)channels; b[23] = 0;
    kit_put32(b + 24, rate); kit_put32(b + 28, rate * channels * 2); b[32] = (u8)(channels * 2); b[33] = 0; b[34] = 16; b[35] = 0;
    memcpy(b + 36, "data", 4); kit_put32(b + 40, data);
    memcpy(b + 44, st, data);
    bool ok = vfs_write_file(path, b, total) != NULL;
    kfree(b);
    return ok;
}
/* carrega um WAV PCM 16 bits como estéreo 48 kHz (reamostragem linear); kfree no retorno */
static inline i16 *kit_load_wav(const char *path, u32 *frames_out) {
    vnode_t *v = vfs_lookup(path);
    if (!v || v->dir || v->size < 44) return NULL;
    vfs_ensure(v);
    const u8 *d = v->data;
    if (memcmp(d, "RIFF", 4) || memcmp(d + 8, "WAVE", 4)) return NULL;
    int ch = 0, rate = 0, bits = 0;
    const u8 *pcm = NULL;
    u32 pcm_len = 0;
    for (u32 off = 12; off + 8 <= v->size;) {
        u32 len = kit_get32(d + off + 4);
        if (!memcmp(d + off, "fmt ", 4) && len >= 16) { ch = d[off + 10]; rate = (int)kit_get32(d + off + 12); bits = d[off + 22]; }
        else if (!memcmp(d + off, "data", 4)) { pcm = d + off + 8; pcm_len = MIN(len, (u32)(v->size - off - 8)); }
        off += 8 + len + (len & 1);
    }
    if (!pcm || bits != 16 || ch < 1 || ch > 2 || rate < 4000) return NULL;
    u32 in_frames = pcm_len / (ch * 2);
    u32 out_frames = (u32)((u64)in_frames * 48000 / rate);
    i16 *o = kmalloc((usize)out_frames * 4 + 4);
    if (!o) return NULL;
    const i16 *src = (const i16 *)pcm;
    for (u32 i = 0; i < out_frames; i++) {
        u32 j = (u32)((u64)i * rate / 48000);
        if (j >= in_frames) j = in_frames - 1;
        o[i * 2] = src[j * ch];
        o[i * 2 + 1] = src[j * ch + (ch == 2 ? 1 : 0)];
    }
    *frames_out = out_frames;
    return o;
}

/* ------------------------------------------------------------ JSON mínimo (leitura por caminho "a.b.c" ou "a.b[3]") */
static inline const char *kit_json_ws(const char *p) { while (*p == ' ' || *p == '\n' || *p == '\r' || *p == '\t') p++; return p; }
static inline const char *kit_json_skip(const char *p) {
    p = kit_json_ws(p);
    if (*p == '"') { p++; while (*p && *p != '"') { if (*p == '\\' && p[1]) p++; p++; } return *p ? p + 1 : p; }
    if (*p == '{' || *p == '[') {
        int depth = 0;
        for (; *p; p++) {
            if (*p == '"') { p++; while (*p && *p != '"') { if (*p == '\\' && p[1]) p++; p++; } if (!*p) return p; continue; }
            if (*p == '{' || *p == '[') depth++;
            else if (*p == '}' || *p == ']') { if (--depth == 0) return p + 1; }
        }
        return p;
    }
    while (*p && *p != ',' && *p != '}' && *p != ']') p++;
    return p;
}
/* devolve o início do valor no caminho, ou NULL */
static inline const char *kit_json_get(const char *p, const char *path) {
    while (p && *path) {
        p = kit_json_ws(p);
        if (*path == '[') {
            int idx = atoi(path + 1);
            while (*path && *path != ']') path++;
            if (*path) path++;
            if (*path == '.') path++;
            if (*p != '[') return NULL;
            p = kit_json_ws(p + 1);
            for (int i = 0; i < idx; i++) {
                p = kit_json_ws(kit_json_skip(p));
                if (*p != ',') return NULL;
                p = kit_json_ws(p + 1);
            }
            continue;
        }
        char key[64];
        int k = 0;
        while (*path && *path != '.' && *path != '[' && k < 63) key[k++] = *path++;
        key[k] = 0;
        if (*path == '.') path++;
        if (*p != '{') return NULL;
        p = kit_json_ws(p + 1);
        bool found = false;
        while (*p == '"') {
            const char *ks = p + 1, *ke = ks;
            while (*ke && *ke != '"') { if (*ke == '\\' && ke[1]) ke++; ke++; }
            bool match = (int)(ke - ks) == k && !memcmp(ks, key, k);
            p = kit_json_ws(ke + 1);
            if (*p == ':') p = kit_json_ws(p + 1);
            if (match) { found = true; break; }
            p = kit_json_ws(kit_json_skip(p));
            if (*p == ',') p = kit_json_ws(p + 1);
        }
        if (!found) return NULL;
    }
    return p ? kit_json_ws(p) : NULL;
}
static inline double kit_json_num(const char *json, const char *path, double def) {
    const char *p = kit_json_get(json, path);
    if (!p || (!isdigit((u8)*p) && *p != '-')) return def;
    bool neg = *p == '-';
    if (neg) p++;
    double v = 0, fr = 0.1;
    while (isdigit((u8)*p)) v = v * 10 + (*p++ - '0');
    if (*p == '.') { p++; while (isdigit((u8)*p)) { v += (*p++ - '0') * fr; fr /= 10; } }
    if (*p == 'e' || *p == 'E') { p++; bool en = *p == '-'; if (*p == '-' || *p == '+') p++; int e = 0; while (isdigit((u8)*p)) e = e * 10 + (*p++ - '0'); v *= kit_pow(10, en ? -e : e); }
    return neg ? -v : v;
}
static inline bool kit_json_str(const char *json, const char *path, char *out, int n) {
    const char *p = kit_json_get(json, path);
    out[0] = 0;
    if (!p || *p != '"') return false;
    p++;
    int k = 0;
    while (*p && *p != '"' && k < n - 1) {
        if (*p == '\\' && p[1]) {
            p++;
            if (*p == 'u') {
                u32 cp = (u32)strtol((char[5]){p[1], p[2], p[3], p[4], 0}, NULL, 16);
                char u[4];
                int l = utf8_encode(cp, u);
                if (k + l < n) { memcpy(out + k, u, l); k += l; }
                p += 5;
                continue;
            }
            out[k++] = *p == 'n' ? '\n' : *p == 't' ? '\t' : *p;
            p++;
            continue;
        }
        out[k++] = *p++;
    }
    out[k] = 0;
    return true;
}
