/*
 * mOS — CFRuntime do SwiftUI.framework (variante com interop ObjC, docs/swift-objc-runtime.md, fase 6).
 *
 * O Compute (grafo de atributos) e o CoreText do OpenSwiftUI registram tipos CF próprios (Graph, Subgraph, CTFont...)
 * com _CFRuntimeRegisterClass/_CFRuntimeCreateInstance. Na variante sem ObjC (userland/sdk/openswiftui/src/cf_mos.cpp)
 * eles viram objetos Swift nativos. Aqui o Swift tem interop ObjC e retém tipos CF com objc_retain/release, então cada
 * instância é um objeto ObjC do libSystem do mOS: classe _MOSCFType (uma por processo), alocada com class_createInstance
 * (o contador fica fora do objeto, no cabeçalho do libSystem). O layout do CFRuntimeBase do swift-corelibs
 * (DEPLOYMENT_RUNTIME_SWIFT, 24 bytes) é mantido: isa, palavra livre e _cfinfoa, onde guardamos o índice do tipo.
 *  -dealloc chama o finalize da CFRuntimeClass; -_cfTypeID responde ao CFGetTypeID do libSystem (cfcollections.c);
 *  -isEqual:/-hash/-description usam equal/hash/copyDebugDesc quando existem.
 * Os nomes mos_oag_CF* são os que o prelude mos_openswiftui_prelude.h põe no C/C++ das dependências.
 */
#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef const void *CFTypeRef;
typedef long CFIndex;
typedef unsigned long CFTypeID;
typedef unsigned long CFHashCode;
typedef const void *CFAllocatorRef;
typedef const void *CFStringRef;
typedef const void *CFDictionaryRef;
typedef unsigned char Boolean;
typedef struct objc_class *Class;
typedef struct objc_object *id;
typedef struct objc_selector *SEL;
typedef id (*IMP)(id, SEL, ...);

typedef struct {
    CFIndex version;
    const char *className;
    void (*init)(CFTypeRef cf);
    CFTypeRef (*copy)(CFAllocatorRef allocator, CFTypeRef cf);
    void (*finalize)(CFTypeRef cf);
    Boolean (*equal)(CFTypeRef cf1, CFTypeRef cf2);
    CFHashCode (*hash)(CFTypeRef cf);
    CFStringRef (*copyFormattingDesc)(CFTypeRef cf, CFDictionaryRef formatOptions);
    CFStringRef (*copyDebugDesc)(CFTypeRef cf);
    void (*reclaim)(CFTypeRef cf);
    uint32_t (*refcount)(intptr_t op, CFTypeRef cf);
    uintptr_t requiredAlignment;
} CFRuntimeClass;

Class objc_getClass(const char *name);
Class objc_allocateClassPair(Class super, const char *name, size_t extra);
void objc_registerClassPair(Class c);
Boolean class_addMethod(Class c, SEL s, IMP imp, const char *types);
IMP class_getMethodImplementation(Class c, SEL s);
id class_createInstance(Class c, size_t extra);
SEL sel_registerName(const char *name);
id objc_retain(id o);
void objc_release(id o);
id objc_autorelease(id o);
Class object_getClass(id o);
CFStringRef CFStringCreateWithCString(CFAllocatorRef a, const char *s, unsigned enc);

#define CFBASE 24                /* isa, palavra livre, _cfinfoa */
#define INFO_TAG 0x4D4F534346000000ull   /* "MOSCF" nos bits altos de _cfinfoa */
enum { MAX_TYPES = 64 };
static const CFRuntimeClass *g_types[MAX_TYPES];
static int g_count;
static Class g_cls;
static IMP g_super_dealloc;

static uint64_t *info(const void *o) { return (uint64_t *)((uint8_t *)o + 16); }
static int type_index(const void *o) {
    if (!o || !g_cls || object_getClass((id)o) != g_cls) return -1;
    uint64_t v = *info(o);
    if ((v & 0xFFFFFFFFFF000000ull) != INFO_TAG) return -1;
    int i = (int)(v & 0xFFFF);
    return i < g_count ? i : -1;
}

