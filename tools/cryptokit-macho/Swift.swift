// Stub da biblioteca padrão Swift, só para o swift-frontend oficial emitir
// Mach-O x86_64-apple-macosx sem o SDK da Apple. Não é a stdlib da Apple.
// Bool guarda Builtin.Int1 para o SILGen baixar condicionais. O CryptoKit.swift
// do repo é compilado em cima disto e do Foundation.swift ao lado.

precedencegroup AssignmentPrecedence { assignment: true associativity: right }
precedencegroup LogicalDisjunctionPrecedence { higherThan: AssignmentPrecedence associativity: left }
precedencegroup LogicalConjunctionPrecedence { higherThan: LogicalDisjunctionPrecedence associativity: left }
precedencegroup ComparisonPrecedence { higherThan: LogicalConjunctionPrecedence }
precedencegroup NilCoalescingPrecedence { higherThan: ComparisonPrecedence associativity: right }
precedencegroup AdditionPrecedence { higherThan: NilCoalescingPrecedence associativity: left }
precedencegroup MultiplicationPrecedence { higherThan: AdditionPrecedence associativity: left }
precedencegroup BitwiseShiftPrecedence { higherThan: MultiplicationPrecedence }
infix operator == : ComparisonPrecedence
infix operator < : ComparisonPrecedence
infix operator <= : ComparisonPrecedence
infix operator >= : ComparisonPrecedence
infix operator > : ComparisonPrecedence
infix operator ?? : NilCoalescingPrecedence
infix operator && : LogicalConjunctionPrecedence
infix operator || : LogicalDisjunctionPrecedence
infix operator + : AdditionPrecedence
infix operator - : AdditionPrecedence
infix operator * : MultiplicationPrecedence
infix operator / : MultiplicationPrecedence
infix operator % : MultiplicationPrecedence
infix operator << : BitwiseShiftPrecedence
infix operator >> : BitwiseShiftPrecedence
infix operator & : AdditionPrecedence
infix operator ^ : AdditionPrecedence
infix operator &+= : AssignmentPrecedence
infix operator += : AssignmentPrecedence
infix operator ..< : ComparisonPrecedence
infix operator ... : ComparisonPrecedence
postfix operator ...

