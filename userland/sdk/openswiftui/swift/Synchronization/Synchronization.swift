// mOS SDK — Synchronization: o subconjunto do módulo Synchronization do Swift 6 que o OpenSwiftUICore usa
// (Atomic<UInt32/Int32>.wrappingAdd, chamado através de withMemoryRebound sobre um ponteiro). O stdlib do mOS não traz
// o módulo; esta versão segue o desenho do original (@_rawLayout + Builtin.addressOfRawLayout, sempre na memória).
// Compilado com -parse-stdlib (Builtin) e o recurso experimental RawLayout.

import Swift

@frozen public struct AtomicUpdateOrdering: Sendable, Equatable {
    @usableFromInline let _raw: Int
    @inlinable init(_raw: Int) { self._raw = _raw }
    public static var relaxed: Self { Self(_raw: 0) }
    public static var acquiring: Self { Self(_raw: 1) }
    public static var releasing: Self { Self(_raw: 2) }
    public static var acquiringAndReleasing: Self { Self(_raw: 3) }
    public static var sequentiallyConsistent: Self { Self(_raw: 4) }
}

@frozen public struct AtomicLoadOrdering: Sendable, Equatable {
    @usableFromInline let _raw: Int
    @inlinable init(_raw: Int) { self._raw = _raw }
    public static var relaxed: Self { Self(_raw: 0) }
    public static var acquiring: Self { Self(_raw: 1) }
    public static var sequentiallyConsistent: Self { Self(_raw: 4) }
}

@frozen public struct AtomicStoreOrdering: Sendable, Equatable {
    @usableFromInline let _raw: Int
    @inlinable init(_raw: Int) { self._raw = _raw }
    public static var relaxed: Self { Self(_raw: 0) }
    public static var releasing: Self { Self(_raw: 2) }
    public static var sequentiallyConsistent: Self { Self(_raw: 4) }
}

/// Valor atômico com o layout do próprio `Value` (dá para reinterpretar memória existente, como o OpenSwiftUI faz).
/// Todas as operações são seq_cst no x86-64 (lock xadd / xchg), o que cobre qualquer ordering pedido.
@_rawLayout(like: Value, movesAsLike)
public struct Atomic<Value>: ~Copyable {
    public init(_ initialValue: consuming Value) {
        UnsafeMutablePointer<Value>(Builtin.addressOfRawLayout(self)).initialize(to: initialValue)
    }

    deinit { UnsafeMutablePointer<Value>(Builtin.addressOfRawLayout(self)).deinitialize(count: 1) }
}

extension Atomic: @unchecked Sendable where Value: Sendable {}

extension Atomic where Value == UInt32 {
    @inlinable public func load(ordering: AtomicLoadOrdering) -> UInt32 {
        UInt32(Builtin.atomicload_seqcst_Int32(Builtin.addressOfRawLayout(self)))
    }
    @inlinable public func store(_ desired: UInt32, ordering: AtomicStoreOrdering) {
        Builtin.atomicstore_seqcst_Int32(Builtin.addressOfRawLayout(self), desired._value)
    }
    @discardableResult
    @inlinable public func wrappingAdd(_ operand: UInt32, ordering: AtomicUpdateOrdering) -> (oldValue: UInt32, newValue: UInt32) {
        let old = UInt32(Builtin.atomicrmw_add_seqcst_Int32(Builtin.addressOfRawLayout(self), operand._value))
        return (old, old &+ operand)
    }
}

extension Atomic where Value == Int32 {
    @inlinable public func load(ordering: AtomicLoadOrdering) -> Int32 {
        Int32(Builtin.atomicload_seqcst_Int32(Builtin.addressOfRawLayout(self)))
    }
    @inlinable public func store(_ desired: Int32, ordering: AtomicStoreOrdering) {
        Builtin.atomicstore_seqcst_Int32(Builtin.addressOfRawLayout(self), desired._value)
    }
    @discardableResult
    @inlinable public func wrappingAdd(_ operand: Int32, ordering: AtomicUpdateOrdering) -> (oldValue: Int32, newValue: Int32) {
        let old = Int32(Builtin.atomicrmw_add_seqcst_Int32(Builtin.addressOfRawLayout(self), operand._value))
        return (old, old &+ operand)
    }
}

extension Atomic where Value == Int {
    @inlinable public func load(ordering: AtomicLoadOrdering) -> Int {
        Int(Builtin.atomicload_seqcst_Int64(Builtin.addressOfRawLayout(self)))
    }
    @inlinable public func store(_ desired: Int, ordering: AtomicStoreOrdering) {
        Builtin.atomicstore_seqcst_Int64(Builtin.addressOfRawLayout(self), desired._value)
    }
    @discardableResult
    @inlinable public func wrappingAdd(_ operand: Int, ordering: AtomicUpdateOrdering) -> (oldValue: Int, newValue: Int) {
        let old = Int(Builtin.atomicrmw_add_seqcst_Int64(Builtin.addressOfRawLayout(self), operand._value))
        return (old, old &+ operand)
    }
}
