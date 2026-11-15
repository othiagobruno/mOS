// mOS — overlay Foundation: o que o Foundation da Apple tem em Calendar, Locale e NSFileHandle e o FoundationEssentials
// do swift-foundation deixa para o FoundationInternationalization (ICU), que o mOS não porta. Os nomes de dias e meses
// vêm de duas tabelas (pt e en): o mOS só fala pt-BR e inglês. O resto do mundo cai no inglês.

import Darwin

internal struct _MOSCalendarNames {
    let weekdays: [String], shortWeekdays: [String], veryShortWeekdays: [String]
    let months: [String], shortMonths: [String], veryShortMonths: [String]
    let quarters: [String], shortQuarters: [String]
    let am: String, pm: String, eras: [String], longEras: [String]

    static let pt = _MOSCalendarNames(
        weekdays: ["domingo", "segunda-feira", "terça-feira", "quarta-feira", "quinta-feira", "sexta-feira", "sábado"],
        shortWeekdays: ["dom.", "seg.", "ter.", "qua.", "qui.", "sex.", "sáb."],
        veryShortWeekdays: ["D", "S", "T", "Q", "Q", "S", "S"],
        months: ["janeiro", "fevereiro", "março", "abril", "maio", "junho", "julho", "agosto", "setembro", "outubro",
                 "novembro", "dezembro"],
        shortMonths: ["jan.", "fev.", "mar.", "abr.", "mai.", "jun.", "jul.", "ago.", "set.", "out.", "nov.", "dez."],
        veryShortMonths: ["J", "F", "M", "A", "M", "J", "J", "A", "S", "O", "N", "D"],
        quarters: ["1º trimestre", "2º trimestre", "3º trimestre", "4º trimestre"],
        shortQuarters: ["T1", "T2", "T3", "T4"],
        am: "AM", pm: "PM", eras: ["a.C.", "d.C."], longEras: ["antes de Cristo", "depois de Cristo"])

    static let en = _MOSCalendarNames(
        weekdays: ["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"],
        shortWeekdays: ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"],
        veryShortWeekdays: ["S", "M", "T", "W", "T", "F", "S"],
        months: ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October",
                 "November", "December"],
        shortMonths: ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"],
        veryShortMonths: ["J", "F", "M", "A", "M", "J", "J", "A", "S", "O", "N", "D"],
        quarters: ["1st quarter", "2nd quarter", "3rd quarter", "4th quarter"],
        shortQuarters: ["Q1", "Q2", "Q3", "Q4"],
        am: "AM", pm: "PM", eras: ["BC", "AD"], longEras: ["Before Christ", "Anno Domini"])

    static func of(_ locale: Locale?) -> _MOSCalendarNames {
        (locale ?? Locale.current).identifier.lowercased().hasPrefix("pt") ? pt : en
    }
}

extension Calendar {
    private var _names: _MOSCalendarNames { _MOSCalendarNames.of(locale) }
    /// a forma isolada é a do contexto; no pt ela começa com maiúscula ("Domingo", "Janeiro")
    private func _standalone(_ a: [String]) -> [String] {
        _names.weekdays == _MOSCalendarNames.pt.weekdays ? a.map { $0.prefix(1).uppercased() + $0.dropFirst() } : a
    }

    public var eraSymbols: [String] { _names.eras }
    public var longEraSymbols: [String] { _names.longEras }
    public var monthSymbols: [String] { _names.months }
    public var shortMonthSymbols: [String] { _names.shortMonths }
    public var veryShortMonthSymbols: [String] { _names.veryShortMonths }
    public var standaloneMonthSymbols: [String] { _standalone(_names.months) }
    public var shortStandaloneMonthSymbols: [String] { _standalone(_names.shortMonths) }
    public var veryShortStandaloneMonthSymbols: [String] { _names.veryShortMonths }
    public var weekdaySymbols: [String] { _names.weekdays }
    public var shortWeekdaySymbols: [String] { _names.shortWeekdays }
    public var veryShortWeekdaySymbols: [String] { _names.veryShortWeekdays }
    public var standaloneWeekdaySymbols: [String] { _standalone(_names.weekdays) }
    public var shortStandaloneWeekdaySymbols: [String] { _standalone(_names.shortWeekdays) }
    public var veryShortStandaloneWeekdaySymbols: [String] { _names.veryShortWeekdays }
    public var quarterSymbols: [String] { _names.quarters }
    public var shortQuarterSymbols: [String] { _names.shortQuarters }
    public var standaloneQuarterSymbols: [String] { _standalone(_names.quarters) }
    public var shortStandaloneQuarterSymbols: [String] { _names.shortQuarters }
    public var amSymbol: String { _names.am }
    public var pmSymbol: String { _names.pm }
}

