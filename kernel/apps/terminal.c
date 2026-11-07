/* mOS — Terminal com o shell "msh" */
#include "apps.h"
#include "net.h"
#include "proc.h"
#include "account.h"
#include "compat.h"
#include "launchsvc.h"

#define LINES_MAX 600
#define COLS_MAX 200
#define FS 13.0f
#define LINE_H 17

typedef struct {
    char *lines[LINES_MAX];
    u32 colors[LINES_MAX];
    int nlines;
    char input[256];
    int caret;
    char cwd[192];
    char hist[32][256];
    int nhist, hidx;
    int scroll;            /* linhas acima do fim */
    /* trabalho assíncrono */
    int job;               /* 0 nenhum, 1 ping, 2 curl, 3 nslookup, 4 dhcp */
    u32 job_ip;
    char job_host[128];
    int job_count, job_seq;
    u64 job_t;
    http_req_t *job_http;
    int tty_pid;            /* sessão de terminal de um programa de ring 3 (0 = shell interno) */
    char tty_partial[512];  /* linha incompleta vinda do programa (ex.: o prompt dele) */
    bool job_resolving, job_waiting;
    int job_recv;
    window_t *win;
} term_t;

enum { J_NONE, J_PING, J_CURL, J_DNS, J_DHCP };

static void put(term_t *t, u32 color, const char *fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    char *p = buf;
    int wrap = 100; /* colunas: quebra linhas longas (o desenho também corta na largura da janela) */
    if (t->win) wrap = MAX(20, (t->win->w - 16) / mono_cell_w(FS));
    for (;;) {
        char *nl = strchr(p, '\n');
        int len = nl ? (int)(nl - p) : (int)strlen(p);
        /* quebra por número de caracteres (UTF-8) */
        int chars = 0, cut = 0;
        while (cut < len) { int nx = utf8_next(p, cut); if (nx > len) nx = len; if (++chars > wrap) break; cut = nx; }
        if (cut < len) { nl = p + cut - 1; len = cut; }
        if (t->nlines == LINES_MAX) {
            kfree(t->lines[0]);
            memmove(t->lines, t->lines + 1, sizeof(char *) * (LINES_MAX - 1));
            memmove(t->colors, t->colors + 1, sizeof(u32) * (LINES_MAX - 1));
            t->nlines--;
        }
        char *l = kmalloc(len + 1);
        memcpy(l, p, len);
        l[len] = 0;
        t->lines[t->nlines] = l;
        t->colors[t->nlines++] = color;
        if (!nl) break;
        p = nl + 1;
    }
    t->scroll = 0;
    if (t->win) wm_invalidate(t->win);
}

#define OUT(...) put(t, 0, __VA_ARGS__)
#define ERR(...) put(t, RGB(255, 110, 100), __VA_ARGS__)
#define OK(...)  put(t, RGB(120, 220, 130), __VA_ARGS__)

static void resolve(term_t *t, const char *in, char *out) {
    if (in[0] == '/') strncpy(out, in, 191);
    else if (in[0] == '~') snprintf(out, 192, "%s%s", CUR_HOME, in + 1);
    else snprintf(out, 192, "%s/%s", strcmp(t->cwd, "/") ? t->cwd : "", in);
    out[191] = 0;
    vnode_t *n = vfs_lookup(out);
    if (n) vfs_path(n, out, 192);
}

static void prompt_str(term_t *t, char *buf, int n) {
    const char *c = t->cwd;
    char shortp[192];
    if (!strncmp(c, CUR_HOME, strlen(CUR_HOME))) { snprintf(shortp, sizeof shortp, "~%s", c + strlen(CUR_HOME)); c = shortp; }
    char user[64];
    int k = 0;
    for (const char *q = account_current(); *q && k < 63; q++) user[k++] = (char)tolower(*q);
    user[k] = 0;
    snprintf(buf, n, "%s@mOS %s %% ", user, c);
}

static void cmd_sobre(term_t *t) {
    char vendor[16], brand[64];
    cpu_info(vendor, brand);
    framebuffer_t *fb = &g_boot->fb;
    u32 acc = T.accent;
    static const char *art[] = {
        "    .--.   .--.    ", "   /    \\ /    \\   ", "  |  .-. | .-.  |  ", "  |  | | | | |  |  ",
        "  |  | | | | |  |  ", "  '--' '-' '-' '--' ", "                   ",
    };
    char l[8][128];
    snprintf(l[0], 128, "visitante@mOS");
    snprintf(l[1], 128, "SO: %s %s x86_64", MOS_NAME, MOS_VERSION);
    snprintf(l[2], 128, "Kernel: Nucleo (UEFI, monolítico)");
    snprintf(l[3], 128, "CPU: %s", brand);
    snprintf(l[4], 128, "Memória: %llu MiB / %llu MiB", (pmm_total_pages() - pmm_free_pages()) / 256, pmm_total_pages() / 256);
    snprintf(l[5], 128, "Resolução: %ux%u", fb->width, fb->height);
    snprintf(l[6], 128, "Shell: msh 0.1   Tema: %s", T.dark ? "Escuro" : "Claro");
    for (int i = 0; i < 7; i++) {
        char line[200];
        snprintf(line, sizeof line, "%s  %s", art[i], l[i]);
        put(t, i == 0 ? acc : 0, "%s", line);
    }
}

