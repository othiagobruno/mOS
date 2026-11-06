// mOS SDK — Synchronization: o subconjunto do módulo Synchronization do Swift 6 que o OpenSwiftUI e o swift-foundation
// usam: Atomic sobre inteiros (load/store/exchange/compareExchange/add/wrappingAdd...) e Mutex (withLock). O stdlib do
// mOS não traz o módulo; esta versão segue o desenho do original (@_rawLayout + Builtin.addressOfRawLayout: o valor vive
// sempre na memória, então dá para reinterpretar memória existente como Atomic, como o OpenSwiftUI faz).
// Compilado com -parse-stdlib (Builtin) e o recurso experimental RawLayout. x86-64: toda operação é seq_cst
// (lock xadd / lock cmpxchg / xchg), o que cobre qualquer ordering pedido.

import Swift

// MARK: - orderings

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

// MARK: - palavras atômicas (os inteiros que cabem num registrador)

/// Inteiro com operações atômicas sobre um endereço. As conformidades abaixo cobrem os inteiros de 32 e 64 bits.
public protocol _MOSAtomicWord: FixedWidthInteger, Sendable {
    static func _atomicLoad(_ p: UnsafeMutableRawPointer) -> Self
    static func _atomicStore(_ p: UnsafeMutableRawPointer, _ v: Self)
    static func _atomicExchange(_ p: UnsafeMutableRawPointer, _ v: Self) -> Self
    static func _atomicCompareExchange(_ p: UnsafeMutableRawPointer, _ expected: Self, _ desired: Self) -> (exchanged: Bool, original: Self)
    static func _atomicAdd(_ p: UnsafeMutableRawPointer, _ v: Self) -> Self   // devolve o valor antigo
}

extension UInt32: _MOSAtomicWord {
    @inlinable public static func _atomicLoad(_ p: UnsafeMutableRawPointer) -> UInt32 { UInt32(Builtin.atomicload_seqcst_Int32(p._rawValue)) }
    @inlinable public static func _atomicStore(_ p: UnsafeMutableRawPointer, _ v: UInt32) { Builtin.atomicstore_seqcst_Int32(p._rawValue, v._value) }
    @inlinable public static func _atomicExchange(_ p: UnsafeMutableRawPointer, _ v: UInt32) -> UInt32 { UInt32(Builtin.atomicrmw_xchg_seqcst_Int32(p._rawValue, v._value)) }
    @inlinable public static func _atomicCompareExchange(_ p: UnsafeMutableRawPointer, _ e: UInt32, _ d: UInt32) -> (exchanged: Bool, original: UInt32) {
        let (o, ok) = Builtin.cmpxchg_seqcst_seqcst_Int32(p._rawValue, e._value, d._value)
        return (Bool(ok), UInt32(o))
    }
    @inlinable public static func _atomicAdd(_ p: UnsafeMutableRawPointer, _ v: UInt32) -> UInt32 { UInt32(Builtin.atomicrmw_add_seqcst_Int32(p._rawValue, v._value)) }
}

extension Int32: _MOSAtomicWord {
    @inlinable public static func _atomicLoad(_ p: UnsafeMutableRawPointer) -> Int32 { Int32(Builtin.atomicload_seqcst_Int32(p._rawValue)) }
    @inlinable public static func _atomicStore(_ p: UnsafeMutableRawPointer, _ v: Int32) { Builtin.atomicstore_seqcst_Int32(p._rawValue, v._value) }
    @inlinable public static func _atomicExchange(_ p: UnsafeMutableRawPointer, _ v: Int32) -> Int32 { Int32(Builtin.atomicrmw_xchg_seqcst_Int32(p._rawValue, v._value)) }
    @inlinable public static func _atomicCompareExchange(_ p: UnsafeMutableRawPointer, _ e: Int32, _ d: Int32) -> (exchanged: Bool, original: Int32) {
        let (o, ok) = Builtin.cmpxchg_seqcst_seqcst_Int32(p._rawValue, e._value, d._value)
        return (Bool(ok), Int32(o))
    }
    @inlinable public static func _atomicAdd(_ p: UnsafeMutableRawPointer, _ v: Int32) -> Int32 { Int32(Builtin.atomicrmw_add_seqcst_Int32(p._rawValue, v._value)) }
}

extension UInt64: _MOSAtomicWord {
    @inlinable public static func _atomicLoad(_ p: UnsafeMutableRawPointer) -> UInt64 { UInt64(Builtin.atomicload_seqcst_Int64(p._rawValue)) }
    @inlinable public static func _atomicStore(_ p: UnsafeMutableRawPointer, _ v: UInt64) { Builtin.atomicstore_seqcst_Int64(p._rawValue, v._value) }
    @inlinable public static func _atomicExchange(_ p: UnsafeMutableRawPointer, _ v: UInt64) -> UInt64 { UInt64(Builtin.atomicrmw_xchg_seqcst_Int64(p._rawValue, v._value)) }
    @inlinable public static func _atomicCompareExchange(_ p: UnsafeMutableRawPointer, _ e: UInt64, _ d: UInt64) -> (exchanged: Bool, original: UInt64) {
        let (o, ok) = Builtin.cmpxchg_seqcst_seqcst_Int64(p._rawValue, e._value, d._value)
        return (Bool(ok), UInt64(o))
    }
    @inlinable public static func _atomicAdd(_ p: UnsafeMutableRawPointer, _ v: UInt64) -> UInt64 { UInt64(Builtin.atomicrmw_add_seqcst_Int64(p._rawValue, v._value)) }
}

