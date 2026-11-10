/* Teste de host dos símbolos Swift de Data e das pontes do OpenBot. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
typedef void *id;
typedef struct { unsigned long long w0, w1; } swstr;
typedef struct ds ds_t;
typedef struct i8arr { void *isa; unsigned long long rc; long long count; long long cap; signed char el[]; } i8arr_t;

ds_t *fdn_storage_init(const void *b, long long n);
void *fdn_storage_bytes(ds_t *d);
long long fdn_storage_length(ds_t *d);
long long fdn_storage_offset(ds_t *d);
void *fdn_storage_metadata(void);
void *fdn_range_metadata(void);
void *fdn_url_metadata(void);
void *fdn_encoding_metadata(void);
void fdn_rep_from_bytes(unsigned char r[16], const void *b, long long n);
void fdn_rep_append(unsigned char r[16], const void *b, long long n);
int fdn_rep_copy(const unsigned char r[16], void *dst, long long nb, long long lo, long long hi, int has);
swstr fdn_pack(const char *s, int n);
int fdn_unpack(swstr s, char *buf, int cap);
id fdn_bridge_string(swstr s);
swstr fdn_string_from_ns(id o);
void fdn_url_init_path(id *slot, swstr path);
swstr fdn_url_path(id *slot);
id fdn_bridge_url(id *slot);
id fdn_url_from_ns(id u);
id fdn_bridge_dict(id d);
id fdn_bridge_array(id a);
id fdn_array_from_ns(id a);
id fdn_dict_literal_pairs(const unsigned char *e, long n, id self);
i8arr_t *fdn_cstring(swstr s, unsigned long long enc);
void fdn_error_from_ns(void *dst, id err);
swstr fdn_error_description(void *box);
id fdn_nserror_from_error(void *box);
id fdn_data_bridge(const unsigned char r[16]);
void fdn_data_from_ns(unsigned char r[16], id data);
void fdn_force_bridge_string(id ns, swstr *out);
int fdn_cond_bridge_string(id ns, swstr *out);
int fdn_cond_bridge_array(id ns, id *out);
unsigned long long fdn_with_unsafe(const void *p, long long n, void *fn, void *ctx, unsigned long long *hi);

static int fails, checks;
static void ok(int c, const char *m) { checks++; if (!c) { fails++; fprintf(stderr, "falhou: %s\n", m); } }

enum { K_STR = 1, K_URL, K_DATA, K_ARR, K_DICT, K_ERR };
typedef struct obj {
    int kind, n, rc;
    char *s;
    struct obj *path;
    unsigned char *bytes;
    struct obj *items[8];
    struct obj *keys[8];
    struct obj *vals[8];
} obj;

static obj *neu(int k) { obj *o = calloc(1, sizeof *o); o->kind = k; o->rc = 1; return o; }
id objc_retain(id o) { if (o) ((obj *)o)->rc++; return o; }
void objc_release(id o) { (void)o; }
id cx_str(const char *s) { obj *o = neu(K_STR); o->s = strdup(s ? s : ""); o->n = (int)strlen(o->s); return o; }
id cx_str_n(const char *s, unsigned long n) {
    obj *o = neu(K_STR); o->s = calloc(1, n + 1); memcpy(o->s, s ? s : "", n); o->n = (int)n; return o;
}
const char *cx_cstr(id o) { return o && ((obj *)o)->kind == K_STR ? ((obj *)o)->s : ""; }

id fdn_host_msg(id o, const char *sel, uintptr_t a, uintptr_t b, int n) {
    obj *x = o;
    (void)n;
    if (!strcmp(sel, "fileURLWithPath:")) {
        obj *u = neu(K_URL); u->path = (obj *)a;
        return u;
    }
    if (!strcmp(sel, "path")) return x ? x->path : 0;
    if (!strcmp(sel, "dataWithBytes:length:")) {
        obj *d = neu(K_DATA);
        d->n = (int)b;
        d->bytes = calloc(1, b ? b : 1);
        if (a && b) memcpy(d->bytes, (void *)a, b);
        return d;
    }
    if (!strcmp(sel, "bytes")) return x ? (id)x->bytes : 0;
    if (!strcmp(sel, "length")) return x ? (id)(uintptr_t)x->n : 0;
    if (!strcmp(sel, "dictionary")) return neu(K_DICT);
    if (!strcmp(sel, "array")) return neu(K_ARR);
    if (!strcmp(sel, "setObject:forKey:") && x && x->n < 8) {
        x->vals[x->n] = (obj *)a;
        x->keys[x->n] = (obj *)b;
        x->n++;
        return 0;
    }
    if (!strcmp(sel, "count")) return x ? (id)(uintptr_t)x->n : 0;
    if (!strcmp(sel, "objectAtIndex:")) {
        int i = (int)a;
        return x && i >= 0 && i < x->n ? x->items[i] : 0;
    }
    if (!strcmp(sel, "localizedDescription")) return x && x->s ? cx_str(x->s) : cx_str("Erro");
    return 0;
}

static unsigned long long body_sum(void *ctx, const void *p, long long n, unsigned long long *hi) {
    const unsigned char *b = p;
    unsigned long long s = 0;
    long long i;
    (void)ctx; (void)hi;
    for (i = 0; i < n; i++) s += b[i];
    return s;
}

int main(void) {
    unsigned char rep[16], out[32];
    ds_t *st;
    swstr s, back;
    char buf[64];
    id url = 0, ns, arr, dict;
    unsigned char pairs[64];
    obj *err;
    void *box[5];
    i8arr_t *cs;
    int n;

    st = fdn_storage_init("abcd", 4);
    ok(st && fdn_storage_length(st) == 4 && fdn_storage_offset(st) == 0, "storage");
    ok(memcmp(fdn_storage_bytes(st), "abcd", 4) == 0, "bytes");
    ok(fdn_storage_metadata() && fdn_range_metadata() && fdn_url_metadata() && fdn_encoding_metadata(), "metadata");

    fdn_rep_from_bytes(rep, "hi", 2);
    ok(rep[14] == 2 && rep[15] == 0 && rep[0] == 'h', "inline");
    fdn_rep_append(rep, "!", 1);
    ok(rep[14] == 3 && memcmp(rep, "hi!", 3) == 0, "append inline");
    n = fdn_rep_copy(rep, out, 8, 1, 3, 1);
    ok(n == 2 && memcmp(out, "i!", 2) == 0, "copy range");

    fdn_rep_from_bytes(rep, "0123456789abcdef", 16);
    ok((rep[15] & 0xC0) == 0x40, "slice tag");
    fdn_rep_append(rep, "XY", 2);
    n = fdn_rep_copy(rep, out, 32, 0, 0, 0);
    ok(n == 18 && memcmp(out, "0123456789abcdefXY", 18) == 0, "append slice");

    ns = fdn_data_bridge(rep);
    ok(ns && ((obj *)ns)->n == 18, "bridge nsdata");
    fdn_data_from_ns(rep, ns);
    n = fdn_rep_copy(rep, out, 32, 0, 0, 0);
    ok(n == 18 && out[16] == 'X', "from nsdata");

    ok(fdn_with_unsafe("ABC", 3, (void *)body_sum, 0, 0) == (unsigned)'A' + 'B' + 'C', "contiguous");

    s = fdn_pack("ola", 3);
    ok(fdn_unpack(s, buf, 64) == 3 && strcmp(buf, "ola") == 0, "small");
    s = fdn_pack("abcdefghijklmnopqrstuvwxyz", 26);
    ok(fdn_unpack(s, buf, 64) == 26 && strcmp(buf, "abcdefghijklmnopqrstuvwxyz") == 0, "large");
    ns = fdn_bridge_string(s);
    ok(ns && strcmp(cx_cstr(ns), "abcdefghijklmnopqrstuvwxyz") == 0, "string to ns");
    back = fdn_string_from_ns(ns);
    ok(fdn_unpack(back, buf, 64) == 26, "string from ns");
    fdn_force_bridge_string(ns, &back);
    ok(fdn_unpack(back, buf, 64) == 26, "force");
    ok(fdn_cond_bridge_string(ns, &back) == 1, "cond string");
    ok(fdn_cond_bridge_string(0, &back) == 0, "cond string nil");

    s = fdn_pack("/tmp/a", 6);
    fdn_url_init_path(&url, s);
    ok(url && strcmp(cx_cstr(((obj *)url)->path), "/tmp/a") == 0, "file url");
    back = fdn_url_path(&url);
    ok(fdn_unpack(back, buf, 64) == 6 && strcmp(buf, "/tmp/a") == 0, "url path");
    ok(fdn_bridge_url(&url) == url && ((obj *)url)->rc == 2, "url bridge");
    ok(fdn_url_from_ns(url) == url, "url from ns");

    memset(pairs, 0, sizeof pairs);
    *(id *)pairs = cx_str("k");
    *(id *)(pairs + 32) = cx_str("v");
    dict = fdn_dict_literal_pairs(pairs, 1, 0);
    ok(dict && ((obj *)dict)->n == 1 && strcmp(((obj *)dict)->vals[0]->s, "v") == 0, "literal");
    ok(fdn_bridge_dict(dict) == dict, "dict bridge");

    {
        obj *arrns = neu(K_ARR);
        arrns->n = 2;
        arrns->items[0] = cx_str("a");
        arrns->items[1] = cx_str("b");
        arr = fdn_array_from_ns(arrns);
        ok(arr && ((i8arr_t *)arr)->count == 2, "array from ns");
        ok(fdn_cond_bridge_array(arrns, &arr) == 1, "cond array");
        ok(fdn_bridge_array(arr) == arr, "array bridge");
    }

    s = fdn_pack("xyz", 3);
    cs = fdn_cstring(s, 4);
    ok(cs && cs->count == 4 && cs->el[0] == 'x' && cs->el[3] == 0, "cstring");
    ok(fdn_cstring(s, 99) == 0, "cstring encoding");

    err = neu(K_ERR);
    err->s = "falhou o arquivo";
    fdn_error_from_ns(box, err);
    back = fdn_error_description(box);
    ok(fdn_unpack(back, buf, 64) > 0 && strcmp(buf, "falhou o arquivo") == 0, "localized");
    ok(fdn_nserror_from_error(box) == (id)err, "convert error");

    if (fails) { fprintf(stderr, "foundation-swift: %d falhas em %d\n", fails, checks); return 1; }
    printf("foundation-swift: %d/%d ok\n", checks, checks);
    return 0;
}
