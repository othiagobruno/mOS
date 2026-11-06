// mOS SDK — SwiftUI: views e modificadores
import MOSBridge

// MARK: - Valores

public struct Font: Equatable {
    var size: Double
    var weight: Int     // 0 normal, 1 semibold, 2 bold, 3 mono
    public static let largeTitle = Font(size: 26, weight: 2)
    public static let title = Font(size: 22, weight: 2)
    public static let title2 = Font(size: 17, weight: 2)
    public static let title3 = Font(size: 15, weight: 1)
    public static let headline = Font(size: 13, weight: 1)
    public static let subheadline = Font(size: 11, weight: 0)
    public static let body = Font(size: 13, weight: 0)
    public static let callout = Font(size: 12, weight: 0)
    public static let footnote = Font(size: 10, weight: 0)
    public static let caption = Font(size: 10, weight: 0)
    public static let caption2 = Font(size: 9, weight: 0)
    public enum Weight { case regular, medium, semibold, bold, heavy, black, light, thin }
    public enum Design { case `default`, monospaced, rounded, serif }
    public static func system(size: Double, weight: Weight = .regular, design: Design = .default) -> Font {
        if design == .monospaced { return Font(size: size, weight: 3) }
        switch weight { case .bold, .heavy, .black: return Font(size: size, weight: 2); case .semibold, .medium: return Font(size: size, weight: 1); default: return Font(size: size, weight: 0) }
    }
    public static func system(_ style: Font, design: Design = .default) -> Font { design == .monospaced ? Font(size: style.size, weight: 3) : style }
    public func bold() -> Font { Font(size: size, weight: weight == 3 ? 3 : 2) }
    public func monospaced() -> Font { Font(size: size, weight: 3) }
    public func weight(_ w: Weight) -> Font { Font.system(size: size, weight: w) }
}

public struct Color: Equatable, _PrimitiveView {
    /// Como view, Color preenche todo o espaço proposto (como no SwiftUI).
    public func _build(_ b: _Builder) { let n = b.add(Node(.background)); n.color = self; n.radius = 0; n.maxW = true; n.maxH = true }
    var r: Double, g: Double, b: Double, a: Double
    public init(red: Double, green: Double, blue: Double, opacity: Double = 1) { r = red; g = green; b = blue; a = opacity }
    public init(white: Double, opacity: Double = 1) { r = white; g = white; b = white; a = opacity }
    public static let primary = Color(red: 0, green: 0, blue: 0, opacity: 0.88)
    public static let secondary = Color(red: 0, green: 0, blue: 0, opacity: 0.5)
    public static let black = Color(white: 0)
    public static let white = Color(white: 1)
    public static let gray = Color(white: 0.55)
    public static let red = Color(red: 1, green: 0.23, blue: 0.19)
    public static let orange = Color(red: 1, green: 0.58, blue: 0)
    public static let yellow = Color(red: 1, green: 0.8, blue: 0)
    public static let green = Color(red: 0.2, green: 0.78, blue: 0.35)
    public static let mint = Color(red: 0, green: 0.78, blue: 0.75)
    public static let teal = Color(red: 0.19, green: 0.69, blue: 0.78)
    public static let blue = Color(red: 0, green: 0.48, blue: 1)
    public static let indigo = Color(red: 0.35, green: 0.34, blue: 0.84)
    public static let purple = Color(red: 0.69, green: 0.32, blue: 0.87)
    public static let pink = Color(red: 1, green: 0.18, blue: 0.33)
    public static let accentColor = Color.blue
    public static let clear = Color(white: 0, opacity: 0)
    public func opacity(_ o: Double) -> Color { Color(red: r, green: g, blue: b, opacity: a * o) }
}

