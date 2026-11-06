// mOS SDK — ponte entre os tipos CF (módulo C CoreFoundation do mOS, objetos Swift nativos criados por
// libMOSCFRuntime.a) e os tipos do Swift. Sem interoperabilidade ObjC não existe `as CFString`/`as CFDictionary`
// (a mesma situação do Linux): use estes helpers.

// MARK: - Hashable (permite [CFString: Any] e Set<CFString>)

extension CFString: @retroactive Equatable, @retroactive Hashable {
    public static func == (lhs: CFString, rhs: CFString) -> Bool { CFEqual(lhs, rhs) }
    public func hash(into hasher: inout Hasher) { hasher.combine(CFHash(self)) }
}
extension CFNumber: @retroactive Equatable, @retroactive Hashable {
    public static func == (lhs: CFNumber, rhs: CFNumber) -> Bool { CFEqual(lhs, rhs) }
    public func hash(into hasher: inout Hasher) { hasher.combine(CFHash(self)) }
}
extension CFData: @retroactive Equatable, @retroactive Hashable {
    public static func == (lhs: CFData, rhs: CFData) -> Bool { CFEqual(lhs, rhs) }
    public func hash(into hasher: inout Hasher) { hasher.combine(CFHash(self)) }
}

// MARK: - tipos de run loop (objetos do libSystem: ponteiros opacos, sem ARC)

public typealias CFRunLoop = MOSCFRunLoopRef
public typealias CFRunLoopObserver = MOSCFRunLoopObserverRef
public typealias CFRunLoopTimer = MOSCFRunLoopTimerRef

// MARK: - String <-> CFString

extension String {
    public init(_ cfString: CFString) {
        if let p = CFStringGetCStringPtr(cfString, CFStringBuiltInEncodings.UTF8.rawValue) {
            self.init(cString: p)
            return
        }
        let max = CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfString), CFStringBuiltInEncodings.UTF8.rawValue) + 1
        var buffer = [CChar](repeating: 0, count: max)
        self = CFStringGetCString(cfString, &buffer, max, CFStringBuiltInEncodings.UTF8.rawValue) ? String(cString: buffer) : ""
    }
    public var cfString: CFString {
        var utf8 = Array(self.utf8)
        return CFStringCreateWithBytes(nil, &utf8, utf8.count, CFStringBuiltInEncodings.UTF8.rawValue, false)!
    }
}

// MARK: - CFNumber / CFBoolean

extension CFNumber {
    public static func make(_ value: Double) -> CFNumber {
        var v = value
        return CFNumberCreate(nil, .doubleType, &v)!
    }
    public static func make(_ value: Int) -> CFNumber {
        var v = value
        return CFNumberCreate(nil, .longType, &v)!
    }
    public var doubleValue: Double {
        var d = 0.0
        _ = CFNumberGetValue(self, .doubleType, &d)
        return d
    }
    public var intValue: Int {
        var i = 0
        _ = CFNumberGetValue(self, .longType, &i)
        return i
    }
}

// Sem ObjC, `x as? CFString` (cast dinâmico para tipo CF) SEMPRE falha no runtime do Swift — como no Linux.
// Os tipos são reconhecidos por CFGetTypeID e convertidos com unsafeBitCast.

/// O objeto é um CF de libMOSCFRuntime (ou uma constante CFSTR)?
public func _mosIsCF(_ object: AnyObject, _ typeID: CFTypeID) -> Bool { CFGetTypeID(object) == typeID }
public func _mosAsCF<T: AnyObject>(_ object: AnyObject, _ typeID: CFTypeID, _: T.Type) -> T? {
    CFGetTypeID(object) == typeID ? unsafeBitCast(object, to: T.self) : nil
}
private func isCFObject(_ o: AnyObject) -> Bool {
    let t = CFGetTypeID(o)
    return t == CFStringGetTypeID() || t == CFNumberGetTypeID() || t == CFBooleanGetTypeID() || t == CFDataGetTypeID()
        || t == CFArrayGetTypeID() || t == CFDictionaryGetTypeID() || t >= 0x400
}

