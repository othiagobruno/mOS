/*
 * URL do overlay Swift do Foundation. URL não é @frozen: o app passa o valor por endereço e
 * só conhece o tamanho pela metadata (fdn_meta.S, tabela fdn_vwt_obj). Por dentro é um NSURL,
 * referência forte (o mesmo layout que foundation_swift_data.c já usava nas pontes).
 *
 * As operações de caminho (lastPathComponent, appendingPathComponent, deletingPathExtension...)
 * são feitas aqui no texto do absoluteString, separado em prefixo (esquema://autoridade),
 * caminho e sufixo (?consulta#fragmento); o URL novo sai de fileURLWithPath: (arquivo) ou
 * URLWithString: (os outros). O lado ObjC é fdn_ns_url_* (fdn_tipos_ns.c / teste de host).
 */
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>
#include "fdn_tipos.h"

char *strcat(char *d, const char *s);

#define SW __attribute__((swiftcall))
#define CTX __attribute__((swift_context))
#define USED __attribute__((used))
#if defined(__x86_64__)
#define SRET(sym, impl) __asm__(".text\n.p2align 4\n.globl \"" sym "\"\n\"" sym "\":\n" \
    "  movq %r8, %r9\n  movq %rcx, %r8\n  movq %rdx, %rcx\n  movq %rsi, %rdx\n" \
    "  movq %rdi, %rsi\n  movq %rax, %rdi\n  jmp " #impl "\n")
#else
#define SRET(sym, impl)
#endif

#define FDN_URL_MAX 4096

/* ---------------------------------------------------------------- texto */

typedef struct { char pre[FDN_URL_MAX]; char path[FDN_URL_MAX]; char suf[FDN_URL_MAX]; } url_partes;

static void cpy(char *d, const char *s, size_t n) {
    if (n >= FDN_URL_MAX) n = FDN_URL_MAX - 1;
    memcpy(d, s, n);
    d[n] = 0;
}

/* "esquema://autoridade/caminho?q#f" -> pre, path, suf. Sem "//": tudo depois do "esquema:" é caminho. */
void fdn_url_split(const char *abs, url_partes *p) {
    const char *s = abs, *colon = strchr(abs, ':'), *ps, *pe;
    const char *q;
    ps = s;
    if (colon) {
        const char *k;
        int ok = colon > s;
        for (k = s; k < colon; k++)
            if (!((*k >= 'a' && *k <= 'z') || (*k >= 'A' && *k <= 'Z') || (*k >= '0' && *k <= '9') || *k == '+' || *k == '-' || *k == '.')) ok = 0;
        if (ok) {
            ps = colon + 1;
            if (ps[0] == '/' && ps[1] == '/') {
                ps += 2;
                while (*ps && *ps != '/' && *ps != '?' && *ps != '#') ps++;
            }
        }
    }
    pe = ps;
    while (*pe && *pe != '?' && *pe != '#') pe++;
    q = pe;
    cpy(p->pre, s, (size_t)(ps - s));
    cpy(p->path, ps, (size_t)(pe - ps));
    cpy(p->suf, q, strlen(q));
}

/* último componente: sem as barras do fim; "/" fica "/" */
void fdn_path_last(const char *p, char *out) {
    size_t n = strlen(p), i;
    while (n > 1 && p[n - 1] == '/') n--;
    if (n == 1 && p[0] == '/') { strcpy(out, "/"); return; }
    i = n;
    while (i > 0 && p[i - 1] != '/') i--;
    cpy(out, p + i, n - i);
}

void fdn_path_ext(const char *p, char *out) {
    char last[FDN_URL_MAX];
    char *dot;
    fdn_path_last(p, last);
    dot = strrchr(last, '.');
    if (!dot || dot == last || !dot[1] || strcmp(last, "/") == 0) { out[0] = 0; return; }
    strcpy(out, dot + 1);
}

static size_t sem_barra(const char *p, int *tinha) {
    size_t n = strlen(p);
    *tinha = 0;
    while (n > 1 && p[n - 1] == '/') { n--; *tinha = 1; }
    return n;
}

