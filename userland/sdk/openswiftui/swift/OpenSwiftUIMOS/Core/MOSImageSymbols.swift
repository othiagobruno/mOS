// mOS — conjunto básico de símbolos do sistema para Image(systemName:) (entra no módulo OpenSwiftUICore).
// O mOS não tem o catálogo CoreGlyphs da Apple: cada símbolo é desenhado aqui por caminhos numa grade
// 24×24 (origem no topo), preenchidos ou traçados pela ponte C (mos_draw_path). Este arquivo só depende
// da stdlib e do MOSBridge, para poder ser compilado isoladamente.
//
// Limitações: não há regra par-ímpar nem recorte transparente na ponte; os "furos" das variantes .fill
// (o X dentro de xmark.circle.fill, por exemplo) são pintados em branco por cima do preenchimento.
import MOSBridge

package enum MOSSymbols {
    package typealias P = (x: Double, y: Double)

    /// Primitiva de um símbolo, em coordenadas da grade 24×24.
    /// É struct (não enum com payloads): enums multi-payload com arrays derrubam o runtime Swift do mOS
    /// hoje (repro nas notas da sessão de imagem); os construtores estáticos imitam os "cases".
    package struct Prim {
        var pts: [P]
        var isFill: Bool        // preenchido (senão traçado)
        var closed: Bool
        var width: Double       // espessura do traço, em unidades da grade
        var isCut: Bool         // "furo": pintado na cor de recorte

        /// polígono preenchido na cor do símbolo
        static func fill(_ p: [P]) -> Prim { Prim(pts: p, isFill: true, closed: true, width: 0, isCut: false) }
        static func stroke(_ p: [P], closed: Bool, width: Double) -> Prim { Prim(pts: p, isFill: false, closed: closed, width: width, isCut: false) }
        static func cutFill(_ p: [P]) -> Prim { Prim(pts: p, isFill: true, closed: true, width: 0, isCut: true) }
        static func cutStroke(_ p: [P], closed: Bool, width: Double) -> Prim { Prim(pts: p, isFill: false, closed: closed, width: width, isCut: true) }
        func with(_ p: [P]) -> Prim { var c = self; c.pts = p; return c }
    }

    /// Espessura padrão do traço (peso regular), em unidades da grade.
    static let lw = 1.8

    // MARK: consulta

    /// Nomes conhecidos (na ordem em que foram definidos).
    package static var names: [String] { table.keys.sorted() }

    package static func has(_ name: String) -> Bool { table[aliases[name] ?? name] != nil }

    /// nome sem preenchimento → nome com preenchimento (usado pelo SymbolVariants.fill)
    package static var fillMapping: [String: String] {
        var m: [String: String] = [:]
        for k in table.keys where !k.hasSuffix(".fill") {
            if table[k + ".fill"] != nil { m[k] = k + ".fill" }
            // xmark.circle → xmark.circle.fill etc. já coberto acima
        }
        return m
    }

    package static let aliases: [String: String] = [
        "gear": "gearshape", "gear.fill": "gearshape.fill", "multiply": "xmark", "plus.app": "plus.square",
        "magnifyingglass.circle": "magnifyingglass", "trash.circle": "trash", "line.horizontal.3": "line.3.horizontal",
        "ellipsis.circle": "ellipsis", "square.and.pencil": "pencil", "photo.fill": "photo", "globe.americas": "globe",
        "cart.fill": "cart", "list.dash": "list.bullet",
    ]

    package static func prims(_ name: String) -> [Prim]? { table[aliases[name] ?? name] }

    // MARK: desenho

    /// Desenha o símbolo. `map` leva um ponto da grade 24×24 às coordenadas da view; `unit` é o tamanho
    /// (em pontos da view) de uma unidade da grade, para a espessura dos traços.
    @discardableResult
    package static func draw(_ name: String, r: Double, g: Double, b: Double, a: Double,
                             unit: Double, map: (Double, Double) -> (Double, Double)) -> Bool {
        guard let list = prims(name) else { return false }
        // cor de recorte: branca (a ponte não recorta)
        let cr = 1.0, cg = 1.0, cb = 1.0
        for p in list {
            if p.isCut {
                emit(p.pts, closed: p.closed, fill: p.isFill, width: p.width * unit, cr, cg, cb, a, map)
            } else {
                emit(p.pts, closed: p.closed, fill: p.isFill, width: p.width * unit, r, g, b, a, map)
            }
        }
        return true
    }

    private static func emit(_ pts: [P], closed: Bool, fill: Bool, width: Double,
                             _ r: Double, _ g: Double, _ b: Double, _ a: Double,
                             _ map: (Double, Double) -> (Double, Double)) {
        guard pts.count >= 2 else { return }
        var xy: [Double] = []
        xy.reserveCapacity(pts.count * 2)
        for p in pts { let q = map(p.x, p.y); xy.append(q.0); xy.append(q.1) }
        xy.withUnsafeBufferPointer { buf in
            mos_draw_path(buf.baseAddress, Int32(pts.count), closed ? 1 : 0, r, g, b, a, fill ? 1 : 0, max(width, 0.5))
        }
    }

    // MARK: geometria

    static func rad(_ d: Double) -> Double { d * Double.pi / 180 }
    static func cosd(_ d: Double) -> Double { msin(rad(d) + Double.pi / 2) }
    static func sind(_ d: Double) -> Double { msin(rad(d)) }

    /// seno sem libm: redução a [-π, π] e série de Taylor (precisão de sobra para desenhar ícones)
    static func msin(_ x: Double) -> Double {
        let twoPi = 2 * Double.pi
        var v = x - twoPi * (x / twoPi).rounded()
        if v > Double.pi / 2 { v = Double.pi - v } else if v < -Double.pi / 2 { v = -Double.pi - v }
        let v2 = v * v
        var term = v, sum = v
        for n in 1...8 {
            term *= -v2 / Double((2 * n) * (2 * n + 1))
            sum += term
        }
        return sum
    }

    /// Arco de `a0` a `a1` graus (0 = direita; ângulos crescem no sentido horário, pois y cresce para baixo).
    static func arc(_ cx: Double, _ cy: Double, _ r: Double, _ a0: Double, _ a1: Double, _ n: Int = 24) -> [P] {
        (0...n).map { i in
            let a = a0 + (a1 - a0) * Double(i) / Double(n)
            return (cx + r * cosd(a), cy + r * sind(a))
        }
    }
    static func circle(_ cx: Double, _ cy: Double, _ r: Double, _ n: Int = 36) -> [P] {
        Array(arc(cx, cy, r, 0, 360, n).dropLast())
    }
    static func ellipse(_ cx: Double, _ cy: Double, _ rx: Double, _ ry: Double, _ n: Int = 36) -> [P] {
        (0..<n).map { i in let a = 360 * Double(i) / Double(n); return (cx + rx * cosd(a), cy + ry * sind(a)) }
    }
    static func rrect(_ x: Double, _ y: Double, _ w: Double, _ h: Double, _ r: Double) -> [P] {
        let r = min(r, w / 2, h / 2)
        if r <= 0 { return [(x, y), (x + w, y), (x + w, y + h), (x, y + h)] }
        return arc(x + w - r, y + r, r, -90, 0, 6) + arc(x + w - r, y + h - r, r, 0, 90, 6)
             + arc(x + r, y + h - r, r, 90, 180, 6) + arc(x + r, y + r, r, 180, 270, 6)
    }
    /// Gira pontos `quarters` quartos de volta (sentido horário) em torno do centro da grade.
    static func rot(_ pts: [P], _ quarters: Int) -> [P] {
        pts.map { p in
            var q = p
            for _ in 0..<((quarters % 4 + 4) % 4) { q = (24 - q.y, q.x) }
            return q
        }
    }
    static func rot(_ list: [Prim], _ quarters: Int) -> [Prim] { list.map { mapPrim($0) { rot($0, quarters) } } }
    /// Escala as primitivas em torno do centro (para pôr um glifo dentro de um círculo/quadrado).
    static func scaled(_ list: [Prim], _ s: Double) -> [Prim] {
        list.map { pr in pr.with(pr.pts.map { (12 + ($0.x - 12) * s, 12 + ($0.y - 12) * s) }) }
    }
    static func mapPrim(_ p: Prim, _ f: ([P]) -> [P]) -> Prim { p.with(f(p.pts)) }
    /// Troca a cor das primitivas para a de recorte (o glifo interno das variantes .circle.fill).
    static func cut(_ list: [Prim]) -> [Prim] {
        list.map { pr in var c = pr; c.isCut = true; return c }
    }
    static func line(_ x0: Double, _ y0: Double, _ x1: Double, _ y1: Double, _ w: Double = lw) -> Prim {
        .stroke([(x0, y0), (x1, y1)], closed: false, width: w)
    }
    static func open(_ pts: [P], _ w: Double = lw) -> Prim { .stroke(pts, closed: false, width: w) }
    static func closed(_ pts: [P], _ w: Double = lw) -> Prim { .stroke(pts, closed: true, width: w) }
    static func dot(_ cx: Double, _ cy: Double, _ r: Double) -> Prim { .fill(circle(cx, cy, r, 16)) }

    /// Lua crescente: o círculo externo menos o interno (calculado pelas interseções).
    static func crescent() -> [P] {
        let (c1x, c1y, r1) = (12.0, 12.0, 9.0)      // disco da lua
        let (c2x, c2y, r2) = (16.5, 7.5, 7.5)       // disco que "come" a lua
        func d2(_ p: P, _ cx: Double, _ cy: Double) -> Double { (p.x - cx) * (p.x - cx) + (p.y - cy) * (p.y - cy) }
        /// trecho contíguo de um círculo amostrado que satisfaz `keep` (gira a lista para o corte ficar nas pontas)
        func run(_ pts: [P], _ keep: (P) -> Bool) -> [P] {
            guard let gap = pts.firstIndex(where: { !keep($0) }) else { return pts }
            let rotated = Array(pts[gap...] + pts[..<gap])
            return rotated.filter(keep)
        }
        let outer = run(circle(c1x, c1y, r1, 96)) { d2($0, c2x, c2y) > r2 * r2 }
        var inner = run(circle(c2x, c2y, r2, 96)) { d2($0, c1x, c1y) < r1 * r1 }
        guard let end = outer.last, let a = inner.first, let b = inner.last else { return outer }
        if d2(a, end.x, end.y) > d2(b, end.x, end.y) { inner.reverse() }   // volta pelo lado de dentro
        return outer + inner
    }

    static func heart() -> [P] {
        (0..<48).map { i in
            let t = 2 * Double.pi * Double(i) / 48
            let s = msin(t)
            func c(_ k: Double) -> Double { msin(k * t + Double.pi / 2) }
            let x = 16 * s * s * s
            let y = 13 * c(1) - 5 * c(2) - 2 * c(3) - c(4)
            return (12 + x * 0.6, 11 - y * 0.6)
        }
    }
    static func star() -> [P] {
        (0..<10).map { i in
            let r = i % 2 == 0 ? 10.5 : 4.4
            let a = -90 + 36 * Double(i)
            return (12 + r * cosd(a), 12.8 + r * sind(a))
        }
    }
    static func gear() -> [P] {
        var pts: [P] = []
        for i in 0..<8 {
            let base = 45 * Double(i)
            for (da, r) in [(-14.0, 7.6), (-9.0, 10.4), (9.0, 10.4), (14.0, 7.6)] {
                pts.append((12 + r * cosd(base + da), 12 + r * sind(base + da)))
            }
        }
        return pts
    }
    static func arrowHead(tip: P, dirDeg: Double, len: Double = 4.5) -> [P] {
        [(tip.x - len * cosd(dirDeg - 40), tip.y - len * sind(dirDeg - 40)), tip,
         (tip.x - len * cosd(dirDeg + 40), tip.y - len * sind(dirDeg + 40))]
    }

    // glifos internos reutilizados nas variantes .circle / .square
    static let plusG: [Prim] = [line(12, 4, 12, 20), line(4, 12, 20, 12)]
    static let minusG: [Prim] = [line(4, 12, 20, 12)]
    static let xG: [Prim] = [line(5, 5, 19, 19), line(19, 5, 5, 19)]
    static let checkG: [Prim] = [open([(4, 12.5), (9.5, 18), (20, 6)])]
    static let infoG: [Prim] = [dot(12, 6.5, 1.6), line(12, 10.5, 12, 19, 2.2)]
    static let exclG: [Prim] = [line(12, 4.5, 12, 14.5, 2.2), dot(12, 19, 1.6)]
    static let personG: [Prim] = [.fill(circle(12, 7.5, 4.2)), .fill(arc(12, 21, 8.5, 180, 360, 20))]
    static let chevronR: [Prim] = [open([(9, 4), (17, 12), (9, 20)], 2.2)]
    static let arrowR: [Prim] = [line(4, 12, 20, 12), open([(13, 5), (20, 12), (13, 19)])]
    static let ringPts = circle(12, 12, 10)

    static func circled(_ inner: [Prim], fill: Bool, s: Double = 0.5) -> [Prim] {
        fill ? [.fill(ringPts)] + cut(scaled(inner, s)) : [closed(ringPts)] + scaled(inner, s)
    }
    static func squared(_ inner: [Prim], fill: Bool, s: Double = 0.5) -> [Prim] {
        let sq = rrect(2.5, 2.5, 19, 19, 4)
        return fill ? [.fill(sq)] + cut(scaled(inner, s)) : [closed(sq)] + scaled(inner, s)
    }

    // MARK: tabela

    static let table: [String: [Prim]] = {
        var t: [String: [Prim]] = [:]
        func pair(_ n: String, _ outline: [Prim], _ filled: [Prim]) { t[n] = outline; t[n + ".fill"] = filled }

        pair("circle", [closed(circle(12, 12, 9.5))], [.fill(circle(12, 12, 9.5))])
        pair("square", [closed(rrect(3, 3, 18, 18, 3))], [.fill(rrect(3, 3, 18, 18, 3))])
        pair("rectangle", [closed(rrect(1.5, 5, 21, 14, 2.5))], [.fill(rrect(1.5, 5, 21, 14, 2.5))])
        let tri: [P] = [(12, 3), (22, 20.5), (2, 20.5)]
        pair("triangle", [closed(tri)], [.fill(tri)])
        pair("star", [closed(star())], [.fill(star())])
        pair("heart", [closed(heart())], [.fill(heart())])
        pair("diamond", [closed([(12, 2), (22, 12), (12, 22), (2, 12)])], [.fill([(12, 2), (22, 12), (12, 22), (2, 12)])])

        t["plus"] = plusG
        t["minus"] = minusG
        t["xmark"] = xG
        t["checkmark"] = checkG
        for (n, g) in [("plus", plusG), ("minus", minusG), ("xmark", xG), ("checkmark", checkG),
                       ("info", infoG), ("exclamationmark", exclG), ("person", personG),
                       ("chevron.right", chevronR), ("arrow.right", arrowR)] {
            pair(n + ".circle", circled(g, fill: false), circled(g, fill: true))
            pair(n + ".square", squared(g, fill: false), squared(g, fill: true))
        }
        t["exclamationmark"] = exclG

        t["chevron.right"] = chevronR
        t["chevron.down"] = rot(chevronR, 1)
        t["chevron.left"] = rot(chevronR, 2)
        t["chevron.up"] = rot(chevronR, 3)
        t["arrow.right"] = arrowR
        t["arrow.down"] = rot(arrowR, 1)
        t["arrow.left"] = rot(arrowR, 2)
        t["arrow.up"] = rot(arrowR, 3)
        for (n, q) in [("chevron.down", 1), ("chevron.left", 2), ("chevron.up", 3),
                       ("arrow.down", 1), ("arrow.left", 2), ("arrow.up", 3)] {
            let base = n.hasPrefix("chevron") ? chevronR : arrowR
            pair(n + ".circle", circled(rot(base, q), fill: false), circled(rot(base, q), fill: true))
        }
        let cw = arc(12, 12.5, 7.5, -60, 240, 28)
        // a ponta fica no fim do arco (240°), apontando no sentido horário (tangente a -30°)
        let cwHead = arrowHead(tip: cw.last!, dirDeg: -30)
        t["arrow.clockwise"] = [open(cw), open(cwHead)]
        t["arrow.counterclockwise"] = [open(cw.map { (24 - $0.x, $0.y) }), open(cwHead.map { (24 - $0.x, $0.y) })]

        let houseFill: [P] = [(12, 2.5), (22.5, 12.5), (19, 12.5), (19, 21), (5, 21), (5, 12.5), (1.5, 12.5)]
        pair("house", [open([(2.5, 12), (12, 3), (21.5, 12)]), open([(5.5, 10), (5.5, 21), (18.5, 21), (18.5, 10)]),
                       open([(10, 21), (10, 15), (14, 15), (14, 21)])],
             [.fill(houseFill), .cutFill(rrect(10, 15, 4, 6, 0.5))])
        pair("gearshape", [closed(gear()), closed(circle(12, 12, 3.5))], [.fill(gear()), .cutFill(circle(12, 12, 3.2))])
        t["magnifyingglass"] = [closed(circle(10, 10, 6.5)), line(15, 15, 21, 21, 2.6)]
        let can: [P] = [(5.5, 6.5), (6.7, 21), (17.3, 21), (18.5, 6.5)]
        pair("trash", [line(3.5, 6.5, 20.5, 6.5), open([(9, 6.5), (9, 3.5), (15, 3.5), (15, 6.5)]), open(can),
                       line(10, 10, 10, 17.5, 1.5), line(14, 10, 14, 17.5, 1.5)],
             [line(3.5, 6.5, 20.5, 6.5), open([(9, 6.5), (9, 3.5), (15, 3.5), (15, 6.5)]), .fill(can)])
        let folder: [P] = [(2.5, 5.5), (2.5, 19.5), (21.5, 19.5), (21.5, 8), (11, 8), (9, 5.5)]
        pair("folder", [closed(folder)], [.fill(folder)])
        let doc: [P] = [(5, 2.5), (14, 2.5), (19, 7.5), (19, 21.5), (5, 21.5)]
        pair("doc", [closed(doc), open([(14, 2.5), (14, 7.5), (19, 7.5)])], [.fill(doc)])
        let docLines: [Prim] = [line(8, 12, 16, 12, 1.4), line(8, 15, 16, 15, 1.4), line(8, 18, 13, 18, 1.4)]
        pair("doc.text", [closed(doc), open([(14, 2.5), (14, 7.5), (19, 7.5)])] + docLines, [.fill(doc)] + cut(docLines))
        pair("person", [closed(circle(12, 7.5, 4.2)), open(arc(12, 21, 8, 180, 360, 20))], personG)
        let bell: [P] = arc(12, 10, 6.5, 180, 360, 16) + [(18.5, 15.5), (20.5, 18.5), (3.5, 18.5), (5.5, 15.5)]
        pair("bell", [closed(bell), open(arc(12, 19.5, 2.2, 0, 180, 8))], [.fill(bell), .fill(arc(12, 19.5, 2.4, 0, 180, 8))])
        let env = rrect(2.5, 5, 19, 14, 2)
        pair("envelope", [closed(env), open([(3, 6), (12, 13), (21, 6)])], [.fill(env), .cutStroke([(3, 6), (12, 13), (21, 6)], closed: false, width: 1.5)])
        let tri2: [P] = [(12, 2.5), (22.5, 21), (1.5, 21)]
        let triExcl: [Prim] = [line(12, 9, 12, 14.5, 2), dot(12, 17.8, 1.3)]
        pair("exclamationmark.triangle", [closed(tri2)] + triExcl, [.fill(tri2)] + cut(triExcl))
        pair("play", [closed([(7, 4), (19.5, 12), (7, 20)])], [.fill([(7, 4), (19.5, 12), (7, 20)])])
        pair("pause", [closed(rrect(6, 4, 4, 16, 1)), closed(rrect(14, 4, 4, 16, 1))], [.fill(rrect(6, 4, 4, 16, 1)), .fill(rrect(14, 4, 4, 16, 1))])
        pair("stop", [closed(rrect(5, 5, 14, 14, 2))], [.fill(rrect(5, 5, 14, 14, 2))])
        pair("backward", [closed([(11.5, 6), (3, 12), (11.5, 18)]), closed([(20.5, 6), (12, 12), (20.5, 18)])],
             [.fill([(11.5, 6), (3, 12), (11.5, 18)]), .fill([(20.5, 6), (12, 12), (20.5, 18)])])
        pair("forward", [closed([(3.5, 6), (12, 12), (3.5, 18)]), closed([(12.5, 6), (21, 12), (12.5, 18)])],
             [.fill([(3.5, 6), (12, 12), (3.5, 18)]), .fill([(12.5, 6), (21, 12), (12.5, 18)])])
        let rays: [Prim] = (0..<8).map { i in
            let a = 45 * Double(i)
            return line(12 + 7.3 * cosd(a), 12 + 7.3 * sind(a), 12 + 10.3 * cosd(a), 12 + 10.3 * sind(a), 1.7)
        }
        pair("sun.max", [closed(circle(12, 12, 4.3))] + rays, [.fill(circle(12, 12, 4.6))] + rays)
        pair("moon", [closed(crescent())], [.fill(crescent())])
        let cloud = arc(7, 14.5, 4, 90, 250, 10) + arc(12.5, 10.5, 5.5, 195, 345, 12) + arc(18, 14.5, 4, 280, 450, 10)
        pair("cloud", [closed(cloud)], [.fill(cloud)])
        let bolt: [P] = [(14, 2), (5, 13.5), (11, 13.5), (9.5, 22), (19, 9.5), (13, 9.5)]
        pair("bolt", [closed(bolt)], [.fill(bolt)])
        let shackle = open([(7.5, 10.5)] + arc(12, 7.5, 4.5, 180, 360, 12) + [(16.5, 10.5)])
        pair("lock", [closed(rrect(5, 10.5, 14, 11, 2)), shackle], [.fill(rrect(5, 10.5, 14, 11, 2)), shackle])
        pair("calendar", [closed(rrect(3, 4.5, 18, 16.5, 2.5)), line(3, 9, 21, 9), line(8, 2.5, 8, 6.5), line(16, 2.5, 16, 6.5)],
             [.fill(rrect(3, 4.5, 18, 16.5, 2.5)), .cutStroke([(3, 9), (21, 9)], closed: false, width: 1.4)])
        let hands: [Prim] = [open([(12, 6), (12, 12), (16.5, 14.5)])]
        pair("clock", [closed(ringPts)] + hands, [.fill(ringPts)] + cut(hands))
        t["line.3.horizontal"] = [line(4, 7, 20, 7), line(4, 12, 20, 12), line(4, 17, 20, 17)]
        t["ellipsis"] = [dot(5, 12, 1.8), dot(12, 12, 1.8), dot(19, 12, 1.8)]
        t["list.bullet"] = [dot(4.5, 7, 1.4), dot(4.5, 12, 1.4), dot(4.5, 17, 1.4),
                            line(8.5, 7, 20, 7, 1.6), line(8.5, 12, 20, 12, 1.6), line(8.5, 17, 20, 17, 1.6)]
        t["square.and.arrow.up"] = [open([(8, 9), (5, 9), (5, 21), (19, 21), (19, 9), (16, 9)]), line(12, 15, 12, 2.5),
                                    open([(8, 6.5), (12, 2.5), (16, 6.5)])]
        t["square.and.arrow.down"] = [open([(8, 9), (5, 9), (5, 21), (19, 21), (19, 9), (16, 9)]), line(12, 2.5, 12, 15),
                                      open([(8, 11), (12, 15), (16, 11)])]
        let pencil: [P] = [(16.5, 3.5), (20.5, 7.5), (8, 20), (3, 21), (4, 16)]
        pair("pencil", [closed(pencil), line(14, 6, 18, 10, 1.4)], [.fill(pencil)])
        t["photo"] = [closed(rrect(2.5, 4.5, 19, 15, 2.5)), open([(5, 17), (10, 11.5), (13.5, 15), (16, 12.5), (19, 16)]), dot(16, 8.5, 1.6)]
        let bm: [P] = [(6, 2.5), (18, 2.5), (18, 21.5), (12, 16.5), (6, 21.5)]
        pair("bookmark", [closed(bm)], [.fill(bm)])
        let flag: [P] = [(5, 3.5), (19, 3.5), (16, 8), (19, 12.5), (5, 12.5)]
        pair("flag", [line(5, 2.5, 5, 22), closed(flag)], [line(5, 2.5, 5, 22), .fill(flag)])
        t["cart"] = [open([(2, 4), (5, 4), (7.5, 15.5), (18.5, 15.5), (20.5, 7), (6, 7)]), dot(9, 19.5, 1.7), dot(17, 19.5, 1.7)]
        let chord = (9.5 * 9.5 - 4.5 * 4.5).squareRoot()
        t["globe"] = [closed(circle(12, 12, 9.5)), closed(ellipse(12, 12, 4, 9.5)), line(2.5, 12, 21.5, 12, 1.4),
                      line(12 - chord, 7.5, 12 + chord, 7.5, 1.4), line(12 - chord, 16.5, 12 + chord, 16.5, 1.4)]
        let bubble: [Prim] = [closed(rrect(3, 4, 18, 13, 4)), open([(7, 17), (6, 21), (11, 17)])]
        pair("bubble.left", bubble, [.fill(rrect(3, 4, 18, 13, 4)), .fill([(7, 16), (6, 21), (11.5, 16)])])
        let grid: [[P]] = [rrect(3, 3, 7.5, 7.5, 1.5), rrect(13.5, 3, 7.5, 7.5, 1.5), rrect(3, 13.5, 7.5, 7.5, 1.5), rrect(13.5, 13.5, 7.5, 7.5, 1.5)]
        pair("square.grid.2x2", grid.map { closed($0) }, grid.map { .fill($0) })
        pair("tag", [closed([(3, 3), (12, 3), (21, 12), (12, 21), (3, 12)]), closed(circle(7.5, 7.5, 1.4, 10))],
             [.fill([(3, 3), (12, 3), (21, 12), (12, 21), (3, 12)]), .cutFill(circle(7.5, 7.5, 1.4, 10))])
        pair("bag", [closed(rrect(4, 8, 16, 13.5, 2)), open(arc(12, 8, 3.5, 180, 360, 10))],
             [.fill(rrect(4, 8, 16, 13.5, 2)), open(arc(12, 8, 3.5, 180, 360, 10))])
        t["wifi"] = [open(arc(12, 19, 14, 225, 315, 16)), open(arc(12, 19, 9.5, 225, 315, 12)),
                     open(arc(12, 19, 5, 225, 315, 8)), dot(12, 19, 1.6)]
        t["speaker.wave.2"] = [closed([(3, 9), (7, 9), (12, 4.5), (12, 19.5), (7, 15), (3, 15)]),
                               open(arc(12, 12, 4.5, -45, 45, 8)), open(arc(12, 12, 8, -45, 45, 10))]
        t["link"] = [closed(rrect(2.5, 9, 11, 6, 3)), closed(rrect(10.5, 9, 11, 6, 3))]
        t["paperplane"] = [closed([(2.5, 11), (21.5, 3), (14, 21.5), (11, 13)]), line(11, 13, 21.5, 3, 1.4)]
        t["paperplane.fill"] = [.fill([(2.5, 11), (21.5, 3), (14, 21.5), (11, 13)])]
        return t
    }()
}
