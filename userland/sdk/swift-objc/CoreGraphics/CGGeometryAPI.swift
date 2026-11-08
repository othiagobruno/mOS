// mOS — a API Swift da geometria do overlay CoreGraphics da Apple. Na Apple a maior parte vem das API notes do
// módulo clang (CGRectGetMinX -> CGRect.minX, CGPointZero -> CGPoint.zero...); aqui o importador não leva swift_name
// para tipos de outro módulo (CGRect/CGPoint/CGAffineTransform são do CoreFoundation, CFCGTypes.h), então os membros
// são escritos em Swift sobre as funções C do libSystem, @_alwaysEmitIntoClient/@_transparent (sem símbolo, como na
// Apple). As conformidades têm os nomes da Apple ($sSo7CGPointVSQ12CoreGraphicsMc...).

extension CGPoint {
    @_transparent public init(x: Int, y: Int) { self.init(x: CGFloat(x), y: CGFloat(y)) }
    @_transparent public init(x: Double, y: Double) { self.init(x: CGFloat(x), y: CGFloat(y)) }
}
extension CGPoint: Equatable {
    @_transparent public static func == (lhs: CGPoint, rhs: CGPoint) -> Bool { lhs.x == rhs.x && lhs.y == rhs.y }
}
extension CGPoint: Hashable {
    public func hash(into hasher: inout Hasher) { hasher.combine(x); hasher.combine(y) }
}

extension CGSize {
    @_transparent public init(width: Int, height: Int) { self.init(width: CGFloat(width), height: CGFloat(height)) }
    @_transparent public init(width: Double, height: Double) { self.init(width: CGFloat(width), height: CGFloat(height)) }
}
extension CGSize: Equatable {
    @_transparent public static func == (lhs: CGSize, rhs: CGSize) -> Bool { lhs.width == rhs.width && lhs.height == rhs.height }
}
extension CGSize: Hashable {
    public func hash(into hasher: inout Hasher) { hasher.combine(width); hasher.combine(height) }
}

extension CGVector {
    @_transparent public init(dx: Int, dy: Int) { self.init(dx: CGFloat(dx), dy: CGFloat(dy)) }
    @_transparent public init(dx: Double, dy: Double) { self.init(dx: CGFloat(dx), dy: CGFloat(dy)) }
}
extension CGVector: Equatable {
    @_transparent public static func == (lhs: CGVector, rhs: CGVector) -> Bool { lhs.dx == rhs.dx && lhs.dy == rhs.dy }
}
extension CGVector: Hashable {
    public func hash(into hasher: inout Hasher) { hasher.combine(dx); hasher.combine(dy) }
}

extension CGRect {
    @_transparent public init(x: CGFloat, y: CGFloat, width: CGFloat, height: CGFloat) {
        self.init(origin: CGPoint(x: x, y: y), size: CGSize(width: width, height: height))
    }
    @_transparent public init(x: Double, y: Double, width: Double, height: Double) {
        self.init(origin: CGPoint(x: x, y: y), size: CGSize(width: width, height: height))
    }
    @_transparent public init(x: Int, y: Int, width: Int, height: Int) {
        self.init(origin: CGPoint(x: x, y: y), size: CGSize(width: width, height: height))
    }
    @_transparent
    public func divided(atDistance: CGFloat, from fromEdge: CGRectEdge) -> (slice: CGRect, remainder: CGRect) {
        var slice = CGRect.zero
        var remainder = CGRect.zero
        CGRectDivide(self, &slice, &remainder, atDistance, fromEdge)
        return (slice, remainder)
    }
}
extension CGRect: Equatable {
    @_transparent public static func == (lhs: CGRect, rhs: CGRect) -> Bool { lhs.origin == rhs.origin && lhs.size == rhs.size }
}
extension CGRect: Hashable {
    public func hash(into hasher: inout Hasher) { hasher.combine(origin); hasher.combine(size) }
}

extension CGAffineTransform: Equatable {
    @_transparent public static func == (lhs: CGAffineTransform, rhs: CGAffineTransform) -> Bool { CGAffineTransformEqualToTransform(lhs, rhs) }
}


