/* mOS — launchctl(1): fala com o launchd do mOS (userland/init.c) pelo serviço Mach LAUNCHD_SERVICE.
 *   launchctl list                    PID, último código de saída e rótulo de cada job
 *   launchctl list RÓTULO             dicionário do job (formato do launchctl legado); 113 se não existe
 *   launchctl load [-w] ARQ.plist...  carrega (e inicia se RunAtLoad/KeepAlive; MachServices ficam sob demanda)
 *   launchctl unload [-w] ARQ.plist   para e descarrega
 *   launchctl start|stop RÓTULO
 *   launchctl remove RÓTULO
 *   launchctl print RÓTULO  (também aceita gui/501/RÓTULO e system/RÓTULO)
 *   launchctl bootstrap DOMÍNIO ARQ / bootout DOMÍNIO ARQ|RÓTULO / kickstart [-k] DOMÍNIO/RÓTULO / enable / disable */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include "../svcproto.h"

extern unsigned bootstrap_port;
int bootstrap_look_up(unsigned bp, const char *name, unsigned *port);
int mach_port_allocate(unsigned task, int right, unsigned *name);
int mach_port_mod_refs(unsigned task, unsigned name, int right, int delta);
int mach_msg(void *msg, int option, unsigned send_size, unsigned rcv_size, unsigned rcv_name, unsigned timeout, unsigned notify);
extern unsigned mach_task_self_;

typedef struct { unsigned bits, size, remote, local, voucher, id; } hdr_t;
static unsigned char buf[SVC_MAX_MSG + 256];

static int call(unsigned op, const char *arg, int quiet) {
    unsigned svc = 0, reply = 0;
    if (bootstrap_look_up(bootstrap_port, LAUNCHD_SERVICE, &svc) != 0 || !svc) { fprintf(stderr, "launchctl: launchd não responde\n"); return 1; }
    if (mach_port_allocate(mach_task_self_, 1, &reply) != 0) return 1;
    unsigned n1 = arg ? (unsigned)strlen(arg) + 1 : 0;
    unsigned size = (unsigned)(sizeof(hdr_t) + sizeof(svc_body_t) + n1 + 3) & ~3u;
    memset(buf, 0, size);
    hdr_t *h = (hdr_t *)buf;
    h->bits = 19 | (21u << 8);
    h->size = size;
    h->remote = svc;
    h->local = reply;
    h->id = SVC_MSG_ID;
    svc_body_t *b = (svc_body_t *)(h + 1);
    b->op = op;
    b->n1 = n1;
    if (n1) memcpy(b + 1, arg, n1);
    int kr = mach_msg(buf, 1 | 2 | 0x10 | 0x100, size, sizeof buf, reply, 15000, 0);
    mach_port_mod_refs(mach_task_self_, reply, 1, -1);
    if (kr) { fprintf(stderr, "launchctl: sem resposta do launchd (0x%x)\n", kr); return 1; }
    b = (svc_body_t *)(buf + sizeof(hdr_t));
    char *txt = (char *)(b + 1);
    if (b->n1 && !quiet) { fwrite(txt, 1, b->n1, b->op ? stderr : stdout); if (b->op && txt[b->n1 - 1] != '\n') fputc('\n', stderr); }
    return (int)b->op;
}
/* "gui/501/com.x" ou "system/com.x" -> "com.x" */
static const char *label_of(const char *s) {
    if (!strncmp(s, "gui/", 4) || !strncmp(s, "user/", 5) || !strncmp(s, "system/", 7) || !strncmp(s, "login/", 6)) {
        const char *l = strrchr(s, '/');
        return l ? l + 1 : s;
    }
    return s;
}
static const char *abs_path(const char *p, char *out) {
    if (p[0] == '/') return p;
    char cwd[PATH_MAX];
    if (!getcwd(cwd, sizeof cwd)) return p;
    snprintf(out, PATH_MAX, "%s/%s", cwd, p);
    return out;
}
static int usage(void) {
    fprintf(stderr, "uso: launchctl list | load ARQ | unload ARQ | start RÓTULO | stop RÓTULO | remove RÓTULO | print RÓTULO\n"
                    "     | bootstrap DOMÍNIO ARQ | bootout DOMÍNIO [ARQ|RÓTULO] | kickstart [-k] DOMÍNIO/RÓTULO\n");
    return 64;
}
int main(int argc, char **argv) {
    if (argc < 2) return usage();
    const char *c = argv[1];
    char tmp[PATH_MAX];
    int r = 0;
    if (!strcmp(c, "list")) return call(LC_LIST, argc > 2 ? label_of(argv[2]) : 0, 0);
    if (!strcmp(c, "load") || !strcmp(c, "unload") || !strcmp(c, "bootstrap") || !strcmp(c, "bootout")) {
        int load = !strcmp(c, "load") || !strcmp(c, "bootstrap"), first = 2, any = 0;
        if (!strcmp(c, "bootstrap") || !strcmp(c, "bootout")) first = 3;
        for (int i = first; i < argc; i++) {
            if (argv[i][0] == '-') continue;          /* -w, -F, -S: sem efeito aqui */
            any = 1;
            const char *p = argv[i];
            if (strstr(p, ".plist")) p = abs_path(p, tmp); else p = label_of(p);
            int e = call(load ? LC_LOAD : LC_UNLOAD, p, 0);
            if (e) r = e;
        }
        if (!any && !strcmp(c, "bootout") && argc > 2) return call(LC_UNLOAD, label_of(argv[2]), 0);
        return any || !load ? r : usage();
    }
    if (argc < 3) return usage();
    if (!strcmp(c, "start")) return call(LC_START, label_of(argv[2]), 0);
    if (!strcmp(c, "stop")) return call(LC_STOP, label_of(argv[2]), 0);
    if (!strcmp(c, "remove")) return call(LC_REMOVE, label_of(argv[2]), 0);
    if (!strcmp(c, "print")) return call(LC_PRINT, label_of(argv[2]), 0);
    if (!strcmp(c, "enable") || !strcmp(c, "disable")) return 0;
    if (!strcmp(c, "kickstart")) {
        int kill = 0;
        const char *t = 0;
        for (int i = 2; i < argc; i++) { if (!strcmp(argv[i], "-k")) kill = 1; else if (argv[i][0] != '-') t = argv[i]; }
        if (!t) return usage();
        if (kill) call(LC_STOP, label_of(t), 1);
        if (kill) usleep(300000);
        return call(LC_START, label_of(t), 0);
    }
    return usage();
}
