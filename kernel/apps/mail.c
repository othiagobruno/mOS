/* mOS — Mail: contas IMAP/SMTP de qualquer provedor (sem conta Apple): IMAP com TLS (993), SMTP com TLS
 * (465) ou STARTTLS (587), LOGIN/AUTH LOGIN; pastas (LIST), últimas 60 mensagens por pasta, leitura com MIME
 * (multipart, base64, quoted-printable, RFC 2047, charsets), marcar como lida, apagar, responder, encaminhar,
 * escrever e enviar; cache local para leitura sem rede. Dados em ~/Biblioteca/Mail */
#include "kit.h"
#include "net.h"
#include "crypto.h"

extern app_t app_mail;

/* ------------------------------------------------------------ conta */
typedef struct {
    char name[64], email[96], user[96], pass[96];
    char imap[96], smtp[96];
    int imap_port, smtp_port;
} account_t;

#define MAXMSG 60
#define MAXFOLD 24
typedef struct { u32 uid; bool seen, flagged, deleted; char from[96], subject[160], date[40]; i64 when; u32 size; } msg_t;

/* conexão de protocolo de linha (IMAP/SMTP) sobre TLS */
typedef struct {
    int tcp;
    tls_conn_t *tls;
    bool plain;                /* ainda sem TLS (SMTP 587 antes do STARTTLS) */
    u8 *buf;
    int len, cap;
    u64 t0;
} conn_t;

enum { I_IDLE, I_DNS, I_CONNECT, I_TLS, I_GREET, I_LOGIN, I_LIST, I_SELECT, I_FETCH, I_READY, I_BODY, I_STORE, I_ERR };
enum { S_IDLE, S_DNS, S_CONNECT, S_TLS, S_GREET, S_EHLO, S_STARTTLS, S_TLS2, S_EHLO2, S_AUTH, S_USER, S_PASS, S_FROM, S_RCPT, S_DATA, S_BODY, S_QUIT, S_DONE, S_ERR };

static struct {
    window_t *w, *cw;
    account_t acc;
    bool has_acc, loaded, setup;
    /* IMAP */
    conn_t ic;
    int ist;
    u32 ip;
    int tag;
    char folders[MAXFOLD][64];
    int nfold, cur_fold;
    msg_t m[MAXMSG];
    int nm, sel, scroll;
    char *body;                /* texto da mensagem aberta */
    u32 body_uid;
    char status[128];
    u32 want_body;             /* UID pedido */
    u32 store_uid;
    bool store_delete;
    char search[64];
    int bscroll;
    /* SMTP / composição */
    conn_t sc;
    int sst, rcpt_i, nrcpt;
    u32 sip;
    char to[256], cc[256], subj[160];
    textarea_t *ta;
    char rcpts[8][96];
    char sstatus[128];
    sbuf_t outmsg;
} ML = {.sel = -1};

/* ------------------------------------------------------------ persistência */
static void obfuscate(char *s, int n) {
    /* a senha não fica em texto puro no disco (não é cofre: use uma senha de app do provedor) */
    static const char *k = "mOS-Mail-chave-local";
    for (int i = 0; i < n && s[i]; i++) { char c = s[i] ^ k[i % 20]; s[i] = c ? c : s[i]; }
}
static void acc_save(void) {
    sbuf_t b = {0};
    char pw[96], enc[200];
    snprintf(pw, sizeof pw, "%s", ML.acc.pass);
    obfuscate(pw, (int)strlen(pw));
    kit_b64_encode((const u8 *)pw, (int)strlen(pw), enc, sizeof enc);
    sb_printf(&b, "A\t%s\t%s\t%s\t%s\t%s\t%d\t%s\t%d\n", ML.acc.name, ML.acc.email, ML.acc.user, enc, ML.acc.imap, ML.acc.imap_port, ML.acc.smtp, ML.acc.smtp_port);
    char p[200];
    kit_save(kit_lib("Mail", "conta.tsv", p, sizeof p), &b);
}
static void cache_save(void) {
    if (!ML.has_acc || ML.cur_fold >= ML.nfold) return;
    sbuf_t b = {0};
    for (int i = 0; i < ML.nm; i++) {
        msg_t *m = &ML.m[i];
        char f[96], s[160];
        kit_field(f, 96, m->from);
        kit_field(s, 160, m->subject);
        sb_printf(&b, "M\t%u\t%d\t%d\t%lld\t%s\t%s\t%s\n", m->uid, m->seen, m->flagged, (long long)m->when, m->date, f, s);
    }
    char p[200], fn[96];
    snprintf(fn, sizeof fn, "cache-%s.tsv", ML.folders[ML.cur_fold]);
    for (char *c = fn; *c; c++) if (*c == '/') *c = '_';
    kit_save(kit_lib("Mail", fn, p, sizeof p), &b);
}
static void cache_load(void) {
    char p[200], fn[96];
    snprintf(fn, sizeof fn, "cache-%s.tsv", ML.nfold ? ML.folders[ML.cur_fold] : "INBOX");
    for (char *c = fn; *c; c++) if (*c == '/') *c = '_';
    char *buf = kit_read(kit_lib("Mail", fn, p, sizeof p), NULL);
    if (!buf) return;
    ML.nm = 0;
    char *cur = buf, *ln, *f[9];
    while ((ln = kit_line(&cur)) && ML.nm < MAXMSG) {
        int n = kit_split(ln, f, 9);
        if (strcmp(f[0], "M") || n < 8) continue;
        msg_t *m = &ML.m[ML.nm++];
        memset(m, 0, sizeof *m);
        m->uid = (u32)strtol(f[1], NULL, 10); m->seen = atoi(f[2]); m->flagged = atoi(f[3]); m->when = strtol(f[4], NULL, 10);
        snprintf(m->date, 40, "%s", f[5]); snprintf(m->from, 96, "%s", f[6]); snprintf(m->subject, 160, "%s", f[7]);
    }
    kfree(buf);
}
static void acc_load(void) {
    if (ML.loaded) return;
    ML.loaded = true;
    char p[200];
    char *buf = kit_read(kit_lib("Mail", "conta.tsv", p, sizeof p), NULL);
    if (!buf) return;
    char *cur = buf, *ln, *f[10];
    while ((ln = kit_line(&cur))) {
        int n = kit_split(ln, f, 10);
        if (strcmp(f[0], "A") || n < 9) continue;
        snprintf(ML.acc.name, 64, "%s", f[1]); snprintf(ML.acc.email, 96, "%s", f[2]); snprintf(ML.acc.user, 96, "%s", f[3]);
        u8 pw[96];
        int l = kit_b64_decode(f[4], (int)strlen(f[4]), pw, 95);
        pw[l] = 0;
        obfuscate((char *)pw, l);
        snprintf(ML.acc.pass, 96, "%s", (char *)pw);
        snprintf(ML.acc.imap, 96, "%s", f[5]); ML.acc.imap_port = atoi(f[6]);
        snprintf(ML.acc.smtp, 96, "%s", f[7]); ML.acc.smtp_port = atoi(f[8]);
        ML.has_acc = true;
    }
    kfree(buf);
    if (ML.has_acc) { snprintf(ML.folders[0], 64, "INBOX"); ML.nfold = 1; cache_load(); }
}

/* ------------------------------------------------------------ conexão */
static void conn_close(conn_t *c) {
    if (c->tls) { tls_free(c->tls); c->tls = NULL; }
    if (c->tcp >= 0) { tcp_close(c->tcp); c->tcp = -1; }
    if (c->buf) { kfree(c->buf); c->buf = NULL; }
    c->len = c->cap = 0;
}
static int conn_send(conn_t *c, const char *s, int n) {
    if (n < 0) n = (int)strlen(s);
    int sent = 0;
    for (int tries = 0; sent < n && tries < 2000; tries++) {
        int k = c->plain ? tcp_send(c->tcp, s + sent, n - sent) : tls_send(c->tls, s + sent, n - sent);
        if (k < 0) return -1;
        sent += k;
        if (k == 0) net_poll();
    }
    return sent;
}
static int conn_pump(conn_t *c) {
    u8 tmp[4096];
    for (;;) {
        int k = c->plain ? tcp_recv(c->tcp, tmp, sizeof tmp) : tls_recv(c->tls, tmp, sizeof tmp);
        if (k < 0) return -1;
        if (k == 0) return 0;
        if (c->len + k + 1 > c->cap) {
            int nc = MAX(c->cap * 2, c->len + k + 65536);
            if (nc > 4 * 1024 * 1024) return -1;
            c->buf = krealloc(c->buf, nc);
            c->cap = nc;
        }
        memcpy(c->buf + c->len, tmp, k);
        c->len += k;
        c->buf[c->len] = 0;
    }
}
static void conn_consume(conn_t *c, int n) {
    if (n >= c->len) { c->len = 0; if (c->buf) c->buf[0] = 0; return; }
    memmove(c->buf, c->buf + n, c->len - n);
    c->len -= n;
    c->buf[c->len] = 0;
}

