/* Loja do Core Data do mOS.
 *
 * NSSQLiteStoreType grava neste formato (primeira linha "MOSCD1"), no caminho que o
 * app pediu. Não é um arquivo SQLite: o libSystem já publica sqlite3_open_v2 que
 * falha de propósito, e um amalgama aqui colidiria com esses símbolos. Quem abrir
 * o arquivo no sqlite3 da Apple não vê um banco. NSInMemoryStoreType não toca disco.
 *
 * Incluir uma vez por unidade. Com CD_HOST, o teste de host (tools/cdtest.c) usa a libc.
 */
#ifdef CD_HOST
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
typedef FILE cd_file;
#else
#include <stddef.h>
#include <stdint.h>
typedef void cd_file;
extern void *fopen(const char *p, const char *m);
extern int fclose(void *f);
extern int fgetc(void *f);
extern size_t fwrite(const void *p, size_t s, size_t n, void *f);
extern int fprintf(void *f, const char *fmt, ...);
extern int snprintf(char *b, size_t n, const char *fmt, ...);
extern int mkdir(const char *p, int m);
extern int rename(const char *a, const char *b);
extern int unlink(const char *p);
extern void free(void *p);
extern void *malloc(size_t n);
extern void *realloc(void *p, size_t n);
extern void *memcpy(void *d, const void *s, size_t n);
extern size_t strlen(const char *s);
extern char *strcpy(char *d, const char *s);
extern char *strdup(const char *s);
extern int strcmp(const char *a, const char *b);
extern unsigned long strtoul(const char *s, char **e, int base);
extern long long strtoll(const char *s, char **e, int base);
extern double strtod(const char *s, char **e);
#endif

#define CD_EOF (-1)

typedef struct cd_attr {
    char *key;
    int kind; /* 1 str, 2 i64, 3 f64 */
    char *s;
    long long i;
    double d;
} cd_attr;

typedef struct cd_row {
    unsigned long id;
    char *entity;
    cd_attr *a;
    int na, ca;
    int deleted;
} cd_row;

typedef struct cd_store {
    int memory;
    char *path;
    cd_row *rows;
    int n, cap;
    unsigned long next_id;
} cd_store;

static cd_row *cd_row_of(cd_store *s, unsigned long id) {
    if (!s) return 0;
    for (int i = 0; i < s->n; i++)
        if (s->rows[i].id == id && !s->rows[i].deleted) return &s->rows[i];
    return 0;
}

static cd_store *cd_new(int memory, const char *path) {
    cd_store *s = (cd_store *)malloc(sizeof *s);
    if (!s) return 0;
    s->memory = memory;
    s->path = path ? strdup(path) : 0;
    s->rows = 0;
    s->n = s->cap = 0;
    s->next_id = 1;
    if (path && !s->path) { free(s); return 0; }
    return s;
}

static void cd_attr_clear(cd_attr *a) {
    free(a->key);
    free(a->s);
    a->key = a->s = 0;
}

static void cd_row_clear(cd_row *r) {
    for (int i = 0; i < r->na; i++) cd_attr_clear(&r->a[i]);
    free(r->a);
    free(r->entity);
    r->a = 0;
    r->entity = 0;
}

static void cd_close(cd_store *s) {
    if (!s) return;
    for (int i = 0; i < s->n; i++) cd_row_clear(&s->rows[i]);
    free(s->rows);
    free(s->path);
    free(s);
}

static char *cd_read_line(cd_file *f) {
    size_t cap = 256, n = 0;
    char *b = (char *)malloc(cap);
    int c;
    if (!b) return 0;
    while ((c = fgetc(f)) != CD_EOF) {
        if (n + 1 >= cap) {
            char *g;
            cap *= 2;
            g = (char *)realloc(b, cap);
            if (!g) { free(b); return 0; }
            b = g;
        }
        if (c == '\n') break;
        if (c != '\r') b[n++] = (char)c;
    }
    if (c == CD_EOF && n == 0) { free(b); return 0; }
    b[n] = 0;
    return b;
}

