/* mOS — osascript(1): roda AppleScript, como o /usr/bin/osascript do macOS.
 *   osascript [-l AppleScript] [-i] [-s {h|s}{e|o}] [-e linha ...] [arquivo | -] [argumento ...]
 * Várias -e viram linhas do mesmo script; sem -e, o script vem do arquivo (ou da entrada padrão com "-" ou nada).
 * Os argumentos vão para o "on run argv". O resultado sai na saída padrão (-s h legível, -s s como código); erros
 * saem no stderr ("linha:coluna: execution error: mensagem (número)", código 1), ou na saída padrão com -s o.
 * -i: modo interativo, um comando por linha. Só AppleScript: -l JavaScript dá erro.
 * O interpretador e as ações do sistema estão no libSystem (userland/libsystem/applescript.c, SPI
 * cx_osascript_run); o TCC vê o osascript como com.mos.osascript. Teste de host: tools/osascripttest.sh. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int cx_osascript_run(const char *src, const char *const *argv, int argc, int source_form, const char *me,
                     char **out, char *err, int errcap);
void cx_tcc_set_identity(const char *bundle);

static int usage(void) {
    fprintf(stderr, "usage: osascript [-l language] [-e script] [-i] [-s {ehso}] [programfile] [argument ...]\n");
    return 1;
}

static char *slurp(FILE *f) {
    size_t cap = 4096, n = 0;
    char *b = malloc(cap);
    if (!b) return 0;
    size_t r;
    while ((r = fread(b + n, 1, cap - n - 1, f)) > 0) {
        n += r;
        if (n + 1 >= cap) { char *nb = realloc(b, cap *= 2); if (!nb) { free(b); return 0; } b = nb; }
    }
    b[n] = 0;
    return b;
}

static int run_one(const char *src, const char *const *args, int nargs, int source_form, int err_to_out, const char *me) {
    char *out = 0, err[600];
    int rc = cx_osascript_run(src, args, nargs, source_form, me, &out, err, sizeof err);
    if (rc) {
        fprintf(err_to_out ? stdout : stderr, "%s\n", err);
        return 1;
    }
    if (out) { printf("%s\n", out); free(out); }
    fflush(stdout);
    return 0;
}

int main(int argc, char **argv) {
    cx_tcc_set_identity("com.mos.osascript");
    size_t cap = 256, len = 0;
    char *script = calloc(1, cap);
    int have_e = 0, interactive = 0, source_form = 0, err_to_out = 0, i = 1;
    if (!script) return 1;
    for (; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "--")) { i++; break; }
        if (a[0] != '-' || !a[1]) break;
        if (!strcmp(a, "-e")) {
            if (++i >= argc) return usage();
            size_t l = strlen(argv[i]);
            if (len + l + 2 > cap) { while (len + l + 2 > cap) cap *= 2; char *n = realloc(script, cap); if (!n) return 1; script = n; }
            if (have_e) script[len++] = '\n';
            memcpy(script + len, argv[i], l);
            len += l;
            script[len] = 0;
            have_e = 1;
        } else if (!strcmp(a, "-l")) {
            if (++i >= argc) return usage();
            if (strcmp(argv[i], "AppleScript")) { fprintf(stderr, "osascript: %s: linguagem não disponível no mOS (só AppleScript)\n", argv[i]); return 1; }
        } else if (!strcmp(a, "-i")) interactive = 1;
        else if (!strcmp(a, "-s")) {
            if (++i >= argc) return usage();
            for (const char *f = argv[i]; *f; f++) {
                if (*f == 's') source_form = 1;
                else if (*f == 'h') source_form = 0;
                else if (*f == 'o') err_to_out = 1;
                else if (*f == 'e') err_to_out = 0;
                else { fprintf(stderr, "osascript: unknown flag '%c' for -s\n", *f); return 1; }
            }
        } else return usage();
    }
    const char *me = "osascript";
    if (interactive && !have_e) {
        char line[4096];
        for (;;) {
            fputs(">> ", stdout);
            fflush(stdout);
            if (!fgets(line, sizeof line, stdin)) break;
            size_t l = strlen(line);
            while (l && (line[l - 1] == '\n' || line[l - 1] == '\r')) line[--l] = 0;
            if (!l) continue;
            run_one(line, 0, 0, source_form, err_to_out, me);
        }
        fputc('\n', stdout);
        free(script);
        return 0;
    }
    if (!have_e) {
        const char *file = i < argc ? argv[i++] : "-";
        FILE *f = strcmp(file, "-") ? fopen(file, "r") : stdin;
        if (!f) { fprintf(stderr, "osascript: %s: No such file or directory\n", file); free(script); return 1; }
        free(script);
        script = slurp(f);
        if (f != stdin) fclose(f);
        if (!script) return 1;
        if (!strncmp(script, "FasdUAS", 7)) { fprintf(stderr, "osascript: %s: scripts compilados (.scpt) não rodam no mOS; use o texto do script\n", file); free(script); return 1; }
        if (strcmp(file, "-")) me = file;
    }
    int rc = run_one(script, (const char *const *)(argv + i), argc - i, source_form, err_to_out, me);
    free(script);
    return rc;
}
