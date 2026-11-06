/* mOS — shell de usuário (ring 3). Lê comandos da entrada da sessão (o Terminal) e roda programas
 * de /Sistema/Programas como processos filhos. Embutidos: cd pwd ls cat echo mkdir rm mv ps kill
 * help exit. "programa arg..." roda e espera; terminar com " &" roda em segundo plano. */
#include "mos.h"

static char line[512];

static void cmd_ls(const char *dir) {
    char name[256], path[512];
    mos_stat_t st;
    for (int i = 0; readdir(dir, i, name, sizeof name) == 1; i++) {
        if (name[0] == '.') continue;
        int n = 0;
        for (const char *s = dir; *s; s++) path[n++] = *s;
        if (n && path[n - 1] != '/') path[n++] = '/';
        for (const char *s = name; *s; s++) path[n++] = *s;
        path[n] = 0;
        if (stat(path, &st) == 0 && st.is_dir) print("%s/\n", name);
        else print("%s  (%u bytes)\n", name, st.size);
    }
}

static void cmd_cat(const char *p) {
    int fd = open(p, MOS_O_RDONLY);
    if (fd < 0) { print("cat: %s: erro %d\n", p, (i64)fd); return; }
    char buf[512];
    i64 n;
    while ((n = read(fd, buf, sizeof buf)) > 0) write(1, buf, (usize)n);
    close(fd);
}

static const char *state_name(int s) { return s == 0 ? "pronto" : s == 1 ? "rodando" : s == 2 ? "dormindo" : s == 3 ? "esperando" : "?"; }

void _start(const char *arg) {
    (void)arg;
    print("msh (modo usuário, pid %d). Digite 'help'.\n", (i64)getpid());
    for (;;) {
        char cwd[128];
        getcwd(cwd, sizeof cwd);
        print("%s $ ", cwd);
        if (readline(line, sizeof line) < 0) { print("\n"); exit(0); }
        /* pipeline: "prog1 args | prog2 args" (programas, não embutidos) */
        char *bar = strchr(line, '|');
        if (bar) {
            *bar = 0;
            char *l = line, *r = bar + 1;
            while (*l == ' ') l++;
            while (*r == ' ') r++;
            char *la = l; while (*la && *la != ' ') la++; if (*la) *la++ = 0; while (*la == ' ') la++;
            char *ra = r; while (*ra && *ra != ' ') ra++; if (*ra) *ra++ = 0; while (*ra == ' ') ra++;
            usize ll = strlen(la); while (ll && la[ll - 1] == ' ') la[--ll] = 0;
            int fds[2];
            if (pipe(fds) < 0) { print("msh: pipe falhou\n"); continue; }
            int io1[3] = {-1, fds[1], -1}, io2[3] = {fds[0], -1, -1};
            int p1 = spawnio(l, *la ? la : 0, io1);
            int p2 = spawnio(r, *ra ? ra : 0, io2);
            close(fds[0]);
            close(fds[1]);                       /* só os filhos ficam com as pontas */
            if (p1 < 0) print("msh: %s: comando não encontrado\n", l);
            if (p2 < 0) print("msh: %s: comando não encontrado\n", r);
            if (p1 > 0) wait(p1);
            if (p2 > 0) wait(p2);
            continue;
        }
        /* separa comando e resto */
        char *c = line;
        while (*c == ' ') c++;
        if (!*c) continue;
        char *rest = c;
        while (*rest && *rest != ' ') rest++;
        if (*rest) *rest++ = 0;
        while (*rest == ' ') rest++;
        usize rl = strlen(rest);
        int bg = rl && rest[rl - 1] == '&';
        if (bg) { rest[--rl] = 0; while (rl && rest[rl - 1] == ' ') rest[--rl] = 0; }

        if (!strcmp(c, "exit")) exit(atoi(rest));
        else if (!strcmp(c, "help")) print("embutidos: cd pwd ls cat echo mkdir rm mv ps kill help exit\nprogramas: qualquer arquivo de /Sistema/Programas (ex.: filho oi, spin 500 &)\n");
        else if (!strcmp(c, "pwd")) print("%s\n", cwd);
        else if (!strcmp(c, "cd")) { if (chdir(*rest ? rest : "/Users/Visitante") < 0) print("cd: %s: não encontrado\n", rest); }
        else if (!strcmp(c, "ls")) cmd_ls(*rest ? rest : cwd);
        else if (!strcmp(c, "cat")) cmd_cat(rest);
        else if (!strcmp(c, "echo")) {
            char *gt = strchr(rest, '>');
            if (gt) {
                *gt = 0;
                char *f = gt + 1;
                while (*f == ' ') f++;
                usize n = strlen(rest);
                while (n && rest[n - 1] == ' ') rest[--n] = 0;
                rest[n] = '\n';
                write_file(f, rest, n + 1);
            } else print("%s\n", rest);
        }
        else if (!strcmp(c, "mkdir")) { if (mkdir(rest) < 0) print("mkdir: falhou\n"); }
        else if (!strcmp(c, "rm")) { if (unlink(rest) < 0) print("rm: %s: não encontrado\n", rest); }
        else if (!strcmp(c, "mv")) {
            char *b = strchr(rest, ' ');
            if (!b) { print("uso: mv origem destino\n"); continue; }
            *b++ = 0;
            if (rename(rest, b) < 0) print("mv: falhou\n");
        }
        else if (!strcmp(c, "ps")) {
            mos_procinfo_t pi;
            print("  PID  PAI  ESTADO     MEM KiB  CPU ms  NOME\n");
            for (int i = 0; proclist(i, &pi) == 1; i++)
                print("%5d %4d  %-9s %8u %7u  %s\n", (i64)pi.pid, (i64)pi.ppid, state_name(pi.state), pi.mem_kb, pi.cpu_ms, pi.name);
        }
        else if (!strcmp(c, "kill")) { if (kill(atoi(rest)) < 0) print("kill: processo não encontrado\n"); }
        else {
            int pid = spawn(c, *rest ? rest : 0);
            if (pid < 0) { print("msh: %s: comando não encontrado\n", c); continue; }
            if (bg) { print("[%d] %s\n", (i64)pid, c); continue; }
            int code = wait(pid);
            if (code) print("[%s terminou com código %d]\n", c, (i64)code);
        }
    }
}
