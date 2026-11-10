/* gcc -DPHOTOS_HOST -I userland/frameworks/Photos -I userland/frameworks/ImageIO \
 *   tools/photostest.c userland/frameworks/Photos/photos_index.c \
 *   userland/frameworks/ImageIO/imageio_decode.c -lm -o /tmp/photostest && /tmp/photostest
 * Índice real de uma pasta. Não sobe a VM. */
#define _POSIX_C_SOURCE 200809L
#include "photos_index.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static int fails;
static void expect(int cond, const char *msg) {
    if (!cond) { fprintf(stderr, "FALHOU: %s\n", msg); fails++; }
    else printf("ok %s\n", msg);
}

static const unsigned char rgba_png[] = {137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,2,0,0,0,2,8,6,0,0,0,114,182,13,36,0,0,0,20,73,68,65,84,120,156,99,248,207,192,240,31,8,27,24,128,52,24,0,0,68,210,9,120,182,135,35,75,0,0,0,0,73,69,78,68,174,66,96,130};

static void write_file(const char *path, const void *b, size_t n) {
    FILE *f = fopen(path, "wb");
    if (!f) { perror(path); return; }
    fwrite(b, 1, n, f);
    fclose(f);
}

static void mvhd(unsigned char *d, unsigned scale, unsigned dur) {
    memset(d, 0, 128);
    d[0] = 0; d[1] = 0; d[2] = 0; d[3] = 20;
    memcpy(d + 4, "ftyp", 4);
    memcpy(d + 8, "mp41", 4);
    d[20] = 0; d[21] = 0; d[22] = 0; d[23] = 116;
    memcpy(d + 24, "moov", 4);
    d[28] = 0; d[29] = 0; d[30] = 0; d[31] = 108;
    memcpy(d + 32, "mvhd", 4);
    d[48] = (unsigned char)(scale >> 24);
    d[49] = (unsigned char)(scale >> 16);
    d[50] = (unsigned char)(scale >> 8);
    d[51] = (unsigned char)scale;
    d[52] = (unsigned char)(dur >> 24);
    d[53] = (unsigned char)(dur >> 16);
    d[54] = (unsigned char)(dur >> 8);
    d[55] = (unsigned char)dur;
}

