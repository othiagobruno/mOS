// mOS SDK — NSCoder, NSKeyedArchiver e NSKeyedUnarchiver. O arquivo é uma árvore com chaves (valores simples,
// listas, dicionários e objetos NSCoding pelo nome do tipo Swift), gravada como JSON com um prefixo próprio do mOS.
// Não lê nem grava o formato bplist do NSKeyedArchiver da Apple; basta para o que o próprio processo arquiva e lê de
// volta (o OpenSwiftUI guarda valores de acessibilidade assim).

/// Nó do arquivo.
public indirect enum _MOSArchiveNode: Codable, Sendable {
    case null
    case bool(Bool)
    case int(Int64)
    case double(Double)
    case string(String)
    case data(Data)
    case array([_MOSArchiveNode])
    case dictionary(keys: [_MOSArchiveNode], values: [_MOSArchiveNode])
    case object(className: String, fields: [String: _MOSArchiveNode])
}

open class NSCoder: NSObject, @unchecked Sendable {
    open var allowsKeyedCoding: Bool { true }
    open var requiresSecureCoding: Bool { false }
    open var decodingFailurePolicy: DecodingFailurePolicy { .setErrorAndReturn }
    open var error: (any Error)? { nil }

    public enum DecodingFailurePolicy: Int, Sendable { case raiseException = 0, setErrorAndReturn = 1 }

    open func encode(_ object: Any?, forKey key: String) {}
    open func encodeConditionalObject(_ object: Any?, forKey key: String) { encode(object, forKey: key) }
    open func encode(_ value: Bool, forKey key: String) {}
    open func encode(_ value: Int, forKey key: String) {}
    open func encode(_ value: Int32, forKey key: String) { encode(Int(value), forKey: key) }
    open func encode(_ value: Int64, forKey key: String) { encode(Int(value), forKey: key) }
    open func encode(_ value: Float, forKey key: String) { encode(Double(value), forKey: key) }
    open func encode(_ value: Double, forKey key: String) {}

    open func containsValue(forKey key: String) -> Bool { false }
    open func decodeObject(forKey key: String) -> Any? { nil }
    open func decodeBool(forKey key: String) -> Bool { false }
    open func decodeInteger(forKey key: String) -> Int { 0 }
    open func decodeInt32(forKey key: String) -> Int32 { Int32(truncatingIfNeeded: decodeInteger(forKey: key)) }
    open func decodeInt64(forKey key: String) -> Int64 { Int64(decodeInteger(forKey: key)) }
    open func decodeDouble(forKey key: String) -> Double { 0 }
    open func decodeFloat(forKey key: String) -> Float { Float(decodeDouble(forKey: key)) }

    public func decodeObject<T>(of cls: T.Type, forKey key: String) -> T? { decodeObject(forKey: key) as? T }
    public func decodeObject(of classes: [AnyClass]?, forKey key: String) -> Any? { decodeObject(forKey: key) }
    public func decodeArrayOfObjects<T>(ofClass cls: T.Type, forKey key: String) -> [T]? { decodeObject(forKey: key) as? [T] }
    public func decodeDictionary<K: Hashable, V>(withKeysOfClass k: K.Type, objectsOfClass v: V.Type, forKey key: String) -> [K: V]? {
        decodeObject(forKey: key) as? [K: V]
    }
    open func failWithError(_ error: any Error) {}
}

public enum _MOSArchiveError: Error, CustomStringConvertible {
    case notAnArchive, unsupportedObject(String), unknownClass(String)
    public var description: String {
        switch self {
        case .notAnArchive: "os dados não são um arquivo do NSKeyedArchiver do mOS"
        case .unsupportedObject(let t): "objeto não arquivável (não é NSCoding): \(t)"
        case .unknownClass(let c): "classe desconhecida no arquivo: \(c)"
        }
    }
}

private let _mosArchiveMagic = Data("mOS-NSKeyedArchive-1\n".utf8)

open class NSKeyedArchiver: NSCoder, @unchecked Sendable {
    public static let rootObjectKey = "root"
    private var fields: [String: _MOSArchiveNode] = [:]
    private let secure: Bool
    private var finished: Data?

    public init(requiringSecureCoding: Bool) { secure = requiringSecureCoding }
    public override convenience init() { self.init(requiringSecureCoding: false) }

    open override var requiresSecureCoding: Bool { secure }

