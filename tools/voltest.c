/*
 * mOS — teste no host da escrita em volumes de /Volumes (kernel/fs/fat.c e hfsplus_wr.c pelas dfs_ops).
 *   voltest <imagem.img>            exercita todos os volumes graváveis e grava a imagem alterada
 *   voltest <imagem.img> --mount    só monta (repete o journal) e lista /seed.txt de cada volume
 * Depois: volimg check <imagem.img> confere a estrutura com um verificador independente.
 *
 * Compilar (como o dmgtest):
 *   zig cc -target x86_64-windows-gnu -O1 -DMOS_HOST_TEST -Ikernel/include -Itools tools/voltest.c
 *     kernel/fs/{apfs,blkdev,bzip2,dfs,dmg,hfsplus,hfsplus_wr,inflate,lzfse,xz,fat}.c -o build/voltest.exe
 */
#include "fs.h"
#include <time.h>

long long g_live_allocs, g_live_bytes;
static int fails, checks;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FALHOU %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

void rtc_now(datetime_t *dt) {
    time_t t = time(NULL);
    struct tm *m = gmtime(&t);
    dt->year = m->tm_year + 1900; dt->month = m->tm_mon + 1; dt->day = m->tm_mday;
    dt->hour = m->tm_hour; dt->minute = m->tm_min; dt->second = m->tm_sec; dt->weekday = m->tm_wday;
}
u32 rand32(void) { return (u32)rand() << 16 ^ (u32)rand(); }
void dfs_publish_new(dmount_t *before) { (void)before; }

static u8 *img;
static u64 img_size;
static bool rd(blkdev_t *d, u64 off, void *buf, u64 len) { memcpy(buf, img + off, len); return true; }
/* queda de energia simulada: depois de crash_after escritas, as seguintes somem em silêncio */
static long crash_after = -1, nwrites;
static bool wr(blkdev_t *d, u64 off, const void *buf, u64 len) {
    nwrites++;
    if (crash_after >= 0 && nwrites > crash_after) return true;
    memcpy(img + off, buf, len);
    return true;
}

static blkdev_t *disk(void) {
    blkdev_t *d = calloc(1, sizeof *d);
    d->size = img_size; d->read = rd; d->write = wr;
    return d;
}

static dmount_t *nth(int i) {
    /* dfs_mounts() vem em ordem inversa de montagem */
    int n = 0;
    for (dmount_t *m = dfs_mounts(); m; m = m->next) n++;
    int k = n - 1 - i;
    for (dmount_t *m = dfs_mounts(); m; m = m->next) if (!k--) return m;
    return NULL;
}

static dnode_t *lookup(dmount_t *m, const char *path) {
    char full[1200];
    snprintf(full, sizeof full, "/Volumes/%s%s", m->root->name, path);
    return dfs_lookup(full);
}

static bool same(dmount_t *m, const char *path, const u8 *want, u64 n) {
    dnode_t *f = lookup(m, path);
    if (!f || f->type == DN_DIR) return false;
    u64 len = 0;
    u8 *b = dfs_read_all(f, 1ull << 30, &len);
    bool ok = b && len == n && !memcmp(b, want, n);
    kfree(b);
    return ok;
}

static int count(dmount_t *m, const char *path) {
    dnode_t *d = lookup(m, path);
    if (!d || d->type != DN_DIR) return -1;
    int n = 0;
    for (dnode_t *c = dfs_first_child(d); c; c = c->next) if (!c->hidden) n++;
    return n;
}

static void unmount_all(void) { while (dfs_mounts()) dfs_unmount(dfs_mounts()); }
static int mount_all(void) { return dfs_mount_disk(disk(), "teste"); }

static u8 *pattern(u64 n, u32 seed) {
    u8 *a = malloc(n ? n : 1);
    for (u64 i = 0; i < n; i++) a[i] = (u8)(i * 131 + seed + (i >> 9));
    return a;
}

