/* mOS — núcleo dos painéis de arquivo (NSOpenPanel/NSSavePanel). Sem classes e sem disco: o teste de host
 * compila este arquivo com -DCX_FP_HOST. Ver appkit_panel_core.h e docs/appkit-plano.md (fase 1). */
#ifdef CX_FP_HOST
#include <stdlib.h>
#include <string.h>
#else
#include "sys.h"
#include "libc.h"
void free(void *p);
#endif
#include "appkit_panel_core.h"

/* ================================================================ texto */
static char *dupz(const char *s) {
    size_t n = strlen(s);
    char *d = malloc(n + 1);
    if (d) memcpy(d, s, n + 1);
    return d;
}
static int lower_ascii(int c) { return c >= 'A' && c <= 'Z' ? c + 32 : c; }

/* Latin-1 0xC0..0xFF -> letra base minúscula (0 = manter o caractere) */
static const char latin_fold[65] = "aaaaaaaceeeeiiiidnooooo\0ouuuuyts" "aaaaaaaceeeeiiiidnooooo\0ouuuuyty";

/* lê um "caractere de comparação" em *p e avança. accents: dobra acentos; senão só a caixa. */
static int fold_next(const unsigned char **p, int accents) {
    const unsigned char *s = *p;
    int c = s[0];
    if (c == 0xC3 && s[1] >= 0x80 && s[1] <= 0xBF) {
        int cp = 0xC0 | (s[1] & 0x3F);
        *p = s + 2;
        if (accents && latin_fold[cp - 0xC0]) return latin_fold[cp - 0xC0];
        if (cp >= 0xC0 && cp <= 0xDE && cp != 0xD7) cp += 0x20;   /* À..Þ -> à..þ */
        return 0x100 + cp;
    }
    *p = s + 1;
    return c < 0x80 ? lower_ascii(c) : 0x200 + c;
}

int fp_compare_names(const char *a, const char *b) {
    const unsigned char *p = (const unsigned char *)a, *q = (const unsigned char *)b;
    while (*p && *q) {
        if (*p >= '0' && *p <= '9' && *q >= '0' && *q <= '9') {
            while (*p == '0' && p[1] >= '0' && p[1] <= '9') p++;
            while (*q == '0' && q[1] >= '0' && q[1] <= '9') q++;
            const unsigned char *pe = p, *qe = q;
            while (*pe >= '0' && *pe <= '9') pe++;
            while (*qe >= '0' && *qe <= '9') qe++;
            if (pe - p != qe - q) return (pe - p) < (qe - q) ? -1 : 1;
            for (; p < pe; p++, q++) if (*p != *q) return *p < *q ? -1 : 1;
            continue;
        }
        int x = fold_next(&p, 1), y = fold_next(&q, 1);
        if (x != y) return x < y ? -1 : 1;
    }
    if (*p || *q) return *p ? 1 : -1;
    int r = strcmp(a, b);
    return r < 0 ? -1 : r > 0;
}

/* igualdade sem caixa (como o APFS padrão): acentos contam */
static int case_eq(const char *a, const char *b) {
    const unsigned char *p = (const unsigned char *)a, *q = (const unsigned char *)b;
    while (*p && *q) if (fold_next(&p, 0) != fold_next(&q, 0)) return 0;
    return !*p && !*q;
}
/* b é prefixo de a, sem caixa nem acento */
static int fold_prefix(const char *a, const char *b) {
    const unsigned char *p = (const unsigned char *)a, *q = (const unsigned char *)b;
    while (*q) { if (!*p) return 0; if (fold_next(&p, 1) != fold_next(&q, 1)) return 0; }
    return 1;
}

/* ================================================================ caminhos */
const char *fp_basename(const char *path) {
    const char *s = path, *last = path;
    size_t n = strlen(path);
    while (n > 1 && path[n - 1] == '/') n--;
    for (size_t i = 0; i + 1 < n; i++) if (s[i] == '/') last = s + i + 1;
    return last;
}

