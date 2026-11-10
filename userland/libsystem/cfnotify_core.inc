/* Tabela de observadores do CFNotificationCenter, sem ObjC e sem notifyd.
 * O centro (ponteiro opaco), o nome (NULL = qualquer) e o objeto (NULL = qualquer)
 * filtram a entrega. A entrega é síncrona. Remover durante o callback vale para
 * os observadores que ainda não foram chamados.
 *
 * Incluído por cfnotify.c e por tools/cfnotifytest.c.
 */
#ifdef CFN_HOST
#include <stdlib.h>
#include <string.h>
#endif

typedef void (*cfn_fn)(void *center, void *observer, void *name_token, void *object, void *info);

typedef struct cfn_obs {
    void *center;
    void *observer;
    cfn_fn fn;
    char *name; /* NULL = todos os nomes */
    void *object; /* NULL = qualquer objeto */
    int alive;
    struct cfn_obs *next;
} cfn_obs;

typedef struct cfn_tab {
    cfn_obs *head;
    int posting;
} cfn_tab;

static cfn_tab *cfn_new(void) { return (cfn_tab *)calloc(1, sizeof(cfn_tab)); }

static void cfn_free(cfn_tab *t) {
    if (!t) return;
    for (cfn_obs *o = t->head; o;) {
        cfn_obs *n = o->next;
        free(o->name);
        free(o);
        o = n;
    }
    free(t);
}

static void cfn_add(cfn_tab *t, void *center, void *observer, cfn_fn fn, const char *name, void *object) {
    if (!t || !fn) return;
    cfn_obs *o = (cfn_obs *)calloc(1, sizeof *o);
    if (!o) return;
    o->center = center;
    o->observer = observer;
    o->fn = fn;
    o->object = object;
    o->alive = 1;
    if (name) {
        o->name = strdup(name);
        if (!o->name) { free(o); return; }
    }
    o->next = t->head;
    t->head = o;
}

static int cfn_match_name(const cfn_obs *o, const char *name) {
    if (!o->name) return 1;
    if (!name) return 0;
    return strcmp(o->name, name) == 0;
}

/* any_name / any_object tiram o filtro correspondente. Nome NULL no pedido, sem any_name,
 * só casa com observador que também não filtrou nome. */
static void cfn_remove(cfn_tab *t, void *center, void *observer, const char *name, void *object, int any_name, int any_object) {
    if (!t) return;
    for (cfn_obs *o = t->head; o; o = o->next) {
        if (!o->alive || o->center != center || o->observer != observer) continue;
        if (!any_name) {
            int same = (o->name == 0 && name == 0) || (o->name && name && strcmp(o->name, name) == 0);
            if (!same) continue;
        }
        if (!any_object && o->object != object) continue;
        o->alive = 0;
    }
}

static int cfn_count(cfn_tab *t, void *center, const char *name) {
    int n = 0;
    if (!t) return 0;
    for (cfn_obs *o = t->head; o; o = o->next) {
        if (!o->alive || o->center != center) continue;
        if (name && !cfn_match_name(o, name)) continue;
        n++;
    }
    return n;
}

/* devolve quantos callbacks rodaram */
static int cfn_post(cfn_tab *t, void *center, const char *name, void *name_token, void *object, void *info) {
    if (!t) return 0;
    cfn_obs *snap[128];
    int n = 0;
    for (cfn_obs *o = t->head; o && n < 128; o = o->next) {
        if (!o->alive || o->center != center) continue;
        if (!cfn_match_name(o, name)) continue;
        if (o->object && o->object != object) continue;
        snap[n++] = o;
    }
    t->posting++;
    int ran = 0;
    for (int i = 0; i < n; i++) {
        if (!snap[i]->alive) continue;
        snap[i]->fn(center, snap[i]->observer, name_token, object, info);
        ran++;
    }
    t->posting--;
    if (!t->posting) {
        cfn_obs **pp = &t->head;
        while (*pp) {
            if (!(*pp)->alive) {
                cfn_obs *d = *pp;
                *pp = d->next;
                free(d->name);
                free(d);
            } else pp = &(*pp)->next;
        }
    }
    return ran;
}
