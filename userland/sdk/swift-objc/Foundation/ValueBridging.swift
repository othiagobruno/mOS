// mOS — Foundation (fase 5, marco B): as pontes dos tipos-valor (Data, URL, Date, UUID, Locale, TimeZone) com as classes do Foundation do libSystem do mOS (NSData, NSURL,
// NSDate, NSUUID, NSLocale, NSTimeZone). No mesmo módulo do tipo, como no Foundation da Apple (numa extensão de outro
// módulo o nome mangled levaria contexto de extensão, $s10Foundation4DataVAAE19_bridge..., e não bateria).
// Como no marco A, a ida cria objetos do mOS e a volta lê pelos métodos públicos.


// MARK: - Data <-> NSData

extension Data: _ObjectiveCBridgeable {
    @_semantics("convertToObjectiveC")
    public func _bridgeToObjectiveC() -> NSData {
        withUnsafeBytes { NSData(bytes: $0.baseAddress, length: $0.count) }
    }

    internal static func _mosData(_ d: NSData) -> Data {
        let n = d.length
        if n == 0 { return Data() }
        return Data(bytes: d.bytes, count: n)
    }

    public static func _forceBridgeFromObjectiveC(_ input: NSData, result: inout Data?) { result = _mosData(input) }

    public static func _conditionallyBridgeFromObjectiveC(_ input: NSData, result: inout Data?) -> Bool {
        result = _mosData(input); return true
    }

    @_effects(readonly)
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSData?) -> Data {
        guard let source else { return Data() }
        return _mosData(source)
    }
}

// MARK: - URL <-> NSURL

extension URL: _ObjectiveCBridgeable {
    @_semantics("convertToObjectiveC")
    public func _bridgeToObjectiveC() -> NSURL {
        if isFileURL { return NSURL.fileURL(withPath: _mosNSString(path), isDirectory: hasDirectoryPath) }
        return NSURL(string: _mosNSString(absoluteString)) ?? NSURL.fileURL(withPath: _mosNSString("/"), isDirectory: true)
    }

    internal static func _mosURL(_ u: NSURL) -> URL? {
        if u.isFileURL, let p = u.path { return URL(fileURLWithPath: _mosString(p)) }
        guard let s = u.absoluteString else { return nil }
        return URL(string: _mosString(s))
    }

    public static func _forceBridgeFromObjectiveC(_ source: NSURL, result: inout URL?) { result = _mosURL(source) }

    public static func _conditionallyBridgeFromObjectiveC(_ source: NSURL, result: inout URL?) -> Bool {
        result = _mosURL(source); return result != nil
    }

    @_effects(readonly)
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSURL?) -> URL {
        guard let source, let u = _mosURL(source) else { return URL(fileURLWithPath: "/") }
        return u
    }
}

// MARK: - Date <-> NSDate

extension Date: _ObjectiveCBridgeable {
    @_semantics("convertToObjectiveC")
    public func _bridgeToObjectiveC() -> NSDate { NSDate(timeIntervalSince1970: timeIntervalSince1970) }

    public static func _forceBridgeFromObjectiveC(_ x: NSDate, result: inout Date?) {
        result = Date(timeIntervalSince1970: x.timeIntervalSince1970)
    }

    public static func _conditionallyBridgeFromObjectiveC(_ x: NSDate, result: inout Date?) -> Bool {
        result = Date(timeIntervalSince1970: x.timeIntervalSince1970); return true
    }

    @_effects(readonly)
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSDate?) -> Date {
        Date(timeIntervalSince1970: source?.timeIntervalSince1970 ?? 0)
    }
}

// MARK: - UUID <-> NSUUID

extension UUID: _ObjectiveCBridgeable {
    @_semantics("convertToObjectiveC")
    public func _bridgeToObjectiveC() -> NSUUID { NSUUID(uuidString: _mosNSString(uuidString))! }

    public static func _forceBridgeFromObjectiveC(_ x: NSUUID, result: inout UUID?) {
        result = UUID(uuidString: _mosString(x.uuidString))
    }

    public static func _conditionallyBridgeFromObjectiveC(_ x: NSUUID, result: inout UUID?) -> Bool {
        result = UUID(uuidString: _mosString(x.uuidString)); return result != nil
    }

    @_effects(readonly)
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSUUID?) -> UUID {
        guard let source, let u = UUID(uuidString: _mosString(source.uuidString)) else { return UUID() }
        return u
    }
}

// MARK: - Locale <-> NSLocale

extension Locale: _ObjectiveCBridgeable {
    // o NSLocale do mOS só tem o atual (currentLocale/systemLocale); o identificador viaja na volta
    @_semantics("convertToObjectiveC")
    public func _bridgeToObjectiveC() -> NSLocale { NSLocale.current }

    public static func _forceBridgeFromObjectiveC(_ x: NSLocale, result: inout Locale?) {
        result = Locale(identifier: _mosString(x.localeIdentifier))
    }

