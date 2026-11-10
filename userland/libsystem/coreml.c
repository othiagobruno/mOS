/* mOS libSystem — CoreML só o que a voz do OpenBot importa.
 *
 * O machocheck do OpenBot 0.66.0 (docs/compat-open-usage.md) lista um único símbolo
 * de CoreML.framework ausente: OBJC_CLASS_$_MLDictionaryFeatureProvider. O restante
 * (MLFeatureValue, MLMultiArray, MLModel) já existe como classe vazia em
 * vscode_fwclasses.c. predictionFromFeatures: não está aqui: a libc++ do
 * onnxruntime e do sherpa-onnx é de outra frente.
 *
 * Funcional: initWithDictionary:error:, featureValueForName:, featureNames,
 * dictionary e objectForKeyedSubscript:. A chave tem de ser string. O valor é o
 * objeto que o chamador colocou (em geral um MLFeatureValue). Dicionário nulo ou
 * chave que não é string devolve nil e um NSError.
 */
#include "foundation.h"
#include "libc.h"
#include "coreml_dict.inc"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);

cx_class_t MLDictionaryFeatureProvider_c __asm__("OBJC_CLASS_$_MLDictionaryFeatureProvider");
cx_class_t MLDictionaryFeatureProvider_m __asm__("OBJC_METACLASS_$_MLDictionaryFeatureProvider");

typedef struct {
    cx_class_t *isa;
    ml_map map;
} ml_fp_t;

static id ml_err(const char *msg) {
    id info = cx_alloc_instance(cx_class_named("NSMutableDictionary"));
    id desc = cx_str(msg ? msg : "MLDictionaryFeatureProvider");
    id key = cx_str("NSLocalizedDescription");
    if (info && desc && key) MSG(void, info, "setObject:forKey:", desc, key);
    id err = MSG(id, (id)cx_class_named("NSError"), "errorWithDomain:code:userInfo:",
                 cx_str("com.apple.CoreML"), (i64)0, info);
    if (desc) objc_release(desc);
    if (key) objc_release(key);
    if (info) objc_release(info);
    return err;
}

static void ml_drop(ml_fp_t *o) {
    for (int i = 0; i < o->map.n; i++)
        if (o->map.e[i].value) objc_release((id)o->map.e[i].value);
    ml_free(&o->map);
}

static id ml_init(id self, SEL c, id dict, id *err) {
    (void)c;
    ml_fp_t *o = (ml_fp_t *)self;
    if (err) *err = 0;
    if (!self) return 0;
    if (!dict) {
        if (err) *err = ml_err("dicionário ausente");
        objc_release(self);
        return 0;
    }
    id keys = MSG0(id, dict, "allKeys");
    u64 n = keys ? MSG0(u64, keys, "count") : 0;
    for (u64 i = 0; i < n; i++) {
        id k = MSG(id, keys, "objectAtIndex:", i);
        if (!k || !cx_is_str(k)) {
            if (err) *err = ml_err("chave que não é string");
            ml_drop(o);
            if (keys) objc_release(keys);
            objc_release(self);
            return 0;
        }
        id v = MSG(id, dict, "objectForKey:", k);
        if (!v) continue;
        objc_retain(v);
        void *prev = 0;
        if (ml_put(&o->map, cx_cstr(k), v, &prev) != 0) {
            objc_release(v);
            if (err) *err = ml_err("sem memória");
            ml_drop(o);
            if (keys) objc_release(keys);
            objc_release(self);
            return 0;
        }
        if (prev) objc_release((id)prev);
    }
    if (keys) objc_release(keys);
    return self;
}

static id ml_feature(id self, SEL c, id name) {
    (void)c;
    if (!name || !cx_is_str(name)) return 0;
    return (id)ml_get(&((ml_fp_t *)self)->map, cx_cstr(name));
}

static id ml_names(id self, SEL c) {
    (void)c;
    ml_fp_t *o = (ml_fp_t *)self;
    id arr = cx_alloc_instance(cx_class_named("NSMutableArray"));
    for (int i = 0; i < o->map.n; i++) {
        id s = cx_str(o->map.e[i].name);
        if (s && arr) MSG(void, arr, "addObject:", s);
        if (s) objc_release(s);
    }
    id set = MSG(id, (id)cx_class_named("NSSet"), "setWithArray:", arr);
    if (arr) objc_release(arr);
    return set;
}

static id ml_dictionary(id self, SEL c) {
    (void)c;
    ml_fp_t *o = (ml_fp_t *)self;
    id d = cx_alloc_instance(cx_class_named("NSMutableDictionary"));
    for (int i = 0; i < o->map.n; i++) {
        if (!o->map.e[i].value) continue;
        id k = cx_str(o->map.e[i].name);
        if (k && d) MSG(void, d, "setObject:forKey:", (id)o->map.e[i].value, k);
        if (k) objc_release(k);
    }
    return objc_autorelease(d);
}

static u64 ml_count(id self, SEL c) {
    (void)c;
    return (u64)((ml_fp_t *)self)->map.n;
}

static void ml_dealloc(id self, SEL c) {
    ml_drop((ml_fp_t *)self);
    void (*sup)(id, SEL) = (void (*)(id, SEL))cx_lookup(cx_class_named("NSObject"), sel_registerName("dealloc"));
    if (sup) sup(self, c);
}

void cx_coreml_init(void) {
    if (cx_class_named("MLDictionaryFeatureProvider")) return;
    cx_class_t *obj = cx_class_named("NSObject");
    cx_define_class(&MLDictionaryFeatureProvider_c, &MLDictionaryFeatureProvider_m,
                    "MLDictionaryFeatureProvider", obj, (u32)(sizeof(ml_fp_t) - sizeof(cx_class_t *)));
    cx_add_method(&MLDictionaryFeatureProvider_c, "initWithDictionary:error:", (void *)ml_init);
    cx_add_method(&MLDictionaryFeatureProvider_c, "featureValueForName:", (void *)ml_feature);
    cx_add_method(&MLDictionaryFeatureProvider_c, "featureNames", (void *)ml_names);
    cx_add_method(&MLDictionaryFeatureProvider_c, "dictionary", (void *)ml_dictionary);
    cx_add_method(&MLDictionaryFeatureProvider_c, "objectForKeyedSubscript:", (void *)ml_feature);
    cx_add_method(&MLDictionaryFeatureProvider_c, "count", (void *)ml_count);
    cx_add_method(&MLDictionaryFeatureProvider_c, "dealloc", (void *)ml_dealloc);
}
