// mOS — o runtime de concorrência não entra no stdlib. Estes tipos cobrem o `.task` e o
// AsyncStream que o OpenSwiftUI declara; o `async`/`await` dos fontes é retirado na preparação.
#if !OPENSWIFTUI_SWIFTUI_RENDERER

public struct TaskPriority: Hashable, Sendable {
    public var rawValue: UInt8
    public init(rawValue: UInt8) { self.rawValue = rawValue }
    public static let high = TaskPriority(rawValue: 0)
    public static let medium = TaskPriority(rawValue: 1)
    public static let low = TaskPriority(rawValue: 2)
    public static let userInitiated = TaskPriority(rawValue: 3)
    public static let utility = TaskPriority(rawValue: 4)
    public static let background = TaskPriority(rawValue: 5)
}

public final class Task<Success, Failure: Error>: @unchecked Sendable {
    fileprivate init(operation: @escaping () -> Success) { _ = operation() }
    public func cancel() {}
}
extension Task where Failure == Never {
    public convenience init(priority: TaskPriority? = nil, operation: @escaping () -> Success) {
        self.init(operation: operation)
        _ = priority
    }
    public static func detached(priority: TaskPriority? = nil, operation: @escaping () -> Success) -> Task<Success, Never> {
        Task<Success, Never>(priority: priority, operation: operation)
    }
}

public protocol AsyncIteratorProtocol {
    associatedtype Element
    mutating func next() -> Element?
}

public protocol AsyncSequence<Element> {
    associatedtype Element
    associatedtype AsyncIterator: AsyncIteratorProtocol where AsyncIterator.Element == Element
    func makeAsyncIterator() -> AsyncIterator
}

public struct AsyncStream<Element>: AsyncSequence {
    public final class Continuation {
        public enum Termination { case finished, cancelled }
        public func yield(_ value: Element) { _ = value }
        public var onTermination: ((Termination) -> Void)?
    }
    public struct Iterator: AsyncIteratorProtocol {
        public mutating func next() -> Element? { nil }
    }
    public init(_ elementType: Element.Type = Element.self, bufferingPolicy: Void? = nil, _ build: (Continuation) -> Void) {
        _ = elementType
        _ = bufferingPolicy
        build(Continuation())
    }
    public func makeAsyncIterator() -> Iterator { Iterator() }
}

@_silgen_name("_mos_pd")
func _mosProtocolPointer(_ which: Int32) -> UnsafeRawPointer

func _observableObjectProtocolDescriptor() -> UnsafeRawPointer { _mosProtocolPointer(0) }
func _commandsProtocolDescriptor() -> UnsafeRawPointer { _mosProtocolPointer(1) }
func _sceneProtocolDescriptor() -> UnsafeRawPointer { _mosProtocolPointer(2) }
func _previewProviderProtocolDescriptor() -> UnsafeRawPointer { _mosProtocolPointer(3) }

#endif
