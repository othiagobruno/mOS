/*
 * mOS — carregador de executáveis Mach-O (o "dyld" do mOS), no espírito do ravynOS.
 *
 * Layout do processo (região de usuário, ver proc.h):
 *   0x400000100000  libSystem.mos   (ELF estático: build/libSystem.mos, embutido no kernel)
 *   0x400001000000  imagem Mach-O   (com slide; os segmentos ficam contíguos)
 *        ...        stubs de símbolos ausentes, depois o heap (brk)
 *   USER_STACK_TOP  pilha: strings, argv, envp, apple, bloco de partida
 *
 * Os binds (dyld_info clássico ou chained fixups) são resolvidos contra a tabela de símbolos
 * do libSystem.mos. A execução começa em cx_start (libSystem), que roda os inicializadores e
 * chama main().
 */
#ifdef MOS_HOST_TEST
#include "host_proc.h"
#else
#include "proc.h"
#endif
#include "compat.h"
#ifdef MOS_HOST_TEST
#undef SYS_OPEN
#endif
#include "syscall.h"           /* SYS_DYLD / MOS_DYLD_* (dlopen em tempo de execução) e errno */
#include "fwlink.h"            /* symlink de .framework gravado como texto */
#include "../../userland/a64rt/a64rt_abi.h"   /* processos arm64: runtime do tradutor (docs/tradutor-arm64.md) */

#define LIBSYSTEM_BASE (USER_BASE + 0x00100000ULL)
#define IMAGE_BASE   (USER_BASE + 0x01000000ULL)
#define IMAGE_LIMIT  (USER_STACK_TOP - USER_STACK_SIZE - 0x200000ULL)
#ifdef MACHO_DEBUG
#define DBG(...) kprintf(__VA_ARGS__)
#else
#define DBG(...) do {} while (0)
#endif

#ifdef MOS_HOST_TEST
const u8 *libsystem_mos, *libsystem_mos_end;
#else
extern const u8 libsystem_mos[], libsystem_mos_end[];
#endif

