/* Agenda em arquivo de texto (MOSCN1). Um commit grava o arquivo inteiro. */
#ifdef CX_CN_HOST
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#else
#include "libc.h"
#include "foundation.h"
char *fgets(char *s, int n, void *f);
int rename(const char *a, const char *b);
#endif
#include "cn_store.h"

enum { CN_MAX = 64, CN_OPS = 64 };

typedef struct {
    int kind;
    cx_cn_contact c;
} cn_op_t;

static cx_cn_contact g_db[CN_MAX];
static int g_n;
static cn_op_t g_op[CN_OPS];
static int g_nop;
static int g_seq;
static int g_auth_over;
static int g_auth_force;
static char g_path[512];

static void scopy(char *d, int cap, const char *s) {
    int i = 0;
    if (!s) s = "";
    if (cap < 1) return;
    for (; i < cap - 1 && s[i] && s[i] != '\t' && s[i] != '\n' && s[i] != '\r'; i++) d[i] = s[i];
    d[i] = 0;
}

static int same(const char *a, const char *b) {
    if (!a) a = "";
    if (!b) b = "";
    return strcmp(a, b) == 0;
}

static int auth_now(void) {
    if (g_auth_force) return g_auth_over;
#ifndef CX_CN_HOST
    {
        int st = cx_tcc_state("kTCCServiceAddressBook");
        if (st == 2) return CX_CN_AUTHORIZED;
        if (st == 1) return CX_CN_DENIED;
    }
#endif
    return CX_CN_NOT_DETERMINED;
}

static void default_path(void) {
    if (g_path[0]) return;
#ifdef CX_CN_HOST
    scopy(g_path, (int)sizeof g_path, "/tmp/mos-contacts-test.moscn");
#else
    {
        const char *home = getenv("HOME");
        if (!home || !home[0]) home = "/Users/Visitante";
        snprintf(g_path, sizeof g_path, "%s/Library/Application Support/com.mos.contacts/contacts.moscn", home);
    }
#endif
}

static int write_all(void *f, const char *s) {
    size_t n = strlen(s);
    return fwrite(s, 1, n, f) == n;
}

static int load_file(void) {
    void *f;
    char line[1024];
    default_path();
    g_n = 0;
    f = fopen(g_path, "r");
    if (!f) return CX_CN_OK;
    if (!fgets(line, (int)sizeof line, f) || strncmp(line, "MOSCN1", 6) != 0) {
        fclose(f);
        return CX_CN_ERR_IO;
    }
    while (fgets(line, (int)sizeof line, f) && g_n < CN_MAX) {
        char *p = line;
        char *fields[12];
        int i;
        cx_cn_contact *c;
        for (i = 0; i < 12; i++) {
            fields[i] = p;
            while (*p && *p != '\t' && *p != '\n' && *p != '\r') p++;
            if (*p == '\t') { *p++ = 0; }
            else { if (*p) *p = 0; }
        }
        c = &g_db[g_n++];
        memset(c, 0, sizeof *c);
        scopy(c->id, 40, fields[0]);
        scopy(c->given, 64, fields[1]);
        scopy(c->family, 64, fields[2]);
        scopy(c->org, 80, fields[3]);
        scopy(c->phone, 64, fields[4]);
        scopy(c->email, 80, fields[5]);
        scopy(c->street, 96, fields[6]);
        scopy(c->city, 48, fields[7]);
        scopy(c->state, 32, fields[8]);
        scopy(c->zip, 16, fields[9]);
        scopy(c->country, 48, fields[10]);
        scopy(c->iso, 8, fields[11]);
    }
    fclose(f);
    return CX_CN_OK;
}

static int ensure_dir(void) {
#ifndef CX_CN_HOST
    char dir[512];
    char *slash;
    default_path();
    scopy(dir, (int)sizeof dir, g_path);
    slash = strrchr(dir, '/');
    if (!slash) return 0;
    *slash = 0;
    {
        char partial[512];
        int n = 0;
        partial[0] = 0;
        for (const char *s = dir; *s && n < (int)sizeof partial - 1; s++) {
            partial[n++] = *s;
            partial[n] = 0;
            if (*s == '/' && n > 1) mkdir(partial, 0755);
        }
        mkdir(partial, 0755);
    }
#endif
    return 0;
}

static int save_file(void) {
    void *f;
    char tmp[540];
    char line[1024];
    int i;
    default_path();
    ensure_dir();
    snprintf(tmp, sizeof tmp, "%s.tmp", g_path);
    f = fopen(tmp, "w");
    if (!f) return CX_CN_ERR_IO;
    if (!write_all(f, "MOSCN1\n")) { fclose(f); return CX_CN_ERR_IO; }
    for (i = 0; i < g_n; i++) {
        cx_cn_contact *c = &g_db[i];
        snprintf(line, sizeof line, "%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\t%s\n",
                 c->id, c->given, c->family, c->org, c->phone, c->email,
                 c->street, c->city, c->state, c->zip, c->country, c->iso);
        if (!write_all(f, line)) {
            fclose(f);
            return CX_CN_ERR_IO;
        }
    }
    if (fclose(f) != 0) return CX_CN_ERR_IO;
    if (rename(tmp, g_path) != 0) return CX_CN_ERR_IO;
    return CX_CN_OK;
}

