/* Teste no host do userland/libsystem/secasn1.c (rodar por tools/secasn1-hosttest.sh). */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

typedef struct { uint32_t kind, offset; const void *sub; uint32_t size; } T;
typedef struct { size_t Length; uint8_t *Data; } Item;
typedef void *Coder;
int SecAsn1CoderCreate(Coder *);
int SecAsn1CoderRelease(Coder);
int SecAsn1Decode(Coder, const void *, size_t, const T *, void *);
int SecAsn1EncodeItem(Coder, const void *, const T *, Item *);
extern const T kSecAsn1IntegerTemplate[], kSecAsn1UnsignedIntegerTemplate[], kSecAsn1UTF8StringTemplate[], kSecAsn1IA5StringTemplate[],
    kSecAsn1OctetStringTemplate[], kSecAsn1BitStringTemplate[], kSecAsn1SequenceOfIntegerTemplate[];

#define SEQ 0x10
#define SET 0x11
#define INT 0x02
#define OCT 0x04
#define OPTIONAL 0x100
#define EXPLICIT 0x200
#define INLINE 0x800
#define GROUP 0x2000
#define CHOICE 0x100000
#define CTX 0x80
#define CONS 0x20

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FALHOU linha %d: %s\n", __LINE__, #c); fails++; } } while (0)
static int item_is(const Item *i, const char *hex) {
    uint8_t b[256]; size_t n = 0;
    for (const char *p = hex; p[0] && p[1]; p += 2) { unsigned v; sscanf(p, "%2x", &v); b[n++] = (uint8_t)v; }
    return i->Length == n && (!n || !memcmp(i->Data, b, n));
}
static size_t unhex(const char *hex, uint8_t *b) {
    size_t n = 0;
    for (const char *p = hex; p[0] && p[1]; p += 2) { unsigned v; sscanf(p, "%2x", &v); b[n++] = (uint8_t)v; }
    return n;
}

/* recibo da loja: SET OF SEQUENCE { INTEGER tipo, INTEGER versão, OCTET STRING valor } */
typedef struct { Item type, version, value; } Attr;
static const T attrT[] = {{SEQ, 0, 0, sizeof(Attr)}, {INT, offsetof(Attr, type), 0, 0}, {INT, offsetof(Attr, version), 0, 0},
                          {OCT, offsetof(Attr, value), 0, 0}, {0, 0, 0, 0}};
typedef struct { Attr **attrs; } Payload;
static const T payloadT[] = {{SET | GROUP, 0, attrT, sizeof(Payload)}};

typedef struct { Item a, b; } Two;
static const T explT[] = {{SEQ, 0, 0, sizeof(Two)}, {OPTIONAL | EXPLICIT | CTX | CONS | 0, offsetof(Two, a), kSecAsn1IntegerTemplate, 0},
                          {INT, offsetof(Two, b), 0, 0}, {0, 0, 0, 0}};
static const T seqIO[] = {{SEQ, 0, 0, sizeof(Two)}, {INT, offsetof(Two, a), 0, 0}, {OCT, offsetof(Two, b), 0, 0}, {0, 0, 0, 0}};
typedef struct { int sel; Item v; } Ch;
static const T choiceT[] = {{CHOICE, offsetof(Ch, sel), 0, sizeof(Ch)}, {INT, offsetof(Ch, v), 0, 1}, {OCT, offsetof(Ch, v), 0, 2}, {0, 0, 0, 0}};
typedef struct { Item x; Ch c; } WithChoice;
static const T withChoiceT[] = {{SEQ, 0, 0, sizeof(WithChoice)}, {INT, offsetof(WithChoice, x), 0, 0}, {INLINE, offsetof(WithChoice, c), choiceT, 0}, {0, 0, 0, 0}};
/* [1] IMPLICIT OCTET STRING */
typedef struct { Item s; } Impl;
static const T implT[] = {{SEQ, 0, 0, sizeof(Impl)}, {CTX | 1, offsetof(Impl, s), kSecAsn1OctetStringTemplate, 0}, {0, 0, 0, 0}};

