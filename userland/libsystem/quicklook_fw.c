/*
 * mOS libSystem — Quick Look: QuickLookUI, QuickLook (C) e QuickLookThumbnailing.
 *
 * Como funciona a pré-visualização de um arquivo (ql_content):
 *   1. extensão do app (PlugIns/<nome>.appex com NSExtensionPointIdentifier com.apple.quicklook.preview): o QLPreviewProvider
 *      do app é carregado no processo e devolve um QLPreviewReply (dados de um tipo, arquivo ou PDF);
 *   2. gerador antigo (.qlgenerator do app, de ~/Library/QuickLook ou /Library/QuickLook) pelo CFPlugIn:
 *      GeneratePreviewForURL preenche um QLPreviewRequest (dados, URL ou desenho num contexto);
 *   3. o próprio mOS por tipo: imagem, PDF (PDFView), texto e código, RTF, HTML (WKWebView quando o WebKit está),
 *      áudio e vídeo (AVPlayerView); senão o ícone grande com nome, tipo, tamanho e data.
 * Miniaturas (QLThumbnailImageCreate, QLThumbnail*, QLThumbnailGenerator): extensão de miniatura do app
 * (QLThumbnailProvider), gerador antigo (GenerateThumbnailForURL), imagem/PDF do mOS; o ícone só no modo de ícone.
 *
 * QLPreviewPanel é um NSPanel de verdade (setas trocam o item, espaço/esc fecham) com o controle pela cadeia de
 * respondedores (acceptsPreviewPanelControl:/begin/endPreviewPanelControl:) e QLPreviewView é uma NSView.
 * Código original do mOS. Registro: cx_quicklook_init (userspace_services.c).
 */
#include "foundation.h"
#include "libc.h"

id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
void objc_setAssociatedObject(id obj, const void *key, id value, u64 policy);
id objc_getAssociatedObject(id obj, const void *key);
IMP cx_lookup(cx_class_t *c, SEL s);
void *_Block_copy(const void *b);
void _Block_release(const void *b);
extern char _NSConcreteGlobalBlock[];
void *dispatch_get_global_queue(long prio, unsigned long flags);
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
u8 UTTypeConformsTo(id a, id b);
id cx_color(double r, double g, double b, double a);

typedef struct { double x, y; } P;
typedef struct { double w, h; } S;
typedef struct { double x, y, w, h; } R;
static cx_class_t *kls(const char *n) { return cx_class_named(n); }
static SEL sl(const char *n) { return sel_registerName(n); }
static id str(const char *s) { return cx_str(s); }
static BOOL responds(id o, const char *s) { return o && cx_responds(object_getClass(o), sl(s)); }
static id dict(void) { return MSG0(id, MSG0(id, (id)kls("NSMutableDictionary"), "alloc"), "init"); }
static id aget(id o, const void *k) { return objc_getAssociatedObject(o, k); }
static void aset(id o, const void *k, id v) { objc_setAssociatedObject(o, k, v, 1); }
#define BLK_FN(b) (*(void **)((u8 *)(b) + 16))
static id num_ull(u64 v) { return MSG(id, (id)kls("NSNumber"), "numberWithUnsignedLongLong:", v); }
static id num_bool(BOOL v) { return MSG(id, (id)kls("NSNumber"), "numberWithBool:", v); }
static id num_ll(i64 v) { return MSG(id, (id)kls("NSNumber"), "numberWithLongLong:", v); }
static id num_dbl(double v) { return MSG(id, (id)kls("NSNumber"), "numberWithDouble:", v); }
static id val_size(S v) { return MSG(id, (id)kls("NSValue"), "valueWithSize:", v); }
static id val_ptr(void *v) { return MSG(id, (id)kls("NSValue"), "valueWithPointer:", v); }
static id font_msg(const char *sel, double size) { return MSG(id, (id)kls("NSFont"), sel, size); }
static id path_join(id a, id b) { return MSG(id, a, "stringByAppendingPathComponent:", b); }
static id at(id arr, i64 i) { return MSG(id, arr, "objectAtIndex:", i); }
static id oget(id d, id k) { return MSG(id, d, "objectForKey:", k); }

/* Core Graphics */
id CGBitmapContextCreate(void *data, size_t w, size_t h, size_t bpc, size_t bpr, void *space, u32 info);
void *CGBitmapContextCreateImage(id c);
void *CGColorSpaceCreateDeviceRGB(void);
void CGColorSpaceRelease(void *s);
void CGContextRelease(id c);
void CGImageRelease(void *i);
void *CGImageRetain(void *i);
size_t CGImageGetWidth(void *i);
size_t CGImageGetHeight(void *i);
void CGContextDrawImage(id c, R r, void *img);
void CGContextScaleCTM(id c, double sx, double sy);
id CGDataConsumerCreateWithCFData(id data);
void CGDataConsumerRelease(id c);
id CGPDFContextCreate(id consumer, const R *media, id info);
void CGPDFContextBeginPage(id c, id info);
void CGPDFContextEndPage(id c);
void CGPDFContextClose(id c);

/* ================================================================ classes */
#define CLS(n) cx_class_t n##_c __asm__("OBJC_CLASS_$_" #n), n##_m __asm__("OBJC_METACLASS_$_" #n);
extern cx_class_t QLPreviewPanel_c __asm__("OBJC_CLASS_$_QLPreviewPanel");
CLS(QLPreviewView) CLS(QLPreviewProvider) CLS(QLPreviewReply) CLS(QLPreviewReplyAttachment) CLS(QLFilePreviewRequest)
CLS(QLThumbnailProvider) CLS(QLThumbnailReply) CLS(QLThumbnailRepresentation) CLS(QLFileThumbnailRequest)
CLS(QLThumbnailGenerator) CLS(QLThumbnailGenerationRequest) CLS(_MOSQLRequest)

/* constantes CFString do QuickLook (QLGenerator.h) */
extern char cx_cfconst_isa[] __asm__("__CFConstantStringClassReference");
typedef struct { void *isa; u64 flags; const char *s; u64 n; } cfs_t;
#define QK(var, text) static cfs_t var##_s = {cx_cfconst_isa, 0x7c8, text, sizeof(text) - 1}; const void *var = &var##_s;
QK(kQLPreviewPropertyDisplayNameKey, "DisplayName") QK(kQLPreviewPropertyWidthKey, "Width") QK(kQLPreviewPropertyHeightKey, "Height")
QK(kQLPreviewPropertyStringEncodingKey, "StringEncoding") QK(kQLPreviewPropertyMIMETypeKey, "MimeType")
QK(kQLPreviewPropertyTextEncodingNameKey, "TextEncoding") QK(kQLPreviewPropertyAttachmentDataKey, "AttachmentData")
QK(kQLPreviewPropertyAttachmentsKey, "Attachments") QK(kQLThumbnailOptionIconModeKey, "IconMode")
#define KEY(v) ((id)(v))

/* ================================================================ tipos */
static id uti_of(id url) {
    id path = url ? MSG0(id, url, "path") : 0;
    if (!path) return 0;
    id ws = MSG0(id, (id)kls("NSWorkspace"), "sharedWorkspace");
    return ((id (*)(id, SEL, id, id *))objc_msgSend)(ws, sl("typeOfFile:error:"), path, (id *)0);
}
static int conforms(id uti, const char *t) { return uti && UTTypeConformsTo(uti, str(t)); }
static int ext_in(id url, const char *list) {
    id e = url ? MSG0(id, MSG0(id, url, "pathExtension"), "lowercaseString") : 0;
    const char *x = e ? cx_cstr(e) : "";
    if (!*x) return 0;
    usize n = strlen(x);
    for (const char *p = list; *p;) {
        const char *q = p;
        while (*q && *q != ' ') q++;
        if ((usize)(q - p) == n && !memcmp(p, x, n)) return 1;
        p = *q ? q + 1 : q;
    }
    return 0;
}
#define IMG_EXT "png jpg jpeg jpe gif bmp tif tiff ico icns webp heic heif tga"
#define TXT_EXT "txt text md markdown c h cc cpp hpp m mm swift py js ts json xml plist yml yaml toml ini cfg conf sh zsh bash css csv tsv log sql rb go rs java kt lua pl php r"
#define MEDIA_EXT "mp4 m4v mov mp3 m4a aac wav aif aiff caf flac"

