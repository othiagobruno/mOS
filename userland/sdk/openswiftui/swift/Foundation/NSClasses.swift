// mOS SDK — as classes do Foundation clássico que o swift-foundation (FoundationEssentials) não traz e que no Linux
// vêm do swift-corelibs-foundation: NSString, NSValue, NSData, NSArray/NSMutableArray, NSDictionary, NSNull,
// NSCoding/NSSecureCoding, e as pontes `as` (String as NSString, [K: V] as NSDictionary...) via _ObjectiveCBridgeable,
// como no corelibs. Classes Swift nativas (sem ObjC): igualdade por isEqual(_:), hash por `hash`.

// MARK: - protocolos de arquivamento

public protocol NSCoding {
    func encode(with coder: NSCoder)
    init?(coder: NSCoder)
}

public protocol NSSecureCoding: NSCoding {
    static var supportsSecureCoding: Bool { get }
}

public protocol NSLocking {
    func lock()
    func unlock()
}

// MARK: - NSString

open class NSString: NSObject, NSCopying, NSMutableCopying, NSSecureCoding, @unchecked Sendable {
    public let _storage: String

    public override init() { _storage = "" }
    public init(string: String) { _storage = string }
    public convenience init(_ string: String) { self.init(string: string) }
    public convenience init?(utf8String: UnsafePointer<CChar>) { self.init(string: String(cString: utf8String)) }
    public convenience init(format: String, _ args: CVarArg...) { self.init(string: String(format: format, arguments: args)) }
    public convenience init?(data: Data, encoding: UInt) {
        guard let s = String(data: data, encoding: String.Encoding(rawValue: encoding)) else { return nil }
        self.init(string: s)
    }

    open var length: Int { _storage.utf16.count }
    open func character(at index: Int) -> unichar { Array(_storage.utf16)[index] }
    open var utf8String: UnsafePointer<CChar>? { _mosPersistentCString(_storage) }
    open func substring(with range: NSRange) -> String {
        let u = Array(_storage.utf16)
        return String(decoding: u[range.location ..< range.upperBound], as: UTF16.self)
    }
    open func substring(from: Int) -> String { substring(with: NSRange(location: from, length: length - from)) }
    open func substring(to: Int) -> String { substring(with: NSRange(location: 0, length: to)) }
    open func range(of s: String) -> NSRange {
        guard let r = _storage.firstRange(of: s) else { return NSRange(location: NSNotFound, length: 0) }
        return NSRange(r, in: _storage)
    }
    open func appending(_ s: String) -> String { _storage + s }
    open var lowercased: String { _storage.lowercased() }
    open var uppercased: String { _storage.uppercased() }
    open var doubleValue: Double { Double(_storage.trimmingCharacters(in: .whitespaces)) ?? 0 }
    open var integerValue: Int { Int(_storage.trimmingCharacters(in: .whitespaces)) ?? 0 }
    open var boolValue: Bool { ["y", "Y", "t", "T", "1"].contains(_storage.trimmingCharacters(in: .whitespaces).prefix(1)) }

    open func copy(with zone: NSZone? = nil) -> Any { self }
    open func mutableCopy(with zone: NSZone? = nil) -> Any { NSMutableString(string: _storage) }
    public static var supportsSecureCoding: Bool { true }
    open func encode(with coder: NSCoder) { coder.encode(_storage, forKey: "NS.string") }
    public required convenience init?(coder: NSCoder) {
        guard let s = coder.decodeObject(forKey: "NS.string") as? String else { return nil }
        self.init(string: s)
    }

    open override var description: String { _storage }
    open override func isEqual(_ object: Any?) -> Bool {
        if let o = object as? NSString { return o._storage == _storage }
        if let o = object as? String { return o == _storage }
        return false
    }
    open override var hash: Int { _storage.hashValue }

