/*
 * libswiftDispatch e libswift_Concurrency que o OpenBot importa.
 * O que tem ABI inteiro claro chama o dispatch que já existe (fila, grupo, semáforo, timer).
 * Metadados Swift (Ma/Mn/Mc) devolvem nil: não há runtime de metadados aqui.
 * Convenção assumida e não conferida com um compilador Swift: método com self em r13;
 * DispatchTimeInterval como (payload em um registrador, tag no seguinte), tags na ordem
 * do enum (seconds 0, milliseconds 1, microseconds 2, nanoseconds 3, never 4).
 * Tarefa e continuation não escalonam: alloc/dealloc são malloc/free, await não suspende.
 */
#include "foundation.h"

void *malloc(size_t n);
void *calloc(size_t n, size_t s);
void free(void *p);
void *dispatch_queue_create(const char *label, void *attr);
void dispatch_async(void *q, void *block);
long dispatch_group_wait(void *g, unsigned long long timeout);
long dispatch_semaphore_signal(void *s);
long dispatch_semaphore_wait(void *s, unsigned long long timeout);
void *dispatch_source_create(const void *type, unsigned long handle, unsigned long mask, void *queue);
void dispatch_source_set_event_handler(void *s, void *block);
void dispatch_source_set_timer(void *s, unsigned long long start, unsigned long long interval, unsigned long long leeway);
void dispatch_source_cancel(void *s);
void dispatch_resume(void *o);
unsigned long long mach_absolute_time(void);
extern char _dispatch_source_type_timer[];

long swift_qos_background(void) __asm__("$s8Dispatch0A3QoSV10backgroundACvgZ");
long swift_qos_unspecified(void) __asm__("$s8Dispatch0A3QoSV11unspecifiedACvgZ");
long swift_qos_user_init(void) __asm__("$s8Dispatch0A3QoSV13userInitiatedACvgZ");
long swift_qos_user_inter(void) __asm__("$s8Dispatch0A3QoSV15userInteractiveACvgZ");
long swift_qos_default(void) __asm__("$s8Dispatch0A3QoSV7defaultACvgZ");
long swift_qos_utility(void) __asm__("$s8Dispatch0A3QoSV7utilityACvgZ");
unsigned long long swift_time_future(void) __asm__("$s8Dispatch0A4TimeV13distantFutureACvgZ");
unsigned long long swift_time_now(void) __asm__("$s8Dispatch0A4TimeV3nowACyFZ");
int swift_timeout_eq(long a, long b) __asm__("$s8Dispatch0A13TimeoutResultO2eeoiySbAC_ACtFZ");
void *swift_queue_make(void) __asm__("$sSo17OS_dispatch_queueC8DispatchE5label3qos10attributes20autoreleaseFrequency6targetABSS_AC0D3QoSVAbCE10AttributesVAbCE011AutoreleaseI0OABSgtcfC");
void *swift_make_timer(long flags, void *q) __asm__("$sSo18OS_dispatch_sourceC8DispatchE15makeTimerSource5flags5queueSo0a1_b1_C6_timer_pAbCE0F5FlagsV_So0a1_b1_I0CSgtFZ");
long swift_inherit(void) __asm__("$sSo17OS_dispatch_queueC8DispatchE20AutoreleaseFrequencyO7inherityA2EmFWC");

long swift_qos_background(void) { return 0x09; }
long swift_qos_unspecified(void) { return 0; }
long swift_qos_user_init(void) { return 0x19; }
long swift_qos_user_inter(void) { return 0x21; }
long swift_qos_default(void) { return 0x15; }
long swift_qos_utility(void) { return 0x11; }
unsigned long long swift_time_future(void) { return ~0ull; }
unsigned long long swift_time_now(void) { return mach_absolute_time(); }
int swift_timeout_eq(long a, long b) { return a == b; }
void *swift_queue_make(void) { return dispatch_queue_create("com.mos.dispatch", 0); }
void *swift_make_timer(long flags, void *q) {
    (void)flags;
    return dispatch_source_create(_dispatch_source_type_timer, 0, 0, q);
}
long swift_inherit(void) { return 0; }

void cx_swift_dq_async(void *group, long qos, long flags, void *block, void *q) {
    (void)group; (void)qos; (void)flags;
    if (q && block) dispatch_async(q, block);
}
unsigned long long cx_swift_time_add_iv(unsigned long long t, long long payload, long long tag) {
    if (t == ~0ull || tag == 4) return t;
    long long mul = tag == 0 ? 1000000000ll : tag == 1 ? 1000000ll : tag == 2 ? 1000ll : tag == 3 ? 1ll : 0;
    if (!mul) return t;
    return t + (unsigned long long)(payload * mul);
}
void cx_swift_timer_sched(void *src, unsigned long long deadline, long long payload, long long tag, unsigned long long leeway) {
    long long mul = tag == 0 ? 1000000000ll : tag == 1 ? 1000000ll : tag == 2 ? 1000ll : tag == 3 ? 1ll : 0;
    unsigned long long iv = mul ? (unsigned long long)(payload * mul) : 0;
    dispatch_source_set_timer(src, deadline, iv, leeway);
}

