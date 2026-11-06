// mOS SDK — RunLoop, Timer, Thread e travas (NSLock, NSRecursiveLock, NSCondition) do Foundation clássico, sobre o
// CFRunLoop do libSystem (userland/libsystem/cfrunloop.c: um laço por thread, timers, observadores, a fila principal
// do GCD). Como no swift-corelibs-foundation: RunLoop/Timer são invólucros Swift dos objetos CF.

import Synchronization

@_silgen_name("pthread_main_np") private func _mos_pthread_main_np() -> Int32
@_silgen_name("pthread_self") private func _mos_pthread_self() -> UInt
@_silgen_name("usleep") private func _mos_usleep(_ us: UInt32) -> Int32

// MARK: - RunLoop

open class RunLoop: NSObject, @unchecked Sendable {
    public let _cf: CFRunLoop
    init(_cf: CFRunLoop) { self._cf = _cf }

    public struct Mode: RawRepresentable, Hashable, Sendable {
        public var rawValue: String
        public init(rawValue: String) { self.rawValue = rawValue }
        public init(_ rawValue: String) { self.rawValue = rawValue }
        public static let `default` = Mode("kCFRunLoopDefaultMode")
        public static let common = Mode("kCFRunLoopCommonModes")
        public static let tracking = Mode("UITrackingRunLoopMode")
        public static let eventTracking = Mode("NSEventTrackingRunLoopMode")
        public static let modalPanel = Mode("NSModalPanelRunLoopMode")
    }

    private static let loopsLock = NSLock()
    private nonisolated(unsafe) static var loops: [OpaquePointer: RunLoop] = [:]
    private static func wrap(_ cf: CFRunLoop) -> RunLoop {
        loopsLock.lock(); defer { loopsLock.unlock() }
        if let r = loops[cf] { return r }
        let r = RunLoop(_cf: cf)
        loops[cf] = r
        return r
    }
    open class var main: RunLoop { wrap(CFRunLoopGetMain()) }
    open class var current: RunLoop { wrap(CFRunLoopGetCurrent()) }

    open func getCFRunLoop() -> CFRunLoop { _cf }
    open var currentMode: Mode? { .default }

    /// Os modos são aceitos, mas o CFRunLoop do mOS roda tudo em todos os modos.
    open func add(_ timer: Timer, forMode mode: Mode) { timer._schedule(on: self) }
    open func add(_ port: Any, forMode mode: Mode) {}

    open func perform(_ block: @escaping @Sendable () -> Void) { perform(inModes: [.common], block: block) }
    open func perform(inModes modes: [Mode], block: @escaping @Sendable () -> Void) {
        let t = Timer(fire: Date(timeIntervalSinceReferenceDate: 0), interval: 0, repeats: false) { _ in block() }
        t._schedule(on: self)
        CFRunLoopWakeUp(_cf)
    }

    open func run() { while run(mode: .default, before: .distantFuture) {} }
    open func run(until limitDate: Date) {
        while limitDate.timeIntervalSinceNow > 0, run(mode: .default, before: limitDate) {}
    }
    /// Roda o laço até `limitDate` ou até tratar uma fonte; false se não há nada agendado (o laço sairia na hora).
    @discardableResult
    open func run(mode: Mode, before limitDate: Date) -> Bool {
        let r = CFRunLoopRunInMode(kCFRunLoopDefaultMode, Swift.max(0, limitDate.timeIntervalSinceNow), true)
        return r != .finished
    }
    open func limitDate(forMode mode: Mode) -> Date? { nil }
    open func acceptInput(forMode mode: Mode, before limitDate: Date) { _ = run(mode: mode, before: limitDate) }
}

// MARK: - Timer

open class Timer: NSObject, @unchecked Sendable {
    private var _cf: CFRunLoopTimer?
    private weak var _loop: RunLoop?
    private var _retained: Unmanaged<Timer>?
    private let _block: (Timer) -> Void
    private var _fireDate: Date
    private var _valid = true
    public let timeInterval: TimeInterval
    private let _repeats: Bool
    open var tolerance: TimeInterval = 0
    open var userInfo: Any?

