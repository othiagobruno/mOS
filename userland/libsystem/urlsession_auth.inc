/* Decisão de NSURLSession: desafio Basic e desafio de servidor (TLS).
 * Quem inclui precisa de string.h (memcpy, strlen) e stdio.h (snprintf).
 *
 * NSURLSessionAuthChallengeDisposition: 0 usa credencial, 1 padrão, 2 cancela, 3 rejeita.
 * NSURLSessionResponseDisposition: 0 cancela, 1 permite, 2 vira download.
 */
#define US_AUTH_USE 0
#define US_AUTH_DEFAULT 1
#define US_AUTH_CANCEL 2
#define US_AUTH_REJECT 3
#define US_DISP_CANCEL 0
#define US_DISP_ALLOW 1
#define US_DISP_DOWNLOAD 2
#define US_METHOD_BASIC "NSURLAuthenticationMethodHTTPBasic"
#define US_METHOD_SERVER "NSURLAuthenticationMethodServerTrust"

static int us_casecmp(const char *a, const char *b) {
    if (!a || !b) return a ? 1 : b ? -1 : 0;
    while (*a && *b) {
        unsigned char ca = (unsigned char)*a, cb = (unsigned char)*b;
        if (ca >= 'A' && ca <= 'Z') ca = (unsigned char)(ca + 32);
        if (cb >= 'A' && cb <= 'Z') cb = (unsigned char)(cb + 32);
        if (ca != cb) return (int)ca - (int)cb;
        a++;
        b++;
    }
    return (int)(unsigned char)*a - (int)(unsigned char)*b;
}

static int us_casecmp_n(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        unsigned char ca = (unsigned char)a[i], cb = (unsigned char)b[i];
        if (ca >= 'A' && ca <= 'Z') ca = (unsigned char)(ca + 32);
        if (cb >= 'A' && cb <= 'Z') cb = (unsigned char)(cb + 32);
        if (ca != cb || !a[i] || !b[i]) return (int)ca - (int)cb;
    }
    return 0;
}

/* 1 se o WWW-Authenticate começa com Basic. O realm, se houver, vai para realm. */
static int us_is_basic(const char *www, char *realm, size_t cap) {
    if (realm && cap) realm[0] = 0;
    if (!www) return 0;
    while (*www == ' ' || *www == '\t') www++;
    if (us_casecmp_n(www, "basic", 5) != 0) return 0;
    if (www[5] && www[5] != ' ' && www[5] != '\t') return 0;
    if (realm && cap) {
        const char *p = www;
        while (*p) {
            if (us_casecmp_n(p, "realm=", 6) == 0) {
                p += 6;
                size_t i = 0;
                if (*p == '"') {
                    p++;
                    while (*p && *p != '"' && i + 1 < cap) realm[i++] = *p++;
                } else {
                    while (*p && *p != ' ' && *p != ',' && *p != '\r' && *p != '\n' && i + 1 < cap) realm[i++] = *p++;
                }
                realm[i] = 0;
                break;
            }
            p++;
        }
    }
    return 1;
}

/* "Basic " + base64(user:pass). 0 = ok. O vetor da RFC 7617 cabe aqui. */
static int us_basic_value(const char *user, const char *pass, char *out, size_t cap) {
    static const char B64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    char raw[384];
    int n = snprintf(raw, sizeof raw, "%s:%s", user ? user : "", pass ? pass : "");
    if (n < 0 || (size_t)n >= sizeof raw || !out) return -1;
    size_t need = 6 + ((size_t)n + 2) / 3 * 4 + 1;
    if (cap < need) return -1;
    memcpy(out, "Basic ", 6);
    char *o = out + 6;
    int i = 0;
    while (i < n) {
        int left = n - i;
        unsigned a = (unsigned char)raw[i++];
        unsigned b = left > 1 ? (unsigned char)raw[i++] : 0;
        unsigned c = left > 2 ? (unsigned char)raw[i++] : 0;
        *o++ = B64[a >> 2];
        *o++ = B64[((a & 3) << 4) | (b >> 4)];
        *o++ = left > 1 ? B64[((b & 15) << 2) | (c >> 6)] : '=';
        *o++ = left > 2 ? B64[c & 63] : '=';
    }
    *o = 0;
    return 0;
}
