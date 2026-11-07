// mOS — overlay Swift "Foundation" da variante com interop ObjC (docs/swift-objc-runtime.md, fase 5), marco A: as
// pontes Swift <-> ObjC (_ObjectiveCBridgeable) de String, números, Bool, Array, Dictionary e Set, com os nomes da
// Apple (os binários Swift da Apple importam _bridgeToObjectiveC/_unconditionallyBridgeFromObjectiveC...).
//
// Diferença deliberada da Apple: a ponte para ObjC cria objetos do Foundation do mOS (NSString, NSArray, NSDictionary
// do foundation.c), em vez de devolver o armazenamento Swift como subclasse de NSString/NSArray. Os métodos do
// Foundation do mOS leem a representação interna deles, então é assim que todo objeto que chega a eles é um que eles
// entendem. Na volta, os objetos ObjC são lidos pelos métodos públicos. O efeito para o programa é o mesmo.

@_exported import Foundation
@_exported import CoreGraphics
// os tipos-valor (Data, URL, Date, UUID, JSON...) do swift-foundation compilam neste mesmo módulo (marco B,
// tools\build-swift-foundation-objc.ps1), como o Foundation da Apple reexporta o seu
import Darwin

// MARK: - ajudantes

@_silgen_name("swift_unknownObjectRetain") internal func _mosRetain(_ o: UnsafeRawPointer) -> UnsafeRawPointer

internal let _mosUTF8: UInt = 4   // NSUTF8StringEncoding

/// NSString do mOS a partir de uma String Swift (UTF-8)
internal func _mosNSString(_ s: String) -> NSString {
    var s = s
    return s.withUTF8 { buf -> NSString in
        if buf.count == 0 { return NSString() }
        return NSString(bytes: buf.baseAddress!, length: buf.count, encoding: _mosUTF8) ?? NSString()
    }
}

/// String Swift a partir de qualquer NSString (lida pelo UTF8String; vazio se não houver)
internal func _mosString(_ ns: NSString) -> String {
    let n = ns.lengthOfBytes(usingEncoding: _mosUTF8)
    guard let p = ns.utf8String else {
        // sem UTF-8 direto: unidade por unidade em UTF-16
        let len = ns.length
        var u: [UInt16] = []
        u.reserveCapacity(len)
        for i in 0..<len { u.append(ns.character(at: i)) }
        return String(decoding: u, as: UTF16.self)
    }
    return p.withMemoryRebound(to: UInt8.self, capacity: n) {
        String(decoding: UnsafeBufferPointer(start: $0, count: n), as: UTF8.self)
    }
}

/// um valor Swift qualquer como objeto ObjC (pontes registradas, ou caixa __SwiftValue do runtime)
@inline(__always) internal func _mosObject(_ v: Any) -> AnyObject { _bridgeAnythingToObjectiveC(v) }

// MARK: - String

extension String: _ObjectiveCBridgeable {
    @_semantics("convertToObjectiveC")
    public func _bridgeToObjectiveC() -> NSString { _mosNSString(self) }

    public static func _forceBridgeFromObjectiveC(_ x: NSString, result: inout String?) { result = _mosString(x) }

    public static func _conditionallyBridgeFromObjectiveC(_ x: NSString, result: inout String?) -> Bool {
        result = _mosString(x)
        return true
    }

    @_effects(readonly)
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSString?) -> String {
        guard let source else { return "" }
        return _mosString(source)
    }
}

extension Substring: _ObjectiveCBridgeable {
    public func _bridgeToObjectiveC() -> NSString { _mosNSString(String(self)) }
    public static func _forceBridgeFromObjectiveC(_ x: NSString, result: inout Substring?) { result = Substring(_mosString(x)) }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSString, result: inout Substring?) -> Bool {
        result = Substring(_mosString(x)); return true
    }
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSString?) -> Substring {
        Substring(source.map(_mosString) ?? "")
    }
}

