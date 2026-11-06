/* mOS — cliente TLS 1.3 (RFC 8446) e TLS 1.2 (RFC 5246) sobre a API TCP
 * não-bloqueante de net.h.
 *
 * O ClientHello oferece as duas versões (supported_versions = 1.3, 1.2;
 * legacy_version = 1.2). Se o ServerHello não trouxer supported_versions
 * (versão 0x0303), roda o handshake TLS 1.2 (ver "TLS 1.2" mais abaixo).
 *
 * TLS 1.3:
 *   - cifras TLS_CHACHA20_POLY1305_SHA256, TLS_AES_128_GCM_SHA256, TLS_AES_256_GCM_SHA384
 *   - troca de chaves x25519, secp256r1 ou secp384r1 (key_share inicial = 1º de tls_group_prefs)
 *   - SNI, ALPN "http/1.1", modo de compatibilidade com middleboxes
 *     (legacy_session_id + ChangeCipherSpec fictício)
 *   - HelloRetryRequest (repete o ClientHello com cookie e/ou com o grupo pedido;
 *     falha com mensagem clara se o servidor exigir um grupo que não temos)
 *   - CertificateRequest (respondemos com Certificate vazio)
 *   - mensagens de handshake fragmentadas em vários registros e registros
 *     fragmentados em várias leituras TCP
 *   - NewSessionTicket (ignorado), KeyUpdate (tratado), alertas
 *
 * Autenticação do servidor (tls_check_certificate / tls_check_signature):
 *   - a cadeia da mensagem Certificate é validada por x509.c até uma raiz da lista
 *     da Mozilla embutida no kernel (assinaturas, datas via rtc_now, CA/keyUsage,
 *     nome do host contra o subjectAltName com regras de curinga);
 *   - a assinatura do CertificateVerify é verificada com a chave do certificado-folha
 *     (rsa_pss_rsae_sha256/384/512, ecdsa_secp256r1_sha256, ecdsa_secp384r1_sha384);
 *   - qualquer falha aborta o handshake com um alerta e texto claro em tls_error().
 *   g_tls_insecure = true desliga as duas verificações (só para depuração).
 *   - revogação por OCSP grampeado (status_request; tls_check_ocsp + x509_ocsp_check):
 *     revoked, assinatura inválida ou resposta vencida abortam; sem grampo, status
 *     unknown ou resposta inutilizável = soft-fail (aceita e registra no log).
 *   Sem CRL, sem consulta OCSP ativa nem Certificate Transparency.
 *
 * TLS 1.2 (só ECDHE — sem RSA estático nem DHE):
 *   - cifras TLS_ECDHE_{ECDSA,RSA}_WITH_{AES_128_GCM_SHA256, AES_256_GCM_SHA384,
 *     CHACHA20_POLY1305_SHA256}; grupos x25519, secp256r1 e secp384r1
 *   - extended_master_secret (RFC 7627; usado sempre que o servidor aceita) e
 *     renegotiation_info (RFC 5746; nunca renegociamos — HelloRequest é ignorado)
 *   - mesma validação de cadeia (x509_verify_chain) do 1.3; a assinatura do
 *     ServerKeyExchange é verificada com a chave do certificado-folha
 *     (rsa_pkcs1_sha256/384/512, rsa_pss_rsae_sha256/384/512, ecdsa_sha256/384)
 *   - PRF do TLS 1.2, Finished nos dois sentidos, nonce explícito do GCM e nonce
 *     XOR número de sequência do ChaCha20 (RFC 7905)
 *   - detecção do sentinela de downgrade do RFC 8446 §4.1.3 ("DOWNGRD") quando
 *     oferecemos 1.3; sem retomada de sessão
 */
#ifdef MOS_HOST_TEST
#include "tls_shim.h"
#else
#include "net.h"
#endif
#include "crypto.h"
#include "x509.h"

/* escape de depuração: true = não autentica o servidor (ver crypto.h) */
bool g_tls_insecure = false;

#define TLS_MAX_PLAIN  16384
#define TLS_MAX_CIPHER (TLS_MAX_PLAIN + 256)
#define TLS_RBUF       (5 + TLS_MAX_CIPHER)
#define TLS_OBUF       (96 * 1024)
#define TLS_HS_MAX     (256 * 1024)          /* maior mensagem de handshake aceita */

enum { CS_AES128 = 0x1301, CS_AES256 = 0x1302, CS_CHACHA = 0x1303 };
/* TLS 1.2: só ECDHE com AEAD */
enum { CS12_ECDSA_AES128 = 0xC02B, CS12_ECDSA_AES256 = 0xC02C, CS12_RSA_AES128 = 0xC02F,
       CS12_RSA_AES256 = 0xC030, CS12_RSA_CHACHA = 0xCCA8, CS12_ECDSA_CHACHA = 0xCCA9 };
enum { GRP_P256 = 0x0017, GRP_P384 = 0x0018, GRP_X25519 = 0x001d };
enum { CT_CCS = 20, CT_ALERT = 21, CT_HANDSHAKE = 22, CT_APP = 23 };
enum { HT_HELLO_REQUEST = 0, HT_CLIENT_HELLO = 1, HT_SERVER_HELLO = 2, HT_NEW_SESSION_TICKET = 4,
       HT_ENCRYPTED_EXT = 8, HT_CERTIFICATE = 11, HT_SERVER_KEY_EXCHANGE = 12, HT_CERT_REQUEST = 13,
       HT_SERVER_HELLO_DONE = 14, HT_CERT_VERIFY = 15, HT_CLIENT_KEY_EXCHANGE = 16, HT_FINISHED = 20,
       HT_CERT_STATUS = 22,
       HT_KEY_UPDATE = 24, HT_MESSAGE_HASH = 254 };
enum { AL_CLOSE_NOTIFY = 0, AL_UNEXPECTED = 10, AL_BAD_MAC = 20, AL_OVERFLOW = 22,
       AL_HS_FAILURE = 40, AL_BAD_CERT = 42, AL_UNSUP_CERT = 43, AL_CERT_EXPIRED = 45,
       AL_UNKNOWN_CA = 48, AL_ILLEGAL = 47, AL_DECODE = 50, AL_DECRYPT = 51,
       AL_PROTO_VERSION = 70, AL_INTERNAL = 80, AL_USER_CANCELED = 90, AL_MISSING_EXT = 109,
       AL_CERT_REVOKED = 44, AL_UNRECOGNIZED_NAME = 112, AL_BAD_CERT_STATUS = 113 };

/* HS_DONE e HS_ERROR precisam ficar por último (comparações "hs < HS_DONE") */
enum { HS_START, HS_WAIT_SH, HS_WAIT_EE, HS_WAIT_CERT_CR, HS_WAIT_CERT, HS_WAIT_CV, HS_WAIT_FIN,
       HS12_WAIT_CERT, HS12_WAIT_SKE, HS12_WAIT_SHD, HS12_WAIT_CCS, HS12_WAIT_FIN,
       HS_DONE, HS_ERROR };

/* ordem de preferência das cifras oferecidas (0 termina a lista).
 * ChaCha20 primeiro: nosso AES é por software e não é resistente a cache-timing.
 * Exportadas para que testes possam forçar uma cifra específica. */
u16 tls_cipher_prefs[4] = { CS_CHACHA, CS_AES128, CS_AES256, 0 };
u16 tls12_cipher_prefs[7] = { CS12_ECDSA_CHACHA, CS12_RSA_CHACHA, CS12_ECDSA_AES128, CS12_RSA_AES128,
                              CS12_ECDSA_AES256, CS12_RSA_AES256, 0 };
/* grupos (supported_groups); o 1º também é o key_share inicial do TLS 1.3 */
u16 tls_group_prefs[4] = { GRP_X25519, GRP_P256, GRP_P384, 0 };
/* versões oferecidas (TLS_V12 | TLS_V13) */
u8 tls_versions = TLS_V12 | TLS_V13;

static const u8 DOWNGRADE_12[8] = { 'D', 'O', 'W', 'N', 'G', 'R', 'D', 1 };
static const u8 DOWNGRADE_11[8] = { 'D', 'O', 'W', 'N', 'G', 'R', 'D', 0 };

static const u8 HRR_RANDOM[32] = {
    0xCF, 0x21, 0xAD, 0x74, 0xE5, 0x9A, 0x61, 0x11, 0xBE, 0x1D, 0x8C, 0x02, 0x1E, 0x65, 0xB8, 0x91,
    0xC2, 0xA2, 0x11, 0x16, 0x7A, 0xBB, 0x8C, 0x5E, 0x07, 0x9E, 0x09, 0xE2, 0xC8, 0xA8, 0x33, 0x9C,
};

typedef struct {
    bool on;
    bool v12;             /* formato de registro do TLS 1.2 (tipo externo real, AAD com seq) */
    u16 suite;            /* AEAD: CS_AES128 / CS_AES256 / CS_CHACHA (também no 1.2) */
    u8 key[32], iv[12];   /* 1.2 + GCM: iv = sal de 4 bytes seguido de zeros */
    u64 seq;
    aes_gcm_ctx gcm;
} tls_aead_t;

struct tls_conn {
    int sock;
    int hs;
    char sni[256];
    char err[192];
    bool closed;          /* close_notify recebido ou TCP encerrado */
    bool sent_ccs, hrr, cert_req;
    bool v12, ems;        /* TLS 1.2 negociado; extended_master_secret em uso */
    u16 ks_group;         /* grupo do key_share (1.3) ou do ServerKeyExchange (1.2) */
    u16 ec_group;                     /* grupo do par efêmero NIST abaixo (0 = nenhum) */
    u8 epriv[48], epub[97];           /* par efêmero P-256/P-384 (gerado sob demanda) */
    u8 srandom[32];                   /* ServerHello.random (1.2) */
    u8 pms[48]; u32 pms_len;          /* segredo ECDHE do 1.2 até o ClientKeyExchange */
    tls_aead_t rd_next;               /* chaves de leitura do 1.2, ativadas no CCS do servidor */

    u8 *rbuf; u32 rlen;               /* registros brutos vindos do TCP */
    u8 *hbuf; u32 hlen, hcap;         /* remontagem de mensagens de handshake */
    u8 *obuf; u32 olen, ooff;         /* saída pendente para o TCP */
    u8 *abuf; u32 alen, aoff;         /* dados de aplicação já decifrados */

    u8 priv[32], pub[32], random[32], sid[32];
    u8 *ch; u32 chlen;                /* ClientHello #1 (início do transcript) */
    u8 *cookie; u32 cookielen;        /* cookie do HelloRetryRequest */
    u8 crctx[255]; u32 crctx_len;     /* certificate_request_context */
    u8 *leaf; u32 leaf_len;           /* cópia do certificado-folha (para o CertificateVerify) */
    u8 *chain; u32 chain_n;           /* cópia da cadeia (para conferir o OCSP grampeado) */
    u32 chain_off[10], chain_len[10];
    bool ocsp_req, ocsp_done;         /* 1.2: servidor ecoou status_request / CertificateStatus visto */

