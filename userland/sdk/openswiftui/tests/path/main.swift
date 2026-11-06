// Teste da camada de Path/DisplayList do SwiftUI do mOS (tools/test-openswiftui-path.ps1):
//  1. geometria em Swift (MOSPathGeometry): contém não-zero/par-ímpar, arcos, trim, descrição/parse;
//  2. contorno do traço em C (mos_path_stroke_outline): largura, terminações, junções, tracejado;
//  3. cena desenhada num canvas do AppKit do mOS: traço, par-ímpar, recorte por retângulo, por caminho,
//     invertido, máscara e sombra. Com MOS_PATH_TRACE/MOS_PATH_RASTER o C registra a área coberta.
import Foundation
import OpenCoreGraphics
import MOSBridge

nonisolated(unsafe) var fails = 0
func check(_ ok: Bool, _ what: String) {
    if ok { print("  ok: \(what)") } else { print("  FALHOU: \(what)"); fails += 1 }
}
func near(_ a: CGFloat, _ b: CGFloat, _ eps: CGFloat = 0.01) -> Bool { abs(a - b) <= eps }

func geometryTests() {
print("== geometria (Swift)")
var sq: [Path.Element] = []
MOSPathGeometry.appendRect(CGRect(x: 0, y: 0, width: 10, height: 10), to: &sq)
MOSPathGeometry.appendRect(CGRect(x: 2, y: 2, width: 6, height: 6), to: &sq)
check(MOSPathGeometry.contains(sq, point: CGPoint(x: 5, y: 5), eoFill: false), "não-zero: centro dentro (anéis no mesmo sentido)")
check(!MOSPathGeometry.contains(sq, point: CGPoint(x: 5, y: 5), eoFill: true), "par-ímpar: centro é buraco")
check(MOSPathGeometry.contains(sq, point: CGPoint(x: 1, y: 1), eoFill: true), "par-ímpar: borda dentro")
check(!MOSPathGeometry.contains(sq, point: CGPoint(x: 11, y: 5), eoFill: false), "fora")
var arc: [Path.Element] = []
MOSPathGeometry.appendArc(center: .zero, radius: 10, start: 0, end: .pi, clockwise: false, to: &arc)
let ab = MOSPathGeometry.boundingRect(arc)
check(near(ab.minX, -10) && near(ab.maxX, 10) && near(ab.minY, 0) && near(ab.maxY, 10, 0.05), "arco 0..π anti-horário: \(ab)")
var arc2: [Path.Element] = []
MOSPathGeometry.appendArc(center: .zero, radius: 10, start: 0, end: .pi, clockwise: true, to: &arc2)
check(near(MOSPathGeometry.boundingRect(arc2).minY, -10, 0.05), "arco 0..π horário passa por y = -10")
// (literal constante mutado derruba o runtime do mOS: swift_isUniquelyReferenced num objeto estático)
var tg: [Path.Element] = []
tg.append(.move(to: CGPoint(x: 0, y: 0)))
MOSPathGeometry.appendArc(tangent1End: CGPoint(x: 10, y: 0), tangent2End: CGPoint(x: 10, y: 10), radius: 4, to: &tg)
if let e = MOSPathGeometry.currentPoint(tg) { check(near(e.x, 10) && near(e.y, 4), "arco tangente termina em (10, 4): \(e)") } else { check(false, "arco tangente") }
var ln: [Path.Element] = [.move(to: .zero), .line(to: CGPoint(x: 100, y: 0))]
let tr = MOSPathGeometry.trimmed(ln, from: 0.25, to: 0.5)
check(tr == [.move(to: CGPoint(x: 25, y: 0)), .line(to: CGPoint(x: 50, y: 0))], "trimmedPath de reta: \(tr)")
var circ: [Path.Element] = []
MOSPathGeometry.appendEllipse(CGRect(x: -10, y: -10, width: 20, height: 20), to: &circ)
let half = MOSPathGeometry.trimmed(circ, from: 0, to: 0.5)
let hb = MOSPathGeometry.boundingRect(half)
check(near(hb.minY, 0, 0.3) && near(hb.maxY, 10, 0.05), "trimmedPath de meio círculo: \(hb)")
let desc = MOSPathGeometry.describe([.move(to: CGPoint(x: 1, y: 2)), .line(to: CGPoint(x: 3.5, y: 4)), .curve(to: CGPoint(x: 9, y: 9), control1: CGPoint(x: 5, y: 6), control2: CGPoint(x: 7, y: 8)), .closeSubpath])
check(desc == "1 2 m 3.5 4 l 5 6 7 8 9 9 c h", "descrição: \(desc)")
check(MOSPathGeometry.parse(desc).map(MOSPathGeometry.describe) == desc, "parse da descrição")
check(MOSPathGeometry.parse("1 2 x") == nil, "parse inválido devolve nil")
check(MOSPathGeometry.roundedRectContains(CGRect(x: 0, y: 0, width: 100, height: 50), cornerSize: CGSize(width: 20, height: 20), point: CGPoint(x: 50, y: 25)), "retângulo arredondado: centro")
check(!MOSPathGeometry.roundedRectContains(CGRect(x: 0, y: 0, width: 100, height: 50), cornerSize: CGSize(width: 20, height: 20), point: CGPoint(x: 1, y: 1)), "retângulo arredondado: canto fora")
var rr: [Path.Element] = []
MOSPathGeometry.appendUnevenRoundedRect(CGRect(x: 0, y: 0, width: 100, height: 50), topLeft: .zero, topRight: CGSize(width: 20, height: 20),
                                        bottomRight: .zero, bottomLeft: CGSize(width: 10, height: 10), continuous: false, to: &rr)
check(MOSPathGeometry.contains(rr, point: CGPoint(x: 0.5, y: 0.5), eoFill: false) && !MOSPathGeometry.contains(rr, point: CGPoint(x: 99, y: 1), eoFill: false),
      "cantos desiguais: quina reta dentro, arredondada fora")
check(MOSPathGeometry.elements(of: MOSPathGeometry.cgPath(rr)) == rr, "ida e volta pelo CGPath do OpenCoreGraphics")

}
geometryTests()

