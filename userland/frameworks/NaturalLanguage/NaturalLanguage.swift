// mOS — overlay Swift do NaturalLanguage (/usr/lib/swift/libswiftNaturalLanguage.dylib, como no macOS):
// languageHypotheses(withMaximum:) sobre o -languageHypothesesWithMaximum: do NaturalLanguage.m do mOS.
// tools/build-frameworks-objc.sh (arquivo swift-overlay-path)

@_exported import NaturalLanguage

// O NLLanguage importado aqui embrulha o NSString sem a ponte para String (o overlay não carrega a Foundation), então
// o Hashable vem daqui, calculado sobre o texto como String: o mesmo hash do Hashable que o importador da Apple
// sintetiza no cliente (rawValue: String), para o dicionário devolvido ser consultado pelos dois lados.
extension NLLanguage: @retroactive Hashable {
    @inline(__always) internal var _mosText: String { String(cString: unsafeBitCast(self, to: NSString.self).utf8String()) }
    public func hash(into hasher: inout Hasher) { hasher.combine(_mosText) }
    public static func == (a: NLLanguage, b: NLLanguage) -> Bool { a._mosText == b._mosText }
}

extension NLLanguageRecognizer {
    public func languageHypotheses(withMaximum maxHypotheses: Int) -> [NLLanguage: Double] {
        var out: [NLLanguage: Double] = [:]
        guard maxHypotheses > 0, let d = __languageHypotheses(withMaximum: UInt(maxHypotheses)) else { return out }
        let keys = d.allKeys()
        for i in 0..<Int(keys.count()) {
            let k = keys.object(at: UInt(i))
            guard let v = d.object(forKey: k) as? NSNumber else { continue }
            // NLLanguage é o NSString embrulhado (um ponteiro só)
            out[unsafeBitCast(k as AnyObject, to: NLLanguage.self)] = v.doubleValue()
        }
        return out
    }
}
