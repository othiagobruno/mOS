/* mOS — init: primeiro processo de usuário, no papel do launchd.
 *
 * Jobs vêm de dois formatos:
 *  1. .servico do mOS em /Sistema/Biblioteca/LaunchDaemons (linhas "chave=valor"):
 *         Programa=relogiod          (nome em /Sistema/Programas ou caminho absoluto)
 *         Argumentos=10              (opcional)
 *         ManterVivo=sim             (reinicia se terminar; com limite para não entrar em laço)
 *         AtrasoMs=3000              (opcional: espera antes de iniciar, ex.: apps que precisam do desktop)
 *  2. plists do launchd (XML) em /System/Library/LaunchDaemons, /Library/LaunchDaemons, /Library/LaunchAgents,
 *     /Users/<nome>/Library/LaunchAgents e /Sistema/Biblioteca/LaunchDaemons: Label, Program, ProgramArguments,
 *     MachServices, RunAtLoad, KeepAlive, EnvironmentVariables, WorkingDirectory, StandardOutPath,
 *     StandardErrorPath, Disabled.
 *
 * MachServices: cada nome é reservado no bootstrap do kernel (MACH_BS_RESERVE): o launchd guarda o direito de
 * receber num conjunto de portas e, quando chega a primeira mensagem, inicia o job (ativação sob demanda). O
 * check_in do daemon leva o direito com a fila; quando o daemon termina, o kernel devolve o direito ao launchd,
 * e a próxima mensagem o inicia de novo. Os clientes não percebem nada (o envio continua valendo).
 *
 * Controle (launchctl, SMAppService, serviços XPC dentro de bundles): serviço Mach LAUNCHD_SERVICE, protocolo em
 * svcproto.h (LC_*). Uma thread espera os filhos (waitany); a principal atende as portas. */
#include "mos.h"
#include "svcnative.h"
#include "plistmini.h"

#define LEGACY_DIR "/Sistema/Biblioteca/LaunchDaemons"
#define MAX_JOBS 64
#define MAX_ARGS 16
#define MAX_ENV  16
#define MAX_SVC  8

typedef struct {
    int used, legacy;
    char label[128], src[256];
    char prog[256], args[256];            /* legacy: Programa/Argumentos */
    char argv[MAX_ARGS][160]; int argc;
    char env[MAX_ENV][192]; int envc;
    char svc[MAX_SVC][128]; unsigned port[MAX_SVC]; int nsvc;
    char out[192], err[192], wd[192];
    int keep, runatload, delay_ms;
    volatile int pid;
    int restarts, last_status, crashes;
    u64 last_start, restart_at;
} job_t;

static job_t jobs[MAX_JOBS];
static unsigned pset, ctl;
static volatile int lk;
static void lock(void) { while (__sync_lock_test_and_set(&lk, 1)) yield(); }
static void unlock(void) { __sync_lock_release(&lk); }

static void scopy(char *d, const char *s, usize cap) { usize n = 0; if (s) while (s[n] && n + 1 < cap) { d[n] = s[n]; n++; } d[n] = 0; }
static int ends(const char *s, const char *suf) { usize a = strlen(s), b = strlen(suf); return a >= b && !strcmp(s + a - b, suf); }
static void cat2(char *out, usize cap, const char *a, const char *b) {
    char tmp[256];
    usize n = 0;
    for (; *a && n + 1 < sizeof tmp; a++) tmp[n++] = *a;
    for (; *b && n + 1 < sizeof tmp; b++) tmp[n++] = *b;
    tmp[n] = 0;
    scopy(out, tmp, cap);
}
static void join(char *out, usize cap, const char *a, const char *b) {
    usize n = 0;
    for (; *a && n + 1 < cap; a++) out[n++] = *a;
    if (n + 1 < cap) out[n++] = '/';
    for (; *b && n + 1 < cap; b++) out[n++] = *b;
    out[n] = 0;
}

static job_t *job_find(const char *label) {
    for (int i = 0; i < MAX_JOBS; i++) if (jobs[i].used && !strcmp(jobs[i].label, label)) return &jobs[i];
    return 0;
}
static job_t *job_new(const char *label) {
    if (job_find(label)) return 0;
    for (int i = 0; i < MAX_JOBS; i++)
        if (!jobs[i].used) { memset(&jobs[i], 0, sizeof jobs[i]); jobs[i].used = 1; scopy(jobs[i].label, label, sizeof jobs[i].label); return &jobs[i]; }
    return 0;
}

