// mOS SDK — os formatadores do Foundation clássico (Formatter, NumberFormatter, DateFormatter, ISO8601DateFormatter,
// DateIntervalFormatter, Mass/Length/EnergyFormatter), NSCalendar.Unit e IndexSet. Sem ICU (o
// FoundationInternationalization ainda não está no mOS): números e datas saem no formato en_US/POSIX, com padrões de
// data (yyyy-MM-dd HH:mm...) feitos aqui sobre o Calendar gregoriano do FoundationEssentials.

// MARK: - Formatter

open class Formatter: NSObject, NSCopying, @unchecked Sendable {
    public enum Context: Int, Sendable { case unknown = 0, dynamic = 1, standalone = 2, listItem = 3, beginningOfSentence = 4, middleOfSentence = 5 }
    public enum UnitStyle: Int, Sendable { case short = 1, medium = 2, long = 3 }
    open var formattingContext: Context = .unknown
    public override init() {}
    open func string(for obj: Any?) -> String? { nil }
    open func editingString(for obj: Any) -> String? { string(for: obj) }
    open func copy(with zone: NSZone? = nil) -> Any { self }
}

// MARK: - NumberFormatter

open class NumberFormatter: Formatter, @unchecked Sendable {
    public enum Style: UInt, Sendable {
        case none = 0, decimal = 1, currency = 2, percent = 3, scientific = 4, spellOut = 5, ordinal = 6,
             currencyISOCode = 8, currencyPlural = 9, currencyAccounting = 10
    }
    public enum RoundingMode: UInt, Sendable { case ceiling, floor, down, up, halfEven, halfDown, halfUp }

    open var numberStyle: Style = .none {
        didSet {
            switch numberStyle {
            case .decimal: maximumFractionDigits = 3; usesGroupingSeparator = true
            case .currency, .currencyISOCode, .currencyAccounting, .currencyPlural: minimumFractionDigits = 2; maximumFractionDigits = 2; usesGroupingSeparator = true
            case .percent: maximumFractionDigits = 0; usesGroupingSeparator = true
            default: maximumFractionDigits = 0; usesGroupingSeparator = false
            }
        }
    }
    open var locale: Locale! = .current
    open var minimumIntegerDigits = 1
    open var maximumIntegerDigits = 42
    open var minimumFractionDigits = 0
    open var maximumFractionDigits = 0
    open var usesGroupingSeparator = false
    open var groupingSeparator: String! = ","
    open var decimalSeparator: String! = "."
    open var groupingSize = 3
    open var currencySymbol: String! = "$"
    open var currencyCode: String! = "USD"
    open var percentSymbol: String! = "%"
    open var minusSign: String! = "-"
    open var positivePrefix: String! = ""
    open var negativePrefix: String! = "-"
    open var positiveSuffix: String! = ""
    open var negativeSuffix: String! = ""
    open var zeroSymbol: String?
    open var nilSymbol: String = ""
    open var notANumberSymbol: String! = "NaN"
    open var roundingMode: RoundingMode = .halfEven
    open var generatesDecimalNumbers = false
    open var allowsFloats = true
    open var isLenient = false
    open var multiplier: NSNumber?