// MARK: - números e Bool (NSNumber)

internal protocol _MOSNumberBridged: _ObjectiveCBridgeable where _ObjectiveCType == NSNumber {}

extension Int: _ObjectiveCBridgeable {
    public func _bridgeToObjectiveC() -> NSNumber { NSNumber(value: self) }
    public static func _forceBridgeFromObjectiveC(_ x: NSNumber, result: inout Int?) { result = x.intValue }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSNumber, result: inout Int?) -> Bool {
        let d = x.doubleValue
        guard d == d.rounded(), d >= -9.3e18, d <= 9.3e18 else { result = nil; return false }
        result = x.intValue; return true
    }
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSNumber?) -> Int { source?.intValue ?? 0 }
}

extension UInt: _ObjectiveCBridgeable {
    public func _bridgeToObjectiveC() -> NSNumber { NSNumber(value: self) }
    public static func _forceBridgeFromObjectiveC(_ x: NSNumber, result: inout UInt?) { result = x.uintValue }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSNumber, result: inout UInt?) -> Bool {
        let d = x.doubleValue
        guard d == d.rounded(), d >= 0 else { result = nil; return false }
        result = x.uintValue; return true
    }
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSNumber?) -> UInt { source?.uintValue ?? 0 }
}

extension Int8: _ObjectiveCBridgeable {
    public func _bridgeToObjectiveC() -> NSNumber { NSNumber(value: self) }
    public static func _forceBridgeFromObjectiveC(_ x: NSNumber, result: inout Int8?) { result = x.int8Value }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSNumber, result: inout Int8?) -> Bool { result = Int8(exactly: x.doubleValue); return result != nil }
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSNumber?) -> Int8 { source?.int8Value ?? 0 }
}
extension UInt8: _ObjectiveCBridgeable {
    public func _bridgeToObjectiveC() -> NSNumber { NSNumber(value: self) }
    public static func _forceBridgeFromObjectiveC(_ x: NSNumber, result: inout UInt8?) { result = x.uint8Value }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSNumber, result: inout UInt8?) -> Bool { result = UInt8(exactly: x.doubleValue); return result != nil }
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSNumber?) -> UInt8 { source?.uint8Value ?? 0 }
}
extension Int16: _ObjectiveCBridgeable {
    public func _bridgeToObjectiveC() -> NSNumber { NSNumber(value: self) }
    public static func _forceBridgeFromObjectiveC(_ x: NSNumber, result: inout Int16?) { result = x.int16Value }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSNumber, result: inout Int16?) -> Bool { result = Int16(exactly: x.doubleValue); return result != nil }
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSNumber?) -> Int16 { source?.int16Value ?? 0 }
}
extension UInt16: _ObjectiveCBridgeable {
    public func _bridgeToObjectiveC() -> NSNumber { NSNumber(value: self) }
    public static func _forceBridgeFromObjectiveC(_ x: NSNumber, result: inout UInt16?) { result = x.uint16Value }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSNumber, result: inout UInt16?) -> Bool { result = UInt16(exactly: x.doubleValue); return result != nil }
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSNumber?) -> UInt16 { source?.uint16Value ?? 0 }
}
extension Int32: _ObjectiveCBridgeable {
    public func _bridgeToObjectiveC() -> NSNumber { NSNumber(value: self) }
    public static func _forceBridgeFromObjectiveC(_ x: NSNumber, result: inout Int32?) { result = x.int32Value }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSNumber, result: inout Int32?) -> Bool { result = Int32(exactly: x.doubleValue); return result != nil }
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSNumber?) -> Int32 { source?.int32Value ?? 0 }
}
extension UInt32: _ObjectiveCBridgeable {
    public func _bridgeToObjectiveC() -> NSNumber { NSNumber(value: self) }
    public static func _forceBridgeFromObjectiveC(_ x: NSNumber, result: inout UInt32?) { result = x.uint32Value }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSNumber, result: inout UInt32?) -> Bool { result = UInt32(exactly: x.doubleValue); return result != nil }
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSNumber?) -> UInt32 { source?.uint32Value ?? 0 }
}
extension Int64: _ObjectiveCBridgeable {
    public func _bridgeToObjectiveC() -> NSNumber { NSNumber(value: self) }
    public static func _forceBridgeFromObjectiveC(_ x: NSNumber, result: inout Int64?) { result = x.int64Value }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSNumber, result: inout Int64?) -> Bool {
        let d = x.doubleValue
        guard d == d.rounded() else { result = nil; return false }
        result = x.int64Value; return true
    }
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSNumber?) -> Int64 { source?.int64Value ?? 0 }
}
extension UInt64: _ObjectiveCBridgeable {
    public func _bridgeToObjectiveC() -> NSNumber { NSNumber(value: self) }
    public static func _forceBridgeFromObjectiveC(_ x: NSNumber, result: inout UInt64?) { result = x.uint64Value }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSNumber, result: inout UInt64?) -> Bool {
        let d = x.doubleValue
        guard d == d.rounded(), d >= 0 else { result = nil; return false }
        result = x.uint64Value; return true
    }
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSNumber?) -> UInt64 { source?.uint64Value ?? 0 }
}
extension Double: _ObjectiveCBridgeable {
    public func _bridgeToObjectiveC() -> NSNumber { NSNumber(value: self) }
    public static func _forceBridgeFromObjectiveC(_ x: NSNumber, result: inout Double?) { result = x.doubleValue }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSNumber, result: inout Double?) -> Bool { result = x.doubleValue; return true }
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSNumber?) -> Double { source?.doubleValue ?? 0 }
}
extension Float: _ObjectiveCBridgeable {
    public func _bridgeToObjectiveC() -> NSNumber { NSNumber(value: self) }
    public static func _forceBridgeFromObjectiveC(_ x: NSNumber, result: inout Float?) { result = x.floatValue }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSNumber, result: inout Float?) -> Bool { result = x.floatValue; return true }
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSNumber?) -> Float { source?.floatValue ?? 0 }
}
extension CGFloat: _ObjectiveCBridgeable {
    public func _bridgeToObjectiveC() -> NSNumber { NSNumber(value: Double(self)) }
    public static func _forceBridgeFromObjectiveC(_ x: NSNumber, result: inout CGFloat?) { result = CGFloat(x.doubleValue) }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSNumber, result: inout CGFloat?) -> Bool { result = CGFloat(x.doubleValue); return true }
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSNumber?) -> CGFloat { CGFloat(source?.doubleValue ?? 0) }
}
extension Bool: _ObjectiveCBridgeable {
    public func _bridgeToObjectiveC() -> NSNumber { NSNumber(value: self) }
    public static func _forceBridgeFromObjectiveC(_ x: NSNumber, result: inout Bool?) { result = x.boolValue }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSNumber, result: inout Bool?) -> Bool {
        let d = x.doubleValue
        guard d == 0 || d == 1 else { result = nil; return false }
        result = d == 1; return true
    }
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSNumber?) -> Bool { source?.boolValue ?? false }
}

