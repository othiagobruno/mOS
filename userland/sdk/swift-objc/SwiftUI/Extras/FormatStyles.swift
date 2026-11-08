// mOS — cópia de userland/sdk/openswiftui/swift/Foundation/FormatStyles.swift para o SwiftUI.framework (variante com
// interop ObjC): os estilos de formatação do FoundationInternationalization (ICU) que o OpenSwiftUI usa, sem ICU.
// mOS SDK — estilos de formatação que o OpenSwiftUI usa e que moram no FoundationInternationalization
// (ICU) da Apple. Sem ICU: datas e durações saem em inglês fixo, com o calendário gregoriano do
// FoundationEssentials. A forma da API é a do Foundation (FormatStyle, DiscreteFormatStyle, Codable).

@_spi(SwiftCorelibsFoundation) import Foundation

// MARK: - capitalização e agrupamento

public struct FormatStyleCapitalizationContext: Hashable, Codable, Sendable {
    public enum Option: Int, Hashable, Codable, Sendable {
        case unknown, standalone, listItem, beginningOfSentence, middleOfSentence
    }
    public var option: Option
    public init(option: Option) { self.option = option }
    public static var unknown: FormatStyleCapitalizationContext { .init(option: .unknown) }
    public static var standalone: FormatStyleCapitalizationContext { .init(option: .standalone) }
    public static var listItem: FormatStyleCapitalizationContext { .init(option: .listItem) }
    public static var beginningOfSentence: FormatStyleCapitalizationContext { .init(option: .beginningOfSentence) }
    public static var middleOfSentence: FormatStyleCapitalizationContext { .init(option: .middleOfSentence) }
}

public enum NumberFormatStyleConfiguration {
    public struct Grouping: Hashable, Codable, Sendable {
        public enum Option: Int, Hashable, Codable, Sendable { case automatic, never }
        public var option: Option
        public init(option: Option) { self.option = option }
        public static var automatic: Grouping { .init(option: .automatic) }
        public static var never: Grouping { .init(option: .never) }
    }
}

// MARK: - Locale.Language (o swift-foundation guarda o código só em Components)

private let _mosRTL: Set<String> = ["ar", "he", "fa", "ur", "yi", "ps", "sd", "ug", "dv", "ckb"]

private func _mosRoundRaw(_ rule: FloatingPointRoundingRule) -> Int {
    switch rule {
    case .toNearestOrEven: 0
    case .toNearestOrAwayFromZero: 1
    case .up: 2
    case .down: 3
    case .towardZero: 4
    case .awayFromZero: 5
    @unknown default: 0
    }
}
private func _mosRound(_ raw: Int) -> FloatingPointRoundingRule {
    switch raw {
    case 1: .toNearestOrAwayFromZero
    case 2: .up
    case 3: .down
    case 4: .towardZero
    case 5: .awayFromZero
    default: .toNearestOrEven
    }
}

extension Locale.Language {
    public var languageCode: Locale.LanguageCode? { _components.languageCode }
    public var script: Locale.Script? { _components.script }
    public var region: Locale.Region? { _components.region }
    public var maximalIdentifier: String {
        let id = _components._identifier
        return id.isEmpty ? "en" : id
    }
    public var characterDirection: Locale.LanguageDirection {
        guard let code = languageCode?.identifier.lowercased(), !code.isEmpty else { return .unknown }
        return _mosRTL.contains(code) ? .rightToLeft : .leftToRight
    }
    public init(identifier: String) {
        let parts = identifier.split { $0 == "-" || $0 == "_" }.map(String.init)
        let code = parts.first.map { Locale.LanguageCode($0) }
        let region = parts.count > 2 ? Locale.Region(parts[2]) : (parts.count > 1 ? Locale.Region(parts[1]) : nil)
        self.init(languageCode: code, script: nil, region: region)
    }
}

extension AttributedString {
    public struct AdaptiveImageGlyph: Hashable, Codable, Sendable {
        public var content: Data
        public init(content: Data = Data()) { self.content = content }
    }
}

// MARK: - números

public struct FloatingPointFormatStyle<Value: BinaryFloatingPoint>: Hashable, Codable, Sendable {
    public var locale: Locale
    public init(locale: Locale = .autoupdatingCurrent) { self.locale = locale }
    public struct Percent: Hashable, Codable, Sendable {
        public var locale: Locale
        public var roundingRaw: Int
        public var increment: Double?
        public init(locale: Locale = .autoupdatingCurrent) {
            self.locale = locale
            self.roundingRaw = 0
            self.increment = nil
        }
        public func rounded(rule: FloatingPointRoundingRule = .toNearestOrEven, increment: Double? = nil) -> Self {
            var copy = self
            copy.roundingRaw = _mosRoundRaw(rule)
            copy.increment = increment
            return copy
        }
        public var attributed: Attributed { Attributed(base: self) }
        public struct Attributed: Hashable, Codable, Sendable {
            public var base: FloatingPointFormatStyle<Value>.Percent
        }
    }
}

extension FloatingPointFormatStyle: FormatStyle {
    public func format(_ value: Value) -> String { String(format: "%g", Double(value)) }
    public func locale(_ locale: Locale) -> Self { var copy = self; copy.locale = locale; return copy }
}

extension FloatingPointFormatStyle.Percent: FormatStyle {
    public func format(_ value: Value) -> String {
        var n = Double(value) * 100
        if let increment, increment != 0 {
            n = (n / increment).rounded(_mosRound(roundingRaw)) * increment
        }
        let text = n.rounded() == n ? String(Int(n)) : String(format: "%.1f", n)
        return text + "%"
    }
    public func locale(_ locale: Locale) -> Self { var copy = self; copy.locale = locale; return copy }
}

extension FloatingPointFormatStyle.Percent.Attributed: FormatStyle {
    public func format(_ value: Value) -> AttributedString { AttributedString(base.format(value)) }
    public func locale(_ locale: Locale) -> Self { var copy = self; copy.base = base.locale(locale); return copy }
}

