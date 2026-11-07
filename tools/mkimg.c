/*
 * mkimg — gera uma imagem de disco GPT com uma partição de sistema EFI (FAT32)
 * contendo uma árvore de diretórios. Uso: mkimg <saida.img> <diretorio> <tamanho_MiB>
 * A imagem pode ser gravada diretamente num pendrive (Rufus modo DD, balenaEtcher, dd).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>

#define SECTOR 512
#define SPC 2 /* setores por cluster (1 KiB) */
#define CLUSTER (SECTOR * SPC)

typedef struct node {
    char name[256];
    int is_dir;
    unsigned char *data;
    uint32_t size;
    struct node *child, *next, *parent;
    uint32_t cluster, nclusters;
    char sfn[11];
} node;

static uint8_t *img;
static uint64_t img_size;
static uint32_t part_lba, part_sectors, fat_sectors, data_lba, total_clusters, next_cluster = 2;
static uint32_t *fat;

static uint32_t crc32(const void *buf, size_t n) {
    static uint32_t t[256];
    if (!t[1]) for (uint32_t i = 0; i < 256; i++) { uint32_t c = i; for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >> 1) : c >> 1; t[i] = c; }
    uint32_t c = 0xFFFFFFFF;
    const uint8_t *p = buf;
    while (n--) c = t[(c ^ *p++) & 0xFF] ^ (c >> 8);
    return c ^ 0xFFFFFFFF;
}

static void wr16(uint8_t *p, uint16_t v) { p[0] = v; p[1] = v >> 8; }
static void wr32(uint8_t *p, uint32_t v) { wr16(p, v); wr16(p + 2, v >> 16); }
static void wr64(uint8_t *p, uint64_t v) { wr32(p, (uint32_t)v); wr32(p + 4, v >> 32); }

#ifdef _WIN32
#include <windows.h>
/* Caminhos acima de 260 caracteres (helpers do Chrome: ".../Helpers/Google Chrome Helper (Aperitif Renderer).app/
 * Contents/MacOS/Google Chrome Helper (Aperitif Renderer)") falhavam no stat/fopen ANSI. Aqui o Windows recebe o
 * caminho absoluto com \\?\ pelas funções wide; os nomes continuam na página de código ANSI, como o readdir dava. */
static wchar_t *wpath(const char *p) {
    static wchar_t rel[4096], full[4200];
    MultiByteToWideChar(CP_ACP, 0, p, -1, rel, 4096);
    for (wchar_t *s = rel; *s; s++) if (*s == L'/') *s = L'\\';
    wcscpy(full, L"\\\\?\\");
    if (!GetFullPathNameW(rel, 4096, full + 4, NULL)) return rel;
    return full;
}
static int path_stat(const char *p, int *is_dir, uint64_t *size) {
    WIN32_FILE_ATTRIBUTE_DATA a;
    if (!GetFileAttributesExW(wpath(p), GetFileExInfoStandard, &a)) return -1;
    *is_dir = (a.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
    *size = ((uint64_t)a.nFileSizeHigh << 32) | a.nFileSizeLow;
    return 0;
}
static FILE *path_open(const char *p) { return _wfopen(wpath(p), L"rb"); }
#else
static int path_stat(const char *p, int *is_dir, uint64_t *size) {
    struct stat st;
    if (stat(p, &st)) return -1;
    *is_dir = S_ISDIR(st.st_mode);
    *size = (uint64_t)st.st_size;
    return 0;
}
static FILE *path_open(const char *p) { return fopen(p, "rb"); }
#endif

static node *load_tree(const char *path, const char *name, node *parent) {
    node *n = calloc(1, sizeof(node));
    strncpy(n->name, name, 255);
    n->parent = parent;
    int is_dir;
    uint64_t fsize;
    if (path_stat(path, &is_dir, &fsize)) { perror(path); exit(1); }
    if (is_dir) {
        n->is_dir = 1;
        node **tail = &n->child;
#ifdef _WIN32
        char pat[1024];
        snprintf(pat, sizeof pat, "%s/*", path);
        WIN32_FIND_DATAW fd;
        HANDLE h = FindFirstFileW(wpath(pat), &fd);
        if (h != INVALID_HANDLE_VALUE) {
            do {
                char en[MAX_PATH * 2];
                WideCharToMultiByte(CP_ACP, 0, fd.cFileName, -1, en, sizeof en, NULL, NULL);
                if (!strcmp(en, ".") || !strcmp(en, "..")) continue;
                char p[1024];
                snprintf(p, sizeof p, "%s/%s", path, en);
                *tail = load_tree(p, en, n);
                tail = &(*tail)->next;
            } while (FindNextFileW(h, &fd));
            FindClose(h);
        }
#else
        DIR *d = opendir(path);
        struct dirent *e;
        while ((e = readdir(d))) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
            char p[1024];
            snprintf(p, sizeof p, "%s/%s", path, e->d_name);
            *tail = load_tree(p, e->d_name, n);
            tail = &(*tail)->next;
        }
        closedir(d);
#endif
    } else {
        struct { uint64_t st_size; } st = { fsize };
        FILE *f = path_open(path);
        if (!f) { perror(path); exit(1); }
        n->size = (uint32_t)st.st_size;
        n->data = malloc(n->size + 1);
        if (n->size && fread(n->data, 1, n->size, f) != n->size) { perror(path); exit(1); }
        fclose(f);
    }
    return n;
}

