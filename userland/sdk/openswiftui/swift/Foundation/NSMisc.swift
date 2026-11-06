// mOS SDK — o restante do Foundation clássico que o OpenSwiftUI usa: NSDate, NSDecimalNumber, ReferenceConvertible,
// Scanner, funções de NSRange e as variantes de String com Locale (lowercased(with:)...).

// MARK: - ReferenceConvertible

/// Tipo-valor do Foundation com uma classe "de referência" correspondente (Date ↔ NSDate, Data ↔ NSData...).
public protocol ReferenceConvertible: _ObjectiveCBridgeable, CustomStringConvertible, CustomDebugStringConvertible, Hashable {
    associatedtype ReferenceType: NSObject, NSCopying
}

extension Data: ReferenceConvertible { public typealias ReferenceType = NSData }
extension Date: ReferenceConvertible { public typealias ReferenceType = NSDate }
extension URL: ReferenceConvertible { public typealias ReferenceType = NSURL }

// MARK: - NSDate

open class NSDate: NSObject, NSCopying, NSSecureCoding, @unchecked Sendable {
    public let timeIntervalSinceReferenceDate: TimeInterval
    public init(timeIntervalSinceReferenceDate ti: TimeInterval) { timeIntervalSinceReferenceDate = ti }
    public override convenience init() { self.init(timeIntervalSinceReferenceDate: Date().timeIntervalSinceReferenceDate) }
    public convenience init(timeIntervalSince1970 secs: TimeInterval) { self.init(timeIntervalSinceReferenceDate: secs - Date.timeIntervalBetween1970AndReferenceDate) }
    public convenience init(timeIntervalSinceNow secs: TimeInterval) { self.init(timeIntervalSinceReferenceDate: Date().timeIntervalSinceReferenceDate + secs) }
    open var timeIntervalSince1970: TimeInterval { timeIntervalSinceReferenceDate + Date.timeIntervalBetween1970AndReferenceDate }
    open var timeIntervalSinceNow: TimeInterval { timeIntervalSinceReferenceDate - Date().timeIntervalSinceReferenceDate }
    public var _date: Date { Date(timeIntervalSinceReferenceDate: timeIntervalSinceReferenceDate) }

    open func copy(with zone: NSZone? = nil) -> Any { self }
    public static var supportsSecureCoding: Bool { true }
    open func encode(with coder: NSCoder) { coder.encode(timeIntervalSinceReferenceDate, forKey: "NS.time") }
    public required convenience init?(coder: NSCoder) { self.init(timeIntervalSinceReferenceDate: coder.decodeDouble(forKey: "NS.time")) }
    open override func isEqual(_ object: Any?) -> Bool {
        if let o = object as? NSDate { return o.timeIntervalSinceReferenceDate == timeIntervalSinceReferenceDate }
        if let o = object as? Date { return o.timeIntervalSinceReferenceDate == timeIntervalSinceReferenceDate }
        return false
    }
    open override var hash: Int { timeIntervalSinceReferenceDate.hashValue }
    open override var description: String { _date.description }
}

extension Date: _ObjectiveCBridgeable {
    public typealias _ObjectiveCType = NSDate
    public func _bridgeToObjectiveC() -> NSDate { NSDate(timeIntervalSinceReferenceDate: timeIntervalSinceReferenceDate) }
    public static func _forceBridgeFromObjectiveC(_ x: NSDate, result: inout Date?) { result = x._date }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSDate, result: inout Date?) -> Bool { result = x._date; return true }
    public static func _unconditionallyBridgeFromObjectiveC(_ x: NSDate?) -> Date { x?._date ?? Date() }
}

// MARK: - NSDecimalNumber

open class NSDecimalNumber: NSNumber, @unchecked Sendable {
    public let _decimal: Decimal
    public init(decimal: Decimal) {
        _decimal = decimal
        super.init(value: NSDecimalNumber._double(decimal))
    }
    public convenience init(string: String?) { self.init(decimal: string.flatMap { Decimal(string: $0) } ?? .nan) }
    public required convenience init?(coder: NSCoder) { self.init(decimal: Decimal(coder.decodeDouble(forKey: "NS.dblval"))) }
    public required convenience init(integerLiteral v: Int) { self.init(decimal: Decimal(v)) }
    public required convenience init(floatLiteral v: Double) { self.init(decimal: Decimal(v)) }
    public required convenience init(booleanLiteral v: Bool) { self.init(decimal: v ? 1 : 0) }
    static func _double(_ d: Decimal) -> Double { Double(d.description) ?? .nan }
    open override var decimalValue: Decimal { _decimal }
    open override var description: String { _decimal.description }
    public static var zero: NSDecimalNumber { NSDecimalNumber(decimal: 0) }
    public static var one: NSDecimalNumber { NSDecimalNumber(decimal: 1) }
    public static var notANumber: NSDecimalNumber { NSDecimalNumber(decimal: .nan) }
}

