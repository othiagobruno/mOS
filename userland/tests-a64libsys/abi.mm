// mOS — teste da fronteira app ↔ libSystem (frente LIBSYS do tradutor arm64, docs/tradutor-arm64.md).
// O mesmo fonte compilado com zig c++ -target aarch64-macos (o app arm64 de verdade, ABI da Apple) e -target x86_64-macos
// (tools/build-libsys-arm64-test.ps1): variádicos com double/long double e argumentos na pilha, va_list repassado,
// structs HFA e retorno indireto (x8), objc_msgSend com struct e 16 argumentos, NSInvocation, blocks e trampolins de
// imp_implementationWithBlock, dispatch, exceção C++ atravessando quadros do libSystem (qsort), setjmp/longjmp, JIT com
// MAP_JIT + sys_icache_invalidate, páginas de 16 KiB, libm e sysctl. Imprime "ok"/"FALHA" por item; saída = falhas.
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <setjmp.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/mman.h>
#include <sys/sysctl.h>
#include <objc/runtime.h>
#include <objc/message.h>
#include <dispatch/dispatch.h>
#include <libkern/OSCacheControl.h>
#include <stdexcept>
#include <objc/NSObject.h>

// uma classe ObjC no binário: com seções __objc_* o carregador inicia o runtime e o Foundation do mOS (cx_objc_boot)
@interface LibsysAbiSonda : NSObject
@end
@implementation LibsysAbiSonda
@end
@interface LibsysAbiErro : LibsysAbiSonda   // exceção ObjC do próprio app (OBJC_EHTYPE_$_LibsysAbiErro no binário)
@end
@implementation LibsysAbiErro
@end
@interface NSException : NSObject
+ (id)exceptionWithName:(id)name reason:(id)reason userInfo:(id)info;
- (void)raise;
- (id)name;
- (id)reason;
@end
@interface NSArray : NSObject
+ (id)arrayWithObjects:(const id *)objs count:(unsigned long)n;
- (void)enumerateObjectsUsingBlock:(void (^)(id obj, unsigned long idx, signed char *stop))b;
@end

static int falhas;
// structs de mais de 16 bytes: no x86_64 da Apple vêm pelo objc_msgSend_stret; no arm64 o objc_msgSend comum (x8 ou d0..d3)
#if defined(__aarch64__)
#define MSG_STRET objc_msgSend
#else
#define MSG_STRET objc_msgSend_stret
#endif
#if defined(__aarch64__)
static const char *arch = "arm64";
#else
static const char *arch = "x86_64";
#endif
#define CHECK(nome, cond) do { if (cond) printf("[%s] ok    %s\n", arch, nome); else { printf("[%s] FALHA %s (linha %d)\n", arch, nome, __LINE__); falhas++; } fflush(stdout); } while (0)

typedef struct { double x, y, w, h; } Rect;            // HFA de 4 double: d0..d3 no arm64, sret no x86_64
typedef struct { long a, b, c, d, e; } Big;             // 40 bytes: x8 no arm64, sret no x86_64
typedef struct { float a, b; } F2;                      // HFA de 2 float
extern "C" id NSStringFromRect(Rect r);

static id S(const char *s) { return ((id (*)(Class, SEL, const char *))objc_msgSend)(objc_getClass("NSString"), sel_registerName("stringWithUTF8String:"), s); }
static const char *U(id o) { return o ? ((const char *(*)(id, SEL))objc_msgSend)(o, sel_registerName("UTF8String")) : "(nil)"; }

