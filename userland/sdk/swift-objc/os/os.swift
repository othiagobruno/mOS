// mOS — overlay Swift "os" da variante com interop ObjC (docs/swift-objc-runtime.md, fase 3), com os nomes do SDK
// da Apple: OSLog (= OS_os_log), os_log(_:dso:log:_:_:) e os tipos de OSLogType. Como no libSystem do mOS
// (darwin.c), só erro e falha são escritos (no stderr, que vai para o Console do mOS); info/debug/padrão somem.

@_exported import os
import Darwin

public typealias OSLog = OS_os_log

extension OSLogType {
    public static var `default`: OSLogType { OSLogType(rawValue: 0x00) }
    public static var info: OSLogType { OSLogType(rawValue: 0x01) }
    public static var debug: OSLogType { OSLogType(rawValue: 0x02) }
    public static var error: OSLogType { OSLogType(rawValue: 0x10) }
    public static var fault: OSLogType { OSLogType(rawValue: 0x11) }
}

extension OS_os_log {
    public static var `default`: OSLog { unsafeBitCast(_mos_os_log_default(), to: OSLog.self) }
    public static var disabled: OSLog { unsafeBitCast(_mos_os_log_disabled(), to: OSLog.self) }
}

/// OSLog(subsystem:category:) do SDK; aqui uma função, porque a classe só existe para o runtime
public func _mosOSLog(subsystem: String, category: String) -> OSLog { os_log_create(subsystem, category) }

// MARK: - OSLog.Category e OSLog(subsystem:category: Category)

extension OS_os_log {
    /// categorias especiais do SDK (o rawValue é o nome que o os_log_create recebe)
    public struct Category {
        public let rawValue: String
        public init(rawValue: String) { self.rawValue = rawValue }
        public static var pointsOfInterest: Category { Category(rawValue: "PointsOfInterest") }
        public static var dynamicTracing: Category { Category(rawValue: "DynamicTracing") }
        public static var dynamicStackTracing: Category { Category(rawValue: "DynamicStackTracing") }
    }

    /// OSLog(subsystem:category:) com uma Category. O OS_os_log é a classe do libSystem (sem init designado que o
    /// Swift enxergue), então o inicializador é este método com o nome do inicializador da Apple: a convenção é a
    /// mesma (argumentos, o metatipo em self, objeto +1 na volta).
    @_silgen_name("$sSo9OS_os_logC0B0E9subsystem8categoryABSS_AbCE8CategoryVtcfC")
    public static func _mosMake(subsystem: String, category: Category) -> OSLog {
        os_log_create(subsystem, category.rawValue)
    }
}

// MARK: - OSSignpostID

public struct OSSignpostID: Equatable, Comparable, Hashable {
    public let rawValue: UInt64
    public init(_ value: UInt64) { rawValue = value }
    /// um identificador novo e único no processo (o os_signpost_id_generate da Apple também é por processo)
    public init(log: OSLog) {
        var v: UInt64
        repeat { v = UInt64(bitPattern: Int64(_mosSignpostNext.add())) } while v == 0 || v == ~0
        rawValue = v
    }
    public init(log: OSLog, object: AnyObject) { rawValue = UInt64(UInt(bitPattern: ObjectIdentifier(object).hashValue)) }
    public static var exclusive: OSSignpostID { OSSignpostID(0xEEEE_B0B5_B2B2_EEEE) }
    public static var invalid: OSSignpostID { OSSignpostID(~0) }
    public static var null: OSSignpostID { OSSignpostID(0) }
    public static func < (a: OSSignpostID, b: OSSignpostID) -> Bool { a.rawValue < b.rawValue }
}

internal final class _MOSCounter {
    private var value: Int = 0
    private let mutex: UnsafeMutablePointer<pthread_mutex_t> = {
        let m = UnsafeMutablePointer<pthread_mutex_t>.allocate(capacity: 1)
        pthread_mutex_init(m, nil)
        return m
    }()
    func add() -> Int { pthread_mutex_lock(mutex); value += 1; let v = value; pthread_mutex_unlock(mutex); return v }
}
internal let _mosSignpostNext = _MOSCounter()