/* 0 = mudou, -1 = devolve o URL como está */
int fdn_path_op(const char *p, int op, const char *arg, int dir, char *out) {
    size_t n;
    int barra;
    out[0] = 0;
    switch (op) {
    case 0: /* appendingPathComponent */
        n = strlen(p);
        if (strlen(p) + strlen(arg) + 3 >= FDN_URL_MAX) return -1;
        strcpy(out, p);
        if (n == 0 || p[n - 1] != '/') strcat(out, "/");
        while (*arg == '/' && n && p[n - 1] == '/') arg++;
        strcat(out, arg);
        if (dir && out[strlen(out) - 1] != '/') strcat(out, "/");
        return 0;
    case 1: /* deletingLastPathComponent */
        if (!p[0]) return -1;
        n = sem_barra(p, &barra);
        if (n == 1 && p[0] == '/') { strcpy(out, "/../"); return 0; }
        while (n > 0 && p[n - 1] != '/') n--;
        if (n == 0) { strcpy(out, "./"); return 0; }
        cpy(out, p, n);
        return 0;
    case 2: /* appendingPathExtension */
        if (!p[0] || !arg[0] || strcmp(p, "/") == 0 || strchr(arg, '/')) return -1;
        n = sem_barra(p, &barra);
        if (n + strlen(arg) + 3 >= FDN_URL_MAX) return -1;
        cpy(out, p, n);
        strcat(out, ".");
        strcat(out, arg);
        if (barra) strcat(out, "/");
        return 0;
    case 3: { /* deletingPathExtension */
        size_t i;
        if (!p[0]) return -1;
        n = sem_barra(p, &barra);
        i = n;
        while (i > 0 && p[i - 1] != '/' && p[i - 1] != '.') i--;
        if (i == 0 || p[i - 1] != '.' || i - 1 == 0 || p[i - 2] == '/') return -1;
        cpy(out, p, i - 1);
        if (barra) strcat(out, "/");
        return 0;
    }
    case 4: { /* standardized: tira "." e resolve ".." */
        char tmp[FDN_URL_MAX];
        char *seg[512];
        int ns = 0, abs = p[0] == '/', fim = 0;
        char *t, *save;
        n = strlen(p);
        if (!n) return -1;
        fim = p[n - 1] == '/';
        cpy(tmp, p, n);
        for (t = tmp; ; t = 0) {
            char *s = t ? t : save;
            char *e;
            if (!s) break;
            while (*s == '/') s++;
            if (!*s) break;
            e = strchr(s, '/');
            if (e) { *e = 0; save = e + 1; } else save = 0;
            if (strcmp(s, ".") == 0) { if (!save) fim = 1; continue; }
            if (strcmp(s, "..") == 0) {
                if (ns > 0 && strcmp(seg[ns - 1], "..") != 0) ns--;
                else if (!abs && ns < 512) seg[ns++] = s;
                if (!save) fim = 1;
                continue;
            }
            if (ns < 512) seg[ns++] = s;
        }
        strcpy(out, abs ? "/" : "");
        for (int i = 0; i < ns; i++) {
            if (i) strcat(out, "/");
            strcat(out, seg[i]);
        }
        if (fim && ns && out[strlen(out) - 1] != '/') strcat(out, "/");
        if (!out[0]) strcpy(out, abs ? "/" : ".");
        return 0;
    }
    }
    return -1;
}

/* Componente de caminho num URL que não é de arquivo: percent-encoding fora do conjunto permitido no caminho (RFC 3986). */
static void pct_path(const char *in, char *out, size_t cap) {
    static const char hx[] = "0123456789ABCDEF";
    size_t k = 0;
    for (; *in && k + 4 < cap; in++) {
        unsigned char c = (unsigned char)*in;
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || strchr("-._~!$&'()*+,;=:@/%", c))
            out[k++] = (char)c;
        else { out[k++] = '%'; out[k++] = hx[c >> 4]; out[k++] = hx[c & 15]; }
    }
    out[k] = 0;
}

/* ---------------------------------------------------------------- objeto */

static void *url_from_text(const char *abs, int file, const char *path) {
    return file ? fdn_ns_url_file(path) : fdn_ns_url_string(abs);
}

