/* mOS ImageIO — XMP sem CoreFoundation (iio_xmp.c). */
#ifndef MOS_IIO_XMP_H
#define MOS_IIO_XMP_H
#include <stddef.h>
#ifdef XMP_HOST
typedef unsigned char u8;
typedef unsigned int u32;
#endif
/* CGImageMetadataType do macOS */
enum { XMP_INVALID = -1, XMP_DEFAULT = 0, XMP_STRING = 1, XMP_ARRAY_UNORDERED = 2, XMP_ARRAY_ORDERED = 3, XMP_ARRAY_ALTERNATE = 4,
       XMP_ALTERNATE_TEXT = 5, XMP_STRUCTURE = 6 };
typedef struct xmp_node {
    char *ns, *prefix, *name;
    int type;
    char *value;                 /* simples */
    char *lang;                  /* qualificador xml:lang */
    struct xmp_node **kids;      /* arrays (itens) e estruturas (campos) */
    int nkids;
} xmp_node;
typedef struct {
    xmp_node **props; int n;
    struct { char *prefix, *uri; } *ns; int nns;
} xmp_doc;
xmp_node *xmp_node_new(const char *ns, const char *prefix, const char *name, int type, const char *value);
void xmp_node_add(xmp_node *parent, xmp_node *kid);
void xmp_node_free(xmp_node *n);
xmp_node *xmp_node_copy(const xmp_node *n);
void xmp_doc_add(xmp_doc *d, xmp_node *n);
void xmp_doc_free(xmp_doc *d);
const char *xmp_uri_for(const xmp_doc *d, const char *prefix);
const char *xmp_prefix_for(const xmp_doc *d, const char *uri);
int xmp_register(xmp_doc *d, const char *uri, const char *prefix);
int xmp_parse(const char *xml, size_t n, xmp_doc *out);
char *xmp_serialize(const xmp_doc *d, size_t *len);
xmp_node *xmp_find(const xmp_doc *d, xmp_node *base, const char *path);
const u8 *xmp_find_packet(const u8 *file, size_t n, size_t *len);
int xmp_embed(const char *uti, const u8 *in, size_t n, const u8 *xmp, size_t xn, u8 **out, size_t *on);
#endif
