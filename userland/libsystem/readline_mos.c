/*
 * mOS libSystem — a API de readline do libedit (/usr/lib/libedit.3.dylib), que o módulo readline do Python embutido
 * em anki, blender, calibre, capcut e outros chama já no import (docs/lacunas/leonardo.md). rl_library_version diz
 * "EditLine wrapper", como no macOS, e o Python escolhe o modo libedit.
 *   - readline(): mostra o prompt e lê uma linha de rl_instream (o terminal do mOS já edita a linha no modo canônico);
 *   - histórico de verdade: add/remove/replace/get, estado, stifle, navegação e arquivo (read/write/truncate) no formato
 *     de texto do GNU readline, que o libedit também lê;
 *   - rl_completion_matches com o prefixo comum; rl_filename_completion_function lista o diretório;
 *   - a interface de callback (rl_callback_handler_install / rl_callback_read_char) junta caracteres até o \n;
 *   - teclas, variáveis e arquivos de inicialização são aceitos e não mudam nada (não há editor de linha próprio).
 */
#include "sys.h"

typedef void FILE;
void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void *realloc(void *p, size_t n);
void free(void *p);
size_t strlen(const char *s);
char *strdup(const char *s);
int strncmp(const char *a, const char *b, size_t n);
void *memcpy(void *d, const void *s, size_t n);
void *memmove(void *d, const void *s, size_t n);
int fputs(const char *s, FILE *f);
int fflush(FILE *f);
int fgetc(FILE *f);
FILE *fopen(const char *p, const char *m);
int fclose(FILE *f);
char *fgets(char *b, int n, FILE *f);
void *opendir(const char *p);
void *readdir(void *d);
int closedir(void *d);
extern FILE *__stdinp, *__stdoutp;

typedef int rl_command_func_t(int, int);
typedef char *rl_compentry_func_t(const char *, int);
typedef char **rl_completion_func_t(const char *, int, int);
typedef int rl_hook_func_t(void);
typedef void rl_vcpfunc_t(char *);
typedef void rl_compdisp_func_t(char **, int, int);

/* ---- variáveis públicas */
const char *rl_library_version = "EditLine wrapper";
int rl_readline_version = 0x0402;
char *rl_readline_name = "";
FILE *rl_instream, *rl_outstream;
static char line_store[4096];
char *rl_line_buffer = line_store;
int rl_point, rl_end, rl_done, rl_mark;
char *rl_prompt;
rl_completion_func_t *rl_attempted_completion_function;
rl_compentry_func_t *rl_completion_entry_function;
rl_hook_func_t *rl_startup_hook, *rl_pre_input_hook, *rl_event_hook;
rl_compdisp_func_t *rl_completion_display_matches_hook;
int rl_completion_append_character = ' ';
int rl_attempted_completion_over, rl_completion_type, rl_completion_query_items = 100;
int rl_completion_suppress_append, rl_completion_word_break_hook, rl_catch_signals = 1, rl_catch_sigwinch = 1;
int rl_inhibit_completion, rl_erase_empty_line, rl_already_prompted, rl_filename_completion_desired;
char *rl_basic_word_break_characters = " \t\n\"\\'`@$><=;|&{(";
char *rl_completer_word_break_characters = " \t\n\"\\'`@$><=;|&{(";
char *rl_completer_quote_characters;
char *rl_special_prefixes;
/* keymaps: o libedit expõe ponteiros; nenhuma tecla é religada de verdade */
static void *km_std[256], *km_meta[256], *km_ctlx[256];
void *emacs_standard_keymap = km_std, *emacs_meta_keymap = km_meta, *emacs_ctlx_keymap = km_ctlx;