func outlineTests() {
print("== contorno do traço (C)")
func outline(_ pts: [(Double, Double)], closed: Bool = false, width: Double, cap: Int32 = 0, join: Int32 = 0, dash: [Double] = []) -> UnsafeMutableRawPointer {
    let src = mos_path_create()!, dst = mos_path_create()!
    mos_path_move(src, pts[0].0, pts[0].1)
    for p in pts.dropFirst() { mos_path_line(src, p.0, p.1) }
    if closed { mos_path_close(src) }
    let rings = dash.withUnsafeBufferPointer { mos_path_stroke_outline(src, dst, width, cap, join, 10, dash.isEmpty ? nil : $0.baseAddress, Int32(dash.count), 0) }
    mos_path_destroy(src)
    print("    (\(rings) anéis)")
    return dst
}
func inside(_ p: UnsafeMutableRawPointer, _ x: Double, _ y: Double) -> Bool { mos_path_contains(p, x, y, 0) != 0 }
var o = outline([(0, 0), (100, 0)], width: 10)
check(inside(o, 50, 4.5) && !inside(o, 50, 5.5) && !inside(o, 101, 0), "reta largura 10, butt")
mos_path_destroy(o)
o = outline([(0, 0), (100, 0)], width: 10, cap: 1)
check(inside(o, 104, 0) && !inside(o, 104, 4), "terminação redonda")
mos_path_destroy(o)
o = outline([(0, 0), (100, 0)], width: 10, cap: 2)
check(inside(o, 104, 4) && inside(o, -4, -4), "terminação quadrada")
mos_path_destroy(o)
o = outline([(0, 0), (100, 0), (100, 100)], width: 10, join: 0)
check(inside(o, 104.5, -4.5), "junção miter cobre a quina")
mos_path_destroy(o)
o = outline([(0, 0), (100, 0), (100, 100)], width: 10, join: 1)
check(inside(o, 103.4, -3.4) && !inside(o, 104.5, -4.5), "junção redonda")
mos_path_destroy(o)
o = outline([(0, 0), (100, 0), (100, 100)], width: 10, join: 2)
check(inside(o, 101.5, -1.5) && !inside(o, 104, -4), "junção chanfrada")
mos_path_destroy(o)
o = outline([(0, 0), (100, 0)], width: 4, dash: [10, 10])
check(inside(o, 5, 0) && !inside(o, 15, 0) && inside(o, 25, 0) && !inside(o, 95, 0), "tracejado [10, 10]")
mos_path_destroy(o)
o = outline([(0, 0), (50, 0), (50, 50), (0, 50)], closed: true, width: 6)
check(inside(o, 0, 25) && inside(o, -2.9, 25) && !inside(o, 25, 25), "contorno fechado (sem terminações, junção no início)")
mos_path_destroy(o)

}
outlineTests()