/* ---------------------------------------------------------------- iniciar */
static const char *base_env[] = { "HOME=/Users/Visitante", "USER=visitante", "LOGNAME=visitante",
                                  "PATH=/usr/local/bin:/Sistema/Programas:/usr/bin:/bin:/usr/sbin:/sbin", "TERM=dumb",
                                  "LANG=pt_BR.UTF-8", "SHELL=/bin/msh", "TMPDIR=/tmp/" };

static void start(job_t *j) {
    j->last_start = time_ms();
    j->restart_at = 0;
    if (j->legacy) {
        j->pid = spawn(j->prog, j->args[0] ? j->args : 0);
    } else {
        const char *argv[MAX_ARGS + 1], *envp[MAX_ENV + 16];
        int n = 0, e = 0;
        for (int i = 0; i < j->argc; i++) argv[n++] = j->argv[i];
        argv[n] = 0;
        static char xsn[192];
        cat2(xsn, sizeof xsn, "XPC_SERVICE_NAME=", j->label);
        for (usize i = 0; i < sizeof base_env / sizeof *base_env; i++) {
            int over = 0;
            for (int k = 0; k < j->envc; k++) {
                const char *eq = strchr(j->env[k], '=');
                usize kl = eq ? (usize)(eq - j->env[k]) : 0;
                if (kl && !strncmp(j->env[k], base_env[i], kl) && base_env[i][kl] == '=') over = 1;
            }
            if (!over) envp[e++] = base_env[i];
        }
        for (int k = 0; k < j->envc; k++) envp[e++] = j->env[k];
        envp[e++] = xsn;
        envp[e] = 0;
        mos_spawn_fa_t acts[3];
        int na = 0;
        const char *paths[2] = { j->out, j->err };
        for (int s = 0; s < 2; s++) {
            if (!paths[s][0]) continue;
            if (file_size(paths[s]) < 0) write_file(paths[s], "", 0);
            memset(&acts[na], 0, sizeof acts[na]);
            acts[na].kind = MOS_SPAWN_FA_OPEN;
            acts[na].a = s + 1;
            acts[na].flags = MOS_O_WRONLY | MOS_O_CREAT | MOS_O_APPEND;
            acts[na].mode = 0644;
            scopy(acts[na].path, paths[s], sizeof acts[na].path);
            na++;
        }
        if (j->wd[0]) {
            memset(&acts[na], 0, sizeof acts[na]);
            acts[na].kind = MOS_SPAWN_FA_CHDIR;
            scopy(acts[na].path, j->wd, sizeof acts[na].path);
            na++;
        }
        mos_spawn_vec_t v = { (u64)argv, (u64)envp, (u64)acts, (unsigned)na, 0 };
        const char *path = j->prog[0] ? j->prog : j->argv[0];
        j->pid = (int)sys3(SYS_SPAWNVEC, (u64)path, (u64)&v, 0);
    }
    if (j->pid > 0) print("launchd: %s iniciado (pid %d)\n", j->label, (i64)j->pid);
    else { print("launchd: %s: não foi possível iniciar %s\n", j->label, j->prog[0] ? j->prog : j->argv[0]); j->pid = 0; j->last_status = 78; }
}

static void reserve(job_t *j) {
    for (int i = 0; i < j->nsvc; i++) {
        unsigned name = 0;
        i64 r = bs_op(MACH_BS_RESERVE, j->svc[i], &name);
        if (r == 0 && name) { j->port[i] = name; port_join(name, pset); }
        else print("launchd: %s: nome %s não reservado (%d)\n", j->label, j->svc[i], r);
    }
}
static void release(job_t *j) {
    for (int i = 0; i < j->nsvc; i++) if (j->port[i]) { port_destroy(j->port[i]); j->port[i] = 0; }
}
/* job com algum nome Mach com mensagem esperando e sem processo: hora de iniciar */
static int wants_start(job_t *j) {
    if (j->pid || !j->nsvc) return 0;
    for (int i = 0; i < j->nsvc; i++) if (j->port[i] && port_pending(j->port[i])) return 1;
    return 0;
}

/* ---------------------------------------------------------------- leitura dos jobs */
static char filebuf[65536];