extension BinaryFloatingPoint {
    public func formatted<S: FormatStyle>(_ format: S) -> S.FormatOutput where S.FormatInput == Self {
        format.format(self)
    }
}

// MARK: - Date.ComponentsFormatStyle.Field

extension Date {
    public struct ComponentsFormatStyle: Hashable, Codable, Sendable {
        public struct Field: Hashable, Codable, Sendable {
            public enum Option: Int, Hashable, Codable, Sendable {
                case year, month, week, day, hour, minute, second
            }
            public var option: Option
            public init(option: Option) { self.option = option }
            public static var year: Field { .init(option: .year) }
            public static var month: Field { .init(option: .month) }
            public static var week: Field { .init(option: .week) }
            public static var day: Field { .init(option: .day) }
            public static var hour: Field { .init(option: .hour) }
            public static var minute: Field { .init(option: .minute) }
            public static var second: Field { .init(option: .second) }
        }
        public struct Style: Hashable, Codable, Sendable {
            public enum Option: Int, Hashable, Codable, Sendable { case wide, abbreviated, condensedAbbreviated, narrow }
            public var option: Option
            public static var wide: Style { .init(option: .wide) }
            public static var abbreviated: Style { .init(option: .abbreviated) }
            public static var condensedAbbreviated: Style { .init(option: .condensedAbbreviated) }
            public static var narrow: Style { .init(option: .narrow) }
        }
        public var fields: Set<Field>
        public var style: Style
        public var locale: Locale
        public init(fields: Set<Field> = [], style: Style = .wide, locale: Locale = .autoupdatingCurrent) {
            self.fields = fields
            self.style = style
            self.locale = locale
        }
    }
}

// MARK: - Date.FormatStyle

extension Date {
    public struct FormatStyle: Hashable, Codable, Sendable {
        public struct DateStyle: Hashable, Codable, Sendable {
            public var rawValue: UInt
            public init(rawValue: UInt) { self.rawValue = rawValue }
            public static let omitted = DateStyle(rawValue: 0)
            public static let numeric = DateStyle(rawValue: 1)
            public static let abbreviated = DateStyle(rawValue: 2)
            public static let long = DateStyle(rawValue: 3)
            public static let complete = DateStyle(rawValue: 4)
        }
        public struct TimeStyle: Hashable, Codable, Sendable {
            public var rawValue: UInt
            public init(rawValue: UInt) { self.rawValue = rawValue }
            public static let omitted = TimeStyle(rawValue: 0)
            public static let shortened = TimeStyle(rawValue: 1)
            public static let standard = TimeStyle(rawValue: 2)
            public static let complete = TimeStyle(rawValue: 3)
        }
        public enum Symbol {
            public struct Era: Hashable, Codable, Sendable { public var on: Bool; public static var omitted: Era { .init(on: false) }; public static var abbreviated: Era { .init(on: true) } }
            public struct Quarter: Hashable, Codable, Sendable { public var on: Bool; public static var omitted: Quarter { .init(on: false) }; public static var abbreviated: Quarter { .init(on: true) } }
            public struct Year: Hashable, Codable, Sendable { public var on: Bool; public static var omitted: Year { .init(on: false) }; public static var defaultDigits: Year { .init(on: true) } }
            public struct Month: Hashable, Codable, Sendable {
                public enum Kind: Int, Hashable, Codable, Sendable { case omitted, digits, wide, abbreviated }
                public var kind: Kind
                public static var omitted: Month { .init(kind: .omitted) }
                public static var wide: Month { .init(kind: .wide) }
                public static var abbreviated: Month { .init(kind: .abbreviated) }
            }
            public struct Week: Hashable, Codable, Sendable { public var on: Bool; public static var omitted: Week { .init(on: false) }; public static var defaultDigits: Week { .init(on: true) } }
            public struct Day: Hashable, Codable, Sendable { public var on: Bool; public static var omitted: Day { .init(on: false) }; public static var defaultDigits: Day { .init(on: true) } }
            public struct DayOfYear: Hashable, Codable, Sendable { public var on: Bool; public static var omitted: DayOfYear { .init(on: false) }; public static var defaultDigits: DayOfYear { .init(on: true) } }
            public struct Weekday: Hashable, Codable, Sendable { public var on: Bool; public static var omitted: Weekday { .init(on: false) }; public static var abbreviated: Weekday { .init(on: true) } }
            public struct Hour: Hashable, Codable, Sendable {
                public enum Kind: Int, Hashable, Codable, Sendable { case omitted, digits, digitsAMPM }
                public var kind: Kind
                public struct AMPMStyle: Hashable, Codable, Sendable {
                    public var rawValue: UInt
                    public init(rawValue: UInt) { self.rawValue = rawValue }
                    public static let omitted = AMPMStyle(rawValue: 0)
                    public static let narrow = AMPMStyle(rawValue: 1)
                    public static let abbreviated = AMPMStyle(rawValue: 2)
                    public static let wide = AMPMStyle(rawValue: 3)
                }
                public static var omitted: Hour { .init(kind: .omitted) }
                public static var defaultDigitsNoAMPM: Hour { .init(kind: .digits) }
                public static func defaultDigits(amPM: AMPMStyle) -> Hour { .init(kind: amPM == .omitted ? .digits : .digitsAMPM) }
            }
            public struct Minute: Hashable, Codable, Sendable { public var on: Bool; public static var omitted: Minute { .init(on: false) }; public static var defaultDigits: Minute { .init(on: true) } }
            public struct Second: Hashable, Codable, Sendable { public var on: Bool; public static var omitted: Second { .init(on: false) }; public static var defaultDigits: Second { .init(on: true) } }
            public struct SecondFraction: Hashable, Codable, Sendable { public var on: Bool; public static var omitted: SecondFraction { .init(on: false) } }
            public struct TimeZone: Hashable, Codable, Sendable { public var on: Bool; public static var omitted: TimeZone { .init(on: false) }; public static var abbreviated: TimeZone { .init(on: true) } }
        }

