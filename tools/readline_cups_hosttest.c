/*
 * mOS — teste de host de userland/libsystem/readline_mos.c e cups_corpus.c (prefixo mos_).
 * Rodar: tools/readline_cups_hosttest.sh. Histórico (memória e arquivo), readline lendo de um fluxo, completion,
 * callback e as opções do CUPS.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef __APPLE__
FILE *__stdinp, *__stdoutp;     /* nomes do libc do macOS; o teste sempre passa rl_instream/rl_outstream */
#endif

typedef struct { char *line; char *timestamp; void *data; } HIST_ENTRY;
typedef struct { HIST_ENTRY **entries; int offset, length, size, flags; } HISTORY_STATE;
extern FILE *mos_rl_instream, *mos_rl_outstream;
extern int mos_history_length, mos_history_base;
extern const char *mos_rl_library_version;
void mos_add_history(const char *);
HIST_ENTRY *mos_history_get(int);
HIST_ENTRY *mos_remove_history(int);
HIST_ENTRY *mos_replace_history_entry(int, const char *, void *);
void *mos_free_history_entry(HIST_ENTRY *);
void mos_clear_history(void);
HISTORY_STATE *mos_history_get_history_state(void);
int mos_write_history(const char *);
int mos_read_history(const char *);
int mos_history_truncate_file(const char *, int);
void mos_stifle_history(int);
char *mos_readline(const char *);
char **mos_rl_completion_matches(const char *, char *(*)(const char *, int));
void mos_rl_callback_handler_install(const char *, void (*)(char *));
void mos_rl_callback_read_char(void);
typedef struct { char *name, *value; } cups_option_t;
int mos_cupsParseOptions(const char *, int, cups_option_t **);
const char *mos_cupsGetOption(const char *, int, cups_option_t *);
int mos_cupsAddOption(const char *, const char *, int, cups_option_t **);
int mos_cupsRemoveOption(const char *, int, cups_option_t **);
void mos_cupsFreeOptions(int, cups_option_t *);
int mos_cupsGetDests2(void *, void **);
void *mos_ppdOpenFile(const char *);
int mos_ppdLastError(int *);
int mos_httpSeparateURI(int, const char *, char *, int, char *, int, char *, int, int *, char *, int);

