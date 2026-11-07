/*
 * mOS — teste da fase 1 de docs/swift-objc-runtime.md: a libobjc do mOS (userland/libsystem/objc.c) com objetos
 * no estilo do Swift com interop ObjC. A classe raiz SWObj imita o Swift._SwiftObject do runtime Swift: memória do
 * malloc (sem o cabeçalho do mOS), contagem dentro do objeto e -retain/-release próprios; o dealloc chama
 * objc_destructInstance, como o swift_deallocClassInstance. Sem ARC (classe raiz com RR próprio).
 *   .\tools\build-swiftobjctest.ps1
 *   $env:MACHOTEST_PATH = '/SwiftObjcTest.app/Contents/MacOS/SwiftObjcTest'
 *   .\build\machotest.exe build\libSystem.mos build\swiftobjctest build\swiftobjctest\SwiftObjcTest.app\Contents\MacOS\SwiftObjcTest
 * Imprime "swiftobjctest: N/N ok".
 */
#import <Foundation/Foundation.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

id objc_retain(id);
void objc_release(id);
id objc_autorelease(id);
void *objc_autoreleasePoolPush(void);
void objc_autoreleasePoolPop(void *);
id objc_storeWeak(id *, id);
id objc_initWeak(id *, id);
id objc_loadWeakRetained(id *);
void objc_destroyWeak(id *);
void *objc_destructInstance(id);
void objc_setAssociatedObject(id, const void *, id, unsigned long);
id objc_getAssociatedObject(id, const void *);
BOOL objc_isUniquelyReferenced(id);
Class objc_getClass(const char *);
Class _objc_realizeClassFromSwift(Class, const void *);
Class objc_readClassPair(Class, const void *);
const char *class_getImageName(Class);
const char *class_getName(Class);
Class class_getSuperclass(Class);
size_t class_getInstanceSize(Class);
BOOL object_isClass(id);
id objc_msgSend(id, SEL, ...);
typedef BOOL (*getclass_hook_t)(const char *, Class *);
void objc_setHook_getClass(getclass_hook_t, getclass_hook_t *);
extern const unsigned long objc_debug_isa_class_mask;

static int ok, total;
static void check(int cond, const char *what) {
    total++;
    if (cond) ok++;
    printf("%s %s\n", cond ? "  ok  " : "  FALHA", what);
    fflush(stdout);
}

static int deallocs, weakly, assocnote;

__attribute__((objc_root_class))
@interface SWObj { Class cls; long rc; int weak; }
+ (id)alloc;
+ (id)self;
+ (Class)class;
+ (id)new;
- (id)init;
- (id)retain;
- (oneway void)release;
- (id)autorelease;
- (unsigned long)retainCount;
- (BOOL)allowsWeakReference;
- (void)_setWeaklyReferenced;
- (void)_noteAssociatedObjects;
- (void)dealloc;
@end
@implementation SWObj
+ (id)alloc { SWObj *o = calloc(1, class_getInstanceSize(self)); o->cls = self; o->rc = 1; return o; }
+ (id)new { return [[self alloc] init]; }
+ (id)self { return self; }
+ (Class)class { return self; }
+ (id)retain { return self; }
+ (oneway void)release {}
+ (id)autorelease { return self; }
- (id)init { return self; }
- (id)retain { __sync_fetch_and_add(&rc, 1); return self; }
- (oneway void)release { if (__sync_sub_and_fetch(&rc, 1) == 0) [self dealloc]; }
- (id)autorelease { return objc_autorelease(self); }
- (unsigned long)retainCount { return (unsigned long)rc; }
- (BOOL)allowsWeakReference { return YES; }
- (void)_setWeaklyReferenced { weak = 1; weakly++; }
- (void)_noteAssociatedObjects { assocnote++; }
- (void)dealloc { deallocs++; objc_destructInstance(self); free(self); }
@end

