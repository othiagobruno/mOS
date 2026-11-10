/*
 * Varre uma pasta real. PNG e JPEG passam por mos_img_decode (ImageIO desta main).
 * GIF e BMP: largura e altura do cabeçalho do arquivo (o ImageIO desta main não os decodifica).
 * Vídeo: duração e quadro vêm do atom mvhd/tkhd quando o arquivo é MP4/MOV. Sem atom, duração 0
 * e pixels 0 — não há valor inventado.
 */
#include "photos_index.h"
#include <ctype.h>
#include <string.h>
#if defined(PHOTOS_HOST) || !defined(PHOTOS_IN_LIBSYSTEM)
#include <stdio.h>
#include <stdlib.h>
#else
#include <stddef.h>
typedef void photos_FILE;
photos_FILE *fopen(const char *p, const char *m);
size_t fread(void *p, size_t s, size_t n, photos_FILE *f);
int fclose(photos_FILE *f);
int fseek(photos_FILE *f, long off, int wh);
long ftell(photos_FILE *f);
#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_END 2
#endif
#define FILE photos_FILE
#endif
/* o restante do libSystem não pode herdar esse apelido */
#ifdef PHOTOS_IN_LIBSYSTEM
#undef FILE
#endif

#ifdef PHOTOS_HOST
#define _POSIX_C_SOURCE 200809L
#include <dirent.h>
#include <sys/stat.h>
#else
typedef struct {
    unsigned long long d_ino, d_seekoff;
    unsigned short d_reclen, d_namlen;
    unsigned char d_type;
    char d_name[1024];
} photos_dirent;
#endif

typedef struct mos_img {
    int w, h;
    int has_alpha;
    unsigned *px;
} mos_img_t;
const char *mos_img_uti(const unsigned char *data, size_t n);
int mos_img_decode(const unsigned char *data, size_t n, mos_img_t *out);
void mos_img_free(mos_img_t *img);

#define PHOTOS_CAP 400
#define PHOTOS_READ_MAX (16u * 1024u * 1024u)

struct photos_lib {
    char root[512];
    photos_asset items[PHOTOS_CAP];
    int n;
};

static char g_err[160];

const char *photos_errmsg(void) { return g_err; }

static void set_err(const char *s) {
    snprintf(g_err, sizeof g_err, "%s", s ? s : "");
}

int photos_auth_from_tcc(int tcc_state) {
    if (tcc_state == 2) return PHOTOS_AUTH_AUTHORIZED;
    if (tcc_state == 1) return PHOTOS_AUTH_DENIED;
    return PHOTOS_AUTH_NOT_DETERMINED;
}

void photos_root_from_home(const char *home, char *out, size_t cap) {
    if (!home || !home[0] || !strcmp(home, "(null)")) home = "/Users/Visitante";
    snprintf(out, cap, "%s/Imagens", home);
}

static const char *ext_of(const char *name) {
    const char *dot = strrchr(name, '.');
    if (!dot || dot == name || strchr(dot, '/')) return "";
    return dot + 1;
}

static int ext_is(const char *name, const char *e) {
    const char *x = ext_of(name);
    while (*x && *e) {
        char a = (char)tolower((unsigned char)*x++);
        char b = (char)tolower((unsigned char)*e++);
        if (a != b) return 0;
    }
    return *x == 0 && *e == 0;
}

int photos_media_of_name(const char *name) {
    if (!name) return 0;
    if (ext_is(name, "png") || ext_is(name, "jpg") || ext_is(name, "jpeg") || ext_is(name, "gif") || ext_is(name, "bmp"))
        return PHOTOS_MEDIA_IMAGE;
    if (ext_is(name, "mov") || ext_is(name, "mp4") || ext_is(name, "m4v")) return PHOTOS_MEDIA_VIDEO;
    return 0;
}

static unsigned rd32be(const unsigned char *p) {
    return ((unsigned)p[0] << 24) | ((unsigned)p[1] << 16) | ((unsigned)p[2] << 8) | p[3];
}

static void measure_still(photos_asset *a, const unsigned char *b, size_t n) {
    mos_img_t im;
    if (n >= 8 && !memcmp(b, "\x89PNG\r\n\x1a\n", 8) && mos_img_decode(b, n, &im) == 0) {
        a->pixel_w = im.w;
        a->pixel_h = im.h;
        mos_img_free(&im);
        return;
    }
    if (n >= 2 && b[0] == 0xFF && b[1] == 0xD8 && mos_img_decode(b, n, &im) == 0) {
        a->pixel_w = im.w;
        a->pixel_h = im.h;
        mos_img_free(&im);
        return;
    }
    if (n >= 10 && !memcmp(b, "GIF8", 4)) {
        a->pixel_w = (int)(b[6] | (b[7] << 8));
        a->pixel_h = (int)(b[8] | (b[9] << 8));
        return;
    }
    if (n >= 26 && b[0] == 'B' && b[1] == 'M') {
        int w = (int)(b[18] | (b[19] << 8) | (b[20] << 16) | (b[21] << 24));
        int h = (int)(b[22] | (b[23] << 8) | (b[24] << 16) | (b[25] << 24));
        if (h < 0) h = -h;
        if (w > 0 && h > 0) { a->pixel_w = w; a->pixel_h = h; }
    }
}

