// mOS — NSAttributedString/NSMutableAttributedString do texto do SwiftUI.framework (variante com interop ObjC).
// Cópia do NSAttributedString em Swift puro da variante nativa (userland/sdk/openswiftui/swift/Foundation/
// AttributedString.swift): o NSAttributedString do libSystem (appkit.c) guarda um dicionário só e o layout de texto do
// OpenSwiftUI precisa de trechos com atributos. O módulo MOSFoundationExtras tem nome ABI "Foundation", mas estas são
// classes Swift (o nome ObjC é _TtC10Foundation18NSAttributedString), sem colidir com a do libSystem.
import Foundation
@_exported import _MOSFoundationSwiftUI   // Bundle, Formatter, RunLoop... (classes ObjC do libSystem)

// MARK: - NSAttributedString

extension NSAttributedString {
    public struct Key: RawRepresentable, Hashable, Sendable, ExpressibleByStringLiteral, CustomStringConvertible {
        public var rawValue: String
        public init(_ rawValue: String) { self.rawValue = rawValue }
        public init(rawValue: String) { self.rawValue = rawValue }
        public init(stringLiteral value: String) { rawValue = value }
        public var description: String { rawValue }
    }
    public struct EnumerationOptions: OptionSet, Sendable {
        public let rawValue: UInt
        public init(rawValue: UInt) { self.rawValue = rawValue }
        public static let reverse = EnumerationOptions(rawValue: 1 << 1)
        public static let longestEffectiveRangeNotRequired = EnumerationOptions(rawValue: 1 << 20)
    }
}

extension NSAttributedString.Key {
    public static let font = NSAttributedString.Key("NSFont")
    public static let foregroundColor = NSAttributedString.Key("NSColor")
    public static let backgroundColor = NSAttributedString.Key("NSBackgroundColor")
    public static let paragraphStyle = NSAttributedString.Key("NSParagraphStyle")
    public static let kern = NSAttributedString.Key("NSKern")
    public static let tracking = NSAttributedString.Key("NSTracking")
    public static let ligature = NSAttributedString.Key("NSLigature")
    public static let baselineOffset = NSAttributedString.Key("NSBaselineOffset")
    public static let underlineStyle = NSAttributedString.Key("NSUnderline")
    public static let underlineColor = NSAttributedString.Key("NSUnderlineColor")
    public static let strikethroughStyle = NSAttributedString.Key("NSStrikethrough")
    public static let strikethroughColor = NSAttributedString.Key("NSStrikethroughColor")
    public static let link = NSAttributedString.Key("NSLink")
    public static let shadow = NSAttributedString.Key("NSShadow")
    public static let attachment = NSAttributedString.Key("NSAttachment")
}

/// Atributos guardados como trechos {intervalo UTF-16, dicionário}. Trechos vizinhos com atributos iguais
/// não são fundidos (enumerate devolve os trechos como estão), o que basta para o layout de texto.
open class NSAttributedString: NSObject, @unchecked Sendable {
    struct Run { var range: NSRange; var attributes: [NSAttributedString.Key: Any] }
    var _string: String
    var _utf16: [UInt16]
    var _runs: [Run]

    public override init() { _string = ""; _utf16 = []; _runs = []; super.init() }
    public init(string: String) {
        _string = string; _utf16 = Array(string.utf16)
        _runs = _utf16.isEmpty ? [] : [Run(range: NSRange(location: 0, length: _utf16.count), attributes: [:])]
        super.init()
    }
    public init(string: String, attributes: [NSAttributedString.Key: Any]?) {
        _string = string; _utf16 = Array(string.utf16)
        _runs = _utf16.isEmpty ? [] : [Run(range: NSRange(location: 0, length: _utf16.count), attributes: attributes ?? [:])]
        super.init()
    }
    public init(attributedString other: NSAttributedString) {
        _string = other._string; _utf16 = other._utf16; _runs = other._runs
        super.init()
    }

    open var string: String { _string }
    open var length: Int { _utf16.count }

    private func runIndex(at location: Int) -> Int? { _runs.firstIndex { $0.range.contains(location) } }