        public var locale: Locale
        public var timeZone: Foundation.TimeZone
        public var calendar: Calendar
        public var capitalizationContext: FormatStyleCapitalizationContext
        var dateStyle: DateStyle?
        var timeStyle: TimeStyle?
        var era: Symbol.Era = .omitted
        var quarter: Symbol.Quarter = .omitted
        var year: Symbol.Year = .omitted
        var month: Symbol.Month = .omitted
        var week: Symbol.Week = .omitted
        var day: Symbol.Day = .omitted
        var dayOfYear: Symbol.DayOfYear = .omitted
        var weekday: Symbol.Weekday = .omitted
        var hour: Symbol.Hour = .omitted
        var minute: Symbol.Minute = .omitted
        var second: Symbol.Second = .omitted
        var secondFraction: Symbol.SecondFraction = .omitted
        var timeZoneSymbol: Symbol.TimeZone = .omitted
        var symbolsSet = false

        public init(date: DateStyle? = nil, time: TimeStyle? = nil, locale: Locale = .autoupdatingCurrent, calendar: Calendar = .autoupdatingCurrent, timeZone: Foundation.TimeZone = .autoupdatingCurrent, capitalizationContext: FormatStyleCapitalizationContext = .unknown) {
            self.locale = locale
            self.calendar = calendar
            self.timeZone = timeZone
            self.capitalizationContext = capitalizationContext
            self.dateStyle = date
            self.timeStyle = time
        }

        public var attributedStyle: Attributed { .init(style: self) }

        public func era(_ format: Symbol.Era = .abbreviated) -> Self { var s = self; s.symbolsSet = true; s.era = format; return s }
        public func quarter(_ format: Symbol.Quarter = .abbreviated) -> Self { var s = self; s.symbolsSet = true; s.quarter = format; return s }
        public func year(_ format: Symbol.Year = .defaultDigits) -> Self { var s = self; s.symbolsSet = true; s.year = format; return s }
        public func month(_ format: Symbol.Month = .abbreviated) -> Self { var s = self; s.symbolsSet = true; s.month = format; return s }
        public func week(_ format: Symbol.Week = .defaultDigits) -> Self { var s = self; s.symbolsSet = true; s.week = format; return s }
        public func day(_ format: Symbol.Day = .defaultDigits) -> Self { var s = self; s.symbolsSet = true; s.day = format; return s }
        public func dayOfYear(_ format: Symbol.DayOfYear = .defaultDigits) -> Self { var s = self; s.symbolsSet = true; s.dayOfYear = format; return s }
        public func weekday(_ format: Symbol.Weekday = .abbreviated) -> Self { var s = self; s.symbolsSet = true; s.weekday = format; return s }
        public func hour(_ format: Symbol.Hour = .defaultDigits(amPM: .abbreviated)) -> Self { var s = self; s.symbolsSet = true; s.hour = format; return s }
        public func minute(_ format: Symbol.Minute = .defaultDigits) -> Self { var s = self; s.symbolsSet = true; s.minute = format; return s }
        public func second(_ format: Symbol.Second = .defaultDigits) -> Self { var s = self; s.symbolsSet = true; s.second = format; return s }
        public func secondFraction(_ format: Symbol.SecondFraction) -> Self { var s = self; s.symbolsSet = true; s.secondFraction = format; return s }
        public func timeZone(_ format: Symbol.TimeZone = .abbreviated) -> Self { var s = self; s.symbolsSet = true; s.timeZoneSymbol = format; return s }

        func render(_ value: Date) -> String {
            var cal = calendar
            cal.timeZone = timeZone
            let c = cal.dateComponents([.year, .month, .day, .hour, .minute, .second, .weekday], from: value)
            if symbolsSet {
                var parts: [String] = []
                if weekday.on, let w = c.weekday { parts.append(_mosWeekday(w, wide: false)) }
                if month.kind != .omitted, let m = c.month {
                    parts.append(month.kind == .wide ? _mosMonth(m, wide: true) : (month.kind == .abbreviated ? _mosMonth(m, wide: false) : String(m)))
                }
                if day.on, let d = c.day { parts.append(String(d)) }
                if year.on, let y = c.year { parts.append(String(y)) }
                var time = ""
                if hour.kind != .omitted, let h = c.hour {
                    let h12 = hour.kind == .digitsAMPM
                    let hourValue = h12 ? (h % 12 == 0 ? 12 : h % 12) : h
                    time = String(hourValue)
                    if minute.on, let mi = c.minute { time += ":" + _mosPad(mi) }
                    if second.on, let s = c.second { time += ":" + _mosPad(s) }
                    if h12 { time += h < 12 ? " AM" : " PM" }
                } else if minute.on, let mi = c.minute {
                    time = _mosPad(mi)
                    if second.on, let s = c.second { time += ":" + _mosPad(s) }
                }
                if !time.isEmpty { parts.append(time) }
                return parts.joined(separator: " ")
            }
            var parts: [String] = []
            if let dateStyle, dateStyle != .omitted {
                let y = c.year ?? 0, m = c.month ?? 1, d = c.day ?? 1
                if dateStyle == .numeric { parts.append("\(m)/\(d)/\(y)") }
                else if dateStyle == .abbreviated { parts.append("\(_mosMonth(m, wide: false)) \(d), \(y)") }
                else if dateStyle == .long { parts.append("\(_mosMonth(m, wide: true)) \(d), \(y)") }
                else if dateStyle == .complete { parts.append("\(_mosWeekday(c.weekday ?? 1, wide: true)), \(_mosMonth(m, wide: true)) \(d), \(y)") }
            }
            if let timeStyle, timeStyle != .omitted, let h = c.hour {
                let h12 = h % 12 == 0 ? 12 : h % 12
                var t = "\(h12):\(_mosPad(c.minute ?? 0))"
                if timeStyle == .standard || timeStyle == .complete { t += ":\(_mosPad(c.second ?? 0))" }
                t += h < 12 ? " AM" : " PM"
                parts.append(t)
            }
            return parts.joined(separator: ", ")
        }
    }
}

