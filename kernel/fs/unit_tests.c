/* mOS — testes de unidade do kernel. Compilados SÓ com -DMOS_UNIT_TESTS (builds de teste; o kernel de produção não tem este
 * arquivo). Não destrutivos por construção: só usam discos em RAM; a verificação da política de escrita nos discos reais
 * grava de volta exatamente o que acabou de ler (idempotente), então mesmo uma política quebrada não altera nenhum byte. */
#ifdef MOS_UNIT_TESTS
#include "kernel.h"
#include "storage.h"
#include "partscan.h"
#include "origin.h"
#include "crypto.h"
#include "account.h"
#include "vfs.h"
#include "install.h"
#include "launchsvc.h"

static int n_ok, n_bad;
#define CHECK(c, what) do { if (c) n_ok++; else { n_bad++; kprintf("unit: FALHOU %s (linha %d)\n", what, __LINE__); } } while (0)

static int hv(char c) { return c <= '9' ? c - '0' : c - 'a' + 10; }
static int unhexs(const char *h, u8 *o) { int n = 0; for (; h[0] && h[1]; h += 2) o[n++] = (u8)(hv(h[0]) << 4 | hv(h[1])); return n; }
static bool eqhex(const u8 *b, int n, const char *h) { u8 t[128]; return unhexs(h, t) == n && !memcmp(t, b, n); }

/* ------------------------------------------------------------------ HMAC / PBKDF2 (vetores gerados com hashlib/hmac do Python) */
static const struct { const char *k; int kl; const char *m; int ml; const char *mac; } HM[] = {
    {"0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b0b", 20, "4869205468657265", 8, "b0344c61d8db38535ca8afceaf0bf12b881dc200c9833da726e9376c2e32cff7"},
    {"4a656665", 4, "7768617420646f2079612077616e7420666f72206e6f7468696e673f", 28, "5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843"},
    {"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", 20, "dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd", 50, "773ea91e36800e46854db8ebd09181a72959098b3ef8c122d9635514ced565fe"},
    {"", 0, "", 0, "b613679a0814d9ec772f95d778c35fc5ff1697c493715653c6c712144292c5ad"},
};
static const struct { const char *pw; int pl; const char *salt; int sl; u32 c; int n; const char *dk; } PB[] = {
    {"70617373776f7264", 8, "73616c74", 4, 1, 32, "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b"},
    {"70617373776f7264", 8, "73616c74", 4, 2, 32, "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43"},
    {"70617373776f7264", 8, "73616c74", 4, 4096, 32, "c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a"},
    {"70617373776f726450415353574f524470617373776f7264", 24, "73616c7453414c5473616c7453414c5473616c7453414c5473616c7453414c5473616c74", 36, 4096, 40, "348c89dbcbd32b2f32d814b8116e84cf2b17347ebc1800181c4e2a1fb8dd53e1c635518c7dac47e9"},
    {"7061737300776f7264", 9, "7361006c74", 5, 4096, 16, "89b69d0516f829893c696226650a8687"},
    {"", 0, "73616c74", 4, 3, 32, "5ddf839afa2d5fb4be56e1a0f48917617559bef61ec122bfca1c7f75ac8f401d"},
    {"73656e6861736567757261313233", 14, "000102030405060708090a0b0c0d0e0f", 16, 100000, 32, "f78abc6632dcaccb58ee19412738d98bf12bd01819a65ec37aa29d7cad2feeaa"},
};

static void test_crypto(void) {
    u8 k[160], m[160], out[64];
    for (u32 i = 0; i < ARRAY_LEN(HM); i++) {
        int kl = unhexs(HM[i].k, k), ml = unhexs(HM[i].m, m);
        hmac(HASH_SHA256, k, kl, m, ml, out);
        CHECK(eqhex(out, 32, HM[i].mac), "HMAC-SHA256 contra vetor");
    }
    u8 pw[128], salt[64], dk[64];
    for (u32 i = 0; i < ARRAY_LEN(PB); i++) {
        int pl = unhexs(PB[i].pw, pw), sl = unhexs(PB[i].salt, salt);
        u64 t0 = timer_ticks();
        pbkdf2_hmac_sha256(pw, pl, salt, sl, PB[i].c, dk, PB[i].n);
        CHECK(eqhex(dk, PB[i].n, PB[i].dk), "PBKDF2-HMAC-SHA256 contra vetor");
        if (PB[i].c >= 100000) kprintf("unit: PBKDF2 %u iterações em %llu ms\n", PB[i].c, timer_ticks() - t0);
    }
    /* chave longa (>64) e idêntica a hmac(): consistência cruzada entre as duas implementações */
    u8 big[200];
    for (int i = 0; i < 200; i++) big[i] = (u8)(i * 7 + 3);
    u8 d1[32], d2[32], one[4] = {0, 0, 0, 1}, msg[10 + 4];
    memcpy(msg, "saltsalts!", 10); memcpy(msg + 10, one, 4);
    hmac(HASH_SHA256, big, 200, msg, 14, d1);
    pbkdf2_hmac_sha256(big, 200, (const u8 *)"saltsalts!", 10, 1, d2, 32);
    CHECK(!memcmp(d1, d2, 32), "PBKDF2 c=1 == HMAC(sal||INT(1)) com chave longa");
}

