/*
 * mOS libSystem — o resto do CUPS que o corpus importa (docs/lacunas/leonardo.md: Qt, Java, wxWidgets, LibreOffice
 * e o Chromium perguntam pelas impressoras ao abrir). O modelo é o mesmo do vscode_media.c: nenhuma impressora e
 * nenhum cupsd. Listas de destino vêm vazias, PPD não abre (PPD_FILE_OPEN_ERROR) e conexão HTTP com o servidor
 * falha como "serviço indisponível". As opções (cupsParseOptions/cupsAddOption/cupsGetOption) são de verdade: os
 * apps montam a lista antes de procurar a impressora.
 */
#include "sys.h"
#include <stdarg.h>

void *malloc(size_t n);
void *realloc(void *p, size_t n);
void free(void *p);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strcasecmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
char *strdup(const char *s);
void *memcpy(void *d, const void *s, size_t n);
int snprintf(char *b, size_t n, const char *fmt, ...);
int gethostname(char *b, size_t n);

typedef struct { char *name, *value; } cups_option_t;
typedef struct { char *name, *instance; int is_default, num_options; cups_option_t *options; } cups_dest_t;
#define IPP_STATUS_ERROR_SERVICE_UNAVAILABLE 0x0503
#define IPP_STATUS_ERROR_NOT_FOUND 0x0406
#define PPD_FILE_OPEN_ERROR 1
static int g_ppd_err = PPD_FILE_OPEN_ERROR;

/* ---- opções */
static int opt_find(const char *name, int n, cups_option_t *o) {
    for (int i = 0; o && i < n; i++) if (o[i].name && !strcasecmp(o[i].name, name)) return i;
    return -1;
}
int cupsAddOption(const char *name, const char *value, int n, cups_option_t **opts) {
    if (!name || !*name || !value || !opts || n < 0) return n;
    int i = opt_find(name, n, *opts);
    if (i >= 0) {
        char *v = strdup(value);
        if (!v) return n;
        free((*opts)[i].value);
        (*opts)[i].value = v;
        return n;
    }
    cups_option_t *o = realloc(*opts, (size_t)(n + 1) * sizeof *o);
    if (!o) return n;
    o[n].name = strdup(name);
    o[n].value = strdup(value);
    if (!o[n].name || !o[n].value) { free(o[n].name); free(o[n].value); *opts = o; return n; }
    *opts = o;
    return n + 1;
}
int cupsAddIntegerOption(const char *name, int value, int n, cups_option_t **opts) {
    char b[24];
    snprintf(b, sizeof b, "%d", value);
    return cupsAddOption(name, b, n, opts);
}
const char *cupsGetOption(const char *name, int n, cups_option_t *opts) {
    if (!name) return 0;
    int i = opt_find(name, n, opts);
    return i >= 0 ? opts[i].value : 0;
}
int cupsGetIntegerOption(const char *name, int n, cups_option_t *opts) {
    const char *v = cupsGetOption(name, n, opts);
    if (!v) return (int)0x80000000;               /* INT_MIN: ausente */
    int s = 1, r = 0;
    if (*v == '-') { s = -1; v++; }
    while (*v >= '0' && *v <= '9') r = r * 10 + (*v++ - '0');
    return s * r;
}
int cupsRemoveOption(const char *name, int n, cups_option_t **opts) {
    if (!name || !opts || !*opts) return n;
    int i = opt_find(name, n, *opts);
    if (i < 0) return n;
    free((*opts)[i].name); free((*opts)[i].value);
    for (int k = i; k + 1 < n; k++) (*opts)[k] = (*opts)[k + 1];
    return n - 1;
}
void cupsFreeOptions(int n, cups_option_t *opts) {
    for (int i = 0; opts && i < n; i++) { free(opts[i].name); free(opts[i].value); }
    free(opts);
}
/* "nome=valor nome2='com espaço' nome3=\"x\" nome4 {a=b}" — sem valor vira "true", "no<nome>" vira false */
int cupsParseOptions(const char *arg, int n, cups_option_t **opts) {
    if (!arg || !opts) return n;
    size_t len = strlen(arg);
    char *buf = malloc(len + 1);
    if (!buf) return n;
    const char *p = arg;
    for (;;) {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == ',') p++;
        if (!*p) break;
        size_t k = 0;
        while (*p && *p != '=' && *p != ' ' && *p != '\t' && *p != '\n' && *p != ',') buf[k++] = *p++;
        buf[k] = 0;
        char *name = buf, *val = buf + k + 1;
        if (*p != '=') {
            if (!strncmp(name, "no", 2) && name[2]) n = cupsAddOption(name + 2, "false", n, opts);
            else n = cupsAddOption(name, "true", n, opts);
            continue;
        }
        p++;
        size_t v = 0;
        int depth = 0;
        while (*p && (depth || (*p != ' ' && *p != '\t' && *p != '\n' && *p != ','))) {
            if (*p == '\'' || *p == '"') {
                char q = *p++;
                while (*p && *p != q) { if (*p == '\\' && p[1]) p++; val[v++] = *p++; }
                if (*p) p++;
                continue;
            }
            if (*p == '{') depth++;
            else if (*p == '}' && depth) depth--;
            if (*p == '\\' && p[1]) p++;
            val[v++] = *p++;
        }
        val[v] = 0;
        n = cupsAddOption(name, val, n, opts);
    }
    free(buf);
    return n;
}

