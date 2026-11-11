// mOS — Combine.framework (tools/build-swift-combine-objc.ps1): Publishers.CombineLatest/3/4, Merge/3...8 e MergeMany,
// que o OpenCombine não tem, com a API e os nomes da Apple (o WhatsApp os usa). Compilado junto do OpenCombine no módulo
// Combine.
//
// Cada publicador de cima recebe demanda ilimitada; a demanda de baixo é respeitada guardando o que chegou sem demanda
// (CombineLatest: só a combinação mais recente; Merge: uma fila). Falha de qualquer um termina tudo na hora.

// MARK: - assinante lateral (um por publicador de cima)

private final class _MOSSide<Input, Failure: Error>: Subscriber {
    let onSubscription: (Subscription) -> Void
    let onValue: (Input) -> Void
    let onCompletion: (Subscribers.Completion<Failure>) -> Void
    init(_ s: @escaping (Subscription) -> Void, _ v: @escaping (Input) -> Void,
         _ c: @escaping (Subscribers.Completion<Failure>) -> Void) {
        onSubscription = s; onValue = v; onCompletion = c
    }
    func receive(subscription: Subscription) { onSubscription(subscription) }
    func receive(_ input: Input) -> Subscribers.Demand { onValue(input); return .none }
    func receive(completion: Subscribers.Completion<Failure>) { onCompletion(completion) }
}

// MARK: - CombineLatest

private final class _MOSCombineLatestInner<Downstream: Subscriber>: Subscription, CustomStringConvertible {
    typealias Output = Downstream.Input
    typealias Failure = Downstream.Failure
    private let lock = UnfairLock.allocate()
    private let combine: ([Any]) -> Output
    private var values: [Any?]
    private var finished: [Bool]
    private var upstreams: [Subscription?]
    private var downstream: Downstream?
    private var demand = Subscribers.Demand.none
    private var pending = false
    private var done = false

    init(_ downstream: Downstream, _ count: Int, _ combine: @escaping ([Any]) -> Output) {
        self.downstream = downstream
        self.combine = combine
        values = Array(repeating: nil, count: count)
        finished = Array(repeating: false, count: count)
        upstreams = Array(repeating: nil, count: count)
    }
    deinit { lock.deallocate() }

    var description: String { "CombineLatest" }

    func attach<P: Publisher>(_ p: P, _ i: Int) where P.Failure == Failure {
        p.subscribe(_MOSSide<P.Output, P.Failure>({ [self] in received(subscription: $0, i) },
                                                 { [self] in received(value: $0, i) },
                                                 { [self] in received(completion: $0, i) }))
    }

    private func received(subscription: Subscription, _ i: Int) {
        lock.lock()
        if done { lock.unlock(); subscription.cancel(); return }
        upstreams[i] = subscription
        lock.unlock()
        subscription.request(.unlimited)
    }

    /// com a trava: devolve a combinação a mandar, se houver valor de todos e demanda
    private func takeLocked() -> Output? {
        guard !done, values.allSatisfy({ $0 != nil }) else { return nil }
        guard demand > .none else { pending = true; return nil }
        pending = false
        demand -= 1
        return combine(values.map { $0! })
    }

    private func send(_ out: Output) {
        lock.lock()
        let ds = downstream
        lock.unlock()
        guard let ds else { return }
        let more = ds.receive(out)
        if more > .none { request(more) }
    }

    private func received(value: Any, _ i: Int) {
        lock.lock()
        if done { lock.unlock(); return }
        values[i] = value
        let out = takeLocked()
        lock.unlock()
        if let out { send(out) }
    }

    private func received<F>(completion: Subscribers.Completion<F>, _ i: Int) {
        lock.lock()
        if done { lock.unlock(); return }
        switch completion {
        case .failure(let e):
            done = true
            let ds = downstream, subs = upstreams
            downstream = nil; upstreams = []
            lock.unlock()
            subs.forEach { $0?.cancel() }
            ds?.receive(completion: .failure(e as! Failure))
        case .finished:
            finished[i] = true
            // termina quando todos terminaram, ou quando um termina sem nunca ter mandado valor
            guard finished.allSatisfy({ $0 }) || values[i] == nil else { lock.unlock(); return }
            done = true
            let ds = downstream, subs = upstreams
            downstream = nil; upstreams = []
            lock.unlock()
            subs.forEach { $0?.cancel() }
            ds?.receive(completion: .finished)
        }
    }