/* ================================================================ conteúdo de uma pré-visualização */
enum { QC_NONE, QC_IMAGE, QC_PDF, QC_TEXT, QC_ATTR, QC_HTML, QC_MEDIA, QC_ICON };
typedef struct { int kind; id url, data, text, image, title; } ql_content;
static void qc_free(ql_content *c) { id o[5] = {c->url, c->data, c->text, c->image, c->title}; for (int i = 0; i < 5; i++) if (o[i]) objc_release(o[i]); memset(c, 0, sizeof *c); }
static id keep(id o) { return o ? objc_retain(o) : 0; }
static id text_from_data(id data, u64 enc) {
    if (!data) return 0;
    id s = MSG(id, MSG0(id, (id)kls("NSString"), "alloc"), "initWithData:encoding:", data, enc ? enc : (u64)4);
    if (!s) s = MSG(id, MSG0(id, (id)kls("NSString"), "alloc"), "initWithData:encoding:", data, (u64)5);   /* Latin-1 */
    return s;
}
/* dados de um tipo → conteúdo (vale para geradores, extensões e arquivos) */
static int content_from_data(ql_content *c, id data, id uti, id props) {
    if (!data) return 0;
    if (conforms(uti, "com.adobe.pdf")) { c->kind = QC_PDF; c->data = keep(data); return 1; }
    if (conforms(uti, "public.image")) {
        id img = MSG(id, MSG0(id, (id)kls("NSImage"), "alloc"), "initWithData:", data);
        if (img) { c->kind = QC_IMAGE; c->image = img; return 1; }
    }
    if (conforms(uti, "public.rtf") || conforms(uti, "com.apple.flat-rtfd")) {
        id a = MSG(id, MSG0(id, (id)kls("NSAttributedString"), "alloc"), "initWithRTF:documentAttributes:", data, (id)0);
        if (a) { c->kind = QC_ATTR; c->text = a; return 1; }
    }
    u64 enc = 4;
    id e = props ? MSG(id, props, "objectForKey:", KEY(kQLPreviewPropertyStringEncodingKey)) : 0;
    if (e) enc = MSG0(u64, e, "unsignedLongLongValue");
    if (conforms(uti, "public.html")) { c->kind = QC_HTML; c->data = keep(data); c->text = text_from_data(data, enc); return 1; }
    if (conforms(uti, "public.text") || conforms(uti, "public.plain-text") || conforms(uti, "public.source-code") || !uti) {
        id s = text_from_data(data, enc);
        if (s) { c->kind = QC_TEXT; c->text = s; return 1; }
    }
    return 0;
}
static int content_from_file(ql_content *c, id url, id uti) {
    if (!url) return 0;
    if (conforms(uti, "public.movie") || conforms(uti, "public.audio") || conforms(uti, "public.audiovisual-content") || ext_in(url, MEDIA_EXT)) {
        c->kind = QC_MEDIA; c->url = keep(url); return 1;
    }
    id data = MSG(id, (id)kls("NSData"), "dataWithContentsOfURL:", url);
    if (!data) return 0;
    if (!uti && ext_in(url, IMG_EXT)) uti = str("public.image");
    if (!uti && ext_in(url, "pdf")) uti = str("com.adobe.pdf");
    if (!uti && ext_in(url, "rtf")) uti = str("public.rtf");
    if (!uti && ext_in(url, "html htm")) uti = str("public.html");
    if (!uti && ext_in(url, TXT_EXT)) uti = str("public.plain-text");
    if (ext_in(url, TXT_EXT) && !conforms(uti, "public.image")) uti = conforms(uti, "public.html") ? uti : str("public.plain-text");
    if (content_from_data(c, data, uti, 0)) { if (!c->url) c->url = keep(url); return 1; }
    return 0;
}

/* ---- blocos de conclusão feitos em C (para as extensões do app) */
typedef struct { void *isa; int flags, reserved; void *invoke; void *desc; } blk_hdr;
static struct { unsigned long reserved, size; } g_blk_desc = {0, sizeof(blk_hdr) + 3 * sizeof(void *)};
typedef struct { blk_hdr b; id result; id error; int done; } done_blk;
static void done_inv(done_blk *b, id result, id error) { b->result = keep(result); b->error = keep(error); b->done = 1; }
static done_blk *new_done(void) {
    done_blk *d = calloc(1, sizeof *d);
    if (!d) return 0;
    d->b.isa = _NSConcreteGlobalBlock; d->b.flags = 1 << 28; d->b.invoke = (void *)done_inv; d->b.desc = &g_blk_desc;
    return d;
}
/* espera a extensão responder, rodando o laço (as extensões podem responder depois) */
static void wait_done(done_blk *d, double secs) {
    id rl = MSG0(id, (id)kls("NSRunLoop"), "currentRunLoop");
    for (int i = 0; !d->done && i < (int)(secs * 50); i++) {
        id until = MSG(id, (id)kls("NSDate"), "dateWithTimeIntervalSinceNow:", 0.02);
        MSG(BOOL, rl, "runMode:beforeDate:", str("kCFRunLoopDefaultMode"), until);
    }
}

/* ---- extensões do app (as .appex de PlugIns) */
static id find_appex(const char *point, id uti) {
    id pl = MSG0(id, MSG0(id, (id)kls("NSBundle"), "mainBundle"), "builtInPlugInsPath");
    if (!pl) return 0;
    id fm = MSG0(id, (id)kls("NSFileManager"), "defaultManager");
    id items = ((id (*)(id, SEL, id, id *))objc_msgSend)(fm, sl("contentsOfDirectoryAtPath:error:"), pl, (id *)0);
    i64 n = items ? MSG0(i64, items, "count") : 0;
    for (i64 i = 0; i < n; i++) {
        id name = at(items, i);
        if (!MSG(BOOL, MSG0(id, name, "pathExtension"), "isEqualToString:", str("appex"))) continue;
        id b = MSG(id, (id)kls("NSBundle"), "bundleWithPath:", path_join(pl, name));
        id ext = b ? MSG(id, b, "objectForInfoDictionaryKey:", str("NSExtension")) : 0;
        id ep = ext ? MSG(id, ext, "objectForKey:", str("NSExtensionPointIdentifier")) : 0;
        if (!ep || strcmp(cx_cstr(ep), point)) continue;
        id attrs = MSG(id, ext, "objectForKey:", str("NSExtensionAttributes"));
        id types = attrs ? MSG(id, attrs, "objectForKey:", str("QLSupportedContentTypes")) : 0;
        i64 tn = types ? MSG0(i64, types, "count") : 0;
        for (i64 k = 0; k < tn; k++)
            if (uti && UTTypeConformsTo(uti, at(types, k))) return b;
    }
    return 0;
}
static id appex_instance(id bundle) {
    if (!bundle || !MSG0(BOOL, bundle, "load")) return 0;
    id ext = MSG(id, bundle, "objectForInfoDictionaryKey:", str("NSExtension"));
    id pc = ext ? MSG(id, ext, "objectForKey:", str("NSExtensionPrincipalClass")) : 0;
    cx_class_t *k = pc ? kls(cx_cstr(pc)) : 0;
    if (!k) { id cls = MSG0(id, bundle, "principalClass"); k = (cx_class_t *)cls; }
    return k ? MSG0(id, MSG0(id, (id)k, "alloc"), "init") : 0;
}

/* ---- QLPreviewReply / QLPreviewReplyAttachment / QLFilePreviewRequest */
static char k_reply_kind, k_reply_type, k_reply_size, k_reply_block, k_reply_url, k_reply_enc, k_reply_att, k_reply_title, k_req_url,
    k_att_data, k_att_type;
static id rp_init_data(id self, SEL c, id type, S size, void *blk) {
    (void)c;
    aset(self, &k_reply_kind, str("data"));
    aset(self, &k_reply_type, type);
    aset(self, &k_reply_size, val_size(size));
    if (blk) aset(self, &k_reply_block, (id)_Block_copy(blk));
    return self;
}
static id rp_init_file(id self, SEL c, id url) { (void)c; aset(self, &k_reply_kind, str("file")); aset(self, &k_reply_url, url); return self; }
static id rp_init_pdf(id self, SEL c, S size, void *blk) {
    (void)c;
    aset(self, &k_reply_kind, str("pdf"));
    aset(self, &k_reply_size, val_size(size));
    if (blk) aset(self, &k_reply_block, (id)_Block_copy(blk));
    return self;
}
static u64 rp_enc(id self, SEL c) { (void)c; id e = aget(self, &k_reply_enc); return e ? MSG0(u64, e, "unsignedLongLongValue") : 4; }
static void rp_set_enc(id self, SEL c, u64 v) { (void)c; aset(self, &k_reply_enc, num_ull(v)); }
static id rp_att(id self, SEL c) { (void)c; id a = aget(self, &k_reply_att); return a ? a : MSG0(id, (id)kls("NSDictionary"), "dictionary"); }
static void rp_set_att(id self, SEL c, id v) { (void)c; aset(self, &k_reply_att, v); }
static id rp_title(id self, SEL c) { (void)c; return aget(self, &k_reply_title); }
static void rp_set_title(id self, SEL c, id v) { (void)c; aset(self, &k_reply_title, v); }
static id att_init(id self, SEL c, id data, id type) { (void)c; aset(self, &k_att_data, data); aset(self, &k_att_type, type); return self; }
static id att_data(id self, SEL c) { (void)c; return aget(self, &k_att_data); }
static id att_type(id self, SEL c) { (void)c; return aget(self, &k_att_type); }
static id rq_url(id self, SEL c) { (void)c; return aget(self, &k_req_url); }
static void provider_default(id self, SEL c, id req, void *blk) {
    (void)self; (void)c; (void)req;
    if (!blk) return;
    id err = MSG(id, (id)kls("NSError"), "errorWithDomain:code:userInfo:", str("QLPreviewErrorDomain"), (i64)1, (id)0);
    ((void (*)(void *, id, id))BLK_FN(blk))(blk, (id)0, err);
}
/* resposta da extensão → conteúdo */
static int content_from_reply(ql_content *c, id reply) {
    if (!reply) return 0;
    id kind = aget(reply, &k_reply_kind);
    const char *k = kind ? cx_cstr(kind) : "";
    void *blk = aget(reply, &k_reply_block);
    c->title = keep(aget(reply, &k_reply_title));
    if (!strcmp(k, "file")) return content_from_file(c, aget(reply, &k_reply_url), uti_of(aget(reply, &k_reply_url)));
    if (!strcmp(k, "data") && blk) {
        id err = 0;
        id data = ((id (*)(void *, id, id *))BLK_FN(blk))(blk, reply, &err);
        id t = aget(reply, &k_reply_type);
        id ident = t ? (responds(t, "identifier") ? MSG0(id, t, "identifier") : t) : 0;
        id props = dict();
        MSG(void, props, "setObject:forKey:", num_ull(rp_enc(reply, 0)), KEY(kQLPreviewPropertyStringEncodingKey));
        int ok = content_from_data(c, data, ident, props);
        objc_release(props);
        return ok;
    }
    if (!strcmp(k, "pdf") && blk) {
        id err = 0;
        id doc = ((id (*)(void *, id, id *))BLK_FN(blk))(blk, reply, &err);
        id data = doc && responds(doc, "dataRepresentation") ? MSG0(id, doc, "dataRepresentation") : 0;
        if (data) { c->kind = QC_PDF; c->data = keep(data); return 1; }
    }
    return 0;
}