/// Formata no estilo do os_log: %{public}/%{private} somem; cada especificador consome um argumento,
/// impresso pela descrição Swift dele (cobre %d %s %@ %f %ld %lu %x...).
@usableFromInline
internal func _mosFormat(_ format: StaticString, _ args: [CVarArg]) -> String {
    var out = ""
    var it = args.makeIterator()
    let bytes = UnsafeBufferPointer(start: format.utf8Start, count: format.utf8CodeUnitCount)
    var i = 0
    var lit: [UInt8] = []
    func flush() { if !lit.isEmpty { out += String(decoding: lit, as: UTF8.self); lit.removeAll() } }
    while i < bytes.count {
        let c = bytes[i]
        if c != UInt8(ascii: "%") { lit.append(c); i += 1; continue }
        i += 1
        if i < bytes.count && bytes[i] == UInt8(ascii: "%") { lit.append(c); i += 1; continue }
        if i < bytes.count && bytes[i] == UInt8(ascii: "{") {
            while i < bytes.count && bytes[i] != UInt8(ascii: "}") { i += 1 }
            i += 1
        }
        // flags, largura, precisão, tamanho, conversão
        while i < bytes.count {
            let s = bytes[i]
            i += 1
            let isConv = (s >= UInt8(ascii: "a") && s <= UInt8(ascii: "z") && !"hlqjztL".utf8.contains(s)) || s == UInt8(ascii: "@") || s == UInt8(ascii: "X") || s == UInt8(ascii: "E") || s == UInt8(ascii: "G")
            if isConv { break }
        }
        flush()
        if let a = it.next() { out += "\(a)" }
    }
    flush()
    return out
}

public func os_log(_ type: OSLogType, dso: UnsafeRawPointer = #dsohandle, log: OSLog = .default,
                   _ message: StaticString, _ args: CVarArg...) {
    guard type == .error || type == .fault else { return }
    let text = _mosFormat(message, args)
    fputs(text + "\n", __stderrp)
}

public func os_log(_ message: StaticString, dso: UnsafeRawPointer? = #dsohandle, log: OSLog = .default,
                   type: OSLogType = .default, _ args: CVarArg...) {
    guard type == .error || type == .fault else { return }
    fputs(_mosFormat(message, args) + "\n", __stderrp)
}

// MARK: - Logger (o que o swift-foundation e os apps usam; como o MOSos do caminho sem ObjC: erro/falha no stderr)

public struct OSLogPrivacy: Sendable, Equatable {
    let _raw: Int
    public static var `public`: OSLogPrivacy { OSLogPrivacy(_raw: 0) }
    public static var `private`: OSLogPrivacy { OSLogPrivacy(_raw: 1) }
    public static var sensitive: OSLogPrivacy { OSLogPrivacy(_raw: 2) }
    public static var auto: OSLogPrivacy { OSLogPrivacy(_raw: 3) }
}

public struct OSLogInterpolation: StringInterpolationProtocol, Sendable {
    public var _text = ""
    public init(literalCapacity: Int, interpolationCount: Int) {}
    public mutating func appendLiteral(_ s: String) { _text += s }
    public mutating func appendInterpolation<T>(_ value: @autoclosure () -> T, privacy: OSLogPrivacy = .auto) { _text += String(describing: value()) }
    public mutating func appendInterpolation(_ value: @autoclosure () -> String, privacy: OSLogPrivacy = .auto) { _text += value() }
}

public struct OSLogMessage: ExpressibleByStringInterpolation, ExpressibleByStringLiteral, Sendable {
    public let _text: String
    public init(stringLiteral value: String) { _text = value }
    public init(stringInterpolation: OSLogInterpolation) { _text = stringInterpolation._text }
}

@usableFromInline internal func _mosLogLine(_ level: String, _ category: String, _ message: String) {
    var line = "[\(level)]"
    if !category.isEmpty { line += " \(category):" }
    fputs(line + " " + message + "\n", __stderrp)
}

public struct Logger: @unchecked Sendable {
    let _category: String
    public init(subsystem: String, category: String) { _category = category }
    public init(_ log: OSLog) { _category = "" }
    public init() { _category = "" }