    open func string(from number: NSNumber) -> String? {
        var v = number.doubleValue
        if let m = multiplier { v *= m.doubleValue }
        if numberStyle == .percent { v *= 100 }
        if v.isNaN { return notANumberSymbol }
        if v == 0, let z = zeroSymbol { return z }
        let neg = v < 0
        v = abs(v)
        // arredonda nas casas máximas e corta zeros à direita até as mínimas
        var scale = 1.0
        for _ in 0 ..< maximumFractionDigits { scale *= 10 }
        let rounded = (v * scale).rounded(.toNearestOrEven)
        var intPart = (rounded / scale).rounded(.towardZero)
        var frac = Int((rounded - intPart * scale).rounded())
        if frac >= Int(scale) { intPart += 1; frac -= Int(scale) }
        var digits = String(Int64(intPart))
        while digits.count < minimumIntegerDigits { digits = "0" + digits }
        if usesGroupingSeparator, groupingSize > 0, digits.count > groupingSize {
            var out = ""
            for (i, c) in digits.reversed().enumerated() {
                if i > 0, i % groupingSize == 0 { out.append(contentsOf: groupingSeparator.reversed()) }
                out.append(c)
            }
            digits = String(out.reversed())
        }
        var fracStr = maximumFractionDigits > 0 ? String(frac) : ""
        while fracStr.count < maximumFractionDigits { fracStr = "0" + fracStr }
        while fracStr.count > minimumFractionDigits, fracStr.hasSuffix("0") { fracStr.removeLast() }
        var s = fracStr.isEmpty ? digits : digits + decimalSeparator + fracStr
        switch numberStyle {
        case .percent: s += percentSymbol
        case .currency, .currencyAccounting: s = currencySymbol + s
        case .currencyISOCode: s = currencyCode + " " + s
        case .ordinal:
            let n = Int(intPart) % 100
            s += (11 ... 13).contains(n) ? "th" : ["th", "st", "nd", "rd", "th", "th", "th", "th", "th", "th"][Int(intPart) % 10]
        case .scientific:
            let e = v == 0 ? 0 : Int(log10(v).rounded(.down))
            s = String(format: "%.\(Swift.max(maximumFractionDigits, 1))fE%d", v / pow(10, Double(e)), e)
        default: break
        }
        return neg ? (negativePrefix ?? "-") + s + (negativeSuffix ?? "") : (positivePrefix ?? "") + s + (positiveSuffix ?? "")
    }
    open override func string(for obj: Any?) -> String? {
        switch obj {
        case let n as NSNumber: string(from: n)
        case let i as Int: string(from: NSNumber(value: i))
        case let d as Double: string(from: NSNumber(value: d))
        case let f as Float: string(from: NSNumber(value: f))
        case let f as CGFloat: string(from: NSNumber(value: Double(f)))
        case let b as Bool: string(from: NSNumber(value: b))
        default: nil
        }
    }
    open func number(from string: String) -> NSNumber? {
        var s = string.replacingOccurrences(of: groupingSeparator ?? ",", with: "")
        if let d = decimalSeparator, d != "." { s = s.replacingOccurrences(of: d, with: ".") }
        s = s.replacingOccurrences(of: currencySymbol ?? "$", with: "").replacingOccurrences(of: percentSymbol ?? "%", with: "")
        s = s.trimmingCharacters(in: .whitespaces)
        if let i = Int(s) { return NSNumber(value: numberStyle == .percent ? Double(i) / 100 : Double(i)) }
        if let d = Double(s) { return NSNumber(value: numberStyle == .percent ? d / 100 : d) }
        return nil
    }
    open class func localizedString(from num: NSNumber, number nstyle: Style) -> String {
        let f = NumberFormatter()
        f.numberStyle = nstyle
        return f.string(from: num) ?? num.stringValue
    }
    open override func copy(with zone: NSZone? = nil) -> Any {
        let f = NumberFormatter()
        f.numberStyle = numberStyle; f.locale = locale
        f.minimumIntegerDigits = minimumIntegerDigits; f.maximumIntegerDigits = maximumIntegerDigits
        f.minimumFractionDigits = minimumFractionDigits; f.maximumFractionDigits = maximumFractionDigits
        f.usesGroupingSeparator = usesGroupingSeparator; f.groupingSeparator = groupingSeparator; f.decimalSeparator = decimalSeparator
        f.currencySymbol = currencySymbol; f.multiplier = multiplier
        return f
    }
}

extension String {
    /// replacingOccurrences(of:with:) do NSString (o FoundationEssentials não tem fora do build da Apple).
    public func replacingOccurrences(of target: String, with replacement: String) -> String {
        guard !target.isEmpty else { return self }
        return replacing(target, with: replacement)
    }
}

// MARK: - DateFormatter

open class DateFormatter: Formatter, @unchecked Sendable {
    public enum Style: UInt, Sendable { case none = 0, short = 1, medium = 2, long = 3, full = 4 }
    public enum Behavior: UInt, Sendable { case `default` = 0, behavior10_0 = 1000, behavior10_4 = 1040 }