    open func attributes(at location: Int, effectiveRange range: UnsafeMutablePointer<NSRange>?) -> [NSAttributedString.Key: Any] {
        guard let i = runIndex(at: location) else {
            range?.pointee = NSRange(location: NSNotFound, length: 0)
            return [:]
        }
        range?.pointee = _runs[i].range
        return _runs[i].attributes
    }
    open func attribute(_ name: NSAttributedString.Key, at location: Int, effectiveRange range: UnsafeMutablePointer<NSRange>?) -> Any? {
        attributes(at: location, effectiveRange: range)[name]
    }
    open func attributes(at location: Int, longestEffectiveRange range: UnsafeMutablePointer<NSRange>?, in rangeLimit: NSRange) -> [NSAttributedString.Key: Any] {
        var r = NSRange()
        let a = attributes(at: location, effectiveRange: &r)
        range?.pointee = r.intersection(rangeLimit) ?? NSRange(location: location, length: 0)
        return a
    }
    open func attributedSubstring(from range: NSRange) -> NSAttributedString {
        let out = NSAttributedString()
        let lo = Swift.max(0, range.location), hi = Swift.min(length, range.upperBound)
        guard hi > lo else { return out }
        out._utf16 = Array(_utf16[lo ..< hi])
        out._string = String(decoding: out._utf16, as: UTF16.self)
        for run in _runs {
            guard let x = run.range.intersection(NSRange(location: lo, length: hi - lo)), x.length > 0 else { continue }
            out._runs.append(Run(range: NSRange(location: x.location - lo, length: x.length), attributes: run.attributes))
        }
        return out
    }
    open func enumerateAttributes(in enumerationRange: NSRange, options opts: EnumerationOptions = [],
                                  using block: ([NSAttributedString.Key: Any], NSRange, UnsafeMutablePointer<ObjCBool>) -> Void) {
        var pieces: [(NSRange, [NSAttributedString.Key: Any])] = []
        for run in _runs {
            if let x = run.range.intersection(enumerationRange), x.length > 0 { pieces.append((x, run.attributes)) }
        }
        if opts.contains(.reverse) { pieces.reverse() }
        var stop = ObjCBool(false)
        for (range, attrs) in pieces {
            block(attrs, range, &stop)
            if stop.boolValue { break }
        }
    }
    open func enumerateAttribute(_ attrName: NSAttributedString.Key, in enumerationRange: NSRange, options opts: EnumerationOptions = [],
                                 using block: (Any?, NSRange, UnsafeMutablePointer<ObjCBool>) -> Void) {
        enumerateAttributes(in: enumerationRange, options: opts) { attrs, range, stop in block(attrs[attrName], range, stop) }
    }
    open func isEqual(to other: NSAttributedString) -> Bool {
        guard _string == other._string, _runs.count == other._runs.count else { return false }
        for (a, b) in zip(_runs, other._runs) where a.range != b.range || Set(a.attributes.keys) != Set(b.attributes.keys) { return false }
        return true
    }
    open override func isEqual(_ object: Any?) -> Bool { (object as? NSAttributedString).map(isEqual(to:)) ?? false }
    open override var hash: Int { _string.hashValue }
    open override func copy() -> Any { NSAttributedString(attributedString: self) }
    open override func mutableCopy() -> Any { NSMutableAttributedString(attributedString: self) }
}

open class NSMutableAttributedString: NSAttributedString, @unchecked Sendable {
    open var mutableString: String { _string }

    /// Divide trechos para que `range` caia em fronteiras e devolve os índices dos trechos dentro dele.
    private func split(_ range: NSRange) -> Range<Int> {
        func splitAt(_ p: Int) {
            guard p > 0, p < length, let i = _runs.firstIndex(where: { $0.range.contains(p) && $0.range.location != p }) else { return }
            let r = _runs[i]
            _runs[i].range.length = p - r.range.location
            _runs.insert(Run(range: NSRange(location: p, length: r.range.upperBound - p), attributes: r.attributes), at: i + 1)
        }
        splitAt(range.location); splitAt(range.upperBound)
        let first = _runs.firstIndex { $0.range.location >= range.location } ?? _runs.count
        let last = _runs.lastIndex { $0.range.upperBound <= range.upperBound }.map { $0 + 1 } ?? first
        return first ..< Swift.max(first, last)
    }
    open func setAttributes(_ attrs: [NSAttributedString.Key: Any]?, range: NSRange) {
        for i in split(range) { _runs[i].attributes = attrs ?? [:] }
    }
    open func addAttributes(_ attrs: [NSAttributedString.Key: Any], range: NSRange) {
        for i in split(range) { _runs[i].attributes.merge(attrs) { _, new in new } }
    }
    open func addAttribute(_ name: NSAttributedString.Key, value: Any, range: NSRange) { addAttributes([name: value], range: range) }
    open func removeAttribute(_ name: NSAttributedString.Key, range: NSRange) {
        for i in split(range) { _runs[i].attributes[name] = nil }
    }
    open func replaceCharacters(in range: NSRange, with str: String) {
        replaceCharacters(in: range, with: NSAttributedString(string: str, attributes: range.location < length ? attributes(at: range.location, effectiveRange: nil) : (length > 0 ? attributes(at: length - 1, effectiveRange: nil) : [:])))
    }
    open func replaceCharacters(in range: NSRange, with attrString: NSAttributedString) {
        let lo = Swift.max(0, Swift.min(range.location, length)), hi = Swift.max(lo, Swift.min(range.upperBound, length))
        var newRuns: [Run] = []
        for run in _runs {
            if run.range.upperBound <= lo { newRuns.append(run); continue }
            if run.range.location < lo { newRuns.append(Run(range: NSRange(location: run.range.location, length: lo - run.range.location), attributes: run.attributes)) }
        }
        for run in attrString._runs { newRuns.append(Run(range: NSRange(location: run.range.location + lo, length: run.range.length), attributes: run.attributes)) }
        let delta = attrString.length - (hi - lo)
        for run in _runs where run.range.upperBound > hi {
            let start = Swift.max(run.range.location, hi)
            newRuns.append(Run(range: NSRange(location: start + delta, length: run.range.upperBound - start), attributes: run.attributes))
        }
        _utf16.replaceSubrange(lo ..< hi, with: attrString._utf16)
        _string = String(decoding: _utf16, as: UTF16.self)
        _runs = newRuns.filter { $0.range.length > 0 }
    }
    open func append(_ attrString: NSAttributedString) { replaceCharacters(in: NSRange(location: length, length: 0), with: attrString) }
    open func insert(_ attrString: NSAttributedString, at loc: Int) { replaceCharacters(in: NSRange(location: loc, length: 0), with: attrString) }
    open func deleteCharacters(in range: NSRange) { replaceCharacters(in: range, with: NSAttributedString()) }
    open func setAttributedString(_ attrString: NSAttributedString) { replaceCharacters(in: NSRange(location: 0, length: length), with: attrString) }
    open func beginEditing() {}
    open func endEditing() {}
}