/* ------------------------------------------------------------ cabeçalhos e MIME */
static void latin1_to_utf8(const char *in, int n, char *out, int cap) {
    int k = 0;
    for (int i = 0; i < n && k < cap - 3; i++) {
        u8 c = (u8)in[i];
        if (c < 0x80) out[k++] = (char)c;
        else { out[k++] = (char)(0xC0 | c >> 6); out[k++] = (char)(0x80 | (c & 63)); }
    }
    out[k] = 0;
}
/* decodifica =?charset?B?...?= e =?charset?Q?...?= */
static void rfc2047(const char *in, char *out, int cap) {
    int k = 0;
    const char *p = in;
    bool last_enc = false;
    while (*p && k < cap - 1) {
        if (p[0] == '=' && p[1] == '?') {
            const char *cs = p + 2, *q1 = strchr(cs, '?');
            if (q1 && (q1[1] == 'B' || q1[1] == 'b' || q1[1] == 'Q' || q1[1] == 'q') && q1[2] == '?') {
                const char *txt = q1 + 3, *end = strstr(txt, "?=");
                if (end) {
                    char raw[512];
                    int rl = 0;
                    if (q1[1] == 'B' || q1[1] == 'b') rl = kit_b64_decode(txt, (int)(end - txt), (u8 *)raw, sizeof raw - 1);
                    else for (const char *t = txt; t < end && rl < (int)sizeof raw - 1; t++) {
                        if (*t == '_') raw[rl++] = ' ';
                        else if (*t == '=' && t + 2 < end) { raw[rl++] = (char)strtol((char[3]){t[1], t[2], 0}, NULL, 16); t += 2; }
                        else raw[rl++] = *t;
                    }
                    raw[rl] = 0;
                    char conv[600];
                    bool latin = !strncasecmp(cs, "iso-8859", 8) || !strncasecmp(cs, "windows-125", 11) || !strncasecmp(cs, "latin", 5);
                    if (latin) latin1_to_utf8(raw, rl, conv, sizeof conv); else snprintf(conv, sizeof conv, "%s", raw);
                    int cl = (int)strlen(conv);
                    if (k + cl < cap) { memcpy(out + k, conv, cl); k += cl; }
                    p = end + 2;
                    last_enc = true;
                    continue;
                }
            }
        }
        /* espaços entre palavras codificadas seguidas são ignorados */
        if (last_enc && (*p == ' ' || *p == '\t')) {
            const char *q = p;
            while (*q == ' ' || *q == '\t') q++;
            if (q[0] == '=' && q[1] == '?') { p = q; continue; }
        }
        last_enc = false;
        out[k++] = *p++;
    }
    out[k] = 0;
}
/* valor de um cabeçalho (desdobrado) */
static bool header_get(const char *hdr, int hl, const char *name, char *out, int cap) {
    int nl = (int)strlen(name);
    for (int i = 0; i < hl; ) {
        const char *ln = hdr + i;
        int e = i;
        while (e < hl && hdr[e] != '\n') e++;
        if ((i == 0 || hdr[i - 1] == '\n') && !strncasecmp(ln, name, nl) && ln[nl] == ':') {
            int k = 0;
            int j = i + nl + 1;
            while (j < hl && (hdr[j] == ' ' || hdr[j] == '\t')) j++;
            for (;;) {
                while (j < hl && hdr[j] != '\n' && k < cap - 1) { if (hdr[j] != '\r') out[k++] = hdr[j]; j++; }
                if (j + 1 < hl && (hdr[j + 1] == ' ' || hdr[j + 1] == '\t')) { j++; while (j < hl && (hdr[j] == ' ' || hdr[j] == '\t')) j++; if (k < cap - 1) out[k++] = ' '; continue; }
                break;
            }
            out[k] = 0;
            return true;
        }
        i = e + 1;
    }
    out[0] = 0;
    return false;
}
static const char *mon3[12] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
static i64 parse_date(const char *d) {
    /* "Sat, 7 Nov 2026 10:22:33 -0300" */
    const char *p = strchr(d, ',');
    p = p ? p + 1 : d;
    while (*p == ' ') p++;
    int day = atoi(p);
    while (*p && *p != ' ') p++;
    while (*p == ' ') p++;
    int mon = 0;
    for (int i = 0; i < 12; i++) if (!strncasecmp(p, mon3[i], 3)) mon = i + 1;
    while (*p && *p != ' ') p++;
    int yr = atoi(p);
    while (*p == ' ') p++;
    while (*p && *p != ' ') p++;
    while (*p == ' ') p++;
    int hh = atoi(p), mi = p[2] == ':' ? atoi(p + 3) : 0;
    while (*p && *p != ' ') p++;
    while (*p == ' ') p++;
    int tz = 0;
    if (*p == '+' || *p == '-') { int v = atoi(p + 1); tz = (v / 100 * 60 + v % 100) * (*p == '-' ? -1 : 1); }
    if (!mon || !yr) return 0;
    extern int g_tz_offset_min;
    return kit_days(yr, mon, day) * 86400 + (hh * 60 + mi - tz + g_tz_offset_min) * 60;
}
static void decode_qp(const char *in, int n, sbuf_t *o) {
    for (int i = 0; i < n; i++) {
        if (in[i] == '=') {
            if (i + 1 < n && (in[i + 1] == '\r' || in[i + 1] == '\n')) { i++; if (in[i] == '\r' && i + 1 < n && in[i + 1] == '\n') i++; continue; }
            if (i + 2 < n) { sb_putc(o, (char)strtol((char[3]){in[i + 1], in[i + 2], 0}, NULL, 16)); i += 2; continue; }
        }
        if (in[i] != '\r') sb_putc(o, in[i]);
    }
}
static void html_to_text(const char *in, sbuf_t *o) {
    bool tag = false, skip = false;
    for (const char *p = in; *p; p++) {
        if (*p == '<') {
            tag = true;
            if (!strncasecmp(p, "<br", 3) || !strncasecmp(p, "<p", 2) || !strncasecmp(p, "</p", 3) || !strncasecmp(p, "<div", 4) || !strncasecmp(p, "</tr", 4) || !strncasecmp(p, "<li", 3)) sb_putc(o, '\n');
            if (!strncasecmp(p, "<style", 6) || !strncasecmp(p, "<script", 7) || !strncasecmp(p, "<head", 5)) skip = true;
            if (!strncasecmp(p, "</style", 7) || !strncasecmp(p, "</script", 8) || !strncasecmp(p, "</head", 6)) skip = false;
            continue;
        }
        if (*p == '>') { tag = false; continue; }
        if (tag || skip) continue;
        if (*p == '&') {
            static const struct { const char *e; const char *r; } ents[] = {{"&nbsp;", " "}, {"&amp;", "&"}, {"&lt;", "<"}, {"&gt;", ">"}, {"&quot;", "\""}, {"&#39;", "'"},
                                                                             {"&aacute;", "á"}, {"&eacute;", "é"}, {"&iacute;", "í"}, {"&oacute;", "ó"}, {"&uacute;", "ú"}, {"&atilde;", "ã"}, {"&otilde;", "õ"}, {"&ccedil;", "ç"}};
            bool done = false;
            for (int i = 0; i < (int)ARRAY_LEN(ents); i++) { int l = (int)strlen(ents[i].e); if (!strncasecmp(p, ents[i].e, l)) { sb_puts(o, ents[i].r); p += l - 1; done = true; break; } }
            if (done) continue;
        }
        if (*p == '\r' || *p == '\t') continue;
        if (*p == '\n') { sb_putc(o, ' '); continue; }
        sb_putc(o, *p);
    }
}
/* extrai o melhor texto de uma parte MIME (recursivo) */
static bool mime_text(const char *msg, int len, sbuf_t *out, int depth) {
    if (depth > 6) return false;
    const char *body = NULL;
    for (int i = 0; i + 1 < len; i++) {
        if (msg[i] == '\n' && (msg[i + 1] == '\n' || (msg[i + 1] == '\r' && i + 2 < len && msg[i + 2] == '\n'))) { body = msg + i + (msg[i + 1] == '\n' ? 2 : 3); break; }
    }
    if (!body) body = msg + len;
    int hl = (int)(body - msg), bl = len - hl;
    char ct[300], te[64];
    header_get(msg, hl, "Content-Type", ct, sizeof ct);
    header_get(msg, hl, "Content-Transfer-Encoding", te, sizeof te);
    char lct[300];
    snprintf(lct, sizeof lct, "%s", ct);
    for (char *c = lct; *c; c++) *c = (char)tolower(*c);
    if (!strncmp(lct, "multipart/", 10)) {
        char *bp = strstr(lct, "boundary=");
        if (!bp) return false;
        char bnd[120];
        const char *src = ct + (bp - lct) + 9;
        int k = 0;
        if (*src == '"') { src++; while (*src && *src != '"' && k < 118) bnd[k++] = *src++; }
        else while (*src && *src != ';' && *src != ' ' && k < 118) bnd[k++] = *src++;
        bnd[k] = 0;
        char delim[130];
        snprintf(delim, sizeof delim, "--%s", bnd);
        int dl = (int)strlen(delim);
        /* coleta as partes; prefere text/plain e, sem ela, text/html */
        const char *parts[24];
        int plen[24], np = 0;
        const char *p = body, *end = body + bl;
        const char *cur = NULL;
        while (p < end) {
            const char *f = NULL;
            for (const char *q = p; q + dl <= end; q++) if ((q == body || q[-1] == '\n') && !memcmp(q, delim, dl)) { f = q; break; }
            if (!f) break;
            if (cur && np < 24) { parts[np] = cur; plen[np] = (int)(f - cur); np++; }
            const char *nl = f + dl;
            if (nl + 1 < end && nl[0] == '-' && nl[1] == '-') break;
            while (nl < end && *nl != '\n') nl++;
            cur = nl + 1;
            p = cur;
        }
        bool alt = strstr(lct, "alternative") != NULL;
        if (alt) {
            for (int pass = 0; pass < 2; pass++)
                for (int i = 0; i < np; i++) {
                    char pct[200];
                    header_get(parts[i], plen[i], "Content-Type", pct, sizeof pct);
                    for (char *c = pct; *c; c++) *c = (char)tolower(*c);
                    if ((pass == 0 && (strstr(pct, "text/plain") || !pct[0])) || (pass == 1 && strstr(pct, "text/html")) || strstr(pct, "multipart/"))
                        if (mime_text(parts[i], plen[i], out, depth + 1)) return true;
                }
            return false;
        }
        bool any = false;
        for (int i = 0; i < np; i++) {
            char disp[120];
            header_get(parts[i], plen[i], "Content-Disposition", disp, sizeof disp);
            if (!strncasecmp(disp, "attachment", 10)) {
                char fn[100] = "anexo";
                char *q = strstr(disp, "filename=");
                if (q) { q += 9; if (*q == '"') q++; int k2 = 0; while (*q && *q != '"' && *q != ';' && k2 < 99) fn[k2++] = *q++; fn[k2] = 0; }
                sb_printf(out, "\n[Anexo: %s]\n", fn);
                continue;
            }
            if (!any && mime_text(parts[i], plen[i], out, depth + 1)) any = true;
        }
        return any;
    }
    if (lct[0] && strncmp(lct, "text/", 5)) return false;
    sbuf_t dec = {0};
    if (!strncasecmp(te, "base64", 6)) {
        u8 *b = kmalloc(bl + 1);
        int n = kit_b64_decode(body, bl, b, bl);
        sb_putn(&dec, (char *)b, n);
        kfree(b);
    } else if (!strncasecmp(te, "quoted-printable", 16)) decode_qp(body, bl, &dec);
    else sb_putn(&dec, body, bl);
    char *txt = dec.p ? dec.p : (char *)"";
    char *conv = NULL;
    char *cs = strstr(lct, "charset=");
    if (cs) {
        cs += 8;
        if (*cs == '"') cs++;
        char hint[32];
        int k = 0;
        while (cs[k] && cs[k] != '"' && cs[k] != ';' && k < 31) { hint[k] = cs[k]; k++; }
        hint[k] = 0;
        if (strcmp(hint, "utf-8") && strcmp(hint, "us-ascii")) { usize ol; conv = charset_to_utf8((const u8 *)txt, dec.len, hint, &ol); if (conv) txt = conv; }
    }
    if (strstr(lct, "text/html")) html_to_text(txt, out);
    else for (char *c = txt; *c; c++) if (*c != '\r') sb_putc(out, *c);
    if (conv) kfree(conv);
    sb_free(&dec);
    return true;
}

