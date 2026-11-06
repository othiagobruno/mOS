/* mOS — primitivas criptográficas (kernel/net/crypto.c)
 *   SHA-256, SHA-384/512, HMAC, HKDF (RFC 5869 / RFC 8446), X25519 (RFC 7748),
 *   ChaCha20-Poly1305 (RFC 8439), AES-128/256-GCM e um CSPRNG ChaCha20.
 * Tudo escrito do zero, sem float, sem libc. */
#pragma once
#ifdef MOS_HOST_TEST
#include "tls_shim.h"
#else
#include "types.h"
#endif

/* ---------- hashes ---------- */
#define SHA256_LEN 32
#define SHA384_LEN 48
#define HASH_MAX_LEN 64

typedef struct { u32 h[8]; u8 buf[64]; u64 total; u32 n; } sha256_ctx;
typedef struct { u64 h[8]; u8 buf[128]; u64 total; u32 n; u32 outlen; } sha512_ctx;

void sha256_init(sha256_ctx *c);
void sha256_update(sha256_ctx *c, const void *data, usize len);
void sha256_final(sha256_ctx *c, u8 out[32]);
void sha256(const void *data, usize len, u8 out[32]);
/* SHA-1: só para identificadores do OCSP (CertID/ResponderID), nunca para assinaturas */
void sha1(const void *data, usize len, u8 out[20]);

void sha384_init(sha512_ctx *c);
void sha512_init(sha512_ctx *c);
void sha512_update(sha512_ctx *c, const void *data, usize len);
void sha512_final(sha512_ctx *c, u8 *out);      /* 48 bytes (384) ou 64 bytes (512) */

/* hash genérico (usado pelo transcript do TLS) */
enum { HASH_SHA256 = 1, HASH_SHA384 = 2, HASH_SHA512 = 3 };
typedef struct { int alg; union { sha256_ctx s256; sha512_ctx s512; } u; } hash_ctx;
u32  hash_len(int alg);
void hash_init(hash_ctx *c, int alg);
void hash_update(hash_ctx *c, const void *data, usize len);
void hash_final(hash_ctx *c, u8 *out);
void hash_peek(const hash_ctx *c, u8 *out);     /* hash parcial sem destruir o contexto */
void hash_buf(int alg, const void *data, usize len, u8 *out);

/* ---------- HMAC / HKDF ---------- */
void hmac(int alg, const u8 *key, usize klen, const void *msg, usize mlen, u8 *out);
void hkdf_extract(int alg, const u8 *salt, usize slen, const u8 *ikm, usize ilen, u8 *prk);
void hkdf_expand(int alg, const u8 *prk, const u8 *info, usize ilen, u8 *out, usize olen);
/* HKDF-Expand-Label do TLS 1.3 (rótulo sem o prefixo "tls13 ") */
void hkdf_expand_label(int alg, const u8 *secret, const char *label,
                       const u8 *ctx, usize clen, u8 *out, usize olen);

/* ---------- X25519 ---------- */
void x25519(u8 out[32], const u8 scalar[32], const u8 point[32]);
void x25519_base(u8 out[32], const u8 scalar[32]);

/* ---------- ChaCha20 / Poly1305 ---------- */
void chacha20_block(const u8 key[32], u32 counter, const u8 nonce[12], u8 out[64]);
void chacha20_xor(const u8 key[32], u32 counter, const u8 nonce[12], const u8 *in, u8 *out, usize len);
typedef struct { u32 r[5], h[5], pad[4]; u8 buf[16]; u32 n; } poly1305_ctx;
void poly1305_init(poly1305_ctx *c, const u8 key[32]);
void poly1305_update(poly1305_ctx *c, const u8 *m, usize len);
void poly1305_final(poly1305_ctx *c, u8 tag[16]);
/* AEAD (RFC 8439). in == out permitido. open devolve 0 se ok, -1 se tag inválida. */
void chacha20poly1305_seal(const u8 key[32], const u8 nonce[12], const u8 *aad, usize alen,
                           const u8 *in, usize len, u8 *out, u8 tag[16]);
int  chacha20poly1305_open(const u8 key[32], const u8 nonce[12], const u8 *aad, usize alen,
                           const u8 *in, usize len, u8 *out, const u8 tag[16]);

/* ---------- AES / GCM ---------- */
typedef struct { u8 rk[240]; int rounds; } aes_ctx;
void aes_setkey(aes_ctx *c, const u8 *key, u32 keylen);   /* 16 ou 32 bytes */
void aes_encrypt_block(const aes_ctx *c, const u8 in[16], u8 out[16]);
typedef struct { aes_ctx aes; u64 hh, hl; } aes_gcm_ctx;
void aes_gcm_init(aes_gcm_ctx *c, const u8 *key, u32 keylen);
void aes_gcm_seal(const aes_gcm_ctx *c, const u8 iv[12], const u8 *aad, usize alen,
                  const u8 *in, usize len, u8 *out, u8 tag[16]);
int  aes_gcm_open(const aes_gcm_ctx *c, const u8 iv[12], const u8 *aad, usize alen,
                  const u8 *in, usize len, u8 *out, const u8 tag[16]);

/* ---------- utilidades ---------- */
int  ct_memeq(const void *a, const void *b, usize n);    /* 1 se iguais (tempo constante) */
void secure_zero(void *p, usize n);

/* ---------- escalonamento de chaves TLS 1.3 (implementado em tls.c) ---------- */
void tls13_derive_handshake(int alg, const u8 *shared, u32 slen, const u8 *th_ch_sh,
                            u8 *hs_secret, u8 *c_hs, u8 *s_hs, u8 *master);
void tls13_derive_app(int alg, const u8 *master, const u8 *th_through_sfin, u8 *c_ap, u8 *s_ap);
void tls13_traffic_keys(int alg, const u8 *secret, u32 keylen, u8 *key, u8 *iv);
void tls13_finished(int alg, const u8 *base_secret, const u8 *th, u8 *out);
/* PRF do TLS 1.2 (RFC 5246 §5); label + seed até 128 bytes */
void tls12_prf(int alg, const u8 *secret, u32 slen, const char *label, const u8 *seed, u32 seedlen,
               u8 *out, u32 olen);
/* preferências do cliente TLS (tls.c), ajustáveis por testes; listas terminadas em 0 */
extern u16 tls_cipher_prefs[4];        /* cifras TLS 1.3 */
extern u16 tls12_cipher_prefs[7];      /* cifras TLS 1.2 (ECDHE + AEAD) */
extern u16 tls_group_prefs[4];         /* 0x001d x25519, 0x0017 secp256r1, 0x0018 secp384r1 */
enum { TLS_V12 = 1, TLS_V13 = 2 };
extern u8 tls_versions;                /* versões oferecidas no ClientHello */

/* Autenticação do servidor TLS (tls.c + x509.c): por padrão (false) a cadeia de
 * certificados é validada contra a lista da Mozilla embutida, o nome do host é
 * conferido e a assinatura do CertificateVerify é verificada; qualquer falha aborta
 * o handshake com texto em tls_error(). true = MODO INSEGURO (aceita qualquer
 * certificado; só para depuração — a UI pode expor isso como opção avançada). */
extern bool g_tls_insecure;

/* ---------- CSPRNG (DRBG ChaCha20 com apagamento de chave) ---------- */
void crypto_rng_add_entropy(const void *data, usize len);
void crypto_random(void *buf, usize len);
