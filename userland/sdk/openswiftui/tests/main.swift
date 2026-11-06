// teste de execução das dependências do OpenSwiftUI no mOS (Swift completo)
import Foundation
import OpenCoreGraphics
import OpenQuartzCore
import OpenObservation
import OpenAttributeGraphShims   // motor: Compute

var fails = 0
func check(_ ok: Bool, _ what: String) {
    if ok { print("  ok: \(what)") } else { print("  FALHOU: \(what)"); fails += 1 }
}

print("== Foundation")
let r = CGRect(x: 0, y: 0, width: 10, height: 10)
check(r.union(CGRect(x: 5, y: 5, width: 10, height: 10)) == CGRect(x: 0, y: 0, width: 15, height: 15), "CGRect.union")
check(r.intersection(CGRect(x: 20, y: 20, width: 1, height: 1)).isNull, "intersection vazia = null")
check(r.insetBy(dx: 2, dy: 3) == CGRect(x: 2, y: 3, width: 6, height: 4), "insetBy")
let half: CGFloat = 0.5
let d: Double = half * 4          // CGFloat -> Double implícito (SE-0307)
check(d == 2, "conversão implícita CGFloat -> Double")
check(sqrt(CGFloat(16)) == 4 && abs(sin(CGFloat.pi / 2) - 1) < 1e-12, "math em CGFloat (MOSLibm)")
check(Set([CGPoint(x: 1, y: 2), CGPoint(x: 1, y: 2)]).count == 1, "CGPoint Hashable")
check(CFEqual(CFStringCreateWithCString(nil, "abc", CFStringBuiltInEncodings.UTF8.rawValue), "abc".cfString), "CFEqual por conteúdo (CFString nativo do Swift)")
check(String("olá, mOS".cfString) == "olá, mOS", "String <-> CFString (UTF-8)")
let dict: [CFString: Any] = ["peso".cfString: 0.3, "nome".cfString: "Inter"]
let back = [CFString: Any](dict.cfDictionary) ?? [:]
check((back["peso".cfString] as? Double) == 0.3 && (back["nome".cfString] as? String) == "Inter", "[CFString: Any] <-> CFDictionary")
check("\(CGFloat(1.5))" == "1.5", "CGFloat description")
do {   // objetos CF dentro de Any (cópia pela value witness table da metadata CF)
    let s = "abc".cfString
    let a: Any = s as AnyObject
    let b = a
    var box: [CFString: Any] = [:]
    box["k".cfString] = b
    let arr: [Any] = [b, box]
    check(arr.count == 2 && CFEqual((box["k".cfString]! as AnyObject), s), "CF dentro de Any/[Any]/[CFString: Any]")
}

print("== NSAttributedString")
let as1 = NSMutableAttributedString(string: "olá mundo")
as1.addAttribute(.kern, value: NSNumber(value: 1.5), range: NSRange(location: 4, length: 5))
var er = NSRange()
let k = as1.attribute(.kern, at: 5, effectiveRange: &er) as? NSNumber
check(as1.length == 9 && k?.doubleValue == 1.5 && er == NSRange(location: 4, length: 5), "addAttribute + attribute(_:at:effectiveRange:)")
var pieces = 0
as1.enumerateAttributes(in: NSRange(location: 0, length: as1.length)) { _, _, _ in pieces += 1 }
check(pieces == 2 && as1.attributedSubstring(from: NSRange(location: 4, length: 5)).string == "mundo", "enumerateAttributes / attributedSubstring")
as1.replaceCharacters(in: NSRange(location: 0, length: 3), with: "oi")
check(as1.string == "oi mundo" && (as1.attribute(.kern, at: 3, effectiveRange: nil) as? NSNumber)?.doubleValue == 1.5, "replaceCharacters desloca os atributos")
check("a\nb\r\nc".components(separatedBy: .newlines).count == 4 && "x y".rangeOfCharacter(from: .whitespaces) != nil, "CharacterSet / components / rangeOfCharacter")

print("== OpenCoreGraphics")
let t = CGAffineTransform(translationX: 10, y: 20).scaledBy(x: 2, y: 3)
check(CGPoint(x: 1, y: 1).applying(t) == CGPoint(x: 12, y: 23), "CGAffineTransform translate+scale")
let inv = t.inverted().concatenating(t)
check(abs(inv.a - 1) < 1e-12 && abs(inv.tx) < 1e-12, "inverted")
let path = CGMutablePath()
path.addEllipse(in: CGRect(x: 0, y: 0, width: 10, height: 10))
check(path.elements.count == 5 && !path.isEmpty, "CGPath.addEllipse")
let red = CGColor(srgbRed: 1, green: 0, blue: 0, alpha: 1)
check(red == CGColor(srgbRed: 1, green: 0, blue: 0, alpha: 1), "CGColor isEqual (NSObject)")
if let space = CGColorSpace(name: CGColorSpace.linearSRGB), let lin = red.converted(to: space, intent: .defaultIntent, options: nil) {
    check(abs(lin.components![0] - 1) < 1e-9, "CGColor sRGB -> linear")
} else { check(false, "CGColorSpace(name:)") }
let gray = CGColor(gray: 0.5, alpha: 1)
check(gray.numberOfComponents == 2, "CGColor cinza")
let layer = CALayer()
layer.contentsScale = 2
check(layer == layer && layer.contentsScale == 2, "CALayer")

print("== OpenObservation")
final class Modelo {
    let registrar = ObservationRegistrar()
    var _valor = 0
    var valor: Int {
        get { registrar.access(self, keyPath: \.valor); return _valor }
        set { registrar.withMutation(of: self, keyPath: \.valor) { _valor = newValue } }
    }
}
extension Modelo: Observable {}
let m = Modelo()
var mudou = 0
withObservationTracking { _ = m.valor } onChange: { mudou += 1 }
m.valor = 1
m.valor = 2              // só a 1ª mudança dispara (rastreamento é de uma vez)
check(mudou == 1 && m.valor == 2, "withObservationTracking dispara uma vez")

print("== OpenAttributeGraphShims sobre o Compute (Swift)")
struct Dobro: Rule {
    typealias Value = Int
    @Attribute var x: Int
    var value: Int { x * 2 }
}
do {
    let g = Graph()
    let sg = Subgraph(graph: g)
    Subgraph.current = sg
    let a = Attribute(value: 21)
    let d = Attribute(Dobro(x: a))
    check(d.wrappedValue == 42, "Rule calcula o atributo derivado (21 * 2)")
    a.wrappedValue = 50
    check(d.wrappedValue == 100, "mudança na entrada invalida e recalcula (50 * 2)")
    Subgraph.current = nil
    sg.invalidate()
    g.invalidate()
}
check(true, "Graph/Subgraph soltos pelo ARC do Swift sem travar")

print("== OAG / ORB (C++)")
@_silgen_name("deps_c_selftest") func deps_c_selftest() -> Int32
fails += Int(deps_c_selftest())

print(fails == 0 ? "TUDO OK" : "\(fails) FALHA(S)")
