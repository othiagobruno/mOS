// mOS — overlay Foundation (fase 5, marco A): constantes e funções globais com os nomes da Apple.

public var NSNotFound: Int { Int.max }

public func NSLog(_ format: String, _ args: CVarArg...) {
    let f = _mosNSString(format)
    withVaList(args) { NSLogv(f, $0) }
}

extension String {
    // assinaturas da Apple (__shared entra no nome mangled)
    public init(format: __shared String, _ arguments: CVarArg...) {
        self = _mosString(NSString(format: format, arguments: arguments))
    }

    public init(format: __shared String, arguments: __shared [CVarArg]) {
        self = _mosString(NSString(format: format, arguments: arguments))
    }

    /// o mOS formata sempre no locale "C" (o do libSystem)
    public init(format: __shared String, locale: __shared Locale?, _ args: CVarArg...) {
        self = _mosString(NSString(format: format, arguments: args))
    }

    public init(format: __shared String, locale: __shared Locale?, arguments: __shared [CVarArg]) {
        self = _mosString(NSString(format: format, arguments: arguments))
    }

    public static func localizedStringWithFormat(_ format: String, _ arguments: CVarArg...) -> String {
        _mosString(NSString(format: format, arguments: arguments))
    }
}

/// String como argumento de formato (%@), pela NSString
extension String: CVarArg {
    public var _cVarArgEncoding: [Int] { _mosNSString(self)._cVarArgEncoding }
}

// MARK: - inteiros a partir de NSNumber (init(truncating:)/init(exactly:))

extension Int { public init(truncating number: __shared NSNumber) { self = number.intValue } }
extension UInt { public init(truncating number: __shared NSNumber) { self = number.uintValue } }
extension Int64 { public init(truncating number: __shared NSNumber) { self = number.int64Value } }
extension UInt64 { public init(truncating number: __shared NSNumber) { self = number.uint64Value } }
extension Int32 { public init(truncating number: __shared NSNumber) { self = number.int32Value } }
extension UInt32 { public init(truncating number: __shared NSNumber) { self = number.uint32Value } }
extension Double { public init(truncating number: __shared NSNumber) { self = number.doubleValue } }
extension Float { public init(truncating number: __shared NSNumber) { self = number.floatValue } }
extension Bool { public init(truncating number: __shared NSNumber) { self = number.boolValue } }

/// o objeto ObjC de uma String (atalho usado dentro do overlay)
extension String {
    @inline(__always) internal var _ns: NSString { _mosNSString(self) }
}
