// mOS — a API Swift do overlay CoreGraphics da Apple para os tipos CF do CoreGraphics (CGPath, CGMutablePath, CGColor,
// CGColorSpace, CGImage, CGContext), sobre as funções C do libSystem (userland/libsystem/coregraphics.c). Na Apple
// quase tudo vem das API notes; aqui é Swift @_alwaysEmitIntoClient (sem símbolo próprio, como na Apple). As funções de
// CGImage/CGContext que o cabeçalho do mOS ainda não declara vão por @_silgen_name.

// As funções CG*Create*/Copy* do cabeçalho do mOS não são auditadas (devolvem Unmanaged, +1).
@_alwaysEmitIntoClient public func _mosTake<T: AnyObject>(_ u: Unmanaged<T>) -> T { u.takeRetainedValue() }
@_alwaysEmitIntoClient public func _mosTake<T: AnyObject>(_ u: Unmanaged<T>?) -> T? { u?.takeRetainedValue() }
@_alwaysEmitIntoClient public func _mosTake<T: AnyObject>(_ v: T) -> T { v }
@_alwaysEmitIntoClient public func _mosTake<T: AnyObject>(_ v: T?) -> T? { v }

// MARK: - CGColorSpace

extension _MOSCFFactoryInitializable where Self == CGColorSpace {
    @_alwaysEmitIntoClient public init?(name: CFString) {
        guard let s = _mosTake(CGColorSpaceCreateWithName(name)) else { return nil }
        self = s
    }
}
extension CGColorSpace {
    @_alwaysEmitIntoClient public static var sRGB: CFString { kCGColorSpaceSRGB }
    @_alwaysEmitIntoClient public static var extendedSRGB: CFString { kCGColorSpaceExtendedSRGB }
    @_alwaysEmitIntoClient public static var linearSRGB: CFString { kCGColorSpaceLinearSRGB }
    @_alwaysEmitIntoClient public static var extendedLinearSRGB: CFString { kCGColorSpaceExtendedLinearSRGB }
    @_alwaysEmitIntoClient public static var displayP3: CFString { kCGColorSpaceDisplayP3 }
    @_alwaysEmitIntoClient public static var genericRGB: CFString { kCGColorSpaceGenericRGB }
    @_alwaysEmitIntoClient public static var genericGray: CFString { kCGColorSpaceGenericGray }
    @_alwaysEmitIntoClient public static var genericGrayGamma2_2: CFString { kCGColorSpaceGenericGrayGamma2_2 }
        @_alwaysEmitIntoClient public var name: CFString? { CGColorSpaceGetName(self)?.takeUnretainedValue() }
    @_alwaysEmitIntoClient public var model: CGColorSpaceModel { CGColorSpaceGetModel(self) }
    @_alwaysEmitIntoClient public var numberOfComponents: Int { Int(CGColorSpaceGetNumberOfComponents(self)) }
    @_alwaysEmitIntoClient public var isWideGamutRGB: Bool { CGColorSpaceIsWideGamutRGB(self) }
}

// `init(_mosRetained:)`: o "init" de uma classe CF importada só pode devolver a mesma instância. Os tipos CF do mOS são
// objetos ObjC, então o padrão `self = unsafeDowncast(obj)` é feito por um init de fábrica (como os do SDK da Apple).
public protocol _MOSCFFactoryInitializable {}
extension _MOSCFFactoryInitializable {
    @_alwaysEmitIntoClient public init(_mosRetained object: Self) { self = object }
}
extension CGColorSpace: _MOSCFFactoryInitializable {}
extension CGColor: _MOSCFFactoryInitializable {}
extension CGPath: _MOSCFFactoryInitializable {}

// MARK: - CGColor

