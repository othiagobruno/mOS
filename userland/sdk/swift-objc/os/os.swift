// mOS — overlay Swift "os" da variante com interop ObjC (docs/swift-objc-runtime.md, fase 3), com os nomes do SDK
// da Apple: OSLog (= OS_os_log), os_log(_:dso:log:_:_:) e os tipos de OSLogType. Como no libSystem do mOS
// (darwin.c), só erro e falha são escritos (no stderr, que vai para o Console do mOS); info/debug/padrão somem.

@_exported import os
import Darwin

public typealias OSLog = OS_os_log

extension OSLogType {
    public static var `default`: OSLogType { OSLogType(rawValue: 0x00) }
    public static var info: OSLogType { OSLogType(rawValue: 0x01) }
    public static var debug: OSLogType { OSLogType(rawValue: 0x02) }
    public static var error: OSLogType { OSLogType(rawValue: 0x10) }
    public static var fault: OSLogType { OSLogType(rawValue: 0x11) }
}

extension OS_os_log {
    public static var `default`: OSLog { unsafeBitCast(_mos_os_log_default(), to: OSLog.self) }
    public static var disabled: OSLog { unsafeBitCast(_mos_os_log_disabled(), to: OSLog.self) }
}

/// OSLog(subsystem:category:) do SDK; aqui uma função, porque a classe só existe para o runtime
public func _mosOSLog(subsystem: String, category: String) -> OSLog { os_log_create(subsystem, category) }

/// Formata no estilo do os_log: %{public}/%{private} somem; cada especificador consome um argumento,
/// impresso pela descrição Swift dele (cobre %d %s %@ %f %ld %lu %x...).
@usableFromInline
internal func _mosFormat(_ format: StaticString, _ args: [CVarArg]) -> String {
    var out = ""
    var it = args.makeIterator()
    let bytes = UnsafeBufferPointer(start: format.utf8Start, count: format.utf8CodeUnitCount)
    var i = 0
    var lit: [UInt8] = []
    func flush() { if !lit.isEmpty { out += String(decoding: lit, as: UTF8.self); lit.removeAll() } }
    while i < bytes.count {
        let c = bytes[i]
        if c != UInt8(ascii: "%") { lit.append(c); i += 1; continue }
        i += 1
        if i < bytes.count && bytes[i] == UInt8(ascii: "%") { lit.append(c); i += 1; continue }
        if i < bytes.count && bytes[i] == UInt8(ascii: "{") {
            while i < bytes.count && bytes[i] != UInt8(ascii: "}") { i += 1 }
            i += 1
        }
        // flags, largura, precisão, tamanho, conversão
        while i < bytes.count {
            let s = bytes[i]
            i += 1
            let isConv = (s >= UInt8(ascii: "a") && s <= UInt8(ascii: "z") && !"hlqjztL".utf8.contains(s)) || s == UInt8(ascii: "@") || s == UInt8(ascii: "X") || s == UInt8(ascii: "E") || s == UInt8(ascii: "G")
            if isConv { break }
        }
        flush()
        if let a = it.next() { out += "\(a)" }
    }
    flush()
    return out
}

public func os_log(_ type: OSLogType, dso: UnsafeRawPointer = #dsohandle, log: OSLog = .default,
                   _ message: StaticString, _ args: CVarArg...) {
    guard type == .error || type == .fault else { return }
    let text = _mosFormat(message, args)
    fputs(text + "\n", __stderrp)
}

public func os_log(_ message: StaticString, dso: UnsafeRawPointer? = #dsohandle, log: OSLog = .default,
                   type: OSLogType = .default, _ args: CVarArg...) {
    guard type == .error || type == .fault else { return }
    fputs(_mosFormat(message, args) + "\n", __stderrp)
}
