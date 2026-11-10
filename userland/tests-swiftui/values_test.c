/* Alignment, UnitPoint, Edge.Set, EdgeInsets, casos de enum e OptionSets.
 * clang -std=c11 -Wall -Wextra -Werror -DSWIFTUI_VALUES_HOST \
 *   userland/libsystem/swiftui_values.c userland/tests-swiftui/values_test.c -o /tmp/sui_values
 */
#define SUI_HOST 1
#include <stdio.h>
#include "../libsystem/swiftui_model.h"

typedef struct { int64_t h, v; } al2;
typedef struct { double x, y; } pt;
typedef struct { uint64_t w0, w1; } pair;
typedef struct { const sui_desc *isa; char key[8]; uint64_t modifiers; } shortcut;

al2 $s7SwiftUI9AlignmentV10topLeadingACvgZ(void);
al2 $s7SwiftUI9AlignmentV6centerACvgZ(void);
al2 $s7SwiftUI9AlignmentV14bottomTrailingACvgZ(void);
int64_t $s7SwiftUI19HorizontalAlignmentV7leadingACvgZ(void);
int64_t $s7SwiftUI19HorizontalAlignmentV6centerACvgZ(void);
int $s7SwiftUI19HorizontalAlignmentV2eeoiySbAC_ACtFZ(int64_t a, int64_t b);
int64_t $s7SwiftUI17VerticalAlignmentV6bottomACvgZ(void);
int64_t $s7SwiftUI17VerticalAlignmentV17firstTextBaselineACvgZ(void);
pt $s7SwiftUI9UnitPointV6centerACvgZ(void);
pt $s7SwiftUI9UnitPointV14bottomTrailingACvgZ(void);
int8_t $s7SwiftUI4EdgeO3SetV10horizontalAEvgZ(void);
int8_t $s7SwiftUI4EdgeO3SetV3allAEvgZ(void);
int8_t $s7SwiftUI4EdgeO3SetVyAeCcfC(int edge);
int8_t $s7SwiftUI4AxisO3SetV8verticalAEvgZ(void);
extern const int32_t $s7SwiftUI11ColorSchemeO4darkyA2CmFWC;
extern const int32_t $s7SwiftUI4FontV9TextStyleO4bodyyA2EmFWC;
extern const int32_t $s7SwiftUI9BlendModeO11plusLighteryA2CmFWC;
extern sui_desc $s7SwiftUI11ColorSchemeOMn;
void *$s7SwiftUI11ColorSchemeOMa(void);
int $s7SwiftUI11ColorSchemeO2eeoiySbAC_ACtFZ(int a, int b);
double $s7SwiftUI5AngleV14animatableDataSdvg(void);
void $s7SwiftUI5AngleV14animatableDataSdvs(double v);
uint64_t $s7SwiftUI14EventModifiersV7commandACvgZ(void);
pair $s7SwiftUI13KeyEquivalentVyACSJcfC(uint64_t w0, uint64_t w1);
void *$s7SwiftUI16KeyboardShortcutV13defaultActionACvgZ(void);

/* Swift devolve EdgeInsets em xmm0..xmm3: lê os quatro registradores. */
static void insets_all(double v, double out[4]) {
    double a = v, b, c, d;
    __asm__ volatile(
        "movsd %4, %%xmm0\n\t"
        "call \"$s7SwiftUI10EdgeInsetsV4_allAC12CoreGraphics7CGFloatV_tcfC\"\n\t"
        "movsd %%xmm0, %0\n\t"
        "movsd %%xmm1, %1\n\t"
        "movsd %%xmm2, %2\n\t"
        "movsd %%xmm3, %3\n\t"
        : "=m"(a), "=m"(b), "=m"(c), "=m"(d)
        : "m"(v)
        : "xmm0", "xmm1", "xmm2", "xmm3", "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11", "memory");
    out[0] = a; out[1] = b; out[2] = c; out[3] = d;
}

#define arm(p) __asm__ volatile("mov %0, %%r13" :: "r"(p) : "r13", "memory")

