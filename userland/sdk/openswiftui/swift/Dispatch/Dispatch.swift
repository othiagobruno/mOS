// mOS SDK — Dispatch (API Swift da Apple) sobre o libdispatch em C do libSystem do mOS (dispatch.c).
// Sem blocos ObjC: usa as variantes *_f (função C + contexto), com a closure num contexto retido.

// MARK: - funções C do libSystem

public typealias dispatch_function_t = @convention(c) (UnsafeMutableRawPointer?) -> Void
@_silgen_name("dispatch_get_main_queue") func _dq_main() -> OpaquePointer
@_silgen_name("dispatch_get_global_queue") func _dq_global(_ identifier: Int, _ flags: UInt) -> OpaquePointer
@_silgen_name("dispatch_queue_create") func _dq_create(_ label: UnsafePointer<CChar>?, _ attr: OpaquePointer?) -> OpaquePointer
@_silgen_name("dispatch_async_f") func _dq_async(_ q: OpaquePointer, _ ctx: UnsafeMutableRawPointer?, _ f: dispatch_function_t)
@_silgen_name("dispatch_sync_f") func _dq_sync(_ q: OpaquePointer, _ ctx: UnsafeMutableRawPointer?, _ f: dispatch_function_t)
@_silgen_name("dispatch_barrier_async_f") func _dq_barrier_async(_ q: OpaquePointer, _ ctx: UnsafeMutableRawPointer?, _ f: dispatch_function_t)
@_silgen_name("dispatch_after_f") func _dq_after(_ when: UInt64, _ q: OpaquePointer, _ ctx: UnsafeMutableRawPointer?, _ f: dispatch_function_t)
@_silgen_name("dispatch_time") func _d_time(_ when: UInt64, _ delta: Int64) -> UInt64
@_silgen_name("dispatch_suspend") func _d_suspend(_ o: OpaquePointer)
@_silgen_name("dispatch_resume") func _d_resume(_ o: OpaquePointer)
@_silgen_name("dispatch_group_create") func _dg_create() -> OpaquePointer
@_silgen_name("dispatch_group_enter") func _dg_enter(_ g: OpaquePointer)
@_silgen_name("dispatch_group_leave") func _dg_leave(_ g: OpaquePointer)
@_silgen_name("dispatch_group_wait") func _dg_wait(_ g: OpaquePointer, _ t: UInt64) -> Int
@_silgen_name("dispatch_group_notify_f") func _dg_notify(_ g: OpaquePointer, _ q: OpaquePointer, _ ctx: UnsafeMutableRawPointer?, _ f: dispatch_function_t)
@_silgen_name("dispatch_group_async_f") func _dg_async(_ g: OpaquePointer, _ q: OpaquePointer, _ ctx: UnsafeMutableRawPointer?, _ f: dispatch_function_t)
@_silgen_name("dispatch_semaphore_create") func _ds_create(_ v: Int) -> OpaquePointer
@_silgen_name("dispatch_semaphore_wait") func _ds_wait(_ s: OpaquePointer, _ t: UInt64) -> Int
@_silgen_name("dispatch_semaphore_signal") func _ds_signal(_ s: OpaquePointer) -> Int
@_silgen_name("dispatch_once_f") func _d_once(_ pred: UnsafeMutablePointer<Int>, _ ctx: UnsafeMutableRawPointer?, _ f: dispatch_function_t)
@_silgen_name("dispatch_get_specific") func _d_get_specific(_ key: UnsafeRawPointer) -> UnsafeMutableRawPointer?
@_silgen_name("dispatch_queue_set_specific") func _dq_set_specific(_ q: OpaquePointer, _ key: UnsafeRawPointer, _ ctx: UnsafeMutableRawPointer?, _ d: dispatch_function_t?)

// MARK: - contexto: a closure vai retida e é liberada depois de rodar

final class _Work {
    let fn: () -> Void
    init(_ fn: @escaping () -> Void) { self.fn = fn }
}
func _ctx(_ fn: @escaping () -> Void) -> UnsafeMutableRawPointer { Unmanaged.passRetained(_Work(fn)).toOpaque() }
let _trampoline: dispatch_function_t = { p in Unmanaged<_Work>.fromOpaque(p!).takeRetainedValue().fn() }

// MARK: - tempo