    /// Opções de desenho de texto (NSStringDrawing do UIFoundation/AppKit).
    public struct DrawingOptions: OptionSet, Sendable {
        public let rawValue: Int
        public init(rawValue: Int) { self.rawValue = rawValue }
        public static let usesLineFragmentOrigin = DrawingOptions(rawValue: 1 << 0)
        public static let usesFontLeading = DrawingOptions(rawValue: 1 << 1)
        public static let usesDeviceMetrics = DrawingOptions(rawValue: 1 << 3)
        public static let truncatesLastVisibleLine = DrawingOptions(rawValue: 1 << 5)
    }
    public struct CompareOptions: OptionSet, Sendable {
        public let rawValue: UInt
        public init(rawValue: UInt) { self.rawValue = rawValue }
        public static let caseInsensitive = CompareOptions(rawValue: 1)
        public static let literal = CompareOptions(rawValue: 2)
        public static let backwards = CompareOptions(rawValue: 4)
        public static let anchored = CompareOptions(rawValue: 8)
        public static let numeric = CompareOptions(rawValue: 64)
    }
}

open class NSMutableString: NSString, @unchecked Sendable {
    private var _mutable: String
    public override init(string: String) { _mutable = string; super.init(string: string) }
    public required convenience init?(coder: NSCoder) {
        guard let s = coder.decodeObject(forKey: "NS.string") as? String else { return nil }
        self.init(string: s)
    }
    open func append(_ s: String) { _mutable += s }
    open override var length: Int { _mutable.utf16.count }
    open override var description: String { _mutable }
    open override func copy(with zone: NSZone? = nil) -> Any { NSString(string: _mutable) }
}

public typealias unichar = UInt16

/// C string com vida longa para APIs que devolvem `UnsafePointer<CChar>` (utf8String, %s): fica num cache por conteúdo.
private nonisolated(unsafe) var _mosCStrings: [String: UnsafeMutablePointer<CChar>] = [:]
private let _mosCStringsLock = NSLock()
public func _mosPersistentCString(_ s: String) -> UnsafePointer<CChar> {
    _mosCStringsLock.lock(); defer { _mosCStringsLock.unlock() }
    if let p = _mosCStrings[s] { return UnsafePointer(p) }
    let u = Array(s.utf8CString)
    let p = UnsafeMutablePointer<CChar>.allocate(capacity: u.count)
    p.initialize(from: u, count: u.count)
    if _mosCStrings.count < 4096 { _mosCStrings[s] = p }
    return UnsafePointer(p)
}

extension NSRange {
    public init<R: RangeExpression>(_ region: R, in target: some StringProtocol) where R.Bound == String.Index {
        let r = region.relative(to: target)
        let s = String(target)
        location = s.utf16.distance(from: s.utf16.startIndex, to: r.lowerBound.samePosition(in: s.utf16) ?? s.utf16.startIndex)
        length = s.utf16.distance(from: r.lowerBound.samePosition(in: s.utf16) ?? s.utf16.startIndex, to: r.upperBound.samePosition(in: s.utf16) ?? s.utf16.endIndex)
    }
}
extension Range where Bound == String.Index {
    public init?(_ range: NSRange, in string: some StringProtocol) {
        guard range.location != NSNotFound else { return nil }
        let u = string.utf16
        guard range.upperBound <= u.count else { return nil }
        let lo = u.index(u.startIndex, offsetBy: range.location), hi = u.index(lo, offsetBy: range.length)
        guard let l = String.Index(lo, within: string), let h = String.Index(hi, within: string) else { return nil }
        self = l ..< h
    }
}

// MARK: - NSValue

/// Caixa para valores (geometria, transformações) — como o NSValue do corelibs: guarda o valor Swift e o tipo dele.
open class NSValue: NSObject, NSCopying, NSSecureCoding, @unchecked Sendable {
    public let _value: Any
    public init(_mosValue value: Any) { _value = value }
    public override init() { _value = () }

