/* Modelo comum dos blocos swiftui_views/modifiers/values/observable.
 * Mesma convenção de swiftui_layout.c: self do swiftcc em r13, valor de view
 * é um ponteiro cujo primeiro campo aponta para um sui_desc. Closure Swift
 * chega como par (função, contexto); o contexto vai em r13 na chamada.
 */
#ifndef MOS_SWIFTUI_MODEL_H
#define MOS_SWIFTUI_MODEL_H

#ifdef SUI_HOST
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#else
#include "libc.h"
#endif

typedef struct { uint32_t kind; const char *name; } sui_desc;
/* Descritor de conformidade (Mc): mesmo formato do de HStack em swiftui_layout.c. */
typedef struct { void *body; const char *proto; } sui_conf;
/* Tabela de testemunhas (WP): aponta para a conformidade e para o body. */
typedef struct { const sui_conf *conf; void *body; } sui_wtab;
/* Requisito de protocolo (Tq, Tl, Tn, Tb): protocolo dono e nome do requisito. */
typedef struct { const sui_desc *proto; const char *name; } sui_req;
/* Metadado de tipo (N): só aponta para o descritor nominal. */
typedef struct { const sui_desc *desc; } sui_meta;
/* Descritor de tipo opaco (QOMQ): nome do modificador que o produziu. */
typedef struct { uint32_t kind; const char *name; } sui_opaque;

typedef struct { void *fn; void *ctx; } sui_fn;

/* Faixa de tipos destes blocos; 1..11 são de hosting/state/env/layout. */
enum {
    SUI_K_TEXT = 20, SUI_K_IMAGE, SUI_K_BUTTON, SUI_K_LABEL, SUI_K_TOGGLE,
    SUI_K_TEXTFIELD, SUI_K_TEXTEDITOR, SUI_K_SCROLL, SUI_K_LIST, SUI_K_FOREACH,
    SUI_K_SPACER, SUI_K_DIVIDER, SUI_K_PROGRESS, SUI_K_MENU, SUI_K_PICKER,
    SUI_K_NAVVIEW, SUI_K_NAVLINK, SUI_K_SECTION, SUI_K_STEPPER, SUI_K_SLIDER,
    SUI_K_GROUP, SUI_K_ANYVIEW, SUI_K_TUPLE, SUI_K_EMPTY, SUI_K_COND,
    SUI_K_COLOR, SUI_K_FONT, SUI_K_ALERT, SUI_K_ALERTBTN, SUI_K_LSK,
    SUI_K_STYLE, SUI_K_MODIFIED = 60, SUI_K_MODIFIER, SUI_K_EDGEINSETS,
    SUI_K_ANIMATION, SUI_K_TRANSACTION, SUI_K_TRANSITION, SUI_K_OBSERVED,
    SUI_K_STATEOBJ, SUI_K_FOCUS, SUI_K_GESTURESTATE, SUI_K_BINDABLE, SUI_K_SPRING,
    SUI_K_INTERP, SUI_K_ENUM = 90, SUI_K_STRUCT, SUI_K_PROTO
};

/* Flags de sui_node. */
enum {
    SUI_F_BOLD = 1, SUI_F_UNDERLINE = 2, SUI_F_RESIZABLE = 4, SUI_F_TEMPLATE = 8,
    SUI_F_SECURE = 16, SUI_F_INDETERMINATE = 32, SUI_F_INDICATORS = 64,
    SUI_F_WEIGHT = 128, SUI_F_ORIGINAL = 256, SUI_F_BUILT = 512
};

/* Nó genérico das views primitivas e contêineres. */
typedef struct {
    const sui_desc *isa;
    char *text;       /* texto, título ou nome do símbolo */
    void *font;       /* sui_font */
    void *color;      /* sui_color */
    void *binding;    /* Binding de isOn/text/selection/value */
    void *child;      /* rótulo ou conteúdo já montado */
    void *native;     /* NSImage, bundle, dados do ForEach, AttributedString */
    void *extra;      /* segunda view (ícone, cabeçalho, mensagem) */
    sui_fn content;   /* closure de conteúdo */
    sui_fn label;     /* closure de rótulo ou destino */
    sui_fn action;    /* ação ou onCommit */
    sui_fn edit;      /* onEditingChanged */
    double num[4];    /* valor, total, kerning, peso, inset */
    uint64_t flags;
    int64_t code;     /* eixos, modo, caso de enum */
    int64_t aux[2];   /* modo de redimensionar e interpolação da Image */
} sui_node;

