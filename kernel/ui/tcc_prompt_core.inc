/* Política do diálogo de permissão da Interface (gravação de tela e acessibilidade).
 * Não abre arquivo e não desenha: quem inclui lê decisions.txt, mostra o diálogo e
 * grava a escolha. O miolo de cx_tcc_decide (tcc.c) é tcc_decide; este arquivo só
 * diz qual pedido vira diálogo e qual botão vira granted 1 ou 0.
 *
 * Estados iguais aos de tcc_core.inc: 0 ausente, 1 denied, 2 granted, 3 pending.
 * cx_tcc_state devolve 3 quando o prompt deve aparecer.
 *
 * O incluinte fornece strlen, strcmp, strncmp, strstr, strrchr, snprintf, memcpy.
 */
enum {
    TCC_PROMPT_MISSING = 0,
    TCC_PROMPT_DENIED = 1,
    TCC_PROMPT_GRANTED = 2,
    TCC_PROMPT_PENDING = 3
};
enum { TCC_PROMPT_KIND_NONE = 0, TCC_PROMPT_KIND_SCREEN = 1, TCC_PROMPT_KIND_AX = 2 };
enum { TCC_PROMPT_SAME = 0, TCC_PROMPT_OPENED = 1, TCC_PROMPT_CLOSED = 2 };
enum { TCC_PROMPT_KEY_ENTER = 10, TCC_PROMPT_KEY_ESC = 0x1b };
enum { TCC_PROMPT_CAP = 64 };

/* Casa que NSHomeDirectory grava hoje (foundation.c). O diálogo lê este caminho,
 * não account_home(): o libsystem não segue a conta da sessão. */
#define TCC_PROMPT_DIR_HOME "/Users/Visitante"
#define TCC_PROMPT_FILE_SUFFIX "/Library/Application Support/com.mos.tcc/decisions.txt"
#define TCC_PROMPT_FILE TCC_PROMPT_DIR_HOME TCC_PROMPT_FILE_SUFFIX
#define TCC_PROMPT_SCREEN "kTCCServiceScreenCapture"
#define TCC_PROMPT_AX "kTCCServiceAccessibility"
#define TCC_PROMPT_EYEBROW "mOS"
#define TCC_PROMPT_DENY "Negar"
#define TCC_PROMPT_ALLOW "Permitir"

typedef struct {
    char bundle[184];
    char service[184];
    int state;
} tcc_prompt_rec;

typedef struct {
    int kind;
    char bundle[184];
    char service[184];
    char app[184];
    char eyebrow[32];
    char title[320];
    char message[640];
    char deny[24];
    char allow[24];
} tcc_prompt_view;

typedef struct {
    int open;
    int hover;
    int pressed;
    tcc_prompt_view view;
} tcc_prompt_ui;

typedef int (*tcc_prompt_decider)(const char *bundle, const char *service, int granted, void *ctx);

static int tcc_prompt_tok_ok(const char *s) {
    if (!s || !s[0] || strlen(s) > 180) return 0;
    if (strstr(s, "..")) return 0;
    for (const char *p = s; *p; p++) {
        char c = *p;
        int ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
                 c == '.' || c == '_' || c == '-' || c == '+';
        if (!ok) return 0;
    }
    return 1;
}

static int tcc_prompt_kind(const char *service) {
    if (service && !strcmp(service, TCC_PROMPT_SCREEN)) return TCC_PROMPT_KIND_SCREEN;
    if (service && !strcmp(service, TCC_PROMPT_AX)) return TCC_PROMPT_KIND_AX;
    return TCC_PROMPT_KIND_NONE;
}

static void tcc_prompt_app_name(char *out, int cap, const char *bundle) {
    const char *dot = strrchr(bundle, '.');
    const char *s = (dot && dot[1]) ? dot + 1 : bundle;
    int generic = !strcmp(s, "app") || !strcmp(s, "mos") || strlen(s) < 2;
    if (generic) {
        snprintf(out, cap, "%s", bundle);
        return;
    }
    snprintf(out, cap, "%s", s);
    if (out[0] >= 'a' && out[0] <= 'z') out[0] = (char)(out[0] - 32);
}

