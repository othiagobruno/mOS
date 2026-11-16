// mOS — overlay Foundation: a API Swift do Scanner da Apple (currentIndex, scanString, scanCharacters(from:),
// scanInt(representation:)...) sobre o NSScanner do libSystem (userland/libsystem/fdn_scanner.c). Toda a lógica
// (charactersToBeSkipped, caseSensitive, locale, posição em UTF-16, falha sem mover) está do lado ObjC, então os
// métodos novos e os antigos (scanString(_:into:)...) se comportam igual, como no macOS.

extension Scanner {
    public enum NumberRepresentation {
        case decimal
        case hexadecimal
    }

    private var _s: String { _mosString(string) }

    /// Conjunto pulado antes de cada leitura (padrão: espaços e quebras de linha; nil = não pula nada).
    public var charactersToBeSkipped: CharacterSet? {
        get { __charactersToBeSkipped.map { CharacterSet._unconditionallyBridgeFromObjectiveC($0) } }
        set { __charactersToBeSkipped = newValue.map { $0._bridgeToObjectiveC() } }
    }

    public var currentIndex: String.Index {
        get {
            let s = _s
            return String.Index(utf16Offset: Swift.min(Int(scanLocation), s.utf16.count), in: s)
        }
        set {
            let s = _s
            scanLocation = s.utf16.distance(from: s.utf16.startIndex, to: newValue)
        }
    }

    public func scanDecimal(_ dcm: UnsafeMutablePointer<Decimal>?) -> Bool { __scanDecimal(dcm) }

    // MARK: - texto

    public func scanString(_ searchString: String) -> String? {
        var r: NSString? = nil
        return scanString(_mosNSString(searchString), into: &r) ? r.map(_mosString) : nil
    }

    public func scanUpToString(_ substring: String) -> String? {
        var r: NSString? = nil
        return scanUpTo(_mosNSString(substring), into: &r) ? r.map(_mosString) : nil
    }

    public func scanCharacters(from set: CharacterSet) -> String? {
        var r: NSString? = nil
        return scanCharacters(from: set._bridgeToObjectiveC(), into: &r) ? r.map(_mosString) : nil
    }

    public func scanUpToCharacters(from set: CharacterSet) -> String? {
        var r: NSString? = nil
        return scanUpToCharacters(from: set._bridgeToObjectiveC(), into: &r) ? r.map(_mosString) : nil
    }

    /// Um Character inteiro (grafema), depois de pular charactersToBeSkipped.
    public func scanCharacter() -> Character? {
        let s = _s
        let loc = Int(_mos_skippedLocation())
        guard loc < s.utf16.count else { return nil }
        let rest = s[String.Index(utf16Offset: loc, in: s)...]
        guard let c = rest.first else { return nil }
        let end = rest.index(after: rest.startIndex)
        scanLocation = s.utf16.distance(from: s.utf16.startIndex, to: end)
        return c
    }

    // MARK: - números

    public func scanInt(representation: NumberRepresentation = .decimal) -> Int? {
        switch representation {
        case .decimal:
            var v = 0
            return scanInt(&v) ? v : nil
        case .hexadecimal:
            var v: UInt64 = 0
            return scanHexInt64(&v) ? Int(truncatingIfNeeded: v) : nil
        }
    }

    public func scanInt32(representation: NumberRepresentation = .decimal) -> Int32? {
        switch representation {
        case .decimal:
            var v: Int32 = 0
            return scanInt32(&v) ? v : nil
        case .hexadecimal:
            var v: UInt32 = 0
            return scanHexInt32(&v) ? Int32(bitPattern: v) : nil
        }
    }

    public func scanInt64(representation: NumberRepresentation = .decimal) -> Int64? {
        switch representation {
        case .decimal:
            var v: Int64 = 0
            return scanInt64(&v) ? v : nil
        case .hexadecimal:
            var v: UInt64 = 0
            return scanHexInt64(&v) ? Int64(bitPattern: v) : nil
        }
    }

    public func scanUInt64(representation: NumberRepresentation = .decimal) -> UInt64? {
        var v: UInt64 = 0
        switch representation {
        case .decimal: return scanUnsignedLongLong(&v) ? v : nil
        case .hexadecimal: return scanHexInt64(&v) ? v : nil
        }
    }

    public func scanFloat(representation: NumberRepresentation = .decimal) -> Float? {
        var v: Float = 0
        switch representation {
        case .decimal: return scanFloat(&v) ? v : nil
        case .hexadecimal: return scanHexFloat(&v) ? v : nil
        }
    }

    public func scanDouble(representation: NumberRepresentation = .decimal) -> Double? {
        var v: Double = 0
        switch representation {
        case .decimal: return scanDouble(&v) ? v : nil
        case .hexadecimal: return scanHexDouble(&v) ? v : nil
        }
    }

    public func scanDecimal() -> Decimal? {
        var v = Decimal()
        return scanDecimal(&v) ? v : nil
    }
}