extension NSNumber {
    public convenience init(value: Int) { self.init(integer: value) }
    public convenience init(value: UInt) { self.init(integer: Int(bitPattern: value)) }
    // o NSNumber do mOS (foundation.c) inicializa por initWithInt:, initWithInteger: e initWithDouble:
    public convenience init(value: Int8) { self.init(integer: Int(value)) }
    public convenience init(value: UInt8) { self.init(integer: Int(value)) }
    public convenience init(value: Int16) { self.init(integer: Int(value)) }
    public convenience init(value: UInt16) { self.init(integer: Int(value)) }
    public convenience init(value: Int32) { self.init(integer: Int(value)) }
    public convenience init(value: UInt32) { self.init(integer: Int(value)) }
    public convenience init(value: Int64) { self.init(integer: Int(value)) }
    public convenience init(value: UInt64) { self.init(integer: Int(bitPattern: UInt(value))) }
    public convenience init(value: Float) { self.init(double: Double(value)) }
    public convenience init(value: Double) { self.init(double: value) }
    public convenience init(value: Bool) { self.init(integer: value ? 1 : 0) }

    public var int8Value: Int8 { Int8(truncatingIfNeeded: int64Value) }
    public var uint8Value: UInt8 { UInt8(truncatingIfNeeded: int64Value) }
    public var int16Value: Int16 { Int16(truncatingIfNeeded: int64Value) }
    public var uint16Value: UInt16 { UInt16(truncatingIfNeeded: int64Value) }
    public var int32Value: Int32 { Int32(truncatingIfNeeded: int64Value) }
    public var uint32Value: UInt32 { UInt32(truncatingIfNeeded: uint64Value) }
    public var int64Value: Int64 { Int64(longLongValue) }
    public var uint64Value: UInt64 { UInt64(unsignedLongLongValue) }
    public var intValue: Int { integerValue }
    public var uintValue: UInt { unsignedIntegerValue }
}