int fp_normalize(const char *path, const char *base, const char *home, char *out) {
    char tmp[FP_PATH_MAX * 2];
    size_t n = 0;
    if (!path) path = "";
    if (path[0] == '~' && (path[1] == 0 || path[1] == '/')) {
        const char *h = home && *home ? home : "/";
        size_t hl = strlen(h);
        if (hl >= sizeof tmp) return 0;
        memcpy(tmp, h, hl); n = hl;
        path++;
    } else if (path[0] != '/') {
        const char *b = base && *base ? base : "/";
        size_t bl = strlen(b);
        if (bl >= sizeof tmp) return 0;
        memcpy(tmp, b, bl); n = bl;
        tmp[n++] = '/';
    }
    size_t pl = strlen(path);
    if (n + pl >= sizeof tmp) return 0;
    memcpy(tmp + n, path, pl); n += pl;
    tmp[n] = 0;
    /* componentes */
    size_t o = 0;
    out[0] = 0;
    for (size_t i = 0; i < n;) {
        while (i < n && tmp[i] == '/') i++;
        size_t j = i;
        while (j < n && tmp[j] != '/') j++;
        size_t len = j - i;
        if (len == 0) break;
        if (len == 1 && tmp[i] == '.') { i = j; continue; }
        if (len == 2 && tmp[i] == '.' && tmp[i + 1] == '.') {
            while (o > 0 && out[o - 1] != '/') o--;
            if (o > 0) o--;
            out[o] = 0;
            i = j;
            continue;
        }
        if (o + 1 + len >= FP_PATH_MAX) return 0;
        out[o++] = '/';
        memcpy(out + o, tmp + i, len); o += len;
        out[o] = 0;
        i = j;
    }
    if (o == 0) { out[0] = '/'; out[1] = 0; }
    return 1;
}

int fp_parent(const char *path, char *out) {
    char t[FP_PATH_MAX];
    if (!fp_normalize(path, "/", "/", t)) return 0;
    if (!strcmp(t, "/")) { out[0] = '/'; out[1] = 0; return 0; }
    size_t n = strlen(t);
    while (n > 0 && t[n - 1] != '/') n--;
    if (n <= 1) { out[0] = '/'; out[1] = 0; return 1; }
    memcpy(out, t, n - 1);
    out[n - 1] = 0;
    return 1;
}

int fp_join(const char *dir, const char *name, char *out) {
    size_t a = strlen(dir), b = strlen(name);
    int slash = !(a > 0 && dir[a - 1] == '/');
    if (a + slash + b >= FP_PATH_MAX) return 0;
    memcpy(out, dir, a);
    if (slash) out[a++] = '/';
    memcpy(out + a, name, b + 1);
    return 1;
}

void fp_ext_of(const char *name, char *out, int cap) {
    const char *dot = 0;
    for (const char *s = name; *s; s++) if (*s == '.') dot = s;
    out[0] = 0;
    if (!dot || dot == name || !dot[1]) return;
    int i = 0;
    for (const char *s = dot + 1; *s && i < cap - 1; s++) out[i++] = (char)lower_ascii((unsigned char)*s);
    out[i] = 0;
}

static const char *const pkg_exts[] = {
    "app", "bundle", "framework", "plugin", "kext", "rtfd", "pages", "numbers", "key", "xcodeproj", "xcworkspace",
    "playground", "photoslibrary", "musiclibrary", "logicx", "band", "mpkg", "component", "appex", "prefpane",
    "qlgenerator", "mdimporter", "saver", "wdgt", "xpc", "dsym", "scptd", "sparsebundle", "fcpbundle",
    "imovielibrary", "theater", "xcarchive", "workflow", "action", "bbprojectd",
};
int fp_is_pkg_ext(const char *ext) {
    if (!ext || !*ext) return 0;
    for (unsigned i = 0; i < sizeof pkg_exts / sizeof pkg_exts[0]; i++) if (!strcmp(ext, pkg_exts[i])) return 1;
    return 0;
}