/* ---- bateria em um volume (pelos ops do driver, como faz vfs_bridge.c) */
static void phase1(dmount_t *m, bool hfs) {
    const dfs_ops_t *o = m->ops;
    printf("[%s] %s: escrita\n", m->volname, o->name);
    CHECK(o->write_file(m, "/novo.txt", "olá, mOS\n", 10), "criar /novo.txt");
    CHECK(o->mkdir(m, "/Pasta Acentuação"), "mkdir com acento");
    CHECK(o->mkdir(m, "/Pasta Acentuação"), "mkdir de pasta existente = true");
    CHECK(o->mkdir(m, "/Pasta Acentuação/Sub"), "mkdir aninhado");
    CHECK(o->write_file(m, "/Pasta Acentuação/Sub/arquivo com nome longo é ç ã.txt", "acentos", 7), "nome longo com acentos");
    CHECK(!o->write_file(m, "/Não Existe/x.txt", "x", 1), "pai inexistente falha");
    /* muitos arquivos: divide nós do catálogo / cresce o diretório FAT */
    CHECK(o->mkdir(m, "/Muitos"), "mkdir Muitos");
    char p[256];
    for (int i = 0; i < 300; i++) {
        snprintf(p, sizeof p, "/Muitos/arquivo número %03d com um nome bem comprido para encher nós.txt", i);
        char c[32];
        int n = snprintf(c, sizeof c, "conteúdo %d", i);
        if (!o->write_file(m, p, c, (u64)n)) { CHECK(0, "write %d", i); break; }
    }
    /* arquivo médio, substituído por outro maior e depois menor */
    u64 n1 = 300000, n2 = 1500000, n3 = 5000;
    u8 *a = pattern(n1, 1), *b = pattern(n2, 2), *c = pattern(n3, 3);
    CHECK(o->write_file(m, "/dados.bin", a, n1), "write 300 KB");
    CHECK(o->write_file(m, "/dados.bin", b, n2), "substitui por 1,5 MB");
    CHECK(o->write_file(m, "/Pasta Acentuação/vazio.txt", "", 0), "arquivo vazio");
    CHECK(o->write_file(m, "/pequeno.bin", c, n3), "write 5 KB");
    CHECK(o->write_file(m, "/pequeno.bin", c, n3 - 1), "trunca 1 byte");
    /* renomear, mover, apagar */
    CHECK(o->write_file(m, "/apagar.txt", "tchau", 5), "apagar.txt");
    CHECK(o->remove(m, "/apagar.txt"), "rm");
    CHECK(!o->remove(m, "/apagar.txt"), "rm de novo falha");
    CHECK(o->rename(m, "/novo.txt", "/renomeado.txt"), "rename");
    CHECK(o->rename(m, "/renomeado.txt", "/Pasta Acentuação/movido.txt"), "move para pasta");
    CHECK(o->rename(m, "/Pasta Acentuação/Sub", "/Sub Movida"), "move pasta para a raiz");
    CHECK(!o->rename(m, "/Sub Movida", "/Sub Movida/dentro"), "pasta para dentro dela mesma falha");
    if (hfs) CHECK(o->rename(m, "/pequeno.bin", "/PEQUENO.bin") || !hfs, "só a caixa");
    CHECK(o->mkdir(m, "/Remover"), "mkdir Remover");
    CHECK(o->mkdir(m, "/Remover/a") && o->mkdir(m, "/Remover/a/b"), "mkdir árvore");
    for (int i = 0; i < 20; i++) { snprintf(p, sizeof p, "/Remover/a/b/f%d.txt", i); o->write_file(m, p, "z", 1); }
    CHECK(o->remove(m, "/Remover"), "rm -r");
    for (int i = 0; i < 300; i += 2) { snprintf(p, sizeof p, "/Muitos/arquivo número %03d com um nome bem comprido para encher nós.txt", i); if (!o->remove(m, p)) { CHECK(0, "rm %d", i); break; } }
    CHECK(o->sync(m), "sync");
    free(a); free(b); free(c);
}