static void list_dir(term_t *t, const char *path, bool longf) {
    vnode_t *d = vfs_lookup(path);
    if (!d) { ERR("ls: %s: Arquivo ou diretório inexistente", path); return; }
    if (!d->dir) { OUT("%s", d->name); return; }
    char line[256] = "";
    for (vnode_t *c = d->child; c; c = c->next) {
        if (c->name[0] == '.') continue;
        if (longf) {
            put(t, c->dir ? RGB(110, 170, 255) : 0, "%s %8llu  %s%s", c->dir ? "drwxr-xr-x" : "-rw-r--r--", (u64)c->size, c->name, c->dir ? "/" : "");
        } else {
            char item[80];
            snprintf(item, sizeof item, "%-18s", c->name);
            if (strlen(line) + strlen(item) > 72) { OUT("%s", line); line[0] = 0; }
            strcat(line, item);
        }
    }
    if (line[0]) OUT("%s", line);
}

static int split(char *s, char **argv, int max) {
    int n = 0;
    while (*s && n < max) {
        while (*s == ' ') s++;
        if (!*s) break;
        if (*s == '"') {
            s++;
            argv[n++] = s;
            while (*s && *s != '"') s++;
        } else {
            argv[n++] = s;
            while (*s && *s != ' ') s++;
        }
        if (*s) *s++ = 0;
    }
    return n;
}

