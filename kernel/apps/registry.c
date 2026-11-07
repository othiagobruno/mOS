/* mOS — registro de aplicativos, abertura de arquivos por tipo e utilidades comuns */
#include "apps.h"
#include "compat.h"
#include "launchsvc.h"

static app_t *all_apps[] = {
    &app_arquivos, &app_horizonte, &app_terminal, &app_notas, &app_editor, &app_calculadora, &app_calendario,
    &app_musica, &app_minas, &app_visualizador, &app_atividade, &app_console, &app_preferencias, &app_sobre,
    &app_componentes, &app_instalador,
    &app_lembretes, &app_contatos,
    &app_imagecapture,
    &app_automator,
    &app_quicktime,
    &app_livros,
    &app_mail,
    &app_atalhos,
    &app_jogos,
    &app_paciencia,
    &app_audiomidi,
    &app_chaves,
    &app_dicionario,
    &app_senhas,
    &app_script,
    &app_bolsa,
    &app_mapas,
    &app_tempo,
    &app_freeform,
    &app_dicas,
    &app_diario,
    &app_fotos,
    &app_gravador,
    &app_grapher,
    &app_fontes,
    &app_medidor,
    &app_captura,
    &app_disco,
    &app_sysinfo,
    &app_xadrez,
    &app_stickies,
    &app_relogio,
};

/* apps do macOS (.app em /Aplicativos) registrados dinamicamente: aparecem no Launchpad e na Busca */
#define MAX_MAC_APPS 96
static app_t *mac_apps[MAX_MAC_APPS];
static int nmac;
static void macos_run(const char *path, const char *name, const char *args);
static int last_spawn_pid = -1;      /* pid do último macos_run bem-sucedido */
static int mac_pid[MAX_MAC_APPS];

static void mac_icon(surface_t *s, int x, int y, int sz) {
    float r = sz * 0.225f, m = sz * 0.08f;
    gfx_round_rect_grad(s, x + m, y + m, sz - 2 * m, sz - 2 * m, r, RGB(120, 128, 150), RGB(62, 66, 84));
    text_draw_center(s, font_get(FONT_SEMIBOLD), sz * 0.42f, rect_make(x, y, sz, sz), RGB(255, 255, 255), "⌘");
}

static void mac_launch(app_t *a) {
    int i = 0;
    while (i < nmac && mac_apps[i] != a) i++;
    app_t *proxy = i < nmac && mac_pid[i] > 0 ? compat_gui_app_for_pid(mac_pid[i]) : NULL;
    if (proxy) { wm_bring_app(proxy); return; }   /* já aberto: traz as janelas do processo */
    a->running = false;                /* as janelas pertencem ao processo (proxy do guisys), não a este atalho */
    last_spawn_pid = -1;
    macos_run(a->exec + 4, a->name, "");   /* exec = "mac:<caminho>" */
    if (i < nmac) mac_pid[i] = last_spawn_pid;
}

/* apps do macOS abertos pela interface (Launchpad, Arquivos, Busca): se o processo cair, avisa
 * como o macOS ("fechou inesperadamente") em vez de sumir em silêncio */
static struct { int pid; char name[64]; } watch[8];

static void watch_add(int pid, const char *name) {
    int k = 0;
    for (int i = 0; i < (int)ARRAY_LEN(watch); i++) if (!watch[i].pid) { k = i; break; }
    watch[k].pid = pid;
    snprintf(watch[k].name, sizeof watch[k].name, "%s", name);
    usize l = strlen(watch[k].name);
    if (l > 4 && !strcasecmp(watch[k].name + l - 4, ".app")) watch[k].name[l - 4] = 0;
}

static void watch_tick(void) {
    for (int i = 0; i < (int)ARRAY_LEN(watch); i++) {
        if (!watch[i].pid || proc_alive(watch[i].pid)) continue;
        int code = proc_exit_code(watch[i].pid);
        watch[i].pid = 0;
        /* 0: saiu normalmente; -9: encerrado pelo usuário (Forçar Encerrar, ⌘Q); -1000: sumiu da tabela */
        if (code == 0 || code == -9 || code == -1000) continue;
        const char *why = code == 134 ? " (abort)" : code == 139 || code == -11 ? " (falha de segmentação)" : "";
        char title[120], msg[200];
        snprintf(title, sizeof title, "\"%s\" fechou inesperadamente.", watch[i].name);
        snprintf(msg, sizeof msg, "O app encerrou com o código %d%s. Os detalhes estão no Console.", code, why);
        static const char *btn[] = {"OK"};
        alert_show(title, msg, btn, 1, NULL, NULL);
    }
}

