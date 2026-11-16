/* mOS — teste do JavaScriptCore.framework na VM (API ObjC e C): imprime "jsc: N/M ok" no stderr.
 *   tools/build-jsctest.sh <zig> <saída>  → JSCTest (linha de comando), roda dentro do mOS */
#import <Foundation/Foundation.h>
#include <stdio.h>
#include <string.h>

@interface NSNull : NSObject
+ (NSNull *)null;
@end
@class JSContext, JSValue, JSVirtualMachine, JSManagedValue;
@protocol JSExport
@end
@interface JSVirtualMachine : NSObject
@end
@interface JSContext : NSObject
- (instancetype)initWithVirtualMachine:(JSVirtualMachine *)vm;
- (JSValue *)evaluateScript:(NSString *)s;
- (JSValue *)objectForKeyedSubscript:(id)k;
- (void)setObject:(id)o forKeyedSubscript:(id)k;
@property (strong) JSValue *exception;
@property (copy) void (^exceptionHandler)(JSContext *, JSValue *);
@property (readonly) JSValue *globalObject;
+ (JSContext *)currentContext;
+ (NSArray *)currentArguments;
+ (JSValue *)currentThis;
@end
@interface JSValue : NSObject
+ (JSValue *)valueWithObject:(id)o inContext:(JSContext *)c;
+ (JSValue *)valueWithNewObjectInContext:(JSContext *)c;
- (id)toObject;
- (NSString *)toString;
- (double)toDouble;
- (int32_t)toInt32;
- (BOOL)toBool;
- (NSArray *)toArray;
- (NSDictionary *)toDictionary;
- (BOOL)isUndefined;
- (BOOL)isNull;
- (BOOL)isObject;
- (JSValue *)callWithArguments:(NSArray *)a;
- (JSValue *)invokeMethod:(NSString *)m withArguments:(NSArray *)a;
- (JSValue *)objectForKeyedSubscript:(id)k;
- (void)setObject:(id)o forKeyedSubscript:(id)k;
@end

/* objeto exportado: propriedades (objeto e número) e métodos com e sem renomear */
@protocol ContaExports <JSExport>
@property (copy) NSString *dono;
@property double saldo;
@property (readonly) int operacoes;
- (double)depositar:(double)v;
- (NSString *)resumoCom:(NSString *)prefixo e:(int)n;
@end
@interface Conta : NSObject <ContaExports> {
    NSString *_dono;
    double _saldo;
    int _ops;
}
@end
@implementation Conta
- (NSString *)dono { return _dono; }
- (void)setDono:(NSString *)d { [d retain]; [_dono release]; _dono = d; }
- (double)saldo { return _saldo; }
- (void)setSaldo:(double)s { _saldo = s; }
- (int)operacoes { return _ops; }
- (double)depositar:(double)v { _saldo += v; _ops++; return _saldo; }
- (NSString *)resumoCom:(NSString *)prefixo e:(int)n { return [NSString stringWithFormat:@"%@:%@:%d:%.1f", prefixo, _dono, n, _saldo]; }
@end

static int g_ok, g_total;
static void check(BOOL c, const char *name, NSString *det) {
    g_total++;
    if (c) { g_ok++; fprintf(stderr, "jsc: ok %s\n", name); }
    else fprintf(stderr, "jsc: FALHA %s (%s)\n", name, det ? [det UTF8String] : "");
}

int main(void) {
    @autoreleasepool {
        JSContext *c = [[JSContext alloc] init];
        JSValue *v = [c evaluateScript:@"1 + 2 * 20"];
        check([v toInt32] == 41, "evaluateScript", [v toString]);
        c[@"nome"] = @"mOS";
        c[@"lista"] = @[@1, @"dois", @{@"tres": @3}, [NSNull null], @YES];
        v = [c evaluateScript:@"nome.length + ':' + lista.length + ':' + lista[2].tres + ':' + (lista[3] === null) + ':' + (lista[4] === true)"];
        check([[v toString] isEqualToString:@"3:5:3:true:true"], "conversões ObjC → JS", [v toString]);
        v = [c evaluateScript:@"({a: [1, 2], b: 'x', c: {d: false}, e: null})"];
        NSDictionary *d = [v toDictionary];
        check([[d objectForKey:@"b"] isEqualToString:@"x"] && [[d objectForKey:@"a"] count] == 2 &&
              [[[d objectForKey:@"c"] objectForKey:@"d"] boolValue] == NO && [d objectForKey:@"e"] == [NSNull null], "conversões JS → ObjC", [d description]);
        /* blocos como funções */
        c[@"somar"] = ^double(double a, double b) { return a + b; };
        c[@"saudar"] = ^NSString *(NSString *quem) { return [NSString stringWithFormat:@"oi %@ (%lu args)", quem, (unsigned long)[[JSContext currentArguments] count]]; };
        v = [c evaluateScript:@"somar(1.5, 2.25) + '|' + saudar('Thiago', 9)"];
        check([[v toString] isEqualToString:@"3.75|oi Thiago (2 args)"], "blocos chamados pelo JS", [v toString]);
        /* chamar função JS do ObjC */
        [c evaluateScript:@"function dobro(x) { return x * 2 } var obj = { n: 5, mais: function(k) { return this.n + k } };"];
        check([[c[@"dobro"] callWithArguments:@[@21]] toInt32] == 42, "callWithArguments", nil);
        check([[c[@"obj"] invokeMethod:@"mais" withArguments:@[@10]] toInt32] == 15, "invokeMethod", nil);
        /* exceções */
        __block NSString *pegou = nil;
        c.exceptionHandler = ^(JSContext *ctx, JSValue *e) { pegou = [[e toString] retain]; };
        [c evaluateScript:@"null.x"];
        check(pegou && [pegou hasPrefix:@"TypeError"], "exceptionHandler", pegou);
        /* JSExport */
        Conta *conta = [[Conta alloc] init];
        conta.dono = @"Ana";
        c[@"conta"] = conta;
        v = [c evaluateScript:@"conta.depositar(10); conta.depositar(2.5); conta.saldo = conta.saldo * 2; conta.operacoes + '|' + conta.saldo + '|' + conta.dono + '|' + conta.resumoComE('R', 7)"];
        check([[v toString] isEqualToString:@"2|25|Ana|R:Ana:7:25.0"], "JSExport: métodos e propriedades", [v toString]);
        [c evaluateScript:@"conta.dono = 'Bia'; conta.operacoes = 99"];
        check([conta.dono isEqualToString:@"Bia"] && conta.operacoes == 2, "JSExport: escrita e somente leitura", conta.dono);
        check([[c[@"conta"] toObject] isEqual:conta], "o objeto volta igual em toObject", nil);
        /* Promise e microtarefas */
        [c evaluateScript:@"var r = 0; Promise.resolve(7).then(function(x) { r = x })"];
        check([c[@"r"] toInt32] == 7, "microtarefas depois do script", nil);
        [conta release];
        [c release];
    }
    fprintf(stderr, "jsc: %d/%d ok\n", g_ok, g_total);
    return g_ok == g_total ? 0 : 1;
}
