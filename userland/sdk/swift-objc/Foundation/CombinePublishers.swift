// mOS — overlay Foundation: o que a Apple põe do Combine no módulo Foundation, com a API e os nomes mangled dela
// (frente SwiftUI/Combine; combinado com a frente do Foundation): Timer.publish/Timer.TimerPublisher,
// NotificationCenter.Publisher, URLSession.DataTaskPublisher, NSObject.KeyValueObservingPublisher e RunLoop e
// OperationQueue como Scheduler. Sobre o Combine.framework do mOS e as classes do Foundation do libSystem.
// (neste módulo o importador não faz a ponte NSString/NSArray: as conversões são explícitas)
#if canImport(Combine)
import Combine
import Dispatch
import Darwin

/// trava simples (pthread): o NSLock não está no módulo clang Foundation da variante
internal final class _MOSCombineLock {
    private let m: UnsafeMutablePointer<pthread_mutex_t>
    init() { m = .allocate(capacity: 1); pthread_mutex_init(m, nil) }
    deinit { pthread_mutex_destroy(m); m.deallocate() }
    func lock() { pthread_mutex_lock(m) }
    func unlock() { pthread_mutex_unlock(m) }
}

// MARK: - estado de demanda comum

/// assinatura que entrega só com demanda; sem demanda, o valor cai (como os publishers de evento da Apple)
internal final class _MOSDemandSubscription<Output, Failure: Error>: Subscription, CustomStringConvertible {
    private let lock = _MOSCombineLock()
    private var downstream: AnySubscriber<Output, Failure>?
    private var demand: Subscribers.Demand = .none
    private var onCancel: (() -> Void)?
    let description: String

    init<S: Subscriber>(_ name: String, _ s: S) where S.Input == Output, S.Failure == Failure {
        description = name
        downstream = AnySubscriber(s)
    }

    func setCancel(_ c: @escaping () -> Void) { lock.lock(); onCancel = c; lock.unlock() }

    func request(_ d: Subscribers.Demand) { lock.lock(); demand += d; lock.unlock() }

    func cancel() {
        lock.lock()
        let c = onCancel
        onCancel = nil
        downstream = nil
        lock.unlock()
        c?()
    }

    /// devolve false se a assinatura já acabou
    @discardableResult
    func send(_ v: Output) -> Bool {
        lock.lock()
        guard let d = downstream else { lock.unlock(); return false }
        guard demand > 0 else { lock.unlock(); return true }
        demand -= 1
        lock.unlock()
        let more = d.receive(v)
        lock.lock(); demand += more; lock.unlock()
        return true
    }

    func finish(_ c: Subscribers.Completion<Failure>) {
        lock.lock()
        let d = downstream
        downstream = nil
        onCancel = nil
        lock.unlock()
        d?.receive(completion: c)
    }

    var hasDemand: Bool { lock.lock(); defer { lock.unlock() }; return demand > 0 && downstream != nil }
}

// MARK: - Timer

extension Timer {
    public static func publish(every interval: TimeInterval, tolerance: TimeInterval? = nil, on runLoop: RunLoop,
                               in mode: RunLoop.Mode, options: RunLoop.SchedulerOptions? = nil) -> Timer.TimerPublisher {
        TimerPublisher(interval: interval, tolerance: tolerance, runLoop: runLoop, mode: mode, options: options)
    }

    public final class TimerPublisher: ConnectablePublisher {
        public typealias Output = Date
        public typealias Failure = Never

        public let interval: TimeInterval
        public let tolerance: TimeInterval?
        public let runLoop: RunLoop
        public let mode: RunLoop.Mode
        public let options: RunLoop.SchedulerOptions?

        private let lock = _MOSCombineLock()
        private var subs: [ObjectIdentifier: _MOSDemandSubscription<Date, Never>] = [:]
        private var timer: Timer?

        public init(interval: TimeInterval, tolerance: TimeInterval? = nil, runLoop: RunLoop, mode: RunLoop.Mode,
                    options: RunLoop.SchedulerOptions? = nil) {
            self.interval = interval
            self.tolerance = tolerance
            self.runLoop = runLoop
            self.mode = mode
            self.options = options
        }