extension Int64: _MOSAtomicWord {
    @inlinable public static func _atomicLoad(_ p: UnsafeMutableRawPointer) -> Int64 { Int64(Builtin.atomicload_seqcst_Int64(p._rawValue)) }
    @inlinable public static func _atomicStore(_ p: UnsafeMutableRawPointer, _ v: Int64) { Builtin.atomicstore_seqcst_Int64(p._rawValue, v._value) }
    @inlinable public static func _atomicExchange(_ p: UnsafeMutableRawPointer, _ v: Int64) -> Int64 { Int64(Builtin.atomicrmw_xchg_seqcst_Int64(p._rawValue, v._value)) }
    @inlinable public static func _atomicCompareExchange(_ p: UnsafeMutableRawPointer, _ e: Int64, _ d: Int64) -> (exchanged: Bool, original: Int64) {
        let (o, ok) = Builtin.cmpxchg_seqcst_seqcst_Int64(p._rawValue, e._value, d._value)
        return (Bool(ok), Int64(o))
    }
    @inlinable public static func _atomicAdd(_ p: UnsafeMutableRawPointer, _ v: Int64) -> Int64 { Int64(Builtin.atomicrmw_add_seqcst_Int64(p._rawValue, v._value)) }
}

extension Int: _MOSAtomicWord {
    @inlinable public static func _atomicLoad(_ p: UnsafeMutableRawPointer) -> Int { Int(Int64._atomicLoad(p)) }
    @inlinable public static func _atomicStore(_ p: UnsafeMutableRawPointer, _ v: Int) { Int64._atomicStore(p, Int64(v)) }
    @inlinable public static func _atomicExchange(_ p: UnsafeMutableRawPointer, _ v: Int) -> Int { Int(Int64._atomicExchange(p, Int64(v))) }
    @inlinable public static func _atomicCompareExchange(_ p: UnsafeMutableRawPointer, _ e: Int, _ d: Int) -> (exchanged: Bool, original: Int) {
        let r = Int64._atomicCompareExchange(p, Int64(e), Int64(d)); return (r.exchanged, Int(r.original))
    }
    @inlinable public static func _atomicAdd(_ p: UnsafeMutableRawPointer, _ v: Int) -> Int { Int(Int64._atomicAdd(p, Int64(v))) }
}

extension UInt: _MOSAtomicWord {
    @inlinable public static func _atomicLoad(_ p: UnsafeMutableRawPointer) -> UInt { UInt(UInt64._atomicLoad(p)) }
    @inlinable public static func _atomicStore(_ p: UnsafeMutableRawPointer, _ v: UInt) { UInt64._atomicStore(p, UInt64(v)) }
    @inlinable public static func _atomicExchange(_ p: UnsafeMutableRawPointer, _ v: UInt) -> UInt { UInt(UInt64._atomicExchange(p, UInt64(v))) }
    @inlinable public static func _atomicCompareExchange(_ p: UnsafeMutableRawPointer, _ e: UInt, _ d: UInt) -> (exchanged: Bool, original: UInt) {
        let r = UInt64._atomicCompareExchange(p, UInt64(e), UInt64(d)); return (r.exchanged, UInt(r.original))
    }
    @inlinable public static func _atomicAdd(_ p: UnsafeMutableRawPointer, _ v: UInt) -> UInt { UInt(UInt64._atomicAdd(p, UInt64(v))) }
}

// MARK: - Atomic

/// Valor atômico com o layout do próprio `Value`.
@frozen
@_rawLayout(like: Value, movesAsLike)
public struct Atomic<Value>: ~Copyable {
    @inlinable public var _address: UnsafeMutableRawPointer {
        @_transparent get { UnsafeMutableRawPointer(Builtin.addressOfRawLayout(self)) }
    }

    @inlinable public init(_ initialValue: consuming Value) {
        UnsafeMutablePointer<Value>(Builtin.addressOfRawLayout(self)).initialize(to: initialValue)
    }

    @inlinable deinit { UnsafeMutablePointer<Value>(Builtin.addressOfRawLayout(self)).deinitialize(count: 1) }
}

extension Atomic: @unchecked Sendable where Value: Sendable {}

extension Atomic where Value: _MOSAtomicWord {
    @inlinable public func load(ordering: AtomicLoadOrdering) -> Value { Value._atomicLoad(_address) }
    @inlinable public func store(_ desired: Value, ordering: AtomicStoreOrdering) { Value._atomicStore(_address, desired) }
    @inlinable public func exchange(_ desired: Value, ordering: AtomicUpdateOrdering) -> Value { Value._atomicExchange(_address, desired) }

