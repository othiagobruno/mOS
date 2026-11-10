/* Protocolos como dados, padrões dos Representables, TimelineView,
 * ToolbarItem, ImageRenderer e Log.runtimeIssues.
 * clang -std=c11 -Wall -Wextra -Werror -DSWIFTUI_PROTO_HOST \
 *   userland/libsystem/swiftui_protocols.c userland/tests-swiftui/protocols_test.c -o /tmp/sui_proto
 */
#define SUI_HOST 1
#include <stdio.h>
#include "../libsystem/swiftui_model.h"

typedef struct { const sui_desc *isa; double interval; int paused; } sched;
typedef struct { const sui_desc *isa; double date; int64_t cadence; } tlctx;
typedef struct { const sui_desc *isa; int64_t placement; sui_fn content; } toolitem;

extern sui_desc $s7SwiftUI4ViewMp, $s7SwiftUI19NSViewRepresentableMp, $s7SwiftUI5ShapeMp, $s7SwiftUI12TimelineViewV7ContextVMn;
extern sui_req $s7SwiftUI4ViewP4BodyAC_AaBTn, $s4Body7SwiftUI4ViewPTl, $s7SwiftUI5ShapePAA10AnimatableTb,
    $s7SwiftUI19NSViewRepresentableP04makeC07context0C4TypeQzAA0cD7ContextVyxG_tFTq, $s11Coordinator7SwiftUI19NSViewRepresentablePTl;
extern sui_conf $s7SwiftUI12TimelineViewVyxq_GAA0D0A2aER_rlMc;
int $s7SwiftUI19NSViewRepresentablePAAE35_invalidatesSizeOnConstraintChangesSbvgZ(void);
void *$s7SwiftUI26UIViewRepresentableContextV11coordinator11CoordinatorQzvg(void);
void *$s7SwiftUI25AnimationTimelineScheduleV15minimumInterval6pausedACSdSg_SbtcfC(double interval, int is_nil, int paused);
void *$s7SwiftUI12TimelineViewVA2A0D0R_rlE_7contentACyxq_Gx_q_AC7ContextVyxq__GctcfC(void *schedule, void *fn, void *ctx);
void *$s7SwiftUI11ToolbarItemVAAytRszrlE9placement7contentACyytq_GAA0cD9PlacementV_q_yXEtcfC(int64_t placement, void *fn, void *ctx);
void *$s7SwiftUI13ImageRendererC7contentACyxGx_tcfC(void *content, void *meta);
void $s7SwiftUI13ImageRendererC5scale12CoreGraphics7CGFloatVvs(double scale);
void $s7SwiftUI13ImageRendererC12proposedSizeAA012ProposedViewF0Vvs(double w, int wnil, double h, int hnil);
void *$s7SwiftUI13ImageRendererC02nsC0So7NSImageCSgvg(void);
void *$s7SwiftUI3LogO013runtimeIssuesC0So9OS_os_logCvgZ(void);

#define arm(p) __asm__ volatile("mov %0, %%r13" :: "r"(p) : "r13", "memory")

/* sizeThatFits padrão: lê xmm0, xmm1 e al. */
static int size_nil(double *w, double *h) {
    double a = 7, b = 7;
    uint64_t flag = 0;
    __asm__ volatile(
        "call \"$s7SwiftUI19NSViewRepresentablePAAE12sizeThatFits_6nsView7contextSo6CGSizeVSgAA08ProposedI4SizeV_0C4TypeQzAA0cD7ContextVyxGtF\"\n\t"
        "movsd %%xmm0, %0\n\t"
        "movsd %%xmm1, %1\n\t"
        "movzbq %%al, %2\n\t"
        : "=m"(a), "=m"(b), "=r"(flag)
        :
        : "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11", "xmm0", "xmm1", "memory");
    *w = a; *h = b;
    return (int)flag;
}

static tlctx *seen_ctx;
static char built = 'b';
static void *timeline_content(tlctx *c) { seen_ctx = c; return &built; }

typedef void *(*body_fn)(void);

