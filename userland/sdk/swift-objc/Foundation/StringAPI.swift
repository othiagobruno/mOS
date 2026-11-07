// mOS — overlay Foundation (marco C): a API de NSString sobre StringProtocol (o NSStringAPI.swift da Apple), com os nomes
// e as assinaturas da Apple. Implementada em Swift sobre os escalares/caracteres da própria String, sem passar pelo
// NSString do mOS (que não tem as variantes com opções/intervalo/locale). O locale é ignorado: o mOS é "C"/pt-BR.
// Opções suportadas: caseInsensitive, diacriticInsensitive, literal, backwards, anchored, numeric (compare) e
// regularExpression (pelo Regex da stdlib).

import Darwin

// MARK: - comparação de caracteres conforme as opções

internal func _mosFold(_ c: Character, _ o: String.CompareOptions) -> String {
    var s = String(c)
    if o.contains(.caseInsensitive) { s = s.lowercased() }
    if o.contains(.diacriticInsensitive) { s = String(String.UnicodeScalarView(s.unicodeScalars.filter { !_mosIsCombining($0) })) }
    return s
}

internal func _mosIsCombining(_ u: Unicode.Scalar) -> Bool {
    switch u.value {
    case 0x0300...0x036F, 0x1AB0...0x1AFF, 0x1DC0...0x1DFF, 0x20D0...0x20FF, 0xFE20...0xFE2F: return true
    default: return false
    }
}

/// tira os diacríticos da forma decomposta mais comum (á -> a): decompõe os latinos pré-compostos usuais
internal func _mosStripDiacritics(_ s: String) -> String {
    var out = String.UnicodeScalarView()
    for u in s.unicodeScalars {
        if _mosIsCombining(u) { continue }
        if let b = _mosLatinBase(u) { out.append(b) } else { out.append(u) }
    }
    return String(out)
}

internal func _mosLatinBase(_ u: Unicode.Scalar) -> Unicode.Scalar? {
    let table: [(ClosedRange<UInt32>, String)] = [
        (0xC0...0xC5, "A"), (0xC7...0xC7, "C"), (0xC8...0xCB, "E"), (0xCC...0xCF, "I"), (0xD1...0xD1, "N"),
        (0xD2...0xD6, "O"), (0xD9...0xDC, "U"), (0xDD...0xDD, "Y"),
        (0xE0...0xE5, "a"), (0xE7...0xE7, "c"), (0xE8...0xEB, "e"), (0xEC...0xEF, "i"), (0xF1...0xF1, "n"),
        (0xF2...0xF6, "o"), (0xF9...0xFC, "u"), (0xFD...0xFD, "y"), (0xFF...0xFF, "y"),
    ]
    for (r, b) in table where r.contains(u.value) { return b.unicodeScalars.first }
    return nil
}

extension StringProtocol {
    internal func _mosNormalized(_ o: String.CompareOptions) -> String {
        var s = String(self)
        if o.contains(.diacriticInsensitive) { s = _mosStripDiacritics(s) }
        if o.contains(.caseInsensitive) { s = s.lowercased() }
        return s
    }

    /// busca de `needle` em self[range] com as opções; o resultado está nos índices de self
    internal func _mosFind(_ needle: String, _ o: String.CompareOptions, _ range: Range<Index>?) -> Range<Index>? {
        let base = self[range ?? startIndex..<endIndex]
        if needle.isEmpty { return nil }
        if o.contains(.regularExpression) {
            guard let re = try? Regex(needle) else { return nil }
            let opts = o.contains(.caseInsensitive) ? re.ignoresCase() : re
            if o.contains(.anchored) {
                guard let m = try? opts.prefixMatch(in: String(base)) else { return nil }
                let off = String(base).distance(from: String(base).startIndex, to: m.range.upperBound)
                return base.startIndex..<base.index(base.startIndex, offsetBy: off)
            }
            guard let m = try? opts.firstMatch(in: String(base)) else { return nil }
            let s = String(base)
            let lo = s.distance(from: s.startIndex, to: m.range.lowerBound), hi = s.distance(from: s.startIndex, to: m.range.upperBound)
            return base.index(base.startIndex, offsetBy: lo)..<base.index(base.startIndex, offsetBy: hi)
        }
        let n = needle.map { _mosFold($0, o) }
        let chars = Array(base.indices)
        if chars.count < n.count { return nil }
        func matches(at k: Int) -> Bool {
            for j in 0..<n.count where _mosFold(base[chars[k + j]], o) != n[j] { return false }
            return true
        }
        let last = chars.count - n.count
        let candidates: [Int]
        if o.contains(.anchored) { candidates = o.contains(.backwards) ? [last] : [0] }
        else { candidates = o.contains(.backwards) ? Array((0...last).reversed()) : Array(0...last) }
        for k in candidates where matches(at: k) {
            let end = k + n.count < chars.count ? chars[k + n.count] : base.endIndex
            return chars[k]..<end
        }
        return nil
    }
}

