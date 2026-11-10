/* Valores pequenos do SwiftUI: Alignment, UnitPoint, Edge.Set, EdgeInsets,
 * casos de enum (WC = índice do caso, como o cliente resiliente lê) e
 * OptionSets. Alinhamento vertical usa -1/0/1 como o HStack de swiftui_layout.c.
 */
#ifdef SWIFTUI_VALUES_HOST
#define SUI_HOST 1
#endif
#include "swiftui_model.h"

typedef struct { int64_t h, v; } sui_align2;
typedef struct { double x, y; } sui_point;
typedef struct { uint64_t w0, w1; } sui_pair;

/* Alignment: dois words, horizontal e vertical. */
static sui_align2 sui_al(int64_t h, int64_t v) { sui_align2 a; a.h = h; a.v = v; return a; }
sui_align2 $s7SwiftUI9AlignmentV10topLeadingACvgZ(void) { return sui_al(-1, -1); }
sui_align2 $s7SwiftUI9AlignmentV3topACvgZ(void) { return sui_al(0, -1); }
sui_align2 $s7SwiftUI9AlignmentV11topTrailingACvgZ(void) { return sui_al(1, -1); }
sui_align2 $s7SwiftUI9AlignmentV7leadingACvgZ(void) { return sui_al(-1, 0); }
sui_align2 $s7SwiftUI9AlignmentV6centerACvgZ(void) { return sui_al(0, 0); }
sui_align2 $s7SwiftUI9AlignmentV8trailingACvgZ(void) { return sui_al(1, 0); }
sui_align2 $s7SwiftUI9AlignmentV13bottomLeadingACvgZ(void) { return sui_al(-1, 1); }
sui_align2 $s7SwiftUI9AlignmentV6bottomACvgZ(void) { return sui_al(0, 1); }
sui_align2 $s7SwiftUI9AlignmentV14bottomTrailingACvgZ(void) { return sui_al(1, 1); }

int64_t $s7SwiftUI19HorizontalAlignmentV7leadingACvgZ(void) { return -1; }
int64_t $s7SwiftUI19HorizontalAlignmentV6centerACvgZ(void) { return 0; }
int $s7SwiftUI19HorizontalAlignmentV2eeoiySbAC_ACtFZ(int64_t a, int64_t b) { return a == b; }

/* Linhas de base: ±2, o HStack trata como topo e base. */
int64_t $s7SwiftUI17VerticalAlignmentV6centerACvgZ(void) { return 0; }
int64_t $s7SwiftUI17VerticalAlignmentV6bottomACvgZ(void) { return 1; }
int64_t $s7SwiftUI17VerticalAlignmentV17firstTextBaselineACvgZ(void) { return -2; }
int64_t $s7SwiftUI17VerticalAlignmentV16lastTextBaselineACvgZ(void) { return 2; }

/* UnitPoint: dois CGFloat em xmm0/xmm1. */
sui_desc $s7SwiftUI9UnitPointVMn = { SUI_K_STRUCT, "UnitPoint" };
static sui_point sui_up(double x, double y) { sui_point p; p.x = x; p.y = y; return p; }
sui_point $s7SwiftUI9UnitPointV10topLeadingACvgZ(void) { return sui_up(0, 0); }
sui_point $s7SwiftUI9UnitPointV3topACvgZ(void) { return sui_up(0.5, 0); }
sui_point $s7SwiftUI9UnitPointV7leadingACvgZ(void) { return sui_up(0, 0.5); }
sui_point $s7SwiftUI9UnitPointV6centerACvgZ(void) { return sui_up(0.5, 0.5); }
sui_point $s7SwiftUI9UnitPointV8trailingACvgZ(void) { return sui_up(1, 0.5); }
sui_point $s7SwiftUI9UnitPointV6bottomACvgZ(void) { return sui_up(0.5, 1); }
sui_point $s7SwiftUI9UnitPointV14bottomTrailingACvgZ(void) { return sui_up(1, 1); }

