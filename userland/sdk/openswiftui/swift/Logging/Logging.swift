// mOS SDK — Logging: a API do swift-log (github.com/apple/swift-log) que o OpenSwiftUI usa com OPENSWIFTUI_SWIFT_LOG,
// em Swift puro e sem dependências (o swift-log original precisa de Glibc/Darwin para os locks e o stdout).
// Mensagens a partir do nível do logger (padrão .info) vão para o stdout do processo (Console/Terminal do mOS).

public struct Logger: Sendable {
    public var label: String
    public var logLevel: Logger.Level
    public var metadata: Logger.Metadata

    public init(label: String) {
        self.label = label
        self.logLevel = .info
        self.metadata = [:]
    }

    public subscript(metadataKey key: String) -> Logger.Metadata.Value? {
        get { metadata[key] }
        set { metadata[key] = newValue }
    }

    @inlinable
    public func log(level: Logger.Level, _ message: @autoclosure () -> Logger.Message,
                    metadata: @autoclosure () -> Logger.Metadata? = nil, source: @autoclosure () -> String? = nil,
                    file: String = #fileID, function: String = #function, line: UInt = #line) {
        guard level >= logLevel else { return }
        _emit(level: level, message: message(), metadata: metadata())
    }

    @usableFromInline
    func _emit(level: Logger.Level, message: Logger.Message, metadata extra: Logger.Metadata?) {
        var all = metadata
        if let extra { all.merge(extra) { _, new in new } }
        let md = all.isEmpty ? "" : " " + all.keys.sorted().map { "\($0)=\(all[$0]!)" }.joined(separator: " ")
        print("\(level.rawValue) \(label):\(md) \(message)")
    }

    @inlinable public func trace(_ message: @autoclosure () -> Logger.Message, metadata: @autoclosure () -> Logger.Metadata? = nil,
                                 file: String = #fileID, function: String = #function, line: UInt = #line) {
        log(level: .trace, message(), metadata: metadata(), file: file, function: function, line: line)
    }
    @inlinable public func debug(_ message: @autoclosure () -> Logger.Message, metadata: @autoclosure () -> Logger.Metadata? = nil,
                                 file: String = #fileID, function: String = #function, line: UInt = #line) {
        log(level: .debug, message(), metadata: metadata(), file: file, function: function, line: line)
    }
    @inlinable public func info(_ message: @autoclosure () -> Logger.Message, metadata: @autoclosure () -> Logger.Metadata? = nil,
                                file: String = #fileID, function: String = #function, line: UInt = #line) {
        log(level: .info, message(), metadata: metadata(), file: file, function: function, line: line)
    }
    @inlinable public func notice(_ message: @autoclosure () -> Logger.Message, metadata: @autoclosure () -> Logger.Metadata? = nil,
                                  file: String = #fileID, function: String = #function, line: UInt = #line) {
        log(level: .notice, message(), metadata: metadata(), file: file, function: function, line: line)
    }
    @inlinable public func warning(_ message: @autoclosure () -> Logger.Message, metadata: @autoclosure () -> Logger.Metadata? = nil,
                                   file: String = #fileID, function: String = #function, line: UInt = #line) {
        log(level: .warning, message(), metadata: metadata(), file: file, function: function, line: line)
    }
    @inlinable public func error(_ message: @autoclosure () -> Logger.Message, metadata: @autoclosure () -> Logger.Metadata? = nil,
                                 file: String = #fileID, function: String = #function, line: UInt = #line) {
        log(level: .error, message(), metadata: metadata(), file: file, function: function, line: line)
    }
    @inlinable public func critical(_ message: @autoclosure () -> Logger.Message, metadata: @autoclosure () -> Logger.Metadata? = nil,
                                    file: String = #fileID, function: String = #function, line: UInt = #line) {
        log(level: .critical, message(), metadata: metadata(), file: file, function: function, line: line)
    }
}

extension Logger {
    public enum Level: String, Codable, CaseIterable, Sendable, Comparable {
        case trace, debug, info, notice, warning, error, critical
        private var order: Int {
            switch self { case .trace: 0; case .debug: 1; case .info: 2; case .notice: 3; case .warning: 4; case .error: 5; case .critical: 6 }
        }
        public static func < (l: Level, r: Level) -> Bool { l.order < r.order }
    }

    public struct Message: ExpressibleByStringLiteral, ExpressibleByStringInterpolation, CustomStringConvertible, Equatable, Sendable {
        private var value: String
        public init(stringLiteral value: String) { self.value = value }
        public var description: String { value }
    }

    public typealias Metadata = [String: MetadataValue]

    public enum MetadataValue: Sendable, Equatable, CustomStringConvertible, ExpressibleByStringLiteral,
        ExpressibleByStringInterpolation, ExpressibleByDictionaryLiteral, ExpressibleByArrayLiteral {
        case string(String)
        case stringConvertible(any CustomStringConvertible & Sendable)
        case dictionary(Metadata)
        case array([Metadata.Value])

        public init(stringLiteral value: String) { self = .string(value) }
        public init(dictionaryLiteral elements: (String, Metadata.Value)...) { self = .dictionary(Dictionary(uniqueKeysWithValues: elements)) }
        public init(arrayLiteral elements: Metadata.Value...) { self = .array(elements) }

        public var description: String {
            switch self {
            case .string(let s): s
            case .stringConvertible(let c): c.description
            case .dictionary(let d): d.description
            case .array(let a): a.description
            }
        }
        public static func == (l: MetadataValue, r: MetadataValue) -> Bool { l.description == r.description }
    }
}

extension Logger.Metadata {
    public typealias Value = Logger.MetadataValue
}
