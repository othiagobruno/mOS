/* State/Binding: valor guardado, projeção e init(get:set:).
 * clang -std=c11 -Wall -Wextra -Werror -DSWIFTUI_STATE_HOST \
 *   userland/libsystem/swiftui_state.c userland/tests-swiftui/state_binding_test.c -o /tmp/sui_state
 */
#include <stdio.h>
#include <stdint.h>

typedef struct { const void *isa; uint64_t value; } sui_state;
typedef struct { uint32_t kind; const char *name; } sui_desc;

extern sui_desc $s7SwiftUI5StateVMn;
extern sui_desc $s7SwiftUI7BindingVMn;
void *$s7SwiftUI5StateVMa(void);
void *$s7SwiftUI7BindingVMa(void);
void *$s7SwiftUI5StateV12wrappedValueACyxGx_tcfC(uint64_t value, void *metadata);
uint64_t $s7SwiftUI5StateV12wrappedValuexvg(void);
void $s7SwiftUI5StateV12wrappedValuexvs(uint64_t value);
void *$s7SwiftUI5StateV14projectedValueAA7BindingVyxGvg(void);
void *$s7SwiftUI7BindingV3get3setACyxGxyc_yxctcfC(uint64_t (*get)(void), void (*set)(uint64_t));
uint64_t $s7SwiftUI7BindingV12wrappedValuexvg(void);
void $s7SwiftUI7BindingV12wrappedValuexvs(uint64_t value);
void *$s7SwiftUI7BindingV14projectedValueACyxGvg(void);
void *$s7SwiftUI10FocusStateV14projectedValueAC7BindingVyx_Gvg(void);

#define arm(p) __asm__ volatile("mov %0, %%r13" :: "r"(p) : "r13", "memory")

static uint64_t box = 7;
static uint64_t host_get(void) { return box; }
static void host_set(uint64_t v) { box = v; }

int main(void) {
    int fails = 0;
    sui_state *s;
    void *b, *p;
    sui_state focus;
    if ($s7SwiftUI5StateVMa() != &$s7SwiftUI5StateVMn || $s7SwiftUI5StateVMn.kind != 2) {
        fprintf(stderr, "FAIL descritor State\n"); fails++;
    }
    s = $s7SwiftUI5StateV12wrappedValueACyxGx_tcfC(41, 0);
    arm(s);
    if ($s7SwiftUI5StateV12wrappedValuexvg() != 41) { fprintf(stderr, "FAIL get\n"); fails++; }
    $s7SwiftUI5StateV12wrappedValuexvs(42);
    if (s->value != 42) { fprintf(stderr, "FAIL set\n"); fails++; }
    b = $s7SwiftUI5StateV14projectedValueAA7BindingVyxGvg();
    arm(b);
    if ($s7SwiftUI7BindingV12wrappedValuexvg() != 42) { fprintf(stderr, "FAIL binding do state\n"); fails++; }
    $s7SwiftUI7BindingV12wrappedValuexvs(9);
    if (s->value != 9) { fprintf(stderr, "FAIL binding grava no state\n"); fails++; }
    if ($s7SwiftUI7BindingV14projectedValueACyxGvg() != b) { fprintf(stderr, "FAIL projected\n"); fails++; }
    p = $s7SwiftUI7BindingV3get3setACyxGxyc_yxctcfC(host_get, host_set);
    arm(p);
    if ($s7SwiftUI7BindingV12wrappedValuexvg() != 7) { fprintf(stderr, "FAIL get closure\n"); fails++; }
    $s7SwiftUI7BindingV12wrappedValuexvs(3);
    if (box != 3) { fprintf(stderr, "FAIL set closure\n"); fails++; }
    focus.isa = 0; focus.value = 99;
    arm(&focus);
    b = $s7SwiftUI10FocusStateV14projectedValueAC7BindingVyx_Gvg();
    arm(b);
    if ($s7SwiftUI7BindingV12wrappedValuexvg() != 0) { fprintf(stderr, "FAIL focus\n"); fails++; }
    $s7SwiftUI7BindingV12wrappedValuexvs(1);
    if (focus.value != 1) { fprintf(stderr, "FAIL focus grava\n"); fails++; }
    if (fails) { fprintf(stderr, "RESULT FAIL %d\n", fails); return 1; }
    printf("RESULT PASS\n");
    return 0;
}