int main(void) {
    char dir[] = "/tmp/mos-photos-XXXXXX";
    char path[256], hidden[256], mov[256], gif[256];
    photos_lib *lib, *missing, *empty;
    photos_query q;
    int idx[16];
    int n;
    const photos_asset *a, *b;
    unsigned char movie[136];
    unsigned char gifb[16];
    if (!mkdtemp(dir)) { perror("mkdtemp"); return 1; }
    snprintf(path, sizeof path, "%s/b.png", dir);
    snprintf(hidden, sizeof hidden, "%s/.oculta.png", dir);
    snprintf(mov, sizeof mov, "%s/clip.mp4", dir);
    snprintf(gif, sizeof gif, "%s/t.gif", dir);
    write_file(path, rgba_png, sizeof rgba_png);
    write_file(hidden, rgba_png, sizeof rgba_png);
    write_file(path, rgba_png, sizeof rgba_png);
    {
        char junk[256];
        snprintf(junk, sizeof junk, "%s/nota.txt", dir);
        write_file(junk, "oi", 2);
    }
    mvhd(movie, 1000, 2500);
    write_file(mov, movie, sizeof movie);
    memset(gifb, 0, sizeof gifb);
    memcpy(gifb, "GIF89a", 6);
    gifb[6] = 3; gifb[8] = 5;
    write_file(gif, gifb, sizeof gifb);

    missing = photos_open("/tmp/mos-photos-nao-existe");
    expect(missing == 0, "pasta ausente não abre");
    expect(photos_errmsg() && strstr(photos_errmsg(), "não existe"), "erro honesto de pasta ausente");

    empty = photos_open("/tmp");
    expect(empty != 0, "/tmp abre");
    photos_close(empty);

    lib = photos_open(dir);
    expect(lib != 0, "biblioteca abre");
    expect(photos_count_all(lib) == 4, "png, gif, mp4 e oculta; txt fica de fora");

    memset(&q, 0, sizeof q);
    q.media = -1;
    n = photos_apply(photos_at(lib, 0), photos_count_all(lib), &q, idx, 16);
    expect(n == 3, "sem ocultos são 3");

    q.include_hidden = 1;
    q.media = PHOTOS_MEDIA_IMAGE;
    q.min_width = 2;
    n = photos_apply(photos_at(lib, 0), photos_count_all(lib), &q, idx, 16);
    expect(n == 3, "três imagens com largura >= 2, inclusive a oculta");

    q.include_hidden = 0;
    q.min_width = 0;
    q.media = PHOTOS_MEDIA_VIDEO;
    n = photos_apply(photos_at(lib, 0), photos_count_all(lib), &q, idx, 16);
    expect(n == 1, "um vídeo");
    a = photos_at(lib, idx[0]);
    expect(a && a->duration > 2.4 && a->duration < 2.6, "duração do mvhd é 2,5 s");

    q.media = PHOTOS_MEDIA_IMAGE;
    snprintf(q.ident_sub, sizeof q.ident_sub, "t.gif");
    q.sort[0] = PHOTOS_SORT_IDENT_ASC;
    q.nsort = 1;
    n = photos_apply(photos_at(lib, 0), photos_count_all(lib), &q, idx, 16);
    expect(n == 1, "filtro pelo identificador");
    a = photos_at(lib, idx[0]);
    expect(a && a->pixel_w == 3 && a->pixel_h == 5, "GIF lê o cabeçalho");

    memset(&q, 0, sizeof q);
    q.media = -1;
    q.sort[0] = PHOTOS_SORT_IDENT_ASC;
    q.nsort = 1;
    q.limit = 1;
    n = photos_apply(photos_at(lib, 0), photos_count_all(lib), &q, idx, 16);
    expect(n == 1, "fetchLimit 1");
    a = photos_at(lib, idx[0]);
    expect(a && strstr(a->ident, "b.png"), "o primeiro identificador é b.png");
    expect(a && a->pixel_w == 2 && a->pixel_h == 2, "PNG 2x2 pelo ImageIO");
    expect(a && a->creation_unix > 0, "creationDate é o mtime do arquivo");

    memset(&q, 0, sizeof q);
    q.media = -1;
    q.sort[0] = PHOTOS_SORT_WIDTH_DESC;
    q.nsort = 1;
    n = photos_apply(photos_at(lib, 0), photos_count_all(lib), &q, idx, 16);
    a = photos_at(lib, idx[0]);
    b = photos_at(lib, idx[n - 1]);
    expect(a && b && a->pixel_w >= b->pixel_w, "ordem por largura");

    expect(photos_auth_from_tcc(0) == PHOTOS_AUTH_NOT_DETERMINED, "TCC ausente");
    expect(photos_auth_from_tcc(3) == PHOTOS_AUTH_NOT_DETERMINED, "TCC pendente");
    expect(photos_auth_from_tcc(1) == PHOTOS_AUTH_DENIED, "TCC negado");
    expect(photos_auth_from_tcc(2) == PHOTOS_AUTH_AUTHORIZED, "TCC concedido");

    {
        char home[64];
        char root[96];
        snprintf(home, sizeof home, "/Users/Visitante");
        photos_root_from_home(home, root, sizeof root);
        expect(strcmp(root, "/Users/Visitante/Imagens") == 0, "pasta padrão é ~/Imagens");
        photos_root_from_home(0, root, sizeof root);
        expect(strcmp(root, "/Users/Visitante/Imagens") == 0, "home vazio cai em Visitante");
    }

    photos_close(lib);
    unlink(path); unlink(hidden); unlink(mov); unlink(gif);
    {
        char junk[256];
        snprintf(junk, sizeof junk, "%s/nota.txt", dir);
        unlink(junk);
    }
    rmdir(dir);
    if (fails) { fprintf(stderr, "%d falha(s)\n", fails); return 1; }
    printf("photos: ok\n");
    return 0;
}
