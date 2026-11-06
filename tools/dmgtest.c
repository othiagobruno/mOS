/*
 * mOS — teste no host dos drivers de imagem de disco (kernel/fs).
 *   dmgtest <imagem.dmg> [pasta-de-saida]
 * Monta a imagem, percorre todos os volumes, lê cada arquivo inteiro e imprime
 * caminho, tamanho e CRC32. Com pasta de saída, extrai os arquivos para comparar.
 */
#include "fs.h"
#ifdef _WIN32
#include <direct.h>
#define mkdir_(p) _mkdir(p)
#else
#include <sys/stat.h>
#define mkdir_(p) mkdir(p, 0755)
#endif

static u32 crc_tab[256];
static u32 crc32(const u8 *p, u64 n) {
    if (!crc_tab[1])
        for (u32 i = 0; i < 256; i++) {
            u32 c = i;
            for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            crc_tab[i] = c;
        }
    u32 c = ~0u;
    for (u64 i = 0; i < n; i++) c = crc_tab[(c ^ p[i]) & 0xFF] ^ (c >> 8);
    return ~c;
}

long long g_live_allocs, g_live_bytes;
static int files, dirs, errors;
static u64 bytes;
static const char *outdir;

static void walk(dnode_t *n, const char *rel) {
    for (dnode_t *c = dfs_first_child(n); c; c = c->next) {
        char path[2048];
        snprintf(path, sizeof(path), "%s/%s", rel, c->name);
        if (c->type == DN_DIR) {
            dirs++;
            printf("D %s\n", path);
            if (outdir) { char p[2300]; snprintf(p, sizeof(p), "%s%s", outdir, path); mkdir_(p); }
            walk(c, path);
            continue;
        }
        u64 len = 0;
        u8 *b = dfs_read_all(c, 1ull << 31, &len);
        if (!b) { errors++; printf("! %s  (%llu bytes) ERRO DE LEITURA\n", path, (unsigned long long)c->size); continue; }
        files++;
        bytes += len;
        printf("%c %s  %llu  %08x\n", c->type == DN_LINK ? 'L' : 'F', path, (unsigned long long)len, crc32(b, len));
        if (outdir && c->type == DN_FILE) {
            char p[2300];
            snprintf(p, sizeof(p), "%s%s", outdir, path);
            FILE *o = fopen(p, "wb");
            if (o) { fwrite(b, 1, len, o); fclose(o); }
        }
        kfree(b);
    }
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "uso: dmgtest <imagem> [saida]\n"); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 2; }
    fseek(f, 0, SEEK_END);
    long long sz = _ftelli64(f);
    fseek(f, 0, SEEK_SET);
    u8 *data = malloc(sz);
    if (fread(data, 1, sz, f) != (size_t)sz) { fprintf(stderr, "leitura falhou\n"); return 2; }
    fclose(f);
    outdir = argc > 2 ? argv[2] : NULL;
    if (outdir && strcmp(outdir, "--cycle")) mkdir_(outdir);
    dfs_init();
    if (outdir && !strcmp(outdir, "--cycle")) {
        /* montar / ler tudo / ejetar várias vezes: as alocações vivas devem voltar ao início */
        outdir = NULL;
        long long base = g_live_allocs, base_b = g_live_bytes;
        for (int it = 0; it < 5; it++) {
            files = dirs = errors = 0;
            if (!dfs_mount_memory(data, sz, argv[1])) { fprintf(stderr, "falha ao montar\n"); return 1; }
            for (dmount_t *m = dfs_mounts(); m; m = m->next) walk(m->root, "");
            long long peak = g_live_bytes;
            dfs_trim();
            long long trimmed = g_live_bytes;
            while (dfs_mounts()) dfs_unmount(dfs_mounts());
            printf("ciclo %d: %d arquivos, %d erros, pico %lld KiB, após trim %lld KiB, após ejetar: %lld alocações / %lld bytes a mais que o início\n",
                   it, files, errors, (peak - base_b) / 1024, (trimmed - base_b) / 1024, g_live_allocs - base, g_live_bytes - base_b);
        }
        return 0;
    }
    int n = dfs_mount_memory(data, sz, argv[1]);
    if (!n) { fprintf(stderr, "nenhum volume montado\n"); return 1; }
    for (dmount_t *m = dfs_mounts(); m; m = m->next) {
        printf("== volume \"%s\" (%s, %s)\n", m->volname, m->ops->name, m->format);
        if (outdir) { char p[600]; snprintf(p, sizeof(p), "%s/%s", outdir, m->volname); mkdir_(p); }
        char rel[300];
        snprintf(rel, sizeof(rel), "/%s", m->volname);
        walk(m->root, rel);
    }
    printf("== %d pastas, %d arquivos, %llu bytes, %d erros\n", dirs, files, (unsigned long long)bytes, errors);
    return errors ? 1 : 0;
}