static void run(term_t *t, char *line) {
    char *argv[16];
    char redirect[192] = "";
    char raw[256]; /* linha original: programas recebem os argumentos como digitados */
    strncpy(raw, line, sizeof raw - 1);
    raw[sizeof raw - 1] = 0;
    char *gt = strchr(line, '>');
    if (gt) { *gt = 0; char *f = gt + 1; while (*f == ' ') f++; resolve(t, f, redirect); }
    int argc = split(line, argv, 16);
    if (!argc) return;
    const char *c = argv[0];
    if (!strcmp(c, "help") || !strcmp(c, "ajuda")) {
        OUT("Comandos do msh:");
        OUT("  ls [-l] [dir]   cd <dir>      pwd          cat <arq>     echo <txt> [> arq]");
        OUT("  mkdir <dir>     touch <arq>   rm <arq>     mv <a> <b>    open <arq|app>");
        OUT("  clear           history       date         uptime        free");
        OUT("  uname [-a]      sobre         lspci        ps            dmesg");
        OUT("  ifconfig        ping <host>   nslookup <h> curl <url>    dhcp");
        OUT("  run <prog>      kill <pid>    sh  (shell de usuário em ring 3, interativo)");
        OUT("  bash [args]     bash do Homebrew (sem readline: a linha é editada pelo Terminal)");
        OUT("  ./prog args     macinfo <arq> (executáveis e .app do macOS, x86_64)");
        OUT("  tema claro|escuro             beep         reboot        shutdown");
    } else if (!strcmp(c, "clear")) {
        for (int i = 0; i < t->nlines; i++) kfree(t->lines[i]);
        t->nlines = 0;
    } else if (!strcmp(c, "pwd")) {
        OUT("%s", t->cwd);
    } else if (!strcmp(c, "cd")) {
        char p[192];
        resolve(t, argc > 1 ? argv[1] : "~", p);
        vnode_t *n = vfs_lookup(p);
        if (!n || !n->dir) ERR("cd: %s: Diretório inexistente", argc > 1 ? argv[1] : "~");
        else vfs_path(n, t->cwd, sizeof t->cwd);
    } else if (!strcmp(c, "ls")) {
        bool lf = argc > 1 && !strcmp(argv[1], "-l");
        char p[192];
        resolve(t, argc > (lf ? 2 : 1) ? argv[lf ? 2 : 1] : ".", p);
        list_dir(t, p, lf);
    } else if (!strcmp(c, "cat")) {
        for (int i = 1; i < argc; i++) {
            char p[192];
            resolve(t, argv[i], p);
            vnode_t *n = vfs_lookup(p);
            if (!n || n->dir) { ERR("cat: %s: Arquivo inexistente", argv[i]); continue; }
            char *tmp = kmalloc(n->size + 1);
            memcpy(tmp, n->data, n->size);
            tmp[n->size] = 0;
            OUT("%s", tmp);
            kfree(tmp);
        }
    } else if (!strcmp(c, "echo")) {
        char out[512] = "";
        for (int i = 1; i < argc; i++) { if (i > 1) strcat(out, " "); if (strlen(out) + strlen(argv[i]) < 500) strcat(out, argv[i]); }
        if (redirect[0]) { strcat(out, "\n"); vfs_write_file(redirect, out, strlen(out)); }
        else OUT("%s", out);
    } else if (!strcmp(c, "mkdir")) {
        for (int i = 1; i < argc; i++) { char p[192]; resolve(t, argv[i], p); vfs_mkdir(p); }
    } else if (!strcmp(c, "touch")) {
        for (int i = 1; i < argc; i++) { char p[192]; resolve(t, argv[i], p); if (!vfs_lookup(p)) vfs_write_file(p, "", 0); }
    } else if (!strcmp(c, "rm")) {
        for (int i = 1; i < argc; i++) {
            if (!strcmp(argv[i], "-r") || !strcmp(argv[i], "-rf")) continue;
            char p[192];
            resolve(t, argv[i], p);
            vnode_t *n = vfs_lookup(p);
            if (!n) ERR("rm: %s: Arquivo inexistente", argv[i]); else vfs_delete(n);
        }
    } else if (!strcmp(c, "mv")) {
        if (argc < 3) { ERR("uso: mv <origem> <destino>"); return; }
        char a[192], b[192];
        resolve(t, argv[1], a);
        resolve(t, argv[2], b);
        vnode_t *n = vfs_lookup(a), *d = vfs_lookup(b);
        if (!n) { ERR("mv: %s: inexistente", argv[1]); return; }
        if (d && d->dir) vfs_move(n, d, NULL);
        else {
            char *sl = strrchr(b, '/');
            *sl = 0;
            vnode_t *pd = vfs_lookup(b[0] ? b : "/");
            if (!pd) ERR("mv: destino inválido");
            else vfs_move(n, pd, sl + 1);
        }
    } else if (!strcmp(c, "open")) {
        /* como o /usr/bin/open do macOS: open [-a App | -b identificador] [arquivo ou URL...]; quem abre é o
         * Launch Services (o padrão do tipo/esquema, ou o app pedido) */
        if (argc < 2) { ERR("uso: open [-a App | -b identificador] [arquivo ou URL ...]"); return; }
        ls_app_t *with = NULL;
        int i = 1;
        if ((!strcmp(argv[1], "-a") || !strcmp(argv[1], "-b")) && argc > 2) {
            with = argv[1][1] == 'b' ? ls_app_by_id(argv[2]) : (argv[2][0] == '/' ? ls_app_by_path(argv[2]) : ls_app_by_name(argv[2]));
            if (!with) { ERR("Unable to find application named '%s'", argv[2]); return; }
            i = 3;
            if (i >= argc) { char ap[400]; snprintf(ap, sizeof ap, "%s", ls_app_path(with)); if (ap[0]) ls_open(ap, NULL); else app_launch(ls_app_builtin(with)); return; }
        }
        for (; i < argc; i++) {
            const char *a = argv[i];
            /* "esquema:" no começo (https://..., mailto:..., slack://...) é URL; o resto é caminho */
            int sl = 0;
            while (a[sl] && (isalnum((u8)a[sl]) || a[sl] == '+' || a[sl] == '-' || a[sl] == '.')) sl++;
            bool url = sl >= 2 && a[sl] == ':' && isalpha((u8)a[0]);
            if (url) { if (!ls_open(a, with)) ERR("No application knows how to open URL %s", a); continue; }
            char p[192];
            resolve(t, a, p);
            if (!vfs_lookup(p)) { ERR("The file %s does not exist.", p); continue; }
            if (with) { if (!ls_open(p, with)) ERR("open: %s não abriu %s", ls_app_name(with), a); }
            else open_path(p);
        }
    } else if (!strcmp(c, "history")) {
        for (int i = 0; i < t->nhist; i++) OUT("%4d  %s", i + 1, t->hist[i]);
    } else if (!strcmp(c, "date")) {
        datetime_t d;
        rtc_now(&d);
        static const char *wd[7] = {"dom", "seg", "ter", "qua", "qui", "sex", "sáb"};
        OUT("%s %02d/%02d/%04d %02d:%02d:%02d -03", wd[d.weekday], d.day, d.month, d.year, d.hour, d.minute, d.second);
    } else if (!strcmp(c, "uptime")) {
        u64 s = timer_ticks() / 1000;
        OUT("ligado há %llu min %llu s", s / 60, s % 60);
    } else if (!strcmp(c, "free")) {
        u64 tot = pmm_total_pages() * 4, fr = pmm_free_pages() * 4;
        OUT("              total        usada        livre");
        OUT("Mem:   %10llu K %10llu K %10llu K", tot, tot - fr, fr);
        OUT("Heap do kernel: %llu KiB", (u64)heap_used() / 1024);
    } else if (!strcmp(c, "uname")) {
        if (argc > 1 && !strcmp(argv[1], "-a")) OUT("mOS mOS 0.1 Nucleo x86_64 UEFI");
        else OUT("mOS");
    } else if (!strcmp(c, "sobre") || !strcmp(c, "neofetch")) {
        cmd_sobre(t);
    } else if (!strcmp(c, "lspci")) {
        for (int i = 0; i < pci_count(); i++) {
            pci_dev_t *d = pci_get(i);
            OUT("%02x:%02x.%x %04x:%04x %s", d->bus, d->dev, d->fn, d->vendor, d->device, pci_class_name(d->class_, d->subclass));
        }
    } else if (!strcmp(c, "ps")) {
        static const char *st[] = {"pronto", "rodando", "dormindo", "esperando"};
        OUT("  PID  ESTADO     MEM(KB)  CPU(ms)  NOME");
        OUT("    0  rodando          -        -  kernel_task (desktop, rede, USB, áudio)");
        for (int i = 0; i < proc_count(); i++) {
            int pid, state;
            char name[64];
            u64 mem, cpu;
            if (proc_info(i, &pid, name, sizeof name, &state, &mem, &cpu))
                OUT("%5d  %-9s %8llu %8llu  %s", pid, st[state & 3], mem, cpu, name);
        }
        OUT("Apps da interface:");
        for (int i = 0; i < apps_count(); i++) if (apps_get(i)->running) OUT("         %s", apps_get(i)->name);
    } else if (!strcmp(c, "run") || !strcmp(c, "executar") || (c[0] == '.' && c[1] == '/')) {
        const char *prog = (c[0] == '.') ? c : (argc > 1 ? argv[1] : NULL);
        if (!prog) { ERR("uso: run <programa> [argumentos]   (/Sistema/Programas: teste, sh, seq, wc...; ou ./programa do macOS)"); return; }
        /* argumentos = resto da linha original depois do programa */
        const char *rest = raw;
        int skip = (c[0] == '.') ? 1 : 2;
        for (int k = 0; k < skip; k++) {
            while (*rest == ' ') rest++;
            while (*rest && *rest != ' ') rest++;
        }
        while (*rest == ' ') rest++;
        char path[192];
        if (strchr(prog, '/')) resolve(t, prog, path); else { strncpy(path, prog, sizeof path - 1); path[sizeof path - 1] = 0; }
        vnode_t *n = strchr(path, '/') ? vfs_lookup(path) : NULL;
        usize pl = strlen(path);
        bool is_app = pl > 4 && !strcasecmp(path + pl - 4, ".app");
        if (n && (is_app || (!n->dir && macho_is_macho(n->data, n->size)))) {
            char err[160] = "";
            int pid = macho_spawn(path, rest, t->cwd, err, sizeof err);
            if (pid < 0) ERR("%s: %s", prog, err[0] ? err : "não foi possível executar");
            else OUT("[%d] %s iniciado (app do macOS, ring 3)", pid, prog);
            return;
        }
        int pid = proc_spawn(path, rest[0] ? rest : NULL);
        if (pid < 0) ERR("run: não foi possível executar %s", prog);
        else OUT("[%d] %s iniciado (ring 3)", pid, prog);
    } else if (!strcmp(c, "macinfo")) {
        if (argc < 2) { ERR("uso: macinfo <executável do macOS ou .app>"); return; }
        char path[192];
        resolve(t, argv[1], path);
        char *rep = kmalloc(8192);
        if (!rep) return;
        rep[0] = 0;
        bool ok = macho_report(path, rep, 8192);
        OUT("%s", rep);
        if (ok && strstr(rep, "modo experimental"))
            put(t, RGB(240, 200, 90), "→ abre só no modo experimental (para na 1ª função que o mOS não tem)");
        else put(t, ok ? RGB(120, 220, 130) : RGB(255, 110, 100), ok ? "→ compatível com o mOS" : "→ ainda não roda no mOS");
        kfree(rep);
    } else if (!strcmp(c, "sh")) {
        int pid = proc_spawn_tty("sh", argc > 1 ? argv[1] : NULL);
        if (pid < 0) { ERR("sh: não foi possível iniciar /Sistema/Programas/sh"); return; }
        t->tty_pid = pid;
        t->tty_partial[0] = 0;
        put(t, RGB(150, 150, 150), "[sh em ring 3, pid %d — exit ou Ctrl+D volta ao msh; Ctrl+C encerra]", pid);
    } else if (!strcmp(c, "bash")) {
        /* bash do Homebrew (docs/porte-brew.md). O Terminal ainda é de linha (não emula VT100), então o
         * readline fica desligado: a edição e o histórico da linha são os do próprio Terminal. +m: sem
         * controle de jobs (o mOS ainda não tem grupos de processo). */
        static const char bash_path[] = "/usr/local/bin/bash";
        if (!vfs_lookup(bash_path)) { ERR("bash: %s não existe (tools/stage-bash.ps1 instala)", bash_path); return; }
        char args[200];
        const char *rest = raw;
        while (*rest == ' ') rest++;
        while (*rest && *rest != ' ') rest++;
        while (*rest == ' ') rest++;
        snprintf(args, sizeof args, "--noediting +m %s", rest[0] ? rest : "-i");
        int pid = proc_spawn_tty(bash_path, args);
        if (pid < 0) { ERR("bash: não foi possível iniciar %s", bash_path); return; }
        t->tty_pid = pid;
        t->tty_partial[0] = 0;
        put(t, RGB(150, 150, 150), "[bash do Homebrew, pid %d — exit ou Ctrl+D volta ao msh; Ctrl+C interrompe]", pid);
    } else if (!strcmp(c, "kill")) {
        if (argc < 2) { ERR("uso: kill <pid>"); return; }
        if (!proc_kill(atoi(argv[1]))) ERR("kill: processo %s não encontrado", argv[1]); else OUT("processo %s encerrado", argv[1]);
    } else if (!strcmp(c, "dmesg")) {
        extern char klog_buf[];
        char *p = klog_buf;
        int lines = 0;
        for (char *q = p; *q; q++) if (*q == '\n') lines++;
        int skip = MAX(0, lines - 40);
        while (skip && *p) { if (*p++ == '\n') skip--; }
        OUT("%s", p);
    } else if (!strcmp(c, "ifconfig")) {
        net_info_t *ni = net_info();
        char ip[20], mk[20], gw[20], dn[20];
        ip_to_str(ni->ip, ip); ip_to_str(ni->mask, mk); ip_to_str(ni->gw, gw); ip_to_str(ni->dns, dn);
        OUT("en0: flags=<%s> driver %s", ni->link ? "UP,RUNNING" : "DOWN", ni->driver[0] ? ni->driver : "nenhum");
        OUT("     ether %02x:%02x:%02x:%02x:%02x:%02x", ni->mac[0], ni->mac[1], ni->mac[2], ni->mac[3], ni->mac[4], ni->mac[5]);
        OUT("     inet %s netmask %s", ni->configured ? ip : "-", ni->configured ? mk : "-");
        OUT("     gateway %s  dns %s", ni->configured ? gw : "-", ni->configured ? dn : "-");
        OUT("     RX %llu pacotes (%llu bytes)  TX %llu pacotes (%llu bytes)", ni->rx_packets, ni->rx_bytes, ni->tx_packets, ni->tx_bytes);
    } else if (!strcmp(c, "dhcp")) {
        net_dhcp_renew();
        OUT("Solicitando endereço via DHCP…");
        t->job = J_DHCP; t->job_t = timer_ticks();
    } else if (!strcmp(c, "ping") || !strcmp(c, "nslookup")) {
        if (argc < 2) { ERR("uso: %s <host>", c); return; }
        strncpy(t->job_host, argv[1], sizeof t->job_host - 1);
        t->job = !strcmp(c, "ping") ? J_PING : J_DNS;
        t->job_resolving = true;
        t->job_count = 0; t->job_seq = 0; t->job_recv = 0; t->job_waiting = false; t->job_t = timer_ticks();
        if (str_to_ip(argv[1], &t->job_ip)) t->job_resolving = false;
        if (t->job == J_PING) OUT("PING %s: 56 bytes de dados", argv[1]);
    } else if (!strcmp(c, "curl")) {
        if (argc < 2) { ERR("uso: curl <url>"); return; }
        char url[300];
        if (strncmp(argv[1], "http", 4)) snprintf(url, sizeof url, "http://%s", argv[1]); else strncpy(url, argv[1], sizeof url - 1);
        t->job_http = http_get(url);
        t->job = J_CURL;
        if (redirect[0]) strncpy(t->job_host, redirect, sizeof t->job_host - 1); else t->job_host[0] = 0;
    } else if (!strcmp(c, "tema")) {
        bool dark = argc > 1 && !strcmp(argv[1], "escuro");
        theme_set(dark, g_accent_index);
        desktop_rebuild_wallpaper();
        extern window_t *wm_nth(int);
        for (int i = 0;; i++) { window_t *w = wm_nth(i); if (!w) break; w->needs_paint = true; }
    } else if (!strcmp(c, "beep")) {
        sound_alert();
    } else if (!strcmp(c, "reboot")) {
        desktop_shutdown_dialog(1);
    } else if (!strcmp(c, "shutdown") || !strcmp(c, "halt")) {
        desktop_shutdown_dialog(0);
    } else if (!strcmp(c, "exit")) {
        wm_close(t->win);
    } else {
        ERR("msh: comando não encontrado: %s", c);
    }
}

