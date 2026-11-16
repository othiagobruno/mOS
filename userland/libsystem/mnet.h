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
    int (*on_head)(void *ctx, struct mnet_h1 *p);                          /* cabeça final pronta, antes do corpo; != 0 aborta */
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

/* ---- Content-Encoding (gzip, deflate, br, em cadeia). NULL = codificação desconhecida ou ausente. */
typedef struct mnet_dec mnet_dec_t;
mnet_dec_t *mnet_dec_new(const char *content_encoding, int (*sink)(void *, const uint8_t *, size_t), void *ctx);
int mnet_dec_feed(mnet_dec_t *d, const uint8_t *in, size_t n);
int mnet_dec_finish(mnet_dec_t *d);
const char *mnet_dec_error(const mnet_dec_t *d);
void mnet_dec_free(mnet_dec_t *d);

/* ---- URL (RFC 3986): partes e resolução de referência relativa (Location, redirects) */
typedef struct {
    char scheme[16];
    char host[256];          /* sem colchetes; IPv6 literal fica "::1" */
    int port;                /* explícita ou a padrão do esquema; 0 se desconhecida */
    int port_explicit, ipv6;
    char *target;            /* caminho + "?" consulta (malloc), sem fragmento; "/" se vazio */
    char *userinfo;          /* "usuario:senha" ou NULL */
} mnet_url_t;
int mnet_url_parse(const char *url, mnet_url_t *u);      /* 0 ok, -1 URL inválida */
void mnet_url_free(mnet_url_t *u);
char *mnet_url_resolve(const char *base, const char *ref); /* malloc; NULL se inválida */
int mnet_default_port(const char *scheme);
char *mnet_url_authority(const mnet_url_t *u);           /* "host[:porta]" para Host/:authority (malloc) */

/* ---- cliente HTTP (NSURLSession, CFNetwork, libcurl usam isto). Erros = códigos de NSURLErrorDomain. */
enum { MNET_E_UNKNOWN = -1, MNET_E_CANCELLED = -999, MNET_E_BAD_URL = -1000, MNET_E_TIMEOUT = -1001,
       MNET_E_UNSUPPORTED = -1002, MNET_E_NO_HOST = -1003, MNET_E_CONNECT = -1004, MNET_E_LOST = -1005,
       MNET_E_REDIRECTS = -1007, MNET_E_OFFLINE = -1009, MNET_E_BAD_RESPONSE = -1011, MNET_E_DECODE = -1015,
       MNET_E_PARSE = -1017, MNET_E_TLS = -1200, MNET_E_CERT_UNTRUSTED = -1202, MNET_E_PROXY = -1004 };