        public func receive<S: Subscriber>(subscriber: S) where S.Input == Date, S.Failure == Never {
            let sub = _MOSDemandSubscription<Date, Never>("Timer.TimerPublisher", subscriber)
            let key = ObjectIdentifier(sub)
            lock.lock(); subs[key] = sub; lock.unlock()
            sub.setCancel { [weak self] in
                guard let self else { return }
                self.lock.lock(); self.subs[key] = nil; self.lock.unlock()
            }
            subscriber.receive(subscription: sub)
        }

        private func fire(_ date: Date) {
            lock.lock()
            let all = Array(subs.values)
            lock.unlock()
            for s in all { s.send(date) }
        }

        public func connect() -> Cancellable {
            lock.lock()
            if let t = timer { lock.unlock(); return _MOSTimerConnection(self, t) }
            let t = Timer(timeInterval: interval, repeats: true) { [weak self] _ in self?.fire(Date()) }
            if let tol = tolerance { t.tolerance = tol }
            timer = t
            lock.unlock()
            runLoop.add(t, forMode: mode)
            return _MOSTimerConnection(self, t)
        }

        fileprivate func disconnect(_ t: Timer) {
            lock.lock()
            if timer === t { timer = nil }
            lock.unlock()
            t.invalidate()
        }
    }
}

private final class _MOSTimerConnection: Cancellable {
    private weak var publisher: Timer.TimerPublisher?
    private let timer: Timer
    init(_ p: Timer.TimerPublisher, _ t: Timer) { publisher = p; timer = t }
    func cancel() {
        if let p = publisher { p.disconnect(timer) } else { timer.invalidate() }
    }
}

// MARK: - NotificationCenter

extension NotificationCenter {
    public func publisher(for name: Notification.Name, object: AnyObject? = nil) -> NotificationCenter.Publisher {
        Publisher(center: self, name: name, object: object)
    }

    public struct Publisher: Combine.Publisher {
        public typealias Output = Notification
        public typealias Failure = Never

        public let center: NotificationCenter
        public let name: Notification.Name
        public let object: AnyObject?

        public init(center: NotificationCenter, name: Notification.Name, object: AnyObject? = nil) {
            self.center = center
            self.name = name
            self.object = object
        }

        public func receive<S: Subscriber>(subscriber: S) where S.Input == Notification, S.Failure == Never {
            let sub = _MOSDemandSubscription<Notification, Never>("NotificationCenter.Publisher", subscriber)
            let token = center.addObserver(forName: name, object: object, queue: nil) { [weak sub] n in
                sub?.send(Notification._unconditionallyBridgeFromObjectiveC(n))
            }
            let c = center
            sub.setCancel { c.removeObserver(token) }
            subscriber.receive(subscription: sub)
        }
    }
}

extension NotificationCenter.Publisher: Equatable {
    public static func == (a: NotificationCenter.Publisher, b: NotificationCenter.Publisher) -> Bool {
        a.center === b.center && a.name == b.name && a.object === b.object
    }
}

// MARK: - URLSession

extension URLSession {
    public func dataTaskPublisher(for url: URL) -> URLSession.DataTaskPublisher {
        DataTaskPublisher(request: URLRequest(url: url), session: self)
    }

    public func dataTaskPublisher(for request: URLRequest) -> URLSession.DataTaskPublisher {
        DataTaskPublisher(request: request, session: self)
    }

    public struct DataTaskPublisher: Combine.Publisher {
        public typealias Output = (data: Data, response: NSURLResponse)
        public typealias Failure = URLError

        public let request: URLRequest
        public let session: URLSession

        public init(request: URLRequest, session: URLSession) {
            self.request = request
            self.session = session
        }

        public func receive<S: Subscriber>(subscriber: S) where S.Input == Output, S.Failure == URLError {
            subscriber.receive(subscription: _MOSDataTaskSubscription(self, subscriber))
        }
    }
}