    public convenience init(point: CGPoint) { self.init(_mosValue: point) }
    public convenience init(cgPoint: CGPoint) { self.init(_mosValue: cgPoint) }
    public convenience init(size: CGSize) { self.init(_mosValue: size) }
    public convenience init(cgSize: CGSize) { self.init(_mosValue: cgSize) }
    public convenience init(rect: CGRect) { self.init(_mosValue: rect) }
    public convenience init(cgRect: CGRect) { self.init(_mosValue: cgRect) }
    public convenience init(range: NSRange) { self.init(_mosValue: range) }
    public convenience init(nonretainedObject object: Any?) { self.init(_mosValue: object as Any) }
    public convenience init(pointer: UnsafeRawPointer?) { self.init(_mosValue: pointer as Any) }

    open var pointValue: CGPoint { _value as? CGPoint ?? .zero }
    open var cgPointValue: CGPoint { pointValue }
    open var sizeValue: CGSize { _value as? CGSize ?? .zero }
    open var cgSizeValue: CGSize { sizeValue }
    open var rectValue: CGRect { _value as? CGRect ?? .zero }
    open var cgRectValue: CGRect { rectValue }
    open var rangeValue: NSRange { _value as? NSRange ?? NSRange(location: 0, length: 0) }
    open var pointerValue: UnsafeMutableRawPointer? { (_value as? UnsafeRawPointer).map { UnsafeMutableRawPointer(mutating: $0) } }
    open var nonretainedObjectValue: Any? { _value }
    /// Valor guardado com o tipo pedido (útil para extensões de outros módulos, como CATransform3D).
    public func _mosValue<T>(as type: T.Type) -> T? { _value as? T }

    open func copy(with zone: NSZone? = nil) -> Any { self }
    public static var supportsSecureCoding: Bool { true }
    open func encode(with coder: NSCoder) {
        switch _value {
        case let p as CGPoint: coder.encode("point", forKey: "NS.kind"); coder.encode(Double(p.x), forKey: "x"); coder.encode(Double(p.y), forKey: "y")
        case let s as CGSize: coder.encode("size", forKey: "NS.kind"); coder.encode(Double(s.width), forKey: "w"); coder.encode(Double(s.height), forKey: "h")
        case let r as CGRect:
            coder.encode("rect", forKey: "NS.kind")
            coder.encode(Double(r.origin.x), forKey: "x"); coder.encode(Double(r.origin.y), forKey: "y")
            coder.encode(Double(r.size.width), forKey: "w"); coder.encode(Double(r.size.height), forKey: "h")
        case let r as NSRange: coder.encode("range", forKey: "NS.kind"); coder.encode(r.location, forKey: "loc"); coder.encode(r.length, forKey: "len")
        default: coder.encode("opaque", forKey: "NS.kind")
        }
    }
    public required convenience init?(coder: NSCoder) {
        switch coder.decodeObject(forKey: "NS.kind") as? String {
        case "point": self.init(_mosValue: CGPoint(x: coder.decodeDouble(forKey: "x"), y: coder.decodeDouble(forKey: "y")))
        case "size": self.init(_mosValue: CGSize(width: coder.decodeDouble(forKey: "w"), height: coder.decodeDouble(forKey: "h")))
        case "rect": self.init(_mosValue: CGRect(x: coder.decodeDouble(forKey: "x"), y: coder.decodeDouble(forKey: "y"),
                                                 width: coder.decodeDouble(forKey: "w"), height: coder.decodeDouble(forKey: "h")))
        case "range": self.init(_mosValue: NSRange(location: coder.decodeInteger(forKey: "loc"), length: coder.decodeInteger(forKey: "len")))
        default: return nil
        }
    }
    open override var description: String { String(describing: _value) }
    open override func isEqual(_ object: Any?) -> Bool {
        guard let o = object as? NSValue else { return false }
        if let a = _value as? AnyHashable, let b = o._value as? AnyHashable { return a == b }
        return o === self
    }
    open override var hash: Int { (_value as? AnyHashable)?.hashValue ?? ObjectIdentifier(self).hashValue }
}

// MARK: - NSData