/* Edge.Set (Int8): top 1, leading 2, bottom 4, trailing 8. */
int8_t $s7SwiftUI4EdgeO3SetV3topAEvgZ(void) { return 1; }
int8_t $s7SwiftUI4EdgeO3SetV7leadingAEvgZ(void) { return 2; }
int8_t $s7SwiftUI4EdgeO3SetV6bottomAEvgZ(void) { return 4; }
int8_t $s7SwiftUI4EdgeO3SetV8trailingAEvgZ(void) { return 8; }
int8_t $s7SwiftUI4EdgeO3SetV10horizontalAEvgZ(void) { return 2 | 8; }
int8_t $s7SwiftUI4EdgeO3SetV8verticalAEvgZ(void) { return 1 | 4; }
int8_t $s7SwiftUI4EdgeO3SetV3allAEvgZ(void) { return 15; }
/* Edge: top 0, leading 1, bottom 2, trailing 3. */
int8_t $s7SwiftUI4EdgeO3SetVyAeCcfC(int edge) { return (edge >= 0 && edge < 4) ? (int8_t)(1 << edge) : 0; }

/* Axis.Set: horizontal 1, vertical 2. */
int8_t $s7SwiftUI4AxisO3SetV10horizontalAEvgZ(void) { return 1; }
int8_t $s7SwiftUI4AxisO3SetV8verticalAEvgZ(void) { return 2; }

/* EdgeInsets: quatro CGFloat (top, leading, bottom, trailing) em xmm0..xmm3. */
sui_desc $s7SwiftUI10EdgeInsetsVMn = { SUI_K_EDGEINSETS, "EdgeInsets" };
sui_meta $s7SwiftUI10EdgeInsetsVN = { &$s7SwiftUI10EdgeInsetsVMn };
__asm__(
    ".text\n"
    ".p2align 4\n"
    ".globl \"$s7SwiftUI10EdgeInsetsV4_allAC12CoreGraphics7CGFloatV_tcfC\"\n"
    ".type \"$s7SwiftUI10EdgeInsetsV4_allAC12CoreGraphics7CGFloatV_tcfC\", @function\n"
    "\"$s7SwiftUI10EdgeInsetsV4_allAC12CoreGraphics7CGFloatV_tcfC\":\n"
    "    movapd %xmm0, %xmm1\n"
    "    movapd %xmm0, %xmm2\n"
    "    movapd %xmm0, %xmm3\n"
    "    ret\n");

/* ColorScheme: light 0, dark 1 (o mesmo int do EnvironmentValues). */
sui_desc $s7SwiftUI11ColorSchemeOMn = { SUI_K_ENUM, "ColorScheme" };
const int32_t $s7SwiftUI11ColorSchemeO5lightyA2CmFWC = 0;
const int32_t $s7SwiftUI11ColorSchemeO4darkyA2CmFWC = 1;
void *$s7SwiftUI11ColorSchemeOMa(void) { return &$s7SwiftUI11ColorSchemeOMn; }
int $s7SwiftUI11ColorSchemeO2eeoiySbAC_ACtFZ(int a, int b) { return (a & 0xFF) == (b & 0xFF); }

/* LayoutDirection: leftToRight 0, rightToLeft 1. */
sui_desc $s7SwiftUI15LayoutDirectionOMn = { SUI_K_ENUM, "LayoutDirection" };
const int32_t $s7SwiftUI15LayoutDirectionO11rightToLeftyA2CmFWC = 1;
void *$s7SwiftUI15LayoutDirectionOMa(void) { return &$s7SwiftUI15LayoutDirectionOMn; }
int $s7SwiftUI15LayoutDirectionO2eeoiySbAC_ACtFZ(int a, int b) { return (a & 0xFF) == (b & 0xFF); }

/* ControlSize: mini 0, small 1, regular 2, large 3. */
sui_desc $s7SwiftUI11ControlSizeOMn = { SUI_K_ENUM, "ControlSize" };
const int32_t $s7SwiftUI11ControlSizeO5smallyA2CmFWC = 1;
void *$s7SwiftUI11ControlSizeOMa(void) { return &$s7SwiftUI11ControlSizeOMn; }

sui_desc $s7SwiftUI13TextAlignmentOMn = { SUI_K_ENUM, "TextAlignment" };

/* CoordinateSpace: global 0, local 1, named 2. */
static sui_desc sui_coord_desc = { SUI_K_ENUM, "CoordinateSpace" };
const int32_t $s7SwiftUI15CoordinateSpaceO6globalyA2CmFWC = 0;
const int32_t $s7SwiftUI15CoordinateSpaceO5localyA2CmFWC = 1;
const int32_t $s7SwiftUI15CoordinateSpaceO5namedyACs11AnyHashableVcACmFWC = 2;
void *$s7SwiftUI15CoordinateSpaceOMa(void) { return &sui_coord_desc; }