/* ------------------------------------------------------------ IMAP */
static void ist_fail(const char *why) {
    snprintf(ML.status, sizeof ML.status, "%s", why);
    ML.ist = I_ERR;
    conn_close(&ML.ic);
}
static void imap_cmd(const char *fmt, ...) {
    char cmd[512], line[600];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof cmd, fmt, ap);
    va_end(ap);
    ML.tag++;
    snprintf(line, sizeof line, "m%d %s\r\n", ML.tag, cmd);
    conn_send(&ML.ic, line, -1);
}
/* procura a linha marcada com a etiqueta atual, respeitando literais {n}; devolve o fim dela ou -1 */
static int imap_tagged(conn_t *c, char *status, int scap) {
    char tag[16];
    int tl = snprintf(tag, sizeof tag, "m%d ", ML.tag);
    int i = 0;
    while (i < c->len) {
        int e = i;
        while (e < c->len && c->buf[e] != '\n') e++;
        if (e >= c->len) return -1;
        /* literal no fim da linha */
        int le = e;
        if (le > i && c->buf[le - 1] == '\r') le--;
        if (le > i && c->buf[le - 1] == '}') {
            int b = le - 2;
            while (b > i && isdigit(c->buf[b])) b--;
            if (c->buf[b] == '{') { int n = atoi((char *)c->buf + b + 1); i = e + 1 + n; continue; }
        }
        if (!memcmp(c->buf + i, tag, tl)) {
            int k = 0;
            for (int j = i + tl; j < le && k < scap - 1; j++) status[k++] = (char)c->buf[j];
            status[k] = 0;
            return e + 1;
        }
        i = e + 1;
    }
    return -1;
}
static void imap_start(void) {
    if (!ML.has_acc) return;
    conn_close(&ML.ic);
    ML.ic.tcp = -1;
    ML.ist = I_DNS;
    ML.tag = 0;
    snprintf(ML.status, sizeof ML.status, "Conectando a %s…", ML.acc.imap);
}
static void parse_fetch(conn_t *c, int end) {
    ML.nm = 0;
    char *p = (char *)c->buf, *stop = (char *)c->buf + end;
    while (p < stop && ML.nm < MAXMSG) {
        char *f = strstr(p, " FETCH (");
        if (!f || f >= stop) break;
        msg_t m;
        memset(&m, 0, sizeof m);
        char *u = strstr(f, "UID ");
        char *nextstar = strstr(f + 8, "\r\n* ");
        if (!nextstar || nextstar > stop) nextstar = stop;
        if (u && u < nextstar) m.uid = (u32)strtol(u + 4, NULL, 10);
        char *fl = strstr(f, "FLAGS (");
        if (fl && fl < nextstar) { char *fe = strchr(fl, ')'); if (fe) { m.seen = strstr(fl, "\\Seen") && strstr(fl, "\\Seen") < fe; m.flagged = strstr(fl, "\\Flagged") && strstr(fl, "\\Flagged") < fe; } }
        char *sz = strstr(f, "RFC822.SIZE ");
        if (sz && sz < nextstar) m.size = (u32)strtol(sz + 12, NULL, 10);
        char *lit = strstr(f, "]");
        char *ob = lit ? strchr(lit, '{') : NULL;
        if (ob && ob < nextstar) {
            int n = atoi(ob + 1);
            char *h = strchr(ob, '\n');
            if (h && h + 1 + n <= stop) {
                h++;
                char v[400];
                header_get(h, n, "From", v, sizeof v);
                char dv[400];
                rfc2047(v, dv, sizeof dv);
                /* "Nome <email>" → Nome */
                char *lt = strchr(dv, '<');
                if (lt && lt > dv) { *lt = 0; char *t = lt - 1; while (t > dv && (*t == ' ' || *t == '"')) *t-- = 0; char *s0 = dv; while (*s0 == '"' || *s0 == ' ') s0++; snprintf(m.from, 96, "%s", s0); }
                else snprintf(m.from, 96, "%s", dv);
                header_get(h, n, "Subject", v, sizeof v);
                rfc2047(v, dv, sizeof dv);
                snprintf(m.subject, 160, "%s", dv[0] ? dv : "(sem assunto)");
                header_get(h, n, "Date", v, sizeof v);
                m.when = parse_date(v);
                p = h + n;
            } else p = nextstar;
        } else p = nextstar;
        if (m.when) {
            int y, mo, d;
            kit_from_days(m.when / 86400, &y, &mo, &d);
            int ty, tm, td;
            kit_today(&ty, &tm, &td);
            if (y == ty && mo == tm && d == td) snprintf(m.date, 40, "%02d:%02d", (int)(m.when % 86400 / 3600), (int)(m.when % 3600 / 60));
            else snprintf(m.date, 40, "%02d/%02d/%02d", d, mo, y % 100);
        }
        if (m.uid) ML.m[ML.nm++] = m;
    }
    /* mais recentes primeiro */
    for (int i = 1; i < ML.nm; i++) { msg_t t = ML.m[i]; int j = i - 1; while (j >= 0 && ML.m[j].uid < t.uid) { ML.m[j + 1] = ML.m[j]; j--; } ML.m[j + 1] = t; }
}
static void imap_poll(void) {
    conn_t *c = &ML.ic;
    char st[160];
    int e;
    switch (ML.ist) {
    case I_DNS: {
        int r = dns_resolve(ML.acc.imap, &ML.ip);
        if (r < 0) ist_fail("Servidor IMAP não encontrado.");
        else if (r > 0) { c->tcp = tcp_connect(ML.ip, (u16)ML.acc.imap_port); c->t0 = timer_ticks(); ML.ist = c->tcp < 0 ? I_ERR : I_CONNECT; }
    } break;
    case I_CONNECT: {
        int s = tcp_state(c->tcp);
        if (s == TCP_OPEN) { c->tls = tls_new(c->tcp, ML.acc.imap); c->plain = false; ML.ist = I_TLS; }
        else if (s == TCP_ERROR || s == TCP_CLOSED || timer_ticks() - c->t0 > 15000) ist_fail("Não foi possível conectar ao servidor IMAP.");
    } break;
    case I_TLS: {
        int h = tls_handshake(c->tls);
        if (h > 0) ML.ist = I_GREET;
        else if (h < 0) { snprintf(st, sizeof st, "TLS: %s", tls_error(c->tls)); ist_fail(st); }
    } break;
    default: break;
    }
    if (ML.ist < I_GREET || ML.ist == I_ERR || ML.ist == I_IDLE) return;
    if (conn_pump(c) < 0) { ist_fail("A conexão foi encerrada pelo servidor."); return; }
    switch (ML.ist) {
    case I_GREET:
        if (c->len && strchr((char *)c->buf, '\n')) {
            if (strncmp((char *)c->buf, "* OK", 4)) { ist_fail("O servidor recusou a conexão."); return; }
            conn_consume(c, c->len);
            /* LOGIN com aspas (senhas com espaços ou aspas são escapadas) */
            char u[200], p[200];
            int k = 0;
            for (const char *q = ML.acc.user; *q && k < 190; q++) { if (*q == '"' || *q == '\\') u[k++] = '\\'; u[k++] = *q; }
            u[k] = 0;
            k = 0;
            for (const char *q = ML.acc.pass; *q && k < 190; q++) { if (*q == '"' || *q == '\\') p[k++] = '\\'; p[k++] = *q; }
            p[k] = 0;
            imap_cmd("LOGIN \"%s\" \"%s\"", u, p);
            secure_zero(p, sizeof p);
            ML.ist = I_LOGIN;
            snprintf(ML.status, sizeof ML.status, "Entrando…");
        }
        break;
    case I_LOGIN:
        if ((e = imap_tagged(c, st, sizeof st)) >= 0) {
            conn_consume(c, e);
            if (strncmp(st, "OK", 2)) { ist_fail("Usuário ou senha incorretos (alguns provedores exigem uma senha de app)."); return; }
            imap_cmd("LIST \"\" \"*\"");
            ML.ist = I_LIST;
        }
        break;
    case I_LIST:
        if ((e = imap_tagged(c, st, sizeof st)) >= 0) {
            ML.nfold = 0;
            snprintf(ML.folders[ML.nfold++], 64, "INBOX");
            char *p = (char *)c->buf;
            while ((p = strstr(p, "* LIST ")) && p < (char *)c->buf + e && ML.nfold < MAXFOLD) {
                char *nl = strchr(p, '\n');
                if (!nl) break;
                *nl = 0;
                if (!strstr(p, "\\Noselect")) {
                    char *nm = nl - 1;
                    while (nm > p && *nm == '\r') nm--;
                    char name[64];
                    if (*nm == '"') { char *b = nm - 1; while (b > p && *b != '"') b--; snprintf(name, 64, "%.*s", (int)(nm - b - 1), b + 1); }
                    else { char *b = nm; while (b > p && *b != ' ') b--; snprintf(name, 64, "%.*s", (int)(nm - b), b + 1); }
                    if (strcasecmp(name, "INBOX")) snprintf(ML.folders[ML.nfold++], 64, "%s", name);
                }
                p = nl + 1;
            }
            conn_consume(c, e);
            if (ML.cur_fold >= ML.nfold) ML.cur_fold = 0;
            imap_cmd("SELECT \"%s\"", ML.folders[ML.cur_fold]);
            ML.ist = I_SELECT;
        }
        break;
    case I_SELECT:
        if ((e = imap_tagged(c, st, sizeof st)) >= 0) {
            int exists = 0;
            char *x = strstr((char *)c->buf, " EXISTS");
            if (x && x < (char *)c->buf + e) { char *b = x - 1; while (b > (char *)c->buf && isdigit(*b)) b--; exists = atoi(b + 1); }
            conn_consume(c, e);
            if (strncmp(st, "OK", 2)) { ist_fail("Não foi possível abrir a pasta."); return; }
            if (!exists) { ML.nm = 0; ML.ist = I_READY; snprintf(ML.status, sizeof ML.status, "Pasta vazia"); cache_save(); return; }
            imap_cmd("FETCH %d:%d (UID FLAGS RFC822.SIZE BODY.PEEK[HEADER.FIELDS (FROM SUBJECT DATE)])", MAX(1, exists - MAXMSG + 1), exists);
            ML.ist = I_FETCH;
            snprintf(ML.status, sizeof ML.status, "Baixando mensagens…");
        }
        break;
    case I_FETCH:
        if ((e = imap_tagged(c, st, sizeof st)) >= 0) {
            parse_fetch(c, e);
            conn_consume(c, e);
            cache_save();
            ML.ist = I_READY;
            int unread = 0;
            for (int i = 0; i < ML.nm; i++) if (!ML.m[i].seen) unread++;
            if (!ML.cur_fold) dock_set_badge(&app_mail, unread);
            snprintf(ML.status, sizeof ML.status, "Atualizado agora");
        }
        break;
    case I_READY:
        if (ML.want_body) {
            imap_cmd("UID FETCH %u BODY.PEEK[]", ML.want_body);
            ML.body_uid = ML.want_body;
            ML.want_body = 0;
            ML.ist = I_BODY;
        } else if (ML.store_uid) {
            if (ML.store_delete) imap_cmd("UID STORE %u +FLAGS (\\Deleted)", ML.store_uid);
            else imap_cmd("UID STORE %u +FLAGS (\\Seen)", ML.store_uid);
            ML.ist = I_STORE;
        }
        break;
    case I_BODY:
        if ((e = imap_tagged(c, st, sizeof st)) >= 0) {
            char *ob = strchr((char *)c->buf, '{');
            sbuf_t out = {0};
            if (ob && ob < (char *)c->buf + e) {
                int n = atoi(ob + 1);
                char *h = strchr(ob, '\n');
                if (h && h + 1 + n <= (char *)c->buf + e) {
                    if (!mime_text(h + 1, n, &out, 0)) sb_puts(&out, "(Esta mensagem não tem texto que o Mail consiga mostrar.)");
                }
            }
            if (ML.body) kfree(ML.body);
            ML.body = out.p ? out.p : strdup("");
            conn_consume(c, e);
            ML.ist = I_READY;
            for (int i = 0; i < ML.nm; i++) if (ML.m[i].uid == ML.body_uid && !ML.m[i].seen) { ML.m[i].seen = true; ML.store_uid = ML.body_uid; ML.store_delete = false; }
        }
        break;
    case I_STORE:
        if ((e = imap_tagged(c, st, sizeof st)) >= 0) {
            conn_consume(c, e);
            if (ML.store_delete) { imap_cmd("EXPUNGE"); ML.store_delete = false; ML.store_uid = 0; break; }
            ML.store_uid = 0;
            ML.ist = I_READY;
            cache_save();
        }
        break;
    }
    (void)e;
}