// MARK: - API

extension StringProtocol {
    public func range<T: StringProtocol>(of aString: T, options mask: String.CompareOptions = [], range searchRange: Range<Index>? = nil,
                                         locale: Locale? = nil) -> Range<Index>? {
        _mosFind(String(aString), mask, searchRange)
    }

    public func contains<T: StringProtocol>(_ other: T) -> Bool {
        other.isEmpty ? false : _mosFind(String(other), [], nil) != nil
    }

    public func localizedCaseInsensitiveContains<T: StringProtocol>(_ other: T) -> Bool {
        _mosFind(String(other), [.caseInsensitive], nil) != nil
    }

    public func compare<T: StringProtocol>(_ aString: T, options mask: String.CompareOptions = [], range: Range<Index>? = nil,
                                           locale: Locale?) -> ComparisonResult {
        let a = range.map { String(self[$0]) } ?? String(self)
        return _mosCompare(a, String(aString), mask)
    }

    public func caseInsensitiveCompare<T: StringProtocol>(_ aString: T) -> ComparisonResult {
        _mosCompare(String(self), String(aString), [.caseInsensitive])
    }

    public func localizedCompare<T: StringProtocol>(_ aString: T) -> ComparisonResult {
        _mosCompare(String(self), String(aString), [])
    }

    public func localizedCaseInsensitiveCompare<T: StringProtocol>(_ aString: T) -> ComparisonResult {
        _mosCompare(String(self), String(aString), [.caseInsensitive])
    }

    /// como o Finder: sem diferenciar maiúsculas e com números por valor
    public func localizedStandardCompare<T: StringProtocol>(_ string: T) -> ComparisonResult {
        _mosCompare(String(self), String(string), [.caseInsensitive, .numeric])
    }

    public func trimmingCharacters(in set: CharacterSet) -> String {
        let s = unicodeScalars
        var lo = s.startIndex, hi = s.endIndex
        while lo < hi && set.contains(s[lo]) { lo = s.index(after: lo) }
        while hi > lo && set.contains(s[s.index(before: hi)]) { hi = s.index(before: hi) }
        var v = String.UnicodeScalarView(); v.append(contentsOf: s[lo..<hi]); return String(v)
    }

    public func components(separatedBy separator: CharacterSet) -> [String] {
        var out: [String] = []
        var cur = String.UnicodeScalarView()
        for u in unicodeScalars {
            if separator.contains(u) { out.append(String(cur)); cur = String.UnicodeScalarView() } else { cur.append(u) }
        }
        out.append(String(cur))
        return out
    }

    public func rangeOfCharacter(from aSet: CharacterSet, options mask: String.CompareOptions = [],
                                 range aRange: Range<Index>? = nil) -> Range<Index>? {
        let r = aRange ?? startIndex..<endIndex
        let s = unicodeScalars
        if mask.contains(.backwards) {
            var i = r.upperBound
            while i > r.lowerBound {
                let p = s.index(before: i)
                if aSet.contains(s[p]) { return p..<i }
                if mask.contains(.anchored) { return nil }
                i = p
            }
            return nil
        }
        var i = r.lowerBound
        while i < r.upperBound {
            let n = s.index(after: i)
            if aSet.contains(s[i]) { return i..<n }
            if mask.contains(.anchored) { return nil }
            i = n
        }
        return nil
    }

    public func replacingOccurrences<Target: StringProtocol, Replacement: StringProtocol>(
        of target: Target, with replacement: Replacement, options: String.CompareOptions = [], range searchRange: Range<Index>? = nil
    ) -> String {
        let t = String(target)
        if t.isEmpty { return String(self) }
        var out = ""
        let lim = searchRange ?? startIndex..<endIndex
        out += self[startIndex..<lim.lowerBound]
        var pos = lim.lowerBound
        let fwd = options.subtracting([.backwards])
        while pos < lim.upperBound, let r = _mosFind(t, fwd, pos..<lim.upperBound) {
            out += self[pos..<r.lowerBound]
            out += replacement
            pos = r.upperBound
            if r.isEmpty { break }
            if options.contains(.anchored) { break }
        }
        out += self[pos..<endIndex]
        return out
    }

