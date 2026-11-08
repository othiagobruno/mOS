// mOS — overlay Swift "CoreGraphics" da variante com interop ObjC (docs/swift-objc-runtime.md, fase 3): o resto da
// API que binários da Apple usam (o CGFloat vem de CGFloat.swift, gerado do módulo CoreGraphics do OpenSwiftUI).

extension CGPoint: Codable {
    public init(from decoder: Decoder) throws {
        var c = try decoder.unkeyedContainer()
        self.init(x: try c.decode(CGFloat.self), y: try c.decode(CGFloat.self))
    }
    public func encode(to encoder: Encoder) throws {
        var c = encoder.unkeyedContainer()
        try c.encode(x); try c.encode(y)
    }
}

extension CGSize: Codable {
    public init(from decoder: Decoder) throws {
        var c = try decoder.unkeyedContainer()
        self.init(width: try c.decode(CGFloat.self), height: try c.decode(CGFloat.self))
    }
    public func encode(to encoder: Encoder) throws {
        var c = encoder.unkeyedContainer()
        try c.encode(width); try c.encode(height)
    }
}

extension CGRect: Codable {
    public init(from decoder: Decoder) throws {
        var c = try decoder.unkeyedContainer()
        self.init(origin: try c.decode(CGPoint.self), size: try c.decode(CGSize.self))
    }
    public func encode(to encoder: Encoder) throws {
        var c = encoder.unkeyedContainer()
        try c.encode(origin); try c.encode(size)
    }
}

@_silgen_name("CGRectMakeWithDictionaryRepresentation")
internal func _CGRectMakeWithDictionaryRepresentation(_ dict: CFDictionary, _ rect: UnsafeMutablePointer<CGRect>) -> Bool

extension CGRect {
    public init?(dictionaryRepresentation dict: CFDictionary) {
        var r = CGRect()
        guard _CGRectMakeWithDictionaryRepresentation(dict, &r) else { return nil }
        self = r
    }
}

// CGContext.draw(_:in:byTiling:) — o overlay da Apple chama CGContextDrawImage / CGContextDrawTiledImage
extension CGContext {
    public func draw(_ image: CGImage, in rect: CGRect, byTiling: Bool = false) {
        if byTiling { __CGContextDrawTiledImage(self, rect, image) } else { __CGContextDrawImage(self, rect, image) }
    }
}

// MARK: - caminhos (o helper "Computer Use" do Grok Bot desenha o realce do cursor com eles)

public enum CGPathFillRule: Int, Sendable {
    case winding
    case evenOdd
}

extension CGContext {
    public func fillPath(using rule: CGPathFillRule = .winding) {
        if rule == .evenOdd { __CGContextEOFillPath(self) } else { __CGContextFillPath(self) }
    }
}

extension CGMutablePath {
    public func move(to point: CGPoint, transform: CGAffineTransform = .identity) {
        var t = transform
        CGPathMoveToPoint(self, &t, point.x, point.y)
    }
    public func addArc(tangent1End: CGPoint, tangent2End: CGPoint, radius: CGFloat, transform: CGAffineTransform = .identity) {
        var t = transform
        CGPathAddArcToPoint(self, &t, tangent1End.x, tangent1End.y, tangent2End.x, tangent2End.y, radius)
    }
}

extension CGPath {
    public func contains(_ point: CGPoint, using rule: CGPathFillRule = .winding, transform: CGAffineTransform = .identity) -> Bool {
        var t = transform
        return CGPathContainsPoint(self, &t, point, rule == .evenOdd)
    }
}

extension CGAffineTransform {
    public static var identity: CGAffineTransform { CGAffineTransform(a: 1, b: 0, c: 0, d: 1, tx: 0, ty: 0) }
}