    public func log(level: OSLogType, _ message: OSLogMessage) {
        if level == .error || level == .fault { _mosLogLine(level == .error ? "error" : "fault", _category, message._text) }
    }
    public func log(_ message: OSLogMessage) {}
    public func trace(_ message: OSLogMessage) {}
    public func debug(_ message: OSLogMessage) {}
    public func info(_ message: OSLogMessage) {}
    public func notice(_ message: OSLogMessage) {}
    public func warning(_ message: OSLogMessage) { _mosLogLine("warning", _category, message._text) }
    public func error(_ message: OSLogMessage) { _mosLogLine("error", _category, message._text) }
    public func critical(_ message: OSLogMessage) { _mosLogLine("critical", _category, message._text) }
    public func fault(_ message: OSLogMessage) { _mosLogLine("fault", _category, message._text) }
}

extension Logger {
    /// o OSLog por trás do Logger (o mOS não guarda subsistema/categoria num objeto de log: devolve o padrão)
    public var logObject: OSLog { .default }
}

// MARK: - OSAllocatedUnfairLock (macOS 13): o estado e o os_unfair_lock num ManagedBuffer, com o layout da Apple
// (@frozen, um campo de referência), porque os apps inlinam lock/unlock/withLock e só pedem o descritor do tipo

@_silgen_name("os_unfair_lock_lock") @usableFromInline internal func _mos_unfair_lock(_ l: UnsafeMutablePointer<UInt32>)
@_silgen_name("os_unfair_lock_unlock") @usableFromInline internal func _mos_unfair_unlock(_ l: UnsafeMutablePointer<UInt32>)
@_silgen_name("os_unfair_lock_trylock") @usableFromInline internal func _mos_unfair_trylock(_ l: UnsafeMutablePointer<UInt32>) -> Bool

@frozen
public struct OSAllocatedUnfairLock<State>: @unchecked Sendable {
    @usableFromInline let __lock: ManagedBuffer<State, UInt32>

    public init(uncheckedState initialState: State) {
        __lock = ManagedBuffer<State, UInt32>.create(minimumCapacity: 1) { b in
            b.withUnsafeMutablePointerToElements { $0.initialize(to: 0) }
            return initialState
        }
    }
    public init(initialState: State) where State: Sendable { self.init(uncheckedState: initialState) }

    @inlinable
    public func withLockUnchecked<R>(_ body: (inout State) throws -> R) rethrows -> R {
        try __lock.withUnsafeMutablePointers { st, l in
            _mos_unfair_lock(l)
            defer { _mos_unfair_unlock(l) }
            return try body(&st.pointee)
        }
    }
    @inlinable
    public func withLock<R: Sendable>(_ body: @Sendable (inout State) throws -> R) rethrows -> R { try withLockUnchecked(body) }
    @inlinable
    public func withLockIfAvailableUnchecked<R>(_ body: (inout State) throws -> R) rethrows -> R? {
        try __lock.withUnsafeMutablePointers { st, l in
            guard _mos_unfair_trylock(l) else { return nil }
            defer { _mos_unfair_unlock(l) }
            return try body(&st.pointee)
        }
    }
    @inlinable
    public func withLockIfAvailable<R: Sendable>(_ body: @Sendable (inout State) throws -> R) rethrows -> R? { try withLockIfAvailableUnchecked(body) }
    @inlinable public func lock() { __lock.withUnsafeMutablePointerToElements { _mos_unfair_lock($0) } }
    @inlinable public func unlock() { __lock.withUnsafeMutablePointerToElements { _mos_unfair_unlock($0) } }
    @inlinable public func lockIfAvailable() -> Bool { __lock.withUnsafeMutablePointerToElements { _mos_unfair_trylock($0) } }
}

extension OSAllocatedUnfairLock where State == () {
    public init() { self.init(uncheckedState: ()) }
    @inlinable
    public func withLockUnchecked<R>(_ body: () throws -> R) rethrows -> R { try withLockUnchecked { (_: inout ()) in try body() } }
    @inlinable
    public func withLock<R: Sendable>(_ body: @Sendable () throws -> R) rethrows -> R { try withLockUnchecked(body) }
}
