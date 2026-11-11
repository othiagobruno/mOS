/* Teste de host do que o ChatGPT desktop pedia e o mOS passou a implementar (10 nov. 2026):
 *   1. predicado do Spotlight (mdq_core.c): operadores, texto, curingas, modificadores c/d/w, atributo multivalor;
 *   2. chave privada P-256 em PEM (ck_pem.c): PKCS#8 e SEC1 válidos, recusa de PEM cifrado / de outra curva;
 *   3. inversa de CATransform3D (catransform_inv.c): A*inv(A) = I; matriz singular devolve a original.
 *
 *   gcc -std=c11 -Wall -Wextra -I userland/libsystem userland/libsystem/mdq_core.c tools/agentes_host.c \
 *       -DCK_PEM_HOST userland/libsystem/ck_pem.c userland/libsystem/catransform_inv.c -o /tmp/agentes-host
 */
#include "mdq_core.h"
#include <stdio.h>
#include <string.h>

int ck_p256_pem_der(const char *pem, long n, unsigned char out[32]);
typedef struct { double m[16]; } T3;
T3 CATransform3DInvert(T3 t);

static int ok, total;
static void check(int cond, const char *what) { total++; if (cond) ok++; printf("%s %s\n", cond ? "  ok  " : "  FALHA", what); }

/* item de teste: atributos por nome, com vários valores */
typedef struct { const char *attr; const char *vals[6]; } kv;
static const kv ITEM[] = {
    {"kMDItemFSName", {"Claude.app", 0}},
    {"kMDItemDisplayName", {"Claude", 0}},
    {"kMDItemCFBundleIdentifier", {"com.anthropic.claudefordesktop", 0}},
    {"kMDItemContentType", {"com.apple.application-bundle", 0}},
    {"kMDItemContentTypeTree", {"com.apple.application-bundle", "com.apple.application", "public.item", 0}},
    {"kMDItemFSSize", {"12345678", 0}},
    {0, {0}},
};
static const char *getv(void *ctx, const char *attr, int i) {
    (void)ctx;
    for (int k = 0; ITEM[k].attr; k++) if (!strcmp(ITEM[k].attr, attr)) return ITEM[k].vals[i];
    return 0;
}
static int q(const char *s) {
    char err[96];
    mdq_node *n = mdq_parse(s, err, sizeof err);
    if (!n) return -1;
    int r = mdq_eval(n, getv, 0);
    mdq_free(n);
    return r;
}

/* chave P-256 de teste (openssl ecparam -genkey -name prime256v1, PKCS#8 e SEC1); valores fixos de exemplo */
static const char *PKCS8 =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIGHAgEAMBMGByqGSM49AgEGCCqGSM49AwEHBG0wawIBAQQgevZzL1gdAFr88hb2\n"
    "OF/2NxApJCzGCEDdfSp6VQO30hyhRANCAAQRWz+jn65BtOMvdyHKcvjBeBSDZH2r\n"
    "1RTwjmYSi9R/zpBnuQ4EiMnCqfMPWiZqB4QdbAd0E7oH50VpuZ1P087G\n"
    "-----END PRIVATE KEY-----\n";
static const char *SEC1 =
    "-----BEGIN EC PRIVATE KEY-----\n"
    "MHcCAQEEIHryGYgGUK+4qvYTGgpL+Q0bYXRYqVkTVZ4kJ9F0r1DVoAoGCCqGSM49\n"
    "AwEHoUQDQgAEEVs/o5+uQbTjL3chynL4wXgUg2R9q9UU8I5mEovUf86QZ7kOBIjJ\n"
    "wqnzD1omageEHWwHdBO6B+dFabmdT9POxg==\n"
    "-----END EC PRIVATE KEY-----\n";
/* chave de curva maior (secp384r1): tem de ser recusada */
static const char *P384 =
    "-----BEGIN PRIVATE KEY-----\n"
    "MIG2AgEAMBAGByqGSM49AgEGBSuBBAAiBIGeMIGbAgEBBDCV\n"
    "-----END PRIVATE KEY-----\n";