/* mvhd/tkhd no começo do arquivo. Não percorre mdat. */
static void measure_movie(photos_asset *a, const unsigned char *b, size_t n) {
    size_t i = 0;
    int saw_ftyp = 0;
    if (n >= 12 && !memcmp(b + 4, "ftyp", 4)) saw_ftyp = 1;
    while (i + 8 <= n) {
        unsigned sz = rd32be(b + i);
        const unsigned char *typ = b + i + 4;
        size_t next;
        if (sz < 8) break;
        next = i + sz;
        if (next > n || next <= i) break;
        if (!memcmp(typ, "moov", 4) || !memcmp(typ, "trak", 4)) {
            i += 8;
            continue;
        }
        if (!memcmp(typ, "mvhd", 4) && sz >= 32) {
            const unsigned char *p = b + i + 8;
            unsigned ver = p[0];
            if (ver == 0 && sz >= 8 + 20) {
                unsigned scale = rd32be(p + 12);
                unsigned dur = rd32be(p + 16);
                if (scale) a->duration = (double)dur / (double)scale;
            } else if (ver == 1 && sz >= 8 + 32) {
                unsigned long long scale = rd32be(p + 20);
                unsigned long long dur = ((unsigned long long)rd32be(p + 24) << 32) | rd32be(p + 28);
                if (scale) a->duration = (double)dur / (double)scale;
            }
        } else if (!memcmp(typ, "tkhd", 4) && a->pixel_w == 0 && sz >= 8 + 84) {
            const unsigned char *p = b + i + 8;
            unsigned ver = p[0];
            size_t off = ver == 1 ? 88 : 76;
            if (8 + off + 8 <= sz) {
                unsigned w = rd32be(p + off);
                unsigned h = rd32be(p + off + 4);
                if ((w >> 16) > 0 && (h >> 16) > 0) {
                    a->pixel_w = (int)(w >> 16);
                    a->pixel_h = (int)(h >> 16);
                }
            }
        } else if (!memcmp(typ, "mdat", 4) || !memcmp(typ, "free", 4) || !memcmp(typ, "wide", 4)) {
            /* o quadro não está aqui */
        }
        i = next;
        if (!saw_ftyp && i > 64 * 1024) break;
    }
}

static int file_mtime(const char *path, long long *out) {
#ifdef PHOTOS_HOST
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    *out = (long long)st.st_mtime;
    return S_ISDIR(st.st_mode) ? 1 : 0;
#else
    unsigned char st[144];
    unsigned mode;
    long long mtime;
    if (stat(path, st) != 0) return -1;
    mode = (unsigned)st[4] | ((unsigned)st[5] << 8);
    memcpy(&mtime, st + 48, sizeof mtime);
    *out = mtime;
    return (mode & 0170000) == 0040000 ? 1 : 0;
#endif
}

static void fill_meta(photos_asset *a) {
    FILE *f;
    unsigned char *buf;
    size_t n = 0;
    long sz;
    f = fopen(a->path, "rb");
    if (!f) return;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return; }
    sz = ftell(f);
    if (sz <= 0) { fclose(f); return; }
    if ((unsigned long)sz > PHOTOS_READ_MAX) sz = (long)PHOTOS_READ_MAX;
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return; }
    buf = malloc((size_t)sz);
    if (!buf) { fclose(f); return; }
    n = fread(buf, 1, (size_t)sz, f);
    fclose(f);
    if (a->media == PHOTOS_MEDIA_IMAGE) measure_still(a, buf, n);
    else if (a->media == PHOTOS_MEDIA_VIDEO) measure_movie(a, buf, n);
    free(buf);
}

static void add_file(photos_lib *lib, const char *path, const char *name) {
    photos_asset *a;
    int media = photos_media_of_name(name);
    long long mt = 0;
    int kind;
    if (!media || lib->n >= PHOTOS_CAP) return;
    kind = file_mtime(path, &mt);
    if (kind != 0) return;
    a = &lib->items[lib->n];
    memset(a, 0, sizeof *a);
    snprintf(a->path, sizeof a->path, "%s", path);
    snprintf(a->ident, sizeof a->ident, "%s", path);
    a->media = media;
    a->hidden = name[0] == '.';
    a->creation_unix = mt;
    fill_meta(a);
    lib->n++;
}