    public init(fire date: Date, interval: TimeInterval, repeats: Bool, block: @escaping @Sendable (Timer) -> Void) {
        _fireDate = date
        timeInterval = repeats ? Swift.max(interval, 0.0001) : 0
        _repeats = repeats
        _block = block
    }
    public convenience init(timeInterval interval: TimeInterval, repeats: Bool, block: @escaping @Sendable (Timer) -> Void) {
        self.init(fire: Date(timeIntervalSinceNow: interval), interval: interval, repeats: repeats, block: block)
    }
    open class func scheduledTimer(withTimeInterval interval: TimeInterval, repeats: Bool,
                                   block: @escaping @Sendable (Timer) -> Void) -> Timer {
        let t = Timer(timeInterval: interval, repeats: repeats, block: block)
        RunLoop.current.add(t, forMode: .default)
        return t
    }

    open var isValid: Bool { _valid }
    open var fireDate: Date {
        get { _cf.map { Date(timeIntervalSinceReferenceDate: CFRunLoopTimerGetNextFireDate($0)) } ?? _fireDate }
        set { _fireDate = newValue; if let c = _cf { CFRunLoopTimerSetNextFireDate(c, newValue.timeIntervalSinceReferenceDate) } }
    }

    /// Executa agora (sem mudar o agendamento); um timer de um disparo fica inválido.
    open func fire() {
        guard _valid else { return }
        _block(self)
        if !_repeats { invalidate() }
    }

    open func invalidate() {
        guard _valid else { return }
        _valid = false
        if let c = _cf {
            CFRunLoopTimerInvalidate(c)
            if let l = _loop { CFRunLoopRemoveTimer(l._cf, c, kCFRunLoopCommonModes) }
        }
        _cf = nil
        // o laço segurava o timer enquanto agendado
        if let r = _retained { _retained = nil; r.release() }
    }

    func _schedule(on loop: RunLoop) {
        guard _valid, _cf == nil else { return }
        _loop = loop
        _retained = Unmanaged.passRetained(self)
        var ctx = CFRunLoopTimerContext(version: 0, info: _retained!.toOpaque(), retain: nil, release: nil, copyDescription: nil)
        let c = CFRunLoopTimerCreate(nil, _fireDate.timeIntervalSinceReferenceDate, timeInterval, 0, 0, { _, info in
            guard let info else { return }
            Unmanaged<Timer>.fromOpaque(info).takeUnretainedValue()._cfFired()
        }, &ctx)
        _cf = c
        CFRunLoopAddTimer(loop._cf, c, kCFRunLoopCommonModes)
    }

    private func _cfFired() {
        guard _valid else { return }
        _block(self)
        if !_repeats { invalidate() }
    }
}

// MARK: - Thread

open class Thread: NSObject, @unchecked Sendable {
    private static let lock = NSLock()
    private nonisolated(unsafe) static var threads: [UInt: Thread] = [:]
    private let _block: (() -> Void)?
    open var name: String?
    open var stackSize: Int = 512 * 1024
    open var qualityOfService: QualityOfService = .default
    open var threadDictionary: [AnyHashable: Any] = [:]
    open private(set) var isExecuting = false
    open private(set) var isFinished = false
    open private(set) var isCancelled = false

    public override init() { _block = nil }
    public init(block: @escaping @Sendable () -> Void) { _block = block }

    open class var current: Thread {
        if _mos_pthread_main_np() != 0 { return _main }
        let id = _mos_pthread_self()
        lock.lock(); defer { lock.unlock() }
        if let t = threads[id] { return t }
        let t = Thread()
        threads[id] = t
        return t
    }
    open class var main: Thread { _main }
    private static let _main: Thread = {
        let t = Thread(); t.name = "main"; return t
    }()
    open class var isMainThread: Bool { _mos_pthread_main_np() != 0 }
    open var isMainThread: Bool { self === Thread.main || (Thread.isMainThread && self === Thread.current) }
    open class var isMultiThreaded: Bool { true }

    open class func sleep(forTimeInterval ti: TimeInterval) {
        var left = ti
        while left > 0 { let s = Swift.min(left, 1); _ = _mos_usleep(UInt32(s * 1_000_000)); left -= s }
    }
    open class func sleep(until date: Date) { sleep(forTimeInterval: date.timeIntervalSinceNow) }
    open class func exit() { Swift.fatalError("Thread.exit() não é suportado no mOS") }
    open class var callStackSymbols: [String] { [] }
    open class var callStackReturnAddresses: [NSNumber] { [] }