int main(void) {
    unsigned char d[32];

    printf("1. predicado do Spotlight\n");
    check(q("kMDItemFSName == \"Claude.app\"") == 1, "igualdade exata");
    check(q("kMDItemFSName == \"claude.app\"") == 0, "sem c, a caixa importa");
    check(q("kMDItemFSName == \"claude.app\"c") == 1, "com c, ignora a caixa");
    check(q("kMDItemFSName == \"Claude*\"") == 1, "curinga no fim");
    check(q("kMDItemDisplayName == \"*ud*\"") == 1, "curinga dos dois lados");
    check(q("kMDItemCFBundleIdentifier == \"com.anthropic.*\"") == 1, "prefixo do bundle id");
    check(q("kMDItemContentTypeTree == \"public.item\"") == 1, "atributo multivalor casa num valor");
    check(q("kMDItemContentTypeTree == \"public.image\"") == 0, "multivalor não casa onde não está");
    check(q("kMDItemContentTypeTree != \"public.image\"") == 1, "!= num multivalor: nenhum valor casa");
    check(q("kMDItemContentTypeTree != \"public.item\"") == 0, "!= falha quando um valor casa");
    check(q("kMDItemFSSize > 1000000") == 1, "comparação numérica maior");
    check(q("kMDItemFSSize < 1000") == 0, "comparação numérica menor");
    check(q("kMDItemFSName == \"x\" || kMDItemDisplayName == \"Claude\"") == 1, "||");
    check(q("kMDItemDisplayName == \"Claude\" && kMDItemFSSize > 0") == 1, "&&");
    check(q("!(kMDItemFSName == \"outro\")") == 1, "negação");
    check(q("\"Claude\"") == 1, "texto solto casa o nome");
    check(q("\"claude\"wc == ") == -1 || 1, "consulta malformada não derruba");
    check(q("$time.now(-60)") == -1, "variável de tempo: recusada (consulta inválida)");
    check(q("kMDItemFSName ==") == -1, "operador sem valor: inválida");

    printf("2. chave P-256 em PEM\n");
    check(ck_p256_pem_der(PKCS8, (long)strlen(PKCS8), d) == 0, "PKCS#8 (BEGIN PRIVATE KEY) decodifica");
    check(ck_p256_pem_der(SEC1, (long)strlen(SEC1), d) == 0, "SEC1 (BEGIN EC PRIVATE KEY) decodifica");
    check(ck_p256_pem_der("não é pem", 9, d) == -1, "texto que não é PEM é recusado");
    check(ck_p256_pem_der(P384, (long)strlen(P384), d) == -1, "curva diferente de P-256 é recusada");
    { char enc[512]; int n = snprintf(enc, sizeof enc,
        "-----BEGIN EC PRIVATE KEY-----\nProc-Type: 4,ENCRYPTED\nDEK-Info: AES-128-CBC,0\n\nAAAA\n-----END EC PRIVATE KEY-----\n");
      check(ck_p256_pem_der(enc, n, d) == -1, "PEM cifrado (DEK-Info) é recusado"); }

    printf("3. CATransform3DInvert\n");
    T3 a = {{2, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 3, 4, 5, 1}};   /* escala x2 + translação */
    T3 inv = CATransform3DInvert(a);
    double prod[16];
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) { double s = 0; for (int k = 0; k < 4; k++) s += a.m[i*4+k]*inv.m[k*4+j]; prod[i*4+j] = s; }
    int ident = 1;
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) { double e = i==j?1:0; if (prod[i*4+j] < e-1e-9 || prod[i*4+j] > e+1e-9) ident = 0; }
    check(ident, "A * inv(A) = I");
    T3 sing = {{0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}};
    T3 si = CATransform3DInvert(sing);
    check(!memcmp(&si, &sing, sizeof si), "matriz singular: devolve a original");

    printf("%d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
