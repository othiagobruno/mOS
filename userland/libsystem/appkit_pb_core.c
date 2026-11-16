/* mOS — núcleo do NSPasteboard (ver appkit_pb_core.h). Teste de host: -DCX_PBK_HOST. */
#ifdef CX_PBK_HOST
#include <string.h>
#else
#include "sys.h"
#include "libc.h"
#endif
#include "appkit_pb_core.h"

static const struct { const char *legacy, *uti; } alias[] = {
    {"NSStringPboardType", "public.utf8-plain-text"}, {"public.plain-text", "public.utf8-plain-text"},
    {"public.text", "public.utf8-plain-text"}, {"NSURLPboardType", "public.url"}, {"Apple URL pasteboard type", "public.url"},
    {"NeXT Rich Text Format v1.0 pasteboard type", "public.rtf"}, {"NSRTFPboardType", "public.rtf"},
    {"NeXT RTFD pasteboard type", "com.apple.flat-rtfd"}, {"NSRTFDPboardType", "com.apple.flat-rtfd"},
    {"Apple HTML pasteboard type", "public.html"}, {"NSHTMLPboardType", "public.html"},
    {"NeXT TIFF v4.0 pasteboard type", "public.tiff"}, {"NSTIFFPboardType", "public.tiff"},
    {"Apple PDF pasteboard type", "com.adobe.pdf"}, {"NSPDFPboardType", "com.adobe.pdf"},
    {"Apple PNG pasteboard type", "public.png"},
    {"NSColor pasteboard type", "com.apple.cocoa.pasteboard.color"}, {"NSColorPboardType", "com.apple.cocoa.pasteboard.color"},
    {"NeXT font pasteboard type", "com.apple.cocoa.pasteboard.character-formatting"},
    {"NeXT ruler pasteboard type", "com.apple.cocoa.pasteboard.paragraph-formatting"},
    {"NeXT tabular text pasteboard type", "public.utf8-tab-separated-values-text"},
    {"Apple files promise pasteboard type", "com.apple.pasteboard.promised-file-url"},
    {"NSFilesPromisePboardType", "com.apple.pasteboard.promised-file-url"},
};
const char *pbk_canonical(const char *t) {
    if (!t) return "";
    for (unsigned i = 0; i < sizeof alias / sizeof alias[0]; i++) if (!strcmp(t, alias[i].legacy)) return alias[i].uti;
    return t;
}
int pbk_is_filenames(const char *t) { return t && (!strcmp(t, "NSFilenamesPboardType") || !strcmp(t, "Apple files pasteboard type")); }
int pbk_is_text(const char *t) {
    const char *c = pbk_canonical(t);
    return !strcmp(c, "public.utf8-plain-text") || !strcmp(c, "public.utf16-plain-text") || !strcmp(c, "public.html") ||
           !strcmp(c, "public.url") || !strcmp(c, "public.file-url") || !strcmp(c, "public.rtf") ||
           !strcmp(c, "public.utf8-tab-separated-values-text") || !strcmp(c, "public.url-name");
}
int pbk_same(const char *a, const char *b) { return a && b && !strcmp(pbk_canonical(a), pbk_canonical(b)); }

static int keep_char(unsigned char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '/' || c == '-' || c == '_' ||
           c == '.' || c == '~' || c == '!' || c == '$' || c == '&' || c == '\'' || c == '(' || c == ')' || c == '*' ||
           c == '+' || c == ',' || c == ';' || c == '=' || c == ':' || c == '@';
}
int pbk_path_to_url(const char *path, char *out, int cap) {
    static const char hex[] = "0123456789ABCDEF";
    int n = 0;
    const char *pre = "file://";
    if (!path || path[0] != '/' || cap < 9) return 0;
    for (const char *p = pre; *p; p++) out[n++] = *p;
    for (const unsigned char *s = (const unsigned char *)path; *s; s++) {
        if (keep_char(*s)) { if (n + 1 >= cap) return 0; out[n++] = (char)*s; }
        else { if (n + 3 >= cap) return 0; out[n++] = '%'; out[n++] = hex[*s >> 4]; out[n++] = hex[*s & 15]; }
    }
    out[n] = 0;
    return 1;
}
static int hexv(char c) { return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1; }
int pbk_url_to_path(const char *url, char *out, int cap) {
    if (!url || strncmp(url, "file://", 7)) return 0;
    const char *s = url + 7;
    if (!strncmp(s, "localhost/", 10)) s += 9;
    if (*s != '/') return 0;
    int n = 0;
    for (; *s && *s != '?' && *s != '#'; s++) {
        char c = *s;
        if (c == '%' && hexv(s[1]) >= 0 && hexv(s[2]) >= 0) { c = (char)(hexv(s[1]) * 16 + hexv(s[2])); s += 2; }
        if (n + 1 >= cap) return 0;
        out[n++] = c;
    }
    while (n > 1 && out[n - 1] == '/') n--;   /* pasta: sem a barra final */
    out[n] = 0;
    return 1;
}