public struct EdgeInsets: Equatable {
    public var top: Double, leading: Double, bottom: Double, trailing: Double
    public init(top: Double, leading: Double, bottom: Double, trailing: Double) { self.top = top; self.leading = leading; self.bottom = bottom; self.trailing = trailing }
}
public struct Edge {
    public struct Set: OptionSet {
        public let rawValue: Int
        public init(rawValue: Int) { self.rawValue = rawValue }
        public static let top = Set(rawValue: 1), leading = Set(rawValue: 2), bottom = Set(rawValue: 4), trailing = Set(rawValue: 8)
        public static let horizontal: Set = [.leading, .trailing], vertical: Set = [.top, .bottom], all: Set = [.top, .leading, .bottom, .trailing]
    }
}
public enum HorizontalAlignment { case leading, center, trailing }
public enum VerticalAlignment { case top, center, bottom, firstTextBaseline }
public struct Alignment { let h: HorizontalAlignment; let v: VerticalAlignment
    public static let center = Alignment(h: .center, v: .center), leading = Alignment(h: .leading, v: .center), trailing = Alignment(h: .trailing, v: .center)
    public static let top = Alignment(h: .center, v: .top), bottom = Alignment(h: .center, v: .bottom), topLeading = Alignment(h: .leading, v: .top)
}

// MARK: - Primitivas

public struct Text: _PrimitiveView {
    var content: String
    var font: Font? = nil
    var color: Color? = nil
    public init(_ s: String) { content = s }
    public init(verbatim s: String) { content = s }
    public func _build(_ b: _Builder) {
        let n = b.add(Node(.text))
        n.text = content
        if let font { n.font = font }
        if let color { n.color = color }
    }
    public func font(_ f: Font) -> Text { var t = self; t.font = f; return t }
    public func bold() -> Text { var t = self; t.font = (font ?? .body).bold(); return t }
    public func foregroundColor(_ c: Color) -> Text { var t = self; t.color = c; return t }
    public func foregroundStyle(_ c: Color) -> Text { var t = self; t.color = c; return t }
}

public struct Button<Label: View>: _PrimitiveView {
    let action: () -> Void
    let title: String?
    let label: Label?
    public init(action: @escaping () -> Void, @ViewBuilder label: () -> Label) { self.action = action; title = nil; self.label = label() }
    public func _build(_ b: _Builder) {
        let n = b.add(Node(.button))
        n.action = action
        if let title { n.text = title }
        else if let label {   // rótulo: usa o texto dos Text internos
            let tmp = Node(.stack)
            let sub = _Builder(root: tmp)
            label._build(sub)
            n.text = _Builder.collectText(tmp)
        }
    }
}
extension Button where Label == Text {
    public init(_ title: String, action: @escaping () -> Void) { self.action = action; self.title = title; label = nil }
}
extension _Builder {
    static func collectText(_ n: Node) -> String {
        if n.kind == .text { return n.text }
        var s = ""
        for c in n.children { let t = collectText(c); if !t.isEmpty { s += s.isEmpty ? t : " " + t } }
        return s
    }
}

public struct TextField: _PrimitiveView {
    let title: String
    let text: Binding<String>
    let secure: Bool
    public init(_ title: String, text: Binding<String>) { self.title = title; self.text = text; secure = false }
    public func _build(_ b: _Builder) {
        let n = b.add(Node(secure ? .secure : .field))
        n.placeholder = title
        n.text = text.wrappedValue
        let t = text
        n.onText = { t.wrappedValue = $0 }
    }
}
public struct SecureField: _PrimitiveView {
    let f: TextField
    public init(_ title: String, text: Binding<String>) { f = TextField(title: title, text: text, secure: true) }
    public func _build(_ b: _Builder) { f._build(b) }
}
extension TextField { init(title: String, text: Binding<String>, secure: Bool) { self.title = title; self.text = text; self.secure = secure } }

public struct TextEditor: _PrimitiveView {
    let text: Binding<String>
    public init(text: Binding<String>) { self.text = text }
    public func _build(_ b: _Builder) {
        let n = b.add(Node(.editor))
        n.text = text.wrappedValue
        n.maxW = true; n.maxH = true
        let t = text
        n.onText = { t.wrappedValue = $0 }
    }
}

public struct Toggle: _PrimitiveView {
    let title: String
    let isOn: Binding<Bool>
    public init(_ title: String, isOn: Binding<Bool>) { self.title = title; self.isOn = isOn }
    public func _build(_ b: _Builder) {
        let n = b.add(Node(.toggle))
        n.text = title
        n.flag = isOn.wrappedValue
        let v = isOn
        n.onBool = { v.wrappedValue = $0 }
    }
}