/* ---- histórico */
typedef struct { char *line; char *timestamp; void *data; } HIST_ENTRY;
typedef struct { HIST_ENTRY **entries; int offset, length, size, flags; } HISTORY_STATE;
static HIST_ENTRY **hist;
static int hist_cap, hist_pos;
int history_length, history_base = 1, history_max_entries, history_stifled_;
static HIST_ENTRY *mk_entry(const char *line, void *data) {
    HIST_ENTRY *e = calloc(1, sizeof *e);
    if (!e) return 0;
    e->line = strdup(line ? line : "");
    e->timestamp = strdup("");
    e->data = data;
    if (!e->line || !e->timestamp) { free(e->line); free(e->timestamp); free(e); return 0; }
    return e;
}
static void free_entry(HIST_ENTRY *e) { if (e) { free(e->line); free(e->timestamp); free(e); } }
void using_history(void) { hist_pos = history_length; }
void add_history(const char *line) {
    if (!line) return;
    if (history_stifled_ && history_max_entries > 0 && history_length >= history_max_entries) {
        free_entry(hist[0]);
        memmove(hist, hist + 1, (size_t)(history_length - 1) * sizeof *hist);
        history_length--;
        history_base++;
    }
    if (history_length == hist_cap) {
        int nc = hist_cap ? hist_cap * 2 : 64;
        HIST_ENTRY **n = realloc(hist, (size_t)(nc + 1) * sizeof *n);
        if (!n) return;
        hist = n; hist_cap = nc;
    }
    HIST_ENTRY *e = mk_entry(line, 0);
    if (!e) return;
    hist[history_length++] = e;
    hist[history_length] = 0;
    hist_pos = history_length;
}
void add_history_time(const char *t) { if (history_length && t) { HIST_ENTRY *e = hist[history_length - 1]; char *n = strdup(t); if (n) { free(e->timestamp); e->timestamp = n; } } }
/* history_get usa o índice absoluto: history_base é o primeiro */
HIST_ENTRY *history_get(int off) {
    int i = off - history_base;
    return i >= 0 && i < history_length ? hist[i] : 0;
}
HIST_ENTRY *remove_history(int which) {
    if (which < 0 || which >= history_length) return 0;
    HIST_ENTRY *e = hist[which];
    memmove(hist + which, hist + which + 1, (size_t)(history_length - which - 1) * sizeof *hist);
    history_length--;
    hist[history_length] = 0;
    if (hist_pos > history_length) hist_pos = history_length;
    return e;
}
void *free_history_entry(HIST_ENTRY *e) { void *d = e ? e->data : 0; free_entry(e); return d; }
HIST_ENTRY *replace_history_entry(int which, const char *line, void *data) {
    if (which < 0 || which >= history_length) return 0;
    HIST_ENTRY *old = hist[which], *e = mk_entry(line, data);
    if (!e) return 0;
    hist[which] = e;
    return old;      /* o chamador libera a antiga (free_history_entry) */
}
void clear_history(void) {
    for (int i = 0; i < history_length; i++) free_entry(hist[i]);
    history_length = 0;
    hist_pos = 0;
    if (hist) hist[0] = 0;
}
HISTORY_STATE *history_get_history_state(void) {
    HISTORY_STATE *s = calloc(1, sizeof *s);
    if (!s) return 0;
    s->entries = hist;
    s->offset = hist_pos;
    s->length = history_length;
    s->size = hist_cap;
    s->flags = history_stifled_ ? 1 : 0;
    return s;
}
void history_set_history_state(HISTORY_STATE *s) { if (s) hist_pos = s->offset; }
HIST_ENTRY **history_list(void) { return hist; }
int where_history(void) { return hist_pos; }
int history_set_pos(int pos) { if (pos < 0 || pos > history_length) return 0; hist_pos = pos; return 1; }
HIST_ENTRY *current_history(void) { return hist_pos < history_length ? hist[hist_pos] : 0; }
HIST_ENTRY *previous_history(void) { if (hist_pos <= 0) return 0; hist_pos--; return hist[hist_pos]; }
HIST_ENTRY *next_history(void) { if (hist_pos >= history_length - 1) { hist_pos = history_length; return 0; } hist_pos++; return hist[hist_pos]; }
void stifle_history(int max) {
    if (max < 0) max = 0;
    while (history_length > max) { free_entry(remove_history(0)); history_base++; }
    history_max_entries = max;
    history_stifled_ = 1;
}
int unstifle_history(void) { int m = history_stifled_ ? history_max_entries : -1; history_stifled_ = 0; return m; }
int history_is_stifled(void) { return history_stifled_; }
int history_total_bytes(void) { int t = 0; for (int i = 0; i < history_length; i++) t += (int)strlen(hist[i]->line); return t; }
int history_search(const char *s, int dir) {
    if (!s) return -1;
    size_t n = strlen(s);
    for (int i = hist_pos; i >= 0 && i < history_length; i += dir < 0 ? -1 : 1) {
        const char *l = hist[i]->line;
        for (size_t k = 0; l[k]; k++) if (!strncmp(l + k, s, n)) { hist_pos = i; return (int)k; }
    }
    return -1;
}
int history_search_prefix(const char *s, int dir) {
    if (!s) return -1;
    size_t n = strlen(s);
    for (int i = hist_pos; i >= 0 && i < history_length; i += dir < 0 ? -1 : 1)
        if (!strncmp(hist[i]->line, s, n)) { hist_pos = i; return 0; }
    return -1;
}
/* arquivo: uma linha por entrada, como o GNU readline grava (o libedit lê os dois formatos) */
static const char *hist_path(const char *f) {
    static char b[1024];
    if (f) return f;
    extern char *getenv(const char *);
    const char *h = getenv("HOME");
    size_t n = h ? strlen(h) : 0;
    if (n > sizeof b - 16) n = 0;
    memcpy(b, h ? h : "", n);
    memcpy(b + n, "/.history", 10);
    return b;
}
int read_history_range(const char *file, int from, int to) {
    FILE *f = fopen(hist_path(file), "r");
    if (!f) return 2;                              /* ENOENT, como o readline devolve errno */
    char b[4096];
    int n = 0;
    while (fgets(b, sizeof b, f)) {
        size_t l = strlen(b);
        while (l && (b[l - 1] == '\n' || b[l - 1] == '\r')) b[--l] = 0;
        if (n == 0 && !strncmp(b, "_HiStOrY_V2_", 12)) continue;    /* cabeçalho do libedit */
        if (n >= from && (to < 0 || n < to)) add_history(b);
        n++;
    }
    fclose(f);
    return 0;
}
int read_history(const char *file) { return read_history_range(file, 0, -1); }
static int write_range(const char *file, const char *mode, int first) {
    FILE *f = fopen(hist_path(file), mode);
    if (!f) return 13;                             /* EACCES */
    for (int i = first; i < history_length; i++) { fputs(hist[i]->line, f); fputs("\n", f); }
    fclose(f);
    return 0;
}
int write_history(const char *file) { return write_range(file, "w", 0); }
int append_history(int n, const char *file) { return write_range(file, "a", history_length - n < 0 ? 0 : history_length - n); }
int history_truncate_file(const char *file, int keep) {
    const char *p = hist_path(file);
    FILE *f = fopen(p, "r");
    if (!f) return 2;
    char **lines = 0;
    int n = 0, cap = 0;
    char b[4096];
    while (fgets(b, sizeof b, f)) {
        if (n == cap) { cap = cap ? cap * 2 : 64; char **nl = realloc(lines, (size_t)cap * sizeof *nl); if (!nl) break; lines = nl; }
        lines[n] = strdup(b);
        if (lines[n]) n++;
    }
    fclose(f);
    int first = keep < 0 ? 0 : n > keep ? n - keep : 0;
    f = fopen(p, "w");
    for (int i = 0; i < n; i++) { if (f && i >= first) fputs(lines[i], f); free(lines[i]); }
    free(lines);
    if (f) fclose(f);
    return f ? 0 : 13;
}

