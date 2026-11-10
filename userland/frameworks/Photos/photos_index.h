/*
 * Índice local da biblioteca de fotos do mOS.
 * A pasta é NSHomeDirectory()/Imagens (a mesma que o app Fotos varre primeiro).
 * MOS_PHOTOS_LIBRARY, se existir, substitui essa pasta. Não há nuvem nem asset inventado.
 */
#ifndef MOS_PHOTOS_INDEX_H
#define MOS_PHOTOS_INDEX_H
#include <stddef.h>

enum {
    PHOTOS_MEDIA_IMAGE = 1,
    PHOTOS_MEDIA_VIDEO = 2
};

/* Espelho do PHAuthorizationStatus que este framework devolve. */
enum {
    PHOTOS_AUTH_NOT_DETERMINED = 0,
    PHOTOS_AUTH_DENIED = 2,
    PHOTOS_AUTH_AUTHORIZED = 3
};

/* Estado do TCC do mOS: 0 ausente, 1 negado, 2 concedido, 3 pendente. */
int photos_auth_from_tcc(int tcc_state);

typedef struct photos_asset {
    char path[512];
    char ident[512];
    int media;
    int hidden;
    int pixel_w, pixel_h;
    long long creation_unix;
    double duration;
} photos_asset;

typedef struct photos_lib photos_lib;

/* NULL se a pasta não existe, não é pasta ou não pode ser lida. photos_errmsg diz qual. */
photos_lib *photos_open(const char *root);
void photos_close(photos_lib *lib);
const char *photos_errmsg(void);
const char *photos_root(const photos_lib *lib);
int photos_count_all(const photos_lib *lib);
const photos_asset *photos_at(const photos_lib *lib, int i);

void photos_root_from_home(const char *home, char *out, size_t cap);

enum {
    PHOTOS_SORT_NONE = 0,
    PHOTOS_SORT_CREATION_ASC = 1,
    PHOTOS_SORT_CREATION_DESC = 2,
    PHOTOS_SORT_IDENT_ASC = 3,
    PHOTOS_SORT_IDENT_DESC = 4,
    PHOTOS_SORT_WIDTH_ASC = 5,
    PHOTOS_SORT_WIDTH_DESC = 6,
    PHOTOS_SORT_HEIGHT_ASC = 7,
    PHOTOS_SORT_HEIGHT_DESC = 8,
    PHOTOS_SORT_DURATION_ASC = 9,
    PHOTOS_SORT_DURATION_DESC = 10
};

typedef struct photos_query {
    int include_hidden;
    int media;              /* -1 = qualquer */
    int min_width;          /* 0 = sem piso */
    char ident_sub[160];    /* vazio = sem filtro de identificador */
    int sort[4];
    int nsort;
    unsigned limit;         /* 0 = sem limite */
} photos_query;

/* Devolve quantos índices couberam em idx_out. Não aloca asset. */
int photos_apply(const photos_asset *all, int n, const photos_query *q, int *idx_out, int cap);

/* 1 imagem, 2 vídeo, 0 outro. */
int photos_media_of_name(const char *name);

#endif
