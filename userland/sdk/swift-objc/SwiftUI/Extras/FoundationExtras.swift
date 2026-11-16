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