/* ---- destinos: nenhum */
int cupsGetDests2(void *http, cups_dest_t **dests) { (void)http; if (dests) *dests = 0; return 0; }
int cupsGetDests(cups_dest_t **dests) { if (dests) *dests = 0; return 0; }
cups_dest_t *cupsGetDest(const char *name, const char *inst, int n, cups_dest_t *dests) {
    for (int i = 0; dests && i < n; i++) {
        if (name ? (dests[i].name && !strcasecmp(dests[i].name, name)) : dests[i].is_default) {
            if (!inst ? !dests[i].instance : (dests[i].instance && !strcasecmp(dests[i].instance, inst))) return &dests[i];
        }
    }
    return 0;
}
int cupsRemoveDest(const char *name, const char *inst, int n, cups_dest_t **dests) {
    if (!dests || !*dests) return n;
    cups_dest_t *d = cupsGetDest(name, inst, n, *dests);
    if (!d) return n;
    free(d->name); free(d->instance); cupsFreeOptions(d->num_options, d->options);
    int i = (int)(d - *dests);
    for (int k = i; k + 1 < n; k++) (*dests)[k] = (*dests)[k + 1];
    return n - 1;
}
int cupsAddDest(const char *name, const char *inst, int n, cups_dest_t **dests) {
    if (!name || !dests) return n;
    if (cupsGetDest(name, inst, n, *dests)) return n;
    cups_dest_t *d = realloc(*dests, (size_t)(n + 1) * sizeof *d);
    if (!d) return n;
    d[n].name = strdup(name); d[n].instance = inst ? strdup(inst) : 0;
    d[n].is_default = 0; d[n].num_options = 0; d[n].options = 0;
    *dests = d;
    return n + 1;
}
int cupsSetDests2(void *http, int n, cups_dest_t *dests) { (void)http; (void)n; (void)dests; return -1; }
void cupsSetDests(int n, cups_dest_t *dests) { (void)n; (void)dests; }
const char *cupsGetDefault2(void *http) { (void)http; return 0; }
const char *cupsGetDefault(void) { return 0; }
int cupsGetPrinters(char ***names) { if (names) *names = 0; return 0; }
/* mídia do destino: sem destino, nenhuma */
int cupsGetDestMediaCount(void *http, cups_dest_t *d, void *info, unsigned flags) { (void)http; (void)d; (void)info; (void)flags; return 0; }
int cupsGetDestMediaByIndex(void *http, cups_dest_t *d, void *info, int i, unsigned flags, void *size) {
    (void)http; (void)d; (void)info; (void)i; (void)flags; (void)size; return 0;
}
int cupsGetDestMediaByName(void *http, cups_dest_t *d, void *info, const char *media, unsigned flags, void *size) {
    (void)http; (void)d; (void)info; (void)media; (void)flags; (void)size; return 0;
}
int cupsGetDestMediaBySize(void *http, cups_dest_t *d, void *info, int w, int l, unsigned flags, void *size) {
    (void)http; (void)d; (void)info; (void)w; (void)l; (void)flags; (void)size; return 0;
}
int cupsGetDestMediaDefault(void *http, cups_dest_t *d, void *info, unsigned flags, void *size) {
    (void)http; (void)d; (void)info; (void)flags; (void)size; return 0;
}
const char *cupsLocalizeDestMedia(void *http, cups_dest_t *d, void *info, unsigned flags, void *size) {
    (void)http; (void)d; (void)info; (void)flags; (void)size; return 0;
}
const char *cupsLocalizeDestOption(void *http, cups_dest_t *d, void *info, const char *o) { (void)http; (void)d; (void)info; return o; }
void *cupsConnectDest(cups_dest_t *d, unsigned flags, int msec, int *cancel, char *res, size_t reslen, void *cb, void *ud) {
    (void)d; (void)flags; (void)msec; (void)cancel; (void)cb; (void)ud;
    if (res && reslen) res[0] = 0;
    return 0;
}
int cupsCreateDestJob(void *http, cups_dest_t *d, void *info, int *job, const char *title, int n, cups_option_t *o) {
    (void)http; (void)d; (void)info; (void)title; (void)n; (void)o;
    if (job) *job = 0;
    return IPP_STATUS_ERROR_NOT_FOUND;
}
int cupsStartDestDocument(void *http, cups_dest_t *d, void *info, int job, const char *doc, const char *fmt, int n,
                          cups_option_t *o, int last) {
    (void)http; (void)d; (void)info; (void)job; (void)doc; (void)fmt; (void)n; (void)o; (void)last;
    return 0x0500 + 0x0;                          /* HTTP_STATUS_ERROR */
}
int cupsFinishDestDocument(void *http, cups_dest_t *d, void *info) { (void)http; (void)d; (void)info; return IPP_STATUS_ERROR_NOT_FOUND; }
int cupsCloseDestJob(void *http, cups_dest_t *d, void *info, int job) { (void)http; (void)d; (void)info; (void)job; return IPP_STATUS_ERROR_NOT_FOUND; }
int cupsPrintFile(const char *printer, const char *file, const char *title, int n, cups_option_t *o) {
    (void)printer; (void)file; (void)title; (void)n; (void)o; return 0;
}
int cupsPrintFile2(void *http, const char *printer, const char *file, const char *title, int n, cups_option_t *o) {
    (void)http; return cupsPrintFile(printer, file, title, n, o);
}
int cupsMarkOptions(void *ppd, int n, cups_option_t *o) { (void)ppd; (void)n; (void)o; return 0; }
const char *cupsGetPPD(const char *name) { (void)name; return 0; }
const char *cupsGetPPD2(void *http, const char *name) { (void)http; (void)name; return 0; }
int cupsGetPPD3(void *http, const char *name, long *mod, char *buf, size_t n) {
    (void)http; (void)name; (void)mod;
    if (buf && n) buf[0] = 0;
    return 404;                                   /* HTTP_STATUS_NOT_FOUND */
}
static char g_user[64] = "visitante";
void cupsSetUser(const char *u) { if (u) { size_t n = strlen(u); if (n >= sizeof g_user) n = sizeof g_user - 1; memcpy(g_user, u, n); g_user[n] = 0; } }
void cupsSetServer(const char *s) { (void)s; }
void cupsSetEncryption(int e) { (void)e; }
int cupsEncryption(void) { return 1; }             /* HTTP_ENCRYPTION_IF_REQUESTED */
void cupsSetPasswordCB(void *cb) { (void)cb; }
void cupsSetPasswordCB2(void *cb, void *ud) { (void)cb; (void)ud; }
int cupsDoAuthentication(void *http, const char *method, const char *res) { (void)http; (void)method; (void)res; return -1; }
const char *cupsGetPassword(const char *prompt) { (void)prompt; return 0; }
typedef struct { void *next; int used; char language[16]; int encoding; } cups_lang_t;
static cups_lang_t g_lang = {0, 1, "pt_BR", 0};     /* CUPS_UTF8 */
cups_lang_t *cupsLangDefault(void) { return &g_lang; }
cups_lang_t *cupsLangGet(const char *l) { (void)l; return &g_lang; }
void cupsLangFree(cups_lang_t *l) { (void)l; }
const char *cupsLangString(cups_lang_t *l, const char *msg) { (void)l; return msg; }
/* cups_array_t de outra biblioteca: nenhum array chega aqui com elementos */
void *cupsArrayFirst(void *a) { (void)a; return 0; }
void *cupsArrayNext(void *a) { (void)a; return 0; }
int cupsArrayCount(void *a) { (void)a; return 0; }