extension Locale {
    /// as chaves são as do NSLocale (kCFLocaleLanguageCodeKey...), como na Apple
    public static func components(fromIdentifier string: String) -> [String: String] {
        var out: [String: String] = [:]
        var base = Substring(string)
        if let at = base.firstIndex(of: "@") {
            for kv in base[base.index(after: at)...].split(separator: ";") {
                let p = kv.split(separator: "=", maxSplits: 1)
                if p.count == 2 { out[String(p[0])] = String(p[1]) }
            }
            base = base[..<at]
        }
        let parts = base.split(whereSeparator: { $0 == "_" || $0 == "-" }).map(String.init)
        guard let lang = parts.first, !lang.isEmpty else { return out }
        out["kCFLocaleLanguageCodeKey"] = lang
        var i = 1
        if i < parts.count, parts[i].count == 4 { out["kCFLocaleScriptCodeKey"] = parts[i]; i += 1 }
        if i < parts.count, parts[i].count == 2 || parts[i].count == 3 { out["kCFLocaleCountryCodeKey"] = parts[i]; i += 1 }
        if i < parts.count { out["kCFLocaleVariantCodeKey"] = parts[i...].joined(separator: "_") }
        return out
    }

    /// códigos ISO 4217 em uso
    public static var isoCurrencyCodes: [String] { _mosISOCurrencyCodes }
    public static var commonISOCurrencyCodes: [String] {
        ["AUD", "BRL", "CAD", "CHF", "CNY", "EUR", "GBP", "HKD", "INR", "JPY", "KRW", "MXN", "NOK", "NZD", "SEK", "USD"]
    }
}

internal let _mosISOCurrencyCodes: [String] = [
    "AED", "AFN", "ALL", "AMD", "ANG", "AOA", "ARS", "AUD", "AWG", "AZN", "BAM", "BBD", "BDT", "BGN", "BHD", "BIF", "BMD",
    "BND", "BOB", "BRL", "BSD", "BTN", "BWP", "BYN", "BZD", "CAD", "CDF", "CHF", "CLP", "CNY", "COP", "CRC", "CUP", "CVE",
    "CZK", "DJF", "DKK", "DOP", "DZD", "EGP", "ERN", "ETB", "EUR", "FJD", "FKP", "GBP", "GEL", "GHS", "GIP", "GMD", "GNF",
    "GTQ", "GYD", "HKD", "HNL", "HTG", "HUF", "IDR", "ILS", "INR", "IQD", "IRR", "ISK", "JMD", "JOD", "JPY", "KES", "KGS",
    "KHR", "KMF", "KPW", "KRW", "KWD", "KYD", "KZT", "LAK", "LBP", "LKR", "LRD", "LSL", "LYD", "MAD", "MDL", "MGA", "MKD",
    "MMK", "MNT", "MOP", "MRU", "MUR", "MVR", "MWK", "MXN", "MYR", "MZN", "NAD", "NGN", "NIO", "NOK", "NPR", "NZD", "OMR",
    "PAB", "PEN", "PGK", "PHP", "PKR", "PLN", "PYG", "QAR", "RON", "RSD", "RUB", "RWF", "SAR", "SBD", "SCR", "SDG", "SEK",
    "SGD", "SHP", "SLE", "SOS", "SRD", "SSP", "STN", "SYP", "SZL", "THB", "TJS", "TMT", "TND", "TOP", "TRY", "TTD", "TWD",
    "TZS", "UAH", "UGX", "USD", "UYU", "UZS", "VES", "VND", "VUV", "WST", "XAF", "XCD", "XOF", "XPF", "YER", "ZAR", "ZMW",
    "ZWL",
]

extension FileHandle {
    /// lê até o fim do arquivo (nil se já estava no fim)
    public func readToEnd() throws -> Data? {
        var out = Data()
        while let chunk = try read(upToCount: 64 * 1024), !chunk.isEmpty { out.append(chunk) }
        return out.isEmpty ? nil : out
    }

    /// até `count` bytes; nil no fim do arquivo
    public func read(upToCount count: Int) throws -> Data? {
        if count <= 0 { return Data() }
        var buf = [UInt8](repeating: 0, count: count)
        var got = 0
        while got < count {
            let n = buf.withUnsafeMutableBytes { Darwin.read(fileDescriptor, $0.baseAddress! + got, count - got) }
            if n < 0 {
                if errno == EINTR { continue }
                throw CocoaError(.fileReadUnknown, userInfo: [NSUnderlyingErrorKey: POSIXError(POSIXErrorCode(rawValue: errno) ?? .EIO)])
            }
            if n == 0 { break }
            got += n
        }
        return got == 0 ? nil : Data(buf[0..<got])
    }
}