/* ------------------------------------------------------------------ entropia (sanidade, NÃO é certificação) */
static void test_entropy(void) {
    rng_status_t st;
    u8 buf[1024], a[16], b[16];
    crypto_random(buf, sizeof buf);
    crypto_rng_status(&st);
    CHECK(st.seeded, "RNG semeado");
    int ones = 0;
    for (int i = 0; i < 1024; i++) for (int k = 0; k < 8; k++) ones += (buf[i] >> k) & 1;
    CHECK(ones > 3900 && ones < 4300, "monobit: proporção de bits 1 plausível em 8192 bits");
    int runs = 1;
    for (int i = 1; i < 8192; i++) runs += ((buf[i / 8] >> (i % 8)) & 1) != ((buf[(i - 1) / 8] >> ((i - 1) % 8)) & 1);
    CHECK(runs > 3900 && runs < 4300, "runs: número de corridas plausível");
    crypto_random(a, 16); crypto_random(b, 16);
    CHECK(memcmp(a, b, 16) != 0, "saídas consecutivas diferentes");
    kprintf("unit: RNG: RDRAND %d/8, RDSEED %d/4, jitter %d, hardware %s (sanidade apenas; entropia NÃO validada)\n",
            st.rdrand_ok, st.rdseed_ok, st.jitter_samples, crypto_rng_hw() ? "sim" : "NÃO");
}

/* ------------------------------------------------------------------ discos em RAM + GPT/MBR */
typedef struct { u8 *mem; } ram_t;
static bool ram_read(blkdev_t *d, u64 off, void *buf, u64 len) { memcpy(buf, ((ram_t *)d->ctx)->mem + off, len); return true; }
static bool ram_write(blkdev_t *d, u64 off, const void *buf, u64 len) { memcpy(((ram_t *)d->ctx)->mem + off, buf, len); return true; }
static blkdev_t *ram_new(u64 mib) {
    blkdev_t *d = kcalloc(1, sizeof *d);
    ram_t *r = kcalloc(1, sizeof *r);
    if (!d || !r) return NULL;
    r->mem = kcalloc(1, mib << 20);
    if (!r->mem) return NULL;
    d->size = mib << 20; d->read = ram_read; d->write = ram_write; d->ctx = r; d->lbs = 512;
    return d;
}
static void ram_free(blkdev_t *d) { kfree(((ram_t *)d->ctx)->mem); kfree(d->ctx); kfree(d); }
static void put32(u8 *p, u32 v) { p[0] = (u8)v; p[1] = (u8)(v >> 8); p[2] = (u8)(v >> 16); p[3] = (u8)(v >> 24); }
static void put64(u8 *p, u64 v) { put32(p, (u32)v); put32(p + 4, (u32)(v >> 32)); }

/* reescreve os CRCs de um GPT em RAM depois de editar as entradas */
static void gpt_fix(u8 *mem, u64 nsec) {
    u8 *h = mem + 512, *e = mem + 1024, *bh = mem + (nsec - 1) * 512;
    put32(h + 88, crc32_calc(e, 128 * 128));
    put32(h + 16, 0); put32(h + 16, crc32_calc(h, 92));
    memcpy(mem + (nsec - 33) * 512, e, 128 * 128);
    memcpy(bh, h, 92); put64(bh + 24, nsec - 1); put64(bh + 32, 1); put64(bh + 72, nsec - 33);
    put32(bh + 16, 0); put32(bh + 16, crc32_calc(bh, 92));
}

