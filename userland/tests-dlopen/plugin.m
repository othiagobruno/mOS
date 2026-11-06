/* mOS — teste do dlopen: MH_BUNDLE (como os .node do Node.js) com construtor, classe ObjC com +load,
 * thread_local, dependência @rpath/libdep.dylib e um símbolo do programa que o carregou (host_value,
 * achado por busca plana: -undefined dynamic_lookup) */
#import <Foundation/Foundation.h>

extern int dep_mul(int a, int b);
extern int dep_ready;
extern int host_value(void);            /* definido no executável */

int plugin_ctor_ran;
int plugin_load_ran;
static __thread int tls_counter = 40;

__attribute__((constructor)) static void plugin_init(void) { plugin_ctor_ran = dep_ready == 7 ? 1 : 2; }

int plugin_add(int a, int b) { return a + b; }
int plugin_dep(int a, int b) { return dep_mul(a, b); }
int plugin_host(void) { return host_value() + 1; }
int plugin_tls(void) { return ++tls_counter; }

@interface DLPlugin : NSObject
- (NSString *)hello;
- (int)twice:(int)x;
@end

@implementation DLPlugin
+ (void)load { plugin_load_ran = 1; }
- (NSString *)hello { return @"olá do plugin"; }
- (int)twice:(int)x { return x * 2; }
@end
