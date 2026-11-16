/*
 * mOS — mnet: núcleo de rede de alto nível dos apps (docs/rede-apps.md).
 *
 * C portátil, sem ObjC e sem chamada de sistema: compila igual no libSystem (freestanding) e no host
 * (gcc -DMNET_HOST) para os testes de tools/mnet_host_test.c. NSURLSession, CFNetwork, libcurl e
 * Network.framework são cascas sobre estas peças.
 *
 *   mnet_buf / mnet_hdrs   memória crescente e lista de cabeçalhos (nome sem caixa)
 *   mnet_h1_*              HTTP/1.1: pedido e parser incremental de resposta (RFC 9112)
 *   mnet_jar_*             cookies (RFC 6265), arquivo no formato Netscape/curl
 *   mnet_hpack_*           HPACK (RFC 7541) para o HTTP/2
 */
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef MNET_HOST
#include <stdlib.h>
#include <string.h>
#else
void *malloc(size_t n);
void *calloc(size_t n, size_t m);
void *realloc(void *p, size_t n);
void free(void *p);
void *memcpy(void *d, const void *s, size_t n);
void *memmove(void *d, const void *s, size_t n);
void *memset(void *d, int c, size_t n);
int memcmp(const void *a, const void *b, size_t n);
size_t strlen(const char *s);
int strcmp(const char *a, const char *b);
int strncmp(const char *a, const char *b, size_t n);
void *memchr(const void *s, int c, size_t n);
char *strchr(const char *s, int c);
#endif

/* ---- utilitários */
typedef struct { uint8_t *p; size_t n, cap; } mnet_buf_t;
int mnet_buf_put(mnet_buf_t *b, const void *d, size_t n);   /* 0 ok, -1 sem memória */
int mnet_buf_puts(mnet_buf_t *b, const char *s);
int mnet_buf_putc(mnet_buf_t *b, int c);
int mnet_buf_putu(mnet_buf_t *b, unsigned long long v);
void mnet_buf_free(mnet_buf_t *b);
void mnet_buf_consume(mnet_buf_t *b, size_t n);            /* tira n bytes do começo */

int mnet_lc(int c);
int mnet_ieq(const char *a, const char *b);                 /* igualdade sem caixa (ASCII) */
int mnet_ieqn(const char *a, const char *b, size_t n);
char *mnet_strndup(const char *s, size_t n);
int mnet_tok_has(const char *list, const char *tok);        /* "gzip, chunked" contém "chunked" (sem caixa) */
/* data HTTP (IMF-fixdate, RFC 850, asctime) e o algoritmo de datas do RFC 6265 §5.1.1 -> segundos Unix; -1 inválida */
long long mnet_http_date(const char *s, size_t n);
size_t mnet_fmt_date(long long t, char out[30]);           /* "Sun, 06 Nov 1994 08:49:37 GMT" */

/* ---- cabeçalhos */
typedef struct { char *name, *value; } mnet_hdr_t;
typedef struct { mnet_hdr_t *v; int n, cap; } mnet_hdrs_t;
int mnet_hdrs_add(mnet_hdrs_t *h, const char *name, size_t nl, const char *value, size_t vl);
int mnet_hdrs_set(mnet_hdrs_t *h, const char *name, const char *value);   /* troca todos os de mesmo nome */
void mnet_hdrs_del(mnet_hdrs_t *h, const char *name);
const char *mnet_hdrs_get(const mnet_hdrs_t *h, const char *name);       /* primeiro, sem caixa */
/* junta repetidos com ", " (Set-Cookie fica de fora, RFC 9110 §5.3); devolve malloc ou NULL */
char *mnet_hdrs_join(const mnet_hdrs_t *h, const char *name);
void mnet_hdrs_free(mnet_hdrs_t *h);

/* ---- HTTP/1.1 */
/* Monta a cabeça do pedido. path é o alvo (origin-form, ou absolute-form para proxy).
   body_len < 0: sem corpo; -2: chunked. Cabeçalhos do chamador vão como vieram (Host primeiro se faltar). */
int mnet_h1_request(mnet_buf_t *out, const char *method, const char *target, const char *host,
                    const mnet_hdrs_t *hdrs, long long body_len);
int mnet_h1_chunk(mnet_buf_t *out, const void *d, size_t n);   /* um pedaço chunked; n = 0 é o último */

enum { MNET_H1_STATUS, MNET_H1_HEADERS, MNET_H1_BODY_LEN, MNET_H1_BODY_CLOSE, MNET_H1_CHUNK_SIZE,
       MNET_H1_CHUNK_DATA, MNET_H1_CHUNK_CRLF, MNET_H1_TRAILERS, MNET_H1_DONE, MNET_H1_ERROR };