    func request(_ d: Subscribers.Demand) {
        lock.lock()
        if done { lock.unlock(); return }
        demand += d
        let out = pending ? takeLocked() : nil
        lock.unlock()
        if let out { send(out) }
    }

    func cancel() {
        lock.lock()
        done = true
        let subs = upstreams
        downstream = nil; upstreams = []
        lock.unlock()
        subs.forEach { $0?.cancel() }
    }
}

extension Publishers {
    public struct CombineLatest<A: Publisher, B: Publisher>: Publisher where A.Failure == B.Failure {
        public typealias Output = (A.Output, B.Output)
        public typealias Failure = A.Failure
        public let a: A
        public let b: B
        public init(_ a: A, _ b: B) { self.a = a; self.b = b }
        public func receive<S: Subscriber>(subscriber: S) where S.Failure == Failure, S.Input == Output {
            let inner = _MOSCombineLatestInner<S>(subscriber, 2) { ($0[0] as! A.Output, $0[1] as! B.Output) }
            subscriber.receive(subscription: inner)
            inner.attach(a, 0); inner.attach(b, 1)
        }
    }

    public struct CombineLatest3<A: Publisher, B: Publisher, C: Publisher>: Publisher
        where A.Failure == B.Failure, B.Failure == C.Failure {
        public typealias Output = (A.Output, B.Output, C.Output)
        public typealias Failure = A.Failure
        public let a: A
        public let b: B
        public let c: C
        public init(_ a: A, _ b: B, _ c: C) { self.a = a; self.b = b; self.c = c }
        public func receive<S: Subscriber>(subscriber: S) where S.Failure == Failure, S.Input == Output {
            let inner = _MOSCombineLatestInner<S>(subscriber, 3) { ($0[0] as! A.Output, $0[1] as! B.Output, $0[2] as! C.Output) }
            subscriber.receive(subscription: inner)
            inner.attach(a, 0); inner.attach(b, 1); inner.attach(c, 2)
        }
    }

    public struct CombineLatest4<A: Publisher, B: Publisher, C: Publisher, D: Publisher>: Publisher
        where A.Failure == B.Failure, B.Failure == C.Failure, C.Failure == D.Failure {
        public typealias Output = (A.Output, B.Output, C.Output, D.Output)
        public typealias Failure = A.Failure
        public let a: A
        public let b: B
        public let c: C
        public let d: D
        public init(_ a: A, _ b: B, _ c: C, _ d: D) { self.a = a; self.b = b; self.c = c; self.d = d }
        public func receive<S: Subscriber>(subscriber: S) where S.Failure == Failure, S.Input == Output {
            let inner = _MOSCombineLatestInner<S>(subscriber, 4) {
                ($0[0] as! A.Output, $0[1] as! B.Output, $0[2] as! C.Output, $0[3] as! D.Output)
            }
            subscriber.receive(subscription: inner)
            inner.attach(a, 0); inner.attach(b, 1); inner.attach(c, 2); inner.attach(d, 3)
        }
    }
}

extension Publishers.CombineLatest: Equatable where A: Equatable, B: Equatable {
    public static func == (l: Self, r: Self) -> Bool { l.a == r.a && l.b == r.b }
}
extension Publishers.CombineLatest3: Equatable where A: Equatable, B: Equatable, C: Equatable {
    public static func == (l: Self, r: Self) -> Bool { l.a == r.a && l.b == r.b && l.c == r.c }
}
extension Publishers.CombineLatest4: Equatable where A: Equatable, B: Equatable, C: Equatable, D: Equatable {
    public static func == (l: Self, r: Self) -> Bool { l.a == r.a && l.b == r.b && l.c == r.c && l.d == r.d }
}