// MARK: - NSRange (a parte Swift do overlay da Apple, NSRange.swift)

extension NSRange {
    @_alwaysEmitIntoClient public var lowerBound: Int { location }
    @_alwaysEmitIntoClient public var upperBound: Int { location + length }
    @_alwaysEmitIntoClient public func contains(_ index: Int) -> Bool { index >= location && index < upperBound }
    @_alwaysEmitIntoClient public func union(_ other: NSRange) -> NSRange {
        let lo = Swift.min(location, other.location), hi = Swift.max(upperBound, other.upperBound)
        return NSRange(location: lo, length: hi - lo)
    }
    @_alwaysEmitIntoClient public func intersection(_ other: NSRange) -> NSRange? {
        let lo = Swift.max(location, other.location), hi = Swift.min(upperBound, other.upperBound)
        return hi >= lo ? NSRange(location: lo, length: hi - lo) : nil
    }
    @_alwaysEmitIntoClient public init(_ range: Range<Int>) { self.init(location: range.lowerBound, length: range.count) }
    @_alwaysEmitIntoClient public init<R: RangeExpression>(_ region: R, in target: some StringProtocol) where R.Bound == String.Index {
        let r = region.relative(to: target)
        let u = target.utf16
        self.init(location: u.distance(from: u.startIndex, to: r.lowerBound), length: u.distance(from: r.lowerBound, to: r.upperBound))
    }
}
extension Range where Bound == Int {
    @_alwaysEmitIntoClient public init?(_ range: NSRange) {
        guard range.location != NSNotFound else { return nil }
        self = range.location ..< range.location + range.length
    }
}
extension Range where Bound == String.Index {
    @_alwaysEmitIntoClient public init?(_ range: NSRange, in string: some StringProtocol) {
        let u = string.utf16
        guard range.location != NSNotFound, range.location + range.length <= u.count else { return nil }
        let lo = u.index(u.startIndex, offsetBy: range.location), hi = u.index(lo, offsetBy: range.length)
        guard let l = String.Index(lo, within: string), let h = String.Index(hi, within: string) else { return nil }
        self = l ..< h
    }
}

// MARK: - ReferenceConvertible (Foundation da Apple; o swift-foundation não tem)

public protocol ReferenceConvertible: _ObjectiveCBridgeable, CustomStringConvertible, CustomDebugStringConvertible, Hashable {
    associatedtype ReferenceType: NSObject, NSCopying
}
extension Data: ReferenceConvertible { public typealias ReferenceType = NSData }
extension Date: ReferenceConvertible { public typealias ReferenceType = NSDate }
extension URL: ReferenceConvertible { public typealias ReferenceType = NSURL }

// Hashable do NSRange (overlay da Apple: $sSo8_NSRangeVSH10FoundationMc)
extension NSRange: Hashable {
    public static func == (lhs: NSRange, rhs: NSRange) -> Bool { lhs.location == rhs.location && lhs.length == rhs.length }
    public func hash(into hasher: inout Hasher) { hasher.combine(location); hasher.combine(length) }
}