    hash_ctx tr;
    int halg; u32 hl;
    u16 suite;
    u8 master[48], c_hs[48], s_hs[48], c_ap[48], s_ap[48];
    tls_aead_t rd, wr;
};

/* ======================================================================
 * escalonamento de chaves do TLS 1.3 (exportado para os testes RFC 8448)
 * ====================================================================== */
void tls13_derive_handshake(int alg, const u8 *shared, u32 slen, const u8 *th_ch_sh,
                            u8 *hs_secret, u8 *c_hs, u8 *s_hs, u8 *master) {
    u32 hl = hash_len(alg);
    u8 zeros[HASH_MAX_LEN] = {0}, early[HASH_MAX_LEN], empty[HASH_MAX_LEN], derived[HASH_MAX_LEN];
    u8 hs[HASH_MAX_LEN];
    hkdf_extract(alg, NULL, 0, zeros, hl, early);
    hash_buf(alg, "", 0, empty);
    hkdf_expand_label(alg, early, "derived", empty, hl, derived, hl);
    hkdf_extract(alg, derived, hl, shared, slen, hs);
    hkdf_expand_label(alg, hs, "c hs traffic", th_ch_sh, hl, c_hs, hl);
    hkdf_expand_label(alg, hs, "s hs traffic", th_ch_sh, hl, s_hs, hl);
    hkdf_expand_label(alg, hs, "derived", empty, hl, derived, hl);
    hkdf_extract(alg, derived, hl, zeros, hl, master);
    if (hs_secret) memcpy(hs_secret, hs, hl);
    secure_zero(hs, sizeof(hs));
    secure_zero(early, sizeof(early));
    secure_zero(derived, sizeof(derived));
}

void tls13_derive_app(int alg, const u8 *master, const u8 *th_through_sfin, u8 *c_ap, u8 *s_ap) {
    u32 hl = hash_len(alg);
    hkdf_expand_label(alg, master, "c ap traffic", th_through_sfin, hl, c_ap, hl);
    hkdf_expand_label(alg, master, "s ap traffic", th_through_sfin, hl, s_ap, hl);
}

void tls13_traffic_keys(int alg, const u8 *secret, u32 keylen, u8 *key, u8 *iv) {
    hkdf_expand_label(alg, secret, "key", NULL, 0, key, keylen);
    hkdf_expand_label(alg, secret, "iv", NULL, 0, iv, 12);
}

void tls13_finished(int alg, const u8 *base_secret, const u8 *th, u8 *out) {
    u32 hl = hash_len(alg);
    u8 fk[HASH_MAX_LEN];
    hkdf_expand_label(alg, base_secret, "finished", NULL, 0, fk, hl);
    hmac(alg, fk, hl, th, hl, out);
    secure_zero(fk, sizeof(fk));
}

/* PRF do TLS 1.2 (RFC 5246 §5): P_hash(secret, label || seed) com HMAC-alg.
 * label + seed cabem com folga em 128 bytes em todos os usos do protocolo. */
void tls12_prf(int alg, const u8 *secret, u32 slen, const char *label, const u8 *seed, u32 seedlen,
               u8 *out, u32 olen) {
    u32 hl = hash_len(alg), ll = (u32)strlen(label);
    u8 buf[HASH_MAX_LEN + 128], a[HASH_MAX_LEN], t[HASH_MAX_LEN];
    if (ll + seedlen > 128) { memset(out, 0, olen); return; }
    u8 *ls = buf + hl;                       /* buf = A(i) || label || seed */
    memcpy(ls, label, ll);
    memcpy(ls + ll, seed, seedlen);
    u32 lsl = ll + seedlen;
    hmac(alg, secret, slen, ls, lsl, a);     /* A(1) */
    while (olen) {
        memcpy(buf, a, hl);
        hmac(alg, secret, slen, buf, hl + lsl, t);
        u32 k = olen < hl ? olen : hl;
        memcpy(out, t, k);
        out += k;
        olen -= k;
        hmac(alg, secret, slen, a, hl, t);   /* A(i+1) */
        memcpy(a, t, hl);
    }
    secure_zero(a, sizeof(a));
    secure_zero(t, sizeof(t));
    secure_zero(buf, sizeof(buf));
}

/* ganchos de autenticação (implementados mais abaixo, junto de handle_certificate) */
static int tls_check_certificate(tls_conn_t *c, const u8 *const *ders, const u32 *lens, u32 count);
static int tls_check_signature(tls_conn_t *c, u16 sigalg, const u8 *sig, u32 siglen,
                               const u8 *transcript_hash);

/* ======================================================================
 * utilidades
 * ====================================================================== */
static inline u32 be16(const u8 *p) { return (u32)p[0] << 8 | p[1]; }
static inline u32 be24(const u8 *p) { return (u32)p[0] << 16 | (u32)p[1] << 8 | p[2]; }
static inline void put16(u8 *p, u32 v) { p[0] = (u8)(v >> 8); p[1] = (u8)v; }
static inline void put24(u8 *p, u32 v) { p[0] = (u8)(v >> 16); p[1] = (u8)(v >> 8); p[2] = (u8)v; }

typedef struct { const u8 *p; u32 n; bool bad; } rd_t;
static u32 rd_u8(rd_t *r) { if (r->n < 1) { r->bad = true; return 0; } r->n--; return *r->p++; }
static u32 rd_u16(rd_t *r) { if (r->n < 2) { r->bad = true; r->n = 0; return 0; } u32 v = be16(r->p); r->p += 2; r->n -= 2; return v; }
static u32 rd_u24(rd_t *r) { if (r->n < 3) { r->bad = true; r->n = 0; return 0; } u32 v = be24(r->p); r->p += 3; r->n -= 3; return v; }
static const u8 *rd_bytes(rd_t *r, u32 n) {
    if (r->n < n) { r->bad = true; r->n = 0; return NULL; }
    const u8 *p = r->p; r->p += n; r->n -= n; return p;
}

static const char *alert_text(u32 d) {
    switch (d) {
    case 0:   return "conexão encerrada (close_notify)";
    case 10:  return "mensagem inesperada";
    case 20:  return "MAC de registro inválido";
    case 22:  return "registro grande demais";
    case 40:  return "falha na negociação (handshake_failure)";
    case 42:  return "certificado inválido";
    case 43:  return "certificado não suportado";
    case 44:  return "certificado revogado";
    case 45:  return "certificado expirado";
    case 46:  return "certificado desconhecido";
    case 47:  return "parâmetro ilegal";
    case 48:  return "autoridade certificadora desconhecida";
    case 49:  return "acesso negado";
    case 50:  return "erro de decodificação";
    case 51:  return "erro de decifragem/verificação";
    case 70:  return "versão de protocolo não suportada";
    case 71:  return "segurança insuficiente";
    case 80:  return "erro interno do servidor";
    case 86:  return "fallback inadequado";
    case 90:  return "cancelado pelo usuário";
    case 109: return "extensão obrigatória ausente";
    case 110: return "extensão não suportada";
    case 112: return "nome do servidor (SNI) não reconhecido";
    case 113: return "resposta de status de certificado inválida";
    case 115: return "identidade PSK desconhecida";
    case 116: return "certificado do cliente exigido";
    case 120: return "nenhum protocolo de aplicação em comum (ALPN)";
    default:  return "alerta desconhecido";
    }
}

static void aead_set(tls_aead_t *a, int alg, u16 suite, const u8 *secret) {
    u32 kl = suite == CS_AES128 ? 16 : 32;
    a->suite = suite;
    tls13_traffic_keys(alg, secret, kl, a->key, a->iv);
    if (suite != CS_CHACHA) aes_gcm_init(&a->gcm, a->key, kl);
    a->seq = 0;
    a->on = true;
}

/* nonce = iv XOR seq (1.3 e ChaCha20 no 1.2). No 1.2 com GCM o iv é sal||zeros,
 * então o resultado é sal || seq — o nonce explícito que enviamos é o próprio seq. */
static void aead_nonce(const tls_aead_t *a, u8 n[12]) {
    memcpy(n, a->iv, 12);
    for (int i = 0; i < 8; i++) n[11 - i] ^= (u8)(a->seq >> (8 * i));
}

/* chaves do TLS 1.2 (do key_block): aead = CS_AES128/CS_AES256/CS_CHACHA */
static void aead_set12(tls_aead_t *a, u16 aead, const u8 *key, const u8 *iv) {
    u32 kl = aead == CS_AES128 ? 16 : 32;
    a->suite = aead;
    memcpy(a->key, key, kl);
    memset(a->iv, 0, 12);
    memcpy(a->iv, iv, aead == CS_CHACHA ? 12 : 4);
    if (aead != CS_CHACHA) aes_gcm_init(&a->gcm, a->key, kl);
    a->seq = 0;
    a->v12 = true;
    a->on = true;
}

/* AAD do TLS 1.2: seq_num || tipo || versão || comprimento do texto claro */
static void aad12(u8 aad[13], u64 seq, u8 type, const u8 ver[2], u32 len) {
    for (int i = 0; i < 8; i++) aad[i] = (u8)(seq >> (56 - 8 * i));
    aad[8] = type;
    aad[9] = ver[0];
    aad[10] = ver[1];
    aad[11] = (u8)(len >> 8);
    aad[12] = (u8)len;
}

/* bytes extras por registro cifrado (cabeçalho incluso) com as chaves de escrita atuais */
static u32 rec_overhead(const tls_aead_t *w) {
    if (!w->on) return 5;
    if (!w->v12) return 5 + 1 + 16;                       /* tipo interno + tag */
    return 5 + 16 + (w->suite == CS_CHACHA ? 0 : 8);      /* tag + nonce explícito (GCM) */
}

/* TLS 1.2: propriedades das cifras */
static bool suite12_ecdsa(u32 s) { return s == CS12_ECDSA_AES128 || s == CS12_ECDSA_AES256 || s == CS12_ECDSA_CHACHA; }
static int suite12_hash(u32 s) { return s == CS12_ECDSA_AES256 || s == CS12_RSA_AES256 ? HASH_SHA384 : HASH_SHA256; }
static u16 suite12_aead(u32 s) {
    return s == CS12_ECDSA_CHACHA || s == CS12_RSA_CHACHA ? CS_CHACHA
         : s == CS12_ECDSA_AES256 || s == CS12_RSA_AES256 ? CS_AES256 : CS_AES128;
}

