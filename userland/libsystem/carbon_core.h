/* mOS libSystem — núcleo sem ObjC do Carbon: parâmetros de EventRef, casamento de tipos dos tratadores e regiões do
 * HIShape (lista de retângulos disjuntos). Usado por carbon_events.c e carbon_shape.c; teste em
 * userland/tests-appkit/carbon_host.c. Sem libc (malloc/free vêm de quem chama). */
#ifndef CARBON_CORE_H
#define CARBON_CORE_H
typedef unsigned int cc_u32;
typedef unsigned long cc_size;
void *cc_alloc(cc_size n);           /* fornecidos por quem usa (libSystem ou teste) */
void cc_free(void *p);

/* ---- EventRef */
typedef struct cc_param { cc_u32 name, type; cc_size size; struct cc_param *next; unsigned char data[]; } cc_param;
typedef struct { int refs; cc_u32 cls, kind; double time; unsigned attrs; cc_param *params; } cc_event;
cc_event *cc_event_new(cc_u32 cls, cc_u32 kind, double time, unsigned attrs);
cc_event *cc_event_copy(const cc_event *e);
void cc_event_release(cc_event *e);            /* libera quando refs chega a 0 */
int cc_event_set(cc_event *e, cc_u32 name, cc_u32 type, cc_size size, const void *data);   /* 0 ou -1 (memória) */
/* devolve 0, ou -9870 (eventParameterNotFoundErr), -9878 (eventParameterTypeMismatch? usamos errAECoercionFail -1700) */
int cc_event_get(const cc_event *e, cc_u32 name, cc_u32 want, cc_u32 *out_type, cc_size cap, cc_size *out_size, void *out);

/* ---- tipos de evento de um tratador */
typedef struct { cc_u32 cls, kind; } cc_spec;
typedef struct { cc_spec *v; int n, cap; } cc_specs;
int cc_specs_add(cc_specs *s, const cc_spec *add, int n);
int cc_specs_remove(cc_specs *s, const cc_spec *rm, int n);   /* -9876 (eventNotHandled?) se algum não estava: usamos paramErr -50 */
int cc_specs_match(const cc_specs *s, cc_u32 cls, cc_u32 kind);
void cc_specs_free(cc_specs *s);

/* ---- HIShape: retângulos disjuntos */
typedef struct { double x, y, w, h; } cc_rect;
typedef struct { cc_rect *r; int n, cap; } cc_shape;
void cc_shape_clear(cc_shape *s);
int cc_shape_set_rect(cc_shape *s, cc_rect r);
int cc_shape_copy(cc_shape *dst, const cc_shape *src);
int cc_shape_union(cc_shape *out, const cc_shape *a, const cc_shape *b);
int cc_shape_intersect(cc_shape *out, const cc_shape *a, const cc_shape *b);
int cc_shape_diff(cc_shape *out, const cc_shape *a, const cc_shape *b);
int cc_shape_xor(cc_shape *out, const cc_shape *a, const cc_shape *b);
int cc_shape_empty(const cc_shape *s);
cc_rect cc_shape_bounds(const cc_shape *s);
int cc_shape_rectangular(const cc_shape *s);
int cc_shape_contains(const cc_shape *s, double x, double y);
int cc_shape_intersects_rect(const cc_shape *s, cc_rect r);
void cc_shape_offset(cc_shape *s, double dx, double dy);
int cc_shape_inset(cc_shape *s, double dx, double dy);
double cc_shape_area(const cc_shape *s);

/* ---- trilhas do HITheme (barra de rolagem, slider, progresso): posição e tamanho do polegar ao longo do comprimento */
typedef struct { double pos, len; } cc_thumb;
/* kind: 0 barra de rolagem (viewsize define o tamanho, mínimo 16), 1 slider (polegar fixo de 15), 2 progresso (preenchido) */
cc_thumb cc_track_thumb(int kind, double length, double minv, double maxv, double value, double viewsize);
/* parte sob a coordenada ao longo da trilha: 129 polegar, 22 página acima/à esquerda, 23 página abaixo/à direita, 0 fora */
int cc_track_hit(int kind, double length, double minv, double maxv, double value, double viewsize, double at);

/* ---- famílias de ícones 'icns' (IconFamilyHandle): cabeçalho 'icns' + tamanho total, depois elementos tipo + tamanho
 * (com os 8 bytes do cabeçalho do elemento), tudo big-endian. */
/* acha o elemento: devolve 1 com o deslocamento e o tamanho dos dados (sem o cabeçalho), ou 0 */
int cc_icns_find(const unsigned char *fam, long n, unsigned type, long *off, long *len);
/* troca (ou acrescenta) o elemento type em out, que precisa de n + dn + 8 bytes; devolve o novo tamanho ou -1 */
long cc_icns_set(const unsigned char *fam, long n, unsigned type, const unsigned char *data, long dn, unsigned char *out);
#endif
