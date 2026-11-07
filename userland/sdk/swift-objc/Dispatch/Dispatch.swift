// mOS — overlay Swift "Dispatch" da variante com interop ObjC (docs/swift-objc-runtime.md, fase 3), escrito para o mOS
// com a API, as assinaturas e os layouts do overlay do SDK da Apple, para binários Swift da Apple resolverem contra ele.
// Os tipos de objeto vêm dos cabeçalhos do macOS (dispatch/*.h do zig, modo OS_OBJECT do Swift): DispatchQueue é a
// classe OS_dispatch_queue, DispatchGroup OS_dispatch_group, DispatchSourceProtocol o protocolo OS_dispatch_source...
// As funções são as do libdispatch em C do libSystem do mOS (dispatch.c, extra.c), chamadas por @_silgen_name.
// Os objetos do dispatch do mOS (filas, grupos, fontes, semáforos) têm o isa da classe OS_dispatch_* no 1º word, como
// no macOS (o Swift lê o tipo dinâmico pelo isa ao abrir existenciais); retain/release da libobjc são inócuos neles.

@_exported import Dispatch
import Darwin

// MARK: - funções C do libSystem

@_silgen_name("dispatch_get_main_queue") internal func _mos_main_queue() -> DispatchQueue
@_silgen_name("dispatch_get_global_queue") internal func _mos_global_queue(_ identifier: Int, _ flags: UInt) -> DispatchQueue
@_silgen_name("dispatch_queue_create") internal func _mos_queue_create(_ label: UnsafePointer<CChar>?, _ attr: OpaquePointer?) -> DispatchQueue
@_silgen_name("dispatch_set_target_queue") internal func _mos_set_target(_ o: DispatchObject, _ q: DispatchQueue?)
@_silgen_name("dispatch_queue_get_label") internal func _mos_queue_label(_ q: DispatchQueue?) -> UnsafePointer<CChar>
@_silgen_name("dispatch_async") internal func _mos_async(_ q: DispatchQueue, _ block: @escaping @convention(block) () -> Void)
@_silgen_name("dispatch_barrier_async") internal func _mos_barrier_async(_ q: DispatchQueue, _ block: @escaping @convention(block) () -> Void)
@_silgen_name("dispatch_sync") internal func _mos_sync(_ q: DispatchQueue, _ block: @convention(block) () -> Void)
@_silgen_name("dispatch_barrier_sync") internal func _mos_barrier_sync(_ q: DispatchQueue, _ block: @convention(block) () -> Void)
@_silgen_name("dispatch_sync_f") internal func _mos_sync_f(_ q: DispatchQueue, _ ctx: UnsafeMutableRawPointer?, _ f: @convention(c) (UnsafeMutableRawPointer?) -> Void)
@_silgen_name("dispatch_barrier_sync_f") internal func _mos_barrier_sync_f(_ q: DispatchQueue, _ ctx: UnsafeMutableRawPointer?, _ f: @convention(c) (UnsafeMutableRawPointer?) -> Void)
internal let _mos_run_closure: @convention(c) (UnsafeMutableRawPointer?) -> Void = { ctx in
    ctx!.assumingMemoryBound(to: (() -> Void).self).pointee()
}
@_silgen_name("dispatch_after") internal func _mos_after(_ when: UInt64, _ q: DispatchQueue, _ block: @escaping @convention(block) () -> Void)
@_silgen_name("dispatch_group_async") internal func _mos_group_async(_ g: DispatchGroup, _ q: DispatchQueue, _ block: @escaping @convention(block) () -> Void)
@_silgen_name("dispatch_group_notify") internal func _mos_group_notify(_ g: DispatchGroup, _ q: DispatchQueue, _ block: @escaping @convention(block) () -> Void)
@_silgen_name("dispatch_group_create") internal func _mos_group_create() -> DispatchGroup
@_silgen_name("dispatch_group_enter") internal func _mos_group_enter(_ g: DispatchGroup)
@_silgen_name("dispatch_group_leave") internal func _mos_group_leave(_ g: DispatchGroup)
@_silgen_name("dispatch_group_wait") internal func _mos_group_wait(_ g: DispatchGroup, _ t: UInt64) -> Int
@_silgen_name("dispatch_semaphore_create") internal func _mos_sema_create(_ v: Int) -> DispatchSemaphore
@_silgen_name("dispatch_semaphore_wait") internal func _mos_sema_wait(_ s: DispatchSemaphore, _ t: UInt64) -> Int
@_silgen_name("dispatch_semaphore_signal") internal func _mos_sema_signal(_ s: DispatchSemaphore) -> Int
@_silgen_name("dispatch_time") internal func _mos_time(_ when: UInt64, _ delta: Int64) -> UInt64
@_silgen_name("dispatch_suspend") internal func _mos_suspend(_ o: DispatchObject)
@_silgen_name("dispatch_resume") internal func _mos_resume(_ o: DispatchObject)
@_silgen_name("dispatch_activate") internal func _mos_activate(_ o: DispatchObject)
@_silgen_name("dispatch_source_create") internal func _mos_source_create(_ type: UnsafeRawPointer, _ handle: UInt, _ mask: UInt, _ q: DispatchQueue?) -> DispatchSource
@_silgen_name("dispatch_source_set_event_handler") internal func _mos_source_set_event(_ s: DispatchSource, _ h: (@convention(block) () -> Void)?)
@_silgen_name("dispatch_source_set_cancel_handler") internal func _mos_source_set_cancel(_ s: DispatchSource, _ h: (@convention(block) () -> Void)?)
@_silgen_name("dispatch_source_set_registration_handler") internal func _mos_source_set_registration(_ s: DispatchSource, _ h: (@convention(block) () -> Void)?)
@_silgen_name("dispatch_source_cancel") internal func _mos_source_cancel(_ s: DispatchSource)
@_silgen_name("dispatch_source_testcancel") internal func _mos_source_testcancel(_ s: DispatchSource) -> Int
@_silgen_name("dispatch_source_get_handle") internal func _mos_source_handle(_ s: DispatchSource) -> UInt
@_silgen_name("dispatch_source_get_mask") internal func _mos_source_mask(_ s: DispatchSource) -> UInt
@_silgen_name("dispatch_source_get_data") internal func _mos_source_data(_ s: DispatchSource) -> UInt
@_silgen_name("dispatch_source_merge_data") internal func _mos_source_merge(_ s: DispatchSource, _ v: UInt)
@_silgen_name("dispatch_source_set_timer") internal func _mos_source_set_timer(_ s: DispatchSource, _ start: UInt64, _ interval: UInt64, _ leeway: UInt64)

