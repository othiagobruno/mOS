/*
 * +[NSException raise:format:] sem abortar.
 * foundation.c só abre o va_list e chama daqui. O motivo é formatado e o objeto
 * sai por objc_exception_throw, o mesmo caminho de -[NSException raise]: com a
 * libc++abi carregada, o @catch recebe a exceção; sem ela, o throw ainda encerra.
 */
#include <stdarg.h>

typedef struct cx_class cx_class_t;
typedef struct objc_object { cx_class_t *isa; } *id;

id cx_str_vfmt(id fmt, va_list ap);
id cx_exception_with(id cls, id name, id reason, id info);
void objc_exception_throw(id e);

void cx_nsexception_raise_format(id cls, id name, id fmt, va_list ap) {
    va_list aq;
    va_copy(aq, ap);
    id reason = cx_str_vfmt(fmt, aq);
    va_end(aq);
    objc_exception_throw(cx_exception_with(cls, name, reason, 0));
}

void cx_nsexception_raise(id cls, id name, id fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    cx_nsexception_raise_format(cls, name, fmt, ap);
    va_end(ap);
}