extension Decimal: _ObjectiveCBridgeable {
    public typealias _ObjectiveCType = NSNumber
    public func _bridgeToObjectiveC() -> NSNumber { NSDecimalNumber(decimal: self) }
    public static func _forceBridgeFromObjectiveC(_ x: NSNumber, result: inout Decimal?) { result = x.decimalValue }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSNumber, result: inout Decimal?) -> Bool { result = x.decimalValue; return true }
    public static func _unconditionallyBridgeFromObjectiveC(_ x: NSNumber?) -> Decimal { x?.decimalValue ?? 0 }
}

// MARK: - NSRange: funções

public func NSIntersectionRange(_ a: NSRange, _ b: NSRange) -> NSRange { a.intersection(b) ?? NSRange(location: 0, length: 0) }
public func NSUnionRange(_ a: NSRange, _ b: NSRange) -> NSRange { a.union(b) }
public func NSLocationInRange(_ loc: Int, _ r: NSRange) -> Bool { r.contains(loc) }
public func NSEqualRanges(_ a: NSRange, _ b: NSRange) -> Bool { a == b }
public func NSStringFromRange(_ r: NSRange) -> String { r.description }

// MARK: - String com Locale

extension StringProtocol {
    /// Sem ICU no mOS: o mapeamento de caixa é o do Unicode (o mesmo para qualquer idioma).
    public func lowercased(with locale: Locale?) -> String { lowercased() }
    public func uppercased(with locale: Locale?) -> String { uppercased() }
    public func capitalized(with locale: Locale?) -> String { capitalized }
    public var capitalized: String {
        var out = ""
        var atStart = true
        for ch in self {
            // sem as tabelas Unicode do stdlib: ASCII alfanumérico, e qualquer coisa fora do ASCII conta como letra
            if ch.asciiValue.map({ ($0 >= 48 && $0 <= 57) || ($0 >= 65 && $0 <= 90) || ($0 >= 97 && $0 <= 122) }) ?? true {
                out += atStart ? String(ch).uppercased() : String(ch).lowercased()
                atStart = false
            } else {
                out.append(ch)
                atStart = true
            }
        }
        return out
    }
    public var localizedLowercase: String { lowercased() }
    public var localizedUppercase: String { uppercased() }
    public var localizedCapitalized: String { capitalized }
}

// MARK: - Scanner

open class Scanner: NSObject, @unchecked Sendable {
    public enum NumberRepresentation: Sendable { case decimal, hexadecimal }

    public let string: String
    open var currentIndex: String.Index
    open var charactersToBeSkipped: CharacterSet? = .whitespacesAndNewlines
    open var caseSensitive = false
    open var locale: Any?

    public init(string: String) {
        self.string = string
        currentIndex = string.startIndex
    }
    open var isAtEnd: Bool {
        var i = currentIndex
        skip(&i)
        return i == string.endIndex
    }
    open var scanLocation: Int {
        get { string.utf16.distance(from: string.utf16.startIndex, to: currentIndex.samePosition(in: string.utf16) ?? string.utf16.endIndex) }
        set { currentIndex = String.Index(string.utf16.index(string.utf16.startIndex, offsetBy: newValue), within: string) ?? string.endIndex }
    }

    private func skip(_ i: inout String.Index) {
        guard let set = charactersToBeSkipped else { return }
        let u = string.unicodeScalars
        while i < string.endIndex, set.contains(u[i]) { i = u.index(after: i) }
    }

