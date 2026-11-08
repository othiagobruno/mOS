/*
 * mOS — libSystem: o que só o Google Chrome pede (docs/porte-chrome.md).
 *
 * Medido com tools/machocheck.c: das funções que o Chrome importa e o mOS não tinha, estas 200 e poucas não
 * aparecem no VS Code/Electron (o resto é coberto pelo porte do VS Code, em mach.c, darwin.c, nsurl.c...).
 *
 * Regra: quando o mOS não tem o recurso (Keychain, DiskArbitration, MIDI, Bonjour, assinatura de código...), a
 * função devolve o ERRO que a API do macOS documenta para "indisponível" — nunca 0, porque em OSStatus/kern_return_t
 * 0 é sucesso e o Chrome seguiria lendo parâmetros de saída que ninguém preencheu. Constantes CFString têm os valores
 * reais da Apple quando conhecidos (são chaves de dicionário: o texto importa só para comparação).
 */
typedef unsigned long size_t;
typedef long ssize_t;
typedef unsigned char u8;
typedef unsigned long long u64;
typedef void *id_;
typedef int OSStatus;
typedef int kern_return_t;
typedef unsigned char Boolean;
typedef struct { long loc, len; } CFRange;

int *__error(void);
#define SET_ERRNO(e) (*__error() = (e))
#define EPERM_ 1
#define ENOENT_ 2
#define ENOSYS_ 78
#define ENOTSUP_ 45
#define KERN_SUCCESS 0
#define KERN_FAILURE 5

size_t strlen(const char *s);
int strncmp(const char *a, const char *b, size_t n);
int strcmp(const char *a, const char *b);
char *strchr(const char *s, int c);
int mkdir(const char *p, unsigned short mode);
double log1p(double x);

/* ---- ponte com o Foundation do mOS (CFStringRef = NSString*, CFDictionaryRef = NSDictionary*...) */
typedef struct cx_class cx_class_t;
cx_class_t *cx_class_named(const char *name);
id_ objc_msgSend(id_ self, void *op, ...);
void *sel_registerName(const char *name);
id_ objc_retain(id_ o);
#define MSG0(T, o, s) ((T (*)(id_, void *))objc_msgSend)((id_)(o), sel_registerName(s))
#define MSG1(T, o, s, a) ((T (*)(id_, void *, __typeof__(a)))objc_msgSend)((id_)(o), sel_registerName(s), (a))
#define CLS(n) ((id_)cx_class_named(n))

static const char *utf8(id_ s) { return s ? MSG0(const char *, s, "UTF8String") : 0; }
static id_ nsstr(const char *s) { return s ? MSG1(id_, CLS("NSString"), "stringWithUTF8String:", s) : 0; }

/* ---- constantes CFString no formato de @"..." (o mesmo de darwin.c) */
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } cfconst_t;
#define K(var, text) \
    static cfconst_t var##_cs = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; \
    const void *var = &var##_cs;

