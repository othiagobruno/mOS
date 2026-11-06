// mOS SDK — String: o que o Foundation da Apple tem e o FoundationEssentials (swift-foundation) não: cString(using:),
// String(bytes:encoding:), String(cString:encoding:) e String(format:). Data e String.Encoding vêm do Essentials.

extension String {
    /// Bytes terminados em NUL na codificação pedida (UTF-8/ASCII/Latin-1; nil se não representável).
    public func cString(using encoding: Encoding) -> [CChar]? {
        switch encoding {
        case .utf8: return utf8.map { CChar(bitPattern: $0) } + [0]
        case .ascii:
            var r: [CChar] = []
            for u in unicodeScalars { guard u.value < 0x80 else { return nil }; r.append(CChar(u.value)) }
            return r + [0]
        case .isoLatin1:
            var r: [CChar] = []
            for u in unicodeScalars { guard u.value < 0x100 else { return nil }; r.append(CChar(bitPattern: UInt8(u.value))) }
            return r + [0]
        default: return nil
        }
    }
    public init?(bytes: some Sequence<UInt8>, encoding: Encoding) { self.init(data: Data(bytes), encoding: encoding) }
    public init?(cString: UnsafePointer<CChar>, encoding: Encoding) {
        self.init(data: Data(bytes: cString, count: strlen(cString)), encoding: encoding)
    }
    public var utf8CString_mos: ContiguousArray<CChar> { utf8CString }
}

@_silgen_name("strlen") private func strlen(_ s: UnsafePointer<CChar>) -> Int

// MARK: - String(format:) com %@ e argumentos posicionais

extension String {
    /// Formata como o NSString: printf do libSystem para cada especificação, e `%@` com a descrição do objeto
    /// (String, NSObject, CustomStringConvertible). Aceita posições (`%1$@`, `%2$d`).
    public init(format: String, _ arguments: CVarArg...) { self.init(format: format, arguments: arguments) }
    public init(format: String, locale: Locale?, _ arguments: CVarArg...) { self.init(format: format, arguments: arguments) }
    public init(format: String, locale: Locale?, arguments: [CVarArg]) { self.init(format: format, arguments: arguments) }
    public init(format: String, arguments: [CVarArg]) {
        var out = ""
        let f = Array(format.unicodeScalars)
        var i = 0, next = 0
        while i < f.count {
            let c = f[i]
            guard c == "%" else { out.unicodeScalars.append(c); i += 1; continue }
            if i + 1 < f.count, f[i + 1] == "%" { out += "%"; i += 2; continue }
            // %[n$][flags][largura][.precisão][modificadores]conversão
            var j = i + 1
            var spec = "%"
            var position: Int? = nil
            var k = j, n = 0
            while k < f.count, let d = Int(String(f[k])) { n = n * 10 + d; k += 1 }
            if k < f.count, f[k] == "$", k > j { position = n - 1; j = k + 1 }
            while j < f.count, !"diouxXeEfFgGaAcCsSpn@".unicodeScalars.contains(f[j]) {
                spec.unicodeScalars.append(f[j]); j += 1
            }
            guard j < f.count else { out += String(String.UnicodeScalarView(f[i...])); break }
            let conv = f[j]
            let argIndex = position ?? next
            if position == nil { next += 1 }
            let arg: CVarArg? = argIndex < arguments.count ? arguments[argIndex] : nil
            if conv == "@" {
                switch arg {
                case let s as String: out += s
                case let s as Substring: out += s
                case let o as NSObject: out += o.description
                case let d as CustomStringConvertible: out += d.description
                case nil: out += "(null)"
                default: out += String(describing: arg!)
                }
            } else if let arg {
                spec.unicodeScalars.append(conv)
                out += String._mosFormatOne(spec, arg)
            }
            i = j + 1
        }
        self = out
    }

    static func _mosFormatOne(_ spec: String, _ arg: CVarArg) -> String {
        withVaList([arg]) { va in
            spec.withCString { f in
                var buffer = [CChar](repeating: 0, count: 128)
                let n = Int(vsnprintf(&buffer, buffer.count, f, va))
                if n < buffer.count { return String(cString: buffer) }
                return withVaList([arg]) { va2 in
                    var big = [CChar](repeating: 0, count: n + 1)
                    _ = vsnprintf(&big, big.count, f, va2)
                    return String(cString: big)
                }
            }
        }
    }
}

// String/Substring/NSObject como argumento C (%s recebe um ponteiro de C string com vida longa; %@ usa o próprio valor)
extension String: CVarArg {
    public var _cVarArgEncoding: [Int] { [Int(bitPattern: _mosPersistentCString(self))] }
}
extension Substring: CVarArg {
    public var _cVarArgEncoding: [Int] { [Int(bitPattern: _mosPersistentCString(String(self)))] }
}
extension NSObject: CVarArg {
    public var _cVarArgEncoding: [Int] { [Int(bitPattern: _mosPersistentCString(description))] }
}

// MARK: - Error.localizedDescription

extension Error {
    /// Como o NSError: errorDescription do LocalizedError, senão a descrição do valor.
    public var localizedDescription: String {
        if let l = self as? LocalizedError, let d = l.errorDescription { return d }
        return "The operation couldn’t be completed. (\(String(reflecting: self)))"
    }
}