extension Publisher {
    public func combineLatest<P: Publisher>(_ other: P) -> Publishers.CombineLatest<Self, P> where Failure == P.Failure {
        Publishers.CombineLatest(self, other)
    }
    public func combineLatest<P: Publisher, T>(_ other: P, _ transform: @escaping (Output, P.Output) -> T)
        -> Publishers.Map<Publishers.CombineLatest<Self, P>, T> where Failure == P.Failure {
        Publishers.CombineLatest(self, other).map(transform)
    }
    public func combineLatest<P: Publisher, Q: Publisher>(_ publisher1: P, _ publisher2: Q)
        -> Publishers.CombineLatest3<Self, P, Q> where Failure == P.Failure, P.Failure == Q.Failure {
        Publishers.CombineLatest3(self, publisher1, publisher2)
    }
    public func combineLatest<P: Publisher, Q: Publisher, T>(_ publisher1: P, _ publisher2: Q,
                                                           _ transform: @escaping (Output, P.Output, Q.Output) -> T)
        -> Publishers.Map<Publishers.CombineLatest3<Self, P, Q>, T> where Failure == P.Failure, P.Failure == Q.Failure {
        Publishers.CombineLatest3(self, publisher1, publisher2).map(transform)
    }
    public func combineLatest<P: Publisher, Q: Publisher, R: Publisher>(_ publisher1: P, _ publisher2: Q, _ publisher3: R)
        -> Publishers.CombineLatest4<Self, P, Q, R> where Failure == P.Failure, P.Failure == Q.Failure, Q.Failure == R.Failure {
        Publishers.CombineLatest4(self, publisher1, publisher2, publisher3)
    }
    public func combineLatest<P: Publisher, Q: Publisher, R: Publisher, T>(
        _ publisher1: P, _ publisher2: Q, _ publisher3: R, _ transform: @escaping (Output, P.Output, Q.Output, R.Output) -> T)
        -> Publishers.Map<Publishers.CombineLatest4<Self, P, Q, R>, T>
        where Failure == P.Failure, P.Failure == Q.Failure, Q.Failure == R.Failure {
        Publishers.CombineLatest4(self, publisher1, publisher2, publisher3).map(transform)
    }
}

// MARK: - Merge

private final class _MOSMergeInner<Downstream: Subscriber>: Subscription, CustomStringConvertible {
    typealias Output = Downstream.Input
    typealias Failure = Downstream.Failure
    private let lock = UnfairLock.allocate()
    private let count: Int
    private var finishedCount = 0
    private var upstreams: [Subscription?]
    private var downstream: Downstream?
    private var demand = Subscribers.Demand.none
    private var buffer: [Output] = []
    private var done = false
    private var draining = false

    init(_ downstream: Downstream, _ count: Int) {
        self.downstream = downstream
        self.count = count
        upstreams = Array(repeating: nil, count: count)
    }
    deinit { lock.deallocate() }

    var description: String { "Merge" }

    func attach<P: Publisher>(_ p: P, _ i: Int) where P.Failure == Failure, P.Output == Output {
        p.subscribe(_MOSSide<P.Output, P.Failure>({ [self] in received(subscription: $0, i) },
                                                 { [self] in received(value: $0) },
                                                 { [self] in received(completion: $0) }))
    }

    /// nenhum publicador de cima: termina logo
    func finishIfEmpty() {
        if count == 0 { received(completion: Subscribers.Completion<Failure>.finished) }
    }

    private func received(subscription: Subscription, _ i: Int) {
        lock.lock()
        if done { lock.unlock(); subscription.cancel(); return }
        upstreams[i] = subscription
        lock.unlock()
        subscription.request(.unlimited)
    }

    private func received(value: Output) {
        lock.lock()
        if done { lock.unlock(); return }
        buffer.append(value)
        lock.unlock()
        drain()
    }

    /// manda o que está na fila enquanto houver demanda; depois, se todos terminaram, termina
    private func drain() {
        lock.lock()
        if draining { lock.unlock(); return }
        draining = true
        while !done, demand > .none, !buffer.isEmpty, let ds = downstream {
            let v = buffer.removeFirst()
            demand -= 1
            lock.unlock()
            let more = ds.receive(v)
            lock.lock()
            demand += more
        }
        draining = false
        if !done, buffer.isEmpty, finishedCount == count {
            done = true
            let ds = downstream
            downstream = nil; upstreams = []
            lock.unlock()
            ds?.receive(completion: .finished)
            return
        }
        lock.unlock()
    }

    private func received<F>(completion: Subscribers.Completion<F>) {
        lock.lock()
        if done { lock.unlock(); return }
        switch completion {
        case .failure(let e):
            done = true
            let ds = downstream, subs = upstreams
            downstream = nil; upstreams = []; buffer = []
            lock.unlock()
            subs.forEach { $0?.cancel() }
            ds?.receive(completion: .failure(e as! Failure))
        case .finished:
            finishedCount += 1
            lock.unlock()
            drain()
        }
    }