/* Security */
K(kSecAttrAccessibleAfterFirstUnlock, "ck") K(kSecAttrType, "type")
K(kSecCFErrorResourceAltered, "SecCSResourceAltered") K(kSecCFErrorResourceMissing, "SecCSResourceMissing")
K(kSecCodeInfoCertificates, "certificates") K(kSecCodeInfoMainExecutable, "main-executable")
K(kSecCodeInfoPList, "info-plist") K(kSecCodeInfoUnique, "unique")
K(kSecCodeSignerEntitlements, "entitlements") K(kSecCodeSignerFlags, "flags") K(kSecCodeSignerIdentity, "signer")
/* DiskArbitration */
K(kDADiskDescriptionDeviceModelKey, "DADeviceModel") K(kDADiskDescriptionDeviceRevisionKey, "DADeviceRevision")
K(kDADiskDescriptionDeviceVendorKey, "DADeviceVendor") K(kDADiskDescriptionMediaBSDNameKey, "DAMediaBSDName")
K(kDADiskDescriptionMediaRemovableKey, "DAMediaRemovable") K(kDADiskDescriptionMediaSizeKey, "DAMediaSize")
K(kDADiskDescriptionVolumeNameKey, "DAVolumeName") K(kDADiskDescriptionVolumePathKey, "DAVolumePath")
K(kDADiskDescriptionVolumeUUIDKey, "DAVolumeUUID")
/* kDADiskDescriptionMatchVolumeMountable e WatchVolumePath são CFDictionary/CFArray: ficam nil (o loader dá 0) */
/* CoreServices / LaunchServices / Spotlight */
K(_kLSOpenOptionBackgroundLaunchKey, "LSBackgroundLaunch")
K(kLSQuarantineDataURLKey, "LSQuarantineDataURL") K(kLSQuarantineOriginURLKey, "LSQuarantineOriginURL")
K(kLSQuarantineTypeKey, "LSQuarantineType") K(kLSQuarantineTypeOtherDownload, "LSQuarantineTypeOtherDownload")
K(kLSQuarantineTypeWebDownload, "LSQuarantineTypeWebDownload") K(kMDItemWhereFroms, "kMDItemWhereFroms")
/* OpenDirectory */
K(kODAttributeTypeAllAttributes, "dsAttributesAll") K(kODAttributeTypeAltSecurityIdentities, "dsAttrTypeStandard:AltSecurityIdentities")
K(kODAttributeTypeMetaRecordName, "dsAttrTypeNative:RecordName") K(kODAttributeTypeRecordName, "dsAttrTypeStandard:RecordName")
K(kODAttributeTypeStandardOnly, "dsAttributesStandardAll") K(kODAttributeTypeUniqueID, "dsAttrTypeStandard:UniqueID")
K(kODRecordTypeGroups, "dsRecTypeStandard:Groups") K(kODRecordTypeUsers, "dsRecTypeStandard:Users")
/* CoreVideo / CoreMedia / VideoToolbox */
K(kCVImageBufferCleanApertureKey, "CVCleanAperture") K(kCVImageBufferCleanApertureHeightKey, "Height")
K(kCVImageBufferCleanApertureWidthKey, "Width") K(kCVImageBufferCleanApertureHorizontalOffsetKey, "HorizontalOffset")
K(kCVImageBufferCleanApertureVerticalOffsetKey, "VerticalOffset")
K(kCVPixelFormatComponentRange, "ComponentRange") K(kCVPixelFormatComponentRange_FullRange, "FullRange")
K(kCMFormatDescriptionExtension_ContentLightLevelInfo, "ContentLightLevelInfo")
K(kCMFormatDescriptionExtension_MasteringDisplayColorVolume, "MasteringDisplayColorVolume")
K(kVTCompressionPropertyKey_ContentLightLevelInfo, "ContentLightLevelInfo")
K(kVTCompressionPropertyKey_HDRMetadataInsertionMode, "HDRMetadataInsertionMode")
K(kVTCompressionPropertyKey_MasteringDisplayColorVolume, "MasteringDisplayColorVolume")
K(kVTCompressionPropertyKey_OutputBitDepth, "OutputBitDepth")
K(kVTCompressionPropertyKey_PixelTransferProperties, "PixelTransferProperties")
K(kVTCompressionPropertyKey_ReferenceBufferCount, "ReferenceBufferCount")
K(kVTHDRMetadataInsertionMode_Auto, "Auto") K(kVTPixelTransferPropertyKey_ScalingMode, "ScalingMode")
K(kVTProfileLevel_HEVC_Main10_AutoLevel, "HEVC_Main10_AutoLevel")
K(kVTProfileLevel_HEVC_Main42210_AutoLevel, "HEVC_Main42210_AutoLevel")
K(kVTScalingMode_CropSourceToCleanAperture, "CropSourceToCleanAperture")
/* CFNetwork / CoreGraphics / outros */
K(kCFProxyTypeAutoConfigurationJavaScript, "kCFProxyTypeAutoConfigurationJavaScript")
K(kCFProxyTypeAutoConfigurationURL, "kCFProxyTypeAutoConfigurationURL")
K(kCGColorClear, "kCGColorClear")
K(UNErrorDomain, "UNErrorDomain") K(ASWebAuthenticationSessionErrorDomain, "com.apple.AuthenticationServices.WebAuthenticationSession")
/* Foundation / AppKit (NSString constantes) */
K(NSDebugDescriptionErrorKey, "NSDebugDescription") K(NSFilePosixPermissions, "NSFilePosixPermissions")
K(NSProgressFileOperationKindDownloading, "NSProgressFileOperationKindDownloading")
K(NSProgressKindFile, "NSProgressKindFile") K(NSUserActivityTypeBrowsingWeb, "NSUserActivityTypeBrowsingWeb")
K(NSURLEffectiveIconKey, "NSURLEffectiveIconKey") K(NSURLQuarantinePropertiesKey, "NSURLQuarantinePropertiesKey")
K(NSApplicationLaunchIsDefaultLaunchKey, "NSApplicationLaunchIsDefaultLaunchKey")
K(NSCalibratedRGBColorSpace, "NSCalibratedRGBColorSpace") K(NSShadowAttributeName, "NSShadow")
K(NSImageNameApplicationIcon, "NSApplicationIcon") K(NSImageNameFolder, "NSFolder")
K(NSPasteboardNameGeneral, "Apple CFPasteboard general")
K(NSSharingServiceNameAddToSafariReadingList, "com.apple.share.System.add-to-safari-reading-list")
K(NSSharingServiceNameComposeEmail, "com.apple.share.Mail.compose")
K(NSAccessibilityChildrenInNavigationOrderAttribute, "AXChildrenInNavigationOrder")
K(NSAccessibilityIdentifierAttribute, "AXIdentifier")
K(NSAccessibilitySearchCurrentElementKey, "AXStartElement") K(NSAccessibilitySearchDirectionKey, "AXDirection")
K(NSAccessibilitySearchDirectionNext, "AXDirectionNext") K(NSAccessibilitySearchDirectionPrevious, "AXDirectionPrevious")
K(NSAccessibilitySearchIdentifiersKey, "AXSearchKey") K(NSAccessibilitySearchResultsLimitKey, "AXResultsLimit")
K(NSAccessibilitySearchTextKey, "AXSearchText") K(NSAccessibilityTabButtonSubrole, "AXTabButton")
K(NSAccessibilityZoomButtonAttribute, "AXZoomButton")
const float NSURLSessionTaskPriorityHigh = 0.75f;

