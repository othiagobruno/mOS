// Shim de Foundation com -module-abi-name Foundation ($s10Foundation…).
// Não é o Foundation da Apple. Serve para o mangling do CryptoKit bater
// com o que o VS Code importa.
import Swift

public protocol ContiguousBytes {
    func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R
}
public protocol DataProtocol: Collection where Element == UInt8, Index == Int {
    subscript(bounds: Range<Int>) -> Data { get }
    subscript(bounds: PartialRangeFrom<Int>) -> Data { get }
}
public struct Data: DataProtocol, ContiguousBytes, Equatable, Sendable {
    public typealias Element = UInt8
    public typealias Iterator = Array<UInt8>.Iterator
    public typealias Index = Int
    public init() {}
    public init(_ bytes: Array<UInt8>) {}
    public init<S: Sequence>(_ elements: S) where S.Element == UInt8 {}
    public var count: Int { Int() }
    public var startIndex: Int { Int() }
    public var endIndex: Int { Int() }
    public func index(after i: Int) -> Int { i }
    public subscript(position: Int) -> UInt8 { UInt8() }
    public subscript(bounds: Range<Int>) -> Data { Data() }
    public subscript(bounds: PartialRangeFrom<Int>) -> Data { Data() }
    public func prefix(_ n: Int) -> Data { Data() }
    public func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R {
        try body(UnsafeRawBufferPointer())
    }
    public func makeIterator() -> Iterator { Iterator(_elements: Array<UInt8>()) }
    public static func + (lhs: Data, rhs: Data) -> Data { Data() }
    public static func == (lhs: Data, rhs: Data) -> Bool { Bool() }
}