static void load_legacy(const char *path, const char *fname) {
    i64 len = read_file(path, filebuf, sizeof filebuf - 1);
    if (len <= 0) return;
    filebuf[len] = 0;
    char label[128];
    scopy(label, fname, sizeof label);
    label[strlen(label) - 8] = 0;                         /* tira ".servico" */
    job_t *j = job_new(label);
    if (!j) return;
    j->legacy = 1;
    scopy(j->src, path, sizeof j->src);
    char v[128];
    for (const char *l = filebuf; *l;) {
        const char *eq = l;
        while (*eq && *eq != '=' && *eq != '\n') eq++;
        if (*eq == '=') {
            usize kl = (usize)(eq - l), n = 0;
            const char *s = eq + 1;
            while (s[n] && s[n] != '\n' && s[n] != '\r' && n + 1 < sizeof v) { v[n] = s[n]; n++; }
            v[n] = 0;
            if (kl == 8 && !strncmp(l, "Programa", 8)) scopy(j->prog, v, sizeof j->prog);
            else if (kl == 10 && !strncmp(l, "Argumentos", 10)) scopy(j->args, v, sizeof j->args);
            else if (kl == 10 && !strncmp(l, "ManterVivo", 10)) j->keep = !strcmp(v, "sim");
            else if (kl == 7 && !strncmp(l, "AtrasoMs", 7)) j->delay_ms = atoi(v);
        }
        while (*l && *l != '\n') l++;
        if (*l) l++;
    }
    if (!j->prog[0]) { print("launchd: %s sem Programa=, ignorado\n", fname); j->used = 0; return; }
    j->runatload = 1;
}

/* devolve o job (ou 0) e uma mensagem de erro curta em *why */
static job_t *load_plist(const char *path, const char **why) {
    *why = "erro de leitura";
    i64 len = read_file(path, filebuf, sizeof filebuf - 1);
    if (len <= 0) return 0;
    filebuf[len] = 0;
    if (len >= 6 && !memcmp(filebuf, "bplist", 6)) { *why = "plist binário não suportado"; return 0; }
    pnode_t *root = plist_parse(filebuf, (usize)len);
    job_t *j = 0;
    const char *label = plist_str(root, "Label");
    if (!root || root->type != PN_DICT) { *why = "plist inválido"; goto out; }
    if (!label) { *why = "sem Label"; goto out; }
    if (plist_bool(root, "Disabled", 0)) { *why = "Disabled"; goto out; }
    if (job_find(label)) { *why = "já carregado"; goto out; }
    j = job_new(label);
    if (!j) { *why = "tabela cheia"; goto out; }
    scopy(j->src, path, sizeof j->src);
    scopy(j->prog, plist_str(root, "Program"), sizeof j->prog);
    pnode_t *pa = plist_get(root, "ProgramArguments");
    if (pa && pa->type == PN_ARRAY)
        for (pnode_t *c = pa->child; c && j->argc < MAX_ARGS; c = c->next) if (c->type == PN_STRING) scopy(j->argv[j->argc++], c->str, sizeof j->argv[0]);
    if (!j->argc && j->prog[0]) scopy(j->argv[j->argc++], j->prog, sizeof j->argv[0]);
    if (!j->argc) { *why = "sem Program/ProgramArguments"; j->used = 0; j = 0; goto out; }
    pnode_t *ms = plist_get(root, "MachServices");
    if (ms && ms->type == PN_DICT)
        for (pnode_t *c = ms->child; c && j->nsvc < MAX_SVC; c = c->next) if (c->key && c->type != PN_FALSE) scopy(j->svc[j->nsvc++], c->key, sizeof j->svc[0]);
    pnode_t *ev = plist_get(root, "EnvironmentVariables");
    if (ev && ev->type == PN_DICT)
        for (pnode_t *c = ev->child; c && j->envc < MAX_ENV; c = c->next)
            if (c->key && c->type == PN_STRING) { cat2(j->env[j->envc], sizeof j->env[0], c->key, "="); cat2(j->env[j->envc], sizeof j->env[0], j->env[j->envc], c->str); j->envc++; }
    scopy(j->wd, plist_str(root, "WorkingDirectory"), sizeof j->wd);
    scopy(j->out, plist_str(root, "StandardOutPath"), sizeof j->out);
    scopy(j->err, plist_str(root, "StandardErrorPath"), sizeof j->err);
    j->keep = plist_bool(root, "KeepAlive", 0);
    j->runatload = plist_bool(root, "RunAtLoad", 0);
    *why = 0;
out:
    plist_free(root);
    return j;
}