/* ------------------------------------------------------------ SMTP */
static void smtp_fail(const char *why) { snprintf(ML.sstatus, sizeof ML.sstatus, "%s", why); ML.sst = S_ERR; conn_close(&ML.sc); }
static void smtp_line(const char *fmt, ...) {
    char l[600];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(l, sizeof l - 2, fmt, ap);
    va_end(ap);
    l[n] = '\r'; l[n + 1] = '\n';
    conn_send(&ML.sc, l, n + 2);
}
/* resposta completa (última linha "NNN "): devolve o código ou 0 */
static int smtp_reply(void) {
    conn_t *c = &ML.sc;
    if (!c->len) return 0;
    int i = 0, code = 0;
    while (i < c->len) {
        int e = i;
        while (e < c->len && c->buf[e] != '\n') e++;
        if (e >= c->len) return 0;
        if (e - i >= 4 && c->buf[i + 3] == ' ') { code = atoi((char *)c->buf + i); conn_consume(c, e + 1); return code; }
        i = e + 1;
    }
    return 0;
}
static void smtp_send(void) {
    /* destinatários */
    ML.nrcpt = 0;
    char all[520];
    snprintf(all, sizeof all, "%s,%s", ML.to, ML.cc);
    char *p = all;
    while (*p && ML.nrcpt < 8) {
        while (*p == ',' || *p == ' ' || *p == ';') p++;
        char *e = p;
        while (*e && *e != ',' && *e != ';') e++;
        char addr[96];
        int l = MIN((int)(e - p), 95);
        memcpy(addr, p, l);
        addr[l] = 0;
        while (l && addr[l - 1] == ' ') addr[--l] = 0;
        char *lt = strchr(addr, '<');
        if (lt) { char *gt = strchr(lt, '>'); if (gt) *gt = 0; memmove(addr, lt + 1, strlen(lt)); }
        if (strchr(addr, '@')) snprintf(ML.rcpts[ML.nrcpt++], 96, "%s", addr);
        p = e;
    }
    if (!ML.nrcpt) { snprintf(ML.sstatus, sizeof ML.sstatus, "Informe pelo menos um destinatário válido."); return; }
    /* mensagem RFC 5322 em UTF-8 (assunto em RFC 2047) */
    sb_free(&ML.outmsg);
    datetime_t d;
    rtc_now(&d);
    static const char *wd[7] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
    extern int g_tz_offset_min;
    int tz = g_tz_offset_min;
    char subj64[400];
    kit_b64_encode((const u8 *)ML.subj, (int)strlen(ML.subj), subj64, sizeof subj64);
    sb_printf(&ML.outmsg, "From: %s <%s>\r\nTo: %s\r\n", ML.acc.name, ML.acc.email, ML.to);
    if (ML.cc[0]) sb_printf(&ML.outmsg, "Cc: %s\r\n", ML.cc);
    sb_printf(&ML.outmsg, "Subject: =?UTF-8?B?%s?=\r\nDate: %s, %d %s %d %02d:%02d:%02d %c%02d%02d\r\n", subj64, wd[d.weekday % 7], d.day, mon3[d.month - 1], d.year,
              d.hour, d.minute, d.second, tz < 0 ? '-' : '+', (tz < 0 ? -tz : tz) / 60, (tz < 0 ? -tz : tz) % 60);
    sb_printf(&ML.outmsg, "Message-ID: <%08x.%08x@mos>\r\nMIME-Version: 1.0\r\nContent-Type: text/plain; charset=utf-8\r\nContent-Transfer-Encoding: 8bit\r\nX-Mailer: Mail do mOS\r\n\r\n", rand32(), rand32());
    int len;
    const char *body = textarea_text(ML.ta, &len);
    bool bol = true;
    for (int i = 0; i < len; i++) {
        if (bol && body[i] == '.') sb_putc(&ML.outmsg, '.');   /* dot-stuffing */
        if (body[i] == '\n') { sb_puts(&ML.outmsg, "\r\n"); bol = true; continue; }
        sb_putc(&ML.outmsg, body[i]);
        bol = false;
    }
    sb_puts(&ML.outmsg, "\r\n.\r\n");
    conn_close(&ML.sc);
    ML.sc.tcp = -1;
    ML.sst = S_DNS;
    snprintf(ML.sstatus, sizeof ML.sstatus, "Enviando…");
}
static void smtp_poll(void) {
    conn_t *c = &ML.sc;
    if (ML.sst == S_IDLE || ML.sst == S_DONE || ML.sst == S_ERR) return;
    if (ML.sst == S_DNS) {
        int r = dns_resolve(ML.acc.smtp, &ML.sip);
        if (r < 0) smtp_fail("Servidor SMTP não encontrado.");
        else if (r > 0) { c->tcp = tcp_connect(ML.sip, (u16)ML.acc.smtp_port); c->t0 = timer_ticks(); ML.sst = S_CONNECT; }
        return;
    }
    if (ML.sst == S_CONNECT) {
        int s = tcp_state(c->tcp);
        if (s == TCP_OPEN) {
            if (ML.acc.smtp_port == 465) { c->tls = tls_new(c->tcp, ML.acc.smtp); c->plain = false; ML.sst = S_TLS; }
            else { c->plain = true; ML.sst = S_GREET; }
        } else if (s == TCP_ERROR || s == TCP_CLOSED || timer_ticks() - c->t0 > 15000) smtp_fail("Não foi possível conectar ao servidor SMTP.");
        return;
    }
    if (ML.sst == S_TLS || ML.sst == S_TLS2) {
        int h = tls_handshake(c->tls);
        if (h > 0) { if (ML.sst == S_TLS) ML.sst = S_GREET; else { smtp_line("EHLO mos.local"); ML.sst = S_EHLO2; } }
        else if (h < 0) smtp_fail("Falha no TLS com o servidor SMTP.");
        return;
    }
    if (conn_pump(c) < 0) { smtp_fail("O servidor SMTP encerrou a conexão."); return; }
    int code = smtp_reply();
    if (!code) return;
    char u64[200];
    switch (ML.sst) {
    case S_GREET: if (code != 220) { smtp_fail("O servidor SMTP recusou a conexão."); return; } smtp_line("EHLO mos.local"); ML.sst = S_EHLO; break;
    case S_EHLO:
        if (code != 250) { smtp_fail("EHLO recusado."); return; }
        if (c->plain) { smtp_line("STARTTLS"); ML.sst = S_STARTTLS; }
        else { smtp_line("AUTH LOGIN"); ML.sst = S_AUTH; }
        break;
    case S_STARTTLS:
        if (code != 220) { smtp_fail("O servidor não aceita STARTTLS."); return; }
        c->tls = tls_new(c->tcp, ML.acc.smtp);
        c->plain = false;
        ML.sst = S_TLS2;
        break;
    case S_EHLO2: if (code != 250) { smtp_fail("EHLO recusado."); return; } smtp_line("AUTH LOGIN"); ML.sst = S_AUTH; break;
    case S_AUTH: if (code != 334) { smtp_fail("O servidor não aceita AUTH LOGIN."); return; } kit_b64_encode((const u8 *)ML.acc.user, (int)strlen(ML.acc.user), u64, sizeof u64); smtp_line("%s", u64); ML.sst = S_USER; break;
    case S_USER: if (code != 334) { smtp_fail("Usuário recusado."); return; } kit_b64_encode((const u8 *)ML.acc.pass, (int)strlen(ML.acc.pass), u64, sizeof u64); smtp_line("%s", u64); secure_zero(u64, sizeof u64); ML.sst = S_PASS; break;
    case S_PASS: if (code != 235) { smtp_fail("Usuário ou senha incorretos para envio."); return; } smtp_line("MAIL FROM:<%s>", ML.acc.email); ML.sst = S_FROM; break;
    case S_FROM: if (code != 250) { smtp_fail("Remetente recusado."); return; } ML.rcpt_i = 0; smtp_line("RCPT TO:<%s>", ML.rcpts[0]); ML.sst = S_RCPT; break;
    case S_RCPT:
        if (code != 250 && code != 251) { snprintf(u64, sizeof u64, "Destinatário recusado: %s", ML.rcpts[ML.rcpt_i]); smtp_fail(u64); return; }
        if (++ML.rcpt_i < ML.nrcpt) smtp_line("RCPT TO:<%s>", ML.rcpts[ML.rcpt_i]);
        else { smtp_line("DATA"); ML.sst = S_DATA; }
        break;
    case S_DATA: if (code != 354) { smtp_fail("O servidor recusou a mensagem."); return; } conn_send(c, ML.outmsg.p, ML.outmsg.len); ML.sst = S_BODY; break;
    case S_BODY:
        if (code != 250) { smtp_fail("A mensagem foi rejeitada pelo servidor."); return; }
        smtp_line("QUIT");
        ML.sst = S_DONE;
        snprintf(ML.sstatus, sizeof ML.sstatus, "Mensagem enviada.");
        notify("Mail", "Mensagem enviada.");
        /* cópia local em Enviados */
        {
            char p[200];
            kit_lib("Mail", "enviados.mbox", p, sizeof p);
            vnode_t *v = vfs_lookup(p);
            if (!v) v = vfs_write_file(p, "", 0);
            if (v) { vfs_append(v, "From mos\r\n", 10); vfs_append(v, ML.outmsg.p, ML.outmsg.len); }
        }
        conn_close(c);
        if (ML.cw) { wm_close(ML.cw); }
        break;
    default: break;
    }
}

