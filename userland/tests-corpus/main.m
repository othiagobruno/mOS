/* mOS — teste Mach-O x86_64 das lacunas do corpus fechadas na rodada de 22/11 (tools/build.sh / build.ps1 -Tests: o
 * "teste" nativo chama este binário). Roda pelo carregador e pelo libSystem de verdade:
 *   swprintf, libffi (ffi_call com struct, variádica e closure como comparador do qsort), ndbm em arquivo,
 *   CGDisplayMode como objeto CF, NSArchiver/NSUnarchiver, NSPort, uma subclasse de NEPacketTunnelProvider
 *   (classe de fachada do classes_corpus.c), readline/histórico, vis/strunvis, matrix_identity_float4x4 e __invert_d4;
 *   e as de 2 apps: compressor bzip2, CFTree, RFC 3394, DNSServiceConstructFullName, pcap_dump, CFBagGetValues.
 * Compilar sem ARC: zig cc -target x86_64-macos -x objective-c -Iuserland/sdk/include main.m -Wl,-undefined,dynamic_lookup */
#import <Foundation/Foundation.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <wchar.h>
#include <fcntl.h>
#include <errno.h>
const char *object_getClassName(id o);

static int fails, total;
#define CK(c, m) do { total++; if (!(c)) { fails++; printf("corpusteste: FALHA %s\n", m); } } while (0)

/* declarações que o SDK mínimo do mOS não traz */
typedef struct ffi_type { size_t size; unsigned short alignment, type; struct ffi_type **elements; } ffi_type;
typedef struct { int abi; unsigned nargs; ffi_type **arg_types; ffi_type *rtype; unsigned bytes; unsigned flags; } ffi_cif;
extern ffi_type ffi_type_sint32, ffi_type_double, ffi_type_float, ffi_type_pointer, ffi_type_sint64, ffi_type_uint8;
int ffi_prep_cif(ffi_cif *, int, unsigned, ffi_type *, ffi_type **);
int ffi_prep_cif_var(ffi_cif *, int, unsigned, unsigned, ffi_type *, ffi_type **);
void ffi_call(ffi_cif *, void (*)(void), void *, void **);
void *ffi_closure_alloc(size_t, void **);
int ffi_prep_closure_loc(void *, ffi_cif *, void (*)(ffi_cif *, void *, void **, void *), void *, void *);
void ffi_closure_free(void *);
typedef struct { void *dptr; size_t dsize; } datum;
void *dbm_open(const char *, int, int);
int dbm_store(void *, datum, datum, int);
datum dbm_fetch(void *, datum);
void dbm_close(void *);
const void *CGDisplayCopyAllDisplayModes(unsigned, const void *);
long CFArrayGetCount(const void *);
const void *CFArrayGetValueAtIndex(const void *, long);
const void *CFRetain(const void *);
void CFRelease(const void *);
unsigned long CFGetTypeID(const void *);
unsigned long CGDisplayModeGetTypeID(void);
size_t CGDisplayModeGetWidth(const void *);
unsigned CGMainDisplayID(void);
void add_history(const char *);
typedef struct { char *line; char *ts; void *data; } HIST_ENTRY;
HIST_ENTRY *history_get(int);
extern int history_base, history_length;
int strvis(char *, const char *, int);
int strunvis(char *, const char *);
typedef float f4 __attribute__((ext_vector_type(4)));
typedef struct { f4 c[4]; } simd_float4x4;
extern const simd_float4x4 matrix_identity_float4x4;
typedef double d4 __attribute__((ext_vector_type(4)));
typedef struct { d4 c[4]; } simd_double4x4;
simd_double4x4 __invert_d4(simd_double4x4);
void *newwin(int, int, int, int);
int waddstr(void *, const char *);
int wmove(void *, int, int);
int winnstr(void *, char *, int);
char *tparm(const char *, long, long, long, long, long, long, long, long, long);
const char *keyname(int);
/* rodada das lacunas de 2 apps */
int BZ2_bzBuffToBuffCompress(char *, unsigned *, char *, unsigned, int, int, int);
int BZ2_bzBuffToBuffDecompress(char *, unsigned *, char *, unsigned, int, int);
typedef struct { long version; void *info; const void *(*retain)(const void *); void (*release)(const void *); void *desc; } CFTreeContext;
void *CFTreeCreate(void *, const CFTreeContext *);
void CFTreeAppendChild(void *, void *);
long CFTreeGetChildCount(void *);
void CFTreeGetChildren(void *, void **);
void CFTreeGetContext(void *, CFTreeContext *);
void *CFTreeGetParent(void *);
int CCSymmetricKeyWrap(unsigned, const unsigned char *, size_t, const unsigned char *, size_t, const unsigned char *, size_t, unsigned char *, size_t *);
int CCSymmetricKeyUnwrap(unsigned, const unsigned char *, size_t, const unsigned char *, size_t, const unsigned char *, size_t, unsigned char *, size_t *);
extern const unsigned char CCrfc3394_iv[];
int DNSServiceConstructFullName(char *, const char *, const char *, const char *);
void *pcap_open_dead(int, int);
void *pcap_dump_open(void *, const char *);
void pcap_dump(unsigned char *, const void *, const unsigned char *);
void pcap_dump_close(void *);
void pcap_close(void *);
const char *pcap_datalink_val_to_name(int);
void *CFBagCreateMutable(void *, long, const void *);
void CFBagAddValue(void *, const void *);
void CFBagGetValues(void *, const void **);
void class_setVersion(Class, int);
int class_getVersion(Class);
typedef struct { unsigned char len, family; unsigned short index; unsigned char type, nlen, alen, slen; char data[12]; } sdl_t;
char *link_ntoa(const sdl_t *);
@interface NSData (MOSCorpus)
+ (id)dataWithBytes:(const void *)b length:(unsigned long)n;
@end
@interface NEPacketTunnelProvider : NSObject
@end
@interface MOSTunnel : NEPacketTunnelProvider
@end
@implementation MOSTunnel
- (int)valor { return 42; }
@end
@interface NSArchiver : NSObject
+ (NSData *)archivedDataWithRootObject:(id)o;
@end
@interface NSUnarchiver : NSObject
+ (id)unarchiveObjectWithData:(NSData *)d;
@end

