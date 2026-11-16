/*
 * mOS — mnet: regras do cache HTTP privado (RFC 9111), sem armazenamento: o NSURLCache guarda as entradas
 * (memória e disco) e pergunta aqui se pode guardar, se ainda está fresca, como revalidar e como juntar um 304.
 */
#include "mnet.h"

/* procura a diretiva d na lista do Cache-Control (todas as linhas); *val recebe o argumento se houver */
static int cc_get(const mnet_hdrs_t *h, const char *d, long long *val) {
    size_t dl = strlen(d);
    int found = 0;
    for (int i = 0; h && i < h->n; i++) {
        if (!mnet_ieq(h->v[i].name, "cache-control")) continue;
        const char *p = h->v[i].value;
        while (*p) {
            while (*p == ' ' || *p == '\t' || *p == ',') p++;
            const char *s = p;
            while (*p && *p != ',' && *p != '=') p++;
            const char *e = p;
            while (e > s && (e[-1] == ' ' || e[-1] == '\t')) e--;
            int match = (size_t)(e - s) == dl && mnet_ieqn(s, d, dl);
            long long v = -1;
            if (*p == '=') {
                p++;
                while (*p == ' ') p++;
                if (*p == '"') p++;
                if (*p >= '0' && *p <= '9') { v = 0; while (*p >= '0' && *p <= '9') { if (v < (1LL << 40)) v = v * 10 + (*p - '0'); p++; } }
                while (*p && *p != ',') p++;
            }
            if (match) { found = 1; if (val) *val = v; }
        }
    }
    return found;
}
static long long hdate(const mnet_hdrs_t *h, const char *n) {
    const char *v = mnet_hdrs_get(h, n);
    return v ? mnet_http_date(v, strlen(v)) : -1;
}
static int default_cacheable(int st) {
    switch (st) { case 200: case 203: case 204: case 206: case 300: case 301: case 308: case 404: case 405: case 410: case 414: case 501: return 1; }
    return 0;
}

int mnet_cache_storable(const char *method, int status, const mnet_hdrs_t *req, const mnet_hdrs_t *resp) {
    if (!mnet_ieq(method, "GET")) return 0;                       /* só GET (HEAD não traz corpo para guardar) */
    if (status < 200 || status == 206) return 0;                  /* sem faixas parciais */
    if (cc_get(req, "no-store", 0) || cc_get(resp, "no-store", 0)) return 0;
    const char *vary = mnet_hdrs_get(resp, "vary");
    if (vary && strchr(vary, '*')) return 0;
    long long v;
    if (cc_get(resp, "max-age", &v) || mnet_hdrs_get(resp, "expires") || cc_get(resp, "public", 0) || cc_get(resp, "private", 0))
        return 1;                                                 /* frescor explícito (cache privado aceita private) */
    if (default_cacheable(status)) return 1;                      /* heurístico ou só revalidável */
    (void)v;
    return 0;
}

/* RFC 9111 §4.2: tempo de vida e idade; devolve segundos de frescor restantes (<= 0: velha) */
long long mnet_cache_fresh_left(const mnet_hdrs_t *resp, int status, long long req_time, long long resp_time, long long now) {
    long long life = -1, v;
    if (cc_get(resp, "no-cache", 0)) return 0;                    /* sempre revalida */
    if (cc_get(resp, "max-age", &v) && v >= 0) life = v;
    else {
        long long ex = hdate(resp, "expires"), dt = hdate(resp, "date");
        const char *exs = mnet_hdrs_get(resp, "expires");
        if (exs) life = ex < 0 ? 0 : ex - (dt >= 0 ? dt : resp_time);   /* Expires inválido = já vencido */
        else if (default_cacheable(status)) {
            long long lm = hdate(resp, "last-modified");
            if (lm >= 0) {
                long long base = dt >= 0 ? dt : resp_time;
                life = base > lm ? (base - lm) / 10 : 0;          /* heurística de 10% (§4.2.2) */
                if (life > 7 * 86400) life = 7 * 86400;
            }
        }
    }
    if (life < 0) return 0;
    long long date = hdate(resp, "date");
    long long age_hdr = 0;
    const char *ag = mnet_hdrs_get(resp, "age");
    if (ag) { while (*ag >= '0' && *ag <= '9') age_hdr = age_hdr * 10 + (*ag++ - '0'); }
    long long apparent = date >= 0 && resp_time > date ? resp_time - date : 0;
    long long corrected = age_hdr + (resp_time - req_time);
    long long initial = apparent > corrected ? apparent : corrected;
    long long current = initial + (now - resp_time);
    return life - current;
}

