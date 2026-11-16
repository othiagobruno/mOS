// mOS — módulo Swift CoreText do SwiftUI.framework na variante com interop ObjC. Toda a API vem do cabeçalho
// include/MOSCoreTextC.h (nomes da Apple) e é implementada pelo CoreText do libSystem; aqui ficam só as conversões
// Swift <-> CF que o código do OpenSwiftUIMOS/Replace usa (_mosCF, _mosDict...), agora pelo bridging do Foundation,
// e as conformâncias que o Swift da Apple dá aos tipos do CoreText. Substitui o CoreText.swift da variante nativa
// (userland/sdk/openswiftui/swift/CoreText), que trazia métricas aproximadas e objetos CF no formato Swift.
@_exported import Foundation
@_exported import _MOSCoreTextC

public typealias CTFontLegibilityWeightRaw = CFIndex

// MARK: - conversões Swift <-> CF

public protocol _MOSCFConvertible { var _mosCFValue: CFTypeRef { get } }
public protocol _MOSCFKey { var _mosCFKey: CFString { get } }

extension CFString: _MOSCFKey { public var _mosCFKey: CFString { self } }
extension String: _MOSCFKey, _MOSCFConvertible {
    public var _mosCFKey: CFString { self as NSString as CFString }
    public var _mosCFValue: CFTypeRef { self as NSString as CFString }
}
extension Int: _MOSCFConvertible { public var _mosCFValue: CFTypeRef { NSNumber(value: self) } }
extension Int32: _MOSCFConvertible { public var _mosCFValue: CFTypeRef { NSNumber(value: self) } }
extension UInt32: _MOSCFConvertible { public var _mosCFValue: CFTypeRef { NSNumber(value: self) } }
extension Double: _MOSCFConvertible { public var _mosCFValue: CFTypeRef { NSNumber(value: self) } }
extension Float: _MOSCFConvertible { public var _mosCFValue: CFTypeRef { NSNumber(value: self) } }
extension CGFloat: _MOSCFConvertible { public var _mosCFValue: CFTypeRef { NSNumber(value: Double(self)) } }
extension Bool: _MOSCFConvertible { public var _mosCFValue: CFTypeRef { self ? kCFBooleanTrue : kCFBooleanFalse } }
extension CTFontLegibilityWeight: _MOSCFConvertible { public var _mosCFValue: CFTypeRef { NSNumber(value: rawValue) } }
extension Array: _MOSCFConvertible {
    public var _mosCFValue: CFTypeRef { (compactMap { _mosToCF($0) } as [AnyObject]) as NSArray }
}
extension Dictionary: _MOSCFConvertible where Key: _MOSCFKey {
    public var _mosCFValue: CFTypeRef { _mosCF }
    public var _mosCF: CFDictionary {
        let d = NSMutableDictionary()
        for (k, v) in self { if let cv = _mosToCF(v) { d.setObject(cv, forKey: k._mosCFKey as NSString) } }
        return d as CFDictionary
    }
}
public protocol _MOSOptional { var _mosWrapped: Any? { get } }
extension Optional: _MOSOptional { public var _mosWrapped: Any? { self.map { $0 as Any } } }

public func _mosToCF(_ v: Any) -> CFTypeRef? {
    if let c = v as? _MOSCFConvertible { return c._mosCFValue }
    if let o = v as? _MOSOptional { return o._mosWrapped.flatMap { _mosToCF($0) } }
    if type(of: v) is AnyClass { return v as AnyObject }
    return nil
}
public func _mosCFNumber(_ v: Int) -> CFNumber { NSNumber(value: v) as CFNumber }
public func _mosCFNumber(_ v: Double) -> CFNumber { NSNumber(value: v) as CFNumber }
public func _mosCFNumber(_ v: CGFloat) -> CFNumber { NSNumber(value: Double(v)) as CFNumber }
public func _mosCFString(_ s: String) -> CFString { s as NSString as CFString }
public func _mosString(_ s: CFString) -> String { s as NSString as String }
public func _mosSwiftDict(_ d: CFDictionary) -> [CFString: Any] {
    var out: [CFString: Any] = [:]
    let nd = d as NSDictionary
    for k in nd.allKeys {
        guard let ks = k as? NSString, let v = nd.object(forKey: ks) else { continue }
        out[ks as CFString] = v
    }
    return out
}
extension CFDictionary {
    public var _mosDict: [CFString: Any]? { _mosSwiftDict(self) }
    public var _mosStringDict: [String: Any]? {
        var out: [String: Any] = [:]
        for (k, v) in _mosSwiftDict(self) { out[_mosString(k)] = v }
        return out
    }
}
extension Optional where Wrapped == AnyObject {
    public var _mosDict: [CFString: Any]? {
        guard let o = self, CFGetTypeID(o) == CFDictionaryGetTypeID() else { return nil }
        return _mosSwiftDict(unsafeBitCast(o, to: CFDictionary.self))
    }
    public var _mosStringDict: [String: Any]? {
        guard let o = self, CFGetTypeID(o) == CFDictionaryGetTypeID() else { return nil }
        return unsafeBitCast(o, to: CFDictionary.self)._mosStringDict
    }
}

// MARK: - conformâncias que o Swift da Apple dá aos tipos do CoreText

extension CTCompositionLanguage: Codable {}
