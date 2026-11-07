/*
 * mOS — teste do runtime Objective-C moderno (userland/libsystem/objcrt.c): protocolos, objetos associados,
 * introspecção, codificação de tipos e imp_implementationWithBlock. Compila igual no macOS
 * (clang -fobjc-arc main.m -framework Foundation). No mOS: .\tools\build-objcrttest.ps1 e rode no harness:
 *   $env:MACHOTEST_PATH = '/ObjcRtTest.app/Contents/MacOS/ObjcRtTest'
 *   .\build\machotest.exe build\libSystem.mos build\objcrttest build\objcrttest\ObjcRtTest.app\Contents\MacOS\ObjcRtTest
 * Imprime "objcrttest: N/N ok".
 */
#import <Foundation/Foundation.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* objc/runtime.h (o SDK do mOS não traz): os protótipos com os tipos da Apple */
#ifndef __APPLE_SDK_RUNTIME__
typedef id (*IMP)(id, SEL, ...);
id objc_msgSend(id, SEL, ...);
@interface NSObject (MOSTesteProtocolo)
- (BOOL)conformsToProtocol:(Protocol *)p;
+ (BOOL)conformsToProtocol:(Protocol *)p;
@end
#endif
typedef struct objc_method *Method;
typedef struct objc_ivar *Ivar;
typedef struct objc_property *objc_property_t;
typedef struct { const char *name, *value; } objc_property_attribute_t;
struct objc_method_description { SEL name; char *types; };
Protocol *objc_getProtocol(const char *);
const char *protocol_getName(Protocol *);
BOOL protocol_conformsToProtocol(Protocol *, Protocol *);
struct objc_method_description protocol_getMethodDescription(Protocol *, SEL, BOOL, BOOL);
BOOL class_conformsToProtocol(Class, Protocol *);
BOOL class_addProtocol(Class, Protocol *);
Protocol *__unsafe_unretained *class_copyProtocolList(Class, unsigned *);
void objc_setAssociatedObject(id, const void *, id, unsigned long);
id objc_getAssociatedObject(id, const void *);
void objc_removeAssociatedObjects(id);
Method *class_copyMethodList(Class, unsigned *);
Ivar *class_copyIvarList(Class, unsigned *);
objc_property_t *class_copyPropertyList(Class, unsigned *);
objc_property_t class_getProperty(Class, const char *);
const char *property_getName(objc_property_t);
const char *property_getAttributes(objc_property_t);
char *property_copyAttributeValue(objc_property_t, const char *);
objc_property_attribute_t *property_copyAttributeList(objc_property_t, unsigned *);
SEL method_getName(Method);
unsigned method_getNumberOfArguments(Method);
char *method_copyReturnType(Method);
char *method_copyArgumentType(Method, unsigned);
Method class_getInstanceMethod(Class, SEL);
const char *ivar_getName(Ivar);
const char *ivar_getTypeEncoding(Ivar);
IMP imp_implementationWithBlock(id);
id imp_getBlock(IMP);
BOOL imp_removeBlock(IMP);
BOOL class_addMethod(Class, SEL, IMP, const char *);
int objc_getClassList(Class __unsafe_unretained *, int);
Class objc_getClass(const char *);
#define OBJC_ASSOCIATION_RETAIN_NONATOMIC 1
#define OBJC_ASSOCIATION_COPY_NONATOMIC 3
#define OBJC_ASSOCIATION_ASSIGN 0

static int ok, total;
static void check(int cond, const char *what) {
    total++;
    if (cond) ok++;
    printf("%s %s\n", cond ? "  ok  " : "  FALHA", what);
    fflush(stdout);
}

@protocol Base
- (int)baseMethod;
@end
@protocol Desenhavel <Base>
- (void)desenhar:(int)vezes;
@optional
- (double)opcional;
@property (nonatomic, readonly) int lados;
@end
@protocol Extra
@end
@protocol Categoria
@end

