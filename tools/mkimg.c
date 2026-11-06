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

static node *load_tree(const char *path, const char *name, node *parent) {
    node *n = calloc(1, sizeof(node));
    strncpy(n->name, name, 255);
    n->parent = parent;
    struct stat st;
    if (stat(path, &st)) { perror(path); exit(1); }
    if (S_ISDIR(st.st_mode)) {
        n->is_dir = 1;
        DIR *d = opendir(path);
        struct dirent *e;
        node **tail = &n->child;
        while ((e = readdir(d))) {
            if (!strcmp(e->d_name, ".") || !strcmp(e->d_name, "..")) continue;
            char p[1024];
            snprintf(p, sizeof p, "%s/%s", path, e->d_name);
            *tail = load_tree(p, e->d_name, n);
            tail = &(*tail)->next;
        }
        closedir(d);
    } else {
        FILE *f = fopen(path, "rb");
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

static void guid(uint8_t *g) { for (int i = 0; i < 16; i++) g[i] = rand() & 0xFF; g[7] = (g[7] & 0x0F) | 0x40; g[8] = (g[8] & 0x3F) | 0x80; }

int main(int argc, char **argv) {
    if (argc < 4) { fprintf(stderr, "uso: mkimg <saida.img> <dir> <MiB>\n"); return 1; }
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
