/*
 * mOS libSystem — CoreServices antigo (CarbonCore e LaunchServices em C) que os apps do corpus ainda ligam:
 * kUTType* / kUTTag*, UTGetOSTypeFromString & cia., UTTypeCopyAllTagsWithClass/CopyDeclaration, UCCollator e
 * LocaleRef, Text Encoding Conversion (TextToUnicodeInfo, UnicodeToTextInfo, TEC*), Memory Manager (Handle/Ptr),
 * Gestalt, CSCopyMachineName/UserName, relógios (TickCount, Microseconds, UpTime), erros do OSStatus e IconRef.
 * O núcleo sem CF/ObjC está em cs_legacy_core.inc (teste de host: tools/cs_legacy_host.c).
 */
#include "foundation.h"
#include "libc.h"
#include "fdn_encoding_core.h"
#include "fdn_icu.h"

typedef int OSStatus;
typedef short OSErr;
typedef unsigned OSType;
typedef unsigned long ItemCount;
typedef unsigned long ByteCount;
typedef char **Handle;
typedef char *Ptr;
id objc_retain(id o);
void objc_release(id o);
id CFArrayCreateMutable(void *alloc, long cap, const void *cb);
void CFArrayAppendValue(id a, id v);
id CFDictionaryCreateMutable(void *alloc, long cap, const void *kcb, const void *vcb);
void CFDictionarySetValue(id d, id k, id v);
void CFRelease(id o);
extern const char kCFTypeArrayCallBacks[], kCFTypeDictionaryKeyCallBacks[], kCFTypeDictionaryValueCallBacks[];
int cx_ls_tag_for_type(const char *uti, const char *cls_, char *out, size_t cap);
int cx_ls_type_info(const char *uti, char *desc, size_t dcap, char *parents, size_t pcap);
int cx_ls_type_for_tag(const char *cls_, const char *tag, const char *conf, char *out, size_t cap);
int sysctlbyname(const char *name, void *old, size_t *oldlen, const void *nw, size_t nwlen);
unsigned long long mach_absolute_time(void);
int gethostname(char *b, size_t n);
char *getenv(const char *n);
long write(int fd, const void *b, size_t n);

#include "cs_legacy_core.inc"

/* ================================================================ constantes (CFString com o valor do macOS) */
extern cx_class_t NSConstantString_c __asm__("__CFConstantStringClassReference");
#define CK(sym, val) static nsstr_t sym##_o = { &NSConstantString_c, 0, (char *)val, sizeof(val) - 1 }; id sym = (id)&sym##_o;
CK(kUTTypeICO, "com.microsoft.ico") CK(kUTTypeRTFD, "com.apple.rtfd") CK(kUTTypeFlatRTFD, "com.apple.flat-rtfd")
CK(kUTImportedTypeDeclarationsKey, "UTImportedTypeDeclarations") CK(kUTExportedTypeDeclarationsKey, "UTExportedTypeDeclarations")
CK(kUTTypeIconFileKey, "UTTypeIconFile")
CK(kUTTagClassNSPboardType, "com.apple.nspboard-type") CK(kUTTagClassOSType, "com.apple.ostype")
CK(kUTTypeItem, "public.item") CK(kUTTypeContent, "public.content") CK(kUTTypeCompositeContent, "public.composite-content")
CK(kUTTypeMessage, "public.message") CK(kUTTypeContact, "public.contact") CK(kUTTypeArchive, "public.archive")
CK(kUTTypeDiskImage, "public.disk-image") CK(kUTTypeDirectory, "public.directory") CK(kUTTypePackage, "com.apple.package")
CK(kUTTypeText, "public.text") CK(kUTTypeUTF8PlainText, "public.utf8-plain-text")
CK(kUTTypeUTF16ExternalPlainText, "public.utf16-external-plain-text") CK(kUTTypeUTF16PlainText, "public.utf16-plain-text")
CK(kUTTypeDelimitedText, "public.delimited-values-text") CK(kUTTypeRTF, "public.rtf") CK(kUTTypeHTML, "public.html")
CK(kUTTypeXMLPropertyList, "com.apple.xml-property-list") CK(kUTTypeBinaryPropertyList, "com.apple.binary-property-list")
CK(kUTTypePropertyList, "com.apple.property-list") CK(kUTTypeJSON, "public.json") CK(kUTTypeSourceCode, "public.source-code")
CK(kUTTypeInkText, "com.apple.ink.inktext") CK(kUTTypeWebArchive, "com.apple.webarchive")
CK(kUTTypeURL, "public.url") CK(kUTTypeFileURL, "public.file-url") CK(kUTTypeInternetLocation, "com.apple.internet-location")
CK(kUTTypeVCard, "public.vcard") CK(kUTTypeExecutable, "public.executable")
CK(kUTTypeImage, "public.image") CK(kUTTypeTIFF, "public.tiff") CK(kUTTypePICT, "com.apple.pict") CK(kUTTypeBMP, "com.microsoft.bmp")
CK(kUTTypeJPEG2000, "public.jpeg-2000") CK(kUTTypeRawImage, "public.camera-raw-image")
CK(kUTTypeScalableVectorGraphics, "public.svg-image") CK(kUTTypeAppleICNS, "com.apple.icns") CK(kUTTypePDF, "com.adobe.pdf")
CK(kUTTypeAudiovisualContent, "public.audiovisual-content") CK(kUTTypeMovie, "public.movie") CK(kUTTypeVideo, "public.video")
CK(kUTTypeAudio, "public.audio") CK(kUTTypeQuickTimeMovie, "com.apple.quicktime-movie")
CK(kUTTypeAppleScript, "com.apple.applescript.text") CK(kUTTypeOSAScript, "com.apple.applescript.script")
CK(kUTTypeOSAScriptBundle, "com.apple.applescript.script-bundle") CK(kUTTypeJavaArchive, "com.sun.java-archive")
CK(kUTTypeZipArchive, "public.zip-archive") CK(kUTTypeGNUZipArchive, "org.gnu.gnu-zip-archive")
CK(kUTTypeBzip2Archive, "public.bzip2-archive") CK(kUTTypeElectronicPublication, "org.idpf.epub-container")
/* chaves do dicionário de UTTypeCopyDeclaration */
CK(kUTTypeIdentifierKey, "UTTypeIdentifier") CK(kUTTypeDescriptionKey, "UTTypeDescription")
CK(kUTTypeConformsToKey, "UTTypeConformsTo") CK(kUTTypeTagSpecificationKey, "UTTypeTagSpecification")
/* atributos de item do LaunchServices antigo e listas do LSSharedFileList */
CK(kLSItemDisplayName, "LSItemDisplayName") CK(kLSItemQuarantineProperties, "LSItemQuarantineProperties")
CK(kLSSharedFileListRecentDocumentItems, "com.apple.LSSharedFileList.RecentDocuments")
CK(kLSSharedFileListFavoriteVolumes, "com.apple.LSSharedFileList.FavoriteVolumes")
CK(kLSSharedFileListRecentItemsMaxAmount, "com.apple.LSSharedFileList.RecentItemsMaxAmount")