static void scan_dir(const char *dir) {
    char name[128], path[320];
    for (int i = 0; readdir(dir, i, name, sizeof name) == 1; i++) {
        join(path, sizeof path, dir, name);
        if (ends(name, ".servico")) { lock(); load_legacy(path, name); unlock(); continue; }
        if (!ends(name, ".plist")) continue;
        const char *why;
        lock();
        job_t *j = load_plist(path, &why);
        if (j) reserve(j);
        unlock();
        if (!j) print("launchd: %s ignorado (%s)\n", path, why);
    }
}

/* ---------------------------------------------------------------- controle (launchctl) */
static char txt[16384];
static usize tn;
static void tput(const char *s) { while (*s && tn + 1 < sizeof txt) txt[tn++] = *s++; txt[tn] = 0; }
static void tnum(i64 v) { char b[24]; int n = 0; u64 u = v < 0 ? (u64)-v : (u64)v; do b[n++] = (char)('0' + u % 10); while (u /= 10); if (v < 0) b[n++] = '-'; while (n) { char c[2] = { b[--n], 0 }; tput(c); } }

static void stop_job(job_t *j) { if (j->pid > 0) sigkill(j->pid, MOS_SIGTERM); }

static void handle_ctl(unsigned char *msg, unsigned len) {
    mhdr_t *h = (mhdr_t *)msg;
    svc_body_t *b = svc_body(msg, len, 0);
    unsigned reply = (h->bits >> 8 & 0x1f) == D_MOVE_SO ? h->local : 0;
    if (!b || h->id != SVC_MSG_ID) { if (reply) svc_reply(reply, 22, 0, 0, 0, 0); return; }
    char s1[320], s2[320];
    scopy(s1, b->n1 ? svc_s1(b) : "", b->n1 + 1 < sizeof s1 ? b->n1 + 1 : sizeof s1);
    scopy(s2, b->n2 ? svc_s2(b) : "", b->n2 + 1 < sizeof s2 ? b->n2 + 1 : sizeof s2);
    tn = 0; txt[0] = 0;
    unsigned code = 0;
    lock();
    job_t *j = s1[0] ? job_find(s1) : 0;
    switch (b->op) {
    case LC_LIST:
        tput("PID\tStatus\tLabel\n");
        for (int i = 0; i < MAX_JOBS; i++) {
            if (!jobs[i].used) continue;
            if (jobs[i].pid) tnum(jobs[i].pid); else tput("-");
            tput("\t"); tnum(jobs[i].last_status); tput("\t"); tput(jobs[i].label); tput("\n");
        }
        break;
    case LC_LOAD: {
        const char *why;
        job_t *n = load_plist(s1, &why);
        if (!n) { code = 17; tput(why); break; }
        reserve(n);
        if (n->runatload || n->keep) start(n);
        break;
    }
    case LC_UNLOAD: case LC_REMOVE:
        if (!j) for (int i = 0; i < MAX_JOBS; i++) if (jobs[i].used && !strcmp(jobs[i].src, s1)) j = &jobs[i];
        if (!j) { code = 113; tput("job desconhecido"); break; }
        j->keep = 0;
        stop_job(j);
        release(j);
        j->used = 0;
        break;
    case LC_START:
        if (!j) { code = 113; tput("job desconhecido"); break; }
        if (!j->pid) start(j);
        break;
    case LC_STOP:
        if (!j) { code = 113; tput("job desconhecido"); break; }
        stop_job(j);
        break;
    case LC_SUBMIT:
        if (j) break;                                   /* já existe: o cliente só faz look_up de novo */
        if (!s1[0] || !s2[0]) { code = 22; break; }
        j = job_new(s1);
        if (!j) { code = 12; break; }
        scopy(j->src, s2, sizeof j->src);
        scopy(j->argv[0], s2, sizeof j->argv[0]);
        j->argc = 1;
        scopy(j->svc[0], s1, sizeof j->svc[0]);
        j->nsvc = 1;
        reserve(j);
        print("launchd: serviço sob demanda %s -> %s\n", s1, s2);
        break;
    case LC_PRINT:
        if (!j) { code = 113; tput("job desconhecido"); break; }
        tput(j->label); tput(" = {\n\tpath = "); tput(j->src);
        tput("\n\tprogram = "); tput(j->prog[0] ? j->prog : j->argc ? j->argv[0] : "");
        tput("\n\tstate = "); tput(j->pid ? "running" : "not running");
        tput("\n\tpid = "); tnum(j->pid);
        tput("\n\tlast exit code = "); tnum(j->last_status);
        tput("\n\tkeepalive = "); tput(j->keep ? "1" : "0");
        tput("\n\trunatload = "); tput(j->runatload ? "1" : "0");
        for (int i = 0; i < j->nsvc; i++) { tput("\n\tendpoint = "); tput(j->svc[i]); tput(j->port[i] ? "" : " (com o daemon)"); }
        tput("\n}\n");
        break;
    default:
        code = 78;
    }
    unlock();
    (void)s2;
    svc_reply(reply, code, 0, 0, txt, (unsigned)tn);
}