typedef struct mnet_h1 {
    int state;
    int no_body;                 /* pedido HEAD ou CONNECT bem-sucedido: resposta sem corpo */
    int status, minor;           /* 200, HTTP/1.minor */
    char reason[64];
    mnet_hdrs_t hdrs, trailers;
    long long length;            /* Content-Length ou -1 */
    long long left;              /* bytes restantes do corpo/pedaço */
    int chunked, keep_alive, upgrade;
    int head_done;               /* cabeçalhos finais (não 1xx) prontos */
    mnet_buf_t line;             /* linha parcial */
    size_t head_bytes;           /* limite de tamanho da cabeça */
    const char *err;
    /* eventos */
    void *ctx;
    void (*on_info)(void *ctx, int status, const mnet_hdrs_t *h);          /* 1xx (menos 101) */
    int (*on_body)(void *ctx, const uint8_t *d, size_t n);                 /* != 0 aborta */
} mnet_h1_t;
void mnet_h1_init(mnet_h1_t *p, int no_body);
/* Consome o que puder; devolve bytes usados (< n só quando a mensagem termina antes: o resto é da próxima)
   ou -1 em erro (p->err). */
long mnet_h1_feed(mnet_h1_t *p, const uint8_t *d, size_t n);
int mnet_h1_eof(mnet_h1_t *p);   /* conexão fechou: 0 se a resposta terminou bem, -1 truncada */
void mnet_h1_free(mnet_h1_t *p);

/* ---- cookies (RFC 6265) */
enum { MNET_SAMESITE_NONE_SET = 0, MNET_SAMESITE_NONE, MNET_SAMESITE_LAX, MNET_SAMESITE_STRICT };
typedef struct mnet_cookie {
    char *name, *value, *domain, *path;
    long long expires;           /* segundos Unix; -1 = sessão */
    long long created, accessed;
    unsigned long long seq;      /* ordem de criação (desempate do §5.4 com o mesmo segundo) */
    unsigned host_only : 1, secure : 1, http_only : 1, persistent : 1;
    int same_site;
    struct mnet_cookie *next;
} mnet_cookie_t;
typedef struct { mnet_cookie_t *head; int count; int max_count; } mnet_jar_t;
void mnet_jar_init(mnet_jar_t *j);
/* Set-Cookie recebido de (scheme, host, path); http = 0 quando vem de API não-HTTP (document.cookie).
   Devolve 1 guardado, 0 rejeitado/apagado. */
int mnet_jar_set(mnet_jar_t *j, const char *set_cookie, const char *scheme, const char *host, const char *path,
                 int http, long long now);
/* cabeçalho Cookie para o pedido (malloc, NULL se nenhum). */
char *mnet_jar_header(mnet_jar_t *j, const char *scheme, const char *host, const char *path, int http, long long now);
void mnet_jar_expire(mnet_jar_t *j, long long now);
int mnet_jar_insert(mnet_jar_t *j, const mnet_cookie_t *c, long long now);   /* cópia, troca o mesmo nome/domínio/caminho */
int mnet_jar_remove(mnet_jar_t *j, const char *name, const char *domain, const char *path);
void mnet_jar_clear(mnet_jar_t *j);
/* arquivo Netscape (o mesmo do curl): só os persistentes, ou todos com sessão = 0 de validade */
int mnet_jar_save(const mnet_jar_t *j, mnet_buf_t *out, int include_session);
int mnet_jar_load(mnet_jar_t *j, const char *text, size_t n, long long now);
int mnet_domain_match(const char *host, const char *domain);
int mnet_path_match(const char *req_path, const char *cookie_path);
int mnet_public_suffix(const char *domain);   /* 1 se for sufixo público conhecido */

/* ---- HPACK (RFC 7541) */
typedef struct { char *name, *value; size_t size; } mnet_hp_ent_t;
typedef struct {
    mnet_hp_ent_t *e;             /* anel: e[(first + i) % cap], i = 0 é a mais nova */
    int first, count, cap;
    size_t size, max;             /* tamanho atual e limite (SETTINGS_HEADER_TABLE_SIZE) */
    size_t pending_max;           /* codificador: novo limite a anunciar; (size_t)-1 = nenhum */
    size_t pending_min;           /* o menor limite desde o último bloco (§4.2) */
} mnet_hpack_t;
void mnet_hpack_init(mnet_hpack_t *t, size_t max);
void mnet_hpack_free(mnet_hpack_t *t);
void mnet_hpack_set_max(mnet_hpack_t *t, size_t max);   /* codificador: o próximo bloco anuncia */
/* decodifica um bloco inteiro; 0 ok, -1 erro de compressão (COMPRESSION_ERROR) */
int mnet_hpack_decode(mnet_hpack_t *t, const uint8_t *d, size_t n, mnet_hdrs_t *out, size_t limit_max);
/* codifica; nomes em minúsculas; sensíveis (authorization, cookie curto) vão como "nunca indexar" */
int mnet_hpack_encode(mnet_hpack_t *t, const mnet_hdrs_t *h, mnet_buf_t *out, int huffman);
int mnet_huff_decode(const uint8_t *d, size_t n, mnet_buf_t *out);
size_t mnet_huff_len(const uint8_t *d, size_t n);
int mnet_huff_encode(const uint8_t *d, size_t n, mnet_buf_t *out);