/* o pedido força revalidar? (no-cache, max-age=0, Pragma: no-cache) */
int mnet_cache_req_revalidate(const mnet_hdrs_t *req) {
    long long v;
    if (cc_get(req, "no-cache", 0)) return 1;
    if (cc_get(req, "max-age", &v) && v == 0) return 1;
    const char *pr = mnet_hdrs_get(req, "pragma");
    return !mnet_hdrs_get(req, "cache-control") && pr && mnet_tok_has(pr, "no-cache");
}
int mnet_cache_must_revalidate(const mnet_hdrs_t *resp) {
    return cc_get(resp, "must-revalidate", 0) || cc_get(resp, "no-cache", 0);
}

/* cabeçalhos condicionais para revalidar a entrada guardada */
int mnet_cache_validators(const mnet_hdrs_t *stored, mnet_hdrs_t *req) {
    int n = 0;
    const char *et = mnet_hdrs_get(stored, "etag"), *lm = mnet_hdrs_get(stored, "last-modified");
    if (et && !mnet_hdrs_get(req, "if-none-match")) { mnet_hdrs_add(req, "If-None-Match", 13, et, strlen(et)); n++; }
    if (lm && !mnet_hdrs_get(req, "if-modified-since")) { mnet_hdrs_add(req, "If-Modified-Since", 17, lm, strlen(lm)); n++; }
    return n;
}

/* 304: os campos novos trocam os guardados (§3.2), menos os de conteúdo */
void mnet_cache_merge_304(mnet_hdrs_t *stored, const mnet_hdrs_t *r304) {
    for (int i = 0; i < r304->n; i++) {
        const char *n = r304->v[i].name;
        if (mnet_ieq(n, "content-length") || mnet_ieq(n, "content-encoding") || mnet_ieq(n, "transfer-encoding") ||
            mnet_ieq(n, "content-range") || mnet_ieq(n, "connection")) continue;
        mnet_hdrs_del(stored, n);
    }
    for (int i = 0; i < r304->n; i++) {
        const char *n = r304->v[i].name;
        if (mnet_ieq(n, "content-length") || mnet_ieq(n, "content-encoding") || mnet_ieq(n, "transfer-encoding") ||
            mnet_ieq(n, "content-range") || mnet_ieq(n, "connection")) continue;
        mnet_hdrs_add(stored, n, strlen(n), r304->v[i].value, strlen(r304->v[i].value));
    }
}

/* chave secundária do Vary: "campo=valor\n" de cada campo listado, com os valores do pedido */
char *mnet_cache_vary_key(const mnet_hdrs_t *resp, const mnet_hdrs_t *req) {
    const char *vary = mnet_hdrs_get(resp, "vary");
    mnet_buf_t b = {0};
    mnet_buf_put(&b, "", 0);
    if (vary) {
        char *all = mnet_hdrs_join(resp, "vary");
        const char *p = all;
        while (p && *p) {
            while (*p == ' ' || *p == ',') p++;
            const char *s = p;
            while (*p && *p != ',' && *p != ' ') p++;
            if (p > s) {
                char nm[64]; size_t l = (size_t)(p - s) < 63 ? (size_t)(p - s) : 63;
                for (size_t k = 0; k < l; k++) nm[k] = (char)mnet_lc((unsigned char)s[k]);
                nm[l] = 0;
                char *v = mnet_hdrs_join(req, nm);
                mnet_buf_puts(&b, nm); mnet_buf_putc(&b, '='); if (v) mnet_buf_puts(&b, v); mnet_buf_putc(&b, '\n');
                free(v);
            }
        }
        free(all);
    }
    if (!b.p) { char *e = malloc(1); if (e) *e = 0; return e; }
    return (char *)b.p;
}
