/* mOS ColorSync — ColorSyncProfile e ColorSyncTransform (incluído por colorsync.c fora do teste de host).
 * Perfil = bytes ICC + perfil lido pelo motor; mutável guarda as tags e remonta o ICC no CopyData.
 * Transform = sequência de perfis (o primeiro é a fonte, o último o destino), cada um com a sua intenção. */
#include "objc.h"
#include <stdbool.h>

extern char cs_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } cs_cfconst_t;
#define CSK(var, text) \
    static cs_cfconst_t var##_box = {(void *)cs_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; \
    const void *var = &var##_box;

CSK(kColorSyncGenericGrayProfile, "com.apple.ColorSync.GenericGray")
CSK(kColorSyncGenericGrayGamma22Profile, "com.apple.ColorSync.GenericGrayGamma2.2")
CSK(kColorSyncGenericRGBProfile, "com.apple.ColorSync.GenericRGB")
CSK(kColorSyncGenericCMYKProfile, "com.apple.ColorSync.GenericCMYK")
CSK(kColorSyncDisplayP3Profile, "com.apple.ColorSync.DisplayP3")
CSK(kColorSyncSRGBProfile, "com.apple.ColorSync.sRGB")
CSK(kColorSyncAdobeRGB1998Profile, "com.apple.ColorSync.AdobeRGB1998")
CSK(kColorSyncGenericLabProfile, "com.apple.ColorSync.GenericLab")
CSK(kColorSyncGenericXYZProfile, "com.apple.ColorSync.GenericXYZ")
CSK(kColorSyncACESCGLinearProfile, "com.apple.ColorSync.ACESCGLinear")
CSK(kColorSyncDCIP3Profile, "com.apple.ColorSync.DCIP3")
CSK(kColorSyncITUR709Profile, "com.apple.ColorSync.ITUR709")
CSK(kColorSyncITUR2020Profile, "com.apple.ColorSync.ITUR2020")
CSK(kColorSyncITUR2100HLGProfile, "com.apple.ColorSync.ITUR2100HLG")
CSK(kColorSyncITUR2100PQProfile, "com.apple.ColorSync.ITUR2100PQ")
CSK(kColorSyncROMMRGBProfile, "com.apple.ColorSync.ROMMRGB")
CSK(kColorSyncProfile, "com.apple.ColorSync.Profile")
CSK(kColorSyncRenderingIntent, "com.apple.ColorSync.RenderingIntent")
CSK(kColorSyncRenderingIntentPerceptual, "Perceptual")
CSK(kColorSyncRenderingIntentRelative, "Relative")
CSK(kColorSyncRenderingIntentSaturation, "Saturation")
CSK(kColorSyncRenderingIntentAbsolute, "Absolute")
CSK(kColorSyncRenderingIntentUseProfileHeader, "UseProfileHeader")
CSK(kColorSyncTransformTag, "com.apple.ColorSync.TransformTag")
CSK(kColorSyncTransformDeviceToPCS, "com.apple.ColorSync.DeviceToPCS")
CSK(kColorSyncTransformPCSToPCS, "com.apple.ColorSync.PCSToPCS")
CSK(kColorSyncTransformPCSToDevice, "com.apple.ColorSync.PCSToDevice")
CSK(kColorSyncTransformDeviceToDevice, "com.apple.ColorSync.DeviceToDevice")
CSK(kColorSyncTransformGamutCheck, "com.apple.ColorSync.GamutCheck")
CSK(kColorSyncBlackPointCompensation, "com.apple.ColorSync.BlackPointCompensation")
CSK(kColorSyncConvertQuality, "com.apple.ColorSync.ConvertQuality")
CSK(kColorSyncBestQuality, "com.apple.ColorSync.BestQuality")
CSK(kColorSyncNormalQuality, "com.apple.ColorSync.NormalQuality")
CSK(kColorSyncDraftQuality, "com.apple.ColorSync.DraftQuality")
CSK(kColorSyncDisplayDeviceClass, "mntr")
CSK(kColorSyncCameraDeviceClass, "scnr")
CSK(kColorSyncScannerDeviceClass, "scnr")
CSK(kColorSyncPrinterDeviceClass, "prtr")
CSK(kColorSyncDeviceDefaultProfileID, "DefaultProfileID")
CSK(kColorSyncCustomProfiles, "CustomProfiles")
CSK(kColorSyncFactoryProfiles, "FactoryProfiles")
CSK(kColorSyncDeviceProfileURL, "DeviceProfileURL")
CSK(kColorSyncDeviceProfileID, "DeviceProfileID")
CSK(kColorSyncDeviceProfileIsFactory, "DeviceProfileIsFactory")
CSK(kColorSyncDeviceProfileIsDefault, "DeviceProfileIsDefault")
CSK(kColorSyncDeviceProfileIsCurrent, "DeviceProfileIsCurrent")
CSK(kColorSyncDeviceClass, "DeviceClass")
CSK(kColorSyncDeviceID, "DeviceID")
CSK(kColorSyncDeviceDescription, "DeviceDescription")
CSK(kColorSyncDeviceDescriptions, "DeviceDescriptions")
CSK(kColorSyncDeviceHostScope, "DeviceHostScope")
CSK(kColorSyncDeviceUserScope, "DeviceUserScope")
CSK(kColorSyncProfileHostScope, "ProfileHostScope")
CSK(kColorSyncProfileUserScope, "ProfileUserScope")
CSK(kColorSyncDeviceModeDescription, "DeviceModeDescription")
CSK(kColorSyncDeviceModeDescriptions, "DeviceModeDescriptions")
CSK(kColorSyncDeviceProfilesNotification, "com.apple.ColorSync.DeviceProfilesNotification")
CSK(kColorSyncDisplayDeviceProfilesNotification, "com.apple.ColorSync.DisplayProfileNotification")
CSK(kColorSyncDeviceRegisteredNotification, "com.apple.ColorSync.DeviceRegisteredNotification")
CSK(kColorSyncDeviceUnregisteredNotification, "com.apple.ColorSync.DeviceUnregisteredNotification")
CSK(kColorSyncTransformCodeFragmentType, "com.apple.ColorSync.CodeFragmentType")
CSK(kColorSyncTransformCodeFragmentMD5, "com.apple.ColorSync.CodeFragmentMD5")
CSK(kColorSyncSigDescription, "desc")