/* ---- geradores antigos (.qlgenerator) pelo CFPlugIn */
typedef struct { u8 b[16]; } uuid_bytes;
id CFUUIDGetConstantUUIDWithBytes(void *alloc, u8 b0, u8 b1, u8 b2, u8 b3, u8 b4, u8 b5, u8 b6, u8 b7, u8 b8, u8 b9, u8 b10, u8 b11, u8 b12, u8 b13, u8 b14, u8 b15);
uuid_bytes CFUUIDGetUUIDBytes(id u);
id CFPlugInCreate(void *alloc, id url);
id CFPlugInFindFactoriesForPlugInTypeInPlugIn(id type, id plugin);
void *CFPlugInInstanceCreate(void *alloc, id factory, id type);
typedef struct {
    void *reserved;
    i32 (*query)(void *self, uuid_bytes iid, void **out);
    u32 (*add_ref)(void *self);
    u32 (*release)(void *self);
    i32 (*thumbnail)(void *self, id req, id url, id uti, id options, S max);
    void (*cancel_thumbnail)(void *self, id req);
    i32 (*preview)(void *self, id req, id url, id uti, id options);
    void (*cancel_preview)(void *self, id req);
} ql_gen_vtbl;
static id gen_type_id(void) { return CFUUIDGetConstantUUIDWithBytes(0, 0x5E, 0x2D, 0x96, 0x80, 0x50, 0x22, 0x40, 0xFA, 0xB8, 0x06, 0x43, 0x34, 0x96, 0x22, 0xE5, 0xB9); }
static id gen_iface_id(void) { return CFUUIDGetConstantUUIDWithBytes(0, 0x86, 0x5A, 0xF5, 0xE0, 0x6D, 0x30, 0x43, 0x45, 0x95, 0x1B, 0xD3, 0x71, 0x05, 0x75, 0x4F, 0x2D); }
static id find_generator(id uti) {
    if (!uti) return 0;
    id dirs[3] = {0, 0, 0};
    id mb = MSG0(id, (id)kls("NSBundle"), "mainBundle");
    dirs[0] = MSG(id, MSG0(id, mb, "bundlePath"), "stringByAppendingPathComponent:", str("Contents/Library/QuickLook"));
    extern id NSHomeDirectory(void);
    dirs[1] = MSG(id, NSHomeDirectory(), "stringByAppendingPathComponent:", str("Library/QuickLook"));
    dirs[2] = str("/Library/QuickLook");
    id fm = MSG0(id, (id)kls("NSFileManager"), "defaultManager");
    for (int d = 0; d < 3; d++) {
        id items = ((id (*)(id, SEL, id, id *))objc_msgSend)(fm, sl("contentsOfDirectoryAtPath:error:"), dirs[d], (id *)0);
        i64 n = items ? MSG0(i64, items, "count") : 0;
        for (i64 i = 0; i < n; i++) {
            id name = at(items, i);
            if (!MSG(BOOL, MSG0(id, name, "pathExtension"), "isEqualToString:", str("qlgenerator"))) continue;
            id path = MSG(id, dirs[d], "stringByAppendingPathComponent:", name);
            id b = MSG(id, (id)kls("NSBundle"), "bundleWithPath:", path);
            id types = b ? MSG(id, b, "objectForInfoDictionaryKey:", str("CFBundleDocumentTypes")) : 0;
            i64 tn = types ? MSG0(i64, types, "count") : 0;
            for (i64 k = 0; k < tn; k++) {
                id cts = MSG(id, at(types, k), "objectForKey:", str("LSItemContentTypes"));
                i64 cn = cts ? MSG0(i64, cts, "count") : 0;
                for (i64 j = 0; j < cn; j++)
                    if (UTTypeConformsTo(uti, at(cts, j))) return MSG(id, (id)kls("NSURL"), "fileURLWithPath:", path);
            }
        }
    }
    return 0;
}
/* instância do gerador (a interface de callbacks) ou 0 */
static ql_gen_vtbl **gen_open(id gen_url, id *bundle_out) {
    id plugin = CFPlugInCreate(0, gen_url);
    if (!plugin) return 0;
    if (bundle_out) *bundle_out = plugin;
    id facs = CFPlugInFindFactoriesForPlugInTypeInPlugIn(gen_type_id(), plugin);
    if (!facs || !MSG0(i64, facs, "count")) return 0;
    ql_gen_vtbl **inst = CFPlugInInstanceCreate(0, at(facs, (i64)0), gen_type_id());
    if (!inst) return 0;
    void *iface = 0;
    if ((*inst)->query && (*inst)->query(inst, CFUUIDGetUUIDBytes(gen_iface_id()), &iface) == 0 && iface) {
        (*inst)->release(inst);
        return iface;
    }
    return inst;   /* o objeto da fábrica já é a interface (como no modelo do Xcode) */
}

/* ---- QLPreviewRequest / QLThumbnailRequest (objetos _MOSQLRequest) */
typedef struct {
    id url, uti, options, bundle;
    S max;
    int cancelled, thumb;
    id out_data, out_uti, out_props, out_url;
    void *out_image;
    id ctx; int ctx_pdf; id ctx_data, ctx_consumer;
} qlreq;
static char k_req;
static qlreq *RQ(id o) {
    id box = aget(o, &k_req);
    return box ? (qlreq *)MSG0(void *, box, "pointerValue") : 0;
}
static id req_new(id url, id uti, id options, id bundle, S max, int thumb) {
    id o = MSG0(id, MSG0(id, (id)&_MOSQLRequest_c, "alloc"), "init");
    qlreq *r = calloc(1, sizeof *r);
    if (!o || !r) { free(r); return 0; }
    r->url = keep(url); r->uti = keep(uti); r->options = keep(options); r->bundle = keep(bundle); r->max = max; r->thumb = thumb;
    aset(o, &k_req, val_ptr((void *)r));
    return o;
}
static void req_free(id o) {
    qlreq *r = RQ(o);
    if (!r) return;
    id objs[] = {r->url, r->uti, r->options, r->bundle, r->out_data, r->out_uti, r->out_props, r->out_url, r->ctx_data};
    for (unsigned i = 0; i < sizeof objs / sizeof objs[0]; i++) if (objs[i]) objc_release(objs[i]);
    if (r->out_image) CGImageRelease(r->out_image);
    free(r);
    aset(o, &k_req, 0);
    objc_release(o);
}
id QLPreviewRequestGetGeneratorBundle(id req) { qlreq *r = RQ(req); return r ? r->bundle : 0; }
id QLThumbnailRequestGetGeneratorBundle(id req) { return QLPreviewRequestGetGeneratorBundle(req); }
u8 QLPreviewRequestIsCancelled(id req) { qlreq *r = RQ(req); return r ? (u8)r->cancelled : 1; }
u8 QLThumbnailRequestIsCancelled(id req) { return QLPreviewRequestIsCancelled(req); }
id QLPreviewRequestCopyURL(id req) { qlreq *r = RQ(req); return r ? keep(r->url) : 0; }
id QLThumbnailRequestCopyURL(id req) { return QLPreviewRequestCopyURL(req); }
id QLPreviewRequestCopyContentUTI(id req) { qlreq *r = RQ(req); return r ? keep(r->uti) : 0; }
id QLThumbnailRequestCopyContentUTI(id req) { return QLPreviewRequestCopyContentUTI(req); }
id QLPreviewRequestCopyOptions(id req) { qlreq *r = RQ(req); return r && r->options ? keep(r->options) : keep(MSG0(id, (id)kls("NSDictionary"), "dictionary")); }
id QLThumbnailRequestCopyOptions(id req) { return QLPreviewRequestCopyOptions(req); }
S QLThumbnailRequestGetMaximumSize(id req) { qlreq *r = RQ(req); S z = {0, 0}; return r ? r->max : z; }
void QLPreviewRequestSetDataRepresentation(id req, id data, id uti, id props) {
    qlreq *r = RQ(req);
    if (!r || r->cancelled) return;
    if (r->out_data) objc_release(r->out_data);
    if (r->out_uti) objc_release(r->out_uti);
    if (r->out_props) objc_release(r->out_props);
    r->out_data = keep(data); r->out_uti = keep(uti); r->out_props = keep(props);
}
void QLPreviewRequestSetURLRepresentation(id req, id url, id uti, id props) {
    qlreq *r = RQ(req);
    if (!r || r->cancelled) return;
    if (r->out_url) objc_release(r->out_url);
    r->out_url = keep(url);
    QLPreviewRequestSetDataRepresentation(req, 0, uti, props);
}
/* contexto de desenho: bitmap (isBitmap) ou PDF; o Flush transforma no resultado */
static id req_context(id req, S size, u8 bitmap) {
    qlreq *r = RQ(req);
    if (!r || size.w < 1 || size.h < 1) return 0;
    if (r->ctx) return r->ctx;
    if (bitmap || r->thumb) {
        void *space = CGColorSpaceCreateDeviceRGB();
        r->ctx = CGBitmapContextCreate(0, (size_t)size.w, (size_t)size.h, 8, (size_t)size.w * 4, space, 2u | (2u << 12));
        CGColorSpaceRelease(space);
        r->ctx_pdf = 0;
    } else {
        r->ctx_data = MSG0(id, MSG0(id, (id)kls("NSMutableData"), "alloc"), "init");
        r->ctx_consumer = CGDataConsumerCreateWithCFData(r->ctx_data);
        R media = {0, 0, size.w, size.h};
        r->ctx = r->ctx_consumer ? CGPDFContextCreate(r->ctx_consumer, &media, 0) : 0;
        if (r->ctx) CGPDFContextBeginPage(r->ctx, 0);
        r->ctx_pdf = 1;
    }
    return r->ctx;
}
id QLPreviewRequestCreateContext(id req, S size, u8 bitmap, id props) { (void)props; id c = req_context(req, size, bitmap); return c ? objc_retain(c) : 0; }
id QLPreviewRequestCreatePDFContext(id req, const R *media, id aux, id props) {
    (void)aux; (void)props;
    S z = {media ? media->w : 612, media ? media->h : 792};
    id c = req_context(req, z, 0);
    return c ? objc_retain(c) : 0;
}
id QLThumbnailRequestCreateContext(id req, S size, u8 bitmap, id props) { (void)props; id c = req_context(req, size, bitmap ? bitmap : 1); return c ? objc_retain(c) : 0; }
static void req_flush(id req, id ctx) {
    qlreq *r = RQ(req);
    if (!r || !ctx || ctx != r->ctx) return;
    if (r->ctx_pdf) {
        CGPDFContextEndPage(ctx);
        CGPDFContextClose(ctx);
        QLPreviewRequestSetDataRepresentation(req, r->ctx_data, str("com.adobe.pdf"), 0);
        if (r->ctx_consumer) CGDataConsumerRelease(r->ctx_consumer);
        r->ctx_consumer = 0;
    } else {
        void *img = CGBitmapContextCreateImage(ctx);
        if (r->thumb) { if (r->out_image) CGImageRelease(r->out_image); r->out_image = img; }
        else if (img) {
            id ni = ((id (*)(id, SEL, void *, S))objc_msgSend)(MSG0(id, (id)kls("NSImage"), "alloc"), sl("initWithCGImage:size:"), img, (S){0, 0});
            id tiff = ni ? MSG0(id, ni, "TIFFRepresentation") : 0;
            if (tiff) QLPreviewRequestSetDataRepresentation(req, tiff, str("public.tiff"), 0);
            if (ni) objc_release(ni);
            CGImageRelease(img);
        }
    }
    r->ctx = 0;
}
void QLPreviewRequestFlushContext(id req, id ctx) { req_flush(req, ctx); }
void QLThumbnailRequestFlushContext(id req, id ctx) { req_flush(req, ctx); }
void QLThumbnailRequestSetImage(id req, void *img, id props) {
    (void)props;
    qlreq *r = RQ(req);
    if (!r || !img) return;
    if (r->out_image) CGImageRelease(r->out_image);
    r->out_image = CGImageRetain(img);
}
static void *nsimage_cg(id img) { return img ? MSG(void *, img, "CGImageForProposedRect:context:hints:", (void *)0, (id)0, (id)0) : 0; }
void QLThumbnailRequestSetImageWithData(id req, id data, id props) {
    id img = data ? MSG(id, MSG0(id, (id)kls("NSImage"), "alloc"), "initWithData:", data) : 0;
    if (img) { QLThumbnailRequestSetImage(req, nsimage_cg(img), props); objc_release(img); }
}
void QLThumbnailRequestSetImageAtURL(id req, id url, id props) { QLThumbnailRequestSetImageWithData(req, url ? MSG(id, (id)kls("NSData"), "dataWithContentsOfURL:", url) : 0, props); }

