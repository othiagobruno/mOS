/* mOS — o TLS da transação de observação do módulo Observation (swift-src stdlib/public/Observation): o
 * ThreadLocal.cpp original usa a camada de threads do runtime; aqui, uma chave pthread criada uma vez.
 * Convenção swiftcall, como no original. Ao contrário do original, ficam visíveis: o SwiftUI.framework do mOS (OpenSwiftUI)
 * lê a lista de acessos da transação pelo mesmo TLS (userland/sdk/swift-objc/SwiftUI/Core/MOSObservationTLS.swift). */
#include <pthread.h>

static pthread_key_t obs_key;
static pthread_once_t obs_once = PTHREAD_ONCE_INIT;
static void obs_key_make(void) { pthread_key_create(&obs_key, 0); }

__attribute__((swiftcall, visibility("default"))) void *_swift_observation_tls_get(void) {
    pthread_once(&obs_once, obs_key_make);
    return pthread_getspecific(obs_key);
}
__attribute__((swiftcall, visibility("default"))) void _swift_observation_tls_set(void *value) {
    pthread_once(&obs_once, obs_key_make);
    pthread_setspecific(obs_key, value);
}
