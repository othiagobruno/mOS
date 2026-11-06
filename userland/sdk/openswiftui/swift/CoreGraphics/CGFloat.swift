// mOS SDK — módulo CoreGraphics (Swift): CGFloat (Double no x86_64), com a API do CoreGraphics.
// Ser o módulo "CoreGraphics" importa por dois motivos: o importador clang mapeia o typedef C CGFloat (dos tipos de
// CFCGTypes.h no CoreFoundation do mOS) para este tipo, e o compilador aplica a conversão implícita CGFloat <-> Double
// (SE-0307). O Foundation do mOS o reexporta.

@_exported import MOSLibm

@frozen
public struct CGFloat: Sendable {
    public typealias NativeType = Double
    public var native: NativeType

    @_transparent public init() { native = 0 }
    @_transparent public init(_ value: Float) { native = NativeType(value) }
    @_transparent public init(_ value: Double) { native = value }
    @_transparent public init(_ value: CGFloat) { native = value.native }
    @_transparent public init(_ value: Int) { native = NativeType(value) }
    @_transparent public init(_ value: UInt) { native = NativeType(value) }
    @_transparent public init(_ value: Int8) { native = NativeType(value) }
    @_transparent public init(_ value: Int16) { native = NativeType(value) }
    @_transparent public init(_ value: Int32) { native = NativeType(value) }
    @_transparent public init(_ value: Int64) { native = NativeType(value) }
    @_transparent public init(_ value: UInt8) { native = NativeType(value) }
    @_transparent public init(_ value: UInt16) { native = NativeType(value) }
    @_transparent public init(_ value: UInt32) { native = NativeType(value) }
    @_transparent public init(_ value: UInt64) { native = NativeType(value) }

    public static var bitWidth: Int { 64 }
}

extension CGFloat: SignedNumeric {
    @_transparent public init?<T: BinaryInteger>(exactly source: T) {
        guard let d = NativeType(exactly: source) else { return nil }
        native = d
    }
    @_transparent public var magnitude: CGFloat { CGFloat(native.magnitude) }
}

extension CGFloat: BinaryFloatingPoint {
    public typealias RawSignificand = UInt64
    public typealias RawExponent = UInt
    public typealias Exponent = Int

    @_transparent public static var exponentBitCount: Int { NativeType.exponentBitCount }
    @_transparent public static var significandBitCount: Int { NativeType.significandBitCount }

    @_transparent public var bitPattern: UInt64 { native.bitPattern }
    @_transparent public init(bitPattern: UInt64) { native = NativeType(bitPattern: bitPattern) }

    @_transparent public var sign: FloatingPointSign { native.sign }
    @_transparent public var exponentBitPattern: UInt { native.exponentBitPattern }
    @_transparent public var significandBitPattern: UInt64 { native.significandBitPattern }

    @_transparent public init(sign: FloatingPointSign, exponentBitPattern: UInt, significandBitPattern: UInt64) {
        native = NativeType(sign: sign, exponentBitPattern: exponentBitPattern, significandBitPattern: significandBitPattern)
    }
    @_transparent public init(nan payload: RawSignificand, signaling: Bool) { native = NativeType(nan: payload, signaling: signaling) }

    @_transparent public static var infinity: CGFloat { CGFloat(NativeType.infinity) }
    @_transparent public static var nan: CGFloat { CGFloat(NativeType.nan) }
    @_transparent public static var signalingNaN: CGFloat { CGFloat(NativeType.signalingNaN) }
    @_transparent public static var greatestFiniteMagnitude: CGFloat { CGFloat(NativeType.greatestFiniteMagnitude) }
    @_transparent public static var pi: CGFloat { CGFloat(NativeType.pi) }
    @_transparent public static var leastNormalMagnitude: CGFloat { CGFloat(NativeType.leastNormalMagnitude) }
    @_transparent public static var leastNonzeroMagnitude: CGFloat { CGFloat(NativeType.leastNonzeroMagnitude) }