/* gera uma pré-visualização pelo gerador; o conteúdo vem do que ele preencheu */
static int content_from_generator(ql_content *c, id url, id uti) {
    id gurl = find_generator(uti);
    if (!gurl) return 0;
    id bundle = 0;
    ql_gen_vtbl **g = gen_open(gurl, &bundle);
    if (!g || !(*g)->preview) return 0;
    id req = req_new(url, uti, 0, bundle, (S){0, 0}, 0);
    int ok = 0;
    if (req && (*g)->preview(g, req, url, uti, 0) == 0) {
        qlreq *r = RQ(req);
        if (r->out_url) ok = content_from_file(c, r->out_url, r->out_uti ? r->out_uti : uti_of(r->out_url));
        else ok = content_from_data(c, r->out_data, r->out_uti, r->out_props);
        id name = r->out_props ? MSG(id, r->out_props, "objectForKey:", KEY(kQLPreviewPropertyDisplayNameKey)) : 0;
        if (ok && name) c->title = keep(name);
    }
    if (req) req_free(req);
    if ((*g)->release) (*g)->release(g);
    return ok;
}
static int content_from_extension(ql_content *c, id url, id uti) {
    id b = find_appex("com.apple.quicklook.preview", uti);
    id prov = b ? appex_instance(b) : 0;
    if (!prov || !responds(prov, "providePreviewForFileRequest:completionHandler:")) { if (prov) objc_release(prov); return 0; }
    id req = MSG0(id, MSG0(id, (id)&QLFilePreviewRequest_c, "alloc"), "init");
    aset(req, &k_req_url, url);
    done_blk *d = new_done();
    if (!d) { objc_release(prov); objc_release(req); return 0; }
    MSG(void, prov, "providePreviewForFileRequest:completionHandler:", req, (void *)d);
    wait_done(d, 10);
    int ok = d->done && content_from_reply(c, d->result);
    if (d->result) objc_release(d->result);
    if (d->error) objc_release(d->error);
    if (d->done) free(d);   /* sem resposta, o bloco fica: a extensão ainda pode chamá-lo */
    objc_release(req);
    objc_release(prov);
    return ok;
}
static void preview_content(ql_content *c, id item) {
    memset(c, 0, sizeof *c);
    id url = responds(item, "previewItemURL") ? MSG0(id, item, "previewItemURL") : (item && responds(item, "isFileURL") ? item : 0);
    id title = responds(item, "previewItemTitle") ? MSG0(id, item, "previewItemTitle") : 0;
    if (url) {
        id uti = uti_of(url);
        if (!content_from_extension(c, url, uti) && !content_from_generator(c, url, uti) && !content_from_file(c, url, uti)) {
            c->kind = QC_ICON;
            c->url = keep(url);
        }
    }
    if (title && !c->title) c->title = keep(title);
    if (!c->title && url) c->title = keep(MSG0(id, url, "lastPathComponent"));
}

