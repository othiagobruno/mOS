/* Environment e EnvironmentValues. O modificador environment(key:value)
 * grava numa tabela por ponteiro de KeyPath. colorScheme, isEnabled,
 * layoutDirection e font também moram no struct. self em r13.
 */
#ifdef SWIFTUI_ENV_HOST
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#else
#include "libc.h"
#endif

typedef struct { uint32_t kind; const char *name; } sui_desc;

typedef struct {
    const void *isa;
    int colorScheme;
    int isEnabled;
    int layoutDirection;
    void *font;
    /* Do bloco de modificadores de texto e controle. */
    int imageScale;
    int controlSize;
    int truncationMode;
    int multilineTextAlignment;
    int textCase;
    int64_t lineLimit;
    int lineLimitNil;
    double lineSpacing;
    void *accentColor;
    void *foregroundColor;
    void *symbolRenderingMode;
    void *tintColor;
} sui_values;

typedef struct {
    const void *isa;
    void *key;
} sui_env;

typedef struct { void *key; uint64_t value; int used; } sui_slot;

sui_desc $s7SwiftUI11EnvironmentVMn = { 4, "Environment" };
sui_desc $s7SwiftUI11EnvironmentV7ContentOMn = { 5, "Environment.Content" };
sui_desc $s7SwiftUI17EnvironmentValuesVMn = { 6, "EnvironmentValues" };

/* Padrões do macOS: imagem média, controle regular, truncar no fim,
 * texto à esquerda, sem textCase (2 = nil) e sem limite de linhas. */
static sui_values sui_global = {
    .isEnabled = 1, .imageScale = 1, .controlSize = 2, .truncationMode = 1,
    .multilineTextAlignment = 0, .textCase = 2, .lineLimit = 0, .lineLimitNil = 1,
};
static sui_slot sui_map[32];

static void *sui_r13(void) {
    void *p;
    __asm__ volatile("mov %%r13, %0" : "=r"(p));
    return p;
}

static sui_values *sui_vals(void) {
    sui_values *v = (sui_values *)sui_r13();
    if (v && v->isa == &$s7SwiftUI17EnvironmentValuesVMn) return v;
    return &sui_global;
}

static void sui_put(void *key, uint64_t value) {
    int i, free_i = -1;
    for (i = 0; i < 32; i++) {
        if (sui_map[i].used && sui_map[i].key == key) { sui_map[i].value = value; return; }
        if (!sui_map[i].used && free_i < 0) free_i = i;
    }
    if (free_i >= 0) { sui_map[free_i].used = 1; sui_map[free_i].key = key; sui_map[free_i].value = value; }
}

static int sui_find(void *key, uint64_t *out) {
    int i;
    for (i = 0; i < 32; i++) if (sui_map[i].used && sui_map[i].key == key) { *out = sui_map[i].value; return 1; }
    return 0;
}

void *$s7SwiftUI17EnvironmentValuesVMa(void) { return &$s7SwiftUI17EnvironmentValuesVMn; }
void *$s7SwiftUI11EnvironmentV7ContentOMa(void) { return &$s7SwiftUI11EnvironmentV7ContentOMn; }

void *$s7SwiftUI17EnvironmentValuesVACycfC(void) {
    sui_values *v = (sui_values *)calloc(1, sizeof *v);
    if (!v) return 0;
    *v = sui_global;
    v->isa = &$s7SwiftUI17EnvironmentValuesVMn;
    return v;
}

int $s7SwiftUI17EnvironmentValuesV11colorSchemeAA05ColorF0Ovg(void) { return sui_vals()->colorScheme; }
void $s7SwiftUI17EnvironmentValuesV11colorSchemeAA05ColorF0Ovs(int v) { sui_vals()->colorScheme = v; sui_put((void *)$s7SwiftUI17EnvironmentValuesV11colorSchemeAA05ColorF0Ovg, (uint64_t)v); }
void *$s7SwiftUI17EnvironmentValuesV11colorSchemeAA05ColorF0OvpMV(void) { return &sui_vals()->colorScheme; }

