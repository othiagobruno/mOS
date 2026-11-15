// mOS — overlay Foundation: pontes ObjC que o swift-foundation só tem com FOUNDATION_FRAMEWORK (URLQueryItem,
// URLComponents, Calendar, CharacterSet), sobre as classes do libSystem. O NSCharacterSet do libSystem só conhece os
// conjuntos prontos: o CharacterSet vira uma subclasse Swift que responde -characterIsMember: por ele.

extension URLQueryItem: _ObjectiveCBridgeable {
    public typealias _ObjectiveCType = NSURLQueryItem
    @_semantics("convertToObjectiveC")
    public func _bridgeToObjectiveC() -> NSURLQueryItem {
        NSURLQueryItem(name: _mosNSString(name), value: value.map(_mosNSString))
    }
    public static func _forceBridgeFromObjectiveC(_ x: NSURLQueryItem, result: inout URLQueryItem?) {
        result = URLQueryItem(name: _mosString(x.name), value: x.value.map(_mosString))
    }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSURLQueryItem, result: inout URLQueryItem?) -> Bool {
        _forceBridgeFromObjectiveC(x, result: &result); return true
    }
    @_effects(readonly)
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSURLQueryItem?) -> URLQueryItem {
        var r: URLQueryItem?
        if let source { _forceBridgeFromObjectiveC(source, result: &r) }
        return r ?? URLQueryItem(name: "", value: nil)
    }
}

extension URLComponents: _ObjectiveCBridgeable {
    public typealias _ObjectiveCType = NSURLComponents
    @_semantics("convertToObjectiveC")
    public func _bridgeToObjectiveC() -> NSURLComponents {
        NSURLComponents(string: _mosNSString(string ?? "")) ?? NSURLComponents(string: _mosNSString(""))!
    }
    public static func _forceBridgeFromObjectiveC(_ x: NSURLComponents, result: inout URLComponents?) {
        result = URLComponents(string: x.string.map(_mosString) ?? "") ?? URLComponents()
    }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSURLComponents, result: inout URLComponents?) -> Bool {
        _forceBridgeFromObjectiveC(x, result: &result); return true
    }
    @_effects(readonly)
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSURLComponents?) -> URLComponents {
        var r: URLComponents?
        if let source { _forceBridgeFromObjectiveC(source, result: &r) }
        return r ?? URLComponents()
    }
}

// MARK: - Calendar

/// os identificadores do CFCalendar (kCFGregorianCalendar = "gregorian"...)
private let _mosCalendarIDs: [(Calendar.Identifier, String)] = [
    (.gregorian, "gregorian"), (.buddhist, "buddhist"), (.chinese, "chinese"), (.coptic, "coptic"),
    (.ethiopicAmeteMihret, "ethiopic"), (.ethiopicAmeteAlem, "ethiopic-amete-alem"), (.hebrew, "hebrew"),
    (.iso8601, "iso8601"), (.indian, "indian"), (.islamic, "islamic"), (.islamicCivil, "islamic-civil"),
    (.japanese, "japanese"), (.persian, "persian"), (.republicOfChina, "roc"), (.islamicTabular, "islamic-tbla"),
    (.islamicUmmAlQura, "islamic-umalqura"),
]

extension Calendar: _ObjectiveCBridgeable {
    public typealias _ObjectiveCType = NSCalendar
    @_semantics("convertToObjectiveC")
    public func _bridgeToObjectiveC() -> NSCalendar {
        let id = _mosCalendarIDs.first { $0.0 == identifier }?.1 ?? "gregorian"
        let c = NSCalendar(calendarIdentifier: _mosNSString(id)) ?? NSCalendar(calendarIdentifier: _mosNSString("gregorian"))!
        c.timeZone = timeZone._bridgeToObjectiveC()
        return c
    }
    public static func _forceBridgeFromObjectiveC(_ x: NSCalendar, result: inout Calendar?) {
        let id = _mosString(x.calendarIdentifier)
        var c = Calendar(identifier: _mosCalendarIDs.first { $0.1 == id }?.0 ?? .gregorian)
        c.timeZone = TimeZone._unconditionallyBridgeFromObjectiveC(x.timeZone)
        result = c
    }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSCalendar, result: inout Calendar?) -> Bool {
        _forceBridgeFromObjectiveC(x, result: &result); return true
    }
    @_effects(readonly)
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSCalendar?) -> Calendar {
        var r: Calendar?
        if let source { _forceBridgeFromObjectiveC(source, result: &r) }
        return r ?? Calendar(identifier: .gregorian)
    }
}

// MARK: - CharacterSet

/// NSCharacterSet que responde pelo CharacterSet Swift (o -characterIsMember: do libSystem só conhece os prontos)
internal final class _MOSSwiftCharacterSet: NSCharacterSet {
    let _set: CharacterSet
    init(_ set: CharacterSet) { _set = set; super.init() }
    override func characterIsMember(_ c: unichar) -> Bool {
        guard let u = Unicode.Scalar(UInt32(c)) else { return false }
        return _set.contains(u)
    }
}

extension CharacterSet: _ObjectiveCBridgeable {
    public typealias _ObjectiveCType = NSCharacterSet
    @_semantics("convertToObjectiveC")
    public func _bridgeToObjectiveC() -> NSCharacterSet { _MOSSwiftCharacterSet(self) }
    public static func _forceBridgeFromObjectiveC(_ x: NSCharacterSet, result: inout CharacterSet?) {
        if let s = x as? _MOSSwiftCharacterSet { result = s._set; return }
        // conjunto do libSystem: lido no plano básico (é onde estão os prontos: espaço, fim de linha...)
        var cs = CharacterSet()
        var lo: UInt32? = nil
        for v in UInt32(0)...0xFFFF {
            let m = (v < 0xD800 || v > 0xDFFF) && x.characterIsMember(unichar(v))
            if m, lo == nil { lo = v }
            if !m, let l = lo { cs.insert(charactersIn: Unicode.Scalar(l)!...Unicode.Scalar(v - 1)!); lo = nil }
        }
        if let l = lo { cs.insert(charactersIn: Unicode.Scalar(l)!...Unicode.Scalar(0xFFFF)!) }
        result = cs
    }
    public static func _conditionallyBridgeFromObjectiveC(_ x: NSCharacterSet, result: inout CharacterSet?) -> Bool {
        _forceBridgeFromObjectiveC(x, result: &result); return true
    }
    @_effects(readonly)
    public static func _unconditionallyBridgeFromObjectiveC(_ source: NSCharacterSet?) -> CharacterSet {
        var r: CharacterSet?
        if let source { _forceBridgeFromObjectiveC(source, result: &r) }
        return r ?? CharacterSet()
    }
}