static void submit(term_t *t) {
    char pr[256];
    prompt_str(t, pr, sizeof pr);
    put(t, RGB(150, 150, 150), "%s%s", pr, t->input);
    if (t->input[0]) {
        if (t->nhist == 32) { memmove(t->hist, t->hist + 1, sizeof(t->hist[0]) * 31); t->nhist--; }
        strcpy(t->hist[t->nhist++], t->input);
    }
    t->hidx = t->nhist;
    char line[256];
    strcpy(line, t->input);
    t->input[0] = 0;
    t->caret = 0;
    run(t, line);
}

static void tick_job(term_t *t) {
    if (!t->job) return;
    u64 now = timer_ticks();
    if ((t->job == J_PING || t->job == J_DNS) && t->job_resolving) {
        int r = dns_resolve(t->job_host, &t->job_ip);
        if (r < 0 || now - t->job_t > 6000) { ERR("%s: não foi possível resolver %s", t->job == J_PING ? "ping" : "nslookup", t->job_host); t->job = 0; return; }
        if (r == 0) return;
        t->job_resolving = false;
        if (t->job == J_DNS) {
            char ip[20];
            ip_to_str(t->job_ip, ip);
            OUT("Nome:     %s", t->job_host);
            OUT("Endereço: %s", ip);
            t->job = 0;
            return;
        }
        t->job_t = 0;
    }
    if (t->job == J_PING) {
        u64 rtt;
        if (t->job_waiting && icmp_ping_reply((u16)t->job_seq, &rtt)) {
            char ip[20];
            ip_to_str(t->job_ip, ip);
            OUT("64 bytes de %s: icmp_seq=%d ttl=64 tempo=%llu ms", ip, t->job_seq, rtt);
            t->job_waiting = false;
            t->job_recv++;
        }
        if (t->job_t == 0 || now - t->job_t >= 1000) {
            if (t->job_waiting) ERR("Tempo esgotado para icmp_seq %d", t->job_seq);
            if (t->job_count >= 4) {
                OUT("--- %s: %d pacotes enviados, %d recebidos, %d%% de perda ---", t->job_host, t->job_count, t->job_recv,
                    (t->job_count - t->job_recv) * 100 / t->job_count);
                t->job = 0;
                return;
            }
            t->job_seq = ++t->job_count;
            icmp_ping_send(t->job_ip, (u16)t->job_seq);
            t->job_t = now;
            t->job_waiting = true;
        }
    } else if (t->job == J_CURL) {
        int r = http_poll(t->job_http);
        if (r == 0) return;
        if (r < 0) ERR("curl: %s", t->job_http->error);
        else {
            if (t->job_host[0]) {
                vfs_write_file(t->job_host, t->job_http->body, t->job_http->body_len);
                OUT("%u bytes salvos em %s (HTTP %d)", t->job_http->body_len, t->job_host, t->job_http->status);
            } else {
                u32 n = MIN(t->job_http->body_len, 6000);
                char *tmp = kmalloc(n + 1);
                memcpy(tmp, t->job_http->body, n);
                tmp[n] = 0;
                for (u32 i = 0; i < n; i++) if ((u8)tmp[i] < 32 && tmp[i] != '\n' && tmp[i] != '\t') tmp[i] = ' ';
                OUT("%s", tmp);
                if (t->job_http->body_len > n) OUT("[… %u bytes omitidos]", t->job_http->body_len - n);
                kfree(tmp);
            }
        }
        http_free(t->job_http);
        t->job_http = NULL;
        t->job = 0;
    } else if (t->job == J_DHCP) {
        net_info_t *ni = net_info();
        if (ni->configured) {
            char ip[20];
            ip_to_str(ni->ip, ip);
            OK("Endereço obtido: %s", ip);
            t->job = 0;
        } else if (now - t->job_t > 8000) { ERR("DHCP: sem resposta"); t->job = 0; }
    }
}

