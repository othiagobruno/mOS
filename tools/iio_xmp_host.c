/* iio_xmp.c no host: parse de um pacote no estilo Adobe/exiftool, caminhos, ida e volta e embutir em arquivos.
 *   gcc -O1 -DXMP_HOST -Iuserland/libsystem tools/iio_xmp_host.c userland/libsystem/iio_xmp.c -o /tmp/iio_xmp_host
 *   /tmp/iio_xmp_host <dir>   (o .py cria as imagens com o PIL e confere o que o PIL lê de volta) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "iio_xmp.h"
static int fails;
#define CHECK(c, m) do { int ok_ = (c); printf("%s %s\n", ok_ ? "ok  " : "FAIL", m); if (!ok_) fails++; } while (0)
static const char *SAMPLE =
    "<?xpacket begin='\xEF\xBB\xBF' id='W5M0MpCehiHzreSzNTczkc9d'?>\n"
    "<x:xmpmeta xmlns:x='adobe:ns:meta/' x:xmptk='Image::ExifTool 12.40'>\n"
    "<rdf:RDF xmlns:rdf='http://www.w3.org/1999/02/22-rdf-syntax-ns#'>\n"
    " <rdf:Description rdf:about='' xmlns:xmp='http://ns.adobe.com/xap/1.0/' xmp:CreatorTool='Câmera &amp; Cia' xmp:Rating='4'/>\n"
    " <rdf:Description rdf:about='' xmlns:dc='http://purl.org/dc/elements/1.1/'>\n"
    "  <dc:title><rdf:Alt><rdf:li xml:lang='x-default'>Pôr do sol</rdf:li><rdf:li xml:lang='en'>Sunset</rdf:li></rdf:Alt></dc:title>\n"
    "  <dc:subject><rdf:Bag><rdf:li>praia</rdf:li><rdf:li>mar</rdf:li><rdf:li>&lt;tag&gt;</rdf:li></rdf:Bag></dc:subject>\n"
    "  <dc:creator><rdf:Seq><rdf:li>Ana</rdf:li></rdf:Seq></dc:creator>\n"
    "  <dc:rights><![CDATA[© 2026 & todos]]></dc:rights>\n"
    " </rdf:Description>\n"
    " <rdf:Description rdf:about='' xmlns:exif='http://ns.adobe.com/exif/1.0/' xmlns:my='http://example.org/my/'>\n"
    "  <exif:Flash rdf:parseType='Resource'><exif:Fired>True</exif:Fired><exif:Mode>2</exif:Mode></exif:Flash>\n"
    "  <my:Lens><rdf:Description my:Make='Zeiss' my:Focal='50'/></my:Lens>\n"
    "  <my:Link rdf:resource='https://example.org/x'/>\n"
    " </rdf:Description>\n"
    "</rdf:RDF>\n</x:xmpmeta>\n<?xpacket end='w'?>";
static const char *val(xmp_doc *d, const char *path) { xmp_node *n = xmp_find(d, 0, path); return n && n->value ? n->value : "(nada)"; }
static int write_file(const char *p, const unsigned char *b, size_t n) { FILE *f = fopen(p, "wb"); if (!f) return 0; fwrite(b, 1, n, f); fclose(f); return 1; }
static unsigned char *read_file(const char *p, size_t *n) {
    FILE *f = fopen(p, "rb"); if (!f) return 0;
    fseek(f, 0, SEEK_END); *n = (size_t)ftell(f); fseek(f, 0, SEEK_SET);
    unsigned char *b = malloc(*n); if (fread(b, 1, *n, f) != *n) { fclose(f); free(b); return 0; }
    fclose(f); return b;
}
static void checks(xmp_doc *d, const char *tag) {
    char m[160];
    snprintf(m, sizeof m, "%s: atributo da Description (xmp:CreatorTool com entidade)", tag);
    CHECK(!strcmp(val(d, "xmp:CreatorTool"), "Câmera & Cia"), m);
    xmp_node *t = xmp_find(d, 0, "dc:title");
    snprintf(m, sizeof m, "%s: dc:title é texto alternativo com 2 idiomas", tag);
    CHECK(t && t->type == XMP_ALTERNATE_TEXT && t->nkids == 2, m);
    snprintf(m, sizeof m, "%s: dc:title[en] = Sunset, [x-default] com acento", tag);
    CHECK(!strcmp(val(d, "dc:title[en]"), "Sunset") && !strcmp(val(d, "dc:title[x-default]"), "Pôr do sol"), m);
    xmp_node *s = xmp_find(d, 0, "dc:subject");
    snprintf(m, sizeof m, "%s: dc:subject é Bag de 3 e o terceiro desescapado", tag);
    CHECK(s && s->type == XMP_ARRAY_UNORDERED && s->nkids == 3 && !strcmp(val(d, "dc:subject[2]"), "<tag>"), m);
    snprintf(m, sizeof m, "%s: dc:creator é Seq", tag);
    CHECK(xmp_find(d, 0, "dc:creator") && xmp_find(d, 0, "dc:creator")->type == XMP_ARRAY_ORDERED, m);
    snprintf(m, sizeof m, "%s: CDATA vira texto", tag);
    CHECK(!strcmp(val(d, "dc:rights"), "© 2026 & todos"), m);
    snprintf(m, sizeof m, "%s: estrutura parseType Resource (exif:Flash.Fired)", tag);
    CHECK(xmp_find(d, 0, "exif:Flash") && xmp_find(d, 0, "exif:Flash")->type == XMP_STRUCTURE && !strcmp(val(d, "exif:Flash.Fired"), "True"), m);
    snprintf(m, sizeof m, "%s: estrutura em Description com atributos e prefixo novo (my:Lens.Make)", tag);
    CHECK(!strcmp(val(d, "my:Lens.Make"), "Zeiss") && xmp_uri_for(d, "my") && !strcmp(xmp_uri_for(d, "my"), "http://example.org/my/"), m);
    snprintf(m, sizeof m, "%s: rdf:resource vira o valor", tag);
    CHECK(!strcmp(val(d, "my:Link"), "https://example.org/x"), m);
}
int main(int argc, char **argv) {
    xmp_doc d;
    CHECK(xmp_parse(SAMPLE, strlen(SAMPLE), &d) == 0 && d.n >= 9, "parse do pacote");
    checks(&d, "lido");
    size_t xn;
    char *x = xmp_serialize(&d, &xn);
    xmp_doc d2;
    CHECK(x && xmp_parse(x, xn, &d2) == 0, "gravado e relido");
    checks(&d2, "ida e volta");
    /* embutir nos arquivos que o .py criou e achar o pacote de volta */
    const char *names[4][2] = {{"a.jpg", "public.jpeg"}, {"a.png", "public.png"}, {"a.tif", "public.tiff"}, {"a.gif", "com.compuserve.gif"}};
    for (int i = 0; argc > 1 && i < 4; i++) {
        char p[512], q[512], m[128];
        snprintf(p, sizeof p, "%s/%s", argv[1], names[i][0]);
        snprintf(q, sizeof q, "%s/xmp-%s", argv[1], names[i][0]);
        size_t n, on, pn;
        unsigned char *in = read_file(p, &n), *out;
        int rc = in ? xmp_embed(names[i][1], in, n, (const unsigned char *)x, xn, &out, &on) : -1;
        const unsigned char *pk = rc == 0 ? xmp_find_packet(out, on, &pn) : 0;
        xmp_doc d3;
        int ok = pk && xmp_parse((const char *)pk, pn, &d3) == 0 && !strcmp(val(&d3, "dc:title[en]"), "Sunset");
        snprintf(m, sizeof m, "%s: embutido e achado de volta", names[i][1]);
        CHECK(ok, m);
        if (rc == 0) write_file(q, out, on);
        /* embutir de novo troca o pacote em vez de duplicar (menos no GIF, que só acrescenta) */
        if (rc == 0 && i < 3) {
            unsigned char *o2; size_t n2;
            xmp_embed(names[i][1], out, on, (const unsigned char *)x, xn, &o2, &n2);
            int count = 0;
            for (size_t k = 0; k + 10 < n2; k++) if (!memcmp(o2 + k, "<x:xmpmeta", 10)) count++;
            snprintf(m, sizeof m, "%s: regravar não duplica o XMP", names[i][1]);
            CHECK(count == 1, m);
        }
    }
    printf("%d falha(s)\n", fails);
    return fails ? 1 : 0;
}
