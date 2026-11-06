/*
 * mOS SDK (importador) — CommonDigest.h: SHA-1 e SHA-256 do CommonCrypto, implementados em
 * userland/libsystem/commoncrypto.c com o layout de contexto do macOS. Torna canImport(CommonCrypto) verdadeiro
 * (o OpenSwiftUI usa CC_SHA1 no StrongHash fora do Darwin quando não há swift-crypto).
 */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif
typedef uint32_t CC_LONG;
typedef uint64_t CC_LONG64;

#define CC_SHA1_DIGEST_LENGTH 20
#define CC_SHA1_BLOCK_BYTES 64
#define CC_SHA256_DIGEST_LENGTH 32
#define CC_SHA256_BLOCK_BYTES 64

typedef struct CC_SHA1state_st {
    CC_LONG h0, h1, h2, h3, h4;
    CC_LONG Nl, Nh;
    CC_LONG data[16];
    int num;
} CC_SHA1_CTX;

typedef struct CC_SHA256state_st {
    CC_LONG count[2];
    CC_LONG hash[8];
    CC_LONG wbuf[16];
} CC_SHA256_CTX;

int CC_SHA1_Init(CC_SHA1_CTX *c);
int CC_SHA1_Update(CC_SHA1_CTX *c, const void *data, CC_LONG len);
int CC_SHA1_Final(unsigned char *md, CC_SHA1_CTX *c);
unsigned char *CC_SHA1(const void *data, CC_LONG len, unsigned char *md);
int CC_SHA256_Init(CC_SHA256_CTX *c);
int CC_SHA256_Update(CC_SHA256_CTX *c, const void *data, CC_LONG len);
int CC_SHA256_Final(unsigned char *md, CC_SHA256_CTX *c);
unsigned char *CC_SHA256(const void *data, CC_LONG len, unsigned char *md);
#ifdef __cplusplus
}
#endif