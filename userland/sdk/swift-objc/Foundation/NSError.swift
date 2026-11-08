// mOS — overlay Foundation (marco C): a ponte entre os erros Swift e o NSError, com a API e os nomes da Apple. O resto
// vem do swift-foundation no ramo FOUNDATION_FRAMEWORK (CocoaError/POSIXError como _BridgedStoredNSError, CustomNSError,
// _ErrorCodeProtocol; tools/build-swift-foundation-objc.ps1, MOS_NSERROR).
//
// O runtime Swift (ErrorObject.mm) procura por dlsym: _ObjectiveCBridgeableError, _bridgeNSErrorToError(_:out:),
// _getErrorDefaultUserInfo(_:) e a conformidade CFError: Error — por isso os nomes e as assinaturas são exatos.

// MARK: - dicionários (dentro do módulo Foundation o importador não faz a ponte)

internal func _mosSwiftDict(_ d: NSDictionary?) -> [String: Any] {
    guard let d else { return [:] }
    var out: [String: Any] = [:]
    for (k, v) in Dictionary<AnyHashable, Any>._unconditionallyBridgeFromObjectiveC(d) {
        if let s = k.base as? String { out[s] = v } else if let s = k.base as? NSString { out[_mosString(s)] = v }
    }
    return out
}

internal func _mosNSDict(_ d: [String: Any]) -> NSDictionary { d._bridgeToObjectiveC() }

// MARK: - protocolos de ponte

/// Um erro Swift que pode ser criado a partir de um NSError (o runtime usa no `as?` de NSError para o tipo)
public protocol _ObjectiveCBridgeableError: Error {
    init?(_bridgedNSError: __shared NSError)
}

/// Erro de enum com valor bruto inteiro e um domínio NSError fixo
public protocol _BridgedNSError: _ObjectiveCBridgeableError, RawRepresentable, Hashable where RawValue: FixedWidthInteger {
    static var _nsErrorDomain: String { get }
}

extension _BridgedNSError {
    public var _domain: String { Self._nsErrorDomain }
    public var _code: Int { Int(truncatingIfNeeded: rawValue) }

    public init?(_bridgedNSError: __shared NSError) {
        guard _mosString(_bridgedNSError.domain) == Self._nsErrorDomain else { return nil }
        self.init(rawValue: RawValue(truncatingIfNeeded: _bridgedNSError.code))
    }

    public func hash(into hasher: inout Hasher) { hasher.combine(_code) }
}

/// Erro guardado como NSError (CocoaError, POSIXError, URLError...)
public protocol _BridgedStoredNSError: CustomNSError, _ObjectiveCBridgeableError, Hashable {
    associatedtype Code: _ErrorCodeProtocol, RawRepresentable where Code.RawValue: FixedWidthInteger
    var _nsError: NSError { get }
    init(_nsError error: NSError)
}

extension _BridgedStoredNSError {
    public var code: Code { Code(rawValue: Code.RawValue(truncatingIfNeeded: _nsError.code))! }

    public init(_ code: Code, userInfo: [String: Any] = [:]) {
        self.init(_nsError: NSError(domain: _mosNSString(Self.errorDomain), code: Int(truncatingIfNeeded: code.rawValue),
                                    userInfo: _mosNSDict(userInfo)))
    }

    /// o userInfo do NSError
    public var userInfo: [String: Any] { _mosSwiftDict(_nsError.userInfo) }

    public init?(_bridgedNSError error: NSError) {
        guard _mosString(error.domain) == Self.errorDomain else { return nil }
        self.init(_nsError: error)
    }

    public var errorCode: Int { _nsError.code }
    public var errorUserInfo: [String: Any] { userInfo }

    public func hash(into hasher: inout Hasher) { hasher.combine(_nsError) }
    public static func == (a: Self, b: Self) -> Bool { a._nsError.isEqual(b._nsError) }

    public func _getEmbeddedNSError() -> AnyObject? { _nsError }
}

// MARK: - NSError e CFError como Error