static void test_partscan(void) {
    blkdev_t *d = ram_new(48);
    if (!d) { CHECK(false, "RAM disk de 48 MiB"); return; }
    ptable_t *t = kmalloc(sizeof *t);
    u8 *mem = ((ram_t *)d->ctx)->mem;
    u64 nsec = d->size / 512;
    CHECK(partscan(d, t) && t->kind == PT_NONE && !t->whole_volume, "disco em branco => PT_NONE");
    u64 off = 0, size = 0;
    CHECK(gpt_create_single_esp(d, "teste", &off, &size) && off == 1048576, "gpt_create_single_esp em disco RAM");
    CHECK(partscan(d, t) && t->kind == PT_GPT && t->n == 1 && t->backup_ok && t->p[0].esp && t->p[0].first == 2048, "GPT criada é lida e validada (inclui backup)");
    CHECK(t->p[0].number == 1 && (t->p[0].last - t->p[0].first + 1) * 512 == size, "geometria da partição confere com o retorno");
    u8 snap1[32]; memcpy(snap1, t->snap, 32);
    CHECK(partscan(d, t) && !memcmp(snap1, t->snap, 32), "resumo (snap) estável entre leituras");
    /* CRC das entradas */
    mem[1024 + 40] ^= 0x01;
    CHECK(partscan(d, t) && t->kind == PT_BAD, "CRC de entradas errado => PT_BAD");
    mem[1024 + 40] ^= 0x01;
    /* cabeçalho */
    mem[512 + 20] ^= 0x01;
    CHECK(partscan(d, t) && t->kind == PT_BAD, "CRC de cabeçalho errado => PT_BAD");
    mem[512 + 20] ^= 0x01;
    CHECK(partscan(d, t) && t->kind == PT_GPT, "restaurado volta a ser válido");
    /* backup ausente: o primário é válido, mas o backup_ok fica falso */
    u8 save[512]; memcpy(save, mem + (nsec - 1) * 512, 512);
    memset(mem + (nsec - 1) * 512, 0, 512);
    CHECK(partscan(d, t) && t->kind == PT_GPT && !t->backup_ok, "sem cabeçalho de backup => backup_ok falso");
    memcpy(mem + (nsec - 1) * 512, save, 512);
    /* sobreposição: duas entradas que se cruzam */
    u8 *e = mem + 1024;
    memcpy(e + 128, e, 128);                       /* a 2ª entrada é cópia da 1ª */
    e[128 + 16] ^= 0xFF;                           /* GUID único diferente */
    gpt_fix(mem, nsec);
    CHECK(partscan(d, t) && t->kind == PT_BAD, "partições sobrepostas => PT_BAD");
    memset(e + 128, 0, 128);
    gpt_fix(mem, nsec);
    /* partição fora dos limites usáveis */
    put64(e + 40, nsec);                           /* last além do disco */
    gpt_fix(mem, nsec);
    CHECK(partscan(d, t) && t->kind == PT_BAD, "partição fora do disco => PT_BAD");
    ram_free(d);

    /* MBR */
    d = ram_new(48);
    mem = ((ram_t *)d->ctx)->mem;
    mem[510] = 0x55; mem[511] = 0xAA;
    mem[446 + 4] = 0xEF; put32(mem + 446 + 8, 2048); put32(mem + 446 + 12, 40000);
    put32(mem + 440, 0x12345678);
    CHECK(partscan(d, t) && t->kind == PT_MBR && t->n == 1 && t->p[0].esp && t->mbr_sig == 0x12345678, "MBR com uma partição 0xEF");
    mem[446 + 16 + 4] = 0x05; put32(mem + 446 + 16 + 8, 50000); put32(mem + 446 + 16 + 12, 10000);
    CHECK(partscan(d, t) && t->kind == PT_MBR && t->has_extended, "MBR com partição estendida é sinalizada");
    mem[446 + 32 + 4] = 0x83; put32(mem + 446 + 32 + 8, 3000); put32(mem + 446 + 32 + 12, 5000);   /* cruza a 1ª */
    CHECK(partscan(d, t) && t->kind == PT_BAD, "MBR com partições sobrepostas => PT_BAD");
    memset(mem + 446 + 32, 0, 16);
    mem[446 + 16] = 0x42;
    CHECK(partscan(d, t) && t->kind == PT_BAD, "MBR com byte de estado inválido => PT_BAD");
    memset(mem + 446 + 16, 0, 16);
    mem[446 + 4] = 0xEE;
    CHECK(partscan(d, t) && t->kind == PT_BAD, "MBR protetor (0xEE) sem GPT => PT_BAD");
    ram_free(d);
    kfree(t);
}

/* particionar: GPT com ESP + Dados, espaços livres e partição nova acrescentada sem mexer nas outras */
static void test_gpt_layout(void) {
    blkdev_t *d = ram_new(64);
    if (!d) { CHECK(false, "RAM disk de 64 MiB"); return; }
    ptable_t *t = kmalloc(sizeof *t);
    u64 nsec = d->size / 512;
    gpt_new_t two[2] = {{2048, 2048 + 40960 - 1, GPT_TYPE_ESP, "mOS"}, {2048 + 40960, 2048 + 40960 + 20480 - 1, GPT_TYPE_DATA, "Dados"}};
    CHECK(gpt_create(d, two, 2), "gpt_create com ESP + Dados");
    CHECK(partscan(d, t) && t->kind == PT_GPT && t->n == 2 && t->backup_ok && t->p[0].esp && !t->p[1].esp, "as duas partições são lidas, com backup");
    CHECK(t->first_usable == 34 && t->last_usable == nsec - 34, "limites usáveis expostos pelo partscan");
    gap_t g[4];
    int ng = part_free_gaps(t, g, 4, 2048);
    CHECK(ng == 1 && g[0].first == 2048 + 40960 + 20480 && g[0].first % 2048 == 0 && (g[0].last + 1) % 2048 == 0 && g[0].last <= nsec - 34,
          "um espaço livre depois de Dados, alinhado a 1 MiB");
    u8 e2[128];
    blk_read(d, 1024 + 128, e2, 128);
    gpt_new_t np = {g[0].first, g[0].first + 4096 - 1, GPT_TYPE_ESP, "mOS2"};
    int num = 0;
    CHECK(gpt_add_partition(d, &np, &num) && num == 3, "gpt_add_partition usa a 1ª entrada vazia");
    CHECK(partscan(d, t) && t->kind == PT_GPT && t->n == 3 && t->backup_ok && t->p[2].first == np.first && t->p[2].last == np.last, "GPT continua válida dos dois lados com a nova");
    u8 e2b[128];
    blk_read(d, 1024 + 128, e2b, 128);
    CHECK(!memcmp(e2, e2b, 128), "a entrada de Dados ficou byte a byte igual");
    gpt_new_t bad = {g[0].first + 1000, g[0].first + 9000, GPT_TYPE_ESP, "x"};
    CHECK(!gpt_add_partition(d, &bad, &num), "partição que cruza outra é recusada");
    gpt_new_t out = {np.last + 1, nsec, GPT_TYPE_ESP, "x"};
    CHECK(!gpt_add_partition(d, &out, &num), "partição fora dos limites é recusada");
    ram_free(d);
    kfree(t);
}

