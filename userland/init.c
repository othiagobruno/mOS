/* mOS — init: primeiro processo de usuário, no papel do launchd. Lê os serviços em
 * /Sistema/Biblioteca/LaunchDaemons/*.servico (linhas "chave=valor"):
 *     Programa=relogiod          (nome em /Sistema/Programas ou caminho absoluto)
 *     Argumentos=10              (opcional)
 *     ManterVivo=sim             (reinicia se terminar; com limite para não entrar em laço)
 *     AtrasoMs=3000              (opcional: espera antes de iniciar, ex.: apps que precisam do desktop)
 * e os inicia; depois espera os filhos e reinicia quem precisar. */
#include "mos.h"

#define DIR "/Sistema/Biblioteca/LaunchDaemons"
#define MAX_SVC 16

typedef struct { char name[64], prog[128], args[128]; int keep, pid, restarts, delay_ms; u64 last_start; } svc_t;
static svc_t svc[MAX_SVC];
static int nsvc;

static void field(const char *txt, const char *key, char *out, usize cap) {
    usize kl = strlen(key);
    out[0] = 0;
    for (const char *l = txt; *l;) {
        if (!strncmp(l, key, kl) && l[kl] == '=') {
            const char *v = l + kl + 1;
            usize n = 0;
            while (v[n] && v[n] != '\n' && v[n] != '\r' && n + 1 < cap) { out[n] = v[n]; n++; }
            out[n] = 0;
            return;
        }
        while (*l && *l != '\n') l++;
        if (*l) l++;
    }
}

static void start(svc_t *s) {
    s->pid = spawn(s->prog, s->args[0] ? s->args : 0);
    s->last_start = time_ms();
    if (s->pid > 0) print("init: %s iniciado (pid %d)\n", s->name, (i64)s->pid);
    else print("init: %s: não foi possível iniciar %s\n", s->name, s->prog);
}

void _start(const char *arg) {
    (void)arg;
    char name[128], path[256], txt[1024];
    for (int i = 0; nsvc < MAX_SVC && readdir(DIR, i, name, sizeof name) == 1; i++) {
        usize n = strlen(name);
        if (n < 9 || strcmp(name + n - 8, ".servico")) continue;
        int k = 0;
        for (const char *s = DIR "/"; *s; s++) path[k++] = *s;
        for (const char *s = name; *s; s++) path[k++] = *s;
        path[k] = 0;
        i64 len = read_file(path, txt, sizeof txt - 1);
        if (len <= 0) continue;
        txt[len] = 0;
        svc_t *s = &svc[nsvc];
        memcpy(s->name, name, n - 8 < sizeof s->name - 1 ? n - 8 : sizeof s->name - 1);
        field(txt, "Programa", s->prog, sizeof s->prog);
        field(txt, "Argumentos", s->args, sizeof s->args);
        char keep[16];
        field(txt, "ManterVivo", keep, sizeof keep);
        s->keep = !strcmp(keep, "sim");
        char delay[16];
        field(txt, "AtrasoMs", delay, sizeof delay);
        for (const char *d = delay; *d >= '0' && *d <= '9'; d++) s->delay_ms = s->delay_ms * 10 + (*d - '0');
        if (!s->prog[0]) { print("init: %s sem Programa=, ignorado\n", name); continue; }
        nsvc++;
    }
    print("init: %d serviço(s) em " DIR "\n", (i64)nsvc);
    for (int i = 0; i < nsvc; i++) if (!svc[i].delay_ms) start(&svc[i]);
    for (int i = 0; i < nsvc; i++) if (svc[i].delay_ms) { sleep_ms(svc[i].delay_ms); start(&svc[i]); }

    for (;;) {
        int code = 0;
        int pid = waitany(&code);
        if (pid < 0) { sleep_ms(60000); continue; }          /* nada para vigiar: dorme */
        for (int i = 0; i < nsvc; i++) {
            svc_t *s = &svc[i];
            if (s->pid != pid) continue;
            s->pid = 0;
            print("init: %s (pid %d) terminou com código %d\n", s->name, (i64)pid, (i64)code);
            if (!s->keep) break;
            if (time_ms() - s->last_start < 10000 && ++s->restarts >= 5) {
                print("init: %s reiniciou demais, desistindo\n", s->name);
                break;
            }
            sleep_ms(1000);
            start(s);
            break;
        }
    }
}
