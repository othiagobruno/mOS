/* ServiceManagement (SMJob*, SMCopyAllJobDictionaries): leitura das respostas do launchd do mOS e os códigos de
 * erro do domínio kSMErrorDomainLaunchd. Sem alocação. Incluído por smjob.c e por tools/smjobtest.c.
 *
 * "launchctl list RÓTULO" (LC_LIST com rótulo) devolve o dicionário de kernel/include/launchjob.inc:
 *     "Label" = "x"; "LastExitStatus" = n; "PID" = n; "Program" = "p";
 * LC_LIST sem rótulo devolve a tabela "PID\tStatus\tLabel", com "-" no PID de quem está parado. */

typedef struct {
    char label[200];
    char program[400];
    int pid;               /* 0 = parado */
    int last_status;
} smj_info_t;

/* códigos que o launchd (userland/init.c) devolve e o errno que o SMJob* põe no CFError, como o launchd da Apple */
#define SMJ_LAUNCHD_EXISTS   17    /* já carregado -> EEXIST */
#define SMJ_LAUNCHD_NOTFOUND 113   /* job desconhecido -> ESRCH */
static long smj_errno_of(int code) {
    if (code == SMJ_LAUNCHD_EXISTS) return 17;    /* EEXIST */
    if (code == SMJ_LAUNCHD_NOTFOUND) return 3;   /* ESRCH */
    if (code < 0) return 60;                      /* sem resposta do launchd: ETIMEDOUT */
    return 22;                                    /* EINVAL: plist recusado */
}

static int smj_copy(char *d, int cap, const char *s, int n) {
    int i = 0;
    if (cap < 1) return -1;
    while (i < n && i + 1 < cap) { d[i] = s[i]; i++; }
    d[i] = 0;
    return i == n ? 0 : -1;
}
static int smj_num(const char *s, int *out) {
    int neg = 0, any = 0;
    long v = 0;
    while (*s == ' ' || *s == '\t') s++;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') { v = v * 10 + (*s - '0'); if (v > 0x7fffffffL) return -1; s++; any = 1; }
    if (!any) return -1;
    *out = (int)(neg ? -v : v);
    return 0;
}

/* procura '"Chave" = ' no texto; devolve o começo do valor */
static const char *smj_find(const char *t, const char *key) {
    int kl = 0;
    while (key[kl]) kl++;
    for (const char *p = t; *p; p++) {
        if (*p != '"') continue;
        int i = 0;
        while (i < kl && p[1 + i] == key[i]) i++;
        if (i == kl && p[1 + kl] == '"' && p[2 + kl] == ' ' && p[3 + kl] == '=' && p[4 + kl] == ' ') return p + 5 + kl;
    }
    return 0;
}
static int smj_str_val(const char *v, char *out, int cap) {
    if (!v || *v != '"') return -1;
    v++;
    int n = 0;
    while (v[n] && v[n] != '"') n++;
    if (v[n] != '"') return -1;
    return smj_copy(out, cap, v, n);
}

/* 0 = ok; -1 = resposta sem Label */
static int smj_parse_one(const char *t, smj_info_t *o) {
    o->label[0] = o->program[0] = 0;
    o->pid = 0;
    o->last_status = 0;
    if (!t || smj_str_val(smj_find(t, "Label"), o->label, (int)sizeof o->label) || !o->label[0]) return -1;
    const char *v = smj_find(t, "PID");
    if (v && smj_num(v, &o->pid)) o->pid = 0;
    v = smj_find(t, "LastExitStatus");
    if (v && smj_num(v, &o->last_status)) o->last_status = 0;
    v = smj_find(t, "Program");
    if (v) smj_str_val(v, o->program, (int)sizeof o->program);
    return 0;
}

/* próxima linha da tabela de LC_LIST; *cur avança. 1 = linha lida, 0 = fim. O cabeçalho é pulado. */
static int smj_parse_row(const char **cur, smj_info_t *o) {
    const char *p = *cur;
    while (p && *p) {
        const char *e = p;
        while (*e && *e != '\n') e++;
        const char *t1 = p;
        while (t1 < e && *t1 != '\t') t1++;
        const char *t2 = t1 < e ? t1 + 1 : e;
        while (t2 < e && *t2 != '\t') t2++;
        const char *next = *e ? e + 1 : e;
        if (t1 < e && t2 < e && !(t1 - p == 3 && p[0] == 'P' && p[1] == 'I' && p[2] == 'D')) {
            char num[24];
            o->program[0] = 0;
            o->pid = 0;
            o->last_status = 0;
            if (!(t1 - p == 1 && *p == '-') && !smj_copy(num, (int)sizeof num, p, (int)(t1 - p))) smj_num(num, &o->pid);
            if (!smj_copy(num, (int)sizeof num, t1 + 1, (int)(t2 - t1 - 1))) smj_num(num, &o->last_status);
            if (!smj_copy(o->label, (int)sizeof o->label, t2 + 1, (int)(e - t2 - 1)) && o->label[0]) { *cur = next; return 1; }
        }
        p = next;
    }
    *cur = p;
    return 0;
}

/* nome do helper de login item: <bundle>/Contents/Library/LoginItems/<nome> */
static int smj_loginitems_dir(const char *bundle, char *out, int cap) {
    const char *suf = "/Contents/Library/LoginItems";
    int n = 0, s = 0;
    if (!bundle || bundle[0] != '/' || !out) return -1;
    while (bundle[n]) n++;
    while (suf[s]) s++;
    while (n > 1 && bundle[n - 1] == '/') n--;
    if (n + s + 1 > cap) return -1;
    for (int i = 0; i < n; i++) out[i] = bundle[i];
    for (int j = 0; j < s; j++) out[n + j] = suf[j];
    out[n + s] = 0;
    return 0;
}