/* ------------------------------------------------------------ composição */
static void compose_paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    smtp_poll();
    gfx_fill(s, 0, w->toolbar_h, u->cw, u->ch - w->toolbar_h, T.content_bg);
    bool busy = ML.sst != S_IDLE && ML.sst != S_DONE && ML.sst != S_ERR;
    if (ui_button_style(u, UI_ID, rect_make(u->cw - 12 - 90, TOOLBAR_Y(28), 90, 28), "Enviar", BTN_FILLED, !busy && ML.has_acc, NULL)) smtp_send();
    int y = w->toolbar_h + 8;
    const char *lab[3] = {"Para:", "Cc:", "Assunto:"};
    char *buf[3] = {ML.to, ML.cc, ML.subj};
    int cap[3] = {sizeof ML.to, sizeof ML.cc, sizeof ML.subj};
    for (int i = 0; i < 3; i++) {
        ui_label(u, 16, y + 7, lab[i], FONT_REGULAR, 13, T.text2);
        ui_textfield(u, 124000 + i, rect_make(84, y, u->cw - 100, 26), buf[i], cap[i], "");
        y += 32;
        gfx_fill(s, 16, y - 3, u->cw - 32, 1, T.separator);
    }
    rect_t body = rect_make(0, y, u->cw, u->ch - y - 26);
    textarea_paint(w, u, ML.ta, body);
    if (ML.sstatus[0]) ui_label(u, 16, u->ch - 22, ML.sstatus, FONT_REGULAR, 12, ML.sst == S_ERR ? RGB(255, 69, 58) : T.text2);
    if (busy) u->repaint = true;
}
static void compose_close(window_t *w) { ML.cw = NULL; }
static void compose(const char *to, const char *subj, const char *quote) {
    if (!ML.ta) ML.ta = textarea_new(14, 0, 0, 16);
    snprintf(ML.to, sizeof ML.to, "%s", to ? to : "");
    ML.cc[0] = 0;
    snprintf(ML.subj, sizeof ML.subj, "%s", subj ? subj : "");
    sbuf_t b = {0};
    if (quote) {
        sb_puts(&b, "\n\n");
        const char *p = quote;
        sb_puts(&b, "> ");
        for (; *p; p++) { sb_putc(&b, *p); if (*p == '\n' && p[1]) sb_puts(&b, "> "); }
    }
    textarea_set(ML.ta, b.p ? b.p : "", -1);
    sb_free(&b);
    ML.sst = S_IDLE;
    ML.sstatus[0] = 0;
    if (ML.cw) { wm_focus(ML.cw); return; }
    ML.cw = wm_create(&app_mail, "Nova Mensagem", -1, -1, 640, 520, WIN_RESIZABLE);
    if (!ML.cw) return;
    ML.cw->paint = compose_paint;
    ML.cw->on_close = compose_close;
    ML.cw->toolbar_h = TOOLBAR_H;
}