public struct Slider: _PrimitiveView {
    let value: Binding<Double>
    let range: ClosedRange<Double>
    public init(value: Binding<Double>, in range: ClosedRange<Double> = 0...1) { self.value = value; self.range = range }
    public func _build(_ b: _Builder) {
        let n = b.add(Node(.slider))
        n.value = value.wrappedValue; n.lower = range.lowerBound; n.upper = range.upperBound
        let v = value
        n.onDouble = { v.wrappedValue = $0 }
    }
}

public struct ProgressView: _PrimitiveView {
    let value: Double, total: Double
    public init(value: Double, total: Double = 1) { self.value = value; self.total = total }
    public func _build(_ b: _Builder) { let n = b.add(Node(.progress)); n.value = value; n.upper = total }
}

public struct Spacer: _PrimitiveView {
    let minLength: Double
    public init(minLength: Double = 0) { self.minLength = minLength }
    public func _build(_ b: _Builder) { let n = b.add(Node(.spacer)); n.minLength = minLength }
}
public struct Divider: _PrimitiveView {
    public init() {}
    public func _build(_ b: _Builder) { b.add(Node(.divider)) }
}
/// Image(systemName:) desenha um símbolo de texto equivalente (o mOS não tem SF Symbols).
public struct Image: _PrimitiveView {
    let glyph: String
    public init(systemName: String) {
        // a fonte do mOS não tem SF Symbols: usa caracteres que ela desenha
        switch systemName {
        case "plus", "plus.circle", "plus.circle.fill": glyph = "+"
        case "minus", "minus.circle": glyph = "−"
        case "xmark", "xmark.circle": glyph = "×"
        case "chevron.right": glyph = "›"
        case "chevron.left": glyph = "‹"
        default: glyph = "•"
        }
    }
    public func _build(_ b: _Builder) { let n = b.add(Node(.text)); n.text = glyph }
}

// MARK: - Pilhas e coleções

public struct VStack<Content: View>: _PrimitiveView {
    let alignment: HorizontalAlignment, spacing: Double?, content: Content
    public init(alignment: HorizontalAlignment = .center, spacing: Double? = nil, @ViewBuilder content: () -> Content) { self.alignment = alignment; self.spacing = spacing; self.content = content() }
    public func _build(_ b: _Builder) {
        let n = Node(.stack)
        n.axisVertical = true; n.spacing = spacing ?? 8
        n.align = alignment == .leading ? -1 : alignment == .trailing ? 1 : 0
        b.container(n) { content._build($0) }
    }
}
public struct HStack<Content: View>: _PrimitiveView {
    let alignment: VerticalAlignment, spacing: Double?, content: Content
    public init(alignment: VerticalAlignment = .center, spacing: Double? = nil, @ViewBuilder content: () -> Content) { self.alignment = alignment; self.spacing = spacing; self.content = content() }
    public func _build(_ b: _Builder) {
        let n = Node(.stack)
        n.axisVertical = false; n.spacing = spacing ?? 8
        n.align = alignment == .top ? -1 : alignment == .bottom ? 1 : 0
        b.container(n) { content._build($0) }
    }
}
public struct ScrollView<Content: View>: _PrimitiveView {
    let content: Content
    public init(@ViewBuilder content: () -> Content) { self.content = content() }
    public func _build(_ b: _Builder) {
        let n = Node(.stack)
        n.axisVertical = true; n.spacing = 8; n.align = -1; n.maxW = true; n.maxH = true
        b.container(n) { content._build($0) }
    }
}
public struct ForEach<Element, Content: View>: _PrimitiveView {
    let items: [Element]
    let make: (Element) -> Content
    public init(_ data: [Element], @ViewBuilder content: @escaping (Element) -> Content) { items = data; make = content }
    public func _build(_ b: _Builder) {
        for e in items { b.child("e") { make(e)._build($0) } }
    }
}
extension ForEach where Element == Int {
    public init(_ range: Range<Int>, @ViewBuilder content: @escaping (Int) -> Content) { items = Array(range); make = content }
    public init(_ range: ClosedRange<Int>, @ViewBuilder content: @escaping (Int) -> Content) { items = Array(range); make = content }
}

// MARK: - Modificadores

