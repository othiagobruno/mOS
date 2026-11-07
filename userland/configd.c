/* mOS — configd: o SCDynamicStore do SystemConfiguration, com a rede e os proxies reais.
 *
 * Serviço Mach CONFIGD_SERVICE (launchd: /Sistema/Biblioteca/LaunchDaemons/com.apple.configd.plist, KeepAlive).
 * Guarda chave -> valor (plist XML, que o libSystem converte de/para CFPropertyList: sysconfig.c). A cada 3 s lê a
 * rede do kernel (MOS_NET_INFO: IP, máscara, roteador, DNS) e publica as chaves do macOS:
 *     State:/Network/Global/IPv4, State:/Network/Global/DNS, State:/Network/Global/Proxies,
 *     State:/Network/Interface, State:/Network/Interface/en0/IPv4, State:/Network/Interface/en0/Link,
 *     State:/Network/Service/<id>/IPv4, State:/Network/Service/<id>/DNS, Setup:/System, Setup:/Network/HostNames
 * Os proxies vêm do dicionário "Proxies" de CONFIG_PROXIES_PLIST (o preferences.plist do macOS; o primeiro que
 * houver), senão o padrão do macOS sem proxy. Quando algo muda, avisa quem pediu (CF_WATCH: chaves exatas ou
 * padrões regex) e posta NET_CHANGE_NOTIFICATION no notifyd, como o configd da Apple. */
#include "mos.h"
#include "svcnative.h"
#include "plistmini.h"

#define MAX_KEYS 256
#define MAX_VAL  4096
#define MAX_WATCH 64
#define SERVICE_ID "8C0F6F2E-6D4F-4E53-9A6F-6D4F53454E30"

typedef struct { int used; char key[160]; char val[MAX_VAL]; unsigned n; } kv_t;
typedef struct { int used, pid; unsigned port, id; char keys[1024], pats[1024]; } watch_t;
static kv_t kv[MAX_KEYS];
static watch_t watches[MAX_WATCH];
static char changed[4096];
static usize nchanged;

/* ---------------------------------------------------------------- regex mínima (ancorada): . * + ? [..] [^..] */
static int cls_match(const char **pp, char c) {
    const char *p = *pp + 1;
    int neg = *p == '^', ok = 0;
    if (neg) p++;
    while (*p && *p != ']') {
        if (p[1] == '-' && p[2] && p[2] != ']') { if (c >= p[0] && c <= p[2]) ok = 1; p += 3; }
        else { if (*p == '\\' && p[1]) p++; if (c == *p) ok = 1; p++; }
    }
    *pp = *p ? p + 1 : p;
    return neg ? !ok : ok;
}
static const char *atom_end(const char *p) {
    if (*p == '[') { p++; if (*p == '^') p++; if (*p == ']') p++; while (*p && *p != ']') p++; return *p ? p + 1 : p; }
    if (*p == '\\' && p[1]) return p + 2;
    return p + 1;
}
static int atom_match(const char *p, char c) {
    if (!c) return 0;
    if (*p == '.') return 1;
    if (*p == '[') { const char *q = p; return cls_match(&q, c); }
    if (*p == '\\') return p[1] == c;
    return *p == c;
}
static int re_here(const char *p, const char *s) {
    if (!*p) return !*s;
    if (*p == '$' && !p[1]) return !*s;
    const char *e = atom_end(p);
    if (*e == '*' || *e == '+' || *e == '?') {
        int min = *e == '+', max = *e == '?' ? 1 : 1 << 30, n = 0;
        while (n < max && atom_match(p, s[n])) n++;
        for (; n >= min; n--) if (re_here(e + 1, s + n)) return 1;
        return 0;
    }
    return atom_match(p, *s) && re_here(e, s + 1);
}
static int re_match(const char *pat, const char *s) { if (*pat == '^') pat++; return re_here(pat, s); }

/* ---------------------------------------------------------------- store */
static kv_t *kv_find(const char *k) { for (int i = 0; i < MAX_KEYS; i++) if (kv[i].used && !strcmp(kv[i].key, k)) return &kv[i]; return 0; }
static void note_change(const char *k) {
    usize n = strlen(k);
    if (nchanged + n + 2 < sizeof changed) { memcpy(changed + nchanged, k, n); nchanged += n; changed[nchanged++] = '\n'; changed[nchanged] = 0; }
}
static int kv_set(const char *k, const char *v, unsigned n) {
    kv_t *e = kv_find(k);
    if (n >= MAX_VAL) return 0;
    if (e && e->n == n && !memcmp(e->val, v, n)) return 1;           /* igual: nada muda */
    if (!e) for (int i = 0; i < MAX_KEYS && !e; i++) if (!kv[i].used) e = &kv[i];
    if (!e) return 0;
    usize kl = strlen(k);
    if (kl >= sizeof e->key) kl = sizeof e->key - 1;
    memcpy(e->key, k, kl); e->key[kl] = 0;
    memcpy(e->val, v, n); e->val[n] = 0; e->n = n;
    e->used = 1;
    note_change(k);
    return 1;
}
static void kv_del(const char *k) { kv_t *e = kv_find(k); if (e) { e->used = 0; note_change(k); } }