int $s7SwiftUI17EnvironmentValuesV9isEnabledSbvg(void) { return sui_vals()->isEnabled; }
void $s7SwiftUI17EnvironmentValuesV9isEnabledSbvs(int v) { sui_vals()->isEnabled = v; sui_put((void *)$s7SwiftUI17EnvironmentValuesV9isEnabledSbvg, (uint64_t)v); }
void *$s7SwiftUI17EnvironmentValuesV9isEnabledSbvpMV(void) { return &sui_vals()->isEnabled; }

int $s7SwiftUI17EnvironmentValuesV15layoutDirectionAA06LayoutF0Ovg(void) { return sui_vals()->layoutDirection; }
void $s7SwiftUI17EnvironmentValuesV15layoutDirectionAA06LayoutF0Ovs(int v) { sui_vals()->layoutDirection = v; sui_put((void *)$s7SwiftUI17EnvironmentValuesV15layoutDirectionAA06LayoutF0Ovg, (uint64_t)v); }
void *$s7SwiftUI17EnvironmentValuesV15layoutDirectionAA06LayoutF0OvpMV(void) { return &sui_vals()->layoutDirection; }

void *$s7SwiftUI17EnvironmentValuesV4fontAA4FontVSgvg(void) { return sui_vals()->font; }
void $s7SwiftUI17EnvironmentValuesV4fontAA4FontVSgvs(void *v) { sui_vals()->font = v; sui_put((void *)$s7SwiftUI17EnvironmentValuesV4fontAA4FontVSgvg, (uint64_t)(uintptr_t)v); }
void *$s7SwiftUI17EnvironmentValuesV4fontAA4FontVSgvpMV(void) { return &sui_vals()->font; }

void *$s7SwiftUI11EnvironmentVyACyxGs7KeyPathCyAA0C6ValuesVxGcfC(void *keyPath) {
    sui_env *e = (sui_env *)calloc(1, sizeof *e);
    if (!e) return 0;
    e->isa = &$s7SwiftUI11EnvironmentVMn;
    e->key = keyPath;
    return e;
}

uint64_t $s7SwiftUI11EnvironmentV12wrappedValuexvg(void) {
    sui_env *e = (sui_env *)sui_r13();
    uint64_t v = 0;
    if (!e) return 0;
    if (sui_find(e->key, &v)) return v;
    if (e->key == (void *)$s7SwiftUI17EnvironmentValuesV11colorSchemeAA05ColorF0Ovg) return (uint64_t)sui_global.colorScheme;
    if (e->key == (void *)$s7SwiftUI17EnvironmentValuesV9isEnabledSbvg) return (uint64_t)sui_global.isEnabled;
    if (e->key == (void *)$s7SwiftUI17EnvironmentValuesV15layoutDirectionAA06LayoutF0Ovg) return (uint64_t)sui_global.layoutDirection;
    if (e->key == (void *)$s7SwiftUI17EnvironmentValuesV4fontAA4FontVSgvg) return (uint64_t)(uintptr_t)sui_global.font;
    return 0;
}

void $s7SwiftUI4ViewPAAE11environmentyQrs15WritableKeyPathCyAA17EnvironmentValuesVqd__G_qd__tlF(void *key, uint64_t value) {
    sui_put(key, value);
    if (key == (void *)$s7SwiftUI17EnvironmentValuesV11colorSchemeAA05ColorF0Ovg) sui_global.colorScheme = (int)value;
    else if (key == (void *)$s7SwiftUI17EnvironmentValuesV9isEnabledSbvg) sui_global.isEnabled = (int)value;
    else if (key == (void *)$s7SwiftUI17EnvironmentValuesV15layoutDirectionAA06LayoutF0Ovg) sui_global.layoutDirection = (int)value;
    else if (key == (void *)$s7SwiftUI17EnvironmentValuesV4fontAA4FontVSgvg) sui_global.font = (void *)(uintptr_t)value;
}