static const char *suite_text(u32 s) {
    switch (s) {
    case CS_AES128: return "TLS_AES_128_GCM_SHA256";
    case CS_AES256: return "TLS_AES_256_GCM_SHA384";
    case CS_CHACHA: return "TLS_CHACHA20_POLY1305_SHA256";
    case CS12_ECDSA_AES128: return "ECDHE_ECDSA_WITH_AES_128_GCM_SHA256";
    case CS12_ECDSA_AES256: return "ECDHE_ECDSA_WITH_AES_256_GCM_SHA384";
    case CS12_RSA_AES128: return "ECDHE_RSA_WITH_AES_128_GCM_SHA256";
    case CS12_RSA_AES256: return "ECDHE_RSA_WITH_AES_256_GCM_SHA384";
    case CS12_RSA_CHACHA: return "ECDHE_RSA_WITH_CHACHA20_POLY1305_SHA256";
    case CS12_ECDSA_CHACHA: return "ECDHE_ECDSA_WITH_CHACHA20_POLY1305_SHA256";
    default: return "?";
    }
}

static const char *group_text(u32 g) {
    return g == GRP_X25519 ? "x25519" : g == GRP_P256 ? "secp256r1" : g == GRP_P384 ? "secp384r1" : "?";
}
/* tamanho da chave pública (no fio) e do segredo compartilhado de cada grupo */
static u32 group_keylen(u32 g) { return g == GRP_P384 ? 97 : g == GRP_P256 ? 65 : 32; }
static u32 group_seclen(u32 g) { return g == GRP_P384 ? 48 : 32; }

static bool group_offered(u32 g) {
    for (int i = 0; i < 4 && tls_group_prefs[i]; i++)
        if (tls_group_prefs[i] == g && (g == GRP_X25519 || g == GRP_P256 || g == GRP_P384)) return true;
    return false;
}

/* par efêmero para o grupo NIST g (x25519 já vem pronto de tls_new) */
static int ensure_ec(tls_conn_t *c, u32 g) {
    if (g == GRP_X25519 || c->ec_group == g) return 0;
    if (ecdh_keygen(g == GRP_P384 ? EC_P384 : EC_P256, c->epriv, c->epub) != 0) return -1;
    c->ec_group = (u16)g;
    return 0;
}

/* segredo ECDHE com o grupo g; 0 = ok */
static int ecdhe_shared(tls_conn_t *c, u32 g, const u8 *peer, u32 plen, u8 out[32]) {
    if (g == GRP_X25519) {
        if (plen != 32) return -1;
        x25519(out, c->priv, peer);
        u8 z = 0;
        for (int i = 0; i < 32; i++) z |= out[i];
        return z ? 0 : -1;
    }
    if (g == GRP_P256 || g == GRP_P384) {
        if (c->ec_group != g) return -1;
        return ecdh_shared(g == GRP_P384 ? EC_P384 : EC_P256, out, c->epriv, peer, plen);
    }
    return -1;
}

/* ======================================================================
 * saída
 * ====================================================================== */
static int flush_out(tls_conn_t *c) {
    while (c->ooff < c->olen) {
        int n = tcp_send(c->sock, c->obuf + c->ooff, (int)(c->olen - c->ooff));
        if (n < 0) return -1;
        if (n == 0) break;
        c->ooff += (u32)n;
    }
    if (c->ooff == c->olen) c->ooff = c->olen = 0;
    return 0;
}

static bool out_space(tls_conn_t *c, u32 need) {
    if (c->ooff) {
        memmove(c->obuf, c->obuf + c->ooff, c->olen - c->ooff);
        c->olen -= c->ooff;
        c->ooff = 0;
    }
    return c->olen + need <= TLS_OBUF;
}

/* enfileira um registro (cifrado se as chaves de escrita estiverem ativas) */
static int queue_record(tls_conn_t *c, u8 type, const u8 *data, u32 len, u16 legacy_ver) {
    if (len > TLS_MAX_PLAIN) return -1;
    bool enc = c->wr.on && type != CT_CCS;
    u32 need = len + (enc ? rec_overhead(&c->wr) : 5);
    if (!out_space(c, need)) return -1;
    u8 *h = c->obuf + c->olen;
    if (!enc) {
        h[0] = type;
        put16(h + 1, legacy_ver);
        put16(h + 3, len);
        memcpy(h + 5, data, len);
    } else if (c->wr.v12) {
        /* TLS 1.2: tipo real no cabeçalho; GCM leva o nonce explícito (= seq) antes do texto */
        u32 ex = c->wr.suite == CS_CHACHA ? 0 : 8;
        h[0] = type;
        put16(h + 1, 0x0303);
        put16(h + 3, ex + len + 16);
        u8 *body = h + 5 + ex;
        memmove(body, data, len);
        u8 nonce[12], aad[13];
        aead_nonce(&c->wr, nonce);
        if (ex) memcpy(h + 5, nonce + 4, 8);
        aad12(aad, c->wr.seq, type, h + 1, len);
        if (c->wr.suite == CS_CHACHA)
            chacha20poly1305_seal(c->wr.key, nonce, aad, 13, body, len, body, body + len);
        else
            aes_gcm_seal(&c->wr.gcm, nonce, aad, 13, body, len, body, body + len);
        c->wr.seq++;
    } else {
        u32 ilen = len + 1;
        h[0] = CT_APP;
        put16(h + 1, 0x0303);
        put16(h + 3, ilen + 16);
        u8 *body = h + 5;
        memmove(body, data, len);
        body[len] = type;
        u8 nonce[12];
        aead_nonce(&c->wr, nonce);
        if (c->wr.suite == CS_CHACHA)
            chacha20poly1305_seal(c->wr.key, nonce, h, 5, body, ilen, body, body + ilen);
        else
            aes_gcm_seal(&c->wr.gcm, nonce, h, 5, body, ilen, body, body + ilen);
        c->wr.seq++;
    }
    c->olen += need;
    return 0;
}

static int fail(tls_conn_t *c, int alert, const char *fmt, ...) {
    if (c->hs == HS_ERROR) return -1;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(c->err, sizeof(c->err), fmt, ap);
    va_end(ap);
    if (alert >= 0) {
        u8 a[2] = {2, (u8)alert};
        if (queue_record(c, CT_ALERT, a, 2, 0x0303) == 0) flush_out(c);
    }
    c->hs = HS_ERROR;
    return -1;
}

static int queue_handshake(tls_conn_t *c, const u8 *msg, u32 len, bool transcript) {
    if (transcript) hash_update(&c->tr, msg, len);
    while (len) {
        u32 k = len > TLS_MAX_PLAIN ? TLS_MAX_PLAIN : len;
        if (queue_record(c, CT_HANDSHAKE, msg, k, 0x0303) < 0)
            return fail(c, AL_INTERNAL, "buffer de saída TLS cheio");
        msg += k; len -= k;
    }
    return 0;
}

static void send_ccs(tls_conn_t *c) {
    if (c->sent_ccs) return;
    u8 one = 1;
    queue_record(c, CT_CCS, &one, 1, 0x0303);
    c->sent_ccs = true;
}

/* ======================================================================
 * ClientHello
 * ====================================================================== */
static bool is_ip_literal(const char *s) {
    if (!*s) return true;
    for (; *s; s++)
        if (!((*s >= '0' && *s <= '9') || *s == '.' || *s == ':')) return false;
    return true;
}

static u8 *build_client_hello(tls_conn_t *c, u32 *outlen) {
    u32 snilen = (u32)strlen(c->sni);
    bool v13 = tls_versions & TLS_V13, v12 = tls_versions & TLS_V12;
    u8 *m = kmalloc(800 + snilen + c->cookielen);
    if (!m) return NULL;
    u8 *p = m;
    *p++ = HT_CLIENT_HELLO;
    p += 3;                               /* comprimento, preenchido no fim */
    *p++ = 3; *p++ = 3;                   /* legacy_version = TLS 1.2 */
    memcpy(p, c->random, 32); p += 32;
    *p++ = 32;                            /* legacy_session_id (compat. middlebox) */
    memcpy(p, c->sid, 32); p += 32;
    u8 *cs = p; p += 2;
    if (v13)
        for (int i = 0; i < 4 && tls_cipher_prefs[i]; i++) { put16(p, tls_cipher_prefs[i]); p += 2; }
    if (v12)
        for (int i = 0; i < 7 && tls12_cipher_prefs[i]; i++) { put16(p, tls12_cipher_prefs[i]); p += 2; }
    put16(cs, (u32)(p - cs - 2));
    *p++ = 1; *p++ = 0;                   /* compressão: null */
    u8 *exts = p; p += 2;

    if (!is_ip_literal(c->sni)) {         /* server_name */
        put16(p, 0); put16(p + 2, snilen + 5); put16(p + 4, snilen + 3);
        p[6] = 0; put16(p + 7, snilen);
        memcpy(p + 9, c->sni, snilen);
        p += 9 + snilen;
    }
    /* status_request (RFC 6066): pede a resposta OCSP grampeada — no 1.3 ela vem na
     * extensão do CertificateEntry da folha, no 1.2 na mensagem CertificateStatus */
    static const u8 sr[] = {0x00, 0x05, 0x00, 0x05, 0x01, 0x00, 0x00, 0x00, 0x00};
    memcpy(p, sr, sizeof(sr)); p += sizeof(sr);
    /* supported_groups (x25519, secp256r1, secp384r1 — ver tls_group_prefs) */
    u8 *gl = p;
    put16(p, 10); p += 6;
    for (int i = 0; i < 4 && tls_group_prefs[i]; i++)
        if (group_offered(tls_group_prefs[i])) { put16(p, tls_group_prefs[i]); p += 2; }
    put16(gl + 2, (u32)(p - gl - 4));
    put16(gl + 4, (u32)(p - gl - 6));
    if (v12) {
        /* ec_point_formats: só não comprimido (RFC 8422) */
        static const u8 pf[] = {0x00, 0x0b, 0x00, 0x02, 0x01, 0x00};
        memcpy(p, pf, sizeof(pf)); p += sizeof(pf);
        /* extended_master_secret (RFC 7627) e renegotiation_info vazio (RFC 5746) */
        static const u8 ems[] = {0x00, 0x17, 0x00, 0x00, 0xff, 0x01, 0x00, 0x01, 0x00};
        memcpy(p, ems, sizeof(ems)); p += sizeof(ems);
    }
    /* signature_algorithms: só o que sabemos verificar. CertificateVerify (1.3):
     * ecdsa_secp256r1_sha256, ecdsa_secp384r1_sha384, rsa_pss_rsae_sha256/384/512.
     * rsa_pkcs1_sha256/384/512 valem no 1.3 apenas para assinaturas DENTRO dos
     * certificados (RFC 8446 §4.2.3) — as cadeias RSA da Web quase todas usam
     * PKCS#1 v1.5 — e no 1.2 também para o ServerKeyExchange. */
    static const u16 sigs[] = {
        0x0403, 0x0804, 0x0503, 0x0805, 0x0806, 0x0401, 0x0501, 0x0601,
    };
    put16(p, 13); put16(p + 2, 2 + 2 * ARRAY_LEN(sigs)); put16(p + 4, 2 * ARRAY_LEN(sigs));
    p += 6;
    for (u32 i = 0; i < ARRAY_LEN(sigs); i++) { put16(p, sigs[i]); p += 2; }
    if (v13) {
        /* supported_versions: TLS 1.3 (e 1.2, se oferecido) */
        put16(p, 43); put16(p + 2, v12 ? 5 : 3); p[4] = v12 ? 4 : 2;
        put16(p + 5, 0x0304);
        if (v12) put16(p + 7, 0x0303);
        p += v12 ? 9 : 7;
        /* key_share: um único grupo */
        u32 kl = group_keylen(c->ks_group);
        put16(p, 51); put16(p + 2, kl + 6); put16(p + 4, kl + 4); put16(p + 6, c->ks_group); put16(p + 8, kl);
        memcpy(p + 10, c->ks_group == GRP_X25519 ? c->pub : c->epub, kl);
        p += 10 + kl;
    }
    /* ALPN: http/1.1 */
    static const u8 alpn[] = {0x00, 0x10, 0x00, 0x0b, 0x00, 0x09, 0x08,
                              'h', 't', 't', 'p', '/', '1', '.', '1'};
    memcpy(p, alpn, sizeof(alpn)); p += sizeof(alpn);
    /* cookie (só após HelloRetryRequest) */
    if (c->cookie) {
        put16(p, 44); put16(p + 2, c->cookielen + 2); put16(p + 4, c->cookielen);
        memcpy(p + 6, c->cookie, c->cookielen);
        p += 6 + c->cookielen;
    }
    put16(exts, (u32)(p - exts - 2));
    u32 len = (u32)(p - m);
    put24(m + 1, len - 4);
    *outlen = len;
    return m;
}

