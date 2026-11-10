/* Host: a mesma export trie que swiftsupport.c/dladdr_trie.h percorre.
 * Não carrega VS Code, Electron nem Chromium: esses binários não estão nesta árvore e este ambiente
 * não sobe o kernel. O teste prova o parser que o dladdr usa, com um Mach-O sintético. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "../userland/libsystem/dladdr_trie.h"

static int fails;
static void expect(int cond, const char *what) {
    if (cond) return;
    fprintf(stderr, "FALHOU %s\n", what);
    fails++;
}

typedef struct { unsigned long long addr; char name[64]; } ent_t;
static ent_t ents[8];
static int nent;
static void add(void *ctx, unsigned long long addr, const char *name, int nl) {
    (void)ctx;
    if (nent >= 8 || nl >= 64) return;
    ents[nent].addr = addr;
    memcpy(ents[nent].name, name, (size_t)nl);
    ents[nent].name[nl] = 0;
    nent++;
}

static void wu32(unsigned char *p, unsigned v) { memcpy(p, &v, 4); }
static void wu64(unsigned char *p, unsigned long long v) { memcpy(p, &v, 8); }

/* Procura o símbolo de maior endereço que ainda é <= a, como dladdr em swiftsupport.c. */
static const char *nearest(unsigned long long a) {
    int best = -1;
    int i;
    for (i = 0; i < nent; i++) if (ents[i].addr <= a && (best < 0 || ents[i].addr > ents[best].addr)) best = i;
    return best < 0 ? NULL : ents[best].name;
}

int main(void) {
    unsigned char img[512];
    unsigned char trie[64];
    unsigned tlen = 0;
    unsigned tsz = 0;
    unsigned long long slide = 0;
    const unsigned char *tr;
    unsigned long long hdr;
    /* raiz: sem terminal, dois filhos _foo e _bar */
    unsigned foo_at, bar_at;
    memset(img, 0, sizeof img);
    wu32(img + 0, 0xFEEDFACFu);
    wu32(img + 16, 2);                 /* ncmds */
    wu32(img + 32, 0x19);              /* LC_SEGMENT_64 */
    wu32(img + 36, 72);
    memcpy(img + 40, "__TEXT", 6);
    wu64(img + 56, 0x100000000ull);    /* vmaddr, não pode ser 0 */
    wu64(img + 64, sizeof img);
    wu64(img + 72, 0);                 /* fileoff */
    wu64(img + 80, sizeof img);
    /* trie montada à parte e copiada para o offset 128 */
    trie[tlen++] = 0;                  /* terminal size 0 */
    trie[tlen++] = 2;                  /* dois filhos */
    memcpy(trie + tlen, "_foo", 4); tlen += 4;
    trie[tlen++] = 0;
    foo_at = tlen;
    trie[tlen++] = 0;                  /* offset preenchido depois */
    memcpy(trie + tlen, "_bar", 4); tlen += 4;
    trie[tlen++] = 0;
    bar_at = tlen;
    trie[tlen++] = 0;
    trie[foo_at] = (unsigned char)tlen;
    trie[tlen++] = 3;                  /* flags 0 + addr uleb 0x1000 */
    trie[tlen++] = 0;
    trie[tlen++] = 0x80;
    trie[tlen++] = 0x20;
    trie[tlen++] = 0;                  /* sem filhos */
    trie[bar_at] = (unsigned char)tlen;
    trie[tlen++] = 3;
    trie[tlen++] = 0;
    trie[tlen++] = 0x80;
    trie[tlen++] = 0x40;               /* 0x2000 */
    trie[tlen++] = 0;
    memcpy(img + 128, trie, tlen);
    wu32(img + 104, 0x80000033u);      /* LC_DYLD_EXPORTS_TRIE */
    wu32(img + 108, 16);
    wu32(img + 112, 128);              /* dataoff */
    wu32(img + 116, tlen);
    tr = dlt_image(img, &tsz, &slide);
    expect(tr != NULL && tsz == tlen, "dlt_image acha a trie");
    hdr = (unsigned long long)img;
    dlt_collect(tr, tr + tsz, tr, (char *)"", 0, 0, hdr, slide, hdr - slide, add, NULL);
    expect(nent == 2, "dois símbolos");
    expect(nearest(hdr + 0x1000) && !strcmp(nearest(hdr + 0x1000), "_foo"), "0x1000 é _foo");
    expect(nearest(hdr + 0x1500) && !strcmp(nearest(hdr + 0x1500), "_foo"), "entre os dois fica _foo");
    expect(nearest(hdr + 0x2000) && !strcmp(nearest(hdr + 0x2000), "_bar"), "0x2000 é _bar");
    expect(nearest(hdr + 0x100) == NULL, "antes do primeiro símbolo não há nome");
    if (fails) { fprintf(stderr, "%d falha(s)\n", fails); return 1; }
    printf("dladdr trie: ok (%d símbolos; VS Code/Electron/Chromium não estão nesta árvore)\n", nent);
    return 0;
}