int main(void) {
    int fails = 0;
    al2 a;
    pt p;
    pair k;
    double in[4], ang = 1.0;
    shortcut *sc;
    a = $s7SwiftUI9AlignmentV10topLeadingACvgZ();
    if (a.h != -1 || a.v != -1) { fprintf(stderr, "FAIL topLeading\n"); fails++; }
    a = $s7SwiftUI9AlignmentV6centerACvgZ();
    if (a.h != 0 || a.v != 0) { fprintf(stderr, "FAIL center\n"); fails++; }
    a = $s7SwiftUI9AlignmentV14bottomTrailingACvgZ();
    if (a.h != 1 || a.v != 1) { fprintf(stderr, "FAIL bottomTrailing\n"); fails++; }
    if (!$s7SwiftUI19HorizontalAlignmentV2eeoiySbAC_ACtFZ($s7SwiftUI19HorizontalAlignmentV7leadingACvgZ(), -1) ||
        $s7SwiftUI19HorizontalAlignmentV2eeoiySbAC_ACtFZ($s7SwiftUI19HorizontalAlignmentV6centerACvgZ(), -1)) {
        fprintf(stderr, "FAIL HorizontalAlignment ==\n"); fails++;
    }
    if ($s7SwiftUI17VerticalAlignmentV6bottomACvgZ() != 1 || $s7SwiftUI17VerticalAlignmentV17firstTextBaselineACvgZ() >= 0) {
        fprintf(stderr, "FAIL VerticalAlignment\n"); fails++;
    }
    p = $s7SwiftUI9UnitPointV6centerACvgZ();
    if (p.x != 0.5 || p.y != 0.5) { fprintf(stderr, "FAIL UnitPoint.center\n"); fails++; }
    p = $s7SwiftUI9UnitPointV14bottomTrailingACvgZ();
    if (p.x != 1 || p.y != 1) { fprintf(stderr, "FAIL UnitPoint.bottomTrailing\n"); fails++; }
    if ($s7SwiftUI4EdgeO3SetV10horizontalAEvgZ() != 10 || $s7SwiftUI4EdgeO3SetV3allAEvgZ() != 15 ||
        $s7SwiftUI4EdgeO3SetVyAeCcfC(2) != 4 || $s7SwiftUI4EdgeO3SetVyAeCcfC(9) != 0) {
        fprintf(stderr, "FAIL Edge.Set\n"); fails++;
    }
    if ($s7SwiftUI4AxisO3SetV8verticalAEvgZ() != 2) { fprintf(stderr, "FAIL Axis.Set\n"); fails++; }
    insets_all(12.5, in);
    if (in[0] != 12.5 || in[1] != 12.5 || in[2] != 12.5 || in[3] != 12.5) { fprintf(stderr, "FAIL EdgeInsets(_all:)\n"); fails++; }
    if ($s7SwiftUI11ColorSchemeO4darkyA2CmFWC != 1 || $s7SwiftUI4FontV9TextStyleO4bodyyA2EmFWC != 6 ||
        $s7SwiftUI9BlendModeO11plusLighteryA2CmFWC != 21) {
        fprintf(stderr, "FAIL casos de enum\n"); fails++;
    }
    if ($s7SwiftUI11ColorSchemeOMa() != &$s7SwiftUI11ColorSchemeOMn ||
        !$s7SwiftUI11ColorSchemeO2eeoiySbAC_ACtFZ(0x101, 1) || $s7SwiftUI11ColorSchemeO2eeoiySbAC_ACtFZ(0, 1)) {
        fprintf(stderr, "FAIL ColorScheme\n"); fails++;
    }
    arm(&ang);
    $s7SwiftUI5AngleV14animatableDataSdvs(3.0);
    arm(&ang);
    if ($s7SwiftUI5AngleV14animatableDataSdvg() != 3.0 || ang != 3.0) { fprintf(stderr, "FAIL Angle\n"); fails++; }
    if ($s7SwiftUI14EventModifiersV7commandACvgZ() != 16) { fprintf(stderr, "FAIL command\n"); fails++; }
    k = $s7SwiftUI13KeyEquivalentVyACSJcfC(0x6b, 0xE100000000000000ULL);
    if (k.w0 != 0x6b || k.w1 != 0xE100000000000000ULL) { fprintf(stderr, "FAIL KeyEquivalent\n"); fails++; }
    sc = $s7SwiftUI16KeyboardShortcutV13defaultActionACvgZ();
    if (!sc || sc->key[0] != '\r' || sc->modifiers != 0) { fprintf(stderr, "FAIL defaultAction\n"); fails++; }
    if (fails) { fprintf(stderr, "RESULT FAIL %d\n", fails); return 1; }
    printf("RESULT PASS\n");
    return 0;
}