// MARK: - coleções

extension Array: _ObjectiveCBridgeable {
    @_semantics("convertToObjectiveC")
    public func _bridgeToObjectiveC() -> NSArray {
        var objs: [AnyObject] = []
        objs.reserveCapacity(count)
        for e in self { objs.append(_mosObject(e)) }
        return objs.withUnsafeBufferPointer { b in
            NSArray(objects: b.baseAddress.map { UnsafePointer<AnyObject>($0) }, count: b.count)
        }
    }

    internal static func _mosElements(_ a: NSArray) -> [AnyObject] {
        var out: [AnyObject] = []
        let n = a.count
        out.reserveCapacity(n)
        for i in 0..<n { out.append(a.object(at: i) as AnyObject) }
        return out
    }

    public static func _forceBridgeFromObjectiveC(_ source: NSArray, result: inout [Element]?) {
        result = _mosElements(source).map { $0 as! Element }
    }

    public static func _conditionallyBridgeFromObjectiveC(_ source: NSArray, result: inout [Element]?) -> Bool {
        var out: [Element] = []
        for o in _mosElements(source) {
            guard let e = o as? Element else { result = nil; return false }
            out.append(e)
        }
        result = out
        return true
    }

    @_effects(readonly)
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSArray?) -> [Element] {
        guard let source else { return [] }
        return _mosElements(source).map { $0 as! Element }
    }
}

extension Dictionary: _ObjectiveCBridgeable {
    @_semantics("convertToObjectiveC")
    public func _bridgeToObjectiveC() -> NSDictionary {
        var ks: [AnyObject] = [], vs: [AnyObject] = []
        ks.reserveCapacity(count); vs.reserveCapacity(count)
        for (k, v) in self { ks.append(_mosObject(k)); vs.append(_mosObject(v)) }
        return ks.withUnsafeBufferPointer { kb in
            vs.withUnsafeBufferPointer { vb in
                NSDictionary(objects: vb.baseAddress.map { UnsafePointer<AnyObject>($0) },
                             forKeys: kb.baseAddress.map { UnsafeRawPointer($0).assumingMemoryBound(to: NSCopying.self) },
                             count: kb.count)
            }
        }
    }

    internal static func _mosPairs(_ d: NSDictionary) -> [(AnyObject, AnyObject)] {
        var out: [(AnyObject, AnyObject)] = []
        let keys = d.allKeys
        let n = keys.count
        out.reserveCapacity(n)
        for i in 0..<n {
            let k = keys.object(at: i) as AnyObject
            if let v = d.object(forKey: k) { out.append((k, v as AnyObject)) }
        }
        return out
    }

    public static func _forceBridgeFromObjectiveC(_ d: NSDictionary, result: inout [Key: Value]?) {
        var out: [Key: Value] = [:]
        for (k, v) in _mosPairs(d) { out[k as! Key] = (v as! Value) }
        result = out
    }