static void m_dealloc(id self, SEL s) {
    int i = type_index(self);
    if (i >= 0 && g_types[i]->finalize) g_types[i]->finalize((CFTypeRef)self);
    g_super_dealloc(self, s);
}
static CFTypeID m_typeid(id self, SEL s) { (void)s; int i = type_index(self); return i >= 0 ? 0x400 + (CFTypeID)i : 1; }
static Boolean m_equal(id self, SEL s, id other) {
    (void)s;
    if (self == other) return 1;
    int i = type_index(self);
    if (i < 0 || type_index(other) != i || !g_types[i]->equal) return 0;
    return g_types[i]->equal((CFTypeRef)self, (CFTypeRef)other);
}
static CFHashCode m_hash(id self, SEL s) {
    (void)s;
    int i = type_index(self);
    return (i >= 0 && g_types[i]->hash) ? g_types[i]->hash((CFTypeRef)self) : (CFHashCode)(uintptr_t)self;
}
static id m_description(id self, SEL s) {
    (void)s;
    int i = type_index(self);
    if (i >= 0 && g_types[i]->copyDebugDesc) { CFStringRef d = g_types[i]->copyDebugDesc((CFTypeRef)self); if (d) return objc_autorelease((id)d); }
    return objc_autorelease((id)CFStringCreateWithCString(0, i >= 0 && g_types[i]->className ? g_types[i]->className : "CFType", 0x08000100));
}

static Class cf_class(void) {
    if (g_cls) return g_cls;
    Class c = objc_getClass("_MOSCFType");
    if (!c) {
        Class ns = objc_getClass("NSObject");
        c = objc_allocateClassPair(ns, "_MOSCFType", 0);
        class_addMethod(c, sel_registerName("dealloc"), (IMP)m_dealloc, "v@:");
        class_addMethod(c, sel_registerName("_cfTypeID"), (IMP)m_typeid, "Q@:");
        class_addMethod(c, sel_registerName("isEqual:"), (IMP)m_equal, "c@:@");
        class_addMethod(c, sel_registerName("hash"), (IMP)m_hash, "Q@:");
        class_addMethod(c, sel_registerName("description"), (IMP)m_description, "@@:");
        objc_registerClassPair(c);
    }
    g_super_dealloc = class_getMethodImplementation(objc_getClass("NSObject"), sel_registerName("dealloc"));
    g_cls = c;
    return c;
}

CFTypeID _CFRuntimeRegisterClass(const CFRuntimeClass *const cls) {
    if (!cls || g_count >= MAX_TYPES) return 0;
    g_types[g_count] = cls;
    return 0x400 + (CFTypeID)g_count++;
}

const CFRuntimeClass *_CFRuntimeGetClassWithTypeID(CFTypeID typeID) {
    CFTypeID i = typeID - 0x400;
    return (typeID >= 0x400 && i < (CFTypeID)g_count) ? g_types[i] : 0;
}

/* extraBytes = tamanho da struct além do CFRuntimeBase de 24 bytes */
CFTypeRef _CFRuntimeCreateInstance(CFAllocatorRef allocator, CFTypeID typeID, CFIndex extraBytes, unsigned char *category) {
    (void)allocator; (void)category;
    CFTypeID i = typeID - 0x400;
    if (typeID < 0x400 || i >= (CFTypeID)g_count || extraBytes < 0) return 0;
    Class c = cf_class();
    /* o NSObject do mOS ocupa 8 bytes (isa); o resto do CFRuntimeBase e a struct vão como bytes extras */
    id o = class_createInstance(c, (size_t)(CFBASE - 8) + (size_t)extraBytes);
    if (!o) return 0;
    memset((uint8_t *)o + 8, 0, (size_t)(CFBASE - 8) + (size_t)extraBytes);
    *info(o) = INFO_TAG | (uint64_t)i;
    if (g_types[i]->init) g_types[i]->init((CFTypeRef)o);
    return (CFTypeRef)o;
}

void _CFRuntimeSetInstanceTypeID(CFTypeRef cf, CFTypeID typeID) {
    if (type_index(cf) >= 0 && typeID >= 0x400) *info(cf) = INFO_TAG | (uint64_t)(typeID - 0x400);
}

/* o C/C++ das dependências chama estes nomes (prelude); os objetos são todos ObjC do libSystem */
CFTypeRef mos_oag_CFRetain(CFTypeRef cf) { return (uintptr_t)cf >= 0x10000 ? (CFTypeRef)objc_retain((id)cf) : cf; }
void mos_oag_CFRelease(CFTypeRef cf) { if ((uintptr_t)cf >= 0x10000) objc_release((id)cf); }
CFIndex mos_oag_CFGetRetainCount(CFTypeRef cf) { (void)cf; return 1; }