extension _MOSCFFactoryInitializable where Self == CGColor {
    @_alwaysEmitIntoClient public init?(colorSpace space: CGColorSpace, components: UnsafePointer<CGFloat>) {
        guard let c = _mosTake(CGColorCreate(space, components)) else { return nil }
        self = c
    }
    @_alwaysEmitIntoClient public init(red: CGFloat, green: CGFloat, blue: CGFloat, alpha: CGFloat) {
        self = _mosTake(CGColorCreateSRGB(red, green, blue, alpha))
    }
    @_alwaysEmitIntoClient public init(srgbRed red: CGFloat, green: CGFloat, blue: CGFloat, alpha: CGFloat) {
        self = _mosTake(CGColorCreateSRGB(red, green, blue, alpha))
    }
    @_alwaysEmitIntoClient public init(gray: CGFloat, alpha: CGFloat) {
        self = _mosTake(CGColorCreateGenericGray(gray, alpha))
    }
}
extension CGColor {
    @_alwaysEmitIntoClient public var alpha: CGFloat { CGColorGetAlpha(self) }
    @_alwaysEmitIntoClient public var colorSpace: CGColorSpace? { CGColorGetColorSpace(self)?.takeUnretainedValue() }
    @_alwaysEmitIntoClient public var numberOfComponents: Int { Int(CGColorGetNumberOfComponents(self)) }
    // Símbolo de dylib (ABI estável). O OpenBot importa este getter de libswiftCoreGraphics;
    // @_alwaysEmitIntoClient não o exportava.
    public var components: [CGFloat]? {
        guard let p = CGColorGetComponents(self) else { return nil }
        return Array(UnsafeBufferPointer(start: p, count: numberOfComponents))
    }
    @_alwaysEmitIntoClient public func copy() -> CGColor? { _mosTake(CGColorCreateCopy(self)) }
    @_alwaysEmitIntoClient public func copy(alpha: CGFloat) -> CGColor? { _mosTake(CGColorCreateCopyWithAlpha(self, alpha)) }
    /// Conversão de espaço de cor: o mOS guarda as cores em sRGB, então a cópia vale para qualquer destino RGB.
    @_alwaysEmitIntoClient
    public func converted(to space: CGColorSpace, intent: CGColorRenderingIntent, options: CFDictionary?) -> CGColor? {
        if space.numberOfComponents == numberOfComponents - 1 || space.model == colorSpace?.model { return copy() }
        guard let c = components, c.count >= 2, space.numberOfComponents >= 3 else { return copy() }
        if c.count == 2 { return CGColor(srgbRed: c[0], green: c[0], blue: c[0], alpha: c[1]) }
        return copy()
    }
}

// MARK: - CGPath / CGMutablePath

extension _MOSCFFactoryInitializable where Self == CGPath {
    @_alwaysEmitIntoClient public init(rect: CGRect, transform: UnsafePointer<CGAffineTransform>?) {
        self = _mosTake(CGPathCreateWithRect(rect, transform))
    }
    @_alwaysEmitIntoClient public init(ellipseIn rect: CGRect, transform: UnsafePointer<CGAffineTransform>?) {
        self = _mosTake(CGPathCreateWithEllipseInRect(rect, transform))
    }
    @_alwaysEmitIntoClient
    public init(roundedRect rect: CGRect, cornerWidth: CGFloat, cornerHeight: CGFloat, transform: UnsafePointer<CGAffineTransform>?) {
        self = _mosTake(CGPathCreateWithRoundedRect(rect, cornerWidth, cornerHeight, transform))
    }
}
extension CGPath {
    @_alwaysEmitIntoClient public var isEmpty: Bool { CGPathIsEmpty(self) }
    @_alwaysEmitIntoClient public var boundingBox: CGRect { CGPathGetBoundingBox(self) }
    @_alwaysEmitIntoClient public var boundingBoxOfPath: CGRect { CGPathGetPathBoundingBox(self) }
    @_alwaysEmitIntoClient public var currentPoint: CGPoint { CGPathGetCurrentPoint(self) }
    @_alwaysEmitIntoClient public func copy() -> CGPath? { _mosTake(CGPathCreateCopy(self)) }
    @_alwaysEmitIntoClient public func copy(using transform: UnsafePointer<CGAffineTransform>?) -> CGPath? {
        _mosTake(CGPathCreateCopyByTransformingPath(self, transform))
    }
    @_alwaysEmitIntoClient public func mutableCopy() -> CGMutablePath? { _mosTake(CGPathCreateMutableCopy(self)) }
    @_alwaysEmitIntoClient public func mutableCopy(using transform: UnsafePointer<CGAffineTransform>?) -> CGMutablePath? {
        _mosTake(CGPathCreateMutableCopyByTransformingPath(self, transform))
    }
    @_alwaysEmitIntoClient public func applyWithBlock(_ block: (UnsafePointer<CGPathElement>) -> Void) {
        withoutActuallyEscaping(block) { b in CGPathApplyWithBlock(self) { b($0) } }
    }
    @_alwaysEmitIntoClient public func isRect(_ rect: UnsafeMutablePointer<CGRect>?) -> Bool { CGPathIsRect(self, rect) }
    @_alwaysEmitIntoClient public static func == (lhs: CGPath, rhs: CGPath) -> Bool { CGPathEqualToPath(lhs, rhs) }
}