extension Date.FormatStyle: FormatStyle {
    public func format(_ value: Date) -> String { render(value) }
    public func locale(_ locale: Locale) -> Self { var copy = self; copy.locale = locale; return copy }
}

extension Date.FormatStyle: DiscreteFormatStyle {
    public func discreteInput(before input: Date) -> Date? { input.addingTimeInterval(-1) }
    public func discreteInput(after input: Date) -> Date? { input.addingTimeInterval(1) }
    public func input(before input: Date) -> Date? { discreteInput(before: input) }
    public func input(after input: Date) -> Date? { discreteInput(after: input) }
}

extension Date.FormatStyle {
    @dynamicMemberLookup
    public struct Attributed: Hashable, Codable, Sendable {
        public var base: Date.FormatStyle
        public init(style: Date.FormatStyle) { self.base = style }
        public subscript<T>(dynamicMember key: KeyPath<Date.FormatStyle, T>) -> T { base[keyPath: key] }
        public subscript<T>(dynamicMember key: WritableKeyPath<Date.FormatStyle, T>) -> T {
            get { base[keyPath: key] }
            set { base[keyPath: key] = newValue }
        }
        public func era(_ format: Date.FormatStyle.Symbol.Era = .abbreviated) -> Self { var s = self; s.base = base.era(format); return s }
        public func quarter(_ format: Date.FormatStyle.Symbol.Quarter = .abbreviated) -> Self { var s = self; s.base = base.quarter(format); return s }
        public func year(_ format: Date.FormatStyle.Symbol.Year = .defaultDigits) -> Self { var s = self; s.base = base.year(format); return s }
        public func month(_ format: Date.FormatStyle.Symbol.Month = .abbreviated) -> Self { var s = self; s.base = base.month(format); return s }
        public func week(_ format: Date.FormatStyle.Symbol.Week = .defaultDigits) -> Self { var s = self; s.base = base.week(format); return s }
        public func day(_ format: Date.FormatStyle.Symbol.Day = .defaultDigits) -> Self { var s = self; s.base = base.day(format); return s }
        public func dayOfYear(_ format: Date.FormatStyle.Symbol.DayOfYear = .defaultDigits) -> Self { var s = self; s.base = base.dayOfYear(format); return s }
        public func weekday(_ format: Date.FormatStyle.Symbol.Weekday = .abbreviated) -> Self { var s = self; s.base = base.weekday(format); return s }
        public func hour(_ format: Date.FormatStyle.Symbol.Hour = .defaultDigits(amPM: .abbreviated)) -> Self { var s = self; s.base = base.hour(format); return s }
        public func minute(_ format: Date.FormatStyle.Symbol.Minute = .defaultDigits) -> Self { var s = self; s.base = base.minute(format); return s }
        public func second(_ format: Date.FormatStyle.Symbol.Second = .defaultDigits) -> Self { var s = self; s.base = base.second(format); return s }
        public func secondFraction(_ format: Date.FormatStyle.Symbol.SecondFraction) -> Self { var s = self; s.base = base.secondFraction(format); return s }
        public func timeZone(_ format: Date.FormatStyle.Symbol.TimeZone = .abbreviated) -> Self { var s = self; s.base = base.timeZone(format); return s }
    }
}

extension Date.FormatStyle.Attributed: FormatStyle {
    public func format(_ value: Date) -> AttributedString { AttributedString(base.render(value)) }
    public func locale(_ locale: Locale) -> Self { var copy = self; copy.base = base.locale(locale); return copy }
}

extension Date.FormatStyle.Attributed: DiscreteFormatStyle {
    public func discreteInput(before input: Date) -> Date? { base.discreteInput(before: input) }
    public func discreteInput(after input: Date) -> Date? { base.discreteInput(after: input) }
    public func input(before input: Date) -> Date? { base.input(before: input) }
    public func input(after input: Date) -> Date? { base.input(after: input) }
}

// MARK: - Date.VerbatimFormatStyle

extension Date {
    public struct VerbatimFormatStyle: Hashable, Codable, Sendable {
        public var timeZone: Foundation.TimeZone
        public var calendar: Calendar
        public var locale: Locale?
        var formatPattern: String
        public init(format: String, locale: Locale? = nil, timeZone: Foundation.TimeZone, calendar: Calendar) {
            self.formatPattern = format
            self.locale = locale
            self.timeZone = timeZone
            self.calendar = calendar
        }
        public var attributedStyle: Attributed { .init(base: .init(formatPattern: formatPattern, locale: locale, calendar: calendar, timeZone: timeZone)) }
        public func format(_ value: Date) -> String { Date.FormatStyle(locale: locale ?? .autoupdatingCurrent, calendar: calendar, timeZone: timeZone).render(value) }
        public func locale(_ locale: Locale) -> Self { var copy = self; copy.locale = locale; return copy }
    }
}

extension Date.VerbatimFormatStyle: FormatStyle {}
extension Date.VerbatimFormatStyle: DiscreteFormatStyle {
    public func discreteInput(before input: Date) -> Date? { input.addingTimeInterval(-1) }
    public func discreteInput(after input: Date) -> Date? { input.addingTimeInterval(1) }
    public func input(before input: Date) -> Date? { discreteInput(before: input) }
    public func input(after input: Date) -> Date? { discreteInput(after: input) }
}