/* Aplica uma operação de caminho e devolve um NSURL novo (+1); -1 da operação devolve o mesmo, retido. */
void *fdn_url_derive(void *u, int op, const char *arg, int dir) {
    url_partes *pt = malloc(sizeof *pt);
    char *np = malloc(FDN_URL_MAX), *abs = malloc(FDN_URL_MAX * 3);
    void *r = 0;
    const char *a = fdn_ns_url_part(u, FDN_URL_ABS);
    int file = fdn_ns_url_isfile(u);
    if (!pt || !np || !abs || !a) goto mesmo;
    if (file) {
        const char *path = fdn_ns_url_part(u, FDN_URL_PATH);
        if (!path || strlen(path) >= FDN_URL_MAX) goto mesmo;
        if (op == 0 && dir < 0) {
            char tmp[FDN_URL_MAX];
            if (fdn_path_op(path, 0, arg, 0, tmp) == 0) dir = fdn_ns_isdir(tmp);
        }
        if (fdn_path_op(path, op, arg, dir > 0, np) != 0) goto mesmo;
        r = url_from_text(0, 1, np);
    } else {
        if (strlen(a) >= FDN_URL_MAX) goto mesmo;
        fdn_url_split(a, pt);
        if (op == 0 || op == 2) {
            pct_path(arg, abs, FDN_URL_MAX);
            arg = abs;
        }
        if (fdn_path_op(pt->path, op, arg, dir > 0, np) != 0) goto mesmo;
        if (pt->pre[0] && np[0] && np[0] != '/' && strstr(pt->pre, "//")) {
            memmove(np + 1, np, strlen(np) + 1);
            np[0] = '/';
        }
        strcpy(abs, pt->pre);
        strcat(abs, np);
        strcat(abs, pt->suf);
        r = url_from_text(abs, 0, 0);
    }
    if (r) goto fim;
mesmo:
    fdn_ns_retain(u);
    r = u;
fim:
    free(pt); free(np); free(abs);
    return r;
}

/* String? a partir de um C string; nil = os dois words zerados */
static fdn_str opt_str(const char *s) {
    fdn_str z = { 0, 0 };
    return s ? fdn_str_make(s, -1) : z;
}

static int read_arg(fdn_str s, char *buf) { return fdn_str_read(s, buf, FDN_URL_MAX) >= 0; }

/* ---------------------------------------------------------------- inits */

/* URL(string:) -> URL?: Optional de 9 bytes (o valor e o byte de tag) */
SRET("$s10Foundation3URLV6stringACSgSSh_tcfC", fdn_url_init_string);
static SW USED void fdn_url_init_string(void **out, fdn_str s) {
    char buf[FDN_URL_MAX];
    void *u = 0;
    if (read_arg(s, buf) && buf[0]) u = fdn_ns_url_string(buf);
    if (u) { *out = u; fdn_enum_tag_store(out, 0, 1, 8); }
    else fdn_enum_tag_store(out, 1, 1, 8);
}

/* URL(fileURLWithPath:) e URL(fileURLWithPath:isDirectory:) */
SRET("$s10Foundation3URLV15fileURLWithPathACSSh_tcfC", fdn_url_init_file);
static SW USED void fdn_url_init_file(void **out, fdn_str s) {
    char buf[FDN_URL_MAX];
    if (!read_arg(s, buf)) buf[0] = 0;
    *out = fdn_ns_url_file(buf);
}
SRET("$s10Foundation3URLV15fileURLWithPath11isDirectoryACSSh_SbtcfC", fdn_url_init_file_dir);
static SW USED void fdn_url_init_file_dir(void **out, fdn_str s, _Bool dir) {
    char buf[FDN_URL_MAX];
    size_t n;
    if (!read_arg(s, buf)) buf[0] = 0;
    n = strlen(buf);
    if ((dir & 1) && n && buf[n - 1] != '/' && n + 1 < FDN_URL_MAX) strcat(buf, "/");
    *out = fdn_ns_url_file(buf);
}

/* pontes com NSURL (_unconditionallyBridgeFromObjectiveC: nil vira URL de arquivo vazio como na Apple não existe; o
 * compilador só chama com objeto) */
SW void *fdn_url_to_ns(void *const *self CTX) __asm__("$s10Foundation3URLV19_bridgeToObjectiveCSo5NSURLCyF");
SW void *fdn_url_to_ns(void *const *self CTX) { fdn_ns_retain(*self); return *self; }
SRET("$s10Foundation3URLV36_unconditionallyBridgeFromObjectiveCyACSo5NSURLCSgFZ", fdn_url_from_ns_sw);
static SW USED void fdn_url_from_ns_sw(void **out, void *ns) {
    if (ns) { fdn_ns_retain(ns); *out = ns; }
    else *out = fdn_ns_url_file("");
}