extension CGPoint {
    @_alwaysEmitIntoClient public static var zero: CGPoint { CGPoint(x: 0, y: 0) }
    @_alwaysEmitIntoClient public func applying(_ t: CGAffineTransform) -> CGPoint { CGPointApplyAffineTransform(self, t) }
    @_alwaysEmitIntoClient public func equalTo(_ p: CGPoint) -> Bool { self == p }
}
extension CGSize {
    @_alwaysEmitIntoClient public static var zero: CGSize { CGSize(width: 0, height: 0) }
    @_alwaysEmitIntoClient public func applying(_ t: CGAffineTransform) -> CGSize { CGSizeApplyAffineTransform(self, t) }
    @_alwaysEmitIntoClient public func equalTo(_ s: CGSize) -> Bool { self == s }
}
extension CGVector { @_alwaysEmitIntoClient public static var zero: CGVector { CGVector(dx: 0, dy: 0) } }
extension CGRect {
    @_alwaysEmitIntoClient public static var zero: CGRect { CGRect(origin: .zero, size: .zero) }
    @_alwaysEmitIntoClient public static var null: CGRect { CGRectNull }
    @_alwaysEmitIntoClient public static var infinite: CGRect { CGRectInfinite }
    @_alwaysEmitIntoClient public var minX: CGFloat { CGRectGetMinX(self) }
    @_alwaysEmitIntoClient public var midX: CGFloat { CGRectGetMidX(self) }
    @_alwaysEmitIntoClient public var maxX: CGFloat { CGRectGetMaxX(self) }
    @_alwaysEmitIntoClient public var minY: CGFloat { CGRectGetMinY(self) }
    @_alwaysEmitIntoClient public var midY: CGFloat { CGRectGetMidY(self) }
    @_alwaysEmitIntoClient public var maxY: CGFloat { CGRectGetMaxY(self) }
    @_alwaysEmitIntoClient public var width: CGFloat { CGRectGetWidth(self) }
    @_alwaysEmitIntoClient public var height: CGFloat { CGRectGetHeight(self) }
    @_alwaysEmitIntoClient public var standardized: CGRect { CGRectStandardize(self) }
    @_alwaysEmitIntoClient public var isEmpty: Bool { CGRectIsEmpty(self) }
    @_alwaysEmitIntoClient public var isNull: Bool { CGRectIsNull(self) }
    @_alwaysEmitIntoClient public var isInfinite: Bool { CGRectIsInfinite(self) }
    @_alwaysEmitIntoClient public var integral: CGRect { CGRectIntegral(self) }
    @_alwaysEmitIntoClient public func equalTo(_ r: CGRect) -> Bool { CGRectEqualToRect(self, r) }
    @_alwaysEmitIntoClient public func insetBy(dx: CGFloat, dy: CGFloat) -> CGRect { CGRectInset(self, dx, dy) }
    @_alwaysEmitIntoClient public func offsetBy(dx: CGFloat, dy: CGFloat) -> CGRect { CGRectOffset(self, dx, dy) }
    @_alwaysEmitIntoClient public func union(_ r: CGRect) -> CGRect { CGRectUnion(self, r) }
    @_alwaysEmitIntoClient public func intersection(_ r: CGRect) -> CGRect { CGRectIntersection(self, r) }
    @_alwaysEmitIntoClient public func contains(_ p: CGPoint) -> Bool { CGRectContainsPoint(self, p) }
    @_alwaysEmitIntoClient public func contains(_ r: CGRect) -> Bool { CGRectContainsRect(self, r) }
    @_alwaysEmitIntoClient public func intersects(_ r: CGRect) -> Bool { CGRectIntersectsRect(self, r) }
    @_alwaysEmitIntoClient public func applying(_ t: CGAffineTransform) -> CGRect { CGRectApplyAffineTransform(self, t) }
    @_alwaysEmitIntoClient public mutating func standardizeInPlace() { self = standardized }
    @_alwaysEmitIntoClient public mutating func makeIntegralInPlace() { self = integral }
    @_alwaysEmitIntoClient public mutating func insetInPlace(dx: CGFloat, dy: CGFloat) { self = insetBy(dx: dx, dy: dy) }
    @_alwaysEmitIntoClient public mutating func offsetInPlace(dx: CGFloat, dy: CGFloat) { self = offsetBy(dx: dx, dy: dy) }
    @_alwaysEmitIntoClient public mutating func formUnion(_ r: CGRect) { self = union(r) }
    @_alwaysEmitIntoClient public mutating func formIntersection(_ r: CGRect) { self = intersection(r) }
}
extension CGAffineTransform {
    @_alwaysEmitIntoClient public init(translationX tx: CGFloat, y ty: CGFloat) { self = CGAffineTransformMakeTranslation(tx, ty) }
    @_alwaysEmitIntoClient public init(scaleX sx: CGFloat, y sy: CGFloat) { self = CGAffineTransformMakeScale(sx, sy) }
    @_alwaysEmitIntoClient public init(rotationAngle angle: CGFloat) { self = CGAffineTransformMakeRotation(angle) }
    @_alwaysEmitIntoClient public init(_ components: CGAffineTransformComponents) { self = CGAffineTransformMakeWithComponents(components) }
    @_alwaysEmitIntoClient public var isIdentity: Bool { CGAffineTransformIsIdentity(self) }
    @_alwaysEmitIntoClient public func translatedBy(x: CGFloat, y: CGFloat) -> CGAffineTransform { CGAffineTransformTranslate(self, x, y) }
    @_alwaysEmitIntoClient public func scaledBy(x: CGFloat, y: CGFloat) -> CGAffineTransform { CGAffineTransformScale(self, x, y) }
    @_alwaysEmitIntoClient public func rotated(by angle: CGFloat) -> CGAffineTransform { CGAffineTransformRotate(self, angle) }
    @_alwaysEmitIntoClient public func inverted() -> CGAffineTransform { CGAffineTransformInvert(self) }
    @_alwaysEmitIntoClient public func concatenating(_ t: CGAffineTransform) -> CGAffineTransform { CGAffineTransformConcat(self, t) }
    @_alwaysEmitIntoClient public func decomposed() -> CGAffineTransformComponents { CGAffineTransformDecompose(self) }
}
