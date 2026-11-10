/* mOS — contas de usuário: ver include/account.h */
#include "account.h"
#include "vfs.h"
#include "crypto.h"
#include "sysstate.h"

#define DIR        "/Sistema/Biblioteca/Usuarios"
#define ITERATIONS 20000                 /* formato 1 (legado): SHA-256 iterado artesanal; só VERIFICA, nunca cria */
/* Formato 2: PBKDF2-HMAC-SHA256 com sal de 16 bytes do CSPRNG e número de iterações CALIBRADO neste hardware
 * (nunca pedido ao usuário), gravado em "Iteracoes". O piso vale para contas novas; contas já gravadas usam o que
 * está no arquivo. Esses valores são decisão de engenharia, não uma garantia de segurança contra ataque offline. */
#define MIN_ITERS  100000u
#define MAX_ITERS  2000000u
#define TARGET_NS  250000000ull

static char cur_name[32] = ACCOUNT_DEFAULT, cur_full[64] = ACCOUNT_DEFAULT, cur_home[64] = "/Users/" ACCOUNT_DEFAULT;

typedef struct { char full[64]; bool admin; char salt[33]; char hash[65]; int fmt; u32 iters; } acct_t;

static void hex(const u8 *b, int n, char *out) {
    static const char d[] = "0123456789abcdef";
    for (int i = 0; i < n; i++) { out[i * 2] = d[b[i] >> 4]; out[i * 2 + 1] = d[b[i] & 15]; }
    out[n * 2] = 0;
}

/* H0 = SHA256(sal || senha); Hi = SHA256(H(i-1) || sal || senha) — custo proposital contra força bruta */
static void derive_v1(const char *salt, const char *pw, char out[65]) {
    u8 h[32];
    sha256_ctx c;
    sha256_init(&c);
    sha256_update(&c, salt, strlen(salt));
    sha256_update(&c, pw, strlen(pw));
    sha256_final(&c, h);
    for (int i = 1; i < ITERATIONS; i++) {
        sha256_init(&c);
        sha256_update(&c, h, 32);
        sha256_update(&c, salt, strlen(salt));
        sha256_update(&c, pw, strlen(pw));
        sha256_final(&c, h);
    }
    hex(h, 32, out);
}