static int is_valid_sfn(const char *s) {
    const char *dot = strchr(s, '.');
    size_t bl = dot ? (size_t)(dot - s) : strlen(s), el = dot ? strlen(dot + 1) : 0;
    if (bl == 0 || bl > 8 || el > 3 || (dot && strchr(dot + 1, '.'))) return 0;
    for (const char *p = s; *p; p++) {
        if (*p == '.') continue;
        if (!(isupper((unsigned char)*p) || isdigit((unsigned char)*p) || strchr("_-~!#$%&", *p))) return 0;
    }
    return 1;
}

static int lfn_count(node *n) { return is_valid_sfn(n->name) ? 0 : (int)((strlen(n->name) + 12) / 13); }

static void make_sfn(node *n, int idx) {
    memset(n->sfn, ' ', 11);
    if (is_valid_sfn(n->name)) {
        const char *dot = strchr(n->name, '.');
        size_t bl = dot ? (size_t)(dot - n->name) : strlen(n->name);
        memcpy(n->sfn, n->name, bl);
        if (dot) memcpy(n->sfn + 8, dot + 1, strlen(dot + 1));
        return;
    }
    char base[16] = {0};
    int k = 0;
    for (const char *p = n->name; *p && k < 6; p++) {
        if (*p == '.') break;
        if (isalnum((unsigned char)*p)) base[k++] = toupper((unsigned char)*p);
    }
    if (!k) base[k++] = 'F';
    char tmp[16];
    snprintf(tmp, sizeof tmp, "%s~%d", base, idx);
    memcpy(n->sfn, tmp, strlen(tmp) > 8 ? 8 : strlen(tmp));
    const char *dot = strrchr(n->name, '.');
    if (dot && dot != n->name) {
        k = 0;
        for (const char *p = dot + 1; *p && k < 3; p++) if (isalnum((unsigned char)*p)) n->sfn[8 + k++] = toupper((unsigned char)*p);
    }
}

static uint32_t dir_entries(node *d) {
    uint32_t e = d->parent ? 2 : 1; /* . e .. (raiz: rótulo do volume) */
    for (node *c = d->child; c; c = c->next) e += 1 + lfn_count(c);
    return e;
}

static void alloc_clusters(node *n) {
    uint32_t bytes = n->is_dir ? dir_entries(n) * 32 : n->size;
    n->nclusters = bytes ? (bytes + CLUSTER - 1) / CLUSTER : (n->is_dir ? 1 : 0);
    if (n->nclusters) {
        n->cluster = next_cluster;
        for (uint32_t i = 0; i < n->nclusters; i++) fat[next_cluster + i] = (i + 1 == n->nclusters) ? 0x0FFFFFFF : next_cluster + i + 1;
        next_cluster += n->nclusters;
        if (next_cluster >= total_clusters + 2) { fprintf(stderr, "imagem pequena demais\n"); exit(1); }
    }
    int idx = 1;
    for (node *c = n->child; c; c = c->next) { make_sfn(c, idx++); alloc_clusters(c); }
}

