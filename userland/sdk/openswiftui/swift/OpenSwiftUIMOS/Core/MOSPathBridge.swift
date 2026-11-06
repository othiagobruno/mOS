// mOS — Path <-> caminho C da ponte (MOSBridge_path.h), usado pelo renderer e por Path.strokedPath
// (entra no módulo OpenSwiftUICore).
package import Foundation
package import OpenCoreGraphicsShims
import MOSBridge

package enum MOSPathBridge {
    /// Caminho C com os elementos de `path` e a afim local -> view `t`. Libere com mos_path_destroy.
    package static func make(_ path: Path, transform t: CGAffineTransform = .identity) -> UnsafeMutableRawPointer? {
        guard let p = mos_path_create() else { return nil }
        feed(p, path)
        mos_path_set_transform(p, Double(t.a), Double(t.b), Double(t.c), Double(t.d), Double(t.tx), Double(t.ty))
        return p
    }

    /// Usa o caminho C dentro de `body` e o libera.
    package static func with<R>(_ path: Path, transform t: CGAffineTransform = .identity, _ body: (UnsafeMutableRawPointer) -> R) -> R? {
        guard let p = make(path, transform: t) else { return nil }
        defer { mos_path_destroy(p) }
        return body(p)
    }

    static func feed(_ p: UnsafeMutableRawPointer, _ path: Path) {
        switch path.storage {
        case .empty:
            break
        case let .rect(r):
            let r = r.standardized
            mos_path_rect(p, Double(r.minX), Double(r.minY), Double(r.width), Double(r.height))
        case let .ellipse(r):
            let r = r.standardized
            mos_path_ellipse(p, Double(r.minX), Double(r.minY), Double(r.width), Double(r.height))
        case let .roundedRect(rr) where !rr.needsContinuousCorners:
            let r = rr.rect.standardized, c = rr.clampedCornerSize
            mos_path_rrect(p, Double(r.minX), Double(r.minY), Double(r.width), Double(r.height), Double(c.width), Double(c.height))
        default:
            feed(p, path.mosElements)
        }
    }

    static func feed(_ p: UnsafeMutableRawPointer, _ elements: [Path.Element]) {
        for e in elements {
            switch e {
            case let .move(to: q): mos_path_move(p, Double(q.x), Double(q.y))
            case let .line(to: q): mos_path_line(p, Double(q.x), Double(q.y))
            case let .quadCurve(to: q, control: c): mos_path_quad(p, Double(c.x), Double(c.y), Double(q.x), Double(q.y))
            case let .curve(to: q, control1: c1, control2: c2):
                mos_path_curve(p, Double(c1.x), Double(c1.y), Double(c2.x), Double(c2.y), Double(q.x), Double(q.y))
            case .closeSubpath: mos_path_close(p)
            }
        }
    }

    /// Elementos de um caminho C (inverso de feed).
    static func elements(of p: UnsafeMutableRawPointer) -> [Path.Element] {
        let n = Int(mos_path_element_count(p))
        var out: [Path.Element] = []
        out.reserveCapacity(n)
        var v = [Double](repeating: 0, count: 6)
        for i in 0..<n {
            let op = v.withUnsafeMutableBufferPointer { mos_path_element(p, Int32(i), $0.baseAddress) }
            func pt(_ k: Int) -> CGPoint { CGPoint(x: CGFloat(v[k]), y: CGFloat(v[k + 1])) }
            switch op {
            case 0: out.append(.move(to: pt(0)))
            case 1: out.append(.line(to: pt(0)))
            case 2: out.append(.quadCurve(to: pt(2), control: pt(0)))
            case 3: out.append(.curve(to: pt(4), control1: pt(0), control2: pt(2)))
            case 4: out.append(.closeSubpath)
            default: break
            }
        }
        return out
    }

    /// Chama `body` com o tracejado do estilo como buffer C (vazio = sem tracejado).
    static func withDash<R>(_ style: StrokeStyle, _ body: (UnsafePointer<Double>?, Int32) -> R) -> R {
        let dash = style.dash.map { Double($0) }
        if dash.isEmpty { return body(nil, 0) }
        return dash.withUnsafeBufferPointer { body($0.baseAddress, Int32(dash.count)) }
    }

    /// Path.strokedPath: contorno do traço como anéis fechados (preenchimento não-zero).
    package static func strokeOutline(_ path: Path, style: StrokeStyle) -> [Path.Element] {
        guard style.lineWidth > 0, !path.isEmpty,
              let src = make(path), let dst = mos_path_create() else { return [] }
        defer { mos_path_destroy(src); mos_path_destroy(dst) }
        withDash(style) { dash, n in
            _ = mos_path_stroke_outline(src, dst, Double(style.lineWidth), style.lineCap.rawValue, style.lineJoin.rawValue,
                                        Double(style.miterLimit), dash, n, Double(style.dashPhase))
        }
        return elements(of: dst)
    }

    /// Traça `path` (local) com a afim `t`, na cor dada.
    package static func stroke(_ path: Path, style: StrokeStyle, transform t: CGAffineTransform,
                               red: Double, green: Double, blue: Double, alpha: Double) {
        _ = with(path, transform: t) { p in
            withDash(style) { dash, n in
                mos_path_stroke(p, Double(style.lineWidth), style.lineCap.rawValue, style.lineJoin.rawValue, Double(style.miterLimit),
                                dash, n, Double(style.dashPhase), red, green, blue, alpha)
            }
        }
    }
}