/* ================================================================ tipos */
typedef struct { const char *uti; const char *exts; } uti_map_t;
#define IMG "png jpg jpeg jpe gif tif tiff bmp heic heif webp ico icns jp2 tga psd svg avif exr hdr dng cr2 nef arw"
#define AUD "mp3 m4a aac wav wave aif aiff aifc flac ogg oga opus caf m4b m4r mid midi amr"
#define MOV "mov mp4 m4v avi mkv webm mpg mpeg 3gp wmv flv ts mts"
#define SRC "c h m mm cc cpp cxx hpp hh swift s asm java kt rs go py rb pl js mjs cjs ts tsx jsx php sh zsh bash fish lua sql cs fs"
#define TXT "txt text md markdown log csv tsv json xml yaml yml toml ini conf cfg html htm css rtf tex srt vtt strings plist"
static const uti_map_t uti_map[] = {
    {"public.image", IMG}, {"public.png", "png"}, {"public.jpeg", "jpg jpeg jpe"}, {"public.tiff", "tif tiff"},
    {"com.compuserve.gif", "gif"}, {"com.microsoft.bmp", "bmp"}, {"public.heic", "heic"}, {"public.heif", "heif heic"},
    {"org.webmproject.webp", "webp"}, {"com.microsoft.ico", "ico"}, {"com.apple.icns", "icns"}, {"public.svg-image", "svg"},
    {"public.avif", "avif"}, {"com.adobe.photoshop-image", "psd"}, {"public.camera-raw-image", "dng cr2 nef arw raf orf rw2"},
    {"com.adobe.pdf", "pdf"}, {"com.adobe.postscript", "ps eps"},
    {"public.audio", AUD}, {"public.mp3", "mp3"}, {"public.mpeg-4-audio", "m4a m4b"}, {"com.microsoft.waveform-audio", "wav wave"},
    {"public.aiff-audio", "aif aiff"}, {"public.aifc-audio", "aifc"}, {"org.xiph.flac", "flac"}, {"org.xiph.ogg-audio", "ogg oga"},
    {"public.movie", MOV}, {"public.video", MOV}, {"public.audiovisual-content", AUD " " MOV}, {"com.apple.quicktime-movie", "mov"},
    {"public.mpeg-4", "mp4 m4v"}, {"public.avi", "avi"}, {"org.matroska.mkv", "mkv"}, {"org.webmproject.webm", "webm"},
    {"public.text", TXT " " SRC}, {"public.plain-text", "txt text md markdown log"}, {"public.utf8-plain-text", "txt text md"},
    {"public.utf16-plain-text", "txt text"}, {"net.daringfireball.markdown", "md markdown"},
    {"public.source-code", SRC}, {"public.c-source", "c"}, {"public.c-header", "h"}, {"public.objective-c-source", "m"},
    {"public.c-plus-plus-source", "cc cpp cxx"}, {"public.swift-source", "swift"}, {"public.python-script", "py"},
    {"public.shell-script", "sh zsh bash"}, {"com.netscape.javascript-source", "js mjs cjs"}, {"public.ruby-script", "rb"},
    {"public.html", "html htm"}, {"public.xml", "xml"}, {"public.json", "json"}, {"public.yaml", "yaml yml"},
    {"public.rtf", "rtf"}, {"com.apple.rtfd", "rtfd"}, {"com.apple.flat-rtfd", "rtfd"},
    {"public.comma-separated-values-text", "csv"}, {"public.tab-separated-values-text", "tsv"},
    {"public.delimited-values-text", "csv tsv"}, {"com.apple.property-list", "plist"}, {"com.apple.xml-property-list", "plist"},
    {"public.zip-archive", "zip"}, {"public.archive", "zip tar gz tgz bz2 tbz xz txz 7z rar zst lz4 cpio"},
    {"org.gnu.gnu-zip-archive", "gz tgz"}, {"public.tar-archive", "tar"}, {"public.bzip2-archive", "bz2 tbz"},
    {"com.apple.disk-image", "dmg"}, {"com.apple.disk-image-udif", "dmg"}, {"public.iso-image", "iso"},
    {"com.apple.installer-package-archive", "pkg mpkg"}, {"com.apple.installer-package", "pkg mpkg"},
    {"public.font", "ttf otf ttc dfont woff woff2"}, {"public.truetype-ttf-font", "ttf"}, {"public.opentype-font", "otf"},
    {"org.openxmlformats.wordprocessingml.document", "docx"}, {"org.openxmlformats.spreadsheetml.sheet", "xlsx"},
    {"org.openxmlformats.presentationml.presentation", "pptx"}, {"com.microsoft.word.doc", "doc"},
    {"com.microsoft.excel.xls", "xls"}, {"com.microsoft.powerpoint.ppt", "ppt"},
    {"org.oasis-open.opendocument.text", "odt"}, {"org.oasis-open.opendocument.spreadsheet", "ods"},
    {"com.apple.iwork.pages.pages", "pages"}, {"com.apple.iwork.numbers.numbers", "numbers"}, {"com.apple.iwork.keynote.key", "key"},
    {"public.database", "db sqlite sqlite3"}, {"org.sqlite.sqlite3", "sqlite sqlite3 db"},
    {"public.vcard", "vcf"}, {"com.apple.ical.ics", "ics"}, {"public.calendar-event", "ics"},
    {"public.executable", ""}, {"public.unix-executable", ""}, {"com.apple.mach-o-binary", "dylib"},
    {"com.apple.application", "app"}, {"com.apple.application-bundle", "app"}, {"com.apple.bundle", "bundle app framework plugin"},
    {"com.apple.framework", "framework"}, {"com.apple.plugin", "plugin"}, {"public.log", "log"},
    {"org.khronos.gltf", "gltf glb"}, {"public.3d-content", "usdz usd obj stl gltf glb fbx dae"}, {"com.pixar.universal-scene-description-mobile", "usdz"},
};

