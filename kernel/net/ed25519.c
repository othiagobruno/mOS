/* mOS — verificação de assinaturas Ed25519 (RFC 8032). A implementação fica em include/ed25519_impl.h, a mesma que o
 * mkimg usa no host para assinar o manifesto do payload (pacotes de atualização: fs/update.c). */
#include "crypto.h"
#define ED_TYPES_DEFINED
typedef u8 ed_u8;
typedef u32 ed_u32;
typedef u64 ed_u64;
typedef i64 ed_i64;
typedef usize ed_size;
#include "ed25519_impl.h"

bool ed25519_check(const u8 sig[64], const void *msg, usize len, const u8 pk[32]) {
    return ed25519_verify(sig, msg, len, pk) == 1;
}
