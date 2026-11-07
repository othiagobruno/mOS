// mOS — Timer.publish do Combine. O OpenCombineFoundation upstream observa KVO e URLSession,
// que o Foundation do mOS não oferece. O ProgressView sem Objective-C só usa este publisher.
public import Foundation
public import OpenCombine

extension Timer {
    public static func publish(
        every interval: TimeInterval,
        tolerance: TimeInterval? = nil,
        on runLoop: RunLoop,
        in mode: RunLoop.Mode,
        options: Void? = nil
    ) -> TimerPublisher {
        TimerPublisher(interval: interval, runLoop: runLoop, mode: mode)
    }

    public final class TimerPublisher: ConnectablePublisher {
        public typealias Output = Date
        public typealias Failure = Never
        public let interval: TimeInterval
        public let runLoop: RunLoop
        public let mode: RunLoop.Mode
        private var subscribers: [AnySubscriber<Date, Never>] = []
        private var timer: Foundation.Timer?

        public init(interval: TimeInterval, runLoop: RunLoop, mode: RunLoop.Mode) {
            self.interval = interval
            self.runLoop = runLoop
            self.mode = mode
        }

        public func receive<Downstream: Subscriber>(subscriber: Downstream)
            where Downstream.Input == Date, Downstream.Failure == Never
        {
            subscribers.append(AnySubscriber(subscriber))
            subscriber.receive(subscription: Subscriptions.empty)
        }

        public func connect() -> any Cancellable {
            let timer = Foundation.Timer(timeInterval: interval, repeats: true) { [weak self] _ in
                guard let self else { return }
                let now = Date()
                for subscriber in self.subscribers { _ = subscriber.receive(now) }
            }
            runLoop.add(timer, forMode: mode)
            self.timer = timer
            return AnyCancellable { [weak self] in
                timer.invalidate()
                self?.timer = nil
            }
        }
    }
}