static void add_ext(fp_model_t *m, const char *s, size_t n) {
    while (n && *s == '.') { s++; n--; }
    if (!n || n > 31) return;
    char e[32];
    for (size_t i = 0; i < n; i++) e[i] = (char)lower_ascii((unsigned char)s[i]);
    e[n] = 0;
    for (int i = 0; i < m->nexts; i++) if (!strcmp(m->exts[i], e)) return;
    char **nx = realloc(m->exts, sizeof(char *) * (size_t)(m->nexts + 1));
    if (!nx) return;
    m->exts = nx;
    m->exts[m->nexts++] = dupz(e);
}
static void add_ext_list(fp_model_t *m, const char *list) {
    while (*list) {
        while (*list == ' ') list++;
        const char *s = list;
        while (*list && *list != ' ') list++;
        if (list > s) add_ext(m, s, (size_t)(list - s));
    }
}

void fp_set_types(fp_model_t *m, const char *const *types, int n) {
    for (int i = 0; i < m->nexts; i++) free(m->exts[i]);
    free(m->exts);
    m->exts = 0; m->nexts = 0;
    m->types_set = n > 0;
    m->folders_ok = m->pkg_ok = m->all_files = 0;
    for (int i = 0; i < n; i++) {
        const char *t = types[i];
        if (!t || !*t) continue;
        if (!strcmp(t, "public.item") || !strcmp(t, "public.data") || !strcmp(t, "public.content") ||
            !strcmp(t, "public.composite-content") || !strcmp(t, "*") || !strcmp(t, "public.file-url")) { m->all_files = 1; continue; }
        if (!strcmp(t, "public.folder") || !strcmp(t, "public.directory")) { m->folders_ok = 1; continue; }
        if (!strcmp(t, "com.apple.package") || !strcmp(t, "com.apple.application-bundle") || !strcmp(t, "com.apple.application")) m->pkg_ok = 1;
        int found = 0;
        for (unsigned k = 0; k < sizeof uti_map / sizeof uti_map[0]; k++)
            if (!strcmp(t, uti_map[k].uti)) { add_ext_list(m, uti_map[k].exts); found = 1; break; }
        if (found) continue;
        /* sem ponto, ou com ponto inicial: é extensão ("png", ".png"). Com ponto no meio é UTI que não conhecemos
         * (a cola já mandou a extensão preferida do UTType junto). */
        const char *dot = 0;
        for (const char *s = t + (t[0] == '.'); *s; s++) if (*s == '.') dot = s;
        if (!dot) add_ext(m, t, strlen(t));
    }
    if (m->all_files) { for (int i = 0; i < m->nexts; i++) free(m->exts[i]); free(m->exts); m->exts = 0; m->nexts = 0; }
}