int main(void) {
    int fails = 0;
    double w, h;
    void *ctx[3];
    char coord = 'k';
    sched *s;
    void *tl;
    toolitem *ti;
    void *r;
    double *img;
    if ($s7SwiftUI4ViewP4BodyAC_AaBTn.proto != &$s7SwiftUI4ViewMp || strcmp($s7SwiftUI4ViewP4BodyAC_AaBTn.name, "Body: View") != 0 ||
        $s4Body7SwiftUI4ViewPTl.proto != &$s7SwiftUI4ViewMp || strcmp($s7SwiftUI4ViewMp.name, "View") != 0) {
        fprintf(stderr, "FAIL View\n"); fails++;
    }
    if ($s7SwiftUI5ShapePAA10AnimatableTb.proto != &$s7SwiftUI5ShapeMp || strcmp($s7SwiftUI5ShapePAA10AnimatableTb.name, "base: Animatable") != 0) {
        fprintf(stderr, "FAIL Shape base\n"); fails++;
    }
    if (strcmp($s7SwiftUI19NSViewRepresentableP04makeC07context0C4TypeQzAA0cD7ContextVyxG_tFTq.name, "makeNSView(context:)") != 0 ||
        $s11Coordinator7SwiftUI19NSViewRepresentablePTl.proto != &$s7SwiftUI19NSViewRepresentableMp) {
        fprintf(stderr, "FAIL NSViewRepresentable\n"); fails++;
    }
    if (size_nil(&w, &h) != 1 || w != 0 || h != 0) { fprintf(stderr, "FAIL sizeThatFits nil\n"); fails++; }
    if (!$s7SwiftUI19NSViewRepresentablePAAE35_invalidatesSizeOnConstraintChangesSbvgZ()) { fprintf(stderr, "FAIL invalidates\n"); fails++; }
    ctx[0] = &coord; ctx[1] = 0; ctx[2] = 0;
    arm(ctx);
    if ($s7SwiftUI26UIViewRepresentableContextV11coordinator11CoordinatorQzvg() != &coord) { fprintf(stderr, "FAIL coordinator\n"); fails++; }

    s = $s7SwiftUI25AnimationTimelineScheduleV15minimumInterval6pausedACSdSg_SbtcfC(0, 1, 1);
    if (!s || s->interval != 1.0 / 60.0 || !s->paused) { fprintf(stderr, "FAIL AnimationTimelineSchedule\n"); fails++; }
    tl = $s7SwiftUI12TimelineViewVA2A0D0R_rlE_7contentACyxq_Gx_q_AC7ContextVyxq__GctcfC(s, (void *)timeline_content, 0);
    arm(tl);
    if (((body_fn)$s7SwiftUI12TimelineViewVyxq_GAA0D0A2aER_rlMc.body)() != &built || !seen_ctx ||
        seen_ctx->isa != &$s7SwiftUI12TimelineViewV7ContextVMn || seen_ctx->date <= 0) {
        fprintf(stderr, "FAIL TimelineView body\n"); fails++;
    } else {
        double first = seen_ctx->date;
        arm(tl);
        ((body_fn)$s7SwiftUI12TimelineViewVyxq_GAA0D0A2aER_rlMc.body)();
        if (!(seen_ctx->date > first)) { fprintf(stderr, "FAIL data do TimelineView avança\n"); fails++; }
    }
    ti = $s7SwiftUI11ToolbarItemVAAytRszrlE9placement7contentACyytq_GAA0cD9PlacementV_q_yXEtcfC(1, (void *)timeline_content, &coord);
    if (!ti || ti->placement != 1 || ti->content.ctx != &coord) { fprintf(stderr, "FAIL ToolbarItem\n"); fails++; }

    r = $s7SwiftUI13ImageRendererC7contentACyxGx_tcfC(&built, 0);
    arm(r);
    $s7SwiftUI13ImageRendererC5scale12CoreGraphics7CGFloatVvs(2);
    arm(r);
    $s7SwiftUI13ImageRendererC12proposedSizeAA012ProposedViewF0Vvs(50, 0, 0, 1);
    arm(r);
    img = $s7SwiftUI13ImageRendererC02nsC0So7NSImageCSgvg();
    if (!img || img[0] != 100 || img[1] != 400) { fprintf(stderr, "FAIL ImageRenderer\n"); fails++; }
    if (!$s7SwiftUI3LogO013runtimeIssuesC0So9OS_os_logCvgZ() ||
        $s7SwiftUI3LogO013runtimeIssuesC0So9OS_os_logCvgZ() != $s7SwiftUI3LogO013runtimeIssuesC0So9OS_os_logCvgZ()) {
        fprintf(stderr, "FAIL Log.runtimeIssues\n"); fails++;
    }
    if (fails) { fprintf(stderr, "RESULT FAIL %d\n", fails); return 1; }
    printf("RESULT PASS\n");
    return 0;
}
