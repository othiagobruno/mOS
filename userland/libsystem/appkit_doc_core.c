/* mOS — núcleo do NSDocument (ver appkit_doc_core.h). Teste de host: -DCX_DC_HOST. */
#ifdef CX_DC_HOST
#include <stdlib.h>
#include <string.h>
#else
#include "sys.h"
#include "libc.h"
void free(void *p);
#endif
#include "appkit_doc_core.h"

void dc_update(dc_count_t *c, int type) {
    int disc = (type & DC_DISCARDABLE) != 0;
    switch (type & ~DC_DISCARDABLE) {
    case DC_DONE: case DC_REDONE: c->change++; break;
    case DC_UNDONE: c->change--; break;
    case DC_CLEARED: c->change = 0; c->saved_at_autosave = 0; c->read_other = 0; break;
    case DC_READ_OTHER: c->read_other = 1; break;
    case DC_AUTOSAVED: c->saved_at_autosave = c->change; break;
    }
    c->discardable = disc && c->change != 0;
}
int dc_edited(const dc_count_t *c) { return c->change != 0 || c->read_other; }
int dc_has_unautosaved(const dc_count_t *c) { return c->change != c->saved_at_autosave; }

static int eq(const char *a, const char *b) { return a && b && !strcmp(a, b); }
void dc_untitled_name(const char *base, const char *const *used, int nused, char *out, int cap) {
    size_t bl = strlen(base);
    for (int k = 1; k < 100000; k++) {
        if ((int)bl + 8 >= cap) { out[0] = 0; return; }
        memcpy(out, base, bl);
        out[bl] = 0;
        if (k > 1) {
            char num[12]; int n = 0, v = k; char r[12]; int rl = 0;
            while (v) { r[rl++] = (char)('0' + v % 10); v /= 10; }
            while (rl) num[n++] = r[--rl];
            out[bl] = ' ';
            memcpy(out + bl + 1, num, (size_t)n);
            out[bl + 1 + n] = 0;
        }
        int clash = 0;
        for (int i = 0; i < nused && !clash; i++) clash = eq(used[i], out);
        if (!clash) return;
    }
}

void dc_mru_init(dc_mru_t *m, int max) { memset(m, 0, sizeof *m); m->max = max > DC_MRU_CAP ? DC_MRU_CAP : max < 0 ? 0 : max; }
static void mru_trim(dc_mru_t *m) { while (m->n > m->max) free(m->item[--m->n]); }
void dc_mru_add(dc_mru_t *m, const char *s) {
    if (!s || !*s || m->max <= 0) return;
    int at = -1;
    for (int i = 0; i < m->n; i++) if (!strcmp(m->item[i], s)) { at = i; break; }
    char *v;
    if (at >= 0) { v = m->item[at]; memmove(&m->item[1], &m->item[0], sizeof(char *) * (size_t)at); }
    else {
        size_t l = strlen(s);
        v = malloc(l + 1);
        if (!v) return;
        memcpy(v, s, l + 1);
        if (m->n == DC_MRU_CAP) free(m->item[--m->n]);
        memmove(&m->item[1], &m->item[0], sizeof(char *) * (size_t)m->n);
        m->n++;
    }
    m->item[0] = v;
    mru_trim(m);
}
void dc_mru_set_max(dc_mru_t *m, int max) { m->max = max > DC_MRU_CAP ? DC_MRU_CAP : max < 0 ? 0 : max; mru_trim(m); }
void dc_mru_clear(dc_mru_t *m) { while (m->n) free(m->item[--m->n]); }

static void cpy(char *d, const char *s, size_t cap) { size_t l = s ? strlen(s) : 0; if (l >= cap) l = cap - 1; if (l) memcpy(d, s, l); d[l] = 0; }
void dc_type_init(dc_type_t *t, const char *name, const char *role, const char *cls) {
    memset(t, 0, sizeof *t);
    cpy(t->name, name, sizeof t->name);
    cpy(t->cls, cls, sizeof t->cls);
    t->role = !role ? DC_ROLE_EDITOR : eq(role, "Editor") ? DC_ROLE_EDITOR : eq(role, "Viewer") ? DC_ROLE_VIEWER : eq(role, "Shell") ? DC_ROLE_SHELL : DC_ROLE_NONE;
}
static void lower_cpy(char *d, const char *s, size_t cap) {
    size_t i = 0;
    if (s && *s == '.') s++;
    for (; s && s[i] && i + 1 < cap; i++) d[i] = (char)(s[i] >= 'A' && s[i] <= 'Z' ? s[i] + 32 : s[i]);
    d[i] = 0;
}
void dc_type_add_ext(dc_type_t *t, const char *ext) { if (t->next < 16 && ext && *ext) lower_cpy(t->ext[t->next++], ext, sizeof t->ext[0]); }
void dc_type_add_uti(dc_type_t *t, const char *uti) { if (t->nuti < 8 && uti && *uti) cpy(t->uti[t->nuti++], uti, sizeof t->uti[0]); }
int dc_type_for_ext(const dc_type_t *t, int n, const char *ext) {
    char e[24];
    lower_cpy(e, ext ? ext : "", sizeof e);
    int best = -1;
    for (int i = 0; i < n; i++) for (int k = 0; k < t[i].next; k++) {
        if (!strcmp(t[i].ext[k], e) || !strcmp(t[i].ext[k], "*")) {
            if (best < 0 || (t[i].role == DC_ROLE_EDITOR && t[best].role != DC_ROLE_EDITOR)) best = i;
            break;
        }
    }
    return best;
}
int dc_type_named(const dc_type_t *t, int n, const char *name) {
    if (!name) return -1;
    for (int i = 0; i < n; i++) if (eq(t[i].name, name)) return i;
    for (int i = 0; i < n; i++) for (int k = 0; k < t[i].nuti; k++) if (eq(t[i].uti[k], name)) return i;
    return -1;
}
int dc_default_type(const dc_type_t *t, int n) {
    for (int i = 0; i < n; i++) if (t[i].role == DC_ROLE_EDITOR && t[i].cls[0]) return i;
    for (int i = 0; i < n; i++) if (t[i].cls[0]) return i;
    return n > 0 ? 0 : -1;
}
int dc_type_writable(const dc_type_t *t) { return t && t->role == DC_ROLE_EDITOR; }

void dc_window_title(const char *display, int edited, char *out, int cap) {
    cpy(out, display ? display : "", (size_t)cap);
    if (!edited) return;
    const char *suf = " \xE2\x80\x94 Editado";
    size_t l = strlen(out), sl = strlen(suf);
    if ((int)(l + sl) < cap) memcpy(out + l, suf, sl + 1);
}
void dc_display_name(const char *path, int hide_ext, char *out, int cap) {
    const char *b = path ? path : "";
    size_t n = strlen(b);
    while (n > 1 && b[n - 1] == '/') n--;
    const char *s = b;
    for (size_t i = 0; i < n; i++) if (b[i] == '/' && i + 1 < n) s = b + i + 1;
    size_t l = (size_t)(b + n - s);
    if (hide_ext) { for (size_t i = l; i > 1; i--) if (s[i - 1] == '.') { l = i - 1; break; } }
    if ((int)l >= cap) l = (size_t)cap - 1;
    memcpy(out, s, l);
    out[l] = 0;
}
