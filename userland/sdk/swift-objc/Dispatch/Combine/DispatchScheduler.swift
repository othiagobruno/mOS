// mOS — overlay Dispatch: DispatchQueue como Scheduler do Combine, como no overlay da Apple (lá também é o Dispatch que
// importa o Combine). Só entra quando o Combine.swiftmodule existe (tools/build-swift-combine-objc.ps1 recompila o
// Dispatch no fim).
import Combine

extension DispatchQueue: Scheduler {
    public struct SchedulerTimeType: Strideable, Codable, Hashable {
        public var dispatchTime: DispatchTime

        public init(_ time: DispatchTime) { dispatchTime = time }

        public func distance(to other: SchedulerTimeType) -> Stride {
            let a = dispatchTime.uptimeNanoseconds, b = other.dispatchTime.uptimeNanoseconds
            return b >= a ? Stride(.nanoseconds(Int(clamping: b - a))) : Stride(.nanoseconds(-Int(clamping: a - b)))
        }

        public func advanced(by n: Stride) -> SchedulerTimeType {
            SchedulerTimeType(dispatchTime + .nanoseconds(n.magnitude))
        }

        public init(from decoder: Decoder) throws {
            let c = try decoder.singleValueContainer()
            dispatchTime = DispatchTime(uptimeNanoseconds: try c.decode(UInt64.self))
        }

        public func encode(to encoder: Encoder) throws {
            var c = encoder.singleValueContainer()
            try c.encode(dispatchTime.uptimeNanoseconds)
        }

        public func hash(into hasher: inout Hasher) { hasher.combine(dispatchTime.rawValue) }

        public static func == (a: SchedulerTimeType, b: SchedulerTimeType) -> Bool { a.dispatchTime == b.dispatchTime }

        /// intervalo em nanossegundos
        public struct Stride: SchedulerTimeIntervalConvertible, Comparable, SignedNumeric, ExpressibleByFloatLiteral, Hashable, Codable {
            public typealias FloatLiteralType = Double
            public typealias IntegerLiteralType = Int
            public typealias Magnitude = Int

            public var magnitude: Int

            public var timeInterval: DispatchTimeInterval { .nanoseconds(magnitude) }

            public init(_ timeInterval: DispatchTimeInterval) {
                switch timeInterval {
                case .seconds(let s): magnitude = s &* 1_000_000_000
                case .milliseconds(let ms): magnitude = ms &* 1_000_000
                case .microseconds(let us): magnitude = us &* 1_000
                case .nanoseconds(let ns): magnitude = ns
                case .never: magnitude = .max
                }
            }

            /// segundos
            public init(integerLiteral value: Int) { self.init(.seconds(value)) }
            public init(floatLiteral value: Double) { magnitude = Int(value * 1_000_000_000) }
            public init?<T: BinaryInteger>(exactly source: T) {
                guard let v = Int(exactly: source) else { return nil }
                self.init(.nanoseconds(v))
            }

            public static func < (a: Stride, b: Stride) -> Bool { a.magnitude < b.magnitude }
            public static func == (a: Stride, b: Stride) -> Bool { a.magnitude == b.magnitude }
            public static func * (a: Stride, b: Stride) -> Stride { Stride(.nanoseconds(a.magnitude * b.magnitude)) }
            public static func + (a: Stride, b: Stride) -> Stride { Stride(.nanoseconds(a.magnitude + b.magnitude)) }
            public static func - (a: Stride, b: Stride) -> Stride { Stride(.nanoseconds(a.magnitude - b.magnitude)) }
            public static func *= (a: inout Stride, b: Stride) { a = a * b }
            public static func += (a: inout Stride, b: Stride) { a = a + b }
            public static func -= (a: inout Stride, b: Stride) { a = a - b }

            public static func seconds(_ s: Int) -> Stride { Stride(.seconds(s)) }
            public static func seconds(_ s: Double) -> Stride { Stride(floatLiteral: s) }
            public static func milliseconds(_ ms: Int) -> Stride { Stride(.milliseconds(ms)) }
            public static func microseconds(_ us: Int) -> Stride { Stride(.microseconds(us)) }
            public static func nanoseconds(_ ns: Int) -> Stride { Stride(.nanoseconds(ns)) }
        }
    }

    public struct SchedulerOptions {
        public var qos: DispatchQoS
        public var flags: DispatchWorkItemFlags
        public var group: DispatchGroup?
        public init(qos: DispatchQoS = .unspecified, flags: DispatchWorkItemFlags = [], group: DispatchGroup? = nil) {
            self.qos = qos; self.flags = flags; self.group = group
        }
    }

    public var now: SchedulerTimeType { SchedulerTimeType(DispatchTime.now()) }

    public var minimumTolerance: SchedulerTimeType.Stride { SchedulerTimeType.Stride(.nanoseconds(0)) }

    public func schedule(options: SchedulerOptions?, _ action: @escaping () -> Void) {
        let o = options ?? SchedulerOptions()
        async(group: o.group, qos: o.qos, flags: o.flags, execute: action)
    }

    public func schedule(after date: SchedulerTimeType, tolerance: SchedulerTimeType.Stride, options: SchedulerOptions?,
                         _ action: @escaping () -> Void) {
        let o = options ?? SchedulerOptions()
        asyncAfter(deadline: date.dispatchTime, qos: o.qos, flags: o.flags, execute: action)
    }

    public func schedule(after date: SchedulerTimeType, interval: SchedulerTimeType.Stride, tolerance: SchedulerTimeType.Stride,
                         options: SchedulerOptions?, _ action: @escaping () -> Void) -> Cancellable {
        let timer = DispatchSource.makeTimerSource(queue: self)
        timer.setEventHandler(handler: action)
        timer.schedule(deadline: date.dispatchTime, repeating: interval.timeInterval, leeway: tolerance.timeInterval)
        timer.resume()
        return AnyCancellable { timer.cancel() }
    }
}