static void cd_unescape(char *s) {
    char *w = s;
    for (char *r = s; *r; r++) {
        if (*r == '\\' && r[1]) {
            r++;
            if (*r == 'n') *w++ = '\n';
            else if (*r == 't') *w++ = '\t';
            else *w++ = *r;
        } else *w++ = *r;
    }
    *w = 0;
}

static int cd_split(char *line, char **f, int nf) {
    int n = 0;
    char *p = line;
    while (n < nf) {
        f[n++] = p;
        char *t = p;
        while (*t && *t != '\t') t++;
        if (!*t) break;
        *t = 0;
        p = t + 1;
    }
    return n;
}

static int cd_add_row(cd_store *s, unsigned long id, const char *entity) {
    cd_row *r;
    if (id >= s->next_id) s->next_id = id + 1;
    if (cd_row_of(s, id)) return 0;
    if (s->n == s->cap) {
        int nc = s->cap ? s->cap * 2 : 8;
        cd_row *g = (cd_row *)realloc(s->rows, (size_t)nc * sizeof *g);
        if (!g) return -1;
        s->rows = g;
        s->cap = nc;
    }
    r = &s->rows[s->n++];
    r->id = id;
    r->entity = strdup(entity ? entity : "");
    r->a = 0;
    r->na = r->ca = 0;
    r->deleted = 0;
    if (!r->entity) { s->n--; return -1; }
    return 0;
}

static int cd_set_raw(cd_store *s, unsigned long id, const char *key, int kind, const char *str, long long iv, double dv) {
    cd_row *r = cd_row_of(s, id);
    cd_attr *a;
    int i;
    if (!r || !key) return -1;
    for (i = 0; i < r->na; i++)
        if (!strcmp(r->a[i].key, key)) break;
    if (i == r->na) {
        if (r->na == r->ca) {
            int nc = r->ca ? r->ca * 2 : 4;
            cd_attr *g = (cd_attr *)realloc(r->a, (size_t)nc * sizeof *g);
            if (!g) return -1;
            r->a = g;
            r->ca = nc;
        }
        a = &r->a[r->na++];
        a->key = strdup(key);
        a->s = 0;
        if (!a->key) { r->na--; return -1; }
    } else a = &r->a[i];
    free(a->s);
    a->s = 0;
    a->kind = kind;
    a->i = iv;
    a->d = dv;
    if (kind == 1) {
        a->s = strdup(str ? str : "");
        if (!a->s) return -1;
    }
    return 0;
}

static int cd_load(cd_store *s) {
    cd_file *f;
    char *magic;
    if (!s->path) return 0;
    f = fopen(s->path, "rb");
    if (!f) return 0;
    magic = cd_read_line(f);
    if (!magic || strcmp(magic, "MOSCD1") != 0) { free(magic); fclose(f); return -1; }
    free(magic);
    for (;;) {
        char *line = cd_read_line(f);
        char *fld[5];
        int n;
        if (!line) break;
        if (!line[0]) { free(line); continue; }
        n = cd_split(line, fld, 5);
        if (line[0] == 'O' && n >= 3) {
            unsigned long id = strtoul(fld[1], 0, 10);
            cd_unescape(fld[2]);
            if (cd_add_row(s, id, fld[2])) { free(line); fclose(f); return -1; }
        } else if (line[0] == 'A' && n >= 5) {
            unsigned long id = strtoul(fld[1], 0, 10);
            int kind = fld[2][0] == 'i' ? 2 : fld[2][0] == 'd' ? 3 : 1;
            cd_unescape(fld[3]);
            cd_unescape(fld[4]);
            if (!cd_row_of(s, id)) cd_add_row(s, id, "");
            if (kind == 2) cd_set_raw(s, id, fld[3], 2, 0, strtoll(fld[4], 0, 10), 0);
            else if (kind == 3) cd_set_raw(s, id, fld[3], 3, 0, 0, strtod(fld[4], 0));
            else cd_set_raw(s, id, fld[3], 1, fld[4], 0, 0);
        }
        free(line);
    }
    fclose(f);
    return 0;
}