    open var dateFormat: String! = ""
    open var dateStyle: Style = .none
    open var timeStyle: Style = .none
    open var locale: Locale! = .current
    open var calendar: Calendar! = Calendar(identifier: .gregorian)
    open var timeZone: TimeZone! = .current
    open var isLenient = false
    open var doesRelativeDateFormatting = false
    open var formatterBehavior: Behavior = .default
    open var amSymbol: String! = "AM"
    open var pmSymbol: String! = "PM"
    open var monthSymbols: [String]! = ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"]
    open var shortMonthSymbols: [String]! = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"]
    open var weekdaySymbols: [String]! = ["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"]
    open var shortWeekdaySymbols: [String]! = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"]

    open func setLocalizedDateFormatFromTemplate(_ template: String) { dateFormat = template }
    open class func dateFormat(fromTemplate tmplate: String, options opts: Int, locale: Locale?) -> String? { tmplate }

    /// Padrão efetivo: dateFormat ou o equivalente en_US dos estilos.
    var _pattern: String {
        if let f = dateFormat, !f.isEmpty { return f }
        let d: String = switch dateStyle {
        case .none: ""
        case .short: "M/d/yy"
        case .medium: "MMM d, y"
        case .long: "MMMM d, y"
        case .full: "EEEE, MMMM d, y"
        }
        let t: String = switch timeStyle {
        case .none: ""
        case .short: "h:mm a"
        case .medium: "h:mm:ss a"
        case .long, .full: "h:mm:ss a z"
        }
        return [d, t].filter { !$0.isEmpty }.joined(separator: d.isEmpty || t.isEmpty ? "" : ", ")
    }

    open func string(from date: Date) -> String {
        var cal = calendar ?? Calendar(identifier: .gregorian)
        cal.timeZone = timeZone ?? .current
        let c = cal.dateComponents([.era, .year, .month, .day, .hour, .minute, .second, .nanosecond, .weekday], from: date)
        let tz = cal.timeZone
        var out = ""
        let p = Array(_pattern)
        var i = 0
        func pad(_ v: Int, _ n: Int) -> String { let s = String(v); return s.count >= n ? s : String(repeating: "0", count: n - s.count) + s }
        while i < p.count {
            let ch = p[i]
            if ch == "'" {
                i += 1
                if i < p.count, p[i] == "'" { out.append("'"); i += 1; continue }
                while i < p.count, p[i] != "'" { out.append(p[i]); i += 1 }
                i += 1
                continue
            }
            guard _mosIsASCIILetter(ch) else { out.append(ch); i += 1; continue }
            var n = 1
            while i + n < p.count, p[i + n] == ch { n += 1 }
            i += n
            switch ch {
            case "y", "u": let y = c.year ?? 0; out += n == 2 ? pad(y % 100, 2) : pad(y, n)
            case "M", "L":
                let m = c.month ?? 1
                out += n >= 4 ? monthSymbols[m - 1] : n == 3 ? shortMonthSymbols[m - 1] : pad(m, n)
            case "d": out += pad(c.day ?? 1, n)
            case "H": out += pad(c.hour ?? 0, n)
            case "k": out += pad((c.hour ?? 0) == 0 ? 24 : c.hour!, n)
            case "h": let h = (c.hour ?? 0) % 12; out += pad(h == 0 ? 12 : h, n)
            case "K": out += pad((c.hour ?? 0) % 12, n)
            case "m": out += pad(c.minute ?? 0, n)
            case "s": out += pad(c.second ?? 0, n)
            case "S":
                let ns = c.nanosecond ?? 0
                var s = pad(ns, 9)
                s = String(s.prefix(n))
                out += s
            case "a": out += (c.hour ?? 0) < 12 ? amSymbol : pmSymbol
            case "E", "e", "c":
                let w = (c.weekday ?? 1) - 1
                out += n >= 4 ? weekdaySymbols[w] : shortWeekdaySymbols[w]
            case "G": out += (c.era ?? 1) == 1 ? "AD" : "BC"
            case "Z", "x", "X":
                let off = tz.secondsFromGMT(for: date)
                if off == 0, ch == "X" { out += "Z"; break }
                let sign = off < 0 ? "-" : "+", a = abs(off)
                out += sign + pad(a / 3600, 2) + (n >= 3 && ch != "Z" || n == 5 ? ":" : "") + pad(a % 3600 / 60, 2)
            case "z", "v", "V", "O": out += tz.abbreviation(for: date) ?? tz.identifier
            default: out += String(repeating: ch, count: n)
            }
        }
        return out
    }
    open override func string(for obj: Any?) -> String? { (obj as? Date).map { string(from: $0) } }