static int tcc_prompt_fill(tcc_prompt_view *v, const char *bundle, const char *service) {
    char b[184], s[184], app[184];
    int kind;
    if (!v) return -1;
    snprintf(b, sizeof b, "%s", bundle ? bundle : "");
    snprintf(s, sizeof s, "%s", service ? service : "");
    kind = tcc_prompt_kind(s);
    memset(v, 0, sizeof *v);
    if (!kind || !tcc_prompt_tok_ok(b)) return -1;
    tcc_prompt_app_name(app, sizeof app, b);
    v->kind = kind;
    snprintf(v->bundle, sizeof v->bundle, "%s", b);
    snprintf(v->service, sizeof v->service, "%s", s);
    snprintf(v->app, sizeof v->app, "%s", app);
    snprintf(v->eyebrow, sizeof v->eyebrow, "%s", TCC_PROMPT_EYEBROW);
    snprintf(v->deny, sizeof v->deny, "%s", TCC_PROMPT_DENY);
    snprintf(v->allow, sizeof v->allow, "%s", TCC_PROMPT_ALLOW);
    if (kind == TCC_PROMPT_KIND_SCREEN) {
        snprintf(v->title, sizeof v->title, "%s quer gravar a tela do mOS", app);
        snprintf(v->message, sizeof v->message,
                 "O pedido de %s está pendente, então a imagem da tela não sai. "
                 "Permitir guarda a concessão neste computador. Negar deixa a captura fechada.",
                 b);
    } else {
        snprintf(v->title, sizeof v->title, "%s quer controlar a interface do mOS", app);
        snprintf(v->message, sizeof v->message,
                 "O pedido de %s está pendente, então a árvore da interface continua fechada. "
                 "Permitir guarda a concessão neste computador. Negar deixa a acessibilidade bloqueada.",
                 b);
    }
    return 0;
}

static int tcc_prompt_take(const char **pp, char *out, int cap) {
    const char *p = *pp;
    int n = 0;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == 0 || *p == '\n' || *p == '\r' || *p == '#') return 0;
    while (*p && *p != ' ' && *p != '\t' && *p != '\n' && *p != '\r') {
        if (n >= cap - 1) return -1;
        out[n++] = *p++;
    }
    out[n] = 0;
    *pp = p;
    return 1;
}

static int tcc_prompt_parse(const char *text, tcc_prompt_rec *recs, int cap) {
    const char *p = text ? text : "";
    int n = 0;
    while (*p && n < cap) {
        const char *line = p;
        char b[184], s[184], st[32];
        int tb, ts, tt, state;
        while (*p && *p != '\n') p++;
        if (*p == '\n') p++;
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        tb = tcc_prompt_take(&line, b, sizeof b);
        ts = tb == 1 ? tcc_prompt_take(&line, s, sizeof s) : 0;
        tt = ts == 1 ? tcc_prompt_take(&line, st, sizeof st) : 0;
        if (tb != 1 || ts != 1 || tt != 1) continue;
        if (!tcc_prompt_tok_ok(b) || !tcc_prompt_tok_ok(s)) continue;
        if (!strcmp(st, "granted")) state = TCC_PROMPT_GRANTED;
        else if (!strcmp(st, "denied")) state = TCC_PROMPT_DENIED;
        else if (!strcmp(st, "pending")) state = TCC_PROMPT_PENDING;
        else continue;
        snprintf(recs[n].bundle, sizeof recs[n].bundle, "%s", b);
        snprintf(recs[n].service, sizeof recs[n].service, "%s", s);
        recs[n].state = state;
        n++;
    }
    return n;
}

static int tcc_prompt_format(char *out, int cap, const tcc_prompt_rec *recs, int n) {
    int used = 0;
    if (!out || cap <= 0) return -1;
    out[0] = 0;
    for (int i = 0; i < n; i++) {
        const char *st = recs[i].state == TCC_PROMPT_GRANTED ? "granted"
                       : recs[i].state == TCC_PROMPT_DENIED ? "denied"
                       : recs[i].state == TCC_PROMPT_PENDING ? "pending" : 0;
        char line[400];
        int m;
        if (!st || !tcc_prompt_tok_ok(recs[i].bundle) || !tcc_prompt_tok_ok(recs[i].service)) continue;
        m = snprintf(line, sizeof line, "%s %s %s\n", recs[i].bundle, recs[i].service, st);
        if (m < 0 || m >= (int)sizeof line || used + m >= cap) return -1;
        memcpy(out + used, line, m);
        used += m;
        out[used] = 0;
    }
    return used;
}

