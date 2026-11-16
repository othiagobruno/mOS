/* mOS PDF — núcleo de leitura (pdf_core.c), sem CoreFoundation. ISO 32000-1/-2.
 * Objetos com arena por documento: tudo é liberado com pdf_close. */
#ifndef MOS_PDF_CORE_H
#define MOS_PDF_CORE_H
#include <stddef.h>
#include <stdint.h>

enum { PO_NULL = 0, PO_BOOL, PO_INT, PO_REAL, PO_NAME, PO_STRING, PO_ARRAY, PO_DICT, PO_STREAM, PO_REF, PO_OP };

typedef struct pdf_obj pdf_obj;
struct pdf_obj {
    uint8_t type;
    uint8_t hex;                 /* string escrita em <hex> */
    uint32_t n;                  /* tamanho (string, array, dict) */
    union {
        int b;
        int64_t i;
        double r;
        char *name;              /* sem a barra, com #xx resolvido; PO_OP: o operador */
        uint8_t *str;
        pdf_obj **arr;
        struct { char **keys; pdf_obj **vals; } dict;
        struct { pdf_obj *dict; size_t off, len; uint8_t *data; size_t dlen; int done; uint32_t num, gen; } stream;
        struct { uint32_t num, gen; } ref;
    } v;
    void *ext;                   /* o pdf_doc dono do objeto (a casca CF resolve referências por ele) */
};

typedef struct pdf_doc pdf_doc;

/* abre (cópia dos bytes); NULL se não é PDF nem dá para reparar */
pdf_doc *pdf_open(const uint8_t *d, size_t n);
void pdf_close(pdf_doc *doc);
/* criptografia: 1 se cifrado; pdf_unlock tenta a senha (de usuário ou de dono), 1 = destravou */
int pdf_is_encrypted(const pdf_doc *doc);
int pdf_is_unlocked(const pdf_doc *doc);
int pdf_unlock(pdf_doc *doc, const char *password);
uint32_t pdf_permissions(const pdf_doc *doc);       /* /P (bits de permissão) */
int pdf_version(const pdf_doc *doc, int *major, int *minor);

pdf_obj *pdf_trailer(pdf_doc *doc);
pdf_obj *pdf_catalog(pdf_doc *doc);
pdf_obj *pdf_info(pdf_doc *doc);
pdf_obj *pdf_get_object(pdf_doc *doc, uint32_t num);   /* objeto indireto (já resolvido) */
pdf_obj *pdf_resolve(pdf_doc *doc, pdf_obj *o);        /* segue referências */
pdf_obj *pdf_dict_get(pdf_doc *doc, pdf_obj *dict, const char *key);   /* resolvido; dict pode ser stream */
pdf_obj *pdf_dict_get_raw(pdf_obj *dict, const char *key);            /* sem resolver */
pdf_obj *pdf_array_get(pdf_doc *doc, pdf_obj *arr, uint32_t i);
int pdf_num(pdf_obj *o, double *out);                  /* int ou real */
const char *pdf_name(pdf_obj *o);
pdf_obj *pdf_stream_dict(pdf_obj *s);

/* dados do stream com os filtros aplicados. Filtros de imagem (DCT, JPX, JBIG2, CCITT) param a cadeia:
 * *image_filter recebe o nome ("DCTDecode"...) e os dados saem como estão até ali. Cache no próprio objeto. */
const uint8_t *pdf_stream_data(pdf_doc *doc, pdf_obj *s, size_t *len, const char **image_filter);
/* bytes crus (sem filtro, mas decifrados) */
const uint8_t *pdf_stream_raw(pdf_doc *doc, pdf_obj *s, size_t *len);

/* páginas (árvore achatada, atributos herdáveis resolvidos) */
int pdf_page_count(pdf_doc *doc);
pdf_obj *pdf_page(pdf_doc *doc, int index);            /* 0-based */
pdf_obj *pdf_page_attr(pdf_doc *doc, pdf_obj *page, const char *key);   /* com herança (Resources, MediaBox, CropBox, Rotate) */
/* conteúdo da página (os streams de /Contents juntados com espaço entre eles); malloc em *out */
int pdf_page_contents(pdf_doc *doc, pdf_obj *page, uint8_t **out, size_t *len);

/* léxico de conteúdo: devolve o próximo objeto (operando) ou operador (PO_OP); NULL no fim.
 * Os objetos saem da arena do documento (ou de uma arena temporária, ver pdf_lex_begin). */
typedef struct pdf_lexer pdf_lexer;
pdf_lexer *pdf_lex_begin(pdf_doc *doc, const uint8_t *d, size_t n);
pdf_obj *pdf_lex_next(pdf_lexer *lx);
/* imagem inline: depois do operador BI, lê o dicionário até ID e os dados até EI */
pdf_obj *pdf_lex_inline_image(pdf_lexer *lx);
size_t pdf_lex_pos(pdf_lexer *lx);
void pdf_lex_end(pdf_lexer *lx);

/* filtros soltos (o renderizador decodifica imagem inline com eles) */
int pdf_decode_filter(pdf_doc *doc, const char *filter, pdf_obj *parms, const uint8_t *in, size_t n, uint8_t **out, size_t *on);

/* cifras e resumos expostos para o teste de host */
void pdf_md5(const uint8_t *d, size_t n, uint8_t out[16]);
void pdf_sha256(const uint8_t *d, size_t n, uint8_t out[32]);
void pdf_sha384(const uint8_t *d, size_t n, uint8_t out[48]);
void pdf_sha512(const uint8_t *d, size_t n, uint8_t out[64]);
void pdf_rc4(const uint8_t *key, size_t kl, uint8_t *buf, size_t n);
int pdf_aes_cbc_decrypt(const uint8_t *key, int keybits, const uint8_t *iv, const uint8_t *in, size_t n, uint8_t *out, size_t *on, int unpad);
void pdf_aes_cbc_encrypt(const uint8_t *key, int keybits, const uint8_t *iv, const uint8_t *in, size_t n, uint8_t *out);

/* arena do documento (a casca CF e o renderizador alocam objetos/strings nela) */
void *pdf_alloc(pdf_doc *doc, size_t n);

#endif
