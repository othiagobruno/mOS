// mOS — overlay Foundation: auxiliares do Foundation da Apple que o swift-foundation não traz fora do
// FOUNDATION_FRAMEWORK. NSRange <-> Range, NSNumber -> inteiros/CGFloat, posição do FileHandle e o errorCode padrão
// de CustomNSError com RawValue inteiro. Assinaturas e nomes da documentação pública da Apple.

import Darwin
import CoreGraphics

// MARK: - NSRange

extension NSRange {
    /// NSNotFound na localização: a faixa não existe
    internal static var _mosNotFound: Int { Int.max }

    public init<R: RangeExpression>(_ region: R) where R.Bound: FixedWidthInteger {
        let r = region.relative(to: 0..<R.Bound.max)
        self.init(location: Int(r.lowerBound), length: Int(r.upperBound - r.lowerBound))
    }

    /// a faixa em UTF-16 de `region` dentro de `target`
    public init<R: RangeExpression, S: StringProtocol>(_ region: R, in target: S) where R.Bound == String.Index {
        let r = region.relative(to: target)
        let u = target.utf16
        let lo = u.distance(from: u.startIndex, to: r.lowerBound)
        let n = u.distance(from: r.lowerBound, to: r.upperBound)
        self.init(location: lo, length: n)
    }

    public func intersection(_ other: NSRange) -> NSRange? {
        let lo = Swift.max(location, other.location)
        let hi = Swift.min(location + length, other.location + other.length)
        return lo <= hi ? NSRange(location: lo, length: hi - lo) : nil
    }
}

extension Range where Bound == String.Index {
    /// nil quando a faixa sai da string ou corta um par substituto ao meio
    public init?(_ range: NSRange, in string: __shared String) {
        if range.location == NSRange._mosNotFound { return nil }
        let u = string.utf16
        guard range.location >= 0, range.length >= 0, range.location + range.length <= u.count,
              let lo = u.index(u.startIndex, offsetBy: range.location, limitedBy: u.endIndex),
              let hi = u.index(lo, offsetBy: range.length, limitedBy: u.endIndex),
              let l = String.Index(lo, within: string), let h = String.Index(hi, within: string) else { return nil }
        self = l..<h
    }
}

// MARK: - NSNumber

extension Int32 {
    public init?(exactly number: __shared NSNumber) {
        let d = number.doubleValue
        guard let v = Int32(exactly: d) else { return nil }
        self = v
    }
}

extension CGFloat {
    public init(truncating number: __shared NSNumber) { self.init(number.doubleValue) }
}

// MARK: - FileHandle

extension FileHandle {
    public func offset() throws -> UInt64 {
        let r = lseek(fileDescriptor, 0, SEEK_CUR)
        if r < 0 { throw POSIXError(POSIXErrorCode(rawValue: errno) ?? .EIO) }
        return UInt64(r)
    }

    @discardableResult
    public func seekToEnd() throws -> UInt64 {
        let r = lseek(fileDescriptor, 0, SEEK_END)
        if r < 0 { throw POSIXError(POSIXErrorCode(rawValue: errno) ?? .EIO) }
        return UInt64(r)
    }

    public func seek(toOffset offset: UInt64) throws {
        if lseek(fileDescriptor, off_t(offset), SEEK_SET) < 0 { throw POSIXError(POSIXErrorCode(rawValue: errno) ?? .EIO) }
    }
}

// MARK: - CustomNSError

extension CustomNSError where Self: RawRepresentable, Self.RawValue: FixedWidthInteger {
    /// o código é o valor bruto, como na Apple
    public var errorCode: Int { numericCast(self.rawValue) }
}

// MARK: - NSRange e geometria como valores (Equatable, Hashable, descrição e ponte com NSValue)

extension NSRange: Equatable, Hashable, CustomStringConvertible {
    public static func == (a: NSRange, b: NSRange) -> Bool { a.location == b.location && a.length == b.length }
    public func hash(into h: inout Hasher) { h.combine(location); h.combine(length) }
    public var description: String { "{\(location), \(length)}" }
}

extension NSRange: _ObjectiveCBridgeable {
    public func _bridgeToObjectiveC() -> NSValue { NSValue(range: self) }
    public static func _forceBridgeFromObjectiveC(_ x: NSValue, result: inout NSRange?) { result = x.rangeValue }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSValue, result: inout NSRange?) -> Bool {
        result = x.rangeValue; return true
    }
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSValue?) -> NSRange {
        source?.rangeValue ?? NSRange(location: 0, length: 0)
    }
}

extension CGPoint: _ObjectiveCBridgeable {
    public func _bridgeToObjectiveC() -> NSValue { NSValue(point: self) }
    public static func _forceBridgeFromObjectiveC(_ x: NSValue, result: inout CGPoint?) { result = x.pointValue }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSValue, result: inout CGPoint?) -> Bool { result = x.pointValue; return true }
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSValue?) -> CGPoint { source?.pointValue ?? CGPoint() }
}

extension CGSize: _ObjectiveCBridgeable {
    public func _bridgeToObjectiveC() -> NSValue { NSValue(size: self) }
    public static func _forceBridgeFromObjectiveC(_ x: NSValue, result: inout CGSize?) { result = x.sizeValue }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSValue, result: inout CGSize?) -> Bool { result = x.sizeValue; return true }
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSValue?) -> CGSize { source?.sizeValue ?? CGSize() }
}

extension CGRect: _ObjectiveCBridgeable {
    public func _bridgeToObjectiveC() -> NSValue { NSValue(rect: self) }
    public static func _forceBridgeFromObjectiveC(_ x: NSValue, result: inout CGRect?) { result = x.rectValue }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSValue, result: inout CGRect?) -> Bool { result = x.rectValue; return true }
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSValue?) -> CGRect { source?.rectValue ?? CGRect() }
}