/// Converte um valor Swift no objeto CF equivalente (String, números, Bool, arrays e dicionários aninhados).
public func _mosCFObject(_ value: Any) -> AnyObject? {
    switch value {
    case let v as String: return v.cfString
    case let v as Bool: return v ? kCFBooleanTrue : kCFBooleanFalse
    case let v as Int: return CFNumber.make(v)
    case let v as Double: return CFNumber.make(v)
    case let v as Float: return CFNumber.make(Double(v))
    case let v as CGFloat: return CFNumber.make(Double(v))
    case let v as [String: Any]: return Dictionary(uniqueKeysWithValues: v.map { ($0.key.cfString, $0.value) }).cfDictionary
    case let v as [Any]: return v.cfArray
    default:
        // objetos CF (inclusive [CFString: Any], cujo cast de chave falharia) e classes Swift passam como estão
        if let d = value as? [AnyHashable: Any] {
            var r: [CFString: Any] = [:]
            for (k, v) in d {
                if let s = k.base as? String { r[s.cfString] = v } else if let o = _mosCFObject(k.base), let s = _mosAsCF(o, CFStringGetTypeID(), CFString.self) { r[s] = v }
            }
            return r.cfDictionary
        }
        guard type(of: value) is AnyClass else { return nil }
        let o = value as AnyObject
        return isCFObject(o) ? o : o
    }
}

/// Converte um objeto CF no valor Swift equivalente (CFString -> String, CFNumber -> Double/Int, CFBoolean -> Bool).
public func _mosSwiftValue(_ object: AnyObject) -> Any {
    if let s = _mosAsCF(object, CFStringGetTypeID(), CFString.self) { return String(s) }
    if let b = _mosAsCF(object, CFBooleanGetTypeID(), CFBoolean.self) { return CFBooleanGetValue(b) }
    if let n = _mosAsCF(object, CFNumberGetTypeID(), CFNumber.self) { return CFNumberIsFloatType(n) ? n.doubleValue as Any : n.intValue as Any }
    if let d = _mosAsCF(object, CFDictionaryGetTypeID(), CFDictionary.self) { return [CFString: Any](d) ?? [:] }
    if let a = _mosAsCF(object, CFArrayGetTypeID(), CFArray.self) { return [Any](a) }
    return object
}

// MARK: - CFArray / CFDictionary

extension Array {
    public var cfArray: CFArray {
        let a: CFMutableArray = CFArrayCreateMutable(nil, count, [kCFTypeArrayCallBacks])
        for element in self {
            if let o = _mosCFObject(element) { CFArrayAppendValue(a, Unmanaged.passUnretained(o).toOpaque()) }
        }
        return a
    }
    public init(_ cfArray: CFArray) where Element == Any {
        self = (0 ..< CFArrayGetCount(cfArray)).compactMap { i in
            CFArrayGetValueAtIndex(cfArray, i).map { _mosSwiftValue(Unmanaged<AnyObject>.fromOpaque($0).takeUnretainedValue()) }
        }
    }
}

extension Dictionary where Key == CFString {
    public var cfDictionary: CFDictionary {
        let d: CFMutableDictionary = CFDictionaryCreateMutable(nil, count, [kCFTypeDictionaryKeyCallBacks], [kCFTypeDictionaryValueCallBacks])
        for (key, value) in self {
            guard let o = _mosCFObject(value) else { continue }
            CFDictionarySetValue(d, Unmanaged.passUnretained(key).toOpaque(), Unmanaged.passUnretained(o).toOpaque())
        }
        return d
    }
    public init?(_ cfDictionary: CFDictionary) {
        let n = CFDictionaryGetCount(cfDictionary)
        var keys = [UnsafeRawPointer?](repeating: nil, count: n)
        var values = [UnsafeRawPointer?](repeating: nil, count: n)
        CFDictionaryGetKeysAndValues(cfDictionary, &keys, &values)
        var result: [CFString: Value] = [:]
        for i in 0 ..< n {
            guard let k = keys[i], let v = values[i],
                  let key = _mosAsCF(Unmanaged<AnyObject>.fromOpaque(k).takeUnretainedValue(), CFStringGetTypeID(), CFString.self) else { continue }
            let object = Unmanaged<AnyObject>.fromOpaque(v).takeUnretainedValue()
            // Any/Double/String...: valor Swift convertido; AnyObject: o objeto CF como está
            let value = Value.self == AnyObject.self ? (object as? Value) : ((_mosSwiftValue(object) as? Value) ?? (object as? Value))
            if let value { result[key] = value }
        }
        self = result
    }
}

extension CFDictionary {
    public subscript(key: CFString) -> AnyObject? {
        guard let v = CFDictionaryGetValue(self, Unmanaged.passUnretained(key).toOpaque()) else { return nil }
        return Unmanaged<AnyObject>.fromOpaque(v).takeUnretainedValue()
    }
}