/* ================================================================ UT* com OSType e tags */
OSType UTGetOSTypeFromString(id s) {
    const char *c = s && cx_is_str(s) ? cx_cstr(s) : 0;
    if (!c || strlen(c) != 4) return 0;
    return (OSType)(u8)c[0] << 24 | (OSType)(u8)c[1] << 16 | (OSType)(u8)c[2] << 8 | (u8)c[3];
}
id UTCreateStringForOSType(OSType t) {
    char b[5] = {(char)(t >> 24), (char)(t >> 16), (char)(t >> 8), (char)t, 0};
    return objc_retain(cx_str(b));
}
static const char *uti_c(id u) { return u && cx_is_str(u) ? cx_cstr(u) : 0; }
id UTTypeCopyAllTagsWithClass(id uti, id cls_) {
    const char *u = uti_c(uti), *c = uti_c(cls_);
    if (!u || !c) return 0;
    char all[1024];
    extern int cx_ls_tags_all(const char *uti, const char *cls_, char *out, size_t cap);
    if (!cx_ls_tags_all(u, c, all, sizeof all) || !all[0]) return 0;
    id arr = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
    for (char *p = all; *p;) {
        char *e = strchr(p, '\n');
        if (e) *e = 0;
        if (*p) CFArrayAppendValue(arr, cx_str(p));
        if (!e) break;
        p = e + 1;
    }
    return arr;
}
/* {UTTypeIdentifier, UTTypeDescription, UTTypeConformsTo[], UTTypeTagSpecification{classe: [tags]}} */
id UTTypeCopyDeclaration(id uti) {
    const char *u = uti_c(uti);
    char desc[256], parents[1024];
    if (!u || !cx_ls_type_info(u, desc, sizeof desc, parents, sizeof parents)) return 0;
    id d = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    CFDictionarySetValue(d, kUTTypeIdentifierKey, cx_str(u));
    if (desc[0]) CFDictionarySetValue(d, kUTTypeDescriptionKey, cx_str(desc));
    id conf = CFArrayCreateMutable(0, 0, &kCFTypeArrayCallBacks);
    for (char *p = parents; *p;) {
        char *e = p;
        while (*e && *e != ' ' && *e != ',' && *e != '\n') e++;
        char save = *e;
        *e = 0;
        if (*p) CFArrayAppendValue(conf, cx_str(p));
        if (!save) break;
        p = e + 1;
    }
    CFDictionarySetValue(d, kUTTypeConformsToKey, conf);
    CFRelease(conf);
    id spec = CFDictionaryCreateMutable(0, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
    static const char *classes[] = {"public.filename-extension", "public.mime-type", "com.apple.ostype", "com.apple.nspboard-type"};
    for (int i = 0; i < 4; i++) {
        id tags = UTTypeCopyAllTagsWithClass(uti, cx_str(classes[i]));
        if (tags) { CFDictionarySetValue(spec, cx_str(classes[i]), tags); CFRelease(tags); }
    }
    CFDictionarySetValue(d, kUTTypeTagSpecificationKey, spec);
    CFRelease(spec);
    return d;
}

/* ================================================================ UCCollator e LocaleRef */
/* LocaleRef: ponteiro para o identificador ICU ("pt_BR"), guardado numa tabela que nunca se libera (como os
 * LocaleRef do Carbon, que valem enquanto o processo vive) */
static char g_locales[32][32];
static int g_nloc;
OSStatus LocaleRefFromLocaleString(const char *s, void **ref) {
    if (!s || !ref) return cs_paramErr;
    for (int i = 0; i < g_nloc; i++) if (!strcmp(g_locales[i], s)) { *ref = g_locales[i]; return 0; }
    if (g_nloc >= 32 || strlen(s) >= 32) return cs_paramErr;
    strcpy(g_locales[g_nloc], s);
    for (char *p = g_locales[g_nloc]; *p; p++) if (*p == '-') *p = '_';
    *ref = g_locales[g_nloc++];
    return 0;
}
/* LangCode/RegionCode do Script Manager para os idiomas mais comuns (Script.h); desconhecido = paramErr */
OSStatus LocaleStringToLangAndRegionCodes(const char *s, short *lang, short *region) {
    static const struct { const char *code; short lang, region; } t[] = {
        {"en", 0, 0}, {"en_US", 0, 0}, {"fr", 1, 1}, {"en_GB", 0, 2}, {"de", 2, 3}, {"it", 3, 4}, {"nl", 4, 5},
        {"sv", 5, 7}, {"es", 6, 8}, {"da", 7, 9}, {"pt", 8, 10}, {"pt_PT", 8, 10}, {"no", 9, 12}, {"nb", 9, 12},
        {"ja", 11, 14}, {"zh_TW", 19, 53}, {"zh_CN", 33, 52}, {"ko", 23, 51}, {"ru", 32, 49}, {"pt_BR", 8, 71},
    };
    if (!s) return cs_paramErr;
    for (unsigned i = 0; i < sizeof t / sizeof t[0]; i++)
        if (!strcmp(t[i].code, s)) { if (lang) *lang = t[i].lang; if (region) *region = t[i].region; return 0; }
    return cs_paramErr;
}
OSStatus UCCreateCollator(void *locale, unsigned opVariant, unsigned opts, void **col) {
    (void)opVariant;
    const char *loc = locale ? (const char *)locale : cx_locale_id();
    return cs_collator_new(loc, opts, (cs_collator **)col);
}
OSStatus UCDisposeCollator(void **col) { if (!col) return cs_paramErr; cs_collator_free(*col); *col = 0; return 0; }
OSStatus UCCompareText(void *col, const u16 *a, unsigned long an, const u16 *b, unsigned long bn, u8 *eq, int *order) {
    return cs_collate(col, a, an, b, bn, eq, order);
}
OSStatus UCCompareTextDefault(unsigned opts, const u16 *a, unsigned long an, const u16 *b, unsigned long bn, u8 *eq, int *order) {
    cs_collator *k = 0;
    OSStatus st = cs_collator_new(cx_locale_id(), opts, &k);
    if (st) return st;
    st = cs_collate(k, a, an, b, bn, eq, order);
    cs_collator_free(k);
    return st;
}
OSStatus UCGetCollationKey(void *col, const u16 *t, unsigned long n, ItemCount max, ItemCount *got, unsigned *key) {
    return cs_collation_key(col, t, n, max, got, key);
}
OSStatus UCCompareCollationKeys(const unsigned *a, ItemCount an, const unsigned *b, ItemCount bn, u8 *eq, int *order) {
    if ((!a && an) || (!b && bn)) return cs_paramErr;
    cs_compare_keys(a, an, b, bn, eq, order);
    return 0;
}

/* ================================================================ Text Encoding Conversion */
unsigned CreateTextEncoding(unsigned base, unsigned variant, unsigned format) { return cs_text_encoding(base, variant, format); }
unsigned GetTextEncodingBase(unsigned e) { return e & 0xffff; }
unsigned GetTextEncodingVariant(unsigned e) { return (e >> 16) & 0x3ff; }
unsigned GetTextEncodingFormat(unsigned e) { return (e >> 26) & 0x3f; }
/* os códigos de script do Mac (smRoman = 0, smJapanese = 1...) são a base da codificação Mac do mesmo número */
OSStatus UpgradeScriptInfoToTextEncoding(short script, short lang, short region, const u8 *font, unsigned *enc) {
    (void)region; (void)font;
    if (!enc) return cs_paramErr;
    if (script == -128 /* kTextScriptDontCare */) script = lang == 11 ? 1 : lang == 23 ? 3 : lang == 32 ? 7 : 0;
    if (script < 0 || script > 32) return -8738;
    *enc = (unsigned)script;
    return 0;
}
OSStatus RevertTextEncodingToScriptInfo(unsigned enc, short *script, short *lang, u8 *font) {
    (void)font;
    unsigned b = enc & 0xffff;
    if (b > 32) return -8738;
    if (script) *script = (short)b;
    if (lang) *lang = b == 1 ? 11 : b == 3 ? 23 : b == 7 ? 32 : 0;
    return 0;
}
OSStatus GetTextEncodingFromScriptInfo(short script, short lang, short region, unsigned *enc) {
    return UpgradeScriptInfoToTextEncoding(script, lang, region, 0, enc);
}
OSStatus ResolveDefaultTextEncoding(unsigned enc) { return enc; }
/* UnicodeMapping {unicodeEncoding, otherEncoding, mappingVersion} */
typedef struct { unsigned unicode, other; int version; } umap_t;
typedef struct { unsigned magic; unsigned other; } t2u_t;
#define T2U_MAGIC 0x54325521u
static OSStatus t2u_new(unsigned other, void **info) {
    if (!info) return cs_paramErr;
    if (!enc_available(cs_cf_enc(other))) return kTextUnsupportedEncodingErr;
    t2u_t *t = malloc(sizeof *t);
    if (!t) return cs_memFullErr;
    t->magic = T2U_MAGIC; t->other = other;
    *info = t;
    return 0;
}
OSStatus CreateTextToUnicodeInfo(const umap_t *m, void **info) { return m ? t2u_new(m->other, info) : cs_paramErr; }
OSStatus CreateTextToUnicodeInfoByEncoding(unsigned enc, void **info) { return t2u_new(enc, info); }
OSStatus CreateUnicodeToTextInfo(const umap_t *m, void **info) { return m ? t2u_new(m->other, info) : cs_paramErr; }
OSStatus CreateUnicodeToTextInfoByEncoding(unsigned enc, void **info) { return t2u_new(enc, info); }
static OSStatus t2u_free(void **info) {
    if (!info || !*info || ((t2u_t *)*info)->magic != T2U_MAGIC) return cs_paramErr;
    ((t2u_t *)*info)->magic = 0;
    free(*info);
    *info = 0;
    return 0;
}
OSStatus DisposeTextToUnicodeInfo(void **info) { return t2u_free(info); }
OSStatus DisposeUnicodeToTextInfo(void **info) { return t2u_free(info); }
/* kUnicodeUseFallbacksMask = 1 << 0 (UnicodeConverter.h): caractere sem equivalente vira '?' em vez de erro */
OSStatus ConvertFromTextToUnicode(void *info, ByteCount n, const void *src, unsigned flags, ItemCount noff, const ByteCount *offs,
                                  ItemCount *noffOut, ByteCount *offsOut, ByteCount outCap, ByteCount *read, ByteCount *outLen,
                                  u16 *out) {
    (void)flags; (void)noff; (void)offs; (void)offsOut;
    t2u_t *t = info;
    if (noffOut) *noffOut = 0;
    if (!t || t->magic != T2U_MAGIC || (!src && n) || (!out && outCap)) return cs_paramErr;
    return cs_to_unicode(t->other, src, n, read, out, outCap, outLen);
}
OSStatus ConvertFromUnicodeToText(void *info, ByteCount n, const u16 *src, unsigned flags, ItemCount noff, const ByteCount *offs,
                                  ItemCount *noffOut, ByteCount *offsOut, ByteCount outCap, ByteCount *read, ByteCount *outLen,
                                  void *out) {
    (void)noff; (void)offs; (void)offsOut;
    t2u_t *t = info;
    if (noffOut) *noffOut = 0;
    if (!t || t->magic != T2U_MAGIC || (!src && n) || (!out && outCap)) return cs_paramErr;
    return cs_from_unicode(t->other, src, n, flags & 1, read, out, outCap, outLen);
}
OSStatus TECCreateConverter(void **conv, unsigned from, unsigned to) {
    if (!conv) return cs_paramErr;
    *conv = 0;
    if (!enc_available(cs_cf_enc(from)) || !enc_available(cs_cf_enc(to))) return -8749;   /* kTECNoConversionPathErr */
    cs_tec *t = calloc(1, sizeof *t);
    if (!t) return cs_memFullErr;
    t->magic = CS_TEC_MAGIC; t->from = from; t->to = to;
    *conv = t;
    return 0;
}
OSStatus TECDisposeConverter(void *conv) {
    cs_tec *t = conv;
    if (!t || t->magic != CS_TEC_MAGIC) return cs_paramErr;
    t->magic = 0;
    free(t);
    return 0;
}
OSStatus TECConvertText(void *conv, const u8 *in, ByteCount n, ByteCount *read, u8 *out, ByteCount cap, ByteCount *wrote) {
    return cs_tec_convert(conv, in, n, read, out, cap, wrote);
}
/* o que ficou guardado no fim é um caractere incompleto: não há como escrevê-lo, então é descartado */
OSStatus TECFlushText(void *conv, u8 *out, ByteCount cap, ByteCount *wrote) {
    (void)out; (void)cap;
    cs_tec *t = conv;
    if (wrote) *wrote = 0;
    if (!t || t->magic != CS_TEC_MAGIC) return cs_paramErr;
    int had = t->npend;
    t->npend = 0;
    return had ? kTextMalformedInputErr : 0;
}
id CFStringConvertEncodingToIANACharSetName(u32 cf);
u32 CFStringConvertIANACharSetNameToEncoding(id name);
/* nome de internet em Str255 (Pascal) */
OSStatus TECGetTextEncodingInternetName(unsigned enc, u8 *name) {
    id s = CFStringConvertEncodingToIANACharSetName(cs_cf_enc(enc));
    const char *c = s ? cx_cstr(s) : 0;
    if (!c || !name) return kTextUnsupportedEncodingErr;
    size_t n = strlen(c);
    if (n > 255) n = 255;
    name[0] = (u8)n;
    memcpy(name + 1, c, n);
    return 0;
}
OSStatus TECGetTextEncodingFromInternetNameOrMIB(unsigned *enc, unsigned usage, const u8 *name, long mib) {
    (void)usage; (void)mib;
    if (!enc || !name) return cs_paramErr;
    char b[256];
    memcpy(b, name + 1, name[0]);
    b[name[0]] = 0;
    u32 e = CFStringConvertIANACharSetNameToEncoding(cx_str(b));
    if (e == 0xffffffffu) return kTextUnsupportedEncodingErr;
    *enc = e;
    return 0;
}
OSStatus TECCopyTextEncodingInternetNameAndMIB(unsigned enc, unsigned usage, id *name, long *mib) {
    (void)usage;
    id s = CFStringConvertEncodingToIANACharSetName(cs_cf_enc(enc));
    if (!s) return kTextUnsupportedEncodingErr;
    if (name) *name = objc_retain(s);
    if (mib) *mib = -1;                                     /* kTEC_MIBEnumDontCare: o mOS não guarda os números MIB */
    return 0;
}
OSStatus TECCountAvailableTextEncodings(ItemCount *n) {
    if (!n) return cs_paramErr;
    const u32 *l = enc_list();
    ItemCount k = 0;
    while (l[k] != ENC_INVALID) k++;
    *n = k;
    return 0;
}
OSStatus TECGetAvailableTextEncodings(unsigned *out, ItemCount max, ItemCount *got) {
    const u32 *l = enc_list();
    ItemCount k = 0;
    for (; l[k] != ENC_INVALID && k < max; k++) if (out) out[k] = l[k];
    if (got) *got = k;
    return 0;
}
/* farejador: tenta cada codificação candidata e conta erros/caracteres válidos, como TECSniffTextEncoding */
typedef struct { unsigned magic; unsigned n; unsigned enc[64]; } sniff_t;
OSStatus TECCreateSniffer(void **s, const unsigned *encs, ItemCount n) {
    if (!s || (!encs && n) || n > 64) return cs_paramErr;
    sniff_t *k = calloc(1, sizeof *k);
    if (!k) return cs_memFullErr;
    k->magic = 0x534e4946u;
    for (ItemCount i = 0; i < n; i++) k->enc[k->n++] = encs[i];
    *s = k;
    return 0;
}
OSStatus TECDisposeSniffer(void *s) { sniff_t *k = s; if (!k || k->magic != 0x534e4946u) return cs_paramErr; k->magic = 0; free(k); return 0; }
OSStatus TECSniffTextEncoding(void *s, const u8 *text, ByteCount n, unsigned *encs, ItemCount ne, ItemCount *errs, ItemCount maxErrs,
                              ItemCount *feats, ItemCount maxFeats) {
    (void)maxErrs; (void)maxFeats;
    sniff_t *k = s;
    if (!k || k->magic != 0x534e4946u || !encs) return cs_paramErr;
    if (ne > k->n) ne = k->n;
    /* erros = 0 se decodifica sem perda; ordena as válidas primeiro, na ordem pedida */
    ItemCount w = 0;
    for (int pass = 0; pass < 2; pass++)
        for (unsigned i = 0; i < k->n && w < ne; i++) {
            int32_t un = 0;
            UChar *u = enc_decode(text, (int32_t)n, cs_cf_enc(k->enc[i]), &un);
            int ok = u != 0;
            free(u);
            if (ok != (pass == 0)) continue;
            encs[w] = k->enc[i];
            if (errs) errs[w] = ok ? 0 : n;
            if (feats) feats[w] = ok ? (ItemCount)un : 0;
            w++;
        }
    return 0;
}

/* ================================================================ Memory Manager */
Handle NewHandle(long size) { return size < 0 ? 0 : cs_new_handle((unsigned long)size, 0); }
Handle NewHandleClear(long size) { return size < 0 ? 0 : cs_new_handle((unsigned long)size, 1); }
Handle TempNewHandle(long size, OSErr *err) { Handle h = NewHandle(size); if (err) *err = cs_memerr; return h; }
void DisposeHandle(Handle h) { cs_dispose_handle(h); }
long GetHandleSize(Handle h) { return cs_handle_size(h); }
void SetHandleSize(Handle h, long size) { cs_set_handle_size(h, size); }
void HLock(Handle h) { if (h) cs_master_of(h)->state |= (signed char)0x80; cs_memerr = h ? 0 : cs_nilHandleErr; }
void HUnlock(Handle h) { if (h) cs_master_of(h)->state &= 0x7f; cs_memerr = h ? 0 : cs_nilHandleErr; }
signed char HGetState(Handle h) { cs_memerr = h ? 0 : cs_nilHandleErr; return h ? cs_master_of(h)->state : 0; }
void HSetState(Handle h, signed char s) { if (h) cs_master_of(h)->state = s; cs_memerr = h ? 0 : cs_nilHandleErr; }
Ptr NewPtr(long size) { return size < 0 ? 0 : cs_new_ptr((unsigned long)size, 0); }
Ptr NewPtrClear(long size) { return size < 0 ? 0 : cs_new_ptr((unsigned long)size, 1); }
void DisposePtr(Ptr p) { cs_dispose_ptr(p); }
long GetPtrSize(Ptr p) { return (long)cs_ptr_size(p); }
OSErr PtrToHand(const void *src, Handle *dst, long size) {
    if (!dst || size < 0) return cs_paramErr;
    *dst = NewHandle(size);
    if (!*dst) return cs_memerr;
    if (size) memcpy(**dst, src, (size_t)size);
    return 0;
}
OSErr MemError(void) { return cs_memerr; }

/* ================================================================ Gestalt, nomes e relógios */
#define gestaltUndefSelectorErr (-5551)
#define FC(a, b, c, d) ((OSType)(a) << 24 | (OSType)(b) << 16 | (OSType)(c) << 8 | (OSType)(d))
static void os_version(int *maj, int *min, int *fix) {
    char v[32] = "14.0";
    size_t n = sizeof v;
    sysctlbyname("kern.osproductversion", v, &n, 0, 0);
    *maj = *min = *fix = 0;
    int *p[3] = {maj, min, fix}, k = 0;
    for (const char *c = v; *c && k < 3; c++) {
        if (*c == '.') { k++; continue; }
        if (*c >= '0' && *c <= '9') *p[k] = *p[k] * 10 + (*c - '0');
    }
}
OSErr Gestalt(OSType sel, int *resp) {
    if (!resp) return cs_paramErr;
    int maj, min, fix;
    os_version(&maj, &min, &fix);
    switch (sel) {
    case FC('s', 'y', 's', 'v'):                            /* 0x10XY; do 10.10 em diante o macOS devolve 0x1099 */
        *resp = maj == 10 && min < 10 ? 0x1000 | min << 4 | (fix > 9 ? 9 : fix) : 0x1099; return 0;
    case FC('s', 'y', 's', '1'): *resp = maj; return 0;
    case FC('s', 'y', 's', '2'): *resp = min; return 0;
    case FC('s', 'y', 's', '3'): *resp = fix; return 0;
    case FC('r', 'a', 'm', ' '): case FC('l', 'r', 'a', 'm'): {
        unsigned long long mem = 0;
        size_t n = sizeof mem;
        if (sysctlbyname("hw.memsize", &mem, &n, 0, 0)) return gestaltUndefSelectorErr;
        *resp = mem > 0x7fffffffULL ? 0x7fffffff : (int)mem;
        return 0;
    }
    case FC('p', 'c', 'l', 'k'): case FC('p', 'r', 'o', 'c'): case FC('c', 'p', 'u', 't'): return gestaltUndefSelectorErr;
    }
    return gestaltUndefSelectorErr;
}
id CSCopyMachineName(void) {
    char b[256];
    if (gethostname(b, sizeof b)) return 0;
    b[sizeof b - 1] = 0;
    char *dot = strchr(b, '.');
    if (dot) *dot = 0;                                       /* nome do computador, sem o domínio */
    return objc_retain(cx_str(b));
}
typedef struct { char *pw_name, *pw_passwd; unsigned pw_uid, pw_gid; long pw_change; char *pw_class, *pw_gecos, *pw_dir, *pw_shell; long pw_expire; } cs_passwd;
cs_passwd *getpwuid(unsigned uid);
unsigned getuid(void);
id CSCopyUserName(u8 shortName) {
    cs_passwd *p = getpwuid(getuid());
    const char *n = p ? (shortName || !p->pw_gecos || !p->pw_gecos[0] ? p->pw_name : p->pw_gecos) : getenv("USER");
    if (!n) return 0;
    char b[256];
    size_t i = 0;
    for (; n[i] && n[i] != ',' && i + 1 < sizeof b; i++) b[i] = n[i];   /* o gecos pode trazer ",sala,fone" */
    b[i] = 0;
    return objc_retain(cx_str(b));
}
/* mach_absolute_time do mOS é em nanossegundos (timebase 1/1) */
unsigned TickCount(void) { return (unsigned)(mach_absolute_time() * 60 / 1000000000ULL); }
void Microseconds(unsigned *wide) { unsigned long long us = mach_absolute_time() / 1000; if (wide) { wide[0] = (unsigned)(us >> 32); wide[1] = (unsigned)us; } }
unsigned long long UpTime(void) { return mach_absolute_time(); }
/* Duration: milissegundos positivos, ou microssegundos negativos se couber melhor */
int AbsoluteToDuration(unsigned long long t) {
    unsigned long long us = t / 1000;
    if (us < 0x7fffffffULL) return -(int)us;
    unsigned long long ms = us / 1000;
    return ms > 0x7fffffffULL ? 0x7fffffff : (int)ms;
}
int IncrementAtomic(int *v) { return __sync_fetch_and_add(v, 1); }
int DecrementAtomic(int *v) { return __sync_fetch_and_sub(v, 1); }
void DebugStr(const u8 *s) {
    if (!s) return;
    write(2, s + 1, s[0]);
    write(2, "\n", 1);
}
/* atividade do usuário: adia o descanso da tela; o mOS não apaga a tela por inatividade sozinho */
OSErr UpdateSystemActivity(u8 type) { return type > 4 ? cs_paramErr : 0; }
const char *GetMacOSStatusErrorString(OSStatus e) { int i = cs_err_index(e); return i < 0 ? "" : cs_errs[i].name; }
const char *GetMacOSStatusCommentString(OSStatus e) { int i = cs_err_index(e); return i < 0 ? "" : cs_errs[i].comment; }

/* ================================================================ IconRef
 * Um IconRef é um objeto com contagem de referência que diz qual ícone: o tipo (OSType) de um ícone do sistema ou
 * o UTI de um tipo de arquivo. cx_iconref_uti() dá o UTI para quem desenha (AppKit: NSImage/NSWorkspace). */
typedef struct { unsigned magic; int refs; OSType creator, type; char uti[128]; void *family; long fsize; } iconref_t;
#define ICON_MAGIC 0x49636f6eu
static const struct { OSType type; const char *uti; } sys_icons[] = {
    {FC('f', 'l', 'd', 'r'), "public.folder"}, {FC('d', 'o', 'c', 'u'), "public.data"},
    {FC('A', 'P', 'P', 'L'), "com.apple.application-bundle"}, {FC('t', 'r', 's', 'h'), "com.apple.trash"},
    {FC('h', 'd', 's', 'k'), "public.volume"}, {FC('c', 'd', 'd', 'r'), "public.volume"}, {FC('s', 'r', 'v', 'r'), "com.apple.file-server"},
    {FC('t', 'e', 'x', 't'), "public.plain-text"}, {FC('f', 'd', 'r', 'p'), "com.apple.alias-file"},
    {FC('c', 'a', 'u', 't'), "com.apple.caution-icon"}, {FC('s', 't', 'o', 'p'), "com.apple.stop-icon"},
    {FC('n', 'o', 't', 'e'), "com.apple.note-icon"}, {FC('s', 'h', 'f', 'd'), "com.apple.shared-folder"},
    {FC('d', 'r', 'o', 'p'), "com.apple.drop-folder"}, {FC('h', 'o', 'm', 'e'), "com.apple.home-folder"},
};
static iconref_t *icon_new(OSType creator, OSType type, const char *uti) {
    iconref_t *r = calloc(1, sizeof *r);
    if (!r) return 0;
    r->magic = ICON_MAGIC; r->refs = 1; r->creator = creator; r->type = type;
    if (uti) { size_t n = strlen(uti); if (n >= sizeof r->uti) n = sizeof r->uti - 1; memcpy(r->uti, uti, n); }
    return r;
}
static iconref_t *icon_of(void *p) { iconref_t *r = p; return r && r->magic == ICON_MAGIC ? r : 0; }
#define noSuchIconErr (-2582)
static struct { OSType creator, type; iconref_t *ref; } g_reg[32];
OSErr GetIconRef(short vref, OSType creator, OSType type, void **out) {
    (void)vref;
    if (!out) return cs_paramErr;
    *out = 0;
    for (int i = 0; i < 32; i++)
        if (g_reg[i].ref && g_reg[i].creator == creator && g_reg[i].type == type) { g_reg[i].ref->refs++; *out = g_reg[i].ref; return 0; }
    const char *uti = 0;
    char buf[128];
    for (unsigned i = 0; i < sizeof sys_icons / sizeof sys_icons[0]; i++) if (sys_icons[i].type == type) uti = sys_icons[i].uti;
    if (!uti) {                                              /* tipo de arquivo pelo OSType registrado no LaunchServices */
        char t[5] = {(char)(type >> 24), (char)(type >> 16), (char)(type >> 8), (char)type, 0};
        if (cx_ls_type_for_tag("com.apple.ostype", t, 0, buf, sizeof buf)) uti = buf;
    }
    if (!uti) return noSuchIconErr;
    *out = icon_new(creator, type, uti);
    return *out ? 0 : cs_memFullErr;
}
OSStatus GetIconRefFromTypeInfo(OSType creator, OSType type, id ext, id mime, unsigned usage, void **out) {
    (void)usage;
    if (!out) return cs_paramErr;
    *out = 0;
    char buf[128];
    const char *e = uti_c(ext), *m = uti_c(mime);
    if ((e && cx_ls_type_for_tag("public.filename-extension", e, 0, buf, sizeof buf)) ||
        (m && cx_ls_type_for_tag("public.mime-type", m, 0, buf, sizeof buf))) {
        *out = icon_new(creator, type, buf);
        return *out ? 0 : cs_memFullErr;
    }
    if (type) return GetIconRef(0, creator, type, out);
    *out = icon_new(creator, type, "public.data");           /* sem informação: o ícone de documento genérico */
    return *out ? 0 : cs_memFullErr;
}
OSErr AcquireIconRef(void *ref) { iconref_t *r = icon_of(ref); if (!r) return cs_paramErr; r->refs++; return 0; }
OSErr ReleaseIconRef(void *ref) {
    iconref_t *r = icon_of(ref);
    if (!r) return cs_paramErr;
    if (--r->refs > 0) return 0;
    for (int i = 0; i < 32; i++) if (g_reg[i].ref == r) return 0;   /* registrado: vive até UnregisterIconRef */
    r->magic = 0;
    free(r->family);
    free(r);
    return 0;
}
u8 IsValidIconRef(void *ref) { return icon_of(ref) != 0; }
/* ícone próprio a partir de um IconFamilyHandle (dados 'icns'), achado depois por GetIconRef(creator, type) */
OSStatus RegisterIconRefFromIconFamily(OSType creator, OSType type, Handle family, void **out) {
    if (!family || !out) return cs_paramErr;
    int slot = -1;
    for (int i = 0; i < 32 && slot < 0; i++) if (!g_reg[i].ref || (g_reg[i].creator == creator && g_reg[i].type == type)) slot = i;
    if (slot < 0) return cs_memFullErr;
    iconref_t *r = icon_new(creator, type, "com.apple.icns");
    if (!r) return cs_memFullErr;
    r->fsize = GetHandleSize(family);
    r->family = malloc((size_t)(r->fsize ? r->fsize : 1));
    if (r->fsize && r->family) memcpy(r->family, *family, (size_t)r->fsize);
    if (g_reg[slot].ref) { iconref_t *o = g_reg[slot].ref; g_reg[slot].ref = 0; ReleaseIconRef(o); }
    g_reg[slot].creator = creator; g_reg[slot].type = type; g_reg[slot].ref = r;
    r->refs++;
    *out = r;
    return 0;
}
OSErr UnregisterIconRef(OSType creator, OSType type) {
    for (int i = 0; i < 32; i++)
        if (g_reg[i].ref && g_reg[i].creator == creator && g_reg[i].type == type) {
            iconref_t *r = g_reg[i].ref;
            g_reg[i].ref = 0;
            return ReleaseIconRef(r);
        }
    return noSuchIconErr;
}
/* para o AppKit: UTI do ícone e, se for um ícone registrado, os dados icns */
const char *cx_iconref_uti(void *ref) { iconref_t *r = icon_of(ref); return r ? r->uti : 0; }
const void *cx_iconref_icns(void *ref, long *len) { iconref_t *r = icon_of(ref); if (len) *len = r ? r->fsize : 0; return r ? r->family : 0; }

/* ================================================================ propriedades de caractere (UCGetCharProperty) sobre o ICU */
enum { kUCCharPropTypeGenlCategory = 1, kUCCharPropTypeCombiningClass = 2, kUCCharPropTypeBidiCategory = 3 };
OSStatus UCGetCharProperty(const u16 *s, unsigned long n, u32 type, u32 *out) {
    if (!s || !n || !out) return cs_paramErr;
    const fdn_icu_t *I = fdn_icu();
    if (!I || !I->ok) return cs_paramErr;
    u32 c = s[0];
    if (c >= 0xd800 && c < 0xdc00 && n > 1 && s[1] >= 0xdc00 && s[1] < 0xe000) c = 0x10000 + ((c - 0xd800) << 10) + (s[1] - 0xdc00);
    switch (type) {
    case kUCCharPropTypeGenlCategory: {
        /* UCharCategory do ICU -> kUCGenlCat* (a ordem da Apple é outra) */
        static const u8 map[30] = {0, 14, 15, 16, 17, 18, 5, 7, 6, 8, 9, 10, 11, 12, 13, 1, 2, 4, 3, 21, 22, 23, 20, 26, 28, 29, 30, 31, 24, 25};
        int t = I->u_charType ? I->u_charType((UChar32)c) : 0;
        *out = t >= 0 && t < 30 ? map[t] : 0;
        return 0;
    }
    case kUCCharPropTypeCombiningClass:
        *out = I->u_getCombiningClass ? I->u_getCombiningClass((UChar32)c) : 0;
        return 0;
    case kUCCharPropTypeBidiCategory: {
        /* UCharDirection do ICU -> kUCBidiCat*; os isolados (19..22) viram neutro */
        static const u8 map[19] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 12, 14, 16, 17, 18, 19};
        int d = I->u_charDirection ? (int)I->u_charDirection((UChar32)c) : 0;
        *out = d >= 0 && d < 19 ? map[d] : 11;
        return 0;
    }
    }
    return cs_paramErr;
}

