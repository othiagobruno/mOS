/* API da loja SQLite usada pela dylib CoreData.framework do disco.
 * A implementação é userland/libsystem/coredata_store.inc (mesmas tabelas mos_object e mos_attr). */
#pragma once
typedef struct cd_store cd_store;

cd_store *moscd_open_file(const char *path);
cd_store *moscd_open_memory(void);
void moscd_close(cd_store *s);
int moscd_save(cd_store *s);
unsigned long moscd_insert(cd_store *s, const char *entity);
void moscd_delete(cd_store *s, unsigned long id);
int moscd_set_str(cd_store *s, unsigned long id, const char *key, const char *val);
int moscd_set_i64(cd_store *s, unsigned long id, const char *key, long long v);
int moscd_set_f64(cd_store *s, unsigned long id, const char *key, double v);
int moscd_get_kind(cd_store *s, unsigned long id, const char *key);
const char *moscd_get_str(cd_store *s, unsigned long id, const char *key);
long long moscd_get_i64(cd_store *s, unsigned long id, const char *key);
double moscd_get_f64(cd_store *s, unsigned long id, const char *key);
const char *moscd_entity_of(cd_store *s, unsigned long id);
unsigned long *moscd_fetch(cd_store *s, const char *entity, const char *pk, const char *pv,
                           unsigned limit, unsigned offset, unsigned *n);
/* "chave == valor" ou "chave == \"valor\"". Devolve 1 se reconheceu. */
int moscd_split_eq(const char *fmt, char *key, unsigned key_n, char *val, unsigned val_n);