int fp_type_ok(const fp_model_t *m, const char *name) {
    if (!m->types_set || m->all_files) return 1;
    char e[32];
    fp_ext_of(name, e, sizeof e);
    if (m->pkg_ok && fp_is_pkg_ext(e)) return 1;
    if (!e[0]) return 0;
    for (int i = 0; i < m->nexts; i++) if (!strcmp(m->exts[i], e)) return 1;
    return 0;
}

/* ================================================================ modelo */
void fp_init(fp_model_t *m, int open_panel, const char *home) {
    memset(m, 0, sizeof *m);
    m->open_panel = open_panel;
    m->can_files = 1;
    m->allow_other_types = 0;
    m->anchor = m->cursor = -1;
    fp_normalize(home && *home ? home : "/", "/", "/", m->home);
    strcpy(m->dir, m->home);
}

static void clear_entries(fp_model_t *m) {
    for (int i = 0; i < m->n; i++) free(m->e[i].name);
    m->n = 0;
}

void fp_free(fp_model_t *m) {
    clear_entries(m);
    free(m->e); free(m->row); free(m->sel);
    for (int i = 0; i < m->nexts; i++) free(m->exts[i]);
    free(m->exts);
    memset(m, 0, sizeof *m);
}

void fp_begin_dir(fp_model_t *m, const char *dir) {
    char t[FP_PATH_MAX];
    if (fp_normalize(dir, m->dir, m->home, t)) strcpy(m->dir, t);
    clear_entries(m);
    m->nrows = 0;
    m->anchor = m->cursor = -1;
    m->ntyped = 0;
}

void fp_add_entry(fp_model_t *m, const char *name, int is_dir, long long mtime, long long size) {
    if (!name || !*name || !strcmp(name, ".") || !strcmp(name, "..")) return;
    if (m->n == m->cap) {
        int nc = m->cap ? m->cap * 2 : 64;
        fp_entry_t *ne = realloc(m->e, sizeof *ne * (size_t)nc);
        if (!ne) return;
        m->e = ne; m->cap = nc;
    }
    fp_entry_t *e = &m->e[m->n];
    memset(e, 0, sizeof *e);
    e->name = dupz(name);
    if (!e->name) return;
    e->is_dir = is_dir != 0;
    e->mtime = mtime;
    e->size = size;
    char ext[32];
    fp_ext_of(name, ext, sizeof ext);
    e->is_pkg = e->is_dir && fp_is_pkg_ext(ext);
    e->hidden = name[0] == '.';
    m->n++;
}

static int navigable(const fp_model_t *m, const fp_entry_t *e) { return e->is_dir && (!e->is_pkg || m->pkgs_as_dirs); }

static void classify(fp_model_t *m, fp_entry_t *e) {
    if (e->vetoed) { e->enabled = 0; return; }
    if (navigable(m, e)) { e->enabled = 1; return; }
    if (!m->open_panel) { e->enabled = 0; return; }
    int ok = m->can_files && fp_type_ok(m, e->name);
    if (!ok && e->is_pkg && m->can_dirs && (!m->types_set || m->folders_ok || m->all_files)) ok = 1;
    e->enabled = ok;
}