/* ---- PPD: nenhum arquivo PPD existe sem impressora */
void *ppdOpenFile(const char *f) { (void)f; g_ppd_err = PPD_FILE_OPEN_ERROR; return 0; }
void *ppdOpenFd(int fd) { (void)fd; g_ppd_err = PPD_FILE_OPEN_ERROR; return 0; }
void *ppdOpen(void *fp) { (void)fp; g_ppd_err = PPD_FILE_OPEN_ERROR; return 0; }
void *ppdOpen2(void *fp) { (void)fp; g_ppd_err = PPD_FILE_OPEN_ERROR; return 0; }
void ppdClose(void *ppd) { (void)ppd; }
int ppdLastError(int *line) { if (line) *line = 0; return g_ppd_err; }
const char *ppdErrorString(int st) {
    static const char *m[] = {"OK", "Unable to open PPD file", "NULL PPD file pointer", "Memory allocation error",
        "Missing PPD-Adobe-4.x header", "Missing value string", "Internal error", "Bad OpenGroup", "OpenGroup without a CloseGroup first",
        "Bad OpenUI/JCLOpenUI", "OpenUI/JCLOpenUI without a CloseUI/JCLCloseUI first", "Bad OrderDependency", "Bad UIConstraints",
        "Missing asterisk in column 1", "Line longer than the maximum allowed (255 characters)", "Illegal control character",
        "Illegal main keyword string", "Illegal option keyword string", "Illegal translation string", "Illegal whitespace character",
        "Bad custom parameter", "Missing option keyword", "Bad value string", "Missing CloseGroup", "Bad CloseUI/JCLCloseUI",
        "Missing CloseUI/JCLCloseUI"};
    return st >= 0 && st < (int)(sizeof m / sizeof *m) ? m[st] : "Unknown";
}
void *ppdFindOption(void *ppd, const char *k) { (void)ppd; (void)k; return 0; }
void *ppdFindChoice(void *o, const char *c) { (void)o; (void)c; return 0; }
void *ppdFindMarkedChoice(void *ppd, const char *k) { (void)ppd; (void)k; return 0; }
void *ppdFindAttr(void *ppd, const char *n, const char *spec) { (void)ppd; (void)n; (void)spec; return 0; }
void *ppdFindNextAttr(void *ppd, const char *n, const char *spec) { (void)ppd; (void)n; (void)spec; return 0; }
void ppdMarkDefaults(void *ppd) { (void)ppd; }
int ppdMarkOption(void *ppd, const char *k, const char *c) { (void)ppd; (void)k; (void)c; return 0; }
int ppdConflicts(void *ppd) { (void)ppd; return 0; }
void *ppdPageSize(void *ppd, const char *name) { (void)ppd; (void)name; return 0; }
float ppdPageWidth(void *ppd, const char *name) { (void)ppd; (void)name; return 0; }
float ppdPageLength(void *ppd, const char *name) { (void)ppd; (void)name; return 0; }
int ppdLocalize(void *ppd) { (void)ppd; return -1; }
void *ppdFindCustomOption(void *ppd, const char *k) { (void)ppd; (void)k; return 0; }
void *ppdFirstCustomParam(void *o) { (void)o; return 0; }
void *ppdNextCustomParam(void *o) { (void)o; return 0; }
void *ppdFindCustomParam(void *o, const char *n) { (void)o; (void)n; return 0; }
void *ppdFirstOption(void *ppd) { (void)ppd; return 0; }
void *ppdNextOption(void *ppd) { (void)ppd; return 0; }