static int send_client_hello(tls_conn_t *c, bool first) {
    u32 len;
    if ((tls_versions & TLS_V13) && ensure_ec(c, c->ks_group) < 0)
        return fail(c, -1, "falha ao gerar a chave efêmera %s", group_text(c->ks_group));
    u8 *m = build_client_hello(c, &len);
    if (!m) return fail(c, -1, "sem memória para o ClientHello");
    if (first) {
        c->ch = m;
        c->chlen = len;
        if (queue_record(c, CT_HANDSHAKE, m, len, 0x0301) < 0) return fail(c, -1, "buffer de saída TLS cheio");
        return 0;
    }
    hash_update(&c->tr, m, len);
    int r = queue_record(c, CT_HANDSHAKE, m, len, 0x0303);
    kfree(m);
    return r < 0 ? fail(c, -1, "buffer de saída TLS cheio") : 0;
}

/* ======================================================================
 * mensagens de handshake do servidor
 * ====================================================================== */
static bool suite_offered(u32 s) {
    if (!(tls_versions & TLS_V13)) return false;
    for (int i = 0; i < 4 && tls_cipher_prefs[i]; i++)
        if (tls_cipher_prefs[i] == s) return true;
    return false;
}

static bool suite12_offered(u32 s) {
    if (!(tls_versions & TLS_V12)) return false;
    for (int i = 0; i < 7 && tls12_cipher_prefs[i]; i++)
        if (tls12_cipher_prefs[i] == s) return true;
    return false;
}

/* ServerHello que escolheu TLS 1.2 */
static int handle_server_hello12(tls_conn_t *c, const u8 *msg, u32 mlen, const u8 *rnd, const u8 *sid,
                                 u32 sidl, u32 suite, u32 comp, bool ems, int reneg) {
    /* RFC 8446 §4.1.3: um servidor TLS 1.3 que responde 1.2 marca o random;
     * como oferecemos 1.3, isso só acontece se alguém removeu o 1.3 do caminho */
    if ((tls_versions & TLS_V13) && (!memcmp(rnd + 24, DOWNGRADE_12, 8) || !memcmp(rnd + 24, DOWNGRADE_11, 8))) {
        kprintf("[tls] %s: sentinela de downgrade no ServerHello.random — conexão abortada\n", c->sni);
        return fail(c, AL_ILLEGAL, "ataque de downgrade detectado: o servidor suporta TLS 1.3, mas a resposta veio em TLS 1.2");
    }
    if (!suite12_offered(suite))
        return fail(c, AL_ILLEGAL, "o servidor escolheu uma cifra não oferecida para TLS 1.2 (0x%04x)", suite);
    if (comp != 0) return fail(c, AL_ILLEGAL, "o servidor escolheu compressão");
    if (sidl == 32 && !memcmp(sid, c->sid, 32))
        return fail(c, AL_ILLEGAL, "o servidor tentou retomar uma sessão que não existe");
    if (reneg < 0) return fail(c, AL_HS_FAILURE, "renegotiation_info do servidor inválido");
    if (reneg == 0)
        kprintf("[tls] %s: AVISO: servidor sem renegotiation_info (RFC 5746) — renegociação nunca é aceita\n", c->sni);
    c->v12 = true;
    c->ems = ems;
    memcpy(c->srandom, rnd, 32);
    c->suite = (u16)suite;
    c->halg = suite12_hash(suite);
    c->hl = hash_len(c->halg);
    hash_init(&c->tr, c->halg);
    hash_update(&c->tr, c->ch, c->chlen);
    hash_update(&c->tr, msg, mlen);
    if (!ems)
        kprintf("[tls] %s: AVISO: servidor sem extended_master_secret (RFC 7627); usando o master secret clássico\n",
                c->sni);
    kprintf("[tls] %s: TLS 1.2 negociado (%s%s)\n", c->sni, suite_text(suite), ems ? ", extended_master_secret" : "");
    c->hs = HS12_WAIT_CERT;
    return 0;
}

static int handle_server_hello(tls_conn_t *c, const u8 *msg, u32 mlen) {
    rd_t r = {msg + 4, mlen - 4, false};
    u32 ver = rd_u16(&r);
    const u8 *rnd = rd_bytes(&r, 32);
    u32 sidl = rd_u8(&r);
    const u8 *sid = rd_bytes(&r, sidl);
    u32 suite = rd_u16(&r);
    u32 comp = rd_u8(&r);
    if (r.bad) return fail(c, AL_DECODE, "ServerHello malformado");
    if (ver < 0x0303 && (ver >> 8) == 3)
        return fail(c, AL_PROTO_VERSION, "o servidor só suporta versões antigas de TLS (anteriores à 1.2)");
    if (ver != 0x0303) return fail(c, AL_PROTO_VERSION, "ServerHello com versão inesperada");
    bool is_hrr = memcmp(rnd, HRR_RANDOM, 32) == 0;

    u32 sv = 0, group = 0, klen = 0, cklen = 0;
    const u8 *key = NULL, *ck = NULL;
    bool have_ks = false, ems = false;
    int reneg = 0;                         /* 0 ausente, 1 ok (vazio), -1 inválido */
    if (r.n) {
        u32 el = rd_u16(&r);
        rd_t e = {rd_bytes(&r, el), el, r.bad};
        if (r.bad) return fail(c, AL_DECODE, "extensões do ServerHello malformadas");
        while (e.n && !e.bad) {
            u32 t = rd_u16(&e), l = rd_u16(&e);
            rd_t x = {rd_bytes(&e, l), l, e.bad};
            if (e.bad) break;
            if (t == 43) sv = rd_u16(&x);
            else if (t == 51) {
                have_ks = true;
                group = rd_u16(&x);
                if (!is_hrr) { klen = rd_u16(&x); key = rd_bytes(&x, klen); }
            } else if (t == 44 && is_hrr) {
                cklen = rd_u16(&x); ck = rd_bytes(&x, cklen);
            } else if (t == 23) {
                if (l != 0) x.bad = true;
                ems = true;
            } else if (t == 5) {
                if (l != 0) x.bad = true;     /* 1.2: o servidor vai mandar CertificateStatus */
                c->ocsp_req = true;
            } else if (t == 0xff01) {
                reneg = l == 1 && x.p[0] == 0 ? 1 : -1;
            }
            if (x.bad) e.bad = true;
        }
        if (e.bad) return fail(c, AL_DECODE, "extensões do ServerHello malformadas");
    }
    if (sv == 0 || sv == 0x0303) {
        if (!(tls_versions & TLS_V12))
            return fail(c, AL_PROTO_VERSION, "o servidor não suporta TLS 1.3 (só versões antigas)");
        if (is_hrr || c->hrr) return fail(c, AL_ILLEGAL, "o servidor escolheu TLS 1.2 depois de um HelloRetryRequest");
        return handle_server_hello12(c, msg, mlen, rnd, sid, sidl, suite, comp, ems, reneg);
    }
    if (sv != 0x0304 || !(tls_versions & TLS_V13))
        return fail(c, AL_ILLEGAL, "o servidor escolheu uma versão de TLS inválida");
    if (sidl != 32 || memcmp(sid, c->sid, 32))
        return fail(c, AL_ILLEGAL, "o servidor não ecoou o legacy_session_id");
    if (!suite_offered(suite)) return fail(c, AL_ILLEGAL, "o servidor escolheu uma cifra não oferecida (0x%04x)", suite);
    if (comp != 0) return fail(c, AL_ILLEGAL, "o servidor escolheu compressão");

    if (is_hrr) {
        if (c->hrr) return fail(c, AL_UNEXPECTED, "segundo HelloRetryRequest recebido");
        c->hrr = true;
        if (have_ks && group == c->ks_group)
            return fail(c, AL_ILLEGAL, "HelloRetryRequest pediu %s, que já foi enviado", group_text(group));
        if (have_ks && !group_offered(group))
            return fail(c, AL_HS_FAILURE,
                        "o servidor exige um grupo de troca de chaves não suportado (0x%04x); só temos x25519, secp256r1 e secp384r1",
                        group);
        if (!ck && !have_ks) return fail(c, AL_ILLEGAL, "HelloRetryRequest sem mudança alguma");
        if (have_ks) c->ks_group = (u16)group;
        c->suite = (u16)suite;
        c->halg = suite == CS_AES256 ? HASH_SHA384 : HASH_SHA256;
        c->hl = hash_len(c->halg);
        /* transcript = message_hash(Hash(CH1)) || HRR || CH2 ... */
        u8 h[4 + HASH_MAX_LEN];
        h[0] = HT_MESSAGE_HASH; h[1] = 0; h[2] = 0; h[3] = (u8)c->hl;
        hash_buf(c->halg, c->ch, c->chlen, h + 4);
        hash_init(&c->tr, c->halg);
        hash_update(&c->tr, h, 4 + c->hl);
        hash_update(&c->tr, msg, mlen);
        if (ck) {
            c->cookie = kmalloc(cklen ? cklen : 1);
            if (!c->cookie) return fail(c, -1, "sem memória");
            memcpy(c->cookie, ck, cklen);
            c->cookielen = cklen;
        }
        kprintf("[tls] %s: HelloRetryRequest (%s%s)\n", c->sni, have_ks ? "grupo " : "cookie",
                have_ks ? group_text(group) : "");
        send_ccs(c);
        return send_client_hello(c, false);
    }

    if (c->hrr && suite != c->suite) return fail(c, AL_ILLEGAL, "cifra mudou após HelloRetryRequest");
    if (!have_ks || !key) return fail(c, AL_MISSING_EXT, "ServerHello sem key_share");
    if (group != c->ks_group || klen != group_keylen(group))
        return fail(c, AL_ILLEGAL, "key_share do servidor inválido");
    if (!c->hrr) {
        c->suite = (u16)suite;
        c->halg = suite == CS_AES256 ? HASH_SHA384 : HASH_SHA256;
        c->hl = hash_len(c->halg);
        hash_init(&c->tr, c->halg);
        hash_update(&c->tr, c->ch, c->chlen);
    }
    hash_update(&c->tr, msg, mlen);

    u8 shared[48];
    if (ecdhe_shared(c, group, key, klen, shared) != 0)
        return fail(c, AL_ILLEGAL, "chave pública %s do servidor inválida", group_text(group));
    kprintf("[tls] %s: TLS 1.3 negociado (%s, %s)\n", c->sni, suite_text(suite), group_text(group));
    u8 th[HASH_MAX_LEN];
    hash_peek(&c->tr, th);
    tls13_derive_handshake(c->halg, shared, group_seclen(group), th, NULL, c->c_hs, c->s_hs, c->master);
    secure_zero(shared, sizeof(shared));
    secure_zero(c->priv, sizeof(c->priv));
    secure_zero(c->epriv, sizeof(c->epriv));
    aead_set(&c->rd, c->halg, c->suite, c->s_hs);
    aead_set(&c->wr, c->halg, c->suite, c->c_hs);
    c->hs = HS_WAIT_EE;
    return 1;   /* troca de chaves de leitura */
}