extension NSError: Error {
    public var _domain: String { _mosString(domain) }
    public var _code: Int { code }
    public var _userInfo: AnyObject? { userInfo }
    public func _getEmbeddedNSError() -> AnyObject? { self }
}

// no mOS um CFError é um NSError
extension CFError: Error {
    public var _domain: String { _mosString(unsafeBitCast(self, to: NSError.self).domain) }
    public var _code: Int { unsafeBitCast(self, to: NSError.self).code }
    public var _userInfo: AnyObject? { unsafeBitCast(self, to: NSError.self).userInfo }
    public func _getEmbeddedNSError() -> AnyObject? { self }
}

// MARK: - conversões e ganchos do runtime

public func _convertErrorToNSError(_ error: Error) -> NSError {
    unsafeDowncast(_bridgeErrorToNSError(error), to: NSError.self)
}

public func _convertNSErrorToError(_ error: NSError?) -> Error {
    if let error { return error }
    return NSError(domain: _mosNSString("Foundation._GenericObjCError"), code: 0, userInfo: nil)
}

/// o NSError de um erro Swift pergunta por aqui o userInfo: CustomNSError.errorUserInfo + os textos do LocalizedError
public func _getErrorDefaultUserInfo<T: Error>(_ error: T) -> AnyObject? {
    var info: [String: Any] = [:]
    if let c = error as? CustomNSError { info = c.errorUserInfo }
    if let l = error as? LocalizedError {
        if info["NSLocalizedDescription"] == nil, let d = l.errorDescription { info["NSLocalizedDescription"] = d }
        if info["NSLocalizedFailureReason"] == nil, let d = l.failureReason { info["NSLocalizedFailureReason"] = d }
        if info["NSLocalizedRecoverySuggestion"] == nil, let d = l.recoverySuggestion { info["NSLocalizedRecoverySuggestion"] = d }
        if info["NSHelpAnchor"] == nil, let d = l.helpAnchor { info["NSHelpAnchor"] = d }
    }
    return info.isEmpty ? nil : _mosNSDict(info)
}

/// NSError -> tipo Swift (T: _ObjectiveCBridgeableError), chamado pelo runtime no `as?`
public func _bridgeNSErrorToError<T: _ObjectiveCBridgeableError>(_ error: NSError, out: UnsafeMutablePointer<T>) -> Bool {
    guard let e = T(_bridgedNSError: error) else { return false }
    out.initialize(to: e)
    return true
}

// MARK: - URLError

public struct URLError: _BridgedStoredNSError {
    public let _nsError: NSError

    public init(_nsError error: NSError) {
        precondition(error.domain.isEqual(NSURLErrorDomain))
        _nsError = error
    }

    public static var errorDomain: String { _mosString(NSURLErrorDomain) }