/* ------------------------------------------------------------ configuração da conta */
static struct { char name[64], email[96], pass[96], imap[96], smtp[96]; int iport, sport; } SU = {.iport = 993, .sport = 465};
static void guess_servers(void) {
    const char *at = strchr(SU.email, '@');
    if (!at) return;
    const char *dom = at + 1;
    static const struct { const char *d, *i, *s; int sp; } known[] = {
        {"gmail.com", "imap.gmail.com", "smtp.gmail.com", 465}, {"googlemail.com", "imap.gmail.com", "smtp.gmail.com", 465},
        {"outlook.com", "outlook.office365.com", "smtp-mail.outlook.com", 587}, {"hotmail.com", "outlook.office365.com", "smtp-mail.outlook.com", 587},
        {"live.com", "outlook.office365.com", "smtp-mail.outlook.com", 587}, {"yahoo.com", "imap.mail.yahoo.com", "smtp.mail.yahoo.com", 465},
        {"yahoo.com.br", "imap.mail.yahoo.com", "smtp.mail.yahoo.com", 465}, {"uol.com.br", "imap.uol.com.br", "smtps.uol.com.br", 465},
        {"bol.com.br", "imap.bol.com.br", "smtps.bol.com.br", 465}, {"terra.com.br", "imap.terra.com.br", "smtp.terra.com.br", 587},
        {"zoho.com", "imap.zoho.com", "smtp.zoho.com", 465}, {"gmx.com", "imap.gmx.com", "mail.gmx.com", 465}, {"aol.com", "imap.aol.com", "smtp.aol.com", 465},
        {"yandex.com", "imap.yandex.com", "smtp.yandex.com", 465}, {"mail.com", "imap.mail.com", "smtp.mail.com", 465}};
    for (int i = 0; i < (int)ARRAY_LEN(known); i++)
        if (!strcasecmp(dom, known[i].d)) { snprintf(SU.imap, 96, "%s", known[i].i); snprintf(SU.smtp, 96, "%s", known[i].s); SU.sport = known[i].sp; SU.iport = 993; return; }
    snprintf(SU.imap, 96, "imap.%s", dom);
    snprintf(SU.smtp, 96, "smtp.%s", dom);
}
static void paint_setup(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    gfx_fill(s, 0, w->toolbar_h, u->cw, u->ch - w->toolbar_h, T.content_bg);
    int cw = MIN(460, u->cw - 40), x = (u->cw - cw) / 2, y = w->toolbar_h + 24;
    gfx_round_rect_grad(s, x + cw / 2 - 32, y, 64, 64, 15, RGB(90, 180, 255), RGB(20, 110, 240));
    gfx_round_rect(s, x + cw / 2 - 20, y + 20, 40, 26, 4, RGB(255, 255, 255));
    gfx_line(s, x + cw / 2 - 18, y + 22, x + cw / 2, y + 36, 2, RGB(20, 110, 240));
    gfx_line(s, x + cw / 2 + 18, y + 22, x + cw / 2, y + 36, 2, RGB(20, 110, 240));
    y += 78;
    ui_label_center(u, rect_make(0, y, u->cw, 26), "Adicionar Conta de E-mail", FONT_BOLD, 20, T.text);
    y += 28;
    ui_label_center(u, rect_make(0, y, u->cw, 18), "Funciona com Gmail, Outlook, Yahoo, UOL e qualquer servidor IMAP/SMTP.", FONT_REGULAR, 12.5f, T.text2);
    y += 32;
    struct { const char *l; char *b; int cap; const char *ph; } f[5] = {{"Nome", SU.name, 64, "Seu nome"}, {"E-mail", SU.email, 96, "nome@exemplo.com"},
                                                                         {"Senha", SU.pass, 96, "Senha (ou senha de app)"}, {"Servidor IMAP", SU.imap, 96, "imap.exemplo.com"},
                                                                         {"Servidor SMTP", SU.smtp, 96, "smtp.exemplo.com"}};
    for (int i = 0; i < 5; i++) {
        ui_label(u, x, y + 6, f[i].l, FONT_REGULAR, 13, T.text2);
        char before[96];
        snprintf(before, sizeof before, "%s", f[i].b);
        if (i == 2) {
            /* senha: mostra pontos */
            char masked[96];
            int l = (int)strlen(SU.pass);
            for (int k = 0; k < l && k < 95; k++) masked[k] = '*';
            masked[MIN(l, 95)] = 0;
            ui_textfield(u, 125000 + i, rect_make(x + 120, y, cw - 120, 26), SU.pass, 96, f[i].ph);
        } else ui_textfield(u, 125000 + i, rect_make(x + 120, y, cw - 120 - (i >= 3 ? 70 : 0), 26), f[i].b, f[i].cap, f[i].ph);
        if (i == 1 && strcmp(before, SU.email) && strchr(SU.email, '@') && strchr(strchr(SU.email, '@'), '.')) guess_servers();
        if (i == 3) ui_stepper(u, UI_ID, rect_make(x + cw - 64, y, 64, 26), &SU.iport, 1, 65535);
        if (i == 4) ui_stepper(u, UI_ID, rect_make(x + cw - 64, y, 64, 26), &SU.sport, 1, 65535);
        y += 34;
    }
    ui_label(u, x, y + 4, "IMAP usa TLS (993). SMTP: 465 (TLS) ou 587 (STARTTLS).", FONT_REGULAR, 11.5f, T.text3);
    y += 30;
    bool ok = SU.email[0] && SU.pass[0] && SU.imap[0] && SU.smtp[0];
    if (ui_button_style(u, UI_ID, rect_make(x + cw - 120, y, 120, 30), "Entrar", BTN_FILLED | BTN_DEFAULT, ok, NULL)) {
        snprintf(ML.acc.name, 64, "%s", SU.name[0] ? SU.name : SU.email);
        snprintf(ML.acc.email, 96, "%s", SU.email);
        snprintf(ML.acc.user, 96, "%s", SU.email);
        snprintf(ML.acc.pass, 96, "%s", SU.pass);
        snprintf(ML.acc.imap, 96, "%s", SU.imap);
        snprintf(ML.acc.smtp, 96, "%s", SU.smtp);
        ML.acc.imap_port = SU.iport;
        ML.acc.smtp_port = SU.sport;
        ML.has_acc = true;
        ML.setup = false;
        ML.nfold = 1;
        snprintf(ML.folders[0], 64, "INBOX");
        ML.cur_fold = 0;
        acc_save();
        secure_zero(SU.pass, sizeof SU.pass);
        imap_start();
    }
    if (ML.has_acc && ui_button_style(u, UI_ID, rect_make(x + cw - 120 - 8 - 100, y, 100, 30), "Cancelar", BTN_GRAY, true, NULL)) ML.setup = false;
}