/* avisa os interessados nas chaves de changed[] */
static int line_has(const char *list, const char *k) {
    usize kl = strlen(k);
    for (const char *l = list; *l;) {
        const char *e = l;
        while (*e && *e != '\n') e++;
        if ((usize)(e - l) == kl && !strncmp(l, k, kl)) return 1;
        l = *e ? e + 1 : e;
    }
    return 0;
}
static int pat_has(const char *pats, const char *k) {
    char p[256];
    for (const char *l = pats; *l;) {
        const char *e = l;
        while (*e && *e != '\n') e++;
        usize n = (usize)(e - l) < sizeof p - 1 ? (usize)(e - l) : sizeof p - 1;
        memcpy(p, l, n); p[n] = 0;
        if (n && re_match(p, k)) return 1;
        l = *e ? e + 1 : e;
    }
    return 0;
}
static void flush_changes(void) {
    if (!nchanged) return;
    for (int w = 0; w < MAX_WATCH; w++) {
        watch_t *x = &watches[w];
        if (!x->used) continue;
        char out[2048];
        usize on = 0;
        for (const char *l = changed; *l;) {
            const char *e = l;
            while (*e && *e != '\n') e++;
            char k[160];
            usize n = (usize)(e - l) < sizeof k - 1 ? (usize)(e - l) : sizeof k - 1;
            memcpy(k, l, n); k[n] = 0;
            if ((line_has(x->keys, k) || pat_has(x->pats, k)) && on + n + 2 < sizeof out) { memcpy(out + on, k, n); on += n; out[on++] = '\n'; }
            l = *e ? e + 1 : e;
        }
        if (!on) continue;
        out[on] = 0;
        i64 kr = svc_note(x->port, SVC_NOTE_ID, x->id, out, (unsigned)on + 1);
        if (kr == SEND_INVALID_DEST) { port_drop(x->port); x->used = 0; }
    }
    nchanged = 0;
    changed[0] = 0;
}

/* ---------------------------------------------------------------- XML */
static char xb[MAX_VAL];
static usize xn;
static void xs(const char *s) { while (*s && xn + 1 < sizeof xb) xb[xn++] = *s++; xb[xn] = 0; }
static void xesc(const char *s) {
    for (; *s; s++) {
        if (*s == '&') xs("&amp;"); else if (*s == '<') xs("&lt;"); else if (*s == '>') xs("&gt;");
        else { char c[2] = { *s, 0 }; xs(c); }
    }
}
static void xbegin(void) { xn = 0; xs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<plist version=\"1.0\">\n<dict>\n"); }
static void xend(void) { xs("</dict>\n</plist>\n"); }
static void xkey(const char *k) { xs("\t<key>"); xesc(k); xs("</key>"); }
static void xstr(const char *k, const char *v) { xkey(k); xs("<string>"); xesc(v); xs("</string>\n"); }
static void xint(const char *k, i64 v) { char b[24]; int n = 0; u64 u = v < 0 ? (u64)-v : (u64)v; char t[24]; do b[n++] = (char)('0' + u % 10); while (u /= 10); int m = 0; if (v < 0) t[m++] = '-'; while (n) t[m++] = b[--n]; t[m] = 0; xkey(k); xs("<integer>"); xs(t); xs("</integer>\n"); }
static void xarr1(const char *k, const char *v) { xkey(k); xs("<array><string>"); xesc(v); xs("</string></array>\n"); }
static void xnode(pnode_t *n) {
    switch (n->type) {
    case PN_DICT: xs("<dict>"); for (pnode_t *c = n->child; c; c = c->next) { xs("<key>"); xesc(c->key ? c->key : ""); xs("</key>"); xnode(c); } xs("</dict>"); break;
    case PN_ARRAY: xs("<array>"); for (pnode_t *c = n->child; c; c = c->next) xnode(c); xs("</array>"); break;
    case PN_STRING: xs("<string>"); xesc(n->str ? n->str : ""); xs("</string>"); break;
    case PN_INT: xs("<integer>"); xs(n->str ? n->str : "0"); xs("</integer>"); break;
    case PN_REAL: xs("<real>"); xs(n->str ? n->str : "0"); xs("</real>"); break;
    case PN_TRUE: xs("<true/>"); break;
    case PN_FALSE: xs("<false/>"); break;
    case PN_DATA: xs("<data>"); xs(n->str ? n->str : ""); xs("</data>"); break;
    case PN_DATE: xs("<date>"); xs(n->str ? n->str : ""); xs("</date>"); break;
    }
}
static void ipstr(unsigned v, char *o) {
    int n = 0;
    for (int i = 3; i >= 0; i--) {
        unsigned b = (v >> (i * 8)) & 0xff;
        if (b >= 100) o[n++] = (char)('0' + b / 100);
        if (b >= 10) o[n++] = (char)('0' + b / 10 % 10);
        o[n++] = (char)('0' + b % 10);
        if (i) o[n++] = '.';
    }
    o[n] = 0;
}
static void put_xml(const char *key) { kv_set(key, xb, (unsigned)xn); }