static void paint(window_t *w, ui_t *u) {
    term_t *t = w->data;
    surface_t *s = u->s;
    gfx_fill(s, 0, 0, u->cw, u->ch, RGBA(28, 28, 30, 255));
    int cols = MIN(COLS_MAX, (u->cw - 16) / mono_cell_w(FS));
    int rows = (u->ch - 12) / LINE_H;
    char pr[512];
    if (t->tty_pid) { strncpy(pr, t->tty_partial, sizeof pr - 1); pr[sizeof pr - 1] = 0; }
    else prompt_str(t, pr, sizeof pr);
    /* linha de entrada é a última */
    int total = t->nlines + 1;
    if (u->wheel) { t->scroll = CLAMP(t->scroll - u->wheel * 3, 0, MAX(0, total - rows)); }
    int first = MAX(0, total - rows - t->scroll);
    int y = 8 + LINE_H - 4;
    for (int i = first; i < total && y < u->ch; i++) {
        if (i < t->nlines) {
            mono_draw(s, FS, 8, y, t->colors[i] ? t->colors[i] : RGB(232, 232, 232), t->lines[i], cols);
        } else if (!t->job) {
            int pc = mono_cell_w(FS);
            mono_draw(s, FS, 8, y, RGB(120, 200, 255), pr, cols);
            int plen = 0;
            for (const char *q = pr; *q; plen++) utf8_decode(&q);
            mono_draw(s, FS, 8 + plen * pc, y, RGB(240, 240, 240), t->input, cols - plen);
            int cpos = 0;
            for (const char *q = t->input; q < t->input + t->caret; cpos++) utf8_decode(&q);
            bool on = ((u->now - u->caret_blink) / 530) % 2 == 0;
            if (on && wm_focused() == w) gfx_fill(s, 8 + (plen + cpos) * pc, y - 12, pc, 15, RGBA(220, 220, 220, 200));
            u->repaint = true;
        }
        y += LINE_H;
    }
    /* teclado */
    if (u->key) {
        int len = (int)strlen(t->input);
        u->caret_blink = u->now;
        if (t->tty_pid && u->key == KEY_ENTER) {
            put(t, 0, "%s%s", t->tty_partial, t->input);
            t->tty_partial[0] = 0;
            char line[260];
            snprintf(line, sizeof line, "%s\n", t->input);
            proc_tty_write(t->tty_pid, line, strlen(line));
            t->input[0] = 0; t->caret = 0;
        }
        else if (t->tty_pid && u->key == 'd' && (u->mods & MOD_CTRL)) proc_tty_eof(t->tty_pid);
        else if (t->tty_pid && u->key == 'c' && (u->mods & MOD_CTRL)) { proc_tty_interrupt(t->tty_pid); put(t, RGB(255, 110, 100), "^C"); }
        else if (u->key == KEY_ENTER) { if (!t->job) submit(t); }
        else if (u->key == 'c' && (u->mods & MOD_CTRL)) { if (t->job) { ERR("^C"); if (t->job_http) { http_free(t->job_http); t->job_http = NULL; } t->job = 0; } else { put(t, RGB(150, 150, 150), "%s%s^C", pr, t->input); t->input[0] = 0; t->caret = 0; } }
        else if (u->key == 'l' && (u->mods & MOD_CTRL)) { char cl[] = "clear"; run(t, cl); }
        else if (u->key == KEY_BACKSPACE) { if (t->caret > 0) { int p = utf8_prev(t->input, t->caret); memmove(t->input + p, t->input + t->caret, len - t->caret + 1); t->caret = p; } }
        else if (u->key == KEY_LEFT) t->caret = utf8_prev(t->input, t->caret);
        else if (u->key == KEY_RIGHT) t->caret = utf8_next(t->input, t->caret);
        else if (u->key == KEY_HOME) t->caret = 0;
        else if (u->key == KEY_END) t->caret = len;
        else if (u->key == KEY_UP) { if (t->hidx > 0) { t->hidx--; strcpy(t->input, t->hist[t->hidx]); t->caret = (int)strlen(t->input); } }
        else if (u->key == KEY_DOWN) { if (t->hidx < t->nhist - 1) { t->hidx++; strcpy(t->input, t->hist[t->hidx]); } else { t->hidx = t->nhist; t->input[0] = 0; } t->caret = (int)strlen(t->input); }
        else if (u->key == KEY_TAB) {
            /* completa nomes de arquivo */
            char *sp = strrchr(t->input, ' ');
            char *word = sp ? sp + 1 : t->input;
            vnode_t *d = vfs_lookup(t->cwd);
            for (vnode_t *ch = d ? d->child : NULL; ch && word[0]; ch = ch->next)
                if (!strncmp(ch->name, word, strlen(word)) && len + strlen(ch->name) < 250) { strcpy(word, ch->name); if (ch->dir) strcat(word, "/"); t->caret = (int)strlen(t->input); break; }
        }
        else if (u->text[0] && !(u->mods & (MOD_CMD | MOD_CTRL))) {
            int tl = (int)strlen(u->text);
            if (len + tl < 250) { memmove(t->input + t->caret + tl, t->input + t->caret, len - t->caret + 1); memcpy(t->input + t->caret, u->text, tl); t->caret += tl; }
        }
        t->scroll = 0;
        u->key_used = true;
    }
    if (u->pressed && u->inside) u->focus = 1;
}