/* a cada quadro: o atalho fica "rodando" (bolinha no Dock) enquanto o processo tiver janelas */
void apps_refresh_mac(void) {
    watch_tick();
    ls_tick();
    extern window_t *wm_nth(int);
    for (int i = 0; i < nmac; i++) {
        if (!(mac_pid[i] > 0 && compat_gui_app_for_pid(mac_pid[i]))) {
            /* aberto por outro caminho (Arquivos, Busca, Terminal): adota o processo pelo .app */
            for (int k = 0;; k++) {
                window_t *w = wm_nth(k);
                if (!w) break;
                const char *b = w->closing || w->app == mac_apps[i] ? NULL : compat_app_bundle(w->app);
                if (b && !strcasecmp(b, mac_apps[i]->exec + 4)) { mac_pid[i] = compat_gui_pid_of(w->app); break; }
            }
        }
        bool on =mac_pid[i] > 0 && compat_gui_app_for_pid(mac_pid[i]) != NULL;
        if (mac_apps[i]->running != on) { mac_apps[i]->running = on; wm_damage(desktop_dock_rect()); }
    }
}

static void scan_mac_apps(void) {
    /* .app apagado: o atalho sai do Launchpad (a memória fica: o Dock pode guardar o ponteiro) */
    int k = 0;
    for (int i = 0; i < nmac; i++) {
        if (vfs_lookup(mac_apps[i]->exec + 4)) { mac_pid[k] = mac_pid[i]; mac_apps[k++] = mac_apps[i]; }
        else kprintf("apps: \"%s\" removido\n", mac_apps[i]->name);
    }
    nmac = k;
    vnode_t *d = vfs_lookup("/Aplicativos");
    for (vnode_t *c = d && d->dir ? d->child : NULL; c && nmac < MAX_MAC_APPS; c = c->next) {
        usize l = strlen(c->name);
        if (!c->dir || l <= 4 || strcasecmp(c->name + l - 4, ".app") || !vfs_child(c, "Contents")) continue;
        char path[300];
        snprintf(path, sizeof path, "/Aplicativos/%s", c->name);
        bool known = false;
        for (int i = 0; i < nmac; i++) if (!strcmp(mac_apps[i]->exec + 4, path)) known = true;
        if (known) continue;
        mac_pid[nmac] = 0;
        app_t *a = kcalloc(1, sizeof(app_t));
        char *nm = kmalloc(128), *ex = kmalloc(310);
        if (!a || !nm || !ex) break;
        bundle_info_t bi;
        memset(&bi, 0, sizeof bi);
        macho_bundle_info(path, &bi);
        if (bi.name[0]) snprintf(nm, 128, "%s", bi.name);
        else { snprintf(nm, 128, "%s", c->name); nm[l - 4] = 0; }
        snprintf(ex, 310, "mac:%s", path);
        a->name = nm; a->exec = ex; a->draw_icon = mac_icon; a->launch = mac_launch;
        mac_apps[nmac++] = a;
        kprintf("apps: app do macOS \"%s\" (%s)\n", nm, path);
    }
}

void apps_register_all(void) {
    for (u32 i = 0; i < ARRAY_LEN(all_apps); i++) {
        app_t *a = all_apps[i];
        /* pré-renderiza os ícones do Dock */
        icon_for(a, g_dock_size);
    }
    app_arquivos.running = true;
    scan_mac_apps();
    ls_init();                         /* Launch Services: tipos, quem abre o quê (chama apps_sync_mac) */
}

/* launchsvc.c: /Aplicativos mudou (app instalado ou apagado) */
void apps_sync_mac(void) { scan_mac_apps(); wm_damage_all(); }

/* launchsvc.c: inicia um .app com argumentos (URL para o navegador); pid ou -1 */
int apps_run_mac(const char *bundle, const char *args) {
    const char *b = strrchr(bundle, '/');
    last_spawn_pid = -1;
    macos_run(bundle, b ? b + 1 : bundle, args ? args : "");
    for (int i = 0; i < nmac; i++) if (!strcmp(mac_apps[i]->exec + 4, bundle) && last_spawn_pid > 0) mac_pid[i] = last_spawn_pid;
    return last_spawn_pid;
}

/* atalho de /Aplicativos que representa um app proxy (janelas de um processo do macOS); NULL se nenhum */
app_t *apps_shortcut_for(app_t *proxy) {
    int pid = compat_gui_pid_of(proxy);
    if (pid <= 0) return NULL;
    for (int i = 0; i < nmac; i++) if (mac_pid[i] == pid) return mac_apps[i];
    return NULL;
}

int apps_count(void) { return (int)ARRAY_LEN(all_apps) + nmac; }
app_t *apps_get(int i) {
    if (i >= 0 && i < (int)ARRAY_LEN(all_apps)) return all_apps[i];
    i -= (int)ARRAY_LEN(all_apps);
    return i >= 0 && i < nmac ? mac_apps[i] : NULL;
}
app_t *apps_find(const char *exec) {
    for (int i = 0; i < apps_count(); i++) if (!strcmp(apps_get(i)->exec, exec)) return apps_get(i);
    return NULL;
}