open class NSData: NSObject, NSCopying, NSMutableCopying, NSSecureCoding, @unchecked Sendable {
    public let _data: Data
    public init(data: Data) { _data = data }
    public override init() { _data = Data() }
    public convenience init(bytes: UnsafeRawPointer?, length: Int) {
        self.init(data: bytes.map { Data(bytes: $0, count: length) } ?? Data())
    }
    open var length: Int { _data.count }
    /// Ponteiro estável para os bytes (o NSData é imutável): uma cópia que vive com o objeto.
    open var bytes: UnsafeRawPointer {
        if let b = _bytesCache { return UnsafeRawPointer(b) }
        let p = UnsafeMutableRawPointer.allocate(byteCount: Swift.max(_data.count, 1), alignment: 16)
        _data.copyBytes(to: p.assumingMemoryBound(to: UInt8.self), count: _data.count)
        _bytesCache = p
        return UnsafeRawPointer(p)
    }
    private var _bytesCache: UnsafeMutableRawPointer?
    deinit { _bytesCache?.deallocate() }

    open func copy(with zone: NSZone? = nil) -> Any { self }
    open func mutableCopy(with zone: NSZone? = nil) -> Any { NSData(data: _data) }
    public static var supportsSecureCoding: Bool { true }
    open func encode(with coder: NSCoder) { coder.encode(_data, forKey: "NS.data") }
    public required convenience init?(coder: NSCoder) {
        guard let d = coder.decodeObject(forKey: "NS.data") as? Data else { return nil }
        self.init(data: d)
    }
    open override func isEqual(_ object: Any?) -> Bool { (object as? NSData)?._data == _data || (object as? Data) == _data }
    open override var hash: Int { _data.hashValue }
    open override var description: String { "<\(_data.count) bytes>" }
}

// MARK: - NSNull, NSArray, NSDictionary

public final class NSNull: NSObject, NSCopying, NSSecureCoding, @unchecked Sendable {
    public override init() {}
    public func copy(with zone: NSZone? = nil) -> Any { self }
    public static var supportsSecureCoding: Bool { true }
    public func encode(with coder: NSCoder) {}
    public required init?(coder: NSCoder) {}
    public override func isEqual(_ object: Any?) -> Bool { object is NSNull }
    public override var hash: Int { 0 }
    public override var description: String { "<null>" }
}

open class NSArray: NSObject, NSCopying, NSMutableCopying, NSSecureCoding, Sequence, @unchecked Sendable {
    public var _elements: [Any]
    public init(array: [Any]) { _elements = array }
    public override init() { _elements = [] }
    public convenience init(objects: Any...) { self.init(array: objects) }
    open var count: Int { _elements.count }
    open func object(at index: Int) -> Any { _elements[index] }
    open var firstObject: Any? { _elements.first }
    open var lastObject: Any? { _elements.last }
    open func contains(_ o: Any) -> Bool { _elements.contains { _mosIsEqual($0, o) } }
    open func index(of o: Any) -> Int { _elements.firstIndex { _mosIsEqual($0, o) } ?? NSNotFound }
    public subscript(i: Int) -> Any { _elements[i] }
    public func makeIterator() -> IndexingIterator<[Any]> { _elements.makeIterator() }
    open func copy(with zone: NSZone? = nil) -> Any { NSArray(array: _elements) }
    open func mutableCopy(with zone: NSZone? = nil) -> Any { NSMutableArray(array: _elements) }
    public static var supportsSecureCoding: Bool { true }
    open func encode(with coder: NSCoder) { coder.encode(_elements, forKey: "NS.objects") }
    public required convenience init?(coder: NSCoder) { self.init(array: coder.decodeObject(forKey: "NS.objects") as? [Any] ?? []) }
    open override func isEqual(_ object: Any?) -> Bool {
        guard let o = object as? NSArray, o.count == count else { return false }
        return zip(_elements, o._elements).allSatisfy { _mosIsEqual($0, $1) }
    }
    open override var hash: Int { count }
    open override var description: String { "(\n" + _elements.map { "    \($0)" }.joined(separator: ",\n") + "\n)" }
}