/* chamado antes de ejetar um volume: quem estiver dentro dele volta para a pasta pessoal */
void terminal_leave_path(const char *prefix) {
    usize n = strlen(prefix);
    extern window_t *wm_nth(int);
    for (int i = 0;; i++) {
        window_t *w = wm_nth(i);
        if (!w) break;
        if (w->app != &app_terminal || !w->data) continue;
        term_t *t = w->data;
        if (!strncmp(t->cwd, prefix, n) && (t->cwd[n] == 0 || t->cwd[n] == '/')) {
            strcpy(t->cwd, CUR_HOME);
            put(t, RGB(150, 150, 150), "(volume ejetado — diretório atual mudou para ~)");
        }
    }
}

static void tick_tty(term_t *t) {
    char buf[1024];
    usize n;
    while ((n = proc_tty_read(t->tty_pid, buf, sizeof buf - 1)) > 0) {
        /* sem emulação de terminal: tira \r, sequências ESC (CSI/OSC) e aplica o backspace */
        usize o = 0;
        for (usize i = 0; i < n; i++) {
            unsigned char ch = (unsigned char)buf[i];
            if (ch == 0x1B) {
                if (i + 1 < n && buf[i + 1] == '[') { i += 2; while (i < n && !((unsigned char)buf[i] >= 0x40 && (unsigned char)buf[i] <= 0x7E)) i++; }
                else if (i + 1 < n && buf[i + 1] == ']') { i += 2; while (i < n && buf[i] != 7 && !(buf[i] == 0x1B && i + 1 < n && buf[i + 1] == '\\')) i++; if (i < n && buf[i] == 0x1B) i++; }
                else i++;
                continue;
            }
            if (ch == '\r' || ch == 7) continue;
            if (ch == 8) { if (o) o--; continue; }
            buf[o++] = (char)ch;
        }
        n = o;
        buf[n] = 0;
        char *p = buf;
        for (;;) {
            char *nl = strchr(p, '\n');
            usize pl = strlen(t->tty_partial);
            if (!nl) { usize add = MIN(strlen(p), sizeof t->tty_partial - pl - 1); memcpy(t->tty_partial + pl, p, add); t->tty_partial[pl + add] = 0; break; }
            *nl = 0;
            put(t, 0, "%s%s", t->tty_partial, p);
            t->tty_partial[0] = 0;
            p = nl + 1;
        }
        if (t->win) wm_invalidate(t->win);
    }
    if (!proc_alive(t->tty_pid)) {
        if (t->tty_partial[0]) put(t, 0, "%s", t->tty_partial);
        put(t, RGB(150, 150, 150), "[sessão terminou (código %d)]", proc_exit_code(t->tty_pid));
        t->tty_pid = 0;
        t->tty_partial[0] = 0;
        if (t->win) wm_invalidate(t->win);
    }
}