/* ---- HTTP/2 (RFC 9113). Codec sem E/S: mnet_h2_feed recebe bytes da rede, a saída vai para c->out.
   is_server = 0 no cliente (NSURLSession, libcurl); 1 só para servidores locais e testes. */
enum { MNET_H2_NO_ERROR = 0, MNET_H2_PROTOCOL_ERROR = 1, MNET_H2_INTERNAL_ERROR = 2, MNET_H2_FLOW_CONTROL_ERROR = 3,
       MNET_H2_SETTINGS_TIMEOUT = 4, MNET_H2_STREAM_CLOSED = 5, MNET_H2_FRAME_SIZE_ERROR = 6,
       MNET_H2_REFUSED_STREAM = 7, MNET_H2_CANCEL = 8, MNET_H2_COMPRESSION_ERROR = 9,
       MNET_H2_ENHANCE_YOUR_CALM = 11 };
typedef struct mnet_h2_stream {
    uint32_t id;
    int state;                         /* 0 aberto, 1 meio fechado (local), 2 meio fechado (remoto), 3 fechado */
    long long send_win, recv_win;
    long long recv_unacked;            /* bytes recebidos ainda sem WINDOW_UPDATE */
    int got_headers;
    void *user;
    struct mnet_h2_stream *next;
} mnet_h2_stream_t;
typedef struct mnet_h2 {
    int is_server, dead, goaway_recv, goaway_sent;
    uint32_t next_id, last_peer_id, goaway_last;
    int err_code;
    const char *err;
    mnet_buf_t in, out;
    int preface_done, preface_in, settings_acked;
    /* ajustes do par */
    uint32_t peer_max_frame, peer_max_concurrent, peer_header_table, peer_max_header_list;
    long long peer_initial_win;
    int peer_enable_push;
    /* nossos */
    uint32_t local_max_frame;
    long long local_initial_win, conn_recv_win, conn_recv_unacked, conn_send_win;
    mnet_hpack_t enc, dec;
    mnet_h2_stream_t *streams;
    int open_streams;
    /* bloco de cabeçalho em montagem (HEADERS/PUSH_PROMISE + CONTINUATION) */
    uint32_t hb_stream; int hb_end_stream; mnet_buf_t hb;
    /* eventos */
    void *ctx;
    void (*on_headers)(void *ctx, mnet_h2_stream_t *s, const mnet_hdrs_t *h, int end_stream);
    int (*on_data)(void *ctx, mnet_h2_stream_t *s, const uint8_t *d, size_t n, int end_stream);
    void (*on_close)(void *ctx, mnet_h2_stream_t *s, int error_code);   /* RST ou fim normal (0) */
    void (*on_goaway)(void *ctx, uint32_t last_id, int error_code);
    void (*on_ping_ack)(void *ctx, const uint8_t opaque[8]);
} mnet_h2_t;
int mnet_h2_init(mnet_h2_t *c, int is_server);           /* enfileira o prefácio e SETTINGS */
void mnet_h2_free(mnet_h2_t *c);
long mnet_h2_feed(mnet_h2_t *c, const uint8_t *d, size_t n);   /* -1: erro de conexão (GOAWAY já enfileirado) */
/* cliente: abre fluxo com cabeçalhos (pseudo :method :scheme :authority :path primeiro); NULL se não puder */
mnet_h2_stream_t *mnet_h2_request(mnet_h2_t *c, const mnet_hdrs_t *h, int end_stream);
int mnet_h2_respond(mnet_h2_t *c, mnet_h2_stream_t *s, const mnet_hdrs_t *h, int end_stream);   /* servidor */
/* manda até n bytes respeitando as janelas; devolve quantos foram (0 = janela cheia) ou -1 */
long mnet_h2_send_data(mnet_h2_t *c, mnet_h2_stream_t *s, const uint8_t *d, size_t n, int end_stream);
long long mnet_h2_send_window(const mnet_h2_t *c, const mnet_h2_stream_t *s);
int mnet_h2_reset(mnet_h2_t *c, mnet_h2_stream_t *s, int code);
int mnet_h2_ping(mnet_h2_t *c, const uint8_t opaque[8]);
int mnet_h2_goaway(mnet_h2_t *c, int code);
mnet_h2_stream_t *mnet_h2_find(mnet_h2_t *c, uint32_t id);
int mnet_h2_can_open(const mnet_h2_t *c);