/// a tarefa só começa na primeira demanda; um valor e o fim, ou o URLError
private final class _MOSDataTaskSubscription<S: Subscriber>: Subscription
    where S.Input == URLSession.DataTaskPublisher.Output, S.Failure == URLError {
    private let lock = _MOSCombineLock()
    private var downstream: S?
    private var task: URLSessionDataTask?
    private let parent: URLSession.DataTaskPublisher

    init(_ p: URLSession.DataTaskPublisher, _ s: S) { parent = p; downstream = s }

    func request(_ demand: Subscribers.Demand) {
        lock.lock()
        guard demand > 0, task == nil, downstream != nil else { lock.unlock(); return }
        let t = parent.session.dataTask(with: parent.request._bridgeToObjectiveC()) { [weak self] data, response, error in
            self?.complete(data, response, error)
        }
        task = t
        lock.unlock()
        t.resume()
    }

    private func complete(_ data: NSData?, _ response: NSURLResponse?, _ error: NSError?) {
        lock.lock()
        guard let d = downstream else { lock.unlock(); return }
        downstream = nil
        task = nil
        lock.unlock()
        if let e = error {
            let ns = e.domain.isEqual(NSURLErrorDomain) ? e : NSError(domain: NSURLErrorDomain, code: -1, userInfo: nil)
            d.receive(completion: .failure(URLError(_nsError: ns)))
            return
        }
        guard let r = response else {
            d.receive(completion: .failure(URLError(_nsError: NSError(domain: NSURLErrorDomain, code: -1011, userInfo: nil))))
            return
        }
        let bytes = data.map { Data._unconditionallyBridgeFromObjectiveC($0) } ?? Data()
        _ = d.receive((data: bytes, response: r))
        d.receive(completion: .finished)
    }

    func cancel() {
        lock.lock()
        let t = task
        task = nil
        downstream = nil
        lock.unlock()
        t?.cancel()
    }
}

// MARK: - KVO

public protocol _KeyValueCodingAndObservingPublishing {}

extension NSObject: _KeyValueCodingAndObservingPublishing {}

extension _KeyValueCodingAndObservingPublishing where Self: NSObject {
    public func publisher<Value>(for keyPath: KeyPath<Self, Value>,
                                 options: NSKeyValueObservingOptions = [.initial, .new]) -> NSObject.KeyValueObservingPublisher<Self, Value> {
        NSObject.KeyValueObservingPublisher(object: self, keyPath: keyPath, options: options)
    }
}

extension NSObject {
    public struct KeyValueObservingPublisher<Subject: NSObject, Value>: Equatable, Combine.Publisher {
        public typealias Output = Value
        public typealias Failure = Never

        public let object: Subject
        public let keyPath: KeyPath<Subject, Value>
        public let options: NSKeyValueObservingOptions

        public init(object: Subject, keyPath: KeyPath<Subject, Value>, options: NSKeyValueObservingOptions) {
            self.object = object
            self.keyPath = keyPath
            self.options = options
        }

        public static func == (a: Self, b: Self) -> Bool {
            a.object === b.object && a.keyPath == b.keyPath && a.options == b.options
        }

        public func didChange() -> Publishers.Map<NSObject.KeyValueObservingPublisher<Subject, Value>, Void> {
            map { _ in () }
        }

        public func receive<S: Subscriber>(subscriber: S) where S.Input == Value, S.Failure == Never {
            guard let path = keyPath._kvcKeyPathString else {
                fatalError("KeyValueObservingPublisher: \(keyPath) não é um caminho @objc")
            }
            let sub = _MOSDemandSubscription<Value, Never>("KeyValueObservingPublisher", subscriber)
            let obj = object
            let kp = keyPath
            let wantPrior = options.contains(.prior)
            let observer = _MOSKVOHelper { [weak sub] change in
                let prior = (change?.object(forKey: _mosNSString("notificationIsPrior")) as? NSNumber)?.boolValue ?? false
                if prior && !wantPrior { return }
                sub?.send(obj[keyPath: kp])
            }
            // o valor inicial sai daqui (com demanda), não do Foundation: o .initial não chega ao addObserver
            var kvoOptions = options
            kvoOptions.remove(.initial)
            obj.addObserver(observer, forKeyPath: _mosNSString(path), options: kvoOptions, context: nil)
            sub.setCancel { obj.removeObserver(observer, forKeyPath: _mosNSString(path), context: nil) }
            subscriber.receive(subscription: _MOSInitialValueSubscription(sub, options.contains(.initial) ? { obj[keyPath: kp] } : nil))
        }
    }
}