extension _MOSCFFactoryInitializable where Self == CGMutablePath {
    @_alwaysEmitIntoClient public init() { self = _mosTake(CGPathCreateMutable()) }
}
extension CGMutablePath {
    @_alwaysEmitIntoClient public func addLine(to point: CGPoint, transform: CGAffineTransform = .identity) {
        var t = transform; CGPathAddLineToPoint(self, &t, point.x, point.y)
    }
    @_alwaysEmitIntoClient public func addQuadCurve(to end: CGPoint, control: CGPoint, transform: CGAffineTransform = .identity) {
        var t = transform; CGPathAddQuadCurveToPoint(self, &t, control.x, control.y, end.x, end.y)
    }
    @_alwaysEmitIntoClient
    public func addCurve(to end: CGPoint, control1: CGPoint, control2: CGPoint, transform: CGAffineTransform = .identity) {
        var t = transform; CGPathAddCurveToPoint(self, &t, control1.x, control1.y, control2.x, control2.y, end.x, end.y)
    }
    @_alwaysEmitIntoClient public func closeSubpath() { CGPathCloseSubpath(self) }
    @_alwaysEmitIntoClient public func addRect(_ rect: CGRect, transform: CGAffineTransform = .identity) {
        var t = transform; CGPathAddRect(self, &t, rect)
    }
    @_alwaysEmitIntoClient public func addRects(_ rects: [CGRect], transform: CGAffineTransform = .identity) {
        for r in rects { addRect(r, transform: transform) }
    }
    @_alwaysEmitIntoClient public func addEllipse(in rect: CGRect, transform: CGAffineTransform = .identity) {
        var t = transform; CGPathAddEllipseInRect(self, &t, rect)
    }
    @_alwaysEmitIntoClient
    public func addRoundedRect(in rect: CGRect, cornerWidth: CGFloat, cornerHeight: CGFloat, transform: CGAffineTransform = .identity) {
        var t = transform; CGPathAddRoundedRect(self, &t, rect, cornerWidth, cornerHeight)
    }
    @_alwaysEmitIntoClient
    public func addArc(center: CGPoint, radius: CGFloat, startAngle: CGFloat, endAngle: CGFloat, clockwise: Bool,
                       transform: CGAffineTransform = .identity) {
        var t = transform; CGPathAddArc(self, &t, center.x, center.y, radius, startAngle, endAngle, clockwise)
    }
    @_alwaysEmitIntoClient
    public func addRelativeArc(center: CGPoint, radius: CGFloat, startAngle: CGFloat, delta: CGFloat, transform: CGAffineTransform = .identity) {
        var t = transform; CGPathAddRelativeArc(self, &t, center.x, center.y, radius, startAngle, delta)
    }
    @_alwaysEmitIntoClient public func addPath(_ path: CGPath, transform: CGAffineTransform = .identity) {
        var t = transform; CGPathAddPath(self, &t, path)
    }
    @_alwaysEmitIntoClient public func addLines(between points: [CGPoint], transform: CGAffineTransform = .identity) {
        guard let first = points.first else { return }
        move(to: first, transform: transform)
        for p in points.dropFirst() { addLine(to: p, transform: transform) }
    }
}

// MARK: - CGImage / CGContext (funções que o cabeçalho do mOS ainda não declara)

@_silgen_name("CGImageGetWidth") public func _mosCGImageGetWidth(_ image: CGImage) -> Int
@_silgen_name("CGImageGetHeight") public func _mosCGImageGetHeight(_ image: CGImage) -> Int
@_silgen_name("CGImageGetColorSpace") public func _mosCGImageGetColorSpace(_ image: CGImage) -> Unmanaged<CGColorSpace>?
@_silgen_name("CGContextScaleCTM") public func _mosCGContextScaleCTM(_ c: CGContext, _ sx: CGFloat, _ sy: CGFloat)
@_silgen_name("CGContextTranslateCTM") public func _mosCGContextTranslateCTM(_ c: CGContext, _ tx: CGFloat, _ ty: CGFloat)
@_silgen_name("CGContextConcatCTM") public func _mosCGContextConcatCTM(_ c: CGContext, _ t: CGAffineTransform)
@_silgen_name("CGContextSaveGState") public func _mosCGContextSaveGState(_ c: CGContext)
@_silgen_name("CGContextRestoreGState") public func _mosCGContextRestoreGState(_ c: CGContext)
@_silgen_name("CGBitmapContextCreateImage") public func _mosCGBitmapContextCreateImage(_ c: CGContext) -> Unmanaged<CGImage>?

extension CGImage {
    @_alwaysEmitIntoClient public var width: Int { _mosCGImageGetWidth(self) }
    @_alwaysEmitIntoClient public var height: Int { _mosCGImageGetHeight(self) }
    @_alwaysEmitIntoClient public var colorSpace: CGColorSpace? { _mosCGImageGetColorSpace(self)?.takeUnretainedValue() }
}

extension CGContext {
    @_alwaysEmitIntoClient public func scaleBy(x: CGFloat, y: CGFloat) { _mosCGContextScaleCTM(self, x, y) }
    @_alwaysEmitIntoClient public func translateBy(x: CGFloat, y: CGFloat) { _mosCGContextTranslateCTM(self, x, y) }
    @_alwaysEmitIntoClient public func concatenate(_ t: CGAffineTransform) { _mosCGContextConcatCTM(self, t) }
    @_alwaysEmitIntoClient public func saveGState() { _mosCGContextSaveGState(self) }
    @_alwaysEmitIntoClient public func restoreGState() { _mosCGContextRestoreGState(self) }
    @_alwaysEmitIntoClient public func makeImage() -> CGImage? { _mosCGBitmapContextCreateImage(self)?.takeRetainedValue() }
}