    public struct Code: RawRepresentable, Hashable, Sendable, _ErrorCodeProtocol {
        public typealias _ErrorType = URLError
        public let rawValue: Int
        public init(rawValue: Int) { self.rawValue = rawValue }
        public static var unknown: Code { Code(rawValue: -1) }
        public static var cancelled: Code { Code(rawValue: -999) }
        public static var badURL: Code { Code(rawValue: -1000) }
        public static var timedOut: Code { Code(rawValue: -1001) }
        public static var unsupportedURL: Code { Code(rawValue: -1002) }
        public static var cannotFindHost: Code { Code(rawValue: -1003) }
        public static var cannotConnectToHost: Code { Code(rawValue: -1004) }
        public static var networkConnectionLost: Code { Code(rawValue: -1005) }
        public static var dnsLookupFailed: Code { Code(rawValue: -1006) }
        public static var httpTooManyRedirects: Code { Code(rawValue: -1007) }
        public static var resourceUnavailable: Code { Code(rawValue: -1008) }
        public static var notConnectedToInternet: Code { Code(rawValue: -1009) }
        public static var redirectToNonExistentLocation: Code { Code(rawValue: -1010) }
        public static var badServerResponse: Code { Code(rawValue: -1011) }
        public static var userCancelledAuthentication: Code { Code(rawValue: -1012) }
        public static var userAuthenticationRequired: Code { Code(rawValue: -1013) }
        public static var zeroByteResource: Code { Code(rawValue: -1014) }
        public static var cannotDecodeRawData: Code { Code(rawValue: -1015) }
        public static var cannotDecodeContentData: Code { Code(rawValue: -1016) }
        public static var cannotParseResponse: Code { Code(rawValue: -1017) }
        public static var internationalRoamingOff: Code { Code(rawValue: -1018) }
        public static var callIsActive: Code { Code(rawValue: -1019) }
        public static var dataNotAllowed: Code { Code(rawValue: -1020) }
        public static var requestBodyStreamExhausted: Code { Code(rawValue: -1021) }
        public static var appTransportSecurityRequiresSecureConnection: Code { Code(rawValue: -1022) }
        public static var fileDoesNotExist: Code { Code(rawValue: -1100) }
        public static var fileIsDirectory: Code { Code(rawValue: -1101) }
        public static var noPermissionsToReadFile: Code { Code(rawValue: -1102) }
        public static var dataLengthExceedsMaximum: Code { Code(rawValue: -1103) }
        public static var secureConnectionFailed: Code { Code(rawValue: -1200) }
        public static var serverCertificateHasBadDate: Code { Code(rawValue: -1201) }
        public static var serverCertificateUntrusted: Code { Code(rawValue: -1202) }
        public static var serverCertificateHasUnknownRoot: Code { Code(rawValue: -1203) }
        public static var serverCertificateNotYetValid: Code { Code(rawValue: -1204) }
        public static var clientCertificateRejected: Code { Code(rawValue: -1205) }
        public static var clientCertificateRequired: Code { Code(rawValue: -1206) }
        public static var cannotLoadFromNetwork: Code { Code(rawValue: -2000) }
        public static var cannotCreateFile: Code { Code(rawValue: -3000) }
        public static var cannotOpenFile: Code { Code(rawValue: -3001) }
        public static var cannotCloseFile: Code { Code(rawValue: -3002) }
        public static var cannotWriteToFile: Code { Code(rawValue: -3003) }
        public static var cannotRemoveFile: Code { Code(rawValue: -3004) }
        public static var cannotMoveFile: Code { Code(rawValue: -3005) }
        public static var downloadDecodingFailedMidStream: Code { Code(rawValue: -3006) }
        public static var downloadDecodingFailedToComplete: Code { Code(rawValue: -3007) }
        public static var backgroundSessionInUseByAnotherProcess: Code { Code(rawValue: -996) }
        public static var backgroundSessionWasDisconnected: Code { Code(rawValue: -997) }
    }

    public var failingURL: URL? {
        (_nsError.userInfo.object(forKey: NSURLErrorFailingURLErrorKey) as? NSURL).map { $0 as URL }
    }
    public var failureURLString: String? {
        (_nsError.userInfo.object(forKey: NSURLErrorFailingURLStringErrorKey) as? NSString).map { _mosString($0) }
    }

    public static var unknown: Code { .unknown }
    public static var cancelled: Code { .cancelled }
    public static var badURL: Code { .badURL }
    public static var timedOut: Code { .timedOut }
    public static var unsupportedURL: Code { .unsupportedURL }
    public static var cannotFindHost: Code { .cannotFindHost }
    public static var cannotConnectToHost: Code { .cannotConnectToHost }
    public static var networkConnectionLost: Code { .networkConnectionLost }
    public static var dnsLookupFailed: Code { .dnsLookupFailed }
    public static var notConnectedToInternet: Code { .notConnectedToInternet }
    public static var badServerResponse: Code { .badServerResponse }
    public static var userAuthenticationRequired: Code { .userAuthenticationRequired }
    public static var secureConnectionFailed: Code { .secureConnectionFailed }
    public static var fileDoesNotExist: Code { .fileDoesNotExist }
    public static var dataLengthExceedsMaximum: Code { .dataLengthExceedsMaximum }
}
