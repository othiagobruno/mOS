/*
 * mOS libSystem — consultas de imagem do dyld usadas pelo runtime Swift completo (e por libobjc/C++):
 * _dyld_register_func_for_add_image, getsectiondata, getsegmentdata, _dyld_get_image_vmaddr_slide.
 * As imagens são as que o carregador do mOS mapeou (_dyld_image_count/_dyld_get_image_header).
 */
#include "foundation.h"
#include "libc.h"

typedef struct { u32 magic, cputype, cpusubtype, filetype, ncmds, sizeofcmds, flags, reserved; } mh64_t;
typedef struct { u32 cmd, cmdsize; char segname[16]; u64 vmaddr, vmsize, fileoff, filesize; u32 maxprot, initprot, nsects, flags; } seg64_t;
typedef struct { char sectname[16], segname[16]; u64 addr, size; u32 offset, align, reloff, nreloc, flags, r1, r2, r3; } sect64_t;
#define LC_SEGMENT_64 0x19

extern u32 _dyld_image_count(void);
extern const void *_dyld_get_image_header(u32 i);

static int nameq(const char *a, const char *b) { for (int i = 0; i < 16; i++) { if (a[i] != b[i]) return 0; if (!a[i]) return 1; } return 1; }

/* deslocamento entre o endereço preferido (__TEXT.vmaddr) e onde a imagem foi mapeada */
static i64 slide_of(const mh64_t *mh) {
    if (!mh || mh->magic != 0xFEEDFACFu) return 0;
    const u8 *p = (const u8 *)(mh + 1);
    for (u32 i = 0; i < mh->ncmds; i++) {
        const seg64_t *s = (const seg64_t *)p;
        if (s->cmd == LC_SEGMENT_64 && nameq(s->segname, "__TEXT")) return (i64)((u64)mh - s->vmaddr);
        p += s->cmdsize;
    }
    return 0;
}
/* _dyld_get_image_vmaddr_slide e as demais consultas de imagem ficam em dlfcn.c */

u8 *getsectiondata(const void *header, const char *seg, const char *sect, u64 *size) {
    const mh64_t *mh = header;
    if (size) *size = 0;
    if (!mh || mh->magic != 0xFEEDFACFu) return 0;
    i64 sl = slide_of(mh);
    const u8 *p = (const u8 *)(mh + 1);
    for (u32 i = 0; i < mh->ncmds; i++) {
        const seg64_t *s = (const seg64_t *)p;
        if (s->cmd == LC_SEGMENT_64 && nameq(s->segname, seg)) {
            const sect64_t *x = (const sect64_t *)(s + 1);
            for (u32 k = 0; k < s->nsects; k++)
                if (nameq(x[k].sectname, sect)) { if (size) *size = x[k].size; return (u8 *)(x[k].addr + sl); }
        }
        p += s->cmdsize;
    }
    return 0;
}
/* getsegbyname/getsectbyname (<mach-o/getsect.h>): o comando de carga do EXECUTÁVEL principal, como no macOS (os
 * campos ficam com os endereços do arquivo; quem chama soma _dyld_get_image_vmaddr_slide(0)). O Bun (Claude Code)
 * acha o JavaScript embutido no segmento __BUN assim; devolver NULL o deixava sem o programa. */
const seg64_t *getsegbyname(const char *seg) {
    const mh64_t *mh = _dyld_get_image_header(0);
    if (!mh || mh->magic != 0xFEEDFACFu || !seg) return 0;
    const u8 *p = (const u8 *)(mh + 1);
    for (u32 i = 0; i < mh->ncmds; i++) {
        const seg64_t *s = (const seg64_t *)p;
        if (s->cmd == LC_SEGMENT_64 && nameq(s->segname, seg)) return s;
        p += s->cmdsize;
    }
    return 0;
}
const sect64_t *getsectbyname(const char *seg, const char *sect) {
    const seg64_t *s = getsegbyname(seg);
    if (!s || !sect) return 0;
    const sect64_t *x = (const sect64_t *)(s + 1);
    for (u32 k = 0; k < s->nsects; k++) if (nameq(x[k].sectname, sect)) return &x[k];
    return 0;
}
u8 *getsegmentdata(const void *header, const char *seg, u64 *size) {
    const mh64_t *mh = header;
    if (size) *size = 0;
    if (!mh || mh->magic != 0xFEEDFACFu) return 0;
    i64 sl = slide_of(mh);
    const u8 *p = (const u8 *)(mh + 1);
    for (u32 i = 0; i < mh->ncmds; i++) {
        const seg64_t *s = (const seg64_t *)p;
        if (s->cmd == LC_SEGMENT_64 && nameq(s->segname, seg)) { if (size) *size = s->vmsize; return (u8 *)(s->vmaddr + sl); }
        p += s->cmdsize;
    }
    return 0;
}

/* callbacks chamados para as imagens já carregadas e guardados para as que o dlopen trouxer depois
 * (dlfcn.c chama cx_dyld_notify_add com a imagem já relocada, antes dos inicializadores dela) */
extern void cx_swift_note(const void *mh, i64 slide);   /* swiftsect.c: __swift5_* */
extern void cx_swift_forget(const void *mh);
typedef void (*add_fn)(const void *mh, i64 slide);
static add_fn add_cbs[32];
static int nadd_cbs;
void _dyld_register_func_for_add_image(void (*fn)(const void *mh, i64 slide)) {
    if (nadd_cbs < 32) add_cbs[nadd_cbs++] = fn;
    u32 n = _dyld_image_count();
    for (u32 i = 0; i < n; i++) {
        const void *h = _dyld_get_image_header(i);
        if (!h) continue;
        cx_swift_note(h, slide_of(h));
        fn(h, slide_of(h));
    }
}
void cx_dyld_notify_add(u32 first, u32 n) {
    for (u32 i = first; i < first + n; i++) {
        const void *h = _dyld_get_image_header(i);
        if (!h) continue;
        cx_swift_note(h, slide_of(h));
        for (int k = 0; k < nadd_cbs; k++) add_cbs[k](h, slide_of(h));
    }
}
/* Último dlclose (MOS_DYLD_CLOSE devolve 1): dlfcn.c chama cx_dyld_notify_remove com o cabeçalho e o slide.
 * O kernel não desmapeia. Dylib com seção __objc_ ou __swift não recebe o aviso. */
typedef void (*rem_fn)(const void *mh, i64 slide);
static rem_fn rem_cbs[32];
static int nrem_cbs;
void _dyld_register_func_for_remove_image(void (*fn)(const void *mh, i64 slide)) {
    if (fn && nrem_cbs < 32) rem_cbs[nrem_cbs++] = fn;
}
void cx_dyld_notify_remove(const void *mh, i64 slide) {
    if (!mh) return;
    cx_swift_forget(mh);
    for (int k = 0; k < nrem_cbs; k++) rem_cbs[k](mh, slide);
}
