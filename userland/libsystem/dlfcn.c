/*
 * mOS libSystem — dlopen/dlsym/dlclose/dlerror em tempo de execução e as consultas de imagem do dyld
 * (_dyld_image_count, _dyld_get_image_header/name/vmaddr_slide, _dyld_get_image_header_containing_address),
 * cobrindo as imagens carregadas na partida e as que vierem depois com dlopen.
 *
 * Quem lê o arquivo, mapeia e aplica os fixups é o kernel (kernel/compat/macho.c, SYS_DYLD, ABI em
 * kernel/include/syscall.h). Aqui, para cada imagem nova: registro das seções ObjC (cx_objc_boot →
 * cx_objc_init_image, objc.c), callbacks de _dyld_register_func_for_add_image (dyldimg.c) e, por
 * fim, os inicializadores (__mod_init_func / __init_offsets), na ordem de dependência — a mesma
 * sequência do dyld da Apple.
 *
 * Handles: (índice da imagem + 1) << 4; RTLD_DEFAULT/NEXT/SELF/MAIN_ONLY com os valores do macOS.
 * Nada é descarregado no dlclose (como o dyld faz com imagens que têm ObjC ou Swift).
 */
#include "sys.h"
#include "libc.h"

#define RTLD_NOLOAD 0x10
#define H_LIBSYSTEM 0x10000          /* devolvido pelo kernel para /usr/lib/libSystem.B.dylib e afins */
#define MAX_KNOWN 128

extern u64 cx_image_base;
extern char **NXArgv;
extern int NXArgc;

/* ---- trava (o dlopen pode ser chamado de várias threads; nunca segurada durante os inicializadores) */
static volatile int dl_lock;
static void lk(void) { while (__sync_lock_test_and_set(&dl_lock, 1)) SYS0(SYS_YIELD); }
static void ulk(void) { __sync_lock_release(&dl_lock); }

/* ---- dlerror: a mensagem vale até ser lida uma vez */
static char dl_err[320];
static int dl_err_set;
static void set_err(const char *fmt, const char *a, const char *b) {
    cx_snprintf(dl_err, sizeof dl_err, fmt, a, b);
    dl_err_set = 1;
}

/* ---- cache das imagens conhecidas (cabeçalho + caminho) */
static u32 known;
static const void *k_hdr[MAX_KNOWN];
static char *k_name[MAX_KNOWN];
static int no_kernel;                /* kernel sem SYS_DYLD: só o executável */

static void sync_images(void) {
    if (no_kernel) return;
    long n = SYS1(SYS_DYLD, MOS_DYLD_COUNT);
    if (n < 0) { no_kernel = 1; return; }
    char buf[512];
    while (known < (u32)n && known < MAX_KNOWN) {
        buf[0] = 0;
        long h = SYS4(SYS_DYLD, MOS_DYLD_IMAGE, known, buf, sizeof buf);
        k_hdr[known] = (const void *)h;
        k_name[known] = strdup(buf);
        known++;
    }
}

u32 _dyld_image_count(void) {
    lk();
    sync_images();
    u32 n = no_kernel ? 1 : known;
    ulk();
    return n;
}

const void *_dyld_get_image_header(u32 i) {
    lk();
    sync_images();
    const void *h = no_kernel ? (i == 0 ? (const void *)cx_image_base : 0) : (i < known ? k_hdr[i] : 0);
    ulk();
    return h;
}

const char *_dyld_get_image_name(u32 i) {
    lk();
    sync_images();
    const char *s = no_kernel ? (i == 0 && NXArgv ? NXArgv[0] : 0) : (i < known ? k_name[i] : 0);
    ulk();
    return s;
}