void cx_cn_reset(void) {
    memset(g_db, 0, sizeof g_db);
    memset(g_op, 0, sizeof g_op);
    g_n = 0;
    g_nop = 0;
    g_seq = 1;
    g_auth_over = 0;
    g_auth_force = 0;
    g_path[0] = 0;
}

void cx_cn_set_path(const char *path) {
    scopy(g_path, (int)sizeof g_path, path);
}

void cx_cn_set_auth(int status) {
    g_auth_force = 1;
    g_auth_over = status;
}

int cx_cn_auth(void) { return auth_now(); }

void cx_cn_begin(void) { g_nop = 0; }

static int push_op(int kind, const cx_cn_contact *in) {
    cn_op_t *o;
    if (g_nop >= CN_OPS) return CX_CN_ERR_FULL;
    o = &g_op[g_nop++];
    memset(o, 0, sizeof *o);
    o->kind = kind;
    if (in) o->c = *in;
    return CX_CN_OK;
}

int cx_cn_stage_add(const cx_cn_contact *in, char id_out[40]) {
    cx_cn_contact c;
    if (!in) return CX_CN_ERR_BAD;
    c = *in;
    if (!c.id[0]) snprintf(c.id, sizeof c.id, "c%d", g_seq++);
    if (id_out) scopy(id_out, 40, c.id);
    return push_op(1, &c);
}

int cx_cn_stage_update(const cx_cn_contact *in) {
    if (!in || !in->id[0]) return CX_CN_ERR_BAD;
    return push_op(2, in);
}

int cx_cn_stage_delete(const char *id) {
    cx_cn_contact c;
    if (!id || !id[0]) return CX_CN_ERR_BAD;
    memset(&c, 0, sizeof c);
    scopy(c.id, 40, id);
    return push_op(3, &c);
}

static int find_id(const char *id) {
    int i;
    for (i = 0; i < g_n; i++) if (same(g_db[i].id, id)) return i;
    return -1;
}

int cx_cn_commit(void) {
    int i, rc;
    if (auth_now() != CX_CN_AUTHORIZED) return CX_CN_ERR_DENIED;
    rc = load_file();
    if (rc != CX_CN_OK) return rc;
    for (i = 0; i < g_nop; i++) {
        cn_op_t *o = &g_op[i];
        int at = find_id(o->c.id);
        if (o->kind == 1) {
            if (at >= 0) g_db[at] = o->c;
            else if (g_n >= CN_MAX) return CX_CN_ERR_FULL;
            else g_db[g_n++] = o->c;
        } else if (o->kind == 2) {
            if (at < 0) return CX_CN_ERR_BAD;
            g_db[at] = o->c;
        } else if (o->kind == 3) {
            if (at < 0) return CX_CN_ERR_BAD;
            g_db[at] = g_db[g_n - 1];
            g_n--;
        }
    }
    rc = save_file();
    if (rc == CX_CN_OK) g_nop = 0;
    return rc;
}

int cx_cn_count(void) {
    if (auth_now() != CX_CN_AUTHORIZED) return -1;
    if (load_file() != CX_CN_OK) return -1;
    return g_n;
}

int cx_cn_get(int index, cx_cn_contact *out) {
    if (auth_now() != CX_CN_AUTHORIZED) return CX_CN_ERR_DENIED;
    if (load_file() != CX_CN_OK) return CX_CN_ERR_IO;
    if (!out || index < 0 || index >= g_n) return CX_CN_ERR_BAD;
    *out = g_db[index];
    return CX_CN_OK;
}

static void add_line(char *out, int cap, int *n, const char *s) {
    int i;
    if (!s || !s[0] || *n >= cap - 1) return;
    if (*n > 0 && *n < cap - 1) out[(*n)++] = '\n';
    for (i = 0; s[i] && *n < cap - 1; i++) out[(*n)++] = s[i];
    out[*n] = 0;
}

int cx_cn_format_postal(const cx_cn_contact *c, char *out, int cap) {
    char mid[160];
    int n = 0;
    int br;
    if (!out || cap < 1) return 0;
    out[0] = 0;
    if (!c) return 0;
    br = (c->iso[0] == 'B' || c->iso[0] == 'b') && (c->iso[1] == 'R' || c->iso[1] == 'r');
    add_line(out, cap, &n, c->street);
    mid[0] = 0;
    if (br) {
        if (c->city[0] && c->state[0]) snprintf(mid, sizeof mid, "%s - %s", c->city, c->state);
        else scopy(mid, (int)sizeof mid, c->city[0] ? c->city : c->state);
    } else if (c->city[0] && c->state[0] && c->zip[0]) {
        snprintf(mid, sizeof mid, "%s, %s %s", c->city, c->state, c->zip);
    } else if (c->city[0] && c->state[0]) {
        snprintf(mid, sizeof mid, "%s, %s", c->city, c->state);
    } else {
        scopy(mid, (int)sizeof mid, c->city[0] ? c->city : c->state);
    }
    add_line(out, cap, &n, mid);
    if (br) add_line(out, cap, &n, c->zip);
    add_line(out, cap, &n, c->country);
    return n;
}