static int cmp_rows(const fp_model_t *m, int a, int b) {
    const fp_entry_t *x = &m->e[a], *y = &m->e[b];
    int r = 0;
    switch (m->sort_key) {
    case FP_SORT_DATE: r = x->mtime < y->mtime ? -1 : x->mtime > y->mtime; break;
    case FP_SORT_SIZE: {
        long long sx = navigable(m, x) ? -1 : x->size, sy = navigable(m, y) ? -1 : y->size;
        r = sx < sy ? -1 : sx > sy;
        break;
    }
    case FP_SORT_KIND: {
        int dx = navigable(m, x), dy = navigable(m, y);
        if (dx != dy) { r = dx ? -1 : 1; break; }
        char ex[32], ey[32];
        fp_ext_of(x->name, ex, sizeof ex);
        fp_ext_of(y->name, ey, sizeof ey);
        r = strcmp(ex, ey);
        r = r < 0 ? -1 : r > 0;
        break;
    }
    default: break;
    }
    if (!r) r = fp_compare_names(x->name, y->name);
    return m->sort_desc ? -r : r;
}

/* merge sort estável das linhas */
static void sort_rows(fp_model_t *m) {
    int n = m->nrows;
    if (n < 2) return;
    int *tmp = malloc(sizeof(int) * (size_t)n);
    if (!tmp) return;
    for (int w = 1; w < n; w *= 2) {
        for (int lo = 0; lo < n; lo += 2 * w) {
            int mid = lo + w < n ? lo + w : n, hi = lo + 2 * w < n ? lo + 2 * w : n;
            int i = lo, j = mid, k = lo;
            while (i < mid && j < hi) tmp[k++] = cmp_rows(m, m->row[j], m->row[i]) < 0 ? m->row[j++] : m->row[i++];
            while (i < mid) tmp[k++] = m->row[i++];
            while (j < hi) tmp[k++] = m->row[j++];
        }
        memcpy(m->row, tmp, sizeof(int) * (size_t)n);
    }
    free(tmp);
}

static void rebuild(fp_model_t *m) {
    free(m->row); free(m->sel);
    m->row = malloc(sizeof(int) * (size_t)(m->n ? m->n : 1));
    m->sel = calloc((size_t)(m->n ? m->n : 1), 1);
    m->nrows = 0;
    if (!m->row || !m->sel) return;
    for (int i = 0; i < m->n; i++) {
        classify(m, &m->e[i]);
        if (m->e[i].hidden && !m->show_hidden) continue;
        m->row[m->nrows++] = i;
    }
    sort_rows(m);
}

void fp_end_dir(fp_model_t *m) {
    rebuild(m);
    m->anchor = m->cursor = -1;
}

void fp_refresh(fp_model_t *m) {
    int keep_n = 0, cur = m->cursor >= 0 && m->cursor < m->nrows ? m->row[m->cursor] : -1;
    int anc = m->anchor >= 0 && m->anchor < m->nrows ? m->row[m->anchor] : -1;
    unsigned char *was = calloc((size_t)(m->n ? m->n : 1), 1);
    if (was && m->sel) for (int r = 0; r < m->nrows; r++) if (m->sel[r]) { was[m->row[r]] = 1; keep_n++; }
    rebuild(m);
    m->cursor = m->anchor = -1;
    if (!was) return;
    for (int r = 0; r < m->nrows; r++) {
        int i = m->row[r];
        if (was[i] && m->e[i].enabled) m->sel[r] = 1;
        if (i == cur) m->cursor = r;
        if (i == anc) m->anchor = r;
    }
    free(was);
    (void)keep_n;
}

void fp_set_sort(fp_model_t *m, int key, int desc) { m->sort_key = key; m->sort_desc = desc; fp_refresh(m); }

fp_entry_t *fp_row_entry(const fp_model_t *m, int row) { return row >= 0 && row < m->nrows ? &m->e[m->row[row]] : 0; }
int fp_row_selectable(const fp_model_t *m, int row) { fp_entry_t *e = fp_row_entry(m, row); return e && e->enabled; }

/* ================================================================ seleção */
void fp_select_none(fp_model_t *m) { if (m->sel) memset(m->sel, 0, (size_t)(m->nrows ? m->nrows : 1)); m->anchor = m->cursor = -1; }