    @inlinable public func compareExchange(expected: Value, desired: Value, ordering: AtomicUpdateOrdering) -> (exchanged: Bool, original: Value) {
        Value._atomicCompareExchange(_address, expected, desired)
    }
    @inlinable public func compareExchange(expected: Value, desired: Value, successOrdering: AtomicUpdateOrdering,
                                           failureOrdering: AtomicLoadOrdering) -> (exchanged: Bool, original: Value) {
        Value._atomicCompareExchange(_address, expected, desired)
    }
    @inlinable public func weakCompareExchange(expected: Value, desired: Value, ordering: AtomicUpdateOrdering) -> (exchanged: Bool, original: Value) {
        Value._atomicCompareExchange(_address, expected, desired)
    }

    @discardableResult
    @inlinable public func wrappingAdd(_ operand: Value, ordering: AtomicUpdateOrdering) -> (oldValue: Value, newValue: Value) {
        let old = Value._atomicAdd(_address, operand)
        return (old, old &+ operand)
    }
    @discardableResult
    @inlinable public func wrappingSubtract(_ operand: Value, ordering: AtomicUpdateOrdering) -> (oldValue: Value, newValue: Value) {
        let old = Value._atomicAdd(_address, 0 &- operand)
        return (old, old &- operand)
    }
    /// Soma com verificação de estouro (como no original: estouro é erro fatal).
    @discardableResult
    @inlinable public func add(_ operand: Value, ordering: AtomicUpdateOrdering) -> (oldValue: Value, newValue: Value) {
        let (old, new) = wrappingAdd(operand, ordering: ordering)
        precondition(operand >= 0 ? new >= old : new < old, "estouro em Atomic.add")
        return (old, new)
    }
    @discardableResult
    @inlinable public func subtract(_ operand: Value, ordering: AtomicUpdateOrdering) -> (oldValue: Value, newValue: Value) {
        let (old, new) = wrappingSubtract(operand, ordering: ordering)
        precondition(operand >= 0 ? new <= old : new > old, "estouro em Atomic.subtract")
        return (old, new)
    }
}

// MARK: - Mutex

/// Trava de exclusão mútua com o estado protegido junto (API do Synchronization). Por baixo, uma trava de espera ativa
/// com cessão da CPU (sched_yield): o mOS não tem futex, e as seções críticas do Foundation são curtas.
@frozen
public struct Mutex<Value: ~Copyable>: ~Copyable {
    @usableFromInline let _lock: Atomic<UInt32>
    @usableFromInline let _value: _MutexCell<Value>

    @inlinable public init(_ initialValue: consuming sending Value) {
        _lock = Atomic<UInt32>(0)
        _value = _MutexCell(initialValue)
    }
}

extension Mutex: @unchecked Sendable where Value: ~Copyable {}

@_silgen_name("sched_yield") @usableFromInline func _mos_sched_yield() -> Int32

extension Mutex where Value: ~Copyable {
    @inlinable public func _acquire() {
        var spins = 0
        while !_lock.compareExchange(expected: 0, desired: 1, ordering: .acquiring).exchanged {
            spins += 1
            if spins > 64 { _ = _mos_sched_yield(); spins = 0 }
        }
    }
    @inlinable public func _tryAcquire() -> Bool { _lock.compareExchange(expected: 0, desired: 1, ordering: .acquiring).exchanged }
    @inlinable public func _release() { _lock.store(0, ordering: .releasing) }
    /// Travar/destravar sem escopo (SPI do Synchronization que o swift-foundation usa).
    @inlinable public borrowing func _unsafeLock() { _acquire() }
    @inlinable public borrowing func _unsafeTryLock() -> Bool { _tryAcquire() }
    @inlinable public borrowing func _unsafeUnlock() { _release() }

    @inlinable
    public borrowing func withLock<Result: ~Copyable, E: Error>(_ body: (inout sending Value) throws(E) -> sending Result) throws(E) -> sending Result {
        _acquire()
        defer { _release() }
        return try body(&_value._address.pointee)
    }

    @inlinable
    public borrowing func withLockIfAvailable<Result: ~Copyable, E: Error>(_ body: (inout sending Value) throws(E) -> sending Result) throws(E) -> sending Result? {
        guard _tryAcquire() else { return nil }
        defer { _release() }
        return try body(&_value._address.pointee)
    }
}

/// Célula com endereço estável para o estado do Mutex.
@frozen
@_rawLayout(like: Value, movesAsLike)
public struct _MutexCell<Value: ~Copyable>: ~Copyable {
    @inlinable public var _address: UnsafeMutablePointer<Value> {
        @_transparent get { UnsafeMutablePointer<Value>(Builtin.addressOfRawLayout(self)) }
    }
    @inlinable public init(_ value: consuming Value) {
        UnsafeMutablePointer<Value>(Builtin.addressOfRawLayout(self)).initialize(to: value)
    }
    @inlinable deinit { UnsafeMutablePointer<Value>(Builtin.addressOfRawLayout(self)).deinitialize(count: 1) }
}