@interface Forma : NSObject <Desenhavel> {
    int _cantos;
    double _area;
}
@property (nonatomic, copy) NSString *nome;
@property (nonatomic, readonly) int lados;
- (int)somar:(int)a com:(double)b;
@end
@implementation Forma
- (int)baseMethod { return 1; }
- (void)desenhar:(int)vezes { (void)vezes; }
- (int)lados { return 4; }
- (int)somar:(int)a com:(double)b { return a + (int)b; }
@end
@interface Quadrado : Forma
@end
@implementation Quadrado
@end
@interface Forma (Cat) <Categoria>
@end
@implementation Forma (Cat)
@end
@interface Solta : NSObject
@end
@implementation Solta
@end

static int deallocs;
@interface Sentinela : NSObject
@end
@implementation Sentinela
- (void)dealloc { deallocs++; }
@end

static char k1, k2, k3;

int main(void) {
    @autoreleasepool {
        /* protocolos */
        Protocol *pd = @protocol(Desenhavel);
        check(pd && !strcmp(protocol_getName(pd), "Desenhavel"), "@protocol + protocol_getName");
        check(objc_getProtocol("Desenhavel") == pd, "objc_getProtocol acha o mesmo objeto");
        check(protocol_conformsToProtocol(pd, @protocol(Base)) && !protocol_conformsToProtocol(pd, @protocol(Extra)),
              "herança de protocolos");
        struct objc_method_description d = protocol_getMethodDescription(pd, @selector(desenhar:), YES, YES);
        struct objc_method_description o = protocol_getMethodDescription(pd, @selector(opcional), NO, YES);
        struct objc_method_description b = protocol_getMethodDescription(pd, @selector(baseMethod), YES, YES);
        check(d.name == @selector(desenhar:) && d.types && !strncmp(d.types, "v", 1) && o.name == @selector(opcional) &&
              b.name == @selector(baseMethod), "protocol_getMethodDescription (obrigatório, opcional, herdado)");
        check(class_conformsToProtocol([Forma class], pd) && class_conformsToProtocol([Quadrado class], @protocol(Base)) &&
              !class_conformsToProtocol([Solta class], pd), "class_conformsToProtocol pela classe, superclasse e protocolo-pai");
        check([[Quadrado new] conformsToProtocol:@protocol(Desenhavel)] && ![[Solta new] conformsToProtocol:@protocol(Desenhavel)] &&
              [Forma conformsToProtocol:@protocol(Base)], "-conformsToProtocol: e +conformsToProtocol:");
        check(class_conformsToProtocol([Forma class], @protocol(Categoria)), "protocolo declarado numa categoria");
        check(!class_conformsToProtocol([Solta class], @protocol(Extra)) && class_addProtocol([Solta class], @protocol(Extra)) &&
              class_conformsToProtocol([Solta class], @protocol(Extra)) && !class_addProtocol([Solta class], @protocol(Extra)),
              "class_addProtocol");
        unsigned np = 0;
        Protocol *__unsafe_unretained *pl = class_copyProtocolList([Forma class], &np);
        check(pl && np == 2, "class_copyProtocolList (Desenhavel + Categoria)");
        free(pl);
        check([@"x" conformsToProtocol:@protocol(NSCopying)], "classes nativas do mOS seguem conformando (NSString/NSCopying)");

        /* objetos associados */
        Solta *dono = [Solta new];
        NSMutableString *m = [NSMutableString stringWithString:@"abc"];
        objc_setAssociatedObject(dono, &k1, m, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        objc_setAssociatedObject(dono, &k2, m, OBJC_ASSOCIATION_COPY_NONATOMIC);
        objc_setAssociatedObject(dono, &k3, m, OBJC_ASSOCIATION_ASSIGN);
        [m appendString:@"d"];
        check(objc_getAssociatedObject(dono, &k1) == m && [objc_getAssociatedObject(dono, &k2) isEqual:@"abc"] &&
              objc_getAssociatedObject(dono, &k3) == m, "retain, copy e assign");
        objc_setAssociatedObject(dono, &k1, nil, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        check(objc_getAssociatedObject(dono, &k1) == nil, "valor nil apaga a associação");
        @autoreleasepool {
            Solta *dono2 = [Solta new];
            objc_setAssociatedObject(dono2, &k1, [Sentinela new], OBJC_ASSOCIATION_RETAIN_NONATOMIC);
            check(deallocs == 0, "o valor associado fica vivo enquanto o dono vive");
            dono2 = nil;
        }
        check(deallocs == 1, "o valor associado morre com o dono");
        objc_removeAssociatedObjects(dono);
        check(objc_getAssociatedObject(dono, &k2) == nil, "objc_removeAssociatedObjects");

        /* introspecção */
        unsigned nm = 0, ni = 0, npr = 0;
        Method *ml = class_copyMethodList([Forma class], &nm);
        int achou = 0;
        for (unsigned i = 0; i < nm; i++) if (method_getName(ml[i]) == @selector(somar:com:)) achou = 1;
        check(ml && nm >= 5 && achou, "class_copyMethodList");
        free(ml);
        Ivar *il = class_copyIvarList([Forma class], &ni);
        check(il && ni == 3 && !strcmp(ivar_getName(il[0]), "_cantos") && !strcmp(ivar_getTypeEncoding(il[1]), "d"),
              "class_copyIvarList + ivar_getTypeEncoding");
        free(il);
        objc_property_t *prl = class_copyPropertyList([Forma class], &npr);
        check(prl && npr >= 2, "class_copyPropertyList");
        free(prl);
        objc_property_t pn = class_getProperty([Quadrado class], "nome");
        char *ivn = property_copyAttributeValue(pn, "V");
        unsigned na = 0;
        objc_property_attribute_t *al = property_copyAttributeList(pn, &na);
        check(pn && !strcmp(property_getName(pn), "nome") && property_getAttributes(pn)[0] == 'T' && ivn && !strcmp(ivn, "_nome") &&
              na >= 3 && !strcmp(al[0].name, "T") && !strcmp(al[0].value, "@\"NSString\""),
              "class_getProperty (herdada), atributos e property_copyAttributeValue");
        free(ivn);
        free(al);
        Method sm = class_getInstanceMethod([Forma class], @selector(somar:com:));
        char *rt = method_copyReturnType(sm), *a2 = method_copyArgumentType(sm, 3);
        check(method_getNumberOfArguments(sm) == 4 && !strcmp(rt, "i") && !strcmp(a2, "d"),
              "method_getNumberOfArguments / copyReturnType / copyArgumentType");
        free(rt);
        free(a2);
        int nc = objc_getClassList(NULL, 0);
        Class __unsafe_unretained *cl = (Class __unsafe_unretained *)calloc((size_t)nc, sizeof(Class));
        objc_getClassList(cl, nc);
        int tem = 0;
        for (int i = 0; i < nc; i++) if (cl[i] == [Quadrado class]) tem = 1;
        check(nc > 10 && tem, "objc_getClassList");
        free(cl);

        /* imp_implementationWithBlock */
        __block int chamadas = 0;
        IMP imp = imp_implementationWithBlock(^int(id selfObj, int a, int b2) { chamadas++; return selfObj ? a * b2 : -1; });
        check(imp && class_addMethod([Solta class], @selector(multiplicar:por:), imp, "i@:ii"), "imp_implementationWithBlock + class_addMethod");
        int r = ((int (*)(id, SEL, int, int))objc_msgSend)(dono, @selector(multiplicar:por:), 6, 7);
        check(r == 42 && chamadas == 1, "o bloco recebe self e os argumentos (6 * 7)");
        IMP impd = imp_implementationWithBlock(^double(id selfObj, double x) { (void)selfObj; return x / 2; });
        class_addMethod([Solta class], @selector(metade:), impd, "d@:d");
        double h = ((double (*)(id, SEL, double))objc_msgSend)(dono, @selector(metade:), 5.0);
        check(h == 2.5, "argumento e retorno em ponto flutuante");
        check(imp_getBlock(imp) != nil && imp_removeBlock(imp) && imp_getBlock(imp) == nil, "imp_getBlock / imp_removeBlock");
    }
    printf("objcrttest: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