@interface SWSub : SWObj { @public long extra; }
- (int)answer;
@end
@implementation SWSub
- (int)answer { return 42; }
@end

static int hook_calls;
static getclass_hook_t old_hook;
static BOOL my_hook(const char *name, Class *out) {
    hook_calls++;
    if (!strcmp(name, "_TtC4Test8Mangled")) { *out = [SWSub self]; return YES; }
    return old_hook ? old_hook(name, out) : NO;
}

static long rc_of(id o) { return (long)[o retainCount]; }

int main(void) {
    printf("swiftobjctest: libobjc do mOS com objetos estilo Swift\n");

    SWSub *s = [SWSub new];
    check(s && [s answer] == 42, "subclasse de raiz com RR próprio recebe mensagem");
    check(rc_of(s) == 1, "contagem inicial 1 (dentro do objeto)");

    objc_retain(s);
    check(rc_of(s) == 2, "objc_retain encaminha para -retain");
    objc_release(s);
    check(rc_of(s) == 1, "objc_release encaminha para -release");
    check(objc_isUniquelyReferenced(s), "objc_isUniquelyReferenced com contagem 1");
    objc_retain(s);
    check(!objc_isUniquelyReferenced(s), "objc_isUniquelyReferenced com contagem 2");
    objc_release(s);

    void *pool = objc_autoreleasePoolPush();
    objc_autorelease(objc_retain(s));
    check(rc_of(s) == 2, "autorelease guarda a referência");
    objc_autoreleasePoolPop(pool);
    check(rc_of(s) == 1, "fim do pool solta a referência");

    id w = nil;
    objc_initWeak(&w, s);
    check(w == s && weakly == 1, "referência fraca aceita e _setWeaklyReferenced chamado");
    id strong = objc_loadWeakRetained(&w);
    check(strong == s && rc_of(s) == 2, "objc_loadWeakRetained devolve retido");
    objc_release(strong);

    static char key;
    id val = [NSObject new];
    objc_setAssociatedObject(s, &key, val, 1 /* RETAIN_NONATOMIC */);
    check(assocnote == 1 && objc_getAssociatedObject(s, &key) == val, "objeto associado e _noteAssociatedObjects");

    int before = deallocs;
    objc_release(s);
    check(deallocs == before + 1, "contagem zero chama -dealloc");
    check(w == nil, "objc_destructInstance zera a referência fraca");
    objc_destroyWeak(&w);

    SWObj *root = [SWObj new];
    objc_retain(root); objc_release(root);
    check(rc_of(root) == 1, "classe raiz também encaminha");
    objc_release(root);

    /* NSObject continua com a contagem do mOS */
    id n = [NSObject new];
    objc_retain(n);
    check([n retainCount] == 2, "NSObject inalterado");
    objc_release(n); objc_release(n);

    Class c = [SWSub self];
    check(_objc_realizeClassFromSwift(c, c) == c && objc_readClassPair(c, 0) == c, "_objc_realizeClassFromSwift/objc_readClassPair");
    check(class_getSuperclass(c) == [SWObj self] && !strcmp(class_getName(c), "SWSub"), "superclasse e nome");
    check(object_isClass((id)c) && !object_isClass([NSObject new]), "object_isClass");
    const char *img = class_getImageName(c);
    check(img && strstr(img, "SwiftObjcTest"), "class_getImageName aponta para o executável");
    check(((unsigned long)c & objc_debug_isa_class_mask) == (unsigned long)c, "objc_debug_isa_class_mask preserva o isa");

    objc_setHook_getClass(my_hook, &old_hook);
    check(objc_getClass("_TtC4Test8Mangled") == c && hook_calls == 1, "objc_setHook_getClass: nome mangled pelo gancho");
    check(objc_getClass("NSObject") == [NSObject self], "objc_getClass normal sem passar pelo gancho");
    check(objc_getClass("NaoExiste123") == nil, "classe inexistente");

    printf("swiftobjctest: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