    func request(_ d: Subscribers.Demand) {
        lock.lock()
        if done { lock.unlock(); return }
        demand += d
        lock.unlock()
        drain()
    }

    func cancel() {
        lock.lock()
        done = true
        let subs = upstreams
        downstream = nil; upstreams = []; buffer = []
        lock.unlock()
        subs.forEach { $0?.cancel() }
    }
}

extension Publishers {
    public struct Merge<A: Publisher, B: Publisher>: Publisher where A.Failure == B.Failure, A.Output == B.Output {
        public typealias Output = A.Output
        public typealias Failure = A.Failure
        public let a: A
        public let b: B
        public init(_ a: A, _ b: B) { self.a = a; self.b = b }
        public func receive<S: Subscriber>(subscriber: S) where S.Failure == Failure, S.Input == Output {
            let inner = _MOSMergeInner<S>(subscriber, 2)
            subscriber.receive(subscription: inner)
            inner.attach(a, 0); inner.attach(b, 1)
        }
    }

    public struct Merge3<A: Publisher, B: Publisher, C: Publisher>: Publisher
        where A.Failure == B.Failure, A.Output == B.Output, B.Failure == C.Failure, B.Output == C.Output {
        public typealias Output = A.Output
        public typealias Failure = A.Failure
        public let a: A
        public let b: B
        public let c: C
        public init(_ a: A, _ b: B, _ c: C) { self.a = a; self.b = b; self.c = c }
        public func receive<S: Subscriber>(subscriber: S) where S.Failure == Failure, S.Input == Output {
            let inner = _MOSMergeInner<S>(subscriber, 3)
            subscriber.receive(subscription: inner)
            inner.attach(a, 0); inner.attach(b, 1); inner.attach(c, 2)
        }
    }

    public struct Merge4<A: Publisher, B: Publisher, C: Publisher, D: Publisher>: Publisher
        where A.Failure == B.Failure, A.Output == B.Output, B.Failure == C.Failure, B.Output == C.Output,
              C.Failure == D.Failure, C.Output == D.Output {
        public typealias Output = A.Output
        public typealias Failure = A.Failure
        public let a: A
        public let b: B
        public let c: C
        public let d: D
        public init(_ a: A, _ b: B, _ c: C, _ d: D) { self.a = a; self.b = b; self.c = c; self.d = d }
        public func receive<S: Subscriber>(subscriber: S) where S.Failure == Failure, S.Input == Output {
            let inner = _MOSMergeInner<S>(subscriber, 4)
            subscriber.receive(subscription: inner)
            inner.attach(a, 0); inner.attach(b, 1); inner.attach(c, 2); inner.attach(d, 3)
        }
    }

    public struct Merge5<A: Publisher, B: Publisher, C: Publisher, D: Publisher, E: Publisher>: Publisher
        where A.Failure == B.Failure, A.Output == B.Output, B.Failure == C.Failure, B.Output == C.Output,
              C.Failure == D.Failure, C.Output == D.Output, D.Failure == E.Failure, D.Output == E.Output {
        public typealias Output = A.Output
        public typealias Failure = A.Failure
        public let a: A
        public let b: B
        public let c: C
        public let d: D
        public let e: E
        public init(_ a: A, _ b: B, _ c: C, _ d: D, _ e: E) { self.a = a; self.b = b; self.c = c; self.d = d; self.e = e }
        public func receive<S: Subscriber>(subscriber: S) where S.Failure == Failure, S.Input == Output {
            let inner = _MOSMergeInner<S>(subscriber, 5)
            subscriber.receive(subscription: inner)
            inner.attach(a, 0); inner.attach(b, 1); inner.attach(c, 2); inner.attach(d, 3); inner.attach(e, 4)
        }
    }

