/* EnvironmentValues e Environment(\.key).
 * clang -std=c11 -Wall -Wextra -Werror -DSWIFTUI_ENV_HOST \
 *   userland/libsystem/swiftui_env.c userland/tests-swiftui/environment_test.c -o /tmp/sui_env
 */
#include <stdio.h>
#include <stdint.h>

typedef struct { const void *isa; int colorScheme; int isEnabled; int layoutDirection; void *font; } sui_values;
typedef struct { uint32_t kind; const char *name; } sui_desc;

extern sui_desc $s7SwiftUI17EnvironmentValuesVMn;
void *$s7SwiftUI17EnvironmentValuesVACycfC(void);
int $s7SwiftUI17EnvironmentValuesV11colorSchemeAA05ColorF0Ovg(void);
void $s7SwiftUI17EnvironmentValuesV11colorSchemeAA05ColorF0Ovs(int v);
void *$s7SwiftUI17EnvironmentValuesV11colorSchemeAA05ColorF0OvpMV(void);
int $s7SwiftUI17EnvironmentValuesV9isEnabledSbvg(void);
void $s7SwiftUI17EnvironmentValuesV9isEnabledSbvs(int v);
void *$s7SwiftUI11EnvironmentVyACyxGs7KeyPathCyAA0C6ValuesVxGcfC(void *keyPath);
uint64_t $s7SwiftUI11EnvironmentV12wrappedValuexvg(void);
void $s7SwiftUI4ViewPAAE11environmentyQrs15WritableKeyPathCyAA17EnvironmentValuesVqd__G_qd__tlF(void *key, uint64_t value);
void *$s7SwiftUI17EnvironmentValuesV4fontAA4FontVSgvg(void);
void $s7SwiftUI17EnvironmentValuesV4fontAA4FontVSgvs(void *v);

#define arm(p) __asm__ volatile("mov %0, %%r13" :: "r"(p) : "r13", "memory")

int main(void) {
    int fails = 0;
    sui_values *v = $s7SwiftUI17EnvironmentValuesVACycfC();
    void *e;
    int *slot;
    char font = 'f';
    if (!v || v->isa != &$s7SwiftUI17EnvironmentValuesVMn || v->isEnabled != 1) {
        fprintf(stderr, "FAIL init\n"); return 1;
    }
    arm(v);
    $s7SwiftUI17EnvironmentValuesV11colorSchemeAA05ColorF0Ovs(1);
    if ($s7SwiftUI17EnvironmentValuesV11colorSchemeAA05ColorF0Ovg() != 1) { fprintf(stderr, "FAIL scheme\n"); fails++; }
    slot = $s7SwiftUI17EnvironmentValuesV11colorSchemeAA05ColorF0OvpMV();
    if (slot != &v->colorScheme) { fprintf(stderr, "FAIL modify\n"); fails++; }
    *slot = 2;
    if ($s7SwiftUI17EnvironmentValuesV11colorSchemeAA05ColorF0Ovg() != 2) { fprintf(stderr, "FAIL modify grava\n"); fails++; }
    $s7SwiftUI17EnvironmentValuesV9isEnabledSbvs(0);
    if ($s7SwiftUI17EnvironmentValuesV9isEnabledSbvg() != 0) { fprintf(stderr, "FAIL enabled\n"); fails++; }
    $s7SwiftUI4ViewPAAE11environmentyQrs15WritableKeyPathCyAA17EnvironmentValuesVqd__G_qd__tlF((void *)0x42, 9);
    e = $s7SwiftUI11EnvironmentVyACyxGs7KeyPathCyAA0C6ValuesVxGcfC((void *)0x42);
    arm(e);
    if ($s7SwiftUI11EnvironmentV12wrappedValuexvg() != 9) { fprintf(stderr, "FAIL key arbitraria\n"); fails++; }
    arm(v);
    $s7SwiftUI17EnvironmentValuesV11colorSchemeAA05ColorF0Ovs(2);
    e = $s7SwiftUI11EnvironmentVyACyxGs7KeyPathCyAA0C6ValuesVxGcfC((void *)$s7SwiftUI17EnvironmentValuesV11colorSchemeAA05ColorF0Ovg);
    arm(e);
    if ($s7SwiftUI11EnvironmentV12wrappedValuexvg() != 2) { fprintf(stderr, "FAIL scheme via Environment\n"); fails++; }
    $s7SwiftUI4ViewPAAE11environmentyQrs15WritableKeyPathCyAA17EnvironmentValuesVqd__G_qd__tlF((void *)$s7SwiftUI17EnvironmentValuesV4fontAA4FontVSgvg, (uint64_t)(uintptr_t)&font);
    if ($s7SwiftUI17EnvironmentValuesV4fontAA4FontVSgvg() != &font) { fprintf(stderr, "FAIL font\n"); fails++; }
    if (fails) { fprintf(stderr, "RESULT FAIL %d\n", fails); return 1; }
    printf("RESULT PASS\n");
    return 0;
}