    @_transparent public var ulp: CGFloat { CGFloat(native.ulp) }
    @_transparent public var exponent: Int { native.exponent }
    @_transparent public var significand: CGFloat { CGFloat(native.significand) }
    @_transparent public var nextUp: CGFloat { CGFloat(native.nextUp) }
    @_transparent public var binade: CGFloat { CGFloat(native.binade) }
    @_transparent public var significandWidth: Int { native.significandWidth }

    @_transparent public init(sign: FloatingPointSign, exponent: Int, significand: CGFloat) {
        native = NativeType(sign: sign, exponent: exponent, significand: significand.native)
    }
    @_transparent public init<Source: BinaryInteger>(_ value: Source) { native = NativeType(value) }
    @_transparent public init<Source: BinaryFloatingPoint>(_ value: Source) { native = NativeType(value) }
    @_transparent public init?<Source: BinaryFloatingPoint>(exactly value: Source) {
        guard let d = NativeType(exactly: value) else { return nil }
        native = d
    }

    @_transparent public var isNormal: Bool { native.isNormal }
    @_transparent public var isFinite: Bool { native.isFinite }
    @_transparent public var isZero: Bool { native.isZero }
    @_transparent public var isSubnormal: Bool { native.isSubnormal }
    @_transparent public var isInfinite: Bool { native.isInfinite }
    @_transparent public var isNaN: Bool { native.isNaN }
    @_transparent public var isSignalingNaN: Bool { native.isSignalingNaN }
    @_transparent public var isCanonical: Bool { native.isCanonical }

    @_transparent public mutating func round(_ rule: FloatingPointRoundingRule) { native.round(rule) }
    @_transparent public mutating func negate() { native.negate() }
    @_transparent public static func + (l: CGFloat, r: CGFloat) -> CGFloat { CGFloat(l.native + r.native) }
    @_transparent public static func - (l: CGFloat, r: CGFloat) -> CGFloat { CGFloat(l.native - r.native) }
    @_transparent public static func * (l: CGFloat, r: CGFloat) -> CGFloat { CGFloat(l.native * r.native) }
    @_transparent public static func / (l: CGFloat, r: CGFloat) -> CGFloat { CGFloat(l.native / r.native) }
    @_transparent public static func += (l: inout CGFloat, r: CGFloat) { l.native += r.native }
    @_transparent public static func -= (l: inout CGFloat, r: CGFloat) { l.native -= r.native }
    @_transparent public static func *= (l: inout CGFloat, r: CGFloat) { l.native *= r.native }
    @_transparent public static func /= (l: inout CGFloat, r: CGFloat) { l.native /= r.native }
    @_transparent public mutating func formRemainder(dividingBy other: CGFloat) { native.formRemainder(dividingBy: other.native) }
    @_transparent public mutating func formTruncatingRemainder(dividingBy other: CGFloat) { native.formTruncatingRemainder(dividingBy: other.native) }
    @_transparent public mutating func formSquareRoot() { native.formSquareRoot() }
    @_transparent public mutating func addProduct(_ lhs: CGFloat, _ rhs: CGFloat) { native.addProduct(lhs.native, rhs.native) }
    @_transparent public func isEqual(to other: CGFloat) -> Bool { native.isEqual(to: other.native) }
    @_transparent public func isLess(than other: CGFloat) -> Bool { native.isLess(than: other.native) }
    @_transparent public func isLessThanOrEqualTo(_ other: CGFloat) -> Bool { native.isLessThanOrEqualTo(other.native) }
    @_transparent public func isTotallyOrdered(belowOrEqualTo other: CGFloat) -> Bool { native.isTotallyOrdered(belowOrEqualTo: other.native) }
}

extension CGFloat: ExpressibleByIntegerLiteral, ExpressibleByFloatLiteral {
    @_transparent public init(integerLiteral value: Int) { native = NativeType(value) }
    @_transparent public init(floatLiteral value: Double) { native = value }
}