#if defined(__x86_64__)
__asm__(
".globl \"$s8Dispatch0A12TimeIntervalO12millisecondsyACSicACmFWC\"\n"
"\"$s8Dispatch0A12TimeIntervalO12millisecondsyACSicACmFWC\":\n"
"  movq %rdi, %rax\n  movq $1, %rdx\n  ret\n"
".globl \"$s8Dispatch1poiyAA0A4TimeVAD_AA0aB8IntervalOtF\"\n"
"\"$s8Dispatch1poiyAA0A4TimeVAD_AA0aB8IntervalOtF\":\n"
"  jmp cx_swift_time_add_iv\n"
".globl \"$s8Dispatch1poiyAA0A4TimeVAD_SdtF\"\n"
"\"$s8Dispatch1poiyAA0A4TimeVAD_SdtF\":\n"
"  cmpq $-1, %rdi\n  je 1f\n"
"  movsd %xmm0, %xmm1\n"
"  movabs $0x41cdcd6500000000, %rax\n" /* 1e9 */
"  movq %rax, %xmm0\n  mulsd %xmm1, %xmm0\n  cvttsd2si %xmm0, %rax\n  addq %rdi, %rax\n  ret\n"
"1: movq %rdi, %rax\n  ret\n"
".globl \"$sSo17OS_dispatch_groupC8DispatchE4waityyF\"\n"
"\"$sSo17OS_dispatch_groupC8DispatchE4waityyF\":\n"
"  movq %r13, %rdi\n  movq $-1, %rsi\n  jmp dispatch_group_wait\n"
".globl \"$sSo17OS_dispatch_groupC8DispatchE4wait7timeoutAC0D13TimeoutResultOAC0D4TimeV_tF\"\n"
"\"$sSo17OS_dispatch_groupC8DispatchE4wait7timeoutAC0D13TimeoutResultOAC0D4TimeV_tF\":\n"
"  movq %rdi, %rsi\n  movq %r13, %rdi\n  jmp dispatch_group_wait\n"
".globl \"$sSo17OS_dispatch_queueC8DispatchE4mainABvgZ\"\n"
"\"$sSo17OS_dispatch_queueC8DispatchE4mainABvgZ\":\n"
"  leaq _dispatch_main_q(%rip), %rax\n  ret\n"
".globl \"$sSo17OS_dispatch_queueC8DispatchE5async5group3qos5flags7executeySo0a1_b1_F0CSg_AC0D3QoSVAC0D13WorkItemFlagsVyyXBtF\"\n"
"\"$sSo17OS_dispatch_queueC8DispatchE5async5group3qos5flags7executeySo0a1_b1_F0CSg_AC0D3QoSVAC0D13WorkItemFlagsVyyXBtF\":\n"
"  movq %r13, %r8\n  jmp cx_swift_dq_async\n"
".globl \"$sSo18OS_dispatch_sourceP8DispatchE15setEventHandler3qos5flags7handleryAC0D3QoSV_AC0D13WorkItemFlagsVyyXBSgtF\"\n"
"\"$sSo18OS_dispatch_sourceP8DispatchE15setEventHandler3qos5flags7handleryAC0D3QoSV_AC0D13WorkItemFlagsVyyXBSgtF\":\n"
"  movq %rdx, %rsi\n  movq %r13, %rdi\n  jmp dispatch_source_set_event_handler\n"
".globl \"$sSo18OS_dispatch_sourceP8DispatchE6cancelyyF\"\n"
"\"$sSo18OS_dispatch_sourceP8DispatchE6cancelyyF\":\n"
"  movq %r13, %rdi\n  jmp dispatch_source_cancel\n"
".globl \"$sSo18OS_dispatch_sourceP8DispatchE6resumeyyF\"\n"
"\"$sSo18OS_dispatch_sourceP8DispatchE6resumeyyF\":\n"
"  movq %r13, %rdi\n  jmp dispatch_resume\n"
".globl \"$sSo21OS_dispatch_semaphoreC8DispatchE6signalSiyF\"\n"
"\"$sSo21OS_dispatch_semaphoreC8DispatchE6signalSiyF\":\n"
"  movq %r13, %rdi\n  jmp dispatch_semaphore_signal\n"
".globl \"$sSo21OS_dispatch_semaphoreC8DispatchE4wait7timeoutAC0D13TimeoutResultOAC0D4TimeV_tF\"\n"
"\"$sSo21OS_dispatch_semaphoreC8DispatchE4wait7timeoutAC0D13TimeoutResultOAC0D4TimeV_tF\":\n"
"  movq %rdi, %rsi\n  movq %r13, %rdi\n  jmp dispatch_semaphore_wait\n"
".globl \"$sSo24OS_dispatch_source_timerP8DispatchE8schedule8deadline9repeating6leewayyAC0E4TimeV_AC0eJ8IntervalOAKtF\"\n"
"\"$sSo24OS_dispatch_source_timerP8DispatchE8schedule8deadline9repeating6leewayyAC0E4TimeV_AC0eJ8IntervalOAKtF\":\n"
"  movq %rcx, %r8\n  movq %rdx, %rcx\n  movq %rsi, %rdx\n  movq %rdi, %rsi\n  movq %r13, %rdi\n  jmp cx_swift_timer_sched\n"
".globl \"$s8Dispatch0A12TimeIntervalOMa\"\n\"$s8Dispatch0A12TimeIntervalOMa\":\n  xor %eax, %eax\n  ret\n"
".globl \"$s8Dispatch0A13WorkItemFlagsVMa\"\n\"$s8Dispatch0A13WorkItemFlagsVMa\":\n  xor %eax, %eax\n  ret\n"
".globl \"$s8Dispatch0A13WorkItemFlagsVMn\"\n\"$s8Dispatch0A13WorkItemFlagsVMn\":\n  xor %eax, %eax\n  ret\n"
".globl \"$s8Dispatch0A13WorkItemFlagsVs10SetAlgebraAAMc\"\n\"$s8Dispatch0A13WorkItemFlagsVs10SetAlgebraAAMc\":\n  xor %eax, %eax\n  ret\n"
".globl \"$s8Dispatch0A3QoSVMa\"\n\"$s8Dispatch0A3QoSVMa\":\n  xor %eax, %eax\n  ret\n"
".globl \"$s8Dispatch0A4TimeVMa\"\n\"$s8Dispatch0A4TimeVMa\":\n  xor %eax, %eax\n  ret\n"
".globl \"$sSo17OS_dispatch_queueC8DispatchE10AttributesVMa\"\n\"$sSo17OS_dispatch_queueC8DispatchE10AttributesVMa\":\n  xor %eax, %eax\n  ret\n"
".globl \"$sSo17OS_dispatch_queueC8DispatchE10AttributesVMn\"\n\"$sSo17OS_dispatch_queueC8DispatchE10AttributesVMn\":\n  xor %eax, %eax\n  ret\n"
".globl \"$sSo17OS_dispatch_queueC8DispatchE10AttributesVs10SetAlgebraACMc\"\n\"$sSo17OS_dispatch_queueC8DispatchE10AttributesVs10SetAlgebraACMc\":\n  xor %eax, %eax\n  ret\n"
".globl \"$sSo17OS_dispatch_queueC8DispatchE20AutoreleaseFrequencyOMa\"\n\"$sSo17OS_dispatch_queueC8DispatchE20AutoreleaseFrequencyOMa\":\n  xor %eax, %eax\n  ret\n"
".globl \"$sSo18OS_dispatch_sourceC8DispatchE10TimerFlagsVMa\"\n\"$sSo18OS_dispatch_sourceC8DispatchE10TimerFlagsVMa\":\n  xor %eax, %eax\n  ret\n"
".globl \"$sSo18OS_dispatch_sourceC8DispatchE10TimerFlagsVMn\"\n\"$sSo18OS_dispatch_sourceC8DispatchE10TimerFlagsVMn\":\n  xor %eax, %eax\n  ret\n"
".globl \"$sSo18OS_dispatch_sourceC8DispatchE10TimerFlagsVs10SetAlgebraACMc\"\n\"$sSo18OS_dispatch_sourceC8DispatchE10TimerFlagsVs10SetAlgebraACMc\":\n  xor %eax, %eax\n  ret\n"
".globl \"$sScA15unownedExecutorScevgTj\"\n\"$sScA15unownedExecutorScevgTj\":\n  xor %eax, %eax\n  ret\n"
".globl \"$sScP8rawValues5UInt8Vvg\"\n\"$sScP8rawValues5UInt8Vvg\":\n  xor %eax, %eax\n  ret\n"
".globl \"$sScPMa\"\n\"$sScPMa\":\n  xor %eax, %eax\n  ret\n"
);
#endif

void *swift_task_alloc(size_t n) { return malloc(n ? n : 1); }
void swift_task_dealloc(void *p) { free(p); }
void *swift_task_create(size_t flags, void *group, void *parent, void *executor, size_t stack, void *fn, void *ctx) {
    (void)flags; (void)group; (void)parent; (void)executor; (void)stack; (void)fn; (void)ctx;
    return calloc(1, 64);
}
void swift_task_switch(void *executor, void *ctx) { (void)executor; (void)ctx; }

typedef struct { void *a, *b; int resumed; void *error; } cx_cont_t;
void *swift_continuation_init(void *a, void *b) {
    cx_cont_t *c = calloc(1, sizeof *c);
    if (c) { c->a = a; c->b = b; }
    return c;
}
void swift_continuation_await(void *c) { (void)c; }
void swift_continuation_throwingResume(void *c) { if (c) ((cx_cont_t *)c)->resumed = 1; }
void swift_continuation_throwingResumeWithError(void *c, void *err) {
    if (!c) return;
    ((cx_cont_t *)c)->resumed = 1;
    ((cx_cont_t *)c)->error = err;
}