static void select_only(fp_model_t *m, int row) {
    fp_select_none(m);
    if (!fp_row_selectable(m, row)) return;
    m->sel[row] = 1;
    m->anchor = m->cursor = row;
}
static void select_range(fp_model_t *m, int a, int b) {
    if (a > b) { int t = a; a = b; b = t; }
    memset(m->sel, 0, (size_t)m->nrows);
    for (int r = a; r <= b; r++) if (fp_row_selectable(m, r)) m->sel[r] = 1;
}

void fp_click(fp_model_t *m, int row, int mods) {
    if (!m->sel) return;
    if (row < 0 || row >= m->nrows) { fp_select_none(m); return; }
    if (!fp_row_selectable(m, row)) return;
    if (!m->multi || !(mods & (FP_MOD_SHIFT | FP_MOD_CMD))) { select_only(m, row); return; }
    if (mods & FP_MOD_CMD) {
        m->sel[row] = !m->sel[row];
        m->anchor = m->cursor = row;
        return;
    }
    if (m->anchor < 0) { select_only(m, row); return; }
    select_range(m, m->anchor, row);
    m->cursor = row;
}

static int step_selectable(const fp_model_t *m, int from, int dir) {
    for (int r = from + dir; r >= 0 && r < m->nrows; r += dir) if (fp_row_selectable(m, r)) return r;
    return -1;
}

void fp_key(fp_model_t *m, int key, int shift, int page_rows) {
    if (!m->sel || !m->nrows) return;
    int cur = m->cursor, to = -1;
    if (page_rows < 1) page_rows = 1;
    switch (key) {
    case FP_KEY_DOWN: to = cur < 0 ? step_selectable(m, -1, 1) : step_selectable(m, cur, 1); break;
    case FP_KEY_UP: to = cur < 0 ? step_selectable(m, m->nrows, -1) : step_selectable(m, cur, -1); break;
    case FP_KEY_HOME: to = step_selectable(m, -1, 1); break;
    case FP_KEY_END: to = step_selectable(m, m->nrows, -1); break;
    case FP_KEY_PAGE_DOWN: {
        int t = (cur < 0 ? -1 : cur) + page_rows;
        if (t >= m->nrows) t = m->nrows - 1;
        to = fp_row_selectable(m, t) ? t : step_selectable(m, t, -1);
        if (to <= cur) to = step_selectable(m, cur, 1);
        break;
    }
    case FP_KEY_PAGE_UP: {
        int t = (cur < 0 ? m->nrows : cur) - page_rows;
        if (t < 0) t = 0;
        to = fp_row_selectable(m, t) ? t : step_selectable(m, t, 1);
        if (to >= cur && cur >= 0) to = step_selectable(m, cur, -1);
        break;
    }
    default: return;
    }
    if (to < 0) return;
    if (shift && m->multi && m->anchor >= 0) { select_range(m, m->anchor, to); m->cursor = to; }
    else select_only(m, to);
}

int fp_select_name(fp_model_t *m, const char *name) {
    for (int r = 0; r < m->nrows; r++) if (case_eq(fp_row_entry(m, r)->name, name)) {
        if (!fp_row_selectable(m, r)) return -1;
        select_only(m, r);
        return r;
    }
    return -1;
}

int fp_type_ahead(fp_model_t *m, const char *utf8, double now) {
    if (!utf8 || !*utf8) return -1;
    if (now - m->typed_at > 1.0) m->ntyped = 0;
    m->typed_at = now;
    size_t l = strlen(utf8);
    if (m->ntyped + (int)l >= (int)sizeof m->typed) return -1;
    memcpy(m->typed + m->ntyped, utf8, l);
    m->ntyped += (int)l;
    m->typed[m->ntyped] = 0;
    /* prefixo; senão o primeiro nome que vem depois na ordem alfabética */
    int best = -1;
    for (int r = 0; r < m->nrows; r++) {
        if (!fp_row_selectable(m, r)) continue;
        if (fold_prefix(fp_row_entry(m, r)->name, m->typed)) { best = r; break; }
    }
    if (best < 0) {
        const char *bn = 0;
        for (int r = 0; r < m->nrows; r++) {
            if (!fp_row_selectable(m, r)) continue;
            const char *nm = fp_row_entry(m, r)->name;
            if (fp_compare_names(nm, m->typed) > 0 && (!bn || fp_compare_names(nm, bn) < 0)) { bn = nm; best = r; }
        }
    }
    if (best >= 0) select_only(m, best);
    return best;
}