/* ------------------------------------------------------------ janela principal */
static const char *folder_label(const char *f) {
    if (!strcasecmp(f, "INBOX")) return "Caixa de Entrada";
    const char *b = strrchr(f, '/');
    b = b ? b + 1 : f;
    if (!strcasecmp(b, "Sent") || !strcasecmp(b, "Sent Mail") || !strcasecmp(b, "Sent Items") || !strcasecmp(b, "Enviados")) return "Enviados";
    if (!strcasecmp(b, "Drafts") || !strcasecmp(b, "Rascunhos")) return "Rascunhos";
    if (!strcasecmp(b, "Trash") || !strcasecmp(b, "Deleted Items") || !strcasecmp(b, "Lixeira") || !strcasecmp(b, "Bin")) return "Lixo";
    if (!strcasecmp(b, "Spam") || !strcasecmp(b, "Junk") || !strcasecmp(b, "Junk Email")) return "Lixo Eletrônico";
    if (!strcasecmp(b, "Archive") || !strcasecmp(b, "All Mail")) return b[0] == 'A' && b[1] == 'l' ? "Todos os E-mails" : "Arquivo";
    return b;
}
static void paint(window_t *w, ui_t *u) {
    surface_t *s = u->s;
    acc_load();
    imap_poll();
    if (!ML.has_acc || ML.setup) { w->sidebar_w = 0; paint_setup(w, u); return; }
    w->sidebar_w = 220;
    int sb = w->sidebar_w;
    /* pastas */
    int y = w->toolbar_h + 4;
    ui_label(u, 18, y + 2, ML.acc.email, FONT_BOLD, 11.5f, T.text2);
    y += 22;
    for (int i = 0; i < ML.nfold; i++) {
        if (ui_sidebar_item(u, 126000 + i, rect_make(10, y, sb - 20, 26), folder_label(ML.folders[i]), ML.cur_fold == i, i == 0 ? glyph_sb_download : glyph_sb_document)) {
            if (ML.cur_fold != i) {
                ML.cur_fold = i; ML.sel = -1; ML.nm = 0;
                cache_load();
                if (ML.ist == I_READY) { imap_cmd("SELECT \"%s\"", ML.folders[i]); ML.ist = I_SELECT; }
            }
        }
        y += 28;
        if (y > u->ch - 60) break;
    }
    if (ui_button_style(u, UI_ID, rect_make(10, u->ch - 36, sb - 20, 28), "Configurar Conta…", BTN_PLAIN_GRAY, true, NULL)) {
        ML.setup = true;
        snprintf(SU.name, 64, "%s", ML.acc.name); snprintf(SU.email, 96, "%s", ML.acc.email);
        snprintf(SU.imap, 96, "%s", ML.acc.imap); snprintf(SU.smtp, 96, "%s", ML.acc.smtp); SU.iport = ML.acc.imap_port; SU.sport = ML.acc.smtp_port;
    }
    /* barra de ferramentas */
    int tx = u->cw - 12;
    tx -= 200;
    ui_searchfield(u, UI_ID, rect_make(tx, TOOLBAR_Y(28), 200, 28), ML.search, sizeof ML.search);
    tx -= 8 + 34;
    if (ui_button_icon(u, UI_ID, rect_make(tx, TOOLBAR_Y(28), 34, 28), glyph_plus, true)) compose(NULL, NULL, NULL);
    ui_tooltip(u, UI_ID, rect_make(tx, TOOLBAR_Y(28), 34, 28), "Escrever Nova Mensagem");
    tx -= 8 + 34;
    bool busy = ML.ist != I_READY && ML.ist != I_ERR && ML.ist != I_IDLE;
    if (ui_button_icon(u, UI_ID, rect_make(tx, TOOLBAR_Y(28), 34, 28), glyph_reload, !busy)) imap_start();
    /* lista */
    int lw = 320;
    rect_t lv = rect_make(sb, w->toolbar_h, lw, u->ch - w->toolbar_h);
    gfx_fill(s, lv.x, lv.y, lv.w, lv.h, T.content_bg);
    gfx_fill(s, lv.x + lv.w - 1, 0, 1, u->ch, T.separator);
    ui_title(u, sb + 16, 0, 36, 220, folder_label(ML.folders[ML.cur_fold]), ML.status);
    rect_t pc = surface_push_clip(s, lv);
    int ly = lv.y - ML.scroll, n = 0;
    char q[64];
    snprintf(q, sizeof q, "%s", ML.search);
    for (char *c = q; *c; c++) *c = (char)tolower(*c);
    for (int i = 0; i < ML.nm; i++) {
        msg_t *m = &ML.m[i];
        if (q[0]) { char h[260]; snprintf(h, sizeof h, "%s %s", m->from, m->subject); for (char *c = h; *c; c++) *c = (char)tolower(*c); if (!strstr(h, q)) continue; }
        rect_t r = rect_make(lv.x, ly, lv.w, 66);
        bool sel = ML.sel == i;
        if (sel) gfx_round_rect(s, r.x + 6, r.y + 2, r.w - 12, r.h - 4, 8, T.accent);
        u32 tc = sel ? RGB(255, 255, 255) : T.text, t2 = sel ? RGBA(255, 255, 255, 220) : T.text2;
        if (!m->seen) gfx_circle(s, r.x + 16, r.y + 20, 4.5f, sel ? RGB(255, 255, 255) : T.accent);
        int dw = text_width(font_get(FONT_REGULAR), 12, m->date);
        text_draw(s, font_get(FONT_REGULAR), 12, r.x + r.w - 14 - dw, r.y + 24, t2, m->date);
        text_draw_ellipsis(s, font_get(FONT_BOLD), 14, r.x + 28, r.y + 24, r.w - 50 - dw, tc, m->from[0] ? m->from : "(sem remetente)");
        text_draw_ellipsis(s, font_get(m->seen ? FONT_REGULAR : FONT_SEMIBOLD), 13, r.x + 28, r.y + 44, r.w - 44, tc, m->subject);
        if (m->size) {
            char kb[24];
            snprintf(kb, sizeof kb, "%u KB", MAX(1u, m->size / 1024));
            text_draw(s, font_get(FONT_REGULAR), 11, r.x + 28, r.y + 60, t2, kb);
        }
        if (!sel) gfx_fill(s, r.x + 28, r.y + r.h - 1, r.w - 40, 1, T.separator);
        if (ui_clicked(u, 126100 + i, r) && ML.sel != i) {
            ML.sel = i; ML.bscroll = 0;
            if (ML.body) { kfree(ML.body); ML.body = NULL; }
            ML.want_body = m->uid;
            if (ML.ist == I_ERR || ML.ist == I_IDLE) imap_start();
        }
        ly += 66;
        n++;
    }
    if (!n) ui_label_center(u, rect_make(lv.x, lv.y + 50, lv.w, 20), busy ? "Carregando…" : "Nenhuma Mensagem", FONT_SEMIBOLD, 15, T.text3);
    surface_pop_clip(s, pc);
    ui_scroll(u, UI_ID, lv, n * 66 + 10, &ML.scroll);
    /* leitura */
    rect_t a = rect_make(sb + lw, w->toolbar_h, u->cw - sb - lw, u->ch - w->toolbar_h);
    gfx_fill(s, a.x, a.y, a.w, a.h, T.content_bg);
    if (ML.sel < 0 || ML.sel >= ML.nm) { ui_label_center(u, rect_make(a.x, a.y + a.h / 2 - 12, a.w, 24), "Nenhuma Mensagem Selecionada", FONT_SEMIBOLD, 17, T.text3); if (busy) u->repaint = true; return; }
    msg_t *m = &ML.m[ML.sel];
    int hx = a.x + 24, hy = a.y + 18;
    kit_avatar(s, hx + 22, hy + 22, 22, m->from);
    text_draw_ellipsis(s, font_get(FONT_BOLD), 15, hx + 56, hy + 18, a.w - 200, T.text, m->from);
    text_draw_ellipsis(s, font_get(FONT_SEMIBOLD), 13, hx + 56, hy + 38, a.w - 120, T.text, m->subject);
    char to[140];
    snprintf(to, sizeof to, "Para: %s", ML.acc.email);
    text_draw_ellipsis(s, font_get(FONT_REGULAR), 12, hx + 56, hy + 56, a.w - 120, T.text2, to);
    int dw = text_width(font_get(FONT_REGULAR), 12, m->date);
    text_draw(s, font_get(FONT_REGULAR), 12, a.x + a.w - 24 - dw, hy + 18, T.text2, m->date);
    /* ações */
    int bx = a.x + a.w - 24;
    bx -= 80;
    if (ui_button_style(u, UI_ID, rect_make(bx, hy + 38, 80, 26), "Apagar", BTN_TINTED_RED, true, NULL)) {
        ML.store_uid = m->uid; ML.store_delete = true;
        memmove(&ML.m[ML.sel], &ML.m[ML.sel + 1], sizeof(msg_t) * (ML.nm - ML.sel - 1));
        ML.nm--; ML.sel = -1;
        cache_save();
        return;
    }
    bx -= 8 + 100;
    if (ui_button_style(u, UI_ID, rect_make(bx, hy + 38, 100, 26), "Encaminhar", BTN_GRAY, ML.body != NULL, NULL)) { char sj[180]; snprintf(sj, sizeof sj, "Enc: %s", m->subject); compose(NULL, sj, ML.body); }
    bx -= 8 + 90;
    if (ui_button_style(u, UI_ID, rect_make(bx, hy + 38, 90, 26), "Responder", BTN_GRAY, true, NULL)) {
        char sj[180];
        snprintf(sj, sizeof sj, strncasecmp(m->subject, "Re:", 3) ? "Re: %s" : "%s", m->subject);
        compose(m->from, sj, ML.body);
    }
    gfx_fill(s, a.x + 20, hy + 76, a.w - 40, 1, T.separator);
    rect_t bv = rect_make(a.x, hy + 84, a.w, a.y + a.h - hy - 84);
    rect_t pc2 = surface_push_clip(s, bv);
    int th = 0;
    if (ML.body) th = text_wrap(s, font_get(FONT_REGULAR), 14, rect_make(a.x + 28, bv.y + 10 - ML.bscroll, a.w - 56, 200000), 21, T.text, ML.body, true);
    else { ui_label(u, a.x + 28, bv.y + 12, ML.ist == I_ERR ? "Sem conexão com o servidor." : "Carregando a mensagem…", FONT_REGULAR, 14, T.text2); u->repaint = true; }
    surface_pop_clip(s, pc2);
    ui_scroll(u, UI_ID, bv, th + 30, &ML.bscroll);
    if (busy) u->repaint = true;
    if ((u->mods & MOD_CMD) && u->key == 'n') { compose(NULL, NULL, NULL); u->key_used = true; }
    if ((u->mods & MOD_CMD) && u->key == 'r' && ML.sel >= 0) { char sj[180]; snprintf(sj, sizeof sj, "Re: %s", m->subject); compose(m->from, sj, ML.body); u->key_used = true; }
}