static int fails, total;
#define CK(c, m) do { total++; if (!(c)) { fails++; printf("FALHA: %s\n", m); } } while (0)
static const char *words[] = {"print", "printf", "private", "public", 0};
static char *gen(const char *t, int st) {
    static int i;
    if (!st) i = 0;
    for (; words[i]; i++) if (!strncmp(words[i], t, strlen(t))) return strdup(words[i++]);
    return 0;
}
static char *got_cb;
static void on_line(char *l) { got_cb = l; }

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : "/tmp";
    CK(!strncmp(mos_rl_library_version, "EditLine wrapper", 16), "rl_library_version do libedit");
    mos_add_history("um"); mos_add_history("dois"); mos_add_history("tres");
    CK(mos_history_length == 3 && !strcmp(mos_history_get(mos_history_base)->line, "um"), "history_get(base)");
    CK(!strcmp(mos_history_get(mos_history_base + 2)->line, "tres") && !mos_history_get(mos_history_base + 3), "history_get fim");
    HIST_ENTRY *old = mos_replace_history_entry(1, "DOIS", 0);
    CK(old && !strcmp(old->line, "dois") && !strcmp(mos_history_get(mos_history_base + 1)->line, "DOIS"), "replace_history_entry");
    mos_free_history_entry(old);
    HISTORY_STATE *st = mos_history_get_history_state();
    CK(st && st->length == 3, "history_get_history_state");
    free(st);
    char path[512];
    snprintf(path, sizeof path, "%s/hist.txt", dir);
    CK(mos_write_history(path) == 0, "write_history");
    mos_clear_history();
    CK(mos_history_length == 0, "clear_history");
    CK(mos_read_history(path) == 0 && mos_history_length == 3 && !strcmp(mos_history_get(mos_history_base + 1)->line, "DOIS"), "read_history");
    CK(mos_history_truncate_file(path, 1) == 0, "history_truncate_file");
    mos_clear_history();
    mos_read_history(path);
    CK(mos_history_length == 1 && !strcmp(mos_history_get(mos_history_base)->line, "tres"), "truncate manteve a última");
    mos_free_history_entry(mos_remove_history(0));
    CK(mos_history_length == 0, "remove_history");
    mos_stifle_history(2);
    mos_add_history("a"); mos_add_history("b"); mos_add_history("c");
    CK(mos_history_length == 2 && !strcmp(mos_history_get(mos_history_base)->line, "b"), "stifle_history descarta a mais antiga");
    unlink(path);
    /* readline de um fluxo */
    char src[] = "primeira linha\nsegunda\r\n";
    mos_rl_instream = fmemopen(src, strlen(src), "r");
    mos_rl_outstream = fopen("/dev/null", "w");
    char *l1 = mos_readline("> "), *l2 = mos_readline("> "), *l3 = mos_readline("> ");
    CK(l1 && !strcmp(l1, "primeira linha") && l2 && !strcmp(l2, "segunda") && !l3, "readline lê linhas e devolve NULL no EOF");
    free(l1); free(l2);
    fclose(mos_rl_instream);
    /* completion */
    char **m = mos_rl_completion_matches("pri", gen);
    CK(m && !strcmp(m[0], "pri") && m[1] && m[3] && !m[4], "rl_completion_matches: prefixo comum e 3 itens");
    for (int i = 0; m && m[i]; i++) free(m[i]);
    free(m);
    m = mos_rl_completion_matches("pub", gen);
    CK(m && !strcmp(m[0], "public") && !m[2], "rl_completion_matches: único");
    for (int i = 0; m && m[i]; i++) free(m[i]);
    free(m);
    /* callback */
    char src2[] = "oi\n";
    mos_rl_instream = fmemopen(src2, 3, "r");
    mos_rl_callback_handler_install("", on_line);
    for (int i = 0; i < 3 && !got_cb; i++) mos_rl_callback_read_char();
    CK(got_cb && !strcmp(got_cb, "oi"), "rl_callback_read_char entrega a linha");
    free(got_cb);
    fclose(mos_rl_instream);
    /* CUPS */
    cups_option_t *o = 0;
    int n = mos_cupsParseOptions("media=A4 sides='two-sided-long-edge' nocollate fit-to-page copies=\"2\"", 0, &o);
    CK(n == 5, "cupsParseOptions conta 5");
    CK(!strcmp(mos_cupsGetOption("media", n, o), "A4") && !strcmp(mos_cupsGetOption("sides", n, o), "two-sided-long-edge"), "cupsGetOption com aspas");
    CK(!strcmp(mos_cupsGetOption("collate", n, o), "false") && !strcmp(mos_cupsGetOption("fit-to-page", n, o), "true"), "cupsParseOptions booleanas");
    CK(!strcmp(mos_cupsGetOption("COPIES", n, o), "2"), "cupsGetOption sem caixa");
    n = mos_cupsAddOption("media", "Letter", n, &o);
    CK(n == 5 && !strcmp(mos_cupsGetOption("media", n, o), "Letter"), "cupsAddOption substitui");
    n = mos_cupsRemoveOption("media", n, &o);
    CK(n == 4 && !mos_cupsGetOption("media", n, o), "cupsRemoveOption");
    mos_cupsFreeOptions(n, o);
    void *dests = (void *)1;
    CK(mos_cupsGetDests2(0, &dests) == 0 && dests == 0, "sem impressoras");
    CK(!mos_ppdOpenFile("/x.ppd") && mos_ppdLastError(0) == 1, "ppdOpenFile falha com PPD_FILE_OPEN_ERROR");
    char sch[16], usr[16], hst[64], res[64]; int port = 0;
    CK(mos_httpSeparateURI(0, "ipp://joao@impressora.local:631/printers/x", sch, 16, usr, 16, hst, 64, &port, res, 64) == 0 &&
       !strcmp(sch, "ipp") && !strcmp(usr, "joao") && !strcmp(hst, "impressora.local") && port == 631 && !strcmp(res, "/printers/x"),
       "httpSeparateURI");
    printf("readline-cups: %d/%d ok\n", total - fails, total);
    return fails ? 1 : 0;
}
