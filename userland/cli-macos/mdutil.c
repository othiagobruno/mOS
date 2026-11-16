/* mOS — mdutil(1): estado do Spotlight por volume.
 *   mdutil -s volume...        "Indexing enabled." / "Indexing disabled."
 *   mdutil -i on|off volume... liga ou desliga a busca no volume (o arquivo .metadata_never_index na raiz, o mesmo
 *                              sinal do macOS; o MDQuery do libSystem não entra num volume que o tenha)
 *   mdutil -E volume...        o mOS não guarda índice (cada busca lê o disco): não há o que apagar, só informa
 *   mdutil -a                  todos os volumes montados (/ e cada pasta de /Volumes)
 * Saída no formato do macOS ("/:\n\tIndexing enabled."). Código 1 em erro de uso ou de gravação. */
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static void flag_path(const char *vol, char *out, size_t cap) {
    snprintf(out, cap, "%s%s.metadata_never_index", vol, vol[strlen(vol) - 1] == '/' ? "" : "/");
}
static int is_off(const char *vol) {
    char p[2048];
    flag_path(vol, p, sizeof p);
    return access(p, F_OK) == 0;
}
static int do_vol(const char *vol, char op, const char *arg) {
    char p[2048];
    if (access(vol, F_OK) != 0) { fprintf(stderr, "%s: Error: unknown volume.\n", vol); return 1; }
    printf("%s:\n", vol);
    if (op == 'i') {
        flag_path(vol, p, sizeof p);
        if (!strcmp(arg, "off")) {
            int fd = open(p, O_WRONLY | O_CREAT, 0644);
            if (fd < 0) { printf("\tError: could not write %s.\n", p); return 1; }
            close(fd);
        } else if (unlink(p) != 0 && access(p, F_OK) == 0) { printf("\tError: could not remove %s.\n", p); return 1; }
    }
    if (op == 'E') { printf("\tIndexing enabled. (o mOS busca direto no disco: não há índice para apagar)\n"); return 0; }
    printf(is_off(vol) ? "\tIndexing disabled.\n" : "\tIndexing enabled.\n");
    return 0;
}

int main(int argc, char **argv) {
    char op = 's';
    const char *arg = 0;
    int all = 0, i = 1, rc = 0, n = 0;
    for (; i < argc && argv[i][0] == '-'; i++) {
        const char *a = argv[i];
        if (!strcmp(a, "-s")) op = 's';
        else if (!strcmp(a, "-E")) op = 'E';
        else if (!strcmp(a, "-a")) all = 1;
        else if (!strcmp(a, "-v") || !strcmp(a, "-V")) continue;
        else if (!strcmp(a, "-i") && i + 1 < argc && (!strcmp(argv[i + 1], "on") || !strcmp(argv[i + 1], "off"))) { op = 'i'; arg = argv[++i]; }
        else { fprintf(stderr, "Usage: mdutil -pEsa -i (on|off) -d volume ...\n"); return 1; }
    }
    if (all) {
        rc |= do_vol("/", op, arg);
        n++;
        DIR *d = opendir("/Volumes");
        struct dirent *e;
        while (d && (e = readdir(d))) {
            if (e->d_name[0] == '.') continue;
            char v[1024];
            snprintf(v, sizeof v, "/Volumes/%s", e->d_name);
            rc |= do_vol(v, op, arg);
        }
        if (d) closedir(d);
    }
    for (; i < argc; i++, n++) rc |= do_vol(argv[i], op, arg);
    if (!n) { fprintf(stderr, "Usage: mdutil -pEsa -i (on|off) -d volume ...\n"); return 1; }
    return rc;
}
