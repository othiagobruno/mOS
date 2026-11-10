/* Views: LocalizedStringKey e interpolação, Text e seus modificadores, Image,
 * Button, Toggle (com Binding de swiftui_state.c), contêineres e body.
 * clang -std=c11 -Wall -Wextra -Werror -DSWIFTUI_VIEWS_HOST -DSWIFTUI_STATE_HOST \
 *   userland/libsystem/swiftui_views.c userland/libsystem/swiftui_state.c \
 *   userland/tests-swiftui/views_test.c -o /tmp/sui_views
 */
#define SUI_HOST 1
#include <stdio.h>
#include "../libsystem/swiftui_model.h"

typedef struct { uint64_t w0, w1; } swstr;

void *$s7SwiftUI18LocalizedStringKeyV13stringLiteralACSS_tcfC(uint64_t w0, uint64_t w1);
void *$s7SwiftUI18LocalizedStringKeyV0D13InterpolationV15literalCapacity18interpolationCountAESi_SitcfC(int64_t cap, int64_t count);
void $s7SwiftUI18LocalizedStringKeyV0D13InterpolationV13appendLiteralyySSF(uint64_t w0, uint64_t w1);
void $s7SwiftUI18LocalizedStringKeyV0D13InterpolationV06appendF0yySSF(uint64_t w0, uint64_t w1);
void $s7SwiftUI18LocalizedStringKeyV0D13InterpolationV06appendF0_9specifieryx_SStAA18_FormatSpecifiableRzlF(const void *value, uint64_t s0, uint64_t s1, void *meta);
void *$s7SwiftUI18LocalizedStringKeyV19stringInterpolationA2C0dG0V_tcfC(void *in);
void *$s7SwiftUI4TextV_9tableName6bundle7commentAcA18LocalizedStringKeyV_SSSgSo8NSBundleCSgs06StaticI0VSgtcfC(void *key, uint64_t t0, uint64_t t1, void *bundle);
void *$s7SwiftUI4TextVyACxcSyRzlufC(const uint64_t *s, void *meta, void *wt);
void *$s7SwiftUI4TextV4boldACyF(void);
void *$s7SwiftUI4TextV10fontWeightyAcA4FontV0E0VSgF(double weight, int is_nil);
void *$s7SwiftUI4TextV1poiyA2C_ACtFZ(void *a, void *b);
void *$s7SwiftUI4TextV9underline_5colorACSb_AA5ColorVSgtF(int on, void *color);
void *$s7SwiftUI5ImageV10systemNameACSS_tcfC(uint64_t w0, uint64_t w1);
void *$s7SwiftUI5ImageV9resizable9capInsets12resizingModeAcA04EdgeF0V_AC08ResizingH0OtF(double t, double l, double b, double r, int mode);
void *$s7SwiftUI5ImageV13renderingModeyA2C017TemplateRenderingE0OSgF(int mode);
void *$s7SwiftUI6ButtonVA2A4TextVRszrlE_6actionACyAEGAA18LocalizedStringKeyV_yyctcfC(void *title, void *afn, void *actx);
void *$s7SwiftUI6ButtonV6action5labelACyxGyyc_xyXEtcfC(void *afn, void *actx, void *lfn, void *lctx);
void *$s7SwiftUI6ToggleVA2A4TextVRszrlE_4isOnACyAEGAA18LocalizedStringKeyV_AA7BindingVySbGtcfC(void *title, void *binding);
void *$s7SwiftUI10ScrollViewV_15showsIndicators7contentACyxGAA4AxisO3SetV_SbxyXEtcfC(int axes, int shows, void *fn, void *ctx);
void *$s7SwiftUI4ListVAAs5NeverORszrlE7contentACyAEq_Gq_yXE_tcfC(void *fn, void *ctx);
void *$s7SwiftUI7ForEachVAA7Element_2IDQZRs_AA4ViewR0_s12IdentifiableADRpzrlE_7contentACyxq_q0_Gx_q0_AIctcfC(void *data, void *fn, void *ctx);
void $s7SwiftUI7ForEachV7contentyq0_7ElementQzcvs(void *fn, void *ctx);
void *$s7SwiftUI12ProgressViewV5value5totalACyAA05EmptyD0VAGGqd__Sg_qd__tcAGRszAGRs_SBRd__lufC(const unsigned char *value, const double *total, void *meta, void *wt);
void *$s7SwiftUI7StepperVA2A4TextVRszrlE_5value2in4step16onEditingChangedACyAEGAA18LocalizedStringKeyV_AA7BindingVyqd__GSNyqd__G6StrideQyd__ySbctcSxRd__lufC(void *title, void *binding, const int64_t *range, const int64_t *step, void *efn, void *ectx);
void *$s7SwiftUI5AlertV6ButtonV7default_6actionAeA4TextV_yycSgtFZ(void *label, void *fn, void *ctx);
void *$s7SwiftUI19_ConditionalContentVA2A4ViewRzAaDR_rlE7storageACyxq_GAC7StorageOyxq__G_tcfC(const uint64_t *storage);
void *$s7SwiftUI7DividerVACycfC(void);
void *$s7SwiftUI5StateV12wrappedValueACyxGx_tcfC(uint64_t value, void *metadata);
void *$s7SwiftUI5StateV14projectedValueAA7BindingVyxGvg(void);
extern sui_desc $s7SwiftUI4TextVMn, $s7SwiftUI7DividerVMn;
extern sui_conf $s7SwiftUI4TextVAA4ViewAAMc, $s7SwiftUI6ButtonVyxGAA4ViewAAMc;
extern sui_meta $s7SwiftUI4TextVN;
int mos_sui_activate(void *view);

