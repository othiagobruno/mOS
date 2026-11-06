// mOS SDK — JSONSerialization (o JSON "sem tipo" do Foundation clássico: [String: Any], [Any], String, NSNumber/Int/
// Double/Bool, NSNull). O JSONEncoder/JSONDecoder tipados vêm do FoundationEssentials.

open class JSONSerialization: NSObject, @unchecked Sendable {
    public struct ReadingOptions: OptionSet, Sendable {
        public let rawValue: UInt
        public init(rawValue: UInt) { self.rawValue = rawValue }
        public static let mutableContainers = ReadingOptions(rawValue: 1 << 0)
        public static let mutableLeaves = ReadingOptions(rawValue: 1 << 1)
        public static let fragmentsAllowed = ReadingOptions(rawValue: 1 << 2)
        public static let json5Allowed = ReadingOptions(rawValue: 1 << 3)
        public static let topLevelDictionaryAssumed = ReadingOptions(rawValue: 1 << 4)
        public static let allowFragments = fragmentsAllowed
    }
    public struct WritingOptions: OptionSet, Sendable {
        public let rawValue: UInt
        public init(rawValue: UInt) { self.rawValue = rawValue }
        public static let prettyPrinted = WritingOptions(rawValue: 1 << 0)
        public static let sortedKeys = WritingOptions(rawValue: 1 << 1)
        public static let fragmentsAllowed = WritingOptions(rawValue: 1 << 2)
        public static let withoutEscapingSlashes = WritingOptions(rawValue: 1 << 3)
    }

    public enum _Error: Error, CustomStringConvertible {
        case invalidObject(String), syntax(Int, String)
        public var description: String {
            switch self {
            case .invalidObject(let t): "tipo inválido para JSON: \(t)"
            case .syntax(let at, let why): "JSON inválido na posição \(at): \(why)"
            }
        }
    }

    open class func isValidJSONObject(_ obj: Any) -> Bool {
        guard obj is [Any] || obj is [String: Any] || obj is NSArray || obj is NSDictionary else { return false }
        var s = ""
        return (try? write(obj, options: [], indent: 0, out: &s)) != nil
    }

    open class func data(withJSONObject obj: Any, options opt: WritingOptions = []) throws -> Data {
        if !opt.contains(.fragmentsAllowed), !(obj is [Any] || obj is [String: Any] || obj is NSArray || obj is NSDictionary) {
            throw _Error.invalidObject("fragmento no nível de cima: \(type(of: obj))")
        }
        var out = ""
        try write(obj, options: opt, indent: 0, out: &out)
        return Data(out.utf8)
    }

    private static func escape(_ s: String, _ opt: WritingOptions, _ out: inout String) {
        out += "\""
        for u in s.unicodeScalars {
            switch u {
            case "\"": out += "\\\""
            case "\\": out += "\\\\"
            case "/": out += opt.contains(.withoutEscapingSlashes) ? "/" : "\\/"
            case "\n": out += "\\n"
            case "\r": out += "\\r"
            case "\t": out += "\\t"
            case "\u{08}": out += "\\b"
            case "\u{0C}": out += "\\f"
            default:
                if u.value < 0x20 {
                    let h = String(u.value, radix: 16)
                    out += "\\u" + String(repeating: "0", count: 4 - h.count) + h
                } else { out.unicodeScalars.append(u) }
            }
        }
        out += "\""
    }

    private static func number(_ d: Double) throws -> String {
        guard d.isFinite else { throw _Error.invalidObject("número não finito") }
        if d == d.rounded(), abs(d) < 1e15 { return String(Int64(d)) }
        return "\(d)"
    }

    private static func write(_ obj: Any, options opt: WritingOptions, indent: Int, out: inout String) throws {
        let pretty = opt.contains(.prettyPrinted)
        let pad = { (n: Int) -> String in pretty ? "\n" + String(repeating: "  ", count: n) : "" }
        switch obj {
        case is NSNull: out += "null"
        case let n as NSNumber:
            if n._isBool { out += n.boolValue ? "true" : "false" }
            else if n._isFloating { out += try number(n.doubleValue) }
            else { out += n.stringValue }
        case let b as Bool: out += b ? "true" : "false"
        case let i as Int: out += String(i)
        case let i as Int64: out += String(i)
        case let i as Int32: out += String(i)
        case let u as UInt: out += String(u)
        case let d as Double: out += try number(d)
        case let f as Float: out += try number(Double(f))
        case let f as CGFloat: out += try number(Double(f))
        case let s as String: escape(s, opt, &out)
        case let s as NSString: escape(s.description, opt, &out)
        case let a as [Any]:
            if a.isEmpty { out += "[]"; return }
            out += "["
            for (i, e) in a.enumerated() {
                if i > 0 { out += "," }
                out += pad(indent + 1)
                try write(e, options: opt, indent: indent + 1, out: &out)
            }
            out += pad(indent) + "]"
        case let d as [String: Any]:
            if d.isEmpty { out += "{}"; return }
            out += "{"
            let keys = opt.contains(.sortedKeys) ? d.keys.sorted() : Array(d.keys)
            for (i, k) in keys.enumerated() {
                if i > 0 { out += "," }
                out += pad(indent + 1)
                escape(k, opt, &out)
                out += pretty ? " : " : ":"
                try write(d[k]!, options: opt, indent: indent + 1, out: &out)
            }
            out += pad(indent) + "}"
        // as classes NS* por último: [Any]/[String: Any] também casam com elas pela ponte s (recursão infinita)
        case let a as NSArray: try write(a._elements, options: opt, indent: indent, out: &out)
        case let d as NSDictionary:
            var m: [String: Any] = [:]
            for (k, v) in d._storage { guard let ks = k.base as? String else { throw _Error.invalidObject("chave não String") }; m[ks] = v }
            try write(m, options: opt, indent: indent, out: &out)
        default: throw _Error.invalidObject(_typeName(type(of: obj)))
        }
    }

