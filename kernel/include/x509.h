/* mOS — verificação de assinaturas de chave pública e certificados X.509
 *   kernel/net/pk.c    aritmética multiprecisão (Montgomery), RSA PKCS#1 v1.5 / PSS,
 *                      ECDSA P-256 / P-384 (só verificação; não precisa de tempo constante)
 *   kernel/net/x509.c  parser ASN.1 DER, X.509, lista de confiança (PEM da Mozilla
 *                      embutido em kernel/arch/certs.S), construção e validação da cadeia
 * Todas as funções devolvem 0 em caso de sucesso. */
#pragma once
#ifdef MOS_HOST_TEST
#include "tls_shim.h"
#else
#include "kernel.h"
#endif
#include "crypto.h"

/* ---------------------------------------------------------------- pk.c */
enum { EC_P256 = 1, EC_P384 = 2 };

/* n/e: inteiros big-endian (zeros à esquerda são ignorados); digest = hash da mensagem */
int rsa_verify_pkcs1(const u8 *n, u32 nlen, const u8 *e, u32 elen, int halg,
                     const u8 *digest, const u8 *sig, u32 siglen);
/* saltlen < 0: aceita qualquer comprimento de sal */
int rsa_verify_pss(const u8 *n, u32 nlen, const u8 *e, u32 elen, int halg,
                   const u8 *digest, const u8 *sig, u32 siglen, int saltlen);
u32 rsa_modulus_bits(const u8 *n, u32 nlen);
/* pub = 04 || X || Y (ponto não comprimido); r, s big-endian */
int ecdsa_verify(int curve, const u8 *pub, u32 publen, const u8 *digest, u32 dlen,
                 const u8 *r, u32 rlen, const u8 *s, u32 slen);
int ec_selftest(int curve);   /* G na curva e n·G = infinito */
/* ECDH (EC_P256 = secp256r1, B = 32; EC_P384 = secp384r1, B = 48):
 * priv = escalar big-endian de B bytes em [1, n-1], pub = 04 || X || Y (1 + 2B bytes).
 * ecdh_shared valida o ponto do par (na curva, coordenadas < p) e devolve o X do
 * segredo compartilhado (B bytes). */
int ecdh_keygen(int curve, u8 *priv, u8 *pub);
int ecdh_shared(int curve, u8 *shared, const u8 *priv, const u8 *peer, u32 peerlen);
int ecdh_p256_keygen(u8 priv[32], u8 pub[65]);
int ecdh_p256(u8 shared[32], const u8 priv[32], const u8 *peer, u32 peerlen);

/* ---------------------------------------------------------------- x509.c */
enum { X509_KEY_NONE = 0, X509_KEY_RSA, X509_KEY_EC };
enum { X509_SIG_NONE = 0, X509_SIG_RSA_PKCS1, X509_SIG_RSA_PSS, X509_SIG_ECDSA };

typedef struct {
    const u8 *raw; u32 raw_len;            /* certificado inteiro (DER) */
    const u8 *tbs; u32 tbs_len;            /* tbsCertificate (com cabeçalho) */
    int sig_kind, sig_hash, pss_salt;      /* algoritmo com que ESTE certificado foi assinado */
    const u8 *sig; u32 sig_len;
    const u8 *issuer; u32 issuer_len;      /* Name (DER completo) */
    const u8 *subject; u32 subject_len;
    const u8 *spki; u32 spki_len;          /* SubjectPublicKeyInfo (DER completo) */
    u64 not_before, not_after;             /* AAAAMMDDhhmmss (UTC) */
    int key_type, curve;
    const u8 *rsa_n; u32 rsa_n_len;
    const u8 *rsa_e; u32 rsa_e_len;
    const u8 *ec_pub; u32 ec_pub_len;
    bool has_bc, ca;
    int pathlen;                           /* -1 = sem limite */
    bool has_ku; u32 ku;                   /* bit i = KeyUsage(i); 0 digitalSignature, 5 keyCertSign */
    bool has_eku, eku_server, eku_ocsp;    /* eku_ocsp: id-kp-OCSPSigning */
    const u8 *serial; u32 serial_len;      /* conteúdo do INTEGER serialNumber */
    bool unknown_critical;
    const u8 *san; u32 san_len;            /* conteúdo de GeneralNames */
} x509_cert_t;

enum {
    X509_OK = 0, X509_E_PARSE, X509_E_UNKNOWN_ISSUER, X509_E_SELF_SIGNED, X509_E_UNTRUSTED_ROOT,
    X509_E_EXPIRED, X509_E_NOT_YET_VALID, X509_E_BAD_SIG, X509_E_UNSUPPORTED, X509_E_NOT_CA,
    X509_E_PATHLEN, X509_E_NAME, X509_E_KEY_USAGE, X509_E_WEAK_KEY, X509_E_CRITICAL_EXT,
    X509_E_TOO_LONG, X509_E_NO_MEM,
};

int  x509_parse(x509_cert_t *c, const u8 *der, u32 len);
void x509_name_cn(const u8 *name, u32 len, char *out, u32 outlen);   /* CN (ou O) legível */
/* assinatura genérica com a chave pública de `key`; msg é hasheada aqui com halg */
int  x509_verify_with_key(const x509_cert_t *key, int kind, int halg, int saltlen,
                          const u8 *msg, u32 mlen, const u8 *sig, u32 siglen);
int  x509_match_host(const x509_cert_t *c, const char *host);       /* 1 = corresponde */

/* lista de confiança: no kernel é carregada sozinha (PEM embutido) no primeiro uso */
int  x509_trust_load_pem(const char *pem, u32 len);                 /* devolve nº de raízes */
u32  x509_trust_count(void);

/* valida leaf + intermediários (qualquer ordem) até uma raiz confiável, datas
 * (now == NULL: pula a checagem de datas) e o nome do host. err recebe texto pt-BR.
 * anchor_cn (opcional) recebe o nome da raiz usada. */
int  x509_verify_chain(const u8 *const *ders, const u32 *lens, u32 count, const char *host,
                       const datetime_t *now, char *err, u32 errlen, char *anchor_cn, u32 cnlen);

/* OCSP grampeado (RFC 6960 / RFC 6066): confere uma OCSPResponse para o certificado
 * ders[0]; o emissor é procurado em ders[1..] (ou na lista de confiança). Aceita
 * assinatura do próprio emissor ou de um responder delegado (emitido por ele, com
 * id-kp-OCSPSigning). now == NULL: não confere datas. msg recebe texto pt-BR.
 *   OCSP_GOOD      status "good" verificado
 *   OCSP_REVOKED   status "revoked" verificado            -> abortar
 *   OCSP_BAD_SIG   assinatura inválida / responder não autorizado -> abortar
 *   OCSP_STALE     resposta vencida ou do futuro           -> abortar
 *   OCSP_UNKNOWN   status "unknown"                        -> soft-fail
 *   OCSP_UNUSABLE  malformada, não é deste certificado, algoritmo não suportado,
 *                  responseStatus != successful             -> soft-fail */
enum { OCSP_GOOD = 0, OCSP_REVOKED, OCSP_BAD_SIG, OCSP_STALE, OCSP_UNKNOWN, OCSP_UNUSABLE };
int  x509_ocsp_check(const u8 *resp, u32 rlen, const u8 *const *ders, const u32 *lens, u32 count,
                     const datetime_t *now, char *msg, u32 msglen);