const char *cx_cstr(id o);
BOOL cx_is_str(id o);
void *CFStringCreateWithCString(void *alloc, const char *s, u32 enc);
void *CFDataCreate(void *alloc, const u8 *b, long n);
const u8 *CFDataGetBytePtr(void *d);
long CFDataGetLength(void *d);
void *CFArrayCreate(void *alloc, const void **vals, long n, const void *cb);
long CFArrayGetCount(void *a);
void *CFArrayGetValueAtIndex(void *a, long i);
void *CFDictionaryGetValue(void *d, const void *k);
void *CFDictionaryCreateMutable(void *alloc, long cap, const void *k, const void *v);
void CFDictionarySetValue(void *d, const void *k, const void *v);
void CFRelease(void *o);
void *CFRetain(void *o);
u8 CFURLGetFileSystemRepresentation(void *url, u8 resolve, u8 *buf, long max);
void cx_object_free(id o);
int open(const char *path, int flags, ...);
long read(int fd, void *b, unsigned long n);
int close(int fd);

enum { CS_TID_PROF = 0x2C0, CS_TID_XF = 0x2C1 };
typedef struct { u32 sig; u8 *b; u32 n; } cs_tag;
typedef struct {
    cx_class_t *isa; u32 magic;
    u8 *data; size_t n;          /* imutável: o arquivo; mutável: remontado em cs_rebuild */
    cs_prof prof; int ok;
    void *url;
    int mutable_;
    u8 header[128];
    cs_tag *tags; int ntags;
} csprof_t;
typedef struct {
    cx_class_t *isa; u32 magic;
    cs_prof src, dst;
    int ok;
} csxf_t;
#define CSP_MAGIC 0x43535046u
#define CSX_MAGIC 0x43535846u
static cx_class_t P_c, P_m, X_c, X_m;
static int cs_ready;
static void prof_dealloc(id self, SEL s) {
    (void)s;
    csprof_t *p = (csprof_t *)self;
    if (p->ok) cs_free(&p->prof);
    free(p->data);
    for (int i = 0; i < p->ntags; i++) free(p->tags[i].b);
    free(p->tags);
    if (p->url) CFRelease(p->url);
    p->magic = 0;
    cx_object_free(self);
}
static void xf_dealloc(id self, SEL s) {
    (void)s;
    csxf_t *x = (csxf_t *)self;
    cs_free(&x->src);
    cs_free(&x->dst);
    x->magic = 0;
    cx_object_free(self);
}
static u64 prof_tid(id self, SEL s) { (void)self; (void)s; return CS_TID_PROF; }
static u64 xf_tid(id self, SEL s) { (void)self; (void)s; return CS_TID_XF; }
static void cs_classes(void) {
    if (cs_ready) return;
    cx_class_t *ns = cx_class_named("NSObject");
    if (!ns) return;
    cx_define_class(&P_c, &P_m, "ColorSyncProfile", ns, (u32)(sizeof(csprof_t) - sizeof(void *)));
    cx_define_class(&X_c, &X_m, "ColorSyncTransform", ns, (u32)(sizeof(csxf_t) - sizeof(void *)));
    cx_add_method(&P_c, "dealloc", (void *)prof_dealloc);
    cx_add_method(&X_c, "dealloc", (void *)xf_dealloc);
    cx_add_method(&P_c, "_cfTypeID", (void *)prof_tid);
    cx_add_method(&X_c, "_cfTypeID", (void *)xf_tid);
    cs_ready = 1;
}
static csprof_t *as_prof(const void *p) { const csprof_t *x = p; return x && x->magic == CSP_MAGIC ? (csprof_t *)x : 0; }
u64 ColorSyncProfileGetTypeID(void) { return CS_TID_PROF; }
u64 ColorSyncTransformGetTypeID(void) { return CS_TID_XF; }