/* ------------------------------------------------------------------ caminho de dispositivo UEFI + identidade física */
static int node(u8 *o, u8 ty, u8 sub, const u8 *body, int bl) { o[0] = ty; o[1] = sub; o[2] = (u8)(bl + 4); o[3] = 0; memcpy(o + 4, body, bl); return bl + 4; }

static int hd_node(u8 *o, u32 no, u64 start, u64 size, const u8 *sig, u8 sigtype) {
    u8 b[38] = {0};
    put32(b, no); put64(b + 4, start); put64(b + 12, size);
    memcpy(b + 20, sig, 16); b[37] = sigtype;
    return node(o, 4, 1, b, 38);
}

static void test_origin(void) {
    u8 dp[256], sig[16];
    for (int i = 0; i < 16; i++) sig[i] = (u8)(0xA0 + i);
    int n = 0;
    u8 root[8] = {0x41, 0xD0, 0x0A, 0x03, 0, 0, 0, 0}, pci[2] = {0, 3}, usb[2] = {1, 0};   /* PciRoot(0)/Pci(3,0)/USB(1,0) */
    n += node(dp + n, 2, 1, root, 8);
    n += node(dp + n, 1, 1, pci, 2);
    n += node(dp + n, 3, 5, usb, 2);
    n += hd_node(dp + n, 1, 2048, 262112, sig, 2);
    n += node(dp + n, 0x7F, 0xFF, NULL, 0);
    devpath_t w;
    CHECK(origin_parse_devpath(dp, n, &w) && w.valid && w.phys.kind == PHYS_USB && w.phys.npci == 1 && w.phys.pci[0][0] == 3 && w.phys.pci[0][1] == 0, "caminho PciRoot/Pci/USB/HD/End é lido");
    CHECK(w.phys.nusb == 1 && w.phys.usb_port[0] == 1 + USB_PORT_BIAS && w.have_part && w.part_no == 1 && w.part_start == 2048 && w.part_size == 262112 && w.sig_type == 2 && !memcmp(w.sig, sig, 16), "porta USB (viés), nó HD e assinatura GPT");
    diskphys_t k;
    memset(&k, 0, sizeof k);
    k.kind = PHYS_USB; k.npci = 1; k.pci[0][0] = 3; k.pci[0][1] = 0; k.nusb = 1; k.usb_port[0] = 1 + USB_PORT_BIAS;
    CHECK(origin_phys_match(&k, &w.phys), "identidade física casa");
    k.usb_port[0]++;
    CHECK(!origin_phys_match(&k, &w.phys), "porta USB diferente não casa");
    k.usb_port[0]--; k.pci[0][1] = 1;
    CHECK(!origin_phys_match(&k, &w.phys), "função PCI diferente não casa");
    k.pci[0][1] = 0; k.npci = 0;
    CHECK(!origin_phys_match(&k, &w.phys), "cadeia PCI desconhecida nunca casa");
    k.npci = 1; k.kind = PHYS_SATA;
    CHECK(!origin_phys_match(&k, &w.phys), "tipo de barramento diferente não casa");
    /* SATA e NVMe */
    n = 0;
    u8 sata[6] = {2, 0, 0xFF, 0xFF, 0, 0}, pci2[2] = {2, 31};
    n += node(dp + n, 2, 1, root, 8); n += node(dp + n, 1, 1, pci2, 2); n += node(dp + n, 3, 0x12, sata, 6);
    n += hd_node(dp + n, 2, 43008, 245760, sig, 2); n += node(dp + n, 0x7F, 0xFF, NULL, 0);
    CHECK(origin_parse_devpath(dp, n, &w) && w.phys.kind == PHYS_SATA && w.phys.sata_port == 2 && w.phys.pci[0][0] == 31 && w.phys.pci[0][1] == 2 && w.part_no == 2, "caminho SATA");
    n = 0;
    u8 nv[12] = {1, 0, 0, 0}, pci3[2] = {0, 4};
    n += node(dp + n, 2, 1, root, 8); n += node(dp + n, 1, 1, pci3, 2); n += node(dp + n, 3, 0x17, nv, 12);
    n += hd_node(dp + n, 1, 2048, 1000, sig, 1); n += node(dp + n, 0x7F, 0xFF, NULL, 0);
    CHECK(origin_parse_devpath(dp, n, &w) && w.phys.kind == PHYS_NVME && w.phys.nsid == 1 && w.sig_type == 1, "caminho NVMe com assinatura MBR");
    /* malformados / não suportados => nunca "válido" */
    CHECK(!origin_parse_devpath(dp, 0, &w), "caminho vazio é recusado");
    CHECK(!origin_parse_devpath(dp, 10, &w), "caminho truncado é recusado");
    n = 0;
    n += node(dp + n, 2, 1, root, 8); n += node(dp + n, 1, 1, pci3, 2); n += node(dp + n, 3, 0x17, nv, 12);
    CHECK(!origin_parse_devpath(dp, n, &w), "caminho sem nó final é recusado");
    u8 atapi[4] = {0, 0, 0, 0};
    n = 0;
    n += node(dp + n, 2, 1, root, 8); n += node(dp + n, 1, 1, pci3, 2); n += node(dp + n, 3, 1, atapi, 4); n += node(dp + n, 0x7F, 0xFF, NULL, 0);
    CHECK(!origin_parse_devpath(dp, n, &w) || w.unsupported, "nó ATAPI não é aceito como disco");
    dp[2] = 2;   /* comprimento de nó menor que 4 */
    CHECK(!origin_parse_devpath(dp, n, &w), "nó com comprimento inválido é recusado");
}

