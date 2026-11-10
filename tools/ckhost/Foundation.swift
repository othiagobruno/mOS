// Foundation mínimo, só para o teste de host conferir o mangling da Apple ($s10Foundation…).
// No mOS o módulo de verdade é o overlay (module-abi-name Foundation); este arquivo não entra nele.
public protocol ContiguousBytes {
    func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R
}
public protocol DataProtocol: Collection where Element == UInt8, Index == Int {}

public struct Data: DataProtocol, ContiguousBytes, Equatable, Sendable {
    var b: [UInt8]
    public init() { b = [] }
    public init(_ bytes: [UInt8]) { b = bytes }
    public init<S: Sequence>(_ elements: S) where S.Element == UInt8 { b = Array(elements) }
    public var startIndex: Int { 0 }
    public var endIndex: Int { b.count }
    public func index(after i: Int) -> Int { i + 1 }
    public subscript(position: Int) -> UInt8 { b[position] }
    public subscript(bounds: Range<Int>) -> Data { Data(Array(b[bounds])) }
    public func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R {
        try b.withUnsafeBytes(body)
    }
    public static func + (lhs: Data, rhs: Data) -> Data { Data(lhs.b + rhs.b) }
    public static func == (lhs: Data, rhs: Data) -> Bool { lhs.b == rhs.b }
}
