/* mOS — registro de aplicativos, abertura de arquivos por tipo e utilidades comuns */
#include "apps.h"
#include "compat.h"

static app_t *all_apps[] = {
    &app_arquivos, &app_horizonte, &app_terminal, &app_notas, &app_editor, &app_calculadora, &app_calendario,
    &app_musica, &app_minas, &app_visualizador, &app_atividade, &app_console, &app_preferencias, &app_sobre,
    &app_componentes, &app_instalador,
};

/* apps do macOS (.app em /Aplicativos) registrados dinamicamente: aparecem no Launchpad e na Busca */
#define MAX_MAC_APPS 24
static app_t *mac_apps[MAX_MAC_APPS];
static int nmac;
static void macos_run(const char *path, const char *name);
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
    macos_run(a->exec + 4, a->name);   /* exec = "mac:<caminho>" */
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
    vnode_t *d = vfs_lookup("/Aplicativos");
    for (vnode_t *c = d && d->dir ? d->child : NULL; c && nmac < MAX_MAC_APPS; c = c->next) {
        usize l = strlen(c->name);
        if (!c->dir || l <= 4 || strcasecmp(c->name + l - 4, ".app") || !vfs_child(c, "Contents")) continue;
        char path[300];
        snprintf(path, sizeof path, "/Aplicativos/%s", c->name);
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

static void macos_run(const char *path, const char *name) {
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
    int pid = macho_spawn(path, "", cwd, err, sizeof err);
    if (pid < 0) { notify("Não foi possível abrir", err); return; }
    last_spawn_pid = pid;
    if (gui) { kprintf("apps: %s iniciado (pid %d)\n", name, pid); watch_add(pid, name); return; }
    app_launch(&app_terminal); /* a saída do programa aparece no Terminal */
    char m[160];
    snprintf(m, sizeof m, "%s (pid %d) em execução; a saída aparece no Terminal.", name, pid);
    notify("App do macOS iniciado", m);
}

void open_path(const char *path) {
    vnode_t *n = vfs_lookup(path);
    if (!n) return;
    usize nl0 = strlen(n->name);
    bool bundle = n->dir && nl0 > 4 && !strcasecmp(n->name + nl0 - 4, ".app") && vfs_child(n, "Contents");
    if (bundle) { macos_run(path, n->name); return; }
    if (n->dir) { finder_open_path(path); return; }
    if (macho_is_macho(n->data, n->size)) { macos_run(path, n->name); return; }
    const char *e = vfs_ext(n->name);
    if (!strcasecmp(e, "html") || !strcasecmp(e, "htm")) {
        char url[160];
        snprintf(url, sizeof url, "file://%s", path);
        app_launch(&app_horizonte);
        browser_open_url(url);
    } else if (!strcasecmp(e, "bmp") || !strcasecmp(e, "ppm") || !strcasecmp(e, "png") || !strcasecmp(e, "jpg") || !strcasecmp(e, "jpeg")) {
        viewer_open_file(path);
    } else if (!strcasecmp(e, "dmg") || !strcasecmp(e, "img")) {
        char msg[300];
        if (vfs_mount_file(n) > 0) {
            snprintf(msg, sizeof msg, "\"%s\" montado", n->name);
            notify(msg, "Disponível em Volumes, na barra lateral do Arquivos.");
            vnode_t *vols = vfs_lookup("/Volumes");
            if (vols && vols->child) finder_open_path("/Volumes");
        } else {
            notify("Não foi possível abrir a imagem", n->name);
        }
    } else if (!strcasecmp(e, "app")) {
        char ex[64];
        strncpy(ex, n->name, sizeof ex - 1);
        ex[sizeof ex - 1] = 0;
        char *d = strrchr(ex, '.');
        if (d) *d = 0;
        for (int i = 0; i < apps_count(); i++) if (!strcmp(apps_get(i)->name, ex)) app_launch(apps_get(i));
    } else {
        editor_open_file(path);
    }
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