    public static func _conditionallyBridgeFromObjectiveC(_ d: NSDictionary, result: inout [Key: Value]?) -> Bool {
        var out: [Key: Value] = [:]
        for (k, v) in _mosPairs(d) {
            guard let kk = k as? Key, let vv = v as? Value else { result = nil; return false }
            out[kk] = vv
        }
        result = out
        return true
    }

    @_effects(readonly)
    public static func _unconditionallyBridgeFromObjectiveC(_ d: NSDictionary?) -> [Key: Value] {
        var r: [Key: Value]? = nil
        if let d { _forceBridgeFromObjectiveC(d, result: &r) }
        return r ?? [:]
    }
}

extension Set: _ObjectiveCBridgeable {
    @_semantics("convertToObjectiveC")
    public func _bridgeToObjectiveC() -> NSSet {
        var objs: [AnyObject] = []
        objs.reserveCapacity(count)
        for e in self { objs.append(_mosObject(e)) }
        return objs.withUnsafeBufferPointer { b in
            NSSet(objects: b.baseAddress.map { UnsafePointer<AnyObject>($0) }, count: b.count)
        }
    }

    public static func _forceBridgeFromObjectiveC(_ s: NSSet, result: inout Set<Element>?) {
        result = Set(Array<AnyObject>._mosElements(s.allObjects as NSArray).map { $0 as! Element })
    }

    public static func _conditionallyBridgeFromObjectiveC(_ s: NSSet, result: inout Set<Element>?) -> Bool {
        var out = Set<Element>()
        for o in Array<AnyObject>._mosElements(s.allObjects as NSArray) {
            guard let e = o as? Element else { result = nil; return false }
            out.insert(e)
        }
        result = out
        return true
    }

    @_effects(readonly)
    public static func _unconditionallyBridgeFromObjectiveC(_ s: NSSet?) -> Set<Element> {
        var r: Set<Element>? = nil
        if let s { _forceBridgeFromObjectiveC(s, result: &r) }
        return r ?? []
    }
}

// MARK: - AnyHashable (chaves de [AnyHashable: Any], como userInfo): vai o valor base, não uma caixa __SwiftValue

extension AnyHashable: _ObjectiveCBridgeable {
    public func _bridgeToObjectiveC() -> NSObject {
        _mosObject(base) as! NSObject
    }
    // como a representação própria do AnyHashable na Apple: NSString vira String e NSNumber inteiro vira Int
    internal static func _mosHashable(_ x: NSObject) -> AnyHashable {
        if let s = x as? NSString { return AnyHashable(_mosString(s)) }
        if let n = x as? NSNumber { let d = n.doubleValue; return d == d.rounded() && abs(d) < 9e15 ? AnyHashable(n.intValue) : AnyHashable(d) }
        return AnyHashable(x)
    }
    public static func _forceBridgeFromObjectiveC(_ x: NSObject, result: inout AnyHashable?) { result = _mosHashable(x) }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSObject, result: inout AnyHashable?) -> Bool {
        result = _mosHashable(x); return true
    }
    @_effects(readonly)
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSObject?) -> AnyHashable {
        _mosHashable(source ?? NSNull())
    }
}

// como na Apple: AnyHashable(NSString) é AnyHashable(String) e AnyHashable(NSNumber) o número Swift, para que chaves que
// voltam do ObjC (userInfo, [AnyHashable: Any]) sejam iguais às criadas em Swift
extension NSString: _HasCustomAnyHashableRepresentation {
    @nonobjc public func _toCustomAnyHashable() -> AnyHashable? { AnyHashable(_mosString(self)) }
}
extension NSNumber: _HasCustomAnyHashableRepresentation {
    @nonobjc public func _toCustomAnyHashable() -> AnyHashable? { AnyHashable._mosHashable(self) }
}