static int unhex(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
static int hex2bin(const char *h, u8 *out, int max) {
    int n = 0;
    for (; h[0] && h[1] && n < max; h += 2, n++) {
        int a = unhex(h[0]), b = unhex(h[1]);
        if (a < 0 || b < 0) return -1;
        out[n] = (u8)(a << 4 | b);
    }
    return n;
}
static void derive_v2(const char *salt_hex, const char *pw, u32 iters, char out[65]) {
    u8 salt[16], dk[32];
    int sl = hex2bin(salt_hex, salt, 16);
    if (sl != 16) { memset(out, '0', 64); out[64] = 0; return; }       /* sal inválido: nunca casa com um hash real */
    pbkdf2_hmac_sha256((const u8 *)pw, strlen(pw), salt, 16, iters, dk, 32);
    hex(dk, 32, out);
    secure_zero(dk, sizeof dk);
}

static u64 now_ns(void) {
    u64 h = hpet_ns();
    return h ? h : timer_ticks() * 1000000ull;
}

/* mede o hardware: iterações que cabem em ~TARGET_NS, entre MIN_ITERS e MAX_ITERS */
u32 account_kdf_iterations(void) {
    static u32 cached;
    if (cached) return cached;
    u8 salt[16] = {0}, dk[32];
    u32 probe = 4000;
    u64 t0 = now_ns();
    pbkdf2_hmac_sha256((const u8 *)"calibracao", 10, salt, 16, probe, dk, 32);
    u64 dt = now_ns() - t0;
    u64 it = dt ? (u64)probe * TARGET_NS / dt : MIN_ITERS;
    if (it < MIN_ITERS) it = MIN_ITERS;
    if (it > MAX_ITERS) it = MAX_ITERS;
    cached = (u32)(it / 1000 * 1000);
    kprintf("contas: PBKDF2 calibrado: %u iterações (%llu us por 4000 iterações; alvo %llu ms, piso %u)\n", cached, dt / 1000,
            TARGET_NS / 1000000, MIN_ITERS);
    return cached;
}

bool account_password_ok(const char *pw, char *err, usize errlen) {
    int n = 0;
    for (const u8 *p = (const u8 *)pw; p && *p; p++) if ((*p & 0xC0) != 0x80) n++;   /* caracteres UTF-8 */
    if (n < 8) { if (err) snprintf(err, errlen, "A senha precisa ter pelo menos 8 caracteres."); return false; }
    if (n > 60) { if (err) snprintf(err, errlen, "A senha pode ter no máximo 60 caracteres."); return false; }
    return true;
}

static bool valid_name(const char *n) {
    usize l = strlen(n);
    if (!l || l > 31 || !strcasecmp(n, "Shared") || !strcasecmp(n, "root") || !strcasecmp(n, ACCOUNT_DEFAULT) || n[0] == '.' || n[0] == '-') return false;
    for (const char *p = n; *p; p++)
        if (!(isalnum((u8)*p) || *p == '.' || *p == '_' || *p == '-')) return false;
    return true;
}

static void path_of(const char *name, char *out, usize cap) { snprintf(out, cap, DIR "/%s.conta", name); }

static void field(const char *txt, const char *key, char *out, usize cap) {
    usize kl = strlen(key);
    out[0] = 0;
    for (const char *l = txt; l && *l;) {
        if (!strncmp(l, key, kl) && l[kl] == '=') {
            const char *v = l + kl + 1;
            usize n = 0;
            while (v[n] && v[n] != '\n' && v[n] != '\r' && n + 1 < cap) { out[n] = v[n]; n++; }
            out[n] = 0;
            return;
        }
        l = strchr(l, '\n');
        if (l) l++;
    }
}

static bool load(const char *name, acct_t *a) {
    memset(a, 0, sizeof *a);
    if (!strcmp(name, ACCOUNT_DEFAULT) && !sysstate_guest_allowed()) return false;   /* Visitante só no live efêmero */
    char p[96];
    path_of(name, p, sizeof p);
    vnode_t *v = vfs_lookup(p);
    if (!v || v->dir || !v->data) {
        if (!strcmp(name, ACCOUNT_DEFAULT)) {          /* conta padrão implícita */
            strcpy(a->full, ACCOUNT_DEFAULT);
            a->admin = true;
            return true;
        }
        return false;
    }
    char *txt = kmalloc(v->size + 1);
    if (!txt) return false;
    memcpy(txt, v->data, v->size);
    txt[v->size] = 0;
    char adm[8], fm[8], it[16];
    field(txt, "Formato", fm, sizeof fm);
    field(txt, "Iteracoes", it, sizeof it);
    a->fmt = fm[0] == '2' ? 2 : 1;
    a->iters = a->fmt == 2 ? (u32)atoi(it) : ITERATIONS;
    if (a->fmt == 2 && (a->iters < 1000 || a->iters > 50000000u)) a->iters = 0;   /* inválido: nunca verifica */
    field(txt, "NomeCompleto", a->full, sizeof a->full);
    field(txt, "Admin", adm, sizeof adm);
    field(txt, "Sal", a->salt, sizeof a->salt);
    field(txt, "Hash", a->hash, sizeof a->hash);
    a->admin = adm[0] == '1';
    if (!a->full[0]) strncpy(a->full, name, sizeof a->full - 1);
    kfree(txt);
    return true;
}

static bool save(const char *name, const acct_t *a) {
    char p[96], txt[400];
    path_of(name, p, sizeof p);
    vfs_mkdir(DIR);
    int n = snprintf(txt, sizeof txt, "NomeCompleto=%s\nAdmin=%d\nFormato=%d\nAlgoritmo=%s\nSal=%s\nHash=%s\nIteracoes=%u\n",
                     a->full, a->admin ? 1 : 0, a->hash[0] ? 2 : a->fmt, a->hash[0] ? "PBKDF2-HMAC-SHA256" : "nenhum", a->salt, a->hash,
                     a->hash[0] ? a->iters : ITERATIONS);
    return vfs_write_file(p, txt, (usize)n) != NULL;
}

#define ERR_NO_RNG "Este computador não tem gerador de números aleatórios de hardware (RDRAND/RDSEED): por segurança o mOS não cria senhas aqui."

/* false = recusado: com senha, só se houver fonte de hardware adequada para o sal (nunca jitter/relógios para algo que será gravado) */
static bool set_password(acct_t *a, const char *pw) {
    if (!pw || !*pw) { a->salt[0] = a->hash[0] = 0; a->fmt = 2; a->iters = 0; return true; }
    if (!crypto_rng_hw()) return false;
    /* o digitar da senha já alimentou o RNG (assistente/login); aqui o sal vem do CSPRNG (net/crypto.c) */
    u8 s[16];
    crypto_rng_add_entropy(pw, strlen(pw));
    crypto_random(s, 16);
    hex(s, 16, a->salt);
    a->fmt = 2;
    a->iters = account_kdf_iterations();
    derive_v2(a->salt, pw, a->iters, a->hash);
    secure_zero(s, sizeof s);
    return true;
}

/* ------------------------------------------------------------ consulta */
static int list_names(char names[][32], int max) {
    int n = 0;
    if (sysstate_guest_allowed()) strcpy(names[n++], ACCOUNT_DEFAULT);
    vnode_t *d = vfs_lookup(DIR);
    for (vnode_t *c = d && d->dir ? d->child : NULL; c && n < max; c = c->next) {
        usize l = strlen(c->name);
        if (l <= 6 || strcmp(c->name + l - 6, ".conta") || l - 6 >= 32) continue;
        char nm[32];
        memcpy(nm, c->name, l - 6);
        nm[l - 6] = 0;
        if (!strcmp(nm, ACCOUNT_DEFAULT)) continue;
        strcpy(names[n++], nm);
    }
    return n;
}

int account_count(void) { char names[32][32]; return list_names(names, 32); }

bool account_get(int i, char *name, usize ncap, char *fullname, usize fcap, bool *admin, bool *has_password) {
    char names[32][32];
    int n = list_names(names, 32);
    if (i < 0 || i >= n) return false;
    acct_t a;
    if (!load(names[i], &a)) return false;
    if (name) snprintf(name, ncap, "%s", names[i]);
    if (fullname) snprintf(fullname, fcap, "%s", a.full);
    if (admin) *admin = a.admin;
    if (has_password) *has_password = a.hash[0] != 0;
    return true;
}

bool account_exists(const char *name) { acct_t a; return name && load(name, &a); }
bool account_is_admin(const char *name) { acct_t a; return name && load(name, &a) && a.admin; }

bool account_verify(const char *name, const char *pw) {
    acct_t a;
    if (!name || !load(name, &a)) return false;
    if (!a.hash[0]) return !pw || !*pw;
    char h[65];
    if (a.fmt == 2) {
        if (!a.iters) return false;
        derive_v2(a.salt, pw ? pw : "", a.iters, h);
    } else derive_v1(a.salt, pw ? pw : "", h);
    u8 diff = 0;                                     /* comparação em tempo constante */
    for (int i = 0; i < 64; i++) diff |= (u8)(h[i] ^ a.hash[i]);
    bool ok = diff == 0;
    if (ok && a.fmt == 1 && pw && *pw && strcmp(name, ACCOUNT_DEFAULT)) {      /* migra o formato legado ao entrar */
        if (set_password(&a, pw) && save(name, &a)) kprintf("contas: \"%s\" migrada do formato 1 para PBKDF2 (%u iterações)\n", name, a.iters);
    }
    return ok;
}

/* ------------------------------------------------------------ alteração */
#define ERR(...) do { if (err) snprintf(err, errlen, __VA_ARGS__); return false; } while (0)

bool account_create(const char *name, const char *full, const char *pw, bool admin, char *err, usize errlen) {
    if (!name || !valid_name(name)) ERR("Nome de conta inválido: use de 1 a 31 letras, números, ponto, hífen ou sublinhado.");
    if (account_exists(name)) ERR("Já existe uma conta chamada \"%s\".", name);
    acct_t a;
    memset(&a, 0, sizeof a);
    snprintf(a.full, sizeof a.full, "%s", full && *full ? full : name);
    for (char *p = a.full; *p; p++) if (*p == '\n' || *p == '\r') *p = ' ';
    a.admin = admin;
    if (!set_password(&a, pw)) ERR(ERR_NO_RNG);
    if (!save(name, &a)) ERR("Não foi possível gravar a conta.");
    char home[96];
    snprintf(home, sizeof home, "/Users/%s", name);
    account_ensure_dirs(home);
    kprintf("contas: criada \"%s\" (%s)%s\n", name, a.full, admin ? ", administrador" : "");
    return true;
}

bool account_set_password(const char *name, const char *old, const char *pw, char *err, usize errlen) {
    acct_t a;
    if (!name || !load(name, &a)) ERR("Conta inexistente.");
    if (!account_verify(name, old)) ERR("A senha atual está incorreta.");
    if (!set_password(&a, pw)) ERR(ERR_NO_RNG);
    if (!save(name, &a)) ERR("Não foi possível gravar a nova senha.");
    return true;
}

bool account_delete(const char *name, bool remove_home, char *err, usize errlen) {
    if (!name || !account_exists(name)) ERR("Conta inexistente.");
    if (!strcmp(name, ACCOUNT_DEFAULT)) ERR("A conta padrão (%s) não pode ser apagada.", ACCOUNT_DEFAULT);
    if (!strcmp(name, cur_name)) ERR("Não é possível apagar a conta em uso.");
    char p[96];
    path_of(name, p, sizeof p);
    vnode_t *v = vfs_lookup(p);
    if (v && !vfs_delete(v)) ERR("Não foi possível apagar a conta.");
    if (remove_home) {
        snprintf(p, sizeof p, "/Users/%s", name);
        vnode_t *h = vfs_lookup(p);
        if (h) vfs_delete(h);
    }
    kprintf("contas: apagada \"%s\"\n", name);
    return true;
}

bool account_set_current(const char *name) {
    acct_t a;
    if (!name || !load(name, &a)) return false;
    snprintf(cur_name, sizeof cur_name, "%s", name);
    snprintf(cur_full, sizeof cur_full, "%s", a.full);
    snprintf(cur_home, sizeof cur_home, "/Users/%s", name);
    account_ensure_dirs(cur_home);
    kprintf("contas: sessão de \"%s\" (%s)\n", cur_name, cur_home);
    return true;
}

const char *account_current(void) { return cur_name; }
const char *account_fullname(void) { return cur_full; }
const char *account_home(void) { return cur_home; }

void account_ensure_dirs(const char *home) {
    static const char *sub[] = {
        "Mesa", "Documentos", "Downloads", "Imagens", "Música", ".Lixeira",
        "Library", "Library/Preferences", "Library/Application Support",
        "Library/Logs", "Library/Saved Application State", "Library/Caches",
        "Library/LaunchAgents"};
    char p[192];
    if (!home || !home[0]) return;
    for (u32 i = 0; i < ARRAY_LEN(sub); i++) {
        snprintf(p, sizeof p, "%s/%s", home, sub[i]);
        vfs_mkdir(p);
    }
}

/* ------------------------------------------------------------ autoteste (build.ps1 -Tests) */
void account_selftest(void) {
    char err[160];
    int fails = 0;
#define T(c, what) do { if (!(c)) { fails++; kprintf("contas: FALHOU %s %s\n", what, err); } } while (0)
    err[0] = 0;
    int before = account_count();
    T(account_verify(ACCOUNT_DEFAULT, ""), "visitante sem senha");
    T(!account_create("x y", "X", "a", false, err, sizeof err), "recusa nome inválido");
    T(account_create("ana", "Ana Souza", "segredo123", true, err, sizeof err), "criar ana");
    T(!account_create("ana", "Ana", "", false, err, sizeof err), "recusa duplicada");
    T(account_count() == before + 1, "contagem");
    T(account_verify("ana", "segredo123"), "senha certa");
    T(!account_verify("ana", "segredo124") && !account_verify("ana", ""), "senha errada recusada");
    T(account_set_password("ana", "segredo123", "nova-senha", err, sizeof err), "trocar senha");
    T(account_verify("ana", "nova-senha") && !account_verify("ana", "segredo123"), "nova senha vale");
    T(account_set_current("ana") && !strcmp(account_home(), "/Users/ana") && vfs_lookup("/Users/ana/Documentos"), "sessão da ana");
    T(vfs_lookup("/Users/ana/Library/Preferences") && vfs_lookup("/Users/ana/Library/Application Support")
      && vfs_lookup("/Users/ana/Library/LaunchAgents"), "pastas de preferências e agentes");
    T(!account_delete("ana", true, err, sizeof err), "não apaga a conta em uso");
    T(account_set_current(ACCOUNT_DEFAULT), "volta ao visitante");
    T(account_delete("ana", true, err, sizeof err) && !account_exists("ana") && !vfs_lookup("/Users/ana"), "apagar ana");
    kprintf("contas: autoteste %s\n", fails ? "FALHOU" : "OK");
#undef T
}