public struct _Modified<C: View>: _PrimitiveView {
    let content: C
    let apply: (_Builder, C) -> Void
    public func _build(_ b: _Builder) { apply(b, content) }
}

extension View {
    public func padding(_ length: Double = 16) -> _Modified<Self> { padding(.all, length) }
    public func padding(_ edges: Edge.Set, _ length: Double = 16) -> _Modified<Self> {
        _Modified(content: self) { b, c in
            let n = Node(.padding)
            n.insets = EdgeInsets(top: edges.contains(.top) ? length : 0, leading: edges.contains(.leading) ? length : 0,
                                  bottom: edges.contains(.bottom) ? length : 0, trailing: edges.contains(.trailing) ? length : 0)
            b.container(n) { c._build($0) }
        }
    }
    public func frame(width: Double? = nil, height: Double? = nil, alignment: Alignment = .center) -> _Modified<Self> {
        _Modified(content: self) { b, c in
            let n = Node(.frame)
            n.fixedW = width ?? -1; n.fixedH = height ?? -1
            n.align = alignment.h == .leading ? -1 : alignment.h == .trailing ? 1 : 0
            b.container(n) { c._build($0) }
        }
    }
    public func frame(minWidth: Double? = nil, maxWidth: Double? = nil, minHeight: Double? = nil, maxHeight: Double? = nil, alignment: Alignment = .center) -> _Modified<Self> {
        _Modified(content: self) { b, c in
            let n = Node(.frame)
            n.maxW = (maxWidth ?? 0) >= 1e9; n.maxH = (maxHeight ?? 0) >= 1e9
            if let mw = maxWidth, mw < 1e9 { n.fixedW = mw }
            if let mh = maxHeight, mh < 1e9 { n.fixedH = mh }
            n.align = alignment.h == .leading ? -1 : alignment.h == .trailing ? 1 : 0
            b.container(n) { c._build($0) }
        }
    }
    public func font(_ f: Font) -> _Modified<Self> {
        _Modified(content: self) { b, c in b.fonts.append(f); c._build(b); b.fonts.removeLast() }
    }
    public func foregroundColor(_ col: Color) -> _Modified<Self> {
        _Modified(content: self) { b, c in b.colors.append(col); c._build(b); b.colors.removeLast() }
    }
    public func foregroundStyle(_ col: Color) -> _Modified<Self> { foregroundColor(col) }
    public func disabled(_ d: Bool) -> _Modified<Self> {
        _Modified(content: self) { b, c in if d { b.disabledDepth += 1 }; c._build(b); if d { b.disabledDepth -= 1 } }
    }
    public func background(_ col: Color) -> _Modified<Self> {
        _Modified(content: self) { b, c in
            let n = Node(.background)
            n.color = col
            b.container(n) { c._build($0) }
        }
    }
    /// Modificadores aceitos sem efeito visual no mOS (mantêm o código compilando como no macOS).
    public func navigationTitle(_ t: String) -> _Modified<Self> { _Modified(content: self) { b, c in _Runtime.shared.title = t; c._build(b) } }
    public func cornerRadius(_ r: Double) -> _Modified<Self> { _Modified(content: self) { b, c in c._build(b) } }
    public func multilineTextAlignment(_ a: HorizontalAlignment) -> _Modified<Self> { _Modified(content: self) { b, c in c._build(b) } }
    public func textFieldStyle<S>(_ s: S) -> _Modified<Self> { _Modified(content: self) { b, c in c._build(b) } }
    public func buttonStyle<S>(_ s: S) -> _Modified<Self> { _Modified(content: self) { b, c in c._build(b) } }
    public func onAppear(perform action: @escaping () -> Void) -> _Modified<Self> {
        _Modified(content: self) { b, c in
            let k = "appear:" + b.path
            if _Runtime.shared.storage[k] == nil { _Runtime.shared.storage[k] = UnsafeMutableRawPointer(bitPattern: 1); _Runtime.shared.pending.append(action) }
            c._build(b)
        }
    }
}
public struct RoundedBorderTextFieldStyle { public init() {} }
public struct PlainButtonStyle { public init() {} }
public struct BorderedProminentButtonStyle { public init() {} }
extension _Runtime { }
