/* mOS — os casos ObjC e CF dos testes do Darling (tests/src: objc_category, objc_exception, objc_super, objc_stret,
 * objc_fpret, objc_load, cfbridge, cfbridge_strequals, cfurl_fm_basic), reescritos com o comportamento do macOS.
 * Compilar sem ARC (usa retain/release e retainCount). O mesmo binário roda no Darling, no macOS e no mOS. */
#import <Foundation/Foundation.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>

/* o que o SDK mínimo do mOS (userland/sdk/include) não traz: as declarações com os tipos da Apple */
#ifndef NSUIntegerMax
#define NSUIntegerMax ULONG_MAX
@interface NSObject (MOSTesteDarwin)
- (NSUInteger)retainCount;
@end
@interface NSException : NSObject
+ (NSException *)exceptionWithName:(NSString *)name reason:(NSString *)reason userInfo:(id)info;
- (NSString *)name;
@end
typedef const struct __CFString *CFStringRef;
typedef const void *CFTypeRef;
typedef const struct __CFAllocator *CFAllocatorRef;
typedef unsigned char UInt8;
typedef long CFIndex;
typedef unsigned int CFStringEncoding;
typedef unsigned char Boolean;
#define kCFStringEncodingASCII 0x0600

#define CFSTR(s) ((CFStringRef)__builtin___CFStringMakeConstantString("" s ""))
CFStringRef CFStringCreateWithBytes(CFAllocatorRef a, const UInt8 *b, CFIndex n, CFStringEncoding e, Boolean ext);
CFTypeRef CFRetain(CFTypeRef o);
void CFRelease(CFTypeRef o);
#endif
#ifndef __CORESERVICES__
/* File Manager do Carbon (CoreServices): FSRef opaco de 80 bytes */
typedef struct { UInt8 hidden[80]; } FSRef;
typedef int OSStatus;
typedef const struct __CFURL *CFURLRef;
typedef long CFURLPathStyle;
#define kCFURLPOSIXPathStyle 0
CFURLRef CFURLCreateWithFileSystemPath(CFAllocatorRef a, CFStringRef path, CFURLPathStyle style, Boolean isDir);
Boolean CFURLGetFSRef(CFURLRef url, FSRef *ref);
OSStatus FSRefMakePath(const FSRef *ref, UInt8 *path, unsigned int max);
#endif

static int fails, total;
#define EXPECT(got, want, what) do { long long g_ = (long long)(got), w_ = (long long)(want); total++; \
    if (g_ == w_) printf("  ok  %s = %lld\n", what, g_); \
    else { fails++; printf("  FALHOU  %s = %lld (esperado %lld)\n", what, g_, w_); } } while (0)

static int loads, dummies;

@interface CasoLoad : NSObject
@end
@implementation CasoLoad
+ (void)load { loads++; }
@end

@interface NSObject (CasoCategoria)
+ (int)casoDummy;
@end
@implementation NSObject (CasoCategoria)
+ (int)casoDummy { return ++dummies; }
@end

@interface CasoBase : NSObject
- (id)quem;
+ (long double)valor;
@end
@implementation CasoBase
- (id)quem { return self; }
+ (long double)valor { return 5.0L; }
@end
@interface CasoFilha : CasoBase
@end
@implementation CasoFilha
- (id)quem { return [super quem]; }
- (BOOL)respondsToSelector:(SEL)s { return [super respondsToSelector:s]; }
+ (long double)valor { return [super valor] + 1.0L; }
@end

typedef struct { float f1; int buf[50]; } grande_t;
@interface CasoStret : NSObject
+ (grande_t)soma:(float)a com:(float)b;
@end
@implementation CasoStret
+ (grande_t)soma:(float)a com:(float)b {
    grande_t s;
    memset(&s, 0, sizeof s);
    s.f1 = a + b;
    s.buf[49] = (self == [CasoStret class]) ? 77 : -1;
    return s;
}
@end

int main(void) {
    setbuf(stdout, 0);
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    printf("runtime\n");
    EXPECT(loads, 1, "+load rodou uma vez antes do main");
    EXPECT([NSObject casoDummy], 1, "método de categoria em NSObject");
    EXPECT([NSString casoDummy], 2, "a categoria vale nas subclasses");
    CasoFilha *f = [CasoFilha new];
    EXPECT([f quem] == f, 1, "[super quem] mantém o self");
    EXPECT([f respondsToSelector:@selector(quem)], 1, "respondsToSelector pelo super");
    EXPECT([f respondsToSelector:@selector(naoExiste)], 0, "respondsToSelector de seletor que não existe");
    [f release];
    grande_t g = [CasoStret soma:1.0f com:2.0f];
    EXPECT(g.f1 == 3.0f && g.buf[49] == 77, 1, "struct grande na volta (stret) com o self certo");
    long double v = [CasoFilha valor];
    EXPECT(v == 6.0L, 1, "long double na volta pelo super (fpret)");

    printf("exceções ObjC\n");
    int etapa = 0;
    NSString *nome = nil;
    @try {
        @throw [NSException exceptionWithName:@"MinhaExcecao" reason:@"teste" userInfo:nil];
    } @catch (NSException *e) {
        nome = [e name];
        etapa = 1;
    } @finally {
        etapa = etapa * 10 + 2;
    }
    EXPECT([nome isEqualToString:@"MinhaExcecao"], 1, "@catch recebe a NSException");
    EXPECT(etapa, 12, "@finally roda depois do @catch");

    printf("ponte CF e Foundation\n");
    CFStringRef cs = CFSTR("Test");
    EXPECT(strcmp([(NSString *)cs UTF8String], "Test"), 0, "CFSTR como NSString (UTF8String)");
    [(id)cs retain];
    EXPECT([(id)cs retainCount] == NSUIntegerMax, 1, "CFSTR constante: retainCount é NSUIntegerMax");
    CFStringRef cs2 = CFStringCreateWithBytes(NULL, (const UInt8 *)"Test2", 5, kCFStringEncodingASCII, false);
    CFRetain(cs2);
    EXPECT([(id)cs2 retainCount], 2, "CFStringCreate + CFRetain = 2");
    CFRelease(cs2); CFRelease(cs2);
    NSString *a = (NSString *)CFSTR("TestString"), *b = [NSString stringWithUTF8String:"TestString"];
    EXPECT([b isEqual:a], 1, "NSString isEqual CFSTR");
    EXPECT([b isEqualToString:a], 1, "NSString isEqualToString CFSTR");
    EXPECT([a isEqual:b], 1, "CFSTR isEqual NSString");
    EXPECT([a isEqualToString:b], 1, "CFSTR isEqualToString NSString");
    EXPECT([a hash] == [b hash], 1, "o mesmo hash dos dois lados");

    printf("CFURL e FSRef\n");
    CFURLRef url = CFURLCreateWithFileSystemPath(NULL, CFSTR("/tmp"), kCFURLPOSIXPathStyle, true);
    FSRef ref;
    EXPECT(CFURLGetFSRef(url, &ref) != 0, 1, "CFURLGetFSRef de /tmp");
    char path[256] = "", want[256] = "";
    EXPECT(FSRefMakePath(&ref, (UInt8 *)path, sizeof path), 0, "FSRefMakePath");
    EXPECT(realpath("/tmp", want) && strcmp(path, want) == 0, 1, "o caminho volta igual");
    CFRelease(url);

    [pool release];
    printf("darwinobjc: %d/%d ok\n", total - fails, total);
    return fails ? 1 : 0;
}
