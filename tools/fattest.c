/*
 * mOS — teste no host do driver FAT32 (kernel/fs/fat.c).
 *   fattest <imagem.img> [saida.img]
 * Abre a imagem GPT/FAT32 gerada pelo mkimg, exercita escrita (pastas, nomes longos com acentos,
 * substituição, diretórios com muitas entradas, renomear, mover, apagar) e confere tudo relendo.
 * Com saida.img, grava a imagem alterada para conferir com outro leitor (ex.: o firmware UEFI).
 */
#include "fs.h"
#include <time.h>

long long g_live_allocs, g_live_bytes;
static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FALHOU %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

void rtc_now(datetime_t *dt) {
    time_t t = time(NULL);
    struct tm *m = localtime(&t);
    dt->year = m->tm_year + 1900; dt->month = m->tm_mon + 1; dt->day = m->tm_mday;
    dt->hour = m->tm_hour; dt->minute = m->tm_min; dt->second = m->tm_sec; dt->weekday = m->tm_wday;
}
u32 rand32(void) { return (u32)rand() << 16 ^ (u32)rand(); }
static int count(fatfs_t *fs, const char *p, bool print);
static bool same(fatfs_t *fs, const char *p, const u8 *want, u64 n);
bool fat_format(blkdev_t *part, const char *label);
int fat_type(fatfs_t *fs);

/* ---- discos em RAM para os testes de formatação, FAT16 e FAT12 */
typedef struct { u8 *mem; } ramdisk_t;
static bool rrd(blkdev_t *d, u64 off, void *buf, u64 len) { memcpy(buf, ((ramdisk_t *)d->ctx)->mem + off, len); return true; }
static bool rwr(blkdev_t *d, u64 off, const void *buf, u64 len) { memcpy(((ramdisk_t *)d->ctx)->mem + off, buf, len); return true; }
static blkdev_t *ramdisk(u64 size) {
    ramdisk_t *r = calloc(1, sizeof *r);
    r->mem = calloc(1, size);
    blkdev_t *d = calloc(1, sizeof *d);
    d->size = size; d->read = rrd; d->write = rwr; d->ctx = r;
    return d;
}
/* formata à mão em FAT12/16 (o driver só formata FAT32), como o mkfs.fat faria */
static void mkfs_small(blkdev_t *d, int bits, u32 spc, u32 rootents) {
    u8 *m = ((ramdisk_t *)d->ctx)->mem;
    u32 tot = (u32)(d->size / 512), res = 1, nf = 2;
    u32 rsec = rootents * 32 / 512;
    u32 nclus = (tot - res - rsec) / spc, fatsz = 0;
    for (int it = 0; it < 16; it++) {
        fatsz = bits == 12 ? ((nclus + 2) * 3 / 2 + 511) / 512 : ((nclus + 2) * 2 + 511) / 512;
        nclus = (tot - res - nf * fatsz - rsec) / spc;
    }
    m[0] = 0xEB; m[1] = 0x3C; m[2] = 0x90;
    m[11] = 0; m[12] = 2; m[13] = (u8)spc; m[14] = (u8)res; m[16] = (u8)nf;
    m[17] = (u8)rootents; m[18] = (u8)(rootents >> 8);
    if (tot < 65536) { m[19] = (u8)tot; m[20] = (u8)(tot >> 8); } else { m[32] = (u8)tot; m[33] = (u8)(tot >> 8); m[34] = (u8)(tot >> 16); }
    m[21] = 0xF8; m[22] = (u8)fatsz; m[23] = (u8)(fatsz >> 8);
    m[38] = 0x29; memcpy(m + 43, bits == 12 ? "PEQUENO12  " : "CARTAO16   ", 11);
    m[510] = 0x55; m[511] = 0xAA;
    for (u32 f = 0; f < nf; f++) {
        u8 *fat = m + (res + f * fatsz) * 512;
        fat[0] = 0xF8; fat[1] = 0xFF; fat[2] = 0xFF; if (bits == 16) fat[3] = 0xFF;
    }
    printf("  FAT%d à mão: %u clusters de %u bytes, raiz com %u entradas\n", bits, nclus, spc * 512, rootents);
}

