/*
 * mOS libSystem — Data do Swift visto do C, pelo libswiftFoundation de verdade.
 *
 * O Data é do /usr/lib/swift/libswiftFoundation.dylib (swift-foundation, tools/build-swift-foundation-objc.sh). Quem
 * está em C no libSystem (o CryptoKit de ck_p256_swift.c) não monta a representação à mão: cria e lê pelas
 * funções públicas do próprio Foundation, então o __DataStorage é sempre o dele.
 *
 *   Data(bytes: UnsafeRawPointer, count: Int)      $s10Foundation4DataV5bytes5countACSV_SitcfC
 *       init estático, sem self: ponteiro e contagem em rdi/rsi; o Data volta em rax:rdx.
 *   Data._bridgeToObjectiveC() -> NSData            $s10Foundation4DataV19_bridgeToObjectiveCSo6NSDataCyF
 *       método não-mutating de struct: o self (dois words) vai como os últimos argumentos comuns; NSData +1.
 * No arm64 a mesma convenção vale com x0/x1 (o swiftcc do clang emite as duas).
 */
#include "foundation.h"
#include "libc.h"
#include <stdint.h>

void *dlopen(const char *path, int mode);
void *dlsym(void *h, const char *name);
id objc_autorelease(id o);
void objc_release(id o);

typedef struct { uint64_t w0, w1; } data_t;
typedef data_t (__attribute__((swiftcall)) *data_init_fn)(const void *bytes, long count);
typedef id (__attribute__((swiftcall)) *data_bridge_fn)(uint64_t w0, uint64_t w1);

static data_init_fn g_init;
static data_bridge_fn g_bridge;
static int g_tried;

static int load(void) {
    if (g_tried) return g_init && g_bridge;
    g_tried = 1;
    void *h = dlopen("/usr/lib/swift/libswiftFoundation.dylib", 0);
    if (!h) return 0;
    g_init = (data_init_fn)dlsym(h, "$s10Foundation4DataV5bytes5countACSV_SitcfC");
    g_bridge = (data_bridge_fn)dlsym(h, "$s10Foundation4DataV19_bridgeToObjectiveCSo6NSDataCyF");
    return g_init && g_bridge;
}

void fdn_rep_from_bytes(uint8_t r[16], const void *bytes, int64_t n) {
    static const uint8_t none[1];
    if (!load()) { memset(r, 0, 16); return; }
    /* vazio também pelo Foundation: Data(bytes:count: 0) devolve o caso .empty com o layout dele */
    data_t d = g_init(n > 0 && bytes ? bytes : none, n > 0 ? (long)n : 0);
    memcpy(r, &d.w0, 8);
    memcpy(r + 8, &d.w1, 8);
}

/* Bytes de um Data. O NSData da ponte fica no autorelease pool, então o ponteiro vale até o pool esvaziar. */
int fdn_rep_view(const uint8_t r[16], const uint8_t **out, int64_t *n, uint8_t tmp[14]) {
    uint64_t w0, w1;
    (void)tmp;
    *out = 0;
    *n = 0;
    if (!load()) return 0;
    memcpy(&w0, r, 8);
    memcpy(&w1, r + 8, 8);
    id ns = g_bridge(w0, w1);
    if (!ns) return 0;
    objc_autorelease(ns);
    *n = (int64_t)MSG0(u64, ns, "length");
    *out = MSG0(const uint8_t *, ns, "bytes");
    return 1;
}