/* deslocamento entre o endereço preferido (__TEXT.vmaddr) e onde a imagem foi mapeada */
static long slide_of(const void *h) {
    const u32 *mh = h;
    if (!mh || mh[0] != 0xFEEDFACFu) return 0;
    const u8 *p = (const u8 *)h + 32;
    for (u32 i = 0; i < mh[4]; i++) {
        u32 cmd = *(const u32 *)p, sz = *(const u32 *)(p + 4);
        if (cmd == 0x19 && !strncmp((const char *)p + 8, "__TEXT", 16)) return (long)((u64)h - *(const u64 *)(p + 24));
        p += sz;
    }
    return 0;
}

long _dyld_get_image_vmaddr_slide(u32 i) { return slide_of(_dyld_get_image_header(i)); }

const void *_dyld_get_image_header_containing_address(const void *addr) {
    if (no_kernel) return 0;
    long i = SYS2(SYS_DYLD, MOS_DYLD_FIND, addr);
    return i >= 0 ? _dyld_get_image_header((u32)i) : 0;
}

/* ---- dlopen */
void *dlsym(void *h, const char *name);
extern void cx_objc_boot(u64 sects, u64 n);                  /* objc.c: liga o runtime (se preciso) e registra */
extern void cx_dyld_notify_add(u32 first, u32 n);            /* dyldimg.c: callbacks de add_image */
typedef void (*init_fn)(int, char **, char **, char **, void *);

static void *handle_of(long idx) { return (void *)(u64)(((u64)idx + 1) << 4); }

void *dlopen(const char *path, int mode) {
    if (!path) return (void *)(long)MOS_RTLD_DEFAULT;         /* o programa principal: busca global */
    char abs[1024];
    const char *p = path;
    if (path[0] != '/' && path[0] != '@') p = cx_path(path, abs, sizeof abs);   /* relativo à pasta atual */
    mos_dyld_open_t o;
    memset(&o, 0, sizeof o);
    o.flags = (u64)(mode & RTLD_NOLOAD);
    lk();
    long r = no_kernel ? -CX_ENOSYS : SYS4(SYS_DYLD, MOS_DYLD_OPEN, p, &o, __builtin_return_address(0));
    if (r < 0) {
        char m[256] = "";
        if (r != -CX_ENOSYS) SYS3(SYS_DYLD, MOS_DYLD_ERROR, m, sizeof m);
        if (r == -MOS_EINVAL && !m[0]) no_kernel = 1;
        ulk();
        if (mode & RTLD_NOLOAD) { dl_err_set = 0; return 0; }   /* RTLD_NOLOAD: NULL sem erro é o normal */
        cx_snprintf(dl_err, sizeof dl_err, "dlopen(%s, %d): %s", path, mode, m[0] ? m : "dlopen não é suportado neste kernel do mOS");
        dl_err_set = 1;
        return 0;
    }
    sync_images();
    ulk();
    if (r == H_LIBSYSTEM) return handle_of(H_LIBSYSTEM);
    if (o.nnew) {
        /* imagens novas: ObjC, avisos de imagem nova, inicializadores (dependências primeiro) */
        if (o.nobjc) cx_objc_boot(o.objc, o.nobjc);
        cx_dyld_notify_add((u32)o.first, (u32)o.nnew);
        static char *apple0[2];
        const u64 *f = (const u64 *)o.inits;
        for (u64 i = 0; i < o.ninits; i++) ((init_fn)f[i])(NXArgc, NXArgv, environ, apple0, 0);
    }
    return handle_of(r);
}

int dlclose(void *h) {
    u64 v = (u64)h;
    if ((long)v < 0 || !v) return 0;
    if (v & 15) { set_err("dlclose(%s): handle inválido%s", "?", ""); return -1; }
    long idx = (long)(v >> 4) - 1;
    if (idx != H_LIBSYSTEM && !no_kernel) SYS2(SYS_DYLD, MOS_DYLD_CLOSE, idx);
    return 0;
}