    /// Lê datas no padrão (só campos numéricos, AM/PM e nomes de mês/dia em inglês).
    open func date(from string: String) -> Date? {
        var cal = calendar ?? Calendar(identifier: .gregorian)
        cal.timeZone = timeZone ?? .current
        var dc = DateComponents()
        var pm: Bool? = nil
        let p = Array(_pattern), s = Array(string)
        var i = 0, j = 0
        func number(_ maxLen: Int) -> Int? {
            var k = j, v = 0
            while k < s.count, k - j < maxLen, let a = s[k].asciiValue, a >= 48, a <= 57 { v = v * 10 + Int(a - 48); k += 1 }
            guard k > j else { return nil }
            j = k
            return v
        }
        func word(_ list: [String]) -> Int? {
            for (idx, w) in list.enumerated() where String(s[j...]).lowercased().hasPrefix(w.lowercased()) { j += w.count; return idx }
            return nil
        }
        while i < p.count {
            let ch = p[i]
            if ch == "'" {
                i += 1
                while i < p.count, p[i] != "'" { if j < s.count, s[j] == p[i] { j += 1 }; i += 1 }
                i += 1
                continue
            }
            guard _mosIsASCIILetter(ch) else {
                guard j < s.count, s[j] == ch || (ch == " " && (s[j] == " " || s[j] == "\t")) else { return nil }
                i += 1; j += 1
                continue
            }
            var n = 1
            while i + n < p.count, p[i + n] == ch { n += 1 }
            i += n
            switch ch {
            case "y", "u":
                guard let y = number(n == 2 ? 2 : 4) else { return nil }
                dc.year = n == 2 ? 2000 + y : y
            case "M", "L":
                if n >= 3 { guard let m = word(n >= 4 ? monthSymbols : shortMonthSymbols) else { return nil }; dc.month = m + 1 }
                else { guard let m = number(2) else { return nil }; dc.month = m }
            case "d": guard let d = number(2) else { return nil }; dc.day = d
            case "H", "k", "h", "K": guard let h = number(2) else { return nil }; dc.hour = h
            case "m": guard let m = number(2) else { return nil }; dc.minute = m
            case "s": guard let v = number(2) else { return nil }; dc.second = v
            case "S":
                guard let f = number(n) else { return nil }
                var ns = f
                for _ in n ..< 9 { ns *= 10 }
                dc.nanosecond = ns
            case "a":
                if String(s[j...]).uppercased().hasPrefix(pmSymbol.uppercased()) { pm = true; j += pmSymbol.count }
                else if String(s[j...]).uppercased().hasPrefix(amSymbol.uppercased()) { pm = false; j += amSymbol.count }
                else { return nil }
            case "E", "e", "c": _ = word(n >= 4 ? weekdaySymbols : shortWeekdaySymbols)
            case "Z", "x", "X":
                if j < s.count, s[j] == "Z" { j += 1; dc.timeZone = TimeZone(secondsFromGMT: 0); break }
                guard j < s.count, s[j] == "+" || s[j] == "-" else { return nil }
                let sign = s[j] == "-" ? -1 : 1; j += 1
                guard let h = number(2) else { return nil }
                if j < s.count, s[j] == ":" { j += 1 }
                let m = number(2) ?? 0
                dc.timeZone = TimeZone(secondsFromGMT: sign * (h * 3600 + m * 60))
            default: break
            }
        }
        if let pm, let h = dc.hour { dc.hour = pm ? (h % 12) + 12 : h % 12 }
        return cal.date(from: dc)
    }
    open override func copy(with zone: NSZone? = nil) -> Any {
        let f = DateFormatter()
        f.dateFormat = dateFormat; f.dateStyle = dateStyle; f.timeStyle = timeStyle
        f.locale = locale; f.calendar = calendar; f.timeZone = timeZone
        return f
    }
    open class func localizedString(from date: Date, dateStyle dstyle: Style, timeStyle tstyle: Style) -> String {
        let f = DateFormatter()
        f.dateStyle = dstyle; f.timeStyle = tstyle
        return f.string(from: date)
    }
}