open class NSMutableArray: NSArray, @unchecked Sendable {
    open func add(_ o: Any) { _elements.append(o) }
    open func addObjects(from array: [Any]) { _elements.append(contentsOf: array) }
    open func insert(_ o: Any, at i: Int) { _elements.insert(o, at: i) }
    open func removeObject(at i: Int) { _elements.remove(at: i) }
    open func removeLastObject() { _ = _elements.popLast() }
    open func removeAllObjects() { _elements.removeAll() }
    open func remove(_ o: Any) { _elements.removeAll { _mosIsEqual($0, o) } }
    open func replaceObject(at i: Int, with o: Any) { _elements[i] = o }
    open override func copy(with zone: NSZone? = nil) -> Any { NSArray(array: _elements) }
}

open class NSDictionary: NSObject, NSCopying, NSMutableCopying, NSSecureCoding, @unchecked Sendable {
    public var _storage: [AnyHashable: Any]
    public init(dictionary: [AnyHashable: Any]) { _storage = dictionary }
    public override init() { _storage = [:] }
    open var count: Int { _storage.count }
    open func object(forKey key: Any) -> Any? { (key as? AnyHashable).flatMap { _storage[$0] } }
    public subscript(key: Any) -> Any? { object(forKey: key) }
    open var allKeys: [Any] { Array(_storage.keys) }
    open var allValues: [Any] { Array(_storage.values) }
    open func copy(with zone: NSZone? = nil) -> Any { NSDictionary(dictionary: _storage) }
    open func mutableCopy(with zone: NSZone? = nil) -> Any { NSMutableDictionary(dictionary: _storage) }
    public static var supportsSecureCoding: Bool { true }
    open func encode(with coder: NSCoder) {
        coder.encode(_storage.keys.map { $0.base }, forKey: "NS.keys")
        coder.encode(Array(_storage.values), forKey: "NS.objects")
    }
    public required convenience init?(coder: NSCoder) {
        let k = coder.decodeObject(forKey: "NS.keys") as? [Any] ?? [], v = coder.decodeObject(forKey: "NS.objects") as? [Any] ?? []
        var d: [AnyHashable: Any] = [:]
        for (a, b) in zip(k, v) { if let h = a as? AnyHashable { d[h] = b } }
        self.init(dictionary: d)
    }
    open override func isEqual(_ object: Any?) -> Bool {
        guard let o = object as? NSDictionary, o.count == count else { return false }
        return _storage.allSatisfy { k, v in o._storage[k].map { _mosIsEqual($0, v) } ?? false }
    }
    open override var hash: Int { count }
    open override var description: String { "{\n" + _storage.map { "    \($0.key) = \($0.value);" }.joined(separator: "\n") + "\n}" }
}

open class NSMutableDictionary: NSDictionary, @unchecked Sendable {
    open func setObject(_ o: Any, forKey key: AnyHashable) { _storage[key] = o }
    open func removeObject(forKey key: Any) { if let k = key as? AnyHashable { _storage[k] = nil } }
    open func removeAllObjects() { _storage.removeAll() }
    public override subscript(key: Any) -> Any? {
        get { object(forKey: key) }
        set { if let k = key as? AnyHashable { _storage[k] = newValue } }
    }
}

/// Igualdade "de Foundation" entre valores quaisquer: isEqual para NSObject, == para Hashable.
public func _mosIsEqual(_ a: Any, _ b: Any) -> Bool {
    if let x = a as? NSObject { return x.isEqual(b) }
    if let y = b as? NSObject { return y.isEqual(a) }
    if let x = a as? AnyHashable, let y = b as? AnyHashable { return x == y }
    return false
}

// MARK: - pontes `as` (como o corelibs no Linux)

extension String: _ObjectiveCBridgeable {
    public typealias _ObjectiveCType = NSString
    public func _bridgeToObjectiveC() -> NSString { NSString(string: self) }
    public static func _forceBridgeFromObjectiveC(_ x: NSString, result: inout String?) { result = x.description }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSString, result: inout String?) -> Bool { result = x.description; return true }
    public static func _unconditionallyBridgeFromObjectiveC(_ x: NSString?) -> String { x?.description ?? "" }
}