void *dlsym(void *h, const char *name) {
    if (!name) return 0;
    long hv = (long)h;
    if (hv > 0) {
        if (hv & 15) { set_err("dlsym(handle inválido, %s)%s", name, ""); return 0; }
        hv = (hv >> 4) - 1;
    }
    long a = no_kernel ? 0 : SYS4(SYS_DYLD, MOS_DYLD_SYM, hv, name, __builtin_return_address(0));
    if (a <= 0) { set_err("dlsym(%s): symbol not found (%s)", name, "mOS"); return 0; }
    return (void *)a;
}

char *dlerror(void) {
    if (!dl_err_set) return 0;
    dl_err_set = 0;
    return dl_err;
}

/* dlopen_preflight: o arquivo existe (o mOS não tem como validar sem carregar) */
int dlopen_preflight(const char *path) {
    if (!path) return 0;
    char abs[1024];
    mos_stat_t st;
    return SYS2(SYS_STAT, cx_path(path, abs, sizeof abs), &st) == 0;
}

/* No macOS as bibliotecas do sistema (/usr/lib, /System/Library) moram no shared cache do dyld, não no disco.
 * O verificador de linkagem do Homebrew pergunta isso (via Fiddle) para não reclamar delas depois de despejar
 * um bottle. No mOS elas são o próprio libSystem ou os aliases do loader (kernel/compat/macho.c). */
_Bool _dyld_shared_cache_contains_path(const char *path) {
    if (!path) return 0;
    const char *a = "/usr/lib/", *b = "/System/Library/";
    int i = 0;
    while (a[i] && path[i] == a[i]) i++;
    if (!a[i]) return 1;
    i = 0;
    while (b[i] && path[i] == b[i]) i++;
    return !b[i];
}

/* O libunwind do LLVM (estático nos binários do zig e de outros toolchains, x86_64 e arm64) acha as tabelas de
 * desenrolar da imagem que contém um pc por aqui: sem isto toda exceção C++ virava std::terminate. Os endereços são
 * os finais (seção + slide da imagem). Frente RUNTIME do tradutor arm64 (docs/tradutor-arm64.md). */
struct dyld_unwind_sections {
    const void *mh;
    const void *dwarf_section;
    unsigned long dwarf_section_length;
    const void *compact_unwind_section;
    unsigned long compact_unwind_section_length;
};
_Bool _dyld_find_unwind_sections(void *addr, struct dyld_unwind_sections *info) {
    { extern int cx_unwind_disabled(void); if (cx_unwind_disabled()) return 0; }   /* MOS_DESENROLAR=0 (unwind_ls.c) */
    const u8 *h = _dyld_get_image_header_containing_address(addr);
    /* pc dentro do próprio libSystem (ELF, as duas arquiteturas): o .eh_frame dele (unwind_ls.c, frente LIBSYS) */
    { extern _Bool cx_ls_unwind_sections(const void *, void *); if (!h && info && cx_ls_unwind_sections(addr, info)) return 1; }
    if (!h || !info || *(const u32 *)h != 0xFEEDFACF) return 0;
    long slide = slide_of(h);
    u32 ncmds = *(const u32 *)(h + 16);
    const u8 *p = h + 32;
    memset(info, 0, sizeof *info);
    info->mh = h;
    for (u32 i = 0; i < ncmds; i++) {
        u32 cmd = *(const u32 *)p, sz = *(const u32 *)(p + 4);
        if (cmd == 0x19) {                                   /* LC_SEGMENT_64: seções de 80 bytes depois de 72 */
            u32 nsects = *(const u32 *)(p + 64);
            for (u32 k = 0; k < nsects; k++) {
                const u8 *s = p + 72 + k * 80;
                u64 a = *(const u64 *)(s + 32) + (u64)slide, n = *(const u64 *)(s + 40);
                if (!strncmp((const char *)s, "__eh_frame", 16)) { info->dwarf_section = (const void *)a; info->dwarf_section_length = n; }
                if (!strncmp((const char *)s, "__unwind_info", 16)) { info->compact_unwind_section = (const void *)a; info->compact_unwind_section_length = n; }
            }
        }
        p += sz;
    }
    return info->dwarf_section || info->compact_unwind_section;
}