open class ISO8601DateFormatter: Formatter, @unchecked Sendable {
    public struct Options: OptionSet, Sendable {
        public let rawValue: UInt
        public init(rawValue: UInt) { self.rawValue = rawValue }
        public static let withYear = Options(rawValue: 1 << 0)
        public static let withMonth = Options(rawValue: 1 << 1)
        public static let withWeekOfYear = Options(rawValue: 1 << 2)
        public static let withDay = Options(rawValue: 1 << 4)
        public static let withTime = Options(rawValue: 1 << 5)
        public static let withTimeZone = Options(rawValue: 1 << 6)
        public static let withSpaceBetweenDateAndTime = Options(rawValue: 1 << 7)
        public static let withDashSeparatorInDate = Options(rawValue: 1 << 8)
        public static let withColonSeparatorInTime = Options(rawValue: 1 << 9)
        public static let withColonSeparatorInTimeZone = Options(rawValue: 1 << 10)
        public static let withFractionalSeconds = Options(rawValue: 1 << 11)
        public static let withFullDate: Options = [.withYear, .withMonth, .withDay, .withDashSeparatorInDate]
        public static let withFullTime: Options = [.withTime, .withColonSeparatorInTime, .withTimeZone, .withColonSeparatorInTimeZone]
        public static let withInternetDateTime: Options = [.withFullDate, .withFullTime]
    }
    open var timeZone: TimeZone! = TimeZone(secondsFromGMT: 0)
    open var formatOptions: Options = .withInternetDateTime

    private var _formatter: DateFormatter {
        let f = DateFormatter()
        f.timeZone = timeZone
        let o = formatOptions
        var date = ""
        if o.contains(.withYear) { date += "yyyy" }
        if o.contains(.withMonth) { date += (o.contains(.withDashSeparatorInDate) && !date.isEmpty ? "-" : "") + "MM" }
        if o.contains(.withDay) { date += (o.contains(.withDashSeparatorInDate) && !date.isEmpty ? "-" : "") + "dd" }
        var time = ""
        if o.contains(.withTime) {
            let c = o.contains(.withColonSeparatorInTime) ? ":" : ""
            time = "HH\(c)mm\(c)ss" + (o.contains(.withFractionalSeconds) ? ".SSS" : "")
        }
        if o.contains(.withTimeZone) { time += o.contains(.withColonSeparatorInTimeZone) ? "XXXXX" : "XXXX" }
        let sep = o.contains(.withSpaceBetweenDateAndTime) ? " " : "'T'"
        f.dateFormat = [date, time].filter { !$0.isEmpty }.joined(separator: sep)
        return f
    }
    open func string(from date: Date) -> String { _formatter.string(from: date) }
    open func date(from string: String) -> Date? { _formatter.date(from: string) }
    open override func string(for obj: Any?) -> String? { (obj as? Date).map { string(from: $0) } }
    open class func string(from date: Date, timeZone: TimeZone, formatOptions: Options = []) -> String {
        let f = ISO8601DateFormatter()
        f.timeZone = timeZone
        if !formatOptions.isEmpty { f.formatOptions = formatOptions }
        return f.string(from: date)
    }
}

