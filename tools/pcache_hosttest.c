/* Teste no host do page cache, writeback e aio. Só arquivos temporários. */
#define MOS_HOST_TEST 1
#include "fs.h"
#include "syscall.h"
#include <stdio.h>
#include <unistd.h>

long long g_live_allocs, g_live_bytes;
void rtc_now(datetime_t *dt) { memset(dt, 0, sizeof *dt); }
u32 rand32(void) { return 1; }

static int fails;
#define CHECK(c, m) do { if (!(c)) { fails++; fprintf(stderr, "FALHOU %s (%s:%d)\n", m, __FILE__, __LINE__); } } while (0)

typedef struct { FILE *fp; u64 reads, writes, read_bytes; } fdv_t;

static bool frd(blkdev_t *d, u64 off, void *buf, u64 len) {
    fdv_t *f = d->ctx;
    f->reads++;
    f->read_bytes += len;
    if (fseeko(f->fp, (off_t)off, SEEK_SET) != 0) return false;
    return fread(buf, 1, len, f->fp) == len;
}
static bool fwr(blkdev_t *d, u64 off, const void *buf, u64 len) {
    fdv_t *f = d->ctx;
    f->writes++;
    if (fseeko(f->fp, (off_t)off, SEEK_SET) != 0) return false;
    return fwrite(buf, 1, len, f->fp) == len;
}
static bool ffl(blkdev_t *d) { return fflush(((fdv_t *)d->ctx)->fp) == 0; }
static bool boom(blkdev_t *d, u64 off, void *buf, u64 len) { (void)d; (void)off; (void)buf; (void)len; fprintf(stderr, "leitura direta na partição\n"); return false; }
static bool boomw(blkdev_t *d, u64 off, const void *buf, u64 len) { (void)d; (void)off; (void)buf; (void)len; fprintf(stderr, "escrita direta na partição\n"); return false; }