/* ================================================================ QLPreviewView */
static char k_item, k_content, k_style, k_auto, k_shouldclose, k_state;
static id mkview(const char *cls, R f) { return ((id (*)(id, SEL, R))objc_msgSend)(MSG0(id, (id)kls(cls), "alloc"), sl("initWithFrame:"), f); }
static void clear_sub(id self) {
    id subs = MSG0(id, MSG0(id, self, "subviews"), "copy");
    i64 n = subs ? MSG0(i64, subs, "count") : 0;
    for (i64 i = 0; i < n; i++) MSG0(void, at(subs, i), "removeFromSuperview");
    if (subs) objc_release(subs);
}
static void add_fill(id self, id v) {
    if (!v) return;
    MSG(void, v, "setAutoresizingMask:", (u64)(2 | 16));   /* largura e altura acompanham */
    MSG(void, self, "addSubview:", v);
    objc_release(v);
}
static id label(const char *s, double size, BOOL bold, R f) {
    id t = mkview("NSTextField", f);
    MSG(void, t, "setStringValue:", str(s));
    MSG(void, t, "setEditable:", (BOOL)NO); MSG(void, t, "setBordered:", (BOOL)NO); MSG(void, t, "setDrawsBackground:", (BOOL)NO);
    MSG(void, t, "setAlignment:", (i64)2);   /* centro */
    MSG(void, t, "setFont:", font_msg(bold ? "boldSystemFontOfSize:" : "systemFontOfSize:", size));
    return t;
}
static void show_icon_info(id self, R b, id url) {
    id path = url ? MSG0(id, url, "path") : 0;
    id ws = MSG0(id, (id)kls("NSWorkspace"), "sharedWorkspace");
    id icon = path ? MSG(id, ws, "iconForFile:", path) : 0;
    double cy = b.h / 2;
    id iv = mkview("NSImageView", (R){b.w / 2 - 64, cy, 128, 128});
    if (icon) { ((void (*)(id, SEL, S))objc_msgSend)(icon, sl("setSize:"), (S){128, 128}); MSG(void, iv, "setImage:", icon); }
    MSG(void, iv, "setAutoresizingMask:", (u64)(1 | 4 | 8 | 32));
    MSG(void, self, "addSubview:", iv); objc_release(iv);
    const char *name = path ? cx_cstr(MSG0(id, path, "lastPathComponent")) : "";
    id l = label(name, 15, YES, (R){10, cy - 30, b.w - 20, 22});
    MSG(void, l, "setAutoresizingMask:", (u64)(2 | 8 | 32)); MSG(void, self, "addSubview:", l); objc_release(l);
    /* tipo, tamanho e data */
    id fm = MSG0(id, (id)kls("NSFileManager"), "defaultManager");
    id attrs = path ? ((id (*)(id, SEL, id, id *))objc_msgSend)(fm, sl("attributesOfItemAtPath:error:"), path, (id *)0) : 0;
    id size = attrs ? MSG(id, attrs, "objectForKey:", str("NSFileSize")) : 0;
    id kind = MSG(id, ws, "localizedDescriptionForType:", uti_of(url));
    char info[256];
    unsigned long long sz = size ? MSG0(u64, size, "unsignedLongLongValue") : 0;
    const char *unit = "bytes";
    double v = (double)sz;
    if (sz >= 1000000000ull) { v /= 1e9; unit = "GB"; } else if (sz >= 1000000) { v /= 1e6; unit = "MB"; } else if (sz >= 1000) { v /= 1e3; unit = "KB"; }
    if (!strcmp(unit, "bytes")) snprintf(info, sizeof info, "%s — %llu bytes", kind ? cx_cstr(kind) : "Documento", sz);
    else snprintf(info, sizeof info, "%s — %.1f %s", kind ? cx_cstr(kind) : "Documento", v, unit);
    id l2 = label(info, 12, NO, (R){10, cy - 54, b.w - 20, 18});
    MSG(void, l2, "setTextColor:", cx_color(0.45, 0.45, 0.47, 1));
    MSG(void, l2, "setAutoresizingMask:", (u64)(2 | 8 | 32)); MSG(void, self, "addSubview:", l2); objc_release(l2);
}
static void show_text(id self, R b, ql_content *c) {
    id sv = mkview("NSScrollView", b);
    MSG(void, sv, "setHasVerticalScroller:", (BOOL)YES);
    id tv = mkview("NSTextView", (R){0, 0, b.w, b.h});
    MSG(void, tv, "setEditable:", (BOOL)NO);
    MSG(void, tv, "setSelectable:", (BOOL)YES);
    if (c->kind == QC_ATTR) MSG(void, MSG0(id, tv, "textStorage"), "setAttributedString:", c->text);
    else {
        MSG(void, tv, "setFont:", font_msg("userFixedPitchFontOfSize:", 12.0));
        MSG(void, tv, "setString:", c->text ? c->text : str(""));
    }
    MSG(void, sv, "setDocumentView:", tv);
    objc_release(tv);
    add_fill(self, sv);
}
static void load_webkit(void) {
    if (kls("WKWebView")) return;
    id b = MSG(id, (id)kls("NSBundle"), "bundleWithPath:", str("/System/Library/Frameworks/WebKit.framework"));
    if (b) MSG0(BOOL, b, "load");
}
static void qv_show(id self) {
    clear_sub(self);
    id box = aget(self, &k_content);
    ql_content *c = box ? MSG0(void *, box, "pointerValue") : 0;
    R b = ((R (*)(id, SEL))objc_msgSend)(self, sl("bounds"));
    if (!c || c->kind == QC_NONE) return;
    switch (c->kind) {
    case QC_IMAGE: {
        id iv = mkview("NSImageView", b);
        MSG(void, iv, "setImageScaling:", (u64)3);   /* proporcional, só reduz */
        MSG(void, iv, "setImage:", c->image);
        add_fill(self, iv);
        break;
    }
    case QC_PDF: {
        id doc = kls("PDFDocument") ? MSG(id, MSG0(id, (id)kls("PDFDocument"), "alloc"), "initWithData:", c->data) : 0;
        if (doc && kls("PDFView")) {
            id pv = mkview("PDFView", b);
            MSG(void, pv, "setDocument:", doc);
            if (responds(pv, "setAutoScales:")) MSG(void, pv, "setAutoScales:", (BOOL)YES);
            add_fill(self, pv);
        } else {
            id img = MSG(id, MSG0(id, (id)kls("NSImage"), "alloc"), "initWithData:", c->data);
            id iv = mkview("NSImageView", b);
            MSG(void, iv, "setImageScaling:", (u64)3);
            MSG(void, iv, "setImage:", img);
            if (img) objc_release(img);
            add_fill(self, iv);
        }
        if (doc) objc_release(doc);
        break;
    }
    case QC_HTML: {
        load_webkit();
        if (kls("WKWebView")) {
            id wv = mkview("WKWebView", b);
            MSG(void, wv, "loadHTMLString:baseURL:", c->text ? c->text : str(""), c->url ? MSG0(id, c->url, "URLByDeletingLastPathComponent") : (id)0);
            add_fill(self, wv);
            break;
        }
        c->kind = QC_TEXT;   /* sem WebKit: o código-fonte */
        show_text(self, b, c);
        break;
    }
    case QC_TEXT: case QC_ATTR: show_text(self, b, c); break;
    case QC_MEDIA: {
        id player = kls("AVPlayer") ? MSG(id, (id)kls("AVPlayer"), "playerWithURL:", c->url) : 0;
        if (player && kls("AVPlayerView")) {
            id pv = mkview("AVPlayerView", b);
            MSG(void, pv, "setPlayer:", player);
            id au = aget(self, &k_auto);
            if (au && MSG0(BOOL, au, "boolValue")) MSG0(void, player, "play");
            add_fill(self, pv);
        } else show_icon_info(self, b, c->url);
        break;
    }
    default: show_icon_info(self, b, c->url); break;
    }
}
static void qv_load(id self) {
    id old = aget(self, &k_content);
    if (old) { ql_content *c = MSG0(void *, old, "pointerValue"); qc_free(c); free(c); aset(self, &k_content, 0); }
    id item = aget(self, &k_item);
    if (!item) { clear_sub(self); return; }
    ql_content *c = calloc(1, sizeof *c);
    if (!c) return;
    preview_content(c, item);
    aset(self, &k_content, val_ptr((void *)c));
    qv_show(self);
}
static ql_content *qv_content(id self) { id b = aget(self, &k_content); return b ? MSG0(void *, b, "pointerValue") : 0; }
static id qv_init_style(id self, SEL c, R f, i64 style) {
    (void)c;
    IMP sup = cx_lookup(QLPreviewView_c.super, sl("initWithFrame:"));
    self = ((id (*)(id, SEL, R))sup)(self, sl("initWithFrame:"), f);
    if (self) aset(self, &k_style, num_ll(style));
    return self;
}
static id qv_init_frame(id self, SEL c, R f) { return qv_init_style(self, c, f, 0); }
static id qv_item(id self, SEL c) { (void)c; return aget(self, &k_item); }
static void qv_set_item(id self, SEL c, id item) { (void)c; aset(self, &k_item, item); qv_load(self); }
static void qv_refresh(id self, SEL c) { (void)c; qv_load(self); }
static void qv_close(id self, SEL c) { (void)c; aset(self, &k_item, 0); qv_load(self); }
static BOOL qv_auto(id self, SEL c) { (void)c; id v = aget(self, &k_auto); return v && MSG0(BOOL, v, "boolValue"); }
static void qv_set_auto(id self, SEL c, BOOL v) { (void)c; aset(self, &k_auto, num_bool(v)); }
static BOOL qv_shouldclose(id self, SEL c) { (void)c; id v = aget(self, &k_shouldclose); return v ? MSG0(BOOL, v, "boolValue") : YES; }
static void qv_set_shouldclose(id self, SEL c, BOOL v) { (void)c; aset(self, &k_shouldclose, num_bool(v)); }
static id qv_state(id self, SEL c) { (void)c; return aget(self, &k_state); }
static void qv_set_state(id self, SEL c, id v) { (void)c; aset(self, &k_state, v); }
static void qv_resize(id self, SEL c, S old) {
    (void)c;
    IMP sup = cx_lookup(QLPreviewView_c.super, sl("resizeSubviewsWithOldSize:"));
    if (sup) ((void (*)(id, SEL, S))sup)(self, sl("resizeSubviewsWithOldSize:"), old);
}

