/* Lista de login items: um caminho absoluto por linha.
 * Sem string.h. Quem inclui já tem strlen, strcmp, memcpy e memset.
 * LI_SYNC: reconciliação com os jobs (init e o teste de host).
 * LI_HOST ou o libSystem: ler e reescrever o arquivo. LI_LOGIC_ONLY omite o arquivo. */
#ifndef LI_MAX
#define LI_MAX 64
#endif
#define LI_PATH 256
#define LI_TEXT 18432

static int li_copy(char *d, int cap, const char *s) {
    int i = 0;
    if (!d || cap < 1) return -1;
    if (!s) s = "";
    while (s[i] && i + 1 < cap) { d[i] = s[i]; i++; }
    d[i] = 0;
    return s[i] ? -1 : i;
}

static int li_eq(const char *a, const char *b) {
    if (!a || !b) return 0;
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

/* Absoluto, sem componente ".." e sem quebra de linha. */
static int li_path_ok(const char *p) {
    const char *s;
    int any = 0;
    if (!p || p[0] != '/') return 0;
    s = p;
    while (*s) {
        const char *e;
        int n;
        if (*s == '\n' || *s == '\r') return 0;
        if (*s == '/') { s++; continue; }
        e = s;
        while (*e && *e != '/') {
            if (*e == '\n' || *e == '\r') return 0;
            e++;
        }
        n = (int)(e - s);
        if (n == 0 || (n == 2 && s[0] == '.' && s[1] == '.')) return 0;
        any = 1;
        s = e;
    }
    if (!any || p[strlen(p) - 1] == '/') return 0;
    return 1;
}

static void li_trim(char *s) {
    char *b = s;
    int n, i, w;
    while (*b == ' ' || *b == '\t' || *b == '\r') b++;
    n = (int)strlen(b);
    while (n > 0 && (b[n - 1] == ' ' || b[n - 1] == '\t' || b[n - 1] == '\r')) n--;
    w = 0;
    for (i = 0; i < n; i++) s[w++] = b[i];
    s[w] = 0;
}

/* Linha longa demais é pulada, não truncada. Devolve quantos caminhos válidos. */
static int li_parse(const char *text, char paths[][LI_PATH], int max) {
    const char *p = text ? text : "";
    int n = 0;
    while (*p && n < max) {
        char line[LI_PATH];
        int i = 0, overflow = 0, k;
        while (*p && *p != '\n') {
            if (i + 1 < LI_PATH) line[i++] = *p;
            else overflow = 1;
            p++;
        }
        line[i] = 0;
        if (*p == '\n') p++;
        if (overflow) continue;
        li_trim(line);
        if (!li_path_ok(line)) continue;
        for (k = 0; k < n; k++) if (li_eq(paths[k], line)) break;
        if (k < n) continue;
        li_copy(paths[n], LI_PATH, line);
        n++;
    }
    return n;
}

static int li_format(char paths[][LI_PATH], int n, char *out, int cap) {
    int o = 0, i;
    if (!out || cap < 1) return -1;
    for (i = 0; i < n; i++) {
        int k = 0;
        while (paths[i][k]) {
            if (o + 2 >= cap) { out[0] = 0; return -1; }
            out[o++] = paths[i][k++];
        }
        out[o++] = '\n';
    }
    out[o] = 0;
    return o;
}

/* 0 = texto novo em out. -1 = caminho recusado (out intacto). */
static int li_text_add(const char *text, const char *path, char *out, int cap) {
    char paths[LI_MAX][LI_PATH];
    int n;
    if (!li_path_ok(path)) return -1;
    n = li_parse(text, paths, LI_MAX);
    if (n >= LI_MAX) {
        int k;
        for (k = 0; k < n; k++) if (li_eq(paths[k], path)) return li_format(paths, n, out, cap) < 0 ? -1 : 0;
        return -1;
    }
    {
        int k;
        for (k = 0; k < n; k++) if (li_eq(paths[k], path)) break;
        if (k == n) li_copy(paths[n++], LI_PATH, path);
    }
    return li_format(paths, n, out, cap) < 0 ? -1 : 0;
}

/* 0 = reescreve. 1 = o caminho não estava (out ainda é o texto normalizado). -1 = não coube. */
static int li_text_remove(const char *text, const char *path, char *out, int cap) {
    char paths[LI_MAX][LI_PATH];
    char keep[LI_MAX][LI_PATH];
    int n, i, m = 0, found = 0;
    if (!path) return -1;
    n = li_parse(text, paths, LI_MAX);
    for (i = 0; i < n; i++) {
        if (li_eq(paths[i], path)) { found = 1; continue; }
        li_copy(keep[m++], LI_PATH, paths[i]);
    }
    if (li_format(keep, m, out, cap) < 0) return -1;
    return found ? 0 : 1;
}

static unsigned li_seed(const char *text) {
    unsigned h = 2166136261u;
    const unsigned char *p = (const unsigned char *)(text ? text : "");
    while (*p) { h ^= *p++; h *= 16777619u; }
    return h;
}

#if defined(LI_SYNC)
typedef struct {
    int used, pid, last_start, runatload, keep;
    char path[LI_PATH];
    char label[128];
} li_slot_t;

static void li_label(const char *path, char *out, int cap) {
    const char *base = path ? path : "";
    const char *p = base;
    const char *pre = "mos.login.";
    int i = 0;
    while (*p) { if (*p == '/') base = p + 1; p++; }
    while (pre[i] && i + 1 < cap) { out[i] = pre[i]; i++; }
    while (*base && i + 1 < cap) { out[i++] = *base++; }
    out[i] = 0;
}

/* Slots com used=0 estão livres. O que saiu da lista e não tem pid é liberado.
 * O que saiu e ainda roda fica com runatload e keep em 0. Devolve quantos o init subiria agora. */
static int li_sync(li_slot_t *slots, int nslots, char paths[][LI_PATH], int npaths) {
    char want[LI_MAX];
    int i, p, up = 0;
    if (!slots || nslots < 1) return 0;
    if (nslots > LI_MAX) nslots = LI_MAX;
    for (i = 0; i < nslots; i++) want[i] = 0;
    for (p = 0; p < npaths; p++) {
        int found = -1;
        for (i = 0; i < nslots; i++)
            if (slots[i].used && li_eq(slots[i].path, paths[p])) { found = i; break; }
        if (found >= 0) { want[found] = 1; continue; }
        for (i = 0; i < nslots; i++) if (!slots[i].used) {
            int z;
            for (z = 0; z < (int)sizeof slots[i]; z++) ((char *)&slots[i])[z] = 0;
            slots[i].used = 1;
            slots[i].runatload = 1;
            li_copy(slots[i].path, LI_PATH, paths[p]);
            li_label(paths[p], slots[i].label, (int)sizeof slots[i].label);
            want[i] = 1;
            break;
        }
    }
    for (i = 0; i < nslots; i++) {
        if (!slots[i].used || want[i]) continue;
        if (slots[i].pid > 0) {
            slots[i].runatload = 0;
            slots[i].keep = 0;
        } else {
            slots[i].used = 0;
            slots[i].runatload = 0;
            slots[i].keep = 0;
        }
    }
    for (i = 0; i < nslots; i++)
        if (slots[i].used && slots[i].pid <= 0 && !slots[i].last_start && (slots[i].runatload || slots[i].keep)) up++;
    return up;
}
#endif

#ifndef LI_LOGIC_ONLY
#ifdef LI_HOST
#include <stdio.h>
typedef FILE li_file_t;
#else
typedef FILE_ li_file_t;
li_file_t *fopen(const char *p, const char *m);
int fclose(li_file_t *f);
size_t fread(void *p, size_t a, size_t b, li_file_t *f);
size_t fwrite(const void *p, size_t a, size_t b, li_file_t *f);
int rename(const char *a, const char *b);
#endif

static int li_file_read(const char *file, char *buf, int cap) {
    li_file_t *f;
    int n;
    if (!buf || cap < 2) return -1;
    buf[0] = 0;
    if (!file) return 0;
    f = fopen(file, "rb");
    if (!f) return 0;
    n = (int)fread(buf, 1, (size_t)(cap - 1), f);
    fclose(f);
    if (n < 0) n = 0;
    if (n >= cap) n = cap - 1;
    buf[n] = 0;
    return n;
}

static void li_tmp_name(const char *file, char *out, int cap) {
    int i = 0;
    while (file && file[i] && i + 5 < cap) { out[i] = file[i]; i++; }
    if (i + 5 < cap) { out[i++] = '.'; out[i++] = 't'; out[i++] = 'm'; out[i++] = 'p'; }
    out[i] = 0;
}

static int li_file_write(const char *file, const char *text) {
    char tmp[512];
    li_file_t *f;
    int n = 0;
    if (!file || !li_path_ok(file)) return -1;
    if (text) while (text[n]) n++;
    li_tmp_name(file, tmp, (int)sizeof tmp);
    if (!tmp[0]) return -1;
    f = fopen(tmp, "wb");
    if (!f) return -1;
    if (n && (int)fwrite(text, 1, (size_t)n, f) != n) { fclose(f); return -1; }
    if (fclose(f)) return -1;
    if (rename(tmp, file) != 0) return -1;
    return 0;
}

/* 0 = gravou. -1 = recusou e não trocou o arquivo. */
static int li_file_add(const char *file, const char *path) {
    char cur[LI_TEXT], out[LI_TEXT];
    if (li_file_read(file, cur, LI_TEXT) < 0) return -1;
    if (li_text_add(cur, path, out, LI_TEXT) != 0) return -1;
    return li_file_write(file, out);
}

static int li_file_remove(const char *file, const char *path) {
    char cur[LI_TEXT], out[LI_TEXT];
    int rc;
    if (li_file_read(file, cur, LI_TEXT) < 0) return -1;
    rc = li_text_remove(cur, path, out, LI_TEXT);
    if (rc < 0) return -1;
    if (rc == 1 && cur[0] == 0) return 0;
    return li_file_write(file, out);
}

static int li_file_list(const char *file, char paths[][LI_PATH], int max) {
    char cur[LI_TEXT];
    if (li_file_read(file, cur, LI_TEXT) < 0) return -1;
    return li_parse(cur, paths, max);
}
#endif
