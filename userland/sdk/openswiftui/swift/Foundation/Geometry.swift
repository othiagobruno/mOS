// mOS SDK — API Swift de CGPoint, CGSize e CGRect (a do swift-corelibs-foundation / CoreGraphics do macOS).
// Os tipos em si são as structs C de CFCGTypes.h no módulo clang CoreFoundation do mOS (uma só declaração para o
// Swift e para os cabeçalhos C das deps); aqui ficam as extensões. CGRectEdge também vem de lá (.minXEdge...).

extension CGPoint: @retroactive Equatable, @retroactive Hashable, @retroactive @unchecked Sendable, @retroactive Codable {
    @_transparent public init(x: Double, y: Double) { self.init(x: CGFloat(x), y: CGFloat(y)) }
    @_transparent public init(x: Int, y: Int) { self.init(x: CGFloat(x), y: CGFloat(y)) }
    public static var zero: CGPoint { CGPoint(x: 0 as CGFloat, y: 0 as CGFloat) }
    public func equalTo(_ point: CGPoint) -> Bool { self == point }
    public static func == (l: CGPoint, r: CGPoint) -> Bool { l.x == r.x && l.y == r.y }
    public func hash(into hasher: inout Hasher) { hasher.combine(x); hasher.combine(y) }
    public init(from decoder: any Decoder) throws {
        var c = try decoder.unkeyedContainer()
        self.init(x: try c.decode(CGFloat.self), y: try c.decode(CGFloat.self))
    }
    public func encode(to encoder: any Encoder) throws { var c = encoder.unkeyedContainer(); try c.encode(x); try c.encode(y) }
}

extension CGSize: @retroactive Equatable, @retroactive Hashable, @retroactive @unchecked Sendable, @retroactive Codable {
    @_transparent public init(width: Double, height: Double) { self.init(width: CGFloat(width), height: CGFloat(height)) }
    @_transparent public init(width: Int, height: Int) { self.init(width: CGFloat(width), height: CGFloat(height)) }
    public static var zero: CGSize { CGSize(width: 0 as CGFloat, height: 0 as CGFloat) }
    public func equalTo(_ size: CGSize) -> Bool { self == size }
    public static func == (l: CGSize, r: CGSize) -> Bool { l.width == r.width && l.height == r.height }
    public func hash(into hasher: inout Hasher) { hasher.combine(width); hasher.combine(height) }
    public init(from decoder: any Decoder) throws {
        var c = try decoder.unkeyedContainer()
        self.init(width: try c.decode(CGFloat.self), height: try c.decode(CGFloat.self))
    }
    public func encode(to encoder: any Encoder) throws { var c = encoder.unkeyedContainer(); try c.encode(width); try c.encode(height) }
}

extension CGRect: @retroactive Equatable, @retroactive Hashable, @retroactive @unchecked Sendable, @retroactive Codable {
    @_transparent public init(x: CGFloat, y: CGFloat, width: CGFloat, height: CGFloat) {
        self.init(origin: CGPoint(x: x, y: y), size: CGSize(width: width, height: height))
    }
    @_transparent public init(x: Double, y: Double, width: Double, height: Double) {
        self.init(origin: CGPoint(x: x, y: y), size: CGSize(width: width, height: height))
    }
    @_transparent public init(x: Int, y: Int, width: Int, height: Int) {
        self.init(origin: CGPoint(x: x, y: y), size: CGSize(width: width, height: height))
    }
    public init(from decoder: any Decoder) throws {
        var c = try decoder.unkeyedContainer()
        self.init(origin: try c.decode(CGPoint.self), size: try c.decode(CGSize.self))
    }
    public func encode(to encoder: any Encoder) throws { var c = encoder.unkeyedContainer(); try c.encode(origin); try c.encode(size) }

    public static var zero: CGRect { CGRect(origin: .zero, size: .zero) }
    public static var null: CGRect { CGRect(x: CGFloat.infinity, y: CGFloat.infinity, width: 0 as CGFloat, height: 0 as CGFloat) }
    public static var infinite: CGRect {
        CGRect(x: -CGFloat.greatestFiniteMagnitude / 2, y: -CGFloat.greatestFiniteMagnitude / 2,
               width: CGFloat.greatestFiniteMagnitude, height: CGFloat.greatestFiniteMagnitude)
    }

    public var isNull: Bool { origin.x == .infinity || origin.y == .infinity }
    public var isEmpty: Bool { isNull || size.width == 0 || size.height == 0 }
    public var isInfinite: Bool { self == .infinite }

