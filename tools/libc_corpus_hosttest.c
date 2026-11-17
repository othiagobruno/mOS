#define _GNU_SOURCE
/*
 * mOS — teste de host de userland/libsystem/libc_corpus.c (entra com prefixo mos_).
 * Wide: compara com a glibc (wcscasecmp/wcsncasecmp/wcsncat/wcscspn); wcslcpy/wcslcat com valores à mão.
 * Mach-O: monta um mach_header_64 e um fat na memória e confere seção/troca de bytes. pthread: caminhos de sucesso.
 * Rodar: tools/libc_corpus_hosttest.sh
 */
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <stdint.h>

/* --- símbolos do objeto do mOS (prefixados) --- */
int mos_wcscasecmp(const wchar_t *, const wchar_t *);
int mos_wcsncasecmp(const wchar_t *, const wchar_t *, size_t);
wchar_t *mos_wcsncat(wchar_t *, const wchar_t *, size_t);
size_t mos_wcscspn(const wchar_t *, const wchar_t *);
size_t mos_wcslcpy(wchar_t *, const wchar_t *, size_t);
size_t mos_wcslcat(wchar_t *, const wchar_t *, size_t);
int mos_pthread_attr_getschedpolicy(const void *, int *);
int mos_pthread_rwlockattr_init(void *);
int mos_pthread_rwlockattr_destroy(void *);
int mos_pthread_key_init_np(int, void (*)(void *));
typedef struct { char sectname[16], segname[16]; uint64_t addr, size; uint32_t offset, align, reloff, nreloc, flags, r1, r2, r3; } sect64;
const sect64 *mos_getsectbynamefromheader_64(const void *, const char *, const char *);
uint8_t *mos_getsectdatafromheader_64(const void *, const char *, const char *, uint64_t *);
void mos_swap_fat_header(void *, int);
void mos_swap_fat_arch(void *, uint32_t, int);

static int fails = 0, total = 0;
#define CK(cond, msg) do { total++; if (!(cond)) { fails++; printf("FALHA: %s\n", msg); } } while (0)

/* sinal normalizado para comparar com a glibc (que pode devolver diferenças grandes) */
static int sgn(int x) { return x < 0 ? -1 : x > 0 ? 1 : 0; }

