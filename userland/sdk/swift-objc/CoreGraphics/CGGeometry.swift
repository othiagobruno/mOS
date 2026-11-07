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
