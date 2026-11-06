// lado C/C++ do teste das deps: ORB (paths sobre o CGPath do libSystem do mOS)
#include <OpenRenderBox/ORBPath.h>
#include <CoreGraphics/CoreGraphics.h>
#include <stdio.h>

extern "C" CFIndex mos_real_CFGetRetainCount(CFTypeRef) __asm__("_CFGetRetainCount");   /* não está no CF do swift-corelibs */

extern "C" int deps_c_selftest(void) {
    int fails = 0;
#define CHECK(c, msg) do { if (!(c)) { printf("  C FALHOU: %s\n", msg); fails++; } else printf("  C ok: %s\n", msg); } while (0)
    /* constantes CFSTR do clang ({isa, flags, ptr, len}) passando pelo CF do mOS (cf_mos.cpp) */
    CFStringRef k = CFSTR("NSFontSizeAttribute");
    CFStringRef dyn = CFStringCreateWithCString(nullptr, "NSFontSizeAttribute", kCFStringEncodingUTF8);
    CHECK(CFStringGetLength(k) == 19, "CFStringGetLength(CFSTR)");
    CHECK(CFEqual(k, dyn) && CFHash(k) == CFHash(dyn), "CFEqual/CFHash entre CFSTR e string criada");
    CHECK(CFGetTypeID(k) == CFStringGetTypeID(), "CFGetTypeID(CFSTR) == CFStringGetTypeID()");
    /* CFRetain/CFRelease "de verdade" (código C sem o prelude, como o CoreText): contagem do Swift */
#undef CFRetain
#undef CFRelease
#undef CFGetRetainCount
    CFRetain(dyn);
    CHECK(mos_real_CFGetRetainCount(dyn) == 2, "CFRetain real sobe a contagem do objeto Swift");
    CFRelease(dyn);
    CHECK(mos_real_CFGetRetainCount(dyn) == 1, "CFRelease real desce a contagem");
    CFRelease(dyn);
    CGRect r = CGRectMake(0, 0, 100, 40);
    ORBPath p = ORBPathMakeRoundedRect(r, 10, 10, ORBPathRoundedCornerStyleCircular, nullptr);
    CHECK(!ORBPathIsEmpty(p), "ORBPathMakeRoundedRect não vazio");
    /* ORBPathContainsPoints e ORBPathCopyCGPath ainda são TODO no OpenRenderBox (devolvem false/nullptr) */
    printf("  C (TODO upstream): ORBPathContainsPoint=%d ORBPathCopyCGPath=%p\n", (int)ORBPathContainsPoint(p, CGPointMake(50, 20), false), (void *)ORBPathCopyCGPath(p));
    CGMutablePathRef cg = CGPathCreateMutable();
    CGPathAddEllipseInRect(cg, nullptr, CGRectMake(0, 0, 20, 20));
    ORBPath q = ORBPathMakeWithCGPath(cg);
    CHECK(!ORBPathIsEmpty(q), "ORBPathMakeWithCGPath (elipse do CGPath do mOS)");
    CHECK(ORBPathEqualToPath(q, q), "ORBPathEqualToPath");
    ORBPathRelease(p);
    ORBPathRelease(q);
    CGPathRelease(cg);
    return fails;
}