extension Date.VerbatimFormatStyle {
    @dynamicMemberLookup
    public struct Attributed: Hashable, Codable, Sendable {
        public struct Base: Hashable, Codable, Sendable {
            public var formatPattern: String
            public var locale: Locale?
            public var calendar: Calendar
            public var timeZone: Foundation.TimeZone
        }
        public var base: Base
        public subscript<T>(dynamicMember key: KeyPath<Base, T>) -> T { base[keyPath: key] }
        public subscript<T>(dynamicMember key: WritableKeyPath<Base, T>) -> T {
            get { base[keyPath: key] }
            set { base[keyPath: key] = newValue }
        }
    }
}

extension Date.VerbatimFormatStyle.Attributed: FormatStyle {
    public func format(_ value: Date) -> AttributedString {
        let style = Date.VerbatimFormatStyle(format: base.formatPattern, locale: base.locale, timeZone: base.timeZone, calendar: base.calendar)
        return AttributedString(style.format(value))
    }
    public func locale(_ locale: Locale) -> Self { var copy = self; copy.base.locale = locale; return copy }
}

extension Date.VerbatimFormatStyle.Attributed: DiscreteFormatStyle {
    public func discreteInput(before input: Date) -> Date? { input.addingTimeInterval(-1) }
    public func discreteInput(after input: Date) -> Date? { input.addingTimeInterval(1) }
    public func input(before input: Date) -> Date? { discreteInput(before: input) }
    public func input(after input: Date) -> Date? { discreteInput(after: input) }
}

// MARK: - datas relativas

extension Date {
    public struct RelativeFormatStyle: Hashable, Codable, Sendable {
        public typealias Field = Date.ComponentsFormatStyle.Field
        public struct UnitsStyle: Hashable, Codable, Sendable {
            public enum Option: Int, Hashable, Codable, Sendable { case wide, spellOut, abbreviated, narrow }
            public var option: Option
            public static var wide: UnitsStyle { .init(option: .wide) }
            public static var spellOut: UnitsStyle { .init(option: .spellOut) }
            public static var abbreviated: UnitsStyle { .init(option: .abbreviated) }
            public static var narrow: UnitsStyle { .init(option: .narrow) }
        }
        public struct Presentation: Hashable, Codable, Sendable {
            public enum Option: Int, Hashable, Codable, Sendable { case numeric, named }
            public var option: Option
            public static var numeric: Presentation { .init(option: .numeric) }
            public static var named: Presentation { .init(option: .named) }
        }
        public var presentation: Presentation
        public var unitsStyle: UnitsStyle
        public var capitalizationContext: FormatStyleCapitalizationContext
        public var locale: Locale
        public var calendar: Calendar
        public var allowedFields: Set<Field>
        public init(presentation: Presentation = .numeric, unitsStyle: UnitsStyle = .wide, locale: Locale = .autoupdatingCurrent, calendar: Calendar = .autoupdatingCurrent, capitalizationContext: FormatStyleCapitalizationContext = .unknown) {
            self.presentation = presentation
            self.unitsStyle = unitsStyle
            self.capitalizationContext = capitalizationContext
            self.locale = locale
            self.calendar = calendar
            self.allowedFields = [.year, .month, .week, .day, .hour, .minute, .second]
        }
        public init(allowedFields: Set<Field>, presentation: Presentation = .numeric, unitsStyle: UnitsStyle = .wide, locale: Locale = .autoupdatingCurrent, calendar: Calendar = .autoupdatingCurrent, capitalizationContext: FormatStyleCapitalizationContext = .unknown) {
            self.init(presentation: presentation, unitsStyle: unitsStyle, locale: locale, calendar: calendar, capitalizationContext: capitalizationContext)
            self.allowedFields = allowedFields
        }
    }

    public struct AnchoredRelativeFormatStyle: Hashable, Codable, Sendable {
        public typealias Field = Date.ComponentsFormatStyle.Field
        public typealias Presentation = Date.RelativeFormatStyle.Presentation
        public typealias UnitsStyle = Date.RelativeFormatStyle.UnitsStyle
        public var anchor: Date
        public var presentation: Presentation
        public var unitsStyle: UnitsStyle
        public var capitalizationContext: FormatStyleCapitalizationContext
        public var locale: Locale
        public var calendar: Calendar
        public var allowedFields: Set<Field>
        public init(anchor: Date, presentation: Presentation = .numeric, unitsStyle: UnitsStyle = .wide, locale: Locale = .autoupdatingCurrent, calendar: Calendar = .autoupdatingCurrent, capitalizationContext: FormatStyleCapitalizationContext = .unknown) {
            self.anchor = anchor
            self.presentation = presentation
            self.unitsStyle = unitsStyle
            self.capitalizationContext = capitalizationContext
            self.locale = locale
            self.calendar = calendar
            self.allowedFields = [.year, .month, .week, .day, .hour, .minute, .second]
        }
        public init(anchor: Date, allowedFields: Set<Field>, presentation: Presentation = .numeric, unitsStyle: UnitsStyle = .wide, locale: Locale = .autoupdatingCurrent, calendar: Calendar = .autoupdatingCurrent, capitalizationContext: FormatStyleCapitalizationContext = .unknown) {
            self.init(anchor: anchor, presentation: presentation, unitsStyle: unitsStyle, locale: locale, calendar: calendar, capitalizationContext: capitalizationContext)
            self.allowedFields = allowedFields
        }
    }
}

extension Date.AnchoredRelativeFormatStyle: FormatStyle {
    public func format(_ input: Date) -> String { _mosRelative(anchor.timeIntervalSince(input), fields: allowedFields, style: unitsStyle) }
    public func locale(_ locale: Locale) -> Self { var copy = self; copy.locale = locale; return copy }
}
extension Date.AnchoredRelativeFormatStyle: DiscreteFormatStyle {
    public func discreteInput(before input: Date) -> Date? { input.addingTimeInterval(-1) }
    public func discreteInput(after input: Date) -> Date? { input.addingTimeInterval(1) }
    public func input(before input: Date) -> Date? { discreteInput(before: input) }
    public func input(after input: Date) -> Date? { discreteInput(after: input) }
}
extension Date.RelativeFormatStyle: FormatStyle {
    public func format(_ input: Date) -> String { _mosRelative(input.timeIntervalSinceNow, fields: allowedFields, style: unitsStyle) }
    public func locale(_ locale: Locale) -> Self { var copy = self; copy.locale = locale; return copy }
}
extension Date.RelativeFormatStyle: DiscreteFormatStyle {
    public func discreteInput(before input: Date) -> Date? { input.addingTimeInterval(-1) }
    public func discreteInput(after input: Date) -> Date? { input.addingTimeInterval(1) }
    public func input(before input: Date) -> Date? { discreteInput(before: input) }
    public func input(after input: Date) -> Date? { discreteInput(after: input) }
}