/// manda o valor inicial na primeira demanda e depois repassa à assinatura do KVO
private final class _MOSInitialValueSubscription<Value>: Subscription {
    private let inner: _MOSDemandSubscription<Value, Never>
    private var initial: (() -> Value)?
    private let lock = _MOSCombineLock()
    init(_ i: _MOSDemandSubscription<Value, Never>, _ v: (() -> Value)?) { inner = i; initial = v }
    func request(_ d: Subscribers.Demand) {
        inner.request(d)
        lock.lock()
        let v = initial
        initial = nil
        lock.unlock()
        if let v { inner.send(v()) }
    }
    func cancel() { inner.cancel() }
}

// MARK: - RunLoop: Scheduler

extension RunLoop: Scheduler {
    public struct SchedulerTimeType: Strideable, Codable, Hashable {
        public var date: Date

        public init(_ date: Date) { self.date = date }

        public func distance(to other: SchedulerTimeType) -> Stride {
            Stride(floatLiteral: other.date.timeIntervalSinceReferenceDate - date.timeIntervalSinceReferenceDate)
        }

        public func advanced(by n: Stride) -> SchedulerTimeType { SchedulerTimeType(date.addingTimeInterval(n.magnitude)) }

        public struct Stride: SchedulerTimeIntervalConvertible, Comparable, SignedNumeric, ExpressibleByFloatLiteral, Hashable, Codable {
            public typealias FloatLiteralType = TimeInterval
            public typealias IntegerLiteralType = TimeInterval
            public typealias Magnitude = TimeInterval

            /// segundos
            public var magnitude: TimeInterval
            public var timeInterval: TimeInterval { magnitude }

            public init(_ timeInterval: TimeInterval) { magnitude = timeInterval }
            public init(integerLiteral value: TimeInterval) { magnitude = value }
            public init(floatLiteral value: TimeInterval) { magnitude = value }
            public init?<T: BinaryInteger>(exactly source: T) {
                guard let v = TimeInterval(exactly: source) else { return nil }
                magnitude = v
            }

            public static func < (a: Stride, b: Stride) -> Bool { a.magnitude < b.magnitude }
            public static func * (a: Stride, b: Stride) -> Stride { Stride(a.magnitude * b.magnitude) }
            public static func + (a: Stride, b: Stride) -> Stride { Stride(a.magnitude + b.magnitude) }
            public static func - (a: Stride, b: Stride) -> Stride { Stride(a.magnitude - b.magnitude) }
            public static func *= (a: inout Stride, b: Stride) { a = a * b }
            public static func += (a: inout Stride, b: Stride) { a = a + b }
            public static func -= (a: inout Stride, b: Stride) { a = a - b }

            public static func seconds(_ s: Int) -> Stride { Stride(TimeInterval(s)) }
            public static func seconds(_ s: Double) -> Stride { Stride(s) }
            public static func milliseconds(_ ms: Int) -> Stride { Stride(TimeInterval(ms) / 1_000) }
            public static func microseconds(_ us: Int) -> Stride { Stride(TimeInterval(us) / 1_000_000) }
            public static func nanoseconds(_ ns: Int) -> Stride { Stride(TimeInterval(ns) / 1_000_000_000) }
        }
    }

    public struct SchedulerOptions {}

    public var now: SchedulerTimeType { SchedulerTimeType(Date()) }
    public var minimumTolerance: SchedulerTimeType.Stride { SchedulerTimeType.Stride(0) }

    public func schedule(options: SchedulerOptions?, _ action: @escaping () -> Void) {
        perform(action)
    }

    public func schedule(after date: SchedulerTimeType, tolerance: SchedulerTimeType.Stride, options: SchedulerOptions?,
                         _ action: @escaping () -> Void) {
        let t = Timer(fire: date.date._bridgeToObjectiveC(), interval: 0, repeats: false) { _ in action() }
        t.tolerance = tolerance.magnitude
        add(t, forMode: RunLoop.Mode.default)
    }

    public func schedule(after date: SchedulerTimeType, interval: SchedulerTimeType.Stride, tolerance: SchedulerTimeType.Stride,
                         options: SchedulerOptions?, _ action: @escaping () -> Void) -> Cancellable {
        let t = Timer(fire: date.date._bridgeToObjectiveC(), interval: interval.magnitude, repeats: true) { _ in action() }
        t.tolerance = tolerance.magnitude
        add(t, forMode: RunLoop.Mode.default)
        return AnyCancellable { t.invalidate() }
    }
}