static pnode_t *find_dict(pnode_t *n, const char *key, int depth) {
    for (; n && depth < 12; n = n->next) {
        if (n->type == PN_DICT && n->key && !strcmp(n->key, key)) return n;
        pnode_t *r = find_dict(n->child, key, depth + 1);
        if (r) return r;
    }
    return 0;
}

static char filebuf[65536];
static char computer[64] = "mOS", localhost[64] = "mos";

static void refresh(void) {
    unsigned ni[4] = { 0, 0, 0, 0 };
    int up = sys3(SYS_NET, MOS_NET_INFO, (u64)ni, 0) == 1 && ni[0];
    char ip[16], mask[16], gw[16], dns[16], bc[16];
    ipstr(ni[0], ip); ipstr(ni[1], mask); ipstr(ni[2], gw); ipstr(ni[3], dns); ipstr((ni[0] & ni[1]) | ~ni[1], bc);

    xbegin(); xkey("Interfaces"); xs("<array><string>lo0</string><string>en0</string></array>\n"); xend(); put_xml("State:/Network/Interface");
    xbegin(); xkey("Active"); xs(up ? "<true/>\n" : "<false/>\n"); xend(); put_xml("State:/Network/Interface/en0/Link");
    if (up) {
        xbegin(); xarr1("Addresses", ip); xarr1("SubnetMasks", mask); xarr1("BroadcastAddresses", bc); xend();
        put_xml("State:/Network/Interface/en0/IPv4");
        xbegin(); xarr1("Addresses", ip); xarr1("SubnetMasks", mask); xstr("Router", gw); xstr("InterfaceName", "en0");
        xstr("ConfirmedInterfaceName", "en0"); xend();
        put_xml("State:/Network/Service/" SERVICE_ID "/IPv4");
        xbegin(); xstr("PrimaryInterface", "en0"); xstr("PrimaryService", SERVICE_ID); xstr("Router", gw); xend();
        put_xml("State:/Network/Global/IPv4");
        if (ni[3]) {
            xbegin(); xarr1("ServerAddresses", dns); xend();
            put_xml("State:/Network/Global/DNS");
            put_xml("State:/Network/Service/" SERVICE_ID "/DNS");
        }
    } else {
        kv_del("State:/Network/Interface/en0/IPv4");
        kv_del("State:/Network/Service/" SERVICE_ID "/IPv4");
        kv_del("State:/Network/Global/IPv4");
        kv_del("State:/Network/Global/DNS");
        kv_del("State:/Network/Service/" SERVICE_ID "/DNS");
    }
    /* proxies: o dicionário Proxies das preferências, senão o padrão do macOS (sem proxy) */
    int have = 0;
    i64 len = read_file(CONFIG_PROXIES_PLIST, filebuf, sizeof filebuf - 1);
    if (len > 0 && memcmp(filebuf, "bplist", 6)) {
        filebuf[len] = 0;
        pnode_t *root = plist_parse(filebuf, (usize)len);
        pnode_t *px = root ? find_dict(root->child, "Proxies", 0) : 0;
        if (px) {
            xn = 0;
            xs("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<plist version=\"1.0\">\n");
            xnode(px);
            xs("\n</plist>\n");
            have = 1;
        }
        const char *cn = 0;
        pnode_t *sys = root ? find_dict(root->child, "System", 0) : 0;
        if (sys) { pnode_t *inner = plist_get(sys, "System"); cn = plist_str(inner ? inner : sys, "ComputerName"); }
        if (cn && cn[0]) { usize k = 0; while (cn[k] && k + 1 < sizeof computer) { computer[k] = cn[k]; k++; } computer[k] = 0; }
        plist_free(root);
    }
    if (!have) {
        xbegin(); xkey("ExceptionsList"); xs("<array><string>*.local</string><string>169.254/16</string></array>\n");
        xint("FTPPassive", 1); xint("HTTPEnable", 0); xint("HTTPSEnable", 0); xint("ProxyAutoConfigEnable", 0); xint("SOCKSEnable", 0); xend();
    }
    put_xml("State:/Network/Global/Proxies");
    xbegin(); xstr("ComputerName", computer); xint("ComputerNameEncoding", 0x08000100); xend(); put_xml("Setup:/System");
    xbegin(); xstr("LocalHostName", localhost); xend(); put_xml("Setup:/Network/HostNames");
}