internal let _mos_concurrent_attr = UnsafeMutableRawPointer.allocate(byteCount: 16, alignment: 16)

/// os tipos de fonte são símbolos de dados do libSystem (&_dispatch_source_type_read...)
internal func _mos_source_type(_ name: String) -> UnsafeRawPointer {
    guard let p = dlsym(UnsafeMutableRawPointer(bitPattern: -2), name) else { fatalError("Dispatch: \(name) ausente no libSystem") }
    return UnsafeRawPointer(p)
}

// os objetos do dispatch do mOS ainda não são objetos ObjC: as conversões para os protocolos de fonte são de ponteiro
@inline(__always) internal func _mos_as<T>(_ s: DispatchSource, _: T.Type) -> T { unsafeBitCast(s, to: T.self) }
@inline(__always) internal func _mos_src(_ p: DispatchSourceProtocol) -> DispatchSource { unsafeBitCast(p, to: DispatchSource.self) }

// MARK: - QoS

public struct DispatchQoS: Equatable {
    public let qosClass: QoSClass
    public let relativePriority: Int

    public static var background: DispatchQoS { DispatchQoS(qosClass: .background, relativePriority: 0) }
    public static var utility: DispatchQoS { DispatchQoS(qosClass: .utility, relativePriority: 0) }
    public static var `default`: DispatchQoS { DispatchQoS(qosClass: .default, relativePriority: 0) }
    public static var userInitiated: DispatchQoS { DispatchQoS(qosClass: .userInitiated, relativePriority: 0) }
    public static var userInteractive: DispatchQoS { DispatchQoS(qosClass: .userInteractive, relativePriority: 0) }
    public static var unspecified: DispatchQoS { DispatchQoS(qosClass: .unspecified, relativePriority: 0) }

    public enum QoSClass {
        case background, utility, `default`, userInitiated, userInteractive, unspecified

        public init?(rawValue: qos_class_t) {
            switch rawValue {
            case QOS_CLASS_BACKGROUND: self = .background
            case QOS_CLASS_UTILITY: self = .utility
            case QOS_CLASS_DEFAULT: self = .default
            case QOS_CLASS_USER_INITIATED: self = .userInitiated
            case QOS_CLASS_USER_INTERACTIVE: self = .userInteractive
            case QOS_CLASS_UNSPECIFIED: self = .unspecified
            default: return nil
            }
        }