// MARK: - Duration

extension Duration {
    public func formatted<S: FormatStyle>(_ format: S) -> S.FormatOutput where S.FormatInput == Duration {
        format.format(self)
    }

    public struct UnitsFormatStyle: Hashable, Codable, Sendable {
        public struct Unit: Hashable, Codable, Sendable {
            public enum Kind: Int, Hashable, Codable, Sendable, CaseIterable {
                case weeks, days, hours, minutes, seconds, milliseconds, microseconds, nanoseconds
            }
            public var kind: Kind
            public static var weeks: Unit { .init(kind: .weeks) }
            public static var days: Unit { .init(kind: .days) }
            public static var hours: Unit { .init(kind: .hours) }
            public static var minutes: Unit { .init(kind: .minutes) }
            public static var seconds: Unit { .init(kind: .seconds) }
            public static var milliseconds: Unit { .init(kind: .milliseconds) }
            public static var microseconds: Unit { .init(kind: .microseconds) }
            public static var nanoseconds: Unit { .init(kind: .nanoseconds) }
            var seconds: Double {
                switch kind {
                case .weeks: 604_800
                case .days: 86_400
                case .hours: 3_600
                case .minutes: 60
                case .seconds: 1
                case .milliseconds: 0.001
                case .microseconds: 0.000_001
                case .nanoseconds: 0.000_000_001
                }
            }
        }
        public struct UnitWidth: Hashable, Codable, Sendable {
            public enum Kind: Int, Hashable, Codable, Sendable { case wide, abbreviated, condensedAbbreviated, narrow }
            public var kind: Kind
            public static var wide: UnitWidth { .init(kind: .wide) }
            public static var abbreviated: UnitWidth { .init(kind: .abbreviated) }
            public static var condensedAbbreviated: UnitWidth { .init(kind: .condensedAbbreviated) }
            public static var narrow: UnitWidth { .init(kind: .narrow) }
        }
        public struct ZeroValueUnitsDisplayStrategy: Hashable, Codable, Sendable {
            public var length: Int
            public static var hide: ZeroValueUnitsDisplayStrategy { .init(length: 0) }
            public static func show(length: Int) -> ZeroValueUnitsDisplayStrategy { .init(length: length) }
        }
        public struct FractionalPartDisplayStrategy: Hashable, Codable, Sendable {
            public var minimumLength: Int
            public var maximumLength: Int
            public var roundingRaw: Int
            public var roundingIncrement: Double?
            public var roundingRule: FloatingPointRoundingRule {
                get { _mosRound(roundingRaw) }
                set { roundingRaw = _mosRoundRaw(newValue) }
            }
            public static var hide: FractionalPartDisplayStrategy { .init(minimumLength: 0, maximumLength: 0, roundingRaw: 0, roundingIncrement: nil) }
            public static func hide(rounded: FloatingPointRoundingRule) -> FractionalPartDisplayStrategy {
                .init(minimumLength: 0, maximumLength: 0, roundingRaw: _mosRoundRaw(rounded), roundingIncrement: nil)
            }
            public static func show(length: Int, rounded rule: FloatingPointRoundingRule = .toNearestOrEven, increment: Double? = nil) -> FractionalPartDisplayStrategy {
                .init(minimumLength: length, maximumLength: length, roundingRaw: _mosRoundRaw(rule), roundingIncrement: increment)
            }
        }
        public var locale: Locale
        public var allowedUnits: Set<Unit>
        public var unitWidth: UnitWidth
        public var maximumUnitCount: Int?
        public var zeroValueUnitsDisplay: ZeroValueUnitsDisplayStrategy
        public var fractionalPartDisplay: FractionalPartDisplayStrategy
        public var valueLengthLimits: Range<Int>?
        public init(allowedUnits: Set<Unit>, width: UnitWidth, maximumUnitCount: Int? = nil, zeroValueUnits: ZeroValueUnitsDisplayStrategy = .hide, valueLength: Int? = nil, fractionalPart: FractionalPartDisplayStrategy = .hide) {
            self.allowedUnits = allowedUnits
            self.unitWidth = width
            self.maximumUnitCount = maximumUnitCount
            self.zeroValueUnitsDisplay = zeroValueUnits
            self.fractionalPartDisplay = fractionalPart
            self.locale = .autoupdatingCurrent
            if let valueLength, valueLength > 0 { self.valueLengthLimits = valueLength..<(valueLength + 1) }
        }
        public var attributed: Attributed { .init(base: self) }
        func fields(of duration: Duration) -> [(Unit, Double)] {
            var left = Double(duration.components.seconds) + Double(duration.components.attoseconds) / 1e18
            let negative = left < 0
            if negative { left = -left }
            let ordered = Unit.Kind.allCases.map { Unit(kind: $0) }.filter { allowedUnits.contains($0) }
            let cap = maximumUnitCount ?? ordered.count
            var out: [(Unit, Double)] = []
            for (i, unit) in ordered.enumerated() {
                if out.count >= cap { break }
                let whole = (i == ordered.count - 1 || out.count + 1 == cap) ? left / unit.seconds : floor(left / unit.seconds)
                left -= floor(left / unit.seconds) * unit.seconds
                if whole == 0 && zeroValueUnitsDisplay.length <= 0 && !out.isEmpty { continue }
                if whole == 0 && out.isEmpty && unit.kind != ordered.last?.kind { continue }
                out.append((unit, negative ? -whole : whole))
            }
            if out.isEmpty, let smallest = ordered.last { out = [(smallest, 0)] }
            return out
        }
        func render(_ duration: Duration) -> String {
            fields(of: duration).map { unit, value in
                let n = value.rounded() == value ? String(Int(value)) : String(format: "%g", value)
                return n + _mosUnitSuffix(unit, width: unitWidth)
            }.joined(separator: unitWidth.kind == .narrow || unitWidth.kind == .condensedAbbreviated ? " " : ", ")
        }
    }