    public struct Merge6<A: Publisher, B: Publisher, C: Publisher, D: Publisher, E: Publisher, F: Publisher>: Publisher
        where A.Failure == B.Failure, A.Output == B.Output, B.Failure == C.Failure, B.Output == C.Output,
              C.Failure == D.Failure, C.Output == D.Output, D.Failure == E.Failure, D.Output == E.Output,
              E.Failure == F.Failure, E.Output == F.Output {
        public typealias Output = A.Output
        public typealias Failure = A.Failure
        public let a: A
        public let b: B
        public let c: C
        public let d: D
        public let e: E
        public let f: F
        public init(_ a: A, _ b: B, _ c: C, _ d: D, _ e: E, _ f: F) {
            self.a = a; self.b = b; self.c = c; self.d = d; self.e = e; self.f = f
        }
        public func receive<S: Subscriber>(subscriber: S) where S.Failure == Failure, S.Input == Output {
            let inner = _MOSMergeInner<S>(subscriber, 6)
            subscriber.receive(subscription: inner)
            inner.attach(a, 0); inner.attach(b, 1); inner.attach(c, 2); inner.attach(d, 3); inner.attach(e, 4); inner.attach(f, 5)
        }
    }

    public struct Merge7<A: Publisher, B: Publisher, C: Publisher, D: Publisher, E: Publisher, F: Publisher, G: Publisher>: Publisher
        where A.Failure == B.Failure, A.Output == B.Output, B.Failure == C.Failure, B.Output == C.Output,
              C.Failure == D.Failure, C.Output == D.Output, D.Failure == E.Failure, D.Output == E.Output,
              E.Failure == F.Failure, E.Output == F.Output, F.Failure == G.Failure, F.Output == G.Output {
        public typealias Output = A.Output
        public typealias Failure = A.Failure
        public let a: A
        public let b: B
        public let c: C
        public let d: D
        public let e: E
        public let f: F
        public let g: G
        public init(_ a: A, _ b: B, _ c: C, _ d: D, _ e: E, _ f: F, _ g: G) {
            self.a = a; self.b = b; self.c = c; self.d = d; self.e = e; self.f = f; self.g = g
        }
        public func receive<S: Subscriber>(subscriber: S) where S.Failure == Failure, S.Input == Output {
            let inner = _MOSMergeInner<S>(subscriber, 7)
            subscriber.receive(subscription: inner)
            inner.attach(a, 0); inner.attach(b, 1); inner.attach(c, 2); inner.attach(d, 3); inner.attach(e, 4); inner.attach(f, 5)
            inner.attach(g, 6)
        }
    }

    public struct Merge8<A: Publisher, B: Publisher, C: Publisher, D: Publisher, E: Publisher, F: Publisher, G: Publisher,
                         H: Publisher>: Publisher
        where A.Failure == B.Failure, A.Output == B.Output, B.Failure == C.Failure, B.Output == C.Output,
              C.Failure == D.Failure, C.Output == D.Output, D.Failure == E.Failure, D.Output == E.Output,
              E.Failure == F.Failure, E.Output == F.Output, F.Failure == G.Failure, F.Output == G.Output,
              G.Failure == H.Failure, G.Output == H.Output {
        public typealias Output = A.Output
        public typealias Failure = A.Failure
        public let a: A
        public let b: B
        public let c: C
        public let d: D
        public let e: E
        public let f: F
        public let g: G
        public let h: H
        public init(_ a: A, _ b: B, _ c: C, _ d: D, _ e: E, _ f: F, _ g: G, _ h: H) {
            self.a = a; self.b = b; self.c = c; self.d = d; self.e = e; self.f = f; self.g = g; self.h = h
        }
        public func receive<S: Subscriber>(subscriber: S) where S.Failure == Failure, S.Input == Output {
            let inner = _MOSMergeInner<S>(subscriber, 8)
            subscriber.receive(subscription: inner)
            inner.attach(a, 0); inner.attach(b, 1); inner.attach(c, 2); inner.attach(d, 3); inner.attach(e, 4); inner.attach(f, 5)
            inner.attach(g, 6); inner.attach(h, 7)
        }
    }

    public struct MergeMany<Upstream: Publisher>: Publisher {
        public typealias Output = Upstream.Output
        public typealias Failure = Upstream.Failure
        public let publishers: [Upstream]
        public init(_ upstream: Upstream...) { publishers = upstream }
        public init<S: Swift.Sequence>(_ upstream: S) where Upstream == S.Element { publishers = Swift.Array(upstream) }
        public func receive<Sub: Subscriber>(subscriber: Sub) where Sub.Failure == Failure, Sub.Input == Output {
            let inner = _MOSMergeInner<Sub>(subscriber, publishers.count)
            subscriber.receive(subscription: inner)
            for (i, p) in publishers.enumerated() { inner.attach(p, i) }
            inner.finishIfEmpty()
        }
        public func merge(with other: Upstream) -> Publishers.MergeMany<Upstream> {
            Publishers.MergeMany(publishers + [other])
        }
    }
}

