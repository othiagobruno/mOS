// mOS — o resto da API Swift do Foundation da Apple que o OpenSwiftUI usa e que o Foundation da variante com interop ObjC
// ainda não tem (SwiftUI.framework). Módulo MOSFoundationExtras, nome ABI "Foundation".
import Foundation
@_exported import Dispatch                  // o Foundation da Apple reexporta o Dispatch
@_exported import _MOSCoreGraphicsSwiftUI   // CGBlendMode... (sdk/CoreGraphics/CGSwiftUI.h)

// MARK: - NSKeyedUnarchiver (as variantes genéricas do overlay da Apple)

extension NSKeyedUnarchiver {
    public static func unarchivedObject<DecodedObjectType: NSObject>(ofClass cls: DecodedObjectType.Type, from data: Data) throws -> DecodedObjectType? {
        try __unarchivedObject(of: cls, from: data) as? DecodedObjectType
    }
}

// MARK: - NSNumber / NSString

extension NSNumber {
    public var decimalValue: Decimal { Decimal(doubleValue) }
}

extension NSString {
    public var deletingLastPathComponent: String {
        let s = self as String
        guard let slash = s.lastIndex(of: "/") else { return "" }
        return slash == s.startIndex ? "/" : String(s[..<slash])
    }
    public var lastPathComponent: String { String((self as String).split(separator: "/").last ?? "") }
    public func appendingPathComponent(_ str: String) -> String {
        let s = self as String
        return s.hasSuffix("/") || s.isEmpty ? s + str : s + "/" + str
    }
}

public func NSIntersectionRange(_ a: NSRange, _ b: NSRange) -> NSRange {
    a.intersection(b) ?? NSRange(location: 0, length: 0)
}
public func NSUnionRange(_ a: NSRange, _ b: NSRange) -> NSRange { a.union(b) }
public func NSEqualRanges(_ a: NSRange, _ b: NSRange) -> Bool { a == b }

public func NSLocalizedString(_ key: String, tableName: String? = nil, bundle: Bundle = Bundle.main, value: String = "", comment: String) -> String {
    bundle.localizedString(forKey: key, value: value, table: tableName)
}

// MARK: - Scanner (API Swift da Apple, sobre a String; sem NSScanner)

public class Scanner {
    public let string: String
    public var currentIndex: String.Index
    public var charactersToBeSkipped: CharacterSet? = .whitespacesAndNewlines
    public var caseSensitive = false

    public init(string: String) {
        self.string = string
        currentIndex = string.startIndex
    }

    public var isAtEnd: Bool { skip(); return currentIndex >= string.endIndex }

    private func skip() {
        guard let skipSet = charactersToBeSkipped else { return }
        while currentIndex < string.endIndex, string[currentIndex].unicodeScalars.allSatisfy({ skipSet.contains($0) }) {
            currentIndex = string.index(after: currentIndex)
        }
    }
    public func scanCharacter() -> Character? {
        skip()
        guard currentIndex < string.endIndex else { return nil }
        let c = string[currentIndex]
        currentIndex = string.index(after: currentIndex)
        return c
    }
    public func scanString(_ searchString: String) -> String? {
        skip()
        guard string[currentIndex...].hasPrefix(searchString) else { return nil }
        currentIndex = string.index(currentIndex, offsetBy: searchString.count)
        return searchString
    }
    public func scanUpToString(_ substring: String) -> String? {
        skip()
        let start = currentIndex
        if let r = string[currentIndex...].range(of: substring) { currentIndex = r.lowerBound } else { currentIndex = string.endIndex }
        return start == currentIndex ? nil : String(string[start..<currentIndex])
    }
    public func scanCharacters(from set: CharacterSet) -> String? {
        skip()
        let start = currentIndex
        while currentIndex < string.endIndex, string[currentIndex].unicodeScalars.allSatisfy({ set.contains($0) }) {
            currentIndex = string.index(after: currentIndex)
        }
        return start == currentIndex ? nil : String(string[start..<currentIndex])
    }
    public func scanUpToCharacters(from set: CharacterSet) -> String? {
        skip()
        let start = currentIndex
        while currentIndex < string.endIndex, !string[currentIndex].unicodeScalars.allSatisfy({ set.contains($0) }) {
            currentIndex = string.index(after: currentIndex)
        }
        return start == currentIndex ? nil : String(string[start..<currentIndex])
    }
    public enum NumberRepresentation { case decimal, hexadecimal }
    public func scanInt(representation: NumberRepresentation = .decimal) -> Int? {
        skip()
        var i = currentIndex
        var digits = ""
        if i < string.endIndex, string[i] == "-" || string[i] == "+" { digits.append(string[i]); i = string.index(after: i) }
        let radix = representation == .decimal ? 10 : 16
        while i < string.endIndex, string[i].isHexDigit, string[i].hexDigitValue! < radix {
            digits.append(string[i]); i = string.index(after: i)
        }
        guard let v = Int(digits, radix: radix) else { return nil }
        currentIndex = i
        return v
    }
    public func scanDouble() -> Double? {
        skip()
        var i = currentIndex
        var s = ""
        while i < string.endIndex, "+-.eE0123456789".contains(string[i]) { s.append(string[i]); i = string.index(after: i) }
        guard let v = Double(s) else { return nil }
        currentIndex = i
        return v
    }
}

// MARK: - CoreGraphics (nomes das API notes da Apple)

extension CGColorRenderingIntent {
    @_alwaysEmitIntoClient public static var defaultIntent: CGColorRenderingIntent { CGColorRenderingIntent(rawValue: 0)! }
}

// MARK: - run loop do CoreFoundation (os cabeçalhos CF do mOS chamam os tipos de MOSCFRunLoopRef...: ponteiros opacos)

public typealias CFRunLoop = MOSCFRunLoopRef
public typealias CFRunLoopObserver = MOSCFRunLoopObserverRef
public typealias CFRunLoopTimer = MOSCFRunLoopTimerRef

// MARK: - IndexSet (o que o IndexSet do mOS ainda não tem)

extension IndexSet {
    public func integerLessThan(_ integer: Int) -> Int? { self.filter { $0 < integer }.max() }
    public func integerGreaterThan(_ integer: Int) -> Int? { self.filter { $0 > integer }.min() }
}