static uint8_t *cluster_ptr(uint32_t c) { return img + (uint64_t)(part_lba + data_lba + (c - 2) * SPC) * SECTOR; }

static uint8_t sfn_checksum(const char *s) {
    uint8_t sum = 0;
    for (int i = 0; i < 11; i++) sum = ((sum & 1) << 7) + (sum >> 1) + (uint8_t)s[i];
    return sum;
}

static void put_entry(uint8_t *e, const char *sfn, uint8_t attr, uint32_t cl, uint32_t size) {
    memcpy(e, sfn, 11);
    e[11] = attr;
    wr16(e + 14, 0x6000); wr16(e + 16, 0x5A66); /* 12:00, 2025-03-06 */
    wr16(e + 18, 0x5A66);
    wr16(e + 20, cl >> 16);
    wr16(e + 22, 0x6000); wr16(e + 24, 0x5A66);
    wr16(e + 26, cl & 0xFFFF);
    wr32(e + 28, size);
}

static void write_node(node *n) {
    if (!n->is_dir) {
        if (n->size) memcpy(cluster_ptr(n->cluster), n->data, n->size);
        return;
    }
    uint8_t *e = cluster_ptr(n->cluster);
    if (n->parent) {
        put_entry(e, ".          ", 0x10, n->cluster, 0); e += 32;
        uint32_t pc = n->parent->parent ? n->parent->cluster : 0;
        put_entry(e, "..         ", 0x10, pc, 0); e += 32;
    } else {
        put_entry(e, "MOS        ", 0x08, 0, 0); e += 32;
    }
    for (node *c = n->child; c; c = c->next) {
        int nl = lfn_count(c);
        uint8_t ck = sfn_checksum(c->sfn);
        size_t len = strlen(c->name);
        for (int i = nl; i >= 1; i--) {
            memset(e, 0, 32);
            e[0] = (uint8_t)i | (i == nl ? 0x40 : 0);
            e[11] = 0x0F; e[13] = ck;
            static const int off[13] = {1, 3, 5, 7, 9, 14, 16, 18, 20, 22, 24, 28, 30};
            for (int k = 0; k < 13; k++) {
                size_t ci = (size_t)(i - 1) * 13 + k;
                uint16_t ch = ci < len ? (uint8_t)c->name[ci] : ci == len ? 0 : 0xFFFF;
                wr16(e + off[k], ch);
            }
            e += 32;
        }
        put_entry(e, c->sfn, c->is_dir ? 0x10 : 0x20, c->cluster, c->is_dir ? 0 : c->size);
        e += 32;
        write_node(c);
    }
}


/* ------------------------------------------------------------------ manifesto do payload de instalação
 * O instalador do mOS (kernel/fs/install.c) copia SÓ os arquivos listados em /mOS/Sistema/Instalacao/manifesto.txt, cada
 * um conferido por SHA-256, e o próprio manifesto termina com uma linha ASSINATURA-SHA256 (hash do que vem antes). Os
 * hashes são calculados aqui, sobre os mesmos bytes que entram na imagem: nada de sessão (contas, logs, preferências, o
 * estado do sistema) pode entrar no payload depois do build. Ficam FORA do payload: a pasta Instalacao e o arquivo
 * Biblioteca/estado-sistema (marca "LIVE" desta mídia). Formato:
 *   mOS-MANIFESTO-1 <n> <total de bytes>
 *   <sha256 hex> <tamanho> <caminho>        (ordenado por strcmp dos caminhos; caminhos absolutos na ESP)
 *   ASSINATURA-SHA256 <sha256 hex de tudo acima>
 */
typedef struct { uint32_t h[8]; uint8_t buf[64]; uint64_t total; uint32_t n; } sha_t;
static const uint32_t K256[64] = {
 0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
 0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
 0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
 0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))