    public static func _conditionallyBridgeFromObjectiveC(_ x: NSLocale, result: inout Locale?) -> Bool {
        result = Locale(identifier: _mosString(x.localeIdentifier)); return true
    }

    @_effects(readonly)
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSLocale?) -> Locale {
        guard let source else { return Locale(identifier: "") }
        return Locale(identifier: _mosString(source.localeIdentifier))
    }
}

// MARK: - TimeZone <-> NSTimeZone

extension TimeZone: _ObjectiveCBridgeable {
    @_semantics("convertToObjectiveC")
    public func _bridgeToObjectiveC() -> NSTimeZone {
        NSTimeZone(name: _mosNSString(identifier)) ?? NSTimeZone(forSecondsFromGMT: secondsFromGMT())
    }

    internal static func _mosTZ(_ x: NSTimeZone) -> TimeZone {
        TimeZone(identifier: _mosString(x.name)) ?? TimeZone(secondsFromGMT: x.secondsFromGMT) ?? TimeZone(secondsFromGMT: 0)!
    }

    public static func _forceBridgeFromObjectiveC(_ x: NSTimeZone, result: inout TimeZone?) { result = _mosTZ(x) }

    public static func _conditionallyBridgeFromObjectiveC(_ x: NSTimeZone, result: inout TimeZone?) -> Bool {
        result = _mosTZ(x); return true
    }

    @_effects(readonly)
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSTimeZone?) -> TimeZone {
        guard let source else { return TimeZone(secondsFromGMT: 0)! }
        return _mosTZ(source)
    }
}

// MARK: - IndexPath <-> NSIndexPath

extension IndexPath: _ObjectiveCBridgeable {
    public func _bridgeToObjectiveC() -> NSIndexPath {
        var idx = self.map { UInt(bitPattern: $0) }
        let n = idx.count
        if idx.isEmpty { idx = [0] }
        return idx.withUnsafeBufferPointer { NSIndexPath(indexes: $0.baseAddress!, length: n) }
    }
    public static func _forceBridgeFromObjectiveC(_ x: NSIndexPath, result: inout IndexPath?) {
        result = IndexPath(indexes: (0..<x.length).map { x.index(atPosition: $0) })
    }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSIndexPath, result: inout IndexPath?) -> Bool {
        _forceBridgeFromObjectiveC(x, result: &result); return true
    }
    @_effects(readonly)
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSIndexPath?) -> IndexPath {
        var r: IndexPath?
        if let source { _forceBridgeFromObjectiveC(source, result: &r) }
        return r ?? IndexPath()
    }
}

// MARK: - DateComponents <-> NSDateComponents (os campos que o NSDateComponents do libSystem guarda; NSDateComponentUndefined
// = Int.max = nil)

extension DateComponents: _ObjectiveCBridgeable {
    @_semantics("convertToObjectiveC")
    public func _bridgeToObjectiveC() -> NSDateComponents {
        let n = NSDateComponents()!
        func v(_ x: Int?) -> Int { x ?? Int.max }
        n.era = v(era); n.year = v(year); n.month = v(month); n.day = v(day)
        n.hour = v(hour); n.minute = v(minute); n.second = v(second); n.nanosecond = v(nanosecond)
        n.weekday = v(weekday); n.weekOfYear = v(weekOfYear); n.yearForWeekOfYear = v(yearForWeekOfYear)
        if let tz = timeZone { n.timeZone = tz._bridgeToObjectiveC() }
        return n
    }
    public static func _forceBridgeFromObjectiveC(_ x: NSDateComponents, result: inout DateComponents?) {
        result = _unconditionallyBridgeFromObjectiveC(x)
    }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSDateComponents, result: inout DateComponents?) -> Bool {
        result = _unconditionallyBridgeFromObjectiveC(x); return true
    }
    @_effects(readonly)
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSDateComponents?) -> DateComponents {
        guard let s = source else { return DateComponents() }
        func o(_ x: Int) -> Int? { x == Int.max ? nil : x }
        var d = DateComponents()
        d.era = o(s.era); d.year = o(s.year); d.month = o(s.month); d.day = o(s.day)
        d.hour = o(s.hour); d.minute = o(s.minute); d.second = o(s.second); d.nanosecond = o(s.nanosecond)
        d.weekday = o(s.weekday); d.weekOfYear = o(s.weekOfYear); d.yearForWeekOfYear = o(s.yearForWeekOfYear)
        if let tz = s.timeZone { d.timeZone = TimeZone._unconditionallyBridgeFromObjectiveC(tz) }
        return d
    }
}

// MARK: - FileHandle.write(contentsOf:) (macOS 10.15)

extension FileHandle {
    public func write<T: DataProtocol>(contentsOf data: T) throws {
        write(Data(data)._bridgeToObjectiveC())
    }
}