/* ---- libc */
typedef int wchar32;   /* wchar_t do macOS: 32 bits */
wchar32 *wcschr(const wchar32 *s, wchar32 c) { for (;; s++) { if (*s == c) return (wchar32 *)s; if (!*s) return 0; } }
wchar32 *wcscpy(wchar32 *d, const wchar32 *s) { wchar32 *r = d; while ((*d++ = *s++)) {} return r; }
wchar32 *wcspbrk(const wchar32 *s, const wchar32 *set) {
    for (; *s; s++) for (const wchar32 *k = set; *k; k++) if (*s == *k) return (wchar32 *)s;
    return 0;
}
float nanf(const char *tag) { (void)tag; return __builtin_nanf(""); }
float log1pf(float x) { return (float)log1p(x); }

/* getopt_long: o suficiente para "--nome", "--nome=valor", "--nome valor" e "-x" */
struct option { const char *name; int has_arg; int *flag; int val; };
char *optarg;
int optind = 1, opterr = 1, optopt;
static int optpos = 1;
int getopt_long(int argc, char *const argv[], const char *shortopts, const struct option *longopts, int *index) {
    optarg = 0;
    if (optind >= argc || !argv[optind] || argv[optind][0] != '-' || !argv[optind][1]) return -1;
    const char *a = argv[optind];
    if (!strcmp(a, "--")) { optind++; return -1; }
    if (a[1] == '-') {
        const char *n = a + 2, *eq = strchr(n, '=');
        size_t nl = eq ? (size_t)(eq - n) : strlen(n);
        optind++;
        for (int i = 0; longopts && longopts[i].name; i++) {
            if (strncmp(longopts[i].name, n, nl) || longopts[i].name[nl]) continue;
            if (index) *index = i;
            if (longopts[i].has_arg == 1 || (longopts[i].has_arg == 2 && eq)) {
                if (eq) optarg = (char *)eq + 1;
                else if (optind < argc) optarg = argv[optind++];
                else return ':';
            }
            if (longopts[i].flag) { *longopts[i].flag = longopts[i].val; return 0; }
            return longopts[i].val;
        }
        return '?';
    }
    int c = (unsigned char)a[optpos];
    const char *o = shortopts ? strchr(shortopts, c) : 0;
    if (!o || c == ':') {
        optopt = c;
        if (!a[++optpos]) { optind++; optpos = 1; }
        return '?';
    }
    if (o[1] == ':') {
        if (a[optpos + 1]) optarg = (char *)a + optpos + 1;
        else if (optind + 1 < argc) optarg = argv[++optind];
        else { optind++; optpos = 1; return ':'; }
        optind++;
        optpos = 1;
    } else if (!a[++optpos]) { optind++; optpos = 1; }
    return c;
}

typedef struct cx_file FILE;
ssize_t getline(char **line, size_t *cap, FILE *f);
char *fgetln(FILE *f, size_t *len) {          /* devolve a linha (sem garantir \0) até a próxima chamada */
    static char *buf;
    static size_t cap;
    ssize_t n = getline(&buf, &cap, f);
    if (n <= 0) { if (len) *len = 0; return 0; }
    if (len) *len = (size_t)n;
    return buf;
}