    public func replacingCharacters<T: StringProtocol, R: RangeExpression>(in range: R, with replacement: T) -> String
        where R.Bound == Index {
        let r = range.relative(to: self)
        return String(self[startIndex..<r.lowerBound]) + String(replacement) + String(self[r.upperBound..<endIndex])
    }

    public func padding<T: StringProtocol>(toLength newLength: Int, withPad padString: T, startingAt padIndex: Int) -> String {
        let s = String(self)
        let len = s.utf16.count
        if newLength <= len { var v = String.UnicodeScalarView(); v.append(contentsOf: s.unicodeScalars.prefix(newLength)); return String(v) }
        let pad = Array(String(padString))
        if pad.isEmpty { return s }
        var out = s
        var k = padIndex
        while out.utf16.count < newLength { out.append(pad[k % pad.count]); k += 1 }
        return out
    }

    public func folding(options: String.CompareOptions = [], locale: Locale?) -> String { _mosNormalized(options) }

    public func appending<T: StringProtocol>(_ aString: T) -> String { String(self) + String(aString) }

    public func appendingFormat<T: StringProtocol>(_ format: T, _ arguments: CVarArg...) -> String {
        String(self) + String(format: String(format), arguments: arguments)
    }

    public func lowercased(with locale: Locale?) -> String { lowercased() }
    public func uppercased(with locale: Locale?) -> String { uppercased() }
    public func capitalized(with locale: Locale?) -> String { capitalized }
    public var localizedLowercase: String { lowercased() }
    public var localizedUppercase: String { uppercased() }
    public var localizedCapitalized: String { capitalized }

    /// o hash do NSString equivalente (o mOS: o do -[NSString hash])
    public var hash: Int { _mosNSString(String(self)).hashValue }

    public func lengthOfBytes(using encoding: String.Encoding) -> Int {
        String(self).data(using: encoding)?.count ?? 0
    }

    public func cString(using encoding: String.Encoding) -> [CChar]? {
        guard let d = String(self).data(using: encoding) else { return nil }
        return d.map { CChar(bitPattern: $0) } + [0]
    }

    public func addingPercentEncoding(withAllowedCharacters allowedCharacters: CharacterSet) -> String? {
        let string = String(self)
        if allowedCharacters.hasIdenticalSwiftStorage(to: .urlPathAllowed) { return URLEncoder.percentEncode(path: string) }
        if allowedCharacters.hasIdenticalSwiftStorage(to: .urlHostAllowed) { return URLEncoder.percentEncode(host: string) }
        let mask = URLComponentAllowedMask(rawValue: allowedCharacters.asciiAllowedMask)
        return URLEncoder.percentEncode(string: string, component: mask, skipAlreadyEncoded: false)
    }

    public var removingPercentEncoding: String? { URLEncoder.percentDecode(string: String(self)) }

    // normalização Unicode: a stdlib compara já normalizado; o texto sai como está (o mOS ainda não tem as tabelas)
    public var decomposedStringWithCanonicalMapping: String { String(self) }
    public var precomposedStringWithCanonicalMapping: String { String(self) }
    public var precomposedStringWithCompatibilityMapping: String { String(self) }

    public func rangeOfComposedCharacterSequence(at anIndex: Index) -> Range<Index> {
        // o Character que contém o índice (os índices de self são de Character)

        var start = startIndex
        for c in indices {
            let next = index(after: c)
            if c <= anIndex && anIndex < next { return c..<next }
            start = next
        }
        return start..<start
    }

    public func rangeOfComposedCharacterSequences<R: RangeExpression>(for range: R) -> Range<Index> where R.Bound == Index {
        let r = range.relative(to: self)
        let lo = rangeOfComposedCharacterSequence(at: r.lowerBound).lowerBound
        let hi = r.upperBound >= endIndex ? endIndex : (r.isEmpty ? lo : rangeOfComposedCharacterSequence(at: index(before: r.upperBound)).upperBound)
        return lo..<hi
    }

