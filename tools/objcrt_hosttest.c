/* Teste de host do runtime ObjC do mOS (objc.c + objcrt.c): protocolos montados em tempo de execução, ivars por nome. */
#include <stdio.h>
#include <string.h>
typedef void *P; typedef const char *SEL; typedef signed char BOOL;
typedef struct { SEL name; const char *types; } md_t;
typedef struct { const char *name, *value; } attr_t;
P objc_allocateProtocol(const char *); void objc_registerProtocol(P); P objc_getProtocol(const char *);
void protocol_addMethodDescription(P, SEL, const char *, BOOL, BOOL); void protocol_addProtocol(P, P);
void protocol_addProperty(P, const char *, const attr_t *, unsigned, BOOL, BOOL);
md_t *protocol_copyMethodDescriptionList(P, BOOL, BOOL, unsigned *); P *protocol_copyPropertyList(P, unsigned *);
P *protocol_copyPropertyList2(P, unsigned *, BOOL, BOOL); BOOL protocol_conformsToProtocol(P, P);
const char *property_getName(P); const char *property_getAttributes(P); SEL sel_registerName(const char *);
P objc_allocateClassPair(P, const char *, unsigned long); BOOL class_addIvar(P, const char *, unsigned long, unsigned char, const char *);
void objc_registerClassPair(P); P class_createInstance(P, unsigned long);
P object_getInstanceVariable(P, const char *, void **); P object_setInstanceVariable(P, const char *, void *);
BOOL objc_collectingEnabled(void);
int total, fails;
#define CHECK(c, m) do { total++; if (c) printf("  ok    %s\n", m); else { fails++; printf("  FALHA %s\n", m); } } while (0)
int main(void) {
    P base = objc_allocateProtocol("THBase"); objc_registerProtocol(base);
    P p = objc_allocateProtocol("THProto");
    CHECK(p && !objc_getProtocol("THProto"), "em construção não aparece no objc_getProtocol");
    protocol_addMethodDescription(p, sel_registerName("go:"), "v24@0:8@16", 1, 1);
    protocol_addMethodDescription(p, sel_registerName("opt"), "v16@0:8", 0, 1);
    protocol_addProtocol(p, base);
    attr_t at[] = {{"T", "@\"NSString\""}, {"C", ""}, {"N", ""}};
    protocol_addProperty(p, "title", at, 3, 1, 1);
    protocol_addProperty(p, "shared", at, 1, 1, 0);
    objc_registerProtocol(p);
    CHECK(objc_getProtocol("THProto") == p, "registrado: objc_getProtocol acha");
    unsigned n = 0; md_t *ml = protocol_copyMethodDescriptionList(p, 1, 1, &n);
    CHECK(n == 1 && !strcmp(ml[0].name, "go:") && !strcmp(ml[0].types, "v24@0:8@16"), "método obrigatório de instância");
    ml = protocol_copyMethodDescriptionList(p, 0, 1, &n);
    CHECK(n == 1 && !strcmp(ml[0].name, "opt"), "método opcional de instância");
    CHECK(protocol_conformsToProtocol(p, base), "addProtocol: herda THBase");
    P *pl = protocol_copyPropertyList(p, &n);
    CHECK(n == 1 && !strcmp(property_getName(pl[0]), "title") && !strcmp(property_getAttributes(pl[0]), "T@\"NSString\",C,N"), "copyPropertyList: title com T@\"NSString\",C,N");
    pl = protocol_copyPropertyList2(p, &n, 1, 0);
    CHECK(n == 1 && !strcmp(property_getName(pl[0]), "shared"), "copyPropertyList2: propriedade de classe");
    protocol_addMethodDescription(p, sel_registerName("late"), "v16@0:8", 1, 1);
    protocol_copyMethodDescriptionList(p, 1, 1, &n);
    CHECK(n == 1, "depois de registrado não aceita método novo");
    P c = objc_allocateClassPair(0, "THObj", 0);
    class_addIvar(c, "_v", 8, 3, "@");
    objc_registerClassPair(c);
    P o = class_createInstance(c, 0);
    void *val = (void *)1;
    CHECK(object_setInstanceVariable(o, "_v", (void *)0x1234) != 0, "object_setInstanceVariable acha _v");
    CHECK(object_getInstanceVariable(o, "_v", &val) != 0 && val == (void *)0x1234, "object_getInstanceVariable lê 0x1234");
    CHECK(object_getInstanceVariable(o, "_nao", &val) == 0 && val == 0, "ivar inexistente: NULL e valor 0");
    CHECK(!objc_collectingEnabled(), "objc_collectingEnabled = NO");
    printf("objcrt_hosttest: %d de %d ok\n", total - fails, total);
    return fails;
}