// ---- métodos da classe de teste (IMPs do app chamados pelo objc_msgSend e pelo NSInvocation do libSystem)
static Rect m_rect(id, SEL, double k) { Rect r = {1 * k, 2 * k, 3 * k, 4 * k}; return r; }
static Big m_big(id, SEL, long k) { Big b = {k, k + 1, k + 2, k + 3, k + 4}; return b; }
static F2 m_f2(id, SEL, float a) { F2 f = {a, a * 2}; return f; }
// 16 argumentos (o máximo do NSMethodSignature do mOS): 8 inteiros em x0..x7, c2/i2 na pilha (empacotados no arm64),
// 6 FP em v0..v5, short/char estendidos pelo chamador
static double m_mix(id self, SEL, char c, short s, int i, long l, long l2, short s2, char c2, int i2,
                    float f, double d1, double d2, double d3, double d4, double d5) {
    (void)self;
    return c + s * 10.0 + i * 100.0 + l * 1000.0 + l2 * 1e4 + s2 * 1e5 + c2 * 1e6 + i2 * 1e7 + f + d1 / 2 + d2 / 4 + d3 / 8 + d4 / 16 + d5 / 32;
}
static const double MIX = -3 + 4 * 10.0 + -5 * 100.0 + 6 * 1000.0 + 7 * 1e4 + -8 * 1e5 + 9 * 1e6 + 1 * 1e7 + 0.5 + 1.0 / 2 + 2.0 / 4 + 3.0 / 8 + 4.0 / 16 + 5.0 / 32;
// 9 FP + 10 inteiros: o 9º double e os inteiros 9 e 10 vão para a pilha (só chamada direta: o NSMethodSignature vai até 16)
static double m_many(id, SEL, long a, long b, long c, long d, long e, long f, long g, long h, double d1, double d2, double d3,
                     double d4, double d5, double d6, double d7, double d8, double d9) {
    return a + b + c + d + e + f + g + h + d1 + d2 + d3 + d4 + d5 + d6 + d7 + d8 + d9 * 1000;
}

// ---- variádico do app que repassa o va_list (char* no arm64 da Apple) ao libSystem
static int fmt(char *b, size_t n, const char *f, ...) { va_list ap; va_start(ap, f); int r = vsnprintf(b, n, f, ap); va_end(ap); return r; }

static int cmp_lanca(const void *a, const void *b) {
    if (*(const int *)a == 13 || *(const int *)b == 13) throw std::runtime_error("13 no qsort");
    return *(const int *)a - *(const int *)b;
}