/* ---------------------------------------------------------------- getters */

#define GET_STR(nome, sym, parte) \
    SW fdn_str nome(void *const *self CTX) __asm__(sym); \
    SW fdn_str nome(void *const *self CTX) { const char *s = fdn_ns_url_part(*self, parte); return fdn_str_make(s ? s : "", -1); }
#define GET_OPT(nome, sym, parte) \
    SW fdn_str nome(void *const *self CTX) __asm__(sym); \
    SW fdn_str nome(void *const *self CTX) { return opt_str(fdn_ns_url_part(*self, parte)); }

GET_STR(fdn_url_get_path, "$s10Foundation3URLV4pathSSvg", FDN_URL_PATH)
GET_STR(fdn_url_abs, "$s10Foundation3URLV14absoluteStringSSvg", FDN_URL_ABS)
GET_STR(fdn_url_rel, "$s10Foundation3URLV14relativeStringSSvg", FDN_URL_REL)
GET_STR(fdn_url_relpath, "$s10Foundation3URLV12relativePathSSvg", FDN_URL_PATH)
GET_OPT(fdn_url_scheme, "$s10Foundation3URLV6schemeSSSgvg", FDN_URL_SCHEME)
GET_OPT(fdn_url_host, "$s10Foundation3URLV4hostSSSgvg", FDN_URL_HOST)
GET_OPT(fdn_url_user, "$s10Foundation3URLV4userSSSgvg", FDN_URL_USER)
GET_OPT(fdn_url_password, "$s10Foundation3URLV8passwordSSSgvg", FDN_URL_PASSWORD)
GET_OPT(fdn_url_query, "$s10Foundation3URLV5querySSSgvg", FDN_URL_QUERY)
GET_OPT(fdn_url_fragment, "$s10Foundation3URLV8fragmentSSSgvg", FDN_URL_FRAGMENT)

static const char *path_of(void *u, char *tmp) {
    const char *a;
    url_partes *pt;
    if (fdn_ns_url_isfile(u)) return fdn_ns_url_part(u, FDN_URL_PATH);
    a = fdn_ns_url_part(u, FDN_URL_ABS);
    if (!a || strlen(a) >= FDN_URL_MAX) return 0;
    pt = malloc(sizeof *pt);
    if (!pt) return 0;
    fdn_url_split(a, pt);
    strcpy(tmp, pt->path);
    free(pt);
    return tmp;
}

SW fdn_str fdn_url_last(void *const *self CTX) __asm__("$s10Foundation3URLV17lastPathComponentSSvg");
SW fdn_str fdn_url_last(void *const *self CTX) {
    char tmp[FDN_URL_MAX], out[FDN_URL_MAX];
    const char *p = path_of(*self, tmp);
    out[0] = 0;
    if (p) fdn_path_last(p, out);
    return fdn_str_make(out, -1);
}
SW fdn_str fdn_url_ext(void *const *self CTX) __asm__("$s10Foundation3URLV13pathExtensionSSvg");
SW fdn_str fdn_url_ext(void *const *self CTX) {
    char tmp[FDN_URL_MAX], out[FDN_URL_MAX];
    const char *p = path_of(*self, tmp);
    out[0] = 0;
    if (p) fdn_path_ext(p, out);
    return fdn_str_make(out, -1);
}
/* path(percentEncoded:): o NSURL deste Foundation guarda o caminho já decodificado */
SW fdn_str fdn_url_path_pe(_Bool pe, void *const *self CTX) __asm__("$s10Foundation3URLV4path14percentEncodedSSSb_tF");
SW fdn_str fdn_url_path_pe(_Bool pe, void *const *self CTX) {
    char tmp[FDN_URL_MAX];
    const char *p = path_of(*self, tmp);
    return fdn_str_make(p ? p : "", -1);
}
SW _Bool fdn_url_isfile(void *const *self CTX) __asm__("$s10Foundation3URLV06isFileB0Sbvg");
SW _Bool fdn_url_isfile(void *const *self CTX) { return fdn_ns_url_isfile(*self) != 0; }
SW _Bool fdn_url_hasdir(void *const *self CTX) __asm__("$s10Foundation3URLV16hasDirectoryPathSbvg");
SW _Bool fdn_url_hasdir(void *const *self CTX) {
    char tmp[FDN_URL_MAX];
    const char *p = path_of(*self, tmp);
    size_t n = p ? strlen(p) : 0;
    return n && p[n - 1] == '/';
}
/* var port: Int? -> (valor em rax, tag em dl: 1 = nil) */
typedef struct { long v; _Bool none; } opt_int;
SW opt_int fdn_url_port(void *const *self CTX) __asm__("$s10Foundation3URLV4portSiSgvg");
SW opt_int fdn_url_port(void *const *self CTX) {
    opt_int r;
    int has = 0;
    r.v = fdn_ns_url_port(*self, &has);
    if (!has) r.v = 0;
    r.none = !has;
    return r;
}
SW long fdn_url_hash(void *const *self CTX) __asm__("$s10Foundation3URLV9hashValueSivg");
SW long fdn_url_hash(void *const *self CTX) { return fdn_ns_hash(*self); }
SW _Bool fdn_url_eq(void *const *a, void *const *b) __asm__("$s10Foundation3URLV2eeoiySbAC_ACtFZ");
SW _Bool fdn_url_eq(void *const *a, void *const *b) { return fdn_ns_equal(*a, *b) != 0; }

