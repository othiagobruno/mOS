// mOS SDK — NSRange, NSNumber, NSAttributedString/NSMutableAttributedString, CharacterSet e utilitários de
// String do Foundation, com as assinaturas da Apple, em Swift puro (sem ObjC). Índices e comprimentos são em
// unidades UTF-16, como no NSString.

// MARK: - NSRange

public struct _NSRange: Hashable, Sendable, CustomStringConvertible {
    public var location: Int
    public var length: Int
    public init() { location = 0; length = 0 }
    public init(location: Int, length: Int) { self.location = location; self.length = length }
    public init(_ range: Range<Int>) { location = range.lowerBound; length = range.count }
    public var upperBound: Int { location + length }
    public var lowerBound: Int { location }
    public func contains(_ index: Int) -> Bool { index >= location && index < location + length }
    public func intersection(_ other: NSRange) -> NSRange? {
        let lo = Swift.max(location, other.location), hi = Swift.min(upperBound, other.upperBound)
        return hi >= lo ? NSRange(location: lo, length: hi - lo) : nil
    }
    public func union(_ other: NSRange) -> NSRange {
        let lo = Swift.min(location, other.location), hi = Swift.max(upperBound, other.upperBound)
        return NSRange(location: lo, length: hi - lo)
    }
    public var description: String { "{\(location), \(length)}" }
}
public typealias NSRange = _NSRange
public let NSNotFound: Int = .max
public func NSMakeRange(_ loc: Int, _ len: Int) -> NSRange { NSRange(location: loc, length: len) }
public func NSMaxRange(_ r: NSRange) -> Int { r.upperBound }
extension Range where Bound == Int {
    public init?(_ range: NSRange) {
        guard range.location != NSNotFound else { return nil }
        self = range.location ..< range.upperBound
    }
}

// MARK: - NSNumber

open class NSNumber: NSObject, @unchecked Sendable {
    private enum Storage { case int(Int64), uint(UInt64), double(Double), bool(Bool) }
    private let storage: Storage
    public init(value: Int) { storage = .int(Int64(value)) }
    public init(value: Int8) { storage = .int(Int64(value)) }
    public init(value: Int16) { storage = .int(Int64(value)) }
    public init(value: Int32) { storage = .int(Int64(value)) }
    public init(value: Int64) { storage = .int(value) }
    public init(value: UInt) { storage = .uint(UInt64(value)) }
    public init(value: UInt8) { storage = .uint(UInt64(value)) }
    public init(value: UInt16) { storage = .uint(UInt64(value)) }
    public init(value: UInt32) { storage = .uint(UInt64(value)) }
    public init(value: UInt64) { storage = .uint(value) }
    public init(value: Double) { storage = .double(value) }
    public init(value: Float) { storage = .double(Double(value)) }
    public init(value: CGFloat) { storage = .double(Double(value)) }
    public init(value: Bool) { storage = .bool(value) }

    open var doubleValue: Double {
        switch storage { case .int(let v): Double(v); case .uint(let v): Double(v); case .double(let v): v; case .bool(let v): v ? 1 : 0 }
    }
    open var int64Value: Int64 {
        switch storage { case .int(let v): v; case .uint(let v): Int64(truncatingIfNeeded: v); case .double(let v): Int64(v); case .bool(let v): v ? 1 : 0 }
    }
    open var floatValue: Float { Float(doubleValue) }
    open var intValue: Int { Int(truncatingIfNeeded: int64Value) }
    open var int32Value: Int32 { Int32(truncatingIfNeeded: int64Value) }
    open var uintValue: UInt { UInt(truncatingIfNeeded: int64Value) }
    open var boolValue: Bool { doubleValue != 0 }
    open var stringValue: String {
        switch storage { case .int(let v): "\(v)"; case .uint(let v): "\(v)"; case .double(let v): "\(v)"; case .bool(let v): v ? "1" : "0" }
    }
    open override var description: String { stringValue }
    open override func isEqual(_ object: Any?) -> Bool {
        guard let o = object as? NSNumber else { return false }
        return doubleValue == o.doubleValue
    }
    open override var hash: Int { doubleValue.hashValue }
}

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
    open override var description: String { _string }
    open override func copy() -> Any { NSAttributedString(attributedString: self) }
    open func mutableCopy() -> Any { NSMutableAttributedString(attributedString: self) }
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