/* ---- HTTP: sem servidor */
static int g_http_err = 61;                         /* ECONNREFUSED */
void *httpConnectEncrypt(const char *host, int port, int enc) { (void)host; (void)port; (void)enc; g_http_err = 61; return 0; }
void *httpConnect(const char *host, int port) { return httpConnectEncrypt(host, port, 0); }
int httpError(void *http) { (void)http; return g_http_err; }
void httpBlocking(void *http, int b) { (void)http; (void)b; }
int httpReconnect2(void *http, int msec, int *cancel) { (void)http; (void)msec; (void)cancel; return -1; }
int httpReconnect(void *http) { (void)http; return -1; }
char *httpGetHostname(void *http, char *s, int len) {
    (void)http;
    if (!s || len <= 0) return 0;
    if (gethostname(s, (size_t)len)) { snprintf(s, (size_t)len, "localhost"); }
    s[len - 1] = 0;
    return s;
}
void *httpGetAddress(void *http) { (void)http; return 0; }
int httpAddrPort(void *addr) {
    if (!addr) return -1;
    const u8 *a = addr;
    u8 fam = a[1];                                /* sockaddr do BSD: len, family */
    if (fam == 2 || fam == 30) return (a[2] << 8) | a[3];
    return 0;
}
int httpAddrLength(const void *addr) {
    if (!addr) return 0;
    const u8 *a = addr;
    return a[1] == 2 ? 16 : a[1] == 30 ? 28 : a[1] == 1 ? 106 : 0;
}
void *httpAddrGetList(const char *host, int fam, const char *svc) { (void)host; (void)fam; (void)svc; return 0; }
void httpAddrFreeList(void *l) { (void)l; }
int httpGetFd(void *http) { (void)http; return -1; }
void httpFlush(void *http) { (void)http; }
int httpUpdate(void *http) { (void)http; return 0x0500; }     /* HTTP_STATUS_ERROR */
int httpWait(void *http, int msec) { (void)http; (void)msec; return 0; }
int httpCheck(void *http) { (void)http; return 0; }
long httpRead2(void *http, char *b, size_t n) { (void)http; (void)b; (void)n; return -1; }
long httpWrite2(void *http, const char *b, size_t n) { (void)http; (void)b; (void)n; return -1; }
int httpGet(void *http, const char *uri) { (void)http; (void)uri; return -1; }
int httpPost(void *http, const char *uri) { (void)http; (void)uri; return -1; }
long httpGetLength2(void *http) { (void)http; return -1; }
const char *httpGetField(void *http, int f) { (void)http; (void)f; return ""; }
void httpSetField(void *http, int f, const char *v) { (void)http; (void)f; (void)v; }
void httpClearFields(void *http) { (void)http; }
const char *httpGetAuthString(void *http) { (void)http; return 0; }
void httpSetAuthString(void *http, const char *scheme, const char *data) { (void)http; (void)scheme; (void)data; }
int httpEncryption(void *http, int e) { (void)http; (void)e; return -1; }
/* httpSeparateURI(decode, uri, scheme, slen, user, ulen, host, hlen, &port, res, rlen) */
int httpSeparateURI(int decode, const char *uri, char *scheme, int sl, char *user, int ul, char *host, int hl, int *port,
                    char *res, int rl) {
    (void)decode;
    if (scheme && sl) scheme[0] = 0;
    if (user && ul) user[0] = 0;
    if (host && hl) host[0] = 0;
    if (res && rl) res[0] = 0;
    if (port) *port = 0;
    if (!uri) return -1;
    const char *p = uri, *c = uri;
    while (*c && *c != ':') c++;
    if (!*c) return -1;                           /* HTTP_URI_STATUS_BAD_URI */
    if (scheme) snprintf(scheme, (size_t)sl, "%.*s", (int)(c - p), p);
    p = c + 1;
    if (p[0] == '/' && p[1] == '/') {
        p += 2;
        const char *end = p;
        while (*end && *end != '/' && *end != '?') end++;
        const char *at = p;
        while (at < end && *at != '@') at++;
        if (at < end) { if (user) snprintf(user, (size_t)ul, "%.*s", (int)(at - p), p); p = at + 1; }
        const char *colon = p;
        while (colon < end && *colon != ':') colon++;
        if (host) snprintf(host, (size_t)hl, "%.*s", (int)(colon - p), p);
        if (colon < end && port) { int v = 0; for (const char *q = colon + 1; q < end && *q >= '0' && *q <= '9'; q++) v = v * 10 + (*q - '0'); *port = v; }
        else if (port) *port = !strcmp(scheme ? scheme : "", "https") || (scheme && !strcmp(scheme, "ipps")) ? 443 :
                                (scheme && (!strcmp(scheme, "ipp") || !strcmp(scheme, "http"))) ? (scheme[0] == 'i' ? 631 : 80) : 0;
        p = end;
    }
    if (res) snprintf(res, (size_t)rl, "%s", *p ? p : "/");
    return 0;                                     /* HTTP_URI_STATUS_OK */
}
int httpAssembleURIf(int enc, char *uri, int len, const char *scheme, const char *user, const char *host, int port, const char *fmt, ...) {
    (void)enc;
    if (!uri || len <= 0) return -1;
    char res[1024];
    va_list ap; va_start(ap, fmt);
    extern int vsnprintf(char *, size_t, const char *, va_list);
    vsnprintf(res, sizeof res, fmt ? fmt : "", ap);
    va_end(ap);
    int n = user && *user ? snprintf(uri, (size_t)len, "%s://%s@%s:%d%s", scheme, user, host, port, res)
                          : snprintf(uri, (size_t)len, "%s://%s:%d%s", scheme, host, port, res);
    return n < len ? 0 : -1;
}
int httpAssembleURI(int enc, char *uri, int len, const char *scheme, const char *user, const char *host, int port, const char *res) {
    return httpAssembleURIf(enc, uri, len, scheme, user, host, port, "%s", res ? res : "");
}