/* ---------------------------------------------------------------- derivados (-> URL) */

#define DERIVE0(nome, sym, op) \
    SRET(sym, nome); \
    static SW USED void nome(void **out, void *const *self CTX) { *out = fdn_url_derive(*self, op, "", 0); }

DERIVE0(fdn_url_del_last, "$s10Foundation3URLV25deletingLastPathComponentACyF", 1)
DERIVE0(fdn_url_del_ext, "$s10Foundation3URLV21deletingPathExtensionACyF", 3)

SRET("$s10Foundation3URLV016standardizedFileB0ACvg", fdn_url_std_file);
static SW USED void fdn_url_std_file(void **out, void *const *self CTX) {
    if (fdn_ns_url_isfile(*self)) *out = fdn_url_derive(*self, 4, "", 0);
    else { fdn_ns_retain(*self); *out = *self; }
}
DERIVE0(fdn_url_std, "$s10Foundation3URLV12standardizedACvg", 4)

SRET("$s10Foundation3URLV23resolvingSymlinksInPathACyF", fdn_url_resolve);
static SW USED void fdn_url_resolve(void **out, void *const *self CTX) {
    void *r = fdn_ns_url_file_op(*self, FDN_URL_OP_RESOLVE);
    if (!r) { fdn_ns_retain(*self); r = *self; }
    *out = r;
}
SRET("$s10Foundation3URLV08absoluteB0ACvg", fdn_url_absolute);
static SW USED void fdn_url_absolute(void **out, void *const *self CTX) {
    void *r = fdn_ns_url_file_op(*self, FDN_URL_OP_ABSOLUTE);
    if (!r) { fdn_ns_retain(*self); r = *self; }
    *out = r;
}

SRET("$s10Foundation3URLV22appendingPathComponentyACSSF", fdn_url_append);
static SW USED void fdn_url_append(void **out, fdn_str c, void *const *self CTX) {
    char buf[FDN_URL_MAX];
    if (!read_arg(c, buf)) buf[0] = 0;
    *out = fdn_url_derive(*self, 0, buf, -1);
}
SRET("$s10Foundation3URLV22appendingPathComponent_11isDirectoryACSS_SbtF", fdn_url_append_dir);
static SW USED void fdn_url_append_dir(void **out, fdn_str c, _Bool dir, void *const *self CTX) {
    char buf[FDN_URL_MAX];
    if (!read_arg(c, buf)) buf[0] = 0;
    *out = fdn_url_derive(*self, 0, buf, dir & 1);
}
SRET("$s10Foundation3URLV22appendingPathExtensionyACSSF", fdn_url_append_ext);
static SW USED void fdn_url_append_ext(void **out, fdn_str e, void *const *self CTX) {
    char buf[FDN_URL_MAX];
    if (!read_arg(e, buf)) buf[0] = 0;
    *out = fdn_url_derive(*self, 2, buf, 0);
}