/* transporte da plataforma (sockets + TLS). Tempos em ms; read devolve >0 bytes, 0 fim, -1 erro, -2 tempo. */
typedef struct mnet_io {
    void *env;
    void *(*connect)(void *env, const char *host, int port, int timeout_ms, int *err);
    int (*starttls)(void *env, void *conn, const char *sni, const uint8_t *alpn, size_t alpn_len, char alpn_out[33],
                    int timeout_ms, char *msg, size_t msglen);    /* 0 ou MNET_E_TLS/MNET_E_CERT_UNTRUSTED/... */
    long (*read)(void *env, void *conn, void *buf, size_t n, int timeout_ms);
    long (*write)(void *env, void *conn, const void *buf, size_t n, int timeout_ms);
    void (*close)(void *env, void *conn);
    long long (*now_ms)(void *env);                 /* relógio monotônico */
    long long (*unix_time)(void *env);              /* segundos Unix (cookies) */
    void (*lock)(void *env);                        /* protege o pool (NULL = um só fio) */
    void (*unlock)(void *env);
    void *(*copy_trust)(void *env, void *conn);     /* SecTrustRef da cadeia do servidor (+1) ou NULL */
    /* opcional: espera a conexão ficar legível (ev 1) ou gravável (ev 4); 1 pronto (ou dados já no TLS), 0 tempo, -1 erro */
    int (*wait)(void *env, void *conn, int ev, int timeout_ms);
} mnet_io_t;
typedef struct mnet_pool_ent mnet_pool_ent_t;
typedef struct {
    const mnet_io_t *io;
    mnet_pool_ent_t *idle;
    int idle_count, max_idle;
    int idle_timeout_ms;
    mnet_jar_t *jar;                                /* cookies compartilhados (NSHTTPCookieStorage) ou NULL */
    char user_agent[160];
} mnet_client_t;
typedef struct {
    int status, version;                            /* version: 10, 11 ou 20 */
    mnet_hdrs_t hdrs;
    char *url;                                      /* URL final (depois dos redirects) */
    long long expected_len;                         /* Content-Length ou -1 */
    char alpn[33];
    int redirects;
    int reused;                                     /* conexão reaproveitada */
} mnet_resp_t;
typedef struct {
    const char *method, *url;
    const mnet_hdrs_t *hdrs;
    const uint8_t *body; size_t body_len;           /* corpo em memória, ou: */
    long (*body_read)(void *ctx, uint8_t *buf, size_t n);   /* corpo em fluxo (0 = fim, <0 erro) */
    int (*body_rewind)(void *ctx);                  /* para repetir depois de redirect 307/308 ou nova conexão */
    long long body_stream_len;                      /* -1 = desconhecido (chunked no 1.1) */
    int timeout_ms;                                 /* sem dados por este tempo -> MNET_E_TIMEOUT (0 = 60 s) */
    int total_timeout_ms;                           /* pedido inteiro (0 = sem limite) */
    int max_redirects;                              /* 0 = 16; -1 = não segue */
    int no_cookies, no_decode, http1_only, no_reuse;
    const char *proxy;                              /* "http://h:p", "https://h:p", "socks5://h:p" ou NULL */
    const char *proxy_auth;                         /* valor de Proxy-Authorization ou usuário:senha do SOCKS */
    void *ctx;
    int (*on_response)(void *ctx, const mnet_resp_t *r);           /* 0 segue; != 0 cancela */
    int (*on_body)(void *ctx, const uint8_t *d, size_t n);         /* != 0 cancela */
    /* redirect: devolve 1 para seguir (pode trocar *url), 0 para entregar o 3xx como resposta final */
    int (*on_redirect)(void *ctx, const mnet_resp_t *r, char **url);
    void (*on_sent)(void *ctx, long long sent, long long total);
    volatile int *cancel;                           /* != 0: cancela no próximo ponto de E/S */
    mnet_jar_t *jar;                                /* jar deste pedido (sessão efêmera); NULL = o do cliente */
    int no_store_cookies;                           /* manda cookies, mas não guarda os Set-Cookie (política "nunca") */
    int bare_headers;                               /* não acrescenta User-Agent/Accept-Language padrão (libcurl) */
    /* conexão TLS nova: trust = SecTrustRef (pode ser NULL); != 0 cancela (desafio de servidor recusado) */
    int (*on_tls)(void *ctx, const char *host, int port, void *trust);
    void (*release_trust)(void *trust);
} mnet_req_t;
void mnet_client_init(mnet_client_t *c, const mnet_io_t *io);
void mnet_client_close_idle(mnet_client_t *c);
/* 0 ok (resposta em *r, corpo já entregue por on_body) ou MNET_E_*; msg recebe o texto do erro */
int mnet_fetch(mnet_client_t *c, const mnet_req_t *q, mnet_resp_t *r, char *msg, size_t msglen);
void mnet_resp_free(mnet_resp_t *r);

/* libSystem (urlsession_io.c): transporte do mOS e o cliente/jar compartilhados do processo */
const mnet_io_t *cx_mnet_io(void);
void *cx_mnet_conn_from_fd(int fd, int close_fd);   /* socket nativo do app -> conexão do transporte */
int cx_mnet_conn_fd(void *conn);
void cx_mnet_conn_set_close(void *conn, int close_fd);
void *cx_mnet_conn_ssl(void *conn);                 /* SSLContextRef da conexão (kCFStreamPropertySSLContext) */
mnet_client_t *cx_mnet_client(void);
mnet_jar_t *cx_mnet_jar(void);

/* ---- cache HTTP privado (RFC 9111): só as regras; o armazenamento é do NSURLCache */
int mnet_cache_storable(const char *method, int status, const mnet_hdrs_t *req, const mnet_hdrs_t *resp);
long long mnet_cache_fresh_left(const mnet_hdrs_t *resp, int status, long long req_time, long long resp_time, long long now);
int mnet_cache_req_revalidate(const mnet_hdrs_t *req);
int mnet_cache_must_revalidate(const mnet_hdrs_t *resp);
int mnet_cache_validators(const mnet_hdrs_t *stored, mnet_hdrs_t *req);
void mnet_cache_merge_304(mnet_hdrs_t *stored, const mnet_hdrs_t *r304);
char *mnet_cache_vary_key(const mnet_hdrs_t *resp, const mnet_hdrs_t *req);