public enum DispatchTimeInterval: Equatable, Sendable {
    case seconds(Int), milliseconds(Int), microseconds(Int), nanoseconds(Int), never
    var nanos: Int64 {
        switch self {
        case .seconds(let s): return Int64(s) * 1_000_000_000
        case .milliseconds(let s): return Int64(s) * 1_000_000
        case .microseconds(let s): return Int64(s) * 1_000
        case .nanoseconds(let s): return Int64(s)
        case .never: return Int64.max
        }
    }
}
public struct DispatchTime: Comparable, Sendable {
    public let rawValue: UInt64
    init(raw: UInt64) { rawValue = raw }
    public init(uptimeNanoseconds: UInt64) { rawValue = uptimeNanoseconds }
    public static func now() -> DispatchTime { DispatchTime(raw: _d_time(0, 0)) }
    public static let distantFuture = DispatchTime(raw: ~0)
    public var uptimeNanoseconds: UInt64 { rawValue }
    public static func < (a: DispatchTime, b: DispatchTime) -> Bool { a.rawValue < b.rawValue }
    public static func + (t: DispatchTime, i: DispatchTimeInterval) -> DispatchTime { i == .never ? .distantFuture : DispatchTime(raw: _d_time(t.rawValue, i.nanos)) }
    public static func + (t: DispatchTime, s: Double) -> DispatchTime { DispatchTime(raw: _d_time(t.rawValue, Int64(s * 1e9))) }
    public static func - (t: DispatchTime, i: DispatchTimeInterval) -> DispatchTime { DispatchTime(raw: _d_time(t.rawValue, -i.nanos)) }
    public static func - (t: DispatchTime, s: Double) -> DispatchTime { DispatchTime(raw: _d_time(t.rawValue, -Int64(s * 1e9))) }
    public func distance(to other: DispatchTime) -> DispatchTimeInterval { .nanoseconds(Int(Int64(bitPattern: other.rawValue &- rawValue))) }
}
public struct DispatchWallTime: Comparable, Sendable {
    public let rawValue: UInt64
    public static func now() -> DispatchWallTime { DispatchWallTime(rawValue: _d_time(0, 0)) }
    public static let distantFuture = DispatchWallTime(rawValue: ~0)
    public static func < (a: DispatchWallTime, b: DispatchWallTime) -> Bool { a.rawValue < b.rawValue }
    public static func + (t: DispatchWallTime, i: DispatchTimeInterval) -> DispatchWallTime { DispatchWallTime(rawValue: _d_time(t.rawValue, i.nanos)) }
    public static func + (t: DispatchWallTime, s: Double) -> DispatchWallTime { DispatchWallTime(rawValue: _d_time(t.rawValue, Int64(s * 1e9))) }
}
public enum DispatchTimeoutResult: Sendable { case success, timedOut }

// MARK: - QoS e opções

public struct DispatchQoS: Equatable, Sendable {
    public enum QoSClass: Sendable { case background, utility, `default`, userInitiated, userInteractive, unspecified }
    public let qosClass: QoSClass
    public let relativePriority: Int
    public init(qosClass: QoSClass, relativePriority: Int) { self.qosClass = qosClass; self.relativePriority = relativePriority }
    public static let background = DispatchQoS(qosClass: .background, relativePriority: 0)
    public static let utility = DispatchQoS(qosClass: .utility, relativePriority: 0)
    public static let `default` = DispatchQoS(qosClass: .default, relativePriority: 0)
    public static let userInitiated = DispatchQoS(qosClass: .userInitiated, relativePriority: 0)
    public static let userInteractive = DispatchQoS(qosClass: .userInteractive, relativePriority: 0)
    public static let unspecified = DispatchQoS(qosClass: .unspecified, relativePriority: 0)
}
public struct DispatchWorkItemFlags: OptionSet, Sendable {
    public let rawValue: UInt
    public init(rawValue: UInt) { self.rawValue = rawValue }
    public static let barrier = DispatchWorkItemFlags(rawValue: 1), detached = DispatchWorkItemFlags(rawValue: 2),
                      assignCurrentContext = DispatchWorkItemFlags(rawValue: 4), noQoS = DispatchWorkItemFlags(rawValue: 8),
                      inheritQoS = DispatchWorkItemFlags(rawValue: 16), enforceQoS = DispatchWorkItemFlags(rawValue: 32)
}

// MARK: - objetos

open class DispatchObject: @unchecked Sendable {
    let obj: OpaquePointer
    init(_ o: OpaquePointer) { obj = o }
    public func suspend() { _d_suspend(obj) }
    public func resume() { _d_resume(obj) }
    public func activate() { _d_resume(obj) }
}

public final class DispatchWorkItem: @unchecked Sendable {
    let block: () -> Void
    public private(set) var isCancelled = false
    var notifications: [(DispatchQueue, () -> Void)] = []
    public init(qos: DispatchQoS = .unspecified, flags: DispatchWorkItemFlags = [], block: @escaping () -> Void) { self.block = block }
    public func perform() {
        if isCancelled { return }
        block()
        for (q, f) in notifications { q.async(execute: f) }
    }
    public func cancel() { isCancelled = true }
    public func notify(queue: DispatchQueue, execute: @escaping () -> Void) { notifications.append((queue, execute)) }
    public func wait() {}
}