/* mutating: self em r13 é o endereço do valor; troca a referência e solta a antiga */
static void troca(void **self, void *novo) {
    void *velho = *self;
    *self = novo;
    fdn_ns_release(velho);
}
SW void fdn_url_m_append(fdn_str c, void **self CTX) __asm__("$s10Foundation3URLV19appendPathComponentyySSF");
SW void fdn_url_m_append(fdn_str c, void **self CTX) {
    char buf[FDN_URL_MAX];
    if (!read_arg(c, buf)) buf[0] = 0;
    troca(self, fdn_url_derive(*self, 0, buf, -1));
}
SW void fdn_url_m_append_dir(fdn_str c, _Bool dir, void **self CTX) __asm__("$s10Foundation3URLV19appendPathComponent_11isDirectoryySS_SbtF");
SW void fdn_url_m_append_dir(fdn_str c, _Bool dir, void **self CTX) {
    char buf[FDN_URL_MAX];
    if (!read_arg(c, buf)) buf[0] = 0;
    troca(self, fdn_url_derive(*self, 0, buf, dir & 1));
}
SW void fdn_url_m_append_ext(fdn_str e, void **self CTX) __asm__("$s10Foundation3URLV19appendPathExtensionyySSF");
SW void fdn_url_m_append_ext(fdn_str e, void **self CTX) {
    char buf[FDN_URL_MAX];
    if (!read_arg(e, buf)) buf[0] = 0;
    troca(self, fdn_url_derive(*self, 2, buf, 0));
}
SW void fdn_url_m_del_last(void **self CTX) __asm__("$s10Foundation3URLV23deleteLastPathComponentyyF");
SW void fdn_url_m_del_last(void **self CTX) { troca(self, fdn_url_derive(*self, 1, "", 0)); }

/* Sem sandbox no mOS: nenhum URL é de escopo de segurança, então start devolve false e stop não tem o que soltar. */
SW _Bool fdn_url_start_scope(void *const *self CTX) __asm__("$s10Foundation3URLV36startAccessingSecurityScopedResourceSbyF");
SW _Bool fdn_url_start_scope(void *const *self CTX) { return 0; }
SW void fdn_url_stop_scope(void *const *self CTX) __asm__("$s10Foundation3URLV35stopAccessingSecurityScopedResourceyyF");
SW void fdn_url_stop_scope(void *const *self CTX) { }

/* ---------------------------------------------------------------- URL.DirectoryHint */

/*
 * Enum resiliente sem payload: um byte com o índice do caso (ordem da declaração no swift-foundation:
 * isDirectory, notDirectory, checkFileSystem, inferFromPath). Os WC são as variáveis globais com o
 * índice que o app lê para montar e comparar o caso.
 */
const uint32_t fdn_hint_is __asm__("$s10Foundation3URLV13DirectoryHintO02isC0yA2EmFWC") = 0;
const uint32_t fdn_hint_not __asm__("$s10Foundation3URLV13DirectoryHintO03notC0yA2EmFWC") = 1;
const uint32_t fdn_hint_check __asm__("$s10Foundation3URLV13DirectoryHintO15checkFileSystemyA2EmFWC") = 2;
const uint32_t fdn_hint_infer __asm__("$s10Foundation3URLV13DirectoryHintO13inferFromPathyA2EmFWC") = 3;

/* URL(filePath:directoryHint:relativeTo:): hint por endereço, base URL? por endereço (9 bytes) */
SRET("$s10Foundation3URLV8filePath13directoryHint10relativeToACSS_AC09DirectoryF0OACSgtcfC", fdn_url_init_filepath);
static SW USED void fdn_url_init_filepath(void **out, fdn_str s, const uint8_t *hint, void *const *base) {
    char buf[FDN_URL_MAX];
    size_t n;
    int dir;
    if (!read_arg(s, buf)) buf[0] = 0;
    n = strlen(buf);
    if (buf[0] != '/' && base && fdn_enum_tag_get(base, 1, 8) == 0 && *base) {
        const char *bp = fdn_ns_url_part(*base, FDN_URL_PATH);
        char tmp[FDN_URL_MAX];
        if (bp && fdn_path_op(bp, 0, buf, 0, tmp) == 0) { strcpy(buf, tmp); n = strlen(buf); }
    }
    switch (hint ? *hint : 3) {
    case 0: dir = 1; break;
    case 1: dir = 0; break;
    case 2: dir = fdn_ns_isdir(buf); break;
    default: dir = n && buf[n - 1] == '/'; break;
    }
    if (dir && n && buf[n - 1] != '/' && n + 1 < FDN_URL_MAX) strcat(buf, "/");
    if (!dir) while (n > 1 && buf[n - 1] == '/') buf[--n] = 0;
    *out = fdn_ns_url_file(buf);
}