    open func scanCharacter() -> Character? {
        var i = currentIndex
        skip(&i)
        guard i < string.endIndex else { return nil }
        let c = string[i]
        currentIndex = string.index(after: i)
        return c
    }
    open func scanString(_ searchString: String) -> String? {
        var i = currentIndex
        skip(&i)
        let rest = string[i...]
        let match = caseSensitive ? rest.hasPrefix(searchString) : rest.lowercased().hasPrefix(searchString.lowercased())
        guard match, !searchString.isEmpty else { return nil }
        let end = string.index(i, offsetBy: searchString.count)
        currentIndex = end
        return String(string[i ..< end])
    }
    open func scanUpToString(_ substring: String) -> String? {
        var i = currentIndex
        skip(&i)
        guard i < string.endIndex else { return nil }
        let rest = string[i...]
        let end = rest.firstRange(of: substring)?.lowerBound ?? string.endIndex
        guard end > i else { return nil }
        currentIndex = end
        return String(string[i ..< end])
    }
    open func scanCharacters(from set: CharacterSet) -> String? {
        var i = currentIndex
        skip(&i)
        let u = string.unicodeScalars
        var j = i
        while j < string.endIndex, set.contains(u[j]) { j = u.index(after: j) }
        guard j > i else { return nil }
        currentIndex = j
        return String(string.unicodeScalars[i ..< j])
    }
    open func scanUpToCharacters(from set: CharacterSet) -> String? {
        var i = currentIndex
        skip(&i)
        let u = string.unicodeScalars
        var j = i
        while j < string.endIndex, !set.contains(u[j]) { j = u.index(after: j) }
        guard j > i else { return nil }
        currentIndex = j
        return String(string.unicodeScalars[i ..< j])
    }
    open func scanInt(representation: NumberRepresentation = .decimal) -> Int? {
        var i = currentIndex
        skip(&i)
        let u = Array(string.unicodeScalars[i...])
        var k = 0
        var neg = false
        if k < u.count, u[k] == "-" || u[k] == "+" { neg = u[k] == "-"; k += 1 }
        if representation == .hexadecimal, k + 1 < u.count, u[k] == "0", u[k + 1] == "x" || u[k + 1] == "X" { k += 2 }
        var v = 0, digits = 0
        while k < u.count {
            let c = u[k].value
            let d: Int
            if c >= 0x30 && c <= 0x39 { d = Int(c - 0x30) }
            else if representation == .hexadecimal, c >= 0x61 && c <= 0x66 { d = Int(c - 0x61 + 10) }
            else if representation == .hexadecimal, c >= 0x41 && c <= 0x46 { d = Int(c - 0x41 + 10) }
            else { break }
            v = v &* (representation == .hexadecimal ? 16 : 10) &+ d
            k += 1; digits += 1
        }
        guard digits > 0 else { return nil }
        currentIndex = string.unicodeScalars.index(i, offsetBy: k)
        return neg ? -v : v
    }
    open func scanDouble(representation: NumberRepresentation = .decimal) -> Double? {
        var i = currentIndex
        skip(&i)
        let u = string.unicodeScalars
        var j = i
        var seenDigit = false
        while j < string.endIndex {
            let c = u[j]
            if (c.value >= 0x30 && c.value <= 0x39) || "+-.eE".unicodeScalars.contains(c) {
                if c.value >= 0x30 && c.value <= 0x39 { seenDigit = true }
                j = u.index(after: j)
            } else { break }
        }
        guard seenDigit, let d = Double(String(u[i ..< j])) else { return nil }
        currentIndex = j
        return d
    }
    open func scanInt64(representation: NumberRepresentation = .decimal) -> Int64? { scanInt(representation: representation).map(Int64.init) }
    open func scanFloat(representation: NumberRepresentation = .decimal) -> Float? { scanDouble(representation: representation).map(Float.init) }
}

// MARK: - NSURL

open class NSURL: NSObject, NSCopying, NSSecureCoding, @unchecked Sendable {
    public let _url: URL
    public init(_mosURL url: URL) { _url = url }
    public convenience init?(string: String) { guard let u = URL(string: string) else { return nil }; self.init(_mosURL: u) }
    public convenience init(fileURLWithPath path: String) { self.init(_mosURL: URL(fileURLWithPath: path)) }
    public convenience init(fileURLWithPath path: String, isDirectory: Bool) { self.init(_mosURL: URL(fileURLWithPath: path, isDirectory: isDirectory)) }
    open var absoluteString: String? { _url.absoluteString }
    open var absoluteURL: URL? { _url.absoluteURL }
    open var path: String? { _url.path }
    open var scheme: String? { _url.scheme }
    open var host: String? { _url.host }
    open var isFileURL: Bool { _url.isFileURL }
    open var lastPathComponent: String? { _url.lastPathComponent }
    open var pathExtension: String? { _url.pathExtension }