int main() {
    printf("[%s] libsys-abi: pid %d\n", arch, getpid());
    char b[512];

    // 1. variádicos: 13 argumentos depois do formato (os do arm64 vão todos para a pilha), double, long double, char, short
#if defined(__aarch64__)
    snprintf(b, sizeof b, "%d|%s|%.3f|%c|%hd|%lld|%.2Lf|%e|%x|%u|%5.1f|%ld|%s", -42, "abc", 3.14159, 'Z', (short)-7, 1234567890123LL,
             (long double)2.5, 1e-3, 0xbeef, 4000000000u, 9.26, -9L, "fim");
#else
    // x86_64: o printf do libSystem x86 lê o long double de 80 bits como 8 bytes e desalinha os argumentos seguintes
    // (pendência da frente x86, não do tradutor); aqui o mesmo formato com double no lugar
    snprintf(b, sizeof b, "%d|%s|%.3f|%c|%hd|%lld|%.2f|%e|%x|%u|%5.1f|%ld|%s", -42, "abc", 3.14159, 'Z', (short)-7, 1234567890123LL,
             2.5, 1e-3, 0xbeef, 4000000000u, 9.26, -9L, "fim");
#endif
    printf("  snprintf: %s\n", b);
    CHECK("snprintf com 13 variádicos (double, long double, short, char)", !strcmp(b, "-42|abc|3.142|Z|-7|1234567890123|2.50|1.000000e-03|beef|4000000000|  9.3|-9|fim"));
    fmt(b, sizeof b, "%s=%g/%d/%.1f", "va", 0.5, 77, -1.26);
    printf("  vsnprintf: %s\n", b);
    CHECK("va_list do app repassado a vsnprintf", !strcmp(b, "va=0.5/77/-1.3"));
    int si = 0; double sd = 0; char ss[16] = "";
    CHECK("sscanf com ponteiros variádicos", sscanf("12 4.5 xyz", "%d %lf %15s", &si, &sd, ss) == 3 && si == 12 && sd == 4.5 && !strcmp(ss, "xyz"));
    long double ld = strtold("3.25", 0);
    CHECK("long double = double (strtold)", sizeof(long double) == (strcmp(arch, "arm64") ? 16 : 8) && ld == 3.25L);

    // 2. HFA por valor para o libSystem e NSString de volta
    Rect r0 = {1, 2, 3, 4};
    const char *rs = U(NSStringFromRect(r0));
    printf("  NSStringFromRect: %s\n", rs ? rs : "(null)");
    CHECK("NSStringFromRect(HFA de 4 double)", !strcmp(rs, "{{1, 2}, {3, 4}}"));

    // 3. classe criada em tempo de execução, chamada pelo objc_msgSend do libSystem
    Class K = objc_allocateClassPair(objc_getClass("NSObject"), "LibsysAbiTeste", 0);
    class_addMethod(K, sel_registerName("rect:"), (IMP)m_rect, "{Rect=dddd}@:d");
    class_addMethod(K, sel_registerName("big:"), (IMP)m_big, "{Big=qqqqq}@:q");
    class_addMethod(K, sel_registerName("f2:"), (IMP)m_f2, "{F2=ff}@:f");
    class_addMethod(K, sel_registerName("mix:s:i:l:l2:s2:c2:i2:f:d1:d2:d3:d4:d5:"), (IMP)m_mix, "d@:csiqqscifddddd");
    class_addMethod(K, sel_registerName("many"), (IMP)m_many, "d@:qqqqqqqqddddddddd");
    objc_registerClassPair(K);
    id o = ((id (*)(id, SEL))objc_msgSend)(((id (*)(Class, SEL))objc_msgSend)(K, sel_registerName("alloc")), sel_registerName("init"));
    CHECK("alloc/init de classe nova", o != nil && object_getClass(o) == K);
    Rect rr = ((Rect (*)(id, SEL, double))MSG_STRET)(o, sel_registerName("rect:"), 2.0);
    CHECK("objc_msgSend devolvendo HFA (d0..d3)", rr.x == 2 && rr.y == 4 && rr.w == 6 && rr.h == 8);
    Big bg = ((Big (*)(id, SEL, long))MSG_STRET)(o, sel_registerName("big:"), 10);
    CHECK("objc_msgSend com retorno indireto (x8)", bg.a == 10 && bg.e == 14);
    F2 f2 = ((F2 (*)(id, SEL, float))objc_msgSend)(o, sel_registerName("f2:"), 1.5f);
    CHECK("objc_msgSend com float e HFA de 2 float", f2.a == 1.5f && f2.b == 3.0f);
    double mx = ((double (*)(id, SEL, char, short, int, long, long, short, char, int, float, double, double, double, double, double))objc_msgSend)(
        o, sel_registerName("mix:s:i:l:l2:s2:c2:i2:f:d1:d2:d3:d4:d5:"), (char)-3, (short)4, -5, 6L, 7L, (short)-8, (char)9, 1, 0.5f, 1.0, 2.0, 3.0, 4.0, 5.0);
    CHECK("objc_msgSend com 16 argumentos (pilha empacotada, char/short com sinal)", mx == MIX);
    double mn = ((double (*)(id, SEL, long, long, long, long, long, long, long, long, double, double, double, double, double, double, double, double, double))objc_msgSend)(
        o, sel_registerName("many"), 1L, 2L, 3L, 4L, 5L, 6L, 7L, 8L, 1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0);
    CHECK("objc_msgSend com o 9º double e inteiros na pilha", mn == 36 + 36 + 9000);
    id nilr = ((id (*)(id, SEL))objc_msgSend)(nil, sel_registerName("qualquer"));
    double nild = ((double (*)(id, SEL))objc_msgSend)(nil, sel_registerName("qualquer"));
    CHECK("mensagem para nil devolve 0 (x0 e d0)", nilr == nil && nild == 0);

    // 4. NSInvocation do libSystem montando a chamada com o PCS da Apple
    Class MS = objc_getClass("NSMethodSignature"), IV = objc_getClass("NSInvocation");
    SEL ss_ = sel_registerName("signatureWithObjCTypes:"), si_ = sel_registerName("invocationWithMethodSignature:");
    SEL st_ = sel_registerName("setTarget:"), sl_ = sel_registerName("setSelector:"), sa_ = sel_registerName("setArgument:atIndex:");
    SEL gr_ = sel_registerName("getReturnValue:"), in_ = sel_registerName("invoke");
    {
        id sig = ((id (*)(Class, SEL, const char *))objc_msgSend)(MS, ss_, "d@:csiqqscifddddd");
        id inv = ((id (*)(Class, SEL, id))objc_msgSend)(IV, si_, sig);
        ((void (*)(id, SEL, id))objc_msgSend)(inv, st_, o);
        ((void (*)(id, SEL, SEL))objc_msgSend)(inv, sl_, sel_registerName("mix:s:i:l:l2:s2:c2:i2:f:d1:d2:d3:d4:d5:"));
        char c = -3, c2 = 9; short s = 4, s2 = -8; int i = -5, i2 = 1; long l = 6, l2 = 7; float f = 0.5f; double d[5] = {1, 2, 3, 4, 5};
        const void *args[] = {&c, &s, &i, &l, &l2, &s2, &c2, &i2, &f, &d[0], &d[1], &d[2], &d[3], &d[4]};
        for (long k = 0; k < 14; k++) ((void (*)(id, SEL, const void *, long))objc_msgSend)(inv, sa_, args[k], k + 2);
        ((void (*)(id, SEL))objc_msgSend)(inv, in_);
        double rv = 0;
        ((void (*)(id, SEL, void *))objc_msgSend)(inv, gr_, &rv);
        CHECK("NSInvocation com 16 argumentos (registradores, pilha, char/short)", rv == MIX);
    }
    {
        id sig = ((id (*)(Class, SEL, const char *))objc_msgSend)(MS, ss_, "{Rect=dddd}@:d");
        id inv = ((id (*)(Class, SEL, id))objc_msgSend)(IV, si_, sig);
        ((void (*)(id, SEL, id))objc_msgSend)(inv, st_, o);
        ((void (*)(id, SEL, SEL))objc_msgSend)(inv, sl_, sel_registerName("rect:"));
        double k = 3;
        ((void (*)(id, SEL, const void *, long))objc_msgSend)(inv, sa_, &k, 2);
        ((void (*)(id, SEL))objc_msgSend)(inv, in_);
        Rect rv = {0, 0, 0, 0};
        ((void (*)(id, SEL, void *))objc_msgSend)(inv, gr_, &rv);
        CHECK("NSInvocation devolvendo HFA", rv.x == 3 && rv.y == 6 && rv.w == 9 && rv.h == 12);
    }
    {
        id sig = ((id (*)(Class, SEL, const char *))objc_msgSend)(MS, ss_, "{Big=qqqqq}@:q");
        id inv = ((id (*)(Class, SEL, id))objc_msgSend)(IV, si_, sig);
        ((void (*)(id, SEL, id))objc_msgSend)(inv, st_, o);
        ((void (*)(id, SEL, SEL))objc_msgSend)(inv, sl_, sel_registerName("big:"));
        long k = 100;
        ((void (*)(id, SEL, const void *, long))objc_msgSend)(inv, sa_, &k, 2);
        ((void (*)(id, SEL))objc_msgSend)(inv, in_);
        long rv[5] = {0, 0, 0, 0, 0};
        ((void (*)(id, SEL, void *))objc_msgSend)(inv, gr_, rv);
        CHECK("NSInvocation com retorno indireto (x8, 40 bytes → 32 guardados)", rv[0] == 100 && rv[3] == 103);
    }

    // 5. blocks: trampolim de imp_implementationWithBlock e dispatch
    __block int visto = 0;
    double (^blk)(id, double, long) = ^double(id self, double x, long n) { visto = (self == o); return x * n; };
    IMP bimp = imp_implementationWithBlock((id)(void *)blk);
    class_addMethod(K, sel_registerName("blk:n:"), bimp, "d@:dq");
    double bv = ((double (*)(id, SEL, double, long))objc_msgSend)(o, sel_registerName("blk:n:"), 2.5, 4L);
    CHECK("imp_implementationWithBlock (trampolim: x1 = self, x0 = bloco)", bv == 10.0 && visto);
    dispatch_queue_t q = dispatch_queue_create("libsys.teste", 0);
    __block long soma = 0;
    dispatch_sync(q, ^{ for (int k = 1; k <= 100; k++) soma += k; });
    CHECK("dispatch_sync com bloco que captura __block", soma == 5050);
    dispatch_semaphore_t sem = dispatch_semaphore_create(0);
    __block double dv = 0;
    dispatch_async(q, ^{ dv = sqrt(2.0); dispatch_semaphore_signal(sem); });
    dispatch_semaphore_wait(sem, DISPATCH_TIME_FOREVER);
    CHECK("dispatch_async + semáforo", fabs(dv - 1.4142135623730951) < 1e-15);

    // 6. exceção C++ lançada dentro do comparador e pega aqui: atravessa os quadros do qsort do libSystem
    int v[8] = {5, 3, 13, 1, 8, 2, 7, 4};
    const char *msg = "";
    try { qsort(v, 8, sizeof v[0], cmp_lanca); } catch (const std::exception &e) { msg = e.what(); }
    CHECK("exceção C++ atravessando o qsort do libSystem", !strcmp(msg, "13 no qsort"));
    int w[6] = {9, 1, 8, 2, 7, 3};
    qsort(w, 6, sizeof w[0], [](const void *a, const void *b) { return *(const int *)a - *(const int *)b; });
    CHECK("qsort com callback do app", w[0] == 1 && w[5] == 9);

    {   // variantes com bloco da libc da Apple (sortb.c)
        typedef struct { int k, ord; } kv;
        kv d[7] = {{3, 0}, {1, 1}, {3, 2}, {2, 3}, {1, 4}, {3, 5}, {0, 6}};
        int (^porK)(const void *, const void *) = ^int(const void *a, const void *b) { return ((const kv *)a)->k - ((const kv *)b)->k; };
        int ms = mergesort_b(d, 7, sizeof d[0], porK);
        int estavel = ms == 0 && d[0].k == 0 && d[1].ord == 1 && d[2].ord == 4 && d[4].ord == 0 && d[5].ord == 2 && d[6].ord == 5;
        int h[9] = {9, 4, 7, 1, 8, 2, 6, 3, 5};
        heapsort_b(h, 9, sizeof h[0], ^int(const void *a, const void *b) { return *(const int *)a - *(const int *)b; });
        int chave = 6;
        int *achou = (int *)bsearch_b(&chave, h, 9, sizeof h[0], ^int(const void *a, const void *b) { return *(const int *)a - *(const int *)b; });
        CHECK("mergesort_b estável, heapsort_b e bsearch_b", estavel && h[0] == 1 && h[8] == 9 && achou == &h[5]);
    }

    // 7. setjmp/longjmp preservando registradores preservados (inclusive d8..d15 no arm64)
    static jmp_buf jb;
    volatile double keep = 1.25;
    volatile int passo = 0;
    if (setjmp(jb) == 0) { passo = 1; longjmp(jb, 5); }
    CHECK("setjmp/longjmp", passo == 1 && keep == 1.25);
    sigjmp_buf sj;
    int sr = sigsetjmp(sj, 1);
    if (sr == 0) siglongjmp(sj, 0);
    CHECK("siglongjmp(env, 0) volta 1", sr == 1);

    // 8. páginas e JIT
    long pg = sysconf(_SC_PAGESIZE);
    void *m = mmap(0, 100000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
    CHECK("página = 16 KiB no arm64 (4 KiB no x86_64), mmap alinhado",
          pg == getpagesize() && pg == (strcmp(arch, "arm64") ? 4096 : 16384) && m != MAP_FAILED && ((unsigned long)m & (pg - 1)) == 0);
    unsigned long long hp = 0; size_t hl = sizeof hp;
    CHECK("sysctl hw.pagesize (64 bits)", sysctlbyname("hw.pagesize", &hp, &hl, 0, 0) == 0 && hl == 8 && (long)hp == pg);
#if defined(__aarch64__)
    unsigned *code = (unsigned *)mmap(0, 16384, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_PRIVATE | MAP_ANON | MAP_JIT, -1, 0);
    pthread_jit_write_protect_np(0);
    code[0] = 0x52800540;                    // mov w0, #42
    code[1] = 0xd65f03c0;                    // ret
    pthread_jit_write_protect_np(1);
    sys_icache_invalidate(code, 8);
    int r42 = ((int (*)(void))code)();
    pthread_jit_write_protect_np(0);
    code[0] = 0x52800ce0;                    // mov w0, #103: código trocado no mesmo lugar
    pthread_jit_write_protect_np(1);
    sys_icache_invalidate(code, 4);
    int r103 = ((int (*)(void))code)();
    CHECK("MAP_JIT + sys_icache_invalidate (código gerado e trocado)", r42 == 42 && r103 == 103);
    char cpu[64] = ""; size_t cl = sizeof cpu;
    int lse = 0; size_t ll = sizeof lse;
    CHECK("sysctl do Apple Silicon (hw.machine, FEAT_LSE)", sysctlbyname("hw.machine", cpu, &cl, 0, 0) == 0 && !strcmp(cpu, "arm64") &&
          sysctlbyname("hw.optional.arm.FEAT_LSE", &lse, &ll, 0, 0) == 0 && lse == 1);
#endif

    // 9. libm (no arm64: compiler-rt do zig + atan2 próprio; no x86_64: x87)
    CHECK("sin/cos/tan", fabs(sin(1.0) - 0.8414709848078965) < 1e-15 && fabs(cos(1.0) - 0.5403023058681398) < 1e-15 && fabs(tan(0.5) - 0.5463024898437905) < 1e-15);
    CHECK("atan2/asin/acos/atan", fabs(atan2(1.0, 2.0) - 0.4636476090008061) < 1e-15 && fabs(atan2(-1.0, -1.0) + 2.356194490192345) < 1e-15 &&
          fabs(asin(0.5) - 0.5235987755982989) < 1e-15 && fabs(acos(0.5) - 1.0471975511965979) < 1e-15 && fabs(atan(10.0) - 1.4711276743037347) < 1e-15);
    CHECK("exp/log/pow/fmod/sqrt/floor", fabs(exp(1.0) - 2.718281828459045) < 1e-14 && fabs(log(10.0) - 2.302585092994046) < 1e-14 &&
          fabs(pow(2.0, 0.5) - 1.4142135623730951) < 1e-14 && fmod(7.5, 2.0) == 1.5 && sqrt(16.0) == 4.0 && floor(-1.5) == -2.0 && ceil(1.2) == 2.0 && trunc(-2.7) == -2.0);

    // 11. exceções Objective-C (objc_exc.c: __cxa_throw da libc++abi do disco + libunwind do libSystem)
    {
        const char *nm = "";
        @try {
            [[NSException exceptionWithName:(id)S("MeuErro") reason:(id)S("motivo") userInfo:nil] raise];
        } @catch (NSException *e) { nm = U([e name]); }
        CHECK("@throw (-[NSException raise]) e @catch (NSException *)", !strcmp(nm, "MeuErro"));
        int fim = 0, pegou = 0;
        @try {
            @try { @throw [[LibsysAbiErro alloc] init]; }
            @catch (NSException *e) { pegou = 1; }          // não casa: a classe é outra
            @finally { fim = 1; }
        } @catch (LibsysAbiSonda *e) { pegou = pegou ? -1 : 2; }   // casa pela superclasse
        CHECK("@catch pela superclasse, @finally e @catch que não casa", fim == 1 && pegou == 2);
        id quem = nil;
        id objs[3] = {(id)S("a"), (id)S("b"), (id)S("c")};
        id arr = [NSArray arrayWithObjects:objs count:3];
        @try {
            [arr enumerateObjectsUsingBlock:^(id obj, unsigned long idx, signed char *stop) { (void)stop; if (idx == 1) @throw obj; }];
        } @catch (id e) { quem = e; }
        CHECK("@throw dentro de bloco do libSystem (enumerateObjectsUsingBlock) e @catch (id)", quem && !strcmp(U(quem), "b"));
        int cxx = 0;
        try { @throw [[LibsysAbiErro alloc] init]; } catch (...) { cxx = 1; }
        CHECK("exceção ObjC pega por catch(...) do C++", cxx == 1);
        int objc = 0;
        @try { try { throw std::runtime_error("c++"); } catch (const std::exception &) { objc = 1; } } @catch (id e) { objc = -1; }
        CHECK("exceção C++ não casa com @catch (id)", objc == 1);
    }

    // 10. AppKit do mOS: NSWindow com o retângulo HFA de ida (initWithContentRect:) e de volta (frame), NSView
    {
        id app = ((id (*)(Class, SEL))objc_msgSend)(objc_getClass("NSApplication"), sel_registerName("sharedApplication"));
        Rect cr = {100, 120, 640, 480};
        id win = ((id (*)(id, SEL, Rect, unsigned long, unsigned long, signed char))objc_msgSend)(
            ((id (*)(Class, SEL))objc_msgSend)(objc_getClass("NSWindow"), sel_registerName("alloc")),
            sel_registerName("initWithContentRect:styleMask:backing:defer:"), cr, 15ul, 2ul, (signed char)1);
        ((void (*)(id, SEL, id))objc_msgSend)(win, sel_registerName("setTitle:"), S("janela arm64"));
        Rect fr = ((Rect (*)(id, SEL))MSG_STRET)(win, sel_registerName("frame"));
        const char *t = U(((id (*)(id, SEL))objc_msgSend)(win, sel_registerName("title")));
        printf("  NSWindow frame {%g, %g, %g, %g} título \"%s\"\n", fr.x, fr.y, fr.w, fr.h, t);
        CHECK("NSApplication + NSWindow (HFA de ida e volta, título)", app && win && fr.w >= 640 && fr.h >= 480 && !strcmp(t, "janela arm64"));
        Rect vr = {0, 0, 50, 25};
        id v = ((id (*)(id, SEL, Rect))objc_msgSend)(((id (*)(Class, SEL))objc_msgSend)(objc_getClass("NSView"), sel_registerName("alloc")),
                                                    sel_registerName("initWithFrame:"), vr);
        Rect vb = ((Rect (*)(id, SEL))MSG_STRET)(v, sel_registerName("bounds"));
        CHECK("NSView initWithFrame:/bounds", v && vb.w == 50 && vb.h == 25);
    }

    printf("[%s] libsys-abi: %d falhas\n", arch, falhas);
    return falhas;
}