int main(void) {
    char path[] = "/tmp/mos-pcache-XXXXXX";
    int fd = mkstemp(path);
    CHECK(fd >= 0, "arquivo temporário");
    if (fd < 0) return 1;
    const u64 sz = 4u << 20;
    CHECK(ftruncate(fd, (off_t)sz) == 0, "tamanho");
    FILE *fp = fdopen(fd, "r+b");
    u8 *blk = calloc(1, 4096);
    for (u64 o = 0; o < sz; o += 4096) {
        memset(blk, (int)(0xA0 + (o >> 20)), 4096);
        CHECK(fwrite(blk, 1, 4096, fp) == 4096, "preenche");
    }
    fflush(fp);
    fdv_t *ctx = calloc(1, sizeof *ctx);
    ctx->fp = fp;
    blkdev_t root = {0};
    root.size = sz; root.read = frd; root.write = fwr; root.flush = ffl; root.ctx = ctx; root.lbs = 512;

    /* antes: cada leitura de 4 KiB vai ao arquivo */
    u64 before_reads = 0, before_bytes = 0;
    u8 page[4096];
    for (int i = 0; i < 64; i++) {
        before_reads++;
        before_bytes += 4096;
        CHECK(root.read(&root, (u64)i * 4096, page, 4096), "leitura crua");
    }
    ctx->reads = 0; ctx->read_bytes = 0; ctx->writes = 0;
    pcache_stats_reset();
    for (int i = 0; i < 64; i++) CHECK(blk_read(&root, (u64)i * 4096, page, 4096), "leitura cache");
    pcache_stats_t ps;
    pcache_stats(&ps);
    printf("antes: %llu leituras, %llu bytes no dispositivo\n", (unsigned long long)before_reads, (unsigned long long)before_bytes);
    printf("depois: %llu leituras, %llu bytes no dispositivo (hits %llu misses %llu ra %llu)\n",
           (unsigned long long)ctx->reads, (unsigned long long)ctx->read_bytes,
           (unsigned long long)ps.hits, (unsigned long long)ps.misses, (unsigned long long)ps.ra_pages);
    CHECK(ctx->reads < before_reads, "readahead reduz o número de leituras");
    CHECK(ctx->reads == ps.dev_reads, "contador do cache");

    /* duas partições: A [1 MiB, 1 MiB+4 KiB+512), B logo em seguida */
    u64 ab = 1u << 20, asz = 4096 + 512, bb = ab + asz, bsz = 8192;
    blkdev_t A = {0}, B = {0};
    A.size = asz; A.base = ab; A.parent = &root; A.read = boom; A.write = boomw;
    B.size = bsz; B.base = bb; B.parent = &root; B.read = boom; B.write = boomw;
    u8 bpat[8192];
    CHECK(blk_read(&B, 0, bpat, bsz), "ler B");
    u8 expectb[8192];
    memset(expectb, 0xA1, 8192);
    CHECK(memcmp(bpat, expectb, bsz) == 0, "B inicial");
    u8 w[600];
    memset(w, 0x5C, sizeof w);
    CHECK(blk_write(&A, asz - sizeof w, w, sizeof w), "writeback em A");
    fseeko(fp, (off_t)(ab + asz - sizeof w), SEEK_SET);
    u8 raw[600];
    CHECK(fread(raw, 1, sizeof raw, fp) == sizeof raw, "ler cru");
    CHECK(raw[0] != 0x5C, "writeback ainda não chegou no arquivo");
    CHECK(blk_flush(&A), "fsync da partição A");
    fseeko(fp, (off_t)(ab + asz - sizeof w), SEEK_SET);
    CHECK(fread(raw, 1, sizeof raw, fp) == sizeof raw && memcmp(raw, w, sizeof w) == 0, "fsync gravou A");
    fseeko(fp, (off_t)bb, SEEK_SET);
    u8 braw[8192];
    CHECK(fread(braw, 1, bsz, fp) == bsz && memcmp(braw, expectb, bsz) == 0, "B intacto depois do fsync de A");
    u8 back[600];
    CHECK(blk_read(&A, asz - sizeof w, back, sizeof w) && memcmp(back, w, sizeof w) == 0, "releitura de A");

    /* aio: submete sem escrever; a conclusão é o pump */
    u64 w0 = ctx->writes;
    struct aiocb cb;
    memset(&cb, 0, sizeof cb);
    cb.aio_fildes = 7;
    cb.aio_offset = 0;
    cb.aio_buf = w;
    cb.aio_nbytes = 64;
    memset(w, 0x11, 64);
    CHECK(aio_bind(7, &B) == 0, "bind");
    CHECK(aio_write(&cb) == 0, "aio_write");
    CHECK(aio_error(&cb) == MOS_EINPROGRESS, "ainda pendente");
    CHECK(ctx->writes == w0, "submissão não escreveu");
    const struct aiocb *one = &cb;
    struct { i64 tv_sec; i64 tv_nsec; } zero = {0, 0};
    CHECK(aio_suspend(&one, 1, &zero) == -MOS_EAGAIN, "timeout zero não espera");
    CHECK(aio_error(&cb) == MOS_EINPROGRESS, "timeout zero não concluiu");
    aio_host_intr = 1;
    CHECK(aio_suspend(&one, 1, NULL) == -MOS_EINTR, "eintr");
    CHECK(aio_error(&cb) == MOS_EINPROGRESS, "eintr não concluiu");
    struct aiocb extra;
    memset(&extra, 0, sizeof extra);
    extra.aio_fildes = 7;
    extra.aio_offset = 128;
    extra.aio_buf = w;
    extra.aio_nbytes = 32;
    CHECK(aio_write(&extra) == 0, "segunda escrita");
    const struct aiocb *both[2] = { &cb, &extra };
    CHECK(aio_suspend(both, 2, NULL) == 0, "suspend devolve no primeiro");
    int done_cb = aio_error(&cb) == 0, done_ex = aio_error(&extra) == 0;
    CHECK(done_cb + done_ex == 1, "um da lista concluiu e o outro segue");
    const struct aiocb *rest = done_cb ? &extra : &cb;
    CHECK(aio_suspend(&rest, 1, NULL) == 0, "o segundo conclui");
    CHECK(aio_error(&cb) == 0 && aio_error(&extra) == 0, "os dois prontos");
    CHECK(aio_return(&cb) == 64 && aio_return(&extra) == 32, "retornos");
    struct { i64 tv_sec; i64 tv_nsec; } bad = {0, 1000000000LL};
    CHECK(aio_suspend(both, 2, &bad) == -MOS_EINVAL, "timespec inválido");
    CHECK(ctx->writes == w0, "aio_write é writeback");
    struct aiocb sy;
    memset(&sy, 0, sizeof sy);
    sy.aio_fildes = 7;
    CHECK(aio_fsync(0, &sy) == 0, "aio_fsync submete");
    const struct aiocb *two = &sy;
    CHECK(aio_suspend(&two, 1, NULL) == 0, "fsync completa");
    CHECK(aio_error(&sy) == 0, "fsync ok");
    aio_return(&sy);
    fseeko(fp, (off_t)bb, SEEK_SET);
    u8 got[64];
    CHECK(fread(got, 1, 64, fp) == 64 && memcmp(got, w, 64) == 0, "aio_fsync persistiu");
    fseeko(fp, (off_t)(bb + 64), SEEK_SET);
    CHECK(fread(braw, 1, 64, fp) == 64 && braw[0] == 0xA1, "vizinho do aio intacto");

    pcache_drop(&root);
    fclose(fp);
    unlink(path);
    free(blk); free(ctx);
    printf("%s\n", fails ? "FALHAS" : "ok");
    return fails ? 1 : 0;
}