    public func enumerateSubstrings<R: RangeExpression>(in range: R, options opts: String.EnumerationOptions = [],
                                                        _ body: @escaping (String?, Range<Index>, Range<Index>, inout Bool) -> Void)
        where R.Bound == Index {
        let r = range.relative(to: self)
        var stop = false
        let kind = opts.rawValue & 0xFF
        var pieces: [Range<Index>] = []
        switch kind {
        case String.EnumerationOptions.byComposedCharacterSequences.rawValue:
            var i = r.lowerBound
            while i < r.upperBound { let n = index(after: i); pieces.append(i..<n); i = n }
        case String.EnumerationOptions.byWords.rawValue:
            var i = r.lowerBound, start: Index? = nil
            while i < r.upperBound {
                let isW = self[i].isLetter || self[i].isNumber || self[i] == "'"
                if isW && start == nil { start = i }
                if !isW, let s = start { pieces.append(s..<i); start = nil }
                i = index(after: i)
            }
            if let s = start { pieces.append(s..<r.upperBound) }
        default:   // linhas, parágrafos e frases: por quebra de linha (frases: também por . ! ?)
            var i = r.lowerBound, s = r.lowerBound
            while i < r.upperBound {
                let c = self[i]
                let brk = c.isNewline || (kind == String.EnumerationOptions.bySentences.rawValue && (c == "." || c == "!" || c == "?"))
                if brk {
                    let e = kind == String.EnumerationOptions.bySentences.rawValue && !c.isNewline ? index(after: i) : i
                    pieces.append(s..<e); s = index(after: i)
                }
                i = index(after: i)
            }
            if s < r.upperBound { pieces.append(s..<r.upperBound) }
        }
        if opts.contains(.reverse) { pieces.reverse() }
        for p in pieces {
            body(opts.contains(.substringNotRequired) ? nil : String(self[p]), p, p, &stop)
            if stop { break }
        }
    }
}

/// compara com as opções (caseInsensitive, diacriticInsensitive, numeric)
internal func _mosCompare(_ a: String, _ b: String, _ o: String.CompareOptions) -> ComparisonResult {
    let x = Array(a._mosNormalized(o)), y = Array(b._mosNormalized(o))
    var i = 0, j = 0
    while i < x.count && j < y.count {
        if o.contains(.numeric), x[i].isNumber, y[j].isNumber {
            var ni = i, nj = j
            while ni < x.count && x[ni].isNumber { ni += 1 }
            while nj < y.count && y[nj].isNumber { nj += 1 }
            let da = String(x[i..<ni]).drop { $0 == "0" }, db = String(y[j..<nj]).drop { $0 == "0" }
            if da.count != db.count { return da.count < db.count ? .orderedAscending : .orderedDescending }
            if da != db { return da < db ? .orderedAscending : .orderedDescending }
            i = ni; j = nj
            continue
        }
        if x[i] != y[j] { return String(x[i]) < String(y[j]) ? .orderedAscending : .orderedDescending }
        i += 1; j += 1
    }
    if x.count - i == y.count - j { return .orderedSame }
    return x.count - i < y.count - j ? .orderedAscending : .orderedDescending
}

// MARK: - String: inits da API de NSString

extension String {
    public init(_ cocoaString: NSString) { self = _mosString(cocoaString) }

    public init?(cString: UnsafePointer<CChar>, encoding enc: Encoding) {
        let n = strlen(cString)
        let d = Data(bytes: cString, count: n)
        guard let s = String(data: d, encoding: enc) else { return nil }
        self = s
    }

    public init(utf16CodeUnits: UnsafePointer<unichar>, count: Int) {
        self = String(decoding: UnsafeBufferPointer(start: utf16CodeUnits, count: count), as: UTF16.self)
    }

    /// (obsoletos na Apple) o encoding é detectado: UTF-8, senão Latin-1
    public init(contentsOfFile path: __shared String) throws {
        let d = try Data(contentsOf: URL(fileURLWithPath: path))
        self = String(data: d, encoding: .utf8) ?? String(data: d, encoding: .isoLatin1) ?? ""
    }

    public init(contentsOf url: __shared URL) throws {
        let d = try Data(contentsOf: url)
        self = String(data: d, encoding: .utf8) ?? String(data: d, encoding: .isoLatin1) ?? ""
    }

    /// texto localizável (SwiftUI/Text); o mOS não traduz, guarda o literal
    public struct LocalizationValue: Equatable, ExpressibleByStringInterpolation, Sendable {
        internal var _key: String
        public init(stringLiteral value: String) { _key = value }
        public init(_ value: String) { _key = value }
    }
}

extension String {
    /// como na Apple: as opções de enumerateSubstrings são as NSStringEnumerationOptions do ObjC
    public typealias EnumerationOptions = NSStringEnumerationOptions
}