/* grupos: só "staff" (20), como o usuário do mOS (getgrgid_r em foundationsys.c) */
int getgrgid_r(unsigned gid, void *g, char *buf, size_t len, void **res);
int getgrnam_r(const char *name, void *g, char *buf, size_t len, void **res);
static struct { char *n, *p; unsigned gid; char **mem; } cx_grp; static char cx_grpbuf[64];
void *getgrgid(unsigned g) { void *r = 0; getgrgid_r(g, &cx_grp, cx_grpbuf, sizeof cx_grpbuf, &r); return r; }
void *getgrnam(const char *n) { void *r = 0; getgrnam_r(n, &cx_grp, cx_grpbuf, sizeof cx_grpbuf, &r); return r; }
int mkdirat(int fd, const char *p, unsigned short mode) {
    const char *cx_at_path(int dirfd, const char *p, char *buf, size_t cap);   /* posixmore.c: dirfd -> caminho */
    char t[1024];
    const char *q = cx_at_path(fd, p, t, sizeof t);   /* o GNU tar cria as pastas com mkdirat(dirfd, ...) */
    return q ? mkdir(q, mode) : -1;
}
int utime(const char *p, const void *times) { (void)p; (void)times; return 0; }   /* o VFS não guarda datas por arquivo */
int shm_open(const char *n, int fl, ...) { (void)n; (void)fl; SET_ERRNO(ENOSYS_); return -1; }
int shm_unlink(const char *n) { (void)n; SET_ERRNO(ENOENT_); return -1; }
int cx_getattrlist(const char *p, void *al, void *buf, size_t sz, unsigned opts);   /* posixmore.c: capacidades do volume */
int getattrlist(const char *p, void *al, void *buf, size_t sz, unsigned opts) { return cx_getattrlist(p, al, buf, sz, opts); }
char *_dirhelper(int which, char *buf, size_t len) { (void)which; (void)buf; (void)len; return 0; }
int proc_get_wakemon_params(int pid, int *rate, int *flags) { (void)pid; (void)rate; (void)flags; SET_ERRNO(ENOTSUP_); return -1; }

/* O Chromium suspende o filho só para registrar as portas do rendezvous e depois retoma.
 * O mOS não congela a tarefa (o filho já corre); responder falha fazia o pai desistir dele. */
kern_return_t task_suspend(unsigned t) { (void)t; return KERN_SUCCESS; }
kern_return_t task_resume(unsigned t) { (void)t; return KERN_SUCCESS; }
kern_return_t task_create_identity_token(unsigned t, unsigned *tok) { (void)t; if (tok) *tok = 0; return KERN_FAILURE; }
kern_return_t task_identity_token_get_task_port(unsigned tok, int flavor, unsigned *port) { (void)tok; (void)flavor; if (port) *port = 0; return KERN_FAILURE; }
/* mach_port_insert_member agora é implementado de verdade em mach.c (portas Mach no kernel, conjuntos
 * de portas); o stub que ficava aqui foi removido para não duplicar o símbolo. */

/* ---- launchd (API antiga launch_msg): não há launchd */
void *launch_data_alloc(int type) { (void)type; return 0; }
void launch_data_free(void *d) { (void)d; }
void *launch_data_new_string(const char *s) { (void)s; return 0; }
Boolean launch_data_dict_insert(void *d, void *v, const char *k) { (void)d; (void)v; (void)k; return 0; }
void *launch_data_dict_lookup(void *d, const char *k) { (void)d; (void)k; return 0; }
int launch_data_get_type(void *d) { (void)d; return 0; }
int launch_data_get_errno(void *d) { (void)d; return ENOSYS_; }
long long launch_data_get_integer(void *d) { (void)d; return 0; }
void *launch_msg(void *req) { (void)req; SET_ERRNO(ENOSYS_); return 0; }

/* ---- libbz2: o Chrome só descomprime (relatórios/pacotes); sem a biblioteca, avisa "erro de configuração" */
int BZ2_bzDecompressInit(void *s, int verbosity, int small) { (void)s; (void)verbosity; (void)small; return -9; }   /* BZ_CONFIG_ERROR */
int BZ2_bzDecompress(void *s) { (void)s; return -2; }                                                          /* BZ_PARAM_ERROR */
int BZ2_bzDecompressEnd(void *s) { (void)s; return 0; }

/* ---- Objective-C */
/* Classes privadas do AppKit que o Chrome procura por nome e cujos métodos ele troca (swizzle). Sem elas, o
 * Chrome falha num CHECK logo no início: o bloco do dispatch_once de +[ChromeSwizzleServicesMenuUpdater install]
 * (chrome_browser_application_mac.mm) exige que _NSServiceEntry responda a -bundleIdentifier e que
 * _NSServicesMenuUpdater responda a -populateMenu:withServiceEntries:forDisplay: (e, depois, -plugMenu:intoMenu:).
 * O mOS não tem menu Serviços: as entradas não têm app e os métodos não fazem nada. */
void *objc_allocateClassPair(void *super, const char *name, unsigned long extra);
void objc_registerClassPair(void *c);
signed char class_addMethod(void *c, void *sel, void *imp, const char *types);
static id_ svc_bundle_id(id_ self, void *cmd) { (void)self; (void)cmd; return 0; }
static void svc_populate(id_ self, void *cmd, id_ menu, id_ entries, id_ display) { (void)self; (void)cmd; (void)menu; (void)entries; (void)display; }
static void svc_plug(id_ self, void *cmd, id_ menu, id_ into) { (void)self; (void)cmd; (void)menu; (void)into; }
static void *private_class(const char *name) {
    void *c = objc_allocateClassPair(cx_class_named("NSObject"), name, 0);
    if (c) objc_registerClassPair(c);
    return c;
}
void cx_chrome_init(void) {         /* chamado por cx_objc_boot (objc.c) */
    void *e = private_class("_NSServiceEntry");
    if (e) class_addMethod(e, sel_registerName("bundleIdentifier"), (void *)svc_bundle_id, "@16@0:8");
    void *u = private_class("_NSServicesMenuUpdater");
    if (u) {
        class_addMethod(u, sel_registerName("populateMenu:withServiceEntries:forDisplay:"), (void *)svc_populate, "v40@0:8@16@24@32");
        class_addMethod(u, sel_registerName("plugMenu:intoMenu:"), (void *)svc_plug, "v32@0:8@16@24");
    }
}