typedef struct { const sui_desc *isa; char *key; int formatted; } sui_lsk;
typedef struct { const sui_desc *isa; char *buf; int len, cap, args; } sui_interp;
typedef struct { const sui_desc *isa; double r, g, b, a; void *native; int space; } sui_color;
typedef struct { const sui_desc *isa; double size, weight; int design, style, bold; void *ct; } sui_font;

/* Modificador aplicado: ModifiedContent { content, modifier }. */
enum {
    SUI_OP_PADDING = 1, SUI_OP_FRAME, SUI_OP_FLEXFRAME, SUI_OP_BACKGROUND, SUI_OP_BGSTYLE,
    SUI_OP_OVERLAY, SUI_OP_OVSTYLE, SUI_OP_FGCOLOR, SUI_OP_FGSTYLE, SUI_OP_FONT,
    SUI_OP_OPACITY, SUI_OP_CORNER, SUI_OP_CLIP, SUI_OP_DISABLED, SUI_OP_APPEAR,
    SUI_OP_CHANGE, SUI_OP_TAP, SUI_OP_HELP, SUI_OP_A11Y_LABEL, SUI_OP_A11Y_HIDDEN,
    SUI_OP_A11Y_ELEMENT, SUI_OP_A11Y_TRAITS, SUI_OP_SHORTCUT, SUI_OP_FOCUSED,
    SUI_OP_SHEET, SUI_OP_ALERT, SUI_OP_POPOVER, SUI_OP_TOOLBAR, SUI_OP_NAVTITLE,
    SUI_OP_TITLEMODE, SUI_OP_STYLE, SUI_OP_SEARCHABLE, SUI_OP_ACCENT, SUI_OP_SCALEFIT,
    SUI_OP_CONTENTSHAPE, SUI_OP_KEYBOARDTYPE, SUI_OP_LABELSHIDDEN, SUI_OP_ROWINSETS,
    SUI_OP_EXIT, SUI_OP_TRUNCATION, SUI_OP_COORDSPACE, SUI_OP_HITTEST, SUI_OP_ROWBG,
    SUI_OP_BARITEMS, SUI_OP_AUTOCORRECT, SUI_OP_TEXTALIGN, SUI_OP_BLUR, SUI_OP_MASK,
    SUI_OP_TRAIT, SUI_OP_OFFSET, SUI_OP_GESTURE, SUI_OP_HOVER, SUI_OP_MODIFIER,
    SUI_OP_SUBMIT, SUI_OP_POSITION, SUI_OP_TEXTCASE, SUI_OP_BLEND, SUI_OP_FIXEDSIZE,
    SUI_OP_LINELIMIT
};

typedef struct {
    const sui_desc *isa;  /* tipo do modificador (_PaddingLayout, _OpacityEffect...) */
    int op;               /* SUI_OP_* */
    int fired;            /* onAppear já rodou; apresentação visível */
    double d[6];          /* números: padding, frame, opacidade, raio, offset */
    uint64_t u[4];        /* bordas, flags de nil, alinhamento, enum, bool */
    void *p[3];           /* view, cor, estilo, Binding, texto */
    sui_fn fn, fn2;       /* ação e segunda ação (onDismiss, message) */
} sui_mod;

typedef struct { const sui_desc *isa; void *content; sui_mod *mod; } sui_modified;

static inline void *sui_self(void) {
    void *p;
    __asm__ volatile("mov %%r13, %0" : "=r"(p));
    return p;
}

static inline const sui_desc *sui_isa(const void *v) {
    return v ? *(const sui_desc *const *)v : 0;
}

static inline int sui_kind(const void *v) {
    const sui_desc *d = sui_isa(v);
    return d ? (int)d->kind : 0;
}

/* Chama uma closure Swift: contexto em r13, até três words em rdi, rsi e rdx;
 * volta o rax. r13 é salvo e restaurado; o resto segue a convenção de C. */