/* ------------------------------------------------------------------ política de escrita nos discos REAIS (idempotente) */
static void test_policy(void) {
    int nd = storage_disk_count();
    CHECK(storage_grant_count() >= 0, "contagem de liberações");
    u8 *a = kmalloc(4096), *b = kmalloc(4096);
    int tested = 0;
    for (int i = 0; i < nd && a && b; i++) {
        blkdev_t *d = storage_disk(i);
        if (!d || !d->write || d->size < (4u << 20)) continue;
        if (blk_writable(d)) continue;      /* já há liberação (o disco do sistema instalado/persistente): o teste é só para discos sem nenhuma */
        u64 off = 1u << 20;                                        /* 1 MiB: dentro do disco, longe da tabela */
        CHECK(blk_read(d, off, a, 4096), "leitura do disco real");
        u64 before = storage_denied_writes();
        bool w1 = blk_write(d, off, a, 4096);                      /* grava o MESMO conteúdo: nada muda mesmo se a política falhar */
        CHECK(!w1, "escrita SEM liberação é recusada pelo driver");
        CHECK(storage_denied_writes() > before, "a recusa é contada");
        CHECK(!blk_writable(d), "blk_writable falso sem liberação");
        /* liberação de uma janela; escrita idempotente dentro e fora */
        if (storage_grant_write(d, off, 4096)) {
            CHECK(blk_writable(d), "blk_writable verdadeiro com liberação");
            CHECK(blk_write(d, off, a, 4096), "escrita DENTRO da faixa liberada é aceita");
            CHECK(!blk_write(d, off + 4096, a, 4096), "escrita logo APÓS a faixa é recusada");
            CHECK(!blk_write(d, off - 512, a, 1024), "escrita que cruza o início da faixa é recusada");
            CHECK(!blk_write(d, 0, a, 512), "escrita no setor 0 é recusada");
            CHECK(blk_read(d, off, b, 4096) && !memcmp(a, b, 4096), "o conteúdo permaneceu idêntico");
            storage_revoke_write(d, off, 4096);
            CHECK(!blk_write(d, off, a, 4096), "depois de revogar, a escrita volta a ser recusada");
        }
        tested++;
    }
    kfree(a); kfree(b);
    kprintf("unit: política de escrita verificada em %d disco(s) real(is)\n", tested);
}