static csprof_t *prof_new(u8 *data, size_t n) {
    cs_classes();
    if (!cs_ready) { free(data); return 0; }
    csprof_t *p = (csprof_t *)cx_alloc_instance(&P_c);
    if (!p) { free(data); return 0; }
    p->magic = CSP_MAGIC;
    p->data = data;
    p->n = n;
    if (data) p->ok = cs_parse(data, n, &p->prof) == 0;
    return p;
}
/* bytes de um perfil ICC (cópia); erro = NULL */
void *ColorSyncProfileCreate(void *data, void **err) {
    if (err) *err = 0;
    const u8 *b = data ? CFDataGetBytePtr(data) : 0;
    long n = data ? CFDataGetLength(data) : 0;
    if (!b || n < 132) return 0;
    u8 *c = malloc((size_t)n);
    if (!c) return 0;
    memcpy(c, b, (size_t)n);
    csprof_t *p = prof_new(c, (size_t)n);
    if (p && !p->ok) { CFRelease(p); return 0; }
    return p;
}
/* nome → perfil embutido (gerado pelo motor) */
static const char *cs_name_for(const char *n) {
    static const struct { const char *cs, *eng; } map[] = {
        {"com.apple.ColorSync.GenericGray", "Gray"}, {"com.apple.ColorSync.GenericGrayGamma2.2", "Gray"},
        {"com.apple.ColorSync.GenericRGB", "sRGB"}, {"com.apple.ColorSync.GenericCMYK", "CMYK"},
        {"com.apple.ColorSync.DisplayP3", "P3"}, {"com.apple.ColorSync.sRGB", "sRGB"},
        {"com.apple.ColorSync.AdobeRGB1998", "AdobeRGB"}, {"com.apple.ColorSync.GenericLab", "Lab"},
        {"com.apple.ColorSync.GenericXYZ", "XYZ"}, {"com.apple.ColorSync.ACESCGLinear", "ACES"},
        {"com.apple.ColorSync.DCIP3", "DCIP3"}, {"com.apple.ColorSync.ITUR709", "709"},
        {"com.apple.ColorSync.ITUR2020", "2020"}, {"com.apple.ColorSync.ITUR2100HLG", "2100 HLG"},
        {"com.apple.ColorSync.ITUR2100PQ", "2100 PQ"}, {"com.apple.ColorSync.ROMMRGB", "ROMM"}};
    for (u32 i = 0; i < sizeof map / sizeof map[0]; i++) if (!strcmp(n, map[i].cs)) return map[i].eng;
    return n;                                  /* nomes de CGColorSpace (kCGColorSpace...) também servem */
}
void *ColorSyncProfileCreateWithName(const void *name) {
    const char *n = name && cx_is_str((id)name) ? cx_cstr((id)name) : 0;
    cs_prof b;
    if (!n || cs_builtin(cs_name_for(n), &b)) return 0;
    u8 *icc = 0;
    size_t len = 0;
    if (b.has_mtx && cs_write_icc(&b, &icc, &len) == 0) { cs_free(&b); return prof_new(icc, len); }
    /* Lab, XYZ e CMYK genéricos: perfil sem matriz, mantido como estrutura (CopyData monta só o cabeçalho) */
    csprof_t *p = prof_new(0, 0);
    if (!p) return 0;
    p->prof = b;
    p->ok = 1;
    return p;
}
void *ColorSyncProfileCreateWithURL(void *url, void **err) {
    char path[1024];
    if (err) *err = 0;
    if (!url || !CFURLGetFileSystemRepresentation(url, 1, (u8 *)path, (long)sizeof path)) return 0;
    int fd = open(path, 0);
    if (fd < 0) return 0;
    size_t cap = 65536, n = 0;
    u8 *b = malloc(cap);
    while (b) {
        if (n == cap) { u8 *nb = cap < (64u << 20) ? realloc(b, cap * 2) : 0; if (!nb) break; b = nb; cap *= 2; }
        long r = read(fd, b + n, cap - n);
        if (r <= 0) break;
        n += (size_t)r;
    }
    close(fd);
    csprof_t *p = b && n ? prof_new(b, n) : (free(b), (csprof_t *)0);
    if (p && !p->ok) { CFRelease(p); return 0; }
    if (p) p->url = CFRetain(url);
    return p;
}
/* perfil vazio para montar com SetTag/SetHeader */
void *ColorSyncProfileCreateMutable(void) {
    csprof_t *p = prof_new(0, 0);
    if (!p) return 0;
    p->mutable_ = 1;
    u8 *h = p->header;
    h[8] = 0x04; h[9] = 0x30;                                 /* versão 4.3 */
    memcpy(h + 36, "acsp", 4);
    static const u8 d50[12] = {0, 0, 0xF6, 0xD6, 0, 1, 0, 0, 0, 0, 0xD3, 0x2D};
    memcpy(h + 68, d50, 12);
    return p;
}
static u32 sig_of(const void *s) {
    const char *c = s && cx_is_str((id)s) ? cx_cstr((id)s) : 0;
    if (!c || strlen(c) < 4) return 0;
    return (u32)(u8)c[0] << 24 | (u32)(u8)c[1] << 16 | (u32)(u8)c[2] << 8 | (u8)c[3];
}
static void cs_rebuild(csprof_t *p) {
    size_t total = 132 + (size_t)p->ntags * 12;
    for (int i = 0; i < p->ntags; i++) total += (p->tags[i].n + 3) & ~3u;
    u8 *b = calloc(total, 1);
    if (!b) return;
    memcpy(b, p->header, 128);
    b[0] = (u8)(total >> 24); b[1] = (u8)(total >> 16); b[2] = (u8)(total >> 8); b[3] = (u8)total;
    b[128] = (u8)(p->ntags >> 24); b[129] = (u8)(p->ntags >> 16); b[130] = (u8)(p->ntags >> 8); b[131] = (u8)p->ntags;
    size_t off = 132 + (size_t)p->ntags * 12;
    for (int i = 0; i < p->ntags; i++) {
        u8 *e = b + 132 + i * 12;
        u32 v[3] = {p->tags[i].sig, (u32)off, p->tags[i].n};
        for (int k = 0; k < 3; k++) { e[k * 4] = (u8)(v[k] >> 24); e[k * 4 + 1] = (u8)(v[k] >> 16); e[k * 4 + 2] = (u8)(v[k] >> 8); e[k * 4 + 3] = (u8)v[k]; }
        memcpy(b + off, p->tags[i].b, p->tags[i].n);
        off += (p->tags[i].n + 3) & ~3u;
    }
    u8 id[16];
    cs_profile_id(b, total, id);
    memcpy(b + 84, id, 16);
    free(p->data);
    p->data = b;
    p->n = total;
    if (p->ok) cs_free(&p->prof);
    p->ok = cs_parse(b, total, &p->prof) == 0;
}
void ColorSyncProfileSetTag(void *prof, const void *signature, void *data) {
    csprof_t *p = as_prof(prof);
    u32 sig = sig_of(signature);
    if (!p || !p->mutable_ || !sig || !data) return;
    const u8 *d = CFDataGetBytePtr(data);
    long n = CFDataGetLength(data);
    if (!d || n <= 0) return;
    int i;
    for (i = 0; i < p->ntags && p->tags[i].sig != sig; i++) {}
    if (i == p->ntags) {
        cs_tag *nt = realloc(p->tags, (size_t)(p->ntags + 1) * sizeof *nt);
        if (!nt) return;
        p->tags = nt;
        p->ntags++;
        p->tags[i].b = 0;
    }
    free(p->tags[i].b);
    p->tags[i].sig = sig;
    p->tags[i].b = malloc((size_t)n);
    if (!p->tags[i].b) { p->tags[i].n = 0; return; }
    memcpy(p->tags[i].b, d, (size_t)n);
    p->tags[i].n = (u32)n;
    cs_rebuild(p);
}
void ColorSyncProfileRemoveTag(void *prof, const void *signature) {
    csprof_t *p = as_prof(prof);
    u32 sig = sig_of(signature);
    if (!p || !p->mutable_) return;
    for (int i = 0; i < p->ntags; i++)
        if (p->tags[i].sig == sig) {
            free(p->tags[i].b);
            p->tags[i] = p->tags[--p->ntags];
            cs_rebuild(p);
            return;
        }
}
void ColorSyncProfileSetHeader(void *prof, void *header) {
    csprof_t *p = as_prof(prof);
    if (!p || !p->mutable_ || !header || CFDataGetLength(header) < 128) return;
    memcpy(p->header, CFDataGetBytePtr(header), 128);
    cs_rebuild(p);
}
void *ColorSyncProfileCopyData(void *prof, void **err) {
    csprof_t *p = as_prof(prof);
    if (err) *err = 0;
    if (!p) return 0;
    if (p->mutable_ && !p->data) cs_rebuild(p);
    return p->data ? CFDataCreate(0, p->data, (long)p->n) : 0;
}
void *ColorSyncProfileCopyHeader(void *prof) {
    csprof_t *p = as_prof(prof);
    if (!p) return 0;
    if (p->data && p->n >= 128) return CFDataCreate(0, p->data, 128);
    return CFDataCreate(0, p->header, 128);
}
static const u8 *prof_tag(csprof_t *p, u32 sig, u32 *len) {
    if (!p || !p->data || p->n < 132) return 0;
    u32 cnt = be32c(p->data + 128);
    for (u32 i = 0; i < cnt && 132 + (size_t)i * 12 + 12 <= p->n; i++) {
        const u8 *t = p->data + 132 + i * 12;
        u32 off = be32c(t + 4), l = be32c(t + 8);
        if (be32c(t) == sig && off + (size_t)l <= p->n) { *len = l; return p->data + off; }
    }
    return 0;
}
bool ColorSyncProfileContainsTag(void *prof, const void *signature) { u32 l; return prof_tag(as_prof(prof), sig_of(signature), &l) != 0; }
void *ColorSyncProfileCopyTag(void *prof, const void *signature) {
    u32 l;
    const u8 *t = prof_tag(as_prof(prof), sig_of(signature), &l);
    return t ? CFDataCreate(0, t, (long)l) : 0;
}
void *ColorSyncProfileCopyDescriptionString(void *prof) {
    csprof_t *p = as_prof(prof);
    return p && p->ok && p->prof.desc[0] ? CFStringCreateWithCString(0, p->prof.desc, 0x08000100) : 0;
}
typedef struct { u8 digest[16]; } ColorSyncMD5;
ColorSyncMD5 ColorSyncProfileGetMD5(void *prof) {
    ColorSyncMD5 m;
    csprof_t *p = as_prof(prof);
    memset(&m, 0, sizeof m);
    if (p && p->data && p->n >= 100) cs_profile_id(p->data, p->n, m.digest);
    return m;
}
void *ColorSyncProfileGetURL(void *prof, void **err) { csprof_t *p = as_prof(prof); if (err) *err = 0; return p ? p->url : 0; }
/* verificação: o motor consegue ler e o tamanho do cabeçalho bate */
bool ColorSyncProfileVerify(void *prof, void **errors, void **warnings) {
    csprof_t *p = as_prof(prof);
    if (errors) *errors = 0;
    if (warnings) *warnings = 0;
    return p && p->ok && (!p->data || be32c(p->data) == p->n);
}
/* perfil de um dispositivo: a tela do mOS é sRGB; câmera/scanner/impressora sem perfil registrado também */
void *ColorSyncProfileCreateDeviceProfile(const void *deviceClass, void *deviceID, const void *profileID) {
    (void)deviceID; (void)profileID;
    const char *c = deviceClass && cx_is_str((id)deviceClass) ? cx_cstr((id)deviceClass) : "mntr";
    return ColorSyncProfileCreateWithName(!strcmp(c, "prtr") ? kColorSyncGenericCMYKProfile : kColorSyncSRGBProfile);
}