/* ======================================================================
 * autenticação do servidor
 * ====================================================================== */
static bool tls_insecure_logged;
#ifdef MOS_HOST_TEST
/* só no teste do host: permite capturar a cadeia recebida (tlstest capture) */
void (*tls_test_chain_hook)(const char *host, const u8 *const *ders, const u32 *lens, u32 count);
void (*tls_test_ocsp_hook)(const char *host, const u8 *resp, u32 len);
#endif

static int tls_check_certificate(tls_conn_t *c, const u8 *const *ders, const u32 *lens, u32 count) {
    c->leaf = kmalloc(lens[0] ? lens[0] : 1);
    if (!c->leaf) return fail(c, AL_INTERNAL, "sem memória");
    memcpy(c->leaf, ders[0], lens[0]);
    c->leaf_len = lens[0];
#ifdef MOS_HOST_TEST
    if (tls_test_chain_hook) tls_test_chain_hook(c->sni, ders, lens, count);
#endif
    if (g_tls_insecure) {
        if (!tls_insecure_logged) {
            kprintf("[tls] AVISO: g_tls_insecure ativo — certificados NÃO são verificados\n");
            tls_insecure_logged = true;
        }
        return 0;
    }
    datetime_t now;
    rtc_now(&now);
    bool clock_ok = now.year >= 2024;
    if (!clock_ok)
        kprintf("[tls] relógio (RTC) parece inválido (%04d-%02d-%02d): validade dos certificados NÃO conferida\n",
                now.year, now.month, now.day);
    u64 t0 = timer_ticks();
    char err[160], anchor[96];
    int e = x509_verify_chain(ders, lens, count, c->sni, clock_ok ? &now : NULL, err, sizeof(err),
                              anchor, sizeof(anchor));
    u32 ms = (u32)(timer_ticks() - t0);
    if (e != X509_OK) {
        kprintf("[tls] %s: certificado rejeitado: %s (%u ms)\n", c->sni, err, ms);
        int al = e == X509_E_EXPIRED || e == X509_E_NOT_YET_VALID ? AL_CERT_EXPIRED
               : e == X509_E_UNKNOWN_ISSUER || e == X509_E_SELF_SIGNED || e == X509_E_UNTRUSTED_ROOT ? AL_UNKNOWN_CA
               : e == X509_E_UNSUPPORTED ? AL_UNSUP_CERT : AL_BAD_CERT;
        return fail(c, al, "%s", err);
    }
    kprintf("[tls] %s: cadeia de %u certificado(s) válida até a raiz \"%s\" (%u ms)\n", c->sni, count, anchor, ms);
    /* guarda a cadeia para conferir o OCSP grampeado (no 1.2 ele chega depois) */
    u32 tot = 0;
    for (u32 i = 0; i < count; i++) tot += lens[i];
    c->chain = kmalloc(tot ? tot : 1);
    if (!c->chain) return fail(c, AL_INTERNAL, "sem memória");
    for (u32 i = 0, off = 0; i < count; i++) {
        memcpy(c->chain + off, ders[i], lens[i]);
        c->chain_off[i] = off;
        c->chain_len[i] = lens[i];
        off += lens[i];
    }
    c->chain_n = count;
    return 0;
}

/* revogação via OCSP grampeado. resp == NULL: o servidor não grampeou (soft-fail).
 * revoked => abortar; assinatura inválida ou resposta vencida => abortar;
 * status unknown / resposta inutilizável => aceitar e registrar no log. */
static int tls_check_ocsp(tls_conn_t *c, const u8 *resp, u32 rlen) {
    if (g_tls_insecure) return 0;
    if (!resp) {
        kprintf("[tls] %s: sem resposta OCSP grampeada — revogação não verificada (soft-fail)\n", c->sni);
        return 0;
    }
#ifdef MOS_HOST_TEST
    if (tls_test_ocsp_hook) tls_test_ocsp_hook(c->sni, resp, rlen);
#endif
    if (!c->chain) return 0;
    const u8 *ders[10];
    for (u32 i = 0; i < c->chain_n; i++) ders[i] = c->chain + c->chain_off[i];
    datetime_t now;
    rtc_now(&now);
    bool clock_ok = now.year >= 2024;
    char msg[256];
    u64 t0 = timer_ticks();
    int e = x509_ocsp_check(resp, rlen, ders, c->chain_len, c->chain_n, clock_ok ? &now : NULL, msg, sizeof(msg));
    u32 ms = (u32)(timer_ticks() - t0);
    switch (e) {
    case OCSP_GOOD:
        kprintf("[tls] %s: %s (%u ms)\n", c->sni, msg, ms);
        return 0;
    case OCSP_UNKNOWN:
    case OCSP_UNUSABLE:
        kprintf("[tls] %s: %s — revogação não verificada (soft-fail)\n", c->sni, msg);
        return 0;
    case OCSP_REVOKED:
        kprintf("[tls] %s: %s\n", c->sni, msg);
        return fail(c, AL_CERT_REVOKED, "%s", msg);
    default:
        kprintf("[tls] %s: %s\n", c->sni, msg);
        return fail(c, AL_BAD_CERT_STATUS, "%s", msg);
    }
}

/* CertificateStatus (RFC 6066 §8): status_type = ocsp(1), u24 comprimento, OCSPResponse */
static bool parse_cert_status(const u8 *p, u32 n, const u8 **resp, u32 *rlen) {
    rd_t r = {p, n, false};
    u32 st = rd_u8(&r), l = rd_u24(&r);
    const u8 *d = rd_bytes(&r, l);
    if (r.bad || r.n || st != 1 || !l) return false;
    *resp = d;
    *rlen = l;
    return true;
}

static int tls_check_signature(tls_conn_t *c, u16 sigalg, const u8 *sig, u32 siglen,
                               const u8 *transcript_hash) {
    if (g_tls_insecure) return 0;
    x509_cert_t leaf;
    if (!c->leaf || x509_parse(&leaf, c->leaf, c->leaf_len) != X509_OK)
        return fail(c, AL_INTERNAL, "certificado do servidor indisponível para o CertificateVerify");
    /* conteúdo assinado: 64 espaços || rótulo || 0x00 || Hash(transcript) */
    static const char label[] = "TLS 1.3, server CertificateVerify";
    u8 content[64 + sizeof(label) + HASH_MAX_LEN];
    memset(content, 0x20, 64);
    memcpy(content + 64, label, sizeof(label));          /* inclui o 0x00 final */
    memcpy(content + 64 + sizeof(label), transcript_hash, c->hl);
    u32 clen = 64 + (u32)sizeof(label) + c->hl;
    int kind, halg, curve = 0;
    switch (sigalg) {
    case 0x0403: kind = X509_SIG_ECDSA; halg = HASH_SHA256; curve = EC_P256; break;
    case 0x0503: kind = X509_SIG_ECDSA; halg = HASH_SHA384; curve = EC_P384; break;
    case 0x0804: kind = X509_SIG_RSA_PSS; halg = HASH_SHA256; break;
    case 0x0805: kind = X509_SIG_RSA_PSS; halg = HASH_SHA384; break;
    case 0x0806: kind = X509_SIG_RSA_PSS; halg = HASH_SHA512; break;
    default:
        return fail(c, AL_ILLEGAL, "o servidor usou um esquema de assinatura não oferecido (0x%04x)", sigalg);
    }
    if (kind == X509_SIG_ECDSA ? (leaf.key_type != X509_KEY_EC || leaf.curve != curve) : leaf.key_type != X509_KEY_RSA)
        return fail(c, AL_ILLEGAL, "esquema de assinatura 0x%04x incompatível com a chave do certificado", sigalg);
    u64 t0 = timer_ticks();
    int e = x509_verify_with_key(&leaf, kind, halg, (int)hash_len(halg), content, clen, sig, siglen);
    u32 ms = (u32)(timer_ticks() - t0);
    if (e != X509_OK) {
        kprintf("[tls] %s: assinatura do CertificateVerify inválida (esquema 0x%04x)\n", c->sni, sigalg);
        return fail(c, AL_DECRYPT, "assinatura do servidor (CertificateVerify) inválida");
    }
    kprintf("[tls] %s: CertificateVerify ok (esquema 0x%04x, %u ms)\n", c->sni, sigalg, ms);
    return 0;
}

#define TLS_MAX_CERTS 10