/* ------------------------------------------------------------------ contas: formato 2, migração do formato 1, falhas */
static void test_accounts(void) {
    char err[160] = "";
    CHECK(!account_password_ok("1234567", err, sizeof err), "senha de 7 caracteres é recusada");
    CHECK(account_password_ok("12345678", err, sizeof err), "senha de 8 caracteres é aceita");
    CHECK(!account_password_ok("", err, sizeof err), "senha vazia é recusada pelo assistente");
    CHECK(account_kdf_iterations() >= 100000, "iterações calibradas respeitam o piso");
    CHECK(account_create("unitv2", "Unit V2", "senhaforte-123", false, err, sizeof err), "cria conta formato 2");
    vnode_t *v = vfs_lookup("/Sistema/Biblioteca/Usuarios/unitv2.conta");
    CHECK(v && v->data && strstr((char *)v->data, "Formato=2") && strstr((char *)v->data, "PBKDF2-HMAC-SHA256"), "arquivo tem Formato=2 e o algoritmo");
    CHECK(account_verify("unitv2", "senhaforte-123") && !account_verify("unitv2", "senhaforte-124") && !account_verify("unitv2", ""), "verifica certo/errado/vazio");
    /* hash adulterado e iterações zeradas nunca verificam */
    char *txt = kmalloc(v->size + 1);
    memcpy(txt, v->data, v->size); txt[v->size] = 0;
    char *it = strstr(txt, "Iteracoes=");
    if (it) { it[10] = '0'; it[11] = '0'; it[12] = '0'; vfs_write_file("/Sistema/Biblioteca/Usuarios/unitv2.conta", txt, strlen(txt)); }
    CHECK(!account_verify("unitv2", "senhaforte-123"), "iterações adulteradas (baixas) nunca verificam");
    kfree(txt);
    CHECK(!account_create("Visitante", "x", "senhaforte-123", false, err, sizeof err), "nome reservado Visitante é recusado");
    /* formato 1 (legado): arquivo escrito à mão com SHA-256 iterado -> verifica e MIGRA para o formato 2 */
    u8 h[32], salt[] = "0123456789abcdef0123456789abcdef";
    sha256_ctx c;
    sha256_init(&c); sha256_update(&c, salt, 32); sha256_update(&c, "legado-pw", 9); sha256_final(&c, h);
    for (int i = 1; i < 20000; i++) { sha256_init(&c); sha256_update(&c, h, 32); sha256_update(&c, salt, 32); sha256_update(&c, "legado-pw", 9); sha256_final(&c, h); }
    char hx[65], f1[300];
    for (int i = 0; i < 32; i++) { static const char d[] = "0123456789abcdef"; hx[i*2] = d[h[i] >> 4]; hx[i*2+1] = d[h[i] & 15]; }
    hx[64] = 0;
    snprintf(f1, sizeof f1, "NomeCompleto=Legado\nAdmin=0\nSal=%s\nHash=%s\nIteracoes=20000\n", (char *)salt, hx);
    vfs_write_file("/Sistema/Biblioteca/Usuarios/legado.conta", f1, strlen(f1));
    CHECK(account_verify("legado", "legado-pw"), "formato 1 (legado) ainda verifica");
    vnode_t *lv = vfs_lookup("/Sistema/Biblioteca/Usuarios/legado.conta");
    CHECK(lv && lv->data && strstr((char *)lv->data, "Formato=2"), "formato 1 é migrado para o 2 ao entrar");
    CHECK(account_verify("legado", "legado-pw") && !account_verify("legado", "outra"), "depois da migração continua verificando");
    account_delete("unitv2", true, err, sizeof err);
    account_delete("legado", true, err, sizeof err);
}

/* ------------------------------------------------------------------ política de conteúdo limpo do payload (espelha o mkimg) */
static void test_payload_policy(void) {
    static const char *const ok[] = {"/EFI/BOOT/BOOTX64.EFI", "/mOS/Sistema/Biblioteca/LaunchDaemons/com.apple.logd.plist", "/mOS/Aplicativos/X.app/Contents/Info.plist"};
    static const char *const bad[] = {
        "/mOS/Users/Visitante/Documentos/a.txt", "/mOS/Users", "/mOS/Sistema/Biblioteca/Usuarios/ana.conta", "/mOS/Sistema/Biblioteca/Logs/kernel.log",
        "/mOS/Sistema/qualquer.conta", "/mOS/Sistema/outro.log", "/mOS/Sistema/Biblioteca/estado-sistema", "/mOS/Sistema/Biblioteca/Preferencias/com.mos.dock.cfg",
        "/mOS/Sistema/Instalacao/manifesto.txt", "/mOS/tmp/x", "/mOS/Volumes/x", "/mOS/a/../b", "/etc/passwd", "/EFI/Microsoft/a.efi", "/EFI/BOOT/OUTRO.EFI",
        "/mOS/a/.Lixeira/b", "mOS/relativo", "/mOS/dir/", "/mOS//dupla", ""};
    for (u32 i = 0; i < ARRAY_LEN(ok); i++) CHECK(install_path_allowed(ok[i]), "caminho de sistema é aceito no manifesto");
    for (u32 i = 0; i < ARRAY_LEN(bad); i++) CHECK(!install_path_allowed(bad[i]), "caminho pessoal/de sessão/inválido é recusado no manifesto");
    static char longp[1100];
    memset(longp, 'a', sizeof longp - 1); memcpy(longp, "/mOS/", 5); longp[sizeof longp - 1] = 0;
    CHECK(!install_path_allowed(longp), "caminho com mais de 1000 caracteres é recusado");
}

