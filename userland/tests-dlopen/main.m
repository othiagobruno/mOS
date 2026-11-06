/* mOS — teste do dlopen em tempo de execução (kernel/compat/macho.c + userland/libsystem/dlfcn.c).
 * Bundle: DLTest.app/Contents/MacOS/{DLTest, Plugin.bundle, libdep.dylib}. Imprime "dlopen-teste: N/N ok".
 * Argumento opcional: caminho de um .node/dylib extra para carregar e listar (ex.: spdlog.node). */
#import <Foundation/Foundation.h>
#include <dlfcn.h>
#include <stdio.h>
#include <pthread.h>
#include <mach-o/dyld.h>

__attribute__((visibility("default"))) int host_value(void) { return 41; }

@interface DLPluginAPI : NSObject   /* só a interface: a classe de verdade vem do plugin */
- (NSString *)hello;
- (int)twice:(int)x;
@end

static void *tls_thread(void *f) { return (void *)(long)((int (*)(void))f)(); }

static int ok, total;
#define CHECK(c, ...) do { total++; if (c) ok++; else { fprintf(stderr, "FALHOU: "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } } while (0)

int main(int argc, char **argv) {
    uint32_t n0 = _dyld_image_count();
    void *h = dlopen("@executable_path/Plugin.bundle", RTLD_NOW);
    CHECK(h != NULL, "dlopen do plugin: %s", dlerror());
    if (!h) { fprintf(stderr, "dlopen-teste: %d/%d ok\n", ok, total); return 1; }
    int (*add)(int, int) = (int (*)(int, int))dlsym(h, "plugin_add");
    CHECK(add && add(2, 3) == 5, "dlsym plugin_add");
    int (*dep)(int, int) = (int (*)(int, int))dlsym(h, "plugin_dep");
    CHECK(dep && dep(6, 7) == 42, "dependência @rpath (dep_mul = %d)", dep ? dep(6, 7) : -1);
    int (*hostf)(void) = (int (*)(void))dlsym(h, "plugin_host");
    CHECK(hostf && hostf() == 42, "símbolo do executável por busca plana");
    int *ctor = (int *)dlsym(h, "plugin_ctor_ran");
    CHECK(ctor && *ctor == 1, "construtor do plugin depois do da dependência (%d)", ctor ? *ctor : -1);
    int *load = (int *)dlsym(h, "plugin_load_ran");
    CHECK(load && *load == 1, "+load da classe ObjC");
    int (*tls)(void) = (int (*)(void))dlsym(h, "plugin_tls");
    CHECK(tls && tls() == 41 && tls() == 42, "thread_local do plugin");
    if (tls) {                              /* outra thread tem a sua cópia (começa em 40 de novo) */
        pthread_t th;
        void *r = 0;
        pthread_create(&th, NULL, tls_thread, (void *)tls);
        pthread_join(th, &r);
        CHECK((long)r == 41, "thread_local do plugin em outra thread (%ld)", (long)r);
    }
    Class c = NSClassFromString(@"DLPlugin");
    CHECK(c != Nil, "classe DLPlugin registrada");
    if (c) {
        DLPluginAPI *o = [[c alloc] init];
        NSString *s = [o hello];
        CHECK([s isEqualToString:@"olá do plugin"], "-hello devolveu %s", s ? [s UTF8String] : "(nil)");
        CHECK([o twice:21] == 42, "-twice:");
    }
    CHECK(dlsym(RTLD_DEFAULT, "plugin_add") == (void *)add, "dlsym(RTLD_DEFAULT)");
    CHECK(dlsym(RTLD_DEFAULT, "host_value") == (void *)host_value, "dlsym(RTLD_DEFAULT) acha o executável");
    CHECK(dlsym(h, "nao_existe_xyz") == NULL && dlerror() != NULL, "símbolo ausente devolve NULL + dlerror");
    CHECK(dlerror() == NULL, "dlerror limpa depois de lida");
    CHECK(dlopen("@executable_path/Plugin.bundle", RTLD_NOLOAD) == h, "RTLD_NOLOAD devolve o mesmo handle");
    CHECK(dlopen("/nao/existe.dylib", RTLD_NOW) == NULL && dlerror() != NULL, "arquivo inexistente");
    void *ls = dlopen("/usr/lib/libSystem.B.dylib", RTLD_LAZY);
    CHECK(ls && dlsym(ls, "malloc") != NULL, "libSystem por dlopen");
    uint32_t n1 = _dyld_image_count();
    CHECK(n1 == n0 + 2, "_dyld_image_count %u -> %u", n0, n1);
    const char *nm = _dyld_get_image_name(n0), *nd = _dyld_get_image_name(n0 + 1);   /* o plugin, depois a dependência */
    CHECK(nm && strstr(nm, "Plugin.bundle") && nd && strstr(nd, "libdep.dylib"), "_dyld_get_image_name: %s, %s", nm ? nm : "(nil)", nd ? nd : "(nil)");
    CHECK(_dyld_get_image_header(n0) != NULL && *(const uint32_t *)_dyld_get_image_header(n0) == 0xFEEDFACF, "_dyld_get_image_header");
    CHECK(_dyld_get_image_header_containing_address((const void *)add) == _dyld_get_image_header(n0), "_dyld_get_image_header_containing_address");
    CHECK(dlclose(h) == 0, "dlclose");
    for (int i = 1; i < argc; i++) {
        void *x = dlopen(argv[i], RTLD_NOW);
        fprintf(stderr, "extra: dlopen(%s) = %p %s\n", argv[i], x, x ? "" : dlerror());
        CHECK(x != NULL, "dlopen extra %s", argv[i]);
        if (x) {
            void *reg = dlsym(x, "napi_register_module_v1");
            fprintf(stderr, "extra: napi_register_module_v1 = %p\n", reg);
        }
    }
    fprintf(stderr, "dlopen-teste: %d/%d ok\n", ok, total);
    return ok == total ? 0 : 1;
}