int main(void) {
    /* ---- wide vs glibc ---- */
    const wchar_t *pares[][2] = {
        {L"Hello", L"hello"}, {L"ABC", L"abd"}, {L"same", L"same"},
        {L"", L""}, {L"a", L""}, {L"", L"a"}, {L"Test123", L"test123"}, {L"XyZ", L"xyz"},
    };
    for (int i = 0; i < 8; i++) {
        CK(sgn(mos_wcscasecmp(pares[i][0], pares[i][1])) == sgn(wcscasecmp(pares[i][0], pares[i][1])), "wcscasecmp");
        for (size_t n = 0; n <= 6; n++)
            CK(sgn(mos_wcsncasecmp(pares[i][0], pares[i][1], n)) == sgn(wcsncasecmp(pares[i][0], pares[i][1], n)), "wcsncasecmp");
    }
    /* wcsncat */
    for (size_t n = 0; n <= 5; n++) {
        wchar_t a[32], b[32];
        wcscpy(a, L"foo"); wcscpy(b, L"foo");
        mos_wcsncat(a, L"barbaz", n);
        wcsncat(b, L"barbaz", n);
        CK(wcscmp(a, b) == 0, "wcsncat");
    }
    /* wcscspn */
    const wchar_t *cs[][2] = { {L"hello world", L" "}, {L"abcdef", L"xyz"}, {L"abcdef", L"fa"}, {L"", L"x"}, {L"aaa", L"a"} };
    for (int i = 0; i < 5; i++)
        CK(mos_wcscspn(cs[i][0], cs[i][1]) == wcscspn(cs[i][0], cs[i][1]), "wcscspn");

    /* ---- wcslcpy/wcslcat (BSD, valores conhecidos) ---- */
    { wchar_t d[8]; size_t r = mos_wcslcpy(d, L"hello", 8); CK(r == 5 && wcscmp(d, L"hello") == 0, "wcslcpy simples"); }
    { wchar_t d[4]; size_t r = mos_wcslcpy(d, L"hello", 4); CK(r == 5 && wcscmp(d, L"hel") == 0, "wcslcpy trunca"); }
    { wchar_t d[1] = {L'x'}; size_t r = mos_wcslcpy(d, L"hi", 0); CK(r == 2 && d[0] == L'x', "wcslcpy n=0 não escreve"); }
    { wchar_t d[16]; wcscpy(d, L"ab"); size_t r = mos_wcslcat(d, L"cde", 16); CK(r == 5 && wcscmp(d, L"abcde") == 0, "wcslcat simples"); }
    { wchar_t d[5]; wcscpy(d, L"ab"); size_t r = mos_wcslcat(d, L"cdef", 5); CK(r == 6 && wcscmp(d, L"abcd") == 0, "wcslcat trunca"); }

    /* ---- pthread (caminhos de sucesso) ---- */
    { int p = -1; CK(mos_pthread_attr_getschedpolicy((void *)1, &p) == 0 && p == 1, "pthread_attr_getschedpolicy"); }
    { char a[64]; CK(mos_pthread_rwlockattr_init(a) == 0 && mos_pthread_rwlockattr_destroy(a) == 0, "pthread_rwlockattr"); }
    CK(mos_pthread_rwlockattr_init(0) == 22, "pthread_rwlockattr_init(NULL)=EINVAL");
    CK(mos_pthread_key_init_np(7, 0) == 0, "pthread_key_init_np");

    /* ---- Mach-O: header com 1 segmento e 2 seções ---- */
    {
        struct { uint32_t cmd, cmdsize; char segname[16]; uint64_t vmaddr, vmsize, fileoff, filesize; uint32_t maxprot, initprot, nsects, flags; } seg;
        unsigned char buf[sizeof(uint32_t) * 8 + sizeof(seg) + sizeof(sect64) * 2];
        memset(buf, 0, sizeof buf);
        uint32_t *mh = (uint32_t *)buf;
        mh[0] = 0xFEEDFACFu; mh[4] = 1; mh[5] = sizeof(seg) + sizeof(sect64) * 2;  /* magic, ncmds=1, sizeofcmds */
        unsigned char *p = buf + sizeof(uint32_t) * 8;
        memset(&seg, 0, sizeof seg);
        seg.cmd = 0x19; seg.cmdsize = sizeof(seg) + sizeof(sect64) * 2;
        strcpy(seg.segname, "__TEXT"); seg.nsects = 2;
        memcpy(p, &seg, sizeof seg);
        sect64 s0, s1;
        memset(&s0, 0, sizeof s0); memset(&s1, 0, sizeof s1);
        strcpy(s0.sectname, "__text"); strcpy(s0.segname, "__TEXT"); s0.addr = 0x1000; s0.size = 0x200;
        strcpy(s1.sectname, "__const"); strcpy(s1.segname, "__TEXT"); s1.addr = 0x1200; s1.size = 0x40;
        memcpy(p + sizeof seg, &s0, sizeof s0);
        memcpy(p + sizeof seg + sizeof(sect64), &s1, sizeof s1);

        const sect64 *g = mos_getsectbynamefromheader_64(buf, "__TEXT", "__const");
        CK(g && g->addr == 0x1200 && g->size == 0x40, "getsectbynamefromheader_64");
        uint64_t sz = 0;
        uint8_t *d = mos_getsectdatafromheader_64(buf, "__TEXT", "__text", &sz);
        CK(d == (uint8_t *)(uintptr_t)0x1000 && sz == 0x200, "getsectdatafromheader_64");
        CK(mos_getsectbynamefromheader_64(buf, "__DATA", "__text") == 0, "seção ausente -> NULL");
        d = mos_getsectdatafromheader_64(buf, "__TEXT", "__nope", &sz);
        CK(d == 0 && sz == 0, "getsectdata ausente -> NULL, size 0");
    }

    /* ---- swap_fat (troca de bytes no lugar, reversível) ---- */
    {
        uint32_t fh[2] = {0xCAFEBABEu, 3};
        uint32_t orig[2] = {fh[0], fh[1]};
        mos_swap_fat_header(fh, 0);
        CK(fh[0] == 0xBEBAFECAu && fh[1] == 0x03000000u, "swap_fat_header");
        mos_swap_fat_header(fh, 0);
        CK(fh[0] == orig[0] && fh[1] == orig[1], "swap_fat_header reversível");

        uint32_t fa[2][5] = { {0x01000007u, 3, 0x1000, 0x2000, 12}, {0x0100000Cu, 0, 0x3000, 0x4000, 14} };
        uint32_t cp[2][5]; memcpy(cp, fa, sizeof fa);
        mos_swap_fat_arch(fa, 2, 0);
        CK(fa[0][2] == 0x00100000u && fa[1][3] == 0x00400000u, "swap_fat_arch");
        mos_swap_fat_arch(fa, 2, 0);
        CK(memcmp(fa, cp, sizeof fa) == 0, "swap_fat_arch reversível");
    }

    printf("%d/%d ok\n", total - fails, total);
    return fails ? 1 : 0;
}
