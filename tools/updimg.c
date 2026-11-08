/*
 * mOS — ferramenta de teste do atualizador (kernel/fs/update.c), no host, com o driver FAT real do kernel.
 *
 *   updimg instalar <mOS.img>
 *       Transforma uma imagem Live (build.ps1) num "sistema instalado" sem passar pelo assistente: estado INSTALADO,
 *       o manifesto do payload vira /mOS/Sistema/Atualizacao/instalado.txt (como o instalador grava), a pasta Instalacao
 *       sai, e entram uma conta "teste" SEM senha e as preferências de idioma/teclado. Só para testes no QEMU.
 *   updimg ver <mOS.img> <caminho>
 *       Mostra tamanho e SHA-256 de um arquivo do volume (conferir o que a atualização trocou).
 *   updimg trocar <disco.img> <volimg.exe>
 *       fat_replace com queda de energia simulada: para k = 1, 2, ... corta as escritas depois da k-ésima no meio de
 *       "trocar a.bin (existe) e b.bin (novo) pelas cópias de U/", remonta, confere que cada arquivo é a versão velha OU a
 *       nova inteira, repete a aplicação (como o boot faz) e confere o resultado final e a estrutura (volimg check).
 *       O disco vem de: volimg mkdisk <disco.img> 64 fat32:T:40
 *
 * Compilar:
 *   zig cc -target x86_64-windows-gnu -O1 -DMOS_HOST_TEST -Ikernel/include -Itools tools/updimg.c
 *     kernel/fs/{apfs,blkdev,bzip2,dfs,dmg,hfsplus,hfsplus_wr,inflate,lzfse,xz,fat}.c kernel/net/crypto.c -o build/updimg.exe
 */
#include "fs.h"
void sha256(const void *data, usize len, u8 out[32]);   /* kernel/net/crypto.c (crypto.h traz o shim do TLS) */
#include <time.h>

long long g_live_allocs, g_live_bytes;
void rtc_now(datetime_t *dt) {
    time_t t = time(NULL);
    struct tm *m = gmtime(&t);
    dt->year = m->tm_year + 1900; dt->month = m->tm_mon + 1; dt->day = m->tm_mday;
    dt->hour = m->tm_hour; dt->minute = m->tm_min; dt->second = m->tm_sec; dt->weekday = m->tm_wday;
}
u32 rand32(void) { return (u32)rand() << 16 ^ (u32)rand(); }
void dfs_publish_new(dmount_t *before) { (void)before; }
u64 timer_ticks(void) { return (u64)clock(); }

static u8 *img;
static u64 img_size;
static long crash_after = -1, nwrites;
static bool rd(blkdev_t *d, u64 off, void *buf, u64 len) { memcpy(buf, img + off, len); return true; }
static bool wr(blkdev_t *d, u64 off, const void *buf, u64 len) {
    nwrites++;
    if (crash_after >= 0 && nwrites > crash_after) return true;     /* depois da queda: as escritas somem */
    memcpy(img + off, buf, len);
    return true;
}

static bool load(const char *p) {
    FILE *f = fopen(p, "rb");
    if (!f) { perror(p); return false; }
    fseek(f, 0, SEEK_END);
    img_size = (u64)_ftelli64(f);
    fseek(f, 0, SEEK_SET);
    img = malloc(img_size);
    bool ok = img && fread(img, 1, img_size, f) == img_size;
    fclose(f);
    return ok;
}
static bool save(const char *p) {
    FILE *f = fopen(p, "wb");
    bool ok = f && fwrite(img, 1, img_size, f) == img_size;
    if (f) fclose(f);
    return ok;
}

/* 1ª partição da GPT (a do mOS / a do volimg mkdisk) */
static fatfs_t *mount_first(void) {
    static blkdev_t disk;
    disk.size = img_size; disk.read = rd; disk.write = wr;
    const u8 *e = img + 2 * 512;                                    /* 1ª entrada da tabela (LBA 2) */
    u64 first = 0, last = 0;
    for (int i = 7; i >= 0; i--) { first = first << 8 | e[32 + i]; last = last << 8 | e[40 + i]; }
    if (memcmp(img + 512, "EFI PART", 8) || !first || last < first) { fprintf(stderr, "sem GPT válida\n"); return NULL; }
    blkdev_t *p = blk_slice(&disk, first * 512, (last - first + 1) * 512);
    fatfs_t *fs = p ? fat_mount(p) : NULL;
    if (fs) fat_enable_rw(fs);
    return fs;
}