/* ---- SHA-1 (só para o Sec-WebSocket-Accept) e base64 */
void mnet_sha1(const void *d, size_t n, uint8_t out[20]);
size_t mnet_b64(const uint8_t *d, size_t n, char *out);              /* out: 4*ceil(n/3)+1 */

/* ---- WebSocket (RFC 6455) com permessage-deflate (RFC 7692). Sem E/S: feed recebe, out guarda o que enviar. */
enum { MNET_WS_CONT = 0, MNET_WS_TEXT = 1, MNET_WS_BINARY = 2, MNET_WS_CLOSE = 8, MNET_WS_PING = 9, MNET_WS_PONG = 10 };
typedef struct mnet_ws {
    int is_server;
    /* permessage-deflate negociado */
    int deflate, my_no_ctx, peer_no_ctx, my_bits, peer_bits;
    void *zin, *zout;
    mnet_buf_t in, msg, out;
    int msg_op, msg_rsv1, in_msg;
    int close_sent, close_recv, close_code;
    char close_reason[124];
    size_t max_msg;                    /* 0 = 64 MiB */
    const char *err;
    void *ctx;
    int (*on_message)(void *ctx, int opcode, const uint8_t *d, size_t n);   /* != 0 aborta (1011) */
    void (*on_ping)(void *ctx, const uint8_t *d, size_t n);                 /* o pong já é enfileirado */
    void (*on_pong)(void *ctx, const uint8_t *d, size_t n);
    void (*on_close)(void *ctx, int code, const char *reason);
    void (*random)(void *ctx, uint8_t *b, size_t n);                        /* máscara do cliente */
} mnet_ws_t;
void mnet_ws_key(const uint8_t rnd[16], char out[25]);
void mnet_ws_accept(const char *key, char out[29]);
/* pedido de upgrade do cliente; protocols = "chat, superchat" ou NULL */
int mnet_ws_request(mnet_buf_t *o, const char *target, const char *host, const char *origin, const char *key,
                    const char *protocols, const mnet_hdrs_t *extra, int offer_deflate);
/* confere o 101 e aplica a extensão negociada; *proto recebe o subprotocolo (malloc) ou NULL; 0 ok, -1 recusado */
int mnet_ws_client_accept(mnet_ws_t *ws, const mnet_h1_t *resp, const char *key, const char *protocols, char **proto, const char **why);
/* lado servidor: valida o pedido (cabeçalhos) e monta a resposta 101 escolhendo subprotocolo e deflate */
int mnet_ws_server_accept(mnet_ws_t *ws, const mnet_hdrs_t *req, const char *const *supported_protocols, mnet_buf_t *resp, char **proto);
int mnet_ws_init(mnet_ws_t *ws, int is_server);
void mnet_ws_free(mnet_ws_t *ws);
int mnet_ws_send(mnet_ws_t *ws, int opcode, const void *d, size_t n);       /* mensagem inteira (texto/binário) */
int mnet_ws_ping(mnet_ws_t *ws, const void *d, size_t n);
int mnet_ws_close(mnet_ws_t *ws, int code, const char *reason);
long mnet_ws_feed(mnet_ws_t *ws, const uint8_t *d, size_t n);                /* -1: erro, o close já foi enfileirado */
int mnet_utf8_valid(const uint8_t *d, size_t n);

/* ---- WebSocket cliente sobre o transporte (ws:// e wss://, cookies, proxy CONNECT/SOCKS, tempo) */
typedef struct {
    mnet_client_t *c;
    void *conn;
    mnet_ws_t ws;
    char *protocol;          /* subprotocolo aceito (malloc) */
    mnet_resp_t resp;        /* a resposta 101 (ou o erro do servidor) */
} mnet_wsconn_t;
/* q->url é ws:// ou wss://; protocols separados por vírgula. 0 ok (w pronto) ou MNET_E_*; w->resp guarda a resposta */
int mnet_ws_open(mnet_client_t *c, const mnet_req_t *q, const char *protocols, mnet_wsconn_t *w, char *msg, size_t msglen);
int mnet_wsconn_flush(mnet_wsconn_t *w, int timeout_ms);       /* manda w->ws.out; 0 ok, -1 erro, -2 tempo */
long mnet_wsconn_poll(mnet_wsconn_t *w, int timeout_ms);       /* lê uma vez e alimenta: >0 bytes, 0 nada (tempo), -1 fim/erro */
void mnet_wsconn_free(mnet_wsconn_t *w);

