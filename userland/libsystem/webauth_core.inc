/* Fluxo de login até o callback, sem conta e sem rede.
 * O app abre a URL de autorização no navegador (NSWorkspace openURL: / LSOpen).
 * A volta é uma URL com o esquema declarado em CFBundleURLTypes (cursor://, slack://,
 * vscode://, claude://, whatsapp://, chatgpt://, grok://, openbot://) ou http://127.0.0.1
 * / http://localhost (Codex e ChatGPT). O token (code, access_token ou token) sai da
 * query ou do fragmento para o Keychain. */
#ifndef WEBAUTH_CORE_INC
#define WEBAUTH_CORE_INC
size_t strlen(const char *s);
int snprintf(char *b, size_t n, const char *fmt, ...);
void *memset(void *d, int c, size_t n);
void *memcpy(void *d, const void *s, size_t n);
char *strstr(const char *h, const char *n);
int strncmp(const char *a, const char *b, size_t n);

#define WA_URL 2048
#define WA_SCH 64
#define WA_TOK 512
#define WA_N 8

typedef struct {
    char service[128];
    char account[96];
    char secret[WA_TOK];
    char kind[16];
} wa_secret;

typedef struct {
    int used;
    int started;
    int done;
    int cancelled;
    int opened;
    char auth_url[WA_URL];
    char scheme[WA_SCH];
    char callback[WA_URL];
} wa_slot;

static wa_slot wa_slots[WA_N];

static int wa_hex(int c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static void wa_decode(const char *s, size_t n, char *out, size_t cap) {
    size_t o = 0;
    for (size_t i = 0; i < n && o + 1 < cap; i++) {
        if (s[i] == '%' && i + 2 < n) {
            int h = wa_hex(s[i + 1]), l = wa_hex(s[i + 2]);
            if (h >= 0 && l >= 0) { out[o++] = (char)((h << 4) | l); i += 2; continue; }
        }
        if (s[i] == '+') { out[o++] = ' '; continue; }
        out[o++] = s[i];
    }
    out[o] = 0;
}

static int wa_eq_ci(const char *a, const char *b) {
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
        if (ca != cb) return 0;
        a++; b++;
    }
    return *a == 0 && *b == 0;
}

/* esquema antes de ':', ou vazio */
static void wa_scheme_of(const char *url, char *out, size_t cap) {
    size_t i = 0;
    out[0] = 0;
    if (!url) return;
    while (url[i] && url[i] != ':' && url[i] != '/' && url[i] != '?' && url[i] != '#') i++;
    if (url[i] != ':' || i == 0 || i >= cap) return;
    memcpy(out, url, i);
    out[i] = 0;
}

static int wa_is_loopback(const char *url) {
    const char *p = strstr(url, "://");
    if (!p) return 0;
    p += 3;
    return strncmp(p, "127.0.0.1", 9) == 0 || strncmp(p, "localhost", 9) == 0 || strncmp(p, "[::1]", 5) == 0;
}

static int wa_scheme_match(const wa_slot *s, const char *url) {
    char got[WA_SCH];
    wa_scheme_of(url, got, sizeof got);
    if (s->scheme[0] == 0) return wa_is_loopback(url);
    if (wa_eq_ci(s->scheme, "http") || wa_eq_ci(s->scheme, "https") || wa_eq_ci(s->scheme, "localhost"))
        return wa_is_loopback(url);
    return wa_eq_ci(s->scheme, got);
}

static int wa_pick_param(const char *q, const char *key, char *out, size_t cap) {
    size_t kn = strlen(key);
    const char *p = q;
    while (p && *p) {
        if ((p == q || p[-1] == '&' || p[-1] == '?' || p[-1] == '#') && strncmp(p, key, kn) == 0 && p[kn] == '=') {
            p += kn + 1;
            const char *e = p;
            while (*e && *e != '&' && *e != '#') e++;
            wa_decode(p, (size_t)(e - p), out, cap);
            return out[0] != 0;
        }
        p++;
    }
    return 0;
}

/* 1 se achou token. kind = access_token | code | token */
static int wa_token_from_url(const char *url, char *tok, size_t cap, char *kind, size_t kcap) {
    tok[0] = kind[0] = 0;
    if (!url) return 0;
    static const char *keys[] = {"access_token", "code", "token"};
    for (int i = 0; i < 3; i++) {
        if (wa_pick_param(url, keys[i], tok, cap)) {
            snprintf(kind, kcap, "%s", keys[i]);
            return 1;
        }
    }
    return 0;
}

static int wa_create(const char *auth_url, const char *scheme) {
    if (!auth_url || !auth_url[0] || strlen(auth_url) >= WA_URL) return -1;
    int id = -1;
    for (int i = 0; i < WA_N; i++) if (!wa_slots[i].used) { id = i; break; }
    if (id < 0) return -1;
    wa_slot *s = &wa_slots[id];
    memset(s, 0, sizeof *s);
    s->used = 1;
    snprintf(s->auth_url, sizeof s->auth_url, "%s", auth_url);
    if (scheme && scheme[0] && strlen(scheme) < WA_SCH) snprintf(s->scheme, sizeof s->scheme, "%s", scheme);
    return id;
}

/* O navegador externo abre aqui (o chamador faz NSWorkspace openURL: / LSOpen). */
static int wa_mark_open(int id) {
    if (id < 0 || id >= WA_N || !wa_slots[id].used || wa_slots[id].done || wa_slots[id].started) return 0;
    wa_slots[id].started = 1;
    wa_slots[id].opened = 1;
    return 1;
}

static int wa_begin(const char *auth_url, const char *scheme) {
    int id = wa_create(auth_url, scheme);
    if (id < 0) return -1;
    wa_mark_open(id);
    return id;
}

static void wa_fill_secret(const wa_slot *s, wa_secret *sec) {
    memset(sec, 0, sizeof *sec);
    const char *sch = s->scheme[0] ? s->scheme : "localhost";
    snprintf(sec->service, sizeof sec->service, "mos.oauth.%s", sch);
    snprintf(sec->account, sizeof sec->account, "callback");
    wa_token_from_url(s->callback, sec->secret, sizeof sec->secret, sec->kind, sizeof sec->kind);
}

/* 1 se alguma sessão em espera consumiu a URL */
static int wa_offer(const char *callback_url, int *id_out, wa_secret *sec) {
    if (id_out) *id_out = -1;
    if (!callback_url || !callback_url[0] || strlen(callback_url) >= WA_URL) return 0;
    for (int i = 0; i < WA_N; i++) {
        wa_slot *s = &wa_slots[i];
        if (!s->used || !s->started || s->done) continue;
        if (!wa_scheme_match(s, callback_url)) continue;
        snprintf(s->callback, sizeof s->callback, "%s", callback_url);
        s->done = 1;
        if (id_out) *id_out = i;
        if (sec) wa_fill_secret(s, sec);
        return 1;
    }
    return 0;
}

static int wa_cancel(int id) {
    if (id < 0 || id >= WA_N || !wa_slots[id].used || wa_slots[id].done) return 0;
    wa_slots[id].done = 1;
    wa_slots[id].cancelled = 1;
    return 1;
}

static void wa_reset(void) { memset(wa_slots, 0, sizeof wa_slots); }

#endif