/* ================================================================ Script Manager: só o script romano; a região vem do locale */
enum { smVersion = 0, smMunged = 2, smEnabled = 4, smBidirect = 6, smFontForce = 8, smIntlForce = 10, smForced = 12, smDefault = 14,
       smPrint = 16, smSysScript = 18, smLastScript = 20, smKeyScript = 22, smSysRef = 24, smKeyCache = 26, smKeySwap = 28,
       smGenFlags = 30, smOverride = 32, smCharPortion = 34, smDoubleByte = 36, smKCHRCache = 38, smRegionCode = 40, smKeyDisableState = 42 };
static int region_code(void) {
    static const struct { const char *loc; int code; } t[] = {
        {"en_US", 0}, {"fr_FR", 1}, {"en_GB", 2}, {"de_DE", 3}, {"it_IT", 4}, {"nl_NL", 5}, {"sv_SE", 7}, {"es_ES", 8}, {"ja_JP", 14},
{"en_US", 0},
{"nl_BE", 6},
{"da_DK", 9},
{"pt_PT", 10},
{"he_IL", 13},
{"ar", 16},
{"fr_CH", 18},
{"de_CH", 19},
{"el_GR", 20},
{"is_IS", 21},
{"mt_MT", 22},
{"tr_TR", 24},
{"hi_IN", 33},
{"ur_PK", 34},
{"it_CH", 36},
{"ro_RO", 39},
{"lt_LT", 41},
{"pl_PL", 42},
{"hu_HU", 43},
{"et_EE", 44},
{"lv_LV", 45},
{"se", 46},
{"fo_FO", 47},
{"fa_IR", 48},
{"ru_RU", 49},
{"ga_IE", 50},
{"ko_KR", 51},
{"zh_CN", 52},
{"zh_TW", 53},
{"th_TH", 54},
{"cs_CZ", 56},
{"sk_SK", 57},
{"bn", 60},
{"be_BY", 61},
{"uk_UA", 62},
{"sr_YU", 65},
{"sl_SI", 66},
{"mk_MK", 67},
{"hr_HR", 68},
{"de_DE", 70},
{"pt_BR", 71},
{"bg_BG", 72},
{"ca_ES", 73},
{"gd", 75},
{"gv", 76},
{"br", 77},
{"iu_CA", 78},
{"cy", 79},
{"dz_BT", 83},
{"hy_AM", 84},
{"ka_GE", 85},
{"es", 86},
{"to_TO", 88},
{"fr", 91},
{"de_AT", 92},
{"gu_IN", 94},
{"pa", 95},
{"ur_IN", 96},
{"vi_VN", 97},
{"fr_BE", 98},
{"uz_UZ", 99},
{"nn_NO", 101},
{"af_ZA", 102},
{"eo", 103},
{"mr_IN", 104},
{"bo", 105},
{"ne_NP", 106},
{"kl", 107},
{"en_IE", 108},
    };
    const char *l = cx_locale_id();
    if (!l) return 0;
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++) if (!strncmp(l, t[i].loc, strlen(t[i].loc))) return t[i].code;
    for (size_t i = 0; i < sizeof t / sizeof t[0]; i++) if (!strncmp(l, t[i].loc, 2)) return t[i].code;   /* só a língua */
    return 0;
}
long GetScriptManagerVariable(short sel) {
    switch (sel) {
    case smVersion: return 0x0710;                                               /* Script Manager 7.1 */
    case smEnabled: return 1;                                                    /* só o romano */
    case smRegionCode: return region_code();
    case smCharPortion: return 0x0100;                                           /* 1/4 do espaço para caracteres (padrão) */
    default: return 0;                                                           /* smRoman, sem bidi, sem 2 bytes, teclado livre */
    }
}
OSErr SetScriptManagerVariable(short sel, long v) { (void)sel; (void)v; return 0; }
