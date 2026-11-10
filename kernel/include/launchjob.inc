/* Regras de launchd.plist(5) que o launchd do mOS (userland/init.c) aplica a cada job:
 * KeepAlive (booleano ou dicionário com SuccessfulExit / Crashed), StartInterval, ThrottleInterval e a
 * resposta de "launchctl list RÓTULO". Sem string.h e sem alocação: quem inclui já tem os tipos u64/i64.
 * LJ_PLIST: o leitor de plist (plistmini.h) já foi incluído e lj_read_plist fica disponível.
 *
 * Código de saída: >= 0 é exit(status); < 0 é morte por sinal (-sig), o que o kernel grava em finish(). */

enum {
    LJ_KEEP_ALWAYS  = 1,   /* KeepAlive = true */
    LJ_KEEP_SUCCESS = 2,   /* KeepAlive.SuccessfulExit = true: reinicia enquanto sair com 0 */
    LJ_KEEP_FAILURE = 4,   /* KeepAlive.SuccessfulExit = false: reinicia enquanto sair com status != 0 */
    LJ_KEEP_CRASH   = 8    /* KeepAlive.Crashed = true: reinicia quando morre por sinal */
};

/* ThrottleInterval ausente: o mOS espera 1 s entre um fim e o próximo início (o launchd da Apple usa 10 s). */
#define LJ_THROTTLE_DEFAULT_MS 1000ull

/* reinicia depois de terminar com este código? */
static int lj_keep_restart(unsigned keep, int code) {
    if (keep & LJ_KEEP_ALWAYS) return 1;
    if (code < 0) return (keep & LJ_KEEP_CRASH) != 0;
    if (code == 0) return (keep & LJ_KEEP_SUCCESS) != 0;
    return (keep & LJ_KEEP_FAILURE) != 0;
}

/* launchd.plist(5): SuccessfulExit implica RunAtLoad (precisa de uma primeira saída para valer).
 * Crashed sozinho não inicia o job. */
static int lj_keep_runatload(unsigned keep) {
    return (keep & (LJ_KEEP_ALWAYS | LJ_KEEP_SUCCESS | LJ_KEEP_FAILURE)) != 0;
}

/* espera antes de reiniciar: ThrottleInterval conta do último início, não do fim */
static u64 lj_restart_delay_ms(unsigned throttle_s, u64 ran_ms) {
    if (!throttle_s) return LJ_THROTTLE_DEFAULT_MS;
    u64 t = (u64)throttle_s * 1000ull;
    return ran_ms >= t ? 0 : t - ran_ms;
}

/* StartInterval: *next é o próximo disparo (0 = ainda não armado). Devolve 1 quando é hora de iniciar.
 * Se o job ainda roda no disparo, o launchd junta os disparos: este é pulado e o próximo fica armado. */
static int lj_interval_tick(u64 now, u64 *next, unsigned interval_s, int running) {
    if (!interval_s || !next) return 0;
    u64 step = (u64)interval_s * 1000ull;
    if (!*next) { *next = now + step; return 0; }
    if (now < *next) return 0;
    *next = now + step;
    return !running;
}

/* "launchctl list RÓTULO" do macOS: um dicionário no formato do launchctl legado. pid 0 = parado
 * (a chave PID some). Devolve o tamanho escrito (sem o 0 final) ou -1 se não coube. */
