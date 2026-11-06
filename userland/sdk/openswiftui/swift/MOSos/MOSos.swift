// mOS SDK — MOSos: o pedaço do módulo `os` (Logger/os_log) que o swift-foundation usa no caminho Darwin. O build do
// Foundation renomeia `import os` para `import MOSos` (como Darwin -> MOSDarwin). As mensagens vão para o stderr.

@_silgen_name("write") private func _mos_write(_ fd: Int32, _ buf: UnsafeRawPointer, _ n: Int) -> Int

@usableFromInline func _mosLogWrite(_ level: String, _ category: String, _ message: String) {
    var line = "[\(level)]"
    if !category.isEmpty { line += " \(category):" }
    line += " \(message)\n"
    var l = line
    l.withUTF8 { _ = _mos_write(2, $0.baseAddress!, $0.count) }
}

public enum OSLogType: Sendable {
    case `default`, info, debug, error, fault
    var _name: String {
        switch self { case .default: "default"; case .info: "info"; case .debug: "debug"; case .error: "error"; case .fault: "fault" }
    }
}

public final class OSLog: @unchecked Sendable {
    public let subsystem: String
    public let category: String
    public init(subsystem: String, category: String) { self.subsystem = subsystem; self.category = category }
    public static let `default` = OSLog(subsystem: "", category: "")
    public static let disabled = OSLog(subsystem: "", category: "")
}

public struct OSLogPrivacy: Sendable, Equatable {
    let _raw: Int
    public static let `public` = OSLogPrivacy(_raw: 0)
    public static let `private` = OSLogPrivacy(_raw: 1)
    public static let sensitive = OSLogPrivacy(_raw: 2)
    public static let auto = OSLogPrivacy(_raw: 3)
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

public struct Logger: Sendable {
    let _category: String
    public init(subsystem: String, category: String) { _category = category }
    public init(_ log: OSLog) { _category = log.category }
    public init() { _category = "" }

    public func log(level: OSLogType = .default, _ message: OSLogMessage) { _mosLogWrite(level._name, _category, message._text) }
    public func log(_ message: OSLogMessage) { log(level: .default, message) }
    public func trace(_ message: OSLogMessage) {}
    public func debug(_ message: OSLogMessage) {}
    public func info(_ message: OSLogMessage) {}
    public func notice(_ message: OSLogMessage) { log(level: .default, message) }
    public func warning(_ message: OSLogMessage) { log(level: .error, message) }
    public func error(_ message: OSLogMessage) { log(level: .error, message) }
    public func critical(_ message: OSLogMessage) { log(level: .fault, message) }
    public func fault(_ message: OSLogMessage) { log(level: .fault, message) }
}

public func os_log(_ type: OSLogType, log: OSLog = .default, _ message: OSLogMessage) {
    _mosLogWrite(type._name, log.category, message._text)
}
public func os_log(_ message: OSLogMessage) { _mosLogWrite("default", "", message._text) }