    /// Nó para um valor qualquer (lança em objetos que não sabem se arquivar).
    static func node(_ value: Any?) throws -> _MOSArchiveNode {
        guard let value else { return .null }
        switch value {
        case is NSNull: return .null
        case let n as NSNumber: return n._isBool ? .bool(n.boolValue) : n._isFloating ? .double(n.doubleValue) : .int(n.int64Value)
        case let b as Bool: return .bool(b)
        case let i as Int: return .int(Int64(i))
        case let i as Int64: return .int(i)
        case let i as Int32: return .int(Int64(i))
        case let u as UInt: return .int(Int64(bitPattern: UInt64(u)))
        case let d as Double: return .double(d)
        case let f as Float: return .double(Double(f))
        case let f as CGFloat: return .double(Double(f))
        case let s as String: return .string(s)
        case let d as Data: return .data(d)
        case let a as [Any]: return .array(try a.map { try node($0) })
        case let d as [AnyHashable: Any]:
            var ks: [_MOSArchiveNode] = [], vs: [_MOSArchiveNode] = []
            for (k, v) in d { ks.append(try node(k.base)); vs.append(try node(v)) }
            return .dictionary(keys: ks, values: vs)
        case let o as NSCoding:
            let sub = NSKeyedArchiver(requiringSecureCoding: false)
            o.encode(with: sub)
            return .object(className: _typeName(type(of: o), qualified: true), fields: sub.fields)
        default:
            throw _MOSArchiveError.unsupportedObject(_typeName(type(of: value)))
        }
    }

    open override func encode(_ object: Any?, forKey key: String) { fields[key] = (try? Self.node(object)) ?? .null }
    open override func encode(_ value: Bool, forKey key: String) { fields[key] = .bool(value) }
    open override func encode(_ value: Int, forKey key: String) { fields[key] = .int(Int64(value)) }
    open override func encode(_ value: Double, forKey key: String) { fields[key] = .double(value) }

    open func finishEncoding() {
        guard finished == nil else { return }
        let root = _MOSArchiveNode.object(className: "", fields: fields)
        finished = _mosArchiveMagic + ((try? JSONEncoder().encode(root)) ?? Data())
    }
    open var encodedData: Data { finishEncoding(); return finished! }

    open class func archivedData(withRootObject object: Any, requiringSecureCoding: Bool) throws -> Data {
        let a = NSKeyedArchiver(requiringSecureCoding: requiringSecureCoding)
        a.fields[rootObjectKey] = try node(object)
        return a.encodedData
    }
}

open class NSKeyedUnarchiver: NSCoder, @unchecked Sendable {
    private let fields: [String: _MOSArchiveNode]
    open override var requiresSecureCoding: Bool { get { _secure } }
    public var _secure = true
    open override var decodingFailurePolicy: DecodingFailurePolicy { get { _policy } }
    public var _policy: DecodingFailurePolicy = .setErrorAndReturn

    init(fields: [String: _MOSArchiveNode]) { self.fields = fields }

    public convenience init(forReadingFrom data: Data) throws {
        guard data.starts(with: _mosArchiveMagic),
              case let .object(_, f) = try JSONDecoder().decode(_MOSArchiveNode.self, from: data.dropFirst(_mosArchiveMagic.count))
        else { throw _MOSArchiveError.notAnArchive }
        self.init(fields: f)
    }

    static func value(_ n: _MOSArchiveNode) -> Any? {
        switch n {
        case .null: return nil
        case .bool(let b): return b
        case .int(let i): return Int(truncatingIfNeeded: i)
        case .double(let d): return d
        case .string(let s): return s
        case .data(let d): return d
        case .array(let a): return a.map { value($0) ?? NSNull() }
        case .dictionary(let ks, let vs):
            var d: [AnyHashable: Any] = [:]
            for (k, v) in zip(ks, vs) { if let kk = value(k) as? AnyHashable { d[kk] = value(v) ?? NSNull() } }
            return d
        case .object(let name, let f):
            guard let t = _typeByName(name) as? NSCoding.Type else { return nil }
            return t.init(coder: NSKeyedUnarchiver(fields: f))
        }
    }

    open override func containsValue(forKey key: String) -> Bool { fields[key] != nil }
    open override func decodeObject(forKey key: String) -> Any? { fields[key].flatMap { Self.value($0) } }
    open override func decodeBool(forKey key: String) -> Bool {
        if case .bool(let b)? = fields[key] { return b }
        return decodeInteger(forKey: key) != 0
    }
    open override func decodeInteger(forKey key: String) -> Int {
        switch fields[key] {
        case .int(let i)?: return Int(truncatingIfNeeded: i)
        case .double(let d)?: return Int(d)
        case .bool(let b)?: return b ? 1 : 0
        default: return 0
        }
    }
    open override func decodeDouble(forKey key: String) -> Double {
        switch fields[key] {
        case .double(let d)?: return d
        case .int(let i)?: return Double(i)
        default: return 0
        }
    }
    open func finishDecoding() {}

    open class func unarchivedObject<T>(ofClass cls: T.Type, from data: Data) throws -> T? {
        try NSKeyedUnarchiver(forReadingFrom: data).decodeObject(forKey: NSKeyedArchiver.rootObjectKey) as? T
    }
    open class func unarchivedObject(ofClasses classes: [AnyClass], from data: Data) throws -> Any? {
        try NSKeyedUnarchiver(forReadingFrom: data).decodeObject(forKey: NSKeyedArchiver.rootObjectKey)
    }
    open class func unarchiveTopLevelObjectWithData(_ data: Data) throws -> Any? {
        try NSKeyedUnarchiver(forReadingFrom: data).decodeObject(forKey: NSKeyedArchiver.rootObjectKey)
    }
}