open class DateIntervalFormatter: Formatter, @unchecked Sendable {
    public typealias Style = DateFormatter.Style
    open var dateStyle: Style = .medium
    open var timeStyle: Style = .medium
    open var dateTemplate: String! = ""
    open var locale: Locale! = .current
    open var calendar: Calendar! = Calendar(identifier: .gregorian)
    open var timeZone: TimeZone! = .current
    private func formatter() -> DateFormatter {
        let f = DateFormatter()
        f.dateStyle = dateStyle; f.timeStyle = timeStyle; f.locale = locale; f.calendar = calendar; f.timeZone = timeZone
        if let t = dateTemplate, !t.isEmpty { f.dateFormat = t }
        return f
    }
    open func string(from fromDate: Date, to toDate: Date) -> String {
        let f = formatter()
        return f.string(from: fromDate) + " – " + f.string(from: toDate)
    }
    open func string(from dateInterval: DateInterval) -> String? { string(from: dateInterval.start, to: dateInterval.end) }
    open override func string(for obj: Any?) -> String? { (obj as? DateInterval).flatMap { string(from: $0) } }
}

/// Base dos formatadores de grandeza (massa, comprimento, energia): número + símbolo da unidade.
open class _MOSUnitFormatter: Formatter, @unchecked Sendable {
    open var numberFormatter: NumberFormatter! = {
        let f = NumberFormatter(); f.numberStyle = .decimal; return f
    }()
    open var unitStyle: UnitStyle = .medium
    open var isForPersonUse = false
    func _string(_ value: Double, _ short: String, _ long: String) -> String {
        let n = numberFormatter.string(from: NSNumber(value: value)) ?? "\(value)"
        return unitStyle == .long ? "\(n) \(long)" : "\(n) \(short)"
    }
}

open class MassFormatter: _MOSUnitFormatter, @unchecked Sendable {
    public enum Unit: Int, Sendable { case gram = 11, kilogram = 14, ounce = 1537, pound = 1538, stone = 1539 }
    open func string(fromValue value: Double, unit: Unit) -> String {
        switch unit {
        case .gram: _string(value, "g", "grams")
        case .kilogram: _string(value, "kg", "kilograms")
        case .ounce: _string(value, "oz", "ounces")
        case .pound: _string(value, "lb", "pounds")
        case .stone: _string(value, "st", "stones")
        }
    }
    open func string(fromKilograms numberInKilograms: Double) -> String { string(fromValue: numberInKilograms, unit: .kilogram) }
    open override func string(for obj: Any?) -> String? { (obj as? NSNumber).map { string(fromKilograms: $0.doubleValue) } }
}

open class LengthFormatter: _MOSUnitFormatter, @unchecked Sendable {
    public enum Unit: Int, Sendable { case millimeter = 8, centimeter = 9, meter = 11, kilometer = 14, inch = 1281, foot = 1282, yard = 1283, mile = 1284 }
    open func string(fromValue value: Double, unit: Unit) -> String {
        switch unit {
        case .millimeter: _string(value, "mm", "millimeters")
        case .centimeter: _string(value, "cm", "centimeters")
        case .meter: _string(value, "m", "meters")
        case .kilometer: _string(value, "km", "kilometers")
        case .inch: _string(value, "in", "inches")
        case .foot: _string(value, "ft", "feet")
        case .yard: _string(value, "yd", "yards")
        case .mile: _string(value, "mi", "miles")
        }
    }
    open func string(fromMeters numberInMeters: Double) -> String { string(fromValue: numberInMeters, unit: .meter) }
    open override func string(for obj: Any?) -> String? { (obj as? NSNumber).map { string(fromMeters: $0.doubleValue) } }
}

open class EnergyFormatter: _MOSUnitFormatter, @unchecked Sendable {
    public enum Unit: Int, Sendable { case joule = 11, kilojoule = 14, calorie = 1793, kilocalorie = 1794 }
    open func string(fromValue value: Double, unit: Unit) -> String {
        switch unit {
        case .joule: _string(value, "J", "joules")
        case .kilojoule: _string(value, "kJ", "kilojoules")
        case .calorie: _string(value, "cal", "calories")
        case .kilocalorie: _string(value, "kcal", "kilocalories")
        }
    }
    open func string(fromJoules numberInJoules: Double) -> String { string(fromValue: numberInJoules, unit: .joule) }
    open override func string(for obj: Any?) -> String? { (obj as? NSNumber).map { string(fromJoules: $0.doubleValue) } }
}