    open func copy(with zone: NSZone? = nil) -> Any { self }
    public static var supportsSecureCoding: Bool { true }
    open func encode(with coder: NSCoder) { coder.encode(_url.absoluteString, forKey: "NS.url") }
    public required convenience init?(coder: NSCoder) {
        guard let s = coder.decodeObject(forKey: "NS.url") as? String, let u = URL(string: s) else { return nil }
        self.init(_mosURL: u)
    }
    open override func isEqual(_ object: Any?) -> Bool { (object as? NSURL)?._url == _url || (object as? URL) == _url }
    open override var hash: Int { _url.hashValue }
    open override var description: String { _url.description }
}

extension URL: _ObjectiveCBridgeable {
    public typealias _ObjectiveCType = NSURL
    public func _bridgeToObjectiveC() -> NSURL { NSURL(_mosURL: self) }
    public static func _forceBridgeFromObjectiveC(_ x: NSURL, result: inout URL?) { result = x._url }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSURL, result: inout URL?) -> Bool { result = x._url; return true }
    public static func _unconditionallyBridgeFromObjectiveC(_ x: NSURL?) -> URL { x?._url ?? URL(fileURLWithPath: "/") }
}
// MARK: - comparações e buscas com opções (API do NSString sobre String)

extension NSString {
    public func isEqual(to aString: String) -> Bool { description == aString }
    public var deletingLastPathComponent: String { (description as NSString)._path.deletingLastPathComponent().path }
    public var lastPathComponent: String { _path.lastPathComponent }
    public var pathExtension: String { _path.pathExtension }
    public var deletingPathExtension: String { _path.deletingPathExtension().path }
    public func appendingPathComponent(_ str: String) -> String { _path.appendingPathComponent(str).path }
    public func appendingPathExtension(_ str: String) -> String? { _path.appendingPathExtension(str).path }
    public var expandingTildeInPath: String { description.hasPrefix("~") ? FileManager.default.homeDirectoryForCurrentUser.path + description.dropFirst() : description }
    var _path: URL { URL(fileURLWithPath: description) }
}

extension StringProtocol {
    /// range(of:options:range:locale:) do NSString: .caseInsensitive, .backwards, .anchored e .literal.
    public func range<T: StringProtocol>(of aString: T, options mask: String.CompareOptions = [], range searchRange: Range<Index>? = nil,
                                         locale: Locale? = nil) -> Range<Index>? {
        let s = String(self)
        let lo = searchRange.flatMap { String.Index($0.lowerBound, within: s) } ?? s.startIndex
        let hi = searchRange.flatMap { String.Index($0.upperBound, within: s) } ?? s.endIndex
        let ci = mask.contains(.caseInsensitive)
        let needle = ci ? String(aString).lowercased() : String(aString)
        guard !needle.isEmpty else { return nil }
        let hay = s[lo ..< hi]
        let n = needle.count
        var candidates = Array(hay.indices)
        if mask.contains(.backwards) { candidates.reverse() }
        if mask.contains(.anchored) { candidates = Array(candidates.prefix(1)) }
        for i in candidates {
            guard let end = s.index(i, offsetBy: n, limitedBy: hi) else { continue }
            let piece = s[i ..< end]
            if (ci ? piece.lowercased() : String(piece)) == needle {
                guard let a = Index(i, within: self), let b = Index(end, within: self) else { return nil }
                return a ..< b
            }
        }
        return nil
    }
    public func compare<T: StringProtocol>(_ aString: T, options mask: String.CompareOptions = [], range: Range<Index>? = nil,
                                           locale: Locale? = nil) -> ComparisonResult {
        var a = String(self), b = String(aString)
        if mask.contains(.caseInsensitive) { a = a.lowercased(); b = b.lowercased() }
        if mask.contains(.numeric), let x = Double(a), let y = Double(b) { return x < y ? .orderedAscending : x > y ? .orderedDescending : .orderedSame }
        return a < b ? .orderedAscending : a > b ? .orderedDescending : .orderedSame
    }
    public func caseInsensitiveCompare<T: StringProtocol>(_ aString: T) -> ComparisonResult { compare(aString, options: .caseInsensitive) }
    public func localizedCaseInsensitiveContains<T: StringProtocol>(_ other: T) -> Bool { range(of: other, options: .caseInsensitive) != nil }
    public func localizedStandardContains<T: StringProtocol>(_ other: T) -> Bool { localizedCaseInsensitiveContains(other) }
}