/* Propriedades que o app lê e grava pelo mesmo caminho das de cima. */
#define SUI_ENV_INT(name, field) \
    int name##vg(void) { return sui_vals()->field; } \
    void name##vs(int v) { sui_vals()->field = v & 0xFF; sui_put((void *)name##vg, (uint64_t)(v & 0xFF)); } \
    void *name##vpMV(void) { return &sui_vals()->field; }
#define SUI_ENV_PTR(name, field) \
    void *name##vg(void) { return sui_vals()->field; } \
    void name##vs(void *v) { sui_vals()->field = v; sui_put((void *)name##vg, (uint64_t)(uintptr_t)v); } \
    void *name##vpMV(void) { return &sui_vals()->field; }

SUI_ENV_INT($s7SwiftUI17EnvironmentValuesV10imageScaleAA5ImageV0F0O, imageScale)
SUI_ENV_INT($s7SwiftUI17EnvironmentValuesV11controlSizeAA07ControlF0O, controlSize)
SUI_ENV_INT($s7SwiftUI17EnvironmentValuesV14truncationModeAA4TextV010TruncationF0O, truncationMode)
SUI_ENV_INT($s7SwiftUI17EnvironmentValuesV22multilineTextAlignmentAA0fG0O, multilineTextAlignment)
SUI_ENV_INT($s7SwiftUI17EnvironmentValuesV8textCaseAA4TextV0F0OSg, textCase)
SUI_ENV_PTR($s7SwiftUI17EnvironmentValuesV11accentColorAA0F0VSg, accentColor)
SUI_ENV_PTR($s7SwiftUI17EnvironmentValuesV15foregroundColorAA0F0VSg, foregroundColor)
SUI_ENV_PTR($s7SwiftUI17EnvironmentValuesV19symbolRenderingModeAA06SymbolfG0VSg, symbolRenderingMode)
SUI_ENV_PTR($s7SwiftUI17EnvironmentValuesV9tintColorAA0F0VSg, tintColor)

/* lineSpacing: CGFloat em xmm0. */
double $s7SwiftUI17EnvironmentValuesV11lineSpacing12CoreGraphics7CGFloatVvg(void) { return sui_vals()->lineSpacing; }
void $s7SwiftUI17EnvironmentValuesV11lineSpacing12CoreGraphics7CGFloatVvs(double v) {
    uint64_t bits;
    sui_vals()->lineSpacing = v < 0 ? 0 : v;
    memcpy(&bits, &sui_vals()->lineSpacing, sizeof bits);
    sui_put((void *)$s7SwiftUI17EnvironmentValuesV11lineSpacing12CoreGraphics7CGFloatVvg, bits);
}
void *$s7SwiftUI17EnvironmentValuesV11lineSpacing12CoreGraphics7CGFloatVvpMV(void) { return &sui_vals()->lineSpacing; }

/* lineLimit: Int? volta como (valor em rax, flag de nil em rdx). */
typedef struct { int64_t n; uint64_t is_nil; } sui_optint;
sui_optint $s7SwiftUI17EnvironmentValuesV9lineLimitSiSgvg(void) {
    sui_optint r;
    r.n = sui_vals()->lineLimitNil ? 0 : sui_vals()->lineLimit;
    r.is_nil = (uint64_t)sui_vals()->lineLimitNil;
    return r;
}
void $s7SwiftUI17EnvironmentValuesV9lineLimitSiSgvs(int64_t n, int is_nil) {
    sui_values *v = sui_vals();
    v->lineLimitNil = is_nil & 1;
    v->lineLimit = v->lineLimitNil ? 0 : (n < 1 ? 1 : n);
    sui_put((void *)$s7SwiftUI17EnvironmentValuesV9lineLimitSiSgvg, (uint64_t)v->lineLimit);
}
void *$s7SwiftUI17EnvironmentValuesV9lineLimitSiSgvpMV(void) { return &sui_vals()->lineLimit; }
