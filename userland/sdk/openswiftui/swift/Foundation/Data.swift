// mOS SDK — Data (bytes contíguos, valor com cópia na escrita via [UInt8]) e String.Encoding, com a API
// do Foundation que as bibliotecas do OpenSwiftUI usam.

@frozen
public struct Data: Hashable, Sendable, RandomAccessCollection, MutableCollection, RangeReplaceableCollection, Codable,
    ExpressibleByArrayLiteral, CustomStringConvertible {
    public typealias Index = Int
    public typealias Element = UInt8
    public typealias Indices = Range<Int>
    @usableFromInline var _bytes: [UInt8]

    public init() { _bytes = [] }
    public init(_ bytes: some Sequence<UInt8>) { _bytes = Array(bytes) }
    public init(bytes: UnsafeRawPointer, count: Int) { _bytes = Array(UnsafeRawBufferPointer(start: bytes, count: count)) }
    public init(buffer: UnsafeBufferPointer<some Any>) { _bytes = Array(UnsafeRawBufferPointer(buffer)) }
    public init(count: Int) { _bytes = Array(repeating: 0, count: count) }
    public init(repeating: UInt8, count: Int) { _bytes = Array(repeating: repeating, count: count) }
    public init(arrayLiteral elements: UInt8...) { _bytes = elements }
    public init(_ cfData: CFData) {
        let n = CFDataGetLength(cfData)
        if let p = CFDataGetBytePtr(cfData), n > 0 { _bytes = Array(UnsafeBufferPointer(start: p, count: n)) } else { _bytes = [] }
    }

    public var startIndex: Int { 0 }
    public var endIndex: Int { _bytes.count }
    public var count: Int { _bytes.count }
    public var isEmpty: Bool { _bytes.isEmpty }
    public subscript(position: Int) -> UInt8 {
        get { _bytes[position] }
        set { _bytes[position] = newValue }
    }
    public subscript(bounds: Range<Int>) -> Data {
        get { Data(_bytes[bounds]) }
        set { _bytes.replaceSubrange(bounds, with: newValue._bytes) }
    }
    public mutating func replaceSubrange<C: Collection>(_ subrange: Range<Int>, with newElements: C) where C.Element == UInt8 {
        _bytes.replaceSubrange(subrange, with: newElements)
    }
    public mutating func append(_ other: Data) { _bytes.append(contentsOf: other._bytes) }
    public mutating func append(_ bytes: UnsafePointer<UInt8>, count: Int) { _bytes.append(contentsOf: UnsafeBufferPointer(start: bytes, count: count)) }
    public mutating func append(contentsOf bytes: [UInt8]) { _bytes.append(contentsOf: bytes) }
    public mutating func resetBytes(in range: Range<Int>) { for i in range { _bytes[i] = 0 } }

    public func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R { try _bytes.withUnsafeBytes(body) }
    public mutating func withUnsafeMutableBytes<R>(_ body: (UnsafeMutableRawBufferPointer) throws -> R) rethrows -> R { try _bytes.withUnsafeMutableBytes(body) }
    public func copyBytes(to pointer: UnsafeMutablePointer<UInt8>, count: Int) {
        _bytes.withUnsafeBufferPointer { b in pointer.update(from: b.baseAddress!, count: Swift.min(count, b.count)) }
    }

    /// Cópia em CFData (objeto CF do mOS).
    public var cfData: CFData { _bytes.withUnsafeBufferPointer { CFDataCreate(nil, $0.baseAddress, $0.count) } }

    public var description: String { "\(count) bytes" }

    public init(from decoder: any Decoder) throws { _bytes = try [UInt8](from: decoder) }
    public func encode(to encoder: any Encoder) throws { try _bytes.encode(to: encoder) }
}

extension String {
    public struct Encoding: RawRepresentable, Hashable, Sendable {
        public var rawValue: UInt
        public init(rawValue: UInt) { self.rawValue = rawValue }
        public static let ascii = Encoding(rawValue: 1)
        public static let utf8 = Encoding(rawValue: 4)
        public static let isoLatin1 = Encoding(rawValue: 5)
        public static let unicode = Encoding(rawValue: 10)
        public static let utf16 = Encoding(rawValue: 10)
        public static let utf16BigEndian = Encoding(rawValue: 0x9000_0100)
        public static let utf16LittleEndian = Encoding(rawValue: 0x9400_0100)
        public static let utf32 = Encoding(rawValue: 0x8C00_0100)
    }

    /// Bytes terminados em NUL na codificação pedida (UTF-8/ASCII/Latin-1; nil se não representável).
    public func cString(using encoding: Encoding) -> [CChar]? {
        switch encoding {
        case .utf8: return utf8.map { CChar(bitPattern: $0) } + [0]
        case .ascii:
            var r: [CChar] = []
            for u in unicodeScalars { guard u.value < 0x80 else { return nil }; r.append(CChar(u.value)) }
            return r + [0]
        case .isoLatin1:
            var r: [CChar] = []
            for u in unicodeScalars { guard u.value < 0x100 else { return nil }; r.append(CChar(bitPattern: UInt8(u.value))) }
            return r + [0]
        default: return nil
        }
    }
    public func data(using encoding: Encoding, allowLossyConversion: Bool = false) -> Data? {
        guard let c = cString(using: encoding) else { return nil }
        return Data(c.dropLast().map { UInt8(bitPattern: $0) })
    }
    public init?(data: Data, encoding: Encoding) {
        switch encoding {
        case .utf8, .ascii: self = String(decoding: data, as: UTF8.self)
        case .isoLatin1: self = String(data.map { Character(Unicode.Scalar($0)) })
        default: return nil
        }
    }
    public init?(bytes: some Sequence<UInt8>, encoding: Encoding) { self.init(data: Data(bytes), encoding: encoding) }
    public init?(cString: UnsafePointer<CChar>, encoding: Encoding) {
        self.init(data: Data(bytes: cString, count: strlen(cString)), encoding: encoding)
    }
    public var utf8CString_mos: ContiguousArray<CChar> { utf8CString }
}

@_silgen_name("strlen") private func strlen(_ s: UnsafePointer<CChar>) -> Int

extension String {
    /// printf do libSystem (sem %@: não há objetos ObjC sem interoperabilidade).
    public init(format: String, _ arguments: CVarArg...) { self.init(format: format, arguments: arguments) }
    public init(format: String, arguments: [CVarArg]) {
        self = withVaList(arguments) { va in
            format.withCString { f in
                var buffer = [CChar](repeating: 0, count: 256)
                let n = Int(vsnprintf(&buffer, buffer.count, f, va))
                if n < buffer.count { return String(cString: buffer) }
                return withVaList(arguments) { va2 in
                    var big = [CChar](repeating: 0, count: n + 1)
                    _ = vsnprintf(&big, big.count, f, va2)
                    return String(cString: big)
                }
            }
        }
    }
}