__attribute__((naked, unused)) static uint64_t sui_tramp(void *fn, void *ctx, uint64_t a, uint64_t b, uint64_t c) {
    __asm__ volatile(
        "push %r13\n\t"
        "push %rbx\n\t"
        "sub $8, %rsp\n\t"
        "mov %rdi, %rax\n\t"
        "mov %rsi, %r13\n\t"
        "mov %rdx, %rdi\n\t"
        "mov %rcx, %rsi\n\t"
        "mov %r8, %rdx\n\t"
        "call *%rax\n\t"
        "add $8, %rsp\n\t"
        "pop %rbx\n\t"
        "pop %r13\n\t"
        "ret\n\t");
}

static inline void sui_call0(sui_fn f) { if (f.fn) sui_tramp(f.fn, f.ctx, 0, 0, 0); }
static inline void sui_call1(sui_fn f, uint64_t a) { if (f.fn) sui_tramp(f.fn, f.ctx, a, 0, 0); }
static inline void sui_call2(sui_fn f, uint64_t a, uint64_t b) { if (f.fn) sui_tramp(f.fn, f.ctx, a, b, 0); }
static inline void sui_call3(sui_fn f, uint64_t a, uint64_t b, uint64_t c) { if (f.fn) sui_tramp(f.fn, f.ctx, a, b, c); }
static inline uint64_t sui_call0r(sui_fn f) { return f.fn ? sui_tramp(f.fn, f.ctx, 0, 0, 0) : 0; }
static inline uint64_t sui_call1r(sui_fn f, uint64_t a) { return f.fn ? sui_tramp(f.fn, f.ctx, a, 0, 0) : 0; }

/* Lê uma String Swift (dois words) para C. Aceita a forma pequena, a forma
 * do fdn_pack do libSystem (cabeçalho {n, bytes}) e a nativa grande (+32). */
static inline int sui_unstr(uint64_t w0, uint64_t w1, char *buf, int cap) {
    unsigned char b[16];
    int n = 0;
    const char *src = 0;
    if (cap <= 0) return -1;
    memcpy(b, &w0, 8);
    memcpy(b + 8, &w1, 8);
    if ((b[15] & 0xF0) == 0xE0) {
        n = b[15] & 0x0F;
        src = (const char *)b;
    } else if ((w1 >> 56) == 0x80 && w0 && (w0 >> 48) == 0 &&
               *(const int64_t *)(uintptr_t)w0 == (int64_t)(w1 & 0xFFFFFFFFULL)) {
        n = (int)(w1 & 0xFFFFFFFFULL);
        src = (const char *)(uintptr_t)w0 + 8;
    } else if ((w1 & 0x00FFFFFFFFFFFFFFULL) && (w0 & 0xFFFFFFFFFFFFULL) && !(w1 & (1ULL << 62))) {
        /* Nativa ou literal; a ponte com NSString (bit 62) não é lida aqui. */
        n = (int)(w0 & 0xFFFFFFFFFFFFULL);
        src = (const char *)(uintptr_t)(w1 & 0x00FFFFFFFFFFFFFFULL) + 32;
    } else {
        buf[0] = 0;
        return 0;
    }
    if (n >= cap) n = cap - 1;
    if (n > 0) memcpy(buf, src, (size_t)n);
    buf[n] = 0;
    return n;
}

static inline char *sui_strdup2(uint64_t w0, uint64_t w1) {
    char tmp[512];
    char *r;
    int n = sui_unstr(w0, w1, tmp, (int)sizeof tmp);
    if (n < 0) n = 0;
    r = (char *)malloc((size_t)n + 1);
    if (!r) return 0;
    memcpy(r, tmp, (size_t)n + 1);
    return r;
}

static inline char *sui_cdup(const char *s) {
    size_t n = s ? strlen(s) : 0;
    char *r = (char *)malloc(n + 1);
    if (!r) return 0;
    if (n) memcpy(r, s, n);
    r[n] = 0;
    return r;
}

static inline void *sui_new(const sui_desc *d, size_t size) {
    void *p = calloc(1, size);
    if (p) *(const sui_desc **)p = d;
    return p;
}

#endif