int main(void) {
    Coder c;
    CHECK(SecAsn1CoderCreate(&c) == 0);
    uint8_t buf[512];
    /* SET { SEQ{3,1,OCT{UTF8 "1.0"}}, SEQ{2,1,OCT{UTF8 "com.x.app"}}, SEQ{17,1,OCT{SET{SEQ{1702,1,OCT{UTF8 "p"}}}}} } (ordem DER) */
    const char *rcpt = "313e300d02010302010104050c03312e303013020102020101040b0c09636f6d2e782e61707030180201110201010410310e300c020206a602010104030c0170";
    size_t n = unhex(rcpt, buf);
    Payload p = {0};
    int r = SecAsn1Decode(c, buf, n, payloadT, &p);
    CHECK(r == 0);
    int cnt = 0;
    for (; p.attrs && p.attrs[cnt]; cnt++) {}
    CHECK(cnt == 3);
    if (cnt == 3) {
        CHECK(item_is(&p.attrs[1]->type, "02"));
        Item s = {0};
        CHECK(SecAsn1Decode(c, p.attrs[1]->value.Data, p.attrs[1]->value.Length, kSecAsn1UTF8StringTemplate, &s) == 0);
        CHECK(s.Length == 9 && !memcmp(s.Data, "com.x.app", 9));
        CHECK(item_is(&p.attrs[2]->type, "11"));
        Payload in = {0};
        CHECK(SecAsn1Decode(c, p.attrs[2]->value.Data, p.attrs[2]->value.Length, payloadT, &in) == 0);
        CHECK(in.attrs && in.attrs[0] && !in.attrs[1] && item_is(&in.attrs[0]->type, "06a6"));
        Item out = {0};
        CHECK(SecAsn1EncodeItem(c, &p, payloadT, &out) == 0);
        CHECK(out.Length == n && !memcmp(out.Data, buf, n));                   /* ida e volta idêntica */
    }
    /* BER indefinido */
    Two t = {0};
    n = unhex("308002010504036162630000", buf);
    CHECK(SecAsn1Decode(c, buf, n, seqIO, &t) == 0);
    CHECK(item_is(&t.a, "05") && t.b.Length == 3 && !memcmp(t.b.Data, "abc", 3));
    /* EXPLICIT opcional: presente e ausente */
    memset(&t, 0, sizeof t);
    n = unhex("3008a003020107020109", buf);
    CHECK(SecAsn1Decode(c, buf, n, explT, &t) == 0 && item_is(&t.a, "07") && item_is(&t.b, "09"));
    Item enc = {0};
    CHECK(SecAsn1EncodeItem(c, &t, explT, &enc) == 0 && enc.Length == n && !memcmp(enc.Data, buf, n));
    memset(&t, 0, sizeof t);
    n = unhex("3003020109", buf);
    CHECK(SecAsn1Decode(c, buf, n, explT, &t) == 0 && t.a.Length == 0 && item_is(&t.b, "09"));
    CHECK(SecAsn1EncodeItem(c, &t, explT, &enc) == 0 && enc.Length == n && !memcmp(enc.Data, buf, n));
    /* OCTET STRING construída (BER) */
    Item s = {0};
    n = unhex("2480040261620401630000", buf);
    CHECK(SecAsn1Decode(c, buf, n, kSecAsn1OctetStringTemplate, &s) == 0 && s.Length == 3 && !memcmp(s.Data, "abc", 3));
    /* BIT STRING: 1 bit */
    n = unhex("03020780", buf);
    CHECK(SecAsn1Decode(c, buf, n, kSecAsn1BitStringTemplate, &s) == 0 && s.Length == 1 && s.Data[0] == 0x80);
    CHECK(SecAsn1EncodeItem(c, &s, kSecAsn1BitStringTemplate, &enc) == 0 && item_is(&enc, "03020780"));
    /* inteiros com e sem sinal */
    n = unhex("020200ff", buf);
    CHECK(SecAsn1Decode(c, buf, n, kSecAsn1UnsignedIntegerTemplate, &s) == 0 && item_is(&s, "ff"));
    CHECK(SecAsn1EncodeItem(c, &s, kSecAsn1UnsignedIntegerTemplate, &enc) == 0 && item_is(&enc, "020200ff"));
    CHECK(SecAsn1Decode(c, buf, n, kSecAsn1IntegerTemplate, &s) == 0 && item_is(&s, "00ff"));
    /* SEQUENCE OF INTEGER */
    Item **ints = 0;
    n = unhex("3006020101020102", buf);
    CHECK(SecAsn1Decode(c, buf, n, kSecAsn1SequenceOfIntegerTemplate, &ints) == 0 && ints && ints[0] && ints[1] && !ints[2] && item_is(ints[1], "02"));
    /* CHOICE dentro de SEQUENCE */
    WithChoice w = {0};
    n = unhex("3007020101040261" "62", buf);
    CHECK(SecAsn1Decode(c, buf, n, withChoiceT, &w) == 0 && w.c.sel == 2 && w.c.v.Length == 2);
    CHECK(SecAsn1EncodeItem(c, &w, withChoiceT, &enc) == 0 && enc.Length == n && !memcmp(enc.Data, buf, n));
    /* [1] IMPLICIT OCTET STRING */
    Impl im = {0};
    n = unhex("3004810278" "79", buf);
    CHECK(SecAsn1Decode(c, buf, n, implT, &im) == 0 && im.s.Length == 2 && !memcmp(im.s.Data, "xy", 2));
    CHECK(SecAsn1EncodeItem(c, &im, implT, &enc) == 0 && enc.Length == n && !memcmp(enc.Data, buf, n));
    /* erros: truncado e tag errada */
    n = unhex("30080201", buf);
    CHECK(SecAsn1Decode(c, buf, n, seqIO, &t) == -26275);
    n = unhex("0401aa", buf);
    CHECK(SecAsn1Decode(c, buf, n, kSecAsn1IA5StringTemplate, &s) == -26275);
    CHECK(SecAsn1CoderRelease(c) == 0);
    printf(fails ? "secasn1: %d falha(s)\n" : "secasn1: ok\n", fails);
    return fails != 0;
}