typedef id_ (*objc_exc_pre_t)(id_);
static objc_exc_pre_t exc_pre;
objc_exc_pre_t objc_setExceptionPreprocessor(objc_exc_pre_t fn) { objc_exc_pre_t o = exc_pre; exc_pre = fn; return o; }
objc_exc_pre_t cx_objc_exception_preprocessor(void) { return exc_pre; }   /* objc_exc.c: aplicado no objc_exception_throw */

/* ---- CoreFoundation */
Boolean CFStringHasPrefix(id_ s, id_ p) {
    const char *a = utf8(s), *b = utf8(p);
    return a && b && !strncmp(a, b, strlen(b));
}
id_ CFStringCreateCopy(void *alloc, id_ s) { (void)alloc; id_ c = s ? MSG0(id_, s, "copy") : 0; return c; }
id_ CFStringCreateMutableCopy(void *alloc, long max, id_ s) { (void)alloc; (void)max; return s ? MSG0(id_, s, "mutableCopy") : 0; }
void CFStringAppend(id_ m, id_ s) { if (m && s) (void)MSG1(id_, m, "appendString:", s); }
/* índices em unidades UTF-16, como o CFStringGetLength (darwin.c) e o CFStringGetCharacters (vscode_cf.c) */
void *malloc(unsigned long n);
void free(void *p);
long CFStringGetLength(id_ s);
void CFStringGetCharacters(id_ s, CFRange r, unsigned short *out);
id_ CFStringCreateWithCharacters(void *alloc, const unsigned short *chars, long n);
id_ CFStringCreateWithSubstring(void *alloc, id_ s, CFRange r) {
    (void)alloc;
    if (!s || r.loc < 0 || r.len < 0 || r.loc + r.len > CFStringGetLength(s)) return 0;
    unsigned short tmp[512], *w = r.len <= 512 ? tmp : malloc((size_t)r.len * 2);
    if (!w) return 0;
    CFStringGetCharacters(s, r, w);
    id_ o = CFStringCreateWithCharacters(0, w, r.len);   /* +1, como todo Create */
    if (w != tmp) free(w);
    return o;
}
CFRange CFStringGetRangeOfCharacterClusterAtIndex(id_ s, long i, int type) {
    (void)type;
    const char *u = utf8(s);
    CFRange r = {i, 1};
    if (!u) return r;
    while (r.loc > 0 && ((u8)u[r.loc] & 0xC0) == 0x80) r.loc--;        /* começo do caractere UTF-8 */
    r.len = 1;
    while (u[r.loc + r.len] && ((u8)u[r.loc + r.len] & 0xC0) == 0x80) r.len++;
    return r;
}
id_ CFStringCreateArrayWithFindResults(void *alloc, id_ s, id_ find, CFRange r, unsigned opts) { (void)alloc; (void)s; (void)find; (void)r; (void)opts; return 0; }   /* NULL = nenhuma ocorrência */

/* (CFDictionaryApplyFunction e as outras coleções ficam em cfcollections.c) */
/* (os *GetTypeID e CFCopyTypeIDDescription precisam bater com o CFGetTypeID de cfcollections.c: ficam lá) */
id_ CFUUIDCreateString(void *alloc, id_ uuid) { (void)alloc; (void)uuid; return 0; }
Boolean CFPreferencesAppValueIsForced(id_ key, id_ app) { (void)key; (void)app; return 0; }     /* sem políticas gerenciadas */
long CFPreferencesGetAppIntegerValue(id_ key, id_ app, Boolean *valid) { (void)key; (void)app; if (valid) *valid = 0; return 0; }
void CFPreferencesSetAppValue(id_ key, id_ value, id_ app) { (void)key; (void)value; (void)app; }
id_ CFPropertyListCreateWithData(void *alloc, id_ data, unsigned long opt, long *fmt, id_ *err) { (void)alloc; (void)data; (void)opt; (void)fmt; if (err) *err = 0; return 0; }
id_ _CFURLCopyPropertyListRepresentation(id_ url) { (void)url; return 0; }
id_ _CFURLCreateFromPropertyListRepresentation(void *alloc, id_ pl) { (void)alloc; (void)pl; return 0; }

