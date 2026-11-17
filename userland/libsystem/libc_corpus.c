/*
 * mOS libSystem — símbolos que os apps do corpus (docs/lacunas/leonardo.md) importam e faltavam:
 *   - pthread: pthread_attr_getschedpolicy, pthread_rwlockattr_init/destroy, pthread_key_init_np
 *   - libmacho (<mach-o/getsect.h>, <mach-o/swap.h>): getsectdatafromheader_64,
 *     getsectbynamefromheader_64, swap_fat_arch, swap_fat_header
 * Entra no libSystem.mos pelo glob de tools/build.sh. Teste de host: tools/libc_corpus_hosttest.sh
 */
#include "sys.h"

/* ---------------------------------------------------------------- strings largas (wchar_t = int) */
/* wcslen/towlower vêm do resto do libSystem (posixmore.c/wctype_wide.c); no host, da libc do host. */
typedef int wchar_t_;
size_t wcslen(const wchar_t_ *s);
int towlower(int wc);

/* wcsncat: concatena no máximo n caracteres de s e termina em 0 (blender/gimp/krita) */
wchar_t_ *wcsncat(wchar_t_ *d, const wchar_t_ *s, size_t n) {
    wchar_t_ *o = d; while (*d) d++;
    while (n && *s) { *d++ = *s++; n--; }
    *d = 0; return o;
}
/* wcscspn: comprimento do prefixo de s sem nenhum caractere de set */
size_t wcscspn(const wchar_t_ *s, const wchar_t_ *set) {
    const wchar_t_ *p = s;
    for (; *p; p++) { const wchar_t_ *q = set; int hit = 0; for (; *q; q++) if (*q == *p) { hit = 1; break; } if (hit) break; }
    return (size_t)(p - s);
}
/* wcscasecmp/wcsncasecmp: comparação sem caixa, por towlower (tabelas Unicode em wctype_wide.c) */
int wcscasecmp(const wchar_t_ *a, const wchar_t_ *b) {
    int ca, cb;
    do { ca = towlower(*a++); cb = towlower(*b++); } while (ca && ca == cb);
    return ca < cb ? -1 : ca > cb;
}
int wcsncasecmp(const wchar_t_ *a, const wchar_t_ *b, size_t n) {
    for (; n; n--) {
        int ca = towlower(*a++), cb = towlower(*b++);
        if (ca != cb) return ca < cb ? -1 : 1;
        if (!ca) return 0;
    }
    return 0;
}
/* wcslcpy/wcslcat: variantes limitadas da BSD; devolvem o comprimento que a origem teria (trunca detectável) */
size_t wcslcpy(wchar_t_ *d, const wchar_t_ *s, size_t n) {
    size_t slen = wcslen(s);
    if (n) { size_t k = slen < n - 1 ? slen : n - 1; for (size_t i = 0; i < k; i++) d[i] = s[i]; d[k] = 0; }
    return slen;
}
size_t wcslcat(wchar_t_ *d, const wchar_t_ *s, size_t n) {
    size_t dlen = 0; while (dlen < n && d[dlen]) dlen++;
    if (dlen == n) return n + wcslen(s);            /* destino sem terminador dentro de n */
    size_t slen = wcslen(s), k = n - dlen - 1, i = 0;
    for (; i < k && s[i]; i++) d[dlen + i] = s[i];
    d[dlen + i] = 0;
    return dlen + slen;
}

/* ---------------------------------------------------------------- pthread (faltavam no corpus) */
/* Darwin: SCHED_OTHER=1, SCHED_RR=2, SCHED_FIFO=4. As threads do mOS rodam na política padrão. */
int pthread_attr_getschedpolicy(const void *a, int *policy) {
    (void)a;
    if (policy) *policy = 1;   /* SCHED_OTHER */
    return 0;
}
/* atributos de rwlock: o mOS não tem atributo por-processo; init/destroy só validam o ponteiro */
int pthread_rwlockattr_init(void *a) { return a ? 0 : 22 /* EINVAL */; }
int pthread_rwlockattr_destroy(void *a) { return a ? 0 : 22; }
/* pthread_key_init_np: registra um destrutor para uma chave reservada (interno da Apple). Sem chaves
 * reservadas no mOS; aceita e devolve 0, como o caminho de sucesso do macOS. */