static void small_fat_test(int bits, u64 size, u32 spc) {
    printf("FAT%d (%llu MB):\n", bits, (unsigned long long)(size >> 20));
    blkdev_t *d = ramdisk(size);
    mkfs_small(d, bits, spc, 512);
    fatfs_t *fs = fat_mount(d);
    CHECK(fs && fat_type(fs) == bits, "mount FAT%d (tipo %d)", bits, fs ? fat_type(fs) : -1);
    if (!fs) return;
    char p[128], q[64];
    for (int i = 0; i < 60; i++) {           /* raiz fixa: cada nome longo ocupa 3-4 entradas */
        snprintf(p, sizeof p, "/Arquivo na raiz número %02d.txt", i);
        snprintf(q, sizeof q, "raiz %d", i);
        CHECK(fat_write_file(fs, p, q, strlen(q)), "write raiz %d", i);
    }
    CHECK(count(fs, "/", false) == 60, "60 na raiz (lidos %d)", count(fs, "/", false));
    CHECK(fat_mkdir(fs, "/Pasta Funda"), "mkdir");
    u64 n = 70000;                            /* muitos clusters: cadeias com entradas pares e ímpares */
    u8 *a = malloc(n);
    for (u64 i = 0; i < n; i++) a[i] = (u8)(i * 13 + (i >> 8));
    CHECK(fat_write_file(fs, "/Pasta Funda/dados.bin", a, n), "write 70 KB");
    for (int i = 0; i < 40; i++) {           /* subpasta cresce em vários clusters */
        snprintf(p, sizeof p, "/Pasta Funda/item com nome longo %02d.txt", i);
        CHECK(fat_write_file(fs, p, "x", 1), "write sub %d", i);
    }
    fat_unmount(fs);
    fs = fat_mount(d);                        /* remonta: tudo tem que ter ido para o "disco" */
    CHECK(fs != NULL, "remount");
    if (!fs) return;
    CHECK(same(fs, "/Pasta Funda/dados.bin", a, n), "reler 70 KB após remontar");
    CHECK(same(fs, "/Arquivo na raiz número 37.txt", (const u8 *)"raiz 37", 7), "reler raiz 37");
    CHECK(count(fs, "/Pasta Funda", false) == 41, "41 na subpasta (lidos %d)", count(fs, "/Pasta Funda", false));
    CHECK(fat_rename(fs, "/Pasta Funda/dados.bin", "/dados movidos.bin"), "mover para a raiz");
    CHECK(same(fs, "/dados movidos.bin", a, n), "reler após mover");
    CHECK(fat_remove(fs, "/Pasta Funda"), "rm -r");
    CHECK(fat_remove(fs, "/dados movidos.bin"), "rm");
    /* com tudo apagado, um arquivo do tamanho do disco quase inteiro precisa caber de novo */
    u64 big = size * 7 / 10;
    u8 *b = calloc(1, big);
    CHECK(fat_write_file(fs, "/grande.bin", b, big), "espaço liberado reaproveitado (%llu KB)", (unsigned long long)(big >> 10));
    free(b);
    fat_unmount(fs);
    free(a);
}

static void format_test(void) {
    printf("fat_format (48 MB):\n");
    blkdev_t *d = ramdisk(48u << 20);
    CHECK(fat_format(d, "Novo"), "format");
    fatfs_t *fs = fat_mount(d);
    CHECK(fs && fat_type(fs) == 32, "mount do recém-formatado");
    if (!fs) return;
    CHECK(!strcmp(fat_label(fs), "NOVO"), "rótulo (%s)", fat_label(fs));
    CHECK(count(fs, "/", false) == 0, "raiz vazia");
    CHECK(fat_mkdir(fs, "/EFI") && fat_mkdir(fs, "/EFI/BOOT"), "mkdir EFI/BOOT");
    CHECK(fat_write_file(fs, "/EFI/BOOT/BOOTX64.EFI", "MZ...", 5), "write efi");
    fat_unmount(fs);
    fs = fat_mount(d);
    CHECK(fs && same(fs, "/EFI/BOOT/BOOTX64.EFI", (const u8 *)"MZ...", 5), "reler após remontar");
    if (fs) fat_unmount(fs);
    CHECK(!fat_format(ramdisk(8u << 20), "x"), "recusa disco pequeno demais para FAT32");
}

/* fat.c também registra volumes no dfs; aqui não é usado */
dmount_t *dfs_attach(const dfs_ops_t *o, blkdev_t *d, const char *v, const char *s, const char *f, void *p, u64 r) { return NULL; }
dnode_t *dfs_new_child(dnode_t *dir, const char *name, u8 type, u64 size) { return NULL; }

static u8 *img;
static u64 img_size, part_off, part_size;
static long long writes;
static bool rd(blkdev_t *d, u64 off, void *buf, u64 len) { memcpy(buf, img + part_off + off, len); return true; }
static bool wr(blkdev_t *d, u64 off, const void *buf, u64 len) { memcpy(img + part_off + off, buf, len); writes++; return true; }

static int count_cb_n;
static void count_cb(void *ctx, const char *name, bool dir, u64 size) { count_cb_n++; if (ctx) printf("  %s%s (%llu)\n", name, dir ? "/" : "", (unsigned long long)size); }
static int count(fatfs_t *fs, const char *p, bool print) { count_cb_n = 0; return fat_list(fs, p, count_cb, print ? (void *)1 : NULL) < 0 ? -1 : count_cb_n; }

