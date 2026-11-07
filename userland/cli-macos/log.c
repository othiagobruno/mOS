/* mOS — log(1): lê o store do logd (os_log, syslog, asl). Subconjunto do log do macOS:
 *   log show [--last 30s|5m|2h|1d] [--start "AAAA-MM-DD HH:MM:SS"] [--info] [--debug]
 *            [--process NOME] [--predicate 'subsystem == "x"' | 'process == "x"' | 'category == "x"' |
 *                                   'eventMessage CONTAINS "x"' | 'senderImagePath CONTAINS "x"'  (e &&)]
 *            [--style default|compact|syslog|json|ndjson]
 *   log stream [mesmas opções]        (acompanha o store, como tail -f)
 *   log erase                         (apaga o store)
 * O store é texto (uma linha por evento, campos separados por tabulação; userland/logd.c, svcproto.h). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "../svcproto.h"

typedef struct { const char *field, *op, *value; } cond_t;
static cond_t conds[8];
static int nconds, show_info, show_debug, style;          /* style: 0 default, 1 compact, 2 syslog, 3 json, 4 ndjson */
static long long since_ms;
static const char *only_proc;
static int printed;

static const char *type_name(int t, int compact) {
    switch (t) {
    case 0: return compact ? "Df" : "Default";
    case 1: return compact ? "I " : "Info";
    case 2: return compact ? "Db" : "Debug";
    case 16: return compact ? "E " : "Error";
    case 17: return compact ? "F " : "Fault";
    default: return compact ? "? " : "Default";
    }
}
/* "a == b", "a CONTAINS b", juntos por && (aspas simples ou duplas) */
static char predbuf[1024];
static void parse_predicate(const char *p) {
    snprintf(predbuf, sizeof predbuf, "%s", p);
    char *s = predbuf;
    while (*s && nconds < 8) {
        while (*s == ' ' || *s == '(') s++;
        char *f = s;
        while (*s && *s != ' ' && *s != '=') s++;
        if (*s) *s++ = 0;
        while (*s == ' ') s++;
        char *op = s;
        while (*s && *s != ' ' && *s != '"' && *s != '\'') s++;
        if (*s == ' ') *s++ = 0;
        while (*s == ' ') s++;
        char q = *s;
        if (q != '"' && q != '\'') break;
        char *v = ++s;
        while (*s && *s != q) s++;
        if (*s) *s++ = 0;
        if (!strcmp(op, "=") || !strcmp(op, "")) op = "==";
        conds[nconds].field = f; conds[nconds].op = op; conds[nconds].value = v; nconds++;
        while (*s == ' ' || *s == ')') s++;
        if (!strncmp(s, "&&", 2)) s += 2; else if (!strncasecmp(s, "AND", 3)) s += 3; else break;
    }
}
static int contains_ci(const char *h, const char *n) {
    size_t ln = strlen(n);
    for (; *h; h++) if (!strncasecmp(h, n, ln)) return 1;
    return !ln;
}
static int match(const char *proc, const char *subsys, const char *cat, const char *msg) {
    if (only_proc && strcmp(proc, only_proc)) return 0;
    for (int i = 0; i < nconds; i++) {
        const char *f = conds[i].field, *v = conds[i].value, *x = "";
        if (!strcmp(f, "subsystem")) x = subsys;
        else if (!strcmp(f, "category")) x = cat;
        else if (!strcmp(f, "process") || !strcmp(f, "senderImagePath") || !strcmp(f, "processImagePath")) x = proc;
        else if (!strcmp(f, "eventMessage") || !strcmp(f, "composedMessage")) x = msg;
        const char *op = conds[i].op;
        if (!strcmp(op, "==") && strcmp(x, v)) return 0;
        if (!strcmp(op, "!=") && !strcmp(x, v)) return 0;
        if (!strcasecmp(op, "CONTAINS") && !contains_ci(x, v)) return 0;
        if (!strcasecmp(op, "BEGINSWITH") && strncmp(x, v, strlen(v))) return 0;
    }
    return 1;
}
static void unescape(char *s) {
    char *o = s;
    for (; *s; s++) {
        if (*s == '\\' && s[1]) { s++; *o++ = *s == 'n' ? '\n' : *s == 't' ? '\t' : *s; }
        else *o++ = *s;
    }
    *o = 0;
}
static void json_str(const char *s) {
    putchar('"');
    for (; *s; s++) {
        if (*s == '"' || *s == '\\') { putchar('\\'); putchar(*s); }
        else if (*s == '\n') fputs("\\n", stdout);
        else if ((unsigned char)*s < 0x20) printf("\\u%04x", *s);
        else putchar(*s);
    }
    putchar('"');
}
static void header(void) {
    if (style == 0) printf("Timestamp                       Thread     Type        Activity             PID    TTL  \n");
    else if (style == 1) printf("Timestamp               Ty Process[PID:TID]\n");
    else if (style == 3) printf("[");
}
static void emit_line(char *line) {
    char *f[7];
    int n = 0;
    f[n++] = line;
    for (char *p = line; *p && n < 7; p++) if (*p == '\t') { *p = 0; f[n++] = p + 1; }
    if (n < 7) return;
    char *nl = strchr(f[6], '\n');
    if (nl) *nl = 0;
    long long ms = atoll(f[0]);
    int pid = atoi(f[1]), type = atoi(f[2]);
    if (ms < since_ms) return;
    if (type == 1 && !show_info) return;
    if (type == 2 && !show_debug) return;
    unescape(f[6]);
    if (!match(f[3], f[4], f[5], f[6])) return;
    time_t t = (time_t)(ms / 1000);
    struct tm *tm = gmtime(&t);
    char ts[64];
    strftime(ts, sizeof ts, "%Y-%m-%d %H:%M:%S", tm);
    if (style == 3 || style == 4) {
        if (style == 3 && printed) printf(",");
        printf(style == 3 ? "{\n  \"timestamp\" : \"%s.%03d+0000\",\n  \"messageType\" : \"%s\",\n  \"processID\" : %d,\n  \"processImagePath\" : "
                          : "{\"timestamp\":\"%s.%03d+0000\",\"messageType\":\"%s\",\"processID\":%d,\"processImagePath\":",
               ts, (int)(ms % 1000), type_name(type, 0), pid);
        json_str(f[3]);
        printf(style == 3 ? ",\n  \"subsystem\" : " : ",\"subsystem\":"); json_str(f[4]);
        printf(style == 3 ? ",\n  \"category\" : " : ",\"category\":"); json_str(f[5]);
        printf(style == 3 ? ",\n  \"eventMessage\" : " : ",\"eventMessage\":"); json_str(f[6]);
        printf(style == 3 ? "\n}" : "}\n");
    } else if (style == 2) {
        printf("%s.%06d+0000  localhost %s[%d]: ", ts, (int)(ms % 1000) * 1000, f[3], pid);
        if (f[4][0]) printf("(%s) [%s:%s] ", f[4], f[4], f[5]);
        printf("%s\n", f[6]);
    } else if (style == 1) {
        printf("%s.%03d %s %s[%d:0] ", ts, (int)(ms % 1000), type_name(type, 1), f[3], pid);
        if (f[4][0]) printf("[%s:%s] ", f[4], f[5]);
        printf("%s\n", f[6]);
    } else {
        printf("%s.%06d+0000 0x%-8x %-11s 0x0                  %-6d 0    %s: ", ts, (int)(ms % 1000) * 1000, pid, type_name(type, 0), pid, f[3]);
        if (f[4][0]) printf("(%s) [%s:%s] ", f[4], f[4], f[5]);
        printf("%s\n", f[6]);
    }
    printed++;
}
static void show_file(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return;
    static char line[SVC_MAX_MSG * 2 + 256];
    while (fgets(line, sizeof line, fp)) emit_line(line);
    fclose(fp);
}
static long long parse_last(const char *s) {
    double v = atof(s);
    char u = s[strlen(s) - 1];
    long long mult = u == 'm' ? 60000 : u == 'h' ? 3600000 : u == 'd' ? 86400000 : 1000;
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return (long long)ts.tv_sec * 1000 + ts.tv_nsec / 1000000 - (long long)(v * mult);
}
static int usage(void) {
    fprintf(stderr, "uso: log show|stream|erase [--last 5m] [--info] [--debug] [--process NOME] [--predicate EXPR] [--style default|compact|syslog|json|ndjson]\n");
    return 64;
}
int main(int argc, char **argv) {
    if (argc < 2) return usage();
    const char *cmd = argv[1];
    for (int i = 2; i < argc; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "--info")) show_info = 1;
        else if (!strcmp(a, "--debug")) show_info = show_debug = 1;
        else if (!strcmp(a, "--level") && i + 1 < argc) { i++; if (!strcmp(argv[i], "debug")) show_info = show_debug = 1; else if (!strcmp(argv[i], "info")) show_info = 1; }
        else if (!strcmp(a, "--last") && i + 1 < argc) since_ms = parse_last(argv[++i]);
        else if (!strcmp(a, "--start") && i + 1 < argc) {
            struct tm tm = { 0 };
            const char *s = argv[++i];
            sscanf(s, "%d-%d-%d %d:%d:%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday, &tm.tm_hour, &tm.tm_min, &tm.tm_sec);
            tm.tm_year -= 1900; tm.tm_mon -= 1;
            since_ms = (long long)timegm(&tm) * 1000;
        }
        else if (!strcmp(a, "--process") && i + 1 < argc) only_proc = argv[++i];
        else if (!strcmp(a, "--predicate") && i + 1 < argc) parse_predicate(argv[++i]);
        else if (!strcmp(a, "--style") && i + 1 < argc) {
            const char *s = argv[++i];
            style = !strcmp(s, "compact") ? 1 : !strcmp(s, "syslog") ? 2 : !strcmp(s, "json") ? 3 : !strcmp(s, "ndjson") ? 4 : 0;
        }
        else if (!strcmp(a, "--no-pager") || !strcmp(a, "--color") || !strcmp(a, "--source")) { if (!strcmp(a, "--color") && i + 1 < argc) i++; }
        else { fprintf(stderr, "log: opção desconhecida: %s\n", a); return usage(); }
    }
    if (!strcmp(cmd, "erase")) { unlink(LOG_STORE); unlink(LOG_STORE_OLD); printf("Deleted selected logs\n"); return 0; }
    if (!strcmp(cmd, "show")) {
        header();
        show_file(LOG_STORE_OLD);
        show_file(LOG_STORE);
        if (style == 3) printf("]\n");
        else if (style != 4) printf("--------------------------------------------------------------------------------------------------------------------\nLog      - Default: %d\n", printed);
        return 0;
    }
    if (!strcmp(cmd, "stream")) {
        printf("Filtering the log data using the given options\n");
        header();
        FILE *fp = fopen(LOG_STORE, "r");
        if (fp) { fseek(fp, 0, SEEK_END); }
        static char line[SVC_MAX_MSG * 2 + 256];
        for (;;) {
            if (!fp) { usleep(500000); fp = fopen(LOG_STORE, "r"); continue; }
            if (fgets(line, sizeof line, fp)) { emit_line(line); fflush(stdout); continue; }
            clearerr(fp);
            usleep(200000);
        }
    }
    return usage();
}