/* ---------------------------------------------------------------- filhos */
static int reaper(void *arg) {
    (void)arg;
    for (;;) {
        int code = 0;
        int pid = waitany(&code);
        if (pid < 0) { sleep_ms(200); continue; }
        lock();
        for (int i = 0; i < MAX_JOBS; i++) {
            job_t *j = &jobs[i];
            if (!j->used || j->pid != pid) continue;
            j->pid = 0;
            j->last_status = code;
            print("launchd: %s (pid %d) terminou com código %d\n", j->label, (i64)pid, (i64)code);
            u64 ran = time_ms() - j->last_start;
            if (ran < 10000) j->crashes++; else j->crashes = 0;
            if (j->keep) {
                if (ran < 10000 && ++j->restarts >= 5) { print("launchd: %s reiniciou demais, desistindo\n", j->label); j->keep = 0; }
                else j->restart_at = time_ms() + 1000;
            }
            break;
        }
        unlock();
    }
}

void _start(const char *arg) {
    (void)arg;
    pset = port_new_set();
    unsigned name = 0;
    if (bs_op(MACH_BS_CHECK_IN, LAUNCHD_SERVICE, &name) == 0 && name) { ctl = name; port_join(ctl, pset); }
    mkdir("/private"); mkdir("/private/var"); mkdir("/private/var/db"); mkdir("/private/var/log"); mkdir("/tmp");

    scan_dir(LEGACY_DIR);
    scan_dir("/System/Library/LaunchDaemons");
    scan_dir("/Library/LaunchDaemons");
    scan_dir("/Library/LaunchAgents");
    char user[128], dir[320];
    for (int i = 0; readdir("/Users", i, user, sizeof user) == 1; i++) {
        join(dir, sizeof dir, "/Users", user);
        char la[360];
        join(la, sizeof la, dir, "Library/LaunchAgents");
        scan_dir(la);
    }
    int n = 0;
    for (int i = 0; i < MAX_JOBS; i++) n += jobs[i].used;
    print("launchd: %d job(s)\n", (i64)n);

    lock();
    for (int i = 0; i < MAX_JOBS; i++) if (jobs[i].used && !jobs[i].delay_ms && (jobs[i].runatload || jobs[i].keep)) start(&jobs[i]);
    u64 t0 = time_ms();
    for (int i = 0; i < MAX_JOBS; i++) if (jobs[i].used && jobs[i].delay_ms) jobs[i].restart_at = t0 + (u64)jobs[i].delay_ms;
    unlock();
    thread_create(reaper, 0);

    static unsigned char msg[SVC_MAX_MSG + 128];
    for (;;) {
        port_wait(pset, 500);
        if (ctl) while (port_pending(ctl)) {
            memset(msg, 0, sizeof(mhdr_t));
            i64 r = mach_msg_n(msg, M_RCV | M_RCV_TO | M_TRAILER, 0, sizeof msg, ctl, 0);
            if (r) break;
            handle_ctl(msg, ((mhdr_t *)msg)->size);
        }
        int waiting = 0;
        lock();
        u64 now = time_ms();
        for (int i = 0; i < MAX_JOBS; i++) {
            job_t *j = &jobs[i];
            if (!j->used || j->pid) continue;
            if (j->restart_at && now >= j->restart_at) { start(j); continue; }
            if (wants_start(j)) {
                /* sob demanda; quem morre logo ao subir espera um pouco antes da próxima tentativa */
                if (j->crashes >= 10) continue;
                if (j->crashes && now - j->last_start < 1000u * (u64)j->crashes) { waiting = 1; continue; }
                start(j);
            }
        }
        /* algum job iniciado ainda não fez check_in: a mensagem continua no conjunto e o wait volta na hora */
        for (int i = 0; i < MAX_JOBS; i++) if (jobs[i].used && jobs[i].pid) for (int k = 0; k < jobs[i].nsvc; k++) if (jobs[i].port[k] && port_pending(jobs[i].port[k])) waiting = 1;
        unlock();
        if (waiting) sleep_ms(50);
    }
}