static bool put(fatfs_t *fs, const char *path, const void *d, u64 n) {
    char tmp[1100];
    snprintf(tmp, sizeof tmp, "%s", path);
    for (char *p = tmp + 1; *p; p++) {
        if (*p != '/') continue;
        *p = 0;
        bool dir;
        if (!(fat_stat(fs, tmp, &dir, NULL) && dir) && !fat_mkdir(fs, tmp)) return false;
        *p = '/';
    }
    return fat_write_file(fs, path, d, n);
}

static int cmd_instalar(const char *file) {
    if (!load(file)) return 1;
    fatfs_t *fs = mount_first();
    if (!fs) return 1;
    u64 n = 0;
    u8 *m = fat_read_file(fs, "/mOS/Sistema/Instalacao/manifesto.txt", &n);
    if (!m) { fprintf(stderr, "a imagem não tem o manifesto do payload (não é uma imagem Live do build.ps1)\n"); return 1; }
    static const char conta[] = "NomeCompleto=Teste\nAdmin=1\nFormato=2\nAlgoritmo=nenhum\nSal=\nHash=\nIteracoes=0\n";
    static const char prefs[] = "idioma=pt\nteclado=abnt2\n";
    bool ok = put(fs, "/mOS/Sistema/Atualizacao/instalado.txt", m, n) &&
              fat_remove(fs, "/mOS/Sistema/Instalacao") &&
              put(fs, "/mOS/Sistema/Biblioteca/estado-sistema", "INSTALADO\n", 10) &&
              put(fs, "/mOS/Sistema/Biblioteca/Usuarios/teste.conta", conta, sizeof conta - 1) &&
              put(fs, "/mOS/Sistema/Biblioteca/Preferencias/com.mos.sistema.cfg", prefs, sizeof prefs - 1) &&
              fat_sync(fs);
    if (!ok || !save(file)) { fprintf(stderr, "falha ao gravar a imagem\n"); return 1; }
    char *nl = memchr(m, '\n', n);
    printf("updimg: %s agora é um sistema INSTALADO (%.*s), conta \"teste\" sem senha\n", file, nl ? (int)(nl - (char *)m) : 0, (char *)m);
    return 0;
}

static int cmd_ver(const char *file, const char *path) {
    if (!load(file)) return 1;
    fatfs_t *fs = mount_first();
    u64 n = 0;
    u8 *d = fs ? fat_read_file(fs, path, &n) : NULL;
    if (!d) { printf("%s: ausente\n", path); return 1; }
    u8 h[32];
    sha256(d, n, h);
    printf("%s: %llu bytes, sha256 ", path, (unsigned long long)n);
    for (int i = 0; i < 32; i++) printf("%02x", h[i]);
    printf("\n");
    return 0;
}

/* ------------------------------------------------------------------ queda de energia no meio das trocas */
static u8 *pattern(u64 n, u8 seed) { u8 *p = malloc(n); for (u64 i = 0; i < n; i++) p[i] = (u8)(seed + i * 31 + (i >> 9)); return p; }
static int is(fatfs_t *fs, const char *path, const u8 *want, u64 n) {
    u64 len = 0;
    u8 *d = fat_read_file(fs, path, &len);
    int r = d && len == n && !memcmp(d, want, n);
    kfree(d);
    return r;
}
static bool exists(fatfs_t *fs, const char *p) { bool dir; return fat_stat(fs, p, &dir, NULL); }

