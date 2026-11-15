// mOS — overlay Foundation: a API Swift do Scanner da Apple (Scanner.swift do Foundation: currentIndex, scanString,
// scanCharacters(from:)...). Em Swift sobre a string do NSScanner do libSystem; a posição dele é em bytes UTF-8
// (scanLocation), então currentIndex converte pela visão utf8. Como na Apple, os métodos pulam os espaços do início.

extension Scanner {
    public enum NumberRepresentation {
        case decimal
        case hexadecimal
    }

    private var _s: String { _mosString(string) }

    public var currentIndex: String.Index {
        get {
            let s = _s, u = s.utf8
            let off = Swift.min(Int(scanLocation), u.count)
            return u.index(u.startIndex, offsetBy: off)
        }
        set {
            let s = _s
            scanLocation = s.utf8.distance(from: s.utf8.startIndex, to: newValue)
        }
    }

    private func _skipSpaces() {
        let s = _s
        var i = currentIndex
        while i < s.endIndex, s[i].isWhitespace { i = s.index(after: i) }
        currentIndex = i
    }

    public func scanString(_ searchString: String) -> String? {
        _skipSpaces()
        let s = _s
        guard s[currentIndex...].hasPrefix(searchString) else { return nil }
        let end = s.index(currentIndex, offsetBy: searchString.count)
        let r = String(s[currentIndex..<end])
        currentIndex = end
        return r
    }

    public func scanUpToString(_ substring: String) -> String? {
        _skipSpaces()
        let s = _s
        let start = currentIndex
        guard start < s.endIndex else { return nil }
        let end = s[start...].range(of: substring)?.lowerBound ?? s.endIndex
        guard end > start else { return nil }
        currentIndex = end
        return String(s[start..<end])
    }

    public func scanCharacters(from set: CharacterSet) -> String? {
        _skipSpaces()
        let s = _s
        let start = currentIndex
        var i = start
        while i < s.endIndex, s[i].unicodeScalars.allSatisfy({ set.contains($0) }) { i = s.index(after: i) }
        guard i > start else { return nil }
        currentIndex = i
        return String(s[start..<i])
    }

    public func scanUpToCharacters(from set: CharacterSet) -> String? {
        _skipSpaces()
        let s = _s
        let start = currentIndex
        var i = start
        while i < s.endIndex, !s[i].unicodeScalars.contains(where: { set.contains($0) }) { i = s.index(after: i) }
        guard i > start else { return nil }
        currentIndex = i
        return String(s[start..<i])
    }

    public func scanCharacter() -> Character? {
        _skipSpaces()
        let s = _s
        guard currentIndex < s.endIndex else { return nil }
        let c = s[currentIndex]
        currentIndex = s.index(after: currentIndex)
        return c
    }

    public func scanInt(representation: NumberRepresentation = .decimal) -> Int? {
        _skipSpaces()
        let s = _s
        var i = currentIndex
        var txt = ""
        if i < s.endIndex, s[i] == "-" || s[i] == "+" { txt.append(s[i]); i = s.index(after: i) }
        if representation == .hexadecimal, s[i...].lowercased().hasPrefix("0x") { i = s.index(i, offsetBy: 2) }
        while i < s.endIndex, representation == .decimal ? s[i].isASCII && s[i].isNumber : s[i].isHexDigit {
            txt.append(s[i]); i = s.index(after: i)
        }
        guard let v = Int(txt, radix: representation == .decimal ? 10 : 16) else { return nil }
        currentIndex = i
        return v
    }

    public func scanDouble(representation: NumberRepresentation = .decimal) -> Double? {
        _skipSpaces()
        if representation == .hexadecimal { return scanInt(representation: .hexadecimal).map(Double.init) }
        let s = _s
        var i = currentIndex
        var txt = ""
        var seenDigit = false, seenDot = false, seenExp = false
        while i < s.endIndex {
            let c = s[i]
            if c.isASCII && c.isNumber { seenDigit = true }
            else if (c == "-" || c == "+") && (txt.isEmpty || txt.last == "e" || txt.last == "E") {}
            else if c == "." && !seenDot && !seenExp { seenDot = true }
            else if (c == "e" || c == "E") && seenDigit && !seenExp { seenExp = true }
            else { break }
            txt.append(c); i = s.index(after: i)
        }
        guard seenDigit, let v = Double(txt) else { return nil }
        currentIndex = i
        return v
    }
}