extension CGFloat: Hashable {
    @_transparent public static func == (l: CGFloat, r: CGFloat) -> Bool { l.native == r.native }
    @_transparent public static func < (l: CGFloat, r: CGFloat) -> Bool { l.native < r.native }
    public func hash(into hasher: inout Hasher) { hasher.combine(native) }
}

extension CGFloat: Strideable {
    @_transparent public func distance(to other: CGFloat) -> CGFloat { CGFloat(other.native - native) }
    @_transparent public func advanced(by amount: CGFloat) -> CGFloat { CGFloat(native + amount.native) }
}

extension CGFloat: CustomStringConvertible, CustomDebugStringConvertible, LosslessStringConvertible {
    public var description: String { native.description }
    public var debugDescription: String { native.debugDescription }
    public init?(_ text: String) {
        guard let d = Double(text) else { return nil }
        native = d
    }
}

extension CGFloat: Codable {
    public init(from decoder: any Decoder) throws { native = try decoder.singleValueContainer().decode(Double.self) }
    public func encode(to encoder: any Encoder) throws { var c = encoder.singleValueContainer(); try c.encode(native) }
}

extension Double {
    @_transparent public init(_ value: CGFloat) { self = value.native }
}
extension Float {
    @_transparent public init(_ value: CGFloat) { self = Float(value.native) }
}
extension Int {
    @_transparent public init(_ value: CGFloat) { self = Int(value.native) }
}

// MARK: - funções matemáticas para CGFloat (como no overlay do CoreGraphics)

@_transparent public func sin(_ x: CGFloat) -> CGFloat { CGFloat(sin(x.native)) }
@_transparent public func cos(_ x: CGFloat) -> CGFloat { CGFloat(cos(x.native)) }
@_transparent public func tan(_ x: CGFloat) -> CGFloat { CGFloat(tan(x.native)) }
@_transparent public func asin(_ x: CGFloat) -> CGFloat { CGFloat(asin(x.native)) }
@_transparent public func acos(_ x: CGFloat) -> CGFloat { CGFloat(acos(x.native)) }
@_transparent public func atan(_ x: CGFloat) -> CGFloat { CGFloat(atan(x.native)) }
@_transparent public func atan2(_ y: CGFloat, _ x: CGFloat) -> CGFloat { CGFloat(atan2(y.native, x.native)) }
@_transparent public func sqrt(_ x: CGFloat) -> CGFloat { CGFloat(x.native.squareRoot()) }
@_transparent public func hypot(_ x: CGFloat, _ y: CGFloat) -> CGFloat { CGFloat(hypot(x.native, y.native)) }
@_transparent public func pow(_ x: CGFloat, _ y: CGFloat) -> CGFloat { CGFloat(pow(x.native, y.native)) }
@_transparent public func exp(_ x: CGFloat) -> CGFloat { CGFloat(exp(x.native)) }
@_transparent public func log(_ x: CGFloat) -> CGFloat { CGFloat(log(x.native)) }
@_transparent public func log2(_ x: CGFloat) -> CGFloat { CGFloat(log2(x.native)) }
@_transparent public func floor(_ x: CGFloat) -> CGFloat { CGFloat(x.native.rounded(.down)) }
@_transparent public func ceil(_ x: CGFloat) -> CGFloat { CGFloat(x.native.rounded(.up)) }
@_transparent public func round(_ x: CGFloat) -> CGFloat { CGFloat(x.native.rounded()) }
@_transparent public func trunc(_ x: CGFloat) -> CGFloat { CGFloat(x.native.rounded(.towardZero)) }
@_transparent public func fmod(_ x: CGFloat, _ y: CGFloat) -> CGFloat { CGFloat(x.native.truncatingRemainder(dividingBy: y.native)) }
@_transparent public func copysign(_ x: CGFloat, _ y: CGFloat) -> CGFloat { CGFloat(Double(signOf: y.native, magnitudeOf: x.native)) }
@_transparent public func fabs(_ x: CGFloat) -> CGFloat { CGFloat(x.native.magnitude) }