/* RoundedCornerStyle: circular 0, continuous 1. */
static sui_desc sui_corner_desc = { SUI_K_ENUM, "RoundedCornerStyle" };
const int32_t $s7SwiftUI18RoundedCornerStyleO10continuousyA2CmFWC = 1;
void *$s7SwiftUI18RoundedCornerStyleOMa(void) { return &sui_corner_desc; }

/* BlendMode, na ordem da declaração do SwiftUI. */
static sui_desc sui_blend_desc = { SUI_K_ENUM, "BlendMode" };
const int32_t $s7SwiftUI9BlendModeO6normalyA2CmFWC = 0;
const int32_t $s7SwiftUI9BlendModeO9colorBurnyA2CmFWC = 4;
const int32_t $s7SwiftUI9BlendModeO7overlayyA2CmFWC = 8;
const int32_t $s7SwiftUI9BlendModeO14destinationOutyA2CmFWC = 19;
const int32_t $s7SwiftUI9BlendModeO10plusDarkeryA2CmFWC = 20;
const int32_t $s7SwiftUI9BlendModeO11plusLighteryA2CmFWC = 21;
void *$s7SwiftUI9BlendModeOMa(void) { return &sui_blend_desc; }

/* Angle: um Double em radianos; self em r13. */
typedef struct { double radians; } sui_angle;
sui_desc $s7SwiftUI5AngleVMn = { SUI_K_STRUCT, "Angle" };
double $s7SwiftUI5AngleV14animatableDataSdvg(void) {
    sui_angle *a = (sui_angle *)sui_self();
    return a ? a->radians : 0;
}
void $s7SwiftUI5AngleV14animatableDataSdvs(double v) {
    sui_angle *a = (sui_angle *)sui_self();
    if (a) a->radians = v;
}
int $s7SwiftUI5AngleV2eeoiySbAC_ACtFZ(double a, double b) { return a == b; }

