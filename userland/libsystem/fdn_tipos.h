/* Tipos-valor do overlay Swift do Foundation (fdn_tipos.c, fdn_meta.S). */
#pragma once
#include <stdint.h>
#include <stddef.h>

/* String Swift em dois words (x86_64): countAndFlags e objeto/discriminador. */
typedef struct { uint64_t w0, w1; } fdn_str;

/* Tabela de value witnesses (ABI Swift 5, 64 bits). As funções usam swiftcc. */
typedef struct fdn_vwt {
    void *(__attribute__((swiftcall)) *init_buffer_copy)(void *dst, void *src, const void *md);
    void (__attribute__((swiftcall)) *destroy)(void *obj, const void *md);
    void *(__attribute__((swiftcall)) *init_copy)(void *dst, void *src, const void *md);
    void *(__attribute__((swiftcall)) *assign_copy)(void *dst, void *src, const void *md);
    void *(__attribute__((swiftcall)) *init_take)(void *dst, void *src, const void *md);
    void *(__attribute__((swiftcall)) *assign_take)(void *dst, void *src, const void *md);
    unsigned (__attribute__((swiftcall)) *get_enum_tag_single_payload)(const void *v, unsigned empty, const void *md);
    void (__attribute__((swiftcall)) *store_enum_tag_single_payload)(void *v, unsigned which, unsigned empty, const void *md);
    size_t size;
    size_t stride;
    uint32_t flags;
    uint32_t extra_inhabitants;
} fdn_vwt;

extern const fdn_vwt fdn_vwt_pod8, fdn_vwt_pod16, fdn_vwt_obj;

/* Enum sem payload: a tabela base e as três testemunhas de enum (flag 0x200000). */
typedef struct fdn_evwt {
    fdn_vwt base;
    unsigned (__attribute__((swiftcall)) *get_enum_tag)(const void *v, const void *md);
    void (__attribute__((swiftcall)) *destructive_project_enum_data)(void *v, const void *md);
    void (__attribute__((swiftcall)) *destructive_inject_enum_tag)(void *v, unsigned tag, const void *md);
} fdn_evwt;

extern const fdn_evwt fdn_evwt_tag1;

fdn_str fdn_str_make(const char *s, long n);
long fdn_str_read(fdn_str s, char *buf, long cap);
unsigned fdn_enum_tag_get(const void *value, unsigned empty, size_t size);
void fdn_enum_tag_store(void *value, unsigned which, unsigned empty, size_t size);
int fdn_date_text(double ref, char *buf, int cap);
int fdn_uuid_parse(const char *s, long n, uint8_t out[16]);
void fdn_uuid_format(const uint8_t u[16], char out[37]);

/* Lado Objective-C: fdn_tipos_ns.c no libSystem, falso no teste de host. Objetos novos voltam com +1. */
void fdn_ns_retain(void *o);
void fdn_ns_release(void *o);
const char *fdn_ns_utf8(void *nsstring);
void *fdn_ns_string(const char *s);
double fdn_ns_now1970(void);
void fdn_ns_random(void *buf, size_t n);
void *fdn_ns_date_new(double t1970);
double fdn_ns_date_t1970(void *date);
void *fdn_ns_uuid_new(const uint8_t b[16]);
void fdn_ns_uuid_bytes(void *uuid, uint8_t out[16]);
void *fdn_ns_note_new(void *name, void *object, void *info);
void *fdn_ns_note_name(void *note);
void *fdn_ns_note_info(void *note);

/* NSURL (fdn_url.c). As partes voltam como C string do objeto (sem +1) ou NULL. */
enum { FDN_URL_PATH, FDN_URL_ABS, FDN_URL_REL, FDN_URL_SCHEME, FDN_URL_HOST, FDN_URL_USER, FDN_URL_PASSWORD,
       FDN_URL_QUERY, FDN_URL_FRAGMENT };
enum { FDN_URL_OP_RESOLVE, FDN_URL_OP_ABSOLUTE };
void *fdn_ns_url_string(const char *s);
void *fdn_ns_url_file(const char *path);
const char *fdn_ns_url_part(void *url, int part);
int fdn_ns_url_isfile(void *url);
long fdn_ns_url_port(void *url, int *has);
void *fdn_ns_url_file_op(void *url, int op);
int fdn_ns_isdir(const char *path);
long fdn_ns_hash(void *o);
int fdn_ns_equal(void *a, void *b);

/* NSLocale / NSTimeZone (fdn_local.c). */
enum { FDN_LOC_ID, FDN_LOC_LANG, FDN_LOC_REGION, FDN_LOC_CURRENCY, FDN_LOC_DECIMAL, FDN_LOC_GROUPING };
const char *fdn_ns_loc_current_id(void);
void *fdn_ns_loc_obj(const char *id);
const char *fdn_ns_loc_obj_id(void *nslocale);
int fdn_loc_part(const char *id, int part, char *out, int cap);
void *fdn_ns_tz_current(void);
void *fdn_ns_tz_with_name(const char *name);
void *fdn_ns_tz_with_abbr(const char *abbr);
void *fdn_ns_tz_with_offset(long secs);
const char *fdn_ns_tz_name(void *tz);
long fdn_ns_tz_offset(void *tz, double t1970);
const char *fdn_ns_tz_abbr(void *tz, double t1970);

/* CharacterSet (fdn_cset.c): o valor é um ponteiro para um bloco com contagem de referências. */
typedef struct fdn_cset fdn_cset;
extern const fdn_vwt fdn_vwt_cset;
int fdn_cset_contains(const fdn_cset *c, uint32_t u);
fdn_cset *fdn_cset_from_utf8(const char *s, long n);

/* Caixa com contagem de referências (fdn_valores.c): o valor guarda o ponteiro. */
typedef struct fdn_box { long rc; void (*dtor)(void *); } fdn_box;
extern const fdn_vwt fdn_vwt_box;
void *fdn_box_new(size_t size, void (*dtor)(void *));
void fdn_box_retain(void *b);
void fdn_box_release(void *b);
void *fdn_query_item_new(const char *name, const char *value);
const char *fdn_query_item_name(void *q);
const char *fdn_query_item_value(void *q);

/* URLComponents (fdn_urlcomp.c) */
int fdn_urlcomp_parse(const char *s, void **out);
char *fdn_urlcomp_string(const void *u);
char *fdn_pct_encode(int comp, const char *s);
char *fdn_pct_decode(const char *s);