    open func cancel() { isCancelled = true }
    open func main() { _block?() }
    /// Roda `main()` numa thread nova (pthread) — o mOS tem threads de kernel.
    open func start() {
        isExecuting = true
        let box = Unmanaged.passRetained(self)
        var tid: UInt = 0
        _ = _mos_pthread_create(&tid, nil, { arg in
            let t = Unmanaged<Thread>.fromOpaque(arg!).takeRetainedValue()
            Thread.lock.lock(); Thread.threads[_mos_pthread_self()] = t; Thread.lock.unlock()
            t.main()
            t.isExecuting = false; t.isFinished = true
            Thread.lock.lock(); Thread.threads[_mos_pthread_self()] = nil; Thread.lock.unlock()
            return nil
        }, box.toOpaque())
        _ = _mos_pthread_detach(tid)
    }
    open class func detachNewThread(_ block: @escaping @Sendable () -> Void) { Thread(block: block).start() }
}

@_silgen_name("pthread_create")
private func _mos_pthread_create(_ t: UnsafeMutablePointer<UInt>, _ attr: UnsafeRawPointer?,
                                 _ fn: @convention(c) (UnsafeMutableRawPointer?) -> UnsafeMutableRawPointer?,
                                 _ arg: UnsafeMutableRawPointer?) -> Int32
@_silgen_name("pthread_detach") private func _mos_pthread_detach(_ t: UInt) -> Int32

public enum QualityOfService: Int, Sendable {
    case userInteractive = 0x21, userInitiated = 0x19, utility = 0x11, background = 0x09, `default` = -1
}

// MARK: - travas

open class NSLock: NSObject, NSLocking, @unchecked Sendable {
    private let _m = Mutex<Void>(())
    open var name: String?
    public override init() {}
    open func lock() { _m._unsafeLock() }
    open func unlock() { _m._unsafeUnlock() }
    open func `try`() -> Bool { _m._unsafeTryLock() }
    open func lock(before limit: Date) -> Bool {
        while !_m._unsafeTryLock() {
            if limit.timeIntervalSinceNow <= 0 { return false }
            _ = _mos_usleep(100)
        }
        return true
    }
}

extension NSLocking {
    public func withLock<R>(_ body: () throws -> R) rethrows -> R {
        lock(); defer { unlock() }
        return try body()
    }
}

open class NSRecursiveLock: NSObject, NSLocking, @unchecked Sendable {
    private let _m = Mutex<Void>(())
    private var _owner: UInt = 0
    private var _depth = 0
    open var name: String?
    public override init() {}
    open func lock() {
        let me = _mos_pthread_self()
        if _owner == me { _depth += 1; return }
        _m._unsafeLock()
        _owner = me; _depth = 1
    }
    open func unlock() {
        _depth -= 1
        if _depth == 0 { _owner = 0; _m._unsafeUnlock() }
    }
    open func `try`() -> Bool {
        let me = _mos_pthread_self()
        if _owner == me { _depth += 1; return true }
        guard _m._unsafeTryLock() else { return false }
        _owner = me; _depth = 1
        return true
    }
}

/// Condição com espera ativa curta (o mOS não tem pthread_cond com futex no Swift daqui).
open class NSCondition: NSObject, NSLocking, @unchecked Sendable {
    private let _m = Mutex<Void>(())
    private var _signals = 0
    open var name: String?
    public override init() {}
    open func lock() { _m._unsafeLock() }
    open func unlock() { _m._unsafeUnlock() }
    open func wait() { _ = wait(until: .distantFuture) }
    open func wait(until limit: Date) -> Bool {
        let start = _signals
        while _signals == start {
            _m._unsafeUnlock()
            _ = _mos_usleep(200)
            _m._unsafeLock()
            if limit.timeIntervalSinceNow <= 0 { return false }
        }
        return true
    }
    open func signal() { _signals &+= 1 }
    open func broadcast() { _signals &+= 1 }
}