// MARK: - NSCalendar

open class NSCalendar: NSObject, @unchecked Sendable {
    public struct Unit: OptionSet, Sendable, Hashable {
        public let rawValue: UInt
        public init(rawValue: UInt) { self.rawValue = rawValue }
        public static let era = Unit(rawValue: 1 << 1)
        public static let year = Unit(rawValue: 1 << 2)
        public static let month = Unit(rawValue: 1 << 3)
        public static let day = Unit(rawValue: 1 << 4)
        public static let hour = Unit(rawValue: 1 << 5)
        public static let minute = Unit(rawValue: 1 << 6)
        public static let second = Unit(rawValue: 1 << 7)
        public static let weekday = Unit(rawValue: 1 << 9)
        public static let weekdayOrdinal = Unit(rawValue: 1 << 10)
        public static let quarter = Unit(rawValue: 1 << 11)
        public static let weekOfMonth = Unit(rawValue: 1 << 12)
        public static let weekOfYear = Unit(rawValue: 1 << 13)
        public static let yearForWeekOfYear = Unit(rawValue: 1 << 14)
        public static let nanosecond = Unit(rawValue: 1 << 15)
        public static let dayOfYear = Unit(rawValue: 1 << 16)
        public static let calendar = Unit(rawValue: 1 << 20)
        public static let timeZone = Unit(rawValue: 1 << 21)
    }
    public let _calendar: Calendar
    public init(calendar: Calendar) { _calendar = calendar }
    public convenience init?(identifier: Calendar.Identifier) { self.init(calendar: Calendar(identifier: identifier)) }
    open class var current: Calendar { .current }
}

// MARK: - IndexSet

/// Conjunto ordenado de inteiros não negativos (como o IndexSet do Foundation), guardado como faixas disjuntas.
public struct IndexSet: Hashable, Sendable, BidirectionalCollection, SetAlgebra, ExpressibleByArrayLiteral, CustomStringConvertible, Codable {
    public typealias Element = Int
    private var ranges: [Range<Int>] = []     // ordenadas, disjuntas, não adjacentes

    public init() {}
    public init(integer: Int) { insert(integer) }
    public init(integersIn range: Range<Int>) { if !range.isEmpty { ranges = [range] } }
    public init(integersIn range: ClosedRange<Int>) { self.init(integersIn: range.lowerBound ..< range.upperBound + 1) }
    public init(arrayLiteral elements: Int...) { for e in elements { insert(e) } }
    public init<S: Sequence>(_ s: S) where S.Element == Int { for e in s { insert(e) } }

    public struct Index: Comparable, Hashable, Sendable {
        let r: Int, v: Int
        public static func < (a: Index, b: Index) -> Bool { (a.r, a.v) < (b.r, b.v) }
    }
    public var startIndex: Index { ranges.isEmpty ? endIndex : Index(r: 0, v: ranges[0].lowerBound) }
    public var endIndex: Index { Index(r: ranges.count, v: 0) }
    public subscript(i: Index) -> Int { i.v }
    public func index(after i: Index) -> Index {
        let n = i.v + 1
        if n < ranges[i.r].upperBound { return Index(r: i.r, v: n) }
        return i.r + 1 < ranges.count ? Index(r: i.r + 1, v: ranges[i.r + 1].lowerBound) : endIndex
    }
    public func index(before i: Index) -> Index {
        if i.r < ranges.count, i.v > ranges[i.r].lowerBound { return Index(r: i.r, v: i.v - 1) }
        let r = i.r - 1
        return Index(r: r, v: ranges[r].upperBound - 1)
    }
    public var count: Int { ranges.reduce(0) { $0 + $1.count } }
    public var isEmpty: Bool { ranges.isEmpty }
    public var first: Int? { ranges.first?.lowerBound }
    public var last: Int? { ranges.last.map { $0.upperBound - 1 } }
    public var rangeView: [Range<Int>] { ranges }