static void tick(app_t *a) {
    extern window_t *wm_nth(int);
    for (int i = 0;; i++) {
        window_t *w = wm_nth(i);
        if (!w) break;
        if (w->app == &app_terminal && w->data && ((term_t *)w->data)->tty_pid) tick_tty(w->data);
    }
    /* saída dos programas de usuário (ring 3) vai para o Terminal em foco (ou o primeiro aberto) */
    {
        char buf[1024];
        usize n = proc_console_read(buf, sizeof buf - 1);
        if (n) {
            buf[n] = 0;
            term_t *dst = NULL;
            window_t *f = wm_focused();
            if (f && f->app == &app_terminal && f->data) dst = f->data;
            for (int i = 0; !dst; i++) { window_t *w = wm_nth(i); if (!w) break; if (w->app == &app_terminal && w->data) dst = w->data; }
            if (dst) { if (n && buf[n - 1] == '\n') buf[n - 1] = 0; put(dst, RGB(200, 220, 255), "%s", buf); }
        }
    }
    for (int i = 0;; i++) {
        window_t *w = wm_nth(i);
        if (!w) break;
        if (w->app == &app_terminal && w->data) tick_job(w->data);
    }
}

static void on_close(window_t *w) {
    term_t *t = w->data;
    for (int i = 0; i < t->nlines; i++) kfree(t->lines[i]);
    if (t->job_http) http_free(t->job_http);
    kfree(t);
    w->data = NULL;
}