/* ---- leitura de linha */
static FILE *in(void) { return rl_instream ? rl_instream : __stdinp; }
static FILE *out(void) { return rl_outstream ? rl_outstream : __stdoutp; }
static void set_line(const char *s, size_t n) {
    if (n >= sizeof line_store) n = sizeof line_store - 1;
    memcpy(line_store, s, n);
    line_store[n] = 0;
    rl_end = rl_point = (int)n;
}
char *readline(const char *prompt) {
    rl_done = 0;
    set_line("", 0);
    if (rl_startup_hook) rl_startup_hook();
    if (prompt && *prompt && !rl_already_prompted) { fputs(prompt, out()); fflush(out()); }
    if (rl_pre_input_hook) rl_pre_input_hook();
    size_t cap = 256, n = 0;
    char *b = malloc(cap);
    if (!b) return 0;
    for (;;) {
        int c = fgetc(in());
        if (c < 0) { if (!n) { free(b); return 0; } break; }    /* EOF sem nada: NULL, como o readline */
        if (c == '\n') break;
        if (n + 1 >= cap) { cap *= 2; char *nb = realloc(b, cap); if (!nb) { free(b); return 0; } b = nb; }
        b[n++] = (char)c;
    }
    if (n && b[n - 1] == '\r') n--;
    b[n] = 0;
    set_line(b, n);
    rl_done = 1;
    return b;
}
int rl_initialize(void) { set_line("", 0); return 0; }
int rl_insert_text(const char *t) {
    if (!t) return 0;
    size_t n = strlen(t), have = (size_t)rl_end;
    if (have + n >= sizeof line_store) n = sizeof line_store - 1 - have;
    memmove(line_store + rl_point + n, line_store + rl_point, have - (size_t)rl_point + 1);
    memcpy(line_store + rl_point, t, n);
    rl_point += (int)n;
    rl_end += (int)n;
    return (int)n;
}
int rl_insert(int count, int key) { char c[2] = {(char)key, 0}; for (int i = 0; i < count; i++) rl_insert_text(c); return 0; }
int rl_delete_text(int from, int to) {
    if (from < 0) from = 0;
    if (to > rl_end) to = rl_end;
    if (from >= to) return 0;
    memmove(line_store + from, line_store + to, (size_t)(rl_end - to) + 1);
    rl_end -= to - from;
    if (rl_point > rl_end) rl_point = rl_end;
    return to - from;
}
void rl_replace_line(const char *t, int clear_undo) { (void)clear_undo; set_line(t ? t : "", t ? strlen(t) : 0); }
char *rl_copy_text(int from, int to) {
    if (from < 0) from = 0;
    if (to > rl_end) to = rl_end;
    if (to < from) to = from;
    char *r = malloc((size_t)(to - from) + 1);
    if (!r) return 0;
    memcpy(r, line_store + from, (size_t)(to - from));
    r[to - from] = 0;
    return r;
}
int rl_redisplay(void) { return 0; }
int rl_forced_update_display(void) { return 0; }
int rl_on_new_line(void) { return 0; }
int rl_crlf(void) { fputs("\n", out()); return 0; }
int rl_ding(void) { fputs("\a", out()); fflush(out()); return 0; }
void rl_prep_terminal(int meta) { (void)meta; }
void rl_deprep_terminal(void) {}
void rl_free_line_state(void) { set_line("", 0); }
void rl_cleanup_after_signal(void) {}
void rl_reset_after_signal(void) {}
void rl_resize_terminal(void) {}
void rl_set_screen_size(int r, int c) { (void)r; (void)c; }
void rl_get_screen_size(int *r, int *c) { if (r) *r = 24; if (c) *c = 80; }
int rl_variable_bind(const char *var, const char *val) { (void)var; (void)val; return 0; }
const char *rl_variable_value(const char *var) { (void)var; return 0; }
int rl_parse_and_bind(char *line) { (void)line; return 0; }
int rl_read_init_file(const char *f) { (void)f; return 0; }
int rl_bind_key(int key, rl_command_func_t *fn) { if (key >= 0 && key < 256) km_std[key] = (void *)fn; return 0; }
int rl_bind_key_in_map(int key, rl_command_func_t *fn, void *map) { if (key >= 0 && key < 256 && map) ((void **)map)[key] = (void *)fn; return 0; }
int rl_unbind_key(int key) { if (key >= 0 && key < 256) km_std[key] = 0; return 0; }
int rl_add_defun(const char *name, rl_command_func_t *fn, int key) { (void)name; return key >= 0 ? rl_bind_key(key, fn) : 0; }
void *rl_get_keymap(void) { return km_std; }
void rl_set_keymap(void *m) { (void)m; }
void *rl_make_bare_keymap(void) { return calloc(256, sizeof(void *)); }
int rl_complete(int ignore, int key) { (void)ignore; (void)key; return 0; }
int rl_insert_completions(int ignore, int key) { (void)ignore; (void)key; return 0; }
int rl_reset_terminal(const char *t) { (void)t; return 0; }
int rl_set_prompt(const char *p) { (void)p; return 0; }
int rl_kill_text(int from, int to) { return rl_delete_text(from, to); }
void rl_display_match_list(char **m, int n, int max) {
    (void)max;
    for (int i = 1; m && i <= n; i++) { fputs(m[i], out()); fputs("\n", out()); }
}
/* matches[0] é o maior prefixo comum; a lista termina em NULL */
char **rl_completion_matches(const char *text, rl_compentry_func_t *gen) {
    if (!gen) return 0;
    char **m = 0;
    int n = 0, cap = 0;
    for (int st = 0;; st++) {
        char *s = gen(text ? text : "", st);
        if (!s) break;
        if (n + 2 >= cap) { cap = cap ? cap * 2 : 16; char **nm = realloc(m, (size_t)cap * sizeof *nm); if (!nm) { free(s); break; } m = nm; }
        m[++n] = s;
    }
    if (!n) { free(m); return 0; }
    size_t l = strlen(m[1]);
    for (int i = 2; i <= n; i++) { size_t k = 0; while (k < l && m[i][k] == m[1][k]) k++; l = k; }
    m[0] = malloc(l + 1);
    if (m[0]) { memcpy(m[0], m[1], l); m[0][l] = 0; }
    m[n + 1] = 0;
    return m;
}
char **completion_matches(const char *text, rl_compentry_func_t *gen) { return rl_completion_matches(text, gen); }
/* entradas do diretório que começam com o texto; o estado 0 reabre */
typedef struct { u64 ino; u64 seekoff; u16 reclen, namlen; u8 type; char name[1024]; } cx_dirent;
char *rl_filename_completion_function(const char *text, int state) {
    static void *d;
    static char dir[1024], pre[256];
    static size_t plen, dlen;
    if (!state) {
        if (d) { closedir(d); d = 0; }
        const char *t = text ? text : "", *slash = 0;
        for (const char *p = t; *p; p++) if (*p == '/') slash = p;
        dlen = slash ? (size_t)(slash - t + 1) : 0;
        if (dlen >= sizeof dir) return 0;
        memcpy(dir, t, dlen);
        dir[dlen] = 0;
        plen = strlen(t + dlen);
        if (plen >= sizeof pre) return 0;
        memcpy(pre, t + dlen, plen + 1);
        d = opendir(dlen ? dir : ".");
        if (!d) return 0;
    }
    if (!d) return 0;
    for (cx_dirent *e; (e = readdir(d));) {
        if (e->name[0] == '.' && (!e->name[1] || (e->name[1] == '.' && !e->name[2]))) continue;
        if (strncmp(e->name, pre, plen)) continue;
        size_t nl = strlen(e->name);
        char *r = malloc(dlen + nl + 1);
        if (!r) return 0;
        memcpy(r, dir, dlen);
        memcpy(r + dlen, e->name, nl + 1);
        return r;
    }
    closedir(d);
    d = 0;
    return 0;
}
char *filename_completion_function(const char *t, int s) { return rl_filename_completion_function(t, s); }
char *rl_username_completion_function(const char *t, int s) { (void)t; (void)s; return 0; }