/* ---- Foundation: geometria */
double floor(double), ceil(double);
typedef struct { double x, y; } nspoint_t;
typedef struct { double x, y, w, h; } nsrect_t;
Boolean NSEqualPoints(nspoint_t a, nspoint_t b) { return a.x == b.x && a.y == b.y; }
nsrect_t NSIntegralRect(nsrect_t r) {          /* menor retângulo de coordenadas inteiras que contém r */
    if (r.w <= 0 || r.h <= 0) return (nsrect_t){0, 0, 0, 0};
    double x0 = floor(r.x), y0 = floor(r.y), x1 = ceil(r.x + r.w), y1 = ceil(r.y + r.h);
    return (nsrect_t){x0, y0, x1 - x0, y1 - y0};
}

/* ---- CoreGraphics / CoreText / Carbon */
id_ CGEventCreateData(void *alloc, id_ ev) { (void)alloc; (void)ev; return 0; }
Boolean CGEventSourceKeyState(int state, unsigned short key) { (void)state; (void)key; return 0; }
id_ CTFontCopyVariationAxesInternal(id_ font) { (void)font; return 0; }
OSStatus CallNextEventHandler(void *next, void *ev) { (void)next; (void)ev; return -9874; }   /* eventNotHandledErr */
unsigned GetEventKind(void *ev) { (void)ev; return 0; }

/* ---- AppKit: "kill ring" do Emacs (Ctrl+K / Ctrl+Y nos campos de texto) — sem efeito */
void _NSInitializeKillRing(void) {}
void _NSAppendToKillRing(id_ s) { (void)s; }
void _NSNewKillRingSequence(void) {}
void _NSSetKillRingToYankedState(void) {}
id_ _NSYankFromKillRing(void) { return 0; }

/* (CoreVideo fica em corevideo.c, da sessão CoreAnimation/IOSurface) */

/* ---- Security: o resto (Keychain de senhas, SecItem*, SecCode*, SecTrust, SSL*) fica em security.c */
#define errSecUnimplemented (-4)
#define errSecNotAvailable (-25291)
#define errAuthorizationDenied (-60005)
/* há um Keychain só (security.c), sempre destrancado: o objeto é um marcador; as funções aceitam kc = NULL */
OSStatus SecKeychainCopyDomainDefault(int domain, void **kc);
OSStatus SecKeychainCopyDefault(void **kc) { return SecKeychainCopyDomainDefault(0, kc); }
OSStatus SecKeychainGetStatus(void *kc, unsigned *st) { (void)kc; if (st) *st = 7; return 0; }   /* unlocked|readable|writable */
OSStatus SecKeychainGetUserInteractionAllowed(Boolean *on) { if (on) *on = 0; return 0; }
OSStatus SecKeychainSetUserInteractionAllowed(Boolean on) { (void)on; return 0; }
OSStatus SecKeychainItemCreateFromContent(unsigned cls, void *attrs, unsigned len, const void *data, void *kc, void *access, void **item) {
    (void)cls; (void)attrs; (void)len; (void)data; (void)kc; (void)access;
    if (item) *item = 0;
    return errSecNotAvailable;
}
OSStatus SecKeychainItemCopyKeychain(void *item, void **kc) { (void)item; if (kc) *kc = 0; return errSecNotAvailable; }
OSStatus SecAccessCreate(id_ desc, id_ trusted, void **acc) { (void)desc; (void)trusted; if (acc) *acc = 0; return errSecUnimplemented; }
OSStatus SecAccessCopyACLList(void *acc, id_ *list) { (void)acc; if (list) *list = 0; return errSecUnimplemented; }
OSStatus SecACLCopyContents(void *acl, id_ *apps, id_ *desc, void *prompt) { (void)acl; (void)prompt; if (apps) *apps = 0; if (desc) *desc = 0; return errSecUnimplemented; }
OSStatus SecACLSetContents(void *acl, id_ apps, id_ desc, unsigned short prompt) { (void)acl; (void)apps; (void)desc; (void)prompt; return errSecUnimplemented; }
OSStatus SecCodeSignerCreate(id_ params, unsigned flags, void **signer) { (void)params; (void)flags; if (signer) *signer = 0; return errSecUnimplemented; }
OSStatus SecCodeSignerAddSignatureWithErrors(void *signer, void *code, unsigned flags, id_ *err) { (void)signer; (void)code; (void)flags; if (err) *err = 0; return errSecUnimplemented; }
OSStatus SecRequirementCopyString(void *req, unsigned flags, id_ *text) { (void)req; (void)flags; if (text) *text = 0; return errSecUnimplemented; }
OSStatus AuthorizationExecuteWithPrivileges(void *a, const char *tool, unsigned opts, char *const *args, void **pipe) {
    (void)a; (void)tool; (void)opts; (void)args;
    if (pipe) *pipe = 0;
    return errAuthorizationDenied;
}
OSStatus AuthorizationRightGet(const char *right, id_ *def) { (void)right; if (def) *def = 0; return errAuthorizationDenied; }
OSStatus AuthorizationRightSet(void *a, const char *right, id_ rules, id_ desc, void *bundle, id_ table) {
    (void)a; (void)right; (void)rules; (void)desc; (void)bundle; (void)table;
    return errAuthorizationDenied;
}
Boolean SMJobBless(id_ domain, id_ label, void *auth, id_ *err) { (void)domain; (void)label; (void)auth; if (err) *err = 0; return 0; }