/// Bool por referência dos blocos de enumeração (no Darwin vem do ObjC).
public struct ObjCBool: ExpressibleByBooleanLiteral, Sendable {
    public var boolValue: Bool
    public init(_ value: Bool) { boolValue = value }
    public init(booleanLiteral value: Bool) { boolValue = value }
}

// MARK: - CharacterSet e String

public struct CharacterSet: Hashable, Sendable, SetAlgebra {
    public typealias Element = Unicode.Scalar
    private enum Kind: Hashable, Sendable { case scalars(Set<UInt32>), whitespaces, newlines, whitespacesAndNewlines, decimalDigits, letters, alphanumerics, punctuation }
    private var kinds: [Kind]
    private var inverted_ = false
    private init(kinds: [Kind]) { self.kinds = kinds }
    public init() { kinds = [.scalars([])] }
    public init(charactersIn string: String) { kinds = [.scalars(Set(string.unicodeScalars.map(\.value)))] }

    public static var whitespaces: CharacterSet { CharacterSet(kinds: [.whitespaces]) }
    public static var newlines: CharacterSet { CharacterSet(kinds: [.newlines]) }
    public static var whitespacesAndNewlines: CharacterSet { CharacterSet(kinds: [.whitespacesAndNewlines]) }
    public static var decimalDigits: CharacterSet { CharacterSet(kinds: [.decimalDigits]) }
    public static var letters: CharacterSet { CharacterSet(kinds: [.letters]) }
    public static var alphanumerics: CharacterSet { CharacterSet(kinds: [.alphanumerics]) }
    public static var punctuationCharacters: CharacterSet { CharacterSet(kinds: [.punctuation]) }
    public var inverted: CharacterSet { var c = self; c.inverted_.toggle(); return c }

    // espaços/quebras/dígitos por tabela explícita; letras e pontuação fora do ASCII pelas propriedades Unicode
    // da stdlib (no mOS as tabelas Unicode do runtime são parciais: o ASCII não passa por elas)
    private static func isNewline(_ v: UInt32) -> Bool { v == 0x0A || v == 0x0B || v == 0x0C || v == 0x0D || v == 0x85 || v == 0x2028 || v == 0x2029 }
    private static func isSpace(_ v: UInt32) -> Bool {
        v == 0x20 || v == 0x09 || v == 0xA0 || v == 0x1680 || (v >= 0x2000 && v <= 0x200A) || v == 0x202F || v == 0x205F || v == 0x3000
    }
    private static func isDigit(_ v: UInt32) -> Bool { (v >= 0x30 && v <= 0x39) || (v >= 0x660 && v <= 0x669) || (v >= 0xFF10 && v <= 0xFF19) }
    private static func isLetter(_ s: Unicode.Scalar) -> Bool {
        let v = s.value
        if v < 0x80 { return (v >= 0x41 && v <= 0x5A) || (v >= 0x61 && v <= 0x7A) }
        if v < 0x250 { return v >= 0xC0 && v != 0xD7 && v != 0xF7 }       /* Latin-1 / Latin Extended */
        return s.properties.isAlphabetic
    }
    private static func isPunct(_ s: Unicode.Scalar) -> Bool {
        let v = s.value
        if v < 0x80 { return "!\"#%&'()*,-./:;?@[\\]_{}".unicodeScalars.contains(s) }
        return s.properties.generalCategory.isPunctuation
    }
    private static func matches(_ k: Kind, _ s: Unicode.Scalar) -> Bool {
        let v = s.value
        switch k {
        case .scalars(let set): return set.contains(v)
        case .whitespaces: return isSpace(v)
        case .newlines: return isNewline(v)
        case .whitespacesAndNewlines: return isSpace(v) || isNewline(v)
        case .decimalDigits: return isDigit(v)
        case .letters: return isLetter(s)
        case .alphanumerics: return isLetter(s) || isDigit(v)
        case .punctuation: return isPunct(s)
        }
    }
    public func contains(_ member: Unicode.Scalar) -> Bool { kinds.contains { Self.matches($0, member) } != inverted_ }