static cd_store *cd_open_memory(void) { return cd_new(1, 0); }

static cd_store *cd_open_file(const char *path) {
    cd_store *s;
    if (!path || !path[0]) return 0;
    s = cd_new(0, path);
    if (!s) return 0;
    if (cd_load(s)) { cd_close(s); return 0; }
    return s;
}

static unsigned long cd_insert(cd_store *s, const char *entity) {
    unsigned long id;
    if (!s) return 0;
    id = s->next_id++;
    if (cd_add_row(s, id, entity && entity[0] ? entity : "Entity")) return 0;
    return id;
}

static void cd_delete(cd_store *s, unsigned long id) {
    cd_row *r = cd_row_of(s, id);
    if (r) r->deleted = 1;
}

static int cd_set_str(cd_store *s, unsigned long id, const char *key, const char *val) {
    cd_row *r;
    int i;
    if (!val) {
        r = cd_row_of(s, id);
        if (!r) return -1;
        for (i = 0; i < r->na; i++)
            if (!strcmp(r->a[i].key, key)) {
                cd_attr_clear(&r->a[i]);
                r->a[i] = r->a[r->na - 1];
                r->na--;
                return 0;
            }
        return 0;
    }
    return cd_set_raw(s, id, key, 1, val, 0, 0);
}

static int cd_set_i64(cd_store *s, unsigned long id, const char *key, long long v) {
    return cd_set_raw(s, id, key, 2, 0, v, (double)v);
}
static int cd_set_f64(cd_store *s, unsigned long id, const char *key, double v) {
    return cd_set_raw(s, id, key, 3, 0, (long long)v, v);
}

static int cd_get_kind(cd_store *s, unsigned long id, const char *key) {
    cd_row *r = cd_row_of(s, id);
    int i;
    if (!r || !key) return 0;
    for (i = 0; i < r->na; i++)
        if (!strcmp(r->a[i].key, key)) return r->a[i].kind;
    return 0;
}
static const char *cd_get_str(cd_store *s, unsigned long id, const char *key) {
    cd_row *r = cd_row_of(s, id);
    int i;
    if (!r) return 0;
    for (i = 0; i < r->na; i++)
        if (!strcmp(r->a[i].key, key) && r->a[i].kind == 1) return r->a[i].s;
    return 0;
}
static long long cd_get_i64(cd_store *s, unsigned long id, const char *key) {
    cd_row *r = cd_row_of(s, id);
    int i;
    if (!r) return 0;
    for (i = 0; i < r->na; i++)
        if (!strcmp(r->a[i].key, key)) return r->a[i].kind == 2 ? r->a[i].i : (long long)r->a[i].d;
    return 0;
}
static double cd_get_f64(cd_store *s, unsigned long id, const char *key) {
    cd_row *r = cd_row_of(s, id);
    int i;
    if (!r) return 0;
    for (i = 0; i < r->na; i++)
        if (!strcmp(r->a[i].key, key)) return r->a[i].kind == 3 ? r->a[i].d : (double)r->a[i].i;
    return 0;
}
static const char *cd_entity_of(cd_store *s, unsigned long id) {
    cd_row *r = cd_row_of(s, id);
    return r ? r->entity : 0;
}

static int cd_match(cd_store *s, cd_row *r, const char *entity, const char *pk, const char *pv) {
    int i;
    (void)s;
    if (r->deleted) return 0;
    if (entity && entity[0] && strcmp(r->entity, entity) != 0) return 0;
    if (!pk) return 1;
    for (i = 0; i < r->na; i++) {
        char num[64];
        const char *got = 0;
        if (strcmp(r->a[i].key, pk) != 0) continue;
        if (r->a[i].kind == 1) got = r->a[i].s;
        else if (r->a[i].kind == 2) { snprintf(num, sizeof num, "%lld", r->a[i].i); got = num; }
        else { snprintf(num, sizeof num, "%.17g", r->a[i].d); got = num; }
        return got && pv && strcmp(got, pv) == 0;
    }
    return 0;
}