void app_launch(app_t *a) {
    if (!a) return;
    if (a->running && wm_count_for(a) && strcmp(a->exec, "arquivos") && strcmp(a->exec, "horizonte") && strcmp(a->exec, "terminal") && strcmp(a->exec, "editor")) {
        wm_bring_app(a);
        return;
    }
    a->running = true;
    if (a->launch) a->launch(a);
    sound_click();
}

void app_quit(app_t *a) {
    if (!a) return;
    if (a->on_quit) a->on_quit(a);
    wm_close_app(a);
    if (strcmp(a->exec, "arquivos")) a->running = false;
}

static void macos_run(const char *path, const char *name, const char *args) {
    usize pl = strlen(path);
    bool gui = pl > 4 && !strcasecmp(path + pl - 4, ".app");   /* bundle: o app abre as próprias janelas */
    char *rep = kmalloc(8192);
    if (!rep) return;
    rep[0] = 0;
    if (!macho_report(path, rep, 8192)) {
        /* mostra o veredito (última linha não vazia do relatório) num alerta */
        char msg[240];
        const char *v = rep, *p = rep;
        while ((p = strchr(p, '\n'))) { p++; if (*p && *p != '\n') v = p; }
        strncpy(msg, v, sizeof msg - 1);
        msg[sizeof msg - 1] = 0;
        char *nl = strchr(msg, '\n');
        if (nl) *nl = 0;
        static const char *btn[] = {"OK"};
        char title[120];
        snprintf(title, sizeof title, "\"%s\" ainda não pode ser aberto no mOS", name);
        alert_show(title, msg, btn, 1, NULL, NULL);
        kfree(rep);
        return;
    }
    kfree(rep);
    char cwd[256], err[160] = "";
    strncpy(cwd, path, sizeof cwd - 1);
    cwd[sizeof cwd - 1] = 0;
    char *sl = strrchr(cwd, '/');
    if (sl && sl != cwd) *sl = 0;
    int pid = macho_spawn(path, args, cwd, err, sizeof err);
    if (pid < 0) { notify("Não foi possível abrir", err); return; }
    last_spawn_pid = pid;
    if (gui) { kprintf("apps: %s iniciado (pid %d)\n", name, pid); watch_add(pid, name); return; }
    app_launch(&app_terminal); /* a saída do programa aparece no Terminal */
    char m[160];
    snprintf(m, sizeof m, "%s (pid %d) em execução; a saída aparece no Terminal.", name, pid);
    notify("App do macOS iniciado", m);
}

/* "Não há um aplicativo definido para abrir o documento" (como o Finder): Escolher Aplicativo… / Cancelar */
static char noapp_path[300];
static void noapp_answer(int button, void *ctx) {
    (void)ctx;
    extern void finder_choose_app(const char *path);
    if (button == 0) finder_choose_app(noapp_path);
}

/* o documento parece texto? (sem bytes nulos no começo) */
static bool looks_text(vnode_t *n) {
    usize k = MIN(n->size, (usize)4096);
    for (usize i = 0; i < k; i++) if (!n->data[i]) return false;
    return true;
}

void open_path(const char *path) {
    vnode_t *n = vfs_lookup(path);
    if (!n) return;
    /* executável solto (sem bundle): roda, com a saída no Terminal */
    if (!n->dir && n->data && macho_is_macho(n->data, n->size)) { macos_run(path, n->name, ""); return; }
    /* bundles, pastas e documentos: o Launch Services escolhe o app (a escolha do usuário, ou o de maior posto) */
    if (ls_open(path, NULL)) return;
    if (n->dir) { finder_open_path(path); return; }
    if (!n->size || looks_text(n)) { editor_open_file(path); return; }
    snprintf(noapp_path, sizeof noapp_path, "%s", path);
    char title[360];
    snprintf(title, sizeof title, "Não há um aplicativo definido para abrir o documento “%s”.", n->name);
    static const char *btn[] = {"Escolher Aplicativo…", "Cancelar"};
    alert_show(title, "Escolha um aplicativo para abrir o documento. Para usá-lo sempre, marque “Sempre Abrir com”.", btn, 2, noapp_answer, NULL);
}

/* ---------------------------------------------------------------- texto monoespaçado */
int mono_cell_w(float size) { return (int)(size * 0.62f + 0.5f); }

void mono_draw(surface_t *s, float size, int x, int baseline, u32 color, const char *t, int maxcols) {
    font_t *f = font_get(FONT_MEDIUM);
    int cw = mono_cell_w(size);
    int col = 0;
    char buf[8];
    while (*t && col < maxcols) {
        const char *p = t;
        u32 cp = utf8_decode(&t);
        int n = (int)(t - p);
        memcpy(buf, p, n);
        buf[n] = 0;
        if (cp != ' ') {
            int gw = text_width(f, size, buf);
            text_draw(s, f, size, x + col * cw + (cw - gw) / 2, baseline, color, buf);
        }
        col++;
    }
}