/* ------------------------------------------------------------------ sem gerador de hardware: nada de senha com jitter/relógios */
static void test_no_hw_rng(void) {
    char err[200] = "";
    crypto_rng_test_force_hw(0);
    CHECK(!crypto_rng_hw(), "ramo sem gerador de hardware ativo no teste");
    CHECK(!account_create("semrng", "Sem RNG", "senhaforte-123", false, err, sizeof err) && strstr(err, "gerador"), "conta COM senha é recusada sem gerador de hardware");
    CHECK(!account_exists("semrng"), "nenhum arquivo de conta foi criado na recusa");
    CHECK(account_create("semsenha", "Sem Senha", "", false, err, sizeof err), "conta SEM senha (sessão live) ainda pode ser criada");
    CHECK(account_verify("semsenha", ""), "conta sem senha verifica com senha vazia");
    crypto_rng_test_force_hw(1);
    CHECK(account_create("comrng", "Com RNG", "senhaforte-123", false, err, sizeof err), "com gerador, a conta com senha é criada");
    crypto_rng_test_force_hw(0);
    CHECK(!account_set_password("comrng", "senhaforte-123", "outrasenha-456", err, sizeof err) && strstr(err, "gerador"), "trocar senha sem gerador é recusado");
    CHECK(account_verify("comrng", "senhaforte-123"), "a senha antiga continua valendo depois da recusa");
    /* migração do formato 1 NÃO acontece sem gerador (não haveria sal adequado) */
    u8 h[32], salt[] = "fedcba9876543210fedcba9876543210";
    sha256_ctx c;
    sha256_init(&c); sha256_update(&c, salt, 32); sha256_update(&c, "leg-pw", 6); sha256_final(&c, h);
    for (int i = 1; i < 20000; i++) { sha256_init(&c); sha256_update(&c, h, 32); sha256_update(&c, salt, 32); sha256_update(&c, "leg-pw", 6); sha256_final(&c, h); }
    char hx[65], f1[300];
    for (int i = 0; i < 32; i++) { static const char d[] = "0123456789abcdef"; hx[i*2] = d[h[i] >> 4]; hx[i*2+1] = d[h[i] & 15]; }
    hx[64] = 0;
    snprintf(f1, sizeof f1, "NomeCompleto=L2\nAdmin=0\nSal=%s\nHash=%s\nIteracoes=20000\n", (char *)salt, hx);
    vfs_write_file("/Sistema/Biblioteca/Usuarios/leg2.conta", f1, strlen(f1));
    CHECK(account_verify("leg2", "leg-pw"), "formato 1 verifica mesmo sem gerador");
    vnode_t *lv = vfs_lookup("/Sistema/Biblioteca/Usuarios/leg2.conta");
    CHECK(lv && lv->data && !strstr((char *)lv->data, "Formato=2"), "formato 1 NÃO é migrado sem gerador de hardware");
    crypto_rng_test_force_hw(-1);
    account_delete("semsenha", true, err, sizeof err);
    account_delete("comrng", true, err, sizeof err);
    account_delete("leg2", true, err, sizeof err);
}

static void write_plist(const char *path, const char *id, const char *exe, const char *scheme) {
    char b[700];
    snprintf(b, sizeof b,
             "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
             "<plist version=\"1.0\"><dict>\n"
             "<key>CFBundleIdentifier</key><string>%s</string>\n"
             "<key>CFBundleExecutable</key><string>%s</string>\n"
             "<key>CFBundleURLTypes</key><array><dict>\n"
             "<key>CFBundleURLSchemes</key><array><string>%s</string></array>\n"
             "</dict></array></dict></plist>\n",
             id, exe, scheme);
    vfs_write_file(path, b, strlen(b));
}