static int lj_put(char *d, int cap, int *o, const char *s) {
    while (*s) { if (*o + 1 >= cap) return -1; d[(*o)++] = *s++; }
    d[*o] = 0;
    return 0;
}
static int lj_put_int(char *d, int cap, int *o, i64 v) {
    char b[24];
    int n = 0;
    u64 u = v < 0 ? (u64)(-(v + 1)) + 1 : (u64)v;
    do b[n++] = (char)('0' + u % 10); while (u /= 10);
    if (v < 0) b[n++] = '-';
    char r[25];
    int k = 0;
    while (n) r[k++] = b[--n];
    r[k] = 0;
    return lj_put(d, cap, o, r);
}
static int lj_fmt_list_one(char *d, int cap, const char *label, const char *program, int pid, int last_status,
                           const char *const *svcs, int nsvc) {
    int o = 0;
    if (!d || cap < 1) return -1;
    d[0] = 0;
    if (lj_put(d, cap, &o, "{\n\t\"LimitLoadToSessionType\" = \"Aqua\";\n\t\"Label\" = \"")) return -1;
    if (lj_put(d, cap, &o, label ? label : "")) return -1;
    if (lj_put(d, cap, &o, "\";\n\t\"OnDemand\" = true;\n\t\"LastExitStatus\" = ")) return -1;
    if (lj_put_int(d, cap, &o, last_status)) return -1;
    if (lj_put(d, cap, &o, ";\n")) return -1;
    if (pid > 0) {
        if (lj_put(d, cap, &o, "\t\"PID\" = ")) return -1;
        if (lj_put_int(d, cap, &o, pid)) return -1;
        if (lj_put(d, cap, &o, ";\n")) return -1;
    }
    if (program && *program) {
        if (lj_put(d, cap, &o, "\t\"Program\" = \"")) return -1;
        if (lj_put(d, cap, &o, program)) return -1;
        if (lj_put(d, cap, &o, "\";\n")) return -1;
    }
    if (nsvc > 0) {
        if (lj_put(d, cap, &o, "\t\"MachServices\" = {\n")) return -1;
        for (int i = 0; i < nsvc; i++) {
            if (lj_put(d, cap, &o, "\t\t\"")) return -1;
            if (lj_put(d, cap, &o, svcs[i])) return -1;
            if (lj_put(d, cap, &o, "\" = mach-port-object;\n")) return -1;
        }
        if (lj_put(d, cap, &o, "\t};\n")) return -1;
    }
    if (lj_put(d, cap, &o, "};\n")) return -1;
    return o;
}

#ifdef LJ_PLIST
typedef struct {
    unsigned keep;          /* LJ_KEEP_* */
    int runatload;          /* RunAtLoad explícito, ou implícito pelo KeepAlive */
    unsigned interval_s;    /* StartInterval */
    unsigned throttle_s;    /* ThrottleInterval (0 = padrão do mOS) */
} lj_policy_t;

static unsigned lj_uint(pnode_t *n) {
    if (!n || n->type != PN_INT || n->num <= 0) return 0;
    return n->num > 0x7fffffff ? 0x7fffffffu : (unsigned)n->num;
}

static void lj_read_plist(pnode_t *root, lj_policy_t *p) {
    p->keep = 0;
    p->interval_s = lj_uint(plist_get(root, "StartInterval"));
    p->throttle_s = lj_uint(plist_get(root, "ThrottleInterval"));
    pnode_t *ka = plist_get(root, "KeepAlive");
    if (ka && ka->type == PN_DICT) {
        pnode_t *se = plist_get(ka, "SuccessfulExit");
        if (se && se->type == PN_TRUE) p->keep |= LJ_KEEP_SUCCESS;
        if (se && se->type == PN_FALSE) p->keep |= LJ_KEEP_FAILURE;
        pnode_t *cr = plist_get(ka, "Crashed");
        if (cr && cr->type == PN_TRUE) p->keep |= LJ_KEEP_CRASH;
        /* NetworkState, PathState, OtherJobEnabled: condições que o mOS não observa; o job continua vivo */
        for (pnode_t *c = ka->child; c; c = c->next)
            if (c->key && strcmp(c->key, "SuccessfulExit") && strcmp(c->key, "Crashed") && strcmp(c->key, "AfterInitialDemand"))
                p->keep |= LJ_KEEP_ALWAYS;
    } else if (plist_bool(root, "KeepAlive", 0)) {
        p->keep = LJ_KEEP_ALWAYS;
    }
    pnode_t *ral = plist_get(root, "RunAtLoad");
    p->runatload = (ral && (ral->type == PN_TRUE || (ral->type == PN_INT && ral->num))) || lj_keep_runatload(p->keep);
}
#endif