static void tick(app_t *a) {
    imap_poll();
    smtp_poll();
    /* nova verificação a cada 5 minutos */
    static u64 last;
    if (ML.has_acc && ML.ist == I_READY && timer_ticks() - last > 5 * 60 * 1000) { last = timer_ticks(); imap_start(); }
}
static void on_close(window_t *w) { ML.w = NULL; }
static void launch(app_t *a) {
    if (ML.w) { wm_focus(ML.w); return; }
    acc_load();
    ML.w = wm_create(&app_mail, "Mail", -1, -1, 1100, 680, WIN_RESIZABLE);
    if (!ML.w) return;
    ML.w->paint = paint;
    ML.w->on_close = on_close;
    ML.w->toolbar_h = TOOLBAR_H;
    ML.w->sidebar_w = ML.has_acc ? 220 : 0;
    ML.w->min_w = 860;
    ML.w->min_h = 480;
    ML.w->title_x = -1;
    ML.ic.tcp = ML.sc.tcp = -1;
    if (ML.has_acc && (ML.ist == I_IDLE || ML.ist == I_ERR)) imap_start();
}

enum { M_NEW = 1, M_GET, M_ACC };
static menu_item_t m_msg[] = {{"Nova Mensagem", "⌘N", M_NEW}, {"Obter Novas Mensagens", "⇧⌘N", M_GET}, {NULL}, {"Configurar Conta…", NULL, M_ACC}};
static menu_t menus[] = {{"Caixa de Correio", m_msg, 4}};
static void on_menu(app_t *a, int act) {
    if (act == M_NEW) compose(NULL, NULL, NULL);
    else if (act == M_GET) imap_start();
    else if (act == M_ACC) ML.setup = true;
}
static void icon(surface_t *s, int x, int y, int sz) {
    float u = sz / 64.0f;
    kit_tile(s, x, y, sz, RGB(90, 180, 255), RGB(20, 110, 240));
    gfx_round_rect(s, x + 12 * u, y + 19 * u, 40 * u, 28 * u, 4 * u, RGB(255, 255, 255));
    gfx_line(s, x + 14 * u, y + 21 * u, x + 32 * u, y + 36 * u, 2.2f * u, RGB(30, 120, 240));
    gfx_line(s, x + 50 * u, y + 21 * u, x + 32 * u, y + 36 * u, 2.2f * u, RGB(30, 120, 240));
}
app_t app_mail = {.name = "Mail", .exec = "mail", .draw_icon = icon, .launch = launch, .tick = tick, .on_menu = on_menu, .menus = menus, .nmenus = 1, .pinned = true};

/* Launch Services: mailto:fulano@x.com?subject=Oi&body=... abre uma mensagem nova (Mail é o leitor padrão) */
static void pct_decode(const char *s, usize n, char *out, usize cap) {
    usize o = 0;
    for (usize i = 0; i < n && o + 1 < cap; i++) {
        if (s[i] == '%' && i + 2 < n) {
            int hi = s[i + 1] <= '9' ? s[i + 1] - '0' : (s[i + 1] | 32) - 'a' + 10, lo = s[i + 2] <= '9' ? s[i + 2] - '0' : (s[i + 2] | 32) - 'a' + 10;
            out[o++] = (char)(hi * 16 + lo);
            i += 2;
        } else out[o++] = s[i] == '+' ? ' ' : s[i];
    }
    out[o] = 0;
}
void mail_open_url(const char *url) {
    char to[256] = "", subj[256] = "", body[1024] = "";
    const char *p = url && !strncasecmp(url, "mailto:", 7) ? url + 7 : (url ? url : "");
    const char *q = strchr(p, '?');
    pct_decode(p, q ? (usize)(q - p) : strlen(p), to, sizeof to);
    while (q && *q) {
        q++;
        const char *amp = strchr(q, '&'), *eq = strchr(q, '=');
        usize len = amp ? (usize)(amp - q) : strlen(q);
        if (eq && eq < q + len) {
            usize kl = (usize)(eq - q);
            if (kl == 7 && !strncasecmp(q, "subject", 7)) pct_decode(eq + 1, len - kl - 1, subj, sizeof subj);
            else if (kl == 4 && !strncasecmp(q, "body", 4)) pct_decode(eq + 1, len - kl - 1, body, sizeof body);
        }
        q = amp;
    }
    app_launch(&app_mail);
    compose(to, subj, NULL);
    if (body[0] && ML.ta) textarea_set(ML.ta, body, -1);
}