/* Certificate do 1.3 (contexto + extensões por certificado) ou do 1.2 (só a lista) */
static int handle_certificate(tls_conn_t *c, const u8 *body, u32 len) {
    rd_t r = {body, len, false};
    u32 cl = 0;
    if (!c->v12) {
        cl = rd_u8(&r);
        rd_bytes(&r, cl);
    }
    u32 ll = rd_u24(&r);
    const u8 *list = rd_bytes(&r, ll);
    if (r.bad || r.n) return fail(c, AL_DECODE, "mensagem Certificate malformada");
    if (cl != 0) return fail(c, AL_ILLEGAL, "Certificate do servidor com contexto não vazio");
    rd_t l = {list, ll, false};
    const u8 *ders[TLS_MAX_CERTS];
    u32 lens[TLS_MAX_CERTS], count = 0;
    const u8 *staple = NULL;
    u32 staple_len = 0;
    while (l.n) {
        u32 n = rd_u24(&l);
        const u8 *der = rd_bytes(&l, n);
        if (!c->v12) {
            /* 1.3: extensões do CertificateEntry; status_request da folha = OCSP grampeado */
            u32 el = rd_u16(&l);
            rd_t e = {rd_bytes(&l, el), el, l.bad};
            while (e.n && !e.bad) {
                u32 et = rd_u16(&e), elen = rd_u16(&e);
                const u8 *ed = rd_bytes(&e, elen);
                if (!e.bad && et == 5 && count == 0 && !parse_cert_status(ed, elen, &staple, &staple_len))
                    return fail(c, AL_DECODE, "status OCSP do certificado malformado");
            }
            if (e.bad) l.bad = true;
        }
        if (l.bad || !n) return fail(c, AL_DECODE, "lista de certificados malformada");
        if (count < TLS_MAX_CERTS) { ders[count] = der; lens[count] = n; }
        count++;
    }
    if (!count) return fail(c, AL_DECODE, "o servidor não enviou certificado");
    if (tls_check_certificate(c, ders, lens, count < TLS_MAX_CERTS ? count : TLS_MAX_CERTS) < 0) return -1;
    if (!c->v12 && tls_check_ocsp(c, staple, staple_len) < 0) return -1;   /* 1.2: ver HT_CERT_STATUS */
    return 0;
}

static int send_client_flight(tls_conn_t *c) {
    send_ccs(c);   /* compatibilidade com middleboxes */
    if (c->cert_req) {
        /* Certificate vazio (não temos certificado de cliente) */
        u8 m[4 + 1 + 255 + 3];
        u32 n = 0;
        m[n++] = HT_CERTIFICATE;
        put24(m + n, 1 + c->crctx_len + 3); n += 3;
        m[n++] = (u8)c->crctx_len;
        memcpy(m + n, c->crctx, c->crctx_len); n += c->crctx_len;
        put24(m + n, 0); n += 3;
        if (queue_handshake(c, m, n, true) < 0) return -1;
    }
    u8 th[HASH_MAX_LEN], fin[4 + HASH_MAX_LEN];
    hash_peek(&c->tr, th);
    fin[0] = HT_FINISHED;
    put24(fin + 1, c->hl);
    tls13_finished(c->halg, c->c_hs, th, fin + 4);
    return queue_handshake(c, fin, 4 + c->hl, true);
}

static int handle_finished(tls_conn_t *c, const u8 *msg, u32 mlen) {
    if (mlen - 4 != c->hl) return fail(c, AL_DECODE, "Finished do servidor com tamanho errado");
    u8 th[HASH_MAX_LEN], expect[HASH_MAX_LEN];
    hash_peek(&c->tr, th);
    tls13_finished(c->halg, c->s_hs, th, expect);
    if (!ct_memeq(expect, msg + 4, c->hl))
        return fail(c, AL_DECRYPT, "verificação do Finished do servidor falhou");
    hash_update(&c->tr, msg, mlen);
    hash_peek(&c->tr, th);
    tls13_derive_app(c->halg, c->master, th, c->c_ap, c->s_ap);
    if (send_client_flight(c) < 0) return -1;
    aead_set(&c->wr, c->halg, c->suite, c->c_ap);
    aead_set(&c->rd, c->halg, c->suite, c->s_ap);
    secure_zero(c->c_hs, sizeof(c->c_hs));
    secure_zero(c->s_hs, sizeof(c->s_hs));
    secure_zero(c->master, sizeof(c->master));
    kfree(c->ch); c->ch = NULL;
    kfree(c->cookie); c->cookie = NULL;
    kfree(c->leaf); c->leaf = NULL;
    kfree(c->chain); c->chain = NULL;
    c->hs = HS_DONE;
    return 1;   /* troca de chaves de leitura */
}

static int handle_key_update(tls_conn_t *c, const u8 *body, u32 len) {
    if (len != 1 || body[0] > 1) return fail(c, AL_DECODE, "KeyUpdate malformado");
    u8 ns[HASH_MAX_LEN];
    hkdf_expand_label(c->halg, c->s_ap, "traffic upd", NULL, 0, ns, c->hl);
    memcpy(c->s_ap, ns, c->hl);
    aead_set(&c->rd, c->halg, c->suite, c->s_ap);
    if (body[0] == 1) {
        static const u8 ku[5] = {HT_KEY_UPDATE, 0, 0, 1, 0};
        if (queue_handshake(c, ku, 5, false) < 0) return -1;
        hkdf_expand_label(c->halg, c->c_ap, "traffic upd", NULL, 0, ns, c->hl);
        memcpy(c->c_ap, ns, c->hl);
        aead_set(&c->wr, c->halg, c->suite, c->c_ap);
    }
    secure_zero(ns, sizeof(ns));
    return 1;
}

/* ======================================================================
 * TLS 1.2 (RFC 5246 + RFC 8422 ECDHE + RFC 7627 EMS + RFC 7905 ChaCha20)
 * ====================================================================== */

/* assinatura do ServerKeyExchange: client_random || server_random || ServerECDHParams */
static int tls12_check_ske_signature(tls_conn_t *c, u32 sigalg, const u8 *params, u32 plen,
                                     const u8 *sig, u32 siglen) {
    int kind, halg;
    switch (sigalg) {
    case 0x0401: kind = X509_SIG_RSA_PKCS1; halg = HASH_SHA256; break;
    case 0x0501: kind = X509_SIG_RSA_PKCS1; halg = HASH_SHA384; break;
    case 0x0601: kind = X509_SIG_RSA_PKCS1; halg = HASH_SHA512; break;
    case 0x0804: kind = X509_SIG_RSA_PSS; halg = HASH_SHA256; break;
    case 0x0805: kind = X509_SIG_RSA_PSS; halg = HASH_SHA384; break;
    case 0x0806: kind = X509_SIG_RSA_PSS; halg = HASH_SHA512; break;
    case 0x0403: kind = X509_SIG_ECDSA; halg = HASH_SHA256; break;   /* no 1.2: qualquer curva */
    case 0x0503: kind = X509_SIG_ECDSA; halg = HASH_SHA384; break;
    default:
        return fail(c, AL_ILLEGAL, "o servidor usou um esquema de assinatura não oferecido (0x%04x)", sigalg);
    }
    if ((kind == X509_SIG_ECDSA) != suite12_ecdsa(c->suite))
        return fail(c, AL_ILLEGAL, "esquema de assinatura 0x%04x incompatível com a cifra %s", sigalg,
                    suite_text(c->suite));
    if (g_tls_insecure) return 0;
    x509_cert_t leaf;
    if (!c->leaf || x509_parse(&leaf, c->leaf, c->leaf_len) != X509_OK)
        return fail(c, AL_INTERNAL, "certificado do servidor indisponível para o ServerKeyExchange");
    if (kind == X509_SIG_ECDSA ? leaf.key_type != X509_KEY_EC : leaf.key_type != X509_KEY_RSA)
        return fail(c, AL_ILLEGAL, "esquema de assinatura 0x%04x incompatível com a chave do certificado", sigalg);
    u8 content[64 + 4 + 255];
    if (plen > sizeof(content) - 64) return fail(c, AL_DECODE, "ServerKeyExchange malformado");
    memcpy(content, c->random, 32);
    memcpy(content + 32, c->srandom, 32);
    memcpy(content + 64, params, plen);
    u64 t0 = timer_ticks();
    int e = x509_verify_with_key(&leaf, kind, halg, (int)hash_len(halg), content, 64 + plen, sig, siglen);
    u32 ms = (u32)(timer_ticks() - t0);
    if (e != X509_OK) {
        kprintf("[tls] %s: assinatura do ServerKeyExchange inválida (esquema 0x%04x)\n", c->sni, sigalg);
        return fail(c, AL_DECRYPT, "assinatura do servidor (ServerKeyExchange) inválida");
    }
    kprintf("[tls] %s: ServerKeyExchange ok (%s, esquema 0x%04x, %u ms)\n", c->sni, group_text(c->ks_group),
            sigalg, ms);
    return 0;
}

static int handle_server_key_exchange(tls_conn_t *c, const u8 *body, u32 len) {
    rd_t r = {body, len, false};
    u32 ctype = rd_u8(&r), grp = rd_u16(&r), pl = rd_u8(&r);
    const u8 *pt = rd_bytes(&r, pl);
    u32 alg = rd_u16(&r), sl = rd_u16(&r);
    const u8 *sig = rd_bytes(&r, sl);
    if (r.bad || r.n) return fail(c, AL_DECODE, "ServerKeyExchange malformado");
    if (ctype != 3) return fail(c, AL_HS_FAILURE, "ServerKeyExchange sem curva nomeada");
    if (!group_offered(grp))
        return fail(c, AL_ILLEGAL, "o servidor escolheu um grupo não oferecido (0x%04x)", grp);
    c->ks_group = (u16)grp;
    if (tls12_check_ske_signature(c, alg, body, 4 + pl, sig, sl) < 0) return -1;
    if (ensure_ec(c, grp) < 0) return fail(c, AL_INTERNAL, "falha ao gerar a chave efêmera %s", group_text(grp));
    c->pms_len = group_seclen(grp);
    if (ecdhe_shared(c, grp, pt, pl, c->pms) != 0)
        return fail(c, AL_ILLEGAL, "chave pública %s do servidor inválida", group_text(grp));
    secure_zero(c->priv, sizeof(c->priv));
    secure_zero(c->epriv, sizeof(c->epriv));
    return 0;
}

