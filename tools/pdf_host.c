/*
 * Teste de host do núcleo de PDF (userland/libsystem/pdf_core.c).
 *   gcc -O1 -DPDF_HOST -Iuserland/libsystem tools/pdf_host.c userland/libsystem/pdf_core.c -lz -o /tmp/pdf_host
 *   python3 tools/pdf_host.py      (gera PDFs — inclusive cifrados — e confere o que o leitor vê)
 * Uso: pdf_host info <arquivo> [senha]  → linhas "chave=valor"
 *      pdf_host hash                    → vetores de MD5/SHA-2/AES/RC4
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "pdf_core.h"

static unsigned char *slurp(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long s = ftell(f);
    fseek(f, 0, SEEK_SET);
    unsigned char *b = malloc((size_t)s + 1);
    if (fread(b, 1, (size_t)s, f) != (size_t)s) { fclose(f); free(b); return 0; }
    fclose(f);
    *n = (size_t)s;
    return b;
}
static void hex(const unsigned char *b, int n) { for (int i = 0; i < n; i++) printf("%02x", b[i]); printf("\n"); }
static void printstr(pdf_obj *o) {
    if (!o || o->type != PO_STRING) { printf("(nenhum)\n"); return; }
    for (uint32_t i = 0; i < o->n; i++) { int c = o->v.str[i]; putchar(c >= 32 && c < 127 ? c : '.'); }
    putchar('\n');
}
int main(int argc, char **argv) {
    if (argc >= 2 && !strcmp(argv[1], "hash")) {
        unsigned char o[64];
        pdf_md5((const unsigned char *)"abc", 3, o); printf("md5="); hex(o, 16);
        pdf_sha256((const unsigned char *)"abc", 3, o); printf("sha256="); hex(o, 32);
        pdf_sha384((const unsigned char *)"abc", 3, o); printf("sha384="); hex(o, 48);
        pdf_sha512((const unsigned char *)"abc", 3, o); printf("sha512="); hex(o, 64);
        /* FIPS-197 apêndice C.1 e C.3 (bloco único, IV zero = ECB) */
        unsigned char k[32], pt[16], ct[16];
        for (int i = 0; i < 32; i++) k[i] = (unsigned char)i;
        for (int i = 0; i < 16; i++) pt[i] = (unsigned char)(i * 0x11);
        unsigned char iv[16] = {0};
        pdf_aes_cbc_encrypt(k, 128, iv, pt, 16, ct); printf("aes128="); hex(ct, 16);
        pdf_aes_cbc_encrypt(k, 256, iv, pt, 16, ct); printf("aes256="); hex(ct, 16);
        size_t on;
        unsigned char back[16];
        pdf_aes_cbc_decrypt(k, 256, iv, ct, 16, back, &on, 0); printf("aes256dec="); hex(back, 16);
        unsigned char r[5] = {'P', 'l', 'a', 'i', 'n'};
        pdf_rc4((const unsigned char *)"Key", 3, r, 5); printf("rc4="); hex(r, 5);
        return 0;
    }
    if (argc < 3 || strcmp(argv[1], "info")) return 1;
    size_t n;
    unsigned char *d = slurp(argv[2], &n);
    pdf_doc *doc = d ? pdf_open(d, n) : 0;
    if (!doc) { printf("open=0\n"); return 0; }
    printf("open=1\n");
    if (argc > 3) printf("unlock=%d\n", pdf_unlock(doc, argv[3]));
    int ma, mi;
    pdf_version(doc, &ma, &mi);
    printf("version=%d.%d\nencrypted=%d\nunlocked=%d\n", ma, mi, pdf_is_encrypted(doc), pdf_is_unlocked(doc));
    printf("pages=%d\n", pdf_page_count(doc));
    printf("title=");
    printstr(pdf_dict_get(doc, pdf_info(doc), "Title"));
    for (int i = 0; i < pdf_page_count(doc); i++) {
        pdf_obj *p = pdf_page(doc, i);
        pdf_obj *mb = pdf_page_attr(doc, p, "MediaBox");
        double v[4] = {0};
        for (int k = 0; k < 4; k++) pdf_num(pdf_array_get(doc, mb, (uint32_t)k), &v[k]);
        double rot = 0;
        pdf_num(pdf_page_attr(doc, p, "Rotate"), &rot);
        unsigned char *c;
        size_t cl;
        pdf_page_contents(doc, p, &c, &cl);
        printf("page%d.box=%g %g %g %g\npage%d.rotate=%g\npage%d.content=", i, v[0], v[1], v[2], v[3], i, rot, i);
        for (size_t k = 0; k < cl && k < 60; k++) putchar(c[k] >= 32 && c[k] < 127 ? c[k] : ' ');
        putchar('\n');
        /* operadores do conteúdo, para conferir o léxico */
        pdf_lexer *L = pdf_lex_begin(doc, c, cl);
        int ops = 0, nums = 0, strs = 0;
        pdf_obj *o;
        while ((o = pdf_lex_next(L))) {
            if (o->type == PO_OP) { ops++; if (!strcmp(o->v.name, "BI")) pdf_lex_inline_image(L); }
            else if (o->type == PO_INT || o->type == PO_REAL) nums++;
            else if (o->type == PO_STRING) strs++;
        }
        pdf_lex_end(L);
        printf("page%d.ops=%d nums=%d strs=%d\n", i, ops, nums, strs);
        free(c);
    }
    pdf_close(doc);
    free(d);
    return 0;
}