static void sha_block(sha_t *c, const uint8_t *p) {
    uint32_t w[64], a, b, cc, d, e, f, g, h;
    for (int i = 0; i < 16; i++) w[i] = (uint32_t)p[i*4] << 24 | (uint32_t)p[i*4+1] << 16 | (uint32_t)p[i*4+2] << 8 | p[i*4+3];
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ROR(w[i-15], 7) ^ ROR(w[i-15], 18) ^ (w[i-15] >> 3), s1 = ROR(w[i-2], 17) ^ ROR(w[i-2], 19) ^ (w[i-2] >> 10);
        w[i] = w[i-16] + s0 + w[i-7] + s1;
    }
    a = c->h[0]; b = c->h[1]; cc = c->h[2]; d = c->h[3]; e = c->h[4]; f = c->h[5]; g = c->h[6]; h = c->h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t t1 = h + (ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25)) + ((e & f) ^ (~e & g)) + K256[i] + w[i];
        uint32_t t2 = (ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22)) + ((a & b) ^ (a & cc) ^ (b & cc));
        h = g; g = f; f = e; e = d + t1; d = cc; cc = b; b = a; a = t1 + t2;
    }
    c->h[0] += a; c->h[1] += b; c->h[2] += cc; c->h[3] += d; c->h[4] += e; c->h[5] += f; c->h[6] += g; c->h[7] += h;
}
static void sha_init(sha_t *c) {
    static const uint32_t iv[8] = {0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    memcpy(c->h, iv, sizeof iv); c->total = 0; c->n = 0;
}
static void sha_update(sha_t *c, const void *d, size_t len) {
    const uint8_t *p = d;
    c->total += len;
    while (len) {
        size_t k = 64 - c->n < len ? 64 - c->n : len;
        memcpy(c->buf + c->n, p, k); c->n += (uint32_t)k; p += k; len -= k;
        if (c->n == 64) { sha_block(c, c->buf); c->n = 0; }
    }
}
static void sha_final(sha_t *c, uint8_t out[32]) {
    uint64_t bits = c->total * 8;
    uint8_t pad = 0x80;
    sha_update(c, &pad, 1);
    pad = 0;
    while (c->n != 56) sha_update(c, &pad, 1);
    uint8_t l[8];
    for (int i = 0; i < 8; i++) l[i] = (uint8_t)(bits >> (56 - 8 * i));
    sha_update(c, l, 8);
    for (int i = 0; i < 8; i++) { out[i*4] = (uint8_t)(c->h[i] >> 24); out[i*4+1] = (uint8_t)(c->h[i] >> 16); out[i*4+2] = (uint8_t)(c->h[i] >> 8); out[i*4+3] = (uint8_t)c->h[i]; }
}
static void sha_hex(const void *d, size_t n, char out[65]) {
    sha_t c; uint8_t h[32];
    sha_init(&c); sha_update(&c, d, n); sha_final(&c, h);
    for (int i = 0; i < 32; i++) sprintf(out + i * 2, "%02x", h[i]);
}

typedef struct { char *path; node *n; } mitem;
static mitem *mitems; static size_t nmitems, capmitems;

static void collect(node *n, const char *prefix) {
    char path[1400];
    for (node *c = n->child; c; c = c->next) {
        snprintf(path, sizeof path, "%s/%s", prefix, c->name);
        if (c->is_dir) {
            if (!strcmp(path, "/mOS/Sistema/Instalacao")) continue;                 /* o próprio manifesto */
            collect(c, path);
            continue;
        }
        if (!strcmp(path, "/mOS/Sistema/Biblioteca/estado-sistema")) continue;      /* marca da mídia, não do payload */
        if (strlen(path) > 1000) { fprintf(stderr, "mkimg: caminho longo demais para o payload (>1000): %s\n", path); exit(1); }
        if (strchr(c->name, '\n') || strchr(c->name, '\\') || !strcmp(c->name, "..")) { fprintf(stderr, "mkimg: nome inválido no payload: %s\n", path); exit(1); }
        if (strcmp(path, "/EFI/BOOT/BOOTX64.EFI") && strncmp(path, "/mOS/", 5)) { fprintf(stderr, "mkimg: arquivo fora de /EFI/BOOT/BOOTX64.EFI e /mOS/: %s\n", path); exit(1); }
        if (nmitems == capmitems) { capmitems = capmitems ? capmitems * 2 : 256; mitems = realloc(mitems, capmitems * sizeof *mitems); }
        mitems[nmitems].path = strdup(path); mitems[nmitems].n = c; nmitems++;
    }
}
static int cmp_mitem(const void *a, const void *b) { return strcmp(((const mitem *)a)->path, ((const mitem *)b)->path); }

static node *sub_dir(node *parent, const char *name) {
    for (node *c = parent->child; c; c = c->next) if (c->is_dir && !strcmp(c->name, name)) return c;
    node *d = calloc(1, sizeof(node));
    strncpy(d->name, name, 255); d->is_dir = 1; d->parent = parent;
    node **t = &parent->child; while (*t) t = &(*t)->next; *t = d;
    return d;
}
static void add_file(node *dir, const char *name, const void *data, size_t size) {
    node *f = calloc(1, sizeof(node));
    strncpy(f->name, name, 255); f->parent = dir; f->size = (uint32_t)size;
    f->data = malloc(size + 1); memcpy(f->data, data, size);
    node **t = &dir->child; while (*t) t = &(*t)->next; *t = f;
}


/* ------------------------------------------------------------------ política de conteúdo limpo do payload
 * O payload (o que o instalador copia) é só o SISTEMA. O staging (a árvore passada ao mkimg) é REJEITADO — o build falha e
 * nenhuma imagem é gerada — se contiver contas, hashes de senha, logs, preferências, estado, pastas pessoais ou qualquer
 * resíduo de sessão. Dados de exemplo "de fábrica" ficam em um overlay Live separado (--live-overlay), que só pode conter
 * /mOS/Users/ e NUNCA entra no manifesto. */
static const char *const DENY_PREFIX[] = {
    "/mOS/Users", "/mOS/Sistema/Biblioteca/Usuarios", "/mOS/Sistema/Biblioteca/Logs", "/mOS/Sistema/Biblioteca/Preferencias",
    "/mOS/Sistema/Instalacao", "/mOS/tmp", "/mOS/Volumes", "/mOS/Sistema/Biblioteca/estado-sistema", NULL};
static const char *deny_reason(const char *path, const char *name) {
    for (int i = 0; DENY_PREFIX[i]; i++) {
        size_t n = strlen(DENY_PREFIX[i]);
        if (!strncmp(path, DENY_PREFIX[i], n) && (path[n] == 0 || path[n] == '/')) return "área pessoal, de sessão ou gerada pelo build";
    }
    size_t l = strlen(name);
    if (l > 6 && !strcmp(name + l - 6, ".conta")) return "arquivo de conta de usuário";
    if (l > 4 && !strcmp(name + l - 4, ".log")) return "registro (log)";
    if (!strcmp(name, ".Lixeira") || !strcmp(name, ".DS_Store") || !strcmp(name, "Thumbs.db")) return "resíduo de sessão";
    return NULL;
}
static void validate_staging(node *n, const char *prefix) {
    char path[1400];
    for (node *c = n->child; c; c = c->next) {
        snprintf(path, sizeof path, "%s/%s", prefix, c->name);
        const char *why = deny_reason(path, c->name);
        if (why) { fprintf(stderr, "mkimg: staging CONTAMINADO: %s (%s). Nenhuma imagem foi gerada.\n", path, why); exit(1); }
        if (c->is_dir) validate_staging(c, path);
    }
}
static void validate_overlay(node *n, const char *prefix) {
    char path[1400];
    for (node *c = n->child; c; c = c->next) {
        snprintf(path, sizeof path, "%s/%s", prefix, c->name);
        if (strcmp(path, "/mOS") && strcmp(path, "/mOS/Users") && strncmp(path, "/mOS/Users/", 11)) {
            fprintf(stderr, "mkimg: o overlay Live só pode conter /mOS/Users/: %s\n", path); exit(1);
        }
        size_t l = strlen(c->name);
        if ((l > 6 && !strcmp(c->name + l - 6, ".conta")) || (l > 4 && !strcmp(c->name + l - 4, ".log")) || !strcmp(c->name, ".Lixeira")) {
            fprintf(stderr, "mkimg: overlay Live com conta/log/resíduo de sessão: %s\n", path); exit(1);
        }
        if (c->is_dir) validate_overlay(c, path);
    }
}
static int merge_overlay(node *dst, node *src) {
    int files = 0;
    node *c = src->child;
    while (c) {
        node *nx = c->next;
        if (c->is_dir) {
            node *d = sub_dir(dst, c->name);
            files += merge_overlay(d, c);
        } else {
            for (node *e = dst->child; e; e = e->next)
                if (!strcmp(e->name, c->name)) { fprintf(stderr, "mkimg: overlay colide com o payload: %s\n", c->name); exit(1); }
            c->parent = dst; c->next = NULL;
            node **t = &dst->child; while (*t) t = &(*t)->next; *t = c;
            files++;
        }
        c = nx;
    }
    return files;
}

static void make_manifest(node *root) {
    collect(root, "");
    qsort(mitems, nmitems, sizeof *mitems, cmp_mitem);
    int has_loader = 0;
    uint64_t total = 0;
    for (size_t i = 0; i < nmitems; i++) { total += mitems[i].n->size; if (!strcmp(mitems[i].path, "/EFI/BOOT/BOOTX64.EFI")) has_loader = 1; }
    if (!has_loader) { fprintf(stderr, "mkimg: falta /EFI/BOOT/BOOTX64.EFI no payload\n"); exit(1); }
    size_t cap = 128 + nmitems * 1100 + 128, len = 0;
    char *m = malloc(cap);
    len += (size_t)snprintf(m + len, cap - len, "mOS-MANIFESTO-1 %zu %llu\n", nmitems, (unsigned long long)total);
    for (size_t i = 0; i < nmitems; i++) {
        char hx[65];
        sha_hex(mitems[i].n->data, mitems[i].n->size, hx);
        len += (size_t)snprintf(m + len, cap - len, "%s %u %s\n", hx, (unsigned)mitems[i].n->size, mitems[i].path);
    }
    char sg[65];
    sha_hex(m, len, sg);
    len += (size_t)snprintf(m + len, cap - len, "ASSINATURA-SHA256 %s\n", sg);
    node *mos = sub_dir(root, "mOS"), *sis = sub_dir(mos, "Sistema"), *bib = sub_dir(sis, "Biblioteca"), *ins = sub_dir(sis, "Instalacao");
    add_file(ins, "manifesto.txt", m, len);
    add_file(bib, "estado-sistema", "LIVE\n", 5);
    printf("mkimg: manifesto do payload: %zu arquivos, %llu bytes (assinatura %.16s…)\n", nmitems, (unsigned long long)total, sg);
    free(m);
}

static void guid(uint8_t *g) { for (int i = 0; i < 16; i++) g[i] = rand() & 0xFF; g[7] = (g[7] & 0x0F) | 0x40; g[8] = (g[8] & 0x3F) | 0x80; }

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "uso: mkimg <saida.img> <dir> <MiB> [--live-overlay <dir>]\n"); return 1; }
    const char *overlay_dir = NULL;
    for (int i = 4; i < argc; i++) {
        if (!strcmp(argv[i], "--live-overlay") && i + 1 < argc) overlay_dir = argv[++i];
        else { fprintf(stderr, "mkimg: argumento desconhecido: %s\n", argv[i]); return 1; }
    }
    remove(argv[1]);   /* fail-closed: se o build falhar, nunca sobra uma imagem antiga para ser gravada por engano */
    srand((unsigned)time(NULL));
    img_size = (uint64_t)atoi(argv[3]) << 20;
    uint64_t total_lba = img_size / SECTOR;
    img = calloc(1, img_size);
    part_lba = 2048;
    part_sectors = (uint32_t)(total_lba - 34 - part_lba);

    /* ---- MBR protetor ---- */
    uint8_t *mbr = img;
    mbr[446 + 4] = 0xEE;
    wr32(mbr + 446 + 8, 1);
    wr32(mbr + 446 + 12, (uint32_t)(total_lba - 1 > 0xFFFFFFFF ? 0xFFFFFFFF : total_lba - 1));
    mbr[446 + 1] = 0; mbr[446 + 2] = 2; mbr[446 + 5] = 0xFF; mbr[446 + 6] = 0xFF; mbr[446 + 7] = 0xFF;
    mbr[510] = 0x55; mbr[511] = 0xAA;

    /* ---- GPT ---- */
    uint8_t entries[128 * 128] = {0};
    static const uint8_t esp_type[16] = {0x28, 0x73, 0x2A, 0xC1, 0x1F, 0xF8, 0xD2, 0x11, 0xBA, 0x4B, 0x00, 0xA0, 0xC9, 0x3E, 0xC9, 0x3B};
    memcpy(entries, esp_type, 16);
    guid(entries + 16);
    wr64(entries + 32, part_lba);
    wr64(entries + 40, part_lba + part_sectors - 1);
    const char *pname = "mOS";
    for (int i = 0; pname[i]; i++) wr16(entries + 56 + i * 2, pname[i]);
    uint32_t ecrc = crc32(entries, sizeof entries);
    uint8_t disk_guid[16];
    guid(disk_guid);
    for (int backup = 0; backup < 2; backup++) {
        uint64_t my = backup ? total_lba - 1 : 1, alt = backup ? 1 : total_lba - 1;
        uint64_t elba = backup ? total_lba - 33 : 2;
        uint8_t *h = img + my * SECTOR;
        memcpy(h, "EFI PART", 8);
        wr32(h + 8, 0x00010000);
        wr32(h + 12, 92);
        wr64(h + 24, my); wr64(h + 32, alt);
        wr64(h + 40, 34); wr64(h + 48, total_lba - 34);
        memcpy(h + 56, disk_guid, 16);
        wr64(h + 72, elba);
        wr32(h + 80, 128); wr32(h + 84, 128);
        wr32(h + 88, ecrc);
        wr32(h + 16, crc32(h, 92));
        memcpy(img + elba * SECTOR, entries, sizeof entries);
    }

    /* ---- FAT32 ---- */
    uint32_t reserved = 32;
    total_clusters = (part_sectors - reserved) / SPC;
    fat_sectors = (total_clusters * 4 + 8 + SECTOR - 1) / SECTOR;
    total_clusters = (part_sectors - reserved - 2 * fat_sectors) / SPC;
    data_lba = reserved + 2 * fat_sectors;
    fat = calloc(fat_sectors * SECTOR / 4, 4);
    fat[0] = 0x0FFFFFF8; fat[1] = 0x0FFFFFFF;

    uint8_t *bs = img + (uint64_t)part_lba * SECTOR;
    bs[0] = 0xEB; bs[1] = 0x58; bs[2] = 0x90;
    memcpy(bs + 3, "MOS 0.1 ", 8);
    wr16(bs + 11, SECTOR); bs[13] = SPC; wr16(bs + 14, reserved); bs[16] = 2;
    bs[21] = 0xF8; wr16(bs + 24, 63); wr16(bs + 26, 255);
    wr32(bs + 28, part_lba); wr32(bs + 32, part_sectors);
    wr32(bs + 36, fat_sectors); wr32(bs + 44, 2); wr16(bs + 48, 1); wr16(bs + 50, 6);
    bs[64] = 0x80; bs[66] = 0x29; wr32(bs + 67, (uint32_t)rand());
    memcpy(bs + 71, "MOS        ", 11); memcpy(bs + 82, "FAT32   ", 8);
    bs[510] = 0x55; bs[511] = 0xAA;

    node *root = load_tree(argv[2], "", NULL);
    validate_staging(root, "");
    make_manifest(root);
    if (overlay_dir) {
        node *ov = load_tree(overlay_dir, "", NULL);
        validate_overlay(ov, "");
        printf("mkimg: overlay Live (fora do payload e do manifesto): %d arquivos\n", merge_overlay(root, ov));
    }
    alloc_clusters(root);
    write_node(root);

    uint8_t *fsi = bs + SECTOR;
    wr32(fsi, 0x41615252); wr32(fsi + 484, 0x61417272);
    wr32(fsi + 488, total_clusters + 2 - next_cluster); wr32(fsi + 492, next_cluster);
    wr32(fsi + 508, 0xAA550000);
    memcpy(bs + 6 * SECTOR, bs, 2 * SECTOR);
    for (int i = 0; i < 2; i++) memcpy(bs + (uint64_t)(reserved + i * fat_sectors) * SECTOR, fat, (size_t)fat_sectors * SECTOR);

    FILE *o = fopen(argv[1], "wb");
    if (!o) { perror(argv[1]); return 1; }
    fwrite(img, 1, img_size, o);
    fclose(o);
    printf("mkimg: %s — %u MiB, %u clusters usados de %u\n", argv[1], (unsigned)(img_size >> 20), next_cluster - 2, total_clusters);
    return 0;
}