public protocol _ExpressibleByBuiltinIntegerLiteral {
    init(_builtinIntegerLiteral value: Builtin.IntLiteral)
}
public protocol ExpressibleByIntegerLiteral: _ExpressibleByBuiltinIntegerLiteral {
    associatedtype IntegerLiteralType
    init(integerLiteral value: IntegerLiteralType)
}
public protocol _ExpressibleByBuiltinStringLiteral {
    init(_builtinStringLiteral start: Builtin.RawPointer, utf8CodeUnitCount: Builtin.Word, isASCII: Builtin.Int1)
}
public protocol ExpressibleByStringLiteral: _ExpressibleByBuiltinStringLiteral {
    associatedtype StringLiteralType: _ExpressibleByBuiltinStringLiteral
    init(stringLiteral value: StringLiteralType)
}
public typealias StringLiteralType = String
public protocol Equatable {}
public protocol Comparable {}
public protocol Error {}
public protocol Sendable {}
public protocol Hashable: Equatable {
    func hash(into hasher: inout Hasher)
}
public protocol IteratorProtocol {
    associatedtype Element
    mutating func next() -> Element?
}
public protocol Sequence {
    associatedtype Element
    associatedtype Iterator: IteratorProtocol where Iterator.Element == Element
    func makeIterator() -> Iterator
}
public protocol Collection: Sequence {
    associatedtype Index
    var startIndex: Index { get }
    var endIndex: Index { get }
    func index(after i: Index) -> Index
    subscript(position: Index) -> Element { get }
}
public protocol CustomStringConvertible {
    var description: String { get }
}
struct _Lit: _ExpressibleByBuiltinIntegerLiteral, ExpressibleByIntegerLiteral {
    var bit: Builtin.Int1
    typealias IntegerLiteralType = _Lit
    init(_builtinIntegerLiteral value: Builtin.IntLiteral) {
        bit = Builtin.s_to_s_checked_trunc_IntLiteral_Int1(value).0
    }
    init(integerLiteral value: _Lit) { bit = value.bit }
}
public struct Bool {
    public var _value: Builtin.Int1
    public init(_ v: Builtin.Int1) { _value = v }
    public init() {
        let lit: _Lit = 0
        _value = lit.bit
    }
}
public struct Int: ExpressibleByIntegerLiteral, Equatable, Comparable {
    public init() {}
    public init(_ v: Int32) {}
    public init(_ v: UInt8) {}
    public init(integerLiteral value: Int) {}
    public init(_builtinIntegerLiteral value: Builtin.IntLiteral) {}
}
public struct Int32: ExpressibleByIntegerLiteral, Equatable {
    public init() {}
    public init(integerLiteral value: Int32) {}
    public init(_builtinIntegerLiteral value: Builtin.IntLiteral) {}
}
public struct UInt8: ExpressibleByIntegerLiteral, Equatable {
    public init() {}
    public init(integerLiteral value: UInt8) {}
    public init(_builtinIntegerLiteral value: Builtin.IntLiteral) {}
}
public struct UInt32: ExpressibleByIntegerLiteral {
    public init() {}
    public init(_ v: Int) {}
    public init(integerLiteral value: UInt32) {}
    public init(_builtinIntegerLiteral value: Builtin.IntLiteral) {}
}
public struct UInt64: ExpressibleByIntegerLiteral, Equatable {
    public init() {}
    public init(integerLiteral value: UInt64) {}
    public init(_builtinIntegerLiteral value: Builtin.IntLiteral) {}
}
public typealias IntegerLiteralType = Int
public protocol _ExpressibleByBuiltinFloatLiteral {
    init(_builtinFloatLiteral value: Builtin.IntLiteral)
}
public protocol ExpressibleByFloatLiteral: _ExpressibleByBuiltinFloatLiteral {
    associatedtype FloatLiteralType: _ExpressibleByBuiltinFloatLiteral
    init(floatLiteral value: FloatLiteralType)
}
public typealias FloatLiteralType = Int
public struct Character: Equatable {}
public struct String: ExpressibleByStringLiteral, Sequence, Equatable {
    public typealias Element = Character
    public typealias Iterator = Array<Character>.Iterator
    public typealias StringLiteralType = String
    public init() {}
    public init(stringLiteral value: String) {}
    public init(_builtinStringLiteral start: Builtin.RawPointer, utf8CodeUnitCount: Builtin.Word, isASCII: Builtin.Int1) {}
    public mutating func append(_ c: Character) {}
    public func makeIterator() -> Iterator { Iterator() }
    public subscript(i: Int) -> Character { Character() }
}
public struct Array<T>: Sequence {
    public struct Iterator: IteratorProtocol {
        public typealias Element = T
        public init() {}
        public mutating func next() -> T? { Optional<T>.none }
    }
    public typealias Element = T
    public init() {}
    public init(repeating: T, count: Int) {}
    public init<S: Sequence>(_ s: S) where S.Element == T {}
    public mutating func append(_ e: T) {}
    public mutating func append<S: Sequence>(contentsOf s: S) where S.Element == T {}
    public mutating func reserveCapacity(_ n: Int) {}
    public var count: Int { Int() }
    public var isEmpty: Bool { Bool() }
    public func prefix(_ n: Int) -> Array<T> { Array() }
    public func map<U>(_ body: (T) -> U) -> Array<U> { Array<U>() }
    public func makeIterator() -> Iterator { Iterator() }
    public subscript(i: Int) -> T { get { _elem() } set {} }
    public subscript(bounds: Range<Int>) -> Array<T> { Array() }
    public subscript(bounds: PartialRangeFrom<Int>) -> Array<T> { Array() }
    public mutating func withUnsafeMutableBytes<R>(_ body: (UnsafeMutableRawBufferPointer) throws -> R) rethrows -> R { try body(UnsafeMutableRawBufferPointer()) }
    public func withUnsafeBytes<R>(_ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R { try body(UnsafeRawBufferPointer()) }
    public func withUnsafeBufferPointer<R>(_ body: (UnsafeBufferPointer<T>) throws -> R) rethrows -> R { try body(UnsafeBufferPointer()) }
    public mutating func withUnsafeMutableBufferPointer<R>(_ body: (UnsafeMutableBufferPointer<T>) throws -> R) rethrows -> R { try body(UnsafeMutableBufferPointer()) }
}
public struct Hasher {
    public mutating func combine(_ v: UInt64) {}
    public mutating func combine(_ v: Array<UInt8>) {}
}
public struct UnsafeMutableRawPointer {}
public struct UnsafeRawPointer {}
public struct UnsafeRawBufferPointer: Sequence {
    public typealias Element = UInt8
    public typealias Iterator = Array<UInt8>.Iterator
    public init() {}
    public func makeIterator() -> Iterator { Iterator() }
    public var baseAddress: UnsafeRawPointer? { Optional<UnsafeRawPointer>.none }
    public var count: Int { Int() }
}
public struct UnsafeMutableRawBufferPointer {
    public init() {}
    public var baseAddress: UnsafeMutableRawPointer? { Optional<UnsafeMutableRawPointer>.none }
}
public struct UnsafeMutablePointer<Pointee> {}
public struct UnsafePointer<Pointee> {}
public struct UnsafeBufferPointer<Element> {
    public init() {}
    public var baseAddress: UnsafePointer<Element>? { Optional<UnsafePointer<Element>>.none }
    public var count: Int { Int() }
}
public struct UnsafeMutableBufferPointer<Element> {
    public init() {}
    public var baseAddress: UnsafeMutablePointer<Element>? { Optional<UnsafeMutablePointer<Element>>.none }
}
public protocol ExpressibleByNilLiteral {
    init(nilLiteral: ())
}
public enum Optional<Wrapped>: ExpressibleByNilLiteral {
    case none
    case some(Wrapped)
    public init(nilLiteral: ()) { self = .none }
    public init(_ v: Wrapped) { self = .some(v) }
}
public struct Range<Bound> {}
public struct ClosedRange<Bound> {}
public struct PartialRangeFrom<Bound> {}
public struct PartialRangeThrough<Bound> {}
public func min(_ a: Int, _ b: Int) -> Int { a }
public func precondition(_ c: Bool) {}
public func precondition(_ c: Bool, _ m: String) {}
func _elem<Element>() -> Element { _elem() }
public func _diagnoseUnexpectedNilOptional(_filenameStart: Builtin.RawPointer, _filenameLength: Builtin.Word, _filenameIsASCII: Builtin.Int1, _line: Builtin.Word, _isImplicitUnwrap: Builtin.Int1) {}
public func withUnsafeBytes<T, R>(of value: T, _ body: (UnsafeRawBufferPointer) throws -> R) rethrows -> R { try body(UnsafeRawBufferPointer()) }
public func withUnsafeMutableBytes<T, R>(of value: inout T, _ body: (UnsafeMutableRawBufferPointer) throws -> R) rethrows -> R { try body(UnsafeMutableRawBufferPointer()) }
public func ?? <T>(l: T?, r: @autoclosure () -> T) -> T { r() }
public func && (l: Bool, r: Bool) -> Bool { l }
public func || (l: Bool, r: Bool) -> Bool { l }
public func == (l: Int, r: Int) -> Bool { Bool() }
public func == (l: Int32, r: Int) -> Bool { Bool() }
public func < (l: Int, r: Int) -> Bool { Bool() }
public func <= (l: Int, r: Int) -> Bool { Bool() }
public func >= (l: Int, r: Int) -> Bool { Bool() }
public func > (l: Int, r: Int) -> Bool { Bool() }
public func ^ (l: UInt8, r: Int) -> UInt8 { l }
public func + (l: Int, r: Int) -> Int { l }
public func - (l: Int, r: Int) -> Int { l }
public func * (l: Int, r: Int) -> Int { l }
public func / (l: Int, r: Int) -> Int { l }
public func % (l: Int, r: Int) -> Int { l }
public func << (l: Int, r: Int) -> Int { l }
public func += (l: inout Int, r: Int) {}
public func >> (l: UInt8, r: Int) -> UInt8 { l }
public func & (l: UInt8, r: Int) -> UInt8 { l }
public func ^ (l: UInt8, r: UInt8) -> UInt8 { l }
public func &+= (l: inout UInt8, r: Int) {}
public func + (l: UnsafeRawPointer, r: Int) -> UnsafeRawPointer { l }
public func ..< (l: Int, r: Int) -> Range<Int> { Range() }
public func ... (l: Int, r: Int) -> ClosedRange<Int> { ClosedRange() }
public postfix func ... (l: Int) -> PartialRangeFrom<Int> { PartialRangeFrom() }
public func == <A: Equatable, B: Equatable>(l: (A, B), r: (A, B)) -> Bool { Bool() }
public func == <A: Equatable, B: Equatable, C: Equatable, D: Equatable>(l: (A, B, C, D), r: (A, B, C, D)) -> Bool { Bool() }
public func == (l: Array<UInt8>, r: Array<UInt8>) -> Bool { Bool() }
public func + (l: String, r: String) -> String { l }
public func + (l: Array<UInt8>, r: Array<UInt8>) -> Array<UInt8> { l }
public func += (l: inout Array<UInt8>, r: Array<UInt8>) {}
