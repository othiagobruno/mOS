// mOS SDK — UUID com a API do Foundation (versão 4, aleatório pelo arc4random_buf do libSystem).

public typealias uuid_t = (UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8,
                           UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8, UInt8)

@_silgen_name("arc4random_buf") private func _mos_arc4random_buf(_ buf: UnsafeMutableRawPointer, _ n: Int)

public struct UUID: Hashable, Equatable, Comparable, Sendable, Codable, CustomStringConvertible, CustomDebugStringConvertible {
    public private(set) var uuid: uuid_t

    public init() {
        var u: uuid_t = (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
        withUnsafeMutableBytes(of: &u) { _mos_arc4random_buf($0.baseAddress!, 16) }
        u.6 = (u.6 & 0x0F) | 0x40      /* versão 4 */
        u.8 = (u.8 & 0x3F) | 0x80      /* variante RFC 4122 */
        uuid = u
    }
    public init(uuid: uuid_t) { self.uuid = uuid }
    public init?(uuidString string: String) {
        var bytes: [UInt8] = []
        var hi: UInt8? = nil
        for (i, c) in string.utf8.enumerated() {
            if c == UInt8(ascii: "-") {
                guard i == 8 || i == 13 || i == 18 || i == 23 else { return nil }
                continue
            }
            let v: UInt8
            switch c {
            case UInt8(ascii: "0") ... UInt8(ascii: "9"): v = c - UInt8(ascii: "0")
            case UInt8(ascii: "a") ... UInt8(ascii: "f"): v = c - UInt8(ascii: "a") + 10
            case UInt8(ascii: "A") ... UInt8(ascii: "F"): v = c - UInt8(ascii: "A") + 10
            default: return nil
            }
            if let h = hi { bytes.append(h << 4 | v); hi = nil } else { hi = v }
        }
        guard bytes.count == 16, hi == nil, string.utf8.count == 36 else { return nil }
        var u: uuid_t = (0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0)
        withUnsafeMutableBytes(of: &u) { for i in 0 ..< 16 { $0[i] = bytes[i] } }
        uuid = u
    }

    private var bytes: [UInt8] { withUnsafeBytes(of: uuid) { Array($0) } }
    public var uuidString: String {
        let hex: [Character] = Array("0123456789ABCDEF")
        var s = ""
        for (i, b) in bytes.enumerated() {
            if i == 4 || i == 6 || i == 8 || i == 10 { s.append("-") }
            s.append(hex[Int(b >> 4)]); s.append(hex[Int(b & 15)])
        }
        return s
    }
    public var description: String { uuidString }
    public var debugDescription: String { uuidString }

    public static func == (l: UUID, r: UUID) -> Bool { l.bytes == r.bytes }
    public static func < (l: UUID, r: UUID) -> Bool { l.bytes.lexicographicallyPrecedes(r.bytes) }
    public func hash(into hasher: inout Hasher) { hasher.combine(bytes) }

    public init(from decoder: any Decoder) throws {
        let s = try decoder.singleValueContainer().decode(String.self)
        guard let u = UUID(uuidString: s) else {
            throw DecodingError.dataCorrupted(DecodingError.Context(codingPath: decoder.codingPath, debugDescription: "UUID inválido: \(s)"))
        }
        self = u
    }
    public func encode(to encoder: any Encoder) throws { var c = encoder.singleValueContainer(); try c.encode(uuidString) }
}