static void test_launch_runtime(void) {
    ls_app_t *a;
    vnode_t *n;
    char *p;
    int r0;
    const char *pref =
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<plist version=\"1.0\"><dict><key>LSHandlers</key><array><dict>"
        "<key>LSHandlerURLScheme</key><string>moslsregressx</string><key>LSHandlerRoleAll</key><string>com.mos.lsregressB</string>"
        "</dict></array></dict></plist>\n";
    CHECK(vfs_lookup("/Users/Visitante/Library/Preferences") && vfs_lookup("/Users/Visitante/Library/Application Support")
          && vfs_lookup("/Users/Visitante/Library/LaunchAgents"), "Visitante nasce com Library");
    ls_init();
    write_plist("/Aplicativos/LsA.app/Contents/Info.plist", "com.mos.lsregressA", "LsA", "moslsregressx");
    write_plist("/Aplicativos/LsB.app/Contents/Info.plist", "com.mos.lsregressB", "LsB", "moslsregressx");
    vfs_write_file("/Aplicativos/LsA.app/Contents/MacOS/LsA", "AAAA", 4);
    vfs_write_file("/Aplicativos/LsB.app/Contents/MacOS/LsB", "BBBB", 4);
    ls_rescan();
    CHECK(ls_app_by_id("com.mos.lsregressA") && ls_app_by_id("com.mos.lsregressB"), "dois bundles registrados");
    a = ls_default_for_scheme("moslsregressx");
    CHECK(a && !strcmp(ls_app_id(a), "com.mos.lsregressA"), "sem escolha, o primeiro registrado abre o esquema");
    r0 = ls_test_reparses();
    ls_test_poke();
    ls_tick();
    CHECK(ls_test_reparses() == r0, "ls_tick sem mudança não relê o bundle");
    n = vfs_lookup("/Aplicativos/LsA.app/Contents/Info.plist");
    p = n && n->data ? strstr((char *)n->data, "com.mos.lsregressA") : NULL;
    CHECK(p != NULL, "plist de LsA está no VFS");
    if (p) p[strlen("com.mos.lsregressA") - 1] = 'C';   /* mesmo tamanho, mesmo mtime */
    ls_test_poke();
    ls_tick();
    CHECK(ls_test_reparses() > r0, "mesmo tamanho de Info.plist com outro conteúdo é relido");
    CHECK(ls_app_by_id("com.mos.lsregressC") && !ls_app_by_id("com.mos.lsregressA"), "o identificador novo substitui o antigo");
    r0 = ls_test_reparses();
    n = vfs_lookup("/Aplicativos/LsA.app/Contents/MacOS/LsA");
    if (n && n->data && n->size) n->data[0] = 'Z';
    ls_test_poke();
    ls_tick();
    CHECK(ls_test_reparses() > r0, "byte do executável, mesmo tamanho, entra no carimbo");
    char prefpath[512];
    snprintf(prefpath, sizeof prefpath, "%s", home_sub("/Library/Preferences/com.apple.LaunchServices/com.apple.launchservices.secure.plist"));
    vfs_write_file(prefpath, pref, strlen(pref));
    ls_test_poke();
    ls_tick();
    a = ls_default_for_scheme("moslsregressx");
    CHECK(a && !strcmp(ls_app_id(a), "com.mos.lsregressB"), "escolha do usuário relida quando o plist muda");
    n = vfs_lookup(prefpath);
    p = n && n->data ? strstr((char *)n->data, "com.mos.lsregressB") : NULL;
    if (p) p[strlen("com.mos.lsregressB") - 1] = 'C';
    a = ls_default_for_scheme("moslsregressx");
    CHECK(a && !strcmp(ls_app_id(a), "com.mos.lsregressC"), "carimbo do plist de escolhas vê o byte trocado");
    vfs_mkdir("/tmp/fw/Demo.framework/Versions/A");
    vfs_write_file("/tmp/fw/Demo.framework/Versions/A/Demo", "MACH", 4);
    vfs_write_file("/tmp/fw/Demo.framework/Versions/Current", "A\n", 2);
    vfs_write_file("/tmp/fw/Demo.framework/Demo", "Versions/A/Demo", 15);
    n = vfs_lookup("/tmp/fw/Demo.framework/Versions/Current/Demo");
    CHECK(n && n->size == 4 && n->data && !memcmp(n->data, "MACH", 4), "texto Current dentro de .framework segue para A");
    vfs_mkdir("/tmp/fp/Store.app/Contents/SC_Info");
    vfs_write_file("/tmp/fp/Store.app/Contents/Info.plist", "x", 1);
    CHECK(ls_install_app("/tmp/fp/Store.app") == -2, "app com SC_Info não é copiado");
    CHECK(!vfs_lookup("/Aplicativos/Store.app"), "FairPlay não deixa resíduo em Aplicativos");
    write_plist("/tmp/vol/Ok.app/Contents/Info.plist", "com.mos.okinstall", "Ok", "mosokinstall");
    vfs_write_file("/tmp/vol/Ok.app/Contents/MacOS/Ok", "bin", 3);
    CHECK(ls_install_app("/tmp/vol/Ok.app") == 1, "bundle copiado de um volume para Aplicativos");
    CHECK(ls_install_app("/tmp/vol/Ok.app") == 0, "segunda instalação não sobrescreve");
    CHECK(ls_app_by_id("com.mos.okinstall") != NULL, "app instalado entra no Launch Services");
    vfs_delete(vfs_lookup("/Aplicativos/LsA.app"));
    vfs_delete(vfs_lookup("/Aplicativos/LsB.app"));
    vfs_delete(vfs_lookup("/Aplicativos/Ok.app"));
    vfs_delete(vfs_lookup(prefpath));
    ls_rescan();
    ls_test_poke();
    ls_tick();
    CHECK(!ls_app_by_id("com.mos.lsregressC") && !ls_app_by_id("com.mos.okinstall"), "apps de teste saem do banco");
}

void unit_tests_run(void) {
    kprintf("unit: ===== testes de unidade (build MOS_UNIT_TESTS) =====\n");
    test_crypto();
    test_entropy();
    test_partscan();
    test_gpt_layout();
    test_origin();
    test_policy();
    test_accounts();
    test_payload_policy();
    test_no_hw_rng();
    test_launch_runtime();
    kprintf("unit: RESUMO: %d verificações ok, %d FALHA(S) — %s\n", n_ok, n_bad, n_bad ? "REPROVADO" : "APROVADO");
}
#endif
