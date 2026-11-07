/*
 * mOS — Launch Services (kernel/include/launchsvc.h). O banco de tipos e de apps que abrem cada tipo, as escolhas
 * do usuário e a abertura propriamente dita (apps do kernel ou apps do macOS).
 *
 * Entrega a apps do macOS: o processo principal do app registra "com.mos.abrir-url.<CFBundleIdentifier>" no
 * bootstrap ao terminar de abrir (userland/libsystem/openurl.c). O kernel põe a URL (ou file://... de um documento)
 * numa mensagem Mach para esse serviço; no app ela vira o Apple Event GURL ou odoc (application:openURLs:,
 * application:openFiles:...). App fechado: é iniciado e a mensagem espera o serviço aparecer (até 2 min). Um
 * navegador recebe a URL web na linha de comando, como o Chrome espera.
 */
#include "launchsvc.h"
#include "plist.h"
#include "vfs.h"
#include "compat.h"
#include "apps.h"
#include "ui.h"
#include "web.h"
#include "proc.h"
#include "syscall.h"

int machipc_post_service(const char *name, int id, const void *body, u32 n);
int apps_run_mac(const char *bundle, const char *args);      /* registry.c: pid ou -1 */
void apps_sync_mac(void);                                    /* registry.c: atalhos do Launchpad para apps novos */
void finder_open_path(const char *path);
void archive_open_file(const char *path);
void diskimage_open_file(const char *path);
void script_open_file(const char *path);
void mail_open_url(const char *url);
extern app_t app_compactacao, app_dimounter;
app_t *compat_gui_app_for_pid(int pid);
void wm_bring_app(app_t *a);

#define OU_PREFIX "com.mos.abrir-url."
#define OU_MSG_ID 0x6d4f5355                 /* 'mOSU', o mesmo do openurl.c */
enum { RANK_NONE, RANK_ALTERNATE, RANK_DEFAULT, RANK_OWNER };

void (*ls_choose_app_hook)(const char *path);
static u32 ls_gen = 1;   /* muda quando apps, tipos ou escolhas mudam: invalida o cache */

/* ================================================================ tipos */
typedef struct {
    const char *id, *desc, *parents, *exts, *mimes;   /* listas separadas por espaço */
    const char *icon;                                  /* UTTypeIconFile (só tipos de apps) */
    ls_app_t *owner;                                   /* app que declarou (NULL = sistema) */
    bool exported;
} uti_t;