static int cmd_trocar(const char *file, const char *volimg) {
    if (!load(file)) return 1;
    fatfs_t *fs = mount_first();
    if (!fs) return 1;
    const u64 na = 9000, nb = 23000, nc = 5000;                     /* vários clusters, tamanhos diferentes */
    u8 *A = pattern(na, 1), *B = pattern(nb, 77), *C = pattern(nc, 150);
    if (!put(fs, "/sis/a.bin", A, na) || !put(fs, "/U/novos/1", B, nb) || !put(fs, "/U/novos/2", C, nc) || !fat_sync(fs)) {
        fprintf(stderr, "preparação falhou\n"); return 1;
    }
    u8 *base = malloc(img_size);
    memcpy(base, img, img_size);
    /* número de escritas de uma passada completa */
    nwrites = 0; crash_after = -1;
    fat_replace(fs, "/U/novos/1", "/sis/a.bin");
    fat_replace(fs, "/U/novos/2", "/sis/b.bin");
    long total = nwrites;
    int bad = 0, tolerated = 0;
    char tmp[600], cmd[1400];
    snprintf(tmp, sizeof tmp, "%s.queda.img", file);
    for (long k = 0; k <= total; k++) {
        memcpy(img, base, img_size);
        fs = mount_first();
        nwrites = 0; crash_after = k;
        fat_replace(fs, "/U/novos/1", "/sis/a.bin");
        fat_replace(fs, "/U/novos/2", "/sis/b.bin");
        crash_after = -1;
        fs = mount_first();                                         /* "reboot": monta de novo do que chegou ao disco */
        int a_old = is(fs, "/sis/a.bin", A, na), a_new = is(fs, "/sis/a.bin", B, nb);
        int b_none = !exists(fs, "/sis/b.bin"), b_new = is(fs, "/sis/b.bin", C, nc);
        if (!(a_old || a_new)) { printf("k=%ld: a.bin não é nem a versão velha nem a nova\n", k); bad++; }
        if (!(b_none || b_new)) { printf("k=%ld: b.bin pela metade\n", k); bad++; }
        /* a retomada do boot: trocar de novo o que ainda tem cópia em novos/ */
        if (exists(fs, "/U/novos/1")) fat_replace(fs, "/U/novos/1", "/sis/a.bin");
        if (exists(fs, "/U/novos/2")) fat_replace(fs, "/U/novos/2", "/sis/b.bin");
        fat_sync(fs);
        if (!is(fs, "/sis/a.bin", B, nb) || !is(fs, "/sis/b.bin", C, nc) || exists(fs, "/U/novos/1") || exists(fs, "/U/novos/2")) {
            printf("k=%ld: depois da retomada o resultado está errado\n", k); bad++;
        }
        save(tmp);
        snprintf(cmd, sizeof cmd, "\"\"%s\" check \"%s\" > \"%s.txt\" 2>&1\"", volimg, tmp, tmp);
        int rc = system(cmd);
        if (rc) {
            /* toleráveis: clusters perdidos da versão antiga (queda entre a troca e o sync da FAT) e a 2ª cópia da FAT
             * atrasada (o fat_sync grava a 1ª e depois a 2ª, em qualquer escrita). Nenhum dos dois afeta o conteúdo. */
            char tp[620], ln[300];
            snprintf(tp, sizeof tp, "%s.txt", tmp);
            FILE *cf = fopen(tp, "r");
            int real = 0;
            while (cf && fgets(ln, sizeof ln, cf))
                if (strstr(ln, "ERRO:") && !strstr(ln, "perdido") && !strstr(ln, "da FAT difere")) { real++; printf("k=%ld: %s", k, ln); }
            if (cf) fclose(cf);
            if (real) bad++;
            else { tolerated++; printf("k=%ld/%ld: a.bin %s, b.bin %s; retomada ok; só clusters perdidos/cópia da FAT atrasada\n", k, total, a_new ? "nova" : "velha", b_new ? "nova" : "ausente"); }
        }
        else printf("k=%ld/%ld: a.bin %s, b.bin %s; retomada ok, estrutura ok\n", k, total, a_new ? "nova" : "velha", b_new ? "nova" : "ausente");
    }
    remove(tmp);
    { char tp[620]; snprintf(tp, sizeof tp, "%s.txt", tmp); remove(tp); }
    printf("trocar: %ld pontos de queda, %d problema(s)\n", total + 1, bad);
    return bad ? 1 : 0;
}

int main(int argc, char **argv) {
    setvbuf(stdout, NULL, _IONBF, 0);
    if (argc >= 3 && !strcmp(argv[1], "instalar")) return cmd_instalar(argv[2]);
    if (argc >= 4 && !strcmp(argv[1], "ver")) return cmd_ver(argv[2], argv[3]);
    if (argc >= 4 && !strcmp(argv[1], "trocar")) return cmd_trocar(argv[2], argv[3]);
    fprintf(stderr, "uso: updimg instalar <mOS.img> | ver <mOS.img> <caminho> | trocar <disco.img> <volimg.exe>\n");
    return 2;
}