static unsigned long *cd_fetch(cd_store *s, const char *entity, const char *pk, const char *pv,
                               unsigned limit, unsigned offset, unsigned *n) {
    unsigned long *ids;
    unsigned total = 0, k = 0, seen = 0;
    int i;
    if (n) *n = 0;
    if (!s) return 0;
    for (i = 0; i < s->n; i++)
        if (cd_match(s, &s->rows[i], entity, pk, pv)) total++;
    ids = (unsigned long *)malloc(((size_t)total + 1) * sizeof *ids);
    if (!ids) return 0;
    for (i = 0; i < s->n; i++) {
        if (!cd_match(s, &s->rows[i], entity, pk, pv)) continue;
        if (seen++ < offset) continue;
        if (limit && k >= limit) break;
        ids[k++] = s->rows[i].id;
    }
    if (n) *n = k;
    return ids;
}

static int cd_escape_write(cd_file *f, const char *str) {
    const char *p;
    if (!str) return 0;
    for (p = str; *p; p++) {
        char b[2];
        if (*p == '\\' || *p == '\t' || *p == '\n' || *p == '\r') {
            b[0] = '\\';
            b[1] = *p == '\n' ? 'n' : *p == '\t' ? 't' : *p == '\r' ? 'n' : '\\';
            if (fwrite(b, 1, 2, f) != 2) return -1;
        } else if (fwrite(p, 1, 1, f) != 1) return -1;
    }
    return 0;
}

static void cd_mkdir_parent(const char *path) {
    char b[1024];
    char *p;
    size_t n = strlen(path);
    if (n >= sizeof b) return;
    memcpy(b, path, n + 1);
    p = b;
    while (*p == '/') p++;
    for (; *p; p++)
        if (*p == '/') { *p = 0; mkdir(b, 0755); *p = '/'; }
}

static int cd_save(cd_store *s) {
    char tmp[1100];
    cd_file *f;
    int i, k;
    if (!s) return -1;
    if (s->memory || !s->path) return 0;
    if (strlen(s->path) + 5 >= sizeof tmp) return -1;
    cd_mkdir_parent(s->path);
    snprintf(tmp, sizeof tmp, "%s.tmp", s->path);
    f = fopen(tmp, "wb");
    if (!f) return -1;
    if (fprintf(f, "MOSCD1\n") < 0) { fclose(f); unlink(tmp); return -1; }
    for (i = 0; i < s->n; i++) {
        cd_row *r = &s->rows[i];
        if (r->deleted) continue;
        if (fprintf(f, "O\t%lu\t", r->id) < 0 || cd_escape_write(f, r->entity) || fwrite("\n", 1, 1, f) != 1) {
            fclose(f); unlink(tmp); return -1;
        }
        for (k = 0; k < r->na; k++) {
            cd_attr *a = &r->a[k];
            const char *kind = a->kind == 2 ? "i" : a->kind == 3 ? "d" : "s";
            char num[64];
            if (fprintf(f, "A\t%lu\t%s\t", r->id, kind) < 0 || cd_escape_write(f, a->key) || fwrite("\t", 1, 1, f) != 1) {
                fclose(f); unlink(tmp); return -1;
            }
            if (a->kind == 2) {
                snprintf(num, sizeof num, "%lld", a->i);
                if (fwrite(num, 1, strlen(num), f) != strlen(num)) { fclose(f); unlink(tmp); return -1; }
            } else if (a->kind == 3) {
                snprintf(num, sizeof num, "%.17g", a->d);
                if (fwrite(num, 1, strlen(num), f) != strlen(num)) { fclose(f); unlink(tmp); return -1; }
            } else if (cd_escape_write(f, a->s)) { fclose(f); unlink(tmp); return -1; }
            if (fwrite("\n", 1, 1, f) != 1) { fclose(f); unlink(tmp); return -1; }
        }
    }
    if (fclose(f)) { unlink(tmp); return -1; }
    if (rename(tmp, s->path)) { unlink(tmp); return -1; }
    return 0;
}