    public func contains(_ v: Int) -> Bool { ranges.contains { $0.contains(v) } }
    public func contains(integersIn r: Range<Int>) -> Bool { r.isEmpty || ranges.contains { $0.lowerBound <= r.lowerBound && $0.upperBound >= r.upperBound } }

    public mutating func insert(integersIn r: Range<Int>) {
        guard !r.isEmpty else { return }
        var lo = r.lowerBound, hi = r.upperBound
        var out: [Range<Int>] = []
        var placed = false
        for x in ranges {
            if x.upperBound < lo { out.append(x) }
            else if x.lowerBound > hi { if !placed { out.append(lo ..< hi); placed = true }; out.append(x) }
            else { lo = Swift.min(lo, x.lowerBound); hi = Swift.max(hi, x.upperBound) }
        }
        if !placed { out.append(lo ..< hi) }
        ranges = out
    }
    public mutating func insert(integersIn r: ClosedRange<Int>) { insert(integersIn: r.lowerBound ..< r.upperBound + 1) }
    public mutating func remove(integersIn r: Range<Int>) {
        guard !r.isEmpty else { return }
        var out: [Range<Int>] = []
        for x in ranges {
            if x.upperBound <= r.lowerBound || x.lowerBound >= r.upperBound { out.append(x); continue }
            if x.lowerBound < r.lowerBound { out.append(x.lowerBound ..< r.lowerBound) }
            if x.upperBound > r.upperBound { out.append(r.upperBound ..< x.upperBound) }
        }
        ranges = out
    }
    public mutating func removeAll() { ranges = [] }

    @discardableResult public mutating func insert(_ v: Int) -> (inserted: Bool, memberAfterInsert: Int) {
        let had = contains(v)
        insert(integersIn: v ..< v + 1)
        return (!had, v)
    }
    @discardableResult public mutating func remove(_ v: Int) -> Int? {
        guard contains(v) else { return nil }
        remove(integersIn: v ..< v + 1)
        return v
    }
    @discardableResult public mutating func update(with v: Int) -> Int? { insert(v).inserted ? nil : v }
    public func union(_ o: IndexSet) -> IndexSet { var r = self; r.formUnion(o); return r }
    public func intersection(_ o: IndexSet) -> IndexSet { IndexSet(filter { o.contains($0) }) }
    public func symmetricDifference(_ o: IndexSet) -> IndexSet { union(o).subtracting(intersection(o)) }
    public mutating func formUnion(_ o: IndexSet) { for r in o.ranges { insert(integersIn: r) } }
    public mutating func formIntersection(_ o: IndexSet) { self = intersection(o) }
    public mutating func formSymmetricDifference(_ o: IndexSet) { self = symmetricDifference(o) }
    public func subtracting(_ o: IndexSet) -> IndexSet { var r = self; for x in o.ranges { r.remove(integersIn: x) }; return r }

    public func integerGreaterThan(_ v: Int) -> Int? { first { $0 > v } }
    public func integerLessThan(_ v: Int) -> Int? { reversed().first { $0 < v } }
    /// Desloca os índices a partir de `index` por `delta` (como shift(startingAt:by:)).
    public mutating func shift(startingAt index: Int, by delta: Int) {
        var out = IndexSet()
        for v in self {
            if v < index { out.insert(v) } else if v + delta >= index || delta > 0 { if v + delta >= 0 { out.insert(v + delta) } }
        }
        self = out
    }

    public var description: String { "\(count) indexes" }
    public init(from decoder: any Decoder) throws { self.init(try [Int](from: decoder)) }
    public func encode(to encoder: any Encoder) throws { try Array(self).encode(to: encoder) }
}

/// Letra ASCII (os padrões de data são ASCII). As propriedades Unicode de Character (isLetter, wholeNumberValue...)
/// dependem das tabelas Unicode do stdlib, que o stdlib do mOS ainda não traz.
@inline(__always) func _mosIsASCIILetter(_ c: Character) -> Bool {
    guard let a = c.asciiValue else { return false }
    return (a >= 65 && a <= 90) || (a >= 97 && a <= 122)
}