/* ---- proxy (regras do CFNetwork): qual proxy usar para uma URL */
enum { MNET_PX_NONE, MNET_PX_HTTP, MNET_PX_HTTPS, MNET_PX_SOCKS, MNET_PX_PAC_URL, MNET_PX_PAC_JS };
typedef struct {
    int http_on, https_on, socks_on, pac_on, exclude_simple;
    char http_host[256], https_host[256], socks_host[256];
    int http_port, https_port, socks_port;
    char *pac_url, *pac_js;          /* malloc */
    char **exceptions; int nexc;     /* "*.local", "169.254/16", "exemplo.com", "<local>" */
} mnet_proxy_cfg_t;
typedef struct { int type; char host[256]; int port; } mnet_proxy_t;
void mnet_proxy_cfg_free(mnet_proxy_cfg_t *c);
int mnet_proxy_add_exception(mnet_proxy_cfg_t *c, const char *pattern);
int mnet_proxy_bypass(const mnet_proxy_cfg_t *c, const char *host);   /* 1 = conexão direta */
/* lista em ordem de preferência; sempre termina em MNET_PX_NONE (direto). Devolve a quantidade. */
int mnet_proxy_for_url(const mnet_proxy_cfg_t *c, const char *url, mnet_proxy_t *out, int max);
/* resultado de um PAC ("PROXY a:1; SOCKS b:2; DIRECT") -> lista; devolve a quantidade */
int mnet_proxy_parse_pac_result(const char *s, mnet_proxy_t *out, int max);

/* ---- autenticação HTTP (RFC 7617 Basic, RFC 7616 Digest MD5/SHA-256/-sess, qop=auth) */
/* escolhe o melhor desafio dos cabeçalhos (www-authenticate ou proxy-authenticate): 1 Basic, 2 Digest MD5, 3 Digest SHA-256 */
/* parâmetro de um desafio ("Digest realm=\"x\", nonce=..."): 1 achou */
int mnet_auth_param(const char *challenge, const char *name, char *out, size_t n);
const char *mnet_auth_pick(const mnet_hdrs_t *h, const char *field, int *score);
/* valor de Authorization para o desafio; cnonce de 16 bytes aleatórios (NULL = gerado com rnd) */
char *mnet_auth_value(const char *challenge, const char *user, const char *pass, const char *method, const char *uri,
                      int nc, const uint8_t cnonce_raw[16]);
void mnet_md5(const void *d, size_t n, uint8_t out[16]);
void mnet_sha256(const void *d, size_t n, uint8_t out[32]);

/* ---- streams do CFStream/NSStream (mnet_stream.c): memória, arquivo, par ligado e socket TCP com SOCKS/TLS */
enum { MNET_SS_NOT_OPEN, MNET_SS_OPENING, MNET_SS_OPEN, MNET_SS_READING, MNET_SS_WRITING, MNET_SS_AT_END,
       MNET_SS_CLOSED, MNET_SS_ERROR };                                      /* = CFStreamStatus */
enum { MNET_SE_OPEN = 1, MNET_SE_BYTES = 2, MNET_SE_SPACE = 4, MNET_SE_ERROR = 8, MNET_SE_END = 16 };   /* = CFStreamEventType */
enum { MNET_SD_POSIX = 1, MNET_SD_OSSTATUS = 2, MNET_SD_SSL = 3, MNET_SD_HTTP = 4, MNET_SD_SOCKS = 5,
       MNET_SD_NETDB = 12 };                                                 /* = kCFStreamErrorDomain* */