static int tcc_prompt_find(const tcc_prompt_rec *recs, int n, const char *bundle, const char *service) {
    for (int i = 0; i < n; i++)
        if (!strcmp(recs[i].bundle, bundle) && !strcmp(recs[i].service, service)) return i;
    return -1;
}

/* granted != 0 concede. Devolve 0 se a tabela ficou válida, -1 se o nome não serve ou não cabe. */
static int tcc_prompt_set(tcc_prompt_rec *recs, int *n, int cap, const char *bundle, const char *service, int granted) {
    int i;
    if (!recs || !n || !tcc_prompt_tok_ok(bundle) || !tcc_prompt_tok_ok(service)) return -1;
    i = tcc_prompt_find(recs, *n, bundle, service);
    if (i < 0) {
        if (*n >= cap) return -1;
        i = (*n)++;
        snprintf(recs[i].bundle, sizeof recs[i].bundle, "%s", bundle);
        snprintf(recs[i].service, sizeof recs[i].service, "%s", service);
    }
    recs[i].state = granted ? TCC_PROMPT_GRANTED : TCC_PROMPT_DENIED;
    return 0;
}

static int tcc_prompt_sync(tcc_prompt_ui *ui, const tcc_prompt_rec *recs, int n) {
    if (!ui) return TCC_PROMPT_SAME;
    if (ui->open && tcc_prompt_kind(ui->view.service)) {
        int i = tcc_prompt_find(recs, n, ui->view.bundle, ui->view.service);
        if (i >= 0 && recs[i].state == TCC_PROMPT_PENDING) {
            tcc_prompt_fill(&ui->view, ui->view.bundle, ui->view.service);
            return TCC_PROMPT_SAME;
        }
    }
    for (int i = 0; i < n; i++) {
        int changed;
        if (recs[i].state != TCC_PROMPT_PENDING || !tcc_prompt_kind(recs[i].service)) continue;
        changed = !ui->open || strcmp(ui->view.bundle, recs[i].bundle) || strcmp(ui->view.service, recs[i].service);
        tcc_prompt_fill(&ui->view, recs[i].bundle, recs[i].service);
        ui->open = 1;
        if (changed) {
            ui->hover = -1;
            ui->pressed = -1;
            return TCC_PROMPT_OPENED;
        }
        return TCC_PROMPT_SAME;
    }
    if (ui->open) {
        ui->open = 0;
        ui->hover = -1;
        ui->pressed = -1;
        return TCC_PROMPT_CLOSED;
    }
    return TCC_PROMPT_SAME;
}

/* 0 negar, 1 permitir, -1 botão que não existe. */
static int tcc_prompt_granted(int button) {
    if (button == 0) return 0;
    if (button == 1) return 1;
    return -1;
}

static int tcc_prompt_button_for_key(const tcc_prompt_ui *ui, int key) {
    if (!ui || !ui->open) return -1;
    if (key == TCC_PROMPT_KEY_ENTER) return 1;
    if (key == TCC_PROMPT_KEY_ESC) return 0;
    return -1;
}

/* decide é cx_tcc_decide no processo do libsystem, ou a gravação equivalente no window server.
 * Se decide devolve diferente de 0, o diálogo continua aberto. */
static int tcc_prompt_choose(tcc_prompt_ui *ui, int button, tcc_prompt_decider decide, void *ctx) {
    char bundle[184], service[184];
    int g, rc;
    if (!ui || !ui->open || !decide) return -1;
    g = tcc_prompt_granted(button);
    if (g < 0) return -1;
    snprintf(bundle, sizeof bundle, "%s", ui->view.bundle);
    snprintf(service, sizeof service, "%s", ui->view.service);
    rc = decide(bundle, service, g, ctx);
    if (rc != 0) return rc;
    ui->open = 0;
    ui->hover = -1;
    ui->pressed = -1;
    return 0;
}
