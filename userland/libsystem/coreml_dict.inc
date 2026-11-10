/* Mapa nome → valor do MLDictionaryFeatureProvider.
 * Chave vazia é recusada. Nome repetido troca o valor e devolve o anterior em *prev
 * (quem chama libera). Valor ausente em ml_get é NULL.
 * Não é o modelo: MLModel continua sem predictionFromFeatures.
 */
typedef struct {
    char *name;
    void *value;
} ml_ent;

typedef struct {
    ml_ent *e;
    int n, cap;
} ml_map;

static int ml_put(ml_map *m, const char *name, void *value, void **prev) {
    if (prev) *prev = 0;
    if (!m || !name || !name[0]) return -1;
    for (int i = 0; i < m->n; i++) {
        if (strcmp(m->e[i].name, name) == 0) {
            if (prev) *prev = m->e[i].value;
            m->e[i].value = value;
            return 0;
        }
    }
    if (m->n == m->cap) {
        int c = m->cap ? m->cap * 2 : 4;
        ml_ent *ne = (ml_ent *)realloc(m->e, (size_t)c * sizeof *ne);
        if (!ne) return -1;
        m->e = ne;
        m->cap = c;
    }
    char *copy = strdup(name);
    if (!copy) return -1;
    m->e[m->n].name = copy;
    m->e[m->n].value = value;
    m->n++;
    return 0;
}

static void *ml_get(const ml_map *m, const char *name) {
    if (!m || !name) return 0;
    for (int i = 0; i < m->n; i++)
        if (strcmp(m->e[i].name, name) == 0) return m->e[i].value;
    return 0;
}

static const char *ml_name_at(const ml_map *m, int i) {
    if (!m || i < 0 || i >= m->n) return 0;
    return m->e[i].name;
}

static void ml_free(ml_map *m) {
    if (!m) return;
    for (int i = 0; i < m->n; i++) free(m->e[i].name);
    free(m->e);
    m->e = 0;
    m->n = m->cap = 0;
}