    public var standardized: CGRect {
        if isNull { return .null }
        var r = self
        if r.size.width < 0 { r.origin.x += r.size.width; r.size.width = -r.size.width }
        if r.size.height < 0 { r.origin.y += r.size.height; r.size.height = -r.size.height }
        return r
    }
    public var width: CGFloat { abs(size.width) }
    public var height: CGFloat { abs(size.height) }
    public var minX: CGFloat { standardized.origin.x }
    public var minY: CGFloat { standardized.origin.y }
    public var maxX: CGFloat { let s = standardized; return s.origin.x + s.size.width }
    public var maxY: CGFloat { let s = standardized; return s.origin.y + s.size.height }
    public var midX: CGFloat { let s = standardized; return s.origin.x + s.size.width / 2 }
    public var midY: CGFloat { let s = standardized; return s.origin.y + s.size.height / 2 }

    public var integral: CGRect {
        if isNull || isInfinite { return self }
        let s = standardized
        let x0 = s.minX.rounded(.down), y0 = s.minY.rounded(.down)
        let x1 = s.maxX.rounded(.up), y1 = s.maxY.rounded(.up)
        return CGRect(x: x0, y: y0, width: x1 - x0, height: y1 - y0)
    }
    public func insetBy(dx: CGFloat, dy: CGFloat) -> CGRect {
        if isNull { return self }
        var r = standardized
        r.origin.x += dx; r.origin.y += dy; r.size.width -= 2 * dx; r.size.height -= 2 * dy
        return (r.size.width < 0 || r.size.height < 0) ? .null : r
    }
    public func offsetBy(dx: CGFloat, dy: CGFloat) -> CGRect {
        if isNull { return self }
        var r = standardized
        r.origin.x += dx; r.origin.y += dy
        return r
    }
    public func union(_ r2: CGRect) -> CGRect {
        if isNull { return r2.standardized }
        if r2.isNull { return standardized }
        let a = standardized, b = r2.standardized
        let x0 = Swift.min(a.minX, b.minX), y0 = Swift.min(a.minY, b.minY)
        let x1 = Swift.max(a.maxX, b.maxX), y1 = Swift.max(a.maxY, b.maxY)
        return CGRect(x: x0, y: y0, width: x1 - x0, height: y1 - y0)
    }
    public func intersection(_ r2: CGRect) -> CGRect {
        if isNull || r2.isNull { return .null }
        let a = standardized, b = r2.standardized
        let x0 = Swift.max(a.minX, b.minX), y0 = Swift.max(a.minY, b.minY)
        let x1 = Swift.min(a.maxX, b.maxX), y1 = Swift.min(a.maxY, b.maxY)
        if x1 < x0 || y1 < y0 { return .null }
        return CGRect(x: x0, y: y0, width: x1 - x0, height: y1 - y0)
    }
    public func intersects(_ r2: CGRect) -> Bool { !intersection(r2).isNull }
    public func contains(_ point: CGPoint) -> Bool {
        if isNull { return false }
        return point.x >= minX && point.x < maxX && point.y >= minY && point.y < maxY
    }
    public func contains(_ rect2: CGRect) -> Bool { union(rect2) == standardized }
    public func divided(atDistance distance: CGFloat, from edge: CGRectEdge) -> (slice: CGRect, remainder: CGRect) {
        let r = standardized
        var slice = r, rem = r
        switch edge {
        case .minXEdge:
            let d = Swift.min(Swift.max(distance, 0), r.size.width)
            slice.size.width = d; rem.origin.x += d; rem.size.width -= d
        case .minYEdge:
            let d = Swift.min(Swift.max(distance, 0), r.size.height)
            slice.size.height = d; rem.origin.y += d; rem.size.height -= d
        case .maxXEdge:
            let d = Swift.min(Swift.max(distance, 0), r.size.width)
            slice.origin.x = r.maxX - d; slice.size.width = d; rem.size.width -= d
        case .maxYEdge:
            let d = Swift.min(Swift.max(distance, 0), r.size.height)
            slice.origin.y = r.maxY - d; slice.size.height = d; rem.size.height -= d
        }
        return (slice, rem)
    }
    public func equalTo(_ rect2: CGRect) -> Bool { self == rect2 }

    public static func == (lhs: CGRect, rhs: CGRect) -> Bool {
        if lhs.isNull || rhs.isNull { return lhs.isNull && rhs.isNull }
        let a = lhs.standardized, b = rhs.standardized
        return a.origin == b.origin && a.size == b.size
    }
    public func hash(into hasher: inout Hasher) {
        let s = isNull ? CGRect(origin: CGPoint(x: CGFloat.infinity, y: CGFloat.infinity), size: .zero) : standardized
        hasher.combine(s.origin)
        hasher.combine(s.size)
    }
}

extension CGPoint: CustomStringConvertible { public var description: String { "(\(x), \(y))" } }
extension CGSize: CustomStringConvertible { public var description: String { "(\(width), \(height))" } }
extension CGRect: CustomStringConvertible { public var description: String { "(\(origin.x), \(origin.y), \(size.width), \(size.height))" } }