typedef struct { double x, y; } P2;
static P2 troca(P2 p, int k) { P2 r = {p.y * k, p.x * k}; return r; }
typedef struct { long a, b, c; } BIG;
static BIG soma(BIG v, long k) { BIG r = {v.a + k, v.b + k, v.c + k}; return r; }
static double mistura(unsigned char a, double b, float c, int d) { return a + b + c + d; }
static void cmp_cb(ffi_cif *cif, void *ret, void **args, void *ud) {
    (void)cif; (void)ud;
    int a = **(int **)args[0], b = **(int **)args[1];
    *(long *)ret = (a > b) - (a < b);
}

int main(void) {
    @autoreleasepool {
        wchar_t w[32];
        int sw = swprintf(w, 32, L"%d-%ls", 7, L"ok");
        if (!(sw == 4 && !wcscmp(w, L"7-ok"))) {
            printf("corpusteste:   swprintf=%d [", sw);
            for (int i = 0; i < 8 && w[i]; i++) printf("%x ", (unsigned)w[i]);
            char nb[32]; int nn = snprintf(nb, sizeof nb, "%d-%ls", 7, L"ok");
            printf("] snprintf %%ls=%d '%s'\n", nn, nb);
        }
        CK(sw == 4 && !wcscmp(w, L"7-ok"), "swprintf");
        /* libffi na ABI System V do x86_64 */
        ffi_cif cif;
        ffi_type *pel[3] = {&ffi_type_double, &ffi_type_double, 0};
        ffi_type pt = {0, 0, 13, pel};
        ffi_type *at[2] = {&pt, &ffi_type_sint32};
        P2 p = {1.5, 2.5}, rp = {0, 0};
        int k = 2;
        void *av[2] = {&p, &k};
        CK(ffi_prep_cif(&cif, 2, 2, &pt, at) == 0, "ffi_prep_cif");
        ffi_call(&cif, (void (*)(void))troca, &rp, av);
        CK(rp.x == 5 && rp.y == 3, "ffi_call struct {double,double} em xmm0/xmm1");
        ffi_type *bel[4] = {&ffi_type_sint64, &ffi_type_sint64, &ffi_type_sint64, 0};
        ffi_type bt = {0, 0, 13, bel};
        ffi_type *bat[2] = {&bt, &ffi_type_sint64};
        BIG vb = {1, 2, 3}, rb = {0, 0, 0};
        long kb = 10;
        void *bav[2] = {&vb, &kb};
        ffi_prep_cif(&cif, 2, 2, &bt, bat);
        ffi_call(&cif, (void (*)(void))soma, &rb, bav);
        CK(rb.a == 11 && rb.c == 13, "ffi_call struct de 24 bytes (memória e retorno indireto)");
        ffi_type *mat[4] = {&ffi_type_uint8, &ffi_type_double, &ffi_type_float, &ffi_type_sint32};
        unsigned char ua = 200; double db = 0.5; float fc = 0.25f; int di = -1;
        void *mav[4] = {&ua, &db, &fc, &di};
        double rd = 0;
        ffi_prep_cif(&cif, 2, 4, &ffi_type_double, mat);
        ffi_call(&cif, (void (*)(void))mistura, &rd, mav);
        CK(rd == 199.75, "ffi_call uint8/double/float/int");
        char buf[64]; char *pb = buf; long n = sizeof buf; const char *fmt = "%d:%.1f"; int vi = 9; double vd = 2.5;
        ffi_type *vat[5] = {&ffi_type_pointer, &ffi_type_sint64, &ffi_type_pointer, &ffi_type_sint32, &ffi_type_double};
        void *vav[5] = {&pb, &n, &fmt, &vi, &vd};
        long rv = 0;
        ffi_prep_cif_var(&cif, 2, 3, 5, &ffi_type_sint32, vat);
        ffi_call(&cif, (void (*)(void))snprintf, &rv, vav);
        CK(rv == 5 && !strcmp(buf, "9:2.5"), "ffi_call variádica (snprintf, al = nº de SSE)");
        void *code = 0, *cl = ffi_closure_alloc(64, &code);
        ffi_type *cat[2] = {&ffi_type_pointer, &ffi_type_pointer};
        ffi_cif ccif;
        ffi_prep_cif(&ccif, 2, 2, &ffi_type_sint32, cat);
        CK(cl && ffi_prep_closure_loc(cl, &ccif, cmp_cb, 0, code) == 0, "ffi closure");
        int arr[5] = {4, 2, 5, 1, 3};
        if (cl) qsort(arr, 5, sizeof(int), (int (*)(const void *, const void *))code);
        CK(arr[0] == 1 && arr[4] == 5, "closure da libffi como comparador do qsort");
        ffi_closure_free(cl);
        /* ndbm em arquivo */
        const char *tmp = getenv("TMPDIR") ? getenv("TMPDIR") : getenv("HOME") ? getenv("HOME") : "/";
        char base[512]; snprintf(base, sizeof base, "%s/corpusteste-dbm", tmp);
        void *d = dbm_open(base, O_RDWR | O_CREAT, 0644);
        datum key = {"chave", 5}, val = {"valor", 5};
        int st = d ? dbm_store(d, key, val, 1) : -9;
        if (st) printf("corpusteste:   dbm_open=%p store=%d errno=%d base=%s\n", d, st, errno, base);
        CK(d && st == 0, "dbm_store");
        if (d) dbm_close(d);
        d = dbm_open(base, O_RDONLY, 0);
        datum got = d ? dbm_fetch(d, key) : (datum){0, 0};
        CK(got.dsize == 5 && !memcmp(got.dptr, "valor", 5), "dbm reaberto");
        if (d) dbm_close(d);
        /* CGDisplayMode é objeto CF */
        const void *modes = CGDisplayCopyAllDisplayModes(CGMainDisplayID(), 0);
        const void *mode = modes && CFArrayGetCount(modes) > 0 ? CFArrayGetValueAtIndex(modes, 0) : 0;
        CK(mode && CFGetTypeID(mode) == CGDisplayModeGetTypeID() && CGDisplayModeGetWidth(mode) > 0, "CGDisplayMode de verdade");
        if (mode) { CFRetain(mode); CFRelease(mode); }
        if (modes) CFRelease(modes);
        /* classes */
        id arq = [NSArchiver archivedDataWithRootObject:@[@"a", @3]];
        id volta = arq ? [NSUnarchiver unarchiveObjectWithData:arq] : nil;
        if (![volta isEqual:(@[@"a", @3])])
        {
            printf("corpusteste:   arquivo=%s (%lu bytes) volta=%s n=%lu\n", arq ? "sim" : "nil", arq ? (unsigned long)[arq length] : 0UL,
                   volta ? object_getClassName(volta) : "nil", volta ? (unsigned long)[volta count] : 0UL);
            for (unsigned long i = 0; volta && i < [volta count]; i++)
                printf("corpusteste:     [%lu] %s %s\n", i, object_getClassName([volta objectAtIndex:i]), [[[volta objectAtIndex:i] description] UTF8String]);
        }
        CK([volta isEqual:(@[@"a", @3])], "NSArchiver/NSUnarchiver ida e volta");
        NSDictionary *d1 = @{@"k": @1, @"j": @"x"}, *d2 = @{@"j": @"x", @"k": @1}, *d3 = @{@"k": @2};
        CK([d1 isEqual:d2] && ![d1 isEqual:d3], "NSDictionary isEqual: por conteúdo");
        CK([[NSData dataWithBytes:"abc" length:3] isEqual:[NSData dataWithBytes:"abc" length:3]], "NSData isEqual: por conteúdo");
        NSArray *a1 = @[@1, @2], *a2 = @[@1, @2], *a3 = @[@2, @1];
        CK([a1 isEqual:a2] && ![a1 isEqual:a3], "NSArray isEqual: por conteúdo");
        id porta = [[NSClassFromString(@"NSPort") alloc] init];
        CK(porta && [porta isKindOfClass:NSClassFromString(@"NSPort")], "NSPort");
        CK([[NSClassFromString(@"NSMachPort") superclass] isEqual:NSClassFromString(@"NSPort")], "NSMachPort : NSPort");
        MOSTunnel *t = [[MOSTunnel alloc] init];
        CK(t && [t valor] == 42 && [t isKindOfClass:NSClassFromString(@"NEProvider")], "subclasse de NEPacketTunnelProvider");
        CK(NSClassFromString(@"ICDeviceBrowser") != nil && NSClassFromString(@"SKScene") != nil, "fachadas ImageCaptureCore e SpriteKit");
        /* curses sem terminal: janela em memória e o interpretador de parâmetros do terminfo */
        void *cw = newwin(3, 10, 0, 0);
        char cl2[16] = {0};
        if (cw) { waddstr(cw, "oi mOS"); wmove(cw, 0, 0); winnstr(cw, cl2, 6); }
        CK(cw && !strcmp(cl2, "oi mOS"), "curses: waddstr/winnstr");
        CK(!strcmp(tparm("\x1b[%i%p1%d;%p2%dH", 4, 9, 0, 0, 0, 0, 0, 0, 0), "\x1b[5;10H"), "curses: tparm do cup");
        CK(!strcmp(keyname(0403), "KEY_UP") && !strcmp(keyname(1), "^A"), "curses: keyname");
        /* readline, vis, simd */
        add_history("primeira");
        CK(history_length >= 1 && !strcmp(history_get(history_base + history_length - 1)->line, "primeira"), "readline: histórico");
        char v[64], u[64];
        strvis(v, "a\tb\n", 0x02 | 0x08 | 0x10);
        CK(strunvis(u, v) == 4 && !strcmp(u, "a\tb\n"), "strvis/strunvis");
        CK(matrix_identity_float4x4.c[2][2] == 1.0f && matrix_identity_float4x4.c[1][2] == 0.0f, "matrix_identity_float4x4 é dado");
        simd_double4x4 m = {{{2, 0, 0, 0}, {0, 4, 0, 0}, {0, 0, 8, 0}, {0, 0, 0, 1}}};
        simd_double4x4 inv = __invert_d4(m);
        CK(inv.c[0][0] == 0.5 && inv.c[1][1] == 0.25 && inv.c[2][2] == 0.125, "__invert_d4");
    }
    {   /* lacunas de 2 apps: bzip2 (compressor novo), CFTree, RFC 3394, DNS-SD, pcap, CFBag, class_setVersion */
        static char plain[200000], comp[220000], back[200000];
        for (int i = 0; i < (int)sizeof plain; i++) plain[i] = "mOS bzip2 "[i % 10] + (char)(i / 5000);
        unsigned cl = sizeof comp, bl = sizeof back;
        int rc = BZ2_bzBuffToBuffCompress(comp, &cl, plain, sizeof plain, 9, 0, 0);
        int rd = rc ? -99 : BZ2_bzBuffToBuffDecompress(back, &bl, comp, cl, 0, 0);
        printf("corpusteste:   bzip2 %u -> %u bytes (rc=%d rd=%d)\n", (unsigned)sizeof plain, cl, rc, rd);
        CK(rc == 0 && rd == 0 && bl == sizeof plain && !memcmp(back, plain, sizeof plain) && cl < 20000, "bzip2: comprime e volta");
        void *root = CFTreeCreate(0, &(CFTreeContext){0, (void *)0x55, 0, 0, 0});
        void *c1 = CFTreeCreate(0, &(CFTreeContext){0, (void *)0x66, 0, 0, 0}), *c2 = CFTreeCreate(0, &(CFTreeContext){0, 0, 0, 0, 0});
        CFTreeAppendChild(root, c1); CFTreeAppendChild(root, c2);
        CFRelease(c1); CFRelease(c2);
        void *kids[2] = {0, 0};
        CFTreeGetChildren(root, kids);
        CFTreeContext cx = {0};
        CFTreeGetContext(kids[0], &cx);
        CK(CFTreeGetChildCount(root) == 2 && kids[0] == c1 && kids[1] == c2 && cx.info == (void *)0x66 && CFTreeGetParent(c2) == root, "CFTree");
        CFRelease(root);
        static const unsigned char kek[16] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15};
        static const unsigned char key[16] = {0x00, 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88, 0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff};
        static const unsigned char want[24] = {0x1F, 0xA6, 0x8B, 0x0A, 0x81, 0x12, 0xB4, 0x47, 0xAE, 0xF3, 0x4B, 0xD8, 0xFB, 0x5A, 0x7B, 0x82,
                                               0x9D, 0x3E, 0x86, 0x23, 0x71, 0xD2, 0xCF, 0xE5};
        unsigned char wr[24], un[16];
        size_t wl = sizeof wr, ul = sizeof un;
        int w1 = CCSymmetricKeyWrap(1, CCrfc3394_iv, 8, kek, 16, key, 16, wr, &wl);
        int w2 = CCSymmetricKeyUnwrap(1, CCrfc3394_iv, 8, kek, 16, wr, 24, un, &ul);
        CK(w1 == 0 && wl == 24 && !memcmp(wr, want, 24) && w2 == 0 && !memcmp(un, key, 16), "CCSymmetricKeyWrap (vetor da RFC 3394)");
        char full[1024];
        CK(DNSServiceConstructFullName(full, "Sala 2.0", "_ipp._tcp", "local.") == 0 && !strcmp(full, "Sala\\0322\\.0._ipp._tcp.local."), "DNSServiceConstructFullName");
        void *pd = pcap_open_dead(1, 65535);
        char pp[256];
        snprintf(pp, sizeof pp, "%s/corpusteste.pcap", getenv("HOME") ? getenv("HOME") : "/tmp");
        void *dump = pd ? pcap_dump_open(pd, pp) : 0;
        struct { long sec, usec; unsigned caplen, len; } ph = {1, 2, 4, 4};
        if (dump) { pcap_dump(dump, &ph, (const unsigned char *)"abcd"); pcap_dump_close(dump); }
        FILE *pf = fopen(pp, "rb");
        unsigned char pb[64] = {0};
        size_t pn = pf ? fread(pb, 1, sizeof pb, pf) : 0;
        if (pf) fclose(pf);
        CK(pn == 24 + 16 + 4 && pb[0] == 0xd4 && pb[3] == 0xa1 && pb[20] == 1 && !memcmp(pb + 40, "abcd", 4) && !strcmp(pcap_datalink_val_to_name(1), "EN10MB"), "pcap: open_dead + dump");
        if (pd) pcap_close(pd);
        void *bag = CFBagCreateMutable(0, 0, 0);
        CFBagAddValue(bag, (void *)1); CFBagAddValue(bag, (void *)2); CFBagAddValue(bag, (void *)1);
        const void *bv[3] = {0, 0, 0};
        CFBagGetValues(bag, bv);
        int ones = (bv[0] == (void *)1) + (bv[1] == (void *)1) + (bv[2] == (void *)1);
        CK(ones == 2 && (bv[0] == (void *)2 || bv[1] == (void *)2 || bv[2] == (void *)2), "CFBagGetValues");
        class_setVersion([MOSTunnel class], 7);
        CK(class_getVersion([MOSTunnel class]) == 7 && class_getVersion([NSObject class]) == 0, "class_setVersion/getVersion");
        sdl_t sd = {20, 18, 1, 6, 3, 6, 0, {'e', 'n', '0', 0, 0x1c, 0x42, 0x0a, 0xb0, 0xff}};
        CK(!strcmp(link_ntoa(&sd), "en0:0.1c.42.a.b0.ff"), "link_ntoa");
        CK(NSClassFromString(@"Protocol") != nil, "classe Protocol");
    }
    printf("corpusteste: %d/%d ok\n", total - fails, total);
    return fails ? 1 : 0;
}