/* ================================================================ QLPreviewPanel */
static id g_panel;
static char k_ds, k_del, k_index, k_ctrl, k_view, k_pstate;
static id pv_of(id panel) { return aget(panel, &k_view); }
static i64 p_count(id self) {
    id ds = aget(self, &k_ds);
    return ds && responds(ds, "numberOfPreviewItemsInPreviewPanel:") ? MSG(i64, ds, "numberOfPreviewItemsInPreviewPanel:", self) : 0;
}
static i64 p_index(id self) { id v = aget(self, &k_index); return v ? MSG0(i64, v, "longLongValue") : 0; }
static id p_item_at(id self, i64 i) {
    id ds = aget(self, &k_ds);
    if (!ds || i < 0 || i >= p_count(self)) return 0;
    if (responds(ds, "previewPanel:previewItemAtIndex:")) return MSG(id, ds, "previewPanel:previewItemAtIndex:", self, i);
    return 0;
}
static void p_show(id self) {
    i64 n = p_count(self), i = p_index(self);
    if (i >= n) i = n ? n - 1 : 0;
    if (i < 0) i = 0;
    aset(self, &k_index, num_ll(i));
    id item = p_item_at(self, i);
    MSG(void, pv_of(self), "setPreviewItem:", item);
    ql_content *c = qv_content(pv_of(self));
    const char *t = c && c->title ? cx_cstr(c->title) : "Quick Look";
    char title[300];
    if (n > 1) snprintf(title, sizeof title, "%s (%lld de %lld)", t, (long long)i + 1, (long long)n);
    else snprintf(title, sizeof title, "%s", t);
    MSG(void, self, "setTitle:", str(title));
}
static id ql_shared(id cls, SEL c) {
    (void)cls; (void)c;
    if (g_panel) return g_panel;
    id p = ((id (*)(id, SEL, R, u64, u64, BOOL))objc_msgSend)(MSG0(id, (id)&QLPreviewPanel_c, "alloc"),
        sl("initWithContentRect:styleMask:backing:defer:"), (R){0, 0, 720, 520}, (u64)(1 | 2 | 8), (u64)2, NO);
    if (!p) return 0;
    MSG(void, p, "setReleasedWhenClosed:", (BOOL)NO);
    MSG(void, p, "setTitle:", str("Quick Look"));
    if (responds(p, "setFloatingPanel:")) MSG(void, p, "setFloatingPanel:", (BOOL)YES);
    id v = ((id (*)(id, SEL, R, i64))objc_msgSend)(MSG0(id, (id)&QLPreviewView_c, "alloc"), sl("initWithFrame:style:"), (R){0, 0, 720, 520}, (i64)0);
    MSG(void, p, "setContentView:", v);
    aset(p, &k_view, v);
    objc_release(v);
    MSG0(void, p, "center");
    g_panel = p;
    return p;
}
static BOOL ql_exists(id cls, SEL c) { (void)cls; (void)c; return g_panel != 0; }
static void ql_set_ds(id self, SEL c, id ds) { (void)c; objc_setAssociatedObject(self, &k_ds, ds, 0); }
static id ql_ds(id self, SEL c) { (void)c; return aget(self, &k_ds); }
static void ql_set_del(id self, SEL c, id d) { (void)c; objc_setAssociatedObject(self, &k_del, d, 0); }
static id ql_del(id self, SEL c) { (void)c; return aget(self, &k_del); }
static void ql_set_index(id self, SEL c, i64 i) { (void)c; aset(self, &k_index, num_ll(i < 0 ? 0 : i)); if (MSG0(BOOL, self, "isVisible")) p_show(self); }
static i64 ql_index(id self, SEL c) { (void)c; return p_index(self); }
static id ql_current(id self, SEL c) { (void)c; return p_item_at(self, p_index(self)); }
static void ql_reload(id self, SEL c) { (void)c; p_show(self); }
static void ql_refresh(id self, SEL c) { (void)c; MSG0(void, pv_of(self), "refreshPreviewItem"); }
static id ql_controller(id self, SEL c) { (void)c; return aget(self, &k_ctrl); }
/* o controlador é o primeiro da cadeia de respondedores (janela principal, depois a chave, e o NSApp) que aceita */
static void ql_update(id self, SEL c) {
    (void)c;
    extern id NSApp;
    id wins[2] = {MSG0(id, NSApp, "mainWindow"), MSG0(id, NSApp, "keyWindow")};
    id found = 0;
    for (int w = 0; w < 2 && !found; w++) {
        if (!wins[w] || wins[w] == self) continue;
        for (id r = MSG0(id, wins[w], "firstResponder"); r && !found; r = responds(r, "nextResponder") ? MSG0(id, r, "nextResponder") : 0)
            if (responds(r, "acceptsPreviewPanelControl:") && MSG(BOOL, r, "acceptsPreviewPanelControl:", self)) found = r;
    }
    if (!found) {
        id dl = MSG0(id, NSApp, "delegate");
        if (responds(dl, "acceptsPreviewPanelControl:") && MSG(BOOL, dl, "acceptsPreviewPanelControl:", self)) found = dl;
    }
    id old = aget(self, &k_ctrl);
    if (old == found) return;
    if (old && responds(old, "endPreviewPanelControl:")) MSG(void, old, "endPreviewPanelControl:", self);
    objc_setAssociatedObject(self, &k_ctrl, found, 0);
    if (found && responds(found, "beginPreviewPanelControl:")) MSG(void, found, "beginPreviewPanelControl:", self);
}
static void super_call(id self, const char *sel, id arg) {
    IMP sup = cx_lookup(QLPreviewPanel_c.super, sl(sel));
    if (sup) ((void (*)(id, SEL, id))sup)(self, sl(sel), arg);
}
static void ql_front(id self, SEL c, id sender) { (void)c; ql_update(self, 0); p_show(self); super_call(self, "makeKeyAndOrderFront:", sender); }
static void ql_order_front(id self, SEL c, id sender) { (void)c; ql_update(self, 0); p_show(self); super_call(self, "orderFront:", sender); }
static void ql_end_control(id self) {
    id old = aget(self, &k_ctrl);
    if (old && responds(old, "endPreviewPanelControl:")) MSG(void, old, "endPreviewPanelControl:", self);
    objc_setAssociatedObject(self, &k_ctrl, 0, 0);
    MSG0(void, pv_of(self), "close");
}
static void ql_out(id self, SEL c, id sender) { (void)c; super_call(self, "orderOut:", sender); ql_end_control(self); }
static void ql_close(id self, SEL c) {
    (void)c;
    IMP sup = cx_lookup(QLPreviewPanel_c.super, sl("close"));
    if (sup) ((void (*)(id, SEL))sup)(self, sl("close"));
    ql_end_control(self);
}
static void ql_key(id self, SEL c, id ev) {
    (void)c;
    id dl = aget(self, &k_del);
    if (responds(dl, "previewPanel:handleEvent:") && MSG(BOOL, dl, "previewPanel:handleEvent:", self, ev)) return;
    id chars = MSG0(id, ev, "charactersIgnoringModifiers");
    const char *s = chars ? cx_cstr(chars) : "";
    u16 code = MSG0(u16, ev, "keyCode");
    i64 i = p_index(self), n = p_count(self);
    if (code == 123 || code == 126) { if (i > 0) ql_set_index(self, 0, i - 1); return; }        /* ← ↑ */
    if (code == 124 || code == 125) { if (i + 1 < n) ql_set_index(self, 0, i + 1); return; }    /* → ↓ */
    if (code == 49 || code == 53 || (s[0] == ' ' && !s[1])) { ql_out(self, 0, 0); return; }     /* espaço, esc */
    IMP sup = cx_lookup(QLPreviewPanel_c.super, sl("keyDown:"));
    if (sup) ((void (*)(id, SEL, id))sup)(self, sl("keyDown:"), ev);
}
static BOOL ql_enter_fs(id self, SEL c, id sender, id opts) {
    (void)c; (void)sender; (void)opts;
    if (!MSG0(BOOL, self, "isVisible")) ql_front(self, 0, 0);
    if (responds(self, "toggleFullScreen:") && !((MSG0(u64, self, "styleMask") >> 14) & 1)) MSG(void, self, "toggleFullScreen:", (id)0);
    return YES;
}
static void ql_exit_fs(id self, SEL c, id opts) { (void)c; (void)opts; if (((MSG0(u64, self, "styleMask") >> 14) & 1) && responds(self, "toggleFullScreen:")) MSG(void, self, "toggleFullScreen:", (id)0); }
static BOOL ql_in_fs(id self, SEL c) { (void)c; return (MSG0(u64, self, "styleMask") >> 14) & 1; }
static id ql_state(id self, SEL c) { (void)c; return aget(self, &k_pstate); }
static void ql_set_state(id self, SEL c, id v) { (void)c; aset(self, &k_pstate, v); }

/* ================================================================ miniaturas */
/* imagem do conteúdo (imagem ou primeira página do PDF) reduzida para caber em max */
static void *fit_image(id img, S max) {
    if (!img) return 0;
    S z = ((S (*)(id, SEL))objc_msgSend)(img, sl("size"));
    if (z.w <= 0 || z.h <= 0) return 0;
    double k = 1;
    if (max.w > 0 && max.h > 0) { double kx = max.w / z.w, ky = max.h / z.h; k = kx < ky ? kx : ky; if (k > 1) k = 1; }
    size_t w = (size_t)(z.w * k + 0.5), h = (size_t)(z.h * k + 0.5);
    if (!w) w = 1;
    if (!h) h = 1;
    void *src = nsimage_cg(img);
    if (!src) return 0;
    void *space = CGColorSpaceCreateDeviceRGB();
    id ctx = CGBitmapContextCreate(0, w, h, 8, w * 4, space, 2u | (2u << 12));
    CGColorSpaceRelease(space);
    if (!ctx) return 0;
    CGContextDrawImage(ctx, (R){0, 0, (double)w, (double)h}, src);
    void *out = CGBitmapContextCreateImage(ctx);
    CGContextRelease(ctx);
    return out;
}
static void *thumb_from_extension(id url, id uti, S max) {
    id b = find_appex("com.apple.quicklook.thumbnail", uti);
    id prov = b ? appex_instance(b) : 0;
    if (!prov || !responds(prov, "provideThumbnailForFileRequest:completionHandler:")) { if (prov) objc_release(prov); return 0; }
    id req = MSG0(id, MSG0(id, (id)&QLFileThumbnailRequest_c, "alloc"), "init");
    aset(req, &k_req_url, url);
    extern void cx_ql_req_size(id req, S max);
    cx_ql_req_size(req, max);
    done_blk *d = new_done();
    if (!d) { objc_release(prov); objc_release(req); return 0; }
    MSG(void, prov, "provideThumbnailForFileRequest:completionHandler:", req, (void *)d);
    wait_done(d, 10);
    void *img = 0;
    if (d->done && d->result) {
        extern void *cx_ql_reply_image(id reply, S max);
        img = cx_ql_reply_image(d->result, max);
    }
    if (d->result) objc_release(d->result);
    if (d->error) objc_release(d->error);
    if (d->done) free(d);
    objc_release(req);
    objc_release(prov);
    return img;
}
static void *thumb_from_generator(id url, id uti, S max, id options) {
    id gurl = find_generator(uti);
    if (!gurl) return 0;
    id bundle = 0;
    ql_gen_vtbl **g = gen_open(gurl, &bundle);
    if (!g || !(*g)->thumbnail) return 0;
    id req = req_new(url, uti, options, bundle, max, 1);
    void *img = 0;
    if (req && (*g)->thumbnail(g, req, url, uti, options, max) == 0) {
        qlreq *r = RQ(req);
        if (r->out_image) img = CGImageRetain(r->out_image);
    }
    if (req) req_free(req);
    if ((*g)->release) (*g)->release(g);
    return img;
}
static void *thumb_builtin(id url, id uti, S max, int icon_mode) {
    if (conforms(uti, "public.image") || conforms(uti, "com.adobe.pdf") || ext_in(url, IMG_EXT " pdf")) {
        id img = MSG(id, MSG0(id, (id)kls("NSImage"), "alloc"), "initWithContentsOfURL:", url);
        void *out = fit_image(img, max);
        if (img) objc_release(img);
        if (out) return out;
    }
    if (icon_mode) {
        id icon = MSG(id, MSG0(id, (id)kls("NSWorkspace"), "sharedWorkspace"), "iconForFile:", MSG0(id, url, "path"));
        if (icon) { ((void (*)(id, SEL, S))objc_msgSend)(icon, sl("setSize:"), max); return fit_image(icon, max); }
    }
    return 0;
}
static void *make_thumbnail(id url, S max, id options) {
    if (!url) return 0;
    id uti = uti_of(url);
    id im = options ? MSG(id, options, "objectForKey:", KEY(kQLThumbnailOptionIconModeKey)) : 0;
    int icon_mode = im && MSG0(BOOL, im, "boolValue");
    void *img = thumb_from_extension(url, uti, max);
    if (!img) img = thumb_from_generator(url, uti, max, options);
    if (!img) img = thumb_builtin(url, uti, max, icon_mode);
    return img;
}
void *QLThumbnailImageCreate(void *alloc, id url, S max, id options) { (void)alloc; return make_thumbnail(url, max, options); }