/* OptionSets e estruturas de um campo. */
uint64_t $s7SwiftUI15SafeAreaRegionsV3allACvgZ(void) { return 3; }
uint64_t $s7SwiftUI11GestureMaskV3allACvgZ(void) { return 3; }
/* EventModifiers: capsLock 1, shift 2, control 4, option 8, command 16. */
uint64_t $s7SwiftUI14EventModifiersV7commandACvgZ(void) { return 16; }
/* SubmitTriggers: text 1, search 2. */
static sui_desc sui_submit_desc = { SUI_K_STRUCT, "SubmitTriggers" };
uint64_t $s7SwiftUI14SubmitTriggersV4textACvgZ(void) { return 1; }
void *$s7SwiftUI14SubmitTriggersVMa(void) { return &sui_submit_desc; }
/* AccessibilityTraits: isButton é o bit 0. */
static sui_desc sui_traits_desc = { SUI_K_STRUCT, "AccessibilityTraits" };
uint64_t $s7SwiftUI19AccessibilityTraitsV8isButtonACvgZ(void) { return 1; }
void *$s7SwiftUI19AccessibilityTraitsVMa(void) { return &sui_traits_desc; }
/* AccessibilityChildBehavior: ignore 0, contain 1, combine 2. */
static sui_desc sui_child_desc = { SUI_K_STRUCT, "AccessibilityChildBehavior" };
typedef struct { const sui_desc *isa; int64_t code; } sui_code;
static sui_code sui_child_ignore = { &sui_child_desc, 0 };
void *$s7SwiftUI26AccessibilityChildBehaviorV6ignoreACvgZ(void) { return &sui_child_ignore; }
void *$s7SwiftUI26AccessibilityChildBehaviorVMa(void) { return &sui_child_desc; }
/* SearchFieldPlacement: automatic 0. */
static sui_desc sui_search_desc = { SUI_K_STRUCT, "SearchFieldPlacement" };
int64_t $s7SwiftUI20SearchFieldPlacementV9automaticACvgZ(void) { return 0; }
void *$s7SwiftUI20SearchFieldPlacementVMa(void) { return &sui_search_desc; }
/* ToolbarItemPlacement: automatic 0, principal 1, cancellationAction 4. */
static sui_desc sui_toolbar_desc = { SUI_K_STRUCT, "ToolbarItemPlacement" };
int64_t $s7SwiftUI20ToolbarItemPlacementV9principalACvgZ(void) { return 1; }
int64_t $s7SwiftUI20ToolbarItemPlacementV18cancellationActionACvgZ(void) { return 4; }
void *$s7SwiftUI20ToolbarItemPlacementVMa(void) { return &sui_toolbar_desc; }
/* NavigationBarItem.TitleDisplayMode: automatic 0, inline 1, large 2. */
static sui_desc sui_title_desc = { SUI_K_ENUM, "NavigationBarItem.TitleDisplayMode" };
const int32_t $s7SwiftUI17NavigationBarItemV16TitleDisplayModeO6inlineyA2EmFWC = 1;
void *$s7SwiftUI17NavigationBarItemV16TitleDisplayModeOMa(void) { return &sui_title_desc; }
/* PopoverAttachmentAnchor: rect 0, point 1. */
static sui_desc sui_anchor_desc = { SUI_K_ENUM, "PopoverAttachmentAnchor" };
const int32_t $s7SwiftUI23PopoverAttachmentAnchorO4rectyAcA0E0V6SourceVySo6CGRectV_GcACmFWC = 0;
void *$s7SwiftUI23PopoverAttachmentAnchorOMa(void) { return &sui_anchor_desc; }
/* Anchor<CGRect>.Source.bounds. */
static sui_desc sui_source_desc = { SUI_K_STRUCT, "Anchor.Source" };
static sui_code sui_source_bounds = { &sui_source_desc, 0 };
void *$s7SwiftUI6AnchorV6SourceVAASo6CGRectVRszlE6boundsAEyAG_GvgZ(void) { return &sui_source_bounds; }
/* AnimationCompletionCriteria: logicallyComplete 0, removed 1. */
static sui_desc sui_criteria_desc = { SUI_K_STRUCT, "AnimationCompletionCriteria" };
int64_t $s7SwiftUI27AnimationCompletionCriteriaV17logicallyCompleteACvgZ(void) { return 0; }
void *$s7SwiftUI27AnimationCompletionCriteriaVMa(void) { return &sui_criteria_desc; }

/* KeyEquivalent guarda o Character (dois words) sem mudar. */
static sui_desc sui_key_desc = { SUI_K_STRUCT, "KeyEquivalent" };
sui_pair $s7SwiftUI13KeyEquivalentVyACSJcfC(uint64_t w0, uint64_t w1) { sui_pair p; p.w0 = w0; p.w1 = w1; return p; }
void *$s7SwiftUI13KeyEquivalentVMa(void) { return &sui_key_desc; }

/* KeyboardShortcut: tecla e modificadores; defaultAction é Return sem modificador. */
typedef struct { const sui_desc *isa; char key[8]; uint64_t modifiers; } sui_shortcut;
sui_desc $s7SwiftUI16KeyboardShortcutVMn = { SUI_K_STRUCT, "KeyboardShortcut" };
static sui_shortcut sui_default_action = { &$s7SwiftUI16KeyboardShortcutVMn, "\r", 0 };
void *$s7SwiftUI16KeyboardShortcutV13defaultActionACvgZ(void) { return &sui_default_action; }
void *$s7SwiftUI16KeyboardShortcutVMa(void) { return &$s7SwiftUI16KeyboardShortcutVMn; }

/* Enums de Text e Image. */
sui_desc $s7SwiftUI4TextV14TruncationModeOMn = { SUI_K_ENUM, "Text.TruncationMode" };
const int32_t $s7SwiftUI4TextV14TruncationModeO4headyA2EmFWC = 0;
const int32_t $s7SwiftUI4TextV14TruncationModeO4tailyA2EmFWC = 1;
void *$s7SwiftUI4TextV14TruncationModeOMa(void) { return &$s7SwiftUI4TextV14TruncationModeOMn; }
sui_desc $s7SwiftUI4TextV4CaseOMn = { SUI_K_ENUM, "Text.Case" };
const int32_t $s7SwiftUI4TextV4CaseO9uppercaseyA2EmFWC = 0;
void *$s7SwiftUI4TextV4CaseOMa(void) { return &$s7SwiftUI4TextV4CaseOMn; }
/* Text.LineStyle: padrão e cor; single = padrão 1. */
sui_desc $s7SwiftUI4TextV9LineStyleVMn = { SUI_K_STRUCT, "Text.LineStyle" };
int64_t $s7SwiftUI4TextV9LineStyleV6singleAEvgZ(void) { return 1; }
void *$s7SwiftUI4TextV9LineStyleVMa(void) { return &$s7SwiftUI4TextV9LineStyleVMn; }

