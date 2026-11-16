/* Configuração do libopus 1.5.2 para o libSystem do mOS (freestanding, x86_64 e arm64).
 * Ponto fixo: o mesmo resultado em qualquer CPU e sem depender da precisão do libm.
 * Sem detecção de CPU em runtime nem intrínsecos: o código C genérico. */
#ifndef MOS_OPUS_CONFIG_H
#define MOS_OPUS_CONFIG_H
#define OPUS_BUILD 1
#define FIXED_POINT 1
#define VAR_ARRAYS 1
#define HAVE_LRINT 1
#define HAVE_LRINTF 1
/* arredondamento par como o lrint da libm, em instrução (frintx/cvtsd2si), sem chamada à libm */
#define lrint(x) ((long)__builtin_rint(x))
#define lrintf(x) ((long)__builtin_rintf(x))
#define PACKAGE_VERSION "1.5.2"
/* a API do libopus não é exportação do libSystem do macOS: fica escondida, só a casca cx_opus_* aparece */
#define OPUS_EXPORT
#endif