    // SetAlgebra (o suficiente para unir conjuntos; operações sobre conjuntos invertidos não são exatas)
    public func union(_ other: CharacterSet) -> CharacterSet { CharacterSet(kinds: kinds + other.kinds) }
    public func intersection(_ other: CharacterSet) -> CharacterSet { var r = CharacterSet(); r.kinds = [.scalars([])]; _ = other; return r }
    public func symmetricDifference(_ other: CharacterSet) -> CharacterSet { union(other) }
    @discardableResult public mutating func insert(_ newMember: Unicode.Scalar) -> (inserted: Bool, memberAfterInsert: Unicode.Scalar) {
        let had = contains(newMember)
        kinds.append(.scalars([newMember.value]))
        return (!had, newMember)
    }
    @discardableResult public mutating func remove(_ member: Unicode.Scalar) -> Unicode.Scalar? { nil }
    @discardableResult public mutating func update(with newMember: Unicode.Scalar) -> Unicode.Scalar? { insert(newMember); return nil }
    public mutating func formUnion(_ other: CharacterSet) { kinds += other.kinds }
    public mutating func formIntersection(_ other: CharacterSet) { self = intersection(other) }
    public mutating func formSymmetricDifference(_ other: CharacterSet) { formUnion(other) }
    public mutating func insert(charactersIn string: String) { kinds.append(.scalars(Set(string.unicodeScalars.map(\.value)))) }
}

extension StringProtocol {
    public func rangeOfCharacter(from aSet: CharacterSet, options mask: String.CompareOptions = [], range aRange: Range<Index>? = nil) -> Range<Index>? {
        let s = String(self)
        let r = aRange.map { Range(uncheckedBounds: (String.Index($0.lowerBound, within: s) ?? s.startIndex, String.Index($0.upperBound, within: s) ?? s.endIndex)) } ?? (s.startIndex ..< s.endIndex)
        let scalars = s.unicodeScalars
        var i = mask.contains(.backwards) ? scalars.index(before: r.upperBound) : r.lowerBound
        while mask.contains(.backwards) ? i >= r.lowerBound : i < r.upperBound {
            if aSet.contains(scalars[i]) {
                let lo = Index(i, within: self) ?? startIndex
                let hi = Index(scalars.index(after: i), within: self) ?? endIndex
                return lo ..< hi
            }
            if mask.contains(.backwards) { if i == r.lowerBound { break }; i = scalars.index(before: i) } else { i = scalars.index(after: i) }
        }
        return nil
    }
    public func components(separatedBy separator: CharacterSet) -> [String] {
        var out: [String] = [], cur = String.UnicodeScalarView()
        for u in unicodeScalars {
            if separator.contains(u) { out.append(String(cur)); cur = String.UnicodeScalarView() } else { cur.append(u) }
        }
        out.append(String(cur))
        return out
    }
    public func components<T: StringProtocol>(separatedBy separator: T) -> [String] {
        let sep = Array(String(separator).utf8), bytes = Array(String(self).utf8)
        guard !sep.isEmpty, bytes.count >= sep.count else { return [String(self)] }
        var out: [String] = [], start = 0, i = 0
        while i + sep.count <= bytes.count {
            if bytes[i ..< i + sep.count].elementsEqual(sep) {
                out.append(String(decoding: bytes[start ..< i], as: UTF8.self))
                i += sep.count; start = i
            } else { i += 1 }
        }
        out.append(String(decoding: bytes[start...], as: UTF8.self))
        return out
    }
    public func trimmingCharacters(in set: CharacterSet) -> String {
        var scalars = Array(unicodeScalars)
        while let f = scalars.first, set.contains(f) { scalars.removeFirst() }
        while let l = scalars.last, set.contains(l) { scalars.removeLast() }
        var v = String.UnicodeScalarView(); v.append(contentsOf: scalars)
        return String(v)
    }
}

extension String {
    public struct CompareOptions: OptionSet, Sendable {
        public let rawValue: UInt
        public init(rawValue: UInt) { self.rawValue = rawValue }
        public static let caseInsensitive = CompareOptions(rawValue: 1)
        public static let literal = CompareOptions(rawValue: 2)
        public static let backwards = CompareOptions(rawValue: 4)
        public static let anchored = CompareOptions(rawValue: 8)
        public static let numeric = CompareOptions(rawValue: 64)
    }
    /// Comprimento em UTF-16 (o `length` do NSString).
    public var _nsLength: Int { utf16.count }
}

extension Unicode.GeneralCategory {
    fileprivate var isPunctuation: Bool {
        switch self {
        case .connectorPunctuation, .dashPunctuation, .openPunctuation, .closePunctuation, .initialPunctuation, .finalPunctuation, .otherPunctuation: true
        default: false
        }
    }
}