/* ---- DiskArbitration: sem sessão (o Chrome trata NULL como "não disponível") */
void *DASessionCreate(void *alloc) { (void)alloc; return 0; }
void DASessionScheduleWithRunLoop(void *s, void *rl, id_ mode) { (void)s; (void)rl; (void)mode; }
void DASessionUnscheduleFromRunLoop(void *s, void *rl, id_ mode) { (void)s; (void)rl; (void)mode; }
void *DADiskCreateFromBSDName(void *alloc, void *s, const char *name) { (void)alloc; (void)s; (void)name; return 0; }
id_ DADiskCopyDescription(void *d) { (void)d; return 0; }
unsigned DADiskCopyIOMedia(void *d) { (void)d; return 0; }
void *DADiskCopyWholeDisk(void *d) { (void)d; return 0; }
void DADiskClaim(void *d, unsigned o, void *rel, void *relctx, void *cb, void *ctx) { (void)d; (void)o; (void)rel; (void)relctx; (void)cb; (void)ctx; }
void DADiskUnclaim(void *d) { (void)d; }
void DADiskEject(void *d, unsigned o, void *cb, void *ctx) { (void)d; (void)o; (void)cb; (void)ctx; }
void DADiskUnmount(void *d, unsigned o, void *cb, void *ctx) { (void)d; (void)o; (void)cb; (void)ctx; }
void *DADissenterCreate(void *alloc, int status, id_ str) { (void)alloc; (void)status; (void)str; return 0; }
int DADissenterGetStatus(void *d) { (void)d; return 0; }
id_ DADissenterGetStatusString(void *d) { (void)d; return 0; }
void DARegisterDiskAppearedCallback(void *s, id_ match, void *cb, void *ctx) { (void)s; (void)match; (void)cb; (void)ctx; }
void DARegisterDiskDisappearedCallback(void *s, id_ match, void *cb, void *ctx) { (void)s; (void)match; (void)cb; (void)ctx; }
void DARegisterDiskDescriptionChangedCallback(void *s, id_ match, id_ watch, void *cb, void *ctx) { (void)s; (void)match; (void)watch; (void)cb; (void)ctx; }

/* ---- IOKit: conexões com drivers (IOUserClient) não existem */
#define kIOReturnUnsupported ((kern_return_t)0xE00002C7)
kern_return_t IOServiceOpen(unsigned svc, unsigned task, unsigned type, unsigned *conn) { (void)svc; (void)task; (void)type; if (conn) *conn = 0; return kIOReturnUnsupported; }
kern_return_t IOConnectCallMethod(unsigned c, unsigned sel, const u64 *in, unsigned nin, const void *sin, size_t sinl, u64 *out, unsigned *nout, void *sout, size_t *soutl) {
    (void)c; (void)sel; (void)in; (void)nin; (void)sin; (void)sinl; (void)out; (void)sout;
    if (nout) *nout = 0;
    if (soutl) *soutl = 0;
    return kIOReturnUnsupported;
}
kern_return_t IOConnectCallStructMethod(unsigned c, unsigned sel, const void *in, size_t inl, void *out, size_t *outl) {
    (void)c; (void)sel; (void)in; (void)inl; (void)out;
    if (outl) *outl = 0;
    return kIOReturnUnsupported;
}

/* ---- SystemConfiguration: interfaces de rede (lista vazia em vez de NULL: o Chrome itera) */
id_ SCNetworkInterfaceCopyAll(void) { id_ a = MSG0(id_, CLS("NSArray"), "array"); return a ? objc_retain(a) : 0; }
id_ SCNetworkInterfaceGetBSDName(void *i) { (void)i; return 0; }
id_ SCNetworkInterfaceGetHardwareAddressString(void *i) { (void)i; return 0; }

/* ---- LaunchServices / Spotlight */
/* LSRegisterURL: openurl.c */
void *MDItemCreate(void *alloc, id_ path) { (void)alloc; (void)path; return 0; }
Boolean MDItemSetAttribute(void *item, id_ name, id_ value) { (void)item; (void)name; (void)value; return 0; }

