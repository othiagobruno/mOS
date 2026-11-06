/* Ferramenta de host: mostra a árvore de objetos de um .nib (NIBArchive). Uso: nibdump arquivo.nib [obj] [prof] */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../userland/libsystem/nibarchive.h"

static nib_t n;
static unsigned char *seen;
static void *al(unsigned long s) { return calloc(1, s); }
static void dump(unsigned o, int depth, int maxd) {
    const nib_obj_t *ob = &n.obj[o];
    const char *cls = nib_class(&n, o);
    printf("#%u %s\n", o, cls);
    if (seen[o] || depth >= maxd) return;
    seen[o] = 1;
    for (unsigned i = 0; i < ob->count; i++) {
        const nib_val_t *v = &n.val[ob->first + i];
        printf("%*s%s = ", depth * 2 + 2, "", n.key[v->key]);
        switch (v->type) {
        case NV_TRUE: printf("YES\n"); break;
        case NV_FALSE: printf("NO\n"); break;
        case NV_NIL: printf("nil\n"); break;
        case NV_FLOAT: case NV_DOUBLE: printf("%g\n", v->d); break;
        case NV_DATA: {
            int txt = 1;
            for (unsigned k = 0; k < v->len; k++) if (v->data[k] < 9 && v->data[k]) txt = 0;
            if (txt) printf("\"%.*s\"\n", (int)(v->len > 80 ? 80 : v->len), v->data);
            else printf("<%u bytes>\n", v->len);
            break;
        }
        case NV_OBJ: {
            const char *c = nib_class(&n, v->obj);
            if (!strcmp(c, "NSString") || !strcmp(c, "NSMutableString") || !strcmp(c, "NSLocalizableString")) {
                const nib_val_t *b = nib_get(&n, v->obj, "NS.bytes");
                printf("@\"%.*s\"\n", b ? (int)(b->len > 80 ? 80 : b->len) : 0, b ? (const char *)b->data : "");
            } else { printf(""); dump(v->obj, depth + 1, maxd); }
            break;
        }
        default: printf("%lld\n", v->i);
        }
    }
}
int main(int argc, char **argv) {
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 2;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *b = malloc(sz);
    fread(b, 1, sz, f);
    if (!nib_parse(&n, b, sz, al)) { printf("não é NIBArchive válido\n"); return 1; }
    printf("objetos %u, chaves %u, valores %u, classes %u\n", n.nobj, n.nkey, n.nval, n.ncls);
    seen = calloc(n.nobj, 1);
    dump(argc > 2 ? atoi(argv[2]) : 0, 0, argc > 3 ? atoi(argv[3]) : 6);
    return 0;
}