/* ---------------------------------------------------------------- transform */
static int intent_of(void *d, const csprof_t *p) {
    const void *v = d ? CFDictionaryGetValue(d, kColorSyncRenderingIntent) : 0;
    const char *s = v && cx_is_str((id)v) ? cx_cstr((id)v) : 0;
    if (s && !strcmp(s, "Relative")) return 1;
    if (s && !strcmp(s, "Saturation")) return 2;
    if (s && !strcmp(s, "Absolute")) return 3;
    if (s && !strcmp(s, "UseProfileHeader") && p && p->data && p->n >= 68) return (int)(be32c(p->data + 64) & 3);
    return 0;
}
static int prof_engine(const csprof_t *p, int intent, cs_prof *out) {
    if (p->data) return cs_parse_intent(p->data, p->n, intent, out);
    *out = p->prof;                    /* Lab/XYZ/CMYK genérico: estrutura sem tabelas alocadas, a cópia rasa basta */
    return 0;
}
void *ColorSyncTransformCreate(void *seq, void *options) {
    (void)options;
    long n = seq ? CFArrayGetCount(seq) : 0;
    if (n < 1) return 0;
    void *first = CFArrayGetValueAtIndex(seq, 0), *last = CFArrayGetValueAtIndex(seq, n - 1);
    csprof_t *ps = as_prof(CFDictionaryGetValue(first, kColorSyncProfile)), *pd = as_prof(CFDictionaryGetValue(last, kColorSyncProfile));
    if (!ps || !pd || !ps->ok || !pd->ok) return 0;
    cs_classes();
    csxf_t *x = (csxf_t *)cx_alloc_instance(&X_c);
    if (!x) return 0;
    x->magic = CSX_MAGIC;
    int ok = prof_engine(ps, intent_of(first, ps), &x->src) == 0;
    ok = ok && prof_engine(pd, intent_of(last, pd), &x->dst) == 0;
    if (!ok) { x->magic = 0; CFRelease(x); return 0; }
    x->ok = 1;
    return x;
}
/* um componente lido/gravado na profundidade pedida */
enum { CSD_8 = 2, CSD_16 = 3, CSD_16F = 4, CSD_32 = 5, CSD_32F = 7 };
static size_t depth_bytes(int d) { return d == CSD_8 ? 1 : d == CSD_16 || d == CSD_16F ? 2 : d == CSD_32 || d == CSD_32F ? 4 : 0; }
static double half_to_f(u16 h) {
    int s = h >> 15, e = (h >> 10) & 31, m = h & 1023;
    double v;
    if (e == 0) v = m / 1024.0 / 16384.0;
    else if (e == 31) v = 65504;
    else {
        v = 1 + m / 1024.0;
        for (int k = 15; k < e; k++) v *= 2;
        for (int k = e; k < 15; k++) v /= 2;
    }
    return s ? -v : v;
}
static u16 f_to_half(double v) {
    int s = v < 0;
    if (s) v = -v;
    if (v < 1.0 / 16384) return (u16)(s << 15 | (u16)(v * 16384 * 1024 + 0.5));
    int e = 15;
    while (v >= 2 && e < 30) { v /= 2; e++; }
    while (v < 1 && e > 1) { v *= 2; e--; }
    return (u16)(s << 15 | e << 10 | ((u16)((v - 1) * 1024 + 0.5) & 1023));
}
static double rd_comp(const u8 *p, int d, int le) {
    switch (d) {
    case CSD_8: return p[0] / 255.0;
    case CSD_16: return (le ? (p[0] | p[1] << 8) : (p[0] << 8 | p[1])) / 65535.0;
    case CSD_16F: return half_to_f((u16)(le ? (p[0] | p[1] << 8) : (p[0] << 8 | p[1])));
    case CSD_32: { u32 v = le ? (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24 : be32c(p); return v / 4294967295.0; }
    default: { u32 v = le ? (u32)p[0] | (u32)p[1] << 8 | (u32)p[2] << 16 | (u32)p[3] << 24 : be32c(p); float f; memcpy(&f, &v, 4); return f; }
    }
}
static void wr_comp(u8 *p, int d, int le, double v) {
    if (d != CSD_16F && d != CSD_32F) v = v < 0 ? 0 : v > 1 ? 1 : v;
    u32 x;
    switch (d) {
    case CSD_8: p[0] = (u8)(v * 255 + 0.5); return;
    case CSD_16: x = (u32)(v * 65535 + 0.5); break;
    case CSD_16F: x = f_to_half(v); break;
    case CSD_32: x = (u32)(v * 4294967295.0 + 0.5); break;
    default: { float f = (float)v; memcpy(&x, &f, 4); break; }
    }
    size_t nb = depth_bytes(d);
    for (size_t i = 0; i < nb; i++) p[le ? i : nb - 1 - i] = (u8)(x >> (8 * i));
}
bool ColorSyncTransformConvert(void *xf, size_t w, size_t h, void *dst, int dd, u32 dlay, size_t dbpr,
                               const void *src, int sd, u32 slay, size_t sbpr, void *options) {
    csxf_t *x = xf;
    (void)options;
    if (!x || x->magic != CSX_MAGIC || !dst || !src) return 0;
    size_t sb = depth_bytes(sd), db = depth_bytes(dd);
    if (!sb || !db) return 0;
    int sn = x->src.nchan, dn = x->dst.nchan;
    u32 sa = slay & 31, da = dlay & 31;
    int s_extra = sa != 0, d_extra = da != 0;
    int s_first = sa == 2 || sa == 4 || sa == 6, d_first = da == 2 || da == 4 || da == 6;
    int s_pm = sa == 1 || sa == 2, d_pm = da == 1 || da == 2;
    int s_has_a = sa >= 1 && sa <= 4, d_has_a = da >= 1 && da <= 4;
    int sle = (slay & 0x7000) == 0x1000 || (slay & 0x7000) == 0x2000, dle = (dlay & 0x7000) == 0x1000 || (dlay & 0x7000) == 0x2000;
    if (sd == CSD_32F || sd == CSD_16F) sle = (slay & 0x7000) != 0x3000 && (slay & 0x7000) != 0x4000;
    if (dd == CSD_32F || dd == CSD_16F) dle = (dlay & 0x7000) != 0x3000 && (dlay & 0x7000) != 0x4000;
    size_t spx = (size_t)(sn + s_extra) * sb, dpx = (size_t)(dn + d_extra) * db;
    for (size_t y = 0; y < h; y++) {
        const u8 *sr = (const u8 *)src + y * sbpr;
        u8 *dr = (u8 *)dst + y * dbpr;
        for (size_t i = 0; i < w; i++) {
            const u8 *sp = sr + i * spx;
            u8 *dp = dr + i * dpx;
            double in[16], out[16], xyz[3], a = 1;
            int base = s_extra && s_first ? 1 : 0;
            for (int k = 0; k < sn; k++) in[k] = rd_comp(sp + (size_t)(base + k) * sb, sd, sle);
            if (s_has_a) a = rd_comp(sp + (size_t)(s_first ? 0 : sn) * sb, sd, sle);
            if (s_pm && a > 0) for (int k = 0; k < sn; k++) in[k] /= a;
            if (x->src.lab) { in[0] *= 100; in[1] = in[1] * 255 - 128; in[2] = in[2] * 255 - 128; }
            cs_to_xyz(&x->src, in, xyz);
            cs_from_xyz(&x->dst, xyz, out);
            if (x->dst.lab) { out[0] /= 100; out[1] = (out[1] + 128) / 255; out[2] = (out[2] + 128) / 255; }
            int dbase = d_extra && d_first ? 1 : 0;
            for (int k = 0; k < dn; k++) wr_comp(dp + (size_t)(dbase + k) * db, dd, dle, d_pm ? out[k] * a : out[k]);
            if (d_extra) wr_comp(dp + (size_t)(d_first ? 0 : dn) * db, dd, dle, d_has_a ? a : 1);
        }
    }
    return 1;
}
/* fragmento de código: a descrição de cada etapa da transformação (tipo e MD5 do perfil) */
void *ColorSyncCreateCodeFragment(void *seq, void *options) {
    (void)options;
    long n = seq ? CFArrayGetCount(seq) : 0;
    const void *v[16];
    long m = 0;
    for (long i = 0; i < n && m < 16; i++) {
        csprof_t *p = as_prof(CFDictionaryGetValue(CFArrayGetValueAtIndex(seq, i), kColorSyncProfile));
        if (!p || !p->ok) continue;
        void *d = CFDictionaryCreateMutable(0, 0, 0, 0);
        const char *type = p->prof.has_mtx ? (p->prof.nchan == 1 ? "GrayTRC" : "MatrixTRC") : p->prof.has_a2b ? "LUT" : p->prof.lab ? "Lab" : "Naive";
        void *ts = CFStringCreateWithCString(0, type, 0x08000100);
        CFDictionarySetValue(d, kColorSyncTransformCodeFragmentType, ts);
        CFRelease(ts);
        if (p->data && p->n >= 100) {
            u8 id[16];
            cs_profile_id(p->data, p->n, id);
            void *md = CFDataCreate(0, id, 16);
            CFDictionarySetValue(d, kColorSyncTransformCodeFragmentMD5, md);
            CFRelease(md);
        }
        v[m++] = d;
    }
    void *a = CFArrayCreate(0, v, m, 0);
    for (long i = 0; i < m; i++) CFRelease((void *)v[i]);
    return a;
}
/* para o CoreGraphics: o perfil lido de um ColorSyncProfileRef (CGColorSpaceCreateWithPlatformColorSpace) */
const u8 *cx_colorsync_profile_bytes(const void *prof, size_t *n) {
    csprof_t *p = as_prof(prof);
    if (!p) return 0;
    if (p->mutable_ && !p->data) cs_rebuild(p);
    *n = p->n;
    return p->data;
}

/* ---- dispositivos: o monitor do mOS, com o perfil sRGB (o compositor mostra sRGB) gravado uma vez em
 * ~/Library/ColorSync/Profiles para quem lê o perfil pelo arquivo (Blender, Krita) */
void *CFDictionaryCreateMutable(void *alloc, long cap, const void *kcb, const void *vcb);
void CFDictionarySetValue(void *d, const void *k, const void *v);
void *CFURLCreateFromFileSystemRepresentation(void *alloc, const u8 *buf, long len, u8 isdir);
extern const void *kCFBooleanTrue, *kCFBooleanFalse;
char *getenv(const char *name);
int snprintf(char *b, size_t n, const char *f, ...);
int mkdir(const char *p, int m);
ssize_t write(int fd, const void *b, size_t n);
static void *cs_display_url(void) {
    static char path[512];
    if (!path[0]) {
        const char *home = getenv("HOME");
        if (!home || !home[0]) home = "/tmp";
        char dir[400];
        snprintf(dir, sizeof dir, "%s/Library", home); mkdir(dir, 0755);
        snprintf(dir, sizeof dir, "%s/Library/ColorSync", home); mkdir(dir, 0755);
        snprintf(dir, sizeof dir, "%s/Library/ColorSync/Profiles", home); mkdir(dir, 0755);
        snprintf(path, sizeof path, "%s/mOS Display sRGB.icc", dir);
        int fd = open(path, 0);
        if (fd >= 0) close(fd);
        else {
            cs_prof b;
            u8 *icc = 0;
            size_t len = 0;
            if (!cs_builtin("sRGB", &b) && cs_write_icc(&b, &icc, &len) == 0) {
                int w = open(path, 577, 0644);          /* O_WRONLY | O_CREAT | O_TRUNC */
                if (w >= 0) { (void)write(w, icc, len); close(w); }
                free(icc);
                cs_free(&b);
            }
        }
    }
    return CFURLCreateFromFileSystemRepresentation(0, (const u8 *)path, (long)strlen(path), 0);
}
static void *cs_display_id(void) { return CFStringCreateWithCString(0, "6D4F5300-0000-4000-8000-000000000001", 0x08000100); }
void *ColorSyncDeviceCopyDeviceInfo(const void *deviceClass, const void *deviceID) {
    (void)deviceID;
    const char *c = deviceClass && cx_is_str((id)deviceClass) ? cx_cstr((id)deviceClass) : "";
    if (strcmp(c, "mntr")) return 0;                     /* só há o monitor */
    void *url = cs_display_url(), *pid = CFStringCreateWithCString(0, "1", 0x08000100), *dev = cs_display_id();
    void *prof = CFDictionaryCreateMutable(0, 0, 0, 0), *fact = CFDictionaryCreateMutable(0, 0, 0, 0), *info = CFDictionaryCreateMutable(0, 0, 0, 0);
    CFDictionarySetValue(prof, kColorSyncDeviceProfileURL, url);
    CFDictionarySetValue(prof, kColorSyncDeviceModeDescription, CFStringCreateWithCString(0, "mOS Display", 0x08000100));
    CFDictionarySetValue(fact, pid, prof);
    CFDictionarySetValue(fact, kColorSyncDeviceDefaultProfileID, pid);
    CFDictionarySetValue(info, kColorSyncDeviceClass, deviceClass);
    CFDictionarySetValue(info, kColorSyncDeviceID, dev);
    CFDictionarySetValue(info, kColorSyncDeviceDescription, CFStringCreateWithCString(0, "mOS Display", 0x08000100));
    CFDictionarySetValue(info, kColorSyncFactoryProfiles, fact);
    CFDictionarySetValue(info, kColorSyncCustomProfiles, CFDictionaryCreateMutable(0, 0, 0, 0));
    CFDictionarySetValue(info, kColorSyncDeviceUserScope, CFStringCreateWithCString(0, "kCFPreferencesAnyUser", 0x08000100));
    CFDictionarySetValue(info, kColorSyncDeviceHostScope, CFStringCreateWithCString(0, "kCFPreferencesCurrentHost", 0x08000100));
    return info;
}
typedef u8 (*cs_iter_fn)(void *profileInfo, void *userInfo);
void ColorSyncIterateDeviceProfiles(cs_iter_fn fn, void *userInfo) {
    if (!fn) return;
    void *d = CFDictionaryCreateMutable(0, 0, 0, 0);
    CFDictionarySetValue(d, kColorSyncDeviceClass, kColorSyncDisplayDeviceClass);
    CFDictionarySetValue(d, kColorSyncDeviceID, cs_display_id());
    CFDictionarySetValue(d, kColorSyncDeviceDescription, CFStringCreateWithCString(0, "mOS Display", 0x08000100));
    CFDictionarySetValue(d, kColorSyncDeviceProfileID, CFStringCreateWithCString(0, "1", 0x08000100));
    CFDictionarySetValue(d, kColorSyncDeviceProfileURL, cs_display_url());
    CFDictionarySetValue(d, kColorSyncDeviceProfileIsFactory, kCFBooleanTrue);
    CFDictionarySetValue(d, kColorSyncDeviceProfileIsDefault, kCFBooleanTrue);
    CFDictionarySetValue(d, kColorSyncDeviceProfileIsCurrent, kCFBooleanTrue);
    (void)fn(d, userInfo);
}