extension Data: _ObjectiveCBridgeable {
    public typealias _ObjectiveCType = NSData
    public func _bridgeToObjectiveC() -> NSData { NSData(data: self) }
    public static func _forceBridgeFromObjectiveC(_ x: NSData, result: inout Data?) { result = x._data }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSData, result: inout Data?) -> Bool { result = x._data; return true }
    public static func _unconditionallyBridgeFromObjectiveC(_ x: NSData?) -> Data { x?._data ?? Data() }
}

extension Array: _ObjectiveCBridgeable {
    public typealias _ObjectiveCType = NSArray
    public func _bridgeToObjectiveC() -> NSArray { NSArray(array: map { $0 as Any }) }
    public static func _forceBridgeFromObjectiveC(_ x: NSArray, result: inout [Element]?) { result = x._elements.compactMap { $0 as? Element } }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSArray, result: inout [Element]?) -> Bool {
        let r = x._elements.compactMap { $0 as? Element }
        guard r.count == x.count else { result = nil; return false }
        result = r
        return true
    }
    public static func _unconditionallyBridgeFromObjectiveC(_ x: NSArray?) -> [Element] { x?._elements.compactMap { $0 as? Element } ?? [] }
}

extension Dictionary: _ObjectiveCBridgeable {
    public typealias _ObjectiveCType = NSDictionary
    public func _bridgeToObjectiveC() -> NSDictionary {
        var d: [AnyHashable: Any] = [:]
        for (k, v) in self { d[AnyHashable(k)] = v }
        return NSDictionary(dictionary: d)
    }
    public static func _forceBridgeFromObjectiveC(_ x: NSDictionary, result: inout [Key: Value]?) { _ = _conditionallyBridgeFromObjectiveC(x, result: &result) }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSDictionary, result: inout [Key: Value]?) -> Bool {
        var r: [Key: Value] = [:]
        for (k, v) in x._storage {
            guard let kk = k.base as? Key, let vv = v as? Value else { result = nil; return false }
            r[kk] = vv
        }
        result = r
        return true
    }
    public static func _unconditionallyBridgeFromObjectiveC(_ x: NSDictionary?) -> [Key: Value] {
        var r: [Key: Value]? = nil
        if let x { _ = _conditionallyBridgeFromObjectiveC(x, result: &r) }
        return r ?? [:]
    }
}

extension Int: _ObjectiveCBridgeable {
    public typealias _ObjectiveCType = NSNumber
    public func _bridgeToObjectiveC() -> NSNumber { NSNumber(value: self) }
    public static func _forceBridgeFromObjectiveC(_ x: NSNumber, result: inout Int?) { result = x.intValue }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSNumber, result: inout Int?) -> Bool { result = x.intValue; return true }
    public static func _unconditionallyBridgeFromObjectiveC(_ x: NSNumber?) -> Int { x?.intValue ?? 0 }
}
extension Double: _ObjectiveCBridgeable {
    public typealias _ObjectiveCType = NSNumber
    public func _bridgeToObjectiveC() -> NSNumber { NSNumber(value: self) }
    public static func _forceBridgeFromObjectiveC(_ x: NSNumber, result: inout Double?) { result = x.doubleValue }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSNumber, result: inout Double?) -> Bool { result = x.doubleValue; return true }
    public static func _unconditionallyBridgeFromObjectiveC(_ x: NSNumber?) -> Double { x?.doubleValue ?? 0 }
}
extension Bool: _ObjectiveCBridgeable {
    public typealias _ObjectiveCType = NSNumber
    public func _bridgeToObjectiveC() -> NSNumber { NSNumber(value: self) }
    public static func _forceBridgeFromObjectiveC(_ x: NSNumber, result: inout Bool?) { result = x.boolValue }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSNumber, result: inout Bool?) -> Bool { result = x.boolValue; return true }
    public static func _unconditionallyBridgeFromObjectiveC(_ x: NSNumber?) -> Bool { x?.boolValue ?? false }
}
