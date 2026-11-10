/* Corpo de moscd_* para o teste de host. A dylib chama os mesmos nomes no libSystem. */
#define _POSIX_C_SOURCE 200809L
#define CD_HOST 1
#include "../userland/libsystem/coredata_store.inc"

cd_store *moscd_open_file(const char *path) { return cd_open_file(path); }
cd_store *moscd_open_memory(void) { return cd_open_memory(); }
void moscd_close(cd_store *s) { cd_close(s); }
int moscd_save(cd_store *s) { return cd_save(s); }
unsigned long moscd_insert(cd_store *s, const char *entity) { return cd_insert(s, entity); }
void moscd_delete(cd_store *s, unsigned long id) { cd_delete(s, id); }
int moscd_set_str(cd_store *s, unsigned long id, const char *key, const char *val) { return cd_set_str(s, id, key, val); }
int moscd_set_i64(cd_store *s, unsigned long id, const char *key, long long v) { return cd_set_i64(s, id, key, v); }
int moscd_set_f64(cd_store *s, unsigned long id, const char *key, double v) { return cd_set_f64(s, id, key, v); }
int moscd_get_kind(cd_store *s, unsigned long id, const char *key) { return cd_get_kind(s, id, key); }
const char *moscd_get_str(cd_store *s, unsigned long id, const char *key) { return cd_get_str(s, id, key); }
long long moscd_get_i64(cd_store *s, unsigned long id, const char *key) { return cd_get_i64(s, id, key); }
double moscd_get_f64(cd_store *s, unsigned long id, const char *key) { return cd_get_f64(s, id, key); }
const char *moscd_entity_of(cd_store *s, unsigned long id) { return cd_entity_of(s, id); }
unsigned long *moscd_fetch(cd_store *s, const char *entity, const char *pk, const char *pv,
                           unsigned limit, unsigned offset, unsigned *n) {
    return cd_fetch(s, entity, pk, pv, limit, offset, n);
}

static void moscd_unquote(char *s) {
    size_t n;
    if (!s) return;
    if (s[0] != '"' && s[0] != '\'') return;
    n = strlen(s);
    if (n >= 2 && s[n - 1] == s[0]) {
        memmove(s, s + 1, n - 2);
        s[n - 2] = 0;
    }
}

int moscd_split_eq(const char *fmt, char *key, unsigned key_n, char *val, unsigned val_n) {
    const char *p, *eq;
    unsigned kn, vn;
    if (!fmt || !key || !val || key_n < 2 || val_n < 2) return 0;
    p = fmt;
    while (*p == ' ') p++;
    eq = strstr(p, "==");
    if (!eq || eq == p) return 0;
    kn = (unsigned)(eq - p);
    while (kn && p[kn - 1] == ' ') kn--;
    if (!kn || kn >= key_n) return 0;
    memcpy(key, p, kn);
    key[kn] = 0;
    p = eq + 2;
    while (*p == ' ') p++;
    vn = (unsigned)strlen(p);
    while (vn && p[vn - 1] == ' ') vn--;
    if (vn >= val_n) return 0;
    memcpy(val, p, vn);
    val[vn] = 0;
    moscd_unquote(val);
    return key[0] != 0;
}
