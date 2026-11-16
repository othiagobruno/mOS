/* mOS — aviso de app baixado da Internet (o que o Gatekeeper do macOS mostra na primeira abertura).
 *
 * Um .app (ou executável) com o xattr com.apple.quarantine sem a marca "aprovado pelo usuário" (0x0040) abre um
 * alerta: "“X” é um app baixado da Internet. Tem certeza de que deseja abri-lo?", com quem baixou e quando
 * (o agente e a hora gravados pelo navegador ou pelo instalador; formato do valor em
 * userland/libsystem/quarantine.c: "<flags hex>;<hora Unix hex>;<agente>;<UUID>"). Abrir grava a marca no
 * xattr (as próximas aberturas são diretas, como no macOS) e segue com a abertura; Cancelar não abre.
 * O mOS não confere assinatura nem notarização: o aviso é o da origem do arquivo.
 * Chamado por macos_run_args (kernel/apps/registry.c). gatekeeper_parse é testável no host (tools/gatekeepertest.c). */
#ifndef GK_HOST
#include "compat.h"
#include "ui.h"
#include "vfs.h"
#endif

#define GK_ATTR "com.apple.quarantine"
#define GK_APPROVED 0x0040u

typedef struct { unsigned flags; unsigned long long ts; char agent[64]; int ok; } gk_qtn_t;

static int gk_hex(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
/* "0083;65f1a2b3;Safari;UUID" -> campos; ok = 0 se não for o formato */
static gk_qtn_t gatekeeper_parse(const char *v, int n) {
    gk_qtn_t q;
    int i = 0, f = 0;
    unsigned long long acc = 0;
    q.flags = 0; q.ts = 0; q.agent[0] = 0; q.ok = 0;
    for (; i < n && v[i] && v[i] != ';'; i++) { int h = gk_hex(v[i]); if (h < 0) return q; acc = acc * 16 + (unsigned)h; f++; }
    if (!f || f > 8) return q;
    q.flags = (unsigned)acc;
    q.ok = 1;
    if (i >= n || v[i] != ';') return q;
    i++;
    acc = 0;
    for (; i < n && v[i] && v[i] != ';'; i++) { int h = gk_hex(v[i]); if (h < 0) break; acc = acc * 16 + (unsigned)h; }
    q.ts = acc;
    if (i < n && v[i] == ';') {
        i++;
        int k = 0;
        for (; i < n && v[i] && v[i] != ';' && k < (int)sizeof q.agent - 1; i++) q.agent[k++] = v[i];
        q.agent[k] = 0;
    }
    return q;
}
/* o mesmo valor com a marca de aprovado (os outros campos ficam) */
static int gatekeeper_approve(const char *v, int n, char *out, int cap) {
    gk_qtn_t q = gatekeeper_parse(v, n);
    if (!q.ok || cap < 6) return -1;
    int i = 0;
    while (i < n && v[i] && v[i] != ';') i++;
    unsigned fl = q.flags | GK_APPROVED;
    static const char hx[] = "0123456789abcdef";
    out[0] = hx[(fl >> 12) & 15]; out[1] = hx[(fl >> 8) & 15]; out[2] = hx[(fl >> 4) & 15]; out[3] = hx[fl & 15];
    int o = 4;
    for (; i < n && o < cap - 1; i++) out[o++] = v[i];
    out[o] = 0;
    return o;
}
static void gk_civil(unsigned long long z, int *Y, int *M, int *D) {
    long long days = (long long)(z / 86400) + 719468, era = days / 146097, doe = days - era * 146097;
    long long yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365, doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    long long mp = (5 * doy + 2) / 153;
    *D = (int)(doy - (153 * mp + 2) / 5 + 1);
    *M = (int)(mp < 10 ? mp + 3 : mp - 9);
    *Y = (int)(yoe + era * 400 + (*M <= 2));
}

#ifndef GK_HOST
static struct { char path[320], name[128], args[1024]; void (*run)(const char *, const char *, const char *); int busy; } gk;

static void gk_answer(int button, void *ctx) {
    (void)ctx;
    gk.busy = 0;
    if (button != 0) { kprintf("gatekeeper: %s não aberto (o usuário cancelou)\n", gk.name); return; }
    vnode_t *n = vfs_lookup(gk.path);
    char v[300], nv[320];
    i64 len = n ? vfs_xattr_get(n, GK_ATTR, v, sizeof v - 1, 0) : -1;
    if (len > 0 && gatekeeper_approve(v, (int)len, nv, (int)sizeof nv) > 0) {
        vfs_xattr_set(n, GK_ATTR, nv, strlen(nv), 0, 0);
        vfs_meta_changed(n);
    }
    kprintf("gatekeeper: %s aberto com a aprovação do usuário\n", gk.name);
    gk.run(gk.path, gk.name, gk.args);
}

/* true: pode abrir agora. false: o aviso está na tela (run é chamado se o usuário escolher Abrir) */
bool gatekeeper_allow(const char *path, const char *name, const char *args, void (*run)(const char *, const char *, const char *)) {
    vnode_t *n = vfs_lookup(path);
    char v[300];
    i64 len = n ? vfs_xattr_get(n, GK_ATTR, v, sizeof v - 1, 0) : -1;
    if (len <= 0) return true;
    v[len] = 0;
    gk_qtn_t q = gatekeeper_parse(v, (int)len);
    if (!q.ok || (q.flags & GK_APPROVED)) return true;
    if (gk.busy) return false;                               /* um aviso por vez; o segundo clique espera a resposta */
    gk.busy = 1;
    snprintf(gk.path, sizeof gk.path, "%s", path);
    snprintf(gk.name, sizeof gk.name, "%s", name);
    snprintf(gk.args, sizeof gk.args, "%s", args ? args : "");
    gk.run = run;
    char shown[128];
    snprintf(shown, sizeof shown, "%s", name);
    usize sl = strlen(shown);
    if (sl > 4 && !strcmp(shown + sl - 4, ".app")) shown[sl - 4] = 0;
    char title[200], msg[300];
    snprintf(title, sizeof title, "“%s” é um app baixado da Internet. Tem certeza de que deseja abri-lo?", shown);
    if (q.ts) {
        int Y, M, D;
        gk_civil(q.ts, &Y, &M, &D);
        snprintf(msg, sizeof msg, "%s baixou este app em %02d/%02d/%04d. Abra só se confiar no desenvolvedor.",
                 q.agent[0] ? q.agent : "Um app", D, M, Y);
    } else snprintf(msg, sizeof msg, "%s baixou este app. Abra só se confiar no desenvolvedor.", q.agent[0] ? q.agent : "Um app");
    static const char *btn[] = {"Abrir", "Cancelar"};
    alert_show(title, msg, btn, 2, gk_answer, 0);
    kprintf("gatekeeper: aviso de app baixado para %s\n", path);
    return false;
}
#endif