int fp_selected_count(const fp_model_t *m) {
    int n = 0;
    if (m->sel) for (int r = 0; r < m->nrows; r++) n += m->sel[r] != 0;
    return n;
}
int fp_selected_row(const fp_model_t *m, int k) {
    if (m->sel) for (int r = 0; r < m->nrows; r++) if (m->sel[r] && k-- == 0) return r;
    return -1;
}

int fp_activate(const fp_model_t *m, int row) {
    fp_entry_t *e = fp_row_entry(m, row);
    if (!e || !e->enabled) return FP_ACT_NONE;
    if (navigable(m, e)) return FP_ACT_ENTER_DIR;
    return m->open_panel ? FP_ACT_CONFIRM : FP_ACT_NONE;
}

int fp_can_confirm(const fp_model_t *m, const char *save_name) {
    if (!m->open_panel) return fp_validate_name(save_name ? save_name : "") == FP_NAME_OK;
    int n = fp_selected_count(m);
    if (n == 0) return m->can_dirs;
    for (int k = 0; k < n; k++) {
        fp_entry_t *e = fp_row_entry(m, fp_selected_row(m, k));
        if (!e || !e->enabled) return 0;
        if (navigable(m, e) && !m->can_dirs) { if (n == 1) return 1; return 0; }  /* uma pasta: o botão entra nela */
    }
    return 1;
}

/* ================================================================ salvar */
int fp_validate_name(const char *name) {
    const char *s = name;
    while (*s == ' ') s++;
    if (!*s) return FP_NAME_EMPTY;
    if (!strcmp(name, ".") || !strcmp(name, "..")) return FP_NAME_RESERVED;
    for (s = name; *s; s++) {
        if (*s == '/') return FP_NAME_SLASH;
        if (*s == ':') return FP_NAME_COLON;
    }
    if (strlen(name) > 255) return FP_NAME_LONG;
    if (name[0] == '.') return FP_NAME_DOT;
    return FP_NAME_OK;
}

int fp_save_name(const fp_model_t *m, const char *name, char *out, int cap) {
    size_t n = strlen(name);
    if ((int)n >= cap) return 0;
    memcpy(out, name, n + 1);
    if (!m->types_set || m->all_files || m->allow_other_types || !m->nexts) return 1;
    char e[32];
    fp_ext_of(name, e, sizeof e);
    for (int i = 0; i < m->nexts; i++) if (!strcmp(e, m->exts[i])) return 1;
    size_t el = strlen(m->exts[0]);
    if ((int)(n + 1 + el) >= cap) return 0;
    out[n] = '.';
    memcpy(out + n + 1, m->exts[0], el + 1);
    return 1;
}

int fp_find(const fp_model_t *m, const char *name) {
    for (int i = 0; i < m->n; i++) if (case_eq(m->e[i].name, name)) return i;
    return -1;
}

void fp_new_folder_name(const fp_model_t *m, const char *base, char *out, int cap) {
    size_t bl = strlen(base);
    if ((int)bl >= cap) bl = (size_t)cap - 1;
    memcpy(out, base, bl); out[bl] = 0;
    for (int k = 2; fp_find(m, out) >= 0 && k < 100000; k++) {
        char num[16]; int nl = 0, v = k;
        char rev[16]; int rl = 0;
        while (v) { rev[rl++] = (char)('0' + v % 10); v /= 10; }
        while (rl) num[nl++] = rev[--rl];
        if ((int)(bl + 1 + (size_t)nl) >= cap) return;
        out[bl] = ' ';
        memcpy(out + bl + 1, num, (size_t)nl);
        out[bl + 1 + (size_t)nl] = 0;
    }
}