static void post_net_change(void) {
    static unsigned char b[512];
    svc_body_t q = { NT_POST, 0, 0, 0, 0, 0, 0 };
    svc_call(NOTIFYD_SERVICE, &q, NET_CHANGE_NOTIFICATION, sizeof NET_CHANGE_NOTIFICATION, 0, 0, b, sizeof b, 2000);
}

void _start(const char *arg) {
    (void)arg;
    unsigned port = 0;
    if (bs_op(MACH_BS_CHECK_IN, CONFIGD_SERVICE, &port) != 0 || !port) { print("configd: check_in falhou\n"); exit(1); }
    refresh();
    nchanged = 0; changed[0] = 0;                      /* a primeira leitura não é "mudança" */
    print("configd: pronto\n");
    static unsigned char msg[SVC_MAX_MSG + 128];
    static char reply[SVC_MAX_MSG - 256];
    u64 last = time_ms();
    for (;;) {
        memset(msg, 0, sizeof(mhdr_t));
        i64 r = mach_msg_n(msg, M_RCV | M_RCV_TO | M_TRAILER, 0, sizeof msg, port, 1000);
        if (time_ms() - last >= 3000) {
            last = time_ms();
            refresh();
            int net = 0;
            for (const char *c = changed; *c; c++) if (!strncmp(c, "State:/Network/", 15)) { net = 1; break; }
            flush_changes();
            if (net) post_net_change();
        }
        if (r) { if (r != RCV_TIMED_OUT && r != 0x10004004) sleep_ms(10); continue; }
        mhdr_t *h = (mhdr_t *)msg;
        unsigned rport = 0;
        svc_body_t *b = svc_body(msg, sizeof msg, &rport);
        unsigned rp = (h->bits >> 8 & 0x1f) == D_MOVE_SO ? h->local : 0;
        if (!b || h->id != SVC_MSG_ID) { if (rport) port_drop(rport); if (rp) svc_reply(rp, 22, 0, 0, 0, 0); continue; }
        char k[160];
        usize kl = b->n1 < sizeof k - 1 ? b->n1 : sizeof k - 1;
        memcpy(k, svc_s1(b), kl);
        k[kl] = 0;
        unsigned code = 0, rn = 0;
        switch (b->op) {
        case CF_GET: { kv_t *e = kv_find(k); if (e) { memcpy(reply, e->val, e->n); rn = e->n; } else code = 1003; break; }  /* kSCStatusNoKey */
        case CF_SET: if (!kv_set(k, svc_s2(b), b->n2 && !svc_s2(b)[b->n2 - 1] ? b->n2 - 1 : b->n2)) code = 1001; break;
        case CF_REMOVE: if (kv_find(k)) kv_del(k); else code = 1003; break;
        case CF_NOTIFY: note_change(k); break;
        case CF_LIST:
            for (int i = 0; i < MAX_KEYS; i++) {
                if (!kv[i].used || (k[0] && !re_match(k, kv[i].key))) continue;
                usize n = strlen(kv[i].key);
                if (rn + n + 2 >= sizeof reply) break;
                memcpy(reply + rn, kv[i].key, n); rn += (unsigned)n; reply[rn++] = '\n';
            }
            break;
        case CF_WATCH: {
            watch_t *w = 0;
            int pid = msg_sender_pid(msg);
            for (int i = 0; i < MAX_WATCH; i++) if (watches[i].used && watches[i].pid == pid && watches[i].id == b->a) w = &watches[i];
            if (!w) for (int i = 0; i < MAX_WATCH && !w; i++) if (!watches[i].used) w = &watches[i];
            if (!w || !rport) { code = 1001; break; }
            if (w->used && w->port) port_drop(w->port);
            w->used = 1; w->pid = pid; w->id = b->a; w->port = rport; rport = 0;
            usize n = b->n1 < sizeof w->keys - 1 ? b->n1 : sizeof w->keys - 1;
            memcpy(w->keys, svc_s1(b), n); w->keys[n] = 0;
            n = b->n2 < sizeof w->pats - 1 ? b->n2 : sizeof w->pats - 1;
            memcpy(w->pats, svc_s2(b), n); w->pats[n] = 0;
            break;
        }
        case CF_UNWATCH: {
            int pid = msg_sender_pid(msg);
            for (int i = 0; i < MAX_WATCH; i++)
                if (watches[i].used && watches[i].pid == pid && watches[i].id == b->a) { port_drop(watches[i].port); watches[i].used = 0; }
            break;
        }
        default: code = 1001;
        }
        if (rport) port_drop(rport);
        if (rp) svc_reply(rp, code, 0, 0, reply, rn);
        flush_changes();
    }
}
