/* State e Binding que o swift_addon do Claude e o serviço de computer use do
 * ChatGPT importam. O valor cabe num uint64. self no swiftcc entra em r13.
 */
#ifdef SWIFTUI_STATE_HOST
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#else
#include "libc.h"
#endif

typedef struct { uint32_t kind; const char *name; } sui_desc;
typedef uint64_t (*sui_get_fn)(void);
typedef void (*sui_set_fn)(uint64_t);

typedef struct {
    const void *isa;
    uint64_t value;
} sui_state;

typedef struct {
    const void *isa;
    uint64_t *slot;
    sui_get_fn get;
    sui_set_fn set;
    uint64_t constant;   /* Binding.constant: o valor fica aqui e a escrita é ignorada */
    int readonly;
} sui_binding;

sui_desc $s7SwiftUI5StateVMn = { 2, "State" };
sui_desc $s7SwiftUI7BindingVMn = { 3, "Binding" };

void *$s7SwiftUI5StateVMa(void) { return &$s7SwiftUI5StateVMn; }
void *$s7SwiftUI7BindingVMa(void) { return &$s7SwiftUI7BindingVMn; }

static void *sui_r13(void) {
    void *p;
    __asm__ volatile("mov %%r13, %0" : "=r"(p));
    return p;
}

void *$s7SwiftUI5StateV12wrappedValueACyxGx_tcfC(uint64_t value, void *metadata) {
    sui_state *s;
    (void)metadata;
    s = (sui_state *)calloc(1, sizeof *s);
    if (!s) return 0;
    s->isa = &$s7SwiftUI5StateVMn;
    s->value = value;
    return s;
}

uint64_t $s7SwiftUI5StateV12wrappedValuexvg(void) {
    sui_state *s = (sui_state *)sui_r13();
    return s ? s->value : 0;
}

void $s7SwiftUI5StateV12wrappedValuexvs(uint64_t value) {
    sui_state *s = (sui_state *)sui_r13();
    if (s) s->value = value;
}

static sui_binding *sui_bind_slot(uint64_t *slot) {
    sui_binding *b = (sui_binding *)calloc(1, sizeof *b);
    if (!b) return 0;
    b->isa = &$s7SwiftUI7BindingVMn;
    b->slot = slot;
    return b;
}

void *$s7SwiftUI5StateV14projectedValueAA7BindingVyxGvg(void) {
    sui_state *s = (sui_state *)sui_r13();
    if (!s) return 0;
    return sui_bind_slot(&s->value);
}

void *$s7SwiftUI7BindingV3get3setACyxGxyc_yxctcfC(sui_get_fn get, sui_set_fn set) {
    sui_binding *b = (sui_binding *)calloc(1, sizeof *b);
    if (!b) return 0;
    b->isa = &$s7SwiftUI7BindingVMn;
    b->get = get;
    b->set = set;
    return b;
}

uint64_t $s7SwiftUI7BindingV12wrappedValuexvg(void) {
    sui_binding *b = (sui_binding *)sui_r13();
    if (!b) return 0;
    if (b->slot) return *b->slot;
    if (b->get) return b->get();
    return 0;
}

void $s7SwiftUI7BindingV12wrappedValuexvs(uint64_t value) {
    sui_binding *b = (sui_binding *)sui_r13();
    if (!b || b->readonly) return;
    if (b->slot) { *b->slot = value; return; }
    if (b->set) b->set(value);
}

/* Binding.constant(_:): o valor genérico chega por referência; cabe num uint64 como no resto do modelo. */
void *$s7SwiftUI7BindingV8constantyACyxGxFZ(const void *value, void *metadata) {
    sui_binding *b = (sui_binding *)calloc(1, sizeof *b);
    (void)metadata;
    if (!b) return 0;
    b->isa = &$s7SwiftUI7BindingVMn;
    if (value) memcpy(&b->constant, value, sizeof b->constant);
    b->slot = &b->constant;
    b->readonly = 1;
    return b;
}

void *$s7SwiftUI7BindingV14projectedValueACyxGvg(void) {
    return sui_r13();
}

void *$s7SwiftUI10FocusStateV14projectedValueAC7BindingVyx_Gvg(void) {
    sui_state *s = (sui_state *)sui_r13();
    if (!s) return 0;
    if (!s->isa) {
        s->isa = &$s7SwiftUI5StateVMn;
        s->value = 0;
    }
    return sui_bind_slot(&s->value);
}