/* resposta ao ServerHelloDone: [Certificate vazio] ClientKeyExchange CCS Finished */
static int send_client_flight12(tls_conn_t *c) {
    if (c->cert_req) {
        static const u8 empty[7] = {HT_CERTIFICATE, 0, 0, 3, 0, 0, 0};
        if (queue_handshake(c, empty, sizeof(empty), true) < 0) return -1;
    }
    u32 pl = group_keylen(c->ks_group);
    u8 cke[5 + 97];
    cke[0] = HT_CLIENT_KEY_EXCHANGE;
    put24(cke + 1, 1 + pl);
    cke[4] = (u8)pl;
    memcpy(cke + 5, c->ks_group == GRP_X25519 ? c->pub : c->epub, pl);
    if (queue_handshake(c, cke, 5 + pl, true) < 0) return -1;

    /* master secret: com EMS, sobre o hash da sessão até o ClientKeyExchange */
    u8 seed[64];
    if (c->ems) {
        u8 sh[HASH_MAX_LEN];
        hash_peek(&c->tr, sh);
        tls12_prf(c->halg, c->pms, c->pms_len, "extended master secret", sh, c->hl, c->master, 48);
    } else {
        memcpy(seed, c->random, 32);
        memcpy(seed + 32, c->srandom, 32);
        tls12_prf(c->halg, c->pms, c->pms_len, "master secret", seed, 64, c->master, 48);
    }
    secure_zero(c->pms, sizeof(c->pms));

    /* key_block = client_key || server_key || client_iv || server_iv (AEAD: sem chaves de MAC) */
    u16 aead = suite12_aead(c->suite);
    u32 kl = aead == CS_AES128 ? 16 : 32, il = aead == CS_CHACHA ? 12 : 4;
    u8 kb[2 * 32 + 2 * 12];
    memcpy(seed, c->srandom, 32);
    memcpy(seed + 32, c->random, 32);
    tls12_prf(c->halg, c->master, 48, "key expansion", seed, 64, kb, 2 * kl + 2 * il);

    u8 one = 1;
    if (queue_record(c, CT_CCS, &one, 1, 0x0303) < 0) return fail(c, AL_INTERNAL, "buffer de saída TLS cheio");
    c->sent_ccs = true;
    aead_set12(&c->wr, aead, kb, kb + 2 * kl);
    aead_set12(&c->rd_next, aead, kb + kl, kb + 2 * kl + il);
    secure_zero(kb, sizeof(kb));

    u8 th[HASH_MAX_LEN], fin[4 + 12];
    hash_peek(&c->tr, th);
    fin[0] = HT_FINISHED;
    put24(fin + 1, 12);
    tls12_prf(c->halg, c->master, 48, "client finished", th, c->hl, fin + 4, 12);
    if (queue_handshake(c, fin, sizeof(fin), true) < 0) return -1;
    c->hs = HS12_WAIT_CCS;
    return 0;
}

static int handle_finished12(tls_conn_t *c, const u8 *body, u32 blen) {
    if (blen != 12) return fail(c, AL_DECODE, "Finished do servidor com tamanho errado");
    u8 th[HASH_MAX_LEN], expect[12];
    hash_peek(&c->tr, th);
    tls12_prf(c->halg, c->master, 48, "server finished", th, c->hl, expect, 12);
    if (!ct_memeq(expect, body, 12)) return fail(c, AL_DECRYPT, "verificação do Finished do servidor falhou");
    secure_zero(c->master, sizeof(c->master));
    kfree(c->ch); c->ch = NULL;
    kfree(c->leaf); c->leaf = NULL;
    kfree(c->chain); c->chain = NULL;
    c->hs = HS_DONE;
    kprintf("[tls] %s: handshake TLS 1.2 concluído\n", c->sni);
    return 0;
}

static int handle_hs_message12(tls_conn_t *c, const u8 *msg, u32 mlen) {
    u8 t = msg[0];
    const u8 *body = msg + 4;
    u32 blen = mlen - 4;
    if (t == HT_HELLO_REQUEST) {               /* nunca renegociamos: ignorado (fora do transcript) */
        if (blen) return fail(c, AL_DECODE, "HelloRequest malformado");
        return 0;
    }
    switch (c->hs) {
    case HS12_WAIT_CERT:
        if (t != HT_CERTIFICATE) return fail(c, AL_UNEXPECTED, "esperava Certificate (veio tipo %u)", t);
        if (handle_certificate(c, body, blen) < 0) return -1;
        hash_update(&c->tr, msg, mlen);
        c->hs = HS12_WAIT_SKE;
        return 0;
    case HS12_WAIT_SKE:
        if (t == HT_CERT_STATUS && c->ocsp_req && !c->ocsp_done) {
            const u8 *resp;
            u32 rlen;
            if (!parse_cert_status(body, blen, &resp, &rlen))
                return fail(c, AL_DECODE, "CertificateStatus malformado");
            c->ocsp_done = true;
            if (tls_check_ocsp(c, resp, rlen) < 0) return -1;
            hash_update(&c->tr, msg, mlen);
            return 0;
        }
        if (t != HT_SERVER_KEY_EXCHANGE)
            return fail(c, AL_UNEXPECTED, "esperava ServerKeyExchange (veio tipo %u)", t);
        if (!c->ocsp_done && tls_check_ocsp(c, NULL, 0) < 0) return -1;   /* sem grampo: soft-fail */
        if (handle_server_key_exchange(c, body, blen) < 0) return -1;
        hash_update(&c->tr, msg, mlen);
        c->hs = HS12_WAIT_SHD;
        return 0;
    case HS12_WAIT_SHD:
        if (t == HT_CERT_REQUEST && !c->cert_req) {
            if (blen < 1) return fail(c, AL_DECODE, "CertificateRequest malformado");
            c->cert_req = true;
            hash_update(&c->tr, msg, mlen);
            return 0;
        }
        if (t != HT_SERVER_HELLO_DONE) return fail(c, AL_UNEXPECTED, "esperava ServerHelloDone (veio tipo %u)", t);
        if (blen) return fail(c, AL_DECODE, "ServerHelloDone malformado");
        hash_update(&c->tr, msg, mlen);
        return send_client_flight12(c);
    case HS12_WAIT_CCS:
        return fail(c, AL_UNEXPECTED, "esperava ChangeCipherSpec (veio handshake tipo %u)", t);
    case HS12_WAIT_FIN:
        if (t != HT_FINISHED) return fail(c, AL_UNEXPECTED, "esperava Finished (veio tipo %u)", t);
        return handle_finished12(c, body, blen);
    case HS_DONE:
        return fail(c, AL_UNEXPECTED, "mensagem de handshake inesperada após o handshake (tipo %u)", t);
    }
    return fail(c, AL_INTERNAL, "estado TLS inválido");
}

/* processa uma mensagem completa; 1 = houve troca de chave de leitura */
static int handle_hs_message(tls_conn_t *c, const u8 *msg, u32 mlen) {
    u8 t = msg[0];
    const u8 *body = msg + 4;
    u32 blen = mlen - 4;
    if (c->v12) return handle_hs_message12(c, msg, mlen);
    switch (c->hs) {
    case HS_WAIT_SH:
        if (t != HT_SERVER_HELLO) return fail(c, AL_UNEXPECTED, "esperava ServerHello (veio tipo %u)", t);
        return handle_server_hello(c, msg, mlen);
    case HS_WAIT_EE:
        if (t != HT_ENCRYPTED_EXT) return fail(c, AL_UNEXPECTED, "esperava EncryptedExtensions (veio tipo %u)", t);
        if (blen < 2 || be16(body) + 2 != blen) return fail(c, AL_DECODE, "EncryptedExtensions malformado");
        hash_update(&c->tr, msg, mlen);
        c->hs = HS_WAIT_CERT_CR;
        return 0;
    case HS_WAIT_CERT_CR:
        if (t == HT_CERT_REQUEST) {
            if (blen < 1 || body[0] + 1u > blen) return fail(c, AL_DECODE, "CertificateRequest malformado");
            c->crctx_len = body[0];
            memcpy(c->crctx, body + 1, c->crctx_len);
            c->cert_req = true;
            hash_update(&c->tr, msg, mlen);
            c->hs = HS_WAIT_CERT;
            return 0;
        }
        /* fallthrough */
    case HS_WAIT_CERT:
        if (t != HT_CERTIFICATE) return fail(c, AL_UNEXPECTED, "esperava Certificate (veio tipo %u)", t);
        if (handle_certificate(c, body, blen) < 0) return -1;
        hash_update(&c->tr, msg, mlen);
        c->hs = HS_WAIT_CV;
        return 0;
    case HS_WAIT_CV: {
        if (t != HT_CERT_VERIFY) return fail(c, AL_UNEXPECTED, "esperava CertificateVerify (veio tipo %u)", t);
        rd_t r = {body, blen, false};
        u32 alg = rd_u16(&r), sl = rd_u16(&r);
        const u8 *sig = rd_bytes(&r, sl);
        if (r.bad || r.n) return fail(c, AL_DECODE, "CertificateVerify malformado");
        u8 th[HASH_MAX_LEN];
        hash_peek(&c->tr, th);
        if (tls_check_signature(c, (u16)alg, sig, sl, th) != 0) return -1;
        hash_update(&c->tr, msg, mlen);
        c->hs = HS_WAIT_FIN;
        return 0;
    }
    case HS_WAIT_FIN:
        if (t != HT_FINISHED) return fail(c, AL_UNEXPECTED, "esperava Finished (veio tipo %u)", t);
        return handle_finished(c, msg, mlen);
    case HS_DONE:
        if (t == HT_NEW_SESSION_TICKET) return 0;          /* sem retomada: ignorado */
        if (t == HT_KEY_UPDATE) return handle_key_update(c, body, blen);
        return fail(c, AL_UNEXPECTED, "mensagem de handshake inesperada após o handshake (tipo %u)", t);
    }
    return fail(c, AL_INTERNAL, "estado TLS inválido");
}

static int append_hs(tls_conn_t *c, const u8 *p, u32 n) {
    if (c->hlen + n > c->hcap) {
        u32 cap = c->hcap ? c->hcap : 4096;
        while (cap < c->hlen + n) cap *= 2;
        if (cap > TLS_HS_MAX + 8) return fail(c, AL_OVERFLOW, "mensagem de handshake grande demais");
        u8 *nb = kmalloc(cap);
        if (!nb) return fail(c, AL_INTERNAL, "sem memória");
        if (c->hlen) memcpy(nb, c->hbuf, c->hlen);
        kfree(c->hbuf);
        c->hbuf = nb;
        c->hcap = cap;
    }
    memcpy(c->hbuf + c->hlen, p, n);
    c->hlen += n;
    u32 off = 0;
    while (c->hlen - off >= 4) {
        u32 ml = be24(c->hbuf + off + 1);
        if (ml > TLS_HS_MAX) return fail(c, AL_OVERFLOW, "mensagem de handshake grande demais");
        if (c->hlen - off < 4 + ml) break;
        int r = handle_hs_message(c, c->hbuf + off, 4 + ml);
        if (r < 0) return -1;
        off += 4 + ml;
        /* mudança de chave deve coincidir com o fim do registro */
        if (r == 1 && off != c->hlen)
            return fail(c, AL_UNEXPECTED, "dados extras após mudança de chaves");
    }
    if (off) {
        memmove(c->hbuf, c->hbuf + off, c->hlen - off);
        c->hlen -= off;
    }
    return 0;
}

/* ======================================================================
 * entrada de registros
 * ====================================================================== */