#define arm(p) __asm__ volatile("mov %0, %%r13" :: "r"(p) : "r13", "memory")

/* String pequena do Swift: bytes inline e 0xE0|n no último byte. */
static swstr small(const char *s) {
    swstr r = { 0, 0 };
    unsigned char b[16] = { 0 };
    int n = (int)strlen(s);
    memcpy(b, s, (size_t)n);
    b[15] = (unsigned char)(0xE0 | n);
    memcpy(&r.w0, b, 8);
    memcpy(&r.w1, b + 8, 8);
    return r;
}

/* Closures de teste: o contexto chega em r13. */
static int calls;
static void *ret_ctx(void) { void *c; __asm__ volatile("mov %%r13, %0" : "=r"(c)); calls++; return c; }
static void bump(void) { int *c; __asm__ volatile("mov %%r13, %0" : "=r"(c)); (*c)++; }

typedef void *(*body_fn)(void);

static void *body_of(sui_conf *conf, void *v) {
    body_fn f = (body_fn)conf->body;
    arm(v);
    return f();
}

int main(void) {
    int fails = 0, hits = 0;
    swstr s;
    sui_lsk *k, *k2;
    sui_interp *in;
    sui_node *t, *t2, *t3, *img, *b, *tg, *sv, *ls, *fe, *pv, *st, *ab, *cc;
    int64_t forty = 42, range[2] = { 1, 9 }, step = 2;
    double pi = 3.14159, total = 1.0;
    unsigned char opt[9];
    uint64_t storage[2];
    char mark = 'v';
    void *state, *bind;
    struct { int64_t n; char b[32]; } big = { 0, "Configurações gerais" };
    uint64_t bigstr[2];

    s = small("Salvar");
    k = $s7SwiftUI18LocalizedStringKeyV13stringLiteralACSS_tcfC(s.w0, s.w1);
    if (!k || strcmp(k->key, "Salvar") != 0) { fprintf(stderr, "FAIL LocalizedStringKey\n"); fails++; }

    in = $s7SwiftUI18LocalizedStringKeyV0D13InterpolationV15literalCapacity18interpolationCountAESi_SitcfC(8, 2);
    s = small("Total: ");
    arm(in); $s7SwiftUI18LocalizedStringKeyV0D13InterpolationV13appendLiteralyySSF(s.w0, s.w1);
    s = small("%d");
    arm(in); $s7SwiftUI18LocalizedStringKeyV0D13InterpolationV06appendF0_9specifieryx_SStAA18_FormatSpecifiableRzlF(&forty, s.w0, s.w1, 0);
    s = small(" / ");
    arm(in); $s7SwiftUI18LocalizedStringKeyV0D13InterpolationV13appendLiteralyySSF(s.w0, s.w1);
    s = small("%.2f");
    arm(in); $s7SwiftUI18LocalizedStringKeyV0D13InterpolationV06appendF0_9specifieryx_SStAA18_FormatSpecifiableRzlF(&pi, s.w0, s.w1, 0);
    s = small(" ok");
    arm(in); $s7SwiftUI18LocalizedStringKeyV0D13InterpolationV06appendF0yySSF(s.w0, s.w1);
    k2 = $s7SwiftUI18LocalizedStringKeyV19stringInterpolationA2C0dG0V_tcfC(in);
    if (!k2 || strcmp(k2->key, "Total: 42 / 3.14 ok") != 0 || !k2->formatted) {
        fprintf(stderr, "FAIL interpolação '%s'\n", k2 ? k2->key : "(nil)"); fails++;
    }

    t = $s7SwiftUI4TextV_9tableName6bundle7commentAcA18LocalizedStringKeyV_SSSgSo8NSBundleCSgs06StaticI0VSgtcfC(k, 0, 0, 0);
    if (!t || t->isa != &$s7SwiftUI4TextVMn || strcmp(t->text, "Salvar") != 0) { fprintf(stderr, "FAIL Text(key)\n"); fails++; }
    arm(t); t2 = $s7SwiftUI4TextV4boldACyF();
    arm(t2); t2 = $s7SwiftUI4TextV10fontWeightyAcA4FontV0E0VSgF(0.3, 0);
    if (!t2 || !(t2->flags & SUI_F_BOLD) || !(t2->flags & SUI_F_WEIGHT) || t2->num[1] != 0.3 || (t->flags & SUI_F_BOLD)) {
        fprintf(stderr, "FAIL bold/fontWeight\n"); fails++;
    }
    arm(t2); t2 = $s7SwiftUI4TextV9underline_5colorACSb_AA5ColorVSgtF(1, &mark);
    if (!(t2->flags & SUI_F_UNDERLINE) || t2->extra != &mark) { fprintf(stderr, "FAIL underline\n"); fails++; }
    big.n = (int64_t)strlen(big.b);
    bigstr[0] = (uint64_t)(uintptr_t)&big;
    bigstr[1] = (0x80ULL << 56) | (uint64_t)big.n;
    t3 = $s7SwiftUI4TextVyACxcSyRzlufC(bigstr, 0, 0);
    if (!t3 || strcmp(t3->text, "Configurações gerais") != 0) { fprintf(stderr, "FAIL Text(String grande)\n"); fails++; }
    t3 = $s7SwiftUI4TextV1poiyA2C_ACtFZ(t2, t3);
    if (!t3 || strcmp(t3->text, "SalvarConfigurações gerais") != 0 || !(t3->flags & SUI_F_BOLD)) { fprintf(stderr, "FAIL Text + Text\n"); fails++; }
    if ($s7SwiftUI4TextVN.desc != &$s7SwiftUI4TextVMn || body_of(&$s7SwiftUI4TextVAA4ViewAAMc, t) != 0) {
        fprintf(stderr, "FAIL Text é folha\n"); fails++;
    }

    s = small("gearshape");
    img = $s7SwiftUI5ImageV10systemNameACSS_tcfC(s.w0, s.w1);
    if (!img || strcmp(img->text, "gearshape") != 0 || !(img->flags & SUI_F_TEMPLATE)) { fprintf(stderr, "FAIL Image(systemName:)\n"); fails++; }
    arm(img); img = $s7SwiftUI5ImageV9resizable9capInsets12resizingModeAcA04EdgeF0V_AC08ResizingH0OtF(1, 2, 3, 4, 1);
    arm(img); img = $s7SwiftUI5ImageV13renderingModeyA2C017TemplateRenderingE0OSgF(1);
    if (!(img->flags & SUI_F_RESIZABLE) || img->num[3] != 4 || img->aux[0] != 1 || (img->flags & SUI_F_TEMPLATE) || !(img->flags & SUI_F_ORIGINAL)) {
        fprintf(stderr, "FAIL Image modificadores\n"); fails++;
    }

    b = $s7SwiftUI6ButtonVA2A4TextVRszrlE_6actionACyAEGAA18LocalizedStringKeyV_yyctcfC(k, (void *)bump, &hits);
    if (!mos_sui_activate(b) || hits != 1) { fprintf(stderr, "FAIL Button ação\n"); fails++; }
    if (sui_kind(body_of(&$s7SwiftUI6ButtonVyxGAA4ViewAAMc, b)) != SUI_K_TEXT) { fprintf(stderr, "FAIL Button body\n"); fails++; }
    b = $s7SwiftUI6ButtonV6action5labelACyxGyyc_xyXEtcfC((void *)bump, &hits, (void *)ret_ctx, &mark);
    calls = 0;
    if (body_of(&$s7SwiftUI6ButtonVyxGAA4ViewAAMc, b) != &mark || body_of(&$s7SwiftUI6ButtonVyxGAA4ViewAAMc, b) != &mark || calls != 1) {
        fprintf(stderr, "FAIL label montado uma vez\n"); fails++;
    }

    state = $s7SwiftUI5StateV12wrappedValueACyxGx_tcfC(0, 0);
    arm(state);
    bind = $s7SwiftUI5StateV14projectedValueAA7BindingVyxGvg();
    tg = $s7SwiftUI6ToggleVA2A4TextVRszrlE_4isOnACyAEGAA18LocalizedStringKeyV_AA7BindingVySbGtcfC(k, bind);
    if (!mos_sui_activate(tg) || ((uint64_t *)state)[1] != 1 || !mos_sui_activate(tg) || ((uint64_t *)state)[1] != 0) {
        fprintf(stderr, "FAIL Toggle inverte o Binding\n"); fails++;
    }

    sv = $s7SwiftUI10ScrollViewV_15showsIndicators7contentACyxGAA4AxisO3SetV_SbxyXEtcfC(2, 1, (void *)ret_ctx, &mark);
    if (!sv || sv->code != 2 || !(sv->flags & SUI_F_INDICATORS) || body_of(&$s7SwiftUI6ButtonVyxGAA4ViewAAMc, sv) != &mark) {
        fprintf(stderr, "FAIL ScrollView\n"); fails++;
    }
    ls = $s7SwiftUI4ListVAAs5NeverORszrlE7contentACyAEq_Gq_yXE_tcfC((void *)ret_ctx, &forty);
    if (body_of(&$s7SwiftUI6ButtonVyxGAA4ViewAAMc, ls) != &forty) { fprintf(stderr, "FAIL List body\n"); fails++; }
    fe = $s7SwiftUI7ForEachVAA7Element_2IDQZRs_AA4ViewR0_s12IdentifiableADRpzrlE_7contentACyxq_q0_Gx_q0_AIctcfC(range, (void *)ret_ctx, &mark);
    body_of(&$s7SwiftUI6ButtonVyxGAA4ViewAAMc, fe);
    arm(fe); $s7SwiftUI7ForEachV7contentyq0_7ElementQzcvs((void *)ret_ctx, &forty);
    if (fe->native != range || body_of(&$s7SwiftUI6ButtonVyxGAA4ViewAAMc, fe) != &forty) { fprintf(stderr, "FAIL ForEach.content\n"); fails++; }

    memcpy(opt, &(double){ 0.25 }, 8);
    opt[8] = 0;
    pv = $s7SwiftUI12ProgressViewV5value5totalACyAA05EmptyD0VAGGqd__Sg_qd__tcAGRszAGRs_SBRd__lufC(opt, &total, 0, 0);
    opt[8] = 1;
    if (!pv || pv->num[0] != 0.25 || pv->num[1] != 1 || (pv->flags & SUI_F_INDETERMINATE)) { fprintf(stderr, "FAIL ProgressView\n"); fails++; }
    pv = $s7SwiftUI12ProgressViewV5value5totalACyAA05EmptyD0VAGGqd__Sg_qd__tcAGRszAGRs_SBRd__lufC(opt, &total, 0, 0);
    if (!(pv->flags & SUI_F_INDETERMINATE)) { fprintf(stderr, "FAIL ProgressView nil\n"); fails++; }

    st = $s7SwiftUI7StepperVA2A4TextVRszrlE_5value2in4step16onEditingChangedACyAEGAA18LocalizedStringKeyV_AA7BindingVyqd__GSNyqd__G6StrideQyd__ySbctcSxRd__lufC(k, bind, range, &step, 0, 0);
    if (!st || st->num[0] != 1 || st->num[1] != 9 || st->num[2] != 2) { fprintf(stderr, "FAIL Stepper\n"); fails++; }

    hits = 0;
    ab = $s7SwiftUI5AlertV6ButtonV7default_6actionAeA4TextV_yycSgtFZ(t, (void *)bump, &hits);
    if (!mos_sui_activate(ab) || hits != 1) { fprintf(stderr, "FAIL Alert.Button\n"); fails++; }
    storage[0] = (uint64_t)(uintptr_t)t; storage[1] = 1;
    cc = $s7SwiftUI19_ConditionalContentVA2A4ViewRzAaDR_rlE7storageACyxq_GAC7StorageOyxq__G_tcfC(storage);
    if (!cc || cc->code != 1 || body_of(&$s7SwiftUI6ButtonVyxGAA4ViewAAMc, cc) != t) { fprintf(stderr, "FAIL _ConditionalContent\n"); fails++; }
    if (sui_isa($s7SwiftUI7DividerVACycfC()) != &$s7SwiftUI7DividerVMn) { fprintf(stderr, "FAIL Divider\n"); fails++; }

    if (fails) { fprintf(stderr, "RESULT FAIL %d\n", fails); return 1; }
    printf("RESULT PASS\n");
    return 0;
}