        public var rawValue: qos_class_t {
            switch self {
            case .background: return QOS_CLASS_BACKGROUND
            case .utility: return QOS_CLASS_UTILITY
            case .default: return QOS_CLASS_DEFAULT
            case .userInitiated: return QOS_CLASS_USER_INITIATED
            case .userInteractive: return QOS_CLASS_USER_INTERACTIVE
            case .unspecified: return QOS_CLASS_UNSPECIFIED
            }
        }

        /// prioridade de fila global (DISPATCH_QUEUE_PRIORITY_*) equivalente
        internal var _priority: Int {
            switch self {
            case .userInteractive, .userInitiated: return 2
            case .default, .unspecified: return 0
            case .utility: return -2
            case .background: return Int(Int16.min)
            }
        }
    }

    public init(qosClass: QoSClass, relativePriority: Int) {
        self.qosClass = qosClass
        self.relativePriority = relativePriority
    }

    public static func == (a: DispatchQoS, b: DispatchQoS) -> Bool {
        a.qosClass == b.qosClass && a.relativePriority == b.relativePriority
    }
}

// MARK: - DispatchWorkItemFlags

public struct DispatchWorkItemFlags: OptionSet, RawRepresentable {
    public let rawValue: UInt
    public init(rawValue: UInt) { self.rawValue = rawValue }

    public static var barrier: DispatchWorkItemFlags { DispatchWorkItemFlags(rawValue: 0x1) }
    public static var detached: DispatchWorkItemFlags { DispatchWorkItemFlags(rawValue: 0x2) }
    public static var assignCurrentContext: DispatchWorkItemFlags { DispatchWorkItemFlags(rawValue: 0x4) }
    public static var noQoS: DispatchWorkItemFlags { DispatchWorkItemFlags(rawValue: 0x8) }
    public static var inheritQoS: DispatchWorkItemFlags { DispatchWorkItemFlags(rawValue: 0x10) }
    public static var enforceQoS: DispatchWorkItemFlags { DispatchWorkItemFlags(rawValue: 0x20) }
}

// MARK: - DispatchWorkItem

public class DispatchWorkItem {
    internal var _block: () -> Void
    internal var _flags: DispatchWorkItemFlags
    internal var _cancelled = false
    internal var _done = false
    internal var _notify: [(DispatchQueue, () -> Void)] = []

    public init(qos: DispatchQoS = .unspecified, flags: DispatchWorkItemFlags = [], block: @escaping @convention(block) () -> Void) {
        _block = block
        _flags = flags
    }

    public init(flags: DispatchWorkItemFlags = [], block: @escaping @convention(block) () -> Void) {
        _block = block
        _flags = flags
    }

    public func perform() {
        if _cancelled { return }
        _block()
        _done = true
        let n = _notify
        _notify = []
        for (q, b) in n { q.async(execute: b) }
    }

    public func wait() { if !_done && !_cancelled { perform() } }

    public func wait(timeout: DispatchTime) -> DispatchTimeoutResult { wait(); return .success }

    public func wait(wallTimeout: DispatchWallTime) -> DispatchTimeoutResult { wait(); return .success }

    public func notify(qos: DispatchQoS = .unspecified, flags: DispatchWorkItemFlags = [], queue: DispatchQueue,
                       execute: @escaping @convention(block) () -> Void) {
        if _done { queue.async(execute: execute) } else { _notify.append((queue, execute)) }
    }

    public func notify(queue: DispatchQueue, execute: DispatchWorkItem) {
        notify(queue: queue) { execute.perform() }
    }

    public func cancel() { _cancelled = true }

    public var isCancelled: Bool { _cancelled }
}

public enum DispatchTimeoutResult {
    case success
    case timedOut
}

// MARK: - tempo

public struct DispatchTime: Comparable {
    public let rawValue: dispatch_time_t

    public static func now() -> DispatchTime { DispatchTime(rawValue: _mos_time(0, 0)) }

    public static var distantFuture: DispatchTime { DispatchTime(rawValue: ~0) }

    fileprivate init(rawValue: dispatch_time_t) { self.rawValue = rawValue }

    public init(uptimeNanoseconds: UInt64) { rawValue = uptimeNanoseconds == 0 ? 1 : uptimeNanoseconds }