public final class DispatchQueue: DispatchObject, @unchecked Sendable {
    public struct Attributes: OptionSet, Sendable {
        public let rawValue: UInt
        public init(rawValue: UInt) { self.rawValue = rawValue }
        public static let concurrent = Attributes(rawValue: 1), initiallyInactive = Attributes(rawValue: 2)
    }
    public enum AutoreleaseFrequency: Sendable { case inherit, workItem, never }
    public let label: String
    nonisolated(unsafe) static var _main: DispatchQueue? = nil
    nonisolated(unsafe) static var _globals: [Int: DispatchQueue] = [:]
    init(obj: OpaquePointer, label: String) { self.label = label; super.init(obj) }
    public convenience init(label: String, qos: DispatchQoS = .unspecified, attributes: Attributes = [],
                            autoreleaseFrequency: AutoreleaseFrequency = .inherit, target: DispatchQueue? = nil) {
        let q = label.withCString { _dq_create($0, attributes.contains(.concurrent) ? OpaquePointer(bitPattern: 1) : nil) }
        self.init(obj: q, label: label)
        if attributes.contains(.initiallyInactive) { _d_suspend(q) }
    }
    public static var main: DispatchQueue {
        if let m = _main { return m }
        let m = DispatchQueue(obj: _dq_main(), label: "com.apple.main-thread")
        _main = m
        return m
    }
    public static func global(qos: DispatchQoS.QoSClass = .default) -> DispatchQueue {
        let id: Int
        switch qos { case .userInteractive: id = 0x21; case .userInitiated: id = 0x19; case .utility: id = 0x11; case .background: id = 0x09; default: id = 0 }
        if let q = _globals[id] { return q }
        let q = DispatchQueue(obj: _dq_global(id, 0), label: "com.apple.root.global")
        _globals[id] = q
        return q
    }
    public func async(group: DispatchGroup? = nil, qos: DispatchQoS = .unspecified, flags: DispatchWorkItemFlags = [], execute work: @escaping () -> Void) {
        if let g = group { _dg_async(g.obj, obj, _ctx(work), _trampoline); return }
        if flags.contains(.barrier) { _dq_barrier_async(obj, _ctx(work), _trampoline) } else { _dq_async(obj, _ctx(work), _trampoline) }
    }
    public func async(execute item: DispatchWorkItem) { async { item.perform() } }
    public func sync<T>(execute work: () throws -> T) rethrows -> T {
        // sync: a closure não escapa; roda dentro da fila via um contexto temporário
        return try withoutActuallyEscaping(work) { w in
            var result: Result<T, Error>? = nil
            let box = _Work { result = Result { try w() } }
            _dq_sync(obj, Unmanaged.passUnretained(box).toOpaque(), { p in Unmanaged<_Work>.fromOpaque(p!).takeUnretainedValue().fn() })
            return try result!.get()
        }
    }
    public func sync(flags: DispatchWorkItemFlags, execute work: () throws -> Void) rethrows { try sync(execute: work) }
    public func sync(execute item: DispatchWorkItem) { sync { item.perform() } }
    public func asyncAfter(deadline: DispatchTime, qos: DispatchQoS = .unspecified, flags: DispatchWorkItemFlags = [], execute work: @escaping () -> Void) {
        _dq_after(deadline.rawValue, obj, _ctx(work), _trampoline)
    }
    public func asyncAfter(deadline: DispatchTime, execute item: DispatchWorkItem) { asyncAfter(deadline: deadline) { item.perform() } }
    public func asyncAfter(wallDeadline: DispatchWallTime, qos: DispatchQoS = .unspecified, flags: DispatchWorkItemFlags = [], execute work: @escaping () -> Void) {
        _dq_after(wallDeadline.rawValue, obj, _ctx(work), _trampoline)
    }
    public static func concurrentPerform(iterations: Int, execute work: (Int) -> Void) { for i in 0..<iterations { work(i) } }
}

public final class DispatchGroup: DispatchObject, @unchecked Sendable {
    public init() { super.init(_dg_create()) }
    public func enter() { _dg_enter(obj) }
    public func leave() { _dg_leave(obj) }
    public func wait() { _ = _dg_wait(obj, ~0) }
    public func wait(timeout: DispatchTime) -> DispatchTimeoutResult { _dg_wait(obj, timeout.rawValue) == 0 ? .success : .timedOut }
    public func wait(wallTimeout: DispatchWallTime) -> DispatchTimeoutResult { _dg_wait(obj, wallTimeout.rawValue) == 0 ? .success : .timedOut }
    public func notify(qos: DispatchQoS = .unspecified, flags: DispatchWorkItemFlags = [], queue: DispatchQueue, execute work: @escaping () -> Void) {
        _dg_notify(obj, queue.obj, _ctx(work), _trampoline)
    }
    public func notify(queue: DispatchQueue, work: DispatchWorkItem) { notify(queue: queue) { work.perform() } }
}

public final class DispatchSemaphore: DispatchObject, @unchecked Sendable {
    public init(value: Int) { super.init(_ds_create(value)) }
    public func wait() { _ = _ds_wait(obj, ~0) }
    public func wait(timeout: DispatchTime) -> DispatchTimeoutResult { _ds_wait(obj, timeout.rawValue) == 0 ? .success : .timedOut }
    public func wait(wallTimeout: DispatchWallTime) -> DispatchTimeoutResult { _ds_wait(obj, wallTimeout.rawValue) == 0 ? .success : .timedOut }
    @discardableResult public func signal() -> Int { _ds_signal(obj) }
}

public enum DispatchPredicate { case onQueue(DispatchQueue), onQueueAsBarrier(DispatchQueue), notOnQueue(DispatchQueue) }
public func dispatchPrecondition(condition: @autoclosure () -> DispatchPredicate) {}