static void phase2(dmount_t *m, bool hfs) {
    printf("[%s] releitura após remontar\n", m->volname);
    u8 *b = pattern(1500000, 2), *c = pattern(5000, 3);
    CHECK(same(m, "/dados.bin", b, 1500000), "dados.bin 1,5 MB");
    CHECK(same(m, hfs ? "/PEQUENO.bin" : "/pequeno.bin", c, 4999), "pequeno.bin truncado");
    CHECK(same(m, "/Pasta Acentuação/movido.txt", (const u8 *)"olá, mOS\n", 10), "movido.txt");
    CHECK(same(m, "/Sub Movida/arquivo com nome longo é ç ã.txt", (const u8 *)"acentos", 7), "nome com acentos");
    CHECK(same(m, "/Pasta Acentuação/vazio.txt", (const u8 *)"", 0), "vazio");
    CHECK(same(m, "/seed.txt", (const u8 *)"semente do volimg\n", 18), "seed.txt intacto");
    CHECK(!lookup(m, "/Remover") && !lookup(m, "/novo.txt") && !lookup(m, "/apagar.txt"), "removidos sumiram");
    CHECK(count(m, "/Muitos") == 150, "150 em Muitos (lidos %d)", count(m, "/Muitos"));
    CHECK(same(m, "/Muitos/arquivo número 151 com um nome bem comprido para encher nós.txt", (const u8 *)"conteúdo 151", 13), "Muitos/151");
    free(b); free(c);
}

/* HFS+: enche o volume, abre buracos e grava um arquivo fragmentado (> 8 extents: overflow) */
static void frag_test(dmount_t *m) {
    const dfs_ops_t *o = m->ops;
    printf("[%s] fragmentação e extents overflow\n", m->volname);
    CHECK(o->mkdir(m, "/Enche"), "mkdir Enche");
    char p[128];
    u8 *blk = pattern(65536, 9);
    int n = 0;
    for (; n < 2000; n++) { snprintf(p, sizeof p, "/Enche/%04d", n); if (!o->write_file(m, p, blk, 65536)) break; }
    CHECK(n > 50, "encheu com %d arquivos", n);
    for (int i = 0; i < n; i += 2) { snprintf(p, sizeof p, "/Enche/%04d", i); o->remove(m, p); }
    u64 big = (u64)(n / 2 - 4) * 65536;
    u8 *a = pattern(big, 77);
    CHECK(o->write_file(m, "/fragmentado.bin", a, big), "arquivo fragmentado de %llu KB", big >> 10);
    CHECK(!o->write_file(m, "/nao-cabe.bin", a, big), "sem espaço falha sem estragar nada");
    char vn[64];
    snprintf(vn, sizeof vn, "%s", m->volname);
    unmount_all();
    mount_all();
    for (dmount_t *x = dfs_mounts(); x; x = x->next) if (!strcmp(x->volname, vn)) m = x;
    CHECK(same(m, "/fragmentado.bin", a, big), "relê o fragmentado");
    o = m->ops;
    /* o fragmentado fica na imagem: volimg check confere os extents overflow dele */
    CHECK(o->remove(m, "/Enche"), "apaga Enche");
    CHECK(o->write_file(m, "/copia.bin", a, big / 2), "o espaço voltou");
    CHECK(o->remove(m, "/copia.bin"), "rm copia");
    free(a); free(blk);
}

/* --crash <volimg.exe>: para k = 1, 2, ... corta a energia depois de k escritas no meio de uma
 * operação, remonta (o journal é repetido) e confere: o volume tem que estar íntegro e a operação
 * tem que ter acontecido inteira ou não ter acontecido. */