static void launch(app_t *a) {
    window_t *w = wm_create(&app_terminal, "Terminal — msh — 80×24", -1, -1, 700, 430, WIN_RESIZABLE);
    if (!w) return;
    term_t *t = kcalloc(1, sizeof(term_t));
    t->win = w;
    strcpy(t->cwd, CUR_HOME);
    w->data = t;
    w->paint = paint;
    w->on_close = on_close;
    w->bg = RGB(28, 28, 30);
    datetime_t d;
    rtc_now(&d);
    put(t, RGB(150, 150, 150), "Último login: %02d/%02d/%04d %02d:%02d no console", d.day, d.month, d.year, d.hour, d.minute);
    put(t, 0, "Bem-vindo ao msh. Digite 'help' para ver os comandos.");
}

static menu_item_t m_shell[] = {{"Nova Janela", "⌘N", 1}, {NULL}, {"Limpar", "⌘K", 2}};
static menu_t menus[] = {{"Shell", m_shell, ARRAY_LEN(m_shell)}};

static void on_menu(app_t *a, int act) {
    if (act == 1) launch(a);
    if (act == 2) {
        window_t *w = wm_focused();
        if (w && w->app == a) { char cl[] = "clear"; run(w->data, cl); wm_invalidate(w); }
    }
}

app_t app_terminal = {
    .name = "Terminal", .exec = "terminal", .draw_icon = icon_terminal, .launch = launch, .tick = tick,
    .on_menu = on_menu, .menus = menus, .nmenus = ARRAY_LEN(menus), .pinned = true,
};