static void walk(photos_lib *lib, const char *dir, int depth) {
#ifdef PHOTOS_HOST
    DIR *d;
    struct dirent *e;
#else
    void *d;
    photos_dirent *e;
    void *opendir(const char *);
    void *readdir(void *);
    int closedir(void *);
#endif
    if (depth > 4 || lib->n >= PHOTOS_CAP) return;
#ifdef PHOTOS_HOST
    d = opendir(dir);
#else
    d = opendir(dir);
#endif
    if (!d) return;
    while ((e = readdir(d)) != 0 && lib->n < PHOTOS_CAP) {
        char sub[512];
        long long mt = 0;
        int kind;
        const char *name = e->d_name;
        if (!name[0] || !strcmp(name, ".") || !strcmp(name, "..")) continue;
        if (name[0] == '.' && photos_media_of_name(name) == 0) continue;
        snprintf(sub, sizeof sub, "%s/%s", dir, name);
        kind = file_mtime(sub, &mt);
        if (kind < 0) continue;
        if (kind == 1) {
            if (name[0] == '.') continue;
            walk(lib, sub, depth + 1);
        } else add_file(lib, sub, name);
    }
    closedir(d);
}

photos_lib *photos_open(const char *root) {
    photos_lib *lib;
    long long mt = 0;
    int kind;
    set_err("");
    if (!root || !root[0]) { set_err("pasta da biblioteca ausente"); return 0; }
    kind = file_mtime(root, &mt);
    if (kind < 0) { set_err("pasta da biblioteca não existe ou não pode ser lida"); return 0; }
    if (kind != 1) { set_err("o caminho da biblioteca não é uma pasta"); return 0; }
    lib = calloc(1, sizeof *lib);
    if (!lib) { set_err("sem memória para o índice"); return 0; }
    snprintf(lib->root, sizeof lib->root, "%s", root);
    walk(lib, lib->root, 0);
    return lib;
}

void photos_close(photos_lib *lib) { free(lib); }
const char *photos_root(const photos_lib *lib) { return lib ? lib->root : ""; }
int photos_count_all(const photos_lib *lib) { return lib ? lib->n : 0; }
const photos_asset *photos_at(const photos_lib *lib, int i) {
    if (!lib || i < 0 || i >= lib->n) return 0;
    return &lib->items[i];
}

static int cmp_one(const photos_asset *a, const photos_asset *b, int sort) {
    int dir = 1;
    int c = 0;
    switch (sort) {
    case PHOTOS_SORT_CREATION_DESC: dir = -1; /* fallthrough */
    case PHOTOS_SORT_CREATION_ASC:
        c = (a->creation_unix > b->creation_unix) - (a->creation_unix < b->creation_unix);
        break;
    case PHOTOS_SORT_IDENT_DESC: dir = -1; /* fallthrough */
    case PHOTOS_SORT_IDENT_ASC:
        c = strcmp(a->ident, b->ident);
        break;
    case PHOTOS_SORT_WIDTH_DESC: dir = -1; /* fallthrough */
    case PHOTOS_SORT_WIDTH_ASC:
        c = (a->pixel_w > b->pixel_w) - (a->pixel_w < b->pixel_w);
        break;
    case PHOTOS_SORT_HEIGHT_DESC: dir = -1; /* fallthrough */
    case PHOTOS_SORT_HEIGHT_ASC:
        c = (a->pixel_h > b->pixel_h) - (a->pixel_h < b->pixel_h);
        break;
    case PHOTOS_SORT_DURATION_DESC: dir = -1; /* fallthrough */
    case PHOTOS_SORT_DURATION_ASC:
        c = (a->duration > b->duration) - (a->duration < b->duration);
        break;
    default:
        return 0;
    }
    if (!c) c = strcmp(a->path, b->path);
    return c * dir;
}

static int pass_query(const photos_asset *a, const photos_query *q) {
    if (!q->include_hidden && a->hidden) return 0;
    if (q->media >= 0 && a->media != q->media) return 0;
    if (q->min_width > 0 && a->pixel_w < q->min_width) return 0;
    if (q->ident_sub[0] && !strstr(a->ident, q->ident_sub)) return 0;
    return 1;
}

int photos_apply(const photos_asset *all, int n, const photos_query *q, int *idx_out, int cap) {
    int tmp[PHOTOS_CAP];
    int nt = 0;
    int i, j;
    photos_query none;
    if (!all || !q || !idx_out || cap <= 0 || n <= 0) return 0;
    if (n > PHOTOS_CAP) n = PHOTOS_CAP;
    for (i = 0; i < n; i++) if (pass_query(&all[i], q)) tmp[nt++] = i;
    for (i = 1; i < nt; i++) {
        int v = tmp[i];
        j = i;
        while (j > 0) {
            int c = 0;
            int s;
            for (s = 0; s < q->nsort && s < 4; s++) {
                c = cmp_one(&all[tmp[j - 1]], &all[v], q->sort[s]);
                if (c) break;
            }
            if (c <= 0) break;
            tmp[j] = tmp[j - 1];
            j--;
        }
        tmp[j] = v;
    }
    (void)none;
    if (q->limit && (unsigned)nt > q->limit) nt = (int)q->limit;
    if (nt > cap) nt = cap;
    for (i = 0; i < nt; i++) idx_out[i] = tmp[i];
    return nt;
}
