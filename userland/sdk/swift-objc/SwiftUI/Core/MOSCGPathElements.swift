// mOS — SwiftUI.framework (variante com interop ObjC): o Path da variante nativa (OpenSwiftUIMOS/Replace/.../Path.swift e
// OpenSwiftUIMOS/Core/MOSPathGeometry.swift) guarda os elementos num CGPath do OpenCoreGraphics (classe Swift com
// `elements`). Aqui o CGPath é o tipo CF do CoreGraphics do libSystem, com o nome mangled da Apple (So6CGPathC); esta
// extensão dá a ele a mesma interface (`CGPath.Element`, `elements`, `CGMutablePath(elements:)`), sobre CGPathApply e
// CGPathAdd*. O setter de `elements` só acrescenta (é o uso do Path); outra mudança é ignorada.
package import CoreGraphics

extension CGPath {
    package enum Element: Equatable {
        case moveToPoint(CGPoint)
        case addLineToPoint(CGPoint)
        case addQuadCurveToPoint(CGPoint, CGPoint)
        case addCurveToPoint(CGPoint, CGPoint, CGPoint)
        case closeSubpath
    }

    package var elements: [Element] {
        get {
            var out: [Element] = []
            applyWithBlock { p in
                let e = p.pointee
                switch e.type {
                case .moveToPoint: out.append(.moveToPoint(e.points[0]))
                case .addLineToPoint: out.append(.addLineToPoint(e.points[0]))
                case .addQuadCurveToPoint: out.append(.addQuadCurveToPoint(e.points[0], e.points[1]))
                case .addCurveToPoint: out.append(.addCurveToPoint(e.points[0], e.points[1], e.points[2]))
                case .closeSubpath: out.append(.closeSubpath)
                @unknown default: break
                }
            }
            return out
        }
        set {
            let old = elements
            guard newValue.count >= old.count, Array(newValue.prefix(old.count)) == old else { return }
            // no mOS CGPath e CGMutablePath são o mesmo objeto do libSystem: CGPathAdd* vale para os dois
            let m = unsafeDowncast(self, to: CGMutablePath.self)
            for e in newValue.dropFirst(old.count) {
                switch e {
                case let .moveToPoint(p): m.move(to: p)
                case let .addLineToPoint(p): m.addLine(to: p)
                case let .addQuadCurveToPoint(c, p): m.addQuadCurve(to: p, control: c)
                case let .addCurveToPoint(c1, c2, p): m.addCurve(to: p, control1: c1, control2: c2)
                case .closeSubpath: m.closeSubpath()
                }
            }
        }
    }
}

extension _MOSCFFactoryInitializable where Self == CGMutablePath {
    package init(elements: [CGPath.Element]) {
        self.init()
        self.elements = elements
    }
}