    public struct TimeFormatStyle: Hashable, Codable, Sendable {
        public struct Pattern: Hashable, Codable, Sendable {
            public enum Fields: Hashable, Codable, Sendable {
                case hourMinute(roundSeconds: Int)
                case hourMinuteSecond(fractionalSecondsLength: Int, roundFractionalSeconds: Int)
                case minuteSecond(fractionalSecondsLength: Int, roundFractionalSeconds: Int)
            }
            public var fields: Fields
            public var paddingForLargestField: Int?
            public static var hourMinute: Pattern { .init(fields: .hourMinute(roundSeconds: 0), paddingForLargestField: nil) }
            public static func hourMinute(padHourToLength: Int, roundSeconds: FloatingPointRoundingRule = .toNearestOrEven) -> Pattern {
                .init(fields: .hourMinute(roundSeconds: _mosRoundRaw(roundSeconds)), paddingForLargestField: padHourToLength)
            }
            public static var hourMinuteSecond: Pattern { .init(fields: .hourMinuteSecond(fractionalSecondsLength: 0, roundFractionalSeconds: 0), paddingForLargestField: nil) }
            public static func hourMinuteSecond(padHourToLength: Int, fractionalSecondsLength: Int = 0, roundFractionalSeconds: FloatingPointRoundingRule = .toNearestOrEven) -> Pattern {
                .init(fields: .hourMinuteSecond(fractionalSecondsLength: fractionalSecondsLength, roundFractionalSeconds: _mosRoundRaw(roundFractionalSeconds)), paddingForLargestField: padHourToLength)
            }
            public static var minuteSecond: Pattern { .init(fields: .minuteSecond(fractionalSecondsLength: 0, roundFractionalSeconds: 0), paddingForLargestField: nil) }
            public static func minuteSecond(padMinuteToLength: Int, fractionalSecondsLength: Int = 0, roundFractionalSeconds: FloatingPointRoundingRule = .toNearestOrEven) -> Pattern {
                .init(fields: .minuteSecond(fractionalSecondsLength: fractionalSecondsLength, roundFractionalSeconds: _mosRoundRaw(roundFractionalSeconds)), paddingForLargestField: padMinuteToLength)
            }
        }
        public var locale: Locale
        public var pattern: Pattern
        public var grouping: NumberFormatStyleConfiguration.Grouping
        public init(pattern: Pattern, locale: Locale = .autoupdatingCurrent) {
            self.pattern = pattern
            self.locale = locale
            self.grouping = .automatic
        }
        public var attributed: Attributed { .init(pattern: pattern, locale: locale, grouping: grouping) }
        public func locale(_ locale: Locale) -> Self { var copy = self; copy.locale = locale; return copy }
        public func grouping(_ grouping: NumberFormatStyleConfiguration.Grouping) -> Self { var copy = self; copy.grouping = grouping; return copy }
        public func format(_ value: Duration) -> String { String(attributed.format(value).characters) }
    }
}

extension FormatStyle where Self == Duration.TimeFormatStyle {
    public static func time(pattern: Duration.TimeFormatStyle.Pattern) -> Self { .init(pattern: pattern) }
}

extension Duration.TimeFormatStyle: FormatStyle {}
extension Duration.TimeFormatStyle: DiscreteFormatStyle {
    public func discreteInput(before input: Duration) -> Duration? { input - .seconds(1) }
    public func discreteInput(after input: Duration) -> Duration? { input + .seconds(1) }
    public func input(before input: Duration) -> Duration? { discreteInput(before: input) }
    public func input(after input: Duration) -> Duration? { discreteInput(after: input) }
}

extension Duration.UnitsFormatStyle: FormatStyle {
    public func format(_ value: Duration) -> String { render(value) }
    public func locale(_ locale: Locale) -> Self { var copy = self; copy.locale = locale; return copy }
}
extension Duration.UnitsFormatStyle: DiscreteFormatStyle {
    public func discreteInput(before input: Duration) -> Duration? { input - .seconds(1) }
    public func discreteInput(after input: Duration) -> Duration? { input + .seconds(1) }
    public func input(before input: Duration) -> Duration? { discreteInput(before: input) }
    public func input(after input: Duration) -> Duration? { discreteInput(after: input) }
}

extension Duration.UnitsFormatStyle {
    @dynamicMemberLookup
    public struct Attributed: Hashable, Codable, Sendable {
        public var base: Duration.UnitsFormatStyle
        public subscript<T>(dynamicMember key: KeyPath<Duration.UnitsFormatStyle, T>) -> T { base[keyPath: key] }
        public subscript<T>(dynamicMember key: WritableKeyPath<Duration.UnitsFormatStyle, T>) -> T {
            get { base[keyPath: key] }
            set { base[keyPath: key] = newValue }
        }
    }
}
extension Duration.UnitsFormatStyle.Attributed: FormatStyle {
    public func format(_ value: Duration) -> AttributedString { AttributedString(base.render(value)) }
    public func locale(_ locale: Locale) -> Self { var copy = self; copy.base = base.locale(locale); return copy }
}
extension Duration.UnitsFormatStyle.Attributed: DiscreteFormatStyle {
    public func discreteInput(before input: Duration) -> Duration? { base.discreteInput(before: input) }
    public func discreteInput(after input: Duration) -> Duration? { base.discreteInput(after: input) }
    public func input(before input: Duration) -> Duration? { base.input(before: input) }
    public func input(after input: Duration) -> Duration? { base.input(after: input) }
}