/* QLThumbnailRef: objeto com URL, tamanho, opções e a imagem pronta */
typedef struct { id url, options; S max; void *image; int cancelled, done; } qlthumb;
static char k_thumb;
static qlthumb *TH(id t) { id b = t ? aget(t, &k_thumb) : 0; return b ? MSG0(void *, b, "pointerValue") : 0; }
id QLThumbnailCreate(void *alloc, id url, S max, id options) {
    (void)alloc;
    id o = MSG0(id, MSG0(id, (id)&_MOSQLRequest_c, "alloc"), "init");
    qlthumb *t = calloc(1, sizeof *t);
    if (!o || !t) { free(t); return 0; }
    t->url = keep(url); t->options = keep(options); t->max = max;
    aset(o, &k_thumb, val_ptr((void *)t));
    return o;
}
static void th_run(qlthumb *t) { if (!t->done && !t->cancelled) { t->image = make_thumbnail(t->url, t->max, t->options); t->done = 1; } }
void *QLThumbnailCopyImage(id th) { qlthumb *t = TH(th); if (!t) return 0; th_run(t); return t->image ? CGImageRetain(t->image) : 0; }
S QLThumbnailGetMaximumSize(id th) { qlthumb *t = TH(th); S z = {0, 0}; return t ? t->max : z; }
R QLThumbnailGetContentRect(id th) {
    qlthumb *t = TH(th);
    R r = {0, 0, 0, 0};
    if (t && t->image) { r.w = (double)CGImageGetWidth(t->image); r.h = (double)CGImageGetHeight(t->image); }
    return r;
}
void QLThumbnailCancel(id th) { qlthumb *t = TH(th); if (t) t->cancelled = 1; }
u8 QLThumbnailIsCancelled(id th) { qlthumb *t = TH(th); return t ? (u8)t->cancelled : 1; }
typedef struct { id th; void *queue; void *blk; } async_t;
static void async_done(void *x) { async_t *a = x; ((void (*)(void *))BLK_FN(a->blk))(a->blk); _Block_release(a->blk); objc_release(a->th); free(a); }
static void async_work(void *x) {
    async_t *a = x;
    th_run(TH(a->th));
    dispatch_async_f(a->queue ? a->queue : dispatch_get_global_queue(0, 0), a, async_done);
}
void QLThumbnailDispatchAsync(id th, void *queue, void *completion) {
    if (!th || !completion) return;
    async_t *a = calloc(1, sizeof *a);
    if (!a) return;
    a->th = objc_retain(th); a->queue = queue; a->blk = _Block_copy(completion);
    dispatch_async_f(dispatch_get_global_queue(0, 0), a, async_work);
}

/* ---- QLThumbnailReply / QLFileThumbnailRequest (lado da extensão do app) */
static char k_tr_size, k_tr_block, k_tr_cur, k_tr_url, k_tr_badge, k_fr_max, k_fr_min, k_fr_scale;
static id tr_with_ctx(id cls, SEL c, S size, void *blk) {
    (void)c;
    id o = MSG0(id, MSG0(id, cls, "alloc"), "init");
    aset(o, &k_tr_size, val_size(size));
    if (blk) aset(o, &k_tr_block, (id)_Block_copy(blk));
    return objc_autorelease(o);
}
static id tr_with_cur(id cls, SEL c, S size, void *blk) { id o = tr_with_ctx(cls, c, size, blk); aset(o, &k_tr_cur, num_bool((BOOL)YES)); return o; }
static id tr_with_url(id cls, SEL c, id url) { (void)c; id o = MSG0(id, MSG0(id, cls, "alloc"), "init"); aset(o, &k_tr_url, url); return objc_autorelease(o); }
static id tr_badge(id self, SEL c) { (void)c; return aget(self, &k_tr_badge); }
static void tr_set_badge(id self, SEL c, id v) { (void)c; aset(self, &k_tr_badge, v); }
/* desenha a resposta: o bloco recebe um CGContext (ou o NSGraphicsContext atual) do tamanho pedido */
void *cx_ql_reply_image(id reply, S max) {
    id url = aget(reply, &k_tr_url);
    if (url) {
        id img = MSG(id, MSG0(id, (id)kls("NSImage"), "alloc"), "initWithContentsOfURL:", url);
        void *out = fit_image(img, max);
        if (img) objc_release(img);
        return out;
    }
    void *blk = aget(reply, &k_tr_block);
    id sz = aget(reply, &k_tr_size);
    if (!blk || !sz) return 0;
    S z = MSG0(S, sz, "sizeValue");
    size_t w = (size_t)(z.w + 0.5), h = (size_t)(z.h + 0.5);
    if (!w || !h) return 0;
    void *space = CGColorSpaceCreateDeviceRGB();
    id ctx = CGBitmapContextCreate(0, w, h, 8, w * 4, space, 2u | (2u << 12));
    CGColorSpaceRelease(space);
    if (!ctx) return 0;
    BOOL ok;
    if (aget(reply, &k_tr_cur)) {
        id gc = MSG(id, (id)kls("NSGraphicsContext"), "graphicsContextWithCGContext:flipped:", ctx, (BOOL)NO);
        MSG0(void, (id)kls("NSGraphicsContext"), "saveGraphicsState");
        MSG(void, (id)kls("NSGraphicsContext"), "setCurrentContext:", gc);
        ok = ((BOOL (*)(void *))BLK_FN(blk))(blk);
        MSG0(void, (id)kls("NSGraphicsContext"), "restoreGraphicsState");
    } else ok = ((BOOL (*)(void *, id))BLK_FN(blk))(blk, ctx);
    void *img = ok ? CGBitmapContextCreateImage(ctx) : 0;
    CGContextRelease(ctx);
    return img;
}
void cx_ql_req_size(id req, S max) {
    aset(req, &k_fr_max, val_size(max));
    aset(req, &k_fr_min, val_size((S){0, 0}));
    aset(req, &k_fr_scale, num_dbl(2.0));
}
static S fr_max(id self, SEL c) { (void)c; id v = aget(self, &k_fr_max); S z = {0, 0}; return v ? MSG0(S, v, "sizeValue") : z; }
static S fr_min(id self, SEL c) { (void)c; id v = aget(self, &k_fr_min); S z = {0, 0}; return v ? MSG0(S, v, "sizeValue") : z; }
static double fr_scale(id self, SEL c) { (void)c; id v = aget(self, &k_fr_scale); return v ? MSG0(double, v, "doubleValue") : 1; }
static void tprov_default(id self, SEL c, id req, void *blk) { provider_default(self, c, req, blk); }

/* ---- QLThumbnailGenerator / QLThumbnailGenerationRequest / QLThumbnailRepresentation */
static char k_gr_url, k_gr_size, k_gr_scale, k_gr_types, k_gr_icon, k_rep_img, k_rep_type;
static id gr_init(id self, SEL c, id url, S size, double scale, u64 types) {
    (void)c;
    aset(self, &k_gr_url, url);
    aset(self, &k_gr_size, val_size(size));
    aset(self, &k_gr_scale, num_dbl(scale));
    aset(self, &k_gr_types, num_ull(types));
    return self;
}
static BOOL gr_icon(id self, SEL c) { (void)c; id v = aget(self, &k_gr_icon); return v && MSG0(BOOL, v, "boolValue"); }
static void gr_set_icon(id self, SEL c, BOOL v) { (void)c; aset(self, &k_gr_icon, num_bool(v)); }
static void *rep_cg(id self, SEL c) { (void)c; id b = aget(self, &k_rep_img); return b ? MSG0(void *, b, "pointerValue") : 0; }
static id rep_ns(id self, SEL c) {
    (void)c;
    void *cg = rep_cg(self, 0);
    if (!cg) return 0;
    id img = ((id (*)(id, SEL, void *, S))objc_msgSend)(MSG0(id, (id)kls("NSImage"), "alloc"), sl("initWithCGImage:size:"), cg, (S){0, 0});
    return img ? objc_autorelease(img) : 0;
}
static i64 rep_type(id self, SEL c) { (void)c; id v = aget(self, &k_rep_type); return v ? MSG0(i64, v, "longLongValue") : 0; }
static R rep_rect(id self, SEL c) { (void)c; void *cg = rep_cg(self, 0); R r = {0, 0, 0, 0}; if (cg) { r.w = (double)CGImageGetWidth(cg); r.h = (double)CGImageGetHeight(cg); } return r; }
static id g_gen;
static id gen_shared(id cls, SEL c) { (void)c; if (!g_gen) g_gen = MSG0(id, MSG0(id, cls, "alloc"), "init"); return g_gen; }
/* tipo da representação: 0 ícone, 1 miniatura de baixa qualidade, 2 miniatura */
static id make_rep(id req) {
    id url = aget(req, &k_gr_url);
    S max = MSG0(S, aget(req, &k_gr_size), "sizeValue");
    double scale = MSG0(double, aget(req, &k_gr_scale), "doubleValue");
    if (scale > 0) { max.w *= scale; max.h *= scale; }
    id opts = dict();
    if (gr_icon(req, 0)) MSG(void, opts, "setObject:forKey:", num_bool((BOOL)YES), KEY(kQLThumbnailOptionIconModeKey));
    void *img = make_thumbnail(url, max, opts);
    i64 type = 2;
    if (!img) {   /* sem miniatura: o ícone, se o pedido aceita ícone */
        u64 types = MSG0(u64, aget(req, &k_gr_types), "unsignedLongLongValue");
        if (types & 1) { MSG(void, opts, "setObject:forKey:", num_bool((BOOL)YES), KEY(kQLThumbnailOptionIconModeKey)); img = thumb_builtin(url, uti_of(url), max, 1); type = 0; }
    }
    objc_release(opts);
    if (!img) return 0;
    id rep = MSG0(id, MSG0(id, (id)&QLThumbnailRepresentation_c, "alloc"), "init");
    aset(rep, &k_rep_img, val_ptr(img));
    aset(rep, &k_rep_type, num_ll(type));
    return objc_autorelease(rep);
}
static id gen_error(void) { return MSG(id, (id)kls("NSError"), "errorWithDomain:code:userInfo:", str("QLThumbnailErrorDomain"), (i64)2, (id)0); }
static void gen_best(id self, SEL c, id req, void *blk) {
    (void)self; (void)c;
    if (!blk) return;
    id rep = req ? make_rep(req) : 0;
    ((void (*)(void *, id, id))BLK_FN(blk))(blk, rep, rep ? (id)0 : gen_error());
}
static void gen_all(id self, SEL c, id req, void *blk) {
    (void)self; (void)c;
    if (!blk) return;
    id rep = req ? make_rep(req) : 0;
    ((void (*)(void *, id, i64, id))BLK_FN(blk))(blk, rep, rep ? rep_type(rep, 0) : 0, rep ? (id)0 : gen_error());
}
static void gen_cancel(id self, SEL c, id req) { (void)self; (void)c; (void)req; }   /* a geração é síncrona: nada pendente */
static void gen_save(id self, SEL c, id req, id url, id type, void *blk) {
    (void)self; (void)c; (void)type;
    id rep = req ? make_rep(req) : 0;
    id err = 0;
    if (rep) {
        id ni = rep_ns(rep, 0);
        id tiff = ni ? MSG0(id, ni, "TIFFRepresentation") : 0;
        id bm = tiff ? MSG(id, (id)kls("NSBitmapImageRep"), "imageRepWithData:", tiff) : 0;
        id png = bm ? MSG(id, bm, "representationUsingType:properties:", (u64)4, MSG0(id, (id)kls("NSDictionary"), "dictionary")) : 0;
        if (!png || !MSG(BOOL, png, "writeToURL:atomically:", url, (BOOL)YES)) err = gen_error();
    } else err = gen_error();
    if (blk) ((void (*)(void *, id))BLK_FN(blk))(blk, err);
}

