/* Retrato de uma pasta para readdir(3). O SYS_READDIR do kernel é por índice ao vivo: quem apaga enquanto varre
 * (remove_dir_all do Rust, rm -r, o limpador de ~/.codex/tmp/arg0 do Codex CLI) desloca os índices, pula uma
 * entrada e o rmdir final falha com ENOTEMPTY. O POSIX pede que cada entrada que não foi apagada nem criada depois do
 * opendir/rewinddir saia uma vez só; o retrato tirado no primeiro readdir garante isso.
 * Sem string.h: quem inclui já tem malloc, realloc, free, strlen e memcpy. tools/dirsnap_host.c no host. */

/* lista: devolve 1 e o nome da entrada número idx, 0 no fim, < 0 em erro */
typedef int (*ds_list_fn)(void *ctx, const char *dir, long idx, char *name, int cap);

typedef struct {
    char *buf;            /* nomes seguidos, cada um com o 0 */
    long *off;            /* início de cada nome em buf */
    long n, used, cap, offcap;
    int ready;
} ds_snap_t;

static void ds_free(ds_snap_t *s) {
    free(s->buf);
    free(s->off);
    s->buf = 0; s->off = 0;
    s->n = s->used = s->cap = s->offcap = 0;
    s->ready = 0;
}

/* 0 = ok; -1 = sem memória (o retrato fica com o que já leu) */
static int ds_fill(ds_snap_t *s, ds_list_fn list, void *ctx, const char *dir) {
    char name[1024];
    ds_free(s);
    s->ready = 1;
    for (long i = 0; i < 1000000; i++) {
        if (list(ctx, dir, i, name, (int)sizeof name) != 1) break;
        long len = (long)strlen(name) + 1;
        if (s->used + len > s->cap) {
            long nc = s->cap ? s->cap * 2 : 4096;
            while (nc < s->used + len) nc *= 2;
            char *nb = (char *)realloc(s->buf, (size_t)nc);
            if (!nb) return -1;
            s->buf = nb; s->cap = nc;
        }
        if (s->n == s->offcap) {
            long no = s->offcap ? s->offcap * 2 : 64;
            long *nf = (long *)realloc(s->off, (size_t)no * sizeof *nf);
            if (!nf) return -1;
            s->off = nf; s->offcap = no;
        }
        memcpy(s->buf + s->used, name, (size_t)len);
        s->off[s->n++] = s->used;
        s->used += len;
    }
    return 0;
}

static const char *ds_get(const ds_snap_t *s, long i) {
    return i >= 0 && i < s->n ? s->buf + s->off[i] : 0;
}