    public var uptimeNanoseconds: UInt64 { rawValue == ~0 ? ~0 : rawValue }

    public static func < (a: DispatchTime, b: DispatchTime) -> Bool { a.rawValue < b.rawValue }
    public static func == (a: DispatchTime, b: DispatchTime) -> Bool { a.rawValue == b.rawValue }
}

public struct DispatchWallTime: Comparable {
    public let rawValue: dispatch_time_t
    public static func now() -> DispatchWallTime { DispatchWallTime(rawValue: _mos_time(~1, 0)) }
    public static var distantFuture: DispatchWallTime { DispatchWallTime(rawValue: ~0) }
    fileprivate init(rawValue: dispatch_time_t) { self.rawValue = rawValue }
    public static func < (a: DispatchWallTime, b: DispatchWallTime) -> Bool { a.rawValue < b.rawValue }
    public static func == (a: DispatchWallTime, b: DispatchWallTime) -> Bool { a.rawValue == b.rawValue }
}

public enum DispatchTimeInterval: Equatable {
    case seconds(Int)
    case milliseconds(Int)
    case microseconds(Int)
    case nanoseconds(Int)
    case never

    internal var _nanoseconds: Int64 {
        switch self {
        case .seconds(let s): return Int64(s) &* 1_000_000_000
        case .milliseconds(let ms): return Int64(ms) &* 1_000_000
        case .microseconds(let us): return Int64(us) &* 1_000
        case .nanoseconds(let ns): return Int64(ns)
        case .never: return Int64.max
        }
    }
}

public func + (time: DispatchTime, seconds: Double) -> DispatchTime {
    if time.rawValue == ~0 { return time }
    let d = Int64(seconds * 1_000_000_000)
    return DispatchTime(rawValue: _mos_time(time.rawValue, d))
}
public func - (time: DispatchTime, seconds: Double) -> DispatchTime { time + (-seconds) }
public func + (time: DispatchTime, interval: DispatchTimeInterval) -> DispatchTime {
    if time.rawValue == ~0 || interval == .never { return .distantFuture }
    return DispatchTime(rawValue: _mos_time(time.rawValue, interval._nanoseconds))
}
public func - (time: DispatchTime, interval: DispatchTimeInterval) -> DispatchTime {
    if time.rawValue == ~0 { return time }
    return DispatchTime(rawValue: _mos_time(time.rawValue, -interval._nanoseconds))
}
public func + (time: DispatchWallTime, seconds: Double) -> DispatchWallTime {
    DispatchWallTime(rawValue: _mos_time(time.rawValue, Int64(seconds * 1_000_000_000)))
}
public func + (time: DispatchWallTime, interval: DispatchTimeInterval) -> DispatchWallTime {
    DispatchWallTime(rawValue: _mos_time(time.rawValue, interval._nanoseconds))
}

// MARK: - DispatchQueue

extension DispatchQueue {
    public struct Attributes: OptionSet {
        public let rawValue: UInt64
        public init(rawValue: UInt64) { self.rawValue = rawValue }
        public static var concurrent: Attributes { Attributes(rawValue: 1 << 1) }
        public static var initiallyInactive: Attributes { Attributes(rawValue: 1 << 2) }
    }

    public enum AutoreleaseFrequency {
        case inherit
        case workItem
        case never
    }

    public enum GlobalQueuePriority {
        case high, `default`, low, background
    }

    public class var main: DispatchQueue { _mos_main_queue() }

    public class func global(qos: DispatchQoS.QoSClass = .default) -> DispatchQueue {
        _mos_global_queue(qos._priority, 0)
    }

    public class func global(priority: GlobalQueuePriority) -> DispatchQueue {
        switch priority {
        case .high: return _mos_global_queue(2, 0)
        case .default: return _mos_global_queue(0, 0)
        case .low: return _mos_global_queue(-2, 0)
        case .background: return _mos_global_queue(Int(Int16.min), 0)
        }
    }

    public convenience init(label: String, qos: DispatchQoS = .unspecified, attributes: Attributes = [],
                            autoreleaseFrequency: AutoreleaseFrequency = .inherit, target: DispatchQueue? = nil) {
        // DISPATCH_QUEUE_CONCURRENT é &_dispatch_queue_attr_concurrent; o dispatch do mOS só distingue nulo (serial)
        // de não nulo, então qualquer endereço estável serve
        let attr: __OS_dispatch_queue_attr? = attributes.contains(.concurrent)
            ? unsafeBitCast(UnsafeRawPointer(_mos_concurrent_attr), to: __OS_dispatch_queue_attr.self) : nil
        self.init(__label: label, attr: attr)
        if let target { _mos_set_target(self, target) }
    }