typedef struct mnet_stream mnet_stream_t;
typedef struct {
    int tls;                                     /* kCFStreamPropertySSLSettings / SocketSecurityLevel */
    char peer_name[256];                         /* kCFStreamSSLPeerName (SNI e verificação) */
    char socks_host[256]; int socks_port, socks_version;   /* kCFStreamPropertySOCKSProxy: 4 ou 5 */
    char socks_user[128], socks_pass[128];
    int timeout_ms;
} mnet_sockopts_t;
mnet_stream_t *mnet_stream_mem_in(const void *d, size_t n);                  /* copia os bytes */
mnet_stream_t *mnet_stream_file_in(const char *path);
mnet_stream_t *mnet_stream_mem_out(void);
mnet_stream_t *mnet_stream_buf_out(void *buf, size_t cap);
mnet_stream_t *mnet_stream_file_out(const char *path, int append);
int mnet_stream_pair(size_t cap, mnet_stream_t **rd, mnet_stream_t **wr);
int mnet_stream_socket(const mnet_io_t *io, const char *host, int port, mnet_stream_t **rd, mnet_stream_t **wr);
int mnet_stream_socket_conn(const mnet_io_t *io, void *conn, int own, mnet_stream_t **rd, mnet_stream_t **wr);
int mnet_stream_open(mnet_stream_t *s);                                      /* socket: conecta em segundo plano */
void mnet_stream_close(mnet_stream_t *s);
void mnet_stream_free(mnet_stream_t *s);
long mnet_stream_read(mnet_stream_t *s, uint8_t *b, size_t n);               /* bloqueia como o CFReadStreamRead */
long mnet_stream_write(mnet_stream_t *s, const uint8_t *b, size_t n);
int mnet_stream_status(mnet_stream_t *s);
int mnet_stream_has_bytes(mnet_stream_t *s);
int mnet_stream_has_space(mnet_stream_t *s);
int mnet_stream_error(mnet_stream_t *s, long *code, const char **msg);       /* domínio (0 = sem erro) */
unsigned mnet_stream_events(mnet_stream_t *s);                               /* eventos a entregar agora */
/* notify: o estado mudou (qualquer fio, com travas internas presas: só sinalize, não chame o stream) */
void mnet_stream_set_notify(mnet_stream_t *s, void (*fn)(void *), void *ctx);
void mnet_stream_set_on_open(mnet_stream_t *s, void (*fn)(void *, mnet_stream_t *), void *ctx);
int mnet_stream_get_buffer(mnet_stream_t *s, uint8_t **p, size_t *n);
const uint8_t *mnet_stream_mem_data(mnet_stream_t *s, size_t *n);            /* DataWrittenToMemoryStream */
long long mnet_stream_offset(mnet_stream_t *s);                              /* FileCurrentOffset */
int mnet_stream_seek(mnet_stream_t *s, long long off);
int mnet_stream_set_append(mnet_stream_t *s, int append);                   /* saída em arquivo, antes do open */
int mnet_stream_is_socket(const mnet_stream_t *s);
int mnet_stream_is_input(const mnet_stream_t *s);
mnet_sockopts_t *mnet_stream_sockopts(mnet_stream_t *s);                     /* mudar antes do open */
void *mnet_stream_conn(mnet_stream_t *s);
int mnet_stream_tls_active(mnet_stream_t *s);
int mnet_stream_starttls(mnet_stream_t *s);                                  /* TLS depois do open */
void mnet_stream_fail(mnet_stream_t *s, int domain, long code, const char *msg);
void mnet_stream_fail_peer(mnet_stream_t *w, int domain, long code, const char *msg);   /* erro no leitor do par */
int mnet_stream_peer_gone(mnet_stream_t *w);                                 /* leitor do par fechou ou saiu */

/* ---- mensagem HTTP solta (CFHTTPMessage), mnet_msg.c */
typedef struct {
    int is_request, ver_major, ver_minor, status;
    char *method, *target, *reason;          /* target: URL do pedido montado ou o alvo lido da linha */
    mnet_hdrs_t hdrs;
    mnet_buf_t body;
    mnet_buf_t in;                           /* cabeça parcial (AppendBytes) */
    int head_done, bad;
} mnet_msg_t;
int mnet_msg_parse_version(const char *v, int *maj, int *min);
int mnet_msg_init_request(mnet_msg_t *m, const char *method, const char *url, const char *version);
int mnet_msg_init_response(mnet_msg_t *m, int status, const char *reason, const char *version);
void mnet_msg_init_empty(mnet_msg_t *m, int is_request);
int mnet_msg_copy(mnet_msg_t *dst, const mnet_msg_t *src);
void mnet_msg_free(mnet_msg_t *m);
int mnet_msg_append(mnet_msg_t *m, const uint8_t *d, size_t n);             /* -1: mensagem inválida */
int mnet_msg_version(const mnet_msg_t *m, char out[12]);                     /* "HTTP/1.1" */
char *mnet_msg_request_target(const mnet_msg_t *m);                          /* malloc */
int mnet_msg_serialize(const mnet_msg_t *m, mnet_buf_t *out);
const char *mnet_status_text(int status);

/* ---- DNS reverso (mnet_util.c): nome PTR de um sockaddr do macOS e o primeiro PTR de uma resposta */
int mnet_dns_ptr_name(const uint8_t *sa, size_t n, char *q, size_t qn);
int mnet_dns_first_ptr(const uint8_t *msg, size_t len, char *out, size_t on);