/* clusters livres contados direto na FAT da imagem (independente do cache do driver) */
static u32 free_clusters(void) {
    u8 *b = img + part_off;
    u32 res = le16(b + 14), nf = b[16], fsz = le32(b + 36), bps = le16(b + 11), spc = b[13];
    u32 tot = le32(b + 32), n = (tot - res - nf * fsz) / spc, k = 0;
    const u8 *fat = b + (u64)res * bps;
    for (u32 c = 2; c < n + 2; c++) if ((le32(fat + c * 4) & 0x0FFFFFFF) == 0) k++;
    return k;
}

static bool same(fatfs_t *fs, const char *p, const u8 *want, u64 n) {
    u64 len = 0;
    u8 *got = fat_read_file(fs, p, &len);
    bool ok = got && len == n && !memcmp(got, want, n);
    kfree(got);
    return ok;
}

int main(int argc, char **argv) {
    if (argc < 2) { fprintf(stderr, "uso: fattest <imagem.img> [saida.img]\n"); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 2; }
    fseek(f, 0, SEEK_END); img_size = ftell(f); fseek(f, 0, SEEK_SET);
    img = malloc(img_size);
    fread(img, 1, img_size, f);
    fclose(f);
    if (memcmp(img + 512, "EFI PART", 8)) { fprintf(stderr, "sem GPT\n"); return 2; }
    const u8 *e = img + le64(img + 512 + 72) * 512;
    part_off = le64(e + 32) * 512;
    part_size = (le64(e + 40) - le64(e + 32) + 1) * 512;
    blkdev_t dev = {.size = part_size, .read = rd, .write = wr};

    fatfs_t *fs = fat_mount(&dev);
    CHECK(fs, "fat_mount");
    if (!fs) return 1;
    if (argc > 2 && !strcmp(argv[2], "--ls")) {     /* só lista, sem alterar */
        const char *dirs[] = {"/", "/EFI", "/EFI/BOOT", "/mOS", "/mOS/Users", "/mOS/Users/Visitante", "/mOS/Users/Visitante/Documentos"};
        for (int i = 0; i < 7; i++) { printf("%s:\n", dirs[i]); count(fs, dirs[i], true); }
        u64 sz; bool dir;
        printf("BOOTX64.EFI: %s, %llu bytes\n", fat_stat(fs, "/EFI/BOOT/BOOTX64.EFI", &dir, &sz) ? "existe" : "NÃO existe", (unsigned long long)sz);
        if (argc > 3) {                               /* extrai o carregador para comparar */
            u64 len = 0;
            u8 *d = fat_read_file(fs, "/EFI/BOOT/BOOTX64.EFI", &len);
            FILE *o = fopen(argv[3], "wb");
            if (d && o) fwrite(d, 1, len, o);
            if (o) fclose(o);
        }
        return 0;
    }
    printf("volume \"%s\", %u clusters livres\n", fat_label(fs), free_clusters());
    printf("/mOS:\n");
    CHECK(count(fs, "/mOS", true) > 0, "listar /mOS");
    u64 sz;
    bool dir;
    CHECK(fat_stat(fs, "/MOS/users/visitante/DOCUMENTOS/bem-vindo.TXT", &dir, &sz) && !dir, "busca sem diferenciar caixa");
    u32 free0 = free_clusters();

    /* 1. pasta e arquivo com nome longo acentuado, várias dezenas de clusters */
    CHECK(fat_mkdir(fs, "/mOS/Teste Ação"), "mkdir acentuado");
    CHECK(fat_mkdir(fs, "/mOS/Teste Ação"), "mkdir de pasta existente devolve true");
    u64 n1 = 100000;
    u8 *a = malloc(n1);
    for (u64 i = 0; i < n1; i++) a[i] = (u8)(i * 31 + (i >> 11));
    const char *p1 = "/mOS/Teste Ação/Relatório de vendas — 2026 (versão final).txt";
    CHECK(fat_write_file(fs, p1, a, n1), "write 100 KB");
    CHECK(same(fs, p1, a, n1), "reler 100 KB");

    /* 2. substituir por conteúdo menor: a cadeia antiga precisa voltar para a lista livre */
    CHECK(fat_write_file(fs, p1, "curto", 5), "substituir");
    CHECK(same(fs, p1, (const u8 *)"curto", 5), "reler substituído");
    CHECK(fat_write_file(fs, "/mOS/Teste Ação/vazio.txt", "", 0), "arquivo vazio");
    CHECK(fat_stat(fs, "/mOS/Teste Ação/vazio.txt", &dir, &sz) && sz == 0, "stat vazio");

    /* 3. diretório com muitas entradas (vários clusters de diretório, nomes 8.3 com ~N) */
    CHECK(fat_mkdir(fs, "/mOS/Muitos"), "mkdir Muitos");
    char p[300], q[300];
    for (int i = 0; i < 300; i++) {
        snprintf(p, sizeof p, "/mOS/Muitos/arquivo com nome comprido número %03d.txt", i);
        snprintf(q, sizeof q, "conteúdo %d", i);
        if (!fat_write_file(fs, p, q, strlen(q))) { CHECK(0, "write %s", p); break; }
    }
    CHECK(count(fs, "/mOS/Muitos", false) == 300, "300 entradas (lidas %d)", count(fs, "/mOS/Muitos", false));
    snprintf(p, sizeof p, "/mOS/Muitos/arquivo com nome comprido número %03d.txt", 257);
    CHECK(same(fs, p, (const u8 *)"conteúdo 257", strlen("conteúdo 257")), "ler entrada 257");

    /* 4. renomear: mesma pasta, só caixa, entre pastas, e mover pasta */
    CHECK(fat_rename(fs, p1, "/mOS/Teste Ação/Relatório.txt"), "rename mesma pasta");
    CHECK(!fat_stat(fs, p1, NULL, NULL), "nome antigo sumiu");
    CHECK(fat_rename(fs, "/mOS/Teste Ação/Relatório.txt", "/mOS/Teste Ação/RELATÓRIO.txt"), "rename só caixa");
    CHECK(fat_rename(fs, "/mOS/Teste Ação/RELATÓRIO.txt", "/mOS/Users/Visitante/Documentos/RELATÓRIO.txt"), "mover arquivo");
    CHECK(same(fs, "/mOS/Users/Visitante/Documentos/RELATÓRIO.txt", (const u8 *)"curto", 5), "conteúdo após mover");
    CHECK(fat_mkdir(fs, "/mOS/Teste Ação/Sub"), "mkdir Sub");
    CHECK(fat_write_file(fs, "/mOS/Teste Ação/Sub/x.txt", "xyz", 3), "write Sub/x");
    CHECK(fat_rename(fs, "/mOS/Teste Ação/Sub", "/mOS/Users/Sub Movida"), "mover pasta");
    CHECK(same(fs, "/mOS/Users/Sub Movida/x.txt", (const u8 *)"xyz", 3), "arquivo dentro da pasta movida");
    CHECK(fat_stat(fs, "/mOS/Users/Sub Movida/../Visitante", &dir, NULL) && dir, "\"..\" da pasta movida aponta para o novo pai");
    CHECK(!fat_rename(fs, "/mOS/Users/Sub Movida/x.txt", "/mOS/Users/Visitante/Documentos/RELATÓRIO.txt"), "não sobrescreve no rename");

    /* 5. apagar tudo que foi criado: o espaço livre tem que voltar ao original */
    CHECK(fat_remove(fs, "/mOS/Muitos"), "rm -r Muitos");
    CHECK(fat_remove(fs, "/mOS/Users/Sub Movida"), "rm pasta movida");
    CHECK(fat_remove(fs, "/mOS/Users/Visitante/Documentos/RELATÓRIO.txt"), "rm arquivo");
    CHECK(fat_remove(fs, "/mOS/Teste Ação"), "rm Teste Ação");
    fat_sync(fs);
    u32 free1 = free_clusters();
    CHECK(free1 + 2 >= free0 && free1 <= free0, "espaço livre voltou (%u antes, %u depois)", free0, free1);
    /* pastas que cresceram (/mOS/Users, /mOS) podem manter um cluster extra, como no Windows */

    /* 6. deixa arquivos para o firmware ler */
    CHECK(fat_mkdir(fs, "/mOS/Users/Visitante/Documentos/Notas de Março"), "mkdir final");
    CHECK(fat_write_file(fs, "/mOS/Users/Visitante/Documentos/Notas de Março/lista de compras.txt", "leite\ncafé\n", 12), "write final");
    CHECK(fat_write_file(fs, "/mOS/Users/Visitante/Documentos/grande.bin", a, n1), "write final grande");
    fat_unmount(fs);

    printf("%d escritas no dispositivo, %lld alocações vivas\n", (int)writes, g_live_allocs);
    format_test();
    small_fat_test(16, 16u << 20, 4);
    small_fat_test(12, 2u << 20, 1);
    printf("%lld alocações vivas no fim\n", g_live_allocs);
    if (argc > 2) {
        FILE *o = fopen(argv[2], "wb");
        fwrite(img, 1, img_size, o);
        fclose(o);
        printf("imagem alterada gravada em %s\n", argv[2]);
    }
    printf(fails ? "%d FALHA(S)\n" : "TUDO OK\n", fails);
    return fails ? 1 : 0;
}