    public var label: String { String(cString: _mos_queue_label(self)) }

    public func async(group: DispatchGroup? = nil, qos: DispatchQoS = .unspecified, flags: DispatchWorkItemFlags = [],
                      execute work: @escaping @convention(block) () -> Void) {
        if let group {
            _mos_group_async(group, self, work)
        } else if flags.contains(.barrier) {
            _mos_barrier_async(self, work)
        } else {
            _mos_async(self, work)
        }
    }

    public func async(execute workItem: DispatchWorkItem) {
        _mos_async(self) { workItem.perform() }
    }

    public func async(group: DispatchGroup, execute workItem: DispatchWorkItem) {
        _mos_group_async(group, self) { workItem.perform() }
    }

    public func sync(execute workItem: DispatchWorkItem) {
        _mosSync(barrier: workItem._flags.contains(.barrier)) { workItem.perform() }
    }

    public func sync<T>(execute work: () throws -> T) rethrows -> T {
        try _mosSync(barrier: false, work)
    }

    public func sync<T>(flags: DispatchWorkItemFlags, execute work: () throws -> T) rethrows -> T {
        try _mosSync(barrier: flags.contains(.barrier), work)
    }

    // sem bloco: o dispatch_sync do mOS copia o bloco (_Block_copy), o que o Swift veria como fuga da closure;
    // a variante _f leva só um ponteiro para a closure, que fica na pilha durante a chamada
    internal func _mosSync<T>(barrier: Bool, _ work: () throws -> T) rethrows -> T {
        var result: Result<T, Error>?
        withoutActuallyEscaping(work) { w in
            var body: () -> Void = { result = Result { try w() } }
            withUnsafeMutablePointer(to: &body) { p in
                if barrier { _mos_barrier_sync_f(self, p, _mos_run_closure) } else { _mos_sync_f(self, p, _mos_run_closure) }
            }
        }
        switch result! {
        case .success(let v): return v
        case .failure(let e): try { throw e }()
        }
        fatalError("inalcançável")
    }

    public func asyncAfter(deadline: DispatchTime, qos: DispatchQoS = .unspecified, flags: DispatchWorkItemFlags = [],
                           execute work: @escaping @convention(block) () -> Void) {
        _mos_after(deadline.rawValue, self, work)
    }

    public func asyncAfter(wallDeadline: DispatchWallTime, qos: DispatchQoS = .unspecified, flags: DispatchWorkItemFlags = [],
                           execute work: @escaping @convention(block) () -> Void) {
        _mos_after(wallDeadline.rawValue, self, work)
    }

    public func asyncAfter(deadline: DispatchTime, execute: DispatchWorkItem) {
        _mos_after(deadline.rawValue, self) { execute.perform() }
    }

    public func asyncAfter(wallDeadline: DispatchWallTime, execute: DispatchWorkItem) {
        _mos_after(wallDeadline.rawValue, self) { execute.perform() }
    }
}

// MARK: - DispatchObject

extension DispatchObject {
    public func suspend() { _mos_suspend(self) }
    public func resume() { _mos_resume(self) }
    public func activate() { _mos_activate(self) }
}

// MARK: - DispatchGroup

extension DispatchGroup {
    public func notify(qos: DispatchQoS = .unspecified, flags: DispatchWorkItemFlags = [], queue: DispatchQueue,
                       execute work: @escaping @convention(block) () -> Void) {
        _mos_group_notify(self, queue, work)
    }

    public func notify(queue: DispatchQueue, work: DispatchWorkItem) {
        _mos_group_notify(self, queue) { work.perform() }
    }

    public func enter() { _mos_group_enter(self) }
    public func leave() { _mos_group_leave(self) }

    public func wait() { _ = _mos_group_wait(self, ~0) }

    public func wait(timeout: DispatchTime) -> DispatchTimeoutResult {
        _mos_group_wait(self, timeout.rawValue) == 0 ? .success : .timedOut
    }

    public func wait(wallTimeout timeout: DispatchWallTime) -> DispatchTimeoutResult {
        _mos_group_wait(self, timeout.rawValue) == 0 ? .success : .timedOut
    }
}