extension Publishers.Merge: Equatable where A: Equatable, B: Equatable {
    public static func == (l: Self, r: Self) -> Bool { l.a == r.a && l.b == r.b }
}
extension Publishers.MergeMany: Equatable where Upstream: Equatable {
    public static func == (l: Self, r: Self) -> Bool { l.publishers == r.publishers }
}

extension Publisher {
    /// mesmo tipo dos dois lados: MergeMany, como no Combine da Apple
    public func merge(with other: Self) -> Publishers.MergeMany<Self> {
        Publishers.MergeMany(self, other)
    }
    public func merge<P: Publisher>(with other: P) -> Publishers.Merge<Self, P> where Failure == P.Failure, Output == P.Output {
        Publishers.Merge(self, other)
    }
    public func merge<B: Publisher, C: Publisher>(with b: B, _ c: C) -> Publishers.Merge3<Self, B, C>
        where Failure == B.Failure, Output == B.Output, B.Failure == C.Failure, B.Output == C.Output {
        Publishers.Merge3(self, b, c)
    }
    public func merge<B: Publisher, C: Publisher, D: Publisher>(with b: B, _ c: C, _ d: D) -> Publishers.Merge4<Self, B, C, D>
        where Failure == B.Failure, Output == B.Output, B.Failure == C.Failure, B.Output == C.Output,
              C.Failure == D.Failure, C.Output == D.Output {
        Publishers.Merge4(self, b, c, d)
    }
    public func merge<B: Publisher, C: Publisher, D: Publisher, E: Publisher>(with b: B, _ c: C, _ d: D, _ e: E)
        -> Publishers.Merge5<Self, B, C, D, E>
        where Failure == B.Failure, Output == B.Output, B.Failure == C.Failure, B.Output == C.Output,
              C.Failure == D.Failure, C.Output == D.Output, D.Failure == E.Failure, D.Output == E.Output {
        Publishers.Merge5(self, b, c, d, e)
    }
    public func merge<B: Publisher, C: Publisher, D: Publisher, E: Publisher, F: Publisher>(
        with b: B, _ c: C, _ d: D, _ e: E, _ f: F) -> Publishers.Merge6<Self, B, C, D, E, F>
        where Failure == B.Failure, Output == B.Output, B.Failure == C.Failure, B.Output == C.Output,
              C.Failure == D.Failure, C.Output == D.Output, D.Failure == E.Failure, D.Output == E.Output,
              E.Failure == F.Failure, E.Output == F.Output {
        Publishers.Merge6(self, b, c, d, e, f)
    }
    public func merge<B: Publisher, C: Publisher, D: Publisher, E: Publisher, F: Publisher, G: Publisher>(
        with b: B, _ c: C, _ d: D, _ e: E, _ f: F, _ g: G) -> Publishers.Merge7<Self, B, C, D, E, F, G>
        where Failure == B.Failure, Output == B.Output, B.Failure == C.Failure, B.Output == C.Output,
              C.Failure == D.Failure, C.Output == D.Output, D.Failure == E.Failure, D.Output == E.Output,
              E.Failure == F.Failure, E.Output == F.Output, F.Failure == G.Failure, F.Output == G.Output {
        Publishers.Merge7(self, b, c, d, e, f, g)
    }
    public func merge<B: Publisher, C: Publisher, D: Publisher, E: Publisher, F: Publisher, G: Publisher, H: Publisher>(
        with b: B, _ c: C, _ d: D, _ e: E, _ f: F, _ g: G, _ h: H) -> Publishers.Merge8<Self, B, C, D, E, F, G, H>
        where Failure == B.Failure, Output == B.Output, B.Failure == C.Failure, B.Output == C.Output,
              C.Failure == D.Failure, C.Output == D.Output, D.Failure == E.Failure, D.Output == E.Output,
              E.Failure == F.Failure, E.Output == F.Output, F.Failure == G.Failure, F.Output == G.Output,
              G.Failure == H.Failure, G.Output == H.Output {
        Publishers.Merge8(self, b, c, d, e, f, g, h)
    }
}