int pthread_key_init_np(int key, void (*dtor)(void *)) { (void)key; (void)dtor; return 0; }

/* ---------------------------------------------------------------- libmacho: seções a partir de um header */
typedef struct { u32 magic, cputype, cpusubtype, filetype, ncmds, sizeofcmds, flags, reserved; } mh64_t;
typedef struct { u32 cmd, cmdsize; char segname[16]; u64 vmaddr, vmsize, fileoff, filesize; u32 maxprot, initprot, nsects, flags; } seg64_t;
typedef struct { char sectname[16], segname[16]; u64 addr, size; u32 offset, align, reloff, nreloc, flags, r1, r2, r3; } sect64_t;
#define LC_SEGMENT_64 0x19

static int mn_eq(const char *a, const char *b) {
    for (int i = 0; i < 16; i++) { if (a[i] != b[i]) return 0; if (!a[i]) return 1; }
    return 1;
}
/* percorre os comandos de carga do header e devolve a struct section_64 pedida (ou 0) */
const sect64_t *getsectbynamefromheader_64(const mh64_t *mhp, const char *seg, const char *sect) {
    if (!mhp || mhp->magic != 0xFEEDFACFu || !seg || !sect) return 0;
    const u8 *p = (const u8 *)(mhp + 1);
    for (u32 i = 0; i < mhp->ncmds; i++) {
        const seg64_t *s = (const seg64_t *)p;
        if (s->cmd == LC_SEGMENT_64 && mn_eq(s->segname, seg)) {
            const sect64_t *x = (const sect64_t *)(s + 1);
            for (u32 k = 0; k < s->nsects; k++) if (mn_eq(x[k].sectname, sect)) return &x[k];
        }
        p += s->cmdsize;
    }
    return 0;
}
/* devolve o endereço (campo addr, sem slide, como no macOS) e o tamanho da seção */
u8 *getsectdatafromheader_64(const mh64_t *mhp, const char *seg, const char *sect, u64 *size) {
    const sect64_t *x = getsectbynamefromheader_64(mhp, seg, sect);
    if (!x) { if (size) *size = 0; return 0; }
    if (size) *size = x->size;
    return (u8 *)(u64)x->addr;
}

/* ---------------------------------------------------------------- libmacho: troca de ordem de bytes do fat */
/* struct fat_header { u32 magic, nfat_arch } e struct fat_arch { u32 cputype, cpusubtype, offset, size, align }.
 * As funções trocam os bytes no lugar, sem olhar a ordem alvo (o chamador decide a direção), como no macOS. */
typedef struct { u32 magic, nfat_arch; } fat_header_t;
typedef struct { u32 cputype, cpusubtype, offset, size, align; } fat_arch_t;

static u32 bswap32(u32 v) {
    return (v >> 24) | ((v >> 8) & 0xFF00u) | ((v << 8) & 0xFF0000u) | (v << 24);
}
void swap_fat_header(fat_header_t *fh, int target_byte_order) {
    (void)target_byte_order;
    if (!fh) return;
    fh->magic = bswap32(fh->magic);
    fh->nfat_arch = bswap32(fh->nfat_arch);
}
void swap_fat_arch(fat_arch_t *fa, u32 nfat, int target_byte_order) {
    (void)target_byte_order;
    for (u32 i = 0; fa && i < nfat; i++) {
        fa[i].cputype = bswap32(fa[i].cputype);
        fa[i].cpusubtype = bswap32(fa[i].cpusubtype);
        fa[i].offset = bswap32(fa[i].offset);
        fa[i].size = bswap32(fa[i].size);
        fa[i].align = bswap32(fa[i].align);
    }
}