static const uti_t sys_types[] = {
    {"public.item", "Item", "", "", ""},
    {"public.content", "Conteúdo", "", "", ""},
    {"public.composite-content", "Conteúdo composto", "public.content", "", ""},
    {"public.data", "Documento", "public.item", "", "application/octet-stream"},
    {"public.directory", "Pasta", "public.item", "", ""},
    {"public.folder", "Pasta", "public.directory", "", ""},
    {"public.volume", "Volume", "public.folder", "", ""},
    {"com.apple.package", "Pacote", "public.directory", "", ""},
    {"com.apple.bundle", "Pacote", "public.directory", "bundle", ""},
    {"com.apple.framework", "Framework", "com.apple.bundle", "framework", ""},
    {"public.executable", "Executável", "public.item", "", ""},
    {"com.apple.application", "Aplicativo", "public.executable", "", ""},
    {"com.apple.application-bundle", "Aplicativo", "com.apple.application com.apple.bundle com.apple.package", "app", ""},
    {"public.unix-executable", "Executável Unix", "public.data public.executable", "", "application/x-executable"},
    {"com.apple.mach-o-binary", "Executável Unix", "public.unix-executable", "", ""},
    {"com.microsoft.windows-executable", "Executável do Windows", "public.data public.executable", "exe", "application/x-msdownload"},
    /* texto */
    {"public.text", "Texto", "public.data public.content", "", "text/*"},
    {"public.plain-text", "Documento de texto simples", "public.text", "txt text", "text/plain"},
    {"public.utf8-plain-text", "Documento de texto simples", "public.plain-text", "", "text/plain;charset=utf-8"},
    {"public.rtf", "Documento RTF", "public.text", "rtf", "text/rtf application/rtf"},
    {"public.html", "Página da web HTML", "public.text", "html htm shtml", "text/html"},
    {"public.xml", "Documento XML", "public.text", "xml", "application/xml text/xml"},
    {"public.xhtml", "Página da web XHTML", "public.xml", "xhtml xht", "application/xhtml+xml"},
    {"public.svg-image", "Imagem SVG", "public.image public.xml", "svg svgz", "image/svg+xml"},
    {"com.apple.property-list", "Lista de propriedades", "public.data", "plist", "application/x-plist"},
    {"public.json", "Documento JSON", "public.text", "json", "application/json"},
    {"public.yaml", "Documento YAML", "public.text", "yaml yml", "application/x-yaml"},
    {"public.comma-separated-values-text", "Documento CSV", "public.text", "csv", "text/csv"},
    {"public.tab-separated-values-text", "Documento TSV", "public.text", "tsv", "text/tab-separated-values"},
    {"net.daringfireball.markdown", "Documento Markdown", "public.plain-text", "md markdown mdown", "text/markdown"},
    {"public.log", "Arquivo de registro", "public.plain-text", "log", ""},
    {"public.source-code", "Código-fonte", "public.plain-text", "", ""},
    {"public.c-source", "Código-fonte C", "public.source-code", "c", "text/x-c"},
    {"public.c-header", "Cabeçalho C", "public.source-code", "h", "text/x-c-header"},
    {"public.c-plus-plus-source", "Código-fonte C++", "public.source-code", "cpp cc cxx c++ cp", "text/x-c++"},
    {"public.c-plus-plus-header", "Cabeçalho C++", "public.source-code", "hpp hh hxx", ""},
    {"public.objective-c-source", "Código-fonte Objective-C", "public.source-code", "m", ""},
    {"public.objective-c-plus-plus-source", "Código-fonte Objective-C++", "public.source-code", "mm", ""},
    {"public.swift-source", "Código-fonte Swift", "public.source-code", "swift", "text/x-swift"},
    {"public.assembly-source", "Código-fonte assembly", "public.source-code", "s asm", ""},
    {"public.patch-file", "Arquivo de patch", "public.plain-text", "patch diff", "text/x-diff"},
    {"public.css", "Folha de estilos CSS", "public.text", "css", "text/css"},
    {"public.script", "Script", "public.source-code", "", ""},
    {"public.shell-script", "Script de shell", "public.script", "sh bash zsh", "application/x-sh"},
    {"com.apple.terminal.shell-script", "Script de shell do Terminal", "public.shell-script", "command tool", ""},
    {"public.python-script", "Script Python", "public.script", "py pyw", "text/x-python-script"},
    {"public.ruby-script", "Script Ruby", "public.script", "rb", "text/x-ruby-script"},
    {"public.perl-script", "Script Perl", "public.script", "pl pm", "text/x-perl-script"},
    {"public.php-script", "Script PHP", "public.script", "php", "text/php"},
    {"com.netscape.javascript-source", "Script JavaScript", "public.script", "js mjs cjs jscript", "text/javascript application/javascript"},
    {"com.microsoft.typescript", "Script TypeScript", "public.script", "ts tsx mts", "application/typescript"},
    {"com.apple.applescript.text", "Script do AppleScript (texto)", "public.plain-text", "applescript", ""},
    {"com.apple.applescript.script", "Script compilado", "public.data", "scpt", ""},
    /* imagens */
    {"public.image", "Imagem", "public.data public.content", "", "image/*"},
    {"public.png", "Imagem PNG", "public.image", "png", "image/png"},
    {"public.jpeg", "Imagem JPEG", "public.image", "jpg jpeg jpe", "image/jpeg"},
    {"com.compuserve.gif", "Imagem GIF", "public.image", "gif", "image/gif"},
    {"com.microsoft.bmp", "Imagem BMP do Windows", "public.image", "bmp", "image/bmp"},
    {"public.tiff", "Imagem TIFF", "public.image", "tif tiff", "image/tiff"},
    {"public.heic", "Imagem HEIC", "public.image", "heic heics", "image/heic"},
    {"public.heif", "Imagem HEIF", "public.image", "heif", "image/heif"},
    {"org.webmproject.webp", "Imagem WebP", "public.image", "webp", "image/webp"},
    {"public.avif", "Imagem AVIF", "public.image", "avif", "image/avif"},
    {"com.microsoft.ico", "Ícone do Windows", "public.image", "ico", "image/vnd.microsoft.icon image/x-icon"},
    {"com.apple.icns", "Ícone do Apple", "public.image", "icns", "image/x-icns"},
    {"public.pbm", "Imagem Netpbm", "public.image", "ppm pbm pgm pnm", "image/x-portable-pixmap"},
    {"com.adobe.photoshop-image", "Imagem do Photoshop", "public.image", "psd", "image/vnd.adobe.photoshop"},
    {"com.adobe.pdf", "Documento PDF", "public.data public.composite-content", "pdf", "application/pdf"},
    /* áudio e vídeo */
    {"public.audiovisual-content", "Conteúdo audiovisual", "public.data public.content", "", ""},
    {"public.audio", "Áudio", "public.audiovisual-content", "", "audio/*"},
    {"public.mp3", "Áudio MP3", "public.audio", "mp3", "audio/mpeg audio/mp3"},
    {"com.microsoft.waveform-audio", "Áudio WAVE", "public.audio", "wav wave", "audio/wav audio/x-wav audio/vnd.wave"},
    {"public.aiff-audio", "Áudio AIFF", "public.audio", "aiff aif", "audio/aiff audio/x-aiff"},
    {"public.mpeg-4-audio", "Áudio MPEG-4", "public.audio", "", "audio/mp4"},
    {"com.apple.m4a-audio", "Áudio MPEG-4", "public.mpeg-4-audio", "m4a", "audio/x-m4a"},
    {"org.xiph.flac", "Áudio FLAC", "public.audio", "flac", "audio/flac"},
    {"org.xiph.ogg-audio", "Áudio Ogg", "public.audio", "ogg oga opus", "audio/ogg"},
    {"public.movie", "Filme", "public.audiovisual-content", "", "video/*"},
    {"public.video", "Vídeo", "public.movie", "", ""},
    {"public.mpeg-4", "Filme MPEG-4", "public.movie", "mp4 mp4v mpeg4 m4v", "video/mp4"},
    {"com.apple.quicktime-movie", "Filme do QuickTime", "public.movie", "mov qt", "video/quicktime"},
    {"public.avi", "Filme AVI", "public.movie", "avi vfw", "video/avi video/x-msvideo"},
    {"public.mpeg", "Filme MPEG", "public.movie", "mpg mpeg m75 m15", "video/mpeg"},
    {"org.matroska.mkv", "Vídeo Matroska", "public.movie", "mkv", "video/x-matroska"},
    {"org.webmproject.webm", "Vídeo WebM", "public.movie", "webm", "video/webm"},
    /* arquivos compactados e imagens de disco */
    {"public.archive", "Arquivo", "public.data", "", ""},
    {"com.pkware.zip-archive", "Arquivo ZIP", "public.data public.archive", "", ""},
    {"public.zip-archive", "Arquivo ZIP", "com.pkware.zip-archive", "zip", "application/zip application/x-zip-compressed"},
    {"org.gnu.gnu-zip-archive", "Arquivo gzip", "public.data public.archive", "gz gzip", "application/gzip application/x-gzip"},
    {"org.gnu.gnu-zip-tar-archive", "Arquivo tar compactado", "org.gnu.gnu-zip-archive", "tgz", ""},
    {"public.tar-archive", "Arquivo tar", "public.data public.archive", "tar", "application/x-tar"},
    {"public.bzip2-archive", "Arquivo bzip2", "public.data public.archive", "bz2 bzip2 tbz tbz2", "application/x-bzip2"},
    {"org.tukaani.xz-archive", "Arquivo XZ", "public.data public.archive", "xz txz", "application/x-xz"},
    {"org.7-zip.7-zip-archive", "Arquivo 7-Zip", "public.data public.archive", "7z", "application/x-7z-compressed"},
    {"com.rarlab.rar-archive", "Arquivo RAR", "public.data public.archive", "rar", "application/vnd.rar application/x-rar-compressed"},
    {"public.disk-image", "Imagem de disco", "public.data public.archive", "img", ""},
    {"com.apple.disk-image-udif", "Imagem de disco", "public.disk-image", "dmg smi", "application/x-apple-diskimage"},
    {"public.iso-image", "Imagem de disco ISO", "public.disk-image", "iso cdr", "application/x-iso9660-image"},
    {"com.apple.installer-package-archive", "Pacote do Instalador", "public.data public.archive", "pkg mpkg", "application/x-newton-compatible-pkg"},
    {"com.sun.java-archive", "Arquivo Java", "public.zip-archive public.executable", "jar", "application/java-archive"},
    {"org.idpf.epub-container", "Livro EPUB", "public.zip-archive public.composite-content", "epub", "application/epub+zip"},
    /* documentos */
    {"public.contact", "Contato", "public.content", "", ""},
    {"public.vcard", "vCard", "public.text public.contact", "vcf vcard", "text/vcard text/x-vcard text/directory"},
    {"public.calendar-event", "Evento de calendário", "public.content", "", ""},
    {"com.apple.ical.ics", "Arquivo iCalendar", "public.calendar-event public.text", "ics ical", "text/calendar"},
    {"com.apple.mail.email", "Mensagem de e-mail", "public.data public.content", "eml", "message/rfc822"},
    {"com.apple.web-internet-location", "Local da Internet", "public.data", "webloc", ""},
    {"public.font", "Fonte", "public.data public.content", "", ""},
    {"public.truetype-ttf-font", "Fonte TrueType", "public.font", "ttf", "font/ttf"},
    {"public.opentype-font", "Fonte OpenType", "public.font", "otf", "font/otf"},
    {"public.truetype-collection-font", "Coleção de fontes TrueType", "public.font", "ttc", "font/collection"},
    {"org.w3.woff", "Fonte WOFF", "public.font", "woff", "font/woff"},
    {"org.w3.woff2", "Fonte WOFF2", "public.font", "woff2", "font/woff2"},
    {"com.microsoft.word.doc", "Documento do Microsoft Word 97", "public.data public.composite-content", "doc", "application/msword"},
    {"org.openxmlformats.wordprocessingml.document", "Documento do Microsoft Word", "public.zip-archive public.composite-content", "docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
    {"com.microsoft.excel.xls", "Planilha do Microsoft Excel 97", "public.data public.composite-content", "xls", "application/vnd.ms-excel"},
    {"org.openxmlformats.spreadsheetml.sheet", "Planilha do Microsoft Excel", "public.zip-archive public.composite-content", "xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
    {"com.microsoft.powerpoint.ppt", "Apresentação do PowerPoint 97", "public.data public.composite-content", "ppt", "application/vnd.ms-powerpoint"},
    {"org.openxmlformats.presentationml.presentation", "Apresentação do PowerPoint", "public.zip-archive public.composite-content", "pptx", "application/vnd.openxmlformats-officedocument.presentationml.presentation"},
    {"public.database", "Banco de dados", "public.data", "sqlite sqlite3 db", "application/vnd.sqlite3"},
};

#define MAX_DYN_TYPES 512
static uti_t dyn_types[MAX_DYN_TYPES];
static int ndyn;

/* palavra w (até wl) na lista separada por espaço, sem diferenciar maiúsculas */
static bool word_in(const char *list, const char *w, usize wl) {
    if (!list || !w || !wl) return false;
    for (const char *p = list; *p;) {
        while (*p == ' ') p++;
        const char *s = p;
        while (*p && *p != ' ') p++;
        if ((usize)(p - s) == wl && !strncasecmp(s, w, wl)) return true;
    }
    return false;
}
/* i-ésima palavra da lista em out */
static bool word_at(const char *list, int i, char *out, usize cap) {
    if (!list) return false;
    for (const char *p = list; *p;) {
        while (*p == ' ') p++;
        const char *s = p;
        while (*p && *p != ' ') p++;
        if (p == s) break;
        if (i-- == 0) { usize n = MIN((usize)(p - s), cap - 1); memcpy(out, s, n); out[n] = 0; return true; }
    }
    return false;
}

static const uti_t *find_type(const char *id) {
    if (!id || !id[0]) return NULL;
    for (u32 i = 0; i < ARRAY_LEN(sys_types); i++) if (!strcasecmp(sys_types[i].id, id)) return &sys_types[i];
    const uti_t *imp = NULL;
    for (int i = 0; i < ndyn; i++)
        if (dyn_types[i].id && !strcasecmp(dyn_types[i].id, id)) { if (dyn_types[i].exported) return &dyn_types[i]; if (!imp) imp = &dyn_types[i]; }
    return imp;
}

/* buffers rotativos para strings devolvidas */
static char *rbuf(void) { static char b[8][256]; static int k; return b[k++ & 7]; }

static bool is_dyn(const char *id) { return id && !strncmp(id, "dyn.", 4); }
#define DYN_PREFIX "dyn.ah62d4rv4ge8"
static const char *dyn_for_ext(const char *ext) {
    char *o = rbuf();
    int n = snprintf(o, 256, DYN_PREFIX);
    for (const char *p = ext; *p && n < 250; p++) n += snprintf(o + n, 256 - (usize)n, "%02x", (u8)((*p >= 'A' && *p <= 'Z') ? *p + 32 : *p));
    return o;
}
static bool dyn_ext(const char *id, char *out, usize cap) {
    if (strncmp(id, DYN_PREFIX, strlen(DYN_PREFIX))) return false;
    const char *h = id + strlen(DYN_PREFIX);
    usize o = 0;
    for (; h[0] && h[1] && o + 1 < cap; h += 2) {
        int hi = h[0] <= '9' ? h[0] - '0' : (h[0] | 32) - 'a' + 10, lo = h[1] <= '9' ? h[1] - '0' : (h[1] | 32) - 'a' + 10;
        out[o++] = (char)(hi * 16 + lo);
    }
    out[o] = 0;
    return o > 0;
}

/* profundidade da conformidade (0 = igual) ou -1 */
static int conform_depth(const char *a, const char *b, int lvl) {
    if (!a || !b || lvl > 16) return -1;
    if (!strcasecmp(a, b)) return 0;
    if (!strcasecmp(b, "public.item")) return 8;         /* tudo é item */
    const uti_t *t = find_type(a);
    if (!t) return is_dyn(a) && (!strcasecmp(b, "public.data")) ? 1 : -1;
    int best = -1;
    char p[128];
    for (int i = 0; word_at(t->parents, i, p, sizeof p); i++) {
        int d = conform_depth(p, b, lvl + 1);
        if (d >= 0 && (best < 0 || d + 1 < best)) best = d + 1;
    }
    return best;
}
bool ls_conforms(const char *uti, const char *to) { return conform_depth(uti, to, 0) >= 0; }

int ls_type_parents(const char *uti, const char **out, int max) {
    const uti_t *t = find_type(uti);
    int n = 0;
    if (!t) { if (is_dyn(uti) && max > 0) out[n++] = "public.data"; return n; }
    char p[128];
    for (int i = 0; n < max && word_at(t->parents, i, p, sizeof p); i++) { char *b = rbuf(); snprintf(b, 256, "%s", p); out[n++] = b; }
    return n;
}

static bool cls_ext(const char *c) { return !c || !c[0] || !strcmp(c, "public.filename-extension"); }
static bool cls_mime(const char *c) { return c && !strcmp(c, "public.mime-type"); }

const char *ls_type_for_tag(const char *tag_class, const char *tag, const char *conforming) {
    if (!tag || !tag[0]) return NULL;
    char t[96];
    snprintf(t, sizeof t, "%s", tag[0] == '.' ? tag + 1 : tag);
    usize tl = strlen(t);
    bool mime = cls_mime(tag_class);
    if (!mime && !cls_ext(tag_class)) return NULL;
    for (int pass = 0; pass < 3; pass++) {   /* sistema, tipos exportados, importados */
        int n = pass == 0 ? (int)ARRAY_LEN(sys_types) : ndyn;
        for (int i = 0; i < n; i++) {
            const uti_t *u = pass == 0 ? &sys_types[i] : &dyn_types[i];
            if (!u->id || (pass == 1 && !u->exported) || (pass == 2 && u->exported)) continue;
            if (!word_in(mime ? u->mimes : u->exts, t, tl)) continue;
            if (conforming && conforming[0] && !ls_conforms(u->id, conforming)) continue;
            return u->id;
        }
    }
    return mime ? NULL : dyn_for_ext(t);
}

int ls_type_tags(const char *uti, const char *tag_class, const char **out, int max) {
    const uti_t *t = find_type(uti);
    int n = 0;
    if (!t) {
        char e[64];
        if (cls_ext(tag_class) && is_dyn(uti) && dyn_ext(uti, e, sizeof e) && max > 0) { char *b = rbuf(); snprintf(b, 256, "%s", e); out[n++] = b; }
        return n;
    }
    const char *list = cls_mime(tag_class) ? t->mimes : cls_ext(tag_class) ? t->exts : NULL;
    char w[128];
    for (int i = 0; n < max && word_at(list, i, w, sizeof w); i++) { char *b = rbuf(); snprintf(b, 256, "%s", w); out[n++] = b; }
    return n;
}
const char *ls_tag_for_type(const char *uti, const char *tag_class) {
    const char *o[1];
    return ls_type_tags(uti, tag_class, o, 1) ? o[0] : NULL;
}

const char *ls_type_desc(const char *uti) {
    const uti_t *t = find_type(uti);
    if (t && t->desc && t->desc[0]) return t->desc;
    char e[64];
    if (is_dyn(uti) && dyn_ext(uti, e, sizeof e)) {
        char *b = rbuf();
        char up[64];
        usize i = 0;
        for (; e[i] && i + 1 < sizeof up; i++) up[i] = (char)((e[i] >= 'a' && e[i] <= 'z') ? e[i] - 32 : e[i]);
        up[i] = 0;
        snprintf(b, 256, "Documento %s", up);
        return b;
    }
    return "Documento";
}

static bool ends_with(const char *s, const char *suf) {
    usize a = strlen(s), b = strlen(suf);
    return a >= b && !strcasecmp(s + a - b, suf);
}
static const char *path_ext(const char *path) {
    const char *b = strrchr(path, '/');
    b = b ? b + 1 : path;
    const char *d = strrchr(b, '.');
    return d && d != b ? d + 1 : "";
}

const char *ls_type_of_path(const char *path) {
    vnode_t *n = vfs_lookup(path);
    if (n && n->dir) {
        if (ends_with(path, ".app") && vfs_child(n, "Contents")) return "com.apple.application-bundle";
        if (ends_with(path, ".framework")) return "com.apple.framework";
        if (ends_with(path, ".bundle")) return "com.apple.bundle";
        if (vfs_is_volume(n)) return "public.volume";
        return "public.folder";
    }
    const char *e = path_ext(path);
    if (e[0]) {
        const char *t = ls_type_for_tag("public.filename-extension", e, NULL);
        if (t && !is_dyn(t)) return t;
        if (n && n->data && macho_is_macho(n->data, n->size)) return "com.apple.mach-o-binary";
        return t;
    }
    if (n && n->data && macho_is_macho(n->data, n->size)) return "com.apple.mach-o-binary";
    if (n && n->data && n->size > 2 && n->data[0] == '#' && n->data[1] == '!') return "public.shell-script";
    return "public.data";
}

/* ================================================================ apps */
typedef struct {
    char *uti;              /* NULL = só extensão */
    char ext[24];           /* minúsculas; "*" = qualquer */
    u8 role, rank;
    char *name, *icon;      /* CFBundleTypeName, CFBundleTypeIconFile */
} claim_t;

struct ls_app {
    char id[128], name[128], path[320], exe[128];
    app_t *builtin;
    void (*open_file)(const char *path);
    void (*open_url)(const char *url);
    claim_t *claims;
    int nclaims, cclaims;
    char schemes[24][40];
    int nschemes;
    bool gone;
    u32 order;
    usize plist_size;       /* para saber se o Info.plist mudou */
};

#define MAX_APPS 256
static ls_app_t *apps[MAX_APPS];
static int napps;
static u32 next_order;

int ls_app_count(void) { int n = 0; for (int i = 0; i < napps; i++) if (!apps[i]->gone) n++; return n; }
ls_app_t *ls_app_at(int i) { for (int k = 0; k < napps; k++) if (!apps[k]->gone && i-- == 0) return apps[k]; return NULL; }
const char *ls_app_name(const ls_app_t *a) { return a ? a->name : ""; }
const char *ls_app_id(const ls_app_t *a) { return a ? a->id : ""; }
const char *ls_app_path(const ls_app_t *a) { return a ? a->path : ""; }
app_t *ls_app_builtin(const ls_app_t *a) { return a ? a->builtin : NULL; }

static bool app_has_scheme(const ls_app_t *a, const char *s) {
    for (int i = 0; i < a->nschemes; i++) if (!strcasecmp(a->schemes[i], s)) return true;
    return false;
}
bool ls_app_is_browser(const ls_app_t *a) { return a && app_has_scheme(a, "http") && app_has_scheme(a, "https"); }

ls_app_t *ls_app_by_id(const char *id) {
    if (!id || !id[0]) return NULL;
    for (int i = 0; i < napps; i++) if (!apps[i]->gone && !strcasecmp(apps[i]->id, id)) return apps[i];
    return NULL;
}
ls_app_t *ls_app_for_builtin(app_t *b) {
    for (int i = 0; i < napps; i++) if (apps[i]->builtin == b) return apps[i];
    return NULL;
}
ls_app_t *ls_app_by_name(const char *name) {
    if (!name || !name[0]) return NULL;
    char n[160];
    snprintf(n, sizeof n, "%s", name);
    if (ends_with(n, ".app")) n[strlen(n) - 4] = 0;
    for (int i = 0; i < napps; i++) if (!apps[i]->gone && !strcasecmp(apps[i]->name, n)) return apps[i];
    for (int i = 0; i < napps; i++) {   /* nome do executável ("Code" para o VS Code) */
        if (apps[i]->gone || !apps[i]->path[0]) continue;
        const char *b = strrchr(apps[i]->path, '/');
        char stem[160];
        snprintf(stem, sizeof stem, "%s", b ? b + 1 : apps[i]->path);
        if (ends_with(stem, ".app")) stem[strlen(stem) - 4] = 0;
        if (!strcasecmp(stem, n) || !strcasecmp(apps[i]->exe, n)) return apps[i];
    }
    return NULL;
}

static ls_app_t *app_new(void) {
    if (napps >= MAX_APPS) return NULL;
    ls_app_t *a = kcalloc(1, sizeof *a);
    if (!a) return NULL;
    a->order = next_order++;
    apps[napps++] = a;
    return a;
}

static claim_t *claim_add(ls_app_t *a) {
    if (a->nclaims == a->cclaims) {
        int nc = a->cclaims ? a->cclaims * 2 : 16;
        claim_t *n = krealloc(a->claims, (usize)nc * sizeof *n);
        if (!n) return NULL;
        a->claims = n;
        a->cclaims = nc;
    }
    claim_t *c = &a->claims[a->nclaims++];
    memset(c, 0, sizeof *c);
    return c;
}
static void claims_clear(ls_app_t *a) {
    for (int i = 0; i < a->nclaims; i++) { kfree(a->claims[i].uti); kfree(a->claims[i].name); kfree(a->claims[i].icon); }
    a->nclaims = 0;
    a->nschemes = 0;
}
static void scheme_add(ls_app_t *a, const char *s) {
    if (a->nschemes >= (int)ARRAY_LEN(a->schemes) || app_has_scheme(a, s)) return;
    usize i = 0;
    for (; s[i] && i + 1 < sizeof a->schemes[0]; i++) a->schemes[a->nschemes][i] = (char)((s[i] >= 'A' && s[i] <= 'Z') ? s[i] + 32 : s[i]);
    a->schemes[a->nschemes][i] = 0;
    a->nschemes++;
}

/* ---------------- apps do sistema */
static void open_in_browser(const char *target) {
    char url[1100];
    if (target[0] == '/') snprintf(url, sizeof url, "file://%s", target); else snprintf(url, sizeof url, "%s", target);
    app_launch(&app_horizonte);
    browser_open_url(url);
}

static ls_app_t *builtin(app_t *app, const char *id, void (*open_file)(const char *), void (*open_url)(const char *)) {
    ls_app_t *a = app_new();
    if (!a) return NULL;
    snprintf(a->id, sizeof a->id, "%s", id);
    snprintf(a->name, sizeof a->name, "%s", app->name);
    snprintf(a->exe, sizeof a->exe, "%s", app->exec);
    a->builtin = app;
    a->open_file = open_file;
    a->open_url = open_url;
    return a;
}
static void bclaim(ls_app_t *a, const char *uti, u8 role, u8 rank) {
    claim_t *c = a ? claim_add(a) : NULL;
    if (!c) return;
    c->uti = strdup(uti);
    c->role = role;
    c->rank = rank;
}

static void register_builtins(void) {
    ls_app_t *a;
    a = builtin(&app_arquivos, "com.apple.finder", finder_open_path, NULL);
    bclaim(a, "public.folder", LS_ROLE_VIEWER, RANK_OWNER);
    a = builtin(&app_horizonte, "com.mos.Horizonte", open_in_browser, open_in_browser);
    if (a) { scheme_add(a, "http"); scheme_add(a, "https"); }
    bclaim(a, "public.html", LS_ROLE_VIEWER, RANK_DEFAULT);
    bclaim(a, "public.xhtml", LS_ROLE_VIEWER, RANK_DEFAULT);
    bclaim(a, "com.apple.web-internet-location", LS_ROLE_VIEWER, RANK_DEFAULT);
    bclaim(a, "public.png", LS_ROLE_VIEWER, RANK_ALTERNATE);
    bclaim(a, "public.jpeg", LS_ROLE_VIEWER, RANK_ALTERNATE);
    bclaim(a, "com.compuserve.gif", LS_ROLE_VIEWER, RANK_ALTERNATE);
    bclaim(a, "public.plain-text", LS_ROLE_VIEWER, RANK_ALTERNATE);
    a = builtin(&app_visualizador, "com.apple.Preview", viewer_open_file, NULL);
    bclaim(a, "public.png", LS_ROLE_VIEWER, RANK_DEFAULT);
    bclaim(a, "public.jpeg", LS_ROLE_VIEWER, RANK_DEFAULT);
    bclaim(a, "com.compuserve.gif", LS_ROLE_VIEWER, RANK_DEFAULT);
    bclaim(a, "com.microsoft.bmp", LS_ROLE_VIEWER, RANK_DEFAULT);
    bclaim(a, "public.pbm", LS_ROLE_VIEWER, RANK_DEFAULT);
    a = builtin(&app_quicktime, "com.apple.QuickTimePlayerX", quicktime_open_file, NULL);
    bclaim(a, "com.microsoft.waveform-audio", LS_ROLE_VIEWER, RANK_DEFAULT);
    bclaim(a, "com.compuserve.gif", LS_ROLE_VIEWER, RANK_ALTERNATE);
    a = builtin(&app_editor, "com.apple.TextEdit", editor_open_file, NULL);
    bclaim(a, "public.plain-text", LS_ROLE_EDITOR, RANK_DEFAULT);
    bclaim(a, "public.rtf", LS_ROLE_EDITOR, RANK_DEFAULT);
    bclaim(a, "public.html", LS_ROLE_EDITOR, RANK_ALTERNATE);
    bclaim(a, "public.xml", LS_ROLE_EDITOR, RANK_ALTERNATE);
    bclaim(a, "public.text", LS_ROLE_EDITOR, RANK_ALTERNATE);
    bclaim(a, "com.apple.property-list", LS_ROLE_EDITOR, RANK_ALTERNATE);
    a = builtin(&app_livros, "com.apple.iBooksX", books_open_file, NULL);
    bclaim(a, "org.idpf.epub-container", LS_ROLE_VIEWER, RANK_OWNER);
    a = builtin(&app_calendario, "com.apple.iCal", calendar_import_file, NULL);
    bclaim(a, "com.apple.ical.ics", LS_ROLE_EDITOR, RANK_OWNER);
    a = builtin(&app_contatos, "com.apple.AddressBook", contacts_import_file, NULL);
    bclaim(a, "public.vcard", LS_ROLE_EDITOR, RANK_OWNER);
    a = builtin(&app_script, "com.apple.ScriptEditor2", script_open_file, NULL);
    bclaim(a, "com.apple.applescript.text", LS_ROLE_EDITOR, RANK_OWNER);
    bclaim(a, "com.netscape.javascript-source", LS_ROLE_EDITOR, RANK_ALTERNATE);
    a = builtin(&app_mail, "com.apple.mail", NULL, mail_open_url);
    if (a) scheme_add(a, "mailto");
    a = builtin(&app_compactacao, "com.apple.archiveutility", archive_open_file, NULL);
    bclaim(a, "public.zip-archive", LS_ROLE_VIEWER, RANK_DEFAULT);
    bclaim(a, "public.tar-archive", LS_ROLE_VIEWER, RANK_DEFAULT);
    bclaim(a, "org.gnu.gnu-zip-archive", LS_ROLE_VIEWER, RANK_DEFAULT);
    bclaim(a, "org.gnu.gnu-zip-tar-archive", LS_ROLE_VIEWER, RANK_DEFAULT);
    bclaim(a, "public.bzip2-archive", LS_ROLE_VIEWER, RANK_DEFAULT);
    bclaim(a, "org.tukaani.xz-archive", LS_ROLE_VIEWER, RANK_DEFAULT);
    a = builtin(&app_dimounter, "com.apple.DiskImageMounter", diskimage_open_file, NULL);
    bclaim(a, "com.apple.disk-image-udif", LS_ROLE_VIEWER, RANK_OWNER);
    bclaim(a, "public.disk-image", LS_ROLE_VIEWER, RANK_OWNER);
    bclaim(a, "public.iso-image", LS_ROLE_VIEWER, RANK_OWNER);
    /* os outros apps do sistema existem para "abrir -b" e para o NSWorkspace achá-los, sem tipos */
    struct { app_t *a; const char *id; } plain[] = {
        {&app_terminal, "com.apple.Terminal"}, {&app_preferencias, "com.apple.systempreferences"}, {&app_notas, "com.apple.Notes"},
        {&app_calculadora, "com.apple.calculator"}, {&app_musica, "com.apple.Music"}, {&app_fotos, "com.apple.Photos"},
        {&app_mapas, "com.apple.Maps"}, {&app_atividade, "com.apple.ActivityMonitor"}, {&app_console, "com.apple.Console"},
        {&app_lembretes, "com.apple.reminders"}, {&app_fontes, "com.apple.FontBook"}, {&app_disco, "com.apple.DiskUtility"},
    };
    for (u32 i = 0; i < ARRAY_LEN(plain); i++) builtin(plain[i].a, plain[i].id, NULL, NULL);
}

/* ---------------- apps do macOS: Info.plist */
static u8 parse_role(const char *r) {
    if (!r) return LS_ROLE_VIEWER;
    if (!strcasecmp(r, "Editor")) return LS_ROLE_EDITOR;
    if (!strcasecmp(r, "Viewer")) return LS_ROLE_VIEWER;
    if (!strcasecmp(r, "Shell")) return LS_ROLE_SHELL;
    if (!strcasecmp(r, "None") || !strcasecmp(r, "QLGenerator")) return LS_ROLE_NONE;
    return LS_ROLE_VIEWER;
}
static u8 parse_rank(const char *r) {
    if (!r) return RANK_DEFAULT;
    if (!strcasecmp(r, "Owner")) return RANK_OWNER;
    if (!strcasecmp(r, "Alternate")) return RANK_ALTERNATE;
    if (!strcasecmp(r, "None")) return RANK_NONE;
    return RANK_DEFAULT;
}

typedef struct { ls_app_t *a; const pl_t *doc; u8 role, rank; bool by_type; } dctx_t;
static void claim_fill(claim_t *c, dctx_t *d) {
    c->role = d->role;
    c->rank = d->rank;
    const char *nm = pl_str(d->doc, "CFBundleTypeName"), *ic = pl_str(d->doc, "CFBundleTypeIconFile");
    if (nm) c->name = strdup(nm);
    if (ic) c->icon = strdup(ic);
}
static void add_type_claim(const char *s, void *ctx) {
    dctx_t *d = ctx;
    claim_t *c = claim_add(d->a);
    if (!c) return;
    c->uti = strdup(s);
    claim_fill(c, d);
}
static void add_ext_claim(const char *s, void *ctx) {
    dctx_t *d = ctx;
    claim_t *c = claim_add(d->a);
    if (!c) return;
    const char *e = s[0] == '.' ? s + 1 : s;
    usize i = 0;
    for (; e[i] && i + 1 < sizeof c->ext; i++) c->ext[i] = (char)((e[i] >= 'A' && e[i] <= 'Z') ? e[i] + 32 : e[i]);
    c->ext[i] = 0;
    claim_fill(c, d);
}
static void add_mime_claim(const char *s, void *ctx) {
    const char *t = ls_type_for_tag("public.mime-type", s, NULL);
    if (t) add_type_claim(t, ctx);
}
static void add_scheme(const char *s, void *ctx) { scheme_add(ctx, s); }

/* string ou array de strings → lista separada por espaço (alocada) */
typedef struct { char buf[1024]; usize n; } wl_t;
static void wl_add(const char *s, void *ctx) {
    wl_t *w = ctx;
    usize l = strlen(s);
    if (w->n + l + 2 >= sizeof w->buf) return;
    if (w->n) w->buf[w->n++] = ' ';
    for (usize i = 0; i < l; i++) w->buf[w->n++] = s[i] == ' ' ? '_' : s[i];
    w->buf[w->n] = 0;
}

static void types_drop_owner(ls_app_t *a) {
    int k = 0;
    for (int i = 0; i < ndyn; i++) {
        if (dyn_types[i].owner == a) {
            kfree((void *)dyn_types[i].id); kfree((void *)dyn_types[i].desc); kfree((void *)dyn_types[i].parents);
            kfree((void *)dyn_types[i].exts); kfree((void *)dyn_types[i].mimes); kfree((void *)dyn_types[i].icon);
            continue;
        }
        dyn_types[k++] = dyn_types[i];
    }
    ndyn = k;
}

static void types_from(ls_app_t *a, const pl_t *arr, bool exported) {
    for (pl_t *t = arr && arr->type == PL_ARRAY ? arr->child : NULL; t && ndyn < MAX_DYN_TYPES; t = t->next) {
        const char *id = pl_str(t, "UTTypeIdentifier");
        if (!id || !id[0]) continue;
        bool sys = false;
        for (u32 i = 0; i < ARRAY_LEN(sys_types); i++) if (!strcasecmp(sys_types[i].id, id)) sys = true;
        if (sys) continue;                                   /* o sistema já define */
        wl_t par = {0}, ex = {0}, mi = {0};
        pl_each_str(pl_get(t, "UTTypeConformsTo"), wl_add, &par);
        const pl_t *spec = pl_get(t, "UTTypeTagSpecification");
        pl_each_str(pl_get(spec, "public.filename-extension"), wl_add, &ex);
        pl_each_str(pl_get(spec, "public.mime-type"), wl_add, &mi);
        uti_t *u = &dyn_types[ndyn++];
        memset(u, 0, sizeof *u);
        u->id = strdup(id);
        u->desc = strdup(pl_str(t, "UTTypeDescription") ? pl_str(t, "UTTypeDescription") : "");
        u->parents = strdup(par.n ? par.buf : "public.data");
        u->exts = strdup(ex.buf);
        u->mimes = strdup(mi.buf);
        const char *ic = pl_str(t, "UTTypeIconFile");
        u->icon = ic ? strdup(ic) : NULL;
        u->owner = a;
        u->exported = exported;
    }
}

static void stem_of(const char *path, char *out, usize cap) {
    const char *b = strrchr(path, '/');
    snprintf(out, cap, "%s", b ? b + 1 : path);
    if (ends_with(out, ".app")) out[strlen(out) - 4] = 0;
}

/* lê (ou relê) o .app; NULL se não for um bundle com Info.plist */
static ls_app_t *register_bundle(const char *path) {
    char ip[400];
    snprintf(ip, sizeof ip, "%s/Contents/Info.plist", path);
    vnode_t *v = vfs_lookup(ip);
    if (!v || v->dir || !v->data) return NULL;
    ls_app_t *a = NULL;
    for (int i = 0; i < napps; i++) if (!strcmp(apps[i]->path, path)) a = apps[i];
    if (a && !a->gone && a->plist_size == v->size) return a;
    pl_t *info = pl_parse(v->data, v->size);
    if (!info || info->type != PL_DICT) { pl_free(info); return NULL; }
    if (!a) { a = app_new(); if (!a) { pl_free(info); return NULL; } }
    else { claims_clear(a); types_drop_owner(a); }
    a->gone = false;
    a->plist_size = v->size;
    snprintf(a->path, sizeof a->path, "%s", path);
    char stem[128];
    stem_of(path, stem, sizeof stem);
    const char *s = pl_str(info, "CFBundleIdentifier");
    if (s && s[0]) snprintf(a->id, sizeof a->id, "%s", s); else snprintf(a->id, sizeof a->id, "mos.app.%s", stem);
    snprintf(a->name, sizeof a->name, "%s", stem);           /* o Finder mostra o nome do arquivo */
    s = pl_str(info, "CFBundleExecutable");
    snprintf(a->exe, sizeof a->exe, "%s", s && s[0] ? s : stem);
    /* tipos que o app define primeiro: as extensões das declarações dele valem para os documentos */
    types_from(a, pl_get(info, "UTExportedTypeDeclarations"), true);
    types_from(a, pl_get(info, "UTImportedTypeDeclarations"), false);
    const pl_t *docs = pl_get(info, "CFBundleDocumentTypes");
    for (pl_t *d = docs && docs->type == PL_ARRAY ? docs->child : NULL; d; d = d->next) {
        dctx_t c = {a, d, parse_role(pl_str(d, "CFBundleTypeRole")), parse_rank(pl_str(d, "LSHandlerRank")), false};
        const pl_t *it = pl_get(d, "LSItemContentTypes");
        if (it && (it->type == PL_STRING || it->count)) { pl_each_str(it, add_type_claim, &c); continue; }
        /* formato antigo: extensões e MIME (as extensões conhecidas viram o UTI) */
        const pl_t *ex = pl_get(d, "CFBundleTypeExtensions");
        pl_each_str(ex, add_ext_claim, &c);
        pl_each_str(pl_get(d, "CFBundleTypeMIMETypes"), add_mime_claim, &c);
        /* "****" só é "qualquer tipo" numa entrada sem extensões nem MIME: o VS Code e o Cursor põem "****" ao lado da
         * extensão em todas as 66 entradas, e o macOS não os torna o app de tudo */
        const pl_t *os = pl_get(d, "CFBundleTypeOSTypes");
        bool specific = (ex && (ex->type == PL_STRING || ex->count)) || pl_get(d, "CFBundleTypeMIMETypes");
        if (!specific && os && os->type == PL_ARRAY)
            for (pl_t *o = os->child; o; o = o->next) if (o->type == PL_STRING && !strcmp(o->str, "****")) add_ext_claim("*", &c);
        if (pl_get(d, "LSTypeIsPackage") && pl_get(d, "LSTypeIsPackage")->num && !ex) add_type_claim("com.apple.package", &c);
    }
    const pl_t *urls = pl_get(info, "CFBundleURLTypes");
    for (pl_t *u = urls && urls->type == PL_ARRAY ? urls->child : NULL; u; u = u->next) pl_each_str(pl_get(u, "CFBundleURLSchemes"), add_scheme, a);
    pl_free(info);
    ls_gen++;
    kprintf("ls: %s (%s): %d tipos de documento, %d esquemas\n", a->name, a->id, a->nclaims, a->nschemes);
    return a;
}

ls_app_t *ls_app_by_path(const char *path) {
    if (!path || !path[0]) return NULL;
    char p[320];
    snprintf(p, sizeof p, "%s", path);
    usize l = strlen(p);
    while (l > 1 && p[l - 1] == '/') p[--l] = 0;
    for (int i = 0; i < napps; i++) if (!apps[i]->gone && !strcmp(apps[i]->path, p)) return apps[i];
    return register_bundle(p);
}

/* ================================================================ escolhas do usuário */
typedef struct { u8 kind; char key[128]; u32 roles; char id[128]; } pref_t;   /* kind: 0 tipo, 1 esquema */
typedef struct { char path[320]; char id[128]; } fpref_t;
static pref_t prefs[256];
static int nprefs;
static fpref_t fprefs[128];
static int nfprefs;
static char prefs_home[256];
static bool prefs_loaded;

static const char *prefs_file(void) { return home_sub("/Library/Preferences/com.apple.LaunchServices/com.apple.launchservices.secure.plist"); }

static void prefs_load(void) {
    const char *home = home_sub("");
    if (prefs_loaded && !strcmp(home, prefs_home)) return;
    prefs_loaded = true;
    ls_gen++;
    snprintf(prefs_home, sizeof prefs_home, "%s", home);
    nprefs = nfprefs = 0;
    vnode_t *v = vfs_lookup(prefs_file());
    pl_t *root = v && !v->dir && v->data ? pl_parse(v->data, v->size) : NULL;
    const pl_t *hs = pl_get(root, "LSHandlers");
    for (pl_t *h = hs && hs->type == PL_ARRAY ? hs->child : NULL; h && nprefs < (int)ARRAY_LEN(prefs); h = h->next) {
        const char *ct = pl_str(h, "LSHandlerContentType"), *sch = pl_str(h, "LSHandlerURLScheme");
        const char *tag = pl_str(h, "LSHandlerContentTag");
        if (!ct && tag) ct = ls_type_for_tag(pl_str(h, "LSHandlerContentTagClass"), tag, NULL);
        if (!ct && !sch) continue;
        static const struct { const char *k; u32 r; } roles[] = {
            {"LSHandlerRoleAll", LS_ROLE_ALL}, {"LSHandlerRoleViewer", LS_ROLE_VIEWER}, {"LSHandlerRoleEditor", LS_ROLE_EDITOR}, {"LSHandlerRoleShell", LS_ROLE_SHELL}};
        for (u32 r = 0; r < ARRAY_LEN(roles) && nprefs < (int)ARRAY_LEN(prefs); r++) {
            const char *id = pl_str(h, roles[r].k);
            if (!id || !id[0] || !strcmp(id, "-")) continue;
            pref_t *p = &prefs[nprefs++];
            p->kind = sch ? 1 : 0;
            snprintf(p->key, sizeof p->key, "%s", sch ? sch : ct);
            if (sch) for (char *c = p->key; *c; c++) if (*c >= 'A' && *c <= 'Z') *c += 32;
            p->roles = roles[r].r;
            snprintf(p->id, sizeof p->id, "%s", id);
        }
    }
    const pl_t *ow = pl_get(root, "MOSOpenWith");     /* "Abrir com" de um arquivo só (no macOS: atributo estendido) */
    for (pl_t *e = ow && ow->type == PL_DICT ? ow->child : NULL; e && nfprefs < (int)ARRAY_LEN(fprefs); e = e->next) {
        if (e->type != PL_STRING || !e->key) continue;
        snprintf(fprefs[nfprefs].path, sizeof fprefs[0].path, "%s", e->key);
        snprintf(fprefs[nfprefs].id, sizeof fprefs[0].id, "%s", e->str);
        nfprefs++;
    }
    pl_free(root);
}

static void xml_esc(sbuf_t *b, const char *s) {
    for (; *s; s++) {
        if (*s == '&') sb_puts(b, "&amp;"); else if (*s == '<') sb_puts(b, "&lt;"); else if (*s == '>') sb_puts(b, "&gt;"); else sb_putc(b, *s);
    }
}
static const char *role_key(u32 r) {
    return r == LS_ROLE_VIEWER ? "LSHandlerRoleViewer" : r == LS_ROLE_EDITOR ? "LSHandlerRoleEditor" : r == LS_ROLE_SHELL ? "LSHandlerRoleShell" : "LSHandlerRoleAll";
}
static void prefs_save(void) {
    ls_gen++;
    sbuf_t b = {0};
    sb_puts(&b, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
                "<plist version=\"1.0\">\n<dict>\n\t<key>LSHandlers</key>\n\t<array>\n");
    for (int i = 0; i < nprefs; i++) {
        pref_t *p = &prefs[i];
        sb_puts(&b, "\t\t<dict>\n");
        char e[64];
        if (p->kind == 0 && is_dyn(p->key) && dyn_ext(p->key, e, sizeof e)) {
            sb_puts(&b, "\t\t\t<key>LSHandlerContentTag</key>\n\t\t\t<string>"); xml_esc(&b, e);
            sb_puts(&b, "</string>\n\t\t\t<key>LSHandlerContentTagClass</key>\n\t\t\t<string>public.filename-extension</string>\n");
        } else {
            sb_printf(&b, "\t\t\t<key>%s</key>\n\t\t\t<string>", p->kind ? "LSHandlerURLScheme" : "LSHandlerContentType");
            xml_esc(&b, p->key);
            sb_puts(&b, "</string>\n");
        }
        sb_printf(&b, "\t\t\t<key>%s</key>\n\t\t\t<string>", role_key(p->roles));
        xml_esc(&b, p->id);
        sb_printf(&b, "</string>\n\t\t\t<key>LSHandlerPreferredVersions</key>\n\t\t\t<dict>\n\t\t\t\t<key>%s</key>\n\t\t\t\t<string>-</string>\n\t\t\t</dict>\n\t\t</dict>\n", role_key(p->roles));
    }
    sb_puts(&b, "\t</array>\n");
    if (nfprefs) {
        sb_puts(&b, "\t<key>MOSOpenWith</key>\n\t<dict>\n");
        for (int i = 0; i < nfprefs; i++) {
            sb_puts(&b, "\t\t<key>"); xml_esc(&b, fprefs[i].path);
            sb_puts(&b, "</key>\n\t\t<string>"); xml_esc(&b, fprefs[i].id); sb_puts(&b, "</string>\n");
        }
        sb_puts(&b, "\t</dict>\n");
    }
    sb_puts(&b, "</dict>\n</plist>\n");
    vfs_mkdir(home_sub("/Library/Preferences/com.apple.LaunchServices"));
    vfs_write_file(prefs_file(), b.p, (usize)b.len);
    sb_free(&b);
}

static ls_app_t *pref_lookup(u8 kind, const char *key, u32 roles) {
    prefs_load();
    for (int pass = 0; pass < 2; pass++)          /* o papel exato primeiro, depois "todos" */
        for (int i = nprefs - 1; i >= 0; i--) {
            pref_t *p = &prefs[i];
            if (p->kind != kind || strcasecmp(p->key, key)) continue;
            if (pass == 0 ? p->roles != roles : !(p->roles & roles)) continue;
            ls_app_t *a = ls_app_by_id(p->id);
            if (a) return a;                       /* app desinstalado: a escolha fica sem efeito */
        }
    return NULL;
}
static void pref_set(u8 kind, const char *key, u32 roles, const char *id) {
    prefs_load();
    int k = 0;
    for (int i = 0; i < nprefs; i++) if (!(prefs[i].kind == kind && !strcasecmp(prefs[i].key, key) && (prefs[i].roles & roles))) prefs[k++] = prefs[i];
    nprefs = k;
    if (id && nprefs < (int)ARRAY_LEN(prefs)) {
        pref_t *p = &prefs[nprefs++];
        p->kind = kind;
        snprintf(p->key, sizeof p->key, "%s", key);
        if (kind == 1) for (char *c = p->key; *c; c++) if (*c >= 'A' && *c <= 'Z') *c += 32;
        p->roles = roles;
        snprintf(p->id, sizeof p->id, "%s", id);
    }
    prefs_save();
}

/* ================================================================ quem abre */
static bool generic_type(const char *u) {
    return !strcasecmp(u, "public.data") || !strcasecmp(u, "public.item") || !strcasecmp(u, "public.content");
}

/* pontos da declaração para o tipo (0 = não serve). >= 100: específica; 1: genérica ("*", public.data) */
static int claim_score(const claim_t *c, const char *uti, const char *ext, u32 roles) {
    if (c->rank == RANK_NONE || c->role == LS_ROLE_NONE || !(c->role & roles)) return 0;
    if (c->uti) {
        if (generic_type(c->uti)) return ls_conforms(uti, c->uti) ? 1 : 0;
        int d = conform_depth(uti, c->uti, 0);
        return d < 0 ? 0 : d == 0 ? 300 : MAX(100, 200 - d * 10);
    }
    if (!strcmp(c->ext, "*")) return 1;
    return ext && ext[0] && !strcasecmp(c->ext, ext) ? 290 : 0;
}

typedef struct { ls_app_t *a; i64 score; const claim_t *c; } cand_t;

static int rank_type(const char *uti, const char *ext, u32 roles, cand_t *out, int max, bool with_generic) {
    int n = 0;
    for (int i = 0; i < napps; i++) {
        ls_app_t *a = apps[i];
        if (a->gone) continue;
        int best = 0;
        const claim_t *bc = NULL;
        u8 brank = 0;
        i64 bestv = 0;
        for (int k = 0; k < a->nclaims; k++) {
            int s = claim_score(&a->claims[k], uti, ext, roles);
            if (!s) continue;
            i64 v = (s == 1 ? 0 : (i64)a->claims[k].rank * 1000) + s;   /* a genérica não vence a específica */
            if (v > bestv) { bestv = v; best = s; bc = &a->claims[k]; brank = a->claims[k].rank; }
        }
        if (!bc || (best == 1 && !with_generic)) continue;
        if (n == max) break;
        /* posto, especificidade, app do sistema, ordem de registro */
        /* posto só vale entre declarações que citam o tipo: a genérica ("*", public.data) fica abaixo de todas */
        i64 sc = (best == 1 ? 0 : (i64)brank * 100000000) + (i64)(best >= 290 ? 3 : best >= 100 ? 2 : 1) * 10000000 + (a->builtin ? 1000000 : 0) - a->order;
        out[n++] = (cand_t){a, sc, bc};
    }
    for (int i = 1; i < n; i++)
        for (int j = i; j > 0 && out[j].score > out[j - 1].score; j--) { cand_t t = out[j]; out[j] = out[j - 1]; out[j - 1] = t; }
    return n;
}

/* lista com a escolha do usuário na frente */
static int handlers_type(const char *uti, const char *ext, u32 roles, ls_app_t *pref, ls_app_t **out, int max) {
    cand_t c[MAX_APPS];
    int n = rank_type(uti, ext, roles, c, MAX_APPS, false), k = 0;
    if (pref && k < max) out[k++] = pref;
    for (int i = 0; i < n && k < max; i++) if (c[i].a != pref) out[k++] = c[i].a;
    return k;
}

static const char *file_ext_lower(const char *path) {
    const char *e = path_ext(path);
    char *b = rbuf();
    usize i = 0;
    for (; e[i] && i + 1 < 64; i++) b[i] = (char)((e[i] >= 'A' && e[i] <= 'Z') ? e[i] + 32 : e[i]);
    b[i] = 0;
    return b;
}

static ls_app_t *file_pref(const char *path) {
    prefs_load();
    for (int i = 0; i < nfprefs; i++) if (!strcmp(fprefs[i].path, path)) return ls_app_by_id(fprefs[i].id);
    return NULL;
}

int ls_handlers_for_type(const char *uti, u32 roles, ls_app_t **out, int max) {
    if (!uti) return 0;
    const char *ext = ls_tag_for_type(uti, "public.filename-extension");
    return handlers_type(uti, ext, roles, pref_lookup(0, uti, roles), out, max);
}
ls_app_t *ls_default_for_type(const char *uti, u32 roles) {
    if (!uti) return NULL;
    ls_app_t *p = pref_lookup(0, uti, roles);
    if (p) return p;
    cand_t c[MAX_APPS];
    const char *ext = ls_tag_for_type(uti, "public.filename-extension");
    int n = rank_type(uti, ext, roles, c, MAX_APPS, true);
    return n ? c[0].a : NULL;
}

int ls_handlers_for_path(const char *path, u32 roles, ls_app_t **out, int max) {
    const char *uti = ls_type_of_path(path);
    char u[160];
    snprintf(u, sizeof u, "%s", uti ? uti : "public.data");
    ls_app_t *pref = file_pref(path);
    if (!pref) pref = pref_lookup(0, u, roles);
    if (!pref) {   /* sem escolha: o padrão (que pode ser genérico) na frente */
        cand_t c[MAX_APPS];
        int n = rank_type(u, file_ext_lower(path), roles, c, MAX_APPS, true);
        if (n) pref = c[0].a;
    }
    return handlers_type(u, file_ext_lower(path), roles, pref, out, max);
}

static struct { char path[300]; u32 roles, gen; ls_app_t *a; } dcache[128];

static ls_app_t *default_for_path_slow(const char *path, u32 roles) {
    ls_app_t *p = file_pref(path);
    if (p) return p;
    const char *uti = ls_type_of_path(path);
    char u[160];
    snprintf(u, sizeof u, "%s", uti ? uti : "public.data");
    if ((p = pref_lookup(0, u, roles))) return p;
    cand_t c[MAX_APPS];
    int n = rank_type(u, file_ext_lower(path), roles, c, MAX_APPS, true);
    return n ? c[0].a : NULL;
}
ls_app_t *ls_default_for_path(const char *path, u32 roles) {
    prefs_load();
    u32 h = 5381;
    for (const char *s = path; *s; s++) h = h * 33 + (u8)*s;
    h = (h ^ roles) % ARRAY_LEN(dcache);
    if (dcache[h].gen == ls_gen && dcache[h].roles == roles && !strcmp(dcache[h].path, path)) return dcache[h].a;
    ls_app_t *a = default_for_path_slow(path, roles);
    if (strlen(path) < sizeof dcache[0].path) { snprintf(dcache[h].path, sizeof dcache[h].path, "%s", path); dcache[h].roles = roles; dcache[h].gen = ls_gen; dcache[h].a = a; }
    return a;
}

/* a declaração do app que abre o arquivo (nome do tipo, ícone) */
static const claim_t *claim_for(ls_app_t *a, const char *path) {
    if (!a) return NULL;
    const char *uti = ls_type_of_path(path);
    const char *ext = file_ext_lower(path);
    const claim_t *best = NULL;
    int bs = 0;
    for (int k = 0; k < a->nclaims; k++) {
        int s = claim_score(&a->claims[k], uti, ext, LS_ROLE_ALL);
        if (s > bs) { bs = s; best = &a->claims[k]; }
    }
    return best;
}

const char *ls_kind_for_path(const char *path) {
    const char *uti = ls_type_of_path(path);
    if (!strcmp(uti, "com.apple.application-bundle")) return "Aplicativo";
    if (!strcmp(uti, "public.folder")) return "Pasta";
    if (!strcmp(uti, "public.volume")) return "Volume";
    const uti_t *t = find_type(uti);
    if (t && t->owner && t->desc && t->desc[0]) return t->desc;      /* tipo do app (UTTypeDescription) */
    ls_app_t *a = ls_default_for_path(path, LS_ROLE_ALL);
    const claim_t *c = a && !a->builtin ? claim_for(a, path) : NULL;
    if (c && c->name && c->name[0] && (!t || is_dyn(uti))) return c->name;  /* CFBundleTypeName de quem abre */
    return ls_type_desc(uti);
}

int ls_handlers_for_scheme(const char *scheme, ls_app_t **out, int max) {
    if (!scheme) return 0;
    ls_app_t *pref = ls_default_for_scheme(scheme);
    int k = 0;
    if (pref && k < max) out[k++] = pref;
    for (int pass = 0; pass < 2; pass++)       /* do sistema primeiro, depois na ordem de registro */
        for (int i = 0; i < napps && k < max; i++) {
            ls_app_t *a = apps[i];
            if (a->gone || a == pref || !app_has_scheme(a, scheme) || (pass == 0) != (a->builtin != NULL)) continue;
            out[k++] = a;
        }
    return k;
}
ls_app_t *ls_default_for_scheme(const char *scheme) {
    if (!scheme) return NULL;
    char s[48];
    usize i = 0;
    for (; scheme[i] && i + 1 < sizeof s; i++) s[i] = (char)((scheme[i] >= 'A' && scheme[i] <= 'Z') ? scheme[i] + 32 : scheme[i]);
    s[i] = 0;
    ls_app_t *p = pref_lookup(1, s, LS_ROLE_ALL);
    if (p && app_has_scheme(p, s)) return p;
    if (p && (!strcmp(s, "http") || !strcmp(s, "https"))) return p;
    for (int pass = 0; pass < 2; pass++)
        for (int k = 0; k < napps; k++)
            if (!apps[k]->gone && app_has_scheme(apps[k], s) && (pass == 0) == (apps[k]->builtin != NULL)) return apps[k];
    return NULL;
}
ls_app_t *ls_default_browser(void) { return ls_default_for_scheme("https"); }
int ls_browsers(ls_app_t **out, int max) {
    int n = 0;
    for (int i = 0; i < napps && n < max; i++) if (!apps[i]->gone && ls_app_is_browser(apps[i])) out[n++] = apps[i];
    return n;
}

bool ls_set_default_for_type(const char *uti, u32 roles, ls_app_t *a) {
    if (!uti || !a) return false;
    pref_set(0, uti, roles ? roles : LS_ROLE_ALL, a->id);
    kprintf("ls: %s agora abre com %s\n", uti, a->name);
    return true;
}
bool ls_set_default_for_path_type(const char *path, ls_app_t *a) {
    const char *uti = ls_type_of_path(path);
    char u[160];
    snprintf(u, sizeof u, "%s", uti);
    /* a escolha por arquivo deste item deixa de valer: o tipo inteiro segue a nova */
    prefs_load();
    int k = 0;
    for (int i = 0; i < nfprefs; i++) if (strcmp(fprefs[i].path, path)) fprefs[k++] = fprefs[i];
    nfprefs = k;
    return ls_set_default_for_type(u, LS_ROLE_ALL, a);
}
void ls_set_handler_for_file(const char *path, ls_app_t *a) {
    prefs_load();
    int k = 0;
    for (int i = 0; i < nfprefs; i++) if (strcmp(fprefs[i].path, path)) fprefs[k++] = fprefs[i];
    nfprefs = k;
    if (a && nfprefs < (int)ARRAY_LEN(fprefs)) {
        snprintf(fprefs[nfprefs].path, sizeof fprefs[0].path, "%s", path);
        snprintf(fprefs[nfprefs].id, sizeof fprefs[0].id, "%s", a->id);
        nfprefs++;
    }
    prefs_save();
}
bool ls_set_default_for_scheme(const char *scheme, ls_app_t *a) {
    if (!scheme || !a) return false;
    pref_set(1, scheme, LS_ROLE_ALL, a->id);
    kprintf("ls: esquema %s agora abre com %s\n", scheme, a->name);
    return true;
}
void ls_set_default_browser(ls_app_t *a) {
    if (!a) return;
    pref_set(1, "http", LS_ROLE_ALL, a->id);
    pref_set(1, "https", LS_ROLE_ALL, a->id);
    pref_set(0, "public.html", LS_ROLE_VIEWER, a->id);
    pref_set(0, "public.xhtml", LS_ROLE_ALL, a->id);
    pref_set(0, "com.apple.web-internet-location", LS_ROLE_ALL, a->id);
    kprintf("ls: navegador padrão: %s\n", a->name);
}

static char ask_id[128];
static bool ask_open;
static void browser_answer(int button, void *ctx) {
    (void)ctx;
    ask_open = false;
    if (button == 0) ls_set_default_browser(ls_app_by_id(ask_id));
}
void ls_request_default_for_scheme(const char *scheme, ls_app_t *a) {
    if (!a || !scheme) return;
    bool web = !strcasecmp(scheme, "http") || !strcasecmp(scheme, "https");
    if (!web) { ls_set_default_for_scheme(scheme, a); return; }
    ls_app_t *cur = ls_default_browser();
    if (cur == a) return;
    if (ask_open && !strcmp(ask_id, a->id)) return;    /* o app pede http e https: uma pergunta só */
    ask_open = true;
    /* o macOS pergunta: "Deseja alterar o navegador padrão para “X” ou manter “Y”?" */
    snprintf(ask_id, sizeof ask_id, "%s", a->id);
    static char title[300], b0[160], b1[160];
    snprintf(title, sizeof title, "Deseja alterar o navegador padrão para “%s” ou manter “%s”?", a->name, cur ? cur->name : "Horizonte");
    snprintf(b0, sizeof b0, "Usar “%s”", a->name);
    snprintf(b1, sizeof b1, "Manter “%s”", cur ? cur->name : "Horizonte");
    static const char *btn[2];
    btn[0] = b0; btn[1] = b1;
    alert_show(title, "", btn, 2, browser_answer, NULL);
}

/* ================================================================ ícones de documentos */
surface_t *macho_icns_file(const char *icns_path, int size);   /* bundle.c */
static struct { char key[400]; int size; surface_t *s; bool none; } dicons[48];
static int dicon_next;

surface_t *ls_doc_icon(const char *path, int size) {
    ls_app_t *a = ls_default_for_path(path, LS_ROLE_ALL);
    if (!a || a->builtin || !a->path[0]) return NULL;
    const claim_t *c = claim_for(a, path);
    const char *icon = c ? c->icon : NULL;
    if (!icon) {
        const uti_t *t = find_type(ls_type_of_path(path));
        if (t && t->owner == a) icon = t->icon;
    }
    if (!icon || !icon[0]) return NULL;
    char key[400];
    snprintf(key, sizeof key, "%s/Contents/Resources/%s%s", a->path, icon, ends_with(icon, ".icns") ? "" : ".icns");
    /* só lembra os que não existem/não decodificam; as superfícies são do cache do bundle.c */
    for (u32 i = 0; i < ARRAY_LEN(dicons); i++)
        if (dicons[i].none && dicons[i].size == size && !strcmp(dicons[i].key, key)) return NULL;
    surface_t *s = macho_icns_file(key, size);
    if (!s) {
        int slot = dicon_next++ % (int)ARRAY_LEN(dicons);
        snprintf(dicons[slot].key, sizeof dicons[slot].key, "%s", key);
        dicons[slot].size = size;
        dicons[slot].none = true;
    }
    return s;
}

void ls_app_draw_icon(const ls_app_t *a, surface_t *s, int x, int y, int size) {
    if (!a) return;
    if (a->builtin) { surface_t *ic = icon_for(a->builtin, size); if (ic) gfx_blend(s, ic, x, y, 255); else if (a->builtin->draw_icon) a->builtin->draw_icon(s, x, y, size); return; }
    surface_t *ic = macho_bundle_icon(a->path, size);
    if (ic) { gfx_blend(s, ic, x, y, 255); return; }
    float r = size * 0.225f, m = size * 0.08f;
    gfx_round_rect_grad(s, x + m, y + m, size - 2 * m, size - 2 * m, r, RGB(120, 128, 150), RGB(62, 66, 84));
    text_draw_center(s, font_get(FONT_SEMIBOLD), size * 0.42f, rect_make(x, y, size, size), RGB(255, 255, 255), "⌘");
}

/* ================================================================ abrir */
typedef struct { char svc[160]; char *url; u64 deadline; int pid; } pend_t;
static pend_t pend[16];

static int running_pid(const ls_app_t *a) {
    for (int i = 0; i < proc_count(); i++) {
        int pid, st;
        u64 mem, cpu;
        char nm[64];
        if (!proc_info(i, &pid, nm, sizeof nm, &st, &mem, &cpu)) continue;
        usize l = strlen(nm);
        if (l && !strncmp(nm, a->exe, l) && (a->exe[l] == 0 || l >= 15)) return pid;
    }
    return 0;
}

static bool post_url(const char *svc, const char *url) {
    usize n = strlen(url);
    if (n >= 16000) return false;
    u8 *body = kmalloc(n + 8);
    if (!body) return false;
    u32 len = (u32)n;
    memcpy(body, &len, 4);
    memcpy(body + 4, url, n + 1);
    int r = machipc_post_service(svc, OU_MSG_ID, body, (u32)(n + 5));
    kfree(body);
    return r == 0;
}

static void redacted(const char *url, char *out, usize cap) {
    const char *sep = strstr(url, "://");
    usize stop = sep ? (usize)(sep - url) + 3 : 0, o = 0, i = 0;
    for (; url[i] && o + 1 < cap; i++) {
        if (i >= stop && stop && !strncmp(url, "file", 4)) { if (url[i] == '?') break; }
        else if (i >= stop && (url[i] == '/' || url[i] == '?' || url[i] == '#')) break;
        out[o++] = url[i];
    }
    out[o] = 0;
}

static bool deliver(ls_app_t *a, const char *url, bool web) {
    char svc[200], shown[200];
    snprintf(svc, sizeof svc, OU_PREFIX "%s", a->id);
    redacted(url, shown, sizeof shown);
    int pid = running_pid(a);
    if (post_url(svc, url)) {
        kprintf("ls: %s entregue a %s\n", shown, a->name);
        app_t *proxy = pid > 0 ? compat_gui_app_for_pid(pid) : NULL;
        if (proxy) wm_bring_app(proxy);
        return true;
    }
    if (!pid) {
        /* o navegador recebe a URL na linha de comando (o Chrome abre as URLs do argv) */
        bool argv = web && ls_app_is_browser(a);
        char args[1200] = "";
        if (argv) snprintf(args, sizeof args, "\"%s\"", url);
        pid = apps_run_mac(a->path, args);
        if (pid < 0) return false;
        if (argv) return true;
    }
    for (u32 i = 0; i < ARRAY_LEN(pend); i++) {
        if (pend[i].url) continue;
        usize n = strlen(url);
        pend[i].url = kmalloc(n + 1);
        if (!pend[i].url) return false;
        memcpy(pend[i].url, url, n + 1);
        snprintf(pend[i].svc, sizeof pend[i].svc, "%s", svc);
        pend[i].deadline = timer_ticks() + 120000;
        pend[i].pid = pid;
        kprintf("ls: %s espera %s abrir\n", shown, a->name);
        return true;
    }
    return false;
}

static void pend_tick(void) {
    for (u32 i = 0; i < ARRAY_LEN(pend); i++) {
        if (!pend[i].url) continue;
        bool done = post_url(pend[i].svc, pend[i].url);
        if (done || timer_ticks() > pend[i].deadline || (pend[i].pid > 0 && !proc_alive(pend[i].pid))) {
            if (done) kprintf("ls: documento entregue a %s\n", pend[i].svc + strlen(OU_PREFIX));
            else kprintf("ls: o app (%s) não abriu a tempo para receber o documento\n", pend[i].svc + strlen(OU_PREFIX));
            kfree(pend[i].url);
            pend[i].url = NULL;
        }
    }
}

static void path_to_url(const char *path, char *out, usize cap) {
    usize o = (usize)snprintf(out, cap, "file://");
    for (const u8 *p = (const u8 *)path; *p && o + 4 < cap; p++) {
        u8 c = *p;
        bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || strchr("/-._~!$&'()*+,;=:@", c);
        if (ok) out[o++] = (char)c;
        else o += (usize)snprintf(out + o, cap - o, "%%%02X", c);
    }
    out[o] = 0;
}
static bool url_to_path(const char *url, char *out, usize cap) {
    if (strncasecmp(url, "file://", 7)) return false;
    const char *p = url + 7;
    if (!strncasecmp(p, "localhost/", 10)) p += 9;
    usize o = 0;
    for (; *p && *p != '?' && *p != '#' && o + 1 < cap; p++) {
        if (*p == '%' && p[1] && p[2]) {
            int hi = p[1] <= '9' ? p[1] - '0' : (p[1] | 32) - 'a' + 10, lo = p[2] <= '9' ? p[2] - '0' : (p[2] | 32) - 'a' + 10;
            out[o++] = (char)(hi * 16 + lo);
            p += 2;
        } else out[o++] = *p;
    }
    while (o > 1 && out[o - 1] == '/') o--;
    out[o] = 0;
    return o > 0;
}
static bool ls_url_scheme(const char *u, char *out, usize cap) {
    if (!u || !((u[0] | 32) >= 'a' && (u[0] | 32) <= 'z')) return false;
    usize i = 0;
    for (; u[i] && u[i] != ':'; i++) {
        char c = u[i];
        if (!((c | 32) >= 'a' && (c | 32) <= 'z') && !(c >= '0' && c <= '9') && c != '+' && c != '-' && c != '.') return false;
        if (i + 1 >= cap) return false;
        out[i] = (char)((c >= 'A' && c <= 'Z') ? c + 32 : c);
    }
    if (u[i] != ':' || i < 2) return false;
    out[i] = 0;
    return true;
}

static bool launch_app(ls_app_t *a) {
    if (a->builtin) { app_launch(a->builtin); return true; }
    int pid = running_pid(a);
    app_t *proxy = pid > 0 ? compat_gui_app_for_pid(pid) : NULL;
    if (proxy) { wm_bring_app(proxy); return true; }
    if (pid > 0) return true;
    return apps_run_mac(a->path, "") > 0;
}

static bool open_doc(ls_app_t *a, const char *path) {
    if (a->builtin) {
        if (a->open_file) a->open_file(path);
        else if (a->open_url) { char u[1100]; path_to_url(path, u, sizeof u); a->open_url(u); }
        else app_launch(a->builtin);
        return true;
    }
    char u[1200];
    path_to_url(path, u, sizeof u);
    return deliver(a, u, true);
}

bool ls_open(const char *target, ls_app_t *with) {
    if (!target || !target[0]) return false;
    char sch[48], path[1024];
    if (target[0] != '/' && ls_url_scheme(target, sch, sizeof sch) && strcmp(sch, "file")) {
        ls_app_t *a = with ? with : ls_default_for_scheme(sch);
        if (!a) { kprintf("ls: ninguém abre URLs %s:\n", sch); return false; }
        if (a->builtin) {
            if (a->open_url) a->open_url(target);
            else app_launch(a->builtin);
            return true;
        }
        return deliver(a, target, !strcmp(sch, "http") || !strcmp(sch, "https"));
    }
    if (target[0] == '/') snprintf(path, sizeof path, "%s", target);
    else if (!url_to_path(target, path, sizeof path)) return false;
    vnode_t *n = vfs_lookup(path);
    if (!n) return false;
    const char *uti = ls_type_of_path(path);
    if (!with && !strcmp(uti, "com.apple.application-bundle")) { ls_app_t *a = ls_app_by_path(path); return a && launch_app(a); }
    if (!with && n->dir) { finder_open_path(path); return true; }
    ls_app_t *a = with ? with : ls_default_for_path(path, LS_ROLE_ALL);
    if (!a) return false;
    return open_doc(a, path);
}

/* ================================================================ vigiar /Aplicativos */
static const char *const app_dirs[] = {"/Aplicativos", "/Aplicativos/Utilitários", "/Applications", "/Applications/Utilities"};

static u64 dir_signature(void) {
    u64 h = 1469598103934665603ull;
    for (u32 d = 0; d < ARRAY_LEN(app_dirs); d++) {
        vnode_t *dir = vfs_lookup(app_dirs[d]);
        for (vnode_t *c = dir && dir->dir ? dir->child : NULL; c; c = c->next) {
            if (!c->dir || !ends_with(c->name, ".app")) continue;
            for (const char *p = c->name; *p; p++) h = (h ^ (u8)*p) * 1099511628211ull;
            vnode_t *ct = vfs_child(c, "Contents");
            vnode_t *ip = ct ? vfs_child(ct, "Info.plist") : NULL;
            h = (h ^ (ip ? ip->size + 1 : 0)) * 1099511628211ull;
        }
    }
    return h;
}

static u64 last_sig, last_check;

/* pedidos dos apps (syscall): a interface (alertas, janelas, processos novos) só muda no loop principal */
typedef struct { u8 kind; bool used; char target[1024]; char app[128]; } defer_t;   /* 0 abrir, 1 esquema padrão */
static defer_t dq[8];
static volatile int dq_lock;

static bool defer(u8 kind, const char *target, ls_app_t *a) {
    while (__sync_lock_test_and_set(&dq_lock, 1)) {}
    bool ok = false;
    for (u32 i = 0; i < ARRAY_LEN(dq) && !ok; i++) {
        if (dq[i].used) continue;
        dq[i].kind = kind;
        snprintf(dq[i].target, sizeof dq[i].target, "%s", target ? target : "");
        snprintf(dq[i].app, sizeof dq[i].app, "%s", a ? a->id : "");
        dq[i].used = ok = true;
    }
    __sync_lock_release(&dq_lock);
    return ok;
}
static void run_deferred(void) {
    for (u32 i = 0; i < ARRAY_LEN(dq); i++) {
        if (!dq[i].used) continue;
        defer_t d = dq[i];
        dq[i].used = false;
        ls_app_t *a = d.app[0] ? ls_app_by_id(d.app) : NULL;
        if (d.kind == 1) { if (a) ls_request_default_for_scheme(d.target, a); }
        else if (d.target[0]) { if (!ls_open(d.target, a)) kprintf("ls: não foi possível abrir o pedido de um app\n"); }
        else if (a) launch_app(a);
    }
}
/* o pedido tem quem atenda? (a resposta da syscall; a abertura vem depois) */
static bool can_open(const char *target, ls_app_t *with) {
    if (!target || !target[0]) return with != NULL;
    if (with) return true;
    char sch[48], path[1024];
    if (target[0] != '/' && ls_url_scheme(target, sch, sizeof sch) && strcmp(sch, "file")) return ls_default_for_scheme(sch) != NULL;
    if (target[0] == '/') snprintf(path, sizeof path, "%s", target);
    else if (!url_to_path(target, path, sizeof path)) return false;
    vnode_t *n = vfs_lookup(path);
    if (!n) return false;
    return n->dir || ls_default_for_path(path, LS_ROLE_ALL) != NULL;
}

void ls_rescan(void) {
    bool seen[MAX_APPS] = {0};
    for (u32 d = 0; d < ARRAY_LEN(app_dirs); d++) {
        vnode_t *dir = vfs_lookup(app_dirs[d]);
        for (vnode_t *c = dir && dir->dir ? dir->child : NULL; c; c = c->next) {
            if (!c->dir || !ends_with(c->name, ".app") || !vfs_child(c, "Contents")) continue;
            char p[320];
            snprintf(p, sizeof p, "%s/%s", app_dirs[d], c->name);
            ls_app_t *a = register_bundle(p);
            for (int i = 0; a && i < napps; i++) if (apps[i] == a) seen[i] = true;
        }
    }
    /* .app que saiu das pastas vigiadas (e não está em outro lugar, como um volume montado) */
    for (int i = 0; i < napps; i++) {
        ls_app_t *a = apps[i];
        if (a->builtin || seen[i] || a->gone) continue;
        bool watched = false;
        for (u32 d = 0; d < ARRAY_LEN(app_dirs); d++) { usize l = strlen(app_dirs[d]); if (!strncmp(a->path, app_dirs[d], l) && a->path[l] == '/') watched = true; }
        if (watched && !vfs_lookup(a->path)) { a->gone = true; ls_gen++; claims_clear(a); types_drop_owner(a); kprintf("ls: %s removido\n", a->name); }
    }
    last_sig = dir_signature();
    apps_sync_mac();
}

void ls_init(void) {
    if (napps) return;
    register_builtins();
    ls_rescan();
    prefs_load();
    kprintf("ls: %d apps, %d tipos de apps, navegador padrão %s\n", ls_app_count(), ndyn, ls_default_browser() ? ls_default_browser()->name : "-");
}

void ls_tick(void) {
    u64 now = timer_ticks();
    run_deferred();
    if (now - last_check < 1500) { return; }
    last_check = now;
    pend_tick();
    if (dir_signature() != last_sig) ls_rescan();
}

/* ================================================================ syscall */
static void line(sbuf_t *b, const ls_app_t *a) { sb_printf(b, "%s\t%s\t%s\n", a->id, a->path, a->name); }

i64 ls_syscall(process_t *p, u64 op, u64 ureq) {
    mos_ls_req_t *rq = kmalloc(sizeof *rq);
    if (!rq) return -MOS_ENOMEM;
    if (!uvm_copy_in(p, rq, ureq, sizeof *rq)) { kfree(rq); return -MOS_EFAULT; }
    rq->key[sizeof rq->key - 1] = rq->key2[sizeof rq->key2 - 1] = rq->key3[sizeof rq->key3 - 1] = 0;
    u32 roles = rq->roles ? rq->roles : LS_ROLE_ALL;
    sbuf_t b = {0};
    i64 r = -1;
    ls_app_t *list[64];
    int n;
    switch (op) {
    case MOS_LS_DEFAULT_SCHEME: { ls_app_t *a = ls_default_for_scheme(rq->key); if (a) { line(&b, a); r = 0; } break; }
    case MOS_LS_ALL_SCHEME: n = ls_handlers_for_scheme(rq->key, list, 64); for (int i = 0; i < n; i++) line(&b, list[i]); r = n; break;
    case MOS_LS_DEFAULT_TYPE: { ls_app_t *a = ls_default_for_type(rq->key, roles); if (a) { line(&b, a); r = 0; } break; }
    case MOS_LS_ALL_TYPE: n = ls_handlers_for_type(rq->key, roles, list, 64); for (int i = 0; i < n; i++) line(&b, list[i]); r = n; break;
    case MOS_LS_DEFAULT_FILE: { ls_app_t *a = ls_default_for_path(rq->key, roles); if (a) { line(&b, a); r = 0; } break; }
    case MOS_LS_ALL_FILE: n = ls_handlers_for_path(rq->key, roles, list, 64); for (int i = 0; i < n; i++) line(&b, list[i]); r = n; break;
    case MOS_LS_SET_SCHEME: { ls_app_t *a = ls_app_by_id(rq->key2); if (a && defer(1, rq->key, a)) r = 0; break; }
    case MOS_LS_SET_TYPE: { ls_app_t *a = ls_app_by_id(rq->key2); if (a && ls_set_default_for_type(rq->key, roles, a)) r = 0; break; }
    case MOS_LS_TYPE_FOR_TAG: { const char *t = ls_type_for_tag(rq->key2, rq->key, rq->key3); if (t) { sb_puts(&b, t); r = 0; } break; }
    case MOS_LS_TAG_FOR_TYPE: { const char *t[16]; n = ls_type_tags(rq->key, rq->key2, t, 16); for (int i = 0; i < n; i++) sb_printf(&b, "%s\n", t[i]); r = n; break; }
    case MOS_LS_CONFORMS: r = ls_conforms(rq->key, rq->key2) ? 1 : 0; break;
    case MOS_LS_TYPE_INFO: {
        if (!find_type(rq->key) && !is_dyn(rq->key)) break;
        const char *par[16];
        n = ls_type_parents(rq->key, par, 16);
        sb_printf(&b, "%s\n", ls_type_desc(rq->key));
        for (int i = 0; i < n; i++) sb_printf(&b, "%s%s", i ? " " : "", par[i]);
        r = 0;
        break;
    }
    case MOS_LS_TYPE_OF_FILE: { const char *t = ls_type_of_path(rq->key); if (t && vfs_lookup(rq->key)) { sb_puts(&b, t); r = 0; } break; }
    case MOS_LS_KIND_OF_FILE: if (vfs_lookup(rq->key)) { sb_puts(&b, ls_kind_for_path(rq->key)); r = 0; } break;
    case MOS_LS_APP_BY_ID: { ls_app_t *a = ls_app_by_id(rq->key); if (a) { line(&b, a); r = 0; } break; }
    case MOS_LS_APP_BY_NAME: { ls_app_t *a = ls_app_by_name(rq->key); if (a) { line(&b, a); r = 0; } break; }
    case MOS_LS_APP_BY_PATH: { ls_app_t *a = ls_app_by_path(rq->key); if (a) { line(&b, a); r = 0; } break; }
    case MOS_LS_OPEN: {
        ls_app_t *a = NULL;
        if (rq->key2[0]) {
            a = (rq->flags & 1) ? ls_app_by_path(rq->key2) : ls_app_by_id(rq->key2);
            if (!a) { r = -MOS_ENOENT; break; }
        }
        r = can_open(rq->key, a) && defer(0, rq->key, a) ? 0 : -1;
        break;
    }
    default: r = -MOS_EINVAL;
    }
    usize len = b.p ? MIN((usize)b.len, sizeof rq->out - 1) : 0;
    if (len) memcpy(rq->out, b.p, len);
    rq->out[len] = 0;
    sb_free(&b);
    if (!uvm_copy_out(p, ureq + __builtin_offsetof(mos_ls_req_t, out), rq->out, len + 1)) r = -MOS_EFAULT;
    kfree(rq);
    return r;
}