// MARK: - DispatchSemaphore

extension DispatchSemaphore {
    public func signal() -> Int { _mos_sema_signal(self) }

    public func wait() { _ = _mos_sema_wait(self, ~0) }

    public func wait(timeout: DispatchTime) -> DispatchTimeoutResult {
        _mos_sema_wait(self, timeout.rawValue) == 0 ? .success : .timedOut
    }

    public func wait(wallTimeout: DispatchWallTime) -> DispatchTimeoutResult {
        _mos_sema_wait(self, wallTimeout.rawValue) == 0 ? .success : .timedOut
    }
}

// MARK: - fontes

extension DispatchSource {
    public static func makeReadSource(fileDescriptor: Int32, queue: DispatchQueue? = nil) -> DispatchSourceRead {
        _mos_as(_mos_source_create(_mos_source_type("_dispatch_source_type_read"), UInt(fileDescriptor), 0, queue), DispatchSourceRead.self)
    }

    public static func makeWriteSource(fileDescriptor: Int32, queue: DispatchQueue? = nil) -> DispatchSourceWrite {
        _mos_as(_mos_source_create(_mos_source_type("_dispatch_source_type_write"), UInt(fileDescriptor), 0, queue), DispatchSourceWrite.self)
    }

    public static func makeTimerSource(flags: DispatchSource.TimerFlags = [], queue: DispatchQueue? = nil) -> DispatchSourceTimer {
        _mos_as(_mos_source_create(_mos_source_type("_dispatch_source_type_timer"), 0, flags.rawValue, queue), DispatchSourceTimer.self)
    }

    public struct TimerFlags: OptionSet {
        public let rawValue: UInt
        public init(rawValue: UInt) { self.rawValue = rawValue }
        public static var strict: TimerFlags { TimerFlags(rawValue: 1) }
    }
}

extension DispatchSourceProtocol {
    public func setEventHandler(qos: DispatchQoS = .unspecified, flags: DispatchWorkItemFlags = [],
                                handler: (@convention(block) () -> Void)?) {
        _mos_source_set_event(_mos_src(self), handler)
    }

    public func setEventHandler(handler: DispatchWorkItem) {
        _mos_source_set_event(_mos_src(self)) { handler.perform() }
    }

    public func setCancelHandler(qos: DispatchQoS = .unspecified, flags: DispatchWorkItemFlags = [],
                                 handler: (@convention(block) () -> Void)?) {
        _mos_source_set_cancel(_mos_src(self), handler)
    }

    public func setRegistrationHandler(qos: DispatchQoS = .unspecified, flags: DispatchWorkItemFlags = [],
                                       handler: (@convention(block) () -> Void)?) {
        _mos_source_set_registration(_mos_src(self), handler)
    }

    public func cancel() { _mos_source_cancel(_mos_src(self)) }
    public func resume() { _mos_resume(_mos_src(self)) }
    public func suspend() { _mos_suspend(_mos_src(self)) }
    public func activate() { _mos_activate(_mos_src(self)) }

    public var handle: UInt { _mos_source_handle(_mos_src(self)) }
    public var mask: UInt { _mos_source_mask(_mos_src(self)) }
    public var data: UInt { _mos_source_data(_mos_src(self)) }
    public var isCancelled: Bool { _mos_source_testcancel(_mos_src(self)) != 0 }
}

extension DispatchSourceTimer {
    public func schedule(deadline: DispatchTime, repeating interval: DispatchTimeInterval = .never,
                         leeway: DispatchTimeInterval = .nanoseconds(0)) {
        let iv: UInt64 = interval == .never ? ~0 : UInt64(interval._nanoseconds)
        _mos_source_set_timer(_mos_src(self), deadline.rawValue, iv, UInt64(leeway._nanoseconds))
    }

    public func schedule(deadline: DispatchTime, repeating interval: Double, leeway: DispatchTimeInterval = .nanoseconds(0)) {
        _mos_source_set_timer(_mos_src(self), deadline.rawValue, UInt64(interval * 1_000_000_000), UInt64(leeway._nanoseconds))
    }
}

// MARK: - dispatchPrecondition (não verificado no mOS)

public enum DispatchPredicate {
    case onQueue(DispatchQueue)
    case onQueueAsBarrier(DispatchQueue)
    case notOnQueue(DispatchQueue)
}

@_transparent
public func dispatchPrecondition(condition: @autoclosure () -> DispatchPredicate) {}
