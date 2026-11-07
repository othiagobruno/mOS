// mOS — overlay Foundation (fase 5, marco B): Notification e URLRequest, com a API e os nomes da Apple, e as pontes com
// NSNotification e NSURLRequest/NSMutableURLRequest do Foundation do mOS.

import _Concurrency

// MARK: - Notification

public struct Notification: Hashable, Equatable, CustomStringConvertible {
    public typealias Name = NSNotificationName

    public var name: Name
    public var object: Any?
    public var userInfo: [AnyHashable: Any]?

    public init(name: Name, object: Any? = nil, userInfo: [AnyHashable: Any]? = nil) {
        self.name = name
        self.object = object
        self.userInfo = userInfo
    }

    public var description: String { "name = \(name.rawValue), object = \(String(describing: object)), userInfo = \(String(describing: userInfo))" }

    public static func == (a: Notification, b: Notification) -> Bool {
        guard a.name == b.name else { return false }
        switch (a.object, b.object) {
        case (nil, nil): break
        case (let x?, let y?): guard (x as AnyObject) === (y as AnyObject) else { return false }
        default: return false
        }
        return (a.userInfo == nil) == (b.userInfo == nil)
    }

    public func hash(into h: inout Hasher) { h.combine(name) }
}

extension Notification: _ObjectiveCBridgeable {
    @_semantics("convertToObjectiveC")
    public func _bridgeToObjectiveC() -> NSNotification {
        let info: NSDictionary? = userInfo.map { $0._bridgeToObjectiveC() }
        return NSNotification(name: name, object: object, userInfo: info)
    }

    internal static func _mosNote(_ n: NSNotification) -> Notification {
        Notification(name: n.name, object: n.object,
                     userInfo: n.userInfo.map { Dictionary<AnyHashable, Any>._unconditionallyBridgeFromObjectiveC($0) })
    }

    public static func _forceBridgeFromObjectiveC(_ x: NSNotification, result: inout Notification?) { result = _mosNote(x) }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSNotification, result: inout Notification?) -> Bool { result = _mosNote(x); return true }
    @_effects(readonly)
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSNotification?) -> Notification {
        guard let source else { return Notification(name: Notification.Name(rawValue: "")) }
        return _mosNote(source)
    }
}

// MARK: - URLRequest

public struct URLRequest: Hashable, Equatable, CustomStringConvertible {
    public typealias CachePolicy = NSURLRequestCachePolicy

    public var url: URL?
    public var cachePolicy: CachePolicy
    public var timeoutInterval: Double
    public var httpMethod: String?
    public var httpBody: Data?
    public var allHTTPHeaderFields: [String: String]?
    public var mainDocumentURL: URL?
    public var httpShouldHandleCookies = true

    public init(url: URL, cachePolicy: CachePolicy = .useProtocolCachePolicy, timeoutInterval: Double = 60.0) {
        self.url = url
        self.cachePolicy = cachePolicy
        self.timeoutInterval = timeoutInterval
        self.httpMethod = "GET"
    }

    public func value(forHTTPHeaderField field: String) -> String? {
        allHTTPHeaderFields?.first { $0.key.lowercased() == field.lowercased() }?.value
    }

    public mutating func setValue(_ value: String?, forHTTPHeaderField field: String) {
        var h = allHTTPHeaderFields ?? [:]
        if let k = h.keys.first(where: { $0.lowercased() == field.lowercased() }) { h[k] = nil }
        if let value { h[field] = value }
        allHTTPHeaderFields = h
    }

    public mutating func addValue(_ value: String, forHTTPHeaderField field: String) {
        if let old = self.value(forHTTPHeaderField: field) { setValue(old + "," + value, forHTTPHeaderField: field) }
        else { setValue(value, forHTTPHeaderField: field) }
    }

    public var description: String { url?.absoluteString ?? "(sem URL)" }

    public static func == (a: URLRequest, b: URLRequest) -> Bool {
        a.url == b.url && a.httpMethod == b.httpMethod && a.httpBody == b.httpBody && a.allHTTPHeaderFields == b.allHTTPHeaderFields
    }
    public func hash(into h: inout Hasher) { h.combine(url); h.combine(httpMethod) }
}

extension URLRequest: _ObjectiveCBridgeable {
    @_semantics("convertToObjectiveC")
    public func _bridgeToObjectiveC() -> NSURLRequest {
        let r = NSMutableURLRequest(url: (url ?? URL(fileURLWithPath: "/"))._bridgeToObjectiveC())
        if let m = httpMethod { r.setHTTPMethod(_mosNSString(m)) }
        for (k, v) in allHTTPHeaderFields ?? [:] { r.setValue(_mosNSString(v), forHTTPHeaderField: _mosNSString(k)) }
        if let b = httpBody { r.setHTTPBody(b._bridgeToObjectiveC()) }
        return r
    }

    internal static func _mosRequest(_ r: NSURLRequest) -> URLRequest {
        var out = URLRequest(url: r.url.map { URL._unconditionallyBridgeFromObjectiveC($0) } ?? URL(fileURLWithPath: "/"))
        out.httpMethod = r.httpMethod.map { _mosString($0) }
        out.httpBody = r.httpBody.map { Data._unconditionallyBridgeFromObjectiveC($0) }
        if let h = r.allHTTPHeaderFields { out.allHTTPHeaderFields = Dictionary<String, String>._unconditionallyBridgeFromObjectiveC(h as NSDictionary) }
        return out
    }

    public static func _forceBridgeFromObjectiveC(_ x: NSURLRequest, result: inout URLRequest?) { result = _mosRequest(x) }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSURLRequest, result: inout URLRequest?) -> Bool { result = _mosRequest(x); return true }
    @_effects(readonly)
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSURLRequest?) -> URLRequest {
        source.map(_mosRequest) ?? URLRequest(url: URL(fileURLWithPath: "/"))
    }
}

// MARK: - NSURLSession.data(for:delegate:) (async)

extension NSURLSession {
    public func data(for request: URLRequest, delegate: (any NSURLSessionTaskDelegate)? = nil) async throws -> (Data, NSURLResponse) {
        try await withCheckedThrowingContinuation { (c: CheckedContinuation<(Data, NSURLResponse), Error>) in
            let task = self.dataTask(with: request._bridgeToObjectiveC()) { data, response, error in
                if let error { c.resume(throwing: error); return }
                c.resume(returning: (data.map { Data._unconditionallyBridgeFromObjectiveC($0) } ?? Data(), response ?? NSURLResponse()))
            }
            task.resume()
        }
    }
}