static u16 rd16(const u8 *p) { return (u16)(p[0] | p[1] << 8); }
static u32 rd32(const u8 *p) { return (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24; }
static u64 rd64(const u8 *p) { return (u64)rd32(p) | (u64)rd32(p + 4) << 32; }
static u32 be32_(const u8 *p) { return (u32)p[0] << 24 | (u32)p[1] << 16 | (u32)p[2] << 8 | p[3]; }
static u64 be64_(const u8 *p) { return (u64)be32_(p) << 32 | be32_(p + 4); }

/* ================================================================ libSystem.mos (ELF) */
typedef struct { const char *name; u64 addr; } lsym_t;
/* Um libSystem por arquitetura do processo: 0 = x86_64 (embutido no kernel), 1 = arm64 (tradutor arm64,
 * docs/tradutor-arm64.md: o libSystem arm64 do disco em A64RT_LIBSYSTEM; o runtime a64rt.mos roda o código).
 * ls_cur é a arquitetura do link em andamento (um por vez, sob a trava grande do kernel). */
typedef struct { const u8 *e, *end; lsym_t *syms; u32 n; u64 entry; u8 *img; u64 span; } lsdb_t;
static lsdb_t ls_db[2];
static int ls_cur;
#define lsyms    (ls_db[ls_cur].syms)
#define nlsyms   (ls_db[ls_cur].n)
#define ls_entry (ls_db[ls_cur].entry)
#define LS_E     (ls_db[ls_cur].e)
#define LS_END   (ls_db[ls_cur].end)
#define LS_CPU   (ls_cur ? 0x0100000Cu : 0x01000007u)       /* CPU_TYPE_ARM64 : CPU_TYPE_X86_64 */
static const u8 *dep_read(const char *path, usize *len);

/* o ELF do libSystem da arquitetura atual (o arm64 vem do disco: copiado uma vez, os nomes apontam para ele) */
static bool ls_blob(char *err, usize errlen) {
    lsdb_t *d = &ls_db[ls_cur];
    if (d->e) return true;
    if (!ls_cur) { d->e = libsystem_mos; d->end = libsystem_mos_end; return true; }
    usize n = 0;
    const u8 *f = dep_read(A64RT_LIBSYSTEM, &n);
    if (!f || n < 64) { snprintf(err, errlen, "app só para Apple Silicon (arm64) e o libSystem arm64 do tradutor (%s) não está instalado", A64RT_LIBSYSTEM); return false; }
    u8 *c = kmalloc_pages(n);
    if (!c) { snprintf(err, errlen, "sem memória para o libSystem arm64"); return false; }
    memcpy(c, f, n);
    d->e = c; d->end = c + n;
    return true;
}

static bool libsystem_index(char *err, usize errlen) {
    if (lsyms) return true;
    if (!ls_blob(err, errlen)) return false;
    const u8 *e = LS_E;
    usize len = (usize)(LS_END - LS_E);
    if (!e || len < 64 || memcmp(e, "\x7F" "ELF", 4)) { snprintf(err, errlen, "libSystem.mos ausente do kernel"); return false; }
    if (rd16(e + 18) != (ls_cur ? 183 : 62)) { snprintf(err, errlen, "libSystem%s.mos com arquitetura errada", ls_cur ? " arm64" : ""); return false; }
    ls_entry = rd64(e + 24) + (rd16(e + 16) == 3 ? LIBSYSTEM_BASE : 0);
    u64 shoff = rd64(e + 40);
    u16 shentsize = rd16(e + 58), shnum = rd16(e + 60);
    if (shoff + (u64)shnum * shentsize > len) { snprintf(err, errlen, "libSystem.mos corrompido"); return false; }
    for (u16 i = 0; i < shnum; i++) {
        const u8 *sh = e + shoff + (u64)i * shentsize;
        if (rd32(sh + 4) != 2) continue;               /* SHT_SYMTAB */
        u64 off = rd64(sh + 24), size = rd64(sh + 32), entsz = rd64(sh + 56);
        const u8 *strsh = e + shoff + (u64)rd32(sh + 40) * shentsize;
        u64 stroff = rd64(strsh + 24);
        if (!entsz || off + size > len) break;
        u64 n = size / entsz;
        lsyms = kcalloc(n, sizeof(lsym_t));
        if (!lsyms) break;
        for (u64 k = 0; k < n; k++) {
            const u8 *s = e + off + k * entsz;
            u8 info = s[4];
            u16 shndx = rd16(s + 6);
            if ((info >> 4) != 1 || !shndx) continue;  /* só GLOBAL definidos */
            if ((info & 15) > 2) continue;             /* NOTYPE (rótulos de asm), OBJECT ou FUNC */
            lsyms[nlsyms].name = (const char *)e + stroff + rd32(s);
            lsyms[nlsyms].addr = rd64(s + 8) + (rd16(e + 16) == 3 ? LIBSYSTEM_BASE : 0);
            nlsyms++;
        }
        break;
    }
    if (!nlsyms) { snprintf(err, errlen, "libSystem.mos sem tabela de símbolos"); return false; }
    return true;
}

static u64 ls_lookup_exact(const char *name, usize n) {
    for (u32 i = 0; i < nlsyms; i++)
        if (!strncmp(lsyms[i].name, name, n) && !lsyms[i].name[n]) return lsyms[i].addr;
    return 0;
}

/* nome Mach-O ("_printf", "_fopen$DARWIN_EXTSN", "___stdoutp") -> endereço no libSystem */
static u64 ls_lookup(const char *mname) {
    if (mname[0] == '_') mname++;
    usize n = strlen(mname);
    u64 a = ls_lookup_exact(mname, n);
    if (a) return a;
    const char *d = strchr(mname, '$');                /* variantes $INODE64, $UNIX2003... */
    return d ? ls_lookup_exact(mname, (usize)(d - mname)) : 0;
}

/* O libSystem.mos é um ELF "static-pie": é carregado em LIBSYSTEM_BASE e as relocações
 * R_X86_64_RELATIVE do PT_DYNAMIC são aplicadas aqui. */

/* monta em buf (span bytes, zerados) os segmentos do libSystem com as relocações aplicadas */
/* ELF static-pie ligado em 0 (libSystem x86_64/arm64, a64rt.mos): segmentos em buf (span bytes, zerados) e relocações
 * RELATIVE do PT_DYNAMIC aplicadas para a base final 'base' */
static bool elf_build(const u8 *e, usize elen, u8 *buf, u64 span, u64 base, u32 relative, u64 dyn_va, u64 dyn_sz) {
    u64 phoff = rd64(e + 32);
    u16 phentsize = rd16(e + 54), phnum = rd16(e + 56);
    for (u16 i = 0; i < phnum; i++) {
        const u8 *ph = e + phoff + (u64)i * phentsize;
        if (rd32(ph) != 1) continue;
        u64 off = rd64(ph + 8), va = rd64(ph + 16), fsz = rd64(ph + 32);
        if (off + fsz > elen || va + fsz > span) return false;
        memcpy(buf + va, e + off, fsz);
    }
    u64 rela = 0, relasz = 0, relaent = 24;
    for (u64 o = 0; dyn_va && o + 16 <= dyn_sz && dyn_va + o + 16 <= span; o += 16) {
        u64 tag = rd64(buf + dyn_va + o), val = rd64(buf + dyn_va + o + 8);
        if (tag == 0) break;
        if (tag == 7) rela = val;           /* DT_RELA */
        if (tag == 8) relasz = val;         /* DT_RELASZ */
        if (tag == 9) relaent = val;        /* DT_RELAENT */
    }
    for (u64 o = 0; rela && relaent && o + relaent <= relasz && rela + o + 24 <= span; o += relaent) {
        u64 r_off = rd64(buf + rela + o), r_info = rd64(buf + rela + o + 8), r_add = rd64(buf + rela + o + 16);
        if ((r_info & 0xFFFFFFFF) != relative || r_off + 8 > span) continue;
        u64 v = base + r_add;
        memcpy(buf + r_off, &v, 8);
    }
    return true;
}
/* extensão dos PT_LOAD (span) e o PT_DYNAMIC */
static u64 elf_span(const u8 *e, u64 *dyn_va, u64 *dyn_sz) {
    u64 phoff = rd64(e + 32), span = 0;
    u16 phentsize = rd16(e + 54), phnum = rd16(e + 56);
    *dyn_va = *dyn_sz = 0;
    for (u16 i = 0; i < phnum; i++) {
        const u8 *ph = e + phoff + (u64)i * phentsize;
        if (rd32(ph) == 1 && rd64(ph + 16) + rd64(ph + 40) > span) span = rd64(ph + 16) + rd64(ph + 40);
        if (rd32(ph) == 2) { *dyn_va = rd64(ph + 16); *dyn_sz = rd64(ph + 40); }
    }
    return (span + 0xFFF) & ~0xFFFULL;
}

#ifndef MOS_HOST_TEST
/* A imagem relocada é a mesma em todo processo (endereço fixo, só relocações RELATIVE): é montada uma vez, em páginas
 * alinhadas (kmalloc_pages), e entra em cada processo em cópia na escrita, como as imagens do icache. Antes cada processo
 * novo pedia 3,2 MiB contíguos (kcalloc) e ficava com uma cópia própria: com o Claude e o Chrome numa VM de 2816 MB e a
 * memória fragmentada, um renderer do Chrome não subiu ("kmalloc: sem memória para 3387392 bytes", "sem memória para o
 * libSystem"). Os 2,2 MiB só de leitura (r e rx) ficam compartilhados; as páginas de dados e bss que um processo escreve
 * viram dele na primeira escrita. A imagem nunca é liberada. Vale para o libSystem de cada arquitetura e para o runtime
 * do tradutor arm64 (a64rt.mos). */
bool uvm_map_cow(process_t *p, u64 va, u64 phys);
#endif

/* ELF static-pie em 'base' no processo; img_ e ispan_ guardam a montagem compartilhada; devolve o span (0 = erro) */
static u64 map_elf(process_t *p, const u8 *e, usize elen, u64 base, u32 relative, u64 maxspan, u8 **img_, u64 *ispan_,
                   const char *what, char *err, usize errlen) {
    u64 dyn_va, dyn_sz, span = elf_span(e, &dyn_va, &dyn_sz);
    if (!span || span > maxspan) { snprintf(err, errlen, "%s com tamanho inválido", what); return 0; }
#ifndef MOS_HOST_TEST
    if (!*img_) {
        u8 *img = kmalloc_pages(span);
        if (img) {
            memset(img, 0, span);
            if (elf_build(e, elen, img, span, base, relative, dyn_va, dyn_sz)) {
                *img_ = img; *ispan_ = span;
                kprintf("macho: %s (%llu KiB) montado uma vez e compartilhado entre os processos (cópia na escrita)\n",
                        what, (unsigned long long)(span >> 10));
            } else kfree(img);
        }
    }
    if (*img_ && *ispan_ == span) {
        for (u64 j = 0; j < span / 4096; j++)
            if (!uvm_map_cow(p, base + j * 4096, (u64)*img_ + j * 4096)) {
                snprintf(err, errlen, "sem memória para o %s", what);
                return 0;
            }
        return span;
    }
#else
    (void)img_; (void)ispan_;
#endif
    /* sem a imagem compartilhada (e no machotest): uma cópia própria, como antes */
    u8 *buf = kcalloc(1, span);
    if (!buf) { snprintf(err, errlen, "sem memória para o %s", what); return 0; }
    if (!elf_build(e, elen, buf, span, base, relative, dyn_va, dyn_sz)) { kfree(buf); snprintf(err, errlen, "%s corrompido", what); return 0; }
    bool ok = uvm_map(p, base, span) && uvm_copy_out(p, base, buf, span);
    kfree(buf);
    if (!ok) { snprintf(err, errlen, "sem memória para o %s", what); return 0; }
    return span;
}

static bool map_libsystem(process_t *p, char *err, usize errlen) {
    lsdb_t *d = &ls_db[ls_cur];
    return map_elf(p, d->e, (usize)(d->end - d->e), LIBSYSTEM_BASE, ls_cur ? 1027 : 8, 0x00F00000, &d->img, &d->span,
                   ls_cur ? "libSystem arm64" : "libSystem", err, errlen) != 0;
}
/* ================================================================ Mach-O */
#define LC_SEGMENT_64          0x19
#define LC_LOAD_DYLIB          0x0C
#define LC_LOAD_WEAK_DYLIB     0x80000018
#define LC_REEXPORT_DYLIB      0x8000001F
#define LC_LAZY_LOAD_DYLIB     0x20
#define LC_LOAD_UPWARD_DYLIB   0x80000023
#define LC_DYLD_INFO           0x22
#define LC_DYLD_INFO_ONLY      0x80000022
#define LC_DYLD_CHAINED_FIXUPS 0x80000034
#define LC_MAIN                0x80000028
#define LC_UNIXTHREAD          0x05
#define LC_VERSION_MIN_MACOSX  0x24
#define LC_BUILD_VERSION       0x32
#define LC_ID_DYLIB            0x0D
#define LC_RPATH               0x8000001C
#define LC_DYLD_EXPORTS_TRIE   0x80000033
#define MH_PIE                 0x200000
#define MH_EXECUTE             2
#define MH_DYLIB               6
#define MH_BUNDLE              8       /* plugins carregados com dlopen (ex.: os .node do Node.js) */

#define MAX_SEGS   16
#define MAX_DYLIBS 160
#define MAX_RPATHS 8
#define MAX_IMAGES 96          /* executável + dylibs/frameworks do bundle + as carregadas com dlopen */
#define MAX_OBJC   128         /* seções ObjC somando todas as imagens */

typedef struct {
    char name[17];
    u64 vmaddr, vmsize, fileoff, filesize;
    bool zero;              /* __PAGEZERO */
    u32 prot;               /* initprot (VM_PROT_*): registrada para o mach_vm_region (machipc.c) */
} seg_t;

typedef struct {
    const u8 *m;            /* início do slice x86_64 */
    usize len;
    seg_t segs[MAX_SEGS];
    int nsegs;
    const char *dylibs[MAX_DYLIBS];
    bool dylib_weak[MAX_DYLIBS];
    int ndylibs;
    u64 vmin, vmax;
    u64 entryoff;
    bool has_main;
    bool has_thread;        /* LC_UNIXTHREAD (alvo anterior ao macOS 10.8): a entrada é o start do crt1 */
    const u8 *dyld_info;    /* comando LC_DYLD_INFO(_ONLY) */
    const u8 *chained;      /* comando LC_DYLD_CHAINED_FIXUPS */
    u64 init_ptrs_addr, init_ptrs_count;     /* __mod_init_func */
    u64 init_offs_addr, init_offs_count;     /* __init_offsets */
    struct { u64 addr, size, kind; } objc[24];   /* seções Objective-C, repassadas ao runtime */
    int nobjc;
    bool has_tlv;
    /* seções de dados (sem instruções, sem zerofill, fora do __LINKEDIT): onde um executável sem MH_PIE guarda os
     * ponteiros absolutos que precisam do slide (ver nonpie_slide) */
    struct { u64 addr, size; bool strict; } dsect[48];   /* strict: descritores de tipo do Go (pares de int32 no meio) */
    int ndsect;
    u64 first_sect;         /* endereço da primeira seção: abaixo dele só há o cabeçalho e os comandos */
    struct { u64 addr, size; } csect[16];   /* seções de código */
    int ncsect;
    const u8 *fstarts; u32 fstarts_size;      /* LC_FUNCTION_STARTS: ULEB dos inícios de função */
    u64 pcln_text;          /* &pcHeader.textStart do Go (pclntab), 0 se não houver */
    u64 tlv_lo, tlv_hi;                      /* template TLS (__thread_data + __thread_bss) */
    u64 tlv_init_end;                        /* fim dos dados inicializados (__thread_data) */
    u64 tlv_desc[8], tlv_desc_size[8];       /* seções de descritores (__thread_vars) */
    int ntlv_desc;    u32 flags;
    u32 minos;              /* 0xMMMMmmpp */
    u32 ftype;              /* MH_EXECUTE ou MH_DYLIB */
    const char *rpaths[MAX_RPATHS];          /* LC_RPATH (podem usar @executable_path/@loader_path) */
    int nrpaths;
    const u8 *trie;         /* export trie (LC_DYLD_EXPORTS_TRIE ou parte de exports do dyld_info) */
    u32 triesz;
} mach_t;

/* nomes de bibliotecas que o libSystem.mos substitui */
bool libsystem_dylib_ok(const char *n) {
    static const char *ok[] = {
        "/usr/lib/libSystem.B.dylib", "/usr/lib/libSystem.dylib", "/usr/lib/libc.dylib",
        "/usr/lib/libm.dylib", "/usr/lib/libdl.dylib", "/usr/lib/libpthread.dylib",
        "/usr/lib/libinfo.dylib", "/usr/lib/libgcc_s.1.dylib", "/usr/lib/libiconv.2.dylib", "/usr/lib/libiconv.dylib",
        /* git e curl do Homebrew: res_9_* (darwin.c) e uidna_* (posixmore.c, só nomes ASCII) */
        "/usr/lib/libresolv.9.dylib", "/usr/lib/libicucore.A.dylib",
        "/usr/lib/libbz2.1.0.dylib",   /* bzip2.c (o unzip do Homebrew a lista; descompressão BZ2_*) */
        "/usr/lib/libxar.1.dylib",     /* compat_agentes.c: o Sparkle a liga para patches delta; xar_open sempre falha */
        /* libc++/libc++abi: operator new/delete, guardas de estáticos e o resto em cxxabi.c e libcxx.c */
        "/usr/lib/libc++.1.dylib", "/usr/lib/libc++abi.dylib",
    };
    for (u32 i = 0; i < ARRAY_LEN(ok); i++) if (!strcmp(n, ok[i])) return true;
    /* frameworks implementados pelo libSystem do mOS (runtime ObjC, Foundation, AppKit, CG) */
    static const char *fw[] = {"/usr/lib/libobjc.A.dylib", "/System/Library/Frameworks/Foundation.framework/",
                               "/System/Library/Frameworks/AppKit.framework/", "/System/Library/Frameworks/Cocoa.framework/",
                               "/System/Library/Frameworks/CoreFoundation.framework/", "/System/Library/Frameworks/CoreGraphics.framework/",
                               "/System/Library/Frameworks/ApplicationServices.framework/", "/System/Library/Frameworks/QuartzCore.framework/",
                               "/System/Library/Frameworks/CoreText.framework/",
                               /* sessão CoreAnimation/IOSurface: iosurface.c / quartzcore.c */
                               "/System/Library/Frameworks/IOSurface.framework/", "/System/Library/Frameworks/CoreVideo.framework/",
                               /* Metal por software (metal.c, metalexec.c, mtlshader.c; docs/metal.md) */
                               "/System/Library/Frameworks/Metal.framework/", "/System/Library/Frameworks/MetalKit.framework/",
                               /* UTType e as constantes UTType* (vscode_media.c) */
                               "/System/Library/Frameworks/UniformTypeIdentifiers.framework/",
                               /* compatibilidade dos apps (docs/compat-apps.md): security.c (Keychain, SecCode), cli.c
                                * (proxies, FSEvents, IOPMAssertion), mach.c (IOKit mínimo) */
                               "/System/Library/Frameworks/Security.framework/", "/System/Library/Frameworks/SystemConfiguration.framework/",
                               "/System/Library/Frameworks/CFNetwork.framework/", "/System/Library/Frameworks/CoreServices.framework/",
                               "/System/Library/Frameworks/IOKit.framework/", "/System/Library/Frameworks/LocalAuthentication.framework/",
                               /* libcurl do Homebrew (docs/porte-brew.md): LDAP e GSS/Kerberos só falham (posixmore.c) */
                               "/System/Library/Frameworks/LDAP.framework/", "/System/Library/Frameworks/Kerberos.framework/",
                               "/System/Library/Frameworks/GSS.framework/",
                               /* o Google Chrome baixado e instalado abre sem o modo experimental (docs/porte-chrome.md): as
                                * funções que ele usa destes frameworks estão em chrome.c e nas sessões donas (áudio, mídia,
                                * IOKit...); o que faltar vira stub que encerra dizendo o nome, como em qualquer outro */
                               "/System/Library/Frameworks/AVFAudio.framework/",
                               "/System/Library/Frameworks/AVFoundation.framework/",
                               "/System/Library/Frameworks/Accelerate.framework/",
                               "/System/Library/Frameworks/Accessibility.framework/",
                               "/System/Library/Frameworks/AudioToolbox.framework/",
                               "/System/Library/Frameworks/AudioUnit.framework/",
                               "/System/Library/Frameworks/AuthenticationServices.framework/",
                               "/System/Library/Frameworks/Carbon.framework/",
                               "/System/Library/Frameworks/CoreAudio.framework/",
                               "/System/Library/Frameworks/CoreBluetooth.framework/",
                               "/System/Library/Frameworks/CoreHaptics.framework/",
                               "/System/Library/Frameworks/CoreImage.framework/",
                               "/System/Library/Frameworks/CoreLocation.framework/",
                               "/System/Library/Frameworks/CoreMIDI.framework/",
                               "/System/Library/Frameworks/CoreML.framework/",
                               "/System/Library/Frameworks/CoreMedia.framework/",
                               "/System/Library/Frameworks/CoreMediaIO.framework/",
                               "/System/Library/Frameworks/CoreWLAN.framework/",
                               "/System/Library/Frameworks/CryptoTokenKit.framework/",
                               "/System/Library/Frameworks/DiskArbitration.framework/",
                               "/System/Library/Frameworks/ForceFeedback.framework/",
                               "/System/Library/Frameworks/GameController.framework/",
                               "/System/Library/Frameworks/IOBluetooth.framework/",
                               "/System/Library/Frameworks/LocalAuthenticationEmbeddedUI.framework/",
                               "/System/Library/Frameworks/MediaAccessibility.framework/",
                               "/System/Library/Frameworks/MediaPlayer.framework/",
                               "/System/Library/Frameworks/Network.framework/",
                               "/System/Library/Frameworks/OpenDirectory.framework/",
                               "/System/Library/Frameworks/OpenGL.framework/",
                               "/System/Library/Frameworks/Quartz.framework/",
                               /* Codex desktop (runtime OWL): liga o framework sem importar símbolos (pede QLThumbnail* pelo nome; sem a classe, sem miniatura) */
                               "/System/Library/Frameworks/QuickLookThumbnailing.framework/",
                               "/System/Library/Frameworks/SafariServices.framework/",
                               "/System/Library/Frameworks/ScreenCaptureKit.framework/",
                               "/System/Library/Frameworks/SecurityInterface.framework/",
                               "/System/Library/Frameworks/ServiceManagement.framework/",
                               "/System/Library/Frameworks/UserNotifications.framework/",
                               "/System/Library/Frameworks/VideoToolbox.framework/",
                               "/System/Library/Frameworks/Vision.framework/",
                               "/System/Library/PrivateFrameworks/SkyLight.framework/", "/usr/lib/libsandbox.1.dylib",
                               "/usr/lib/libbsm.0.dylib", "/usr/lib/libcups.2.dylib", "/usr/lib/libpmenergy.dylib", "/usr/lib/libpmsample.dylib"};
    for (u32 i = 0; i < ARRAY_LEN(fw); i++) if (!strncmp(n, fw[i], strlen(fw[i]))) return true;
    return !strncmp(n, "/usr/lib/system/", 16);
}

/* libc++ e libc++abi: vêm da dylib do LLVM no disco quando ela existe (link_image e dyld_open); o libSystem só as
 * substitui quando ela falta */
#define LIBCXX_DISK "/usr/lib/libc++.1.dylib"
static bool libcxx_name(const char *n) { return !strcmp(n, LIBCXX_DISK) || !strcmp(n, "/usr/lib/libc++abi.dylib"); }
/* libicucore: o ICU4C no disco (/usr/lib/libicucore.A.dylib, tools/build-icu.ps1, nomes sem versão como a da Apple) quando
 * ele existe; o Intl do JavaScriptCore no Claude Code (Bun) importa 184 funções dela. Sem o arquivo, o libSystem cobre só o
 * uidna_* (IDN do curl, posixmore.c), como antes. */
#define LIBICU_DISK "/usr/lib/libicucore.A.dylib"

static const char *arch_name(u32 cpu) {
    switch (cpu) {
    case 0x01000007: return "x86_64";
    case 0x0100000C: return "arm64";
    case 7: return "i386";
    case 12: return "arm";
    case 18: return "ppc";
    case 0x01000012: return "ppc64";
    }
    return "desconhecida";
}

/* escolhe o slice da arquitetura do processo (LS_CPU: x86_64, ou arm64 no tradutor) de um binário universal; lista as
 * arquiteturas em archs */
static bool pick_slice(const u8 *img, usize len, const u8 **out, usize *olen, char *archs, usize acap, char *err, usize errlen) {
    archs[0] = 0;
    u32 magic = be32_(img);
    if (magic == 0xCAFEBABE || magic == 0xCAFEBABF) {
        bool b64 = magic == 0xCAFEBABF;
        u32 n = be32_(img + 4);
        if (n == 0 || n > 32) { snprintf(err, errlen, "binário universal inválido"); return false; }
        const u8 *found = NULL;
        usize flen = 0;
        for (u32 i = 0; i < n; i++) {
            const u8 *a = img + 8 + i * (b64 ? 32 : 20);
            if ((usize)(a - img) + (b64 ? 32 : 20) > len) break;
            u32 cpu = be32_(a);
            u64 off = b64 ? be64_(a + 8) : be32_(a + 8), sz = b64 ? be64_(a + 16) : be32_(a + 12);
            usize al = strlen(archs);
            snprintf(archs + al, acap - al, "%s%s", al ? ", " : "", arch_name(cpu));
            if (cpu == LS_CPU && off < len && sz <= len - off) { found = img + off; flen = (usize)sz; }
        }
        if (!found) { snprintf(err, errlen, "sem versão %s (só %s)", arch_name(LS_CPU), archs); return false; }
        *out = found;
        *olen = flen;
        return true;
    }
    if (len >= 8 && rd32(img) == 0xFEEDFACF) {
        snprintf(archs, acap, "%s", arch_name(rd32(img + 4)));
        *out = img;
        *olen = len;
        return true;
    }
    snprintf(err, errlen, "não é um executável Mach-O de 64 bits");
    return false;
}

/* arquitetura de um processo novo pelo executável: x86_64 sempre que houver; arm64 (tradutor, docs/tradutor-arm64.md)
 * quando só há arm64/arm64e, ou quando pedem (MOS_ARCH=arm64 no ambiente, ou o arquivo A64RT_DIR/preferir-arm64) */
static int choose_arch(const u8 *img, usize len, bool force_arm64) {
    if (len < 8) return 0;
    u32 magic = be32_(img);
    if (magic == 0xCAFEBABE || magic == 0xCAFEBABF) {
        bool b64 = magic == 0xCAFEBABF, x86 = false, arm = false;
        u32 n = be32_(img + 4);
        for (u32 i = 0; i < n && i < 32; i++) {
            const u8 *a = img + 8 + i * (b64 ? 32 : 20);
            if ((usize)(a - img) + (b64 ? 32 : 20) > len) break;
            u32 cpu = be32_(a);
            if (cpu == 0x01000007) x86 = true;
            if (cpu == 0x0100000C) arm = true;
        }
        return arm && (force_arm64 || !x86) ? 1 : 0;
    }
    return rd32(img) == 0xFEEDFACF && rd32(img + 4) == 0x0100000C ? 1 : 0;
}

static bool parse(mach_t *mo, const u8 *m, usize len, bool dylib, char *err, usize errlen) {
    memset(mo, 0, sizeof *mo);
    mo->m = m;
    mo->len = len;
    if (len < 32 || rd32(m) != 0xFEEDFACF) { snprintf(err, errlen, "cabeçalho Mach-O inválido"); return false; }
    if (rd32(m + 4) != LS_CPU) { snprintf(err, errlen, "arquitetura %s não suportada (o processo é %s)", arch_name(rd32(m + 4)), arch_name(LS_CPU)); return false; }
    u32 ftype = rd32(m + 12), ncmds = rd32(m + 16), sizeofcmds = rd32(m + 20);
    mo->flags = rd32(m + 24);
    mo->ftype = ftype;
    if (!dylib && ftype != MH_EXECUTE) { snprintf(err, errlen, "não é um executável (tipo Mach-O %u)", ftype); return false; }
    if (dylib && ftype != MH_DYLIB && ftype != MH_BUNDLE) { snprintf(err, errlen, "não é uma biblioteca dinâmica (tipo Mach-O %u)", ftype); return false; }
    if (32 + (u64)sizeofcmds > len) { snprintf(err, errlen, "comandos de carga fora do arquivo"); return false; }
    mo->vmin = ~0ULL;
    const u8 *c = m + 32, *end = m + 32 + sizeofcmds;
    for (u32 i = 0; i < ncmds; i++) {
        if (c + 8 > end) { snprintf(err, errlen, "comando de carga truncado"); return false; }
        u32 cmd = rd32(c), csz = rd32(c + 4);
        if (csz < 8 || c + csz > end) { snprintf(err, errlen, "comando de carga inválido"); return false; }
        switch (cmd) {
        case LC_SEGMENT_64: {
            if (csz < 72) break;
            u64 vmaddr = rd64(c + 24), vmsize = rd64(c + 32), fileoff = rd64(c + 40), filesize = rd64(c + 48);
            u32 initprot = rd32(c + 60), nsects = rd32(c + 64);
            /* os índices de segmento do dyld_info contam todos os LC_SEGMENT_64, inclusive o
             * __PAGEZERO: ele entra na lista, mas não na imagem */
            bool zero = !vmaddr && !initprot;
            if (mo->nsegs >= MAX_SEGS) { snprintf(err, errlen, "segmentos demais"); return false; }
            if (fileoff > len || filesize > len - fileoff || filesize > vmsize) { snprintf(err, errlen, "segmento fora do arquivo"); return false; }
            seg_t *s = &mo->segs[mo->nsegs++];
            memcpy(s->name, c + 8, 16);
            s->name[16] = 0;
            s->vmaddr = vmaddr; s->vmsize = vmsize; s->fileoff = fileoff; s->filesize = zero ? 0 : filesize;
            s->zero = zero;
            s->prot = initprot;
            if (zero) break;
            if (vmaddr < mo->vmin) mo->vmin = vmaddr;
            if (vmaddr + vmsize > mo->vmax) mo->vmax = vmaddr + vmsize;
            /* Processo arm64: a imagem cobre a página guest de 16 KiB do último byte do segmento.
             * O vmaddr dos binários da Apple já é múltiplo de 16 KiB; o slide preserva isso porque
             * a base da imagem também é. Não muda o vmaddr usado nos fixups. x86_64 não entra aqui. */
            if (ls_cur && vmsize) {
                u64 page_hi = A64_GUEST_PAGE_END(vmaddr, vmsize);
                if (page_hi > mo->vmax) mo->vmax = page_hi;
            }
            for (u32 k = 0; k < nsects && 72 + (k + 1) * 80 <= csz; k++) {
                const u8 *sc = c + 72 + k * 80;
                u32 type = rd32(sc + 64) & 0xFF;
                u64 addr = rd64(sc + 32), size = rd64(sc + 40);
                if (size && (!mo->first_sect || addr < mo->first_sect)) mo->first_sect = addr;
                {   /* S_ATTR_PURE_INSTRUCTIONS | S_ATTR_SOME_INSTRUCTIONS; zerofill: S_ZEROFILL, S_GB_ZEROFILL, S_THREAD_LOCAL_ZEROFILL */
                    u32 sflags = rd32(sc + 64);
                    bool code = (sflags & 0x80000400u) != 0, zf = type == 0x01 || type == 0x0C || type == 0x12;
                    if (code && size && mo->ncsect < 16) { mo->csect[mo->ncsect].addr = addr; mo->csect[mo->ncsect++].size = size; }
                    /* pclntab do Go: fluxo de bytes e offsets (os falsos positivos ali davam "index out of range" no
                     * funcfile); só o textStart do pcHeader (offset 24) é ponteiro, e ele entra pelo pcln_text. O
                     * __noptrdata fica: "sem ponteiros" é só para o coletor; ele guarda a lista de inittask do doInit. */
                    bool pcln = !strncmp((const char *)sc, "__lrodata_gopcln", 16) || !strncmp((const char *)sc, "__gopclntab", 16);
                    if (pcln && size >= 32) mo->pcln_text = addr + 24;
                    bool noptr = pcln;
                    if (!code && !zf && !noptr && size >= 8 && strncmp((const char *)c + 8, "__LINKEDIT", 16) && mo->ndsect < 48) {
                        mo->dsect[mo->ndsect].addr = addr;
                        mo->dsect[mo->ndsect].strict = !strncmp((const char *)sc, "__go_type", 16);
                        mo->dsect[mo->ndsect++].size = size;
                    }
                }
                if (type == 0x09) { mo->init_ptrs_addr = addr; mo->init_ptrs_count = size / 8; }
                if (type == 0x16) { mo->init_offs_addr = addr; mo->init_offs_count = size / 4; }
                if (type == 0x11 || type == 0x12) {               /* dados TLS (modelo) */
                    if (!mo->tlv_hi || addr < mo->tlv_lo) mo->tlv_lo = addr;
                    if (addr + size > mo->tlv_hi) mo->tlv_hi = addr + size;
                    if (type == 0x11 && addr + size > mo->tlv_init_end) mo->tlv_init_end = addr + size;
                }
                if (type == 0x13 && mo->ntlv_desc < 8) {          /* descritores */
                    mo->tlv_desc[mo->ntlv_desc] = addr;
                    mo->tlv_desc_size[mo->ntlv_desc++] = size;
                }
                if (type == 0x15) mo->has_tlv = true;             /* inicializadores TLS: não suportados */
                {
                    static const char *names[] = {"", "__objc_classlist", "__objc_catlist", "__objc_selrefs", "__objc_classrefs",
                                                  "__objc_superrefs", "__objc_protolist", "__cfstring", "__objc_nlclslist",
                                                  "__objc_nlcatlist", "__objc_protorefs", "__objc_catlist2",
                                                  /* 12..16: literais constantes do Xcode 15+ (@{...}, @[...], @42, @1.5) */
                                                  "__objc_dictobj", "__objc_arrayobj", "__objc_intobj", "__objc_doubleobj", "__objc_floatobj"};
                    char sn[17];
                    memcpy(sn, sc, 16);
                    sn[16] = 0;
                    for (u64 kd = 1; kd < ARRAY_LEN(names); kd++)
                        if (!strcmp(sn, names[kd]) && mo->nobjc < 24) {
                            mo->objc[mo->nobjc].addr = addr;
                            mo->objc[mo->nobjc].size = size;
                            mo->objc[mo->nobjc++].kind = kd;
                        }
                }
            }
            break;
        }
        case LC_LOAD_DYLIB: case LC_LOAD_WEAK_DYLIB: case LC_REEXPORT_DYLIB: case LC_LAZY_LOAD_DYLIB: case LC_LOAD_UPWARD_DYLIB: {
            u32 off = rd32(c + 8);
            if (off >= csz || mo->ndylibs >= MAX_DYLIBS) break;
            mo->dylib_weak[mo->ndylibs] = cmd == LC_LOAD_WEAK_DYLIB;
            mo->dylibs[mo->ndylibs++] = (const char *)c + off;
            break;
        }
        case LC_DYLD_INFO: case LC_DYLD_INFO_ONLY: {
            mo->dyld_info = c;
            u32 eo = rd32(c + 40), es = rd32(c + 44);
            if (es && (u64)eo + es <= len) { mo->trie = m + eo; mo->triesz = es; }
            break;
        }
        case LC_DYLD_EXPORTS_TRIE: {
            u32 eo = rd32(c + 8), es = rd32(c + 12);
            if (es && (u64)eo + es <= len) { mo->trie = m + eo; mo->triesz = es; }
            break;
        }
        case LC_RPATH: {
            u32 off = rd32(c + 8);
            if (off < csz && mo->nrpaths < MAX_RPATHS) mo->rpaths[mo->nrpaths++] = (const char *)c + off;
            break;
        }
        case LC_DYLD_CHAINED_FIXUPS: mo->chained = c; break;
        case 0x26: {   /* LC_FUNCTION_STARTS */
            u32 fo = rd32(c + 8), fs = rd32(c + 12);
            if (fs && (u64)fo + fs <= len) { mo->fstarts = m + fo; mo->fstarts_size = fs; }
            break;
        }
        case LC_MAIN: mo->entryoff = rd64(c + 8); mo->has_main = true; break;
        case LC_UNIXTHREAD: if (csz >= 16 && rd32(c + 8) == (ls_cur ? 6u : 4u)) mo->has_thread = true; break;   /* x86_THREAD_STATE64 / ARM_THREAD_STATE64 */
        case LC_VERSION_MIN_MACOSX: mo->minos = rd32(c + 8); break;
        case LC_BUILD_VERSION: mo->minos = rd32(c + 12); break;
        }
        c += csz;
    }
    if (mo->vmin == ~0ULL || mo->vmax <= mo->vmin) { snprintf(err, errlen, "nenhum segmento"); return false; }
    return true;
}

static void dylib_short(const char *path, char *out, usize cap) {
    /* "/System/Library/Frameworks/AppKit.framework/Versions/C/AppKit" -> "AppKit.framework" */
    const char *fw = strstr(path, ".framework");
    if (fw) {
        const char *s = fw;
        while (s > path && s[-1] != '/') s--;
        snprintf(out, cap, "%.*s.framework", (int)(fw - s), s);
        return;
    }
    const char *b = strrchr(path, '/');
    snprintf(out, cap, "%s", b ? b + 1 : path);
}

/* ---- leitura de opcodes ULEB/SLEB */
static u64 uleb(const u8 **p, const u8 *end) {
    u64 v = 0;
    int sh = 0;
    while (*p < end) {
        u8 b = *(*p)++;
        if (sh < 64) v |= (u64)(b & 0x7F) << sh;
        sh += 7;
        if (!(b & 0x80)) break;
    }
    return v;
}
static i64 sleb(const u8 **p, const u8 *end) {
    i64 v = 0;
    int sh = 0;
    u8 b = 0;
    while (*p < end) {
        b = *(*p)++;
        if (sh < 64) v |= (i64)(b & 0x7F) << sh;
        sh += 7;
        if (!(b & 0x80)) break;
    }
    if (sh < 64 && (b & 0x40)) v |= -((i64)1 << sh);
    return v;
}

/* ================================================================ arquivos das dependências */
static void path_dir(const char *p, char *out, usize cap);
static void path_norm(char *p, usize cap);
static const u8 *dep_follow(const char *path, usize *len, int depth);
#ifdef MOS_HOST_TEST
const u8 *host_read_file(const char *path, usize *len);   /* tools/machotest.c */
static const u8 *dep_read_raw(const char *path, usize *len) { return host_read_file(path, len); }
static bool dyld_tolerant(void) { return getenv("MACHOTEST_TOLERANT") != NULL; }
#else
#include "vfs.h"
static const u8 *dep_read_raw(const char *path, usize *len) {
    vnode_t *v = vfs_lookup(path);
    if (!v || v->dir || !v->data || !v->size) return NULL;
    *len = v->size;
    return v->data;
}
/* modo experimental: se este arquivo existir, bibliotecas do sistema que o mOS não tem não impedem
 * o app de abrir; as funções delas viram stubs que encerram o processo dizendo o que faltou */
#define DYLD_TOLERANT_FLAG "/Sistema/Biblioteca/dyld-experimental"
static bool dyld_tolerant(void) { return vfs_lookup(DYLD_TOLERANT_FLAG) != NULL; }
#endif
static const u8 *dep_read(const char *path, usize *len) { return dep_follow(path, len, 0); }

/* ================================================================ estado do carregamento */
typedef struct {
    mach_t mo;
    char path[320];
    int idx;
    u64 base;               /* endereço final de mo.vmin */
    i64 slide;
    u64 hdr;                /* endereço final do cabeçalho Mach-O */
    i16 dep[MAX_DYLIBS];    /* imagem carregada para cada dylib pedida (-1: sistema / ausente) */
    bool visited;
    int refs;               /* dlopen/dlclose (o mOS nunca descarrega a imagem) */
    u8 *own;                /* cópia própria dos comandos de carga + export trie (image_persist) */
    /* __TEXT compartilhado entre links diferentes (tcache, mais abaixo): só durante a montagem */
    bool text_shared;       /* o __TEXT já está mapeado do tcache: image_fix não o copia */
    void *tc;               /* a entrada do tcache usada */
    u64 tlo, tnp, tfileoff; /* endereço final do __TEXT, páginas e deslocamento no arquivo (tnp = 0: não compartilha) */
} image_t;

typedef struct {
    image_t *im[MAX_IMAGES];
    int n;
    int order[MAX_IMAGES], norder;     /* dependências antes de quem as usa; o executável por último */
    /* símbolos ausentes viram stubs */
    char **missing;
    u32 nmissing, capmissing;
    u64 stub_base, end;
    u64 unresolved_fn;
    /* bibliotecas do sistema que o mOS não tem (as 4 primeiras por nome) */
    char absent[400];
    int nabsent;
    char archs[96];
    /* inicializadores (endereços finais) e seções ObjC de todas as imagens */
    u64 *inits;
    u32 ninits, capinits;
    struct { u64 addr, size, kind; } objc[MAX_OBJC];
    int nobjc;
    bool report_only, tolerant;
    bool tlv_per_thread;    /* o libSystem tem cx_tlv_bootstrap: TLS por thread */
    char *err;
    usize errlen;
    /* carga em tempo de execução (dlopen): o grafo fica vivo em process_t.dyld depois do macho_load */
    int first_new;          /* primeira imagem da carga atual (registros TLS da área de stubs dela) */
    char lasterr[240];      /* último erro do dlopen (dlerror) */
    int arch;               /* 0 = x86_64, 1 = arm64 (tradutor): libSystem, slices e stubs desta arquitetura */
} link_t;

typedef struct {
    link_t *K;
    image_t *I;
    mach_t *mo;
    u8 *img;                /* imagem montada em memória do kernel (NULL no modo relatório) */
    u64 size;               /* vmax - vmin */
    i64 slide;              /* endereço final = vmaddr + slide */
    bool report_only;
    char *err;
    usize errlen;
} load_t;

static u64 *slot_at(load_t *L, u64 vmaddr) {
    if (!L->img || vmaddr < L->mo->vmin || vmaddr + 8 > L->mo->vmax) return NULL;
    return (u64 *)(L->img + (vmaddr - L->mo->vmin));
}

/* índice do stub para um símbolo ausente (cria se preciso) */
/* a área de stubs começa com uma página: 64 bytes zerados (constantes ausentes apontam para eles), os
 * registros de TLS por imagem {modelo, tamanho inicializado, tamanho total, índice} e int3 no resto */
#define ZERO_AREA 0x1000
#define TLV_RECORD(idx) (64 + (u64)(idx) * 32)

/* constantes (dados) pelo nome, nas convenções da Apple: _kCFxxx, NSxxxKey, xxxNotification... Um
 * stub de código no lugar de uma constante faz o app ler as instruções do stub como ponteiro. */
static bool data_like(const char *n) {
    if (n[0] == '_') n++;
    if (n[0] == 'k' && n[1] >= 'A' && n[1] <= 'Z') return true;
    if (!strncmp(n, "OBJC_EHTYPE_$_", 14) || !strncmp(n, "OBJC_IVAR_$_", 12)) return true;
    static const char *pre[] = {"NSAccessibility", "NSAppearanceName", "NSPasteboardType", "NSPasteboardName", "NSPrint",
                                "NSCalendarIdentifier", "NSLocale", "NSImageHint", "CATransform3DIdentity", "CGAffineTransformIdentity",
                                "CGRectNull", "CGRectZero", "CGRectInfinite", "CGPointZero", "CGSizeZero", "NSZeroRect", "NSZeroPoint", "NSZeroSize"};
#define WIDE_CONSTS 1      /* prefixos/sufixos extras (AppKit) */
    static const char *suf[] = {"Key", "Keys", "Notification", "Domain", "Exception", "Constant", "Identifier", "Attribute",
                                "AttributeName", "Mode", "Option", "Version", "VersionString", "VersionNumber"
#if WIDE_CONSTS
                                , "Type", "Role", "Subrole", "ColorSpace", "Trait", "PboardType", "DocumentType"
#endif
                                };
    usize l = strlen(n);
    if (l < 3 || n[0] < 'A' || n[0] > 'Z') return false;   /* constantes públicas começam com maiúscula */
    /* funções com nome de constante: CFBundleGetValueForInfoDictionaryKey, CFNotificationCenterPostNotification... */
    static const char *verbs[] = {"Get", "Copy", "Create", "Set", "Add", "Remove", "Post", "Register", "Make", "Apply",
                                  "Find", "Contains", "Append", "Insert", "Replace", "Perform", "Observe"};
    for (u32 i = 0; i < ARRAY_LEN(verbs); i++) {
        const char *v = strstr(n, verbs[i]);
        if (v && v > n && v[strlen(verbs[i])] >= 'A' && v[strlen(verbs[i])] <= 'Z') return false;
    }
    /* NSAccessibility tem funções com nome de constante (UnignoredChildren, RoleDescription...):
     * só são constantes as que terminam num destes */
    if (!strncmp(n, "NSAccessibility", 15)) {
        static const char *acc[] = {"Role", "Subrole", "Action", "Value", "Attribute", "Notification"};
        for (u32 i = 0; i < ARRAY_LEN(acc); i++) {
            usize al = strlen(acc[i]);
            if (l > al && !strcmp(n + l - al, acc[i])) return true;
        }
        return false;
    }
    for (u32 i = 0; WIDE_CONSTS && i < ARRAY_LEN(pre); i++) if (!strncmp(n, pre[i], strlen(pre[i]))) return true;
    for (u32 i = 0; i < ARRAY_LEN(suf); i++) {
        usize sl = strlen(suf[i]);
        if (l > sl && !strcmp(n + l - sl, suf[i])) return true;
    }
    return false;
}

static u64 missing_stub(link_t *K, const char *name) {
    for (u32 i = 0; i < K->nmissing; i++) if (!strcmp(K->missing[i], name)) return K->stub_base + ZERO_AREA + i * 32;
    if (K->nmissing == K->capmissing) {
        u32 nc = K->capmissing ? K->capmissing * 2 : 32;
        char **p = krealloc(K->missing, nc * sizeof(char *));
        if (!p) return 0;
        K->missing = p;
        K->capmissing = nc;
    }
    K->missing[K->nmissing] = strdup(name);
    return K->stub_base + ZERO_AREA + (K->nmissing++) * 32;
}

/* ---- export trie (formato do dyld): devolve o início da informação terminal do símbolo */
static const u8 *trie_find(const u8 *t, u32 n, const char *s) {
    const u8 *end = t + n, *p = t;
    for (int guard = 0; guard < 4096 && p < end; guard++) {
        u64 tsz = uleb(&p, end);
        if (!*s) return tsz ? p : NULL;
        const u8 *ch = p + tsz;
        if (ch >= end) return NULL;
        u8 nch = *ch++;
        const u8 *next = NULL;
        for (u8 i = 0; i < nch && ch < end; i++) {
            const char *q = s;
            const u8 *e = ch;
            while (e < end && *e && *e == (u8)*q) { e++; q++; }
            bool match = e < end && !*e;
            while (ch < end && *ch) ch++;
            ch++;
            u64 off = uleb(&ch, end);
            if (match) { s = q; next = t + off; break; }
        }
        if (!next || next >= end) return NULL;
        p = next;
    }
    return NULL;
}

static u64 image_export(link_t *K, int idx, const char *name, int depth) {
    if (idx < 0 || idx >= K->n || depth > 8) return 0;
    image_t *I = K->im[idx];
    if (!I->mo.trie) return 0;
    const u8 *end = I->mo.trie + I->mo.triesz;
    const u8 *p = trie_find(I->mo.trie, I->mo.triesz, name);
    if (!p) return 0;
    u64 flags = uleb(&p, end);
    if (flags & 0x08) {                                   /* REEXPORT: vem de outra biblioteca */
        u64 ord = uleb(&p, end);
        const char *nm = (p < end && *p) ? (const char *)p : name;
        if (ord >= 1 && ord <= (u64)I->mo.ndylibs && I->dep[ord - 1] >= 0) return image_export(K, I->dep[ord - 1], nm, depth + 1);
        return ls_lookup(nm);
    }
    if (flags & 0x10) return I->hdr + uleb(&p, end);      /* STUB_AND_RESOLVER: usa o stub */
    u64 v = uleb(&p, end);
    if ((flags & 3) == 2) return v;                       /* ABSOLUTE */
    return I->hdr + v;
}

/* ord: ordinal da biblioteca (1..n) ou especial (0 a própria imagem, -1 o executável, -2 busca plana) */
static u64 resolve(load_t *L, int ord, const char *name, bool weak) {
    link_t *K = L->K;
    image_t *I = L->I;
    u64 a = 0;
    if (K->tlv_per_thread && !strcmp(name, "__tlv_bootstrap")) return ls_lookup("_cx_tlv_bootstrap");
    if (ord > 0 && ord <= I->mo.ndylibs && I->dep[ord - 1] >= 0) a = image_export(K, I->dep[ord - 1], name, 0);
    else if (ord == 0) a = image_export(K, I->idx, name, 0);
    else if (ord == -1) a = image_export(K, 0, name, 0);
    if (!a) a = ls_lookup(name);
    for (int i = 0; i < K->n && !a; i++) a = image_export(K, i, name, 0);
    if (a) return a;
    if (weak) return 0;
    u64 stub = missing_stub(K, name);
    /* classe ObjC que o mOS não tem: um stub de código não serve como classe (o runtime leria o isa
     * de dentro dele). Usa NSObject no lugar: subclasses continuam válidas e mensagens que só a classe
     * original entende caem no tratamento de seletor desconhecido do runtime. O nome fica na lista
     * de ausentes. */
    if (!strncmp(name, "_OBJC_CLASS_$_", 14)) { u64 o = ls_lookup("_OBJC_CLASS_$_NSObject"); if (o) return o; }
    if (!strncmp(name, "_OBJC_METACLASS_$_", 18)) { u64 o = ls_lookup("_OBJC_METACLASS_$_NSObject"); if (o) return o; }
    if (stub && data_like(name)) return K->stub_base;     /* constante ausente: lê 0 / nil */
    return stub;
}

static bool do_bind(load_t *L, u64 vmaddr, const char *name, bool weak, i64 addend, int ord) {
    u64 v = resolve(L, ord, name, weak);
    if (L->report_only) return true;
    u64 *s = slot_at(L, vmaddr);
    if (!s) { snprintf(L->err, L->errlen, "bind fora da imagem (%s)", name); return false; }
    *s = v ? v + (u64)addend : 0;
    return true;
}

/* ---- dyld_info clássico */
static bool run_rebase(load_t *L, const u8 *p, const u8 *end) {
    mach_t *mo = L->mo;
    int seg = 0;
    u64 off = 0;
    if (L->report_only) return true;
    while (p < end) {
        u8 b = *p++, op = b & 0xF0, imm = b & 0x0F;
        u64 count = 0, skip = 0;
        switch (op) {
        case 0x00: return true;
        case 0x10: break;                                       /* SET_TYPE_IMM */
        case 0x20: seg = imm; off = uleb(&p, end); break;       /* SET_SEGMENT_AND_OFFSET_ULEB */
        case 0x30: off += uleb(&p, end); break;
        case 0x40: off += (u64)imm * 8; break;
        case 0x50: count = imm; skip = 0; goto rebase;
        case 0x60: count = uleb(&p, end); skip = 0; goto rebase;
        case 0x70: count = 1; skip = uleb(&p, end); goto rebase;
        case 0x80: count = uleb(&p, end); skip = uleb(&p, end); goto rebase;
        default: snprintf(L->err, L->errlen, "opcode de rebase desconhecido 0x%x", b); return false;
        }
        continue;
    rebase:
        if (seg >= mo->nsegs) { snprintf(L->err, L->errlen, "rebase em segmento inválido"); return false; }
        for (u64 i = 0; i < count; i++) {
            u64 *s = slot_at(L, mo->segs[seg].vmaddr + off);
            if (!s) { snprintf(L->err, L->errlen, "rebase fora da imagem"); return false; }
            /* valor fora da imagem = símbolo absoluto codificado como rebase: não recebe slide (ver run_chained) */
            if (*s >= mo->vmin && *s <= mo->vmax) *s += (u64)L->slide;
            /* arm64: o byte alto é ignorado pelo hardware (TBI) e carrega marcas, como o bit 63 do nome do RTTI não único
             * da libc++ da Apple: o slide vai nos 56 bits baixos e a marca fica */
            else if (L->K->arch && (*s & 0x00FFFFFFFFFFFFFFULL) >= mo->vmin && (*s & 0x00FFFFFFFFFFFFFFULL) <= mo->vmax) *s += (u64)L->slide;
            off += 8 + skip;
        }
    }
    return true;
}

static bool run_bind(load_t *L, const u8 *p, const u8 *end, bool lazy) {
    mach_t *mo = L->mo;
    int seg = 0, ordinal = 0;
    u64 off = 0;
    i64 addend = 0;
    const char *sym = "";
    bool weak = false;
    while (p < end) {
        u8 b = *p++, op = b & 0xF0, imm = b & 0x0F;
        u64 count, skip;
        switch (op) {
        case 0x00: if (!lazy) return true; break;               /* DONE (lazy: separa entradas) */
        case 0x10: ordinal = imm; break;
        case 0x20: ordinal = (int)uleb(&p, end); break;
        case 0x30: ordinal = imm ? (int)(i8)(0xF0 | imm) : 0; break;
        case 0x40:
            sym = (const char *)p;
            weak = (imm & 1) != 0;
            while (p < end && *p) p++;
            p++;
            break;
        case 0x50: break;
        case 0x60: addend = sleb(&p, end); break;
        case 0x70: seg = imm; off = uleb(&p, end); break;
        case 0x80: off += uleb(&p, end); break;
        case 0x90: count = 1; skip = 0; goto bind;
        case 0xA0: count = 1; skip = uleb(&p, end); goto bind;
        case 0xB0: count = 1; skip = (u64)imm * 8; goto bind;
        case 0xC0: count = uleb(&p, end); skip = uleb(&p, end); goto bind;
        default: snprintf(L->err, L->errlen, "opcode de bind desconhecido 0x%x", b); return false;
        }
        continue;
    bind:
        if (seg >= mo->nsegs) { snprintf(L->err, L->errlen, "bind em segmento inválido"); return false; }
        if (ordinal > 0 && ordinal <= mo->ndylibs && mo->dylib_weak[ordinal - 1]) weak = true;
        for (u64 i = 0; i < count; i++) {
            if (!do_bind(L, mo->segs[seg].vmaddr + off, sym, weak, addend, ordinal)) return false;
            off += 8 + skip;
            if (L->report_only) { off += (count - i - 1) * (8 + skip); break; }
        }
    }
    return true;
}

/* ---- chained fixups (macOS 11+) */
static bool run_chained(load_t *L) {
    mach_t *mo = L->mo;
    u32 dataoff = rd32(mo->chained + 8), datasize = rd32(mo->chained + 12);
    if ((u64)dataoff + datasize > mo->len || datasize < 28) { snprintf(L->err, L->errlen, "chained fixups fora do arquivo"); return false; }
    const u8 *h = mo->m + dataoff, *hend = h + datasize;
    u32 starts_off = rd32(h + 4), imports_off = rd32(h + 8), symbols_off = rd32(h + 12);
    u32 imports_count = rd32(h + 16), imports_format = rd32(h + 20);
    if (rd32(h) != 0 || starts_off >= datasize || imports_off > datasize || symbols_off > datasize) {
        snprintf(L->err, L->errlen, "cabeçalho de chained fixups inválido");
        return false;
    }
    /* resolve os imports uma vez */
    u64 *targets = imports_count ? kcalloc(imports_count, sizeof(u64)) : NULL;
    if (imports_count && !targets) { snprintf(L->err, L->errlen, "sem memória"); return false; }
    usize isz = imports_format == 1 ? 4 : imports_format == 2 ? 8 : 16;
    for (u32 i = 0; i < imports_count; i++) {
        const u8 *im = h + imports_off + i * isz;
        if (im + isz > hend) { kfree(targets); snprintf(L->err, L->errlen, "import fora da tabela"); return false; }
        int lib;
        u32 name_off;
        bool weak;
        i64 addend = 0;
        if (imports_format == 3) {
            u16 l = rd16(im);
            lib = l >= 0xFFF0 ? (int)(i16)l : (int)l;
            weak = (rd16(im + 2) & 1) != 0; name_off = rd32(im + 4); addend = (i64)rd64(im + 8);
        } else {
            u32 v = rd32(im), l = v & 0xFF;
            lib = l >= 0xF0 ? (int)(i8)l : (int)l;
            weak = (v >> 8) & 1; name_off = v >> 9;
            if (imports_format == 2) addend = (i32)rd32(im + 4);
        }
        if (lib > 0 && lib <= mo->ndylibs && mo->dylib_weak[lib - 1]) weak = true;
        const char *name = (const char *)h + symbols_off + name_off;
        if ((const u8 *)name >= hend) { kfree(targets); snprintf(L->err, L->errlen, "nome de import inválido"); return false; }
        u64 a = resolve(L, lib, name, weak);
        targets[i] = a ? a + (u64)addend : 0;
    }
    const u8 *si = h + starts_off;
    u32 seg_count = rd32(si);
    bool ok = true;
    for (u32 s = 0; ok && s < seg_count && s < (u32)mo->nsegs + 1; s++) {
        u32 so = rd32(si + 4 + s * 4);
        if (!so) continue;
        const u8 *ss = si + so;
        if (ss + 22 > hend) { ok = false; break; }
        u16 page_size = rd16(ss + 4), fmt = rd16(ss + 6), page_count = rd16(ss + 20);
        u64 seg_off = rd64(ss + 8);
        /* 2 = DYLD_CHAINED_PTR_64, 6 = _64_OFFSET (passo de 4 bytes); arm64e (passo de 8): 1 = ARM64E (rebase por vmaddr),
         * 12 = ARM64E_USERLAND, 13 = ARM64E_USERLAND24 (rebase por deslocamento, bind com ordinal de 24 bits). Os
         * ponteiros autenticados viram ponteiros puros: o PAC do tradutor é a identidade (docs/tradutor-arm64.md).
         * Nenhum binário dos apps-alvo neste repositório pede um formato fora desta lista. */
        bool e64 = fmt == 1 || fmt == 12 || fmt == 13;
        if (fmt != 2 && fmt != 6 && !e64) { snprintf(L->err, L->errlen, "formato de ponteiro encadeado %u não suportado", fmt); ok = false; break; }
        if (L->report_only) continue;
        for (u16 pg = 0; ok && pg < page_count; pg++) {
            u16 start = rd16(ss + 22 + pg * 2);
            if (start == 0xFFFF) continue;
            u64 va = mo->vmin + seg_off + (u64)pg * page_size + start;
            for (int guard = 0; guard < 100000; guard++) {
                u64 *slot = slot_at(L, va);
                if (!slot) { snprintf(L->err, L->errlen, "fixup fora da imagem"); ok = false; break; }
                u64 v = *slot, next;
                if (e64) {
                    bool auth = v >> 63, bind = (v >> 62) & 1;
                    next = (v >> 51) & 0x7FF;
                    if (bind) {
                        u32 ord = (u32)(fmt == 13 ? (v & 0xFFFFFF) : (v & 0xFFFF));
                        i64 add = auth ? 0 : ((i64)(((v >> 32) & 0x7FFFF) << 45) >> 45);   /* addend de 19 bits com sinal */
                        if (ord >= imports_count) { snprintf(L->err, L->errlen, "ordinal de import inválido"); ok = false; break; }
                        *slot = targets[ord] ? targets[ord] + (u64)add : 0;
                    } else if (auth) {
                        *slot = L->I->hdr + (v & 0xFFFFFFFFULL);                         /* runtimeOffset */
                    } else {
                        u64 target = v & 0x7FFFFFFFFFFULL, high8 = (v >> 43) & 0xFF;
                        *slot = (fmt == 1 ? target + (u64)L->slide : L->I->hdr + target) | (high8 << 56);
                    }
                    if (!next) break;
                    va += next * 8;
                    continue;
                }
                next = (v >> 51) & 0xFFF;
                if (v >> 63) {
                    u32 ord = (u32)(v & 0xFFFFFF);
                    i64 add = (i64)((v >> 24) & 0xFF);
                    if (ord >= imports_count) { snprintf(L->err, L->errlen, "ordinal de import inválido"); ok = false; break; }
                    *slot = targets[ord] ? targets[ord] + (u64)add : 0;
                } else {
                    /* DYLD_CHAINED_PTR_64: alvo é vmaddr; _64_OFFSET: deslocamento a partir do cabeçalho.
                     * Alvo fora da imagem = valor absoluto que o linker do zig codificou como rebase (ex.:
                     * __swiftImmortalRefCount = 0x80000004ffffffff no contador dos objetos estáticos do Swift):
                     * vai como está, sem slide (o ld64 da Apple nem gera rebase para símbolo absoluto). */
                    u64 target = v & 0xFFFFFFFFFULL, high8 = (v >> 36) & 0xFF;
                    bool inside = fmt == 2 ? (target >= mo->vmin && target <= mo->vmax) : target <= mo->vmax - mo->vmin;
                    u64 addr = !inside ? target : fmt == 2 ? target + (u64)L->slide : L->I->hdr + target;
                    *slot = addr | (high8 << 56);
                }
                if (!next) break;
                va += next * 4;
            }
        }
    }
    kfree(targets);
    return ok;
}

/* Executável sem MH_PIE (ex.: o language_server do Antigravity, Go com cgo): foi feito para 0x100000000, mas o mOS
 * põe toda imagem em IMAGE_BASE, e o dyld_info dele só traz 4 rebases. O código x86-64 do Go e do clang é relativo ao
 * RIP (o objdump não acha imediatos nem deslocamentos absolutos dentro da imagem), então o que fica errado são os
 * ponteiros absolutos nos dados: __mod_init_func, a moduledata e as tabelas de tipos do Go, vtables. Sem relocações
 * no arquivo, varremos as seções de dados e somamos o slide a cada palavra alinhada que cai dentro da imagem. Um
 * inteiro que por acaso caia em [vmin, vmax) — de 4,29 a ~4,5 bilhões — também mudaria; nos dados de um executável
 * isso é raro, e sem a varredura o programa nem chega ao main. */
/* v aponta para dentro da imagem, antes ou depois do slide? */
static bool in_img(const mach_t *mo, i64 slide, u64 v) {
    return (v >= mo->vmin && v < mo->vmax) || (v >= mo->vmin + (u64)slide && v < mo->vmax + (u64)slide);
}
static void u64_sift(u64 *a, u64 i, u64 n) {
    for (;;) {
        u64 c = 2 * i + 1;
        if (c >= n) return;
        if (c + 1 < n && a[c + 1] > a[c]) c++;
        if (a[i] >= a[c]) return;
        u64 t = a[i]; a[i] = a[c]; a[c] = t;
        i = c;
    }
}
static void u64_sort(u64 *a, u64 n) {   /* heapsort: sem recursão nem memória extra */
    for (u64 i = n / 2; i-- > 0;) u64_sift(a, i, n);
    for (u64 e = n; e-- > 1;) { u64 t = a[0]; a[0] = a[e]; a[e] = t; u64_sift(a, 0, e); }
}
static bool u64_has(const u64 *a, u64 n, u64 v) {
    u64 l = 0, h = n;
    while (l < h) { u64 m = (l + h) / 2; if (a[m] < v) l = m + 1; else h = m; }
    return l < n && a[l] == v;
}
static void nonpie_slide(load_t *L) {
    mach_t *mo = L->mo;
    u64 n = 0, rej = 0;
    /* o cabeçalho não é alvo de ponteiro de dados, e 0x100000000 (= vmin) é também o par de int32 {0, 1}: um contador
     * de 16 milhões no inicializador de dependências do C++ do language_server */
    u64 lo = mo->first_sect > mo->vmin ? mo->first_sect : mo->vmin + 1;
    /* inícios de função (LC_FUNCTION_STARTS, deslocamentos ULEB a partir do __TEXT): um valor que cai numa seção de
     * código, dentro de __go_type, só é ponteiro se for o início de uma função ou a borda de uma seção (nas outras seções
     * há tabelas de saltos do Go com endereços absolutos no meio das funções). Os descritores de tipo do Go têm pares
     * de int32 {nameOff, 1} que, lidos como 64 bits, caem no __text; deslizados, davam "name offset out of range". */
    u64 text = mo->vmin, nf = 0, *fs = NULL;
    for (int i = 0; i < mo->nsegs; i++) if (!strcmp(mo->segs[i].name, "__TEXT")) text = mo->segs[i].vmaddr;
    if (mo->fstarts) {
        const u8 *p = mo->fstarts, *e = p + mo->fstarts_size;
        u64 cnt = 0;
        for (const u8 *q = p; q < e;) { if (!uleb(&q, e)) break; cnt++; }
        fs = cnt ? kmalloc(cnt * 8) : NULL;
        u64 a = text;
        for (const u8 *q = p; fs && q < e && nf < cnt;) { u64 d = uleb(&q, e); if (!d) break; a += d; fs[nf++] = a; }
    }
    /* funcvals do Go dentro de __go_type (o Hasher de um mapType aponta para um {fn} isolado no meio dos nomes): o
     * endereço deles é alvo de outro ponteiro; o par falso {PkgPath, 1} do uncommontype nunca é (acha-se por offset).
     * Passe 1: todos os valores, ainda sem slide, que apontam para dentro das seções estritas. */
    u64 nt = 0, *tg = NULL;
    for (int pass = 0; pass < 2; pass++) {
        for (int i = 0; i < mo->ndsect; i++) {
            u64 a = (mo->dsect[i].addr + 7) & ~7ull, e = mo->dsect[i].addr + mo->dsect[i].size;
            for (; a + 8 <= e; a += 8) {
                u64 *s = slot_at(L, a);
                if (!s) continue;
                for (int k = 0; k < mo->ndsect; k++)
                    if (mo->dsect[k].strict && *s >= mo->dsect[k].addr && *s < mo->dsect[k].addr + mo->dsect[k].size) {
                        if (pass) tg[nt] = *s;
                        nt++;
                        break;
                    }
            }
        }
        if (pass) break;
        tg = nt ? kmalloc(nt * 8) : NULL;
        if (!tg) break;
        nt = 0;
    }
    if (tg) u64_sort(tg, nt); else nt = 0;
    for (int i = 0; i < mo->ndsect; i++) {
        u64 a = (mo->dsect[i].addr + 7) & ~7ull, e = mo->dsect[i].addr + mo->dsect[i].size;
        for (; a + 8 <= e; a += 8) {
            u64 *s = slot_at(L, a);
            if (!s || *s < lo || *s >= mo->vmax) continue;
            u64 v = *s;
            bool in_code = false, edge = false;
            for (int k = 0; k < mo->ncsect; k++)
                if (v >= mo->csect[k].addr && v <= mo->csect[k].addr + mo->csect[k].size) {
                    in_code = true;
                    if (v == mo->csect[k].addr || v == mo->csect[k].addr + mo->csect[k].size) edge = true;
                }
            if (in_code && !edge && fs && mo->dsect[i].strict) {   /* fora dos tipos há tabelas de saltos e o maxpc: meio do código */
                u64 l = 0, h = nf;                                   /* busca binária: fs está em ordem crescente */
                while (l < h) { u64 m2 = (l + h) / 2; if (fs[m2] < v) l = m2 + 1; else h = m2; }
                if ((l >= nf || fs[l] != v) && !u64_has(tg, nt, a)) { rej++; continue; }
                /* início de função, mas pode ser o par {PkgPath, Mcount=1|Xcount=0} do uncommontype. O equal de um _type
                 * vem seguido do gcdata (ponteiro ou 0); o falso, de {Moff, 0} (inteiro pequeno). O Hasher de um mapType
                 * vem depois de três ponteiros (Key, Elem, Group). Os anteriores já podem ter levado o slide. */
                u64 *nx = slot_at(L, a + 8), *p1 = slot_at(L, a - 8), *p2 = slot_at(L, a - 16), *p3 = slot_at(L, a - 24);
                bool next_ok = !nx || !*nx || (*nx >= mo->vmin && *nx < mo->vmax);
                bool three = p1 && p2 && p3 && in_img(mo, L->slide, *p1) && in_img(mo, L->slide, *p2) && in_img(mo, L->slide, *p3);
                if (!next_ok && !three && !u64_has(tg, nt, a)) { rej++; continue; }
            }
            *s += (u64)L->slide;
            n++;
        }
    }
    if (mo->pcln_text) { u64 *t = slot_at(L, mo->pcln_text); if (t && *t >= lo && *t < mo->vmax) { *t += (u64)L->slide; n++; } }
    if (fs) kfree(fs);
    if (tg) kfree(tg);
    kprintf("macho: executável sem PIE: %llu ponteiros deslizados em %d seções de dados (%llu valores no código recusados, %llu funções)\n",
            (unsigned long long)n, mo->ndsect, (unsigned long long)rej, (unsigned long long)nf);
}

static bool apply_dyld_info(load_t *L) {
    mach_t *mo = L->mo;
    if (mo->dyld_info && L->img && L->slide && mo->ftype == MH_EXECUTE && !(mo->flags & MH_PIE)) nonpie_slide(L);
    if (mo->dyld_info) {
        const u8 *d = mo->dyld_info;
        struct { u32 off, size; } part[4] = {{rd32(d + 8), rd32(d + 12)}, {rd32(d + 16), rd32(d + 20)}, {rd32(d + 24), rd32(d + 28)}, {rd32(d + 32), rd32(d + 36)}};
        for (int i = 0; i < 4; i++)
            if ((u64)part[i].off + part[i].size > mo->len) { snprintf(L->err, L->errlen, "dyld_info fora do arquivo"); return false; }
        const u8 *m = mo->m;
        if (part[0].size && !run_rebase(L, m + part[0].off, m + part[0].off + part[0].size)) return false;
        if (part[1].size && !run_bind(L, m + part[1].off, m + part[1].off + part[1].size, false)) return false;
        /* weak binds: só substituem se o libSystem tiver o símbolo — ignorados */
        if (part[3].size && !run_bind(L, m + part[3].off, m + part[3].off + part[3].size, true)) return false;
        return true;
    }
    if (mo->ftype == MH_EXECUTE && !(mo->flags & MH_PIE)) { snprintf(L->err, L->errlen, "executável sem informações de relocação (não é PIE)"); return false; }
    return true;
}

static void push_init(link_t *K, u64 a) {
    if (K->ninits == K->capinits) {
        u32 nc = K->capinits ? K->capinits * 2 : 64;
        u64 *n = krealloc(K->inits, nc * sizeof(u64));
        if (!n) return;
        K->inits = n;
        K->capinits = nc;
    }
    K->inits[K->ninits++] = a;
}

/* aplica os fixups de uma imagem em buf (NULL: só resolve os símbolos, para o relatório) e
 * recolhe inicializadores e seções ObjC */
static bool image_fix(link_t *K, image_t *I, u8 *buf) {
    mach_t *mo = &I->mo;
    load_t L = {.K = K, .I = I, .mo = mo, .img = buf, .size = mo->vmax - mo->vmin, .slide = I->slide,
                .report_only = !buf, .err = K->err, .errlen = K->errlen};
    if (buf)
        for (int i = 0; i < mo->nsegs; i++) {
            seg_t *s = &mo->segs[i];
            if (I->text_shared && !strcmp(s->name, "__TEXT")) continue;   /* páginas do tcache, já com o conteúdo do arquivo */
            if (!s->zero && s->filesize) memcpy(buf + (s->vmaddr - mo->vmin), mo->m + s->fileoff, s->filesize);
        }
    bool ok = mo->chained ? run_chained(&L) : apply_dyld_info(&L);
    if (!ok || !buf) return ok;
    /* TLS (thread_local): com o cx_tlv_bootstrap do libSystem, cada thread tem sua cópia do modelo e a
     * chave do descritor aponta para o registro da imagem (TLV_RECORD); sem ele, o modelo é a única
     * cópia (processos de uma thread) e a chave é o endereço do modelo */
    u64 key_val = K->tlv_per_thread ? K->stub_base + TLV_RECORD(I->idx - K->first_new) : mo->tlv_lo + (u64)I->slide;
    for (int i = 0; i < mo->ntlv_desc; i++)
        for (u64 a = mo->tlv_desc[i]; a + 24 <= mo->tlv_desc[i] + mo->tlv_desc_size[i]; a += 24) {
            u64 *key = slot_at(&L, a + 8);
            if (key) *key = key_val;
        }
    for (u64 k = 0; k < mo->init_ptrs_count; k++) {
        u64 *s = slot_at(&L, mo->init_ptrs_addr + k * 8);
        if (s && *s) push_init(K, *s);
    }
    for (u64 k = 0; k < mo->init_offs_count; k++) {
        u64 a = mo->init_offs_addr + k * 4;
        if (a < mo->vmin || a + 4 > mo->vmax) break;
        push_init(K, I->hdr + rd32(buf + (a - mo->vmin)));
    }
    for (int i = 0; i < mo->nobjc && K->nobjc < MAX_OBJC; i++) {
        K->objc[K->nobjc].addr = mo->objc[i].addr + (u64)I->slide;
        K->objc[K->nobjc].size = mo->objc[i].size;
        K->objc[K->nobjc++].kind = mo->objc[i].kind;
    }
    return true;
}

/* ---- caminhos (@executable_path, @loader_path, @rpath) */
static void path_dir(const char *p, char *out, usize cap) {
    const char *s = NULL;
    for (const char *q = p; *q; q++) if (*q == '/' || *q == '\\') s = q;
    if (!s) snprintf(out, cap, ".");
    else snprintf(out, cap, "%.*s", (int)(s - p), p);
}

/* normaliza "a/b/../c" -> "a/c" (aceita '\' dos caminhos do host) */
static void path_norm(char *p, usize cap) {
    char tmp[384];
    snprintf(tmp, sizeof tmp, "%s", p);
    for (char *c = tmp; *c; c++) if (*c == '\\') *c = '/';
    const char *comp[64];
    int nc = 0;
    bool abs = tmp[0] == '/';
    char *s = tmp;
    while (*s) {
        while (*s == '/') *s++ = 0;
        if (!*s) break;
        char *b = s;
        while (*s && *s != '/') s++;
        if (*s) *s++ = 0;
        if (!strcmp(b, ".")) continue;
        if (!strcmp(b, "..") && nc > 0 && strcmp(comp[nc - 1], "..")) { nc--; continue; }
        if (nc < 64) comp[nc++] = b;
    }
    usize o = 0;
    if (abs && cap > 1) p[o++] = '/';
    for (int i = 0; i < nc && o + 1 < cap; i++) {
        int w = snprintf(p + o, cap - o, "%s%s", i ? "/" : "", comp[i]);
        if (w < 0) break;
        o += (usize)w;
    }
    if (o >= cap) o = cap - 1;
    p[o] = 0;
}

/* Symlink de .framework gravado como texto ("A", "Versions/Current/Electron Framework").
 * Link quebrado devolve NULL para o locate() ainda tentar Versions/A. */
static const u8 *dep_follow(const char *path, usize *len, int depth) {
    const u8 *d = dep_read_raw(path, len);
    char tgt[512], dir[400], next[520];
    usize n2 = 0;
    const u8 *d2;
    if (!d || !path || depth >= 8) return d;
    if (!strstr(path, ".framework")) return d;
    if (!fwlink_target(d, *len, tgt, sizeof tgt)) return d;
    if (tgt[0] == '/') snprintf(next, sizeof next, "%s", tgt);
    else {
        path_dir(path, dir, sizeof dir);
        snprintf(next, sizeof next, "%s/%s", dir, tgt);
    }
    path_norm(next, sizeof next);
    d2 = dep_follow(next, &n2, depth + 1);
    if (!d2) return NULL;
    *len = n2;
    return d2;
}

static void path_subst(const char *s, const char *loader, const char *exe, char *out, usize cap) {
    char d[320];
    if (!strncmp(s, "@executable_path", 16)) { path_dir(exe, d, sizeof d); snprintf(out, cap, "%s%s", d, s + 16); }
    else if (!strncmp(s, "@loader_path", 12)) { path_dir(loader, d, sizeof d); snprintf(out, cap, "%s%s", d, s + 12); }
    else snprintf(out, cap, "%s", s);
}

static int find_loaded(link_t *K, const char *p) {
    for (int i = 0; i < K->n; i++) if (!strcmp(K->im[i]->path, p)) return i;
    return -1;
}

/* procura cand (e, num .framework, as variantes Versions/A e Versions/Current, já que o FAT não tem
 * links simbólicos). Devolve o índice se já carregada; senão -1, com *data preenchido se achou */
static int locate(link_t *K, char *cand, usize cap, const u8 **data, usize *len) {
    *data = NULL;
    path_norm(cand, cap);
    char alt[384];
    /* X.framework/X e X.framework/Versions/A/X são o mesmo arquivo no macOS (link simbólico); a imagem do mOS
     * não tem links, então os dois existem como cópias. Procura primeiro, por todos os nomes, uma imagem já
     * carregada: sem isso o UIKit do Catalyst entrava duas vezes (classes duplicadas) no WhatsApp. */
    for (int pass = 0; pass < 2; pass++)
    for (int v = 0; v < 3; v++) {
        const char *p = cand;
        if (v) {
            const char *fw = strstr(cand, ".framework/");
            if (!fw) break;
            const char *tail = fw + 11;
            if (strchr(tail, '/')) break;
            snprintf(alt, sizeof alt, "%.*sVersions/%s/%s", (int)(tail - cand), cand, v == 1 ? "A" : "Current", tail);
            p = alt;
        }
        if (pass == 0) {
            int i = find_loaded(K, p);
            if (i >= 0) return i;
            if (v == 0) {                                   /* X.framework/Versions/<v>/X -> X.framework/X */
                const char *fw = strstr(cand, ".framework/Versions/");
                const char *vs = fw ? strchr(fw + 20, '/') : NULL;
                if (vs && !strchr(vs + 1, '/')) {
                    char base[384];
                    snprintf(base, sizeof base, "%.*s%s", (int)(fw + 11 - cand), cand, vs + 1);
                    i = find_loaded(K, base);
                    if (i >= 0) return i;
                }
            }
            continue;
        }
        *data = dep_read(p, len);
        if (*data) {
            if (p != cand) snprintf(cand, cap, "%s", p);
            return -1;
        }
    }
    return -1;
}

static void note_absent(link_t *K, const char *dylib) {
    char s[96];
    dylib_short(dylib, s, sizeof s);
    usize n = strlen(s);
    for (const char *f = K->absent; (f = strstr(f, s)); f += n)
        if ((f == K->absent || f[-1] == ' ') && (!f[n] || f[n] == ',')) return;
    if (K->nabsent < 4) {
        usize l = strlen(K->absent);
        snprintf(K->absent + l, sizeof K->absent - l, "%s%s", l ? ", " : "", s);
    }
    K->nabsent++;
}

static int link_add(link_t *K, const char *path, const u8 *file, usize flen, bool dylib);

/* acha e carrega uma dependência "@..." pedida por I; -1: não achou; -2: erro (em K->err) */
static int link_dep(link_t *K, image_t *I, const char *name) {
    char cand[384], base[320];
    const u8 *data;
    usize len;
    const char *exe = K->im[0]->path;
    if (!strncmp(name, "@rpath/", 7)) {
        for (int pass = 0; pass < 2; pass++) {
            image_t *R = pass ? K->im[0] : I;
            if (pass && R == I) break;
            for (int r = 0; r < R->mo.nrpaths; r++) {
                path_subst(R->mo.rpaths[r], R->path, exe, base, sizeof base);
                snprintf(cand, sizeof cand, "%s/%s", base, name + 7);
                int i = locate(K, cand, sizeof cand, &data, &len);
                if (i >= 0) return i;
                if (data) { i = link_add(K, cand, data, len, true); return i < 0 ? -2 : i; }
            }
        }
        return -1;
    }
    path_subst(name, I->path, exe, cand, sizeof cand);
    int i = locate(K, cand, sizeof cand, &data, &len);
    if (i >= 0) return i;
    if (!data) return -1;
    i = link_add(K, cand, data, len, true);
    return i < 0 ? -2 : i;
}

/* dependência com caminho absoluto fora do sistema (Homebrew: /usr/local/opt/<f>/lib, @@HOMEBREW_PREFIX@@,
 * @@HOMEBREW_CELLAR@@ e as bibliotecas de /usr/lib que o mOS não tem). -1: não achou; -2: erro (em K->err).
 * Os caminhos ficam no heap: link_add é recursivo pela cadeia de dependências (git -> libcurl -> libssl ->
 * libcrypto...) e roda na pilha de 64 KiB do kernel da thread que fez o exec. */
static int link_abs_dep(link_t *K, const char *dn) {
    enum { CAP = 384 };
    char *cand = kmalloc(2 * CAP);
    if (!cand) { snprintf(K->err, K->errlen, "sem memória"); return -2; }
    char *opt = cand + CAP;
    if (!strncmp(dn, "@@HOMEBREW_PREFIX@@", 19)) snprintf(cand, CAP, "/usr/local%s", dn + 19);
    else if (!strncmp(dn, "@@HOMEBREW_CELLAR@@", 19)) snprintf(cand, CAP, "/usr/local/Cellar%s", dn + 19);
    else snprintf(cand, CAP, "%s", dn);
    int d = -1;
    if (cand[0] == '/') {
        const u8 *data;
        usize len;
        /* /usr/local/Cellar/<fórmula>/<versão>/X: no Homebrew, /usr/local/opt/<fórmula> é um link para esse keg.
         * Sem o Cellar no disco vale o opt; com os dois, o opt é o canônico (a mesma dylib não entra duas vezes). */
        if (!strncmp(cand, "/usr/local/Cellar/", 18)) {
            const char *f = cand + 18, *v = strchr(f, '/'), *x = v ? strchr(v + 1, '/') : NULL;
            if (x) {
                snprintf(opt, CAP, "/usr/local/opt/%.*s%s", (int)(v - f), f, x);
                if (dep_read(opt, &len)) snprintf(cand, CAP, "%s", opt);
            }
        }
        d = locate(K, cand, CAP, &data, &len);
        /* ncurses, libffi e expat: a fórmula do Homebrew (uma cópia só). libz, libcurl e
         * libcompression estão na imagem, em /usr/lib (build.ps1). O par é {nome pedido, caminho}.
         * Outra linha, no mesmo formato, cabe aqui — inclusive uma libswift*. */
        static const char *const usrlib_alias[][2] = {
            {"/usr/lib/libncurses.5.4.dylib", "/usr/local/opt/ncurses/lib/libncursesw.6.dylib"},
            {"/usr/lib/libz.1.dylib",         "/usr/lib/libz.1.dylib"},
            {"/usr/lib/libffi.dylib",         "/usr/local/opt/libffi/lib/libffi.8.dylib"},
            {"/usr/lib/libcurl.4.dylib",      "/usr/lib/libcurl.4.dylib"},
            {"/usr/lib/libexpat.1.dylib",     "/usr/local/opt/expat/lib/libexpat.1.dylib"},
            {"/usr/lib/libcompression.dylib", "/usr/lib/libcompression.dylib"},
        };
        for (usize k = 0; d < 0 && !data && k < sizeof usrlib_alias / sizeof usrlib_alias[0]; k++)
            if (!strcmp(cand, usrlib_alias[k][0])) {
                snprintf(cand, CAP, "%s", usrlib_alias[k][1]);
                d = locate(K, cand, CAP, &data, &len);
            }
        if (d < 0 && data) { d = link_add(K, cand, data, len, true); if (d < 0) d = -2; }   /* link_add copia o caminho */
    }
    kfree(cand);
    return d;
}

static int link_add(link_t *K, const char *path, const u8 *file, usize flen, bool dylib) {
    char nm[96];
    dylib_short(path, nm, sizeof nm);
    if (K->n >= MAX_IMAGES) { snprintf(K->err, K->errlen, "bibliotecas demais no bundle (máximo %d)", MAX_IMAGES); return -1; }
    image_t *I = kcalloc(1, sizeof *I);
    if (!I) { snprintf(K->err, K->errlen, "sem memória"); return -1; }
    const u8 *m;
    usize ml;
    char archs[96] = "", e[160];
    bool ok = pick_slice(file, flen, &m, &ml, archs, sizeof archs, e, sizeof e) && parse(&I->mo, m, ml, dylib, e, sizeof e);
    if (!K->n) snprintf(K->archs, sizeof K->archs, "%s", archs);
    if (ok && I->mo.has_tlv) { snprintf(e, sizeof e, "usa inicializadores thread-local, ainda não suportados"); ok = false; }
    if (!ok) {
        if (dylib) snprintf(K->err, K->errlen, "%s: %s", nm, e);
        else snprintf(K->err, K->errlen, "%s", e);
        kfree(I);
        return -1;
    }
    snprintf(I->path, sizeof I->path, "%s", path);
    I->idx = K->n;
    K->im[K->n++] = I;
    for (int i = 0; i < I->mo.ndylibs; i++) {
        const char *dn = I->mo.dylibs[i];
        I->dep[i] = -1;
        if (dn[0] == '@' && dn[1] != '@') {          /* "@@HOMEBREW_...@@" é tratado abaixo */
            int d = link_dep(K, I, dn);
            if (d == -2) return -1;
            if (d >= 0) { I->dep[i] = (i16)d; continue; }
            if (I->mo.dylib_weak[i]) continue;
            char s[96];
            dylib_short(dn, s, sizeof s);
            snprintf(K->err, K->errlen, "%s não está dentro do app (pedido por %s)", s, nm);
            return -1;
        }
        if (libcxx_name(dn)) {
            /* libc++/libc++abi: a do LLVM no disco (/usr/lib/libc++.1.dylib, tools/build-libcxx.ps1, com a libc++abi
             * dentro), que tem o layout que o código inline dos módulos .node e do runtime Swift espera (ios_base,
             * vtables de stringstream, facetas de locale, chrono). Sem ela no disco (imagem antiga, machotest sem
             * /usr/lib), o desvio para o libSystem (cxxabi.c e libcxx.c), como antes. */
            int d = link_abs_dep(K, LIBCXX_DISK);
            if (d == -2) return -1;
            if (d >= 0) { I->dep[i] = (i16)d; continue; }
        }
        if (!strcmp(dn, LIBICU_DISK)) {   /* o ICU do disco; sem ele, o uidna_* do libSystem (abaixo) */
            int d = link_abs_dep(K, LIBICU_DISK);
            if (d == -2) return -1;
            if (d >= 0) { I->dep[i] = (i16)d; continue; }
        }
        if (libsystem_dylib_ok(dn)) {
            /* Foundation.framework é do libSystem, mas a parte Swift dele (overlay + FoundationEssentials com o ABI da
             * Apple, docs/swift-objc-runtime.md, fase 5) é uma dylib Mach-O: no macOS 14+ ela mora dentro do framework,
             * no SDK antigo em /usr/lib/swift/libswiftFoundation.dylib. Se existir no disco, entra junto. */
            if (!strncmp(dn, "/System/Library/Frameworks/Foundation.framework/", 48)) {
                char sw[64] = "/usr/lib/swift/libswiftFoundation.dylib";
                const u8 *data; usize len;
                int d = locate(K, sw, sizeof sw, &data, &len);
                if (d < 0 && data) d = link_add(K, sw, data, len, true);
                if (d >= 0) I->dep[i] = (i16)d;
            }
            continue;
        }
        {   /* dylib com caminho absoluto fora do sistema (ex.: /usr/local/opt/readline/lib, do Homebrew) */
            int d = link_abs_dep(K, dn);
            if (d == -2) return -1;
            if (d >= 0) { I->dep[i] = (i16)d; continue; }
        }
        if (I->mo.dylib_weak[i]) continue;
        note_absent(K, dn);
    }
    return I->idx;
}

static void link_order(link_t *K, int i) {
    image_t *I = K->im[i];
    if (I->visited) return;
    I->visited = true;
    for (int d = 0; d < I->mo.ndylibs; d++) if (I->dep[d] >= 0) link_order(K, I->dep[d]);
    K->order[K->norder++] = i;
}

/* registra a proteção inicial (initprot) de cada segmento das imagens [from, n) no kernel, para o
 * mach_vm_region do libSystem responder como o macOS (o base::ProtectedMemory do Chromium confere que o
 * segmento PROTECTED_MEMORY é só leitura). Não muda a PTE: ver machipc.c.
 * No arm64 o fim sobe até a página guest de 16 KiB, sem invadir o segmento seguinte. */
#ifndef MOS_HOST_TEST
static u64 seg_prot_hi(image_t *I, seg_t *g, u64 lo, u64 hi) {
    if (!ls_cur || !g->vmsize) return hi;
    u64 up = A64_GUEST_PAGE_END(lo, g->vmsize);
    for (int t = 0; t < I->mo.nsegs; t++) {
        seg_t *o = &I->mo.segs[t];
        if (o == g || o->zero || !o->vmsize) continue;
        u64 olo = o->vmaddr + (u64)I->slide;
        if (olo > lo && olo < up) up = olo;
    }
    return up;
}
#endif
static void segs_prot(process_t *p, link_t *K, int from) {
#ifndef MOS_HOST_TEST
    for (int i = from; i < K->n; i++) {
        image_t *I = K->im[i];
        for (int s = 0; s < I->mo.nsegs; s++) {
            seg_t *g = &I->mo.segs[s];
            if (g->zero || !g->vmsize) continue;
            u64 lo = g->vmaddr + (u64)I->slide;
            machipc_set_prot(p, lo, seg_prot_hi(I, g, lo, lo + g->vmsize), g->prot);
        }
    }
#else
    (void)p; (void)K; (void)from;
#endif
}

/* põe a imagem em at; devolve o próximo endereço livre (alinhado a 64 KiB) */
static u64 place_image(image_t *I, u64 at) {
    u64 size = I->mo.vmax - I->mo.vmin;
    I->base = at;
    I->slide = (i64)(at - I->mo.vmin);
    I->hdr = at;
    for (int s = 0; s < I->mo.nsegs; s++)
        if (!I->mo.segs[s].zero && !I->mo.segs[s].fileoff && I->mo.segs[s].filesize) { I->hdr = I->mo.segs[s].vmaddr + (u64)I->slide; break; }
    return (at + size + 0xFFFF) & ~0xFFFFULL;
}

static bool link_layout(link_t *K) {
    u64 at = IMAGE_BASE;
    for (int i = 0; i < K->n; i++) {
        at = place_image(K->im[i], at);
        if (at > IMAGE_LIMIT - 0x100000) {
            snprintf(K->err, K->errlen, "o app não cabe no espaço de um processo do mOS (%llu MiB)", (unsigned long long)((at - IMAGE_BASE) >> 20));
            return false;
        }
    }
    K->stub_base = K->end = at;
    return true;
}

static link_t *link_new(char *err, usize errlen, bool report_only) {
    link_t *K = kcalloc(1, sizeof *K);
    if (!K) { snprintf(err, errlen, "sem memória"); return NULL; }
    K->err = err;
    K->errlen = errlen;
    K->report_only = report_only;
    K->arch = ls_cur;
    K->tolerant = dyld_tolerant();
    K->unresolved_fn = ls_lookup("_cx_unresolved");
    K->tlv_per_thread = ls_lookup("_cx_tlv_bootstrap") != 0;
#ifndef MOS_HOST_TEST
    /* diagnóstico do porte: registra cada função ausente e devolve 0 em vez de encerrar */
    if (vfs_lookup("/Sistema/Biblioteca/dyld-registrar")) { u64 f = ls_lookup("_cx_unresolved_log"); if (f) K->unresolved_fn = f; }
#endif
    return K;
}

static void link_free(link_t *K) {
    if (!K) return;
    for (int i = 0; i < K->n; i++) { kfree(K->im[i]->own); kfree(K->im[i]); }
    for (u32 i = 0; i < K->nmissing; i++) kfree(K->missing[i]);
    kfree(K->missing);
    kfree(K->inits);
    kfree(K);
}

/* monta o grafo de imagens a partir do executável (as bibliotecas ausentes ficam em K->absent) */
static bool link_open(link_t *K, const char *path, const u8 *img, usize len) {
    if (link_add(K, path, img, len, false) < 0) return false;
    link_order(K, 0);
    return link_layout(K);
}

static void absent_summary(link_t *K, char *out, usize cap) {
    if (K->nabsent > 4) snprintf(out, cap, "%s e mais %d", K->absent, K->nabsent - 4);
    else snprintf(out, cap, "%s", K->absent);
}

/* área de stubs da carga atual (imagens K->first_new..K->n-1), montada no kernel e copiada para
 * K->stub_base:
 *   [0, 64)       zeros: constantes ausentes apontam para cá (leem 0 / nil)
 *   [64, 0x1000)  registros TLS por imagem {modelo, tamanho inicializado, tamanho total, índice}; o
 *                 resto é int3: uma chamada a uma "constante" que era função para ali
 *   stubs         "movabs rdi, nome; movabs rax, cx_unresolved; jmp rax" por símbolo ausente
 *   nomes, inicializadores (endereços absolutos), seções ObjC {addr, size, kind} */
static u8 *stub_area(link_t *K, usize *area_out, usize *inits_off, usize *objc_off) {
    usize stub_code = ZERO_AREA + (usize)K->nmissing * 32, names = 0;
    for (u32 i = 0; i < K->nmissing; i++) names += strlen(K->missing[i]) + 1;
    *inits_off = (stub_code + names + 7) & ~(usize)7;
    *objc_off = *inits_off + (usize)K->ninits * 8;
    usize area = (*objc_off + (usize)K->nobjc * 24 + 0xFFF) & ~(usize)0xFFF;
    u8 *stubs = kcalloc(1, area);
    if (!stubs) return NULL;
    memset(stubs + 64, K->arch ? 0x00 : 0xCC, ZERO_AREA - 64);   /* int3 / udf #0 */
    for (int i = K->first_new; i < K->n; i++) {            /* registros de TLS por imagem */
        u64 *r = (u64 *)(stubs + TLV_RECORD(i - K->first_new));
        mach_t *m = &K->im[i]->mo;
        r[0] = m->tlv_hi ? m->tlv_lo + (u64)K->im[i]->slide : 0;
        r[1] = m->tlv_init_end > m->tlv_lo ? m->tlv_init_end - m->tlv_lo : 0;
        r[2] = m->tlv_hi > m->tlv_lo ? m->tlv_hi - m->tlv_lo : 0;
        r[3] = (u64)i;                                     /* índice global: a tabela por thread do libSystem */
    }
    usize no = stub_code;
    for (u32 i = 0; i < K->nmissing; i++) {
        u8 *s = stubs + ZERO_AREA + i * 32;
        u64 name_va = K->stub_base + no;
        usize nl = strlen(K->missing[i]) + 1;
        memcpy(stubs + no, K->missing[i], nl);
        no += nl;
        if (K->arch) {   /* arm64: ldr x0, nome; ldr x16, cx_unresolved; br x16 (literais em +16/+24) */
            u32 w[4] = {0x58000080u, 0x580000B0u, 0xD61F0200u, 0xD4200000u};   /* ldr x0,#16; ldr x16,#20; br x16; brk #0 */
            u64 fn = K->unresolved_fn;
#ifdef MOS_HOST_TEST
            if (getenv("MACHOTEST_STUB_LOG")) {   /* ldr x0,#24; mov x8,#0x7FFF; svc #0x4d4f; mov x0,#0 | ret em +16 */
                w[0] = 0x580000C0u; w[1] = 0xD28FFFE8u; w[2] = 0xD409A9E1u; w[3] = 0xD2800000u;
                u32 ret = 0xD65F03C0u;
                memcpy(s + 16, &ret, 4);
                memcpy(s + 24, &name_va, 8);
                memcpy(s, w, 16);
                continue;
            }
#endif
            memcpy(s, w, 16);
            memcpy(s + 16, &name_va, 8);
            memcpy(s + 24, &fn, 8);
            continue;
        }
        s[0] = 0x48; s[1] = 0xBF; memcpy(s + 2, &name_va, 8);              /* movabs rdi, imm64 */
#ifdef MOS_HOST_TEST
        /* machotest com MACHOTEST_STUB_LOG: registra a chamada e devolve 0 (mapeia o que o app usa) */
        if (getenv("MACHOTEST_STUB_LOG")) {
            s[10] = 0xB8; s[11] = 0xFF; s[12] = 0x7F; s[13] = 0; s[14] = 0;  /* mov eax, 0x7FFF */
            s[15] = 0xCD; s[16] = 0x80;                                     /* int 0x80 */
            s[17] = 0x31; s[18] = 0xC0;                                     /* xor eax, eax */
            s[19] = 0xC3;                                                   /* ret */
            continue;
        }
#endif
        s[10] = 0x48; s[11] = 0xB8; memcpy(s + 12, &K->unresolved_fn, 8);  /* movabs rax, imm64 */
        s[20] = 0xFF; s[21] = 0xE0;                                       /* jmp rax */
    }
    if (K->ninits) memcpy(stubs + *inits_off, K->inits, (usize)K->ninits * 8);
    u64 *ob = (u64 *)(stubs + *objc_off);
    for (int i = 0; i < K->nobjc; i++) { ob[i * 3] = K->objc[i].addr; ob[i * 3 + 1] = K->objc[i].size; ob[i * 3 + 2] = K->objc[i].kind; }
    *area_out = area;
    return stubs;
}

/* ================================================================ dlopen em tempo de execução
 * Depois do macho_load o grafo (link_t) fica em process_t.dyld. SYS_DYLD (syscall.h) carrega
 * MH_DYLIB/MH_BUNDLE no processo em execução: as imagens novas vão para a região de mmap, recebem os
 * fixups contra as imagens já carregadas + libSystem e ganham uma área de stubs própria; o libSystem
 * (userland/libsystem/dlfcn.c) registra as seções ObjC e roda os inicializadores que voltam. */

/* o que o dlopen/dlsym ainda consultam de uma imagem carregada (comandos de carga: nomes das dylibs e
 * LC_RPATH; export trie) passa para uma cópia do kernel: o arquivo no VFS pode ser regravado ou
 * apagado enquanto o processo roda */
static usize own_len(image_t *I) { return I->own ? 32 + rd32(I->own + 20) + I->mo.triesz : 0; }

static void image_rebase_ptrs(image_t *I, const u8 *from, const u8 *to) {
    mach_t *mo = &I->mo;
    for (int i = 0; i < mo->ndylibs; i++) mo->dylibs[i] = (const char *)to + ((const u8 *)mo->dylibs[i] - from);
    for (int i = 0; i < mo->nrpaths; i++) mo->rpaths[i] = (const char *)to + ((const u8 *)mo->rpaths[i] - from);
    if (mo->dyld_info) mo->dyld_info = to + (mo->dyld_info - from);
    if (mo->chained) mo->chained = to + (mo->chained - from);
}

static void image_persist(image_t *I) {
    mach_t *mo = &I->mo;
    if (I->own || !mo->m) return;
    usize hl = 32 + rd32(mo->m + 20);
    u8 *o = kmalloc(hl + mo->triesz + 1);
    if (!o) return;                                       /* sem memória: continua apontando para o arquivo */
    memcpy(o, mo->m, hl);
    if (mo->triesz) memcpy(o + hl, mo->trie, mo->triesz);
    image_rebase_ptrs(I, mo->m, o);
    if (mo->trie) mo->trie = o + hl;
    mo->m = o;
    mo->len = hl;                                         /* os dados da linkedit não estão mais aqui */
    I->own = o;
}

static void link_keep(process_t *p, link_t *K) {
    for (int i = 0; i < K->n; i++) { image_persist(K->im[i]); K->im[i]->refs = 1; }
    for (u32 i = 0; i < K->nmissing; i++) kfree(K->missing[i]);
    kfree(K->missing);
    K->missing = NULL; K->nmissing = K->capmissing = 0;
    kfree(K->inits);
    K->inits = NULL; K->ninits = K->capinits = 0;
    K->nobjc = 0;
    K->err = K->lasterr; K->errlen = sizeof K->lasterr; K->lasterr[0] = 0;
    if (p->dyld) link_free(p->dyld);
    p->dyld = K;
}

/* fork: o filho tem a mesma memória, então recebe uma cópia do grafo */
void *macho_dyld_clone(void *v) {
    link_t *S = v;
    if (!S) return NULL;
    link_t *K = kmalloc(sizeof *K);
    if (!K) return NULL;
    memcpy(K, S, sizeof *K);
    K->missing = NULL; K->nmissing = K->capmissing = 0;
    K->inits = NULL; K->ninits = K->capinits = 0;
    K->err = K->lasterr; K->errlen = sizeof K->lasterr;
    int ok = 0;
    for (; ok < S->n; ok++) {
        image_t *I = kmalloc(sizeof *I);
        if (!I) break;
        memcpy(I, S->im[ok], sizeof *I);
        if (I->own) {
            usize l = own_len(I);
            u8 *o = kmalloc(l + 1);
            if (!o) { kfree(I); break; }
            memcpy(o, I->own, l);
            image_rebase_ptrs(I, I->own, o);
            if (I->mo.trie) I->mo.trie = o + (I->mo.trie - I->own);
            I->mo.m = o;
            I->own = o;
        }
        K->im[ok] = I;
    }
    K->n = ok;
    if (ok < S->n) { link_free(K); return NULL; }
    return K;
}

void macho_dyld_free(void *v) { link_free(v); }

static int image_at(link_t *K, u64 a) {
    for (int i = 0; i < K->n; i++) {
        image_t *I = K->im[i];
        if (a >= I->base && a < I->base + (I->mo.vmax - I->mo.vmin)) return i;
    }
    return -1;
}

static void dyld_rollback(link_t *K, int n0) {
    for (int i = n0; i < K->n; i++) { kfree(K->im[i]->own); kfree(K->im[i]); K->im[i] = NULL; }
    K->n = n0;
}

/* RTLD_NOLOAD: só procura entre as já carregadas */
static int dyld_find_loaded(link_t *K, image_t *C, const char *req, const char *cwd) {
    char cand[384];
    if (!strncmp(req, "@rpath/", 7)) {
        usize rl = strlen(req + 6);                         /* "/resto" */
        for (int i = 0; i < K->n; i++) {
            usize pl = strlen(K->im[i]->path);
            if (pl >= rl && !strcmp(K->im[i]->path + pl - rl, req + 6)) return i;
        }
        return -1;
    }
    if (req[0] == '@') path_subst(req, C->path, K->im[0]->path, cand, sizeof cand);
    else if (req[0] == '/') snprintf(cand, sizeof cand, "%s", req);
    else snprintf(cand, sizeof cand, "%s/%s", cwd, req);
    const u8 *d;
    usize l;
    return locate(K, cand, sizeof cand, &d, &l);
}

#define DYLD_H_LIBSYSTEM 0x10000            /* handle de dlopen("/usr/lib/libSystem.B.dylib") e afins */

#ifndef MOS_HOST_TEST
/* __TEXT compartilhado entre links (tcache, junto do icache mais abaixo) */
static void tcache_plan(link_t *K, int from, void *const *hit);
static bool map_fresh(process_t *p, link_t *K, int from, u64 a, u64 b);
static void tcache_after_mount(process_t *p, image_t *I);
#endif

static i64 dyld_open(process_t *p, link_t *K, const char *req, u64 out_va, u64 caller, const char *cwd) {
    mos_dyld_open_t o;
    if (!uvm_copy_in(p, &o, out_va, sizeof o)) return -MOS_EFAULT;
    bool noload = (o.flags & MOS_RTLD_NOLOAD) != 0;
    memset(&o, 0, sizeof o);
    K->lasterr[0] = 0;
    usize cxx_len;
    if (libcxx_name(req) && dep_read(LIBCXX_DISK, &cxx_len)) req = LIBCXX_DISK;   /* a libc++ do disco, como no link_image */
    else if (!strcmp(req, LIBICU_DISK) && dep_read(LIBICU_DISK, &cxx_len)) { /* o ICU do disco, como no link_image */ }
    else if (libsystem_dylib_ok(req)) return DYLD_H_LIBSYSTEM;
    int ci = image_at(K, caller);
    image_t *C = K->im[ci >= 0 ? ci : 0];
    if (noload) {
        int i = dyld_find_loaded(K, C, req, cwd);
        if (i < 0) { snprintf(K->lasterr, sizeof K->lasterr, "%s: não está carregada (RTLD_NOLOAD)", req); return -MOS_ENOENT; }
        K->im[i]->refs++;
        return uvm_copy_out(p, out_va, &o, sizeof o) ? i : -MOS_EFAULT;
    }
    int n0 = K->n;
    char absent[400];
    int nabsent = K->nabsent;
    memcpy(absent, K->absent, sizeof absent);
    K->absent[0] = 0;
    K->nabsent = 0;
    int idx;
    if (req[0] == '@') idx = link_dep(K, C, req);         /* @rpath (da imagem que chamou e do executável), @loader_path... */
    else {
        char cand[384];
        const u8 *data;
        usize len;
        if (req[0] == '/') snprintf(cand, sizeof cand, "%s", req);
        else snprintf(cand, sizeof cand, "%s/%s", cwd, req);
        idx = locate(K, cand, sizeof cand, &data, &len);   /* >= 0: já carregada; -1 com data: achou o arquivo */
        if (idx < 0 && data) { int ai = link_add(K, cand, data, len, true); idx = ai < 0 ? -2 : ai; }
    }
    i64 r = idx;
    if (idx == -1) {
        bool sys = !strncmp(req, "/System/", 8) || !strncmp(req, "/usr/lib/", 9);
        snprintf(K->lasterr, sizeof K->lasterr, sys ? "%s: biblioteca do sistema que o mOS ainda não tem" : "%s: arquivo não encontrado", req);
        r = -MOS_ENOENT;
    } else if (idx == -2) r = -MOS_EINVAL;             /* mensagem já em K->lasterr (link_add) */
    if (r >= 0 && K->n > n0 && K->nabsent && !K->tolerant) {
        char need[200];
        absent_summary(K, need, sizeof need);
        snprintf(K->lasterr, sizeof K->lasterr, "%s requer %s, que o mOS ainda não tem", req, need);
        r = -MOS_ENOENT;
    }
    if (r >= 0 && K->n > n0) {
        /* imagens novas: ordem de dependência, endereços na região de mmap, fixups e área de stubs */
        K->first_new = n0;
        K->norder = 0;
        for (int i = n0; i < K->n; i++) link_order(K, i);
        if (!p->mmap_next) p->mmap_next = USER_MMAP_BASE;
        u64 base0 = (p->mmap_next + 0xFFFF) & ~0xFFFFULL, at = base0;
        for (int i = n0; i < K->n; i++) at = place_image(K->im[i], at);
        K->stub_base = K->end = at;
#ifndef MOS_HOST_TEST
        tcache_plan(K, n0, NULL);            /* __TEXT já montado por outro processo, em outro endereço (ex.: helpers) */
        bool mapped = at + 0x200000 <= USER_END && map_fresh(p, K, n0, base0, at);
#else
        bool mapped = at + 0x200000 <= USER_END && uvm_map(p, base0, at - base0);
#endif
        if (!mapped) {
            snprintf(K->lasterr, sizeof K->lasterr, "%s: sem memória para a imagem", req);
            r = -MOS_ENOMEM;
        } else p->mmap_next = at;
        for (int k = 0; r >= 0 && k < K->norder; k++) {
            image_t *I = K->im[K->order[k]];
            u64 size = I->mo.vmax - I->mo.vmin;
#ifndef MOS_HOST_TEST
            if (size >= (16ull << 20)) {          /* o ICACHE_MIN do spawn, definido mais abaixo */
                /* grande (o Chromium Framework tem 307 MiB): montada direto nas páginas do processo, mapeadas e zeradas
                 * acima, como no spawn. O buffer contíguo do kernel do mesmo tamanho falhava com outro app grande aberto
                 * ("kmalloc: sem memória para 322191360 bytes" com o Slack rodando) e dobrava a memória durante a montagem */
                u64 fl = irq_save(), old_cr3 = read_cr3();          /* sem troca de tarefa com o CR3 emprestado */
                __asm__ volatile("mov %0, %%cr3" :: "r"((u64)p->pml4) : "memory");
                bool fixed = image_fix(K, I, (u8 *)I->base);
                __asm__ volatile("mov %0, %%cr3" :: "r"(old_cr3) : "memory");
                irq_restore(fl);
                if (!fixed) r = -MOS_EINVAL;
                else tcache_after_mount(p, I);
            } else
#endif
            {
                u8 *buf = kcalloc(1, size);
                if (!buf) { snprintf(K->lasterr, sizeof K->lasterr, "sem memória para a imagem (%llu KiB)", (unsigned long long)(size >> 10)); r = -MOS_ENOMEM; break; }
                if (!image_fix(K, I, buf)) r = -MOS_EINVAL;
                else if (!uvm_copy_out(p, I->base, buf, size)) { snprintf(K->lasterr, sizeof K->lasterr, "falha ao copiar a imagem"); r = -MOS_EFAULT; }
                kfree(buf);
            }
#ifndef MOS_HOST_TEST
            /* diagnóstico: os tracepoints valem também para imagens do dlopen (ex.: Google Chrome Framework) */
            if (r >= 0) { extern void tp_arm(process_t *p, const char *path, u64 base, u64 vmin); tp_arm(p, I->path, I->base, I->mo.vmin); }
#endif
        }
        usize area = 0, inits_off = 0, objc_off = 0;
        u8 *stubs = r >= 0 ? stub_area(K, &area, &inits_off, &objc_off) : NULL;
        if (r >= 0 && (!stubs || !uvm_map(p, K->stub_base, area) || !uvm_copy_out(p, K->stub_base, stubs, area))) {
            snprintf(K->lasterr, sizeof K->lasterr, "sem memória para o processo");
            r = -MOS_ENOMEM;
        }
        kfree(stubs);
        if (r >= 0) {
            p->mmap_next = K->stub_base + area + 0x1000;
            o.inits = K->stub_base + inits_off; o.ninits = K->ninits;
            o.objc = K->stub_base + objc_off; o.nobjc = (u64)K->nobjc;
            o.first = (u64)n0; o.nnew = (u64)(K->n - n0);
            kprintf("dyld: dlopen %s: %d imagem(ns) em %llx (%u símbolo(s) ausente(s)%s%s)\n", req, K->n - n0,
                    (unsigned long long)base0, K->nmissing, K->nmissing ? ", ex.: " : "", K->nmissing ? K->missing[0] : "");
            segs_prot(p, K, n0);
            for (int i = n0; i < K->n; i++) image_persist(K->im[i]);
        }
        for (u32 i = 0; i < K->nmissing; i++) kfree(K->missing[i]);
        kfree(K->missing);
        K->missing = NULL; K->nmissing = K->capmissing = 0;
        kfree(K->inits);
        K->inits = NULL; K->ninits = K->capinits = 0;
        K->nobjc = 0;
        K->first_new = 0;
    }
    if (r < 0) dyld_rollback(K, n0);
    K->nabsent = nabsent;
    memcpy(K->absent, absent, sizeof absent);
    if (r < 0) return r;
    K->im[idx]->refs++;
    return uvm_copy_out(p, out_va, &o, sizeof o) ? r : -MOS_EFAULT;
}

/* imagem e as dependências dela (em largura), como o dlsym com handle do dyld */
static u64 dyld_sym_tree(link_t *K, int root, const char *name) {
    int q[MAX_IMAGES], h = 0, t = 0;
    u8 seen[MAX_IMAGES] = {0};
    q[t++] = root;
    seen[root] = 1;
    while (h < t) {
        int i = q[h++];
        u64 a = image_export(K, i, name, 0);
        if (a) return a;
        image_t *I = K->im[i];
        for (int d = 0; d < I->mo.ndylibs; d++) {
            int j = I->dep[d];
            if (j >= 0 && j < K->n && !seen[j] && t < MAX_IMAGES) { seen[j] = 1; q[t++] = j; }
        }
    }
    return 0;
}

static u64 dyld_sym(link_t *K, i64 h, const char *sym, u64 caller) {
    char name[1032];
    snprintf(name, sizeof name, "_%s", sym);              /* dlsym recebe o nome C, sem o "_" do Mach-O */
    u64 a = 0;
    int ci = image_at(K, caller);
    if (h == DYLD_H_LIBSYSTEM) return ls_lookup(name);
    if (h >= 0) {
        if (h >= K->n) return 0;
        a = dyld_sym_tree(K, (int)h, name);
    } else if (h == MOS_RTLD_DEFAULT) {
        for (int i = 0; i < K->n && !a; i++) a = image_export(K, i, name, 0);
    } else if (h == MOS_RTLD_MAIN_ONLY) {
        return image_export(K, 0, name, 0);
    } else if (h == MOS_RTLD_SELF) {
        a = dyld_sym_tree(K, ci >= 0 ? ci : 0, name);
    } else if (h == MOS_RTLD_NEXT) {
        for (int i = ci + 1; i < K->n && !a; i++) a = image_export(K, i, name, 0);
    } else return 0;
    return a ? a : ls_lookup(name);
}

/* string de usuário (até cap - 1 bytes), por trechos que não cruzam página */
static bool ustr(process_t *p, u64 va, char *out, usize cap) {
    usize i = 0;
    while (i + 1 < cap) {
        usize chunk = 4096 - (usize)((va + i) & 0xFFF);
        if (chunk > cap - 1 - i) chunk = cap - 1 - i;
        if (!uvm_copy_in(p, out + i, va + i, chunk)) return false;
        for (usize k = 0; k < chunk; k++) if (!out[i + k]) return true;
        i += chunk;
    }
    out[cap - 1] = 0;
    return true;
}

/* SYS_DYLD (op, a, b, c): ver MOS_DYLD_* em kernel/include/syscall.h */
i64 macho_dyld_syscall(process_t *p, u64 op, u64 a, u64 b, u64 c) {
    link_t *K = p->dyld;
    if (!K) return -MOS_EINVAL;                           /* processo que não é Mach-O */
    ls_cur = K->arch;                                     /* libSystem e slices da arquitetura do processo */
    K->err = K->lasterr;
    K->errlen = sizeof K->lasterr;
    switch (op) {
    case MOS_DYLD_OPEN: {
        char path[384];
        if (!ustr(p, a, path, sizeof path)) return -MOS_EFAULT;
#ifdef MOS_HOST_TEST
        const char *cwd = "";
#else
        const char *cwd = p->cwd;
#endif
        return dyld_open(p, K, path, b, c, cwd);
    }
    case MOS_DYLD_SYM: {
        char name[1024];
        if (!ustr(p, b, name, sizeof name)) return -MOS_EFAULT;
        return (i64)dyld_sym(K, (i64)a, name, c);
    }
    case MOS_DYLD_CLOSE:
        /* 1 = esta foi a última referência. A imagem continua mapeada: o mOS não descarrega.
         * dlfcn.c usa o 1 para avisar remove_image num dylib sem seção __objc_/__swift. */
        if (a >= (u64)K->n || !K->im[a] || K->im[a]->refs == 0) return 0;
        K->im[a]->refs--;
        return K->im[a]->refs == 0 ? 1 : 0;
    case MOS_DYLD_COUNT: return K->n;
    case MOS_DYLD_IMAGE: {
        if (a >= (u64)K->n) return 0;
        image_t *I = K->im[a];
        if (b && c) {
            usize l = strlen(I->path);
            if (l + 1 > c) l = c - 1;
            char z = 0;
            if (!uvm_copy_out(p, b, I->path, l) || !uvm_copy_out(p, b + l, &z, 1)) return -MOS_EFAULT;
        }
        return (i64)I->hdr;
    }
    case MOS_DYLD_FIND: { int i = image_at(K, a); return i >= 0 ? i : -MOS_ESRCH; }
    case MOS_DYLD_ERROR: {
        usize l = strlen(K->lasterr);
        if (!a || !b) return (i64)l;
        if (l + 1 > b) l = b - 1;
        char z = 0;
        if (!uvm_copy_out(p, a, K->lasterr, l) || !uvm_copy_out(p, a + l, &z, 1)) return -MOS_EFAULT;
        return (i64)l;
    }
    }
    return -MOS_EINVAL;
}

/* ================================================================ argv / pilha */
static int split_args(const char *arg, char **out, int max, char *store, usize cap) {
    int n = 0;
    usize o = 0;
    const char *p = arg;
    while (p && *p && n < max) {
        while (*p == ' ' || *p == '\t') p++;
        if (!*p) break;
        out[n++] = store + o;
        char q = 0;
        while (*p && (q || (*p != ' ' && *p != '\t'))) {
            if (!q && (*p == '"' || *p == '\'')) { q = *p++; continue; }
            if (q && *p == q) { q = 0; p++; continue; }
            if (*p == '\\' && p[1]) p++;
            if (o + 1 < cap) store[o++] = *p;
            p++;
        }
        if (o < cap) store[o++] = 0;
    }
    return n;
}

typedef struct { u8 *buf; usize cap, used; u64 top; } ustack_t;   /* cresce para baixo */

static u64 us_push(ustack_t *s, const void *data, usize n, usize align) {
    usize pos = (s->cap - s->used - n) & ~(align - 1);
    if (pos > s->cap || s->cap - pos < n) return 0;
    memcpy(s->buf + pos, data, n);
    s->used = s->cap - pos;
    return s->top - (s->cap - pos);
}

/* diretório atual do próximo processo (macho_spawn) */
static char next_cwd[256] = "/Users/Visitante";
void macho_set_cwd(const char *cwd) { snprintf(next_cwd, sizeof next_cwd, "%s", cwd && cwd[0] == '/' ? cwd : "/Users/Visitante"); }

#ifndef MOS_HOST_TEST
/* ================================================================ imagens compartilhadas entre processos (icache)
 * O Electron Framework (208 MiB) era montado num buffer contíguo do kernel e copiado inteiro em cada processo: o VS
 * Code tem 6 a 8 processos com ele, e numa VM de 2,7 GiB o 6º já não achava 208 MiB contíguos ("sem memória para a
 * imagem"): o host de extensões não subia. Agora:
 *  - a imagem grande é montada direto nas páginas do processo (CR3 dele durante o image_fix; a syscall roda com as
 *    interrupções desligadas), sem o buffer contíguo;
 *  - depois dos fixups o conteúdo só depende do link (bases e tamanhos das imagens, área de stubs, libSystem). Para o
 *    mesmo link, as páginas físicas da primeira carga entram nos processos seguintes como cópia-na-escrita (o mesmo
 *    mecanismo do fork, process.c): só a página que um processo escrever vira dele. Os efeitos da montagem no link
 *    (stubs de símbolos ausentes, na ordem; inicializadores) ficam guardados e são repetidos;
 *  - a entrada sai quando nenhum processo a usa (macho_icache_trim, ao liberar um espaço de endereços). Tirar uma
 *    entrada só larga a referência do cache: quem ainda mapeia as páginas continua com elas. */
#define ICACHE_MAX 6
#define ICACHE_MIN (16ull << 20)         /* só imagens grandes */
typedef struct {
    bool used;
    u64 key, base, size, npages;
    u64 *phys;
    char **missing; u32 nmissing;        /* símbolos ausentes que a montagem acrescentou, na ordem */
    u64 *inits; u32 ninits;              /* inicializadores que ela registrou */
} icache_t;
static icache_t icache[ICACHE_MAX];
bool uvm_share_cow(process_t *p, u64 va, u64 *phys);
bool uvm_map_cow(process_t *p, u64 va, u64 phys);
u16 uvm_page_refs(u64 phys);
void uvm_page_put(u64 phys);
void uvm_page_get(u64 phys);

static u64 fnv(u64 h, const void *d, usize n) { const u8 *b = d; for (usize i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ull; } return h; }
static u64 fnv64(u64 h, u64 v) { return fnv(h, &v, 8); }
/* tudo o que muda o conteúdo de I depois dos fixups: o link inteiro (o executável pelo conteúdo, as outras imagens pelo
 * caminho e pelos bytes no VFS), a ordem de montagem, a área de stubs e os modos do carregador */
static u64 icache_key(link_t *K, image_t *I) {
    u64 h = 1469598103934665603ull;
    h = fnv64(h, (u64)K->n); h = fnv64(h, K->stub_base); h = fnv64(h, K->end); h = fnv64(h, (u64)K->first_new);
    h = fnv64(h, (u64)K->tolerant); h = fnv64(h, (u64)K->tlv_per_thread); h = fnv64(h, (u64)K->norder);
    h = fnv64(h, (u64)K->arch);
    for (int k = 0; k < K->norder; k++) h = fnv64(h, (u64)K->order[k]);
    for (int i = 0; i < K->n; i++) {
        image_t *J = K->im[i];
        h = fnv64(h, J->base); h = fnv64(h, J->mo.vmax - J->mo.vmin); h = fnv64(h, (u64)J->mo.len); h = fnv64(h, (u64)J->idx);
        if (i) { h = fnv(h, J->path, strlen(J->path)); h = fnv64(h, (u64)(usize)J->mo.m); }
        else h = fnv(h, J->mo.m, J->mo.len < (4u << 20) ? J->mo.len : (4u << 20));
    }
    h = fnv(h, I->path, strlen(I->path));
    return fnv64(h, I->base);
}
static icache_t *icache_find(u64 key, image_t *I) {
    u64 size = I->mo.vmax - I->mo.vmin;
    for (int i = 0; i < ICACHE_MAX; i++)
        if (icache[i].used && icache[i].key == key && icache[i].base == I->base && icache[i].size == size) return &icache[i];
    return NULL;
}
static void icache_drop(icache_t *c) {
    for (u64 j = 0; j < c->npages; j++) if (c->phys[j]) uvm_page_put(c->phys[j]);
    for (u32 i = 0; i < c->nmissing; i++) kfree(c->missing[i]);
    kfree(c->missing); kfree(c->inits); kfree(c->phys);
    memset(c, 0, sizeof *c);
}
/* sem processo usando: só o cache segura a página do cabeçalho (ninguém escreve nela) */
static void tcache_trim(void);
void macho_icache_trim(void) {
    /* a amostra é a última página (o __LINKEDIT, só leitura): a primeira é do __TEXT, que pode vir emprestado do arquivo
     * (tcache_borrow) e então tem o VFS e o tcache como donos a mais, e a entrada nunca seria liberada */
    for (int i = 0; i < ICACHE_MAX; i++)
        if (icache[i].used && icache[i].npages && uvm_page_refs(icache[i].phys[icache[i].npages - 1]) <= 1) {
            kprintf("macho: imagem compartilhada liberada (%llu MiB)\n", (unsigned long long)(icache[i].size >> 20));
            icache_drop(&icache[i]);
        }
    tcache_trim();                          /* depois do icache: ele também segura páginas do __TEXT */
}
/* acerto no cache: repete os efeitos da montagem no link e mapeia as páginas em COW.
 * 1 = mapeada; 0 = falta memória; -1 = o link divergiu (quem chama monta a imagem do jeito normal) */
static int icache_map(process_t *p, link_t *K, image_t *I, icache_t *c) {
    u32 m0 = K->nmissing;
    for (u32 i = 0; i < c->nmissing; i++) missing_stub(K, c->missing[i]);
    if (K->nmissing != m0 + c->nmissing) { kprintf("macho: link divergiu do cache em %s; montando de novo\n", I->path); return -1; }
    for (u32 i = 0; i < c->ninits; i++) push_init(K, c->inits[i]);
    for (int i = 0; i < I->mo.nobjc && K->nobjc < MAX_OBJC; i++) {
        K->objc[K->nobjc].addr = I->mo.objc[i].addr + (u64)I->slide;
        K->objc[K->nobjc].size = I->mo.objc[i].size;
        K->objc[K->nobjc++].kind = I->mo.objc[i].kind;
    }
    for (u64 j = 0; j < c->npages; j++)
        if (!uvm_map_cow(p, I->base + j * 4096, c->phys[j])) return 0;
    return 1;
}
/* primeira carga de uma imagem grande: as páginas do processo viram COW e o cache fica com uma referência */
static void icache_add(process_t *p, link_t *K, image_t *I, u64 key, u32 m0, u32 i0) {
    u64 size = I->mo.vmax - I->mo.vmin;
    icache_t *c = NULL;
    for (int pass = 0; pass < 2 && !c; pass++) {
        if (pass) macho_icache_trim();
        for (int i = 0; i < ICACHE_MAX && !c; i++) if (!icache[i].used) c = &icache[i];
    }
    if (!c) return;
    u64 np = (size + 4095) / 4096;
    u32 nm = K->nmissing - m0, ni = K->ninits - i0;
    u64 *phys = kcalloc(np, sizeof *phys);
    char **miss = nm ? kcalloc(nm, sizeof *miss) : NULL;
    u64 *inits = ni ? kcalloc(ni, sizeof *inits) : NULL;
    if (!phys || (nm && !miss) || (ni && !inits)) { kfree(phys); kfree(miss); kfree(inits); return; }
    for (u32 i = 0; i < nm; i++) miss[i] = strdup(K->missing[m0 + i]);
    for (u32 i = 0; i < ni; i++) inits[i] = K->inits[i0 + i];
    c->used = true; c->key = key; c->base = I->base; c->size = size; c->phys = phys; c->npages = 0;
    c->missing = miss; c->nmissing = nm; c->inits = inits; c->ninits = ni;
    for (u64 j = 0; j < np; j++, c->npages++)
        if (!uvm_share_cow(p, I->base + j * 4096, &phys[j])) { icache_drop(c); return; }
    const char *s = strrchr(I->path, '/');
    kprintf("macho: %s (%llu MiB) compartilhada entre processos (cópia na escrita)\n", s ? s + 1 : I->path,
            (unsigned long long)(size >> 20));
}

/* ================================================================ __TEXT compartilhado entre links diferentes (tcache)
 * O icache só serve processos com o mesmo link (mesmos endereços). Mas o __TEXT de uma imagem (código e constantes, 290
 * dos 307 MiB do Chromium Framework e 203 dos 215 MiB do Electron Framework) não depende do endereço de carga quando nenhum
 * fixup cai nele: os chained fixups ficam nos segmentos de dados. O Chromium carrega o framework por dlopen no processo
 * principal e linkado nos helpers, em endereços diferentes, e o principal de um app Electron tem outro executável que os
 * helpers: eram duas cópias de cada framework, e com o Slack aberto numa VM de 2 GiB os helpers do Chromium não subiam
 * ("sem memória para o processo"). A primeira montagem direta de uma imagem grande guarda aqui as páginas do __TEXT
 * (cópia na escrita, como o icache); as montagens seguintes do mesmo arquivo, em qualquer endereço, mapeiam essas páginas
 * em vez de alocar e copiar, e o image_fix só monta o resto. */
#define TCACHE_MAX 6
/* borrowed: as páginas são as do próprio arquivo no VFS (tcache_borrow), e o VFS continua dono implícito delas */
typedef struct { bool used, borrowed; const u8 *file; usize flen; u64 npages, fileoff; u64 *phys; } tcache_t;
static tcache_t tcache[TCACHE_MAX];

/* o __TEXT de I pode ser compartilhado? Só com chained fixups, inteiro vindo do arquivo e sem nenhum fixup dentro dele */
static bool text_range(image_t *I, u64 *lo, u64 *np, u64 *fileoff) {
    mach_t *mo = &I->mo;
    if (!mo->chained) return false;
    const seg_t *t = NULL;
    for (int i = 0; i < mo->nsegs && !t; i++) if (!strcmp(mo->segs[i].name, "__TEXT")) t = &mo->segs[i];
    if (!t || t->zero || (t->vmaddr & 0xFFF) || (t->vmsize & 0xFFF) || t->vmsize < ICACHE_MIN || t->filesize < t->vmsize ||
        t->fileoff + t->vmsize > mo->len)
        return false;
    u32 dataoff = rd32(mo->chained + 8), datasize = rd32(mo->chained + 12);
    if ((u64)dataoff + datasize > mo->len || datasize < 28) return false;
    const u8 *h = mo->m + dataoff, *hend = h + datasize;
    u32 starts_off = rd32(h + 4);
    if ((u64)starts_off + 4 > datasize) return false;
    const u8 *si = h + starts_off;
    u32 seg_count = rd32(si);
    u64 tl = t->vmaddr, th = t->vmaddr + t->vmsize;
    for (u32 s = 0; s < seg_count; s++) {
        if (si + 8 + (u64)s * 4 > hend) return false;
        u32 so = rd32(si + 4 + s * 4);
        if (!so) continue;
        const u8 *ss = si + so;
        if (ss + 22 > hend) return false;
        u16 page_size = rd16(ss + 4), page_count = rd16(ss + 20);
        u64 a = mo->vmin + rd64(ss + 8);
        if (a + (u64)page_count * page_size <= tl || a >= th) continue;     /* fixups de outro segmento */
        for (u16 pg = 0; pg < page_count; pg++) {
            if (ss + 24 + (u64)pg * 2 > hend) return false;
            u64 pa = a + (u64)pg * page_size;
            if (pa + page_size > tl && pa < th && rd16(ss + 22 + pg * 2) != 0xFFFF) return false;   /* fixup no __TEXT */
        }
    }
    *lo = tl; *np = t->vmsize / 4096; *fileoff = t->fileoff;
    return true;
}
static void tcache_drop(tcache_t *c) {
    for (u64 j = 0; j < c->npages; j++) if (c->phys[j]) uvm_page_put(c->phys[j]);
    kfree(c->phys);
    memset(c, 0, sizeof *c);
}
/* entradas que nenhum processo usa mais (o cache é o único dono das páginas conferidas) */
static void tcache_trim(void) {
    for (int i = 0; i < TCACHE_MAX; i++) {
        tcache_t *c = &tcache[i];
        if (!c->used || !c->npages) continue;
        u64 n = c->npages;
        u16 lim = c->borrowed ? 2 : 1;      /* emprestada: o arquivo também é dono (se o VFS já largou, sobra 1) */
        if (uvm_page_refs(c->phys[0]) <= lim && uvm_page_refs(c->phys[n / 2]) <= lim && uvm_page_refs(c->phys[n - 1]) <= lim) {
            kprintf("macho: __TEXT compartilhado liberado (%llu MiB)\n", (unsigned long long)(n >> 8));
            tcache_drop(c);
        }
    }
}
/* entrada do mesmo arquivo; as páginas guardadas são conferidas por amostragem com o arquivo (o VFS pode ter regravado o
 * arquivo no mesmo endereço) */
static tcache_t *tcache_find(image_t *I) {
    for (int i = 0; i < TCACHE_MAX; i++) {
        tcache_t *c = &tcache[i];
        if (!c->used || c->file != I->mo.m || c->flen != I->mo.len || c->npages != I->tnp || c->fileoff != I->tfileoff) continue;
        for (u64 k = 0; k <= 16; k++) {
            u64 j = k * (c->npages - 1) / 16;
            if (memcmp((const void *)c->phys[j], I->mo.m + c->fileoff + j * 4096, 4096)) {
                kprintf("macho: __TEXT guardado não confere com o arquivo; descartado\n");
                tcache_drop(c);
                return NULL;
            }
        }
        return c;
    }
    return NULL;
}
/* Sem entrada ainda: se o arquivo está num buffer de kmalloc_pages (os grandes lidos do disco, fat.c) e o __TEXT começa
 * numa página dele, a entrada usa as próprias páginas do arquivo, sem cópia. Cada página ganha a referência do cache
 * (pref[], process.c); o VFS segue dono implícito, o vfs_append nunca escreve num buffer desses no lugar, e o kfree dele
 * larga só a sua parte. Antes eram duas cópias do __TEXT na RAM, o arquivo e as páginas montadas: 203 MiB a mais com o
 * Electron Framework e 248 MiB com o Google Chrome Framework, e o Claude com o Chrome não cabiam em 2816 MB. */
static tcache_t *tcache_borrow(image_t *I) {
    const u8 *src = I->mo.m + I->tfileoff;
    if (!I->tnp || !kmalloc_is_pages(I->mo.m) || ((u64)src & 0xFFF) || I->tfileoff + I->tnp * 4096 > I->mo.len) return NULL;
    tcache_t *c = NULL;
    for (int pass = 0; pass < 2 && !c; pass++) {
        if (pass) tcache_trim();
        for (int i = 0; i < TCACHE_MAX && !c; i++) if (!tcache[i].used) c = &tcache[i];
    }
    u64 *phys = c ? kcalloc(I->tnp, sizeof *phys) : NULL;
    if (!phys) return NULL;
    c->used = true; c->borrowed = true; c->file = I->mo.m; c->flen = I->mo.len; c->fileoff = I->tfileoff; c->phys = phys;
    for (u64 j = 0; j < I->tnp; j++) { phys[j] = (u64)src + j * 4096; uvm_page_get(phys[j]); }
    c->npages = I->tnp;
    const char *s = strrchr(I->path, '/');
    kprintf("macho: %s: __TEXT (%llu MiB) mapeado das páginas do arquivo, sem cópia\n", s ? s + 1 : I->path,
            (unsigned long long)(I->tnp >> 8));
    return c;
}
/* antes de mapear: para cada imagem grande nova (sem acerto no icache: hit[i] nulo), se o __TEXT vem do tcache */
static void tcache_plan(link_t *K, int from, void *const *hit) {
    for (int i = from; i < K->n; i++) {
        image_t *I = K->im[i];
        I->text_shared = false; I->tc = NULL; I->tnp = 0;
        u64 lo, np, fo;
        if ((hit && hit[i]) || I->mo.vmax - I->mo.vmin < ICACHE_MIN || !text_range(I, &lo, &np, &fo)) continue;
        I->tlo = lo + (u64)I->slide; I->tnp = np; I->tfileoff = fo;
        if ((I->tc = tcache_find(I)) || (I->tc = tcache_borrow(I))) I->text_shared = true;
    }
}
/* páginas de [a, b): as do __TEXT de uma imagem com text_shared vêm do tcache (cópia na escrita), o resto é memória nova */
static bool map_fresh(process_t *p, link_t *K, int from, u64 a, u64 b) {
    while (a < b) {
        u64 next = b;
        image_t *T = NULL;
        for (int i = from; i < K->n; i++) {
            image_t *I = K->im[i];
            if (!I->text_shared) continue;
            u64 lo = I->tlo, hi = lo + I->tnp * 4096;
            if (a >= lo && a < hi) { T = I; next = MIN(hi, b); break; }
            if (lo > a && lo < next) next = lo;
        }
        if (T) {
            tcache_t *c = T->tc;
            for (u64 v = a; v < next; v += 4096) if (!uvm_map_cow(p, v, c->phys[(v - T->tlo) / 4096])) return false;
        } else if (!uvm_map(p, a, next - a)) return false;
        a = next;
    }
    return true;
}
/* depois da montagem direta: o __TEXT novo (ainda sem entrada) fica guardado para os próximos processos */
static void tcache_after_mount(process_t *p, image_t *I) {
    if (I->text_shared) {
        const char *s = strrchr(I->path, '/');
        tcache_t *c = I->tc;
        kprintf("macho: %s: __TEXT (%llu MiB) compartilhado com %s\n", s ? s + 1 : I->path, (unsigned long long)(I->tnp >> 8),
                c && c->borrowed ? "o arquivo" : "outro processo");
    } else if (I->tnp) {
        tcache_t *c = NULL;
        for (int pass = 0; pass < 2 && !c; pass++) {
            if (pass) tcache_trim();
            for (int i = 0; i < TCACHE_MAX && !c; i++) if (!tcache[i].used) c = &tcache[i];
        }
        u64 *phys = c ? kcalloc(I->tnp, sizeof *phys) : NULL;
        if (phys) {
            c->used = true; c->file = I->mo.m; c->flen = I->mo.len; c->fileoff = I->tfileoff; c->phys = phys; c->npages = 0;
            for (u64 j = 0; j < I->tnp; j++, c->npages++)
                if (!uvm_share_cow(p, I->tlo + j * 4096, &phys[j])) { tcache_drop(c); c = NULL; break; }
            if (c && c->npages != I->tnp) { tcache_drop(c); c = NULL; }
        }
    }
    I->text_shared = false; I->tc = NULL; I->tnp = 0;
}
#endif

/* ================================================================ entrada principal */
/* ================================================================ processos arm64 (tradutor, docs/tradutor-arm64.md)
 * O executável e as dylibs são arm64 e ligados ao libSystem arm64; a thread começa no runtime a64rt.mos (x86_64), mapeado
 * em A64RT_BASE com uma pilha do host de A64RT_HSTACK alinhada ao próprio tamanho, e recebe em rdi o a64rt_boot_t. */
static bool arm64_forced(process_t *p) {
#ifdef MOS_HOST_TEST
    (void)p;
    const char *v = getenv("MOS_ARCH");
    return v && !strcmp(v, "arm64");
#else
    if (vfs_lookup(A64RT_DIR "/preferir-arm64")) return true;
    for (int i = 0; p->spawn_env && i < p->spawn_envc; i++)
        if (p->spawn_env[i] && !strcmp(p->spawn_env[i], "MOS_ARCH=arm64")) return true;
    return false;
#endif
}

static struct { const u8 *e; usize len; u8 *img; u64 span; } rt_db;

static bool map_a64rt(process_t *p, u64 gentry, u64 gsp, u64 gx0, u64 hdr, bool forced,
                      u64 *entry, u64 *rsp, u64 *rdi, char *err, usize errlen) {
    if (!rt_db.e) {
        usize n = 0;
        const u8 *f = dep_read(A64RT_PATH, &n);
        if (!f || n < 64 || memcmp(f, "\x7F" "ELF", 4) || rd16(f + 18) != 62 || rd16(f + 16) != 3) {
            snprintf(err, errlen, "app arm64: o runtime do tradutor (%s) não está instalado", A64RT_PATH);
            return false;
        }
        u8 *c = kmalloc_pages(n);
        if (!c) { snprintf(err, errlen, "sem memória para o runtime arm64"); return false; }
        memcpy(c, f, n);
        rt_db.e = c; rt_db.len = n;
    }
    u64 span = map_elf(p, rt_db.e, rt_db.len, A64RT_BASE, 8, A64RT_SPAN_MAX - 2 * A64RT_HSTACK, &rt_db.img, &rt_db.span,
                       "runtime arm64 (a64rt.mos)", err, errlen);
    if (!span) return false;
    u64 hs = (A64RT_BASE + span + A64RT_HSTACK - 1) & ~(A64RT_HSTACK - 1);
    if (!uvm_map(p, hs, A64RT_HSTACK)) { snprintf(err, errlen, "sem memória para a pilha do runtime arm64"); return false; }
    a64rt_boot_t b;
    memset(&b, 0, sizeof b);
    b.magic = A64RT_BOOT_MAGIC; b.version = 1;
    b.guest_entry = gentry; b.guest_sp = gsp; b.guest_x0 = gx0;
    b.ls_base = LIBSYSTEM_BASE; b.ls_size = ls_db[1].span ? ls_db[1].span : 0;
    b.rt_base = A64RT_BASE; b.rt_size = span;
    b.hstack_lo = hs;
    b.flags = forced ? A64RT_BOOT_FORCED : 0;
#ifdef MOS_HOST_TEST
    if (getenv("MOS_A64_DEBUG")) b.flags |= A64RT_BOOT_DEBUG | (!strcmp(getenv("MOS_A64_DEBUG"), "2") ? A64RT_BOOT_TRACE : 0);
    const char *eng = getenv("MOS_A64_ENGINE");
    if (eng && !strcmp(eng, "interp")) b.flags |= A64RT_BOOT_INTERP;
    if (eng && !strcmp(eng, "jit")) b.flags |= A64RT_BOOT_JIT;
#else
    if (vfs_lookup(A64RT_DIR "/depurar")) b.flags |= A64RT_BOOT_DEBUG;
    if (vfs_lookup(A64RT_DIR "/interpretador")) b.flags |= A64RT_BOOT_INTERP;
#endif
    b.exe_hdr = hdr;
    u64 bva = (hs + A64RT_HSTACK - sizeof b) & ~15ULL;
    if (!uvm_copy_out(p, bva, &b, sizeof b)) { snprintf(err, errlen, "falha ao montar o runtime arm64"); return false; }
    *entry = A64RT_BASE + rd64(rt_db.e + 24);
    *rsp = bva - 8;                                     /* 16n - 8, como logo após um call */
    *rdi = bva;
    kprintf("macho: processo arm64 pelo tradutor (runtime %llu KiB%s)\n", (unsigned long long)(span >> 10), forced ? ", arm64 pedido" : "");
    return true;
}

bool macho_load(process_t *p, const u8 *img, usize len, const char *arg, u64 *entry,
                u64 *user_rsp, u64 *rdi, char *err, usize errlen) {
    bool force_arm64 = arm64_forced(p);
    ls_cur = choose_arch(img, len, force_arm64);           /* arm64 só com o tradutor (a64rt.mos + libSystem arm64) */
    if (!libsystem_index(err, errlen)) return false;
    link_t *K = link_new(err, errlen, false);
    if (!K) return false;
    bool ok = link_open(K, p->path, img, len);
    /* LC_UNIXTHREAD (alvo anterior ao macOS 10.8, como o spawn-helper do node-pty do VS Code): o start do crt1 só monta
     * argc/argv/envp e chama exit(main(...)), que é o que o cx_start do libSystem faz com o LC_MAIN. Entra pelo _main
     * exportado; as cópias de NXArgc/environ do crt1 dentro do executável ficam sem valor. */
    if (ok && !K->im[0]->mo.has_main && K->im[0]->mo.has_thread) {
        image_t *I0 = K->im[0];
        u64 mainp = image_export(K, 0, "_main", 0);
        u64 text = 0;
        bool have_text = false;
        for (int i = 0; i < I0->mo.nsegs; i++)
            if (!strcmp(I0->mo.segs[i].name, "__TEXT")) { text = I0->mo.segs[i].vmaddr + (u64)I0->slide - I0->mo.segs[i].fileoff; have_text = true; }
        if (mainp && have_text && mainp >= text) { I0->mo.entryoff = mainp - text; I0->mo.has_main = true; }
    }
    if (ok && !K->im[0]->mo.has_main) { snprintf(err, errlen, "sem LC_MAIN (executáveis anteriores ao macOS 10.8 não são suportados)"); ok = false; }
    if (ok && K->nabsent && !K->tolerant) {
        char need[200];
        absent_summary(K, need, sizeof need);
        snprintf(err, errlen, "requer %s, que o mOS ainda não tem", need);
        ok = false;
    }
    if (ok) ok = map_libsystem(p, err, errlen);
    DBG("libsystem mapeado %d\n", ok);
#ifdef MOS_HOST_TEST
    if (ok && !uvm_map(p, IMAGE_BASE, K->end - IMAGE_BASE)) { snprintf(err, errlen, "sem memória para o processo"); ok = false; }
#else
    /* imagens grandes que um processo com o mesmo link já montou entram compartilhadas (icache_map); o resto da faixa
     * (as outras imagens e as folgas entre elas) é memória nova, como antes */
    icache_t *hit[MAX_IMAGES];
    u64 ikey[MAX_IMAGES];
    for (int i = 0; i < K->n; i++) {
        image_t *I = K->im[i];
        hit[i] = NULL; ikey[i] = 0;
        if (!ok || I->mo.vmax - I->mo.vmin < ICACHE_MIN) continue;
        ikey[i] = icache_key(K, I);
        hit[i] = icache_find(ikey[i], I);
    }
    if (ok) tcache_plan(K, 0, (void *const *)hit);   /* __TEXT de outro link do mesmo arquivo (ex.: o dlopen do principal) */
    for (u64 a = IMAGE_BASE; ok && a < K->end;) {
        u64 next = K->end;
        bool shared = false;
        for (int i = 0; i < K->n; i++) {
            if (!hit[i]) continue;
            u64 b = K->im[i]->base, e = (b + K->im[i]->mo.vmax - K->im[i]->mo.vmin + 0xFFF) & ~0xFFFULL;
            if (a >= b && a < e) { shared = true; next = e; break; }
            if (b > a && b < next) next = b;
        }
        if (!shared && !map_fresh(p, K, 0, a, next)) { snprintf(err, errlen, "sem memória para o processo"); ok = false; }
        a = next;
    }
#endif
    /* cada imagem recebe os fixups e vai para o processo (uma por vez) */
    for (int k = 0; ok && k < K->norder; k++) {
        image_t *I = K->im[K->order[k]];
        u64 size = I->mo.vmax - I->mo.vmin;
#ifndef MOS_HOST_TEST
        icache_t *c = hit[K->order[k]];
        if (c) {
            int r = icache_map(p, K, I, c);
            if (r > 0) {
                extern void tp_arm(process_t *p, const char *path, u64 base, u64 vmin);
                tp_arm(p, I->path, I->base, I->mo.vmin);
                continue;
            }
            if (!r || !uvm_map(p, I->base, size)) { snprintf(err, errlen, "sem memória para o processo"); ok = false; break; }
        }
        u32 m0 = K->nmissing, i0 = K->ninits;
        if (size >= ICACHE_MIN) {
            /* grande: montada direto nas páginas do processo (já mapeadas e zeradas), sem buffer contíguo no kernel */
            u64 fl = irq_save(), old_cr3 = read_cr3();          /* sem troca de tarefa com o CR3 emprestado */
            __asm__ volatile("mov %0, %%cr3" :: "r"((u64)p->pml4) : "memory");
            ok = image_fix(K, I, (u8 *)I->base);
            __asm__ volatile("mov %0, %%cr3" :: "r"(old_cr3) : "memory");
            irq_restore(fl);
            if (!ok) break;
            tcache_after_mount(p, I);
            if (ikey[K->order[k]]) icache_add(p, K, I, ikey[K->order[k]], m0, i0);
        } else
#endif
        {
            u8 *buf = kcalloc(1, size);
            if (!buf) { snprintf(err, errlen, "sem memória para a imagem (%llu KiB)", (unsigned long long)(size >> 10)); ok = false; break; }
            ok = image_fix(K, I, buf);
            if (ok && !uvm_copy_out(p, I->base, buf, size)) { snprintf(err, errlen, "falha ao copiar a imagem"); ok = false; }
            kfree(buf);
        }
#ifndef MOS_HOST_TEST
        /* diagnóstico: pontos de /Sistema/Biblioteca/diag-tracepoints (kernel/proc/tracepoint.c) */
        if (ok) { extern void tp_arm(process_t *p, const char *path, u64 base, u64 vmin); tp_arm(p, I->path, I->base, I->mo.vmin); }
#endif
    }
    if (!ok) { link_free(K); return false; }
    if (K->n > 1) kprintf("macho: %d imagens carregadas (executável + bibliotecas do bundle)\n", K->n);
#ifdef MOS_HOST_TEST
    if (getenv("MACHOTEST_MAP"))
        for (int i = 0; i < K->n; i++) {
            const char *s = strrchr(K->im[i]->path, '/');
            kprintf("  %012llx-%012llx %s (ObjC: %d seções)\n", (unsigned long long)K->im[i]->base,
                    (unsigned long long)(K->im[i]->base + K->im[i]->mo.vmax - K->im[i]->mo.vmin), s ? s + 1 : K->im[i]->path, K->im[i]->mo.nobjc);
        }
#endif
    if (K->nabsent) kprintf("macho: modo experimental, %d biblioteca(s) do sistema ausente(s) (ex.: %s)\n", K->nabsent, K->absent);

    usize area, inits_off, objc_off;
    u8 *stubs = stub_area(K, &area, &inits_off, &objc_off);
    if (!stubs) { snprintf(err, errlen, "sem memória"); link_free(K); return false; }
    if (K->nmissing) kprintf("macho: %u símbolo(s) ausente(s) no libSystem do mOS (ex.: %s)\n", K->nmissing, K->missing[0]);
    ok = uvm_map(p, K->stub_base, area) && uvm_copy_out(p, K->stub_base, stubs, area);
    kfree(stubs);
    if (!ok) { snprintf(err, errlen, "sem memória para o processo"); link_free(K); return false; }
    DBG("imagem copiada\n");
    p->brk = p->brk_start = K->stub_base + area + 0x10000;

    image_t *I0 = K->im[0];
    mach_t *mo = &I0->mo;
    /* pilha: strings + vetores + bloco de partida */
    u64 slo = USER_STACK_TOP - USER_STACK_SIZE;
    if (!uvm_map(p, slo, USER_STACK_SIZE)) { snprintf(err, errlen, "sem memória para a pilha"); link_free(K); return false; }
    ustack_t st = {NULL, 0, 0, USER_STACK_TOP};
    /* o Electron passa dezenas de KB de argv (a chave ",r," da memória compartilhada) e o
     * ambiente do LaunchOptions. O caminho curto (arg string) continua para o shell. */
    bool full = p->spawn_argc > 0 && p->spawn_argv;
    usize scap = full || p->spawn_envc > 0 ? (800u << 10) : 32768;
    st.buf = kmalloc(scap);
    st.cap = scap;
    st.used = 0;
    char *argstore = full ? NULL : kmalloc(4096);
    char **av = full ? NULL : kmalloc(sizeof(char *) * 64);
    u64 *tmp = kmalloc(sizeof(u64) * 3 * MAX_OBJC);
    if (!st.buf || !tmp || (!full && (!argstore || !av))) {
        kfree(st.buf); kfree(argstore); kfree(av); kfree(tmp);
        snprintf(err, errlen, "sem memória");
        link_free(K);
        return false;
    }
    int argc = 1;
    if (full) argc = p->spawn_argc;
    else { av[0] = p->path; argc += split_args(arg, av + 1, 62, argstore, 4096); }
    u64 *uargv = kmalloc(sizeof(u64) * (usize)(argc > 0 ? argc : 1));
    int envc = p->spawn_envc > 0 && p->spawn_env ? p->spawn_envc : 8;
    u64 *uenv = kmalloc(sizeof(u64) * (usize)envc);
    u64 uapple[2];
    bool args_ok = uargv && uenv;
    /* As strings vão num bloco só, em ordem crescente como no XNU: argv[0] < argv[1] < ... < envp... < apple.
     * O libuv (process.title do Node/Electron) e o setproctitle reescrevem o argv em lugar contando com isso:
     * tamanho = argv[argc-1] + strlen - argv[0]. Com as strings empilhadas uma a uma de cima para baixo (ordem
     * invertida), o tamanho saía negativo e o memset do "Cursor Helper: shared-process" passava do topo da pilha.
     * Primeiro só se medem os tamanhos; os endereços saem do começo do bloco. */
    usize *soff = kmalloc(sizeof(usize) * ((usize)argc + (usize)envc + 1));
    if (!soff) args_ok = false;
    usize sblk = 0;
    for (int i = 0; args_ok && i < argc; i++) {
        const char *s = full ? p->spawn_argv[i] : av[i];
        soff[i] = sblk;
        sblk += strlen(s ? s : "") + 1;
    }
    char pwd[272];
    snprintf(pwd, sizeof pwd, "PWD=%s", next_cwd);
    macho_set_cwd(NULL);
    /* SHELL: o bash do Homebrew quando está na imagem (o VS Code roda "$SHELL -i -l -c" para ler o ambiente de
     * login e abre o terminal integrado com ele). O msh do Terminal não é um arquivo, então sem bash não há um
     * shell que aceite -c: o caminho antigo continua e o spawn falha logo (ENOENT) em vez de esperar o prazo. */
    usize bash_len;                                      /* dep_read: vfs_lookup no kernel, arquivo no harness do host */
    const char *shell_env = dep_read("/usr/local/bin/bash", &bash_len) ? "SHELL=/usr/local/bin/bash" : "SHELL=/bin/msh";
    const char *envs[] = {"HOME=/Users/Visitante", "USER=visitante", "LOGNAME=visitante",
                          "PATH=/usr/local/bin:/Sistema/Programas:/usr/bin:/bin", p->tty ? "TERM=xterm-256color" : "TERM=dumb",   /* sessão do Terminal: isatty(0..2) verdadeiro */
                          "LANG=pt_BR.UTF-8",
                          pwd, shell_env};
    bool own_env = p->spawn_envc > 0 && p->spawn_env;
    int nenv = own_env ? envc : (int)ARRAY_LEN(envs);
    for (int i = 0; args_ok && i < nenv; i++) {
        const char *s = own_env ? (p->spawn_env[i] ? p->spawn_env[i] : "") : envs[i];
        soff[argc + i] = sblk;
        sblk += strlen(s) + 1;
    }
    char ep[420];           /* "executable_path=" + caminho (process_t.path tem até 384) */
    snprintf(ep, sizeof ep, "executable_path=%s", p->path);
    usize ep_off = sblk;
    sblk += strlen(ep) + 1;
    u8 *sblkbuf = args_ok ? kmalloc(sblk ? sblk : 1) : NULL;
    if (!sblkbuf) args_ok = false;
    for (int i = 0; args_ok && i < argc; i++) {
        const char *s = full ? p->spawn_argv[i] : av[i];
        if (!s) s = "";
        memcpy(sblkbuf + soff[i], s, strlen(s) + 1);
    }
    for (int i = 0; args_ok && i < nenv; i++) {
        const char *s = own_env ? (p->spawn_env[i] ? p->spawn_env[i] : "") : envs[i];
        memcpy(sblkbuf + soff[argc + i], s, strlen(s) + 1);
    }
    if (args_ok) memcpy(sblkbuf + ep_off, ep, strlen(ep) + 1);
    u64 sbase = args_ok ? us_push(&st, sblkbuf, sblk, 8) : 0;
    if (!sbase) args_ok = false;
    for (int i = 0; args_ok && i < argc; i++) uargv[i] = sbase + soff[i];
    for (int i = 0; args_ok && i < nenv; i++) uenv[i] = sbase + soff[argc + i];
    uapple[0] = args_ok ? sbase + ep_off : 0;
    uapple[1] = 0;
    kfree(sblkbuf);
    kfree(soff);
    u64 z = 0;
    us_push(&st, &z, 8, 16);
    us_push(&st, uapple, sizeof uapple, 8);
    u64 apple_va = st.top - st.used;
    u64 env_va = 0, argv_va = 0;
    if (args_ok) {
        us_push(&st, &z, 8, 8);
        us_push(&st, uenv, sizeof(u64) * (usize)envc, 8);
        env_va = st.top - st.used;
        us_push(&st, &z, 8, 8);
        us_push(&st, uargv, sizeof(u64) * (usize)argc, 8);
        argv_va = st.top - st.used;
    }
    u64 main_va = 0;
    for (int i = 0; i < mo->nsegs; i++)
        if (!strcmp(mo->segs[i].name, "__TEXT")) main_va = mo->segs[i].vmaddr + (u64)I0->slide - mo->segs[i].fileoff + mo->entryoff;
    u64 objc_va = 0;
    if (K->nobjc) {
        for (int i = 0; i < K->nobjc; i++) {
            tmp[i * 3] = K->objc[i].addr;
            tmp[i * 3 + 1] = K->objc[i].size;
            tmp[i * 3 + 2] = K->objc[i].kind;
        }
        objc_va = us_push(&st, tmp, sizeof(u64) * 3 * (usize)K->nobjc, 16);
    }
    /* inicializadores de todas as imagens, na ordem de dependência, como ponteiros absolutos */
    u64 blk[12] = {
        main_va,
        K->ninits ? K->stub_base + inits_off : 0, K->ninits,
        0, 0,
        I0->hdr, (u64)argc, argv_va, env_va, apple_va, objc_va, (u64)K->nobjc,
    };
    u64 blk_va = us_push(&st, blk, sizeof blk, 16);
    u64 ret0 = 0;
    us_push(&st, &ret0, 8, 16);                         /* rsp = 16n: "endereço de retorno" falso */
    u64 rsp = st.top - st.used - 8;                     /* entra em cx_start com rsp = 16n - 8 */
    us_push(&st, &ret0, 8, 8);
    ok = args_ok && blk_va && main_va && uvm_copy_out(p, st.top - st.used, st.buf + (st.cap - st.used), st.used);
    kfree(st.buf);
    kfree(argstore);
    kfree(av);
    kfree(uargv);
    kfree(uenv);
    kfree(tmp);
    (void)objc_off;                                     /* na partida as seções ObjC vão pela pilha */
    if (!ok) { link_free(K); snprintf(err, errlen, "falha ao montar a pilha"); return false; }
    if (K->arch) {   /* arm64: a thread começa no runtime do tradutor, que entra no cx_start arm64 com x0 = bloco */
        u64 e0 = *entry;
        if (!map_a64rt(p, ls_entry, rsp + 8, blk_va, I0->hdr, force_arm64 && choose_arch(img, len, false) == 0,
                       &e0, &rsp, &blk_va, err, errlen)) { link_free(K); return false; }
        segs_prot(p, K, 0);
        link_keep(p, K);
        *entry = e0;
        *user_rsp = rsp;
        *rdi = blk_va;
        return true;
    }
    segs_prot(p, K, 0);
    /* o grafo de imagens fica vivo para o dlopen em tempo de execução (SYS_DYLD) */
    link_keep(p, K);
    *entry = ls_entry;
    *user_rsp = rsp;
    *rdi = blk_va;
    return true;
}

#ifdef MOS_HOST_TEST
/* tools/machotest.c: nome da função do libSystem que contém addr (para diagnosticar falhas) */
const char *macho_host_symbolize(u64 addr, u64 *off) {
    /* todas as funções da tabela de símbolos do ELF, inclusive as static (locais) */
    const u8 *e = libsystem_mos;
    const char *best = NULL;
    u64 ba = 0, bias = rd16(e + 16) == 3 ? LIBSYSTEM_BASE : 0;
    u64 shoff = rd64(e + 40);
    u16 shentsize = rd16(e + 58), shnum = rd16(e + 60);
    for (u16 i = 0; i < shnum; i++) {
        const u8 *sh = e + shoff + (u64)i * shentsize;
        if (rd32(sh + 4) != 2) continue;
        u64 so = rd64(sh + 24), size = rd64(sh + 32), entsz = rd64(sh + 56);
        u64 stroff = rd64(e + shoff + (u64)rd32(sh + 40) * shentsize + 24);
        for (u64 k = 0; entsz && k < size / entsz; k++) {
            const u8 *s = e + so + k * entsz;
            if ((s[4] & 15) != 2 || !rd16(s + 6)) continue;      /* só FUNC definidas */
            u64 a = rd64(s + 8) + bias;
            if (a <= addr && a > ba) { ba = a; best = (const char *)e + stroff + rd32(s); }
        }
    }
    if (off) *off = addr - ba;
    return best;
}
#endif

/* ================================================================ relatório */
bool macho_report_image(const u8 *img, usize len, const char *name, char *out, usize cap) {
    char err[200] = "", verdict[320];
    static char body[4096];
    usize o = 0;
#define OUT(...) do { if (o < sizeof body) o += (usize)snprintf(body + o, sizeof body - o, __VA_ARGS__); } while (0)
    bool runs = false;
    body[0] = 0;
    link_t *K = NULL;
    ls_cur = choose_arch(img, len, false);              /* só arm64: relatório pelo tradutor */
    if (!libsystem_index(err, sizeof err)) snprintf(verdict, sizeof verdict, "Veredito: %s", err);
    else if (!(K = link_new(err, sizeof err, true))) snprintf(verdict, sizeof verdict, "Veredito: %s", err);
    else if (!link_open(K, name, img, len)) {
        OUT("%s\n", name);
        if (K->archs[0]) OUT("Arquiteturas: %s\n", K->archs);
        snprintf(verdict, sizeof verdict, "Veredito: não roda no mOS (%s)", err);
    } else {
        image_t *I0 = K->im[0];
        mach_t *mo = &I0->mo;
        OUT("%s\nArquiteturas: %s\n", name, K->archs);
        if (mo->minos) OUT("macOS mínimo: %u.%u\n", mo->minos >> 16, (mo->minos >> 8) & 0xFF);
        OUT("Ligação: %s\n", mo->chained ? "chained fixups (macOS 11+)" : mo->dyld_info ? "dyld_info" : "nenhuma");
        OUT("Bibliotecas:\n");
        int shown = 0;
        for (int i = 0; i < mo->ndylibs; i++) {
            char s[96];
            dylib_short(mo->dylibs[i], s, sizeof s);
            if (I0->dep[i] >= 0) { OUT("  [ok] %s (dentro do app)\n", s); continue; }
            if (libsystem_dylib_ok(mo->dylibs[i])) { OUT("  [ok] %s\n", s); continue; }
            if (shown < 10) { OUT("  [falta] %s%s\n", s, mo->dylib_weak[i] ? " (opcional)" : ""); shown++; }
        }
        if (K->n > 1) {
            u64 total = 0;
            for (int i = 1; i < K->n; i++) total += K->im[i]->mo.vmax - K->im[i]->mo.vmin;
            OUT("Bibliotecas do próprio app: %d (%llu MiB)\n", K->n - 1, (unsigned long long)(total >> 20));
        }
        char need[200] = "";
        absent_summary(K, need, sizeof need);
        if (K->nabsent) OUT("Bibliotecas do sistema que o mOS não tem (%d): %s\n", K->nabsent, need);
        bool libs_ok = !K->nabsent || K->tolerant;
        bool built = libs_ok;
        for (int k = 0; built && k < K->norder; k++) built = image_fix(K, K->im[K->order[k]], NULL);
        if (built && K->nmissing) {
            OUT("Símbolos sem implementação no libSystem do mOS (%u):", K->nmissing);
            u32 nshow = K->nmissing > 12 ? 8 : K->nmissing;      /* listas enormes escondem o veredito no Terminal */
            for (u32 i = 0; i < nshow; i++) OUT(" %s", K->missing[i]);
            if (nshow < K->nmissing) OUT(" ... e mais %u", K->nmissing - nshow);
            OUT("\n");
        }
        runs = built && mo->has_main;
        if (!libs_ok) snprintf(verdict, sizeof verdict, "Veredito: precisa de %s, que o mOS ainda não tem", need);
        else if (!built) snprintf(verdict, sizeof verdict, "Veredito: não carrega (%s)", err);
        else if (!mo->has_main) snprintf(verdict, sizeof verdict, "Veredito: formato antigo (sem LC_MAIN)");
        else if (K->nabsent) snprintf(verdict, sizeof verdict, "Veredito: modo experimental — carrega, mas para na primeira chamada a uma das %u funções ausentes", K->nmissing);
        else if (K->nmissing) snprintf(verdict, sizeof verdict, "Veredito: roda, mas para se chamar um dos símbolos acima");
        else snprintf(verdict, sizeof verdict, "Veredito: compatível — roda no mOS");
    }
    link_free(K);
#undef OUT
    /* o veredito é sempre a última linha, mesmo que o corpo precise ser cortado */
    usize vl = strlen(verdict) + 2;
    if (cap <= vl) { snprintf(out, cap, "%s", verdict); return runs; }
    usize room = cap - vl - 1, bl = strlen(body);
    if (bl > room) {
        bl = room > 5 ? room - 5 : 0;
        while (bl && body[bl - 1] != '\n') bl--;
        snprintf(out, cap, "%.*s...\n%s\n", (int)bl, body, verdict);
    } else snprintf(out, cap, "%s%s\n", body, verdict);
    return runs;
}