static sui_desc sui_resize_desc = { SUI_K_ENUM, "Image.ResizingMode" };
const int32_t $s7SwiftUI5ImageV12ResizingModeO4tileyA2EmFWC = 0;
const int32_t $s7SwiftUI5ImageV12ResizingModeO7stretchyA2EmFWC = 1;
void *$s7SwiftUI5ImageV12ResizingModeOMa(void) { return &sui_resize_desc; }
static sui_desc sui_interp_desc = { SUI_K_ENUM, "Image.Interpolation" };
const int32_t $s7SwiftUI5ImageV13InterpolationO4highyA2EmFWC = 3;
void *$s7SwiftUI5ImageV13InterpolationOMa(void) { return &sui_interp_desc; }
sui_desc $s7SwiftUI5ImageV21TemplateRenderingModeOMn = { SUI_K_ENUM, "Image.TemplateRenderingMode" };
const int32_t $s7SwiftUI5ImageV21TemplateRenderingModeO8templateyA2EmFWC = 0;
void *$s7SwiftUI5ImageV21TemplateRenderingModeOMa(void) { return &$s7SwiftUI5ImageV21TemplateRenderingModeOMn; }
sui_desc $s7SwiftUI5ImageV5ScaleOMn = { SUI_K_ENUM, "Image.Scale" };
const int32_t $s7SwiftUI5ImageV5ScaleO5smallyA2EmFWC = 0;
void *$s7SwiftUI5ImageV5ScaleOMa(void) { return &$s7SwiftUI5ImageV5ScaleOMn; }

/* Font.Design: default 0, serif 1, rounded 2, monospaced 3. */
sui_desc $s7SwiftUI4FontV6DesignOMn = { SUI_K_ENUM, "Font.Design" };
const int32_t $s7SwiftUI4FontV6DesignO7defaultyA2EmFWC = 0;
const int32_t $s7SwiftUI4FontV6DesignO10monospacedyA2EmFWC = 3;
void *$s7SwiftUI4FontV6DesignOMa(void) { return &$s7SwiftUI4FontV6DesignOMn; }
/* Font.TextStyle: largeTitle 0 ... body 6 ... caption2 10. */
static sui_desc sui_textstyle_desc = { SUI_K_ENUM, "Font.TextStyle" };
const int32_t $s7SwiftUI4FontV9TextStyleO4bodyyA2EmFWC = 6;
void *$s7SwiftUI4FontV9TextStyleOMa(void) { return &sui_textstyle_desc; }

/* Color.RGBColorSpace: sRGB 0, sRGBLinear 1, displayP3 2. */
static sui_desc sui_space_desc = { SUI_K_ENUM, "Color.RGBColorSpace" };
const int32_t $s7SwiftUI5ColorV13RGBColorSpaceO4sRGByA2EmFWC = 0;
const int32_t $s7SwiftUI5ColorV13RGBColorSpaceO9displayP3yA2EmFWC = 2;
void *$s7SwiftUI5ColorV13RGBColorSpaceOMa(void) { return &sui_space_desc; }

/* SymbolRenderingMode: monochrome 0, multicolor 1, hierarchical 2, palette 3. */
sui_desc $s7SwiftUI19SymbolRenderingModeVMn = { SUI_K_STRUCT, "SymbolRenderingMode" };
static sui_code sui_sym_hier = { &$s7SwiftUI19SymbolRenderingModeVMn, 2 };
void *$s7SwiftUI19SymbolRenderingModeV12hierarchicalACvgZ(void) { return &sui_sym_hier; }
void *$s7SwiftUI19SymbolRenderingModeVMa(void) { return &$s7SwiftUI19SymbolRenderingModeVMn; }