extension Duration.TimeFormatStyle {
    @dynamicMemberLookup
    public struct Attributed: Hashable, Codable, Sendable {
        public var pattern: Pattern
        public var locale: Locale
        public var grouping: NumberFormatStyleConfiguration.Grouping
        public subscript<T>(dynamicMember key: KeyPath<Duration.TimeFormatStyle.Attributed, T>) -> T { self[keyPath: key] }
        public subscript<T>(dynamicMember key: WritableKeyPath<Duration.TimeFormatStyle.Attributed, T>) -> T {
            get { self[keyPath: key] }
            set { self[keyPath: key] = newValue }
        }
        public func locale(_ locale: Locale) -> Self { var copy = self; copy.locale = locale; return copy }
        public func format(_ value: Duration) -> AttributedString {
            var seconds = Double(value.components.seconds) + Double(value.components.attoseconds) / 1e18
            if seconds < 0 { seconds = 0 }
            let frac: Int
            let showHours: Bool
            switch pattern.fields {
            case .hourMinute:
                frac = 0; showHours = true; seconds = (seconds / 60).rounded() * 60
            case let .hourMinuteSecond(fractionalSecondsLength, _):
                frac = fractionalSecondsLength; showHours = true
            case let .minuteSecond(fractionalSecondsLength, _):
                frac = fractionalSecondsLength; showHours = false
            }
            let total = Int(seconds.rounded(.towardZero))
            let hours = total / 3600
            let minutes = showHours ? (total % 3600) / 60 : total / 60
            let secs = total % 60
            let pad = pattern.paddingForLargestField ?? 1
            var text: String
            if showHours {
                text = _mosPad(hours, pad) + ":" + _mosPad(minutes)
                if case .hourMinute = pattern.fields {} else { text += ":" + _mosPad(secs) }
            } else {
                text = _mosPad(minutes, pad) + ":" + _mosPad(secs)
            }
            if frac > 0 {
                let fraction = seconds - Double(total)
                let digits = Int((fraction * pow(10, Double(frac))).rounded())
                text += "." + String(format: "%0\(frac)d", digits)
            }
            return AttributedString(text)
        }
    }
}
extension Duration.TimeFormatStyle.Attributed: FormatStyle {}
extension Duration.TimeFormatStyle.Attributed: DiscreteFormatStyle {
    public func discreteInput(before input: Duration) -> Duration? { input - .seconds(1) }
    public func discreteInput(after input: Duration) -> Duration? { input + .seconds(1) }
    public func input(before input: Duration) -> Duration? { discreteInput(before: input) }
    public func input(after input: Duration) -> Duration? { discreteInput(after: input) }
}

// MARK: - texto

private let _mosMonths = ["January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"]
private let _mosMonthsShort = ["Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"]
private let _mosWeekdays = ["Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"]
private let _mosWeekdaysShort = ["Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"]

private func _mosMonth(_ m: Int, wide: Bool) -> String {
    let names = wide ? _mosMonths : _mosMonthsShort
    return names[min(max(m, 1), 12) - 1]
}
private func _mosWeekday(_ w: Int, wide: Bool) -> String {
    let names = wide ? _mosWeekdays : _mosWeekdaysShort
    return names[min(max(w, 1), 7) - 1]
}
private func _mosPad(_ n: Int, _ width: Int = 2) -> String {
    let s = String(n)
    if s.count >= width { return s }
    return String(repeating: "0", count: width - s.count) + s
}
private func _mosUnitSuffix(_ unit: Duration.UnitsFormatStyle.Unit, width: Duration.UnitsFormatStyle.UnitWidth) -> String {
    let wide: String
    let abbr: String
    let narrow: String
    switch unit.kind {
    case .weeks: (wide, abbr, narrow) = (" weeks", " wk", "w")
    case .days: (wide, abbr, narrow) = (" days", " days", "d")
    case .hours: (wide, abbr, narrow) = (" hours", " hr", "h")
    case .minutes: (wide, abbr, narrow) = (" minutes", " min", "m")
    case .seconds: (wide, abbr, narrow) = (" seconds", " sec", "s")
    case .milliseconds: (wide, abbr, narrow) = (" milliseconds", " ms", "ms")
    case .microseconds: (wide, abbr, narrow) = (" microseconds", " µs", "µs")
    case .nanoseconds: (wide, abbr, narrow) = (" nanoseconds", " ns", "ns")
    }
    switch width.kind {
    case .wide: return wide
    case .abbreviated: return abbr
    case .condensedAbbreviated: return abbr.replacingOccurrences(of: " ", with: "")
    case .narrow: return narrow
    }
}
private func _mosRelative(_ seconds: TimeInterval, fields: Set<Date.ComponentsFormatStyle.Field>, style: Date.RelativeFormatStyle.UnitsStyle) -> String {
    let past = seconds < 0
    let absSec = abs(seconds)
    let choices: [(Date.ComponentsFormatStyle.Field, Double, String, String)] = [
        (.year, 31_536_000, "year", "yr"),
        (.month, 2_592_000, "month", "mo"),
        (.week, 604_800, "week", "wk"),
        (.day, 86_400, "day", "day"),
        (.hour, 3_600, "hour", "hr"),
        (.minute, 60, "minute", "min"),
        (.second, 1, "second", "sec"),
    ]
    for (field, size, name, short) in choices where fields.contains(field) && absSec >= size {
        let n = Int(absSec / size)
        let unit = style == .narrow || style == .abbreviated ? short : name
        let body = "\(n) \(unit)" + (n == 1 || style == .narrow ? "" : (style == .abbreviated ? "" : "s"))
        if past { return body + " ago" }
        return "in " + body
    }
    return past ? "just now" : "now"
}