static int handle_record(tls_conn_t *c, u8 type, u8 *hdr, u8 *data, u32 len) {
    if (type == CT_CCS) {
        if (c->v12) {
            /* TLS 1.2: CCS de verdade — ativa as chaves de leitura (seq volta a 0) */
            if (c->hs != HS12_WAIT_CCS || len != 1 || data[0] != 1 || c->rd.on)
                return fail(c, AL_UNEXPECTED, "ChangeCipherSpec inesperado");
            if (c->hlen) return fail(c, AL_UNEXPECTED, "ChangeCipherSpec no meio de uma mensagem de handshake");
            c->rd = c->rd_next;
            secure_zero(&c->rd_next, sizeof(c->rd_next));
            c->hs = HS12_WAIT_FIN;
            return 0;
        }
        if (c->hs < HS_DONE && len == 1 && data[0] == 1) return 0;   /* CCS fictício */
        return fail(c, AL_UNEXPECTED, "ChangeCipherSpec inesperado");
    }
    if (c->rd.on && c->rd.v12) {
        /* TLS 1.2: tipo externo é o real; GCM traz o nonce explícito de 8 bytes */
        u32 ex = c->rd.suite == CS_CHACHA ? 0 : 8;
        if (len < ex + 16) return fail(c, AL_BAD_MAC, "registro cifrado curto demais");
        u8 nonce[12], aad[13];
        aead_nonce(&c->rd, nonce);
        if (ex) memcpy(nonce + 4, data, 8);
        u32 clen = len - ex - 16;
        aad12(aad, c->rd.seq, type, hdr + 1, clen);
        data += ex;
        int ok = c->rd.suite == CS_CHACHA
            ? chacha20poly1305_open(c->rd.key, nonce, aad, 13, data, clen, data, data + clen)
            : aes_gcm_open(&c->rd.gcm, nonce, aad, 13, data, clen, data, data + clen);
        if (ok != 0) return fail(c, AL_BAD_MAC, "falha ao decifrar registro (MAC inválido)");
        c->rd.seq++;
        len = clen;
        if (len > TLS_MAX_PLAIN) return fail(c, AL_OVERFLOW, "registro grande demais");
    } else if (c->rd.on) {
        if (type != CT_APP) {
            if (type == CT_ALERT && len == 2 && c->hs < HS_DONE) { /* alerta em claro do servidor */ }
            else return fail(c, AL_UNEXPECTED, "registro sem cifra após a troca de chaves");
        } else {
            if (len < 17) return fail(c, AL_BAD_MAC, "registro cifrado curto demais");
            u8 nonce[12];
            aead_nonce(&c->rd, nonce);
            u32 clen = len - 16;
            int ok = c->rd.suite == CS_CHACHA
                ? chacha20poly1305_open(c->rd.key, nonce, hdr, 5, data, clen, data, data + clen)
                : aes_gcm_open(&c->rd.gcm, nonce, hdr, 5, data, clen, data, data + clen);
            if (ok != 0) return fail(c, AL_BAD_MAC, "falha ao decifrar registro (MAC inválido)");
            c->rd.seq++;
            while (clen && data[clen - 1] == 0) clen--;
            if (!clen) return fail(c, AL_UNEXPECTED, "registro cifrado sem tipo de conteúdo");
            type = data[--clen];
            len = clen;
            if (len > TLS_MAX_PLAIN) return fail(c, AL_OVERFLOW, "registro grande demais");
        }
    } else if (type == CT_APP) {
        return fail(c, AL_UNEXPECTED, "dados de aplicação antes do handshake");
    }

    switch (type) {
    case CT_HANDSHAKE:
        if (!len) return fail(c, AL_UNEXPECTED, "registro de handshake vazio");
        return append_hs(c, data, len);
    case CT_ALERT:
        if (len != 2) return fail(c, AL_DECODE, "alerta malformado");
        if (data[1] == AL_CLOSE_NOTIFY) {
            c->closed = true;
            if (c->hs < HS_DONE) return fail(c, -1, "o servidor encerrou a conexão durante o handshake");
            return 0;
        }
        if (data[1] == AL_USER_CANCELED) return 0;
        /* alertas de aviso (nível 1) não são fatais no TLS 1.2 — p. ex. unrecognized_name
         * enviado junto do ServerHello por alguns servidores */
        if (data[0] == 1 && (c->v12 || (c->hs == HS_WAIT_SH && data[1] == AL_UNRECOGNIZED_NAME))) {
            kprintf("[tls] %s: alerta de aviso ignorado: %s (%u)\n", c->sni, alert_text(data[1]), data[1]);
            return 0;
        }
        return fail(c, -1, "o servidor enviou alerta: %s (%u)", alert_text(data[1]), data[1]);
    case CT_APP:
        if (c->hs != HS_DONE) return fail(c, AL_UNEXPECTED, "dados de aplicação antes do fim do handshake");
        if (c->hlen) return fail(c, AL_UNEXPECTED, "dados de aplicação intercalados com handshake");
        if (c->aoff == c->alen) c->aoff = c->alen = 0;
        if (c->alen + len > TLS_MAX_PLAIN + 64) return fail(c, AL_INTERNAL, "buffer de aplicação cheio");
        memcpy(c->abuf + c->alen, data, len);
        c->alen += len;
        return 0;
    default:
        return fail(c, AL_UNEXPECTED, "tipo de registro desconhecido (%u)", type);
    }
}

/* lê do TCP e processa registros completos.
 * want_app: para assim que houver dados de aplicação; senão para ao concluir o handshake. */
static int pump(tls_conn_t *c, bool want_app) {
    for (;;) {
        if (c->hs == HS_ERROR) return -1;
        if (want_app ? c->alen > c->aoff : c->hs == HS_DONE) return 0;
        if (c->closed) return 0;
        u32 need = 5;
        if (c->rlen >= 5) {
            u32 rl = be16(c->rbuf + 3);
            if (rl > TLS_MAX_CIPHER) return fail(c, AL_OVERFLOW, "registro TLS grande demais (%u bytes)", rl);
            need = 5 + rl;
        }
        if (c->rlen < need) {
            int n = tcp_recv(c->sock, c->rbuf + c->rlen, (int)(TLS_RBUF - c->rlen));
            if (n < 0) {
                if (c->hs != HS_DONE) return fail(c, -1, "conexão TCP encerrada durante o handshake");
                c->closed = true;        /* sem close_notify: tratado como fim de arquivo */
                return 0;
            }
            if (n == 0) return 0;
            c->rlen += (u32)n;
            continue;
        }
        u8 type = c->rbuf[0];
        u32 ver = be16(c->rbuf + 1);
        if ((ver >> 8) != 3) return fail(c, AL_DECODE, "registro com versão inválida (servidor não fala TLS?)");
        int r = handle_record(c, type, c->rbuf, c->rbuf + 5, need - 5);
        memmove(c->rbuf, c->rbuf + need, c->rlen - need);
        c->rlen -= need;
        if (r < 0) return -1;
    }
}

/* ======================================================================
 * API pública
 * ====================================================================== */
tls_conn_t *tls_new(int tcp_sock, const char *sni) {
    tls_conn_t *c = kmalloc(sizeof(*c));
    if (!c) return NULL;
    memset(c, 0, sizeof(*c));
    c->sock = tcp_sock;
    c->hs = HS_START;
    if (sni) {
        u32 n = (u32)strlen(sni);
        if (n > sizeof(c->sni) - 1) n = sizeof(c->sni) - 1;
        memcpy(c->sni, sni, n);
        c->sni[n] = 0;
    }
    c->rbuf = kmalloc(TLS_RBUF);
    c->obuf = kmalloc(TLS_OBUF);
    c->abuf = kmalloc(TLS_MAX_PLAIN + 64);
    if (!c->rbuf || !c->obuf || !c->abuf) { tls_free(c); return NULL; }
    crypto_random(c->priv, 32);
    crypto_random(c->random, 32);
    crypto_random(c->sid, 32);
    x25519_base(c->pub, c->priv);
    /* key_share inicial do 1.3: o primeiro grupo suportado da lista de preferências */
    c->ks_group = GRP_X25519;
    for (int i = 0; i < 4 && tls_group_prefs[i]; i++)
        if (group_offered(tls_group_prefs[i])) { c->ks_group = tls_group_prefs[i]; break; }
    snprintf(c->err, sizeof(c->err), "sem erro");
    return c;
}

int tls_handshake(tls_conn_t *c) {
    if (!c) return -1;
    if (c->hs == HS_ERROR) return -1;
    if (c->hs == HS_DONE) return flush_out(c) < 0 ? fail(c, -1, "erro ao enviar dados TCP") : 1;
    int st = tcp_state(c->sock);
    if (st == TCP_ERROR || st == TCP_CLOSED) return fail(c, -1, "conexão TCP falhou ou foi fechada");
    if (c->hs == HS_START) {
        if (st == TCP_CONNECTING) return 0;
        if (send_client_hello(c, true) < 0) return -1;
        c->hs = HS_WAIT_SH;
    }
    if (flush_out(c) < 0) return fail(c, -1, "erro ao enviar dados TCP");
    if (pump(c, false) < 0) return -1;
    if (flush_out(c) < 0) return fail(c, -1, "erro ao enviar dados TCP");
    return c->hs == HS_DONE ? 1 : 0;
}

int tls_send(tls_conn_t *c, const void *data, int len) {
    if (!c || c->hs == HS_ERROR) return -1;
    if (c->hs != HS_DONE) {
        int r = tls_handshake(c);
        if (r <= 0) return r;
    }
    if (flush_out(c) < 0) return fail(c, -1, "erro ao enviar dados TCP");
    const u8 *p = data;
    int done = 0;
    while (done < len) {
        u32 k = (u32)(len - done);
        if (k > TLS_MAX_PLAIN) k = TLS_MAX_PLAIN;
        if (!out_space(c, rec_overhead(&c->wr) + k)) break;
        queue_record(c, CT_APP, p + done, k, 0x0303);
        done += (int)k;
    }
    if (flush_out(c) < 0) return fail(c, -1, "erro ao enviar dados TCP");
    return done;
}

int tls_recv(tls_conn_t *c, void *buf, int max) {
    if (!c || c->hs == HS_ERROR) return -1;
    if (c->hs != HS_DONE) {
        int r = tls_handshake(c);
        if (r <= 0) return r;
    }
    if (flush_out(c) < 0) return fail(c, -1, "erro ao enviar dados TCP");
    if (c->alen == c->aoff) {
        if (pump(c, true) < 0) return -1;
        if (flush_out(c) < 0) return fail(c, -1, "erro ao enviar dados TCP");
    }
    u32 avail = c->alen - c->aoff;
    if (avail) {
        u32 k = (u32)max < avail ? (u32)max : avail;
        memcpy(buf, c->abuf + c->aoff, k);
        c->aoff += k;
        return (int)k;
    }
    return c->closed ? -1 : 0;
}

void tls_free(tls_conn_t *c) {
    if (!c) return;
    if (c->hs == HS_DONE && !c->closed && c->obuf) {
        u8 a[2] = {1, AL_CLOSE_NOTIFY};
        if (queue_record(c, CT_ALERT, a, 2, 0x0303) == 0) flush_out(c);
    }
    kfree(c->rbuf);
    kfree(c->obuf);
    kfree(c->abuf);
    kfree(c->hbuf);
    kfree(c->ch);
    kfree(c->cookie);
    kfree(c->leaf);
    kfree(c->chain);
    secure_zero(c, sizeof(*c));
    kfree(c);
}

const char *tls_error(tls_conn_t *c) {
    return c ? c->err : "conexão TLS inexistente";
}