/* ================================================================ registro */
static void K(cx_class_t *k, cx_class_t *m, const char *n, const char *sup) { if (!cx_class_named(n)) cx_define_class(k, m, n, kls(sup), 0); }
#define A(k, s, f) cx_add_method(&k##_c, s, (void *)(f))
#define CA(k, s, f) cx_add_cmethod(&k##_c, s, (void *)(f))
void cx_quicklook_init(void) {
    K(&QLPreviewView_c, &QLPreviewView_m, "QLPreviewView", "NSView");
    K(&QLPreviewProvider_c, &QLPreviewProvider_m, "QLPreviewProvider", "NSObject");
    K(&QLPreviewReply_c, &QLPreviewReply_m, "QLPreviewReply", "NSObject");
    K(&QLPreviewReplyAttachment_c, &QLPreviewReplyAttachment_m, "QLPreviewReplyAttachment", "NSObject");
    K(&QLFilePreviewRequest_c, &QLFilePreviewRequest_m, "QLFilePreviewRequest", "NSObject");
    K(&QLThumbnailProvider_c, &QLThumbnailProvider_m, "QLThumbnailProvider", "NSObject");
    K(&QLThumbnailReply_c, &QLThumbnailReply_m, "QLThumbnailReply", "NSObject");
    K(&QLThumbnailRepresentation_c, &QLThumbnailRepresentation_m, "QLThumbnailRepresentation", "NSObject");
    K(&QLFileThumbnailRequest_c, &QLFileThumbnailRequest_m, "QLFileThumbnailRequest", "NSObject");
    K(&QLThumbnailGenerator_c, &QLThumbnailGenerator_m, "QLThumbnailGenerator", "NSObject");
    K(&QLThumbnailGenerationRequest_c, &QLThumbnailGenerationRequest_m, "QLThumbnailGenerationRequest", "NSObject");
    K(&_MOSQLRequest_c, &_MOSQLRequest_m, "_MOSQLRequest", "NSObject");

    CA(QLPreviewPanel, "sharedPreviewPanel", ql_shared); CA(QLPreviewPanel, "sharedPreviewPanelExists", ql_exists);
    A(QLPreviewPanel, "makeKeyAndOrderFront:", ql_front); A(QLPreviewPanel, "orderFront:", ql_order_front);
    A(QLPreviewPanel, "orderOut:", ql_out); A(QLPreviewPanel, "close", ql_close); A(QLPreviewPanel, "keyDown:", ql_key);
    A(QLPreviewPanel, "setDataSource:", ql_set_ds); A(QLPreviewPanel, "dataSource", ql_ds);
    A(QLPreviewPanel, "setDelegate:", ql_set_del); A(QLPreviewPanel, "delegate", ql_del);
    A(QLPreviewPanel, "setCurrentPreviewItemIndex:", ql_set_index); A(QLPreviewPanel, "currentPreviewItemIndex", ql_index);
    A(QLPreviewPanel, "currentPreviewItem", ql_current); A(QLPreviewPanel, "reloadData", ql_reload);
    A(QLPreviewPanel, "refreshCurrentPreviewItem", ql_refresh); A(QLPreviewPanel, "updateController", ql_update);
    A(QLPreviewPanel, "currentController", ql_controller);
    A(QLPreviewPanel, "enterFullScreenMode:withOptions:", ql_enter_fs); A(QLPreviewPanel, "exitFullScreenModeWithOptions:", ql_exit_fs);
    A(QLPreviewPanel, "isInFullScreenMode", ql_in_fs); A(QLPreviewPanel, "displayState", ql_state); A(QLPreviewPanel, "setDisplayState:", ql_set_state);

    A(QLPreviewView, "initWithFrame:style:", qv_init_style); A(QLPreviewView, "initWithFrame:", qv_init_frame);
    A(QLPreviewView, "previewItem", qv_item); A(QLPreviewView, "setPreviewItem:", qv_set_item);
    A(QLPreviewView, "refreshPreviewItem", qv_refresh); A(QLPreviewView, "close", qv_close);
    A(QLPreviewView, "autostarts", qv_auto); A(QLPreviewView, "setAutostarts:", qv_set_auto);
    A(QLPreviewView, "shouldCloseWithWindow", qv_shouldclose); A(QLPreviewView, "setShouldCloseWithWindow:", qv_set_shouldclose);
    A(QLPreviewView, "displayState", qv_state); A(QLPreviewView, "setDisplayState:", qv_set_state);
    A(QLPreviewView, "resizeSubviewsWithOldSize:", qv_resize);

    A(QLPreviewReply, "initWithDataOfContentType:contentSize:dataCreationBlock:", rp_init_data);
    A(QLPreviewReply, "initWithFileURL:", rp_init_file); A(QLPreviewReply, "initForPDFWithPageSize:documentCreationBlock:", rp_init_pdf);
    A(QLPreviewReply, "stringEncoding", rp_enc); A(QLPreviewReply, "setStringEncoding:", rp_set_enc);
    A(QLPreviewReply, "attachments", rp_att); A(QLPreviewReply, "setAttachments:", rp_set_att);
    A(QLPreviewReply, "title", rp_title); A(QLPreviewReply, "setTitle:", rp_set_title);
    A(QLPreviewReplyAttachment, "initWithData:contentType:", att_init); A(QLPreviewReplyAttachment, "data", att_data);
    A(QLPreviewReplyAttachment, "contentType", att_type);
    A(QLFilePreviewRequest, "fileURL", rq_url); A(QLFileThumbnailRequest, "fileURL", rq_url);
    A(QLFileThumbnailRequest, "maximumSize", fr_max); A(QLFileThumbnailRequest, "minimumSize", fr_min); A(QLFileThumbnailRequest, "scale", fr_scale);
    A(QLPreviewProvider, "providePreviewForFileRequest:completionHandler:", provider_default);
    A(QLThumbnailProvider, "provideThumbnailForFileRequest:completionHandler:", tprov_default);
    CA(QLThumbnailReply, "replyWithContextSize:drawingBlock:", tr_with_ctx);
    CA(QLThumbnailReply, "replyWithContextSize:currentContextDrawingBlock:", tr_with_cur);
    CA(QLThumbnailReply, "replyWithImageFileURL:", tr_with_url);
    A(QLThumbnailReply, "extensionBadge", tr_badge); A(QLThumbnailReply, "setExtensionBadge:", tr_set_badge);

    CA(QLThumbnailGenerator, "sharedGenerator", gen_shared);
    A(QLThumbnailGenerator, "generateBestRepresentationForRequest:completionHandler:", gen_best);
    A(QLThumbnailGenerator, "generateRepresentationsForRequest:updateHandler:", gen_all);
    A(QLThumbnailGenerator, "cancelRequest:", gen_cancel);
    A(QLThumbnailGenerator, "saveBestRepresentationForRequest:toFileAtURL:withContentType:completionHandler:", gen_save);
    A(QLThumbnailGenerationRequest, "initWithFileAtURL:size:scale:representationTypes:", gr_init);
    A(QLThumbnailGenerationRequest, "iconMode", gr_icon); A(QLThumbnailGenerationRequest, "setIconMode:", gr_set_icon);
    A(QLThumbnailRepresentation, "CGImage", rep_cg); A(QLThumbnailRepresentation, "NSImage", rep_ns);
    A(QLThumbnailRepresentation, "type", rep_type); A(QLThumbnailRepresentation, "contentRect", rep_rect);
}
