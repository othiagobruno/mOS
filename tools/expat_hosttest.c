/*
 * mOS — teste de host da expat vendorizada (userland/third_party/expat/expat_config.h + fonte em .tools).
 * Compila os 3 .c da expat para o host e analisa um XML, conferindo os handlers de elemento, atributo e texto.
 * Rodar: tools/expat_hosttest.sh
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <expat.h>

/* a expat (config do mOS, HAVE_ARC4RANDOM_BUF) chama arc4random_buf para semear o hash; o host (glibc) não
 * tem o símbolo, então o fornecemos aqui — qualquer entropia serve para o teste. */
void arc4random_buf(void *buf, size_t n) {
    unsigned char *p = buf;
    for (size_t i = 0; i < n; i++) p[i] = (unsigned char)(rand() & 0xff);
}

static int starts = 0, ends = 0, fails = 0, total = 0;
static char text[256]; static int tlen = 0;
static char first_el[32]; static char attr_val[32];

#define CK(cond, msg) do { total++; if (!(cond)) { fails++; printf("FALHA: %s\n", msg); } } while (0)

static void XMLCALL on_start(void *u, const XML_Char *name, const XML_Char **atts) {
    (void)u;
    if (starts == 0) { strncpy(first_el, name, sizeof first_el - 1); }
    for (int i = 0; atts[i]; i += 2)
        if (strcmp(atts[i], "id") == 0) strncpy(attr_val, atts[i + 1], sizeof attr_val - 1);
    starts++;
}
static void XMLCALL on_end(void *u, const XML_Char *name) { (void)u; (void)name; ends++; }
static void XMLCALL on_text(void *u, const XML_Char *s, int len) {
    (void)u;
    for (int i = 0; i < len && tlen < (int)sizeof text - 1; i++)
        if (s[i] != '\n' && s[i] != ' ') text[tlen++] = s[i];
}

int main(void) {
    const char *xml = "<root id='42'><child>Hello</child><child>World</child></root>";
    XML_Parser p = XML_ParserCreate(NULL);
    CK(p != NULL, "XML_ParserCreate");
    XML_SetElementHandler(p, on_start, on_end);
    XML_SetCharacterDataHandler(p, on_text);
    enum XML_Status st = XML_Parse(p, xml, (int)strlen(xml), 1);
    CK(st == XML_STATUS_OK, "XML_Parse OK");
    CK(starts == 3 && ends == 3, "3 elementos abertos e fechados");
    CK(strcmp(first_el, "root") == 0, "primeiro elemento = root");
    CK(strcmp(attr_val, "42") == 0, "atributo id = 42");
    text[tlen] = 0;
    CK(strcmp(text, "HelloWorld") == 0, "texto concatenado");

    /* erro: XML malformado deve ser detectado */
    XML_Parser q = XML_ParserCreate(NULL);
    const char *bad = "<root><unclosed></root>";
    XML_Parse(q, bad, (int)strlen(bad), 1);
    CK(XML_GetErrorCode(q) != XML_ERROR_NONE, "XML malformado -> erro");
    const XML_LChar *msg = XML_ErrorString(XML_GetErrorCode(q));
    CK(msg != NULL && msg[0] != 0, "XML_ErrorString não vazia");
    XML_ParserFree(q);
    XML_ParserFree(p);

    printf("%d/%d ok\n", total - fails, total);
    return fails ? 1 : 0;
}