print("== cena no canvas (traço e recorte)")
nonisolated(unsafe) var canvas: UnsafeMutableRawPointer? = nil
nonisolated(unsafe) var drawn = 0
let drawScene: @convention(c) (UnsafeMutableRawPointer?, Double, Double) -> Void = { _, w, h in
    drawn += 1
    if drawn > 1 { return }   // uma vez basta para o registro
    mos_log("-- canvas \(Int(w))x\(Int(h))")
    mos_path_trace(3)   // registro + rasterização própria (mede a área coberta)
    mos_clip_reset()
    let p = mos_path_create()!
    // 1. traço: reta 100 x 6 (área 600) e polilinha com junções redondas
    mos_log("-- 1. traço reto largura 6 (esperado 600 px)")
    mos_path_move(p, 20, 20); mos_path_line(p, 120, 20)
    mos_path_stroke(p, 6, 0, 0, 10, nil, 0, 0, 0.9, 0.1, 0.1, 1)
    mos_log("-- 1b. polilinha tracejada, junção redonda")
    mos_path_clear(p)
    mos_path_move(p, 20, 40); mos_path_line(p, 120, 40); mos_path_line(p, 120, 90)
    let dash: [Double] = [8, 4]
    dash.withUnsafeBufferPointer { mos_path_stroke(p, 3, 1, 1, 10, $0.baseAddress, 2, 0, 0.1, 0.2, 0.9, 1) }
    // 2. par-ímpar: quadrado 100 com furo 50 (esperado 7500 px)
    mos_log("-- 2. par-ímpar (esperado 7500 px)")
    mos_path_clear(p)
    mos_path_rect(p, 150, 10, 100, 100); mos_path_rect(p, 175, 35, 50, 50)
    mos_path_fill(p, 1, 0.2, 0.7, 0.3, 1)
    // 3. recorte retangular 100 x 50 sobre um preenchimento do canvas inteiro (esperado 5000 px)
    mos_log("-- 3. clip retângulo 100x50 (esperado 5000 px)")
    mos_clip_push_rect(20, 150, 100, 50)
    mos_fill_rect(0, 0, w, h, 0, 1, 0.6, 0, 1)
    mos_clip_pop()
    // 4. recorte por caminho: círculo de raio 40 (esperado π·40² ≈ 5027 px)
    mos_log("-- 4. clip círculo r=40 (esperado ~5027 px)")
    mos_path_clear(p)
    mos_path_ellipse(p, 260, 140, 80, 80)
    mos_clip_push_path(p, 0, 0)
    mos_fill_rect(0, 0, w, h, 0, 0.5, 0, 0.8, 1)
    // 4b. recorte aninhado: retângulo dentro do círculo (metade de baixo: ~2513 px)
    mos_log("-- 4b. clip aninhado círculo ∩ metade de baixo (esperado ~2513 px)")
    mos_clip_push_rect(0, 180, w, h)
    mos_fill_rect(0, 0, w, h, 0, 0, 0, 0, 1)
    mos_clip_pop()
    mos_clip_pop()
    // 5. recorte invertido: quadrado 60 sem o círculo de raio 20 (esperado 3600 − 1257 ≈ 2343 px)
    mos_log("-- 5. clip invertido (esperado ~2343 px)")
    mos_clip_push_rect(20, 220, 60, 60)
    mos_path_clear(p)
    mos_path_ellipse(p, 30, 230, 40, 40)
    mos_clip_push_path(p, 0, 1)
    mos_fill_rect(0, 0, w, h, 0, 0.3, 0.3, 0.3, 1)
    mos_clip_pop(); mos_clip_pop()
    // 6. máscara: elipse 80 x 40 como máscara de um retângulo (esperado π·40·20 ≈ 2513 px)
    mos_log("-- 6. máscara elíptica (esperado ~2513 px)")
    mos_mask_begin()
    mos_path_clear(p)
    mos_path_ellipse(p, 100, 230, 80, 40)
    mos_path_fill(p, 0, 1, 1, 1, 1)
    mos_mask_end_clip(0)
    mos_fill_rect(0, 0, w, h, 0, 0.8, 0.2, 0.6, 1)
    mos_clip_pop()
    // 7. sombra de um retângulo arredondado
    mos_log("-- 7. sombra raio 6 deslocada (0, 4)")
    mos_path_clear(p)
    mos_path_rrect(p, 200, 230, 60, 40, 8, 8)
    mos_path_fill_shadow(p, 0, 6, 0, 4, 0, 0, 0, 0.4)
    mos_path_fill(p, 0, 1, 1, 1, 1)
    // 8. caminho rápido: sem rasterização própria, traço e forma de vários anéis num GC_POLY do kernel
    mos_log("-- 8. traço e par de anéis pelo GC_POLY do kernel")
    mos_path_trace(1)
    mos_path_clear(p)
    mos_path_move(p, 300, 240); mos_path_line(p, 380, 240); mos_path_line(p, 340, 290); mos_path_close(p)
    mos_path_stroke(p, 4, 0, 0, 10, nil, 0, 0, 0, 0, 0, 1)
    mos_path_clear(p)
    mos_path_rect(p, 300, 10, 30, 30); mos_path_ellipse(p, 340, 10, 30, 30)
    mos_path_fill(p, 0, 0.9, 0.5, 0.1, 1)
    mos_path_destroy(p)
    mos_clip_reset()
    mos_log("-- profundidade final do recorte: \(mos_clip_depth())")
    mos_path_trace(0)
}
let render: @convention(c) (UnsafeMutableRawPointer?, UnsafeMutableRawPointer?, Double, Double) -> Void = { _, content, w, h in
    if canvas == nil {
        canvas = mos_canvas(drawScene, nil)
        mos_add_subview(content, canvas)
    }
    mos_set_frame(canvas, 0, 0, w, h)
    mos_set_needs_display(canvas)
}
print(fails == 0 ? "geometria e contorno: TUDO OK" : "geometria e contorno: \(fails) falha(s)")
mos_run_app("Path", 400, 300, render, nil)