/* ---- Network.framework: Bonjour e parâmetros TLS (nenhum objeto é criado; os setters ignoram NULL) */
void *nw_browse_descriptor_create_bonjour_service(const char *type, const char *domain) { (void)type; (void)domain; return 0; }
void *nw_browser_create(void *desc, void *params) { (void)desc; (void)params; return 0; }
void nw_browser_set_queue(void *b, void *q) { (void)b; (void)q; }
void nw_browser_set_state_changed_handler(void *b, void *h) { (void)b; (void)h; }
void nw_browser_set_browse_results_changed_handler(void *b, void *h) { (void)b; (void)h; }
void nw_browser_start(void *b) { (void)b; }
void nw_browser_cancel(void *b) { (void)b; }
void *nw_browse_result_copy_endpoint(void *r) { (void)r; return 0; }
u64 nw_browse_result_get_changes(void *a, void *b) { (void)a; (void)b; return 0; }
const char *nw_endpoint_get_bonjour_service_name(void *e) { (void)e; return 0; }
int nw_error_get_error_code(void *e) { (void)e; return 0; }
int nw_interface_get_type(void *i) { (void)i; return 0; }                 /* nw_interface_type_other */
void nw_path_enumerate_interfaces(void *p, void *block) { (void)p; (void)block; }
void *nw_parameters_create_secure_tcp(void *tls, void *tcp) { (void)tls; (void)tcp; return 0; }
void _nw_parameters_configure_protocol_default_configuration(void *p) { (void)p; }
void _nw_parameters_configure_protocol_disable(void *p) { (void)p; }

/* ---- MediaAccessibility: legendas com a aparência padrão */
/* No macOS as cores nunca são NULL: o Chrome (CaptionStyle) converte cada uma para sRGB e exige 4 componentes
 * (CHECK). Texto branco sobre fundo preto, janela transparente, como o padrão do sistema. */
extern void *CGColorCreateSRGB(double r, double g, double b, double a);
id_ MACaptionAppearanceCopyForegroundColor(int domain, int *behavior) { (void)domain; if (behavior) *behavior = 0; return (id_)CGColorCreateSRGB(1, 1, 1, 1); }
id_ MACaptionAppearanceCopyBackgroundColor(int domain, int *behavior) { (void)domain; if (behavior) *behavior = 0; return (id_)CGColorCreateSRGB(0, 0, 0, 1); }
id_ MACaptionAppearanceCopyWindowColor(int domain, int *behavior) { (void)domain; if (behavior) *behavior = 0; return (id_)CGColorCreateSRGB(0, 0, 0, 1); }
id_ MACaptionAppearanceCopyFontDescriptorForStyle(int domain, int *behavior, int style) { (void)domain; (void)style; if (behavior) *behavior = 0; return 0; }
double MACaptionAppearanceGetForegroundOpacity(int domain, int *behavior) { (void)domain; if (behavior) *behavior = 0; return 1.0; }
double MACaptionAppearanceGetBackgroundOpacity(int domain, int *behavior) { (void)domain; if (behavior) *behavior = 0; return 0.8; }
double MACaptionAppearanceGetWindowOpacity(int domain, int *behavior) { (void)domain; if (behavior) *behavior = 0; return 0.0; }
double MACaptionAppearanceGetWindowRoundedCornerRadius(int domain, int *behavior) { (void)domain; if (behavior) *behavior = 0; return 0.0; }
double MACaptionAppearanceGetRelativeCharacterSize(int domain, int *behavior) { (void)domain; if (behavior) *behavior = 0; return 1.0; }
int MACaptionAppearanceGetTextEdgeStyle(int domain, int *behavior) { (void)domain; if (behavior) *behavior = 0; return 1; }   /* None */

/* ---- CoreMIDI (MIDI 2.0) e AudioToolbox (arquivos de áudio): sem dispositivos */
#define kMIDINotPermitted (-10844)
OSStatus MIDIInputPortCreateWithProtocol(unsigned client, id_ name, int proto, unsigned *port, void *block) { (void)client; (void)name; (void)proto; (void)block; if (port) *port = 0; return kMIDINotPermitted; }
void *MIDIEventListInit(void *list, int proto) { (void)list; (void)proto; return 0; }
void *MIDIEventListAdd(void *list, size_t size, void *cur, u64 time, size_t words, const unsigned *data) { (void)list; (void)size; (void)cur; (void)time; (void)words; (void)data; return 0; }
OSStatus MIDISendEventList(unsigned port, unsigned dest, const void *list) { (void)port; (void)dest; (void)list; return kMIDINotPermitted; }
#define kAudioFileUnspecifiedError 0x7768743F   /* 'wht?' */
/* AudioFileSetProperty/WritePackets/Close e o resto do AudioFile: coreaudio.c */
OSStatus AudioFileInitializeWithCallbacks(void *ctx, void *rd, void *wr, void *gs, void *ss, unsigned type, const void *fmt, unsigned flags, void **id) {
    (void)ctx; (void)rd; (void)wr; (void)gs; (void)ss; (void)type; (void)fmt; (void)flags;
    if (id) *id = 0;
    return kAudioFileUnspecifiedError;
}