// MARK: - OperationQueue: Scheduler

extension OperationQueue: Scheduler {
    public struct SchedulerTimeType: Strideable, Codable, Hashable {
        public var date: Date
        public init(_ date: Date) { self.date = date }
        public func distance(to other: SchedulerTimeType) -> Stride {
            Stride(other.date.timeIntervalSinceReferenceDate - date.timeIntervalSinceReferenceDate)
        }
        public func advanced(by n: Stride) -> SchedulerTimeType { SchedulerTimeType(date.addingTimeInterval(n.magnitude)) }

        public struct Stride: SchedulerTimeIntervalConvertible, Comparable, SignedNumeric, ExpressibleByFloatLiteral, Hashable, Codable {
            public typealias FloatLiteralType = TimeInterval
            public typealias IntegerLiteralType = TimeInterval
            public typealias Magnitude = TimeInterval
            public var magnitude: TimeInterval
            public var timeInterval: TimeInterval { magnitude }
            public init(_ timeInterval: TimeInterval) { magnitude = timeInterval }
            public init(integerLiteral value: TimeInterval) { magnitude = value }
            public init(floatLiteral value: TimeInterval) { magnitude = value }
            public init?<T: BinaryInteger>(exactly source: T) {
                guard let v = TimeInterval(exactly: source) else { return nil }
                magnitude = v
            }
            public static func < (a: Stride, b: Stride) -> Bool { a.magnitude < b.magnitude }
            public static func * (a: Stride, b: Stride) -> Stride { Stride(a.magnitude * b.magnitude) }
            public static func + (a: Stride, b: Stride) -> Stride { Stride(a.magnitude + b.magnitude) }
            public static func - (a: Stride, b: Stride) -> Stride { Stride(a.magnitude - b.magnitude) }
            public static func *= (a: inout Stride, b: Stride) { a = a * b }
            public static func += (a: inout Stride, b: Stride) { a = a + b }
            public static func -= (a: inout Stride, b: Stride) { a = a - b }
            public static func seconds(_ s: Int) -> Stride { Stride(TimeInterval(s)) }
            public static func seconds(_ s: Double) -> Stride { Stride(s) }
            public static func milliseconds(_ ms: Int) -> Stride { Stride(TimeInterval(ms) / 1_000) }
            public static func microseconds(_ us: Int) -> Stride { Stride(TimeInterval(us) / 1_000_000) }
            public static func nanoseconds(_ ns: Int) -> Stride { Stride(TimeInterval(ns) / 1_000_000_000) }
        }
    }

    public struct SchedulerOptions {}

    public var now: SchedulerTimeType { SchedulerTimeType(Date()) }
    public var minimumTolerance: SchedulerTimeType.Stride { SchedulerTimeType.Stride(0) }

    public func schedule(options: SchedulerOptions?, _ action: @escaping () -> Void) {
        addOperation(action)
    }

    /// o atraso corre numa fila global do dispatch; a ação entra nesta fila na hora
    public func schedule(after date: SchedulerTimeType, tolerance: SchedulerTimeType.Stride, options: SchedulerOptions?,
                         _ action: @escaping () -> Void) {
        let delay = max(0, date.date.timeIntervalSinceNow)
        DispatchQueue.global().asyncAfter(deadline: .now() + delay) { [weak self] in self?.addOperation(action) }
    }

    public func schedule(after date: SchedulerTimeType, interval: SchedulerTimeType.Stride, tolerance: SchedulerTimeType.Stride,
                         options: SchedulerOptions?, _ action: @escaping () -> Void) -> Cancellable {
        let timer = DispatchSource.makeTimerSource(queue: DispatchQueue.global())
        let delay = max(0, date.date.timeIntervalSinceNow)
        timer.setEventHandler { [weak self] in self?.addOperation(action) }
        timer.schedule(deadline: .now() + delay, repeating: interval.magnitude, leeway: .nanoseconds(Int(tolerance.magnitude * 1e9)))
        timer.resume()
        return AnyCancellable { timer.cancel() }
    }
}
#endif
