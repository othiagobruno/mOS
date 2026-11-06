// mOS — geometria de Path em Swift puro para o caminho não-Darwin (entra no módulo OpenSwiftUICore).
// No mOS o Path guarda os elementos num CGPath do OpenCoreGraphics (classe Swift com `elements`); o
// OpenRenderBox não tem ORBPath fora do Darwin. Aqui ficam as operações que o upstream delega ao
// CoreGraphics/RenderBox: retângulos arredondados e arcos como Béziers, limites, ponto dentro (não-zero
// e par-ímpar), comprimento e recorte (trimmedPath), descrição textual no formato do CGPath e o parse dela.
// O traço (strokedPath) usa o gerador de contorno em C (mos_path_stroke_outline) via MOSPathBridge.
package import Foundation
package import OpenCoreGraphicsShims

package enum MOSPathGeometry {
    /// 4 (√2 − 1) / 3: controle de Bézier cúbica para um quarto de círculo.
    package static let kappa: CGFloat = 0.5522847498307936

    // MARK: - conversão CGPath (OpenCoreGraphics) <-> Path.Element

    package static func cgPath(_ elements: [Path.Element]) -> CGMutablePath {
        let p = CGMutablePath()
        p.elements.reserveCapacity(elements.count)
        for e in elements { p.elements.append(cgElement(e)) }
        return p
    }

    package static func cgElement(_ e: Path.Element) -> CGPath.Element {
        switch e {
        case let .move(to: p): return .moveToPoint(p)
        case let .line(to: p): return .addLineToPoint(p)
        case let .quadCurve(to: p, control: c): return .addQuadCurveToPoint(c, p)
        case let .curve(to: p, control1: c1, control2: c2): return .addCurveToPoint(c1, c2, p)
        case .closeSubpath: return .closeSubpath
        }
    }

    package static func element(_ e: CGPath.Element) -> Path.Element {
        switch e {
        case let .moveToPoint(p): return .move(to: p)
        case let .addLineToPoint(p): return .line(to: p)
        case let .addQuadCurveToPoint(c, p): return .quadCurve(to: p, control: c)
        case let .addCurveToPoint(c1, c2, p): return .curve(to: p, control1: c1, control2: c2)
        case .closeSubpath: return .closeSubpath
        }
    }

    package static func elements(of path: CGPath) -> [Path.Element] { path.elements.map(element) }

    // MARK: - formas

    package static func appendRect(_ r: CGRect, transform t: CGAffineTransform = .identity, to out: inout [Path.Element]) {
        let r = r.standardized
        out.append(.move(to: CGPoint(x: r.minX, y: r.minY).applying(t)))
        out.append(.line(to: CGPoint(x: r.maxX, y: r.minY).applying(t)))
        out.append(.line(to: CGPoint(x: r.maxX, y: r.maxY).applying(t)))
        out.append(.line(to: CGPoint(x: r.minX, y: r.maxY).applying(t)))
        out.append(.closeSubpath)
    }

    /// Elipse inscrita, começando em (maxX, midY), na mesma orientação do retângulo.
    package static func appendEllipse(_ r: CGRect, transform t: CGAffineTransform = .identity, to out: inout [Path.Element]) {
        let r = r.standardized
        let rx = r.width / 2, ry = r.height / 2, cx = r.midX, cy = r.midY
        let kx = rx * kappa, ky = ry * kappa
        func p(_ x: CGFloat, _ y: CGFloat) -> CGPoint { CGPoint(x: x, y: y).applying(t) }
        out.append(.move(to: p(cx + rx, cy)))
        out.append(.curve(to: p(cx, cy + ry), control1: p(cx + rx, cy + ky), control2: p(cx + kx, cy + ry)))
        out.append(.curve(to: p(cx - rx, cy), control1: p(cx - kx, cy + ry), control2: p(cx - rx, cy + ky)))
        out.append(.curve(to: p(cx, cy - ry), control1: p(cx - rx, cy - ky), control2: p(cx - kx, cy - ry)))
        out.append(.curve(to: p(cx + rx, cy), control1: p(cx + kx, cy - ry), control2: p(cx + rx, cy - ky)))
        out.append(.closeSubpath)
    }

    /// Retângulo arredondado com o mesmo raio (rx, ry) nos quatro cantos. `continuous` aproxima a curvatura
    /// contínua do SwiftUI estendendo o canto (~1,28 × raio) com controles mais suaves.
    package static func appendRoundedRect(_ r: CGRect, cornerSize: CGSize, continuous: Bool,
                                          transform t: CGAffineTransform = .identity, to out: inout [Path.Element]) {
        let s = CGSize(width: max(cornerSize.width, 0), height: max(cornerSize.height, 0))
        appendUnevenRoundedRect(r, topLeft: s, topRight: s, bottomRight: s, bottomLeft: s, continuous: continuous, transform: t, to: &out)
    }

    package static func appendUnevenRoundedRect(_ r: CGRect, topLeft: CGSize, topRight: CGSize, bottomRight: CGSize, bottomLeft: CGSize,
                                                continuous: Bool, transform t: CGAffineTransform = .identity, to out: inout [Path.Element]) {
        let r = r.standardized
        guard !r.isNull, !r.isInfinite else { return }
        let hw = r.width / 2, hh = r.height / 2
        func clamp(_ c: CGSize) -> CGSize { CGSize(width: min(max(c.width, 0), hw), height: min(max(c.height, 0), hh)) }
        let tl = clamp(topLeft), tr = clamp(topRight), br = clamp(bottomRight), bl = clamp(bottomLeft)
        if tl == .zero && tr == .zero && br == .zero && bl == .zero { appendRect(r, transform: t, to: &out); return }
        func p(_ x: CGFloat, _ y: CGFloat) -> CGPoint { CGPoint(x: x, y: y).applying(t) }
        // extensão do canto e fator do controle: circular = (1, kappa); contínuo ≈ (1.28, 0.64)
        let ext: CGFloat = continuous ? 1.28 : 1, k: CGFloat = continuous ? 0.64 : kappa
        func e(_ c: CGSize, _ half: CGFloat, w: Bool) -> CGFloat { min((w ? c.width : c.height) * ext, half) }
        // topo: da esquerda para a direita
        out.append(.move(to: p(r.minX + e(tl, hw, w: true), r.minY)))
        out.append(.line(to: p(r.maxX - e(tr, hw, w: true), r.minY)))
        if tr != .zero {
            let ex = e(tr, hw, w: true), ey = e(tr, hh, w: false)
            out.append(.curve(to: p(r.maxX, r.minY + ey), control1: p(r.maxX - ex * (1 - k), r.minY), control2: p(r.maxX, r.minY + ey * (1 - k))))
        }
        out.append(.line(to: p(r.maxX, r.maxY - e(br, hh, w: false))))
        if br != .zero {
            let ex = e(br, hw, w: true), ey = e(br, hh, w: false)
            out.append(.curve(to: p(r.maxX - ex, r.maxY), control1: p(r.maxX, r.maxY - ey * (1 - k)), control2: p(r.maxX - ex * (1 - k), r.maxY)))
        }
        out.append(.line(to: p(r.minX + e(bl, hw, w: true), r.maxY)))
        if bl != .zero {
            let ex = e(bl, hw, w: true), ey = e(bl, hh, w: false)
            out.append(.curve(to: p(r.minX, r.maxY - ey), control1: p(r.minX + ex * (1 - k), r.maxY), control2: p(r.minX, r.maxY - ey * (1 - k))))
        }
        out.append(.line(to: p(r.minX, r.minY + e(tl, hh, w: false))))
        if tl != .zero {
            let ex = e(tl, hw, w: true), ey = e(tl, hh, w: false)
            out.append(.curve(to: p(r.minX + ex, r.minY), control1: p(r.minX, r.minY + ey * (1 - k)), control2: p(r.minX + ex * (1 - k), r.minY)))
        }
        out.append(.closeSubpath)
    }

    package static func roundedRectContains(_ r: CGRect, cornerSize c: CGSize, point q: CGPoint) -> Bool {
        guard r.contains(q) else { return false }
        let rx = min(c.width, r.width / 2), ry = min(c.height, r.height / 2)
        guard rx > 0, ry > 0 else { return true }
        let cx = q.x < r.minX + rx ? r.minX + rx : q.x > r.maxX - rx ? r.maxX - rx : q.x
        let cy = q.y < r.minY + ry ? r.minY + ry : q.y > r.maxY - ry ? r.maxY - ry : q.y
        let dx = (q.x - cx) / rx, dy = (q.y - cy) / ry
        return dx * dx + dy * dy <= 1
    }

    // MARK: - arcos (semântica do CGPathAddArc: ângulos em radianos, `clockwise` no sentido decrescente)

    /// Arco de `start` varrendo `delta` radianos; liga ao ponto atual com uma reta ou começa um subcaminho.
    package static func appendArc(center: CGPoint, radius: CGFloat, start: CGFloat, delta: CGFloat,
                                  transform t: CGAffineTransform = .identity, to out: inout [Path.Element]) {
        let r = abs(radius)
        let p0 = CGPoint(x: center.x + r * cos(start), y: center.y + r * sin(start)).applying(t)
        if currentPoint(out) == nil { out.append(.move(to: p0)) } else { out.append(.line(to: p0)) }
        guard r > 0, delta != 0 else { return }
        let n = max(1, Int((abs(delta) / (CGFloat.pi / 2)).rounded(.up)))
        let step = delta / CGFloat(n)
        let h = 4 / 3 * tan(step / 4)
        var a = start
        for _ in 0..<n {
            let b = a + step
            let ca = cos(a), sa = sin(a), cb = cos(b), sb = sin(b)
            let c1 = CGPoint(x: center.x + r * (ca - h * sa), y: center.y + r * (sa + h * ca)).applying(t)
            let c2 = CGPoint(x: center.x + r * (cb + h * sb), y: center.y + r * (sb - h * cb)).applying(t)
            let e = CGPoint(x: center.x + r * cb, y: center.y + r * sb).applying(t)
            out.append(.curve(to: e, control1: c1, control2: c2))
            a = b
        }
    }

    package static func appendArc(center: CGPoint, radius: CGFloat, start: CGFloat, end: CGFloat, clockwise: Bool,
                                  transform t: CGAffineTransform = .identity, to out: inout [Path.Element]) {
        let twoPi = 2 * CGFloat.pi
        var delta = end - start
        if clockwise {
            if delta > 0 { delta -= twoPi * (delta / twoPi).rounded(.up) }
            if delta < -twoPi { delta = -twoPi }
        } else {
            if delta < 0 { delta += twoPi * (-delta / twoPi).rounded(.up) }
            if delta > twoPi { delta = twoPi }
        }
        appendArc(center: center, radius: radius, start: start, delta: delta, transform: t, to: &out)
    }

    /// Arco tangente às retas (atual → t1) e (t1 → t2), como CGPathAddArcToPoint.
    package static func appendArc(tangent1End t1: CGPoint, tangent2End t2: CGPoint, radius: CGFloat,
                                  transform t: CGAffineTransform = .identity, to out: inout [Path.Element]) {
        out.append(contentsOf: tangentArc(from: currentPoint(out), t1, t2, radius: radius, transform: t))
    }

    /// Elementos do arco tangente (a partir do ponto atual `cur`, já transformado).
    static func tangentArc(from cur: CGPoint?, _ t1: CGPoint, _ t2: CGPoint, radius: CGFloat, transform t: CGAffineTransform) -> [Path.Element] {
        guard let cur else { return [.move(to: t1.applying(t))] }
        // o ponto atual já está transformado: volta para o espaço local
        let p0 = t.isIdentity ? cur : cur.applying(t.inverted())
        let v1 = CGPoint(x: p0.x - t1.x, y: p0.y - t1.y), v2 = CGPoint(x: t2.x - t1.x, y: t2.y - t1.y)
        let l1 = hypot(v1.x, v1.y), l2 = hypot(v2.x, v2.y)
        let cross = v1.x * v2.y - v1.y * v2.x
        guard l1 > 0, l2 > 0, abs(cross) > 1e-12, radius > 0 else { return [.line(to: t1.applying(t))] }
        let u1 = CGPoint(x: v1.x / l1, y: v1.y / l1), u2 = CGPoint(x: v2.x / l2, y: v2.y / l2)
        let cosA = max(-1, min(1, u1.x * u2.x + u1.y * u2.y))
        let half = acos(cosA) / 2
        let d = radius / tan(half)                  // distância de t1 aos pontos de tangência
        let a = CGPoint(x: t1.x + u1.x * d, y: t1.y + u1.y * d)
        let b = CGPoint(x: t1.x + u2.x * d, y: t1.y + u2.y * d)
        let bis = CGPoint(x: u1.x + u2.x, y: u1.y + u2.y), bl = hypot(bis.x, bis.y)
        let dc = radius / sin(half)
        let c = CGPoint(x: t1.x + bis.x / bl * dc, y: t1.y + bis.y / bl * dc)
        let sa = atan2(a.y - c.y, a.x - c.x), sb = atan2(b.y - c.y, b.x - c.x)
        var delta = sb - sa
        while delta > .pi { delta -= 2 * .pi }
        while delta < -.pi { delta += 2 * .pi }
        var arc: [Path.Element] = [.move(to: cur)]   // appendArc liga com reta a partir do ponto atual
        appendArc(center: c, radius: radius, start: sa, delta: delta, transform: t, to: &arc)
        arc.removeFirst()
        return arc
    }

    // MARK: - consultas

    /// Ponto atual: o último ponto; depois de closeSubpath, o início do subcaminho; nil sem elementos.
    package static func currentPoint(_ els: [Path.Element]) -> CGPoint? {
        var start: CGPoint? = nil, cur: CGPoint? = nil
        for e in els {
            switch e {
            case let .move(to: p): start = p; cur = p
            case let .line(to: p), let .quadCurve(to: p, _), let .curve(to: p, _, _):
                if start == nil { start = p }
                cur = p
            case .closeSubpath: cur = start
            }
        }
        return cur
    }

    /// Polilinhas (com indicação de fechamento); tol = erro máximo aproximado.
    package static func flatten(_ els: [Path.Element], tolerance tol: CGFloat = 0.1) -> [(points: [CGPoint], closed: Bool)] {
        var out: [(points: [CGPoint], closed: Bool)] = []
        var cur: [CGPoint] = []
        var last = CGPoint.zero, start = CGPoint.zero
        func flush(_ closed: Bool) { if !cur.isEmpty { out.append((cur, closed)) }; cur = [] }
        func steps(_ len: CGFloat) -> Int { max(1, min(128, Int((len / max(tol, 1e-6)).squareRoot().rounded(.up)))) }
        for e in els {
            switch e {
            case let .move(to: p):
                flush(false); start = p; last = p; cur = [p]
            case let .line(to: p):
                if cur.isEmpty { cur = [last]; start = last }
                cur.append(p); last = p
            case let .quadCurve(to: p, control: c):
                if cur.isEmpty { cur = [last]; start = last }
                let n = steps(hypot(c.x - last.x, c.y - last.y) + hypot(p.x - c.x, p.y - c.y))
                for i in 1...n {
                    let u = CGFloat(i) / CGFloat(n), v = 1 - u
                    cur.append(CGPoint(x: v * v * last.x + 2 * v * u * c.x + u * u * p.x, y: v * v * last.y + 2 * v * u * c.y + u * u * p.y))
                }
                last = p
            case let .curve(to: p, control1: c1, control2: c2):
                if cur.isEmpty { cur = [last]; start = last }
                let n = steps(hypot(c1.x - last.x, c1.y - last.y) + hypot(c2.x - c1.x, c2.y - c1.y) + hypot(p.x - c2.x, p.y - c2.y)) * 3 / 2 + 1
                for i in 1...n {
                    let u = CGFloat(i) / CGFloat(n), v = 1 - u
                    let a = v * v * v, b = 3 * v * v * u, cc = 3 * v * u * u, d = u * u * u
                    cur.append(CGPoint(x: a * last.x + b * c1.x + cc * c2.x + d * p.x, y: a * last.y + b * c1.y + cc * c2.y + d * p.y))
                }
                last = p
            case .closeSubpath:
                flush(true); last = start
            }
        }
        flush(false)
        return out
    }

    /// Limites dos segmentos, sem os pontos de controle (as curvas são amostradas).
    package static func boundingRect(_ els: [Path.Element]) -> CGRect {
        var minX = CGFloat.infinity, minY = CGFloat.infinity, maxX = -CGFloat.infinity, maxY = -CGFloat.infinity
        for line in flatten(els, tolerance: 0.01) {
            for p in line.points { minX = min(minX, p.x); minY = min(minY, p.y); maxX = max(maxX, p.x); maxY = max(maxY, p.y) }
        }
        guard minX <= maxX else { return .null }
        return CGRect(x: minX, y: minY, width: maxX - minX, height: maxY - minY)
    }

    package static func contains(_ els: [Path.Element], point q: CGPoint, eoFill: Bool) -> Bool {
        var winding = 0
        for line in flatten(els, tolerance: 0.05) {
            let pts = line.points
            guard pts.count > 1 else { continue }
            for i in 0..<pts.count {
                let a = pts[i], b = pts[(i + 1) % pts.count]   // todo subcaminho é fechado para preencher
                if (a.y <= q.y) != (b.y <= q.y) {
                    let x = a.x + (q.y - a.y) * (b.x - a.x) / (b.y - a.y)
                    if x > q.x { winding += a.y < b.y ? 1 : -1 }
                }
            }
        }
        return eoFill ? winding & 1 != 0 : winding != 0
    }

    package static func transformed(_ els: [Path.Element], _ t: CGAffineTransform) -> [Path.Element] {
        guard !t.isIdentity else { return els }
        return els.map { e in
            switch e {
            case let .move(to: p): return .move(to: p.applying(t))
            case let .line(to: p): return .line(to: p.applying(t))
            case let .quadCurve(to: p, control: c): return .quadCurve(to: p.applying(t), control: c.applying(t))
            case let .curve(to: p, control1: c1, control2: c2): return .curve(to: p.applying(t), control1: c1.applying(t), control2: c2.applying(t))
            case .closeSubpath: return .closeSubpath
            }
        }
    }

    // MARK: - trimmedPath

    /// Trecho entre as frações `from` e `to` do comprimento total (curvas cortadas por de Casteljau).
    package static func trimmed(_ els: [Path.Element], from: CGFloat, to: CGFloat) -> [Path.Element] {
        // segmentos com comprimento (fechamentos viram retas até o início)
        enum Seg { case line(CGPoint, CGPoint), quad(CGPoint, CGPoint, CGPoint), cubic(CGPoint, CGPoint, CGPoint, CGPoint) }
        var segs: [(seg: Seg, len: CGFloat, newSubpath: Bool)] = []
        var last = CGPoint.zero, start = CGPoint.zero, pendingMove = true
        func add(_ s: Seg, _ len: CGFloat) { segs.append((s, len, pendingMove)); pendingMove = false }
        for e in els {
            switch e {
            case let .move(to: p): last = p; start = p; pendingMove = true
            case let .line(to: p): add(.line(last, p), hypot(p.x - last.x, p.y - last.y)); last = p
            case let .quadCurve(to: p, control: c): add(.quad(last, c, p), curveLength([last, c, p])); last = p
            case let .curve(to: p, control1: c1, control2: c2): add(.cubic(last, c1, c2, p), curveLength([last, c1, c2, p])); last = p
            case .closeSubpath:
                if last != start { add(.line(last, start), hypot(start.x - last.x, start.y - last.y)) }
                last = start; pendingMove = true
            }
        }
        let total = segs.reduce(0) { $0 + $1.len }
        guard total > 0 else { return [] }
        let a = max(0, min(1, from)) * total, b = max(0, min(1, to)) * total
        guard b > a else { return [] }
        var out: [Path.Element] = []
        var pos: CGFloat = 0, needMove = true
        for s in segs {
            let s0 = pos, s1 = pos + s.len
            pos = s1
            if s.newSubpath { needMove = true }
            guard s1 > a, s0 < b, s.len > 0 else { continue }
            let t0 = max(0, (a - s0) / s.len), t1 = min(1, (b - s0) / s.len)
            switch s.seg {
            case let .line(p, q):
                let pa = lerp(p, q, t0), pb = lerp(p, q, t1)
                if needMove { out.append(.move(to: pa)); needMove = false }
                out.append(.line(to: pb))
            case let .quad(p0, c, p1):
                let pts = subcurve([p0, c, p1], t0, t1)
                if needMove { out.append(.move(to: pts[0])); needMove = false }
                out.append(.quadCurve(to: pts[2], control: pts[1]))
            case let .cubic(p0, c1, c2, p1):
                let pts = subcurve([p0, c1, c2, p1], t0, t1)
                if needMove { out.append(.move(to: pts[0])); needMove = false }
                out.append(.curve(to: pts[3], control1: pts[1], control2: pts[2]))
            }
        }
        return out
    }

    static func lerp(_ a: CGPoint, _ b: CGPoint, _ t: CGFloat) -> CGPoint { CGPoint(x: a.x + (b.x - a.x) * t, y: a.y + (b.y - a.y) * t) }

    static func curveLength(_ c: [CGPoint]) -> CGFloat {
        var len: CGFloat = 0, prev = c[0]
        for i in 1...32 { let p = evaluate(c, CGFloat(i) / 32); len += hypot(p.x - prev.x, p.y - prev.y); prev = p }
        return len
    }

    static func evaluate(_ c: [CGPoint], _ t: CGFloat) -> CGPoint {
        var pts = c
        while pts.count > 1 { pts = (0..<(pts.count - 1)).map { lerp(pts[$0], pts[$0 + 1], t) } }
        return pts[0]
    }

    /// Divide em t (de Casteljau): (esquerda, direita), cada uma com o mesmo grau.
    static func split(_ c: [CGPoint], _ t: CGFloat) -> ([CGPoint], [CGPoint]) {
        var left: [CGPoint] = [], right: [CGPoint] = []
        var pts = c
        while !pts.isEmpty {
            left.append(pts[0]); right.insert(pts[pts.count - 1], at: 0)
            pts = pts.count > 1 ? (0..<(pts.count - 1)).map { lerp(pts[$0], pts[$0 + 1], t) } : []
        }
        return (left, right)
    }

    /// Trecho [t0, t1] da curva (t por fração de comprimento, convertido para parâmetro por amostragem).
    static func subcurve(_ c: [CGPoint], _ f0: CGFloat, _ f1: CGFloat) -> [CGPoint] {
        func param(_ f: CGFloat) -> CGFloat {
            if f <= 0 { return 0 }
            if f >= 1 { return 1 }
            let n = 64
            var acc: [CGFloat] = [0], prev = c[0]
            for i in 1...n { let p = evaluate(c, CGFloat(i) / CGFloat(n)); acc.append(acc[i - 1] + hypot(p.x - prev.x, p.y - prev.y)); prev = p }
            let target = f * acc[n]
            var i = 1
            while i < n && acc[i] < target { i += 1 }
            let seg = acc[i] - acc[i - 1]
            let u = seg > 0 ? (target - acc[i - 1]) / seg : 0
            return (CGFloat(i - 1) + u) / CGFloat(n)
        }
        let t0 = param(f0), t1 = param(f1)
        var curve = c
        if t1 < 1 { curve = split(curve, t1).0 }
        if t0 > 0 { curve = split(curve, t1 > 0 ? t0 / t1 : 0).1 }
        return curve
    }

    // MARK: - descrição textual (formato de _CGPathCopyDescription: "x y m", "x y l", "cx cy x y q",
    // "c1x c1y c2x c2y x y c", "h"; o parse aceita também "x y w h re")

    package static func describe(_ els: [Path.Element]) -> String {
        func n(_ v: CGFloat) -> String {
            let d = Double(v)
            if d == d.rounded(), abs(d) < 1e15 { return String(Int64(d)) }
            return "\(d)"
        }
        func p(_ q: CGPoint) -> String { "\(n(q.x)) \(n(q.y))" }
        var parts: [String] = []
        for e in els {
            switch e {
            case let .move(to: q): parts.append("\(p(q)) m")
            case let .line(to: q): parts.append("\(p(q)) l")
            case let .quadCurve(to: q, control: c): parts.append("\(p(c)) \(p(q)) q")
            case let .curve(to: q, control1: c1, control2: c2): parts.append("\(p(c1)) \(p(c2)) \(p(q)) c")
            case .closeSubpath: parts.append("h")
            }
        }
        return parts.joined(separator: " ")
    }

    package static func parse(_ s: String) -> [Path.Element]? {
        var stack: [CGFloat] = []
        var out: [Path.Element] = []
        for token in s.split(whereSeparator: { $0 == " " || $0 == "\n" || $0 == "\t" || $0 == "," }) {
            if let v = Double(token) { stack.append(CGFloat(v)); continue }
            func pt(_ i: Int) -> CGPoint { CGPoint(x: stack[i], y: stack[i + 1]) }
            switch token {
            case "m": guard stack.count == 2 else { return nil }; out.append(.move(to: pt(0)))
            case "l": guard stack.count == 2 else { return nil }; out.append(.line(to: pt(0)))
            case "q": guard stack.count == 4 else { return nil }; out.append(.quadCurve(to: pt(2), control: pt(0)))
            case "c": guard stack.count == 6 else { return nil }; out.append(.curve(to: pt(4), control1: pt(0), control2: pt(2)))
            case "h": guard stack.isEmpty else { return nil }; out.append(.closeSubpath)
            case "re": guard stack.count == 4 else { return nil }; appendRect(CGRect(x: stack[0], y: stack[1], width: stack[2], height: stack[3]), to: &out)
            default: return nil
            }
            stack.removeAll()
        }
        return stack.isEmpty ? out : nil
    }
}
