// mOS — overlay Swift "ObjectiveC" da variante com interop ObjC (docs/swift-objc-runtime.md, fase 3), escrito para o
// mOS com os mesmos nomes e layouts do overlay do SDK da Apple, para que binários Swift da Apple (ex.: Postico)
// resolvam contra ele: ObjCBool, Selector, NSZone, NSObject Equatable/Hashable/CVarArg e as funções que o compilador
// chama nas pontes de BOOL e SEL. Compilado com -import-underlying-module sobre o módulo clang ObjectiveC.

@_exported import ObjectiveC

// MARK: - ObjCBool (BOOL do x86_64 macOS é signed char)

@frozen
public struct ObjCBool: ExpressibleByBooleanLiteral {
    @usableFromInline var _value: Int8

    @_transparent
    @usableFromInline init(_ value: Int8) { _value = value }

    @_transparent
    public init(_ value: Bool) { _value = value ? 1 : 0 }

    @_transparent
    public init(booleanLiteral value: Bool) { self.init(value) }

    @_transparent
    public var boolValue: Bool { _value != 0 }
}

extension ObjCBool: CustomReflectable {
    public var customMirror: Mirror { Mirror(reflecting: boolValue) }
}

extension ObjCBool: CustomStringConvertible {
    public var description: String { boolValue.description }
}

// o compilador chama estas na ponte BOOL <-> Bool
@_transparent
public func _convertBoolToObjCBool(_ x: Bool) -> ObjCBool { ObjCBool(x) }

@_transparent
public func _convertObjCBoolToBool(_ x: ObjCBool) -> Bool { x.boolValue }

// MARK: - Selector (SEL)

@frozen
public struct Selector: ExpressibleByStringLiteral {
    var ptr: OpaquePointer

    public init(_ str: String) {
        self = str.withCString { sel_registerName($0) }   // SEL já chega como Selector
    }

    public init(unicodeScalarLiteral value: String) { self.init(value) }
    public init(extendedGraphemeClusterLiteral value: String) { self.init(value) }
    public init(stringLiteral value: String) { self.init(value) }
}

extension Selector: Equatable, Hashable {
    public static func == (lhs: Selector, rhs: Selector) -> Bool { sel_isEqual(lhs, rhs) }
    public func hash(into hasher: inout Hasher) { hasher.combine(ptr) }
}

extension Selector: CustomStringConvertible {
    public var description: String { String(cString: sel_getName(self)) }
}

extension String {
    public init(_sel: Selector) { self = String(cString: sel_getName(_sel)) }
}

extension Selector: CustomReflectable {
    public var customMirror: Mirror { Mirror(reflecting: String(_sel: self)) }
}

// MARK: - NSZone

@frozen
public struct NSZone {
    var pointer: OpaquePointer
}

// MARK: - autoreleasepool

@_silgen_name("objc_autoreleasePoolPush") internal func __pushAutoreleasePool() -> OpaquePointer
@_silgen_name("objc_autoreleasePoolPop") internal func __popAutoreleasePool(_ pool: OpaquePointer)

public func autoreleasepool<Result>(invoking body: () throws -> Result) rethrows -> Result {
    let pool = __pushAutoreleasePool()
    defer { __popAutoreleasePool(pool) }
    return try body()
}

// MARK: - NSObject: Equatable, Hashable (pelo -isEqual: e -hash do ObjC)

extension NSObject: Equatable, Hashable {
    public static func == (lhs: NSObject, rhs: NSObject) -> Bool { lhs.isEqual(rhs) }

    @nonobjc
    public var hashValue: Int { hash }

    public func hash(into hasher: inout Hasher) { hasher.combine(hashValue) }

    public func _rawHashValue(seed: Int) -> Int { hashValue }
}

public func ~= (x: NSObject, y: NSObject) -> Bool { x.isEqual(y) }

// MARK: - CVarArg (objetos ObjC em String(format:), NSLog...)

extension NSObject: CVarArg {
    public var _cVarArgEncoding: [Int] {
        _autorelease(self)
        return _encodeBitsAsWords(self)
    }
}