/* ---- interface de callback */
static rl_vcpfunc_t *cb_handler;
static char cb_buf[4096];
static size_t cb_n;
void rl_callback_handler_install(const char *prompt, rl_vcpfunc_t *h) {
    cb_handler = h;
    cb_n = 0;
    set_line("", 0);
    if (rl_startup_hook) rl_startup_hook();
    if (prompt && *prompt) { fputs(prompt, out()); fflush(out()); }
    if (rl_pre_input_hook) rl_pre_input_hook();
}
void rl_callback_read_char(void) {
    int c = fgetc(in());
    if (!cb_handler) return;
    if (c < 0) { rl_vcpfunc_t *h = cb_handler; h(0); return; }    /* EOF: o handler recebe NULL */
    if (c == '\n' || c == '\r') {
        char *l = malloc(cb_n + 1);
        if (!l) return;
        memcpy(l, cb_buf, cb_n);
        l[cb_n] = 0;
        cb_n = 0;
        set_line("", 0);
        cb_handler(l);
        return;
    }
    if (cb_n + 1 < sizeof cb_buf) cb_buf[cb_n++] = (char)c;
    set_line(cb_buf, cb_n);
}
void rl_callback_handler_remove(void) { cb_handler = 0; cb_n = 0; }
void rl_callback_sigcleanup(void) { cb_n = 0; }