/* ---- IPP: a mensagem é montada (ipp.c do vscode_media), mas nunca sai */
long ippLength(void *ipp) { (void)ipp; return 0; }
int ippRead(void *http, void *ipp) { (void)http; (void)ipp; return -1; }              /* IPP_STATE_ERROR */
int ippWrite(void *http, void *ipp) { (void)http; (void)ipp; return -1; }
int ippSetState(void *ipp, int st) { (void)ipp; (void)st; return 1; }
int ippSetRequestId(void *ipp, int id) { (void)ipp; (void)id; return 1; }
int ippSetOperation(void *ipp, int op) { (void)ipp; (void)op; return 1; }
int ippGetOperation(void *ipp) { (void)ipp; return 0; }
int ippGetValueTag(void *attr) { (void)attr; return 0; }
int ippGetBoolean(void *attr, int i) { (void)attr; (void)i; return 0; }
const char *ippErrorString(int e) {
    switch (e) {
    case 0: return "successful-ok";
    case 0x0406: return "client-error-not-found";
    case 0x0503: return "server-error-service-unavailable";
    default: return "unknown";
    }
}
void *ippAddResolution(void *ipp, int group, const char *name, int units, int x, int y) {
    (void)ipp; (void)group; (void)name; (void)units; (void)x; (void)y; return 0;
}