static int crash_test(const char *path, const char *checker) {
    int nv = mount_all();
    dmount_t *m = nv ? nth(0) : NULL;
    if (!m || !m->writable || strcmp(m->ops->name, "HFS+")) { printf("--crash pede um volume HFS+ com journal\n"); return 1; }
    char p[160];
    m->ops->mkdir(m, "/Base");
    for (int i = 0; i < 120; i++) { snprintf(p, sizeof p, "/Base/arquivo base número %03d com nome comprido.txt", i); m->ops->write_file(m, p, "base", 4); }
    unmount_all();
    u8 *base = malloc(img_size);
    memcpy(base, img, img_size);
    u8 *data = pattern(200000, 5);
    int bad = 0, k;
    for (k = 1; k < 400; k++) {
        memcpy(img, base, img_size);
        mount_all();
        m = nth(0);
        nwrites = 0;
        crash_after = k;
        /* operação com split de nós do catálogo, alocação e mudança de pasta */
        bool done = m->ops->write_file(m, "/Base/arquivo base número 050 e meio, novo.txt", data, 200000) &&
                    m->ops->rename(m, "/Base/arquivo base número 010 com nome comprido.txt", "/movido.txt");
        long used = nwrites;
        unmount_all();                                     /* as escritas do desmontar também somem */
        crash_after = -1;
        if (used <= k) { printf("crash: operação completa com %ld escritas; %d pontos de queda testados\n", used, k - 1); break; }
        (void)done;
        mount_all();                                       /* repete o journal */
        m = nth(0);
        bool has_new = same(m, "/Base/arquivo base número 050 e meio, novo.txt", data, 200000);
        bool any_new = lookup(m, "/Base/arquivo base número 050 e meio, novo.txt") != NULL;
        if (any_new && !has_new) { printf("crash k=%d: arquivo novo pela metade\n", k); bad++; }
        bool mv_old = lookup(m, "/Base/arquivo base número 010 com nome comprido.txt") != NULL, mv_new = lookup(m, "/movido.txt") != NULL;
        if (mv_old == mv_new) { printf("crash k=%d: rename pela metade (antigo %d, novo %d)\n", k, mv_old, mv_new); bad++; }
        unmount_all();
        FILE *f = fopen(path, "wb");
        fwrite(img, 1, img_size, f);
        fclose(f);
        char cmd[1024];
        char chk[512];
        snprintf(chk, sizeof chk, "%s", checker);
        for (char *q = chk; *q; q++) if (*q == '/') *q = '\\';   /* o cmd.exe não aceita "a/b.exe" */
        /* o cmd /c tira o 1º e o último ": um par extra em volta de tudo */
        snprintf(cmd, sizeof cmd, "\"\"%s\" check \"%s\" > \"%s.check.txt\"\"", chk, path, path);
        if (system(cmd) != 0) { printf("crash k=%d: volimg check acusou erros\n", k); bad++; }
    }
    free(base);
    free(data);
    printf("crash: %d problema(s)\n", bad);
    return bad ? 1 : 0;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc < 2) { fprintf(stderr, "uso: voltest <imagem.img> [--mount]\n"); return 2; }
    FILE *f = fopen(argv[1], "rb");
    if (!f) { perror(argv[1]); return 1; }
    fseek(f, 0, SEEK_END);
    img_size = (u64)_ftelli64(f);
    fseek(f, 0, SEEK_SET);
    img = malloc(img_size);
    fread(img, 1, img_size, f);
    fclose(f);
    dfs_init();
    if (argc > 3 && !strcmp(argv[2], "--crash")) return crash_test(argv[1], argv[3]);
    int nv = mount_all();
    printf("%d volume(s)\n", nv);
    if (argc > 2 && !strcmp(argv[2], "--mount")) {
        for (int i = 0; i < nv; i++) {
            dmount_t *m = nth(i);
            dnode_t *s = lookup(m, "/seed.txt");
            u64 len = 0;
            u8 *b = s ? dfs_read_all(s, 1 << 20, &len) : NULL;
            printf("[%s] gravável=%d seed.txt=\"%.*s\"\n", m->volname, m->writable, (int)len, b ? (char *)b : "");
            kfree(b);
        }
        unmount_all();
    } else {
        int nw = 0;
        char names[16][64];
        for (int i = 0; i < nv; i++) {
            dmount_t *m = nth(i);
            snprintf(names[i], sizeof names[i], "%s", m->volname);
            CHECK(m->writable, "%s gravável", m->volname);
            if (!m->writable) continue;
            nw++;
            phase1(m, strcmp(m->ops->name, "FAT") != 0);
        }
        unmount_all();
        mount_all();
        for (int i = 0; i < nv; i++) {
            dmount_t *m = nth(i);
            if (m->writable) phase2(m, strcmp(m->ops->name, "FAT") != 0);
        }
        for (int i = 0; i < nv; i++) {
            dmount_t *m = nth(i);
            if (m->writable && strcmp(m->ops->name, "FAT")) { frag_test(m); break; }
        }
        unmount_all();
        printf("%d volume(s) gravável(is), %d verificações, %d falha(s)\n", nw, checks, fails);
    }
    f = fopen(argv[1], "wb");
    fwrite(img, 1, img_size, f);
    fclose(f);
    printf("alocações vivas no fim: %lld\n", g_live_allocs);
    return fails ? 1 : 0;
}
