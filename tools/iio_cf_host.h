/* Tipos do runtime ObjC do mOS que imageio_cf.inc usa, para o teste de host (tools/imageio_cf_host.c). */
#include <stdbool.h>
#include <stdint.h>
typedef struct cx_class { const char *name; void *dealloc; void *tid; } cx_class_t;
typedef struct objc_object { cx_class_t *isa; } *id;
typedef const char *SEL;
typedef signed char BOOL;
cx_class_t *cx_class_named(const char *name);
cx_class_t *cx_define_class(cx_class_t *cls, cx_class_t *meta, const char *name, cx_class_t *super, uint32_t extra);
void cx_add_method(cx_class_t *cls, const char *sel, void *imp);
id cx_alloc_instance(cx_class_t *cls);