    open class func jsonObject(with data: Data, options opt: ReadingOptions = []) throws -> Any {
        var p = _Parser(bytes: Array(data))
        p.skipBOM()
        p.ws()
        let v = try p.value()
        p.ws()
        guard p.i == p.b.count else { throw _Error.syntax(p.i, "lixo depois do valor") }
        if !opt.contains(.fragmentsAllowed), !(v is [Any] || v is [String: Any]) { throw _Error.syntax(0, "fragmento no nível de cima") }
        return v
    }

    private struct _Parser {
        let b: [UInt8]
        var i = 0
        init(bytes: [UInt8]) { b = bytes }
        mutating func skipBOM() { if b.count >= 3, b[0] == 0xEF, b[1] == 0xBB, b[2] == 0xBF { i = 3 } }
        mutating func ws() { while i < b.count, b[i] == 0x20 || b[i] == 0x0A || b[i] == 0x0D || b[i] == 0x09 { i += 1 } }
        mutating func expect(_ c: UInt8) throws { guard i < b.count, b[i] == c else { throw _Error.syntax(i, "esperava '\(Character(Unicode.Scalar(c)))'") }; i += 1 }
        mutating func lit(_ s: String, _ v: Any) throws -> Any {
            let u = Array(s.utf8)
            guard i + u.count <= b.count, Array(b[i ..< i + u.count]) == u else { throw _Error.syntax(i, "valor inválido") }
            i += u.count
            return v
        }
        mutating func value() throws -> Any {
            guard i < b.count else { throw _Error.syntax(i, "fim inesperado") }
            switch b[i] {
            case UInt8(ascii: "{"):
                i += 1; ws()
                var d: [String: Any] = [:]
                if i < b.count, b[i] == UInt8(ascii: "}") { i += 1; return d }
                while true {
                    ws(); let k = try string(); ws(); try expect(UInt8(ascii: ":")); ws()
                    d[k] = try value(); ws()
                    if i < b.count, b[i] == UInt8(ascii: ",") { i += 1; continue }
                    try expect(UInt8(ascii: "}")); return d
                }
            case UInt8(ascii: "["):
                i += 1; ws()
                var a: [Any] = []
                if i < b.count, b[i] == UInt8(ascii: "]") { i += 1; return a }
                while true {
                    ws(); a.append(try value()); ws()
                    if i < b.count, b[i] == UInt8(ascii: ",") { i += 1; continue }
                    try expect(UInt8(ascii: "]")); return a
                }
            case UInt8(ascii: "\""): return try string()
            case UInt8(ascii: "t"): return try lit("true", NSNumber(value: true))
            case UInt8(ascii: "f"): return try lit("false", NSNumber(value: false))
            case UInt8(ascii: "n"): return try lit("null", NSNull())
            default: return try number()
            }
        }
        mutating func number() throws -> Any {
            let start = i
            var isFloat = false
            while i < b.count {
                let c = b[i]
                if (c >= 0x30 && c <= 0x39) || c == UInt8(ascii: "-") || c == UInt8(ascii: "+") { i += 1 }
                else if c == UInt8(ascii: ".") || c == UInt8(ascii: "e") || c == UInt8(ascii: "E") { isFloat = true; i += 1 }
                else { break }
            }
            let s = String(decoding: b[start ..< i], as: UTF8.self)
            if !isFloat, let v = Int(s) { return NSNumber(value: v) }
            guard let d = Double(s) else { throw _Error.syntax(start, "número inválido") }
            return NSNumber(value: d)
        }
        mutating func hex4() throws -> UInt32 {
            guard i + 4 <= b.count, let v = UInt32(String(decoding: b[i ..< i + 4], as: UTF8.self), radix: 16) else { throw _Error.syntax(i, "\\u inválido") }
            i += 4
            return v
        }
        mutating func string() throws -> String {
            try expect(UInt8(ascii: "\""))
            var out = [UInt8]()
            while i < b.count {
                let c = b[i]; i += 1
                if c == UInt8(ascii: "\"") { return String(decoding: out, as: UTF8.self) }
                if c != UInt8(ascii: "\\") { out.append(c); continue }
                guard i < b.count else { break }
                let e = b[i]; i += 1
                switch e {
                case UInt8(ascii: "n"): out.append(0x0A)
                case UInt8(ascii: "r"): out.append(0x0D)
                case UInt8(ascii: "t"): out.append(0x09)
                case UInt8(ascii: "b"): out.append(0x08)
                case UInt8(ascii: "f"): out.append(0x0C)
                case UInt8(ascii: "u"):
                    var v = try hex4()
                    if v >= 0xD800, v < 0xDC00, i + 6 <= b.count, b[i] == UInt8(ascii: "\\"), b[i + 1] == UInt8(ascii: "u") {
                        i += 2
                        let lo = try hex4()
                        v = 0x10000 + ((v - 0xD800) << 10) + (lo - 0xDC00)
                    }
                    out.append(contentsOf: Array(String(Character(Unicode.Scalar(v) ?? "\u{FFFD}")).utf8))
                default: out.append(e)
                }
            }
            throw _Error.syntax(i, "string sem fim")
        }
    }
}
