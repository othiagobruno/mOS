/*
 * mOS libSystem — classes ObjC que o Swift com interop ObjC (binários Swift da Apple, docs/swift-objc-runtime.md)
 * referencia pelo nome: as classes OS_OBJECT do libdispatch e do os_log (OS_dispatch_queue, OS_os_log...).
 *
 * No SDK da Apple elas são "objc_runtime_visible": o código Swift só usa o objeto-classe (_OBJC_CLASS_$_OS_...,
 * objc_lookUpClass) para metadata, casts e inits de conveniência das extensões do overlay Dispatch. Sem elas o
 * carregador poria NSObject no lugar. Hierarquia como no macOS: NSObject > OS_object > OS_dispatch_object > ...
 * Os objetos do dispatch do mOS (dispatch.c) ainda não têm estas classes no isa.
 */
#include "foundation.h"

#define OSCLASS(N) cx_class_t N##_c __asm__("OBJC_CLASS_$_" #N); cx_class_t N##_m __asm__("OBJC_METACLASS_$_" #N);
OSCLASS(OS_object)
OSCLASS(OS_dispatch_object)
OSCLASS(OS_dispatch_queue)
OSCLASS(OS_dispatch_queue_serial)
OSCLASS(OS_dispatch_queue_concurrent)
OSCLASS(OS_dispatch_queue_main)
OSCLASS(OS_dispatch_queue_global)
OSCLASS(OS_dispatch_queue_attr)
OSCLASS(OS_dispatch_group)
OSCLASS(OS_dispatch_semaphore)
OSCLASS(OS_dispatch_source)
OSCLASS(OS_dispatch_data)
OSCLASS(OS_dispatch_io)
OSCLASS(OS_dispatch_workloop)
OSCLASS(OS_os_log)

extern cx_class_t NSObject_c __asm__("OBJC_CLASS_$_NSObject");

/* ---- SPI do libdispatch da Apple que o _Concurrency (DispatchGlobalExecutor.cpp, ExecutorBridge.cpp) chama */
void dispatch_async_f(void *q, void *ctx, void (*fn)(void *));
int cx_dispatch_drain_main(void);
extern volatile int cx_main_queue_live;
int usleep(unsigned us);

/* um job Swift é um objeto cuja metadata é uma DispatchClassMetadata (swift/ABI/Metadata.h): kind, opaque,
 * opaqueObjC[3], vtableType, vtableInvoke -> o invoke fica 48 bytes depois do ponto de endereço */
typedef void (*cx_swift_invoke_t)(void *job, void *ctx, u32 flags);
static void run_swift_job(void *job) {
    u8 *md = *(u8 **)job;
    ((cx_swift_invoke_t)*(void **)(md + 48))(job, 0, 0);
}
extern char _dispatch_main_q[];
extern volatile int cx_dispatch_main_used;
void dispatch_async_swift_job(void *q, void *job, u32 qos) {
    (void)qos;
    /* o executor principal do Swift usa a fila principal: ela passa a segurar o run loop principal (dispatch.c) */
    if (q == (void *)_dispatch_main_q) cx_dispatch_main_used = 1;
    dispatch_async_f(q, job, run_swift_job);
}

/* dispatch_main: a thread principal vira o laço da fila principal (o `async main` do Swift termina com exit()) */
__attribute__((noreturn)) void dispatch_main(void) {
    cx_main_queue_live = 1;
    for (;;) if (!cx_dispatch_drain_main()) usleep(1000);
}

/* asserções de fila (dispatchPrecondition / MainActor.assertIsolated): o dispatch do mOS não rastreia a fila atual */
void dispatch_assert_queue(void *q) { (void)q; }
void dispatch_assert_queue_V2(void *q) __asm__("dispatch_assert_queue$V2");
void dispatch_assert_queue_V2(void *q) { (void)q; }
void dispatch_assert_queue_not(void *q) { (void)q; }
void dispatch_assert_queue_not_V2(void *q) __asm__("dispatch_assert_queue_not$V2");
void dispatch_assert_queue_not_V2(void *q) { (void)q; }

/* vouchers (contexto de QoS/importância que viaja com o trabalho): o mOS não tem; nenhum voucher adotado */
void *voucher_adopt(void *v) { (void)v; return 0; }
void *voucher_copy(void) { return 0; }
void *voucher_copy_without_importance(void) { return 0; }
void voucher_release(void *v) { (void)v; }
void *voucher_retain(void *v) { return v; }

/* chamado por cx_objc_boot (objc.c) depois do Foundation */
void cx_swiftobjc_init(void) {
#define DEF(N, S) cx_define_class(&N##_c, &N##_m, #N, &S##_c, 0)
    DEF(OS_object, NSObject);
    DEF(OS_dispatch_object, OS_object);
    DEF(OS_dispatch_queue, OS_dispatch_object);
    DEF(OS_dispatch_queue_serial, OS_dispatch_queue);
    DEF(OS_dispatch_queue_concurrent, OS_dispatch_queue);
    DEF(OS_dispatch_queue_main, OS_dispatch_queue_serial);
    DEF(OS_dispatch_queue_global, OS_dispatch_queue);
    DEF(OS_dispatch_queue_attr, OS_dispatch_object);
    DEF(OS_dispatch_group, OS_dispatch_object);
    DEF(OS_dispatch_semaphore, OS_dispatch_object);
    DEF(OS_dispatch_source, OS_dispatch_object);
    DEF(OS_dispatch_data, OS_dispatch_object);
    DEF(OS_dispatch_io, OS_dispatch_object);
    DEF(OS_dispatch_workloop, OS_dispatch_queue);
    DEF(OS_os_log, OS_object);
#undef DEF
}
